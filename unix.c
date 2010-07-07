/*
 * X11 front end to ick-keys.
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#include <unistd.h>
#include <pwd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "misc.h"
#include "xplib.h"

char *pname;			       /* program name */

Display *disp = NULL;
int screen;
Window root = None, ourwin = None;
Atom compound_text_atom, targets_atom, timestamp_atom;
Atom atom_atom, atom_pair_atom, multiple_atom, integer_atom;
Atom sel_property;

Time hktime = CurrentTime;

#define SELDELTA 16384
char *seldata = NULL;
int seldatalen;
Time seltime = CurrentTime;
Window selrequestor;
Atom selproperty;

void error(char *fmt, ...)
{
    va_list ap;
    char errbuf[2048];

    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s: %s\n", pname, errbuf);
}

struct hotkey {
    int exists;
    int keycode;
    unsigned modifiers;
} *hotkeys = NULL;
int hotkeysize = 0;

struct hotkey_event {
    struct hotkey_event *next;
    int id;
    Time time;
} *hkhead = NULL, *hktail = NULL;

#define XEV_HOTKEY 1
#define XEV_PASTE 2

Atom convert_sel_inner(Window requestor, Atom target, Atom property)
{
    if (target == XA_STRING) {
	XChangeProperty (disp, requestor, property, XA_STRING,
			 8, PropModeReplace,
			 (unsigned char *)seldata, seldatalen);
	return property;
    } else if (target == compound_text_atom) {
	XTextProperty tp;
	XmbTextListToTextProperty(disp, &seldata, 1,
				  XCompoundTextStyle, &tp);
	XChangeProperty(disp, requestor, property, target,
			tp.format, PropModeReplace,
			tp.value, tp.nitems);
	return property;
    } else if (target == targets_atom) {
	Atom targets[16];
	int len = 0;
	targets[len++] = timestamp_atom;
	targets[len++] = targets_atom;
	targets[len++] = multiple_atom;
	targets[len++] = XA_STRING;
	targets[len++] = compound_text_atom;
	XChangeProperty (disp, requestor, property,
			 atom_atom, 32, PropModeReplace,
			 (unsigned char *)targets, len);
	return property;
    } else if (target == timestamp_atom) {
	Time rettime = seltime;
	XChangeProperty (disp, requestor, property,
			 integer_atom, 32, PropModeReplace,
			 (unsigned char *)&rettime, 1);
	return property;
    } else {
	return None;
    }
}

Atom convert_sel_outer(Window requestor, Atom target, Atom property)
{
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
	unsigned long nitems, bytes_after;
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
			 (unsigned char *)data, nitems);

	XFree(data);

	return property;
    } else {
        if (property == (Atom)None)
            property = target;      /* ICCCM says this is a sensible default */
	return convert_sel_inner(requestor, target, property);
    }
}

char *do_paste(Window window, Atom property, int Delete)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after, nread;
    unsigned char *data;
    char *ret = NULL;
    int retlen = 0;

    nread = 0;
    while (XGetWindowProperty(disp, window, property, nread / 4, SELDELTA,
                              Delete, AnyPropertyType, &actual_type,
                              &actual_format, &nitems, &bytes_after,
                              (unsigned char **) &data) == Success) {
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

	    if (actual_format != 8) {
		error("unexpected data format: expected 8-bit, got %d-bit",
		      actual_format);
	    }
	}

        if (nitems > 0) {
	    int oldlen = retlen;
	    retlen += nitems * actual_format / 8;
	    ret = sresize(ret, retlen+1, char);
	    memcpy(ret + oldlen, data, retlen - oldlen);
	    nread += nitems * actual_format / 8;
        }
        XFree(data);
        if (nitems == 0)
            break;
    }

    if (ret)
	ret[retlen] = '\0';
    return ret;
}

