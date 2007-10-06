#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

/* ----------------------------------------------------------------------
 * Output routines for 24-bit Windows BMP. (It's a nice simple
 * format which can easily be converted into other formats; please
 * don't shoot me.)
 */

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

static void bmpinit(struct Bitmap *bm, char const *filename,
                    int width, int height) {
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
    unsigned long scanlen = 3 * width;
    unsigned long padding = ((scanlen+3)&~3) - scanlen;
    unsigned long bitsize = (scanlen + padding) * height;
    FILE *fp = fopen(filename, "wb");
    fputs("BM", fp);
    fput32(0x36 + bitsize, fp); fput16(0, fp); fput16(0, fp);
    fput32(0x36, fp); fput32(0x28, fp); fput32(width, fp); fput32(height, fp);
    fput16(1, fp); fput16(24, fp); fput32(0, fp); fput32(bitsize, fp);
    fput32(0xB6D, fp); fput32(0xB6D, fp); fput32(0, fp); fput32(0, fp);

    bm->fp = fp;
    bm->padding = padding;
}

static void bmppixel(struct Bitmap *bm,
                     unsigned char r, unsigned char g, unsigned char b) {
    putc(b, bm->fp);
    putc(g, bm->fp);
    putc(r, bm->fp);
}

static void bmpendrow(struct Bitmap *bm) {
    int j;
    for (j = 0; j < bm->padding; j++)
        putc(0, bm->fp);
}

static void bmpclose(struct Bitmap *bm) {
    fclose(bm->fp);
}

/* ----------------------------------------------------------------------
 * Prepare latitude/longitude points for each pixel in cube bitmaps.
 */

#define PI 3.141592653589793238462643383279502884197169399

struct listitem {
    int gtopo_x, gtopo_y;
    short *result;
};

struct list {
    struct listitem *data;
    int ndata, datasize, ptr;
};

void init_list(struct list *list)
{
    list->ndata = list->datasize = list->ptr = 0;
    list->data = NULL;
}

void add_list(struct list *list, short *data, double y, double x)
{
    if (list->ndata >= list->datasize) {
	list->datasize = list->ndata + 16384;
	list->data = realloc(list->data, sizeof(*list->data) * list->datasize);
    }
    list->data[list->ndata].result = data;
    list->data[list->ndata].gtopo_x = (int)x % 43200;
    list->data[list->ndata].gtopo_y = y;

    /*
     * Antarctic Slice Hack.
     * 
     * The GTOPO30 data has a bug: the leftmost line of pixels in
     * the W180S60 tile appears to be entirely made of zeroes,
     * causing a very thin linear cut from the South Pole out to
     * the edge of Antarctica. To work around this bug, I change
     * gtopo_x from 0 to 1 if gtopo_y is at least 20900.
     */
    if (list->data[list->ndata].gtopo_y >= 20900 &&
	list->data[list->ndata].gtopo_x == 0)
	list->data[list->ndata].gtopo_x = 1;

    list->ndata++;
}

int list_compar(const void *av, const void *bv)
{
    const struct listitem *a = (const struct listitem *)av;
    const struct listitem *b = (const struct listitem *)bv;

    if (a->gtopo_y > b->gtopo_y)
	return +1;
    if (a->gtopo_y < b->gtopo_y)
	return -1;
    if (a->gtopo_x > b->gtopo_x)
	return +1;
    if (a->gtopo_x < b->gtopo_x)
	return -1;
    return 0;
}

void sort_list(struct list *list)
{
    qsort(list->data, list->ndata, sizeof(struct listitem), list_compar);
}

