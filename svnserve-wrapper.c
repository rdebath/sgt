/*
 * svnserve-wrapper: a wrapper on `svnserve -t' which implements
 * fine-grained commit control.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>

#include "tree234.h"

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

struct access {
    char *username;
    tree234 *paths;
};

struct access_path {
    char *path;
    int allow;
};

int accesscmp(void *av, void *bv)
{
    struct access_path *a = (struct access_path *)av;
    struct access_path *b = (struct access_path *)bv;

    return strcmp(a->path, b->path);
}

struct access *new_access_db(char *username)
{
    struct access *ac;

    ac = malloc(sizeof(struct access));
    if (!ac) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    ac->paths = newtree234(accesscmp);
    ac->username = username;

    return ac;
}

void add_access_path(struct access *ac, int allow, char *pathname)
{
    struct access_path *path, *prev;

    path = malloc(sizeof(struct access_path));
    if (!path) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    path->path = malloc(1+strlen(pathname));
    if (!path->path) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    strcpy(path->path, pathname);
    path->allow = allow;

    prev = add234(ac->paths, path);

    if (prev != path) {
	/*
	 * Existing rule for this pathname is superseded.
	 */
	del234(ac->paths, prev);
	add234(ac->paths, path);
	free(prev->path);
	free(prev);
    }
}

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

/*
 * The Subversion protocol transmits two sets of data representing
 * the directory path, side by side. A typical request will be
 * something like `open-dir', which will take one argument that's
 * the pathname of the directory relative to the original base URL
 * specified at the start of the connection, and another argument
 * that's a `token' (basically a handle) representing the directory
 * one level above it.
 * 
 * Since we are a security application, we have to allow for the
 * possibility that a malicious client may attempt to confuse the
 * Subversion server by passing a token to the wrong parent
 * directory. Therefore, we must check ourselves that the tokens
 * and the pathnames match up and make sense. Thus, I store a
 * tree234 of currently active dir tokens.
 */
struct dir_token {
    char *token;
    char *path;
};

struct svn_semantics {
    char *base_path;
    char *repository;
    tree234 *tokens;
    struct access *ac;
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

    struct svn_semantics *sstate;
};

struct buffer {
    char *data;
    int len, size;
};

void buffer_add(struct buffer *buf, char *data, int len);

int tokencmp(void *av, void *bv)
{
    struct dir_token *a = (struct dir_token *)av;
    struct dir_token *b = (struct dir_token *)bv;

    return strcmp(a->token, b->token);
}

struct svn_semantics *new_semantic_state(char *repository)
{
    struct svn_semantics *state = malloc(sizeof(struct svn_semantics));
    if (!state) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    state->base_path = NULL;
    state->repository = repository;
    state->tokens = newtree234(tokencmp);

    return state;
}

#define ISTYPE(x) (len > 0 && p->generic.type == (x))
#define ISNUM(n) (ISTYPE(SVT_NUMBER) && p->num.value == (n))
#define ISWORD(s) (ISTYPE(SVT_WORD) && \
		   p->text.len == strlen((s)) && \
		   !memcmp(data + p->text.datapos, s, p->text.len))
#define ADVANCE (p++, len--)
#define EXPECT(expr) do { \
    if (!(expr)) { \
	fprintf(stderr, "error parsing svnserve protocol [%d]\n", __LINE__); \
	exit(1); \
    } \
    ADVANCE; \
} while (0)
#define ENDLIST(depth) do { \
    int d = (depth); \
    while (d > 0) { \
	if (len <= 0) { \
	    fprintf(stderr, "error parsing svnserve protocol [%d]\n", __LINE__); \
	    exit(1); \
	} \
	if (ISTYPE(SVT_OPEN)) d++; \
	else if (ISTYPE(SVT_CLOSE)) d--; \
	p++, len--; \
    } \
} while (0)

char *duptext(union svnprotocol_item *item, char *data)
{
    char *ret;
    ret = malloc(item->text.len + 1);
    if (!ret) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }
    memcpy(ret, data + item->text.datapos, item->text.len);
    ret[item->text.len] = '\0';
    return ret;
}

