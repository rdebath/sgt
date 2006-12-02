/*
 * Reimplementation of Deflate (RFC1951) compression. Adapted from
 * the version in PuTTY, and extended to write dynamic Huffman
 * trees and choose block boundaries usefully.
 */

/*
 * TODO:
 * 
 *  - Feature: it would probably be useful to add a third format
 *    type to read and write actual gzip files.
 * 
 *  - Feature: could do with forms of flush other than SYNC_FLUSH.
 *    I'm not sure exactly how those work when you don't know in
 *    advance that your next block will be static (as we did in
 *    PuTTY). And remember the 9-bit limitation of zlib.
 *     + also, zlib has FULL_FLUSH which clears the LZ77 state as
 * 	 well, for random access.
 *
 *  - Compression quality: introduce the option of choosing a
 *    static block instead of a dynamic one, where that's more
 *    efficient.
 *
 *  - Compression quality: chooseblock() appears to be computing
 *    wildly inaccurate block size estimates. Possible resolutions:
 *     + find and fix some trivial bug I haven't spotted yet
 *     + abandon the entropic approximation and go with trial
 * 	 Huffman runs
 *
 *  - Compression quality: see if increasing SYMLIMIT causes
 *    dynamic blocks to start being consistently smaller than it.
 *
 *  - Compression quality: we ought to be able to fall right back
 *    to actual uncompressed blocks if really necessary, though
 *    it's not clear what the criterion for doing so would be.
 *
 *  - Performance: chooseblock() is currently computing the whole
 *    entropic approximation for every possible block size. It
 *    ought to be able to update it incrementally as it goes along
 *    (assuming of course we don't jack it all in and go for a
 *    proper Huffman analysis).
 */

/*
 * This software is copyright 2000-2006 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "deflate.h"

#define snew(type) ( (type *) malloc(sizeof(type)) )
#define snewn(n, type) ( (type *) malloc((n) * sizeof(type)) )
#define sresize(x, n, type) ( (type *) realloc((x), (n) * sizeof(type)) )
#define sfree(x) ( free((x)) )

#define lenof(x) (sizeof((x)) / sizeof(*(x)))

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

/* ----------------------------------------------------------------------
 * This file can be compiled in a number of modes.
 * 
 * With -DSTANDALONE, it builds a self-contained deflate tool which
 * can compress, decompress, and also analyse a deflated file to
 * print out the sequence of literals and copy commands it
 * contains.
 * 
 * With -DTESTMODE, it builds a test application which is given a
 * file on standard input, both compresses and decompresses it, and
 * outputs the re-decompressed result so it can be conveniently
 * diffed against the original. Define -DTESTDBG as well for lots
 * of diagnostics.
 */

#if defined TESTDBG
/* gcc-specific diagnostic macro */
#define debug_int(x...) ( fprintf(stderr, x) )
#define debug(x) ( debug_int x )
#else
#define debug(x)
#endif

#ifdef STANDALONE
#define ANALYSIS
#endif

#ifdef ANALYSIS
int analyse = FALSE;
#endif

/* ----------------------------------------------------------------------
 * Basic LZ77 code. This bit is designed modularly, so it could be
 * ripped out and used in a different LZ77 compressor. Go to it,
 * and good luck :-)
 */

typedef struct LZ77 LZ77;

/*
 * Set up an LZ77 context. The user supplies two function pointer
 * parameters: `literal', called when the compressor outputs a
 * literal byte, and `match', called when the compressor outputs a
 * match. Each of these functions, in addition to parameters
 * describing the data being output, also receives a void * context
 * parameter which is passed in to lz77_new().
 */
static LZ77 *lz77_new(void (*literal)(void *ctx, unsigned char c),
		      void (*match)(void *ctx, int distance, int len),
		      void *ctx);

/*
 * Supply data to be compressed. This produces output by calling
 * the literal() and match() functions passed to lz77_new().
 * 
 * This function buffers data internally, so in any given call it
 * will not necessarily output literals and matches which cover all
 * the data passed in to it. To force a buffer flush, use
 * lz77_flush().
 */
static void lz77_compress(LZ77 *lz, const void *data, int len);

/*
 * Force the LZ77 compressor to output literals and matches which
 * cover all the data it has received as input until now.
 *
 * After calling this function, the LZ77 stream is still active:
 * future input data will still be matched against data which was
 * provided before the flush. Therefore, this function can be used
 * to produce guaranteed record boundaries in a single data stream,
 * but can _not_ be used to reuse the same LZ77 context for two
 * entirely separate data streams (otherwise the second one might
 * make backward references beyond the start of its data).
 * 
 * Calling this in the middle of a data stream can decrease
 * compression quality. If maximum compression is the only concern,
 * do not call this function until the very end of the stream.
 */
static void lz77_flush(LZ77 *lz);

/*
 * Free an LZ77 context when it's finished with. This does not
 * automatically flush buffered data; call lz77_flush explicitly to
 * do that.
 */
static void lz77_free(LZ77 *lz);

/*
 * Modifiable parameters.
 */
#define MAXMATCHDIST 32768	       /* maximum backward distance */
#define MAXMATCHLEN 258		       /* maximum length of a match */
#define HASHMAX 2039		       /* one more than max hash value */
#define HASHCHARS 3		       /* how many chars make a hash */
#define MAXLAZY 3		       /* limit of lazy matching */

/*
 * Derived parameters.
 */
#define WINSIZE (MAXMATCHDIST + HASHCHARS * 2)  /* actual window size */

struct LZ77 {
    /*
     * Administrative data passed in from lz77_new().
     */
    void *ctx;
    void (*literal)(void *ctx, unsigned char c);
    void (*match)(void *ctx, int distance, int len);

    /*
     * The actual data in the sliding window, stored as a circular
     * buffer. `winpos' marks the head of the buffer. So
     * data[winpos] is the most recent character; data[winpos+1] is
     * the most recent but one; and data[winpos-1] (all mod
     * WINSIZE, of course) is the least recent character still in
     * the buffer.
     */
    unsigned char data[WINSIZE];
    int winpos;

    /*
     * This variable indicates the number of input characters we
     * have accepted into the window which haven't been output yet.
     * It can exceed WINSIZE if we're tracking a particularly long
     * match.
     */
    int k;

    /*
     * This variable indicates the number of valid bytes in the
     * sliding window. It starts at zero at the beginning of the
     * data stream, then rapidly rises to WINSIZE; it can also
     * decrease by small amounts during the compression when we
     * need to rewind the state by a few bytes.
     */
    int nvalid;

    /*
     * This variable indicates that the compressor state has been
     * rewound by a few bytes, which causes the main loop to reuse
     * characters already in the sliding window instead of taking
     * more from the input data.
     */
    int rewound;

    /*
     * This array links the window positions into disjoint lists
     * chained from the hash table. hashnext[pos] gives the
     * position of the next most recent sequence of three
     * characters with the same hash as this one. Indices in this
     * table have the same meaning as indices in `data', and mark
     * the _most recent_ of the three characters involved.
     * 
     * The list need only be singly linked: we detect the end of a
     * hash chain by observing that pos and hashnext[pos] are on
     * opposite sides of the maximum match distance.
     * 
     * If hashnext[pos] < 0, that also indicates the end of a hash
     * chain. This only comes up at the start of the algorithm
     * before the hash chain has been used.
     */
    int hashnext[WINSIZE];

    /*
     * The hash table. For each hash value, this stores the
     * location within the window of the most recent sequence of
     * three characters with that hash value, or -1 if there hasn't
     * been one yet.
     */
    int hashhead[HASHMAX];

    /*
     * This list links together the set of backward distances which
     * currently constitute valid matches. Unlike the other arrays
     * of size WINSIZE, this one is indexed by absolute backward
     * distance: its indices are not dependent on winpos.
     *
     * This list can contain multiple disjoint linked lists when
     * we're doing lazy matching. (Two candidate matches starting
     * at positions which differ by less than HASHCHARS cannot both
     * have the same backward distance, because if they did then
     * they'd be part of the same longer match and obviously the
     * one starting earlier would be preferable; so there can be no
     * collision within this array.)
     * 
     * matchnext[pos] is
     * 	- less than 0 if pos is the last element in such a list
     * 	- exactly 0 if pos is not part of such a list at all (this
     * 	  does not clash with a real backward distance because real
     * 	  backward distances must be at least 1!)
     * 	- otherwise gives the next shortest backward distance of a
     * 	  so-far valid match.
     */
    int matchnext[WINSIZE];

    /*
     * These variables track the heads of linked lists in
     * nextmatch. matchhead[i] is the list of possible matches
     * starting i bytes after the last output, or -1 if the list is
     * currently empty.
     */
    int matchhead[HASHCHARS];

    /*
     * These variables track the current length, and best backward
     * distance, of the matches listed in matchhead. Entries in
     * these two arrays can persist after matchhead[i] has become
     * -1, because only after we know how long all the matches are
     * going to be can we decide which one to output.
     */
    int matchlen[HASHCHARS], matchdist[HASHCHARS];

    /*
     * These are the literals saved during lazy matching: if we
     * output a match starting at k+i, we need i literals from
     * position k.
     */
    unsigned char literals[HASHCHARS-1];
};

static int lz77_hash(const unsigned char *data) {
    return (257*data[0] + 263*data[1] + 269*data[2]) % HASHMAX;
}

static LZ77 *lz77_new(void (*literal)(void *ctx, unsigned char c),
		      void (*match)(void *ctx, int distance, int len),
		      void *ctx)
{
    LZ77 *lz;
    int i;

    lz = (LZ77 *)malloc(sizeof(LZ77));
    if (!lz)
	return NULL;

    lz->ctx = ctx;
    lz->literal = literal;
    lz->match = match;

    for (i = 0; i < WINSIZE; i++) {
	lz->data[i] = 0;
	lz->hashnext[i] = -1;
	lz->matchnext[i] = 0;
    }

    for (i = 0; i < HASHMAX; i++) {
	lz->hashhead[i] = -1;
    }

    for (i = 0; i < HASHCHARS; i++) {
	lz->matchhead[i] = -1;
	lz->matchlen[i] = 0;
	lz->matchdist[i] = 0;
	lz->literals[i] = 0;
    }
    
    lz->winpos = 0;
    lz->k = 0;
    lz->nvalid = 0;
    lz->rewound = 0;

    return lz;
}

