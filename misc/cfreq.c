#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Functions to manage cumulative frequency tables.
 */

typedef struct {
    int n;			       /* number of largest array */
    				       /* (ie 1 less than number of arrays) */
    int nsyms;			       /* number of distinct symbols */
    int **arrays;
} *CFreq;

/*
 * Create a cumulative frequency table for a given number of symbols.
 */
CFreq cfreq_new(int nsyms) {
    int i, j, n, pwr;
    int **arrays;
    CFreq ret;

    /*
     * Allocate the container structure.
     */
    ret = (CFreq) malloc(sizeof(*ret));
    ret->nsyms = nsyms;

    /*
     * First find the next power of two >= nsyms.
     */
    for (i=0; (1<<i) < nsyms; i++);
    ret->n = i;

    /*
     * Now we need one more than that many arrays.
     */
    ret->arrays = (int **)malloc(sizeof(int *) * (ret->n+1));

    /*
     * Now create each actual array. Array i has size
     * (nsyms-1)/2^(i+1)+1.
     */
    for (i=0; i <= ret->n; i++) {
	int size = (nsyms-1) / (1<<(i+1)) + 1;
	ret->arrays[i] = (int *)malloc(sizeof(int) * size);
	for (j = 0; j < size; j++)
	    ret->arrays[i][j] = 0;
    }

    /*
     * Done.
     */
    return ret;
}

/*
 * Destroy a cumulative frequency table.
 */
void cfreq_free(CFreq c) {
    int i;

    assert(c != NULL);

    for (i = 0; i <= c->n; i++)
	free(c->arrays[i]);
    free(c->arrays);
    free(c);
}

/*
 * Increment the count of symbol `sym' by `count'.
 */
int cfreq_increment(CFreq c, int sym, int count) {
    int i, j;

    assert(0 <= sym && sym < c->nsyms);

    for (i = 0; i <= c->n; i++) {
	if (sym & (1<<i))
	    continue;
	j = sym >> (i+1);
	c->arrays[i][j] += count;
    }
}

/*
 * Cumulative frequency lookup: return the total count of symbols
 * with value less than `sym'.
 */
int cfreq_clookup(CFreq c, int sym) {
    int i, j, count;

    assert(0 <= sym && sym <= c->nsyms);

    /* Special case: the total count in the whole table. */
    if (sym == c->nsyms)
	return c->arrays[c->n][0];

    count = 0;
    for (i = 0; i <= c->n; i++) {
	if (!(sym & (1<<i)))
	    continue;
	j = sym >> (i+1);
	count += c->arrays[i][j];
    }
    return count;
}

/*
 * Single frequency lookup: return the count of symbol `sym'.
 */
int cfreq_slookup(CFreq c, int sym) {
    int i, j, count, e;

    assert(0 <= sym && sym < c->nsyms);

    count = c->arrays[c->n][0];
    for (i = c->n-1; i >= 0; i--) {
	j = sym >> (i+1);
	e = c->arrays[i][j];
	if (sym & (1<<i))
	    count = count - e;
	else
	    count = e;
    }
    return count;
}

/*
 * Return a symbol index given a cumulative frequency.
 */
int cfreq_whichsym(CFreq c, int n) {
    int ch, i, j;

    assert(0 <= n && n < c->arrays[c->n][0]);

    ch = 0;
    for (i = c->n-1; i >= 0; i--) {
	j = c->arrays[i][ch];
	if (n < j) {
	    ch = ch * 2;
	} else {
	    ch = ch * 2 + 1;
	    n = n - j;
	}
    }
    return ch;
}

int main(void) {
    /*
     * To test this, we create a cumulative frequency table of 13
     * elements (it's a good number with a varied bit pattern), and
     * set each single frequency to a different power of three.
     * Then there's absolutely no way any cumulative frequency can
     * be mistaken for any other; so we can look them all up and
     * test them individually.
     */

    CFreq c;
    int i, j, n, ret;
    int a[13];			       /* a simple frequency table */
    int total;

    c = cfreq_new(13);
    for (i = 0, n = 1; i < 13; i++, n *= 3) {
	a[i] = n;
	cfreq_increment(c, i, n);
    }

    /*
     * That's set it up. Now loop over all possible cumulative
     * frequencies.
     */
    for (i = 0; i <= 13; i++) {
	/* Compute the right answer using a. */
	for (j = n = 0; j < i; j++)
	    n += a[j];
	/* Now test it against the cfreq functions. */
	ret = cfreq_clookup(c, i);
	printf("cumulative %2d: %10d  should be %10d\n", i, ret, n);
	assert(ret == n);
    }

    /*
     * Also, test each single-frequency lookup.
     */
    for (i = 0; i < 13; i++) {
	ret = cfreq_slookup(c, i);
	printf("    single %2d: %10d  should be %10d\n", i, ret, a[i]);
	assert(ret == a[i]);
    }

    /*
     * Finally, test all possible frequency->symbol transitions.
     */
    j = 0;
    for (i = 0; i < 13; i++) {
	ret = cfreq_whichsym(c, j);
	printf("    freq->sym %10d: %2d  should be %2d\n", j, ret, i);
	j += a[i] - 1;
	ret = cfreq_whichsym(c, j);
	printf("    freq->sym %10d: %2d  should be %2d\n", j, ret, i);
	j++;
    }

    return 0;
}
