/*
 * 4tris - a Tetris clone.
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
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>

#define GAME_WIDTH 640
#define GAME_HEIGHT 512

#define PLAY_WIDTH 10
#define PLAY_HEIGHT 30

#define SQUARE_SIDE 16
#define HIGHLIGHT 3
#define NCOLOURS 18
#define LIB_WIDTH (NCOLOURS*SQUARE_SIDE)
#define LIB_HEIGHT (256*SQUARE_SIDE)

#define FLAG_REDGE    0x01
#define FLAG_LEDGE    0x02
#define FLAG_TEDGE    0x04
#define FLAG_BEDGE    0x08
#define FLAG_TRCORNER 0x10
#define FLAG_TLCORNER 0x20
#define FLAG_BRCORNER 0x40
#define FLAG_BLCORNER 0x80

#define CAT2(x,y) x ## y
#define CAT(x,y) CAT2(x,y)
#define ASSERT(x) enum {CAT(assertion_,__LINE__) = 1 / (x)}

#define lenof(x) (sizeof((x))/sizeof(*(x)))

#ifndef SHAPESET
#define SHAPESET tetris_shapes
#endif

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

char *tetris_shapes[7] = {
    "X0:10011121:10011112:01112112:10111221", /* T-piece */
    "X3:01112131:10111213",	             /* long bar */
    "X4:00011121:10111202:01112122:10111220", /* reverse L */
    "X7:01111222:11122021",	             /* Z skew */
    "X8:02121121:10112122",	             /* S skew */
    "X10:01112120:00101112:02011121:10111222", /* forward L */
    "X17:00011011",		             /* square */
};

char *pentris_shapes[18] = {
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
};

struct shapeset {
    gpointer data[4];		       /* things to free when this dies */
    guint8 **shapes;		       /* shapelet -> (coords,flags) pairs */
    guint8 *anticlock;		       /* shapelet -> nextshapelet */
    guint8 *clockwise;		       /* shapelet -> prevshapelet */
    guint8 *reflected;		       /* shapelet -> mirrorshapelet */
    guint8 *width;		       /* shapelet -> width */
    guint8 *start;		       /* starting shapes */
    guint8 *colours;		       /* shape colours */
    int nstart;			       /* number of starting shapes */
};

struct shapeset *make_shapeset(char **shapes, int nshapes)
{
    int i, j, k;
    int nshapelets, nsquares;
    struct shapeset *ss;
    guint8 *shapedata;

