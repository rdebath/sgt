/* xdispchar  dump X keyboard events */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void error(char *fmt, ...) ;
void init_X(void) ;
void run_X(void) ;
void done_X(void) ;
void full_redraw(void) ;

char *pname ;			       /* program name */

/* set from command-line parameters */
char *display = NULL ;
char *geometry = NULL ;
char *filename = NULL ;

int main(int ac, char **av) {
    pname = *av ;

    /* parse the command line arguments */
    while (--ac) {
	char *p = *++av ;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error ("option `%s' expects a parameter", p) ;
	    display = *++av, --ac ;
	} else if (!strcmp(p, "-geometry") || !strcmp(p, "-geom")) {
	    if (!av[1])
		error ("option `%s' expects a parameter", p) ;
	    geometry = *++av, --ac ;
	} else if (*p=='-') {
	    error ("unrecognised option `%s'", p) ;
	} else {
	    filename = p ;
	}
    }

    init_X() ;
    run_X() ;
    done_X() ;

    return 0;
}

/* handle errors */

void error (char *fmt, ...) {
    va_list ap ;
    char errbuf[200] ;

    done_X() ;
    va_start (ap, fmt) ;
    vsprintf (errbuf, fmt, ap) ;
    va_end (ap) ;
    fprintf (stderr, "%s: %s\n", pname, errbuf) ;
    exit (1) ;
}

/* begin the X interface */

char *lcasename = "xdispchar" ;
char *ucasename = "XDispChar" ;

Display *disp = NULL ;
Window ourwin = None ;
Pixmap ourpm = None ;
GC ourgc = None ;
int screen, wwidth, wheight ;

void init_X(void) {
    Window root ;
    int x = 0, y = 0, width = 512, height = 128 ;
    int i, got = 0 ;
    XWMHints wm_hints ;
    XSizeHints size_hints ;
    XClassHint class_hints ;
    XTextProperty textprop ;
    XGCValues gcv ; 

    /* open the X display */
    disp = XOpenDisplay (display) ;
    if (!disp)
	error ("unable to open display") ;

    /* get the screen and root-window */
    screen = DefaultScreen (disp) ;
    root = RootWindow (disp, screen) ;

    /* parse the geometry specification, if one was given */
    if (geometry)
	got = XParseGeometry (geometry, &x, &y, &width, &height) ;
    if (got & XNegative)
	x += DisplayWidth(disp, screen)-width ;
    if (got & YNegative)
	y += DisplayHeight(disp, screen)-height ;

    /* actually create the window */
    ourwin = XCreateSimpleWindow (disp, root, x, y, width, height,0, 
				  BlackPixel(disp, screen),
				  WhitePixel(disp, screen)) ;

    /* send three billion hints to the window manager
     * first off, simple starter hints */
    wm_hints.input = True ;
    wm_hints.initial_state = NormalState ;
    wm_hints.flags = InputHint | StateHint ;
    XSetWMHints (disp, ourwin, &wm_hints) ;

    /* window position, size, min size etc */
    size_hints.x = x ; size_hints.width = wwidth = width ;
    size_hints.y = y ; size_hints.height = wheight = height ;
    size_hints.min_width = size_hints.min_height = 64 ;
    size_hints.max_width = DisplayWidth (disp, screen) ;
    size_hints.max_height = DisplayHeight (disp, screen) ;
    size_hints.width_inc = size_hints.height_inc = 1 ;
    size_hints.flags = USSize | PMinSize | PMaxSize | PResizeInc ;
    if (got & (XValue | YValue))
	size_hints.flags |= USPosition ;
    else
	size_hints.flags |= PPosition ;
    XSetWMNormalHints (disp, ourwin, &size_hints) ;

    /* resource class name */
    class_hints.res_name = lcasename ;
    class_hints.res_class = ucasename;
    XSetClassHint (disp, ourwin, &class_hints) ;

    /* window and icon names */
    XStringListToTextProperty (&ucasename, 1, &textprop) ;
    XSetWMName (disp, ourwin, &textprop) ;
    XStringListToTextProperty (&lcasename, 1, &textprop) ;
    XSetWMIconName (disp, ourwin, &textprop) ;

    /* specify what events we wish to receive */
    XSelectInput (disp, ourwin, StructureNotifyMask |
		  SubstructureRedirectMask | ButtonPressMask |
		  ExposureMask | KeyPressMask) ;

    /* get a graphics context */
    ourgc = XCreateGC (disp, ourwin, 0, &gcv) ;

    /* actually display the window at last */
    XMapWindow (disp, ourwin) ;
    XFlush (disp) ;
}

void run_X(void) {
    XEvent ev ;

    do {
	XNextEvent (disp, &ev) ;
	switch (ev.type) {
	  case KeyPress:
	    printf("state=%08x key=%08x\n", ev.xkey.state, ev.xkey.keycode);
	    break;
	}
    } while (ev.type != ButtonPress) ;
}

void done_X(void) {
    int i ;

    if (ourgc != None)
	XFreeGC (disp, ourgc) ;
    if (ourpm != None)
	XFreePixmap (disp, ourpm) ;
    if (ourwin != None)
	XDestroyWindow (disp, ourwin) ;
    if (disp)
	XCloseDisplay (disp) ;
}
