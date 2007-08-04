/*
 * RFC2047 encoded header parser for Timber.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "timber.h"
#include "charset.h"

/*
 * We use the RFC 2822 definition of whitespace, rather than
 * relying on ctype.h.
 */
#define WSP(c) ( (c)==' ' || (c)=='\t' )
#define FWS(c) ( WSP(c) || (c)=='\n' )

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
	  case 4:		       /* expect any non-control char */
	    if (c >= '\0' && c < ' ')
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
 * words and passing them to the provided output function. Any text
 * which isn't part of an encoded-word is output using the provided
 * default charset.
 */
void rfc2047_decode(const char *text, int length, parser_output_fn_t output,
		    void *outctx, int type, int display,
		    int default_charset)
{
    int quoting = FALSE;

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

	    wbuf1 = snewn(elen, char);

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
		wbuf2 = snewn(base64_decode_length(dlen), char);
		tlen = base64_decode(data, dlen, (unsigned char *)wbuf2);
	    } else if (!strcmp(enc, "Q") || !strcmp(enc, "q")) {
		wbuf2 = snewn(dlen, char);
		tlen = qp_decode(data, dlen, wbuf2, TRUE);
	    } else {
		wbuf2 = NULL;
		tlen = 0;
	    }

	    if (tlen > 0) {
		/*
		 * Contents of an encoded-word should be output
		 * _almost_ unchanged. The sole exception is that
		 * we disallow (omit) newlines.
		 */
		const char *p, *q;
		p = wbuf2;
		while (p < wbuf2 + tlen) {
		    q = p;
		    while (q < wbuf2 + tlen && *q != '\n')
			q++;
		    output(outctx, p, q - p, type, charset);
		    while (q < wbuf2 + tlen && *q == '\n')
			q++;
		    p = q;
		}
	    }
	    if (wbuf2)
		sfree(wbuf2);

	    sfree(wbuf1);

	    text += elen;
	    length -= elen;
	}

	/*
	 * We've either finished with the encoded-word, or there
	 * wasn't one. No encoded word here. Proceed to the next
	 * potential one (= sign), and then output.
	 * 
	 * Special case: if we did see an encoded-word, and the
	 * whitespace following it is immediately followed by
	 * another encoded-word, we may avoid outputting the
	 * whitespace. If `type' indicates this is from a
	 * structured field, we always avoid outputting the
	 * whitespace; if not, it's from an unstructured field so
	 * we only skip the whitespace if it is precisely "\n ".
	 * (This is RFC2047's way of allowing multiple separate
	 * encoded-words to represent contiguous pieces of text.)
	 * 
	 * DELIBERATE DEPARTURE FROM STANDARD: RFC2047 is very
	 * clear that a quoted string MUST NOT contain
	 * encoded-words, and also that encoded-words MUST begin at
	 * word boundaries. However, it is the experience of my
	 * large and varied mail archive that both these
	 * constraints are frequently violated, and it is my
	 * opinion that the best indication of the email author's
	 * intent is given by decoding them anyway. (Of course, I
	 * wouldn't dream of _generating_ an encoded-word in such
	 * circumstances; that would be evil on entirely another
	 * level.)
	 */
	if (text != startpoint) {
	    startpoint = text;
	} else {
	    assert(length > 0);
	    text++, length--;
	}
	while (length > 0 && *text != '=')
	    text++, length--;
	if (elen > 0 &&
	    (type != TYPE_HEADER_DECODED_TEXT ||
	     (text - startpoint == 2 &&
	      startpoint[0] == '\n' &&
	      startpoint[1] == ' ')) &&
	    spot_encoded_word(text, length))
	    /* do not output anything */;
	else if (text - startpoint > 0) {
	    if (display) {
		output(outctx, startpoint, text-startpoint,
		       type, default_charset);
	    } else {
		const char *p = startpoint;
		while (p < text) {
		    if (*p == '\n' ||
			((type != TYPE_HEADER_DECODED_TEXT) &&
			 (*p == '"' || (FWS(*p) && !quoting) ||
			  (*p == '\\' && p+1 < text)))) {
			if (p > startpoint)
			    output(outctx, startpoint, p-startpoint,
				   type, default_charset);
			startpoint = p+1;
			if (*p == '"')
			    quoting = !quoting;
			if (*p == '\\')
			    p++;
			if (FWS(*p)) {
			    /*
			     * When parsing a structured header for
			     * information, I replace any amount of
			     * FWS not in a quoted string by a
			     * single space.
			     */
			    output(outctx, " ", 1, type, CS_ASCII);
			    while (p < text && FWS(*p))
				p++;
			    startpoint = p;
			    p--;       /* so that subsequent p++ is right */
			}
		    }
		    p++;
		}
		if (p > startpoint)
		    output(outctx, startpoint, p-startpoint,
			   type, default_charset);
	    }
	}
    }
}

