/*
 * Header file for dither.c.
 */

#include "png.h"

void dither_image(int w, int h, const png_pixel *input, 
		  const png_pixel *palette, int palettelen,
		  unsigned char *output);
