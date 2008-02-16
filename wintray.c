/*
 * Single-user Windows front end for ick-proxy, sitting in the
 * System Tray during a specific user's login session.
 */

#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tree234.h"
#include "proxy.h"
#include "misc.h"

#ifdef DEBUG
static FILE *debug_fp = NULL;
static HANDLE debug_hdl = INVALID_HANDLE_VALUE;
static int debug_got_console = 0;

void dputs(char *buf)
{
    DWORD dw;

    if (!debug_got_console) {
	if (AllocConsole()) {
	    debug_got_console = 1;
	    debug_hdl = GetStdHandle(STD_OUTPUT_HANDLE);
	}
    }
    if (!debug_fp) {
	debug_fp = fopen("debug.log", "w");
    }

    if (debug_hdl != INVALID_HANDLE_VALUE) {
	WriteFile(debug_hdl, buf, strlen(buf), &dw, NULL);
    }
    fputs(buf, debug_fp);
    fflush(debug_fp);
}

void debug_fmt(char *fmt, ...)
{
    char *buf;
    va_list ap;

    va_start(ap, fmt);
    buf = dupfmt(fmt, ap);
    dputs(buf);
    sfree(buf);
    va_end(ap);
}

#define debug(x) (debug_fmt x)
#else
#define debug(x)
#endif

#define IDI_MAINICON 200

#define WM_XUSER     (WM_USER + 0x2000)
#define WM_NETEVENT  (WM_XUSER + 5)
#define WM_SYSTRAY   (WM_XUSER + 6)
#define WM_SYSTRAY2  (WM_XUSER + 7)

HINSTANCE ickproxy_instance;
HWND ickproxy_hwnd;
HWND aboutbox = NULL;
const char ickproxy_appname[] = "ick-proxy";

char *override_inpac = NULL;
char *override_script = NULL;
char *override_outpac = NULL;

static void error(char *s)
{
    MessageBox(ickproxy_hwnd, s, ickproxy_appname, MB_OK | MB_ICONERROR);
}

static void cleanup_sockets(void);

void platform_fatal_error(const char *s)
{
    MessageBox(ickproxy_hwnd, s, ickproxy_appname, MB_OK | MB_ICONERROR);
    cleanup_sockets();
    WSACleanup();
    exit(1);
}

static char *get_filename_for_user(char **err, const char *username,
				   const char *filename)
{
    char *appdata;

    /*
     * This is a single-user only front end, so we can always
     * simply look at the user's own APPDATA directory.
     */

    appdata = getenv("APPDATA");
    if (appdata == NULL) {
	*err = dupfmt("APPDATA environment variable not set");
	return NULL;
    }

    return dupfmt("%s\\%s", appdata, filename);
}

char *read_whole_file(char **err, char *filename)
{
    char *ret;
    int len, size;
    FILE *fp;

    fp = fopen(filename, "r");
    if (!fp) {
	*err = dupfmt("Unable to open %s", filename);
	return NULL;
    }

    size = 1024;
    ret = snewn(size, char);
    len = 0;

    while (1) {
	char readbuf[4096];
	int readret = fread(readbuf, 1, sizeof(readbuf), fp);

	if (readret < 0) {
	    sfree(ret);
	    *err = dupfmt("Unable to read %s", filename);
	    return NULL;
	} else if (readret == 0) {
	    fclose(fp);
	    return ret;
	} else {
	    if (size < len + readret + 1) {
		size = (len + readret + 1) * 3 / 2 + 1024;
		ret = sresize(ret, size, char);
	    }
	    memcpy(ret + len, readbuf, readret);
	    ret[len + readret] = '\0';
	    len += readret;
	}
    }
}

char *get_script_for_user(char **err, const char *username)
{
    char *fname, *ret;

    if (override_script)
	fname = dupstr(override_script);
    else
	fname = get_filename_for_user(err, username, "ick-proxy\\rewrite.ick");
    if (!fname)
	return NULL;		       /* err has already been filled in */

    ret = read_whole_file(err, fname);
    sfree(fname);
    return ret;
}

