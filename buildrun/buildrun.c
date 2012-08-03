/*
 * buildrun.c: wait to run one command until an instance of another
 * completes successfully
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Implementation strategy
 * -----------------------
 *
 * The basic plan for making buildrun -r wait for buildrun -w is that
 * buildrun -w creates a named pipe, opens it for writing, and does
 * not actually write to it; then buildrun -r opens the same named
 * pipe for reading. When buildrun -w terminates, anything which was
 * trying to read from the pipe receives EOF, so buildrun -r can wake
 * up and look to see if the run was successful.
 *
 * buildrun -w's report of success or failure is done by the very
 * simple approach of deleting the pipe file before terminating, if
 * the run was successful. So after buildrun -r receives EOF on the
 * pipe, it immediately tries to open it again - and if it gets
 * ENOENT, that's the success condition. Otherwise it reopens the pipe
 * and blocks again until another buildrun -w succeeds.
 *
 * If the run was unsuccessful, buildrun -r must block immediately and
 * stay blocked until buildrun -w next runs. This is achieved
 * automatically by the above strategy, since attempting to open a
 * pipe for reading when nothing is trying to write it will cause the
 * open(2) call to block until something opens the pipe for writing.
 *
 * Likewise, if the last buildrun -w was unsuccessful and no -w run is
 * active at the moment buildrun -r starts in the first place,
 * buildrun -r must block until one starts up - and again, this is
 * done automatically by the attempt to open the pipe for reading.
 *
 * Of course, the blocking open behaviour of named pipes works both
 * ways: if buildrun -w tried to open the pipe for writing when no
 * buildrun -r is trying to read it, _it_ would block. For that
 * reason, buildrun -w actually opens with O_RDWR, so it can't block.
 *
 * Just in case the user tries to run two buildrun -w instances on the
 * same control file, we also flock(LOCK_EX) the named pipe once we've
 * opened it, so that the second -w instance can cleanly report the
 * error condition.
 *
 * Finally, there is a small race condition in the mechanism I've just
 * described. Suppose one buildrun -w instance finishes and deletes
 * its pipe file - but before buildrun -r manages to unblock and
 * notice the file's absence, another buildrun -w starts up and
 * recreates it. This is a more or less theoretical risk in the
 * interactive environments where I expect this tool to be used, but
 * even so, I like to solve these problems where I can; what I do is
 * to create not just a named pipe, but a _subdirectory_ containing a
 * named pipe. So a successful buildrun -w deletes the pipe and
 * removes the containing directory; a subsequent buildrun -w will
 * create a fresh directory and pipe. Then buildrun -r starts by
 * setting its _cwd_ to the directory containing the pipe - which
 * means that if one buildrun -w finishes and another one starts,
 * buildrun -r will still have its cwd pointing at the orphaned
 * directory from the first, and won't accidentally find the pipe file
 * created by the second in its own directory.
 */

#define PIPEFILE "pipe"

char *ctldir, *pipepath, **command;

int writemode(void)
{
    pid_t pid;
    int fd, status;

    if (mkdir(ctldir, 0700) < 0 && errno != EEXIST) {
        fprintf(stderr, "buildrun: %s: mkdir: %s\n", ctldir, strerror(errno));
        return 1;
    }
    if (mknod(pipepath, S_IFIFO | 0600, 0) < 0 && errno != EEXIST) {
        fprintf(stderr, "buildrun: %s: mknod: %s\n", pipepath,strerror(errno));
        return 1;
    }
    fd = open(pipepath, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "buildrun: %s: open: %s\n", pipepath, strerror(errno));
        return 1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EAGAIN) {
            fprintf(stderr, "buildrun: another write process "
                    "is already active\n");
        } else {
            fprintf(stderr, "buildrun: %s: flock: %s\n",
                    pipepath, strerror(errno));
        }
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "buildrun: fork: %s\n", strerror(errno));
        return 1;
    } else if (pid == 0) {
        close(fd);
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

    if (status == 0) {
        if (remove(pipepath) < 0) {
            fprintf(stderr, "buildrun: %s: remove: %s\n", pipepath,
                    strerror(errno));
            return 1;
        }
        if (rmdir(ctldir) < 0) {
            fprintf(stderr, "buildrun: %s: rmdir: %s\n", ctldir,
                    strerror(errno));
            return 1;
        }
    }

    return status;
}

