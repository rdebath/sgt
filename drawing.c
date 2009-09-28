#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mus.h"
#include "misc.h"
#include "pscript.h"
#include "header.h"
#include "input.h"
#include "melody.h"
#include "preproc.h"
#include "drawing.h"
#include "notes.h"
#include "measure.h"
#include "slurs.h"

struct working wrk;		       /* global - defined in mus.h */

int xpos, ypos, axpos;		       /* where we are on page, so far */
int y;				       /* top of current score line */

/* Horizontal coordinates of left-hand end of current score line: used
 * for drawing the actual lines comprising the staves, at the end of
 * processing the line */
static int bx, bax;

/* Horizontal coordinates of a break in the stave lines on the current
 * score line, if there is one. There *should* only be one per piece
 * (to introduce a coda section) and so we assume there will be no more
 * than one per line. Note: break_ax == -1 means no break on this line. */
static int break_from_x, break_to_x, break_ax;

/* Bar numbering. Not used internally - only for *printing* bar numbers,
 * and reporting errors. */
static int bar_num = 1, prev_bar_num;

/* Whether a time signature has been assumed recently (i.e. on the last
 * line) and must therefore be re-drawn at the start of the next line. */
static int new_time_sig = TRUE;

/* Is this the beginning of the piece? Because if it is we must print the
 * stave names. */
static int first = TRUE;

/* The positions, on each clef, for the successive sharps and flats
 * in each key signature. Measured in multiples of ph. */
static int sharppos[7][7] = {
    {+4, +1, +5, +2, -1, +3,  0},      /* treble clef */
    {-1, +3,  0, +4, +1, -2, +2},      /* soprano clef */
    {+1, -2, +2, -1, +3,  0, +4},      /* mezzo-soprano clef */
    {+3,  0, +4, +1, -2, +2, -1},      /* alto clef */
    {-2, +2, -1, +3,  0, +4, +1},      /* tenor clef */
    { 0, +4, +1, -2, +2, -1, +3},      /* baritone clef */
    {+2, -1, +3,  0, -3, +1, -2}       /* bass clef */
};
static int flatpos[7][7] = {
    { 0, +3, -1, +2, -2, +1, -3},      /* treble clef */
    {+2, -2, +1, -3,  0, -4, -1},      /* soprano clef */
    {-3,  0, -4, -1, +2, -2, +1},      /* mezzo-soprano clef */
    {-1, +2, -2, +1, -3,  0, -4},      /* alto clef */
    {+1, -3,  0, -4, -1, +2, -2},      /* tenor clef */
    {-4, -1, +2, -2, +1, -3,  0},      /* baritone clef */
    {-2, +1, -3,  0, -4, -1, -5}       /* bass clef */
};

/* Initial accidental values for each key signature. First index is the
 * key signature (0 to 14 as returned by parse_key_sig()) and second is
 * the note (C=0, D=1, ..., B=6). */
static int initial_acc[15][7] = {
/*       C       D       E       F       G       A       B       */
    {   FLAT,   FLAT,   FLAT,   FLAT,   FLAT,   FLAT,   FLAT},  /* Cb major */
    {   FLAT,   FLAT,   FLAT,NATURAL,   FLAT,   FLAT,   FLAT},  /* Gb major */
    {NATURAL,   FLAT,   FLAT,NATURAL,   FLAT,   FLAT,   FLAT},  /* Db major */
    {NATURAL,   FLAT,   FLAT,NATURAL,NATURAL,   FLAT,   FLAT},  /* Ab major */
    {NATURAL,NATURAL,   FLAT,NATURAL,NATURAL,   FLAT,   FLAT},  /* Eb major */
    {NATURAL,NATURAL,   FLAT,NATURAL,NATURAL,NATURAL,   FLAT},  /* Bb major */
    {NATURAL,NATURAL,NATURAL,NATURAL,NATURAL,NATURAL,   FLAT},  /* F  major */
    {NATURAL,NATURAL,NATURAL,NATURAL,NATURAL,NATURAL,NATURAL},  /* C  major */
    {NATURAL,NATURAL,NATURAL,  SHARP,NATURAL,NATURAL,NATURAL},  /* G  major */
    {  SHARP,NATURAL,NATURAL,  SHARP,NATURAL,NATURAL,NATURAL},  /* D  major */
    {  SHARP,NATURAL,NATURAL,  SHARP,  SHARP,NATURAL,NATURAL},  /* A  major */
    {  SHARP,  SHARP,NATURAL,  SHARP,  SHARP,NATURAL,NATURAL},  /* E  major */
    {  SHARP,  SHARP,NATURAL,  SHARP,  SHARP,  SHARP,NATURAL},  /* B  major */
    {  SHARP,  SHARP,  SHARP,  SHARP,  SHARP,  SHARP,NATURAL},  /* F# major */
    {  SHARP,  SHARP,  SHARP,  SHARP,  SHARP,  SHARP,  SHARP},  /* C# major */
};

