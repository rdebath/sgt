#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/*
 * C# code implementing two line-fitting algorithms which solve the
 * same problem with different quality/speed tradeoffs.
 *
 * The problem is: given a finite sequence of (x,y) coordinates of
 * points, and a positive real epsilon, find a subsequence of those
 * points which if connected with line segments yields an
 * approximation to the original set of points, such that every
 * original point is at most distance epsilon away from the output
 * set of line segments.
 *
 * (In fact, both of the functions below will always return output
 * which satisfies the additional condition that every original
 * point not in the output subsequence is at most distance epsilon
 * away from the specific line segment between the two subsequence
 * points either side of it. So they won't give optimal results if
 * the point list doubles back on itself.)
 *
 * Both the functions below expect to take as input an array of
 * 'point' structures (each just containing an x,y coordinate pair)
 * and an epsilon, and write into an output array of int the indices
 * of the input array which give the points in the selected
 * subsequence. The output array should always begin with zero and
 * end with the last index in the input array. Return value from the
 * function is the number of points written into the output array.
 */

struct point {
    double x, y;
};

/*
 * Greedy algorithm. This works by finding the longest line segment
 * starting from point 0 which satisfies the maximum distance
 * criterion, outputting it, and then looking for the longest line
 * segment starting from its endpoint and so on.
 *
 * To implement this naively would take O(N^3) time: one loop per
 * output segment, one loop inside that trying all possible lengths
 * for that segment, and a further loop inside _that_ which looked
 * at each point bypassed by the candidate segment and checked its
 * maximum distance.
 *
 * However, we do better than that by eliminating the inner loop.
 * When considering each line segment, we consider the interval of
 * angles subtended at the fixed starting point by the possible set
 * of line segments which pass within epsilon of all points seen so
 * far. We can dynamically update that range as each new point comes
 * into view. This means that we can determine the validity of a
 * candidate segment by a fixed amount of work (just check its angle
 * against the current interval, and update the current interval),
 * and also that we can terminate the second-level loop early as
 * soon as the interval drops to size zero (meaning that we have
 * encountered two points such that _no_ line segment from our
 * starting point can pass within epsilon of both). So this has
 * O(N^2) time in the worst case, and in the best case (if the path
 * wiggles enough that the early termination criterion is always
 * invoked after a bounded number of points) can be as good as O(N).
 *
 * (Actually, we consider gradients rather than angles, to avoid any
 * trigonometry - the worst thing we need in this work is a square
 * root.)
 */
int linefit_greedy(const struct point *pts, int npts, double epsilon,
		   int *ret)
{
    double eps2 = epsilon * epsilon;
    int i, j, k;

    i = k = 0;
    ret[k++] = 0;

    while (i < npts-1) {
	double tmax = 0, tmin = 0;
	double a = 0, b = 0, c = 0, d = 0;/* orthogonal matrix */
	double x1, y1;
	int gotmatrix = 0;
	int best = i+1;

	x1 = pts[i].x;
	y1 = pts[i].y;

	for (j = i+1; j < npts; j++) {
	    double x0, y0, vx, vy, len2, len, nx, ny;

	    x0 = pts[j].x;
	    y0 = pts[j].y;

	    vx = x0-x1; vy = y0-y1;    /* vector along line */
	    len2 = vx*vx + vy*vy;      /* squared length of vector v */
	    len = sqrt(len2);	       /* actual length of vector v */
	    nx = -vy/len; ny = vx/len; /* unit vector normal to v */

	    if (len2 > eps2) {
		if (!gotmatrix) {
		    /*
		     * Establish a frame of reference for checking
		     * max and min tangents. We arrange that the
		     * matrix
		     *
		     *   (a b)
		     *   (c d)
		     *
		     * transforms the coordinate system in which
		     * (p0-p1) lies on the positive x-axis into the
		     * actual coordinate system of the inputs. Thus,
		     * its inverse (which equals its transpose,
		     * since it's an orthogonal matrix) transforms
		     * the other way.
		     */
		    a = ny;
		    b = nx;
		    c = -nx;
		    d = ny;

		    /*
		     * Determine the min and max gradients of lines
		     * that are tangent to the circle of radius
		     * epsilon about that point in the transformed
		     * coordinates.
		     */
		    tmax = sqrt(eps2 / (len2 - eps2));
		    tmin = -tmax;
		    gotmatrix = 1;
		} else {
		    /*
		     * We've already got a frame of reference.
		     * Transform our new point's coordinates into
		     * that.
		     */
		    double x = a*vx + c*vy, y = b*vx + d*vy;

		    if (x <= -epsilon) {
			break;     /* totally the wrong direction */
		    } else {
			/*
			 * Work out the tangent to the new point's
			 * epsilon-circle in _its_ natural
			 * coordinate system (i.e. the one in which
			 * the new point lies on the positive
			 * x-axis).
			 */
			double t = sqrt(eps2 / (len2 - eps2));
			/*
			 * Work out the tangent, in our main
			 * coordinate system, of the vector straight
			 * to the new point.
			 */
			double tp = y / x;

			if (x > epsilon || y < 0) {
			    /*
			     * Transform the upper tangent t into
			     * our main coordinate system using the
			     * formula
			     *
			     * tan(a+b) = (tan(a)+tan(b))/(1-tan(a)tan(b))
			     *
			     * and use it to update our current
			     * tmax.
			     */
			    double tnew = (tp + t) / (1 - t*tp);
			    if (tmax > tnew)
				tmax = tnew;
			}
			if (x > epsilon || y > 0) {
			    /*
			     * Do the same with the lower tangent -t
			     * and update tmin.
			     */
			    double tnew = (tp - t) / (1 + t*tp);
			    if (tmin < tnew)
				tmin = tnew;
			}

			/*
			 * And if we've reduced the interval of
			 * viable tangents to less than zero size,
			 * then we have two points of which no line
			 * at all through pts[i] can simultaneously
			 * come within epsilon.
			 */
			if (tmin > tmax)
			    break;

			/*
			 * Finally, we can also check the
			 * tangent to this point itself to see
			 * if it falls within the interval. If
			 * not, we can rule out _this_ point,
			 * but not terminate the whole loop.
			 */
			if (tp < tmin || tp > tmax)
			    continue;
		    }
		}
	    }

	    best = j;
	}

	ret[k++] = i = best;
    }

    return k;
}

