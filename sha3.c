/*
 * SHA-3 (Keccak) as described at
 * 
 *   http://keccak.noekeon.org/specs_summary.html
 */

/*
 * This is by no means a general implementation of Keccak; it is just
 * the subset required for SHA-3. The missing pieces are:
 *
 *  - state sizes other than 1600 (general Keccak supports 25*2^i for
 *    i=1,2,4,8,16,32,64)
 *
 *  - input lengths that are not an integer number of bytes (general
 *    Keccak can hash any string of bits)
 *
 *  - squeezing of more than one block from the sponge (general Keccak
 *    can be used as a stream cipher as well as a hash function, and
 *    more).
 */

#include <string.h>
#include <assert.h>

typedef unsigned int uint32;

/* Add special cases for compilers with native 64-bit ints here */
#if defined(__GNUC__)
#define HAS_REAL_UINT64
typedef unsigned long long uint64;
#endif

#ifndef HAS_REAL_UINT64
typedef struct {
    uint32 hi, lo;
} uint64;
#endif

/*
 * Arithmetic implementations. Note that AND, XOR and NOT can
 * overlap destination with one source, but the others can't.
 */
#ifdef HAS_REAL_UINT64
#define rol(r,x,y) ( r = ((x) << (y)) | (((x)) >> (64-(y))) )
#define and(r,x,y) ( r = (x) & (y) )
#define xor(r,x,y) ( r = (x) ^ (y) )
#define not(r,x) ( r = ~(x) )
#define INIT(h,l) ( ((uint64)h << 32) | ((uint64)l) )
#define BUILD(r,h,l) ( r = ((uint64)h << 32) | ((uint64)l) )
#define EXTRACT(h,l,r) ( h = (uint32)(r >> 32), l = (uint32)(r & 0xFFFFFFFF) )
#else
#define rolB(r,x,y) ( r.lo = (x.lo >> (64-(y))) | (x.hi << ((y)-32)), \
                      r.hi = (x.hi >> (64-(y))) | (x.lo << ((y)-32)) )
#define rolL(r,x,y) ( r.lo = (x.hi >> (32-(y))) | (x.lo << (y)), \
                      r.hi = (x.lo >> (32-(y))) | (x.hi << (y)) )
#define rol(r,x,y) ( y < 32 ? rolL(r,x,y) : rolB(r,x,y) )
#define and(r,x,y) ( r.lo = x.lo & y.lo, r.hi = x.hi & y.hi )
#define xor(r,x,y) ( r.lo = x.lo ^ y.lo, r.hi = x.hi ^ y.hi )
#define not(r,x) ( r.lo = ~x.lo, r.hi = ~x.hi )
#define INIT(h,l) { h, l }
#define BUILD(r,h,l) ( r.hi = h, r.lo = l )
#define EXTRACT(h,l,r) ( h = r.hi, l = r.lo )
#endif

/*
 * General Keccak is defined such that its state is a 5x5 array of
 * words which can be any power-of-2 size from 1 up to 64. SHA-3 fixes
 * on 64, and so do we.
 */
typedef uint64 keccak_core_state[5][5];
#define KECCAK_ROUNDS 24               /* would differ for other word sizes */

/*
 * Core Keccak function: just squodge the state around internally,
 * without adding or extracting any data from it.
 */
static void keccak_f(keccak_core_state A)
{
    int i, x, y;

    static const uint64 RC[24] = {
        /*
         * Keccak round constants, generated via the LFSR specified in
         * the Keccak reference by the following piece of Python:

rc = [1]
while len(rc) < 7*24:
    k = rc[-1]
    k *= 2
    if k >= 0x100:
        k = k ^ 0x171
    rc.append(k)

rc = [x & 1 for x in rc]
for i in range(24):
    RC = 0
    for j in range(7):
        RC = RC | (rc[7*i+j] << ((1 << j)-1))
    print "        INIT(0x%08x, 0x%08x)," % (RC>>32, RC & 0xFFFFFFFF)

         */

        INIT(0x00000000, 0x00000001),
        INIT(0x00000000, 0x00008082),
        INIT(0x80000000, 0x0000808a),
        INIT(0x80000000, 0x80008000),
        INIT(0x00000000, 0x0000808b),
        INIT(0x00000000, 0x80000001),
        INIT(0x80000000, 0x80008081),
        INIT(0x80000000, 0x00008009),
        INIT(0x00000000, 0x0000008a),
        INIT(0x00000000, 0x00000088),
        INIT(0x00000000, 0x80008009),
        INIT(0x00000000, 0x8000000a),
        INIT(0x00000000, 0x8000808b),
        INIT(0x80000000, 0x0000008b),
        INIT(0x80000000, 0x00008089),
        INIT(0x80000000, 0x00008003),
        INIT(0x80000000, 0x00008002),
        INIT(0x80000000, 0x00000080),
        INIT(0x00000000, 0x0000800a),
        INIT(0x80000000, 0x8000000a),
        INIT(0x80000000, 0x80008081),
        INIT(0x80000000, 0x00008080),
        INIT(0x00000000, 0x80000001),
        INIT(0x80000000, 0x80008008),
    };

    static const int r[5][5] = {
        /*
         * Keccak per-element rotation counts, generated from the
         * matrix formula in the Keccak reference by the following
         * piece of Python:

ts = [[None]*5 for k in range(5)]
ts[0][0] = -1
v = (1,0)
for i in range(24):
    ts[v[0]][v[1]] = i
    v = (v[1], (2*v[0]+3*v[1]) % 5)

for x in range(5):
    sep = "        {"
    s = ""
    for y in range(5):
        t = ts[x][y]
        s = s + sep + "%2d" % (((t+1)*(t+2)/2) % 64)
        sep = ", "
    print s + "},"

         */
        { 0, 36,  3, 41, 18},
        { 1, 44, 10, 45,  2},
        {62,  6, 43, 15, 61},
        {28, 55, 25, 21, 56},
        {27, 20, 39,  8, 14},
    };

    for (i = 0; i < KECCAK_ROUNDS; i++) {
        uint64 C[5], D[5], B[5][5];

        /* theta step */
        for (x = 0; x < 5; x++) {
            C[x] = A[x][0];
            for (y = 1; y < 5; y++)
                xor(C[x], C[x], A[x][y]);
        }
        for (x = 0; x < 5; x++) {
            rol(D[x], C[(x+1) % 5], 1);
            xor(D[x], D[x], C[(x+4) % 5]);
        }
        for (x = 0; x < 5; x++) {
            for (y = 0; y < 5; y++) {
                xor(A[x][y], A[x][y], D[x]);
            }
        }

        /* rho and pi steps */
        for (x = 0; x < 5; x++) {
            for (y = 0; y < 5; y++) {
                rol(B[y][(2*x+3*y) % 5], A[x][y], r[x][y]);
            }
        }

        /* chi step */
        for (x = 0; x < 5; x++) {
            for (y = 0; y < 5; y++) {
                uint64 tmp;
                not(tmp, B[(x+1)%5][y]);
                and(tmp, tmp, B[(x+2)%5][y]);
                xor(A[x][y], B[x][y], tmp);
            }
        }

        /* iota step */
        xor(A[0][0], A[0][0], RC[i]);
    }
}

