/*
 * xcopy: quickly pipe text data into, or out of, the primary X
 * selection
 */

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
enum { STRING, CTEXT, UTF8, TARGETS, TIMESTAMP, CUSTOM } mode = STRING;
int use_clipboard = False;
char *custom_seltype = NULL;
int fork_when_writing = True;

/* selection data */
char *seltext;
int sellen, selsize;
#define SELDELTA 16384

/* functional parameters */
int reading;                           /* read instead of writing? */
int convert_to_ctext = True;	       /* Xmb convert to compound text? */
int verbose;

const char usagemsg[] =
    "usage: xcopy [ -r ] [ -u | -c ] [ -C ]\n"
    "where: -r       read X selection and print on stdout\n"
    "       no -r    read stdin and store in X selection\n"
    "       -u       work with UTF8_STRING type selections\n"
    "       -c       work with COMPOUND_TEXT type selections\n"
    "       -C       suppress automatic conversion to COMPOUND_TEXT\n"
    "       -b       read the CLIPBOARD rather than the PRIMARY selection\n"
    "       -t       get the list of targets available to retrieve\n"
    "       -T       get the time stamp of the selection contents\n"
    "       -a atom  get an arbitrary form of the selection data\n"
    "       -F       do not fork in write mode\n"
    "       -v       proceed verbosely when reading selection\n"
    " also: xcopy --version              report version number\n"
    "       xcopy --help                 display this help text\n"
    "       xcopy --licence              display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "xcopy is copyright 2001-2004,2008 Simon Tatham.\n"
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
        printf("xcopy revision %s\n", p);
    } else {
        printf("xcopy: unknown version\n");
    }
}

