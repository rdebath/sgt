/* melody.c  read and parse melody lines */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mus.h"
#include "header.h"
#include "input.h"
#include "misc.h"
#include "melody.h"
#include "pscript.h"

/* Internal prototypes */
static void read_stream_control (MelodyLine *);
static void read_note (MelodyLine *);
static int read_duration (NoteStem *);
static int read_pitches (NoteStem *);
static int read_pitch (NoteStem *);
static int read_flags (NoteStem *);
static void add_event (MelodyLine *, MelodyEvent *);

static int nd_gc(void), gc(void);      /* called like [nd_]getchr */

/* Read a melody line from the input file. Uses the getchr and nd_getchr
 * routines from input.c. Assumes it is called just after the opening "{"
 * has been read, and returns just after having read the closing "}". */
MelodyLine *readML(void) {
    MelodyLine *ml = mmalloc (sizeof(MelodyLine));

    /* Initialise the structure. */
    ml->stems_force = 0;
    ml->head = ml->tail = NULL;
    ml->beam = NULL;
    ml->slur = NULL;
    ml->lyrics = NULL;

    /* Begin to read. */
    while (1) {
	switch (nd_gc()) {
	  case '[': case ']': case '/': case '\\': case '<': case '"':
	  case '(': case ')': case 'Z':
	    /* It's a stream control. */
	    read_stream_control (ml);
	    continue;
	  case '}':
	    /* End of melody line */
	    gc();
	    break;
	  default:
	    /* It's a note */
	    read_note (ml);
	    continue;
	}
	break;
    }

    return ml;
}

