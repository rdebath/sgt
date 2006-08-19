/*
 * Implementation of enum.h.
 */

#include "enum.h"

int perm_count(int n)
{
    int ret = 1;
    int i;

    for (i = 2; i <= n; i++)
	ret *= i;

    return ret;
}

/*
 * Return the total number of possible (n,k) combinations, i.e. the
 * binomial coefficient n!/(k!(n-k)!).
 */
int comb_count(int n, int k)
{
    int ret, i;

    /*
     * If we actually tried to compute all those factorials and
     * divide them, we'd get integer overflow almost immediately.
     * Instead, we work along the relevant row of Pascal's
     * Triangle: we start with (n choose 1) == 1, and repeatedly
     * make use of the identity (n choose k+1) = (n-k)/(k+1) * (n
     * choose k).
     */

    /*
     * Never cross the centre line of Pascal's Triangle, or we risk
     * unnecessary integer overflow again in cases where (n choose
     * k) is nice and small but (n choose n/2) is huge. To avoid
     * this, we use the identity (n choose k) = (n choose n-k).
     */
    if (k+k > n)
	k = n - k;

    ret = 1;
    for (i = 0; i < k; i++)
	ret = (ret * (n-i)) / (i+1);

    return ret;
}

int perm_next(int *array, int n)
{
    int i, j, k;

    /*
     * To find the next permutation in order, we must first find
     * the last element of the array which is less than its
     * right-hand neighbour. If there isn't one, the array is
     * already in descending order and we return failure.
     */
    for (i = n-2; i >= 0; i--)
	if (array[i] < array[i+1])
	    break;
    if (i < 0)
	return 0;

    /*
     * Now we must increase array[i] by as little as possible,
     * which we do by finding the smallest array[j], j>i, greater
     * than array[i], and swapping it with array[i].
     * 
     * Since, by construction, array[i+1...n-1] is in decreasing
     * order, this means we find the largest j>i with
     * array[j]>array[i].
     * 
     * The swap preserves the descending order property.
     * 
     * (We could use a binary search to speed up this swap in large
     * cases, reducing the complexity of this step from O(n-i) to
     * O(log(n-i)), but there seems little point, because the
     * _overall_ complexity of the algorithm is necessarily O(n-i)
     * anyway due to the subsequent array reversal step.)
     */
    for (j = n-1; j > i; j--)
	if (array[j] > array[i]) {
	    int tmp = array[j];
	    array[j] = array[i];
	    array[i] = tmp;
	    break;
	}

    /*
     * Having done that, we now reorder all the elements after
     * array[i] into increasing order, since this is obviously the
     * lexicographically least order they can possibly be in. Since
     * they're currently in strict descending order, there's no
     * need to use a general sort function; we just reverse them.
     */
    for (j = i+1, k = n-1; j < k; j++, k--) {
	int tmp = array[j];
	array[j] = array[k];
	array[k] = tmp;
    }

    return 1;
}

void perm_first(int *array, int n)
{
    int i;

    for (i = 0; i < n; i++)
	array[i] = i;
}

int perm_to_index(int *array, int n)
{
    int ret = 0;
    int i, k;

    while (n > 0) {
	/*
	 * We're about to add a number to ret which is at most n-1,
	 * so make room for it.
	 */
	ret *= n;

	/*
	 * Determine where array[0] is in the overall order, by
	 * comparing it to all subsequent elements.
	 */
	for (i = 1; i < n; i++)
	    if (array[0] > array[i])
		ret++;

	/*
	 * Now we do the same processing to the rest of the array.
	 */
	array++;
	n--;
    }

    return ret;
}

void perm_from_index(int *array, int n, int index)
{
    int i, j, k;

    /*
     * FIXME: We could also do this in a top-down fashion: divide
     * index by (n-1)! to find out which element belonged at the
     * front, then move it into place by rotating array[0..i]
     * upwards by one, then replace index with the remainder from
     * the division and iterate.
     * 
     * That approach requires the array to already be filled with
     * the right things in the right order; the advantage of doing
     * it this way is not that it's a more efficient way to
     * implement perm_from_index as given here, but that it permits
     * us to implement a version of the function which permutes a
     * _provided_ ordered list.
     */

    for (i = 1; i <= n; i++) {
	j = index % i;
	index /= i;

	/*
	 * Place a number here in such a way that it ends up being
	 * the jth smallest.
	 */
	array[n-i] = j;
	for (k = n-i+1; k < n; k++)
	    if (array[k] >= array[n-i])
		array[k]++;
    }
}

