/* Module to deal with various "continuous" markings, i.e. those that can be
 * carried over a newline (apart from slurs, which have their own module):
 * ties, hairpin crescendos/diminuendos, and 8va/8vb markings. */

#include <stdio.h>

#include "mus.h"
#include "header.h"
#include "pscript.h"
#include "drawing.h"
#include "misc.h"
#include "measure.h"
#include "contin.h"

enum {				       /* used in wrk.saveties[][] */
    TIE_NONE, TIE_DOWN, TIE_UP
};

extern void windup_ties(void) {
    int i, j;

    for (i=0; i<max_staves; i++)
	for (j=0; j<=MAXPITCH*2; j++)
	    if (wrk.ties[i][j].exists)
		put_tie (wrk.ties[i][j].x, wrk.ties[i][j].ax, xpos, axpos,
			 stave_y (i, STAVE_MIDDLE)+ph*(j-MAXPITCH),
			 wrk.ties[i][j].down);
}

extern void restart_ties(void) {
    int i, j;

    for (i=0; i<max_staves; i++)
	for (j=0; j<=MAXPITCH*2; j++)
	    wrk.ties[i][j].x = xpos, wrk.ties[i][j].ax = axpos;
}

extern void save_ties(void) {
    int i, j;

    for (i=0; i<max_staves; i++)
	for (j=0; j<=MAXPITCH*2; j++) {
	    if (wrk.ties[i][j].exists)
		wrk.saveties[i][j] = (wrk.ties[i][j].down ? TIE_DOWN :
				      TIE_UP);
	    else
		wrk.saveties[i][j] = TIE_NONE;
	}
}

extern void restore_ties(void) {
    int i, j;

    for (i=0; i<max_staves; i++)
	for (j=0; j<=MAXPITCH*2; j++) {
	    if (wrk.saveties[i][j] != TIE_NONE) {
		wrk.ties[i][j].exists = TRUE;
		wrk.ties[i][j].x = xpos;
		wrk.ties[i][j].ax = axpos;
		wrk.ties[i][j].down = (wrk.saveties[i][j] == TIE_DOWN);
	    } else
		wrk.ties[i][j].exists = FALSE;
	}
}

extern void windup_hairpins(void) {
    int i;

    for (i=0; i<max_staves; i++)
	if (wrk.hairpins[i].exists) {
	    /* relies on this routine not modifying anything but "exists" */
	    end_hairpin(i);
	    wrk.hairpins[i].exists = TRUE;
	}
}

extern void restart_hairpins(void) {
    int i;

    for (i=0; i<max_staves; i++)
	if (wrk.hairpins[i].exists)
	    begin_hairpin(i, wrk.hairpins[i].down);
}

extern void begin_hairpin(int stave, int down) {
    wrk.hairpins[stave].exists = TRUE;
    wrk.hairpins[stave].x = xpos;
    wrk.hairpins[stave].ax = axpos;
    wrk.hairpins[stave].down = down;
}

extern void end_hairpin(int stave) {
    int lwid, rwid;

    if (wrk.hairpins[stave].down)
	lwid = hairpin_width/2, rwid = 0;
    else
	lwid = 0, rwid = hairpin_width/2;

    put_line (wrk.hairpins[stave].x, wrk.hairpins[stave].ax,
	      stave_y(stave, STAVE_BOTTOM)-hairpin_yoff-lwid,
	      xpos, axpos, stave_y(stave, STAVE_BOTTOM)-hairpin_yoff-rwid,
	      thin_barline);
    put_line (wrk.hairpins[stave].x, wrk.hairpins[stave].ax,
	      stave_y(stave, STAVE_BOTTOM)-hairpin_yoff+lwid,
	      xpos, axpos, stave_y(stave, STAVE_BOTTOM)-hairpin_yoff+rwid,
	      thin_barline);

    wrk.hairpins[stave].exists = FALSE;
}

extern void windup_eightva(void) {
    int i;

    for (i=0; i<max_staves; i++)
	if (wrk.eightva[i].exists) {
	    /* relies on this routine not modifying anything but "exists" */
	    end_eightva(i);
	    wrk.eightva[i].exists = TRUE;
	}
}

extern void restart_eightva(void) {
    int i;

    for (i=0; i<max_staves; i++)
	if (wrk.eightva[i].exists)
	    begin_eightva(i, wrk.eightva[i].down);
}

extern void begin_eightva(int stave, int down) {
    wrk.eightva[stave].exists = TRUE;
    wrk.eightva[stave].x = xpos;
    wrk.eightva[stave].ax = axpos;
    wrk.eightva[stave].down = down;
}

extern void end_eightva(int stave) {
    int sign = (wrk.eightva[stave].down ? -1 : 1);
    int end = (sign < 0 ? STAVE_BOTTOM : STAVE_TOP);
    int numpos = (sign < 0 ? 0 : -smalldigit_ht);

    put_symbol (wrk.eightva[stave].x + eightva_fwd, wrk.eightva[stave].ax,
		stave_y(stave, end) + sign*eightva_yoff + numpos,
		s_small8);
    put_line (wrk.eightva[stave].x + eightva_lstart, wrk.eightva[stave].ax,
	      stave_y(stave, end) + sign*eightva_yoff,
	      xpos - eightva_back, axpos,
	      stave_y(stave, end) + sign*eightva_yoff,
	      thin_barline);
    put_line (xpos - eightva_back, axpos,
	      stave_y(stave, end) + sign*eightva_yoff,
	      xpos - eightva_back, axpos,
	      stave_y(stave, end) + sign*(eightva_yoff-smalldigit_ht),
	      thin_barline);

    wrk.eightva[stave].exists = FALSE;
}
