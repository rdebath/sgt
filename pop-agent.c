/*
 * pop-agent: an agent for caching POP-3 (RFC 1939) credentials.
 * 
 * TODO: We could plausibly introduce cmdline options which
 * overrode bits of the config file.
 * 
 * TODO: Mailbox lock contention is an interesting problem. It
 * manifests itself as an error return from PASS, so we currently
 * confuse it with a rejected password. Qpopper reports -ERR
 * [IN-USE]; check the RFC to see if this is standard, and consider
 * some alternative handling for it?
 */

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <netdb.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/un.h>

#define FD_SET_MAX(fd, set, max) \
        do { FD_SET((fd),(set)); (max) = ((max)<=(fd)?(fd)+1:(max)); } while(0)

char *pop_hostname, *username, *askpass;
int pop_port, timeout;

struct in_addr pop_addr;

#define PASSWORD_MAX_LEN 256
int got_password;
char password[PASSWORD_MAX_LEN+1];

#define STAT_MAX_LEN 4096
char statdata[STAT_MAX_LEN];

static sig_atomic_t seen_sigchld;

void sigchld(int sig)
{
    seen_sigchld++;
}

static char dirname[L_tmpnam];
static char sockname[L_tmpnam + 50];

int must_cleanup;

void cleanup(void)
{
    unlink(sockname);
    rmdir(dirname);
}

int debugging;
FILE *debugfp;

enum { UNIXFD, INETFD };

struct Connection {
    struct Connection *next, *prev;
    enum {
        INITIAL, SENT_USER, SENT_PASS,
        UNAUTH, UNAUTH_SENT_USER, UNAUTH_WANT_PASS, UNAUTH_SENT_PASS,
        SESSION_CMD, SESSION_RESP, SESSION_ML_RESP, QUIT
    } state;
    char *pending_cmd;
    int multiline, save_stat, unixfd_eof;
    struct fd {
        int fd;
        char *buf;
        int buflen, bufsize;
    } fds[2];
};

char *statenames[] = {
    "INITIAL", "SENT_USER", "SENT_PASS",
    "UNAUTH", "UNAUTH_SENT_USER", "UNAUTH_WANT_PASS", "UNAUTH_SENT_PASS",
    "SESSION_CMD", "SESSION_RESP", "SESSION_ML_RESP", "QUIT"
};

#define uses_inetfd(state) ( \
    (state)==INITIAL || (state)==SENT_USER || (state)==SENT_PASS || \
    (state)==UNAUTH_SENT_USER || (state)==UNAUTH_SENT_PASS || \
    (state)==SESSION_RESP || (state)==SESSION_ML_RESP || (state)==QUIT)
#define uses_unixfd(state) (!uses_inetfd((state)))
#define which_fd(state) (uses_inetfd((state)) ? INETFD : UNIXFD)

#define destroy(c) do { \
    close(c->fds[UNIXFD].fd); \
    close(c->fds[INETFD].fd); \
    if ((c)->prev) (c)->prev->next = (c)->next; else chead = (c)->next; \
    if ((c)->next) (c)->next->prev = (c)->prev; \
    if ((c)->fds[UNIXFD].buf) free((c)->fds[UNIXFD].buf); \
    if ((c)->fds[INETFD].buf) free((c)->fds[INETFD].buf); \
    if ((c)->pending_cmd) free((c)->pending_cmd); \
    if (debugging) fprintf(debugfp, "*** connection terminated\n"); \
    free(c); \
} while (0)

int writef(int fd, char *fmt, ...)
{
    char buf[4096];
    int ret, len;
    char *p;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-3, fmt, ap);
    buf[sizeof(buf)-2] = '\0';
    va_end(ap);

    len = strlen(buf);
    p = buf;
    while (len > 0) {
        ret = write(fd, p, len);
        if (ret <= 0)
            return ret;
        p += ret;
        len -= ret;
    }
    return p - buf;
}

int sendf(struct Connection *c, int whichfd, char *fmt, ...)
{
    char buf[4096];
    int ret, len;
    int fd = c->fds[whichfd].fd;
    char *p;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-3, fmt, ap);
    buf[sizeof(buf)-2] = '\0';
    va_end(ap);

    if (debugging) {
        fprintf(debugfp, "%s %s\n", whichfd == UNIXFD ? "<--" : "-->", buf);
    }

    strcat(buf, "\r\n");

    len = strlen(buf);
    p = buf;
    while (len > 0) {
        ret = write(fd, p, len);
        if (ret <= 0)
            return ret;
        p += ret;
        len -= ret;
    }
    return p - buf;
}