int setcomb_next(char *array, int n, int k)
{
    /*
     * In most cases we find the rightmost set element and move it
     * right by one.
     * 
     * If this isn't possible because it's already at the far
     * right, we must find the _next_ rightmost element, move that
     * right by one, and reset the last one to just after it.
     * 
     * If that isn't possible either then we go for the next
     * rightmost still. So in the general case we find the
     * rightmost element which can be moved right at all, move it
     * right by one, and place all the subsequent elements
     * immediately after it.
     * 
     * If no element at all can be moved right, we are already at
     * the largest possible combination, so we return failure.
     */
    int i, right = 0;

    for (i = n-1; i >= 0; i--) {
	if (array[i]) {
	    right++;
	    if (i+1 < n && !array[i+1]) {
		/*
		 * We can move this element right. Do so, and reset
		 * all subsequent elements to their leftmost
		 * possible positions.
		 */
		int j;

		array[i] = 0;
		for (j = i+1; j < n; j++)
		    array[j] = (j - (i+1)) < right;

		return 1;
	    }
	}
    }

    /*
     * Failed.
     */
    return 0;
}

void setcomb_first(char *array, int n, int k)
{
    int i;

    for (i = 0; i < n; i++)
	array[i] = (i < k);
}

int setcomb_to_index(char *array, int n, int k)
{
    int ret = 0;
    int i, nck;

    /* nck equals n choose k; we adjust it gradually as we alter n and k. */
    nck = comb_count(n, k);

    while (1) {
	if (k == 0)
	    return ret;		       /* only one position now */

	if (*array) {
	    /*
	     * First element found.
	     */
	    nck = nck * k / n;
	    k--;
	} else {
	    /*
	     * First element not found, meaning we have just
	     * skipped over (n-1 choose k-1) positions.
	     */
	    ret += nck * k / n;
	    nck = nck * (n-k) / n;
	}
	array++;
	n--;
    }
}

void setcomb_from_index(char *array, int n, int k, int index)
{
    int i, nck;

    /* nck equals n choose k; we adjust it gradually as we alter n and k. */
    nck = comb_count(n, k);

    while (k > 0) {
	if (index < nck * k / n) {
	    /*
	     * First element is here.
	     */
	    *array = 1;
	    nck = nck * k / n;
	    k--;
	} else {
	    /*
	     * It isn't.
	     */
	    *array = 0;
	    index -= nck * k / n;
	    nck = nck * (n-k) / n;
	}
	array++;
	n--;
    }

    /*
     * Fill up the remainder of the array with zeroes.
     */
    while (n-- > 0)
	*array++ = 0;
}

int listcomb_next(int *array, int n, int k)
{
    /*
     * In most cases we increment the last list element.
     * 
     * If this isn't possible because it's already equal to n-1, we
     * must increment the _next_ to last element, and reset the
     * last one to one more than that.
     * 
     * If that isn't possible either then we go for the next one
     * back still. So in the general case we find the rightmost
     * list element array[i] which is not equal to i+n-k, increment
     * it, and set all the subsequent elements array[j] to
     * array[i]+j-i.
     * 
     * If no element at all can be safely incremented, we are
     * already at the largest possible combination, so we return
     * failure.
     */
    int i, right = 0;

    for (i = k-1; i >= 0; i--) {
	if (array[i] < i+n-k) {
	    int j;

	    array[i]++;
	    
	    for (j = i+1; j < n; j++)
		array[j] = array[j-1] + 1;

	    return 1;
	}
    }

    /*
     * Failed.
     */
    return 0;
}

void listcomb_first(int *array, int n, int k)
{
    int i;

    for (i = 0; i < k; i++)
	array[i] = i;
}

int listcomb_to_index(int *array, int n, int k)
{
    int ret = 0;
    int i, j, nck;

    /* nck equals n choose k; we adjust it gradually as we alter n and k. */
    nck = comb_count(n, k);

    j = 0;
    while (k > 0) {
	/*
	 * If the element here is not j, we must skip over (n-1
	 * choose k-1) positions in which it is.
	 * 
	 * FIXME: it would be nice if we could find a closed form
	 * for this and replace the while loop with some sort of
	 * binary search, thus rendering the algorithm O(k) rather
	 * than O(n). May not be possible, but worth thinking
	 * about.
	 */
	while (j++ < *array) {
	    ret += nck * k / n;
	    nck = nck * (n-k) / n;
	    n--;
	}

	/*
	 * Now we've found an element, so decrement both n and k.
	 */
	nck = nck * k / n;
	k--;
	array++;
	n--;
    }

    return ret;
}

