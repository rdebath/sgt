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
#include <string.h>
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
 * A convenient way to generate a constant (string,length) pair to
 * pass to functions such as istrlencmp.
 */
#define SL(s) (s) , (sizeof((s))-1)

/*
 * Type definitions and forward references.
 */
struct mime_record {
    /*
     * We are going to need to hold some extra bookkeeping data in
     * a mime_details. So rather than cluttering up the structure
     * definition in timber.h, I'm just going to define a
     * container structure here which adds a couple of extra
     * fields.
     */
    struct mime_record *next;	       /* for linking them in a list */
    const char *rawdescription;	       /* the one in md isn't const */
    int description_len;	       /* for while description is still raw */
    struct mime_details md;
    /*
     * A file name can come from a `name' parameter in Content-
     * Type, or a `filename' parameter in Content-Disposition. The
     * former is deprecated in favour of the latter (as stated in
     * RFC 2046), so the latter takes priority. Hence, we track at
     * all times where our current filename has come from.
     */
    enum { NO_FNAME, CT_NAME, CD_FILENAME } filename_location;
    char *boundary;		       /* dynamically allocated */
};
struct header_type {
    /* Special case: header_name might begin with `*'
     * to indicate it's a prefix. */
    const char *header_name;
    int header_id;
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
	CONTENT_DISPOSITION,
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

void parse_headers(char const *base, char const *message, int msglen,
		   int full_message, int pass, int default_charset,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx,
		   struct mime_record *md);
void recurse_mime_part(const char *base, struct mime_record *md,
		       struct mime_record ***next, int default_charset,
		       parser_info_fn_t info, void *infoctx);
struct lexed_header *lex_header(char const *header, int length, int type);
void init_mime_record(struct mime_record *md, int default_charset);
void free_mime_record(struct mime_record *md);
void parse_content_type(struct lexed_header *lh, struct mime_record *mr);
void parse_content_disp(struct lexed_header *lh, struct mime_record *mr);
void parse_content_transfer(struct lexed_header *lh, struct mime_record *mr);
int unquote(const char *in, int inlen, int dblquot, char *out);
void info_addr(parser_info_fn_t info, void *infoctx,
	       const char *addr, int addrlen,
	       const char *name, int namelen,
	       int header_id, int default_charset);
char *rfc2047_to_utf8_string(const char *text, int len,
			     int structured, int default_charset);
int latoi(const char *text, int len);

/*
 * This is the main message-parsing function. It produces two
 * outputs:
 * 
 *  - To the `output' function, it passes the entire text of the
 *    message, piece by piece. Each piece is annotated with a type
 *    (`header', `body') and a charset. The idea is that the
 *    implementation of the output function can construct a
 *    version of the message designed for displaying to a user on
 *    a terminal with a fixed charset, and also potentially
 *    highlight the headers and other structurally significant
 *    parts of the message.
 * 
 *  - To the `info' function, it passes a sequence of pieces of
 *    information about the message. Among these are the date, the
 *    text of the Subject line (if any), all email addresses
 *    mentioned in To, From, Cc headers etc, the Message-ID of the
 *    parent message (from In-Reply-To) if available, and full
 *    details of the type, encoding, charset and location within
 *    the message of each MIME body part.
 */
void parse_message(const char *message, int msglen,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx,
		   int default_charset)
{
    /*
     * We are not intending to be a _strict_ RFC822 parser here:
     * if we receive a malformatted mail, our goal is to make as
     * much sense of it as we still can, not to refuse to read it
     * at all. Thus, we skip over any line containing something
     * that doesn't look like a proper header.
     */

    int pass;
    struct mime_record *toplevel_part;

    /*
     * `default_charset' might have been passed in to us if we are
     * parsing a message to be sent (in which case we consider it
     * to be in whatever charset the compose buffer was edited
     * in). If we're parsing a received message, a default charset
     * of CS_NONE means we should choose our own sensible default.
     */
    if (default_charset == CS_NONE)
	default_charset = charset_upgrade(CS_ASCII);

    toplevel_part = snew(struct mime_record);

    init_mime_record(toplevel_part, default_charset);

    for (pass = 0; pass < 2; pass++) {
	/*
	 * We make two passes over the message header section. In
	 * the first pass, we look for MIME part headers and parse
	 * them, and then we recurse into multiparts and
	 * message/rfc822 parts to find all the MIME parts in the
	 * message. In the course of this, we make a note if we
	 * find anything that mentions a character set. If so, and
	 * if the first character set mentioned is a reasonable
	 * superset of ASCII (i.e. not a 7-bit transport format
	 * such as HZ or UTF-7), we adopt it as the default
	 * character set in which to parse any text in the
	 * headers. This is to deal with evil producers which
	 * assume the target MUA is in the same character set as
	 * them and hence feel free to put 8-bit text in headers,
	 * _but_ which also have the minimal sanity to leave a
	 * character set header so that the reader can at least
	 * guess how the 8-bit text should be interpreted.
	 * 
	 * In the second pass, we do the main header processing
	 * and are now able to output a best-effort charset-
	 * translated form of each header.
	 * 
	 * There's a slight phase-order problem in that
	 * Content-Description headers are best parsed in the
	 * first pass, but contain arbitrary textual information
	 * that wants to be interpreted in the default charset.
	 * Therefore, the first pass saves the Content-Description
	 * header in its raw form, and after we've finished we go
	 * back through and charset-parse it.
	 */
	parse_headers(message, message, msglen, TRUE, pass, default_charset,
		      output, outctx, info, infoctx, toplevel_part);

	if (pass == 0) {
	    struct mime_record *mr, **next = &toplevel_part->next;
	    recurse_mime_part(message, toplevel_part, &next,
			      default_charset, info, infoctx);

	    /*
	     * Now go through the MIME parts looking at the
	     * character sets. The first one we find that isn't
	     * ASCII, we see if it's a not-totally-insane superset
	     * of it, and if so adopt it as the default charset.
	     */
	    for (mr = toplevel_part; mr; mr = mr->next) {
		if (mr->md.charset != CS_ASCII &&
		    charset_contains_ascii(mr->md.charset)) {
		    default_charset = mr->md.charset;
		    break;
		}
	    }
	}
    }

    /*
     * Now walk the list of MIME parts, RFC2047-translating each
     * one's Content-Description, passing it to info(), and
     * freeing it.
     */
    {
	struct mime_record *mr, *mrtmp;
	struct message_parse_info inf;

	mr = toplevel_part;
	while (mr) {
	    mrtmp = mr->next;

	    if (mr->rawdescription)
		mr->md.description =
		    rfc2047_to_utf8_string(mr->rawdescription,
					   mr->description_len,
					   TYPE_HEADER_DECODED_TEXT,
					   default_charset);

	    inf.type = PARSE_MIME_PART;
	    inf.header = 0;
	    inf.u.md = mr->md;	       /* structure copy */
	    info(infoctx, &inf);

	    free_mime_record(mr);
	    sfree(mr);
	    mr = mrtmp;
	}
    }
}

void recurse_mime_part(const char *base, struct mime_record *mr,
		       struct mime_record ***next, int default_charset,
		       parser_info_fn_t info, void *infoctx)
{
#ifdef DIAGNOSTICS
    printf("Got MIME part: %s/%s (%d, cs=%d) at (%d,%d)\n"
	   "  (%s, fn=\"%s\", b=\"%s\", desc=\"%.*s\")\n",
	   mr->md.major, mr->md.minor,
	   mr->md.transfer_encoding, mr->md.charset,
	   mr->md.offset, mr->md.length,
	   disposition_name(mr->md.disposition),
	   mr->md.filename, mr->boundary,
	   mr->description_len, mr->rawdescription);
#endif

    if (!istrcmp(mr->md.major, "multipart") &&
	mr->md.transfer_encoding == NO_ENCODING && mr->boundary != NULL) {

	/*
	 * Go through a multipart looking for instances of the
	 * separator line, and recurse into each subpart.
	 */

	const char *partstart = NULL, *partend = NULL;
	const char *here = base + mr->md.offset, *end = here + mr->md.length;
	int blen = strlen(mr->boundary);
	int done = FALSE;

	while (here < end && !done) {
	    if (end - here > blen+2 &&
		here[0] == '-' && here[1] == '-' &&
		!istrlencmp(here+2, blen, mr->boundary, blen)) {
		int got = FALSE;

		partend = here;
		here += blen+2;
		if (here+2 < end && here[0] == '-' &&
		    here[1] == '-' && here[2] == '\n') {
		    here += 3;
		    done = TRUE;       /* special end separator */
		    got = TRUE;
		} else if (here < end && here[0] == '\n') {
		    here++;
		    got = TRUE;
		}

		if (got) {
		    /*
		     * Remove a trailing newline.
		     */
		    if (partend > partstart && partend[-1] == '\n')
			partend--;

		    if (partstart) {
			struct mime_record *this_part;
			this_part = snew(struct mime_record);
			init_mime_record(this_part, default_charset);
			**next = this_part;
			*next = &this_part->next;
			parse_headers(base, partstart, partend - partstart,
				      FALSE, 0, default_charset,
				      null_output_fn, NULL,
				      null_info_fn, NULL, this_part);
			recurse_mime_part(base, this_part, next,
					  default_charset, info, infoctx);
		    }
		    partstart = here;
		}
	    } else {
		while (here < end && *here != '\n')
		    here++;
		if (here < end)
		    here++;	       /* eat the newline */
	    }
	}
    } else if (!istrcmp(mr->md.major, "message") &&
	       !istrcmp(mr->md.minor, "rfc822") &&
	       mr->md.transfer_encoding == NO_ENCODING) {
	/*
	 * We recurse into message/rfc822 to find the type of the
	 * message _body_ (which might in turn hold multiparts).
	 * 
	 * Although this is in theory a full RFC822 message, we
	 * only parse it for its MIME headers; the rest is
	 * irrelevant.
	 */
	struct mime_record *this_part;
	this_part = snew(struct mime_record);
	init_mime_record(this_part, default_charset);
	**next = this_part;
	*next = &this_part->next;
	parse_headers(base, base + mr->md.offset, mr->md.length,
		      FALSE, 0, default_charset,
		      null_output_fn, NULL,
		      null_info_fn, NULL, this_part);
	recurse_mime_part(base, this_part, next, default_charset,
			  info, infoctx);
    }
}

void parse_headers(char const *base, char const *message, int msglen,
		   int full_message, int pass, int default_charset,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx,
		   struct mime_record *mr)
{
#ifdef TESTMODE
    char const *endpoint = message + msglen;
#endif
    char const *p, *q, *r, *rr, *s;