char *get_inpac_for_user(char **err, const char *username)
{
    char *fname, *ret;

    if (override_inpac)
	fname = dupstr(override_inpac);
    else
	fname = get_filename_for_user(err, username, "ick-proxy\\input.pac");
    if (!fname)
	return NULL;		       /* err has already been filled in */

    ret = read_whole_file(err, fname);
    sfree(fname);
    return ret;
}

char *name_outpac_for_user(char **err, const char *username)
{
    char *fname;

    if (override_outpac)
	fname = dupstr(override_outpac);
    else
	fname = get_filename_for_user(err, username, "ick-proxy\\output.pac");

    return fname;
}

enum { SOCK_LISTENER, SOCK_CONNECTION };

struct socket {
    SOCKET s;
    int type;
    char *wdata;
    int wdatalen, wdatapos;
    struct listenctx *lctx;
    struct connctx *cctx;
};

tree234 *sockets;

static int sockcmp(const void *av, const void *bv)
{
    unsigned long a = (unsigned long)(((const struct socket *)av)->s);
    unsigned long b = (unsigned long)(((const struct socket *)bv)->s);

    if (a < b)
	return -1;
    else if (a > b)
	return +1;
    else
	return 0;
}
static int sockfind(const void *av, const void *bv)
{
    unsigned long a = (unsigned long)(*(SOCKET *)av);
    unsigned long b = (unsigned long)(((const struct socket *)bv)->s);

    if (a < b)
	return -1;
    else if (a > b)
	return +1;
    else
	return 0;
}

struct socket *new_sockstruct(SOCKET s, int type)
{
    struct socket *ret = snew(struct socket);

    ret->s = s;
    ret->type = type;
    ret->wdata = NULL;
    ret->wdatalen = ret->wdatapos = 0;
    ret->lctx = NULL;
    ret->cctx = NULL;

    add234(sockets, ret);

    return ret;
}

int create_new_listener(char **err, int port, struct listenctx *ctx)
{
    SOCKADDR_IN addr;
    int addrlen;
    SOCKET s;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
	switch (WSAGetLastError()) {
	  case WSAENETDOWN:
	    *err = dupstr("Network is down"); return -1;
	  case WSAEAFNOSUPPORT:
	    *err = dupstr("TCP/IP support not present"); return -1;
	  default: *err = dupstr("socket(): unknown error"); return -1;
	}

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (port < 0)
	port = 0;
    addr.sin_port = htons((short)port);
    if (bind (s, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR)
	switch (WSAGetLastError()) {
	  case WSAENETDOWN: *err = dupstr("Network is down"); return -1;
	  default: *err = dupstr("bind(): unknown error"); return -1;
	}

    if (listen(s, 5) == SOCKET_ERROR)
	switch (WSAGetLastError()) {
	  case WSAENETDOWN: *err = dupstr("Network is down"); return -1;
	  default: *err = dupstr("listen(): unknown error"); return -1;
	}

    if (WSAAsyncSelect (s, ickproxy_hwnd,
			WM_NETEVENT, FD_ACCEPT) == SOCKET_ERROR)
	switch (WSAGetLastError()) {
	  case WSAENETDOWN: *err = dupstr("Network is down"); return -1;
	  default: *err = dupstr("WSAAsyncSelect(): unknown error"); return -1;
	}
    addrlen = sizeof(addr);
    if (getsockname(s, (SOCKADDR *)&addr, &addrlen))
	switch (WSAGetLastError()) {
	  case WSAENETDOWN: *err = dupstr("Network is down"); return -1;
	  default: *err = dupstr("WSAAsyncSelect(): unknown error"); return -1;
	}
    port = ntohs(addr.sin_port);

    {
	struct socket *ret = new_sockstruct(s, SOCK_LISTENER);
	ret->lctx = ctx;
	return port;
    }
}

static void cleanup_sockets(void)
{
    int i;
    struct socket *s;

    for (i = 0; (s = (struct socket *)delpos234(sockets, 0)) != NULL ;) {
        closesocket(s->s);
	sfree(s->wdata);
	sfree(s);
    }
}


#define IDM_CLOSE    0x0010
#define IDM_ABOUT    0x0020

static HMENU systray_menu;

static int tray_init(void)
{
    BOOL res;
    NOTIFYICONDATA tnid;
    HICON hicon;

#ifdef NIM_SETVERSION
    tnid.uVersion = 0;
    res = Shell_NotifyIcon(NIM_SETVERSION, &tnid);
#endif

    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = ickproxy_hwnd;
    tnid.uID = 1;                      /* unique within this systray use */
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hicon = LoadIcon (ickproxy_instance, MAKEINTRESOURCE(201));
    strcpy(tnid.szTip, "ick-proxy (URL rewriting server)");

    res = Shell_NotifyIcon(NIM_ADD, &tnid); 

    if (hicon) 
        DestroyIcon(hicon); 

    systray_menu = CreatePopupMenu();
    AppendMenu (systray_menu, MF_ENABLED, IDM_ABOUT, "About");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_CLOSE, "Close ick-proxy");

    return res; 
}

