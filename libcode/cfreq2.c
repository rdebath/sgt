#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Functions to manage cumulative frequency tables using the
 * alternative bitwise array indexing approach.
 */

/*
 * This file is copyright 2001,2005 Simon Tatham.
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

/*
 * Initialise a cumulative frequency table. (Hardly worth writing
 * this function; all it does is to initialise everything in the
 * array to zero.)
 */
void cf_init(int *table, int n)
{
    int i;

    for (i = 0; i < n; i++)
	table[i] = 0;
}

/*
 * Increment the count of symbol `sym' by `count'.
 */
void cf_add(int *table, int n, int sym, int count)
{
    int bit;

    bit = 1;
    while (sym != 0) {
	if (sym & bit) {
	    table[sym] += count;
	    sym &= ~bit;
	}
	bit <<= 1;
    }

    table[0] += count;
}

/*
 * Cumulative frequency lookup: return the total count of symbols
 * with value less than `sym'.
 */
int cf_clookup(int *table, int n, int sym)
{
    int bit, index, limit, count;

    if (sym == 0)
	return 0;

    assert(0 < sym && sym <= n);

    count = table[0];		       /* start with the whole table size */

    bit = 1;
    while (bit < n)
	bit <<= 1;

    limit = n;

    while (bit > 0) {
	/*
	 * Find the least number with its lowest set bit in this
	 * position which is greater than or equal to sym.
	 */
	index = ((sym + bit - 1) &~ (bit * 2 - 1)) + bit;

	if (index < limit) {
	    count -= table[index];
	    limit = index;
	}

	bit >>= 1;
    }

    return count;
}

/*
 * Single frequency lookup: return the count of symbol `sym'.
 */
int cf_slookup(int *table, int n, int sym)
{
    int count, bit;

    assert(0 <= sym && sym < n);

    count = table[sym];

    for (bit = 1; sym+bit < n && !(sym & bit); bit <<= 1)
	count -= table[sym+bit];

    return count;
}

/*
 * Return the largest symbol index such that the cumulative
 * frequency up to that symbol is less than _or equal to_ count.
 */
int cf_whichsym(int *table, int n, int count) {
    int bit, sym, top;

    assert(count >= 0 && count < table[0]);

    bit = 1;
    while (bit < n)
	bit <<= 1;

    sym = 0;
    top = table[0];

    while (bit > 0) {
	if (sym+bit < n) {
	    if (count >= top - table[sym+bit])
		sym += bit;
	    else
		top -= table[sym+bit];
	}

	bit >>= 1;
    }

    return sym;
}

int main(void)
{
    /*
     * To test this, we create a sequence of cumulative frequency
     * tables with sizes 1 to 16; in each table, we set each single
     * frequency to a different power of three. Then there's
     * absolutely no way any cumulative frequency can be mistaken
     * for any other; so we can look them all up and test them
     * individually.
     */

    int i, j, n, size, ret;
    int cf[16];			       /* a simple frequency table */
    int a[16];			       /* a simple frequency table */
    int total;

    for (size = 1; size <= 16; size++) {
	printf("trying table size %d\n", size);

	cf_init(cf, size);

	for (i = 0, n = 1; i < size; i++, n *= 3) {
	    a[i] = n;
	    cf_add(cf, size, i, n);
	}

	/*
	 * That's set it up. Now loop over all possible cumulative
	 * frequencies.
	 */
	for (i = 0; i <= size; i++) {
	    /* Compute the right answer using a. */
	    for (j = n = 0; j < i; j++)
		n += a[j];
	    /* Now test it against the cfreq functions. */
	    ret = cf_clookup(cf, size, i);
	    printf("cumulative %2d: %10d  should be %10d\n", i, ret, n);
	    assert(ret == n);
	}

	/*
	 * Also, test each single-frequency lookup.
	 */
	for (i = 0; i < size; i++) {
	    ret = cf_slookup(cf, size, i);
	    printf("    single %2d: %10d  should be %10d\n", i, ret, a[i]);
	    assert(ret == a[i]);
	}

	/*
	 * Finally, test all possible frequency->symbol transitions.
	 */
	j = 0;
	for (i = 0; i < size; i++) {
	    ret = cf_whichsym(cf, size, j);
	    printf("    freq->sym %10d: %2d  should be %2d\n", j, ret, i);
	    assert(ret == i);
	    j += a[i] - 1;
	    ret = cf_whichsym(cf, size, j);
	    printf("    freq->sym %10d: %2d  should be %2d\n", j, ret, i);
	    assert(ret == i);
	    j++;
	}
    }

    return 0;
}
