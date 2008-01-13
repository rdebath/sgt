/*
 * Enumerate all solutions of the Bedlam Cube, and other puzzles
 * which can be considered in the same format (namely packing
 * arbitrarily specified polycubes into a cuboidal space of total
 * area not more than 64).
 * 
 * The idea for optimising the search algorithm was cribbed from
 * http://www.danieltebbutt.com/bedlam.java by Daniel Tebbutt, but
 * no actual code is copied.
 * 
 * Compiled with gcc -O3 on my 2.4GHz Core 2 Duo, this program
 * runs in just under 40 minutes and produces all 19186 solutions.
 */

/*
 * Possible future features for other search spaces:
 * 
 *  - specifying an initial set of used bits, so that it would be
 *    possible to search non-cuboidal spaces. (However, the
 *    bounding cuboid would still be constrained to have area <=
 *    64 with current program architecture.)
 * 
 *  - generalising the bitmap type to something other than long
 *    long, probably a struct containing multiple long longs. This
 *    might permit larger polyomino puzzles to be solved, although
 *    it's unclear if larger polycubes would be computationally
 *    feasible.
 * 
 *  - permitting reflections, although I currently don't know of
 *    any puzzle which requires that (2D puzzles such as
 *    pentominoes are easily dealt with by considering them to be
 *    NxMx1 puzzles in 3D, whereupon rotation about the x- or
 *    y-axis is a non-reflecting 3D transform with the effect of a
 *    reflection in 2D).
 */

#include <stdio.h>
#include <assert.h>

#define lenof(x) ( sizeof((x))/sizeof(*(x)) )

/*
 * We express the shapes of pieces within the cube by using a
 * 64-bit integer (held in an unsigned long long) to represent the
 * entire cube. Our standard mapping from bit positions to the
 * cube is to store each horizontal slice in turn, in normal
 * lexical order, starting from the top slice, from the MSB
 * downwards. Hence, if bit 63 is the MSB and bit 0 is the LSB,
 * then the top slice of the cube (viewed from above) consists of
 * bits
 * 
 *   63 62 61 60
 *   59 58 57 56
 *   55 54 53 52
 *   51 50 49 48
 * 
 * and the bottom slice consists of bits
 * 
 *   15 14 13 12
 *   11 10  9  8
 *    7  6  5  4
 *    3  2  1  0.
 */

struct piece {
    /*
     * `identifier' is used during output, to indicate which piece
     * occupies a particular square of the space. Identifier
     * letters should ideally be mnemonic in some fashion of the
     * piece they represent.
     */
    char identifier;
    /*
     * `followflag' is used to indicate interchangeable pieces. A
     * piece with followflag set is inhibited from ever being
     * placed until the previous piece in the list has already
     * been placed. Thus, if several pieces are exactly the same
     * shape and you want to avoid marking solutions as distinct
     * if they differ only by permuting those pieces, you should
     * put all of them together in one lump and mark all but the
     * first with followflag.
     * 
     * This field is ignored unless you also #define USE_FOLLOWFLAG.
     */
    char followflag;
    unsigned long long bitpattern;
};

#ifdef PENTOMINOES

