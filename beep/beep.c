/*
 * beep: make a beeping noise, by any means necessary
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifndef NO_X11
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include <unistd.h>
#include <fcntl.h>

const char usagemsg[] =
    "usage: beep [ -v ] ["
#ifndef NO_X11
#ifndef NO_XKB
    " -Xkb |"
#endif
    " -X |"
#endif
    " -T | -S ]\n"
    "where: "
#ifndef NO_X11
#ifndef NO_XKB
    "-Xkb   beep using the Xkb extension or not at all\n       "
#endif
    "-X     beep using the X display or not at all\n       "
#endif
    "-T     beep using /dev/tty or not at all\n"
    "       -S     beep using standard output or not at all\n"
    "       -v     log failed and successful beep attempts\n"
    " also: beep --version              report version number\n"
    "       beep --help                 display this help text\n"
    "       beep --licence              display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "beep is copyright 2006 Simon Tatham.\n"
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
        while (*p && isspace((unsigned char)*p)) p++;
        for (q = p; *q && !isspace((unsigned char)*q) && *q != '$'; q++);
        if (*q) *q = '\0';
        printf("beep revision %s", p);
    } else {
        printf("beep: unknown version");
    }
#ifdef NO_X11
    printf(", compiled without X11");
#elif defined NO_XKB
    printf(", compiled without Xkb");
#endif
    putchar('\n');
}

int main(int argc, char **argv) {
#ifndef NO_X11
    char *display = NULL;
#endif
    int verbose = 0;
    enum {
        XKB = 1, X11 = 2, TTY = 4, STDOUT = 8
    } mode = XKB | X11 | TTY | STDOUT;
    char errbuf[4096];
    int errlen = 0;
    int done = 0;
    char *pname = argv[0];

    /* parse the command line arguments */
    while (--argc) {
	char *p = *++argv;

#ifndef NO_X11
	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!argv[1]) {
		fprintf(stderr, "%s: option `%s' expects a parameter\n",
			pname, p);
		return 1;
	    }
	    display = *++argv, --argc;
        } else if (!strcmp(p, "-X")) {
            mode = X11;
#ifndef NO_XKB
        } else if (!strcmp(p, "-Xkb")) {
            mode = XKB;
#endif
        } else
#endif
	if (!strcmp(p, "-T")) {
            mode = TTY;
        } else if (!strcmp(p, "-S")) {
            mode = STDOUT;
        } else if (!strcmp(p, "-v")) {
            verbose = 1;
        } else if (!strcmp(p, "--help")) {
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
	    fprintf(stderr, "%s: parameter `%s' unexpected\n", pname, p);
	    return 1;
	}
    }

#ifndef NO_X11
    if (!done && (mode & (XKB|X11))) {
	Display *disp = XOpenDisplay(display);
	if (!disp) {
	    errlen += sprintf(errbuf+errlen,
			      "%s: unable to open X display\n", pname);
	} else {
#ifndef NO_XKB
            if (!done && (mode & XKB)) {
                if (XkbBell(disp, None, 0, NULL)) {
                    XCloseDisplay(disp);
                    if (verbose) {
                        errlen += sprintf(errbuf+errlen,
                                          "%s: successfully beeped via Xkb\n", pname);
                    }
                    done = 1;
                }
            }
#endif
            if (!done && (mode & X11)) {
                XBell(disp, 0);
                XCloseDisplay(disp);
                if (verbose) {
                    errlen += sprintf(errbuf+errlen,
                                      "%s: successfully beeped via X11\n", pname);
                }
                done = 1;
            }
	}
    }
#endif

    if (!done && (mode & TTY)) {
	int fd = open("/dev/tty", O_WRONLY);
	if (fd < 0) {
	    errlen += sprintf(errbuf+errlen,
			      "%s: unable to open /dev/tty: %s\n",
			      pname, strerror(errno));
	} else {
	    if (write(fd, "\007", 1) < 0) {
		errlen += sprintf(errbuf+errlen, "%s: unable to write to"
				  " /dev/tty: %s\n", pname, strerror(errno));
	    } else {
		if (verbose) {
		    errlen += sprintf(errbuf+errlen, "%s: successfully beeped"
				      " via /dev/tty\n", pname);
		}
		done = 1;
	    }
	    close(fd);
	}
    }

    if (!done && (mode & STDOUT)) {
	if (write(1, "\007", 1) < 0) {
	    errlen += sprintf(errbuf+errlen, "%s: unable to write to standard "
			      "output: %s\n", pname, strerror(errno));
	} else {
	    if (verbose) {
		errlen += sprintf(errbuf+errlen, "%s: successfully beeped"
				  " via standard output\n", pname);
	    }
	    done = 1;
	}
    }

    if (verbose || !done) {
	errbuf[errlen] = '\0';
	fputs(errbuf, stderr);
    }

    return (done ? EXIT_SUCCESS : EXIT_FAILURE);
}
