/*
 * v.h: header for my image viewer `v'.
 */

#ifndef V_V_H
#define V_V_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <stdlib.h> /* for size_t */

/*
 * v.c
 */
void fatal(char *fmt, ...);

/*
 * malloc.c
 */
void *smalloc(size_t size);
void *srealloc(void *p, size_t size);
void sfree(void *p);
char *dupstr(const char *s);
#define snew(type) \
    ( (type *) smalloc (sizeof (type)) )
#define snewn(number, type) \
    ( (type *) smalloc ((number) * sizeof (type)) )
#define sresize(array, number, type) \
    ( (type *) srealloc ((array), (number) * sizeof (type)) )

/*
 * Image backend files.
 */
typedef struct image image;
void image_init(void);
image *image_load(char *filename, char **err);
void image_free(image *img);
int image_width(image *img);
int image_height(image *img);
void image_to_pixmap(image *img, GdkPixmap *pm, int w, int h);

/*
 * bumf.c
 */
void licence(void);
void help(void);
void usage(void);
void showversion(void);

#endif
