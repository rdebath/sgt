/*
 * Code implementing the `data'-type font so popular in the 1980s,
 * which I (perhaps ill-advisedly) used in Rocket Attack.
 */

#include <stdio.h>

#include "data.h"

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

typedef unsigned short column;

static column space[] = { 0, 0 };

static column digit0[] = { 0xfe, 0x101, 0x101, 0x101, 0x1f1, 0xfe };
static column digit1[] = { 0x1f, 0x1ff };
static column digit2[] = { 0x8e, 0x11f, 0x111, 0x111, 0x111, 0xe1 };
static column digit3[] = { 0x82, 0x111, 0x111, 0x111, 0x11f, 0xfe };
static column digit4[] = { 0x1f8, 0x4, 0x4, 0x7, 0x1f, 0x4 };
static column digit5[] = { 0xe2, 0x111, 0x111, 0x111, 0x11f, 0x10e };
static column digit6[] = { 0xfe, 0x111, 0x111, 0x111, 0x11f, 0x8e };
static column digit7[] = { 0x100, 0x100, 0x100, 0x100, 0x11f, 0xff };
static column digit8[] = { 0xfe, 0x1f1, 0x111, 0x111, 0x111, 0xfe };
static column digit9[] = { 0xe0, 0x110, 0x110, 0x110, 0x11f, 0xff };

static column openparen[] = { 0xfe, 0x101 };
static column closeparen[] = { 0x101, 0xfe };
static column colon[] = { 0x28 };
static column equals[] = { 0x28, 0x28, 0x28 };
static column minus[] = { 0x10, 0x10, 0x10 };

static column letterA[] = { 0x1f, 0xff, 0x110, 0x110, 0xf0, 0x1f };
static column letterB[] = { 0xfe, 0x11f, 0x111, 0x111, 0xf1, 0x0e };
static column letterC[] = { 0xfe, 0x11f, 0x101, 0x101, 0x101 };
static column letterD[] = { 0xfe, 0x11f, 0x101, 0x101, 0x101, 0xfe };
static column letterE[] = { 0xfe, 0x11f, 0x111, 0x111, 0x111, 0x101 };
static column letterF[] = { 0xff, 0x11f, 0x110, 0x110, 0x110, 0x100 };
static column letterG[] = { 0xfe, 0x101, 0x101, 0x111, 0x11f, 0xde };
static column letterH[] = { 0x1ff, 0x1f, 0x10, 0x10, 0x10, 0x1ff };
static column letterI[] = { 0x1ff, 0x0f };
static column letterJ[] = { 0x01, 0x01, 0x01, 0x1f, 0x1fe };
static column letterK[] = { 0x1ff, 0x1f, 0x10, 0x1f0, 0x10, 0x0f };
static column letterL[] = { 0x1fe, 0x1f, 0x01, 0x01, 0x01, 0x01 };
static column letterM[] = { 0xff, 0x10f, 0x100, 0xff, 0x100, 0x100, 0xff };
static column letterN[] = { 0xff, 0x10f, 0x100, 0x100, 0x100, 0xff };
static column letterO[] = { 0xfe, 0x101, 0x101, 0x101, 0x1c1, 0xfe };
static column letterP[] = { 0x1ff, 0x11f, 0x110, 0x110, 0x110, 0xe0 };
static column letterQ[] = { 0xfe, 0x101, 0x101, 0x101, 0x10f, 0xff };
static column letterR[] = { 0xff, 0x11f, 0x110, 0x110, 0xf0, 0x0f };
static column letterS[] = { 0xe6, 0x111, 0x111, 0x111, 0x11f, 0x8e };
static column letterT[] = { 0x100, 0x100, 0x100, 0x1ff, 0x11f, 0x100, 0x100 };
static column letterU[] = { 0x1fe, 0x01, 0x01, 0x01, 0x1f, 0x1fe };
static column letterV[] = { 0x1f0, 0x0e, 0x01, 0x01, 0x1fe, 0x1f0 };
static column letterW[] = { 0x1fe, 0x01, 0x01, 0x1fe, 0x01, 0x1f, 0x1fe };
static column letterX[] = { 0x1c7, 0x28, 0x10, 0x10, 0x2f, 0x1c7 };
static column letterY[] = { 0x1e0, 0x10, 0x1f, 0x1f, 0x10, 0x1e0 };
static column letterZ[] = { 0x10e, 0x11f, 0x111, 0x121, 0x121, 0xc1 };

#define C(x) { x, lenof(x) }
#define CNONE { NULL, 0 }

