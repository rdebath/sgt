/* filigram.c - draw `filigram' pictures, based on the idea of plotting
 * the fractional part of a polynomial in x and y over a pixel grid
 *
 * This program is copyright 2000 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#define VERSION "$Revision: 1.2 $"

#define TRUE 1
#define FALSE 0
#define lenof(x) (sizeof ((x)) / sizeof ( *(x) ))

/* ----------------------------------------------------------------------
 * Function prototypes and structure type predeclarations.
 */

struct Bitmap;
static void fput32(unsigned long val, FILE *fp);
static void fput16(unsigned val, FILE *fp);
static void bmpinit(struct Bitmap *bm, char const *filename,
                    int width, int height);
static void bmppixel(struct Bitmap *bm,
                     unsigned char r, unsigned char g, unsigned char b);
static void bmpendrow(struct Bitmap *bm);
static void bmpclose(struct Bitmap *bm);

struct Poly;
static struct Poly *polyread(char const *string);
static struct Poly *polypdiff(struct Poly *input, int wrtx);
static double polyeval(struct Poly *p, double x, double y);
static void polyfree(struct Poly *p);

struct RGB;
struct Colours;
static struct Colours *colread(char const *string);
static void colfree(struct Colours *cols);
static struct RGB colfind(struct Colours *cols, int xg, int yg);

/* ----------------------------------------------------------------------
 * Output routines for 24-bit Windows BMP. (It's a nice simple
 * format which can easily be converted into other formats; please
 * don't shoot me.)
 */

struct Bitmap {
    FILE *fp;
    unsigned long padding;
};

static void fput32(unsigned long val, FILE *fp) {
    fputc((val >>  0) & 0xFF, fp);
    fputc((val >>  8) & 0xFF, fp);
    fputc((val >> 16) & 0xFF, fp);
    fputc((val >> 24) & 0xFF, fp);
}
static void fput16(unsigned val, FILE *fp) {
    fputc((val >>  0) & 0xFF, fp);
    fputc((val >>  8) & 0xFF, fp);
}

static void bmpinit(struct Bitmap *bm, char const *filename,
                    int width, int height) {
    /*
     * File format is:
     *
     * 2char "BM"
     * 32bit total file size
     * 16bit zero (reserved)
     * 16bit zero (reserved)
     * 32bit 0x36 (offset from start of file to image data)
     * 32bit 0x28 (size of following BITMAPINFOHEADER)
     * 32bit width
     * 32bit height
     * 16bit 0x01 (planes=1)
     * 16bit 0x18 (bitcount=24)
     * 32bit zero (no compression)
     * 32bit size of image data (total file size minus 0x36)
     * 32bit 0xB6D (XPelsPerMeter)
     * 32bit 0xB6D (YPelsPerMeter)
     * 32bit zero (palette colours used)
     * 32bit zero (palette colours important)
     *
     * then bitmap data, BGRBGRBGR... with padding zeros to bring
     * scan line to a multiple of 4 bytes. Padding zeros DO happen
     * after final scan line. Scan lines work from bottom upwards.
     */
    unsigned long scanlen = 3 * width;
    unsigned long padding = ((scanlen+3)&~3) - scanlen;
    unsigned long bitsize = (scanlen + padding) * height;
    FILE *fp = fopen(filename, "wb");
    fputs("BM", fp);
    fput32(0x36 + bitsize, fp); fput16(0, fp); fput16(0, fp);
    fput32(0x36, fp); fput32(0x28, fp); fput32(width, fp); fput32(height, fp);
    fput16(1, fp); fput16(24, fp); fput32(0, fp); fput32(bitsize, fp);
    fput32(0xB6D, fp); fput32(0xB6D, fp); fput32(0, fp); fput32(0, fp);

    bm->fp = fp;
    bm->padding = padding;
}

static void bmppixel(struct Bitmap *bm,
                     unsigned char r, unsigned char g, unsigned char b) {
    putc(b, bm->fp);
    putc(g, bm->fp);
    putc(r, bm->fp);
}

static void bmpendrow(struct Bitmap *bm) {
    int j;
    for (j = 0; j < bm->padding; j++)
        putc(0, bm->fp);
}

static void bmpclose(struct Bitmap *bm) {
    fclose(bm->fp);
}

