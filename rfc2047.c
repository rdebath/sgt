/*
 * RFC2047 encoded header parser for Timber.
 */

#include <assert.h>

#include "timber.h"
#include "charset.h"

static int spot_encoded_word(const char *text, int length)
{
    int state = 0;
    const char *orig_text = text;

    while (length > 0) {
	int c = *text;
	text++, length--;

	switch (state) {
	  case 0:		       /* expect `=' of initial `=?' */
	    if (c != '=') return 0;
	    state = 1;
	    break;
	  case 1:		       /* expect separating `?' */
	    if (c != '?') return 0;
	    state++;
	    break;
	  case 2: case 3:	       /* expect token, or separating `?' */
	    if (c <= ' ' || c > '~' ||
		c == '(' || c == ')' ||
		c == '<' || c == '>' ||
		c == '[' || c == ']' ||
		c == '@' ||
		c == ',' ||
		c == ';' ||
		c == ':' ||
		c == '/' ||
		c == '.' ||
		c == '=' ||
		c == '\\' ||
		c == '"')
		return 0;
	    if (c == '?')
		state++;
	    break;
	  case 4:		       /* expect any non-space */
	    if (c <= ' ' || c > '~')
		return 0;
	    if (c == '?')
		state++;
	    break;
	  case 5:		       /* expect final `=' */
	    if (c != '=')
		return 0;
	    return text - orig_text;
	}
    }

    /*
     * If we fall out of this loop, it's because what looked like a
     * valid encoded word up to now has just abruptly ended.
     */
    return 0;
}

/*
 * This function takes as input a piece of header text in which it
 * is known that RFC2047 encoded-words are intended to be
 * interpreted. (Thus, it may be a small phrase or comment out of a
 * larger actual header.)
 * 
 * It works its way along the header, finding and decoding encoded-
 * words and passing them to the provided output function (always
 * with type TYPE_HEADER_TEXT). Any text which isn't part of an
 * encoded-word is output using the provided default charset.
 */
void rfc2047(const char *text, int length, parser_output_fn_t output,
	     int structured, int default_charset)
{
    while (length > 0) {
	const char *startpoint = text;
	int elen;

	/*
	 * Every time we come round to the top of this loop, it's
	 * because we're at the beginning of a word, and hence an
	 * encoded-word might start here.
	 */
	elen = spot_encoded_word(text, length);
	if (elen) {
	    /*
	     * Found an encoded word. Break it up into its
	     * component parts and decode it.
	     */
	    char *wbuf1, *wbuf2, *cset, *enc, *data, *p;
	    int dlen, tlen, charset;

	    wbuf1 = smalloc(elen);

	    memcpy(wbuf1, text, elen);

	    cset = p = wbuf1 + 2;      /* skip the initial `=?' */
	    while (*p != '?') p++;
	    assert(p - wbuf1 < elen);
	    *p++ = '\0';

	    enc = p;
	    while (*p != '?') p++;
	    assert(p - wbuf1 < elen);
	    *p++ = '\0';

	    data = p;
	    dlen = elen - (p - wbuf1) - 2;   /* avoid trailing `?=' */
	    assert(dlen > 0);

	    charset = charset_from_mimeenc(cset);

	    if (!strcmp(enc, "B") || !strcmp(enc, "b")) {
		wbuf2 = smalloc(base64_decode_length(dlen));
		tlen = base64_decode(data, dlen, wbuf2);
	    } else if (!strcmp(enc, "Q") || !strcmp(enc, "q")) {
		wbuf2 = smalloc(dlen);
		tlen = qp_decode(data, dlen, wbuf2, TRUE);
	    } else {
		wbuf2 = NULL;
		tlen = 0;
	    }

	    if (tlen > 0)
		output(wbuf2, tlen, TYPE_HEADER_TEXT, charset);
	    if (wbuf2)
		sfree(wbuf2);

	    sfree(wbuf1);

	    text += elen;
	    length -= elen;
	}

	/*
	 * We've either finished with the encoded-word, or there
	 * wasn't one. No encoded word here. Proceed to the next
	 * whitespace, and past it, and then output.
	 * 
	 * Special case: if we did see an encoded-word, and the
	 * whitespace following it is immediately followed by
	 * another encoded-word, we may avoid outputting the
	 * whitespace. If `structured' is set, this is from a
	 * structured field so we always avoid outputting the
	 * whitespace; if not, it's from an unstructured field so
	 * we only skip the whitespace if it is precisely "\n ".
	 * (This is RFC2047's way of allowing multiple separate
	 * encoded-words to represent contiguous pieces of text.)
	 */
	startpoint = text;
	while (length > 0 &&
	       (*text != ' ' && *text != '\t' && *text != '\n'))
	    text++, length--;
	while (length > 0 &&
	       (*text == ' ' || *text == '\t' || *text == '\n'))
	    text++, length--;
	if (elen > 0 &&
	    (structured ||
	     (text - startpoint == 2 &&
	      startpoint[0] == '\n' &&
	      startpoint[1] == ' ')) &&
	    spot_encoded_word(text, length))
	    /* do not output anything */;
	else if (text - startpoint > 0)
	    output(startpoint, text-startpoint, TYPE_HEADER_TEXT,
		   default_charset);
    }
}

#ifdef TESTMODE

/*
gcc -g -DTESTMODE -Icharset -o rfc2047{,.c} \
    build/{base64,qp,cs-mimeenc,malloc}.o
 */

void fatal(int code, ...) { abort(); }

void test_output_fn(const char *text, int len, int type, int charset)
{
    printf("%d (%d) <%.*s>\n", type, charset, len, text);
}

#define TEST(s) do { \
    printf("Testing >%s<\n", s); \
    rfc2047(s, sizeof(s)-1, test_output_fn, TRUE, CS_ASCII); \
} while (0)

int main(void)
{
    TEST("=?ISO-2022-JP?B?QmVzdCBIYXBwaW5lc3M=?=");
    TEST("=?iso-8859-1?q?J=FCrgen?= Fischer");
    TEST("=?US-ASCII?Q?Keith_Moore?=");
    TEST("=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\n "
	 "=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=");
    /* ... but separators other than "\n " are still visible ... */
    TEST("=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\n  "
	 "=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=");
    TEST("=?ISO-8859-1?Q?a?=");
    TEST("=?ISO-8859-1?Q?a?= b");
    TEST("=?ISO-8859-1?Q?a?= =?ISO-8859-1?Q?b?=");
    TEST("=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=");
    TEST("=?ISO-8859-1?Q?a?=\n    =?ISO-8859-1?Q?b?=");
    TEST("=?ISO-8859-1?Q?a_b?=");
    TEST("=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=");
}

#endif
