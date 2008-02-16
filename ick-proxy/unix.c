/*
 * Main program and networking code for Unix ick-proxy.
 * 
 * On Unix, we have two modes of operation. In multi-user mode we
 * simply fork off and run as a daemon, opening a master socket on
 * a specified port and serving PACs from it which cause us to
 * open additional sockets. In single-user mode we construct
 * exactly one PAC, write it to a specified file, and serve
 * connections on the resulting port; in single-user mode we are
 * also bound to a child process whose lifetime we share.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>

#include "misc.h"
#include "proxy.h"
#include "tree234.h"

char *override_inpac = NULL;
char *override_script = NULL;
char *override_outpac = NULL;

void get_user(char *buf, int buflen)
{
    struct passwd *p;
    uid_t uid = getuid();
    char *user;

    /*
     * First, find who we think we are using getlogin. If this
     * agrees with our uid, we'll go along with it. This should
     * allow sharing of uids between several login names whilst
     * coping correctly with people who have su'ed.
     */
    user = getlogin();
    if (user)
	p = getpwnam(user);
    else
	p = NULL;
    if (p && p->pw_uid == uid) {
	/*
	 * The result of getlogin() really does correspond to our
	 * uid. Fine.
	 */
	strncpy(buf, user, buflen);
	buf[buflen-1] = '\0';
    } else {
	/*
	 * If that didn't work, for whatever reason, we'll do the
	 * simpler version: look up our uid in the password file
	 * and map it straight to a name.
	 */
	p = getpwuid(uid);
	strncpy(buf, p->pw_name, buflen);
	buf[buflen-1] = '\0';
    }
}

enum { FD_SIGNAL_PIPE, FD_LISTENER, FD_CONNECTION };

struct fd {
    int fd;
    int type;
    int deleted;
    char *wdata;
    int wdatalen, wdatapos;
    struct listenctx *lctx;
    struct connctx *cctx;
};

struct fd *fds = NULL;
int nfds = 0, fdsize = 0;

struct fd *new_fdstruct(int fd, int type)
{
    struct fd *ret;

    if (nfds >= fdsize) {
	fdsize = nfds * 3 / 2 + 32;
	fds = sresize(fds, fdsize, struct fd);
    }

    ret = &fds[nfds++];

    ret->fd = fd;
    ret->type = type;
    ret->wdata = NULL;
    ret->wdatalen = ret->wdatapos = 0;
    ret->lctx = NULL;
    ret->cctx = NULL;
    ret->deleted = 0;

    return ret;
}

int create_new_listener(char **err, int port, struct listenctx *ctx)
{
    int fd;
    struct fd *f;
    struct sockaddr_in addr;
    int addrlen;

    /*
     * Establish the listening socket and retrieve its port
     * number.
     */
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	*err = dupfmt("socket(PF_INET): %s\n", strerror(errno));
	return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (port < 0)
	port = 0;
    addr.sin_port = htons(port);
    addrlen = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, addrlen) < 0) {
	*err = dupfmt("bind: %s\n", strerror(errno));
	return -1;
    }
    if (listen(fd, 5) < 0) {
	*err = dupfmt("listen: %s\n", strerror(errno));
	return -1;
    }
    addrlen = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addrlen)) {
	*err = dupfmt("getsockname: %s\n", strerror(errno));
	return -1;
    }
    port = ntohs(addr.sin_port);

    /*
     * Now construct an fd structure to hold it.
     */
    f = new_fdstruct(fd, FD_LISTENER);
    f->lctx = ctx;

    return port;
}

static char *get_filename_for_user(char **err, const char *username,
				   const char *filename)
{
    struct passwd *p;
    
    p = getpwnam(username);
    if (!p) {
	*err = dupfmt("getpwnam(\"%s\"): %s", username, strerror(errno));
	return NULL;
    }

    if (!p->pw_dir) {
	*err = dupfmt("user %s has no home directory listed", username);
	return NULL;
    }

    return dupfmt("%s/%s", p->pw_dir, filename);
}