static void tray_shutdown(void)
{
    BOOL res; 
    NOTIFYICONDATA tnid; 
 
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = ickproxy_hwnd;
    tnid.uID = 1;

    res = Shell_NotifyIcon(NIM_DELETE, &tnid); 

    DestroyMenu(systray_menu);
}

/*
 * The About box.
 */
static int CALLBACK LicenceProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam);

void showversion(int line, char *buffer)
{
    sprintf(buffer, "%s version 2.0");
}

static int CALLBACK AboutProc(HWND hwnd, UINT msg,
			      WPARAM wParam, LPARAM lParam)
{
    char vbuf[160];
    switch (msg) {
      case WM_INITDIALOG:
	showversion(0, vbuf);
	SetDlgItemText(hwnd, 100, vbuf);
	showversion(1, vbuf);
	SetDlgItemText(hwnd, 101, vbuf);
	showversion(2, vbuf);
	SetDlgItemText(hwnd, 102, vbuf);
	return 1;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	    aboutbox = NULL;
	    DestroyWindow(hwnd);
	    return 0;
	  case 112:
	    EnableWindow(hwnd, 0);
	    DialogBox(ickproxy_instance, MAKEINTRESOURCE(301),
		      NULL, LicenceProc);
	    EnableWindow(hwnd, 1);
	    SetActiveWindow(hwnd);
	    return 0;
	}
	return 0;
      case WM_CLOSE:
	aboutbox = NULL;
	DestroyWindow(hwnd);
	return 0;
    }
    return 0;
}

/*
 * Dialog-box function for the Licence box.
 */
static int CALLBACK LicenceProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
      case WM_INITDIALOG:
	return 1;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	    EndDialog(hwnd, 1);
	    return 0;
	}
	return 0;
      case WM_CLOSE:
	EndDialog(hwnd, 1);
	return 0;
    }
    return 0;
}

