/*
 * Header file for an LZ77 implementation.
 */

#ifndef LZ77_H
#define LZ77_H

struct LZ77InternalContext;
struct LZ77Context {
    struct LZ77InternalContext *ictx;
    void *userdata;
    void (*literal)(struct LZ77Context *ctx, unsigned char c);
    void (*match)(struct LZ77Context *ctx, int distance, int len);
};

/*
 * Initialise the private fields of an LZ77Context. It's up to the
 * user to initialise the public fields.
 */
int lz77_init(struct LZ77Context *ctx);

/*
 * Supply data to be compressed. Will update the private fields of
 * the LZ77Context, and will call literal() and match() to output.
 */
void lz77_compress(struct LZ77Context *ctx, unsigned char *data, int len);

#endif
