/* nca   non-contiguous array management
 * 
 * This program is copyright (C) 1995 Simon Tatham and Jon Skeet. All rights
 * reserved. This program is part of the Tatham-Skeet Software Library; see
 * the library licence for details of copying permission.
 */

#include "nca.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif

typedef struct _unit {
    struct _unit *next, *prev ;
    long size ;
    char body[0] ;
} unit ;

typedef struct _header {
    long minblock, elt_size, size ;
    unit *head, *tail ;
} header ;

NCA nca_new (long min_block, long element_size) {
    header *retval ;

    retval = (header *) malloc (sizeof(header)) ;

    if (!retval)
	return NULL ;

    retval->size = 0 ;
    retval->head = retval->tail = 0 ;
    retval->minblock = min_block ;
    retval->elt_size = element_size ;

    return (NCA) retval ;
}

/* since all the following routines have a parameter 'NCA nca' we can do: */

#define hdr   ( (header *) nca )

void nca_free (NCA nca) {
    unit *u = hdr->head ;

    while (u) {
	unit *w = u->next ;
	free (u) ;
	u = w ;
    }

    free (hdr) ;
}

static void pointtoitem (NCA nca, long index, unit **un, long *idx) {
    unit *u ;

    if (index <= hdr->size / 2) {
	/* first half of the array: search from the start */

	u = hdr->head ;
	while (u->next && index>=u->size) {
	    index -= u->size ;
	    u = u->next ;
	}
    } else {
	/* second half of the array; search from the back end */

	u = hdr->tail ;
	index -= hdr->size - u->size ;

	while (u->prev && index<0) {
	    u = u->prev ;
	    index += u->size ;
	}
    }

    *un = u ;
    *idx = index ;
}

void *nca_element (NCA nca, long index) {
    unit *u ;
    long idx ;

    if (index < 0 || index >= hdr->size)
	return NULL ;

    pointtoitem (nca, index, &u, &idx) ;
    return (void *) (u->body + idx * hdr->elt_size) ;
}

void nca_get_array (void *array, NCA nca, long index, long count) {
    unit *u ;
    long idx ;

    if (index < 0 || count<=0 || index+count > hdr->size)
	return ;

    pointtoitem (nca, index, &u, &idx) ;
    if (idx==u->size)
	u = u->next, idx = 0 ;
    while (count>0 && u) {
	long elts = u->size - idx ;
	if (elts>count)
	    elts = count ;
	memcpy (array, u->body+idx*hdr->elt_size, elts*hdr->elt_size) ;
	idx += elts ;
	if (idx==u->size)
	    u = u->next, idx = 0 ;
	count -= elts ;
	array = ((char *) array) + elts*hdr->elt_size ;
    }
}

void nca_repl_array (NCA nca, void *array, long index, long count) {
    unit *u ;
    long idx ;

    if (index < 0 || count<=0 || index+count > hdr->size)
	return ;

    pointtoitem (nca, index, &u, &idx) ;
    if (idx==u->size)
	u = u->next, idx = 0 ;
    while (count>0 && u) {
	long elts = u->size - idx ;
	if (elts>count)
	    elts = count ;
	memcpy (u->body+idx*hdr->elt_size, array, elts*hdr->elt_size) ;
	idx += elts ;
	if (idx==u->size)
	    u = u->next, idx = 0 ;
	count -= elts ;
	array = ((char *) array) + elts*hdr->elt_size ;
    }
}

static void make_unit (NCA nca, unit **u, long *index, unit *dest, long size) {
    long elts ;

    for (dest->size = 0; dest->size < size;) {
	if (*index>=(*u)->size)
	    *u = (*u)->next, *index = 0 ;
	/* we must copy either (*u)->size-*index or size-dest->size
	 * elements, whichever is the smaller */
	elts = (*u)->size-*index ;
	if (elts > size-dest->size)
	    elts = size-dest->size ;
	
	memcpy (dest->body + dest->size*hdr->elt_size, (*u)->body + *index*hdr->elt_size,
		elts * hdr->elt_size) ;
	
	(*index) += elts, dest->size += elts ;
    }
}

#define us(x) (sizeof(unit)+(x)->elt_size*(x)->minblock*2)
#define unitsize(x) us( (header *) x )