/* ----------------------------------------------------------------------
 * Routines for handling polynomials.
 */

struct PolyTerm {
    int constant;
    int xpower;
    int ypower;
    struct PolyTerm *next;
};

/*
 * A polynomial is stored as a 2-D array of PolyTerms, going up to
 * `maxdegree' in both cases. The nonzero terms are linked as a
 * list once the polynomial is finalised.
 */
struct Poly {
    int deg;
    struct PolyTerm *head;
    struct PolyTerm *terms;
};

/*
 * Build the linked list in a Poly structure.
 */
static void polymklist(struct Poly *p) {
    struct PolyTerm **tail;
    int i;
    tail = &p->head;
    for (i = 0; i < p->deg * p->deg; i++)
        if (p->terms[i].constant != 0) {
            *tail = &p->terms[i];
            tail = &p->terms[i].next;
        }
    *tail = NULL;
}

/*
 * Parse a string into a polynomial.
 *
 * String syntax must be: a number of terms, separated by + or -.
 * (Whitespace is fine, and is skipped.) Each term consists of an
 * optional integer, followed by an optional x power (x followed by
 * an integer), followed by an optional y power. For example:
 * 
 *   x2+2xy+y2
 * 
 * TODO if I ever get the energy: improve this to be a general
 * expression evaluator.
 */
static struct Poly *polyread(char const *string) {
    struct Poly *output;
    int sign, factor, xpower, ypower;
    int i, j, k;

    output = (struct Poly *)malloc(sizeof(struct Poly));
    output->deg = 0;
    output->terms = NULL;

    while (*string) {
        sign = +1;
        factor = 1;
        xpower = 0;
        ypower = 0;
        if (*string == '-') {
            sign = -1;
            string++;
        }
        i = strspn(string, "0123456789");
        if (i) {
            factor = atoi(string);
            string += i;
        }
        if (*string == 'x') {
            string++;
            xpower = 1;
            i = strspn(string, "0123456789");
            if (i) {
                xpower = atoi(string);
                string += i;
            }
        }
        if (*string == 'y') {
            string++;
            ypower = 1;
            i = strspn(string, "0123456789");
            if (i) {
                ypower = atoi(string);
                string += i;
            }
        }

        i = xpower;
        if (i < ypower) i = ypower;
        i++;
        if (i > output->deg) {
            int old;
            struct PolyTerm *newterms;

            old = output->deg;
            newterms = (struct PolyTerm *)malloc(i*i*sizeof(struct PolyTerm));
            for (j = 0; j < i; j++)
                for (k = 0; k < i; k++) {
                    newterms[k*i+j].xpower = j;
                    newterms[k*i+j].ypower = k;
                    if (j<old && k<old)
                        newterms[k*i+j].constant =
                        output->terms[k*old+j].constant;
                    else
                        newterms[k*i+j].constant = 0;
                }
            if (output->terms)
                free(output->terms);
            output->terms = newterms;
            output->deg = i;
        }
        output->terms[ypower*output->deg+xpower].constant += sign*factor;

        if (*string == '-') {
            /* do nothing */
        } else if (*string == '+') {
            string++;                  /* skip the plus */
        } else if (*string) {
            polyfree(output);
            return NULL;               /* error! */
        }
    }

    polymklist(output);
    return output;
}

/*
 * Partially differentiate a polynomial with respect to x (if
 * wrtx!=0) or to y (if wrtx==0).
 */
static struct Poly *polypdiff(struct Poly *input, int wrtx) {
    struct Poly *output;
    int deg, yp, xp, ydp, xdp, factor, constant;

    output = (struct Poly *)malloc(sizeof(struct Poly));
    deg = output->deg = input->deg;
    output->terms = (struct PolyTerm *)malloc(output->deg * output->deg *
                                              sizeof(struct PolyTerm));

    for (yp = 0; yp < deg; yp++)
        for (xp = 0; xp < deg; xp++) {
            xdp = xp; ydp = yp;
            factor = wrtx ? ++xdp : ++ydp;
            if (xdp >= deg || ydp >= deg)
                constant = 0;
            else
                constant = input->terms[ydp*deg+xdp].constant;
            output->terms[yp*deg+xp].xpower = xp;
            output->terms[yp*deg+xp].ypower = yp;
            output->terms[yp*deg+xp].constant = constant * factor;
        }

