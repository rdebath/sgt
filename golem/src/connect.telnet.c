/*
 * tn.c   telnet protocol implementation as client
 *
 * prototype: always opens localhost:23
 */

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef int SOCKET;
#define SOCKET_ERROR -1

static SOCKET s;

#define	IAC	255		/* interpret as command: */
#define	DONT	254		/* you are not to use option */
#define	DO	253		/* please, you use option */
#define	WONT	252		/* I won't use option */
#define	WILL	251		/* I will use option */
#define	SB	250		/* interpret as subnegotiation */
#define	SE	240		/* end sub negotiation */

#define	AYT	246		/* are you there */
#define	AO	245		/* abort output--but let prog finish */
#define	IP	244		/* interrupt process--permanently */
#define	BREAK	243		/* break */

#define TELOPT_BINARY	0	/* 8-bit data path */
#define TELOPT_ECHO	1	/* echo */
#define	TELOPT_RCP	2	/* prepare to reconnect */
#define	TELOPT_SGA	3	/* suppress go ahead */
#define	TELOPT_NAMS	4	/* approximate message size */
#define	TELOPT_STATUS	5	/* give status */
#define	TELOPT_TM	6	/* timing mark */
#define	TELOPT_RCTE	7	/* remote controlled transmission and echo */
#define TELOPT_NAOL 	8	/* negotiate about output line width */
#define TELOPT_NAOP 	9	/* negotiate about output page size */
#define TELOPT_NAOCRD	10	/* negotiate about CR disposition */
#define TELOPT_NAOHTS	11	/* negotiate about horizontal tabstops */
#define TELOPT_NAOHTD	12	/* negotiate about horizontal tab disposition */
#define TELOPT_NAOFFD	13	/* negotiate about formfeed disposition */
#define TELOPT_NAOVTS	14	/* negotiate about vertical tab stops */
#define TELOPT_NAOVTD	15	/* negotiate about vertical tab disposition */
#define TELOPT_NAOLFD	16	/* negotiate about output LF disposition */
#define TELOPT_XASCII	17	/* extended ascic character set */
#define	TELOPT_LOGOUT	18	/* force logout */
#define	TELOPT_BM	19	/* byte macro */
#define	TELOPT_DET	20	/* data entry terminal */
#define	TELOPT_SUPDUP	21	/* supdup protocol */
#define	TELOPT_SUPDUPOUTPUT 22	/* supdup output */
#define	TELOPT_SNDLOC	23	/* send location */
#define	TELOPT_TTYPE	24	/* terminal type */
#define	TELOPT_EOR	25	/* end or record */
#define	TELOPT_TUID	26	/* TACACS user identification */
#define	TELOPT_OUTMRK	27	/* output marking */
#define	TELOPT_TTYLOC	28	/* terminal location number */
#define	TELOPT_3270REGIME 29	/* 3270 regime */
#define	TELOPT_X3PAD	30	/* X.3 PAD */
#define	TELOPT_NAWS	31	/* window size */
#define	TELOPT_TSPEED	32	/* terminal speed */
#define	TELOPT_LFLOW	33	/* remote flow control */
#define TELOPT_LINEMODE	34	/* Linemode option */
#define TELOPT_XDISPLOC	35	/* X Display Location */
#define TELOPT_OLD_ENVIRON 36	/* Old - Environment variables */
#define	TELOPT_AUTHENTICATION 37/* Authenticate */
#define	TELOPT_ENCRYPT	38	/* Encryption option */
#define TELOPT_NEW_ENVIRON 39	/* New - Environment variables */
#define	TELOPT_EXOPL	255	/* extended-options-list */

#define	TELQUAL_IS	0	/* option is... */
#define	TELQUAL_SEND	1	/* send option */
#define	TELQUAL_INFO	2	/* ENVIRON: informational version of IS */
#define BSD_VAR 1
#define BSD_VALUE 0
#define RFC_VAR 0
#define RFC_VALUE 1

static char buffer[2048];
static int buflen;

struct Opt {
    int send;			       /* what we initially send */
    int nsend;			       /* -ve send if requested to stop it */
    int ack, nak;		       /* +ve and -ve acknowledgements */
    int option;			       /* the option code */
    enum {
	REQUESTED, ACTIVE, INACTIVE
    } state;
};

static struct Opt o_ttype = {WILL, WONT, DO, DONT, TELOPT_TTYPE, REQUESTED};
static struct Opt o_echo = {DO, DONT, WILL, WONT, TELOPT_ECHO, REQUESTED};
static struct Opt o_we_sga = {WILL, WONT, DO, DONT, TELOPT_SGA, REQUESTED};
static struct Opt o_they_sga = {DO, DONT, WILL, WONT, TELOPT_SGA, REQUESTED};

static struct Opt *opts[] = {
    &o_ttype, &o_echo, &o_we_sga, &o_they_sga, NULL
};

static int in_synch;

static int debugfd = -1;

