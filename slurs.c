#include <stdio.h>

#include "mus.h"
#include "drawing.h"
#include "measure.h"
#include "pscript.h"
#include "misc.h"
#include "header.h"
#include "beams.h"
#include "slurs.h"

static Slur **slurs = NULL;	       /* global list of active slurs */
static int slur_size = 0, slur_count = 0;
#define slur_delta 256

static int height (Slur *slur, SlurPoint *p, int xoff, int down);

Slur *new_slur(int stave) {
    Slur *s = mmalloc (sizeof(Slur));

    s->head = s->tail = s->mark = NULL;
    if (slur_count >= slur_size) {
	slur_size += slur_delta;
	slurs = mrealloc (slurs, slur_size*sizeof(*slurs));
    }
    s->centre_y = stave_y(stave, STAVE_MIDDLE);
    slurs[slur_count++] = s;
    return s;
}

void record_slur (Slur *slur, Beam *beam, int x, int ax,
		  int left, int right, int ymin, int ymax,
		  int down, int stem, int is_tail) {
    SlurPoint *p = mmalloc(sizeof(*p));

    if (is_tail) {
	p->next = NULL;
	if (slur->tail)
	    slur->tail->next = p;
	else {
	    slur->head = p;
	    slur->first_down = down;
	    slur->first_stem = stem;
	}
	slur->tail = p;
	if (!slur->mark)
	    slur->mark = p;
    } else {
	if (!slur->head || !slur->head->next)
	    return;		       /* can't be done! */
	p->next = slur->head->next;
	slur->head->next = p;
    }
    slur->last_down = down;
    slur->last_stem = stem;

    p->x = x;
    p->ax = ax;
    p->left = left;
    p->right = right;
    p->ymin = ymin;
    p->ymax = ymax;

    if (beam)
	beam->tail->sptr = p;
}

void slur_shift_mark (int rx) {
    int i;
    SlurPoint *p;

    for (i=0; i<slur_count; i++) {
	for (p=slurs[i]->mark; p; p=p->next)
	    p->x += rx;
	slurs[i]->mark = NULL;
    }
}

static struct SavedSlur {
    Slur *slur;
    int stave;
    int mline;
} *saved_slurs = NULL;

/* Save the active slurs at the end of a bar, in order to continue them in
 * the next bar. */
void save_slurs(void) {
    int i, j, mline_num;
    struct SavedSlur *sp;

    sp = saved_slurs = mmalloc ((wrk.mline_count+1) * sizeof(*saved_slurs));
    for (i=0; i<max_staves; i++) {
	mline_num = 0;
	for (j=0; j<wrk.mline_count; j++)
	    if (wrk.mlines[j]->stave_number==i) {
		if (wrk.mlines[j]->slur) {
		    sp->slur = wrk.mlines[j]->slur;
		    sp->stave = i;
		    sp->mline = mline_num;
		    sp++;
		}
		mline_num++;
	    }
    }
    sp->stave = -1;		       /* terminator */
}

/* Restore the active slurs at the beginning of a bar. */
void restore_slurs(void) {
    int i, j, mline_num;
    struct SavedSlur *sp;

    if (!saved_slurs)
	return;
    sp = saved_slurs;
    for (i=0; i<max_staves; i++) {
	mline_num = 0;
	for (j=0; j<wrk.mline_count; j++)
	    if (wrk.mlines[j]->stave_number==i) {
		if (sp->stave == i && sp->mline == mline_num)
		    wrk.mlines[j]->slur = sp++->slur;
		mline_num++;
	    }
    }
    mfree (saved_slurs);
}

/* Wind up the active slurs before a line break by placing a final entry on
 * each one. */
void windup_slurs (void) {
    struct SavedSlur *sp;

    if (!saved_slurs)
	return;

    for (sp = saved_slurs; sp->stave != -1; sp++) {
	record_slur (sp->slur, NULL, xpos, axpos, 0, 0,
		     stave_y(sp->stave, STAVE_BOTTOM),
		     stave_y(sp->stave, STAVE_TOP),
		     FALSE, FALSE, TRUE);
	sp->slur = NULL;
    }
}

/* Restart active slurs after a line break by recreating them with phoney
 * start entries. */
void restart_slurs (void) {
    struct SavedSlur *sp;

    if (!saved_slurs)
	return;

    for (sp = saved_slurs; sp->stave != -1; sp++) {
	sp->slur = new_slur (sp->stave);
	record_slur (sp->slur, NULL, xpos, axpos, 0, 0,
		     stave_y(sp->stave, STAVE_BOTTOM),
		     stave_y(sp->stave, STAVE_TOP),
		     FALSE, FALSE, TRUE);
    }
    slur_shift_mark(0);		       /* un-mark those entries */
}