    while (msglen > 0) {
#ifdef TESTMODE
	assert(endpoint == message + msglen);
#endif
	if (*message == '\n') {
	    /*
	     * \n at the start of a header line means we are
	     * looking at the blank line separating headers from
	     * body. Advance past that newline, exit this loop,
	     * and go on to the message body.
	     */
	    message++, msglen--;
	    break;
	}

	/*
	 * Find the word introducing the header line. We expect a
	 * number of non-whitespace characters, followed by a
	 * colon.
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
		 * It may be a bogus header, but we still need to
		 * translate and output it. The line runs from p
		 * to message.
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
	 * Find the full extent of the header line, by searching
	 * to the next newline not followed by whitespace.
	 *
	 * (We can't actually remove the newlines at this stage,
	 * because we may need to know where they were later for
	 * RFC2047 purposes - two RFC2047 encoded words separated
	 * by exactly (newline, space) must be concatenated
	 * without a separating space.)
	 *
	 * We are currently sitting on the colon.
	 */
	q = message;
	message++, msglen--;	       /* eat the colon */
	while (msglen > 0 && WSP(*message))
	    message++, msglen--;       /* eat WSP */
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
	 * p points to the start of the header line. q points to
	 * the end of the header _name_. r points to the start of
	 * the header text (i.e. between q and r is the colon and
	 * any whitespace). `message' points to the end of the
	 * header (just before the final newline).
	 *
	 * Slight ick: this block is enclosed in `do while (0)' so
	 * that `continue' will leave it early.
	 */

#ifdef DIAGNOSTICS
	printf("Got header \"%.*s\" = \"%.*s\"\n", q-p, p, message-r, r);
#endif

	do {
	    static const struct header_type headers[] = {
		/* These are sorted in (case-insensitive) order. */
		{"Bcc", H_BCC, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"Cc", H_CC, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"Comments", 0, ENCODED_ANYWHERE, DO_NOTHING},
		{"Content-Description", H_CONTENT_DESCRIPTION,
			ENCODED_ANYWHERE, CONTENT_DESCRIPTION},
		{"Content-Disposition", H_CONTENT_DISPOSITION,
			ENCODED_COMMENTS, CONTENT_DISPOSITION},
		{"Content-ID", 0, ENCODED_COMMENTS, DO_NOTHING},
		{"Content-Transfer-Encoding", H_CONTENT_TRANSFER_ENCODING,
			ENCODED_COMMENTS, CONTENT_TRANSFER_ENCODING},
		{"Content-Type", H_CONTENT_TYPE, ENCODED_COMMENTS,
			CONTENT_TYPE},
		{"Date", H_DATE, ENCODED_COMMENTS, EXTRACT_DATE},
		{"From", H_FROM, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"In-Reply-To", H_IN_REPLY_TO, ENCODED_COMMENTS,
			EXTRACT_MESSAGE_IDS},
		{"Message-ID", H_MESSAGE_ID, ENCODED_COMMENTS,
			EXTRACT_MESSAGE_IDS},
		{"Received", 0, ENCODED_COMMENTS, DO_NOTHING},
		{"References", H_REFERENCES, ENCODED_COMMENTS,
			EXTRACT_MESSAGE_IDS},
		{"Reply-To", H_REPLY_TO, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"Resent-Bcc", H_RESENT_BCC, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Resent-Cc", H_RESENT_CC, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Resent-Date", 0, ENCODED_COMMENTS, DO_NOTHING},
		{"Resent-From", H_RESENT_FROM, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Resent-Message-ID", 0, ENCODED_COMMENTS, DO_NOTHING},
		{"Resent-Reply-To", H_RESENT_REPLY_TO, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Resent-Sender", H_RESENT_SENDER, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Resent-To", H_RESENT_TO, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Return-Path", H_RETURN_PATH, ENCODED_CPHRASES,
			EXTRACT_ADDRESSES},
		{"Sender", H_SENDER, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"Subject", H_SUBJECT, ENCODED_ANYWHERE, EXTRACT_SUBJECT},
		{"To", H_TO, ENCODED_CPHRASES, EXTRACT_ADDRESSES},
		{"*X-", 0, ENCODED_ANYWHERE, DO_NOTHING},
	    };
	    static const struct header_type null_header = {
		"<other>", NO_ENCODED, DO_NOTHING
	    };
	    int bot, mid, top;
	    const char *name, *ourname;
	    int namelen, ourlen, prefix, cmp;
	    const struct header_type *hdr;
	    struct lexed_header *lh, *lhbase;

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
		    cmp = (tolower((unsigned char)*name) -
			   tolower((unsigned char)*ourname));
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
	     * output the comments are the only _interesting_ bit
	     * in terms of RFC2047 encoding.
	     */

	    /*
	     * Abandon this header entirely and move straight on
	     * to the next one if there isn't _something_
	     * interesting in the header. During pass 0, and in
	     * sub-MIME-parts, we only care about MIME headers. */
	    if ((pass == 0 || !full_message) &&
		hdr->action != CONTENT_TYPE &&
		hdr->action != CONTENT_TRANSFER_ENCODING &&
		hdr->action != CONTENT_DESCRIPTION &&
		hdr->action != CONTENT_DISPOSITION)
		continue;

	    /*
	     * Perform semantic parsing.
	     */
	    lh = lhbase = lex_header(r, message - r, hdr->action);
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
	      case EXTRACT_ADDRESSES:
		/*
		 * Every address appears before a comma, a
		 * semicolon, or end-of-string. So the simplest
		 * way is if first we scan forward to find one of
		 * those, and _then_ we look back from there to
		 * deal with what we've found.
		 */
		while (lh->token) {
		    struct lexed_header *lh2 = lh;

		    /*
		     * Scan forwards for a terminating character.
		     */
		    while (lh2->token &&
			   (!lh2->is_punct ||
			    (lh2->token[0] != ';' &&
			     lh2->token[0] != ',')))
			lh2++;
		    /*
		     * Now look backwards. If we see a >, then we
		     * expect to find it preceded by an address, a
		     * <, and then a display phrase. If instead we
		     * see a word, that _is_ the address, and its
		     * following comment (if any) is the display
		     * name.
		     */
		    if (lh2 >= lh+3 &&
			lh2[-1].is_punct && lh2[-1].token[0] == '>' &&
			lh2[-3].is_punct && lh2[-3].token[0] == '<') {
			struct lexed_header *lh3;
			int displen;
			char *dispdata;

			lh3 = lh2-3;

			displen = 0;
			while (lh3 > lh &&
			       !lh3[-1].is_punct) {
			    lh3--;
			    displen += (displen > 0) + lh3->length;
			}
			if (displen > 0) {
			    int i;
			    dispdata = snewn(displen, char);
			    i = 0;
			    while (lh3 < lh2-3) {
				memcpy(dispdata+i, lh3->token,
				       lh3->length);
				i += lh3->length;
				if (lh3+1 < lh2-3)
				    dispdata[i++] = ' ';
				lh3++;
			    }
			    assert(i == displen);
			} else
			    dispdata = NULL;
			info_addr(info, infoctx,
				  lh2[-2].token, lh2[-2].length,
				  dispdata, displen,
				  hdr->header_id, default_charset);
			sfree(dispdata);
		    } else if (lh2 > lh && !lh2[-1].is_punct) {
			info_addr(info, infoctx,
				  lh2[-1].token, lh2[-1].length,
				  lh2[-1].comment, lh2[-1].clength,
				  hdr->header_id, default_charset);
		    }
		    lh = lh2;
		    /*
		     * Eat the terminating character.
		     */
		    if (lh->token)
			lh++;
		}
		break;
	      case EXTRACT_MESSAGE_IDS:
		{
		    /*
		     * Any single word between a pair of <> and
		     * containing an @ is a Message-ID.
		     */
		    while (lh->token) {
			if (lh[0].token && lh[1].token && lh[2].token &&
			    lh[0].is_punct && lh[0].token[0]=='<' &&
			    !lh[1].is_punct &&
			    lh[2].is_punct && lh[2].token[0]=='>') {
			    struct message_parse_info inf;
			    inf.type = PARSE_MESSAGE_ID;
			    inf.header = hdr->header_id;
			    inf.u.string = snewn(1+lh[1].length, char);
			    memcpy(inf.u.string, lh[1].token, lh[1].length);
			    inf.u.string[lh[1].length] = '\0';
			    info(infoctx, &inf);
			    sfree(inf.u.string);
			}
			lh++;
		    }
		}
		break;
	      case EXTRACT_DATE:
		{
		    struct tm tm;
		    time_t t;
		    int m;
		    const char *months="JanFebMarAprMayJunJulAugSepOctNovDec";

		    /*
		     * Skip optional day of week (signified by a
		     * following comma).
		     */
		    if (lh[0].token && lh[1].token &&
			lh[1].is_punct && lh[1].token[0] == ',')
			lh += 2;

		    /*
		     * Day.
		     */
		    if (!lh[0].token || lh[0].is_punct)
			break;
		    tm.tm_mday = latoi(lh[0].token, lh[0].length);
		    if (tm.tm_mday < 1 || tm.tm_mday > 31)
			break;
		    lh++;

		    /*
		     * Month.
		     */
		    if (!lh[0].token || lh[0].is_punct)
			break;
		    for (m = 0; m < 12; m++) {
			if (!istrlencmp(lh[0].token, lh[0].length,
					months+3*m, 3))
			    break;
		    }
		    if (m > 11)
			break;
		    tm.tm_mon = m;
		    lh++;

		    /*
		     * Year.
		     */
		    if (!lh[0].token || lh[0].is_punct)
			break;
		    if (lh[0].length == 2) {
			/* `obs-year'. */
			tm.tm_year = latoi(lh[0].token, lh[0].length);
			if (tm.tm_year < 50)   /* 00-49 mean 2000-2049 */
			    tm.tm_year += 100;
		    } else {
			tm.tm_year = -1900 + latoi(lh[0].token, lh[0].length);
			if (tm.tm_year < 0)
			    break;
		    }
		    lh++;

