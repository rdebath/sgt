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
#include <string.h>
#include <stddef.h>
#include <math.h>

#include "misc.h"

#include "cmdline.h"

#include "bmpwrite.h"

#define VERSION "$Revision$"

/* ----------------------------------------------------------------------
 * Functions to handle complex numbers and lists of them.
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

typedef struct {
    Complex *list;
    Complex *factor;
    int n, size;
} RootList;

void zero_list(RootList *l) {
    l->list = l->factor = NULL;
    l->n = l->size = 0;
}

void add_list(RootList *l, Complex z, Complex factor) {
    if (l->n >= l->size) {
        l->size = l->n + 16;
        l->list = realloc(l->list, l->size * sizeof(Complex));
        l->factor = realloc(l->factor, l->size * sizeof(Complex));
    }
    l->list[l->n] = z;
    l->factor[l->n] = factor;
    l->n++;
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
    RootList roots;
    struct Colours *colours;
    int nfade, limit;
    double overpower;
    double minfade;                    /* minimum colour intensity for fade */
    int cyclic;                        /* do colours cycle, or asymptote? */
    int blur;                          /* are iteration boundaries blurred? */
    int preview;
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
    Complex z, d, prevz;
    Complex overpower;
    struct Bitmap *bm;
    int icount, root;
    double iflt, fade;
    double tolerance;

    /*
     * Raising the whole function to a power is equivalent to
     * raising each individual term to that power. It's a bit
     * hacky, but by far the easiest way to do this is to modify
     * the factors list at the start.
     */
    overpower = cfromreal(params.overpower);
    for (k = 0; k < params.roots.n; k++)
        params.roots.factor[k] = cmul(overpower, params.roots.factor[k]);

    bm = bmpinit(params.outfile, params.width, params.height, params.outtype);

    if (params.preview) {
	tolerance = 8*8 *
	    fabs((params.y1-params.y0) * (params.x1-params.x0)) /
	    (params.height * params.width);
    } else {
	tolerance = 1e-10;
    }

    for (i = 0; i < params.height; i++) {
        for (j = 0; j < params.width; j++) {
            z.i = params.y0 + (params.y1-params.y0) * i / params.height;
            z.r = params.x0 + (params.x1-params.x0) * j / params.width;
            prevz = z;

	    if (params.preview) {
		struct RGB c;

		/*
		 * Special mode for previewing the movement of the
		 * roots in an animation. In this mode we simply
		 * draw a small and crude circle around each root.
		 */
		root = -1;
		for (k = 0; k < params.roots.n; k++) {
		    if (cdistsquared(z, params.roots.list[k]) < tolerance)
			root = k;
		}
		if (root >= 0)
		    c = colfind(params.colours, root);
		else
		    c.r = c.g = c.b = 0;
                bmppixel(bm, toint(c.r*255.0), toint(c.g*255.0),
			 toint(c.b*255.0));
		continue;
	    }

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
                 * Newton-Raphson on a function of this type is
                 * very easy. We're computing
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
                 * Now each fi(z) will be of the form (z-a)^alpha.
                 * So
                 * 
                 *    fi(z) = (z-a)^alpha
                 * => fi'(z) = alpha*(z-a)^(alpha-1)
                 * => fi'(z)/fi(z) = alpha/(z-a)
                 * 
                 * Hence, our complete formula says that
                 * 
                 *    f'(z)/f(z) =  sum  alpha_i/(z-a_i)
                 *                   i
                 * 
                 * So we compute that, and then subtract its
                 * reciprocal from z, and we're done!
                 */
                d = czero;
                for (k = 0; k < params.roots.n; k++)
                    d = cadd(d, cdiv(params.roots.factor[k],
				     csub(z, params.roots.list[k])));
                if (d.r == 0 && d.i == 0) {
                    root = -1;
                    break;
                }
                z = csub(z, cdiv(cone, d));
                icount++;
                if (icount > params.limit) {
                    root = -1;
                    break;
                }
            }

            if (params.blur && root >= 0) {
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
                double dist0, dist1;
                double logt, log0, log1, proportion;

                dist0 = cdistsquared(prevz, params.roots.list[root]);
                dist1 = cdistsquared(z, params.roots.list[root]);

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
             * Now colour according to icount and root.
             */
            {
                struct RGB c;
                c = colfind(params.colours, root);
                fade *= 256.0;
                if (fade > 255.0)
                    fade = 255.0;
                bmppixel(bm, toint(c.r*fade),
                         toint(c.g*fade), toint(c.b*fade));
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

int addroot(char *string, void *vret)
{
    RootList *list = (RootList *)vret;
    Complex z, factor;
    char *s2;

    s2 = strchr(string, '/');
    if (s2)
        *s2++ = '\0';
    else
        s2 = "1";

    if (!parsecomplex(string, &z))
	return 0;
    if (!parsecomplex(s2, &factor))
	return 0;
    add_list(list, z, factor);
    return 1;
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
	double xcentre, ycentre, xrange, yrange, scale, minfade, overpower;
	int gotxcentre, gotycentre, gotxrange, gotyrange, gotscale, gotminfade,
	    gotoverpower;
	int cyclic, blur;
	struct Colours *colours;
        struct RGB nullcol;
	int fade, limit;
	int preview;
	RootList roots;
    } optdata = {
	FALSE,
	NULL,
	FALSE,
	{0,0},
	0, 0, 0, 0, 0, 0, 0,
	FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
	-1, -1,
	NULL,
        {0,0,0},
	-1, -1,
	FALSE,
	{NULL,0,0},
    };

    static const struct Cmdline options[] = {
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, offsetof(struct optdata, outfile), -1},
	{1, "--ppm", 0, NULL, "output PPM rather than BMP",
		NULL, NULL, -1, offsetof(struct optdata, outppm)},
	{1, "--preview", 0, NULL, "just preview positions of roots",
		NULL, NULL, -1, offsetof(struct optdata, preview)},
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
	{1, "--fade", 'f', "16", "how many shades of each colour",
		"fade parameter", parseint, offsetof(struct optdata, fade), -1},
	{1, "--minfade", 'F', "0.4", "dimmest shade of each colour to use",
		"minimum fade value", parseflt, offsetof(struct optdata, minfade), offsetof(struct optdata, gotminfade)},
	{1, "--cyclic", 'C', "yes", "allocate colours cyclically",
		"cyclic setting", parsebool, offsetof(struct optdata, cyclic), -1},
	{1, "--blur", 'B', "no", "blur out iteration boundaries",
		"blur setting", parsebool, offsetof(struct optdata, blur), -1},
        {1, "--power", 'p', "1", "raise entire function to a power",
                "overall power", parseflt, offsetof(struct optdata, overpower), offsetof(struct optdata, gotoverpower)},
	{2, "--colours\0--colors", 'c', "1,0,0:0,1,0", "colours for roots",
		"colour specification", parsecols, offsetof(struct optdata, colours), -1},
	{2, "--nullcolour\0--nullcolor", 'n', "0,0,0", "null colour for non-converging points",
		"colour specification", parsecol, offsetof(struct optdata, nullcol), -1},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, -1, offsetof(struct optdata, verbose)},
	{0, NULL, 0, "<root>", "a complex root of the polynomial",
		"complex number", addroot, offsetof(struct optdata, roots), -1},
	{0, NULL, 0, "<root>/<factor>", "a root with convergence modifier",
		"complex number", addroot, offsetof(struct optdata, roots), -1},
    };

    char *usageextra[] = {
	" - if only one of -x and -y specified, will compute the other so",
	    "   as to preserve the aspect ratio",
    };

    zero_list(&optdata.roots);

    parse_cmdline("newton", argc, argv, options, lenof(options), &optdata);

    if (argc < 2 || optdata.roots.n == 0)
	usage_message("newton [options] <root> <root>...",
		      options, lenof(options),
		      usageextra, lenof(usageextra));

    /*
     * Having read the arguments, now process them.
     */

    /* If no output file, complain. */
    if (!optdata.outfile) {
	fprintf(stderr, "newton: no output file specified: "
		"use something like `-o file.bmp'\n");
	return EXIT_FAILURE;
    } else
	par.outfile = optdata.outfile;
    par.outtype = (optdata.outppm ? PPM : BMP);
    par.preview = optdata.preview;

    /* If no roots, complain. */
    if (!optdata.roots.n) {
	fprintf(stderr, "newton: no roots specified: "
		"use something like `1 1+i -1 -i-1'\n");
	return EXIT_FAILURE;
    } else
	par.roots = optdata.roots;

    /* If no colours, default to a standard string. */
    if (!optdata.colours)
	/* assume colread will succeed */
	par.colours = colread("1,0,0:1,1,0:0,1,0:0,0,1:1,0,1:0,1,1");
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
	fprintf(stderr, "newton: no output size specified: "
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
	fprintf(stderr, "newton: no image extent specified: "
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
	int i;
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Mathematical extent [%g,%g] x [%g,%g]\n",
	       par.x0, par.x1, par.y0, par.y1);
	for (i = 0; i < par.roots.n; i++)
	    printf("Root at %g + %g i\n", par.roots.list[i].r,
		   par.roots.list[i].i);
    }

    /*
     * If no fade parameter given, default to 16.
     */
    par.nfade = optdata.fade > 0 ? optdata.fade : 16;

    /*
     * If no limit given, default to 256.
     */
    par.limit = optdata.limit > 0 ? optdata.limit : 256;

    /*
     * If no minfade given, default to 0.4.
     */
    par.minfade = optdata.gotminfade ? optdata.minfade : 0.4;

    /*
     * If no overall power given, default to 1.
     */
    par.overpower = optdata.gotoverpower ? optdata.overpower : 1.0;

    /*
     * cyclic and blur default to true and false respectively.
     */
    par.cyclic = optdata.cyclic >= 0 ? optdata.cyclic : 1;
    par.blur = optdata.blur >= 0 ? optdata.blur : 0;

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    colfree(par.colours);
    return i;
}
