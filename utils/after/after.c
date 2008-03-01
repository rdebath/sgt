/*
 * after: wait for an unrelated process to terminate, returning
 * its exit code if possible.
 */

/*
 * Possible future extensions:
 * 
 *  - improvements to existing methods
 *     * the ptrace method on MacOS has the really unpleasant
 * 	 property that if after is interrupted or killed then the
 * 	 traced process dies too. We could mitigate that by
 * 	 trapping some common fatal signals (certainly including
 * 	 SIGINT and SIGTERM) and have them PT_DETACH before
 * 	 self-reraising. Alternatively, we could fork off a
 * 	 subprocess to do the tracing, and a pipe communicating
 * 	 with the parent; then if the subprocess detected its pipe
 * 	 end being closed it could PT_DETACH. Then even kill -9
 * 	 against the primary `after' process would allow the
 * 	 subprocess to clean up.
 * 
 *  - alternatives to ptrace on systems supporting different
 *    process-debugging mechanisms
 * 
 *  - possibly an approach based on invoking ps?
 *     * with a longish interval, to prevent system overload
 *     * and we have to deal with the profusion of ps options. I
 * 	 suspect going by SUS/POSIX is the best thing here: set
 * 	 POSIXLY_CORRECT and expect ps to support the options
 * 	 given in SUS.
 *     * need to try to avoid race condition if the process ends
 * 	 and another one of the same name starts up. Printing the
 * 	 start time seems like the sensible thing, except that SUS
 * 	 doesn't give a standard option to do that; we can only
 * 	 print elapsed time since the process began, which means
 * 	 we have to be alert for off-by-1s errors if we wait 30s
 * 	 and the time only changes by 29 for some irritating
 * 	 reason.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char errbuf[4096];
int errlen = 0;
char *pname;

#ifndef NO_PTRACE

/*
 * ptrace-based waiting method, which gets the process's exit
 * code.
 */

#include <sys/ptrace.h>

#if !defined PTRACE_FLAVOUR_LINUX && !defined PTRACE_FLAVOUR_BSD
/*
 * Attempt to autodetect ptrace flavour.
 */
#if defined PTRACE_ATTACH

/*
 * Linux flavour.
 */
#define ATTACH_TO(pid) ptrace(PTRACE_ATTACH, pid, NULL, 0)
#define ATTACH_STR "ptrace(PTRACE_ATTACH, %d)"
#define CONTINUE(pid) ptrace(PTRACE_CONT, pid, NULL, 0)
#define CONT_STR "ptrace(PTRACE_CONT, %d)"

#elif defined PT_ATTACH

/*
 * BSD/MacOS flavour.
 */
#define ATTACH_TO(pid) ptrace(PT_ATTACH, pid, NULL, 0)
#define ATTACH_STR "ptrace(PT_ATTACH, %d)"
#define CONTINUE(pid) ptrace(PT_CONTINUE, pid, (caddr_t)1, 0)
#define CONT_STR "ptrace(PT_CONTINUE, %d)"

#else
#error Unable to autodetect ptrace flavour.
#endif /* ptrace flavour detection */

#endif /* outer ifdef containing flavour detection */

int after_ptrace(int pid)
{
    if (
#ifdef PTRACE_FLAVOUR_LINUX
	ptrace(PTRACE_ATTACH, pid, NULL, 0)
#else
	ptrace(PT_ATTACH, pid, NULL, 0)
#endif
	< 0) {
	/*
	 * If we can't attach to the process, that's not a fatal
	 * error; it just means we fall back to the next available
	 * method.
	 */
	errlen += sprintf(errbuf+errlen,
			  "%s: "ATTACH_STR": %.256s\n",
			  pname, pid, strerror(errno));
	return -1;
    }

    /*
     * Having successfully attached to the process, however, any
     * subsequent error is fatal.
     */

    while (1) {
	int wpid, wstatus;
	wpid = waitpid(pid, &wstatus, 0);
	if (wpid < 0) {
	    perror("wait");
	    return 127;
	} else if (WIFEXITED(wstatus)) {
	    return WEXITSTATUS(wstatus);
	} else {
	    if (CONTINUE(pid) < 0) {
		fprintf(stderr, "%s: "CONT_STR": %.256s\n",
			pname, pid, strerror(errno));
		return 127;
	    }
	}
    }
}