void check_access(struct access *ac, char *base_path, char *path)
{
    char *full_path, *p;

    full_path = malloc(10 + strlen(base_path) + strlen(path));
    if (!full_path) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    p = full_path;

    if (base_path[0] != '/') *p++ = '/';
    strcpy(p, base_path);
    p += strlen(p);
    assert(p > full_path);
    if (p[-1] != '/') *p++ = '/';
    strcpy(p, path + (path[0] == '/' ? 1 : 0));
    p += strlen(p);
    if (p[-1] == '/' && p > full_path+1) *--p = '\0';

    /*
     * Now check the access database.
     */
    while (1) {
	struct access_path testpath, *ap;

	testpath.path = full_path;

	ap = find234(ac->paths, &testpath, NULL);

	if (ap) {
	    /*
	     * Search is over. We have found our access entry.
	     */
	    if (ap->allow)
		break;		       /* fine; just free full_path and go */
	    fprintf(stderr, "svnserve-wrapper: permission for %s to commit"
		    " to %s denied\n", ac->username, full_path);
	    exit(1);
	}

	/*
	 * Now look one stage further up.
	 */
	if (full_path[0] == '/' && !full_path[1]) {
	    /*
	     * We have reached the top of the path tree and found
	     * no entry in the config file. In this situation we
	     * conservatively avoid allowing the commit, but also
	     * print an error indicating that the configuration
	     * file ought to mention it explicitly.
	     */
	    fprintf(stderr, "svnserve-wrapper: configuration file does not"
		    " state default policy for %s\n", ac->username);
	    exit(1);
	}
	p = full_path + strlen(full_path);
	while (p > full_path && p[-1] != '/') p--;
	if (p > full_path+1) p--;
	*p = '\0';
    }

    free(full_path);
}

void add_token_path(struct svn_semantics *state, char *token, char *path)
{
    struct dir_token *t;

    t = malloc(sizeof(struct dir_token));
    if (!t) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    t->token = token;
    t->path = malloc(1 + strlen(path));
    if (!t->path) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }
    strcpy(t->path, path);

    if (add234(state->tokens, t) != t) {
	fprintf(stderr, "svnserve-wrapper: attempt to reuse an existing "
		"dir-token\n");
	exit(1);
    }
}

struct dir_token *lookup_token(struct svn_semantics *state, char *token)
{
    struct dir_token t, *tt;
    t.token = token;
    t.path = NULL;

    tt = find234(state->tokens, &t, NULL);

    if (!tt) {
	fprintf(stderr, "svnserve-wrapper: attempt to reference a "
		"non-existent dir-token\n");
	exit(1);
    }

    return tt;
}

void remove_token_path(struct svn_semantics *state, char *token)
{
    struct dir_token *t = lookup_token(state, token);

    del234(state->tokens, t);

    free(t->token);
    free(t->path);
    free(t);
}

void verify_token_parent_path(struct svn_semantics *state,
			      char *token, char *path)
{
    char *slash;
    struct dir_token *t;

    /*
     * Find the rightmost slash in path. If it doesn't exist,
     * pretend it was right at the start.
     */
    slash = strrchr(path, '/');
    if (!slash)
	slash = path;

    /*
     * Now check that everything before that path is precisely
     * equal to the path represented by token.
     */
    t = lookup_token(state, token);
    if (strlen(t->path) != slash-path ||
	memcmp(t->path, path, slash-path)) {
	fprintf(stderr, "svnserve-wrapper: parent token did not match "
		"directory\n");
	exit(1);
    }
}

void parse_svn_message(struct svn_semantics *state, char *data,
		       union svnprotocol_item *items, int nitems)
{
    union svnprotocol_item *p = items;
    int len = nitems;