int xeventloop(int retflags)
{
    XEvent ev, e2;
    int i;

    while (1) {
	if ((retflags & XEV_HOTKEY) && hkhead)
	    return XEV_HOTKEY;
	XNextEvent (disp, &ev);
	switch (ev.type) {
	  case KeyPress:
	    for (i = 0; i < hotkeysize; i++)
		if (hotkeys[i].exists &&
		    ev.xkey.keycode == hotkeys[i].keycode &&
		    (ev.xkey.state & ~Mod2Mask) == hotkeys[i].modifiers) {
		    struct hotkey_event *hk = snew(struct hotkey_event);
		    hk->id = i;
		    hk->time = ev.xkey.time;
		    hk->next = NULL;
		    if (hktail)
			hktail->next = hk;
		    else
			hkhead = hk;
		    hktail = hk;
		    break;
		}
            break;
	  case SelectionClear:
	    seldata = NULL;
	    break;
	  case SelectionRequest:
	    if (seldata) {
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
	    break;
	  case SelectionNotify:
	    if (retflags & XEV_PASTE) {
		selrequestor = ev.xselection.requestor;
		selproperty = ev.xselection.property;
		return XEV_PASTE;
	    }
	    break;
	}
    }
}

void platform_fatal_error(const char *s)
{
    fprintf(stderr, "%s: %s\n", pname, s);
    exit(1);
}

char *get_sawfish_socket(char *display)
{
    char protocol[1024], host[1024];
    int dispno, screen;
    char *user;
    uid_t uid;
    struct passwd *p;

    user = getlogin();
    uid = getuid();
    setpwent();
    if (user && (p = getpwnam(user)) != NULL && p->pw_uid == uid)
        user = dupstr(user);
    else if ((p = getpwuid(uid)) != NULL)
        user = dupstr(p->pw_name);
    else
        user = dupfmt("%d", uid);
    endpwent();

    if ((protocol[0] = host[0] = '\0', dispno = screen = 0, display == NULL) ||
        (sscanf(display, "%1000[^/:]/%1000[^:]:%i.%i", protocol, host, &dispno, &screen)) == 4 ||
        (protocol[0] = '\0', sscanf(display, "%1000[^:]:%i.%i", host, &dispno, &screen)) == 3 ||
        (protocol[0] = host[0] = '\0', sscanf(display, ":%i.%i", &dispno, &screen)) == 2 ||
        (screen = 0, sscanf(display, "%1000[^/:]/%1000[^:]:%i", protocol, host, &dispno)) == 3 ||
        (protocol[0] = '\0', screen = 0, sscanf(display, "%1000[^:]:%i", host, &dispno)) == 2 ||
        (protocol[0] = host[0] = '\0', screen = 0, sscanf(display, ":%i", &dispno)) == 1) {
        if (protocol[0])
            strcat(protocol, "/");
        if (!host[0])
            gethostname(host, 1000);
        if (!strchr(host, '.')) {
            struct hostent *h = gethostbyname(host);
            if (h) {
                char *newhost;
                int i;
                for (i = -1; (newhost = (i<0?h->h_name:h->h_aliases[i])) != NULL; i++)
                    if (strchr(newhost, '.')) {
                        strncpy(host, newhost, 1000);
                        break;
                    }
            }
        }
        return dupfmt("/tmp/.sawfish-%s/%s%s:%d.%d", user, protocol, host, dispno, screen);
    } else
        return dupstr(display);        /* random fallback */
}

char *sawfish_socket;

void sawfish_client(char *string)
{
    struct sockaddr_un addr;
    int i, j, fd;
    char *data;
    char *realcmd = dupfmt("(call-command (quote %s))", string);
    int len = strlen(realcmd);
    unsigned long lenbuf;

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sawfish_socket, lenof(addr.sun_path)-1);
    addr.sun_path[lenof(addr.sun_path)-1] = '\0';

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        error("socket: %s", strerror(errno));
        sfree(realcmd);
        return;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error("connect: %s", strerror(errno));
        close(fd);
        sfree(realcmd);
        return;
    }
    data = malloc(1 + sizeof(unsigned long) + len);
    data[0] = 0;
    lenbuf = len;
    memcpy(data+1, &lenbuf, sizeof(lenbuf));
    memcpy(data+1+sizeof(unsigned long), realcmd, len);
    sfree(realcmd);
    len += 1+sizeof(unsigned long);
    for (i = 0; i < len ;) {
        j = write(fd, data + i, len - i);
        if (j < 0) {
            error("write: %s", strerror(errno));
            close(fd);
            sfree(data);
            return;
        }
        i += j;
    }
    len = sizeof(unsigned long);
    for (i = 0; i < len ;) {
        j = read(fd, data + i, len - i);
        if (j < 0) {
            error("read: %s", strerror(errno));
            close(fd);
            sfree(data);
            return;
        }
        i += j;
    }
    memcpy(&lenbuf, data, sizeof(lenbuf));
    len = lenbuf + sizeof(lenbuf);
    for (i = sizeof(lenbuf); i < len ;) {
        j = read(fd, data + i, len - i);
        if (j < 0) {
            error("read: %s", strerror(errno));
            close(fd);
            sfree(data);
            return;
        }
        i += j;
    }
    close(fd);
    sfree(data);
}

