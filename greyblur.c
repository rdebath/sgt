/*
 * greyblur.c: produce an image containing a random blur of black
 * and white and shades of grey.
 */

#include <stdio.h>
#include <stdlib.h>

#define VERSION "$Revision: 4787 $"

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
 * Actually do the plotting.
 */

static int float_compare(const void *av, const void *bv)
{
    const float *a = (const float *)av;
    const float *b = (const float *)bv;
    if (*a < *b)
        return -1;
    else if (*a > *b)
        return +1;
    else
        return 0;
}

static void generate(int w, int h, unsigned char *retgrid, int steps,
                     int levels)
{
    float *fgrid;
    float *fgrid2;
    int step, i, j;
    float min, max;

    fgrid = malloc(w*h*sizeof(float));

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fgrid[i*w+j] = rand() / (float)RAND_MAX;
        }
    }

    /*
     * Evolve the starting grid using a cellular automaton.
     * Currently, I'm doing something very simple indeed, which is
     * to set each square to the average of the surrounding nine
     * cells (or the average of fewer, if we're on a corner).
     */
    for (step = 0; step < steps; step++) {
        fgrid2 = malloc(w*h*sizeof(float));

        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                float sx, xbar;
                int n, p, q;

                /*
                 * Compute the average of the surrounding cells.
                 */
                n = 0;
                sx = 0.F;
                for (p = -1; p <= +1; p++) {
                    for (q = -1; q <= +1; q++) {
                        if (i+p < 0 || i+p >= h || j+q < 0 || j+q >= w)
                            continue;
                        n++;
                        sx += fgrid[(i+p)*w+(j+q)];
                    }
                }
                xbar = sx / n;

                fgrid2[i*w+j] = xbar;
            }
        }

        free(fgrid);
        fgrid = fgrid2;
    }

    fgrid2 = malloc(w*h*sizeof(float));
    memcpy(fgrid2, fgrid, w*h*sizeof(float));
    qsort(fgrid2, w*h, sizeof(float), float_compare);
    min = fgrid2[0];
    max = fgrid2[w*h-1];

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            float val = fgrid[i*w+j];
            int level = levels * (val - min) / (max - min);
            if (level >= levels) level = levels-1;
            retgrid[i*w+j] = 255 * level / (levels-1);
        }
    }

    free(fgrid2);
    free(fgrid);
}

struct Params {
    char *outfile;
    int width, height;
    int steps, levels;
};

int plot(struct Params par)
{
    int i, j;
    unsigned char *grid;
    struct Bitmap bm;

    grid = malloc(par.width*par.height);
    generate(par.width, par.height, grid, par.steps, par.levels);

    bmpinit(&bm, par.outfile, par.width, par.height);
    for (i = 0; i < par.height; i++) {
        for (j = 0; j < par.width; j++) {
            int val = grid[par.width * i + j];
            bmppixel(&bm, val, val, val);
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
    fprintf(stderr, "greyblur: unable to parse %s `%s'\n", name, string);
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
    fprintf(stderr, "greyblur: unable to parse %s `%s'\n", name, string);
    return 0;
}

int main(int ac, char **av) {
    char const *usagemsg[] = {
	"usage: greyblur [options]",
	"       -o, --output file.bmp   output bitmap name",
	"       -s, --size NNNxNNN      output bitmap size",
	"       -n, --steps NNN         number of iterations of smoothing",
	"       -l, --levels NNN        number of levels of output colour",
	"       -v, --verbose           report details of what is done",
    };
    struct Params par;
    int usage = FALSE;
    int verbose = 0;
    int metal = FALSE;
    char *outfile = NULL;
    int imagex = 0, imagey = 0;
    int steps = 0, levels = 0;
    int i;
    int radius = 0, minradius = 0, prefradius = 0, distance = 0;

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
		    {"--steps", 'm'},
		    {"--levels", 'p'},
		    {"--verbose", 'v'},
		    {"--help", 'h'},
		    {"--version", 'V'},
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
		    fprintf(stderr, "greyblur: unknown long option `%.*s'\n",
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
	      case 'V':
		{
		    char *p = VERSION;
		    int i;
		    p += strcspn(p, " ");
		    if (*p) p++;
		    i = strcspn(p, " $");
		    printf("greyblur version %.*s\n", i, p);
		}
		return EXIT_SUCCESS;
	      default:		       /* other options require an arg */
		if (!*val) {
		    if (!--ac) {
			fprintf(stderr,
				"greyblur: option `%s' requires an argument\n",
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
                  case 'n':            /* --steps */
		    if (!parseint(val, &steps, "step count"))
			return EXIT_FAILURE;
		    break;
                  case 'l':            /* --levels */
		    if (!parseint(val, &levels, "level count"))
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
	    fprintf(stderr, "greyblur: no output file specified: "
		    "use something like `-o file.bmp'\n");
	    return EXIT_FAILURE;
	} else
	    par.outfile = outfile;

	/*
	 * Now complain if no output image size was specified.
	 */
	if (!imagex || !imagey) {
	    fprintf(stderr, "greyblur: no output size specified: "
		    "use something like `-s 400x400'\n");
	    return EXIT_FAILURE;
	} else {
	    par.width = imagex;
	    par.height = imagey;
	}

        par.steps = (steps > 0 ? steps : 5);
        par.levels = (levels > 0 ? levels : 2);

	/*
	 * If we're in verbose mode, regurgitate the final
	 * parameters.
	 */
	if (verbose) {
            int i;
            printf("Output file `%s', %d x %d\n",
                   par.outfile, par.width, par.height);
            printf("Smoothing steps: %d\n", par.steps);
            printf("Output grey levels: %d\n", par.levels);
	}

	i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

        return i;
    }
}
