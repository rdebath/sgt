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
int randomupto(int n);

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
    unsigned j;
    for (j = 0; j < bm->padding; j++)
        putc(0, bm->fp);
}

static void bmpclose(struct Bitmap *bm) {
    fclose(bm->fp);
}

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
void initrandbig(void) {
    int i;
    srand(time(NULL));
    for (i = 0; i < N1; i++)
	randpool[i] = rand() * rand() * rand();
    pos = 0;
}
unsigned randbig(void) {
    unsigned ret;
    ret = randpool[pos] = randpool[(pos+N1-1)%N1] + randpool[(pos+N2-1)%N1];
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
    struct Bitmap bm;

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
                float nx, ny, nz, nr, lx, ly, lz, ldot;
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

    bmpinit(&bm, params.outfile, params.width, params.height);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            struct RGB p = pvals[j*w+i];
            bmppixel(&bm, p.r, p.g, p.b);
        }
        bmpendrow(&bm);
    }
    bmpclose(&bm);

    free(pixels);

    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

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
    fprintf(stderr, "bubbles: unable to parse %s `%s'\n", name, string);
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
    fprintf(stderr, "bubbles: unable to parse %s `%s'\n", name, string);
    return 0;
}

int main(int ac, char **av) {
    char const *usagemsg[] = {
	"usage: bubbles [options]",
	"       -o, --output file.bmp   output bitmap name",
	"       -s, --size NNNxNNN      output bitmap size",
        "       -r, --radius NNN        maximum bubble radius",
        "       -m, --minradius NNN     minimum bubble radius",
        "       -p, --preferred NNN     preferred distance between bubbles",
        "       -d, --distance NNN      minimum distance between bubbles",
        "       -M, --metal             draw metal balls instead of bubbles",
	"       -v, --verbose           report details of what is done",
    };
    struct Params par;
    int usage = FALSE;
    int verbose = 0;
    int metal = FALSE;
    char *outfile = NULL;
    int imagex = 0, imagey = 0;
    int i;
    int radius = 0, minradius = 0, prefradius = 0, distance = 0;

    initrandbig();

    if (ac < 2) {
	usage = TRUE;
    } else while (--ac) {
	char *arg = *++av;

	if (arg[0] != '-') {
            usage = TRUE;
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
		    {"--radius", 'r'},
		    {"--minradius", 'm'},
		    {"--preferred", 'p'},
		    {"--distance", 'd'},
		    {"--verbose", 'v'},
		    {"--help", 'h'},
		    {"--version", 'V'},
		    {"--metal", 'M'},
		};
		int i, j;
		for (i = 0; i < lenof(longopts); i++) {
		    j = strlen(longopts[i].name);
		    if (!strncmp(arg, longopts[i].name, j) &&
			j == (int)strcspn(arg, "=")) {
			c = longopts[i].letter;
			val = arg + j;
			if (*val) val++;
			break;
		    }
		}
		if (c == '-') {
		    fprintf(stderr, "bubbles: unknown long option `%.*s'\n",
			    strcspn(arg, "="), arg);
		    return EXIT_FAILURE;
		}
	    }
	    switch (c) {
	      case 'v':
		verbose++;
		break;
	      case 'h':
		usage = TRUE;
		break;
              case 'M':
                metal = TRUE;
                break;
	      case 'V':
		{
		    char *p = VERSION;
		    int i;
		    p += strcspn(p, " ");
		    if (*p) p++;
		    i = strcspn(p, " $");
		    printf("bubbles version %.*s\n", i, p);
		}
		return EXIT_SUCCESS;
	      default:		       /* other options require an arg */
		if (!*val) {
		    if (!--ac) {
			fprintf(stderr,
				"bubbles: option `%s' requires an argument\n",
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
                  case 'p':            /* --prefradius */
		    if (!parseint(val, &prefradius, "preferred distance"))
			return EXIT_FAILURE;
		    break;
                  case 'r':            /* --radius */
		    if (!parseint(val, &radius, "maximum radius"))
			return EXIT_FAILURE;
		    break;
                  case 'm':            /* --minradius */
		    if (!parseint(val, &minradius, "minimum radius"))
			return EXIT_FAILURE;
		    break;
                  case 'd':            /* --distance */
		    if (!parseint(val, &distance, "minimum distance"))
			return EXIT_FAILURE;
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
	if (!imagex || !imagey) {
	    fprintf(stderr, "bubbles: no output size specified: "
		    "use something like `-s 400x400'\n");
	    return EXIT_FAILURE;
	} else {
	    par.width = imagex;
	    par.height = imagey;
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
            int i;
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
}
