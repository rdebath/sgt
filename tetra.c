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
#include <string.h>
#include <time.h>
#include <math.h>

#include "misc.h"

#include "bmpwrite.h"

#include "cmdline.h"

#define VERSION "$Revision$"

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
    int i, j;
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
    struct Bitmap *bm;
    int height;

    height = (sqrt(3)/2) * params.side;

    bm = bmpinit(params.outfile, params.width, params.height);

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

	    if ((yf == 0 ||
		 (int)(nxf*(params.side/2)+0.5) ==
		 (int)(yf*(params.side/2)+0.5)) &&
		params.show_edges) {
		c.r = c.g = c.b = 0;
	    }

	    bmppixel(bm, c.r*255.0, c.g*255.0, c.b*255.0);
        }
        bmpendrow(bm);
    }
    bmpclose(bm);

    return 1;
}

/* ----------------------------------------------------------------------
 * Main program: parse the command line and call plot() when satisfied.
 */

struct ColourList {
    int n, max;
    struct RGB *colours;
};

int addcol(char *string, void *vret)
{
    struct ColourList *list = (struct ColourList *)vret;
    int n = list->n++;
    struct RGB c;

    return parsecol(string, (n < list->max ? list->colours + n : &c));
}

int main(int argc, char **argv) {
    int verbose = FALSE;
    char *outfile = NULL;
    struct Size imagesize = {0,0};
    int side = 0;
    int vertex_mode = FALSE, face_mode = FALSE;
    struct RGB colours[4], centre;
    struct ColourList colourlist = { 0, 4, colours };
    int gotcentre = FALSE;
    double distance;
    int cuboid = FALSE;
    int gotdistance = FALSE;
    int edges = FALSE;

    struct Params par;
    int i;

    struct Cmdline options[] = {
	{0, NULL, 0, "<colour>", "specify a colour (must be 0 or 4 of them)",
		"colour", addcol, &colourlist, NULL},
	{1, "--centre", 'R', "colour", "specify centre colour for random choice",
		"centre colour", parsecol, &centre, &gotcentre},
	{1, "--distance", 'D', NULL, "specify radial distance for random choice",
		"distance", parseflt, &distance, &gotdistance},
	{1, "--cuboid", 'B', NULL, "use colour cuboid not cube in random choice",
		NULL, NULL, NULL, &cuboid},
	{1, "--corners", 'c', NULL, "assign colours to corners of solid",
		NULL, NULL, NULL, &vertex_mode},
	{1, "--faces", 'f', NULL, "assign colours to faces of solid",
		NULL, NULL, NULL, &face_mode},
	{1, "--edges", 'e', NULL, "show edges of solid",
		NULL, NULL, NULL, &edges},
	{1, "--grid", 'g', "NNN", "grid size (side length of triangle)",
		"grid size", parseint, &side, NULL},
	{1, "--output", 'o', "file.bmp", "output bitmap name",
		"filename", parsestr, &outfile, NULL},
	{1, "--size", 's', "NNNxNNN", "output bitmap size",
		"output bitmap size", parsesize, &imagesize, NULL},
	{1, "--verbose", 'v', NULL, "report details of what is done",
		NULL, NULL, NULL, &verbose},
    };

    parse_cmdline("tetra", argc, argv, options, lenof(options));

    if (argc < 2)
	usage_message("tetra [options] [<colour> <colour> <colour> <colour>]",
		      options, lenof(options), NULL, 0);

    /*
     * Having read the arguments, now process them.
     */

    /* If no output file, complain. */
    if (!outfile) {
	fprintf(stderr, "tetra: no output file specified: "
		"use something like `-o file.bmp'\n");
	return EXIT_FAILURE;
    } else
	par.outfile = outfile;

    /* If no colours, default to a standard set. */
    if (colourlist.n == 0) {
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
		 *  - first we rotate by a random amount about the
		 *    z-axis.
		 *
		 *  - then we rotate about the x-axis by an amount
		 *    chosen in a biased manner: the angle must be
		 *    between 0 and pi, and the probability of any
		 *    angular band must be proportional to the area
		 *    of a sphere's surface contained in that band.
		 *    This tells us that the probability density at
		 *    angle theta is proportional to sin(theta),
		 *    and hence that the cumulative probability
		 *    function is (1-cos(theta))/2. Thus, we pick a
		 *    random number x between 0 and 1, and apply
		 *    the inverse of this function, i.e. we compute
		 *    acos(1-2x).
		 *
		 *  - lastly we rotate by a random amount about the
		 *    z-axis again.
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
	    parsecol("1,0,0", &par.c[0]);
	    parsecol("0,1,0", &par.c[1]);
	    parsecol("0,0,1", &par.c[2]);
	    parsecol("1,1,1", &par.c[3]);
	}
    } else if (colourlist.n == 4) {
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
    if (!imagesize.w || !imagesize.h) {
	fprintf(stderr, "tetra: no output size specified: "
		"use something like `-s 400x400'\n");
	return EXIT_FAILURE;
    } else {
	par.width = imagesize.w;
	par.height = imagesize.h;
    }

    /*
     * Ensure at most one of vertex_mode and face_mode was
     * specified.
     */
    if (vertex_mode && face_mode) {
	fprintf(stderr, "tetra: at most one of `-c' and `-f' expected");
	return EXIT_FAILURE;
    }

    par.side = side;
    par.show_edges = edges;
    par.colour_vertices = vertex_mode; /* making face mode the default */

    /*
     * If we're in verbose mode, regurgitate the final
     * parameters.
     */
    if (verbose) {
	printf("Output file `%s', %d x %d\n",
	       par.outfile, par.width, par.height);
	printf("Tetrahedron side length %d\n", par.side);
	printf("Edges %sshown\n", par.show_edges ? "" : "not ");
	printf("Colours assigned to %s\n",
	       par.colour_vertices ? "corners" : "faces");
    }

    i = plot(par) ? EXIT_SUCCESS : EXIT_FAILURE;

    return i;
}
