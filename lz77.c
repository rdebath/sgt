/*
 * LZ77 compressor. In the probably vain hope that this will be the
 * _last time_ I have to write one.
 */

#include <stdlib.h>
#include "lz77.h"

/*
 * Modifiable parameters.
 */
#define WINSIZE 32768		       /* window size. Must be power of 2! */
#define HASHMAX 2039		       /* one more than max hash value */
#define MAXMATCH 32		       /* how many matches we track */
#define HASHCHARS 3		       /* how many chars make a hash */

/*
 * This compressor takes a less slapdash approach than the
 * gzip/zlib one. Rather than allowing our hash chains to fall into
 * disuse near the far end, we keep them doubly linked so we can
 * _find_ the far end, and then every time we add a new byte to the
 * window (thus rolling round by one and removing the previous
 * byte), we can carefully remove the hash chain entry.
 */

#define INVALID -1		       /* invalid hash _and_ invalid offset */
struct WindowEntry {
    int next, prev;		       /* array indices within the window */
    int hashval;
};

struct HashEntry {
    int first;			       /* window index of first in chain */
};

struct Match {
    int distance, len;
};

struct LZ77InternalContext {
    struct WindowEntry win[WINSIZE];
    unsigned char data[WINSIZE];
    int winpos;
    struct HashEntry hashtab[HASHMAX];
};

static int lz77_hash(unsigned char *data) {
    return (257*data[0] + 263*data[1] + 269*data[2]) % HASHMAX;
}

int lz77_init(struct LZ77Context *ctx) {
    struct LZ77InternalContext *st;
    int i;

    st = (struct LZ77InternalContext *)malloc(sizeof(*st));
    if (!st)
	return 0;

    ctx->ictx = st;

    for (i = 0; i < WINSIZE; i++)
	st->win[i].next = st->win[i].prev = st->win[i].hashval = INVALID;
    for (i = 0; i < HASHMAX; i++)
	st->hashtab[i].first = INVALID;
    st->winpos = 0;

    return 1;
}

#define CHARAT(k) ( (k)<0 ? st->data[(st->winpos+k)&(WINSIZE-1)] : data[k] )

