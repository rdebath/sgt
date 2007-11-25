/*
 * Map a group by breadth-first search given some generators, to
 * find the shortest path back to the identity for every element.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#define lenof(x) ( sizeof(x) / sizeof(*(x)) )

/*
 * This structure describes a group type. It can expect to be the
 * first element of a larger structure containing parameters.
 * (Thus, the methods defined here may cast from the `struct group'
 * back to its containing structure.)
 */
struct group {
    /*
     * Return the size in bytes of the natural form of a group
     * element.
     */
    int (*eltsize)(struct group *gctx);
    /*
     * Return the natural form of the starting (usually identity)
     * group element(s). Returns 1 if an element was returned, 0 if
     * not (because i was too big).
     */
    int (*identity)(struct group *gctx, void *velt, int i);
    /*
     * Give an integer index for the natural form of a group
     * element.
     */
    int (*index)(struct group *gctx, void *velt);
    /*
     * Turn an integer index back into the natural element form.
     */
    void (*fromindex)(struct group *gctx, void *velt, int index);
    /*
     * Find the maximum index ever used by the above routines.
     * (Actually, the maximum plus 1, in conventional C idiom.)
     */
    int (*maxindex)(struct group *gctx);
    /*
     * Print out an element.
     */
    void (*printelt)(struct group *gctx, void *velt);
    /*
     * Parse an element from string form. Return NULL on success,
     * or a message describing the parse error.
     */
    char *(*parseelt)(struct group *gctx, void *velt, char *text);
    /*
     * Return the number of available moves (generators) in the
     * group. Guaranteed to be called before makemove(), so it may
     * initialise things.
     */
    int (*moves)(struct group *gctx);
    /*
     * Make move number n on a given element. `out' is true if
     * we're making outward moves (while mapping the group) and
     * false for inward ones (solving using the resulting map).
     */
    void (*makemove)(struct group *gctx, void *velt, int n, int out);
    /*
     * Return the name of move number n.
     */
    char *(*movename)(struct group *gctx, int n);
};

/* ----------------------------------------------------------------------
 * Group parameter structure which deals with groups that are
 * basically permutations. In this section we also handle groups
 * which are permutations of multisets, i.e. which have some sets
 * of identical elements.
 */
struct perm {
    struct group vtable;
    int total, n, *counts;
    int nmoves;
    char *movenames;
    int **moves;
};

int perm_eltsize(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;

    return ctx->total * sizeof(int);
}

int perm_identity(struct group *gctx, void *velt, int index)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j, k;

    if (index > 0) return 0;

    for (i = j = 0; i < ctx->n; i++) {
	for (k = 0; k < ctx->counts[i]; k++)
	    elt[j++] = i;
    }

    return 1;
}

int perm_index(struct group *gctx, void *velt)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j, k, n, nck, K, N, NcK, subindex, mult, ret;
    int *p;

    n = ctx->total;

    ret = 0;
    mult = 1;
    for (i = 0; i < ctx->n; i++) {
	k = ctx->counts[i];

	/*
	 * We are completely ignoring elements of elt[] less than
	 * i; among the remaining n, we are looking for the k
	 * elements equal to i, and constructing an index in the
	 * range 0 to (n choose k)-1 indicating their positions in
	 * the list.
	 * 
	 * So, first compute n choose k.
	 */
	K = k;
	if (K+K > n)
	    K = n - K;
	nck = 1;
	for (j = 0; j < K; j++)
	    nck = (nck * (n-j)) / (j+1);

	/*
	 * Now adjust this gradually as we go through the array.
	 */
	p = elt;
	subindex = 0;
	K = k;
	N = n;
	NcK = nck;
	while (1) {
	    if (K == 0)
		break;

	    while (*p < i)
		p++;		       /* ignore this element */

	    if (*p == i) {
		/*
		 * First element found.
		 */
		NcK = NcK * K / N;
		K--;
	    } else {
		/*
		 * First element not found, meaning we have just
		 * skipped over (N-1 choose K-1) positions.
		 */
		subindex += NcK * K / N;
		NcK = NcK * (N-K) / N;
	    }
	    p++;
	    N--;
	}

	/*
	 * Now subindex is the value we want.
	 */
	ret += subindex * mult;
	mult *= nck;
	n -= k;
    }

    return ret;
}

void perm_fromindex(struct group *gctx, void *velt, int index)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j, k, n, nck, K, N, NcK, subindex, max;
    int *p;

    n = max = ctx->total;

    for (i = 0; i < n; i++)
	elt[i] = max;		       /* `not set yet' value */

    for (i = 0; i < ctx->n; i++) {
	k = ctx->counts[i];

	/*
	 * We divide index by (n choose k) and use the remainder
	 * to decide which of the n elements of elt[] currently
	 * set to max we should set to i.
	 * 
	 * So, first compute n choose k.
	 */
	K = k;
	if (K+K > n)
	    K = n - K;
	nck = 1;
	for (j = 0; j < K; j++)
	    nck = (nck * (n-j)) / (j+1);

	subindex = index % nck;
	index /= nck;

	/*
	 * Now adjust this gradually as we go through the array.
	 */
	p = elt;
	K = k;
	N = n;
	NcK = nck;
	while (K > 0) {
	    while (*p != max)
		p++;
	    if (subindex < NcK * K / N) {
		/*
		 * First element is here.
		 */
		*p = i;
		NcK = NcK * K / N;
		K--;
	    } else {
		/*
		 * It isn't.
		 */
		subindex -= NcK * K / N;
		NcK = NcK * (N-K) / N;
	    }
	    p++;
	    N--;
	}

	n -= k;
    }
}

