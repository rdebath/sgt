/* tetra.c - draw rolling-tetrahedron tilings
 *
 * This program is copyright 2000,2004 Simon Tatham.
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

#define VERSION "$Revision: 1.2 $"

#define TRUE 1
#define FALSE 0
#define lenof(x) (sizeof ((x)) / sizeof ( *(x) ))

/* ----------------------------------------------------------------------
 * Function prototypes and structure type predeclarations.
 */

struct RGB {
    double r, g, b;
};

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
 * Matrix utilities.
 */

typedef struct {
    double v[3][3];
} Matrix;

#define ID { { {1.0,0.0,0.0}, {0.0,1.0,0.0}, {0.0,0.0,1.0} } }

Matrix matmul(Matrix a, Matrix b)
{
    int i, j, k;
    Matrix ret;

    for (i = 0; i < 3; i++)
	for (j = 0; j < 3; j++) {
	    ret.v[i][j] = 0;
	    for (k = 0; k < 3; k++)
		ret.v[i][j] += a.v[k][j] * b.v[i][k];
	}

    return ret;
}

void transform(double *c, Matrix m)
{
    int i, j, k;
    double ret[3];

    for (i = 0; i < 3; i++) {
	ret[i] = 0;
	for (j = 0; j < 3; j++)
	    ret[i] += m.v[j][i] * c[j];
    }

    for (i = 0; i < 3; i++)
	c[i] = ret[i];
}

/* ----------------------------------------------------------------------
 * The plot function.
 */

struct Params {
    char *outfile;
    struct RGB c[4];
    int width, height, side;
    int show_edges, colour_vertices;
};

int getcorner(int x, int y)
{
    int i;

    i = ((x & 2) + y) & 3;

    return 1 << i;
}

double dist(double dx, double dy)
{
    return sqrt(dx*dx+3*dy*dy);
}

int plot(struct Params params) {
    int i, j, yi, xi;
    double xf, nxf, yf;
    struct Bitmap bm;
    int height;

    height = (sqrt(3)/2) * params.side;

    bmpinit(&bm, params.outfile, params.width, params.height);

    for (i = 0; i < params.height; i++) {
	yf = (float)i / height;
	yi = (int)yf;
	yf -= yi;
	
        for (j = 0; j < params.width; j++) {
	    struct RGB c;
	    int jmid;
	    int c1,c2,c3;
	    double d12, d13;
	    int xo, xs;

	    xf = (float)j / (params.side/2);
	    xi = (int)xf;
	    xf -= xi;

	    /*
	     * See which side of the diagonal we're on. This
	     * determines which type of triangle we're in.
	     */
	    if ((xi+yi) & 1) {
		nxf = 1.0-xf;
		xo = 1;
		xs = -1;
	    } else {
		nxf = xf;
		xo = 0;
		xs = 1;
	    }

	    jmid = ((i - yi*height) * params.side + height) / (2 * height);

	    if (nxf > yf) {
		/*
		 * Downward-pointing triangle.
		 */
		c1 = getcorner(xi+xo,yi);
		c2 = getcorner(xi+xo+2*xs,yi);
		c3 = getcorner(xi+xo+xs,yi+1);
		d12 = (nxf - yf) / 2.0;
		d13 = yf;
	    } else {
		/*
		 * Upward-pointing triangle.
		 */
		c1 = getcorner(xi+xo+xs,yi+1);
		c2 = getcorner(xi+xo-xs,yi+1);
		c3 = getcorner(xi+xo,yi);
		d12 = (yf - nxf) / 2.0;
		d13 = 1.0 - yf;
	    }

	    if (params.colour_vertices) {
		struct RGB C1, C2, C3;
		switch (c1) {
		  case 1: C1 = params.c[0]; break;
		  case 2: C1 = params.c[1]; break;
		  case 4: C1 = params.c[2]; break;
		  default /* case 8 */: C1 = params.c[3]; break;
		}
		switch (c2) {
		  case 1: C2 = params.c[0]; break;
		  case 2: C2 = params.c[1]; break;
		  case 4: C2 = params.c[2]; break;
		  default /* case 8 */: C2 = params.c[3]; break;
		}
		switch (c3) {
		  case 1: C3 = params.c[0]; break;
		  case 2: C3 = params.c[1]; break;
		  case 4: C3 = params.c[2]; break;
		  default /* case 8 */: C3 = params.c[3]; break;
		}
		c.r = C1.r + d12 * (C2.r - C1.r) + d13 * (C3.r - C1.r);
		c.g = C1.g + d12 * (C2.g - C1.g) + d13 * (C3.g - C1.g);
		c.b = C1.b + d12 * (C2.b - C1.b) + d13 * (C3.b - C1.b);
	    } else {
		switch (c1 | c2 | c3) {
		  case 15 ^ 1: c = params.c[0]; break;
		  case 15 ^ 2: c = params.c[1]; break;
		  case 15 ^ 4: c = params.c[2]; break;
		  default /* case 15 ^ 8 */: c = params.c[3]; break;
		}
	    }

	    if ((yf == 0 || nxf * (params.side/2) == jmid) &&
		params.show_edges) {
		c.r = c.g = c.b = 0;
	    }

	    bmppixel(&bm, c.r*255.0, c.g*255.0, c.b*255.0);
        }
        bmpendrow(&bm);
    }
    bmpclose(&bm);

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
    fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
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
    fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
    return 0;
}