void cubeface(int x1, int y1, int z1,
	      int x2, int y2, int z2,
	      int xo, int yo, int zo,
	      int vertex, int scale, short *bmdata, struct list *list)
{
    int x, y;

    for (y = 0; y < scale; y++) {
	double ym = (y * 2 + 1) / (double)scale - 1.0;
	for (x = 0; x < scale; x++) {
	    double xm = (x * 2 + 1) / (double)scale - 1.0;
	    double X, Y, Z, L, lat, lon;

	    X = xm * x1 + ym * x2 + xo;
	    Y = xm * y1 + ym * y2 + yo;
	    Z = xm * z1 + ym * z2 + zo;
#if 0
printf("%f,%f,%f -> ", X, Y, Z);
#endif

	    L = sqrt(X*X + Y*Y + Z*Z);
	    X /= L;
	    Y /= L;
	    Z /= L;

	    /*
	     * Optionally apply transformation to centre the poles
	     * on vertices rather than faces.
	     */
	    if (vertex) {
		double matrix[9];
		double Xp, Yp, Zp;

		matrix[0] = +1/sqrt(6);
		matrix[1] = -1/sqrt(6);
		matrix[2] = +2/sqrt(6);
		matrix[3] = +1/sqrt(2);
		matrix[4] = +1/sqrt(2);
		matrix[5] =  0;
		matrix[6] = -1/sqrt(3);
		matrix[7] = +1/sqrt(3);
		matrix[8] = +1/sqrt(3);

		Xp = matrix[0] * X + matrix[1] * Y + matrix[2] * Z;
		Yp = matrix[3] * X + matrix[4] * Y + matrix[5] * Z;
		Zp = matrix[6] * X + matrix[7] * Y + matrix[8] * Z;

		X = Xp; Y = Yp; Z = Zp;
	    }

	    /*
	     * Equator is on X-Y plane, with Greenwich meridian in
	     * positive X direction; north pole is at Z=+1 and
	     * south at Z=-1. Thus, latitude in radians north is
	     * asin(Z), while longitude is atan2(Y,X).
	     */

	    lat = asin(Z);
	    lon = atan2(Y,X);

	    /*
	     * Convert to gtopo coordinates.
	     */
	    lat = 10800 - (21600 / PI) * lat;
	    lon = 21600 + (21600 / PI) * lon;
#if 0
printf("%f,%f\n", lat, lon);
#endif

	    /*
	     * And add to the list.
	     */
	    add_list(list, bmdata + scale * y + x, lat, lon);
	}
    }
}

/* ----------------------------------------------------------------------
 * Output the bitmaps.
 */
void bmpout(char *fname, int scale, short *data)
{
    struct Bitmap bm;
    int x, y;

    bmpinit(&bm, fname, scale, scale);

    for (y = scale; y-- > 0 ;) {
	for (x = 0; x < scale; x++) {
	    short val = data[scale*y+x];
	    int r, g, b;

	    if (val == -9999) {
		/* Special case: sea. */
		r = g = 0;
		b = 255;
	    } else {
#ifdef ORIG_SCALE
		/*
		 * Our scale runs from -407 to +8752 metres. I'm
		 * going to situate pure green at sea level, pure
		 * yellow at the top of Everest, and pure cyan at
		 * -407.
		 */
		g = 255;
		r = b = 0;
		if (val < 0) {
		    b = (-val) * 255 / 407;
		} else {
		    r = val * 255 / 8752;
		}
#else
		/*
		 * These colours are roughly simulated from my
		 * Philips world atlas.
		 */
		static const struct {
		    int minht, maxht, repht;
		    int r, g, b;
		} colours[] = {
		    {-500, -500, -500, 0x00, 0xa0, 0x00},
		    {-500, 0, -250, 0x00, 0xc0, 0x00},
		    {0, 200, 100, 0x00, 0xff, 0x00},
		    {200, 400, 300, 0xc0, 0xff, 0x00},
		    {400, 1000, 700, 0xff, 0xff, 0x00},
		    {1000, 1500, 1250, 0xff, 0xc0, 0x00},
		    {1500, 2000, 1750, 0xc0, 0x80, 0x00},
		    {2000, 3000, 2500, 0xa0, 0x70, 0x00},
		    {3000, 4000, 3500, 0x50, 0x70, 0xb0},
		    {4000, 6000, 5000, 0x78, 0xa8, 0xd8},
		    {6000, 9000, 9000, 0xa0, 0xe0, 0xff},
		};
		int i;
		int r0, r1, g0, g1, b0, b1, v0, v1;

		v0 = v1 = -9999;

		for (i = 0; i < lenof(colours); i++) {
		    if (colours[i].repht <= val) {
			r0 = colours[i].r;
			g0 = colours[i].g;
			b0 = colours[i].b;
			v0 = colours[i].repht;
		    }
		    if (val <= colours[i].repht && v1 == -9999) {
			r1 = colours[i].r;
			g1 = colours[i].g;
			b1 = colours[i].b;
			v1 = colours[i].repht;
		    }
		}

		r = r0 + (r1 - r0) * (val - v0) / (v1==v0 ? 1 : v1-v0);
		g = g0 + (g1 - g0) * (val - v0) / (v1==v0 ? 1 : v1-v0);
		b = b0 + (b1 - b0) * (val - v0) / (v1==v0 ? 1 : v1-v0);
#endif
	    }

	    bmppixel(&bm, r, g, b);
	}
	bmpendrow(&bm);
    }

    bmpclose(&bm);
}

