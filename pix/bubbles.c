/* bubbles.c - draw pretty patterns of bubbles
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "misc.h"

#include "cmdline.h"

#include "bmpwrite.h"

#define VERSION "$Revision$"

int randomupto(int n);

/* ----------------------------------------------------------------------
 * Miscellaneous useful functions.
 */

/* Integer square root. */
int squarert(int n) {
    int d, a, b, di;

    d = n;
    a = 0;
    b = 1 << 30;
    do {
        a >>= 1;
        di = 2*a + b;
        if (di <= d) {
            d -= di;
            a += b;
        }
        b >>= 2;
    } while (b);

    return a;
}

/* Random numbers. */
#define RANDBIG_MAX 0xFFFFFFFFUL
#define N1 55
#define N2 31
static unsigned randpool[N1];
static int pos;
void initrandbig(unsigned seed) {
    int i, j;
    unsigned seed0, seed1, out;

    seed0 = seed1 = 0;
    for (j = 0; j < 16; j++) {
	seed0 |= (seed & (1<<j));
	seed >>= 1;
	seed1 |= (seed & (1<<j));
    }

    /*
     * We seed the additive congruential pool using a simple linear
     * congruential random number generator. The good old Spectrum
     * one (a -> (a*75) % 65537, always use a-1) outputs in the
     * precise range 0..65535 and so seems quite a useful thing for
     * this.
     * 
     * Since we need 32-bit random numbers, we run two linear
     * congruential sequences in parallel and bitwise-interleave
     * the results. This is better than using successive outputs
     * from the same sequence because it allows us to accept a
     * 32-bit input seed.
     * 
     * (This is not a generally good alternative to a 32-bit LC
     * generator, because although it has 32 bits of seed it only
     * has a period of 2^16. For seeding an additive congruential
     * generator, it's not too bad, though.)
     */
    for (i = 0; i < N1; i++) {
	seed0 = (((seed0+1) * 75) % 65537) - 1;
	seed1 = (((seed1+1) * 75) % 65537) - 1;
	out = 0;
	for (j = 16; j-- ;) {
	    out |= (seed1 & (1<<j));
	    out <<= 1;
	    out |= (seed0 & (1<<j));
	}
	randpool[i] = out;
    }
    pos = 0;
}
unsigned randbig(void) {
    unsigned ret;
    ret = randpool[pos] += randpool[(pos+N2)%N1];
    pos = (pos+1)%N1;
    return ret;
}

/* Random number in range [0,n). */
int randomupto(int n) {
    unsigned divisor = RANDBIG_MAX / (unsigned)n;
    unsigned maximum = (unsigned)n * divisor;
    while (1) {
        unsigned r = randbig();
        if (r >= maximum)
            continue;
        return r / divisor;
    }
}

/* ----------------------------------------------------------------------
 * The plot function.
 */

struct Params {
    char *outfile;
    int width, height;
    int prefradius, maxradius, minradius, distance;
    int metal, trace;
};

#define set(a,b) ( (a) > (b) ? (a) = (b) : (a) )

#define THICKNESS(r) (1.0)

