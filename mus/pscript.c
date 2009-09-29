#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mus.h"
#include "pscript.h"
#include "prologue.h"
#include "misc.h"
#include "measure.h"

static int page_number;
static int roman, italic;	       /* have these fonts been used? */

FILE *out_fp;

void use_font(int font) {	       /* mark a font as used */
    switch (font) {
      case F_ROMAN:
	roman = 1;
	break;
      case F_ITALIC:
	italic = 1;
	break;
    }
}

void ps_init(void) {
    time_t t;

    time (&t);
    fprintf(out_fp, "%%!PS-Adobe-1.0\n%%%%Creator: MUS2PS by Simon Tatham\n");
    fprintf(out_fp, "%%%%Title: Musical Score\n%%%%CreationDate: %s",
	    ctime (&t));	       /* ctime() gives \n on the end */
    fprintf(out_fp, "%%%%Pages: (atend)\n%%%%DocumentFonts: (atend)\n");
    fprintf(out_fp, "%%%%BoundingBox: 0 0 %d %d\n%%%%EndComments\n",
	    (xmax+rmarg+49) / 50, (ymax+tmarg+49) / 50);
    write_prologue(out_fp);
    fprintf(out_fp, "%%%%EndProlog\n");
    roman = italic = 0;
    ps_init_page(1);
}

void ps_done(void) {
    ps_end_page();
    fprintf(out_fp, "%%%%Trailer\nrestore\n%%%%DocumentFonts:%s%s\n"
	    "%%%%Pages: %d\n", roman ? " Times-Roman" : "",
	    italic ? " Times-Italic" : "",
	    page_number);
}

void ps_init_page(int page) {
    page_number = page;
    fprintf(out_fp, "%%%%Page: %d %d\nsave ip\n", page, page);
}

void ps_end_page(void) {
    fprintf(out_fp, "showpage restore\n");
}

void ps_throw_page(void) {
    ps_end_page();
    ps_init_page(page_number+1);
}

struct sym {
    int x, ax, y;
    int sym;
};

const char *const symnames[] = {
    "accent", "acciaccatura", "appoggiatura", "arpeggio", "big0",
    "big1", "big2", "big3", "big4", "big5", "big6", "big7", "big8",
    "big9", "bowdown", "bowup", "bracelower", "braceupper",
    "bracketlower", "bracketupper", "breath", "breve", "clefC",
    "clefF", "clefG", "coda", "ditto", "doubleflat", "doublesharp",
    "dynamicf", "dynamicm", "dynamicp", "dynamics", "dynamicz",
    "fermata", "flat", "harmart", "harmnat", "headcrotchet",
    "headminim", "legato", "mordentlower", "mordentupper", "natural",
    "repeatmarks", "restbreve", "restcrotchet", "restdemi",
    "resthemi", "restminim", "restquaver", "restsemi", "segno",
    "semibreve", "sforzando", "sharp", "small0", "small1", "small2",
    "small3", "small4", "small5", "small6", "small7", "small8",
    "small9", "smallflat", "smallnatural", "smallsharp",
    "staccatissdn", "staccatissup", "staccato", "stopping",
    "taildemidn", "taildemiup", "tailhemidn", "tailhemiup",
    "tailquaverdn", "tailquaverup", "tailsemidn", "tailsemiup",
    "timeCbar", "timeC", "trill", "turn"
};

#define SYM_LINE -1
#define SYM_BEAM -2
#define SYM_TIE -3
#define SYM_TEXT -4

static struct sym *syms = NULL;
static int symbols = 0, max_sym = 0;
#define delta_sym 2048		       /* size to grow syms[] by */

static char *textstore = NULL;
static int textptr = 0, text_size = 0;
#define delta_text 4096		       /* size to grow textstore[] by */

void clear_syms(void) {
    symbols = textptr = 0;
}

void put_symbol(int x, int ax, int y, int sym) {
    if (symbols >= max_sym) {
	syms = (struct sym *) mrealloc (syms, (max_sym+delta_sym)*
					sizeof(struct sym));
	max_sym += delta_sym;
    }

    syms[symbols].x = x;
    syms[symbols].ax = ax;
    syms[symbols].y = y;
    syms[symbols].sym = sym;
    symbols++;
}

void put_line(int x1, int ax1, int y1,
	      int x2, int ax2, int y2, int thickness) {
    put_symbol(x1, ax1, y1, SYM_LINE);
    put_symbol(x2, ax2, y2, thickness);
}

void put_tie(int x1, int ax1, int x2, int ax2, int y, int down) {
    put_symbol(x1, ax1, y, SYM_TIE);
    put_symbol(x2, ax2, y, down);
}

void put_beam(int x1, int y1, int x2, int y2) {
    put_symbol(x1, 0, y1, SYM_BEAM);
    put_symbol(x2, 0, y2, 0);
}

