/*
 * filter/main.c - main program.
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "filter.h"

#define min(x,y) ( (x)<(y)?(x):(y) )
#define max(x,y) ( (x)>(y)?(x):(y) )

/* ----------------------------------------------------------------------
 * Chunk of code lifted from PuTTY's misc.c to manage buffers of
 * data to be written to an fd.
 */

#define BUFFER_GRANULE  512

typedef struct bufchain_tag {
    struct bufchain_granule *head, *tail;
    int buffersize;		       /* current amount of buffered data */
} bufchain;
struct bufchain_granule {
    struct bufchain_granule *next;
    int buflen, bufpos;
    char buf[BUFFER_GRANULE];
};

static void bufchain_init(bufchain *ch)
{
    ch->head = ch->tail = NULL;
    ch->buffersize = 0;
}

static int bufchain_size(bufchain *ch)
{
    return ch->buffersize;
}

static void bufchain_add(bufchain *ch, const void *data, int len)
{
    const char *buf = (const char *)data;

    if (len == 0) return;

    ch->buffersize += len;

    if (ch->tail && ch->tail->buflen < BUFFER_GRANULE) {
	int copylen = min(len, BUFFER_GRANULE - ch->tail->buflen);
	memcpy(ch->tail->buf + ch->tail->buflen, buf, copylen);
	buf += copylen;
	len -= copylen;
	ch->tail->buflen += copylen;
    }
    while (len > 0) {
	int grainlen = min(len, BUFFER_GRANULE);
	struct bufchain_granule *newbuf;
	newbuf = (struct bufchain_granule *)
	    malloc(sizeof(struct bufchain_granule));
	if (!newbuf) {
	    fprintf(stderr, "filter: out of memory\n");
	    exit(1);
	}
	newbuf->bufpos = 0;
	newbuf->buflen = grainlen;
	memcpy(newbuf->buf, buf, grainlen);
	buf += grainlen;
	len -= grainlen;
	if (ch->tail)
	    ch->tail->next = newbuf;
	else
	    ch->head = ch->tail = newbuf;
	newbuf->next = NULL;
	ch->tail = newbuf;
    }
}

static void bufchain_consume(bufchain *ch, int len)
{
    struct bufchain_granule *tmp;

    assert(ch->buffersize >= len);
    while (len > 0) {
	int remlen = len;
	assert(ch->head != NULL);
	if (remlen >= ch->head->buflen - ch->head->bufpos) {
	    remlen = ch->head->buflen - ch->head->bufpos;
	    tmp = ch->head;
	    ch->head = tmp->next;
	    free(tmp);
	    if (!ch->head)
		ch->tail = NULL;
	} else
	    ch->head->bufpos += remlen;
	ch->buffersize -= remlen;
	len -= remlen;
    }
}

static void bufchain_prefix(bufchain *ch, void **data, int *len)
{
    *len = ch->head->buflen - ch->head->bufpos;
    *data = ch->head->buf + ch->head->bufpos;
}

/* ----------------------------------------------------------------------
 * End of bufchain stuff. Main program begins.
 */

#define LOCALBUF_LIMIT 65536

sig_atomic_t exitcode = -1;
pid_t childpid = -1;

int signalpipe[2];

void sigchld(int sig)
{
    write(signalpipe[1], "C", 1);
}

struct termios oldattrs, newattrs;

void attrsonexit(void)
{
    tcsetattr(0, TCSANOW, &oldattrs);
}

struct mainopts {
    int pipe;
    int quit;
};

int option(struct mainopts *opts, struct tstate *state,
	   int shortopt, char *longopt, char *value)
{
    /*
     * Process standard options across all filter programs here;
     * hand everything else on to tstate_option.
     */
    if (longopt) {
	if (!strcmp(longopt, "pipe")) {
	    shortopt = 'p';
	} if (!strcmp(longopt, "quit")) {
	    shortopt = 'q';
	} else
	    return tstate_option(state, shortopt, longopt, value);
    }

    if (shortopt == 'p') {
	if (value)
	    return OPT_SPURIOUSARG;
	opts->pipe = TRUE;
    } else if (shortopt == 'q') {
	if (value)
	    return OPT_SPURIOUSARG;
	opts->quit = TRUE;
    } else
	return tstate_option(state, shortopt, longopt, value);

    return OPT_OK;
}

