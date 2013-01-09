/*
 * Windows front end for ick-keys, sitting in the System Tray.
 */

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#include "misc.h"
#include "icklang.h"
#include "xplib.h"

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

HINSTANCE ickkeys_instance;
HWND ickkeys_hwnd;
HWND aboutbox = NULL;
const char ickkeys_appname[] = "ick-keys";

char *override_script = NULL;

void configure(void);
static void tray_shutdown(void);

void error(char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    MessageBox(ickkeys_hwnd, buf, ickkeys_appname, MB_OK | MB_ICONERROR);
}

void platform_fatal_error(const char *s)
{
    MessageBox(ickkeys_hwnd, s, ickkeys_appname, MB_OK | MB_ICONERROR);
    tray_shutdown();
    exit(1);
}

static char *translate_error_code(DWORD error)
{
    LPTSTR sysbuf;
    char *ret;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		  FORMAT_MESSAGE_FROM_SYSTEM |
		  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR)&sysbuf, 0, NULL);
    ret = dupstr(sysbuf);
    LocalFree(sysbuf);
    return ret;
}

static HWND select_window(int order)
{
    POINT p;
    HWND w, w2;

    GetCursorPos(&p);
    w = NULL;
    while (1) {
        switch (order & FIRST_CHOICE_MASK) {
          case END_OF_OPTIONS:
            return NULL;
          case WINDOW_UNDER_MOUSE:
            if ((w = WindowFromPoint(p)) != NULL) {
                while ((w2 = GetParent(w)) != NULL)
                    w = w2;
            }
            if (w == GetDesktopWindow() || w == GetShellWindow())
                w = NULL;
            break;
          case WINDOW_WITH_FOCUS:
            w = GetForegroundWindow();
            break;
        }
        if (w)
            return w;

        order >>= CHOICE_SHIFT;
    }
}

void minimise_window(int order)
{
    HWND w = select_window(order);
    if (w) {
	if (SendMessage(w, WM_SYSCOMMAND, SC_MINIMIZE, -1))
	    ShowWindow(w, SW_MINIMIZE);
    }
}

void maximise_window(int order)
{
    HWND w = select_window(order);
    if (w) {
        /*
         * We're actually _toggling_ maximised state here, so first we
         * must find out whether the window is already maximised, and
         * then we can decide what to try to turn it into.
         */
        WINDOWPLACEMENT wp;
        UINT showcmd;
        WPARAM syscmd;

        if (!GetWindowPlacement(w, &wp))
            return;

        showcmd = (wp.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
        syscmd  = (wp.showCmd == SW_MAXIMIZE ? SC_RESTORE : SC_MAXIMIZE);

	if (SendMessage(w, WM_SYSCOMMAND, syscmd, -1))
	    ShowWindow(w, showcmd);
    }
}

void window_to_back(int order)
{
    HWND w = select_window(order);
    if (w)
	SetWindowPos(w, HWND_BOTTOM, 0, 0, 0, 0,
		     SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE |
		     SWP_NOMOVE | SWP_NOSIZE);
}

void kill_window(int order)
{
    HWND w = select_window(order);
    if (w)
        SendMessage(w, WM_SYSCOMMAND, SC_CLOSE, -1);
}

char *read_clipboard(void)
{
    HGLOBAL clipdata;
    char *s, *ret;

    if (!OpenClipboard(NULL)) {
        error("unable to read clipboard");
	return NULL;
    }
    clipdata = GetClipboardData(CF_TEXT);
    CloseClipboard();
    if (!clipdata) {
        /* clipboard contains no text */
	return dupstr("");
    }
    s = GlobalLock(clipdata);
    if (!s) {
        error("unable to lock clipboard memory");
	return NULL;
    }
    ret = dupstr(s);
    GlobalUnlock(s);
    return ret;
}

void write_clipboard(const char *data)
{
    HGLOBAL clipdata;
    int len = strlen(data);
    char *lock;

    clipdata = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, len+1);

    if (!clipdata) {
        error("unable to allocate clipboard memory");
    }
    if (!(lock = GlobalLock(clipdata))) {
        GlobalFree(clipdata);
        error("unable to lock clipboard memory");
    }

    strcpy(lock, data);

    GlobalUnlock(clipdata);
    if (OpenClipboard(ickkeys_hwnd)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, clipdata);
        CloseClipboard();
    } else {
        GlobalFree(clipdata);
        error("unable to write to clipboard");
    }
}

