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

struct hotkey {
    int exists;
    int keycode;
    unsigned modifiers;
} *hotkeys = NULL;
int hotkeysize = 0;

struct hotkey_event {
    struct hotkey_event *next;
    int id;
} *hkhead = NULL, *hktail = NULL;

#define XEV_HOTKEY 1

int xeventloop(int retflags)
{
    XEvent ev;
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
		    hk->next = NULL;
		    if (hktail)
			hktail->next = hk;
		    else
			hkhead = hk;
		    hktail = hk;
		    break;
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

    /* open the X display */
    disp = XOpenDisplay(display);
    if (!disp)
	error("unable to open display");

    sawfish_socket = get_sawfish_socket(display);

    /* grab all the relevant hot keys */
    configure();

    while (1) {
	xeventloop(XEV_HOTKEY);
	while (hkhead) {
	    struct hotkey_event *hk = hkhead;
	    hkhead = hk->next;
	    run_hotkey(hk->id);
	    sfree(hk);
	}
	hktail = NULL;
    }

    XCloseDisplay (disp);

    return 0;
}