/* ----------------------------------------------------------------------
 * Main function to read a bunch of map files in and faff with them.
 */

void do_map(int y, int w, int h, char **filenames, struct list *list)
{
    FILE *fps[9];
    int nfps;
    char *fname;
    int totalw;
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

    for (i = 0; i < h; i++) {
	for (j = 0; j < nfps; j++) {
	    int ret;
	
	    ret = fread(buf, 1, w*2, fps[j]);
	    assert(ret == w*2);

	    for (k = 0; k < w; k++) {
		int x = w*j+k;
		short val = (buf[k*2] << 8) | buf[k*2+1];
#if 0
if (x == 0 || x == 1) printf("%d: %6d\n", x, val);
#endif
		while (list->data[list->ptr].gtopo_y == i+y &&
		       list->data[list->ptr].gtopo_x == x) {
		    *list->data[list->ptr].result = val;
		    list->ptr++;
		}
	    }
	}
    }

    for (j = 0; j < nfps; j++)
	pclose(fps[j]);
}

char *row1[] = {
    "W180N90", "W140N90", "W100N90", "W060N90", "W020N90",
    "E020N90", "E060N90", "E100N90", "E140N90",
};
char *row2[] = {
    "W180N40", "W140N40", "W100N40", "W060N40", "W020N40",
    "E020N40", "E060N40", "E100N40", "E140N40",
};
char *row3[] = {
    "W180S10", "W140S10", "W100S10", "W060S10", "W020S10",
    "E020S10", "E060S10", "E100S10", "E140S10",
};
char *rowA[] = {
    "W180S60", "W120S60", "W060S60", "W000S60", "E060S60", "E120S60",
};

int main(int argc, char **argv)
{
    int scale;
    int vertex;
    struct list list;
    short *bmdata;

    scale = -1;
    vertex = 0;

    while (--argc > 0) {
	char *p = *++argv;

	if (*p == '-') {
	    char c = *++p;
	    p++;

	    switch (c) {
	      case 'v':
		vertex = 1;
		break;
	      default:
		fprintf(stderr, "unknown option '-%c'\n", c);
		break;
	    }
	} else {
	    if (scale == -1)
		scale = atoi(p);
	}
    }

    if (scale == -1) {
	fprintf(stderr, "usage: makecube [-v] <scale>\n");
	return 1;
    }

    init_list(&list);
    bmdata = malloc(6 * scale*scale * sizeof(short));

    /*
     * Set up the cube faces.
     */
    cubeface(1,0,0, 0,1,0, 0,0,1, vertex, scale, bmdata + 0*scale*scale, &list);
    cubeface(1,0,0, 0,1,0, 0,0,-1, vertex, scale, bmdata + 1*scale*scale, &list);
    cubeface(1,0,0, 0,0,1, 0,1,0, vertex, scale, bmdata + 2*scale*scale, &list);
    cubeface(1,0,0, 0,0,1, 0,-1,0, vertex, scale, bmdata + 3*scale*scale, &list);
    cubeface(0,1,0, 0,0,1, 1,0,0, vertex, scale, bmdata + 4*scale*scale, &list);
    cubeface(0,1,0, 0,0,1, -1,0,0, vertex, scale, bmdata + 5*scale*scale, &list);

    /*
     * Sort the list of points.
     */
    sort_list(&list);

#if 0
    {
	int i;
	for (i = 0; i < list.ndata; i++)
	    printf("%d,%d -> %p\n", list.data[i].gtopo_x,
		   list.data[i].gtopo_y, list.data[i].result);
    }
#endif

    do_map(0, 4800, 6000, row1, &list);
    do_map(6000, 4800, 6000, row2, &list);
    do_map(12000, 4800, 6000, row3, &list);
    do_map(18000, 7200, 3600, rowA, &list);

    assert(list.ptr == list.ndata);

    bmpout("cube1.bmp", scale, bmdata + 0*scale*scale);
    bmpout("cube2.bmp", scale, bmdata + 1*scale*scale);
    bmpout("cube3.bmp", scale, bmdata + 2*scale*scale);
    bmpout("cube4.bmp", scale, bmdata + 3*scale*scale);
    bmpout("cube5.bmp", scale, bmdata + 4*scale*scale);
    bmpout("cube6.bmp", scale, bmdata + 5*scale*scale);
}