#endif

#ifndef NO_PROC

/*
 * /proc-based waiting method.
 */

int after_proc(int pid)
{
    char buf[128];

    sprintf(buf, "/proc/%d", pid);

    if (chdir(buf) < 0) {
	/*
	 * Fallback error.
	 */
	errlen += sprintf(errbuf+errlen,
			  "%s: chdir(\"%.32s\"): %.256s\n",
			  pname, buf, strerror(errno));
	return -1;
    }

    do {
	sleep(1);
    } while (getcwd(buf, sizeof(buf)));

    return 0;
}

#endif

#ifdef NO_PTRACE
#define NO_EXITCODE
#endif

const char usagemsg[] =
    "usage: after"
#ifndef NO_EXITCODE
    " [ -x | -z ]"
#endif
    " <pid>\n"
#ifndef NO_EXITCODE
    "where: -x     return an error if the process's exit code is unavailable\n"
    "       -z     return 0 instead of the process's exit code\n"
#endif
    " also: after --version             report version number\n"
    "       after --help                display this help text\n"
    "       after --licence             display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "after is copyright 2008 Simon Tatham.\n"
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
#define SVN_REV "$Revision: 6566 $"
    char rev[sizeof(SVN_REV)];
    char *p, *q;

    strcpy(rev, SVN_REV);

    for (p = rev; *p && *p != ':'; p++);
    if (*p) {
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        for (q = p; *q && !isspace((unsigned char)*q) && *q != '$'; q++);
        if (*q) *q = '\0';
        printf("after revision %s", p);
    } else {
        printf("after: unknown version");
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    int pid = -1;
    enum { UNWANTED, OPTIONAL, MANDATORY } exitcode = OPTIONAL;
    int ret;

    pname = argv[0];

    /* parse the command line arguments */
    while (--argc) {
	char *p = *++argv;

#ifndef NO_EXITCODE
	if (!strcmp(p, "-x")) {
            exitcode = MANDATORY;
        } else if (!strcmp(p, "-z")) {
            exitcode = UNWANTED;
        } else
#endif
	if (!strcmp(p, "--help")) {
	    usage();
	    return 0;
        } else if (!strcmp(p, "--version")) {
            version();
	    return 0;
        } else if (!strcmp(p, "--licence") || !strcmp(p, "--license")) {
            licence();
	    return 0;
	} else if (*p=='-') {
	    fprintf(stderr, "%s: unrecognised option `%s'\n", pname, p);
	    return 1;
	} else {
	    if (pid >= 0) {
		fprintf(stderr, "%s: parameter `%s' unexpected\n", pname, p);
		return 1;
	    } else {
		pid = atoi(p);
	    }
	}
    }

    if (pid < 0) {
	usage();
	return 0;
    }

#ifndef NO_PTRACE
    ret = after_ptrace(pid);
    if (ret >= 0)
	return (exitcode == UNWANTED ? 0 : ret);
#endif

    if (exitcode == MANDATORY) {
	/*
	 * If we reach here, the user has demanded the process's
	 * return code, and we haven't been able to get it. Print
	 * the error messages we accrued while trying, and abandon
	 * ship.
	 */
	fputs(errbuf, stderr);
	return 127;
    }

#ifndef NO_PROC
    ret = after_proc(pid);
    if (ret >= 0)
	return ret;
#endif

    /*
     * If we reach here, we have run out of all our options. Print
     * all our error messages, and return total failure.
     */
    fputs(errbuf, stderr);
    return 127;
}
