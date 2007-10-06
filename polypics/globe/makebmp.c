#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------------
 * Output routines for 1-bit Windows BMP. (It's a nice simple
 * format which can easily be converted into other formats; please
 * don't shoot me.)
 */

struct Bitmap {
    FILE *fp;
    unsigned long padbits;
    unsigned long curr32, currbits;
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

static void bmpinit(struct Bitmap *bm, char const *filename,
                    int width, int height) {
    /*
     * File format is:
     *
     * 2char "BM"
     * 32bit total file size
     * 16bit zero (reserved)
     * 16bit zero (reserved)
     * 32bit 0x3E (offset from start of file to image data)
     * 32bit 0x28 (size of following BITMAPINFOHEADER)
     * 32bit width
     * 32bit height
     * 16bit 0x01 (planes=1)
     * 16bit 0x01 (bitcount=1)
     * 32bit zero (no compression)
     * 32bit size of image data (total file size minus 0x3E)
     * 32bit 0xB6D (XPelsPerMeter)
     * 32bit 0xB6D (YPelsPerMeter)
     * 32bit zero (palette colours used)
     * 32bit zero (palette colours important)
     * 4byte FF,FF,FF,00 (white background)
     * 4byte 00,00,00,00 (black foreground)
     *
     * then raw bitmap data at 8 bits per byte, working from left
     * to right (MSbit first in each byte) along each scan line.
     * Scan lines are padded to a four-byte boundary, and work from
     * bottom upwards.
     */
    unsigned long scanlen = 4 * ((width+31) / 32);
    unsigned long padbits = 8*scanlen - width;
    unsigned long bitsize = scanlen * height;
    FILE *fp = fopen(filename, "wb");
    fputs("BM", fp);
    fput32(0x3E + bitsize, fp); fput16(0, fp); fput16(0, fp);
    fput32(0x3E, fp); fput32(0x28, fp); fput32(width, fp); fput32(height, fp);
    fput16(1, fp); fput16(1, fp); fput32(0, fp); fput32(bitsize, fp);
    fput32(0xB6D, fp); fput32(0xB6D, fp); fput32(0, fp); fput32(0, fp);
    fput32(0xFFFFFF, fp); fput32(0x000000, fp);

    bm->fp = fp;
    bm->padbits = padbits;
    bm->curr32 = bm->currbits = 0;
}

static void bmppixel(struct Bitmap *bm, int p) {
    if (p)
	bm->curr32 |= 1 << (7 - bm->currbits);
    bm->currbits++;
    if (bm->currbits == 8) {
	fputc(bm->curr32, bm->fp);
	bm->curr32 = bm->currbits = 0;
    }
}

static void bmpendrow(struct Bitmap *bm) {
    int j;
    for (j = 0; j < bm->padbits; j++)
        bmppixel(bm, 0);
}

static void bmpclose(struct Bitmap *bm) {
    fclose(bm->fp);
}

/* ----------------------------------------------------------------------
 * Main function to read a bunch of map files in and faff with them.
 */

void do_map(struct Bitmap *bm, int threshold, int scale, int w, int h,
	    char **filenames)
{
    FILE *fps[9];
    int nfps;
    char *fname;
    int totalw;
    int row[43200];
    unsigned char buf[20480];
    int i, j, k;

    nfps = totalw = 0;
    while (totalw < 43200) {
	char buf[4096];
	totalw += w;
	fname = *filenames++;
	sprintf(buf, "zcat %s.DEM.gz", fname);
	fps[nfps] = popen(buf, "r");
	assert(fps[nfps] != NULL);
	nfps++;
    }

    assert(totalw == 43200);
    assert(nfps <= 9);

    memset(row, 0, sizeof(row));

    for (i = 0; i < h; i++) {
	for (j = 0; j < nfps; j++) {
	    int ret;
	
	    ret = fread(buf, 1, w*2, fps[j]);
	    assert(ret == w*2);

	    for (k = 0; k < w; k++) {
		short x = (buf[k*2] << 8) | buf[k*2+1];
		if (x >= threshold)
		    row[w*j+k]++;
	    }
	}

	if ((i+1) % scale == 0) {
	    for (j = 0; j < 43200; j += scale) {
		int total = 0;
		for (k = 0; k < scale; k++)
		    total += row[j+k];
		/*
		 * total is now in the range [0,scale^2). I'm going
		 * to arbitrarily set my threshold at the square
		 * root of the cell size, i.e. scale.
		 */
		bmppixel(bm, total >= scale);
	    }
	    bmpendrow(bm);
	    memset(row, 0, sizeof(row));
	}
    }

    for (j = 0; j < nfps; j++)
	pclose(fps[j]);
}

char *row1[] = {
    "W180N90", "W140N90", "W100N90", "W060N90", "W020N90",
    "E020N90", "E060N90", "E100N90", "E140N90",
    "W180N90", "W140N90", "W100N90", "W060N90", "W020N90",
    "E020N90", "E060N90", "E100N90", "E140N90"
};
char *row2[] = {
    "W180N40", "W140N40", "W100N40", "W060N40", "W020N40",
    "E020N40", "E060N40", "E100N40", "E140N40",
    "W180N40", "W140N40", "W100N40", "W060N40", "W020N40",
    "E020N40", "E060N40", "E100N40", "E140N40"
};
char *row3[] = {
    "W180S10", "W140S10", "W100S10", "W060S10", "W020S10",
    "E020S10", "E060S10", "E100S10", "E140S10",
    "W180S10", "W140S10", "W100S10", "W060S10", "W020S10",
    "E020S10", "E060S10", "E100S10", "E140S10"
};
char *rowA[] = {
    "W180S60", "W120S60", "W060S60", "W000S60", "E060S60", "E120S60",
    "W180S60", "W120S60", "W060S60", "W000S60", "E060S60", "E120S60"
};

int main(int argc, char **argv)
{
    int threshold, scale, rotate;
    struct Bitmap bm;

    threshold = scale = -1;
    rotate = 0;

    while (--argc > 0) {
	char *p = *++argv;

	if (*p == '-' && p[1] && !isdigit(p[1])) {
	    char c = *++p;
	    p++;

	    switch (c) {
	      case 'r':
		rotate = atoi(p);
		break;
	      default:
		fprintf(stderr, "unknown option '-%c'\n", c);
		break;
	    }
	} else {
	    if (threshold == -1)
		threshold = atoi(p);
	    else if (scale == -1)
		scale = atoi(p);
	}
    }

    if (threshold == -1 || scale == -1) {
	fprintf(stderr, "usage: makebmp <threshold> <scale>\n");
	return 1;
    }

    /*
     * Map files are 4800x6000 and 7200x3600 (for main and
     * Antarctic tiles respectively). Scale factor must divide all
     * of these, which is equivalent to saying it must divide their
     * greatest common divisor, which is 1200.
     */
    if (1200 % scale != 0) {
	fprintf(stderr, "scale factor must be a divisor of 1200\n");
	return 1;
    }

    bmpinit(&bm, "output.bmp", 43200 / scale, 21600 / scale);
    do_map(&bm, threshold, scale, 4800, 6000, row1 + 3*rotate);
    do_map(&bm, threshold, scale, 4800, 6000, row2 + 3*rotate);
    do_map(&bm, threshold, scale, 4800, 6000, row3 + 3*rotate);
    do_map(&bm, threshold, scale, 7200, 3600, rowA + 2*rotate);
    bmpclose(&bm);
}
