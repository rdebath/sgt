/*
 * Read a Deflate-encoded file, and translate it into a variant form
 * of Deflate in which bits are read from the MSB to LSB of each
 * input byte rather than the other way round. This makes no
 * difference to the compression ratio, but admits more convenient
 * arithmetic algorithms in decoding.
 *
 * On input, the program can cope with the bare Deflate format
 * (RFC1951), the minimal Zlib wrapper encoding (RFC1950) or the
 * gzip file format (RFC1952). On output, it always produces (the
 * analogue of) bare Deflate.
 */

/*
 * This software is copyright 2000-2006,2008 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define snew(type) ( (type *) malloc(sizeof(type)) )
#define snewn(n, type) ( (type *) malloc((n) * sizeof(type)) )
#define sresize(x, n, type) ( (type *) realloc((x), (n) * sizeof(type)) )
#define sfree(x) ( free((x)) )

#define lenof(x) (sizeof((x)) / sizeof(*(x)))

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

#if defined TESTDBG
/* gcc-specific diagnostic macro */
#define debug_int(x...) ( fprintf(stderr, x) )
#define debug(x) ( debug_int x )
#else
#define debug(x)
#endif

enum {
    DEFLATE_TYPE_BARE,
    DEFLATE_TYPE_ZLIB,
    DEFLATE_TYPE_GZIP
};

typedef struct deflate_decompress_ctx deflate_decompress_ctx;

/*
 * Enumeration of error codes. The strange macro is so that I can
 * define description arrays in the accompanying source.
 */
#define DEFLATE_ERRORLIST(A) \
    A(DEFLATE_NO_ERR, "success"), \
    A(DEFLATE_ERR_ZLIB_HEADER, "invalid zlib header"), \
    A(DEFLATE_ERR_ZLIB_WRONGCOMP, "zlib header specifies non-deflate compression"), \
    A(DEFLATE_ERR_GZIP_HEADER, "invalid gzip header"), \
    A(DEFLATE_ERR_GZIP_WRONGCOMP, "gzip header specifies non-deflate compression"), \
    A(DEFLATE_ERR_GZIP_FHCRC, "gzip header specifies disputed FHCRC flag"), \
    A(DEFLATE_ERR_SMALL_HUFTABLE, "under-committed Huffman code space"), \
    A(DEFLATE_ERR_LARGE_HUFTABLE, "over-committed Huffman code space"), \
    A(DEFLATE_ERR_CHECKSUM, "incorrect data checksum"), \
    A(DEFLATE_ERR_INLEN, "incorrect data length"), \
    A(DEFLATE_ERR_UNEXPECTED_EOF, "unexpected end of data")
#define DEFLATE_ENUM_DEF(x,y) x
enum { DEFLATE_ERRORLIST(DEFLATE_ENUM_DEF), DEFLATE_NUM_ERRORS };
#undef DEFLATE_ENUM_DEF

/* ----------------------------------------------------------------------
 * Deflate functionality common to both compression and decompression.
 */

