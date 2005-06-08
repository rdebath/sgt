/*
 * Generic reusable binary-heap maintenance code.
 * 
 * This file is copyright 2005 Simon Tatham.
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
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "bheap.h"

#ifdef TESTMODE
int coverage;			       /* bit flags for various if clauses */
int checked_coverage;
#define COVERAGE(x) ( coverage |= (1 << (x)) )
#else
#define COVERAGE(x) ( (void)0 )
#endif

struct bheap {
    int nelts, maxelts;
    int eltsize;
    int direction;
    bheap_cmpfn_t compare;
    void *compare_ctx;
    char *elts;
};

/*
 * Our array is zero-based. Therefore the children of 0 are 1 and
 * 2; the children of 1 are 3 and 4; of 2 are 5 and 6, and so on.
 * In other words, the children of node n are 2n+1 and 2n+2, and
 * the parent of node n is floor((n-1)/2).
 */
#define PARENT(n) ( ((n)-1)/2 )
#define LCHILD(n) ( 2*(n)+1 )
#define RCHILD(n) ( 2*(n)+2 )

/*
 * This macro calls the compare function, and returns TRUE if the
 * two elements are the wrong way up and should be swapped.
 * 
 * It doesn't _have_ to be called on a parent and child, but its
 * return value assumes that. So you could call it on any two
 * elements x,y and it would return TRUE if y ought to be the
 * parent of x.
 */
#define MISORDERED(bh,parent,child) \
    ( (bh)->direction * \
	  (bh)->compare((bh)->compare_ctx, \
			    (bh)->elts + (parent) * (bh)->eltsize, \
			    (bh)->elts + (child) * (bh)->eltsize) > 0 )

/*
 * This macro swaps two elements of the heap.
 */
#define SWAP(bh,x,y) do { \
    memcpy((bh)->elts + (bh)->maxelts * (bh)->eltsize, \
	   (bh)->elts + (x) * (bh)->eltsize, (bh)->eltsize); \
    memcpy((bh)->elts + (x) * (bh)->eltsize, \
	   (bh)->elts + (y) * (bh)->eltsize, (bh)->eltsize); \
    memcpy((bh)->elts + (y) * (bh)->eltsize, \
	   (bh)->elts + (bh)->maxelts * (bh)->eltsize, (bh)->eltsize); \
} while (0)

bheap *bheap_new(int maxelts, int eltsize, int direction,
		 bheap_cmpfn_t compare, void *compare_ctx)
{
    bheap *bh = malloc(sizeof(bheap));
    if (!bh)
	return NULL;

    /*
     * Allocate one extra element of space, to use for swapping
     * things.
     */
    bh->elts = malloc(maxelts * (eltsize+1));
    if (!bh->elts)
	return NULL;

    bh->nelts = 0;
    bh->maxelts = maxelts;
    bh->eltsize = eltsize;
    bh->direction = direction;
    bh->compare = compare;
    bh->compare_ctx = compare_ctx;

    return bh;
}

void *bheap_add(bheap *bh, void *elt)
{
    int i;

    if (bh->nelts >= bh->maxelts) {
	COVERAGE(0);
	return NULL;
    }

    COVERAGE(1);
    /*
     * Add the element to the end of the array.
     */
    memcpy(bh->elts + bh->nelts * bh->eltsize, elt, bh->eltsize);
    bh->nelts++;

    /*
     * Now swap it with its parent repeatedly to preserve the heap
     * property.
     */
    i = bh->nelts-1;

    if (i == 0)
	COVERAGE(2);

    while (i > 0) {
	int p = PARENT(i);

	COVERAGE(3);

	if (MISORDERED(bh, p, i)) {
	    COVERAGE(4);
	    SWAP(bh, p, i);
	    i = p;
	} else {
	    COVERAGE(5);
	    break;		       /* we're done */
	}
    }

    return elt;
}

void *bheap_topmost(bheap *bh, void *elt)
{
    if (bh->nelts <= 0) {
	COVERAGE(6);
	return NULL;
    }

    COVERAGE(7);
    memcpy(elt, bh->elts, bh->eltsize);
    return elt;
}

