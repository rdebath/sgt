/*
 * `v', a simple image viewer designed to lack the various annoying
 * UI glitches I've encountered in other image viewers.
 */

/*
 * Things I don't like about other viewers:
 *
 *  - eeyes is mostly nice but really, really needs keypresses to
 *    be interpreted identically whether the list or image window
 *    is visible. Also I think I'd prefer the list window to be
 *    displayed only on request.
 *
 *  - ImageMagick's `display' is way too slow, and doesn't even
 *    have a list window.
 *
 *  - xv is shareware, and also slow (though not as slow as
 *    `display'), and I don't much like some of its design
 *    decisions (such as the `Bummer' box rather than a
 *    command-line error on startup, and the selection semantics in
 *    the list box). It also suffers from the eeyes problem (you
 *    can get caught out by which window is active).
 *
 *  - eog has too many panes; most if not all of an image viewer's
 *    screen space should be devoted to the image!
 */

/*
 * Desired features:
 *
 *  - ability to load lots of images and step back and forth
 *    between them.
 *     + passing wildcards / file lists on the command line,
 *       obviously
 *     + also it might be handy to be able to prepare a text file
 *       containing a list of image file names, and have v read
 *       from the text file (or stdin). Might be handy next time I
 *       want to create an animation (fractal, raytraced or
 *       whatever).
 *
 *  - scaling of images so they fit on the screen.
 *     + so we need to sense the screen size
 *     + and also it would be good to somehow sense the presence of
 *       the GNOME taskbar, although I've no idea how you can tell
 *       that.
 *     + should also be able to switch back to 1-1 viewing, which
 *       probably means scrollbars.
 *     + manual scaling would probably be nice too.
 * 
 *  - manual rotation by 90 degrees at a time would be helpful.
 *
 *  - an ee-like list window is a possibility, but can get in the
 *    way as much as it helps when viewing a big image. Uncertain.
 * 
 *  - I'm wondering about a mode in which the window stays the same
 *    size and images are centred in it (or scaled down /
 *    scrollbarred to fit), as well as the obvious mode in which
 *    the window changes size to adapt to each image.
 *     + in this mode, a good initial window size might be the
 *       maximum of all the images being shown?
 *     + perhaps a one-pixel black border within the grey default
 *       window background, which would have the advantage of
 *       making it always unambiguous where the image actually
 *       began?
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "v.h"

/*
 * FIXME: These should be set from the current display size.
 * Tricky, because I'd also like to take the GNOME taskbar into
 * account. Not sure how to do that.
 */
int maxw = 1574, maxh = 1106;

void fatal(char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "fatal error: ");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    exit(1);
}

struct ilist {
    image **images;
    char **filenames;
    int n, size;
};

struct ilist *ilist_new(void)
{
    struct ilist *il = snew(struct ilist);
    il->images = NULL;
    il->filenames = NULL;
    il->n = il->size = 0;
    return il;
}

char *ilist_load(struct ilist *il, char *filename)
{
    char *err;
    image *im = image_load(filename, &err);
    if (!im)
	return err;
    else {
	if (il->n >= il->size) {
	    il->size = il->size * 3 / 2 + 16;
	    il->images = sresize(il->images, il->size, image *);
	    il->filenames = sresize(il->filenames, il->size, char *);
	}
	il->images[il->n] = im;
	il->filenames[il->n] = filename;
	il->n++;
	return NULL;
    }
}

void ilist_free(struct ilist *il)
{
    while (il->n > 0) {
	il->n--;
        image_free(il->images[il->n]);
	sfree(il->filenames[il->n]);
    }
    sfree(il->images);
    sfree(il->filenames);
    sfree(il);
}

struct window {
    struct ilist *il;
    int pos;
    GtkWidget *window, *area;
    GdkPixmap *pm;
};

