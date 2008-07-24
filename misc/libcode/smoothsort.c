/*
 * smoothsort.c: Implementation of Edsger Dijkstra's "smoothsort".
 * 
 * Currently not even slightly working. It runs, and it even
 * terminates, but is the output list remotely sorted? Is it
 * buggery. Some rigorous invariant-testing at every stage of the
 * algorithm required, methinks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#ifndef TESTMODE
#define NDEBUG
#else
static void check(int phase, size_t heapoid_size, size_t *mask,
		  size_t b, size_t c);
#endif

/*
 * If WITH_CTX is not defined, this file will create a
 * smoothsort() function with a prototype identical to the
 * standard C library qsort(). If it is defined, then the
 * comparison function will accept an additional void * parameter,
 * and smoothsort() itself will be given that parameter to pass in
 * to all compare calls it makes.
 */
#ifdef WITH_CTX
#define CTXPARAM , void *ctx
#define CTXARG , ctx
#else
#define CTXPARAM
#define CTXARG
#endif

typedef int (*cmpfn)(const void *a, const void *b CTXPARAM);

#define COMPARE(i, j) compare(((char *)base)+(i)*(size), \
			      ((char *)base)+(j)*(size) CTXARG)
#define SWAP(i, j) memswap(((char *)base)+(i)*(size), \
			   ((char *)base)+(j)*(size), size)

static void memswap(void *av, void *bv, size_t size)
{
    char tmp[4096];
    char *a = (char *)av, *b = (char *)bv;

    while (size > 0) {
	size_t thissize = size > sizeof(tmp) ? sizeof(tmp) : size;
	memcpy(tmp, a, thissize);
	memcpy(a, b, thissize);
	memcpy(b, tmp, thissize);
	a += thissize;
	b += thissize;
	size -= thissize;
    }
}

/*
 * mask[0] and mask[1] represent the bit mask of stretch sizes
 * we're using. It has to be larger than a size_t, because
 * Leonardo numbers grow less fast than powers of two. The obvious
 * thing would be to store it as two words end to end, but this
 * has portability problems (fiddly to reliably determine the
 * maximum number of bits you can fit in a size_t), and actually
 * the alternative is faster anyway: _interleave_ the bits, so
 * that mask[0] contains all the even-numbered bits of the logical
 * mask and mask[1] all the odd-numbered bits. Then a one-bit
 * shift left or right consists of swapping the two words and
 * shifting only one of them, which is much easier than shifting
 * both and fiddling about with the carried-over bit.
 */

static void up(size_t *mask, size_t *b, size_t *c)
{
    size_t tmp;

    if (mask) {
	assert(!(mask[0] & 1));
	tmp = mask[0] >> 1;
	mask[0] = mask[1];
	mask[1] = tmp;
    }

    tmp = *b;
    *b += *c + 1;
    *c = tmp;
}

static void down(size_t *mask, size_t *b, size_t *c)
{
    size_t tmp;

    if (mask) {
	tmp = mask[1] << 1;
	mask[1] = mask[0];
	mask[0] = tmp;
    }

    tmp = *c;
    *c = *b - *c - 1;
    *b = tmp;
}

static void sift(void *base, size_t size, cmpfn compare CTXPARAM,
		 size_t root, size_t b, size_t c)
{
    while (b > 1) {
	/*
	 * Find the larger child, and see if it needs to be
	 * exchanged with the root.
	 */
	down(NULL, &b, &c);
	if (COMPARE(root-1, root-c-1) > 0) {
	    if (COMPARE(root, root-1) >= 0)
		return;
	    SWAP(root, root-1);
	    down(NULL, &b, &c);
	    root--;
	} else {
	    if (COMPARE(root, root-c-1) >= 0)
		return;
	    SWAP(root, root-c-1);
	    root -= c+1;
	}
    }
}