int perm_maxindex(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;
    int f, i, j, k, n, nck;

    n = ctx->total;

    f = 1;
    for (i = 0; i < ctx->n; i++) {
	k = ctx->counts[i];
	if (k+k > n)
	    k = n - k;
	nck = 1;
	for (j = 0; j < k; j++)
	    nck = (nck * (n-j)) / (j+1);
	f *= nck;
	n -= k;
    }

    return f;
}

void perm_printelt(struct group *gctx, void *velt)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->total; i++)
        printf("%s%d", i ? "," : "", elt[i]+1);
}

char *perm_parseelt(struct group *gctx, void *velt, char *s)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j;

    for (i = 0; i < ctx->total; i++) {
        while (*s && !isdigit((unsigned char)*s)) s++;
        if (!*s)
            return "Not enough numbers in list";
        elt[i] = atoi(s) - 1;
        while (*s && isdigit((unsigned char)*s)) s++;
    }

    for (i = 0; i < ctx->n; i++) {
	int c = 0;
        for (j = 0; j < ctx->total; j++)
            if (elt[j] == i)
		c++;
        if (c != ctx->counts[i])
            return "Wrong number of some index";
    }

    return NULL;
}

int perm_moves(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;
    return ctx->nmoves;
}

void perm_makemove(struct group *gctx, void *velt, int n, int out)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int *i = ctx->moves[n];
    int tmp;

    while (*i >= 0) {
	tmp = elt[i[0]];
	while (i[1] >= 0) {
	    elt[i[0]] = elt[i[1]];
	    i++;
	}
	elt[i[0]] = tmp;
	i++;
	/* now i points at the -1 terminating a cycle description */
	i++;
	/* now i points at the _next_ cycle description, or a terminating -1 */
    }
}

char *perm_movename(struct group *gctx, int n)
{
    struct perm *ctx = (struct perm *)gctx;
    char *c = ctx->movenames;
    while (n > 0) {
        c += 1+strlen(c);
        n--;
    }
    return c;
}

int twiddle3x3_movedata[] = {
    0,1,4,3,-1,-1,
    1,2,5,4,-1,-1,
    3,4,7,6,-1,-1,
    4,5,8,7,-1,-1,
    0,3,4,1,-1,-1,
    1,4,5,2,-1,-1,
    3,6,7,4,-1,-1,
    4,7,8,5,-1,-1,
};

int *twiddle3x3_moves[] = {
    twiddle3x3_movedata + 0 * 6,
    twiddle3x3_movedata + 1 * 6,
    twiddle3x3_movedata + 2 * 6,
    twiddle3x3_movedata + 3 * 6,
    twiddle3x3_movedata + 4 * 6,
    twiddle3x3_movedata + 5 * 6,
    twiddle3x3_movedata + 6 * 6,
    twiddle3x3_movedata + 7 * 6,
};

int twiddle3x3counts[] = { 1,1,1,1,1,1,1,1,1 };
int rtwiddle3x3counts[] = { 3,3,3 };

struct perm twiddle3x3 = {
    {perm_eltsize, perm_identity, perm_index, perm_fromindex, perm_maxindex,
     perm_printelt, perm_parseelt, perm_moves, perm_makemove, perm_movename},
    9, 9, twiddle3x3counts,
    8, "a\0b\0c\0d\0A\0B\0C\0D", twiddle3x3_moves
};

struct perm rtwiddle3x3 = {
    {perm_eltsize, perm_identity, perm_index, perm_fromindex, perm_maxindex,
     perm_printelt, perm_parseelt, perm_moves, perm_makemove, perm_movename},
    9, 3, rtwiddle3x3counts,
    8, "a\0b\0c\0d\0A\0B\0C\0D", twiddle3x3_moves
};

/* ----------------------------------------------------------------------
 * Group parameter structure which deals with 4x4 Twiddle (which
 * has strong parity invariants and so is worth indexing in a more
 * efficient manner than the naive 16! approach).
 * 
 * The invariants for 4x4 Twiddle are:
 * 
 *  - every tile stays on its original chessboard colour. Thus
 *    1,3,6,8,9,11,14,16 and 2,4,5,7,10,12,13,15 are disjoint at
 *    all times. This reduces the number of permutations from 16!
 *    to 8!^2.
 * 
 *  - the permutation parity of each half must always be the same,
 *    which loses another factor of 2.
 */

static const int twiddle4x4_white[8] = { 0,2,5,7,8,10,13,15 };
static const int twiddle4x4_black[8] = { 1,3,4,6,9,11,12,14 };
#define CHESS4x4(x) ( (((x)>>2) ^ (x)) & 1 )
#define CHESSINDEX(x) ( (x)>>1 )

int twiddle4x4_index(struct group *gctx, void *velt)
{
    int *elt = (int *)velt;
    int i, j, mult, ret;

    ret = 0;

    /*
     * Index the permutations of the white squares.
     */
    mult = 8;
    for (i = 0; i < 7; i++) {
	int k = 0;
	for (j = 0; j < i; j++)
	    if (elt[twiddle4x4_white[j]] < elt[twiddle4x4_white[i]])
		k++;
	ret = ret * mult + CHESSINDEX(elt[twiddle4x4_white[i]]) - k;
	mult--;
    }

    /*
     * Now index the black squares, but stop short of the final
     * multiplication by two because that's deducible from the
     * rest.
     */
    mult = 8;
    for (i = 0; i < 6; i++) {
	int k = 0;
	for (j = 0; j < i; j++)
	    if (elt[twiddle4x4_black[j]] < elt[twiddle4x4_black[i]])
		k++;
	ret = ret * mult + CHESSINDEX(elt[twiddle4x4_black[i]]) - k;
	mult--;
    }

    return ret;
}