static void read_melody(void);
static void free_melody(void);
static void free_ml (MelodyLine *ml);

static void draw_clefs (void);
static void draw_keysig (int oldk, int newk);
static void draw_timesig(void);

#define BARLINE_THIN	'|'
#define BARLINE_THICK	'I'
#define REPEAT_MARKER	':'
static void bar_line (int type, int repeating) {
    int i, j;

    if (type == REPEAT_MARKER) {
	for (i=0; i<max_staves; i++)
	    put_symbol (xpos, axpos, stave_y (i, STAVE_MIDDLE),
			s_repeatmarks);
	xpos += repeat_width;
    } else {
	int xx = (type == BARLINE_THIN ? thin_barline/2 : thick_barline/2);
	int top, bottom;

	i = 0;

	while (i < max_staves) {
	    j = i;
	    while (1) {
		int split;
		switch (opts.barline_split) {
		  case SPL_NEVER: split = FALSE; break;
		  case SPL_BRACKET: split = (staves[i].bkt1 == 0); break;
		  case SPL_NONBRACE: split = (staves[i].bkt1 != -1); break;
		  case SPL_ALWAYS: split = TRUE; break;
		  default: split = TRUE; break;   /* pacify gcc */
		}
		if (split || j>=max_staves-1)
		    break;
		j++;
	    }

	    top = stave_y (i, STAVE_TOP);
	    bottom = stave_y (j, STAVE_BOTTOM);

	    /* on single-line stave, must go past the line sometimes */
	    if (staves[i].clef == CLEF_LINE && (repeating || i==j))
		top += line_bar_len/2;
	    if (staves[j].clef == CLEF_LINE && (repeating || i==j))
		bottom -= line_bar_len/2;

	    /* shorten the thicker bar lines because we are using linecap 2 */
	    top -= xx*2-thin_barline;
	    bottom += xx*2-thin_barline;

	    put_line (xpos+xx, axpos, top, xpos+xx, axpos, bottom, xx*2);
	    i = j+1;
	}
	xpos += xx*2;
    }
}

void do_bar_line (int type, int eol) {
    static const char *const types[2][6] = {
        {"|", "||", "|I", ":|I", "I|:", ":II:"},
	{"|", "||", "|I", ":|I", "||", ":|I"},
    };
    const char *p;

    p = types[eol ? 1 : 0][type];
    while (*p) {
	bar_line (*p++, (type >= BL_REP_END));
	if (*p)
	    xpos += barline_ispace;
    }
}

void draw_staves (void) {
    int i, y;

    for (i=0; i<max_staves; i++) {
	for (y=stave_y(i, STAVE_BOTTOM); y<=stave_y(i, STAVE_TOP); y+=nh) {
	    if (break_ax != -1) {
		put_line (bx, bax, y, break_from_x, break_ax, y,
			  thin_barline);
		put_line (break_to_x, break_ax, y,
			  xpos-thin_barline/2, axpos, y, thin_barline);
	    } else
		put_line (bx, bax, y, xpos-thin_barline/2, axpos, y,
			  thin_barline);
	}
    }
}

/* Draw all the stuff at the start of a line. Includes a "carry-over" bar
 * line from the previous line, which must be passed in as a parameter. */
