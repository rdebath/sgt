/*
 * filter/main.c - main program.
 */

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

int main(int argc, char **argv)
{
    int errors = 0;
    int masterfd, slavefd, pid;
    struct termios oldattrs, newattrs;
    char ptyname[FILENAME_MAX];

    /*
     * Process command line.
     */
    while (--argc) {
	char *p = *++argv;
	if (*p == '-') {
	    if (!strcmp(p, "--")) {
		--argc, ++argv;	       /* point at argument after "--" */
		break;		       /* finish processing options */
	    } else if (p[1] == '-') {
		fprintf(stderr, "unknown long option: %s\n", p);
		errors = 1;
	    } else {
		char c;
		while ((c = *++p) != '\0') {
		    fprintf(stderr, "unknown short option: -%c\n", c);
		    errors = 1;
		}
	    }
	} else {
	    break;
	}
    }
    if (errors)
	return 1;

    /*
     * Allocate the pty.
     */
    masterfd = pty_get(ptyname);
    slavefd = open(ptyname, O_RDWR);
    if (slavefd < 0) {
	perror("slave pty: open");
	return 1;
    }

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
	close(masterfd);
	close(0);
	close(1);
	close(2);
	fcntl(slavefd, F_SETFD, 0);    /* don't close on exec */
	dup2(slavefd, 0);
	dup2(slavefd, 1);
	dup2(slavefd, 2);
	setsid();
	setpgrp();
	tcsetpgrp(0, getpgrp());
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
     * Now we're the parent. Close the slave fd and start copying
     * stuff back and forth.
     */
    close(slavefd);

    tcgetattr(0, &oldattrs);
    newattrs = oldattrs;
    newattrs.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR);
    newattrs.c_oflag &= ~(ONLCR | OCRNL);
    newattrs.c_lflag &= ~(ISIG | ICANON | ECHO);
    tcsetattr(0, TCSADRAIN, &newattrs);

    while (1) {
	fd_set rset;
	char buf[4096];
	int ret;

	FD_ZERO(&rset);
	FD_SET(masterfd, &rset);
	FD_SET(0, &rset);

	ret = select(masterfd+1, &rset, NULL, NULL, NULL);

	if (ret < 0) {
	    perror("select");
	    return 1;
	}

	if (FD_ISSET(masterfd, &rset)) {
	    ret = read(masterfd, buf, sizeof(buf));
	    if (ret <= 0)
		break;
	    write(1, buf, ret);
	}
	if (FD_ISSET(0, &rset)) {
	    ret = read(0, buf, sizeof(buf));
	    if (ret <= 0)
		break;
	    write(masterfd, buf, ret);
	}
    }

    if (exitcode < 0) {
	int pid;
	exitcode = wait(&pid);
    }

    tcsetattr(0, TCSANOW, &oldattrs);

    return exitcode;
}
