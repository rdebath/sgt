/*
 * Windows network listener. Waits for incoming connections, and
 * forks off a thread to deal with each one.
 *
 * Intended to be reusable. Parts requiring modification are in a
 * separate module.
 */

/*
 * Imports from other modules:
 *
 *  - `listener_appname' is a `char const *' pointing at an
 *    application name.
 *
 *  - `listener_nports' is an `int' giving the number of ports to
 *    listen on.
 *
 *  - `listener_ports' is an `int const *' giving the array of
 *    those ports.
 *
 *  - `listener_newthread' is a function to be run when a
 *    connection has been accepted. Takes as arguments: the new
 *    socket, the port number the connection arrived on, and the
 *    address of the remote end of the connection.
 *
 *  - `listener_cmdline' is a function called at startup with the
 *    program's command line.
 *
 *  - resource 200 should be an icon.
 */

#include <windows.h>
#include <winsock.h>

#define IDI_MAINICON 200

#define WM_XUSER     (WM_USER + 0x2000)
#define WM_NETEVENT  (WM_XUSER + 5)

extern char const *listener_appname;
extern int listener_nports;
extern int const *listener_ports;
extern void listener_cmdline(char *cmdline);
extern int listener_newthread(SOCKET sock, int port, SOCKADDR_IN remoteaddr);

HINSTANCE listener_instance;
HWND listener_hwnd;
static SOCKET *mainsocks;

static void error(char *s) {
    MessageBox(listener_hwnd, s, listener_appname, MB_OK | MB_ICONERROR);
}

static void setup_sockets(void) {
    int i;
    SOCKADDR_IN addr;

    mainsocks = malloc(listener_nports * sizeof(SOCKET));
    if (mainsocks == NULL) {
        error("Out of memory during init"); return;
    }

    for (i = 0; i < listener_nports; i++) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET)
            switch (WSAGetLastError()) {
              case WSAENETDOWN:
                error("Network is down"); return;
              case WSAEAFNOSUPPORT:
                error("TCP/IP support not present"); return;
              default: error("socket(): unknown error"); return;
            }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons((short)listener_ports[i]);
        if (bind (s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
            switch (WSAGetLastError()) {
              case WSAENETDOWN: error("Network is down"); return;
              default: error("bind(): unknown error"); return;
            }

        if (listen(s, 5) == SOCKET_ERROR)
            switch (WSAGetLastError()) {
              case WSAENETDOWN: error("Network is down"); return;
              default: error("listen(): unknown error"); return;
            }

        if (WSAAsyncSelect (s, listener_hwnd,
                            WM_NETEVENT, FD_ACCEPT) == SOCKET_ERROR)
            switch (WSAGetLastError()) {
              case WSAENETDOWN: error("Network is down"); return;
              default: error("WSAAsyncSelect(): unknown error"); return;
            }

        mainsocks[i] = s;
    }
}

static void cleanup_sockets(void) {
    int i;

    for (i = 0; i < listener_nports; i++) {
        closesocket(mainsocks[i]);
    }
}

struct listener_args {
    SOCKET sock;
    SOCKADDR_IN addr;
    int port;
};

DWORD WINAPI newthread(LPVOID param) {
    struct listener_args *args = (struct listener_args *)param;
    int ret;
    ret = listener_newthread(args->sock, args->port, args->addr);
    free(param);
    return ret;
}

static LRESULT CALLBACK WndProc (HWND hwnd, UINT message,
                                 WPARAM wParam, LPARAM lParam) {
    switch (message) {
      case WM_DESTROY:
	PostQuitMessage (0);
	return 0;
      case WM_NETEVENT:
        {
            SOCKET s = (SOCKET) wParam;
            SOCKET newsock;
            SOCKADDR_IN addr;
            u_long tmp;
            struct listener_args *args;
            DWORD tid;
            int addrlen, i;

            /* assert WSAGETSELECTEVENT(lParam) == FD_ACCEPT */
            addrlen = sizeof(addr);
            newsock = accept(s, (SOCKADDR *)&addr, &addrlen);
            if (newsock == SOCKET_ERROR)
                switch (WSAGetLastError()) {
                  case WSAENETDOWN: error("Network is down"); return 0;
                  default: error("listen(): unknown error"); return 0;
                }
            /*
             * Disable async-select on the new socket.
             */
            WSAAsyncSelect(newsock, hwnd, 0, 0);
            /*
             * Return the socket to blocking mode.
             */
            tmp = 0;
            ioctlsocket(newsock, FIONBIO, &tmp);
            /*
             * Now start a new thread to deal with the new socket.
             */
            args = malloc(sizeof(*args));
            if (args) {
                args->sock = newsock;
                args->addr = addr;     /* structure copy */
                args->port = 0;
                for (i = 0; i < listener_nports; i++)
                    if (mainsocks[i] == s)
                        args->port = listener_ports[i];
                /* Win95: the thread-id param may not be NULL. So we have
                 * a dummy DWORD which receives the thread id which we then
                 * throw away :-) */
                CreateThread(NULL, 0, &newthread, args, 0, &tid);
            } else {
                closesocket(newsock);
            }
        }
	return 0;
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
    WORD winsock_ver;
    WSADATA wsadata;
    WNDCLASS wndclass;
    MSG msg;

    listener_instance = inst;

    listener_cmdline(cmdline);

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
	wndclass.lpszClassName = listener_appname;

	RegisterClass (&wndclass);
    }

    listener_hwnd = NULL;

    listener_hwnd = CreateWindow (listener_appname, listener_appname,
                                  WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  100, 100, NULL, NULL, inst, NULL);

    setup_sockets();

    ShowWindow (listener_hwnd, SW_HIDE);

    while (GetMessage (&msg, NULL, 0, 0) == 1) {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    cleanup_sockets();
    WSACleanup();
    return msg.wParam;
}
