/*
 * tree234.c: reasonably generic counted 2-3-4 tree routines.
 * 
 * This file is copyright 1999-2001 Simon Tatham.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tree234.h"

#define smalloc malloc
#define sfree free

#define mknew(typ) ( (typ *) smalloc (sizeof (typ)) )

#ifdef TEST
#define LOG(x) (printf x)
#else
#define LOG(x)
#endif

typedef struct node234_Tag node234;

struct tree234_Tag {
    node234 *root;
    cmpfn234 cmp;
};

struct node234_Tag {
    node234 *parent;
    node234 *kids[4];
    int counts[4];
    void *elems[3];
};

/*
 * Create a 2-3-4 tree.
 */
tree234 *newtree234(cmpfn234 cmp) {
    tree234 *ret = mknew(tree234);
    LOG(("created tree %p\n", ret));
    ret->root = NULL;
    ret->cmp = cmp;
    return ret;
}

/*
 * Free a 2-3-4 tree (not including freeing the elements).
 */
static void freenode234(node234 *n) {
    if (!n)
	return;
    freenode234(n->kids[0]);
    freenode234(n->kids[1]);
    freenode234(n->kids[2]);
    freenode234(n->kids[3]);
    sfree(n);
}
void freetree234(tree234 *t) {
    freenode234(t->root);
    sfree(t);
}

/*
 * Internal function to count a node.
 */
static int countnode234(node234 *n) {
    int count = 0;
    int i;
    if (!n)
	return 0;
    for (i = 0; i < 4; i++)
	count += n->counts[i];
    for (i = 0; i < 3; i++)
	if (n->elems[i])
	    count++;
    return count;
}

/*
 * Count the elements in a tree.
 */
int count234(tree234 *t) {
    if (t->root)
	return countnode234(t->root);
    else
	return 0;
}

/*
 * Propagate a node overflow up a tree until it stops. Returns 0 or
 * 1, depending on whether the root had to be split or not.
 */
static int add234_insert(node234 *left, void *e, node234 *right,
			 node234 **root, node234 *n, int ki) {
    int lcount, rcount;
    /*
     * We need to insert the new left/element/right set in n at
     * child position ki.
     */
    lcount = countnode234(left);
    rcount = countnode234(right);
    while (n) {
	LOG(("  at %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	     n,
	     n->kids[0], n->counts[0], n->elems[0],
	     n->kids[1], n->counts[1], n->elems[1],
	     n->kids[2], n->counts[2], n->elems[2],
	     n->kids[3], n->counts[3]));
	LOG(("  need to insert %p/%d [%p] %p/%d at position %d\n",
	     left, lcount, e, right, rcount, ki));
	if (n->elems[1] == NULL) {
	    /*
	     * Insert in a 2-node; simple.
	     */
	    if (ki == 0) {
		LOG(("  inserting on left of 2-node\n"));
		n->kids[2] = n->kids[1];     n->counts[2] = n->counts[1];
		n->elems[1] = n->elems[0];
		n->kids[1] = right;          n->counts[1] = rcount;
		n->elems[0] = e;
		n->kids[0] = left;           n->counts[0] = lcount;
	    } else { /* ki == 1 */
		LOG(("  inserting on right of 2-node\n"));
		n->kids[2] = right;          n->counts[2] = rcount;
		n->elems[1] = e;
		n->kids[1] = left;           n->counts[1] = lcount;
	    }
	    if (n->kids[0]) n->kids[0]->parent = n;
	    if (n->kids[1]) n->kids[1]->parent = n;
	    if (n->kids[2]) n->kids[2]->parent = n;
	    LOG(("  done\n"));
	    break;
	} else if (n->elems[2] == NULL) {
	    /*
	     * Insert in a 3-node; simple.
	     */
	    if (ki == 0) {
		LOG(("  inserting on left of 3-node\n"));
		n->kids[3] = n->kids[2];    n->counts[3] = n->counts[2];
		n->elems[2] = n->elems[1];
		n->kids[2] = n->kids[1];    n->counts[2] = n->counts[1];
		n->elems[1] = n->elems[0];
		n->kids[1] = right;         n->counts[1] = rcount;
		n->elems[0] = e;
		n->kids[0] = left;          n->counts[0] = lcount;
	    } else if (ki == 1) {
		LOG(("  inserting in middle of 3-node\n"));
		n->kids[3] = n->kids[2];    n->counts[3] = n->counts[2];
		n->elems[2] = n->elems[1];
		n->kids[2] = right;         n->counts[2] = rcount;
		n->elems[1] = e;
		n->kids[1] = left;          n->counts[1] = lcount;
	    } else { /* ki == 2 */
		LOG(("  inserting on right of 3-node\n"));
		n->kids[3] = right;         n->counts[3] = rcount;
		n->elems[2] = e;
		n->kids[2] = left;          n->counts[2] = lcount;
	    }
	    if (n->kids[0]) n->kids[0]->parent = n;
	    if (n->kids[1]) n->kids[1]->parent = n;
	    if (n->kids[2]) n->kids[2]->parent = n;
	    if (n->kids[3]) n->kids[3]->parent = n;
	    LOG(("  done\n"));
	    break;
	} else {
	    node234 *m = mknew(node234);
	    m->parent = n->parent;
	    LOG(("  splitting a 4-node; created new node %p\n", m));
	    /*
	     * Insert in a 4-node; split into a 2-node and a
	     * 3-node, and move focus up a level.
	     * 
	     * I don't think it matters which way round we put the
	     * 2 and the 3. For simplicity, we'll put the 3 first
	     * always.
	     */
	    if (ki == 0) {
		m->kids[0] = left;          m->counts[0] = lcount;
		m->elems[0] = e;
		m->kids[1] = right;         m->counts[1] = rcount;
		m->elems[1] = n->elems[0];
		m->kids[2] = n->kids[1];    m->counts[2] = n->counts[1];
		e = n->elems[1];
		n->kids[0] = n->kids[2];    n->counts[0] = n->counts[2];
		n->elems[0] = n->elems[2];
		n->kids[1] = n->kids[3];    n->counts[1] = n->counts[3];
	    } else if (ki == 1) {
		m->kids[0] = n->kids[0];    m->counts[0] = n->counts[0];
		m->elems[0] = n->elems[0];
		m->kids[1] = left;          m->counts[1] = lcount;
		m->elems[1] = e;
		m->kids[2] = right;         m->counts[2] = rcount;
		e = n->elems[1];
		n->kids[0] = n->kids[2];    n->counts[0] = n->counts[2];
		n->elems[0] = n->elems[2];
		n->kids[1] = n->kids[3];    n->counts[1] = n->counts[3];
	    } else if (ki == 2) {
		m->kids[0] = n->kids[0];    m->counts[0] = n->counts[0];
		m->elems[0] = n->elems[0];
		m->kids[1] = n->kids[1];    m->counts[1] = n->counts[1];
		m->elems[1] = n->elems[1];
		m->kids[2] = left;          m->counts[2] = lcount;
		/* e = e; */
		n->kids[0] = right;         n->counts[0] = rcount;
		n->elems[0] = n->elems[2];
		n->kids[1] = n->kids[3];    n->counts[1] = n->counts[3];
	    } else { /* ki == 3 */
		m->kids[0] = n->kids[0];    m->counts[0] = n->counts[0];
		m->elems[0] = n->elems[0];
		m->kids[1] = n->kids[1];    m->counts[1] = n->counts[1];
		m->elems[1] = n->elems[1];
		m->kids[2] = n->kids[2];    m->counts[2] = n->counts[2];
		n->kids[0] = left;          n->counts[0] = lcount;
		n->elems[0] = e;
		n->kids[1] = right;         n->counts[1] = rcount;
		e = n->elems[2];
	    }
	    m->kids[3] = n->kids[3] = n->kids[2] = NULL;
	    m->counts[3] = n->counts[3] = n->counts[2] = 0;
	    m->elems[2] = n->elems[2] = n->elems[1] = NULL;
	    if (m->kids[0]) m->kids[0]->parent = m;
	    if (m->kids[1]) m->kids[1]->parent = m;
	    if (m->kids[2]) m->kids[2]->parent = m;
	    if (n->kids[0]) n->kids[0]->parent = n;
	    if (n->kids[1]) n->kids[1]->parent = n;
	    LOG(("  left (%p): %p/%d [%p] %p/%d [%p] %p/%d\n", m,
		 m->kids[0], m->counts[0], m->elems[0],
		 m->kids[1], m->counts[1], m->elems[1],
		 m->kids[2], m->counts[2]));
	    LOG(("  right (%p): %p/%d [%p] %p/%d\n", n,
		 n->kids[0], n->counts[0], n->elems[0],
		 n->kids[1], n->counts[1]));
	    left = m;  lcount = countnode234(left);
	    right = n; rcount = countnode234(right);
	}
	if (n->parent)
	    ki = (n->parent->kids[0] == n ? 0 :
		  n->parent->kids[1] == n ? 1 :
		  n->parent->kids[2] == n ? 2 : 3);
	n = n->parent;
    }

    /*
     * If we've come out of here by `break', n will still be
     * non-NULL and all we need to do is go back up the tree
     * updating counts. If we've come here because n is NULL, we
     * need to create a new root for the tree because the old one
     * has just split into two. */
    if (n) {
	while (n->parent) {
	    int count = countnode234(n);
	    int childnum;
	    childnum = (n->parent->kids[0] == n ? 0 :
			n->parent->kids[1] == n ? 1 :
			n->parent->kids[2] == n ? 2 : 3);
	    n->parent->counts[childnum] = count;
	    n = n->parent;
	}
	return 0;		       /* root unchanged */
    } else {
	LOG(("  root is overloaded, split into two\n"));
	(*root) = mknew(node234);
	(*root)->kids[0] = left;     (*root)->counts[0] = lcount;
	(*root)->elems[0] = e;
	(*root)->kids[1] = right;    (*root)->counts[1] = rcount;
	(*root)->elems[1] = NULL;
	(*root)->kids[2] = NULL;     (*root)->counts[2] = 0;
	(*root)->elems[2] = NULL;
	(*root)->kids[3] = NULL;     (*root)->counts[3] = 0;
	(*root)->parent = NULL;
	if ((*root)->kids[0]) (*root)->kids[0]->parent = (*root);
	if ((*root)->kids[1]) (*root)->kids[1]->parent = (*root);
	LOG(("  new root is %p/%d [%p] %p/%d\n",
	     (*root)->kids[0], (*root)->counts[0],
	     (*root)->elems[0],
	     (*root)->kids[1], (*root)->counts[1]));
	return 1;		       /* root moved */
    }
}

