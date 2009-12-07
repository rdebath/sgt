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
#include <fcntl.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "misc.h"
#include "xplib.h"

char *pname;			       /* program name */
Display *disp = NULL;

void error(char *fmt, ...)
{
    va_list ap;
    char errbuf[2048];

    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s: %s\n", pname, errbuf);
}

void platform_fatal_error(const char *s)
{
    fprintf(stderr, "%s: %s\n", pname, s);
    exit(1);
}

void minimise_window(void)
{
    system("sawfish-client -c '"
	   "(let ((w1 (query-pointer-window))"
		 "(w2 (input-focus)))"
	      "(cond ((not (null w1)) (iconify-window w1))"
	            "((not (null w2)) (iconify-window w2))"
	      ")"
	   ")"
	   "' >& /dev/null");
}

void window_to_back(void)
{
    system("sawfish-client -c '"
	   "(let ((w1 (query-pointer-window))"
		 "(w2 (input-focus)))"
	      "(cond ((not (null w1)) (lower-window w1))"
	            "((not (null w2)) (lower-window w2))"
	      ")"
	   ")"
	   "' >& /dev/null");
}

char *read_clipboard(void)
{
    char *buf = NULL;
    int buflen = 0, bufsize = 0;
    FILE *fp;
    int ret;

    /*
     * Disgustinger. Should I fold pieces of xcopy into this?
     */
    fp = popen("xcopy -r", "r");
    if (!fp) {
	error("popen: %s", strerror(errno));
	return NULL;
    }
    while (1) {
	if (bufsize < buflen + 2048) {
	    bufsize = buflen * 3 / 2 + 2048;
	    buf = sresize(buf, bufsize, char);
	}
	ret = fread(buf + buflen, 1, bufsize-1 - buflen, fp);
	if (ret < 0) {
	    error("fread: %s", strerror(errno));
	    pclose(fp);
	    sfree(buf);
	    return NULL;
	} else if (ret == 0) {
	    buf[buflen] = '\0';
	    ret = pclose(fp);
	    if (ret < 0) {
		error("wait4: %s", strerror(errno));
		sfree(buf);
		buf = NULL;
	    } else if (ret > 0) {
		error("xcopy -r returned error code: %d", ret);
		sfree(buf);
		buf = NULL;
	    }
	    return buf;
	} else {
	    buflen += ret;
	}
    }
}

void write_clipboard(const char *buf)
{
    FILE *fp;
    int pos, len, ret;

    /*
     * Still disgusting.
     */
    fp = popen("xcopy", "w");
    if (!fp) {
	error("popen: %s", strerror(errno));
	return;
    }
    len = strlen(buf);
    pos = 0;
    while (pos < len) {
	ret = fwrite(buf + pos, 1, len - pos, fp);
	if (ret < 0) {
	    error("fwrite: %s", strerror(errno));
	    pclose(fp);
	    return;
	} else {
	    pos += ret;
	}
    }

    ret = pclose(fp);
    if (ret < 0) {
	error("wait4: %s", strerror(errno));
    } else if (ret > 0) {
	error("xcopy returned error code: %d", ret);
    }
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

void debug_message(const char *msg)
{
    printf("ick-keys debug: %s\n", msg);
}

struct hotkey {
    int exists;
    int keycode;
    unsigned modifiers;
} *hotkeys = NULL;
int hotkeysize = 0;

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

    /* open the X display */
    disp = XOpenDisplay(display);
    if (!disp)
	error("unable to open display");

    /* grab all the relevant hot keys */
    configure();

    while (1) {
	XEvent ev;
	int i;

	XNextEvent (disp, &ev);
	switch (ev.type) {
	  case KeyPress:
	    for (i = 0; i < hotkeysize; i++)
		if (hotkeys[i].exists &&
		    ev.xkey.keycode == hotkeys[i].keycode &&
		    (ev.xkey.state & ~Mod2Mask) == hotkeys[i].modifiers) {
		    run_hotkey(i);
		    break;
		}
	    break;
	}
    }

    XCloseDisplay (disp);

    return 0;
}
