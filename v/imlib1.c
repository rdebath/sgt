/*
 * Back end for `v' based on gdk-imlib1.
 */

#include <gdk_imlib.h>

#include "v.h"

struct image {
    GdkImlibImage *im;
};

void image_init(void)
{
    gdk_imlib_init();
    gtk_widget_push_visual(gdk_imlib_get_visual());
    gtk_widget_push_colormap(gdk_imlib_get_colormap());
}

image *image_load(char *filename, char **err)
{
    GdkImlibImage *im;
    image *ret;

    im = gdk_imlib_load_image(filename);
    if (!im) {
        if (err)
            *err = "gdk_imlib_load_image failed";
        return NULL;
    }
    ret = snew(image);
    ret->im = im;
    return ret;
}

void image_free(image *img)
{
    gdk_imlib_destroy_image(img->im);
    sfree(img);
}

int image_width(image *img)
{
    return img->im->rgb_width;
}

int image_height(image *img)
{
    return img->im->rgb_height;
}

void image_to_pixmap(image *img, GdkPixmap *pm, int w, int h)
{
    GdkPixmap *ourpm;
    GdkGC *gc;

    gdk_imlib_render(img->im, w, h);
    ourpm = gdk_imlib_move_image(img->im);

    gc = gdk_gc_new(pm);
    gdk_draw_pixmap(pm, gc, ourpm, 0, 0, 0, 0, w, h);
    gdk_gc_unref(gc);

    gdk_imlib_free_pixmap(ourpm);
}
