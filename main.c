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

#include "filter.h"

sig_atomic_t exitcode = -1;

void sigchld(int sig)
{
    int pid;
    exitcode = wait(&pid);	       /* reap the exit code */
}

struct termios oldattrs, newattrs;

void attrsonexit(void)
{
    tcsetattr(0, TCSANOW, &oldattrs);
}

struct mainopts {
    int pipe;
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
	} else
	    return tstate_option(state, shortopt, longopt, value);
    }

    if (shortopt == 'p') {
	if (value)
	    return OPT_SPURIOUSARG;
	opts->pipe = TRUE;
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

    opts.pipe = TRUE;

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
			}
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
	    close(2);
	    dup2(slavew, 2);
	    setsid();
	    setpgrp();
	    tcsetpgrp(0, getpgrp());
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
	fd_set rset;
	char buf[65536];
	int ret;
	double wait;
	int itimeout, otimeout;
	struct timeval tv, *ptv;

	if (iwait && owait)
	    wait = (iwait < owait ? iwait : owait);
	else if (iwait)
	    wait = iwait;
	else if (owait)
	    wait = owait;
	else
	    wait = 0.0;

	if (wait) {
	    ptv = &tv;
	    tv.tv_sec = (int)wait;
	    tv.tv_usec = (int)1000000 * (wait - tv.tv_sec);
	} else {
	    ptv = NULL;
	}

	FD_ZERO(&rset);
	FD_SET(masterr, &rset);
	FD_SET(0, &rset);

	ret = select(masterr+1, &rset, NULL, NULL, ptv);

	itimeout = otimeout = FALSE;
	if (ret == 0) {
	    double left = (ret == 0 ? 0.0 : tv.tv_sec + tv.tv_usec/1000000.0);
	    wait -= left;
	    if (iwait) {
		iwait -= wait;
		assert(iwait >= 0);
		if (!iwait)
		    itimeout = TRUE;
	    }
	    if (owait) {
		owait -= wait;
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
		if (ret <= 0)
		    break;
	    } else
		ret = 0;

	    translated = translate(state, buf, ret, &translen, &owait, 0);

	    if (translen > 0)
		write(1, translated, translen);

	    free(translated);
	}
	if (FD_ISSET(0, &rset) || itimeout) {
	    char *translated;
	    int translen;

	    if (FD_ISSET(0, &rset)) {
		ret = read(0, buf, sizeof(buf));
		if (ret <= 0)
		    break;
	    } else
		ret = 0;

	    translated = translate(state, buf, ret, &translen, &iwait, 1);

	    if (translen)
		write(masterw, translated, translen);

	    free(translated);
	}
    }

    if (exitcode < 0) {
	int pid;
	exitcode = wait(&pid);
    }

    tstate_done(state);

    return exitcode;
}