void listcomb_from_index(int *array, int n, int k, int index)
{
    int ret = 0;
    int i, j, nck;

    /* nck equals n choose k; we adjust it gradually as we alter n and k. */
    nck = comb_count(n, k);

    j = 0;
    while (k > 0) {
	/*
	 * The element here is j in the first (n-1 choose k-1)
	 * positions, otherwise more than j.
	 * 
	 * FIXME: it would be nice if we could find a closed form
	 * for this and replace the while loop with some sort of
	 * binary search, thus rendering the algorithm O(k) rather
	 * than O(n). May not be possible, but worth thinking
	 * about.
	 */
	while (index >= nck * k / n) {
	    index -= nck * k / n;
	    nck = nck * (n-k) / n;
	    n--;
	    j++;
	}

	nck = nck * k / n;
	k--;
	*array++ = j++;
	n--;
    }
}

#ifdef UNIT_TEST

#include <stdio.h>

int ntests, nfails;

#define FAILIF(x,r,y) do { \
    if ( (x) r (y) ) { \
	printf("%d: FAIL: %s (%d) %s %s (%d)\n", __LINE__, #x, x, #r, #y, y); \
	nfails++; \
    } \
    ntests++; \
} while (0)

#define EQ(x,y) FAILIF(x,!=,y)
#define LT(x,y) FAILIF(x,>=,y)
#define LE(x,y) FAILIF(x,>,y)

#define PERM_NMAX 7
#define COMB_NMAX 12

void test_perm(int n)
{
    int a1[PERM_NMAX], a2[PERM_NMAX], aprev[PERM_NMAX], count[PERM_NMAX];
    int i, j, index, eindex;

#ifndef QUIET
    printf("perm enumeration tests, n=%d\n", n);
#endif

    eindex = 0;
    perm_first(a1, n);
    do {

	index = perm_to_index(a1, n);
	perm_from_index(a2, n, index);

#ifndef QUIET
	{
	    const char *sep;
	    putchar('{');
	    sep = "";
	    for (i = 0; i < n; i++) {
		printf("%s%d", sep, a1[i]);
		sep = ",";
	    }
	    printf("} -> %d -> {", index);
	    sep = "";
	    for (i = 0; i < n; i++) {
		printf("%s%d", sep, a2[i]);
		sep = ",";
	    }
	    printf("}\n");
	}
#endif

	/*
	 * Check that a1 contains the numbers 0..n-1 in some order.
	 */
	for (i = 0; i < n; i++)
	    count[i] = 0;
	for (i = 0; i < n; i++) {
	    LE(0, a1[i]);
	    LT(a1[i], n);
	    EQ(count[i], 0);
	    count[i]++;
	}

	/*
	 * Check that a1 is lexicographically strictly greater than
	 * the previous permutation, if any.
	 */
	if (eindex > 0) {
	    for (i = 0; i < n; i++) {
		LE(aprev[i], a1[i]);
		if (aprev[i] < a1[i])
		    break;
	    }
	    LT(i, n);
	}
	memcpy(aprev, a1, sizeof(*a1) * n);

	/*
	 * Check that perm_to_index got the answer right.
	 */
	EQ(index, eindex);

	/*
	 * And perm_from_index.
	 */
	EQ(memcmp(a2, a1, sizeof(*a1)*n), 0);

	eindex++;
    } while (perm_next(a1, n));

#ifndef QUIET
    printf("perm discontinuous tests, n=%d\n", n);
#endif

    /*
     * Now do a bunch of these tests again, with the input
     * permutation _not_ being simple increasing integers.
     */
    eindex = 0;
    for (i = 0; i < n; i++)
	a1[i] = i + i*i + i*i*i;

    do {

	index = perm_to_index(a1, n);

#ifndef QUIET
	{
	    const char *sep;
	    putchar('{');
	    sep = "";
	    for (i = 0; i < n; i++) {
		printf("%s%d", sep, a1[i]);
		sep = ",";
	    }
	    printf("} -> %d\n", index);
	}
#endif

	/*
	 * Check that a1 contains the right numbers in some order.
	 */
	for (i = 0; i < n; i++)
	    count[i] = 0;
	for (i = 0; i < n; i++) {
	    for (j = 0; j < n; j++)
		if (a1[i] == j + j*j + j*j*j)
		    break;
	    LT(j, n);
	    EQ(count[j], 0);
	    count[j]++;
	}

	/*
	 * Check that a1 is lexicographically strictly greater than
	 * the previous permutation, if any.
	 */
	if (eindex > 0) {
	    for (i = 0; i < n; i++) {
		LE(aprev[i], a1[i]);
		if (aprev[i] < a1[i])
		    break;
	    }
	    LT(i, n);
	}
	memcpy(aprev, a1, sizeof(*a1) * n);

	/*
	 * Check that perm_to_index got the answer right.
	 */
	EQ(index, eindex);

	eindex++;
    } while (perm_next(a1, n));

    EQ(eindex, perm_count(n));
}