    if (!state->base_path) {
	char *url;
	/*
	 * We expect the very first client-side message to give the
	 * path of the Subversion repository. In fact we know quite
	 * precisely what we expect the message to look like.
	 */
	EXPECT(ISTYPE(SVT_OPEN));
	EXPECT(ISTYPE(SVT_NUMBER));
	EXPECT(ISTYPE(SVT_OPEN));
	ENDLIST(1);
	EXPECT(ISTYPE(SVT_STRING));
	url = duptext(p-1, data);
	EXPECT(ISTYPE(SVT_CLOSE));

	/*
	 * Now parse the URL. We discard the protocol and host
	 * parts, and head straight for the pathname. We then bomb
	 * out if that pathname doesn't _begin_ with the prefix
	 * given to us in argv[2], and we save everything after
	 * that so that we know the absolute within-repository
	 * paths to inform our access control decisions.
	 */
	{
	    char *q;

	    q = url;
	    while (*q && *q != ':') q++;
	    if (q[0] == ':' && q[1] == '/' && q[2] == '/') {
		q += 3;
		while (*q && *q != '/') q++;
		if (q[0] == '/') {
		    /*
		     * Now we've found the pathname.
		     */
		    if (strncmp(q, state->repository,
				strlen(state->repository)) ||
			(q[strlen(state->repository)] != '/' &&
			 q[strlen(state->repository)] != '\0')) {
			fprintf(stderr, "svnserve-wrapper: attempt to "
				"access wrong repository <%s>\n", url);
			exit(1);
		    }
		    q += strlen(state->repository);
		    state->base_path = q;
		}
	    }
	}

	if (!state->base_path) {
	    fprintf(stderr,
		    "svnserve-wrapper: unable to parse repository URL\n");
	    exit(1);
	}
    } else {

	/*
	 * Otherwise, we're watching editor commands and looking
	 * out for attempts to modify parts of the repository.
	 */

	EXPECT(ISTYPE(SVT_OPEN));

	if (ISWORD("open-root")) {
	    char *token;

	    /*
	     * This opens base_path itself. Expect a revision tuple
	     * which we ignore, followed by a token.
	     */
	    check_access(state->ac, state->base_path, "");

	    ADVANCE;

	    EXPECT(ISTYPE(SVT_OPEN));
	    EXPECT(ISTYPE(SVT_OPEN));
	    ENDLIST(1);
	    EXPECT(ISTYPE(SVT_STRING));
	    token = duptext(p-1, data);
	    add_token_path(state, token, "");
	} else if (ISWORD("open-dir") ||
		   ISWORD("open-file") ||
		   ISWORD("add-dir") ||
		   ISWORD("add-file") ||
		   ISWORD("delete-entry")) {
	    char *path, *cmd, *ptoken, *ttoken;

	    cmd = duptext(p, data);
	    ADVANCE;

	    /*
	     * All of these commands start off with a pathname,
	     * which we concatenate to the end of base_path and
	     * check.
	     */
	    EXPECT(ISTYPE(SVT_OPEN));
	    EXPECT(ISTYPE(SVT_STRING));

	    path = duptext(p-1, data);
	    check_access(state->ac, state->base_path, path);

	    /*
	     * delete-entry now provides a revision tuple, which we
	     * ignore.
	     */
	    if (!strcmp(cmd, "delete-entry")) {
		EXPECT(ISTYPE(SVT_OPEN));
		ENDLIST(1);		
	    }

	    /*
	     * All five of these commands now provide a parent-dir
	     * token, which we grab and verify against the path.
	     */
	    EXPECT(ISTYPE(SVT_STRING));
	    ptoken = duptext(p-1, data);
	    verify_token_parent_path(state, ptoken, path);
	    free(ptoken);

	    /*
	     * open-dir and add-dir follow this with a child-dir
	     * token, which we grab and install in our dir token
	     * list.
	     */
	    if (!strcmp(cmd, "open-dir") || !strcmp(cmd, "add-dir")) {
		EXPECT(ISTYPE(SVT_STRING));
		ttoken = duptext(p-1, data);
		add_token_path(state, ttoken, path);
	    }

	    free(path);
	} else if (ISWORD("close-dir")) {
	    char *token;

	    ADVANCE;

	    /*
	     * Expect a dir token, which we grab and remove from
	     * our token list.
	     */
	    EXPECT(ISTYPE(SVT_OPEN));
	    EXPECT(ISTYPE(SVT_STRING));
	    token = duptext(p-1, data);
	    remove_token_path(state, token);
	    free(token);
	}
    }
}

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
#ifdef DEBUG_MESSAGES
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
#endif

		if (state->outgoing)
		    parse_svn_message(state->sstate, data,
				      state->items, state->nitems);

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
    if (buf->len == 0 && close_afterwards)
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

/*
 * Read an entire line of text from a file. Return a buffer
 * malloced to be as big as necessary (caller must free).
 */
static char *fgetline(FILE *fp)
{
    char *ret = malloc(512);
    int size = 512, len = 0;

    if (!ret) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }

    while (fgets(ret + len, size - len, fp)) {
	len += strlen(ret + len);
	if (ret[len-1] == '\n')
	    break;		       /* got a newline, we're done */
	size = len + 512;
	ret = realloc(ret, size);
	if (!ret) {
	    fprintf(stderr, "svnserve-wrapper: out of memory\n");
	    exit(1);
	}
    }
    if (len == 0) {		       /* first fgets returned NULL */
	free(ret);
	return NULL;
    }
    ret[len] = '\0';
    return ret;
}

