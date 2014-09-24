/*
 * WinURL: a small System tray application which will, on request,
 * convert the Clipboard contents into a URL and launch it with the
 * Windows default browser.
 * 
 * Copyright 2002-2006 Simon Tatham. All rights reserved.
 * 
 * You may copy and use this file under the terms of the MIT
 * Licence. For details, see the file LICENCE provided in the
 * WinURL distribution archive. At the time of writing, a copy of
 * the licence is also available at
 * 
 *   http://www.opensource.org/licenses/mit-license.html
 * 
 * but this cannot be guaranteed not to have changed in the future.
 */

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define IDI_MAINICON 200

#define WM_XUSER     (WM_USER + 0x2000)
#define WM_SYSTRAY   (WM_XUSER + 6)
#define WM_SYSTRAY2  (WM_XUSER + 7)

HINSTANCE winurl_instance;
HWND winurl_hwnd;
char const *winurl_appname = "WinURL";

#define IDM_CLOSE    0x0010
#define IDM_ABOUT    0x0020
#define IDM_LAUNCH   0x0030

#define IDHOT_LAUNCH 0x0010

static HMENU systray_menu;

static int CALLBACK AboutProc(HWND hwnd, UINT msg,
			      WPARAM wParam, LPARAM lParam);
static int CALLBACK LicenceProc(HWND hwnd, UINT msg,
				WPARAM wParam, LPARAM lParam);
HWND aboutbox = NULL;

/* Allocate the concatenation of N strings. Terminate arg list with NULL. */
char *dupcat(const char *s1, ...)
{
    int len;
    char *p, *q, *sn;
    va_list ap;

    len = strlen(s1);
    va_start(ap, s1);
    while (1) {
	sn = va_arg(ap, char *);
	if (!sn)
	    break;
	len += strlen(sn);
    }
    va_end(ap);

    p = malloc(len + 1);
    if (!p) return p;
    strcpy(p, s1);
    q = p + strlen(p);

    va_start(ap, s1);
    while (1) {
	sn = va_arg(ap, char *);
	if (!sn)
	    break;
	strcpy(q, sn);
	q += strlen(q);
    }
    va_end(ap);

    return p;
}

extern int systray_init(void) {
    BOOL res;
    NOTIFYICONDATA tnid;
    HICON hicon;

#ifdef NIM_SETVERSION
    tnid.uVersion = 0;
    res = Shell_NotifyIcon(NIM_SETVERSION, &tnid);
#endif

    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = winurl_hwnd; 
    tnid.uID = 1;                      /* unique within this systray use */
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hicon = LoadIcon (winurl_instance, MAKEINTRESOURCE(201));
    strcpy(tnid.szTip, "WinURL (URL launcher)");

    res = Shell_NotifyIcon(NIM_ADD, &tnid); 

    if (hicon) 
        DestroyIcon(hicon); 

    systray_menu = CreatePopupMenu();
    AppendMenu (systray_menu, MF_ENABLED, IDM_LAUNCH, "Launch URL");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_ABOUT, "About");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_CLOSE, "Close WinURL");

    SetMenuDefaultItem(systray_menu, IDM_LAUNCH, FALSE);

    return res; 
}

extern void systray_shutdown(void) {
    BOOL res; 
    NOTIFYICONDATA tnid; 
 
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = winurl_hwnd; 
    tnid.uID = 1;

    res = Shell_NotifyIcon(NIM_DELETE, &tnid); 

    DestroyMenu(systray_menu);
}

int hotkey_init(void)
{
    return RegisterHotKey(winurl_hwnd, IDHOT_LAUNCH, MOD_WIN, 'W');
}

void hotkey_shutdown(void)
{
    UnregisterHotKey(winurl_hwnd, IDHOT_LAUNCH);
}

static int launch_it(char *url)
{
    if (url) {
	return (int)ShellExecute(winurl_hwnd, "open", url,
				 NULL, NULL, SW_SHOWNORMAL);
    }
    return 0;
}

static int our_isspace(char c) {
    return (c && (isspace((unsigned char)c) || c == '\xA0'));
}

/* Check if the argument is a plausible URL scheme followed by a colon. */
static int is_url_scheme(const char *s) {
    if (isalpha((unsigned char)*s)) {
	s++;
	while (isalnum((unsigned char)*s))
	    s++;
	if (*s == ':')
	    return 1;
    }
    return 0;
}

