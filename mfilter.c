/*
 * mfilter.c   filter on a Mono connection, to automatically log in
 */

#define _GNU_SOURCE
#include <features.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/signal.h>

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

enum { USER, PASS, TERM, NVALUES };
char strings[NVALUES][100];
int gotstr[NVALUES];

int rfd, wfd;			       /* read and write fds */

static void run_connection(void);

static int expect (char *, ...);
static void wstring (char *);
static void wdata(int, void *, int);
static int readconn(void *, int);
static void acquire_password(void);
static void (*sane_signal(int sig, void (*func)(int)))(int);

FILE *dfp;

char *cargs[50];
int cnargs = 0;

char *passprogs[10];
int npassprogs = 0;

int use_pipes = 0;

struct termios oldattrs, newattrs;
void termios_cleanup(void)
{
    tcsetattr(0, TCSANOW, &oldattrs);
}

int main(int ac, char **av) {
    int stat;
    char *initcode = NULL;
    FILE *fp;
    char buffer[1024];
    int opts = 1;

    while (--ac) {
	char *p = *++av;
	char c, *v;

	if (opts && *p == '-' && p[1]) {
	    if (!strcmp(p, "--")) {
		opts = 0;
	    } else switch (c = *++p) {
	      case 'U': case 'P': case 'T':
		/* options which take an argument */
		v = p[1] ? p+1 : --ac ? *++av : NULL;
		if (!v) {
		    fprintf(stderr, "mfilter: option `-%c' requires an argument\n",
			    c);
		    return 1;
		}
		switch (c) {
		  case 'U':		       /* change user name */
		    strncpy(strings[USER], v, lenof(strings[USER])-1);
		    strings[USER][lenof(strings[USER])-1] = '\0';
		    gotstr[USER] = 1;
		    break;
		  case 'T':		       /* change term type */
		    strncpy(strings[TERM], v, lenof(strings[TERM])-1);
		    strings[TERM][lenof(strings[TERM])-1] = '\0';
		    gotstr[TERM] = 1;
		    break;
		  case 'P':		       /* change default pw - read from file */
		    assert(npassprogs < lenof(passprogs));
		    passprogs[npassprogs++] = v;
		    break;
		}
		break;
	      case 'p':
		use_pipes = 1;
		break;
	      default:
		fprintf(stderr, "mfilter: unrecognised option `-%c'\n", *p);
		break;
	    }
	} else {
	    opts = 0;
	    assert(cnargs < lenof(cargs));
	    cargs[cnargs++] = p;
	}
    }

    if (!cargs) {
	fprintf(stderr, "usage: mfilter [-Uname] [-ppassprog [...]]"
		" [-Tterm] <connectprogram> <args>\n");
	return (ac != 1);
    }

    if (npassprogs)
	acquire_password();

    putenv("TERM=unknown");

    run_connection();

    tcgetattr(0, &oldattrs);
    atexit(termios_cleanup);
    newattrs = oldattrs;
    newattrs.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR);
    newattrs.c_lflag &= ~(ISIG | ICANON | ECHO);
    tcsetattr(0, TCSADRAIN, &newattrs);

    while (1) {
	int i = expect("\nlogin: ",
		       (gotstr[USER] ? "Account : " : ""),
		       (gotstr[PASS] ? "Password : " : ""),
		       "[Y]/[N] ?",
		       (gotstr[TERM] ? "Terminal : " : ""),
		       NULL);
	if (i == 4)
	    break;

	switch (i) {
	  case 0:
	    wstring ("mono\r");
	    if (dfp) fprintf(dfp, "Logged in as `mono'\n");
	    break;
	  case 1:
	    wstring (strings[USER]);
	    wstring ("\r");
	    if (dfp) fprintf(dfp, "Sent user name `%s'\n", strings[USER]);
	    break;
	  case 2:
	    wstring (strings[PASS]);
	    memset(strings[PASS], 0, sizeof(strings[PASS]));
	    wstring ("\r");
	    if (dfp) fprintf(dfp, "Sent password\n");
	    break;
	  case 3:
	    wstring ("n");
	    if (dfp) fprintf(dfp, "Declined to disconnect other satellite(s)\n");
	    break;
	}
    }

    wstring ("\001\013");
    wstring(strings[TERM]);
    wstring("\r");
    if (dfp) fprintf(dfp, "Sent terminal type\n");

    /*
     * Now just copy data back and forth until done.
     */
    {
	char buf[65536];
	while (readconn(buf, 65536) > 0);
    }

    wait (&stat);
    if (WIFSIGNALED(stat)) {
	fprintf(stderr, "mfilter: child exited with signal %d\n",
		WTERMSIG(stat));
	return 1;
    }
    if (WIFEXITED(stat)) {
	return WEXITSTATUS(stat);
    }

    return 0;
}

