#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <netdb.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/un.h>

#define lenof(x) (sizeof((x)) / sizeof(*(x)))

/* ----------------------------------------------------------------------
 * JavaScript-based URL rewriting mechanism (needs libjs0)
 */

#include <js.h>

JSInterpPtr jsinterpreter = NULL;

void init_url_rewriter(char *cfgfile)
{
    JSInterpOptions options;

    js_init_default_options(&options);
    jsinterpreter = js_create_interp(&options);

    if (!js_eval_file(jsinterpreter, cfgfile)) {
	fprintf(stderr, "ick-proxy: load '%s': %s\n", cfgfile,
		js_error_message(jsinterpreter));
	exit(1);
    }
}

char url_rewrite_error;		       /* has a magic address */

char *url_rewrite(char *url)
{
    JSType input, output;
    char *ret;

    if (!jsinterpreter)
	return NULL;

    js_type_make_string(jsinterpreter, &input, url, strlen(url));

    if (!js_apply(jsinterpreter, "RewriteURL", 1, &input))
	return &url_rewrite_error;

    js_result(jsinterpreter, &output);

    if (output.type != JS_TYPE_STRING)
	return &url_rewrite_error;

    /*
     * Return NULL if the URL hasn't changed
     */
    if (strlen(url) == output.u.s->len &&
	!memcmp(url, output.u.s->data, output.u.s->len))
	return NULL;

    /*
     * Otherwise, return the rewritten URL.
     */
    ret = malloc(1 + output.u.s->len);
    ret[output.u.s->len] = '\0';
    memcpy(ret, output.u.s->data, output.u.s->len);
    return ret;
}

/* 
 * End of JS-based stuff.
 * ----------------------------------------------------------------------
 */

#define FD_SET_MAX(fd, set, max) \
        do { FD_SET((fd),(set)); (max) = ((max)<=(fd)?(fd)+1:(max)); } while(0)

static int signalpipe[2];

void sigchld(int sig)
{
    write(signalpipe[1], "x", 1);
}

int debugging;
FILE *debugfp;

enum { UNIXFD };

struct Connection {
    struct Connection *next, *prev, *nextinline;
    enum {
        INITIAL, AWAITBLANKLINE, CLOSING
    } state;
    char *pending_cmd;
    int multiline, save_stat, unixfd_eof;
    int frozen;
    char *url;                         /* dynamically allocated */
    char *response;                    /* not dynamically allocated */
    struct fd {
        int fd;
        char *buf;
        int buflen, bufsize;
    } fds[1];
};

char *statenames[] = {
    "INITIAL", "AWAITBLANKLINE", "CLOSING"
};

#define destroy(c) do { \
    close(c->fds[UNIXFD].fd); \
    if ((c)->prev) (c)->prev->next = (c)->next; else chead = (c)->next; \
    if ((c)->next) (c)->next->prev = (c)->prev; \
    if ((c)->fds[UNIXFD].buf) free((c)->fds[UNIXFD].buf); \
    if ((c)->pending_cmd) free((c)->pending_cmd); \
    if ((c)->url) free((c)->url); \
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

int strprefix(char *str, char *pfx)
{
    return !strncmp(str, pfx, strlen(pfx));
}