static void trinkle(void *base, size_t size, cmpfn compare CTXPARAM,
		    size_t root, size_t *inmask, size_t b, size_t c, int full)
{
    size_t mask[2];

    /*
     * Take a local copy of the mask, so we can fiddle with it.
     */
    mask[0] = inmask[0];
    mask[1] = inmask[1];

    while (1) {
	size_t biggest = root;
	int action = 0;

	/*
	 * Compare against the two children of the current
	 * stretch, if it's got any.
	 */
	if (b > 1 && full) {
	    if (COMPARE(root-1, biggest) > 0) {
		biggest = root-1;
		action = 1;
	    }
	    if (COMPARE(root-b+c, biggest) > 0) {
		biggest = root-b+c;
		action = 2;
	    }
	}

	/*
	 * Compare against the root of the previous stretch, if
	 * there is one.
	 */
	if (mask[0] > 1 || mask[1] > 0) {
	    if (COMPARE(root-b, biggest) > 0) {
		biggest = root-b;
		action = 3;
	    }
	}

	/*
	 * Now choose what to do.
	 */
	switch (action) {
	  case 1:		       /* second substretch of this one */
	    SWAP(root, root-1);
	    root = root-1;
	    down(NULL, &b, &c);
	    down(NULL, &b, &c);
	    sift(base, size, compare CTXARG, root, b, c);
	    return;
	  case 2:		       /* first substretch of this one */
	    SWAP(root, root-b+c);
	    root = root-b+c;
	    down(NULL, &b, &c);
	    sift(base, size, compare CTXARG, root, b, c);
	    return;
	  case 3:		       /* previous stretch */
	    SWAP(root, root-b);
	    root = root-b;
	    mask[0] ^= 1;
	    do {
		up(mask, &b, &c);
	    } while (!(mask[0] & 1));
	    full = 1;
	    continue;
	  default /* case 0 */:        /* already the biggest, we're done */
	    return;
	}
    }
}

void smoothsort(void *base, size_t nmemb, size_t size, cmpfn compare CTXPARAM)
{
    int heapoid_size;
    size_t mask[2], b, c;

    /*
     * Initialise the heapoid so that it contains a singleton
     * element. (Allowing it ever to be zero-sized would require
     * irritating and unnecessary special-case handling.)
     */
    mask[0] = 1;
    mask[1] = 0;
    b = c = 1;
    heapoid_size = 1;

    /*
     * Phase 1: build the heapoid.
     */
    while (heapoid_size < nmemb) {
	int merge = mask[0] & mask[1] & 1;

	/*
	 * Heapoid invariant restoration. If we're merging two
	 * stretches, or if the current last stretch is not going
	 * to be part of the heapoid's final configuration, then
	 * we can merely sift. Otherwise, we must trinkle.
	 */
	if (merge || heapoid_size + c + 1 <= nmemb)
	    sift(base, size, compare CTXARG, heapoid_size - 1, b, c);
	else
	    trinkle(base, size, compare CTXARG,
		    heapoid_size - 1, mask, b, c, 1);

	/*
	 * Now we can add our new element to the heapoid.
	 */
	heapoid_size++;
	if (merge) {
	    mask[0] ^= 3;
	    mask[1] ^= 1;
	    up(mask, &b, &c);
	    up(mask, &b, &c);
	} else {
	    do {
		down(mask, &b, &c);
	    } while (b > 1);
	    mask[0] ^= 1;
	}

#ifdef TESTMODE
	check(1, heapoid_size, mask, b, c);
#endif
    }

    /*
     * Phase 1.5: switch over to the phase-2 invariants.
     */
    trinkle(base, size, compare CTXARG,
	    heapoid_size-1, mask, b, c, 1);

#ifdef TESTMODE
    check(2, heapoid_size, mask, b, c);
#endif

    /*
     * Phase 2: dismantle the heapoid.
     */
    while (heapoid_size > 1) {
	heapoid_size--;
	if (b > 1) {
	    /*
	     * Split up a stretch into two substretches. Heapoid
	     * invariant restoration: we must (semi)trinkle the
	     * roots of the resulting two substretches back into
	     * place, which it's easiest (from the point of view
	     * of having mask[] looking sensible for the trinkle
	     * function) to do as we go along.
	     */
	    mask[0] ^= 1;
	    down(mask, &b, &c);
	    mask[0] ^= 1;
	    trinkle(base, size, compare CTXARG,
		    heapoid_size - c - 1, mask, b, c, 0);
	    down(mask, &b, &c);
	    mask[0] ^= 1;
	    trinkle(base, size, compare CTXARG,
		    heapoid_size - 1, mask, b, c, 0);
	} else {
	    /*
	     * Remove a singleton from the end of the heapoid.
	     * Invariants are naturally preserved by this.
	     */
	    mask[0] ^= 1;
	    do {
		up(mask, &b, &c);
	    } while (!(mask[0] & 1));
	}
#ifdef TESTMODE
	check(2, heapoid_size, mask, b, c);
#endif
    }
}