static const unsigned char lenlenmap[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

#define MAXCODELEN 16

/*
 * Given a sequence of Huffman code lengths, compute the actual
 * codes, in the final form suitable for feeding to outbits (i.e.
 * already bit-mirrored).
 *
 * Returns the maximum code length found. Can also return -1 to
 * indicate the table was overcommitted (too many or too short
 * codes to exactly cover the possible space), or -2 to indicate it
 * was undercommitted (too few or too long codes).
 */
static int hufcodes(const unsigned char *lengths, int *codes, int nsyms)
{
    int count[MAXCODELEN], startcode[MAXCODELEN];
    int code, maxlen;
    int i, j;

    /* Count the codes of each length. */
    maxlen = 0;
    for (i = 1; i < MAXCODELEN; i++)
	count[i] = 0;
    for (i = 0; i < nsyms; i++) {
	count[lengths[i]]++;
	if (maxlen < lengths[i])
	    maxlen = lengths[i];
    }
    /* Determine the starting code for each length block. */
    code = 0;
    for (i = 1; i < MAXCODELEN; i++) {
	startcode[i] = code;
	code += count[i];
	if (code > (1 << i))
	    maxlen = -1;	       /* overcommitted */
	code <<= 1;
    }
    if (code < (1 << MAXCODELEN))
	maxlen = -2;		       /* undercommitted */
    /* Determine the code for each symbol. Mirrored, of course. */
    for (i = 0; i < nsyms; i++) {
	code = startcode[lengths[i]]++;
	codes[i] = 0;
	for (j = 0; j < lengths[i]; j++) {
	    codes[i] = (codes[i] << 1) | (code & 1);
	    code >>= 1;
	}
    }

    return maxlen;
}

/*
 * Adler32 checksum function.
 */
static unsigned long adler32_update(unsigned long s,
				    const unsigned char *data, int len)
{
    unsigned s1 = s & 0xFFFF, s2 = (s >> 16) & 0xFFFF;
    int i;

    for (i = 0; i < len; i++) {
	s1 += data[i];
	s2 += s1;
	if (!(i & 0xFFF)) {
	    s1 %= 65521;
	    s2 %= 65521;
	}
    }

    return ((s2 % 65521) << 16) | (s1 % 65521);
}

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

typedef struct {
    short code, extrabits;
    int min, max;
} coderecord;

static const coderecord lencodes[] = {
    {257, 0, 3, 3},
    {258, 0, 4, 4},
    {259, 0, 5, 5},
    {260, 0, 6, 6},
    {261, 0, 7, 7},
    {262, 0, 8, 8},
    {263, 0, 9, 9},
    {264, 0, 10, 10},
    {265, 1, 11, 12},
    {266, 1, 13, 14},
    {267, 1, 15, 16},
    {268, 1, 17, 18},
    {269, 2, 19, 22},
    {270, 2, 23, 26},
    {271, 2, 27, 30},
    {272, 2, 31, 34},
    {273, 3, 35, 42},
    {274, 3, 43, 50},
    {275, 3, 51, 58},
    {276, 3, 59, 66},
    {277, 4, 67, 82},
    {278, 4, 83, 98},
    {279, 4, 99, 114},
    {280, 4, 115, 130},
    {281, 5, 131, 162},
    {282, 5, 163, 194},
    {283, 5, 195, 226},
    {284, 5, 227, 257},
    {285, 0, 258, 258},
};

static const coderecord distcodes[] = {
    {0, 0, 1, 1},
    {1, 0, 2, 2},
    {2, 0, 3, 3},
    {3, 0, 4, 4},
    {4, 1, 5, 6},
    {5, 1, 7, 8},
    {6, 2, 9, 12},
    {7, 2, 13, 16},
    {8, 3, 17, 24},
    {9, 3, 25, 32},
    {10, 4, 33, 48},
    {11, 4, 49, 64},
    {12, 5, 65, 96},
    {13, 5, 97, 128},
    {14, 6, 129, 192},
    {15, 6, 193, 256},
    {16, 7, 257, 384},
    {17, 7, 385, 512},
    {18, 8, 513, 768},
    {19, 8, 769, 1024},
    {20, 9, 1025, 1536},
    {21, 9, 1537, 2048},
    {22, 10, 2049, 3072},
    {23, 10, 3073, 4096},
    {24, 11, 4097, 6144},
    {25, 11, 6145, 8192},
    {26, 12, 8193, 12288},
    {27, 12, 12289, 16384},
    {28, 13, 16385, 24576},
    {29, 13, 24577, 32768},
};

/*
 * The way we work the Huffman decode is to have a table lookup on
 * the first N bits of the input stream (in the order they arrive,
 * of course, i.e. the first bit of the Huffman code is in bit 0).
 * Each table entry lists the number of bits to consume, plus
 * either an output code or a pointer to a secondary table.
 */
struct table;
struct tableentry;

struct tableentry {
    unsigned char nbits;
    short code;
    struct table *nexttable;
};

struct table {
    int mask;			       /* mask applied to input bit stream */
    struct tableentry *table;
};

#define MAXSYMS 288

#define DWINSIZE 32768

/*
 * Build a single-level decode table for elements
 * [minlength,maxlength) of the provided code/length tables, and
 * recurse to build subtables.
 */
static struct table *mkonetab(int *codes, unsigned char *lengths, int nsyms,
			      int pfx, int pfxbits, int bits)
{
    struct table *tab = snew(struct table);
    int pfxmask = (1 << pfxbits) - 1;
    int nbits, i, j, code;

    tab->table = snewn(1 << bits, struct tableentry);
    tab->mask = (1 << bits) - 1;

    for (code = 0; code <= tab->mask; code++) {
	tab->table[code].code = -1;
	tab->table[code].nbits = 0;
	tab->table[code].nexttable = NULL;
    }

    for (i = 0; i < nsyms; i++) {
	if (lengths[i] <= pfxbits || (codes[i] & pfxmask) != pfx)
	    continue;
	code = (codes[i] >> pfxbits) & tab->mask;
	for (j = code; j <= tab->mask; j += 1 << (lengths[i] - pfxbits)) {
	    tab->table[j].code = i;
	    nbits = lengths[i] - pfxbits;
	    if (tab->table[j].nbits < nbits)
		tab->table[j].nbits = nbits;
	}
    }
    for (code = 0; code <= tab->mask; code++) {
	if (tab->table[code].nbits <= bits)
	    continue;
	/* Generate a subtable. */
	tab->table[code].code = -1;
	nbits = tab->table[code].nbits - bits;
	if (nbits > 7)
	    nbits = 7;
	tab->table[code].nbits = bits;
	tab->table[code].nexttable = mkonetab(codes, lengths, nsyms,
					      pfx | (code << pfxbits),
					      pfxbits + bits, nbits);
    }

    return tab;
}

/*
 * Build a decode table, given a set of Huffman tree lengths.
 */
static struct table *mktable(unsigned char *lengths, int nlengths,
#ifdef ANALYSIS
			     const char *alphabet,
#endif
			     int *error)
{
    int codes[MAXSYMS];
    int maxlen;

#ifdef ANALYSIS
    if (alphabet && analyse_level > 1) {
	int i, col = 0;
	printf("code lengths for %s alphabet:\n", alphabet);
	for (i = 0; i < nlengths; i++) {
	    col += printf("%3d", lengths[i]);
	    if (col > 72) {
		putchar('\n');
		col = 0;
	    }
	}
	if (col > 0)
	    putchar('\n');
    }
#endif

    maxlen = hufcodes(lengths, codes, nlengths);

    if (maxlen < 0) {
	*error = (maxlen == -1 ? DEFLATE_ERR_LARGE_HUFTABLE :
		  DEFLATE_ERR_SMALL_HUFTABLE);
	return NULL;
    }

    /*
     * Now we have the complete list of Huffman codes. Build a
     * table.
     */
    return mkonetab(codes, lengths, nlengths, 0, 0, maxlen < 9 ? maxlen : 9);
}

static int freetable(struct table **ztab)
{
    struct table *tab;
    int code;

    if (ztab == NULL)
	return -1;

    if (*ztab == NULL)
	return 0;

    tab = *ztab;

    for (code = 0; code <= tab->mask; code++)
	if (tab->table[code].nexttable != NULL)
	    freetable(&tab->table[code].nexttable);

    sfree(tab->table);
    tab->table = NULL;

    sfree(tab);
    *ztab = NULL;

    return (0);
}

struct deflate_decompress_ctx {
    struct table *staticlentable, *staticdisttable;
    struct table *currlentable, *currdisttable, *lenlentable;
    enum {
	ZLIBSTART,
	GZIPSTART, GZIPMETHFLAGS, GZIPIGNORE1, GZIPIGNORE2, GZIPIGNORE3,
	GZIPEXTRA, GZIPFNAME, GZIPCOMMENT,
	OUTSIDEBLK, TREES_HDR, TREES_LENLEN, TREES_LEN, TREES_LENREP,
	INBLK, GOTLENSYM, GOTLEN, GOTDISTSYM,
	UNCOMP_LEN, UNCOMP_NLEN, UNCOMP_DATA,
	END,
	ADLER1, ADLER2,
	CRC1, CRC2, ILEN1, ILEN2,
	FINALSPIN
    } state;
    int sym, hlit, hdist, hclen, lenptr, lenextrabits, lenaddon, len,
	lenrep, lastblock;
    int uncomplen;
    unsigned char lenlen[19];
    unsigned char lengths[286 + 32];
    unsigned long bits;
    int nbits;
    unsigned char window[DWINSIZE];
    int winpos;
    unsigned char *outblk;
    int outlen, outsize;
    unsigned long outbits;
    int noutbits;
    int type;
    unsigned long checksum;
    unsigned long bytesout;
    int gzflags, gzextralen;
    int outfixed;
#ifdef ANALYSIS
    int bytesread;
    int bitcount_before;
#define BITCOUNT(dctx) ( (dctx)->bytesread * 8 - (dctx)->nbits )
#endif
};

static void outbits(deflate_decompress_ctx *out, unsigned long bits, int nbits)
{
    bits &= (1UL << nbits) - 1;
    assert(out->noutbits + nbits <= 32);
    out->outbits |= bits << (32 - out->noutbits - nbits);
    out->noutbits += nbits;
    while (out->noutbits >= 8) {
	if (out->outlen >= out->outsize) {
	    out->outsize = out->outlen + 64;
	    out->outblk = sresize(out->outblk, out->outsize, unsigned char);
	}
	out->outblk[out->outlen++] = (unsigned char) (out->outbits >> 24);
	out->outbits <<= 8;
	out->noutbits -= 8;
    }
#ifdef STATISTICS
    out->bitcount += nbits;
#endif
}

deflate_decompress_ctx *deflate_decompress_new(int type, int outfixed)
{
    deflate_decompress_ctx *dctx = snew(deflate_decompress_ctx);
    unsigned char lengths[288];

    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 256 - 144);
    memset(lengths + 256, 7, 280 - 256);
    memset(lengths + 280, 8, 288 - 280);
    dctx->staticlentable = mktable(lengths, 288,
#ifdef ANALYSIS
				   NULL,
#endif
				   NULL);
    assert(dctx->staticlentable);
    memset(lengths, 5, 32);
    dctx->staticdisttable = mktable(lengths, 32,
#ifdef ANALYSIS
				    NULL,
#endif
				    NULL);
    assert(dctx->staticdisttable);
    dctx->state = (type == DEFLATE_TYPE_ZLIB ? ZLIBSTART :
		   type == DEFLATE_TYPE_GZIP ? GZIPSTART :
		   OUTSIDEBLK);
    dctx->currlentable = dctx->currdisttable = dctx->lenlentable = NULL;
    dctx->bits = 0;
    dctx->nbits = 0;
    dctx->winpos = 0;
    dctx->type = type;
    dctx->lastblock = FALSE;
    dctx->checksum = (type == DEFLATE_TYPE_ZLIB ? 1 : 0);
    dctx->bytesout = 0;
    dctx->gzflags = dctx->gzextralen = 0;
    dctx->outbits = 0;
    dctx->noutbits = 0;
    dctx->outfixed = outfixed;
#ifdef ANALYSIS
    dctx->bytesread = dctx->bitcount_before = 0;
#endif

    return dctx;
}