#ifndef XMAX
#define XMAX 10
#endif
#ifndef ZMAX
#define ZMAX 1
#endif
#define YMAX (60 / XMAX / ZMAX)
#define ORIENTMASK 0x10101

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'F', 0, (0x04ULL << (XMAX*2)) | (0x07ULL << XMAX) | (0x02ULL)},
    {'I', 0, (0x00ULL << (XMAX*2)) | (0x00ULL << XMAX) | (0x1FULL)},
    {'L', 0, (0x00ULL << (XMAX*2)) | (0x01ULL << XMAX) | (0x0FULL)},
    {'N', 0, (0x00ULL << (XMAX*2)) | (0x03ULL << XMAX) | (0x0EULL)},
    {'P', 0, (0x00ULL << (XMAX*2)) | (0x03ULL << XMAX) | (0x07ULL)},
    {'T', 0, (0x02ULL << (XMAX*2)) | (0x02ULL << XMAX) | (0x07ULL)},
    {'U', 0, (0x00ULL << (XMAX*2)) | (0x05ULL << XMAX) | (0x07ULL)},
    {'V', 0, (0x01ULL << (XMAX*2)) | (0x01ULL << XMAX) | (0x07ULL)},
    {'W', 0, (0x01ULL << (XMAX*2)) | (0x03ULL << XMAX) | (0x06ULL)},
    {'X', 0, (0x02ULL << (XMAX*2)) | (0x07ULL << XMAX) | (0x02ULL)},
    {'Y', 0, (0x00ULL << (XMAX*2)) | (0x02ULL << XMAX) | (0x0FULL)},
    {'Z', 0, (0x01ULL << (XMAX*2)) | (0x07ULL << XMAX) | (0x04ULL)},
};

#elif defined YELLOW_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1
#define USE_FOLLOWFLAG 1

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'E', 0, 0040063ULL},	       /* E from Bedlam Cube */
    {'Z', 0, 0010013ULL},	       /* Z from Bedlam Cube */
    {'z', 0, 0002013ULL},	       /* mirror image of Z */
    {'L', 0, 0000017ULL},	       /* an L-tetromino */
    {'l', 1, 0000017ULL},	       /* another L-tetromino */
    {'T', 0, 0000013ULL},	       /* an L-triomino */
    {'t', 1, 0000013ULL},	       /* another L-triomino */
};

#elif defined ORANGE_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1
#define USE_FOLLOWFLAG 1

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'S', 0, 0004074ULL},	       /* mirror image of S from Bedlam Cube */
    {'s', 1, 0004074ULL},	       /* another copy of S */
    {'C', 0, 0040074ULL},	       /* C from Bedlam Cube */
    {'Z', 0, 0010013ULL},	       /* Z from Bedlam Cube */
    {'L', 0, 0000017ULL},	       /* an L-tetromino */
    {'l', 1, 0000017ULL},	       /* another L */
};

#elif defined GREEN_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'S', 0, 0001071ULL},	       /* S from Bedlam Cube */
    {'Q', 0, 0001033ULL},	       /* a square with a hat on top */
    {'L', 0, 0000017ULL},	       /* an L-tetromino */
    {'Z', 0, 0010013ULL},	       /* the Z piece from the Bedlam Cube */
    {'z', 0, 0002013ULL},	       /* a mirror image of Z */
    {'T', 0, 0060023ULL},	       /* mirror of T from Bedlam Cube */
};

#elif defined RED_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'L', 0, 0004017ULL},	       /* like the L from the Bedlam Cube */
    {'W', 0, 0000137ULL},	       /* W-pentomino with the corner filled */
    {'B', 0, 0001117ULL},	       /* branch tetracube with 2 long ends */
    {'Z', 0, 0002013ULL},	       /* a mirror image of Bedlam Cube Z */
    {'S', 0, 0030066ULL},	       /* a very strange piece indeed */
};

#elif defined BLUE_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1
#define USE_FOLLOWFLAG

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'I', 0, 0000007ULL},	       /* straight 1x1x3 cuboid */
    {'Z', 0, 0010013ULL},	       /* the Z piece from the Bedlam Cube */
    {'Y', 1, 0010013ULL},	       /* another Z */
    {'X', 1, 0010013ULL},	       /* a third Z */
    {'z', 0, 0002013ULL},	       /* a mirror image of Z */
    {'y', 1, 0002013ULL},	       /* another z */
    {'x', 1, 0002013ULL},	       /* a third z */
};

#elif defined WHITE_IMPUZZABLE

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1
#define USE_FOLLOWFLAG

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'S', 0, 0200311ULL},	       /* the S piece from the Bedlam Cube */
    {'Z', 0, 0010013ULL},	       /* the Z piece from the Bedlam Cube */
    {'z', 1, 0010013ULL},	       /* a second copy of Z */
    {'L', 0, 0000071ULL},	       /* a simple L-tetromino */
    {'T', 0, 0022130ULL},	       /* the T piece from the Bedlam Cube */
    {'H', 0, 0200740ULL},	       /* the H piece from the Bedlam Cube */
};