void test_setcomb(int n)
{
    char a1[COMB_NMAX], a2[COMB_NMAX];
    int indices[COMB_NMAX+1];
    int i, j, k, index;

#ifndef QUIET
    printf("setcomb round-trip index tests, n=%d\n", n);
#endif

    /*
     * Enumerate all 2^n possible subsets of {0,...,n-1}, and check
     * that setcomb_to_index() translates each one correctly to a
     * combination index, that setcomb_from_index() translates each
     * one correctly back again, and that the indices are in the
     * expected order.
     */
    for (i = 0; i <= n; i++)
	indices[i] = comb_count(n, i) - 1;

    for (i = 0; i < (1 << n); i++) {

	k = 0;
	for (j = 0; j < n; j++) {
	    a1[j] = (i & (1 << (n-1-j))) != 0;
	    k += a1[j] != 0;
	}

	index = setcomb_to_index(a1, n, k);
	setcomb_from_index(a2, n, k, index);

#ifndef QUIET
	for (j = 0; j < n; j++)
	    putchar(a1[j] ? '#' : '-');
	printf(" (k =%3d) ->%5d -> ", k, index);
	for (j = 0; j < n; j++)
	    putchar(a2[j] ? '#' : '-');
	putchar('\n');
#endif

	EQ(index, indices[k]);
	indices[k]--;

	EQ(memcmp(a1, a2, n), 0);
    }

    /*
     * Now enumerate using setcomb_first() and setcomb_next(), and
     * verify that we get the expected number of combinations, that
     * each combination is valid, and that the indices are also as
     * expected.
     */
    for (k = 0; k <= n; k++) {
	index = 0;

#ifndef QUIET
	printf("setcomb enumeration tests, n=%d, k=%d\n", n, k);
#endif

	setcomb_first(a1, n, k);
	do {
	    for (i = j = 0; j < n; j++)
		i += a1[j] != 0;
	    EQ(i, k);

	    i = setcomb_to_index(a1, n, k);

#ifndef QUIET
	    for (j = 0; j < n; j++)
		putchar(a1[j] ? '#' : '-');
	    printf(" ->%5d\n", i);
#endif

	    EQ(index, i);
	    index++;
	} while (setcomb_next(a1, n, k));

	i = comb_count(n, k);
	EQ(index, i);
    }
}

void test_listcomb(int n)
{
    char set[COMB_NMAX];
    int list1[COMB_NMAX], list2[COMB_NMAX];
    int i, j, k, index, eindex;

    for (k = 0; k <= n; k++) {

#ifndef QUIET
	printf("listcomb enumeration tests, n=%d, k=%d\n", n, k);
#endif

	/*
	 * Enumerate the combinations using listcomb_first and
	 * listcomb_next. For each one, check that it's valid,
	 * check that listcomb_to_index returns the expected value,
	 * check that listcomb_from_index reconstructs the same
	 * combination, and check that the listcomb and setcomb
	 * functions are describing the same combination.
	 */
	eindex = 0;
	listcomb_first(list1, n, k);
	do {
	    /*
	     * Validity check: 0 <= list1[0] < ... < list1[k-1] < n.
	     */
	    for (i = 0; i <= k; i++) {
		int lower = (i == 0 ? -1 : list1[i-1]);
		int upper = (i == k ? n : list1[i]);
		LT(lower, upper);
	    }

	    /*
	     * Verify that listcomb_to_index gives the right answer.
	     */
	    index = listcomb_to_index(list1, n, k);

	    /*
	     * And that it comes back again properly.
	     */
	    listcomb_from_index(list2, n, k, index);

	    /*
	     * Verify against setcomb.
	     */
	    setcomb_from_index(set, n, k, eindex);

#ifndef QUIET
	    {
		const char *sep;
		putchar('{');
		sep = "";
		for (i = 0; i < k; i++) {
		    printf("%s%d", sep, list1[i]);
		    sep = ",";
		}
		printf("} -> %d -> {", index);
		sep = "";
		for (i = 0; i < k; i++) {
		    printf("%s%d", sep, list2[i]);
		    sep = ",";
		}
		printf("} [");
		for (i = 0; i < n; i++)
		    putchar(set[i] ? '#' : '-');
		printf("]\n");
	    }
#endif

	    EQ(index, eindex);

	    EQ(memcmp(list1, list2, sizeof(*list1) * k), 0);

	    for (i = j = 0; i < n; i++)
		if (set[i]) {
		    EQ(i, list1[j]);
		    j++;
		}
	    EQ(j, k);		       /* must have gone through all indices */

	    eindex++;

	} while (listcomb_next(list1, n, k));
    }
}

int main(void)
{
    int n;

    for (n = 0; n <= PERM_NMAX; n++) {
	test_perm(n);
    }

    for (n = 0; n <= COMB_NMAX; n++) {
	test_setcomb(n);
	test_listcomb(n);
    }

    printf("Failed %d / %d tests\n", nfails, ntests);
    return 0;
}

#endif
