/*
 * xcopy: quickly pipe text data into, or out of, the primary X
 * selection
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int init_X(void);
void run_X(void);
void done_X(void);
void full_redraw(void);
void do_paste(Window window, Atom property, int Delete);

char *pname;			       /* program name */

void error (char *fmt, ...);

/* set from command-line parameters */
char *display = NULL;
int utf8 = 0;

/* selection data */
char *seltext;
int sellen, selsize;
#define SELDELTA 16384

/* functional parameters */
int reading;                           /* read instead of writing? */

int main(int ac, char **av) {
    int n;
    int eventloop;

    pname = *av;

    /* parse the command line arguments */
    while (--ac) {
	char *p = *++av;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error ("option `%s' expects a parameter", p);
	    display = *++av, --ac;
        } else if (!strcmp(p, "-r")) {
            reading = True;
        } else if (!strcmp(p, "-u")) {
            utf8 = 1;
	} else if (*p=='-') {
	    error ("unrecognised option `%s'", p);
	} else {
	    error ("no parameters required");
	}
    }

    if (!reading) {
        seltext = malloc(SELDELTA);
        if (!seltext)
            error ("out of memory");
        selsize = SELDELTA;
        sellen = 0;
        do {
            n = fread(seltext+sellen, 1, selsize-sellen, stdin);
            sellen += n;
            if (sellen >= selsize) {
                seltext = realloc(seltext, selsize += SELDELTA);
                if (!seltext)
                    error ("out of memory");
            }
        } while (n > 0);
        if (sellen == selsize) {
            seltext = realloc(seltext, selsize += SELDELTA);
            if (!seltext)
                error ("out of memory");
        }
        seltext[sellen] = '\0';
    }

    eventloop = init_X();
    if (!reading) {
        /*
         * If we are writing the selection, we must go into the
         * background now.
         */
        int pid = fork();
        if (pid < 0) {
            error("unable to fork: %s", strerror(errno));
        } else if (pid > 0) {
            /*
             * we are the parent; just exit
             */
            return 0;
        }
        /*
         * we are the child
         */
        close(0);
        close(1);
        close(2);
        chdir("/");
    }
    if (eventloop)
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

char *lcasename = "xcopy";
char *ucasename = "XCopy";

Display *disp = NULL;
Window ourwin = None;
Atom compound_text_atom;
int screen, wwidth, wheight;

Atom strtype = XA_STRING;

/*
 * Returns TRUE if we need to enter an event loop, FALSE otherwise.
 */
int init_X(void) {
    Window root;
    int x = 0, y = 0, width = 512, height = 128;
    int i, got = 0;
    XWMHints wm_hints;
    XSizeHints size_hints;
    XClassHint class_hints;
    XTextProperty textprop;
    XGCValues gcv;

    /* open the X display */
    disp = XOpenDisplay (display);
    if (!disp)
	error ("unable to open display");

    if (utf8) {
	strtype = XInternAtom(disp, "UTF8_STRING", False);
	if (!strtype)
	    error ("unable to get UTF8_STRING property");
    }

    /* get the screen and root-window */
    screen = DefaultScreen (disp);
    root = RootWindow (disp, screen);

    x = y = 0;
    width = height = 10;	       /* doesn't really matter */

    /* actually create the window */
    ourwin = XCreateSimpleWindow (disp, root, x, y, width, height,0, 
				  BlackPixel(disp, screen),
				  WhitePixel(disp, screen));

    /* resource class name */
    class_hints.res_name = lcasename;
    class_hints.res_class = ucasename;
    XSetClassHint (disp, ourwin, &class_hints);

    /* do selection fiddling */
    if (reading) {
        /*
         * We are reading the selection, so we must FIXME.
         */
        if (XGetSelectionOwner(disp, XA_PRIMARY) == None) {
            /* No primary selection, so use the cut buffer. */
            do_paste(DefaultRootWindow(disp), XA_CUT_BUFFER0, False);
            return False;
        } else {
            Atom sel_property = XInternAtom(disp, "VT_SELECTION", False);
            XConvertSelection(disp, XA_PRIMARY, strtype,
                              sel_property, ourwin, CurrentTime);
            return True;
        }
    } else {
        /*
         * We are writing to the selection, so we establish
         * ourselves as selection owner. Also place the data in
         * CUT_BUFFER0.
         */
        XSetSelectionOwner (disp, XA_PRIMARY, ourwin, CurrentTime);
        if (XGetSelectionOwner (disp, XA_PRIMARY) != ourwin)
            error ("unable to obtain primary X selection\n");
        compound_text_atom = XInternAtom(disp, "COMPOUND_TEXT", False);
	XChangeProperty(disp, DefaultRootWindow(disp), XA_CUT_BUFFER0,
			strtype, 8, PropModeReplace, seltext, sellen);
        return True;
    }
}