static LRESULT CALLBACK WndProc (HWND hwnd, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    int ret;
    POINT cursorpos;                   /* cursor position */
    static int menuinprogress;

    switch (message) {
      case WM_DESTROY:
	PostQuitMessage (0);
	return 0;
      case WM_NETEVENT:
        {
            SOCKET s = (SOCKET) wParam;
            SOCKET newsock;
            SOCKADDR_IN addr;
            int addrlen;
	    struct socket *ss, *ss2;

	    /*
	     * Look the socket up in the tree.
	     */
	    ss = (struct socket *)find234(sockets, &s, sockfind);
	    if (!ss) {
		/*
		 * This might not be an error condition: the
		 * socket might be one we've recently got rid of
		 * and this event might be queued up from before
		 * we did so. So we just ignore it.
		 */
		return 0;
	    }

	    if (ss->type == SOCK_LISTENER &&
		WSAGETSELECTEVENT(lParam) == FD_ACCEPT) {
		/*
		 * Accept a new connection and prepare to handle
		 * it.
		 */
		addrlen = sizeof(addr);
		newsock = accept(s, (SOCKADDR *)&addr, &addrlen);
		if (newsock == SOCKET_ERROR)
		    switch (WSAGetLastError()) {
		      case WSAENETDOWN: error("Network is down"); return 0;
		      default: error("listen(): unknown error"); return 0;
		    }

		if (WSAAsyncSelect(newsock, ickproxy_hwnd,
				   WM_NETEVENT, FD_READ) == SOCKET_ERROR)
		    switch (WSAGetLastError()) {
		      case WSAENETDOWN: error("Network is down"); return 0;
		      default: error("WSAAsyncSelect(): unknown error"); return 0;
		    }

		ss2 = new_sockstruct(newsock, SOCK_CONNECTION);
		ss2->cctx = new_connection(ss->lctx);
	    } else if (ss->type == SOCK_CONNECTION &&
		       WSAGETSELECTEVENT(lParam) == FD_READ) {
		/*
		 * There's data to be read.
		 */
		char readbuf[4096];
		int ret;

		ret = recv(ss->s, readbuf, sizeof(readbuf), 0);
		if (ret <= 0) {
		    /*
		     * This shouldn't happen in a sensible
		     * HTTP connection, so we abandon the
		     * connection if it does.
		     */
		    closesocket(ss->s);
		    del234(sockets, ss);
		    sfree(ss->wdata);
		    free_connection(ss->cctx);
		    sfree(ss);
		} else {
		    if (!ss->wdata) {
			/*
			 * If we haven't got an HTTP response
			 * yet, keep feeding data to proxy.c
			 * in the hope of acquiring one.
			 */
			ss->wdata = got_data(ss->cctx, readbuf, ret);
			if (ss->wdata) {
			    ss->wdatalen = strlen(ss->wdata);
			    ss->wdatapos = 0;

			    if (WSAAsyncSelect(ss->s, ickproxy_hwnd,
					       WM_NETEVENT, FD_READ | FD_WRITE)
				== SOCKET_ERROR)
				switch (WSAGetLastError()) {
				  case WSAENETDOWN: error("Network is down"); return 0;
				  default: error("WSAAsyncSelect(): unknown error"); return 0;
				}
			} else {
			    /*
			     * Otherwise, just drop our read data
			     * on the floor.
			     */
			}
		    }
		}
	    } else if (ss->type == SOCK_CONNECTION &&
		       WSAGETSELECTEVENT(lParam) == FD_WRITE &&
		       ss->wdatapos < ss->wdatalen) {
		/*
		 * The socket is writable, and we have data to
		 * write. Write it.
		 */
		int ret = send(ss->s, ss->wdata + ss->wdatapos,
			       ss->wdatalen - ss->wdatapos, 0);
		if (ret <= 0) {
		    /*
		     * Shouldn't happen; abandon the connection.
		     */
		    closesocket(ss->s);
		    del234(sockets, ss);
		    sfree(ss->wdata);
		    free_connection(ss->cctx);
		    sfree(ss);
		    break;
		} else {
		    ss->wdatapos += ret;
		    if (ss->wdatapos == ss->wdatalen) {
			shutdown(ss->s, 1 /* SD_SEND */);
		    }
		}
	    }
        }
	return 0;
      case WM_SYSTRAY:
	if (lParam == WM_RBUTTONUP) {
	    GetCursorPos(&cursorpos);
	    PostMessage(hwnd, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
	    return 0;
	}
	break;
      case WM_SYSTRAY2:
        if (!menuinprogress) {
            menuinprogress = 1;
            SetForegroundWindow(hwnd);
            ret = TrackPopupMenu(systray_menu,
                                 TPM_RIGHTALIGN | TPM_BOTTOMALIGN |
                                 TPM_RIGHTBUTTON,
                                 wParam, lParam, 0, ickproxy_hwnd, NULL);
            menuinprogress = 0;
        }
	return 0;
      case WM_COMMAND:
	if ((wParam & ~0xF) == IDM_CLOSE) {
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	    return 0;
	}
	if ((wParam & ~0xF) == IDM_ABOUT) {
	    if (!aboutbox) {
		aboutbox = CreateDialog(ickproxy_instance,
					MAKEINTRESOURCE(300),
					NULL, AboutProc);
		ShowWindow(aboutbox, SW_SHOWNORMAL);
		/*
		 * Sometimes the window comes up minimised / hidden
		 * for no obvious reason. Prevent this.
		 */
		SetForegroundWindow(aboutbox);
		SetWindowPos(aboutbox, HWND_TOP, 0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	    }
	    return 0;
	}
	break;
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}

/*
 * Split a complete command line into argc/argv, attempting to do
 * it exactly the same way Windows itself would do it (so that
 * console utilities, which receive argc and argv from Windows,
 * will have their command lines processed in the same way as GUI
 * utilities which get a whole command line and must break it
 * themselves).
 * 
 * Does not modify the input command line.
 * 
 * The final parameter (argstart) is used to return a second array
 * of char * pointers, the same length as argv, each one pointing
 * at the start of the corresponding element of argv in the
 * original command line. So if you get half way through processing
 * your command line in argc/argv form and then decide you want to
 * treat the rest as a raw string, you can. If you don't want to,
 * `argstart' can be safely left NULL.
 * 
 * (This piece of code was hoicked directly out of the PuTTY
 * winutils.c, but the rest of the copyrights on the PuTTY code
 * base do not apply to this particular code because I know it was
 * written entirely by me.)
 */
void split_into_argv(char *cmdline, int *argc, char ***argv,
		     char ***argstart)
{
    char *p;
    char *outputline, *q;
    char **outputargv, **outputargstart;
    int outputargc;

    /*
     * At first glance the rules appeared to be:
     *
     *  - Single quotes are not special characters.
     *
     *  - Double quotes are removed, but within them spaces cease
     *    to be special.
     *
     *  - Backslashes are _only_ special when a sequence of them
     *    appear just before a double quote. In this situation,
     *    they are treated like C backslashes: so \" just gives a
     *    literal quote, \\" gives a literal backslash and then
     *    opens or closes a double-quoted segment, \\\" gives a
     *    literal backslash and then a literal quote, \\\\" gives
     *    two literal backslashes and then opens/closes a
     *    double-quoted segment, and so forth. Note that this
     *    behaviour is identical inside and outside double quotes.
     *
     *  - Two successive double quotes become one literal double
     *    quote, but only _inside_ a double-quoted segment.
     *    Outside, they just form an empty double-quoted segment
     *    (which may cause an empty argument word).
     *
     *  - That only leaves the interesting question of what happens
     *    when one or more backslashes precedes two or more double
     *    quotes, starting inside a double-quoted string. And the
     *    answer to that appears somewhat bizarre. Here I tabulate
     *    number of backslashes (across the top) against number of
     *    quotes (down the left), and indicate how many backslashes
     *    are output, how many quotes are output, and whether a
     *    quoted segment is open at the end of the sequence:
     * 
     *                      backslashes
     * 
     *               0         1      2      3      4
     * 
     *         0   0,0,y  |  1,0,y  2,0,y  3,0,y  4,0,y
     *            --------+-----------------------------
     *         1   0,0,n  |  0,1,y  1,0,n  1,1,y  2,0,n
     *    q    2   0,1,n  |  0,1,n  1,1,n  1,1,n  2,1,n
     *    u    3   0,1,y  |  0,2,n  1,1,y  1,2,n  2,1,y
     *    o    4   0,1,n  |  0,2,y  1,1,n  1,2,y  2,1,n
     *    t    5   0,2,n  |  0,2,n  1,2,n  1,2,n  2,2,n
     *    e    6   0,2,y  |  0,3,n  1,2,y  1,3,n  2,2,y
     *    s    7   0,2,n  |  0,3,y  1,2,n  1,3,y  2,2,n
     *         8   0,3,n  |  0,3,n  1,3,n  1,3,n  2,3,n
     *         9   0,3,y  |  0,4,n  1,3,y  1,4,n  2,3,y
     *        10   0,3,n  |  0,4,y  1,3,n  1,4,y  2,3,n
     *        11   0,4,n  |  0,4,n  1,4,n  1,4,n  2,4,n
     * 
     * 
     *      [Test fragment was of the form "a\\\"""b c" d.]
     * 
     * There is very weird mod-3 behaviour going on here in the
     * number of quotes, and it even applies when there aren't any
     * backslashes! How ghastly.
     * 
     * With a bit of thought, this extremely odd diagram suddenly
     * coalesced itself into a coherent, if still ghastly, model of
     * how things work:
     * 
     *  - As before, backslashes are only special when one or more
     *    of them appear contiguously before at least one double
     *    quote. In this situation the backslashes do exactly what
     *    you'd expect: each one quotes the next thing in front of
     *    it, so you end up with n/2 literal backslashes (if n is
     *    even) or (n-1)/2 literal backslashes and a literal quote
     *    (if n is odd). In the latter case the double quote
     *    character right after the backslashes is used up.
     * 
     *  - After that, any remaining double quotes are processed. A
     *    string of contiguous unescaped double quotes has a mod-3
     *    behaviour:
     * 
     *     * inside a quoted segment, a quote ends the segment.
     *     * _immediately_ after ending a quoted segment, a quote
     *       simply produces a literal quote.
     *     * otherwise, outside a quoted segment, a quote begins a
     *       quoted segment.
     * 
     *    So, for example, if we started inside a quoted segment
     *    then two contiguous quotes would close the segment and
     *    produce a literal quote; three would close the segment,
     *    produce a literal quote, and open a new segment. If we
     *    started outside a quoted segment, then two contiguous
     *    quotes would open and then close a segment, producing no
     *    output (but potentially creating a zero-length argument);
     *    but three quotes would open and close a segment and then
     *    produce a literal quote.
     */

    /*
     * First deal with the simplest of all special cases: if there
     * aren't any arguments, return 0,NULL,NULL.
     */
    while (*cmdline && isspace(*cmdline)) cmdline++;
    if (!*cmdline) {
	if (argc) *argc = 0;
	if (argv) *argv = NULL;
	if (argstart) *argstart = NULL;
	return;
    }

    /*
     * This will guaranteeably be big enough; we can realloc it
     * down later.
     */
    outputline = snewn(1+strlen(cmdline), char);
    outputargv = snewn(strlen(cmdline)+1 / 2, char *);
    outputargstart = snewn(strlen(cmdline)+1 / 2, char *);

    p = cmdline; q = outputline; outputargc = 0;

    while (*p) {
	int quote;

	/* Skip whitespace searching for start of argument. */
	while (*p && isspace(*p)) p++;
	if (!*p) break;

	/* We have an argument; start it. */
	outputargv[outputargc] = q;
	outputargstart[outputargc] = p;
	outputargc++;
	quote = 0;

	/* Copy data into the argument until it's finished. */
	while (*p) {
	    if (!quote && isspace(*p))
		break;		       /* argument is finished */

	    if (*p == '"' || *p == '\\') {
		/*
		 * We have a sequence of zero or more backslashes
		 * followed by a sequence of zero or more quotes.
		 * Count up how many of each, and then deal with
		 * them as appropriate.
		 */
		int i, slashes = 0, quotes = 0;
		while (*p == '\\') slashes++, p++;
		while (*p == '"') quotes++, p++;

		if (!quotes) {
		    /*
		     * Special case: if there are no quotes,
		     * slashes are not special at all, so just copy
		     * n slashes to the output string.
		     */
		    while (slashes--) *q++ = '\\';
		} else {
		    /* Slashes annihilate in pairs. */
		    while (slashes >= 2) slashes -= 2, *q++ = '\\';

		    /* One remaining slash takes out the first quote. */
		    if (slashes) quotes--, *q++ = '"';

		    if (quotes > 0) {
			/* Outside a quote segment, a quote starts one. */
			if (!quote) quotes--, quote = 1;

			/* Now we produce (n+1)/3 literal quotes... */
			for (i = 3; i <= quotes+1; i += 3) *q++ = '"';

			/* ... and end in a quote segment iff 3 divides n. */
			quote = (quotes % 3 == 0);
		    }
		}
	    } else {
		*q++ = *p++;
	    }
	}

	/* At the end of an argument, just append a trailing NUL. */
	*q++ = '\0';
    }

    outputargv = sresize(outputargv, outputargc, char *);
    outputargstart = sresize(outputargstart, outputargc, char *);

    if (argc) *argc = outputargc;
    if (argv) *argv = outputargv; else sfree(outputargv);
    if (argstart) *argstart = outputargstart; else sfree(outputargstart);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    WORD winsock_ver;
    WSADATA wsadata;
    WNDCLASS wndclass;
    MSG msg;

    ickproxy_instance = inst;

    /*
     * Parse the command line.
     */
    {
	int argc;
	char **argv;

	split_into_argv(cmdline, &argc, &argv, NULL);

	while (--argc > 0) {
	    char *p = *++argv;
	    if (*p == '-') {
		if (!strcmp(p, "-s")) {
		    if (--argc <= 0) {
			fprintf(stderr, "ick-proxy: -s expected a parameter\n");
			return 1;
		    }
		    override_script = *++argv;
		} else if (!strcmp(p, "-i")) {
		    if (--argc <= 0) {
			fprintf(stderr, "ick-proxy: -i expected a parameter\n");
			return 1;
		    }
		    override_inpac = *++argv;
		} else if (!strcmp(p, "-o")) {
		    if (--argc <= 0) {
			fprintf(stderr, "ick-proxy: -o expected a parameter\n");
			return 1;
		    }
		    override_outpac = *++argv;
		} else {
		    char *err = dupfmt("unrecognised command-line option '%s'",
				       p);
		    error(err);
		    sfree(err);
		    return 1;
		}
	    } else {
		error("unexpected command-line argument that is not an"
		      " option");
		return 1;
	    }
	}
    }

    winsock_ver = MAKEWORD(1, 1);
    if (WSAStartup(winsock_ver, &wsadata)) {
	MessageBox(NULL, "Unable to initialise WinSock", "WinSock Error",
		   MB_OK | MB_ICONEXCLAMATION);
	return 1;
    }
    if (LOBYTE(wsadata.wVersion) != 1 || HIBYTE(wsadata.wVersion) != 1) {
	MessageBox(NULL, "WinSock version is incompatible with 1.1",
		   "WinSock Error", MB_OK | MB_ICONEXCLAMATION);
	WSACleanup();
	return 1;
    }

    if (!prev) {
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = inst;
	wndclass.hIcon         = LoadIcon (inst,
					   MAKEINTRESOURCE(IDI_MAINICON));
	wndclass.hCursor       = LoadCursor (NULL, IDC_IBEAM);
	wndclass.hbrBackground = GetStockObject (BLACK_BRUSH);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = ickproxy_appname;

	RegisterClass (&wndclass);
    }

    ickproxy_hwnd = NULL;

    ickproxy_hwnd = CreateWindow (ickproxy_appname, ickproxy_appname,
                                  WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  100, 100, NULL, NULL, inst, NULL);

    ShowWindow (ickproxy_hwnd, SW_HIDE);

    sockets = newtree234(sockcmp);
    ick_proxy_setup();
    tray_init();

    /*
     * Single-user ick-proxy setup.
     */
    {
	char *outpac_text;
	char *outpac_file;
	char *err;

	outpac_text = configure_for_user("singleuser");
	assert(outpac_text);
	outpac_file = name_outpac_for_user(&err, "singleuser");
	if (!outpac_file) {
	    error(err);
	    return 1;
	} else {
	    FILE *fp = fopen(outpac_file, "w");
	    int pos, len, ret;

	    pos = 0;
	    len = strlen(outpac_text);
	    while (pos < len) {
		ret = fwrite(outpac_text + pos, 1, len - pos, fp);
		if (ret < 0) {
		    err = dupfmt("Unable to write to \"%s\"", outpac_file);
		    error(err);
		    cleanup_sockets();
		    WSACleanup();
		    return 1;
		} else {
		    pos += ret;
		}
	    }

	    fclose(fp);
	}
    }

    while (GetMessage (&msg, NULL, 0, 0) == 1) {
	TranslateMessage (&msg);
	DispatchMessage (&msg);
    }

    cleanup_sockets();
    WSACleanup();
    return msg.wParam;
}
