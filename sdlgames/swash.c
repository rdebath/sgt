/*
 * Implementation of a `swash' font (each character can extend out
 * of its cell in a variety of different ways depending on
 * context), using a dynamic programming algorithm to find the
 * global optimum set of extensions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef TESTMODE
#define debug(x) ( printf x )
#else
#define debug(x)
#endif

#define FONT_HEIGHT 9
#define EXTENSION_LEN 3
#define INTERNAL_EXTENSION_LEN 2
#define MAX_VX_OPTIONS 3
#define MAX_TEXT_LEN 110

struct chr {
    unsigned char bitpattern[FONT_HEIGHT];
    int width;
    int hc, hx;
    int vx[MAX_VX_OPTIONS];	       /* terminated by the zero entry */
};

struct punct {
    unsigned char bitpattern[FONT_HEIGHT+2*EXTENSION_LEN];
    int width;
    int hx_inhibit;
    int vx_inhibit;
    int left_overlap, right_overlap;
};

#define TR 0x01
#define BR 0x02
#define TM 0x04
#define BM 0x08
#define TL 0x10
#define BL 0x20

/* Map TL and BL bits to TR and BR respectively, and vice versa */
#define R_TO_L(x) ( ((x) & (TR|BR)) << 4 )
#define L_TO_R(x) ( ((x) & (TL|BL)) >> 4 )

#define LEFT(x) ( (x) & (TL|BL) )
#define RIGHT(x) ( (x) & (TR|BR) )

#define ALL4 (TL|TR|BL|BR)

static struct chr alphabet[] = {
    {{0x1e,0x3f,0x33,0x3f,0x3f,0x33,0x33,0x33,0x33},6,BL|BR,0,{BL,BR,0}},
    {{0x3e,0x3f,0x33,0x3e,0x3f,0x33,0x33,0x3f,0x3e},6,TL|BL,TL,{0}},
    {{0x1e,0x3f,0x30,0x30,0x30,0x30,0x30,0x3f,0x1e},6,TR|BR,TR,{0}},
    {{0x3e,0x3f,0x33,0x33,0x33,0x33,0x33,0x3f,0x3e},6,TL|BL,TL,{0}},
    {{0x3f,0x3f,0x30,0x3c,0x3c,0x30,0x30,0x3f,0x3f},6,ALL4,TR,{0}},
    {{0x3f,0x3f,0x30,0x3c,0x3c,0x30,0x30,0x30,0x30},6,TL|TR|BL,TR,{BL,0}},
    {{0x1e,0x3f,0x30,0x37,0x37,0x33,0x33,0x3f,0x1f},6,TR,TR,{0}},
    {{0x33,0x33,0x33,0x3f,0x3f,0x33,0x33,0x33,0x33},6,ALL4,0,{BL|TR,TL|BR,0}},
    {{0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03},2,0,0,{TL|TR,BL|BR,0}},
    {{0x3f,0x3f,0x06,0x06,0x06,0x06,0x06,0x3e,0x3c},6,TL|BL|TR,BL,{0}},
    {{0x33,0x33,0x36,0x3c,0x3e,0x33,0x33,0x33,0x33},6,ALL4,0,{BL|TR,TL|BR,0}},
    {{0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3f,0x3f},6,TL|BL|BR,BR,{TL,0}},
    {{0x7e,0xff,0xdb,0xdb,0xdb,0xc3,0xc3,0xc3,0xc3},8,BL|BR,0,{BL,BR,0}},
    {{0x63,0x73,0x7b,0x7f,0x6f,0x67,0x63,0x63,0x63},7,ALL4,0,{BL|TR,TL|BR,0}},
    {{0x1e,0x3f,0x33,0x33,0x33,0x33,0x33,0x3f,0x1e},6,0,0,{0}},
    {{0x3e,0x3f,0x33,0x3f,0x3e,0x30,0x30,0x30,0x30},6,TL|BL,TL,{BL,0}},
    {{0x1e,0x3f,0x33,0x33,0x33,0x33,0x36,0x3f,0x1b},6,0,0,{0}},
    {{0x3e,0x3f,0x33,0x3f,0x3e,0x33,0x33,0x33,0x33},6,TL|BL|BR,0,{BL,BR,0}},
    {{0x1e,0x3f,0x30,0x3e,0x1f,0x03,0x03,0x3f,0x1e},6,BL|TR,BL|TR,{0}},
    {{0x3f,0x3f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c},6,TL|TR,TL|TR,{BM,0}},
    {{0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x3f,0x1e},6,TL|TR,0,{TL,TR,0}},
    {{0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x1e,0x0c},6,TL|TR,0,{TL,TR,0}},
    {{0xc3,0xc3,0xc3,0xc3,0xdb,0xdb,0xdb,0xff,0x7e},8,TL|TR,0,{TL,TR,0}},
    {{0x33,0x33,0x33,0x1e,0x0c,0x1e,0x33,0x33,0x33},6,ALL4,0,{BL|TR,TL|BR,0}},
    {{0x63,0x63,0x63,0x3e,0x0c,0x0c,0x0c,0x0c,0x0c},7,TL|TR,0,{TL|BM,TR|BM,0}},
    {{0x3f,0x3f,0x03,0x06,0x0c,0x18,0x30,0x3f,0x3f},6,TL|BR,TL|BR,{0}},
};

