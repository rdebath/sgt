/*
 * Unix-socket equivalent of netcat. Just connects to a specified
 * Unix socket path and transfers data back and forth until the
 * connection is closed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define min(x,y) ( (x)<(y)?(x):(y) )
#define max(x,y) ( (x)>(y)?(x):(y) )

/* ----------------------------------------------------------------------
 * Chunk of code lifted from PuTTY's misc.c to manage buffers of
 * data to be written to an fd.
 */

#define BUFFER_GRANULE  512

typedef struct bufchain_tag {
    struct bufchain_granule *head, *tail;
    int buffersize;		       /* current amount of buffered data */
} bufchain;
struct bufchain_granule {
    struct bufchain_granule *next;
    int buflen, bufpos;
    char buf[BUFFER_GRANULE];
};

static void bufchain_init(bufchain *ch)
{
    ch->head = ch->tail = NULL;
    ch->buffersize = 0;
}

static int bufchain_size(bufchain *ch)
{
    return ch->buffersize;
}

static void bufchain_add(bufchain *ch, const void *data, int len)
{
    const char *buf = (const char *)data;

    if (len == 0) return;

    ch->buffersize += len;

    if (ch->tail && ch->tail->buflen < BUFFER_GRANULE) {
	int copylen = min(len, BUFFER_GRANULE - ch->tail->buflen);
	memcpy(ch->tail->buf + ch->tail->buflen, buf, copylen);
	buf += copylen;
	len -= copylen;
	ch->tail->buflen += copylen;
    }
    while (len > 0) {
	int grainlen = min(len, BUFFER_GRANULE);
	struct bufchain_granule *newbuf;
	newbuf = (struct bufchain_granule *)
	    malloc(sizeof(struct bufchain_granule));
	if (!newbuf) {
	    fprintf(stderr, "filter: out of memory\n");
	    exit(1);
	}
	newbuf->bufpos = 0;
	newbuf->buflen = grainlen;
	memcpy(newbuf->buf, buf, grainlen);
	buf += grainlen;
	len -= grainlen;
	if (ch->tail)
	    ch->tail->next = newbuf;
	else
	    ch->head = ch->tail = newbuf;
	newbuf->next = NULL;
	ch->tail = newbuf;
    }
}

static void bufchain_consume(bufchain *ch, int len)
{
    struct bufchain_granule *tmp;

    assert(ch->buffersize >= len);
    while (len > 0) {
	int remlen = len;
	assert(ch->head != NULL);
	if (remlen >= ch->head->buflen - ch->head->bufpos) {
	    remlen = ch->head->buflen - ch->head->bufpos;
	    tmp = ch->head;
	    ch->head = tmp->next;
	    free(tmp);
	    if (!ch->head)
		ch->tail = NULL;
	} else
	    ch->head->bufpos += remlen;
	ch->buffersize -= remlen;
	len -= remlen;
    }
}

static void bufchain_prefix(bufchain *ch, void **data, int *len)
{
    *len = ch->head->buflen - ch->head->bufpos;
    *data = ch->head->buf + ch->head->bufpos;
}

/* ----------------------------------------------------------------------
 * End of bufchain stuff. Main socket-handling code begins.
 */

#define LOCALBUF_LIMIT 65536

