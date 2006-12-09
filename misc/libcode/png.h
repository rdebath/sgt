/*
 * Header file for a PNG decoder. (And perhaps an encoder too one
 * day; the png_pixel and png structures would work just as well in
 * the reverse direction.)
 */

#ifndef PNGIN_PNGIN_H
#define PNGIN_PNGIN_H

/*
 * We need deflate.h at the header level rather than just the
 * source level, because we include Deflate's error code list into
 * ours.
 */
#include "deflate.h"

typedef struct png_pixel png_pixel;
typedef struct png png;
typedef struct png_decoder png_decoder;

struct png_pixel {
    unsigned short r, g, b, a;
};

struct png {
    /*
     * Width and height of the image, in pixels.
     */
    int width, height;

    /*
     * The actual array of image pixels. This array has size
     * (width*height), of course, and is indexed in normal reading
     * order: rows are listed from top to bottom, and within each
     * row pixels are listed from left to right. To retrieve the
     * pixel at coordinates (x,y), where x is defined with zero on
     * the left and (width-1) on the right and y is defined with
     * zero at the top and (height-1) at the bottom, you access
     * pixels[y*width+x].
     */
    png_pixel *pixels;

    /*
     * The image's preferred background colour, if any. This might
     * be relevant if you're displaying an image with a non-trivial
     * alpha channel and you don't already know what background you
     * want to display it against.
     * 
     * If a background colour is specified, background.a will be
     * 0xFFFF and background.{r,g,b} will be the colour. If no
     * background colour was specified in the image file,
     * background.a will be 0.
     */
    png_pixel background;

    /*
     * The intended aspect ratio of the image, given as two
     * non-zero integers representing the relative width and height
     * of a pixel. I.e. if (waspect, haspect) is (2, 1) it means a
     * pixel is meant to be twice as wide as it is high.
     *
     * These two values are both guaranteed to be non-zero.
     */
    unsigned long waspect, haspect;
};

/*
 * The simple common case. Given a filename, open that file, try to
 * interpret its contents as a PNG image, and return a pointer to a
 * structure containing the decoded image data. If the returned
 * pointer is NULL, then `error' will have been filled in with an
 * error code from the enumeration defined below.
 * 
 * If you don't care about the error code, you can pass `error' as
 * NULL.
 */
png *png_decode_file(const char *filename, int *error);

/*
 * Free a png structure and its internal pixel array. (You can
 * safely do this yourself if you like with two calls to free(),
 * but it's easier to let us do it for you :-)
 */
void png_free(png *p);

/*
 * If you're getting your PNG data from some source other than a
 * disk file, here's an alternative interface to the decoder, which
 * supports passing in the data in any size of chunks from a source
 * of your choice.
 * 
 * The sequence is:
 *  - use png_decoder_new() to allocate a decoder context
 *  - pass data to that context using one or more calls to
 *    png_decoder_data()
 *  - indicate end-of-data by calling png_decoder_data() with
 *    length zero (in which case `data' may be NULL)
 *  - call png_decoder_image() to turn the decoder context into an
 *    actual image structure.
 * 
 * If there is a decoding error, png_decoder_data() will return a
 * non-zero error code, at which point you should stop calling it
 * and the only thing you can usefully do with the decoder context
 * is to clear it up using png_decoder_free().
 * 
 * Note that png_decoder_image() also frees the png_decoder
 * structure.
 */
png_decoder *png_decoder_new(void);
int png_decoder_data(png_decoder *pd, const void *data, int len);
png *png_decoder_image(png_decoder *pd);
void png_decoder_free(png_decoder *pd);

/*
 * Enumeration of error codes. The strange macro is so that I can
 * define description arrays in the accompanying source.
 * 
 * This is also a bit fiddly because I incorporate the Deflate
 * error list into the PNG one, so in fact the macro doesn't even
 * cover the whole lot.
 */
