/*
 * greyblur.c: produce an image containing a random blur of black
 * and white and shades of grey.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"

#include "bmpwrite.h"

#include "cmdline.h"

#define VERSION "$Revision: 4787 $"

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
    struct Bitmap *bm;

    grid = malloc(par.width*par.height);
    generate(par.width, par.height, grid, par.steps, par.levels);

    bm = bmpinit(par.outfile, par.width, par.height);
    for (i = 0; i < par.height; i++) {
        for (j = 0; j < par.width; j++) {
            int val = grid[par.width * i + j];
            bmppixel(bm, val, val, val);
        }
        bmpendrow(bm);
    }
    bmpclose(bm);

    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

int main(int argc, char **argv) {
    int verbose = 0;
    char *outfile = NULL;
    struct Size imagesize = {0,0};
    int steps = 0, levels = 0;
    struct Params par;
    int i;

    struct Cmdline options[] = {
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, &outfile, NULL},
	{1, "--size", 's', "NNNxNNN", "output bitmap size",
		"output bitmap size", parsesize, &imagesize, NULL},
	{1, "--steps", 'n', "NNN", "number of iterations of smoothing",
		"step count", parseint, &steps, NULL},
	{1, "--levels", 'l', "NNN", "number of levels of output colour",
		"level count", parseint, &levels, NULL},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, NULL, &verbose},
    };

    parse_cmdline("greyblur", argc, argv, options, lenof(options));

    if (argc < 2)
	usage_message("greyblur [options]", options, lenof(options), NULL, 0);

    /*
     * Having read the arguments, now process them.
     */

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
    if (!imagesize.w || !imagesize.h) {
	fprintf(stderr, "greyblur: no output size specified: "
		"use something like `-s 400x400'\n");
	return EXIT_FAILURE;
    } else {
	par.width = imagesize.w;
	par.height = imagesize.h;
    }

    par.steps = (steps > 0 ? steps : 5);
    par.levels = (levels > 0 ? levels : 2);

    /*
     * If we're in verbose mode, regurgitate the final
     * parameters.
     */
    if (verbose) {
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Smoothing steps: %d\n", par.steps);
	printf("Output grey levels: %d\n", par.levels);
    }

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    return i;
}