/* Check if the argument is a vaguely plausible local-part followed
 * by an `@'. */
static int is_local_part(const char *s) {
    while (*s &&
	   (isalnum((unsigned char)*s) || strchr("!#$%&'*+-/=?^_`{|}~.", *s)))
	s++;
    return (*s == '@');
}

/*
 * Take a text string and try to extract a URL from it, and launch it.
 */
static int launch_url(char *s)
{
    int ret;
    if (is_url_scheme(s)) {
	/*
	 * Looks like it already has a URL scheme. Fine as it is.
	 */
	ret = launch_it(s);
    } else if (is_local_part(s)) {
	/*
	 * It's likely to be an email address, so we'll prefix
	 * `mailto:'.
	 */
	char *url = dupcat("mailto:", s, NULL);
	ret = launch_it(url);
	free(url);
    } else {
	/*
	 * No idea. We'll prefix `http://' on the
	 * assumption that it's a truncated URL of the form
	 * `www.thingy.com/wossname' or just `www.thingy.com'.
	 * 
	 * Exception: if the URL starts "//", we only prefix the
	 * `http:'.
	 */
	char *url;
	if (s[0] == '/' && s[1] == '/')
	    url = dupcat("http:", s, NULL);
	else
	    url = dupcat("http://", s, NULL);
	ret = launch_it(url);
	free(url);
    }
    return ret;
}

void do_launch_urls(void)
{
    HGLOBAL clipdata;
    char *s = NULL, *t = NULL, *p, *q, *r, *e;
    int len, ret, in_url = 0, n = 0;

    if (!OpenClipboard(NULL)) {
	goto error; /* unable to read clipboard */
    }
    clipdata = GetClipboardData(CF_TEXT);
    CloseClipboard();
    if (!clipdata) {
	goto error; /* clipboard contains no text */
    }
    s = GlobalLock(clipdata);
    if (!s) {
        goto error; /* unable to lock clipboard memory */
    }

    /*
     * Now strip (some) whitespace from the URL text.
     * In a future version this might be made configurable.
     */
    len = strlen(s);
    t = malloc(len+8);                 /* leading "http://" plus trailing \0 */
    if (!t) {
	GlobalUnlock(s);
	goto error;		       /* out of memory */
    }

    p = s;
    q = t;
    /* Strip leading whitespace. */
    while (p[0] && our_isspace(p[0]))
	p++;

    /* Find any newlines, and close up any white-space around them. */
    while ((r = strpbrk(p, "\r\n"))) {
	char *rr = r;
	/* Strip whitespace before newline(s) */
	while (our_isspace(rr[0]))
	    rr--;
	while (p <= rr)
	    *q++ = *p++;
	/* Strip newline(s) and following whitespace */
	p = r;
	while (p[0] && our_isspace(p[0]))
	    p++;
    }

    /* Strip any trailing whitespace */
    r = p + strlen(p);
    while (r > p && our_isspace(r[-1]))
	r--;
    while (p < r)
	*q++ = *p++;

    *q = '\0';

    GlobalUnlock(s);

    /*
     * Now we have whitespace-filtered text in t.
     * First look for <>-delimited strings per RFC3986, and try to
     * launch any we find as URLs.
     */
    q = t;
    while ((e = strpbrk(q, "<>"))) {
	switch (*e) {
	  case '<':
	    q = e+1;
	    in_url = 1;
	    break;
	  case '>':
	    if (in_url) {
		/* deal with RFC1738 <URL:...> */
		if (strncmp(q, "URL:", 4) == 0)
		    q += 4;
		/* Remove whitespace between < / <URL: / > and URL. */
		while (our_isspace(*q)) q++;
		{
		    char *ee = e-1;
		    while (our_isspace(*ee)) ee--;
		    ee[1] = '\0';
		}
		/* We may as well allow non-RFC <foo@example.com> and
		 * indeed <URL:www.example.com>. */
		(void) launch_url(q);
		n++;
		in_url = 0;
	    }
	    q = e+1;
	    break;
	}
    }

    if (!n) {
	/* Didn't find any URLs by that method. Try parsing the string
	 * as a whole as one. */
	ret = launch_url(t);
    }

    free(t);

    error:
    /*
     * Not entirely sure what we should do in case of error here.
     * Perhaps a simple beep might be suitable. Then again, `out of
     * memory' is a bit scarier. FIXME: should probably do
     * different error handling depending on context.
     */
    ;
}