static struct punct punctuation[] = {
    {{0,0,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},2,0,0},
    {{0,0,0,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x03,0x03,0,0,0},2,ALL4,0},
    {{0,0,0,0x1b,0x1b,0x0a,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},5,TL|TR,0},
    {{0,0,0,0x36,0x36,0x7f,0x7f,0x36,0x7f,0x7f,0x36,0x36,0,0,0},7,ALL4,0},
    {{4,12,12,0x1e,0x3f,0x30,0x3e,0x1f,0x03,0x03,0x3f,0x1e,12,12,8},6,ALL4,0},
    {{0,0,0,0x33,0x33,0x03,0x06,0x0c,0x18,0x30,0x33,0x33,0,0,0},6,ALL4,0},
    {{0,0,0,0x1c,0x36,0x36,0x1c,0x1b,0x37,0x36,0x3f,0x1b,0,0,0},6,ALL4,0},
    {{0,0,0,0x03,0x03,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},2,TL|TR,0},
    {{1,3,6,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,6,3,1},4,ALL4,TR|BR,0,2},
    {{8,12,6,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,6,12,8},4,ALL4,TL|BL,2,0},
    {{0,0,0,0x00,0x00,0x33,0x1e,0x3f,0x3f,0x1e,0x33,0x00,0,0,0},6,ALL4,0},
    {{0,0,0,0x00,0x00,0x0c,0x0c,0x3f,0x3f,0x0c,0x0c,0x00,0,0,0},6,ALL4,0},
    {{0,0,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,2,0,0},2,BL|BR,0},
    {{0,0,0,0x00,0x00,0x00,0x00,0x3f,0x3f,0x00,0x00,0x00,0,0,0},6,ALL4,0},
    {{0,0,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0,0,0},2,BL|BR,0},
    {{0,0,0,0x03,0x03,0x03,0x06,0x0c,0x18,0x30,0x30,0x30,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x33,0x37,0x3f,0x3b,0x33,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x0c,0x1c,0x1c,0x0c,0x0c,0x0c,0x0c,0x1e,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x33,0x03,0x1f,0x3e,0x30,0x3f,0x3f,0,0,0},6,ALL4,0},
    {{0,0,0,0x3f,0x3f,0x03,0x1e,0x1f,0x03,0x33,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x30,0x30,0x30,0x36,0x36,0x3f,0x3f,0x06,0x06,0,0,0},6,ALL4,0},
    {{0,0,0,0x3f,0x3f,0x30,0x3e,0x3f,0x03,0x33,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x30,0x3e,0x3f,0x33,0x33,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x3f,0x3f,0x07,0x0e,0x1c,0x18,0x18,0x18,0x18,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x33,0x3f,0x1e,0x33,0x33,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x33,0x33,0x3f,0x1f,0x03,0x3f,0x1e,0,0,0},6,ALL4,0},
    {{0,0,0,0x00,0x00,0x03,0x03,0x00,0x03,0x03,0x00,0x00,0,0,0},2,ALL4,0},
    {{0,0,0,0x00,0x00,0x03,0x03,0x00,0x03,0x03,0x02,0x00,0,0,0},2,ALL4,0},
    {{0,0,0,0x00,0x00,0x03,0x06,0x0c,0x0c,0x06,0x03,0x00,0,0,0},4,ALL4,0},
    {{0,0,0,0x00,0x00,0x3f,0x3f,0x00,0x3f,0x3f,0x00,0x00,0,0,0},6,ALL4,0},
    {{0,0,0,0x00,0x00,0x0c,0x06,0x03,0x03,0x06,0x0c,0x00,0,0,0},4,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x03,0x07,0x0c,0x0c,0x00,0x0c,0x0c,0,0,0},6,ALL4,0},
    {{0,0,0,0x1e,0x3f,0x33,0x33,0x37,0x37,0x30,0x3f,0x1e,0,0,0},6,ALL4,0},
    /* here we have a gap for the 26 alphabetics */
    {{15,15,12,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,12,15,15},4,ALL4,TR|BR,0,2},
    {{0,0,0,0x30,0x30,0x30,0x18,0x0c,0x06,0x03,0x03,0x03,0,0,0},6,ALL4,0},
    {{15,15,3,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,3,15,15},4,ALL4,TL|BL,2,0},
    {{0,0,0,0x0c,0x1e,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},6,TL|TR,0},
    {{0,0,0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x3f,0,0,0},6,BL|BR,0},
    {{0,0,0,0x03,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},2,TL|TR,0},
    /* here we have a gap for the 26 lower-case alphabetics */
    {{7,15,12,0x0c,0x0c,0x0c,0x38,0x38,0x0c,0x0c,0x0c,0x0c,12,15,7},6,ALL4,TR|BR,0,2},
    {{0,0,3,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,3,0,0},2,ALL4,0},
    {{0x38,0x3c,0x0c,0x0c,0x0c,0x0c,0x07,0x07,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x38},6,ALL4,TL|BL,2,0},
    {{0,0,0,0x1b,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0,0,0},6,TL|TR,0},
    /* 0x7f is a magic thing which inhibits horiz extends and has zero size */
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},0,ALL4,0},
};

