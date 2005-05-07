/*
 * Map a group by breadth-first search given some generators, to
 * find the shortest path back to the identity for every element.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define lenof(x) ( sizeof(x) / sizeof(*(x)) )

/*
 * This structure describes a group type. It can expect to be the
 * first element of a larger structure containing parameters.
 * (Thus, the methods defined here may cast from the `struct group'
 * back to its containing structure.)
 */
struct group {
    /*
     * Return the size in bytes of the natural form of a group
     * element.
     */
    int (*eltsize)(struct group *gctx);
    /*
     * Return the natural form of the starting (usually identity)
     * group element.
     */
    void (*identity)(struct group *gctx, void *velt);
    /*
     * Give an integer index for the natural form of a group
     * element.
     */
    int (*index)(struct group *gctx, void *velt);
    /*
     * Turn an integer index back into the natural element form.
     */
    void (*fromindex)(struct group *gctx, void *velt, int index);
    /*
     * Find the maximum index ever used by the above routines.
     * (Actually, the maximum plus 1, in conventional C idiom.)
     */
    int (*maxindex)(struct group *gctx);
    /*
     * Print out an element.
     */
    void (*printelt)(struct group *gctx, void *velt);
    /*
     * Parse an element from string form. Return NULL on success,
     * or a message describing the parse error.
     */
    char *(*parseelt)(struct group *gctx, void *velt, char *text);
    /*
     * Return the number of available moves (generators) in the
     * group. Guaranteed to be called before makemove(), so it may
     * initialise things.
     */
    int (*moves)(struct group *gctx);
    /*
     * Make move number n on a given element.
     */
    void (*makemove)(struct group *gctx, void *velt, int n);
    /*
     * Return the name of move number n.
     */
    char *(*movename)(struct group *gctx, int n);
};

/* ----------------------------------------------------------------------
 * Group parameter structure which deals with groups that are
 * basically permutations.
 */
struct perm {
    struct group vtable;
    int n;
    int nmoves;
    char *movenames;
    int **moves;
};


int perm_eltsize(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;

    return ctx->n * sizeof(int);
}

void perm_identity(struct group *gctx, void *velt)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->n; i++)
	elt[i] = i;
}

int perm_index(struct group *gctx, void *velt)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret;

    ret = 0;
    mult = ctx->n;
    for (i = 0; i < ctx->n-1; i++) {
	int k = 0;
	for (j = 0; j < i; j++)
	    if (elt[j] < elt[i])
		k++;
	ret = ret * mult + elt[i] - k;
	mult--;
    }

    return ret;
}

void perm_fromindex(struct group *gctx, void *velt, int index)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret;

    ret = 0;
    elt[ctx->n-1] = 0;
    mult = 2;
    for (i = ctx->n-1; i-- ;) {
	elt[i] = index % mult;
	index /= mult;
	for (j = i+1; j < ctx->n; j++)
	    if (elt[j] >= elt[i])
		elt[j]++;
	mult++;
    }
}

int perm_maxindex(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;
    int f, i;

    f = 1;
    for (i = 1; i <= ctx->n; i++)
	f *= i;

    return f;
}

void perm_printelt(struct group *gctx, void *velt)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->n; i++)
        printf("%s%d", i ? "," : "", elt[i]+1);
}

char *perm_parseelt(struct group *gctx, void *velt, char *s)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int i, j;

    for (i = 0; i < ctx->n; i++) {
        while (*s && !isdigit((unsigned char)*s)) s++;
        if (!*s)
            return "Not enough numbers in list";
        elt[i] = atoi(s) - 1;
        while (*s && isdigit((unsigned char)*s)) s++;
    }

    for (i = 0; i < ctx->n; i++) {
        for (j = 0; j < ctx->n; j++)
            if (elt[i] == j)
                break;
        if (j == ctx->n)
            return "Numbers were not a permutation";
    }

    return NULL;
}

int perm_moves(struct group *gctx)
{
    struct perm *ctx = (struct perm *)gctx;
    return ctx->nmoves;
}