void got_line(struct Connection *c, int fdnum, char *line, int linelen)
{
    char *url, *oldurl, *newurl;
    int urllen;
    int spare = 100;

    if (debugging) {
        fprintf(debugfp, "%s %s\n", fdnum == UNIXFD ? ">--" : "--<", line);
    }

    switch (c->state) {
      case INITIAL:
        while (*line && !isspace((unsigned char)*line)) line++;
        while (*line && isspace((unsigned char)*line)) line++;
        if (!*line) {
            c->response = "501 ick-proxy recognises no command without"
                " following URL";
            break;
        }
        url = line;
        while (*line && !isspace((unsigned char)*line)) line++;
        urllen = line - url;

        oldurl = malloc(urllen + 1);
        if (!oldurl) {
            c->response = "500 ick-proxy memory allocation failed";
            break;
        }

        memcpy(oldurl, url, urllen);
        oldurl[urllen] = '\0';

	newurl = url_rewrite(oldurl);

	if (!newurl) {
            c->response = "501 ick-proxy did not need to rewrite this URL";
	} else if (newurl == &url_rewrite_error) {
            c->response = "500 ick-proxy URL rewriting function failed";
	} else {
            c->response = "302 ick-proxy redirection";
	}

	if (c->response[0] == '3') {
	    c->url = newurl;
        } else {
	    free(newurl);
	}
	free(oldurl);

        c->state = AWAITBLANKLINE;
        break;

      case AWAITBLANKLINE:
        if (*line)
            break;
        sendf(c, UNIXFD, "HTTP/1.1 %s", c->response);
        {
            time_t t;
            struct tm tm;
            char timebuf[128];

            t = time(NULL);
            tm = *gmtime(&t);
            strftime(timebuf, lenof(timebuf),
                     "%a, %d %b %Y %H:%M:%S GMT", &tm);
            sendf(c, UNIXFD, "Date: %s", timebuf);
        }
        sendf(c, UNIXFD, "Server: ick-proxy");
        if (strprefix(c->response, "302"))
            sendf(c, UNIXFD, "Location: %s", c->url);
        sendf(c, UNIXFD, "Connection: close");
        sendf(c, UNIXFD, "Content-Type: text/html; charset=US-ASCII");
        sendf(c, UNIXFD, "");
        /*
         * Now construct an error page.
         */
        sendf(c, UNIXFD, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">");
        sendf(c, UNIXFD, "<HTML><HEAD>");
        sendf(c, UNIXFD, "<TITLE>%s</TITLE>", c->response);
        sendf(c, UNIXFD, "</HEAD><BODY>");
        sendf(c, UNIXFD, "<H1>%s</H1>", c->response);
        if (strprefix(c->response, "302"))
            sendf(c, UNIXFD, "The document has moved <A"
                  " HREF=\"%s\">here</A>.<P>", c->url);
        sendf(c, UNIXFD, "</BODY></HTML>");
        shutdown(c->fds[UNIXFD].fd, SHUT_WR);
        c->state = CLOSING;
        break;

      case CLOSING:
        break;
    }
}

