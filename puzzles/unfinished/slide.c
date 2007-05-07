/*
 * slide.c: Implementation of the block-sliding puzzle `Klotski'.
 */

/*
 * TODO:
 * 
 *  - Solve function:
 *     * try to generate a solution when Solve is pressed
 *        + from the start, or from here? From here, I fear.
 * 	  + hence, not much point saving the solution in an aux
 * 	    string
 *     * Inertia-like method for telling the user the solution
 *     * standalone solver which draws diagrams
 * 
 *  - The dragging semantics are still subtly wrong in complex
 *    cases.
 * 
 *  - Improve the generator.
 *     * actually, we seem to be mostly sensible already now. I
 * 	 want more choice over the type of main block and location
 * 	 of the exit/target, and I think I probably ought to give
 * 	 up on compactness and just bite the bullet and have the
 * 	 target area right outside the main wall, but mostly I
 * 	 think it's OK.
 *     * but adjust the presets, because 8x6 is _slow_ to
 * 	 generate.
 *     * also, introduce a difficulty scheme, in the form of
 * 	 limiting the minimum move count. This is obviously
 * 	 sensible, because it also speeds up generation since the
 * 	 solver can bomb out once it hits that ceiling!
 * 	  + I was going to say I'd need to work out a minimum move
 * 	    count for each grid size, but actually I think not: if
 * 	    you ask for too few moves, it just has to remove still
 * 	    more singletons, until at move count 1 you end up with
 * 	    nothing in your way at all and it SERVES YOU RIGHT!
 * 
 *  - Improve the graphics.
 *     * All the colours are a bit wishy-washy. _Some_ dark
 * 	 colours would surely not be excessive? Probably darken
 * 	 the tiles, the walls and the main block, and leave the
 * 	 target marker pale.
 *     * The cattle grid effect is still disgusting. Think of
 * 	 something completely different.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"
#include "tree234.h"

/*
 * The implementation of this game revolves around the insight
 * which makes an exhaustive-search solver feasible: although
 * there are many blocks which can be rearranged in many ways, any
 * two blocks of the same shape are _indistinguishable_ and hence
 * the number of _distinct_ board layouts is generally much
 * smaller. So we adopt a representation for board layouts which
 * is inherently canonical, i.e. there are no two distinct
 * representations which encode indistinguishable layouts.
 *
 * The way we do this is to encode each square of the board, in
 * the normal left-to-right top-to-bottom order, as being one of
 * the following things:
 *  - the first square (in the given order) of a block (`anchor')
 *  - special case of the above: the anchor for the _main_ block
 *    (i.e. the one which the aim of the game is to get to the
 *    target position)
 *  - a subsequent square of a block whose previous square was N
 *    squares ago
 *  - an impassable wall
 * 
 * (We also separately store data about which board positions are
 * forcefields only passable by the main block. We can't encode
 * that in the main board data, because then the main block would
 * destroy forcefields as it went over them.)
 *
 * Hence, for example, a 2x2 square block would be encoded as
 * ANCHOR, followed by DIST(1), and w-2 squares later on there
 * would be DIST(w-1) followed by DIST(1). So if you start at the
 * last of those squares, the DIST numbers give you a linked list
 * pointing back through all the other squares in the same block.
 *
 * So the solver simply does a bfs over all reachable positions,
 * encoding them in this format and storing them in a tree234 to
 * ensure it doesn't ever revisit an already-analysed position.
 */

enum {
    /*
     * The colours are arranged here so that every base colour is
     * directly followed by its highlight colour and then its
     * lowlight colour. Do not break this, or draw_tile() will get
     * confused.
     */
    COL_BACKGROUND,
    COL_HIGHLIGHT,
    COL_LOWLIGHT,
    COL_DRAGGING,
    COL_DRAGGING_HIGHLIGHT,
    COL_DRAGGING_LOWLIGHT,
    COL_MAIN,
    COL_MAIN_HIGHLIGHT,
    COL_MAIN_LOWLIGHT,
    COL_MAIN_DRAGGING,
    COL_MAIN_DRAGGING_HIGHLIGHT,
    COL_MAIN_DRAGGING_LOWLIGHT,
    COL_TARGET,
    COL_TARGET_HIGHLIGHT,
    COL_TARGET_LOWLIGHT,
    NCOLOURS
};

/*
 * Board layout is a simple array of bytes. Each byte holds:
 */
#define ANCHOR      255		       /* top-left-most square of some piece */
#define MAINANCHOR  254		       /* anchor of _main_ piece */
#define EMPTY       253		       /* empty square */
#define WALL        252		       /* immovable wall */
#define MAXDIST     251
/* all other values indicate distance back to previous square of same block */
#define ISDIST(x) ( (unsigned char)((x)-1) <= MAXDIST-1 )
#define DIST(x) (x)
#define ISANCHOR(x) ( (x)==ANCHOR || (x)==MAINANCHOR )
#define ISBLOCK(x) ( ISANCHOR(x) || ISDIST(x) )

/*
 * MAXDIST is the largest DIST value we can encode. This must
 * therefore also be the maximum puzzle width in theory (although
 * solver running time will dictate a much smaller limit in
 * practice).
 */
#define MAXWID MAXDIST

struct game_params {
    int w, h;
};

struct game_immutable_state {
    int refcount;
    unsigned char *forcefield;
};

struct game_state {
    int w, h;
    unsigned char *board;
    int tx, ty;			       /* target coords for MAINANCHOR */
    int minmoves;		       /* for display only */
    int lastmoved, lastmoved_pos;      /* for move counting */
    int movecount;
    int completed;
    struct game_immutable_state *imm;
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->w = 8;
    ret->h = 6;

    return ret;
}

static const struct game_params slide_presets[] = {
    {6, 5},
    {7, 5},
    {7, 6},
    {8, 6},
};

static int game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char str[80];

    if (i < 0 || i >= lenof(slide_presets))
        return FALSE;

    ret = snew(game_params);
    *ret = slide_presets[i];

    sprintf(str, "%dx%d", ret->w, ret->h);

    *name = dupstr(str);
    *params = ret;
    return TRUE;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    params->w = params->h = atoi(string);
    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
    }
}

static char *encode_params(game_params *params, int full)
{
    char data[256];

    sprintf(data, "%dx%d", params->w, params->h);

    return dupstr(data);
}

static config_item *game_configure(game_params *params)
{
    config_item *ret;
    char buf[80];

    ret = snewn(3, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].sval = dupstr(buf);
    ret[0].ival = 0;

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h);
    ret[1].sval = dupstr(buf);
    ret[1].ival = 0;

    ret[2].name = NULL;
    ret[2].type = C_END;
    ret[2].sval = NULL;
    ret[2].ival = 0;

    return ret;
}

static game_params *custom_params(config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].sval);
    ret->h = atoi(cfg[1].sval);

    return ret;
}

static char *validate_params(game_params *params, int full)
{
    if (params->w > MAXWID)
	return "Width must be at most " STR(MAXWID);

    if (params->w < 5)
	return "Width must be at least 5";
    if (params->h < 4)
	return "Height must be at least 4";

    return NULL;
}