void deflate_decompress_free(deflate_decompress_ctx *dctx)
{
    if (dctx->currlentable && dctx->currlentable != dctx->staticlentable)
	freetable(&dctx->currlentable);
    if (dctx->currdisttable && dctx->currdisttable != dctx->staticdisttable)
	freetable(&dctx->currdisttable);
    if (dctx->lenlentable)
	freetable(&dctx->lenlentable);
    freetable(&dctx->staticlentable);
    freetable(&dctx->staticdisttable);
    sfree(dctx);
}

static int huflookup(deflate_decompress_ctx *dctx, struct table *tab)
{
    unsigned long bits = dctx->bits;
    int nbits = dctx->nbits;
    while (1) {
	struct tableentry *ent;
	ent = &tab->table[bits & tab->mask];
	if (ent->nbits > nbits)
	    return -1;		       /* not enough data */
	bits >>= ent->nbits;
	nbits -= ent->nbits;
	if (ent->code == -1)
	    tab = ent->nexttable;
	else {
	    if (!dctx->outfixed) {
		int i;
		for (i = 0; i < dctx->nbits - nbits; i++)
		    outbits(dctx, (dctx->bits >> i), 1);
	    }
	    dctx->bits = bits;
	    dctx->nbits = nbits;
	    return ent->code;
	}

	/*
	 * If we reach here with `tab' null, it can only be because
	 * there was a missing entry in the Huffman table. This
	 * should never occur even with invalid input data, because
	 * we enforce at mktable time that the Huffman codes should
	 * precisely cover the code space; so we can enforce this
	 * by assertion.
	 */
	assert(tab);
    }
}