static void read_stream_control (MelodyLine *ml) {
    MelodyEvent *me = mmalloc (sizeof(MelodyEvent));
    StreamControl *sc = &me->u.s;      /* for ease of access */
    int c;
    char *p, *str = NULL;

    me->type = EV_STREAM;
    me->line = line_no;
    me->col = col_no;
    sc->text = NULL;

    switch (c = nd_gc()) {
      case 'Z':	
	gc();
	sc->type = SC_ZERONOTE;
	break;
      case '[':
	gc();
	sc->type = SC_BEAMSTART;
	break;
      case ']':
	gc();
	sc->type = SC_BEAMSTOP;
	if (nd_gc() == '-') {
	    gc();
	    if (nd_gc() != '[') {
		error (me->line, me->col, "syntax error in melody line:"
		       " expected `[' after `]-'");
		return;
	    }
	    sc->type = SC_BEAMBREAK;
	    gc();
	}
	break;
      case '/':
	gc();
	sc->type = SC_SLURSTART;
	break;
      case '\\':
	gc();
	sc->type = SC_SLURSTOP;
	break;
      case '(':
	gc();
	sc->type = SC_NTUPLET;
	sc->ntuplet_denom = 0;
	while ( (c=gc()) >= '0' && c <= '9')
	    sc->ntuplet_denom = (10*sc->ntuplet_denom) + c-'0';
	if (c != '/') {
	    error (me->line, me->col,
		   "expected `/' after numerator of n-tuplet");
	    return;
	}
	sc->ntuplet_num = 0;
	while ( (c=gc()) >= '0' && c <= '9')
	    sc->ntuplet_num = (10*sc->ntuplet_num) + c-'0';
	if (c != ':') {
	    error (me->line, me->col,
		   "expected `:' after denominator of n-tuplet");
	    return;
	}
	wrk.factor = lcm (wrk.factor, sc->ntuplet_denom);
	break;
      case ')':
	gc();
	sc->type = SC_NTUPLET_END;
	break;
      case '<':
	gc();
	/* Could be <p>, <d>, <8va>, <8vb>, <,>, </>, <~> */
	switch (c = gc()) {
	  case 'P':
	    sc->type = SC_FORCE_DOWN;
	    break;
	  case 'D':
	    sc->type = SC_FORCE_UP;
	    break;
	  case '8':
	    if (gc() != 'V') {
		error (me->line, me->col, "syntax error in melody line:"
		       " expected `v' after `<8'");
		return;
	    }
	    c = gc();
	    if (c == 'A')
		sc->type = SC_8VA;
	    else if (c == 'B')
		sc->type = SC_8VB;
	    else {
		error (me->line, me->col, "syntax error in melody line:"
		       " expected `a' or `b' after `<8v'");
		return;
	    }
	    break;
	  case ',':
	    sc->type = SC_BREATH;
	    break;
	  case '/':
	    sc->type = SC_GENPAUSE;
	    break;
	  case 'U':
	    if (nd_gc() == '!') {
		sc->type = SC_INVTURN;
		gc();
	    } else
		sc->type = SC_TURN;
	    break;
	  default:
	    error (me->line, me->col, "syntax error in melody line:"
		   " expected `p', `d', `8', `,', `/' or `u' after `<'");
	    return;
	}
	if (gc() != '>') {
	    error (me->line, me->col, "syntax error in melody line:"
		   " expected `>' to end `<...>' control");
	    return;
	}
	break;
      case '"':
	/* It's some kind of quoted string; get it into memory first. */
	getword (&str);
	/* Check if the string is actually empty. */
	if (!str[0] || !str[1] || str[1] == '"') {
	    error (me->line, me->col, "quoted string is empty");
	    return;
	}
	/* Check the second character, because it's general. */
	if (str[1] != '<' && str[1] != '>' && str[2] != ' ') {
	    error (me->line, me->col,
		   "expected space in quoted string after `%c'", str[1]);
	    return;
	}
	/* Remove the trailing quote. */
	str[strlen(str)-1] = '\0';
	/* Could be dynamics, lyrics, above or below text, cresc or dim. */
	switch (str[1]) {
	  case '<': case '>':
	    /* cresc or dim */
	    if (str[2]) {
		error (me->line, me->col,
		       "unexpected text after `%c' in quoted string", str[0]);
		return;
	    }
	    sc->type = (str[1] == '<' ? SC_CRESC : SC_DIM);
	    break;
	  case 'D': case 'd':
	    /* dynamics */
	    p = str+3;
	    while (*p && !isspace(*p)) {
		static const char *const dynamic_str = "fmpsz";
		char *q = strchr(dynamic_str, *p);
		if (!q) {
		    error (me->line, me->col,
			   "character `%c' not a valid dynamic", *p);
		    return;
		}
		*p++ = q - dynamic_str + '\xD1';
	    }
	    sc->type = SC_DYNAMIC;
	    sc->text = dupstr (str+3);
	    break;
	  case 'L': case 'l': case 'A': case 'a': case 'B': case 'b':
	    sc->text = dupstr (str+3);
	    sc->type = (str[1]=='L' || str[1]=='l' ? SC_LYRICS :
			str[1]=='A' || str[1]=='a' ? SC_TEXT_ABOVE :
			str[1]=='B' || str[1]=='b' ? SC_TEXT_BELOW : -1);
	    break;
	}
	break;
      default:			       /* should never happen */
	gc();
	fatal(me->line, me->col, "internal fault: reached `default'"
	      " clause in `read_stream_control': char %d", c);
	break;			       /* *certainly* shouldn't reach this! */
    }
    add_event (ml, me);
    mfree (str);
}

static void read_note (MelodyLine *ml) {
    MelodyEvent *me = mmalloc (sizeof(MelodyEvent));
    NoteStem *ns = &me->u.n;	       /* for ease of access */
    int i;

    me->type = EV_NOTE;
    me->line = line_no;
    me->col = col_no;
    ns->hhead = ns->htail = NULL;
    ns->fhead = ns->ftail = NULL;

    if (!(i=read_duration (ns)))
	return;
    if (i!=2 && !read_pitches (ns))
	return;
    if (!read_flags (ns))
	return;

    add_event (ml, me);
}

/* This returns 0 on failure, 1 on success, 2 on reading a special "bar rest"
 * (which is not followed by any pitches). */
