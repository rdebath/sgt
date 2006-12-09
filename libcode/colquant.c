/*
 * Colour quantiser.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include "colquant.h"

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

struct octree_node {
    int leaf;
    union {
	struct {
	    struct octree_node *children[8];
	    struct octree_node *next;  /* next in freelist, or at this level */
	} nonleaf;
	struct {
	    unsigned long pixels, r, g, b;
	} leaf;
    } u;
};

struct colquant {
    int ncolours, depth, leaves;
    struct octree_node *nodes;
    struct octree_node **firstatlevel;
    struct octree_node *root;
    struct octree_node *freelist;
};

static struct octree_node *cq_alloc(colquant *cq, int leaf)
{
    struct octree_node *node;

    assert(cq->freelist);

    node = cq->freelist;
    cq->freelist = node->u.nonleaf.next;

    node->leaf = leaf;
    if (leaf) {
	node->u.leaf.pixels = node->u.leaf.r =
	    node->u.leaf.g = node->u.leaf.b = 0;
	cq->leaves++;
    } else {
	int i;
	for (i = 0; i < 8; i++)
	    node->u.nonleaf.children[i] = NULL;
	node->u.nonleaf.next = NULL;
    }

    return node;
}

static void cq_free(colquant *cq, struct octree_node *node)
{
    if (node->leaf)
	cq->leaves--;
    node->u.nonleaf.next = cq->freelist;
    cq->freelist = node;
}

colquant *colquant_new(int ncolours, int depth)
{
    colquant *cq = (colquant *)malloc(sizeof(colquant));
    int i, totalnodes;

    if (!cq)
	return NULL;

    cq->ncolours = ncolours;
    cq->depth = depth;
    cq->leaves = 0;

    /*
     * An upper bound on the maximum number of nodes we can
     * possibly use is (ncolours+1) * (depth+1), on the grounds
     * that that gives a totally disjoint path from the root to
     * each of ncolours+1 lowest-level leaf nodes.
     * 
     * We won't need that many (if nothing else, all those
     * ncolours+1 paths will share the same root node!), but it's a
     * good enough upper bound.
     */
    totalnodes = (ncolours+1) * (depth+1);
    cq->nodes = malloc(totalnodes * sizeof(struct octree_node));
    if (!cq->nodes) {
	free(cq);
	return NULL;
    }

    cq->firstatlevel = malloc(depth * sizeof(struct octree_node *));
    if (!cq->firstatlevel) {
	free(cq->nodes);
	free(cq);
	return NULL;
    }

    /*
     * All nodes are initially on the free list.
     */
    for (i = 0; i < totalnodes; i++)
	cq->nodes[i].u.nonleaf.next =
	(i+1 < totalnodes ? &cq->nodes[i+1] : NULL);
    cq->freelist = &cq->nodes[0];

    /*
     * All firstatlevel lists are initially empty.
     */
    for (i = 0; i < depth; i++)
	cq->firstatlevel[i] = NULL;

    /*
     * Allocate a root, to save special cases in colquant_data.
     */
    cq->root = cq_alloc(cq, FALSE);
    cq->firstatlevel[0] = cq->root;

    return cq;
}

void colquant_free(colquant *cq)
{
    if (cq) {
	free(cq->firstatlevel);
	free(cq->nodes);
	free(cq);
    }
}