static void emit_char(deflate_decompress_ctx *dctx, int c)
{
    dctx->window[dctx->winpos] = c;
    dctx->winpos = (dctx->winpos + 1) & (DWINSIZE - 1);
    if (dctx->type == DEFLATE_TYPE_ZLIB) {
	unsigned char uc = c;
	dctx->checksum = adler32_update(dctx->checksum, &uc, 1);
    } else if (dctx->type == DEFLATE_TYPE_GZIP) {
	unsigned char uc = c;
	dctx->checksum = crc32_update(dctx->checksum, &uc, 1);
    }
    dctx->bytesout++;
}

static void fixed_literal(deflate_decompress_ctx *dctx, unsigned char c)
{
    if (c <= 143) {
	/* 0 through 143 are 8 bits long starting at 00110000. */
	outbits(dctx, 0x30 + c, 8);
    } else {
	/* 144 through 255 are 9 bits long starting at 110010000. */
	outbits(dctx, 0x190 - 144 + c, 9);
    }
}

static void fixed_match(deflate_decompress_ctx *dctx, int distance, int len)
{
    const coderecord *d, *l;
    int i, j, k;

    /*
     * Binary-search to find which length code we're transmitting.
     */
    i = -1;
    j = sizeof(lencodes) / sizeof(*lencodes);
    while (1) {
	assert(j - i >= 2);
	k = (j + i) / 2;
	if (len < lencodes[k].min)
	    j = k;
	else if (len > lencodes[k].max)
	    i = k;
	else {
	    l = &lencodes[k];
	    break;		       /* found it! */
	}
    }

    /*
     * Transmit the length code. 256-279 are seven bits starting at
     * 0000000; 280-287 are eight bits starting at 11000000.
     */
    if (l->code <= 279) {
	outbits(dctx, l->code - 256, 7);
    } else {
	outbits(dctx, 0xc0 - 280 + l->code, 8);
    }

    /*
     * Transmit the extra bits.
     */
    if (l->extrabits)
	outbits(dctx, len - l->min, l->extrabits);

    /*
     * Binary-search to find which distance code we're transmitting.
     */
    i = -1;
    j = sizeof(distcodes) / sizeof(*distcodes);
    while (1) {
	assert(j - i >= 2);
	k = (j + i) / 2;
	if (distance < distcodes[k].min)
	    j = k;
	else if (distance > distcodes[k].max)
	    i = k;
	else {
	    d = &distcodes[k];
	    break;		       /* found it! */
	}
    }

    /*
     * Transmit the distance code. Five bits starting at 00000.
     */
    outbits(dctx, d->code, 5);

    /*
     * Transmit the extra bits.
     */
    if (d->extrabits)
	outbits(dctx, distance - d->min, d->extrabits);
}

#define EATBITS(n) ( dctx->nbits -= (n), dctx->bits >>= (n) )