void set_unix_url_opener_command(const char *cmd)
{
    /* ignored on this platform */
}

void open_url(const char *url)
{
    ShellExecute(ickkeys_hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void debug_message(const char *msg)
{
    MessageBox(ickkeys_hwnd, msg, "ick-keys debug message",
	       MB_OK | MB_ICONINFORMATION);
}

void spawn_process(const char *cmd)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;
    CreateProcess(NULL, cmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS,
		  NULL, NULL, &si, &pi);
}

int *hotkeyids = NULL;
int hotkeyidsize = 0;

#define IDM_CLOSE    0x0010
#define IDM_ABOUT    0x0020
#define IDM_RECONF   0x0030
#define IDM_HK_BASE  0x0040

static HMENU systray_menu;
static int systray_nhk;  /* number of hotkey-related menu entries,
                          * including a separator below all hotkeys */

void unregister_all_hotkeys(void)
{
    int i;
    for (i = 0; i < hotkeyidsize; i++)
	if (hotkeyids[i] >= 0) {
	    UnregisterHotKey(ickkeys_hwnd, hotkeyids[i]);
	    hotkeyids[i] = -1;
	}
    while (systray_nhk > 0) {
        DeleteMenu(systray_menu, systray_nhk-1, MF_BYPOSITION);
        systray_nhk--;
    }
}

int register_hotkey(int index, int modifiers, const char *key,
		    const char *keyorig)
{
    int id = (index + 1) * 16;
    int mods;
    char name[512];

    if (index >= hotkeyidsize) {
	int oldsize = hotkeyidsize;
	hotkeyidsize = index * 3 / 2 + 16;
	hotkeyids = sresize(hotkeyids, hotkeyidsize, int);
	while (oldsize < hotkeyidsize)
	    hotkeyids[oldsize++] = -1;
    }

    mods = 0;
    name[0] = '\0';
    if (modifiers & LEFT_WINDOWS) {
	mods |= MOD_WIN;
        strcat(name, "Windows-");
    }
    if (modifiers & ALT) {
	mods |= MOD_ALT;
        strcat(name, "Alt-");
    }
    if (modifiers & CTRL) {
	mods |= MOD_CONTROL;
        strcat(name, "Ctrl-");
    }
    if (modifiers & SHIFT) {
	mods |= MOD_SHIFT;
        strcat(name, "Shift-");
    }
    sprintf(name + strlen(name), "%c", (char)toupper((unsigned char)key[0]));

    if (!RegisterHotKey(ickkeys_hwnd, id, mods,
			(char)toupper((unsigned char)key[0]))) {
	char *syserr = translate_error_code(GetLastError());
	error("Unable to register hot key '%s': %s\n",
	      keyorig, syserr);
	sfree(syserr);
	return 0;
    }

    if (!systray_nhk) {
        InsertMenu(systray_menu, systray_nhk,
                   MF_BYPOSITION | MF_SEPARATOR, 0, 0);
        systray_nhk++;
    }
    InsertMenu(systray_menu, systray_nhk - 1, MF_BYPOSITION,
               IDM_HK_BASE + 0x10 * index, name);
    systray_nhk++;

    hotkeyids[index] = id;
    return 1;
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
    tnid.hWnd = ickkeys_hwnd;
    tnid.uID = 1;                      /* unique within this systray use */
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hicon = LoadIcon (ickkeys_instance, MAKEINTRESOURCE(201));
    strcpy(tnid.szTip, "ick-keys (miscellaneous hot-keys utility)");

    res = Shell_NotifyIcon(NIM_ADD, &tnid); 

    if (hicon) 
        DestroyIcon(hicon); 

    systray_menu = CreatePopupMenu();
    AppendMenu (systray_menu, MF_ENABLED, IDM_ABOUT, "&About");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_RECONF,
		"&Re-read configuration");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_CLOSE, "&Close ick-keys");

    return res; 
}