/*
 * Add an element e to a 2-3-4 tree t. Returns e on success, or if
 * an existing element compares equal, returns that.
 */
static void *add234_internal(tree234 *t, void *e, int index) {
    node234 *n;
    int ki;
    void *orig_e = e;
    int c;

    LOG(("adding node %p to tree %p\n", e, t));
    if (t->root == NULL) {
	t->root = mknew(node234);
	t->root->elems[1] = t->root->elems[2] = NULL;
	t->root->kids[0] = t->root->kids[1] = NULL;
	t->root->kids[2] = t->root->kids[3] = NULL;
	t->root->counts[0] = t->root->counts[1] = 0;
	t->root->counts[2] = t->root->counts[3] = 0;
	t->root->parent = NULL;
	t->root->elems[0] = e;
	LOG(("  created root %p\n", t->root));
	return orig_e;
    }

    n = t->root;
    while (n) {
	LOG(("  node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	     n,
	     n->kids[0], n->counts[0], n->elems[0],
	     n->kids[1], n->counts[1], n->elems[1],
	     n->kids[2], n->counts[2], n->elems[2],
	     n->kids[3], n->counts[3]));
	if (index >= 0) {
	    if (!n->kids[0]) {
		/*
		 * Leaf node. We want to insert at kid position
		 * equal to the index:
		 * 
		 *   0 A 1 B 2 C 3
		 */
		ki = index;
	    } else {
		/*
		 * Internal node. We always descend through it (add
		 * always starts at the bottom, never in the
		 * middle).
		 */
		if (index <= n->counts[0]) {
		    ki = 0;
		} else if (index -= n->counts[0] + 1, index <= n->counts[1]) {
		    ki = 1;
		} else if (index -= n->counts[1] + 1, index <= n->counts[2]) {
		    ki = 2;
		} else if (index -= n->counts[2] + 1, index <= n->counts[3]) {
		    ki = 3;
		} else
		    return NULL;       /* error: index out of range */
	    }
	} else {
	    if ((c = t->cmp(e, n->elems[0])) < 0)
		ki = 0;
	    else if (c == 0)
		return n->elems[0];	       /* already exists */
	    else if (n->elems[1] == NULL || (c = t->cmp(e, n->elems[1])) < 0)
		ki = 1;
	    else if (c == 0)
		return n->elems[1];	       /* already exists */
	    else if (n->elems[2] == NULL || (c = t->cmp(e, n->elems[2])) < 0)
		ki = 2;
	    else if (c == 0)
		return n->elems[2];	       /* already exists */
	    else
		ki = 3;
	}
	LOG(("  moving to child %d (%p)\n", ki, n->kids[ki]));
	if (!n->kids[ki])
	    break;
	n = n->kids[ki];
    }

    add234_insert(NULL, e, NULL, &t->root, n, ki);

    return orig_e;
}

void *add234(tree234 *t, void *e) {
    if (!t->cmp)		       /* tree is unsorted */
	return NULL;

    return add234_internal(t, e, -1);
}
void *addpos234(tree234 *t, void *e, int index) {
    if (index < 0 ||		       /* index out of range */
	t->cmp)			       /* tree is sorted */
	return NULL;		       /* return failure */

    return add234_internal(t, e, index);  /* this checks the upper bound */
}

/*
 * Look up the element at a given numeric index in a 2-3-4 tree.
 * Returns NULL if the index is out of range.
 */
void *index234(tree234 *t, int index) {
    node234 *n;

    if (!t->root)
	return NULL;		       /* tree is empty */

    if (index < 0 || index >= countnode234(t->root))
	return NULL;		       /* out of range */

    n = t->root;
    
    while (n) {
	if (index < n->counts[0])
	    n = n->kids[0];
	else if (index -= n->counts[0] + 1, index < 0)
	    return n->elems[0];
	else if (index < n->counts[1])
	    n = n->kids[1];
	else if (index -= n->counts[1] + 1, index < 0)
	    return n->elems[1];
	else if (index < n->counts[2])
	    n = n->kids[2];
	else if (index -= n->counts[2] + 1, index < 0)
	    return n->elems[2];
	else
	    n = n->kids[3];
    }

    /* We shouldn't ever get here. I wonder how we did. */
    return NULL;
}

/*
 * Find an element e in a sorted 2-3-4 tree t. Returns NULL if not
 * found. e is always passed as the first argument to cmp, so cmp
 * can be an asymmetric function if desired. cmp can also be passed
 * as NULL, in which case the compare function from the tree proper
 * will be used.
 */
void *findrelpos234(tree234 *t, void *e, cmpfn234 cmp,
		    int relation, int *index) {
    node234 *n;
    void *ret;
    int c;
    int idx, ecount, kcount, cmpret;

    if (t->root == NULL)
	return NULL;

    if (cmp == NULL)
	cmp = t->cmp;

    n = t->root;
    /*
     * Attempt to find the element itself.
     */
    idx = 0;
    ecount = -1;
    /*
     * Prepare a fake `cmp' result if e is NULL.
     */
    cmpret = 0;
    if (e == NULL) {
	assert(relation == REL234_LT || relation == REL234_GT);
	if (relation == REL234_LT)
	    cmpret = +1;	       /* e is a max: always greater */
	else if (relation == REL234_GT)
	    cmpret = -1;	       /* e is a min: always smaller */
    }
    while (1) {
	for (kcount = 0; kcount < 4; kcount++) {
	    if (kcount >= 3 || n->elems[kcount] == NULL ||
		(c = cmpret ? cmpret : cmp(e, n->elems[kcount])) < 0) {
		break;
	    }
	    if (n->kids[kcount]) idx += n->counts[kcount];
	    if (c == 0) {
		ecount = kcount;
		break;
	    }
	    idx++;
	}
	if (ecount >= 0)
	    break;
	if (n->kids[kcount])
	    n = n->kids[kcount];
	else
	    break;
    }

    if (ecount >= 0) {
	/*
	 * We have found the element we're looking for. It's
	 * n->elems[ecount], at tree index idx. If our search
	 * relation is EQ, LE or GE we can now go home.
	 */
	if (relation != REL234_LT && relation != REL234_GT) {
	    if (index) *index = idx;
	    return n->elems[ecount];
	}

	/*
	 * Otherwise, we'll do an indexed lookup for the previous
	 * or next element. (It would be perfectly possible to
	 * implement these search types in a non-counted tree by
	 * going back up from where we are, but far more fiddly.)
	 */
	if (relation == REL234_LT)
	    idx--;
	else
	    idx++;
    } else {
	/*
	 * We've found our way to the bottom of the tree and we
	 * know where we would insert this node if we wanted to:
	 * we'd put it in in place of the (empty) subtree
	 * n->kids[kcount], and it would have index idx
	 * 
	 * But the actual element isn't there. So if our search
	 * relation is EQ, we're doomed.
	 */
	if (relation == REL234_EQ)
	    return NULL;

	/*
	 * Otherwise, we must do an index lookup for index idx-1
	 * (if we're going left - LE or LT) or index idx (if we're
	 * going right - GE or GT).
	 */
	if (relation == REL234_LT || relation == REL234_LE) {
	    idx--;
	}
    }

    /*
     * We know the index of the element we want; just call index234
     * to do the rest. This will return NULL if the index is out of
     * bounds, which is exactly what we want.
     */
    ret = index234(t, idx);
    if (ret && index) *index = idx;
    return ret;
}
void *find234(tree234 *t, void *e, cmpfn234 cmp) {
    return findrelpos234(t, e, cmp, REL234_EQ, NULL);
}
void *findrel234(tree234 *t, void *e, cmpfn234 cmp, int relation) {
    return findrelpos234(t, e, cmp, relation, NULL);
}
void *findpos234(tree234 *t, void *e, cmpfn234 cmp, int *index) {
    return findrelpos234(t, e, cmp, REL234_EQ, index);
}

/*
 * Tree transformation used in delete and split: move a subtree
 * right, from child ki of a node to the next child. Update k and
 * index so that they still point to the same place in the
 * transformed tree. Assumes the destination child _does_ have a
 * right neighbour which is not full, and that the source child
 * _does_ have a subtree to spare.
 * 
 *                . C .                     . B .
 *               /     \     ->            /     \
 * [more] a A b B c   d D e      [more] a A b   c C d D e
 */
static void trans234_subtree_right(node234 *n, int ki, int *k, int *index) {
    node234 *src, *dest;
    int i, srclen, adjust;

    src = n->kids[ki];
    dest = n->kids[ki+1];

    LOG(("  trans234_subtree_right(%p, %d):\n", n, ki));
    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    src %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 src,
	 src->kids[0], src->counts[0], src->elems[0],
	 src->kids[1], src->counts[1], src->elems[1],
	 src->kids[2], src->counts[2], src->elems[2],
	 src->kids[3], src->counts[3]));
    LOG(("    dest %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 dest,
	 dest->kids[0], dest->counts[0], dest->elems[0],
	 dest->kids[1], dest->counts[1], dest->elems[1],
	 dest->kids[2], dest->counts[2], dest->elems[2],
	 dest->kids[3], dest->counts[3]));
    /*
     * Move over the rest of the destination node to make space.
     */
    dest->kids[3] = dest->kids[2];    dest->counts[3] = dest->counts[2];
    dest->elems[2] = dest->elems[1];
    dest->kids[2] = dest->kids[1];    dest->counts[2] = dest->counts[1];
    dest->elems[1] = dest->elems[0];
    dest->kids[1] = dest->kids[0];    dest->counts[1] = dest->counts[0];

    i = (src->elems[2] ? 2 : 1);       /* which element to move over */

    dest->elems[0] = n->elems[ki];
    n->elems[ki] = src->elems[i];
    src->elems[i] = NULL;

    dest->kids[0] = src->kids[i+1];   dest->counts[0] = src->counts[i+1];
    src->kids[i+1] = NULL;            src->counts[i+1] = 0;

    if (dest->kids[0]) dest->kids[0]->parent = dest;

    adjust = dest->counts[0] + 1;

    n->counts[ki] -= adjust;
    n->counts[ki+1] += adjust;

    srclen = n->counts[ki];

    LOG(("    before: k,index = %d,%d\n", (*k), (*index)));
    if ((*k) == ki && (*index) > srclen) {
	(*index) -= srclen + 1;
	(*k)++;
    } else if ((*k) == ki+1) {
	(*index) += adjust;
    }
    LOG(("    after: k,index = %d,%d\n", (*k), (*index)));

    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    src %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 src,
	 src->kids[0], src->counts[0], src->elems[0],
	 src->kids[1], src->counts[1], src->elems[1],
	 src->kids[2], src->counts[2], src->elems[2],
	 src->kids[3], src->counts[3]));
    LOG(("    dest %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 dest,
	 dest->kids[0], dest->counts[0], dest->elems[0],
	 dest->kids[1], dest->counts[1], dest->elems[1],
	 dest->kids[2], dest->counts[2], dest->elems[2],
	 dest->kids[3], dest->counts[3]));
}