static char *board_text_format(int w, int h, unsigned char *data,
			       unsigned char *forcefield)
{
    int wh = w*h;
    int *dsf = snew_dsf(wh);
    int i, x, y;
    int retpos, retlen = (w*2+2)*(h*2+1)+1;
    char *ret = snewn(retlen, char);

    for (i = 0; i < wh; i++)
	if (ISDIST(data[i]))
	    dsf_merge(dsf, i - data[i], i);
    retpos = 0;
    for (y = 0; y < 2*h+1; y++) {
	for (x = 0; x < 2*w+1; x++) {
	    int v;
	    int i = (y/2)*w+(x/2);

#define dtype(i) (ISBLOCK(data[i]) ? \
		  dsf_canonify(dsf, i) : data[i])
#define dchar(t) ((t)==EMPTY ? ' ' : (t)==WALL ? '#' : \
		  data[t] == MAINANCHOR ? '*' : '%')

	    if (y % 2 && x % 2) {
		int j = dtype(i);
		v = dchar(j);
	    } else if (y % 2 && !(x % 2)) {
		int j1 = (x > 0 ? dtype(i-1) : -1);
		int j2 = (x < 2*w ? dtype(i) : -1);
		if (j1 != j2)
		    v = '|';
		else
		    v = dchar(j1);
	    } else if (!(y % 2) && (x % 2)) {
		int j1 = (y > 0 ? dtype(i-w) : -1);
		int j2 = (y < 2*h ? dtype(i) : -1);
		if (j1 != j2)
		    v = '-';
		else
		    v = dchar(j1);
	    } else {
		int j1 = (x > 0 && y > 0 ? dtype(i-w-1) : -1);
		int j2 = (x > 0 && y < 2*h ? dtype(i-1) : -1);
		int j3 = (x < 2*w && y > 0 ? dtype(i-w) : -1);
		int j4 = (x < 2*w && y < 2*h ? dtype(i) : -1);
		if (j1 == j2 && j2 == j3 && j3 == j4)
		    v = dchar(j1);
		else if (j1 == j2 && j3 == j4)
		    v = '|';
		else if (j1 == j3 && j2 == j4)
		    v = '-';
		else
		    v = '+';
	    }

	    assert(retpos < retlen);
	    ret[retpos++] = v;
	}
	assert(retpos < retlen);
	ret[retpos++] = '\n';
    }
    assert(retpos < retlen);
    ret[retpos++] = '\0';
    assert(retpos == retlen);

    return ret;
}

/* ----------------------------------------------------------------------
 * Solver.
 */

/*
 * During solver execution, the set of visited board positions is
 * stored as a tree234 of the following structures. `w', `h' and
 * `data' are obvious in meaning; `dist' represents the minimum
 * distance to reach this position from the starting point.
 * 
 * `prev' links each board to the board position from which it was
 * most efficiently derived.
 */
struct board {
    int w, h;
    int dist;
    struct board *prev;
    unsigned char *data;
};

static int boardcmp(void *av, void *bv)
{
    struct board *a = (struct board *)av;
    struct board *b = (struct board *)bv;
    return memcmp(a->data, b->data, a->w * a->h);
}

static struct board *newboard(int w, int h, unsigned char *data)
{
    struct board *b = malloc(sizeof(struct board) + w*h);
    b->data = (unsigned char *)b + sizeof(struct board);
    memcpy(b->data, data, w*h);
    b->w = w;
    b->h = h;
    b->dist = -1;
    b->prev = NULL;
    return b;
}

/*
 * The actual solver. Given a board, attempt to find the minimum
 * length of move sequence which moves MAINANCHOR to (tx,ty), or
 * -1 if no solution exists. Returns that minimum length, and
 * (FIXME) optionally also writes out the actual moves into an
 * as-yet-unprovided parameter.
 */
static int solve_board(int w, int h, unsigned char *board,
		       unsigned char *forcefield, int tx, int ty)
{
    int wh = w*h;
    struct board *b, *b2, *b3;
    int *next, *anchors, *which;
    int *movereached, *movequeue, mqhead, mqtail;
    tree234 *sorted, *queue;
    int i, j, dir;
    int qlen, lastdist;
    int ret;

#ifdef SOLVER_DIAGNOSTICS
    {
	char *t = board_text_format(w, h, board);
	for (i = 0; i < h; i++) {
	    for (j = 0; j < w; j++) {
		int c = board[i*w+j];
		if (ISDIST(c))
		    printf("D%-3d", c);
		else if (c == MAINANCHOR)
		    printf("M   ");
		else if (c == ANCHOR)
		    printf("A   ");
		else if (c == WALL)
		    printf("W   ");
		else if (c == EMPTY)
		    printf("E   ");
	    }
	    printf("\n");
	}
	
	printf("Starting solver for:\n%s\n", t);
	sfree(t);
    }
#endif

    sorted = newtree234(boardcmp);
    queue = newtree234(NULL);

    b = newboard(w, h, board);
    b->dist = 0;
    add234(sorted, b);
    addpos234(queue, b, 0);
    qlen = 1;

    next = snewn(wh, int);
    anchors = snewn(wh, int);
    which = snewn(wh, int);
    movereached = snewn(wh, int);
    movequeue = snewn(wh, int);
    lastdist = -1;

    while ((b = delpos234(queue, 0)) != NULL) {
	qlen--;
	if (b->dist != lastdist) {
#ifdef SOLVER_DIAGNOSTICS
	    printf("dist %d (%d)\n", b->dist, count234(sorted));
#endif
	    lastdist = b->dist;
	}
	/*
	 * Find all the anchors and form a linked list of the
	 * squares within each block.
	 */
	for (i = 0; i < wh; i++) {
	    next[i] = -1;
	    anchors[i] = FALSE;
	    which[i] = -1;
	    if (ISANCHOR(b->data[i])) {
		anchors[i] = TRUE;
		which[i] = i;
	    } else if (ISDIST(b->data[i])) {
		j = i - b->data[i];
		next[j] = i;
		which[i] = which[j];
	    }
	}

	/*
	 * For each anchor, do an array-based BFS to find all the
	 * places we can slide it to.
	 */
	for (i = 0; i < wh; i++) {
	    if (!anchors[i])
		continue;

	    mqhead = mqtail = 0;
	    for (j = 0; j < wh; j++)
		movereached[j] = FALSE;
	    movequeue[mqtail++] = i;
	    while (mqhead < mqtail) {
		int pos = movequeue[mqhead++];

		/*
		 * Try to move in each direction from here.
		 */
		for (dir = 0; dir < 4; dir++) {
		    int dx = (dir == 0 ? -1 : dir == 1 ? +1 : 0);
		    int dy = (dir == 2 ? -1 : dir == 3 ? +1 : 0);
		    int offset = dy*w + dx;
		    int newpos = pos + offset;
		    int d = newpos - i;

		    /*
		     * For each square involved in this block,
		     * check to see if the square d spaces away
		     * from it is either empty or part of the same
		     * block.
		     */
		    for (j = i; j >= 0; j = next[j]) {
			int jy = (pos+j-i) / w + dy, jx = (pos+j-i) % w + dx;
			if (jy >= 0 && jy < h && jx >= 0 && jx < w &&
			    ((b->data[j+d] == EMPTY || which[j+d] == i) &&
			     (b->data[i] == MAINANCHOR || !forcefield[j+d])))
			    /* ok */;
			else
			    break;
		    }
		    if (j >= 0)
			continue;	       /* this direction wasn't feasible */

		    /*
		     * If we've already tried moving this piece
		     * here, leave it.
		     */
		    if (movereached[newpos])
			continue;
		    movereached[newpos] = TRUE;
		    movequeue[mqtail++] = newpos;

		    /*
		     * We have a viable move. Make it.
		     */
		    b2 = newboard(w, h, b->data);
		    for (j = i; j >= 0; j = next[j])
			b2->data[j] = EMPTY;
		    for (j = i; j >= 0; j = next[j])
			b2->data[j+d] = b->data[j];

		    b3 = add234(sorted, b2);
		    if (b3 != b2) {
			sfree(b2);	       /* we already got one */
		    } else {
			b2->dist = b->dist + 1;
			b2->prev = b;
			addpos234(queue, b2, qlen++);
			if (b2->data[ty*w+tx] == MAINANCHOR)
			    goto done;     /* search completed! */
		    }
		}
	    }
	}
    }
    b2 = NULL;

    done:

    if (b2)
	ret = b2->dist;
    else
	ret = -1;		       /* no solution */

    freetree234(queue);

    while ((b = delpos234(sorted, 0)) != NULL)
	sfree(b);
    freetree234(sorted);

    sfree(next);
    sfree(anchors);
    sfree(movereached);
    sfree(movequeue);
    sfree(which);

    return ret;
}

