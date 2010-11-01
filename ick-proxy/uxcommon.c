/*
 * Main networking code for Unix versions of ick-proxy.
 * 
 * On Unix, we have two modes of operation. In multi-user mode we
 * simply fork off and run as a daemon, opening a master socket on
 * a specified port and serving PACs from it which cause us to
 * open additional sockets. In single-user mode we construct
 * exactly one PAC, write it to a specified file, and serve
 * connections on the resulting port; in single-user mode we are
 * also bound to a child process whose lifetime we share.
 */

/*
 * Possible future work:
 * 
 *  - if anyone ever uses the systemwide server mode and wants it
 *    to read config files for a user whose home directory is not
 *    world-x, it would be feasible in principle to set up a
 *    method of config file access via userv:
 *     + spec out a tiny userv service which reads
 * 	 ~/.ick-proxy/rewrite.ick and ~/.ick-proxy/input.pac
 *     + provide default implementation for the service, and a
 * 	 fragment of systemwide userv config that sets it up
 *     + have the ick-proxy server attempt to use this service as
 * 	 the first method of getting at config files, and only
 * 	 fall back to reading the files directly if the userv
 * 	 service refuses to run.
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

enum { FD_CLIENT, FD_SIGNAL_PIPE, FD_LISTENER, FD_CONNECTION };

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
    socklen_t addrlen;

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
    const char *dir;
    
    if (*username) {
	p = getpwnam(username);
	if (!p) {
	    *err = dupfmt("getpwnam(\"%s\"): %s", username, strerror(errno));
	    return NULL;
	}

	if (!p->pw_dir) {
	    *err = dupfmt("user %s has no home directory listed", username);
	    return NULL;
	}

	dir = p->pw_dir;
    } else {
	dir = getenv("HOME");
	if (!dir)
	    dir = "/";
    }

    return dupfmt("%s/%s", dir, filename);
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
	return NULL;		       /* err has already been filled in */

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
	return NULL;		       /* err has already been filled in */

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

static int signalpipe[2];
static pid_t child_pid = -1;

void sigchld(int sig)
{
    write(signalpipe[1], "c", 1);
}

void sighup(int sig)
{
    write(signalpipe[1], "h", 1);
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
	exit(1);		       /* here we stop returning to uxmain() */
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
        if (i == signalpipe[1])
            continue;
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

void run_subprocess(char **cmd)
{
    int pid;

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
}

int configure_single_user(void)
{
    char *outpac_text;
    char *outpac_file;
    char *err;

    outpac_text = configure_for_user("");
    assert(outpac_text);
    outpac_file = name_outpac_for_user(&err, "");
    if (!outpac_file) {
	fprintf(stderr, "%s\n", err);
	return 0;
    } else {
	int fd = open(outpac_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	int pos, len, ret;

	pos = 0;
	len = strlen(outpac_text);
	while (pos < len) {
	    ret = write(fd, outpac_text + pos, len - pos);
	    if (ret < 0) {
		fprintf(stderr, "%s: write: %s\n", outpac_file,
			strerror(errno));
		return 0;
	    } else {
		pos += ret;
	    }
	}

	close(fd);
    }

    return 1;
}

int uxmain(int multiuser, int port, char *dropprivuser, char **singleusercmd,
	   int clientfd, int (*clientfdread)(int fd), int daemon)
{
    signal(SIGPIPE, SIG_IGN);
    ick_proxy_setup();

    /*
     * Do the different setup required for single- and multi-user
     * mode.
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
    } else {
	if (pipe(signalpipe) < 0) {
	    fprintf(stderr, "ick-proxy: pipe: %s\n", strerror(errno));
	    exit(1);
	}
	new_fdstruct(signalpipe[0], FD_SIGNAL_PIPE);

	signal(SIGHUP, sighup);

	if (!configure_single_user())
	    return 1;

	if (singleusercmd) {
	    run_subprocess(singleusercmd);
	} else if (clientfd >= 0) {
	    new_fdstruct(clientfd, FD_CLIENT);
	}
    }

    if (daemon)
	daemonise(dropprivuser);

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
	maxfd = 0;
	for (i = j = 0; j < nfds; j++) {

	    if (fds[j].deleted) {
		sfree(fds[j].wdata);
		assert(!fds[j].lctx);  /* we don't free listeners currently */
		free_connection(fds[j].cctx);
		continue;
	    }
	    fds[i] = fds[j];

	    switch (fds[i].type) {
	      case FD_SIGNAL_PIPE:
	      case FD_CLIENT:
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
		    char buf[4096];
		    int j, ret, reread, child;

		    /*
		     * Read bytes from the signal pipe.
		     */
		    ret = read(fds[i].fd, buf, sizeof(buf));

		    reread = child = 0;
		    for (j = 0; j < ret; j++) {
			if (buf[j] == 'h') {
			    reread = 1;
			} else if (buf[j] == 'c') {
			    child = 1;
			}
		    }

		    if (child) {
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

		    if (reread) {
			/*
			 * Re-read our configuration on SIGHUP.
			 */
			if (!multiuser)
			    configure_single_user();
		    }
		}
		break;
	      case FD_CLIENT:
		if (FD_ISSET(fds[i].fd, &rfds)) {
		    /*
		     * Call the clientfdread function, and
		     * terminate if it returns true.
		     */
		    if (clientfdread(fds[i].fd))
			return 0;
		}
		break;
	      case FD_LISTENER:
		if (FD_ISSET(fds[i].fd, &rfds)) {
		    /*
		     * New connection has come in. Accept it.
		     */
		    struct fd *f;
		    struct sockaddr_in addr;
		    socklen_t addrlen = sizeof(addr);
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
