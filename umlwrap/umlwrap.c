/*
 * umlwrap: Wrap a single command or an interactive shell inside a
 * UML sandbox, permitting read-only access to the external
 * filesystem (with specified exceptions) and no network access.
 */

#define _GNU_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "options.h"
#include "malloc.h"
#include "sel.h"
#include "protocol.h"
#include "isdup.h"
#include "rawmode.h"
#include "fgetline.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PNAME "umlwrap"

typedef enum { DIR_R=1, DIR_W=2, DIR_RW = 3 } rwdir;

/*
 * Terminals to restore to their previous termios settings on
 * output.
 */
struct termrestore {
    int fd;
    struct termios termios;
} *termrestores = NULL;
int ntermrestores = 0, termrestoresize = 0;
void termrestore_at_exit(void)
{
    int i;
    for (i = 0; i < ntermrestores; i++)
	tcsetattr(termrestores[i].fd, TCSANOW, &termrestores[i].termios);
    ntermrestores = 0;
}

/*
 * This structure defines a file descriptor of ours which we are
 * going to forward to the inner process. These are directional
 * (each one is forwarded only in a specific direction).
 */
struct fd {
    /*
     * These fields are filled in during command-line option
     * configuration.
     */
    int fd;			       /* the actual fd */
    rwdir rw;			       /* 1=read, 2=write, 3=both */
    /*
     * The following fields are used for internal bookkeeping once
     * we start trying to figure out which fds are supposed to go
     * where.
     */
    int is_tty, is_controlling;
    dev_t tty_id;
    int first;			       /* first of a set of duplicates */
    int overall_rw;		       /* I/O directions used in this group */
    int sp[2];			       /* socketpair used for this group */
    int serial;			       /* index of UML serial port for group */
    struct termios termios;
    struct winsize winsize;
    int baseindex;		       /* base index in inner fd array */
};

struct optctx {
    int ncmdwords, cmdwordsize;
    char **cmdwords;
    int nenvvars, envvarsize;
    char **envvars;
    int clearenv;
    int nunions, unionsize;
    char **unions;
    int nwritables, writablesize;
    char **writables;
    const char *root;
    int rootwrite;
    /*
     * uid,gid,groups configuration: all three are taken from the
     * exterior caller if no options are specified, all three are
     * overridden by --user if that's specified, and each can be
     * explicitly overridden on top of that. This translates into
     * an array of three options for each set of values, and a bit
     * mask in which bit (1 << i) is set iff something meaningful
     * is in array[i]. The lowest set entry is used; so explicit
     * configuration goes in array[0], --user in array[1], and the
     * defaults from the caller are initially placed in array[2].
     */
    uid_t uid[3];
    int uid_mask;
    gid_t gid[3];
    int gid_mask;
    gid_t *groups[3];
    int ngroups[3];
    int groups_mask;
    gid_t ttygid;
    const char *kernel;
    const char *initfs;
    char *hostname;
    char *cwd;
    struct fd *fds;
    int nfds, fdsize;
    int memory;			       /* currently in Mb */
    int verbose;
    int error;
};