char *read_whole_file(char **err, char *filename)
{
    char *ret;
    int len, size;
    int fd;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
	*err = dupfmt("%s: open: %s", filename, strerror(errno));
	return NULL;
    }

    size = 1024;
    ret = snewn(size, char);
    len = 0;

    while (1) {
	char readbuf[4096];
	int readret = read(fd, readbuf, sizeof(readbuf));

	if (readret < 0) {
	    sfree(ret);
	    *err = dupfmt("%s: read: %s", filename, strerror(errno));
	    return NULL;
	} else if (readret == 0) {
	    close(fd);
	    return ret;
	} else {
	    if (size < len + readret + 1) {
		size = (len + readret + 1) * 3 / 2 + 1024;
		ret = sresize(ret, size, char);
	    }
	    memcpy(ret + len, readbuf, readret);
	    ret[len + readret] = '\0';
	    len += readret;
	}
    }
}

char *get_script_for_user(char **err, const char *username)
{
    char *fname, *ret;

    if (override_script)
	fname = dupstr(override_script);
    else
	fname = get_filename_for_user(err, username, ".ick-proxy/rewrite.ick");
    if (!fname)
	return;			       /* err has already been filled in */

    ret = read_whole_file(err, fname);
    sfree(fname);
    return ret;
}

char *get_inpac_for_user(char **err, const char *username)
{
    char *fname, *ret;

    if (override_inpac)
	fname = dupstr(override_inpac);
    else
	fname = get_filename_for_user(err, username, ".ick-proxy/input.pac");
    if (!fname)
	return;			       /* err has already been filled in */

    ret = read_whole_file(err, fname);
    sfree(fname);
    return ret;
}

char *name_outpac_for_user(char **err, const char *username)
{
    char *fname;

    if (override_outpac)
	fname = dupstr(override_outpac);
    else
	fname = get_filename_for_user(err, username, ".ick-proxy/output.pac");

    return fname;
}

int is_daemon = 0;

void daemonise(char *dropprivuser)
{
    pid_t pid;
    int i, fd;
    int droppriv_uid = -1;

    if (dropprivuser) {
	struct passwd *p;
	p = getpwnam(dropprivuser);
	if (!p) {
	    fprintf(stderr, "ick-proxy: getpwnam(\"%s\"): %s\n", dropprivuser,
		    strerror(errno));
	    exit(1);
	}
	droppriv_uid = p->pw_uid;
    }

    /*
     * Fork.
     */
    pid = fork();
    if (pid < 0) {
	perror("fork");
	exit(1);
    } else if (pid > 0) {
	/*
	 * We are the parent. Terminate.
	 */
	exit(0);
    }

    /*
     * If we get here, we're the child process. We fork _again_ at
     * this point, for paranoid reasons which I can't remember but
     * I know I read about it in Stevens once.
     */
    pid = fork();
    if (pid < 0) {
	perror("fork");
	exit(1);		       /* here we stop returning to main() */
    } else if (pid > 0) {
	exit(0);
    }

    is_daemon = 1;

    /* Open /dev/null on std{in,out,err}. */
    fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2)
	    close(fd);
    }
    /* Close all other open files except the ones we're actually using. */
    for (i = 3; i < 4096; i++) {
	int j;
	for (j = 0; j < nfds; j++)
	    if (fds[j].fd == i)
		break;
	if (j == nfds)
	    close(i);
    }
    /* setsid() to destroy controlling terminal. */
    setsid();
    /* Stop holding on to any unnecessary directory handles. */
    chdir("/");

    /* Drop privileges. */
    if (dropprivuser)
	setuid(droppriv_uid);
}

void platform_fatal_error(const char *err)
{
    if (is_daemon) {
	syslog(LOG_DAEMON | LOG_ERR, "ick-proxy fatal error: %s", err);
    } else {
	fprintf(stderr, "ick-proxy: fatal error: %s\n", err);
    }
    exit(1);
}

