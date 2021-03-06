/*
 * amergesort.c: Mergesort in-place on an array, sorting stably in
 * guaranteed O(N log N) time.
 */

/*
 * We employ three completely different merging strategies, all of
 * which work in place and are stable.
 *
 * The first and simplest algorithm is taken from this website:
 *
 *   http://thomas.baudel.name/Visualisation/VisuTri/inplacestablesort.html
 *
 * I don't know where (if anywhere) it's mentioned in the academic
 * literature. It's a recursive algorithm: given two lists end to
 * end in an array, we divide the larger one exactly in half, then
 * binary-search the other one to find the corresponding split
 * point (paying careful attention to stability, of course). We
 * then block-exchange the second half of the first list with the
 * first half of the second, leaving us two smaller lists which we
 * recurse into in the obvious way. The downside of this is that
 * it isn't actually a linear-time algorithm: it recurses to depth
 * O(log N) (even in the worst case of the second split point
 * being at one extreme end of the list, every recursive step must
 * reduce the list size to at most 3/4 of its original value) and
 * all of the processing at any given level takes O(N) time in
 * total, and thus a single merge is O(N log N) and a sort built
 * entirely out of them would be O(N log^2 N).
 *
 * However, on smallish lists this crude algorithm outperforms the
 * more complex but asymptotically optimal ones in the academic
 * literature. Hence, we use the above algorithm on smallish
 * lists, but once the list size goes past a certain threshold, we
 * switch over to a pair of genuinely linear-time stable merging
 * algorithms. This gives the algorithm as a whole O(N log N) time
 * complexity in the asymptotic limit (and hence theoretically
 * optimal), while also being as fast as I can make it in
 * practice.
 *
 * Both of these algorithms are taken from Antonios Symvonis's
 * paper "Optimal Stable Merging":
 *
 *   A. Symvonis. Optimal stable merging. The Computer Journal,
 *   38(8):681-690, 1995.
 *   http://citeseer.ist.psu.edu/78814.html
 *
 * They are described in sections 5 and 6 of the paper.
 *
 * Both algorithms require a buffer of distinct elements to be
 * separated from the main array as a preliminary step. This is
 * used as temporary space in various ways, and is then merged
 * back into the main array at the end of the procedure. (It's
 * fairly easy to stably merge two lists in linear time if the
 * length of one is of the order of the square root of that of the
 * other; see the "distribute" subroutines below for details.)
 *
 * The section 6 approach requires a buffer of about 3*sqrt(N)
 * distinct keys. We divide the rest of the array into blocks of
 * size around sqrt(N), taking care to have the boundary between
 * the two input lists fall exactly on a block boundary. We
 * reserve a piece of buffer space the size of two blocks (the
 * merge buffer) plus a piece with one element for every block
 * (the tracking buffer). We then merge the two input lists in the
 * obvious way, by iterating along both of them in parallel and
 * comparing elements as we go. The output of this merging is
 * written into the merge buffer (by swapping each merged element
 * into that buffer once we know which it is). Before the merge
 * buffer becomes completely full, we are guaranteed to have
 * emptied (i.e. filled with merge-buffer elements) an entire
 * block from one input list or the other; we then swap that
 * entire block with the full block from the merge buffer, and
 * then continue merging, treating the merge buffer as circular.
 *
 * When this procedure terminates, we have our merged list
 * occupying exactly the same space as the original list - except
 * that its blocks are permuted in some apparently unhelpful
 * order. This is where the tracking buffer comes in: the first
 * time we swap a completed block out of the merge buffer, we find
 * the smallest element in the tracking buffer and swap it into
 * the location corresponding to where we put the first completed
 * block. Next time, we find the next smallest element, by
 * searching only those locations we haven't yet nailed down
 * (which is easy to track, since the block slots we _have_ used
 * up are always in two contiguous subsequences starting from the
 * beginning of each input sublist). So we end up with the
 * (distinct) elements in the tracking buffer being arranged in
 * the same order as the blocks of the output list; now we simply
 * sort the tracking buffer, mirroring every swap as a swap of
 * blocks of the output list. Done!
 *
 * This is beautifully simple, but it only works if we can get a
 * buffer of 3*sqrt(N) distinct elements out to begin with. If
 * there aren't enough distinct elements in the array, we fall
 * back to the approach in section 5.
 *
 * The section 5 approach _also_ begins by dividing array into
 * blocks of equal size with the boundary between the two input
 * lists falling a block boundary. In this case, however, we make
 * the blocks as large as necessary to arrange that the total
 * number of blocks is at most the available size of the buffer.
 * We then sort those blocks by their first element - but we take
 * care to do so stably, again by moving the buffer elements about
 * in a way that mirrors the movement of array blocks, so we can
 * always remember where every block started out and use that to
 * break ties in comparison. Having done that, we now only have
 * local sections of the array that are out of place, so we go
 * through and find them and use block exchanges to put them
 * right. This only manages to work in linear time because there
 * is at most one block exchange per distinct element (see
 * Symvonis for a more detailed proof), so it relies for its
 * efficiency on there being fewer than O(sqrt(N)) distinct
 * elements in the first place. Fortunately, that's precisely the
 * case in which we _need_ it to be efficient, so that it can take
 * over where section 6 didn't work!
 *
 * Both of these algorithms, when they break up the input list
 * into blocks, can leave a partial block at the start of the
 * first list and the end of the second. We then distribute those
 * stably into the list at the end, which is a linear-time
 * operation as long as we do it carefully. (The fiddly case is
 * after a section-5 merge, in which the partial blocks' size need
 * not be bounded by O(sqrt(N)) and so we can't afford to do a
 * binary search per element - we must be sure to only do a binary
 * search per _distinct_ element.)
 *
 * Symvonis's paper, incidentally, also presents a more complex
 * merging algorithm which is "optimal" in the sense that its
 * required number of comparisons is O(m log (n/m+1)) rather than
 * O(m+n) (where m is the length of the shorter list). In other
 * words, its job is to do especially well in the case where one
 * list is much shorter than the other: clearly you want in that
 * situation to be able to get away with a number of comparisons
 * suited to a few binary searches of the longer list rather than
 * going linearly through the whole thing. This doesn't concern us
 * in a mergesort, however: for some array sizes the occasional
 * sub-merge _will_ be wildly unbalanced, but all the other
 * sub-merges will be balanced so doing that occasional merge
 * better won't affect the complexity of the overall sort. So we
 * stick to the simpler approaches throughout.
 *
 * Finally, I make one improvement of my own to Symvonis's
 * algorithms. Symvonis was solving the problem of a single merge
 * operation, so he incorporated the operations of extracting and
 * remerging his buffer into the linear-time merge operation.
 * However, a full merge sort consists of _lots_ of merges, and it
 * would seem silly to go to the effort of individual buffer
 * extraction for each of them. Instead, therefore, I extract a
 * single large buffer at the very start of the algorithm, and
 * remerge it once at the end, having used it for all the merge
 * passes both small and large. This saves considerable effort,
 * and brings the performance of the algorithm as a whole to an at
 * least _reasonably_ practical level: my benchmarks based on
 * counting comparisons and swaps suggest that it's between 1.5
 * and 3 times as slow as Heapsort, depending on how many distinct
 * keys you have and which of comparisons and swaps is more
 * expensive for you. Given the insanely complex appearance of
 * most of the algorithms in the literature, I think that's not
 * bad at all in practical terms as a price for stability!
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

/*
 * If WITH_CTX is not defined, this file will create an
 * amergesort() function with a prototype identical to the
 * standard C library qsort(). If it is defined, then the
 * comparison function will accept an additional void * parameter,
 * and amergesort() itself will be given that parameter to pass in
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

#ifdef TESTMODE
static int swaps, compares;
#endif

#define COMPARE(i, j) compare(((char *)base)+(i)*(size), \
			      ((char *)base)+(j)*(size) CTXARG)
#define REALSWAP(i, j, n) memswap(((char *)base)+(i)*(size), \
			          ((char *)base)+(j)*(size), (n)*(size))
#ifdef TESTMODE
#define SWAP(i, j) ( swaps++, REALSWAP(i, j, 1) )
#define SWAPN(i, j, n) ( swaps += (n), REALSWAP(i, j, n) )
#else
#define SWAP(i, j) ( REALSWAP(i, j, 1) )
#define SWAPN(i, j, n) ( REALSWAP(i, j, n) )
#endif

#ifdef TESTMODE
static void subseq_should_be_sorted(size_t start, size_t len);
#endif

/*
 * Tunable parameters. We do simple insertion sort for very small
 * sublists before beginning our merge passes, so we need a
 * threshold size for that; then we have a second threshold
 * further up where we switch over from crude recursive merging to
 * Symvonis's optimal algorithms.
 *
 * Both of these parameters were determined empirically by setting
 * them at various levels and then benchmarking the sort.
 *
 * The recursion threshold shown strikes a reasonable balance
 * between good performance with lots of distinct elements and
 * with few. Setting it higher trades off for fewer comparisons at
 * the expense of more swaps, at the lots-of-distinct-elements end
 * of the scale.
 */