		    /*
		     * Hour, and following colon.
		     */
		    if (!lh[0].token || lh[0].is_punct)
			break;
		    tm.tm_hour = latoi(lh[0].token, lh[0].length);
		    if (tm.tm_hour < 0 || tm.tm_hour > 23)
			break;
		    lh++;
		    if (!lh[0].is_punct || lh[0].token[0] != ':')
			break;
		    lh++;

		    /*
		     * Minute.
		     */
		    if (!lh[0].token || lh[0].is_punct)
			break;
		    tm.tm_min = latoi(lh[0].token, lh[0].length);
		    if (tm.tm_min < 0 || tm.tm_min > 59)
			break;
		    lh++;

		    /*
		     * Optional colon-plus-seconds.
		     */
		    if (lh[0].is_punct && lh[0].token[0] == ':') {
			lh++;
			if (!lh[0].token || lh[0].is_punct)
			    break;
			tm.tm_sec = latoi(lh[0].token, lh[0].length);
			if (tm.tm_sec < 0 || tm.tm_hour > 61)
			    break;
			lh++;
		    } else
			tm.tm_sec = 0;

		    /*
		     * Time zone.
		     */
		    if (lh[0].token && !lh[0].is_punct) {
			if (lh[0].length == 5 &&
			    (lh[0].token[0] == '+' || lh[0].token[0] == '-')) {
			    int sign = (lh[0].token[0] == '+' ? -1 : +1);
			    tm.tm_hour +=
				sign * latoi(lh[0].token+1, 2);
			    tm.tm_min +=
				sign * latoi(lh[0].token+3, 2);
			} else {
			    /*
			     * Some obsolete time zone names are
			     * hard-coded into RFC 2822.
			     */
			    static const struct {
				const char *zone;
				int len;
				int hours;
			    } zones[] = {
				{SL("UT"), 0},
				{SL("GMT"), 0},
				{SL("EST"), +5},
				{SL("EDT"), +4},
				{SL("CST"), +6},
				{SL("CDT"), +5},
				{SL("MST"), +7},
				{SL("MDT"), +6},
				{SL("PST"), +8},
				{SL("PDT"), +7},
			    };
			    int i;

			    for (i = 0; i < lenof(zones); i++) {
				if (!istrlencmp(lh[0].token, lh[0].length,
						zones[i].zone, zones[i].len)) {
				    tm.tm_hour += zones[i].hours;
				    break;
				}
			    }
			}
		    }

