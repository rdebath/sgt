/*
 * My personal SMTP-time mail filter.
 * 
 * This source file should be linked with viruswatch/userfilter.c,
 * viruswatch/scanner.c (and its dependency viruswatch/inflate.c),
 * and libcharset. My current compile line is
 * 
 * gcc -Wall -O0 -g -I../viruswatch -I../charset -o mailfilter mailfilter.c ../viruswatch/userfilter.c ../viruswatch/scanner.c ../viruswatch/inflate.c ../charset/libcharset.a
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "scanner.h"
#include "charset.h"

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

/*
 * Return true iff the string a starts with the string b.
 */
int strprefix(const char *a, const char *b)
{
    return !strncmp(a, b, strlen(b));
}
int strcaseprefix(const char *a, const char *b)
{
    return !strncasecmp(a, b, strlen(b));
}

/*
 * Return true iff the string b appears in memory within string a,
 * ending at point e.
 */
int strsuffix(const char *a, const char *e, const char *b)
{
    int blen = strlen(b);
    return e - a >= blen && !strncmp(e - blen, b, blen);
}
int strcasesuffix(const char *a, const char *e, const char *b)
{
    int blen = strlen(b);
    return e - a >= blen && !strncasecmp(e - blen, b, blen);
}

/* ----------------------------------------------------------------------
 * Utility functions.
 */

/*
 * Compare a string-with-length to a zero-terminated string.
 */
int strlenzcmp(const char *s1, int len1, const char *s2)
{
    while (len1 > 0 && *s2) {
	if (*s1 != *s2)
	    return (int)((unsigned char)*s1) - (int)((unsigned char)*s2);
	s1++;
	len1--;
	s2++;
    }

    if (len1 > 0)
	return +1;
    else if (*s2)
	return -1;
    else
	return 0;
}

#if 0
/* ----------------------------------------------------------------------
 * Very simple subfilter to detect the Japanese spams I seem to be
 * getting in ridiculous quantity at the moment.
 * 
 * Disabled, because the alphabet-identifying strand of the
 * general-purpose scanner filter _also_ detects these spams and is
 * less fragile. However, there's no point throwing this away; I
 * might still need it if I have to disable the alphabet stuff at
 * any point.
 */
const char *japanese_filter(int len, const char *data)
{
    static int state = 0;

    while (len--) {
	int c = *data++;

	switch (state) {
	  case 0:
	    if (c == '\n')
		state = 1;
	    break;
	  case 1:
	    if (c == '\n')
		state = 2;
	    else
		state = 0;
	    break;
	  case 2:
	    if (c == '\n')
		/* stay in 2 */;
	    else if (c == '\x1B')
		state = 3;
	    else
		state = -1;
	    break;
	  case 3:
	    if (c == '$')
		state = 4;
	    else
		state = -1;
	    break;
	  case 4:
	    if (c == 'B')
		state = 5;
	    else
		state = -1;
	    break;
	  case 5:
	    if (c == '(')
		state = 6;
	    else
		state = -1;
	    break;
	  case 6:
	    if (c == ',')
		return "This looks like a spam I've been "
		"seeing a lot recently.";
	    else
		state = -1;
	    break;
	}
    }

    return NULL;
}
#endif

/* ----------------------------------------------------------------------
 * Subfilter based on scanner.c:
 *
 *  - rejects animated GIFs (I only ever see these in pump-and-dump
 *    spam)
 *
 *  - analyses the character set of the text parts of incoming
 *    messages, and rejects anything which it can identify as being
 *    primarily in a language I don't speak. This covers a lot of
 *    spam, and also means that anyone mailing me in good faith in
 *    a foreign character set will at least get _some_ indication
 *    that I'm unlikely to read it, which is better than they'll
 *    get if I bin it due to being unable to tell it from spam.
 */

static int greetingspam = 0;

/* --------------------------------------------------
 * Scanner submodule to detect animated GIFs.
 */

typedef struct agif {
    char hdr[10];
    int state;
    int skiplen;
    int delaytime;
    int imagecount;
    int reported;
} agif;

enum {
    SC_AGIF = SC_USER
};

static void scanner_init_agif(scanner *s, stream *st, void *vctx)
{
    agif *ctx = (agif *)vctx;

    ctx->state = 0;
    ctx->skiplen = 0;
    ctx->delaytime = 0;
    ctx->imagecount = 0;
    ctx->reported = 0;
}

static void scanner_feed_agif(scanner *s, stream *st, void *vctx,
			      const char *data, int len)
{
    agif *ctx = (agif *)vctx;

    while (len--) {
	char c = *data++;
	unsigned char uc = c;	       /* for ctype functions */

	if (ctx->state < 0)
	    return;

	if (ctx->skiplen > 0) {
	    ctx->skiplen--;
	    continue;
	}

	if (ctx->state < 6) {
	    /*
	     * States 0-5: we're still matching the identifying
	     * header.
	     */
	    if (c == "GIF87a"[ctx->state] || c == "GIF89a"[ctx->state])
		ctx->state++;
	    else
		ctx->state = -1;
	} else if (ctx->state < 13) {
	    /*
	     * States 6-12: accumulate seven bytes of main GIF
	     * header.
	     */
	    ctx->hdr[ctx->state - 6] = c;
	    ctx->state++;
	    if (ctx->state == 13) {
		/*
		 * We have the header. Check for a colour table and
		 * skip it.
		 */
		int flags = (unsigned char)ctx->hdr[4];
		if (flags & 0x80)
		    ctx->skiplen = 3 * (1 << (1 + (flags & 7)));
	    }
	} else if (ctx->state == 13) {
	    /*
	     * State 13: in the main GIF stream, about to see an
	     * introducer byte of some kind.
	     */
	    if (c == 0x21)
		ctx->state = 14;
	    else if (c == 0x2C) {
		ctx->imagecount++;
		ctx->state = 100;
	    } else
		ctx->state = -1;       /* end of useful GIF data */
	} else if (ctx->state == 14) {
	    /*
	     * State 14: we've seen an extension introducer. Now
	     * we're looking at the label byte which says what kind
	     * of extension it is.
	     */
	    if (uc == 0xF9)
		ctx->state = 50;       /* graphics introducer */
	    else
		ctx->state = 15;       /* unknown extension type */
	} else if (ctx->state == 15) {
	    /*
	     * State 15: we're about to encounter a sub-block of an
	     * extension.
	     */
	    if (uc == 0)
		ctx->state = 13;       /* end of extension; back to top */
	    else
		ctx->skiplen = uc;     /* skip one sub-block */
	} else if (ctx->state == 50) {
	    /*
	     * State 50: we're just about to see the first
	     * sub-block of a graphic control extension. This
	     * should have length 4, and if it doesn't then we
	     * don't know what it is.
	     */
	    if (uc == 4)
		ctx->state = 51;
	    else {
		ctx->skiplen = uc;
		ctx->state = 15;       /* treat as unknown extension type */
	    }
	} else if (ctx->state >= 51 && ctx->state <= 54) {
	    /*
	     * States 51-54: we're in the middle of the length-4
	     * sub-block of a graphic control extension. The delay
	     * time field is the middle two of those four bytes; if
	     * either is non-zero, we set the delaytime flag.
	     */
	    if ((ctx->state == 52 || ctx->state == 53) && uc != 0)
		ctx->delaytime = 1;
	    ctx->state++;
	    if (ctx->state == 55)
		ctx->state = 15;       /* now skip rest of extension */
	} else if (ctx->state >= 100 && ctx->state <= 108) {
	    /*
	     * States 100-108: we've just seen the introducer for
	     * an image section, and now we're collecting its
	     * header.
	     */
	    ctx->hdr[ctx->state - 100] = c;
	    ctx->state++;
	    if (ctx->state == 109) {
		/*
		 * We have the header. Check for a colour table and
		 * skip it.
		 */
		int flags = (unsigned char)ctx->hdr[8];
		if (flags & 0x80)
		    ctx->skiplen = 3 * (1 << (1 + (flags & 7)));
	    }
	} else if (ctx->state == 109) {
	    /*
	     * State 109: we've passed the header and the colour
	     * table for an image. This byte is the initial code
	     * length, which we ignore; then the rest of the image
	     * is a stream of sub-blocks.
	     */
	    ctx->state = 15;
	}
    }

    if (ctx->delaytime != 0 && ctx->imagecount > 1) {
	/*
	 * This GIF contains more than one image, and at least one
	 * graphic control extension specifying a non-zero delay.
	 * It is therefore animated.
	 */
	if (!ctx->reported)
	    scanner_report(s, st, SC_AGIF);
	ctx->reported = 1;
    }
}

