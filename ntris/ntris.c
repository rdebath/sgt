/*
 * ntris - a Tetris clone.
 * 
 * TODO:
 *  - weird flicker on focus-in; for starters, check all the event
 *    handlers are returning the right boolean. Some aren't
 *    returning anything at all, which may be the problem.
 *  - make up colours for Pentris pieces.
 *  - configurable playing area size.
 *  - skew randomness of Pentris pieces.
 *  - configurable window size to match playing area.
 *  - draw Next Piece in a place that works sensibly with playing area.
 *  - keep score.
 *  - track end-of-game, and enable restarts later.
 *  - timed dropping! Duh!
 *  - pause mode.
 *  - GTK front end. Menus, config box.
 *  - perhaps bizarre mission-type play?
 *     * Weird playing conditions:
 *        + pre-placed junk at bottom (can be dense or sparse)
 *        + pieces could have unusual colours (and pieces of
 *          specific colours are significant)?
 *        + skewed piece distribution
 *     * Weird objectives:
 *        + destroy particular bricks
 *        + remove all pockets of air, or all overhangs
 *        + achieve low maximum height of area contents
 *        + use up N whole pieces of particular types as they're
 *          dropped
 *        + clear the arena totally? VERY VERY HARD.
 *        + create N groups of {exactly,at least} {2,3,4} lines;
 *          perhaps 2-line or 3-line groups could be required to be
 *          noncontiguous?)
 *        + create N lines using particular colours, or without
 *          particular piece types
 *        + _achieve_ an unsupported overhang!
 *     * Sequential objectives
 *        + build up to a particular height and then get back down
 *     * Whole weird scenarios:
 *        + some stuff already present is `infectious': after a
 *          drop, any brick that touches it will become infected
 *          and change to its colour. The aim is to destroy all
 *          infected bricks, which you therefore have to do by
 *          arranging that the piece you drop to create a line
 *          leaves nothing in contact with the remaining infected
 *          stuff after the line is removed. (Infection propagation
 *          happens after line removal.)
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ntris.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define lenof(x) (sizeof((x))/sizeof(*(x)))

/*
 * Prefix on each shape string is:
 * 
 *  - X means this piece cannot be reflected (it would make Tetris
 *    too easy).
 *  - Y means this piece reflects to a rotation of itself, mapping
 *    0123 -> 0321.
 *  - Z means this piece reflects to a rotation of itself, mapping
 *    0123 -> 2103.
 *  - W means this piece reflects to a rotation of itself, mapping
 *    0123 -> 1032.
 *  - A means the piece is the first of two 2-shapelet shapes which
 *    reflects to the one after it. The one after it is labelled B.
 *  - C means the piece is the first of two 4-shapelet shapes which
 *    reflects to the one after it, mapping 0123 -> 0321. The one
 *    after it is labelled D.
 *  - E means the piece is the first of two 4-shapelet shapes which
 *    reflects to the one after it, mapping 0123 -> 2103. The one
 *    after it is labelled F.
 */

char *tetris_shapes[7+1] = {
    "X0:10011121:10011112:01112112:10111221", /* T-piece */
    "X3:01112131:10111213",	             /* long bar */
    "X4:00011121:10111202:01112122:10111220", /* reverse L */
    "X7:01111222:11122021",	             /* Z skew */
    "X8:02121121:10112122",	             /* S skew */
    "X10:01112120:00101112:02011121:10111222", /* forward L */
    "X17:00011011",		             /* square */
    NULL
};

char *pentris_shapes[18+1] = {
    "Y0:0102122221:1020211222:0010200121:0010010212",  /* U */
    "X1:1101211012",                                   /* X */
    "C2:1121122232:1121122220:1121122201:1121122213",  /* P */
    "D3:1121122202:1121122223:1121122231:1121122210",  /* reverse P */
    "X4:0212223242:2021222324",                        /* I */
    "C5:0212223231:2423222111:4232221213:2021222333",  /* L */
    "D6:4232221211:2021222313:0212223233:2423222131",  /* reverse L */
    "E7:0111122232:1312222120:3222211101:2021111213",  /* N */
    "F8:0212112131:2322121110:3121221202:1011212223",  /* reverse N */
    "C9:0001112112:0212111021:2221110110:2010111201",  /* F */
    "D10:2021110112:0010111221:0201112110:2212111001", /* reverse F */
    "Y11:0010201112:0001021121:0212221110:2021221101", /* T */
    "W12:0001111222:0212112120:2221111000:2010110102", /* W */
    "C13:2102122232:1221222324:2312223242:3220212223", /* Y */
    "D14:2112223242:1220212223:2302122232:3221222324", /* reverse Y */
    "A15:0201112120:2212111000",                       /* Z */
    "B16:2221110100:2010111202",                       /* reverse Z */
    "W17:0001021222:0212222120:2221201000:2010000102", /* V */
    NULL
};

