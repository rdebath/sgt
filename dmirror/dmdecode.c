/*
 * Space-efficient in-memory decoder for bit-mirrored Deflate, as
 * produced by 'dmirror'.
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

struct huflength {
    unsigned short len, code, index;
};
struct huf {
    int nlengths;
    struct huflength lengths[15];
    unsigned short *syms;
};

/*
 * Set up a Huffman table. On input 'syms' contains the code length
 * for each symbol, or zero if that symbol has no code; on output,
 * the same space is reused as part of the Huffman table data
 * structure.
 */
void hufsetup(struct huf *huf, unsigned short *syms, int nsyms)
{
    unsigned i, j, code;
    struct huflength *const lengths = huf->lengths;

    /*
     * Start by counting up the number of symbols of each code length.
     */
    for (i = 0; i < 15; i++)
	lengths[i].len = 0;
    for (i = 0; i < nsyms; i++) {
	if (syms[i])
	    lengths[syms[i]-1].len++;
    }

    /*
     * Now work out the smallest code value for each length. Here we
     * also check by assertion that the Huffman code space is
     * precisely covered, neither over- nor under-committed.
     */
    code = 0;
    for (i = 0; i < 15; i++) {
	lengths[i].code = code;
	code += lengths[i].len << (15 - i);
    }
    assert(code == 1 << 16);

    /*
     * Find the cumulative symbol counts.
     */
    lengths[0].index = lengths[0].len;
    for (i = 1; i < 15; i++)
	lengths[i].index = lengths[i].len + lengths[i-1].index;
    code = nsyms;

    /*
     * Work out the lexicographic index of each symbol.
     */
    for (i = nsyms; i-- > 0 ;) {
	if (syms[i])
	    syms[i] = --lengths[syms[i]-1].index;
	else   /* assign terminal codes to unrepresented symbols */
	    syms[i] = --code;
    }

    /*
     * Compress the lengths table to only the used elements.
     */
    for (i = j = 0; i < 15; i++) {
	if (lengths[i].len) {
	    lengths[j].len = i + 1;
	    lengths[j].code = lengths[i].code;
	    lengths[j].index = lengths[i].index;
	    j++;
	}
    }
    huf->nlengths = j;

    /*
     * Now syms[] maps each symbol value to its lexicographic index.
     * We want it exactly the other way round, so invert the
     * permutation.
     */
    for (i = 0; i < nsyms; i++)
	syms[i] += nsyms;	       /* mark as not-done-yet */

    for (i = 0; i < nsyms; i++) {
	unsigned k, saved;

	j = i;
	saved = syms[j];
	while (syms[j] >= nsyms) {
	    k = syms[j] - nsyms;
	    syms[j] = saved;
	    saved = j;
	    j = k;
	}
    }

    huf->syms = syms;
}