static void lz77_free(LZ77 *lz)
{
    free(lz);
}

static void lz77_hashsearch(LZ77 *lz, int hash, unsigned char *currchars)
{
    int pos, nextpos, matchlimit;

    assert(lz->matchhead[lz->k-HASHCHARS] < 0);
    assert(lz->matchdist[lz->k-HASHCHARS] == 0);
    assert(lz->matchlen[lz->k-HASHCHARS] == 0);

    if (lz->k < HASHCHARS+MAXLAZY-1) {
	/*
	 * Save the literal at this position, in case we need it.
	 */
	lz->literals[lz->k - HASHCHARS] =
	    lz->data[(lz->winpos + HASHCHARS-1) % WINSIZE];
    }

    /*
     * Find the most distant place in the window where we could
     * viably start a match. This is limited by the maximum match
     * distance, and it's also limited by the current amount of
     * valid data in the window.
     */
    matchlimit = (lz->nvalid < MAXMATCHDIST ? lz->nvalid : MAXMATCHDIST);
    matchlimit = (matchlimit + lz->winpos) % WINSIZE;

    pos = lz->hashhead[hash];
    while (pos != -1) {
	int bdist;
	int i;

	/*
	 * Compute the backward distance.
	 */
	bdist = (pos + WINSIZE - lz->winpos) % WINSIZE;

	/*
	 * If there's already a match being tracked at this
	 * distance, don't bother trying this one. Also, it's
	 * possible this match may be in the future (because if we
	 * rewound the compressor, some entries at the head of the
	 * hash chain will not be valid again _yet_), so check that
	 * too.
	 */
	if (bdist > 0 && bdist <= MAXMATCHDIST && !lz->matchnext[bdist]) {

	    /*
	     * Make sure the characters at pos really do match the
	     * ones in currchars.
	     */
	    for (i = 0; i < HASHCHARS; i++)
		if (lz->data[(pos + i) % WINSIZE] != currchars[i])
		    break;
	    if (i == HASHCHARS) {
		/*
		 * They did, so this is a valid match position. Add
		 * it to the list of possible match locations.
		 */
		lz->matchnext[bdist] = lz->matchhead[lz->k-HASHCHARS];
		lz->matchhead[lz->k-HASHCHARS] = bdist;
		/*
		 * We're working through the window from most to
		 * least recent, and we want to prefer matching
		 * against more recent data; so here we only set
		 * matchdist on the first (i.e. most recent) match
		 * we find.
		 */
		if (!lz->matchlen[lz->k-HASHCHARS]) {
		    lz->matchlen[lz->k-HASHCHARS] = HASHCHARS;
		    lz->matchdist[lz->k-HASHCHARS] = bdist;
		}
	    }
	}

	/*
	 * Step along the hash chain to try the next candidate
	 * location. If we've gone past matchlimit, stop.
	 * 
	 * Special case: we could also link to ourself here. This
	 * occurs when this window position is repeating a previous
	 * hash and nothing else has had the same hash in between.
	 */
	nextpos = lz->hashnext[pos];
	if (nextpos == pos ||
	    (matchlimit + WINSIZE - nextpos) % WINSIZE >
	    (matchlimit + WINSIZE - pos) % WINSIZE)
	    break;
	else
	    pos = nextpos;
    }
}

static void lz77_outputmatch(LZ77 *lz)
{
    int besti = -1, bestval = -1;
    int i;

    /*
     * Decide which match to output. The rule is: in order for a
     * match starting i characters later than another one to be
     * worth choosing, it must be at least i characters longer as
     * well.
     *
     * This reduces to a concrete algorithm in which we subtract i
     * from the length of match[i], and pick the largest of the
     * resulting numbers, breaking ties in favour of _later_
     * matches.
     */
    for (i = MAXLAZY; i-- > 0 ;)
	if (lz->matchlen[i] - i > bestval) {
	    bestval = lz->matchlen[i] - i;
	    besti = i;
	}

    /*
     * Output the match plus its pre-literals (if any).
     */
    for (i = 0; i < besti; i++)
	lz->literal(lz->ctx, lz->literals[i]);
    lz->match(lz->ctx, lz->matchdist[besti], lz->matchlen[besti]);

    /*
     * Decrease k by the amount of data we've just output.
     */
    lz->k -= besti + lz->matchlen[besti];

    /*
     * Clean out the matchdist and matchlen arrays.
     */
    for (i = 0; i < HASHCHARS; i++)
	lz->matchdist[i] = lz->matchlen[i] = 0;

    /*
     * If k is HASHCHARS or more, we must rewind the compressor
     * state by a few bytes so that we can reprocess those bytes
     * and look for new matches starting there. Because the window
     * size is a few bytes more than MAXMATCHDIST, this does not
     * impact our ability to find matches at the maximum distance
     * even after this rewinding.
     */
    if (lz->k >= HASHCHARS) {
	int rw = lz->k - (HASHCHARS-1);

	assert(lz->rewound + rw < HASHCHARS);

	lz->winpos = (lz->winpos + rw) % WINSIZE;
	lz->rewound += rw;
	lz->k -= rw;
	lz->nvalid -= rw;
    }
}

static void lz77_flush(LZ77 *lz)
{
    int i;

    while (lz->k >= HASHCHARS) {
	/*
	 * Output a match.
	 */
	lz77_outputmatch(lz);

	/*
	 * Clean up the match lists.
	 */
	for (i = 0; i < HASHCHARS; i++) {
	    while (lz->matchhead[i] >= 0) {
		int tmp = lz->matchhead[i];

		lz->matchhead[i] = lz->matchnext[tmp];
		lz->matchnext[tmp] = 0;
	    }
	}

	/*
	 * In case we've rewound, call the main compress function
	 * with length zero to reprocess the rewound data. Then we
	 * might go back round this loop if that spotted a final
	 * match after the one we output above.
	 */
	lz77_compress(lz, NULL, 0);
    }

    assert(lz->k < HASHCHARS);
    /*
     * Now just output literals until we reduce k to zero.
     */
    while (lz->k > 0) {
	lz->k--;
	lz->literal(lz->ctx, lz->data[(lz->winpos + lz->k) % WINSIZE]);
    }
}

static void lz77_compress(LZ77 *lz, const void *vdata, int len)
{
    const unsigned char *data = (const unsigned char *)vdata;

    while (lz->rewound > 0 || len > 0) {
	unsigned char currchars[HASHCHARS];
	int hash;
	int rewound_this_time;

	/*
	 * First thing we do with every character we see: add it to
	 * the sliding window, and shift the window round.
	 */
	lz->winpos = (lz->winpos + WINSIZE-1) % WINSIZE;
	if (lz->rewound == 0) {
	    len--;
	    lz->data[lz->winpos] = *data++;
	    rewound_this_time = 0;
	} else {
	    lz->rewound--;
	    rewound_this_time = 1;
	}
	lz->k++;
	if (lz->nvalid < WINSIZE)
	    lz->nvalid++;

	/*
	 * If we're still within one hash length of the start of
	 * the input data, there really is nothing more we can do
	 * at all just yet.
	 */
	if (lz->nvalid < HASHCHARS)
	    continue;

	/*
	 * Hash the most recent three characters.
	 */
	{
	    int i;
	    for (i = 0; i < HASHCHARS; i++)
		currchars[i] = lz->data[(lz->winpos + i) % WINSIZE];
	    hash = lz77_hash(currchars);
	}

	/*
	 * If k is within the range [HASHCHARS, HASHCHARS+MAXLAZY),
	 * make a list of matches starting HASHCHARS bytes before
	 * the end of the window, and store the head of the list in
	 * matchhead[k-HASHCHARS].
	 */
	if (lz->k >= HASHCHARS && lz->k < HASHCHARS+MAXLAZY)
	    lz77_hashsearch(lz, hash, currchars);

	/*
	 * We never let k get bigger than HASHCHARS unless there's
	 * a match starting at it. So if k==HASHCHARS and there
	 * isn't such a match, output a literal and decrement k.
	 */
	if (lz->k == HASHCHARS && lz->matchhead[0] < 0) {
	    lz->literal(lz->ctx, currchars[HASHCHARS-1]);
	    lz->k--;
	}

	/*
	 * For each list of matches we're currently tracking which
	 * we _haven't_ only just started, go through the list and
	 * winnow it to only those which have continued to match.
	 */
	if (lz->k > HASHCHARS) {
	    int i;
	    for (i = 0; i < MAXLAZY && lz->k-i > HASHCHARS; i++) {
		int outpos = -1, inpos = lz->matchhead[i], nextinpos;
		int bestdist = 0; 

		while (inpos >= 0) {
		    nextinpos = lz->matchnext[inpos];

		    /*
		     * See if this match is still going.
		     */
		    if (
#ifdef MAXMATCHLEN
			lz->matchlen[i] < MAXMATCHLEN &&
#endif

			lz->data[(lz->winpos + inpos) % WINSIZE] ==
			lz->data[lz->winpos]) {
			/*
			 * It is; put it back on the winnowed list.
			 */
			if (outpos < 0)
			    lz->matchhead[i] = inpos;
			else
			    lz->matchnext[outpos] = inpos;
			outpos = inpos;
			/*
			 * Because we built up the match list in
			 * reverse order compared to the hash
			 * chain, we must prefer _later_ entries in
			 * the match list in order to prefer
			 * matching against the most recent data.
			 */
			bestdist = inpos;
		    } else {
			/*
			 * It isn't; mark it as unused in the
			 * matchnext array.
			 */
			lz->matchnext[inpos] = 0;
		    }

		    inpos = nextinpos;
		}

		/*
		 * Terminate the new list.
		 */
		if (outpos < 0)
		    lz->matchhead[i] = -1;
		else
		    lz->matchnext[outpos] = -1;

		/*
		 * And update the distance/length tracker for this
		 * match.
		 */
		if (bestdist) {
		    lz->matchdist[i] = bestdist;
		    lz->matchlen[i]++;
		}
	    }
	}

	/*
	 * Add the current window position to the head of the
	 * appropriate hash chain.
	 * 
	 * (If the compressor is still recovering from a rewind,
	 * this position will _already_ be on its hash chain, so we
	 * don't do this.)
	 */
	if (!rewound_this_time) {
	    lz->hashnext[lz->winpos] = lz->hashhead[hash];
	    lz->hashhead[hash] = lz->winpos;
	}

	/*
	 * If k >= HASHCHARS+MAXLAZY-1 (meaning that we've had a
	 * chance to try a match at every viable starting point)
	 * and there are no ongoing matches, we must output
	 * something.
	 */
	if (lz->k >= HASHCHARS+MAXLAZY-1) {
	    int i;
	    for (i = 0; i < HASHCHARS; i++)
		if (lz->matchhead[i] >= 0)
		    break;
	    if (i == HASHCHARS)
		lz77_outputmatch(lz);
	}
    }
}

