/*
 * DoIt client.
 * 
 * TODO: --help and --version and suchlike.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "doit.h"

char *dupstr(char *s)
{
    char *ret = malloc(1+strlen(s));
    if (!ret) {
        perror("malloc");
        exit(1);
    }
    strcpy(ret, s);
    return ret;
}

char *get_pwd(void)
{
    char *buf, *ret;
    int size = FILENAME_MAX;

    buf = malloc(size);
    while ( (ret = getcwd(buf, size)) == NULL && errno == ERANGE) {
        size *= 2;
        buf = realloc(buf, size);
    }
    return buf;
}

void *read_secret_file(char *fname, int *len) {
    FILE *fp;
    void *secret;
    int secretlen;

    fp = fopen(fname, "rb");
    if (!fp) {
        perror(fname);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    secretlen = ftell(fp);
    secret = malloc(secretlen);
    if (!secret) {
        perror("malloc");
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    fread(secret, 1, secretlen, fp);
    fclose(fp);

    *len = secretlen;
    return secret;
}

int make_connection(unsigned long ipaddr, int port)
{
    int sock;
    unsigned long a;
    struct sockaddr_in addr;

#if 0
    struct hostent *h;
    a = inet_addr(host);
    if (a == (unsigned long)-1) {
        h = gethostbyname(host);
        if (!h) {
            perror(host);
            return -1;
        }
        memcpy(&a, h->h_addr, sizeof(a));
    }
#endif
    a = ntohl(ipaddr);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    addr.sin_addr.s_addr = htonl(a);
    addr.sin_port = htons(port);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return -1;
    }

    return sock;
}

int do_send(int sock, void *buffer, int len)
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

int do_receive(int sock, doit_ctx *ctx)
{
    char buf[1024];
    int ret;

    ret = recv(sock, buf, sizeof(buf), 0);
    if (ret > 0)
        doit_incoming_data(ctx, buf, ret);
    return ret;
}

int do_fetch_pascal_string(int sock, doit_ctx *ctx, char *buf)
{
    int x = doit_incoming_data(ctx, NULL, 0);
    unsigned char len;
    while (x == 0) {
        if (do_receive(sock, ctx) < 0)
            return -1;
        x = doit_incoming_data(ctx, NULL, 0);
    }
    if (doit_read(ctx, &len, 1) != 1)
        return -1;
    x--;
    while (x < len) {
        if (do_receive(sock, ctx) < 0)
            return -1;
        x = doit_incoming_data(ctx, NULL, 0);
    }
    if (doit_read(ctx, buf, len) != len)
        return -1;
    return len;
}

/*
 * Helper function to fetch a whole line from the socket.
 */
char *do_fetch(int sock, doit_ctx *ctx, int line_terminate, int *length)
{
    char *cmdline = NULL;
    int cmdlen = 0, cmdsize = 0;
    char buf[1024];
    int len;

    /*
     * Start with any existing buffered data.
     */
    len = doit_incoming_data(ctx, NULL, 0);
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
    }
}
char *do_fetch_line(int sock, doit_ctx *ctx)
{
    int len;
    return do_fetch(sock, ctx, 1, &len);
}
char *do_fetch_all(int sock, doit_ctx *ctx, int *len)
{
    return do_fetch(sock, ctx, 0, len);
}

/*
 * Helper functions to encrypt and send data.
 */
int do_doit_send(int sock, doit_ctx *ctx, void *data, int len)
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
int do_doit_send_str(int sock, doit_ctx *ctx, char *str)
{
    return do_doit_send(sock, ctx, str, strlen(str));
}

/*
 * Parse the command-line verb.
 */
enum {
    NOTHING,                           /* invalid command */
    WF,                                /* path-translation + ShellExecute */
    WWW,                               /* start a browser */
    WCLIP,                             /* read/write the clipboard */
    WCMD,                              /* run a process with output */
    WIN,                               /* just run a process */
};

int parse_cmd(char *verb)
{
    if (!strcmp(verb, "wf")) return WF;
    if (!strcmp(verb, "www")) return WWW;
    if (!strcmp(verb, "wclip")) return WCLIP;
    if (!strcmp(verb, "wcmd")) return WCMD;
    if (!strcmp(verb, "win")) return WIN;
    return NOTHING;
}

/*
 * Path name translation.
 */
struct mapping {
    char *from;
    char *to;
};
struct hostcfg {
    char *name;
    int nmappings;
    struct mapping *mappings;
};
struct hostcfg *hostcfgs = NULL;
int nhostcfgs = 0;

struct hostcfg *cfg = NULL;