int main(int ac, char **av) {
    int n;
    int eventloop;

    pname = *av;

    /* parse the command line arguments */
    while (--ac > 0) {
	char *p = *++av;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error ("option `%s' expects a parameter", p);
	    display = *++av, --ac;
        } else if (!strcmp(p, "-r")) {
            reading = True;
        } else if (!strcmp(p, "-u")) {
            mode = UTF8;
        } else if (!strcmp(p, "-c")) {
            mode = CTEXT;
        } else if (!strcmp(p, "-C")) {
            convert_to_ctext = False;
        } else if (!strcmp(p, "-b")) {
            use_clipboard = True;
        } else if (!strcmp(p, "-t")) {
            mode = TARGETS;
        } else if (!strcmp(p, "-T")) {
            mode = TIMESTAMP;
        } else if (!strcmp(p, "-a")) {
	    if (--ac > 0)
		custom_seltype = *++av;
	    else
		error ("expected an argument to `-a'");
            mode = CUSTOM;
	} else if (!strcmp(p, "-F")) {
	    fork_when_writing = False;
	} else if (!strcmp(p, "-v")) {
	    verbose = True;
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
	    error ("unrecognised option `%s'", p);
	} else {
	    error ("no parameters required");
	}
    }

    if (!reading) {
	if (mode == TARGETS || mode == TIMESTAMP || mode == CUSTOM) {
	    error ("%s not supported in writing mode; use -r",
		   (mode == TARGETS ? "-t" :
		    mode == TIMESTAMP ? "-T" :
		    /* mode == CUSTOM ? */ "-a"));
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
    if (!reading && fork_when_writing) {
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
Atom compound_text_atom, targets_atom, timestamp_atom, atom_atom, integer_atom;
Atom multiple_atom, atom_pair_atom;
int screen, wwidth, wheight;

Atom strtype = XA_STRING;
Atom sel_atom = XA_PRIMARY;
Atom expected_type = (Atom)None;
int expected_format = 8;

static const char *translate_atom(Display *disp, Atom atom)
{
    if (atom == None)
	return "None";
    else
	return XGetAtomName(disp, atom);
}

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

    targets_atom = XInternAtom(disp, "TARGETS", False);
    timestamp_atom = XInternAtom(disp, "TIMESTAMP", False);
    atom_atom = XInternAtom(disp, "ATOM", False);
    atom_pair_atom = XInternAtom(disp, "ATOM_PAIR", False);
    multiple_atom = XInternAtom(disp, "MULTIPLE", False);
    integer_atom = XInternAtom(disp, "INTEGER", False);
    if (mode == UTF8) {
	strtype = XInternAtom(disp, "UTF8_STRING", False);
    } else if (mode == CTEXT) {
	strtype = XInternAtom(disp, "COMPOUND_TEXT", False);
    } else if (mode == TARGETS) {
	strtype = targets_atom;
	expected_type = atom_atom;
	expected_format = 32;
    } else if (mode == TIMESTAMP) {
	strtype = timestamp_atom;
	expected_type = integer_atom;
	expected_format = 32;
    } else if (mode == CUSTOM) {
	strtype = XInternAtom(disp, custom_seltype, True);
	if (!strtype)
	    error ("atom '%s' does not exist on the server", custom_seltype);
	expected_format = 0;
    }
    if (use_clipboard) {
        sel_atom = XInternAtom(disp, "CLIPBOARD", False);
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
         * We are reading the selection. Call XConvertSelection to
         * request transmission of the selection data in the
         * appropriate format; the X event loop will then wait to
         * receive the data from the selection owner.
	 *
	 * If there is no selection owner, look in the cut buffer
	 * property on the root window.
         */
        if (XGetSelectionOwner(disp, sel_atom) == None) {
            /* No primary selection, so use the cut buffer. */
	    if (verbose)
		fprintf(stderr, "no selection owner: trying cut buffer\n");
            if (strtype == XA_STRING)
		do_paste(DefaultRootWindow(disp), XA_CUT_BUFFER0, False);
            return False;
        } else {
            Atom sel_property = XInternAtom(disp, "VT_SELECTION", False);
	    if (verbose)
		fprintf(stderr, "calling XConvertSelection: selection=%s"
			" target=%s property=%s requestor=%08lx\n",
			translate_atom(disp, sel_atom),
			translate_atom(disp, strtype),
			translate_atom(disp, sel_property),
			ourwin);
            XConvertSelection(disp, sel_atom, strtype,
                              sel_property, ourwin, CurrentTime);
            return True;
        }
    } else {
        /*
         * We are writing to the selection, so we establish
         * ourselves as selection owner. Also place the data in
         * CUT_BUFFER0, if it isn't of an exotic type (cut buffers
         * can only take ordinary string data, it turns out).
         */
        XSetSelectionOwner (disp, sel_atom, ourwin, CurrentTime);
        if (XGetSelectionOwner (disp, sel_atom) != ourwin)
            error ("unable to obtain primary X selection");
        compound_text_atom = XInternAtom(disp, "COMPOUND_TEXT", False);
	if (strtype == XA_STRING) {
	    /*
	     * ICCCM-required cut buffer initialisation.
	     */
	    XChangeProperty(disp, root, XA_CUT_BUFFER0,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER1,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER2,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER3,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER4,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER5,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER6,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    XChangeProperty(disp, root, XA_CUT_BUFFER7,
			    XA_STRING, 8, PropModeAppend, "", 0);
	    /*
	     * Rotate the cut buffers and add our text in CUT_BUFFER0.
	     */
	    XRotateBuffers(disp, 1);
	    XStoreBytes(disp, seltext, sellen);
	}
        return True;
    }
}

Atom convert_sel_inner(Window requestor, Atom target, Atom property) {
    if (target == strtype) {
	XChangeProperty (disp, requestor, property, strtype,
			 8, PropModeReplace, seltext, sellen);
	return property;
    } else if (target == compound_text_atom && convert_to_ctext) {
	XTextProperty tp;
	XmbTextListToTextProperty (disp, &seltext, 1,
				   XCompoundTextStyle, &tp);
	XChangeProperty (disp, requestor, property, target,
			 tp.format, PropModeReplace,
			 tp.value, tp.nitems);
	return property;
    } else if (target == targets_atom) {
	Atom targets[16];
	int len = 0;
	targets[len++] = timestamp_atom;
	targets[len++] = targets_atom;
	targets[len++] = multiple_atom;
	targets[len++] = strtype;
	if (strtype != compound_text_atom && convert_to_ctext)
	    targets[len++] = compound_text_atom;
	XChangeProperty (disp, requestor, property,
			 atom_atom, 32, PropModeReplace,
			 (unsigned char *)targets, len);
	return property;
    } else if (target == timestamp_atom) {
	Time rettime = CurrentTime;
	XChangeProperty (disp, requestor, property,
			 integer_atom, 32, PropModeReplace,
			 (unsigned char *)&rettime, 1);
	return property;
    } else {
	return None;
    }
}

Atom convert_sel_outer(Window requestor, Atom target, Atom property) {
    if (target == multiple_atom) {
	/*
	 * Support for the MULTIPLE selection type, since it's
	 * specified as required in the ICCCM. Completely
	 * untested, though, because I have no idea of what X
	 * application might implement it for me to test against.
	 */

	int size = SELDELTA;
	Atom actual_type;
	int actual_format, i;
	long nitems, bytes_after, nread;
	unsigned char *data;
	Atom *adata;

        if (property == (Atom)None)
            return None;               /* ICCCM says this isn't allowed */

	/*
	 * Fetch the requestor's window property giving a list of
	 * selection requests.
	 */
	while (XGetWindowProperty(disp, requestor, property, 0, size,
				  False, AnyPropertyType, &actual_type,
				  &actual_format, &nitems, &bytes_after,
				  (unsigned char **) &data) == Success &&
	       nitems * (actual_format / 8) == size) {
	    XFree(data);
	    size *= 3 / 2;
	}

	if (actual_type != atom_pair_atom || actual_format != 32) {
	    XFree(data);
	    return None;
	}

	adata = (Atom *)data;

	for (i = 0; i+1 < nitems; i += 2) {
            if (adata[i+1] != (Atom)None)   /* ICCCM says this isn't allowed */
                adata[i+1] = convert_sel_inner(requestor, adata[i],
                                               adata[i+1]);
	}

	XChangeProperty (disp, requestor, property,
			 atom_pair_atom, 32, PropModeReplace,
			 data, nitems);

	XFree(data);

	return property;
    } else {
        if (property == (Atom)None)
            property = target;      /* ICCCM says this is a sensible default */
	return convert_sel_inner(requestor, target, property);
    }
}

void run_X(void) {
    XEvent ev, e2;

    while (1) {
	XNextEvent (disp, &ev);
        if (reading) {
            switch (ev.type) {
              case SelectionNotify:
		if (verbose)
		    fprintf(stderr, "got SelectionNotify: requestor=%08lx "
			    "selection=%s target=%s property=%s\n",
			    ev.xselection.requestor,
			    translate_atom(disp, ev.xselection.selection),
			    translate_atom(disp, ev.xselection.target),
			    translate_atom(disp, ev.xselection.property));

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
                e2.xselection.property =
		    convert_sel_outer(ev.xselectionrequest.requestor,
				      ev.xselectionrequest.target,
				      ev.xselectionrequest.property);
                XSendEvent (disp, ev.xselectionrequest.requestor,
			    False, 0, &e2);
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
	if (verbose)
	    fprintf(stderr, "got %ld items of %d-byte data, type=%s;"
		    " %ld to go\n", nitems, actual_format,
		    translate_atom(disp, actual_type), bytes_after);

	if (nitems > 0) {
	    /*
	     * We expect all returned chunks of data to be
	     * multiples of 4 bytes (because we can only request
	     * the subsequent starting offset in 4-byte
	     * increments). Of course you can store an odd number
	     * of bytes in a selection, so this can't be the case
	     * every time XGetWindowProperty returns; but it
	     * should be the case every time it returns _and there
	     * is more data to come_.
	     *
	     * Hence, whenever XGetWindowProperty returns, we
	     * verify that the size of the data returned _last_
	     * time was divisible by 4.
	     */
	    if ((nread & 3) != 0) {
		error("unexpected data size: %d read (not a multiple"
		      " of 4), but more to come", nread);
	    }

	    if (expected_type != (Atom)None && actual_type != expected_type) {
		const char *expout = translate_atom(disp, expected_type);
		const char *gotout = translate_atom(disp, actual_type);
		error("unexpected data type: expected %s, got %s",
		      expout, gotout);
	    }
	    if (expected_format && expected_format != actual_format) {
		error("unexpected data format: expected %d-bit, got %d-bit",
		      expected_format, actual_format);
	    }
	}

        if (nitems > 0) {
	    if (mode == TARGETS) {
		assert(actual_format == 32);
		int i;
		for (i = 0; i < nitems; i++) {
		    Atom x = ((Atom *)data)[i];
		    printf("%s\n", translate_atom(disp, x));
		}
	    } else if (mode == TIMESTAMP) {
		assert(actual_format == 32);
		Time x = ((Time *)data)[0];
		printf("%lu\n", (unsigned long)x);
	    } else {
		fwrite(data, actual_format / 8, nitems, stdout);
		nread += nitems * actual_format / 8;
	    }
        }
        XFree(data);
        if (nitems == 0)
            break;
	if (bytes_after == 0) {
	    /*
	     * We've come to the end of the property we're reading.
	     * If we have the Delete flag set, we may be receiving
	     * the selection data in multiple chunks, in which case
	     * we should reset nread to zero so that when the next
	     * chunk arrives we resume reading from the start of the
	     * replaced property. Otherwise, this is our cue to
	     * exit.
	     */
	    if (Delete)
		nread = 0;
	    else
		break;
	}
    }
}
