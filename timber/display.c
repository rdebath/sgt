/*
 * display.c: output a message from Timber's mail store in a
 * display format.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "timber.h"
#include "charset.h"

static int displayable(const struct mime_details *md)
{
    /*
     * One day I might sort out some internal means of displaying
     * HTML and possibly also text/enriched, and then those will be
     * added to the list of instantly displayable things.
     */
    if (!istrcmp(md->major, "text") && !istrcmp(md->minor, "plain"))
	return TRUE;
    return FALSE;
}

/*
 * This function goes through a list of MIME parts and sorts out
 * their Content-Disposition values. It arranges that
 * 
 *  (a) all UNSPECIFIEDs are removed, so that everything is
 * 	explicitly either INLINE or ATTACHMENT
 * 
 *  (b) nothing is INLINE unless we do actually know how to display
 * 	it inline
 * 
 *  (c) all vaguely sensible properties apply (such as never making
 * 	more than one of a multipart/alternative set INLINE).
 * 
 * This is an extremely heuristic routine; there is no deep logic,
 * just a set of rules of thumb that seemed like a good idea at the
 * time. Future maintainers should feel free to frob it aimlessly
 * if it looks as if it's doing the wrong thing!
 */
static void fix_content_dispositions(struct mime_details *md, int nmd)
{
    int i, j, k, def;
    int count[3];
    cassert(UNSPECIFIED >= 0 && UNSPECIFIED < lenof(count));
    cassert(INLINE >= 0 && INLINE < lenof(count));
    cassert(ATTACHMENT >= 0 && ATTACHMENT < lenof(count));

    /*
     * Start by counting up the original disposition values. We'll
     * use these counts later on.
     */
    for (i = 0; i < lenof(count); i++)
	count[i] = 0;
    for (i = 0; i < nmd; i++) {
	if (md[i].disposition < 0 || md[i].disposition >= lenof(count))
	    md[i].disposition = UNSPECIFIED;
	count[md[i].disposition]++;
    }

    /*
     * Rule one: any MIME part which we cannot directly display is
     * automatically an attachment. This even overrides an explicit
     * INLINE designation!
     */
    for (i = 0; i < nmd; i++)
	if (!displayable(&md[i]))
	    md[i].disposition = ATTACHMENT;

    /*
     * Rule two: anything which is a subpart of a non-multipart is
     * automatically an attachment.
     * 
     * (If I were in a mood to _ever_ display message/rfc822 parts
     * inline, then I would have to reverse this rule: any inline
     * message/rfc822 part would have to have its subparts treated
     * recursively according to the same set of rules as the outer
     * message. Good job I'm not!)
     */
    for (i = 0; i < nmd; i++) {
	if (istrcmp(md[i].major, "multipart")) {
	    int newi = i;
	    for (j = i+1;
		 j < nmd && md[j].offset >= md[i].offset &&
		 md[j].offset < md[i].length;
		 j++) {
		newi = j;
		md[j].disposition = ATTACHMENT;
	    }
	    i = newi;
	}
    }

    /*
     * Rule three: out of everything still not explicitly set to
     * ATTACHMENT, the first part (which by now must be
     * displayable) should be INLINE. (If it's INLINE already, we
     * need do nothing; if it's UNSPECIFIED we set it INLINE.)
     */
    for (i = 0; i < nmd; i++) {
	if (md[i].disposition != ATTACHMENT) {
	    md[i].disposition = INLINE;
	    break;
	}
    }

    /*
     * Rule four: if the original set of dispositions contains at
     * least one ATTACHMENT, at least one UNSPECIFIED and no
     * INLINEs, we assume the sending mailer thinks INLINE is the
     * default, and thus we now convert all UNSPECIFIEDs to INLINE.
     * Conversely, if the original set of dispositions contains at
     * least one INLINE, at least one UNSPECIFIED and no
     * ATTACHMENTs, we assume the sending mailer thinks ATTACHMENT
     * is the default.
     * 
     * If we can't figure out the sending mailer's default, we take
     * ATTACHMENT to be the default default (as it were).
     */
    def = ATTACHMENT;
    if (count[UNSPECIFIED] > 0) {
	if (count[INLINE] > 0 && count[ATTACHMENT] == 0)
	    def = ATTACHMENT;
	else if (count[ATTACHMENT] > 0 && count[INLINE] == 0)
	    def = INLINE;
    }

    for (i = 0; i < nmd; i++)
	if (md[i].disposition == UNSPECIFIED)
	    md[i].disposition = def;

    /*
     * Rule five: within any multipart/alternative, at most one
     * immediate subpart may be INLINE.
     */
    for (i = 0; i < nmd; i++) {
	if (!istrcmp(md[i].major, "multipart") &&
	    !istrcmp(md[i].major, "alternative")) {
	    int seen_inline = FALSE;
	    int newi = i;

	    for (j = i+1;
		 j < nmd && md[j].offset >= md[i].offset &&
		 md[j].offset < md[i].length;
		 j++) {
		int newj = j;
		/*
		 * Part j is an immediate subpart of the
		 * multipart/alternative part i. Bypass any
		 * subparts of _that_.
		 */
		if (seen_inline)
		    md[j].disposition = ATTACHMENT;
		for (k = j+1;
		     k < nmd && md[k].offset >= md[j].offset &&
		     md[k].offset < md[j].length;
		     k++) {
		    newj = k;
		    if (seen_inline)
			md[k].disposition = ATTACHMENT;
		}
		if (md[j].disposition == INLINE)
		    seen_inline = TRUE;
		newi = j = newj;
	    }
	}
    }
}