#elif defined SOMA

/*
 * The Soma cube, surprisingly, is quite difficult to get the
 * right set of solutions for. It's easy enough to present the
 * pieces, pick an asymmetric piece like the L-tetromino and
 * constrain it to a specific orientation, and get 480 solutions;
 * but there are really only 240 interestingly distinct ones,
 * because they come in _mirror_ image pairs (since all the pieces
 * are reflection-symmetric except for Z and z which are images of
 * each other).
 * 
 * So I don't think we can narrow down to only one copy of every
 * genuinely distinct solution just by constraining a specific
 * piece in the way we have done for all the other puzzles
 * supported in this file. Hmmm.
 */

#define XMAX 3
#define YMAX 3
#define ZMAX 3
#define ORIENTMASK 1

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    {'L', 0, 000071ULL},	       /* an L-tetromino */
    {'T', 0, 000072ULL},	       /* a T-tetromino */
    {'S', 0, 000036ULL},	       /* a skew tetromino */
    {'3', 0, 000031ULL},	       /* an L-triomino */
    {'B', 0, 010031ULL},	       /* a branch tetromino */
    {'Z', 0, 010013ULL},	       /* the Z piece from the Bedlam Cube */
    {'z', 1, 010032ULL},	       /* a mirror image of Z */
};

#else /* ifdef BEDLAM */

#define XMAX 4
#define YMAX 4
#define ZMAX 4
#define ORIENTMASK 1		       /* constrain F to be one way round */

/*
 * The piece shapes.
 */
static const struct piece pieces[] = {
    /*
     * Plane pentominoes, named after their identifying letter in
     * the usual pentomino nomenclature.
     */
    {'F', 0, 0x000472ULL},
    {'X', 0, 0x000272ULL},
    {'W', 0, 0x000463ULL},
    /*
     * Pentacubes derived by adding a knobble above or below one
     * square of a T-tetromino. Named, somewhat loosely, after the
     * place where the knobble goes.
     */
    {'M', 0, 0x200072ULL},	       /* the Middle */
    {'B', 0, 0x020072ULL},	       /* the Base end */
    {'H', 0, 0x100072ULL},	       /* the Horizontal end */
    /*
     * Pentacubes derived by adding a knobble above or below one
     * square of an L-tetromino. Named, somewhat loosely, after the
     * place where the knobble goes.
     * 
     * Note that the H pentacube above can also be considered in
     * this category, in which case its mnemonic letter must be
     * considered to stand for `Half way along'. Or you could
     * consider it to be `Half way between the two categories', if
     * you preferred.
     */
    {'L', 0, 0x100074ULL},	       /* the Long leg's end */
    {'S', 0, 0x010071ULL},	       /* the Short leg's end */
    {'C', 0, 0x100071ULL},	       /* the Corner */
    /*
     * Pentacubes derived by adding a knobble above or below one
     * square of a skew tetromino. Named, somewhat loosely, after
     * the place where the knobble goes.
     */
    {'E', 0, 0x100036ULL},	       /* an End */
    {'I', 0, 0x200036ULL},	       /* an Interior square */
    /*
     * The two pieces left over don't fit into any particularly
     * sensible category, and are named unsystematically. To say
     * the least.
     */
    {'T', 0, 0x300026ULL},	       /* the Twisty one */
    {'Z', 0, 0x100013ULL},	  /* the Zmallest (ran out of good letters!) */
};

#endif

#define AREA (XMAX*YMAX*ZMAX)
/* Compile-time assertion to ensure we fit in a 64-bit box. */
enum { dummy = 1 / (AREA <= 64) };

#define FULL ((1ULL<<(AREA-1)) + (1ULL<<(AREA-1))-1)    /* avoid overflow */