void run_X(void) {
    XEvent ev, e2;

    while (1) {
	XNextEvent (disp, &ev);
        if (reading) {
            switch (ev.type) {
              case SelectionNotify:
                if (ev.xselection.property != None)
                    do_paste(ev.xselection.requestor,
                             ev.xselection.property, True);
                return;
            }
        } else {
            switch (ev.type) {
              case SelectionClear:
                /* Selection has been cleared by another app. */
                return;
              case SelectionRequest:
                e2.xselection.type = SelectionNotify;
                e2.xselection.requestor = ev.xselectionrequest.requestor;
                e2.xselection.selection = ev.xselectionrequest.selection;
                e2.xselection.target = ev.xselectionrequest.target;
                e2.xselection.time = ev.xselectionrequest.time;
                if (ev.xselectionrequest.target == strtype) {
                    XChangeProperty (disp, ev.xselectionrequest.requestor,
                                     ev.xselectionrequest.property, strtype,
                                     8, PropModeReplace, seltext, sellen);
                    e2.xselection.property = ev.xselectionrequest.property;
                } else if (ev.xselectionrequest.target == compound_text_atom) {
                    XTextProperty tp;
                    XmbTextListToTextProperty (disp, &seltext, 1,
                                               XCompoundTextStyle, &tp);
                    XChangeProperty (disp, ev.xselectionrequest.requestor,
                                     ev.xselectionrequest.property,
                                     ev.xselectionrequest.target,
                                     tp.format, PropModeReplace,
                                     tp.value, tp.nitems);
                    e2.xselection.property = ev.xselectionrequest.property;
                } else {
                    e2.xselection.property = None;
                }
                XSendEvent (disp, ev.xselectionrequest.requestor, False, 0, &e2);
            }
        }
    }
}

void done_X(void) {
    int i;

    if (ourwin != None)
	XDestroyWindow (disp, ourwin);
    if (disp)
	XCloseDisplay (disp);
}

void do_paste(Window window, Atom property, int Delete) {
    Atom actual_type;
    int actual_format, i;
    long nitems, bytes_after, nread;
    unsigned char *data;

    nread = 0;
    while (XGetWindowProperty(disp, window, property, nread / 4, SELDELTA,
                              Delete, AnyPropertyType, &actual_type,
                              &actual_format, &nitems, &bytes_after,
                              (unsigned char **) &data) == Success) {
	/*
	 * We expect all returned chunks of data to be multiples of
	 * 4 bytes (because we can only request the subsequent
	 * starting offset in 4-byte increments). Of course you can
	 * store an odd number of bytes in a selection, so this
	 * can't be the case every time XGetWindowProperty returns;
	 * but it should be the case every time it returns _and
	 * there is more data to come_.
	 * 
	 * Hence, whenever XGetWindowProperty returns, we verify
	 * that the size of the data returned _last_ time was
	 * divisible by 4.
	 */
	if (nitems > 0)
	    assert((nread & 3) == 0);

        if (actual_type == strtype && nitems > 0) {
            assert(actual_format == 8);
            fwrite(data, 1, nitems, stdout);
            nread += nitems;
        }
        XFree(data);
        if (actual_type != strtype || nitems == 0)
            break;
    }
}
