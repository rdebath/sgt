/*
 * DoIt: a remote-execution thingy for Windows.
 */

#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <string.h>

#include "doit.h"

typedef unsigned int uint32;

extern HWND listener_hwnd;
extern HINSTANCE listener_instance;

typedef struct {
    uint32 h[5];
} SHA_Core_State;

#define SHA_BLKSIZE 64

typedef struct {
    SHA_Core_State core;
    unsigned char block[SHA_BLKSIZE];
    int blkused;
    uint32 lenhi, lenlo;
} SHA_State;

void SHA_Init(SHA_State *s);
void SHA_Bytes(SHA_State *s, void *p, int len);
void SHA_Final(SHA_State *s, unsigned char *output);

static char *secret;
static int secretlen;

/*
 * Export the application name.
 */
char const *listener_appname = "DoIt";

/*
 * Export the list of ports to listen on.
 */
static int const port_array[] = { DOIT_PORT };
int listener_nports = sizeof(port_array) / sizeof(*port_array);
int const *listener_ports = port_array;

/*
 * Helper function to deal with send() partially completing.
 */
static int do_send(SOCKET sock, void *buffer, int len)
{
    char *buf = (char *)buffer;
    int ret, sent;

    sent = 0;
    while (len > 0 && (ret = send(sock, buf, len, 0)) > 0) {
        buf += ret;
        len -= ret;
        sent += ret;
    }
    if (ret <= 0)
        return ret;
    else
        return sent;
}

char *write_clip(char *data, int len)
{
    HGLOBAL clipdata;
    char *lock;

    clipdata = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, len+1);

    if (!clipdata) {
        return "-GlobalAlloc failed\n";
    }
    if (!(lock = GlobalLock(clipdata))) {
        GlobalFree(clipdata);
        return "-GlobalLock failed\n";
    }

    memcpy(lock, data, len);
    lock[len] = '\0';                  /* trailing null */

    GlobalUnlock(clipdata);
    if (OpenClipboard(listener_hwnd)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, clipdata);
        CloseClipboard();
        return "+\n";
    } else {
        GlobalFree(clipdata);
        return "-OpenClipboard failed\r\n";
    }
}

char *read_clip(int *is_err)
{
    HGLOBAL clipdata;
    char *s;

    if (!OpenClipboard(NULL)) {
        *is_err = 1;
        return "-OpenClipboard failed\r\n";
    }
    clipdata = GetClipboardData(CF_TEXT);
    CloseClipboard();
    if (!clipdata) {
        *is_err = 1;
        return "-GetClipboardData failed\r\n";
    }
    s = GlobalLock(clipdata);
    if (!s) {
        *is_err = 1;
        return "-GlobalLock failed\r\n";
    }
    *is_err = 0;
    return s;
}

struct process {
    char *error;
    HANDLE fromchild;
    HANDLE hproc;
};

struct process start_process(char *cmdline, int wait, int output)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE fromchild, tochild;
    HANDLE childout, parentout, childin, parentin;
    int inherit = FALSE;
    struct process ret;

    ret.error = NULL;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.wShowWindow = SW_SHOWNORMAL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    if (output) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        if (!CreatePipe(&parentout, &childout, &sa, 0) ||
            !CreatePipe(&childin, &parentin, &sa, 0)) {
            ret.error = "-CreatePipe failed\n";
            return ret;
        }
        if (!DuplicateHandle(GetCurrentProcess(), parentin,
                             GetCurrentProcess(), &tochild,
                             0, FALSE, DUPLICATE_SAME_ACCESS)) {
            ret.error = "-DuplicateHandle failed\n";
            return ret;
        }
        CloseHandle(parentin);
        if (!DuplicateHandle(GetCurrentProcess(), parentout,
                             GetCurrentProcess(), &fromchild,
                             0, FALSE, DUPLICATE_SAME_ACCESS)) {
            ret.error = "-DuplicateHandle failed\n";
            return ret;
        }
        CloseHandle(parentout);
        si.hStdInput = childin;
        si.hStdOutput = si.hStdError = childout;
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        inherit = TRUE;
        ret.fromchild = fromchild;
    }
    if (CreateProcess(NULL, cmdline, NULL, NULL, inherit,
                      CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS,
                      NULL, NULL, &si, &pi) == 0) {
        ret.error = "-CreateProcess failed\n";
    } else {
        ret.hproc = pi.hProcess;
    }
    if (output) {
        CloseHandle(childin);
        CloseHandle(childout);
        CloseHandle(tochild);
        if (ret.error)
            CloseHandle(fromchild);
    }
    return ret;
}

int read_process_output(struct process proc, char *buf, int len)
{
    DWORD got;
    if (ReadFile(proc.fromchild, buf, len, &got, NULL) && got > 0) {
        return got;
    } else
        return 0;
}

int process_exit_code(struct process proc)
{
    DWORD exitcode;

    WaitForSingleObject(proc.hproc, INFINITE);
    if (!GetExitCodeProcess(proc.hproc, &exitcode))
        return -1;
    else
        return exitcode;
}

