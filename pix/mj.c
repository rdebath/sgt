/* mj.c - draw Mandelbrot and Julia sets
 *
 * This program is copyright 2000,2007 Simon Tatham.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#include "misc.h"

#define VERSION "~VERSION~"

#include "cmdline.h"

#include "bmpwrite.h"

/* ----------------------------------------------------------------------
 * Functions to handle complex numbers.
 */

typedef struct {
    double r, i;
} Complex;

const Complex czero = { 0.0, 0.0 };
const Complex cone = { 1.0, 0.0 };

Complex cfromreal(double r) {
    Complex ret;
    ret.r = r;
    ret.i = 0.0;
    return ret;
}

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

/* ----------------------------------------------------------------------
 * Routines for handling lists of colours.
 */
struct Colours {
    int ncolours;
    struct RGB *colours;
    struct RGB nullcol;
};

/*
 * Read a colour list into a Colours structure.
 */
static struct Colours *colread(char const *string) {
    struct Colours *ret;
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
    if (i < 0)
        return cols->nullcol;
    n = i % cols->ncolours;
    if (n < 0) n += cols->ncolours;
    return cols->colours[n];
}

/* ----------------------------------------------------------------------
 * The plot function.
 */

struct Params {
    char *outfile;
    int outtype;
    double x0, x1, y0, y1;
    int width, height;
    int julia;
    int blur;
    int limit;
    Complex c;
    struct Colours *colours;
};

static int toint(double d) {
    int i = (int)d;
    if (i <= 0) {
        i = (int)(d + -i + 1) - (-i + 1);
    }
    return i;
}

int plot(struct Params params)
{
    int i, j;
    Complex z, c, w, prevz;
    struct Bitmap *bm;
    int icount;
    double iflt;
    const double tolerance = 4;

    bm = bmpinit(params.outfile, params.width, params.height, params.outtype);

    for (i = 0; i < params.height; i++) {
        for (j = 0; j < params.width; j++) {
            w.i = params.y0 + (params.y1-params.y0) * i / params.height;
            w.r = params.x0 + (params.x1-params.x0) * j / params.width;

            /*
             * For each point, begin an iteration at w.
             */
            if (params.julia) {
                z = w;
                c = params.c;
            } else {
                z = czero;
                c = w;
            }
            icount = 0;
            prevz = z;
            while (1) {
                if (cdistsquared(z, czero) > tolerance)
                    break;

                prevz = z;
                z = cadd(cmul(z, z), c);

                icount++;
                if (icount > params.limit) {
                    break;
                }
            }

            if (params.blur && icount < params.limit) {
                /*
                 * Adjust the integer iteration count by an ad-hoc
                 * measure of the `fractional iteration count'. We
                 * guess at this by calculating how far between the
                 * one value of z and the next the tolerance level
                 * appeared: if one value of z was only just inside
                 * the tolerance level and the next was way outside
                 * it, we add virtually zero to the iteration
                 * count, but if one value was miles inside the
                 * tolerance level and the next was only just
                 * outside it we add almost 1.
                 * 
                 * This is done on a logarithmic scale, because
                 * it seems sensible.
                 */
                double dist0, dist1;
                double logt, log0, log1, proportion;

                dist0 = cdistsquared(prevz, czero);
                dist1 = cdistsquared(z, czero);

                if (dist1 > tolerance && dist0 <= tolerance) {
                    logt = log(tolerance);
                    log0 = log(dist0);
                    log1 = log(dist1);
                    proportion = (logt - log1) / (log1 - log0);
                } else
                    proportion = 0;
                iflt = icount + proportion;
            } else {
                iflt = icount;
            }

            /*
             * Now colour according to icount.
             */
            {
                struct RGB c;
                if (icount > params.limit)
                    c = params.colours->nullcol;
                else {
                    /*
                     * Distribute the colours in the list in a
                     * quadratic fashion through the range of
                     * possible iteration counts.
                     * 
                     * (FIXME: it would be helpful here to be able
                     * to normalise the start of the colour
                     * sequence to the _smallest_ iteration count
                     * appearing in the picture, or better still to
                     * normalise so that the colour histogram was
                     * roughly right overall; but in order to do
                     * that we'd have to reorganise this routine
                     * somewhat in order to do the entire plot
                     * before generating any of the output.)
                     */
                    double cf = ((params.colours->ncolours-1)
                                 * sqrt(iflt / params.limit));
                    int c0 = (int)floor(cf), c1 = (int)ceil(cf);
                    double cp = cf - c0;
                    struct RGB cc0, cc1;

                    assert(c0 >= 0 && c0 < params.colours->ncolours);
                    assert(c1 >= 0 && c1 < params.colours->ncolours);

                    cc0 = colfind(params.colours, c0);
                    cc1 = colfind(params.colours, c1);

                    c.r = cc0.r * (1-cp) + cc1.r * cp;
                    c.g = cc0.g * (1-cp) + cc1.g * cp;
                    c.b = cc0.b * (1-cp) + cc1.b * cp;
                }
                bmppixel(bm, toint(c.r*255), toint(c.g*255), toint(c.b*255));
            }
        }
        bmpendrow(bm);
    }
    bmpclose(bm);

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
int parsecomplex(char const *string, Complex *z) {
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
            return 0;
        }
    }
    return 1;
}
int parsecomplexv(char *string, void *vret)
{
    return parsecomplex(string, (Complex *)vret);
}

