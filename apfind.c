/*
 * Find arithmetic progressions in a stream of input numbers.
 * 
 * Requires quadratic time and storage in the size of the input.
 * Optimised for streams in which there aren't many APs (it will
 * still work in quadratic time and space, but it will use less
 * space the fewer APs there are).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define BLOW_BY_BLOW

#define NUMTYPE unsigned long
#define NUMPARSE(s) strtoul((s), NULL, 0)
#define INDEXTYPE size_t
#define LENTYPE size_t

/*
 * Multi-level array data structure.
 */
struct mlaentry {
    INDEXTYPE i, j;
    LENTYPE len;
};
struct mlachild {
    struct mlaentry first;
    union mla *child;
};
#define MLASIZE 65536
#define NCHILDREN (MLASIZE / sizeof(struct mlachild))
#define NENTRIES (MLASIZE / sizeof(struct mlaentry))
union mla {
    struct {
	int level;
	union mla *parent;
    } generic;
    struct {
	int level;
	union mla *parent;
	int nchildren;
	struct {
	    struct mlaentry first;
	    union mla *child;
	} children[NCHILDREN];
    } nonleaf;
    struct {
	int level;
	union mla *parent;
	int nentries;
	struct mlaentry entries[NENTRIES];
    } leaf;
};

union mla *mla_new(void)
{
    union mla *mla = malloc(sizeof(union mla));
    if (!mla) {
	fprintf(stderr, "apfind: out of memory\n");
	exit(1);
    }
    mla->leaf.level = 0;
    mla->leaf.parent = NULL;
    mla->leaf.nentries = 0;
    return mla;
}

union mla *mla_append(union mla *mla, INDEXTYPE i, INDEXTYPE j, LENTYPE len)
{
    union mla *node, *node2;

    node = mla;
    while (node->generic.level > 0)
	node = node->nonleaf.children[node->nonleaf.nchildren - 1].child;
    if (node->leaf.nentries >= NENTRIES) {
	do {
	    node = node->generic.parent;
	} while (node && node->nonleaf.nchildren == NCHILDREN);
	if (!node) {
	    node = malloc(sizeof(union mla));
	    if (!node) {
		fprintf(stderr, "apfind: out of memory\n");
		exit(1);
	    }
	    node->nonleaf.level = mla->generic.level + 1;
	    node->nonleaf.parent = NULL;
	    node->nonleaf.nchildren = 1;
	    node->nonleaf.children[0].child = mla;
	    if (mla->generic.level == 0)
		node->nonleaf.children[0].first = mla->leaf.entries[0];
	    else
		node->nonleaf.children[0].first = mla->nonleaf.children[0].first;
	    mla = node;
	}
	do {
	    node2 = malloc(sizeof(union mla));
	    if (!node2) {
		fprintf(stderr, "apfind: out of memory\n");
		exit(1);
	    }
	    node2->generic.level = node->generic.level - 1;
	    node2->generic.parent = node;
	    if (node2->generic.level > 0) {
		node2->nonleaf.nchildren = 0;
	    } else {
		node2->leaf.nentries = 0;
	    }
	    node->nonleaf.children[node->nonleaf.nchildren].first.i = i;
	    node->nonleaf.children[node->nonleaf.nchildren].first.j = j;
	    node->nonleaf.children[node->nonleaf.nchildren].first.len = len;
	    node->nonleaf.children[node->nonleaf.nchildren].child = node2;
	    node->nonleaf.nchildren++;
	    node = node2;
	} while (node->generic.level > 0);
    }
    node->leaf.entries[node->leaf.nentries].i = i;
    node->leaf.entries[node->leaf.nentries].j = j;
    node->leaf.entries[node->leaf.nentries].len = len;
    node->leaf.nentries++;
    return mla;
}