/*
 * Tree transformation used in delete and split: move a subtree
 * left, from child ki of a node to the previous child. Update k
 * and index so that they still point to the same place in the
 * transformed tree. Assumes the destination child _does_ have a
 * right neighbour which is not full, and that the source child
 * _does_ have a subtree to spare.
 *
 *      . B .                             . C .
 *     /     \                ->         /     \
 *  a A b   c C d D e [more]      a A b B c   d D e [more]
 */
static void trans234_subtree_left(node234 *n, int ki, int *k, int *index) {
    node234 *src, *dest;
    int i, srclen, adjust;

    src = n->kids[ki];
    dest = n->kids[ki-1];

    LOG(("  trans234_subtree_left(%p, %d):\n", n, ki));
    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    dest %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 dest,
	 dest->kids[0], dest->counts[0], dest->elems[0],
	 dest->kids[1], dest->counts[1], dest->elems[1],
	 dest->kids[2], dest->counts[2], dest->elems[2],
	 dest->kids[3], dest->counts[3]));
    LOG(("    src %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 src,
	 src->kids[0], src->counts[0], src->elems[0],
	 src->kids[1], src->counts[1], src->elems[1],
	 src->kids[2], src->counts[2], src->elems[2],
	 src->kids[3], src->counts[3]));

    i = (dest->elems[1] ? 2 : 1);      /* where in dest to put it */
    dest->elems[i] = n->elems[ki-1];
    n->elems[ki-1] = src->elems[0];

    dest->kids[i+1] = src->kids[0];   dest->counts[i+1] = src->counts[0];

    if (dest->kids[i+1]) dest->kids[i+1]->parent = dest;

    /*
     * Move over the rest of the source node.
     */
    src->kids[0] = src->kids[1];      src->counts[0] = src->counts[1];
    src->elems[0] = src->elems[1];
    src->kids[1] = src->kids[2];      src->counts[1] = src->counts[2];
    src->elems[1] = src->elems[2];
    src->kids[2] = src->kids[3];      src->counts[2] = src->counts[3];
    src->elems[2] = NULL;
    src->kids[3] = NULL;              src->counts[3] = 0;

    adjust = dest->counts[i+1] + 1;

    n->counts[ki] -= adjust;
    n->counts[ki-1] += adjust;

    LOG(("    before: k,index = %d,%d\n", (*k), (*index)));
    if ((*k) == ki) {
	(*index) -= adjust;
	if ((*index) < 0) {
	    (*index) += n->counts[ki-1] + 1;
	    (*k)--;
	}
    }
    LOG(("    after: k,index = %d,%d\n", (*k), (*index)));

    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    dest %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 dest,
	 dest->kids[0], dest->counts[0], dest->elems[0],
	 dest->kids[1], dest->counts[1], dest->elems[1],
	 dest->kids[2], dest->counts[2], dest->elems[2],
	 dest->kids[3], dest->counts[3]));
    LOG(("    src %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 src,
	 src->kids[0], src->counts[0], src->elems[0],
	 src->kids[1], src->counts[1], src->elems[1],
	 src->kids[2], src->counts[2], src->elems[2],
	 src->kids[3], src->counts[3]));
}

/*
 * Tree transformation used in delete and split: merge child nodes
 * ki and ki+1 of a node. Update k and index so that they still
 * point to the same place in the transformed tree. Assumes both
 * children _are_ sufficiently small.
 *
 *      . B .                .
 *     /     \     ->        |
 *  a A b   c C d      a A b B c C d
 */
static void trans234_subtree_merge(node234 *n, int ki, int *k, int *index) {
    node234 *left, *right;
    int i, leftlen, rightlen;

    left = n->kids[ki];               leftlen = n->counts[ki];
    right = n->kids[ki+1];            rightlen = n->counts[ki+1];

    LOG(("  trans234_subtree_merge(%p, %d):\n", n, ki));
    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    left %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 left,
	 left->kids[0], left->counts[0], left->elems[0],
	 left->kids[1], left->counts[1], left->elems[1],
	 left->kids[2], left->counts[2], left->elems[2],
	 left->kids[3], left->counts[3]));
    LOG(("    right %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 right,
	 right->kids[0], right->counts[0], right->elems[0],
	 right->kids[1], right->counts[1], right->elems[1],
	 right->kids[2], right->counts[2], right->elems[2],
	 right->kids[3], right->counts[3]));

    left->elems[1] = n->elems[ki];

    left->kids[2] = right->kids[0];   left->counts[2] = right->counts[0];
    left->elems[2] = right->elems[0];
    left->kids[3] = right->kids[1];   left->counts[3] = right->counts[1];

    if (left->kids[2]) left->kids[2]->parent = left;
    if (left->kids[3]) left->kids[3]->parent = left;

    n->counts[ki] += rightlen + 1;

    sfree(right);

    /*
     * Move the rest of n up by one.
     */
    for (i = ki+1; i < 3; i++) {
	n->kids[i] = n->kids[i+1];
	n->counts[i] = n->counts[i+1];
    }
    for (i = ki; i < 2; i++) {
	n->elems[i] = n->elems[i+1];
    }
    n->kids[3] = NULL;
    n->counts[3] = 0;
    n->elems[2] = NULL;

    if (k) {
	LOG(("    before: k,index = %d,%d\n", (*k), (*index)));
	if ((*k) == ki+1) {
	    (*k)--;
	    (*index) += leftlen + 1;
	} else if ((*k) > ki+1) {
	    (*k)--;
	}
	LOG(("    after: k,index = %d,%d\n", (*k), (*index)));
    }

    LOG(("    parent %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 n,
	 n->kids[0], n->counts[0], n->elems[0],
	 n->kids[1], n->counts[1], n->elems[1],
	 n->kids[2], n->counts[2], n->elems[2],
	 n->kids[3], n->counts[3]));
    LOG(("    merged %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	 left,
	 left->kids[0], left->counts[0], left->elems[0],
	 left->kids[1], left->counts[1], left->elems[1],
	 left->kids[2], left->counts[2], left->elems[2],
	 left->kids[3], left->counts[3]));

}
    
/*
 * Delete an element e in a 2-3-4 tree. Does not free the element,
 * merely removes all links to it from the tree nodes.
 */