static struct { column *data; int width; } data_font[] = {
    C(space),			       /* space */
    CNONE, 			       /* exclam */
    CNONE, 			       /* quotedbl */
    CNONE, 			       /* numbersign */
    CNONE, 			       /* dollar */
    CNONE, 			       /* percent */
    CNONE, 			       /* ampersand */
    CNONE, 			       /* quoteright */
    C(openparen),		       /* parenleft */
    C(closeparen),		       /* parenright */
    CNONE, 			       /* asterisk */
    CNONE, 			       /* plus */
    CNONE, 			       /* comma */
    C(minus), 			       /* hyphen */
    CNONE, 			       /* period */
    CNONE, 			       /* slash */
    C(digit0),			       /* zero */
    C(digit1),			       /* one */
    C(digit2),			       /* two */
    C(digit3),			       /* three */
    C(digit4),			       /* four */
    C(digit5),			       /* five */
    C(digit6),			       /* six */
    C(digit7),			       /* seven */
    C(digit8),			       /* eight */
    C(digit9),			       /* nine */
    C(colon),			       /* colon */
    CNONE, 			       /* semicolon */
    CNONE, 			       /* less */
    C(equals), 			       /* equal */
    CNONE, 			       /* greater */
    CNONE, 			       /* question */
    CNONE, 			       /* at */
    C(letterA),			       /* A */
    C(letterB),			       /* B */
    C(letterC),			       /* C */
    C(letterD),			       /* D */
    C(letterE),			       /* E */
    C(letterF),			       /* F */
    C(letterG),			       /* G */
    C(letterH),			       /* H */
    C(letterI),			       /* I */
    C(letterJ),			       /* J */
    C(letterK),			       /* K */
    C(letterL),			       /* L */
    C(letterM),			       /* M */
    C(letterN),			       /* N */
    C(letterO),			       /* O */
    C(letterP),			       /* P */
    C(letterQ),			       /* Q */
    C(letterR),			       /* R */
    C(letterS),			       /* S */
    C(letterT),			       /* T */
    C(letterU),			       /* U */
    C(letterV),			       /* V */
    C(letterW),			       /* W */
    C(letterX),			       /* X */
    C(letterY),			       /* Y */
    C(letterZ),			       /* Z */
    CNONE, 			       /* bracketleft */
    CNONE, 			       /* backslash */
    CNONE, 			       /* bracketright */
    CNONE, 			       /* asciicircum */
    CNONE, 			       /* underscore */
    CNONE, 			       /* quoteleft */
    C(letterA),			       /* a */
    C(letterB),			       /* b */
    C(letterC),			       /* c */
    C(letterD),			       /* d */
    C(letterE),			       /* e */
    C(letterF),			       /* f */
    C(letterG),			       /* g */
    C(letterH),			       /* h */
    C(letterI),			       /* i */
    C(letterJ),			       /* j */
    C(letterK),			       /* k */
    C(letterL),			       /* l */
    C(letterM),			       /* m */
    C(letterN),			       /* n */
    C(letterO),			       /* o */
    C(letterP),			       /* p */
    C(letterQ),			       /* q */
    C(letterR),			       /* r */
    C(letterS),			       /* s */
    C(letterT),			       /* t */
    C(letterU),			       /* u */
    C(letterV),			       /* v */
    C(letterW),			       /* w */
    C(letterX),			       /* x */
    C(letterY),			       /* y */
    C(letterZ),			       /* z */
    CNONE, 			       /* braceleft */
    CNONE, 			       /* bar */
    CNONE, 			       /* braceright */
    CNONE, 			       /* asciitilde */
};

int data_width(char *text)
{
    int ret = 0;
    while (*text) {
	int c = (unsigned char) *text++;
	if (c >= 0x20 && c < 0x20 + lenof(data_font) &&
	    data_font[c-0x20].width > 0)
	    ret += 1 + data_font[c-0x20].width;
    }
    return ret > 0 ? ret-1 : ret;
}

int data_text(char *text, int x, int y,
	      void (*plot)(void *ctx, int x, int y, int on), void *ctx)
{
    int ix, iy;

    while (*text) {
	int c = (unsigned char) *text++;
	if (c >= 0x20 && c < 0x20 + lenof(data_font) &&
	    data_font[c-0x20].width > 0) {
	    column *data = data_font[c-0x20].data;
	    int width = data_font[c-0x20].width;
	    for (ix = 0; ix < width; ix++) {
		for (iy = 0; iy <= 8; iy++) {
		    plot(ctx, x + ix, y + iy, !!(data[ix] & (256>>iy)));
		}
	    }
	    x += width;
	    for (iy = 0; iy <= 8; iy++)
		plot(ctx, x, y + iy, 0);
	    x++;
	}
    }
}