/*
 * Truly optimal fitting algorithm. This approach examines _every_
 * possible subsequence of the input points, and picks the one which
 * is best according to the two criteria
 *
 *  (a) use as few line segments as possible to make the maximum
 * 	error at most epsilon
 *  (b) break ties between equally short output sequences which
 * 	achieve this by picking the one with the _smallest_ maximum
 * 	error, i.e. get it even smaller than epsilon by as much as
 * 	possible.
 *
 * Using a dynamic programming approach similar in structure to the
 * Viterbi algorithm, we arrange to search all 2^N possible
 * subsequences in at worst O(N^3) time.
 *
 * We do this by working along the list, finding the optimal fit to
 * every initial subsequence and storing it for future reference.
 * For each subsequence, we consider every possible line segment
 * terminating in its last point, and combine that with the
 * already-known optimal solution for everything up to the start of
 * that line segment. Comparing the scores for all of those
 * candidates tells us the optimal solution for the current initial
 * subsequence, which we then store and use in further iterations.
 *
 * The tangent-based range analysis from the greedy algorithm is
 * reused to achieve early termination and pruning of the search
 * space: when considering line segments terminating in a given
 * point, we stop considering as soon as we realise that no line
 * segment that long or longer can work.
 */
int linefit_optimal(const struct point *pts, int npts, double epsilon,
		    int *ret)
{
    struct score {
	int nsegs;
	double maxerr;
	int prev;
    } *scores;
    double eps2 = epsilon * epsilon;

    int i, j, k;

    scores = (struct score *)malloc(sizeof(struct score) * npts);

    scores[0].nsegs = 0;
    scores[0].maxerr = 0.0;
    scores[0].prev = 0;

    for (i = 1; i < npts; i++) {
	double tmax = 0, tmin = 0;
	double a = 0, b = 0, c = 0, d = 0;/* orthogonal matrix */
	double x1, y1;
	int gotmatrix = 0;

	scores[i].nsegs = npts+1;      /* ensure anything is better */
	scores[i].maxerr = epsilon;    /* initialise these to any old */
	scores[i].prev = i;	       /*   thing, just in case */

	x1 = pts[i].x;
	y1 = pts[i].y;

	for (j = i-1; j >= 0; j--) {
	    double x0, y0, vx, vy, len2, len, nx, ny, maxerr;

	    /*
	     * Consider the line segment from pts[j] to pts[i]. Find
	     * the maximum distance from that line segment to any
	     * point in between.
	     */

	    x0 = pts[j].x;
	    y0 = pts[j].y;

	    vx = x0-x1; vy = y0-y1;    /* vector along line */
	    len2 = vx*vx + vy*vy;      /* squared length of vector v */
	    len = sqrt(len2);	       /* actual length of vector v */
	    nx = -vy/len; ny = vx/len; /* unit vector normal to v */

	    if (len2 > eps2) {
		if (!gotmatrix) {
		    /*
		     * Establish a frame of reference for checking
		     * max and min tangents. We arrange that the
		     * matrix
		     *
		     *   (a b)
		     *   (c d)
		     *
		     * transforms the coordinate system in which
		     * (p0-p1) lies on the positive x-axis into the
		     * actual coordinate system of the inputs. Thus,
		     * its inverse (which equals its transpose,
		     * since it's an orthogonal matrix) transforms
		     * the other way.
		     */
		    a = ny;
		    b = nx;
		    c = -nx;
		    d = ny;

		    /*
		     * Determine the min and max gradients of lines
		     * that are tangent to the circle of radius
		     * epsilon about that point in the transformed
		     * coordinates.
		     */
		    tmax = sqrt(eps2 / (len2 - eps2));
		    tmin = -tmax;
		    gotmatrix = 1;
		} else {
		    /*
		     * We've already got a frame of reference.
		     * Transform our new point's coordinates into
		     * that.
		     */
		    double x = a*vx + c*vy, y = b*vx + d*vy;

		    if (x <= -epsilon) {
			break;	       /* totally the wrong direction */
		    } else {
			/*
			 * Work out the tangent to the new point's
			 * epsilon-circle in _its_ natural
			 * coordinate system (i.e. the one in which
			 * the new point lies on the positive
			 * x-axis).
			 */
			double t = sqrt(eps2 / (len2 - eps2));
			/*
			 * Work out the tangent, in our main
			 * coordinate system, of the vector straight
			 * to the new point.
			 */
			double tp = y / x;

			if (x > epsilon || y < 0) {
			    /*
			     * Transform the upper tangent t into
			     * our main coordinate system using the
			     * formula
			     *
			     * tan(a+b) = (tan(a)+tan(b))/(1-tan(a)tan(b))
			     *
			     * and use it to update our current
			     * tmax.
			     */
			    double tnew = (tp + t) / (1 - t*tp);
			    if (tmax > tnew)
				tmax = tnew;
			}
			if (x > epsilon || y > 0) {
			    /*
			     * Do the same with the lower tangent -t
			     * and update tmin.
			     */
			    double tnew = (tp - t) / (1 + t*tp);
			    if (tmin < tnew)
				tmin = tnew;
			}

			/*
			 * And if we've reduced the interval of
			 * viable tangents to less than zero size,
			 * then we have two points of which no line
			 * at all through pts[i] can simultaneously
			 * come within epsilon.
			 */
			if (tmin > tmax)
			    break;

			/*
			 * Finally, we can also check the tangent to
			 * this point itself to see if it falls
			 * within the interval. If not, we can rule
			 * out _this_ point, but not terminate the
			 * whole loop.
			 */
			if (tp < tmin || tp > tmax)
			    continue;
		    }
		}
	    }

	    if (scores[j].nsegs+1 > scores[i].nsegs)
		continue;	       /* need not try this one at all */

	    maxerr = scores[j].maxerr;

	    for (k = j+1; k < i; k++) {
		double dist;
		double x = pts[k].x - x1, y = pts[k].y - y1;
		double vdp = vx * x + vy * y;   /* dot product with v */

		if (vdp < 0)
		    dist = sqrt(x*x + y*y);
		else if (vdp > len2)
		    dist = sqrt((x-vx)*(x-vx) + (y-vy)*(y-vy));
		else
		    dist = fabs(nx * x + ny * y);

		if (maxerr < dist)
		    maxerr = dist;
		if (dist > epsilon)
		    break;	       /* early termination */
	    }

	    if (maxerr <= epsilon) {
		int nsegs = scores[j].nsegs + 1;
		if (nsegs < scores[i].nsegs ||
		    (nsegs == scores[i].nsegs &&
		     maxerr < scores[i].maxerr)) {
		    scores[i].nsegs = nsegs;
		    scores[i].maxerr = maxerr;
		    scores[i].prev = j;
		}
	    }
	}
    }

    i = npts-1;
    j = scores[i].nsegs;
    k = j+1;
    while (1) {
	ret[j] = i;
	if (i == 0) {
	    assert(j == 0);
	    break;
	}
	i = scores[i].prev;
	j--;
    }

    free(scores);

    return k;
}