static int signalpipe[2];
static pid_t child_pid = -1;

void sigchld(int sig)
{
    write(signalpipe[1], "x", 1);
}

void run_subprocess(char **cmd)
{
    int pid;

    if (pipe(signalpipe) < 0) {
        fprintf(stderr, "ick-proxy: pipe: %s\n", strerror(errno));
        exit(1);
    }
    signal(SIGCHLD, sigchld);

    pid = fork();

    if (pid < 0) {
	fprintf(stderr, "ick-proxy: fork: %s\n", strerror(errno));
	exit(1);
    } else if (pid == 0) {
	execvp(cmd[0], cmd);
	fprintf(stderr, "ick-proxy: exec(\"%s\"): %s\n",
		cmd[0], strerror(errno));
	exit(127);		       /* if all else fails */
    }

    /* we only reach here if pid > 0, i.e. we are the parent */
    child_pid = pid;
    new_fdstruct(signalpipe[0], FD_SIGNAL_PIPE);
}

const char usagemsg[] =
    "  usage: ick-proxy [options] <subcommand>\n"
    "     or: ick-proxy [options] -t <test-url>\n"
    "     or: ick-proxy [options] --multiuser\n"
    "  where: --multiuser    run as a daemon serving PACs over HTTP\n"
    "         -t             test-rewrite a single URL and terminate\n"
    "         <subcommand>   write a single PAC and run with lifetime of subcommand\n"
    "options: -p <port>      specify master port for in multi-user mode\n"
    "         -u <username>  run as unprivileged user in multi-user mode\n"
    "         -s <script>    override default location for rewrite script\n"
    "         -i <inpac>     override default location for input .PAC file\n"
    "         -o <outpac>    override default location for output .PAC file\n"
    " also:   ick-proxy --version  report version number\n"
    "         ick-proxy --help     display this help text\n"
    "         ick-proxy --licence  display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "ick-proxy is copyright 2004-8 Simon Tatham.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person\n"
    "obtaining a copy of this software and associated documentation files\n"
    "(the \"Software\"), to deal in the Software without restriction,\n"
    "including without limitation the rights to use, copy, modify, merge,\n"
    "publish, distribute, sublicense, and/or sell copies of the Software,\n"
    "and to permit persons to whom the Software is furnished to do so,\n"
    "subject to the following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall be\n"
    "included in all copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS\n"
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"
    "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
    "SOFTWARE.\n"
    ;

void licence(void) {
    fputs(licencemsg, stdout);
}

void version(void) {
    printf("ick-proxy version 2.0\n");
}