static int sb_opt, sb_len;
static char *sb_buf = NULL;
static int sb_size = 0;
#define SB_DELTA 1024

static void s_write (char *buf, int len) {
    write (s, buf, len);
}

static void c_write (char *buf, int len) {
    if (buflen >= 0) {
	int rlen = sizeof(buffer)-buflen;
	if (len < rlen)
	    rlen = len;
	memcpy (buffer+buflen, buf, rlen);
	buflen += rlen;
    } else
	write (1, buf, len);
    if (debugfd != -1)
	write (debugfd, buf, len);
}

static void send_opt (int cmd, int option) {
    char b[3];

    b[0] = IAC; b[1] = cmd; b[2] = option;
    s_write (b, 3);
}

static void proc_rec_opt (int cmd, int option) {
    struct Opt **o;

    for (o = opts; *o; o++) {
	if ((*o)->option == option && (*o)->ack == cmd) {
	    switch ((*o)->state) {
	      case REQUESTED:
		(*o)->state = ACTIVE;
		break;
	      case ACTIVE:
		break;
	      case INACTIVE:
		(*o)->state = ACTIVE;
		send_opt ((*o)->send, option);
		break;
	    }
	    return;
	} else if ((*o)->option == option && (*o)->nak == cmd) {
	    switch ((*o)->state) {
	      case REQUESTED:
		(*o)->state = INACTIVE;
		break;
	      case ACTIVE:
		(*o)->state = INACTIVE;
		send_opt ((*o)->nsend, option);
		break;
	      case INACTIVE:
		break;
	    }
	    return;
	}
    }
    /*
     * If we reach here, the option was one we weren't prepared to
     * cope with. So send a negative ack.
     */
    send_opt ((cmd == WILL ? DONT : WONT), option);
}

static void process_subneg (void) {
    char b[16], *p, *q;
    int var, value;

    switch (sb_opt) {
      case TELOPT_TTYPE:
	if (sb_len == 1 && sb_buf[0] == TELQUAL_SEND) {
	    b[0] = IAC; b[1] = SB; b[2] = TELOPT_TTYPE;
	    b[3] = TELQUAL_IS;
	    strcpy(b+4, "VT220");
	    b[9] = IAC; b[10] = SE;
	    s_write (b, 11);
	}
	break;
    }
}

static void do_telnet_init (void) {
    struct Opt **o;

    /*
     * Initialise option states.
     */
    for (o = opts; *o; o++)
	if ((*o)->state == REQUESTED)
	    send_opt ((*o)->send, (*o)->option);

    /*
     * Set up SYNCH state.
     */
    in_synch = FALSE;
}

static void do_telnet_read (char *buf, int len) {
    static enum {
	TOPLEVEL, SEENIAC, SEENWILL, SEENWONT, SEENDO, SEENDONT,
	SEENSB, SUBNEGOT, SUBNEG_IAC, SEENCR
    } state = TOPLEVEL;
    char b[10];

    while (len--) {
	int c = (unsigned char) *buf++;
	switch (state) {
          case TOPLEVEL:
          case SEENCR:
            if (c == 0 && state == SEENCR)
                state = TOPLEVEL;
            else if (c == IAC)
                state = SEENIAC;
            else {
                b[0] = c;
                if (!in_synch)
                    c_write (b, 1);
                if (c == 13)
                    state = SEENCR;
                else
                    state = TOPLEVEL;
            }
            break;
	  case SEENIAC:
	    if (c == DO) state = SEENDO;
	    else if (c == DONT) state = SEENDONT;
	    else if (c == WILL) state = SEENWILL;
	    else if (c == WONT) state = SEENWONT;
	    else if (c == SB) state = SEENSB;
	    else state = TOPLEVEL;   /* we really ignore _everything_ else! */
	    break;
	  case SEENWILL:
	    proc_rec_opt (WILL, c);
	    state = TOPLEVEL;
	    break;
	  case SEENWONT:
	    proc_rec_opt (WONT, c);
	    state = TOPLEVEL;
	    break;
	  case SEENDO:
	    proc_rec_opt (DO, c);
	    state = TOPLEVEL;
	    break;
	  case SEENDONT:
	    proc_rec_opt (DONT, c);
	    state = TOPLEVEL;
	    break;
	  case SEENSB:
	    sb_opt = c;
	    sb_len = 0;
	    state = SUBNEGOT;
	    break;
	  case SUBNEGOT:
	    if (c == IAC)
		state = SUBNEG_IAC;
	    else {
		subneg_addchar:
		if (sb_len >= sb_size) {
		    char *newbuf;
		    sb_size += SB_DELTA;
		    newbuf = (sb_buf ?
			      realloc(sb_buf, sb_size) :
			      malloc(sb_size));
		    if (newbuf)
			sb_buf = newbuf;
		    else
			sb_size -= SB_DELTA;
		}
		if (sb_len < sb_size)
		    sb_buf[sb_len++] = c;
		state = SUBNEGOT;      /* in case we came here by goto */
	    }
	    break;
	  case SUBNEG_IAC:
	    if (c != SE)
		goto subneg_addchar;   /* yes, it's a hack, I know, but... */
	    else {
		process_subneg();
		state = TOPLEVEL;
	    }
	    break;
	}
    }
}