/*
 * Upper bound on the number of ways any piece may be placed. The
 * top left corner can be anywhere, and the piece can be in any of
 * 24 orientations (that being the size of the non-reflecting
 * symmetry group of a cube - any of the 6 faces can be uppermost,
 * and the cube can then be any of 4 ways round given the top
 * face).
 */
#define MAXPLACEMENTS (AREA*24)

#define NPIECES lenof(pieces)

static const int coordmax[3] = { XMAX, YMAX, ZMAX };

/*
 * Workspace within which we build lists of piece bitmaps
 */
static unsigned long long workspace[NPIECES * MAXPLACEMENTS * 2];
static unsigned long long *workspaceptr = workspace;

/*
 * Actual bitmap lists.
 */
struct piecelist {
    unsigned long long *ptr;
    int len;
};

/*
 * Complete list of placements for each piece.
 */
static struct piecelist placements[NPIECES];

/*
 * List of placements for each piece, divided up by the index of
 * its highest set bit.
 */
static struct piecelist bitplacements[NPIECES][64];

/*
 * Long long comparison function, for sorting.
 */
static int compare_longlong(const void *av, const void *bv)
{
    const unsigned long long a = *(const unsigned long long *)av;
    const unsigned long long b = *(const unsigned long long *)bv;
    if (a < b)
	return -1;
    else if (a > b)
	return +1;
    else
	return 0;
}

/*
 * Initialisation function which sets up the piece placement lists.
 */