void begin_line(int left_indent, int barline_type) {
    int i, j;			       /* general purpose */
    
    /* set up our starting point coordinates */
    axpos = 0, xpos = left_indent + lmarg;
    y = ypos - (scorespacing*opts.ysquash)/2, ypos -= lineheight;
    /* throw a page, if necessary */
    if (ypos<bmarg) {
	ps_throw_page();
	y = ymax - (scorespacing*opts.ysquash)/2, ypos = ymax - lineheight;
    }

    /* set up other variables */
    bx = xpos + thin_barline/2, bax = axpos;/* line begins here */
    break_ax = -1;		       /* no break, as yet */

    clear_syms();		       /* begin collecting symbols */

    /* print bar number, if required - we don't print it for bar one */
    if (opts.bar_numbers && bar_num != 1) {
	char s[10];
	sprintf(s, "%d", bar_num);
	put_text(xpos, 0, y+barnum_yoff, s, ALIGN_LEFT);
    }

    if (first) {
	for (i=0; i<max_staves; i++)
	    if (staves[i].name && *staves[i].name) {
		int yy;
		if (i<max_staves-1 && staves[i+1].name &&
		    !*staves[i+1].name)/* name between two staves */
		    yy = stave_y(i, STAVE_BOTTOM) + stave_y(i+1, STAVE_TOP);
		else
		    yy = 2*stave_y(i, STAVE_MIDDLE);
		yy /= 2;
		put_text (xpos - sname_xoff, axpos, yy - text_ht/3,
			  staves[i].name, ALIGN_RIGHT);
	    }
	first = FALSE;
    }

    /* draw brackets and braces to left of score line */
    for (i=0; i<max_staves; i++) {
	if (staves[i].bkt == -1 && (i==0 || staves[i-1].bkt1 != -1))
	    put_symbol(xpos-brace_xoffset, 0,
		       stave_y(i,STAVE_BOTTOM)-brace_top_yoff, s_braceupper);
	else if (staves[i].bkt == -1)
	    put_symbol(xpos-brace_xoffset, 0,
		       stave_y(i,STAVE_TOP)+brace_bot_yoff, s_bracelower);
	else if (staves[i].bkt > 0) {
	    int b = staves[i].bkt;
	    int b_minus = (i==0 ? 0 : staves[i-1].bkt1);
	    int b_plus = (i==max_staves-1 ? 0 : staves[i].bkt1);

	    for (j=0; j<b; j++)
		put_line(xpos-j*bracket_spacing-bracket_xoff,
			 0, stave_y(i, STAVE_TOP),
			 xpos-j*bracket_spacing-bracket_xoff, 0,
			 stave_y(i, STAVE_BOTTOM),
			 bracket_thick);
	    for (j=b_minus; j<b; j++)
		put_symbol(xpos-j*bracket_spacing+bracket_s_xoff, 0,
			   stave_y(i, STAVE_TOP), s_bracketupper);
	    for (j=b_plus; j<b; j++)
		put_symbol(xpos-j*bracket_spacing+bracket_s_xoff, 0,
			   stave_y(i, STAVE_BOTTOM), s_bracketlower);
	}

	if (i<max_staves-1 && staves[i].bkt1 > 0) {
	    for (j=0; j<staves[i].bkt1; j++)
		put_line(xpos-j*bracket_spacing-bracket_xoff, 0,
			 stave_y(i, STAVE_BOTTOM),
			 xpos-j*bracket_spacing-bracket_xoff, 0,
			 stave_y(i+1, STAVE_TOP), bracket_thick);
	}
    }

    /* if there's more than one stave, draw a vertical line down the left */
    if (max_staves > 1)
	put_line (xpos+thin_barline/2, 0, stave_y(0, STAVE_TOP),
		  xpos+thin_barline/2, 0, stave_y(max_staves-1, STAVE_BOTTOM),
		  thin_barline);

    draw_clefs();
    draw_keysig(7, opts.keysig);
    if (new_time_sig) {
	new_time_sig = FALSE;
	draw_timesig();
    }

    restart_slurs();
    restart_ties();

    if (barline_type == BL_REP_START || barline_type == BL_REP_BOTH)
	do_bar_line (BL_REP_START, FALSE);

    restart_hairpins();
    restart_eightva();
}

