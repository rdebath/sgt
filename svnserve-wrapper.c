/*
 * svnserve-wrapper: a wrapper on `svnserve -t' which implements
 * fine-grained commit control.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define setfd(set, max, fd) do { \
    FD_SET(fd, &set); \
    if (fd >= max) max = fd+1; \
} while (0)

typedef enum {
    SVT_OPEN, SVT_CLOSE, SVT_WORD, SVT_NUMBER, SVT_STRING
} svntype;

typedef enum {
    SVS_START, SVS_WANTSPACE,
    SVS_WORD,
    SVS_NUMBER,			       /* can also be prefix to STRING */
    SVS_STRING
} svnstate;

union svnprotocol_item {
    struct {
	svntype type;
    } generic;			       /* covers OPEN and CLOSE */
    struct {
	svntype type;
	int datapos;
	int len;
    } text;			       /* covers both WORD and STRING */
    struct {
	svntype type;
	int value;
    } num;			       /* covers NUMBER */
};

struct svnprotocol_state {
    /*
     * We don't actually need to determine the full parse tree for
     * each SVN request and response. The only tests we perform are
     * limited to the first few elements of the message, so we need
     * only store a fixed number of the initial elements. Of
     * course, we still have to _do_ the parsing of the rest, so as
     * to know when we reach the end of a request.
     */

    union svnprotocol_item items[32];
    int nitems;

    /*
     * Parse state.
     */
    svnstate state;
    int datapos;		       /* for strings */
    int stringlen, stringleft;
    int numval;
    union svnprotocol_item item;

    int parendepth;

    /*
     * Buffer into which to put vetted messages.
     */
    struct buffer *outbuf;

    /*
     * Whether we're the outgoing (client->server commands) data
     * flow, or the incoming (server->client responses) one.
     */
    int outgoing;
};

struct buffer {
    char *data;
    int len, size;
};

void buffer_add(struct buffer *buf, char *data, int len);

void svnprotocol_init(struct svnprotocol_state *state)
{
    memset(state, 0, sizeof(*state));
    state->state = SVS_START;
    state->nitems = 0;
    state->parendepth = 0;
}

