/*
 * Mastermind solver.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* ----------------------------------------------------------------------
 * Book-keeping, general infrastructure, utility functions.
 */

#ifndef HAVE_NO_ALLOCA
#include <alloca.h>
#define TEMPALLOC alloca
#define TEMPFREE(x)
#else
#include <stdlib.h>
#define TEMPALLOC malloc
#define TEMPFREE(x) free(x)
#endif

#define min(a,b) ( (a)<(b) ? (a) : (b) )
#define max(a,b) ( (a)>(b) ? (a) : (b) )

struct params {
    int pegs;
    int colours;
    int comb;			       /* (colours)^(pegs) */
};

typedef void (*get_mark_fn_t)(struct params p, int *black, int *white,
			      unsigned char *guess, int sure, void *ctx);

struct params make_params(int pegs, int colours)
{
    struct params ret;
    int i;

    ret.pegs = pegs;
    ret.colours = colours;
    ret.comb = 1;
    for (i = 0; i < pegs; i++) {
	assert(ret.comb <= INT_MAX / colours);   /* check overflow */
	ret.comb = ret.comb * colours;
    }

    return ret;
}

void make_comb(struct params p, unsigned char *out, int index)
{
    int i;
    for (i = p.pegs; i-- > 0 ;) {
	out[i] = index % p.colours;
	index /= p.colours;
    }
}

void compute_mark(struct params p, int *black, int *white,
		  unsigned char *a, unsigned char *b)
{
    int i;
    int *acount, *bcount;
    int both, bl;

    acount = TEMPALLOC(p.colours * sizeof(int));
    bcount = TEMPALLOC(p.colours * sizeof(int));
    for (i = 0; i < p.colours; i++)
	acount[i] = bcount[i] = 0;

    /*
     * Count the number of pegs of each colour in each input.
     */
    for (i = 0; i < p.pegs; i++) {
	acount[a[i]]++;
	bcount[b[i]]++;
    }

    /*
     * Each colour gives rise to a marking peg (either black or
     * white) for each peg of that colour in _both_ inputs.
     */
    both = 0;
    for (i = 0; i < p.colours; i++)
	both += min(acount[i], bcount[i]);

    /*
     * Each actual positional match between a and b makes one of
     * those pegs black.
     */
    bl = 0;
    for (i = 0; i < p.pegs; i++)
	if (a[i] == b[i])
	    bl++;
    *black = bl;

    /*
     * And the rest are white.
     */
    *white = both - bl;

#ifdef MARK_DIAGNOSTICS
    printf("compute_mark(");
    for (i = 0; i < p.pegs; i++)
	putchar('a' + a[i]);
    putchar(',');
    for (i = 0; i < p.pegs; i++)
	putchar('a' + b[i]);
    printf(") = %db %dw\n", *black, *white);
#endif

    TEMPFREE(acount);
    TEMPFREE(bcount);
}

/* ----------------------------------------------------------------------
 * Symmetry optimisation.
 * 
 * Since the process of selecting the optimum guess does quite a
 * lot of processing on each candidate guess, it's very helpful to
 * winnow the candidate list before this happens if possible. We
 * can do this by applying symmetry: in a fresh game, it cannot
 * matter whether we guess aabb or ccdd (assuming the opponent's
 * probability distribution to be symmetric in permutations of the
 * colours), and neither can it matter whether we guess abab or
 * aabb (assuming the distribution to be symmetric in permutations
 * of the positions).
 * 
 * Therefore, we can take one look at an initial guess like `bbcc'
 * and immediately know that it isn't worth spending CPU time on
 * analysing it, since `aabb' will give the same analysis results
 * and is as good a choice as any. This massively cuts down on CPU
 * grind during play.
 * 
 * It doesn't have to stop after the initial guess, either. The
 * current symmetry algorithm goes like this:
 * 
 *  - We divide the peg positions into equivalence classes. They
 *    all start off equivalent; two peg positions become distinct
 *    the first time we make a guess which distinguishes them (i.e.
 *    puts a different colour in them).
 * 
 *  - We also keep track of the highest-numbered colour we have so
 *    far used.
 * 
 *  - If the new guess uses any as yet unused colours, it must use
 *    a contiguous set of them starting at the lowest one
 *    available. (Otherwise, the guess can be mapped to a
 *    lexicographically-less one by permuting the unused - and
 *    hence as yet indistinguishable from one another - colours.)
 *    Also, no unused colour may be more frequent than a
 *    lower-numbered one.
 * 
 *  - Within any equivalence class of peg positions, all colours
 *    (not just unused ones) must be used in increasing order.
 */
