/*
 * Somewhat silly utility to communicate an exit status between two
 * unrelated processes.
 *
 * The setup is: suppose your conceptual aim is to run two processes
 * in conditional sequence, as "a && b" would. However, process a
 * generates a lot of output, and you don't want it cluttering the
 * scrollback of the terminal in which you're running b - but
 * neither do you want to discard a's output, because it's
 * _sometimes_ useful. Also, a may fail, in which case you might
 * want to tweak things and retry.
 * 
 * (The particular situation of this form which I had in mind when I
 * wrote it was that a is a large and verbose compilation process,
 * and b involves running the resulting program through some tests
 * or debugging.)
 *
 * buildrun will let you run a in one terminal and b in another, and
 * will not start b until an instance of a completes successfully -
 * but if the last instance of a _did_ complete successfully, will
 * let you start b any time you like.
 *
 * Usage:
 * 
 * Create an empty file on a local filesystem (don't get NFS
 * involved). Let's say it's /tmp/foo. Now, in your a-window, run
 * 
 *   buildrun -w /tmp/foo <command a>
 * 
 * and then, in your b-window, run
 *
 *   buildrun -r /tmp/foo <command b>
 *
 * The former will write an exit status to the file when a
 * completes. The latter will wait to launch command b until command
 * a is not currently running and the file contains a _successful_
 * exit status - which might be immediate if it already contains
 * that status from a terminated run of a, or it might involve
 * waiting for a run of a to complete, or it might involve waiting
 * for a run of a to _start_ and then complete successfully - or it
 * might have to wait patiently while multiple runs of a start and
 * fail, until finally one succeeds.
 *
 * Dependencies:
 *
 * This program relies on the 'inotify' system call suite, which is
 * specific to Linux and requires a kernel version of 2.6.13 or
 * greater. Also, the glibc support for those system calls first
 * appeared in glibc 2.4.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/inotify.h>

int main(int argc, char **argv)
{
    enum { UNSPEC, READ, WRITE } mode = UNSPEC;
    int doing_opts = 1;
    char *lockfile = NULL;
    char **command = NULL;

    while (--argc > 0) {
	char *p = *++argv;
	if (doing_opts && *p == '-') {
	    if (!strcmp(p, "-w"))
		mode = WRITE;
	    else if (!strcmp(p, "-r"))
		mode = READ;
	    else if (!strcmp(p, "--"))
		doing_opts = 0;
	    else {
		fprintf(stderr, "buildrun: unrecognised option '%s'\n", p);
		return 1;
	    }
	} else {
	    if (!lockfile) {
		lockfile = p;
	    } else {
		command = argv;
		break;
	    }	
	}
    }

    if (mode == UNSPEC) {
	fprintf(stderr, "buildrun: expected -w or -r\n");
	return 1;
    }

    if (!lockfile) {
	fprintf(stderr, "buildrun: expected a lock file name\n");
	return 1;
    }

    if (mode == WRITE) {
	/*
	 * In write mode, we are fairly simple. We open the lock
	 * file for writing, and exclusively lock it; we truncate it
	 * to zero size (indicating in-progress); then we perform
	 * our command, write its exit code to the lock file, and
	 * terminate.
	 */
	int status, lockfd;

	if (!command) {
	    fprintf(stderr, "buildrun: -w mode requires a command\n");
	    return 1;
	}

	lockfd = open(lockfile, O_WRONLY);
	if (!lockfd) {
	    fprintf(stderr, "buildrun: %s: open: %s\n", lockfile,
		    strerror(errno));
	    return 1;
	}

	if (flock(lockfd, LOCK_EX) < 0) {
	    fprintf(stderr, "buildrun: %s: flock(LOCK_EX): %s\n", lockfile,
		    strerror(errno));
	    return 1;
	}

	ftruncate(lockfd, 0);

	{
	    pid_t pid = fork();
	    if (pid < 0) {
		fprintf(stderr, "buildrun: fork: %s\n", strerror(errno));
		return 1;
	    } else if (pid == 0) {
		int fdflags;

		fdflags = fcntl(lockfd, F_GETFD);
		if (fdflags != -1)
		    fcntl(lockfd, F_SETFD, fdflags | FD_CLOEXEC);
		execvp(command[0], command);
		fprintf(stderr, "buildrun: %s: exec: %s\n",
			command[0], strerror(errno));
		return 127;
	    }

	    while (1) {
		if (waitpid(pid, &status, 0) < 0) {
		    fprintf(stderr, "buildrun: wait: %s\n", strerror(errno));
		    return 1;
		}
		if (WIFEXITED(status)) {
		    status = WEXITSTATUS(status);
		    break;
		} else if (WIFSIGNALED(status)) {
		    status = 128 | WTERMSIG(status);
		    break;
		}
	    }
	}

	{
	    char buf[1];
	    buf[0] = status;
	    write(lockfd, buf, 1);
	}

	return status;
    }

    if (mode == READ) {
	/*
	 * In read mode:
	 * 
	 * We shared-lock the file (which causes us, in particular,
	 * to wait for any currently active buildrun -w to
	 * terminate) and read out the exit status. If it's zero, we
	 * are done; we release our shared lock, and then we execute
	 * a command if one was supplied.
	 *
	 * If it's non-zero, we then register an inotify on the file
	 * which will inform us the next time it is modified. Then
	 * we release our shared lock, wait for that to report
	 * something, and loop round again.
	 */
	int inofd = -1;
	int lockfd = open(lockfile, O_RDONLY);
	if (!lockfd) {
	    fprintf(stderr, "buildrun: %s: open: %s\n", lockfile,
		    strerror(errno));
	    return 1;
	}

	while (1) {
	    char buf[1];
	    int ret;

	    if (flock(lockfd, LOCK_SH) < 0) {
		fprintf(stderr, "buildrun: %s: flock(LOCK_SH): %s\n", lockfile,
			strerror(errno));
		return 1;
	    }

	    if (lseek(lockfd, 0, SEEK_SET) < 0) {
		fprintf(stderr, "buildrun: %s: lseek: %s\n", lockfile,
			strerror(errno));
		return 1;
	    }
	    if ((ret = read(lockfd, buf, 1)) < 0) {
		fprintf(stderr, "buildrun: %s: read: %s\n", lockfile,
			strerror(errno));
		return 1;
	    }
	    if (ret == 0) {
		fprintf(stderr, "buildrun: %s: file is empty\n", lockfile);
		return 1;
	    }

	    if (buf[0] == 0) {
		if (flock(lockfd, LOCK_UN) < 0) {
		    fprintf(stderr, "buildrun: %s: flock(LOCK_UN): %s\n",
			    lockfile, strerror(errno));
		    return 1;
		}
		close(lockfd);
		if (inofd >= 0)
		    close(inofd);
		break;
	    }

	    if (inofd < 0) {
		inofd = inotify_init();
		if (inofd < 0) {
		    fprintf(stderr, "buildrun: inotify_init: %s\n",
			    strerror(errno));
		    return 1;
		}
		if (inotify_add_watch(inofd, lockfile, IN_MODIFY) < 0) {
		    fprintf(stderr, "buildrun: %s: inotify_add_watch: %s\n",
			    lockfile, strerror(errno));
		    return 1;
		}
	    }

            if (flock(lockfd, LOCK_UN) < 0) {
                fprintf(stderr, "buildrun: %s: flock(LOCK_UN): %s\n",
                        lockfile, strerror(errno));
                return 1;
            }

	    while (1) {
		struct inotify_event ev;

		ret = read(inofd, &ev, sizeof(ev));
		if (ret < 0) {
		    fprintf(stderr, "buildrun: inotify fd: read: %s\n",
			    strerror(errno));
		    return 1;
		}
		if (ret != sizeof(ev)) {
		    fprintf(stderr, "buildrun: inotify fd: read: "
			    "unexpectedly little data returned\n");
		    return 1;
		}
		if (ev.mask & IN_MODIFY)
		    break;
	    }
	}
    }

    if (command) {
	execvp(command[0], command);
	fprintf(stderr, "buildrun: %s: exec: %s\n", command[0], strerror(errno));
	return 127;
    } else {
	/*
	 * In -r mode, exec-ing a command is optional; in the
	 * absence of one, we simply return true, so the user can
	 * invoke as 'buildrun -r file && complex shell command'.
	 */
	return 0;
    }
}