/* Process the keywords at the start of a bar. Return 0 if the bar
 * is "multiple bars' rest", 1 if there are melody lines to be read
 * and processed in the bar. */
static int bar_start_kws(void) {
    char *buf = NULL;

    wrk.repeat_ending = REP_ENDING_NONE;
    if (wrk.bar_right)
	mfree (wrk.bar_right);
    wrk.bar_right = NULL;
    wrk.bars_rest = 0;
    wrk.same_bar = FALSE;

    if (!getword(&buf))
	fatal(-1, -1, "file terminates unexpectedly");

    while (!atoi(buf) && !strchr(buf, '|')) {
	if (!strcmp(buf, "FIRST")) {
	    /* "First" repeat ending */
	    wrk.repeat_ending = REP_ENDING_1ST;
	    save_ties();
	} else if (!strcmp(buf, "SECOND")) {
	    /* "Second" repeat ending */
	    wrk.repeat_ending = REP_ENDING_2ND;
	    restore_ties();
	} else if (!strcmp(buf, "SEGNO")) {
	    /* Segno sign above bar line */
	    put_symbol (xpos, axpos, y+sign_yoff, s_segno);
	} else if (!strcmp(buf, "CODA")) {
	    /* Coda sign above bar line, plus potential break in staves */
	    if (axpos > 0) {
		break_ax = axpos;
		break_from_x = xpos - thin_barline/2;
		xpos += coda_break_wid;
		break_to_x = xpos + thin_barline/2;
		bar_line (BARLINE_THIN, FALSE);
	    } else
		xpos += coda_width;
	    put_symbol (xpos, axpos, y+sign_yoff, s_coda);
	} else if (!strcmp(buf, "REST")) {
	    /* This bar contains no melody data; every stave gets a sort of
	     * "|- 12 -|" type symbol written in the middle, with the number
	     * of bars rest. */
	    int l, i;
	    char *p;

	    if (!getword(&buf))
		fatal(-1, -1, "file terminates unexpectedly");
	    if ( (wrk.bars_rest = atoi(buf)) <= 0)
		error(line_no, -1, "expected number after REST keyword");
	    l = bigdigit_wid * (strlen(buf)-1);
	    for (i=0; i<max_staves; i++) {
		int yy = stave_y (i, STAVE_MIDDLE);
		int xp = xpos + bar_end_space;

		put_line (xp+barsrest_ospace, axpos, yy-barsrest_cross/2,
			  xp+barsrest_ospace, axpos, yy+barsrest_cross/2,
			  barsrest_thick);
		put_line (xp+barsrest_ospace+barsrest_thick/2, axpos, yy,
			  xp+barsrest_ospace+barsrest_stem, axpos, yy,
			  barsrest_thick);
		put_line (xp+barsrest_whole+l, axpos, yy-barsrest_cross/2,
			  xp+barsrest_whole+l, axpos, yy+barsrest_cross/2,
			  barsrest_thick);
		put_line (xp+barsrest_whole+l-barsrest_stem, axpos, yy,
			  xp+barsrest_whole+l-barsrest_thick/2, axpos, yy,
			  barsrest_thick);
		for (p=buf; *p; p++)
		    put_symbol (xp+barsrest_ospace+barsrest_stem+
				barsrest_ispace+bigdigit_wid*(p-buf),
				axpos, yy-bigdigit_ht/2, bigdigit(*p - '0'));
	    }
	    xpos += l+barsrest_whole+barsrest_ospace;
	} else if (!strcmp(buf, "SAMEBAR")) {
	    wrk.same_bar = TRUE;
	} else {
	    int rjust = FALSE;

	    if (!strcmp(buf, "RIGHT")) {
		rjust = TRUE;
		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
	    }

	    if (buf[0] != '"' || buf[strlen(buf)-1] != '"') {
		if (!rjust)
		    error (line_no, -1,
			   "expected string, keyword or stave number");
		else
		    error (line_no, -1,
			   "expected string after RIGHT keyword");
	    }

	    buf[strlen(buf)-1] = '\0';
	    if (rjust)
		wrk.bar_right = dupstr (buf+1);
	    else
		put_text (xpos, axpos, y+text_yoff, buf+1, ALIGN_LEFT);
	}
	if (!getword(&buf))
	    fatal(-1, -1, "file terminates unexpectedly");
    }

    ungetword (&buf);
    mfree (buf);

    return (wrk.bars_rest == 0);
}