    ss = (struct shapeset *)g_malloc(sizeof(struct shapeset));
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
    ss->data[0] = (guint8 **)g_malloc(nshapelets * sizeof(guint8 *));
    ss->data[1] = (guint8 *)g_malloc((5*nshapelets+nshapes) * sizeof(guint8));
    ss->data[2] = shapedata = (guint8 *)g_malloc(2*nsquares * sizeof(guint8));
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
	    gboolean array[18][18];
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

struct game_instance {
    GtkWidget *area;
    GdkPixmap *pixmap, *library;
    GdkFont *font;
    GdkGC *black_gc, *white_gc, *random_gc;
    GdkColor cols[3*NCOLOURS];
    GdkColormap *colmap;
    gint32 keystate[65536/32];
    enum { TITLESCR, IN_GAME } state;
    struct shapeset *ss;
    guint8 playarea[PLAY_WIDTH*PLAY_HEIGHT][2];
    int currshape, nextshape, shapecolour, shape_x, shape_y;
    int leftpressed, rightpressed, acpressed, cwpressed, rpressed;
    int down_disabled;
    int update_minx, update_maxx, update_miny, update_maxy;
};

#define PLAYAREA(x,y,n) (inst->playarea[(y)*PLAY_WIDTH+(x)][n])

int rand_shape(struct game_instance *inst)
{
    int divisor = RAND_MAX / inst->ss->nstart;
    int limit = divisor * inst->ss->nstart;
    int i;

    while ( (i = rand()) >= limit )
	;
    i = inst->ss->start[i / divisor];
    return i;
}

void cls(struct game_instance *inst)
{
    gdk_draw_rectangle(inst->pixmap, inst->black_gc,
		       TRUE, 0, 0, GAME_WIDTH, GAME_HEIGHT);
}

void text(struct game_instance *inst, int x, int y, char *str)
{
    gdk_draw_text(inst->pixmap, inst->font, inst->white_gc, x, y,
		  str, strlen(str));
}

void block(struct game_instance *inst, int x, int y, int col, int type)
{
    if (col < 0) {
	gdk_draw_rectangle(inst->pixmap, inst->black_gc, TRUE,
			   x, y, SQUARE_SIDE, SQUARE_SIDE);
    } else {
	gdk_draw_pixmap(inst->pixmap, inst->random_gc, inst->library,
			col*SQUARE_SIDE, type*SQUARE_SIDE,
			x, y, SQUARE_SIDE, SQUARE_SIDE);
    }
    if (inst->update_minx > x)
	inst->update_minx = x;
    if (inst->update_miny > y)
	inst->update_miny = y;
    if (inst->update_maxx < x+SQUARE_SIDE)
	inst->update_maxx = x+SQUARE_SIDE;
    if (inst->update_maxy < y+SQUARE_SIDE)
	inst->update_maxy = y+SQUARE_SIDE;
}

void titlescr(struct game_instance *inst)
{
    cls(inst);
    text(inst, 10, 20, "Press a key to play");

#if 0 /* graphics diagnostics */
    block(inst, 50, 50, 3, FLAG_LEDGE|FLAG_TEDGE|FLAG_BRCORNER);
    block(inst, 58, 50, 3, FLAG_REDGE|FLAG_TEDGE|FLAG_BEDGE);
    block(inst, 50, 58, 3, FLAG_REDGE|FLAG_BEDGE|FLAG_TLCORNER);
    block(inst, 42, 58, 3, FLAG_LEDGE|FLAG_BEDGE|FLAG_TEDGE);

    block(inst, 150, 50, 5, FLAG_REDGE|FLAG_TEDGE|FLAG_BLCORNER);
    block(inst, 142, 50, 5, FLAG_LEDGE|FLAG_TEDGE|FLAG_BEDGE);
    block(inst, 150, 58, 5, FLAG_LEDGE|FLAG_BEDGE|FLAG_TRCORNER);
    block(inst, 158, 58, 5, FLAG_REDGE|FLAG_BEDGE|FLAG_TEDGE);
#endif
}

void playarea(struct game_instance *inst)
{
    cls(inst);
    gdk_draw_rectangle(inst->pixmap, inst->white_gc, TRUE,
		       SQUARE_SIDE/2, 0,
		       SQUARE_SIDE*(PLAY_WIDTH+1),
		       SQUARE_SIDE*(PLAY_HEIGHT*2+1)/2);
    gdk_draw_rectangle(inst->pixmap, inst->black_gc, TRUE,
		       SQUARE_SIDE-1, 0,
		       SQUARE_SIDE*PLAY_WIDTH+2, SQUARE_SIDE*PLAY_HEIGHT+1);
}

void draw_shape(struct game_instance *inst, int shape, int x, int y, int c)
{
    guint8 *p;

    p = inst->ss->shapes[shape];
    while (*p != 0xFF) {
	int xy, flag;
	xy = *p++;
	flag = *p++;
	block(inst,
	      (x + ((xy>>4) & 0xF)) * SQUARE_SIDE,
	      (y + ( xy     & 0xF)) * SQUARE_SIDE, c, flag);
    }
}

int shape_fits(struct game_instance *inst, int shape, int x, int y)
{
    guint8 *p;

    p = inst->ss->shapes[shape];
    while (*p != 0xFF) {
	int xy, xx, yy;
	xy = *p++;
	p++;
	xx = x+((xy>>4)&0xF);
	yy = y+(xy&0xF);
	if (xx < 0 || xx >= PLAY_WIDTH || yy < 0 || yy >= PLAY_HEIGHT)
	    return FALSE;
	if (PLAYAREA(xx,yy,0) != 0xFF)
	    return FALSE;
    }
    return TRUE;
}

void write_shape(struct game_instance *inst, int shape, int x, int y)
{
    guint8 *p;
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

int init_shape(struct game_instance *inst)
{
    inst->shape_x = (PLAY_WIDTH - inst->ss->width[inst->currshape]) / 2;
    inst->shape_y = 0;
    if (!shape_fits(inst, inst->currshape, inst->shape_x, inst->shape_y))
	return FALSE;
    draw_shape(inst, inst->currshape, inst->shape_x+1, inst->shape_y,
	       inst->shapecolour);
    draw_shape(inst, inst->nextshape, PLAY_WIDTH+3, 1, 
	       inst->ss->colours[inst->nextshape]);
}

void init_game(struct game_instance *inst)
{
    memset(inst->playarea, 0xFF, sizeof(inst->playarea));
    srand(time(NULL));
    inst->currshape = rand_shape(inst);
    inst->shapecolour = inst->ss->colours[inst->currshape];
    inst->nextshape = rand_shape(inst);
    init_shape(inst);
}

int try_move_left(struct game_instance *inst)
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

int try_move_right(struct game_instance *inst)
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

int try_move_down(struct game_instance *inst)
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

int try_anticlock(struct game_instance *inst)
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

int try_clockwise(struct game_instance *inst)
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

int try_reflect(struct game_instance *inst)
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

int check_lines(struct game_instance *inst)
{
    int src, targ;
    int count = 0;
    int i;

    targ = src = PLAY_HEIGHT-1;
    while (src >= 0) {
	int is_line;

	/* See if this line is, well, a line */
	is_line = 1;
	for (i = 0; i < PLAY_WIDTH; i++)
	    if (PLAYAREA(i,src,0) == 0xFF)
		is_line = 0;

	if (is_line) {
	    /* Seal the edges of the neighbouring lines */
	    for (i = 0; i < PLAY_WIDTH; i++) {
		if (targ < PLAY_HEIGHT-1) {
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
	    for (i = 0; i < PLAY_WIDTH; i++) {
		PLAYAREA(i,targ,0) = PLAYAREA(i,src,0);
		PLAYAREA(i,targ,1) = PLAYAREA(i,src,1);
	    }
	    targ--;
	}

	src--;
    }

    while (targ >= 0) {
	for (i = 0; i < PLAY_WIDTH; i++) {
	    PLAYAREA(i,targ,0) = PLAYAREA(i,targ,1) = 0xFF;
	}
	targ--;
    }

    /* Redraw the playing area pixmap */
    for (targ = 0; targ < PLAY_HEIGHT; targ++) {
	for (i = 0; i < PLAY_WIDTH; i++) {
	    int c, flag;
	    c = PLAYAREA(i,targ,0);
	    flag = PLAYAREA(i,targ,1);
	    block(inst, (i+1)*SQUARE_SIDE, targ*SQUARE_SIDE,
		  (c == 0xFF ? -1 : c), flag);
	}
    }

    return count;
}

int drop(struct game_instance *inst)
{
    if (try_move_down(inst))
	return FALSE;
    write_shape(inst, inst->currshape, inst->shape_x, inst->shape_y);
    draw_shape(inst, inst->nextshape, PLAY_WIDTH+3, 1, -1);
    check_lines(inst);
    inst->currshape = inst->nextshape;
    inst->shapecolour = inst->ss->colours[inst->currshape];
    inst->nextshape = rand_shape(inst);
    init_shape(inst);
    inst->down_disabled = TRUE;
}

void update_rect(struct game_instance *inst, int x, int y, int w, int h)
{
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    gtk_widget_draw(inst->area, &rect);
}

void update_all(struct game_instance *inst)
{
    update_rect(inst, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    inst->update_minx = GAME_WIDTH+1;
    inst->update_maxx = -1;
    inst->update_miny = GAME_HEIGHT+1;
    inst->update_maxy = -1;
}

void update_minimal(struct game_instance *inst)
{
    if (inst->update_minx < inst->update_maxx &&
	inst->update_miny < inst->update_maxy) {
	update_rect(inst, inst->update_minx, inst->update_miny,
		    inst->update_maxx - inst->update_minx,
		    inst->update_maxy - inst->update_miny);
	inst->update_minx = GAME_WIDTH+1;
	inst->update_maxx = -1;
	inst->update_miny = GAME_HEIGHT+1;
	inst->update_maxy = -1;
    }
}

gint delete_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    /*
     * This is a game, so never delay an attempted close - the boss
     * might be walking in :-)
     */
    return FALSE;
}

gint configure_area(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    struct game_instance *inst = (struct game_instance *)data;

    if (inst->pixmap)
	gdk_pixmap_unref(inst->pixmap);

    inst->pixmap = gdk_pixmap_new(widget->window, GAME_WIDTH, GAME_HEIGHT, -1);
    inst->library = gdk_pixmap_new(widget->window, LIB_WIDTH, LIB_HEIGHT, -1);
    inst->font = gdk_font_load("variable");
    inst->black_gc = widget->style->black_gc;
    inst->white_gc = widget->style->white_gc;

    /*
     * Set up the colour map.
     */
    inst->colmap = gdk_colormap_get_system();
    {
	char *colours[] = {
	    "#cc0000", "#880000", "#ff0000",
	    "#cc6600", "#884400", "#ff7f00",
	    "#cc9900", "#886600", "#ffbf00",
	    "#cccc00", "#888800", "#ffff00",
	    "#00cc00", "#008800", "#00ff00",
	    "#008400", "#005800", "#00b000",
	    "#008484", "#005858", "#00b0b0",
	    "#00cccc", "#008888", "#00ffff",
	    "#0066cc", "#004488", "#007fff",
	    "#9900cc", "#660088", "#bf00ff",
	    "#cc00cc", "#880088", "#ff00ff",
	    "#cc9999", "#886666", "#ffbfbf",
	    "#cccc99", "#888866", "#ffffbf",
	    "#99cc99", "#668866", "#bfffbf",
	    "#9999cc", "#666688", "#bfbfff",
	    "#757575", "#4e4e4e", "#9c9c9c",
	    "#999999", "#666666", "#bfbfbf",
	    "#cccccc", "#888888", "#ffffff",
	};
	ASSERT(sizeof(colours)/sizeof(*colours)==3*NCOLOURS);
	gboolean success[3*NCOLOURS];
	int i;

	for (i = 0; i < 3*NCOLOURS; i++) {
	    if (!gdk_color_parse(colours[i], &inst->cols[i]))
		g_error("4tris: couldn't parse colour \"%s\"\n", colours[i]);
	}

	gdk_colormap_alloc_colors(inst->colmap, inst->cols, 3*NCOLOURS,
				  FALSE, FALSE, success);
	for (i = 0; i < 3*NCOLOURS; i++) {
	    if (!success[i])
		g_error("4tris: couldn't allocate colour \"%s\"\n", colours[i]);
	}
    }

    /*
     * Now set up the library pixmap, drawing each square.
     */
    {
	GdkGC *gc = gdk_gc_new(widget->window);
	int col, type, i;
	for (col = 0; col < NCOLOURS; col++) {
	    for (type = 0; type < 256; type++) {
		/* Main rectangle */
		gdk_gc_set_foreground(gc, &inst->cols[3*col+0]);
		gdk_draw_rectangle(inst->library, gc, TRUE,
				   col*SQUARE_SIDE, type*SQUARE_SIDE,
				   SQUARE_SIDE, SQUARE_SIDE);
		/* Highlights on left and top corners */
		gdk_gc_set_foreground(gc, &inst->cols[3*col+2]);
		if (type & FLAG_TLCORNER) {
		    gdk_draw_rectangle(inst->library, gc, TRUE,
				       col*SQUARE_SIDE,
				       type*SQUARE_SIDE,
				       HIGHLIGHT, HIGHLIGHT);
		}
		if (type & FLAG_TRCORNER) {
		    for (i = 1; i < HIGHLIGHT; i++) {
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+SQUARE_SIDE-i,
					   type*SQUARE_SIDE+i,
					   i, 1);
		    }
		}
		if (type & FLAG_BLCORNER) {
		    for (i = 1; i < HIGHLIGHT; i++) {
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+i,
					   type*SQUARE_SIDE+SQUARE_SIDE-i,
					   1, i);
		    }
		}
		/* Shadows on right and bottom corners */
		gdk_gc_set_foreground(gc, &inst->cols[3*col+1]);
		if (type & FLAG_BRCORNER) {
		    gdk_draw_rectangle(inst->library, gc, TRUE,
				       col*SQUARE_SIDE+SQUARE_SIDE-HIGHLIGHT,
				       type*SQUARE_SIDE+SQUARE_SIDE-HIGHLIGHT,
				       HIGHLIGHT, HIGHLIGHT);
		}
		if (type & FLAG_TRCORNER) {
		    for (i = 1; i < HIGHLIGHT; i++) {
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+SQUARE_SIDE-i-1,
					   type*SQUARE_SIDE,
					   1, i);
		    }
		}
		if (type & FLAG_BLCORNER) {
		    for (i = 1; i < HIGHLIGHT; i++) {
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE,
					   type*SQUARE_SIDE+SQUARE_SIDE-i-1,
					   i, 1);
		    }
		}
		/* Highlights on left and top edges */
		gdk_gc_set_foreground(gc, &inst->cols[3*col+2]);
		if (type & FLAG_LEDGE) {
		    for (i = 0; i < HIGHLIGHT; i++) {
			int offset = (type & FLAG_BEDGE ? i+1 : 0);
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+i,
					   type*SQUARE_SIDE,
					   1, SQUARE_SIDE-offset);
		    }
		}
		if (type & FLAG_TEDGE) {
		    for (i = 0; i < HIGHLIGHT; i++) {
			int offset = (type & FLAG_REDGE ? i+1 : 0);
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE,
					   type*SQUARE_SIDE+i,
					   SQUARE_SIDE-offset, 1);
		    }
		}
		/* Shadows on right and bottom edges */
		gdk_gc_set_foreground(gc, &inst->cols[3*col+1]);
		if (type & FLAG_REDGE) {
		    for (i = 0; i < HIGHLIGHT; i++) {
			int offset = (type & FLAG_TEDGE ? i+1 : 0);
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+SQUARE_SIDE-i-1,
					   type*SQUARE_SIDE+offset,
					   1, SQUARE_SIDE-offset);
		    }
		}
		if (type & FLAG_BEDGE) {
		    for (i = 0; i < HIGHLIGHT; i++) {
			int offset = (type & FLAG_LEDGE ? i+1 : 0);
			gdk_draw_rectangle(inst->library, gc, TRUE,
					   col*SQUARE_SIDE+offset,
					   type*SQUARE_SIDE+SQUARE_SIDE-i-1,
					   SQUARE_SIDE-offset, 1);
		    }
		}
	    }
	}
	inst->random_gc = gc;
    }

    titlescr(inst);

    inst->state = TITLESCR;

    return TRUE;
}