void fatal(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", PNAME);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void addfd(struct optctx *ctx, int fd, rwdir rw)
{
    if (ctx->nfds >= ctx->fdsize) {
	ctx->fdsize = ctx->nfds * 3 / 2 + 32;
	ctx->fds = sresize(ctx->fds, ctx->fdsize, struct fd);
    }
    ctx->fds[ctx->nfds].fd = fd;
    ctx->fds[ctx->nfds].rw = rw;
    ctx->nfds++;
}

static int option(void *vctx, char optchr, char *longopt, char *value)
{
    struct optctx *ctx = (struct optctx *)vctx;

    if (optchr == 'k' || (longopt && !strcmp(longopt, "kernel"))) {
	if (!value) return -2;
	ctx->kernel = value;
	return 2;
    }

    if (optchr == 'I' || (longopt && !strcmp(longopt, "initfs"))) {
	if (!value) return -2;
	ctx->initfs = value;
	return 2;
    }

    if ((longopt && !strcmp(longopt, "hostname"))) {
	if (!value) return -2;
	ctx->hostname = value;
	return 2;
    }

    if (optchr == 'C' || (longopt && !strcmp(longopt, "cwd"))) {
	if (!value) return -2;
	/*
	 * Non-portability: realpath(x,NULL) is nonstandard
	 * according to the Linux realpath(3) manpage.
	 */
	ctx->cwd = realpath(value, NULL);
	if (!ctx->cwd) {
	    fprintf(stderr, "%s: %s: realpath: %s\n", PNAME, value,
		    strerror(errno));
	    ctx->error = 1;
	}
	return 2;
    }

    if ((longopt && !strcmp(longopt, "user"))) {
	/*
	 * Specify a user name (or numeric uid) and use it to set
	 * all of uid, gid and groups.
	 */
	char *err;
	struct passwd *pw;
	long uid;

	if (!value) return -2;

	pw = getpwnam(value);
	if (!pw) {
	    uid = strtol(value, &err, 0);
	    if (*value && !*err)
		pw = getpwuid(uid);

	    if (!pw) {
		fprintf(stderr, "%s: invalid user name '%s'\n",
			PNAME, value);
		ctx->error = 1;
		return 2;
	    }
	}

	ctx->uid[1] = pw->pw_uid;
	ctx->uid_mask |= 1 << 1;
	ctx->gid[1] = pw->pw_gid;
	ctx->gid_mask |= 1 << 1;
	ctx->ngroups[1] = 0;
	getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ctx->ngroups[1]);
	ctx->groups[1] = snewn(ctx->ngroups[1], gid_t);
	getgrouplist(pw->pw_name, pw->pw_gid,
		     ctx->groups[1], &ctx->ngroups[1]);
	ctx->groups_mask |= 1 << 1;

	return 2;
    }

    if ((longopt && !strcmp(longopt, "uid"))) {
	/*
	 * Specify a numeric uid (or user name) and use it to set
	 * just the uid.
	 */
	char *err;
	struct passwd *pw;
	long uid;

	if (!value) return -2;

	uid = strtol(value, &err, 0);
	if (*value && !*err) {
	    ctx->uid[0] = uid;
	    ctx->uid_mask |= 1 << 0;
	    return 2;
	}

	pw = getpwnam(value);
	if (!pw) {
	    if (!pw) {
		fprintf(stderr, "%s: invalid user id '%s'\n",
			PNAME, value);
		ctx->error = 1;
		return 2;
	    }
	}
	ctx->uid[0] = pw->pw_uid;
	ctx->uid_mask |= 1 << 0;
	return 2;
    }

    if ((longopt && !strcmp(longopt, "gid"))) {
	/*
	 * Specify a numeric gid (or group name) and use it to set
	 * just the gid.
	 */
	char *err;
	struct group *gr;
	long gid;

	if (!value) return -2;

	gid = strtol(value, &err, 0);
	if (*value && !*err) {
	    ctx->gid[0] = gid;
	    ctx->gid_mask |= 1 << 0;
	    return 2;
	}

	gr = getgrnam(value);
	if (!gr) {
	    if (!gr) {
		fprintf(stderr, "%s: invalid group id '%s'\n",
			PNAME, value);
		ctx->error = 1;
		return 2;
	    }
	}
	ctx->gid[0] = gr->gr_gid;
	ctx->gid_mask |= 1 << 0;
	return 2;
    }

    if ((longopt && !strcmp(longopt, "groups"))) {
	/*
	 * Specify a comma-separated list of numeric gids (or
	 * group names) and use them to set just the supplementary
	 * groups list.
	 */
	char *next, *err;
	struct group *gr;
	long lgid;
	gid_t gid;
	int gsize;

	if (!value) return -2;

	gsize = 0;
	ctx->groups[0] = NULL;
	ctx->ngroups[0] = 0;

	while (*value) {
	    next = value + strcspn(value, ",");
	    if (*next)
		*next++ = '\0';

	    lgid = strtol(value, &err, 0);
	    if (*value && !*err) {
		gid = lgid;
	    } else {
		gr = getgrnam(value);
		if (gr) {
		    gid = gr->gr_gid;
		} else {
		    fprintf(stderr, "%s: invalid group id '%s'\n",
			    PNAME, value);
		    ctx->error = 1;
		    return 2;
		}
	    }
	    if (ctx->ngroups[0] >= gsize) {
		gsize = ctx->ngroups[0] * 3 / 2 + 32;
		ctx->groups[0] = sresize(ctx->groups[0], gsize, gid_t);
	    }
	    ctx->groups[0][ctx->ngroups[0]++] = gid;

	    value = next;
	}
	ctx->groups_mask |= 1 << 0;
	return 2;
    }

    if ((longopt && !strcmp(longopt, "ttygid"))) {
	/*
	 * Specify a numeric gid (or group name) and use it to set
	 * the gid which will be used for devices in /dev/pts.
	 */
	char *err;
	struct group *gr;
	long gid;

	if (!value) return -2;

	gid = strtol(value, &err, 0);
	if (*value && !*err) {
	    ctx->ttygid = gid;
	    return 2;
	}

	gr = getgrnam(value);
	if (!gr) {
	    if (!gr) {
		fprintf(stderr, "%s: invalid group id '%s'\n",
			PNAME, value);
		ctx->error = 1;
		return 2;
	    }
	}

	ctx->ttygid = gr->gr_gid;
	return 2;
    }

    if (optchr == 'm' || (longopt && !strcmp(longopt, "memory"))) {
	if (!value) return -2;
	ctx->memory = atoi(value);
	return 2;
    }

    if (optchr == 'v' || (longopt && !strcmp(longopt, "verbose"))) {
	ctx->verbose = TRUE;
	return 1;
    }
    if (optchr == 'q' || (longopt && !strcmp(longopt, "quiet"))) {
	ctx->verbose = FALSE;
	return 1;
    }

    if (optchr == 'R' || (longopt && !strcmp(longopt, "root"))) {
	if (!value) return -2;
	/*
	 * Non-portability: realpath(x,NULL) is nonstandard
	 * according to the Linux realpath(3) manpage.
	 */
	ctx->root = realpath(value, NULL);
	if (!ctx->root) {
	    fprintf(stderr, "%s: %s: realpath: %s\n", PNAME, value,
		    strerror(errno));
	    ctx->error = 1;
	}
	return 2;
    }

    if (optchr == 'u' || (longopt && !strcmp(longopt, "union"))) {
	if (!value) return -2;
	if (ctx->nunions >= ctx->unionsize) {
	    ctx->unionsize = ctx->nunions * 3 / 2 + 64;
	    ctx->unions = sresize(ctx->unions, ctx->unionsize, char *);
	}
	/*
	 * Non-portability: realpath(x,NULL) is nonstandard
	 * according to the Linux realpath(3) manpage.
	 */
	ctx->unions[ctx->nunions] = realpath(value, NULL);
	if (!ctx->unions[ctx->nunions]) {
	    fprintf(stderr, "%s: %s: realpath: %s\n", PNAME, value,
		    strerror(errno));
	    ctx->error = 1;
	}
	ctx->nunions++;
	return 2;
    }

    if (optchr == 'w' || (longopt && !strcmp(longopt, "writable"))) {
	if (!value) return -2;
	if (ctx->nwritables >= ctx->writablesize) {
	    ctx->writablesize = ctx->nwritables * 3 / 2 + 64;
	    ctx->writables = sresize(ctx->writables,
				     ctx->writablesize, char *);
	}
	/*
	 * Non-portability: realpath(x,NULL) is nonstandard
	 * according to the Linux realpath(3) manpage.
	 */
	ctx->writables[ctx->nwritables] = realpath(value, NULL);
	if (!ctx->writables[ctx->nwritables]) {
	    fprintf(stderr, "%s: %s: realpath: %s\n", PNAME, value,
		    strerror(errno));
	    ctx->error = 1;
	}
	ctx->nwritables++;
	return 2;
    }

    if (optchr == 'W' || (longopt && !strcmp(longopt, "root-writable"))) {
	/*
	 * We count the number of times (up to 2) that this option
	 * has been seen.
	 */
	if (ctx->rootwrite < 2)
	    ctx->rootwrite++;
	return 1;
    }

    if (optchr == 'E' || (longopt && !strcmp(longopt, "clearenv"))) {
	ctx->clearenv = 1;
	return 1;
    }

    if (optchr == 'e' || (longopt && !strcmp(longopt, "env"))) {
	if (!value) return -2;
	if (ctx->nenvvars >= ctx->envvarsize) {
	    ctx->envvarsize = ctx->nenvvars * 3 / 2 + 64;
	    ctx->envvars = sresize(ctx->envvars, ctx->envvarsize, char *);
	}
	ctx->envvars[ctx->nenvvars] = value;
	ctx->nenvvars++;
	return 2;
    }

    if ((longopt && !strcmp(longopt, "fd-read"))) {
	if (!value) return -2;
	addfd(ctx, atoi(value), DIR_R);
	return 2;
    }
    if ((longopt && !strcmp(longopt, "fd-write"))) {
	if (!value) return -2;
	addfd(ctx, atoi(value), DIR_W);
	return 2;
    }
    if ((longopt && !strcmp(longopt, "fd-rw"))) {
	if (!value) return -2;
	addfd(ctx, atoi(value), DIR_RW);
	return 2;
    }
    if ((longopt && !strcmp(longopt, "fd"))) {
	int fd, flags;
	if (!value) return -2;
	fd = atoi(value);
	flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
	    fprintf(stderr, "%s: fd %d: fcntl(F_GETFL): %s\n", PNAME, fd,
		    strerror(errno));
	    ctx->error = 1;
	    return 2;
	}
	/*
	 * I'm not sure how best to portably get the O_RDONLY/
	 * O_WRONLY/O_RDWR status out of this. One can't just test
	 * by ANDing with each in turn, because one of the flags
	 * itself might be zero (they aren't required to all be
	 * distinct bits). I think the best I can do is to OR them
	 * all together, AND with that, and test the result
	 * against each individually.
	 */
	if ((flags & (O_RDONLY|O_WRONLY|O_RDWR)) == O_RDONLY)
	    addfd(ctx, fd, DIR_R);
	else if ((flags & (O_RDONLY|O_WRONLY|O_RDWR)) == O_WRONLY)
	    addfd(ctx, fd, DIR_W);
	else if ((flags & (O_RDONLY|O_WRONLY|O_RDWR)) == O_RDWR)
	    addfd(ctx, fd, DIR_RW);
	return 2;
    }

    if ((longopt && !strcmp(longopt, "licence")) ||
	(longopt && !strcmp(longopt, "license"))) {
	extern const char *const licence[];
	int i;

	for (i = 0; licence[i]; i++)
	    fputs(licence[i], stdout);

	exit(0);
    }

    if ((longopt && !strcmp(longopt, "help"))) {
	static const char *const help[] = {
	    "usage: umlwrap [options] [command [args...]]\n",
	    "where: -k path, --kernel=path   pathname to UML kernel binary\n",
	    "       -I path, --initfs=path   pathname to UML initial root fs\n",
	    "       --hostname=name          hostname used in UML system\n",
	    "       -C dir, --cwd=dir        working directory for command\n",
	    "       --user=name              set uid,gid,groups for command\n",
	    "       --uid=name               set uid only for command\n",
	    "       --gid=name               set gid only for command\n",
	    "       --groups=name[,name...]  set groups list for command\n",
	    "       --ttygid=name            set owning gid for tty devices\n",
	    "       -m megs, --memory=megs   set available memory in Mb\n",
	    "       -R path, --root=path     use different root directory\n",
	    "       -u path, --union=path    overlay on root directory\n",
	    "       -w path, --writable=path make a subdirectory writable\n",
	    "       -W, --root-writable      make whole VFS writable\n",
	    "       -e var=val, --env=var=val set an environment variable\n",
	    "       -E, --clearenv           do not copy calling environment\n",
	    "       --fd-read=fd             propagate an extra read-only fd\n",
	    "       --fd-write=fd            propagate an extra write-only fd\n",
	    "       --fd-rw=fd               propagate an extra read-write fd\n",
	    "       --fd=fd                  propagate an extra fd (auto-sense rw status)\n",
	    "       -v, --verbose            print lots of diagnostic messages\n",
	    "       -q, --quiet              print no diagnostic messages\n",
	    " also: umlwrap --help           show this text\n",
	    "       umlwrap --licence        show (MIT) licence text\n",
	    NULL
	};
	int i;

	for (i = 0; help[i]; i++)
	    fputs(help[i], stdout);

	exit(0);
    }

    if (!optchr && !longopt) {
	if (ctx->ncmdwords >= ctx->cmdwordsize) {
	    ctx->cmdwordsize = ctx->ncmdwords * 3 / 2 + 64;
	    ctx->cmdwords = sresize(ctx->cmdwords, ctx->cmdwordsize, char *);
	}
	ctx->cmdwords[ctx->ncmdwords] = value;
	ctx->ncmdwords++;
	return 1;
    }

    return -1;
}