struct symmetry_data {
    int highest_used_colour;
    unsigned char *eqclasses;
    unsigned char *tmp;
};

int symm_filter(struct params p, struct symmetry_data *symm,
		unsigned char *guess)
{
    int next_unused = symm->highest_used_colour + 1;
    int i;

#ifdef SYMMETRY_DIAGNOSTICS
    printf("symm_filter: ");
    for (i = 0; i < p.pegs; i++)
	putchar('a' + guess[i]);
#endif

    /*
     * Check that unused colours are used in the right order, and
     * with the right frequency. For this section, symm->tmp is an
     * array[p.colours] of the frequency of each colour.
     */
    memset(symm->tmp, 0, p.colours);
    for (i = 0; i < p.pegs; i++)
	symm->tmp[guess[i]]++;
    for (i = symm->highest_used_colour + 2; i < p.colours; i++)
	if (symm->tmp[i-1] < symm->tmp[i]) {
#ifdef SYMMETRY_DIAGNOSTICS
	    printf(" wrong colour frequency\n");
#endif
	    return 0;		       /* colour frequency is non-decreasing */
	}

    /*
     * Now check that colours are increasing within each
     * equivalence class. For this section, symm->tmp is an
     * array[p.pegs] of the lowest acceptable colour number within
     * each equivalence class.
     */
    memset(symm->tmp, 0, p.pegs);
    for (i = 0; i < p.pegs; i++) {
	if (guess[i] < symm->tmp[symm->eqclasses[i]]) {
#ifdef SYMMETRY_DIAGNOSTICS
	    printf(" wrong colour ordering\n");
#endif
	    return 0;		       /* colour ordering was violated */
	}
	symm->tmp[symm->eqclasses[i]] = guess[i];
    }

    /*
     * If we haven't tripped over anything by now, we're clear.
     */
#ifdef SYMMETRY_DIAGNOSTICS
    printf(" ok\n");
#endif
    return 1;
}

void symm_update(struct params p, struct symmetry_data *symm,
		 unsigned char *guess)
{
    int i;

    /*
     * Update the highest-used-colour index.
     */
    for (i = 0; i < p.pegs; i++)
	if (guess[i] > symm->highest_used_colour)
	    symm->highest_used_colour = guess[i];

    /*
     * Update the equivalence classes. 
     */
    assert(p.pegs * p.colours <= 0xFF);
    memset(symm->tmp, 0xFF, p.pegs * p.colours);
    for (i = 0; i < p.pegs; i++) {
	int index = p.pegs * guess[i] + symm->eqclasses[i];
	if (symm->tmp[index] == 0xFF)
	    symm->tmp[index] = i;
    }
    for (i = 0; i < p.pegs; i++) {
	int index = p.pegs * guess[i] + symm->eqclasses[i];
	assert(symm->tmp[index] != 0xFF);
	symm->eqclasses[i] = symm->tmp[index];
    }
#ifdef SYMMETRY_DIAGNOSTICS
    printf("symm_update: ");
    for (i = 0; i < p.pegs; i++)
	putchar('a' + symm->eqclasses[i]);
    printf(", %d\n", symm->highest_used_colour);
#endif
}

/* ----------------------------------------------------------------------
 * Actual strategy module.
 */

/*
 * Select the next guess, given a list of possibilities.
 * 
 * Currently the strategy is:
 * 
 *  - we aim to minimise the worst-case number of possibilities in
 *    the move after this one.
 *  - So, for every _possible_ guess, we divide our current
 *    possibility list into subsets based on what answer they would
 *    give for that guess. Then we find the size of the largest
 *    subset. After doing this for each possible guess, we actually
 *    choose a guess which minimises that maximum subset size.
 *  - Additionally, we select a guess from the possibility list
 *    itself wherever possible. In rare cases, selecting a guess
 *    which we know is not itself possible is actually the optimal
 *    move.
 * 
 * Parameters:
 * 
 *  - `output' is an array of `p.pegs' unsigned chars which will
 *    hold the guess we decide on.
 *  - `poss' is an array of `p.comb' unsigned chars which are
 *    nonzero iff that combination has not yet been ruled out.
 * 
 * Return value is 1 if we're _sure_ of the answer (i.e. there is
 * only one possibility).
 */
int nextguess(struct params p, unsigned char *output, unsigned char *poss,
	      struct symmetry_data *symm)
{
    unsigned char *gc, *rc;	       /* guess and result candidates */
    int *setsizes;
    int best, bestmaxset, maxset;
    int i, j;