#ifndef INSERTION_THRESHOLD
#define INSERTION_THRESHOLD 8
#endif
#ifndef RECURSION_THRESHOLD
#define RECURSION_THRESHOLD 128
#endif

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
 * Square root of an unsigned integer. Rounds up.
 */
static size_t squareroot(size_t n) {
    unsigned long d, a, b, di;

    d = n;
    a = 0;
    b = 1;
    while ((n >> 2) >= b)
	b <<= 2;
    do {
        a >>= 1;
        di = 2*a + b;
        if (di <= d) {
            d -= di;
            a += b;
        }
        b >>= 2;
    } while (b);

    if (d)
	a++;			       /* round up */

    return a;
}

/*
 * Exchange two blocks of elements, one starting at "start" of
 * size m, and one straight after that of size n. We use the
 * double-reverse technique, for its good cache locality.
 */
static void block_exchange(void *base, size_t size,
			   size_t start, size_t m, size_t n)
{
    size_t i, j;

    if (m == 0 || n == 0)
	return;
    for (i = start, j = start+m-1; i < j; i++, j--)
	SWAP(i, j);
    for (i = start+m, j = start+m+n-1; i < j; i++, j--)
	SWAP(i, j);
    for (i = start, j = start+m+n-1; i < j; i++, j--)
	SWAP(i, j);
}

