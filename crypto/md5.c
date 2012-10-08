#include <stdio.h>
#include <string.h>

#ifdef TEST_VERBOSE
#define TEST
#define debug(x) printf x
#else
#define debug(x)
#endif

typedef unsigned int uint32;

/* ----------------------------------------------------------------------
 * Core MD5 algorithm: processes 16-word blocks into a message digest.
 */

#define F(x,y,z) ( ((x) & (y)) | ((~(x)) & (z)) )
#define G(x,y,z) ( ((x) & (z)) | ((~(z)) & (y)) )
#define H(x,y,z) ( (x) ^ (y) ^ (z) )
#define I(x,y,z) ( (y) ^ ( (x) | ~(z) ) )

#define rol(x,y) ( ((x) << (y)) | (((uint32)x) >> (32-y)) )

#define subround(f,w,x,y,z,k,s,ti) \
debug(("  w=%08x blkval=%08x ti=%08x x=%08x y=%08x z=%08x \n" \
       " fn=%08x acc=%08x rolled=%08x final=%08x\n", \
       w, block[k], ti, x, y, z, f(x,y,z), \
       w + f(x,y,z) + block[k] + ti, \
       rol(w + f(x,y,z) + block[k] + ti, s), \
       x + rol(w + f(x,y,z) + block[k] + ti, s))); \
       w = x + rol(w + f(x,y,z) + block[k] + ti, s)

typedef struct {
    uint32 h[4];
} MD5_Core_State;

void MD5_Core_Init(MD5_Core_State *s) {
    s->h[0] = 0x67452301;
    s->h[1] = 0xefcdab89;
    s->h[2] = 0x98badcfe;
    s->h[3] = 0x10325476;
}

