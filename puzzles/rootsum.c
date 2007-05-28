/*
 * Search, reasonably efficiently, for tuples of non-square
 * integers whose square roots sum to a near-integer.
 * 
 * Inspired by a BASIC program posted on rec.puzzles by Ed Murphy,
 * 2007-05-26, message-ID <46579839$0$4696$4c368faf@roadrunner.com>.
 */

/*
 * gcc -I ../../library -o rootsum rootsum.c ../../library/tree234.c -lm
 * gcc -DGMP -I ../../library -o rootsum rootsum.c ../../library/tree234.c -lm -lgmp
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "tree234.h"

#ifdef GMP
#include "gmp.h"
#define DOUBLE mpf_t
#define SQRT(x,y) mpf_sqrt(x,y)
#define SET(x,y) mpf_set(x,y)
#define INIT(x) mpf_init(x)
#define CLEAR(x) mpf_clear(x)
#define SET_UI(x,y) mpf_set_ui(x,y)
#define MODF(x,i,y) (mpf_floor(i,y), mpf_sub(x,y,i))
#define MUL(r,x,y) mpf_mul(r,x,y)
#define SUB(r,x,y) mpf_sub(r,x,y)
#define ADD(r,x,y) mpf_add(r,x,y)
#define CMP_SI(x,r,y) (mpf_cmp_si(x,y) r 0)
#define CMP(x,r,y) (mpf_cmp(x,y) r 0)
#define TO_DOUBLE(x) mpf_get_d(x)
#define TO_INT(x) mpf_get_si(x)
#define SETPREC(x) mpf_set_default_prec(x)
#else
#define DOUBLE double
#define SQRT(x,y) ((x) = sqrt((y)))
#define SET(x,y) ((x) = (y))
#define INIT(x) ((void)0)
#define CLEAR(x) ((void)0)
#define SET_UI(x,y) ((x)=(y))
#define MODF(x,i,y) ((x) = modf(y,&(i)))
#define MUL(r,x,y) ((r)=(x)*(y))
#define SUB(r,x,y) ((r)=(x)-(y))
#define ADD(r,x,y) ((r)=(x)+(y))
#define CMP_SI(x,r,y) ((x) r (y))
#define CMP(x,r,y) ((x) r (y))
#define TO_DOUBLE(x) (x)
#define TO_INT(x) ((int)(x))
#define SETPREC(x) ((void)0)
#endif

struct root {
    int n;
    DOUBLE root;
    DOUBLE fracpart;
};

DOUBLE best;

enum { BOTH, POSITIVE, NEGATIVE } mode = BOTH;

int fraccmp(void *av, void *bv)
{
    struct root *a = (struct root *)av;
    struct root *b = (struct root *)bv;
    if (CMP(a->fracpart,<,b->fracpart))
	return -1;
    else if (CMP(a->fracpart,>,b->fracpart))
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
    DOUBLE *a = (DOUBLE *)av;
    struct root *b = (struct root *)bv;
    if (CMP(a[0],<,b->fracpart))
	return -1;
    else if (CMP(a[0],>,b->fracpart))
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
    DOUBLE tmp;
    int rt;

    INIT(tmp);

    r->n = n;
    INIT(r->root);
    SET_UI(tmp, n);
    SQRT(r->root, tmp);
    INIT(r->fracpart);
    MODF(r->fracpart, tmp, r->root);
    rt = TO_INT(tmp);

    if (rt * rt == n) {
	/*
	 * Square integer. Ignore.
	 */
	free(r);
	CLEAR(tmp);
	return NULL;
    }

    add234(byfrac, r);
    add234(byn, r);

    CLEAR(tmp);

    return r;
}

void candidate(struct root **roots, int n, DOUBLE x)
{
    DOUBLE y;
    int i;

    INIT(y);
    MODF(x,y,x);
    SET_UI(y,1);
    SUB(y,y,x);

    if (mode == POSITIVE)
	/* leave x as it is */;
    else if (mode == NEGATIVE)
	SET(x,y);
    else if (CMP(x,>,y))
	SET(x,y);

    if (CMP(best,>,x)) {
	DOUBLE total;
	int done0 = 0;

	INIT(total);
	SET(total, roots[0]->root);

	SET(best, x);

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
	    ADD(total, total, roots[i]->root);
	}
	printf("%.17g %.17g\n", TO_DOUBLE(total), TO_DOUBLE(x));

	CLEAR(total);
    }

    CLEAR(y);
}

void search(struct root **roots, int i, int n, DOUBLE x)
{
    struct root *r;

    if (i > 0) {
	int j;
	DOUBLE newx;

	INIT(newx);

	for (j = 0;
	     (r = (struct root *)index234(byn, j)) != NULL &&
	     r->n <= roots[i+1]->n;
	     j++) {
	    roots[i] = r;
	    ADD(newx, x, r->fracpart);
	    search(roots, i-1, n, newx);
	}

	CLEAR(newx);
    } else {
	/*
	 * We have all but one of our roots, so we know what we'd
	 * like our fractional part to ideally be. So now we
	 * search within the `byfrac' tree to find our way
	 * immediately to the two best candidates (one on either
	 * side).
	 */
	int pos;
	DOUBLE target, intpart;

	INIT(target);
	INIT(intpart);
	SET_UI(target, n);
	SUB(target, target, x);
	MODF(target, intpart, target);
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
	ADD(target, x, r->fracpart);
	candidate(roots, n, target);
	roots[i] = r = index234(byfrac, pos-1);
	ADD(target, x, r->fracpart);
	candidate(roots, n, target);

	CLEAR(target);
	CLEAR(intpart);
    }
}

int main(int argc, char **argv)
{
    int nroots = 3;
    int maxn;
    struct root *r;
    struct root **roots;

    SETPREC(256);

    INIT(best);
    SET_UI(best, 1);

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