int plot(struct Params params) {
    int w = params.width, h = params.height;
    int r = params.prefradius, d = params.distance;
    int i,j;
    struct Bitmap *bm;

    int *pixels;                       /* store max possible radius at point */
    struct RGB { unsigned char r, g, b; } *pvals;   /* store colour at point */

    pixels = malloc(w*h*sizeof(*pixels));
    pvals = malloc(w*h*sizeof(*pvals));
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        int p = r;
        if (i < r) set(p, i);
        if (w-1-i < r) set(p, w-1-i);
        if (j < r) set(p, j);
        if (h-1-j < r) set(p, h-1-j);
        pixels[j*w+i] = p;
        pvals[j*w+i].r = pvals[j*w+i].g = pvals[j*w+i].b = 0;
    }

    while (1) {
        /* Loop over all pixels, selecting one. */
        int radius = 0, npixels = 0, x = 0, y = 0;
        int xmin, ymin, xmax, ymax;
        float r1_2, r2_2;
        float pmax;
        int R,G,B;

        for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
            int p = pixels[j*w+i];
            if (radius > p)
                continue;
            if (radius < p) {
                radius = p;
                npixels = 0;
            }
            /* We have npixels pixels so far, and we're adding a new one.
             * Choose it if and only if randomupto(npixels+1)==0. */
            npixels++;
            if (randomupto(npixels) == 0) {
                x = i; y = j;
            }
        }

        /* If there were no suitable spots to begin a bubble, leave. */
        if (radius <= params.minradius)
            break;

	if (params.trace) {
	    printf("Radius %d: chose (%d,%d) out of %d\n",
		   radius, x, y, npixels);
	}

        /* If we've managed to exceed the maximum radius, clip the bubble. */
        if (radius > params.maxradius)
            radius = params.maxradius;

        /* We now have x,y,radius. Draw a bubble. */
        xmin = x - radius; ymin = y - radius;
        xmax = x + radius; ymax = y + radius;
        if (params.metal) {
            r1_2 = radius * radius;
            for (j = ymin; j <= ymax; j++) for (i = xmin; i <= xmax; i++) {
                int dist2 = (j-y)*(j-y) + (i-x)*(i-x);
                float z1, nx, ny, nz, lx, ly, lz, ldot, vdot;
                int pv1, pval;

                if (dist2 <= r1_2)
                    z1 = sqrt(r1_2 - dist2);
                else
                    continue;          /* this pixel is outside the bubble */

		/* Normalised vector giving surface normal. */
                nx = (x-i)/(float)radius;
                ny = (y-j)/(float)radius;
                nz = -z1/radius;
                pval = 0;

		/* Vector pointing from a light source. */
                lx = 1/sqrt(3); ly = lx; lz = lx;

		/* Dot product between incoming light vector and
		 * surface normal. */
                ldot = lx * nx + ly * ny + lz * nz;

		/* Diffuse shading component. */
		pv1 = -64.0 * ldot;
		pval = pval + pv1;

		/* Vector of perfect reflection of the light source. */
		/* lx -= 2*ldot*nx; ly -= 2*ldot*ny; */ lz -= 2*ldot*nz;

		/* Dot product of viewer vector and perfect-reflection
		 * vector. (Vector to viewer is (0,0,-1).) */
		vdot = -lz;

		if (vdot >= 0) {
		    /* Phong's formula. Ad hoc roughness coefficient is 3. */
		    pv1 = 111.0 * (vdot*vdot*vdot*vdot);

		    pval = pval + pv1;
		}

                pval += 80;	       /* ambient light */

                if (pval > 255) pval = 255;

                pvals[j*w+i].r = pval;
                pvals[j*w+i].g = pval;
                pvals[j*w+i].b = pval;
            }
            
        } else {
            R = 128 + randomupto(128);
            G = 128 + randomupto(128);
            B = 128 + randomupto(128);
            /*
             * Algorithm for the appearance of a bubble. Assume the
             * bubble is composed of two spherical shells, outer
             * radius `radius', inner radius
             * `radius-THICKNESS(radius)', between which is a layer
             * of constant (small) opacity. Thus, to compute the
             * bubble colour at any given point, we calculate the
             * height of both shells above the Z plane and
             * subtract.
             */
            r1_2 = radius * radius;
            r2_2 = (radius - THICKNESS(radius)) * (radius - THICKNESS(radius));
            /*
             * Max value is the value at
             * (maxradius-THICKNESS(maxradius)).
             */
            pmax = sqrt(params.maxradius * params.maxradius -
                        (params.maxradius - THICKNESS(params.maxradius)) *
                        (params.maxradius - THICKNESS(params.maxradius)));
            for (j = ymin; j <= ymax; j++) for (i = xmin; i <= xmax; i++) {
                int dist2 = (j-y)*(j-y) + (i-x)*(i-x);
                double z1, z2 = 0;
                float pv1, pv2, pval;
                float nx, ny, nz, lx, ly, lz, ldot;
                if (dist2 <= r1_2)
                    z1 = sqrt(r1_2 - dist2);
                else
                    continue;          /* this pixel is outside the bubble */
                if (dist2 <= r2_2)
                    z2 = sqrt(r2_2 - dist2);
                pv1 = (z1-z2) / pmax;

                /* Draw a small reflection in the bubble. */
                nx = (x-i)/(float)radius; ny = (y-j)/(float)radius; nz = z1/radius;
                lz = 1; lx = 0; ly = 0;
                ldot = lx * nx + ly * ny + lz * nz;
                lx -= 2*ldot*nx; ly -= 2*ldot*ny; lz -= 2*ldot*nz;
                /* This is the vector pointing away from the bubble towards
                 * the reflected light source. Now rotate it so that we are
                 * looking at it down the z-axis _from_ the light source. */
                nx = 3*(lx+2*lz);
                ny = -4*lx+5*ly+2*lz;
                nz = sqrt(5)*(lz-2*(lx+ly));
                /* Now clip it against a rectangular window. */

                pv2 = 0;
                if (nz < 0) {
                    nx /= nz; ny /= nz;
                    if (nx >= -0.2 && nx <= 0.2 &&
                        ny >= -0.2 && ny <= 0.2)
                        pv2 = 1;
                }

                pval = (pv1 > pv2 ? pv1 : pv2);

                pvals[j*w+i].r = R * pval;
                pvals[j*w+i].g = G * pval;
                pvals[j*w+i].b = B * pval;
            }
        }

        /* And update the pixel array. */
        xmin = x - (radius+r+d); ymin = y - (radius+r+d);
        xmax = x + (radius+r+d); ymax = y + (radius+r+d);
        if (xmin < 0) xmin = 0; if (xmax >= w) xmax = w-1;
        if (ymin < 0) ymin = 0; if (ymax >= h) ymax = h-1;
        for (j = ymin; j <= ymax; j++) for (i = xmin; i <= xmax; i++) {
            int dist = squarert((j-y)*(j-y) + (i-x)*(i-x));
            if (dist < radius)
                dist = -1;
            else if (dist < radius+d)
                dist = 0;
            else
                dist -= radius+d;
            set(pixels[j*w+i], dist);
        }
    }

    bm = bmpinit(params.outfile, params.width, params.height);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            struct RGB p = pvals[j*w+i];
            bmppixel(bm, p.r, p.g, p.b);
        }
        bmpendrow(bm);
    }
    bmpclose(bm);

    free(pixels);

    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

