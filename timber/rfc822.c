/*
 * Parser for e-mail messages (RFC 822, 2822, 2047 at least,
 * probably others are involved).
 */

/*
 * IMPORTANT: We expect line endings to have been converted to
 * Unix/C internal format (\n only) by the time a message is given
 * to this parser. Converting network transfer format or DOS format
 * (\r\n), or any other OS's local convention, into this form is
 * the job of whatever code imported the message data in the first
 * place.
 */

#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "timber.h"
#include "charset.h"

/*
 * We use the RFC 2822 definition of whitespace, rather than
 * relying on ctype.h.
 */
#define WSP(c) ( (c)==' ' || (c)=='\t' )
#define FWS(c) ( WSP(c) || (c)=='\n' )

/*
 * Type definitions and forward references.
 */
struct header_type {
    /* Special case: header_name might begin with `*'
     * to indicate it's a prefix. */
    const char *header_name;
    enum {
	NO_ENCODED,
	    ENCODED_COMMENTS,
	    ENCODED_CPHRASES,
	    ENCODED_ANYWHERE
    } rfc2047_location;
    enum {
	DO_NOTHING,
	    EXTRACT_ADDRESSES,
	    EXTRACT_MESSAGE_IDS,
	    EXTRACT_DATE,
	    EXTRACT_SUBJECT,
	    CONTENT_TYPE,
	    CONTENT_TRANSFER_ENCODING,
	    CONTENT_DESCRIPTION,
    } action;
};
struct lexed_header {
    /*
     * A pointer to a dynamically allocated array of these
     * structures is returned from lex_header(). Each element of
     * the array describes a single token of the message, _plus_
     * the comment immediately following that token if any.
     */
    int is_punct;
    char const *token;		       /* points into input string */
    int length;
    char const *comment;
    int clength;
};
struct mime_details {
    /*
     * This structure describes details of a single MIME part.
     */
    char const *major, *minor;	       /* the two parts of the MIME type */
    int majorlen, minorlen;	       /* and the length fields */
    enum { NONE, QP, BASE64 } transfer_encoding;
    int charset;
};

struct lexed_header *lex_header(char const *header, int length, int type);
void init_mime_details(struct mime_details *md);
void parse_content_type(struct lexed_header *lh, struct mime_details *md);
int unquote(const char *in, int inlen, int dblquot, char *out);

int istrlencmp(const char *s1, int l1, const char *s2, int l2);

/*
 * This is the main message-parsing function. It produces two
 * outputs:
 * 
 *  - To the `output' function, it passes the entire text of the
 *    message, piece by piece. Each piece is annotated with a type
 *    (`header', `body') and a charset. The idea is that the
 *    implementation of the output function can construct a version
 *    of the message designed for displaying to a user on a
 *    terminal with a fixed charset, and also potentially highlight
 *    the headers and other structurally significant parts of the
 *    message.
 * 
 *  - To the `info' function, it passes a sequence of pieces of
 *    information about the message. Among these are the date, the
 *    text of the Subject line (if any), all email addresses
 *    mentioned in To, From, Cc headers etc, and the Message-ID of
 *    the parent message (from In-Reply-To) if available.
 */
