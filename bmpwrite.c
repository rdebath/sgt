/* ----------------------------------------------------------------------
 * Functions to write out .BMP files.
 */

#include <stdio.h>
#include <stdlib.h>

#include "bmpwrite.h"

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

struct Bitmap *bmpinit(char const *filename, int width, int height) {
    /*
     * File format is:
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
    struct Bitmap *bm;
    unsigned long scanlen = 3 * width;
    unsigned long padding = ((scanlen+3)&~3) - scanlen;
    unsigned long bitsize = (scanlen + padding) * height;
    FILE *fp = fopen(filename, "wb");
    fputs("BM", fp);
    fput32(0x36 + bitsize, fp); fput16(0, fp); fput16(0, fp);
    fput32(0x36, fp); fput32(0x28, fp); fput32(width, fp); fput32(height, fp);
    fput16(1, fp); fput16(24, fp); fput32(0, fp); fput32(bitsize, fp);
    fput32(0xB6D, fp); fput32(0xB6D, fp); fput32(0, fp); fput32(0, fp);

    bm = (struct Bitmap *)malloc(sizeof(struct Bitmap));
    bm->fp = fp;
    bm->padding = padding;
    return bm;
}

void bmppixel(struct Bitmap *bm,
	      unsigned char r, unsigned char g, unsigned char b) {
    putc(b, bm->fp);
    putc(g, bm->fp);
    putc(r, bm->fp);
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
