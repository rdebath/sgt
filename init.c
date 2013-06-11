/*
 * init.c: run as the init program inside a UML instance being
 * used for umlwrap.
 */

#define _XOPEN_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <termios.h>
#include <grp.h>

#include <linux/major.h>

#include "malloc.h"
#include "sel.h"
#include "protocol.h"
#include "rawmode.h"
#include "movefds.h"
#include "fgetline.h"

void fatal(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "umlwrap init: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void control_writeerr(sel_wfd *wfd, int error)
{
    fprintf(stderr, "umlwrap init: control fd: write: %s\n", strerror(errno));
    exit(1);
}

static void control_readerr(sel_rfd *rfd, int error)
{
    fprintf(stderr, "umlwrap init: control fd: read: %s\n", strerror(errno));
    exit(1);
}

typedef enum { DIR_R=1, DIR_W=2, DIR_RW = 3 } rwdir;

struct fd_entry {
    int outerfd;
    rwdir direction;
    char *ptsname;
    int first;			       /* first of an identical group? */
    int innerfd;
    int wflags, rflags;
};

struct fd_assign {
    struct fd_entry *entry;
    int innerfd;
};

struct init_ctx {
    int nunions;
    int rootrw;
    char *root;
    struct fd_entry *fds;
    int nfds, fdsize;
    struct fd_assign *assigns;
    int nassigns, assignsize;
    char **cmdwords;
    int ncmdwords, cmdwordsize;
    char **envvars;
    int nenvvars, envvarsize;
    char **setupcmds;
    int nsetupcmds, setupcmdsize;
    sel *sel;
    sel_rfd *control_rfd;
    sel_wfd *control_wfd;
    int maxfd;
    protoread *pr;
    int ctty;
    char *cttyname;
    uid_t uid;
    gid_t gid;
    gid_t ttygid;
    gid_t *groups;
    int ngroups;
    int child_pid;
    int got_exitcode;
    char *cwd;
    list protocopies;
};

#define WRITEINT(buf, val) do { \
    unsigned long wi_val = (val); \
    unsigned char *wi_buf = (unsigned char *)buf; \
    wi_buf[0] = wi_val >> 24; \
    wi_buf[1] = wi_val >> 16; \
    wi_buf[2] = wi_val >> 8; \
    wi_buf[3] = wi_val; \
} while (0)

int signalpipe[2];
static void sigchld(int sig)
{
    write(signalpipe[1], "C", 1);
}
static void signalpipe_readdata(sel_rfd *rfd, void *vdata, size_t len)
{
    struct init_ctx *ctx = (struct init_ctx *)sel_rfd_get_ctx(rfd);
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	if (pid == ctx->child_pid && (WIFEXITED(pid) || WIFSIGNALED(pid))) {
	    /*
	     * The main child process has terminated. Send its
	     * exit code.
	     */
	    unsigned char intbuf[4];
	    WRITEINT(intbuf, status);
	    protowrite(ctx->control_wfd, CMD_EXITCODE,
		       intbuf, 4, (void *)NULL);

	    ctx->got_exitcode = 1;
	}
    }
}

static int unionmount(struct init_ctx *ctx, void *data, size_t len)
{
    char *s;
    char buf[40];

    s = snewn(len+1, char);
    memcpy(s, data, len);
    s[len] = '\0';

    sprintf(buf, "/tmp/u%d", ctx->nunions);

    if (mkdir(buf, 0666) < 0) {
	char err[512];
	int elen = sprintf(err, "%.40s: mkdir: %.450s",
			   buf, strerror(errno));
	protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	return 0;
    } else if (mount("none", buf, "hostfs", MS_MGC_VAL, s) < 0) {
	char err[512];
	int elen = sprintf(err, "%.220s: mkdir: %.220s",
			   s, strerror(errno));
	protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	rmdir(buf);
	return 0;
    } else {
	ctx->nunions++;
	return 1;
    }
}