void parse_message(const char *msg, int len,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx)
{
    /*
     * Begin at the beginning, and expect to parse headers until we
     * see a blank line.
     * 
     * We are not intending to be a _strict_ RFC822 parser here: if
     * we receive a malformatted mail, our goal is to make as much
     * sense of it as we still can, not to refuse to read it at
     * all. Thus, we skip over any line containing something that
     * doesn't look like a proper header. In particular, it is
     * entirely possible that we will be passed a message extracted
     * from an mbox folder, in which case the `From ' separator
     * line will still be at the top and we should definitely not
     * choke on that.
     */

    const char *p, *q, *r, *s, *message;
    int pass, msglen;
    struct mime_details toplevel_part;
    int default_charset = CS_CP1252;

    init_mime_details(&toplevel_part);

    for (pass = 0; pass < 2; pass++) {
	/*
	 * We make two passes over the message header section. In
	 * the first pass, we only look for a Content-Type header,
	 * and if we find one we parse it to see if it mentions a
	 * character set. If so, and if this character set is a
	 * reasonable superset of ASCII (i.e. not a 7-bit transport
	 * format such as HZ or UTF-7), we adopt it as the default
	 * character set in which to parse any text in the headers.
	 * This is to deal with evil producers which assume the
	 * target MUA is in the same character set as them and
	 * hence feel free to put 8-bit text in headers, _but_
	 * which also have the minimal sanity to leave a character
	 * set header so that the reader can at least guess how the
	 * 8-bit text should be interpreted.
	 * 
	 * In the second pass, we do the main header processing and
	 * are now able to output a best-effort charset-translated
	 * form of each header.
	 */
	message = msg;
	msglen = len;
	p = message;

	if (pass == 1) {
	    if (toplevel_part.charset != CS_ASCII &&
		charset_contains_ascii(toplevel_part.charset))
		default_charset = toplevel_part.charset;
	}

	while (msglen > 0) {
	    if (*message == '\n') {
		/*
		 * \n at the start of a header line means we are
		 * looking at the blank line separating headers from
		 * body. Advance past that newline, exit this loop, and
		 * go on to the message body.
		 */
		message++, msglen--;
		break;
	    }

	    /*
	     * Find the word introducing the header line. We expect a
	     * number of non-whitespace characters, followed by a colon.
	     */
	    p = message;
	    while (msglen > 0 && *message != ':' &&
		   *message != '\n' && !WSP(*message))
		message++, msglen--;

	    if (msglen <= 0 || *message != ':') {
		/*
		 * This line is not a well-formed header. Skip it and
		 * move on.
		 */
		while (msglen > 0 && *message != '\n')
		    message++, msglen--;   /* scan up to \n */
		if (pass == 1) {
		    /*
		     * It may be a bogus header, but we still need
		     * to translate and output it. The line runs
		     * from p to message.
		     */
		    output(outctx, p, message-p,
			   TYPE_HEADER_TEXT, default_charset);
		    output(outctx, "\n", 1, TYPE_HEADER_TEXT, CS_ASCII);
		}
		if (msglen > 0)
		    message++, msglen--;   /* eat the \n itself */
		continue;
	    }

	    /*
	     * Find the full extent of the header line, by
	     * searching to the next newline not followed by
	     * whitespace.
	     * 
	     * (We can't actually remove the newlines at this
	     * stage, because we may need to know where they were
	     * later for RFC2047 purposes - two RFC2047 encoded
	     * words separated by exactly (newline, space) must be
	     * concatenated without a separating space.)
	     * 
	     * We are currently sitting on the colon.
	     */
	    q = message;
	    message++;		       /* eat the colon */
	    while (msglen > 0 && WSP(*message)) message++;   /* eat WSP */
	    r = message;
	    while (1) {
		while (msglen > 0 && *message != '\n')
		    message++, msglen--;
		if (msglen < 2 || !WSP(message[1]))
		    break;	       /* this is end of header */
		message++, msglen--;   /* eat the newline */
	    }

	    /*
	     * We have a header. Process it.
	     * 
	     * p points to the start of the header line. q points
	     * to the end of the header _name_. r points to the
	     * start of the header text (i.e. between q and r is
	     * the colon and any whitespace). `message' points to
	     * the end of the header (just before the final
	     * newline).
	     * 
	     * Slight ick: this block is enclosed in `do while (0)'
	     * so that `continue' will leave it early.
	     */

#ifdef DIAGNOSTICS
	    printf("Got header \"%.*s\" = \"%.*s\"\n", q-p, p, message-r, r);
#endif

	    do {
		static const struct header_type headers[] = {
		    /* These are sorted in (case-insensitive) order. */
		    {"Bcc", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Cc", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Comments", ENCODED_ANYWHERE, DO_NOTHING},
		    {"Content-Description", ENCODED_ANYWHERE,
			    CONTENT_DESCRIPTION},
		    {"Content-ID", ENCODED_COMMENTS, DO_NOTHING},
		    {"Content-Transfer-Encoding", ENCODED_COMMENTS,
			    CONTENT_TRANSFER_ENCODING},
		    {"Content-Type", ENCODED_COMMENTS, CONTENT_TYPE},
		    {"Date", ENCODED_COMMENTS, EXTRACT_DATE},
		    {"From", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"In-Reply-To", ENCODED_COMMENTS, EXTRACT_MESSAGE_IDS},
		    {"Message-ID", ENCODED_COMMENTS, EXTRACT_MESSAGE_IDS},
		    {"Received", ENCODED_COMMENTS, DO_NOTHING},
		    {"References", ENCODED_COMMENTS, EXTRACT_MESSAGE_IDS},
		    {"Reply-To", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-Bcc", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-Cc", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-Date", ENCODED_COMMENTS, DO_NOTHING},
		    {"Resent-From", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-Message-ID", ENCODED_COMMENTS, DO_NOTHING},
		    {"Resent-Reply-To", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-Sender", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Resent-To", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Return-Path", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Sender", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"Subject", ENCODED_ANYWHERE, EXTRACT_SUBJECT},
		    {"To", ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		    {"*X-", ENCODED_ANYWHERE, DO_NOTHING},
		};
		static const struct header_type null_header = {
		    "<other>", NO_ENCODED, DO_NOTHING
		};
		int bot, mid, top;
		const char *name, *ourname;
		int namelen, ourlen, prefix, cmp;
		const struct header_type *hdr;
		struct lexed_header *lh;

		bot = -1;
		top = lenof(headers);
		hdr = &null_header;
		while (top - bot > 1) {
		    mid = (top + bot) / 2;
		    name = headers[mid].header_name;
		    if (*name == '*') {
			name++;
			prefix = TRUE;
		    } else {
			prefix = FALSE;
		    }
		    namelen = strlen(name);

		    ourname = p;
		    ourlen = q - p;

		    while (namelen > 0 && ourlen > 0) {
			cmp = tolower(*name) - tolower(*ourname);
			if (cmp)
			    break;
			name++, namelen--;
			ourname++, ourlen--;
		    }
		    if (cmp == 0 && namelen == 0 && (prefix || ourlen == 0)) {
			/*
			 * We have a match.
			 */
			hdr = &headers[mid];
			break;
		    }
		    if (cmp < 0 || namelen == 0)
			bot = mid;
		    else
			top = mid;
		}

		/*
		 * We perform semantic and output header parsing
		 * entirely separately, since they are largely
		 * orthogonal; for example, when parsing an
		 * In-Reply-To header for semantics the important
		 * thing is the message-ids, and comments are to be
		 * ignored, but when parsing the same header for
		 * output the comments are the only _interesting_
		 * bit in terms of RFC2047 encoding.
		 */

		/*
		 * Abandon this header entirely and move straight
		 * on to the next one if there isn't _something_
		 * interesting in the header. Everything is
		 * interesting during pass 1, but during pass 0 we
		 * only care about Content-Type.
		 */
		if (pass == 0 && hdr->action != CONTENT_TYPE)
		    continue;

		/*
		 * Perform semantic parsing.
		 */
		lh = lex_header(r, message - r, hdr->action);
#ifdef DIAGNOSTICS
		{
		    int i;
		    for (i = 0; lh[i].token; i++) {
			printf("%d [%.*s] (%.*s)\n",
			       lh[i].is_punct,
			       lh[i].length, lh[i].token,
			       lh[i].clength, lh[i].comment);
		    }
		}
#endif
		switch (hdr->action) {
		  case CONTENT_TYPE:
		    parse_content_type(lh, &toplevel_part);
		    break;
		  default:
		    break;
		}
		sfree(lh);

		/*
		 * Parse the header for output. The _semantic_
		 * header parsing is pretty much orthogonal to this
		 * display-prettification, and so we will do that
		 * separately.
		 */
		if (pass == 0)
		    continue;

		s = r;
		output(outctx, p, r - p, TYPE_HEADER_NAME, CS_ASCII);
		switch (hdr->rfc2047_location) {
		  case NO_ENCODED:
		    output(outctx, r, message - r, TYPE_HEADER_TEXT,
			   default_charset);
		    break;
		  case ENCODED_ANYWHERE:
		    rfc2047(r, message - r, output, outctx,
			    FALSE, default_charset);
		    break;
		  case ENCODED_COMMENTS:
		    /*
		     * I believe this much is general to any
		     * structured header in which comments are
		     * permitted:
		     *
		     *  - If we see an open paren, we parse a
		     *    comment, which means we scan up to
		     *    the closing paren (dodging
		     *    backslash-quoted characters on the
		     *    way), and feed the inside of the
		     *    parens to rfc2047().
		     *
		     *  - If we see a double quote, we parse a
		     *    quoted string, which means we scan up
		     *    to the closing quote (dodging
		     *    backslash-quoted characters on the
		     *    way of course) and pass that lot
		     *    straight to output().
		     */
		    p = r;
		    while (r < message) {
			if (*r == '(') {
			    r++;
			    output(outctx, p, r-p, TYPE_HEADER_TEXT,
				   default_charset);
			    p = r;
			    while (r < message && *r != ')') {
				if (*r == '\\' && r+1 < message)
				    r++;
				r++;
			    }
			    rfc2047(p, r-p, output, outctx,
				    TRUE, default_charset);
			    p = r;
			} else if (*r == '"') {
			    r++;
			    while (r < message && *r != '"') {
				if (*r == '\\' && r+1 < message)
				    r++;
				r++;
			    }
			} else if (*r == '\\' && r+1 < message) {
			    r += 2;
			} else {
			    r++;
			}
		    }
		    if (r > p)
			output(outctx, p, r-p, TYPE_HEADER_TEXT,
			       default_charset);
		    break;
		  case ENCODED_CPHRASES:
		    /*
		     * This is the most complex case for
		     * display parsing. The syntax is:
		     *
		     *  - a `mailbox' is either a bare address,
		     *    or a potentially-RFC2047able-phrase
		     *    followed by an address in angle
		     *    brackets.
		     *
		     *  - an `address' is either a mailbox, or
		     *    a group. A group is a potentially-
		     *    RFC2047able-phrase, a colon, a comma-
		     *    separated list of mailboxes, and a
		     *    final semicolon.
		     *
		     *  - an `address-list' is a comma-
		     *    separated list of addresses.
		     *
		     * In theory there is also a `mailbox-list'
		     * which is a comma-separated list of
		     * mailboxes; this is used in some address
		     * headers. For example, `Reply-To' takes
		     * an address-list and hence may contain
		     * groups, but `From' takes a mailbox-list
		     * and cannot.
		     *
		     * However, I'm going to ignore this
		     * distinction; anyone using group syntax
		     * in a From header coming into this parser
		     * will find that the parser will DWIM.
		     * This doesn't seem to me to be a large
		     * practical problem, and it makes my life
		     * easier.
		     *
		     * So what I actually do here is:
		     *
		     *  - Place a marker.
		     *
		     *  - Scan along the string, dodging
		     *    quoted-strings, backslash-quoted
		     *    characters and comments, until we see
		     *    a significant punctuation event.
		     *    Significant punctuation events are
		     *    `:', `;', `<', `>', `,' and
		     *    end-of-string.
		     *
		     *  - If we see `:' or `<', then everything
		     *    between the marker and here is part
		     *    of a potentially-RFC2047able phrase.
		     *    Whereas if we see `>', `,', `;' or
		     *    end-of-string, everything between the
		     *    marker and here was an addr-spec or
		     *    nothing at all.
		     *
		     *  - So now we re-scan from the marker.
		     *    Anything in a quoted-string we pass
		     *    through unchanged; anything in parens
		     *    we parse as an RFC2047able comment;
		     *    anything not in either we deal with
		     *    _as appropriate_ given the above.
		     */
		    while (r < message) {
			int rfc2047able;
			p = r;
			while (r < message &&
			       *r != ':' && *r != ';' &&
			       *r != '<' && *r != '>' && *r != ',') {
			    if (*r == '(' || *r == '"') {
				int end = (*r == '"' ? '"' : ')');
				r++;
				while (r < message && *r != end) {
				    if (*r == '\\' && r+1 < message)
					r++;
				    r++;
				}
			    } else if (*r == '\\' && r+1 < message) {
				r += 2;
			    } else {
				r++;
			    }
			}
			if (r < message && (*r == ':' || *r == '<'))
			    rfc2047able = TRUE;
			else
			    rfc2047able = FALSE;
			if (r < message)
			    r++;   /* we'll eat this punctuation too */
			/*
			 * Now re-scan from p.
			 */
			q = p;
			while (1) {
			    if (q < r && *q == '\\' && q+1 < r) {
				q += 2;
			    } else if (q < r && *q != '(' && *q != '"') {
				q++;
			    } else {
				int end;
				if (rfc2047able)
				    rfc2047(p, q-p, output, outctx,
					    TRUE, default_charset);
				else
				    output(outctx, p, q-p,
					   TYPE_HEADER_TEXT,
					   default_charset);
				if (q == r)
				    break;
				output(outctx, q, 1,
				       TYPE_HEADER_TEXT, CS_ASCII);
				end = (*q == '"' ? '"' : ')');
				p = ++q;
				while (q < r && *q != end) {
				    if (*q == '\\' && q+1 < r)
					q++;
				    q++;
				}
				if (end == ')')
				    rfc2047(p, q-p, output, outctx,
					    TRUE, default_charset);
				else
				    output(outctx, p, q-p,
					   TYPE_HEADER_TEXT,
					   default_charset);
				if (q == r)
				    break;
				output(outctx, q, 1,
				       TYPE_HEADER_TEXT, CS_ASCII);
				p = ++q;
			    }
			}
		    }
		    break;
		}
		output(outctx, "\n", 1, TYPE_HEADER_TEXT, CS_ASCII);
		r = s;
	    } while (0);

	    /*
	     * Eat the newline after the header.
	     */
	    if (msglen > 0) {
		assert(*message == '\n');
		message++, msglen--;
	    }
	}
    }
}

struct lexed_header *lex_header(char const *header, int length, int type)
{
    struct lexed_header *ret = NULL;
    struct lexed_header *tok;
    int retlen = 0, retsize = 0;
    char const *punct, *p;

    /*
     * The interesting punctuation elements vary with the type
     * of header.
     */
    switch (type) {
      default: punct = ""; break;
      case EXTRACT_ADDRESSES: punct = ":;<>,"; break;
      case EXTRACT_MESSAGE_IDS: punct = "<>"; break;
      case EXTRACT_DATE: punct = ",:"; break;
      case CONTENT_TYPE: punct = "/;="; break;
    }

    tok = NULL;

    while (1) {
	/*
	 * Eat whitespace and comments.
	 */
	while (length > 0) {
	    if (*header == '(') {
		header++, length--;
		if (tok)
		    tok->comment = header;
		while (length > 0 && *header != ')') {
		    if (*header == '\\' && length > 1)
			header++, length--;
		    header++, length--;
		}
		if (tok)
		    tok->clength = header - tok->comment;
		tok = NULL;	       /* any further comments are ignored */
	    } else if (!FWS(*header)) {
		break;
	    }
	    header++, length--;	       /* eat FWS or the closing `)' */
	}

	/*
	 * Allocate space for a token structure. Even if this is
	 * end-of-string, we will need an array element to say so.
	 */
	if (retlen >= retsize) {
	    retsize = retlen + 64;
	    ret = srealloc(ret, retsize * sizeof(*ret));
	}
	tok = &ret[retlen];
	retlen++;

	if (length <= 0) {
	    /*
	     * End of string. Mark a special array element with
	     * token==NULL and length==0.
	     */
	    tok->token = tok->comment = NULL;
	    tok->length = tok->clength = 0;
	    tok->is_punct = FALSE;
	    break;
	}

	/*
	 * Now we have a genuine token.
	 */
	tok->token = header;
	tok->comment = NULL;
	tok->clength = 0;

	/*
	 * See if it's punctuation.
	 */
	tok->is_punct = FALSE;
	for (p = punct; *p; p++)
	    if (*p == *header) {
		tok->is_punct = TRUE;
		break;
	    }
	if (tok->is_punct) {
	    tok->length = 1;
	    header++, length--;
	    continue;
	}

	/*
	 * Otherwise, we have a non-punctuation token; follow it
	 * all the way to the next FWS, comment introducer or
	 * significant punctuation, while dodging quoted strings
	 * and quoted pairs.
	 */
	while (length > 0 && !FWS(*header) && *header != '(') {
	    if (*header == '\\' && length > 1) {
		header++, length--;
	    } else if (*header == '"') {
		header++, length--;
		while (length > 0 && *header != '"') {
		    if (*header == '\\' && length > 1)
			header++, length--;
		    header++, length--;
		}
		assert(length == 0 || *header == '"');
	    } else {
		for (p = punct; *p; p++)
		    if (*p == *header)
			break;
		if (*p)
		    break;
	    }
	    header++, length--;
	}
	tok->length = header - tok->token;
    }

    return ret;
}

void init_mime_details(struct mime_details *md)
{
    md->major = "text";
    md->minor = "plain";
    md->majorlen = 4;
    md->minorlen = 5;
    md->transfer_encoding = NONE;
    md->charset = CS_ASCII;
}

void parse_content_type(struct lexed_header *lh, struct mime_details *md)
{
    /*
     * If the first three tokens are a word, a slash and another
     * word, we can collect the MIME type itself.
     */
    if ((!lh[0].is_punct && lh[0].length > 0) &&
	( lh[1].is_punct && lh[1].length > 0 && *lh[1].token == '/') &&
	(!lh[2].is_punct && lh[2].length > 0)) {
	md->major = lh[0].token;
	md->majorlen = lh[0].length;
	md->minor = lh[2].token;
	md->minorlen = lh[2].length;
	lh += 3;
    }

    /*
     * Now scan the rest of the header for semicolons. After each
     * semicolon, we look to see if there's a word, an equals sign
     * and some other stuff; if so, we process a parameter.
     */
    while (lh[0].length > 0) {
	if (!lh[0].is_punct || *lh[0].token != ';') {
	    lh++;
	    continue;
	}

	/*
	 * We have a semicolon.
	 */
	lh++;
	if ((!lh[0].is_punct && lh[0].length > 0) &&
	    ( lh[1].is_punct && lh[1].length > 0 && *lh[1].token == '=') &&
	    (!lh[2].is_punct && lh[2].length > 0)) {
	    /*
	     * We have a `param=value' construction. See if the
	     * parameter is one we know how to handle.
	     */
	    if (!istrlencmp(lh[0].token, lh[0].length, "charset", 7)) {
		char *cset = smalloc(1 + lh[2].length);
		int csl;

		csl = unquote(lh[2].token, lh[2].length, TRUE, cset);
		assert(csl <= lh[2].length);
		cset[csl] = '\0';

		md->charset = charset_upgrade(charset_from_mimeenc(cset));
	    }

	    lh += 4;
	}
    }
}

/*
 * Remove backslash-quoted pairs, and optionally double quotes too,
 * from a string. Returns the output length.
 */
int unquote(const char *in, int inlen, int dblquot, char *out)
{
    char *orig_out = out;

    /*
     * Entertainingly, we don't even need to _track_ whether we're
     * inside a quoted string! The only function of quotes is to
     * alter token boundaries, and by the time we come here the
     * token boundaries have already been settled. So all we need
     * to do here is to drop any unbackslashed double quote
     * characters.
     */
    while (inlen > 0) {
	if (dblquot && *in == '"') {
	    in++, inlen--;
	    continue;
	} else if (*in == '\\' && inlen > 1) {
	    in++, inlen--;
	}
	*out++ = *in;
	in++, inlen--;
    }

    return out - orig_out;
}

int istrlencmp(const char *s1, int l1, const char *s2, int l2)
{
    int cmp;

    while (l1 > 0 && l2 > 0) {
	cmp = tolower(*s1) - tolower(*s2);
	if (cmp)
	    return cmp;
	s1++, l1--;
	s2++, l2--;
    }

    /*
     * We've reached the end of one or both strings.
     */
    if (l1 > 0)
	return +1;		       /* s1 is longer, therefore is greater */
    else if (l2 > 0)
	return -1;		       /* s2 is longer */
    else
	return 0;
}

void null_output_fn(void *ctx, const char *text, int len,
		    int type, int charset)
{
}

void null_info_fn(void *ctx, int type, const char *text, int len)
{
}

/* FIXME: should this really be in here, or should it go in another module?
 * It is, after all, a _client_ of the parser code. */
void db_info_fn(void *ctx, int type, const char *text, int len)
{
    /* FIXME */
}
void parse_for_db(const char *message, int msglen)
{
    parse_message(message, msglen, null_output_fn, NULL, db_info_fn, NULL);
}

#ifdef TESTMODE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

/*
gcc -Wall -g -DTESTMODE -Icharset -o rfc822{,.c} \
    build/{base64,qp,cs-*,malloc,rfc2047}.o
 */

void fatal(int code, ...) { abort(); }

void test_output_fn(void *outctx, const char *text, int len,
		    int type, int charset)
{
    /* printf("%d (%d) <%.*s>\n", type, charset, len, text); */
    charset_state instate = CHARSET_INIT_STATE;
    charset_state outstate = CHARSET_INIT_STATE;
    wchar_t midbuf[256];
    wchar_t const *midptr;
    char outbuf[256];
    int inret, midret, midlen;

    if (type == TYPE_HEADER_NAME)
	printf("\033[1;31m");

    while ( (inret = charset_to_unicode(&text, &len, midbuf,
					lenof(midbuf), charset,
					&instate, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
					       lenof(outbuf), CS_UTF8,
					       &outstate, NULL, 0)) > 0) {
	    fwrite(outbuf, 1, midret, stdout);
	}
    }
    if (type == TYPE_HEADER_NAME)
	printf("\033[39;0m");
}

void test_info_fn(void *infoctx, int type, const char *text, int len)
{
    printf("%d: <%.*s>\n", type, len, text);
}

int main(void)
{
    char *message = NULL;
    int msglen = 0, msgsize = 0;

    while (1) {
	int ret, i, j;

	if (msgsize - msglen < 1024) {
	    msgsize = msglen + 1024;
	    message = realloc(message, msgsize);
	}

	ret = read(0, message + msglen, msgsize - msglen);
	if (ret < 0) {
	    perror("read");
	    return 1;
	}
	if (ret == 0)
	    break;

	for (i = j = 0; i < ret; i++) {
	    if (message[msglen + i] != '\r')
		message[msglen + j++] = message[msglen + i];
	}
	msglen += j;
    }

    /*
     * First test the info gathering.
     */
    parse_message(message, msglen, null_output_fn, NULL, test_info_fn, NULL);

    /*
     * Now test the post-parse output.
     */
    parse_message(message, msglen, test_output_fn, NULL, null_info_fn, NULL);

    return 0;
}

#endif