void colquant_data(colquant *cq, png_pixel *pixels, int npixels)
{
    while (npixels-- > 0) {
	png_pixel p = *pixels++;
	unsigned long pix, r, g, b;
	int i, depth;
	struct octree_node *node, *child;

	/*
	 * FIXME: integer overflow fears dictate that I use 8-bit
	 * colour channels rather than 16. Can we do something
	 * about this?
	 */
	p.r >>= 8;
	p.g >>= 8;
	p.b >>= 8;

	/*
	 * Work down the octree, adding nodes as necessary, until
	 * we either reach our maximum depth and add a leaf node,
	 * or we reach an existing leaf node.
	 */
	depth = 0;
	node = cq->root;

	while (!node->leaf) {
	    /*
	     * Work out which child path we're taking.
	     */
	    int cindex = ((p.r & (0x80 >> depth) ? 4 : 0) |
			  (p.g & (0x80 >> depth) ? 2 : 0) |
			  (p.b & (0x80 >> depth) ? 1 : 0));

	    child = node->u.nonleaf.children[cindex];
	    if (!child) {
		/*
		 * Create a new node.
		 */
		child = cq_alloc(cq, (depth+1 == cq->depth));
		if (!child->leaf) {
		    child->u.nonleaf.next = cq->firstatlevel[depth+1];
		    cq->firstatlevel[depth+1] = child;
		}
		node->u.nonleaf.children[cindex] = child;
	    }

	    node = child;
	    depth++;
	}

	/*
	 * Now we're at a leaf node. Add our data.
	 */
	node->u.leaf.pixels++;
	node->u.leaf.r += p.r;
	node->u.leaf.g += p.g;
	node->u.leaf.b += p.b;

	/*
	 * If we've just increased the number of leaves to more
	 * than ncolours, we must reduce nodes until this is no
	 * longer the case.
	 */
	while (cq->leaves > cq->ncolours) {
	    /*
	     * Find the lowest level at which there's an internal
	     * node, and pick the most recently added node at that
	     * level. By construction, it must have only leaf nodes
	     * as children, so we can reduce it.
	     */
	    for (depth = cq->depth; depth-- > 0 ;) {
		if (cq->firstatlevel[depth])
		    break;
	    }

	    node = cq->firstatlevel[depth];
	    cq->firstatlevel[depth] = node->u.nonleaf.next;

	    /*
	     * Count up the totals of the node's children, and free
	     * the children.
	     */
	    pix = r = g = b = 0;
	    for (i = 0; i < 8; i++)
		if (node->u.nonleaf.children[i]) {
		    pix += node->u.nonleaf.children[i]->u.leaf.pixels;
		    r += node->u.nonleaf.children[i]->u.leaf.r;
		    g += node->u.nonleaf.children[i]->u.leaf.g;
		    b += node->u.nonleaf.children[i]->u.leaf.b;
		    cq_free(cq, node->u.nonleaf.children[i]);
		}

	    /*
	     * Turn this node into a leaf node.
	     */
	    node->leaf = TRUE;
	    node->u.leaf.pixels = pix;
	    node->u.leaf.r = r;
	    node->u.leaf.g = g;
	    node->u.leaf.b = b;
	    cq->leaves++;
	}
    }
}

int colquant_get_palette(colquant *cq, png_pixel *palette)
{
    int palettelen;
    int depth, i;
    struct octree_node *node, *child;

    /*
     * To get the quantised palette, we simply enumerate all the
     * leaf nodes in the tree, and return the average colour of
     * each as a palette entry.
     * 
     * We could walk the tree recursively, or iteratively with an
     * explicit stack, but actually the simplest way is to make use
     * of the fact that we have all the non-leaf nodes helpfully
     * linked into lists for us: so we simply walk each of those
     * lists, and look for leaf children of each internal node.
     */
    palettelen = 0;

    for (depth = 0; depth < cq->depth; depth++) {
	for (node = cq->firstatlevel[depth]; node;
	     node = node->u.nonleaf.next) {
	    for (i = 0; i < 8; i++) {
		child = node->u.nonleaf.children[i];

		if (child && child->leaf) {
		    palette[palettelen].r =
			0x0101 * (child->u.leaf.r / child->u.leaf.pixels);
		    palette[palettelen].g =
			0x0101 * (child->u.leaf.g / child->u.leaf.pixels);
		    palette[palettelen].b =
			0x0101 * (child->u.leaf.b / child->u.leaf.pixels);
		    palettelen++;
		}
	    }
	}
    }

    assert(palettelen == cq->leaves);
    assert(palettelen <= cq->ncolours);

    return palettelen;
}

#ifdef CQ_TESTMODE

/*
 * The test mode works on an actual PNG, so it needs to be linked
 * with pngin.c.
 */

int main(int argc, char **argv)
{
    int error;
    png_pixel palette[256];
    png *p;
    colquant *cq;
    int i, plen;

    cq = colquant_new(256, 8);

    for (i = 1; i < argc; i++) {
	p = png_decode_file(argv[i], &error);
	if (p) {
	    colquant_data(cq, p->pixels, p->width * p->height);
	    png_free(p);
	} else {
	    printf("%s: error: %s\n", argv[i], png_error_msg[error]);
	}
    }

    plen = colquant_get_palette(cq, palette);
    assert(plen <= 256);
    for (i = 0; i < plen; i++)
	printf("%3d: r=%04x g=%04x b=%04x\n", i,
	       palette[i].r, palette[i].g, palette[i].b);

    return 0;
}

#endif