#ifndef SHAPESET
#define SHAPESET tetris_shapes
#endif
static char **default_shapeset = SHAPESET;

struct shapeset {
    void *data[4];		       /* things to free when this dies */
    unsigned char **shapes;		       /* shapelet -> (coords,flags) pairs */
    unsigned char *anticlock;		       /* shapelet -> nextshapelet */
    unsigned char *clockwise;		       /* shapelet -> prevshapelet */
    unsigned char *reflected;		       /* shapelet -> mirrorshapelet */
    unsigned char *width;		       /* shapelet -> width */
    unsigned char *start;		       /* starting shapes */
    unsigned char *colours;		       /* shape colours */
    int nstart;			       /* number of starting shapes */
};

static struct shapeset *make_shapeset(char **shapes, int nshapes)
{
    int i, j, k;
    int nshapelets, nsquares;
    struct shapeset *ss;
    unsigned char *shapedata;

    ss = (struct shapeset *)malloc(sizeof(struct shapeset));
    for (i = 0; i < lenof(ss->data); i++)
	ss->data[i] = NULL;
    ss->nstart = nshapes;

    /*
     * First, count the shapes to see what we've got.
     */
    nshapelets = 0;
    nsquares = 0;
    for (i = 0; i < nshapes; i++) {
	char *p = shapes[i];
	int len;
	p = strchr(p, ':');
	while (p && *p) {
	    p++;		       /* eat the colon */
	    len = strcspn(p, ":");     /* find the next one, or the end */
	    nsquares += (len/2)+1;     /* one extra for a terminator */
	    nshapelets++;
	    p += len;
	}
    }

    /*
     * Now allocate the storage for everything.
     */
    ss->data[0] = (unsigned char **)malloc(nshapelets *
					   sizeof(unsigned char *));
    ss->data[1] = (unsigned char *)malloc((5*nshapelets+nshapes) *
					  sizeof(unsigned char));
    ss->data[2] = shapedata = (unsigned char *)malloc(2*nsquares *
						      sizeof(unsigned char));
    ss->shapes = ss->data[0];
    ss->anticlock = ss->data[1];
    ss->clockwise = ss->anticlock + nshapelets;
    ss->reflected = ss->clockwise + nshapelets;
    ss->width = ss->reflected + nshapelets;
    ss->start = ss->width + nshapelets;
    ss->colours = ss->start + nshapes;

    /*
     * Now progress through the shapes setting up the various data.
     */
    j = 0;
    k = 0;
    for (i = 0; i < nshapes; i++) {
	char *p = shapes[i];
	char reflecttype = *p++;
	int colour = atoi(p);
	int shapeindex = 0;
	ss->start[i] = j;
	p += strcspn(p, ":");
	while (*p) {
	    int array[18][18];
	    int x, y, w;
	    memset(array, 0, sizeof(array));

	    ss->colours[j] = colour;
	    ss->shapes[j] = shapedata+k;

	    p++;
	    while (*p && *p != ':') {
		int z;
		sscanf(p, "%02x", &z);
		array[1+((z>>4) & 0xF)][1+(z & 0xF)] = 1;
		p += 2;
	    }

	    w = 0;
	    for (y = 0; y < 16; y++) {
		for (x = 0; x < 16; x++) {
		    if (array[1+x][1+y]) {
			int flags = 0;
			shapedata[k++] = (x << 4) + y;
			if (w < x+1) w = x+1;
			/* Now for the edge and corner data */
			if (!array[0+x][1+y]) flags |= FLAG_LEDGE;
			if (!array[2+x][1+y]) flags |= FLAG_REDGE;
			if (!array[1+x][0+y]) flags |= FLAG_TEDGE;
			if (!array[1+x][2+y]) flags |= FLAG_BEDGE;
			if (!array[0+x][0+y]) flags |= FLAG_TLCORNER;
			if (!array[2+x][0+y]) flags |= FLAG_TRCORNER;
			if (!array[0+x][2+y]) flags |= FLAG_BLCORNER;
			if (!array[2+x][2+y]) flags |= FLAG_BRCORNER;
			shapedata[k++] = flags;
		    }
		}
	    }

	    shapedata[k++] = 0xFF;
	    shapedata[k++] = 0xFF;

	    if (*p)
		ss->anticlock[j] = j+1;
	    else
		ss->anticlock[j] = ss->start[i];
	    ss->clockwise[ss->anticlock[j]] = j;

	    switch (reflecttype) {
	      default:
		ss->reflected[j] = j; break;   /* reflection does nothing */
	      case 'A':
		ss->reflected[j] = j+2; break;
	      case 'B':
		ss->reflected[j] = j-2; break;
	      case 'C':
		ss->reflected[j] = (j-shapeindex) + 4 + ((-shapeindex)&3);
		break;
	      case 'D':
		ss->reflected[j] = (j-shapeindex) - 4 + ((-shapeindex)&3);
		break;
	      case 'E':
		ss->reflected[j] = (j-shapeindex) + 4 + ((2-shapeindex)&3);
		break;
	      case 'F':
		ss->reflected[j] = (j-shapeindex) - 4 + ((2-shapeindex)&3);
		break;
	      case 'Y':
		ss->reflected[j] = (j-shapeindex) + ((-shapeindex)&3);
		break;
	      case 'Z':
		ss->reflected[j] = (j-shapeindex) + ((2-shapeindex)&3);
		break;
	      case 'W':
		ss->reflected[j] = (j-shapeindex) + (shapeindex^1);
		break;
	    }

	    ss->width[j] = w;

	    j++;
	    shapeindex++;
	}
    }

