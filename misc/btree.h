/*
 * Flexible B-tree implementation. Supports reference counting for
 * copy-on-write, user-defined node properties, and variable
 * degree.
 * 
 * This file is copyright 2001,2004 Simon Tatham.
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

#ifndef BTREE_H
#define BTREE_H

typedef struct btree btree;
typedef void *bt_element_t;

typedef int (*cmpfn_t)(void *state, const bt_element_t, const bt_element_t);
typedef bt_element_t (*copyfn_t)(void *state, const bt_element_t);

enum {
    BT_REL_EQ, BT_REL_LT, BT_REL_LE, BT_REL_GT, BT_REL_GE
};

typedef union nodecomponent nodecomponent;
typedef nodecomponent *nodeptr;

/*
 * For type-checking purposes, and to ensure I don't accidentally
 * confuse node_addr with node_ptr during implementation, I'll
 * define node_addr for the in-memory case as being a struct
 * containing only a nodeptr.
 * 
 * This unfortunately needs to go in btree.h so that clients
 * writing user properties can know about the nodecomponent
 * structure.
 */
typedef struct {
    nodeptr p;
} node_addr;

/*
 * A B-tree node is a horrible thing when you're trying to be
 * flexible. It is of variable size, and it contains a variety of
 * distinct types of thing: nodes, elements, some counters, some
 * user-defined properties ... it's a horrible thing. So we define
 * it as an array of unions, each union being either an `int' or a
 * `bt_element_t' or a `node_addr'...
 */

union nodecomponent {
    int i;
    node_addr na;
    bt_element_t ep;
};

btree *bt_new(cmpfn_t cmp, copyfn_t copy, void *state, int mindegree);
void bt_free(btree *bt);
btree *bt_clone(btree *bt);
int bt_count(btree *bt);
bt_element_t bt_index(btree *bt, int index);
bt_element_t bt_index_w(btree *bt, int index);
bt_element_t bt_findrelpos(btree *bt, bt_element_t element, cmpfn_t cmp,
			   int relation, int *index);
bt_element_t bt_findrel(btree *bt, bt_element_t element, cmpfn_t cmp,
			int relation);
bt_element_t bt_findpos(btree *bt, bt_element_t element, cmpfn_t cmp,
			int *index);
bt_element_t bt_find(btree *bt, bt_element_t element, cmpfn_t cmp);
bt_element_t bt_replace(btree *bt, bt_element_t element, int index);
void bt_addpos(btree *bt, bt_element_t element, int pos);
bt_element_t bt_add(btree *bt, bt_element_t element);
bt_element_t bt_delpos(btree *bt, int pos);
bt_element_t bt_del(btree *bt, bt_element_t element);
btree *bt_join(btree *bt1, btree *bt2);
btree *bt_joinr(btree *bt1, btree *bt2);
btree *bt_splitpos(btree *bt, int index, int before);
btree *bt_split(btree *bt, bt_element_t element, cmpfn_t cmp, int rel);

#endif /* BTREE_H */