struct consoledata_ctx {
    struct optctx *optctx;
    char *line;
    size_t linelen, linesize;
};

static void console_readdata(sel_rfd *rfd, void *vdata, size_t len)
{
    char *data = (char *)vdata;
    struct consoledata_ctx *ctx =
	(struct consoledata_ctx *)sel_rfd_get_ctx(rfd);;

    if (len == 0) {
	int fd = sel_rfd_delete(rfd);
	close(fd);
	termrestore_at_exit();
	fprintf(stderr, "%s: end of file on UML console (kernel panic?)\n",
		PNAME);
	exit(1);
    }

    if (!ctx->optctx->verbose)
	return;

    while (len > 0) {
	char *newline = memchr(data, '\n', len);
	size_t copylen = newline ? newline - data + 1 : len;

	if (ctx->linelen + len >= ctx->linesize) {
	    ctx->linesize = (ctx->linelen + len) * 3 / 2 + 512;
	    ctx->line = sresize(ctx->line, ctx->linesize, char);
	}
	memcpy(ctx->line + ctx->linelen, data, copylen);
	ctx->linelen += copylen;

	if (newline) {
	    assert(ctx->linelen > 0);
	    ctx->line[ctx->linelen-1] = '\0';
	    fprintf(stderr, "console: %s\n", ctx->line);
	    ctx->linelen = 0;
	}

	len -= copylen;
	data += copylen;
    }
}