/* --------------------------------------------------
 * Scanner submodule to analyse the alphabet distribution of a
 * message's text, so I can reject things that are obviously not in
 * English.
 */

/* List of alphabets. Rather than going through the whole of Unicode, I'm
 * currently only listing alphabets in which I can remember receiving
 * unwanted mail. */
#define ALPH_ENUM(X) \
    X(LATIN), \
    X(GREEK), \
    X(CYRILLIC), \
    X(THAI), \
    X(ARABIC), \
    X(HEBREW), \
    X(HANGUL), \
    X(CJK)

#define ALPH_ENUM_SYM(x) ALPH_ ## x
enum { ALPH_ENUM(ALPH_ENUM_SYM), ALPH_COUNT };

struct range {
    int low, high, alph;
};

const struct range ranges[] = {
    {0x41, 0x5A, ALPH_LATIN},	       /* A-Z */
    {0x61, 0x7A, ALPH_LATIN},	       /* a-z */
    {0x374, 0x3F6, ALPH_GREEK},
    {0x400, 0x50F, ALPH_CYRILLIC},
    {0x591, 0x5F4, ALPH_HEBREW},
    {0x60C, 0x6FE, ALPH_ARABIC},
    {0xE01, 0xE5B, ALPH_THAI},
    {0x1100, 0x11FF, ALPH_HANGUL},
    {0x1F00, 0x1FFF, ALPH_GREEK},
    {0x2E80, 0x2FFF, ALPH_CJK},
    {0x3040, 0x312F, ALPH_CJK},
    {0x3130, 0x318F, ALPH_HANGUL},
    {0x31A0, 0x31FF, ALPH_CJK},
    {0x3400, 0x4DB5, ALPH_CJK},
    {0x4E00, 0x9FA5, ALPH_CJK},
    {0xAC00, 0xD7A3, ALPH_HANGUL},
    {0xF900, 0xFA6A, ALPH_CJK},
    {0xFB1D, 0xFB4F, ALPH_HEBREW},
    {0xFB50, 0xFEFC, ALPH_ARABIC},
    {0xFF65, 0xFF9F, ALPH_CJK},
    {0xFFA0, 0xFFDC, ALPH_HANGUL},
    {0x20000, 0x2A6D6, ALPH_CJK},
    {0x2F800, 0x2FA1D, ALPH_CJK},
};

typedef struct text {
    int charset;
    int html, htmlstate;
    char htmltag[16];
    int htmltagpos, htmlstylenest;
    char htmlattr[16], htmlattrval[1024];
    int htmlattrpos, htmlattrvalpos;
    int qp, qpstate;
    charset_state state;
    int freq[ALPH_COUNT];
    wchar_t firstbit[257];
    int firstbitlen;
} text;

static int scanned_textpart_already = 0;
static int foreign_language = 0;

static void scanner_init_text(scanner *s, stream *st, void *vctx)
{
    text *ctx = (text *)vctx;
    struct structural_path_element path[MAXSTREAMS];
    int pathlen, index, i, textpart_priority;
    char *cte;

    ctx->charset = CS_NONE;
    ctx->qp = ctx->html = 0;
    ctx->qpstate = ctx->htmlstate = ctx->htmltagpos = ctx->htmlstylenest = 0;
    ctx->htmlattrpos = ctx->htmlattrvalpos = 0;
    ctx->state = charset_init_state;
    ctx->firstbitlen = 0;
    for (i = 0; i < ALPH_COUNT; i++)
	ctx->freq[i] = 0;

    /*
     * Find the innermost RFC822 linkdata.
     */
    pathlen = scanner_trace_ancestry(s, st, path);
    index = pathlen-1;
    while (index >= 0 && path[index].type != ST_RFC822)
	index--;
    if (index < 0)
	return;			       /* didn't find any (!) */

    ctx->charset =
	charset_from_mimeenc(path[index].extra->rfc822.charset);
    if (ctx->charset == CS_NONE)       /* not in a charset we know about, so */
	ctx->charset = CS_ASCII;       /*   fall back to treating as ASCII */

    /*
     * See if the content type is text/html, in which case we will
     * strip out HTML tags to extract just the text for this
     * analysis.
     */
    if (!strcasecmp(path[index].extra->rfc822.content_type, "text/html"))
	ctx->html = 1;

    /*
     * Determine the content transfer encoding. 7bit, 8bit and QP
     * we deal with ourselves. Base64, however, we leave to the
     * scanner code.
     */
    cte = path[index].extra->rfc822.content_transfer_encoding;
    if (!strcasecmp(cte, "quoted-printable") && index == pathlen-1) {
	ctx->qp = 1;
    } else if (!strcasecmp(cte, "base64") && index == pathlen-2 &&
	       path[index+1].type == ST_BASE64) {
	/* ok */
    } else if ((!strcasecmp(cte, "7bit") ||
		!strcasecmp(cte, "8bit") ||
		!strcasecmp(cte, "binary") ||
		!cte[0]) &&
	       index == pathlen-1) {
	/* ok */
    } else {
	/* strange stream which we ignore */
	ctx->charset = CS_NONE;
    }

    /*
     * We generally only look at the first text part of a message.
     * Exception: a text/html part supersedes a previous
     * text/plain (because we may be able to do better deductions
     * with more information).
     */
    textpart_priority = (ctx->html ? 2 : 1);
    if (ctx->charset != CS_NONE) {
	if (scanned_textpart_already >= textpart_priority) {
	    ctx->charset = CS_NONE;
	    return;		       /* we only look at the first part */
	} else
	    scanned_textpart_already = textpart_priority;
    }

#ifdef DEBUG
    if (ctx->charset != CS_NONE)
	printf("Stream %p: %s, qp=%d\n", (void *)st,
	       charset_to_localenc(ctx->charset), ctx->qp);
#endif
}

