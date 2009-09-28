#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "mus.h"
#include "drawing.h"
#include "melody.h"
#include "notes.h"
#include "misc.h"
#include "pscript.h"
#include "measure.h"
#include "header.h"

/* This module handles the actual processing of melody line events and the
 * drawing of the consequent symbols. In some sense all this code belongs in
 * "drawing.c" but I didn't want to create a monster module. */

typedef struct {
    int perm_left, perm_right;
    int temp_left, temp_right;
} note_extent;

static void n_centre (note_extent *, int, int);
static void n_left (note_extent *, int);
static void n_right (note_extent *, int);

void stream_controls(void) {
    int i, j;

    for (i=0; i<wrk.mline_count; i++) {
	while (wrk.mline_ptr[i] && wrk.mline_ptr[i]->type != EV_NOTE &&
	       (wrk.mline_ptr[i]->type != EV_STREAM ||
		wrk.mline_ptr[i]->u.s.type != SC_ZERONOTE)) {
	    MelodyEvent *me = wrk.mline_ptr[i];
	    StreamControl *sc;
	    int stave = wrk.mlines[i]->stave_number;

	    wrk.mline_ptr[i] = wrk.mline_ptr[i]->next;

	    if (me->type != EV_STREAM)
		continue;
	    sc = &me->u.s;
	    switch (sc->type) {
	      case SC_BEAMSTART:
		if (wrk.mlines[i]->beam)
		    error (me->line, me->col, "attempt to nest `[' and `]'");
		else {
		    wrk.mlines[i]->beam=new_beam(stave);
		    wrk.mlines[i]->beam->line = me->line;
		    wrk.mlines[i]->beam->col = me->col;
		}
		break;
	      case SC_BEAMBREAK:
		if (!wrk.mlines[i]->beam)
		    error (me->line, me->col,
			   "encountered `]-[' outside `[' and `]'");
		else
		    beam_break (wrk.mlines[i]->beam);
		break;
	      case SC_BEAMSTOP:
		if (!wrk.mlines[i]->beam)
		    error (me->line, me->col,
			   "encountered `]' without matching `['");
		else
		    wrk.mlines[i]->beam = NULL;
		break;
	      case SC_SLURSTART:
		if (wrk.mlines[i]->slur)
		    error (me->line, me->col, "attempt to nest `/' and `\\'");
		else
		    wrk.mlines[i]->slur=new_slur(stave);
		break;
	      case SC_SLURSTOP:
		if (!wrk.mlines[i]->slur)
		    error (me->line, me->col,
			   "encountered `\\' without matching `/'");
		else
		    wrk.mlines[i]->slur = NULL;
		break;
	      case SC_8VA:
		if (wrk.eightva[stave].exists) {
		    if (wrk.eightva[stave].down)
			end_eightva(stave);
		    else
			error (me->line, me->col,
			       "encountered nested `<8va>' directives");
		} else
		    begin_eightva(stave, FALSE);
		break;
	      case SC_8VB:
		if (wrk.eightva[stave].exists) {
		    if (!wrk.eightva[stave].down)
			end_eightva(stave);
		    else
			error (me->line, me->col,
			       "encountered nested `<8vb>' directives");
		} else
		    begin_eightva(stave, TRUE);
		break;
	      case SC_BREATH:
		put_symbol (xpos, axpos, stave_y (stave, STAVE_TOP),
			    s_breath);
		xpos += breath_width;
		break;
	      case SC_GENPAUSE:
		for (j=0; j<max_staves; j++) {
		    int yy = stave_y (j, STAVE_TOP);
		    put_line (xpos, axpos, yy - genpause_dy/2,
			      xpos + genpause_dx, axpos, yy + genpause_dy/2,
			      genpause_thick);
		    put_line (xpos + genpause_xoff, axpos, yy - genpause_dy/2,
			      xpos + genpause_dx + genpause_xoff, axpos,
			      yy + genpause_dy/2, genpause_thick);
		}
		xpos += genpause_dx + genpause_xoff;
		break;
	      case SC_TURN:
		put_symbol (xpos + turn_width/2, axpos,
			    stave_y(stave, STAVE_TOP) + flag_ospace,
			    s_turn);
		xpos += turn_width;
		break;
	      case SC_INVTURN:
		put_symbol (xpos + turn_width/2, axpos,
			    stave_y(stave, STAVE_TOP) + flag_ospace +
			    turn_inv_space, s_turn);
		put_line (xpos + turn_width/2, axpos,
			  stave_y(stave,STAVE_TOP) + flag_ospace,
			  xpos + turn_width/2, axpos,
			  stave_y(stave,STAVE_TOP) + flag_ospace +
			  2*turn_inv_space + flag_ht[NF_TURN],
			  turn_inv_thick);
		xpos += turn_width;
		break;
	      case SC_DYNAMIC:
		put_text (xpos, axpos,
			  stave_y (stave, STAVE_BOTTOM) - dynamic_yoff,
			  sc->text, ALIGN_LEFT);
		break;
	      case SC_TEXT_ABOVE:
		put_text (xpos, axpos,
			  stave_y (stave, STAVE_TOP) + text_yoff,
			  sc->text, ALIGN_LEFT);
		break;
	      case SC_TEXT_BELOW:
		put_text (xpos, axpos,
			  stave_y (stave, STAVE_BOTTOM) - text_below_yoff,
			  sc->text, ALIGN_LEFT);
		break;
	      case SC_LYRICS:
		wrk.mlines[i]->lyrics = sc->text;
		break;
	      case SC_CRESC:
		if (wrk.hairpins[stave].exists) {
		    end_hairpin(stave);
		    if (!wrk.hairpins[stave].down)
			break;
		}
		begin_hairpin(stave, FALSE);
		break;
	      case SC_DIM:
		if (wrk.hairpins[stave].exists) {
		    end_hairpin(stave);
		    if (wrk.hairpins[stave].down)
			break;
		}
		begin_hairpin(stave, TRUE);
		break;
	      default:
		fatal (me->line, me->col,
		       "internal fault: stream control of unknown type %d",
		       sc->type);
		break;
	    }
	}
    }
}