static void setup_piecelists(void)
{
    int transforms[24][3];

    /*
     * First, compute the 24 coordinate transformations
     * corresponding to all the possible piece orientations.
     */
    {
	int i;
	for (i = 0; i < 24; i++) {
	    int topface = i / 4, rotate = i % 4;
	    /*
	     * `topface' runs from 0 to 5, and gives us the
	     * permutation of the three coordinates.
	     */
	    transforms[i][0] = topface / 2;
	    transforms[i][1] = (topface / 2 + 1) % 3;
	    transforms[i][2] = (topface / 2 + 2) % 3;
	    if (topface % 2) {
		int tmp = transforms[i][1];
		transforms[i][1] = transforms[i][2];
		transforms[i][2] = tmp;
		/*
		 * If we perform an odd permutation of the
		 * coordinate axes, we must also negate one of
		 * them to keep the overall transformation
		 * non-reflecting. We denote negation by XORing
		 * 0x80 into an element.
		 */
		transforms[i][0] ^= 0x80;
	    }

	    /*
	     * Now we rotate about the 0-axis.
	     */
	    while (rotate-- > 0) {
		int tmp = transforms[i][1];
		transforms[i][1] = transforms[i][2];
		transforms[i][2] = tmp ^ 0x80;
	    }
	}
    }
 
    /*
     * Now prepare the complete list of placement bitmaps for
     * every piece. We constrain the first piece - the F-pentomino
     * - to the first of the list of possible orientations, which
     * eliminates symmetry-equivalent solutions from the output
     * list.
     * 
     * In physical terms, our constraint is that the F-pentomino
     * should be in the horizontal plane, and oriented like
     * 
     *   *
     *   ***
     *    *
     * 
     * in all solutions we output.
     */
    {
	int piece, orient, orients, oc[3], ok, bit, newbit;
	unsigned long long source, placement;

	for (piece = 0; piece < NPIECES; piece++) {
	    source = pieces[piece].bitpattern;
	    placements[piece].ptr = workspaceptr;
	    for (orient = 0; orient < 24; orient++) {
		if (piece == 0 && !(ORIENTMASK & (1ULL << orient)))
		    continue;
		for (oc[0] = -XMAX; oc[0] < XMAX; oc[0]++)
		    for (oc[1] = -YMAX; oc[1] < YMAX; oc[1]++)
			for (oc[2] = -ZMAX; oc[2] < ZMAX; oc[2]++) {
			    placement = 0ULL;
			    ok = 1;
			    for (bit = 0; bit < 64; bit++) {
				int c[3], nc[3];
				int i;

				if (!(source & (1ULL<<bit)))
				    continue;

				c[0] = bit % XMAX;
				c[1] = (bit / XMAX) % YMAX;
				c[2] = (bit / (XMAX*YMAX));

				for (i = 0; i < 3; i++) {
				    nc[i] = c[transforms[orient][i] & 3];
				    if (transforms[orient][i] & 0x80)
					nc[i] *= -1;
				}
				for (i = 0; i < 3; i++)
				    nc[i] += oc[i];
				for (i = 0; i < 3; i++)
				    if (nc[i] < 0 || nc[i] >= coordmax[i]) {
					ok = 0;
					break;
				    }
				if (i == 3) {
				    newbit = (nc[2]*YMAX+nc[1])*XMAX+nc[0];
				    placement |= 1ULL << newbit;
				}
			    }
			    if (ok) {
				assert(workspaceptr <
				       workspace + lenof(workspace));
				*workspaceptr++ = placement;
			    }
			}
	    }
	    placements[piece].len = workspaceptr - placements[piece].ptr;
	}
    }

    /*
     * Now sort each placement list into numerical order.
     */
    {
	int i;
	for (i = 0; i < NPIECES; i++)
	    qsort(placements[i].ptr, placements[i].len,
		  sizeof(*placements[i].ptr), compare_longlong);
    }

    /*
     * Sorting the lists enables us to de-duplicate them easily.
     */
    {
	int i, j, k;
	for (i = 0; i < NPIECES; i++) {
	    unsigned long long last = 0xFFFFFFFFFFFFFFFFULL;
	    for (j = k = 0; j < placements[i].len; j++)
		if (j == 0 || placements[i].ptr[j] != last)
		    placements[i].ptr[k++] = last = placements[i].ptr[j];
	    placements[i].len = k;
	}
    }

#ifdef DUMP_PLACEMENTS
    {
	int i, j;
	for (i = 0; i < NPIECES; i++) {
	    for (j = 0; j < placements[i].len; j++)
		printf("%c[%3d]: %016llx\n", pieces[i].identifier,
		       j, placements[i].ptr[j]);
	}
    }
#endif

    /*
     * Now divide each placement list into sublists based on its
     * highest set bit. We can do this by indexing into the
     * existing lists, since sorting them has already brought
     * together the elements of each sublist.
     */
    {
	int i, j, k, m;
	unsigned long long bottom, top;

	for (i = 0; i < NPIECES; i++) {
	    k = 0;
	    for (j = 0; j < 64; j++) {
		bottom = 1ULL << j;
		top = bottom + (bottom - 1);   /* carefully avoid overflow */

		m = k;
		bitplacements[i][j].ptr = placements[i].ptr + m;
		while (k < placements[i].len &&
		       placements[i].ptr[k] <= top) {
		    assert(placements[i].ptr[k] >= bottom);
		    k++;
		}
		bitplacements[i][j].len = k - m;
	    }
	}
    }

#ifdef DUMP_BITPLACEMENTS
    {
	int i, j, b;
	for (i = 0; i < NPIECES; i++) {
	    for (b = 0; b < 64; b++) {
		for (j = 0; j < bitplacements[i][b].len; j++)
		    printf("%c[%2d,%3d]: %016llx\n", pieces[i].identifier,
			   b, j, bitplacements[i][b].ptr[j]);
	    }
	}
    }
#endif

}

/*
 * The recursive function which does the main work.
 */
