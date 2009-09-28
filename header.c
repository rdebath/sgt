#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mus.h"
#include "misc.h"
#include "input.h"
#include "header.h"
#include "drawing.h"
#include "pscript.h"
#include "measure.h"

struct options opts = {		       /* default values */
    -1, -1, SPL_NEVER, 0,	       /* Externals */
    0, 0, 0, maximum_ax, 1.0	       /* Internals */
};

struct stave *staves = NULL;
int max_staves = 0;		       /* initial array size */
int real_max_staves;

static const char *const realclefs[] = {
    "TREBLE", "SOPRANO", "MEZZO-SOPRANO", "ALTO", "TENOR",
    "BARITONE", "BASS", "LINE", 0
};
const char *const *const clefs = realclefs;

void read_header(void) {
    char *buf = NULL;

    /* the first word of the file must be MUS */
    if (!getword(&buf) || strcmp(buf,"MUS"))
	fatal(-1, -1, "file does not begin with MUS signature");

    /* the next should be a number denoting the version
     * of the file format */
    if (!getword(&buf))
	fatal(-1, -1, "file terminates unexpectedly");
    if (strspn(buf,"0123456789")!=strlen(buf))
	fatal(line_no, -1, "string `%s' is not a valid version number",
	      buf);
    if (atoi(buf) > MUS_VERSION)
	fatal(line_no, -1, "MUS file format version %s unsupported", buf);

    /* now we expect a section header, i.e. ending with a colon */
    if (!getword(&buf))
	fatal(-1, -1, "file terminates unexpectedly");
    if (buf[strlen(buf)-1] != ':')
	fatal(line_no, -1, "expected a section header");

    /* so keep processing sections, until we get "MUSIC:" */
    while (strcmp(buf,"MUSIC:")) {
	int i;			       /* we use this everywhere... */

	/* let's be nice to the Americans: "staves" may be spelled "staffs" */
	if (!strcmp(buf, "STAVES:") || !strcmp(buf, "STAFFS:")) {
	    /* read in stave definitions */
	    while (1) {
		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		/* this should be a stave number */
		if (strspn(buf, "0123456789") != strlen(buf))
		    break;

		/* careful here: staves are numbered from zero *inside*
		 * the program, but from one in the MUS file. */
		i = atoi(buf);
		if (i<1) {
		    error(line_no, -1, "stave number %d invalid", i);
		    continue;
		}
		if (i>max_staves) {
		    staves = mrealloc (staves, sizeof(struct stave)*i);
		    while (max_staves<i)
			staves[max_staves++].exists = 0;
		}
		staves[i-1].exists = 1;
		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		if ( (staves[i-1].clef = sparse (buf, clefs)) < 0) {
		    error(line_no, -1, "unknown clef type `%s'", buf);
		    staves[i-1].exists = 0;
		}
	    }

	    if (max_staves==0)
		fatal(-1, -1, "no staves specified");

	    for (i=0; i<max_staves; i++) {
		if (!staves[i].exists)
		    fatal(-1, -1, "stave number %d not defined, "
			  "though a later one is", i);
		staves[i].bkt = staves[i].bkt1 = 0;
		staves[i].name = NULL;
	    }

	    if (stave_max == 0)
		stave_max = max_staves;
	    if (max_staves < stave_max)
		fatal(-1, -1, "stave range on command line exceeds number"
		      " of staves in file");

	    memmove (staves, staves+stave_min-1,
		     (stave_max-stave_min+1) * sizeof(*staves));

	    /* read in bracket/brace directives */
	    while (buf[strlen(buf)-1] != ':') {
		int top, bottom, keywd;
		static const char *const bktbrace[] = {
		    "BRACKET", "BRACE", "NAME", 0
		};
		char *p;

		if ( (keywd = sparse (buf, bktbrace)) < 0) {
		    error(line_no, -1, "unrecognised directive `%s'", buf);
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    continue;
		}

		if (keywd == 2) {
		    char *p;
		    int i;
		    /* NAME directive: treated separately */
		    do {	       /* do while (0), so we can `break' */
			if (!getword(&buf))
			    fatal(-1, -1, "file terminates unexpectedly");
			if (strspn(buf, "0123456789-") != strlen(buf)) {
			    error (line_no, -1, "`%s' is not a valid stave"
				   " number or stave range", buf);
			    break;
			}
			p = strchr(buf, '-');
			if (!p) {
			    /* single stave */
			    i = atoi(buf);
			    if (i<1 || i>max_staves)
				error (line_no, -1, "stave number %d out of"
				       " range", i);
			    else if (i<stave_min || i>stave_max) {
				/* just eat the next word... */
				if (!getword(&buf))
				    fatal(-1, -1,
					  "file terminates unexpectedly");
			    } else if (staves[i-stave_min].name)
				error (line_no, -1, "stave %d already has a"
				       " name", i);
			    else {
				if (!getword(&buf))
				    fatal(-1, -1,
					  "file terminates unexpectedly");
				if (buf[0] != '"')
				    error (line_no, -1,
					   "expected quoted string"
					   " after `NAME'");
				else {
				    buf[strlen(buf)-1] = '\0';
				    staves[i-stave_min].name = dupstr (buf+1);
				}
			    }
			} else if (p == strrchr(buf, '-')) {
			    /* two staves - had better be adjacent */
			    *p++ = '\0';
			    i = atoi (buf);
			    if (i<1 || i>max_staves-1)
				error (line_no, -1, "stave number %d out of"
				       " range", i);
			    else if (atoi(p) != i+1)
				error (line_no, -1, "stave range `%s-%s' does"
				       " not describe two adjacent staves",
				       buf, p);
			    else if (i<stave_min || i>=stave_max) {
				/* just eat the next word... */
				if (!getword(&buf))
				    fatal(-1, -1,
					  "file terminates unexpectedly");
			    } else if (staves[i-stave_min].name)
				error (line_no, -1, "stave %d already has a"
				       " name", i);
			    else if (staves[i-stave_min+1].name)
				error (line_no, -1, "stave %d already has a"
				       " name", i+1);
			    else {
				if (!getword(&buf))
				    fatal(-1, -1,
					  "file terminates unexpectedly");
				if (buf[0] != '"')
				    error (line_no, -1,
					   "expected quoted string"
					   " after `NAME'");
				else {
				    buf[strlen(buf)-1] = '\0';
				    staves[i-stave_min].name = dupstr (buf+1);
				    staves[i-stave_min+1].name = "";
				}
			    }
			} else
			    error (line_no, -1, "`%s' contains multiple `-'"
				   " signs", buf);
		    } while (0);
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    continue;
		}

		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");

		p = strchr(buf, '-');
		if (!p || p != strrchr(buf, '-') ||  /* exactly one minus */
		    strspn(p, "0123456789-") != strlen(p)) {/* nothing else */
		    error(line_no, -1, "unable to parse stave range `%s'",
			  buf);
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    continue;
		}
		*p++ = '\0';
		top = atoi(buf), bottom = atoi(p);
		if (top<1 || bottom>max_staves || top>=bottom) {
		    *--p = '-';	       /* restore buffer, for error message */
		    error(line_no, -1, "unable to parse stave range `%s'",
			  buf);
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    continue;
		}

		if (bottom <= stave_min || top >= stave_max) {
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    continue;
		}
		if (top < stave_min)
		    top = stave_min;
		if (bottom > stave_max)
		    bottom = stave_max;
		top -= stave_min, bottom -= stave_min;

		if (keywd == 1) {      /* BRACE */
		    if (top+1 != bottom)
			error(line_no, -1,
			      "can only brace two adjacent staves");
		    else if (staves[top].clef == CLEF_LINE ||
			     staves[top+1].clef == CLEF_LINE)
			error(line_no, -1, "can't brace single-line stave");
		    else if (staves[top].bkt > 0 ||
			     staves[top].bkt1 > 0 ||
			     staves[top+1].bkt > 0)
			error(line_no, -1,
			      "can't brace and bracket the same stave");
		    else {
			staves[top].bkt = -1;
			staves[top].bkt1 = -1;
			staves[top+1].bkt = -1;
		    }
		} else {	       /* BRACKET */
		    int blevel = staves[top].bkt;

		    for (i=top; i<bottom; i++) {
			if (staves[i].bkt == -1 ||
			    staves[i].bkt1 == -1 ||
			    staves[i+1].bkt == -1) {
			    error(line_no, -1,
				  "can't brace and bracket the same stave");
			    break;
			}
			if (staves[i].bkt != blevel ||
			    staves[i].bkt1 != blevel ||
			    staves[i+1].bkt != blevel) {
			    error(line_no, -1,
				  "can't produce non-nested brackets");
			    break;
			}
		    }
		    if (i==bottom) for (i=top; i<bottom; i++) {
			staves[i].bkt = blevel+1;
			staves[i].bkt1 = blevel+1;
			staves[i+1].bkt = blevel+1;
		    }
		}

		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
	    }
	} else if (!strcmp(buf, "TITLES:")) {
	    static const char *const titles[] = {
		"MAIN-TITLE", "AUTHOR", "SUBTITLE", "INSTRUCTIONS", 0
	    };

	    while (1) {
		char *title, *p, *q;

		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		if (buf[strlen(buf)-1] == ':')
		    break;

		i = sparse (buf, titles);
		if (i<0) {
		    error(line_no, -1, "unrecognised keyword `%s'", buf);
		    continue;
		}

		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		if (*buf != '"') {
		    error(line_no, -1, "unable to find title text");
		    continue;
		}
		title = mmalloc(2*strlen(buf)+1);

		p = title, q = buf+1;
		while (*q && (q[1] && *q!='"')) {
		    if (*q=='\\' || *q=='(' || *q==')')
			*p++ = '\\';
		    *p++ = *q++;
		}
		*p = '\0';

		if (i>=0 && i<=2) {
		    static const int sizes[] = {
			maintitle_ht, author_ht, subtitle_ht
		    };

		    ypos -= sizes[i];
		    fprintf(out_fp, "(%s)\n/Times-Roman findfont %d scalefont"
			    " setfont\ndup stringwidth pop %d exch sub\n%s"
			    "\n%d add %d moveto show\n", title, sizes[i],
			    xmax-lmarg, (i==1 ? "" : "2 div "), lmarg, ypos);
		    use_font (F_ROMAN);
		} else {
		    clear_syms();
		    ypos -= text_ht;
		    put_text (lmarg + indent + first_indent, 0, ypos, title,
			      ALIGN_LEFT);
		    draw_all (0);
		}
		mfree (title);
	    }
	} else if (!strcmp(buf, "EXTERNALS:")) {
	    /* this section sets options that are visible in the printout */
	    static const char *const externals[] = {
		"TIME", "KEY", "SPLIT", "BAR-NUMBERS", 0
	    };
	    static const char *const splitlevels[] = {
		"NEVER", "BRACKET", "NONBRACE", "ALWAYS", 0
	    };
	    static const char *const offon[] = {"OFF", "ON", 0};
	    
	    while (1) {
		char *buf2 = NULL;

		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		if (buf[strlen(buf)-1] == ':')
		    break;

		i = sparse (buf, externals);
		switch (i) {
		  case -1:
		    error(line_no, -1, "unrecognised directive `%s'", buf);
		  case 0:	       /* TIME */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if ( (opts.timesig = parse_time_sig (buf)) == -1)
			error(line_no, -1,
			      "unable to parse time signature `%s'", buf);
		    break;
		  case 1:	       /* KEY */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if (!getword(&buf2))
			fatal(-1, -1, "file terminates unexpectedly");
		    if ( (opts.keysig = parse_key_sig (buf, buf2)) == -1)
			error(line_no, -1,
			      "unable to parse key signature `%s %s'",
			      buf, buf2);
		    break;
		  case 2:	       /* SPLIT */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if ( (opts.barline_split = sparse (buf, splitlevels)) < 0)
		    {
			error(line_no, -1,
			      "unrecognised split keyword `%s'", buf);
			opts.barline_split = SPL_NEVER;
		    }
		    break;
		  case 3:	       /* BAR-NUMBERS */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if ( (opts.bar_numbers = sparse (buf, offon)) < 0) {
			error(line_no, -1, 
			      "unrecognised bar numbering keyword `%s'",
			      buf);
			opts.bar_numbers = 0;
		    }
		    break;
		}
		mfree (buf2);
	    }
	} else if (!strcmp(buf, "INTERNALS:")) {
	    /* this section contains options that change the interpretation
	     * of the rest of the Mus file */
	    static const char *const internals[] = {
		"NOTE-NAMES", "NOTES", "LINE-BREAKS", "SQUASH", "YSQUASH", 0
	    };
	    static const char *const notenames[] = {"ENGLISH", "GERMAN", 0};
	    static const char *const notes[] = {"NATURAL", "AS-KEY", 0};
	    static const char *const linebrks[] = {"AUTO", "MANUAL", 0};

	    while (1) {
		if (!getword(&buf))
		    fatal(-1, -1, "file terminates unexpectedly");
		if (buf[strlen(buf)-1] == ':')
		    break;

		i = sparse (buf, internals);
		switch (i) {
		  case -1:
		    error(line_no, -1, "unrecognised directive `%s'", buf);
		  case 0:	       /* NOTE-NAMES */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    i = sparse (buf, notenames);
		    if (i<0)
			error(line_no, -1,
			      "unrecognised note-names keyword `%s'", buf);
		    else
			opts.german_notes = (i == 1 ? 1 : 0);
		    break;
		  case 1:	       /* NOTES */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    i = sparse (buf, notes);
		    if (i<0)
			error(line_no, -1,
			      "unrecognised notes keyword `%s'", buf);
		    else
			opts.notes_as_key = (i == 1 ? 1 : 0);
		    break;
		  case 2:	       /* LINE-BREAKS */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    i = sparse (buf, linebrks);
		    if (i<0)
			error(line_no, -1,
			      "unrecognised line-breaks keyword `%s'", buf);
		    else
			opts.manual_breaks = (i == 1 ? 1 : 0);
		    break;
		  case 3:	       /* SQUASH */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if (strspn(buf, "0123456789") != strlen(buf))
			error(line_no, -1,
			      "squash factor `%s' is not a number", buf);
		    else {
			i = atoi(buf);
			if (i<=0 || i>100)
			    error(line_no, -1,
				  "squash factor %d is not between 1 and 100",
				  i);
			else
			    opts.max_ax = maximum_ax * (i/100.0);
		    }
		    break;
		  case 4:	       /* YSQUASH */
		    if (!getword(&buf))
			fatal(-1, -1, "file terminates unexpectedly");
		    if (strspn(buf, "0123456789") != strlen(buf))
			error(line_no, -1,
			      "y squash factor `%s' is not a number", buf);
		    else {
			i = atoi(buf);
			if (i<=0)
			    error(line_no, -1,
				  "y squash factor %d is negative", i);
			else
			    opts.ysquash = i / 100.0;
		    }
		}
	    }
	} else
	    fatal(line_no, -1, "unrecognised section header `%s'", buf);
    }
    
    if (max_staves==0)
	fatal(-1, -1, "no staves specified");
    if (opts.timesig == -1)
	fatal(-1, -1, "no time signature specified");
    if (opts.keysig == -1)
	fatal(-1, -1, "no key signature specified");

    real_max_staves = max_staves;
    max_staves = stave_max - stave_min + 1;

    free (buf);
}