int svnprotocol_parse(struct svnprotocol_state *state, char *data,
		      int pos, int len)
{
    union svnprotocol_item item;
    int gotone, error;
    int used = 0;

    while (len--) {
	char c = data[pos];

	gotone = error = FALSE;

#ifdef DEBUG_CHARS
	fprintf(stderr, "seen <%c> %d\n", c, state->state);
#endif

	switch (state->state) {
	  case SVS_START:
	    if (c == ' ' || c == '\n') {
		/* ignore extra whitespace here */
	    } else if (c >= '0' && c <= '9') {
		state->state = SVS_NUMBER;
		state->numval = c - '0';
	    } else if ((c >= 'a' && c <= 'z') ||
		       (c >= 'A' && c <= 'Z')) {
		state->state = SVS_WORD;
		state->datapos = pos;
		state->stringlen = 1;
	    } else if (c == '(') {
		state->item.generic.type = SVT_OPEN;
		state->state = SVS_WANTSPACE;
	    } else if (c == ')') {
		state->item.generic.type = SVT_CLOSE;
		state->state = SVS_WANTSPACE;
	    } else {
		error = TRUE;
	    }
	    break;

	  case SVS_WORD:
	    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '-') {
		state->stringlen++;
	    } else if (c == ' ' || c == '\n') {
		item.text.type = SVT_WORD;
		item.text.datapos = state->datapos;
		item.text.len = state->stringlen;
		gotone = TRUE;
		state->state = SVS_START;
	    } else {
		error = TRUE;
	    }
	    break;

	  case SVS_NUMBER:
	    if (c >= '0' && c <= '9') {
		state->numval *= 10;
		state->numval += c - '0';
	    } else if (c == ':') {
		state->datapos = pos + 1;
		state->stringlen = state->stringleft = state->numval;
		if (state->stringleft)
		    state->state = SVS_STRING;
		else {
		    state->item.text.type = SVT_STRING;
		    state->item.text.datapos = state->datapos;
		    state->item.text.len = state->stringlen;
		    state->state = SVS_WANTSPACE;
		}
	    } else if (c == ' ' || c == '\n') {
		item.num.type = SVT_NUMBER;
		item.num.value = state->numval;
		gotone = TRUE;
		state->state = SVS_START;
	    } else {
		error = TRUE;
	    }
	    break;

	  case SVS_STRING:
	    assert(state->stringleft > 0);
	    state->stringleft--;
	    if (state->stringleft == 0) {
		state->item.text.type = SVT_STRING;
		state->item.text.datapos = state->datapos;
		state->item.text.len = state->stringlen;
		state->state = SVS_WANTSPACE;
	    }
	    break;

	  case SVS_WANTSPACE:
	    if (c == ' ' || c == '\n') {
		item = state->item;    /* structure copy */
		gotone = TRUE;
		state->state = SVS_START;
	    } else {
		error = TRUE;
	    }
	    break;
	}

	pos++;

	if (gotone) {
	    if (state->nitems < lenof(state->items))
		state->items[state->nitems++] = item;   /* structure copy */
	    if (item.generic.type == SVT_OPEN)
		state->parendepth++;
	    else if (item.generic.type == SVT_CLOSE)
		state->parendepth--;

	    if (state->parendepth < 0) {
		error = TRUE;
	    } else if (state->parendepth == 0) {
		int i;
		fprintf(stderr, "--- start %d\n", state->outgoing);

		for (i = 0; i < state->nitems; i++) {
		    switch (state->items[i].generic.type) {
		      case SVT_NUMBER:
			fprintf(stderr, "number %d\n", state->items[i].num.value);
			break;
		      case SVT_STRING:
			fprintf(stderr, "string <%.*s>\n", state->items[i].text.len,
				data + state->items[i].text.datapos);
			break;
		      case SVT_WORD:
			fprintf(stderr, "word <%.*s>\n", state->items[i].text.len,
				data + state->items[i].text.datapos);
			break;
		      case SVT_OPEN:
			fprintf(stderr, "open\n");
			break;
		      case SVT_CLOSE:
			fprintf(stderr, "close\n");
			break;
		    }
		}

		fprintf(stderr, "--- end\n");

		used = pos;

		state->nitems = 0;
		state->parendepth = 0;
	    }
	}

	if (error) {
	    fprintf(stderr,
		    "svnserve-wrapper: failed to parse svn protocol\n");
	    exit(1);
	}
    }

    if (used)
	buffer_add(state->outbuf, data, used);
    return used;
}

