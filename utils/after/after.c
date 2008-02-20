/*
 * after: wait for an unrelated process to terminate, returning
 * its exit code if possible.
 */

/*
 * Possible future extensions:
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

int after_ptrace(int pid)
{
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
	/*
	 * If we can't attach to the process, that's not a fatal
	 * error; it just means we fall back to the next available
	 * method.
	 */
	errlen += sprintf(errbuf+errlen,
			  "%s: ptrace(PTRACE_ATTACH, %d): %.256s\n",
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
	    if (ptrace(PTRACE_CONT, pid, NULL, 0) < 0) {
		perror("ptrace(PTRACE_CONT)");
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