static void *delpos234_internal(tree234 *t, int index) {
    node234 *n;
    void *retval;
    int ei = -1;

    retval = 0;

    n = t->root;
    LOG(("deleting item %d from tree %p\n", index, t));
    while (1) {
	while (n) {
	    int ki;
	    node234 *sub;

	    LOG(("  node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d index=%d\n",
		 n,
		 n->kids[0], n->counts[0], n->elems[0],
		 n->kids[1], n->counts[1], n->elems[1],
		 n->kids[2], n->counts[2], n->elems[2],
		 n->kids[3], n->counts[3],
		 index));
	    if (index < n->counts[0]) {
		ki = 0;
	    } else if (index -= n->counts[0]+1, index < 0) {
		ei = 0; break;
	    } else if (index < n->counts[1]) {
		ki = 1;
	    } else if (index -= n->counts[1]+1, index < 0) {
		ei = 1; break;
	    } else if (index < n->counts[2]) {
		ki = 2;
	    } else if (index -= n->counts[2]+1, index < 0) {
		ei = 2; break;
	    } else {
		ki = 3;
	    }
	    /*
	     * Recurse down to subtree ki. If it has only one element,
	     * we have to do some transformation to start with.
	     */
	    LOG(("  moving to subtree %d\n", ki));
	    sub = n->kids[ki];
	    if (!sub->elems[1]) {
		LOG(("  subtree has only one element!\n"));
		if (ki > 0 && n->kids[ki-1]->elems[1]) {
		    /*
		     * Child ki has only one element, but child
		     * ki-1 has two or more. So we need to move a
		     * subtree from ki-1 to ki.
		     */
		    trans234_subtree_right(n, ki-1, &ki, &index);
		} else if (ki < 3 && n->kids[ki+1] &&
			   n->kids[ki+1]->elems[1]) {
		    /*
		     * Child ki has only one element, but ki+1 has
		     * two or more. Move a subtree from ki+1 to ki.
		     */
		    trans234_subtree_left(n, ki+1, &ki, &index);
		} else {
		    /*
		     * ki has only one element, and has no
		     * neighbour with more than one. So pick a
		     * neighbour and merge it with ki, taking an
		     * element down from n to go in the middle.
		     * 
		     * (Since at all points we have avoided
		     * descending to a node with only one element,
		     * we can be sure that n is not reduced to
		     * nothingness by this move, _unless_ it was
		     * the very first node, ie the root of the
		     * tree. In that case we remove the now-empty
		     * root and replace it with its single large
		     * child as shown.)
		     */
		    trans234_subtree_merge(n, ki>0 ? ki-1 : ki, &ki, &index);
		    sub = n->kids[ki];

		    if (!n->elems[0]) {
			/*
			 * The root is empty and needs to be
			 * removed.
			 */
			LOG(("  shifting root!\n"));
			t->root = sub;
			sub->parent = NULL;
			sfree(n);
		    }
		}
	    }
	    n = sub;
	}
	if (!retval)
	    retval = n->elems[ei];

	if (ei==-1)
	    return NULL;	       /* although this shouldn't happen */

	/*
	 * Treat special case: this is the one remaining item
	 * in the tree. n is the tree root (no parent), has one
	 * element (no elems[1]), and has no kids (no kids[0]).
	 */
	if (!n->parent && !n->elems[1] && !n->kids[0]) {
	    LOG(("  removed last element in tree\n"));
	    sfree(n);
	    t->root = NULL;
	    return retval;
	}

	/*
	 * Now we have the element we want, as n->elems[ei], and we
	 * have also arranged for that element not to be the only
	 * one in its node. So...
	 */

	if (!n->kids[0] && n->elems[1]) {
	    /*
	     * Case 1. n is a leaf node with more than one element,
	     * so it's _really easy_. Just delete the thing and
	     * we're done.
	     */
	    int i;
	    LOG(("  case 1\n"));
	    for (i = ei; i < 2 && n->elems[i+1]; i++)
		n->elems[i] = n->elems[i+1];
	    n->elems[i] = NULL;
	    /*
	     * Having done that to the leaf node, we now go back up
	     * the tree fixing the counts.
	     */
	    while (n->parent) {
		int childnum;
		childnum = (n->parent->kids[0] == n ? 0 :
			    n->parent->kids[1] == n ? 1 :
			    n->parent->kids[2] == n ? 2 : 3);
		n->parent->counts[childnum]--;
		n = n->parent;
	    }
	    return retval;	       /* finished! */
	} else if (n->kids[ei]->elems[1]) {
	    /*
	     * Case 2a. n is an internal node, and the root of the
	     * subtree to the left of e has more than one element.
	     * So find the predecessor p to e (ie the largest node
	     * in that subtree), place it where e currently is, and
	     * then start the deletion process over again on the
	     * subtree with p as target.
	     */
	    node234 *m = n->kids[ei];
	    void *target;
	    LOG(("  case 2a\n"));
	    while (m->kids[0]) {
		m = (m->kids[3] ? m->kids[3] :
		     m->kids[2] ? m->kids[2] :
		     m->kids[1] ? m->kids[1] : m->kids[0]);		     
	    }
	    target = (m->elems[2] ? m->elems[2] :
		      m->elems[1] ? m->elems[1] : m->elems[0]);
	    n->elems[ei] = target;
	    index = n->counts[ei]-1;
	    n = n->kids[ei];
	} else if (n->kids[ei+1]->elems[1]) {
	    /*
	     * Case 2b, symmetric to 2a but s/left/right/ and
	     * s/predecessor/successor/. (And s/largest/smallest/).
	     */
	    node234 *m = n->kids[ei+1];
	    void *target;
	    LOG(("  case 2b\n"));
	    while (m->kids[0]) {
		m = m->kids[0];
	    }
	    target = m->elems[0];
	    n->elems[ei] = target;
	    n = n->kids[ei+1];
	    index = 0;
	} else {
	    /*
	     * Case 2c. n is an internal node, and the subtrees to
	     * the left and right of e both have only one element.
	     * So combine the two subnodes into a single big node
	     * with their own elements on the left and right and e
	     * in the middle, then restart the deletion process on
	     * that subtree, with e still as target.
	     */
	    node234 *a = n->kids[ei], *b = n->kids[ei+1];
	    int j;

	    LOG(("  case 2c\n"));
	    a->elems[1] = n->elems[ei];
	    a->kids[2] = b->kids[0];
	    a->counts[2] = b->counts[0];
	    if (a->kids[2]) a->kids[2]->parent = a;
	    a->elems[2] = b->elems[0];
	    a->kids[3] = b->kids[1];
	    a->counts[3] = b->counts[1];
	    if (a->kids[3]) a->kids[3]->parent = a;
	    sfree(b);
	    n->counts[ei] = countnode234(a);
	    /*
	     * That's built the big node in a, and destroyed b. Now
	     * remove the reference to b (and e) in n.
	     */
	    for (j = ei; j < 2 && n->elems[j+1]; j++) {
		n->elems[j] = n->elems[j+1];
		n->kids[j+1] = n->kids[j+2];
		n->counts[j+1] = n->counts[j+2];
	    }
	    n->elems[j] = NULL;
	    n->kids[j+1] = NULL;
	    n->counts[j+1] = 0;
            /*
             * It's possible, in this case, that we've just removed
             * the only element in the root of the tree. If so,
             * shift the root.
             */
            if (n->elems[0] == NULL) {
                LOG(("  shifting root!\n"));
                t->root = a;
                a->parent = NULL;
                sfree(n);
            }
	    /*
	     * Now go round the deletion process again, with n
	     * pointing at the new big node and e still the same.
	     */
	    n = a;
	    index = a->counts[0] + a->counts[1] + 1;
	}
    }
}
void *delpos234(tree234 *t, int index) {
    if (index < 0 || index >= countnode234(t->root))
	return NULL;
    return delpos234_internal(t, index);
}
void *del234(tree234 *t, void *e) {
    int index;
    if (!findrelpos234(t, e, NULL, REL234_EQ, &index))
	return NULL;		       /* it wasn't in there anyway */
    return delpos234_internal(t, index); /* it's there; delete it. */
}

/*
 * Join two subtrees together with a separator element between
 * them, given their relative height.
 * 
 * (Height<0 means the left tree is shorter, >0 means the right
 * tree is shorter, =0 means (duh) they're equal.)
 * 
 * It is assumed that any checks needed on the ordering criterion
 * have _already_ been done.
 * 
 * The value returned in `height' is 0 or 1 depending on whether the
 * resulting tree is the same height as the original larger one, or
 * one higher.
 */
static node234 *join234_internal(node234 *left, void *sep,
				 node234 *right, int *height) {
    node234 *root, *node;
    int relht = *height;
    int ki;

    LOG(("  join: joining %p [%p] %p, relative height is %d\n",
	 left, sep, right, relht));
    if (relht == 0) {
	/*
	 * The trees are the same height. Create a new one-element
	 * root containing the separator and pointers to the two
	 * nodes.
	 */
	node234 *newroot;
	newroot = mknew(node234);
	newroot->kids[0] = left;     newroot->counts[0] = countnode234(left);
	newroot->elems[0] = sep;
	newroot->kids[1] = right;    newroot->counts[1] = countnode234(right);
	newroot->elems[1] = NULL;
	newroot->kids[2] = NULL;     newroot->counts[2] = 0;
	newroot->elems[2] = NULL;
	newroot->kids[3] = NULL;     newroot->counts[3] = 0;
	newroot->parent = NULL;
	if (left) left->parent = newroot;
	if (right) right->parent = newroot;
	*height = 1;
	LOG(("  join: same height, brand new root\n"));
	return newroot;
    }

    /*
     * This now works like the addition algorithm on the larger
     * tree. We're replacing a single kid pointer with two kid
     * pointers separated by an element; if that causes the node to
     * overload, we split it in two, move a separator element up to
     * the next node, and repeat.
     */
    if (relht < 0) {
	/*
	 * Left tree is shorter. Search down the right tree to find
	 * the pointer we're inserting at.
	 */
	node = root = right;
	while (++relht < 0) {
	    node = node->kids[0];
	}
	ki = 0;
	right = node->kids[ki];
    } else {
	/*
	 * Right tree is shorter; search down the left to find the
	 * pointer we're inserting at.
	 */
	node = root = left;
	while (--relht > 0) {
	    if (node->elems[2])
		node = node->kids[3];
	    else if (node->elems[1])
		node = node->kids[2];
	    else
		node = node->kids[1];
	}
	if (node->elems[2])
	    ki = 3;
	else if (node->elems[1])
	    ki = 2;
	else
	    ki = 1;
	left = node->kids[ki];
    }

    /*
     * Now proceed as for addition.
     */
    *height = add234_insert(left, sep, right, &root, node, ki);

    return root;
}
static int height234(tree234 *t) {
    int level = 0;
    node234 *n = t->root;
    while (n) {
	level++;
	n = n->kids[0];
    }
    return level;
}
tree234 *join234(tree234 *t1, tree234 *t2) {
    int size2 = countnode234(t2->root);
    if (size2 > 0) {
	void *element;
	int relht;

	if (t1->cmp) {
	    element = index234(t2, 0);
	    element = findrelpos234(t1, element, NULL, REL234_GE, NULL);
	    if (element)
		return NULL;
	}

	element = delpos234(t2, 0);
	relht = height234(t1) - height234(t2);
	t1->root = join234_internal(t1->root, element, t2->root, &relht);
	t2->root = NULL;
    }
    return t1;
}
tree234 *join234r(tree234 *t1, tree234 *t2) {
    int size1 = countnode234(t1->root);
    if (size1 > 0) {
	void *element;
	int relht;

	if (t2->cmp) {
	    element = index234(t1, size1-1);
	    element = findrelpos234(t2, element, NULL, REL234_LE, NULL);
	    if (element)
		return NULL;
	}

	element = delpos234(t1, size1-1);
	relht = height234(t1) - height234(t2);
	t2->root = join234_internal(t1->root, element, t2->root, &relht);
	t1->root = NULL;
    }
    return t2;
}

/*
 * Split out the first <index> elements in a tree and return a
 * pointer to the root node. Leave the root node of the remainder
 * in t.
 */