static void acquire_password(void)
{
    FILE *fp;
    int i, j, ret;
    char passfrags[lenof(passprogs)][100];
    int passlen = -1;

    for (i = 0; i < npassprogs; i++) {
	fp = popen(passprogs[i], "r");
	if (!fp) {
	    fprintf(stderr, "mfilter: unable to get password fragment `%s'\n",
		    passprogs[i]);
	    break;
	}
	ret = fread(passfrags[i], 1, lenof(passfrags[i]), fp);
	fclose(fp);

	if (ret <= 0) {
	    fprintf(stderr, "mfilter: no data in password fragment from `%s'\n",
		    passprogs[i]);
	    break;
	}

	if (passlen < 0)
	    passlen = ret;
	else if (passlen != ret) {
	    fprintf(stderr, "mfilter: password fragments `%s' and `%s' are"
		    " different lengths\n", passprogs[0], passprogs[i]);
	}
    }

    if (i == npassprogs) {
	/*
	 * Got them all. Combine them.
	 */

	for (i = 0; i < passlen; i++) {
	    strings[PASS][i] = 0;
	    for (j = 0; j < npassprogs; j++)
		strings[PASS][i] ^= passfrags[j][i];
	}
	gotstr[PASS] = 1;
    }

    memset(passfrags, 0, sizeof(passfrags));
}

static void sigwinch_handler(int sig)
{
    struct winsize size;
    if (ioctl(0, TIOCGWINSZ, (void *)&size) >= 0)
	ioctl(rfd, TIOCSWINSZ, (void *)&size);
}

static void run_connection(void) {
    int r[2], w[2];
    int pid;
    char slavename[FILENAME_MAX];

    if (use_pipes) {

	if (pipe(r) < 0 || pipe(w) < 0) {
	    perror("mfilter: pipe");
	    exit(1);
	}

	rfd = r[0];
	wfd = w[1];

    } else {

	rfd = wfd = pty_get(slavename);

    }

    pid = fork();
    if (pid < 0) {
	perror("mfilter: fork");
	exit(1);
    } else if (pid == 0) {
	/*
	 * we're the child; shift our fds around and then exec the
	 * connection program
	 */
	close(0);
	close(1);
	if (use_pipes) {
	    close(r[0]);
	    close(w[1]);
	    dup2 (r[1], 1);
	    dup2 (w[0], 0);
	    close(r[1]);
	    close(w[0]);
	} else {
	    int slavefd = open(slavename, O_RDWR);
	    int i;

	    if (slavefd < 0) {
		perror("slave pty: open");
		_exit(1);
	    }
	    close(rfd);
	    fcntl(slavefd, F_SETFD, 0);    /* don't close on exec */
	    dup2(slavefd, 0);
	    dup2(slavefd, 1);
	    setsid();
	    ioctl(slavefd, TIOCSCTTY, 1);
	    /* Close everything _else_, for tidiness. */
	    for (i = 3; i < 1024; i++)
		close(i);
	}
	cargs[cnargs] = NULL;
	execvp(cargs[0], cargs);
	perror("mfilter: exec");
	exit(1);
    }

    if (use_pipes) {
	close(r[1]);
	close(w[0]);
    }
    sane_signal(SIGWINCH, sigwinch_handler);
}

/*
 * The constant task of this utility is to funnel stdin to the
 * connection's input and funnel the connection's output to stdout.
 * So here's a routine which does that, and at the same time acts
 * as a `read' on the connection, so that expect() can work with
 * it.
 */