void buffer_add(struct buffer *buf, char *data, int len)
{
    if (buf->len + len > buf->size) {
	buf->size = (buf->len + len) * 3 / 2 + 512;
	buf->data = realloc(buf->data, buf->size);
	if (!buf->data) {
	    fprintf(stderr, "svnserve-wrapper: out of memory\n");
	    exit(1);
	}
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    assert(buf->len <= buf->size);
}

void buffer_remove(struct buffer *buf, int n)
{
    assert(buf->len >= n);
    memmove(buf->data, buf->data + n, buf->len - n);
    buf->len -= n;
}

void buffer_write(struct buffer *buf, int fd, int close_afterwards)
{
    int ret;
    assert(buf->len > 0);
    ret = write(fd, buf->data, buf->len);
    if (ret < 0) {
	perror("svnserve-wrapper: write");
	exit(1);
    }

    /*
     * This buffer_remove is potentially inefficient. I may have to
     * think up a better way.
     */
    buffer_remove(buf, ret);
    if (buf->len = 0 && close_afterwards)
	close(fd);
}

void buffer_read(struct buffer *buf, int fd, int *closed,
		 struct svnprotocol_state *state)
{
    int ret, oldlen, used;
    char data[16384];

    ret = read(fd, data, sizeof(data));
    if (ret < 0) {
	perror("svnserve-wrapper: read");
	exit(1);
    }

    if (ret == 0) {
	*closed = TRUE;
	return;
    }

    oldlen = buf->len;
    buffer_add(buf, data, ret);
    used = svnprotocol_parse(state, buf->data, oldlen, ret);
    /*
     * This buffer_remove cannot leave more than sizeof(data) in
     * the buffer, so it doesn't have a potentially unbounded
     * memmove.
     */
    buffer_remove(buf, used);
}

int main(int argc, char **argv)
{
    /*
     * Usage: svnserve-wrapper <username> <repository>
     */
    char *username, *repository;
    int fromchild[2], tochild[2];
    int fromchild_closed, stdin_closed;
    int pid;
    struct buffer buf_from_child = { NULL,0,0 }, buf_from_stdin = { NULL,0,0 };
    struct buffer buf_to_child = { NULL,0,0 }, buf_to_stdout = { NULL,0,0 };
    struct svnprotocol_state command_state, response_state;

    if (argc != 3) {
	fprintf(stderr, "usage: svnserve-wrapper <username> <repository>\n");
	return 1;
    }

    username = argv[1];
    repository = argv[2];

    if (pipe(fromchild) < 0 || pipe(tochild) < 0) {
	perror("svnserve-wrapper: pipe");
	return 1;
    }

    pid = fork();

    if (pid < 0) {
	perror("svnserve-wrapper: fork");
	return 1;
    }

    if (pid == 0) {
	/*
	 * We are the child process.
	 */
	close(fromchild[0]);
	close(tochild[1]);
	dup2(fromchild[1], 1);
	dup2(tochild[0], 0);
	close(fromchild[1]);
	close(tochild[0]);
	/* FIXME: Silly pathname. */
	execlp("/home/simon/src/svn-research/svn/subversion/svnserve/svnserve",
	       "svnserve", "-t", "--tunnel-user", username, NULL);
	perror("svnserve-wrapper: exec");
	return 127;
    }

    /*
     * Now we know we're the parent process. Sit here, accumulate
     * data from both ends of the connection, and send it on once
     * we've checked it.
     */
    close(fromchild[1]);
    close(tochild[0]);
    fromchild_closed = stdin_closed = FALSE;
    svnprotocol_init(&command_state);
    svnprotocol_init(&response_state);
    command_state.outbuf = &buf_to_child;
    command_state.outgoing = TRUE;
    response_state.outbuf = &buf_to_stdout;
    response_state.outgoing = FALSE;

    while (1) {
	fd_set reads, writes;
	int maxfd, ret;

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	maxfd = 0;

	/*
	 * Always read from both child and stdin unless we've seen
	 * EOF on either.
	 */
	if (!fromchild_closed)
	    setfd(reads, maxfd, fromchild[0]);
	if (!stdin_closed)
	    setfd(reads, maxfd, 0);

	/*
	 * Write to child or stdout iff we have data available to
	 * write to each.
	 */
	if (buf_to_child.len > 0)
	    setfd(writes, maxfd, tochild[1]);
	if (buf_to_stdout.len > 0)
	    setfd(writes, maxfd, 1);

	/*
	 * Do the select.
	 */
	ret = select(maxfd, &reads, &writes, NULL, NULL);

	if (ret < 0) {
	    perror("svnserve-wrapper: select");
	    return 1;
	}

	assert(ret > 0);	       /* can't time out: we passed NULL */

	/*
	 * Now process each possibility.
	 */
	if (FD_ISSET(tochild[1], &writes))
	    buffer_write(&buf_to_child, tochild[1], stdin_closed);
	if (FD_ISSET(1, &writes))
	    buffer_write(&buf_to_stdout, 1, fromchild_closed);
	if (FD_ISSET(fromchild[0], &reads))
	    buffer_read(&buf_from_child, fromchild[0], &fromchild_closed,
			&response_state);
	if (FD_ISSET(0, &reads))
	    buffer_read(&buf_from_stdin, 0, &stdin_closed, &command_state);
    }
}