/* Count the set bits in a word. */
static int popcount(unsigned int word)
{
    word = ((word & 0xAAAAAAAA) >>  1) + (word & 0x55555555);
    word = ((word & 0xCCCCCCCC) >>  2) + (word & 0x33333333);
    word = ((word & 0xF0F0F0F0) >>  4) + (word & 0x0F0F0F0F);
    word = ((word & 0xFF00FF00) >>  8) + (word & 0x00FF00FF);
    word = ((word & 0xFFFF0000) >> 16) + (word & 0x0000FFFF);
    return word;
}

/*
 * Simple PRNG for deciding which way to slant vertical extensions.
 * The deal with this thing is that it's seeded entirely off the
 * input string, so displaying exactly the same string twice gives
 * exactly the same output - the same guarantee you get from the
 * global optimiser.
 * 
 * The random number generator we use is simply the CRC32 linear
 * feedback shift register.
 */
static void swash_srand(unsigned *seed, char *text)
{
    int c, i, newbit, x32term, crcword;

    crcword = 0;

    while (*text) {
	c = 0xFF & *text++;

	for (i = 0; i < 8; i++) {
	    newbit = c & 1;
	    c >>= 1;
	    x32term = (crcword & 1) ^ newbit;
	    crcword = (crcword >> 1) ^ (x32term * 0xEDB88320);
	}
    }

    *seed = crcword;
}
static int swash_random_bit(unsigned *seed)
{
    int crcword = *seed;
    int x32term = (crcword & 1);
    crcword = (crcword >> 1) ^ (x32term * 0xEDB88320);
    *seed = crcword;
    return crcword & 1;
}