static void scanner_feed_text_unicode(scanner *s, stream *st, text *ctx,
				      const wchar_t *data, int len)
{
    int uselen;

    uselen = lenof(ctx->firstbit)-1 - ctx->firstbitlen;

    if (uselen > 0) {
	if (uselen > len)
	    uselen = len;
	memcpy(ctx->firstbit + ctx->firstbitlen, data,
	       uselen * sizeof(wchar_t));
	ctx->firstbitlen += uselen;
    }
}

static void scanner_feed_text_posthtml(scanner *s, stream *st, text *ctx,
				       const char *data, int len)
{
    wchar_t wbuf[512];
    int wbuflen;

    while (len > 0) {
	wbuflen = charset_to_unicode(&data, &len, wbuf, lenof(wbuf),
				     ctx->charset, &ctx->state, NULL, 0);
	if (wbuflen > 0) {
	    int i;

	    scanner_feed_text_unicode(s, st, ctx, wbuf, wbuflen);

	    for (i = 0; i < wbuflen; i++) {
		int ch = wbuf[i];
		int bot, mid, top, alph = -1;

		/*
		 * Search the ranges array to determine what
		 * alphabet this character belongs to.
		 */
		bot = -1;
		top = lenof(ranges);
		while (top - bot > 1) {
		    mid = (bot + top) / 2;
		    if (ch > ranges[mid].high)
			bot = mid;
		    else if (ch < ranges[mid].low)
			top = mid;
		    else {
			alph = ranges[mid].alph;
			break;
		    }
		}

		if (alph >= 0) {
		    ctx->freq[alph]++;
		}
	    }

#ifdef DEBUG
	    {
		char buf[1024];
		int buflen;
		const wchar_t *wptr = wbuf;
		int wlenleft = wbuflen;

		printf("%p: ", (void *)st);

		while (wlenleft > 0) {
		    buflen = charset_from_unicode(&wptr, &wlenleft,
						  buf, lenof(buf),
						  CS_UTF8, NULL, NULL);
		    fwrite(buf, 1, buflen, stdout);
		}

		printf("\n");
	    }
#endif
	}
    }
}

static const char *specific_msg;