void twiddle4x4_fromindex(struct group *gctx, void *velt, int index)
{
    int *elt = (int *)velt;
    int i, j, mult, bperm, wperm;

    /*
     * Start by putting on the implicit final bit. We'll work out
     * what it _should_ have said later.
     */
    index *= 2;

    /*
     * Now unwind the permutation of the black squares.
     */
    elt[twiddle4x4_black[7]] = 0;
    mult = 2;
    bperm = 0;
    for (i = 7; i-- ;) {
	elt[twiddle4x4_black[i]] = index % mult;
	index /= mult;
	for (j = i+1; j < 8; j++)
	    if (elt[twiddle4x4_black[j]] >= elt[twiddle4x4_black[i]])
		elt[twiddle4x4_black[j]]++, bperm ^= 1;
	mult++;
    }
    /*
     * And then the white squares.
     */
    elt[twiddle4x4_white[7]] = 0;
    mult = 2;
    wperm = 0;
    for (i = 7; i-- ;) {
	elt[twiddle4x4_white[i]] = index % mult;
	index /= mult;
	for (j = i+1; j < 8; j++)
	    if (elt[twiddle4x4_white[j]] >= elt[twiddle4x4_white[i]])
		elt[twiddle4x4_white[j]]++, wperm ^= 1;
	mult++;
    }

    /*
     * If the permutations were of different parity, swap the last
     * two black squares.
     */
    if (wperm != bperm) {
	j = elt[twiddle4x4_black[6]];
	elt[twiddle4x4_black[6]] = elt[twiddle4x4_black[7]];
	elt[twiddle4x4_black[7]] = j;
    }

    /*
     * And finally, map everything back to real number values.
     */
    for (i = 0; i < 8; i++) {
	elt[twiddle4x4_white[i]] = twiddle4x4_white[elt[twiddle4x4_white[i]]];
	elt[twiddle4x4_black[i]] = twiddle4x4_black[elt[twiddle4x4_black[i]]];
    }
}

int twiddle4x4_maxindex(struct group *gctx)
{
    int f, i;

    f = 1;
    for (i = 1; i <= 8; i++)
	f *= i;

    return f * f / 2;
}

char *twiddle4x4_parseelt(struct group *gctx, void *velt, char *s)
{
    int *elt = (int *)velt;
    char *ret;
    int i, j, bp, wp;

    ret = perm_parseelt(gctx, velt, s);
    if (ret)
	return ret;

    /*
     * Check the chessboard property and permutation parity.
     */
    for (i = 0; i < 16; i++)
	if (CHESS4x4(i) != CHESS4x4(elt[i]))
	    return "Chessboard separation violated";
    bp = wp = 0;
    for (i = 0; i < 7; i++)
	for (j = i+1; j < 8; j++) {
	    if (elt[twiddle4x4_black[i]] > elt[twiddle4x4_black[j]])
		bp ^= 1;
	    if (elt[twiddle4x4_white[i]] > elt[twiddle4x4_white[j]])
		wp ^= 1;
	}
    if (bp != wp)
	return "Parity constraint violated";

    return NULL;
}

int twiddle4x4_movedata[] = {
    0,2,10,8,-1,1,6,9,4,-1,-1,
    1,3,11,9,-1,2,7,10,5,-1,-1,
    4,6,14,12,-1,5,10,13,8,-1,-1,
    5,7,15,13,-1,6,11,14,9,-1,-1,
    0,8,10,2,-1,1,4,9,6,-1,-1,
    1,9,11,3,-1,2,5,10,7,-1,-1,
    4,12,14,6,-1,5,8,13,10,-1,-1,
    5,13,15,7,-1,6,9,14,11,-1,-1,
};

int *twiddle4x4_moves[] = {
    twiddle4x4_movedata + 0 * 11,
    twiddle4x4_movedata + 1 * 11,
    twiddle4x4_movedata + 2 * 11,
    twiddle4x4_movedata + 3 * 11,
    twiddle4x4_movedata + 4 * 11,
    twiddle4x4_movedata + 5 * 11,
    twiddle4x4_movedata + 6 * 11,
    twiddle4x4_movedata + 7 * 11,
};

int twiddle4x4counts[] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };

struct perm twiddle4x4 = {
    {perm_eltsize, perm_identity, twiddle4x4_index, twiddle4x4_fromindex,
     twiddle4x4_maxindex, perm_printelt, twiddle4x4_parseelt, perm_moves,
     perm_makemove, perm_movename},
    16, 16, twiddle4x4counts,
    8, "a\0b\0c\0d\0A\0B\0C\0D", twiddle4x4_moves
};

/* ----------------------------------------------------------------------
 * Group parameter structure which deals with 3x3 orientable
 * Twiddle.
 */
struct otwid {
    struct group vtable;
    int w, n;
    int nmoves;
    char *movenames;
    int **moves;
};

int otwid_eltsize(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;

    return ctx->n * sizeof(int);
}

int otwid_identity(struct group *gctx, void *velt, int index)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i;

    if (index > 0) return 0;

    for (i = 0; i < ctx->n; i++)
	elt[i] = i*4;

    return 1;
}