void *bheap_remove(bheap *bh, void *elt)
{
    if (bh->nelts <= 0) {
	COVERAGE(8);
	return NULL;
    }

    if (elt)
	memcpy(elt, bh->elts, bh->eltsize);

    bh->nelts--;

    if (bh->nelts > 0) {
	int i;

	COVERAGE(8);
	/*
	 * Move the highest-index element of the heap into the top
	 * position.
	 */
	SWAP(bh, bh->nelts, 0);

	/*
	 * Now repeatedly move it down the heap by swapping it with
	 * the more suitable of its children.
	 */
	i = 0;
	while (1) {
	    int lc, rc;

	    lc = LCHILD(i);
	    rc = RCHILD(i);

	    COVERAGE(9);

	    if (lc >= bh->nelts) {
		COVERAGE(10);
		break;		       /* we've hit bottom */
	    }

	    if (rc >= bh->nelts) {
		/*
		 * Special case: there is only one child to check.
		 */
		COVERAGE(11);
		if (MISORDERED(bh, i, lc)) {
		    COVERAGE(12);
		    SWAP(bh, i, lc);
		} else {
		    COVERAGE(13);
		}
		/* _Now_ we've hit bottom. */
		break;
	    } else {
		COVERAGE(14);
		/*
		 * The common case: there are two children and we
		 * must check them both.
		 */
		if (MISORDERED(bh, i, lc) || MISORDERED(bh, i, rc)) {
		    COVERAGE(15);
		    /*
		     * Pick the more appropriate child to swap with
		     * (i.e. the one which would want to be the
		     * parent if one were above the other - as one
		     * is about to be).
		     */
		    if (MISORDERED(bh, lc, rc)) {
			COVERAGE(16);
			SWAP(bh, i, rc);
			i = rc;
		    } else {
			COVERAGE(17);
			SWAP(bh, i, lc);
			i = lc;
		    }
		} else {
		    /* This element is in the right place; we're done. */
		    COVERAGE(18);
		    break;
		}
	    }
	}
    } else {
	COVERAGE(19);
    }

    return elt;
}

int bheap_count(bheap *bh)
{
    return bh->nelts;
}

void bheap_free(bheap *bh)
{
    if (bh) {
	if (bh->elts)
	    free(bh->elts);
	free(bh);
    }
}

#ifdef TESTMODE

#include <stdio.h>

/* we _really_ need assertions enabled, for this test */
#undef NDEBUG
#include <assert.h>

#define MAX 8

#define CHECK_COVERAGE(c, statement) do { \
    coverage &= ~ (1 << (c)); \
    statement; \
    assert(coverage & (1 << (c))); \
    checked_coverage |= (1 << (c)); \
} while (0)

int intcmp_ctx;
int array[MAX];
int n;

bheap *bh;

int intcmp(void *vctx, const void *av, const void *bv)
{
    const int *a = (const int *)av;
    const int *b = (const int *)bv;
    int *ctx = (int *)vctx;

    assert(ctx == &intcmp_ctx);

    if (*a < *b)
	return -1;
    else if (*a > *b)
	return +1;
    return 0;
}

void add(int x)
{
    int *ret = bheap_add(bh, &x);

    if (n >= MAX) {
	assert(ret == NULL);
    } else {
	assert(ret == &x);

	array[n++] = x;
    }

    assert(bheap_count(bh) == n);
}

void rem(int x)
{
    int out1, *ret1, out2, *ret2;

    ret1 = bheap_topmost(bh, &out1);
    ret2 = bheap_remove(bh, &out2);

    if (n == 0) {
	assert(x < 0);		       /* test the tests! */
	assert(ret1 == NULL);
	assert(ret2 == NULL);
    } else {
	int i;

	assert(x >= 0);		       /* test the tests! */
	assert(ret1 == &out1);
	assert(ret2 == &out2);
	assert(out1 == out2);
	assert(out1 == x);

	/* Now find x in _our_ array and remove it. */
	for (i = 0; i < n; i++) {
	    assert(array[i] >= x);
	    if (array[i] == x) {
		int tmp;

		tmp = array[n-1];
		array[n-1] = array[i];
		array[i] = tmp;

		break;
	    }
	}
	assert(i < n);		       /* we expect to have found it */
	n--;
    }

    assert(bheap_count(bh) == n);
}