    gc = TEMPALLOC(p.pegs);
    rc = TEMPALLOC(p.pegs);
    setsizes = TEMPALLOC(((p.pegs+1) * (p.pegs+1)) * sizeof(int));

    best = -1;
    bestmaxset = p.comb * 4 + 1;

    /*
     * Check the easy case.
     */
    for (i = j = 0; i < p.comb; i++)
	if (poss[i]) {
	    if (j == 0)
		make_comb(p, output, i);
	    j++;
	}
    if (j == 1)
	return 1;

    for (i = 0; i < p.comb; i++) {
	/*
	 * Construct combination i, and see how well it would do as
	 * a guess.
	 */
	make_comb(p, gc, i);

	if (!symm_filter(p, symm, gc))
	    continue;

#ifdef EASY_STRATEGY
	if (!poss[i])
	    continue;
#endif

	for (j = 0; j < (p.pegs+1) * (p.pegs+1); j++) {
	    setsizes[j] = 0;
	}
	maxset = 0;

	for (j = 0; j < p.comb; j++)
	    if (poss[j]) {
		int black, white, index;

		make_comb(p, rc, j);
		compute_mark(p, &black, &white, gc, rc);
		index = black * (p.pegs+1) + white;
		setsizes[index]++;
		if (maxset < setsizes[index])
		    maxset = setsizes[index];
		if (maxset * 2 > bestmaxset)
		    break;	       /* early termination */
	    }

	/*
	 * Now we know the maximum set size for this guess
	 * candidate. Multiply it by 2, and add 1 if the guess is
	 * not itself a possibility. (This causes us to value an
	 * actually possible guess over an impossible one in the
	 * event of a tie. Apart from the slight strategic
	 * advantage of maximising the chance of an early
	 * termination, this is also in particular vital to
	 * ensuring that when there's only one possibility left we
	 * guess it!)
	 */
	maxset = (maxset * 2) + (!poss[i]);
	if (maxset < bestmaxset) {
	    bestmaxset = maxset;
	    best = i;
	}
#ifdef SETSIZE_DIAGNOSTICS
	printf("%d %d %d %d\n", i, maxset, bestmaxset, best);
#endif
    }

    TEMPFREE(gc);
    TEMPFREE(rc);
    TEMPFREE(setsizes);

    assert(best >= 0);
    make_comb(p, output, best);

    return 0;
}

int winnow(struct params p, unsigned char *poss, unsigned char *guess,
	   int black, int white)
{
    unsigned char *tmp;
    int i, b, w, count;

    tmp = TEMPALLOC(p.pegs);

    count = 0;

    for (i = 0; i < p.comb; i++)
	if (poss[i]) {
	    make_comb(p, tmp, i);
	    compute_mark(p, &b, &w, tmp, guess);
	    if (b != black || w != white)
		poss[i] = 0;
	    else
		count++;
	}

    return count;
}

struct userguess {
    int len;
    unsigned char *guess;
    struct userguess *next;
};

int playgame(struct params p, get_mark_fn_t mark, void *markctx, int *moves,
	     struct userguess *userguesses)
{
    unsigned char *poss = TEMPALLOC(p.comb);
    unsigned char *guess = TEMPALLOC(p.pegs);
    struct symmetry_data asymm, *symm = &asymm;
    int remaining, ret, nmoves;

    memset(poss, 1, p.comb);

    symm->highest_used_colour = -1;
    symm->eqclasses = TEMPALLOC(p.pegs);
    symm->tmp = TEMPALLOC(p.pegs * p.colours);
    memset(symm->eqclasses, 0, p.pegs);

    nmoves = 0;

    while (1) {
	int b, w, sure;
	if (userguesses && userguesses->len < 0) {
	    int c;
	    while (userguesses && userguesses->len < 0)
		userguesses = userguesses->next;
	    printf("Possibilities:");
	    for (c = 0; c < p.comb; c++)
		if (poss[c]) {
		    int i;
		    make_comb(p, guess, c);
		    putchar(' ');
		    for (i = 0; i < p.pegs; i++)
			putchar('a' + guess[i]);
		}
	    printf("\n");
	}
	if (userguesses) {
	    sure = 0;
	    memcpy(guess, userguesses->guess, p.pegs);
	    userguesses = userguesses->next;
	} else {
	    sure = nextguess(p, guess, poss, symm);
	}
	symm_update(p, symm, guess);
	mark(p, &b, &w, guess, sure, markctx);
	nmoves++;
	if (sure) {
	    ret = 1;
	    break;
	}
	assert(b + w <= p.pegs);
	if (b == p.pegs && w == 0) {
	    ret = 1;
	    break;
	}
	remaining = winnow(p, poss, guess, b, w);
	if (remaining == 0) {
	    ret = 0;
	    break;
	}
    }