/*
 * Unstable sort algorithm used to sort the buffer every so often.
 * Any old sort algorithm will work here: since the amount of
 * buffer needing to be sorted is always O(sqrt(size of
 * subproblem)), we could even get away with an easy O(N^2) sort.
 * However, I'm going to use heapsort, just for a bit of extra
 * oomph.
 */
#define PARENT(n) ( ((n)-1)/2 )
#define LCHILD(n) ( 2*(n)+1 )
#define RCHILD(n) ( 2*(n)+2 )
static void bufsort(void *base, size_t size, cmpfn compare CTXPARAM,
		    size_t start, size_t nmemb)
{
    size_t n, i;

    /*
     * Phase 1: build the heap. We want the _largest_ element at
     * the top.
     */
    n = 0;
    while (n < nmemb) {
	n++;

	/*
	 * Swap element n with its parent repeatedly to preserve
	 * the heap property.
	 */
	i = n-1;

	while (i > 0) {
	    int p = PARENT(i);

	    if (COMPARE(start+p, start+i) < 0) {
		SWAP(start+p, start+i);
		i = p;
	    } else
		break;
	}
    }

    /*
     * Phase 2: repeatedly remove the largest element and stick it
     * at the top of the array.
     */
    while (n > 0) {
	/*
	 * The largest element is at position 0. Put it at the top,
	 * and swap the arbitrary element from that position into
	 * position 0.
	 */
	n--;
	SWAP(start+0, start+n);

	/*
	 * Now repeatedly move that arbitrary element down the heap
	 * by swapping it with the more suitable of its children.
	 */
	i = 0;
	while (1) {
	    int lc, rc;

	    lc = LCHILD(i);
	    rc = RCHILD(i);

	    if (lc >= n)
		break;		       /* we've hit bottom */

	    if (rc >= n) {
		/*
		 * Special case: there is only one child to check.
		 */
		if (COMPARE(start+i, start+lc) < 0)
		    SWAP(start+i, start+lc);

		/* _Now_ we've hit bottom. */
		break;
	    } else {
		/*
		 * The common case: there are two children and we
		 * must check them both.
		 */
		if (COMPARE(start+i, start+lc) < 0 ||
		    COMPARE(start+i, start+rc) < 0) {
		    /*
		     * Pick the more appropriate child to swap with
		     * (i.e. the one which would want to be the
		     * parent if one were above the other - as one
		     * is about to be).
		     */
		    if (COMPARE(start+lc, start+rc) < 0) {
			SWAP(start+i, start+rc);
			i = rc;
		    } else {
			SWAP(start+i, start+lc);
			i = lc;
		    }
		} else {
		    /* This element is in the right place; we're done. */
		    break;
		}
	    }
	}
    }
}

/*
 * Stably distribute a buffer of "bufsize" elements from the
 * beginning of the sequence of "nmemb" elements starting at
 * "start" throughout the rest of the sequence. Elements in the
 * buffer are assumed to come before equivalent elements in the
 * main sequence. This is O(N) if the buffer is at most O(sqrt(N))
 * in size.
 */
static void ldistribute(void *base, size_t size, cmpfn compare CTXPARAM,
 			size_t start, size_t nmemb, size_t bufsize)
{
    size_t buftop = start + bufsize;

    while (bufsize > 0) {
	size_t bufbot = buftop - bufsize;
	/*
	 * Binary-search in the main array (or rather, that
	 * part of it from buftop upwards) to find where the
	 * element at bufbot should be placed. In the event of
	 * a tie, stability is preserved by treating bufbot as
	 * coming before any other indistinguishable element.
	 */
	size_t bot = buftop - 1, top = start + nmemb, mid;

	while (top - bot > 1) {
	    mid = (top + bot) / 2;
	    if (COMPARE(bufbot, mid) <= 0)
		top = mid;
	    else
		bot = mid;
	}

	/*
	 * Now we have a piece of array looking like this:
	 *
	 * bufbot     buftop              top
	 *   +-+--------+------------------+
	 *   |X|    A   |         B        |
	 *   +-+--------+------------------+
	 *
	 * and we need it to look like this:
	 *   +------------------+-+--------+
	 *   |         B        |X|    A   |
	 *   +------------------+-+--------+
	 *
	 * This is therefore a trivial block exchange: we move
	 * the whole buffer to after B, and then just stop
	 * considering element X to be part of the buffer.
	 */
	block_exchange(base, size, bufbot, bufsize, top - buftop);
	buftop = top;
	bufbot = buftop - bufsize;
	/*
	 * While we're decrementing bufsize, we must now skip over
	 * any further elements which compare equal to the one we
	 * just put in place. This is vital to preserve linear
	 * time in the case of few distinct elements: in that
	 * situation the block we're distributing might be larger
	 * than O(sqrt(N)), so doing a binary search per element
	 * of it would bring us to O(N log N) time in total.
	 * Skipping equal elements at this point brings us back
	 * down to one binary search - and one block exchange -
	 * per _distinct_ element, which is acceptable.
	 */
	do {
	    bufsize--;
	} while (bufsize > 0 && COMPARE(buftop - bufsize, bufbot) == 0);
    }
}