void perm_makemove(struct group *gctx, void *velt, int n)
{
    struct perm *ctx = (struct perm *)gctx;
    int *elt = (int *)velt;
    int *i = ctx->moves[n];
    int tmp;

    while (*i >= 0) {
	tmp = elt[i[0]];
	while (i[1] >= 0) {
	    elt[i[0]] = elt[i[1]];
	    i++;
	}
	elt[i[0]] = tmp;
	i++;
	/* now i points at the -1 terminating a cycle description */
	i++;
	/* now i points at the _next_ cycle description, or a terminating -1 */
    }
}

char *perm_movename(struct group *gctx, int n)
{
    struct perm *ctx = (struct perm *)gctx;
    char *c = ctx->movenames;
    while (n > 0) {
        c += 1+strlen(c);
        n--;
    }
    return c;
}

int twiddle3x3_movedata[] = {
    0,1,4,3,-1,-1,
    1,2,5,4,-1,-1,
    3,4,7,6,-1,-1,
    4,5,8,7,-1,-1,
    0,3,4,1,-1,-1,
    1,4,5,2,-1,-1,
    3,6,7,4,-1,-1,
    4,7,8,5,-1,-1,
};

int *twiddle3x3_moves[] = {
    twiddle3x3_movedata + 0 * 6,
    twiddle3x3_movedata + 1 * 6,
    twiddle3x3_movedata + 2 * 6,
    twiddle3x3_movedata + 3 * 6,
    twiddle3x3_movedata + 4 * 6,
    twiddle3x3_movedata + 5 * 6,
    twiddle3x3_movedata + 6 * 6,
    twiddle3x3_movedata + 7 * 6,
};

struct perm twiddle3x3 = {
    {perm_eltsize, perm_identity, perm_index, perm_fromindex, perm_maxindex,
     perm_printelt, perm_parseelt, perm_moves, perm_makemove, perm_movename},
    9,
    8, "a\0b\0c\0d\0A\0B\0C\0D", twiddle3x3_moves
};

/* ----------------------------------------------------------------------
 * Group parameter structure which deals with 3x3 orientable
 * Twiddle.
 */
struct otwid {
    struct group vtable;
    int w, n;
    int nmoves;
    char *movenames;
    int **moves;
};

int otwid_eltsize(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;

    return ctx->n * sizeof(int);
}

void otwid_identity(struct group *gctx, void *velt)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->n; i++)
	elt[i] = i*4;
}

int otwid_index(struct group *gctx, void *velt)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret;

    /*
     * Index the permutation.
     */
    ret = 0;
    mult = ctx->n;
    for (i = 0; i < ctx->n-1; i++) {
	int k = 0;
	for (j = 0; j < i; j++)
	    if (elt[j] < elt[i])
		k++;
	ret = ret * mult + elt[i]/4 - k;
	mult--;
    }

    /*
     * Now index the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, o, sp, dp, p;

        e = elt[i] / 4;
        o = elt[i] & 3;
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        assert(p == (o & 1));
        o -= p;
        ret = ret * 2 + (o >> 1);
    }

    return ret;
}

void otwid_fromindex(struct group *gctx, void *velt, int index)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j, mult, ret, orients;

    orients = index;
    index >>= ctx->n;

    /*
     * Construct the permutation.
     */
    ret = 0;
    elt[ctx->n-1] = 0;
    mult = 2;
    for (i = ctx->n-1; i-- ;) {
	elt[i] = index % mult;
	index /= mult;
	for (j = i+1; j < ctx->n; j++)
	    if (elt[j] >= elt[i])
		elt[j]++;
	mult++;
    }

    /*
     * Now set up the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, sp, dp, p;

        e = elt[i];
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        elt[i] *= 4;
        elt[i] += p & 1;
        if (orients & (1 << (ctx->n-1-i)))
            elt[i] += 2;
    }
}

int otwid_maxindex(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int f, i;

    f = 1;
    for (i = 1; i <= ctx->n; i++)
	f *= i;

    return f << ctx->n;
}

void otwid_printelt(struct group *gctx, void *velt)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i;

    for (i = 0; i < ctx->n; i++)
        printf("%d%c", elt[i]/4+1, "uldr"[elt[i] & 3]);
}

char *otwid_parseelt(struct group *gctx, void *velt, char *s)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int i, j;

    for (i = 0; i < ctx->n; i++) {
        while (*s && !isdigit((unsigned char)*s)) s++;
        if (!*s)
            return "Not enough numbers in list";
        elt[i] = 4 * (atoi(s) - 1);
        while (*s && isdigit((unsigned char)*s)) s++;
        if (*s == 'u')
            /* do nothing */;
        else if (*s == 'l')
            elt[i]++;
        else if (*s == 'd')
            elt[i] += 2;
        else if (*s == 'r')
            elt[i] += 3;
    }

    for (i = 0; i < ctx->n; i++) {
        for (j = 0; j < ctx->n; j++)
            if (elt[i]/4 == j)
                break;
        if (j == ctx->n)
            return "Numbers were not a permutation";
    }

    /*
     * Now index the orientations.
     */
    for (i = 0; i < ctx->n; i++) {
        /*
         * We expect each tile to have an orientation parity equal
         * to the parity of its Manhattan distance from its correct
         * position. That is, a tile can only be pointing up or
         * down if it's on the same chessboard colour as its
         * correct square, and can only be pointing left or right
         * if it's on the wrong colour.
         */
        int e, o, sp, dp, p;

        e = elt[i] / 4;
        o = elt[i] & 3;
        sp = ((i%ctx->w) ^ (i/ctx->w)) & 1;
        dp = ((e%ctx->w) ^ (e/ctx->w)) & 1;
        p = sp ^ dp;
        if (p != (o & 1))
            return "Orientations were inconsistent with positions";
    }

    return NULL;
}