gint expose_area(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    struct game_instance *inst = (struct game_instance *)data;

    if (inst->pixmap) {
	gdk_draw_pixmap(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
			inst->pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
    }
    return FALSE;
}

#define KEY_PRESSED(k) \
    (inst->keystate[(k) / 32] & (1 << ((k) % 32)))

gint key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    struct game_instance *inst = (struct game_instance *)data;
    int oldstate, newstate;

    if (event->type == GDK_KEY_PRESS) {
	inst->keystate[event->keyval / 32] |= 1 << (event->keyval % 32);
    } else {
	inst->keystate[event->keyval / 32] &= ~(1 << (event->keyval % 32));
    }

    if (event->type == GDK_KEY_PRESS && inst->state == TITLESCR) {
	if (event->keyval == 'q') {
	    gtk_main_quit();
	} else {
	    inst->state = IN_GAME;
	    inst->ss = make_shapeset(SHAPESET, lenof(SHAPESET));
	    inst->leftpressed = inst->rightpressed = 0;
	    inst->acpressed = inst->cwpressed = inst->rpressed = 0;
            inst->down_disabled = FALSE;
	    playarea(inst);
	    init_game(inst);
	    update_all(inst);
	}
    }
}

gint timer_func(gpointer data)
{
    struct game_instance *inst = (struct game_instance *)data;

    if (inst->state != IN_GAME)
	return TRUE;

    if (KEY_PRESSED('a') || (KEY_PRESSED('z') && !inst->leftpressed)) {
	if (try_move_left(inst))
	    update_minimal(inst);
    }
    inst->leftpressed = KEY_PRESSED('z');

    if (KEY_PRESSED('s') || (KEY_PRESSED('x') && !inst->rightpressed)) {
	if (try_move_right(inst))
	    update_minimal(inst);
    }
    inst->rightpressed = KEY_PRESSED('x');

    if (KEY_PRESSED('\'') && !inst->acpressed) {
	if (try_anticlock(inst))
	    update_minimal(inst);
    }
    inst->acpressed = KEY_PRESSED('\'');

    if (KEY_PRESSED('#') && !inst->cwpressed) {
	if (try_clockwise(inst))
	    update_minimal(inst);
    }
    inst->cwpressed = KEY_PRESSED('#');

    if (KEY_PRESSED(' ') && !inst->rpressed) {
	if (try_reflect(inst))
	    update_minimal(inst);
    }
    inst->rpressed = KEY_PRESSED(' ');

    if (KEY_PRESSED('/') && !inst->down_disabled) {
	drop(inst);
	update_minimal(inst);
    }
    else if (!KEY_PRESSED('/') && inst->down_disabled) {
        inst->down_disabled = FALSE;
    }

    return TRUE;
}

void destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

gint focus_event(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    if (event->in)
	gdk_key_repeat_disable();
    else
	gdk_key_repeat_restore();
}

int main(int argc, char **argv)
{
    GtkWidget *window;
    struct game_instance the_inst;
    struct game_instance *inst = &the_inst;   /* so we always write `inst->' */

    inst->pixmap = NULL;
    memset(inst->keystate, 0, sizeof(inst->keystate));

    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    inst->area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(inst->area),
			  GAME_WIDTH, GAME_HEIGHT);

    gtk_container_add(GTK_CONTAINER(window), inst->area);

    gtk_signal_connect(GTK_OBJECT(window), "destroy",
		       GTK_SIGNAL_FUNC(destroy), inst);
    gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		       GTK_SIGNAL_FUNC(delete_window), inst);
    gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
		       GTK_SIGNAL_FUNC(key_event), inst);
    gtk_signal_connect(GTK_OBJECT(window), "key_release_event",
		       GTK_SIGNAL_FUNC(key_event), inst);
    gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
		       GTK_SIGNAL_FUNC(focus_event), inst);
    gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
		       GTK_SIGNAL_FUNC(focus_event), inst);
    gtk_signal_connect(GTK_OBJECT(inst->area), "configure_event",
		       GTK_SIGNAL_FUNC(configure_area), inst);
    gtk_signal_connect(GTK_OBJECT(inst->area), "expose_event",
		       GTK_SIGNAL_FUNC(expose_area), inst);
    gtk_timeout_add(20, timer_func, inst);
    gtk_widget_add_events(GTK_WIDGET(inst->area),
			  GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_widget_show(inst->area);
    gtk_widget_show(window);
    gdk_key_repeat_disable();

    gtk_main();

    gdk_key_repeat_restore();

    return 0;
}