void draw_slurs (double hdsq) {
    int i, down, ht;
    int sign, ystart, yend, xstart, xend;
    SlurPoint *p;

    for (i=0; i<slur_count; i++) {
	/* decide whether the slur goes up or down */
	if (!slurs[i]->first_stem ^ !slurs[i]->last_stem) {
	    /* Exactly one of the two end notes has a stem. So the slur
	     * goes up if that stem goes down, and vice versa. */
	    down = ! (slurs[i]->first_stem ? slurs[i]->first_down :
		      slurs[i]->last_down);
	} else if (slurs[i]->first_stem && slurs[i]->last_stem &&
		   (!slurs[i]->first_down)==(!slurs[i]->last_down)) {
	    /* Both notes have stems, and the stems both point the same
	     * way. Again, the slur goes in the opposite direction to the
	     * stems. */
	    down = !slurs[i]->first_down;
	} else {
	    /* Neither note has a stem, or alternatively both notes have
	     * stems but they point in opposite directions. We have to work
	     * on the relative distances between the min/max values and the
	     * centre line of the stave. */
	    int top = (slurs[i]->head->ymax + slurs[i]->tail->ymax)/2;
	    int bot = (slurs[i]->head->ymin + slurs[i]->tail->ymin)/2;
	    down = (top - slurs[i]->centre_y > slurs[i]->centre_y - bot);
	}

	/* project the ax coordinates on to the x axis */
	for (p = slurs[i]->head; p; p = p->next)
	    p->x += hdsq * p->ax;

	/* Calculate the slur height.
	 * 
	 * Slurs are drawn as parabolic arcs; this can be easily achieved
	 * using a Bezier curve. Start with the four control points strung
	 * out equidistantly along a line from the start point to the end
	 * point, then hoick the middle two points upwards by 4/3 of the
	 * desired maximum height of the parabola. ("Maximum height" is
	 * measured up from the centre of the aforementioned line.)
	 *
	 * Slurs have a minimum height of three-quarters of "slur_ht_min". */
	ht = slur_ht_min;
	for (p = slurs[i]->head; p->next ;) {
	    int new_ht;
	    new_ht = height (slurs[i], p, p->right, down);
	    if (ht < new_ht)
		ht = new_ht;
	    p = p->next;
	    new_ht = height (slurs[i], p, -p->left, down);
	    if (ht < new_ht)
		ht = new_ht;
	}

	/* And draw the slur! */
	if (down) {
	    sign = -1;
	    ystart = slurs[i]->head->ymin;
	    yend = slurs[i]->tail->ymin;
	} else {
	    sign = +1;
	    ystart = slurs[i]->head->ymax;
	    yend = slurs[i]->tail->ymax;
	}
	xstart = slurs[i]->head->x;
	xend = slurs[i]->tail->x;
	fprintf(out_fp, "newpath %d %d moveto %d %d %d %d %d %d curveto\n",
		xstart, ystart+slur_thick_min,
		(2*xstart+xend)/3, (2*ystart+yend)/3+sign*(ht+slur_thick_max),
		(xstart+xend*2)/3, (ystart+yend*2)/3+sign*(ht+slur_thick_max),
		xend, yend+slur_thick_min);
	fprintf(out_fp,
		"%d %d lineto %d %d %d %d %d %d curveto closepath fill\n",
		xend, yend,
		(xstart+xend*2)/3, (ystart+yend*2)/3 + sign * ht,
		(2*xstart+xend)/3, (2*ystart+yend)/3 + sign * ht,
		xstart, ystart);

	/* free up the data structures */
	for (p=slurs[i]->head; p ;) {
	    SlurPoint *tmp = p;
	    p = p->next;
	    mfree (tmp);
	}
	mfree (slurs[i]);
    }
    slur_count = 0;
}

/* Calculate the desired height of a slur passing exactly through the given
 * point. */
static int height (Slur *slur, SlurPoint *p, int xoff, int down) {
    double t, ybase, yreal;

    if (slur->tail->x == slur->head->x)
	return slur_ht_min;	       /* catch division by zero */
    t = (p->x+xoff - slur->head->x) * 1.0 / (slur->tail->x - slur->head->x);

    ybase = (t * (down ? slur->tail->ymin : slur->tail->ymax) +
	     (1-t) * (down ? slur->head->ymin : slur->head->ymax));
    yreal = (down ? p->ymin : p->ymax);

    if (t*(1-t)==0)
	return slur_ht_min;	       /* and again */
    return (down ? -1 : 1) * (yreal-ybase) / (3*t*(1-t));
}