void minimise_window(void)
{
    sawfish_client("(let ((w1 (query-pointer-window))"
                        " (w2 (input-focus)))"
                     " (cond ((not (null w1)) (iconify-window w1))"
                           " ((not (null w2)) (iconify-window w2))"
                      ")"
                   ")");
}

void window_to_back(void)
{
    sawfish_client("(let ((w1 (query-pointer-window))"
                        " (w2 (input-focus)))"
                     " (cond ((not (null w1)) (lower-window w1))"
                           " ((not (null w2)) (lower-window w2))"
                      ")"
                   ")");
}

char *read_clipboard(void)
{
    char *ret = NULL;

    if (XGetSelectionOwner(disp, XA_PRIMARY) == None) {
	/* No primary selection, so use the cut buffer. */
	ret = do_paste(root, XA_CUT_BUFFER0, False);
    } else {
	XConvertSelection(disp, XA_PRIMARY, XA_STRING,
			  sel_property, ourwin, CurrentTime);
	xeventloop(XEV_PASTE);
	if (selrequestor)
	    ret = do_paste(selrequestor, selproperty, True);
    }

    return ret;
}

void write_clipboard(const char *buf)
{
    seldatalen = strlen(buf);
    seldata = dupstr(buf);
    seltime = hktime;

    XSetSelectionOwner(disp, XA_PRIMARY, ourwin, seltime);
    if (XGetSelectionOwner (disp, XA_PRIMARY) != ourwin)
	error ("unable to obtain primary X selection");
    /*
     * ICCCM-required cut buffer initialisation.
     */
    XChangeProperty(disp, root, XA_CUT_BUFFER0,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER1,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER2,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER3,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER4,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER5,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER6,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    XChangeProperty(disp, root, XA_CUT_BUFFER7,
		    XA_STRING, 8, PropModeAppend, (unsigned char *)"", 0);
    /*
     * Rotate the cut buffers and add our text in CUT_BUFFER0.
     */
    XRotateBuffers(disp, 1);
    XStoreBytes(disp, seldata, seldatalen);
}

void open_url(const char *url)
{
    char *buf, *p;
    const char *q;

    buf = snewn(128 + 4 * strlen(url), char);
    p = buf;
    p += sprintf(p, "$HOME/adm/urllaunch.pl '");
    for (q = url; *q; q++) {
	if (*q != '\'') {
	    *p++ = *q;
	} else {
	    *p++ = '\'';
	    *p++ = '\\';
	    *p++ = '\'';
	    *p++ = '\'';
	}
    }
    *p++ = '\'';
    *p = '\0';
    system(buf);
    sfree(buf);
}

void spawn_process(const char *cmd)
{
    pid_t pid = fork();
    if (pid == 0) {
	/*
	 * Double-fork so that the real subprocess ends up with a
	 * parent other than us.
	 */
	pid_t pid2 = fork();
	if (pid2 == 0) {
	    execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);
	    _exit(127);
	} else {
	    _exit(0);
	}
    } else if (pid < 0) {
	fprintf(stderr, "ick-keys: fork: %s\n", strerror(errno));
    } else {
	/*
	 * Reap the immediate child.
	 */
	int status;
	waitpid(pid, &status, 0);
    }
}