/* ----------------------------------------------------------------------
 * Random board generation.
 */

static void generate_board(int w, int h, int *rtx, int *rty, int *minmoves,
			   random_state *rs, unsigned char **rboard,
			   unsigned char **rforcefield)
{
    int wh = w*h;
    unsigned char *board, *board2, *forcefield;
    unsigned char *tried_merge;
    int *dsf;
    int *list, nlist, pos;
    int tx, ty;
    int i, j;
    int moves;

    /*
     * Set up a board and fill it with singletons, except for a
     * border of walls.
     */
    board = snewn(wh, unsigned char);
    forcefield = snewn(wh, unsigned char);
    board2 = snewn(wh, unsigned char);
    memset(board, ANCHOR, wh);
    memset(forcefield, FALSE, wh);
    for (i = 0; i < w; i++)
	board[i] = board[i+w*(h-1)] = WALL;
    for (i = 0; i < h; i++)
	board[i*w] = board[i*w+(w-1)] = WALL;

    tried_merge = snewn(wh * wh, unsigned char);
    memset(tried_merge, 0, wh*wh);
    dsf = snew_dsf(wh);

    /*
     * Invent a main piece at one extreme. (FIXME: vary the
     * extreme, and the piece.)
     */
    board[w+1] = MAINANCHOR;
    board[w+2] = DIST(1);
    board[w*2+1] = DIST(w-1);
    board[w*2+2] = DIST(1);

    /*
     * Invent a target position. (FIXME: vary this too.)
     */
    tx = w-2;
    ty = h-3;
    forcefield[ty*w+tx+1] = forcefield[(ty+1)*w+tx+1] = TRUE;
    board[ty*w+tx+1] = board[(ty+1)*w+tx+1] = EMPTY;

    /*
     * Gradually remove singletons until the game becomes soluble.
     */
    for (j = w; j-- > 0 ;)
	for (i = h; i-- > 0 ;)
	    if (board[i*w+j] == ANCHOR) {
		/*
		 * See if the board is already soluble.
		 */
		if ((moves = solve_board(w, h, board, forcefield,
					 tx, ty)) >= 0)
		    goto soluble;

		/*
		 * Otherwise, remove this piece.
		 */
		board[i*w+j] = EMPTY;
	    }
    assert(!"We shouldn't get here");
    soluble:

    /*
     * Make a list of all the inter-block edges on the board.
     */
    list = snewn(wh*2, int);
    nlist = 0;
    for (i = 0; i+1 < w; i++)
	for (j = 0; j < h; j++)
	    list[nlist++] = (j*w+i) * 2 + 0;   /* edge to the right of j*w+i */
    for (j = 0; j+1 < h; j++)
	for (i = 0; i < w; i++)
	    list[nlist++] = (j*w+i) * 2 + 1;   /* edge below j*w+i */

    /*
     * Now go through that list in random order, trying to merge
     * the blocks on each side of each edge.
     */
    shuffle(list, nlist, sizeof(*list), rs);
    while (nlist > 0) {
	int x1, y1, p1, c1;
	int x2, y2, p2, c2;

	pos = list[--nlist];
	y1 = y2 = pos / (w*2);
	x1 = x2 = (pos / 2) % w;
	if (pos % 2)
	    y2++;
	else
	    x2++;
	p1 = y1*w+x1;
	p2 = y2*w+x2;

	/*
	 * Immediately abandon the attempt if we've already tried
	 * to merge the same pair of blocks along a different
	 * edge.
	 */
	c1 = dsf_canonify(dsf, p1);
	c2 = dsf_canonify(dsf, p2);
	if (tried_merge[c1 * wh + c2])
{printf("aha\n");
	    continue;
}

	/*
	 * In order to be mergeable, these two squares must each
	 * either be, or belong to, a non-main anchor, and their
	 * anchors must also be distinct.
	 */
	if (!ISBLOCK(board[p1]) || !ISBLOCK(board[p2]))
	    continue;
	while (ISDIST(board[p1]))
	    p1 -= board[p1];
	while (ISDIST(board[p2]))
	    p2 -= board[p2];
	if (board[p1] == MAINANCHOR || board[p2] == MAINANCHOR || p1 == p2)
	    continue;

	/*
	 * We can merge these blocks. Try it, and see if the
	 * puzzle remains soluble.
	 */
	memcpy(board2, board, wh);
	j = -1;
	while (p1 < wh || p2 < wh) {
	    /*
	     * p1 and p2 are the squares at the head of each block
	     * list. Pick the smaller one and put it on the output
	     * block list.
	     */
	    i = min(p1, p2);
	    if (j < 0) {
		board[i] = ANCHOR;
	    } else {
		assert(i - j <= MAXDIST);
		board[i] = DIST(i - j);
	    }
	    j = i;

	    /*
	     * Now advance whichever list that came from.
	     */
	    if (i == p1) {
		do {
		    p1++;
		} while (p1 < wh && board[p1] != DIST(p1-i));
	    } else {
		do {
		    p2++;
		} while (p2 < wh && board[p2] != DIST(p2-i));
	    }
	}
	j = solve_board(w, h, board, forcefield, tx, ty);
	if (j < 0) {
	    /*
	     * Didn't work. Revert the merge.
	     */
	    memcpy(board, board2, wh);
	    tried_merge[c1 * wh + c2] = tried_merge[c2 * wh + c1] = TRUE;
	} else {
	    int c;

	    moves = j;

	    dsf_merge(dsf, c1, c2);
	    c = dsf_canonify(dsf, c1);
	    for (i = 0; i < wh; i++)
		tried_merge[c*wh+i] = (tried_merge[c1*wh+i] |
				       tried_merge[c2*wh+i]);
	    for (i = 0; i < wh; i++)
		tried_merge[i*wh+c] = (tried_merge[i*wh+c1] |
				       tried_merge[i*wh+c2]);
	}
    }

    sfree(board2);

    *rtx = tx;
    *rty = ty;
    *rboard = board;
    *rforcefield = forcefield;
    *minmoves = moves;
}

/* ----------------------------------------------------------------------
 * End of solver/generator code.
 */

static char *new_game_desc(game_params *params, random_state *rs,
			   char **aux, int interactive)
{
    int w = params->w, h = params->h, wh = w*h;
    int tx, ty, minmoves;
    unsigned char *board, *forcefield;
    char *ret, *p;
    int i;

    generate_board(params->w, params->h, &tx, &ty, &minmoves, rs,
		   &board, &forcefield);
#ifdef GENERATOR_DIAGNOSTICS
    {
	char *t = board_text_format(params->w, params->h, board);
	printf("%s\n", t);
	sfree(t);
    }
#endif

    /*
     * Encode as a game ID.
     */
    ret = snewn(wh * 6 + 40, char);
    p = ret;
    i = 0;
    while (i < wh) {
	if (ISDIST(board[i])) {
	    p += sprintf(p, "d%d", board[i]);
	    i++;
	} else {
	    int count = 1;
	    int b = board[i], f = forcefield[i];
	    int c = (b == ANCHOR ? 'a' :
		     b == MAINANCHOR ? 'm' :
		     b == EMPTY ? 'e' :
		     /* b == WALL ? */ 'w');
	    if (f) *p++ = 'f';
	    *p++ = c;
	    i++;
	    while (i < wh && board[i] == b && forcefield[i] == f)
		i++, count++;
	    if (count > 1)
		p += sprintf(p, "%d", count);
	}
    }
    p += sprintf(p, ",%d,%d,%d", tx, ty, minmoves);
    ret = sresize(ret, p+1 - ret, char);

    /*
     * FIXME: generate an aux string
     */

    sfree(board);
    sfree(forcefield);

    return ret;
}