/* ----------------------------------------------------------------------
 * Deflate functionality common to both compression and decompression.
 */

static const unsigned char lenlenmap[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

#define MAXCODELEN 16

/*
 * Given a sequence of Huffman code lengths, compute the actual
 * codes, in the final form suitable for feeding to outbits (i.e.
 * already bit-mirrored).
 *
 * Returns the maximum code length found.
 */
static int hufcodes(const unsigned char *lengths, int *codes, int nsyms)
{
    int count[MAXCODELEN], startcode[MAXCODELEN];
    int code, maxlen;
    int i, j;

    /* Count the codes of each length. */
    maxlen = 0;
    for (i = 1; i < MAXCODELEN; i++)
	count[i] = 0;
    for (i = 0; i < nsyms; i++) {
	count[lengths[i]]++;
	if (maxlen < lengths[i])
	    maxlen = lengths[i];
    }
    /* Determine the starting code for each length block. */
    code = 0;
    for (i = 1; i < MAXCODELEN; i++) {
	startcode[i] = code;
	code += count[i];
	code <<= 1;
    }
    /* Determine the code for each symbol. Mirrored, of course. */
    for (i = 0; i < nsyms; i++) {
	code = startcode[lengths[i]]++;
	codes[i] = 0;
	for (j = 0; j < lengths[i]; j++) {
	    codes[i] = (codes[i] << 1) | (code & 1);
	    code >>= 1;
	}
    }

    return maxlen;
}

/* ----------------------------------------------------------------------
 * Deflate compression.
 */

#define SYMLIMIT 65536
#define SYMPFX_LITLEN    0x00000000U
#define SYMPFX_DIST      0x40000000U
#define SYMPFX_EXTRABITS 0x80000000U
#define SYMPFX_CODELEN   0xC0000000U
#define SYMPFX_MASK      0xC0000000U

#define SYM_EXTRABITS_MASK 0x3C000000U
#define SYM_EXTRABITS_SHIFT 26

struct deflate_compress_ctx {
    LZ77 *lz;
    unsigned char *outbuf;
    int outlen, outsize;
    unsigned long outbits;
    int noutbits;
    int firstblock;
    unsigned long *syms;
    int symstart, nsyms;
    int type;
    unsigned long adler32;
    int lastblock;
    int finished;
#ifdef STATISTICS
    unsigned long bitcount;
#endif
};

static void outbits(deflate_compress_ctx *out,
		    unsigned long bits, int nbits)
{
    assert(out->noutbits + nbits <= 32);
    out->outbits |= bits << out->noutbits;
    out->noutbits += nbits;
    while (out->noutbits >= 8) {
	if (out->outlen >= out->outsize) {
	    out->outsize = out->outlen + 64;
	    out->outbuf = sresize(out->outbuf, out->outsize, unsigned char);
	}
	out->outbuf[out->outlen++] = (unsigned char) (out->outbits & 0xFF);
	out->outbits >>= 8;
	out->noutbits -= 8;
    }
#ifdef STATISTICS
    out->bitcount += nbits;
#endif
}

/*
 * Binary heap functions used by buildhuf(). Each one assumes the
 * heap to be stored in an array of ints, with two ints per node
 * (user data and key). They take in the old heap length, and
 * return the new one.
 */
#define HEAPPARENT(x) (((x)-2)/4*2)
#define HEAPLEFT(x) ((x)*2+2)
#define HEAPRIGHT(x) ((x)*2+4)
static int addheap(int *heap, int len, int userdata, int key)
{
    int me, dad, tmp;

    me = len;
    heap[len++] = userdata;
    heap[len++] = key;

    while (me > 0) {
	dad = HEAPPARENT(me);
	if (heap[me+1] < heap[dad+1]) {
	    tmp = heap[me]; heap[me] = heap[dad]; heap[dad] = tmp;
	    tmp = heap[me+1]; heap[me+1] = heap[dad+1]; heap[dad+1] = tmp;
	    me = dad;
	} else
	    break;
    }

    return len;
}
static int rmheap(int *heap, int len, int *userdata, int *key)
{
    int me, lc, rc, c, tmp;

    len -= 2;
    *userdata = heap[0];
    *key = heap[1];
    heap[0] = heap[len];
    heap[1] = heap[len+1];

    me = 0;

    while (1) {
	lc = HEAPLEFT(me);
	rc = HEAPRIGHT(me);
	if (lc >= len)
	    break;
	else if (rc >= len || heap[lc+1] < heap[rc+1])
	    c = lc;
	else
	    c = rc;
	if (heap[me+1] > heap[c+1]) {
	    tmp = heap[me]; heap[me] = heap[c]; heap[c] = tmp;
	    tmp = heap[me+1]; heap[me+1] = heap[c+1]; heap[c+1] = tmp;
	} else
	    break;
	me = c;
    }

    return len;
}

/*
 * The core of the Huffman algorithm: takes an input array of
 * symbol frequencies, and produces an output array of code
 * lengths.
 *
 * This is basically a generic Huffman implementation, but it has
 * one zlib-related quirk which is that it caps the output code
 * lengths to fit in an unsigned char (which is safe since Deflate
 * will reject anything longer than 15 anyway). Anyone wanting to
 * rip it out and use it in another context should find that easy
 * to remove.
 */
#define HUFMAX 286
static void buildhuf(const int *freqs, unsigned char *lengths, int nsyms)
{
    int parent[2*HUFMAX-1];
    int length[2*HUFMAX-1];
    int heap[2*HUFMAX];
    int heapsize;
    int i, j, n;
    int si, sj;

    assert(nsyms <= HUFMAX);

    memset(parent, 0, sizeof(parent));

    /*
     * Begin by building the heap.
     */
    heapsize = 0;
    for (i = 0; i < nsyms; i++)
	if (freqs[i] > 0)	       /* leave unused symbols out totally */
	    heapsize = addheap(heap, heapsize, i, freqs[i]);

    /*
     * Now repeatedly take two elements off the heap and merge
     * them.
     */
    n = HUFMAX;
    while (heapsize > 2) {
	heapsize = rmheap(heap, heapsize, &i, &si);
	heapsize = rmheap(heap, heapsize, &j, &sj);
	parent[i] = n;
	parent[j] = n;
	heapsize = addheap(heap, heapsize, n, si + sj);
	n++;
    }

    /*
     * Now we have our tree, in the form of a link from each node
     * to the index of its parent. Count back down the tree to
     * determine the code lengths.
     */
    memset(length, 0, sizeof(length));
    /* The tree root has length 0 after that, which is correct. */
    for (i = n-1; i-- ;)
	if (parent[i] > 0)
	    length[i] = 1 + length[parent[i]];

    /*
     * And that's it. (Simple, wasn't it?) Copy the lengths into
     * the output array and leave.
     * 
     * Here we cap lengths to fit in unsigned char.
     */
    for (i = 0; i < nsyms; i++)
	lengths[i] = (length[i] > 255 ? 255 : length[i]);
}

/*
 * Wrapper around buildhuf() which enforces the Deflate restriction
 * that no code length may exceed 15 bits, or 7 for the auxiliary
 * code length alphabet. This function has the same calling
 * semantics as buildhuf(), except that it might modify the freqs
 * array.
 */
static void deflate_buildhuf(int *freqs, unsigned char *lengths,
			     int nsyms, int limit)
{
    int smallestfreq, totalfreq, nactivesyms;
    int num, denom, adjust;
    int i;
    int maxprob;

    /*
     * First, try building the Huffman table the normal way. If
     * this works, it's optimal, so we don't want to mess with it.
     */
    buildhuf(freqs, lengths, nsyms);

    for (i = 0; i < nsyms; i++)
	if (lengths[i] > limit)
	    break;

    if (i == nsyms)
	return;			       /* OK */

    /*
     * The Huffman algorithm can only ever generate a code length
     * of N bits or more if there is a symbol whose probability is
     * less than the reciprocal of the (N+2)th Fibonacci number
     * (counting from F_0=0 and F_1=1), i.e. 1/2584 for N=16, or
     * 1/55 for N=8. (This is a necessary though not sufficient
     * condition.)
     *
     * Why is this? Well, consider the input symbol with the
     * smallest probability. Let that probability be x. In order
     * for this symbol to have a code length of at least 1, the
     * Huffman algorithm will have to merge it with some other
     * node; and since x is the smallest probability, the node it
     * gets merged with must be at least x. Thus, the probability
     * of the resulting combined node will be at least 2x. Now in
     * order for our node to reach depth 2, this 2x-node must be
     * merged again. But what with? We can't assume the node it
     * merges with is at least 2x, because this one might only be
     * the _second_ smallest remaining node. But we do know the
     * node it merges with must be at least x, so our order-2
     * internal node is at least 3x.
     *
     * How small a node can merge with _that_ to get an order-3
     * internal node? Well, it must be at least 2x, because if it
     * was smaller than that then it would have been one of the two
     * smallest nodes in the previous step and been merged at that
     * point. So at least 3x, plus at least 2x, comes to at least
     * 5x for an order-3 node.
     *
     * And so it goes on: at every stage we must merge our current
     * node with a node at least as big as the bigger of this one's
     * two parents, and from this starting point that gives rise to
     * the Fibonacci sequence. So we find that in order to have a
     * node n levels deep (i.e. a maximum code length of n), the
     * overall probability of the root of the entire tree must be
     * at least F_{n+2} times the probability of the rarest symbol.
     * In other words, since the overall probability is 1, it is a
     * necessary condition for a code length of 16 or more that
     * there must be at least one symbol with probability <=
     * 1/F_18.
     *
     * (To demonstrate that a probability this big really can give
     * rise to a code length of 16, consider the set of input
     * frequencies { 1-epsilon, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55,
     * 89, 144, 233, 377, 610, 987 }, for arbitrarily small
     * epsilon.)
     *
     * So here buildhuf() has returned us an overlong code. So to
     * ensure it doesn't do it again, we add a constant to all the
     * (non-zero) symbol frequencies, causing them to become more
     * balanced and removing the danger. We can then feed the
     * results back to the standard buildhuf() and be
     * assert()-level confident that the resulting code lengths
     * contain nothing outside the permitted range.
     */
    maxprob = (limit == 16 ? 2584 : 55);   /* no point in computing full F_n */
    totalfreq = nactivesyms = 0;
    smallestfreq = -1;
    for (i = 0; i < nsyms; i++) {
	if (freqs[i] == 0)
	    continue;
	if (smallestfreq < 0 || smallestfreq > freqs[i])
	    smallestfreq = freqs[i];
	totalfreq += freqs[i];
	nactivesyms++;
    }
    assert(smallestfreq <= totalfreq / maxprob);

    /*
     * We want to find the smallest integer `adjust' such that
     * (totalfreq + nactivesyms * adjust) / (smallestfreq +
     * adjust) is less than maxprob. A bit of algebra tells us
     * that the threshold value is equal to
     *
     *   totalfreq - maxprob * smallestfreq
     *   ----------------------------------
     *          maxprob - nactivesyms
     *
     * rounded up, of course. And we'll only even be trying
     * this if
     */
    num = totalfreq - smallestfreq * maxprob;
    denom = maxprob - nactivesyms;
    adjust = (num + denom - 1) / denom;

    /*
     * Now add `adjust' to all the input symbol frequencies.
     */
    for (i = 0; i < nsyms; i++)
	if (freqs[i] != 0)
	    freqs[i] += adjust;

    /*
     * Rebuild the Huffman tree...
     */
    buildhuf(freqs, lengths, nsyms);

    /*
     * ... and this time it ought to be OK.
     */
    for (i = 0; i < nsyms; i++)
	assert(lengths[i] <= limit);
}

struct huftrees {
    unsigned char *len_litlen;
    int *code_litlen;
    unsigned char *len_dist;
    int *code_dist;
    unsigned char *len_codelen;
    int *code_codelen;
};

/*
 * Write out a single symbol, given the three Huffman trees.
 */
static void writesym(deflate_compress_ctx *out,
		     unsigned sym, struct huftrees *trees)
{
    unsigned basesym = sym &~ SYMPFX_MASK;
    int i;

    switch (sym & SYMPFX_MASK) {
      case SYMPFX_LITLEN:
	debug(("send: litlen %d\n", basesym));
	outbits(out, trees->code_litlen[basesym], trees->len_litlen[basesym]);
	break;
      case SYMPFX_DIST:
	debug(("send: dist %d\n", basesym));
	outbits(out, trees->code_dist[basesym], trees->len_dist[basesym]);
	break;
      case SYMPFX_CODELEN:
	debug(("send: codelen %d\n", basesym));
	outbits(out, trees->code_codelen[basesym],trees->len_codelen[basesym]);
	break;
      case SYMPFX_EXTRABITS:
	i = basesym >> SYM_EXTRABITS_SHIFT;
	basesym &= ~SYM_EXTRABITS_MASK;
	debug(("send: extrabits %d/%d\n", basesym, i));
	outbits(out, basesym, i);
	break;
    }
}

static void outblock(deflate_compress_ctx *out,
		     int blklen, int dynamic)
{
    int freqs1[286], freqs2[30], freqs3[19];
    unsigned char len1[286], len2[30], len3[19];
    int code1[286], code2[30], code3[19];
    int hlit, hdist, hclen, bfinal, btype;
    int treesrc[286 + 30];
    int treesyms[286 + 30];
    int codelen[19];
    int i, ntreesrc, ntreesyms;
    struct huftrees ht;
#ifdef STATISTICS
    unsigned long bitcount_before;
#endif

    ht.len_litlen = len1;
    ht.len_dist = len2;
    ht.len_codelen = len3;
    ht.code_litlen = code1;
    ht.code_dist = code2;
    ht.code_codelen = code3;

    /*
     * Build the two main Huffman trees.
     */
    if (dynamic) {
	/*
	 * Count up the frequency tables.
	 */
	memset(freqs1, 0, sizeof(freqs1));
	memset(freqs2, 0, sizeof(freqs2));
	freqs1[256] = 1;	       /* we're bound to need one EOB */
	for (i = 0; i < blklen; i++) {
	    unsigned sym = out->syms[(out->symstart + i) % SYMLIMIT];

	    /*
	     * Increment the occurrence counter for this symbol, if
	     * it's in one of the Huffman alphabets and isn't extra
	     * bits.
	     */
	    if ((sym & SYMPFX_MASK) == SYMPFX_LITLEN) {
		sym &= ~SYMPFX_MASK;
		assert(sym < lenof(freqs1));
		freqs1[sym]++;
	    } else if ((sym & SYMPFX_MASK) == SYMPFX_DIST) {
		sym &= ~SYMPFX_MASK;
		assert(sym < lenof(freqs2));
		freqs2[sym]++;
	    }
	}
	deflate_buildhuf(freqs1, len1, lenof(freqs1), 15);
	deflate_buildhuf(freqs2, len2, lenof(freqs2), 15);
    } else {
	/*
	 * Fixed static trees.
	 */
	for (i = 0; i < lenof(len1); i++)
	    len1[i] = (i < 144 ? 8 :
		       i < 256 ? 9 :
		       i < 280 ? 7 : 8);
	for (i = 0; i < lenof(len2); i++)
	    len2[i] = 5;
    }
    hufcodes(len1, code1, lenof(freqs1));
    hufcodes(len2, code2, lenof(freqs2));

    if (dynamic) {
	/*
	 * Determine HLIT and HDIST.
	 */
	for (hlit = 286; hlit > 257 && len1[hlit-1] == 0; hlit--);
	for (hdist = 30; hdist > 1 && len2[hdist-1] == 0; hdist--);

	/*
	 * Write out the list of symbols used to transmit the
	 * trees.
	 */
	ntreesrc = 0;
	for (i = 0; i < hlit; i++)
	    treesrc[ntreesrc++] = len1[i];
	for (i = 0; i < hdist; i++)
	    treesrc[ntreesrc++] = len2[i];
	ntreesyms = 0;
	for (i = 0; i < ntreesrc ;) {
	    int j = 1;
	    int k;

	    /* Find length of run of the same length code. */
	    while (i+j < ntreesrc && treesrc[i+j] == treesrc[i])
		j++;

	    /* Encode that run as economically as we can. */
	    k = j;
	    if (treesrc[i] == 0) {
		/*
		 * Zero code length: we can output run codes for
		 * 3-138 zeroes. So if we have fewer than 3 zeroes,
		 * we just output literals. Otherwise, we output
		 * nothing but run codes, and tweak their lengths
		 * to make sure we aren't left with under 3 at the
		 * end.
		 */
		if (k < 3) {
		    while (k--)
			treesyms[ntreesyms++] = 0 | SYMPFX_CODELEN;
		} else {
		    while (k > 0) {
			int rpt = (k < 138 ? k : 138);
			if (rpt > k-3 && rpt < k)
			    rpt = k-3;
			assert(rpt >= 3 && rpt <= 138);
			if (rpt < 11) {
			    treesyms[ntreesyms++] = 17 | SYMPFX_CODELEN;
			    treesyms[ntreesyms++] =
				(SYMPFX_EXTRABITS | (rpt - 3) |
				 (3 << SYM_EXTRABITS_SHIFT));
			} else {
			    treesyms[ntreesyms++] = 18 | SYMPFX_CODELEN;
			    treesyms[ntreesyms++] =
				(SYMPFX_EXTRABITS | (rpt - 11) |
				 (7 << SYM_EXTRABITS_SHIFT));
			}
			k -= rpt;
		    }
		}
	    } else {
		/*
		 * Non-zero code length: we must output the first
		 * one explicitly, then we can output a copy code
		 * for 3-6 repeats. So if we have fewer than 4
		 * repeats, we _just_ output literals. Otherwise,
		 * we output one literal plus at least one copy
		 * code, and tweak the copy codes to make sure we
		 * aren't left with under 3 at the end.
		 */
		assert(treesrc[i] < 16);
		treesyms[ntreesyms++] = treesrc[i] | SYMPFX_CODELEN;
		k--;
		if (k < 3) {
		    while (k--)
			treesyms[ntreesyms++] = treesrc[i] | SYMPFX_CODELEN;
		} else {
		    while (k > 0) {
			int rpt = (k < 6 ? k : 6);
			if (rpt > k-3 && rpt < k)
			    rpt = k-3;
			assert(rpt >= 3 && rpt <= 6);
			treesyms[ntreesyms++] = 16 | SYMPFX_CODELEN;
			treesyms[ntreesyms++] = (SYMPFX_EXTRABITS | (rpt - 3) |
						 (2 << SYM_EXTRABITS_SHIFT));
			k -= rpt;
		    }
		}
	    }

	    i += j;
	}
	assert((unsigned)ntreesyms < lenof(treesyms));

	/*
	 * Count up the frequency table for the tree-transmission
	 * symbols, and build the auxiliary Huffman tree for that.
	 */
	memset(freqs3, 0, sizeof(freqs3));
	for (i = 0; i < ntreesyms; i++) {
	    unsigned sym = treesyms[i];

	    /*
	     * Increment the occurrence counter for this symbol, if
	     * it's the Huffman alphabet and isn't extra bits.
	     */
	    if ((sym & SYMPFX_MASK) == SYMPFX_CODELEN) {
		sym &= ~SYMPFX_MASK;
		assert(sym < lenof(freqs3));
		freqs3[sym]++;
	    }
	}
	deflate_buildhuf(freqs3, len3, lenof(freqs3), 7);
	hufcodes(len3, code3, lenof(freqs3));

	/*
	 * Reorder the code length codes into transmission order, and
	 * determine HCLEN.
	 */
	for (i = 0; i < 19; i++)
	    codelen[i] = len3[lenlenmap[i]];
	for (hclen = 19; hclen > 4 && codelen[hclen-1] == 0; hclen--);
    } else {
	hlit = hdist = hclen = ntreesyms = 0;   /* placate optimiser */
    }

    /*
     * Actually transmit the block.
     */

    /* 3-bit block header */
    bfinal = (out->lastblock ? 1 : 0);
    btype = dynamic ? 2 : 1;
    debug(("send: bfinal=%d btype=%d\n", bfinal, btype));
    outbits(out, bfinal, 1);
    outbits(out, btype, 2);

#ifdef STATISTICS
    bitcount_before = out->bitcount;
#endif

    if (dynamic) {
	/* HLIT, HDIST and HCLEN */
	debug(("send: hlit=%d hdist=%d hclen=%d\n", hlit, hdist, hclen));
	outbits(out, hlit - 257, 5);
	outbits(out, hdist - 1, 5);
	outbits(out, hclen - 4, 4);

	/* Code lengths for the auxiliary tree */
	for (i = 0; i < hclen; i++) {
	    debug(("send: lenlen %d\n", codelen[i]));
	    outbits(out, codelen[i], 3);
	}

	/* Code lengths for the literal/length and distance trees */
	for (i = 0; i < ntreesyms; i++)
	    writesym(out, treesyms[i], &ht);
#ifdef STATISTICS
	fprintf(stderr, "total tree size %lu bits\n",
		out->bitcount - bitcount_before);
#endif
    }

    /* Output the actual symbols from the buffer */
    for (i = 0; i < blklen; i++) {
	unsigned sym = out->syms[(out->symstart + i) % SYMLIMIT];
	writesym(out, sym, &ht);
    }

    /* Output the end-of-data symbol */
    writesym(out, SYMPFX_LITLEN | 256, &ht);

    /*
     * Remove all the just-output symbols from the symbol buffer by
     * adjusting symstart and nsyms.
     */
    out->symstart = (out->symstart + blklen) % SYMLIMIT;
    out->nsyms -= blklen;
}

static void outblock_wrapper(deflate_compress_ctx *out,
			     int best_dynamic_len)
{
    /*
     * Final block choice function: we have the option of either
     * outputting a dynamic block of length best_dynamic_len, or a
     * static block of length out->nsyms. Whichever gives us the
     * best value for money, we do.
     *
     * FIXME: currently we always choose dynamic except for empty
     * blocks. We should make a sensible judgment.
     */
    if (out->nsyms == 0)
	outblock(out, 0, FALSE);
    else
	outblock(out, best_dynamic_len, TRUE);
}

static void chooseblock(deflate_compress_ctx *out)
{
    int freqs1[286], freqs2[30];
    int i, bestlen;
    double bestvfm;
    int nextrabits;

    memset(freqs1, 0, sizeof(freqs1));
    memset(freqs2, 0, sizeof(freqs2));
    freqs1[256] = 1;		       /* we're bound to need one EOB */
    nextrabits = 0;

    /*
     * Iterate over all possible block lengths, computing the
     * entropic coding approximation to the final length at every
     * stage. We divide the result by the number of symbols
     * encoded, to determine the `value for money' (overall
     * bits-per-symbol count) of a block of that length.
     */
    bestlen = -1;
    bestvfm = 0.0;
    for (i = 0; i < out->nsyms; i++) {
	unsigned sym = out->syms[(out->symstart + i) % SYMLIMIT];

	if (i > 0 && (sym & SYMPFX_MASK) == SYMPFX_LITLEN) {
	    /*
	     * This is a viable point at which to end the block.
	     * Compute the length approximation and hence the value
	     * for money.
	     */
	    double len = 0.0, vfm;
	    int k;
	    int total;

	    /*
	     * FIXME: we should be doing this incrementally, rather
	     * than recomputing the whole thing at every byte
	     * position. Also, can we fiddle the logs somehow to
	     * avoid having to do floating point?
	     */
	    total = 0;
	    for (k = 0; k < (int)lenof(freqs1); k++) {
		if (freqs1[k])
		    len -= freqs1[k] * log(freqs1[k]);
		total += freqs1[k];
	    }
	    if (total)
		len += total * log(total);
	    total = 0;
	    for (k = 0; k < (int)lenof(freqs2); k++) {
		if (freqs2[k])
		    len -= freqs2[k] * log(freqs2[k]);
		total += freqs2[k];
	    }
	    if (total)
		len += total * log(total);
	    len /= log(2);
	    len += nextrabits;
	    len += 300;   /* very approximate size of the Huffman trees */

	    vfm = i / len;	       /* symbols encoded per bit */
/* fprintf(stderr, "chooseblock: i=%d gives len %g, vfm %g\n", i, len, vfm); */
	    if (bestlen < 0 || vfm > bestvfm) {
		bestlen = i;
		bestvfm = vfm;
	    }
	}

	/*
	 * Increment the occurrence counter for this symbol, if
	 * it's in one of the Huffman alphabets and isn't extra
	 * bits.
	 */
	if ((sym & SYMPFX_MASK) == SYMPFX_LITLEN) {
	    sym &= ~SYMPFX_MASK;
	    assert(sym < lenof(freqs1));
	    freqs1[sym]++;
	} else if ((sym & SYMPFX_MASK) == SYMPFX_DIST) {
	    sym &= ~SYMPFX_MASK;
	    assert(sym < lenof(freqs2));
	    freqs2[sym]++;
	} else if ((sym & SYMPFX_MASK) == SYMPFX_EXTRABITS) {
	    nextrabits += (sym &~ SYMPFX_MASK) >> SYM_EXTRABITS_SHIFT;
	}
    }

    assert(bestlen > 0);

/* fprintf(stderr, "chooseblock: bestlen %d, bestvfm %g\n", bestlen, bestvfm); */
    outblock_wrapper(out, bestlen);
}

/*
 * Force the current symbol buffer to be flushed out as a single
 * block.
 */
static void flushblock(deflate_compress_ctx *out)
{
    /*
     * Because outblock_wrapper guarantees to output either a
     * dynamic block of the given length or a static block of
     * length out->nsyms, we know that passing out->nsyms as the
     * given length will definitely result in us using up the
     * entire buffer.
     */
    outblock_wrapper(out, out->nsyms);
    assert(out->nsyms == 0);
}

/*
 * Place a symbol into the symbols buffer.
 */
static void outsym(deflate_compress_ctx *out, unsigned long sym)
{
    assert(out->nsyms < SYMLIMIT);
    out->syms[(out->symstart + out->nsyms++) % SYMLIMIT] = sym;

    if (out->nsyms == SYMLIMIT)
	chooseblock(out);
}

typedef struct {
    short code, extrabits;
    int min, max;
} coderecord;

static const coderecord lencodes[] = {
    {257, 0, 3, 3},
    {258, 0, 4, 4},
    {259, 0, 5, 5},
    {260, 0, 6, 6},
    {261, 0, 7, 7},
    {262, 0, 8, 8},
    {263, 0, 9, 9},
    {264, 0, 10, 10},
    {265, 1, 11, 12},
    {266, 1, 13, 14},
    {267, 1, 15, 16},
    {268, 1, 17, 18},
    {269, 2, 19, 22},
    {270, 2, 23, 26},
    {271, 2, 27, 30},
    {272, 2, 31, 34},
    {273, 3, 35, 42},
    {274, 3, 43, 50},
    {275, 3, 51, 58},
    {276, 3, 59, 66},
    {277, 4, 67, 82},
    {278, 4, 83, 98},
    {279, 4, 99, 114},
    {280, 4, 115, 130},
    {281, 5, 131, 162},
    {282, 5, 163, 194},
    {283, 5, 195, 226},
    {284, 5, 227, 257},
    {285, 0, 258, 258},
};

static const coderecord distcodes[] = {
    {0, 0, 1, 1},
    {1, 0, 2, 2},
    {2, 0, 3, 3},
    {3, 0, 4, 4},
    {4, 1, 5, 6},
    {5, 1, 7, 8},
    {6, 2, 9, 12},
    {7, 2, 13, 16},
    {8, 3, 17, 24},
    {9, 3, 25, 32},
    {10, 4, 33, 48},
    {11, 4, 49, 64},
    {12, 5, 65, 96},
    {13, 5, 97, 128},
    {14, 6, 129, 192},
    {15, 6, 193, 256},
    {16, 7, 257, 384},
    {17, 7, 385, 512},
    {18, 8, 513, 768},
    {19, 8, 769, 1024},
    {20, 9, 1025, 1536},
    {21, 9, 1537, 2048},
    {22, 10, 2049, 3072},
    {23, 10, 3073, 4096},
    {24, 11, 4097, 6144},
    {25, 11, 6145, 8192},
    {26, 12, 8193, 12288},
    {27, 12, 12289, 16384},
    {28, 13, 16385, 24576},
    {29, 13, 24577, 32768},
};

static void literal(void *vctx, unsigned char c)
{
    deflate_compress_ctx *out = (deflate_compress_ctx *)vctx;

    outsym(out, SYMPFX_LITLEN | c);
}

static void match(void *vctx, int distance, int len)
{
    const coderecord *d, *l;
    int i, j, k;
    deflate_compress_ctx *out = (deflate_compress_ctx *)vctx;

    assert(len >= 3 && len <= 258);

    /*
     * Binary-search to find which length code we're
     * transmitting.
     */
    i = -1;
    j = sizeof(lencodes) / sizeof(*lencodes);
    while (1) {
	assert(j - i >= 2);
	k = (j + i) / 2;
	if (len < lencodes[k].min)
	    j = k;
	else if (len > lencodes[k].max)
	    i = k;
	else {
	    l = &lencodes[k];
	    break;		       /* found it! */
	}
    }

    /*
     * Transmit the length code.
     */
    outsym(out, SYMPFX_LITLEN | l->code);

    /*
     * Transmit the extra bits.
     */
    if (l->extrabits) {
	outsym(out, (SYMPFX_EXTRABITS | (len - l->min) |
		     (l->extrabits << SYM_EXTRABITS_SHIFT)));
    }

    /*
     * Binary-search to find which distance code we're
     * transmitting.
     */
    i = -1;
    j = sizeof(distcodes) / sizeof(*distcodes);
    while (1) {
	assert(j - i >= 2);
	k = (j + i) / 2;
	if (distance < distcodes[k].min)
	    j = k;
	else if (distance > distcodes[k].max)
	    i = k;
	else {
	    d = &distcodes[k];
	    break;		       /* found it! */
	}
    }

    /*
     * Write the distance code.
     */
    outsym(out, SYMPFX_DIST | d->code);

    /*
     * Transmit the extra bits.
     */
    if (d->extrabits) {
	outsym(out, (SYMPFX_EXTRABITS | (distance - d->min) |
		     (d->extrabits << SYM_EXTRABITS_SHIFT)));
    }
}

deflate_compress_ctx *deflate_compress_new(int type)
{
    deflate_compress_ctx *out;

    out = snew(deflate_compress_ctx);
    out->type = type;
    out->outbits = out->noutbits = 0;
    out->firstblock = TRUE;
#ifdef STATISTICS
    out->bitcount = 0;
#endif

    out->syms = snewn(SYMLIMIT, unsigned long);
    out->symstart = out->nsyms = 0;

    out->adler32 = 1;
    out->lastblock = FALSE;
    out->finished = FALSE;

    out->lz = lz77_new(literal, match, out);

    return out;
}

void deflate_compress_free(deflate_compress_ctx *out)
{
    sfree(out->syms);
    lz77_free(out->lz);
    sfree(out);
}

static unsigned long adler32_update(unsigned long s,
				    const unsigned char *data, int len)
{
    unsigned s1 = s & 0xFFFF, s2 = (s >> 16) & 0xFFFF;
    int i;

    for (i = 0; i < len; i++) {
	s1 += data[i];
	s2 += s1;
	if (!(i & 0xFFF)) {
	    s1 %= 65521;
	    s2 %= 65521;
	}
    }

    return ((s2 % 65521) << 16) | (s1 % 65521);
}

void deflate_compress_data(deflate_compress_ctx *out,
			   const void *vblock, int len, int flushtype,
			   void **outblock, int *outlen)
{
    const unsigned char *block = (const unsigned char *)vblock;

    assert(!out->finished);

    out->outbuf = NULL;
    out->outlen = out->outsize = 0;

    /*
     * If this is the first block, output the header.
     */
    if (out->firstblock) {
	switch (out->type) {
	  case DEFLATE_TYPE_BARE:
	    break;		       /* no header */
	  case DEFLATE_TYPE_ZLIB:
	    /*
	     * Zlib (RFC1950) header bytes: 78 9C. (Deflate
	     * compression, 32K window size, default algorithm.)
	     */
	    outbits(out, 0x9C78, 16);
	    break;
	}
	out->firstblock = FALSE;
    }

    /*
     * Feed our data to the LZ77 compression phase.
     */
    lz77_compress(out->lz, block, len);

    /*
     * Update checksums.
     */
    if (out->type == DEFLATE_TYPE_ZLIB)
	out->adler32 = adler32_update(out->adler32, block, len);

    switch (flushtype) {
	/*
	 * FIXME: what other flush types are available and useful?
	 * In PuTTY, it was clear that we generally wanted to be in
	 * a static block so it was safe to open one. Here, we
	 * probably prefer to be _outside_ a block if we can. Think
	 * about this.
	 */
      case DEFLATE_NO_FLUSH:
	break;			       /* don't flush any data at all (duh) */
      case DEFLATE_SYNC_FLUSH:
	/*
	 * Flush the LZ77 compressor.
	 */
	lz77_flush(out->lz);

	/*
	 * Close the current block.
	 */
	flushblock(out);

	/*
	 * Then output an empty _uncompressed_ block: send 000,
	 * then sync to byte boundary, then send bytes 00 00 FF
	 * FF.
	 */
	outbits(out, 0, 3);
	if (out->noutbits)
	    outbits(out, 0, 8 - out->noutbits);
	outbits(out, 0, 16);
	outbits(out, 0xFFFF, 16);
	break;
      case DEFLATE_END_OF_DATA:
	/*
	 * Flush the LZ77 compressor.
	 */
	lz77_flush(out->lz);

	/*
	 * Output a block with BFINAL set.
	 */
	out->lastblock = TRUE;
	flushblock(out);

	/*
	 * Sync to byte boundary, flushing out the final byte.
	 */
	if (out->noutbits)
	    outbits(out, 0, 8 - out->noutbits);

	/*
	 * Output the adler32 checksum, in zlib mode.
	 */
	if (out->type == DEFLATE_TYPE_ZLIB) {
	    outbits(out, (out->adler32 >> 24) & 0xFF, 8);
	    outbits(out, (out->adler32 >> 16) & 0xFF, 8);
	    outbits(out, (out->adler32 >>  8) & 0xFF, 8);
	    outbits(out, (out->adler32 >>  0) & 0xFF, 8);
	}

	out->finished = TRUE;
	break;
    }

    /*
     * Return any data that we've generated.
     */
    *outblock = (void *)out->outbuf;
    *outlen = out->outlen;
}

/* ----------------------------------------------------------------------
 * Deflate decompression.
 */

/*
 * The way we work the Huffman decode is to have a table lookup on
 * the first N bits of the input stream (in the order they arrive,
 * of course, i.e. the first bit of the Huffman code is in bit 0).
 * Each table entry lists the number of bits to consume, plus
 * either an output code or a pointer to a secondary table.
 */
struct table;
struct tableentry;

struct tableentry {
    unsigned char nbits;
    short code;
    struct table *nexttable;
};

struct table {
    int mask;			       /* mask applied to input bit stream */
    struct tableentry *table;
};

#define MAXSYMS 288

#define DWINSIZE 32768

/*
 * Build a single-level decode table for elements
 * [minlength,maxlength) of the provided code/length tables, and
 * recurse to build subtables.
 */
static struct table *mkonetab(int *codes, unsigned char *lengths, int nsyms,
			      int pfx, int pfxbits, int bits)
{
    struct table *tab = snew(struct table);
    int pfxmask = (1 << pfxbits) - 1;
    int nbits, i, j, code;

    tab->table = snewn(1 << bits, struct tableentry);
    tab->mask = (1 << bits) - 1;

    for (code = 0; code <= tab->mask; code++) {
	tab->table[code].code = -1;
	tab->table[code].nbits = 0;
	tab->table[code].nexttable = NULL;
    }

    for (i = 0; i < nsyms; i++) {
	if (lengths[i] <= pfxbits || (codes[i] & pfxmask) != pfx)
	    continue;
	code = (codes[i] >> pfxbits) & tab->mask;
	for (j = code; j <= tab->mask; j += 1 << (lengths[i] - pfxbits)) {
	    tab->table[j].code = i;
	    nbits = lengths[i] - pfxbits;
	    if (tab->table[j].nbits < nbits)
		tab->table[j].nbits = nbits;
	}
    }
    for (code = 0; code <= tab->mask; code++) {
	if (tab->table[code].nbits <= bits)
	    continue;
	/* Generate a subtable. */
	tab->table[code].code = -1;
	nbits = tab->table[code].nbits - bits;
	if (nbits > 7)
	    nbits = 7;
	tab->table[code].nbits = bits;
	tab->table[code].nexttable = mkonetab(codes, lengths, nsyms,
					      pfx | (code << pfxbits),
					      pfxbits + bits, nbits);
    }

    return tab;
}

/*
 * Build a decode table, given a set of Huffman tree lengths.
 */
static struct table *mktable(unsigned char *lengths, int nlengths)
{
    int codes[MAXSYMS];
    int maxlen;

    maxlen = hufcodes(lengths, codes, nlengths);

    /*
     * Now we have the complete list of Huffman codes. Build a
     * table.
     */
    return mkonetab(codes, lengths, nlengths, 0, 0, maxlen < 9 ? maxlen : 9);
}

static int freetable(struct table **ztab)
{
    struct table *tab;
    int code;

    if (ztab == NULL)
	return -1;

    if (*ztab == NULL)
	return 0;

    tab = *ztab;

    for (code = 0; code <= tab->mask; code++)
	if (tab->table[code].nexttable != NULL)
	    freetable(&tab->table[code].nexttable);

    sfree(tab->table);
    tab->table = NULL;

    sfree(tab);
    *ztab = NULL;

    return (0);
}

struct deflate_decompress_ctx {
    struct table *staticlentable, *staticdisttable;
    struct table *currlentable, *currdisttable, *lenlentable;
    enum {
	START, OUTSIDEBLK,
	TREES_HDR, TREES_LENLEN, TREES_LEN, TREES_LENREP,
	INBLK, GOTLENSYM, GOTLEN, GOTDISTSYM,
	UNCOMP_LEN, UNCOMP_NLEN, UNCOMP_DATA,
	END, ADLER1, ADLER2, FINALSPIN
    } state;
    int sym, hlit, hdist, hclen, lenptr, lenextrabits, lenaddon, len,
	lenrep, lastblock;
    int uncomplen;
    unsigned char lenlen[19];
    unsigned char lengths[286 + 32];
    unsigned long bits;
    int nbits;
    unsigned char window[DWINSIZE];
    int winpos;
    unsigned char *outblk;
    int outlen, outsize;
    int type;
    unsigned long adler32;
#ifdef ANALYSIS
    int bytesout;
    int bytesread;
    int bitcount_before;
#define BITCOUNT(dctx) ( (dctx)->bytesread * 8 - (dctx)->nbits )
#endif
};

deflate_decompress_ctx *deflate_decompress_new(int type)
{
    deflate_decompress_ctx *dctx = snew(deflate_decompress_ctx);
    unsigned char lengths[288];

    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 256 - 144);
    memset(lengths + 256, 7, 280 - 256);
    memset(lengths + 280, 8, 288 - 280);
    dctx->staticlentable = mktable(lengths, 288);
    memset(lengths, 5, 32);
    dctx->staticdisttable = mktable(lengths, 32);
    if (type == DEFLATE_TYPE_BARE)
	dctx->state = OUTSIDEBLK;
    else
	dctx->state = START;
    dctx->currlentable = dctx->currdisttable = dctx->lenlentable = NULL;
    dctx->bits = 0;
    dctx->nbits = 0;
    dctx->winpos = 0;
    dctx->type = type;
    dctx->lastblock = FALSE;
    dctx->adler32 = 1;
#ifdef ANALYSIS
    dctx->bytesout = 0;
    dctx->bytesread = dctx->bitcount_before = 0;
#endif

    return dctx;
}