/*
 * Mirror image to ldistribute: stably distribute a buffer of
 * "bufsize" elements from the _end_ of the sequence of "nmemb"
 * elements starting at "start" throughout the rest of the
 * sequence, assuming elements in the buffer to come _after_
 * things in the main sequence.
 *
 * Hideous clone-and-hack of ldistribute. I'd much rather have a
 * useful way of combining the two routines into one piece of
 * source code.
 */
static void rdistribute(void *base, size_t size, cmpfn compare CTXPARAM,
 			size_t start, size_t nmemb, size_t bufsize)
{
    size_t bufbot = start + nmemb - bufsize;

    while (bufsize > 0) {
	size_t buftop = bufbot + bufsize;
	/*
	 * Binary-search in the main array (or rather, that part
	 * of it from buftop upwards) to find where the element at
	 * buftop-1 should be placed. In the event of a tie,
	 * stability is preserved by treating bufbot as coming
	 * after any other indistinguishable element.
	 */
	size_t bot = start-1, top = bufbot, mid;

	while (top - bot > 1) {
	    mid = (top + bot) / 2;
	    if (COMPARE(buftop-1, mid) < 0)
		top = mid;
	    else
		bot = mid;
	}

	/*
	 * Now we have a piece of array looking like this:
	 *
	 *  top               bufbot     buftop
	 *   +------------------+--------+-+
	 *   |         B        |    A   |X|
	 *   +------------------+--------+-+
	 *
	 * and we need it to look like this:
	 *   +--------+-+------------------+
	 *   |    A   |X|         B        |
	 *   +--------+-+------------------+
	 *
	 * This is therefore a trivial block exchange: we move
	 * the whole buffer to after B, and then just stop
	 * considering element X to be part of the buffer.
	 */
	block_exchange(base, size, top, bufbot - top, bufsize);
	bufbot = top;
	buftop = bufbot + bufsize;
	/*
	 * As in ldistribute, we must skip over identical elements
	 * here, to preserve linear time complexity.
	 */
	do {
	    bufsize--;
	} while (bufsize > 0 && COMPARE(bufbot + bufsize-1, buftop-1) == 0);
    }
}

/*
 * Recursive merge algorithm.
 */
static void rmerge(void *base, size_t size, cmpfn compare CTXPARAM,
		   size_t start, size_t m, size_t n)
{
    size_t bot, mid, top;
    size_t mcut, ncut, nstart, newnstart, end;

    if (m == 0 || n == 0)
	return;
    if (m == 1 && n == 1) {
	if (COMPARE(start, start+1) > 0)
	    SWAP(start, start+1);
	return;
    }

    nstart = start + m;
    end = nstart + n;

    if (m >= n) {
	mcut = start + m / 2;
	bot = nstart - 1;
	top = end;
	while (top - bot > 1) {
	    mid = (bot + top) / 2;
	    if (COMPARE(mcut, mid) > 0)
		bot = mid;
	    else
		top = mid;
	}
	ncut = top;
    } else {
	ncut = nstart + n / 2;
	bot = start - 1;
	top = nstart;
	while (top - bot > 1) {
	    mid = (bot + top) / 2;
	    if (COMPARE(ncut, mid) >= 0)
		bot = mid;
	    else
		top = mid;
	}
	mcut = top;
    }

    block_exchange(base, size, mcut, nstart-mcut, ncut-nstart);
    newnstart = mcut + (ncut-nstart);
    rmerge(base, size, compare CTXARG, start, mcut-start, newnstart-mcut);
    rmerge(base, size, compare CTXARG, newnstart, ncut-newnstart, end-ncut);
}

