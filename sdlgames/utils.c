#include "utils.h"

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

void line(int x1, int y1, int x2, int y2,
	  void (*plot)(void *, int, int), void *ctx)
{
    int dx, dy, lx, ly, sx, sy, dl, ds, d, i;

    /*
     * Simple Bresenham's line drawing should be adequate here.
     * First sort out what octant we're in.
     */
    dx = x2 - x1; dy = y2 - y1;

    if (dx < 0 || (dx == 0 && dy < 0)) {
	int tmp;
	/*
	 * Canonify the order of the endpoints, so that drawing a
	 * line from point A to point B and then drawing in a
	 * different colour from B back to A can't ever leave any
	 * pixels of the first colour.
	 */
	tmp = x1; x1 = x2; x2 = tmp; dx = -dx;
	tmp = y1; y1 = y2; y2 = tmp; dy = -dy;
    }

    if (abs(dx) > abs(dy)) {
	lx = sign(dx); ly = 0;	       /* independent variable on x axis */
	sx = 0; sy = sign(dy);	       /* dependent variable on y axis */
	dl = abs(dx); ds = abs(dy);
    } else {
	lx = 0; ly = sign(dy);	       /* independent variable on y axis */
	sx = sign(dx); sy = 0;	       /* dependent variable on x axis */
	dl = abs(dy); ds = abs(dx);
    }

    /*
     * Now we've normalised the octants, draw the line. Decision
     * variable starts at dl, decrements by 2*ds, and we add on
     * 2*dl and move up one pixel when it's gone past zero.
     */
    d = dl;
    plot(ctx, x1, y1);
    for (i = 0; i < dl; i++) {
	d -= 2*ds;
	if (d < 0) {
	    d += 2*dl;
	    x1 += sx; y1 += sy;
	}
	x1 += lx; y1 += ly;
	plot(ctx, x1, y1);
    }
}

void circle(int r, void (*plot)(void *, int, int), void *ctx)
{
    int dx, dy, d, d2, i;

    /*
     * Bresenham's again.
     */

    dy = r;
    dx = 0;
    d = 0;
    while (dy >= dx) {

	plot(ctx, dx, dy);

	d += 2*dx + 1;
	d2 = d - (2*dy - 1);
	if (abs(d2) < abs(d)) {
	    d = d2;
	    dy--;
	}
	dx++;
    }
}