    TEMPFREE(poss);
    TEMPFREE(guess);
    TEMPFREE(symm->eqclasses);
    TEMPFREE(symm->tmp);

    if (moves)
	*moves = nmoves;

    return ret;
}

/* ----------------------------------------------------------------------
 * User interface and main program.
 */

void ask_user_for_mark(struct params p, int *black, int *white,
		       unsigned char *guess, int sure, void *ctx)
{
    int i, b, w;
    char input[4096];

    if (sure)
	printf("Then it has to be ");
    else
	printf("Enter marking pegs for guess ");
    for (i = 0; i < p.pegs; i++)
	putchar('a' + guess[i]);

    if (sure) {
	printf(".\n");
	*black = p.pegs;
	*white = 0;
	return;
    }
    printf(": ");

    if (!fgets(input, sizeof(input), stdin)) {
	printf("\nGame abandoned.\n");
	exit(1);
    }

    b = w = 0;
    for (i = 0; input[i]; i++) {
	if (input[i] == 'b' || input[i] == 'B' ||   /* black */
	    input[i] == 'r' || input[i] == 'R' ||   /* red (US Mastermind?) */
	    input[i] == 'g' || input[i] == 'G')     /* gear turns (NetHack) */
	    b++;
	else if (input[i] == 'w' || input[i] == 'W' ||/* white */
		 input[i] == 't' || input[i] == 'T')  /* tumbler clicks (NH) */
	    w++;
    }

    *black = b;
    *white = w;
}

void compute_internal_mark(struct params p, int *black, int *white,
			   unsigned char *guess, int sure, void *ctx)
{
    unsigned char *real = (unsigned char *)ctx;
    compute_mark(p, black, white, guess, real);
    assert(!sure || (*black == p.pegs && *white == 0));
}

void compute_internal_mark_verbose(struct params p, int *black, int *white,
				   unsigned char *guess, int sure, void *ctx)
{
    unsigned char *real = (unsigned char *)ctx;
    int i;

    compute_mark(p, black, white, guess, real);
    if (sure) {
	assert(*black == p.pegs && *white == 0);
	printf("So it had to be ");
	for (i = 0; i < p.pegs; i++)
	    putchar('a' + guess[i]);
    } else {
	printf("Guessed ");
	for (i = 0; i < p.pegs; i++)
	    putchar('a' + guess[i]);
	printf(": score ");
	for (i = 0; i < *black; i++)
	    putchar('b');
	for (i = 0; i < *white; i++)
	    putchar('w');
    }
    printf("\n");
}

static const char helptext[] =
    "usage: mmsolver [-pPEGS] [-cCOLOURS] [-a [-v] | -rRESULT]\n"
    "where: -pPEGS       select number of pegs in game (default 4)\n"
    "       -cCOLOURS    select number of colours in game (default 6)\n"
    "       -a           try every combination\n"
    "       -v           verbose: print all move counts during a run with -a\n"
    "       -rRESULT     specify a result and run algorithm on it\n"
    "and:   if neither -v nor -r given, will print guesses and ask for marks\n"
    "       in the form of a string made up of some number of b and w\n"
    "       (including none); r and g are synonyms for b, and t for w.\n"
    "       combinations are specified as a sequence of lower case letters\n"
    "       starting at a\n";