static char *makeversion(char *buffer, char *revision)
{
    char *p = buffer;
    strcpy(buffer, revision);
    p += strcspn(p, "0123456789");
    if (*p) {
	p[strcspn(p, " $")] = '\0';
    }
    if (!*p)
	return NULL;
    return p;
}

#define STR2(s) #s
#define STR(s) STR2(s)

static void showversion(HWND hwnd, UINT ctrl)
{
    char versionbuf[80], buffer[80];

#ifdef VERSION
    sprintf(buffer, "WinURL version %.50s", STR(VERSION));
#else
    sprintf(buffer, "WinURL unknown version");
#endif
    SetDlgItemText(hwnd, ctrl, buffer);
}


static int CALLBACK AboutProc(HWND hwnd, UINT msg,
			      WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
      case WM_INITDIALOG:
	showversion(hwnd, 102);
	return 1;
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDOK:
	  case IDCANCEL:
	    aboutbox = NULL;
	    DestroyWindow(hwnd);
	    return 0;
	  case 112:
	    EnableWindow(hwnd, 0);
	    DialogBox(winurl_instance, MAKEINTRESOURCE(301),
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
	  case IDCANCEL:
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
                                 WPARAM wParam, LPARAM lParam) {
    int ret;
    POINT cursorpos;                   /* cursor position */
    static int menuinprogress;

    switch (message) {
      case WM_SYSTRAY:
	if (lParam == WM_RBUTTONUP) {
	    GetCursorPos(&cursorpos);
	    PostMessage(hwnd, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
	} else if (lParam == WM_LBUTTONUP) {
	    do_launch_urls();
	}
	break;
      case WM_SYSTRAY2:
        if (!menuinprogress) {
            menuinprogress = 1;
            SetForegroundWindow(hwnd);
            ret = TrackPopupMenu(systray_menu,
                                 TPM_RIGHTALIGN | TPM_BOTTOMALIGN |
                                 TPM_RIGHTBUTTON,
                                 wParam, lParam, 0, winurl_hwnd, NULL);
            menuinprogress = 0;
        }
	break;
      case WM_HOTKEY:
	if (wParam == IDHOT_LAUNCH)
	    do_launch_urls();
	return 0;
      case WM_COMMAND:
	if ((wParam & ~0xF) == IDM_CLOSE) {
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
	} else if ((wParam & ~0xF) == IDM_LAUNCH) {
	    do_launch_urls();
	} else if ((wParam & ~0xF) == IDM_ABOUT) {
	    if (!aboutbox) {
		aboutbox = CreateDialog(winurl_instance,
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
	}
	break;
      case WM_DESTROY:
	PostQuitMessage (0);
	return 0;
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
    WNDCLASS wndclass;
    MSG msg;

    winurl_hwnd = NULL;

    while (*cmdline && isspace(*cmdline)) cmdline++;
    if (cmdline[0] == '-' && cmdline[1] == 'o' &&
	(!cmdline[2] || isspace(cmdline[2]))) {
	/*
	 * -o means _just_ launch the current clipboard contents as
	 * a URL and then terminate, rather than starting up a
	 * window etc.
	 */
	do_launch_urls();
	return 0;
    }

    winurl_instance = inst;

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
	wndclass.lpszClassName = winurl_appname;

	RegisterClass (&wndclass);
    }

    winurl_hwnd = CreateWindow (winurl_appname, winurl_appname,
				WS_OVERLAPPEDWINDOW | WS_VSCROLL,
				CW_USEDEFAULT, CW_USEDEFAULT,
				100, 100, NULL, NULL, inst, NULL);

    if (systray_init()) {

	ShowWindow (winurl_hwnd, SW_HIDE);

	if (!hotkey_init()) {
	    MessageBox(winurl_hwnd, "Unable to register hot key",
		       "WinURL Warning", MB_ICONWARNING | MB_OK);
	}

	while (GetMessage (&msg, NULL, 0, 0) == 1) {
	    TranslateMessage (&msg);
	    DispatchMessage (&msg);
	}
	
	hotkey_shutdown();
	systray_shutdown();
    }

    return msg.wParam;
}