int main(int argc, char **argv) {
    int verbose = 0;
    int metal = FALSE;
    char *outfile = NULL;
    struct Size imagesize = {0,0};
    int i;
    int radius = 0, minradius = 0, prefradius = 0, distance = 0;
    struct Params par;

    struct Cmdline options[] = {
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, &outfile, NULL},
	{1, "--size", 's', "NNNxNNN", "output bitmap size",
		"output bitmap size", parsesize, &imagesize, NULL},
	{1, "--radius", 'r', "NNN", "maximum bubble radius",
		"maximum radius", parseint, &radius, NULL},
	{1, "--minradius", 'm', "NNN", "minimum bubble radius",
		"minimum radius", parseint, &radius, NULL},
	{1, "--preferred", 'p', "NNN", "preferred distance between bubbles",
		"preferred distance", parseint, &prefradius, NULL},
	{1, "--distance", 'd', "NNN", "minimum distance between bubbles",
		"minimum distance", parseint, &distance, NULL},
	{1, "--metal", 'M', NULL, "draw metal balls instead of bubbles",
		NULL, NULL, NULL, &metal},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, NULL, &verbose},
    };

    parse_cmdline("bubbles", argc, argv, options, lenof(options));

    if (argc < 2)
	usage_message("bubbles [options]", options, lenof(options), NULL, 0);

    initrandbig(time(NULL));	       /* FIXME: configurable input seed */

    /*
     * Having read the arguments, now process them.
     */

    /* If no output file, complain. */
    if (!outfile) {
	fprintf(stderr, "bubbles: no output file specified: "
		"use something like `-o file.bmp'\n");
	return EXIT_FAILURE;
    } else
	par.outfile = outfile;

    /*
     * Now complain if no output image size was specified.
     */
    if (!imagesize.w || !imagesize.h) {
	fprintf(stderr, "bubbles: no output size specified: "
		"use something like `-s 400x400'\n");
	return EXIT_FAILURE;
    } else {
	par.width = imagesize.w;
	par.height = imagesize.h;
    }

    /*
     * If no radius, minradius or distance specified, default
     * to standard values. Minradius defaults to 0; prefradius
     * defaults to maxradius.
     */
    par.maxradius = (radius > 0 ? radius : 32);
    par.distance = (distance > 0 ? distance : 8);
    par.minradius = (minradius > 0 ? minradius : 0);
    par.prefradius = (prefradius > 0 ? prefradius : par.maxradius);

    par.metal = metal;

    if (verbose > 1)
	par.trace = 1;
    else
	par.trace = 0;

    /*
     * If we're in verbose mode, regurgitate the final
     * parameters.
     */
    if (verbose) {
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Bubble radius range %d to %d\n",
	       par.minradius, par.maxradius);
	printf("Minimum distance %d, preferred distance %d\n",
	       par.distance, par.prefradius);
	printf("Appearance: %s\n",
	       par.metal ? "metal balls" : "bubbles");
    }

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    return i;
}