    polymklist(output);
    return output;
}

/*
 * Evaluate a polynomial.
 * 
 * TODO: make this more efficient by pre-computing the powers once
 * instead of doing them all, linearly, per term.
 */
static double polyeval(struct Poly *p, double x, double y) {
    struct PolyTerm *pt;
    double result, term;
    int i;

    result = 0.0;
    for (pt = p->head; pt; pt = pt->next) {
        term = pt->constant;
        for (i = 0; i < pt->xpower; i++) term *= x;
        for (i = 0; i < pt->ypower; i++) term *= y;
        result += term;
    }
    return result;
}

/*
 * Free a polynomial when finished.
 */
static void polyfree(struct Poly *p) {
    free(p->terms);
    free(p);
}

/* ----------------------------------------------------------------------
 * Routines for handling lists of colours.
 */
struct RGB {
    double r, g, b;
};

struct Colours {
    int xmod;
    int ncolours;
    struct RGB *colours;
};

/*
 * Read a colour list into a Colours structure.
 */
static struct Colours *colread(char const *string) {
    struct Colours *ret;
    int i;
    struct RGB c, *clist;
    int nc, csize;
    char *q;

    ret = (struct Colours *)malloc(sizeof(struct Colours));
    ret->xmod = ret->ncolours = 0;
    ret->colours = NULL;
    nc = csize = 0;
    clist = NULL;

    /* If there's an `integer!' prefix, set xmod. */
    i = strspn(string, "0123456789");
    if (string[i] == '!') {
	ret->xmod = atoi(string);
	string += i + 1;
    }

    while (*string) {
	/* Now we expect triplets of doubles separated by any punctuation. */
	c.r = strtod(string, &q); string = q; if (!*string) break; string++;
	c.g = strtod(string, &q); string = q; if (!*string) break; string++;
	c.b = strtod(string, &q); string = q;
	if (nc >= csize) {
	    csize = nc + 16;
	    clist = realloc(clist, csize * sizeof(struct RGB));
	}
	clist[nc++] = c;
	if (!*string) break; string++;
    }
    ret->ncolours = nc;
    ret->colours = clist;
    return ret;
}

/*
 * Free a Colours structure when done.
 */
static void colfree(struct Colours *cols) {
    free(cols->colours);
    free(cols);
}

/*
 * Look up a colour in a Colours structure.
 */
static struct RGB colfind(struct Colours *cols, int xg, int yg) {
    int n;
    if (cols->xmod) {
	xg %= cols->xmod;
        if (xg < 0) xg += cols->xmod;
	yg *= cols->xmod;
    }
    n = (xg+yg) % cols->ncolours;
    if (n < 0) n += cols->ncolours;
    return cols->colours[n];
}

/* ----------------------------------------------------------------------
 * The code to do the actual plotting.
 */

struct Params {
    int width, height;
    double x0, x1, y0, y1;
    double xscale, yscale, oscale;
    int fading;
    char const *filename;
    struct Poly *poly;
    struct Colours *colours;
};

static int toint(double d) {
    int i = (int)d;
    if (i <= 0) {
        i = (int)(d + -i + 1) - (-i + 1);
    }
    return i;
}