/*
 * Helper function to fetch a whole line from the socket.
 */
char *do_fetch(SOCKET sock, doit_ctx *ctx, int line_terminate, int *length)
{
    char *cmdline = NULL;
    int cmdlen = 0, cmdsize = 0;
    char buf[1024];
    int len;

    /*
     * Start with any existing buffered data.
     */
    len = doit_incoming_data(ctx, NULL, 0);
    if (len < 0)
        return NULL;
    cmdline = malloc(256);
    cmdlen = 0;
    cmdsize = 256;
    while (1) {
        if (len > 0) {
            if (cmdsize < cmdlen + len + 1) {
                cmdsize = cmdlen + len + 1 + 256;
                cmdline = realloc(cmdline, cmdsize);
                if (!cmdline)
                    return NULL;
            }
            while (len > 0) {
                doit_read(ctx, cmdline+cmdlen, 1);
                if (line_terminate &&
                    cmdlen > 0 && cmdline[cmdlen] == '\n') {
                    cmdline[cmdlen] = '\0';
                    *length = cmdlen;
                    return cmdline;
                }
                cmdlen++;
                len--;
            }
        }
        len = recv(sock, buf, sizeof(buf), 0);
        if (len <= 0) {
            *length = cmdlen;
            return line_terminate ? NULL : cmdline;
        }
        len = doit_incoming_data(ctx, buf, len);
        if (len < 0)
            return NULL;
    }
}
char *do_fetch_line(SOCKET sock, doit_ctx *ctx)
{
    int len;
    return do_fetch(sock, ctx, 1, &len);
}
char *do_fetch_all(SOCKET sock, doit_ctx *ctx, int *len)
{
    return do_fetch(sock, ctx, 0, len);
}

/*
 * Helper functions to encrypt and send data.
 */
int do_doit_send(SOCKET sock, doit_ctx *ctx, void *data, int len)
{
    void *odata;
    int olen;
    odata = doit_send(ctx, data, len, &olen);
    if (odata) {
        int ret = do_send(sock, odata, olen);
        free(odata);
        return ret;
    } else {
        return -1;
    }
}
int do_doit_send_str(SOCKET sock, doit_ctx *ctx, char *str)
{
    return do_doit_send(sock, ctx, str, strlen(str));
}

/*
 * Export the function that handles a connection.
 */
int listener_newthread(SOCKET sock, int port, SOCKADDR_IN remoteaddr) {
    int len;
    char *nonce = NULL;
    doit_ctx *ctx = NULL;
    char *cmdline = NULL;
    DWORD threadid;

    ctx = doit_init_ctx(secret, secretlen);
    if (!ctx)
        goto done;

    doit_perturb_nonce(ctx, "server", 6);
    doit_perturb_nonce(ctx, &remoteaddr, sizeof(remoteaddr));
    threadid = GetCurrentThreadId();
    doit_perturb_nonce(ctx, &threadid, sizeof(threadid));
    nonce = doit_make_nonce(ctx, &len);
    if (do_send(sock, nonce, len) != len)
        goto done;
    free(nonce);
    nonce = NULL;

    cmdline = do_fetch_line(sock, ctx);
    if (!cmdline)
        goto done;

    if (!strcmp(cmdline, "ShellExecute")) {
        /*
         * Read a second line and feed it to ShellExecute(). Give
         * back either "+" (meaning OK) or "-" followed by an error
         * message (meaning not OK).
         */
        free(cmdline); cmdline = NULL;
        cmdline = do_fetch_line(sock, ctx);
        if (32 >= (int)ShellExecute(listener_hwnd, NULL, cmdline, NULL,
                                    NULL, SW_SHOWNORMAL)) {
            do_doit_send_str(sock, ctx, "-ShellExecute failed\n");
        } else {
            do_doit_send_str(sock, ctx, "+\n");
        }
        free(cmdline); cmdline = NULL;
        goto done;
    }

    if (!strcmp(cmdline, "WriteClipboard")) {
        /*
         * Read data until the connection is closed, and write it
         * to the Windows clipboard.
         */
        int len;
        char *msg;
        free(cmdline); cmdline = NULL;
        cmdline = do_fetch_all(sock, ctx, &len);
        if (cmdline) {
            msg = write_clip(cmdline, len);
            free(cmdline); cmdline = NULL;
        } else
            msg = "-error reading input\n";
        do_doit_send_str(sock, ctx, msg);
        goto done;
    }

    if (!strcmp(cmdline, "ReadClipboard")) {
        /*
         * Read the Windows Clipboard. Give back either "+\n"
         * followed by the clipboard data, or "-" followed by an
         * error message and "\n".
         */
        int is_err;
        char *data = read_clip(&is_err);
        if (is_err) {
            /* data is an error message */
            do_doit_send_str(sock, ctx, data);
        } else {
            do_doit_send_str(sock, ctx, "+\n");
            do_doit_send(sock, ctx, data, strlen(data));
            GlobalUnlock(data);
        }
        goto done;
    }

    if (!strcmp(cmdline, "CreateProcessNoWait") ||
        !strcmp(cmdline, "CreateProcessWait") ||
        !strcmp(cmdline, "CreateProcessWithOutput")) {
        /*
         * Read a second line and feed it to CreateProcess.
         * Optionally, wait for it to finish, or even send output
         * back.
         * 
         * If output is sent back, it is sent as a sequence of
         * Pascal-style length-prefixed strings (a single byte
         * followed by that many characters), and finally
         * terminated by a \0 length byte. After _that_ comes the
         * error indication, which may be "+number\n" for a
         * termination with exit code, or "-errmsg\n" if a system
         * call fails.
         */
        int wait, output;
        struct process proc;

        if (!strcmp(cmdline, "CreateProcessNoWait")) wait = output = 0;
        if (!strcmp(cmdline, "CreateProcessWait")) wait = 1, output = 0;
        if (!strcmp(cmdline, "CreateProcessWithOutput")) wait = output = 1;
        free(cmdline); cmdline = NULL;
        cmdline = do_fetch_line(sock, ctx);
        
        proc = start_process(cmdline, wait, output);
        if (proc.error) {
            do_doit_send_str(sock, ctx, proc.error);
            goto done;
        }
        if (wait) {
            int err;
            if (output) {
                char buf[256];
                int len;
                while ((len = read_process_output(proc, buf+1,
                                                  sizeof(buf)-1)) > 0) {
                    buf[0] = len;
                    do_doit_send(sock, ctx, buf, len+1);
                }
                do_doit_send(sock, ctx, "\0", 1);
            }
            err = process_exit_code(proc);
            if (err < 0) {
                do_doit_send_str(sock, ctx, "-GetExitCodeProcess failed\n");
            } else {
                char buf[32];
                sprintf(buf, "+%d\n", err);
                do_doit_send_str(sock, ctx, buf);
            }
        } else {
            do_doit_send_str(sock, ctx, "+\n");
        }
    }

    done:
    if (nonce)
        free(nonce);
    if (cmdline)
        free(cmdline);
    if (ctx)
        /* FIXME: free ctx */;
    closesocket(sock);
    return 0;
}