int otwid_index(struct group *gctx, void *velt)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret;

    /*
     * Index the permutation.
     */
    ret = 0;
    mult = ctx->n;
    for (i = 0; i < ctx->n-1; i++) {
	int k = 0;
	for (j = 0; j < i; j++)
	    if (elt[j] < elt[i])
		k++;
	ret = ret * mult + elt[i]/4 - k;
	mult--;
    }

    /*
     * Now index the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, o, sp, dp, p;

        e = elt[i] / 4;
        o = elt[i] & 3;
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        assert(p == (o & 1));
        o -= p;
        ret = ret * 2 + (o >> 1);
    }

    return ret;
}

void otwid_fromindex(struct group *gctx, void *velt, int index)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret, orients;

    orients = index;
    index >>= ctx->n;

    /*
     * Construct the permutation.
     */
    ret = 0;
    elt[ctx->n-1] = 0;
    mult = 2;
    for (i = ctx->n-1; i-- ;) {
	elt[i] = index % mult;
	index /= mult;
	for (j = i+1; j < ctx->n; j++)
	    if (elt[j] >= elt[i])
		elt[j]++;
	mult++;
    }

    /*
     * Now set up the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, sp, dp, p;

        e = elt[i];
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        elt[i] *= 4;
        elt[i] += p & 1;
        if (orients & (1 << (ctx->n-1-i)))
            elt[i] += 2;
    }
}

int otwid_maxindex(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int f, i;

    f = 1;
    for (i = 1; i <= ctx->n; i++)
	f *= i;

    return f << ctx->n;
}

void otwid_printelt(struct group *gctx, void *velt)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->n; i++)
        printf("%d%c", elt[i]/4+1, "uldr"[elt[i] & 3]);
}

char *otwid_parseelt(struct group *gctx, void *velt, char *s)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j;

    for (i = 0; i < ctx->n; i++) {
        while (*s && !isdigit((unsigned char)*s)) s++;
        if (!*s)
            return "Not enough numbers in list";
        elt[i] = 4 * (atoi(s) - 1);
        while (*s && isdigit((unsigned char)*s)) s++;
        if (*s == 'u')
            /* do nothing */;
        else if (*s == 'l')
            elt[i]++;
        else if (*s == 'd')
            elt[i] += 2;
        else if (*s == 'r')
            elt[i] += 3;
    }

    for (i = 0; i < ctx->n; i++) {
        for (j = 0; j < ctx->n; j++)
            if (elt[i]/4 == j)
                break;
        if (j == ctx->n)
            return "Numbers were not a permutation";
    }

    /*
     * Now index the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, o, sp, dp, p;

        e = elt[i] / 4;
        o = elt[i] & 3;
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        if (p != (o & 1))
            return "Orientations were inconsistent with positions";
    }

    return NULL;
}

int otwid_moves(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;
    return ctx->nmoves;
}

void otwid_makemove(struct group *gctx, void *velt, int n, int out)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int *i = ctx->moves[n];
    int rot = *i++;
    int tmp;

    while (*i >= 0) {
	tmp = elt[i[0]];
	while (i[1] >= 0) {
	    elt[i[0]] = elt[i[1]];
            elt[i[0]] ^= (elt[i[0]] ^ (elt[i[0]] + rot)) & 3;
	    i++;
	}
	elt[i[0]] = tmp;
        elt[i[0]] ^= (elt[i[0]] ^ (elt[i[0]] + rot)) & 3;
	i++;
	/* now i points at the -1 terminating a cycle description */
	i++;
	/* now i points at the _next_ cycle description, or a terminating -1 */
    }
}

char *otwid_movename(struct group *gctx, int n)
{
    struct otwid *ctx = (struct otwid *)gctx;
    char *c = ctx->movenames;
    while (n > 0) {
        c += 1+strlen(c);
        n--;
    }
    return c;
}

int otwiddle3x3_movedata[] = {
    1,0,1,4,3,-1,-1,
    1,1,2,5,4,-1,-1,
    1,3,4,7,6,-1,-1,
    1,4,5,8,7,-1,-1,
    3,0,3,4,1,-1,-1,
    3,1,4,5,2,-1,-1,
    3,3,6,7,4,-1,-1,
    3,4,7,8,5,-1,-1,
};

int *otwiddle3x3_moves[] = {
    otwiddle3x3_movedata + 0 * 7,
    otwiddle3x3_movedata + 1 * 7,
    otwiddle3x3_movedata + 2 * 7,
    otwiddle3x3_movedata + 3 * 7,
    otwiddle3x3_movedata + 4 * 7,
    otwiddle3x3_movedata + 5 * 7,
    otwiddle3x3_movedata + 6 * 7,
    otwiddle3x3_movedata + 7 * 7,
};

struct otwid otwiddle3x3 = {
    {otwid_eltsize, otwid_identity, otwid_index, otwid_fromindex, otwid_maxindex,
     otwid_printelt, otwid_parseelt, otwid_moves, otwid_makemove, otwid_movename},
    3, 9,
    8, "a\0b\0c\0d\0A\0B\0C\0D", otwiddle3x3_moves
};

/* ----------------------------------------------------------------------
 * Cube. Not actually a group, but should be mappable by the same means.
 */

struct cube {
    struct group vtable;
    int w, h;
};

int cube_eltsize(struct group *gctx)
{
    struct cube *ctx = (struct cube *)gctx;
    /* cube x,y; then cube faces (lrudtb); then grid */
    return 2 + 6 + (ctx->w * ctx->h);
}

int cube_identity(struct group *gctx, void *velt, int i)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;

    if (i >= ctx->w * ctx->h)
	return 0;

    memset(elt+2, 1, 6);
    memset(elt+8, 0, ctx->w * ctx->h);
    elt[0] = i % ctx->w;
    elt[1] = i / ctx->w;

    return 1;
}