struct display_output_ctx {
    int output_charset;
    int display_type;
    int boring;
};

static void display_output_fn(void *vctx, const char *text, int len,
			      int type, int charset)
{
    struct display_output_ctx *ctx = (struct display_output_ctx *)vctx;
    charset_state instate = CHARSET_INIT_STATE;
    charset_state outstate = CHARSET_INIT_STATE;
    wchar_t midbuf[256];
    wchar_t const *midptr;
    char outbuf[256];
    int inret, midret, midlen;

    /*
     * Here is a good place to filter out boring headers for
     * display. This relies on the RFC822 parser always passing us
     * the entire header _name_ as a single string.
     */
    if (type == TYPE_HEADER_NAME)
	ctx->boring = is_boring_hdr(text, len);
    else if (type != TYPE_HEADER_TEXT)
	ctx->boring = FALSE;

    if (ctx->boring)
	return;

    if (type == TYPE_HEADER_NAME && ctx->display_type == DISPLAY_ANSI)
	printf("\033[1;31m");
    if (type == TYPE_ATTACHMENT_ID_LINE && ctx->display_type == DISPLAY_ANSI)
	printf("\033[1;33m");

    while ( (inret = charset_to_unicode(&text, &len, midbuf,
					lenof(midbuf), charset,
					&instate, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
					       lenof(outbuf),
					       ctx->output_charset,
					       &outstate, NULL, 0)) > 0) {
	    fwrite(outbuf, 1, midret, stdout);
	}
    }
    if ((type == TYPE_HEADER_NAME || type == TYPE_ATTACHMENT_ID_LINE) &&
	ctx->display_type == DISPLAY_ANSI)
	printf("\033[39;0m");
}