void deflate_decompress_free(deflate_decompress_ctx *dctx)
{
    if (dctx->currlentable && dctx->currlentable != dctx->staticlentable)
	freetable(&dctx->currlentable);
    if (dctx->currdisttable && dctx->currdisttable != dctx->staticdisttable)
	freetable(&dctx->currdisttable);
    if (dctx->lenlentable)
	freetable(&dctx->lenlentable);
    freetable(&dctx->staticlentable);
    freetable(&dctx->staticdisttable);
    sfree(dctx);
}

static int huflookup(unsigned long *bitsp, int *nbitsp, struct table *tab)
{
    unsigned long bits = *bitsp;
    int nbits = *nbitsp;
    while (1) {
	struct tableentry *ent;
	ent = &tab->table[bits & tab->mask];
	if (ent->nbits > nbits)
	    return -1;		       /* not enough data */
	bits >>= ent->nbits;
	nbits -= ent->nbits;
	if (ent->code == -1)
	    tab = ent->nexttable;
	else {
	    *bitsp = bits;
	    *nbitsp = nbits;
	    return ent->code;
	}

	if (!tab) {
	    /*
	     * There was a missing entry in the table, presumably
	     * due to an invalid Huffman table description, and the
	     * subsequent data has attempted to use the missing
	     * entry. Return a decoding failure.
	     */
	    return -2;
	}
    }
}

