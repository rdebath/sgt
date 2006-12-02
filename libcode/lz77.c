/*
 * LZ77 compressor. In the probably vain hope that this will be the
 * _last time_ I have to write one.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lz77.h"

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

LZ77 *lz77_new(void (*literal)(void *ctx, unsigned char c),
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

void lz77_free(LZ77 *lz)
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

void lz77_flush(LZ77 *lz)
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

void lz77_compress(LZ77 *lz, const void *vdata, int len)
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

#ifdef LZ77_TESTMODE

#include <stdio.h>

/*
 * These tests are mostly constructed to check specific
 * match-finding situations. Strings of capital letters are
 * arranged so that they never repeat any sequence of three
 * characters, and hence they are incompressible by this algorithm.
 */

const char *const tests[] = {
    /*
     * Basics: an incompressible string to make sure we don't
     * compress it by mistake.
     */
    "AAABAACAADAAEAAFAAGAAHAAIAAJAAKAALAAMAANAAOAAPAAQAARAASAATAAUAAVAAWAAXAA",

    /*
     * Simple repeated string. Repeating the string three times
     * rather than two also checks that we prefer to match against
     * more recent data when there's no length difference.
     */
    "ZAabcBBABCABabcDABEABFabcABGABHA",
    "ZAabcdBBABCABabcdDABEABFabcdABGABHA",
    "ZAabcdeBBABCABabcdeDABEABFabcdeABGABHA",

    /*
     * Self-overlapping match.
     */
    "RABSABTAspoonspoonspoonspoonspoonspoonspoonspoonBUABVAB",

    /*
     * Lazy matching, one step on. `flapping' should be rendered as
     * a literal `f' plus a match against `lapping', rather than as
     * a match against `flap' followed by one against `ping'.
     */
    "ACCACDACflapEACFACGlappingACHACIAflappingCJACK",

    /*
     * Lazy matching, two steps on. (Transmitting `fl' as literals
     * is still superior to transmitting two matches.) Then
     * gradually reduce the length of the later match until it's no
     * longer profitable to use it.
     */
    "ACCACDACflapEACFACGapplianceACHACIAflapplianceCJACK",
    "ACCACDACflapEACFACGappliancACHACIAflapplianceCJACK",
    "ACCACDACflapEACFACGapplianACHACIAflapplianceCJACK",
    "ACCACDACflapEACFACGappliaACHACIAflapplianceCJACK",
    "ACCACDACflapEACFACGappliACHACIAflapplianceCJACK",
    "ACCACDACflapEACFACGapplACHACIAflapplianceCJACK",

    /*
     * Non-lazy matching, three steps on. (Transmitting the `fla'
     * as literals is _not_ superior to transmitting it as a match;
     * and in fact it's best to transmit the longest initial match
     * we can - `flap' - and then cover `athetic' with the next
     * match.)
     */
    "ACCACDACflapEACFACGpatheticACHACIAflapatheticCJACK",

    /*
     * Test that various kinds of match correctly find an
     * immediately following match in all circumstances.
     */
    "WAabcdeCXfghijACYACabcdefghijZAD",
    "WAabcdeCXbcdefACYACfghijZADabcdefghijBADCA",
    "WAabcdeCXbcdefgACYACfghijZADabcdefghijBADCA",
    "0WAabcdeCXbcdefACcdefghYACfghijklmZADabcdefghijklmBADCA",
    "2WAabcdeCXbcdefACcdefghiYACfghijklmZADabcdefghijklmBADCA",
    "1WAabcdeCXbcdefgACcdefghYACfghijklmZADabcdefghijklmBADCA",
    "0WAabcdefCXbcdefACcdefgYACfghijklmZADabcdefghijklmBADCA",
    "0WAabcdefCXbcdefgACcdefgYACfghijklmZADabcdefghijklmBADCA",
    "0WAabcdefCXbcdefACcdefghYACfghijklmZADabcdefghijklmBADCA",
    "1WAabcdeCXbcdefgACcdefghYACfghijklmZADabcdefghijklmBADCA",
    "0WAabcdefCXbcdefgACcdefghYACfghijklmZADabcdefghijklmBADCA",
    "WAabcdeCXbcdefgACcdefghiYACfghijZADabcdefghijBADCA",

#if 0
    /*
     * Lazy matching: nasty cases in which it can be marginally
     * better _not_ to lazily match. In at least some of these
     * cases, choosing the superficially longer lazy match
     * eliminates an opportunity to render the entire final
     * lower-case section using one more match and at least three
     * fewer literals.
     * 
     * (The compressor as currently written does not in fact
     * perform optimally in this situation.)
     */
    "WAabcdeCXbcdefghijklACYACfghijklmnoZADabcdefghijklmnoBADCA",
    "WAabcdeCXbcdefghijklACYACghijklmnoZADabcdefghijklmnoBADCA",
    "WAabcdeCXbcdefghijklACYACfghijklmnZADabcdefghijklmnBADCA",
    "WAabcdeCXbcdefghijklACYACghijklmnZADabcdefghijklmnBADCA",
    "WAabcdeCXbcdefghijklACYACfghijklmZADabcdefghijklmBADCA",
    "WAabcdeCXbcdefghijklACYACghijklmZADabcdefghijklmBADCA",
    "WAabcdCXbcdefACcdefghijkYACghijklmnZADabcdefghijklmnBADCA",
    "WAabcdCXbcdefACcdefghijkYACfghijklmnZADabcdefghijklmnBADCA",
    "WAabcdCXbcdefACcdefghijkYACefghijklmnZADabcdefghijklmnBADCA",
    "WAabcdCXbcdefACcdefghijkYACghijklmZADabcdefghijklmBADCA",
    "WAabcdCXbcdefACcdefghijkYACfghijklmZADabcdefghijklmBADCA",
    "WAabcdCXbcdefACcdefghijkYACefghijklmZADabcdefghijklmBADCA",
    "WAabcdCXbcdefACcdefghijkYACghijklmZADabcdefghijklBADCA",
    "WAabcdCXbcdefACcdefghijkYACfghijklmZADabcdefghijklBADCA",
    "WAabcdCXbcdefACcdefghijkYACefghijklmZADabcdefghijklBADCA",
#endif

    /*
     * Fun final test.
     */
    "Pease porridge hot, pease porridge cold, pease porridge"
	" in the pot, nine days old.",
};