static void control_packet(void *vctx, int type, void *data, size_t len)
{
    struct init_ctx *ctx = (struct init_ctx *)vctx;
    char buf[40], *s, *p;
    unsigned long mflags;
    long id;
    int i, ret;

    switch (type) {
      case CMD_UNION:
	printf("got CMD_UNION(%.*s)\n", (int)len, (char *)data);
	unionmount(ctx, data, len);
	break;
      case CMD_ROOTRW:
	printf("got CMD_ROOTRW\n");
	ctx->rootrw = 1;
	break;
      case CMD_ROOT:
	printf("got CMD_ROOT(%.*s)\n", (int)len, (char *)data);
	sprintf(buf, "/tmp/r");
	if (mkdir(buf, 0666) < 0) {
	    char err[512];
	    int elen = sprintf(err, "%.40s: mkdir: %.450s",
			       buf, strerror(errno));
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    break;
	}

	ctx->root = snewn(len+1, char);
	memcpy(ctx->root, data, len);
	ctx->root[len] = '\0';

	mflags = MS_MGC_VAL;
	if (!ctx->rootrw)
	    mflags |= MS_RDONLY;

	if (ctx->nunions > 0) {
	    char *p;
	    const char *sep;
	    int i;

	    if (!unionmount(ctx, data, len))
		break;

	    s = snewn(40 * ctx->nunions + 80, char);
	    p = s;

	    sep = "dirs=";
	    for (i = 0; i < ctx->nunions; i++) {
		p += sprintf(p, "%s/tmp/u%d=r%c", sep, i, i==0?'w':'o');
		sep = ":";
	    }

	    assert(p - s < 40 * ctx->nunions + 80);

	    ret = mount("none", buf, "aufs", mflags, s);
	} else {
	    s = ctx->root;
	    ret = mount("none", buf, "hostfs", mflags, s);
	}

	if (ret < 0) {
	    char err[512];
	    int elen = sprintf(err, "%.220s: mount: %.220s",
			       s, strerror(errno));
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    rmdir(buf);
	}

	break;

      case CMD_WRITABLE:
	printf("got CMD_WRITABLE(%.*s)\n", (int)len, (char *)data);
	if (!ctx->root) {
	    char err[512];
	    int elen = sprintf(err, "CMD_WRITABLE received before CMD_ROOT");
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    break;
	}
	if (len == 0 || ((char *)data)[0] != '/') {
	    char err[512];
	    int elen = sprintf(err, "CMD_WRITABLE directory not absolute");
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    break;
	}
	s = snewn(len + 40 + strlen(ctx->root), char);
	strcpy(s, "/tmp/r");
	strcat(s, ctx->root);
	p = s + strlen(s);
	if (p > s && p[-1] == '/')
	    p--;
	sprintf(p, "%.*s", (int)len, (char *)data);

	if (mount("none", s, "hostfs", MS_MGC_VAL, p) < 0) {
	    char err[512];
	    int elen = sprintf(err, "%.220s: mount: %.220s",
			       s, strerror(errno));
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    rmdir(buf);
	}

	break;

      case CMD_HOSTNAME:
	printf("got CMD_HOSTNAME(%.*s)\n", (int)len, (char *)data);
	if (sethostname(data, len) < 0) {
	    char err[512];
	    int elen = sprintf(err, "sethostname(\"%.*s\"): %.220s",
			       (int)(len<220?len:220), (char *)data,
			       strerror(errno));
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	break;

      case CMD_UID:
	if (len != 4) {
	    char err[512];
	    int elen = sprintf(err, "CMD_UID packet had bad length %d",
			       (int)len);
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	id = ((unsigned char *)data)[0];
	id = (id << 8) | ((unsigned char *)data)[1];
	id = (id << 8) | ((unsigned char *)data)[2];
	id = (id << 8) | ((unsigned char *)data)[3];
	printf("got CMD_UID(%ld)\n", id);
	ctx->uid = id;
	break;

      case CMD_GID:
	if (len != 4) {
	    char err[512];
	    int elen = sprintf(err, "CMD_GID packet had bad length %d",
			       (int)len);
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	id = ((unsigned char *)data)[0];
	id = (id << 8) | ((unsigned char *)data)[1];
	id = (id << 8) | ((unsigned char *)data)[2];
	id = (id << 8) | ((unsigned char *)data)[3];
	printf("got CMD_GID(%ld)\n", id);
	ctx->gid = id;
	break;

      case CMD_TTYGID:
	if (len != 4) {
	    char err[512];
	    int elen = sprintf(err, "CMD_TTYGID packet had bad length %d",
			       (int)len);
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	id = ((unsigned char *)data)[0];
	id = (id << 8) | ((unsigned char *)data)[1];
	id = (id << 8) | ((unsigned char *)data)[2];
	id = (id << 8) | ((unsigned char *)data)[3];
	printf("got CMD_TTYGID(%ld)\n", id);
	ctx->ttygid = id;
	break;

      case CMD_GROUPS:
	if (len % 4 != 0) {
	    char err[512];
	    int elen = sprintf(err, "CMD_GROUPS packet had bad length %d",
			       (int)len);
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	ctx->ngroups = len / 4;
	ctx->groups = snewn(ctx->ngroups, gid_t);
	printf("got CMD_GROUPS(");
	for (i = 0; i < ctx->ngroups; i++) {
	    id = ((unsigned char *)data)[4*i+0];
	    id = (id << 8) | ((unsigned char *)data)[4*i+1];
	    id = (id << 8) | ((unsigned char *)data)[4*i+2];
	    id = (id << 8) | ((unsigned char *)data)[4*i+3];
	    ctx->groups[i] = id;
	    printf("%s%ld", i ? "," : "", id);
	}
	printf(")\n");
	break;

      case CMD_CHDIR:
	printf("got CMD_CHDIR(%.*s)\n", (int)len, (char *)data);
	ctx->cwd = snewn(len+1, char);
	memcpy(ctx->cwd, data, len);
	ctx->cwd[len] = '\0';
	break;

      case CMD_ENVIRON:
	printf("got CMD_ENVIRON(%.*s)\n", (int)len, (char *)data);
	if (ctx->nenvvars >= ctx->envvarsize) {
	    ctx->envvarsize = ctx->nenvvars * 3 / 2 + 64;
	    ctx->envvars = sresize(ctx->envvars, ctx->envvarsize, char *);
	}
	ctx->envvars[ctx->nenvvars] = snewn(len+1, char);
	memcpy(ctx->envvars[ctx->nenvvars], data, len);
	ctx->envvars[ctx->nenvvars][len] = '\0';
	ctx->nenvvars++;
	break;

      case CMD_COMMAND:
	printf("got CMD_COMMAND(%.*s)\n", (int)len, (char *)data);
	if (ctx->ncmdwords >= ctx->cmdwordsize) {
	    ctx->cmdwordsize = ctx->ncmdwords * 3 / 2 + 64;
	    ctx->cmdwords = sresize(ctx->cmdwords, ctx->cmdwordsize, char *);
	}
	ctx->cmdwords[ctx->ncmdwords] = snewn(len+1, char);
	memcpy(ctx->cmdwords[ctx->ncmdwords], data, len);
	ctx->cmdwords[ctx->ncmdwords][len] = '\0';
	ctx->ncmdwords++;
	break;

      case CMD_SETUPCMD:
	printf("got CMD_SETUPCMD(%.*s)\n", (int)len, (char *)data);
	if (ctx->nsetupcmds >= ctx->setupcmdsize) {
	    ctx->setupcmdsize = ctx->nsetupcmds * 3 / 2 + 64;
	    ctx->setupcmds = sresize(ctx->setupcmds, ctx->setupcmdsize,
				     char *);
	}
	ctx->setupcmds[ctx->nsetupcmds] = snewn(len+1, char);
	memcpy(ctx->setupcmds[ctx->nsetupcmds], data, len);
	ctx->setupcmds[ctx->nsetupcmds][len] = '\0';
	ctx->nsetupcmds++;
	break;

      case CMD_PTY:
      case CMD_PTYC:
	printf("got CMD_%s(...)\n", (type == CMD_PTY ? "PTY" : "PTYC"));
	{
	    struct termios termios;
	    struct winsize winsize;
	    int fd;
	    char *name;

	    if (len != sizeof(termios) + sizeof(winsize)) {
		char err[512];
		int elen = sprintf(err, "CMD_PTY message size was %d,"
				   " expected %d", (int)len,
				   (int)(sizeof(termios) + sizeof(winsize)));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if ((fd = open("/dev/ptmx", O_RDWR)) < 0) {
		char err[512];
		int elen = sprintf(err, "/dev/ptmx: open: %.200s",
				   strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if (grantpt(fd) < 0) {
		char err[512];
		int elen = sprintf(err, "/dev/ptmx: grantpt: %.200s",
				   strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if (unlockpt(fd) < 0) {
		char err[512];
		int elen = sprintf(err, "/dev/ptmx: unlockpt: %.200s",
				   strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if ((name = ptsname(fd)) == NULL) {
		char err[512];
		int elen = sprintf(err, "/dev/ptmx: ptsname: %.200s",
				   strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if (chown(name, ctx->uid, ctx->ttygid) < 0) {
		char err[512];
		int elen = sprintf(err, "/dev/ptmx: chown: %.200s",
				   strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    memcpy(&termios, data, sizeof(termios));
	    memcpy(&winsize, data + sizeof(termios), sizeof(winsize));
	    if (tcsetattr(fd, TCSANOW, &termios) < 0) {
		char err[512];
		int elen = sprintf(err, "%.200s: tcsetattr: %.200s",
				   ptsname(fd), strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }
	    if (ioctl(fd, TIOCSWINSZ, &winsize) < 0) {
		char err[512];
		int elen = sprintf(err, "%.200s: ioctl(TIOCSWINSZ): %.200s",
				   ptsname(fd), strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if (ctx->nfds + 3 >= ctx->fdsize) {
		ctx->fdsize = ctx->nfds * 3 / 2 + 64;
		ctx->fds = sresize(ctx->fds, ctx->fdsize, struct fd_entry);
	    }
	    ctx->fds[ctx->nfds].outerfd = fd;
	    ctx->fds[ctx->nfds].ptsname = dupstr(name);
	    ctx->fds[ctx->nfds].direction = DIR_R;
	    ctx->fds[ctx->nfds].first = 1;
	    ctx->fds[ctx->nfds].innerfd = open(name, O_RDONLY | O_NOCTTY);
	    ctx->fds[ctx->nfds].wflags = 0;
	    ctx->fds[ctx->nfds].rflags = 0;
	    ctx->fds[ctx->nfds+1].outerfd = fd;
	    ctx->fds[ctx->nfds+1].ptsname = ctx->fds[ctx->nfds].ptsname;
	    ctx->fds[ctx->nfds+1].direction = DIR_W;
	    ctx->fds[ctx->nfds+1].first = 0;
	    ctx->fds[ctx->nfds+1].innerfd = open(name, O_WRONLY | O_NOCTTY);
	    ctx->fds[ctx->nfds+1].wflags = 0;
	    ctx->fds[ctx->nfds+1].rflags = 0;
	    ctx->fds[ctx->nfds+2].outerfd = fd;
	    ctx->fds[ctx->nfds+2].ptsname = ctx->fds[ctx->nfds].ptsname;
	    ctx->fds[ctx->nfds+2].direction = DIR_RW;
	    ctx->fds[ctx->nfds+2].first = 0;
	    ctx->fds[ctx->nfds+2].innerfd = open(name, O_RDWR | O_NOCTTY);
	    ctx->fds[ctx->nfds+2].wflags = 0;
	    ctx->fds[ctx->nfds+2].rflags = 0;
	    if (ctx->maxfd < ctx->fds[ctx->nfds].outerfd)
		ctx->maxfd = ctx->fds[ctx->nfds].outerfd;
	    if (ctx->maxfd < ctx->fds[ctx->nfds].innerfd)
		ctx->maxfd = ctx->fds[ctx->nfds].innerfd;
	    if (ctx->maxfd < ctx->fds[ctx->nfds+1].innerfd)
		ctx->maxfd = ctx->fds[ctx->nfds+1].innerfd;
	    if (ctx->maxfd < ctx->fds[ctx->nfds+2].innerfd)
		ctx->maxfd = ctx->fds[ctx->nfds+2].innerfd;

	    if (type == CMD_PTYC) {
		ctx->ctty = ctx->fds[ctx->nfds].outerfd;
		ctx->cttyname = ctx->fds[ctx->nfds].ptsname;
	    }

	    ctx->nfds += 3;

	    break;
	}
	break;

      case CMD_IPIPE:
      case CMD_OPIPE:
      case CMD_IOPIPE:
	printf("got CMD_%s\n",
	       (type == CMD_IPIPE ? "IPIPE" :
		type == CMD_OPIPE ? "OPIPE" : "IOPIPE"));
	{
	    int fds[2];
	    int outerindex = (type == CMD_IPIPE ? 1 : 0);
	    rwdir dir;

	    if (type == CMD_IOPIPE) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
		    char err[512];
		    int elen = sprintf(err, "socketpair: %.200s",
				       strerror(errno));
		    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			       (void *)NULL);
		    break;
		}
		dir = DIR_RW;
	    } else {
		if (pipe(fds) < 0) {
		    char err[512];
		    int elen = sprintf(err, "pipe: %.200s", strerror(errno));
		    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			       (void *)NULL);
		    break;
		}
		dir = (type == CMD_IPIPE ? DIR_R : DIR_W);
	    }

	    if (ctx->nfds >= ctx->fdsize) {
		ctx->fdsize = ctx->nfds * 3 / 2 + 64;
		ctx->fds = sresize(ctx->fds, ctx->fdsize, struct fd_entry);
	    }
	    ctx->fds[ctx->nfds].outerfd = fds[outerindex];
	    ctx->fds[ctx->nfds].ptsname = NULL;
	    ctx->fds[ctx->nfds].direction = dir;
	    ctx->fds[ctx->nfds].first = 1;
	    ctx->fds[ctx->nfds].innerfd = fds[1-outerindex];
	    ctx->fds[ctx->nfds].wflags = 0;
	    ctx->fds[ctx->nfds].rflags = 0;
	    if (type == CMD_IPIPE)
		ctx->fds[ctx->nfds].wflags = PROTOCOPY_CLOSE_WFD;
	    else if (type == CMD_IOPIPE)
		ctx->fds[ctx->nfds].wflags = PROTOCOPY_SHUTDOWN_WFD;
	    else if (type == CMD_OPIPE)
		ctx->fds[ctx->nfds].wflags = PROTOCOPY_CLOSE_RFD;
	    ctx->nfds++;
	    if (ctx->maxfd < ctx->fds[ctx->nfds].outerfd)
		ctx->maxfd = ctx->fds[ctx->nfds].outerfd;
	    if (ctx->maxfd < ctx->fds[ctx->nfds].innerfd)
		ctx->maxfd = ctx->fds[ctx->nfds].innerfd;
	    break;
	}
	break;

      case CMD_ISERIAL:
      case CMD_OSERIAL:
	{
	    int fdindex, serindex, serfd;
	    char sername[64];

	    if (len != 8) {
		char err[512];
		int elen = sprintf(err, "CMD_%cSERIAL packet had bad length"
				   " %d", (type==CMD_ISERIAL?'I':'O'),
				   (int)len);
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    serindex = ((unsigned char *)data)[0];
	    serindex = (serindex << 8) | ((unsigned char *)data)[1];
	    serindex = (serindex << 8) | ((unsigned char *)data)[2];
	    serindex = (serindex << 8) | ((unsigned char *)data)[3];
	    fdindex = ((unsigned char *)data)[4];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[5];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[6];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[7];
	    printf("got CMD_%s(%d,%d)\n",
		   (type == CMD_ISERIAL ? "ISERIAL" : "OSERIAL"),
		   serindex, fdindex);

	    if (fdindex < 0 || fdindex >= ctx->nfds) {
		char err[512];
		int elen = sprintf(err, "CMD_%cSERIAL packet had bad fd index"
				   " %d (max %d)", (type==CMD_ISERIAL?'I':'O'),
				   fdindex, ctx->nfds);
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    sprintf(sername, "/dev/ttyS%d", serindex);
	    if (mknod(sername, S_IFCHR | 0700,
		      makedev(TTY_MAJOR, 64+serindex)) < 0 &&
		errno != EEXIST /* it's OK if we've made it already */) {
		char err[512];
		int elen = sprintf(err, "%.200s: mknod: %.200s",
				   sername, strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if ((serfd = open(sername,
			      (type==CMD_ISERIAL?O_RDONLY:O_WRONLY))) < 0) {
		char err[512];
		int elen = sprintf(err, "%.200s: open: %.200s",
				   sername, strerror(errno));
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    raw_mode(serfd, NULL, NULL);
	    if (ctx->maxfd < serfd)
		ctx->maxfd = serfd;

	    if (type == CMD_ISERIAL) {
		list_add(&ctx->protocopies, (listnode *)
			 protocopy_decode_new(ctx->sel, serfd,
					      ctx->fds[fdindex].outerfd,
					      ctx->fds[fdindex].wflags));
	    } else {
		list_add(&ctx->protocopies, (listnode *)
			 protocopy_encode_new(ctx->sel,
					      ctx->fds[fdindex].outerfd,
					      serfd,
					      ctx->fds[fdindex].rflags));
	    }
	}
	break;

      case CMD_ASSIGNFD:
	{
	    int fdindex, innerfd;

	    if (len != 8) {
		char err[512];
		int elen = sprintf(err, "CMD_%cSERIAL packet had bad length"
				   " %d", (type==CMD_ISERIAL?'I':'O'),
				   (int)len);
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    innerfd = ((unsigned char *)data)[0];
	    innerfd = (innerfd << 8) | ((unsigned char *)data)[1];
	    innerfd = (innerfd << 8) | ((unsigned char *)data)[2];
	    innerfd = (innerfd << 8) | ((unsigned char *)data)[3];
	    fdindex = ((unsigned char *)data)[4];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[5];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[6];
	    fdindex = (fdindex << 8) | ((unsigned char *)data)[7];

	    printf("got CMD_ASSIGNFD(%d,%d)\n", innerfd, fdindex);

	    if (ctx->maxfd < innerfd)
		ctx->maxfd = innerfd;

	    if (fdindex < 0 || fdindex >= ctx->nfds) {
		char err[512];
		int elen = sprintf(err, "CMD_%cSERIAL packet had bad fd index"
				   " %d (max %d)", (type==CMD_ISERIAL?'I':'O'),
				   fdindex, ctx->nfds);
		protowrite(ctx->control_wfd, CMD_FAILURE, err, elen,
			   (void *)NULL);
		break;
	    }

	    if (ctx->nassigns >= ctx->assignsize) {
		ctx->assignsize = ctx->nassigns * 3 / 2 + 64;
		ctx->assigns = sresize(ctx->assigns, ctx->assignsize,
				       struct fd_assign);
	    }
	    ctx->assigns[ctx->nassigns].entry = &ctx->fds[fdindex];
	    ctx->assigns[ctx->nassigns].innerfd = innerfd;
	    ctx->nassigns++;
	}
	break;

      case CMD_GO:
	printf("got CMD_GO\n");
	/*
	 * Before we fork, some cleanups. First, stick a NULL on
	 * the end of the command so that it's proper execvp
	 * input.
	 */
	if (ctx->ncmdwords >= ctx->cmdwordsize) {
	    ctx->cmdwordsize = ctx->ncmdwords * 3 / 2 + 64;
	    ctx->cmdwords = sresize(ctx->cmdwords, ctx->cmdwordsize, char *);
	}
	ctx->cmdwords[ctx->ncmdwords] = NULL;

	/*
	 * Unilaterally try to mount some dynamic filesystems
	 * under the new root. We won't worry if they don't work,
	 * though: the user is entitled to make up crazy root fs
	 * structures if they must.
	 */
	mount("none", "/tmp/r/proc", "proc", MS_MGC_VAL, NULL);
	mount("none", "/tmp/r/sys", "sysfs", MS_MGC_VAL, NULL);
	mount("none", "/tmp/r/tmp", "tmpfs", MS_MGC_VAL, NULL);
	mount("none", "/tmp/r/var/tmp", "tmpfs", MS_MGC_VAL, NULL);
	if (mount("none", "/tmp/r/dev", "tmpfs", MS_MGC_VAL, NULL) >= 0) {
	    chmod("/tmp/r/dev", 0755);
	    mode_t oldumask = umask(0);
	    mknod("/tmp/r/dev/console", S_IFCHR|0700, makedev(TTYAUX_MAJOR,1));
	    symlink("/proc/kcore", "/tmp/r/dev/core");
	    symlink("/proc/self/fd", "/tmp/r/dev/fd");
	    mknod("/tmp/r/dev/full", S_IFCHR|0666, makedev(MEM_MAJOR,7));
	    mknod("/tmp/r/dev/kmem", S_IFCHR|0600, makedev(MEM_MAJOR,2));
	    mknod("/tmp/r/dev/kmsg", S_IFCHR|0600, makedev(MEM_MAJOR,11));
	    for (i = 0; i < 8; i++) {
		char buf[40];
		sprintf(buf, "/tmp/r/dev/loop%d", i);
		mknod(buf, S_IFBLK|0600, makedev(LOOP_MAJOR,i));
	    }
	    mknod("/tmp/r/dev/kmem", S_IFCHR|0600, makedev(MEM_MAJOR,1));
	    mknod("/tmp/r/dev/null", S_IFCHR|0666, makedev(MEM_MAJOR,3));
	    mknod("/tmp/r/dev/port", S_IFCHR|0600, makedev(MEM_MAJOR,4));
	    mknod("/tmp/r/dev/ptmx", S_IFCHR|0666, makedev(TTYAUX_MAJOR,2));
	    if (mkdir("/tmp/r/dev/pts", 0755) >= 0)
		mount("none", "/tmp/r/dev/pts", "devpts", MS_MGC_VAL, NULL);
	    for (i = 0; i < 16; i++) {
		char buf[40];
		sprintf(buf, "/tmp/r/dev/ram%d", i);
		mknod(buf, S_IFBLK|0600, makedev(RAMDISK_MAJOR,i));
	    }
	    mknod("/tmp/r/dev/random", S_IFCHR|0666, makedev(MEM_MAJOR,8));
	    if (mkdir("/tmp/r/dev/shm", S_ISVTX|0777) >= 0)
		mount("none", "/tmp/r/dev/shm", "tmpfs", MS_MGC_VAL, NULL);
	    symlink("/proc/self/fd/0", "/tmp/r/dev/stdin");
	    symlink("/proc/self/fd/1", "/tmp/r/dev/stdout");
	    symlink("/proc/self/fd/2", "/tmp/r/dev/stderr");
	    mknod("/tmp/r/dev/tty", S_IFCHR|0666, makedev(TTYAUX_MAJOR,0));
	    mknod("/tmp/r/dev/urandom", S_IFCHR|0666, makedev(MEM_MAJOR,9));
	    mknod("/tmp/r/dev/zero", S_IFCHR|0666, makedev(MEM_MAJOR,5));
	    umask(oldumask);
	}

	/*
	 * Actually fork and start doing things!
	 */
	ctx->child_pid = fork();
	if (ctx->child_pid < 0) {
	    char err[512];
	    int elen = sprintf(err, "fork: %.200s", strerror(errno));
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	    break;
	}

	if (ctx->child_pid == 0) {
	    int fd, i, fdmaplen, *fdmap;

	    /*
	     * We are the child. Set up the execution environment
	     * for the wrapped process.
	     */

	    /* Set our controlling tty */
	    setsid();
	    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY);
		close(fd);
	    }
	    if (ctx->cttyname && (fd = open(ctx->cttyname, O_RDWR)) > 0) {
		ioctl(fd, TIOCSCTTY);
		close(fd);
	    }

	    /* Set up the fds */
	    fdmaplen = 1 + ctx->maxfd;
	    fdmap = snewn(fdmaplen, int);
	    for (i = 0; i < fdmaplen; i++)
		fdmap[i] = -1;
	    for (i = 0; i < ctx->nassigns; i++) {
		fdmap[ctx->assigns[i].innerfd] =
		    ctx->assigns[i].entry->innerfd;
	    }
	    if (move_fds(0, fdmap, fdmaplen, NULL) < 0)
		exit(127);

	    if (chroot("/tmp/r") < 0) {
		fprintf(stderr, "/tmp/r: chroot: %s\n", strerror(errno));
		exit(127);
	    }

	    if (chdir(ctx->cwd) < 0) {
		fprintf(stderr, "%s: chdir: %s\n", ctx->cwd, strerror(errno));
		exit(127);
	    }

	    /*
	     * Run the CMD_SETUPCMD command list after setting up
	     * everything we can set up without dropping
	     * privileges, but before actually dropping
	     * privileges. So these commands will execute in the
	     * final fs layout, with the final fd and tty setup,
	     * and in the correct directory. All they lack is the
	     * uid (because they must be root) and the
	     * environment.
	     */
	    for (i = 0; i < ctx->nsetupcmds; i++)
		system(ctx->setupcmds[i]);

	    if (setgroups(ctx->ngroups, ctx->groups) < 0) {
		fprintf(stderr, "setgroups: %s\n", strerror(errno));
		exit(127);
	    }
	    if (setgid(ctx->gid) < 0) {
		fprintf(stderr, "setgid(%d): %s\n", (int)ctx->gid,
			strerror(errno));
		exit(127);
	    }
	    if (setuid(ctx->uid) < 0) {
		fprintf(stderr, "setuid(%d): %s\n", (int)ctx->uid,
			strerror(errno));
		exit(127);
	    }

	    /*
	     * Set up the environment. It would be nice to do this
	     * using the exec call, but we can't, for two reasons:
	     * 	(a) execvpe does not exist. All the library exec
	     * 	    functions do at most one of searching $PATH
	     * 	    and letting you provide an envp argument; none
	     * 	    does both.
	     * 	(b) in any case, we want to _use_ the new $PATH
	     * 	    when looking up the command to execute in the
	     * 	    first place!
	     */
	    clearenv();
	    for (i = 0; i < ctx->nenvvars; i++)
		putenv(ctx->envvars[i]);

	    if (execvp(ctx->cmdwords[0], ctx->cmdwords) < 0)
		fprintf(stderr, "%s: execvp: %s\n", ctx->cmdwords[0],
			strerror(errno));
	    exit(127);
	}

	/*
	 * As the parent process, close our ends of all the pipes
	 * and things we passed to the child process, so that
	 * we'll notice if the child closes them.
	 */
	for (i = 0; i < ctx->nfds; i++)
	    close(ctx->fds[i].innerfd);

	protowrite(ctx->control_wfd, CMD_GONE, (void *)NULL);
	break;

      case CMD_WINSIZE:
	/* We don't debug this, because it'll interrupt the session
	 * on the far side. */
	/* printf("got CMD_WINSIZE\n"); */
	if (ctx->ctty >= 0) {
	    struct winsize winsize;
	    if (len == sizeof(winsize)) {
		memcpy(&winsize, data, len);
		ioctl(ctx->ctty, TIOCSWINSZ, &winsize);
	    }
	}
	break;

      default:
	{
	    char err[512];
	    int elen = sprintf(err, "Unexpected control command %d", type);
	    protowrite(ctx->control_wfd, CMD_FAILURE, err, elen, (void *)NULL);
	}
	break;
    }
}

static void control_readdata(sel_rfd *rfd, void *vdata, size_t len)
{
    struct init_ctx *ctx = (struct init_ctx *)sel_rfd_get_ctx(rfd);

    protoread_data(ctx->pr, vdata, len, control_packet, ctx);
}

/*
 * String processing function for untangling /proc/mounts.
 * /proc/mounts consists of a fixed number of space-separated
 * words, but some of those words represent arbitrary strings such
 * as VFS pathnames. Hence there's an escaping mechanism to handle
 * those.
 * 
 * This function takes a word starting at p, advances to the next
 * word, and returns the new pointer; it also modifies the data at
 * p to unescape the word in question.
 */
static char *procmounts_advance(char *p)
{
    char *q = p;

    while (*p && *p != ' ') {
	if (*p == '\\') {
	    int d, c = 0;
	    p++;
	    for (d = 0; d < 3; d++) {
		if (*p && *p >= '0' && *p <= '7') {
		    c = 8*c + (*p) - '0';
		    p++;
		} else {
		    break;
		}
	    }
	    *q++ = c;
	} else {
	    *q++ = *p++;
	}
    }
    if (*p)
	p++;
    *q = '\0';
    return p;
}

int main(void)
{
    sel *sel;
    struct init_ctx *ctx;
    int controlfd;
    int consolefd;
    int ret;
    int shut_down;

    /*
     * We expect to have been initialised in a trivial root
     * filesystem consisting only of empty directories called /tmp
     * and /dev, and a directory called /sbin containing the
     * binary of this process (which we can ignore).
     * 
     * This filesystem is read-only, so we begin first of all by
     * mounting tmpfses over /tmp and /dev. Then we can mknod
     * /dev/console, which should enable us to give debugging
     * output.
     */
    if (mount("none", "/tmp", "tmpfs", MS_MGC_VAL, NULL) < 0 ||
	mount("none", "/dev", "tmpfs", MS_MGC_VAL, NULL) < 0 ||
	mount("none", "/proc", "proc", MS_MGC_VAL, NULL) < 0 ||
	mknod("/dev/console", S_IFCHR | 0700, makedev(TTYAUX_MAJOR, 1)) < 0 ||
	mknod("/dev/ptmx", S_IFCHR | 0700, makedev(TTYAUX_MAJOR, 2)) < 0 ||
	mkdir("/dev/pts", 0755) < 0 ||
	mount("none", "/dev/pts", "devpts", MS_MGC_VAL, NULL) < 0 ||
	(consolefd = open("/dev/console", O_RDWR)) < 0)
	return 1;		  /* nothing to do but cause a kernel panic */
    if (consolefd != 0 && dup2(consolefd, 0) < 0)
	return 1;
    if (consolefd != 1 && dup2(consolefd, 1) < 0)
	return 1;
    if (consolefd != 2 && dup2(consolefd, 2) < 0)
	return 1;
    if (consolefd != 0 && consolefd != 1 && consolefd != 2)
	close(consolefd);

    printf("umlwrap init: started\n");

    /*
     * Now we can create device nodes for the serial ports we'll
     * need to read from. Immediately create the first one,
     * because that's the control channel from the umlwrap process
     * on the outside.
     */
    if (mknod("/dev/ttyS0", S_IFCHR | 0700, makedev(TTY_MAJOR, 64)) < 0) {
	perror("umlwrap init: mknod /dev/ttyS0");
	return 1;
    }

    controlfd = open("/dev/ttyS0", O_RDWR);
    if (controlfd < 0) {
	perror("umlwrap init: open /dev/ttyS0");
	return 1;
    }
    raw_mode(controlfd, NULL, NULL);

    printf("umlwrap init: opened control channel\n");

    /*
     * Now begin setting up a select loop.
     */
    ctx = snew(struct init_ctx);
    sel = sel_new(ctx);
    ctx->nunions = 0;
    ctx->rootrw = 0;
    ctx->root = NULL;
    ctx->fds = NULL;
    ctx->nfds = ctx->fdsize = 0;
    ctx->assigns = NULL;
    ctx->nassigns = ctx->assignsize = 0;
    ctx->cmdwords = NULL;
    ctx->ncmdwords = ctx->cmdwordsize = 0;
    ctx->envvars = NULL;
    ctx->nenvvars = ctx->envvarsize = 0;
    ctx->setupcmds = NULL;
    ctx->nsetupcmds = ctx->setupcmdsize = 0;
    ctx->pr = protoread_new();
    ctx->sel = sel;
    ctx->ctty = -1;
    ctx->cttyname = NULL;
    ctx->uid = 1;		       /* *shrug* */
    ctx->gid = 1;		       /* *shrug* */
    ctx->ttygid = 0;
    ctx->groups = NULL;
    ctx->ngroups = 0;
    list_init(&ctx->protocopies);

    assert(controlfd > 2);
    ctx->maxfd = controlfd;

    ctx->control_rfd = sel_rfd_add(sel, controlfd, control_readdata,
				   control_readerr, ctx);
    ctx->control_wfd = sel_wfd_add(sel, controlfd, NULL,
				   control_writeerr, ctx);

    /*
     * Set up a means of bringing SIGCHLD back to the main
     * program.
     */
    if (pipe(signalpipe) < 0) {
	perror("umlwrap init: pipe");
	return 1;
    }
    sel_rfd_add(sel, signalpipe[0], signalpipe_readdata, NULL, ctx);
    signal(SIGCHLD, sigchld);

    /*
     * Let the outside wrapper know we've set ourselves up.
     */
    protowrite(ctx->control_wfd, CMD_HELLO, (void *)NULL);

    /*
     * Now we enter the main select loop.
     */
    shut_down = 0;
    ctx->got_exitcode = 0;
    do {
	ret = sel_iterate(sel, -1);

	if (ctx->got_exitcode && !shut_down) {
	    listnode *node, *tmp;

	    ret = sel_iterate(sel, 0);
	    /*
	     * The child process has terminated, and we've done an
	     * extra iteration of the select loop so that we know
	     * we've read everything it had to send. Now we stop
	     * sending and receiving data to the outside world: we
	     * close down all our incoming protocopies, and we
	     * artificially send EOF on all our data streams to
	     * the outside world.
	     */
	    LOOP_OVER_LIST(node, tmp, &ctx->protocopies) {
		if (node->type == LISTNODE_PROTOCOPY_ENCODE) {
		    protocopy_encode *pe = (protocopy_encode *)node;
		    protocopy_encode_cutoff(pe);
		} else if (node->type == LISTNODE_PROTOCOPY_DECODE) {
		    protocopy_decode *pd = (protocopy_decode *)node;
		    protocopy_decode_destroy(pd);
		}
	    }
	    shut_down = 1;	       /* so we only do this once */

	    /*
	     * Also, while we're here, we forcibly umount or
	     * remount as RO any filesystem which is currently
	     * mounted RW, and then we sync. This means that if
	     * the setupcmds have mounted strange filesystems,
	     * they should automatically be properly cleaned up on
	     * exit.
	     */
	    while (1) {
		FILE *fp;
		char *line;
		int done_one = 0;

		fp = fopen("/proc/mounts", "r");
		while (fp && (line = fgetline(fp)) != NULL) {
		    char *p, *dev, *dir, *fs, *opts;
		    p = line;
		    p[strcspn(p, "\r\n")] = '\0';
		    dev = p; p = procmounts_advance(p);
		    dir = p; p = procmounts_advance(p);
		    fs = p; p = procmounts_advance(p);
		    opts = p; p = procmounts_advance(p);
		    if (strcmp(fs, "rootfs") != 0 &&
			strcmp(fs, "proc") != 0 &&
			strcmp(fs, "tmpfs") != 0 &&
			strcmp(fs, "sysfs") != 0 &&
			strcmp(fs, "devpts") != 0 &&
			strcmp(fs, "hostfs") != 0 &&
			strstr(opts, "rw")) {
			int ret;

			ret = umount(dir);
			if (ret >= 0) {
			    printf("umount of %s successful\n", dir);
			    done_one = 1;
			} else {
			    ret = mount(dev, dir, fs, MS_MGC_VAL |
					MS_REMOUNT | MS_RDONLY, NULL);
			    if (ret < 0) {
				printf("read-only remount of %s on %s: %s\n",
				       dev, dir, strerror(errno));
			    } else {
				printf("read-only remount of %s on %s "
				       "successful\n", dev, dir);
				done_one = 1;
			    }
			}
		    }
		    sfree(line);
		}

		/*
		 * umounting one directory may free up a
		 * previously blocked umount, so go round again
		 * for as long as we're achieving something.
		 */
		if (!done_one)
		    break;
	    }

	    sync();
	}

    } while (ret == 0);

    return 0;
}