#ifdef TESTMODE

/*
 * gcc -g -O0 -DTESTMODE -o smoothsort smoothsort.c
 */

#define NTEST 123

static int testarray[NTEST];

static int compare(const void *av, const void *bv)
{
    const int *a = (const int *)av;
    const int *b = (const int *)bv;
    return *a < *b ? -1 : *a > *b ? +1 : 0;
}

static void checkstretch(size_t root, size_t b, size_t c, int dubious)
{
    if (b > 1) {
	size_t aroot = root - b + c, broot = root - 1;
	if (!dubious) {
	    assert(testarray[root] >= testarray[aroot]);
	    assert(testarray[root] >= testarray[broot]);
	}
	down(NULL, &b, &c);
	checkstretch(aroot, b, c, 0);
	down(NULL, &b, &c);
	checkstretch(broot, b, c, 0);
    }
}

static void check(int phase, size_t heapoid_size, size_t *inmask,
		  size_t b, size_t c)
{
    size_t mask[2];
    int bs[64], cs[64];
    int nsizes = 0;
    int first = 1;

    mask[0] = inmask[0];
    mask[1] = inmask[1];

    /*
     * Check that every stretch is correctly structured
     * internally, and preserve their sizes on a list.
     */
    while (mask[0] || mask[1]) {
	assert(heapoid_size >= b);
	if (mask[0] & 1) {
	    checkstretch(heapoid_size - 1, b, c, phase==1 && first);
	    mask[0] ^= 1;
	    first = 0;
	    bs[nsizes] = b;
	    cs[nsizes] = c;
	    nsizes++;
	    heapoid_size -= b;
	}
	up(mask, &b, &c);
    }
    assert(heapoid_size == 0);

    /*
     * Now check that the roots of the stretches are correctly
     * ordered.
     */
    while (nsizes > 0) {
	nsizes--;
	b = bs[nsizes];
	c = cs[nsizes];
	if (phase == 1 && (heapoid_size + b+c+1 <= NTEST || nsizes==0))
	    break;		       /* finished initial sorted segment */
	if (heapoid_size > 0)
	    assert(testarray[heapoid_size+b-1] >= testarray[heapoid_size-1]);
	heapoid_size += b;
    }
}

int main(void)
{
    int i;

    srand(1234);

    for (i = 0; i < NTEST; i++)
	testarray[i] = i;
    for (i = NTEST; i-- > 1 ;) {
	int j = rand() % (i+1);
	int t = testarray[i];
	testarray[i] = testarray[j];
	testarray[j] = t;
    }
    for (i = 0; i < NTEST; i++)
	printf("b %3d: %3d\n", i, testarray[i]);
    smoothsort(testarray, NTEST, sizeof(*testarray), compare);
    for (i = 0; i < NTEST; i++)
	printf("a %3d: %3d\n", i, testarray[i]);
    return 0;
}

#endif