struct testctx {
    const char *data;
    int len, ptr;
};

void match(void *vctx, int distance, int len) {
    struct testctx *ctx = (struct testctx *)vctx;

    assert(distance > 0);
    assert(distance <= ctx->ptr);
    assert(len >= HASHCHARS);
    assert(len <= ctx->len - ctx->ptr);
    assert(len <= MAXMATCHLEN);
    assert(!memcmp(ctx->data + ctx->ptr, ctx->data + ctx->ptr - distance,
		   len));

    printf("<%d,%d>", distance, len);
    fflush(stdout);

    ctx->ptr += len;
}

void literal(void *vctx, unsigned char c) {
    struct testctx *ctx = (struct testctx *)vctx;

    assert(ctx->ptr < ctx->len);
    assert(c == (unsigned char)(ctx->data[ctx->ptr]));

    fputc(c, stdout);
    fflush(stdout);

    ctx->ptr++;
}

void dotest(const void *data, int len, int step)
{
    struct testctx t;
    LZ77 *lz;
    int j;

    t.data = data;
    t.len = len;
    t.ptr = 0;
    lz = lz77_new(literal, match, &t);
    for (j = 0; j < t.len; j += step)
	lz77_compress(lz, t.data + j, (t.len - j < step ? t.len - j : step));
    lz77_flush(lz);
    lz77_free(lz);
    assert(t.len == t.ptr);
    printf("\n");
}

#define lenof(x) (sizeof((x))/sizeof(*(x)))

int main(int argc, char **argv) {
    int i, len, truncate = 0;
    int step;
    char *filename = NULL;

    step = 48000;		       /* big step by default */

    while (--argc) {
	char *p = *++argv;
	if (!strcmp(p, "-t")) {
	    truncate = 1;
	} else if (p[0] == '-' && p[1] == 'b') {
	    step = atoi(p+2);	       /* -bN sets block size to N */
	} else if (p[0] != '-') {
	    filename = p;
	}
    }

    if (filename) {
	char *data = NULL;
	int datalen = 0, datasize = 0;
	int c;
	FILE *fp = fopen(filename, "rb");

	while ( (c = fgetc(fp)) != EOF) {
	    if (datalen >= datasize) {
		datasize = (datalen * 3 / 2) + 512;
		data = realloc(data, datasize);
	    }
	    data[datalen++] = c;
	}

	fclose(fp);
	dotest(data, datalen, step);
	
    } else {
	for (i = 0; i < lenof(tests); i++) {
	    for (len = (truncate ? 0 : strlen(tests[i]));
		 len <= strlen(tests[i]); len++) {
		dotest(tests[i], len, step);
	    }
	}
    }

    return 0;
}

#endif
