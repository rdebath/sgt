/*
 * pterm - a fusion of the PuTTY terminal emulator with a Unix pty
 * back end, all running as a GTK application. Wish me luck.
 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define PUTTY_DO_GLOBALS	       /* actually _define_ globals */
#include "putty.h"

#define CAT2(x,y) x ## y
#define CAT(x,y) CAT2(x,y)
#define ASSERT(x) enum {CAT(assertion_,__LINE__) = 1 / (x)}

#define NCOLOURS (lenof(((Config *)0)->colours))

struct gui_data {
    GtkWidget *area, *sbar;
    GtkBox *hbox;
    GtkAdjustment *sbar_adjust;
    GdkPixmap *pixmap;
    GdkFont *fonts[2];                 /* normal and bold (for now!) */
    GdkCursor *rawcursor, *textcursor;
    GdkColor cols[NCOLOURS];
    GdkColormap *colmap;
    int font_width, font_height;
};

static struct gui_data the_inst;
static struct gui_data *inst = &the_inst;   /* so we always write `inst->' */
static int send_raw_mouse;

void ldisc_update(int echo, int edit)
{
    /*
     * This is a stub in pterm. If I ever produce a Unix
     * command-line ssh/telnet/rlogin client (i.e. a port of plink)
     * then it will require some termios manoeuvring analogous to
     * that in the Windows plink.c, but here it's meaningless.
     */
}

int askappend(char *filename)
{
    /*
     * FIXME: for the moment we just wipe the log file. Since I
     * haven't yet enabled logging, this shouldn't matter yet!
     */
    return 2;
}

void logevent(char *string)
{
    /*
     * FIXME: event log entries are currently ignored.
     */
}

/*
 * Translate a raw mouse button designation (LEFT, MIDDLE, RIGHT)
 * into a cooked one (SELECT, EXTEND, PASTE).
 * 
 * In Unix, this is not configurable; the X button arrangement is
 * rock-solid across all applications, everyone has a three-button
 * mouse or a means of faking it, and there is no need to switch
 * buttons around at all.
 */
Mouse_Button translate_button(Mouse_Button button)
{
    if (button == MBT_LEFT)
	return MBT_SELECT;
    if (button == MBT_MIDDLE)
	return MBT_PASTE;
    if (button == MBT_RIGHT)
	return MBT_EXTEND;
    return 0;			       /* shouldn't happen */
}

/*
 * Minimise or restore the window in response to a server-side
 * request.
 */
void set_iconic(int iconic)
{
    /* FIXME: currently ignored */
}

/*
 * Move the window in response to a server-side request.
 */
void move_window(int x, int y)
{
    /* FIXME: currently ignored */
}

/*
 * Move the window to the top or bottom of the z-order in response
 * to a server-side request.
 */
void set_zorder(int top)
{
    /* FIXME: currently ignored */
}

/*
 * Refresh the window in response to a server-side request.
 */
void refresh_window(void)
{
    /* FIXME: currently ignored */
}

/*
 * Maximise or restore the window in response to a server-side
 * request.
 */
void set_zoomed(int zoomed)
{
    /* FIXME: currently ignored */
}

/*
 * Report whether the window is iconic, for terminal reports.
 */
int is_iconic(void)
{
    return 0;			       /* FIXME */
}

/*
 * Report the window's position, for terminal reports.
 */
void get_window_pos(int *x, int *y)
{
    *x = 3; *y = 4;		       /* FIXME */
}

/*
 * Report the window's pixel size, for terminal reports.
 */
void get_window_pixels(int *x, int *y)
{
    *x = 1; *y = 2;		       /* FIXME */
}

/*
 * Return the window or icon title.
 */
char *get_window_title(int icon)
{
    return "FIXME: window title retrieval not yet implemented";
}

gint delete_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    /*
     * FIXME: warn on close?
     */
    return FALSE;
}

