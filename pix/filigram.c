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

/* ----------------------------------------------------------------------
 * Function prototypes and structure type predeclarations.
 */

struct Bitmap;
static void fput32(unsigned long val, FILE *fp);
static void fput16(unsigned val, FILE *fp);
static void bmpinit(struct Bitmap *bm, char *filename,
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

static void bmpinit(struct Bitmap *bm, char *filename,
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
    FILE *fp = fopen("output.bmp", "wb");
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
    double iscale, oscale;
    int fading;
    char const *poly, *colours;
};

static int toint(double d) {
    int i = (int)d;
    if (i <= 0) {
        i = (int)(d + -i + 1) - (-i + 1);
    }
    return i;
}

static int plot(struct Params params) {
    struct Poly *f, *dfdx, *dfdy;
    struct Colours *colours;
    struct Bitmap bm;

    double xstep, ystep;
    double x, xfrac, y, yfrac;
    double dzdx, dzdy, dscale;
    double z, xfade, yfade, fade;
    struct RGB c;
    int i, j, xg, yg;

    f = polyread(params.poly);
    if (!f) {
        fprintf(stderr, "unable to parse polynomial `%s'\n", params.poly);
        return 0;
    }
    dfdx = polypdiff(f, 1);
    dfdy = polypdiff(f, 0);

    colours = colread(params.colours);
    if (!colours) {
        fprintf(stderr, "unable to parse colour specification `%s'\n",
                params.colours);
        return 0;
    }

    bmpinit(&bm, "output.bmp", params.width, params.height);

    xstep = (params.x1 - params.x0) / params.width;
    ystep = (params.y1 - params.y0) / params.height;
    dscale = params.iscale * params.oscale;
    for (i = 0; i < params.height; i++) {
        y = params.y0 + ystep * i;
        yfrac = y / params.iscale; yfrac -= toint(yfrac);

        for (j = 0; j < params.width; j++) {
            x = params.x0 + xstep * j;
            xfrac = x / params.iscale; xfrac -= toint(xfrac);

            dzdx = polyeval(dfdx, x, y) * dscale;
            dzdy = polyeval(dfdy, x, y) * dscale;
            xg = toint(dzdx + 0.5); dzdx -= xg;
            yg = toint(dzdy + 0.5); dzdy -= yg;

            z = polyeval(f, x, y) * params.oscale;
            z -= xg * xfrac;
            z -= yg * yfrac;
            z -= toint(z);

            xfade = dzdx; if (xfade < 0) xfade = -xfade;
            yfade = dzdy; if (yfade < 0) yfade = -yfade;
            fade = 1.0 - (xfade < yfade ? yfade : xfade) * 2;

	    c = colfind(colours, xg, yg);

	    if (params.fading) z *= fade;
	    z *= 256.0;

            bmppixel(&bm, toint(c.r*z), toint(c.g*z), toint(c.b*z));
        }
        bmpendrow(&bm);
    }

    bmpclose(&bm);
    colfree(colours);
    polyfree(dfdy);
    polyfree(dfdx);
    polyfree(f);
    return 1;
}

#define PURPLEGREEN

static const double colours[][3] = {
#ifdef PURPLEGREEN
    {0.7, 0.0, 1.0},
    {0.0, 0.63, 0.0},
    {0.0, 0.63, 0.0},
    {0.7, 0.0, 1.0},
#endif
#ifdef JEWELS
    {157.0/256.0,  4.0/256.0,  4.0/256.0},
    {122.0/256.0,  7.0/256.0,143.0/256.0},
    {210.0/256.0,165.0/256.0,  7.0/256.0},
    {  7.0/256.0,174.0/256.0, 11.0/256.0},
#endif
};

int main(void) {
    struct Params p = {
	640, 512,
	    -10.0, +10.0, -8.0, +8.0,
	    1/32.0, 1/64.0,
	    1,
	    "4x4-17x2y2+4y4",
	    /* "0.7,0,1:0,0.63,0" */
            /* "2!0.61,0.0,0.0:0.48,0.0,0.56:0.82,0.64,0.0:0.0,0.7,0.0" */
            "1,0,0:1,1,0:0,1,0:0,1,1:0,0,1:1,0,1"
    };
#ifdef CIRCLES
    struct Params p = {
        640, 512,
            -10.0, +10.0, -8.0, +8.0,
            1/32.0, 4.0,
            0,
            "x2+y2"
            "1,0,0:1,1,0:0,1,0:0,1,1:0,0,1:1,0,1"
    };
#endif
#ifndef ORIGINAL
    struct Params p = {
        640, 512,
            -10.0, +10.0, -8.0, +8.0,
            1/32.0, 1/4.0,
            0,
            "x2y2"
            "1,0,0:1,1,0:0,1,0:0,1,1:0,0,1:1,0,1"
    };
#endif
    /*
     * FIXME: process command line options to set up parameters.
     *
     *  - output file name
     *
     *  - output image size
     *
     *  - mathematical-coordinate image bounds
     *
     *  - EITHER input scale OR base-image size
     *    (special case: `--base', default, means this _is_ the base image)
     *
     *  - preserving of aspect ratio on any of the size
     *    specifications once an aspect has been set by another
     *
     *  - output scale
     *
     *  - whether to fade
     *
     *  - some means of specifying colours
     *
     *  - polynomial
     */
    return plot(p) ? EXIT_SUCCESS : EXIT_FAILURE;
}
