#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <string.h>

/*
 * Export the application name.
 */
char const *listener_appname = "DoIt";

/*
 * Export the list of ports to listen on.
 */
static int const port_array[] = { 6666 };
int listener_nports = sizeof(port_array) / sizeof(*port_array);
int const *listener_ports = port_array;

/*
 * Export the function that handles a connection.
 */
int listener_newthread(SOCKET sock, int port, SOCKADDR_IN remoteaddr) {
    char *cmdline = NULL;
    int cmdlen = 0, cmdsize = 0;
    char buf[64];
    int len, newlen;

    while (1) {
        len = recv(sock, buf, sizeof(buf), 0);
        if (cmdsize < cmdlen + len + 1) {
            cmdsize = cmdlen + len + 1 + 256;
            cmdline = realloc(cmdline, cmdsize);
            if (!cmdline) {
                closesocket(sock);
                return 0;
            }
        }
        memcpy(cmdline+cmdlen, buf, len);
        cmdline[cmdlen+len] = '\0';
        newlen = strcspn(buf, "\r\n");
        if (newlen < len) {
            buf[newlen] = '\0';
            break;
        }
    }
    MessageBox(NULL, buf, "Message!", MB_OK);
    send(sock, "+ok\r\n", 5, 0);
    closesocket(sock);
    return 0;
}

/*
 * Export the function that gets the command line.
 */
void listener_cmdline(char *cmdline) {
}