int otwid_moves(struct group *gctx)
{
    struct otwid *ctx = (struct otwid *)gctx;
    return ctx->nmoves;
}

void otwid_makemove(struct group *gctx, void *velt, int n)
{
    struct otwid *ctx = (struct otwid *)gctx;
    int *elt = (int *)velt;
    int *i = ctx->moves[n];
    int rot = *i++;
    int tmp;

    while (*i >= 0) {
	tmp = elt[i[0]];
	while (i[1] >= 0) {
	    elt[i[0]] = elt[i[1]];
            elt[i[0]] ^= (elt[i[0]] ^ (elt[i[0]] + rot)) & 3;
	    i++;
	}
	elt[i[0]] = tmp;
        elt[i[0]] ^= (elt[i[0]] ^ (elt[i[0]] + rot)) & 3;
	i++;
	/* now i points at the -1 terminating a cycle description */
	i++;
	/* now i points at the _next_ cycle description, or a terminating -1 */
    }
}

char *otwid_movename(struct group *gctx, int n)
{
    struct otwid *ctx = (struct otwid *)gctx;
    char *c = ctx->movenames;
    while (n > 0) {
        c += 1+strlen(c);
        n--;
    }
    return c;
}

int otwiddle3x3_movedata[] = {
    1,0,1,4,3,-1,-1,
    1,1,2,5,4,-1,-1,
    1,3,4,7,6,-1,-1,
    1,4,5,8,7,-1,-1,
    3,0,3,4,1,-1,-1,
    3,1,4,5,2,-1,-1,
    3,3,6,7,4,-1,-1,
    3,4,7,8,5,-1,-1,
};

int *otwiddle3x3_moves[] = {
    otwiddle3x3_movedata + 0 * 7,
    otwiddle3x3_movedata + 1 * 7,
    otwiddle3x3_movedata + 2 * 7,
    otwiddle3x3_movedata + 3 * 7,
    otwiddle3x3_movedata + 4 * 7,
    otwiddle3x3_movedata + 5 * 7,
    otwiddle3x3_movedata + 6 * 7,
    otwiddle3x3_movedata + 7 * 7,
};

struct otwid otwiddle3x3 = {
    {otwid_eltsize, otwid_identity, otwid_index, otwid_fromindex, otwid_maxindex,
     otwid_printelt, otwid_parseelt, otwid_moves, otwid_makemove, otwid_movename},
    3, 9,
    8, "a\0b\0c\0d\0A\0B\0C\0D", otwiddle3x3_moves
};