static node234 *split234_internal(tree234 *t, int index) {
    node234 *lfrag, *rfrag, *lptr, *rptr, *n;
    void *lsep, *rsep;
    int level, llevel, rlevel;
    int ki;

    n = t->root;
    LOG(("splitting tree %p at point %d\n", t, index));

    /*
     * Easy special cases.
     */
    if (index == 0)		       /* return nothing */
	return NULL;
    if (index == countnode234(t->root)) {   /* return the whole tree */
	node234 *ret = t->root;
	t->root = NULL;
	return ret;
    }

    /*
     * Search down the tree to find the split point. We will have
     * headaches if we leave a node behind us with only one
     * element, so we do the deletion transforms on the way down to
     * ensure this doesn't happen.
     */
    while (n) {
	LOG(("  node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d index=%d\n",
	     n,
	     n->kids[0], n->counts[0], n->elems[0],
	     n->kids[1], n->counts[1], n->elems[1],
	     n->kids[2], n->counts[2], n->elems[2],
	     n->kids[3], n->counts[3],
	     index));
	if (index <= n->counts[0]) {
	    ki = 0;
	} else if (index -= n->counts[0]+1, index <= n->counts[1]) {
	    ki = 1;
	} else if (index -= n->counts[1]+1, index <= n->counts[2]) {
	    ki = 2;
	} else {
	    index -= n->counts[2]+1;
	    ki = 3;
	}
	/*
	 * Recurse down to subtree ki. We want to make sure that it
	 * has _lots_ of elements.
	 */
	LOG(("  moving to subtree %d\n", ki));
	if (!n->kids[ki])
	    break;
	if (!n->kids[ki]->elems[2]) {
	    node234 *sub;

	    LOG(("  subtree has fewer than three elements\n"));
	    sub = n->kids[ki];

	    /*
	     * We want to make sure child ki has three elements, so
	     * that if we use one up in merging two nodes below it,
	     * it will still have more than one.
	     */
	    if (ki > 0 && ki < 3 && n->elems[ki]) {
		/*
		 * We have a neighbour on either side. We have many
		 * cases:
		 * 
		 *  - We are medium, at least one of our neighbours
		 *    is non-small. Move over a subtree from a
		 *    non-small neighbour.
		 * 
		 *  - We are medium, both our neighbours are small.
		 *    Move one of our subtrees _away_ into a
		 *    neighbour, and merge with the other
		 *    neighbour. (We must choose carefully which
		 *    neighbour to move a subtree into; we have to
		 *    arrange for our resulting search path to lead
		 *    down into the _large_ node, not the medium
		 *    one, that's left over!)
		 * 
		 *  - We are small, both our neighbours are
		 *    non-small. Move over a subtree from each
		 *    neighbour.
		 * 
		 *  - We are small, at least one of our neighbours
		 *    is small. Merge with a neighbour.
		 */
		LOG(("  we have two neighbours\n"));
		if (sub->elems[1]) {
		    /* we are medium */
		    if (n->kids[ki-1]->elems[1]) {
			/* our left neighbour is non-small */
			LOG(("  medium with non-small left neighbour\n"));
			trans234_subtree_right(n, ki-1, &ki, &index);
		    } else if (n->kids[ki+1]->elems[1]) {
			/* our right neighbour is non-small */
			LOG(("  medium with non-small right neighbour\n"));
			trans234_subtree_left(n, ki+1, &ki, &index);
		    } else {
			LOG(("  medium with two small neighbours\n"));
			/* both neighbours are small */
			if (index > sub->counts[0] + 1 + sub->counts[1]) {
			    /* move subtree left, merge with RH neighbour */
			    LOG(("  moving left, merging with right\n"));
			    trans234_subtree_left(n, ki, &ki, &index);
			    trans234_subtree_merge(n, ki, &ki, &index);
			} else {
			    /* move subtree right, merge with LH neighbour */
			    LOG(("  moving right, merging with left\n"));
			    trans234_subtree_right(n, ki, &ki, &index);
			    sub = n->kids[ki-1];
			    trans234_subtree_merge(n, ki-1, &ki, &index);
			}
		    }
		} else {
		    /* we are small */
		    if (!n->kids[ki-1]->elems[1]) {
			/* our left neighbour is small too */
			LOG(("  small with small left neighbour\n"));
			sub = n->kids[ki-1];
			trans234_subtree_merge(n, ki-1, &ki, &index);
		    } else if (!n->kids[ki+1]->elems[1]) {
			/* our right neighbour is small too */
			LOG(("  small with small right neighbour\n"));
			trans234_subtree_merge(n, ki, &ki, &index);
		    } else {
			/* both our neighbours are non-small */
			LOG(("  small with two non-small neighbours\n"));
			trans234_subtree_right(n, ki-1, &ki, &index);
			trans234_subtree_left(n, ki+1, &ki, &index);
		    }
		}
	    } else {
		/*
		 * We have neighbours on only one side. We have
		 * many cases again:
		 * 
		 *  - We are medium, our neighbour is non-small.
		 *    Move a subtree from the neighbour.
		 * 
		 *  - We are medium, our neighbour is small, we
		 *    have a non-small neighbour beyond that. Move
		 *    a subtree from the next-but-one into the
		 *    neighbour, then move a subtree from the
		 *    neighbour into us.
		 * 
		 *  - We are medium, our neighbour is small, we
		 *    have a small neighbour beyond that. Move a
		 *    subtree from us into the neighbour, then one
		 *    from there into the next-but-one, then merge
		 *    with the neighbour. (Note that everything
		 *    that was originally within us is still in us
		 *    when we finish.)
		 * 
		 *  - We are medium, our neighbour is small, we
		 *    have no neighbours beyond that. Hence, our
		 *    parent must be the root. Make do. [1]
		 * 
		 *  - We are small, our neighbour is large. Move
		 *    two subtrees from the neighbour.
		 * 
		 *  - We are small, our neighbour is small. Merge
		 *    with the neighbour.
		 * 
		 *  - We are small, our neighbour is medium, we
		 *    have a non-small neighbour beyond that. Move
		 *    a subtree from the next-but-one into the
		 *    neighbour, and then move two subtrees from
		 *    there to us.
		 * 
		 *  - We are small, our neighbour is medium, we
		 *    have a small neighbour beyond that. Move a
		 *    subtree from the neighbour into the one
		 *    beyond, and merge with the now-small
		 *    neighbour.
		 * 
		 *  - We are small, our neighbour is medium, we
		 *    have no neighbours beyond that. Hence, our
		 *    parent must be the root. Move a subtree from
		 *    the neighbour to ourselves and make do. [1]
		 * 
		 * [1] `Make do' means that we are a medium node
		 * with a small neighbour, the only children of a
		 * small root. This is perfectly OK if one of our
		 * elements doesn't end up being used in a merge at
		 * the next level. If it _does_, we are reduced to
		 * being one of two small children of a small root
		 * - so we must come _back_ and merge the root into
		 * its children.
		 */
		int one, two, merge, neighbours;
		void (*away)(node234 *n, int ki, int *k, int *index);
		void (*toward)(node234 *n, int ki, int *k, int *index);

		if (ki == 0) {
		    one = 1; two = 2; merge = 0;
		    away = trans234_subtree_right;
		    toward = trans234_subtree_left;
		} else {
		    one = ki-1; two = ki-2; merge = one;
		    away = trans234_subtree_left;
		    toward = trans234_subtree_right;
		}

		if (ki == 1 || (ki == 0 && !n->elems[1]))
		    neighbours = 1;
		else
		    neighbours = 2;

		LOG(("  we are on the %s end\n", ki==0 ? "left" : "right"));
		if (sub->elems[1]) {
		    /* we are medium */
		    if (n->kids[one]->elems[1]) {
			/* neighbour is non-small */
			LOG(("  medium, non-small\n"));
			toward(n, one, &ki, &index);
		    } else if (neighbours == 1) {
			/* only one small neighbour; make do */
			LOG(("  medium, small, none; MAKE DO\n"));
		    } else if (n->kids[two]->elems[1]) {
			/* next-but-one is non-small */
			LOG(("  medium, small, non-small\n"));
			toward(n, two, &ki, &index);
			toward(n, one, &ki, &index);
		    } else {
			/* next-but-one is small */
			LOG(("  medium, small, small\n"));
			away(n, ki, &ki, &index);
			away(n, one, &ki, &index);
			sub = n->kids[merge];
			trans234_subtree_merge(n, merge, &ki, &index);
		    }
		} else {
		    /* we are small */
		    if (n->kids[one]->elems[2]) {
			/* neighbour is large */
			LOG(("  small, large\n"));
			toward(n, one, &ki, &index);
			toward(n, one, &ki, &index);
		    } else if (!n->kids[one]->elems[1]) {
			/* neighbour is small */
			LOG(("  small, small\n"));
			sub = n->kids[merge];
			trans234_subtree_merge(n, merge, &ki, &index);
		    } else if (neighbours == 1) {
			/* only one neighbour which is medium */
			LOG(("  small, medium, none; move and MAKE DO\n"));
			toward(n, one, &ki, &index);
			/* now make do */
		    } else if (!n->kids[two]->elems[1]) {
			/* neighbour is medium, next-but-one is small */
			LOG(("  small, medium, small\n"));
			away(n, one, &ki, &index);
			sub = n->kids[merge];
			trans234_subtree_merge(n, merge, &ki, &index);
		    } else {
			/* neighbour is medium, next-but-one is non-small */
			LOG(("  small, medium, non-small\n"));
			toward(n, two, &ki, &index);
			toward(n, one, &ki, &index);
			toward(n, one, &ki, &index);
		    }
		}
	    }
	    /*
	     * We should have ended up with sub still pointing at
	     * the subtree we're actually going to.
	     */
	    assert(sub == n->kids[ki]);

	    /*
	     * Check if we've reduced n to smallness or to
	     * nonexistence in all this madness. If n is the root,
	     * we might have reduced it completely to nonexistence,
	     * so we'd better shift the tree root. (If we've
	     * reduced the root to smallness, we don't care; we can
	     * deal with that later.)
	     * 
	     * If n is not the root but has been reduced to
	     * smallness, it MUST be the case that n is one of two
	     * small children of a small root, and so we can merge
	     * the root into the children.
	     */
	    if (!n->elems[1]) {
		/* n is small or nonexistent */
		if (!n->parent) {
		    /* n is root */
		    if (!n->elems[0]) {
			/* n is nonexistent. Shift root. */
			LOG(("  root is defunct, new root is %p\n", n->kids[0]));
			t->root = n->kids[0];
			n->kids[0]->parent = NULL;
			sfree(n);
		    }
		} else {
		    /* n is not root */
		    /* n MUST be small but not nonexistent */
		    assert(n->elems[0]);
		    /* n MUST be child of the root */
		    assert(n->parent != NULL && n->parent->parent == NULL);
		    /* n MUST be one of only two children */
		    assert(n->parent->elems[1] == NULL);
		    /* n's sibling MUST also be small */
		    assert(n->parent->kids[0]->elems[1] == NULL &&
			   n->parent->kids[1]->elems[1] == NULL);
		    LOG(("  must go back and merge root into kids\n"));
		    n = t->root->kids[0];
		    trans234_subtree_merge(t->root, 0, NULL, NULL);
		    n->parent = NULL;
		    sfree(t->root);
		    t->root = n;
		}
	    }

	    n = sub;
	} else
	    n = n->kids[ki];
    }

    /*
     * Now we see the node where we want to perform the split. So
     * we're going to insert two pointers in place of the one at
     * ki. To begin with, at the leaf, these pointers are NULL.
     */
    lfrag = rfrag = NULL;
    lsep = rsep = NULL;
    lptr = rptr = NULL;
    level = 0;
    while (1) {
	node234 *sib;
	int i;

	LOG(("  split node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
	     n,
	     n->kids[0], n->counts[0], n->elems[0],
	     n->kids[1], n->counts[1], n->elems[1],
	     n->kids[2], n->counts[2], n->elems[2],
	     n->kids[3], n->counts[3]));
	LOG(("  splitting at child index %d\n", ki));

	if (ki > 0 && ki < 3 && n->elems[ki]) {
	    /*
	     * If we're inserting between two elements, this is
	     * easy. Just split the node, put lptr on the right of
	     * the left one and rptr on the left of the right one,
	     * put the new node pointers in lptr and rptr, and go
	     * up a level.
	     */
	    sib = mknew(node234);
	    for (i = 0; i < 3; i++) {
		if (i+ki < 3 && n->elems[i+ki]) {
		    sib->elems[i] = n->elems[i+ki];
		    sib->kids[i+1] = n->kids[i+ki+1];
		    if (sib->kids[i+1]) sib->kids[i+1]->parent = sib;
		    sib->counts[i+1] = n->counts[i+ki+1];
		    n->elems[i+ki] = NULL;
		    n->kids[i+ki+1] = NULL;
		    n->counts[i+ki+1] = 0;
		} else {
		    sib->elems[i] = NULL;
		    sib->kids[i+1] = NULL;
		    sib->counts[i+1] = 0;
		}
	    }
	    n->kids[ki] = lptr;
	    if (lptr) lptr->parent = n;
	    n->counts[ki] = countnode234(lptr);
	    sib->kids[0] = rptr;
	    if (rptr) rptr->parent = sib;
	    sib->counts[0] = countnode234(rptr);
	    lptr = n;
	    rptr = sib;
	    LOG(("  left node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
		 n,
		 n->kids[0], n->counts[0], n->elems[0],
		 n->kids[1], n->counts[1], n->elems[1],
		 n->kids[2], n->counts[2], n->elems[2],
		 n->kids[3], n->counts[3]));
	    LOG(("  right node %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
		 sib,
		 sib->kids[0], sib->counts[0], sib->elems[0],
		 sib->kids[1], sib->counts[1], sib->elems[1],
		 sib->kids[2], sib->counts[2], sib->elems[2],
		 sib->kids[3], sib->counts[3]));
	    n = lptr->parent;
	    level++;
	    if (n) {
		ki = (n->kids[0] == lptr ? 0 :
		      n->kids[1] == lptr ? 1 :
		      n->kids[2] == lptr ? 2 : 3);
	    }

	    /*
	     * It's possible we've come off the root here, in which
	     * case we've successfully split the tree into two
	     * equally high subtrees. Add our fragments back in,
	     * then terminate.
	     */
	    if (!n) {
		LOG(("  fell off top, lroot is %p, rroot is %p\n",
		     lptr, rptr));
		rptr->parent = lptr->parent = NULL;
		if (rsep) {
		    int height = rlevel - level;
		    LOG(("  final join: rfrag [%p], rsep [%p],"
			 " rroot [%p] heightdiff=%d\n",
			 rfrag, rsep, rptr, height));
		    rptr = join234_internal(rfrag, rsep, rptr, &height);
		}
		if (lsep) {
		    int height = level - llevel;
		    LOG(("  final join: lroot [%p], lsep [%p],"
			 " lfrag [%p] heightdiff=%d\n",
			 lptr, lsep, lfrag, height));
		    lptr = join234_internal(lptr, lsep, lfrag, &height);
		}
		LOG(("  after final joins, keeping rroot %p,"
		     " returning lroot %p\n", rptr, lptr));
		t->root = rptr;
		return lptr;
	    }
	} else {
	    /*
	     * If ki is either zero or past the index of the last
	     * element, we are trying to split a node at its edge.
	     * So we have snipped a whole chunk off (it's held in
	     * lptr if ki==0 or rptr if ki!=0). So we put the other
	     * chunk back at the point we've reached, then search
	     * upwards to find the successor node to the split
	     * point; then we remove _that_, grab hold of the
	     * subtrees on either side of it, and continue
	     * snipping. (But we _don't_ go up a level, because we
	     * might have to do this case again pointing the other
	     * way!)
	     */
	    int left = (ki == 0);
	    int i;
	    node234 *tmp;

	    LOG(("  split at %s edge, searching up...\n",
		 left?"left":"right"));
	    if (left) {
		if (lptr) lptr->parent = NULL;
		if (lsep) {
		    int height = level - llevel;
		    LOG(("  joining new lfrag %p (height %d) to"
			 " old %p (height %d), sep %p\n",
			 lptr, level, lfrag, llevel, lsep));
		    lfrag = join234_internal(lptr, lsep, lfrag, &height);
		    llevel = level + height;
		} else {
		    LOG(("  no existing lfrag\n"));
		    lfrag = lptr;
		    llevel = level;
		}
		LOG(("  new lfrag is %p height %d\n", lfrag, llevel));
		n->kids[ki] = rptr;
		n->counts[ki] = countnode234(rptr);
		if (rptr) rptr->parent = n;
	    } else {
		if (rptr) rptr->parent = NULL;
		if (rsep) {
		    int height = rlevel - level;
		    LOG(("  joining old rfrag %p (height %d) to"
			 " new %p (height %d), sep %p\n",
			 rfrag, rlevel, rptr, level, rsep));
		    rfrag = join234_internal(rfrag, rsep, rptr, &height);
		    rlevel = level + height;
		} else {
		    LOG(("  no existing rfrag\n"));
		    rfrag = rptr;
		    rlevel = level;
		}
		LOG(("  new rfrag is %p height %d\n", rfrag, rlevel));
		n->kids[ki] = lptr;
		n->counts[ki] = countnode234(lptr);
		if (lptr) lptr->parent = n;
	    }

	    while (1) {
		tmp = n;
		n = n->parent;
		level++;
		if (n == NULL)
		    break;
		ki = (n->kids[0] == tmp ? 0 :
		      n->kids[1] == tmp ? 1 :
		      n->kids[2] == tmp ? 2 : 3);
		n->counts[ki] = countnode234(n->kids[ki]);
		LOG(("  searched up to node %p, we were child %d of it\n",
		     n, ki));
		if ((left && ki != 0) ||
		    (!left && ki < 3 && n->elems[ki]))
		    break;
	    }

	    /*
	     * If n is NULL, we've come off the top of the tree in
	     * our search for another node to go with the fragment
	     * we've snipped off. Which means we're _done_ - the
	     * fragment is complete. All that remains is to add the
	     * _other_ fragment we've been collecting back into the
	     * larger part of the tree.
	     */
	    if (!n) {
		LOG(("  fell off edge, we are complete\n"));
		if (left) {
		    if (rsep) {
			int height = rlevel - level;
			LOG(("  final join: rfrag [%p], rsep [%p],"
			     " root [%p] heightdiff=%d\n",
			     rfrag, rsep, t->root, height));
			t->root = join234_internal(rfrag, rsep,
						   t->root, &height);
		    }
		    LOG(("  returning lfrag %p, keeping root %p\n",
			 lfrag, t->root));
		    return lfrag;
		} else {
		    if (lsep) {
			int height = level - llevel;
			LOG(("  final join: root [%p], lsep [%p],"
			     " lfrag [%p] heightdiff=%d\n",
			     t->root, lsep, lfrag, height));
			t->root = join234_internal(t->root, lsep,
						   lfrag, &height);
		    }
		    LOG(("  returning root %p, keeping rfrag %p\n",
			 t->root, rfrag));
		    lfrag = t->root;
		    t->root = rfrag;
		    return lfrag;
		}
	    }

	    /*
	     * We've now discovered node n, with kid index ki
	     * pointing down to where we've just come from. So if
	     * `left' is TRUE, we take the element to the left of
	     * that (by construction, it exists), split the node at
	     * that point, and continue the algorithm from there.
	     * If `left' is FALSE, we do the same thing with the
	     * element to the right.
	     */
	    LOG(("  found grandad %p: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
		 n,
		 n->kids[0], n->counts[0], n->elems[0],
		 n->kids[1], n->counts[1], n->elems[1],
		 n->kids[2], n->counts[2], n->elems[2],
		 n->kids[3], n->counts[3]));
	    LOG(("  we are child %d", ki));
	    if (left) {
		lsep = n->elems[--ki];
	    } else {
		rsep = n->elems[ki];
	    }
	    LOG((", removing element %d (%p)\n", ki, n->elems[ki]));

	    /*
	     * We want to shrink node n so that element ki
	     * vanishes. We can only do this if node n has more
	     * than one element. Fortunately, it does because we
	     * did the deletion transforms on the way down from the
	     * root to make sure all nodes we passed through were
	     * more than minimum size...
	     * 
	     * ... _unless_ node n is itself the root. In this case
	     * we need to remove the element in the root, remove
	     * the root node itself, and then we've split the tree
	     * into two slightly shorter subtrees.
	     */
	    
	    /* Ensure no 2-nodes show up anywhere _but_ at the root. */
	    assert(n->elems[1] || n == t->root);

	    if (!n->elems[1]) {
		/*
		 * This is the root.
		 */
		lptr = n->kids[0];
		rptr = n->kids[1];
		assert((left ? lsep : rsep) == n->elems[0]);
		lptr->parent = rptr->parent = NULL;
		sfree(n);

		LOG(("  hit and removed 2-root, lroot is %p, rroot is %p\n",
		     lptr, rptr));

		if (rsep) {
		    int height = rlevel - level;
		    LOG(("  final join: rfrag [%p], rsep [%p],"
			 " rroot [%p] heightdiff=%d\n",
			 rfrag, rsep, rptr, height));
		    rptr = join234_internal(rfrag, rsep, rptr, &height);
		}
		if (lsep) {
		    int height = level - llevel;
		    LOG(("  final join: lroot [%p], lsep [%p],"
			 " lfrag [%p] heightdiff=%d\n",
			 lptr, lsep, lfrag, height));
		    lptr = join234_internal(lptr, lsep, lfrag, &height);
		}
		LOG(("  after final joins, keeping rroot %p,"
		     " returning lroot %p\n", rptr, lptr));
		t->root = rptr;
		return lptr;
	    }

	    /*
	     * Shrink node n so that element ki vanishes. Set lptr
	     * and rptr to the two pointers brought together by
	     * this transformation.
	     */
	    lptr = n->kids[ki];
	    rptr = n->kids[ki+1];
	    for (i = ki; i < 4; i++) {
		if (i+1 < 4) {
		    n->kids[i] = n->kids[i+1];
		    n->counts[i] = n->counts[i+1];
		} else {
		    n->kids[i] = NULL;
		    n->counts[i] = 0;
		}
		if (i+1 < 3) {
		    n->elems[i] = n->elems[i+1];
		} else if (i < 3) {
		    n->elems[i] = NULL;
		}
	    }
	    LOG(("  grandad %p is now: %p/%d [%p] %p/%d [%p] %p/%d [%p] %p/%d\n",
		 n,
		 n->kids[0], n->counts[0], n->elems[0],
		 n->kids[1], n->counts[1], n->elems[1],
		 n->kids[2], n->counts[2], n->elems[2],
		 n->kids[3], n->counts[3]));

	    /*
	     * Now we need to insert lptr and rptr at kid position
	     * ki in node n - i.e. we can go round to the beginning
	     * of this loop again!
	     */
	}
    }
}
tree234 *splitpos234(tree234 *t, int index, int before) {
    tree234 *ret;
    node234 *n;
    int count;

    count = countnode234(t->root);
    if (index < 0 || index > count)
	return NULL;		       /* error */
    ret = newtree234(t->cmp);
    n = split234_internal(t, index);
    if (before) {
	/* We want to return the ones before the index. */
	ret->root = n;
    } else {
	/*
	 * We want to keep the ones before the index and return the
	 * ones after.
	 */
	ret->root = t->root;
	t->root = n;
    }
    return ret;
}
tree234 *split234(tree234 *t, void *e, cmpfn234 cmp, int rel) {
    int before;
    int index;

    assert(rel != REL234_EQ);

    if (rel == REL234_GT || rel == REL234_GE) {
	before = 1;
	rel = (rel == REL234_GT ? REL234_LE : REL234_LT);
    } else {
	before = 0;
    }
    if (!findrelpos234(t, e, cmp, rel, &index))
	index = 0;

    return splitpos234(t, index+1, before);
}