static void tidy_up (NCA nca, unit *start, unit *end, int dealloc) {
    unit *prev = start->prev, *next = end->next ;
    unit *u, *newstart, *newend, **p ;
    long index, size, twelve = (hdr->minblock + (hdr->minblock>>1)), twelves ;

    size = start->size, u = start ;
    while (u != end) {
	u = u->next ;
	size += u->size ;
    }

    /* we divide this section into "half full" blocks, i.e. size
     * = (minsize + maxsize) / 2.
     * then we do one bigger or smaller block at the end.
     * e.g. 12-12-8 or 12-12-16. (taking example 'minblock' to be 8)
     * If we need 12-12-5 to 12-12-7, we have to do less 12s,
     * i.e. 12-12-5 becomes 12-9-8 and 12-12-7 becomes 12-10-9.
     */

    if (size <= hdr->minblock*2)       /* special case: small bit */
	twelves = 0 ;
    else if (size % twelve >= hdr->minblock || size % twelve == 0)
	twelves = size / twelve ;
    else			       /* nasty case */
	twelves = size / twelve - 1 ;

    /* get some twelves */
    u = start ;
    index = 0 ;
    end->next = NULL ;

    p = &newstart ;
    newstart = newend = NULL ;
    size -= twelves * twelve ;
    while (twelves--) {
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = newend ;
	newend = *p ;
	newend->next = NULL ;
	p = &newend->next ;
	make_unit (nca, &u, &index, newend, twelve) ;
    }

    /* now do the rest of the block(s) */
    if (size > hdr->minblock * 2) {
	/* we need two blocks */
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = newend ;
	newend = *p ;
	newend->next = NULL ;
	p = &newend->next ;
	make_unit (nca, &u, &index, newend, size/2) ;
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = newend ;
	newend = *p ;
	newend->next = NULL ;
	p = &newend->next ;
	make_unit (nca, &u, &index, newend, size - (size/2)) ;
    } else if (size > 0) {
	/* just one will do */
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = newend ;
	newend = *p ;
	newend->next = NULL ;
	p = &newend->next ;
	make_unit (nca, &u, &index, newend, size) ;
    }
    if (prev)
	prev->next = newstart ;
    else
	hdr->head = newstart ;

    if (next)
	next->prev = newend ;
    else
	hdr->tail = newend ;

    newstart->prev = prev ;
    newend->next = next ;

    end->next = start->prev = NULL ;
    if (!dealloc)
	return ;
    for (u = start; u;) {
	unit *w = u->next ;
	free (u) ;
	u = w ;
    }
}

static void split_unit (NCA nca, unit *u, long index) {
    unit *v = malloc (unitsize(nca)) ;

    memcpy (v->body, u->body + index*hdr->elt_size, (u->size - index) * hdr->elt_size) ;
    v->size = u->size - index ;
    u->size = index ;
    v->next = u->next ;
    if (v->next)
	v->next->prev = v ;
    else
	hdr->tail = v ;
    u->next = v ;
    v->prev = u ;
}

void nca_insert_array (NCA nca, void *array, long index, long count) {
    unit *u, *v, *w ;
    long idx ;

    if (index < 0 || index > hdr->size)
	return ;

    if (hdr->size==0) {
	hdr->size = count ;
	if (count <= hdr->minblock * 2) {
	    hdr->head = hdr->tail = (unit *) malloc (unitsize(nca)) ;
	    hdr->head->prev = hdr->head->next = NULL ;
	    hdr->head->size = count ;
	    memcpy (hdr->head->body, array, count*hdr->elt_size) ;
	    return ;
	} else {
	    /* make one *big* block, and pass back to tidy_up */
	    hdr->head = hdr->tail = (unit *) malloc (sizeof(unit)+
						     count*hdr->elt_size) ;
	    hdr->head->prev = hdr->head->next = NULL ;
	    hdr->head->size = count ;
	    memcpy (hdr->head->body, array, count*hdr->elt_size) ;
	    tidy_up (nca, hdr->head, hdr->tail, 1) ;
	    return ;
	}
    }
    pointtoitem (nca, index, &u, &idx) ;
    hdr->size += count ;
    if (u->size + count <= hdr->minblock*2) {
	/* easy: just insert in this block */
	if (u->size != idx)
	    memmove (u->body + (idx + count)*hdr->elt_size,
		     u->body + idx*hdr->elt_size,
		     hdr->elt_size * (u->size-idx)) ;
	u->size += count ;
	memmove (u->body + idx*hdr->elt_size, array, count * hdr->elt_size) ;
	return ;
    }
    /* somehow or other, we're going to have to split this unit */
    if (u->size >= hdr->minblock*2 && u->size/2 + count <= hdr->minblock*2) {
	/* not bad... split this block, and insert into one sub-block */
	split_unit (nca, u, u->size/2) ;
	if (idx > u->size) {
	    idx -= u->size ;
	    u = u->next ;
	}
	if (u->size != idx)
	    memmove (u->body + (idx + count)*hdr->elt_size,
		     u->body + idx*hdr->elt_size,
		     hdr->elt_size * (u->size-idx)) ;
	u->size += count ;
	memmove (u->body + idx*hdr->elt_size, array, count * hdr->elt_size) ;
	return ;
    }
    /* nothing's going to make it easy for us... we're going to have to
     * do it the hard way. Create a new *big* chain, and link it into the
     * original one. */
    split_unit (nca, u, idx) ;
    v = u->next ;
    w = (unit *) malloc (sizeof(unit)+count*hdr->elt_size) ;
    u->next = w ;
    w->next = v ;
    v->prev = w ;
    w->prev = u ;
    w->size = count ;
    memcpy (w->body, array, count*hdr->elt_size) ;
    tidy_up (nca, u, v, 1) ;
    return ;
}