static int read_duration (NoteStem *ns) {
    int c, i, denom, special = FALSE, ret = 1;
    int sline = line_no, scol = col_no;

    switch (c=gc()) {
      case 'M':
	/* must be a minim */
	ns->duration = 32;
	break;
      case 'C':
	/* must be a crotchet */
	ns->duration = 16;
	break;
      case 'Q':
	/* must be a quaver */
	ns->duration = 8;
	break;
      case 'B':
	/* breve, or "black semibreve" */
	if (nd_gc() == 'S') {
	    gc();
	    ns->duration = 64;
	    ns->heads_type = HEAD_CROTCHET;
	    ns->stem_type = STEM_NONE;
	    ns->rest_type = REST_INVALID;
	    special = TRUE;
	} else
	    ns->duration = 128;
	break;
      case 'S':
	/* semibreve or semiquaver */
	if ( (c=gc()) != 'B' && c != 'Q') {
	    error (sline, scol, "unrecognised duration `S%c'", c);
	    return 0;
	}
	ns->duration = (c=='B' ? 64 : 4);
	break;
      case 'D':
	/* next character must be 'Q' because it is a demisemi */
	if (gc() != 'Q') {
	    error (sline, scol, "unrecognised duration `D%c'", c);
	    return 0;
	}
	ns->duration = 2;
	break;
      case 'H':
	/* next character must be 'Q' because it is a hemidemisemi */
	if (gc() != 'Q') {
	    error (sline, scol, "unrecognised duration `H%c'", c);
	    return 0;
	}
	ns->duration = 1;
	break;
      case 'A':
	/* next character should be 'P' or 'C': appoggiatura, acciaccatura */
	if ( (c=gc()) != 'P' && c != 'C') {
	    error (sline, scol, "unrecognised duration `A%c'", c);
	    return 0;
	}
	ns->duration = 0;
	ns->heads_type = (c=='P' ? HEAD_AP : HEAD_AC);
	ns->stem_type = STEM_NONE;
	ns->rest_type = REST_INVALID;
	special = TRUE;
	break;
      case 'R':
	/* special: exactly one bar rest, and looks like a semibreve rest */
	ns->duration = bar_len (opts.timesig);
	ns->rest_type = REST_SB;
	ns->hhead = ns->htail = mmalloc(sizeof(NoteHead));
	ns->hhead->next = ns->hhead->prev = NULL;
	ns->hhead->pitch = PITCH_REST;
	ns->hhead->flags = 0;
	ret = 2;
	special = TRUE;
	break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	i = c-'0';
	while ( (c=nd_gc()) >= '0' && c <= '9') {
	    i = i*10 + c-'0';
	    gc();
	}
	if (i!=0 && i!=1 && i!=2 && i!=4 && i!=8 && i!=16 && i!=32 && i!=64) {
	    error (sline, scol, "unrecognised duration %d", i);
	    return 0;
	} else {
	    if (!i)
		i = 1;
	    else
		i *= 2;
	    ns->duration = 128 / i;
	}
      default:
	error (sline, scol, "unrecognised duration `%c'", c);
	return 0;
    }

    /* fill in the stem, head and rest types */
    if (!special) {
	ns->heads_type = (ns->duration == 128 ? HEAD_BREVE :
			  ns->duration == 64 ? HEAD_SEMIBREVE :
			  ns->duration >= 32 ? HEAD_MINIM : HEAD_CROTCHET);
	ns->stem_type = (ns->duration >= 64 ? STEM_NONE :
			 ns->duration >= 16 ? STEM_PLAIN :
			 ns->duration == 8 ? STEM_1TAIL :
			 ns->duration == 4 ? STEM_2TAIL :
			 ns->duration == 2 ? STEM_3TAIL :
			 ns->duration == 1 ? STEM_4TAIL : -1);
	ns->rest_type = (ns->duration == 128 ? REST_B :
			 ns->duration == 64 ? REST_SB :
			 ns->duration == 32 ? REST_M :
			 ns->duration == 16 ? REST_C :
			 ns->duration == 8 ? REST_Q :
			 ns->duration == 4 ? REST_SQ :
			 ns->duration == 2 ? REST_DQ :
			 ns->duration == 1 ? REST_HQ : REST_INVALID);
    }

    /* check for dots on the note */
    denom = 1;
    ns->dots = 0;
    while (nd_gc() == '.') {
	gc();
	denom <<= 1;
	ns->dots++;
    }
    if (ns->duration % denom) {
	error (sline, scol,
	       "duration resolution too low: too many dots");
	return 0;
    }
    ns->duration = (denom*2-1) * ns->duration / denom;

    return ret;
}