void debug_message(const char *msg)
{
    printf("ick-keys debug: %s\n", msg);
}

void unregister_all_hotkeys(void)
{
    int i;
    for (i = 0; i < hotkeysize; i++)
	if (hotkeys[i].exists) {
	    XUngrabKey(disp, hotkeys[i].keycode, hotkeys[i].modifiers,
		       XDefaultRootWindow(disp));
	    hotkeys[i].exists = 0;
	}
}

int register_hotkey(int index, int mod, const char *key,
		    const char *keyorig)
{
    int keycode;
    unsigned modifiers;

    keycode = XKeysymToKeycode(disp, key[0]);
    modifiers = 0;
    if (mod & LEFT_WINDOWS)
	modifiers |= Mod4Mask;
    if (mod & CTRL)
	modifiers |= ControlMask;
    if (mod & ALT)
	modifiers |= Mod1Mask;
    XGrabKey(disp, keycode, modifiers, XDefaultRootWindow(disp),
             False, GrabModeAsync, GrabModeAsync);
    /* And the same again with Num Lock on */
    XGrabKey(disp, keycode, modifiers | Mod2Mask, XDefaultRootWindow(disp),
             False, GrabModeAsync, GrabModeAsync);

    if (index >= hotkeysize) {
	int oldsize = hotkeysize;
	hotkeysize = index * 3 / 2 + 16;
	hotkeys = sresize(hotkeys, hotkeysize, struct hotkey);
	while (oldsize < hotkeysize)
	    hotkeys[oldsize++].exists = 0;
    }

    hotkeys[index].exists = 1;
    hotkeys[index].keycode = keycode;
    hotkeys[index].modifiers = modifiers;

    return 1;
}

int main(int ac, char **av)
{
    char *display = NULL;

    pname = *av;

    /* parse the command line arguments */
    while (--ac) {
	char *p = *++av;

	if (!strcmp(p, "-display") || !strcmp(p, "-disp")) {
	    if (!av[1])
		error ("option '%s' expects a parameter", p);
	    display = *++av, --ac;
	} else if (*p=='-') {
	    error("unrecognised option '%s'", p);
	} else {
	    add_config_filename(p);
	}
    }

    /* open the X display and grab various bits and bobs */
    disp = XOpenDisplay(display);
    if (!disp)
	error("unable to open display");
    screen = DefaultScreen(disp);
    root = RootWindow(disp, screen);
    compound_text_atom = XInternAtom(disp, "COMPOUND_TEXT", False);
    targets_atom = XInternAtom(disp, "TARGETS", False);
    timestamp_atom = XInternAtom(disp, "TIMESTAMP", False);
    atom_atom = XInternAtom(disp, "ATOM", False);
    atom_pair_atom = XInternAtom(disp, "ATOM_PAIR", False);
    multiple_atom = XInternAtom(disp, "MULTIPLE", False);
    integer_atom = XInternAtom(disp, "INTEGER", False);
    sel_property = XInternAtom(disp, "VT_SELECTION", False);

    /* create a trivial window for selection ownership */
    ourwin = XCreateSimpleWindow(disp, root, 0, 0, 10, 10,0,
				 BlackPixel(disp, screen),
				 WhitePixel(disp, screen));
    {
	XClassHint class_hints;
	class_hints.res_name = class_hints.res_class = "ick-keys";
	XSetClassHint (disp, ourwin, &class_hints);
    }

    /* figure out the sawfish socket name, for minimising/lowering windows */
    sawfish_socket = get_sawfish_socket(display);

    /* grab all the relevant hot keys */
    configure();

    while (1) {
	xeventloop(XEV_HOTKEY);
	while (hkhead) {
	    struct hotkey_event *hk = hkhead;
	    hkhead = hk->next;
	    hktime = hk->time;
	    run_hotkey(hk->id);
	    hktime = CurrentTime;
	    sfree(hk);
	}
	hktail = NULL;
    }

    XCloseDisplay (disp);

    return 0;
}
