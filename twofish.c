/*
 * Twofish implementation with full keying.
 */

typedef unsigned int word32;

#define GET_32BIT_LSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0]) | \
  ((unsigned long)(unsigned char)(cp)[1] << 8) | \
  ((unsigned long)(unsigned char)(cp)[2] << 16) | \
  ((unsigned long)(unsigned char)(cp)[3] << 24))

#define PUT_32BIT_LSB_FIRST(cp, value) do { \
  (cp)[0] = (value); \
  (cp)[1] = (value) >> 8; \
  (cp)[2] = (value) >> 16; \
  (cp)[3] = (value) >> 24; } while (0)

#define NROUNDS 16

typedef struct {
    word32 keysched[8 + 2*NROUNDS];
    word32 SM[4][256];
} TwofishContext;

void twofish_encrypt(TwofishContext *ctx, word32 *block) {
    word32 a,b,c,d;
    word32 *initkey, *finalkey, *roundkey;
    int i;

    initkey = ctx->keysched;
    finalkey = ctx->keysched + 4;
    roundkey = ctx->keysched + 8;

    /* Whitening on input. */
    a = block[0] ^ initkey[0]; b = block[1] ^ initkey[1];
    c = block[2] ^ initkey[2]; d = block[3] ^ initkey[3];

    for (i = 0; i < NROUNDS; i++) {
	word32 t0, t1;
	t0 = (ctx->SM[0][(a      ) & 0xFF] ^
	      ctx->SM[1][(a >>  8) & 0xFF] ^
	      ctx->SM[2][(a >> 16) & 0xFF] ^
	      ctx->SM[3][(a >> 24) & 0xFF]);
	t1 = (ctx->SM[0][(b >> 24) & 0xFF] ^
	      ctx->SM[1][(b      ) & 0xFF] ^
	      ctx->SM[2][(b >>  8) & 0xFF] ^
	      ctx->SM[3][(b >> 16) & 0xFF]);
	t0 += t1; t1 += t0;
	t0 += roundkey[0];
	t1 += roundkey[1];
	roundkey += 2;
	c ^= t0; c = (c >> 1) ^ (c << 31);
	d = (d << 1) ^ (d >> 31); d ^= t1;
	t0 = a; a = c; c = t0;
	t1 = b; b = d; d = t1;
    }

    /* Whitening on output. Also undo final swap. */
    block[0] = c ^ finalkey[0]; block[1] = d ^ finalkey[1];
    block[2] = a ^ finalkey[2]; block[3] = b ^ finalkey[3];
}

void twofish_decrypt(TwofishContext *ctx, word32 *block) {
    word32 a,b,c,d;
    word32 *initkey, *finalkey, *roundkey;
    int i;

    initkey = ctx->keysched + 4;
    finalkey = ctx->keysched;
    roundkey = ctx->keysched + (8 + (NROUNDS-1)*2);

    /* Whitening on input. */
    a = block[0] ^ initkey[0]; b = block[1] ^ initkey[1];
    c = block[2] ^ initkey[2]; d = block[3] ^ initkey[3];

    for (i = 0; i < NROUNDS; i++) {
	word32 t0, t1;
	t0 = (ctx->SM[0][(a      ) & 0xFF] ^
	      ctx->SM[1][(a >>  8) & 0xFF] ^
	      ctx->SM[2][(a >> 16) & 0xFF] ^
	      ctx->SM[3][(a >> 24) & 0xFF]);
	t1 = (ctx->SM[0][(b >> 24) & 0xFF] ^
	      ctx->SM[1][(b      ) & 0xFF] ^
	      ctx->SM[2][(b >>  8) & 0xFF] ^
	      ctx->SM[3][(b >> 16) & 0xFF]);
	t0 += t1; t1 += t0;
	t0 += roundkey[0];
	t1 += roundkey[1];
	roundkey -= 2;
	c = (c << 1) ^ (c >> 31); c ^= t0;
	d ^= t1; d = (d >> 1) ^ (d << 31);
	t0 = a; a = c; c = t0;
	t1 = b; b = d; d = t1;
    }

    /* Whitening on output. Also undo final swap. */
    block[0] = c ^ finalkey[0]; block[1] = d ^ finalkey[1];
    block[2] = a ^ finalkey[2]; block[3] = b ^ finalkey[3];
}

static const unsigned char twofish_MDS[4][4] = {
    {0x01, 0xEF, 0x5B, 0x5B},
    {0x5B, 0xEF, 0xEF, 0x01},
    {0xEF, 0x5B, 0x01, 0xEF},
    {0xEF, 0x01, 0xEF, 0x5B},
};