static int read_pitches (NoteStem *ns) {
    if (nd_gc() == '(') {
	gc();
	while (nd_gc() != ')')
	    if (!read_pitch (ns))
		return 0;
	gc();
	return 1;
    } else
	return read_pitch (ns);
}

static int read_pitch (NoteStem *ns) {
    NoteHead *nh = mmalloc (sizeof(NoteHead));
    int c, note_letter, octave;
    int hline, hcol;		       /* for errors */

    nh->flags = 0;
    hline = line_no;
    hcol = col_no;

    /* note letter */
    c = gc();
    if (c=='N') {
	nh->pitch = PITCH_NONE;
	nh->accidental = NO_ACCIDENTAL;
        goto done;		       /* I know, I know... I reckon it's
					* excusable just this once, OK? */
    } else if (c=='R') {
	nh->pitch = PITCH_REST;
	goto done;		       /* all right, and this once too */
    } else if (c=='H' && !opts.german_notes) {
	error (hline, hcol, "`H' note not allowed unless"
	       " `note-names German' given");
	return 0;
    } else if (c<'A' || c>'G') {
	error (hline, hcol, "invalid pitch `%c'", c);
	return 0;
    }
    note_letter = c - 'A';

    /* accidental, if any */
    switch (c = nd_gc()) {
      case 'X':
	nh->accidental = DOUBLE_SHARP;
	gc();
	break;
      case '#':
	nh->accidental = SHARP;
	gc();
	break;
      case 'N':
	nh->accidental = NATURAL;
	gc();
	break;
      case 'B':
	nh->accidental = FLAT;
	gc();
	if (nd_gc() == 'B') {
	    nh->accidental = DOUBLE_FLAT;
	    gc();
	}
	break;
      default:
	if (opts.notes_as_key)
	    nh->accidental = NO_ACCIDENTAL;
	else
	    nh->accidental = NATURAL;
	break;
    }

    /* do German notes: here we assume that accidentals are scalar, i.e.
     * we can add (FLAT-NATURAL) to anything to flatten it by a semitone */
    if (note_letter == 'H' - 'A') {
	note_letter = 'B' - 'A';
    } else if (opts.german_notes && note_letter == 'B' - 'A') {
	if (nh->accidental == DOUBLE_FLAT)
	    error (hline, hcol,
		   "cannot process B double flat in German notation");
	else
	    nh->accidental += (FLAT - NATURAL);
    }

    /* octave number */
    if ( (c=gc()) == '0')
	octave = 0;
    else if (c != '+' && c != '-') {
	error (hline, hcol, "invalid octave number `%c'", c);
	return 0;
    } else {
	int cc;
	if ( (cc=gc()) <= '0' || cc > '9') {
	    error (hline, hcol, "invalid octave number `%c%c'", c, cc);
	    return 0;
	}
	octave = (c=='+' ? cc-'0' : '0'-cc);
    }

    /* note-head flags: * for artificial harmonic, - for tie */
    while ( (c=nd_gc()) == '*' || c == '-') {
	if (c=='*')
	    nh->flags |= NH_ARTHARM;
	else if (c=='-')
	    nh->flags |= NH_TIE;
	gc();
    }

    /* now build the pitch out of note_letter and octave */
    if (octave==0 && note_letter != 'C'-'A') {
	error(hline, hcol, "only C allowed in octave zero");
	return 0;
    } else if (octave == 0) {
	nh->pitch = 0;
    } else if (octave < 0) {
	if (note_letter < 'C'-'A')
	    note_letter += 7;
	nh->pitch = 7 * octave + note_letter - 2;
    } else if (octave > 0) {
	if (note_letter <= 'C'-'A')
	    note_letter += 7;
	nh->pitch = 7 * octave + note_letter - 9;
    }

    done:
    /* stick the note head on the linked list */
    nh->next = NULL;
    nh->prev = ns->htail;
    if (ns->htail)
	ns->htail->next = nh;
    else
	ns->hhead = nh;
    ns->htail = nh;

    return 1;
}