#define PNG_ERRORLIST(A) \
    A(PNG_NO_ERR, "success"), \
    A(PNG_ERR_OUT_OF_MEMORY, "out of memory"), \
    A(PNG_ERR_OPENING_FILE, "error opening file"), \
    A(PNG_ERR_READING_FILE, "error reading file"), \
    A(PNG_ERR_HEADER_INCORRECT, "PNG header is incorrect"), \
    A(PNG_ERR_CHUNK_CRC_MISMATCH, "chunk failed CRC check"), \
    A(PNG_ERR_FIRST_CHUNK_NOT_IHDR, "first chunk is not IHDR"), \
    A(PNG_ERR_IHDR_OUT_OF_ORDER, "IHDR chunk appears out of order"), \
    A(PNG_ERR_IHDR_WRONG_LENGTH, "IHDR chunk has unexpected length"), \
    A(PNG_ERR_UNSUPPORTED_COMP_METHOD, "unsupported compression method"), \
    A(PNG_ERR_UNSUPPORTED_FILTER_METHOD, "unsupported filter method"), \
    A(PNG_ERR_IMAGE_DIMENSION_ZERO, "image has zero size"), \
    A(PNG_ERR_IMAGE_TOO_BIG_TO_HANDLE, "image is too big to handle"), \
    A(PNG_ERR_UNSUPPORTED_BIT_DEPTH, "unsupported bit depth"), \
    A(PNG_ERR_UNSUPPORTED_COLOUR_TYPE, "unsupported colour type"), \
    A(PNG_ERR_BIT_DEPTH_INVALID_FOR_COLOUR_TYPE, "bit depth and colour type mismatch"), \
    A(PNG_ERR_UNSUPPORTED_INTERLACE_TYPE, "unsupported interlace type"), \
    A(PNG_ERR_PLTE_OUT_OF_ORDER, "PLTE chunk appears out of order"), \
    A(PNG_ERR_PLTE_LEN_INVALID, "PLTE chunk has invalid length"), \
    A(PNG_ERR_MANDATORY_PLTE_MISSING, "PLTE chunk is missing in an indexed-colour image"), \
    A(PNG_ERR_tRNS_OUT_OF_ORDER, "tRNS chunk appears out of order"), \
    A(PNG_ERR_tRNS_LEN_INVALID, "tRNS chunk has invalid length"), \
    A(PNG_ERR_tRNS_NOT_PERMITTED, "tRNS chunk not permitted for this colour type"), \
    A(PNG_ERR_bKGD_OUT_OF_ORDER, "bKGD chunk appears out of order"), \
    A(PNG_ERR_bKGD_LEN_INVALID, "bKGD chunk has invalid length"), \
    A(PNG_ERR_pHYs_OUT_OF_ORDER, "pHYs chunk appears out of order"), \
    A(PNG_ERR_pHYs_LEN_INVALID, "pHYs chunk has invalid length"), \
    A(PNG_ERR_IDAT_OUT_OF_ORDER, "IDAT chunk appears out of order"), \
    A(PNG_ERR_NOT_ENOUGH_IMAGE_DATA, "not enough image data in IDAT chunks"), \
    A(PNG_ERR_TOO_MUCH_IMAGE_DATA, "too much image data in IDAT chunks"), \
    A(PNG_ERR_ILLEGAL_FILTER_TYPE, "illegal filter type code in image data"), \
    A(PNG_ERR_COLOUR_INDEX_OUTSIDE_PALETTE, "out-of-range colour index in image data"), \
    A(PNG_ERR_UNSUPPORTED_CRITICAL_CHUNK, "unrecognised critical chunk encountered"), \
    A(PNG_ERR_IEND_OUT_OF_ORDER, "IEND chunk appears out of order"), \
    A(PNG_ERR_IEND_CONTAINS_DATA, "IEND chunk contains data"), \
    A(PNG_ERR_DATA_AFTER_IEND, "data appears after IEND chunk"), \
    A(PNG_ERR_TRUNCATED, "file is truncated")

#define PNG_ENUM_DEF(x,y) x
enum {
    PNG_ERRORLIST(PNG_ENUM_DEF),
    PNG_ERR_DEFLATE_ERROR_BASE,
    PNG_ERR_DEFLATE_ERROR_LAST = PNG_ERR_DEFLATE_ERROR_BASE +
	DEFLATE_NUM_ERRORS - 1,
    PNG_NUM_ERRORS
};
#undef PNG_ENUM_DEF

/*
 * Arrays mapping the above error codes to, respectively, a text
 * error string and a textual representation of the symbolic error
 * code.
 */
extern const char *const png_error_msg[PNG_NUM_ERRORS];
extern const char *const png_error_sym[PNG_NUM_ERRORS];

#endif /* PNGIN_PNGIN_H */
