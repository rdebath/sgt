/*
 * Back end for `v' based on imlib2.
 */

#include "v.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <Imlib2.h>
#include <gdk/gdkx.h>

struct image {
    Imlib_Image image;
};

void image_init(void)
{
}

image *image_load(char *filename, char **err)
{
    Imlib_Image im;
    image *ret;

    im = imlib_load_image(filename);
    if (!im) {
	if (err)
	    *err = "imlib_load_image returned failure";
	return NULL;
    } else {
	ret = snew(image);
	ret->image = im;
	return ret;
    }
}

void image_free(image *img)
{
    imlib_context_set_image(img->image);
    imlib_free_image();
    sfree(img);
}

int image_width(image *img)
{
    imlib_context_set_image(img->image);
    return imlib_image_get_width();
}

int image_height(image *img)
{
    imlib_context_set_image(img->image);
    return imlib_image_get_height();
}

void image_to_pixmap(image *img, GdkPixmap *pm, int w, int h)
{
    int realw, realh;
    int need_free;

    imlib_context_set_image(img->image);
    realw = imlib_image_get_width();
    realh = imlib_image_get_height();
    if (w != realw || h != realh) {
	Imlib_Image newimg;

	newimg = imlib_create_cropped_scaled_image(0, 0, realw, realh, w, h);
	imlib_context_set_image(newimg);
	need_free = TRUE;
    } else
	need_free = FALSE;

    imlib_context_set_display(GDK_WINDOW_XDISPLAY(pm));
    imlib_context_set_visual(GDK_VISUAL_XVISUAL(gdk_visual_get_system()));
    imlib_context_set_colormap
	(GDK_COLORMAP_XCOLORMAP(gdk_colormap_get_system()));
    imlib_context_set_drawable(GDK_WINDOW_XWINDOW(pm));
    imlib_context_set_blend(1);
    imlib_render_image_on_drawable(0, 0);

    if (need_free)
	imlib_free_image();
}