/*
 * RFC2047 sets the maximum length of an encoded-word at 75
 * characters, and specifies a line length limit of 76 characters
 * for any header containing an encoded-word.
 * 
 * Therefore, I drop the maximum word length to 74, to leave room
 * for one character of punctuation in addition to the initial
 * post-fold space.
 */
char *rfc2047_encode(const char *text, int length, int input_charset,
		     const int *output_charsets, int ncharsets,
		     int type, int firstlen)
{
    wchar_t *wtext;
    int wlen;
    char *atext;
    int i, output_charset;

    /*
     * Begin by converting the input text into wide-character
     * Unicode.
     */
    {
	int wsize;
	charset_state instate = CHARSET_INIT_STATE;
	const char *tptr = text;
	int tlen = length;

	wsize = wlen = 0;
	wtext = NULL;
	while (*tptr) {
	    int ret;

	    if (wlen >= wsize) {
		wsize = wlen + 512;
		wtext = sresize(wtext, wsize, wchar_t);
	    }

	    ret = charset_to_unicode(&tptr, &tlen, wtext+wlen, wsize-wlen-1,
				     input_charset, &instate, NULL, 0);
	    if (ret == 0)
		break;
	    wlen += ret;
	}

	wtext[wlen] = '\0';
    }

    /*
     * Now we attempt to encode the string in each of the supplied
     * charsets.
     * 
     * Our strategy is rather different depending on whether the
     * charset is ASCII or not. If it's ASCII, we simply convert
     * the whole Unicode string to one long ASCII string, and
     * provided there are no conversion errors we simply return the
     * result (after first quoting any difficult punctuation if
     * `type' indicates a structured field). But if not, we must
     * separate it into _multiple_ strings in the output charset,
     * each one self-contained (started from an initial conversion
     * state and cleaned up at the end), each one encoding to an
     * RFC2047 word at most 74 characters long. Or even shorter, if
     * `firstlen' is set and it's the first word.
     */

    for (i = 0; i < ncharsets; i++) {
	int error, *errp;
	int max_enc_len;
	const wchar_t *wp;
	int wl;

	output_charset = output_charsets[i];

	wp = wtext;
	wl = wlen;

	/*
	 * We check for charset conversion errors on all charsets
	 * except the last in the list. When trying the last
	 * charset, we accept a shoddy job if necessary, because we
	 * have no other options.
	 */
	if (i < ncharsets-1)
	    errp = &error;
	else
	    errp = NULL;
	error = FALSE;

	atext = NULL;

	if (output_charset == CS_ASCII) {
	    /*
	     * Just do sensible charset conversion.
	     */

	    int asize, alen;
	    charset_state outstate = CHARSET_INIT_STATE;

	    asize = alen = 0;
	    atext = NULL;

	    while (*wp) {
		int ret;

		if (alen >= asize) {
		    asize = alen + 512;
		    atext = sresize(atext, asize, char);
		}

		ret = charset_from_unicode(&wp, &wl, atext+alen, asize-alen-1,
					   output_charset, &outstate, errp);
		if (ret == 0 || error)
		    break;
		alen += ret;
	    }

	    if (error) {
		sfree(atext);
		continue;
	    }

	    /*
	     * Successful conversion to ASCII. Terminate the
	     * string.
	     */
	    atext[alen] = '\0';

	    /*
	     * Quote difficult characters. According to RFC 2822,
	     * the valid ASCII characters we need not quote are
	     * !#$%&'*+-/=?^_`{|}~ and alphanumerics. (And space,
	     * of course.) So if anything other than that shows up
	     * in the string then we must surround it with double
	     * quotes; and if any double quotes or backslashes show
	     * up within the quoted string then we must
	     * backslash-escape them.
	     * 
	     * In principle, we should also check here for
	     * TYPE_HEADER_DECODED_COMMENT and backslash-escape
	     * parentheses and backslashes. However, in practice
	     * any data we receive here from a comment will have
	     * been output straight from rfc822.c in the display
	     * parsing mode, and in that situation it doesn't
	     * _strip_ those backslashes so we don't need to put
	     * them back on.
	     */
	    if (type == TYPE_HEADER_DECODED_PHRASE &&
		atext[strspn(atext,
			     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			     "abcdefghijklmnopqrstuvwxyz"
			     "0123456789 !#$%&'*+-/=?^_`{|}~")]) {
		char *atext2, *p, *q;

		/*
		 * Leave enough room to backslash every character,
		 * plus the two enclosing quotes and the NUL.
		 */
		atext2 = snewn(strlen(atext) * 2 + 3, char);
		p = atext;
		q = atext2;
		*q++ = '"';
		while (*p) {
		    if (*p == '"' || *p == '\\')
			*q++ = '\\';
		    *q++ = *p++;
		}
		*q++ = '"';
		*q++ = '\0';

		sfree(atext);
		atext = atext2;
	    }

	    /*
	     * ... and finish the conversion loop.
	     */
	    break;

	} else {
	    char const *csname = charset_to_mimeenc(output_charset);
	    int csnamelen = strlen(csname);
	    char *prefix = "";
	    int alen = 0;

	    while (*wp) {
		char rawbuf[100], encbuf[100];
		int rawlen = 0, enclen;
		int blen, qlen, enc;
		charset_state outstate = CHARSET_INIT_STATE;

		/*
		 * Determine the maximum encoded length of this
		 * encoded-word.
		 */
		if (!*prefix) {	       /* this is the first word */
		    assert(firstlen <= 74);
		    max_enc_len = firstlen;
		} else {
		    max_enc_len = 74;
		}
		max_enc_len -= 7 + csnamelen;   /* =?charset?Q?...?= */

		/*
		 * Do the conversion from Unicode, slowly and carefully
		 * to avoid going over the limit.
		 */
		while (*wp) {
		    int ret1, ret2;
		    const wchar_t *wp2;
		    charset_state outstate2, outstate3;
		    int wl;

		    wp2 = wp;
		    wl = 1;
		    outstate2 = outstate;

		    /*
		     * Convert a single character ...
		     */
		    ret1 = charset_from_unicode(&wp2, &wl, rawbuf+rawlen,
						sizeof(rawbuf)-rawlen,
						output_charset, &outstate2,
						errp);
		    if (error)
			break;

		    /*
		     * ... and clean up.
		     */
		    outstate3 = outstate2;
		    ret2 = charset_from_unicode(NULL, 0, rawbuf+rawlen+ret1,
						sizeof(rawbuf)-rawlen-ret1,
						output_charset, &outstate3,
						NULL);

		    /*
		     * If the resulting text is still short enough
		     * to fit in the encoded-word, then we can
		     * afford to let the character in (but leave
		     * out the cleanup data), because we know that
		     * if the _next_ character doesn't fit then we
		     * can still clean up sensibly.
		     * 
		     * So now we need to determine the length of
		     * the encoded word. I'm willing to encode
		     * either base64 or QP, whichever is shorter.
		     * (I expect this to correlate strongly with
		     * human-readability: for a QP string to be
		     * shorter than the base64 equivalent it must
		     * contain mostly printable ASCII, which means
		     * it's better to use QP so that a user of a
		     * non-RFC2047-aware MUA still gets the gist.)
		     */
		    blen = base64_encode_length(rawlen+ret1+ret2, FALSE);
		    qlen = qp_rfc2047_encode(rawbuf, rawlen+ret1+ret2, NULL);
		    if (blen <= max_enc_len || qlen <= max_enc_len) {
			wp = wp2;
			rawlen += ret1;
			outstate = outstate2;
		    } else {
			break;
		    }
		}
		if (error)
		    break;

		/*
		 * We're finished. Clean up.
		 */
		rawlen += charset_from_unicode(NULL, 0, rawbuf+rawlen,
					       sizeof(rawbuf)-rawlen,
					       output_charset, &outstate,
					       NULL);

		/*
		 * Now we have data in rawbuf, length rawlen, which
		 * needs to be turned into an encoded-word.
		 */
		blen = base64_encode_length(rawlen, FALSE);
		qlen = qp_rfc2047_encode(rawbuf, rawlen, NULL);
		if (blen < qlen) {
		    enclen = base64_encode((unsigned char *)rawbuf,
					   rawlen, encbuf, FALSE);
		    enc = 'B';
		    assert(enclen == blen);
		} else {
		    enclen = qp_rfc2047_encode(rawbuf, rawlen, encbuf);
		    enc = 'Q';
		    assert(enclen == qlen);
		}
		assert(enclen <= max_enc_len);
		atext = sresize(atext, alen + 100, char);
		alen += sprintf(atext+alen, "%s=?%s?%c?%.*s?=",
				prefix, csname, enc, enclen, encbuf);
		prefix = "\n ";
	    }

	    if (error) {
		sfree(atext);
		continue;
	    }

	    /*
	     * If we've got here, we have a fully converted string.
	     */
	    break;
	}
    }

    /*
     * And that's it! We should now have `atext' containing our
     * fully encoded string, so we can free wtext and leave.
     */
    sfree(wtext);
    return atext;
}