static int read_flags (NoteStem *ns) {
    NoteFlags *nf = NULL;
    int c;

    while (1) {
	int fline = line_no, fcol = col_no;

	if (!nf)
	    nf = mmalloc (sizeof(NoteFlags));

	/* in here, we do "return 1;" if we are looking at a non-flag, and
	 * "return 0;" if we get an error in a flag. "break;", of course,
	 * if we parse a valid flag. */
	switch (c=nd_gc()) {
	  case '.':  nf->type = NF_STACCATO;	  gc(); break;
	  case '_':  nf->type = NF_LEGATO;	  gc(); break;
	  case '\'': nf->type = NF_STACCATISSIMO; gc(); break;
	  case '^':  nf->type = NF_SFORZANDO;	  gc(); break;
	  case 'W':  nf->type = NF_DOWNBOW;	  gc(); break;
	  case 'V':  nf->type = NF_UPBOW;	  gc(); break;
	  case '+':  nf->type = NF_STOPPING;	  gc(); break;
	  case 'P':  nf->type = NF_FERMATA;	  gc(); break;
	  case '>':  nf->type = NF_ACCENT;	  gc(); break;
	  case 'O':  nf->type = NF_NATHARM;	  gc(); break;
	  case 'T':  nf->type = NF_TRILL;	  gc(); break;
	  case 'G':  nf->type = NF_ARPEGGIO;	  gc(); break;
	  case 'U': case '~':
	    nf->type = (c=='U' ? NF_TURN : NF_MORDENT);
	    gc();
	    if (nd_gc() == '!') {
		gc();
		nf->type = (c=='U' ? NF_INVTURN : NF_INVMORDENT);
	    }
	    break;
	  case '=':
	    gc();
	    if ( (c=gc()) < '1' || c > '4') {
		error (fline, fcol, "expected digit in range 1-4 after `='");
		return 0;
	    }
	    nf->type = NF_TREMOLO;
	    nf->extra = c-'0';
	    break;
	  case '`':
	    gc();
	    c = gc();
	    if (c == '#')
		nf->type = NF_SMALLSHARP;
	    else if (c=='N')
		nf->type = NF_SMALLNATURAL;
	    else if (c=='B')
		nf->type = NF_SMALLFLAT;
	    else {
		error (fline, fcol, "expected `#', `n' or `b' after ``'");
		return 0;
	    }
	    break;
	  case 'I':
	    gc();
	    c = gc();
	    if (c<'0' || c>'5') {
		error (fline, fcol, "expected digit in range 0-5 after `i'");
		return 0;
	    }
	    nf->type = NF_FINGERING;
	    nf->extra = c-'0';
	    break;
	  default:
	    return 1;
	}

	/* stick the flag on the linked list */
	nf->next = NULL;
	nf->prev = ns->ftail;
	if (ns->ftail)
	    ns->ftail->next = nf;
	else
	    ns->fhead = nf;
	ns->ftail = nf;
	nf = NULL;
    }
}

static void add_event (MelodyLine *ml, MelodyEvent *me) {
    me->next = NULL;
    me->prev = ml->tail;
    if (ml->tail)
	ml->tail->next = me;
    else
	ml->head = me;
    ml->tail = me;
}

static int nd_gc (void) {
    char c;

    while (c = nd_getchr(), isspace(c))
	getchr();
    return nd_getchr();
}

static int gc (void) {
    char c;

    while (c = nd_getchr(), isspace(c))
	getchr();
    return getchr();
}