int main(int argc, char **argv)
{
    int tryall = 0, verbose = 0;
    int pegs = 4, colours = 6;
    int i;
    unsigned char *answer = NULL;
    int answerlen = -1;
    struct params p;
    char *pname = argv[0];
    struct userguess *ughead = NULL, *ugtail = NULL, *ug;

    while (--argc) {
	char *p = *++argv;
	char *v;

	if (*p == '-') {
	    p++;
	    while (*p) {
		int c = *p++;
		switch (c) {
		  case 'h':
		  case '?':
		    fputs(helptext, stdout);
		    return 0;
		  case 'a':
		    tryall = 1;
		    break;
		  case 'v':
		    verbose = 1;
		    break;
		  case 's':
		    ug = TEMPALLOC(sizeof(struct userguess));
		    ug->len = -1;
		    ug->guess = NULL;

		    if (ugtail)
			ugtail->next = ug;
		    else
			ughead = ug;
		    ugtail = ug;
		    ug->next = NULL;
		    break;
		  case 'p':
		  case 'c':
		  case 'r':
		  case 'g':
		    if (*p)
			v = p, p = "";
		    else if (--argc)
			v = *++argv;
		    else {
			fprintf(stderr, "%s: expected argument to '-%c'\n",
				pname, c);
			return 1;
		    }
		    switch (c) {
		      case 'p':
			pegs = atoi(v);
			break;
		      case 'c':
			colours = atoi(v);
			break;
		      case 'r':
			answerlen = strlen(v);
			answer = TEMPALLOC(answerlen);
			for (i = 0; i < answerlen; i++) {
			    answer[i] = v[i] - 'a';
			}
			break;
		      case 'g':
			ug = TEMPALLOC(sizeof(struct userguess));
			ug->len = strlen(v);
			ug->guess = TEMPALLOC(ug->len);
			for (i = 0; i < ug->len; i++)
			    ug->guess[i] = v[i] - 'a';

			if (ugtail)
			    ugtail->next = ug;
			else
			    ughead = ug;
			ugtail = ug;
			ug->next = NULL;
			break;
		    }
		    break;
		  default:
		    fprintf(stderr, "%s: unrecognised argument '-%c'\n",
			    pname, c);
		    return 1;
		}
	    }
	}
    }

    if (pegs < 1 || colours < 1) {
	fprintf(stderr, "%s: parameters out of range\n", pname);
	return 1;
    }

    for (ug = ughead; ug; ug = ug->next) {
	if (ug->len < 0)
	    continue;		       /* special case */
	for (i = 0; i < ug->len; i++) {
	    if (ug->guess[i] >= colours) {
		fprintf(stderr, "%s: user-supplied guess '", pname);
		for (i = 0; i < ug->len; i++)
		    fputc('a' + ug->guess[i], stderr);
		fprintf(stderr, "' uses a colour beyond limit of '%c'\n",
			'a' + colours - 1);
		return 1;
	    }
	}
	if (ug->len != pegs) {
	    fprintf(stderr, "%s: user-supplied guess '", pname);
	    for (i = 0; i < ug->len; i++)
		fputc('a' + ug->guess[i], stderr);
	    fprintf(stderr, "' does not match peg count %d\n", pegs);
	    return 1;
	}
    }

    if (answer && !tryall) {
	if (answerlen != pegs) {
	    fprintf(stderr, "%s: combination supplied to '-r' is wrong"
		    " length\n", pname);
	    return 1;
	}
	for (i = 0; i < pegs; i++)
	    if (answer[i] >= colours) {
		fprintf(stderr, "%s: combination supplied to '-r' contains"
			" invalid colour\n", pname);
		return 1;
	    }
    }

    p = make_params(pegs, colours);
    if (tryall) {
	int i, movetotal, moves, ret;
	unsigned char *comb = TEMPALLOC(p.pegs);
	int *freq = NULL;
	int freqsize = 0;

	movetotal = 0;

	for (i = 0; i < p.comb; i++) {
	    make_comb(p, comb, i);
	    /* No sensible reason to support pre-made moves in this mode */
	    ret = playgame(p, compute_internal_mark, comb, &moves, NULL);
	    assert(ret);
	    if (verbose) {
		int j;
		for (j = 0; j < p.pegs; j++)
		    putchar('a' + comb[j]);
		printf(": %d\n", moves);
	    }
	    if (moves >= freqsize) {
		int ofs = freqsize;
		int j;

		freqsize = moves + 32;
		freq = realloc(freq, freqsize * sizeof(int));

		for (j = ofs; j < freqsize; j++)
		    freq[j] = 0;
	    }
	    freq[moves]++;
	    movetotal += moves;
	}

	TEMPFREE(comb);

	for (i = 0; i < freqsize; i++)
	    if (freq[i])
		printf("%d moves: %d case%s\n", i, freq[i],
		       freq[i] > 1 ? "s" : "");
	printf("average %d/%d = %g\n", movetotal, p.comb, (double)movetotal/p.comb);
    } else {
	int moves;
	int ret;
	if (answer)
	    ret = playgame(p, compute_internal_mark_verbose, answer, &moves,
			   ughead);
	else
	    ret = playgame(p, ask_user_for_mark, NULL, &moves, ughead);

	if (ret)
	    printf("Solved successfully after %d moves.\n", moves);
	else
	    printf("Contradiction reached!\n");
    }
    return 0;
}