static char *validate_desc(game_params *params, char *desc)
{
    int w = params->w, h = params->h, wh = w*h;
    int *active, *link;
    int mains = 0, mpos = -1;
    int i, j, tx, ty, minmoves;
    char *ret;

    active = snewn(wh, int);
    link = snewn(wh, int);
    i = 0;

    while (*desc && *desc != ',') {
	if (i >= wh) {
	    ret = "Too much data in game description";
	    goto done;
	}
	link[i] = -1;
	active[i] = FALSE;
	if (*desc == 'f' || *desc == 'F') {
	    desc++;
	    if (!*desc) {
		ret = "Expected another character after 'f' in game "
		    "description";
		goto done;
	    }
	}

	if (*desc == 'd' || *desc == 'D') {
	    int dist;

	    desc++;
	    if (!isdigit((unsigned char)*desc)) {
		ret = "Expected a number after 'd' in game description";
		goto done;
	    }
	    dist = atoi(desc);
	    while (*desc && isdigit((unsigned char)*desc)) desc++;

	    if (dist <= 0 || dist > i) {
		ret = "Out-of-range number after 'd' in game description";
		goto done;
	    }

	    if (!active[i - dist]) {
		ret = "Invalid back-reference in game description";
		goto done;
	    }

	    link[i] = i - dist;
	    for (j = i; j > 0; j = link[j])
		if (j == i-1 || j == i-w)
		    break;
	    if (j < 0) {
		ret = "Disconnected piece in game description";
		goto done;
	    }

	    active[i] = TRUE;
	    active[link[i]] = FALSE;
	    i++;
	} else {
	    int c = *desc++;
	    int count = 1;

	    if (!strchr("aAmMeEwW", c)) {
		ret = "Invalid character in game description";
		goto done;
	    }
	    if (isdigit((unsigned char)*desc)) {
		count = atoi(desc);
		while (*desc && isdigit((unsigned char)*desc)) desc++;
	    }
	    if (i + count > wh) {
		ret = "Too much data in game description";
		goto done;
	    }
	    while (count-- > 0) {
		active[i] = (strchr("aAmM", c) != NULL);
		link[i] = -1;
		if (strchr("mM", c) != NULL) {
		    mains++;
		    mpos = i;
		}
		i++;
	    }
	}
    }
    if (mains != 1) {
	ret = (mains == 0 ? "No main piece specified in game description" :
	       "More than one main piece specified in game description");
	goto done;
    }
    if (i < wh) {
	ret = "Not enough data in game description";
	goto done;
    }

    /*
     * Now read the target coordinates.
     */
    i = sscanf(desc, ",%d,%d,%d", &tx, &ty, &minmoves);
    if (i < 2) {
	ret = "No target coordinates specified";
	goto done;
	/*
	 * (but minmoves is optional)
	 */
    }

    ret = NULL;

    done:
    sfree(active);
    sfree(link);
    return ret;
}

static game_state *new_game(midend *me, game_params *params, char *desc)
{
    int w = params->w, h = params->h, wh = w*h;
    game_state *state;
    int i;

    state = snew(game_state);
    state->w = w;
    state->h = h;
    state->board = snewn(wh, unsigned char);
    state->lastmoved = state->lastmoved_pos = -1;
    state->movecount = 0;
    state->imm = snew(struct game_immutable_state);
    state->imm->refcount = 1;
    state->imm->forcefield = snewn(wh, unsigned char);

    i = 0;

    while (*desc && *desc != ',') {
	int f = FALSE;

	assert(i < wh);

	if (*desc == 'f') {
	    f = TRUE;
	    desc++;
	    assert(*desc);
	}

	if (*desc == 'd' || *desc == 'D') {
	    int dist;

	    desc++;
	    dist = atoi(desc);
	    while (*desc && isdigit((unsigned char)*desc)) desc++;

	    state->board[i] = DIST(dist);
	    state->imm->forcefield[i] = f;

	    i++;
	} else {
	    int c = *desc++;
	    int count = 1;

	    if (isdigit((unsigned char)*desc)) {
		count = atoi(desc);
		while (*desc && isdigit((unsigned char)*desc)) desc++;
	    }
	    assert(i + count <= wh);

	    c = (c == 'a' || c == 'A' ? ANCHOR :
		 c == 'm' || c == 'M' ? MAINANCHOR :
		 c == 'e' || c == 'E' ? EMPTY :
		 /* c == 'w' || c == 'W' ? */ WALL);		 

	    while (count-- > 0) {
		state->board[i] = c;
		state->imm->forcefield[i] = f;
		i++;
	    }
	}
    }

    /*
     * Now read the target coordinates.
     */
    state->tx = state->ty = 0;
    state->minmoves = -1;
    i = sscanf(desc, ",%d,%d,%d", &state->tx, &state->ty, &state->minmoves);

    if (state->board[state->ty*w+state->tx] == MAINANCHOR)
	state->completed = 0;	       /* already complete! */
    else
	state->completed = -1;

    return state;
}

static game_state *dup_game(game_state *state)
{
    int w = state->w, h = state->h, wh = w*h;
    game_state *ret = snew(game_state);

    ret->w = state->w;
    ret->h = state->h;
    ret->board = snewn(wh, unsigned char);
    memcpy(ret->board, state->board, wh);
    ret->tx = state->tx;
    ret->ty = state->ty;
    ret->minmoves = state->minmoves;
    ret->lastmoved = state->lastmoved;
    ret->lastmoved_pos = state->lastmoved_pos;
    ret->movecount = state->movecount;
    ret->completed = state->completed;
    ret->imm = state->imm;
    ret->imm->refcount++;

    return ret;
}

static void free_game(game_state *state)
{
    if (--state->imm->refcount <= 0) {
	sfree(state->imm->forcefield);
	sfree(state->imm);
    }
    sfree(state->board);
    sfree(state);
}

static char *solve_game(game_state *state, game_state *currstate,
			char *aux, char **error)
{
    /*
     * FIXME: we have a solver, so use it
     * 
     * FIXME: we should have generated an aux string, so use that
     */
    return NULL;
}

static char *game_text_format(game_state *state)
{
    return board_text_format(state->w, state->h, state->board,
			     state->imm->forcefield);
}

struct game_ui {
    int dragging;
    int drag_anchor;
    int drag_offset_x, drag_offset_y;
    int drag_currpos;
    unsigned char *reachable;
    int *bfs_queue;		       /* used as scratch in interpret_move */
};

static game_ui *new_ui(game_state *state)
{
    int w = state->w, h = state->h, wh = w*h;
    game_ui *ui = snew(game_ui);

    ui->dragging = FALSE;
    ui->drag_anchor = ui->drag_currpos = -1;
    ui->drag_offset_x = ui->drag_offset_y = -1;
    ui->reachable = snewn(wh, unsigned char);
    memset(ui->reachable, 0, wh);
    ui->bfs_queue = snewn(wh, int);

    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui->bfs_queue);
    sfree(ui->reachable);
    sfree(ui);
}

