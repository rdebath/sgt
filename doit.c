#include <windows.h>
#include <winsock.h>

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
    char msg[160];
    char c;
    sprintf(msg, "hello, %s:%d on my port %d\n",
            inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port), port);
    send(sock, msg, strlen(msg), 0);
    recv(sock, &c, 1, 0);
    closesocket(sock);
    return 0;
}