static void merge(void *base, size_t size, cmpfn compare CTXPARAM,
		  size_t bufsize, size_t start, size_t m, size_t n)
{
    size_t s6blksize = squareroot(m+n);
    size_t s6bufsize = 2*s6blksize + m/s6blksize + n/s6blksize;
    size_t blkstart, blkend, blksize, blocks, mblocks, nblocks, mextra, nextra;
    int method;

    if (m + n <= RECURSION_THRESHOLD) {
	rmerge(base, size, compare CTXARG, start, m, n);
	return;
    }

    /*
     * Decide which merge algorithm we're using, and work out the
     * size of the blocks.
     */
    if (bufsize >= s6bufsize) {
	method = 6;		       /* Section 6 standard merge */
	blksize = s6blksize;
    } else {
	method = 5;		       /* Section 5 limited-buffer merge */
	blksize = (m+n + bufsize - 2) / (bufsize - 1);
    }

    /*
     * We're going to partition our array into blocks of size
     * blksize, by leaving a partial block at the start and one at
     * the end so that the m-blocks and n-blocks abut directly.
     */
    mblocks = m / blksize;
    mextra = m - mblocks * blksize;
    blkstart = start + mextra;
    nblocks = n / blksize;
    nextra = n - nblocks * blksize;
    blocks = mblocks + nblocks;
    blkend = blkstart + blocks * blksize;

    if (mblocks && nblocks) {
	if (method == 6) {
	    size_t mi, mb, mr, ni, nb, nr, blkindex;
	    size_t mergebufin, mergebufout;

	    /*
	     * Section 6 merge. We need a tracking buffer of size
	     * "blocks", and a merge buffer of size 2*blksize.
	     */
	    size_t mergebuf = 0;       /* start of buffer space */
	    size_t trackbuf = mergebuf + 2*blksize;
	    assert(trackbuf + blocks <= s6bufsize);

	    /*
	     * Start by sorting the tracking buffer, since we're
	     * going to use it to order the output blocks of the
	     * merge.
	     */
	    bufsort(base, size, compare CTXARG, trackbuf, blocks);

	    /*
	     * Now simply start reading the two input lists of
	     * blocks, and writing merged output into the merge
	     * buffer.
	     */
	    mi = blkstart;	       /* index of next element */
	    mb = 0;		       /* index of current block */
	    mr = blksize;	       /* elements remaining in that block */
	    ni = start + m;	       /* index of next element */
	    nb = mblocks;	       /* index of current block */
	    nr = blksize;	       /* elements remaining in that block */
	    mergebufin = mergebufout = 0;
	    while (mi < start + m || ni < blkend) {
		blkindex = blocks;     /* dummy value: no finished block */

		/*
		 * Decide which list we're taking an item from.
		 */
		if (ni >= blkend ||
		    (mi < start + m && COMPARE(mi, ni) <= 0)) {
		    /* Take from the m-list. */
		    SWAP(mergebufin, mi);
		    mergebufin = (mergebufin + 1) % (2*blksize);
		    mi++;
		    mr--;
		    if (mr == 0) {
			blkindex = mb++;
			mr = blksize;
		    }
		} else {
		    /* Take from the n-list. */
		    SWAP(mergebufin, ni);
		    mergebufin = (mergebufin + 1) % (2*blksize);
		    ni++;
		    nr--;
		    if (nr == 0) {
			blkindex = nb++;
			nr = blksize;
		    }
		}

		/*
		 * If we've emptied (i.e. filled with merge buffer
		 * elements) an entire input block on either the
		 * m- or n-side, we now fill it with merge output
		 * data from the merge buffer.
		 */
		if (blkindex < blocks) {
		    size_t smallest, i;

		    SWAPN(mergebufout, blkstart + blksize * blkindex, blksize);
		    mergebufout = (mergebufout + blksize) % (2*blksize);

		    /*
		     * Now we must find the smallest as yet unused
		     * element in the tracking buffer, and swap it
		     * into the place matching this block, so that
		     * we know what order to output the blocks in
		     * when we've finished.
		     */
		    smallest = blkindex;
		    for (i = mb; i < mblocks; i++)
			if (smallest == blocks ||
			    COMPARE(trackbuf + i, trackbuf + smallest) < 0)
			    smallest = i;
		    for (i = nb; i < blocks; i++)
			if (smallest == blocks ||
			    COMPARE(trackbuf + i, trackbuf + smallest) < 0)
			    smallest = i;
		    if (smallest != blkindex)
			SWAP(trackbuf + blkindex, trackbuf + smallest);
		}
	    }

	    /*
	     * Our stably merged output list is now sitting in our
	     * block list, except that the blocks are permuted
	     * into some arbitrary wrong order, and the tracking
	     * buffer knows what order that is. So we now
	     * selection-sort the tracking buffer, and swap real
	     * blocks in parallel with the swaps done in that
	     * sort. (Selection sort is used because it uses the
	     * minimum number of swaps, and they're what's
	     * expensive here.)
	     */
	    {
		size_t i, j, smallest;
		for (i = 0; i < blocks; i++) {
		    smallest = i;
		    for (j = i+1; j < blocks; j++) {
			if (COMPARE(trackbuf + j, trackbuf + smallest) < 0)
			    smallest = j;
		    }
		    if (i != smallest) {
			SWAP(trackbuf + i, trackbuf + smallest);
			SWAPN(blkstart + i * blksize,
			      blkstart + smallest * blksize, blksize);
		    }
		}
	    }

	    /*
	     * And that's our main merge complete.
	     */
	} else {
	    size_t firstn, currpos;
	    size_t movedstart = 0, movedend = 0;
	    int movedseq = 0;

	    /*
	     * Sort the buffer.
	     */
	    bufsort(base, size, compare CTXARG, 0, blocks);

	    /*
	     * Identify the buffer element corresponding to the
	     * first n-block. We will keep this index correct
	     * throughout the following sort, so that we can
	     * always tell which input sequence a given block
	     * belongs to by comparing its corresponding element
	     * in the buffer with this one.
	     */
	    firstn = mblocks;

	    /*
	     * Selection-sort the blocks by their first element,
	     * breaking ties using the buffer. We also mirror
	     * block swaps in the buffer, and keep firstn up to
	     * date in the process.
	     */
	    {
		size_t i, j, smallest;
		for (i = 0; i < blocks; i++) {
		    smallest = i;
		    for (j = i+1; j < blocks; j++) {
			int cmp = COMPARE(blkstart + j * blksize,
					  blkstart + smallest * blksize);
			if (!cmp)
			    cmp = COMPARE(j, smallest);
			if (cmp < 0)
			    smallest = j;
		    }
		    if (i != smallest) {
			SWAPN(blkstart + i * blksize,
			      blkstart + smallest * blksize, blksize);
			SWAP(i, smallest);
			if (i == firstn || smallest == firstn)
			    firstn = i + smallest - firstn;
		    }
		}
	    }

	    /*
	     * "currpos" will track the next unmerged element from
	     * here to the end of the array.
	     */
	    currpos = blkstart;

	    while (currpos < blkend) {
		int seqA, seqB, cmp;
		size_t i, apos = currpos, bpos;

		/*
		 * We're looking at the next unmerged element,
		 * which I'll call A. Find out which original
		 * sequence it's from: usually we do this by
		 * finding the buffer entry corresponding to its
		 * block, although if A is part of a stretch of
		 * the array we moved in a previous iteration then
		 * the buffer may be wrong.
		 */
		if (apos >= movedstart && apos < movedend) {
		    seqA = movedseq;
		    bpos = movedend;
		} else {
		    i = (apos - blkstart) / blksize;
		    seqA = COMPARE(i, firstn) >= 0;/* 0 means m, 1 means n */
		    bpos = blkstart + (i+1) * blksize;
		}

		/*
		 * Search forward to find the next element B from
		 * the _other_ sequence, whichever it is.
		 */
		i = (bpos - blkstart) / blksize;
		seqB = !seqA;
		while (i < blocks && (COMPARE(i, firstn) >= 0) == seqA) {
		    i++;
		    bpos = blkstart + i * blksize;
		}

		/*
		 * If B doesn't exist (we've hit the end of the
		 * list), we've finished!
		 */
		if (bpos == blkend)
		    break;

		/*
		 * Otherwise, see if some merging needs to be
		 * done. If B comes after the element directly
		 * before it (from the other sequence), then we
		 * don't need to move anything just yet.
		 *
		 * (Note that "comes after" must be interpreted
		 * stably, which means we must break ties by
		 * referring to our knowledge of which original
		 * sequences the two elements are from.)
		 */
		cmp = COMPARE(bpos-1, bpos);
		if (cmp == 0)
		    cmp = seqA - seqB; /* break ties correctly */

		if (cmp < 0) {
		    /*
		     * This is the easy case: everything from A to
		     * just before B is already correctly merged,
		     * so we can simply advance currpos.
		     */
		    currpos = bpos;
		    movedstart = movedend = 0;
		} else {
		    size_t bot, mid, top;
		    size_t cpos;

		    /*
		     * And this is the case where we actually have
		     * to do some work (bah): B must be inserted
		     * somewhere between A and where it currently
		     * is. (Up to and including putting it
		     * _before_ A itself.) So we start by
		     * binary-searching for that insertion point.
		     * Again, we must take care to break
		     * comparison ties in a direction dependent on
		     * seqA and seqB.
		     */
		    bot = apos-1;
		    top = bpos;
		    while (top - bot > 1) {
			mid = (top + bot) / 2;
			cmp = COMPARE(mid, bpos);
			if (cmp == 0)
			    cmp = seqA - seqB;
			if (cmp < 0)
			    bot = mid;
			else
			    top = mid;
		    }
		    cpos = top;

		    /*
		     * Now "cpos" points at some element C of A's
		     * sequence which comes after element B. (The
		     * above search cannot have terminated with
		     * "top" pointing at B itself, because
		     * otherwise we'd be in the easy case above.)
		     *
		     * We can't just move B to that position yet,
		     * though, because there may be further
		     * elements of _B's_ sequence which come
		     * before C. So now we search forward for
		     * those.
		     */
		    bot = bpos;
		    /* i is still pointing at B's block number; start there. */
		    while (++i < blocks) {
			/*
			 * See if we can skip an entire block in
			 * our search.
			 */
			if ((COMPARE(i, firstn) >= 0) != seqB)
			    break;     /* no, this is A's sequence again */
			/* Check the first element of the new block. */
			cmp = COMPARE(blkstart + i * blksize, cpos);
			if (cmp == 0)
			    cmp = seqB - seqA;
			if (cmp > 0) {
			    break;     /* gone too far */
			} else {
			    /* yes, we can skip a block */
			    bot = blkstart + i * blksize;
			}
		    }
		    /* Now we can binary-search one block only. */
		    top = bot - (bot-blkstart) % blksize + blksize;
		    while (top - bot > 1) {
			mid = (top + bot) / 2;
			cmp = COMPARE(mid, cpos);
			if (cmp == 0)
			    cmp = seqB - seqA;
			if (cmp < 0)
			    bot = mid;
			else
			    top = mid;
		    }

		    /*
		     * Now we're ready. We have a chunk of array
		     * looking like
		     * 
		     * apos   cpos   bpos       top
		     *  +------+------+----------+
		     *  |  P   |  Q   |    R     |
		     *  +------+------+----------+
		     * 
		     * and we know that everything up to cpos is
		     * correctly positioned, and that everything
		     * in stretch R must come before element C (at
		     * the start of stretch Q). So we can
		     * block-exchange Q with R, and update currpos
		     * to point at where the end of R ended up.
		     */
		    block_exchange(base, size, cpos, bpos - cpos, top - bpos);
		    currpos = cpos + (top - bpos);

		    /*
		     * And record the fact that we've moved
		     * stretch Q, so we know which sequence it
		     * belongs to better than the buffer does.
		     */
		    movedstart = currpos;
		    movedend = top;
		    movedseq = seqA;
		}
	    }
	}
    }

#ifdef TESTMODE
    /*
     * Our main block sequence should now be correctly merged.
     */
    subseq_should_be_sorted(blkstart, blkend - blkstart);
#endif

    /*
     * Now we need to stably distribute the partial blocks from
     * each end into the main sorted sequence, and we're done.
     */
    ldistribute(base, size, compare CTXARG, start, blkend-start, mextra);
    rdistribute(base, size, compare CTXARG, start, m+n, nextra);
}