static void switch_to_image(struct window *w, int index)
{
    int iw, ih;
    image *im;

    assert(w->il->n > index);

    w->pos = index;

    im = w->il->images[index];
    iw = image_width(im);
    ih = image_height(im);

    /*
     * Scale the image down if it's too big.
     */
    if (iw > maxw || ih > maxh) {
	float wscale = (float)iw / maxw;
	float hscale = (float)ih / maxh;
	float scale = (wscale > hscale ? wscale : hscale);
	iw /= scale;
	ih /= scale;
    }

    gtk_drawing_area_size(GTK_DRAWING_AREA(w->area), iw, ih);
    gtk_window_set_title(GTK_WINDOW(w->window), w->il->filenames[index]);

    /*
     * This section is conditional because we might not have shown
     * the window at all yet.
     */
    if (w->window->window) {
	GtkRequisition req;
	gtk_widget_size_request(w->window, &req);
	gdk_window_resize(w->window->window, req.width, req.height);

        if (w->pm)
            gdk_pixmap_unref(w->pm);

        w->pm = gdk_pixmap_new(w->area->window, iw, ih, -1);
        image_to_pixmap(im, w->pm, iw, ih);
    }
}

static void window_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static gint expose_area(GtkWidget *widget, GdkEventExpose *event,
                        gpointer data)
{
    struct window *w = (struct window *)data;

    /*
     * If we ever need to position a small image within a larger
     * drawing area, the `ox' and `oy' fragments commented out here
     * should do all that's necessary.
     */
    if (w->pm) {
	gdk_draw_pixmap(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
			w->pm,
			event->area.x /* - w->ox */,
                        event->area.y /* - w->oy */,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
    }
    return TRUE;
}

static int win_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    struct window *w = (struct window *)data;

    if (event->string[0] && !event->string[1]) {
	int c = event->string[0];

	if (c == 'q' || c == 'Q' || c == '\x11')
	    gtk_widget_destroy(w->window);/* quit */
	if ((c == ' ' || c == 'n' || c == 'N') && w->pos+1 < w->il->n)
	    switch_to_image(w, w->pos+1);
	if ((c == '\010' || c == '\177' ||
             c == 'b' || c == 'B' ||
             c == 'p' || c == 'P') && w->pos > 0)
	    switch_to_image(w, w->pos-1);
    }

    return FALSE;
}

static struct window *new_window(struct ilist *il)
{
    struct window *w = snew(struct window);

    w->il = il;

    w->pm = NULL;

    w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    w->area = gtk_drawing_area_new();

    gtk_widget_show(w->area);
    gtk_container_add(GTK_CONTAINER(w->window), w->area);

    gtk_signal_connect(GTK_OBJECT(w->window), "destroy",
                       GTK_SIGNAL_FUNC(window_destroy), NULL);
    gtk_signal_connect(GTK_OBJECT(w->window), "key_press_event",
		       GTK_SIGNAL_FUNC(win_key_press), w);
    gtk_signal_connect(GTK_OBJECT(w->area), "expose_event",
		       GTK_SIGNAL_FUNC(expose_area), w);

    switch_to_image(w, 0);             /* just to calculate the size */

    gtk_widget_show(w->window);

    switch_to_image(w, 0);             /* now actually create the pixmap */

    return w;
}

int main(int argc, char **argv)
{
    struct ilist *il;
    int opts = TRUE, errs = FALSE;

    gtk_init(&argc, &argv);
    image_init();

    il = ilist_new();
    while (--argc > 0) {
	char *p = *++argv;

	if (opts && *p == '-') {
	    if (!strcmp(p, "--")) {
		opts = FALSE;
		continue;
	    } else {
		fprintf(stderr, "v: unrecognised command-line"
			" option '%s'\n", p);
		errs = TRUE;
	    }
	} else {
	    char *err = ilist_load(il, p);
	    if (err) {
		fprintf(stderr, "v: unable to load image '%s': %s\n", p, err);
		errs = TRUE;
	    }
	}
    }

    if (errs)
	return 1;

    if (il->n == 0) {
	fprintf(stderr, "usage: v <image> [ <image>... ]\n");
	return 0;
    }

    new_window(il);

    gtk_main();

    return 0;
}