/* ----------------------------------------------------------------------
 * Command-line processing.
 */

struct namedgroup {
    char *name;
    struct group *gp;
} groups[] = {
    {"twiddle3x3", &twiddle3x3.vtable},
    {"otwiddle3x3", &otwiddle3x3.vtable},
};

struct cmdline {
    struct group *gp;
    char *gpname;
    int save;
    int load;
};

void proc_cmdline(int argc, char **argv, struct cmdline *cl)
{
    int opts = 1;

    cl->save = cl->load = 0;
    cl->gp = groups[0].gp;
    cl->gpname = groups[0].name;

    while (--argc > 0) {
        char *p = *++argv;

        if (opts && *p == '-') {
            if (!strcmp(p, "-s"))
                cl->save = 1;
            else if (!strcmp(p, "-l"))
                cl->load = 1;
            else if (!strcmp(p, "--"))
                opts = 0;
            else
                fprintf(stderr, "unrecognised option '%s'\n", p);
        } else {
            int i;
            for (i = 0; i < lenof(groups); i++)
                if (!strcmp(p, groups[i].name)) {
                    cl->gp = groups[i].gp;
                    cl->gpname = groups[i].name;
                    break;
                }
            if (i == lenof(groups)) {
                fprintf(stderr, "unrecognised group name '%s'\n", p);
            }
        }
    }
}

/* ----------------------------------------------------------------------
 * Small test subprograms.
 */

#ifdef TEST_PERMUTATION_INDEX

struct perm perm4 = {
    {perm_index, perm_fromindex},
    4
};

int main(void)
{
    int ia[24][4] = {
	{0,1,2,3},
	{0,1,3,2},
	{0,2,1,3},
	{0,2,3,1},
	{0,3,1,2},
	{0,3,2,1},
	{1,0,2,3},
	{1,0,3,2},
	{1,2,0,3},
	{1,2,3,0},
	{1,3,0,2},
	{1,3,2,0},
	{2,0,1,3},
	{2,0,3,1},
	{2,1,0,3},
	{2,1,3,0},
	{2,3,0,1},
	{2,3,1,0},
	{3,0,1,2},
	{3,0,2,1},
	{3,1,0,2},
	{3,1,2,0},
	{3,2,0,1},
	{3,2,1,0},
    };
    int i;
    int io[4];

    for (i = 0; i < 24; i++) {
	int index = perm_index(&perm2x2.vtable, ia[i]);
	perm_fromindex(&perm2x2.vtable, io, index);
	assert(ia[i][0] == io[0]);
	assert(ia[i][1] == io[1]);
	assert(ia[i][2] == io[2]);
	assert(ia[i][3] == io[3]);
	assert(index == i);
	printf("%d %d%d%d%d\n", index, io[0], io[1], io[2], io[3]);
    }

    return 0;
}
#define GOT_MAIN
#endif

#ifdef TEST_MOVES

int main(int argc, char **argv)
{
    void *elt;
    int eltsize;
    int i;
    struct group *gp;
    struct cmdline cl;

    proc_cmdline(argc, argv, &cl);
    gp = cl.gp;

    eltsize = gp->eltsize(gp);
    elt = malloc(eltsize);

    for (i = 0; i < gp->moves(gp); i++) {
        printf("%s: ", gp->movename(gp, i));
        gp->identity(gp, elt);
        gp->printelt(gp, elt);
        printf(" (%d)", gp->index(gp, elt));
	gp->makemove(gp, elt, i);
        printf(" -> ");
        gp->printelt(gp, elt);
        printf(" (%d)", gp->index(gp, elt));
        printf("\n");
    }

    return 0;
}
#define GOT_MAIN

#endif

#ifndef GOT_MAIN

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

/* ----------------------------------------------------------------------
 * The real main program, containing the meat of the group-mapping
 * algorithm.
 */

typedef unsigned char dist_t;
#define DIST_TODO 128
#define DIST_MASK (DIST_TODO - 1)
#define DIST_UNSEEN DIST_MASK