#ifdef TESTMODE

#include <stdio.h>

/*
make && gcc -Wall -g -DTESTMODE -Icharset -o rfc2047{,.c} \
    build/{base64,qp,cs-*,malloc}.o
 */

void fatal(int code, ...) { abort(); }

void test_output_fn(void *outctx, const char *text, int len,
		    int type, int charset)
{
    printf("%d (%d) <%.*s>\n", type, charset, len, text);
}

#define SL(s) (s) , (sizeof((s))-1)

#define DTEST(s) do { \
    printf("Testing >%s<\n", s); \
    rfc2047_decode(SL(s),test_output_fn,NULL,TRUE,FALSE,CS_ASCII); \
} while (0)

#define ETEST(s,i,o) do { \
    char *text = rfc2047_encode(SL(s),i,o,lenof(o),TRUE,60); \
    printf("--------------: %s\n", text); \
    printf("Decode:\n"); \
    rfc2047_decode(text,strlen(text),test_output_fn,NULL,TRUE,FALSE,CS_ASCII); \
    sfree(text); \
} while (0)

int main(void)
{
    DTEST("=?ISO-2022-JP?B?QmVzdCBIYXBwaW5lc3M=?=");
    DTEST("=?iso-8859-1?q?J=FCrgen?= Fischer");
    DTEST("=?US-ASCII?Q?Keith_Moore?=");
    DTEST("=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\n "
	 "=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=");
    /* ... but separators other than "\n " are still visible ... */
    DTEST("=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\n  "
	 "=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=");
    DTEST("=?ISO-8859-1?Q?a?=");
    DTEST("=?ISO-8859-1?Q?a?= b");
    DTEST("=?ISO-8859-1?Q?a?= =?ISO-8859-1?Q?b?=");
    DTEST("=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=");
    DTEST("=?ISO-8859-1?Q?a?=\n    =?ISO-8859-1?Q?b?=");
    DTEST("=?ISO-8859-1?Q?a_b?=");
    DTEST("=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=");

    {
	static const int charsets[] = { CS_ASCII, CS_ISO8859_1, CS_UTF8 };
	static const int charsets2[] = { CS_ASCII, CS_ISO8859_1,
		CS_ISO2022_JP, CS_UTF8 };
	static const int charsets3[] = { CS_ISO2022_KR, CS_UTF8 };
	ETEST("hello world!", CS_ASCII, charsets);
	ETEST("hello, world", CS_ASCII, charsets);
	ETEST("J\xc3\xb8rgen Fischer", CS_UTF8, charsets);
	ETEST("J\xc3\xb8rgen Fischer is the name of somebody who emailed"
	      " me once. I'm going to go on and on for a bit so that"
	      " this line goes well over the limit.", CS_UTF8, charsets);
	ETEST("\xE4\xB8\xBA\xE6\x82\xA8\xE5\xBB\xBA\xEF\xBC\x88\xE4\xB8\xAD"
	      "\xE8\x8B\xB1\xE6\x96\x87\xEF\xBC\x89\xE4\xBC\x81\xE4\xB8\x9A"
	      "\xE7\xBD\x91\xE7\xAB\x99", CS_UTF8, charsets);
	ETEST("\xE3\x80\x90\xE3\x81\x8F\xE3\x81\x98\xE4\xBB\x98\xE3\x80\x91"
	      "\xE2\x98\x85\xEF\xBC\x94\xE4\xB8\x87\xE9\x83\xA8\xE9\x85\x8D"
	      "\xE4\xBF\xA1\xE3\x82\xA2\xE3\x82\xAF\xE3\x82\xBB\xE3\x82\xB9"
	      "\xE3\x82\xA2\xE3\x83\x83\xE3\x83\x97\xEF\xBC\x81\xE6\xA5\xAD"
	      "\xE7\x95\x8C\xE6\x9C\x80\xE5\xAE\x89\xE5\x80\xA4\xEF\xBC\x81"
	      "\xEF\xBC\x81", CS_UTF8, charsets);
	ETEST("\xE3\x80\x90\xE3\x81\x8F\xE3\x81\x98\xE4\xBB\x98\xE3\x80\x91"
	      "\xE2\x98\x85\xEF\xBC\x94\xE4\xB8\x87\xE9\x83\xA8\xE9\x85\x8D"
	      "\xE4\xBF\xA1\xE3\x82\xA2\xE3\x82\xAF\xE3\x82\xBB\xE3\x82\xB9"
	      "\xE3\x82\xA2\xE3\x83\x83\xE3\x83\x97\xEF\xBC\x81\xE6\xA5\xAD"
	      "\xE7\x95\x8C\xE6\x9C\x80\xE5\xAE\x89\xE5\x80\xA4\xEF\xBC\x81"
	      "\xEF\xBC\x81", CS_UTF8, charsets2);
	ETEST("This is another deliberately long line which is going to go "
	      "over the limit. This time I'm testing to see whether "
	      "ISO-2022-KR text gets the initial sequence everywhere.",
	      CS_UTF8, charsets3);
    }

    return 0;
}

#endif
