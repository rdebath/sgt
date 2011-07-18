/*
 * greyblur.c: produce an image containing a random blur of black
 * and white and shades of grey.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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
    /*
     * We actually compute a region larger than the one we're
     * returning, to avoid edge effects due to the edge and corner
     * pixels getting greater weight in averaging steps and hence
     * unduly influencing their surroundings.
     */
    int W = w + 2*steps, H = h + 2*steps;

    float *fgrid;
    float *fgrid2;
    int step, i, j;
    float min, max;

    fgrid = malloc(W*H*sizeof(float));

    for (i = 0; i < H; i++) {
        for (j = 0; j < W; j++) {
            fgrid[i*W+j] = rand() / (float)RAND_MAX;
        }
    }

    /*
     * Evolve the starting grid using a cellular automaton.
     * Currently, I'm doing something very simple indeed, which is
     * to set each square to the average of the surrounding nine
     * cells (or the average of fewer, if we're on a corner).
     */
    for (step = 0; step < steps; step++) {
        fgrid2 = malloc(W*H*sizeof(float));

        for (i = 0; i < H; i++) {
            for (j = 0; j < W; j++) {
                float sx, xbar;
                int n, p, q;

                /*
                 * Compute the average of the surrounding cells.
                 */
                n = 0;
                sx = 0.F;
                for (p = -1; p <= +1; p++) {
                    for (q = -1; q <= +1; q++) {
                        if (i+p < 0 || i+p >= H || j+q < 0 || j+q >= W)
                            continue;
                        n++;
                        sx += fgrid[(i+p)*W+(j+q)];
                    }
                }
                xbar = sx / n;

                fgrid2[i*W+j] = xbar;
            }
        }

        free(fgrid);
        fgrid = fgrid2;
    }

    fgrid2 = malloc(W*H*sizeof(float));
    memcpy(fgrid2, fgrid, W*H*sizeof(float));
    qsort(fgrid2, W*H, sizeof(float), float_compare);
    min = fgrid2[0];
    max = fgrid2[W*H-1];

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            float val = fgrid[(i+steps)*W+(j+steps)];
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
    int outtype;
    int width, height;
    int steps, levels;
};

int plot(struct Params par)
{
    int ii, i, j;
    unsigned char *grid;
    struct Bitmap *bm;

    grid = malloc(par.width*par.height);
    generate(par.width, par.height, grid, par.steps, par.levels);

    bm = bmpinit(par.outfile, par.width, par.height, par.outtype);
    for (ii = 0; ii < par.height; ii++) {
	/*
	 * Since the grey blur is random anyway, this shouldn't
	 * make a real difference, but it seems more elegant to
	 * arrange that _for the same random seed_ we will output
	 * BMP and PPM images looking identical.
	 */
	if (par.outtype == BMP)
	    i = ii;
	else
	    i = par.height-1 - ii;
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
    struct Params par;
    int i;

    struct optdata {
	int verbose;
	char *outfile;
	int outppm;
	struct Size imagesize;
	int steps, levels;
    } optdata = {
	0,
	NULL,
	FALSE,
	{0,0},
	0, 0,
    };

    static const struct Cmdline options[] = {
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, offsetof(struct optdata, outfile), -1},
	{1, "--ppm", 0, NULL, "output PPM rather than BMP",
		NULL, NULL, -1, offsetof(struct optdata, outppm)},
	{1, "--size", 's', "NNNxNNN", "output bitmap size",
		"output bitmap size", parsesize, offsetof(struct optdata, imagesize), -1},
	{1, "--steps", 'n', "NNN", "number of iterations of smoothing",
		"step count", parseint, offsetof(struct optdata, steps), -1},
	{1, "--levels", 'l', "NNN", "number of levels of output colour",
		"level count", parseint, offsetof(struct optdata, levels), -1},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, -1, offsetof(struct optdata, verbose)},
    };

    parse_cmdline("greyblur", argc, argv, options, lenof(options), &optdata);

    if (argc < 2)
	usage_message("greyblur [options]", options, lenof(options), NULL, 0);

    /*
     * Having read the arguments, now process them.
     */

    /* If no output file, complain. */
    if (!optdata.outfile) {
	fprintf(stderr, "greyblur: no output file specified: "
		"use something like `-o file.bmp'\n");
	return EXIT_FAILURE;
    } else
	par.outfile = optdata.outfile;
    par.outtype = (optdata.outppm ? PPM : BMP);

    /*
     * Now complain if no output image size was specified.
     */
    if (!optdata.imagesize.w || !optdata.imagesize.h) {
	fprintf(stderr, "greyblur: no output size specified: "
		"use something like `-s 400x400'\n");
	return EXIT_FAILURE;
    } else {
	par.width = optdata.imagesize.w;
	par.height = optdata.imagesize.h;
    }

    par.steps = (optdata.steps > 0 ? optdata.steps : 5);
    par.levels = (optdata.levels > 0 ? optdata.levels : 2);

    /*
     * If we're in verbose mode, regurgitate the final
     * parameters.
     */
    if (optdata.verbose) {
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Smoothing steps: %d\n", par.steps);
	printf("Output grey levels: %d\n", par.levels);
    }

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    return i;
}
