#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "mus.h"
#include "pscript.h"
#include "header.h"
#include "drawing.h"
#include "misc.h"
#include "measure.h"
#include "input.h"
#include "slurs.h"

static char *pname;		       /* program name */

/* coordinates are in 3600ths of an inch, ie 1/50 point */
int xmax, ymax;			       /* for A4 paper */
int lineheight;			       /* "constant" */
int break_now;			       /* drawing.c will set this */

static int pwidth = 29763, pheight = 42094;   /* default A4 paper */
int lmarg = 2125, rmarg = 2125, tmarg = 2125, bmarg = 2125;

static int column, verbose = FALSE;    /* for progress reports */

int stave_min = 1, stave_max = 0;      /* stave range actually being drawn */
static char *fname = NULL;	       /* input file, or NULL if stdin */
static char *oname = NULL;	       /* input file, or NULL if stdout */
static void parse_args (int ac, char **av);

int main(int ac, char **av) {
    int barline_type, eof;
    int too_long;

    pname = *av;
    parse_args (ac, av);
    xmax = pwidth - rmarg;
    ymax = pheight - tmarg;

    if (fname) {
	if (!(in_fp = fopen(fname, "r"))) {
	    fprintf(stderr, "%s: unable to open `%s'\n", pname, fname);
	    exit (1);
	}
    } else
	in_fp = stdin;
    if (oname) {
	if (!(out_fp = fopen(oname, "w"))) {
	    fprintf(stderr, "%s: unable to open `%s'\n", pname, oname);
	    exit (1);
	}
    } else
	out_fp = stdout;
    
    ps_init();
    ypos = ymax;

    read_header();		       /* up to the MUSIC: section */

    /* initialise variables and "constants" */
    y = 0;			       /* make stave_y() predictable */
    lineheight = stave_y(1,0) - stave_y(max_staves,2) +
	(scorespacing * opts.ysquash);
    wrk.bar_right = NULL;	       /* make sure we don't try to free it */

    /* allocate arrays */
    wrk.notes_used = mmalloc (max_staves*sizeof(*wrk.notes_used));
    wrk.ties = mmalloc (max_staves*sizeof(*wrk.ties));
    wrk.saveties = mmalloc (max_staves*sizeof(*wrk.saveties));
    wrk.accidental = mmalloc (max_staves*sizeof(*wrk.accidental));
    wrk.hairpins = mmalloc (max_staves*sizeof(*wrk.hairpins));
    wrk.eightva = mmalloc (max_staves*sizeof(*wrk.eightva));

    /* clear arrays */
    memset (wrk.ties, 0, max_staves*sizeof(*wrk.ties));
    memset (wrk.saveties, 0, max_staves*sizeof(*wrk.saveties));
    memset (wrk.hairpins, 0, max_staves*sizeof(*wrk.hairpins));
    memset (wrk.eightva, 0, max_staves*sizeof(*wrk.eightva));

    column = 0;
    begin_line(indent + first_indent, BL_NORMAL);
    do {
	break_now = FALSE;
	barline_type = process_bar(&eof);
	too_long = (xpos + axpos*opts.max_ax > xmax);
	if (!opts.manual_breaks) {
	    if (wrk.same_bar)
		break_now = FALSE;
	    else
		break_now = too_long;
	}
	if (eof)
	    break_now = TRUE;
	do_bar_line (barline_type, break_now);
	redraw_params(break_now);
	if (verbose) {
	    column += report_bar_num ();
	    if (column > 70) {
		static const char *const text_bars[] = {
		    "|\n", "||\n", "|||\n", ":|\n", "||\n|:", ":|\n|:"
		};
		static int cols[] = {
		    0, 0, 0, 0, 2, 2
		};
		fputs(text_bars[barline_type], stderr);
		column = cols[barline_type];
	    } else {
		static const char *const text_bars[] = {
		    "|", "||", "|||", ":|", "|:", ":|:"
		};
		static int cols[] = {
		    1, 2, 3, 2, 2, 3
		};
		fputs(text_bars[barline_type], stderr);
		column += cols[barline_type];
	    }
	}
	if (break_now) {
	    double hdsq;

	    windup_slurs();
	    windup_ties();
	    windup_hairpins();
	    windup_eightva();
	    draw_staves();
	    if (too_long)
		/* be slightly careful in case axpos==0, which it shouldn't */
		hdsq = (axpos ? ((double)(xmax-xpos))/axpos : 0);
	    else
		hdsq = opts.max_ax;
	    draw_beams (hdsq);
	    draw_slurs (hdsq);
	    draw_all (hdsq);
	    if (!eof)
		begin_line (indent, barline_type);
	}
    } while (!eof);

    if (column)
	fprintf(stderr, "\n");

    ps_done();

    return 0;
}

/* convert a string to a measurement. (name by analogy with atoi, atol, atof)
 * Syntax is a number, followed by a unit. Units are cm, in, mm, pt.
 * Output is in MUS-standard 1/3600 inch units. */
static int atom (char *s) {
    char *p = s + strspn(s, "0123456789.");
    double unit;

    if (!strcmp(p, "in"))
	unit = 3600;
    else if (!strcmp(p, "cm"))
	unit = 3600 / 2.54;
    else if (!strcmp(p, "mm"))
	unit = 3600 / 25.4;
    else if (!strcmp(p, "pt"))
	unit = 3600 / 72;
    else {
	fprintf(stderr, "%s: unable to parse measurement `%s'\n"
		"measurements can be in cm, mm, in or pt (points)\n"
		"e.g. `1.3cm' or `22mm'\n", pname, s);
	exit (1);
    }
    *p = '\0';
    return (int) (unit * atof(s));
}