static void console_readerr(sel_rfd *rfd, int error)
{
    fprintf(stderr, "%s: UML console: read: %s\n", PNAME, strerror(errno));
    int fd = sel_rfd_delete(rfd);
    close(fd);
}

struct control_ctx {
    struct optctx *optctx;
    sel_rfd *rfd;
    sel_wfd *wfd;
    sel *sel;
    protoread *pr;
    int status;			       /* result of wait() */
    int gotstatus;		       /* have we got status yet? */
    list protocopies;
    int cttyfd;
};

int signalpipe[2];
static void sigwinch(int sig)
{
    write(signalpipe[1], "W", 1);
}
static void signalpipe_readdata(sel_rfd *rfd, void *vdata, size_t len)
{
    struct control_ctx *ctx = (struct control_ctx *)sel_rfd_get_ctx(rfd);
    struct winsize winsize;

    if (ctx->cttyfd >= 0 &&
	ioctl(ctx->cttyfd, TIOCGWINSZ, (void *)&winsize) >= 0) {
	protowrite(ctx->wfd, CMD_WINSIZE, &winsize, sizeof(winsize),
		   (void *)NULL);
    }
}

static void control_writeerr(sel_wfd *wfd, int error)
{
    struct control_ctx *ctx = (struct control_ctx *)sel_wfd_get_ctx(wfd);

    fprintf(stderr, "%s: UML control fd: write: %s\n", PNAME, strerror(errno));
    close(sel_wfd_delete(ctx->wfd));
    close(sel_rfd_delete(ctx->rfd));
    sfree(ctx);
}

static void control_readerr(sel_rfd *rfd, int error)
{
    struct control_ctx *ctx = (struct control_ctx *)sel_rfd_get_ctx(rfd);

    fprintf(stderr, "%s: UML control fd: read: %s\n", PNAME, strerror(errno));
    close(sel_wfd_delete(ctx->wfd));
    close(sel_rfd_delete(ctx->rfd));
    sfree(ctx);
}

#define WRITEINT(buf, val) do { \
    unsigned long wi_val = (val); \
    unsigned char *wi_buf = (unsigned char *)buf; \
    wi_buf[0] = wi_val >> 24; \
    wi_buf[1] = wi_val >> 16; \
    wi_buf[2] = wi_val >> 8; \
    wi_buf[3] = wi_val; \
} while (0)