void test(const struct point *array, int len, char *name)
{
    int ret[100];
    int i, nret;

    nret = linefit_greedy(array, len, 1, ret);
    printf("%s greedy:  ", name);
    for (i = 0; i < nret; i++) {
	printf(" %d", ret[i]);
    }
    printf("\n");

    nret = linefit_optimal(array, len, 1, ret);
    printf("%s optimal: ", name);
    for (i = 0; i < nret; i++) {
	printf(" %d", ret[i]);
    }
    printf("\n");
}

#define lenof(x) (sizeof((x))/sizeof(*(x)))

int main(void)
{
    static const struct point test1[] = {
	{18,18}, {20,20}, {21,20}, {30,20}, {40,20}, {49,20}, {50,20}, {52,22}
    };
    static const struct point test2[] = {
	{10,10}, {20,10.99}, {30,9.01}, {40,10}
    };
    static const struct point test3[] = {
	{10,10}, {20,11.4}, {30,10.99}, {40,10}
    };
    static const struct point test4[] = {
	{10,10.1}, {13,11}, {16,11.9}, {19,11}, {22,10.1}, {25,11},
	{28,11.9}, {31,11}, {34,10.1}, {37,11}, {40,11.9}, {43,11},
	{46,10.1}, {49,11}, {52,11.9},
    };
    test(test1, lenof(test1), "1");
    test(test2, lenof(test2), "2");
    test(test3, lenof(test3), "3");
    test(test4, lenof(test4), "4");

    {
	int npoints = 1001;
	int i;
	struct point *points = malloc(npoints * sizeof(struct point));
	const double twopi = 2 * 3.1415926535897932384626433832795;
	for (i = 0; i < npoints; i++) {
	    points[i].x = 11*cos(twopi*i/(npoints-1));
	    points[i].y = 11*sin(twopi*i/(npoints-1));
	}
	test(points, npoints, "5");
    }

    return 0;
}