void put_text(int x, int ax, int y, char *text, int alignment) {
    char *p, *q, *parsed_text = (char *)mmalloc(strlen(text)+1);

    q = parsed_text, p = text;
    while (*p) {
	if (*p=='\\' || *p=='(' || *p==')')
	    *q++ = '\\';
	if (*p != '`')
	    *q++ = *p++;
	else {
	    char *r = ++p;
	    int i;
	    static const char *const notes[] = {
		"0", "b", "1", "sb", "2", "m", "4", "c", "8", "q",
		"16", "sq", "32", "dq", "64", "hq", 0
	    };

	    while (*p && *p!='`')
		p++;
	    if (!*p) {
		error(-1, -1, "unfinished `...` construct in \"%s\"", text);
		continue;
	    }
	    while (*(p-1)=='.' && p-r>0)
		p--;
	    *p = '\0';		       /* temporarily */
	    i = sparse (r, notes);
	    if (i<0)
		error(-1, -1, "unrecognised note type `%s`", r);
	    else {
		*q++ = '\xC9' + i/2;
		while (*p++=='.')
		    *q++ = '\xC8';
	    }
	}
    }
    *q = '\0';

    put_symbol(x, ax, y, SYM_TEXT);
    put_symbol(0, textptr, 0, alignment);
    while (textptr+strlen(parsed_text)+1 > text_size) {
	char *p = (char *)mrealloc (textstore, sizeof(char)*
				    (text_size+delta_text));
	textstore = p;
	text_size += delta_text;
    }
    strcpy (textstore+textptr, parsed_text);
    textptr += strlen(parsed_text)+1;

    mfree (parsed_text);
}

void draw_all(double hdsq) {
    static const char *const tnames[] = {
 	".tdot", ".tbreve", ".tsemibreve", ".tminim", ".tcrotchet",
 	".tquaver", ".tsemiquaver", ".tdemisemiquaver",
 	".themidemisemiquaver", ".df", ".dm", ".dp", ".ds", ".dz"
    };
    int s;
    struct sym *sym;
    char *p;
    int ht;			       /* for working out ties */

    /* project double X coordinate on to axis */
    for (s=0; s<symbols; s++)
	syms[s].x += hdsq * syms[s].ax;

    for (s=symbols, sym=syms; s; s--) {
	switch (sym->sym) {
	  case SYM_LINE:
	    fprintf(out_fp, "%d %d %d %d %d line\n", sym[1].sym,
		    sym->x, sym->y, sym[1].x, sym[1].y);
	    sym++,s--;
	    break;
	  case SYM_BEAM:
	    fprintf(out_fp, "%d %d %d %d beam\n",
		    sym->x, sym->y, sym[1].x, sym[1].y);
	    sym++,s--;
	    break;
	  case SYM_TEXT:
	    fprintf(out_fp, "/Times-Italic findfont " stringise(text_ht)
		    " scalefont setfont %d %d moveto\n", sym->x, sym->y);
	    if (sym[1].sym != ALIGN_LEFT) {
		int wid=0;

		putc('(', out_fp);
		for (p=textstore+sym[1].ax; *p; p++) {
		    if (*p>='\40' && *p<='\176')
			putc(*p, out_fp);
		    else
			wid += tiny_widths[*p-'\xC8'];
		}

		fprintf(out_fp, ") stringwidth pop");
		if (wid>0)
		    fprintf(out_fp, " %d add", wid);
		if (sym[1].sym == ALIGN_CENTRE)
		    fprintf(out_fp, " 2 div");
		fprintf(out_fp, " neg 0 rmoveto\n");
	    }
	    p = textstore+sym[1].ax;
	    while (*p) {
		if (*p>='\40' && *p<='\176') {
		    putc('(', out_fp);
		    while (*p>='\40' && *p<='\176')
			fputc (*p++, out_fp);
		    fprintf(out_fp, ") show\n");
		} else 
		    fprintf(out_fp, "%s\n", tnames[(*p++) - '\xC8']);
	    }

	    sym++,s--;
	    break;
	  case SYM_TIE:
	    ht = abs(sym[1].x - sym->x)/2;
	    if (ht > tie_norm_ht)
		ht = tie_norm_ht;
	    fprintf(out_fp, "%d %d %d %d tie\n", (sym[1].sym ? ht : -ht),
		    sym->x, sym->y, sym[1].x);
	    sym++,s--;
	    break;
	  default:
	    fprintf(out_fp, "%d %d .%s\n", sym->x, sym->y,
		    symnames[sym->sym]);
	    break;
	}
	sym++;
    }
}

static int mark_point;

void put_mark(void) {
    mark_point = symbols;
}

void shift_mark(int rx) {
    int i;

    for (i=mark_point; i<symbols; i++) {
	if (syms[i].x<0)
	    syms[i].x = -syms[i].x;
	else
	    syms[i].x += rx;
    }
}