static int plot(struct Params params) {
    struct Poly *dfdx, *dfdy;
    struct Bitmap bm;

    double xstep, ystep;
    double x, xfrac, y, yfrac;
    double dzdx, dzdy, dxscale, dyscale;
    double z, xfade, yfade, fade;
    struct RGB c;
    int i, j, xg, yg;

    dfdx = polypdiff(params.poly, 1);
    dfdy = polypdiff(params.poly, 0);

    bmpinit(&bm, params.filename, params.width, params.height);

    xstep = (params.x1 - params.x0) / params.width;
    ystep = (params.y1 - params.y0) / params.height;
    dxscale = params.xscale * params.oscale;
    dyscale = params.yscale * params.oscale;
    for (i = 0; i < params.height; i++) {
        y = params.y0 + ystep * i;
        yfrac = y / params.yscale; yfrac -= toint(yfrac);

        for (j = 0; j < params.width; j++) {
            x = params.x0 + xstep * j;
            xfrac = x / params.xscale; xfrac -= toint(xfrac);

            dzdx = polyeval(dfdx, x, y) * dxscale;
            dzdy = polyeval(dfdy, x, y) * dyscale;
            xg = toint(dzdx + 0.5); dzdx -= xg;
            yg = toint(dzdy + 0.5); dzdy -= yg;

            z = polyeval(params.poly, x, y) * params.oscale;
            z -= xg * xfrac;
            z -= yg * yfrac;
            z -= toint(z);

            xfade = dzdx; if (xfade < 0) xfade = -xfade;
            yfade = dzdy; if (yfade < 0) yfade = -yfade;
            fade = 1.0 - (xfade < yfade ? yfade : xfade) * 2;

	    c = colfind(params.colours, xg, yg);

	    if (params.fading) z *= fade;
	    z *= 256.0;

            bmppixel(&bm, toint(c.r*z), toint(c.g*z), toint(c.b*z));
        }
        bmpendrow(&bm);
    }

    bmpclose(&bm);
    polyfree(dfdy);
    polyfree(dfdx);
    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

int parsesize(char const *string, int *x, int *y, char const *name) {
    int i;
    i = strspn(string, "0123456789");
    if (i > 0) {
	*x = atoi(string);
	string += i;
    } else
	goto parsefail;
    if (*string++ != 'x')
	goto parsefail;
    i = strspn(string, "0123456789");
    if (i > 0) {
	*y = atoi(string);
	string += i;
    } else
	goto parsefail;
    if (*string)
	goto parsefail;
    return 1;
    parsefail:
    fprintf(stderr, "filigram: unable to parse %s `%s'\n", name, string);
    return 0;
}

int parseflt(char const *string, double *d, char const *name) {
    char *endp;
    *d = strtod(string, &endp);
    if (endp && *endp) {
	fprintf(stderr, "filigram: unable to parse %s `%s'\n", name, string);
	return 0;
    } else
	return 1;
}

int main(int ac, char **av) {
    char const *usagemsg[] = {
	"usage: filigram [options]",
	"       -o, --output file.bmp   output bitmap name",
	"       -s, --size NNNxNNN      output bitmap size",
	"       -x, --xrange NNN        mathematical x range",
	"       -y, --yrange NNN        mathematical y range",
	"       -X, --xcentre NNN       mathematical x centre point",
	"       -Y, --ycentre NNN       mathematical y centre point",
	"       -b, --basesize NNNxNNN  base image size",
	"       -B, --base              this *is* the base image (default)",
	"       -I, --iscale NNN        input scale (ie base pixel spacing)",
	"       -O, --oscale NNN        output scale (ie modulus)",
	"       -f, --fade              turn on fading at edges of patches",
	"       -p, --poly x2+2xy+y2    polynomial to plot",
	"       -c, --colours 1,0,0:0,1,0  colours for patches",
	"       -v, --verbose           report details of what is done",
	" - at most one of -b, -B and -I should be used",
	" - can give 0 as one dimension in -s or -b and that dimension will",
	"   be computed so as to preserve the aspect ratio",
	" - if only one of -x and -y specified, will compute the other so",
	"   as to preserve the aspect ratio",
	" - colours can be prefixed with N! to get two-dimensional colour",
	"   selection (this needs to be better explained :-)"
    };

    int usage = FALSE;
    int fade = FALSE;
    int isbase = FALSE;
    int verbose = FALSE;
    char *outfile = NULL;
    int imagex = 0, imagey = 0, basex = 0, basey = 0;
    double xcentre, ycentre, xrange, yrange, iscale, oscale;
    int gotxcentre, gotycentre, gotxrange, gotyrange, gotiscale, gotoscale;
    struct Poly *poly = NULL;
    struct Colours *colours = NULL;

    gotxcentre = gotycentre = gotxrange =
	gotyrange = gotiscale = gotoscale = FALSE;

    if (ac < 2) {
	usage = TRUE;
    } else while (--ac) {
	char *arg = *++av;

	if (arg[0] != '-') {
	    usage = TRUE;
	    break;
	} else {
	    char c = arg[1];
	    char *val = arg+2;
	    if (c == '-') {
		static const struct {
		    char const *name;
		    int letter;
		} longopts[] = {
		    {"--output", 'o'},
		    {"--size", 's'},
		    {"--xrange", 'x'},
		    {"--yrange", 'y'},
		    {"--xcentre", 'X'},
		    {"--xcenter", 'X'},
		    {"--ycentre", 'Y'},
		    {"--ycenter", 'Y'},
		    {"--basesize", 'b'},
		    {"--base", 'B'},
		    {"--iscale", 'I'},
		    {"--oscale", 'O'},
		    {"--fade", 'f'},
		    {"--poly", 'p'},
		    {"--colours", 'c'},
		    {"--verbose", 'v'},
		    {"--help", 'h'},
		    {"--version", 'V'},
		};
		int i, j;
		for (i = 0; i < lenof(longopts); i++) {
		    j = strlen(longopts[i].name);
		    if (!strncmp(arg, longopts[i].name, j) &&
			j == strcspn(arg, "=")) {
			c = longopts[i].letter;
			val = arg + j;
			if (*val) val++;
			break;
		    }
		}
		if (c == '-') {
		    fprintf(stderr, "filigram: unknown long option `%.*s'\n",
			    strcspn(arg, "="), arg);
		    return EXIT_FAILURE;
		}
	    }
	    switch (c) {
	      case 'B':
		isbase = TRUE;
		break;
	      case 'f':
		fade = TRUE;
		break;
	      case 'v':
		verbose = TRUE;
		break;
	      case 'h':
		usage = TRUE;
		break;
	      case 'V':
		{
		    char *p = VERSION;
		    int i;
		    p += strcspn(p, " ");
		    if (*p) p++;
		    i = strcspn(p, " $");
		    printf("filigram version %.*s\n", i, p);
		}
		return EXIT_SUCCESS;
	      default:		       /* other options require an arg */
		if (!*val) {
		    if (!--ac) {
			fprintf(stderr,
				"filigram: option `%s' requires an argument\n",
				arg);
			return EXIT_FAILURE;
		    }
		    val = *++av;
		}
		switch (c) {
		  case 'o':	       /* --output */
		    outfile = val;
		    break;
		  case 's':	       /* --size */
		    if (!parsesize(val, &imagex, &imagey, "output image size"))
			return EXIT_FAILURE;
		    break;
		  case 'x':	       /* --xrange */
		    if (!parseflt(val, &xrange, "x range"))
			return EXIT_FAILURE;
		    gotxrange = TRUE;
		    break;
		  case 'y':	       /* --yrange */
		    if (!parseflt(val, &yrange, "y range"))
			return EXIT_FAILURE;
		    gotyrange = TRUE;
		    break;
		  case 'X':	       /* --xcentre */
		    if (!parseflt(val, &xcentre, "x centre"))
			return EXIT_FAILURE;
		    gotxcentre = TRUE;
		    break;
		  case 'Y':	       /* --ycentre */
		    if (!parseflt(val, &ycentre, "y centre"))
			return EXIT_FAILURE;
		    gotycentre = TRUE;
		    break;
		  case 'b':	       /* --basesize */
		    if (!parsesize(val, &basex, &basey, "base image size"))
			return EXIT_FAILURE;
		    break;
		  case 'I':	       /* --iscale */
		    if (!parseflt(val, &iscale, "input scale"))
			return EXIT_FAILURE;
		    gotiscale = TRUE;
		    break;
		  case 'O':	       /* --oscale */
		    if (!parseflt(val, &oscale, "output scale"))
			return EXIT_FAILURE;
		    gotoscale = TRUE;
		    break;
		  case 'p':	       /* --poly */
		    poly = polyread(val);
		    if (!poly) {
			fprintf(stderr, "filigram: "
				"unable to parse polynomial `%s'\n", val);
			return EXIT_FAILURE;
		    }
		    break;
		  case 'c':	       /* --colours */
		    colours = colread(val);
		    if (!colours) {
			fprintf(stderr, "filigram: "
				"unable to parse colour specification `%s'\n",
				val);
			return EXIT_FAILURE;
		    }
		    break;
		}
		break;
	    }
	}
    }

    if (usage) {
	int i;
	for (i = 0; i < lenof(usagemsg); i++)
	    puts(usagemsg[i]);
	return ac == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
	/*
	 * Having read the arguments, now process them.
	 */
	struct Params par;
	int i;
	double aspect;

	/* If no output scale, default to 1. */
	if (!gotoscale)
	    par.oscale = 1.0;
	else
	    par.oscale = oscale;

	/* If no output file, complain. */
	if (!outfile) {
	    fprintf(stderr, "filigram: no output file specified: "
		    "use something like `-o file.bmp'\n");
	    return EXIT_FAILURE;
	} else
	    par.filename = outfile;

	/* If no polynomial, complain. */
	if (!poly) {
	    fprintf(stderr, "filigram: no polynomial specified: "
		    "use something like `-p x2+2xy+y2'\n");
	    return EXIT_FAILURE;
	} else
	    par.poly = poly;

	/* If no colours, default to 1,1,1. */
	if (!colours)
	    par.colours = colread("1,1,1");   /* assume this will succeed */
	else
	    par.colours = colours;

	par.fading = fade;

	/*
	 * If precisely one aspect-ratio-providing size specified,
	 * save the aspect ratio from it, and use it to fill in
	 * blanks in other sizes.
	 */
	if ((imagex&&imagey) + (basex&&basey) + (gotxrange&&gotyrange) == 1) {
	    if (imagex && imagey) aspect = (double)imagex / imagey;
	    if (basex && basey) aspect = (double)basex / basey;
	    if (gotxrange && gotyrange) aspect = gotxrange / gotyrange;

	    if (imagex && !imagey) imagey = imagex / aspect;
	    if (!imagex && imagey) imagex = imagey * aspect;
	    if (basex && !basey) basey = basex / aspect;
	    if (!basex && basey) basex = basey * aspect;
	    if (gotxrange && !gotyrange)
		yrange = xrange / aspect, gotyrange = TRUE;
	    if (!gotxrange && gotyrange)
		xrange = yrange * aspect, gotxrange = TRUE;
	}

	/*
	 * Now complain if no output image size was specified.
	 */
	if (!imagex || !imagey) {
	    fprintf(stderr, "filigram: no output size specified: "
		    "use something like `-s 640x480'\n");
	    return EXIT_FAILURE;
	} else {
	    par.width = imagex;
	    par.height = imagey;
	}

	/*
	 * Also complain if no input mathematical extent specified.
	 */
	if (!gotxrange || !gotyrange) {
	    fprintf(stderr, "filigram: no image extent specified: "
		    "use something like `-x 20 -y 15'\n");
	    return EXIT_FAILURE;
	} else {
	    if (!gotxcentre) xcentre = 0.0;
	    if (!gotycentre) ycentre = 0.0;
	    par.x0 = xcentre - xrange;
	    par.x1 = xcentre + xrange;
	    par.y0 = ycentre - yrange;
	    par.y1 = ycentre + yrange;
	}

	/*
	 * All that's left to set up is xscale and yscale. At this
	 * stage we expect to see at most one of
	 *   `isbase' true
	 *   `basex' and `basey' both nonzero
	 *   `gotiscale' true
	 */
	i = (!!isbase) + (basex && basey) + (!!gotiscale);
	if (i > 1) {
	    fprintf(stderr, "filigram: "
		    "expected at most one of `-b', `-B', `-i'\n");
	    return EXIT_FAILURE;
	} else if (i == 0) {
	    /* Default: isbase is true. */
	    isbase = TRUE;
	}
	if (isbase) {
	    basex = imagex, basey = imagey;
	}
	if (gotiscale)
	    par.xscale = par.yscale = iscale;
	else {
	    par.xscale = (par.x1 - par.x0) / basex;
	    par.yscale = (par.y1 - par.y0) / basey;
	}

	/*
	 * If we're in verbose mode, regurgitate the final
	 * parameters.
	 */
	if (verbose) {
            printf("Output file `%s', %d x %d\n",
                   par.filename, par.width, par.height);
            printf("Mathematical extent [%g,%g] x [%g,%g]\n",
                   par.x0, par.x1, par.y0, par.y1);
            printf("Base pixel spacing %g (horiz), %g (vert)\n",
                   par.xscale, par.yscale);
            printf("Output modulus %g\n",
                   par.oscale);
	}

	i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

        colfree(par.colours);
        polyfree(par.poly);
        return i;
    }
}
