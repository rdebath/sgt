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
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>

#include "ntris.h"

#define GAME_WIDTH 640
#define GAME_HEIGHT 512

#define PLAY_WIDTH 10
#define PLAY_HEIGHT 30

#define SQUARE_SIDE 16
#define HIGHLIGHT 3
#define NCOLOURS 18
#define LIB_WIDTH (NCOLOURS*SQUARE_SIDE)
#define LIB_HEIGHT (256*SQUARE_SIDE)

#define CAT2(x,y) x ## y
#define CAT(x,y) CAT2(x,y)
#define ASSERT(x) enum {CAT(assertion_,__LINE__) = 1 / (x)}

#define lenof(x) (sizeof((x))/sizeof(*(x)))

struct frontend_instance {
    GtkWidget *area;
    GdkPixmap *pixmap, *library;
    GdkFont *font;
    GdkGC *black_gc, *white_gc, *random_gc;
    GdkColor cols[3*NCOLOURS];
    GdkColormap *colmap;
    gint32 keystate[65536/32];
    int leftpressed, rightpressed, acpressed, cwpressed, rpressed;
    int down_disabled;
    int update_minx, update_maxx, update_miny, update_maxy;
    enum { TITLESCR, IN_GAME } state;
    struct ntris_instance *ti;
};

void cls(struct frontend_instance *inst)
{
    gdk_draw_rectangle(inst->pixmap, inst->black_gc,
		       TRUE, 0, 0, GAME_WIDTH, GAME_HEIGHT);
}

void text(struct frontend_instance *inst, int x, int y, char *str)
{
    gdk_draw_text(inst->pixmap, inst->font, inst->white_gc, x, y,
		  str, strlen(str));
}

void block(struct frontend_instance *inst, int x, int y, int col, int type)
{
    x *= SQUARE_SIDE;
    y *= SQUARE_SIDE;
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

void titlescr(struct frontend_instance *inst)
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

void playarea(struct frontend_instance *inst)
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

void update_rect(struct frontend_instance *inst, int x, int y, int w, int h)
{
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    gtk_widget_draw(inst->area, &rect);
}

void update_all(struct frontend_instance *inst)
{
    update_rect(inst, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    inst->update_minx = GAME_WIDTH+1;
    inst->update_maxx = -1;
    inst->update_miny = GAME_HEIGHT+1;
    inst->update_maxy = -1;
}

void update_minimal(struct frontend_instance *inst)
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
    struct frontend_instance *inst = (struct frontend_instance *)data;

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
    struct frontend_instance *inst = (struct frontend_instance *)data;

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
    struct frontend_instance *inst = (struct frontend_instance *)data;

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
	    inst->ti = init_game(inst, PLAY_WIDTH, PLAY_HEIGHT, NULL);
	    inst->leftpressed = inst->rightpressed = 0;
	    inst->acpressed = inst->cwpressed = inst->rpressed = 0;
            inst->down_disabled = FALSE;
	    playarea(inst);
	    init_shape(inst->ti);
	    update_all(inst);
	}
    }
    return FALSE;
}

gint timer_func(gpointer data)
{
    struct frontend_instance *inst = (struct frontend_instance *)data;

    if (inst->state != IN_GAME)
	return TRUE;

    if (KEY_PRESSED('a') || (KEY_PRESSED('z') && !inst->leftpressed)) {
	if (try_move_left(inst->ti))
	    update_minimal(inst);
    }
    inst->leftpressed = KEY_PRESSED('z');

    if (KEY_PRESSED('s') || (KEY_PRESSED('x') && !inst->rightpressed)) {
	if (try_move_right(inst->ti))
	    update_minimal(inst);
    }
    inst->rightpressed = KEY_PRESSED('x');

    if (KEY_PRESSED('\'') && !inst->acpressed) {
	if (try_anticlock(inst->ti))
	    update_minimal(inst);
    }
    inst->acpressed = KEY_PRESSED('\'');

    if (KEY_PRESSED('#') && !inst->cwpressed) {
	if (try_clockwise(inst->ti))
	    update_minimal(inst);
    }
    inst->cwpressed = KEY_PRESSED('#');

    if (KEY_PRESSED(' ') && !inst->rpressed) {
	if (try_reflect(inst->ti))
	    update_minimal(inst);
    }
    inst->rpressed = KEY_PRESSED(' ');

    if (KEY_PRESSED('/') && !inst->down_disabled) {
	if (softdrop(inst->ti)) {
	    init_shape(inst->ti);      /* FIXME: check for game-over! */
            inst->down_disabled = TRUE;
        }
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
    return FALSE;
}

int main(int argc, char **argv)
{
    GtkWidget *window;
    struct frontend_instance the_inst;
    struct frontend_instance *inst = &the_inst;   /* so we always write `inst->' */

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
