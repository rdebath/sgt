/*
 * Dithering image palettiser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "png.h"

void dither_image(int w, int h, const png_pixel *input, 
		  const png_pixel *palette, int palettelen,
		  unsigned char *output)
{
    typedef struct internal_pixel {
	int r, g, b;
    } internal_pixel;
    internal_pixel *row1, *row2, *thisrow, *nextrow, *tmprow;
    int x, y;

    row1 = malloc(w * sizeof(internal_pixel));
    row2 = malloc(w * sizeof(internal_pixel));

    assert(row1 && row2);	       /* er, FIXME? */

    thisrow = row1;
    nextrow = row2;

    /*
     * Starting this loop at -1 causes thisrow to be filled up
     * before we start the first real row.
     */
    for (y = -1; y < h; y++) {
	if (y+1 < h) {
	    for (x = 0; x < w; x++) {
		nextrow[x].r = input[(y+1)*w+x].r;
		nextrow[x].g = input[(y+1)*w+x].g;
		nextrow[x].b = input[(y+1)*w+x].b;
	    }
	}

	if (y >= 0) {
	    for (x = 0; x < w; x++) {
		int dist, bestdist, besti, i;
		int dr, dg, db;

		/*
		 * Find the nearest palette element to this colour,
		 * by (approximate) Euclidean distance.
		 */
		bestdist = INT_MAX;
		besti = -1;
		for (i = 0; i < palettelen; i++) {
		    dr = palette[i].r - thisrow[x].r;
		    dg = palette[i].g - thisrow[x].g;
		    db = palette[i].b - thisrow[x].b;

		    /*
		     * Squaring three 16-bit numbers and adding
		     * them together would overflow a 32-bit
		     * number, so instead we shift one of them
		     * right by 8 before squaring. This still
		     * leaves a _strictly_ monotonic distance
		     * function, which should be good enough.
		     */
		    dist = dr * (dr >> 8) + dg * (dg >> 8) + db * (db >> 8);
		    if (dist < bestdist) {
			bestdist = dist;
			besti = i;
		    }
		}
		assert(besti >= 0);

		/*
		 * Write the pixel value into the output array.
		 */
		i = besti;
		output[y*w+x] = i;

		/*
		 * Recompute the error term, and apply
		 * Floyd-Steinberg error diffusion.
		 */
		dr = palette[i].r - thisrow[x].r;
		dg = palette[i].g - thisrow[x].g;
		db = palette[i].b - thisrow[x].b;
		if (x > 0) {
		    nextrow[x-1].r -= 1*dr/16;
		    nextrow[x-1].g -= 1*dg/16;
		    nextrow[x-1].b -= 1*db/16;
		}
		if (x+1 < w) {
		    thisrow[x+1].r -= 7*dr/16;
		    thisrow[x+1].g -= 7*dg/16;
		    thisrow[x+1].b -= 7*db/16;
		    nextrow[x+1].r -= 1*dr/16;
		    nextrow[x+1].g -= 1*dg/16;
		    nextrow[x+1].b -= 1*db/16;
		}
		nextrow[x].r -= 5*dr/16;
		nextrow[x].g -= 5*dg/16;
		nextrow[x].b -= 5*db/16;
	    }
	}

	tmprow = nextrow;
	nextrow = thisrow;
	thisrow = tmprow;
    }

    free(row1);
    free(row2);
}

#ifdef DITHER_TESTMODE

/*
 * Combined test of colour quantisation and dithering. Link it with
 * pngin.c and colquant.c.
 */

#include "colquant.h"

/* ----------------------------------------------------------------------
 * Functions to write out .BMP files. Unlike my usual functions of
 * this type, these ones are paletted.
 */

static void fput32(unsigned long val, FILE *fp);
static void fput16(unsigned val, FILE *fp);

struct Bitmap {
    FILE *fp;
    unsigned long padding;
};

static void fput32(unsigned long val, FILE *fp) {
    fputc((val >>  0) & 0xFF, fp);
    fputc((val >>  8) & 0xFF, fp);
    fputc((val >> 16) & 0xFF, fp);
    fputc((val >> 24) & 0xFF, fp);
}
static void fput16(unsigned val, FILE *fp) {
    fputc((val >>  0) & 0xFF, fp);
    fputc((val >>  8) & 0xFF, fp);
}