void amergesort(void *base, size_t nmemb, size_t size, cmpfn compare CTXPARAM)
{
    size_t bufsize = 0;

    /*
     * Stupid special case: sorting zero elements should just
     * return rather than go mad.
     */
    if (nmemb == 0)
	return;

    /*
     * Start by extracting a buffer of size 3*sqrt(n), or as large
     * as we can get short of that.
     * 
     * The buffer will end up at the start of the array, but the
     * buffer extraction procedure cannot involve _swapping_
     * elements into that area, because moving an element past
     * other elements it hasn't been compared with would risk
     * destroying stability. So instead, we move the buffer
     * gradually up the array as we go, and move it back down when
     * we've finished.
     *
     * Since we're extracting the buffer once for the entire sort,
     * we're allowed to do O(N log N) work to get it if we need
     * to. (Good job too, since the fact that the list is unsorted
     * at this stage would make it tricky otherwise.)
     *
     * Special case: if the entire list is small enough that we're
     * never going to need a Symvonis algorithm at all, we needn't
     * bother and we can just leave bufsize at zero.
     */
    if (nmemb > RECURSION_THRESHOLD) {
	size_t bufsize_preferred = 3 * squareroot(nmemb);
	size_t buftop = 1, currpos = 1;
	bufsize = 1;

	while (currpos < nmemb && bufsize < bufsize_preferred) {
	    /*
	     * See if the next array element is equal to anything
	     * in the buffer. We can safely do a log-time binary
	     * search of the buffer to check this.
	     */
	    size_t bufbot = buftop - bufsize;
	    size_t bot = buftop - bufsize - 1, top = buftop, mid;
	    int cmp;

	    while (top - bot > 1) {
		mid = (top + bot) / 2;
		cmp = COMPARE(currpos, mid);
		if (cmp == 0) {
		    currpos++;
		    goto buf_extract_continue;   /* order-2 "continue" */
		}
		else if (cmp < 0)
		    top = mid;
		else
		    bot = mid;
	    }

	    /*
	     * If we get here, then the new element can indeed go
	     * in the buffer, and moreover "top" points at exactly
	     * where it needs to be.
	     * 
	     * So we have a chunk of array that looks like
	     * 
	     * bufbot   top   buftop       currpos
	     *    +------+-------+------------+-+
	     *    |   A  |   B   |     C      |X|
	     *    +------+-------+------------+-+
	     * 
	     * which we want to rearrange so that it looks like
	     *    +------------+------+-+-------+
	     *    |     C      |   A  |X|   B   |
	     *    +------------+------+-+-------+
	     * 
	     * To achieve this, we do a block exchange of AB with
	     * C to get this intermediate stage:
	     *    +------------+------+-------+-+
	     *    |     C      |   A  |   B   |X|
	     *    +------------+------+-------+-+
	     *
	     * followed by a nearly trivial block exchange of B
	     * with X.
	     *
	     * Elements that don't end up in the buffer (i.e.
	     * things in C) are only ever moved once by one of
	     * these procedures, so there are O(N) such moves in
	     * total. Elements that do end up in the buffer are
	     * moved once for every subsequent buffer element
	     * added, so there are O(bufsize^2) such moves - but
	     * bufsize itself is O(sqrt(N)), so this is O(N) time
	     * in total as well.
	     */
	    block_exchange(base, size, bufbot, bufsize, currpos - buftop);
	    block_exchange(base, size, currpos - (buftop-top), buftop-top, 1);
	    buftop = ++currpos;
	    bufsize++;
#ifdef TESTMODE
	    subseq_should_be_sorted(buftop - bufsize, bufsize);
#endif

	    buf_extract_continue:;
	}

	/*
	 * Now we've got our complete buffer, move it back down to
	 * the bottom of the array.
	 */
	block_exchange(base, size, 0, buftop - bufsize, bufsize);

#ifdef TESTMODE
	subseq_should_be_sorted(0, bufsize);
#endif
    }

    /*
     * Special case: if we only managed to get _one_ buffer
     * element, then that must mean absolutely all elements in the
     * array compare equal. Hence, no sorting required; just
     * return.
     */
    if (bufsize == 1)
	return;

    /*
     * Now we can do the main sort. We start with simple insertion
     * sorts (which are stable) for small subarrays, and then use
     * stable merging algorithms to merge those subarrays
     * gradually up into a whole sorted array. (Well, a whole
     * sorted array apart from the buffer.)
     *
     * Insertion sorts are O(N^2), but if we cap the size of the
     * insertion sorts at a constant, then the initial set of
     * insertion sorts is merely O(constant^2 * N/constant) =
     * O(N), so it isn't a theoretical obstacle to O(N log N)
     * complexity overall.
     */
    {
	size_t sublistlen = INSERTION_THRESHOLD;
	size_t subliststart, thissublistlen, thissublistlen2;

	/* Insertion sort pass. */
	subliststart = bufsize;
	while (subliststart < nmemb) {
	    size_t i, j;

	    thissublistlen = sublistlen;
	    if (thissublistlen > nmemb - subliststart)
		thissublistlen = nmemb - subliststart;

	    for (i = 1; i < thissublistlen; i++) {
		/* Insertion-sort element i into place. */
		j = i;
		while (j > 0 &&
		       COMPARE(subliststart+j-1, subliststart+j) > 0) {
		    SWAP(subliststart+j-1, subliststart+j);
		    j--;
		}
	    }

#ifdef TESTMODE
	    subseq_should_be_sorted(subliststart, thissublistlen);
#endif

	    subliststart += thissublistlen;
	}

	/*
	 * Merge passes. In each of these we combine sublists of
	 * length sublistlen into sublists of length 2*sublistlen
	 * (with special cases for short lists at the end).
	 */
	while (1) {
	    subliststart = bufsize;

	    while (subliststart < nmemb) {
		thissublistlen = sublistlen;
		if (thissublistlen > nmemb - subliststart)
		    thissublistlen = nmemb - subliststart;

		thissublistlen2 = sublistlen;
		if (thissublistlen2 > nmemb - subliststart - thissublistlen)
		    thissublistlen2 = nmemb - subliststart - thissublistlen;

		if (thissublistlen2 > 0)
		    merge(base, size, compare CTXARG, bufsize,
			  subliststart, thissublistlen, thissublistlen2);

#ifdef TESTMODE
		subseq_should_be_sorted(subliststart,
					thissublistlen + thissublistlen2);
#endif
		if (subliststart == bufsize &&
		    subliststart + thissublistlen + thissublistlen2 == nmemb)
		    goto merging_done; /* two-level break */

		subliststart += thissublistlen + thissublistlen2;
	    }

	    sublistlen *= 2;
	}
	merging_done:;
    }

    /*
     * Finally, we must sort the buffer, and then merge it back
     * into the rest of the sorted array. The stability
     * requirement here is that we know the buffer contained the
     * _first_ occurrence of every element in it, so we must
     * preserve that when distributing it back into the output.
     */
    if (bufsize) {
	bufsort(base, size, compare CTXARG, 0, bufsize);
	ldistribute(base, size, compare CTXARG, 0, nmemb, bufsize);
    }

    /*
     * And that's it! One array, stably sorted in place in O(N log
     * N) time.
     */
#ifdef TESTMODE
    subseq_should_be_sorted(0, nmemb);
#endif
}

