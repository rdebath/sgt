/*
 * PNG decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "png.h"

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define GET_32BIT_MSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0] << 24) | \
  ((unsigned long)(unsigned char)(cp)[1] << 16) | \
  ((unsigned long)(unsigned char)(cp)[2] << 8) | \
  ((unsigned long)(unsigned char)(cp)[3]))

#define PUT_32BIT_MSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)((value) >> 24), \
  (cp)[1] = (unsigned char)((value) >> 16), \
  (cp)[2] = (unsigned char)((value) >> 8), \
  (cp)[3] = (unsigned char)(value) )

#define GET_16BIT_MSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0] << 8) | \
  ((unsigned long)(unsigned char)(cp)[1]))

#define PUT_16BIT_MSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)((value) >> 8), \
  (cp)[1] = (unsigned char)(value) )

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

enum chunking_state {
    CS_PNGHDR,			       /* cstate_param goes from 0 to 7 */
    CS_LEN,			       /* cstate_param goes from 0 to 3 */
    CS_NAME,			       /* cstate_param goes from 0 to 3 */
    CS_DATA,			       /* cstate_param goes from 0 to clen-1 */
    CS_CRC			       /* cstate_param goes from 0 to 3 */
};

enum file_state {
    FS_START,			       /* need IHDR first */
    FS_POST_IHDR,
    FS_HAD_IDAT,
    FS_POST_IDAT,
    FS_POST_IEND
};