		    /*
		     * If we're still here, we have accumulated a
		     * complete date.
		     */
		    t = mktime(&tm);

		    {
			struct message_parse_info inf;
			inf.type = PARSE_DATE;
			inf.header = hdr->header_id;
			inf.u.date = t;
			info(infoctx, &inf);
		    }
		}
		break;
	      case EXTRACT_SUBJECT:
		{
		    struct message_parse_info inf;
		    inf.type = PARSE_SUBJECT;
		    inf.header = hdr->header_id;
		    inf.u.string =
			rfc2047_to_utf8_string(r, message - r,
					       TYPE_HEADER_DECODED_TEXT,
					       default_charset);
		    info(infoctx, &inf);
		    sfree(inf.u.string);
		}
		break;
	      case CONTENT_TYPE:
		if (pass == 0)
		    parse_content_type(lh, mr);
		break;
	      case CONTENT_DISPOSITION:
		/*
		 * Content-Disposition is defined in RFC 2183.
		 */
		if (pass == 0)
		    parse_content_disp(lh, mr);
		break;
	      case CONTENT_TRANSFER_ENCODING:
		if (pass == 0)
		    parse_content_transfer(lh, mr);
		break;
	      case CONTENT_DESCRIPTION:
		if (pass == 0) {
		    mr->rawdescription = r;
		    mr->description_len = message - r;
		}
		break;
	      case DO_NOTHING:
		break;
	    }
	    sfree(lhbase);

	    /*
	     * Parse the header for output (pass 1 only).
	     */
	    if (pass == 0 || !full_message)
		continue;

	    s = r;
	    output(outctx, p, r - p, TYPE_HEADER_NAME, CS_ASCII);
	    switch (hdr->rfc2047_location) {
	      case NO_ENCODED:
		output(outctx, r, message - r, TYPE_HEADER_TEXT,
		       default_charset);
		break;
	      case ENCODED_ANYWHERE:
		rfc2047_decode(r, message - r, output, outctx,
			       TYPE_HEADER_DECODED_TEXT,
			       TRUE, default_charset);
		break;
	      case ENCODED_COMMENTS:
		/*
		 * I believe this much is general to any
		 * structured header in which comments are
		 * permitted:
		 *
		 *  - If we see an open paren, we parse a comment,
		 *    which means we scan up to the closing paren
		 *    (dodging backslash-quoted characters on the
		 *    way), and feed the inside of the parens to
		 *    rfc2047_decode().
		 *
		 *  - If we see a double quote, we parse a quoted
		 *    string, which means we scan up to the
		 *    closing quote (dodging backslash-quoted
		 *    characters on the way of course) and pass
		 *    that lot straight to output().
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
			rfc2047_decode(p, r-p, output, outctx,
				       TYPE_HEADER_DECODED_COMMENT,
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
		 * This is the most complex case for display
		 * parsing. The syntax is:
		 *
		 *  - a `mailbox' is either a bare address, or a
		 *    potentially-RFC2047able-phrase followed by
		 *    an address in angle brackets.
		 *
		 *  - an `address' is either a mailbox, or a
		 *    group. A group is a potentially-
		 *    RFC2047able-phrase, a colon, a comma-
		 *    separated list of mailboxes, and a final
		 *    semicolon.
		 *
		 *  - an `address-list' is a comma-separated list
		 *    of addresses.
		 *
		 * In theory there is also a `mailbox-list' which
		 * is a comma-separated list of mailboxes; this is
		 * used in some address headers. For example,
		 * `Reply-To' takes an address-list and hence may
		 * contain groups, but `From' takes a mailbox-list
		 * and cannot.
		 *
		 * However, I'm going to ignore this distinction;
		 * anyone using group syntax in a From header
		 * coming into this parser will find that the
		 * parser will DWIM. This doesn't seem to me to be
		 * a large practical problem, and it makes my life
		 * easier.
		 *
		 * So what I actually do here is:
		 *
		 *  - Place a marker.
		 *
		 *  - Scan along the string, dodging
		 *    quoted-strings, backslash-quoted characters
		 *    and comments, until we see a significant
		 *    punctuation event. Significant punctuation
		 *    events are `:', `;', `<', `>', `,' and
		 *    end-of-string.
		 *
		 *  - If we see `:' or `<', then everything
		 *    between the marker and here is part of a
		 *    potentially-RFC2047able phrase. Whereas if
		 *    we see `>', `,', `;' or end-of-string,
		 *    everything between the marker and here was
		 *    an addr-spec or nothing at all.
		 *
		 *  - So now we re-scan from the marker. Anything
		 *    in a quoted-string or in parens we parse as
		 *    an RFC2047able string; anything not in
		 *    either we deal with _as appropriate_ given
		 *    the above.
		 * 
		 * DELIBERATE DEPARTURE FROM STANDARD: RFC2047 is
		 * very clear that a quoted string MUST NOT
		 * contain encoded-words. However, it is the
		 * experience of my large and varied mail archive
		 * that it often does, and it is my opinion that
		 * the best indication of the email author's
		 * intent is given by decoding them anyway. (Of
		 * course, I wouldn't dream of _generating_ an
		 * encoded-word within a quoted string; that would
		 * be evil on entirely another level.)
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
		    /*
		     * Mark the last non-whitespace before this
		     * piece of punctuation.
		     */
		    rr = r;
		    while (rr > p && FWS(rr[-1]))
			rr--;

		    if (r < message && (*r == ':' || *r == '<'))
			rfc2047able = TRUE;
		    else
			rfc2047able = FALSE;
		    if (r < message)
			r++;   /* we'll eat this punctuation too */
		    /*
		     * Now re-scan from p to rr doing potential
		     * RFC2047 things.
		     */
		    q = p;
		    while (1) {
			if (q < rr && *q == '\\' && q+1 < rr) {
			    q += 2;
			} else if (q < rr && *q != '(' && *q != '"') {
			    q++;
			} else {
			    int end;
			    if (rfc2047able)
				rfc2047_decode(p, q-p, output, outctx,
					       TYPE_HEADER_DECODED_PHRASE,
					       TRUE, default_charset);
			    else
				output(outctx, p, q-p,
				       TYPE_HEADER_TEXT,
				       default_charset);
			    if (q == rr)
				break;
			    output(outctx, q, 1,
				   TYPE_HEADER_TEXT, CS_ASCII);
			    end = (*q == '"' ? '"' : ')');
			    p = ++q;
			    while (q < rr && *q != end) {
				if (*q == '\\' && q+1 < rr)
				    q++;
				q++;
			    }
			    rfc2047_decode(p, q-p, output, outctx,
					   (end == ')' ?
					    TYPE_HEADER_DECODED_COMMENT :
					    TYPE_HEADER_DECODED_PHRASE),
					   TRUE, default_charset);
			    if (q == rr)
				break;
			    output(outctx, q, 1,
				   TYPE_HEADER_TEXT, CS_ASCII);
			    p = ++q;
			}
		    }
		    /*
		     * And finally output the whitespace and
		     * punctuation.
		     */
		    output(outctx, rr, r-rr, TYPE_HEADER_TEXT, CS_ASCII);
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

    mr->md.offset = message - base;
    mr->md.length = msglen;
}

