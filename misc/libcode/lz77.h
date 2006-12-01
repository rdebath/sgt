/*
 * Header file for an LZ77 implementation.
 */

#ifndef LZ77_H
#define LZ77_H

typedef struct LZ77 LZ77;

/*
 * Set up an LZ77 context. The user supplies two function pointer
 * parameters: `literal', called when the compressor outputs a
 * literal byte, and `match', called when the compressor outputs a
 * match. Each of these functions, in addition to parameters
 * describing the data being output, also receives a void * context
 * parameter which is passed in to lz77_new().
 */
LZ77 *lz77_new(void (*literal)(void *ctx, unsigned char c),
	       void (*match)(void *ctx, int distance, int len),
	       void *ctx);

/*
 * Supply data to be compressed. This produces output by calling
 * the literal() and match() functions passed to lz77_new().
 * 
 * This function buffers data internally, so in any given call it
 * will not necessarily output literals and matches which cover all
 * the data passed in to it. To force a buffer flush, use
 * lz77_flush().
 */
void lz77_compress(LZ77 *lz, const void *data, int len);

/*
 * Force the LZ77 compressor to output literals and matches which
 * cover all the data it has received as input until now.
 *
 * After calling this function, the LZ77 stream is still active:
 * future input data will still be matched against data which was
 * provided before the flush. Therefore, this function can be used
 * to produce guaranteed record boundaries in a single data stream,
 * but can _not_ be used to reuse the same LZ77 context for two
 * entirely separate data streams (otherwise the second one might
 * make backward references beyond the start of its data).
 * 
 * Calling this in the middle of a data stream can decrease
 * compression quality. If maximum compression is the only concern,
 * do not call this function until the very end of the stream.
 */
void lz77_flush(LZ77 *lz);

/*
 * Free an LZ77 context when it's finished with. This does not
 * automatically flush buffered data; call lz77_flush explicitly to
 * do that.
 */
void lz77_free(LZ77 *lz);

#endif