int cube_index(struct group *gctx, void *velt)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;
    int i, ret;

    ret = 0;

    for (i = 6; i-- ;)
	ret = ret * 2 + (elt[i+2] != 0);

    for (i = ctx->w * ctx->h; i-- ;)
	ret = ret * 2 + (elt[i+8] != 0);

    ret = ret * ctx->h + elt[1];
    ret = ret * ctx->w + elt[0];

    return ret;
}

void cube_fromindex(struct group *gctx, void *velt, int index)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;
    int i;

    elt[0] = index % ctx->w; index /= ctx->w;
    elt[1] = index % ctx->h; index /= ctx->h;

    for (i = 0; i < ctx->w * ctx->h; i++) {
	elt[i+8] = index & 1; index >>= 1;
    }

    for (i = 0; i < 6; i++) {
	elt[i+2] = index & 1; index >>= 1;
    }
}

int cube_maxindex(struct group *gctx)
{
    struct cube *ctx = (struct cube *)gctx;

    return (ctx->h * ctx->w) << (6 + ctx->h * ctx->w);
}

void cube_printelt(struct group *gctx, void *velt)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;
    int i, faces;

    for (i = 0; i < ctx->h * ctx->w; i += 4) {
	int j, k, digit = 0;
	for (j = 8, k = 0; j != 0 && i+k < ctx->h * ctx->w; k++, j >>= 1)
	    if (elt[8+i+k])
		digit |= j;
	putchar("0123456789ABCDEF"[digit]);
    }
    putchar(',');
    printf("%d", elt[1] * ctx->w + elt[0]);

    faces = 0;
    for (i = 0; i < 6; i++)
	if (elt[2+i])
	    faces |= 32 >> i;
    if (faces)
	printf(",%02x", faces);
}

char *cube_parseelt(struct group *gctx, void *velt, char *text)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;
    int n;

    memset(elt+2, 0, 6 + ctx->w * ctx->h);
    elt[0] = 0;
    elt[1] = 0;

    n = 0;
    while (*text && *text != ',') {
	char buf[2];
	int i, j;
	buf[0] = *text++;
	buf[1] = '\0';
	i = 0;
	sscanf(buf, "%x", &i);
	for (j = 0; j < 4; j++) {
	    int bit = (i & (8 >> j)) ? 1 : 0;
	    if (n + j < ctx->w * ctx->h)
		elt[8 + n + j] = bit;
	}
	n += 4;
    }
    if (*text) {
	int ret, pos, faces;

	text++;
	ret = sscanf(text, "%d,%x", &pos, &faces);
	if (ret >= 1) {
	    elt[0] = pos % ctx->w;
	    elt[1] = pos / ctx->w;
	}

	if (ret >= 2) {
	    int i;
	    for (i = 0; i < 6; i++)
		if (faces & (32 >> i))
		    elt[2+i] = 1;
	}
    }

    return NULL;
}

int cube_moves(struct group *gctx)
{
    return 4;
}

void cube_makemove(struct group *gctx, void *velt, int n, int out)
{
    struct cube *ctx = (struct cube *)gctx;
    unsigned char *elt = (unsigned char *)velt;
    int t;

    /* Validate. */
    switch (n) {
      case 0:			       /* L */
	if (elt[0] <= 0) return;
	break;
      case 1:			       /* R */
	if (elt[0] >= ctx->w - 1) return;
	break;
      case 2:			       /* U */
	if (elt[1] <= 0) return;
	break;
      case 3:			       /* D */
	if (elt[1] >= ctx->h - 1) return;
	break;
    }

    if (out) {
	/* Interchange _before_ moving. */
	int p = 8 + (elt[1] * ctx->w + elt[0]);

	t = elt[p];
	elt[p] = elt[7];
	elt[7] = t;
    }

    /* Roll. */
    			       /* 234567 */
    /* cube x,y; then cube faces (lrudtb); then grid */
    switch (n) {
      case 0:			       /* L */
	elt[0]--;
	t=elt[7]; elt[7]=elt[2]; elt[2]=elt[6]; elt[6]=elt[3]; elt[3]=t;
	break;
      case 1:			       /* R */
	elt[0]++;
	t=elt[7]; elt[7]=elt[3]; elt[3]=elt[6]; elt[6]=elt[2]; elt[2]=t;
	break;
      case 2:			       /* U */
	elt[1]--;
	t=elt[7]; elt[7]=elt[4]; elt[4]=elt[6]; elt[6]=elt[5]; elt[5]=t;
	break;
      case 3:			       /* D */
	elt[1]++;
	t=elt[7]; elt[7]=elt[5]; elt[5]=elt[6]; elt[6]=elt[4]; elt[4]=t;
	break;
    }

    if (!out) {
	/* Interchange _after_ moving. */
	int p = 8 + (elt[1] * ctx->w + elt[0]);
	int t;

	t = elt[p];
	elt[p] = elt[7];
	elt[7] = t;
    }
}

char *cube_movename(struct group *gctx, int n)
{
    return "L\0R\0U\0D" + 2*n;
}

struct cube cube3x3 = {
    {cube_eltsize, cube_identity, cube_index, cube_fromindex, cube_maxindex,
     cube_printelt, cube_parseelt, cube_moves, cube_makemove, cube_movename},
    3, 3
};

struct cube cube4x4 = {
    {cube_eltsize, cube_identity, cube_index, cube_fromindex, cube_maxindex,
     cube_printelt, cube_parseelt, cube_moves, cube_makemove, cube_movename},
    4, 4
};