#ifdef TESTMODE

/*
 * gcc -g -O0 -DTESTMODE -o amergesort amergesort.c
 */

#define NTEST 12345
#define NKEYS 100
#define KEYPOS 100000

static int testarray[NTEST];
static int original[NTEST];

static int compare(const void *av, const void *bv)
{
    const int *ap = (const int *)av;
    const int *bp = (const int *)bv;
    int a = *ap;
    int b = *bp;
    a /= KEYPOS;
    b /= KEYPOS;
    compares++;
    return a < b ? -1 : a > b ? +1 : 0;
}

/* Used for comparisons in the test rig, which is allowed to know about the
 * low-order digits. */
static int cheating_compare(const void *av, const void *bv)
{
    const int *ap = (const int *)av;
    const int *bp = (const int *)bv;
    int a = *ap;
    int b = *bp;
    return a < b ? -1 : a > b ? +1 : 0;
}

static void subseq_should_be_sorted(size_t start, size_t len)
{
    size_t i;

    for (i = 1; i < len; i++)
	assert(testarray[start+i] > testarray[start+i-1]);
}

int main(void)
{
    int i, j;

    srand(1234);

    compares = swaps = 0;
    for (j = 0; j < 100; j++) {
	for (i = 0; i < NTEST; i++)
	    testarray[i] = rand() % NKEYS * KEYPOS;
	for (i = NTEST; i-- > 1 ;) {
	    int j = rand() % (i+1);
	    int t = testarray[i];
	    testarray[i] = testarray[j];
	    testarray[j] = t;
	}
	for (i = 0; i < NTEST; i++)
	    testarray[i] += i;
	memcpy(original, testarray, sizeof(original));
	qsort(original, NTEST, sizeof(*original), compare);
	amergesort(testarray, NTEST, sizeof(*testarray), compare);
	for (i = 0; i < NTEST; i++)
	    assert(testarray[i] == original[i]);
    }
    printf("%d compares, %d swaps\n", compares / 100, swaps / 100);
    return 0;
}

#endif