void lz77_compress(struct LZ77Context *ctx, unsigned char *data, int len) {
    struct LZ77InternalContext *st = ctx->ictx;
    int i, hash, distance, off, nmatch, matchlen, advance;
    struct Match defermatch, matches[MAXMATCH];
    int deferchr;

    defermatch.len = 0;
    while (len >= HASHCHARS) {
	/*
	 * Hash the next few characters.
	 */
	hash = lz77_hash(data);

	/*
	 * Look the hash up in the corresponding hash chain and see
	 * what we can find.
	 */
	nmatch = 0;
	for (off = st->hashtab[hash].first;
	     off != INVALID; off = st->win[off].next) {
	    /* distance = 1       if off == st->winpos-1 */
	    /* distance = WINSIZE if off == st->winpos   */
	    distance = WINSIZE - (off + WINSIZE - st->winpos) % WINSIZE;
	    for (i = 0; i < HASHCHARS; i++)
		if (CHARAT(i) != CHARAT(i-distance))
		    break;
	    if (i == HASHCHARS) {
		matches[nmatch].distance = distance;
		matches[nmatch].len = 3;
		if (++nmatch >= MAXMATCH)
		    break;
	    }
	}

	if (nmatch > 0) {
	    /*
	     * We've now filled up matches[] with nmatch potential
	     * matches. Follow them down to find the longest. (We
	     * assume here that it's always worth favouring a
	     * longer match over a shorter one.)
	     */
	    matchlen = HASHCHARS;
	    while (matchlen < len) {
		int j;
		for (i = j = 0; i < nmatch; i++) {
		    if (CHARAT(matchlen) ==
			CHARAT(matchlen - matches[i].distance)) {
			matches[j++] = matches[i];
		    }
		}
		if (j == 0)
		    break;
		matchlen++;
		nmatch = j;
	    }

	    /*
	     * We've now got all the longest matches. We favour the
	     * shorter distances, which means we go with matches[0].
	     * So see if we want to defer it or throw it away.
	     */
	    matches[0].len = matchlen;
	    if (defermatch.len > 0) {
		if (matches[0].len > defermatch.len + 1) {
		    /* We have a better match. Emit the deferred char,
		     * and defer this match. */
		    ctx->literal(ctx, deferchr);
		    defermatch = matches[0];
		    deferchr = data[0];
		    advance = 1;
		} else {
		    /* We don't have a better match. Do the deferred one. */
		    ctx->match(ctx, defermatch.distance, defermatch.len);
		    advance = defermatch.len - 1;
		    defermatch.len = 0;
		}
	    } else {
		/* There was no deferred match. Defer this one. */
		defermatch = matches[0];
		deferchr = data[0];
		advance = 1;
	    }	    
	} else {
	    /*
	     * We found no matches. Emit the deferred match, if
	     * any; otherwise emit a literal.
	     */
	    if (defermatch.len > 0) {
		ctx->match(ctx, defermatch.distance, defermatch.len);
		advance = defermatch.len - 1;
		defermatch.len = 0;
	    } else {
		ctx->literal(ctx, data[0]);
		advance = 1;
	    }
	}

	/*
	 * Now advance the position by `advance' characters,
	 * keeping the window and hash chains consistent.
	 */
	while (advance > 0) {
	    /*
	     * Remove the hash entry at winpos from the tail of its
	     * chain, or empty the chain if it's the only thing on
	     * the chain.
	     */
	    if (st->win[st->winpos].prev != INVALID) {
		st->win[st->win[st->winpos].prev].next = INVALID;
	    } else if (st->win[st->winpos].hashval != INVALID) {
		st->hashtab[st->win[st->winpos].hashval].first = INVALID;
	    }

	    /*
	     * Create a new entry at winpos and add it to the head
	     * of its hash chain.
	     */
	    st->win[st->winpos].hashval = hash;
	    st->win[st->winpos].prev = INVALID;
	    off = st->win[st->winpos].next = st->hashtab[hash].first;
	    st->hashtab[hash].first = st->winpos;
	    if (off != INVALID)
		st->win[off].prev = st->winpos;
	    st->data[st->winpos] = data[0];

	    /*
	     * Advance all pointers.
	     */
	    data++;
	    len--;
	    st->winpos = (st->winpos + 1) & (WINSIZE-1);
	    advance--;
	}
    }

    /*
     * We've reached the end of the line. Emit everything that's
     * left.
     */
    if (defermatch.len > 0) {
	ctx->match(ctx, defermatch.distance, defermatch.len);
	advance = defermatch.len - 1;
	data += advance; len -= advance;
    }
    while (len--)
	ctx->literal(ctx, *data++);
}

#ifdef TESTMODE

#include <stdio.h>

char *test1 = "Pease porridge hot, pease porridge cold, pease porridge"
    " in the pot, nine days old.";
char *test2 = "oojamaflip fooj x fooj foojamaflip flip flop";
char *test3 = "yip yip yip yip piggy yip spod spod chickypig";

void match(struct LZ77Context *ctx, int distance, int len) {
    printf("<%d,%d>", distance, len);
}

void literal(struct LZ77Context *ctx, unsigned char c) {
    fputc(c, stdout);
}

int main(void) {
    struct LZ77Context ctx;
    ctx.match = match;
    ctx.literal = literal;
    lz77_init(&ctx);
    lz77_compress(&ctx, test1, strlen(test1));
    printf("\n");
    lz77_init(&ctx);
    lz77_compress(&ctx, test2, strlen(test2));
    printf("\n");
    lz77_init(&ctx);
    lz77_compress(&ctx, test3, strlen(test3));
    printf("\n");
    return 0;
}

#endif
