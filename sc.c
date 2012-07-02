/*
 * sc - an 'nc' clone, with robust EOF handling in both directions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>

const char helpmsg[] =
    "usage: sc [ options ] host port\n"
    "   or: sc [ options ] -l port\n"
    "   or: sc [ options ] -U path\n"
    "where: -l                listen instead of connecting (just give port)\n"
    "       -U                Unix-domain socket (give socket pathname)\n"
    "       -u                UDP or Unix datagram socket\n"
    " also: sc --version      report version number\n"
    "       sc --help         display this help text\n"
    "       sc --licence      display the (MIT) licence text\n"
    ;

void version(void) {
    char *p, *q;

#define SVN_REV "$Revision: 7317 $"
    char rev[sizeof(SVN_REV)];
    strcpy(rev, SVN_REV);
#undef SVN_REV

    for (p = rev; *p && *p != ':'; p++);
    if (*p) {
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        for (q = p; *q && !isspace((unsigned char)*q) && *q != '$'; q++);
        if (*q) *q = '\0';
        printf("sc revision %s\n", p);
    } else {
        printf("sc: unknown version\n");
    }
}

const char licencemsg[] =
    "sc is copyright 2011 Simon Tatham.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person\n"
    "obtaining a copy of this software and associated documentation files\n"
    "(the \"Software\"), to deal in the Software without restriction,\n"
    "including without limitation the rights to use, copy, modify, merge,\n"
    "publish, distribute, sublicense, and/or sell copies of the Software,\n"
    "and to permit persons to whom the Software is furnished to do so,\n"
    "subject to the following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall be\n"
    "included in all copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS\n"
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"
    "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
    "SOFTWARE.\n"
    ;

int make_socket(int listening, struct sockaddr *addr, socklen_t addrlen,
                int family, int socktype, int protocol, int report_errors)
{
    int fd;

    fd = socket(family, socktype, protocol);
    if (fd < 0) {
        if (report_errors)
            fprintf(stderr, "sc: socket: %s\n", strerror(errno));
        return -1;
    }

    if (listening) {
        struct sockaddr_storage peeraddr;
        socklen_t peerlen = sizeof(peeraddr);
        int fd2;

        if (bind(fd, addr, addrlen) < 0) {
            if (report_errors)
                fprintf(stderr, "sc: bind: %s\n", strerror(errno));
            return -1;
        }
        if (listen(fd, 1) < 0) {
            if (report_errors)
                fprintf(stderr, "sc: listen: %s\n", strerror(errno));
            return -1;
        }
        fd2 = accept(fd, (struct sockaddr *)&peeraddr, &peerlen);
        if (fd2 < 0) {
            if (report_errors)
                fprintf(stderr, "sc: listen: %s\n", strerror(errno));
            return -1;
        }
        close(fd);
        return fd2;
    } else {
        if (connect(fd, addr, addrlen) < 0) {
            if (report_errors)
                fprintf(stderr, "sc: connect: %s\n", strerror(errno));
            return -1;
        }
        return fd;
    }
}

int main(int argc, char **argv)
{
    int doing_args = 1;
    int nargs = 0;
    char **argsout, **args;

    int listening = 0, unixdomain = 0, datagram = 0;
    char *host;
    char *port;
    char *pathname;

    int fd;

    char sendbuf[32768], recvbuf[32768];
    size_t sendlen, recvlen;
    int sendeof, recveof;

    args = argsout = argv + 1;

    while (--argc > 0) {
        char *arg = *++argv;

        if (doing_args && arg[0] == '-') {
            /*
             * Option.
             */
            if (arg[1] == '-') {
                char *val;

                /*
                 * GNU-style long option, or '--' option terminator.
                 */
                if (!arg[2]) {
                    doing_args = 0;
                    continue;
                }

                val = strchr(arg, '=');
                if (val)
                    *val++ = '\0';

                if (!strcmp(arg, "--help")) {
                    fputs(helpmsg, stdout);
                    return 0;
                } else if (!strcmp(arg, "--version")) {
                    version();
                    return 0;
                } else if (!strcmp(arg, "--licence")) {
                    fputs(licencemsg, stdout);
                    return 0;
                } else {
                    fprintf(stderr, "sc: unrecognised option '%s'\n", arg);
                    return 1;
                }
            } else {
                char c;

                arg++;

                while ((c = *arg++) != '\0') {
                    switch (c) {
                      case 'l':
                        listening = 1;
                        break;
                      case 'U':
                        unixdomain = 1;
                        break;
                      case 'u':
                        datagram = 1;
                        break;
                      default:
                        fprintf(stderr, "sc: unrecognised option '-%c'\n", c);
                        return 1;
                    }
                }
            }
        } else {
            nargs++;
            *argsout++ = arg;
        }
    }

    if (unixdomain) {
        if (nargs < 1) {
            fprintf(stderr, "sc: expected Unix socket pathname after -U\n");
            return 1;
        }
        pathname = (nargs--, *args++);
    } else if (listening) {
        if (nargs < 1) {
            fprintf(stderr, "sc: expected port number after -l\n");
            return 1;
        }
        host = NULL;
        port = (nargs--, *args++);
    } else {
        if (nargs < 2) {
            fprintf(stderr, "sc: expected host name and port number\n");
            return 1;
        }
        host = (nargs--, *args++);
        port = (nargs--, *args++);
    }

    if (nargs > 0) {
        /*
         * FIXME: it would be nice at this point to fork off a
         * subprocess connected to the other end of our stdin/stdout
         * which runs the provided command.
         */
        fprintf(stderr, "sc: unexpected extra arguments\n");
        return 1;
    }

    if (unixdomain) {
        struct sockaddr_un uaddr;

        uaddr.sun_family = AF_UNIX;
        strncpy(uaddr.sun_path, pathname, sizeof(uaddr.sun_path));

        fd = make_socket(listening, (struct sockaddr *)&uaddr,
                         sizeof(uaddr), PF_UNIX,
                         (datagram ? SOCK_DGRAM : SOCK_STREAM), 0, 1);
        if (fd < 0)
            return 1;
    } else {
        struct addrinfo hints;
        struct addrinfo *addrs, *ai;
        int ret;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = (datagram ? SOCK_DGRAM : SOCK_STREAM);
        hints.ai_protocol = 0;
        if (!host)
            hints.ai_flags = AI_PASSIVE;
        else
            hints.ai_flags = 0;

        ret = getaddrinfo(host, port, &hints, &addrs);
        if (ret) {
            fprintf(stderr, "sc: getaddrinfo: %s\n", gai_strerror(ret));
            return 1;
        }

        for (ai = addrs; ai; ai = ai->ai_next) {
            fd = make_socket(listening, ai->ai_addr, ai->ai_addrlen,
                             ai->ai_family, ai->ai_socktype, ai->ai_protocol,
                             (ai->ai_next ? 0 : 1));
            if (fd >= 0)
                break;
        }

        if (fd < 0)
            return 1;

        freeaddrinfo(addrs);
    }

    sendlen = recvlen = 0;
    sendeof = recveof = 0;
    while (!sendeof || !recveof || sendlen > 0 || recvlen > 0) {
        fd_set rfds, wfds;
        int maxfd;
        int ret;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        maxfd = 0;

#define addfd(fd, set, max) do                  \
        {                                       \
            FD_SET((fd), &(set));               \
            if (maxfd < (fd)+1)                 \
                maxfd = (fd)+1;                 \
        } while (0)

        if (sendlen > 0)
            addfd(fd, wfds, maxfd);
        if (!sendeof && sendlen < sizeof(sendbuf))
            addfd(0, rfds, maxfd);
        if (recvlen > 0)
            addfd(1, wfds, maxfd);
        if (!recveof && recvlen < sizeof(recvbuf))
            addfd(fd, rfds, maxfd);

#undef addfd

        ret = select(maxfd, &rfds, &wfds, NULL, NULL);

        if (ret < 0) {
            fprintf(stderr, "sc: select: %s\n", strerror(errno));
            return 1;
        }

        if (FD_ISSET(fd, &wfds)) {
            ret = write(fd, sendbuf, sendlen);
            if (ret < 0) {
                fprintf(stderr, "sc: write: %s\n", strerror(errno));
                return 1;
            }
            assert(sendlen > 0 && sendlen <= (size_t)ret);
            sendlen -= ret;
            memmove(sendbuf, sendbuf + ret, sendlen);
        }

        if (FD_ISSET(1, &wfds)) {
            ret = write(1, recvbuf, recvlen);
            if (ret < 0) {
                fprintf(stderr, "sc: write: %s\n", strerror(errno));
                return 1;
            }
            assert(recvlen > 0 && recvlen <= (size_t)ret);
            recvlen -= ret;
            memmove(recvbuf, recvbuf + ret, recvlen);
        }

        if (FD_ISSET(fd, &rfds)) {
            ret = read(fd, recvbuf + recvlen, sizeof(recvbuf) - recvlen);
            if (ret < 0) {
                fprintf(stderr, "sc: read: %s\n", strerror(errno));
                return 1;
            } else if (ret == 0) {
                recveof = 1;
            } else {
                recvlen += ret;
            }
        }

        if (FD_ISSET(0, &rfds)) {
            ret = read(0, sendbuf + sendlen, sizeof(sendbuf) - sendlen);
            if (ret < 0) {
                fprintf(stderr, "sc: read: %s\n", strerror(errno));
                return 1;
            } else if (ret == 0) {
                sendeof = 1;
            } else {
                sendlen += ret;
            }
        }

        if (recveof == 1 && recvlen == 0) {
            close(1);
            recveof = 2;               /* indicate that we've now done it */
        }
        if (sendeof == 1 && sendlen == 0) {
            shutdown(fd, SHUT_WR);
            sendeof = 2;               /* indicate that we've now done it */
        }
    }

    return 0;
}