void send_cmd(struct Connection *c, char *cmd)
{
    if (!strcasecmp(cmd, "MC_OSTAT")) {
        sendf(c, UNIXFD, "+OK %s", statdata);
        c->state = SESSION_CMD;        /* this needs no server response */
    } else if (!strcasecmp(cmd, "MC_NSTAT")) {
        sendf(c, INETFD, "STAT");
        c->save_stat = 1;
    } else {
        sendf(c, INETFD, "%s", cmd);
        
        /*
         * List of multiline commands. Each of these commands
         * expects either a +OK followed by further lines, or a
         * -ERR followed by nothing further.
         * 
         * Note that LIST and UIDL are only multiline when they
         * have no arguments; RETR and TOP are multiline always.
         */
        if (!strcasecmp(cmd, "LIST") ||
            !strcasecmp(cmd, "UIDL") ||
            !strncasecmp(cmd, "RETR", 4) ||
            !strncasecmp(cmd, "TOP", 3))
            c->multiline = 1;
    }
}

void got_line(struct Connection *c, int fdnum, char *line, int linelen)
{

    if (debugging) {
        fprintf(debugfp, "%s %s\n", fdnum == UNIXFD ? ">--" : "--<", line);
    }

    switch (c->state) {

        /*
         * State INITIAL. We expect to see the POP greeting; if we
         * already have a POP password, we should then begin
         * authentication with the server _before_ sending a
         * greeting on to the client. Otherwise, we send a greeting
         * that makes it clear we have no authentication yet.
         * 
         * If we see a negative greeting, we pass it straight
         * through, stay in state INITIAL (so we keep reading from
         * inetfd) and the connection will presumably close shortly
         * after that.
         */
      case INITIAL: assert(fdnum == INETFD);
        if (!strncmp(line, "+OK", 3)) {
            if (!got_password) {
                sendf(c, UNIXFD, "+OK pop-agent unauthenticated");
                c->state = UNAUTH;
            } else {
                sendf(c, INETFD, "USER %s", username);
                c->state = SENT_USER;
            }
        } else
            sendf(c, UNIXFD, "%s", line);
        break;

        /*
         * State SENT_USER. We are performing automated
         * authentication, we've sent USER, and we expect to see a
         * response. If we get a success response, we send PASS.
         * If it's a failure response:
         * 
         *  - if the user was expecting a reply to a command they
         *    sent, we pass the failure response straight to the
         *    user.
         *  - if this is at connection startup and the user was
         *    expecting a greet, we send a greet that explains what
         *    happened and delete our internal auth details.
         * 
         * Either way, we go to UNAUTH state and wait for the user
         * to make the next move.
         */
      case SENT_USER: assert(fdnum == INETFD);
        if (!strncmp(line, "+OK", 3)) {
            assert(got_password);
            sendf(c, INETFD, "PASS %s", password);
            c->state = SENT_PASS;
        } else {
            if (c->pending_cmd) {
                free(c->pending_cmd);
                c->pending_cmd = NULL;
                sendf(c, UNIXFD, "-ERR automatic authentication was"
                       " rejected at USER");
            } else {
                sendf(c, UNIXFD, "+OK but pop-agent authentication"
                       " discarded! (USER failed)");
            }
            got_password = 0;
            c->state = UNAUTH;
        }
        break;

        /*
         * State SENT_USER. We are performing automated
         * authentication, we've sent PASS, and we expect to see a
         * response. If we get a success response, we either pass a
         * cheerful greet to the user and enter state SESSION_CMD
         * (if this is connection startup) or send their stored
         * command and enter state SESSION_RESP (if they'd
         * previously sent a command in state UNAUTH).
         * 
         * If it's a failure response:
         * 
         *  - if the user was expecting a reply to a command they
         *    sent, we pass the failure response straight to the
         *    user.
         *  - if this is at connection startup and the user was
         *    expecting a greet, we send a greet that explains what
         *    happened and delete our internal auth details.
         * 
         * Either way, we go to UNAUTH state and wait for the user
         * to make the next move.
         */
      case SENT_PASS: assert(fdnum == INETFD);
        if (!strncmp(line, "+OK", 3)) {
            if (c->pending_cmd) {
                c->state = SESSION_RESP;
                send_cmd(c, c->pending_cmd);
            } else {
                sendf(c, UNIXFD,
                       "+OK pop-agent authenticated and ready");
                c->state = SESSION_CMD;
            }
        } else {
            if (c->pending_cmd) {
                free(c->pending_cmd);
                c->pending_cmd = NULL;
                sendf(c, UNIXFD, "-ERR automatic authentication was"
                       " rejected at PASS");
            } else {
                sendf(c, UNIXFD, "+OK but pop-agent authentication"
                       " discarded! (PASS failed)");
            }
            got_password = 0;
            c->state = UNAUTH;
        }
        break;

        /*
         * States UNAUTH and UNAUTH_WANT_PASS: we are
         * unauthenticated and waiting for a user command. For most
         * commands, we request out-of-line authentication via
         * askpass, save the command until the authentication
         * exchange is complete, send USER, and go to state
         * SENT_USER to wait for a response.
         * 
         * One exception: QUIT is passed straight through and we
         * enter state QUIT, in which we pass data through from the
         * server until it closes the connection.
         * 
         * We also allow the user to send USER and PASS. In state
         * UNAUTH, we're expecting USER (and we know what username
         * it should be) so we reject PASS; in state
         * UNAUTH_WANT_PASS we expect PASS and hence reject USER.
         * If we receive the right one of these commands, we enter
         * state UNAUTH_SENT_USER or UNAUTH_SENT_PASS and wait for
         * a server response.
         */
      case UNAUTH: case UNAUTH_WANT_PASS: assert(fdnum == UNIXFD);
        if (!strncasecmp(line, "USER ", 5)) {
            if (c->state == UNAUTH_WANT_PASS) {
                sendf(c, UNIXFD,
                       "-ERR pop-agent expected PASS after USER");
            } else if (!strcmp(line+5, username)) {
                sendf(c, INETFD, "%s", line);
                c->state = UNAUTH_SENT_USER;
            } else {
                sendf(c, UNIXFD, "-ERR this pop-agent expects "
                       "username '%s'", username);
                /* stay in UNAUTH state, having rejected the command */
            }
        } else if (!strncasecmp(line, "PASS ", 5)) {
            if (c->state == UNAUTH) {
                sendf(c, UNIXFD,
                       "-ERR pop-agent expected USER before PASS");
            } else if (strlen(line+5) > PASSWORD_MAX_LEN) {
                sendf(c, UNIXFD, "-ERR pop-agent cannot deal with "
                       "passwords longer than %d", PASSWORD_MAX_LEN);
            } else {
                strcpy(password, line+5);
                sendf(c, INETFD, "%s", line);
                c->state = UNAUTH_SENT_PASS;
            }
        } else if (!strncasecmp(line, "QUIT", 4)) {
            sendf(c, INETFD, "%s", line);
            c->state = QUIT;
        } else {
            if (askpass) {
                FILE *fp;
                char str[4096];

                /*
                 * Defer the command until we finish authenticating.
                 */
                c->pending_cmd = malloc(1+linelen);
                if (!c->pending_cmd) {
                    sendf(c, UNIXFD,
                           "-ERR pop-agent out of memory");
                    return;
                }
                strcpy(c->pending_cmd, line);

                snprintf(str, sizeof(str),
                         "%s 'Enter POP-3 password for %s at %s'",
                         askpass, username, pop_hostname);
                str[sizeof(str)-1] = '\0';
                fp = popen(str, "r");
                if (!fp || !fgets(str, sizeof(str), fp)) {
                    pclose(fp);
                    sendf(c, UNIXFD, "-ERR automatic authentication"
                           " failed (%s)", strerror(errno));
                } else {
                    pclose(fp);
                    /*
                     * We've received a password; now let's send the
                     * username and enter state SENT_USER, in the vague
                     * hope that the password we send will work. If it
                     * doesn't, we'll return -ERR and have another
                     * chance.
                     */
                    str[strcspn(str, "\r\n")] = '\0';
                    strncpy(password, str, PASSWORD_MAX_LEN);
                    password[PASSWORD_MAX_LEN] = '\0';
                    got_password = 1;
                    sendf(c, INETFD, "USER %s", username);
                    c->state = SENT_USER;
                }
            } else {
                sendf(c, UNIXFD, "-ERR authentication required");
                /* command rejected; stay in UNAUTH state */
            }
        }
        break;

        /*
         * State QUIT: throw server data through to the client
         * until there isn't any any more.
         */
      case QUIT: assert(fdnum == INETFD);
        sendf(c, UNIXFD, "%s", line);
        break;

        /*
         * State UNAUTH_SENT_USER: the user has supplied a USER
         * command which we've sent on. We expect to see a
         * response, which we will copy straight to the user
         * whatever it is. If it's positive, we now enter state
         * UNAUTH_WANT_PASS; otherwise we go back to UNAUTH.
         */
      case UNAUTH_SENT_USER: assert(fdnum == INETFD);
        sendf(c, UNIXFD, "%s", line);        
        if (!strncmp(line, "+OK", 3))
            c->state = UNAUTH_WANT_PASS;
        else
            c->state = UNAUTH;
        break;

        /*
         * State UNAUTH_SENT_PASS: the user has supplied a PASS
         * command which we've sent on. We expect to see a
         * response, which we will copy straight to the user
         * whatever it is. If it's positive, we now enter state
         * SESSION_CMD; otherwise we go back to UNAUTH.
         */
      case UNAUTH_SENT_PASS: assert(fdnum == INETFD);
        if (!strncmp(line, "+OK", 3)) {
            /* Prefix our own thanks on to the response. */
            sendf(c, UNIXFD, "+OK [pop-agent says thanks]%s",
                   line+3);
            c->state = SESSION_CMD;
        } else {
            sendf(c, UNIXFD, "%s", line);
            c->state = UNAUTH;
        }
        break;

        /*
         * State SESSION_CMD. We are now fully authenticated and
         * ready to gateway real POP commands. If the client sends
         * USER or PASS (not knowing they've been done for it), we
         * politely ignore them; otherwise we pass them through and
         * enter state SESSION_RESP, expecting a response from the
         * server.
         */
      case SESSION_CMD: assert(fdnum == UNIXFD);
        if (!strncasecmp(line, "USER ", 5) ||
            !strncasecmp(line, "PASS ", 5)) {
            sendf(c, UNIXFD, "+OK that's nice, dear");
        } else {
            c->state = SESSION_RESP;
            send_cmd(c, line);
        }
        break;

        /*
         * State SESSION_RESP. We have sent a POP command and are
         * expecting a status response. If the response is +OK and
         * the command was a multiline one, we enter state
         * SESSION_ML_RESP to process the remaining lines of the
         * response; otherwise we go straight back to SESSION_CMD.
         * 
         * Also, if this is a STAT response and we are meant to be
         * saving it, do so.
         */
      case SESSION_RESP: assert(fdnum == INETFD);

        if (c->multiline && !strncmp(line, "+OK", 3)) {
            c->state = SESSION_ML_RESP;
        } else
            c->state = SESSION_CMD;

        if (c->save_stat && !strncmp(line, "+OK ", 4)) {
            strncpy(statdata, line+4, sizeof(statdata));
            statdata[sizeof(statdata)-1] = '\0';
        }

        c->multiline = 0;
        c->save_stat = 0;

        sendf(c, UNIXFD, "%s", line);
        break;

        /*
         * State SESSION_ML_RESP. We are processing a multi-line
         * response from the server. So whatever we've received
         * gets passed straight to the client, but also if it's a
         * lone dot on a line the multiline response is terminated
         * and we enter state SESSION_CMD again.
         */
      case SESSION_ML_RESP: assert(fdnum == INETFD);
        sendf(c, UNIXFD, "%s", line);
        if (!strcmp(line, "."))
            c->state = SESSION_CMD;
        break;
    }
}

