/* newton.c - draw fractals based on the convergence of the Newton-
 * Raphson algorithm in the complex plane in the presence of
 * multiple roots
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
#include <math.h>

#define VERSION "$Revision: 1.3 $"

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
 * Functions to handle complex numbers and lists of them.
 */

typedef struct {
    double r, i;
} Complex;

const Complex czero = { 0.0, 0.0 };
const Complex cone = { 1.0, 0.0 };

Complex cadd(Complex a, Complex b) {
    Complex ret;
    ret.r = a.r + b.r;
    ret.i = a.i + b.i;
    return ret;
}

Complex csub(Complex a, Complex b) {
    Complex ret;
    ret.r = a.r - b.r;
    ret.i = a.i - b.i;
    return ret;
}

Complex cmul(Complex a, Complex b) {
    Complex ret;
    ret.r = a.r * b.r - a.i * b.i;
    ret.i = a.i * b.r + a.r * b.i;
    return ret;
}

Complex cdiv(Complex a, Complex b) {
    Complex ret;
    double divisor = b.r * b.r + b.i * b.i;
    ret.r = (a.r * b.r + a.i * b.i) / divisor;
    ret.i = (a.i * b.r - a.r * b.i) / divisor;
    return ret;
}

double cdistsquared(Complex a, Complex b) {
    double dr, di;
    dr = a.r - b.r;
    di = a.i - b.i;
    return dr * dr + di * di;
}

typedef struct {
    Complex *list;
    int n, size;
} ComplexList;

void zero_list(ComplexList *l) {
    l->list = NULL;
    l->n = l->size = 0;
}

void add_list(ComplexList *l, Complex z) {
    if (l->n >= l->size) {
        l->size = l->n + 16;
        l->list = realloc(l->list, l->size * sizeof(Complex));
    }
    l->list[l->n++] = z;
}

/* ----------------------------------------------------------------------
 * Routines for handling lists of colours.
 */
struct RGB {
    double r, g, b;
};