void MD5_Block(MD5_Core_State *s, uint32 *block) {
    uint32 a,b,c,d;

    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];

    debug((" 0: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, a, b, c, d, 0, 7, 0xd76aa478);
    debug((" 1: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, d, a, b, c, 1, 12, 0xe8c7b756);
    debug((" 2: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, c, d, a, b, 2, 17, 0x242070db);
    debug((" 3: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, b, c, d, a, 3, 22, 0xc1bdceee);
    debug((" 4: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, a, b, c, d, 4, 7, 0xf57c0faf);
    debug((" 5: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, d, a, b, c, 5, 12, 0x4787c62a);
    debug((" 6: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, c, d, a, b, 6, 17, 0xa8304613);
    debug((" 7: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, b, c, d, a, 7, 22, 0xfd469501);
    debug((" 8: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, a, b, c, d, 8, 7, 0x698098d8);
    debug((" 9: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, d, a, b, c, 9, 12, 0x8b44f7af);
    debug(("10: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, c, d, a, b, 10, 17, 0xffff5bb1);
    debug(("11: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, b, c, d, a, 11, 22, 0x895cd7be);
    debug(("12: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, a, b, c, d, 12, 7, 0x6b901122);
    debug(("13: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, d, a, b, c, 13, 12, 0xfd987193);
    debug(("14: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, c, d, a, b, 14, 17, 0xa679438e);
    debug(("15: %08x %08x %08x %08x\n", a, b, c, d));
    subround(F, b, c, d, a, 15, 22, 0x49b40821);
    debug(("16: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, a, b, c, d, 1, 5, 0xf61e2562);
    debug(("17: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, d, a, b, c, 6, 9, 0xc040b340);
    debug(("18: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, c, d, a, b, 11, 14, 0x265e5a51);
    debug(("19: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, b, c, d, a, 0, 20, 0xe9b6c7aa);
    debug(("20: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, a, b, c, d, 5, 5, 0xd62f105d);
    debug(("21: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, d, a, b, c, 10, 9, 0x02441453);
    debug(("22: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, c, d, a, b, 15, 14, 0xd8a1e681);
    debug(("23: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, b, c, d, a, 4, 20, 0xe7d3fbc8);
    debug(("24: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, a, b, c, d, 9, 5, 0x21e1cde6);
    debug(("25: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, d, a, b, c, 14, 9, 0xc33707d6);
    debug(("26: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, c, d, a, b, 3, 14, 0xf4d50d87);
    debug(("27: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, b, c, d, a, 8, 20, 0x455a14ed);
    debug(("28: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, a, b, c, d, 13, 5, 0xa9e3e905);
    debug(("29: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, d, a, b, c, 2, 9, 0xfcefa3f8);
    debug(("30: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, c, d, a, b, 7, 14, 0x676f02d9);
    debug(("31: %08x %08x %08x %08x\n", a, b, c, d));
    subround(G, b, c, d, a, 12, 20, 0x8d2a4c8a);
    debug(("32: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, a, b, c, d, 5, 4, 0xfffa3942);
    debug(("33: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, d, a, b, c, 8, 11, 0x8771f681);
    debug(("34: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, c, d, a, b, 11, 16, 0x6d9d6122);
    debug(("35: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, b, c, d, a, 14, 23, 0xfde5380c);
    debug(("36: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, a, b, c, d, 1, 4, 0xa4beea44);
    debug(("37: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, d, a, b, c, 4, 11, 0x4bdecfa9);
    debug(("38: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, c, d, a, b, 7, 16, 0xf6bb4b60);
    debug(("39: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, b, c, d, a, 10, 23, 0xbebfbc70);
    debug(("40: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, a, b, c, d, 13, 4, 0x289b7ec6);
    debug(("41: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, d, a, b, c, 0, 11, 0xeaa127fa);
    debug(("42: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, c, d, a, b, 3, 16, 0xd4ef3085);
    debug(("43: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, b, c, d, a, 6, 23, 0x04881d05);
    debug(("44: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, a, b, c, d, 9, 4, 0xd9d4d039);
    debug(("45: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, d, a, b, c, 12, 11, 0xe6db99e5);
    debug(("46: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, c, d, a, b, 15, 16, 0x1fa27cf8);
    debug(("47: %08x %08x %08x %08x\n", a, b, c, d));
    subround(H, b, c, d, a, 2, 23, 0xc4ac5665);
    debug(("48: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, a, b, c, d, 0, 6, 0xf4292244);
    debug(("49: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, d, a, b, c, 7, 10, 0x432aff97);
    debug(("50: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, c, d, a, b, 14, 15, 0xab9423a7);
    debug(("51: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, b, c, d, a, 5, 21, 0xfc93a039);
    debug(("52: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, a, b, c, d, 12, 6, 0x655b59c3);
    debug(("53: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, d, a, b, c, 3, 10, 0x8f0ccc92);
    debug(("54: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, c, d, a, b, 10, 15, 0xffeff47d);
    debug(("55: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, b, c, d, a, 1, 21, 0x85845dd1);
    debug(("56: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, a, b, c, d, 8, 6, 0x6fa87e4f);
    debug(("57: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, d, a, b, c, 15, 10, 0xfe2ce6e0);
    debug(("58: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, c, d, a, b, 6, 15, 0xa3014314);
    debug(("59: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, b, c, d, a, 13, 21, 0x4e0811a1);
    debug(("60: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, a, b, c, d, 4, 6, 0xf7537e82);
    debug(("61: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, d, a, b, c, 11, 10, 0xbd3af235);
    debug(("62: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, c, d, a, b, 2, 15, 0x2ad7d2bb);
    debug(("63: %08x %08x %08x %08x\n", a, b, c, d));
    subround(I, b, c, d, a, 9, 21, 0xeb86d391);
    debug(("64: %08x %08x %08x %08x\n", a, b, c, d));

    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    debug(("done: %08x %08x %08x %08x\n", s->h[0], s->h[1], s->h[2], s->h[3]));
}

/* ----------------------------------------------------------------------
 * Outer MD5 algorithm: take an arbitrary length byte string,
 * convert it into 16-word blocks with the prescribed padding at
 * the end, and pass those blocks to the core MD5 algorithm.
 */

#define BLKSIZE 64

typedef struct {
    MD5_Core_State core;
    unsigned char block[BLKSIZE];
    int blkused;
    uint32 lenhi, lenlo;
} MD5_State;

void MD5_Init(MD5_State *s) {
    MD5_Core_Init(&s->core);
    s->blkused = 0;
    s->lenhi = s->lenlo = 0;
}

void MD5_Bytes(MD5_State *s, void *p, int len) {
    unsigned char *q = (unsigned char *)p;
    uint32 wordblock[16];
    uint32 lenw = len;
    int i;

    /*
     * Update the length field.
     */
    s->lenlo += lenw;
    s->lenhi += (s->lenlo < lenw);

    if (s->blkused+len < BLKSIZE) {
        /*
         * Trivial case: just add to the block.
         */
        memcpy(s->block + s->blkused, q, len);
        s->blkused += len;
    } else {
        /*
         * We must complete and process at least one block.
         */
        while (s->blkused + len >= BLKSIZE) {
            memcpy(s->block + s->blkused, q, BLKSIZE - s->blkused);
            q += BLKSIZE - s->blkused;
            len -= BLKSIZE - s->blkused;
            /* Now process the block. Gather bytes little-endian into words */
            for (i = 0; i < 16; i++) {
                wordblock[i] =
                    ( ((uint32)s->block[i*4+3]) << 24 ) |
                    ( ((uint32)s->block[i*4+2]) << 16 ) |
                    ( ((uint32)s->block[i*4+1]) <<  8 ) |
                    ( ((uint32)s->block[i*4+0]) <<  0 );
            }
            MD5_Block(&s->core, wordblock);
            s->blkused = 0;
        }
        memcpy(s->block, q, len);
        s->blkused = len;
    }
}

void MD5_Final(MD5_State *s, unsigned char *output) {
    int i;
    int pad;
    unsigned char c[64];
    uint32 lenhi, lenlo;

    if (s->blkused >= 56)
        pad = 56 + 64 - s->blkused;
    else
        pad = 56 - s->blkused;

    lenhi = (s->lenhi << 3) | (s->lenlo >> (32-3));
    lenlo = (s->lenlo << 3);

    memset(c, 0, pad);
    c[0] = 0x80;
    MD5_Bytes(s, &c, pad);

    c[7] = (lenhi >> 24) & 0xFF;
    c[6] = (lenhi >> 16) & 0xFF;
    c[5] = (lenhi >>  8) & 0xFF;
    c[4] = (lenhi >>  0) & 0xFF;
    c[3] = (lenlo >> 24) & 0xFF;
    c[2] = (lenlo >> 16) & 0xFF;
    c[1] = (lenlo >>  8) & 0xFF;
    c[0] = (lenlo >>  0) & 0xFF;

    MD5_Bytes(s, &c, 8);

    for (i = 0; i < 4; i++) {
        output[4*i+3] = (s->core.h[i] >> 24) & 0xFF;
        output[4*i+2] = (s->core.h[i] >> 16) & 0xFF;
        output[4*i+1] = (s->core.h[i] >>  8) & 0xFF;
        output[4*i+0] = (s->core.h[i] >>  0) & 0xFF;
    }
}

void MD5_Simple(void *p, int len, unsigned char *output) {
    MD5_State s;

    MD5_Init(&s);
    MD5_Bytes(&s, p, len);
    MD5_Final(&s, output);
}

#ifdef TEST

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DOTEST(s,d) \
   MD5_Simple(s, sizeof(s)-1, digest); assert(memcmp(digest, d, 16) == 0);

int main(int argc, char **argv)
{
    unsigned char digest[16];
    char *p;

    if (argc <= 1) {
	/*
	 * These test strings and their expected hash values come
	 * straight from RFC 1321.
	 */
	DOTEST("",
	       "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e");
	DOTEST("a",
	       "\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61");
	DOTEST("abc",
	       "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72");
	DOTEST("message digest",
	       "\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d\x52\x5a\x2f\x31\xaa\xf1\x61\xd0");
	DOTEST("abcdefghijklmnopqrstuvwxyz",
	       "\xc3\xfc\xd3\xd7\x61\x92\xe4\x00\x7d\xfb\x49\x6c\xca\x67\xe1\x3b");
	DOTEST("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	       "\xd1\x74\xab\x98\xd2\x77\xd9\xf5\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f");
	DOTEST("1234567890123456789012345678901234567890"
	       "1234567890123456789012345678901234567890",
	       "\x57\xed\xf4\xa2\x2b\xe3\xc9\x55\xac\x49\xda\x2e\x21\x07\xb6\x7a");
	printf("ok\n");
    } else {
	MD5_State s;
	FILE *fp;
	char buf[4096];
	int i, ret;

	if (!(fp = fopen(argv[1], "rb"))) {
	    perror("open");
	    return 1;
	}
	MD5_Init(&s);
	while ((ret = fread(buf, 1, sizeof(buf), fp)) > 0) {
	    MD5_Bytes(&s, buf, ret);
	}
	if (ret < 0) {
	    perror("read");
	    return 1;
	}
	fclose(fp);
	MD5_Final(&s, digest);
	for (i = 0; i < 16; i++)
	    printf("%02x", digest[i]);
	printf("\n");
    }
    return 0;
}

#endif