void run_daemon(char *daemon_cmd)
{
    struct sockaddr_un addr;
    int addrlen;
    int master_sock;
    int child_pid = -1;
    int local_seen_sigchld = 0;
    struct hostent *h;

    struct Connection *chead = NULL;

    got_password = 0;
    strcpy(statdata, "0 0");

    /*
     * Clean up the socket on exit.
     */
    must_cleanup = 1;
    atexit(cleanup);

    /*
     * Look up the host name for the real POP server.
     */
    h = gethostbyname(pop_hostname);
    if (h == NULL) {
        fprintf(stderr, "pop-agent: gethostbyname(\"%s\"): %s\n",
                pop_hostname, strerror(errno));
        exit(1);
    }
    memcpy(&pop_addr, h->h_addr, sizeof(pop_addr));

    /*
     * Create our listening socket.
     */
    if (tmpnam(dirname) == NULL) {
        fprintf(stderr, "pop-agent: unable to generate name for socket\n");
        exit(1);
    }
    if (mkdir(dirname, 0700) < 0) {
        fprintf(stderr, "pop-agent: mkdir(\"%s\"): %s\n", dirname, strerror(errno));
        exit(1);
    }

    strcpy(sockname, dirname);
    strcat(sockname, "/socket");

    if (strlen(sockname) + 1 > sizeof(addr.sun_path)) {
        fprintf(stderr, "pop-agent: filename \"%s\" is too long for a Unix socket\n",
                sockname);
        exit(1);
    }

    master_sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (master_sock < 0) {
        fprintf(stderr, "pop-agent: socket(AF_UNIX): %s\n", strerror(errno));
        exit(1);
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockname);
    addrlen = sizeof(addr);
    if (bind(master_sock, (struct sockaddr *)&addr, addrlen) < 0) {
        fprintf(stderr, "pop-agent: bind(\"%s\"): %s\n", sockname, strerror(errno));
        exit(1);
    }
    if (listen(master_sock, 5) < 0) {
        fprintf(stderr, "pop-agent: listen: %s\n", strerror(errno));
        exit(1);
    }

    /*
     * Now either spawn the command (if we've been given a
     * command), or fork ourself off as a daemon (if we haven't).
     */
    seen_sigchld = 0;
    if (daemon_cmd) {
        int pid;

        signal(SIGCHLD, sigchld);
        pid = fork();

        if (pid < 0) {
            fprintf(stderr, "pop-agent: fork: %s\n", strerror(errno));
            exit(1);
        } else if (pid == 0) {
            setenv("POP_AGENT", sockname, 1);
            execl("/bin/sh", "sh", "-c", daemon_cmd, NULL);
            fprintf(stderr, "pop-agent: exec(\"%s\"): %s\n",
                    daemon_cmd, strerror(errno));
            exit(127);                 /* if all else fails */
        }

        /* we only reach here if pid > 0, i.e. we are the parent */
        child_pid = pid;
    } else {
        int pid = fork();

        if (pid < 0) {
            fprintf(stderr, "pop-agent: fork: %s\n", strerror(errno));
            exit(1);
        } else if (pid > 0) {
            printf("POP-AGENT=%s\n", sockname);
            must_cleanup = 0;          /* inhibit removal of socket :-) */
            exit(0);                   /* started daemon successfully */
        }

        /* we only reach here if pid == 0, i.e. we are the child */
    }

    /*
     * Right. Now run round and round in a select loop.
     */
    while (1) {
        fd_set rfds;
        int maxfd, ret;
        struct Connection *c, *next;
        struct timeval tv;

        maxfd = 0;

        FD_ZERO(&rfds);
        FD_SET_MAX(master_sock, &rfds, maxfd);

        for (c = chead; c; c = c->next) {
            if (!c->unixfd_eof)
                FD_SET_MAX(c->fds[UNIXFD].fd, &rfds, maxfd);
            FD_SET_MAX(c->fds[INETFD].fd, &rfds, maxfd);
        }

        if (timeout > 0) {
            tv.tv_usec = 0;
            tv.tv_sec = timeout;
        }

        ret = select(maxfd, &rfds, NULL, NULL, (timeout > 0 ? &tv : NULL));
        if (ret > 0) {
            if (FD_ISSET(master_sock, &rfds)) {
                int subsock;
                addrlen = sizeof(addr);
                subsock = accept(master_sock, (struct sockaddr *)&addr,
                                 &addrlen);
                if (subsock >= 0) {
                    /*
                     * Create a new connection to the server, and
                     * load both fds into a Connection structure.
                     */
                    int inetsock;
                    struct sockaddr_in addr;
                    struct Connection *conn;

                    conn = malloc(sizeof(struct Connection));
                    if (!conn) {
                        writef(subsock,
                               "-ERR pop-agent out of memory\r\n");
                        close(subsock);
                        continue;
                    }

                    inetsock = socket(PF_INET, SOCK_STREAM, 0);
                    if (inetsock < 0) {
                        writef(subsock,
                               "-ERR Unable to create socket: %s\r\n",
                               strerror(errno));
                        free(conn);
                        close(subsock);
                        continue;
                    }

                    addr.sin_family = AF_INET;
                    addr.sin_addr = pop_addr;
                    addr.sin_port = htons(pop_port);
                    if (connect(inetsock, (struct sockaddr *)&addr,
                                sizeof(addr)) < 0) {
                        writef(subsock,
                               "-ERR Unable to connect to %s:%d: %s",
                               pop_hostname, pop_port, strerror(errno));
                        free(conn);
                        close(subsock);
                        continue;
                    }

                    conn->fds[UNIXFD].fd = subsock;
                    conn->fds[UNIXFD].buf = conn->fds[INETFD].buf = NULL;
                    conn->fds[UNIXFD].buflen = conn->fds[INETFD].buflen = 0;
                    conn->fds[UNIXFD].bufsize = conn->fds[INETFD].bufsize = 0;

                    conn->state = INITIAL;
                    conn->pending_cmd = NULL;
                    conn->save_stat = conn->multiline = conn->unixfd_eof = 0;

                    conn->fds[INETFD].fd = inetsock;
                    conn->next = chead;
                    conn->prev = NULL;
                    if (chead) chead->prev = conn;
                    chead = conn;

                    if (debugging) {
                        fprintf(debugfp, "*** new connection in state %s\n",
                                statenames[conn->state]);
                    }
                }
            }
            for (c = chead; c; c = next) {
                int i;

                /*
                 * We might destroy c and remove it from the list
                 * during the course of this loop; so we must get
                 * hold of the next one to visit _right now_ before
                 * anything confusing happens to the list.
                 */
                next = c->next;

                /*
                 * Read input on both fds (in particular this
                 * allows us to notice either side closing the
                 * connection at any time, even when we're in
                 * principle listening to the other side).
                 */
                for (i = 0; i < 2; i++) {
                    if (FD_ISSET(c->fds[i].fd, &rfds)) {
                        char buf[4096];
                        int ret;

                        ret = read(c->fds[i].fd, buf, sizeof(buf));

                        if (ret < 0) {
                            if (i == INETFD)
                                sendf(c, UNIXFD,
                                       "-ERR Read error on POP "
                                       "connection: %s", strerror(errno));
                            destroy(c);
                            goto nextconn;
                        }

                        if (ret == 0) {
                            /*
                             * EOF on one socket or the other. If
                             * this is the inetfd, it should be
                             * sufficient just to close both
                             * sockets and go away (unexpected
                             * closes can happen even when we
                             * thought we were waiting for user
                             * input); if it's the unixfd, we
                             * should stop trying to read the
                             * unixfd, and continue.
                             */
                            if (i == INETFD) {
                                if (debugging)
                                    fprintf(debugfp, "*** server closed"
                                            " connection\n");
                                destroy(c);
                                goto nextconn;
                            } else {
                                if (debugging)
                                    fprintf(debugfp, "*** client closed"
                                            " connection\n");
                                c->unixfd_eof = 1;
                            }
                        }

                        if (c->fds[i].buflen + ret > c->fds[i].bufsize) {
                            c->fds[i].bufsize = c->fds[i].buflen + ret + 4096;
                            c->fds[i].buf = realloc(c->fds[i].buf,
                                                    c->fds[i].bufsize);
                            if (c->fds[i].buf == NULL) {
                                sendf(c, UNIXFD,
                                       "-ERR pop-agent out of memory");
                                destroy(c);
                                goto nextconn;
                            }
                        }

                        memcpy(c->fds[i].buf + c->fds[i].buflen, buf, ret);
                        c->fds[i].buflen += ret;
                    }
                }

                /*
                 * Now we only _process_ input from the fd we're
                 * really listening to. This may change each time
                 * we go round the loop (because got_line is liable
                 * to change the connection's state).
                 */
                while (1) {
                    int linelen;
                    char *p;

                    i = which_fd(c->state);
                    p = memchr(c->fds[i].buf, '\n',
                               c->fds[i].buflen);
                    if (p == NULL)     /* no complete lines available */
                        break;

                    linelen = p - c->fds[i].buf;
                    if (linelen > 0 && p[-1] == '\r')
                        p--;
                    *p = '\0'; /* for convenience of got_line() */
                    got_line(c, i, c->fds[i].buf, p - c->fds[i].buf);
                    if (debugging) {
                        fprintf(debugfp, "*** state is now %s\n",
                                statenames[c->state]);
                    }
                    c->fds[i].buflen -= linelen+1;
                    memmove(c->fds[i].buf,
                            c->fds[i].buf+linelen+1,
                            c->fds[i].buflen);
                }

                /*
                 * If we have now reached a state where we need to
                 * read the unixfd, but we've already received EOF
                 * on it, just close up shop.
                 */
                if (uses_unixfd(c->state) && c->unixfd_eof) {
                    destroy(c);
                }

                nextconn:;
            }
        } else if (ret < 0 && errno == EINTR) {
            int local_seen_sigchld_new;
            int pid, status;

            local_seen_sigchld_new = seen_sigchld;
            if (local_seen_sigchld_new > local_seen_sigchld) {
                local_seen_sigchld = local_seen_sigchld_new;
                pid = wait(&status);
                if (pid > 0 && pid == child_pid &&
                    (WIFEXITED(status) || WIFSIGNALED(status))) {
                    /*
                     * The child process has terminated, so we must
                     * too.
                     */
                    exit(0);
                }
            }
        } else if (ret == 0) {
            /*
             * Timed out. Forget the stored password, and if we
             * have no child process we also terminate.
             */
            memset(password, 0, sizeof(password));
            got_password = 0;
            exit(0);
        }
    }
}