typedef struct {
    keccak_core_state A;
    unsigned char bytes[25*8];
    int bytes_got, bytes_wanted, hash_bytes;
} keccak_state;

/*
 * Keccak accumulation function: given a piece of message, add it to
 * the hash.
 */
void keccak_accumulate(keccak_state *s, void *vdata, int len)
{
    unsigned char *data = (unsigned char *)vdata;

    while (len >= s->bytes_wanted - s->bytes_got) {
        int x, y, b, n;

        b = s->bytes_wanted - s->bytes_got;
        memcpy(s->bytes + s->bytes_got, data, b);
        len -= b;
        data += b;

        n = 0;
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                uint64 u;
                uint32 uh, ul;

                if (n*8 >= s->bytes_wanted)
                    break;

                /*
                 * Keccak is little-endian
                 */
                ul = ( ( ((uint32)s->bytes[n*8+0]) <<  0 ) |
                       ( ((uint32)s->bytes[n*8+1]) <<  8 ) |
                       ( ((uint32)s->bytes[n*8+2]) << 16 ) |
                       ( ((uint32)s->bytes[n*8+3]) << 24 ) );
                uh = ( ( ((uint32)s->bytes[n*8+4]) <<  0 ) |
                       ( ((uint32)s->bytes[n*8+5]) <<  8 ) |
                       ( ((uint32)s->bytes[n*8+6]) << 16 ) |
                       ( ((uint32)s->bytes[n*8+7]) << 24 ) );
                n++;

                BUILD(u, uh, ul);
                xor(s->A[x][y], s->A[x][y], u);
            }
        }
        keccak_f(s->A);

        s->bytes_got = 0;
    }

    memcpy(s->bytes + s->bytes_got, data, len);
    s->bytes_got += len;
}

/*
 * Keccak output function.
 */
void keccak_output(keccak_state *s, unsigned char *output)
{
    int x, y, n;

    /*
     * Add message padding.
     */
    {
        unsigned char data[25*8];
        int len = s->bytes_wanted - s->bytes_got;
        if (len == 0)
            len = s->bytes_wanted;
        memset(data, 0, len);
        data[0] |= 1;
        data[len-1] |= 0x80;
        keccak_accumulate(s, data, len);
    }

    n = 0;
    for (y = 0; y < 5; y++) {
        for (x = 0; x < 5; x++) {
            uint32 uh, ul;

            if (n >= s->hash_bytes)
                break;

            EXTRACT(uh, ul, s->A[x][y]);

            /*
             * Keccak is little-endian
             */
            output[n++] = ul; ul >>= 8;
            output[n++] = ul; ul >>= 8;
            output[n++] = ul; ul >>= 8;
            output[n++] = ul; ul >>= 8;
            output[n++] = uh; uh >>= 8;
            output[n++] = uh; uh >>= 8;
            output[n++] = uh; uh >>= 8;
            output[n++] = uh; uh >>= 8;
        }
    }
}

void keccak_init(keccak_state *s, int hashlen)
{
    int x, y;

    assert(hashlen==224 || hashlen==256 || hashlen==384 || hashlen==512);

    s->hash_bytes = hashlen / 8;
    s->bytes_wanted = (8 * 25) - 2*s->hash_bytes;
    s->bytes_got = 0;

    for (y = 0; y < 5; y++) {
        for (x = 0; x < 5; x++) {
            BUILD(s->A[x][y], 0, 0);
        }
    }
}
