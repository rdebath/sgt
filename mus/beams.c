#include <stdio.h>
#include <stdlib.h>

#include "mus.h"
#include "measure.h"
#include "pscript.h"
#include "notes.h"
#include "header.h"
#include "drawing.h"
#include "beams.h"

static Beam **beams = NULL;	       /* global list of active beams */
static int beam_size = 0, beam_count = 0;
#define beam_delta 256

#ifndef min
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

static BeamEntry *new_entry (Beam *beam);

Beam *new_beam(int stave) {
    Beam *b = mmalloc (sizeof(Beam));

    b->head = b->tail = b->mark = NULL;
    b->minlen = (staves[stave].clef == CLEF_LINE ? stem_line_len :
		 stem_norm_len);
    b->stave = stave;
    if (beam_count >= beam_size) {
	beam_size += beam_delta;
	beams = mrealloc (beams, beam_size*sizeof(*beams));
    }
    beams[beam_count++] = b;
    return b;
}

void beam_break (Beam *beam) {
    BeamEntry *e = new_entry (beam);
    e->is_break = TRUE;
}

void add_to_beam (Beam *beam, int x, int ax, int y, int tails) {
    BeamEntry *e = new_entry (beam);
    e->is_break = FALSE;
    e->x = x;
    e->ax = ax;
    e->y = y;
    e->tails = tails;
    if (!beam->mark)
	beam->mark = e;
}

void deferred_flag (Beam *beam, int symbol, int height,
		    int above, int outside, int xpos, int axpos) {
    if (beam->tail) {
	DeferFlag *f = mmalloc (sizeof(DeferFlag));

	f->next = NULL;
	if (beam->tail->ftail)
	    beam->tail->ftail->next = f;
	else
	    beam->tail->fhead = f;
	beam->tail->ftail = f;

	f->sym = symbol;
	f->ht = height;
	f->above = above;
	f->outside = outside;
	f->xpos = xpos;
	f->axpos = axpos;
    }
}

void beam_shift_mark (int rx) {
    int i;
    BeamEntry *e;

    for (i=0; i<beam_count; i++) {
	for (e=beams[i]->mark; e; e=e->next) {
	    e->x += rx;
	    if (e->fhead) {
		DeferFlag *f = e->fhead;
		while (f) {
		    f->xpos += rx;
		    f = f->next;
		}
	    }
	}
	beams[i]->mark = NULL;
    }
}

static BeamEntry *new_entry (Beam *beam) {
    BeamEntry *e = mmalloc (sizeof(BeamEntry));

    e->next = NULL;
    if (beam->tail)
	beam->tail->next = e;
    else
	beam->head = e;
    beam->tail = e;
    e->fhead = e->ftail = NULL;
    e->sptr = NULL;

    return e;
}