static const unsigned char twofish_RS[4][8] = {
    {0x01, 0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E},
    {0xA4, 0x56, 0x82, 0xF3, 0x1E, 0xC6, 0x68, 0xE5},
    {0x02, 0xA1, 0xFC, 0xC1, 0x47, 0xAE, 0x3D, 0x19},
    {0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E, 0x03},
};

static int twofish_gf28mul(int x, int y, int poly) {
    int r = 0;
    while (y > 0) {
	if (y & 1)
	    r ^= x;
	y >>= 1;
	x <<= 1;
	if (x & 0x100)
	    x ^= poly;
    }
    return r;
}

static int twofish_gf28dot(const unsigned char *x, const unsigned char *y,
			   int len, int poly) {
    int r = 0;
    while (len--)
	r ^= twofish_gf28mul(*x++, *y++, poly);
    return r;
}

/*
 * The q0 and q1 permutations.
 */
static int twofish_q(int x, const unsigned char t[4][16]) {
    int a0, b0, a1, b1, a2, b2, a3, b3, a4, b4;
    a0 = (x>>4) & 0xF;
    b0 = x & 0xF;
    a1 = a0 ^ b0;
    b1 = (a0 ^ ((b0 >> 1) | (b0 << 3)) ^ (a0<<3)) & 0xF;
    a2 = t[0][a1];
    b2 = t[1][b1];
    a3 = a2 ^ b2;
    b3 = (a2 ^ ((b2 >> 1) | (b2 << 3)) ^ (a2<<3)) & 0xF;
    a4 = t[2][a3];
    b4 = t[3][b3];
    return 16*b4 + a4;
}

static int twofish_q0(int x) {
    static const unsigned char t[4][16] = {
	{ 0x8, 0x1, 0x7, 0xD, 0x6, 0xF, 0x3, 0x2,
	  0x0, 0xB, 0x5, 0x9, 0xE, 0xC, 0xA, 0x4, },
	{ 0xE, 0xC, 0xB, 0x8, 0x1, 0x2, 0x3, 0x5,
	  0xF, 0x4, 0xA, 0x6, 0x7, 0x0, 0x9, 0xD, },
	{ 0xB, 0xA, 0x5, 0xE, 0x6, 0xD, 0x9, 0x0,
	  0xC, 0x8, 0xF, 0x3, 0x2, 0x4, 0x7, 0x1, },
	{ 0xD, 0x7, 0xF, 0x4, 0x1, 0x2, 0x6, 0xE,
	  0x9, 0xB, 0x3, 0x0, 0x8, 0x5, 0xC, 0xA, },
    };
    return twofish_q(x, t);
}

static int twofish_q1(int x) {
    static const unsigned char t[4][16] = {
	{ 0x2, 0x8, 0xB, 0xD, 0xF, 0x7, 0x6, 0xE,
	  0x3, 0x1, 0x9, 0x4, 0x0, 0xA, 0xC, 0x5, },
	{ 0x1, 0xE, 0x2, 0xB, 0x4, 0xC, 0x3, 0x7,
	  0x6, 0xD, 0xA, 0x5, 0xF, 0x9, 0x0, 0x8, },
	{ 0x4, 0xC, 0x7, 0x5, 0x1, 0x6, 0x9, 0xA,
	  0x0, 0xE, 0xD, 0x8, 0x2, 0xB, 0x3, 0xF, },
	{ 0xB, 0x9, 0x5, 0x1, 0xC, 0x3, 0xD, 0xE,
	  0x6, 0x4, 0x7, 0xF, 0x2, 0x0, 0x8, 0xA, },
    };
    return twofish_q(x, t);
}

/*
 * The g function, which is the first half of the h function used
 * in key expansion. g is also used as the S-box mapping.
 */