static struct access *read_access_file(char *username, char *repository)
{

#define CFG_ERR do { \
    fprintf(stderr, "svnserve-wrapper: error in config file [%d] at" \
		" line %d\n", __LINE__, lineno); \
    exit(1); \
} while (0)

    char *fname, *line;
    struct access *ac;
    int lineno = 0;
    FILE *fp;

    fname = malloc(strlen(repository) + 256);
    if (!fname) {
	fprintf(stderr, "svnserve-wrapper: out of memory\n");
	exit(1);
    }
    sprintf(fname, "%s/conf/svnserve-wrapper.conf", repository);

    fp = fopen(fname, "r");
    if (!fp) {
	fprintf(stderr, "svnserve-wrapper: unable to open config file\n");
	exit(1);
    }
    free(fname);

    ac = new_access_db(username);

    while ( (line = fgetline(fp)) != NULL ) {
	int allow;
	char *path;
	char *p = line;
	lineno++;

	while (*p && isspace((unsigned char)*p)) p++;

	if (*p != '#') {
	    /*
	     * A valid config file line contains some
	     * space-separated fields:
	     * 
	     *  - `A' or `D', for `allow' or `deny'.
	     * 
	     *  - A path name.
	     * 
	     * 	- Zero or more user names (none means this rule
	     * 	  applies to anybody at all).
	     * 
	     * Later rules supersede earlier ones when they apply
	     * to precisely the same path. However, rules applying
	     * to more specific paths supersede rules applying to
	     * their parent (more general) paths. So it would be
	     * valid, for example, to do something like this:
	     * 
	     *   A /putty simon ben owen jacob
	     *   D /
	     *   A / simon
	     * 
	     * to construct a repository in which `simon' may
	     * commit to absolutely anything, `ben', `owen' and
	     * `jacob' may commit only to paths below /putty, and
	     * nobody else may commit to anything at all.
	     */
	    if (*p != 'A' && *p != 'D') CFG_ERR;
	    allow = (*p == 'A');

	    p++;
	    while (*p && isspace((unsigned char)*p)) p++;

	    path = p;
	    while (*p && !isspace((unsigned char)*p)) p++;
	    if (*p)
		*p++ = '\0';
	    while (*p && isspace((unsigned char)*p)) p++;

	    if (*p) {
		int ok = FALSE;
		char *user;
		/*
		 * If there's at least one user name mentioned, we
		 * must see if it mentions _us_.
		 */
		while (*p) {
		    user = p;
		    while (*p && !isspace((unsigned char)*p)) p++;
		    if (*p)
			*p++ = '\0';
		    while (*p && isspace((unsigned char)*p)) p++;

		    if (!strcmp(user, username))
			ok = TRUE;
		}

		if (!ok)
		    goto done;
	    }

	    /*
	     * Now add this path to our access database.
	     */
	    add_access_path(ac, allow, path);
	}

	done:
	free(line);
    }

    return ac;

#undef CFG_ERR

}

int main(int argc, char **argv)
{
    /*
     * Usage: svnserve-wrapper <username> <repository>
     */
    char *username, *repository, *svnserve;
    int fromchild[2], tochild[2];
    int fromchild_closed, stdin_closed;
    int pid;
    struct buffer buf_from_child = { NULL,0,0 }, buf_from_stdin = { NULL,0,0 };
    struct buffer buf_to_child = { NULL,0,0 }, buf_to_stdout = { NULL,0,0 };
    struct svnprotocol_state command_state, response_state;

    if (argc != 3 && argc != 4) {
	fprintf(stderr, "usage: svnserve-wrapper <username> <repository>"
		" [<svnserve-path>]\n");
	return 1;
    }

    username = argv[1];
    repository = argv[2];
    if (argc == 4)
	svnserve = argv[3];
    else
	svnserve = NULL;

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
	(svnserve ? execl : execlp)
	    (svnserve ? svnserve : "svnserve",
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
    command_state.sstate = new_semantic_state(repository);
    command_state.sstate->ac = read_access_file(username, repository);

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
