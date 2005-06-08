/*
 * Header file for bheap.c.
 * 
 * This file is copyright 2005 Simon Tatham.
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

#ifndef BHEAP_H
#define BHEAP_H

typedef struct bheap bheap;

typedef int (*bheap_cmpfn_t)(void *ctx, const void *x, const void *y);

/*
 * Create a new binary heap, of maximum size `maxelts'. `eltsize'
 * is the size of each element (use `sizeof'). `compare' is the
 * function that determines the ordering between elements;
 * `compare_ctx' is passed as its first argument.
 * 
 * If `direction' is +1, the heap sorts the largest things to the
 * top, so it can return the largest element easily. If it's -1,
 * the heap sorts the smallest things to the top.
 */
bheap *bheap_new(int maxelts, int eltsize, int direction,
		 bheap_cmpfn_t compare, void *compare_ctx);

/*
 * Add an element to the heap. Returns `elt', or NULL if the heap
 * is full.
 */
void *bheap_add(bheap *bh, void *elt);

/*
 * Non-destructively read the topmost element in the heap and store
 * it at `elt'. Returns `elt', or NULL if the heap is empty.
 */
void *bheap_topmost(bheap *bh, void *elt);

/*
 * Remove, and optionally also read, the topmost element in the
 * heap. Stores it at `elt' if `elt' is not itself NULL.
 * 
 * Returns `elt', or NULL if the heap is empty. Note that if you
 * pass elt==NULL the two cases can therefore not be distinguished!
 */
void *bheap_remove(bheap *bh, void *elt);

/*
 * Count the number of elements currently in the heap.
 */
int bheap_count(bheap *bh);

/*
 * Destroy a heap. If the elements in the heap need freeing, you'd
 * better make sure there aren't any left before calling this
 * function, because the heap code won't know about it and won't do
 * anything.
 */
void bheap_free(bheap *bh);

#endif /* BHEAP_H */