void swash_text(int x, int y, char *text,
		void (*plot)(void *ctx, int x, int y), void *plotctx)
{
    /*
     * We optimise globally along the string to find the best
     * possible set of character extensions. This is done using a
     * dynamic programming approach: we work backwards[1] along the
     * string, trying all possible extension sets for each
     * character. For each extension set, we determine (a) whether
     * it's even valid according to the rules, and (b) how many
     * extensions it manages to squeeze in. Then, if it satisfies
     * (a), we store it as a possibility for that character.
     * Eventually we run out of choices for the character, and step
     * back one space to the previous one.
     *
     * Where the dynamic programming comes in is in determining
     * (b). For each legal extension set for a character, we not
     * only count the extensions in that character itself, but we
     * also look at the possibilities for the _next_ character
     * (which we had already processed due to working backwards)
     * and see how many of them can legally be placed after this
     * one. Of those, we choose the one with the highest score. So
     * we end up having optimised globally along the whole string
     * to find the largest possible set of extensions consistent
     * with the rules.
     *
     * [1] Working backwards along the string means that the `next'
     * pointers point forwards, which means we can go forwards
     * again in the actual drawing phase. There's no real reason it
     * needs to be this way round: we could optimise forwards and
     * draw backwards if we preferred.
     */

    struct option {
	struct option *next;
	struct chr *chr;
	struct punct *punct;
	int vx, hx;
	int score;
    };

    const int leftx[] = { 0, TL, BL };
    const int rightx[] = { 0, TR, BR };
    const int vertx[] = { TL, TM, TR, BL, BM, BR };

    /*
     * The maximum number of options per character is:
     * 
     *  - We can have MAX_VX_OPTIONS vertical extension choices.
     *  - We can have three (number of elements in leftx) left
     *    feature choices.
     *  - We can have three (number of elements in rightx) right
     *    feature choices.
     *  - However, we can't have the same nonzero left and right
     *    feature, so that restricts the 9 horizontal options to 7.
     */
    struct option opts[MAX_TEXT_LEN][7*MAX_VX_OPTIONS+1];
    int len;
    struct option *opt;
    struct chr *chr;
    struct punct *punct;
    int i, j, k;
    int bestscore;
    struct option *nextbest;
    int left, right, rf;
    int seed;

    swash_srand(&seed, text);

    len = strlen(text);

    for (i = len; i-- ;) {
	opt = opts[i];
	debug(("processing char %d ('%c')\n", i, text[i]));

	if ((text[i] >= 'A' && text[i] <= 'Z') ||
	    (text[i] >= 'a' && text[i] <= 'z')) {
	    /*
	     * An alphabetic character. In this situation:
	     * 
	     *  - we must go through all the possible options for
	     *    extending/connecting left, extending/connecting
	     *    right, and extending vertically. (We treat all
	     *    left and right features as identical during this
	     *    loop, and decide whether they're extensions or
	     *    connections later.)
	     * 
	     *  - A set of extensions is immediately invalid if a
	     *    left or right feature overlaps with a vertical
	     *    extension, or if left and right features both
	     *    exist at the same height. (FIXME: perhaps A
	     *    should be allowed to connect at both BL and BR,
	     *    whereas E shouldn't? Uncertain.)
	     * 
	     *  - A set of extensions may not coexist with an
	     *    option for the next character if:
	     * 
	     *     * the next character is a space (making our
	     *       right feature an extension and not a
	     *       connection) and our right feature is not
	     *       listed in our hx field.
	     * 
	     *     * the next character is a space (making our
	     *       right feature an extension and not a
	     *       connection) and our right feature clashes with
	     *       its left feature (which is the left extension
	     *       of whatever came after the space).
	     * 
	     *     * the next character is a non-space (making our
	     *       right feature a connection and not an
	     *       extension) and our right feature is not listed
	     *       in our hc field.
	     * 
	     *     * the next character is a non-space (making our
	     *       right feature a connection and not an
	     *       extension) and its left feature is not listed
	     *       in its hc field.
	     * 
	     *     * the next character is a non-space (making our
	     *       right feature and its left feature a
	     *       connection) and our right feature and its left
	     *       feature do not match exactly.
	     * 
	     *     * the next character has a BL vertical extension
	     *       while we have a BR, or has a TL while we have
	     *       a TR. (Prevents exceptionally close-packed
	     *       vertical extensions. Note that the letter I is
	     *       listed as having TL|TR or BL|BR at any given
	     *       time - the actual plotting code must notice
	     *       that the two extensions overlap and avoid
	     *       drawing them twice.)
	     */
	    int l, lf, r, rf, v, vx;
	    int myscore;

	    if (text[i] >= 'a')
		chr = &alphabet[text[i] - 'a'];
	    else
		chr = &alphabet[text[i] - 'A'];

	    for (l = 0; l < 3; l++) {
		lf = leftx[l];
		if (((chr->hx|chr->hc) & lf) != lf) {
		    debug(("  skipping invalid left feature 0x%02x\n", lf));
		    continue;
		}
		for (r = 0; r < 3; r++) {
		    rf = rightx[r];
		    if (((chr->hx|chr->hc) & rf) != rf) {
			debug(("  skipping invalid right feature 0x%02x\n",
			       rf));
			continue;
		    }
		    if (lf & R_TO_L(rf))
			continue;      /* left and right features clash */
		    for (v = 0; v < 3; v++) {
			vx = chr->vx[v];

			if (vx & (lf|rf)) {
			    debug(("  skipping vx 0x%02x (feature clash with "
				   "lf/rf 0x%02x/0x%02x)\n",
				   vx, lf, rf));
			    continue;
			}

			debug(("  trying vx=0x%02x hx=0x%02x\n", vx, lf|rf));

			myscore = popcount(lf | rf | vx);
			debug(("  score for this char is %d\n", myscore));

			bestscore = -1;

			if (i+1 >= len) {

			    /*
			     * This is the rightmost char in the
			     * string. We have to check our right
			     * feature against our hx field, but
			     * other than that we're done already.
			     */
			    if (rf != (rf & chr->hx)) {
				debug(("  rightmost char has rx illegal\n"));
				continue;
			    }

			    bestscore = 0;
			    nextbest = NULL;

			} else for (k = 0; opts[i+1][k].score >= 0; k++) {

			    /* Enforce the above validity criteria. */
			    if (!opts[i+1][k].chr && rf != (rf & chr->hx)) {
				debug(("    clashes with opt %p: rx illegal\n",
				       &opts[i+1][k]));
				continue;
			    }
			    if (!opts[i+1][k].chr &&
				(rf & L_TO_R(opts[i+1][k].hx))) {
				debug(("    clashes with opt %p: rx clash\n",
				       &opts[i+1][k]));
				continue;
			    }
			    if (opts[i+1][k].chr && rf != (rf & chr->hc)) {
				debug(("    clashes with opt %p: rc illegal\n",
				       &opts[i+1][k]));
				continue;
			    }
			    if (opts[i+1][k].chr &&
				(LEFT(opts[i+1][k].hx) !=
				 (LEFT(opts[i+1][k].hx) & opts[i+1][k].chr->hc)
				 )) {
				debug(("    clashes with opt %p: lc illegal\n",
				       &opts[i+1][k]));
				continue;
			    }
			    if (opts[i+1][k].chr &&
				(rf != L_TO_R(opts[i+1][k].hx))) {
				debug(("    clashes with opt %p: rc "
				       "mismatch\n", &opts[i+1][k]));
				continue;
			    }
			    if (opts[i+1][k].vx & R_TO_L(vx)) {
				debug(("    clashes with opt %p: vx "
				       "crowding\n", &opts[i+1][k]));
				continue;
			    }

			    debug(("    valid for %p, scoring %d+%d\n",
				   &opts[i+1][k],
				   myscore, opts[i+1][k].score));
			    if (bestscore < opts[i+1][k].score) {
				bestscore = opts[i+1][k].score;
				nextbest = &opts[i+1][k];
			    }
			}

			if (bestscore >= 0) {
			    opt->vx = vx;
			    opt->hx = lf | rf;
			    opt->next = nextbest;
			    opt->score = myscore + bestscore;
			    opt->chr = chr;
			    opt->punct = NULL;
			    debug(("  filing opt %p (vx=0x%02x, hx=0x%02x),"
				   " scoring %d\n", opt, opt->vx, opt->hx,
				   opt->score));
			    opt++;
			}

			if (vx == 0)
			    break;     /* the vx list has terminated */
		    }
		}
	    }
	} else {
	    /*
	     * A punctuation (non-alphabetic) character. This means
	     * we must not have attempted to connect left from the
	     * next character to the right. Also we propagate the
	     * leftward `hx' bits of the next character to ensure
	     * we make no attempt to extend towards them in the
	     * next word. Finally, we also disallow any extensions
	     * which the punctuation character itself doesn't like.
	     * 
	     * We carry forward at most three options: not
	     * extending left, extending left at TL, and at BL.
	     * 
	     * Special case: if this is the last character in the
	     * string, our entire behaviour is fixed and we only
	     * present one option (no extension, no score).
	     */

	    int c = text[i];

	    /* Everything we really don't understand defaults to space. */
	    if (c < ' ' || c > '\x7F')
		c = ' ';

	    if (c >= ' ' && c < 'A')
		punct = &punctuation[c-' '];
	    else if (c > 'Z' && c < 'a')
		punct = &punctuation[c-26-' '];
	    else if (c > 'z')
		punct = &punctuation[c-52-' '];

	    for (j = 0; j < 3; j++) {
		int hx = leftx[j];

		debug(("  trying hx = 0x%02x\n", hx));

		if (i+1 < len) {

		    bestscore = -1;
		    for (k = 0; opts[i+1][k].score >= 0; k++) {
			/* We don't propagate nonzero hx from non-alphas. */
                        if (!opts[i+1][k].chr && hx) {
                            debug(("    skipping opt %p: hx abandoned\n",
				   &opts[i+1][k]));
                            continue;
                        }
			/* For alphas, the hx now has to match. */
			if (opts[i+1][k].chr && opts[i+1][k].hx != hx) {
			    debug(("    skipping opt %p: hx mismatch\n",
				   &opts[i+1][k]));
			    continue;
			}
                        if (opts[i+1][k].chr &&
                            hx != (opts[i+1][k].chr->hx & hx)) {
                            debug(("    skipping opt %p: lx illegal\n",
				   &opts[i+1][k]));
                            continue;
                        }
			if (opts[i+1][k].chr &&
			    opts[i+1][k].hx & R_TO_L(punct->hx_inhibit)) {
			    debug(("    skipping opt %p: lx inhibited\n",
				   &opts[i+1][k]));
			    continue;
			}
			if (opts[i+1][k].chr &&
			    opts[i+1][k].vx & R_TO_L(punct->vx_inhibit)) {
			    debug(("    skipping opt %p: vx illegal\n",
				   &opts[i+1][k]));
			    continue;
			}
			debug(("    valid for opt %p, scores %d\n",
			       &opts[i+1][k], opts[i+1][k].score));
			if (bestscore < opts[i+1][k].score) {
			    bestscore = opts[i+1][k].score;
			    nextbest = &opts[i+1][k];
			}
		    }
		    if (bestscore >= 0) {
			opt->vx = punct->vx_inhibit;
			opt->hx = hx | LEFT(punct->hx_inhibit);
			opt->next = nextbest;
			opt->score = bestscore;
			opt->chr = NULL;
			opt->punct = punct;
			debug(("  filing opt %p, scoring %d\n",
			       opt, opt->score));
			opt++;
		    }
		} else {
		    if (hx) continue;  /* no extensions */
		    debug(("  filing trivial opt %p, scoring zero\n", opt));
		    opt->vx = opt->hx = 0;
		    opt->next = NULL;
		    opt->score = 0;
		    opt->chr = NULL;
		    opt->punct = punct;
		    opt++;
		}
	    }
	}
	opt->score = -1;	       /* terminate the list */
    }

    /*
     * Now all we have left to do is to go through the options for
     * the leftmost character and pick the best one. In the process
     * we must also do a last bit of weeding-out of invalid
     * options: _if_ the character is a non-space, then we must
     * ensure its left feature is in its hx field.
     */
    bestscore = -1;
    for (k = 0; opts[0][k].score >= 0; k++) {
	if (opts[0][k].chr &&
	    LEFT(opts[0][k].hx)!=(opts[0][k].chr->hx & LEFT(opts[0][k].hx))) {
	    debug(("skipping first-char option %p: hx illegal\n", &opts[0][k]));
	    continue;
	}
	if (bestscore < opts[0][k].score) {
	    debug(("first-char option %p is valid, scoring %d\n",
		   &opts[0][k], opts[0][k].score));
	    bestscore = opts[0][k].score;
	    opt = &opts[0][k];
	}
    }

    assert(bestscore >= 0);
    debug(("Total score is %d\n", bestscore));

    /*
     * Now the hurly-burly is well and truly done. All that remains
     * is to plot the actual text.
     */
    left = EXTENSION_LEN;
    rf = right = 0;
    for (i = 0; i <= len; i++) {
	if (i == len)
	    right = EXTENSION_LEN;
	else if (opt->chr)
	    right = 1;
	else
	    right = INTERNAL_EXTENSION_LEN;

	if (rf) {
	    int sx, sy, py;
	    if (rf == TR)
		sy = y, py = 0;
	    else
		sy = y+7, py = 1;
	    sx = x-3;

	    for (k = 0; k < right+2; k++) {
		if (k == right+1 && right > 1) {
		    plot(plotctx, sx, sy+py);
		} else {
		    plot(plotctx, sx,sy);
		    plot(plotctx, sx,sy+1);
		}
		sx++;
	    }
	}

	if (i == len)
	    break;

	debug(("'%c', vx=0x%02x, hx=0x%02x\n", text[i], opt->vx, opt->hx));

	if (opt->chr) {
	    for (j = 0; j < FONT_HEIGHT; j++)
		for (k = 0; k < opt->chr->width; k++)
		    if (opt->chr->bitpattern[j] & (1 << (opt->chr->width-1-k)))
			plot(plotctx, x+k, y+j);

	    /* Plot the vertical extensions. */
	    for (j = 0; j < 6; j++) {
		int sy, dy, sx;
		k = vertx[j];
		if (!(opt->vx & k))
		    continue;
		if (k & (TL|TM|TR))
		    sy = y+1, dy = -1;
		else
		    sy = y+7, dy = +1;
		if (k & (TL|BL))
		    sx = x;
		else if (k & (TR|BR))
		    sx = x+opt->chr->width-2;
		else {
		    /* For TM/BM, we search for the stem we're extending. */
		    sx = 1;
		    while (sx < opt->chr->width+2 &&
			   ((opt->chr->bitpattern[j] >> sx) & 3) != 3)
			sx++;
		    sx = x + opt->chr->width-2-sx;
		}

		/* Special case for letter I: stop if a TR and TL extension
		 * overlap, or a BL and BR do. */
		if ((k & (TR|BR)) && (opt->vx & R_TO_L(k)) && sx == x)
		    continue;

		for (k = 0; k < EXTENSION_LEN+1; k++) {
		    plot(plotctx, sx,sy);
		    plot(plotctx, sx+1,sy);
		    sy += dy;
		}
		plot(plotctx, sx + swash_random_bit(&seed), sy);
	    }

	    /* Plot the left extension, if any. */
	    if (LEFT(opt->hx)) {
		int sx, sy, py;
		if ((opt->hx & TL))
		    sy = y, py = 0;
		else
		    sy = y+7, py = 1;
		sx = x+1;

		for (k = 0; k < left+2; k++) {
		    if (k == left+1 && left > 1) {
			plot(plotctx, sx, sy+py);
		    } else {
			plot(plotctx, sx,sy);
			plot(plotctx, sx,sy+1);
		    }
		    sx--;
		}
	    }

	    x += opt->chr->width+1;
	    left = 1;
	    rf = RIGHT(opt->hx);
	} else {
	    x -= opt->punct->left_overlap;
	    for (j = -EXTENSION_LEN; j < FONT_HEIGHT+EXTENSION_LEN; j++)
		for (k = 0; k < opt->punct->width; k++)
		    if (opt->punct->bitpattern[j+EXTENSION_LEN] &
			(1 << (opt->punct->width-1-k)))
			plot(plotctx, x+k, y+j);
	    x -= opt->punct->right_overlap;
	    if (opt->punct->width)     /* width-0 chars are special case */
		x += opt->punct->width+1;
	    left = INTERNAL_EXTENSION_LEN;
	    rf = 0;
	}

	opt = opt->next;
    }
}

#ifdef TESTMODE

#define MAXWIDTH 400
#define HEIGHT (FONT_HEIGHT + 2*EXTENSION_LEN)

char buffer[HEIGHT][MAXWIDTH];
int minx, maxx;

void plot(void *ignored, int x, int y)
{
    assert(x >= 0 && x < MAXWIDTH && y >= 0 && y < HEIGHT);
    buffer[y][x] = '#';
    if (minx > x)
	minx = x;
    if (maxx < x+1)
	maxx = x+1;
}

int main(int argc, char **argv)
{
    int i;

    memset(buffer, ' ', sizeof(buffer));
    minx = MAXWIDTH+1;
    maxx = -1;

    swash_text(EXTENSION_LEN, EXTENSION_LEN,
	       argc > 1 ? argv[1] : "HELLO WORLD",
	       plot, NULL);

    for (i = 0; i < HEIGHT; i++) {
	printf("%.*s\n", maxx-minx, buffer[i]+minx);
    }

    return 0;
}

#endif