void update_todo(int **todos, int granularity, int ntodos, int i, int inc)
{
    int j;

    i /= granularity;
    for (j = 0; j < ntodos; j++) {
	todos[j][i] += inc;
	i >>= 1;
    }
}

int main(int argc, char **argv)
{
    struct group *gp;
    dist_t *dists;
    int ndists;
    int granularity;
    int ntodos, whichtodo;
    int **todos[2];
    void *elt, *elt2;
    int nmoves, eltsize, currdist, size;
    char buf[512];
    struct cmdline cl;

    proc_cmdline(argc, argv, &cl);
    gp = cl.gp;

    /*
     * Allocate space to store elements in.
     */
    eltsize = gp->eltsize(gp);
    elt = malloc(eltsize);
    elt2 = malloc(eltsize);

    ndists = gp->maxindex(gp);
    nmoves = gp->moves(gp);            /* cache to avoid recomputation */

    /*
     * Load an existing group map rather than recreating this one,
     * if asked to.
     */
    if (cl.load) {
        int fd;
        char buf[256];

        sprintf(buf, "groupmap.%s", cl.gpname);

        /*
         * This is Unix-specific, because using mmap is just _so_
         * much more efficient.
         */
        fd = open(buf, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s: open: %s\n", buf, strerror(errno));
            return 1;
        }

        dists = mmap(NULL, ndists * sizeof(dist_t),
                     PROT_READ, MAP_PRIVATE, fd, 0);
        if (!dists) {
            fprintf(stderr, "%s: mmap: %s\n", buf, strerror(errno));
            return 1;
        }

    } else {

        /*
         * Set up big array for group.
         */
        dists = (dist_t *)malloc(ndists * sizeof(dist_t));
        assert(sizeof(dist_t) == 1);   /* FIXME: memset is wrong if not */
        memset(dists, DIST_UNSEEN, ndists * sizeof(dist_t));

        /*
         * Set up a bunch of smaller arrays of int, counting number of
         * todo elements in subregions of the array. We'll need two of
         * these, since we switch back and forth every time we finish
         * processing the elements at a given distance.
         *
         * We'd like to use not more than 1/4 of the space taken up by
         * the dists array itself. This means that:
         *
         *  - one complete set of index arrays comes to about twice
         *    the size of the finest-grained one (sum of powers of
         *    two)
         *
         *  - thus two of them come to four times that size
         *
         *  - so we want 4 * sizeof(int) * length / granularity to
         *    be at most one quarter of length * sizeof(dist_t).
         */
        granularity = 16 * sizeof(int) / sizeof(dist_t);
        assert((granularity & (granularity-1)) == 0);  /* must be power of 2 */
        ntodos = 1;
        while ((granularity << (ntodos-1)) < ndists)
            ntodos++;

        for (whichtodo = 0; whichtodo < 2; whichtodo++) {
            int i;

            todos[whichtodo] = (int **)malloc(ntodos * sizeof(int *));

            for (i = 0; i < ntodos; i++) {
                int g = granularity << i;
                int size = (ndists + g - 1) / g;
                todos[whichtodo][i] = (int *)malloc(size * sizeof(int));
                memset(todos[whichtodo][i], 0, size * sizeof(int));
            }
        }

        /*
         * Find the initial element and add it to the array marked as
         * todo.
         */
        gp->identity(gp, elt);
        {
            int i = gp->index(gp, elt);
            dists[i] = DIST_TODO | 0;
            update_todo(todos[0], granularity, ntodos, i, +1);
        }
        whichtodo = 0;

        /*
         * Set up miscellaneous stuff.
         */
        currdist = 0;
        size = 1;                      /* must count the identity element! */

        while (todos[whichtodo][ntodos-1][0] > 0) {
            printf("%d elements at distance %d\n", todos[whichtodo][ntodos-1][0],
                   currdist);

            /*
             * Process a distance level by emptying one todo tree into
             * the other.
             */
            while (todos[whichtodo][ntodos-1][0] > 0) {
                /*
                 * Search down the todo tree to find the next element
                 * that needs processing.
                 */
                int i, j;

                i = 0;
                for (j = ntodos-2; j >= 0; j--) {
                    i *= 2;
                    if (todos[whichtodo][j][i] == 0)
                        i++;
                }
                assert(todos[whichtodo][0][i] > 0);

                /*
                 * Now i is the index of the lowest non-empty entry in
                 * the finest-grained todo table. Search the actual
                 * distance array to find the element itself.
                 */
                j = i * granularity;
                for (i = j; i < j + granularity && i < ndists; i++)
                    if (dists[i] == (DIST_TODO | currdist))
                        break;
                assert(i < j + granularity && i < ndists);

                gp->fromindex(gp, elt, i);
#ifdef DEBUG_SEARCH
                printf("todo=%d; processing element ", todos[whichtodo][ntodos-1][0]);
                gp->printelt(gp, elt);
                printf(" (%d)\n", i);
#endif

                /*
                 * Process the element by applying every possible move
                 * to it.
                 */
                for (j = 0; j < nmoves; j++) {
                    int newi;
                    memcpy(elt2, elt, eltsize);
                    gp->makemove(gp, elt2, j);
                    newi = gp->index(gp, elt2);

                    if (dists[newi] == DIST_UNSEEN) {
                        dists[newi] = DIST_TODO | (currdist+1);
                        update_todo(todos[whichtodo ^ 1], granularity, ntodos,
                                    newi, +1);
                        size++;
#ifdef DEBUG_SEARCH
                        printf("found element at dist %d: ", currdist+1);
                        gp->printelt(gp, elt2);
                        printf(" (%d)\n", newi);
#endif
                    }
                }

                /*
                 * Mark this element as done.
                 */
                dists[i] &= ~DIST_TODO;
                update_todo(todos[whichtodo], granularity, ntodos, i, -1);
            }

            /*
             * Now switch todo lists.
             */
            assert(todos[whichtodo][ntodos-1][0] == 0);
            whichtodo ^= 1;
            currdist++;
        }

        /*
         * The group is mapped. Report its total size.
         */
        printf("Total group size is %d\n", size);
    }

    /*
     * If we were asked to on the command line, save a dump of the
     * group map rather than entering interactive mode.
     */
    if (cl.save) {
        FILE *fp;
        int offset, max;
        char buf[256];

        sprintf(buf, "groupmap.%s", cl.gpname);

        fp = fopen(buf, "wb");
        if (!fp) {
            fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
            return 1;
        }
        max = ndists * sizeof(dist_t);
        offset = 0;
        while (offset < max) {
            int len = max - offset;
            if (len > 16384)
                len = 16384;
            len = fwrite((char *)dists + offset, 1, len, fp);
            if (len < 0) {
                fprintf(stderr, "%s: fopen: %s\n", buf, strerror(errno));
                return 1;
            }
            offset += len;
        }
        fclose(fp);

        return 0;
    }

    while (fgets(buf, sizeof(buf), stdin)) {
        char *err;

        buf[strcspn(buf, "\r\n")] = '\0';

        if ((err = gp->parseelt(gp, elt, buf)) != NULL) {
            printf("parse error: %s\n", err);
            continue;
        }

        /*
         * We have an element description. Report a shortest
         * sequence of moves required to `solve' it down to the
         * identity.
         */

        while (1) {
            int dist, j;

            /*
             * Find the current element distance. If it's 0, we're
             * at the identity and have finished.
             */
            dist = dists[gp->index(gp, elt)];
            if (dist == 0)
                break;
            if (dist == DIST_UNSEEN) {
                printf("position unreachable\n");
                break;
            }

            /*
             * Now try each possible move from here, until we find
             * one which reduces the distance.
             */
	    for (j = 0; j < nmoves; j++) {
		int newi;

		memcpy(elt2, elt, eltsize);
		gp->makemove(gp, elt2, j);
		newi = gp->index(gp, elt2);
		if (dists[newi] < dist)
                    break;
            }

            assert(j < nmoves);

            printf("Move %s -> ", gp->movename(gp, j));
            gp->printelt(gp, elt2);
            printf("\n");

            memcpy(elt, elt2, eltsize);
        }
    }

    return 0;
}

#endif