static char *encode_ui(game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, char *encoding)
{
}

static void game_changed_state(game_ui *ui, game_state *oldstate,
                               game_state *newstate)
{
}

#define PREFERRED_TILESIZE 32
#define TILESIZE (ds->tilesize)
#define BORDER (TILESIZE/2)
#define COORD(x)  ( (x) * TILESIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + TILESIZE) / TILESIZE - 1 )
#define BORDER_WIDTH (1 + TILESIZE/20)
#define HIGHLIGHT_WIDTH (1 + TILESIZE/16)

#define FLASH_INTERVAL 0.10F
#define FLASH_TIME 3*FLASH_INTERVAL

struct game_drawstate {
    int tilesize;
    int w, h;
    unsigned long *grid;	       /* what's currently displayed */
    int started;
};

static char *interpret_move(game_state *state, game_ui *ui, game_drawstate *ds,
			    int x, int y, int button)
{
    int w = state->w, h = state->h, wh = w*h;
    int tx, ty, i, j;
    int qhead, qtail;

    if (button == LEFT_BUTTON) {
	tx = FROMCOORD(x);
	ty = FROMCOORD(y);

	if (tx < 0 || tx >= w || ty < 0 || ty >= h ||
	    !ISBLOCK(state->board[ty*w+tx]))
	    return NULL;	       /* this click has no effect */

	/*
	 * User has clicked on a block. Find the block's anchor
	 * and register that we've started dragging it.
	 */
	i = ty*w+tx;
	while (ISDIST(state->board[i]))
	    i -= state->board[i];
	assert(i >= 0 && i < wh);

	ui->dragging = TRUE;
	ui->drag_anchor = i;
	ui->drag_offset_x = tx - (i % w);
	ui->drag_offset_y = ty - (i / w);
	ui->drag_currpos = i;

	/*
	 * Now we immediately bfs out from the current location of
	 * the anchor, to find all the places to which this block
	 * can be dragged.
	 */
	memset(ui->reachable, FALSE, wh);
	qhead = qtail = 0;
	ui->reachable[i] = TRUE;
	ui->bfs_queue[qtail++] = i;
	for (j = i; j < wh; j++)
	    if (state->board[j] == DIST(j - i))
		i = j;
	while (qhead < qtail) {
	    int pos = ui->bfs_queue[qhead++];
	    int x = pos % w, y = pos / w;
	    int dir;

	    for (dir = 0; dir < 4; dir++) {
		int dx = (dir == 0 ? -1 : dir == 1 ? +1 : 0);
		int dy = (dir == 2 ? -1 : dir == 3 ? +1 : 0);
		int newpos;

		if (x + dx < 0 || x + dx >= w ||
		    y + dy < 0 || y + dy >= h)
		    continue;

		newpos = pos + dy*w + dx;
		if (ui->reachable[newpos])
		    continue;	       /* already done this one */

		/*
		 * Now search the grid to see if the block we're
		 * dragging could fit into this space.
		 */
		for (j = i; j >= 0; j = (ISDIST(state->board[j]) ?
					 j - state->board[j] : -1)) {
		    int jx = (j+pos-ui->drag_anchor) % w;
		    int jy = (j+pos-ui->drag_anchor) / w;
		    int j2;

		    if (jx + dx < 0 || jx + dx >= w ||
			jy + dy < 0 || jy + dy >= h)
			break;	       /* this position isn't valid at all */

		    j2 = (j+pos-ui->drag_anchor) + dy*w + dx;

		    if (state->board[j2] == EMPTY &&
			(!state->imm->forcefield[j2] ||
			 state->board[ui->drag_anchor] == MAINANCHOR))
			continue;
		    while (ISDIST(state->board[j2]))
			j2 -= state->board[j2];
		    assert(j2 >= 0 && j2 < wh);
		    if (j2 == ui->drag_anchor)
			continue;
		    else
			break;
		}

		if (j < 0) {
		    /*
		     * If we got to the end of that loop without
		     * disqualifying this position, mark it as
		     * reachable for this drag.
		     */
		    ui->reachable[newpos] = TRUE;
		    ui->bfs_queue[qtail++] = newpos;
		}
	    }
	}

	/*
	 * And that's it. Update the display to reflect the start
	 * of a drag.
	 */
	return "";
    } else if (button == LEFT_DRAG && ui->dragging) {
	tx = FROMCOORD(x);
	ty = FROMCOORD(y);

	tx -= ui->drag_offset_x;
	ty -= ui->drag_offset_y;
	if (tx < 0 || tx >= w || ty < 0 || ty >= h ||
	    !ui->reachable[ty*w+tx])
	    return NULL;	       /* this drag has no effect */

	ui->drag_currpos = ty*w+tx;
	return "";
    } else if (button == LEFT_RELEASE && ui->dragging) {
	char data[256], *str;

	/*
	 * Terminate the drag, and if the piece has actually moved
	 * then return a move string quoting the old and new
	 * locations of the piece's anchor.
	 */
	if (ui->drag_anchor != ui->drag_currpos) {
	    sprintf(data, "M%d-%d", ui->drag_anchor, ui->drag_currpos);
	    str = dupstr(data);
	} else
	    str = "";		       /* null move; just update the UI */
	
	ui->dragging = FALSE;
	ui->drag_anchor = ui->drag_currpos = -1;
	ui->drag_offset_x = ui->drag_offset_y = -1;
	memset(ui->reachable, 0, wh);

	return str;
    }

    return NULL;
}

static int move_piece(int w, int h, const unsigned char *src,
		      unsigned char *dst, unsigned char *ff, int from, int to)
{
    int wh = w*h;
    int i, j;

    if (!ISANCHOR(dst[from]))
	return FALSE;

    /*
     * Scan to the far end of the piece's linked list.
     */
    for (i = j = from; j < wh; j++)
	if (src[j] == DIST(j - i))
	    i = j;

    /*
     * Remove the piece from its old location in the new
     * game state.
     */
    for (j = i; j >= 0; j = (ISDIST(src[j]) ? j - src[j] : -1))
	dst[j] = EMPTY;

    /*
     * And put it back in at the new location.
     */
    for (j = i; j >= 0; j = (ISDIST(src[j]) ? j - src[j] : -1)) {
	int jn = j + to - from;
	if (jn < 0 || jn >= wh)
	    return FALSE;
	if (dst[jn] == EMPTY && (!ff[jn] || src[from] == MAINANCHOR)) {
	    dst[jn] = src[j];
	} else {
	    return FALSE;
	}
    }

    return TRUE;
}