    return ss;
}

struct ntris_instance {
    struct frontend_instance *fe;
    struct shapeset *ss;
    int play_width, play_height;
    unsigned char (*playarea)[2];
    int currshape, nextshape, shapecolour, shape_x, shape_y;
};

#define PLAYAREA(x,y,n) (inst->playarea[(y)*inst->play_width+(x)][n])

static int rand_shape(struct ntris_instance *inst)
{
    int divisor = RAND_MAX / inst->ss->nstart;
    int limit = divisor * inst->ss->nstart;
    int i;

    while ( (i = rand()) >= limit )
	;
    i = inst->ss->start[i / divisor];
    return i;
}

static void draw_shape(struct ntris_instance *inst, int shape,
		       int x, int y, int c)
{
    unsigned char *p;

    p = inst->ss->shapes[shape];
    while (*p != 0xFF) {
	int xy, flag;
	xy = *p++;
	flag = *p++;
	block(inst->fe, (x + ((xy>>4) & 0xF)), (y + ( xy     & 0xF)), c, flag);
    }
}

static int shape_fits(struct ntris_instance *inst, int shape, int x, int y)
{
    unsigned char *p;

    p = inst->ss->shapes[shape];
    while (*p != 0xFF) {
	int xy, xx, yy;
	xy = *p++;
	p++;
	xx = x+((xy>>4)&0xF);
	yy = y+(xy&0xF);
	if (xx < 0 || xx >= inst->play_width ||
	    yy < 0 || yy >= inst->play_height)
	    return FALSE;
	if (PLAYAREA(xx,yy,0) != 0xFF)
	    return FALSE;
    }
    return TRUE;
}

static void write_shape(struct ntris_instance *inst, int shape, int x, int y)
{
    unsigned char *p;
    int c;

    p = inst->ss->shapes[shape];
    c = inst->shapecolour;
    while (*p != 0xFF) {
	int xy, xx, yy, flag;
	xy = *p++;
	flag = *p++;
	xx = x+((xy>>4)&0xF);
	yy = y+(xy&0xF);
	PLAYAREA(xx,yy,0) = c;
	PLAYAREA(xx,yy,1) = flag;
    }
}

int init_shape(struct ntris_instance *inst)
{
    inst->shape_x = (inst->play_width - inst->ss->width[inst->currshape]) / 2;
    inst->shape_y = 0;
    if (!shape_fits(inst, inst->currshape, inst->shape_x, inst->shape_y))
	return FALSE;
    draw_shape(inst, inst->currshape, inst->shape_x+1, inst->shape_y,
	       inst->shapecolour);
    draw_shape(inst, inst->nextshape, inst->play_width+3, 1, 
	       inst->ss->colours[inst->nextshape]);
    return TRUE;
}