gint configure_area(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    struct gui_data *inst = (struct gui_data *)data;

    if (inst->pixmap)
	gdk_pixmap_unref(inst->pixmap);

    inst->pixmap = gdk_pixmap_new(widget->window,
				  (cfg.width * inst->font_width +
				   2*cfg.window_border),
				  (cfg.height * inst->font_height +
				   2*cfg.window_border), -1);

    {
	GdkGC *gc;
	gc = gdk_gc_new(inst->area->window);
	gdk_gc_set_foreground(gc, &inst->cols[18]);   /* default background */
	gdk_draw_rectangle(inst->pixmap, gc, 1, 0, 0,
			   cfg.width * inst->font_width + 2*cfg.window_border,
			   cfg.height * inst->font_height + 2*cfg.window_border);
	gdk_gc_unref(gc);
    }

    /*
     * Set up the colour map.
     */
    inst->colmap = gdk_colormap_get_system();
    {
	static const int ww[] = {
	    6, 7, 8, 9, 10, 11, 12, 13,
	    14, 15, 16, 17, 18, 19, 20, 21,
	    0, 1, 2, 3, 4, 5
	};
	gboolean success[NCOLOURS];
	int i;

	assert(lenof(ww) == NCOLOURS);

	for (i = 0; i < NCOLOURS; i++) {
	    inst->cols[i].red = cfg.colours[ww[i]][0] * 0x0101;
	    inst->cols[i].green = cfg.colours[ww[i]][1] * 0x0101;
	    inst->cols[i].blue = cfg.colours[ww[i]][2] * 0x0101;
	}

	gdk_colormap_alloc_colors(inst->colmap, inst->cols, NCOLOURS,
				  FALSE, FALSE, success);
	for (i = 0; i < NCOLOURS; i++) {
	    if (!success[i])
		g_error("pterm: couldn't allocate colour %d (#%02x%02x%02x)\n",
			i, cfg.colours[i][0], cfg.colours[i][1], cfg.colours[i][2]);
	}
    }

    return TRUE;
}