int deflate_decompress_data(deflate_decompress_ctx *dctx,
			    const void *vblock, int len,
			    void **outblock, int *outlen)
{
    const coderecord *rec;
    const unsigned char *block = (const unsigned char *)vblock;
    int code, bfinal, btype, rep, dist, nlen, header, cksum;
    int error = 0;

    dctx->outblk = NULL;
    dctx->outsize = 0;
    dctx->outlen = 0;

    if (len == 0) {
	if (dctx->state != FINALSPIN) {
	    error = DEFLATE_ERR_UNEXPECTED_EOF;
	    goto finished;
	} else {
	    if (dctx->outfixed) {
		outbits(dctx, 0, 7); /* end-of-block code, meaning EOF */
	    }
	    outbits(dctx, 0, 8 - (dctx->nbits & 7));   /* flush last byte */
	    goto finished;
	}
    }

    while (len > 0 || dctx->nbits > 0) {
	while (dctx->nbits < 24 && len > 0) {
	    dctx->bits |= (*block++) << dctx->nbits;
	    dctx->nbits += 8;
	    len--;
#ifdef ANALYSIS
	    dctx->bytesread++;
#endif
	}
	switch (dctx->state) {
	  case ZLIBSTART:
	    /* Expect 16-bit zlib header. */
	    if (dctx->nbits < 16)
		goto finished;	       /* done all we can */

            /*
             * The header is stored as a big-endian 16-bit integer,
             * in contrast to the general little-endian policy in
             * the rest of the format :-(
             */
            header = (((dctx->bits & 0xFF00) >> 8) |
                      ((dctx->bits & 0x00FF) << 8));
            EATBITS(16);

            /*
             * Check the header:
             *
             *  - bits 8-11 should be 1000 (Deflate/RFC1951)
             *  - bits 12-15 should be at most 0111 (window size)
             *  - bit 5 should be zero (no dictionary present)
             *  - we don't care about bits 6-7 (compression rate)
             *  - bits 0-4 should be set up to make the whole thing
             *    a multiple of 31 (checksum).
             */
	    if ((header & 0xF000) >  0x7000 ||
                (header & 0x0020) != 0x0000 ||
                (header % 31) != 0) {
		error = DEFLATE_ERR_ZLIB_HEADER;
                goto finished;
	    }
            if ((header & 0x0F00) != 0x0800) {
		error = DEFLATE_ERR_ZLIB_WRONGCOMP;
                goto finished;
	    }
	    dctx->state = OUTSIDEBLK;
	    break;
	  case GZIPSTART:
	    /* Expect 16-bit gzip header. */
	    if (dctx->nbits < 16)
		goto finished;
	    header = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (header != 0x8B1F) {
		error = DEFLATE_ERR_GZIP_HEADER;
		goto finished;
	    }
	    dctx->state = GZIPMETHFLAGS;
	    break;
	  case GZIPMETHFLAGS:
	    /* Expect gzip compression method and flags bytes. */
	    if (dctx->nbits < 16)
		goto finished;
	    header = dctx->bits & 0xFF;
	    EATBITS(8);
	    if (header != 8) {
		error = DEFLATE_ERR_GZIP_WRONGCOMP;
		goto finished;
	    }
	    dctx->gzflags = dctx->bits & 0xFF;
	    if (dctx->gzflags & 2) {
		/*
		 * The FHCRC flag is slightly confusing. RFC1952
		 * documents it as indicating the presence of a
		 * two-byte CRC16 of the gzip header, occurring
		 * just before the beginning of the Deflate stream.
		 * However, gzip itself (as of 1.3.5) appears to
		 * believe it indicates that the current gzip
		 * `member' is not the final one, i.e. that the
		 * stream is composed of multiple gzip members
		 * concatenated together, and furthermore gzip will
		 * refuse to decode any file that has it set.
		 * 
		 * For this reason, I label it as `disputed' and
		 * also refuse to decode anything that has it set.
		 * I don't expect this to be a problem in practice.
		 */
		error = DEFLATE_ERR_GZIP_FHCRC;
		goto finished;
	    }
	    EATBITS(8);
	    dctx->state = GZIPIGNORE1;
	    break;
	  case GZIPIGNORE1:
	  case GZIPIGNORE2:
	  case GZIPIGNORE3:
	    /* Expect two bytes of gzip timestamp/XFL/OS, which we ignore. */
	    if (dctx->nbits < 16)
		goto finished;
	    EATBITS(16);
	    if (dctx->state == GZIPIGNORE3) {
		dctx->state = GZIPEXTRA;
	    } else
		dctx->state++;	       /* maps IGNORE1 -> IGNORE2 -> IGNORE3 */
	    break;
	  case GZIPEXTRA:
	    if (dctx->gzflags & 4) {
		/* Expect two bytes of extra-length count, then that many
		 * extra bytes of header data, all of which we ignore. */
		if (!dctx->gzextralen) {
		    if (dctx->nbits < 16)
			goto finished;
		    dctx->gzextralen = dctx->bits & 0xFFFF;
		    EATBITS(16);
		    break;
		} else if (dctx->gzextralen > 0) {
		    if (dctx->nbits < 8)
			goto finished;
		    EATBITS(8);
		    if (--dctx->gzextralen > 0)
			break;
		}
	    }
	    dctx->state = GZIPFNAME;
	    break;
	  case GZIPFNAME:
	    if (dctx->gzflags & 8) {
		/*
		 * Expect a NUL-terminated filename.
		 */
		if (dctx->nbits < 8)
		    goto finished;
		code = dctx->bits & 0xFF;
		EATBITS(8);
	    } else
		code = 0;
	    if (code == 0)
		dctx->state = GZIPCOMMENT;
	    break;
	  case GZIPCOMMENT:
	    if (dctx->gzflags & 16) {
		/*
		 * Expect a NUL-terminated filename.
		 */
		if (dctx->nbits < 8)
		    goto finished;
		code = dctx->bits & 0xFF;
		EATBITS(8);
	    } else
		code = 0;
	    if (code == 0)
		dctx->state = OUTSIDEBLK;
	    break;
	  case OUTSIDEBLK:
	    /* Expect 3-bit block header. */
	    if (dctx->nbits < 3)
		goto finished;	       /* done all we can */
	    bfinal = dctx->bits & 1;
	    if (bfinal)
		dctx->lastblock = TRUE;
	    if (!dctx->outfixed)
		outbits(dctx, bfinal, 1);
	    EATBITS(1);
	    btype = dctx->bits & 3;
	    if (!dctx->outfixed)
		outbits(dctx, btype, 2);
	    EATBITS(2);
	    if (btype == 0) {
		int to_eat = dctx->nbits & 7;
		dctx->state = UNCOMP_LEN;
		if (!dctx->outfixed)
		    outbits(dctx, 0, to_eat);
		EATBITS(to_eat);       /* align to byte boundary */
	    } else if (btype == 1) {
		dctx->currlentable = dctx->staticlentable;
		dctx->currdisttable = dctx->staticdisttable;
		dctx->state = INBLK;
	    } else if (btype == 2) {
		dctx->state = TREES_HDR;
	    }
	    debug(("recv: bfinal=%d btype=%d\n", bfinal, btype));
#ifdef ANALYSIS
	    if (analyse_level > 1) {
		static const char *const btypes[] = {
		    "uncompressed", "static", "dynamic", "type 3 (unknown)"
		};
		printf("new block, %sfinal, %s\n",
		       bfinal ? "" : "not ",
		       btypes[btype]);
	    }
#endif
	    break;
	  case TREES_HDR:
	    /*
	     * Dynamic block header. Five bits of HLIT, five of
	     * HDIST, four of HCLEN.
	     */
	    if (dctx->nbits < 5 + 5 + 4)
		goto finished;	       /* done all we can */
	    dctx->hlit = 257 + (dctx->bits & 31);
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 5);
	    EATBITS(5);
	    dctx->hdist = 1 + (dctx->bits & 31);
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 5);
	    EATBITS(5);
	    dctx->hclen = 4 + (dctx->bits & 15);
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 4);
	    EATBITS(4);
	    debug(("recv: hlit=%d hdist=%d hclen=%d\n", dctx->hlit,
		   dctx->hdist, dctx->hclen));
