/* ----------------------------------------------------------------------
 * Functions to write out .BMP files. Also supports PPM, because it
 * turns out to come in useful in some situations.
 */

#include <stdio.h>
#include <stdlib.h>

#include "bmpwrite.h"

static void fput32(unsigned long val, FILE *fp);
static void fput16(unsigned val, FILE *fp);

struct Bitmap {
    FILE *fp;
    int close;
    int type;
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

struct Bitmap *bmpinit(char const *filename, int width, int height, int type) {
    struct Bitmap *bm;

    bm = (struct Bitmap *)malloc(sizeof(struct Bitmap));
    bm->type = type;

    if (filename[0] == '-' && !filename[1]) {
	bm->fp = stdout;
	bm->close = 0;
    } else {
	bm->fp = fopen(filename, "wb");
	bm->close = 1;
    }

    if (type == BMP) {
	/*
	 * BMP File format is:
	 *
	 * 2char "BM"
	 * 32bit total file size
	 * 16bit zero (reserved)
	 * 16bit zero (reserved)
	 * 32bit 0x36 (offset from start of file to image data)
	 * 32bit 0x28 (size of following BITMAPINFOHEADER)
	 * 32bit width
	 * 32bit height
	 * 16bit 0x01 (planes=1)
	 * 16bit 0x18 (bitcount=24)
	 * 32bit zero (no compression)
	 * 32bit size of image data (total file size minus 0x36)
	 * 32bit 0xB6D (XPelsPerMeter)
	 * 32bit 0xB6D (YPelsPerMeter)
	 * 32bit zero (palette colours used)
	 * 32bit zero (palette colours important)
	 *
	 * then bitmap data, BGRBGRBGR... with padding zeros to bring
	 * scan line to a multiple of 4 bytes. Padding zeros DO happen
	 * after final scan line. Scan lines work from bottom upwards.
	 */
	unsigned long scanlen, bitsize;

	scanlen = 3 * width;
	bm->padding = ((scanlen+3)&~3) - scanlen;
	bitsize = (scanlen + bm->padding) * height;

	fputs("BM", bm->fp);
	fput32(0x36 + bitsize, bm->fp);
	fput16(0, bm->fp); fput16(0, bm->fp);
	fput32(0x36, bm->fp); fput32(0x28, bm->fp);
	fput32(width, bm->fp); fput32(height, bm->fp);
	fput16(1, bm->fp); fput16(24, bm->fp);
	fput32(0, bm->fp); fput32(bitsize, bm->fp);
	fput32(0xB6D, bm->fp); fput32(0xB6D, bm->fp);
	fput32(0, bm->fp); fput32(0, bm->fp);
    } else {
	/*
	 * PPM file format is:
	 *
	 * 2char "P6"
	 * arbitrary whitespace
	 * ASCII decimal width
	 * arbitrary whitespace
	 * ASCII decimal height
	 * arbitrary whitespace
	 * ASCII decimal maximum RGB value (here 255, for one-byte-per-sample)
	 * one whitespace character
	 *
	 * then simply bitmap data, RGBRGBRGB...
	 */
	fprintf(bm->fp, "P6 %d %d 255\n", width, height);

	bm->padding = 0;
    }

    return bm;
}

void bmppixel(struct Bitmap *bm,
	      unsigned char r, unsigned char g, unsigned char b) {
    if (bm->type == BMP) {
	putc(b, bm->fp);
	putc(g, bm->fp);
	putc(r, bm->fp);
    } else {
	putc(r, bm->fp);
	putc(g, bm->fp);
	putc(b, bm->fp);
    }
}

void bmpendrow(struct Bitmap *bm) {
    int j;
    for (j = 0; j < bm->padding; j++)
	putc(0, bm->fp);
}

void bmpclose(struct Bitmap *bm) {
    if (bm->close)
	fclose(bm->fp);
    free(bm);
}