unsigned hufdecode(const struct huf *huf, unsigned long *inbits, int *ninbits)
{
    int i;
    unsigned short inword = *inbits >> 16;

    /*
     * We search the lengths array linearly from the start. This is
     * more efficient than it sounds, because the fact that this
     * array represents a _Huffman_ code means that the shorter
     * lengths - and therefore the early entries in the array - will
     * appear much more often than the longer ones. The alternative
     * strategy of binary search is actually less efficient in this
     * case, since it lets the existence of the rare longer codes
     * influence the running time of even the common case.
     */
    for (i = 0; i+1 < huf->nlengths; i++)
	if (inword < huf->lengths[i+1].code)
	    break;

    *inbits <<= huf->lengths[i].len;
    *ninbits -= huf->lengths[i].len;

    inword -= huf->lengths[i].code;
    inword >>= 16 - huf->lengths[i].len;
    inword += huf->lengths[i].index;

    return huf->syms[inword];
}

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
    int btype, bfinal;

    do {
	REFILL;

	/*
	 * Read Deflate block header.
	 */
	bfinal = GETBITS(1); EATBITS(1);
	btype = GETBITS(2); EATBITS(2);

	assert(btype != 3);	       /* undefined block type */

	if (btype == 0) {
	    /*
	     * A Deflate uncompressed block. Start by eating bits to
	     * align to a block boundary, then read two 16-bit
	     * length words which should be two's-complements of
	     * each other.
	     */
	    unsigned len, len2;
	    EATBITS(ninbits & 7);
	    REFILL;
	    len = GETBITS(16); EATBITS(16);
	    len2 = GETBITS(16); EATBITS(16);
	    assert(ninbits == 0);
	    assert(len2 == len ^ 0xFFFF);
#ifdef ILENCOUNT
	    assert(inend - in >= len);
#endif
	    while (len-- > 0)
		OUTPUT(*in++);
	} else {
	    unsigned short treelen[288 + 32];
	    unsigned i, sym;
	    struct huf litlentree, disttree;

	    /*
	     * A standard Deflate block. First see whether we've got
	     * static or dynamic Huffman trees.
	     */
	    if (btype == 1) {
		/*
		 * Set up the static trees.
		 */
		for (i = 0; i < 144; i++) treelen[i] = 8;
		for (i = 144; i < 256; i++) treelen[i] = 9;
		for (i = 256; i < 280; i++) treelen[i] = 7;
		for (i = 280; i < 288; i++) treelen[i] = 8;
		for (i = 288; i < 288 + 32; i++) treelen[i] = 5;
		hufsetup(&litlentree, treelen, 288);
		hufsetup(&disttree, treelen + 288, 32);
	    } else {
		unsigned short lenlen[19];
		struct huf lenlentree;
		unsigned hlit, hdist, hclen;
		static const unsigned char lenlenmap[] = {
		    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3,
		    13, 2, 14, 1, 15
		};

		/*
		 * Dynamic trees. Start by reading a 14-bit block
		 * header.
		 */
		hlit = 257 + GETBITS(5); EATBITS(5);
		hdist = 1 + GETBITS(5); EATBITS(5);
		hclen = 4 + GETBITS(4); EATBITS(4);

		/*
		 * Read in the length codes for the Huffman tree
		 * that encodes the main length codes, and then set
		 * up that Huffman tree.
		 */
		for (i = 0; i < lenof(lenlen); i++)
		    lenlen[i] = 0;
		for (i = 0; i < hclen; i++) {
		    REFILL;
		    lenlen[lenlenmap[i]] = GETBITS(3); EATBITS(3);
		}
		hufsetup(&lenlentree, lenlen, lenof(lenlen));

		/*
		 * Now read in the length codes for the main trees,
		 * and build them. (We must read these in a single
		 * combined pass, because run-length encoding can
		 * run on between them.)
		 */
		i = 0;
		while (i < hlit + hdist) {
		    REFILL;
		    sym = hufdecode(&lenlentree, &inbits, &ninbits);
		    if (sym < 16) {
			treelen[i++] = sym;
		    } else {
			unsigned replen, code;
			if (sym == 16) {
			    replen = 3 + GETBITS(2); EATBITS(2);
			    code = (i > 0 ? treelen[i-1] : 0);
			} else if (sym == 17) {
			    replen = 3 + GETBITS(3); EATBITS(3);
			    code = 0;
			} else if (sym == 18) {
			    replen = 11 + GETBITS(7); EATBITS(7);
			    code = 0;
			}
			assert(i + replen <= hlit + hdist);
			while (replen-- > 0)
			    treelen[i++] = code;
		    }
		}
		hufsetup(&litlentree, treelen, hlit);
		hufsetup(&disttree, treelen + hlit, hdist);
	    }

	    /*
	     * Now we're in the block proper. Read data until we see
	     * the end-of-block code.
	     */
	    while (1) {
		REFILL;
		sym = hufdecode(&litlentree, &inbits, &ninbits);
		assert(ninbits >= 0);
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
		    sym = hufdecode(&disttree, &inbits, &ninbits);
		    REFILL;
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
	}

    } while (!bfinal);

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