void draw_beams (double hdsq) {
    int i, j, k, entries;
    BeamEntry *e;
    int startx, starty, endx, endy, ydiff;
    int stems;
    double beam_grad;
    struct TickLevels {
	int tick, x, y;
    } *array;

    for (i=0; i<beam_count; i++) {
	if (beams[i]->head->x == beams[i]->tail->x) {
	    error (beams[i]->line, beams[i]->col,
		   "beam contains only one note");
	    continue;
	} else if (beams[i]->head->x >= beams[i]->tail->x)
	    fatal (-1, -1, "internal fault: negative-width beam");

	/* project all the AX coordinates on to the X axis */
	for (e=beams[i]->head; e; e=e->next)
	    if (!e->is_break)
		e->x += e->ax * hdsq;

	/* calculate the start/end points of the beam, by the following
	 * algorithm. The description assumes the beam is on upward stems;
	 * the equivalent for downward ones is given in brackets.
	 * (1) Initially, let the beam travel from the given start point to
	 * the given end point.
	 * (2) If the beam gradient is greater than beam_grad_max, either
	 * positively or negatively, then move the lower end up (down) to
	 * fix it.
	 * (3) If any points on the beam are above (below) the beam, raise
	 * (lower) the beam as a whole until that is no longer the case.
	 * (4) Raise (lower) the whole beam by beams[i]->minlen [plus
	 * beam_vspace times the number of ticks minus 1?] */

	/* Step 1 */
	startx = beams[i]->head->x, endx = beams[i]->tail->x;
	starty = beams[i]->head->y, endy = beams[i]->tail->y;
	/* Step 2 */
	ydiff = (endx-startx) * beam_grad_max;
	if (abs(endy-starty) > ydiff) {
	    if ( (starty<endy) ^ (beams[i]->down) )   /* move the left end */
		starty = endy + (starty<endy ? -ydiff : ydiff);
	    else		       /* move the right end */
		endy = starty + (endy<starty ? -ydiff : ydiff);
	}
	beam_grad = (endy-starty) * 1.0 / (endx-startx);
	/* Step 3 */
	for (e=beams[i]->head; e; e=e->next) {
	    int ypos;
	    if (e->is_break)
		continue;
	    ypos = (e->x - startx) * beam_grad + starty;
	    if ( (e->y > ypos) ^ (beams[i]->down) ) {
		starty += (e->y - ypos);
		endy += (e->y - ypos);
	    }
	}
	/* Step 4 */
	ydiff = (beams[i]->down ? -1 : 1) * beams[i]->minlen;
	starty += ydiff, endy += ydiff;

	/* draw the stems to go on the beams */
	stems = 0;
	for (e=beams[i]->head; e; e=e->next) {
	    int ypos;
	    int nmin, nmax, smin, smax;

	    if (e->is_break)
		continue;
	    ypos = (e->x - startx) * beam_grad + starty;
	    put_line (e->x, 0, e->y, e->x, 0, ypos, thin_barline);

	    nmin = (e->y < ypos ? e->y : ypos);
	    nmax = (e->y < ypos ? ypos : e->y);
	    smin = nmin -= flag_ospace;
	    smax = nmax += flag_ospace;

	    if (e->fhead) {
		/* do the deferred flags */
		DeferFlag *f;

		for (f=e->fhead; f; f=f->next)
		    flag (beams[i]->stave, NULL, &nmin, &nmax, &smin, &smax,
			  f->sym, f->ht, f->above, f->outside, f->xpos,
			  f->axpos);
	    }
	    if (e->sptr) {
		/* deferred information goes into the slur structures */
		e->sptr->ymin = smin;
		e->sptr->ymax = smax;
	    }
	    /* save these Y coordinates for later, as they will come in
	     * handy when drawing the lower beams and we do not want to
	     * have to re-calculate them */
	    e->y = ypos;
	    /* also, count the number of stems because we will need to
	     * know how many we have in a moment */
	    stems++;
	}

	/* draw the principal (ie topmost) beam itself */
	put_beam (startx, starty, endx, endy);

	/* prepare this variable for the lower beams */
	ydiff = (beams[i]->down ? beam_vspace : -beam_vspace);

	/* Now we work out where all the lower beams go. We do this by
	 * allocating an array with an entry for each *half* of each space
	 * between two adjacent stems, and allocating that half-space a
	 * "tick level" (how many beams exist in that half-space). First we
	 * go through and put in all the "full beams", i.e. those that exist
	 * between two or more stems and are not just ticks on a single stem.
	 * Then we insert the ticks: a tick *always* goes to the left, unless
	 * it is on the leftmost stem or the stem it is on has more beams to
	 * the right than to the left. Finally we draw the beams in.
	 * Note that the array has (2*stems-1) entries rather than (2*stems-2)
	 * because each member is a *structure*, containing not only the
	 * tick level, but also the (x,y) coordinates of the point on the
	 * principal beam that lies immediately to the left of the space in
	 * question. So one more element is needed on the end of the array
	 * to store the (x,y) coordinates of the right hand end of the
	 * beam. */

	/* allocate the array */
	entries = stems*2-2;
	array = mmalloc ( (entries+1) * sizeof(*array) );
	/* populate its (x,y) coordinate fields */
	for (j=0, e=beams[i]->head; e; e=e->next) {
	    if (e->is_break)
		continue;
	    array[j].x = e->x;
	    array[j].y = e->y;
	    j++;
	    if (e->next) {
		array[j].x = (e->x + e->next->x)/2;
		array[j].y = (e->y + e->next->y)/2;
		j++;
	    }
	}
	/* insert the "full beams", i.e. those that are not ticks */
	for (j=0, e=beams[i]->head; e && e->next; e=e->next) {
	    if (e->is_break) {
		if (j>1)
		    array[j-1].tick = array[j-2].tick = 1;
	    } else {
		array[j].tick = array[j+1].tick = min(e->tails,
						      e->next->tails);
		j += 2;
	    }
	}
	array[entries].tick = 0;
	/* insert the ticks */
	for (j=0, e=beams[i]->head; e; e=e->next) {
	    if (e->is_break)
		continue;
	    if (e->tails > array[j].tick) {
		/* there is a tick here: put it in */
		if (j<1 || array[j].tick > array[j-1].tick)
		    array[j].tick = e->tails;   /* goes to the right */
		else
		    array[j-1].tick = e->tails;   /* goes to the left */
	    }
	    j += 2;
	}
	/* actually draw the lower beams */
	for (k=2; k<=4; k++) {
	    int sx = 0, sy = 0;
	    for (j=0; j<=entries; j++) {
		if ((j<1 || array[j-1].tick < k) && array[j].tick >= k)
		    sx = array[j].x, sy = array[j].y + ydiff*(k-1);
		else if ((j>=1 && array[j-1].tick >= k) && array[j].tick < k)
		    put_beam (sx, sy, array[j].x, array[j].y + ydiff*(k-1));
	    }
	}

	/* free up the data structures */
	for (e=beams[i]->head; e ;) {
	    BeamEntry *f = e;
	    if (e->fhead) {
		DeferFlag *df = e->fhead, *dft;
		while (df) {
		    dft = df->next;
		    mfree (df);
		    df = dft;
		}
	    }
	    e = e->next;
	    mfree (f);
	}
	mfree (beams[i]);
    }
    beam_count = 0;
}