struct lexed_header *lex_header(char const *header, int length, int type)
{
    struct lexed_header *ret = NULL;
    struct lexed_header *tok;
    int retlen = 0, retsize = 0;
    char const *punct, *p;

    /*
     * The interesting punctuation elements vary with the type of
     * header.
     */
    switch (type) {
      default: punct = ""; break;
      case EXTRACT_ADDRESSES: punct = ":;<>,"; break;
      case EXTRACT_MESSAGE_IDS: punct = "<>"; break;
      case EXTRACT_DATE: punct = ",:"; break;
      case CONTENT_TYPE: punct = "/;="; break;
      case CONTENT_DISPOSITION: punct = "/;="; break;
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
	    ret = sresize(ret, retsize, struct lexed_header);
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
	    if (length > 0)
		header++, length--;
	}
	tok->length = header - tok->token;
    }

    return ret;
}

void init_mime_record(struct mime_record *mr, int default_charset)
{
    init_mime_details(&mr->md);
    mr->filename_location = NO_FNAME;
    mr->boundary = NULL;
    mr->next = NULL;
    mr->rawdescription = 0;
    mr->description_len = 0;
    mr->md.charset = default_charset;
}

void free_mime_record(struct mime_record *mr)
{
    free_mime_details(&mr->md);
    sfree(mr->boundary);
}