/* ----------------------------------------------------------------------
 * Command-line processing.
 */

struct namedgroup {
    char *name;
    struct group *gp;
} groups[] = {
    {"twiddle3x3", &twiddle3x3.vtable},
    {"twiddle4x4", &twiddle4x4.vtable},
    {"otwiddle3x3", &otwiddle3x3.vtable},
    {"rtwiddle3x3", &rtwiddle3x3.vtable},
    {"cube3x3", &cube3x3.vtable},
    {"cube4x4", &cube4x4.vtable},
};

struct cmdline {
    struct group *gp;
    char *gpname;
    char *arg2;
    char **rest;
    int nrest;
    int save;
    int load;
    int special;
};

enum { SPECIAL_NONE, SPECIAL_INDEX, SPECIAL_FROMINDEX, SPECIAL_MAXINDEX,
       SPECIAL_TRYMOVES_OUT, SPECIAL_TRYMOVES_IN };

void proc_cmdline(int argc, char **argv, struct cmdline *cl)
{
    int opts = 1;
    int gotgroup = 0;

    cl->save = cl->load = 0;
    cl->special = SPECIAL_NONE;
    cl->gp = groups[0].gp;
    cl->gpname = groups[0].name;
    cl->arg2 = NULL;
    cl->rest = NULL;
    cl->nrest = 0;

    while (--argc > 0) {
        char *p = *++argv;

        if (opts && *p == '-') {
            if (!strcmp(p, "-s"))
                cl->save = 1;
            else if (!strcmp(p, "-l"))
                cl->load = 1;
            else if (!strcmp(p, "-i"))
                cl->special = SPECIAL_INDEX;
            else if (!strcmp(p, "-f"))
                cl->special = SPECIAL_FROMINDEX;
            else if (!strcmp(p, "-m"))
                cl->special = SPECIAL_MAXINDEX;
            else if (!strcmp(p, "-t"))
                cl->special = SPECIAL_TRYMOVES_IN;
            else if (!strcmp(p, "-T"))
                cl->special = SPECIAL_TRYMOVES_OUT;
            else if (!strcmp(p, "--"))
                opts = 0;
            else
                fprintf(stderr, "unrecognised option '%s'\n", p);
        } else if (!gotgroup) {
            int i;
            for (i = 0; i < lenof(groups); i++)
                if (!strcmp(p, groups[i].name)) {
                    cl->gp = groups[i].gp;
                    cl->gpname = groups[i].name;
                    break;
                }
            if (i == lenof(groups)) {
                fprintf(stderr, "unrecognised group name '%s'\n", p);
            }
	    gotgroup = 1;
        } else if (!cl->arg2) {
	    cl->arg2 = p;
	} else {
	    if (!cl->rest)
		cl->rest = (char **)malloc((argc+1) * sizeof(char *));
	    cl->rest[cl->nrest++] = p;
	}
    }
}

/* ----------------------------------------------------------------------
 * The main program, containing the meat of the group-mapping
 * algorithm.
 */

typedef unsigned char dist_t;
#define DIST_TODO 128
#define DIST_MASK (DIST_TODO - 1)
#define DIST_UNSEEN DIST_MASK

#ifdef TWO_BIT_METHOD
/*
 * If the group map is really big, we drop down to storing only two
 * bits per element. 00 means totally unseen; 01 means finished
 * with; 10 and 11 alternate between meaning `to do now' and `to do
 * next', and on each pass through the group we wipe out all
 * instances of one and convert them into the other.
 */
#define SET_STATE(index, todo, dist) do { \
    int v = (todo) + (todo) + (((dist) & 1) | !(todo)); \
    int idx = (index) >> 2; \
    int shift = 2 * ((index) & 3); \
    twobit[idx] = (twobit[idx] &~ (3 << shift)) | (v << shift); \
} while (0)
#define STATE_UNSEEN(index) ( \
    ((twobit[(index)>>2] >> (2*((index)&3))) & 3) == 0)
#define STATE_TODO(index, currdist) ( \
    ((twobit[(index)>>2] >> (2*((index)&3))) & 3) == 2 + ((currdist)&1))
#else
/*
 * At one byte per group element, we have the luxury of storing the
 * entire group map in memory at once.
 */
#define SET_STATE(index, todo, dist) do { \
    dists[index] = ((todo) * DIST_TODO) + (dist); \
} while (0)
#define STATE_UNSEEN(index) (dists[index] == DIST_UNSEEN)
#define STATE_TODO(index, currdist) (dists[index] == (DIST_TODO | (currdist)))
#endif

void update_todo(int **todos, int granularity, int ntodos, int i, int inc)
{
    int j;

    i /= granularity;
    for (j = 0; j < ntodos; j++) {
	todos[j][i] += inc;
	i >>= 1;
    }
}