/* For parameter changes. */
static int redraw_clefs, redraw_key, redraw_time, oldkey;

/* Do the keywords that begin the next bar but have to be processed at the
 * end of this one: CLEF, KEY, TIME and NEWLINE. */
static void param_change_kws(void) {
    char *buf = NULL;

    redraw_clefs = FALSE, redraw_key = FALSE, redraw_time = FALSE;

    while (1) {
	int stave;

	if (!getword(&buf))
	    fatal(-1, -1, "file terminates unexpectedly");

	if (!strcmp(buf, "CLEF")) {
	    /* syntax: CLEF <stave-number> <clef> */
	    int clef;

	    if (!getword(&buf))
		fatal(-1, -1, "file terminates unexpectedly");
	    if (strspn(buf, "0123456789") != strlen(buf) ||
		(stave=atoi(buf)) < 1 || stave > real_max_staves) {
		error(line_no, -1, "unrecognised stave number `%s'", buf);
		continue;
	    }
	    if (!getword(&buf))
		fatal(-1, -1, "file terminates unexpectedly");
	    if (stave < stave_min || stave > stave_max)
		continue;	       /* this stave isn't being drawn */
	    stave -= stave_min;	       /* convert to *our* numbering */
	    if (staves[stave].clef == CLEF_LINE) {
		error(line_no, -1,
		      "cannot change clef on a single-line stave");
		continue;
	    }
	    if ((clef = sparse(buf, clefs)) < 0) {
		error(line_no, -1, "unrecognised clef type `%s'", buf);
		continue;
	    }
	    if (clef == CLEF_LINE)
		error(line_no, -1, "cannot change clef to `LINE'");
	    else
		staves[stave].clef = clef, redraw_clefs = TRUE;
	} else if (!strcmp(buf, "KEY")) {
	    /* usage: as in "EXTERNALS" section */
	    char *buf2 = NULL;
	    int key;

	    if (!getword(&buf))
		fatal(-1, -1, "file terminates unexpectedly");
	    if (!getword(&buf2))
		fatal(-1, -1, "file terminates unexpectedly");

	    if ( (key = parse_key_sig (buf, buf2)) < 0)
		error(line_no, -1, "unable to parse key signature `%s %s'",
		      buf, buf2);
	    else {
		oldkey = opts.keysig;
		opts.keysig = key;
		redraw_key = TRUE;
	    }

	    mfree (buf2);
	} else if (!strcmp(buf, "TIME")) {
	    /* usage: as in "EXTERNALS" section */
	    int time;

	    if (!getword(&buf))
		fatal(-1, -1, "file terminates unexpectedly");

	    if ( (time = parse_time_sig (buf)) < 0)
		error(line_no, -1, "unable to parse time signature `%s'",
		      buf);
	    else
		opts.timesig = time, redraw_time = TRUE;
	} else if (!strcmp(buf, "NEWLINE")) {
	    if (!opts.manual_breaks)
		error(line_no, -1, "manual line breaking not in force,"
		      " `NEWLINE' encountered");
	    else
		break_now = TRUE;
	} else {
	    ungetword(&buf);
	    break;
	}
    }

    mfree (buf);
}

void redraw_params(int break_now) {
    if (!redraw_clefs && redraw_key)
	xpos += bar_end_space;
    if (redraw_clefs)
	draw_clefs();
    if (redraw_key)
	draw_keysig(oldkey, opts.keysig);
    if (redraw_time) {
	draw_timesig();
	if (break_now)
	    new_time_sig = TRUE;
    }
}

/* Process one bar. Returns the type of the bar line, corresponding
 * to the "enum" in drawing.h. */