static void do_opt_val (char opt, char *value) {
    char *p;

    switch (opt) {
      case 'o':			       /* output file */
	oname = value;
	break;
      case 'w':			       /* page width */
	pwidth = atom(value);
	break;
      case 'h':			       /* page height */
	pheight = atom(value);
	break;
      case 'l':			       /* left margin */
	lmarg = atom(value);
	break;
      case 'r':			       /* right margin */
	rmarg = atom(value);
	break;
      case 't':			       /* top margin */
	tmarg = atom(value);
	break;
      case 'b':			       /* bottom margin */
	bmarg = atom(value);
	break;
      case 's':			       /* stave range */
	if (strspn(value, "0123456789-") != strlen(value)) {
	    fprintf(stderr, "%s: unable to parse stave range `%s'\n",
		    pname, value);
	    exit (1);
	}
	p = strchr (value, '-');
	if (!p) {
	    /* one stave only */
	    stave_min = stave_max = atoi(value);
	} else if (p == strrchr(value, '-')) {
	    *p++ = '\0';
	    stave_min = atoi(value);
	    stave_max = atoi(p);
	    if (stave_min >= stave_max) {
		fprintf(stderr, "%s: unable to parse stave range `%s'\n",
			pname, value);
		exit (1);
	    }
	} else {
	    fprintf(stderr, "%s: unable to parse stave range `%s'\n",
		    pname, value);
	    exit (1);
	}

	if (stave_min < 1) {
	    fprintf(stderr, "%s: unable to parse stave range `%s'\n",
		    pname, value);
	    exit (1);
	}
	    
	break;
    }
}

static void usage (char *str) {
    if (str) {
	fprintf(stderr, "%s: %s\n", pname, str);
	exit (1);
    } else {
	fprintf(stderr, "usage: %s [-v] [-V] [-?] [-o output-file] "
		"[-w page-width] [-h page-height]\n       [-l left-margin]"
		" [-r right-margin] [-t top-margin] [-b bottom-margin]\n"
		"       [-s stave-range] [filename]\nwhere: -v means verbose,"
		" -V means show version, -? means show this text\n", pname);
	exit (0);
    }
}

static void parse_args (int ac, char **av) {
    char buf[40];		       /* only used for a small error msg */
    while (--ac) {
	char c, *p = *++av;
	if (*p != '-') {
	    /* it's a filename: make sure we don't have two! */
	    if (fname)
		usage ("multiple filenames specified");
	    fname = p;
	    if (!oname) {
		char *q;
		oname = mmalloc (strlen(fname)+10);
		strcpy (oname, fname);
		q = strrchr(oname, '.');
		if (!q)
		    q = oname + strlen(oname);
		strcpy (q, ".ps");
	    }
	} else switch (c = *++p) {
	  case '?':		       /* just show usage and exit */
	    usage (NULL);
	    break;
	  case 'v':		       /* verbose */
	    verbose = TRUE;
	    break;
	  case 'V':		       /* version */
	    fprintf (stderr, "%s version %d\n", pname, MUS_VERSION);
	    exit (0);
	    break;
	  case 'o': case 'w': case 'h': case 'l': case 'r':
	  case 't': case 'b': case 's':
	    if (p[1])
		do_opt_val (c, p+1);
	    else if (--ac)
		do_opt_val (c, *++av);
	    else {
		sprintf(buf, "option `-%c' requires an argument", c);
		usage (buf);
	    }
	    break;
	  default:
	    sprintf(buf, "option `-%c' unknown", c);
	    usage (buf);
	    break;
	}
    }
}

static void report(char *result, int line, int col) {
    if (line != -1) {
	if (col != -1)
	    sprintf(result, "line %d col %d: ", line, col);
	else
	    sprintf(result, "line %d: ", line);
    } else
	*result = '\0';
}

void fatal(int line, int col, char *fmt, ...) {
    char position[40];
    va_list ap;

    report (position, line, col);

    va_start (ap, fmt);
    fprintf(stderr, "%s%s: %s", (column ? "\n" : ""), pname, position);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(2);
}

void error(int line, int col, char *fmt, ...) {
    char position[40];
    va_list ap;

    report (position, line, col);

    va_start (ap, fmt);
    fprintf(stderr, "%s%s: %s", (column ? "\n" : ""), pname, position);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    column = 0;
}

/*
 * My own versions of malloc, realloc and free. Because I want
 * malloc and realloc to bomb out and exit the program if they run
 * out of memory, realloc to reliably call malloc if passed a NULL
 * pointer, and free to reliably do nothing if passed a NULL
 * pointer. Of course we can also put trace printouts in, if we
 * need to. I also implement dupstr() here since not all C
 * libraries have strdup().
 */

void *mmalloc(size_t size) {
    void *p = malloc (size);
    if (!p)
	fatal (-1, -1, "out of memory");
    return p;
}

void *mrealloc(void *ptr, size_t size) {
    void *p;
    if (!ptr)
	p = mmalloc (size);
    else {
	p = realloc (ptr, size);
	if (!p)
	    fatal (-1, -1, "out of memory");
    }
    return p;
}

void mfree(void *ptr) {
    if (ptr)
	free (ptr);
}

char *dupstr(char *ptr) {
    char *p = mmalloc(1+strlen(ptr));
    strcpy(p, ptr);
    return p;
}
