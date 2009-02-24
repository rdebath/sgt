/*
 * Space-efficient in-memory decoder for bit-mirrored Deflate with
 * one fixed-tree block, as produced by 'dmirror -f'.
 *
 * Compile with various #defines:
 * 
 *  - defining 'ILENCOUNT' causes mirrorinflate() to take a
 *    parameter counting the length of the compressed data, and
 *    bounds-check against it.
 * 
 *  - defining 'OLENCOUNT' causes mirrorinflate() to return the
 *    length of the decompressed data rather than void.
 * 
 *  - defining 'NULLPERMITTED' causes mirrorinflate() to accept NULL
 *    as an output pointer, in which case no data will be written.
 *    (Useful with OLENCOUNT, to go over the input data twice in
 *    order to know how much memory to allocate.)
 *
 *  - defining 'STANDALONE' compiles in a simple main().
 */

/*
 * This software is copyright 2008 Simon Tatham.
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

#include <stddef.h>
#include <assert.h>

#define lenof(x) (sizeof((x))/sizeof(*(x)))

#ifdef STANDALONE
#define ILENCOUNT
#define OLENCOUNT
#define NULLPERMITTED
#endif

#ifdef NULLPERMITTED
#define REALOUTPUT(c) ( (void)((out) ? (*out++ = (c)) : (c)) )
#define READBACK(dist) ( ((out) ? *(out - dist) : 0) )
#else
#define REALOUTPUT(c) ( *out++ = (c) )
#define READBACK(dist) ( *(out - dist) )
#endif

#ifdef OLENCOUNT
#define OUTPUT(c) (retlen++, REALOUTPUT(c))
#else
#define OUTPUT(c) REALOUTPUT(c)
#endif

/* GETBITS can be simpler if 'inbits' is known to be an instance of an
 * exactly-32-bit integer type */
#define GETBITS(n) ((inbits >> (32-(n))) & ((1 << (n))-1))
#define EATBITS(n) (inbits <<= (n), ninbits -= (n))

#ifdef ILENCOUNT
#define REFILL do { \
    while (ninbits <= 24 && in < inend) { \
	inbits |= *in++ << (24 - ninbits); \
	ninbits += 8; \
    } \
} while (0)
#else
#define REFILL do { \
    while (ninbits <= 24) { \
	inbits |= *in++ << (24 - ninbits); \
	ninbits += 8; \
    } \
} while (0)
#endif

#ifdef OLENCOUNT
size_t
#else
void
#endif
mirrorinflate(void *decompressed, const void *compressed
#ifdef ILENCOUNT
	      , size_t complen
#endif
	      )
{
#ifdef OLENCOUNT
    size_t retlen = 0;
#endif
    unsigned char *out = (unsigned char *)decompressed;
    const unsigned char *in = (unsigned char *)compressed;
#ifdef ILENCOUNT
    const unsigned char *inend = in + complen;
#endif
    unsigned long inbits = 0;
    int ninbits = 0;
    unsigned sym;

    while (1) {
	REFILL;

	sym = GETBITS(8);
	if (sym < 0xC0) {
	    if (sym < 0x30) {
		sym = GETBITS(7) + 256;
		EATBITS(7);
	    } else {
		sym = GETBITS(8) - 0x30;
		EATBITS(8);
	    }
	} else {
	    if (sym < 0xC8) {
		sym = GETBITS(8) - 0xC0 + 280;
		EATBITS(8);
	    } else {
		sym = GETBITS(9) - 0x190 + 144;
		EATBITS(9);
	    }
	}
	assert(ninbits > 0);

	if (sym < 256) {
	    OUTPUT(sym);
	} else if (sym == 256) {
	    break;
	} else {
	    struct codedata { unsigned short extrabits, addend; };
	    static const struct codedata lendata[] = {
		{0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8},
		{0,9}, {0,10}, {1,11}, {1,13}, {1,15},
		{1,17}, {2,19}, {2,23}, {2,27}, {2,31},
		{3,35}, {3,43}, {3,51}, {3,59}, {4,67},
		{4,83}, {4,99}, {4,115}, {5,131}, {5,163},
		{5,195}, {5,227}, {0,258}
	    };
	    static const struct codedata distdata[] = {
		{0,1}, {0,2}, {0,3}, {0,4}, {1,5}, {1,7},
		{2,9}, {2,13}, {3,17}, {3,25}, {4,33},
		{4,49}, {5,65}, {5,97}, {6,129}, {6,193},
		{7,257}, {7,385}, {8,513}, {8,769},
		{9,1025}, {9,1537}, {10,2049}, {10,3073},
		{11,4097}, {11,6145}, {12,8193}, {12,12289},
		{13,16385}, {13,24577}
	    };
	    unsigned len, dist;

	    sym -= 257;
	    assert(sym < lenof(lendata));
	    len = lendata[sym].addend;
	    len += GETBITS(lendata[sym].extrabits);
	    EATBITS(lendata[sym].extrabits);

	    REFILL;
	    sym = GETBITS(5);
	    EATBITS(5);
	    assert(sym < lenof(distdata));
	    dist = distdata[sym].addend;
	    dist += GETBITS(distdata[sym].extrabits);
	    EATBITS(distdata[sym].extrabits);
	    assert(ninbits >= 0);

	    while (len-- > 0) {
		unsigned char c = READBACK(dist);
		OUTPUT(c);
	    }
	}
    }

#ifdef OLENCOUNT
    return retlen;
#endif
}

#ifdef STANDALONE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv)
{
    char *infile = NULL, *outfile = NULL;
    FILE *infp, *outfp;
    unsigned char *in, *out;
    size_t inlen, insize, outlen;

    if (argc > 1)
	infile = argv[1];

    if (argc > 2)
	outfile = argv[2];

    if (infile) {
	infp = fopen(infile, "rb");
	if (!infp) {
	    fprintf(stderr, "%s: fopen: %s\n", infile, strerror(errno));
	    return 1;
	}
    } else
	infp = stdin;

    inlen = 0;
    insize = 32768;
    in = malloc(insize);
    while (1) {
	int ret;
	if (!in) {
	    fprintf(stderr, "out of memory\n");
	    return 1;
	}
	ret = fread(in + inlen, 1, insize - inlen, infp);
	if (ret < 0) {
	    fprintf(stderr, "%s: fread: %s\n", infile, strerror(errno));
	    return 1;
	} else if (ret == 0) {
	    break;
	} else {
	    inlen += ret;
	    if (insize - inlen < 32768) {
		insize = insize * 5 / 4 + 32768;
		in = realloc(in, insize);
	    }
	}
    }

    if (infp != stdin)
	fclose(infp);

    outlen = mirrorinflate(NULL, in, inlen);
    out = malloc(outlen);
    if (!out) {
	fprintf(stderr, "out of memory\n");
	return 1;
    }
    mirrorinflate(out, in, inlen);

    if (outfile) {
	outfp = fopen(outfile, "wb");
	if (!outfp) {
	    fprintf(stderr, "%s: fopen: %s\n", outfile, strerror(errno));
	    return 1;
	}
    } else
	outfp = stdout;

    fwrite(out, 1, outlen, outfp);

    if (outfp != stdout)
	fclose(outfp);

    return 0;
}

#endif