static node234 *copynode234(node234 *n, copyfn234 copyfn, void *copyfnstate) {
    int i;
    node234 *n2 = mknew(node234);

    for (i = 0; i < 3; i++) {
	if (n->elems[i] && copyfn)
	    n2->elems[i] = copyfn(copyfnstate, n->elems[i]);
	else
	    n2->elems[i] = n->elems[i];
    }

    for (i = 0; i < 4; i++) {
	if (n->kids[i]) {
	    n2->kids[i] = copynode234(n->kids[i], copyfn, copyfnstate);
	    n2->kids[i]->parent = n2;
	} else {
	    n2->kids[i] = NULL;
	}
	n2->counts[i] = n->counts[i];
    }

    return n2;
}
tree234 *copytree234(tree234 *t, copyfn234 copyfn, void *copyfnstate) {
    tree234 *t2;

    t2 = newtree234(t->cmp);
    t2->root = copynode234(t->root, copyfn, copyfnstate);
    t2->root->parent = NULL;

    return t2;
}

#ifdef TEST

/*
 * Test code for the 2-3-4 tree. This code maintains an alternative
 * representation of the data in the tree, in an array (using the
 * obvious and slow insert and delete functions). After each tree
 * operation, the verify() function is called, which ensures all
 * the tree properties are preserved:
 *  - node->child->parent always equals node
 *  - tree->root->parent always equals NULL
 *  - number of kids == 0 or number of elements + 1;
 *  - tree has the same depth everywhere
 *  - every node has at least one element
 *  - subtree element counts are accurate
 *  - any NULL kid pointer is accompanied by a zero count
 *  - in a sorted tree: ordering property between elements of a
 *    node and elements of its children is preserved
 * and also ensures the list represented by the tree is the same
 * list it should be. (This last check also doubly verifies the
 * ordering properties, because the `same list it should be' is by
 * definition correctly ordered. It also ensures all nodes are
 * distinct, because the enum functions would get caught in a loop
 * if not.)
 */