static void tray_shutdown(void)
{
    BOOL res; 
    NOTIFYICONDATA tnid; 
 
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = ickkeys_hwnd;
    tnid.uID = 1;

    res = Shell_NotifyIcon(NIM_DELETE, &tnid); 

    DestroyMenu(systray_menu);
}

/*
 * The About box.
 */
static int CALLBACK LicenceProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam);

void showversion(char *buffer)
{
    /*
     * This string is edited by the bob build script to show the
     * Subversion revision number used in the checkout.
     * 
     * At least, I hope so.
     */
    char *revision = "~SVNREVISION~";
    if (revision[0] == '~') {
	/* If not, we fall back to this. */
	sprintf(buffer, "Unspecified revision");
    } else {
	sprintf(buffer, "Revision %s", revision);
    }
}

static int CALLBACK AboutProc(HWND hwnd, UINT msg,
			      WPARAM wParam, LPARAM lParam)
{
    char vbuf[160];
    switch (msg) {
      case WM_INITDIALOG:
	showversion(vbuf);
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
	    DialogBox(ickkeys_instance, MAKEINTRESOURCE(301),
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
    int i;

    switch (message) {
      case WM_DESTROY:
	PostQuitMessage (0);
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
                                 wParam, lParam, 0, ickkeys_hwnd, NULL);
            menuinprogress = 0;
        }
	return 0;
      case WM_HOTKEY:
	for (i = 0; i < hotkeyidsize; i++)
	    if (hotkeyids[i] >= 0 && hotkeyids[i] == wParam)
		break;
	if (i == hotkeyidsize)
	    return 0;
	run_hotkey(i);
	break;
      case WM_COMMAND:
	if ((wParam & ~0xF) == IDM_CLOSE) {
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	    return 0;
	}
	if ((wParam & ~0xF) == IDM_ABOUT) {
	    if (!aboutbox) {
		aboutbox = CreateDialog(ickkeys_instance,
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
	if ((wParam & ~0xF) == IDM_RECONF) {
	    configure();
	    return 0;
	}
        if ((wParam & ~0xF) >= IDM_HK_BASE &&
            (wParam & ~0xF) < (unsigned)(IDM_HK_BASE + 0x10 * hotkeyidsize)) {
            i = (unsigned)((wParam & ~0xF) - IDM_HK_BASE) / 0x10;
	    if (hotkeyids[i] >= 0) {
                run_hotkey(i);
                return 0;
            }            
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
    WNDCLASS wndclass;
    MSG msg;

    ickkeys_instance = inst;

    /*
     * Parse the command line.
     */
    {
	int argc;
	char **argv;

	split_into_argv(cmdline, &argc, &argv, NULL);

	argc++;
	argv--;
	while (--argc > 0) {
	    char *p = *++argv;
	    if (*p == '-') {
		char *err = dupfmt("unrecognised command-line option '%s'",
				   p);
		error(err);
		sfree(err);
		return 1;
	    } else {
		add_config_filename(p);
	    }
	}
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
	wndclass.lpszClassName = ickkeys_appname;

	RegisterClass (&wndclass);
    }

    ickkeys_hwnd = NULL;

    ickkeys_hwnd = CreateWindow (ickkeys_appname, ickkeys_appname,
				 WS_OVERLAPPEDWINDOW | WS_VSCROLL,
				 CW_USEDEFAULT, CW_USEDEFAULT,
				 100, 100, NULL, NULL, inst, NULL);

    ShowWindow (ickkeys_hwnd, SW_HIDE);

    tray_init();

    configure();

    while (GetMessage (&msg, NULL, 0, 0) == 1) {
	TranslateMessage (&msg);
	DispatchMessage (&msg);
    }

    tray_shutdown(); 
    return msg.wParam;
}