void process_notes(void) {
    int i, j, k, new_time_pos = INT_MAX;
    note_extent e;
    char notepos_x[MAXPITCH*2+1];
#define notepos (notepos_x+MAXPITCH)
    int curr_acc_x[2*MAXPITCH+1];
#define curr_acc (curr_acc_x+MAXPITCH)

    put_mark();
    e.perm_left = e.perm_right = 0;

    for (i=0; i<max_staves; i++)
	for (j=0; j<MAXPITCH*2+1; j++)
	    wrk.notes_used[i][j] = FALSE;

    for (i=0; i<wrk.mline_count; i++) {
	wrk.mlines[i]->just_done = FALSE;
	wrk.mlines[i]->ntuplet = 0;
	if (wrk.mline_ptr[i] && wrk.mline_ptr[i]->type==EV_NOTE &&
	    wrk.mlines[i]->time_pos <= wrk.time_pos) {

	    NoteStem *ns = &wrk.mline_ptr[i]->u.n;
	    NoteHead *h;
	    NoteFlags *f;

	    int stem_min = INT_MAX, stem_max = INT_MIN;
	    int stem_bx = xpos, stem_by = 0, stem = FALSE;

	    int stave = wrk.mlines[i]->stave_number;
	    int sy = stave_y (stave, STAVE_MIDDLE);
	    int cpitch = centre_pitch(staves[stave].clef) + MAXPITCH;
	    int rest_y = sy;

	    int nmin, nmax, smin, smax;

	    if (wrk.mlines[i]->stems_force)
		rest_y += (wrk.mlines[i]->stems_down ? -nh : nh);

	    e.temp_left = e.temp_right = 0;

	    wrk.mlines[i]->time_pos += ns->duration;

	    wrk.mlines[i]->max_y = stave_y(stave, STAVE_TOP);
	    wrk.mlines[i]->min_y = stave_y(stave, STAVE_BOTTOM);

	    for (j = -MAXPITCH; j <= MAXPITCH; j++)
		notepos[j] = FALSE, curr_acc[j] = NO_ACCIDENTAL;

	    for (h=ns->hhead; h; h=h->next) {
		/* do note head */
		if (h->pitch == PITCH_REST) {
		    if (ns->rest_type != REST_INVALID) {
			static const int rest_syms[] = {
			    s_restbreve, s_restminim, s_restminim,
			    s_restcrotchet, s_restquaver, s_restsemi,
			    s_restdemi, s_resthemi
			};

			if (ns->rest_type == REST_B) {
			    /* breve rest needs raising */
			    rest_y += ph;
			} else if (ns->rest_type == REST_SB) {
			    /* semibreve rest needs moving whatever happens */
			    if (staves[stave].clef != CLEF_LINE)
				rest_y += nh - sb_rest_ht/2;
			    else
				rest_y -= sb_rest_ht/2;
			} else if (ns->rest_type == REST_M) {
			    /* minim rest needs moving whatever happens */
			    rest_y += sb_rest_ht/2;
			}
			    
			put_symbol (xpos, axpos, rest_y,
				    rest_syms[ns->rest_type]);
			n_centre (&e, 0, rest_widths[ns->rest_type]);
		    } else
			error (wrk.mline_ptr[i]->line, wrk.mline_ptr[i]->col,
			       "unable to generate rest for ornament note");
		} else {
		    int hy = sy + ph * h->pitch;
		    int q, sbx, sby, head;
		    int flip = FALSE;
		    static const int note_heads[] = {
			s_breve, s_semibreve, s_headminim, s_headcrotchet,
			s_acciaccatura, s_appoggiatura, s_harmart
		    };

		    if (h->accidental != NO_ACCIDENTAL &&
			wrk.accidental[stave][h->pitch+cpitch]!=h->accidental)
			curr_acc[h->pitch] = h->accidental;

		    if (wrk.notes_used[stave][h->pitch+MAXPITCH])
			flip = TRUE;
		    else {
			wrk.notes_used[stave][h->pitch+MAXPITCH-1] = TRUE;
			wrk.notes_used[stave][h->pitch+MAXPITCH] = TRUE;
			wrk.notes_used[stave][h->pitch+MAXPITCH+1] = TRUE;
		    }
		    notepos[h->pitch] = TRUE;

		    if (flip) {
			q = note_widths[ns->heads_type];
			if (ns->stem_down)
			    q = -q;
		    } else
			q = 0;

		    if (ns->stem_type != STEM_NONE) {
			if ( (!flip) ^ (!ns->stem_down) ) {
			    sbx = xpos + q + sprout_left_x[ns->heads_type];
			    sby = hy + sprout_left_y[ns->heads_type];
			} else {
			    sbx = xpos + q + sprout_right_x[ns->heads_type];
			    sby = hy + sprout_right_y[ns->heads_type];
			}
			stem_bx = sbx;
			stem = TRUE;
		    } else
			sby = hy;

		    if (stem_max < sby) stem_max = sby;
		    if (stem_min > sby) stem_min = sby;

		    if (ns->stem_down)
			stem_by = stem_max;
		    else
			stem_by = stem_min;

		    if (h->flags & NH_ARTHARM)
			head = HEAD_ARTHARM;
		    else
			head = ns->heads_type;
		    put_symbol (q+xpos, axpos, hy, note_heads[head]);
		    n_centre (&e, q, note_widths[head]);
	
		    if (ns->ntuplet == NTUP_START) {
			wrk.mlines[i]->max_y = (stave_y(stave, STAVE_TOP) +
						ntuplet_clear);
			wrk.mlines[i]->min_y = (stave_y(stave, STAVE_BOTTOM) -
						ntuplet_clear);
		    }
		    if (wrk.mlines[i]->max_y < hy)
			wrk.mlines[i]->max_y = hy;
		    if (wrk.mlines[i]->min_y > hy)
			wrk.mlines[i]->min_y = hy;

		    /* leger lines */
		    if (h->pitch >= 6 || h->pitch <= -6) {
			int r;
			int sign = (h->pitch < 0 ? -1 : 1);

			n_centre (&e, q, leger_width);
			for (r=6*sign; sign*r <= sign*h->pitch; r+=2*sign)
			    put_line (q-(leger_width/2)+xpos, axpos, sy+ph*r,
				      q+(leger_width/2)+xpos, axpos, sy+ph*r,
				      thin_barline);
		    }
		}
	    }

	    /* draw dots, if the note is dotted */
	    if (ns->dots) {
		for (k=0; k<ns->dots; k++) {
		    n_right (&e, e.temp_right + (k ? dot_xoff : dot_ispace));
		    for (j = (-MAXPITCH) | 1; j <= MAXPITCH; j+=2)
			if (notepos[j] || (j>-MAXPITCH && notepos[j-1]))
			    put_symbol (e.temp_right + xpos, axpos,
					sy + ph*j - dot_halfht, s_staccato);
		}
		n_right (&e, e.temp_right + dot_ospace);
	    }

	    /* draw accidentals, if there are any */
	    {
		char ac_slot[7];
		int ac_x, ac_y, apos;
		static const int acc_sym[] = {
		    s_doublesharp, s_sharp, s_natural, s_flat, s_doubleflat
		};

		for (j=0; j<sizeof(ac_slot)/sizeof(*ac_slot); j++)
		    ac_slot[j] = 0;

		ac_x = xpos - e.temp_left - acc_space;
		ac_y = centre_pitch (staves[stave].clef);

		for (j = -MAXPITCH; j <= MAXPITCH; j++) {
		    if (curr_acc[j] != NO_ACCIDENTAL) {
			wrk.accidental[stave][j+ac_y+MAXPITCH] = curr_acc[j];
			for (apos = 0;
			     apos < sizeof(ac_slot)/sizeof(*ac_slot)-1;
			     apos++)
			    if (!ac_slot[apos])
				break;
			n_left (&e, e.temp_left + acc_width * (apos+1));
			put_symbol (ac_x - apos * acc_width, axpos,
				    sy + ph * j, acc_sym[curr_acc[j]]);
			ac_slot[apos] = sizeof(ac_slot)/sizeof(*ac_slot);
		    }
		    for (k=0; k<sizeof(ac_slot)/sizeof(*ac_slot); k++)
			if (ac_slot[k] > 0)
			    ac_slot[k]--;
		}
	    }	    

	    /* draw lyrics */
	    if (wrk.mlines[i]->lyrics) {
		put_text (xpos, axpos,
			  stave_y(stave, STAVE_BOTTOM) - lyrics_yoff,
			  wrk.mlines[i]->lyrics, ALIGN_CENTRE);
		wrk.mlines[i]->lyrics = NULL;
	    }

	    /* draw stem on note */
	    nmin = stem_min;
	    nmax = stem_max;
	    if (stem) {
		int stem_len, tails;
		
		tails = (ns->stem_type==STEM_1TAIL ? 1 :
			 ns->stem_type==STEM_2TAIL ? 2 :
			 ns->stem_type==STEM_3TAIL ? 3 :
			 ns->stem_type==STEM_4TAIL ? 4 : 0);

		if (wrk.mlines[i]->beam) {
		    stem_len = stem_max - stem_min;

		    nmin = stem_min, nmax = stem_max;

		    if (ns->stem_down) {
			stem_len = -stem_len;
			wrk.mlines[i]->beam->down = TRUE;
		    } else
			wrk.mlines[i]->beam->down = FALSE;

		    if (stem_len)
			put_line (stem_bx, axpos, stem_by,
				  stem_bx, axpos, stem_by + stem_len,
				  thin_barline);

		    add_to_beam (wrk.mlines[i]->beam,
				 stem_bx, axpos, stem_by + stem_len,
				 tails);
		} else {
		    if (staves[stave].clef == CLEF_LINE)
			stem_len = stem_line_len;
		    else
			stem_len = stem_norm_len;

		    stem_len += stem_max-stem_min;
		    stem_len += tails * stem_tail_extra;

		    if (ns->stem_down) {
			stem_len = -stem_len;
			nmin = stem_by + stem_len;
			nmax = stem_by;
		    } else {
			nmin = stem_by;
			nmax = stem_by + stem_len;
		    }

		    put_line (stem_bx, axpos, stem_by,
			      stem_bx, axpos, stem_by+stem_len, thin_barline);

		    if (tails)
			n_right (&e, tail_width + stem_bx - xpos);
		    while (tails--) {
			put_symbol (stem_bx, axpos, stem_by+stem_len,
				    (ns->stem_down ? s_tailquaverdn :
				     s_tailquaverup));
			stem_len += (ns->stem_down ? tail_vspace : -tail_vspace);
		    }
		}
	    }

	    /* draw note flags */
	    smin = nmin -= flag_ospace;
	    smax = nmax += flag_ospace;
	    for (f=ns->fhead; f; f=f->next) {
		int down = ns->stem_down;
		Beam *beam = wrk.mlines[i]->beam;
		int y;

		static const int flags[] = {
		    s_staccato, s_legato, s_INVALID, s_sforzando,
		    s_bowdown, s_bowup, s_stopping, s_fermata, s_harmnat,
		    s_accent, s_trill, s_turn, s_INVALID, s_mordentupper,
		    s_mordentlower, s_smallsharp, s_smallnatural, s_smallflat,
		    s_INVALID, s_INVALID, s_INVALID
		};

		switch (f->type) {	
		  case NF_STACCATO: case NF_LEGATO:
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 flags[f->type], flag_ht[f->type], down, FALSE,
			 xpos, axpos);
		    break;
		  case NF_STACCATISSIMO:
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 down ? s_staccatissdn : s_staccatissup,
			 flag_ht[f->type], down, FALSE, xpos, axpos);
		    break;
		  case NF_SFORZANDO: case NF_DOWNBOW: case NF_UPBOW:
		  case NF_STOPPING: case NF_FERMATA: case NF_NATHARM:
		  case NF_TRILL: case NF_TURN: case NF_MORDENT:
		  case NF_INVMORDENT: case NF_SMALLSHARP:
		  case NF_SMALLNATURAL: case NF_SMALLFLAT:
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 flags[f->type], flag_ht[f->type], TRUE, TRUE,
			 xpos, axpos);
		    break;
		  case NF_ACCENT:
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 flags[f->type], flag_ht[f->type], FALSE, TRUE,
			 xpos, axpos);
		    break;
		  case NF_INVTURN:
		    /* Special case */
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 s_INVALID, flag_ht[NF_TURN], TRUE, TRUE,
			 xpos, axpos);
		    break;
		  case NF_FINGERING:
		    flag(stave, beam, &nmin, &nmax, &smin, &smax,
			 smalldigit(f->extra), smalldigit_ht, TRUE, TRUE,
			 xpos, axpos);
		    break;
		  case NF_TREMOLO:
		    if (staves[stave].clef == CLEF_LINE)
			k = stem_line_len;
		    else
			k = stem_norm_len;
		    k = (k/2) + stem_max - stem_min;
		    if (ns->stem_down)
			k = -k;
		    k -= (trem_space/2)*(f->extra-1);
		    for (j=0; j<f->extra; j++)
			put_line (stem_bx - trem_xspread, axpos,
				  stem_by + k + trem_space*j - trem_yspread,
				  stem_bx + trem_xspread, axpos,
				  stem_by + k + trem_space*j + trem_yspread,
				  trem_thick);
		    break;
		  case NF_ARPEGGIO:
		    k = (stem_max - stem_min + arp_height-1) / arp_height;
		    y = (stem_max+stem_min)/2 - (k-1)*(arp_height/2);
		    for (j=0; j<k; j++)
			put_symbol (xpos - e.temp_left - arp_space, axpos,
				    y + j*arp_height, s_arpeggio);
		    n_left (&e, e.temp_left + arp_width);
		    break;
		}
	    }

	    /*
	     * Do ties, which we've left until now to make sure
	     * e.temp_left is finalised. Otherwise you get
	     * entertaining behaviour when tying together two
	     * chords with adjacent notes: the righthand X position
	     * of the tie suddenly changes halfway up the chord as
	     * we suddenly realise there are things to the left of
	     * the stem.
	     */
	    for (h=ns->hhead; h; h=h->next) {
		if (h->pitch != PITCH_REST) {
		    int hy = sy + ph * h->pitch;
		    if (wrk.ties[stave][h->pitch+MAXPITCH].exists) {
			int p = h->pitch+MAXPITCH;
			put_tie (-wrk.ties[stave][p].x - tie_space,
				 wrk.ties[stave][p].ax,
				 xpos-e.temp_left-tie_space, axpos, hy,
				 wrk.ties[stave][p].down);
			wrk.ties[stave][p].exists = FALSE;
		    }
		    if (h->flags & NH_TIE) {
			int p = h->pitch+MAXPITCH;
			/* we can't fill in x yet - it'll get done below */
			wrk.ties[stave][p].ax = axpos;
			wrk.ties[stave][p].down = ns->stem_down;
			wrk.ties[stave][p].exists = TRUE;
		    }
		}
	    }

	    /* record the extent of the note for the slurs */
	    if (wrk.mlines[i]->slur)
		record_slur (wrk.mlines[i]->slur, wrk.mlines[i]->beam,
			     xpos, axpos, e.temp_left, e.temp_right,
			     smin, smax, ns->stem_down, stem, TRUE);

	    wrk.mlines[i]->just_done = TRUE;
	    wrk.mlines[i]->last_stem_down = ns->stem_down;
	    wrk.mlines[i]->ntuplet = ns->ntuplet;

	    wrk.mline_ptr[i] = wrk.mline_ptr[i]->next;
	} else if (wrk.mline_ptr[i] && wrk.mline_ptr[i]->type==EV_STREAM &&
		   wrk.mline_ptr[i]->u.s.type == SC_ZERONOTE &&
		   wrk.mlines[i]->time_pos <= wrk.time_pos) {
	    wrk.mlines[i]->just_done = TRUE;

	    wrk.mline_ptr[i] = wrk.mline_ptr[i]->next;
	}
    }

    shift_mark (e.perm_left);
    beam_shift_mark (e.perm_left);
    slur_shift_mark (e.perm_left);

    xpos += e.perm_left + e.perm_right;

    /* fix up ties */
    for (i=0; i<max_staves; i++)
	for (j=0; j<=2*MAXPITCH; j++)
	    if (wrk.ties[i][j].exists && wrk.ties[i][j].ax == axpos)
		wrk.ties[i][j].x = xpos;

    /* fix up ntuplets */
    for (i=0; i<wrk.mline_count; i++)
	if (wrk.mlines[i]->just_done) {
	    if (wrk.mlines[i]->ntuplet == NTUP_START) {
		wrk.mlines[i]->ntup_x = xpos - e.perm_right;
		wrk.mlines[i]->ntup_ax = axpos;
	    } else if (wrk.mlines[i]->ntuplet) {
		char str[40];	       /* big enough for a *big* number */
		int xx, ax, yy, wid;
		char *p;

		if (wrk.mlines[i]->last_stem_down)
		    yy = wrk.mlines[i]->max_y + ntuplet_space;
		else
		    yy = wrk.mlines[i]->min_y - ntuplet_space - smalldigit_ht;

		sprintf(str, "%d", wrk.mlines[i]->ntuplet);

		xx = (wrk.mlines[i]->ntup_x + xpos - e.perm_right) / 2;
		wid = smalldigit_wid * strlen(str);
		ax = (wrk.mlines[i]->ntup_ax + axpos) / 2;

		if (wrk.mlines[i]->slur)
		    record_slur (wrk.mlines[i]->slur, NULL, xx, ax, wid/2,
				 wid/2, yy-ntuplet_vspace,
				 yy+smalldigit_ht+ntuplet_vspace,
				 FALSE, FALSE, FALSE);

		xx -= (wid-smalldigit_wid)/2;
		for (p=str; *p; p++) {
		    put_symbol (xx, ax, yy, smalldigit(*p-'0'));
		    xx += smalldigit_wid;
		}
	    }
	}

    /* advance time pos */
    for (i=0; i<wrk.mline_count; i++)
	if ( (wrk.mline_ptr[i] || wrk.mlines[i]->just_done)
	    && new_time_pos > wrk.mlines[i]->time_pos)
	    new_time_pos = wrk.mlines[i]->time_pos;
    if (new_time_pos < INT_MAX) {
	axpos += (new_time_pos-wrk.time_pos) * 100 / wrk.factor;
	wrk.time_pos = new_time_pos;
    }
}