struct Colours {
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
    ret->ncolours = 0;
    ret->colours = NULL;
    nc = csize = 0;
    clist = NULL;

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
static struct RGB colfind(struct Colours *cols, int i) {
    int n;
    n = i % cols->ncolours;
    if (n < 0) n += cols->ncolours;
    return cols->colours[n];
}

/* ----------------------------------------------------------------------
 * The plot function.
 */

struct Params {
    char *outfile;
    double x0, x1, y0, y1;
    int width, height;
    ComplexList roots;
    struct Colours *colours;
    int nfade, limit;
    double minfade;                    /* minimum colour intensity for fade */
    int cyclic;                        /* do colours cycle, or asymptote? */
    int blur;                          /* are iteration boundaries blurred? */
};

static int toint(double d) {
    int i = (int)d;
    if (i <= 0) {
        i = (int)(d + -i + 1) - (-i + 1);
    }
    return i;
}

int plot(struct Params params) {
    int i, j, k;
    Complex z, w, d, prevz;
    struct Bitmap bm;
    int icount, root;
    double iflt, fade;
    const double tolerance = 1e-10;

    bmpinit(&bm, params.outfile, params.width, params.height);

    for (i = 0; i < params.height; i++) {
        for (j = 0; j < params.width; j++) {
            z.i = params.y0 + (params.y1-params.y0) * i / params.height;
            z.r = params.x0 + (params.x1-params.x0) * j / params.width;
            w = z;
            prevz = z;

            /*
             * For each point, begin an iteration at z.
             */
            icount = 0;
            root = -1;
            while (1) {
                /*
                 * Test proximity to each root.
                 */
                for (k = 0; k < params.roots.n; k++) {
                    if (cdistsquared(z, params.roots.list[k]) < tolerance) {
                        root = k;
                        break;
                    }
                }
                if (root >= 0)
                    break;
                prevz = z;

                /*
                 * Newton-Raphson on a polynomial is very easy.
                 * We're computing
                 * 
                 *    z <- z - f(z) / f'(z)
                 * 
                 * But when f(z) is the product of a large number
                 * of small fi(z), f(z) and f'(z) are intimately
                 * related. Consider:
                 * 
                 *    f(z) = product fi(z)
                 *              i
                 * 
                 * and so, by the product rule,
                 * 
                 * d/dz product fi(z) = sum (fi'(z) . product fj(z))
                 *         i             i            j =/= i
                 * 
                 *                    = sum (fi'(z)/fi(z) . product fj(z))
                 *                       i                     j
                 * 
                 *                    = sum (fi'(z)/fi(z) . f(z))
                 *                       i
                 * 
                 * => f'(z) = f(z) . sum ( fi'(z) / fi(z) )
                 *                    i
                 * 
                 * Now each fi(z) will be (z-a), a single root. So
                 * 
                 *    fi(z) = z-a
                 * => fi'(z) = 1
                 * => fi'(z)/fi(z) = 1/(z-a)
                 * 
                 * Hence, our complete formula says that
                 * 
                 *    f'(z)/f(z) =  sum  1/(z-a)
                 *                 roots
                 * 
                 * So we compute that, and then subtract its
                 * reciprocal from z, and we're done!
                 */
                d = czero;
                for (k = 0; k < params.roots.n; k++)
                    d = cadd(d, cdiv(cone, csub(z, params.roots.list[k])));
                z = csub(z, cdiv(cone, d));
                icount++;
                if (icount > params.limit) {
                    printf("problem point at %g + %g i\n", w.r, w.i);
                    root = -1;
                    break;
                }
            }

            if (params.blur) {
                /*
                 * Adjust the integer iteration count by an ad-hoc
                 * measure of the `fractional iteration count'. We
                 * guess at this by calculating how far between the
                 * one value of z and the next the tolerance level
                 * appeared: if one value of z was only just
                 * outside the tolerance level and the next was way
                 * inside it, we add virtually zero to the
                 * iteration count, but if one value was miles
                 * outside the tolerance level and the next was
                 * only just inside it we add almost 1.
                 * 
                 * This is done on a logarithmic scale, because
                 * tests show that works much better.
                 */
                double dist0 = cdistsquared(prevz, params.roots.list[k]);
                double dist1 = cdistsquared(z, params.roots.list[k]);
                double logt, log0, log1, proportion;
                if (dist1 < tolerance && dist0 > tolerance) {
                    logt = log(tolerance);
                    log0 = log(dist0);
                    log1 = log(dist1);
                    proportion = (logt - log0) / (log1 - log0);
                } else
                    proportion = 0;
                iflt = icount + proportion;
            } else {
                iflt = icount;
            }

            if (params.cyclic) {
                fade = 1.0 - fmod(iflt, params.nfade) / ((double)params.nfade);
            } else {
                fade = pow(1.0 - 1.0 / params.nfade, iflt);
            }
            fade = params.minfade + (1.0 - params.minfade) * fade;

            /*
             * 
             */

            /*
             * Now colour according to icount and root.
             */
            {
                struct RGB c;
                c = colfind(params.colours, root);
                fade *= 256.0;
                if (fade > 255.0)
                    fade = 255.0;
                bmppixel(&bm, toint(c.r*fade),
                         toint(c.g*fade), toint(c.b*fade));
            }
        }
        bmpendrow(&bm);
    }
    bmpclose(&bm);

    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

/*
 * Parse a complex number. A complex number fits in a single
 * argument word, and satisfies the following grammar:
 * 
 * complex = firstterm | complex term
 * firstterm = simpleterm | term
 * term = sign simpleterm
 * simpleterm = float | float i | i
 * 
 * where `sign' is either + or -, `i' is either `i' or `j', and
 * `float' is any string recognised as a float by strtod.
 */
int parsecomplex(char const *string, Complex *z, char *name) {
    char const *orig = string;

    z->r = z->i = 0.0;

    while (*string) {
        int sign = +1;
        double d;
        char *endp;

        if (*string == '+' || *string == '-')
            sign = (*string == '-' ? -1 : +1), string++;
        if (*string == 'i' || *string == 'j') {
            d = 1;
        } else {
            d = strtod(string, &endp);
            if (!endp || endp == string) {
                fprintf(stderr, "newton: unable to parse %s `%s'\n",
                        name, orig);
                return 0;
            }
            string = endp;
        }
        if (*string == 'i' || *string == 'j') {
            z->i += sign * d;
            string++;
        } else {
            z->r += sign * d;
        }

        if (*string && !(*string == '+' || *string == '-')) {
            fprintf(stderr, "newton: unable to parse %s `%s'\n", name, orig);
            return 0;
        }
    }
    return 1;
}

int parseint(char const *string, int *ret, char const *name) {
    int i;
    i = strspn(string, "0123456789");
    if (i > 0) {
	*ret = atoi(string);
	string += i;
    } else
	goto parsefail;
    if (*string)
	goto parsefail;
    return 1;
    parsefail:
    fprintf(stderr, "newton: unable to parse %s `%s'\n", name, string);
    return 0;
}

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
    fprintf(stderr, "newton: unable to parse %s `%s'\n", name, string);
    return 0;
}

int parseflt(char const *string, double *d, char const *name) {
    char *endp;
    *d = strtod(string, &endp);
    if (endp && *endp) {
	fprintf(stderr, "newton: unable to parse %s `%s'\n", name, string);
	return 0;
    } else
	return 1;
}

int parsebool(char const *string, int *d, char const *name) {
    if (!strcmp(string, "yes") ||
        !strcmp(string, "true") ||
        !strcmp(string, "verily")) {
        *d = 1;
        return 1;
    } else if (!strcmp(string, "no") ||
               !strcmp(string, "false") ||
               !strcmp(string, "nowise")) {
        *d = 0;
        return 1;
    }
    fprintf(stderr, "newton: unable to parse %s `%s'\n", name, string);
    return 0;
}

int main(int ac, char **av) {
    char const *usagemsg[] = {
	"usage: newton [options] [roots]",
	"       -o, --output file.bmp   output bitmap name",
	"       -s, --size NNNxNNN      output bitmap size",
	"       -x, --xrange NNN        mathematical x range",
	"       -y, --yrange NNN        mathematical y range",
	"       -X, --xcentre NNN       mathematical x centre point",
	"       -Y, --ycentre NNN       mathematical y centre point",
	"       -S, --scale NNN         image scale (ie pixel spacing)",
	"       -l, --limit 256         maximum limit on iterations",
	"       -f, --fade 16           how many shades of each colour",
	"       -F, --minfade 0.4       dimmest shade of each colour to use",
	"       -C, --cyclic yes        allocate colours cyclically",
	"       -B, --blur no           blur out iteration boundaries",
	"       -c, --colours 1,0,0:0,1,0  colours for roots",
	"       -v, --verbose           report details of what is done",
	" - if only one of -x and -y specified, will compute the other so",
	"   as to preserve the aspect ratio",
    };
    struct Params par;
    ComplexList roots;
    int usage = FALSE;
    int verbose = FALSE;
    char *outfile = NULL;
    int imagex = 0, imagey = 0;
    double xcentre, ycentre, xrange, yrange, scale, minfade;
    int gotxcentre, gotycentre, gotxrange, gotyrange, gotscale, gotminfade;
    int cyclic = -1, blur = -1;
    struct Colours *colours = NULL;
    int done_args = FALSE;
    int fade = -1, limit = -1;

    zero_list(&roots);
    gotxcentre = gotycentre = gotxrange =
	gotyrange = gotscale = FALSE;

    if (ac < 2) {
	usage = TRUE;
    } else while (--ac) {
	char *arg = *++av;

	if (arg[0] != '-' || done_args) {
            Complex z;
            if (!parsecomplex(arg, &z, "root"))
                return EXIT_FAILURE;
            add_list(&roots, z);
	} else if (!strcmp(arg, "--")) {
            done_args = TRUE;
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
		    {"--scale", 'S'},
		    {"--limit", 'l'},
		    {"--fade", 'f'},
		    {"--minfade", 'F'},
		    {"--colours", 'c'},
		    {"--cyclic", 'C'},
		    {"--blur", 'B'},
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
		    fprintf(stderr, "newton: unknown long option `%.*s'\n",
			    strcspn(arg, "="), arg);
		    return EXIT_FAILURE;
		}
	    }
	    switch (c) {
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
		    printf("newton version %.*s\n", i, p);
		}
		return EXIT_SUCCESS;
	      default:		       /* other options require an arg */
		if (!*val) {
		    if (!--ac) {
			fprintf(stderr,
				"newton: option `%s' requires an argument\n",
				arg);
			return EXIT_FAILURE;
		    }
		    val = *++av;
		}
		switch (c) {
		  case 'o':	       /* --output */
		    outfile = val;
		    break;
                  case 'l':            /* --limit */
		    if (!parseint(val, &limit, "limit"))
			return EXIT_FAILURE;
		    break;
                  case 'f':            /* --fade */
		    if (!parseint(val, &fade, "fade parameter"))
			return EXIT_FAILURE;
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
		  case 'F':	       /* --minfade */
		    if (!parseflt(val, &minfade, "minimum fade value"))
			return EXIT_FAILURE;
		    gotminfade = TRUE;
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
		  case 'S':	       /* --scale */
		    if (!parseflt(val, &scale, "scale"))
			return EXIT_FAILURE;
		    gotscale = TRUE;
		    break;
                  case 'C':            /* --cyclic */
                    if (!parsebool(val, &cyclic, "cyclic setting"))
                        return EXIT_FAILURE;
                    break;
                  case 'B':            /* --blur */
                    if (!parsebool(val, &blur, "blur setting"))
                        return EXIT_FAILURE;
                    break;
		  case 'c':	       /* --colours */
		    colours = colread(val);
		    if (!colours) {
			fprintf(stderr, "newton: "
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

	/* If no output file, complain. */
	if (!outfile) {
	    fprintf(stderr, "newton: no output file specified: "
		    "use something like `-o file.bmp'\n");
	    return EXIT_FAILURE;
	} else
	    par.outfile = outfile;

	/* If no roots, complain. */
	if (!roots.n) {
	    fprintf(stderr, "newton: no roots specified: "
		    "use something like `1 1+i -1 -i-1'\n");
	    return EXIT_FAILURE;
	} else
	    par.roots = roots;

	/* If no colours, default to a standard string. */
	if (!colours)
            /* assume colread will succeed */
	    par.colours = colread("1,0,0:1,1,0:0,1,0:0,0,1:1,0,1:0,1,1");
	else
	    par.colours = colours;

        /*
         * If a scale was specified, use it to deduce xrange and
         * yrange from imagex and imagey, or vice versa.
         */
        if (gotscale) {
            if (imagex && !gotxrange) xrange = imagex * scale;
            else if (gotxrange && !imagex) imagex = xrange / scale;

            if (imagey && !gotyrange) yrange = imagey * scale;
            else if (gotyrange && !imagey) imagey = yrange / scale;
        }

	/*
	 * If precisely one explicit aspect ratio specified, use it
	 * to fill in blanks in other sizes.
	 */
        aspect = 0;
        if (imagex && imagey) {
            aspect = (double)imagex / imagey;
        }
        if (gotxrange && gotyrange) {
            double newaspect = xrange / yrange;
            if (aspect != 0 && newaspect != aspect)
                aspect = -1;
            else
                aspect = newaspect;
        }

        if (aspect > 0) {
            if (imagex && !imagey) imagey = imagex / aspect;
            if (!imagex && imagey) imagex = imagey * aspect;
            if (gotxrange && !gotyrange)
                yrange = xrange / aspect, gotyrange = TRUE;
            if (!gotxrange && gotyrange)
                xrange = yrange * aspect, gotxrange = TRUE;
        }

	/*
	 * Now complain if no output image size was specified.
	 */
	if (!imagex || !imagey) {
	    fprintf(stderr, "newton: no output size specified: "
		    "use something like `-s 400x400'");
	    return EXIT_FAILURE;
	} else {
	    par.width = imagex;
	    par.height = imagey;
	}

	/*
	 * Also complain if no input mathematical extent specified.
	 */
	if (!gotxrange || !gotyrange) {
	    fprintf(stderr, "newton: no image extent specified: "
		    "use something like `-x 2 -y 2'\n");
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
	 * If we're in verbose mode, regurgitate the final
	 * parameters.
	 */
	if (verbose) {
            int i;
            printf("Output file `%s', %d x %d\n",
                   par.outfile, par.width, par.height);
            printf("Mathematical extent [%g,%g] x [%g,%g]\n",
                   par.x0, par.x1, par.y0, par.y1);
            for (i = 0; i < roots.n; i++)
                printf("Root at %g + %g i\n", roots.list[i].r,
                       roots.list[i].i);
	}

        /*
         * If no fade parameter given, default to 16.
         */
        par.nfade = fade > 0 ? fade : 16;

        /*
         * If no limit given, default to 256.
         */
        par.limit = limit > 0 ? limit : 256;

        /*
         * If no minfade given, default to 0.4.
         */
        par.minfade = gotminfade ? minfade : 0.4;

        /*
         * cyclic and blur default to true and false respectively.
         */
        par.cyclic = cyclic >= 0 ? cyclic : 1;
        par.blur = blur >= 0 ? blur : 0;

	i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

        colfree(par.colours);
        return i;
    }
}