static void scanner_feed_text_postqp(scanner *s, stream *st, text *ctx,
				     const char *data, int len)
{
    if (ctx->html) {
	/*
	 * Strip out HTML tags. I think that HTML entities should
	 * not be sufficiently numerous (or even alphabetic, in
	 * most cases) to be worth handling.
	 */
	char htbuf[1024];
	int htsize = 0;

	/*
	 * Decode HTML.
	 */

	while (len > 0) {
	    int oc = -1;
	    unsigned char c = *data++;

	    len--;

	    if (ctx->htmlstate == 0) {
		if (c == '<') {
		    ctx->htmlstate = 1;
		    ctx->htmltagpos = 0;
		} else if (ctx->htmlstylenest == 0) {
		    /*
		     * A character outside any HTML tag and not
		     * enclosed in <style>. Pass it through.
		     */
		    oc = c;
		}
	    } else {
		/*
		 * States are:
		 *  - 1: we are collecting the tag name
		 *  - 2: we are collecting an attribute name
		 *  - 3: we are collecting an attribute value
		 *  - any of those ORed with 16: we are also in quotes
		 */
		if (ctx->htmlstate == 3 && (c == '>' || c == ' ')) {
		    /*
		     * Characters which terminate collection of an
		     * attribute value. At this point we have a
		     * complete HTML attribute, so process that.
		     */
#if 0
		    printf("'%.*s' '%.*s'='%.*s'\n",
			   ctx->htmltagpos, ctx->htmltag,
			   ctx->htmlattrpos, ctx->htmlattr,
			   ctx->htmlattrvalpos, ctx->htmlattrval);
#endif
		    if (ctx->htmltagpos == 1 &&
			!memcmp(ctx->htmltag, "a", 1) &&
			ctx->htmlattrpos == 4 &&
			!memcmp(ctx->htmlattr, "href", 4)) {
			/*
			 * Analyse hrefs.
			 */
			char *p, *q;
			if (ctx->htmlattrvalpos >= sizeof(ctx->htmlattrval))
			    ctx->htmlattrvalpos = sizeof(ctx->htmlattrval) - 1;
			ctx->htmlattrval[ctx->htmlattrvalpos] = '\0';
			p = strstr(ctx->htmlattrval, "//");
			if (p && *p) {
			    p += 2;
			    q = p + strcspn(p, "/");
			    if (q - 8 > p &&
				!strncasecmp(q - 8, ".chat.ru", 8)) {
				specific_msg = "This appears to be a prolific"
				    " spam.";
			    }
			}
			if (!strncmp(ctx->htmlattrval, "http://cid-", 11) &&
			    !strncmp(ctx->htmlattrval + 11 +
				     strspn(ctx->htmlattrval+11,
					    "0123456789abcdefABCDEF"),
				     ".spaces.live.com/blog/", 22)) {
			    specific_msg = "This appears to be a prolific"
				" pharmaceutical spam.";
			}
			if (!strncmp(ctx->htmlattrval, "http://", 7) &&
			    strsuffix(ctx->htmlattrval + 7,
				      ctx->htmlattrval + 7 +
				      strspn(ctx->htmlattrval+7,
					     "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ."),
				      ".interia.pl")) {
			    specific_msg =
				"This appears to be a prolific spam.";
			}
			if (!strncmp(ctx->htmlattrval,
				     "http://groups.yahoo.com/group/", 30) &&
			    !strncmp(ctx->htmlattrval + 30 +
				     strspn(ctx->htmlattrval+30,
					    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"),
				     "/message/1", 10)) {
			    specific_msg =
				"This appears to be a prolific spam.";
			}
			if (!strncmp(ctx->htmlattrval,
				     "http://profiles.yahoo.com/blog/", 31) &&
			    !strncmp(ctx->htmlattrval + 31 +
				     strspn(ctx->htmlattrval+31,
					    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"),
				     "?eid=", 5)) {
			    specific_msg =
				"This appears to be a prolific spam.";
			}
		    }
		}

		if (c == '>' /* && !(ctx->htmlstate & 16) */) {
		    /*
		     * Tag is closing. Process it.
		     */
		    if (ctx->htmltagpos == 5 &&
			!memcmp(ctx->htmltag, "style", 5))
			ctx->htmlstylenest++;
		    else if (ctx->htmltagpos == 6 &&
			     !memcmp(ctx->htmltag, "/style", 6) &&
			     ctx->htmlstylenest > 0)
			ctx->htmlstylenest--;

		    /*
		     * And revert to outside-any-tag state.
		     */
		    ctx->htmlstate = 0;
		} else if (c == '"') {
		    /*
		     * Started or stopped quoting.
		     */
		    ctx->htmlstate ^= 16;
		} else if (c == ' ' && !(ctx->htmlstate & 16)) {
		    /*
		     * Whitespace outside quotes means we go into
		     * attribute-name mode.
		     */
		    ctx->htmlstate = 2;
		    ctx->htmlattrpos = 0;
		} else if (c == '=' && ctx->htmlstate == 2) {
		    /*
		     * Got our attribute name. Switch to
		     * collecting the attribute value.
		     */
		    ctx->htmlstate = 3;
		    ctx->htmlattrvalpos = 0;
		} else if ((ctx->htmlstate & ~16) == 1 &&
			   ctx->htmltagpos < sizeof(ctx->htmltag)) {
		    /*
		     * Add this character to the tag name we're
		     * collecting.
		     */
		    ctx->htmltag[ctx->htmltagpos++] =
			tolower((unsigned char)c);
		} else if ((ctx->htmlstate & ~16) == 2 &&
			   ctx->htmlattrpos < sizeof(ctx->htmlattr)) {
		    /*
		     * Add this character to the attribute name.
		     */
		    ctx->htmlattr[ctx->htmlattrpos++] =
			tolower((unsigned char)c);
		} else if ((ctx->htmlstate & ~16) == 3 &&
			   ctx->htmlattrvalpos < sizeof(ctx->htmlattrval)) {
		    /*
		     * Add this character to the attribute value.
		     */
		    ctx->htmlattrval[ctx->htmlattrvalpos++] =
			tolower((unsigned char)c);
		}
	    }

	    if (oc >= 0)
		htbuf[htsize++] = oc;

	    if (htsize == lenof(htbuf) || len == 0) {
		scanner_feed_text_posthtml(s, st, ctx, htbuf, htsize);
		htsize = 0;
	    }
	}

    } else {
	/*
	 * This isn't an HTML stream, so just pass the text
	 * straight through.
	 */
	scanner_feed_text_posthtml(s, st, ctx, data, len);
    }
}

static void scanner_feed_text(scanner *s, stream *st, void *vctx,
			      const char *data, int len)
{
    text *ctx = (text *)vctx;

    if (ctx->charset == CS_NONE)
	return;			       /* ignore this stream totally */

    if (ctx->qp) {
	char qpbuf[1024];
	int qpsize = 0;

	/*
	 * Decode quoted-printable.
	 */

	while (len > 0) {
	    int oc = -1;
	    unsigned char c = *data++;

	    len--;

	    if (ctx->qpstate == 0) {
		if (c != '=')
		    oc = c;
		else
		    ctx->qpstate = 0x100;
	    } else if (ctx->qpstate == 0x100) {
		if (c >= '0' && c <= '9')
		    ctx->qpstate = 0x200 + c - '0';
		else if (c >= 'a' && c <= 'f')
		    ctx->qpstate = 0x20A + c - 'a';
		else if (c >= 'A' && c <= 'F')
		    ctx->qpstate = 0x20A + c - 'A';
		else
		    ctx->qpstate = 0;
	    } else if (ctx->qpstate >= 0x200) {
		if (c >= '0' && c <= '9')
		    oc = (ctx->qpstate << 4) + c - '0';
		else if (c >= 'a' && c <= 'f')
		    oc = (ctx->qpstate << 4) + 10 + c - 'a';
		else if (c >= 'A' && c <= 'F')
		    oc = (ctx->qpstate << 4) + 10 + c - 'A';

		if (oc >= 0)
		    oc &= 0xFF;

		ctx->qpstate = 0;
	    }

	    if (oc >= 0)
		qpbuf[qpsize++] = oc;

	    if (qpsize == lenof(qpbuf) || len == 0) {
		scanner_feed_text_postqp(s, st, ctx, qpbuf, qpsize);
		qpsize = 0;
	    }
	}
    } else {
	/*
	 * This isn't a QP stream, so just pass the text straight
	 * through.
	 */
	scanner_feed_text_postqp(s, st, ctx, data, len);
    }
}

static void scanner_cleanup_text(scanner *s, stream *st, void *vctx)
{
    text *ctx = (text *)vctx;
    const wchar_t *str;
    int linelen, stringlen;
    int i;

    if (ctx->charset == CS_NONE)
	return;			       /* ignore this stream totally */

#ifdef DEBUG
    printf("%p cleaning up [", (void *)st);
    {
	char *sep = "";
	for (i = 0; i < ALPH_COUNT; i++) {
	    printf("%s%d", sep, ctx->freq[i]);
	    sep = " ";
	}
    }
    printf("]\n");
#endif

    /*
     * If any alphabet other than ALPH_LATIN outnumbers ALPH_LATIN
     * by more than a factor of two, we assume this message is in a
     * language I don't speak.
     */
    for (i = 0; i < ALPH_COUNT; i++)
	if (i != ALPH_LATIN && ctx->freq[i] > ctx->freq[ALPH_LATIN])
	    foreign_language = 1;

    /*
     * If the start of the message text matches a really obvious
     * spam pattern, we reject.
     */
    assert(ctx->firstbitlen < lenof(ctx->firstbit));
    ctx->firstbit[ctx->firstbitlen] = L'\0';
    str = L"Make a miracles in bed!\n";
    linelen = wcscspn(ctx->firstbit, L"\n");
    if (!wcsncmp(ctx->firstbit, str, wcslen(str)) ||
	!wcsncmp(ctx->firstbit + linelen + 1, str, wcslen(str)))
	specific_msg = "This appears to be a prolific pharmacy spam.";
    str = L"You are receiving this email because we wish you to use";
    linelen = wcscspn(ctx->firstbit, L"\n");
    if (!wcsncmp(ctx->firstbit, str, wcslen(str)) ||
	!wcsncmp(ctx->firstbit + linelen + 1, str, wcslen(str)))
	specific_msg = "This appears to be a prolific spam for varying types of business.";
    str = L"See your card as often as you wish during the next 15 days.\n";
    if (!wcsncmp(ctx->firstbit + linelen + 1, str, wcslen(str))) {
	greetingspam |= 2;
	if (greetingspam == 3)
	    specific_msg = "This appears to be a prolific greetings card spam.";
    }
    str = L"West Union Group is searching for";
    if (!wcsncmp(ctx->firstbit + linelen + 1, str, wcslen(str))) {
	specific_msg = "This appears to be a prolific job offer spam.";
    }
    str = L"Adobe CS3 Master Collection for PC or MAC includes:";
    i = wcscspn(ctx->firstbit, L"\r\n");
    if (i >= wcslen(str) &&
	!wcsncmp(ctx->firstbit + i - wcslen(str), str, wcslen(str))) {
	specific_msg = "This appears to be a prolific pirated-software spam.";
    }
    {
	wchar_t compressed[1024], *p;
	const wchar_t *q;
	wchar_t last = 0;
	q = ctx->firstbit;
	while (*q == '\r' || *q == '\n') q++;
	p = compressed;
	last = ' ';
	while (p - compressed < 256 && *q && *q != '\r' && *q != '\n') {
	    char c = *q;
	    if (isspace((unsigned char)c))
		c = ' ';
	    if (c != last)
		*p++ = c;
	    last = c;
	    q++;
	}
	if (p > compressed && p[-1] == ' ')
	    p--;
	*p = '\0';

	str = L"Bachelor, MasterMBA, and Doctorate";
	if (!wcsncmp(compressed, str, wcslen(str)))
	    specific_msg = "This appears to be a prolific diploma mill spam.";
	str = L"WHAT A GREAT IDEA!&nbsp;We";
	if (!wcsncmp(compressed, str, wcslen(str)))
	    specific_msg = "This appears to be a prolific diploma mill spam.";
	str = L"hey i found your contact info online.";
	if (!wcsncasecmp(compressed, str, wcslen(str)))
	    specific_msg = "This appears to be spam.";
	str = L"hey i found ur contact info online.";
	if (!wcsncasecmp(compressed, str, wcslen(str)))
	    specific_msg = "This appears to be spam.";

	while (*q == '\r' || *q == '\n' || isspace((unsigned char)*q)) q++;
	p = compressed;
	while (p - compressed < 1023 && *q && *q != '\r' && *q != '\n') {
	    char c = *q;
	    if (isspace((unsigned char)c))
		c = ' ';
	    *p++ = c;
	    q++;
	}
	*p = '\0';

	if (!wcsncmp(compressed, L"http://cid-", 11) &&
	    !wcsncmp(compressed + 11 +
		     wcsspn(compressed+11, L"0123456789abcdefABCDEF"),
		     L".spaces.live.com/blog/", 22))
	    specific_msg = "This appears to be a prolific pharmaceutical spam.";
    }
    str = L"You are receiving this email because we wish you to use our web design service.";
    stringlen = wcslen(str);
    linelen = wcscspn(ctx->firstbit, L"\n");
    while (linelen > 0 && iswspace(ctx->firstbit[linelen-1]))
	linelen--;
    if (linelen >= stringlen &&
	!wcsncmp(ctx->firstbit + linelen - stringlen, str, stringlen)) {
	specific_msg = "This appears to be a persistent corporate-web-design spam.";
    }
    str = L"Collectively, we focus on different sources of power, new green tech, and reusable materials.";
    stringlen = wcslen(str);
    linelen = wcscspn(ctx->firstbit, L"\n");
    while (linelen > 0 && iswspace(ctx->firstbit[linelen-1]))
	linelen--;
    if (linelen >= stringlen &&
	!wcsncmp(ctx->firstbit + linelen - stringlen, str, stringlen)) {
	specific_msg = "This appears to be a prolific spam.";
    }
}

static int agif_found = 0;
static int dslpng_found = 0;
static int rtf_found = 0;

static void scanner_init_filename(scanner *s, stream *st, void *vctx)
{
    struct structural_path_element path[MAXSTREAMS];
    int npath = scanner_trace_ancestry(s, st, path);
    int i;

    for (i = 0; i < npath; i++) {
	if (path[i].type == ST_RFC822 &&
	    (strcaseprefix(path[i].extra->rfc822.filename, "DSL") ||
	     strcaseprefix(path[i].extra->rfc822.filename, "DSC")) &&
	    !strcasecmp(path[i].extra->rfc822.filename + 3 +
			strspn(path[i].extra->rfc822.filename+3, "0123456789"),
			".png"))
	    dslpng_found = 1;
	if (path[i].type == ST_RFC822 &&
	    !strcasecmp(path[i].extra->rfc822.content_type,
			"application/octet-stream") &&
	    strcasesuffix(path[i].extra->rfc822.filename,
			  path[i].extra->rfc822.filename + 
			  strlen(path[i].extra->rfc822.filename),
			  ".rtf"))
	    rtf_found = 1;
    }
}
static void scanner_feed_null(scanner *s, stream *st, void *vctx,
			      const char *data, int len) {}

void report(void *ctx, int condition,
	    struct structural_path_element *path, int pathlen)
{
    if (condition == SC_AGIF) {
	/*
	 * Given the typical use of an animated GIF in spam, I
	 * don't think it's necessary to go right into zip files
	 * for this. Just stick with animated GIFs in the main MIME
	 * structure.
	 */
	int i;
	for (i = 0; i < pathlen; i++)
	    if (path[i].type != ST_ROOT && path[i].type != ST_RFC822 &&
		path[i].type != ST_BASE64 && path[i].type != ST_UUE)
		return;
	agif_found = 1;
    }
}

static scanner *darkly = NULL;

const char *scanner_filter(int len, const char *data)
{
    if (!darkly) {
	darkly = scanner_new(report, NULL);
	scanner_register(darkly, scanner_init_agif, scanner_feed_agif,
			 NULL, sizeof(agif));
	scanner_register(darkly, scanner_init_text, scanner_feed_text,
			 scanner_cleanup_text, sizeof(text));
	scanner_register(darkly, scanner_init_filename, scanner_feed_null,
			 NULL, 0);
    }

    if (len == 0) {
	scanner_free(darkly);
    } else {
	scanner_feed_data(darkly, data, len);
    }

    if (specific_msg)
	return specific_msg;

    if (agif_found)
	return "Regrettably I currently reject any mail "
	"containing an animated GIF.";

    if (foreign_language)
	return "Regrettably I only speak English.";

    if (dslpng_found)
	return "This looks like a drugs spam I've been seeing a lot of.";

    if (rtf_found)
	return "I'm treating main message bodies in RTF as a sign of spam.";

    return NULL;
}

/* ----------------------------------------------------------------------
 * Subfilter which rejects messages based on simple header
 * criteria, used for any particularly persistent single annoyances
 * against whom it's a plausible defence.
 */

const char *process_address(const char *hdr, const char *addr)
{
#ifdef DEBUG
    printf("%s: <%s>\n", hdr, addr);
#endif
    char *at;

    if (!strcasecmp(hdr, "From")) {
	/*
	 * `HotScripts Newsletter'
	 */
	if (!strcmp(addr, "newsletter@hotscripts.com") ||
	    !strcmp(addr, "newsletter@inetinteractive.com") ||
	    !strcmp(addr, "hotscripts@newsletter.inetinteractive.com") ||
	    !strcmp(addr, "hotscripts@newsletter.myinet.com"))
	    return "I have never subscribed to this newsletter, so "
	    "I am forced to consider it as spam.";

	/*
	 * `tothesource'
	 */
	if (!strcmp(addr, "reply@engageculture.com") ||
	    !strcmp(addr, "reply2@engageculture.com"))
	    return "You have been spamming me with this drivel since 2004. "
	    "Stop it!";

	/*
	 * greetings.com (a sudden slew on the morning of
	 * 2007-01-17)
	 */
	if (!strcasecmp(addr, "greeting@greetings.com"))
	    return "I'm assuming this to be spam.";

	/*
	 * eventosrh.com.br, whoever they are.
	 */
	at = strchr(addr, '@');
	if (at && (!strcasecmp(at+1, "eventosrh-info.com.br") ||
		   !strcasecmp(at+1, "eventosrh.com.br")))
	    return "You have been persistently spamming me since 2007-02. STOP IT!";

	/*
	 * Persistent newsletters.
	 */
	if (!strcasecmp(addr, "info@technologytransfertactics.com") ||
	    (at && !strcasecmp(at+1, "selfgrowth.com")))
	    return "This address has persistently sent me newsletters to "
	    "which I never subscribed. I am therefore considering it a spammer.";

	/*
	 * anales.gr.
	 */
	if (at && (!strcasecmp(at+1, "anales.gr")))
	    return "I'm currently assuming mail from this domain to be spam.";

	/*
	 * info@lottery.co.uk.
	 */
	if (at && (!strcasecmp(at+1, "lottery.co.uk")))
	    return "I'm currently assuming mail from this domain to be spam.";

	/*
	 * 2009-01-28: a lot of identical 'Job offer from
	 * Coca-Cola' emails which look more like viruses than
	 * spam, but the executable is replaced by a giant pile of
	 * zero bytes. Seems easier, for the moment, to block them
	 * on their from address rather than messing about with
	 * viruswatch.
	 */
	if (!strcasecmp(addr, "hr@coca-cola.com")) {
	    return "This address is currently under suspicion of being "
		"a virus vector.";
	}

	/*
	 * 'Sid and Sally', whoever they are, have been persistently
	 * spamming me for a while now.
	 */
	if (!strcasecmp(addr, "sidandsally@id1.idrelay.com")) {
	    return "I don't know what this is, but I get a lot of it"
		" and never asked for it, so I'm assuming it to be spam.";
	}

	/*
	 * 2009-09-09: emergency response to a sudden flood.
	 */
	if (!strncmp(addr, "Amina@", 6) && strlen(addr) > 10 &&
	    !strcmp(addr + strlen(addr) - 10, ".pobox.com")) {
	    return "This looks like spam.";
	}

	/*
	 * 2009-10-04: another emergency flood response.
	 */
	if (!strcasecmp(addr, "NAVER-MAILER@naver.com")) {
	    return "This looks like part of a flood of meaningless non-bounces.";
	}

	/*
	 * 2010-01-04: some software distribution site has
	 * unilaterally decided to send me weekly volume reports in
	 * the mistaken belief that I care
	 */
	if (!strcasecmp(addr, "users@brothersoft.com")) {
	    return "I have never requested regular mail from this address and must therefore consider it spam.";
	}

	/*
	 * 2010-01-07: one of the more persistent 'networking sites'
	 */
	if (!strcasecmp(addr, "admin@vkontakte.ru")) {
	    return "I have received a lot of unwanted mail from this address and am therefore considering it a spammer.";
	}

	/*
	 * 2010-07-28: three identical emails in quick succession
	 * from this one make me reach for the ban button.
	 */
	if (!strcasecmp(addr, "info@miraclegroup.com")) {
	    return "This address appears to be a corporate spammer of some sort.";
	}

        /*
         * 2010-12-16: time to go through recent spam and make a list
         * of prolific annoyances.
         */
        if (!strcasecmp(addr, "marketing@giveshoes.org") ||
            !strcasecmp(addr, "mail@avgenakis.gr") ||
            !strcasecmp(addr, "boletines@uptodown.com") ||
            !strcasecmp(addr, "phplist@ovs-genealogy.com") ||
            !strcasecmp(addr, "thesecondadam@charter.net") ||
            !strcasecmp(addr, "sidandsally@id1.idrelay.com") ||
            (at && (!strcasecmp(at+1, "hallmark.com") ||
                    !strcasecmp(at+1, "process-news.com") ||
                    !strcasecmp(at+1, "greenpower.msgfocus.com") ||
                    !strcasecmp(at+1, "greenschoolrecyclingfundraiser.org")
                ))) {
            return "I'm currently assuming mail from this address to be spam.";
        }

        /*
         * 2011-05-03: a sudden flood of identical 419s.
         */
        if (!strcasecmp(addr, "mpchristos@aol.com")) {
            return "I've had 15 identical emails from this address already. Enough is enough.";
        }

        /*
         * 'Davis Micro LLC'
         */
        if (!strcasecmp(addr, "brandsdragon@gmail.com") ||
            !strcasecmp(addr, "davismicro@gmail.com") ||
            !strcasecmp(addr, "everbuyingtrade@gmail.com")) {
            return "You have been persistently spamming me since October 2010. Please stop.";
        }

        /*
         * Select2gether
         */
        if (!strcasecmp(addr, "do-not-reply@select2gether.com")) {
            return "You have been persistently spamming me since February 2011. Please stop.";
        }

        /*
         * 2011-10-11: a prolific new newsletter I didn't subscribe
         * to.
         */
        if (!strcasecmp(at+1, "thechurchreport.com")) {
            return "I have never subscribed to this newsletter and must therefore consider it spam.";
        }

        /*
         * 2012-05-03: a lovely birthday present of sudden prolific
         * sales-brocure spam.
         */
        if (!strcasecmp(addr, "sales@oooteplomir.com")) {
            return "I'm assuming mail from this address to be spam.";
        }

        /*
         * 2012-07-02: combit.net have mailed me for the last time.
         */
        if (at && (!strcasecmp(at+1, "combit.net")))
	    return "PR information from this domain is unsolicited and unwanted. I consider it spam.";

        /*
         * 2012-08-07: 'Pirooz', whoever that might be.
         */
        if (!strcmp(addr, "daryakala@gmail.com"))
            return "This address has persistently spammed me and is blocked.";
    }

    if (!strcasecmp(hdr, "Reply-to")) {
	/*
	 * `tothesource' again
	 */
	if (!strcmp(addr, "comments@tothesource.org"))
	    return "You have been spamming me with this drivel since 2004. "
	    "Stop it!";

	/*
	 * greetings.com (a sudden slew on the morning of
	 * 2007-01-17)
	 */
	if (!strcasecmp(addr, "greeting@greetings.com"))
	    return "I'm assuming this to be spam.";

	/*
	 * eventosrh.com.br again.
	 */
	at = strchr(addr, '@');
	if (at && (!strcasecmp(at+1, "eventosrh-info.com.br") ||
		   !strcasecmp(at+1, "eventosrh.com.br")))
	    return "You have been persistently spamming me since 2007-02. STOP IT!";
    }

    if (!strcasecmp(hdr, "Cc")) {
	if (!strcmp(addr, "alsaplayer-announce@lists.tartarus.org") ||
	    !strcmp(addr, "alsaplayer-devel@lists.tartarus.org"))
	    return "Anything copied to both me and the alsaplayer developers"
	    " is unlikely to be real mail.";
    }

    if (!strcasecmp(hdr, "To")) {
	if (!strcmp(addr, "puttkammerjens@compuserver.de") ||
	    !strcmp(addr, "puttiphs@scg.co.th") ||
	    !strcmp(addr, "puttfarcken@uke.de"))
	    return "The headers make this look like spam.";

        if (!strcmp(addr, "021group25@googlegroups.com"))
	    return "I have never subscribed to any list called 021group25@googlegroups.com and so must consider messages purporting to be sent through it to be spam.";
    }

    return NULL;
}

int seen_pharm_word = 0;
int seen_strange_word = 0;

void process_subjword(const char *word)
{
    /*
     * There's a particularly persistent pharmacy spam type at the
     * moment which is easily identifiable from the subject lines.
     * These subject lines contain an upper-case word with at least
     * two lower-case random letters mixed into it. The upper-case
     * letters spell one of the following words.
     * 
     * Capital I is often missed out and replaced with lower case
     * l, for these purposes, so we treat I as a lower-case letter.
     */
#define LC(c) ( ((c) >= 'a' && (c) <= 'z') || (c) == 'I' )
#define UC(c) ( ((c) >= 'A' && (c) <= 'Z') && (c) != 'I' )
    static const char *const ucwords[] = {
        "PHARMACY", "PHARMA", "PHARM",
	"VAGRA", "VAGGRA", "VAGRRA", "WAGRA", "WAGGRA", "WAGRRA",
        "VALUM", "VALLUM",
        "CALS", "CALLS",
        "AMBEN", "AMBBEN",
    };
    static const char *const auxwords[] = {
	/*
	 * These words also appear in the subject lines I'm
	 * thinking of. Anything other than that, however,
	 * disqualifies the mail from rejection.
	 */
	"my", "your", "good", "new", "news", "test", "the", "Re:",
    };
    const char *p, *q;
    int nlc, nuc, i;

    for (i = 0; i < lenof(auxwords); i++)
	if (!strcmp(word, auxwords[i]))
	    return;		       /* aux word which we ignore */

    /*
     * Check that there are at least two lower-case letters, at
     * least one upper-case one, and nothing _but_ upper and lower
     * case.
     */
    nlc = nuc = 0;
    for (p = word; *p; p++) {
	if (LC(*p))
	    nlc++;
	else if (UC(*p))
	    nuc++;
	else {
	    seen_strange_word = 1;
	    return;
	}
    }
    if (nlc < 2 || nuc < 1) {
	seen_strange_word = 1;
	return;
    }

    /*
     * Now see if the upper-case letters spell one of the above
     * words.
     */
    for (i = 0; i < lenof(ucwords); i++) {
	p = word;
	q = ucwords[i];
	while (1) {
	    while (LC(*p))
		p++;		       /* skip lowercase */
	    if (*p != *q)
		break;
	    if (!*p) {
		seen_pharm_word = 1;
		return;
	    }
	    p++;
	    q++;
	}
    }

    seen_strange_word = 1;
    return;
}

const char *process_subject(const char *subject)
{
    if (strprefix(subject, "You've received a"))
	greetingspam |= 1;

    if (strprefix(subject, "Link Exchange"))
	return "Mails with subject lines beginning 'Link Exchange' are"
	" presumed to be persistent and prolific spam.";

    {
	const char *p = subject;

	if (strprefix(p, "aid ") ||
	    strprefix(p, "lift ") ||
	    strprefix(p, "hoist ") ||
	    strprefix(p, "heave ") ||
	    strprefix(p, "boost ") ||
	    strprefix(p, "raise ") ||
	    strprefix(p, "uplift ") ||
	    strprefix(p, "ascent ") ||
	    strprefix(p, "empower ") ||
	    strprefix(p, "support ")) {
	    p += strcspn(p, " ") + 1;
	    if (strprefix(p, "your ")) {
		p += 5;
		if (strprefix(p, "belove ") ||
		    strprefix(p, "lover ") ||
		    strprefix(p, "darling ") ||
		    strprefix(p, "sweet ")) {
		    p += strcspn(p, " ") + 1;
		}
		if (strprefix(p, "sexual ") ||
		    strprefix(p, "couch ") ||
		    strprefix(p, "bed ") ||
		    strprefix(p, "night ")) {
		    p += strcspn(p, " ") + 1;
		    if (strprefix(p, "adventure") ||
			strprefix(p, "experience") ||
			strprefix(p, "event") ||
			strprefix(p, "times")) {
			return "This looks like a prolific drugs spam.";
		    }
		} else if (strprefix(p, "sexuality")) {
		    return "This looks like a prolific drugs spam.";
		}
	    }
	}
    }

    if (strprefix(subject, "Greetings From Mr. Tomo Sand Nori"))
	return "Seven of these messages were quite enough, thanks.";

    /* 2011-02-28: a sudden flood of these */
    if (strprefix(subject, "Investor insights"))
	return "This appears to be a persistent pump-and-dump spam.";

    return NULL;
}

/*
 * We use the RFC 2822 definition of whitespace, rather than
 * relying on ctype.h.
 */
#define WSP(c) ( (c)==' ' || (c)=='\t' )
#define FWS(c) ( WSP(c) || (c)=='\n' )

const char *header_filter(int len, const char *data)
{
    static int state = 0;
    static char hdrbuf[64];
    static unsigned hdrlen = 0;
    static char addrbuf[512];
    static unsigned addrlen = 0;
    static char wordbuf[512];
    static unsigned wordlen = 0;
    static char wholesubjbuf[2048];
    static unsigned wholesubjlen = 0;
    const char *msg;

    while (len--) {
	int c = *data++;

	/*
	 * We expect either \r\n or \n as our newline format. So if
	 * we see a \r, then we will consistently ignore it.
	 */
	if (c == '\r')
	    continue;

	switch (state) {
	  case 0:
	    case_0:
	    /*
	     * Start of header line; we're accumulating a header
	     * name.
	     */
	    addrlen = 0;

	    if (WSP(c))		       /* malformed header, probably "From " */
		state = 1;	       /* so skip rest of header */
	    else if (c == '\n')
		hdrlen = 0;	       /* start again */

	    if (c != ':') {
		if (hdrlen < lenof(hdrbuf))
		    hdrbuf[hdrlen++] = c;
		/* stay in state 0 */
	    } else {
		state = 1;	       /* skip header, unless we change mind */
		if (hdrlen < lenof(hdrbuf)) {
		    hdrbuf[hdrlen] = '\0';
		    if (!strcasecmp(hdrbuf, "From") ||
			!strcasecmp(hdrbuf, "Reply-to") ||
			!strcasecmp(hdrbuf, "To") ||
			!strcasecmp(hdrbuf, "Cc")) {
			state = 10;    /* header containing addresses */
		    } else if (!strcasecmp(hdrbuf, "Subject")) {
			state = 30;    /* subject line */
		    }
		}
		break;
	    }
	  case 1:
	    /*
	     * We're in the middle of a header which we're
	     * ignoring.
	     */
	    if (c == '\n')
		state = 2;
	    break;
	  case 2:
	    /*
	     * We've just been ignoring a header, and encountered a
	     * newline. If the next character is a space, we're
	     * back in state 1 and ignoring stuff again.
	     */
	    if (c == '\n')
		state = -1;	       /* actually the end of all headers */
	    else if (WSP(c))
		state = 1;	       /* carry on ignoring stuff */
	    else {
		state = 0;	       /* new header keyword */
		hdrlen = 0;
		goto case_0;	       /* so we must process it (yuck) */
	    }
	    break;

	  case 10:
	    case_10:
	    /*
	     * We're now processing a header which contains email
	     * addresses. The deal with this is:
	     *
	     * 	- we must completely ignore any text inside
	     * 	  parentheses
	     * 	- any other string of non-whitespace characters
	     * 	  other than :;,<> is collected as a potential
	     * 	  address; quoted text within it is included even
	     * 	  if it does contain whitespace or those
	     * 	  punctuation characters
	     * 	- if the next thing (not counting parenthesised
	     * 	  comments) after the candidate address is , or >
	     * 	  or ; or end-of-header, then the candidate address
	     * 	  really is an address.
	     */
	    if (c == '\n') {
		state = 20;
	    } else if (c == ':' || c == '<' || c == ')') {
		addrlen = 0;
	    } else if (c == ',' || c == ';' || c == '>') {
		if (addrlen > 0 && addrlen < lenof(addrbuf)) {
		    addrbuf[addrlen] = 0;
		    if ((msg = process_address(hdrbuf, addrbuf)) != NULL)
			return msg;
		}
		addrlen = 0;
	    } else if (WSP(c)) {
		/* do nothing */
	    } else if (c == '(') {
		state = 21;
	    } else {
		/*
		 * Accumulate an alphabetic token, i.e. candidate
		 * address.
		 */
		addrlen = 0;
		state = 11;
		goto case_11;
	    }
	    break;
	  case 11:
	    case_11:
	    /*
	     * We're accumulating an alphabetic token.
	     */
	    if (c == '\n' || c == ':' || c == ';' || c == '<' ||
		c == '>' || c == ',' || WSP(c) || c == '(' || c == ')') {
		state = 10;
		goto case_10;
	    } else if (c == '"') {
		state = 12;	       /* in quotes */
	    } else if (c == '\\') {
		state = 13;	       /* backslashed next character */
	    } else {
		if (addrlen < lenof(addrbuf))
		    addrbuf[addrlen++] = c;
	    }
	    break;
	  case 12:
	    /*
	     * Alphabetic token inside double quotes.
	     */
	    if (c == '"') {
		state = 11;
	    } else if (c == '\n') {
		state = 20;	       /* abandon quotes */
	    } else if (c == '\\') {
		state = 14;	       /* backslashed within quotes */
	    } else {
		if (addrlen < lenof(addrbuf))
		    addrbuf[addrlen++] = c;
	    }
	    break;
	  case 13:
	    /*
	     * Backslashed character in alphabetic token.
	     */
	    if (c == '\n') {
		state = 20;	       /* abandon quoting */
	    } else {
		if (addrlen < lenof(addrbuf))
		    addrbuf[addrlen++] = c;
		state = 11;
	    }
	    break;
	  case 14:
	    /*
	     * Backslashed character inside quotes in alphabetic
	     * token.
	     */
	    if (c == '\n') {
		state = 20;	       /* abandon quoting */
	    } else {
		if (addrlen < lenof(addrbuf))
		    addrbuf[addrlen++] = c;
		state = 12;
	    }
	    break;
	  case 20:
	    /*
	     * In the middle of an address header, we've just seen
	     * a newline. If the next character is whitespace, go
	     * back to state 10 and continue with the header.
	     * Otherwise, process a potential final address and go
	     * back to state zero.
	     */
	    if (WSP(c)) {
		state = 10;
	    } else {
		if (addrlen > 0 && addrlen < lenof(addrbuf)) {
		    addrbuf[addrlen] = 0;
		    if ((msg = process_address(hdrbuf, addrbuf)) != NULL)
			return msg;
		}
		addrlen = 0;

		if (c == '\n')
		    state = -1;
		else {
		    state = 0;
		    hdrlen = 0;
		    goto case_0;
		}
	    }
	    break;
	  case 21:
	    /*
	     * In the middle of an address header, we've seen an
	     * open parenthesis. Discard comment text until we see
	     * a closing one.
	     */
	    if (c == '\n') {
		state = 20;	       /* abandon comment */
	    } else if (c == ')') {
		state = 10;
	    } else if (c == '\\') {
		state = 22;	       /* backslash */
	    }
	    break;
	  case 22:
	    /*
	     * Backslash-quoted character inside comment.
	     */
	    if (c == '\n') {
		state = 20;	       /* abandon quoting and comment */
	    }
	    break;
	  case 30:
	    /*
	     * Subject line header, when not already accumulating a
	     * word.
	     */
	    if (c == '\n')
		state = 31;
	    else {
		if (!WSP(c)) {
		    if (wholesubjlen < lenof(wholesubjbuf)-1)
			wholesubjbuf[wholesubjlen++] = c;
		    wordbuf[0] = c;
		    wordlen = 1;
		    state = 32;
		}
	    }
	    break;
	  case 31:
	    /*
	     * Subject line header, just after a newline. If the
	     * next character is whitespace, go back to state 30
	     * and continue with the header. Otherwise, go back to
	     * state zero.
	     */
	    if (WSP(c)) {
		state = 30;
	    } else {
		/*
		 * End of subject header. See what we've got.
		 */
		if (seen_pharm_word && !seen_strange_word)
		    return "This looks like an online pharmacy spam. "
		    "I'm not interested, thanks.";

		assert(wholesubjlen>=0 && wholesubjlen<lenof(wholesubjbuf));
		wholesubjbuf[wholesubjlen] = '\0';
		if ((msg = process_subject(wholesubjbuf)) != NULL)
		    return msg;

		if (c == '\n')
		    state = -1;
		else {
		    state = 0;
		    hdrlen = 0;
		    goto case_0;
		}
	    }
	    break;
	  case 32:
	    /*
	     * Subject line header, in the middle of accumulating a
	     * word.
	     */
	    if (!FWS(c)) {
		if (wholesubjlen < lenof(wholesubjbuf)-1)
		    wholesubjbuf[wholesubjlen++] = c;
		if (wordlen < lenof(wordbuf))
		    wordbuf[wordlen++] = c;
	    } else {
		if (wordlen > 0 && wordlen < lenof(wordbuf)) {
		    wordbuf[wordlen] = 0;
		    process_subjword(wordbuf);
		}
		wordlen = 0;
		if (c == '\n')
		    state = 31;
		else {
		    if (wholesubjlen < lenof(wholesubjbuf)-1)
			wholesubjbuf[wholesubjlen++] = c;
		    state = 30;
		}
	    }
	    break;
	}
    }

    return NULL;
}

/* ----------------------------------------------------------------------
 * Main filter function, bringing all of the above together.
 */

const char *realfilter(const char *id, int len, const char *data)
{
    const char *ret;
    struct stat st;

    /*
     * Reject all mail to putty-announce, unless the magic key file
     * is currently present.
     */
    if (!strcmp(id, "RCPT") &&
	!strlenzcmp(data, len, "putty-announce@lists.tartarus.org") &&
	stat("/home/simon/allow-putty-announce", &st) != 0)
	return "putty-announce does not accept incoming mail.";

    /*
     * Other non-standard addresses to which lots of spam is
     * coming.
     */
    if (!strcmp(id, "RCPT") &&
	!strlenzcmp(data, len, "simon-tzsz@tartarus.org"))
	return "This address is disused.";

    /*
     * Otherwise, we don't care about headers; only the message
     * data is of interest to the rest of these filters.
     */
    if (strcmp(id, "DATA"))
	return NULL;

#if 0
    if ((ret = japanese_filter(len, data)) != NULL)
	return ret;
#endif

    if ((ret = scanner_filter(len, data)) != NULL)
	return ret;

    if ((ret = header_filter(len, data)) != NULL)
	return ret;

    return NULL;
}

const char *filter(const char *id, int len, const char *data)
{
    static char logname[PATH_MAX];
    static FILE *logfp = NULL;
    static int started_data = 0;
    static int newline = 1;
    static const char *msg = NULL;

    if (!logname[0]) {
	char datebuf[512];
	time_t t;
	struct tm tm;

	t = time(NULL);
	tm = *gmtime(&t);
	strftime(datebuf, 512, "%Y-%m-%d.%H:%M:%S", &tm);
	snprintf(logname, PATH_MAX, "%s/maillog/%s.%d", getenv("HOME"),
		 datebuf, getpid());

	logfp = fopen(logname, "w");
    }

    if (!msg)
	msg = realfilter(id, len, data);

    if (strcmp(id, "DATA")) {
	assert(!started_data);
	if (logfp) {
	    fprintf(logfp, "%s:%d:%.*s\n", id, len, len, data);
	}
    } else if (len > 0) {
	if (logfp) {
	    int i;
	    if (!started_data) {
		fprintf(logfp, "%s\n", id);
		started_data = 1;
	    }
	    for (i = 0; i < len; i++) {
		if (newline) {
		    putc('|', logfp);
		    putc(' ', logfp);
		}
		putc(data[i], logfp);
		newline = (data[i] == '\n');
	    }
	}
    } else {
	if (logfp) {
	    if (!newline)
		putc('\n', logfp);
	    if (msg)
		fprintf(logfp, "Rejected: %s\n", msg);
	    fclose(logfp);
	    if (!msg)
		unlink(logname);
	}
	return msg;
    }

    return NULL;
}