static void emit_char(deflate_decompress_ctx *dctx, int c)
{
    dctx->window[dctx->winpos] = c;
    dctx->winpos = (dctx->winpos + 1) & (DWINSIZE - 1);
    if (dctx->outlen >= dctx->outsize) {
	dctx->outsize = dctx->outlen * 3 / 2 + 512;
	dctx->outblk = sresize(dctx->outblk, dctx->outsize, unsigned char);
    }
    if (dctx->type == DEFLATE_TYPE_ZLIB) {
	unsigned char uc = c;
	dctx->adler32 = adler32_update(dctx->adler32, &uc, 1);
    }
    dctx->outblk[dctx->outlen++] = c;
#ifdef ANALYSIS
    dctx->bytesout++;
#endif
}

#define EATBITS(n) ( dctx->nbits -= (n), dctx->bits >>= (n) )

int deflate_decompress_data(deflate_decompress_ctx *dctx,
			    const void *vblock, int len,
			    void **outblock, int *outlen)
{
    const coderecord *rec;
    const unsigned char *block = (const unsigned char *)vblock;
    int code, bfinal, btype, rep, dist, nlen, header, adler;
    int error = 0;

    if (len == 0) {
	*outblock = NULL;
	*outlen = 0;
	if (dctx->state != FINALSPIN)
	    return DEFLATE_ERR_UNEXPECTED_EOF;
	else
	    return 0;
    }

    dctx->outblk = NULL;
    dctx->outsize = 0;
    dctx->outlen = 0;

    while (len > 0 || dctx->nbits > 0) {
	while (dctx->nbits < 24 && len > 0) {
	    dctx->bits |= (*block++) << dctx->nbits;
	    dctx->nbits += 8;
	    len--;
#ifdef ANALYSIS
	    dctx->bytesread++;
#endif
	}
	switch (dctx->state) {
	  case START:
	    /* Expect 16-bit zlib header. */
	    if (dctx->nbits < 16)
		goto finished;	       /* done all we can */

            /*
             * The header is stored as a big-endian 16-bit integer,
             * in contrast to the general little-endian policy in
             * the rest of the format :-(
             */
            header = (((dctx->bits & 0xFF00) >> 8) |
                      ((dctx->bits & 0x00FF) << 8));
            EATBITS(16);

            /*
             * Check the header:
             *
             *  - bits 8-11 should be 1000 (Deflate/RFC1951)
             *  - bits 12-15 should be at most 0111 (window size)
             *  - bit 5 should be zero (no dictionary present)
             *  - we don't care about bits 6-7 (compression rate)
             *  - bits 0-4 should be set up to make the whole thing
             *    a multiple of 31 (checksum).
             */
            if ((header & 0x0F00) != 0x0800 ||
                (header & 0xF000) >  0x7000 ||
                (header & 0x0020) != 0x0000 ||
                (header % 31) != 0) {
		error = DEFLATE_ERR_ZLIB_HEADER;
                goto finished;
	    }

	    dctx->state = OUTSIDEBLK;
	    break;
	  case OUTSIDEBLK:
	    /* Expect 3-bit block header. */
	    if (dctx->nbits < 3)
		goto finished;	       /* done all we can */
	    bfinal = dctx->bits & 1;
	    if (bfinal)
		dctx->lastblock = TRUE;
	    EATBITS(1);
	    btype = dctx->bits & 3;
	    EATBITS(2);
	    if (btype == 0) {
		int to_eat = dctx->nbits & 7;
		dctx->state = UNCOMP_LEN;
		EATBITS(to_eat);       /* align to byte boundary */
	    } else if (btype == 1) {
		dctx->currlentable = dctx->staticlentable;
		dctx->currdisttable = dctx->staticdisttable;
		dctx->state = INBLK;
	    } else if (btype == 2) {
		dctx->state = TREES_HDR;
	    }
	    debug(("recv: bfinal=%d btype=%d\n", bfinal, btype));
	    break;
	  case TREES_HDR:
	    /*
	     * Dynamic block header. Five bits of HLIT, five of
	     * HDIST, four of HCLEN.
	     */
	    if (dctx->nbits < 5 + 5 + 4)
		goto finished;	       /* done all we can */
	    dctx->hlit = 257 + (dctx->bits & 31);
	    EATBITS(5);
	    dctx->hdist = 1 + (dctx->bits & 31);
	    EATBITS(5);
	    dctx->hclen = 4 + (dctx->bits & 15);
	    EATBITS(4);
	    debug(("recv: hlit=%d hdist=%d hclen=%d\n", dctx->hlit,
		   dctx->hdist, dctx->hclen));
	    dctx->lenptr = 0;
	    dctx->state = TREES_LENLEN;
	    memset(dctx->lenlen, 0, sizeof(dctx->lenlen));
	    break;
	  case TREES_LENLEN:
	    if (dctx->nbits < 3)
		goto finished;
	    while (dctx->lenptr < dctx->hclen && dctx->nbits >= 3) {
		dctx->lenlen[lenlenmap[dctx->lenptr++]] =
		    (unsigned char) (dctx->bits & 7);
		debug(("recv: lenlen %d\n", (unsigned char) (dctx->bits & 7)));
		EATBITS(3);
	    }
	    if (dctx->lenptr == dctx->hclen) {
		dctx->lenlentable = mktable(dctx->lenlen, 19);
		dctx->state = TREES_LEN;
		dctx->lenptr = 0;
	    }
	    break;
	  case TREES_LEN:
	    if (dctx->lenptr >= dctx->hlit + dctx->hdist) {
		dctx->currlentable = mktable(dctx->lengths, dctx->hlit);
		dctx->currdisttable = mktable(dctx->lengths + dctx->hlit,
					      dctx->hdist);
		freetable(&dctx->lenlentable);
		dctx->lenlentable = NULL;
		dctx->state = INBLK;
		break;
	    }
	    code = huflookup(&dctx->bits, &dctx->nbits, dctx->lenlentable);
	    debug(("recv: codelen %d\n", code));
	    if (code == -1)
		goto finished;
	    if (code == -2) {
		error = DEFLATE_ERR_INVALID_HUFFMAN;
		goto finished;
	    }
	    if (code < 16)
		dctx->lengths[dctx->lenptr++] = code;
	    else {
		dctx->lenextrabits = (code == 16 ? 2 : code == 17 ? 3 : 7);
		dctx->lenaddon = (code == 18 ? 11 : 3);
		dctx->lenrep = (code == 16 && dctx->lenptr > 0 ?
				dctx->lengths[dctx->lenptr - 1] : 0);
		dctx->state = TREES_LENREP;
	    }
	    break;
	  case TREES_LENREP:
	    if (dctx->nbits < dctx->lenextrabits)
		goto finished;
	    rep =
		dctx->lenaddon +
		(dctx->bits & ((1 << dctx->lenextrabits) - 1));
	    EATBITS(dctx->lenextrabits);
	    if (dctx->lenextrabits)
		debug(("recv: codelen-extrabits %d/%d\n", rep - dctx->lenaddon,
		       dctx->lenextrabits));
	    while (rep > 0 && dctx->lenptr < dctx->hlit + dctx->hdist) {
		dctx->lengths[dctx->lenptr] = dctx->lenrep;
		dctx->lenptr++;
		rep--;
	    }
	    dctx->state = TREES_LEN;
	    break;
	  case INBLK:
#ifdef ANALYSIS
	    dctx->bitcount_before = BITCOUNT(dctx);
#endif
	    code = huflookup(&dctx->bits, &dctx->nbits, dctx->currlentable);
	    debug(("recv: litlen %d\n", code));
	    if (code == -1)
		goto finished;
	    if (code == -2) {
		error = DEFLATE_ERR_INVALID_HUFFMAN;
		goto finished;
	    }
	    if (code < 256) {
#ifdef ANALYSIS
		if (analyse)
		    printf("%d: literal %d [%d]\n", dctx->bytesout, code,
			   BITCOUNT(dctx) - dctx->bitcount_before);
#endif
		emit_char(dctx, code);
	    } else if (code == 256) {
		if (dctx->lastblock)
		    dctx->state = END;
		else
		    dctx->state = OUTSIDEBLK;
		if (dctx->currlentable != dctx->staticlentable) {
		    freetable(&dctx->currlentable);
		    dctx->currlentable = NULL;
		}
		if (dctx->currdisttable != dctx->staticdisttable) {
		    freetable(&dctx->currdisttable);
		    dctx->currdisttable = NULL;
		}
	    } else if (code < 286) {   /* static tree can give >285; ignore */
		dctx->state = GOTLENSYM;
		dctx->sym = code;
	    }
	    break;
	  case GOTLENSYM:
	    rec = &lencodes[dctx->sym - 257];
	    if (dctx->nbits < rec->extrabits)
		goto finished;
	    dctx->len =
		rec->min + (dctx->bits & ((1 << rec->extrabits) - 1));
	    if (rec->extrabits)
		debug(("recv: litlen-extrabits %d/%d\n",
		       dctx->len - rec->min, rec->extrabits));
	    EATBITS(rec->extrabits);
	    dctx->state = GOTLEN;
	    break;
	  case GOTLEN:
	    code = huflookup(&dctx->bits, &dctx->nbits, dctx->currdisttable);
	    debug(("recv: dist %d\n", code));
	    if (code == -1)
		goto finished;
	    if (code == -2) {
		error = DEFLATE_ERR_INVALID_HUFFMAN;
		goto finished;
	    }
	    dctx->state = GOTDISTSYM;
	    dctx->sym = code;
	    break;
	  case GOTDISTSYM:
	    rec = &distcodes[dctx->sym];
	    if (dctx->nbits < rec->extrabits)
		goto finished;
	    dist = rec->min + (dctx->bits & ((1 << rec->extrabits) - 1));
	    if (rec->extrabits)
		debug(("recv: dist-extrabits %d/%d\n",
		       dist - rec->min, rec->extrabits));
	    EATBITS(rec->extrabits);
	    dctx->state = INBLK;
#ifdef ANALYSIS
	    if (analyse)
		printf("%d: copy len=%d dist=%d [%d]\n", dctx->bytesout,
		       dctx->len, dist,
		       BITCOUNT(dctx) - dctx->bitcount_before);
#endif
	    while (dctx->len--)
		emit_char(dctx, dctx->window[(dctx->winpos - dist) &
					     (DWINSIZE - 1)]);
	    break;
	  case UNCOMP_LEN:
	    /*
	     * Uncompressed block. We expect to see a 16-bit LEN.
	     */
	    if (dctx->nbits < 16)
		goto finished;
	    dctx->uncomplen = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    dctx->state = UNCOMP_NLEN;
	    break;
	  case UNCOMP_NLEN:
	    /*
	     * Uncompressed block. We expect to see a 16-bit NLEN,
	     * which should be the one's complement of the previous
	     * LEN.
	     */
	    if (dctx->nbits < 16)
		goto finished;
	    nlen = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (dctx->uncomplen == 0)
		dctx->state = OUTSIDEBLK;	/* block is empty */
	    else
		dctx->state = UNCOMP_DATA;
	    break;
	  case UNCOMP_DATA:
	    if (dctx->nbits < 8)
		goto finished;
	    emit_char(dctx, dctx->bits & 0xFF);
	    EATBITS(8);
	    if (--dctx->uncomplen == 0)
		dctx->state = OUTSIDEBLK;	/* end of uncompressed block */
	    break;
	  case END:
	    /*
	     * End of compressed data. We align to a byte boundary,
	     * and then look for format-specific trailer data.
	     */
	    {
		int to_eat = dctx->nbits & 7;
		EATBITS(to_eat);
	    }
	    if (dctx->type == DEFLATE_TYPE_ZLIB)
		dctx->state = ADLER1;
	    else
		dctx->state = FINALSPIN;
	    break;
	  case ADLER1:
	    if (dctx->nbits < 16)
		goto finished;
	    adler = (dctx->bits & 0xFF) << 8;
	    EATBITS(8);
	    adler |= (dctx->bits & 0xFF);
	    EATBITS(8);
	    if (adler != ((dctx->adler32 >> 16) & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = ADLER2;
	    break;
	  case ADLER2:
	    if (dctx->nbits < 16)
		goto finished;
	    adler = (dctx->bits & 0xFF) << 8;
	    EATBITS(8);
	    adler |= (dctx->bits & 0xFF);
	    EATBITS(8);
	    if (adler != (dctx->adler32 & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = FINALSPIN;
	    break;
	  case FINALSPIN:
	    /* Just ignore any trailing garbage on the data stream. */
	    EATBITS(dctx->nbits);
	    break;
	}
    }

    finished:
    *outblock = dctx->outblk;
    *outlen = dctx->outlen;
    return error;
}

#define A(code,str) str
const char *const deflate_error_msg[DEFLATE_NUM_ERRORS] = {
    DEFLATE_ERRORLIST(A)
};
#undef A

#define A(code,str) #code
const char *const deflate_error_sym[DEFLATE_NUM_ERRORS] = {
    DEFLATE_ERRORLIST(A)
};
#undef A

#ifdef STANDALONE

int main(int argc, char **argv)
{
    unsigned char buf[65536], *outbuf;
    int ret, err, outlen;
    deflate_decompress_ctx *dhandle;
    deflate_compress_ctx *chandle;
    int type = DEFLATE_TYPE_ZLIB, opts = TRUE;
    int compress = FALSE, decompress = FALSE;
    int got_arg = FALSE;
    char *filename = NULL;
    FILE *fp;

    while (--argc) {
        char *p = *++argv;

	got_arg = TRUE;

        if (p[0] == '-' && opts) {
            if (!strcmp(p, "-b"))
                type = DEFLATE_TYPE_BARE;
            else if (!strcmp(p, "-c"))
                compress = TRUE;
            else if (!strcmp(p, "-d"))
                decompress = TRUE;
            else if (!strcmp(p, "-a"))
                analyse = decompress = TRUE;
            else if (!strcmp(p, "--"))
                opts = FALSE;          /* next thing is filename */
            else {
                fprintf(stderr, "unknown command line option '%s'\n", p);
                return 1;
            }
        } else if (!filename) {
            filename = p;
        } else {
            fprintf(stderr, "can only handle one filename\n");
            return 1;
        }
    }

    if (!compress && !decompress) {
	fprintf(stderr, "usage: deflate [ -c | -d | -a ] [ -b ] [filename]\n");
	return (got_arg ? 1 : 0);
    }

    if (compress && decompress) {
	fprintf(stderr, "please do not specify both compression and"
		" decompression\n");
	return (got_arg ? 1 : 0);
    }

    if (compress) {
	chandle = deflate_compress_new(type);
	dhandle = NULL;
    } else {
	dhandle = deflate_decompress_new(type);
	chandle = NULL;
    }

    if (filename)
        fp = fopen(filename, "rb");
    else
        fp = stdin;

    if (!fp) {
        assert(filename);
        fprintf(stderr, "unable to open '%s'\n", filename);
        return 1;
    }

    do {
	ret = fread(buf, 1, sizeof(buf), fp);
	outbuf = NULL;
	if (dhandle) {
	    if (ret > 0)
		err = deflate_decompress_data(dhandle, buf, ret,
					      (void **)&outbuf, &outlen);
	    else
		err = deflate_decompress_data(dhandle, NULL, 0,
					      (void **)&outbuf, &outlen);
	} else {
	    if (ret > 0)
		deflate_compress_data(chandle, buf, ret, DEFLATE_NO_FLUSH,
				      (void **)&outbuf, &outlen);
	    else
		deflate_compress_data(chandle, buf, ret, DEFLATE_END_OF_DATA,
				      (void **)&outbuf, &outlen);
	    err = 0;
	}
        if (outbuf) {
            if (!analyse && outlen)
                fwrite(outbuf, 1, outlen, stdout);
            sfree(outbuf);
        }
	if (err > 0) {
            fprintf(stderr, "decoding error: %s\n", deflate_error_msg[err]);
            return 1;
        }
    } while (ret > 0);

    if (dhandle)
	deflate_decompress_free(dhandle);
    if (chandle)
	deflate_compress_free(chandle);

    if (filename)
        fclose(fp);

    return 0;
}

#endif

#ifdef TESTMODE

int main(int argc, char **argv)
{
    char *filename = NULL;
    FILE *fp;
    deflate_compress_ctx *chandle;
    deflate_decompress_ctx *dhandle;
    unsigned char buf[65536], *outbuf, *outbuf2;
    int ret, err, outlen, outlen2;
    int dlen = 0, clen = 0;
    int opts = TRUE;

    while (--argc) {
        char *p = *++argv;

        if (p[0] == '-' && opts) {
            if (!strcmp(p, "--"))
                opts = FALSE;          /* next thing is filename */
            else {
                fprintf(stderr, "unknown command line option '%s'\n", p);
                return 1;
            }
        } else if (!filename) {
            filename = p;
        } else {
            fprintf(stderr, "can only handle one filename\n");
            return 1;
        }
    }

    if (filename)
        fp = fopen(filename, "rb");
    else
        fp = stdin;

    if (!fp) {
        assert(filename);
        fprintf(stderr, "unable to open '%s'\n", filename);
        return 1;
    }

    chandle = deflate_compress_new(DEFLATE_TYPE_ZLIB);
    dhandle = deflate_decompress_new(DEFLATE_TYPE_ZLIB);

    do {
	ret = fread(buf, 1, sizeof(buf), fp);
	if (ret <= 0) {
	    deflate_compress_data(chandle, NULL, 0, DEFLATE_END_OF_DATA,
				  (void **)&outbuf, &outlen);
	} else {
	    dlen += ret;
	    deflate_compress_data(chandle, buf, ret, DEFLATE_NO_FLUSH,
				  (void **)&outbuf, &outlen);
	}
	if (outbuf) {
	    clen += outlen;
	    err = deflate_decompress_data(dhandle, outbuf, outlen,
					  (void **)&outbuf2, &outlen2);
	    sfree(outbuf);
	    if (outbuf2) {
		if (outlen2)
		    fwrite(outbuf2, 1, outlen2, stdout);
		sfree(outbuf2);
	    }
	    if (!err && ret <= 0) {
		/*
		 * signal EOF
		 */
		err = deflate_decompress_data(dhandle, NULL, 0,
					      (void **)&outbuf2, &outlen2);
		assert(outbuf2 == NULL);
	    }
	    if (err) {
		fprintf(stderr, "decoding error: %s\n",
			deflate_error_msg[err]);
		return 1;
	    }
	}
    } while (ret > 0);

    fprintf(stderr, "%d plaintext -> %d compressed\n", dlen, clen);

    return 0;
}

#endif
