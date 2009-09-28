#include <stdio.h>
#include "mus.h"
#include "header.h"
#include "melody.h"
#include "preproc.h"

/* This module handles the preprocessing phase of reading a MUS file. We must:
 *  (a) go through and determine which way every note stem goes,
 *      making sure that beams have all the associated stems going
 *      the same way. At the same time, actually remove the <p> and
 *      <d> stream controls from the melody lines. Also sort the
 *      note heads so that they go one way from the tip of the
 *      stem.
 * 
 *    - FIXME! Stemless notes - semibreves and breves - should have
 *      a canonical stem direction. Otherwise, consider the case
 *      where two staves contain breve chords, and both chords
 *      contain adjacent notes. You probably don't want one central
 *      line of notes, and some displaced notes on one side in one
 *      chord, and some on the other side in the other chord. That
 *      would put the notes in three columns, where two is more
 *      sane.
 * 
 *  (b) adjust the "duration" fields in all NoteStem structures, to
 *      cope with ntuplets. Initially durations are measured in
 *      hemidemisemiquavers, so that a semibreve is 64, but we want
 *      to multiply up by "factor" on non- ntuplets and reduce
 *      ntupletted notes by their ntuplet fraction. Note that we
 *      retain the ntuplet stream controls, though, because they
 *      will be necessary in placing the "3" or whatever above the
 *      affected notes.
 *  (c) adjust the "pitch" fields in all NoteHead structures, to
 *      measure pitch in diatonic notes from the middle line of the
 *      stave, rather than from Middle C. This includes converting
 *      PITCH_NONE to zero, so that single- line staves are handled
 *      almost no differently.
 */

/* Initially, input pitches may range from -63 (C-9) to +63 (C+9), since
 * octaves above 9 are not permitted by the syntax. However, any note may be
 * shifted by up to six tones whilst relocating the pitch to be measured from
 * the middle of the stave as specified in (c) above. So we must maintain an
 * array from -69 to +69 of integers. The actual array is "notes_x", but we
 * will refer to it using the macro "notes" so that our actual array indices
 * can be the pitch values.
 * 
 * Note: although this is a static array, it is *not* an arbitrary limit, and
 * therefore does not go against my design goals. */
static unsigned int notes_x[2*MAXPITCH+1];
#define notes (notes_x+MAXPITCH)

static void clear_note_array(void);
static int correct_direction(int);
static void sort_stem(NoteHead **head, NoteHead **tail, int direction);

