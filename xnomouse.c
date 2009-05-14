/* xnomouse  disable X mouse input until a particular key is pressed */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

void error(char *fmt, ...);
void init_X(void);
void run_X(void);
void done_X(void);

char *pname;			       /* program name */

/* set from command-line parameters */
char *display = NULL;

int mousefd;

int main(int ac, char **av) {
    pname = *av;

    /* parse the command line arguments */
    while (--ac) {
	char *p = *++av;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error ("option `%s' expects a parameter", p);
	    display = *++av, --ac;
	} else if (*p=='-') {
	    error ("unrecognised option `%s'", p);
	} else {
	    error ("unexpected argument `%s'", p);
	}
    }

    init_X();
    run_X();
    done_X();

    return 0;
}

/* handle errors */

void error (char *fmt, ...) {
    va_list ap;
    char errbuf[200];

    done_X();
    va_start (ap, fmt);
    vsprintf (errbuf, fmt, ap);
    va_end (ap);
    fprintf (stderr, "%s: %s\n", pname, errbuf);
    exit (1);
}

/* begin the X interface */

char *lcasename = "xnomouse";
char *ucasename = "XNoMouse";

Display *disp = NULL;

void init_X(void) {
    Window root;

    /* open the X display */
    disp = XOpenDisplay (display);
    if (!disp)
	error ("unable to open display");

    root = XDefaultRootWindow(disp);

    XGrabPointer(disp, root, False,
		 ButtonPressMask | ButtonReleaseMask |
		 PointerMotionMask,
		 GrabModeAsync, GrabModeAsync,
		 None, None, CurrentTime);
    XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync,
		  CurrentTime);
}

void run_X(void) {
    XEvent ev;
    int keycode;

    while (1) {
	XNextEvent (disp, &ev);
	switch (ev.type) {
	  case KeyPress:
	    keycode = XKeycodeToKeysym(disp, ev.xkey.keycode, 0);
	    if (keycode == 'q') {
		XUngrabPointer(disp, ev.xkey.time);
		XUngrabKeyboard(disp, ev.xkey.time);
		return;
	    }
	    break;
	}
    }
}

void done_X(void) {
    if (disp)
	XCloseDisplay (disp);
}