void parse_content_type(struct lexed_header *lh, struct mime_record *mr)
{
    /*
     * If the first three tokens are a word, a slash and another
     * word, we can collect the MIME type itself.
     */
    if ((!lh[0].is_punct && lh[0].length > 0) &&
	( lh[1].is_punct && lh[1].length > 0 && *lh[1].token == '/') &&
	(!lh[2].is_punct && lh[2].length > 0)) {
	int i;
	mr->md.major = snewn(1+lh[0].length, char);
	for (i = 0; i < lh[0].length; i++)
	    mr->md.major[i] = tolower((unsigned char)lh[0].token[i]);
	mr->md.major[lh[0].length] = '\0';
	mr->md.minor = snewn(1+lh[2].length, char);
	for (i = 0; i < lh[2].length; i++)
	    mr->md.minor[i] = tolower((unsigned char)lh[2].token[i]);
	mr->md.minor[lh[2].length] = '\0';
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
	    if (!istrlencmp(lh[0].token, lh[0].length, SL("charset"))) {
		char *cset = snewn(1 + lh[2].length, char);
		int csl;

		csl = unquote(lh[2].token, lh[2].length, TRUE, cset);
		assert(csl <= lh[2].length);
		cset[csl] = '\0';

		mr->md.charset = charset_from_mimeenc(cset);
	    } else if (!istrlencmp(lh[0].token, lh[0].length, SL("name"))) {
		if (mr->filename_location != CD_FILENAME) {
		    int len;

		    sfree(mr->md.filename);
		    mr->md.filename = snewn(1 + lh[2].length, char);
		    len = unquote(lh[2].token, lh[2].length, TRUE,
				  mr->md.filename);
		    assert(len <= lh[2].length);
		    mr->md.filename[len] = '\0';

		    mr->filename_location = CT_NAME;
		}
	    } else if (!istrlencmp(lh[0].token,lh[0].length,SL("boundary"))) {
		int len;

		sfree(mr->boundary);
		mr->boundary = snewn(1 + lh[2].length, char);
		len = unquote(lh[2].token, lh[2].length, TRUE,
			      mr->boundary);
		assert(len <= lh[2].length);
		mr->boundary[len] = '\0';
	    }
	    lh += 3;
	}
    }
}