#ifdef ANALYSIS
	    if (analyse_level > 1)
		printf("hlit=%d, hdist=%d, hclen=%d\n",
		        dctx->hlit, dctx->hdist, dctx->hclen);
#endif
	    dctx->lenptr = 0;
	    dctx->state = TREES_LENLEN;
	    memset(dctx->lenlen, 0, sizeof(dctx->lenlen));
	    break;
	  case TREES_LENLEN:
	    if (dctx->nbits < 3)
		goto finished;
	    while (dctx->lenptr < dctx->hclen && dctx->nbits >= 3) {
		dctx->lenlen[lenlenmap[dctx->lenptr++]] =
		    (unsigned char) (dctx->bits & 7);
		debug(("recv: lenlen %d\n", (unsigned char) (dctx->bits & 7)));
		if (!dctx->outfixed)
		    outbits(dctx, dctx->bits, 3);
		EATBITS(3);
	    }
	    if (dctx->lenptr == dctx->hclen) {
		dctx->lenlentable = mktable(dctx->lenlen, 19,
#ifdef ANALYSIS
					    "code length",
#endif
					    &error);
		if (!dctx->lenlentable)
		    goto finished;     /* error code set up by mktable */
		dctx->state = TREES_LEN;
		dctx->lenptr = 0;
	    }
	    break;
	  case TREES_LEN:
	    if (dctx->lenptr >= dctx->hlit + dctx->hdist) {
		dctx->currlentable = mktable(dctx->lengths, dctx->hlit,
#ifdef ANALYSIS
					     "literal/length",
#endif
					     &error);
		if (!dctx->currlentable)
		    goto finished;     /* error code set up by mktable */
		dctx->currdisttable = mktable(dctx->lengths + dctx->hlit,
					      dctx->hdist,
#ifdef ANALYSIS
					      "distance",
#endif
					      &error);
		if (!dctx->currdisttable)
		    goto finished;     /* error code set up by mktable */
		freetable(&dctx->lenlentable);
		dctx->lenlentable = NULL;
		dctx->state = INBLK;
		break;
	    }
	    code = huflookup(dctx, dctx->lenlentable);
	    debug(("recv: codelen %d\n", code));
	    if (code == -1)
		goto finished;
	    if (code < 16) {
#ifdef ANALYSIS
		if (analyse_level > 1)
		    printf("code-length %d\n", code);
#endif
		dctx->lengths[dctx->lenptr++] = code;
	    } else {
		dctx->lenextrabits = (code == 16 ? 2 : code == 17 ? 3 : 7);
		dctx->lenaddon = (code == 18 ? 11 : 3);
		dctx->lenrep = (code == 16 && dctx->lenptr > 0 ?
				dctx->lengths[dctx->lenptr - 1] : 0);
		dctx->state = TREES_LENREP;
	    }
	    break;
	  case TREES_LENREP:
	    if (dctx->nbits < dctx->lenextrabits)
		goto finished;
	    rep =
		dctx->lenaddon +
		(dctx->bits & ((1 << dctx->lenextrabits) - 1));
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, dctx->lenextrabits);
	    EATBITS(dctx->lenextrabits);
	    if (dctx->lenextrabits)
		debug(("recv: codelen-extrabits %d/%d\n", rep - dctx->lenaddon,
		       dctx->lenextrabits));
#ifdef ANALYSIS
	    if (analyse_level > 1)
		printf("code-length-repeat: %d copies of %d\n", rep,
		       dctx->lenrep);
#endif
	    while (rep > 0 && dctx->lenptr < dctx->hlit + dctx->hdist) {
		dctx->lengths[dctx->lenptr] = dctx->lenrep;
		dctx->lenptr++;
		rep--;
	    }
	    dctx->state = TREES_LEN;
	    break;
	  case INBLK:
#ifdef ANALYSIS
	    dctx->bitcount_before = BITCOUNT(dctx);
#endif
	    code = huflookup(dctx, dctx->currlentable);
	    debug(("recv: litlen %d\n", code));
	    if (code == -1)
		goto finished;
	    if (code < 256) {
#ifdef ANALYSIS
		if (analyse_level > 0)
		    printf("%lu: literal %d [%d]\n", dctx->bytesout, code,
			   BITCOUNT(dctx) - dctx->bitcount_before);
#endif
		if (dctx->outfixed)
		    fixed_literal(dctx, code);
		emit_char(dctx, code);
	    } else if (code == 256) {
		if (dctx->lastblock)
		    dctx->state = END;
		else
		    dctx->state = OUTSIDEBLK;
		if (dctx->currlentable != dctx->staticlentable) {
		    freetable(&dctx->currlentable);
		    dctx->currlentable = NULL;
		}
		if (dctx->currdisttable != dctx->staticdisttable) {
		    freetable(&dctx->currdisttable);
		    dctx->currdisttable = NULL;
		}
	    } else if (code < 286) {   /* static tree can give >285; ignore */
		dctx->state = GOTLENSYM;
		dctx->sym = code;
	    }
	    break;
	  case GOTLENSYM:
	    rec = &lencodes[dctx->sym - 257];
	    if (dctx->nbits < rec->extrabits)
		goto finished;
	    dctx->len =
		rec->min + (dctx->bits & ((1 << rec->extrabits) - 1));
	    if (rec->extrabits)
		debug(("recv: litlen-extrabits %d/%d\n",
		       dctx->len - rec->min, rec->extrabits));
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, rec->extrabits);
	    EATBITS(rec->extrabits);
	    dctx->state = GOTLEN;
	    break;
	  case GOTLEN:
	    code = huflookup(dctx, dctx->currdisttable);
	    debug(("recv: dist %d\n", code));
	    if (code == -1)
		goto finished;
	    dctx->state = GOTDISTSYM;
	    dctx->sym = code;
	    break;
	  case GOTDISTSYM:
	    rec = &distcodes[dctx->sym];
	    if (dctx->nbits < rec->extrabits)
		goto finished;
	    dist = rec->min + (dctx->bits & ((1 << rec->extrabits) - 1));
	    if (rec->extrabits)
		debug(("recv: dist-extrabits %d/%d\n",
		       dist - rec->min, rec->extrabits));
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, rec->extrabits);
	    EATBITS(rec->extrabits);
	    dctx->state = INBLK;
#ifdef ANALYSIS
	    if (analyse_level > 0)
		printf("%lu: copy len=%d dist=%d [%d]\n", dctx->bytesout,
		       dctx->len, dist,
		       BITCOUNT(dctx) - dctx->bitcount_before);
#endif
	    if (dctx->outfixed)
		fixed_match(dctx, dist, dctx->len);
	    while (dctx->len--)
		emit_char(dctx, dctx->window[(dctx->winpos - dist) &
					     (DWINSIZE - 1)]);
	    break;
	  case UNCOMP_LEN:
	    /*
	     * Uncompressed block. We expect to see a 16-bit LEN.
	     */
	    if (dctx->nbits < 16)
		goto finished;
	    dctx->uncomplen = dctx->bits & 0xFFFF;
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 16);
	    EATBITS(16);
	    dctx->state = UNCOMP_NLEN;
	    break;
	  case UNCOMP_NLEN:
	    /*
	     * Uncompressed block. We expect to see a 16-bit NLEN,
	     * which should be the one's complement of the previous
	     * LEN.
	     */
	    if (dctx->nbits < 16)
		goto finished;
	    nlen = dctx->bits & 0xFFFF;
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 16);
	    EATBITS(16);
	    if (dctx->uncomplen == 0) {/* block is empty */
		if (dctx->lastblock)
		    dctx->state = END;
		else
		    dctx->state = OUTSIDEBLK;
	    } else
		dctx->state = UNCOMP_DATA;
	    break;
	  case UNCOMP_DATA:
	    if (dctx->nbits < 8)
		goto finished;
#ifdef ANALYSIS
	    if (analyse_level > 0)
		printf("%lu: uncompressed %d [8]\n", dctx->bytesout,
		       (int)(dctx->bits & 0xFF));