#include <stdarg.h>

#define srealloc realloc

/*
 * Error reporting function.
 */
void error(char *fmt, ...) {
    va_list ap;
    printf("ERROR: ");
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    printf("\n");
}

/* The array representation of the data. */
void **array;
int arraylen, arraysize;
cmpfn234 cmp;

/* The tree representation of the same data. */
tree234 *tree;

typedef struct {
    int treedepth;
    int elemcount;
} chkctx;

int chknode(chkctx *ctx, int level, node234 *node,
             void *lowbound, void *highbound) {
    int nkids, nelems;
    int i;
    int count;

    /* Count the non-NULL kids. */
    for (nkids = 0; nkids < 4 && node->kids[nkids]; nkids++);
    /* Ensure no kids beyond the first NULL are non-NULL. */
    for (i = nkids; i < 4; i++)
        if (node->kids[i]) {
            error("node %p: nkids=%d but kids[%d] non-NULL",
                   node, nkids, i);
        } else if (node->counts[i]) {
            error("node %p: kids[%d] NULL but count[%d]=%d nonzero",
                   node, i, i, node->counts[i]);
	}

    /* Count the non-NULL elements. */
    for (nelems = 0; nelems < 3 && node->elems[nelems]; nelems++);
    /* Ensure no elements beyond the first NULL are non-NULL. */
    for (i = nelems; i < 3; i++)
        if (node->elems[i]) {
            error("node %p: nelems=%d but elems[%d] non-NULL",
                   node, nelems, i);
        }

    if (nkids == 0) {
        /*
         * If nkids==0, this is a leaf node; verify that the tree
         * depth is the same everywhere.
         */
        if (ctx->treedepth < 0)
            ctx->treedepth = level;    /* we didn't know the depth yet */
        else if (ctx->treedepth != level)
            error("node %p: leaf at depth %d, previously seen depth %d",
                   node, level, ctx->treedepth);
    } else {
        /*
         * If nkids != 0, then it should be nelems+1, unless nelems
         * is 0 in which case nkids should also be 0 (and so we
         * shouldn't be in this condition at all).
         */
        int shouldkids = (nelems ? nelems+1 : 0);
        if (nkids != shouldkids) {
            error("node %p: %d elems should mean %d kids but has %d",
                   node, nelems, shouldkids, nkids);
        }
    }

    /*
     * nelems should be at least 1.
     */
    if (nelems == 0) {
        error("node %p: no elems", node, nkids);
    }

    /*
     * Add nelems to the running element count of the whole tree.
     */
    ctx->elemcount += nelems;

    /*
     * Check ordering property: all elements should be strictly >
     * lowbound, strictly < highbound, and strictly < each other in
     * sequence. (lowbound and highbound are NULL at edges of tree
     * - both NULL at root node - and NULL is considered to be <
     * everything and > everything. IYSWIM.)
     */
    if (cmp) {
	for (i = -1; i < nelems; i++) {
	    void *lower = (i == -1 ? lowbound : node->elems[i]);
	    void *higher = (i+1 == nelems ? highbound : node->elems[i+1]);
	    if (lower && higher && cmp(lower, higher) >= 0) {
		error("node %p: kid comparison [%d=%s,%d=%s] failed",
		      node, i, lower, i+1, higher);
	    }
	}
    }

    /*
     * Check parent pointers: all non-NULL kids should have a
     * parent pointer coming back to this node.
     */
    for (i = 0; i < nkids; i++)
        if (node->kids[i]->parent != node) {
            error("node %p kid %d: parent ptr is %p not %p",
                   node, i, node->kids[i]->parent, node);
        }


    /*
     * Now (finally!) recurse into subtrees.
     */
    count = nelems;

    for (i = 0; i < nkids; i++) {
        void *lower = (i == 0 ? lowbound : node->elems[i-1]);
        void *higher = (i >= nelems ? highbound : node->elems[i]);
	int subcount = chknode(ctx, level+1, node->kids[i], lower, higher);
	if (node->counts[i] != subcount) {
	    error("node %p kid %d: count says %d, subtree really has %d",
		  node, i, node->counts[i], subcount);
	}
        count += subcount;
    }

    return count;
}

void verifytree(tree234 *tree, void **array, int arraylen) {
    chkctx ctx;
    int i;
    void *p;

    ctx.treedepth = -1;                /* depth unknown yet */
    ctx.elemcount = 0;                 /* no elements seen yet */
    /*
     * Verify validity of tree properties.
     */
    if (tree->root) {
	if (tree->root->parent != NULL)
	    error("root->parent is %p should be null", tree->root->parent);
        chknode(&ctx, 0, tree->root, NULL, NULL);
    }
    printf("tree depth: %d\n", ctx.treedepth);
    /*
     * Enumerate the tree and ensure it matches up to the array.
     */
    for (i = 0; NULL != (p = index234(tree, i)); i++) {
        if (i >= arraylen)
            error("tree contains more than %d elements", arraylen);
        if (array[i] != p)
            error("enum at position %d: array says [%p]%s, tree says [%p]%s",
                   i, array[i], array[i], p, p);
    }
    if (ctx.elemcount != i) {
        error("tree really contains %d elements, enum gave %d",
               ctx.elemcount, i);
    }
    if (i < arraylen) {
        error("enum gave only %d elements, array has %d", i, arraylen);
    }
    i = count234(tree);
    if (ctx.elemcount != i) {
        error("tree really contains %d elements, count234 gave %d",
	      ctx.elemcount, i);
    }
}
void verify(void) { verifytree(tree, array, arraylen); }

void internal_addtest(void *elem, int index, void *realret) {
    int i, j;
    void *retval;

    if (arraysize < arraylen+1) {
        arraysize = arraylen+1+256;
        array = (array == NULL ? smalloc(arraysize*sizeof(*array)) :
                 srealloc(array, arraysize*sizeof(*array)));
    }

    i = index;
    /* now i points to the first element >= elem */
    retval = elem;                  /* expect elem returned (success) */
    for (j = arraylen; j > i; j--)
	array[j] = array[j-1];
    array[i] = elem;                /* add elem to array */
    arraylen++;

    if (realret != retval) {
        error("add: retval was %p expected %p", realret, retval);
    }

    verify();
}

void addtest(void *elem) {
    int i;
    void *realret;

    realret = add234(tree, elem);

    i = 0;
    while (i < arraylen && cmp(elem, array[i]) > 0)
        i++;
    if (i < arraylen && !cmp(elem, array[i])) {
        void *retval = array[i];       /* expect that returned not elem */
	if (realret != retval) {
	    error("add: retval was %p expected %p", realret, retval);
	}
    } else
	internal_addtest(elem, i, realret);
}

void addpostest(void *elem, int i) {
    void *realret;

    realret = addpos234(tree, elem, i);

    internal_addtest(elem, i, realret);
}

void delpostest(int i) {
    int index = i;
    void *elem = array[i], *ret;

    /* i points to the right element */
    while (i < arraylen-1) {
	array[i] = array[i+1];
	i++;
    }
    arraylen--;			       /* delete elem from array */

    if (tree->cmp)
	ret = del234(tree, elem);
    else
	ret = delpos234(tree, index);

    if (ret != elem) {
	error("del returned %p, expected %p", ret, elem);
    }

    verify();
}

void deltest(void *elem) {
    int i;

    i = 0;
    while (i < arraylen && cmp(elem, array[i]) > 0)
        i++;
    if (i >= arraylen || cmp(elem, array[i]) != 0)
        return;                        /* don't do it! */
    delpostest(i);
}