void run_daemon(char *daemon_cmd, int port)
{
    struct sockaddr_in addr;
    int addrlen;
    int master_sock;
    int child_pid = -1;
    struct hostent *h;

    struct Connection *chead = NULL;
    struct Connection *current_open = NULL;

    if (pipe(signalpipe) < 0) {
        fprintf(stderr, "ick-proxy: pipe: %s\n", strerror(errno));
        exit(1);
    }

    master_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (master_sock < 0) {
        fprintf(stderr, "ick-proxy: socket(PF_INET): %s\n", strerror(errno));
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    addrlen = sizeof(addr);
    if (bind(master_sock, (struct sockaddr *)&addr, addrlen) < 0) {
        fprintf(stderr, "ick-proxy: bind: %s\n", strerror(errno));
        exit(1);
    }
    if (listen(master_sock, 5) < 0) {
        fprintf(stderr, "ick-proxy: listen: %s\n", strerror(errno));
        exit(1);
    }

    addrlen = sizeof(addr);
    if (getsockname(master_sock, (struct sockaddr *)&addr, &addrlen)) {
        fprintf(stderr, "ick-proxy: getsockname: %s\n",
                strerror(errno));
        exit(1);
    }

    /*
     * Now either spawn the command (if we've been given a
     * command), or fork ourself off as a daemon (if we haven't).
     */
    if (daemon_cmd) {
        int pid;

        signal(SIGCHLD, sigchld);
        pid = fork();

        if (pid < 0) {
            fprintf(stderr, "ick-proxy: fork: %s\n", strerror(errno));
            exit(1);
        } else if (pid == 0) {
            char buf[40];
            sprintf(buf, "%d", ntohs(addr.sin_port));
            setenv("ICK_PROXY", buf, 1);
            execl("/bin/sh", "sh", "-c", daemon_cmd, NULL);
            fprintf(stderr, "ick-proxy: exec(\"%s\"): %s\n",
                    daemon_cmd, strerror(errno));
            exit(127);                 /* if all else fails */
        }

        /* we only reach here if pid > 0, i.e. we are the parent */
        child_pid = pid;
    } else {
        int pid = fork();

        if (pid < 0) {
            fprintf(stderr, "ick-proxy: fork: %s\n", strerror(errno));
            exit(1);
        } else if (pid > 0) {
            char buf[40];
            sprintf(buf, "%d", ntohs(addr.sin_port));
            if (port == 0)
		printf("ICK_PROXY=%s\n", buf);
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
        FD_SET_MAX(signalpipe[0], &rfds, maxfd);

        for (c = chead; c; c = c->next) {
            if (!c->frozen) {
                FD_SET_MAX(c->fds[UNIXFD].fd, &rfds, maxfd);
            }
        }

        ret = select(maxfd, &rfds, NULL, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(signalpipe[0], &rfds)) {
                int pid, status;

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
                               "-ERR ick-proxy out of memory\r\n");
                        close(subsock);
                        continue;
                    }

                    conn->fds[UNIXFD].fd = subsock;
                    conn->fds[UNIXFD].buf = NULL;
                    conn->fds[UNIXFD].buflen = 0;
                    conn->fds[UNIXFD].bufsize = 0;

                    conn->state = INITIAL;
                    conn->pending_cmd = NULL;
                    conn->save_stat = conn->multiline = conn->unixfd_eof = 0;
                    conn->url = NULL;

                    if (debugging) {
                        fprintf(debugfp, "*** new connection in state %s\n",
                                statenames[conn->state]);
                    }

                    conn->next = chead;
                    conn->prev = NULL;
                    conn->frozen = 0;
                    if (chead) chead->prev = conn;
                    chead = conn;
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
                for (i = 0; i < lenof(c->fds); i++) {
                    if (FD_ISSET(c->fds[i].fd, &rfds)) {
                        char buf[4096];
                        int ret;

                        ret = read(c->fds[i].fd, buf, sizeof(buf));

                        if (ret < 0) {
                            destroy(c);
                            goto nextconn;
                        }

                        if (ret == 0) {
                            /*
                             * EOF.
                             */
                            if (debugging)
                                fprintf(debugfp, "*** client closed"
                                        " connection\n");
                            destroy(c);
                            goto nextconn;
                        }

                        if (c->fds[i].buflen + ret > c->fds[i].bufsize) {
                            c->fds[i].bufsize = c->fds[i].buflen + ret + 4096;
                            c->fds[i].buf = realloc(c->fds[i].buf,
                                                    c->fds[i].bufsize);
                            if (c->fds[i].buf == NULL) {
                                sendf(c, UNIXFD,
                                       "550 ick-proxy out of memory");   /* FIXME */
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
                i = UNIXFD;            /* there is only one */
                while (1) {
                    int linelen;
                    char *p;

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

                nextconn:;
            }
        } else if (ret < 0 && errno == EINTR) {
            continue;
        } else {
            fprintf(stderr, "ick-proxy: select: %s\n", strerror(errno));
            exit(1);
        }
    }
}

void help(void)
{
    fprintf(stderr, "FIXME\n");
}

int main(int argc, char **argv)
{
    char *daemon_cmd = NULL;
    char *cfgfile = NULL;
    char *testurl = NULL;
    int doing_opts = 1;
    int port = 0;

    /*
     * Parse the arguments.
     */
    while (--argc > 0) {
        char *p = *++argv;
        if (*p == '-' && doing_opts) {
            if (!strcmp(p, "--debug")) {
                debugfp = fdopen(3, "w");
                if (!debugfp) {
                    fprintf(stderr, "ick-proxy: unable to access fd 3 for "
                            "debugging: %s\n", strerror(errno));
                    return 1;
                } else {
                    debugging = 1;
                    setlinebuf(debugfp);
                }
            } else if (!strcmp(p, "-p")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -p expected a parameter\n");
                    return 1;
                }
                port = atoi(*++argv);
            } else if (!strcmp(p, "-c")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -c expected a parameter\n");
                    return 1;
                }
		cfgfile = *++argv;
            } else if (!strcmp(p, "-t")) {
                if (--argc <= 0) {
                    fprintf(stderr, "ick-proxy: -t expected a parameter\n");
                    return 1;
                }
		testurl = *++argv;
            } else if (!strcmp(p, "--help") || !strcmp(p, "-help") ||
                       !strcmp(p, "-?") || !strcmp(p, "-h")) {
                /* Valid help request. */
                help();
                return 0;
            } else if (!strcmp(p, "--")) {
                doing_opts = 0;
            } else {
                /* Invalid option; give help as well as whining. */
                fprintf(stderr, "ick-proxy: unrecognised option '%s'\n", p);
                help();
                return 1;
            }
        } else {
            daemon_cmd = p;
        }
    }

    if (cfgfile)
	init_url_rewriter(cfgfile);

    if (testurl) {
	char *ret = url_rewrite(testurl);

	if (!ret)
	    printf("no rewrite\n");
	else if (ret == &url_rewrite_error)
	    printf("error rewriting URL\n");
	else
	    printf("%s\n", ret);
    } else {
	run_daemon(daemon_cmd, port);
    }

    return 0;
}