void parse_content_transfer(struct lexed_header *lh, struct mime_record *mr)
{
    /*
     * Content-Transfer-Encoding is incredibly simple.
     */
    if (!lh[0].is_punct && lh[0].length > 0) {
	if (!istrlencmp(lh[0].token, lh[0].length, SL("quoted-printable"))) {
	    mr->md.transfer_encoding = QP;
	} else if (!istrlencmp(lh[0].token, lh[0].length, SL("base64"))) {
	    mr->md.transfer_encoding = BASE64;
	} else if (!istrlencmp(lh[0].token, lh[0].length, SL("7bit")) ||
		   !istrlencmp(lh[0].token, lh[0].length, SL("8bit")) ||
		   !istrlencmp(lh[0].token, lh[0].length, SL("binary"))) {
	    mr->md.transfer_encoding = NO_ENCODING;
	}
    }
}

void parse_content_disp(struct lexed_header *lh, struct mime_record *mr)
{
    /*
     * If the first token is a word, we can collect the
     * disposition type itself.
     */
    if ((!lh[0].is_punct && lh[0].length > 0)) {
	if (!istrlencmp(lh[0].token, lh[0].length, SL("inline"))) {
	    mr->md.disposition = INLINE;
	} else if (!istrlencmp(lh[0].token, lh[0].length, SL("attachment"))) {
	    mr->md.disposition = ATTACHMENT;
	}
	lh++;
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
	    if (!istrlencmp(lh[0].token, lh[0].length, SL("filename"))) {
		int len;

		sfree(mr->md.filename);
		mr->md.filename = snewn(1 + lh[2].length, char);
		len = unquote(lh[2].token, lh[2].length, TRUE,
			      mr->md.filename);
		assert(len <= lh[2].length);
		mr->md.filename[len] = '\0';

		mr->filename_location = CD_FILENAME;
	    }
	    lh += 3;
	}
    }
}