int process_bar(int *eof) {
    int i = bar_start_kws();
    int j;
    static const char *const barlines[] = {
	"|", "||", "|||", ":|", "|:", ":|:", 0
    };
    char *buf = NULL;
    int bar_x, bar_ax;		       /* bar start, for repeat endings */

    wrk.factor = 1;
    wrk.mlines = NULL;
    wrk.mline_count = wrk.mline_size = 0;
    for (j=0; j<max_staves; j++)
	staves[j].ditto = FALSE;
    wrk.bar_min_width = 0;
    bar_x = xpos, bar_ax = axpos;
    xpos += bar_end_space;
    if (i == 1) {
	/* from now, we can re-use i */
	read_melody();
	restore_slurs();
	preprocess_melody();
	wrk.mline_ptr = mmalloc (wrk.mline_count * sizeof(*wrk.mline_ptr));

	for (j=0; j<wrk.mline_count; j++) {
	    wrk.mline_ptr[j] = wrk.mlines[j]->head;
	    wrk.mlines[j]->time_pos = 0;
	    wrk.mlines[j]->beam = NULL;
	}
	wrk.time_pos = 0;

	/* set up the initial accidentals in force */
	for (i=0; i<max_staves; i++) {
	    if (staves[i].clef == CLEF_LINE) {
		for (j=0; j<2*MAXPITCH+1; j++)
		    wrk.accidental[i][j] = NATURAL;
	    } else {
		for (j=0; j<2*MAXPITCH+1; j++)
		    wrk.accidental[i][j]=initial_acc[opts.keysig][(j+PMOD)%7];
	    }
	}

	do {
	    stream_controls();
	    process_notes();
	    for (j=0; j<wrk.mline_count; j++)
		if (wrk.mline_ptr[j])
		    break;
	} while (j<wrk.mline_count);

	mfree (wrk.mline_ptr);
	save_slurs();
	free_melody();
    }
    xpos += bar_end_space;
    if (xpos < wrk.bar_min_width + bar_x)
	xpos = wrk.bar_min_width + bar_x;
    for (i=0; i<max_staves; i++)
	if (staves[i].ditto) {
	    int j;
	    for (j=0; j<=2*MAXPITCH; j++)   /* wipe out all ties */
		wrk.ties[i][j].exists = FALSE;
	    put_symbol ( (bar_x+xpos)/2, (bar_ax+axpos)/2,
			stave_y(i, STAVE_MIDDLE), s_ditto);
	}

    if (!getword(&buf)) {
	fatal(-1, -1, "expected bar line, found end of file");
	i = BL_THIN_THICK;
    } else
	i = sparse (buf, barlines);
    if (i<0) {
	error(line_no, -1, "unrecognised bar line type `%s'", buf);
	i = BL_NORMAL;
    }

    /* check for end-of-file straight after a bar line */
    if (getword(&buf)) {
	*eof = FALSE;
	ungetword (&buf);
    } else
	*eof = TRUE;

    /* repeat ending markers, if any */
    if (wrk.repeat_ending) {
	put_line(bar_x, bar_ax, y+rep_ending_bot,
		 bar_x, bar_ax, y+rep_ending_top, thin_barline);
	put_line(bar_x, bar_ax, y+rep_ending_top,
		 xpos, axpos, y+rep_ending_top, thin_barline);
	if (wrk.repeat_ending == REP_ENDING_1ST)
	    put_line(xpos, axpos, y+rep_ending_bot,
		     xpos, axpos, y+rep_ending_top, thin_barline);
	put_symbol(bar_x+rep_ending_symx, bar_ax, y+rep_ending_symy,
		   wrk.repeat_ending == REP_ENDING_1ST ? s_big1 : s_big2);
    }

    /* text above the bar, if any */
    if (wrk.bar_right) {
	put_text (xpos, axpos, y+text_yoff, wrk.bar_right, ALIGN_RIGHT);
	mfree (wrk.bar_right);
	wrk.bar_right = NULL;
    }

    /* increment bar number */
    prev_bar_num = bar_num;
    if (!wrk.same_bar && (bar_num>1 || wrk.time_pos>=bar_len(opts.timesig))) {
	if (wrk.bars_rest)
	    bar_num += wrk.bars_rest;
	else
	    bar_num++;
    }

    /* do end of bar keywords (really start of next bar) */
    if (!*eof)
	param_change_kws();

    mfree (buf);
    return i;
}

