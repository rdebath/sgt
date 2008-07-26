/*
 * smoothsort.c: Implementation of Edsger Dijkstra's "smoothsort".
 */

/*
 * First, an explanation of the algorithm. Dijkstra's own
 * explanation is not very clear (in my opinion), so I'll write my
 * own.
 * 
 * Smoothsort is structurally similar to heapsort. It involves
 * building a heap-like structure (which I'm going to refer to
 * throughout as a "heapoid") in the array, by starting it off at
 * the bottom of the array and assimilating the array elements
 * into it one by one until it fills the whole array. Then in the
 * second phase, the heapoid is dismantled one element at a time,
 * in such a way that the element extracted at every step is the
 * largest and so the array is left in sorted order once the
 * heapoid has shrunk to nothingness.
 *
 * The difference between smoothsort and heapsort is that
 * smoothsort's heapoid is completely different from heapsort's
 * binary heap. In smoothsort, the heapoid is composed of
 * sub-blocks with sizes based on Fibonacci numbers, each with a
 * similar internal substructure (giving the tree-like nature
 * required of a heapoid), and is in general arranged with the
 * largest numbers at the _top_ rather than the bottom.
 *
 * In particular, this means that smoothsort only ever exchanges a
 * pair of elements when doing so brings them into their correct
 * order if they weren't already (unlike heapsort, which starts by
 * deliberately seeking out the array's largest element and
 * putting it at the _bottom_). This property is what gives
 * smoothsort its good performance on already sorted or
 * nearly-sorted lists.
 *
 * The structure of the heapoid
 * ----------------------------
 * 
 * I said above that the heapoid was composed of sub-blocks
 * (Dijkstra calls them "stretches") whose sizes were "based on"
 * Fibonacci numbers. In fact, they're what Dijkstra calls
 * "Leonardo numbers", which have the property that each one is
 * equal to the sum of the previous two _plus one_. So they go 1,
 * 1, 3, 5, 9, 15, 25, 41, ... (They're closely related to
 * Fibonacci numbers, though: if you take the Fibonacci sequence,
 * multiply each number in it by two and subtract one, you end up
 * with the Leonardo numbers.)
 *
 * In general, the heapoid consists of a series of stretches, each
 * of which has length equal to some Leonardo number. The
 * stretches are arranged in descending order of size, and all
 * their sizes are distinct, with the sole exception that there
 * are allowed to be two stretches of size 1 (because the number 1
 * occurs twice in the Leonardo sequence shown above). Each
 * non-trivial stretch (i.e. larger than one element) is
 * considered to consist of a root element at the right, and below
 * that two sub-stretches with sizes equal to the previous two
 * Leonardo numbers. So a 9-stretch, for instance, could be broken
 * up into a 5-stretch followed by a 3-stretch followed by a root
 * element.
 *
 * To add a new element to the end of the heapoid during the
 * building phase, we do one of two things. If the last (shortest)
 * two stretches already in the heapoid have lengths equal to
 * _adjacent_ Leonardo numbers in the sequence, then we can
 * combine the two with our new element to form a stretch of the
 * next size up. If that isn't the case, then we simply append our
 * new number to the end as a stretch of size 1. (Dijkstra proves,
 * so you don't have to, that this always works and that the first
 * case never ends up forming a second stretch of a size you've
 * already got.)
 *
 * To remove an element from the heapoid during the dismantling
 * phase, we do the inverse of those two cases: if the last
 * stretch currently in the heapoid is of size 1 then we simply
 * remove it, and otherwise we break up the smallest stretch into
 * its two substretches and take off the root element.
 *
 * The ordering constraints on the heapoid
 * ---------------------------------------
 *
 * When the heapoid is fully built, it satisfies two ordering
 * constraints. Firstly, the root element of each stretch is the
 * largest element in it, and the same applies to its substretches
 * and their substretches and so on. In other words, each stretch
 * can be considered as a (slightly unbalanced) binary tree which
 * has the _heap property_ (that no child is larger than its
 * parent) throughout. Secondly, the root elements of all the
 * (top-level) stretches are in increasing order; so in
 * particular, the last element in the heapoid (i.e. the root of
 * the final stretch) is always the largest element in the entire
 * structure.
 *
 * These ordering constraints are preserved throughout the
 * dismantling phase of the sort. However, during the build phase,
 * slightly weaker conditions are maintained in order to save some
 * unnecessary work. Specifically, the last stretch in the heapoid
 * is not required to have its root be the largest thing in it
 * (though its substretches, if any, must be properly ordered).
 * Also, not all the stretches are required to have their roots in
 * increasing order: instead, only those stretches which are as
 * big as they can get without overflowing the array must be
 * correctly ordered (since any other stretch will eventually end
 * up being combined with something else, so ordering its root
 * would just be a waste of time that would only have to be
 * redone). This is always some initial subsequence of the
 * stretches.
 *
 * These ordering constraints are maintained by two procedures
 * which look a lot like the maintenance functions for a binary
 * heap.
 *
 * The first procedure, which Dijkstra calls "sift", starts with a
 * potentially misordered element at the root of some stretch, and
 * moves it down into the stretch until it's correctly positioned.
 * It looks very like the corresponding operation on a binary
 * heap: you see if the element is larger than both its children,
 * and if not you swap it with its larger child and (tail-)recurse
 * into that substretch. (In fact, the sift operation is actually
 * slightly _simpler_ than the binary heap version. In a binary
 * heap you have to worry about the special case where a parent
 * element has only one child; in smoothsort's heapoid every
 * element has either two children or none.)
 *
 * The second procedure, which Dijkstra refers to by the exciting
 * non-word "trinkle", does a similar thing but also adds the
 * stretch to the subsequence whose roots are in increasing order.
 * It actually looks very similar in concept: it just considers
 * the root element of each stretch to have (up to) _three_
 * children, which consist of its obvious two children inside its
 * own stretch and also the root of the previous stretch. All of
 * these three children want to be smaller than the root element;
 * so we find the largest of those children, swap it with the root
 * if necessary, and (tail-)recurse to there. If we recursed to
 * the root of the previous stretch, then we are still trinkling
 * and must do another three-way compare in the next iteration; if
 * we descended into a substretch then we have finished doing
 * three-way compares and can revert to a simple sift.
 *
 * We use these procedures as follows:
 *
 * At the start of each iteration of the build phase, our last
 * stretch has its root element potentially out of order. We're
 * either going to add a new stretch after it or turn it into a
 * substretch of something, so we must certainly fix this by
 * either doing a sift or a trinkle starting from its root. If
 * we're subsuming it into a larger stretch, we need only sift; if
 * there's room to _later_ subsume it into a larger stretch, we
 * again need only sift; but if we're adding a new 1-stretch after
 * it and it's of the maximum size that will fit, then we must
 * trinkle. (This is why we allowed ourselves to leave its root
 * element unordered at the end of the previous build-phase
 * iteration: by waiting until we know whether we're merging the
 * stretch or not, we can avoid doing an unnecessary trinkle when
 * a sift would have been sufficient.)
 *
 * At the end of the build phase, our very last stretch has its
 * root element potentially wrong, so we must trinkle that.
 *
 * During the dismantle phase, the case where we remove a
 * 1-stretch from the end of the heapoid is harmless and cannot
 * destroy the ordering properties. The only hard case is the one
 * where we break up a larger stretch into two substretches and a
 * root element which we discard. In this case, the root elements
 * of the two exposed substretches will not necessarily be in the
 * right order compared to the roots of all the other stretches;
 * so we must trinkle them both, starting with the larger (lower)
 * one. (Well, actually, we can do a slightly optimised trinkle
 * which Dijkstra calls a "semitrinkle": because each of those
 * substretches was correctly ordered within itself, the first
 * iteration of the trinkle can be simplified by not bothering to
 * compare the root element with its two internal children, since
 * we already know it's larger.)
 *
 * That's it!
 * ----------
 *
 * That's the conceptual structure of the smoothsort algorithm. If
 * you want to try reading Dijkstra's original description, then
 * (at the time of writing) it's available at
 *
 *   http://www.cs.utexas.edu/users/EWD/transcriptions/EWD07xx/EWD796a.html
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

#ifdef DIAGRAM
static void write_diagram(void);
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

#ifdef DIAGRAM
    write_diagram();
#endif

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
#ifdef DIAGRAM
	write_diagram();
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
#ifdef DIAGRAM
    write_diagram();
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
#ifdef DIAGRAM
	write_diagram();
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

#ifdef DIAGRAM

/*
gcc -g -O0 -DDIAGRAM -o smoothsort smoothsort.c && ./smoothsort && \
    multi convert s/png/gif/ smoothsort0*.png && \
    gifsicle -d 1 smoothsort0*.gif > smoothsort.gif
 */

#define NTEST 123

static int testarray[NTEST];

static int compare(const void *av, const void *bv)
{
    const int *a = (const int *)av;
    const int *b = (const int *)bv;
    return *a < *b ? -1 : *a > *b ? +1 : 0;
}

static void write_diagram(void)
{
    char command[256];
    static int frame = 0;
    int yp, y, xp, x;
    const int grid = 3;
    FILE *fp;

    sprintf(command, "convert -depth 8 -size %dx%d gray:- smoothsort%04d.png",
	    NTEST*grid, NTEST*grid, frame++);
    fp = popen(command, "w");
    for (yp = 0; yp < NTEST*grid; yp++) {
	y = NTEST-1 - (yp / grid);
	for (xp = 0; xp < NTEST*grid; xp++) {
	    x = xp / grid;

	    if (testarray[x] == y)
		fputc(0, fp);
	    else
		fputc(255, fp);
	}
    }
    pclose(fp);
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
    smoothsort(testarray, NTEST, sizeof(*testarray), compare);
    return 0;
}

#endif
