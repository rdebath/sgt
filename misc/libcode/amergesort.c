/*
 * amergesort.c: Mergesort in-place on an array, sorting stably in
 * guaranteed O(N log N) time.
 */

/*
 * The approach taken to merging is the "particularly simple"
 * method given in section 6 of Antonios Symvonis's paper "Optimal
 * Stable Merging"; if not enough distinguishable elements are
 * present, we fall back to the method in section 5 of the same
 * paper.
 *
 *   A. Symvonis. Optimal stable merging. The Computer Journal,
 *   38(8):681-690, 1995.
 *   http://citeseer.ist.psu.edu/78814.html
 *
 * Both of these merging methods require the preliminary
 * extraction of a buffer of O(sqrt(N)) distinct elements to use
 * as temporary space for the rest of the merge, and then that
 * buffer has to be merged back into the rest of the array at the
 * end. This is all very well in theory if you want to do a single
 * merge, but it seems to me that for more practical performance
 * in a full merge _sort_ involving lots and lots of merges of
 * various sizes, what we should clearly do is to extract a buffer
 * once at the very start of the entire procedure rather than
 * faffing about with it in every single merge and sub-merge. This
 * should enable us to do most of the sort in a relatively
 * straightforward manner without causing _too_ much chaos.
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
 * stick to the simple approach throughout.
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

#define COMPARE(i, j) compare(((char *)base)+(i)*(size), \
			      ((char *)base)+(j)*(size) CTXARG)
#define REALSWAP(i, j, n) memswap(((char *)base)+(i)*(size), \
			          ((char *)base)+(j)*(size), (n)*(size))
#define SWAP(i, j) ( REALSWAP(i, j, 1) )
#define SWAPN(i, j, n) ( REALSWAP(i, j, n) )

#ifdef TESTMODE
static void subseq_should_be_sorted(size_t start, size_t len);
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
	bufsize--;
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
	bufsize--;
    }
}

static void merge(void *base, size_t size, cmpfn compare CTXPARAM,
		  size_t bufsize, size_t start, size_t m, size_t n)
{
    size_t s6blksize = squareroot(m+n);
    size_t s6bufsize = 2*s6blksize + m/s6blksize + n/s6blksize;
    size_t blkstart, blkend, blksize, blocks, mblocks, nblocks, mextra, nextra;
    int method;

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
    size_t bufsize;

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
     */
    {
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
     * insertion sorts at a constant (determined empirically by
     * measuring the performance of the resulting sort as a whole)
     * then the initial set of insertion sorts is merely
     * O(constant^2 * N/constant) = O(N), so it isn't a
     * theoretical obstacle to O(N log N) complexity overall.
     */
    {
	size_t sublistlen = 8;	       /* FIXME: tune this */
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
    bufsort(base, size, compare CTXARG, 0, bufsize);
    ldistribute(base, size, compare CTXARG, 0, nmemb, bufsize);

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
    int i;

    srand(1234);

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
	printf("%5d: %5d [%5d]\n", i, testarray[i], original[i]);
    for (i = 0; i < NTEST; i++)
	assert(testarray[i] == original[i]);
    printf("ok\n");
    return 0;
}

#endif