int parsecols(char *string, void *vret) {
    struct Colours **ret = (struct Colours **)vret;
    *ret = colread(string);
    if (!*ret)
	return 0;
    return 1;
}

int main(int argc, char **argv) {
    struct Params par;
    int i;
    double aspect;

    struct optdata {
	int verbose;
	char *outfile;
	int outppm;
	struct Size imagesize;
	double xcentre, ycentre, xrange, yrange, scale;
	int gotxcentre, gotycentre, gotxrange, gotyrange, gotscale;
	int blur;
	struct Colours *colours;
        struct RGB nullcol;
	int limit;
        int gotc;
        Complex c;
    } optdata = {
	FALSE,
	NULL,
	FALSE,
	{0,0},
	0, 0, 0, 0, 0,
	FALSE, FALSE, FALSE, FALSE, FALSE,
	-1,
	NULL,
        {0,0,0},
	-1,
	FALSE,
	{0,0},
    };

    static const struct Cmdline options[] = {
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, offsetof(struct optdata, outfile), -1},
	{1, "--ppm", 0, NULL, "output PPM rather than BMP",
		NULL, NULL, -1, offsetof(struct optdata, outppm)},
	{1, "--size", 's', "NNNxNNN", "output bitmap size",
		"output bitmap size", parsesize, offsetof(struct optdata, imagesize), -1},
	{1, "--xrange", 'x', "NNN", "mathematical x range",
		"x range", parseflt, offsetof(struct optdata, xrange), offsetof(struct optdata, gotxrange)},
	{1, "--yrange", 'y', "NNN", "mathematical y range",
		"y range", parseflt, offsetof(struct optdata, yrange), offsetof(struct optdata, gotyrange)},
	{2, "--xcentre\0--xcenter", 'X', "NNN", "mathematical x centre point",
		"x centre", parseflt, offsetof(struct optdata, xcentre), offsetof(struct optdata, gotxcentre)},
	{2, "--ycentre\0--ycenter", 'Y', "NNN", "mathematical y centre point",
		"y centre", parseflt, offsetof(struct optdata, ycentre), offsetof(struct optdata, gotycentre)},
	{1, "--scale", 'S', "NNN", "image scale (ie pixel spacing)",
		"image scale", parseflt, offsetof(struct optdata, scale), offsetof(struct optdata, gotscale)},
	{1, "--limit", 'l', "256", "maximum limit on iterations",
		"limit", parseint, offsetof(struct optdata, limit), -1},
	{1, "--blur", 'B', "no", "blur out iteration boundaries",
		"blur setting", parsebool, offsetof(struct optdata, blur), -1},
	{2, "--colours\0--colors", 'c', "0,0,1:0,1,1:1,1,1", "colours to spread across iteration range",
		"colour specification", parsecols, offsetof(struct optdata, colours), -1},
	{2, "--nullcolour\0--nullcolor", 'n', "0,0,0", "null colour for non-converging points",
		"colour specification", parsecol, offsetof(struct optdata, nullcol), -1},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, -1, offsetof(struct optdata, verbose)},
	{0, NULL, 0, "<c>", "constant for plotting a Julia set",
		"complex number", parsecomplexv, offsetof(struct optdata, c), offsetof(struct optdata, gotc)},
    };

    char *usageextra[] = {
	" - if only one of -x and -y specified, will compute the other so",
	    "   as to preserve the aspect ratio",
    };

    parse_cmdline("mj", argc, argv, options, lenof(options), &optdata);

    if (argc < 2)
	usage_message("mj [options] [<c>]",
		      options, lenof(options),
		      usageextra, lenof(usageextra));

    /*
     * Having read the arguments, now process them.
     */

    /* If no output file, complain. */
    if (!optdata.outfile) {
	fprintf(stderr, "mj: no output file specified: "
		"use something like `-o file.bmp'\n");
	return EXIT_FAILURE;
    } else
	par.outfile = optdata.outfile;
    par.outtype = (optdata.outppm ? PPM : BMP);

    if (optdata.gotc) {
        par.c = optdata.c;
        par.julia = TRUE;
    } else
        par.julia = FALSE;

    /* If no colours, default to a standard string. */
    if (!optdata.colours)
	/* assume colread will succeed */
	par.colours = colread("0,0,1:0,1,1:1,1,1");
    else
	par.colours = optdata.colours;
    par.colours->nullcol = optdata.nullcol;

    /*
     * If a scale was specified, use it to deduce xrange and
     * yrange from optdata.imagesize.w and optdata.imagesize.h, or vice versa.
     */
    if (optdata.gotscale) {
	if (optdata.imagesize.w && !optdata.gotxrange) {
            optdata.xrange = optdata.imagesize.w * optdata.scale;
            optdata.gotxrange = TRUE;
        } else if (optdata.gotxrange && !optdata.imagesize.w) {
            optdata.imagesize.w = optdata.xrange / optdata.scale;
        }

	if (optdata.imagesize.h && !optdata.gotyrange) {
            optdata.yrange = optdata.imagesize.h * optdata.scale;
            optdata.gotyrange = TRUE;
        } else if (optdata.gotyrange && !optdata.imagesize.h) {
            optdata.imagesize.h = optdata.yrange / optdata.scale;
        }
    }

    /*
     * If precisely one explicit aspect ratio specified, use it
     * to fill in blanks in other sizes.
     */
    aspect = 0;
    if (optdata.imagesize.w && optdata.imagesize.h) {
	aspect = (double)optdata.imagesize.w / optdata.imagesize.h;
    }
    if (optdata.gotxrange && optdata.gotyrange) {
	double newaspect = optdata.xrange / optdata.yrange;
	if (aspect != 0 && newaspect != aspect)
	    aspect = -1;
	else
	    aspect = newaspect;
    }

    if (aspect > 0) {
	if (optdata.imagesize.w && !optdata.imagesize.h) optdata.imagesize.h = optdata.imagesize.w / aspect;
	if (!optdata.imagesize.w && optdata.imagesize.h) optdata.imagesize.w = optdata.imagesize.h * aspect;
	if (optdata.gotxrange && !optdata.gotyrange)
	    optdata.yrange = optdata.xrange / aspect, optdata.gotyrange = TRUE;
	if (!optdata.gotxrange && optdata.gotyrange)
	    optdata.xrange = optdata.yrange * aspect, optdata.gotxrange = TRUE;
    }

    /*
     * Now complain if no output image size was specified.
     */
    if (!optdata.imagesize.w || !optdata.imagesize.h) {
	fprintf(stderr, "mj: no output size specified: "
		"use something like `-s 400x400'\n");
	return EXIT_FAILURE;
    } else {
	par.width = optdata.imagesize.w;
	par.height = optdata.imagesize.h;
    }

    /*
     * Also complain if no input mathematical extent specified.
     */
    if (!optdata.gotxrange || !optdata.gotyrange) {
	fprintf(stderr, "mj: no image extent specified: "
		"use something like `-x 2 -y 2'\n");
	return EXIT_FAILURE;
    } else {
	int ysign = (optdata.outppm ? -1 : +1);
	if (!optdata.gotxcentre) optdata.xcentre = 0.0;
	if (!optdata.gotycentre) optdata.ycentre = 0.0;
	par.x0 = optdata.xcentre - optdata.xrange;
	par.x1 = optdata.xcentre + optdata.xrange;
	par.y0 = optdata.ycentre - ysign * optdata.yrange;
	par.y1 = optdata.ycentre + ysign * optdata.yrange;
    }

    /*
     * If we're in verbose mode, regurgitate the final
     * parameters.
     */
    if (optdata.verbose) {
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Mathematical extent [%g,%g] x [%g,%g]\n",
	       par.x0, par.x1, par.y0, par.y1);
        if (par.julia) {
            printf("Draw Julia set with c = %g + %g i\n", par.c.r, par.c.i);
        } else { 
            printf("Draw Mandelbrot set\n");
        }
    }

    /*
     * If no limit given, default to 256.
     */
    par.limit = optdata.limit > 0 ? optdata.limit : 256;

    /*
     * blur defaults to false.
     */
    par.blur = optdata.blur >= 0 ? optdata.blur : 0;

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    colfree(par.colours);
    return i;
}