int parseflt(char const *string, double *d, char const *name) {
    char *endp;
    *d = strtod(string, &endp);
    if (endp && *endp) {
	fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
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
    fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
    return 0;
}

/*
 * Read a colour into an RGB structure.
 */
int parsecol(char const *string, struct RGB *ret, char const *name) {
    struct RGB c;
    char *q;

    ret->r = strtod(string, &q);
    string = q;
    if (!*string) {
	fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
	return 0;
    } else
	string++;
    ret->g = strtod(string, &q);
    string = q;
    if (!*string) {
	fprintf(stderr, "tetra: unable to parse %s `%s'\n", name, string);
	return 0;
    } else
	string++;
    ret->b = strtod(string, &q);

    return 1;
}

int main(int ac, char **av) {
    char const *usagemsg[] = {
	"usage: tetra [options] [roots]",
	"       -C, --colour            specify a colour (must be 0 or 4)",
	"       -R, --centre            specify centre colour for random choice",
	"       -D, --distance          specify radial distance for random choice",
	"       -B, --cuboid            use colour cuboid not cube in random choice",
	"       -c, --corners           assign colours to corners of solid",
	"       -f, --faces             assign colours to faces of solid",
	"       -e, --edges             show edges of solid",
	"       -g, --grid NNN          grid size (side length of triangle)",
	"       -o, --output file.bmp   output bitmap name",
	"       -s, --size NNNxNNN      output bitmap size",
	"       -v, --verbose           report details of what is done",
    };
    struct Params par;
    int usage = FALSE;
    int verbose = FALSE;
    char *outfile = NULL;
    int imagex = 0, imagey = 0;
    int side = 0;
    int done_args = FALSE;
    enum { FACES, VERTICES } mode = FACES;
    struct RGB colours[4], centre;
    int ncolours = 0, gotcentre = FALSE;
    double distance;
    int cuboid = FALSE;
    int gotdistance = FALSE;
    int edges = FALSE;

    if (ac < 2) {
	usage = TRUE;
    } else while (--ac) {
	char *arg = *++av;

	if (arg[0] != '-' || done_args) {
	    return EXIT_FAILURE;
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
		    {"--corners", 'c'},
		    {"--colour", 'C'},
		    {"--centre", 'R'},
		    {"--distance", 'D'},
		    {"--cuboid", 'B'},
		    {"--faces", 'f'},
		    {"--edges", 'e'},
		    {"--grid", 'g'},
		    {"--output", 'o'},
		    {"--size", 's'},
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
		    fprintf(stderr, "tetra: unknown long option `%.*s'\n",
			    strcspn(arg, "="), arg);
		    return EXIT_FAILURE;
		}
	    }
	    switch (c) {
	      case 'v':
		verbose = TRUE;
		break;
	      case 'f':
		mode = FACES;
		break;
	      case 'e':
		edges = TRUE;
		break;
	      case 'c':
		mode = VERTICES;
		break;
	      case 'B':
		cuboid = TRUE;
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
		    printf("tetra version %.*s\n", i, p);
		}
		return EXIT_SUCCESS;
	      default:		       /* other options require an arg */
		if (!*val) {
		    if (!--ac) {
			fprintf(stderr,
				"tetra: option `%s' requires an argument\n",
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
		  case 'g':	       /* --grid */
		    if (!parseint(val, &side, "grid size"))
			return EXIT_FAILURE;
		    break;
		  case 'C':	       /* --colour */
		    if (ncolours < 4 &&
			!parsecol(val, &colours[ncolours], "colour"))
			return EXIT_FAILURE;
		    ncolours++;
		    break;
		  case 'R':	       /* --centre */
		    if (!parsecol(val, &centre, "centre colour"))
			return EXIT_FAILURE;
		    gotcentre = TRUE;
		    break;
		  case 'D':	       /* --distance */
		    if (!parseflt(val, &distance, "distance"))
			return EXIT_FAILURE;
		    gotdistance = TRUE;
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
	    fprintf(stderr, "tetra: no output file specified: "
		    "use something like `-o file.bmp'\n");
	    return EXIT_FAILURE;
	} else
	    par.outfile = outfile;

	/* If no colours, default to a standard set. */
	if (ncolours == 0) {
	    if (gotcentre || gotdistance) {
		if (gotcentre && gotdistance) {
		    double coords[4][3] = {
			{ 1.0, 0.0, 0.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, 1.0 },
			{ 1.0, 1.0, 1.0 },
		    };

		    Matrix m1 = ID, m2 = ID, m3 = ID, m;
		    double angle;
		    int i, j;

		    /*
		     * To select a completely random rotation:
		     * 
		     * 	- first we rotate by a random amount about
		     * 	  the z-axis.
		     * 
		     * 	- then we rotate about the x-axis by an
		     * 	  amount chosen in a biased manner: the
		     * 	  angle must be between 0 and pi, and the
		     * 	  probability of any angular band must be
		     * 	  proportional to the area of a sphere's
		     * 	  surface contained in that band. This
		     * 	  tells us that the probability density at
		     * 	  angle theta is proportional to
		     * 	  sin(theta), and hence that the cumulative
		     * 	  probability function is (1-cos(theta))/2.
		     * 	  Thus, we pick a random number x between 0
		     * 	  and 1, and apply the inverse of this
		     * 	  function, i.e. we compute acos(1-2x).
		     * 
		     * 	- lastly we rotate by a random amount about
		     * 	  the z-axis again.
		     */

		    srand(time(NULL));

		    angle = rand() / (double)RAND_MAX;
		    m1.v[0][0] = m1.v[1][1] = cos(angle);
		    m1.v[0][1] = -(m1.v[1][0] = sin(angle));

		    angle = acos(1 - 2 * (rand() / (double)RAND_MAX));
		    m2.v[1][1] = m2.v[2][2] = cos(angle);
		    m2.v[1][2] = -(m2.v[2][1] = sin(angle));

		    angle = rand() / (double)RAND_MAX;
		    m3.v[0][0] = m3.v[1][1] = cos(angle);
		    m3.v[0][1] = -(m3.v[1][0] = sin(angle));

		    m = matmul(matmul(m1,m2), m3);

		    for (i = 0; i < 4; i++) {
			transform(coords[i], m);
			for (j = 0; j < 3; j++) {
			    static const double scale[3] = {0.30, 0.59, 0.11};
			    double *cp, *op;
			    coords[i][j] *= distance;
			    if (cuboid)
				coords[i][j] /= scale[j];

			    if (j == 0)
				cp = &par.c[i].r, op = &centre.r;
			    else if (j == 1)
				cp = &par.c[i].g, op = &centre.g;
			    else
				cp = &par.c[i].b, op = &centre.b;

			    *cp = *op + coords[i][j];
			    if (*cp < 0) *cp = 0;
			    if (*cp > 1) *cp = 1;
			}
		    }
		    
		} else {
		    fprintf(stderr, "tetra: require both centre and distance"
			    " for random choice");
		    return EXIT_FAILURE;
		}
	    } else {
		parsecol("1,0,0", &par.c[0], "");
		parsecol("0,1,0", &par.c[1], "");
		parsecol("0,0,1", &par.c[2], "");
		parsecol("1,1,1", &par.c[3], "");
	    }
	} else if (ncolours == 4) {
	    par.c[0] = colours[0];
	    par.c[1] = colours[1];
	    par.c[2] = colours[2];
	    par.c[3] = colours[3];
	} else {
	    fprintf(stderr, "tetra: expected exactly zero or four colours");
	    return EXIT_FAILURE;
	}

	/*
	 * Now complain if no output image size was specified.
	 */
	if (!imagex || !imagey) {
	    fprintf(stderr, "tetra: no output size specified: "
		    "use something like `-s 400x400'");
	    return EXIT_FAILURE;
	} else {
	    par.width = imagex;
	    par.height = imagey;
	}

	/*
	 * If we're in verbose mode, regurgitate the final
	 * parameters.
	 */
	if (verbose) {
            int i;
            printf("Output file `%s', %d x %d\n",
                   par.outfile, par.width, par.height);
	}

	par.side = side;
	par.show_edges = edges;
	par.colour_vertices = (mode == VERTICES);

	i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

        return i;
    }
}