/* Read in melody lines */
static void read_melody(void) {
    char *buf = NULL;
    int stave, force, c, ditto;

    while (getword(&buf) && (stave=atoi(buf))!=0) {
	ditto = FALSE;

	if (stave<1 || stave>real_max_staves)
	    error (line_no, -1, "stave number `%d' out of range", stave);

	if (wrk.mline_count >= wrk.mline_size) {
	    wrk.mline_size += 32;      /* 32 at a time should be fine */
	    wrk.mlines = mrealloc (wrk.mlines,
				   wrk.mline_size * sizeof(*wrk.mlines));
	}
	force = 0;
	if (stave < stave_min || stave > stave_max)
	    stave = -1;
	else
	    stave -= stave_min;

	/* do keywords like DITTO, DOWN, UP */
	while (1) {
	    while ( (c=nd_getchr()) != EOF && isspace(c))
		getchr();
	    if (nd_getchr() == '{')
		break;
	    if (!getword (&buf))
		fatal (-1, -1, "bar line expected before end of file");
	    if (!strcmp(buf, "DITTO")) {
		ditto = TRUE;
		if (stave != -1) {
		    staves[stave].ditto = TRUE;
		    wrk.bar_min_width = ditto_width;
		}
		break;
	    } else if (!strcmp(buf, "DOWN")) {
		force = -1;
	    } else if (!strcmp(buf, "UP")) {
		force = 1;
	    } else
		error (line_no, -1,
		       "unrecognised melody keyword \"%s\"", buf);
	}

	if (!ditto) {
	    getchr();		       /* eat up the open brace */
	    wrk.mlines[wrk.mline_count] = readML();
	    if (stave != -1) {
		wrk.mlines[wrk.mline_count]->stave_number = stave;
		if ( (wrk.mlines[wrk.mline_count]->stems_force = (force!=0)) )
		    wrk.mlines[wrk.mline_count]->stems_down = (force < 0);
		wrk.mline_count++;
	    } else
		free_ml (wrk.mlines[wrk.mline_count]);
	}
    }
    ungetword (&buf);
    mfree (buf);
}

/* Free melody lines */
static void free_melody(void) {
    int i;

    for (i=0; i<wrk.mline_count; i++)
	free_ml (wrk.mlines[i]);
    mfree (wrk.mlines);
    wrk.mline_count = wrk.mline_size = 0;
    wrk.mlines = NULL;
}

static void free_ml (MelodyLine *ml) {
    MelodyEvent *me;
    NoteHead *n_h;
    NoteFlags *nf;

    for (me=ml->head; me ;) {
	MelodyEvent *temp;
	if (me->type == EV_STREAM || me->type == EV_SGARBAGE) {
	    if (me->u.s.text)
		mfree (me->u.s.text);
	} else {
	    for (n_h=me->u.n.hhead; n_h ;) {
		NoteHead *temp = n_h->next;
		mfree (n_h);
		n_h = temp;
	    }
	    for (nf=me->u.n.fhead; nf ;) {
		NoteFlags *temp = nf->next;
		mfree (nf);
		nf = temp;
	    }
	}
	temp = me->next;
	mfree (me);
	me = temp;
    }
    mfree (ml);
}

int report_bar_num(void) {
    char buf[40];		       /* can't *have* that many bars... */
    sprintf(buf, " %d ", prev_bar_num);
    fputs(buf, stderr);
    fflush (stderr);
    return strlen(buf);
}