static void control_packet(void *vctx, int type, void *data, size_t len)
{
    struct control_ctx *ctx = (struct control_ctx *)vctx;
    int i, j;

    if (type == CMD_HELLO) {
	unsigned char intbuf[4];
	unsigned char intbuf2[4];

	/*
	 * The init process is alive. Begin issuing commands.
	 */

	/*
	 * Set up the filesystems. First the union layers on top
	 * of the base root fs, in reverse order...
	 */
	for (i = ctx->optctx->nunions; i-- > 0 ;)
	    protowrite(ctx->wfd, CMD_UNION, ctx->optctx->unions[i],
		       strlen(ctx->optctx->unions[i]), (void *)NULL);
	/*
	 * ... then the base root fs itself, optionally preceded
	 * by the flag that turns it r/w.
	 */
	if (ctx->optctx->rootwrite)
	    protowrite(ctx->wfd, CMD_ROOTRW, (void *)NULL);
	protowrite(ctx->wfd, CMD_ROOT, ctx->optctx->root,
		   strlen(ctx->optctx->root), (void *)NULL);

	/*
	 * Now the writable subareas.
	 */
	for (i = 0; i < ctx->optctx->nwritables; i++)
	    protowrite(ctx->wfd, CMD_WRITABLE, ctx->optctx->writables[i],
		       strlen(ctx->optctx->writables[i]), (void *)NULL);

	/*
	 * Set up the host name.
	 */
	protowrite(ctx->wfd, CMD_HOSTNAME, ctx->optctx->hostname,
		   strlen(ctx->optctx->hostname), (void *)NULL);

	/*
	 * Set up the user id, group id and supplementary groups
	 * list.
	 */
	{
	    uid_t uid;
	    gid_t gid;
	    gid_t *groups;
	    int ngroups;
	    unsigned char *groupbuf;

	    for (i = 0; i < 3; i++)
		if (ctx->optctx->uid_mask & (1 << i)) {
		    uid = ctx->optctx->uid[i];
		    break;
		}
	    assert(i < 3);
	    WRITEINT(intbuf, uid);
	    protowrite(ctx->wfd, CMD_UID, intbuf, 4, (void *)NULL);

	    for (i = 0; i < 3; i++)
		if (ctx->optctx->gid_mask & (1 << i)) {
		    gid = ctx->optctx->gid[i];
		    break;
		}
	    assert(i < 3);
	    WRITEINT(intbuf, gid);
	    protowrite(ctx->wfd, CMD_GID, intbuf, 4, (void *)NULL);

	    WRITEINT(intbuf, ctx->optctx->ttygid);
	    protowrite(ctx->wfd, CMD_TTYGID, intbuf, 4, (void *)NULL);

	    for (i = 0; i < 3; i++)
		if (ctx->optctx->groups_mask & (1 << i)) {
		    groups = ctx->optctx->groups[i];
		    ngroups = ctx->optctx->ngroups[i];
		    break;
		}
	    assert(i < 3);
	    groupbuf = snewn(4*ngroups, unsigned char);
	    for (i = 0; i < ngroups; i++)
		WRITEINT(groupbuf+i*4, groups[i]);
	    protowrite(ctx->wfd, CMD_GROUPS, groupbuf, 4*ngroups,
		       (void *)NULL);
	    sfree(groupbuf);
	}

	/*
	 * Begin the I/O channel forwarding setup. Allocate the
	 * ptys and pipes on the inside.
	 */
	j = 0;
	for (i = 0; i < ctx->optctx->nfds; i++) {
	    if (ctx->optctx->fds[i].first != i)
		continue;
	    ctx->optctx->fds[i].baseindex = j;
	    if (ctx->optctx->fds[i].is_tty) {
		int cmd = CMD_PTY;
		if (ctx->optctx->fds[i].is_controlling)
		    cmd = CMD_PTYC;
		protowrite(ctx->wfd, cmd,
			   &ctx->optctx->fds[i].termios,
			   sizeof(ctx->optctx->fds[i].termios),
			   &ctx->optctx->fds[i].winsize,
			   sizeof(ctx->optctx->fds[i].winsize),
			   (void *)NULL);
		j += 3;		       /* read, write, read+write */
	    } else if (ctx->optctx->fds[i].rw == DIR_R) {
		protowrite(ctx->wfd, CMD_IPIPE, (void *)NULL);
		j++;
	    } else if (ctx->optctx->fds[i].rw == DIR_W) {
		protowrite(ctx->wfd, CMD_OPIPE, (void *)NULL);
		j++;
	    } else if (ctx->optctx->fds[i].rw == DIR_RW) {
		protowrite(ctx->wfd, CMD_IOPIPE, (void *)NULL);
		j++;
	    }
	}
	/*
	 * Now assign them to the UML serial ports we'll use to
	 * pipe them in and out of the virtual environment.
	 */
	for (i = 0; i < ctx->optctx->nfds; i++) {
	    if (ctx->optctx->fds[i].first != i)
		continue;
	    WRITEINT(intbuf, ctx->optctx->fds[i].serial);
	    WRITEINT(intbuf2, ctx->optctx->fds[i].baseindex);
	    if (ctx->optctx->fds[i].overall_rw & DIR_R) {
		protowrite(ctx->wfd, CMD_ISERIAL, intbuf, 4, intbuf2, 4,
			   (void *)NULL);
	    }
	    if (ctx->optctx->fds[i].overall_rw & DIR_W) {
		protowrite(ctx->wfd, CMD_OSERIAL, intbuf, 4, intbuf2, 4,
			   (void *)NULL);
	    }
	}
	/*
	 * And then assign them to actual fds on the inside.
	 */
	for (i = 0; i < ctx->optctx->nfds; i++) {
	    WRITEINT(intbuf, ctx->optctx->fds[i].fd);
	    j = ctx->optctx->fds[ctx->optctx->fds[i].first].baseindex;
	    if (ctx->optctx->fds[i].is_tty)
		j += ctx->optctx->fds[i].rw - DIR_R;
	    WRITEINT(intbuf2, j);
	    protowrite(ctx->wfd, CMD_ASSIGNFD, intbuf, 4, intbuf2, 4,
		       (void *)NULL);
	}

	/*
	 * Set the working directory.
	 */
	protowrite(ctx->wfd, CMD_CHDIR, ctx->optctx->cwd,
		   strlen(ctx->optctx->cwd), (void *)NULL);

	/*
	 * Send the environment.
	 */
	if (!ctx->optctx->clearenv) {
	    for (i = 0; environ[i]; i++)
		protowrite(ctx->wfd, CMD_ENVIRON, environ[i],
			   strlen(environ[i]), (void *)NULL);
	}
	for (i = 0; i < ctx->optctx->nenvvars; i++)
	    protowrite(ctx->wfd, CMD_ENVIRON, ctx->optctx->envvars[i],
		       strlen(ctx->optctx->envvars[i]), (void *)NULL);

	/*
	 * Send the actual command.
	 */
	for (i = 0; i < ctx->optctx->ncmdwords; i++)
	    protowrite(ctx->wfd, CMD_COMMAND, ctx->optctx->cmdwords[i],
		       strlen(ctx->optctx->cmdwords[i]), (void *)NULL);

	/*
	 * And go!
	 */
	protowrite(ctx->wfd, CMD_GO, (void *)NULL);
    } else if (type == CMD_FAILURE) {
	termrestore_at_exit();
	fprintf(stderr, "umlwrap-init: %.*s\n", (int)len, (char *)data);
	exit(1);
    } else if (type == CMD_GONE) {
	if (ctx->optctx->verbose) {
	    fprintf(stderr, "umlwrap: command started\n");
	}
	/*
	 * Now we put the ttys into raw mode and set up the
	 * protocopies to funnel data back and forth. (We delayed
	 * doing it until now because we were still printing
	 * verbose log messages.)
	 */
	for (i = 0; i < ctx->optctx->nfds; i++) {

	    if (ctx->optctx->fds[i].first != i)
		continue;

	    if (ctx->optctx->fds[i].is_tty) {
		if (ntermrestores >= termrestoresize) {
		    termrestoresize = ntermrestores * 3 / 2 + 16;
		    termrestores = sresize(termrestores, termrestoresize,
					   struct termrestore);
		}
		termrestores[ntermrestores].fd = dup(ctx->optctx->fds[i].fd);
		if (raw_mode(ctx->optctx->fds[i].fd, NULL,
			     &termrestores[ntermrestores].termios) >= 0)
		    ntermrestores++;
	    }

	    if (ctx->optctx->fds[i].overall_rw & DIR_R)
		list_add(&ctx->protocopies, (listnode *)
			 protocopy_encode_new(ctx->sel,
					      ctx->optctx->fds[i].fd,
					      ctx->optctx->fds[i].sp[1]));
	    if (ctx->optctx->fds[i].overall_rw & DIR_W)
		list_add(&ctx->protocopies, (listnode *)
			 protocopy_decode_new(ctx->sel,
					      ctx->optctx->fds[i].sp[1],
					      ctx->optctx->fds[i].fd));

	}
    } else if (type == CMD_EXITCODE) {
	ctx->status = ((unsigned char *)data)[0];
	ctx->status = (ctx->status << 8) | ((unsigned char *)data)[1];
	ctx->status = (ctx->status << 8) | ((unsigned char *)data)[2];
	ctx->status = (ctx->status << 8) | ((unsigned char *)data)[3];
	ctx->gotstatus = TRUE;
    }
}