struct Bitmap *bmpinit(char const *filename, int width, int height,
		       int palettelen, unsigned long *palette) {
    int i;

    /*
     * File format is:
     *
     * 2char "BM"
     * 32bit total file size
     * 16bit zero (reserved)
     * 16bit zero (reserved)
     * 32bit 0x36+4*plen (offset from start of file to image data)
     * 32bit 0x28 (size of following BITMAPINFOHEADER)
     * 32bit width
     * 32bit height
     * 16bit 0x01 (planes=1)
     * 16bit 0x08 (bitcount=8)
     * 32bit zero (no compression)
     * 32bit size of image data (total file size minus 0x36)
     * 32bit 0xB6D (XPelsPerMeter)
     * 32bit 0xB6D (YPelsPerMeter)
     * 32bit plen (palette colours used)
     * 32bit plen (palette colours important)
     * 
     * the the palette (plen words of the form 0x00RRGGBB)
     *
     * then bitmap data, 1 byte per pixel, with padding zeros to
     * bring scan line to a multiple of 4 bytes. Padding zeros DO
     * happen after final scan line. Scan lines work from bottom
     * upwards.
     */
    struct Bitmap *bm;
    unsigned long scanlen = width;
    unsigned long padding = ((scanlen+3)&~3) - scanlen;
    unsigned long bitsize = (scanlen + padding) * height;
    FILE *fp = fopen(filename, "wb");
    fputs("BM", fp);
    fput32(0x36 + bitsize, fp); fput16(0, fp); fput16(0, fp);
    fput32(0x36 + 4*palettelen, fp);
    fput32(0x28, fp); fput32(width, fp); fput32(height, fp);
    fput16(1, fp); fput16(8, fp); fput32(0, fp); fput32(bitsize, fp);
    fput32(0xB6D, fp); fput32(0xB6D, fp); fput32(palettelen, fp);
    fput32(palettelen, fp);

    for (i = 0; i < palettelen; i++)
	fput32(palette[i], fp);

    bm = (struct Bitmap *)malloc(sizeof(struct Bitmap));
    bm->fp = fp;
    bm->padding = padding;
    return bm;
}

void bmppixel(struct Bitmap *bm, unsigned char pix) {
    putc(pix, bm->fp);
}

void bmpendrow(struct Bitmap *bm) {
    int j;
    for (j = 0; j < bm->padding; j++)
        putc(0, bm->fp);
}

void bmpclose(struct Bitmap *bm) {
    fclose(bm->fp);
    free(bm);
}

/* ----------------------------------------------------------------------
 * Main test program.
 */

int main(int argc, char **argv)
{
    int error;
    png_pixel palette[256];
    unsigned long upalette[256];
    png *p;
    int i, plen;

#ifdef FIXED_PALETTE
    for (i = 0; i < 6*6*6; i++) {
	int ir = i / (6*6);
	int ig = (i / 6) % 6;
	int ib = i % 6;

	palette[i].r = ir * (0xFFFF / 5);
	palette[i].g = ig * (0xFFFF / 5);
	palette[i].b = ib * (0xFFFF / 5);
    }
    plen = i;
#else
    /*
     * First pass: accumulate a single palette across all images.
     */
    {
	colquant *cq;
	cq = colquant_new(256, 8);
	for (i = 1; i < argc; i++) {
	    p = png_decode_file(argv[i], &error);
	    if (p) {
		colquant_data(cq, p->pixels, p->width * p->height);
		png_free(p);
	    } else {
		printf("%s: error: %s\n", argv[i], png_error_msg[error]);
	    }
	}
	plen = colquant_get_palette(cq, palette);
	colquant_free(cq);
    }
#endif

    /*
     * Translate the palette into the right shape for the bmp
     * functions.
     */
    assert(plen <= 256);
    for (i = 0; i < plen; i++) {
	upalette[i] = palette[i].r >> 8;
	upalette[i] <<= 8;
	upalette[i] |= palette[i].g >> 8;
	upalette[i] <<= 8;
	upalette[i] |= palette[i].b >> 8;
    }

    /*
     * Second pass: dither the images into the new palette, and
     * write them out as paletted BMPs.
     */
    for (i = 1; i < argc; i++) {
	p = png_decode_file(argv[i], &error);
	if (p) {
	    unsigned char *pdata = malloc(p->width * p->height);
	    struct Bitmap *bm;
	    char filename[FILENAME_MAX];
	    int x, y, suffix;

	    dither_image(p->width, p->height, p->pixels,
			 palette, plen, pdata);

	    strcpy(filename, argv[i]); /* yes yes, but it's just a test prog */
	    suffix = strlen(filename);
	    if (suffix > 4 && !strcmp(filename+suffix-4, ".png"))
		suffix -= 4;
	    sprintf(filename + suffix, ".bmp");
	    printf("%s -> %s\n", argv[i], filename);
	    bm = bmpinit(filename, p->width, p->height, plen, upalette);
	    for (y = p->height; y-- > 0 ;) {
		for (x = 0; x < p->width; x++)
		    bmppixel(bm, pdata[y*p->width+x]);
		bmpendrow(bm);
	    }
	    bmpclose(bm);
	    free(pdata);
	    png_free(p);
	}
    }

    return 0;
}

#endif
