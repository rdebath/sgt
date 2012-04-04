/* xwaitbell  wait for an X bell to occur */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

void error(char *fmt, ...);
void init_X(void);
void run_X(void);
void done_X(void);
void full_redraw(void);

char *pname;			       /* program name */

/* set from command-line parameters */
char *display = NULL;

int main(int ac, char **av) {
    pname = *av;

    /* parse the command line arguments */
    while (--ac) {
	char *p = *++av;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error("option `%s' expects a parameter", p);
	    display = *++av, --ac;
	} else if (*p=='-') {
	    error("unrecognised option `%s'", p);
	}
    }

    init_X();
    run_X();
    done_X();

    return 0;
}

/* handle errors */

void error(char *fmt, ...) {
    va_list ap;
    char errbuf[200];

    done_X();
    va_start(ap, fmt);
    vsprintf(errbuf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s: %s\n", pname, errbuf);
    exit(1);
}

/* begin the X interface */

char *lcasename = "xdispchar";
char *ucasename = "XDispChar";

Display *disp = NULL;
Window ourwin = None;
Pixmap ourpm = None;
GC ourgc = None;
int screen, wwidth, wheight;

int xkb_event;

void init_X(void) {
    int major, minor;

    /* open the X display */
    disp = XOpenDisplay(display);
    if (!disp)
	error("unable to open display");

    major = XkbMajorVersion;
    minor = XkbMinorVersion;
    if (!XkbQueryExtension(disp, NULL, &xkb_event, NULL, &major, &minor))
        error("XKB extension not supported");

    XkbSelectEvents(disp, XkbUseCoreKbd, XkbBellNotifyMask, XkbBellNotifyMask);
}

void run_X(void) {
    XEvent ev;
    int keysym;

    do {
	XNextEvent(disp, &ev);
    } while (ev.type != xkb_event);
}

void done_X(void) {
    if (disp)
	XCloseDisplay(disp);
}