int main(int argc, char **argv)
{
    struct group *gp;
#ifdef TWO_BIT_METHOD
    unsigned char *twobit;
#endif
    dist_t *dists;
    int ndists;
    int granularity;
    int ntodos, whichtodo;
    int **todos[2];
    void *elt, *elt2;
    int nmoves, eltsize, currdist, size;
    char buf[512];
    struct cmdline cl;

    proc_cmdline(argc, argv, &cl);
    gp = cl.gp;

    /*
     * Allocate space to store elements in.
     */
    eltsize = gp->eltsize(gp);
    elt = malloc(eltsize);
    elt2 = malloc(eltsize);

    ndists = gp->maxindex(gp);
    nmoves = gp->moves(gp);            /* cache to avoid recomputation */

    /*
     * Special command-line-enabled modes.
     */
    if (cl.special == SPECIAL_MAXINDEX) {
	printf("%d\n", ndists);
	return 0;
    } else if (cl.special == SPECIAL_INDEX) {
	char *err;
        if ((err = gp->parseelt(gp, elt, cl.arg2)) != NULL) {
            printf("%s: parse error: %s\n", cl.arg2, err);
	    return 1;
        }
	printf("%d\n", gp->index(gp, elt));
	return 0;
    } else if (cl.special == SPECIAL_FROMINDEX) {
	gp->fromindex(gp, elt, atoi(cl.arg2));
	gp->printelt(gp, elt);
	printf("\n");
	return 0;
    } else if (cl.special == SPECIAL_TRYMOVES_IN ||
	       cl.special == SPECIAL_TRYMOVES_OUT) {
	char *err;
	int i, j;

        if ((err = gp->parseelt(gp, elt, cl.arg2)) != NULL) {
            fprintf(stderr, "%s: parse error: %s\n", cl.arg2, err);
	    return 1;
        }
	for (i = 0; i < cl.nrest; i++) {
	    for (j = 0; j < nmoves; j++)
		if (!strcmp(gp->movename(gp, j), cl.rest[i]))
		    break;
	    if (j == nmoves) {
		fprintf(stderr, "%s: unrecognised move name\n", cl.rest[i]);
		return 1;
	    }
	    gp->makemove(gp, elt, j, cl.special == SPECIAL_TRYMOVES_OUT);
	    printf("%s -> ", cl.rest[i]);
	    gp->printelt(gp, elt);
	    printf("\n");
	}
	return 0;
    }

    /*
     * Load an existing group map rather than recreating this one,
     * if asked to.
     */
    if (cl.load) {
        int fd;
        char buf[256];

        sprintf(buf, "groupmap.%s", cl.gpname);

        /*
         * This is Unix-specific, because using mmap is just _so_
         * much more efficient.
         */
        fd = open(buf, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s: open: %s\n", buf, strerror(errno));
            return 1;
        }

        dists = mmap(NULL, ndists * sizeof(dist_t),
                     PROT_READ, MAP_SHARED, fd, 0);
        if (!dists) {
            fprintf(stderr, "%s: mmap: %s\n", buf, strerror(errno));
            return 1;
        }

    } else {

        /*
         * Set up big array for group.
         */
#ifdef TWO_BIT_METHOD
	/*
	 * The state array (whether each element is unseen, done,
	 * to do now or to do later) is stored at two bits per
	 * element. We need a separate `dists' array for the actual
	 * resulting group map (i.e. distances from start), which
	 * we'll create via mmap so that it isn't taking up main
	 * memory.
	 */
        twobit = (unsigned char *)malloc(ndists / 4);/* 2 bits per element */
        memset(twobit, 0, ndists / 4);

	/* Create a dists file. */
	{
	    FILE *fp;
	    int offset, max, fd;
	    char buf[256];
	    dist_t bigbuf[16384];

	    assert(sizeof(dist_t) == 1);   /* FIXME: memset is wrong if not */
	    memset(bigbuf, DIST_UNSEEN, 16384 * sizeof(dist_t));

	    sprintf(buf, "groupmap.%s", cl.gpname);

	    fp = fopen(buf, "wb");
	    if (!fp) {
		fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
		return 1;
	    }
	    max = ndists * sizeof(dist_t);
	    offset = 0;
	    while (offset < max) {
		int len = max - offset;
		if (len > 16384 * sizeof(dist_t))
		    len = 16384 * sizeof(dist_t);
		len = fwrite((char *)bigbuf, 1, len, fp);
		if (len < 0) {
		    fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
		    return 1;
		}
		offset += len;
	    }
	    fclose(fp);

	    fd = open(buf, O_RDWR);
	    if (fd < 0) {
		fprintf(stderr, "%s: open: %s\n", buf, strerror(errno));
		return 1;
	    }

	    dists = mmap(NULL, ndists * sizeof(dist_t),
			 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	    if (!dists) {
		fprintf(stderr, "%s: mmap: %s\n", buf, strerror(errno));
		return 1;
	    }
	}

#else
        dists = (dist_t *)malloc(ndists * sizeof(dist_t));
        assert(sizeof(dist_t) == 1);   /* FIXME: memset is wrong if not */
        memset(dists, DIST_UNSEEN, ndists * sizeof(dist_t));
#endif

        /*
         * Set up a bunch of smaller arrays of int, counting number of
         * todo elements in subregions of the array. We'll need two of
         * these, since we switch back and forth every time we finish
         * processing the elements at a given distance.
         *
         * We'd like to use not more than 1/4 of the space taken up by
         * the dists/twobit array itself. This means that:
         *
         *  - one complete set of index arrays comes to about twice
         *    the size of the finest-grained one (sum of powers of
         *    two)
         *
         *  - thus two of them come to four times that size
         *
         *  - so we want 4 * sizeof(int) * length / granularity to
         *    be at most one quarter of length * sizeof(dist_t).
         */
#ifdef TWO_BIT_METHOD
        granularity = 16 * sizeof(int) * 4;
#else
        granularity = 16 * sizeof(int) / sizeof(dist_t);
#endif
        assert((granularity & (granularity-1)) == 0);  /* must be power of 2 */
        ntodos = 1;
        while ((granularity << (ntodos-1)) < ndists)
            ntodos++;

        for (whichtodo = 0; whichtodo < 2; whichtodo++) {
            int i;

            todos[whichtodo] = (int **)malloc(ntodos * sizeof(int *));

            for (i = 0; i < ntodos; i++) {
                int g = granularity << i;
                int size = (ndists + g - 1) / g;
                todos[whichtodo][i] = (int *)malloc(size * sizeof(int));
                memset(todos[whichtodo][i], 0, size * sizeof(int));
            }
        }

        /*
         * Find the initial element(s) and add them to the array
         * marked as todo.
         */
	{
	    int n = 0;

	    while (gp->identity(gp, elt, n)) {
		int i = gp->index(gp, elt);
		SET_STATE(i, 1, 0);
		update_todo(todos[0], granularity, ntodos, i, +1);
		n++;
	    }
	    whichtodo = 0;

	    size = n;
	}

        currdist = 0;

        while (todos[whichtodo][ntodos-1][0] > 0) {
            printf("%d elements at distance %d\n", todos[whichtodo][ntodos-1][0],
                   currdist);

            /*
             * Process a distance level by emptying one todo tree into
             * the other.
             */
            while (todos[whichtodo][ntodos-1][0] > 0) {
                /*
                 * Search down the todo tree to find the next element
                 * that needs processing.
                 */
                int i, j;

                i = 0;
                for (j = ntodos-2; j >= 0; j--) {
                    i *= 2;
                    if (todos[whichtodo][j][i] == 0)
                        i++;
                }
                assert(todos[whichtodo][0][i] > 0);

                /*
                 * Now i is the index of the lowest non-empty entry in
                 * the finest-grained todo table. Search the actual
                 * distance array to find the element itself.
                 */
                j = i * granularity;
                for (i = j; i < j + granularity && i < ndists; i++)
                    if (STATE_TODO(i, currdist))
                        break;
                assert(i < j + granularity && i < ndists);

                gp->fromindex(gp, elt, i);
#ifdef DEBUG_SEARCH
                printf("todo=%d; processing element ", todos[whichtodo][ntodos-1][0]);
                gp->printelt(gp, elt);
                printf(" (%d)\n", i);
#endif

                /*
                 * Process the element by applying every possible move
                 * to it.
                 */
                for (j = 0; j < nmoves; j++) {
                    int newi;
                    memcpy(elt2, elt, eltsize);
                    gp->makemove(gp, elt2, j, 1);
                    newi = gp->index(gp, elt2);

                    if (STATE_UNSEEN(newi)) {
                        SET_STATE(newi, 1, currdist+1);
                        update_todo(todos[whichtodo ^ 1], granularity, ntodos,
                                    newi, +1);
                        size++;
#ifdef DEBUG_SEARCH
                        printf("found element at dist %d: ", currdist+1);
                        gp->printelt(gp, elt2);
                        printf(" (%d)\n", newi);
#endif
                    }
                }

                /*
                 * Mark this element as done.
                 */
                SET_STATE(i, 0, currdist);
                update_todo(todos[whichtodo], granularity, ntodos, i, -1);
#ifdef TWO_BIT_METHOD
		/*
		 * This is also where we set each element in the
		 * real dists file.
		 */
		dists[i] = currdist;
#endif
            }

            /*
             * Now switch todo lists.
             */
            assert(todos[whichtodo][ntodos-1][0] == 0);
            whichtodo ^= 1;
            currdist++;
        }

        /*
         * The group is mapped. Report its total size.
         */
        printf("Total group size is %d\n", size);
    }

#ifdef TWO_BIT_METHOD
    /*
     * In this mode, cl.save simply means we exit now, since we've
     * already written out everything we wanted to disk.
     */
    if (cl.save)
	return 0;
#else
    /*
     * If we were asked to on the command line, save a dump of the
     * group map rather than entering interactive mode.
     */
    if (cl.save) {
        FILE *fp;
        int offset, max;
        char buf[256];

        sprintf(buf, "groupmap.%s", cl.gpname);

        fp = fopen(buf, "wb");
        if (!fp) {
            fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
            return 1;
        }
        max = ndists * sizeof(dist_t);
        offset = 0;
        while (offset < max) {
            int len = max - offset;
            if (len > 16384)
                len = 16384;
            len = fwrite((char *)dists + offset, 1, len, fp);
            if (len < 0) {
                fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
                return 1;
            }
            offset += len;
        }
        fclose(fp);

        return 0;
    }
#endif

    while (fgets(buf, sizeof(buf), stdin)) {
        char *err;

        buf[strcspn(buf, "\r\n")] = '\0';

        if ((err = gp->parseelt(gp, elt, buf)) != NULL) {
            printf("parse error: %s\n", err);
            continue;
        }

        /*
         * We have an element description. Report a shortest
         * sequence of moves required to `solve' it down to the
         * identity.
         */

        while (1) {
            int dist, j;

            /*
             * Find the current element distance. If it's 0, we're
             * at the identity and have finished.
             */
            dist = dists[gp->index(gp, elt)];
            if (dist == 0)
                break;
            if (dist == DIST_UNSEEN) {
                printf("position unreachable\n");
                break;
            }

            /*
             * Now try each possible move from here, until we find
             * one which reduces the distance.
             */
	    for (j = 0; j < nmoves; j++) {
		int newi;

		memcpy(elt2, elt, eltsize);
		gp->makemove(gp, elt2, j, 0);
		newi = gp->index(gp, elt2);
		if (dists[newi] < dist)
                    break;
            }

            assert(j < nmoves);

            printf("Move %s -> ", gp->movename(gp, j));
            gp->printelt(gp, elt2);
            printf("\n");

            memcpy(elt, elt2, eltsize);
        }
    }

    return 0;
}
