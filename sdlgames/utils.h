/*
 * Draw a line. plot() will be called for each point on the line in
 * turn.
 */
void line(int x1, int y1, int x2, int y2,
	  void (*plot)(void *, int, int), void *ctx);

/*
 * Draw a circle. plot() will be called for all points in one
 * octant: specifically, plot(ctx,x,y) will be called for points
 * with x >= 0, y > 0 and y >= x.
 */
void circle(int r, void (*plot)(void *, int, int), void *ctx);