static void do_telnet_write (char *buf, int len) {
    char *p;
    static char iac[2] = { IAC, IAC };

    p = buf;
    while (p < buf+len) {
	char *q = p;

	while ((unsigned char)*p != IAC && p < buf+len) p++;
	s_write (q, p-q);
	
	while (p < buf+len && (unsigned char)*p == IAC) {
	    p++;
	    s_write (iac, 2);
	}
    }
}

static void do_telnet_except(void) {
    in_synch = TRUE;
}

int do_connection (char *host, int port) {
    struct sockaddr_in addr;
    struct termios oldterm, newterm;
    int i;
    unsigned long a;
    struct hostent *h;

    if ( (a = inet_addr(host)) == (unsigned long) INADDR_NONE) {
	if ( (h = gethostbyname(host)) == NULL) {
	    perror("gethostbyname");
	    return 1;
	}
	memcpy (&a, h->h_addr, sizeof(a));
    }
    a = ntohl(a);

    if (port == -1)
	port = 23;

    s = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    bind (s, (struct sockaddr *)&addr, sizeof(addr));

    addr.sin_addr.s_addr = htonl(a);
    addr.sin_port = htons(port);
    connect (s, (struct sockaddr *)&addr, sizeof(addr));

    i = 1;
    ioctl (s, FIONBIO, &i);

    if (buflen < 0) {
	tcgetattr (0, &oldterm);
	newterm = oldterm;
	newterm.c_lflag &= ~(ISIG | ICANON | ECHO) ;
	newterm.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR) ;
	newterm.c_oflag &= ~(OPOST | ONLCR | OCRNL);
	newterm.c_cc[VMIN] = 1 ;
	newterm.c_cc[VTIME] = 0 ;
	tcsetattr (0, TCSANOW, &newterm);
    }

    do_telnet_init ();

    while (1) {
	fd_set rd, ex;
	int ret;
	char buf[256];

	FD_ZERO (&rd); FD_ZERO (&ex);
	FD_SET (s, &rd); FD_SET (s, &ex);
	FD_SET (0, &rd);
	ret = select (s+1, &rd, NULL, &ex, NULL);
	if (ret < 0) {
	    perror("select");
	    return 1;
	}

	if (FD_ISSET (0, &ex)) {
	    do_telnet_except();
	}

	if (FD_ISSET (0, &rd)) {
	    ret = read (0, buf, sizeof(buf));
	    if (ret < 0) {
		perror("read");
		if (buflen < 0)
		    tcsetattr (0, TCSANOW, &oldterm);
		return 1;
	    }
	    if (ret == 0) {
		fprintf(stderr, "zero read on stdin?\r\n");
		if (buflen < 0)
		    tcsetattr (0, TCSANOW, &oldterm);
		return 1;
	    }
	    do_telnet_write (buf, ret);
	}

	if (FD_ISSET (s, &rd)) {
	    ret = recv(s, buf, sizeof(buf), 0);
	    if (ret < 0) {
		perror("recv");
		if (buflen < 0)
		    tcsetattr (0, TCSANOW, &oldterm);
		return 1;
	    }
	    if (ret == 0)
		break;		       /* connection has closed */
	    if (in_synch) {
		int i;
		if (ioctl (s, SIOCATMARK, &i) < 0) {
		    perror("ioctl(SIOCATMARK)");
		    if (buflen < 0)
			tcsetattr (0, TCSANOW, &oldterm);
		    return 1;
		}
		if (i) {
		    ret = recv(s, buf, sizeof(buf), MSG_OOB);
		    if (ret < 0) {
			perror("recv(MSG_OOB)");
			if (buflen < 0)
			    tcsetattr (0, TCSANOW, &oldterm);
			return 1;
		    }
		    in_synch = FALSE;
		}
	    }
	    do_telnet_read (buf, ret);
	}
    }
    if (buflen < 0)
	tcsetattr (0, TCSANOW, &oldterm);
    close (s);
    return 0;
}

int main(void) {
    char host[256];

    /*
     * Bouncer: get hold of a machine.
     */
    buflen = 0;
    if (do_connection ("mono.org", 99))
	strcpy (host, "electron.mono.org");
    else {
	char *p = buffer, *q = buffer;
	buffer[buflen] = '\0';
	while (*p && *p != ' ' && *p != '\n' && *p != '\r')
	    p++;
	if (*p == ' ') {
	    q = ++p;
	    while (*p && *p != '\n' && *p != '\r')
		p++;
	}
	*p = '\0';
	strcpy (host, q);
    }

    buflen = -1;
    debugfd = open("logfile", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    return do_connection(host, -1);
}