struct ntris_instance *init_game(struct frontend_instance *fe,
				 int width, int height, char **shapeset)
{
    struct ntris_instance *inst = malloc(sizeof(struct ntris_instance));
    int nshapes;

    if (!shapeset)
	shapeset = default_shapeset;

    for (nshapes = 0; shapeset[nshapes]; nshapes++);
    inst->ss = make_shapeset(shapeset, nshapes);

    inst->play_width = width;
    inst->play_height = height;
    inst->playarea = malloc(width * height * sizeof(*inst->playarea));

    memset(inst->playarea, 0xFF, width*height*sizeof(*inst->playarea));
    srand(time(NULL));
    inst->currshape = rand_shape(inst);
    inst->shapecolour = inst->ss->colours[inst->currshape];
    inst->nextshape = rand_shape(inst);

    inst->fe = fe;

    return inst;
}

int try_move_left(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->currshape, inst->shape_x-1, inst->shape_y)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->shape_x--;
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

int try_move_right(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->currshape, inst->shape_x+1, inst->shape_y)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->shape_x++;
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

int try_move_down(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->currshape, inst->shape_x, inst->shape_y+1)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->shape_y++;
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

int try_anticlock(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->ss->anticlock[inst->currshape],
		   inst->shape_x, inst->shape_y)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->currshape = inst->ss->anticlock[inst->currshape];
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

int try_clockwise(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->ss->clockwise[inst->currshape],
		   inst->shape_x, inst->shape_y)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->currshape = inst->ss->clockwise[inst->currshape];
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

int try_reflect(struct ntris_instance *inst)
{
    if (shape_fits(inst, inst->ss->reflected[inst->currshape],
		   inst->shape_x, inst->shape_y)) {
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, -1);
	inst->currshape = inst->ss->reflected[inst->currshape];
	draw_shape(inst, inst->currshape,
		   inst->shape_x+1, inst->shape_y, inst->shapecolour);
	return TRUE;
    }
    return FALSE;
}

static int check_lines(struct ntris_instance *inst)
{
    int src, targ;
    int count = 0;
    int i;

    targ = src = inst->play_height-1;
    while (src >= 0) {
	int is_line;

	/* See if this line is, well, a line */
	is_line = 1;
	for (i = 0; i < inst->play_width; i++)
	    if (PLAYAREA(i,src,0) == 0xFF)
		is_line = 0;

	if (is_line) {
	    /* Seal the edges of the neighbouring lines */
	    for (i = 0; i < inst->play_width; i++) {
		if (targ < inst->play_height-1) {
		    PLAYAREA(i,targ+1,1) |=
			FLAG_TEDGE | FLAG_TLCORNER | FLAG_TRCORNER;
		}
		if (src > 0) {
		    PLAYAREA(i,src-1,1) |=
			FLAG_BEDGE | FLAG_BLCORNER | FLAG_BRCORNER;
		}
	    }
	    count++;
	} else {
	    /* Just copy the line to the target position */
	    for (i = 0; i < inst->play_width; i++) {
		PLAYAREA(i,targ,0) = PLAYAREA(i,src,0);
		PLAYAREA(i,targ,1) = PLAYAREA(i,src,1);
	    }
	    targ--;
	}

	src--;
    }

    while (targ >= 0) {
	for (i = 0; i < inst->play_width; i++) {
	    PLAYAREA(i,targ,0) = PLAYAREA(i,targ,1) = 0xFF;
	}
	targ--;
    }

    /* Redraw the playing area */
    for (targ = 0; targ < inst->play_height; targ++) {
	for (i = 0; i < inst->play_width; i++) {
	    int c, flag;
	    c = PLAYAREA(i,targ,0);
	    flag = PLAYAREA(i,targ,1);
	    block(inst->fe, (i+1), targ, (c == 0xFF ? -1 : c), flag);
	}
    }

    return count;
}

static void postdrop(struct ntris_instance *inst)
{
    write_shape(inst, inst->currshape, inst->shape_x, inst->shape_y);
    draw_shape(inst, inst->nextshape, inst->play_width+3, 1, -1);
    check_lines(inst);
    inst->currshape = inst->nextshape;
    inst->shapecolour = inst->ss->colours[inst->currshape];
    inst->nextshape = rand_shape(inst);
}

int softdrop(struct ntris_instance *inst)
{
    if (try_move_down(inst))
	return FALSE;
    postdrop(inst);
    return TRUE;
}

void harddrop(struct ntris_instance *inst)
{
    while (try_move_down(inst))
	continue;
    postdrop(inst);
}