static game_state *execute_move(game_state *state, char *move)
{
    int w = state->w, h = state->h /* , wh = w*h */;
    char c;
    int a1, a2, n;
    game_state *ret = dup_game(state);

    while (*move) {
        c = *move;
	if (c == 'M') {
            move++;
            if (sscanf(move, "%d-%d%n", &a1, &a2, &n) != 2 ||
		!move_piece(w, h, state->board, ret->board,
			    state->imm->forcefield, a1, a2)) {
                free_game(ret);
                return NULL;
            }
	    if (a1 == ret->lastmoved) {
		/*
		 * If the player has moved the same piece as they
		 * moved last time, don't increment the move
		 * count. In fact, if they've put the piece back
		 * where it started from, _decrement_ the move
		 * count.
		 */
		if (a2 == ret->lastmoved_pos) {
		    ret->movecount--;  /* reverted last move */
		    ret->lastmoved = ret->lastmoved_pos = -1;
		} else {
		    ret->lastmoved = a2;
		    /* don't change lastmoved_pos */
		}
	    } else {
		ret->lastmoved = a2;
		ret->lastmoved_pos = a1;
		ret->movecount++;
	    }
	    if (ret->board[a2] == MAINANCHOR &&
		a2 == ret->ty * w + ret->tx && ret->completed < 0)
		ret->completed = ret->movecount;
            move += n;
        } else {
            free_game(ret);
            return NULL;
        }
        if (*move == ';')
            move++;
        else if (*move) {
            free_game(ret);
            return NULL;
        }
    }

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(game_params *params, int tilesize,
			      int *x, int *y)
{
    /* fool the macros */
    struct dummy { int tilesize; } dummy = { tilesize }, *ds = &dummy;

    *x = params->w * TILESIZE + 2*BORDER;
    *y = params->h * TILESIZE + 2*BORDER;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
			  game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static void raise_colour(float *target, float *src, float *limit)
{
    int i;
    for (i = 0; i < 3; i++)
	target[i] = (2*src[i] + limit[i]) / 3;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    game_mkhighlight(fe, ret, COL_BACKGROUND, COL_HIGHLIGHT, COL_LOWLIGHT);

    /*
     * When dragging a tile, we light it up a bit.
     */
    raise_colour(ret+3*COL_DRAGGING,
		 ret+3*COL_BACKGROUND, ret+3*COL_HIGHLIGHT);
    raise_colour(ret+3*COL_DRAGGING_HIGHLIGHT,
		 ret+3*COL_HIGHLIGHT, ret+3*COL_HIGHLIGHT);
    raise_colour(ret+3*COL_DRAGGING_LOWLIGHT,
		 ret+3*COL_LOWLIGHT, ret+3*COL_HIGHLIGHT);

    /*
     * The main tile is tinted blue.
     */
    ret[COL_MAIN * 3 + 0] = ret[COL_BACKGROUND * 3 + 0];
    ret[COL_MAIN * 3 + 1] = ret[COL_BACKGROUND * 3 + 1];
    ret[COL_MAIN * 3 + 2] = ret[COL_HIGHLIGHT * 3 + 2];
    game_mkhighlight_specific(fe, ret, COL_MAIN,
			      COL_MAIN_HIGHLIGHT, COL_MAIN_LOWLIGHT);

    /*
     * And we light that up a bit too when dragging.
     */
    raise_colour(ret+3*COL_MAIN_DRAGGING,
		 ret+3*COL_MAIN, ret+3*COL_MAIN_HIGHLIGHT);
    raise_colour(ret+3*COL_MAIN_DRAGGING_HIGHLIGHT,
		 ret+3*COL_MAIN_HIGHLIGHT, ret+3*COL_MAIN_HIGHLIGHT);
    raise_colour(ret+3*COL_MAIN_DRAGGING_LOWLIGHT,
		 ret+3*COL_MAIN_LOWLIGHT, ret+3*COL_MAIN_HIGHLIGHT);

    /*
     * The target area on the floor is tinted green.
     */
    ret[COL_TARGET * 3 + 0] = ret[COL_BACKGROUND * 3 + 0];
    ret[COL_TARGET * 3 + 1] = ret[COL_HIGHLIGHT * 3 + 1];
    ret[COL_TARGET * 3 + 2] = ret[COL_BACKGROUND * 3 + 2];
    game_mkhighlight_specific(fe, ret, COL_TARGET,
			      COL_TARGET_HIGHLIGHT, COL_TARGET_LOWLIGHT);

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, game_state *state)
{
    int w = state->w, h = state->h, wh = w*h;
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i;

    ds->tilesize = 0;
    ds->w = w;
    ds->h = h;
    ds->started = FALSE;
    ds->grid = snewn(wh, unsigned long);
    for (i = 0; i < wh; i++)
	ds->grid[i] = ~(unsigned long)0;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->grid);
    sfree(ds);
}

#define BG_NORMAL       0x00000001UL
#define BG_TARGET       0x00000002UL
#define BG_FORCEFIELD   0x00000004UL
#define FLASH_LOW       0x00000008UL
#define FLASH_HIGH      0x00000010UL
#define FG_WALL         0x00000020UL
#define FG_MAIN         0x00000040UL
#define FG_NORMAL       0x00000080UL
#define FG_DRAGGING     0x00000100UL
#define FG_LBORDER      0x00000200UL
#define FG_TBORDER      0x00000400UL
#define FG_RBORDER      0x00000800UL
#define FG_BBORDER      0x00001000UL
#define FG_TLCORNER     0x00002000UL
#define FG_TRCORNER     0x00004000UL
#define FG_BLCORNER     0x00008000UL
#define FG_BRCORNER     0x00010000UL

/*
 * Utility function.
 */
#define TYPE_MASK 0xF000
#define COL_MASK 0x0FFF
#define TYPE_RECT 0x0000
#define TYPE_TLCIRC 0x4000
#define TYPE_TRCIRC 0x5000
#define TYPE_BLCIRC 0x6000
#define TYPE_BRCIRC 0x7000
static void maybe_rect(drawing *dr, int x, int y, int w, int h, int coltype)
{
    int colour = coltype & COL_MASK, type = coltype & TYPE_MASK;

    if (colour > NCOLOURS)
	return;
    if (type == TYPE_RECT) {
	draw_rect(dr, x, y, w, h, colour);
    } else {
	int cx, cy, r;

	clip(dr, x, y, w, h);

	cx = x;
	cy = y;
	assert(w == h);
	r = w-1;
	if (type & 0x1000)
	    cx += r;
	if (type & 0x2000)
	    cy += r;
	draw_circle(dr, cx, cy, r, colour, colour);

	unclip(dr);
    }
}

static void draw_tile(drawing *dr, game_drawstate *ds,
		      int x, int y, unsigned long val)
{
    int tx = COORD(x), ty = COORD(y);
    int cc, ch, cl;

    /*
     * Draw the tile background.
     */
    if (val & BG_TARGET)
	cc = COL_TARGET;
    else
	cc = COL_BACKGROUND;
    ch = cc+1;
    cl = cc+2;
    if (val & FLASH_LOW)
	cc = cl;
    else if (val & FLASH_HIGH)
	cc = ch;

    draw_rect(dr, tx, ty, TILESIZE, TILESIZE, cc);
    if (val & BG_FORCEFIELD) {
	/*
	 * Cattle-grid effect to indicate that nothing but the
	 * main block can slide over this square.
	 */
	int n = 3 * (TILESIZE / (3*HIGHLIGHT_WIDTH));
	int i;

	for (i = 1; i < n; i += 3) {
	    draw_rect(dr, tx,ty+(TILESIZE*i/n), TILESIZE,HIGHLIGHT_WIDTH, cl);
	    draw_rect(dr, tx+(TILESIZE*i/n),ty, HIGHLIGHT_WIDTH,TILESIZE, cl);
	}
    }

    /*
     * Draw the tile foreground, i.e. some section of a block or
     * wall.
     */
    if (val & FG_WALL) {
	cc = COL_BACKGROUND;
	ch = cc+1;
	cl = cc+2;
	if (val & FLASH_LOW)
	    cc = cl;
	else if (val & FLASH_HIGH)
	    cc = ch;

	draw_rect(dr, tx, ty, TILESIZE, TILESIZE, cc);
	if (val & FG_LBORDER)
	    draw_rect(dr, tx, ty, HIGHLIGHT_WIDTH, TILESIZE,
		      ch);
	if (val & FG_RBORDER)
	    draw_rect(dr, tx+TILESIZE-HIGHLIGHT_WIDTH, ty,
		      HIGHLIGHT_WIDTH, TILESIZE, cl);
	if (val & FG_TBORDER)
	    draw_rect(dr, tx, ty, TILESIZE, HIGHLIGHT_WIDTH, ch);
	if (val & FG_BBORDER)
	    draw_rect(dr, tx, ty+TILESIZE-HIGHLIGHT_WIDTH,
		      TILESIZE, HIGHLIGHT_WIDTH, cl);
	if (!((FG_BBORDER | FG_LBORDER) &~ val))
	    draw_rect(dr, tx, ty+TILESIZE-HIGHLIGHT_WIDTH,
		      HIGHLIGHT_WIDTH, HIGHLIGHT_WIDTH, cc);
	if (!((FG_TBORDER | FG_RBORDER) &~ val))
	    draw_rect(dr, tx+TILESIZE-HIGHLIGHT_WIDTH, ty,
		      HIGHLIGHT_WIDTH, HIGHLIGHT_WIDTH, cc);
	if (val & FG_TLCORNER)
	    draw_rect(dr, tx, ty, HIGHLIGHT_WIDTH, HIGHLIGHT_WIDTH, ch);
	if (val & FG_BRCORNER)
	    draw_rect(dr, tx+TILESIZE-HIGHLIGHT_WIDTH,
		      ty+TILESIZE-HIGHLIGHT_WIDTH,
		      HIGHLIGHT_WIDTH, HIGHLIGHT_WIDTH, cl);
    } else if (val & (FG_MAIN | FG_NORMAL)) {
	int x[6], y[6];

	if (val & FG_DRAGGING)
	    cc = (val & FG_MAIN ? COL_MAIN_DRAGGING : COL_DRAGGING);
	else
	    cc = (val & FG_MAIN ? COL_MAIN : COL_BACKGROUND);
	ch = cc+1;
	cl = cc+2;

	if (val & FLASH_LOW)
	    cc = cl;
	else if (val & FLASH_HIGH)
	    cc = ch;

	/*
	 * Drawing the blocks is hellishly fiddly. The blocks
	 * don't stretch to the full size of the tile; there's a
	 * border around them of size BORDER_WIDTH. Then they have
	 * bevelled borders of size HIGHLIGHT_WIDTH, and also
	 * rounded corners.
	 * 
	 * I tried for some time to find a clean and clever way to
	 * figure out what needed drawing from the corner and
	 * border flags, but in the end the cleanest way I could
	 * find was the following. We divide the grid square into
	 * 25 parts by ruling four horizontal and four vertical
	 * lines across it; those lines are at BORDER_WIDTH and
	 * BORDER_WIDTH+HIGHLIGHT_WIDTH from the top, from the
	 * bottom, from the left and from the right. Then we
	 * carefully consider each of the resulting 25 sections of
	 * square, and decide separately what needs to go in it
	 * based on the flags. In complicated cases there can be
	 * up to five possibilities affecting any given section
	 * (no corner or border flags, just the corner flag, one
	 * border flag, the other border flag, both border flags).
	 * So there's a lot of very fiddly logic here and all I
	 * could really think to do was give it my best shot and
	 * then test it and correct all the typos. Not fun to
	 * write, and I'm sure it isn't fun to read either, but it
	 * seems to work.
	 */

	x[0] = tx;
	x[1] = x[0] + BORDER_WIDTH;
	x[2] = x[1] + HIGHLIGHT_WIDTH;
	x[5] = tx + TILESIZE;
	x[4] = x[5] - BORDER_WIDTH;
	x[3] = x[4] - HIGHLIGHT_WIDTH;

	y[0] = ty;
	y[1] = y[0] + BORDER_WIDTH;
	y[2] = y[1] + HIGHLIGHT_WIDTH;
	y[5] = ty + TILESIZE;
	y[4] = y[5] - BORDER_WIDTH;
	y[3] = y[4] - HIGHLIGHT_WIDTH;

#define RECT(p,q) x[p], y[q], x[(p)+1]-x[p], y[(q)+1]-y[q]

	maybe_rect(dr, RECT(0,0),
		   (val & (FG_TLCORNER | FG_TBORDER | FG_LBORDER)) ? -1 : cc);
	maybe_rect(dr, RECT(1,0),
		   (val & FG_TLCORNER) ? ch : (val & FG_TBORDER) ? -1 :
		   (val & FG_LBORDER) ? ch : cc);
	maybe_rect(dr, RECT(2,0),
		   (val & FG_TBORDER) ? -1 : cc);
	maybe_rect(dr, RECT(3,0),
		   (val & FG_TRCORNER) ? cl : (val & FG_TBORDER) ? -1 :
		   (val & FG_RBORDER) ? cl : cc);
	maybe_rect(dr, RECT(4,0),
		   (val & (FG_TRCORNER | FG_TBORDER | FG_RBORDER)) ? -1 : cc);
	maybe_rect(dr, RECT(0,1),
		   (val & FG_TLCORNER) ? ch : (val & FG_LBORDER) ? -1 :
		   (val & FG_TBORDER) ? ch : cc);
	maybe_rect(dr, RECT(1,1),
		   (val & FG_TLCORNER) ? cc : -1);
	maybe_rect(dr, RECT(1,1),
		   (val & FG_TLCORNER) ? ch | TYPE_TLCIRC :
		   !((FG_TBORDER | FG_LBORDER) &~ val) ? ch | TYPE_BRCIRC :
		   (val & (FG_TBORDER | FG_LBORDER)) ? ch : cc);
	maybe_rect(dr, RECT(2,1),
		   (val & FG_TBORDER) ? ch : cc);
	maybe_rect(dr, RECT(3,1),
		   (val & (FG_TBORDER | FG_RBORDER)) == FG_TBORDER ? ch :
		   (val & (FG_TBORDER | FG_RBORDER)) == FG_RBORDER ? cl :
		   !((FG_TBORDER|FG_RBORDER) &~ val) ? cc | TYPE_BLCIRC : cc);
	maybe_rect(dr, RECT(4,1),
		   (val & FG_TRCORNER) ? ch : (val & FG_RBORDER) ? -1 :
		   (val & FG_TBORDER) ? ch : cc);
	maybe_rect(dr, RECT(0,2),
		   (val & FG_LBORDER) ? -1 : cc);
	maybe_rect(dr, RECT(1,2),
		   (val & FG_LBORDER) ? ch : cc);
	maybe_rect(dr, RECT(2,2),
		   cc);
	maybe_rect(dr, RECT(3,2),
		   (val & FG_RBORDER) ? cl : cc);
	maybe_rect(dr, RECT(4,2),
		   (val & FG_RBORDER) ? -1 : cc);
	maybe_rect(dr, RECT(0,3),
		   (val & FG_BLCORNER) ? cl : (val & FG_LBORDER) ? -1 :
		   (val & FG_BBORDER) ? cl : cc);
	maybe_rect(dr, RECT(1,3),
		   (val & (FG_BBORDER | FG_LBORDER)) == FG_BBORDER ? cl :
		   (val & (FG_BBORDER | FG_LBORDER)) == FG_LBORDER ? ch :
		   !((FG_BBORDER|FG_LBORDER) &~ val) ? cc | TYPE_TRCIRC : cc);
	maybe_rect(dr, RECT(2,3),
		   (val & FG_BBORDER) ? cl : cc);
	maybe_rect(dr, RECT(3,3),
		   (val & FG_BRCORNER) ? cc : -1);
	maybe_rect(dr, RECT(3,3),
		   (val & FG_BRCORNER) ? cl | TYPE_BRCIRC :
		   !((FG_BBORDER | FG_RBORDER) &~ val) ? cl | TYPE_TLCIRC :
		   (val & (FG_BBORDER | FG_RBORDER)) ? cl : cc);
	maybe_rect(dr, RECT(4,3),
		   (val & FG_BRCORNER) ? cl : (val & FG_RBORDER) ? -1 :
		   (val & FG_BBORDER) ? cl : cc);
	maybe_rect(dr, RECT(0,4),
		   (val & (FG_BLCORNER | FG_BBORDER | FG_LBORDER)) ? -1 : cc);
	maybe_rect(dr, RECT(1,4),
		   (val & FG_BLCORNER) ? ch : (val & FG_BBORDER) ? -1 :
		   (val & FG_LBORDER) ? ch : cc);
	maybe_rect(dr, RECT(2,4),
		   (val & FG_BBORDER) ? -1 : cc);
	maybe_rect(dr, RECT(3,4),
		   (val & FG_BRCORNER) ? cl : (val & FG_BBORDER) ? -1 :
		   (val & FG_RBORDER) ? cl : cc);
	maybe_rect(dr, RECT(4,4),
		   (val & (FG_BRCORNER | FG_BBORDER | FG_RBORDER)) ? -1 : cc);

#undef RECT

    }

    draw_update(dr, tx, ty, TILESIZE, TILESIZE);
}

static void game_redraw(drawing *dr, game_drawstate *ds, game_state *oldstate,
			game_state *state, int dir, game_ui *ui,
			float animtime, float flashtime)
{
    int w = state->w, h = state->h, wh = w*h;
    unsigned char *board;
    int *dsf;
    int x, y, mainanchor, mainpos, dragpos;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed
	 * and can vary with front ends. To be on the safe side,
	 * all games should start by drawing a big
	 * background-colour rectangle covering the whole window.
	 */
	draw_rect(dr, 0, 0, 10*ds->tilesize, 10*ds->tilesize, COL_BACKGROUND);
	ds->started = TRUE;
    }

    /*
     * Construct the board we'll be displaying (which may be
     * different from the one in state if ui describes a drag in
     * progress).
     */
    board = snewn(wh, unsigned char);
    memcpy(board, state->board, wh);
    if (ui->dragging) {
	int mpret = move_piece(w, h, state->board, board,
			       state->imm->forcefield,
			       ui->drag_anchor, ui->drag_currpos);
	assert(mpret);
    }

    /*
     * Build a dsf out of that board, so we can conveniently tell
     * which edges are connected and which aren't.
     */
    dsf = snew_dsf(wh);
    mainanchor = -1;
    for (y = 0; y < h; y++)
	for (x = 0; x < w; x++) {
	    int i = y*w+x;

	    if (ISDIST(board[i]))
		dsf_merge(dsf, i, i - board[i]);
	    if (board[i] == MAINANCHOR)
		mainanchor = i;
	    if (board[i] == WALL) {
		if (x > 0 && board[i-1] == WALL)
		    dsf_merge(dsf, i, i-1);
		if (y > 0 && board[i-w] == WALL)
		    dsf_merge(dsf, i, i-w);
	    }
	}
    assert(mainanchor >= 0);
    mainpos = dsf_canonify(dsf, mainanchor);
    dragpos = ui->drag_currpos > 0 ? dsf_canonify(dsf, ui->drag_currpos) : -1;

    /*
     * Now we can construct the data about what we want to draw.
     */
    for (y = 0; y < h; y++)
	for (x = 0; x < w; x++) {
	    int i = y*w+x;
	    int j;
	    unsigned long val;
	    int canon;

	    /*
	     * See if this square is part of the target area.
	     */
	    j = i + mainanchor - (state->ty * w + state->tx);
	    while (j >= 0 && j < wh && ISDIST(board[j]))
		j -= board[j];
	    if (j == mainanchor)
		val = BG_TARGET;
	    else
		val = BG_NORMAL;

	    if (state->imm->forcefield[i])
		val |= BG_FORCEFIELD;

	    if (flashtime > 0) {
		int flashtype = (int)(flashtime / FLASH_INTERVAL) & 1;
		val |= (flashtype ? FLASH_LOW : FLASH_HIGH);
	    }

	    if (board[i] != EMPTY) {
		canon = dsf_canonify(dsf, i);

		if (board[i] == WALL)
		    val |= FG_WALL;
		else if (canon == mainpos)
		    val |= FG_MAIN;
		else
		    val |= FG_NORMAL;
		if (canon == dragpos)
		    val |= FG_DRAGGING;

		/*
		 * Now look around to see if other squares
		 * belonging to the same block are adjacent to us.
		 */
		if (x == 0 || canon != dsf_canonify(dsf, i-1))
		    val |= FG_LBORDER;
		if (y== 0 || canon != dsf_canonify(dsf, i-w))
		    val |= FG_TBORDER;
		if (x == w-1 || canon != dsf_canonify(dsf, i+1))
		    val |= FG_RBORDER;
		if (y == h-1 || canon != dsf_canonify(dsf, i+w))
		    val |= FG_BBORDER;
		if (!(val & (FG_TBORDER | FG_LBORDER)) &&
		    canon != dsf_canonify(dsf, i-1-w))
		    val |= FG_TLCORNER;
		if (!(val & (FG_TBORDER | FG_RBORDER)) &&
		    canon != dsf_canonify(dsf, i+1-w))
		    val |= FG_TRCORNER;
		if (!(val & (FG_BBORDER | FG_LBORDER)) &&
		    canon != dsf_canonify(dsf, i-1+w))
		    val |= FG_BLCORNER;
		if (!(val & (FG_BBORDER | FG_RBORDER)) &&
		    canon != dsf_canonify(dsf, i+1+w))
		    val |= FG_BRCORNER;
	    }

	    if (val != ds->grid[i]) {
		draw_tile(dr, ds, x, y, val);
		ds->grid[i] = val;
	    }
	}

    /*
     * Update the status bar.
     */
    {
	char statusbuf[256];

	/*
	 * FIXME: do something about auto-solve?
	 */
	sprintf(statusbuf, "%sMoves: %d",
		(state->completed >= 0 ? "COMPLETED! " : ""),
		(state->completed >= 0 ? state->completed : state->movecount));
	if (state->minmoves >= 0)
	    sprintf(statusbuf+strlen(statusbuf), " (min %d)",
		    state->minmoves);

	status_bar(dr, statusbuf);
    }

    sfree(dsf);
    sfree(board);
}

static float game_anim_length(game_state *oldstate, game_state *newstate,
			      int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(game_state *oldstate, game_state *newstate,
			       int dir, game_ui *ui)
{
    if (oldstate->completed < 0 && newstate->completed >= 0)
        return FLASH_TIME;

    return 0.0F;
}

static int game_timing_state(game_state *state, game_ui *ui)
{
    return TRUE;
}

static void game_print_size(game_params *params, float *x, float *y)
{
}

static void game_print(drawing *dr, game_state *state, int tilesize)
{
}

#ifdef COMBINED
#define thegame nullgame
#endif

const struct game thegame = {
    "Slide", NULL, NULL,
    default_params,
    game_fetch_preset,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    TRUE, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    FALSE, solve_game,		       /* FIXME */
    TRUE, game_text_format,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    game_changed_state,
    interpret_move,
    execute_move,
    PREFERRED_TILESIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    FALSE, FALSE, game_print_size, game_print,
    TRUE,			       /* wants_statusbar */
    FALSE, game_timing_state,
    0,				       /* flags */
};