static int readconn(void *buf, int len)
{
    fd_set rfds;
    int ret;

    while (1) {

	FD_ZERO(&rfds);
	FD_SET(rfd, &rfds);
	FD_SET(0, &rfds);

	do {
	    ret = select(rfd+1, &rfds, NULL, NULL, NULL);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
	    perror("mfilter: select");
	    exit(1);
	}

	if (FD_ISSET(0, &rfds)) {

	    do {
		ret = read(0, buf, len);
	    } while (ret < 0 && errno == EINTR);

	    wdata(wfd, buf, ret);
	}
	if (FD_ISSET(rfd, &rfds)) {
	    do {
		ret = read(rfd, buf, ret);
	    } while (ret < 0 && errno == EINTR);

	    if (!use_pipes && ret < 0 && errno == EIO) {
		/* For some reason EIO is returned from ptys on clean exit. */
		ret = 0;
	    }
	    wdata(0, buf, ret);
	    return ret;
	}
    }
}

/*
 * expect and wstring: a very simple request/response analyser. Not
 * used in the main Mono code.
 */

static int expect (char *s1, ...) {
    char buf[1024];
    char *strings[8];		       /* 8 ought to be enough */
    int returns[8];
    int nstr, slen = 0;
    int ret = 1;
    int len, pos = 0;
    va_list ap;

    nstr = 0;
    if (*s1)
	strings[nstr] = s1, returns[nstr++] = 0;
    va_start (ap, s1);
    while ( (strings[nstr] = va_arg(ap, char *)) != NULL ) {
	if (slen < strlen(strings[nstr]))
	    slen = strlen(strings[nstr]);
	if (*strings[nstr])	       /* empty strings are ignored */
	    returns[nstr++] = ret;
	ret++;
    }

    while ( (len = readconn(buf+pos, sizeof(buf)-pos)) > 0 ) {
	while (len--) {
	    int i;
	    pos++;
	    for (i=0; i<nstr; i++) {
		if (pos >= strlen(strings[i]) &&
		    !memcmp(buf+pos-strlen(strings[i]),
			    strings[i], strlen(strings[i])))
		    return returns[i]; /* got it */
	    }
	}
	if (pos > slen) {
	    memmove(buf, buf+pos-slen, slen);
	    pos = slen;
	}
    }

    if (!len) {
	fprintf(stderr, "mfilter: unexpected EOF while waiting"
		" for login prompts\n");
	exit(1);
    } else if (len < 0) {
	perror("mfilter: read");
	exit(1);
    }
    return -1;			       /* panic */
}

static void wdata(int fd, void *sv, int len)
{
    char *s = (char *)sv;
    int written;
    while ( (written = write (fd, s, len)) > 0)
	s += written, len -= written;
    if (len < 0) {
	perror("mfilter: write");
	exit(1);
    }
}

static void wstring (char *s) {
    wdata(wfd, s, strlen(s));
}

/*
 * Allocate a pty. Returns a file handle to the master end of the
 * pty, and stores the full pathname to the slave end into `name'.
 * `name' is a buffer of size at least FILENAME_MAX.
 * 
 * Does not return on error.
 */
int pty_get(char *name)
{
    int fd;

    fd = open("/dev/ptmx", O_RDWR);

    if (fd < 0) {
	perror("/dev/ptmx: open");
	exit(1);
    }

    if (grantpt(fd) < 0) {
	perror("grantpt");
	exit(1);
    }
    
    if (unlockpt(fd) < 0) {
	perror("unlockpt");
	exit(1);
    }

    name[FILENAME_MAX-1] = '\0';
    strncpy(name, ptsname(fd), FILENAME_MAX-1);

    return fd;
}

/*
 * Calling signal() is a non-portable, as it varies in meaning between
 * platforms and depending on feature macros, and has stupid semantics
 * at least some of the time.
 *
 * This function provides the same interface as the libc function, but
 * provides consistent semantics.  It assumes POSIX semantics for
 * sigaction() (so you might need to do some more work if you port to
 * something ancient like SunOS 4)
 */
static void (*sane_signal(int sig, void (*func)(int)))(int) {
    struct sigaction sa;
    struct sigaction old;
    
    sa.sa_handler = func;
    if(sigemptyset(&sa.sa_mask) < 0)
	return SIG_ERR;
    sa.sa_flags = SA_RESTART;
    if(sigaction(sig, &sa, &old) < 0)
	return SIG_ERR;
    return old.sa_handler;
}