/* A sample data set and test utility. Designed for pseudo-randomness,
 * and yet repeatability. */

/*
 * This random number generator uses the `portable implementation'
 * given in ANSI C99 draft N869. It assumes `unsigned' is 32 bits;
 * change it if not.
 */
int randomnumber(unsigned *seed) {
    *seed *= 1103515245;
    *seed += 12345;
    return ((*seed) / 65536) % 32768;
}

int mycmp(void *av, void *bv) {
    char const *a = (char const *)av;
    char const *b = (char const *)bv;
    return strcmp(a, b);
}

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

char *strings[] = {
    "a", "ab", "absque", "coram", "de",
    "palam", "clam", "cum", "ex", "e",
    "sine", "tenus", "pro", "prae",
    "banana", "carrot", "cabbage", "broccoli", "onion", "zebra",
    "penguin", "blancmange", "pangolin", "whale", "hedgehog",
    "giraffe", "peanut", "bungee", "foo", "bar", "baz", "quux",
    "murfl", "spoo", "breen", "flarn", "octothorpe",
    "snail", "tiger", "elephant", "octopus", "warthog", "armadillo",
    "aardvark", "wyvern", "dragon", "elf", "dwarf", "orc", "goblin",
    "pixie", "basilisk", "warg", "ape", "lizard", "newt", "shopkeeper",
    "wand", "ring", "amulet"
};

#define NSTR lenof(strings)

void findtest(void) {
    static const int rels[] = {
	REL234_EQ, REL234_GE, REL234_LE, REL234_LT, REL234_GT
    };
    static const char *const relnames[] = {
	"EQ", "GE", "LE", "LT", "GT"
    };
    int i, j, rel, index;
    char *p, *ret, *realret, *realret2;
    int lo, hi, mid, c;

    for (i = 0; i < (int)NSTR; i++) {
	p = strings[i];
	for (j = 0; j < (int)(sizeof(rels)/sizeof(*rels)); j++) {
	    rel = rels[j];

	    lo = 0; hi = arraylen-1;
	    while (lo <= hi) {
		mid = (lo + hi) / 2;
		c = strcmp(p, array[mid]);
		if (c < 0)
		    hi = mid-1;
		else if (c > 0)
		    lo = mid+1;
		else
		    break;
	    }

	    if (c == 0) {
		if (rel == REL234_LT)
		    ret = (mid > 0 ? array[--mid] : NULL);
		else if (rel == REL234_GT)
		    ret = (mid < arraylen-1 ? array[++mid] : NULL);
		else
		    ret = array[mid];
	    } else {
		assert(lo == hi+1);
		if (rel == REL234_LT || rel == REL234_LE) {
		    mid = hi;
		    ret = (hi >= 0 ? array[hi] : NULL);
		} else if (rel == REL234_GT || rel == REL234_GE) {
		    mid = lo;
		    ret = (lo < arraylen ? array[lo] : NULL);
		} else
		    ret = NULL;
	    }

	    realret = findrelpos234(tree, p, NULL, rel, &index);
	    if (realret != ret) {
		error("find(\"%s\",%s) gave %s should be %s",
		      p, relnames[j], realret, ret);
	    }
	    if (realret && index != mid) {
		error("find(\"%s\",%s) gave %d should be %d",
		      p, relnames[j], index, mid);
	    }
	    if (realret && rel == REL234_EQ) {
		realret2 = index234(tree, index);
		if (realret2 != realret) {
		    error("find(\"%s\",%s) gave %s(%d) but %d -> %s",
			  p, relnames[j], realret, index, index, realret2);
		}
	    }
#if 0
	    printf("find(\"%s\",%s) gave %s(%d)\n", p, relnames[j],
		   realret, index);
#endif
	}
    }

    realret = findrelpos234(tree, NULL, NULL, REL234_GT, &index);
    if (arraylen && (realret != array[0] || index != 0)) {
	error("find(NULL,GT) gave %s(%d) should be %s(0)",
	      realret, index, array[0]);
    } else if (!arraylen && (realret != NULL)) {
	error("find(NULL,GT) gave %s(%d) should be NULL",
	      realret, index);
    }

    realret = findrelpos234(tree, NULL, NULL, REL234_LT, &index);
    if (arraylen && (realret != array[arraylen-1] || index != arraylen-1)) {
	error("find(NULL,LT) gave %s(%d) should be %s(0)",
	      realret, index, array[arraylen-1]);
    } else if (!arraylen && (realret != NULL)) {
	error("find(NULL,LT) gave %s(%d) should be NULL",
	      realret, index);
    }
}

void splittest(tree234 *tree, void **array, int arraylen) {
    int i;
    tree234 *tree3, *tree4;
    for (i = 0; i <= arraylen; i++) {
	tree3 = copytree234(tree, NULL, NULL);
	tree4 = splitpos234(tree3, i, 0);
	verifytree(tree3, array, i);
	verifytree(tree4, array+i, arraylen-i);
	join234(tree3, tree4);
	freetree234(tree4);	       /* left empty by join */
	verifytree(tree3, array, arraylen);
	freetree234(tree3);
    }
}

int main(void) {
    int in[NSTR];
    int i, j, k;
    int tworoot, tmplen;
    unsigned seed = 0;
    tree234 *tree2, *tree3, *tree4;
    int c;

    setvbuf(stdout, NULL, _IOLBF, 0);

    for (i = 0; i < (int)NSTR; i++) in[i] = 0;
    array = NULL;
    arraylen = arraysize = 0;
    tree = newtree234(mycmp);
    cmp = mycmp;

    verify();
    for (i = 0; i < 10000; i++) {
        j = randomnumber(&seed);
        j %= NSTR;
        printf("trial: %d\n", i);
        if (in[j]) {
            printf("deleting %s (%d)\n", strings[j], j);
            deltest(strings[j]);
            in[j] = 0;
        } else {
            printf("adding %s (%d)\n", strings[j], j);
            addtest(strings[j]);
            in[j] = 1;
        }
	findtest();
    }

    while (arraylen > 0) {
        j = randomnumber(&seed);
        j %= arraylen;
        deltest(array[j]);
    }

    freetree234(tree);

    /*
     * Now try an unsorted tree. We don't really need to test
     * delpos234 because we know del234 is based on it, so it's
     * already been tested in the above sorted-tree code; but for
     * completeness we'll use it to tear down our unsorted tree
     * once we've built it.
     */
    tree = newtree234(NULL);
    cmp = NULL;
    verify();
    for (i = 0; i < 1000; i++) {
	printf("trial: %d\n", i);
	j = randomnumber(&seed);
	j %= NSTR;
	k = randomnumber(&seed);
	k %= count234(tree)+1;
	printf("adding string %s at index %d\n", strings[j], k);
	addpostest(strings[j], k);
    }

    /*
     * While we have this tree in its full form, we'll take a copy
     * of it to use in split and join testing.
     */
    tree2 = copytree234(tree, NULL, NULL);
    verifytree(tree2, array, arraylen);/* check the copy is accurate */
    /*
     * Split tests. Split the tree at every possible point and
     * check the resulting subtrees.
     */
    tworoot = (!tree2->root->elems[1]);/* see if it has a 2-root */
    splittest(tree2, array, arraylen);
    /*
     * Now do the split test again, but on a tree that has a 2-root
     * (if the previous one didn't) or doesn't (if the previous one
     * did).
     */
    tmplen = arraylen;
    while ((!tree2->root->elems[1]) == tworoot) {
	delpos234(tree2, --tmplen);
    }
    printf("now trying splits on second tree\n");
    splittest(tree2, array, tmplen);
    freetree234(tree2);

    /*
     * Back to the main testing of uncounted trees.
     */
    while (count234(tree) > 0) {
	printf("cleanup: tree size %d\n", count234(tree));
	j = randomnumber(&seed);
	j %= count234(tree);
	printf("deleting string %s from index %d\n", (char *)array[j], j);
	delpostest(j);
    }
    freetree234(tree);

    /*
     * Finally, do some testing on split/join on _sorted_ trees. At
     * the same time, we'll be testing split on very small trees.
     */
    tree = newtree234(mycmp);
    cmp = mycmp;
    arraylen = 0;
    for (i = 0; i < 16; i++) {
	addtest(strings[i]);
	tree2 = copytree234(tree, NULL, NULL);
	splittest(tree2, array, arraylen);
	freetree234(tree2);
    }
    freetree234(tree);

    /*
     * Test silly cases of join: join(emptytree, emptytree), and
     * also ensure join correctly spots when sorted trees fail the
     * ordering constraint.
     */
    tree = newtree234(mycmp);
    tree2 = newtree234(mycmp);
    tree3 = newtree234(mycmp);
    tree4 = newtree234(mycmp);
    assert(mycmp(strings[0], strings[1]) < 0);   /* just in case :-) */
    add234(tree2, strings[1]);
    add234(tree4, strings[0]);
    array[0] = strings[0];
    array[1] = strings[1];
    verifytree(tree, array, 0);
    verifytree(tree2, array+1, 1);
    verifytree(tree3, array, 0);
    verifytree(tree4, array, 1);

    /*
     * So:
     *  - join(tree,tree3) should leave both tree and tree3 unchanged.
     *  - joinr(tree,tree2) should leave both tree and tree2 unchanged.
     *  - join(tree4,tree3) should leave both tree3 and tree4 unchanged.
     *  - join(tree, tree2) should move the element from tree2 to tree.
     *  - joinr(tree4, tree3) should move the element from tree4 to tree3.
     *  - join(tree,tree3) should return NULL and leave both unchanged.
     *  - join(tree3,tree) should work and create a bigger tree in tree3.
     */
    assert(tree == join234(tree, tree3));
    verifytree(tree, array, 0);
    verifytree(tree3, array, 0);
    assert(tree2 == join234r(tree, tree2));
    verifytree(tree, array, 0);
    verifytree(tree2, array+1, 1);
    assert(tree4 == join234(tree4, tree3));
    verifytree(tree3, array, 0);
    verifytree(tree4, array, 1);
    assert(tree == join234(tree, tree2));
    verifytree(tree, array+1, 1);
    verifytree(tree2, array, 0);
    assert(tree3 == join234r(tree4, tree3));
    verifytree(tree3, array, 1);
    verifytree(tree4, array, 0);
    assert(NULL == join234(tree, tree3));
    verifytree(tree, array+1, 1);
    verifytree(tree3, array, 1);
    assert(tree3 == join234(tree3, tree));
    verifytree(tree3, array, 2);
    verifytree(tree, array, 0);

    return 0;
}

#endif