void nca_delete (NCA nca, long index, long count) {
    unit *u, *v ;
    long uindex, vindex ;

    if (index<0 || count<=0 || index+count>hdr->size)
	return ;		       /* brief sanity check there */
    pointtoitem (nca, index, &u, &uindex) ;
    pointtoitem (nca, index+count, &v, &vindex) ;
    if (hdr->size==count) {
	/* special case: we've deleted the entire nca */
	unit *w, *u = hdr->head ;
	while (u) {
	    w = u->next ;
	    free(u) ;
	    u = w ;
	}
	hdr->head = hdr->tail = NULL ;
	hdr->size = 0 ;
    } else if (u == v) {
	/* remove a chunk from the middle */
	memmove (u->body+uindex*hdr->elt_size, u->body+vindex*hdr->elt_size,
		 (u->size-vindex) * hdr->elt_size) ;
	u->size -= count ;
	hdr->size -= count ;
	if (u->size < hdr->minblock) {
	    if (u->next)
		tidy_up (nca, u, u->next, 1) ;
	    else if (u->prev)
		tidy_up (nca, u->prev, u, 1) ;
	}
    } else {
	unit *w ;
	split_unit (nca, u, uindex) ;
	split_unit (nca, v, vindex) ;
	w = u->next ;
	if ( (u->next = v->next) )
	    u->next->prev = u ;
	else
	    hdr->tail = u ;
	v->next = NULL ;
	while (w) {
	    v = w->next ;
	    free (w) ;
	    w = v ;
	}
	hdr->size -= count ;
	if (u->next->next)
	    tidy_up (nca, u, u->next->next, 1) ;
	else if (u->prev && u->next)
	    tidy_up (nca, u->prev, u->next, 1) ;
	else if (u->next)
	    tidy_up (nca, u, u->next, 1) ;
	else if (u->prev)
	    tidy_up (nca, u->prev, u, 1) ;
    }
}

#define otr ( (header *) outer)	       /* for this routine only */
#define inr ( (header *) inner)

void nca_merge (NCA outer, NCA inner, long index) {
    unit *u ;
    long uindex ;

    if (index<0 || index>otr->size || otr->elt_size != inr->elt_size ||
	inr->size==0)
	return ;		       /* sanity checking */

    if (!otr->size) {		       /* the outer is zero size */
	otr->head = inr->head ;
	otr->tail = inr->tail ;
	if (inr->elt_size != otr->elt_size)
	    tidy_up (outer, otr->head, otr->tail, 1) ;
	inr->head = inr->tail = NULL ;
	otr->size = inr->size ;
	nca_free (inner) ;
	return ;
    }
    pointtoitem (outer, index, &u, &uindex) ;
    if (uindex==u->size && u->next) {
	uindex = 0 ;
	u = u->next ;
    }
    if (uindex==u->size) {	       /* now it's *at* the end */
	inr->head->prev = otr->tail ;
	otr->tail->next = inr->head ;
	otr->tail = inr->tail ;
	otr->size += inr->size ;
	if (inr->minblock != otr->minblock)
	    tidy_up (outer, inr->head->prev, otr->tail, 1) ;
	else
	    tidy_up (outer, inr->head->prev, inr->head, 1) ;
    } else if (index==0) {	       /* now it's *at* the start */
	inr->tail->next = otr->head ;
	otr->head->prev = inr->tail ;
	otr->head = inr->head ;
	otr->size += inr->size ;
	if (inr->minblock != otr->minblock)
	    tidy_up (outer, otr->head, inr->tail->next, 1) ;
	else
	    tidy_up (outer, inr->tail, inr->tail->next, 1) ;
    } else {
	if (uindex==0)		       /* fits between 2 blocks */
	    u = u->prev ;
	else			       /* have to split a block */
	    split_unit (outer, u, uindex) ;
	inr->tail->next = u->next ;
	u->next->prev->next = u->next ;
	u->next = inr->head ;
	inr->tail->next->prev = inr->tail ;
	u->next->prev = u ;
	otr->size += inr->size ;
	if (inr->minblock != otr->minblock)
	    tidy_up (outer, u, inr->tail->next, 1) ;
	else {
	    unit *uu = inr->tail->next ;
	    tidy_up (outer, inr->head->prev, inr->head, 1) ;
	    if (uu)
		tidy_up (outer, uu->prev, uu, 1) ;
	}
    }
    inr->head = inr->tail = NULL ;
    nca_free (inner) ;
}