int main(void)
{
    coverage = checked_coverage = 0;

    bh = bheap_new(MAX, sizeof(int), +1, intcmp, &intcmp_ctx);

    /*
     * Various sets of adds and removes which test all the code
     * paths marked with COVERAGE() statements.
     */
    CHECK_COVERAGE(2, add(4));
    CHECK_COVERAGE(3, add(7));
    CHECK_COVERAGE(4, add(2));
    CHECK_COVERAGE(1, add(6));
    add(3);
    add(1);
    CHECK_COVERAGE(5, add(8));
    add(5);
    CHECK_COVERAGE(0, add(9));	       /* check the full-heap case */

    CHECK_COVERAGE(7, rem(1));
    CHECK_COVERAGE(8, rem(2));
    CHECK_COVERAGE(9, rem(3));
    CHECK_COVERAGE(14, rem(4));
    rem(5);
    rem(6);
    rem(7);
    CHECK_COVERAGE(19, rem(8));
    CHECK_COVERAGE(6, rem(-1));	       /* and check the empty-heap case */
    CHECK_COVERAGE(8, rem(-1));	       /* and check the empty-heap case */

    add(1);
    add(2);
    add(3);
    CHECK_COVERAGE(12, rem(1));
    rem(2);
    rem(3);

    add(1);
    add(3);
    add(2);
    CHECK_COVERAGE(13, rem(1));
    rem(2);
    rem(3);

    add(1);
    add(2);
    add(3);
    add(4);
    CHECK_COVERAGE(17, rem(1));
    rem(2);
    rem(3);
    rem(4);

    add(1);
    add(3);
    add(2); 
    add(4);
    CHECK_COVERAGE(16, rem(1));
    rem(2);
    rem(3);
    rem(4);

    add(1);
    add(2);
    add(3);
    add(5);
    add(6);
    add(7);
    add(4);
    CHECK_COVERAGE(18, rem(1));
    CHECK_COVERAGE(15, rem(2));
    rem(3);
    CHECK_COVERAGE(10, rem(4));
    CHECK_COVERAGE(11, rem(5));
    rem(6);
    rem(7);

    /*
     * See what happens with compare equality.
     */
    add(3);
    add(3);
    add(3);
    add(3);
    add(3);
    add(3);
    add(3);
    add(3);
    rem(3);
    rem(3);
    rem(3);
    rem(3);
    rem(3);
    rem(3);
    rem(3);
    rem(3);

    add(5);
    add(4);
    add(7);
    add(4);
    add(1);
    add(4);
    add(3);
    rem(1);
    rem(3);
    rem(4);
    rem(4);
    rem(4);
    rem(5);
    rem(7);

    /*
     * Interleave some adds and removes, turning the heap into a
     * real priority queue rather than a glorified sorter.
     * 
     * We add the digits of pi in order, and keep the heap size
     * capped at 5 by extracting one before adding the next.
     * 
     * Python code that generates this test sequence:

python -c '
list=[]
for c in "314159265358979323846264338327950288":
    c = int(c)
    if len(list) >= 5:
        list.sort()
        d = list[0]
        del list[0]
        print ("    rem(%d); /"+"* %s *"+"/") % (d, repr(list))
    list.append(c)
    list.sort()
    print ("    add(%d); /"+"* %s *"+"/") % (c, repr(list))
while len(list) > 0:
    list.sort()
    d = list[0]
    del list[0]
    print ("    rem(%d); /"+"* %s *"+"/") % (d, repr(list))
print "    rem(-1);"
'

     */
    add(3); /* [3] */
    add(1); /* [1, 3] */
    add(4); /* [1, 3, 4] */
    add(1); /* [1, 1, 3, 4] */
    add(5); /* [1, 1, 3, 4, 5] */
    rem(1); /* [1, 3, 4, 5] */
    add(9); /* [1, 3, 4, 5, 9] */
    rem(1); /* [3, 4, 5, 9] */
    add(2); /* [2, 3, 4, 5, 9] */
    rem(2); /* [3, 4, 5, 9] */
    add(6); /* [3, 4, 5, 6, 9] */
    rem(3); /* [4, 5, 6, 9] */
    add(5); /* [4, 5, 5, 6, 9] */
    rem(4); /* [5, 5, 6, 9] */
    add(3); /* [3, 5, 5, 6, 9] */
    rem(3); /* [5, 5, 6, 9] */
    add(5); /* [5, 5, 5, 6, 9] */
    rem(5); /* [5, 5, 6, 9] */
    add(8); /* [5, 5, 6, 8, 9] */
    rem(5); /* [5, 6, 8, 9] */
    add(9); /* [5, 6, 8, 9, 9] */
    rem(5); /* [6, 8, 9, 9] */
    add(7); /* [6, 7, 8, 9, 9] */
    rem(6); /* [7, 8, 9, 9] */
    add(9); /* [7, 8, 9, 9, 9] */
    rem(7); /* [8, 9, 9, 9] */
    add(3); /* [3, 8, 9, 9, 9] */
    rem(3); /* [8, 9, 9, 9] */
    add(2); /* [2, 8, 9, 9, 9] */
    rem(2); /* [8, 9, 9, 9] */
    add(3); /* [3, 8, 9, 9, 9] */
    rem(3); /* [8, 9, 9, 9] */
    add(8); /* [8, 8, 9, 9, 9] */
    rem(8); /* [8, 9, 9, 9] */
    add(4); /* [4, 8, 9, 9, 9] */
    rem(4); /* [8, 9, 9, 9] */
    add(6); /* [6, 8, 9, 9, 9] */
    rem(6); /* [8, 9, 9, 9] */
    add(2); /* [2, 8, 9, 9, 9] */
    rem(2); /* [8, 9, 9, 9] */
    add(6); /* [6, 8, 9, 9, 9] */
    rem(6); /* [8, 9, 9, 9] */
    add(4); /* [4, 8, 9, 9, 9] */
    rem(4); /* [8, 9, 9, 9] */
    add(3); /* [3, 8, 9, 9, 9] */
    rem(3); /* [8, 9, 9, 9] */
    add(3); /* [3, 8, 9, 9, 9] */
    rem(3); /* [8, 9, 9, 9] */
    add(8); /* [8, 8, 9, 9, 9] */
    rem(8); /* [8, 9, 9, 9] */
    add(3); /* [3, 8, 9, 9, 9] */
    rem(3); /* [8, 9, 9, 9] */
    add(2); /* [2, 8, 9, 9, 9] */
    rem(2); /* [8, 9, 9, 9] */
    add(7); /* [7, 8, 9, 9, 9] */
    rem(7); /* [8, 9, 9, 9] */
    add(9); /* [8, 9, 9, 9, 9] */
    rem(8); /* [9, 9, 9, 9] */
    add(5); /* [5, 9, 9, 9, 9] */
    rem(5); /* [9, 9, 9, 9] */
    add(0); /* [0, 9, 9, 9, 9] */
    rem(0); /* [9, 9, 9, 9] */
    add(2); /* [2, 9, 9, 9, 9] */
    rem(2); /* [9, 9, 9, 9] */
    add(8); /* [8, 9, 9, 9, 9] */
    rem(8); /* [9, 9, 9, 9] */
    add(8); /* [8, 9, 9, 9, 9] */
    rem(8); /* [9, 9, 9, 9] */
    rem(9); /* [9, 9, 9] */
    rem(9); /* [9, 9] */
    rem(9); /* [9] */
    rem(9); /* [] */
    rem(-1);

    bheap_free(bh);

    {
	int i;

	for (i = 0; i < 20; i++) {
	    if (!(coverage & (1 << i)))
		printf("coverage is missing %d\n", i);
	    else if (!(checked_coverage & (1 << i)))
		printf("checked_coverage is missing %d\n", i);
	}
    }

    printf("finished testing, no assertions failed\n");

    return 0;
}

#endif