int readmode()
{
    pid_t pid;
    int fd, ret, status;
    char buf[1];

    /*
     * Our strategy against race conditions relies on changing our cwd
     * to the control dir, but that will confuse the user command if
     * we exec one afterwards. So we do most of our work in a
     * subprocess, and don't pollute our main process context.
     */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "buildrun: fork: %s\n", strerror(errno));
        return 1;
    } else if (pid == 0) {
        /*
         * The main show.
         */
        if (chdir(ctldir) < 0) {
            if (errno == ENOENT) {
                /*
                 * If the control directory doesn't exist at all, it
                 * must be because the last -w run completed
                 * successfully, so we immediately return success.
                 */
                return 0;
            }
            fprintf(stderr, "buildrun: %s: chdir: %s\n",
                    ctldir, strerror(errno));
            return 1;
        }
        while (1) {
            fd = open(PIPEFILE, O_RDONLY);
            if (fd < 0) {
                if (errno == ENOENT)
                    return 0;          /* success! */

                fprintf(stderr, "buildrun: %s: open: %s\n",
                        pipepath, strerror(errno));
                return 1;
            }

            ret = read(fd, buf, 1);
            if (ret < 0) {
                fprintf(stderr, "buildrun: %s: read: %s\n",
                        pipepath, strerror(errno));
                return 1;
            } else if (ret > 0) {
                fprintf(stderr, "buildrun: %s: read: unexpectedly received "
                        "some data!\n", pipepath, strerror(errno));
                return 1;
            }
            close(fd);
        }
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

    if (status != 0)                   /* something went wrong in child */
        return status;

    /*
     * In -r mode, running a command is optional; in the absence of
     * one, we simply return true, so the user can invoke as 'buildrun
     * -r file && complex shell command'.
     */
    if (!command)
        return 0;

    execvp(command[0], command);
    fprintf(stderr, "buildrun: %s: exec: %s\n", command[0], strerror(errno));
    return 127;
}

const char usagemsg[] =
    "usage: buildrun -w <control_dir> <command> [<args>...]\n"
    "   or: buildrun -r <control_dir> [<command> [<args>...]]\n"
    "where: -w               write mode: run a build command\n"
    "       -r               read mode: wait for a build command to succeed\n"
    "       <control_dir>    directory to use for communication; suggest in /tmp\n"
    " also: buildrun --version              report version number\n"
    "       buildrun --help                 display this help text\n"
    "       buildrun --licence              display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "buildrun is copyright 2012 Simon Tatham.\n"
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
#define SVN_REV "$Revision$"
    char rev[sizeof(SVN_REV)];
    char *p, *q;

    strcpy(rev, SVN_REV);

    for (p = rev; *p && *p != ':'; p++);
    if (*p) {
        p++;
        while (*p && isspace(*p)) p++;
        for (q = p; *q && *q != '$'; q++);
        if (*q) *q = '\0';
        printf("buildrun revision %s\n", p);
    } else {
        printf("buildrun: unknown version\n");
    }
}

int main(int argc, char **argv)
{
    enum { UNSPEC, READ, WRITE } mode = UNSPEC;
    int doing_opts = 1;

    ctldir = NULL;
    command = NULL;

    while (--argc > 0) {
        char *p = *++argv;
        if (doing_opts && *p == '-') {
            if (!strcmp(p, "-w")) {
                mode = WRITE;
            } else if (!strcmp(p, "-r")) {
                mode = READ;
            } else if (!strcmp(p, "--")) {
                doing_opts = 0;
            } else if (!strcmp(p, "--version")) {
                version();
                return 0;
            } else if (!strcmp(p, "--help")) {
                usage();
                return 0;
            } else if (!strcmp(p, "--licence") || !strcmp(p, "--license")) {
                licence();
                return 0;
            } else {
                fprintf(stderr, "buildrun: unrecognised option '%s'\n", p);
                return 1;
            }
        } else {
            if (!ctldir) {
                ctldir = p;
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

    if (!ctldir) {
        fprintf(stderr, "buildrun: expected a control directory name\n");
        return 1;
    }

    pipepath = malloc(strlen(ctldir) + strlen(PIPEFILE) + 2);
    if (!pipepath) {
        fprintf(stderr, "buildrun: out of memory\n");
        return 1;
    }
    sprintf(pipepath, "%s/%s", ctldir, PIPEFILE);

    if (mode == WRITE) {
        return writemode();
    } else if (mode == READ) {
        return readmode();
    }
}