#undef otr
#undef inr

#define ret ( (header *) retn )

NCA nca_cut (NCA nca, long index, long count) {
    NCA retn ;
    unit *u, *v ;
    long uindex, vindex ;

    if (index<0 || count<0 || index+count > hdr->size)
	return NULL ;

    retn = nca_new (hdr->minblock, hdr->elt_size) ;

    if (count==hdr->size && index==0) {
	/* special case: cut the whole thing */
	ret->size = hdr->size ;
	ret->head = hdr->head ;
	ret->tail = hdr->tail ;
	hdr->size = 0 ;
	hdr->head = hdr->tail = NULL ;
	return retn ;
    }

    if (count==0)
	return retn ;

    pointtoitem (nca, index, &u, &uindex) ;
    split_unit (nca, u, uindex) ;

    pointtoitem (nca, index+count, &v, &vindex) ;
    split_unit (nca, v, vindex) ;

    ret->head = u->next ;
    ret->tail = v ;

    if ( (u->next = v->next) )
	u->next->prev = u ;
    else
	hdr->tail = u ;

    ret->tail->next = ret->head->prev = NULL ;

    if (u->next->next)
	tidy_up (nca, u, u->next->next, 1) ;
    else if (u->prev)
	tidy_up (nca, u->prev, u->next, 1) ;
    else
	tidy_up (nca, u, u->next, 1) ;
    if (ret->head->next == ret->tail ||
	(ret->head->next && ret->head->next->next == ret->tail))
	tidy_up (ret, ret->head, ret->tail, 1) ;
    else {
	if (ret->head->next && ret->head->next != ret->tail)
	    tidy_up (ret, ret->head, ret->head->next, 1) ;
	if (ret->tail->prev && ret->tail->prev != ret->head)
	    tidy_up (ret, ret->tail->prev, ret->tail, 1) ;
    }

    ret->size = count ;
    hdr->size -= count ;
    return ret ;
}

NCA nca_copy (NCA nca, long index, long count) {
    NCA retn ;
    unit *u, **p ;
    long idx, size, twelve = (hdr->minblock + (hdr->minblock>>1)), twelves ;

    if (index<0 || count<0 || index+count > hdr->size)
	return NULL ;

    retn = nca_new (hdr->minblock, hdr->elt_size) ;

    size = count ;
    pointtoitem (nca, index, &u, &idx) ;

    /* we divide this section into "half full" blocks, i.e. size
     * = (minsize + maxsize) / 2.
     * then we do one bigger or smaller block at the end.
     * e.g. 12-12-8 or 12-12-16. (taking example 'minblock' to be 8)
     * If we need 12-12-5 to 12-12-7, we have to do less 12s,
     * i.e. 12-12-5 becomes 12-9-8 and 12-12-7 becomes 12-10-9.
     */

    if (size <= hdr->minblock*2)       /* special case: small bit */
	twelves = 0 ;
    else if (size % twelve >= hdr->minblock || size % twelve == 0)
	twelves = size / twelve ;
    else			       /* nasty case */
	twelves = size / twelve - 1 ;

    /* get some twelves */
    p = &ret->head ;
    ret->head = ret->tail = NULL ;
    size -= twelves * twelve ;
    while (twelves--) {
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = ret->tail ;
	ret->tail = *p ;
	ret->tail->next = NULL ;
	p = &ret->tail->next ;
	make_unit (nca, &u, &idx, ret->tail, twelve) ;
    }

    /* now do the rest of the block(s) */
    if (size > hdr->minblock * 2) {
	/* we need two blocks */
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = ret->tail ;
	ret->tail = *p ;
	ret->tail->next = NULL ;
	p = &ret->tail->next ;
	make_unit (nca, &u, &idx, ret->tail, size/2) ;
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = ret->tail ;
	ret->tail = *p ;
	ret->tail->next = NULL ;
	p = &ret->tail->next ;
	make_unit (nca, &u, &idx, ret->tail, size - (size/2)) ;
    } else if (size > 0) {
	/* just one will do */
	*p = (unit *) malloc (unitsize(nca)) ;
	(*p)->prev = ret->tail ;
	ret->tail = *p ;
	ret->tail->next = NULL ;
	p = &ret->tail->next ;
	make_unit (nca, &u, &idx, ret->tail, size) ;
    }

    ret->size = count ;
    ret->head->prev = ret->tail->next = NULL ;

    return ret ;
}

long nca_size (NCA nca) {
    return hdr->size ;
}