LENTYPE mla_search(union mla *mla, INDEXTYPE i, INDEXTYPE j)
{
    size_t bot, top, mid;
    int cmp;

    while (mla->generic.level > 0) {
	bot = 0;
	top = mla->nonleaf.nchildren;
	while (top - bot > 1) {
	    mid = (bot + top) / 2;
	    if (mla->nonleaf.children[mid].first.j != j)
		cmp = mla->nonleaf.children[mid].first.j < j ? -1 : +1;
	    else if (mla->nonleaf.children[mid].first.i != i)
		cmp = mla->nonleaf.children[mid].first.i > i ? -1 : +1;
	    else {
		/* Exact match! */
		return mla->nonleaf.children[mid].first.len;
	    }
	    if (cmp < 0)
		bot = mid;
	    else
		top = mid;
	}
	mla = mla->nonleaf.children[bot].child;
    }

    bot = 0;
    top = mla->nonleaf.nchildren + 1;
    while (top - bot > 1) {
	mid = (bot + top) / 2;
	if (mla->leaf.entries[mid].j != j)
	    cmp = mla->leaf.entries[mid].j < j ? -1 : +1;
	else if (mla->leaf.entries[mid].i != i)
	    cmp = mla->leaf.entries[mid].i > i ? -1 : +1;
	else {
	    /* Exact match! */
	    return mla->leaf.entries[mid].len;
	}
	if (cmp < 0)
	    bot = mid;
	else
	    top = mid;
    }
    return 2;
}

/*
 * Linked list of blocks to store the input numbers in.
 */
#define LISTBLKSIZE 65536
#define NLISTENTRIES (LISTBLKSIZE / sizeof(NUMTYPE))
struct listblk {
    struct listblk *prev;
    int nentries;
    INDEXTYPE firstindex;
    NUMTYPE entries[NLISTENTRIES];
};

void print_sequence(NUMTYPE i, NUMTYPE j, LENTYPE len)
{
    NUMTYPE start = j - (j-i) * (len-1);
    if (len == 0) {
	printf("%d\n", len);
	return;
    }
    printf("%d:", len);
    while (len > 0) {
	printf(" %d", start);
	start += (j-i);
	len--;
    }
    printf("\n");
}

LENTYPE bestlen = 0;
NUMTYPE besti, bestj;

void candidate(NUMTYPE i, NUMTYPE j, LENTYPE len)
{
    if (bestlen < len) {
	bestlen = len;
	besti = i;
	bestj = j;
#ifdef BLOW_BY_BLOW
	print_sequence(i, j, len);
#endif
    }
}

int main(void)
{
    char buf[4096];
    union mla *mla;
    struct listblk *head = NULL;
    NUMTYPE firstseen, lastseen;
    INDEXTYPE nnumbers;
    size_t lineno;

    mla = mla_new();

    lineno = 1;
    nnumbers = 0;
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	NUMTYPE num;
	struct listblk *iblk, *jblk;
	size_t iindex, jindex;

	iblk = head;
	jblk = head;
	iindex = nnumbers - 1;
	jindex = nnumbers - 1;

	buf[strcspn(buf, "\r\n")] = '\0';
	num = NUMPARSE(buf);

	if (head) {
	    if (lastseen > num) {
		fprintf(stderr, "apfind: at line %d: monotonicity failure\n",
			lineno);
		return 1;
	    }
	} else
	    firstseen = num;

	lineno++;
	lastseen = num;
	candidate(num, num, 1);

	if (!head || head->nentries == NLISTENTRIES) {
	    struct listblk *newhead = malloc(sizeof(struct listblk));
	    if (!newhead) {
		fprintf(stderr, "apfind: out of memory\n");
		exit(1);
	    }
	    newhead->nentries = 0;
	    newhead->prev = head;
	    newhead->firstindex = nnumbers;
	    head = newhead;
	}
	head->entries[head->nentries++] = num;
	nnumbers++;

	while (jblk) {
	    NUMTYPE i, j;

	    j = jblk->entries[jindex - jblk->firstindex];
	    candidate(j, num, 2);

	    if (num - j > j - firstseen)
		break;
	    i = j - (num - j);

	    while (1) {
		if (iblk->entries[iindex - iblk->firstindex] <= i)
		    break;
		iindex--;
		if (iindex < iblk->firstindex) {
		    iblk = iblk->prev;
		    /* iblk must be non-NULL because i >= firstseen */
		}
	    }

	    if (iblk->entries[iindex - iblk->firstindex] == i) {
		LENTYPE len = mla_search(mla, iindex, jindex);
		mla = mla_append(mla, jindex, nnumbers-1, len+1);
		candidate(j, num, len+1);
	    }

	    if (jindex == 0)
		break;
	    jindex--;
	    if (jindex < jblk->firstindex) {
		jblk = jblk->prev;
		/* jblk must be non-NULL because jindex >= 0 */
	    }
	}
    }

    if (bestlen > 0)
	print_sequence(besti, bestj, bestlen);

    return 0;
}
