/* nca   non-contiguous array management
 * 
 * This program is copyright (C) 1995 Simon Tatham and Jon Skeet. All rights
 * reserved. This program is part of the Tatham-Skeet Software Library; see
 * the library licence for details of copying permission.
 */

#ifndef LIB_NCA_H
#define LIB_NCA_H

typedef void *NCA ;

/* Set up an NCA, of zero initial size. */
NCA nca_new (long min_block, long element_size) ;

/* Destroy an entire NCA, and release the memory back to the OS. */
void nca_free (NCA nca) ;

/* Return a pointer to the nth element of an NCA. (NB These pointers do NOT
 * survive the action of the next few routines!)
 */
void *nca_element (NCA nca, long index) ;

/* Copy a section of an NCA into an array. */
void nca_get_array (void *array, NCA nca, long index, long count) ;

/* Replace a section of an NCA with the contents of an array. */
void nca_repl_array (NCA nca, void *array, long index, long count) ;

/* Insert an array of elements into an NCA at a given position. */
void nca_insert_array (NCA nca, void *array, long index, long count) ;

/* Delete a given number of elements from an NCA at a given position. */
void nca_delete (NCA nca, long index, long count) ;

/* Insert one NCA into the middle of another. The inserted NCA ceases to
 * exist as an independent entity. */
void nca_merge (NCA outer, NCA inner, long index) ;

/* Create an NCA by cutting a number of items out of the middle of another.
 */
NCA nca_cut (NCA nca, long index, long count) ;

/* Create an NCA by copying a number of items out of the middle of another.
 */
NCA nca_copy (NCA nca, long index, long count) ;

/* Return the current number of elements in an NCA. */
long nca_size (NCA nca) ;

#endif