/*
 * Export the function that gets the command line.
 */
void listener_cmdline(char *cmdline) {
    FILE *fp;

    fp = fopen(cmdline, "rb");
    if (!fp) {
        secretlen = 0;
        secret = "";
        return;
    }
    fseek(fp, 0, SEEK_END);
    secretlen = ftell(fp);
    secret = malloc(secretlen);
    if (!secret) {
        secretlen = 0;
        secret = "";
    }
    fseek(fp, 0, SEEK_SET);
    fread(secret, 1, secretlen, fp);
    fclose(fp);
}

/* ======================================================================
 * System tray functions.
 */

#define WM_XUSER     (WM_USER + 0x2000)
#define WM_SYSTRAY   (WM_XUSER + 6)
#define WM_SYSTRAY2  (WM_XUSER + 7)
#define IDM_CLOSE    0x0010

static HMENU systray_menu;

extern int listener_init(void) {
    BOOL res;
    NOTIFYICONDATA tnid;
    HICON hicon;

#ifdef NIM_SETVERSION
    tnid.uVersion = 0;
    res = Shell_NotifyIcon(NIM_SETVERSION, &tnid);
#endif

    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = listener_hwnd; 
    tnid.uID = 1;                      /* unique within this systray use */
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hicon = LoadIcon (listener_instance, MAKEINTRESOURCE(201));
    strcpy(tnid.szTip, "DoIt (remote-execution daemon)");

    res = Shell_NotifyIcon(NIM_ADD, &tnid); 

    if (hicon) 
        DestroyIcon(hicon); 

    systray_menu = CreatePopupMenu();
    AppendMenu (systray_menu, MF_ENABLED, IDM_CLOSE, "Close DoIt");

    return res; 
}

extern void listener_shutdown(void) {
    BOOL res; 
    NOTIFYICONDATA tnid; 
 
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = listener_hwnd; 
    tnid.uID = 1;

    res = Shell_NotifyIcon(NIM_DELETE, &tnid); 

    DestroyMenu(systray_menu);
}

extern int listener_wndproc(HWND hwnd, UINT message,
                            WPARAM wParam, LPARAM lParam) {
    int ret;
    POINT cursorpos;                   /* cursor position */
    static int menuinprogress;

    if (message == WM_SYSTRAY && lParam == WM_RBUTTONUP) {
        GetCursorPos(&cursorpos);
        PostMessage(hwnd, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
    }

    if (message == WM_SYSTRAY2) {
        if (!menuinprogress) {
            menuinprogress = 1;
            SetForegroundWindow(hwnd);
            ret = TrackPopupMenu(systray_menu,
                                 TPM_RIGHTALIGN | TPM_BOTTOMALIGN |
                                 TPM_RIGHTBUTTON,
                                 wParam, lParam, 0, listener_hwnd, NULL);
            menuinprogress = 0;
        }
    }

    if (message == WM_COMMAND && (wParam & ~0xF) == IDM_CLOSE) {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
    }
    return 1;                          /* not handled */
}