void preprocess_melody (void) {
    MelodyLine **ml;
    int i;

    for (ml = wrk.mlines, i = wrk.mline_count; i; i--, ml++) {
	/* process one melody line */

	MelodyEvent *me;
	MelodyEvent *beam_start;
	int force, iforce;	       /* -1 for down, 1 for up, 0 for none */
	int currfact;		       /* for ntupletting */
	int ntupletting = FALSE, ntup_start = 0, ntup_num = 0;
	NoteStem *last_note = NULL;
	int clef = staves[(*ml)->stave_number].clef;

	/* initial stem direction forcing */
	if ((*ml)->stems_force)
	    iforce = ((*ml)->stems_down ? -1 : 1);
	else if (clef == CLEF_LINE)
	    iforce = 1;		       /* single-line staves force up */
	else
	    iforce = 0;

	/* initial duration multiplier */
	currfact = wrk.factor;

	/* we're not beaming notes, to begin with */
	beam_start = NULL;

	me = (*ml)->head;
	force = iforce;
	while (me) {
	    if (me->type == EV_STREAM && (me->u.s.type == SC_FORCE_UP ||
					  me->u.s.type == SC_FORCE_DOWN)) {
		if (beam_start)
		    error (me->line, me->col,
			   "can't process <p> or <d> within a beam");
		else
		    force = (me->u.s.type == SC_FORCE_DOWN ? -1 : 1);

		if (me->prev)
		    me->prev->next = me->next;
		else
		    (*ml)->head = me->next;

		if (me->next)
		    me->next->prev = me->prev;
		else
		    (*ml)->tail = me->prev;

		me->type = EV_SGARBAGE;
	    } else if (me->type == EV_STREAM && me->u.s.type == SC_NTUPLET) {
		if (ntupletting) {     /* ntuplet already in progress */
		    error (me->line, me->col, "cannot nest ntuplets");
		    me->type = EV_SGARBAGE;
		} else {
		    currfact = ((wrk.factor / me->u.s.ntuplet_denom) *
				me->u.s.ntuplet_num);
		    ntupletting = TRUE;
		    ntup_start = TRUE;
		    ntup_num = me->u.s.ntuplet_denom;
		    last_note = NULL;
		}
		me->type = EV_SGARBAGE;
	    } else if (me->type == EV_STREAM &&
		       me->u.s.type == SC_NTUPLET_END) {
		if (!ntupletting) {    /* get confused */
		    error (me->line, me->col,
			   "`)' encountered, ntuplet not in progress");
		    me->type = EV_SGARBAGE;
		} else {
		    currfact = wrk.factor;
		    ntupletting = FALSE;
		    if (last_note)
			last_note->ntuplet = ntup_num;
		}
		me->type = EV_SGARBAGE;
	    } else if (me->type == EV_STREAM &&
		       me->u.s.type == SC_BEAMSTART) {
		beam_start = me;
		clear_note_array();
	    } else if (me->type == EV_STREAM &&
		       me->u.s.type == SC_BEAMSTOP) {
		/* go through from beam_start to here, forcing the stems
		 * whichever way we decided */
		if (!force)
		    force = correct_direction(clef);
		while (beam_start && beam_start != me) {
		    if (beam_start->type == EV_NOTE)
			beam_start->u.n.stem_down = (force<0);
		    beam_start = beam_start->next;
		}
		beam_start = NULL;
		force = iforce;
	    } else if (me->type == EV_NOTE) {
		NoteHead *nh;

		if (ntup_start) {
		    me->u.n.ntuplet = NTUP_START;
		    ntup_start = FALSE;
		} else
		    me->u.n.ntuplet = NTUP_NONE;
		last_note = &me->u.n;

		me->u.n.duration *= currfact;
		if (!beam_start)
		    clear_note_array();

		/* count the notes in every pitch */
		for (nh = me->u.n.hhead; nh; nh = nh->next) {
		    if (nh->pitch == PITCH_NONE) {
			if (clef != CLEF_LINE) {
			    error (me->line, me->col, "cannot process"
				   " unpitched note on multi-line stave");
			    me->type = EV_NGARBAGE;
			} else
			    nh->pitch = 0;
		    } else if (nh->pitch != PITCH_REST) {
			if (clef == CLEF_LINE) {
			    error (me->line, me->col, "cannot process"
				   " pitched note on single-line stave");
			    me->type = EV_NGARBAGE;
			}
			nh->pitch -= centre_pitch (clef);
			notes[nh->pitch]++;
		    }
		}

		if (!beam_start) {
		    if (!force)
			force = correct_direction(clef);
		    me->u.n.stem_down = (force < 0);
		    force = iforce;
		}

		/*
		 * Now stem_down is correct; so sort.
		 */
		sort_stem(&me->u.n.hhead, &me->u.n.htail,
			  me->u.n.stem_down ? -1 : +1);
	    }
	    me = me->next;
	}
    }
}

static void clear_note_array(void) {
    int i;
    for (i=0; i < sizeof(notes_x)/sizeof(*notes_x); i++)
	notes_x[i] = 0;
}

static int correct_direction(int clef) {
    int top = +69;
    int bot = -69;

    while (top > bot) {
	if (notes[top] < notes[bot])
	    return 1;		       /* stem goes up */
	else if (notes[top] > notes[bot])
	    return -1;		       /* stem goes down */
	top--;
	bot++;
    }

    /* The notes are exactly balanced around the centre of the stave. So we
     * now go *by* the centre of the stave: balanced notes have stems going
     * down if the centre of the stave is above Middle C, or up if the centre
     * of the stave is on or below Middle C. */
    return (centre_pitch(clef) > 0 ? -1 : 1);
}


/*
 * Sort a linked list of note heads.
 */
static void sort_stem(NoteHead **head, NoteHead **tail, int direction) {
    NoteHead *p, *q, *e;
    int insize, nmerges, psize, qsize, i;

    insize = 1;

    while (1) {
        p = (*head);
        (*head) = NULL;
        (*tail) = NULL;

        nmerges = 0;  /* count number of merges we do in this pass */

        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
		q = q->next;
                if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {

                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {
		    /* p is empty; e must come from q. */
		    e = q; q = q->next; qsize--;
		} else if (qsize == 0 || !q) {
		    /* q is empty; e must come from p. */
		    e = p; p = p->next; psize--;
		} else if (direction * (p->pitch - q->pitch) <= 0) {
		    /* First element of p is lower (or same);
		     * e must come from p. */
		    e = p; p = p->next; psize--;
		} else {
		    /* First element of q is lower; e must come from q. */
		    e = q; q = q->next; qsize--;
		}

                /* add the next element to the merged list */
		if ((*tail)) {
		    (*tail)->next = e;
		} else {
		    (*head) = e;
		}
		(*tail) = e;
            }

            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }
	(*tail)->next = NULL;

        /* If we have done only one merge, we're finished. */
        if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
            return;

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}