int main(int argc, char **argv)
{
    char **singleusercmd = NULL;
    char *testurl = NULL;
    char *dropprivuser = NULL;
    char thisuserbuf[1024];
    int doing_opts = 1;
    int port = 880;
    int multiuser = 0;

    /*
     * Parse the arguments.
     */
    while (--argc > 0) {
        char *p = *++argv;
        if (*p == '-' && doing_opts) {
            if (!strcmp(p, "--multiuser")) {
		multiuser = 1;
            } else if (!strcmp(p, "-p")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -p expected a parameter\n");
                    return 1;
                }
                port = atoi(*++argv);
            } else if (!strcmp(p, "-s")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -s expected a parameter\n");
                    return 1;
                }
		override_script = *++argv;
            } else if (!strcmp(p, "-i")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -i expected a parameter\n");
                    return 1;
                }
		override_inpac = *++argv;
            } else if (!strcmp(p, "-o")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -o expected a parameter\n");
                    return 1;
                }
		override_outpac = *++argv;
            } else if (!strcmp(p, "-t")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -t expected a parameter\n");
                    return 1;
                }
		testurl = *++argv;
            } else if (!strcmp(p, "-u")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -u expected a parameter\n");
                    return 1;
                }
		dropprivuser = *++argv;
            } else if (!strcmp(p, "--help") || !strcmp(p, "-help") ||
                       !strcmp(p, "-?") || !strcmp(p, "-h")) {
                /* Valid help request. */
                usage();
                return 0;
            } else if (!strcmp(p, "--licence") || !strcmp(p, "--license")) {
                licence();
                return 0;
            } else if (!strcmp(p, "--version")) {
                version();
                return 0;
            } else if (!strcmp(p, "--")) {
                doing_opts = 0;
            } else {
                /* Invalid option; give help as well as whining. */
                fprintf(stderr, "ick-proxy: unrecognised option '%s'\n", p);
                usage();
                return 1;
            }
        } else {
            singleusercmd = argv;
            break;
        }
    }

    get_user(thisuserbuf, lenof(thisuserbuf));
    signal(SIGPIPE, SIG_IGN);
    ick_proxy_setup();

    /*
     * Distinguish the various running modes. First, rule out
     * URL-test mode...
     */
    if (testurl) {
	char *err, *ret;

	ret = rewrite_url(&err, thisuserbuf, testurl);
	if (!ret) {
	    fprintf(stderr, "ick-proxy: rewrite error: %s\n", err);
	    sfree(err);
	    return 1;
	} else {
	    puts(ret);
	    sfree(ret);
	    return 0;
	}
    }

    /*
     * ... then do the different setup required for single- and
     * multi-user mode.
     */

    if (multiuser) {
	/*
	 * Set up our master listening socket _before_ turning
	 * into a daemon, so that we'll report on stderr if it
	 * fails, and more importantly so that we can allocate our
	 * master listening port below 1024 before dropping
	 * privileges.
	 */
	configure_master(port);
	daemonise(dropprivuser);
    } else {
	char *outpac_text;
	char *outpac_file;
	char *err;

	if (!singleusercmd) {
	    fprintf(stderr, "ick-proxy: expected a command in single-user"
		    " mode\n");
	    return 1;
	}
	outpac_text = configure_for_user(thisuserbuf);
	assert(outpac_text);
	outpac_file = name_outpac_for_user(&err, thisuserbuf);
	if (!outpac_file) {
	    fprintf(stderr, "%s\n", err);
	    return 1;
	} else {
	    int fd = open(outpac_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	    int pos, len, ret;

	    pos = 0;
	    len = strlen(outpac_text);
	    while (pos < len) {
		ret = write(fd, outpac_text + pos, len - pos);
		if (ret < 0) {
		    fprintf(stderr, "%s: write: %s\n", outpac_text,
			    strerror(errno));
		    return 1;
		} else {
		    pos += ret;
		}
	    }

	    close(fd);
	}
	run_subprocess(singleusercmd);
    }

    /*
     * Now we're ready to run our main loop. Keep looping round on
     * select.
     */
    while (1) {
	fd_set rfds, wfds;
	int i, j, maxfd, ret;

#define FD_SET_MAX(fd, set, max) \
        do { FD_SET((fd),(set)); (max) = ((max)<=(fd)?(fd)+1:(max)); } while(0)

	/*
	 * Loop round the fd list putting fds into our select
	 * sets. Also in this loop we remove any that were marked
	 * as deleted in the previous loop.
	 */
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	for (i = j = 0; j < nfds; j++) {

	    if (fds[j].deleted)
		continue;
	    fds[i] = fds[j];

	    switch (fds[i].type) {
	      case FD_SIGNAL_PIPE:
	      case FD_LISTENER:
		FD_SET_MAX(fds[i].fd, &rfds, maxfd);
		break;
	      case FD_CONNECTION:
		/*
		 * Always read from a connection socket. Even
		 * after we've started writing, the peer might
		 * still be sending (e.g. because we shamefully
		 * jumped the gun before waiting for the end of
		 * the HTTP request) and so we should be prepared
		 * to read data and throw it away.
		 */
		FD_SET_MAX(fds[i].fd, &rfds, maxfd);
		/*
		 * Also attempt to write, if we have data to write.
		 */
		if (fds[i].wdatapos < fds[i].wdatalen)
		    FD_SET_MAX(fds[i].fd, &wfds, maxfd);
		break;
	    }

	    i++;
	}
	nfds = i;

        ret = select(maxfd, &rfds, &wfds, NULL, NULL);
	if (ret <= 0) {
	    if (ret < 0 && (errno != EINTR))
		platform_fatal_error(dupfmt("select: %s", strerror(errno)));
	    continue;
	}

	for (i = 0; i < nfds; i++) {
	    switch (fds[i].type) {
	      case FD_SIGNAL_PIPE:
		if (FD_ISSET(fds[i].fd, &rfds)) {
		    pid_t pid;
		    int status;
		    int buf[4096];

		    /*
		     * Empty signal pipe, and ignore its contents.
		     */
		    read(fds[i].fd, buf, sizeof(buf));

		    /*
		     * Our child process may have terminated.
		     * Check.
		     */
		    pid = wait(&status);
		    if (pid > 0 && pid == child_pid &&
			(WIFEXITED(status) || WIFSIGNALED(status))) {
			/*
			 * It has. Abandon ship!
			 */
			return 0;
		    }
		}
		break;
	      case FD_LISTENER:
		if (FD_ISSET(fds[i].fd, &rfds)) {
		    /*
		     * New connection has come in. Accept it.
		     */
		    struct fd *f;
		    struct sockaddr_in addr;
		    int addrlen = sizeof(addr);
		    int newfd = accept(fds[i].fd, (struct sockaddr *)&addr,
				       &addrlen);
		    if (newfd < 0)
			break;	       /* not sure what happened there */

		    f = new_fdstruct(newfd, FD_CONNECTION);
		    f->cctx = new_connection(fds[i].lctx);
		}
		break;
	      case FD_CONNECTION:
		if (FD_ISSET(fds[i].fd, &rfds)) {
		    /*
		     * There's data to be read.
		     */
		    char readbuf[4096];
		    int ret;

		    ret = read(fds[i].fd, readbuf, sizeof(readbuf));
		    if (ret <= 0) {
			/*
			 * This shouldn't happen in a sensible
			 * HTTP connection, so we abandon the
			 * connection if it does.
			 */
			close(fds[i].fd);
			fds[i].deleted = 1;
			break;
		    } else {
			if (!fds[i].wdata) {
			    /*
			     * If we haven't got an HTTP response
			     * yet, keep feeding data to proxy.c
			     * in the hope of acquiring one.
			     */
			    fds[i].wdata = got_data(fds[i].cctx, readbuf, ret);
			    if (fds[i].wdata) {
				fds[i].wdatalen = strlen(fds[i].wdata);
				fds[i].wdatapos = 0;
			    }
			} else {
			    /*
			     * Otherwise, just drop our read data
			     * on the floor.
			     */
			}
		    }
		}
		if (FD_ISSET(fds[i].fd, &wfds) &&
		    fds[i].wdatapos < fds[i].wdatalen) {
		    /*
		     * The socket is writable, and we have data to
		     * write. Write it.
		     */
		    int ret = write(fds[i].fd, fds[i].wdata + fds[i].wdatapos,
				    fds[i].wdatalen - fds[i].wdatapos);
		    if (ret <= 0) {
			/*
			 * Shouldn't happen; abandon the connection.
			 */
			close(fds[i].fd);
			fds[i].deleted = 1;
			break;
		    } else {
			fds[i].wdatapos += ret;
			if (fds[i].wdatapos == fds[i].wdatalen) {
			    shutdown(fds[i].fd, SHUT_WR);
			}
		    }
		}
		break;
	    }
	}

    }

    return 0;
}