void run_client(void)
{
    int sock;
    struct sockaddr_un addr;
    int addrlen;
    char *sockname;
    int just_seen_r = 0;
    int stdin_eof = 0;

    sockname = getenv("POP_AGENT");
    if (!sockname) {
        fprintf(stderr, "pop-agent: $POP_AGENT not set: unable to find a"
                " running agent\n");
        exit(1);
    }

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "pop-agent: socket(AF_UNIX): %s\n", strerror(errno));
        exit(1);
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockname);
    addrlen = sizeof(addr);
    if (connect(sock, (struct sockaddr *)&addr, addrlen) < 0) {
        fprintf(stderr, "pop-agent: bind(\"%s\"): %s\n", sockname,
                strerror(errno));
        exit(1);
    }

    while (1) {
        fd_set rfds;
        int maxfd, ret;

        maxfd = 0;

        FD_ZERO(&rfds);
        FD_SET_MAX(sock, &rfds, maxfd);
        if (!stdin_eof)
            FD_SET_MAX(0, &rfds, maxfd);

        ret = select(maxfd, &rfds, NULL, NULL, NULL /* FIXME: timeout */);
        if (ret > 0) {
            char buf[2048];
            if (FD_ISSET(sock, &rfds)) {
                ret = read(sock, buf, sizeof(buf));
                if (ret < 0) {
                    fprintf(stderr, "pop-agent: read from socket: %s\n",
                            strerror(errno));
                    exit(1);
                } else if (ret == 0) {
                    exit(0);
                } else {
                    writef(1, "%.*s", ret, buf);
                }
            }
            if (FD_ISSET(0, &rfds)) {
                ret = read(0, buf, sizeof(buf));
                if (ret < 0) {
                    fprintf(stderr, "pop-agent: read from stdin: %s\n",
                            strerror(errno));
                    exit(1);
                } else if (ret == 0) {
                    stdin_eof = 1;
                    shutdown(sock, 1);
                } else {
                    char *p = buf;
                    int len = ret;
                    while (len > 0) {
                        char *n = memchr(p, '\n', len);
                        char *r = memchr(p, '\r', len);
                        char *q;
                        q = (!n ? r : !r ? n : r > n ? n : r);
                        if (q) {
                            if (*q == '\n' && (q > p || !just_seen_r)) {
                                /* \n without \r */
                                writef(sock, "%.*s\r\n", q - p, p);
                            } else {
                                writef(sock, "%.*s", q+1 - p, p);
                            }
                            len -= q+1 - p;
                            p = q+1;
                            just_seen_r = (*q == '\r');
                        } else {
                            writef(sock, "%.*s", len, p);
                            len = 0;
                            just_seen_r = 0;
                        }
                    }
                }
            }
        }
    }

}