void display_message(char *ego, int charset, int type)
{
    char *location, *message, *separator;
    int msglen;
    struct display_output_ctx ctx;
    struct mime_details *md = NULL;
    int nmd, i;

    location = message_location(ego);
    if (!location)
	error(err_nosuchmsg, ego);
    else {
	message = store_retrieve(location, &msglen, &separator);
	if (!message)
	    fatal(err_internal, "mail mentioned in db is not in mail store");
	sfree(location);

	/*
	 * Display the headers.
	 */
	ctx.output_charset = charset;
	ctx.display_type = type;
	ctx.boring = FALSE;
	parse_message(message, msglen, display_output_fn, &ctx,
		      null_info_fn, NULL);
	printf("\n");		       /* separating blank line */

	/*
	 * Retrieve the locations of the MIME parts from the
	 * database.
	 */
	md = find_mime_parts(ego, &nmd);
	if (!md) {
	    error(err_internal, "no-mime-parts");
	    goto cleanup;
	}

	/*
	 * Fix up the Content-Dispositions.
	 */
	fix_content_dispositions(md, nmd);

#if 0
	{
	    int j;
	    for (j = 0; j < nmd; j++)
		printf("MIME part: %s/%s (%s, cs=%s) at (%d,%d)\n"
		       "  (%s, fn=\"%s\", desc=\"%s\")\n",
		       md[j].major, md[j].minor,
		       encoding_name(md[j].transfer_encoding),
		       charset_to_localenc(md[j].charset),
		       md[j].offset, md[j].length,
		       disposition_name(md[j].disposition),
		       md[j].filename, md[j].description);
	}
#endif

	/*
	 * For each MIME part, display the text (decoded and
	 * translated) and/or a heading line giving details of it.
	 */
	for (i = 0; i < nmd; i++) {
	    /*
	     * Heading line is of the format
	     * 
	     *   [1] filename: description [text/plain, ISO-8859-1]
	     * 
	     * The separating colon-space between filename and
	     * description only applies when both are present.
	     */
	    {
		const char *charsetstr;
		int buflen;
		char *buf, *p, *sep;

		charsetstr = charset_to_localenc(md[i].charset);
		if (!charsetstr)
		    charsetstr = "unknown charset";

		buflen = 40;	       /* the index and all the punctuation */
		if (md[i].filename)
		    buflen += strlen(md[i].filename);
		if (md[i].description)
		    buflen += strlen(md[i].description);
		buflen += strlen(md[i].major);
		buflen += strlen(md[i].minor);
		buflen += strlen(charsetstr);

		buf = smalloc(buflen);
		p = buf;
		p += sprintf(p, "[%d]", i);
		sep = " ";
		if (md[i].filename) {
		    p += sprintf(p, "%s%s", sep, md[i].filename);
		    sep = ": ";
		}
		if (md[i].description)
		    p += sprintf(p, "%s%s", sep, md[i].description);
		p += sprintf(p, " [%s/%s", md[i].major, md[i].minor);
		/*
		 * We only bother with the charset for text parts.
		 */
		if (!strcmp(md[i].major, "text"))
		    p += sprintf(p, ", %s", charsetstr);
		p += sprintf(p, "]\n");

		assert(p - buf < buflen);

		/*
		 * This ID line has been constructed from data in
		 * the database, which means it is currently
		 * expressed in UTF-8.
		 */
		display_output_fn(&ctx, buf, p - buf,
				  TYPE_ATTACHMENT_ID_LINE, CS_UTF8);
		sfree(buf);
	    }

	    if (md[i].disposition == INLINE) {
		char *dbuffer = NULL, *decoded;
		int declen;

		switch (md[i].transfer_encoding) {
		  case NO_ENCODING:
		    decoded = message + md[i].offset;
		    declen = md[i].length;
		    break;
		  case QP:
		    dbuffer = smalloc(md[i].length);
		    declen = qp_decode(message + md[i].offset, md[i].length,
				       dbuffer, FALSE);
		    decoded = dbuffer;
		    break;
		  case BASE64:
		    dbuffer = smalloc(base64_decode_length(md[i].length));
		    declen = base64_decode(message + md[i].offset,
					   md[i].length, dbuffer);
		    decoded = dbuffer;
		    break;
		}

		display_output_fn(&ctx, decoded, declen,
				  TYPE_BODY_TEXT, md[i].charset);
	    }
	}

	cleanup:
	if (md) {
	    for (i = 0; i < nmd; i++)
		free_mime_details(&md[i]);
	    sfree(md);
	}
	sfree(message);
	sfree(separator);
    }
}
