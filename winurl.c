/*
 * WinURL: a small System tray application which will, on request,
 * convert the Clipboard contents into a URL and launch it with the
 * Windows default browser.
 * 
 * Copyright 2002 Simon Tatham. All rights reserved.
 * 
 * You may copy and use this file under the terms of the MIT
 * Licence. For details, see the file LICENCE provided in the <<PROGRAMNAME>>
 * distribution archive. At the time of writing, a copy of the
 * licence is also available at
 * 
 *   http://www.opensource.org/licenses/mit-license.html
 * 
 * but this cannot be guaranteed not to have changed in the future.
 */

#include <windows.h>

#include <stdlib.h>

#define IDI_MAINICON 200

#define WM_XUSER     (WM_USER + 0x2000)
#define WM_SYSTRAY   (WM_XUSER + 6)
#define WM_SYSTRAY2  (WM_XUSER + 7)

HINSTANCE winurl_instance;
HWND winurl_hwnd;
char const *winurl_appname = "WinURL";

#define IDM_CLOSE    0x0010
#define IDM_ABOUT    0x0020

static HMENU systray_menu;

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
    AppendMenu (systray_menu, MF_ENABLED, IDM_ABOUT, "About");
    AppendMenu(systray_menu, MF_SEPARATOR, 0, 0);
    AppendMenu (systray_menu, MF_ENABLED, IDM_CLOSE, "Close WinURL");

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

char *read_clip(int *is_err)
{
    HGLOBAL clipdata;
    char *s;

    if (!OpenClipboard(NULL)) {
        *is_err = 1;
        return "-unable to read clipboard\r\n";
    }
    clipdata = GetClipboardData(CF_TEXT);
    CloseClipboard();
    if (!clipdata) {
        *is_err = 1;
        return "-clipboard contains no text\r\n";
    }
    s = GlobalLock(clipdata);
    if (!s) {
        *is_err = 1;
        return "-unable to lock clipboard memory\r\n";
    }
    *is_err = 0;
    return s;
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
      case WM_COMMAND:
	if ((wParam & ~0xF) == IDM_CLOSE) {
	    SendMessage(hwnd, WM_CLOSE, 0, 0);
//	} else if ((wParam & ~0xF) == IDM_ABOUT) {
//	    if (!aboutbox) {
//		aboutbox = CreateDialog(winurl_instance,
//					MAKEINTRESOURCE(300),
//					NULL, AboutProc);
//		ShowWindow(aboutbox, SW_SHOWNORMAL);
//		/*
//		 * Sometimes the window comes up minimised / hidden
//		 * for no obvious reason. Prevent this.
//		 */
//		SetForegroundWindow(aboutbox);
//		SetWindowPos(aboutbox, HWND_TOP, 0, 0, 0, 0,
//			     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
//	    }
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

    winurl_hwnd = NULL;

    winurl_hwnd = CreateWindow (winurl_appname, winurl_appname,
				WS_OVERLAPPEDWINDOW | WS_VSCROLL,
				CW_USEDEFAULT, CW_USEDEFAULT,
				100, 100, NULL, NULL, inst, NULL);

    if (systray_init()) {

	ShowWindow (winurl_hwnd, SW_HIDE);

	while (GetMessage (&msg, NULL, 0, 0) == 1) {
	    TranslateMessage (&msg);
	    DispatchMessage (&msg);
	}
	
	systray_shutdown();
    }

    return msg.wParam;
}