static void draw_clefs (void) {
    int i;

    xpos += clef_width;

    for (i=0; i<max_staves; i++) {
	int type, posn = 0;

	switch (staves[i].clef) {
	  case CLEF_TREBLE:	type = s_clefG, posn = -nh;	break;
	  case CLEF_SOPRANO:	type = s_clefG, posn = 0;	break;
	  case CLEF_MEZZSOPR:	type = s_clefC, posn = -nh;	break;
	  case CLEF_ALTO:	type = s_clefC, posn = 0;	break;
	  case CLEF_TENOR:	type = s_clefC, posn = nh;	break;
	  case CLEF_BARITONE:	type = s_clefF, posn = 0;	break;
	  case CLEF_BASS:	type = s_clefF, posn = nh;	break;
	  default: /* CLEF_LINE */ type = -1;			break;
	}

	if (type != -1)
	    put_symbol (xpos-clef_back, axpos,
			stave_y(i, STAVE_MIDDLE)+posn, type);
    }
}

/* Draw a key signature. This is also used when changing keys, so if "oldk"
 * is not 7 (C major) some naturals may have to be drawn. */
static void draw_keysig (int oldk, int newk) {
    static const int sharp_pitch[] = {
	3, 0, 4, 1, 5, 2, 6
    };
    int i, j;

    /* Natural out old sharps, if any... */
    for (i=7; i<oldk; i++)
        if (initial_acc[newk][sharp_pitch[i-7]] == NATURAL) {
	    xpos += ks_nat_width;
	    for (j=0; j<max_staves; j++)
		if (staves[j].clef != CLEF_LINE)
		    put_symbol(xpos-ks_nat_xoff, axpos,
			       stave_y(j, STAVE_MIDDLE) +
			       sharppos[staves[j].clef][i-7]*ph, s_natural);
	}
    /* ...then flats, if any */
    for (i=7; i>oldk; i--)
        if (initial_acc[newk][sharp_pitch[i-1]] == NATURAL) {
	    xpos += ks_nat_width;
	    for (j=0; j<max_staves; j++)
		if (staves[j].clef != CLEF_LINE)
		    put_symbol(xpos-ks_nat_xoff, axpos,
			       stave_y(j, STAVE_MIDDLE) +
			       flatpos[staves[j].clef][7-i]*ph, s_natural);
	}

    /* Draw key signature: sharps first, if any... */
    for (i=7; i<newk; i++) {
	xpos += ks_sharp_width;
	for (j=0; j<max_staves; j++)
	    if (staves[j].clef != CLEF_LINE)
		put_symbol(xpos-ks_sharp_xoff, axpos,
			   stave_y(j, STAVE_MIDDLE) +
			   sharppos[staves[j].clef][i-7]*ph, s_sharp);
    }
    /* ...then flats, if any */
    for (i=7; i>newk; i--) {
	xpos += ks_flat_width;
	for (j=0; j<max_staves; j++)
	    if (staves[j].clef != CLEF_LINE)
		put_symbol(xpos-ks_flat_xoff, axpos,
			   stave_y(j, STAVE_MIDDLE) +
			   flatpos[staves[j].clef][7-i]*ph, s_flat);
    }
}

static void draw_timesig(void) {
    int i, j;

    xpos += time_space;
    if (opts.timesig == TIME_C || opts.timesig == TIME_CBAR) {
	for (i=0; i<max_staves; i++)
	    put_symbol(xpos, axpos, stave_y(i, STAVE_MIDDLE),
		       (opts.timesig == TIME_C ? s_timeC : s_timeCbar));
	xpos += time_common;
    } else {
	int pl, ql, px, qx, len;
	char p[5], q[5];

	sprintf(p, "%d", (opts.timesig >> 8) & 0xFF);
	sprintf(q, "%d", opts.timesig & 0xFF);

	pl = strlen(p);
	ql = strlen(q);

	len = pl>ql ? pl : ql;

	px = xpos + (len-pl)*bigdigit_wid/2;
	qx = xpos + (len-ql)*bigdigit_wid/2;

	for (i=0; i<max_staves; i++) {
	    int y = stave_y(i, STAVE_MIDDLE);

	    for (j=0; j<pl; j++)
		put_symbol(px+j*bigdigit_wid, axpos, y+32,
			       bigdigit(p[j]-'0'));
	    for (j=0; j<ql; j++)
		put_symbol(qx+j*bigdigit_wid, axpos, y-464,
			   bigdigit(q[j]-'0'));
	}
	xpos += len*bigdigit_wid;
    }
}