static void recurse(unsigned long long cube, unsigned long long *placed)
{
    /*
     * Special case: if we've finished a recursion, print the
     * solution and bottom out.
     */
    if (cube == FULL) {
	int i, bit;
	for (bit = AREA; bit-- > 0 ;) {
	    char c = '*';
	    for (i = 0; i < NPIECES; i++)
		if (placed[i] & (1ULL << bit)) {
		    assert(c == '*');
		    c = pieces[i].identifier;
		}
	    assert(c != '*');
	    putchar(c);
	}
	putchar('\n');
	fflush(stdout);
	return;
    }

    /*
     * Otherwise, find the highest unfilled bit, and go through
     * each of the unplaced pieces in turn, trying any placement
     * which fills that bit.
     */
    {
	int i, j, bit = 0;
	unsigned long long unfilled = cube ^ FULL;

	if (unfilled >= (1ULL << 32)) { unfilled >>= 32; bit += 32; }
	if (unfilled >= (1ULL << 16)) { unfilled >>= 16; bit += 16; }
	if (unfilled >= (1ULL <<  8)) { unfilled >>=  8; bit +=  8; }
	if (unfilled >= (1ULL <<  4)) { unfilled >>=  4; bit +=  4; }
	if (unfilled >= (1ULL <<  2)) { unfilled >>=  2; bit +=  2; }
	if (unfilled >= (1ULL <<  1)) { unfilled >>=  1; bit +=  1; }

	for (i = 0; i < NPIECES; i++) if (!placed[i]) {
#ifdef USE_FOLLOWFLAG
	    if (pieces[i].followflag && !placed[i-1])
		continue;
#endif
	    for (j = 0; j < bitplacements[i][bit].len; j++) {
		unsigned long long candidate = bitplacements[i][bit].ptr[j];
		if (candidate & cube)
		    continue;	       /* this placement overlaps something */
		placed[i] = candidate;
		recurse(cube | candidate, placed);
	    }
	    placed[i] = 0;
	}
    }
}

int main(void)
{
    unsigned long long placed[NPIECES];
    int i;

    setup_piecelists();
    for (i = 0; i < NPIECES; i++)
	placed[i] = 0ULL;
    recurse(0ULL, placed);

    return 0;
}

/*
 * ----------------------------------------------------------------------
 * 
 * Faintly interesting Unix commands for the solution list output from this
 * ------------------------------------------------------------------------
 *
 * My Bedlam Cube has the small piece (Z) coloured blue, and all
 * others clear. The packaging hints that the `best' solutions are
 * those which put the Z piece in the middle. So here's a grep
 * command which filters out the subset of solutions in which Z is
 * contained entirely within the central 2x2x2 cube.
 * (Interestingly, there are 99 such solutions, but the packaging
 * on my cube quotes 198, exactly twice as many. My guess is that
 * they made their count by placing the Z in the centre in a fixed
 * orientation, and enumerating the possible ways to arrange the
 * other twelve pieces around it. This would have led them to
 * overcount by a factor of two, since the Z has two-way
 * rotational symmetry about an axis of the form (1, 1, 0) which
 * they might easily have failed to notice.)
 *
 * grep -x '[^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z]..[^Z][^Z]..[^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z]..[^Z][^Z]..[^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z][^Z]'
 *
 * The T and L pieces can twine around each other in a marginally
 * cute manner, causing them to fit together inside a 2x2x3
 * cuboid. However, in my cube at least this is not a comfortable
 * position for them, because they're not well-made enough to fit
 * together _neatly_ in that configuration. So it's nice to be
 * able to grep out solutions which include that. (Of course,
 * remove the v flag to return _only_ twined solutions.)
 *
 * grep -vxE '[^TL]*...[^TL]...[^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL]...[^TL]...[^TL]*|[^TL]*..[^TL][^TL]..[^TL][^TL]..[^TL][^TL][^TL][^TL][^TL][^TL]..[^TL][^TL]..[^TL][^TL]..[^TL]*|[^TL]*..[^TL][^TL]..[^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL]..[^TL][^TL]..[^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL][^TL]..[^TL][^TL]..[^TL]*'
 *
 * Here's a quick command to take a solution in one-line form and
 * display it in four slices, for convenient replication in a
 * physical cube.
 *
 * perl -pe 's/(\S{16})/$& /g; s/(\S{4})/$& /g; s/ *$//; y/ /\n/'
 *
 */