void run_password(void)
{
    int sock;
    struct sockaddr_un addr;
    int addrlen;
    char *pw;
    char buffer[4096];
    int i;
    char *sockname;

    sockname = getenv("POP_AGENT");
    if (!sockname) {
        fprintf(stderr, "pop-agent: $POP_AGENT not set: unable to find a"
                " running agent\n");
        exit(1);
    }

    snprintf(buffer, 4096, "Enter POP-3 password for %s at %s: ",
             username, pop_hostname);
    pw = getpass(buffer);

    if (pw == NULL) {
        fprintf(stderr, "Abandoned.\n");
        exit(1);
    }

    if (strlen(pw) > PASSWORD_MAX_LEN) {
        fprintf(stderr, "pop-agent: cannot deal with "
                   "passwords longer than %d\n", PASSWORD_MAX_LEN);
        exit(1);
    }

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "pop-agent: socket(AF_UNIX): %s\n", strerror(errno));
        exit(1);
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockname);
    addrlen = sizeof(addr);
    if (connect(sock, (struct sockaddr *)&addr, addrlen) < 0) {
        fprintf(stderr, "pop-agent: bind(\"%s\"): %s\n", sockname,
                strerror(errno));
        exit(1);
    }

    for (i = 0; i < 3; i++) {
        int ret = read(sock, buffer, sizeof(buffer)-1);
        if (ret > 0) {
            char *p = memchr(buffer, '\n', ret);
            if (!p) {
                char c;
                buffer[ret] = '\0';
                do {
                    ret = read(sock, &c, 1);
                } while (ret == 1 && c != '\n');
            } else {
                if (p > buffer && p[-1] == '\r') p--;
                *p = '\0';
            }
        }
        if (ret <= 0) {
            if (ret < 0)
                fprintf(stderr, "pop-agent: read from socket: %s\n",
                        strerror(errno));
            else
                fprintf(stderr, "pop-agent: unexpected EOF on socket\n");
            exit(1);
        } else if (buffer[0] != '+') {
            fprintf(stderr, "pop-agent: server error: %s\n", buffer+5);
            exit(1);
        }

        if (i == 0)
            writef(sock, "USER %s\r\n", username);
        else if (i == 1)
            writef(sock, "PASS %s\r\n", pw);
        else if (i == 2)
            break;
    }
}