gint expose_area(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    /* struct gui_data *inst = (struct gui_data *)data; */

    /*
     * Pass the exposed rectangle to terminal.c, which will call us
     * back to do the actual painting.
     */
    if (inst->pixmap) {
	gdk_draw_pixmap(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
			inst->pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
    }
    return TRUE;
}

#define KEY_PRESSED(k) \
    (inst->keystate[(k) / 32] & (1 << ((k) % 32)))

gint key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    /* struct gui_data *inst = (struct gui_data *)data; */
    char output[32];
    int start, end;

    if (event->type == GDK_KEY_PRESS) {
#ifdef KEY_DEBUGGING
	{
	    int i;
	    printf("keypress: keyval = %04x, state = %08x; string =",
		   event->keyval, event->state);
	    for (i = 0; event->string[i]; i++)
		printf(" %02x", (unsigned char) event->string[i]);
	    printf("\n");
	}
#endif

	/*
	 * NYI:
	 *  - Shift-PgUp/PgDn for scrollbar
	 *  - nethack mode
	 *  - alt+numpad
	 *  - Compose key (!!! requires Unicode faff before even trying)
	 *  - Shift-Ins for paste (need to deal with pasting first)
	 */

	/*
	 * Shift-PgUp and Shift-PgDn don't even generate keystrokes
	 * at all.
	 */
	if (event->keyval == GDK_Page_Up && (event->state & GDK_SHIFT_MASK)) {
	    term_scroll(0, -cfg.height/2);
	    return TRUE;
	}
	if (event->keyval == GDK_Page_Down && (event->state & GDK_SHIFT_MASK)) {
	    term_scroll(0, +cfg.height/2);
	    return TRUE;
	}

	/* ALT+things gives leading Escape. */
	output[0] = '\033';
	strncpy(output+1, event->string, 31);
	output[31] = '\0';
	end = strlen(output);
	if (event->state & GDK_MOD1_MASK)
	    start = 0;
	else
	    start = 1;

	/* Control-` is the same as Control-\ (unless gtk has a better idea) */
	if (!event->string[0] && event->keyval == '`' &&
	    (event->state & GDK_CONTROL_MASK)) {
	    output[1] = '\x1C';
	    end = 2;
	}

	/* Control-Break is the same as Control-C */
	if (event->keyval == GDK_Break &&
	    (event->state & GDK_CONTROL_MASK)) {
	    output[1] = '\003';
	    end = 2;
	}

	/* Control-2, Control-Space and Control-@ are NUL */
	if (!event->string[0] &&
	    (event->keyval == ' ' || event->keyval == '2' ||
	     event->keyval == '@') &&
	    (event->state & (GDK_SHIFT_MASK |
			     GDK_CONTROL_MASK)) == GDK_CONTROL_MASK) {
	    output[1] = '\0';
	    end = 2;
	}

	/* Control-Shift-Space is 160 (ISO8859 nonbreaking space) */
	if (!event->string[0] && event->keyval == ' ' &&
	    (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) ==
	    (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
	    output[1] = '\240';
	    end = 2;
	}

	/* We don't let GTK tell us what Backspace is! We know better. */
	if (event->keyval == GDK_BackSpace &&
	    !(event->state & GDK_SHIFT_MASK)) {
	    output[1] = cfg.bksp_is_delete ? '\x7F' : '\x08';
	    end = 2;
	}

	/* Shift-Tab is ESC [ Z */
	if (event->keyval == GDK_ISO_Left_Tab ||
	    (event->keyval == GDK_Tab && (event->state & GDK_SHIFT_MASK))) {
	    end = 1 + sprintf(output+1, "\033[Z");
	}

	/*
	 * Application keypad mode.
	 */
	if (app_keypad_keys && !cfg.no_applic_k) {
	    int xkey = 0;
	    switch (event->keyval) {
	      case GDK_Num_Lock: xkey = 'P'; break;
	      case GDK_KP_Divide: xkey = 'Q'; break;
	      case GDK_KP_Multiply: xkey = 'R'; break;
	      case GDK_KP_Subtract: xkey = 'S'; break;
		/*
		 * Keypad + is tricky. It covers a space that would
		 * be taken up on the VT100 by _two_ keys; so we
		 * let Shift select between the two. Worse still,
		 * in xterm function key mode we change which two...
		 */
	      case GDK_KP_Add:
		if (cfg.funky_type == 2) {
		    if (event->state & GDK_SHIFT_MASK)
			xkey = 'l';
		    else
			xkey = 'k';
		} else if (event->state & GDK_SHIFT_MASK)
			xkey = 'm';
		else
		    xkey = 'l';
		break;
	      case GDK_KP_Enter: xkey = 'M'; break;
	      case GDK_KP_0: case GDK_KP_Insert: xkey = 'p'; break;
	      case GDK_KP_1: case GDK_KP_End: xkey = 'q'; break;
	      case GDK_KP_2: case GDK_KP_Down: xkey = 'r'; break;
	      case GDK_KP_3: case GDK_KP_Page_Down: xkey = 's'; break;
	      case GDK_KP_4: case GDK_KP_Left: xkey = 't'; break;
	      case GDK_KP_5: case GDK_KP_Begin: xkey = 'u'; break;
	      case GDK_KP_6: case GDK_KP_Right: xkey = 'v'; break;
	      case GDK_KP_7: case GDK_KP_Home: xkey = 'w'; break;
	      case GDK_KP_8: case GDK_KP_Up: xkey = 'x'; break;
	      case GDK_KP_9: case GDK_KP_Page_Up: xkey = 'y'; break;
	      case GDK_KP_Decimal: case GDK_KP_Delete: xkey = 'n'; break;
	    }
	    if (xkey) {
		if (vt52_mode) {
		    if (xkey >= 'P' && xkey <= 'S')
			end = 1 + sprintf(output+1, "\033%c", xkey);
		    else
			end = 1 + sprintf(output+1, "\033?%c", xkey);
		} else
		    end = 1 + sprintf(output+1, "\033O%c", xkey);
		goto done;
	    }
	}

	/*
	 * Next, all the keys that do tilde codes. (ESC '[' nn '~',
	 * for integer decimal nn.)
	 *
	 * We also deal with the weird ones here. Linux VCs replace F1
	 * to F5 by ESC [ [ A to ESC [ [ E. rxvt doesn't do _that_, but
	 * does replace Home and End (1~ and 4~) by ESC [ H and ESC O w
	 * respectively.
	 */
	{
	    int code = 0;
	    switch (event->keyval) {
	      case GDK_F1:
		code = (event->state & GDK_SHIFT_MASK ? 23 : 11);
		break;
	      case GDK_F2:
		code = (event->state & GDK_SHIFT_MASK ? 24 : 12);
		break;
	      case GDK_F3:
		code = (event->state & GDK_SHIFT_MASK ? 25 : 13);
		break;
	      case GDK_F4:
		code = (event->state & GDK_SHIFT_MASK ? 26 : 14);
		break;
	      case GDK_F5:
		code = (event->state & GDK_SHIFT_MASK ? 28 : 15);
		break;
	      case GDK_F6:
		code = (event->state & GDK_SHIFT_MASK ? 29 : 17);
		break;
	      case GDK_F7:
		code = (event->state & GDK_SHIFT_MASK ? 31 : 18);
		break;
	      case GDK_F8:
		code = (event->state & GDK_SHIFT_MASK ? 32 : 19);
		break;
	      case GDK_F9:
		code = (event->state & GDK_SHIFT_MASK ? 33 : 20);
		break;
	      case GDK_F10:
		code = (event->state & GDK_SHIFT_MASK ? 34 : 21);
		break;
	      case GDK_F11:
		code = 23;
		break;
	      case GDK_F12:
		code = 24;
		break;
	      case GDK_F13:
		code = 25;
		break;
	      case GDK_F14:
		code = 26;
		break;
	      case GDK_F15:
		code = 28;
		break;
	      case GDK_F16:
		code = 29;
		break;
	      case GDK_F17:
		code = 31;
		break;
	      case GDK_F18:
		code = 32;
		break;
	      case GDK_F19:
		code = 33;
		break;
	      case GDK_F20:
		code = 34;
		break;
	    }
	    if (!(event->state & GDK_CONTROL_MASK)) switch (event->keyval) {
	      case GDK_Home: case GDK_KP_Home:
		code = 1;
		break;
	      case GDK_Insert: case GDK_KP_Insert:
		code = 2;
		break;
	      case GDK_Delete: case GDK_KP_Delete:
		code = 3;
		break;
	      case GDK_End: case GDK_KP_End:
		code = 4;
		break;
	      case GDK_Page_Up: case GDK_KP_Page_Up:
		code = 5;
		break;
	      case GDK_Page_Down: case GDK_KP_Page_Down:
		code = 6;
		break;
	    }
	    /* Reorder edit keys to physical order */
	    if (cfg.funky_type == 3 && code <= 6)
		code = "\0\2\1\4\5\3\6"[code];

	    if (vt52_mode && code > 0 && code <= 6) {
		end = 1 + sprintf(output+1, "\x1B%c", " HLMEIG"[code]);
		goto done;
	    }

	    if (cfg.funky_type == 5 &&     /* SCO function keys */
		code >= 11 && code <= 34) {
		char codes[] = "MNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@[\\]^_`{";
		int index = 0;
		switch (event->keyval) {
		  case GDK_F1: index = 0; break;
		  case GDK_F2: index = 1; break;
		  case GDK_F3: index = 2; break;
		  case GDK_F4: index = 3; break;
		  case GDK_F5: index = 4; break;
		  case GDK_F6: index = 5; break;
		  case GDK_F7: index = 6; break;
		  case GDK_F8: index = 7; break;
		  case GDK_F9: index = 8; break;
		  case GDK_F10: index = 9; break;
		  case GDK_F11: index = 10; break;
		  case GDK_F12: index = 11; break;
		}
		if (event->state & GDK_SHIFT_MASK) index += 12;
		if (event->state & GDK_CONTROL_MASK) index += 24;
		end = 1 + sprintf(output+1, "\x1B[%c", codes[index]);
		goto done;
	    }
	    if (cfg.funky_type == 5 &&     /* SCO small keypad */
		code >= 1 && code <= 6) {
		char codes[] = "HL.FIG";
		if (code == 3) {
		    output[1] = '\x7F';
		    end = 2;
		} else {
		    end = 1 + sprintf(output+1, "\x1B[%c", codes[code-1]);
		}
		goto done;
	    }
	    if ((vt52_mode || cfg.funky_type == 4) && code >= 11 && code <= 24) {
		int offt = 0;
		if (code > 15)
		    offt++;
		if (code > 21)
		    offt++;
		if (vt52_mode)
		    end = 1 + sprintf(output+1,
				      "\x1B%c", code + 'P' - 11 - offt);
		else
		    end = 1 + sprintf(output+1,
				      "\x1BO%c", code + 'P' - 11 - offt);
		goto done;
	    }
	    if (cfg.funky_type == 1 && code >= 11 && code <= 15) {
		end = 1 + sprintf(output+1, "\x1B[[%c", code + 'A' - 11);
		goto done;
	    }
	    if (cfg.funky_type == 2 && code >= 11 && code <= 14) {
		if (vt52_mode)
		    end = 1 + sprintf(output+1, "\x1B%c", code + 'P' - 11);
		else
		    end = 1 + sprintf(output+1, "\x1BO%c", code + 'P' - 11);
		goto done;
	    }
	    if (cfg.rxvt_homeend && (code == 1 || code == 4)) {
		end = 1 + sprintf(output+1, code == 1 ? "\x1B[H" : "\x1BOw");
		goto done;
	    }
	    if (code) {
		end = 1 + sprintf(output+1, "\x1B[%d~", code);
		goto done;
	    }
	}

	/*
	 * Cursor keys. (This includes the numberpad cursor keys,
	 * if we haven't already done them due to app keypad mode.)
	 * 
	 * Here we also process un-numlocked un-appkeypadded KP5,
	 * which sends ESC [ G.
	 */
	{
	    int xkey = 0;
	    switch (event->keyval) {
	      case GDK_Up: case GDK_KP_Up: xkey = 'A'; break;
	      case GDK_Down: case GDK_KP_Down: xkey = 'B'; break;
	      case GDK_Right: case GDK_KP_Right: xkey = 'C'; break;
	      case GDK_Left: case GDK_KP_Left: xkey = 'D'; break;
	      case GDK_Begin: case GDK_KP_Begin: xkey = 'G'; break;
	    }
	    if (xkey) {
		/*
		 * The arrow keys normally do ESC [ A and so on. In
		 * app cursor keys mode they do ESC O A instead.
		 * Ctrl toggles the two modes.
		 */
		if (vt52_mode) {
		    end = 1 + sprintf(output+1, "\033%c", xkey);
		} else if (!app_cursor_keys ^
			   !(event->state & GDK_CONTROL_MASK)) {
		    end = 1 + sprintf(output+1, "\033O%c", xkey);
		} else {		    
		    end = 1 + sprintf(output+1, "\033[%c", xkey);
		}
		goto done;
	    }
	}

	done:

#ifdef KEY_DEBUGGING
	{
	    int i;
	    printf("generating sequence:");
	    for (i = start; i < end; i++)
		printf(" %02x", (unsigned char) output[i]);
	    printf("\n");
	}
#endif
	ldisc_send(output+start, end-start, 1);
	term_out();
    }

    return TRUE;
}

gint timer_func(gpointer data)
{
    /* struct gui_data *inst = (struct gui_data *)data; */

    term_update();
    return TRUE;
}

void pty_input_func(gpointer data, gint sourcefd, GdkInputCondition condition)
{
    /* struct gui_data *inst = (struct gui_data *)data; */
    char buf[4096];
    int ret;

    ret = read(sourcefd, buf, sizeof(buf));

    /*
     * Clean termination condition is that either ret == 0, or ret
     * < 0 and errno == EIO. Not sure why the latter, but it seems
     * to happen. Boo.
     */
    if (ret == 0 || (ret < 0 && errno == EIO)) {
	exit(0);
    }

    if (ret < 0) {
	perror("read pty master");
	exit(1);
    }
    if (ret > 0)
	from_backend(0, buf, ret);
    term_out();
}

void destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

gint focus_event(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    has_focus = event->in;
    term_out();
    term_update();
    return FALSE;
}

/*
 * set or clear the "raw mouse message" mode
 */
void set_raw_mouse_mode(int activate)
{
    activate = activate && !cfg.no_mouse_rep;
    send_raw_mouse = activate;
    if (send_raw_mouse)
	gdk_window_set_cursor(inst->area->window, inst->rawcursor);
    else
	gdk_window_set_cursor(inst->area->window, inst->textcursor);
}

void request_resize(int w, int h)
{
    /* FIXME: currently ignored */
}

void palette_set(int n, int r, int g, int b)
{
    /* FIXME: currently ignored */
}
void palette_reset(void)
{
    /* FIXME: currently ignored */
}

void write_clip(wchar_t * data, int len, int must_deselect)
{
    /* FIXME: currently ignored */
}

void get_clip(wchar_t ** p, int *len)
{
    if (p) {
	/* FIXME: currently nonfunctional */
	*p = NULL;
	*len = 0;
    }
}

void set_title(char *title)
{
    /* FIXME: currently ignored */
}

void set_icon(char *title)
{
    /* FIXME: currently ignored */
}

void set_sbar(int total, int start, int page)
{
    inst->sbar_adjust->lower = 0;
    inst->sbar_adjust->upper = total;
    inst->sbar_adjust->value = start;
    inst->sbar_adjust->page_size = page;
    inst->sbar_adjust->step_increment = 1;
    inst->sbar_adjust->page_increment = page/2;
    gtk_adjustment_changed(inst->sbar_adjust);
}

void scrollbar_moved(GtkAdjustment *adj, gpointer data)
{
    term_scroll(1, (int)adj->value);
}

void sys_cursor(int x, int y)
{
    /*
     * This is meaningless under X.
     */
}

void beep(int mode)
{
    gdk_beep();
}

int CharWidth(Context ctx, int uc)
{
    /*
     * Under X, any fixed-width font really _is_ fixed-width.
     * Double-width characters will be dealt with using a separate
     * font. For the moment we can simply return 1.
     */
    return 1;
}

Context get_ctx(void)
{
    GdkGC *gc;
    if (!inst->area->window)
	return NULL;
    gc = gdk_gc_new(inst->area->window);
    return gc;
}

void free_ctx(Context ctx)
{
    GdkGC *gc = (GdkGC *)ctx;
    gdk_gc_unref(gc);
}

/*
 * Draw a line of text in the window, at given character
 * coordinates, in given attributes.
 *
 * We are allowed to fiddle with the contents of `text'.
 */
void do_text(Context ctx, int x, int y, char *text, int len,
	     unsigned long attr, int lattr)
{
    int nfg, nbg, t;
    GdkGC *gc = (GdkGC *)ctx;

    /*
     * NYI:
     *  - ATTR_WIDE (is this for Unicode CJK? I hope so)
     *  - LATTR_* (ESC # 4 double-width and double-height stuff)
     *  - cursor shapes other than block
     *  - VT100 line drawing stuff; code pages in general!
     *  - shadow bolding
     *  - underline
     */

    nfg = 2 * ((attr & ATTR_FGMASK) >> ATTR_FGSHIFT);
    nbg = 2 * ((attr & ATTR_BGMASK) >> ATTR_BGSHIFT);
    if (attr & ATTR_REVERSE) {
	t = nfg;
	nfg = nbg;
	nbg = t;
    }
    if (cfg.bold_colour && (attr & ATTR_BOLD))
	nfg++;
    if (cfg.bold_colour && (attr & ATTR_BLINK))
	nbg++;
    if (attr & TATTR_ACTCURS) {
	nfg = NCOLOURS-2;
	nbg = NCOLOURS-1;
    }

    gdk_gc_set_foreground(gc, &inst->cols[nbg]);
    gdk_draw_rectangle(inst->pixmap, gc, 1,
		       x*inst->font_width+cfg.window_border,
		       y*inst->font_height+cfg.window_border,
		       len*inst->font_width, inst->font_height);

    gdk_gc_set_foreground(gc, &inst->cols[nfg]);
    gdk_draw_text(inst->pixmap, inst->fonts[0], gc,
		  x*inst->font_width+cfg.window_border,
		  y*inst->font_height+cfg.window_border+inst->fonts[0]->ascent,
		  text, len);

    if (attr & ATTR_UNDER) {
	int uheight = inst->fonts[0]->ascent + 1;
	if (uheight >= inst->font_height)
	    uheight = inst->font_height - 1;
	gdk_draw_line(inst->pixmap, gc, x*inst->font_width+cfg.window_border,
		      y*inst->font_height + uheight + cfg.window_border,
		      (x+len)*inst->font_width-1+cfg.window_border,
		      y*inst->font_height + uheight + cfg.window_border);
    }

    gdk_draw_pixmap(inst->area->window, gc, inst->pixmap,
		    x*inst->font_width+cfg.window_border,
		    y*inst->font_height+cfg.window_border,
		    x*inst->font_width+cfg.window_border,
		    y*inst->font_height+cfg.window_border,
		    len*inst->font_width, inst->font_height);
}

void do_cursor(Context ctx, int x, int y, char *text, int len,
	       unsigned long attr, int lattr)
{
    int passive;
    GdkGC *gc = (GdkGC *)ctx;

    /*
     * NYI: cursor shapes other than block
     */
    if (attr & TATTR_PASCURS) {
	attr &= ~TATTR_PASCURS;
	passive = 1;
    } else
	passive = 0;
    do_text(ctx, x, y, text, len, attr, lattr);
    if (passive) {
	gdk_gc_set_foreground(gc, &inst->cols[NCOLOURS-1]);
	gdk_draw_rectangle(inst->pixmap, gc, 0,
			   x*inst->font_width+cfg.window_border,
			   y*inst->font_height+cfg.window_border,
			   len*inst->font_width-1, inst->font_height-1);
	gdk_draw_pixmap(inst->area->window, gc, inst->pixmap,
			x*inst->font_width+cfg.window_border,
			y*inst->font_height+cfg.window_border,
			x*inst->font_width+cfg.window_border,
			y*inst->font_height+cfg.window_border,
			len*inst->font_width, inst->font_height);
    }
}

void modalfatalbox(char *p, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    GtkWidget *window;
    extern int pty_master_fd;	       /* declared in pty.c */

    gtk_init(&argc, &argv);

    do_defaults(NULL, &cfg);

    inst->fonts[0] = gdk_font_load(cfg.font);
    inst->fonts[1] = NULL;             /* FIXME: what about bold font? */
    inst->font_width = gdk_char_width(inst->fonts[0], ' ');
    inst->font_height = inst->fonts[0]->ascent + inst->fonts[0]->descent;

    init_ucs();

    back = &pty_backend;
    back->init(NULL, 0, NULL, 0);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    inst->area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(inst->area),
			  inst->font_width * cfg.width + 2*cfg.window_border,
			  inst->font_height * cfg.height + 2*cfg.window_border);
    inst->sbar_adjust = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
    inst->sbar = gtk_vscrollbar_new(inst->sbar_adjust);
    inst->hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_box_pack_start(inst->hbox, inst->area, FALSE, FALSE, 0);
    gtk_box_pack_start(inst->hbox, inst->sbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(inst->hbox));
//    gtk_container_add(GTK_CONTAINER(window), inst->sbar);

    {
	GdkGeometry geom;
	geom.min_width = inst->font_width + 2*cfg.window_border;
	geom.min_height = inst->font_height + 2*cfg.window_border;
	geom.max_width = geom.max_height = -1;
	geom.base_width = 2*cfg.window_border;
	geom.base_height = 2*cfg.window_border;
	geom.width_inc = inst->font_width;
	geom.height_inc = inst->font_height;
	geom.min_aspect = geom.max_aspect = 0;
	gtk_window_set_geometry_hints(GTK_WINDOW(window), inst->area, &geom,
				      GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE |
				      GDK_HINT_RESIZE_INC);
	// FIXME: base_width and base_height don't seem to work properly.
	// Wonder if this is because base of _zero_ is deprecated, and we
	// need a nonzero base size? Will it help if I put in the scrollbar?
	// FIXME also: the scrollbar is not working, find out why.
    }

    gtk_signal_connect(GTK_OBJECT(window), "destroy",
		       GTK_SIGNAL_FUNC(destroy), inst);
    gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		       GTK_SIGNAL_FUNC(delete_window), inst);
    gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
		       GTK_SIGNAL_FUNC(key_event), inst);
    gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
		       GTK_SIGNAL_FUNC(focus_event), inst);
    gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
		       GTK_SIGNAL_FUNC(focus_event), inst);
    gtk_signal_connect(GTK_OBJECT(inst->area), "configure_event",
		       GTK_SIGNAL_FUNC(configure_area), inst);
    gtk_signal_connect(GTK_OBJECT(inst->area), "expose_event",
		       GTK_SIGNAL_FUNC(expose_area), inst);
    gtk_signal_connect(GTK_OBJECT(inst->sbar_adjust), "value_changed",
		       GTK_SIGNAL_FUNC(scrollbar_moved), inst);
    gtk_timeout_add(20, timer_func, inst);
    gdk_input_add(pty_master_fd, GDK_INPUT_READ, pty_input_func, inst);
    gtk_widget_add_events(GTK_WIDGET(inst->area),
			  GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_widget_show(inst->area);
    gtk_widget_show(inst->sbar);
    gtk_widget_show(GTK_WIDGET(inst->hbox));
    gtk_widget_show(window);

    inst->textcursor = gdk_cursor_new(GDK_XTERM);
    inst->rawcursor = gdk_cursor_new(GDK_ARROW);
    gdk_window_set_cursor(inst->area->window, inst->textcursor);

    term_init();
    term_size(24, 80, 2000);

    gtk_main();

    return 0;
}