static void control_readdata(sel_rfd *rfd, void *vdata, size_t len)
{
    struct control_ctx *ctx = (struct control_ctx *)sel_rfd_get_ctx(rfd);

    protoread_data(ctx->pr, vdata, len, control_packet, ctx);
}

int main(int argc, char **argv)
{
    struct optctx actx, *ctx = &actx;
    struct control_ctx *ctlctx;
    int watchdogpipe[2], consolepipe[2], controlsock[2];
    char **kerncl;
    int nkerncl, kernclsize;
    pid_t pid;
    sel *sel;
    int i, j, ret;
    int cttyfd;
    dev_t cttydev;

    ctx->ncmdwords = ctx->cmdwordsize = 0;
    ctx->cmdwords = NULL;
    ctx->nenvvars = ctx->envvarsize = 0;
    ctx->envvars = NULL;
    ctx->clearenv = 0;
    ctx->nunions = ctx->unionsize = 0;
    ctx->unions = NULL;
    ctx->nwritables = ctx->writablesize = 0;
    ctx->writables = NULL;
    ctx->root = "/";
    ctx->rootwrite = 0;
    ctx->kernel = "linux";
    ctx->initfs = UMLROOT;	       /* should be #defined by Makefile */
    ctx->memory = 512;
    ctx->verbose = FALSE;
    ctx->uid[2] = getuid();
    ctx->uid_mask = 1 << 2;
    ctx->gid[2] = getgid();
    ctx->gid_mask = 1 << 2;
    j = 0;
    ctx->groups[2] = NULL;
    while (1) {
	j = j * 3 / 2 + 32;
	ctx->groups[2] = sresize(ctx->groups[2], j, gid_t);
	ret = getgroups(j, ctx->groups[2]);
	if (ret > 0) {
	    ctx->ngroups[2] = ret;
	    break;
	} else if (errno != EINVAL) {
	    perror(PNAME ": getgroups");
	    return 1;
	}
	/* else go round again with more space */
    }
    ctx->groups_mask = 1 << 2;
    {
	struct group *gr;
	if ((gr = getgrnam("tty")) != NULL)
	    ctx->ttygid = gr->gr_gid;
	else
	    ctx->ttygid = 0;
    }
    ctx->cwd = getcwd(NULL, 0);	       /* NB non-portable Linux extension */
    ctx->error = 0;
    {
	/*
	 * gethostname() has slightly silly semantics with regard
	 * to truncation.
	 */
	size_t size = 64;
	ctx->hostname = snewn(size, char);
	while (1) {
	    int ret = gethostname(ctx->hostname, size);
	    if (ret < 0) {
		perror(PNAME ": hostname");
		strcpy(ctx->hostname, "unknownhost");
		break;
	    }
	    ctx->hostname[size-1] = '\0';   /* ensure NUL termination */
	    if (strlen(ctx->hostname) < size-1)
		break;		       /* success */
	    size = size * 3 / 2;
	    ctx->hostname = sresize(ctx->hostname, size, char);
	}
	/*
	 * Now append "-umlwrap".
	 */
	ctx->hostname = sresize(ctx->hostname, strlen(ctx->hostname)+9, char);
	strcat(ctx->hostname, "-umlwrap");
    }
    ctx->fds = NULL;
    ctx->nfds = ctx->fdsize = 0;
    addfd(ctx, 0, DIR_R);
    addfd(ctx, 1, DIR_W);
    addfd(ctx, 2, DIR_W);

    if (!parse_opts(PNAME, argc-1, argv+1, option, ctx) || ctx->error)
	return 1;

    atexit(termrestore_at_exit);

    if (!ctx->ncmdwords) {
	/*
	 * If the user hasn't provided a command to run at all,
	 * our default is to run their shell with no arguments.
	 *
	 * Rather than replicating the code from option(), the
	 * simplest way to add a command word is to _call_
	 * option()!
	 */
	option(ctx, '\0', NULL, getenv("SHELL"));
    }

    /*
     * If the user asks for the root directory to be writable, we
     * throw an error, on the grounds that that probably wasn't
     * what they meant. Specifying -W twice will override that.
     */
    if (ctx->rootwrite == 1 && !strcmp(ctx->root, "/") && !ctx->nunions) {
	fprintf(stderr, "%s: -W with default root directory is risky\n"
		"%*sspecify -W twice to override this diagnostic\n",
		PNAME, (int)(strlen(PNAME)+2), "");
	return 1;
    }

    /*
     * Go through the list of fds working out which are duplicates
     * of which, or refer to the same tty. Also identify which tty
     * is the controlling one, while we're here.
     */
    {
	FILE *fp;
	char *data = NULL;
	int dev;

	cttyfd = open("/dev/tty", O_RDWR);
	dev = -1;

	/*
	 * I had thought that opening /dev/tty and fstatting it
	 * would be adequate to get the st_rdev value, but in fact
	 * that always returns the st_rdev for /dev/tty itself,
	 * leaving me none the wiser about the _real_ device id of
	 * my controlling tty.
	 *
	 * As far as I can tell, the most sensible way to find out
	 * _what_ our controlling terminal is is the way ps does
	 * it: read /proc/foo/stat (here /proc/self/stat) and
	 * extract the device ID from the text-formatted data in
	 * there. Gah!
	 */
	fp = fopen("/proc/self/stat", "r");
	if (fp) {
	    data = fgetline(fp);
	    fclose(fp);
	}
	if (data) {
	    char *p = strrchr(data, ')');
	    if (p && p[1])
		sscanf(p+2, "%*s %*d %*d %*d %d", &dev);
	    sfree(data);
	}

	cttydev = dev;
    }

    for (i = 0; i < ctx->nfds; i++) {
	/*
	 * Identify ttys.
	 */
	ctx->fds[i].is_tty = isatty(ctx->fds[i].fd);
	if (ctx->fds[i].is_tty) {
	    struct stat st;
	    if (fstat(ctx->fds[i].fd, &st) < 0) {
		fprintf(stderr, "%s: fd %d: fstat: %s\n", PNAME,
			ctx->fds[i].fd, strerror(errno));
		return 1;
	    }
	    ctx->fds[i].tty_id = st.st_rdev;
	    ctx->fds[i].is_controlling = (st.st_rdev == cttydev);

	    /*
	     * For ttys, group together any set of fds which are
	     * the same tty, regardless of whether they're
	     * technically duplicate fds.
	     */
	    ctx->fds[i].first = i;
	    for (j = 0; j < i; j++)
		if (ctx->fds[j].is_tty &&
		    ctx->fds[j].tty_id == ctx->fds[i].tty_id) {
		    ctx->fds[i].first = j;
		    break;
		}

	    if (ctx->fds[i].first == i) {
		if (tcgetattr(ctx->fds[i].fd, &ctx->fds[i].termios) < 0) {
		    fprintf(stderr, "%s: fd %d: tcgetattr: %s\n", PNAME,
			    ctx->fds[i].fd, strerror(errno));
		    return 1;
		}
		if (ioctl(ctx->fds[i].fd, TIOCGWINSZ,
			  (void *)&ctx->fds[i].winsize) < 0) {
		    fprintf(stderr, "%s: fd %d: ioctl(TIOCGWINSZ): %s\n",
			    PNAME, ctx->fds[i].fd, strerror(errno));
		    return 1;
		}
	    }
	} else {
	    ctx->fds[i].is_controlling = 0;
	    /*
	     * For non-tty fds, use isdup. Different directions of
	     * the same fd are treated separately.
	     */
	    ctx->fds[i].first = i;
	    for (j = 0; j < i; j++)
		if (!ctx->fds[j].is_tty &&
		    ctx->fds[j].rw == ctx->fds[i].rw &&
		    isdup(ctx->fds[j].fd, ctx->fds[i].fd, 1)) {
		    ctx->fds[i].first = j;
		    break;
		}
	}
	if (ctx->fds[i].first == i)
	    ctx->fds[i].overall_rw = 0;
	ctx->fds[ctx->fds[i].first].overall_rw |= ctx->fds[i].rw;
    }

    /*
     * Create some pipes and socketpairs which we'll use to talk
     * to various bits of our subprocesses.
     */
    if (pipe(watchdogpipe) < 0 ||
	pipe(consolepipe) < 0) {
	perror(PNAME ": pipe");
	return 1;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, controlsock) < 0) {
	perror(PNAME ": socketpair");
	return 1;
    }

    /*
     * Set up the kernel command line.
     */
    kernclsize = 40;
    kerncl = snewn(kernclsize, char *);
    nkerncl = 0;
    kerncl[nkerncl++] = dupstr(ctx->kernel);
    kerncl[nkerncl++] = snewn(80, char);
    sprintf(kerncl[nkerncl-1], "mem=%dM", ctx->memory);
    kerncl[nkerncl++] = dupstr("rootfstype=hostfs");
    kerncl[nkerncl++] = snewn(40 + strlen(ctx->initfs), char);
    sprintf(kerncl[nkerncl-1], "rootflags=%s", ctx->initfs);
    kerncl[nkerncl++] = dupstr("ro");
    kerncl[nkerncl++] = snewn(40, char);
    sprintf(kerncl[nkerncl-1], "ssl0=fd:%d", controlsock[0]);
    /*
     * Set up some more serial ports for our forwarded fds.
     */
    j = 1;			       /* serial port index */
    for (i = 0; i < ctx->nfds; i++) {
	if (ctx->fds[i].first != i)
	    continue;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, ctx->fds[i].sp) < 0) {
	    perror(PNAME ": socketpair");
	    return 1;
	}
	if (nkerncl+1 >= kernclsize) {
	    kernclsize = (nkerncl + 1) * 3 / 2;
	    kerncl = sresize(kerncl, kernclsize, char *);
	}
	kerncl[nkerncl++] = dupstr("ro");
	kerncl[nkerncl++] = snewn(40, char);
	sprintf(kerncl[nkerncl-1], "ssl%d=fd:%d", j, ctx->fds[i].sp[0]);
	ctx->fds[i].serial = j++;
    }
    assert(nkerncl < kernclsize);
    kerncl[nkerncl++] = NULL;

    /*
     * Start up an instance of UML, together with a watchdog
     * process which should kill it in the event of the main
     * program disappearing.
     *
     * We set up two pipes to communicate with our subprocesses
     * UML. One is not written to at all: the main process holds
     * the write end, so that if it dies the watchdog subprocess
     * will notice and kill everything off. The other becomes
     * UML's stdout/stderr, so that the kernel boot messages are
     * funnelled back to the main process in order to display them
     * in verbose mode.
     */
    pid = fork();
    if (pid < 0) {
	perror(PNAME ": fork");
	return 1;
    } else if (pid == 0) {
	pid_t pid2;

	/*
	 * We are the child process. Establish ourselves in a new
	 * process group, so that the whole of UML can be
	 * conveniently killed when the time comes.
	 */
	setpgrp();
	close(watchdogpipe[1]);

	/*
	 * Now fork again and exec UML.
	 */
	pid2 = fork();
	if (pid2 < 0) {
	    perror(PNAME " (subprocess): fork");
	    _exit(127);
	} else if (pid2 == 0) {
	    int sillystdinpipe[2];

	    /*
	     * Dup the write end of our consolepipe on to UML's
	     * stdout and stderr, and put a pipe on its stdin
	     * which nothing ever writes to. At all costs we must
	     * ensure UML doesn't talk directly to the controlling
	     * terminal from which umlwrap was called, because
	     * it'll set it into nonblocking mode and irritate the
	     * user.
	     */
	    close(watchdogpipe[0]);
	    close(consolepipe[0]);
	    dup2(consolepipe[1], 1);
	    dup2(consolepipe[1], 2);
	    close(consolepipe[1]);
	    close(controlsock[1]);
	    for (i = 0; i < ctx->nfds; i++) {
		if (ctx->fds[i].first != i)
		    continue;
		close(ctx->fds[i].sp[1]);
	    }
	    if (pipe(sillystdinpipe) == 0) {
		dup2(sillystdinpipe[0], 0);
		close(sillystdinpipe[0]);
	    }

	    execvp(ctx->kernel, kerncl);

	    /*
	     * By this time we've lost our original stderr, but we
	     * can at least talk back through the console pipe so
	     * that the error will be reported in verbose mode.
	     */
	    perror(PNAME " (second subprocess): execlp");
	    exit(127);
	}

	/*
	 * The watchdog process just waits for the watchdog pipe
	 * to close, then destroys its entire process group.
	 */
	signal(SIGTERM, SIG_IGN);
	close(consolepipe[0]);
	close(consolepipe[1]);
	close(controlsock[0]);
	close(controlsock[1]);
	for (i = 0; i < ctx->nfds; i++) {
	    if (ctx->fds[i].first != i)
		continue;
	    close(ctx->fds[i].sp[0]);
	    close(ctx->fds[i].sp[1]);
	}
	{
	    char buf[16];
	    int ret;

	    while (1) {
		ret = read(watchdogpipe[0], buf, sizeof(buf));
		if (ret > 0)
		    continue;	       /* _shouldn't_ happen! */
		if (ret < 0 && (errno == EINTR || errno == EAGAIN))
		    continue;	       /* I've no idea if that can happen */
		break;
	    }

	    kill(-getpgrp(), SIGKILL);
	    exit(127);   /* in the bizarre case of that not working */
	}
    }

    close(watchdogpipe[0]);
    close(consolepipe[1]);
    close(controlsock[0]);
    for (i = 0; i < ctx->nfds; i++) {
	if (ctx->fds[i].first != i)
	    continue;
	close(ctx->fds[i].sp[0]);
    }

    /*
     * Set up the main select loop.
     */
    sel = sel_new(NULL);

    {
	struct consoledata_ctx *conctx = snew(struct consoledata_ctx);
	conctx->optctx = ctx;
	conctx->line = NULL;
	conctx->linelen = conctx->linesize = 0;
	sel_rfd_add(sel, consolepipe[0],
		    console_readdata, console_readerr, conctx);
    }

    ctlctx = snew(struct control_ctx);
    ctlctx->optctx = ctx;
    ctlctx->wfd = sel_wfd_add(sel, controlsock[1], NULL,
			      control_writeerr, ctlctx);
    ctlctx->rfd = sel_rfd_add(sel, controlsock[1],
			      control_readdata, control_readerr, ctlctx);
    ctlctx->sel = sel;
    ctlctx->pr = protoread_new();
    ctlctx->gotstatus = 0;
    ctlctx->status = -1;
    ctlctx->cttyfd = cttyfd;
    list_init(&ctlctx->protocopies);

    /*
     * Set up a means of bringing SIGWINCH back to the main
     * program.
     */
    if (pipe(signalpipe) < 0) {
	perror("umlwrap init: pipe");
	return 1;
    }
    sel_rfd_add(sel, signalpipe[0], signalpipe_readdata, NULL, ctlctx);
    signal(SIGWINCH, sigwinch);

    /*
     * Now we enter the main select loop.
     */
    do {
	ret = sel_iterate(sel, -1);

	/*
	 * If we've got the exit code, and all outgoing
	 * protocopies are defunct, then we can leave.
	 */
	if (ctlctx->gotstatus) {
	    int ok = 1;
	    listnode *node, *tmp;

	    LOOP_OVER_LIST(node, tmp, &ctlctx->protocopies) {
		if (node->type == LISTNODE_PROTOCOPY_DECODE) {
		    ok = 0;
		    break;
		}
	    }

	    if (ok) {
		int exitcode;

		termrestore_at_exit();

		if (WIFEXITED(ctlctx->status)) {
		    if (ctx->verbose)
			fprintf(stderr, "umlwrap: command terminated with"
				" status %d\n", WEXITSTATUS(ctlctx->status));
		    exitcode = WEXITSTATUS(ctlctx->status);
		} else if (WIFSIGNALED(ctlctx->status)) {
		    if (ctx->verbose)
			fprintf(stderr, "umlwrap: command terminated on"
				" signal %d\n", WTERMSIG(ctlctx->status));
		    exitcode = 128 + WTERMSIG(ctlctx->status);
		} else {
		    fprintf(stderr, "umlwrap: CMD_EXITCODE received with"
			    " strange status %d\n", ctlctx->status);
		    exitcode = 127;
		}

		exit(exitcode);
	    }
	}

    } while (ret == 0);

    return 0;
}