int main(int argc, char **argv)
{
    int errors = 0;
    int masterr, masterw, slaver, slavew, pid;
    double iwait = 0.0, owait = 0.0;
    char ptyname[FILENAME_MAX];
    tstate *state;
    struct mainopts opts;
    bufchain tochild, tostdout;
    int tochild_active, tostdout_active, fromstdin_active, fromchild_active;

    opts.pipe = FALSE;
    opts.quit = FALSE;

    state = tstate_init();

    /*
     * Process command line.
     */
    while (--argc > 0) {
	char *p = *++argv;
	if (*p == '-') {
	    if (!strcmp(p, "--")) {
		--argc, ++argv;	       /* point at argument after "--" */
		break;		       /* finish processing options */
	    } else if (p[1] == '-') {
		char *q;
		int ret;

		q = strchr(p, '=');
		if (q)
		    *q++ = '\0';

		ret = option(&opts, state, '\0', p+2, q);
		if (ret == OPT_MISSINGARG) {
		    assert(!q);

		    if (--argc > 0) {
			q = *++argv;
			ret = option(&opts, state, '\0', p+2, q);
		    } else {
			fprintf(stderr, "option %s expects an argument\n", p);
			errors = 1;
		    }
		}
		
		if (ret == OPT_UNKNOWN) {
		    fprintf(stderr, "unknown long option: %s\n", p);
		    errors = 1;
		} else if (ret == OPT_SPURIOUSARG) {
		    fprintf(stderr, "option %s does not expect an argument\n",
			    p);
		    errors = 1;
		}
	    } else {
		char c;
		int ret;

		while ((c = *++p) != '\0') {
		    ret = option(&opts, state, c, NULL, NULL);
		    if (ret == OPT_MISSINGARG) {
			char *q;

			if (p[1])
			    q = p+1;
			else if (--argc > 0)
			    q = *++argv;
			else {
			    fprintf(stderr, "option -%c expects an"
				    " argument\n", c);
			    errors = 1;
			    q = NULL;
			}
			if (q)
			    ret = option(&opts, state, c, NULL, q);
		    }

		    if (ret == OPT_UNKNOWN) {
			fprintf(stderr, "unknown option: -%c\n", c);
			errors = 1;
		    } else if (ret == OPT_SPURIOUSARG) {
			fprintf(stderr, "option -%c does not expect an"
				" argument\n", c);
			errors = 1;
		    }
		}
	    }
	} else {
	    tstate_argument(state, p);
	}
    }
    if (errors)
	return 1;

    /*
     * Allocate the pipe for transmitting signals back to the
     * top-level select loop.
     */
    if (pipe(signalpipe) < 0) {
	perror("pipe");
	return 1;
    }

    /*
     * Allocate the pty or pipes.
     */
    if (opts.pipe) {
	int p[2];
	if (pipe(p) < 0) {
	    perror("pipe");
	    return 1;
	}
	masterr = p[0];
	slavew = p[1];
	if (pipe(p) < 0) {
	    perror("pipe");
	    return 1;
	}
	slaver = p[0];
	masterw = p[1];
    } else {
	masterr = pty_get(ptyname);
	masterw = dup(masterr);
	slaver = open(ptyname, O_RDWR);
	slavew = dup(slaver);
	if (slaver < 0) {
	    perror("slave pty: open");
	    return 1;
	}
    }

    bufchain_init(&tochild);
    bufchain_init(&tostdout);
    tochild_active = tostdout_active = TRUE;
    fromchild_active = fromstdin_active = TRUE;
    tstate_ready(state, &iwait, &owait);

    /*
     * Fork and execute the command.
     */
    pid = fork();
    if (pid < 0) {
	perror("fork");
	return 1;
    }

    if (pid == 0) {
	int i;
	/*
	 * We are the child.
	 */
	close(masterr);
	close(masterw);

	fcntl(slaver, F_SETFD, 0);    /* don't close on exec */
	fcntl(slavew, F_SETFD, 0);    /* don't close on exec */
	close(0);
	dup2(slaver, 0);
	close(1);
	dup2(slavew, 1);
	if (!opts.pipe) {
	    int fd;
	    close(2);
	    dup2(slavew, 2);
	    setsid();
	    setpgrp();
	    tcsetpgrp(0, getpgrp());
	    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY);
		close(fd);
	    }
	    ioctl(slavew, TIOCSCTTY);
	}
	/* Close everything _else_, for tidiness. */
	for (i = 3; i < 1024; i++)
	    close(i);
	if (argc > 0) {
	    execvp(argv[0], argv);     /* assumes argv has trailing NULL */
	} else {
	    execl(getenv("SHELL"), getenv("SHELL"), NULL);
	}
	/*
	 * If we're here, exec has gone badly foom.
	 */
	perror("exec");
	exit(127);
    }

    /*
     * Now we're the parent. Close the slave fds and start copying
     * stuff back and forth.
     */
    close(slaver);
    close(slavew);
    childpid = pid;
    signal(SIGCHLD, sigchld);

    if (!opts.pipe) {
	tcgetattr(0, &oldattrs);
	newattrs = oldattrs;
	newattrs.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR);
	newattrs.c_oflag &= ~(ONLCR | OCRNL);
	newattrs.c_lflag &= ~(ISIG | ICANON | ECHO);
	atexit(attrsonexit);
	tcsetattr(0, TCSADRAIN, &newattrs);
    }

    while (1) {
	fd_set rset, wset;
	char buf[65536];
	int maxfd, ret;
	double twait;
	int itimeout, otimeout, used_iwait, used_owait;
	struct timeval tv, *ptv;

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	maxfd = 0;
	twait = HUGE_VAL;
	used_iwait = used_owait = FALSE;

	FD_SET(signalpipe[0], &rset);
	maxfd = max(signalpipe[0]+1, maxfd);

	if (tochild_active && bufchain_size(&tochild)) {
	    FD_SET(masterw, &wset);
	    maxfd = max(masterw+1, maxfd);
	}
	if (tostdout_active && bufchain_size(&tostdout)) {
	    FD_SET(1, &wset);
	    maxfd = max(1+1, maxfd);
	}
	if (fromstdin_active && bufchain_size(&tochild) < LOCALBUF_LIMIT) {
	    FD_SET(0, &rset);
	    maxfd = max(0+1, maxfd);
	    if (iwait) {
		used_iwait = TRUE;
		twait = min(twait, iwait);
	    }
	}
	if (fromchild_active && bufchain_size(&tostdout) < LOCALBUF_LIMIT) {
	    FD_SET(masterr, &rset);
	    maxfd = max(masterr+1, maxfd);
	    if (owait) {
		used_owait = TRUE;
		twait = min(twait, owait);
	    }
	}

	if (twait == HUGE_VAL) {
	    ptv = NULL;
	} else {
	    ptv = &tv;
	    tv.tv_sec = (int)twait;
	    tv.tv_usec = (int)1000000 * (twait - tv.tv_sec);
	}

	do {
	    ret = select(maxfd, &rset, &wset, NULL, ptv);
	} while (ret < 0 && (errno == EINTR || errno == EAGAIN));

	itimeout = otimeout = FALSE;
	if (ret == 0) {
	    double left = (ret == 0 ? 0.0 : tv.tv_sec + tv.tv_usec/1000000.0);
	    twait -= left;
	    if (iwait && used_iwait) {
		iwait -= twait;
		assert(iwait >= 0);
		if (!iwait)
		    itimeout = TRUE;
	    }
	    if (owait && used_owait) {
		owait -= twait;
		assert(owait >= 0);
		if (!owait)
		    otimeout = TRUE;
	    }
	}

	if (ret < 0) {
	    perror("select");
	    return 1;
	}

	if (FD_ISSET(masterr, &rset) || otimeout) {
	    char *translated;
	    int translen;

	    if (FD_ISSET(masterr, &rset)) {
		ret = read(masterr, buf, sizeof(buf));
		if (ret <= 0) {
		    /*
		     * EIO from a pty master just means end of
		     * file, annoyingly. Why can't it report
		     * ordinary EOF?
		     */
		    if (!opts.pipe && errno == EIO)
			ret = 0;
		    if (ret < 0) {
			perror("child process: read");
		    }
		    close(masterr);
		    fromchild_active = FALSE;
		    ret = 0;
		}
	    } else
		ret = 0;

	    if (ret || otimeout) {
		translated = translate(state, buf, ret, &translen, &owait, 0);

		if (translen > 0)
		    bufchain_add(&tostdout, translated, translen);

		free(translated);
	    }
	}
	if (FD_ISSET(0, &rset) || itimeout) {
	    char *translated;
	    int translen;

	    if (FD_ISSET(0, &rset)) {
		ret = read(0, buf, sizeof(buf));
		if (ret <= 0) {
		    if (ret < 0) {
			perror("stdin: read");
		    }
		    close(0);
		    fromstdin_active = FALSE;
		    ret = 0;
		}
	    } else
		ret = 0;

	    if (ret || itimeout) {
		translated = translate(state, buf, ret, &translen, &iwait, 1);

		if (translen)
		    bufchain_add(&tochild, translated, translen);

		free(translated);
	    }
	}
	if (FD_ISSET(1, &wset)) {
	    void *data;
	    int len, ret;
	    bufchain_prefix(&tostdout, &data, &len);
	    if ((ret = write(1, data, len)) < 0) {
		perror("stdout: write");
		close(1);
		close(masterr);
		tostdout_active = fromchild_active = FALSE;
	    } else
		bufchain_consume(&tostdout, ret);
	}
	if (FD_ISSET(masterw, &wset)) {
	    void *data;
	    int len;
	    bufchain_prefix(&tochild, &data, &len);
	    if ((ret = write(masterw, data, len)) < 0) {
		perror("child process: write");
		close(0);
		close(masterw);
		tochild_active = fromstdin_active = FALSE;
	    } else
		bufchain_consume(&tochild, ret);
	}
	if (FD_ISSET(signalpipe[0], &rset)) {
	    ret = read(signalpipe[0], buf, 1);
	    if (ret == 1 && buf[0] == 'C') {
		int pid, code;
		pid = wait(&code);     /* reap the exit code */
		if (pid == childpid)
		    exitcode = code;
	    }
	}

	/*
	 * If there can be no further data from a direction (the
	 * input fd has been closed and the buffered data is used
	 * up) but its output fd is still open, close it.
	 */
	if (!fromstdin_active && !bufchain_size(&tochild) && tochild_active) {
	    tochild_active = FALSE;
	    close(masterw);
	}
	if (!fromchild_active && !bufchain_size(&tostdout) && tostdout_active){
	    tostdout_active = FALSE;
	    close(1);
	}

	/*
	 * Termination condition with pipes is that there's still
	 * data flowing in at least one direction.
	 * 
	 * Termination condition for a pty-based run is that the
	 * child process hasn't yet terminated and/or there is
	 * still buffered data to send.
	 */
	if (opts.pipe) {
	    /*
	     * The condition `there is data flowing in a direction'
	     * unpacks as: there is at least potentially data left
	     * to be written (i.e. EITHER the input side of that
	     * direction is still open OR we have some buffered
	     * data left from before it closed) AND we are able to
	     * write it (the output side is still active).
	     */
	    if (tochild_active &&
		(fromstdin_active || bufchain_size(&tochild)))
		/* stdin -> child direction is still active */;
	    else if (tostdout_active &&
		     (fromchild_active || bufchain_size(&tostdout)))
		/* child -> stdout direction is still active */;
	    else
		break;		       /* neither is active any more */
	} else {
	    if (opts.quit) {
		if (exitcode >= 0 || !fromchild_active || !fromstdin_active ||
		    !tochild_active || !tostdout_active)
		    break;
	    } else {
		if (exitcode < 0)
		    /* process is still active */;
		else if (tochild_active && bufchain_size(&tochild))
		    /* data still to be sent to child's children */;
		else if (tostdout_active && bufchain_size(&tostdout))
		    /* data still to be sent to stdout */;
		else
		    break;		       /* terminate */
	    }
	}
    }

    close(masterw);
    close(masterr);

    if (exitcode < 0) {
	int pid, code;
	pid = wait(&code);
	exitcode = code;
    }

    tstate_done(state);

    return (WIFEXITED(exitcode) ? WEXITSTATUS(exitcode) :
	    128 | WTERMSIG(exitcode));
}