void slashmap(char *p)
{
    p += strcspn(p, "/");
    while (*p) {
        *p = '\\';
        p += strcspn(p, "/");
    }
}

void new_hostcfg(char *name)
{
    nhostcfgs++;
    if (hostcfgs == NULL) {
        hostcfgs = malloc(nhostcfgs * sizeof(*hostcfgs));
    } else {
        hostcfgs = realloc(hostcfgs, nhostcfgs * sizeof(*hostcfgs));
    }
    hostcfgs[nhostcfgs-1].name = dupstr(name);
    hostcfgs[nhostcfgs-1].nmappings = 0;
    hostcfgs[nhostcfgs-1].mappings = NULL;
}

void add_pathmap(char *from, char *to)
{
    struct hostcfg *h;
    if (nhostcfgs == 0)
        return;
    h = &hostcfgs[nhostcfgs-1];
    h->nmappings++;
    if (h->mappings == NULL) {
        h->mappings = malloc(h->nmappings * sizeof(*h->mappings));
    } else {
        h->mappings = realloc(h->mappings,
                              h->nmappings * sizeof(*h->mappings));
    }
    h->mappings[h->nmappings-1].from = dupstr(from);
    h->mappings[h->nmappings-1].to = dupstr(to);
    /* Do \ -> / mapping here */
    slashmap(h->mappings[h->nmappings-1].from);
    slashmap(h->mappings[h->nmappings-1].to);
}

void select_hostcfg(char *name)
{
    int i;
    for (i = 0; i < nhostcfgs; i++) {
        if (strncmp(name, hostcfgs[i].name, strlen(hostcfgs[i].name)) == 0) {
            cfg = &hostcfgs[i];
            return;
        }
    }
}

char *path_translate(char *path)
{
    int i;

    if (!cfg)
        return path;

    path = dupstr(path);
    slashmap(path);
    for (i = 0; i < cfg->nmappings; i++) {
        char *from = cfg->mappings[i].from;
        int fromlen = strlen(from);
        if (strncmp(path, from, fromlen) == 0) {
            char *newpath;
            newpath = malloc(strlen(cfg->mappings[i].to) +
                             strlen(path+fromlen) + 1);
            strcpy(newpath, cfg->mappings[i].to);
            strcat(newpath, path+fromlen);
            free(path);
            path = newpath;
        }
    }
    return path;
}