static word32 twofish_g(word32 X, word32 *L, int k) {
    if (k >= 4)
	X = ((twofish_q1((X      ) & 0xFF)      ) |
	     (twofish_q0((X >>  8) & 0xFF) <<  8) |
	     (twofish_q0((X >> 16) & 0xFF) << 16) |
	     (twofish_q1((X >> 24) & 0xFF) << 24)) ^ L[3];
    if (k >= 3)
	X = ((twofish_q1((X      ) & 0xFF)      ) |
	     (twofish_q1((X >>  8) & 0xFF) <<  8) |
	     (twofish_q0((X >> 16) & 0xFF) << 16) |
	     (twofish_q0((X >> 24) & 0xFF) << 24)) ^ L[2];
    X = ((twofish_q0((X      ) & 0xFF)      ) |
	 (twofish_q1((X >>  8) & 0xFF) <<  8) |
	 (twofish_q0((X >> 16) & 0xFF) << 16) |
	 (twofish_q1((X >> 24) & 0xFF) << 24)) ^ L[1];
    X = ((twofish_q0((X      ) & 0xFF)      ) |
	 (twofish_q0((X >>  8) & 0xFF) <<  8) |
	 (twofish_q1((X >> 16) & 0xFF) << 16) |
	 (twofish_q1((X >> 24) & 0xFF) << 24)) ^ L[0];
    X = ((twofish_q1((X      ) & 0xFF)      ) |
	 (twofish_q0((X >>  8) & 0xFF) <<  8) |
	 (twofish_q1((X >> 16) & 0xFF) << 16) |
	 (twofish_q0((X >> 24) & 0xFF) << 24));
    return X;
}

/*
 * The h function, used in key expansion.
 */
static word32 twofish_h(word32 X, word32 *L, int k) {
    unsigned char y[4];
    X = twofish_g(X, L, k);
    PUT_32BIT_LSB_FIRST(y, X);
    return ((twofish_gf28dot(y, twofish_MDS[0], 4, 0x169)      ) |
	    (twofish_gf28dot(y, twofish_MDS[1], 4, 0x169) <<  8) |
	    (twofish_gf28dot(y, twofish_MDS[2], 4, 0x169) << 16) |
	    (twofish_gf28dot(y, twofish_MDS[3], 4, 0x169) << 24));
}

void twofish_setup(TwofishContext *ctx, unsigned char *key, int keylen) {
    unsigned char paddedkey[32];
    word32 Me[4], Mo[4], S[4];
    int i, j, p, k;

    /* Process key into words, and split up into Me and Mo vectors */
    memset(paddedkey, 0, 32);
    memcpy(paddedkey, key, keylen);
    k = (keylen+7) / 8;		       /* k == 2, 3 or 4 */
    for (i = 0; i < 2*k; i++)
	(i%2 ? Mo : Me)[i/2] = GET_32BIT_LSB_FIRST(paddedkey+i*4);

    /* Derive the S vector. */
    for (i = 0; i < k; i++) {
	int j;
	S[k-1-i] = 0;
	for (j = 0; j < 4; j++)
	    S[k-1-i] |= twofish_gf28dot(twofish_RS[j],
					paddedkey+8*i, 8, 0x14D) << (8*j);
    }

    /* Generate the expanded key. */
    for (i = 0; i < 40; i += 2) {
	word32 A, B;
	A = twofish_h(i * 0x01010101, Me, k);
	B = twofish_h((i+1) * 0x01010101, Mo, k);
	B = (B << 8) | (B >> 24);
	A += B;
	B += A;
	ctx->keysched[i] = A;
	ctx->keysched[i+1] = (B << 9) | (B >> 23);
    }

    /* Generate the key-dependent S boxes and then compose them with
     * the MDS matrix to form the SM tables. */
    for (j = 0; j < 256; j++) {
	word32 X = 0x01010101 * j;
	X = twofish_g(X, S, k);
	for (i = 0; i < 4; i++) {
	    int y = (X >> (8*i)) & 0xFF;
	    word32 SMentry = 0;
	    for (p = 0; p < 4; p++)
		SMentry |= twofish_gf28mul(twofish_MDS[p][i],
					   y, 0x169) << (8*p);
	    ctx->SM[i][j] = SMentry;
	}
    }
}

#ifdef TESTMODE
int main(void) {
    unsigned char key[32];
    word32 block[8];
    TwofishContext ctx;

    memset(key, 0, sizeof(key));
    memset(block, 0, sizeof(block));

    twofish_setup(&ctx, key, 16);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_encrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_decrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);

    memcpy(key, ("\x01\x23\x45\x67\x89\xab\xcd\xef"
		 "\xfe\xdc\xba\x98\x76\x54\x32\x10"
		 "\x00\x11\x22\x33\x44\x55\x66\x77"
		 "\x88\x99\xaa\xbb\xcc\xdd\xee\xff"), 32);
    twofish_setup(&ctx, key, 24);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_encrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_decrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);

    twofish_setup(&ctx, key, 32);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_encrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);
    twofish_decrypt(&ctx, block);
    printf("%08x:%08x:%08x:%08x **\n", block[0], block[1], block[2], block[3]);

    return 0;
}
#endif