void help(void)
{
    fprintf(stderr,
            "usage:     pop-agent          connect to a running agent\n"
            "           pop-agent -p       place a password into the agent\n"
            "           pop-agent -d cmd   start an agent that lasts for the"
            " lifetime of cmd\n"
            "           pop-agent -d       start agent, giving env"
            " settings on stdout\n"
            );
}

int main(int argc, char **argv)
{
    enum { MODE_DAEMON, MODE_CLIENT, MODE_PASSWORD };

    int mode = MODE_CLIENT;
    char *daemon_cmd = NULL;

    /*
     * Parse the arguments.
     */
    while (--argc > 0) {
        char *p = *++argv;
        if (!strcmp(p, "-p")) {
            mode = MODE_PASSWORD;
        } else if (!strcmp(p, "-debug")) {
            debugfp = fdopen(3, "w");
            if (!debugfp) {
                fprintf(stderr, "pop-agent: unable to access fd 3 for "
                        "debugging: %s\n", strerror(errno));
                return 1;
            } else {
                debugging = 1;
                setlinebuf(debugfp);
            }
        } else if (!strcmp(p, "-d")) {
            if (--argc > 0) {
                daemon_cmd = *++argv;
            }
            mode = MODE_DAEMON;
        } else {
            if (!strcmp(p, "--help") || !strcmp(p, "-help") ||
                !strcmp(p, "-?") || !strcmp(p, "-h")) {
                /* Valid help request. */
                help();
                return 0;
            } else {
                /* Invalid option; give help as well as whining. */
                fprintf(stderr, "pop-agent: unrecognised option '%s'\n", p);
                help();
                return 1;
            }
        }
    }

    /*
     * Set up pre-config-file defaults.
     */
    pop_hostname = askpass = NULL;
    username = getenv("USER");
    pop_port = 110;
    timeout = -1;

    /*
     * Now read the config file.
     */
    {
        char fname[FILENAME_MAX];
        char buf[4096];
        char *var, *val, *copyval;
        snprintf(fname, sizeof(fname), "%s/.pop-agent", getenv("HOME"));
        FILE *fp = fopen(fname, "r");
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            var = buf;
            while (*var && isspace(*var)) var++;
            if (*var == '#') continue;
            val = strchr(buf, ' ');
            if (val) {
                *val++ = '\0';
                while (*val && isspace(*val)) val++;
            } else
                continue;
            copyval = malloc(1+strlen(val));
            if (!copyval) {
                fprintf(stderr, "pop-agent: out of memory\n");
                exit(1);
            }
            strcpy(copyval, val);
            if (!strcasecmp(var, "pophost")) {
                pop_hostname = copyval;
            } else if (!strcasecmp(var, "askpass")) {
                askpass = copyval;
            } else if (!strcasecmp(var, "popport")) {
                free(copyval);
                pop_port = atoi(val);
            } else if (!strcasecmp(var, "timeout")) {
                free(copyval);
                if (strcasecmp(val, "none"))
                    timeout = atoi(val);
                else
                    timeout = -1;
            } else
                free(copyval);
        }
        fclose(fp);
    }

    switch (mode) {
      case MODE_DAEMON:
        run_daemon(daemon_cmd);
        break;
      case MODE_CLIENT:
        run_client();
        break;
      case MODE_PASSWORD:
        run_password();
        break;
    }

    return 0;
}