int main(int argc, char **argv)
{
    int sock;
    doit_ctx *ctx;
    void *data, *secretdata;
    int len, secretlen;
    FILE *fp;
    char fname[FILENAME_MAX];
    char pbuf[256];
    char *msg, *dir, *path;

    unsigned long hostaddr;
    char *secret = NULL;
    char *browser = NULL;
    int port = DOIT_PORT;

    int cmd = 0;
    char *arg = NULL;
    int clip_read = 0;
    int need_readclip_output;
    int nogo = 0, errs = 0;
    int errcode;

    /*
     * Read the config file `.doitrc'.
     */
    strcpy(fname, getenv("HOME"));
    strcat(fname, "/.doitrc");
    fp = fopen(fname, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            char *cmd, *arg;
            buf[strcspn(buf, "\r\n")] = '\0';
            cmd = buf + strspn(buf, " \t");
            arg = cmd + strcspn(cmd, " \t");
            if (*arg) *arg++ = '\0';
            arg += strspn(arg, " \t");

            if (!strcmp(cmd, "secret"))
                secret = dupstr(arg);
            else if (!strcmp(cmd, "browser"))
                browser = dupstr(arg);
            else if (!strcmp(cmd, "host"))
                new_hostcfg(arg);
            else if (!strcmp(cmd, "map")) {
                char *arg2;
                arg2 = arg + strcspn(arg, " \t");
                if (*arg2) *arg2++ = '\0';
                arg2 += strspn(arg2, " \t");
                add_pathmap(arg, arg2);
            }
        }
        fclose(fp);
    }

    /*
     * Parse the command line to find out what to actually do.
     */
    cmd = parse_cmd(argv[0]);          /* are we called as wf, wclip etc? */
    /*
     * Parse command line arguments.
     */
    while (--argc) {
	char *p = *++argv;
	if (*p == '-') {
	    /*
	     * An option.
	     */
	    while (p && *++p) {
		char c = *p;
		switch (c) {
		  case '-':
		    /*
		     * Long option.
		     */
		    {
			char *opt, *val;
			opt = p++;     /* opt will have _one_ leading - */
			while (*p && *p != '=')
			    p++;	       /* find end of option */
			if (*p == '=') {
			    *p++ = '\0';
			    val = p;
			} else
			    val = NULL;
			if (!strcmp(opt, "-help")) {
			    /* FIXME: help(); */
			    nogo = 1;
			} else if (!strcmp(opt, "-version")) {
			    /* FIXME: showversion(); */
			    nogo = 1;
			} else if (!strcmp(opt, "-licence") ||
				   !strcmp(opt, "-license")) {
			    /* FIXME: licence(); */
			    nogo = 1;
			}
			/*
			 * A sample option requiring an argument:
			 * 
			 * else if (!strcmp(opt, "-output")) {
			 *     if (!val)
			 *         errs = 1, error(err_optnoarg, opt);
			 *     else
			 *         ofile = val;
			 * }
			 */
			else {
			    errs = 1;
                            fprintf(stderr,
                                    "doit: no such option \"-%s\"\n", opt);
			}
		    }
		    p = NULL;
		    break;
		  case 'h':
		  case 'V':
		  case 'L':
                  case 'r':
		    /*
		     * Option requiring no parameter.
		     */
		    switch (c) {
		      case 'h':
			/* FIXME: help(); */
			nogo = 1;
			break;
		      case 'V':
			/* FIXME: showversion(); */
			nogo = 1;
			break;
		      case 'L':
			/* FIXME: licence(); */
			nogo = 1;
			break;
                      case 'r':
                        clip_read = 1;
                        break;
		    }
		    break;
		    /*
		     * FIXME. Put cases here for single-char
		     * options that require parameters. An example
		     * is shown, commented out.
		     */
		  /* case 'o': */
		    /*
		     * Option requiring parameter.
		     */
		    p++;
		    if (!*p && argc > 1)
			--argc, p = *++argv;
		    else if (!*p) {
			errs = 1;
                        fprintf(stderr,
                                "doit: option \"-%c\" requires an argument\n",
                                c);
		    }
		    /*
		     * Now c is the option and p is the parameter.
		     */
		    switch (c) {
			/*
			 * A sample option requiring an argument:
			 *
		         * case 'o':
			 *   ofile = p;
			 *   break;
			 */
		    }
		    p = NULL;	       /* prevent continued processing */
		    break;
		  default:
		    /*
		     * Unrecognised option.
		     */
		    {
			errs = 1;
                        fprintf(stderr, "doit: no such option \"-%c\"\n", c);
		    }
		}
	    }
	} else {
	    /*
	     * A non-option argument.
	     */
            if (cmd == 0) {
                cmd = parse_cmd(p);
                if (cmd == 0) {
                    errs = 1;
                    fprintf(stderr, "doit: unrecognised command \"%s\"\n", p);
                }
            } else {
                if (arg == NULL) {
                    arg = dupstr(p);
                } else {
                    arg = realloc(arg, 2+strlen(arg)+strlen(p));
                    strcat(arg, " ");
                    strcat(arg, p);
                }
            }
	}
    }

    if (errs)
	exit(EXIT_FAILURE);
    if (nogo)
	exit(EXIT_SUCCESS);

    if (!cmd) {
        fprintf(stderr, "doit: no command verb supplied; try \"--help\"\n");
        exit(EXIT_FAILURE);
    }

    /*
     * Find out what host we should be connecting to.
     */
    {
        char *e, *prehost;
        int len;
        struct hostent *h;
        unsigned long a;

        e = getenv("DOIT_HOST");
        if (e != NULL) {
            prehost = dupstr(e);
        } else {
            e = getenv("SSH_CLIENT");
            if (e != NULL) {
                len = strcspn(e, " ");
                prehost = dupstr(e);
                prehost[len] = '\0';
            } else {
                e = getenv("DISPLAY");
                if (e != NULL) {
                    len = strcspn(e, ":");
                    prehost = dupstr(e);
                    prehost[len] = '\0';
                }
            }
        }

        a = inet_addr(prehost);
        if (a == (unsigned long)-1) {
            h = gethostbyname(prehost);
        } else {
            h = gethostbyaddr((const char *)&a, sizeof(a), AF_INET);
        }
        if (!h) {
            perror(prehost);
            return -1;
        }
        memcpy(&hostaddr, h->h_addr, sizeof(hostaddr));

        select_hostcfg(h->h_name);
    }

    secretdata = read_secret_file(secret, &secretlen);
    if (!secretdata)
        return 1;
    ctx = doit_init_ctx(secretdata, secretlen);
    if (!ctx)
        return 1;
    memset(secretdata, 0, secretlen);
    free(secretdata);

    sock = make_connection(hostaddr, port);
    if (sock < 0)
        return 1;

    doit_perturb_nonce(ctx, "client", 6);
    {
        int pid = getpid();
        doit_perturb_nonce(ctx, &pid, sizeof(pid));
    }
    data = doit_make_nonce(ctx, &len);

    while (!doit_got_keys(ctx)) {
        do_receive(sock, ctx);
    }

    if (do_send(sock, data, len) != len) {
        close(sock);
        return 1;
    }
    free(data);

    need_readclip_output = 0;
    switch (cmd) {
      case WF:
        if (!arg) {
            fprintf(stderr, "doit: \"wf\" requires an argument\n");
            exit(EXIT_FAILURE);
        }
        dir = get_pwd();
        path = malloc(2+strlen(dir)+strlen(arg));
        sprintf(path, "%s/%s", dir, arg);
        path = path_translate(path);
        do_doit_send_str(sock, ctx, "ShellExecute\n");
        do_doit_send_str(sock, ctx, path);
        do_doit_send_str(sock, ctx, "\n");
        break;
      case WIN:
        if (!arg) {
            fprintf(stderr, "doit: \"win\" requires an argument\n");
            exit(EXIT_FAILURE);
        }
        do_doit_send_str(sock, ctx, "CreateProcessNoWait\n");
        do_doit_send_str(sock, ctx, arg);
        do_doit_send_str(sock, ctx, "\n");
        break;
      case WWW:
        if (!arg) {
            fprintf(stderr, "doit: \"www\" requires an argument\n");
            exit(EXIT_FAILURE);
        }
        do_doit_send_str(sock, ctx, "CreateProcessNoWait\n");
        do_doit_send_str(sock, ctx, browser);
        do_doit_send_str(sock, ctx, " ");
        do_doit_send_str(sock, ctx, arg);
        do_doit_send_str(sock, ctx, "\n");
        break;
      case WCLIP:
        if (clip_read) {
            do_doit_send_str(sock, ctx, "ReadClipboard\n");
            need_readclip_output = 1;
        } else {
            char buf[512];
            do_doit_send_str(sock, ctx, "WriteClipboard\n");
            while (fgets(buf, sizeof(buf), stdin)) {
                int newlinepos = strcspn(buf, "\n");
                do_doit_send(sock, ctx, buf, newlinepos);
                if (buf[newlinepos]) {
                    do_doit_send_str(sock, ctx, "\r\n");
                }
            }
            shutdown(sock, 1);
        }
        break;
      case WCMD:
        if (!arg) {
            fprintf(stderr, "doit: \"wcmd\" requires an argument\n");
            exit(EXIT_FAILURE);
        }
        do_doit_send_str(sock, ctx, "CreateProcessWithOutput\ncmd /c ");
        dir = get_pwd();
        path = malloc(strlen(dir)+2);
        sprintf(path, "%s/", dir);
        dir = path_translate(path);
        if (dir[0] && dir[1] == ':') {
            do_doit_send(sock, ctx, dir, 2);
            do_doit_send_str(sock, ctx, " & ");
            dir += 2;
        }
        do_doit_send_str(sock, ctx, "cd ");
        if (strlen(dir) > 1 && dir[strlen(dir)-1] == '\\')
            dir[strlen(dir)-1] = '\0';
        do_doit_send_str(sock, ctx, dir);
        do_doit_send_str(sock, ctx, " & ");
        do_doit_send_str(sock, ctx, arg);
        do_doit_send_str(sock, ctx, "\n");
        while ( (len = do_fetch_pascal_string(sock, ctx, pbuf)) > 0) {
            fwrite(pbuf, 1, len, stdout);
        }
        if (len < 0) {
            fprintf(stderr, "doit: connection unexpectedly closed\n");
            close(sock);
            return 1;
        }
        break;
    }
    errcode = 0;
    msg = do_fetch_line(sock, ctx);
    if (msg[0] != '+') {
        fprintf(stderr, "doit: remote error: %s\n", msg+1);
    } else if (msg[1]) {
        /*
         * Extract error code.
         */
        if (atoi(msg+1) != 0)
            errcode = 1;
    }

    if (need_readclip_output) {
        int len;
        char *p;
        msg = do_fetch_all(sock, ctx, &len);
        p = msg;

        while (len > 0) {
            char *q = memchr(p, '\r', len);
            if (!q) {
                fwrite(p, 1, len, stdout);
                break;
            }
            fwrite(p, 1, q-p, stdout);
            len -= q-p;
            p = q;
            if (len > 1 && p[0] == '\r' && p[1] == '\n') {
                fputc('\n', stdout);
                p += 2;
                len -= 2;
            } else {
                fputc('\r', stdout);
                p++;
                len--;
            }
        }
    }

    close(sock);
    return errcode;
}
