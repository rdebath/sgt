/*
 * Header file for a simple colour quantiser.
 */

#include "png.h"		       /* for png_pixel */

typedef struct colquant colquant;

colquant *colquant_new(int ncolours, int depth);
void colquant_free(colquant *cq);
void colquant_data(colquant *cq, png_pixel *pixels, int npixels);
int colquant_get_palette(colquant *cq, png_pixel *palette);