#endif
	    if (dctx->outfixed)
		fixed_literal(dctx, dctx->bits & 0xFF);
	    emit_char(dctx, dctx->bits & 0xFF);
	    if (!dctx->outfixed)
		outbits(dctx, dctx->bits, 8);
	    EATBITS(8);
	    if (--dctx->uncomplen == 0) {	/* end of uncompressed block */
		if (dctx->lastblock)
		    dctx->state = END;
		else
		    dctx->state = OUTSIDEBLK;
	    }
	    break;
	  case END:
	    /*
	     * End of compressed data. We align to a byte boundary,
	     * and then look for format-specific trailer data.
	     */
	    {
		int to_eat = dctx->nbits & 7;
		if (!dctx->outfixed)
		    outbits(dctx, 0, to_eat);
		EATBITS(to_eat);
	    }
	    if (dctx->type == DEFLATE_TYPE_ZLIB)
		dctx->state = ADLER1;
	    else if (dctx->type == DEFLATE_TYPE_GZIP)
		dctx->state = CRC1;
	    else
		dctx->state = FINALSPIN;
	    break;
	  case ADLER1:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = (dctx->bits & 0xFF) << 8;
	    EATBITS(8);
	    cksum |= (dctx->bits & 0xFF);
	    EATBITS(8);
	    if (cksum != ((dctx->checksum >> 16) & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = ADLER2;
	    break;
	  case ADLER2:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = (dctx->bits & 0xFF) << 8;
	    EATBITS(8);
	    cksum |= (dctx->bits & 0xFF);
	    EATBITS(8);
	    if (cksum != (dctx->checksum & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = FINALSPIN;
	    break;
	  case CRC1:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (cksum != (dctx->checksum & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = CRC2;
	    break;
	  case CRC2:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (cksum != ((dctx->checksum >> 16) & 0xFFFF)) {
		error = DEFLATE_ERR_CHECKSUM;
		goto finished;
	    }
	    dctx->state = ILEN1;
	    break;
	  case ILEN1:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (cksum != (dctx->bytesout & 0xFFFF)) {
		error = DEFLATE_ERR_INLEN;
		goto finished;
	    }
	    dctx->state = ILEN2;
	    break;
	  case ILEN2:
	    if (dctx->nbits < 16)
		goto finished;
	    cksum = dctx->bits & 0xFFFF;
	    EATBITS(16);
	    if (cksum != ((dctx->bytesout >> 16) & 0xFFFF)) {
		error = DEFLATE_ERR_INLEN;
		goto finished;
	    }
	    dctx->state = FINALSPIN;
	    break;
	  case FINALSPIN:
	    /* Just ignore any trailing garbage on the data stream. */
	    /* (We could alternatively throw an error here, if we wanted
	     * to detect and complain about trailing garbage.) */
	    EATBITS(dctx->nbits);
	    break;
	}
    }

    finished:
    *outblock = dctx->outblk;
    *outlen = dctx->outlen;
    return error;
}

#define A(code,str) str
const char *const deflate_error_msg[DEFLATE_NUM_ERRORS] = {
    DEFLATE_ERRORLIST(A)
};
#undef A

#define A(code,str) #code
const char *const deflate_error_sym[DEFLATE_NUM_ERRORS] = {
    DEFLATE_ERRORLIST(A)
};
#undef A

enum { DEFLATE_TYPE_AUTO = 3 };

int main(int argc, char **argv)
{
    unsigned char buf[65536];
    void *outbuf;
    int ret, err, outlen;
    deflate_decompress_ctx *dhandle;
    int type = DEFLATE_TYPE_AUTO, opts = TRUE;
    int got_arg = FALSE;
    int outfixed = FALSE;
    char *filename = NULL;
    FILE *fp;

    while (--argc) {
        char *p = *++argv;

	got_arg = TRUE;

        if (p[0] == '-' && opts) {
            if (!strcmp(p, "-b"))
                type = DEFLATE_TYPE_BARE;
            else if (!strcmp(p, "-g"))
                type = DEFLATE_TYPE_GZIP;
            else if (!strcmp(p, "-z"))
                type = DEFLATE_TYPE_ZLIB;
            else if (!strcmp(p, "-f"))
                outfixed = TRUE;
            else if (!strcmp(p, "--"))
                opts = FALSE;          /* next thing is filename */
            else {
                fprintf(stderr, "unknown command line option '%s'\n", p);
                return 1;
            }
        } else if (!filename) {
            filename = p;
        } else {
            fprintf(stderr, "can only handle one filename\n");
            return 1;
        }
    }

    dhandle = NULL;

    if (filename)
        fp = fopen(filename, "rb");
    else
        fp = stdin;

    if (!fp) {
        assert(filename);
        fprintf(stderr, "unable to open '%s'\n", filename);
        return 1;
    }

    do {
	ret = fread(buf, 1, sizeof(buf), fp);
	if (!dhandle) {
	    if (type == DEFLATE_TYPE_AUTO) {
		/*
		 * Attempt to autodetect the input file type.
		 */
		if (ret >= 2 && buf[0] == 0x1F && buf[1] == 0x8B)
		    type = DEFLATE_TYPE_GZIP;
		else if (ret >= 2 && buf[0] == 0x78 &&
			 (buf[1] & 0x20) == 0 &&
			 (buf[0]*256+buf[1]) % 31 == 0)
		    type = DEFLATE_TYPE_ZLIB;
		else
		    type = DEFLATE_TYPE_BARE;
	    }
	    dhandle = deflate_decompress_new(type, outfixed);
	}
	outbuf = NULL;
	if (ret > 0)
	    err = deflate_decompress_data(dhandle, buf, ret,
					  (void **)&outbuf, &outlen);
	else
	    err = deflate_decompress_data(dhandle, NULL, 0,
					  (void **)&outbuf, &outlen);
        if (outbuf) {
            if (outlen)
                fwrite(outbuf, 1, outlen, stdout);
            sfree(outbuf);
        }
	if (err > 0) {
            fprintf(stderr, "decoding error: %s\n", deflate_error_msg[err]);
            return 1;
        }
    } while (ret > 0);

    if (dhandle)
	deflate_decompress_free(dhandle);

    if (filename)
        fclose(fp);

    return 0;
}
