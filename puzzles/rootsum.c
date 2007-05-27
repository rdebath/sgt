/*
 * Search, reasonably efficiently, for tuples of non-square
 * integers whose square roots sum to a near-integer.
 * 
 * Inspired by a BASIC program posted on rec.puzzles by Ed Murphy,
 * 2007-05-26, message-ID <46579839$0$4696$4c368faf@roadrunner.com>.
 */

/*
 * gcc -I ../../library -o rootsum rootsum.c ../../library/tree234.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "tree234.h"

struct root {
    int n;
    double root;
    double fracpart;
};

double best = 1.0;

enum { BOTH, POSITIVE, NEGATIVE } mode = BOTH;

int fraccmp(void *av, void *bv)
{
    struct root *a = (struct root *)av;
    struct root *b = (struct root *)bv;
    if (a->fracpart < b->fracpart)
	return -1;
    else if (a->fracpart > b->fracpart)
	return +1;
    else if (a->n < b->n)
	return -1;
    else if (a->n > b->n)
	return +1;
    else
	return 0;
}

int fracsearch(void *av, void *bv)
{
    double a = *(double *)av;
    struct root *b = (struct root *)bv;
    if (a < b->fracpart)
	return -1;
    else if (a > b->fracpart)
	return +1;
    else
	return 0;
}

int ncmp(void *av, void *bv)
{
    struct root *a = (struct root *)av;
    struct root *b = (struct root *)bv;
    if (a->n < b->n)
	return -1;
    else if (a->n > b->n)
	return +1;
    else
	return 0;
}

tree234 *byn, *byfrac;

struct root *compute_root(int n)
{
    struct root *r = malloc(sizeof(struct root));
    double intpart;

    r->n = n;
    r->root = sqrt(n);
    r->fracpart = modf(r->root, &intpart);

    if ((int)intpart * (int)intpart == n) {
	/*
	 * Square integer. Ignore.
	 */
	free(r);
	return NULL;
    }

    add234(byfrac, r);
    add234(byn, r);

    return r;
}

void candidate(struct root **roots, int n, double x)
{
    double y;
    int i;

    x = modf(x, &y);
    y = 1.0 - x;
    if (mode == POSITIVE)
	/* leave x as it is */;
    else if (mode == NEGATIVE)
	x = y;
    else if (x > y)
	x = y;

    if (best > x) {
	double total = roots[0]->root;
	int done0 = 0;

	best = x;

	/*
	 * Output our new best result. We tweak this slightly to
	 * sort the numbers: 1 to n-1 will be in numerical order,
	 * but 0 needs slotting in somewhere.
	 */
	for (i = 1; i <= n; i++) {
	    if (!done0 && (i == n || roots[0]->n <= roots[i]->n)) {
		printf("%d ", roots[0]->n);
		done0 = 1;
	    }
	    if (i == n)
		break;
	    printf("%d ", roots[i]->n);
	    total += roots[i]->root;
	}
	printf("%.17g %.17g\n", total, x);
    }
}

void search(struct root **roots, int i, int n, double x)
{
    struct root *r;

    if (i > 0) {
	int j;
	for (j = 0;
	     (r = (struct root *)index234(byn, j)) != NULL &&
	     r->n <= roots[i+1]->n;
	     j++) {
	    roots[i] = r;
	    search(roots, i-1, n, x + r->fracpart);
	}
    } else {
	/*
	 * We have all but one of our roots, so we know what we'd
	 * like our fractional part to ideally be. So now we
	 * search within the `byfrac' tree to find our way
	 * immediately to the two best candidates (one on either
	 * side).
	 */
	int pos;
	double target, intpart;

	target = modf(n - x, &intpart);
	r = (struct root *)
	    findrelpos234(byfrac, &target, fracsearch, REL234_GE, &pos);

	/*
	 * There are several special cases:
	 * 
	 *  - if the search returned NULL, then there is no
	 *    element in the tree with a fractional part greater
	 *    than or equal to x. This means that our two
	 *    candidates are the last and first elements in the
	 *    tree.
	 * 
	 *  - if the search returned the element at position 0,
	 *    then _all_ elements in the tree have a fractional
	 *    part greater than or equal to x. This means, again,
	 *    that our two candidates are the last and first
	 *    elements in the tree.
	 * 
	 *  - otherwise, our two candidates are the element r, and
	 *    the element at index (pos-1).
	 */
	if (!r || pos == 0) {
	    r = index234(byfrac, 0);
	    pos = count234(byfrac);
	}
	roots[i] = r;
	candidate(roots, n, x + r->fracpart);
	roots[i] = r = index234(byfrac, pos-1);
	candidate(roots, n, x + r->fracpart);
    }
}

int main(int argc, char **argv)
{
    int nroots = 3;
    int maxn;
    struct root *r;
    struct root **roots;

    byn = newtree234(ncmp);
    byfrac = newtree234(fraccmp);

    while (--argc > 0) {
	char *p = *++argv;
	if (isdigit((unsigned char)*p))
	    nroots = atoi(p);
	else if (!strcmp(p, "-p"))
	    mode = POSITIVE;
	else if (!strcmp(p, "-n"))
	    mode = NEGATIVE;
    }

    roots = malloc(nroots * sizeof(struct root *));

    for (maxn = 2 ;; maxn++) {
	fprintf(stderr, " %d\r", maxn);
	fflush(stderr);
	r = compute_root(maxn);
	if (!r)
	    continue;
	roots[nroots-1] = r;
	search(roots, nroots-2, nroots, r->fracpart);
    }
}