static const unsigned char pnghdr[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

struct interlace_pass {
    int xstart, xstep, ystart, ystep;
};

/*
 * With no interlacing, there is one pass over the image which
 * takes in all its pixels.
 */
static const struct interlace_pass null_interlace_passes[] = {
    {0, 1, 0, 1},
};

/*
 * Adam7 interlacing consists of seven passes which between them
 * specify every pixel exactly once.
 */
static const struct interlace_pass adam7_interlace_passes[] = {
    {0, 8, 0, 8},
    {4, 8, 0, 8},
    {0, 4, 4, 8},
    {2, 4, 0, 4},
    {0, 2, 2, 4},
    {1, 2, 0, 2},
    {0, 1, 1, 2},
};

/*
 * Multipliers that convert values from each bit depth into a
 * full-range 16-bit number. The index into this array is the bit
 * depth itself (1,2,4,8,16); all other entries are either zero or
 * absent.
 */
static const int bitdepth_mult[] = {
    0, 0xFFFF, 0x5555, 0, 0x1111, 0, 0, 0, 0x0101,
    0, 0, 0, 0, 0, 0, 0, 1
};

struct png_decoder {
    png *p;
    int image_ok;

    enum chunking_state cstate;
    unsigned cstate_param;
    unsigned long clen, crc, realcrc;
    unsigned char cname[4];
    unsigned char *cdata;

    enum file_state fstate;
    int bitdepth, coltype;
    int samples_per_pixel, bits_per_pixel;
    const struct interlace_pass *ipasses;
    int nipasses;

    png_pixel palette[256];
    int palettelen;
    int transvalues[3];

    int curr_scanline, scanline_bytes, nscanlines;
    int curr_scanline_pixels, curr_scanline_len, max_scanline_len;
    unsigned char *filtered_scanline;
    unsigned char *scanline;
    unsigned char *last_scanline;

    int seen_ihdr;

    deflate_decompress_ctx *dc;
};

/*
 * CRC32 checksum function.
 */

static unsigned long crc32_update(unsigned long crcword,
				  const unsigned char *data, int len)
{
    static const unsigned long crc32_table[256] = {
	0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL,
	0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
	0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L,
	0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
	0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
	0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
	0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL,
	0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
	0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L,
	0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
	0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L,
	0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
	0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L,
	0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
	0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
	0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
	0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL,
	0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
	0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L,
	0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
	0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL,
	0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
	0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL,
	0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
	0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
	0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
	0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L,
	0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
	0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L,
	0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
	0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L,
	0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
	0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL,
	0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
	0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
	0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
	0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL,
	0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
	0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL,
	0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
	0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L,
	0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
	0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L,
	0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
	0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
	0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
	0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L,
	0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
	0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL,
	0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
	0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L,
	0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
	0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL,
	0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
	0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
	0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
	0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L,
	0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
	0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L,
	0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
	0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L,
	0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
	0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L,
	0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
    };
    crcword ^= 0xFFFFFFFFL;
    while (len--) {
	unsigned long newbyte = *data++;
	newbyte ^= crcword & 0xFFL;
	crcword = (crcword >> 8) ^ crc32_table[newbyte];
    }
    return crcword ^ 0xFFFFFFFFL;
}

png_decoder *png_decoder_new(void)
{
    png_decoder *pd = (png_decoder *)malloc(sizeof(png_decoder));
    if (!pd)
	return pd;

    pd->p = NULL;
    pd->image_ok = FALSE;

    pd->cstate = CS_PNGHDR;
    pd->cstate_param = 0;
    pd->cdata = NULL;

    pd->fstate = FS_START;

    pd->filtered_scanline = pd->scanline = pd->last_scanline = NULL;
    pd->curr_scanline = pd->scanline_bytes = 0;
    pd->nscanlines = pd->max_scanline_len = 0;
    pd->curr_scanline_pixels = pd->curr_scanline_len = 0;

    pd->palettelen = 0;

    /* Out-of-range values to prevent accidental matching */
    pd->transvalues[0] = pd->transvalues[1] = pd->transvalues[2] = -1;

    pd->dc = NULL;

    return pd;
}

void png_decoder_free(png_decoder *pd)
{
    if (!pd)
	return;

    if (pd->dc)
	deflate_decompress_free(pd->dc);

    if (pd->p)
	png_free(pd->p);

    free(pd);
}

static int png_got_scanline(png_decoder *pd)
{
    int filter_type;
    unsigned char *in, *out, *last;
    int i, j, filtx, reconx, a, b, c, p, pa, pb, pc, pr;
    int xcoord, ycoord, leftoffset;
    unsigned long bits;
    int nbits;

    leftoffset = pd->bits_per_pixel / 8;
    if (leftoffset < 1)
	leftoffset = 1;

    /*
     * We reach here when there's a full filtered scanline in
     * pd->filtered_scanline. So now we apply the inverse filter
     * transformation and fill up pd->scanline with real data.
     */

    in = pd->filtered_scanline;
    out = pd->scanline;
    last = pd->last_scanline;
    /* Exchange scanline with last_scanline for the next one. */
    pd->scanline = last;
    pd->last_scanline = out;

    filter_type = *in++;

    if (filter_type >= 5)
	return PNG_ERR_ILLEGAL_FILTER_TYPE;

    for (i = 0; i < pd->curr_scanline_len; i++) {
	filtx = in[i];
	a = (i-leftoffset >= 0 ? out[i-leftoffset] : 0);
	b = last[i];
	c = (i-leftoffset >= 0 ? last[i-leftoffset] : 0);

	switch (filter_type) {
	  default /*case 0*/:	       /* no filtering */
	    reconx = filtx;
	    break;
	  case 1:		       /* sub */
	    reconx = filtx + a;
	    break;
	  case 2:		       /* up */
	    reconx = filtx + b;
	    break;
	  case 3:		       /* average */
	    reconx = filtx + ((a + b) >> 1);
	    break;
	  case 4:		       /* Paeth */
	    p = a + b - c;
	    pa = abs(p - a);
	    pb = abs(p - b);
	    pc = abs(p - c);
	    if (pa <= pb && pa <= pc)
		pr = a;
	    else if (pb <= pc)
		pr = b;
	    else
		pr = c;
	    reconx = filtx + pr;
	    break;
	}

	out[i] = reconx;
    }

    /*
     * Now we have a full unfiltered scanline. Convert the string
     * of bytes into pixel colour values, taking into account the
     * bit depth, the colour type, and the palette.
     */
    nbits = 0;
    bits = 0;
    in = out;
    ycoord = pd->curr_scanline * pd->ipasses->ystep + pd->ipasses->ystart;
    for (i = 0; i < pd->curr_scanline_pixels; i++) {
	int samples[4];
	png_pixel pix;

	/*
	 * Unpack bits and bytes into samples according to the bit
	 * depth.
	 */
	for (j = 0; j < pd->samples_per_pixel; j++) {
	    while (nbits < pd->bitdepth) {
		bits = (bits << 8) | *in++;
		nbits += 8;
	    }
	    samples[j] = bits >> (nbits - pd->bitdepth);
	    bits ^= samples[j] << (nbits - pd->bitdepth);
	    nbits -= pd->bitdepth;
	}

	/*
	 * Amalgamate samples into pixels according to the colour
	 * type.
	 */
	switch (pd->coltype) {
	  case 0:		       /* greyscale */
	    pix.r = pix.g = pix.b = bitdepth_mult[pd->bitdepth] * samples[0];
	    if (pix.r == pd->transvalues[0])
		pix.a = 0;
	    else
		pix.a = 0xFFFF;
	    break;
	  case 2:		       /* true colour */
	    pix.r = bitdepth_mult[pd->bitdepth] * samples[0];
	    pix.g = bitdepth_mult[pd->bitdepth] * samples[1];
	    pix.b = bitdepth_mult[pd->bitdepth] * samples[2];
	    if (pix.r == pd->transvalues[0] &&
		pix.g == pd->transvalues[1] &&
		pix.b == pd->transvalues[2])
		pix.a = 0;
	    else
		pix.a = 0xFFFF;
	    break;
	  case 3:		       /* paletted */
	    if (samples[0] >= pd->palettelen)
		return PNG_ERR_COLOUR_INDEX_OUTSIDE_PALETTE;
	    pix = pd->palette[samples[0]];
	    break;
	  case 4:		       /* greyscale + alpha */
	    pix.r = pix.g = pix.b = bitdepth_mult[pd->bitdepth] * samples[0];
	    pix.a = bitdepth_mult[pd->bitdepth] * samples[1];
	    break;
	  default /*case 6*/:	       /* true colour + alpha */
	    pix.r = bitdepth_mult[pd->bitdepth] * samples[0];
	    pix.g = bitdepth_mult[pd->bitdepth] * samples[1];
	    pix.b = bitdepth_mult[pd->bitdepth] * samples[2];
	    pix.a = bitdepth_mult[pd->bitdepth] * samples[3];
	    break;
	}

	/*
	 * And write the pixel into the proper place in the output
	 * image.
	 */
	xcoord = i * pd->ipasses->xstep + pd->ipasses->xstart;
	pd->p->pixels[ycoord * pd->p->width + xcoord] = pix;
    }

    return 0;
}

static int png_got_idat(png_decoder *pd, const void *vdata, int len)
{
    const unsigned char *data = (const unsigned char *)vdata;
    unsigned char c;

    while (len > 0) {
	/*
	 * Collect the image data into scanlines and pass them on
	 * to defiltering.
	 */
	if (pd->curr_scanline == 0 && pd->scanline_bytes == 0) {
	    /*
	     * We're starting a new pass. Compute the scanline length
	     * and the number of scanlines in the pass.
	     */
	    while (1) {
		if (pd->nipasses <= 0)
		    return PNG_ERR_TOO_MUCH_IMAGE_DATA;

		pd->nscanlines =
		    (pd->p->height - 1 + pd->ipasses->ystep -
		     pd->ipasses->ystart) / pd->ipasses->ystep;
		pd->curr_scanline_pixels =
		    (pd->p->width - 1 + pd->ipasses->xstep -
		     pd->ipasses->xstart) / pd->ipasses->xstep;
		pd->curr_scanline_len =
		    (pd->curr_scanline_pixels * pd->bits_per_pixel + 7) / 8;
		assert(pd->curr_scanline_len <= pd->max_scanline_len);

		if (pd->nscanlines == 0 || pd->curr_scanline_len == 0) {
		    /*
		     * This pass is empty. Skip it.
		     */
		    pd->nipasses--, pd->ipasses++;
		} else
		    break;
	    }

	    /*
	     * Zero the previous scanline.
	     */
	    memset(pd->last_scanline, 0, pd->curr_scanline_len);
	}

	len--;
	c = *data++;

	pd->filtered_scanline[pd->scanline_bytes++] = c;

	if (pd->scanline_bytes == pd->curr_scanline_len+1) {
	    int err = png_got_scanline(pd);

	    if (err)
		return err;

	    pd->scanline_bytes = 0;
	    if (++pd->curr_scanline == pd->nscanlines) {
		pd->nipasses--, pd->ipasses++;
		pd->curr_scanline = pd->scanline_bytes = 0;
	    }
	}
    }

    return 0;
}

static int png_gotchunk(png_decoder *pd)
{
    /*
     * If we've just seen an IDAT chunk and this isn't another one,
     * do end-of-IDAT processing.
     */
    if (pd->fstate == FS_HAD_IDAT && memcmp(pd->cname, "IDAT", 4)) {
	int deflate_error;
	void *outblock;
	int outlen;

	/*
	 * Check that the zlib stream was correctly terminated, and
	 * free it while we're here.
	 */
	deflate_error = deflate_decompress_data(pd->dc, NULL, 0,
						&outblock, &outlen);
	if (deflate_error)
	    return PNG_ERR_DEFLATE_ERROR_BASE + deflate_error;
	deflate_decompress_free(pd->dc);
	pd->dc = NULL;

	/*
	 * Also check that we've had exactly the right amount of
	 * image data. If we had too much, we'd already have seen
	 * an error from png_got_idat; so the simplest way to check
	 * we have just the right amount is to send in one more
	 * byte and hope png_got_idat _does_ throw an error.
	 */
	if (png_got_idat(pd, "\0", 1) == 0)
	    return PNG_ERR_NOT_ENOUGH_IMAGE_DATA;

	/*
	 * We're now in the post-IDAT state in which further IDAT
	 * chunks are illegal.
	 */
	pd->fstate = FS_POST_IDAT;
    }

    /*
     * Determine the chunk type.
     */
    if (!memcmp(pd->cname, "IHDR", 4)) {
	unsigned long w, h;

	/*
	 * Image header chunk. This tells us the size of the image,
	 * which means we can allocate the output data structure.
	 */

	/* IHDR must come right at the beginning. */
	if (pd->fstate != FS_START)
	    return PNG_ERR_IHDR_OUT_OF_ORDER;
	pd->fstate = FS_POST_IHDR;

	/* IHDR has a fixed length. */
	if (pd->clen != 13)
	    return PNG_ERR_IHDR_WRONG_LENGTH;

	/*
	 * The compression and filter methods must be fixed values,
	 * otherwise we don't know how to handle the file at all.
	 */
	if (pd->cdata[10] != 0)
	    return PNG_ERR_UNSUPPORTED_COMP_METHOD;
	if (pd->cdata[11] != 0)
	    return PNG_ERR_UNSUPPORTED_FILTER_METHOD;

	w = GET_32BIT_MSB_FIRST(pd->cdata + 0);
	h = GET_32BIT_MSB_FIRST(pd->cdata + 4);

	if (w == 0 || h == 0)
	    return PNG_ERR_IMAGE_DIMENSION_ZERO;

	if (w > ULONG_MAX / sizeof(png_pixel) / h)
	    return PNG_ERR_IMAGE_TOO_BIG_TO_HANDLE;

	pd->bitdepth = pd->cdata[8];
	pd->coltype = pd->cdata[9];

	if (pd->bitdepth != 1 &&
	    pd->bitdepth != 2 &&
	    pd->bitdepth != 4 &&
	    pd->bitdepth != 8 &&
	    pd->bitdepth != 16)
	    return PNG_ERR_UNSUPPORTED_BIT_DEPTH;

	if (pd->coltype != 0 &&
	    pd->coltype != 2 &&
	    pd->coltype != 3 &&
	    pd->coltype != 4 &&
	    pd->coltype != 6)
	    return PNG_ERR_UNSUPPORTED_COLOUR_TYPE;

	if (pd->cdata[12] == 0) {
	    pd->ipasses = null_interlace_passes;
	    pd->nipasses = lenof(null_interlace_passes);
	} else if (pd->cdata[12] == 1) {
	    pd->ipasses = adam7_interlace_passes;
	    pd->nipasses = lenof(adam7_interlace_passes);
	} else
	    return PNG_ERR_UNSUPPORTED_INTERLACE_TYPE;

	/*
	 * Bit depth and colour type are not independent.
	 */
	{
	    /*
	     * Since all the valid bit depths are powers of two,
	     * the simplest thing is to OR the valid ones together
	     * for each colour type.
	     */
	    static const int valid_depths[] = {
		1|2|4|8|16,	       /* greyscale: all depths allowed */
		0,		       /* colour type 1 is unused */
		8|16,		       /* true colour: 8 bits or more */
		1|2|4|8,	       /* paletted: up to 8 bits */
		8|16,		       /* greyscale+alpha: 8 bits or more */
		0,		       /* colour type 5 is unused */
		8|16,		       /* truecolour+alpha: 8 bits or more */
	    };

	    /*
	     * While we're here, let's also calculate the total
	     * amount of data stored per pixel.
	     */
	    static const int channels[] = { 1, 0, 3, 1, 2, 0, 4 };

	    if (!(valid_depths[pd->coltype] & pd->bitdepth))
		return PNG_ERR_BIT_DEPTH_INVALID_FOR_COLOUR_TYPE;

	    pd->samples_per_pixel = channels[pd->coltype];
	    pd->bits_per_pixel = pd->samples_per_pixel * pd->bitdepth;
	}

	pd->p = malloc(sizeof(png));
	if (!pd->p)
	    return PNG_ERR_OUT_OF_MEMORY;
	pd->p->width = w;
	pd->p->height = h;
	pd->p->pixels = malloc(w * h * sizeof(png_pixel));
	if (!pd->p->pixels)
	    return PNG_ERR_OUT_OF_MEMORY;
	pd->p->background.r = pd->p->background.g =
	    pd->p->background.b = pd->p->background.a = 0;
	pd->p->waspect = pd->p->haspect = 1;

	pd->max_scanline_len = (w * pd->bits_per_pixel + 7) / 8;
	pd->filtered_scanline = malloc(pd->max_scanline_len + 1);
	pd->scanline = malloc(pd->max_scanline_len);
	pd->last_scanline = malloc(pd->max_scanline_len);

	if (!pd->filtered_scanline ||
	    !pd->scanline ||
	    !pd->last_scanline)
	    return PNG_ERR_OUT_OF_MEMORY;

	pd->dc = deflate_decompress_new(DEFLATE_TYPE_ZLIB);
	if (!pd->dc)
	    return PNG_ERR_OUT_OF_MEMORY;

    } else if (!memcmp(pd->cname, "PLTE", 4)) {
	int i;

	/*
	 * Palette chunk. This can appear even in images which
	 * aren't actually paletted, in which case it's a suggested
	 * palette should the decoder need to quantise it. We don't
	 * need to worry about that, but we can process the chunk
	 * unconditionally if we see it.
	 */

	if (pd->fstate != FS_POST_IHDR)
	    return PNG_ERR_PLTE_OUT_OF_ORDER;

	if (pd->clen % 3 != 0)
	    return PNG_ERR_PLTE_LEN_INVALID;

	pd->palettelen = pd->clen / 3;
	if (pd->palettelen > (1 << pd->bitdepth))
	    return PNG_ERR_PLTE_LEN_INVALID;

	for (i = 0; i < pd->palettelen; i++) {
	    pd->palette[i].r = 0x0101 * pd->cdata[i*3+0];
	    pd->palette[i].g = 0x0101 * pd->cdata[i*3+1];
	    pd->palette[i].b = 0x0101 * pd->cdata[i*3+2];
	    pd->palette[i].a = 0xFFFF;
	}

    } else if (!memcmp(pd->cname, "tRNS", 4)) {
	int i;

	if (pd->fstate != FS_POST_IHDR)
	    return PNG_ERR_tRNS_OUT_OF_ORDER;

	/*
	 * Transparency chunk. This is treated totally differently
	 * depending on whether this is a paletted image or not.
	 */
	if (pd->coltype == 3) {
	    /*
	     * This chunk adds an alpha channel to each palette
	     * entry. It may terminate earlier than the actual
	     * palette, in which case we leave the trailing entries
	     * at alpha=255.
	     */
	    if (!pd->palettelen)
		return PNG_ERR_tRNS_OUT_OF_ORDER;
	    if (pd->clen == 0 || pd->clen > pd->palettelen)
		return PNG_ERR_tRNS_LEN_INVALID;
	    for (i = 0; i < pd->clen; i++)
		pd->palette[i].a = 0x0101 * pd->cdata[i];
	} else if (pd->coltype == 0 || pd->coltype == 2) {
	    /*
	     * This chunk specifies a single colour value
	     * (greyscale or RGB) which should be treated as
	     * transparent when encountered during the main image
	     * decode.
	     */
	    int nsamples = (pd->coltype == 0 ? 1 : 3);
	    
	    if (pd->clen != 2 * nsamples)
		return PNG_ERR_tRNS_LEN_INVALID;
	    for (i = 0; i < nsamples; i++) {
		pd->transvalues[i] = GET_16BIT_MSB_FIRST(pd->cdata + i*2);
		pd->transvalues[i] &= (1 << pd->bitdepth)-1;
		pd->transvalues[i] *= bitdepth_mult[pd->bitdepth];
	    }
	} else
	    return PNG_ERR_tRNS_NOT_PERMITTED;
    } else if (!memcmp(pd->cname, "bKGD", 4)) {
	/*
	 * Background chunk. This defines the image's preferred
	 * background colour, which we simply shovel into the
	 * output structure for the user to deal with.
	 */
	int nsamples, i;
	int samples[3];

	if (pd->fstate != FS_POST_IHDR)
	    return PNG_ERR_bKGD_OUT_OF_ORDER;

	switch (pd->coltype) {
	  case 0: case 4:	       /* greyscale with or without alpha */
	    if (pd->clen != 2)
		return PNG_ERR_bKGD_LEN_INVALID;
	    nsamples = 1;
	    break;
	  case 2: case 6:	       /* true colour with or without alpha */
	    if (pd->clen != 6)
		return PNG_ERR_bKGD_LEN_INVALID;
	    nsamples = 3;
	    break;
	  case 3:		       /* paletted */
	    if (!pd->palettelen)
		return PNG_ERR_bKGD_OUT_OF_ORDER;
	    if (pd->clen != 1)
		return PNG_ERR_bKGD_LEN_INVALID;
	    nsamples = 0;
	    break;
	}

	if (nsamples) {
	    for (i = 0; i < nsamples; i++) {
		samples[i] = GET_16BIT_MSB_FIRST(pd->cdata + i*2);
		samples[i] &= (1 << pd->bitdepth)-1;
		samples[i] *= bitdepth_mult[pd->bitdepth];
	    }
	    pd->p->background.r = samples[0];
	    pd->p->background.g = samples[1 % nsamples];
	    pd->p->background.b = samples[2 % nsamples];
	    pd->p->background.a = 0xFFFF;   /* indicate background colour */
	} else {
	    int index = pd->cdata[0];
	    if (index > pd->palettelen)
		return PNG_ERR_COLOUR_INDEX_OUTSIDE_PALETTE;
	    pd->p->background = pd->palette[index];
	}
    } else if (!memcmp(pd->cname, "pHYs", 4)) {
	unsigned long ha, va;

	/*
	 * Physical dimensions chunk. We extract the aspect ratio
	 * from this and put it into the output image.
	 * 
	 * We don't check the units byte, because I can't currently
	 * imagine a client of this code needing it. It'd be easy
	 * enough to add, though.
	 */

	if (pd->fstate != FS_POST_IHDR)
	    return PNG_ERR_pHYs_OUT_OF_ORDER;
	if (pd->clen != 9)
	    return PNG_ERR_pHYs_LEN_INVALID;

	/*
	 * Make sure both measurements are non-zero. The spec
	 * doesn't actually say they _must_ be non-zero, but I
	 * think it would be silly to make them zero. If one is
	 * zero, we don't fault the PNG (I can't justify that
	 * without the spec's approval), but we ignore the pHYs
	 * chunk.
	 */
	va = GET_32BIT_MSB_FIRST(pd->cdata);
	ha = GET_32BIT_MSB_FIRST(pd->cdata+4);

	if (va != 0 && ha != 0) {
	    pd->p->waspect = ha;
	    pd->p->haspect = va;
	}
	
    } else if (!memcmp(pd->cname, "IDAT", 4)) {
	int deflate_error;
	void *outblock;
	int outlen;
	int err;

	/*
	 * If we're a paletted image and beginning the IDAT stream,
	 * check that we've seen a palette.
	 */
	if (pd->fstate == FS_POST_IHDR &&
	    pd->coltype == 3 &&
	    !pd->palettelen)
	    return PNG_ERR_MANDATORY_PLTE_MISSING;

	/*
	 * Image data chunk. Feed it through deflate decompression,
	 * and hand the resulting bytes (if any) off to the image
	 * data stream processor.
	 */
	if (pd->fstate != FS_POST_IHDR && pd->fstate != FS_HAD_IDAT)
	    return PNG_ERR_IDAT_OUT_OF_ORDER;
	pd->fstate = FS_HAD_IDAT;

	deflate_error = deflate_decompress_data(pd->dc, pd->cdata, pd->clen,
						&outblock, &outlen);
	if (deflate_error)
	    return PNG_ERR_DEFLATE_ERROR_BASE + deflate_error;

	if (outlen)
	    err = png_got_idat(pd, outblock, outlen);
	else
	    err = 0;
	if (outblock)
	    free(outblock);

	if (err)
	    return err;

    } else if (!memcmp(pd->cname, "IEND", 4)) {
	/*
	 * Image trailer chunk.
	 */

	if (pd->fstate != FS_POST_IDAT)
	    return PNG_ERR_IEND_OUT_OF_ORDER;
	if (pd->clen != 0)
	    return PNG_ERR_IEND_CONTAINS_DATA;
	pd->fstate = FS_POST_IEND;
	pd->image_ok = TRUE;
    } else if (!(pd->cname[0] & 0x20)) {
	/*
	 * Critical chunk we don't recognise. Encountering one of
	 * these is a fatal decoding error.
	 */
	if (pd->fstate == FS_START)
	    return PNG_ERR_FIRST_CHUNK_NOT_IHDR;

	return PNG_ERR_UNSUPPORTED_CRITICAL_CHUNK;
    } else {
	/*
	 * Some other chunk we can safely ignore - except that we
	 * care about the ordering constraints.
	 */
	if (pd->fstate == FS_START)
	    return PNG_ERR_FIRST_CHUNK_NOT_IHDR;
    }

    return 0;
}

int png_decoder_data(png_decoder *pd, const void *vdata, int len)
{
    const unsigned char *data = (const unsigned char *)vdata;

    if (len == 0 && pd->fstate != FS_POST_IEND)
	return PNG_ERR_TRUNCATED;

    while (len > 0) {
	unsigned char c;

	len--;
	c = *data++;

	switch (pd->cstate) {
	  case CS_PNGHDR:
	    if (c != pnghdr[pd->cstate_param])
		return PNG_ERR_HEADER_INCORRECT;
	    if (++pd->cstate_param == 8) {
		pd->cstate_param = 0;
		pd->cstate = CS_LEN;
		pd->clen = 0;
	    }
	    break;
	  case CS_LEN:
	    if (pd->fstate == FS_POST_IEND)
		return PNG_ERR_DATA_AFTER_IEND;
	    pd->clen = (pd->clen << 8) | c;
	    if (++pd->cstate_param == 4) {
		pd->cstate_param = 0;
		pd->cstate = CS_NAME;
	    }
	    break;
	  case CS_NAME:
	    pd->cname[pd->cstate_param] = c;
	    if (++pd->cstate_param == 4) {
		if (pd->clen > 0) {
		    pd->cstate_param = 0;
		    pd->cstate = CS_DATA;
		    pd->cdata = malloc(pd->clen);
		} else {
		    pd->cstate_param = 0;
		    pd->cstate = CS_CRC;
		    pd->cdata = malloc(1);
		}
		if (!pd->cdata)
		    return PNG_ERR_OUT_OF_MEMORY;
		pd->realcrc = crc32_update(0, pd->cname, 4);
                pd->crc = 0;
	    }
	    break;
	  case CS_DATA:
	    pd->cdata[pd->cstate_param] = c;
	    if (++pd->cstate_param == pd->clen) {
		pd->cstate_param = 0;
		pd->cstate = CS_CRC;
		pd->realcrc = crc32_update(pd->realcrc, pd->cdata, pd->clen);
	    }
	    break;
	  case CS_CRC:
	    pd->crc = (pd->crc << 8) | c;
	    if (++pd->cstate_param == 4) {
		int chunkerr;

		if (pd->crc != pd->realcrc)
		    return PNG_ERR_CHUNK_CRC_MISMATCH;

		chunkerr = png_gotchunk(pd);

		free(pd->cdata);
		pd->cdata = NULL;

		if (chunkerr)
		    return chunkerr;

		pd->cstate_param = 0;
		pd->cstate = CS_LEN;
		pd->clen = 0;
	    }
	}
    }

    return 0;
}

png *png_decoder_image(png_decoder *pd)
{
    png *ret;

    if (pd->image_ok) {
	ret = pd->p;
	pd->p = NULL;
    } else
	ret = NULL;

    png_decoder_free(pd);
    return ret;
}

png *png_decode_file(const char *filename, int *error)
{
    FILE *fp;
    png_decoder *pd;

    if (error) *error = 0;

    fp = fopen(filename, "rb");
    if (!fp) {
	if (error) *error = PNG_ERR_OPENING_FILE;
	return NULL;
    }

    pd = png_decoder_new();
    if (!pd) {
	if (error) *error = PNG_ERR_OUT_OF_MEMORY;
	fclose(fp);
	return NULL;
    }

    while (1) {
	char buf[4096];
	int fret, pret;

	fret = fread(buf, 1, sizeof(buf), fp);
	if (fret < 0) {
	    if (error) *error = PNG_ERR_READING_FILE;
	    png_decoder_free(pd);
	    fclose(fp);
	    return NULL;
	} else if (fret >= 0) {
	    pret = png_decoder_data(pd, buf, fret);
	    if (pret != 0) {
		if (error) *error = pret;
		png_decoder_free(pd);
		fclose(fp);
		return NULL;
	    }

	    if (fret == 0)
		break;
	}
    }

    fclose(fp);

    return png_decoder_image(pd);
}

void png_free(png *p)
{
    if (!p)
	return;

    free(p->pixels);
    free(p);
}

#define PNG_ERROR_MSG(x,y) y
#define DEFLATE_ERROR_MSG(x,y) "decompression error in image data: " y
const char *const png_error_msg[PNG_NUM_ERRORS] = {
    PNG_ERRORLIST(PNG_ERROR_MSG),
    DEFLATE_ERRORLIST(DEFLATE_ERROR_MSG)
};
#define PNG_ERROR_SYM(x,y) #x
#define DEFLATE_ERROR_SYM(x,y) "PNG_ERR_DEFLATE_ERROR_BASE+"#x
const char *const png_error_sym[PNG_NUM_ERRORS] = {
    PNG_ERRORLIST(PNG_ERROR_SYM),
    DEFLATE_ERRORLIST(DEFLATE_ERROR_SYM)
};

#ifdef PNGIN_TESTMODE

#ifdef BMPOUT
/* For a test program which converts PNG to BMP, link against bmpwrite.c
 * from my `pix' source directory. */
#include "bmpwrite.h"
#endif

int main(int argc, char **argv)
{
    int error;
    png *p;
    int i;

    for (i = 1; i < argc; i++) {
	p = png_decode_file(argv[i], &error);
	if (p) {
	    int x, y;

#ifdef BMPOUT
	    struct Bitmap *bm;
	    char filename[FILENAME_MAX];
	    int suffix;

	    strcpy(filename, argv[i]); /* yes yes, but it's just a test prog */
	    suffix = strlen(filename);
	    if (suffix > 4 && !strcmp(filename+suffix-4, ".png"))
		suffix -= 4;
	    sprintf(filename + suffix, ".bmp");
	    printf("%s -> %s\n", argv[i], filename);
	    bm = bmpinit(filename, p->width, p->height);
	    for (y = p->height; y-- > 0 ;) {
		for (x = 0; x < p->width; x++) {
		    png_pixel pix = p->pixels[y*p->width+x];
		    if (p->background.a) {
			pix.r = ((pix.r * pix.a) +
				 (p->background.r * (0xFFFF - pix.a)))/0xFFFF;
			pix.g = ((pix.g * pix.a) +
				 (p->background.g * (0xFFFF - pix.a)))/0xFFFF;
			pix.b = ((pix.b * pix.a) +
				 (p->background.b * (0xFFFF - pix.a)))/0xFFFF;
		    }
		    bmppixel(bm, pix.r >> 8, pix.g >> 8, pix.b >> 8);
		}
		bmpendrow(bm);
	    }
	    bmpclose(bm);
#else
	    printf("%s: bkgd r%04X/g%04X/b%04X/a%04X, ratio %lu:%lu\n\n",
		   argv[i], p->background.r, p->background.g,
		   p->background.b, p->background.a, p->waspect, p->haspect);

	    for (y = 0; y < p->height; y++) {
		for (x = 0; x < p->width; x++)
		    printf("[r%04x] ", p->pixels[y*p->width+x].r);
		printf("\n");
		for (x = 0; x < p->width; x++)
		    printf("[g%04X] ", p->pixels[y*p->width+x].g);
		printf("\n");
		for (x = 0; x < p->width; x++)
		    printf("[b%04X] ", p->pixels[y*p->width+x].b);
		printf("\n");
		for (x = 0; x < p->width; x++)
		    printf("[a%04X] ", p->pixels[y*p->width+x].a);
		printf("\n\n");
	    }
#endif
	    png_free(p);
	} else {
	    printf("%s: error: %s\n", argv[i], png_error_msg[error]);
	}
    }

    return 0;
}

#endif