static void n_centre (note_extent *e, int xpos, int width) {
    n_left (e, width/2-xpos);
    n_right (e, xpos+width/2);
}

static void n_left (note_extent *e, int width) {
    if (e->perm_left < width)
	e->perm_left = width;
    if (e->temp_left < width)
	e->temp_left = width;
}

static void n_right (note_extent *e, int width) {
    if (e->perm_right < width)
	e->perm_right = width;
    if (e->temp_right < width)
	e->temp_right = width;
}

/* Mark a flag on a note. Special case: if 'symbol' is s_INVALID, then
 * we must do an inverted turn, defined as a normal turn with extra space
 * "turn_inv_space" above and below it, and a vertical line extending from
 * the top to the bottom of that extra space through the centre of the
 * turn, of thickness "turn_inv_thick".
 * 
 * If "beam" is non-NULL, this routine merely sets a deferred-flag marker. */
void flag (int stave, Beam *beam, int *nmin, int *nmax, int *smin, int *smax,
	   int symbol, int height, int above, int outside, int xpos,
	   int axpos) {
    int my;

    if (beam) {
	deferred_flag (beam, symbol, height, above, outside, xpos, axpos);
	return;
    }
    if (symbol == s_INVALID)
	height += 2*turn_inv_space;
    if (above) {
	my = *nmax + flag_ispace;
	if (outside && my < stave_y (stave, STAVE_TOP)+flag_ospace)
	    my = stave_y (stave, STAVE_TOP)+flag_ospace;
	*nmax = my + height;
	if (!outside)
	    *smax = *nmax;
    } else {
	my = *nmin - flag_ispace;
	if (outside && my > stave_y (stave, STAVE_BOTTOM)+flag_ospace)
	    my = stave_y (stave, STAVE_BOTTOM)+flag_ospace;
	*nmin = my -= height;
	if (!outside)
	    *smin = *nmin;
    }
    if (symbol == s_INVALID) {
	symbol = s_turn;
	put_line (xpos, axpos, my, xpos, axpos, my+height, turn_inv_thick);
	my += turn_inv_space;
    }
    put_symbol (xpos, axpos, my, symbol);
}