void do_connect(char *sockname)
{
    int sock;
    struct sockaddr_un addr;
    int addrlen;
    bufchain tosock, tostdout;
    int tosock_active, tostdout_active, fromstdin_active, fromsock_active;

    if (strlen(sockname) >= sizeof(addr.sun_path)) {
	fprintf(stderr, "unc: socket name '%s' is too long\n", sockname);
	exit(1);
    }

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "unc: socket(AF_UNIX): %s\n", strerror(errno));
        exit(1);
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockname);
    addrlen = sizeof(addr);
    if (connect(sock, (struct sockaddr *)&addr, addrlen) < 0) {
        fprintf(stderr, "unc: connect(\"%s\"): %s\n", sockname, strerror(errno));
        exit(1);
    }

    bufchain_init(&tosock);
    bufchain_init(&tostdout);
    tosock_active = tostdout_active = 1;
    fromsock_active = fromstdin_active = 1;

    while (1) {
	fd_set rset, wset;
	char buf[65536];
	int maxfd, ret;

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	maxfd = 0;

	if (tosock_active && bufchain_size(&tosock)) {
	    FD_SET(sock, &wset);
	    maxfd = max(sock+1, maxfd);
	}
	if (tostdout_active && bufchain_size(&tostdout)) {
	    FD_SET(1, &wset);
	    maxfd = max(1+1, maxfd);
	}
	if (fromstdin_active && bufchain_size(&tosock) < LOCALBUF_LIMIT) {
	    FD_SET(0, &rset);
	    maxfd = max(0+1, maxfd);
	}
	if (fromsock_active && bufchain_size(&tostdout) < LOCALBUF_LIMIT) {
	    FD_SET(sock, &rset);
	    maxfd = max(sock+1, maxfd);
	}

	ret = select(maxfd, &rset, &wset, NULL, NULL);

	if (ret < 0) {
	    perror("unc: select");
	    exit(1);
	}

	if (FD_ISSET(sock, &rset)) {
	    ret = read(sock, buf, sizeof(buf));
	    if (ret <= 0) {
		if (ret < 0) {
		    perror("unc: socket: read");
		}
		shutdown(sock, SHUT_RD);
		fromsock_active = 0;
		ret = 0;
	    } else
		bufchain_add(&tostdout, buf, ret);
	}
	if (FD_ISSET(0, &rset)) {
	    ret = read(0, buf, sizeof(buf));
	    if (ret <= 0) {
		if (ret < 0) {
		    perror("unc: stdin: read");
		}
		close(0);
		fromstdin_active = 0;
		ret = 0;
	    } else
		bufchain_add(&tosock, buf, ret);
	}
	if (FD_ISSET(1, &wset)) {
	    void *data;
	    int len, ret;
	    bufchain_prefix(&tostdout, &data, &len);
	    if ((ret = write(1, data, len)) < 0) {
		perror("unc: stdout: write");
		close(1);
		shutdown(sock, SHUT_RD);
		tostdout_active = fromsock_active = 0;
	    } else
		bufchain_consume(&tostdout, ret);
	}
	if (FD_ISSET(sock, &wset)) {
	    void *data;
	    int len;
	    bufchain_prefix(&tosock, &data, &len);
	    if ((ret = write(sock, data, len)) < 0) {
		perror("unc: socket: write");
		close(0);
		shutdown(sock, SHUT_WR);
		tosock_active = fromstdin_active = 0;
	    } else
		bufchain_consume(&tosock, ret);
	}

	/*
	 * If there can be no further data from a direction (the
	 * input fd has been closed and the buffered data is used
	 * up) but its output fd is still open, close it.
	 */
	if (!fromstdin_active && !bufchain_size(&tosock) && tosock_active) {
	    tosock_active = 0;
	    shutdown(sock, SHUT_WR);
	}
	if (!fromsock_active && !bufchain_size(&tostdout) && tostdout_active) {
	    tostdout_active = 0;
	    close(1);
	}

	/*
	 * Termination condition is that there's still data flowing
	 * in at least one direction.
	 * 
	 * The condition `there is data flowing in a direction'
	 * unpacks as: there is at least potentially data left to
	 * be written (i.e. EITHER the input side of that direction
	 * is still open OR we have some buffered data left from
	 * before it closed) AND we are able to write it (the
	 * output side is still active).
	 */
	if (tosock_active &&
	    (fromstdin_active || bufchain_size(&tosock)))
	    /* stdin -> sock direction is still active */;
	else if (tostdout_active &&
		 (fromsock_active || bufchain_size(&tostdout)))
	    /* sock -> stdout direction is still active */;
	else
	    break;		       /* neither is active any more */
    }
}

int main(int argc, char **argv)
{
    /* FIXME: command line */
    do_connect(argv[1]);
    return 0;
}