/*
 * Remove backslash-quoted pairs, and optionally double quotes
 * too, from a string. Returns the output length.
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

struct utf8_string_output_ctx {
    char *text;
    int textlen, textsize;
};
void utf8_string_output(void *vctx, const char *text, int len,
			int type, int charset)
{
    struct utf8_string_output_ctx *ctx = (struct utf8_string_output_ctx *)vctx;
    charset_state instate = CHARSET_INIT_STATE;
    charset_state outstate = CHARSET_INIT_STATE;
    wchar_t midbuf[256];
    wchar_t const *midptr;
    char outbuf[256];
    int inret, midret, midlen;

    if (charset == CS_NONE)
	charset = CS_ASCII;

    charset = charset_upgrade(charset);

    while ( (inret = charset_to_unicode(&text, &len, midbuf,
					lenof(midbuf), charset,
					&instate, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
					       lenof(outbuf), CS_UTF8,
					       &outstate, NULL)) > 0) {
	    if (ctx->textlen + midret >= ctx->textsize) {
		ctx->textsize = ctx->textlen + midret + 256;
		ctx->text = sresize(ctx->text, ctx->textsize, char);
	    }
	    memcpy(ctx->text + ctx->textlen, outbuf, midret);
	    ctx->textlen += midret;
	}
    }
}
char *rfc2047_to_utf8_string(const char *text, int len,
			     int type, int default_charset)
{
    struct utf8_string_output_ctx ctx;

    ctx.text = NULL;
    ctx.textlen = ctx.textsize = 0;
    rfc2047_decode(text, len, utf8_string_output, &ctx, type,
		   FALSE, default_charset);
    utf8_string_output(&ctx, "\0", 1, TYPE_HEADER_TEXT, CS_UTF8);
    return ctx.text;
}

void info_addr(parser_info_fn_t info, void *infoctx,
	       const char *addr, int addrlen,
	       const char *name, int namelen,
	       int header_id, int default_charset)
{
    struct message_parse_info inf;

    inf.type = PARSE_ADDRESS;
    inf.header = header_id;
    /*
     * `name' must be fed through RFC2047 parsing.
     */
    inf.u.addr.display_name =
	rfc2047_to_utf8_string(name, namelen, TYPE_HEADER_DECODED_PHRASE,
			       default_charset);
    /*
     * `addr' must not.
     */
    inf.u.addr.address = snewn(1+addrlen, char);
    memcpy(inf.u.addr.address, addr, addrlen);
    inf.u.addr.address[addrlen] = '\0';

    info(infoctx, &inf);

    sfree(inf.u.addr.address);
    sfree(inf.u.addr.display_name);
}

int latoi(const char *text, int len)
{
    int ret = 0;

    while (len > 0) {
	if (*text >= '0' && *text <= '9')
	    ret = ret * 10 + *text - '0';
	else
	    break;
	text++, len--;
    }

    return ret;
}

void null_output_fn(void *ctx, const char *text, int len,
		    int type, int charset)
{
}

void null_info_fn(void *ctx, struct message_parse_info *info)
{
}

#ifdef TESTMODE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

/*
make && gcc -Wall -g -DTESTMODE -Icharset -o rfc822{,.c} \
    build/{base64,misc,qp,cs-*,malloc,rfc2047}.o
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

    charset = charset_upgrade(charset);

    while ( (inret = charset_to_unicode(&text, &len, midbuf,
					lenof(midbuf), charset,
					&instate, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
					       lenof(outbuf), CS_UTF8,
					       &outstate, NULL)) > 0) {
	    fwrite(outbuf, 1, midret, stdout);
	}
    }
    if (type == TYPE_HEADER_NAME)
	printf("\033[39;0m");
}

void test_info_fn(void *ctx, struct message_parse_info *info)
{
    char *ct;

    switch (info->type) {
      case PARSE_ADDRESS:
	printf("Address [%s]: %s <%s>\n", header_name(info->header),
	       info->u.addr.display_name, info->u.addr.address);
	break;
      case PARSE_MESSAGE_ID:
	printf("Message-ID [%s]: <%s>\n", header_name(info->header),
	       info->u.string);
	break;
      case PARSE_SUBJECT:
	printf("Subject: %s\n",
	       info->u.string);
	break;
      case PARSE_DATE:
	ct = ctime(&info->u.date);
	printf("Date: %.*s\n",
	       (int)(strlen(ct)-1), ct);
	break;
      case PARSE_MIME_PART:
	printf("MIME part: %s/%s (%s, %s, %s)\n",
	       info->u.md.major, info->u.md.minor,
	       charset_to_localenc(info->u.md.charset),
	       encoding_name(info->u.md.transfer_encoding),
	       disposition_name(info->u.md.disposition));
	if (info->u.md.filename)
	    printf("           filename=%s\n", info->u.md.filename);
	if (info->u.md.description)
	    printf("           description=%s\n", info->u.md.description);
	printf("           located at (%d,%d)\n",
	       info->u.md.offset, info->u.md.length);
	break;
    }
}

int main(void)
{
    char *message = NULL;
    int msglen = 0, msgsize = 0;

    while (1) {
	int ret, i, j;

	if (msgsize - msglen < 1024) {
	    msgsize = msglen + 1024;
	    message = sresize(message, msgsize, char);
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
    parse_message(message, msglen, null_output_fn, NULL, test_info_fn, NULL,
		  CS_NONE);

    /*
     * Now test the post-parse output.
     */
    parse_message(message, msglen, test_output_fn, NULL, null_info_fn, NULL,
		  CS_NONE);

    sfree(message);
    
    return 0;
}

#endif
