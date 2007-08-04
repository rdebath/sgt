/*
 * display.c: output a message from Timber's mail store in a
 * display format.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "timber.h"
#include "charset.h"

int mime_parts_overlap(const struct mime_details *md1,
		       const struct mime_details *md2)
{
    /*
     * Two intervals overlap iff each one's start comes
     * before the other's end. (This is easily seen by
     * considering the interval trichotomy law: given two
     * intervals, either A is entirely below B, or B is
     * entirely below A, or they overlap. A is entirely
     * below B iff max(A) < min(B), and vice versa.)
     */
    return (md1->offset < md2->offset + md2->length &&
	    md2->offset < md1->offset + md1->length);
}

/*
 * Go through a list of MIME parts, some of which are subparts of
 * others, and find each one's immediate parent. Returns a
 * dynamically allocated list of `nmd' integers.
 */
int *mime_part_parents(const struct mime_details *md, int nmd)
{
    int *ret = snewn(nmd, int);
    int i, j;

    ret[0] = -1;		       /* special case */
    for (i = 1; i < nmd; i++) {
	/*
	 * Find the parent of part i by searching backwards for the
	 * nearest part which overlaps it. Simple but effective.
	 */
	for (j = i; j-- ;) {
	    if (mime_parts_overlap(&md[i], &md[j])) {
		ret[i] = j;
		break;
	    }
	}
    }

    return ret;
}

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
    int full;
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
    if (type == TYPE_HEADER_NAME && !ctx->full)
	ctx->boring = is_boring_hdr(text, len);
    else if (!is_type_header_text(type))
	ctx->boring = FALSE;

    if (ctx->boring)
	return;

    if (type == TYPE_HEADER_NAME && ctx->display_type == DISPLAY_ANSI)
	printf("\033[1;31m");
    if (type == TYPE_ATTACHMENT_ID_LINE && ctx->display_type == DISPLAY_ANSI)
	printf("\033[1;33m");

    if (charset == CS_NONE)
	charset = CS_ASCII;
    charset = charset_upgrade(charset);

    while ( (inret = charset_to_unicode(&text, &len, midbuf,
					lenof(midbuf), charset,
					&instate, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
					       lenof(outbuf),
					       ctx->output_charset,
					       &outstate, NULL)) > 0) {
	    fwrite(outbuf, 1, midret, stdout);
	}
    }
    if ((type == TYPE_HEADER_NAME || type == TYPE_ATTACHMENT_ID_LINE) &&
	ctx->display_type == DISPLAY_ANSI)
	printf("\033[39;0m");
}

struct display_parse_ctx {
    int nmd, mdsize;
    struct mime_details *md;
};

void display_info_fn(void *vctx, struct message_parse_info *info)
{
    struct display_parse_ctx *ctx = (struct display_parse_ctx *)vctx;

    if (info->type == PARSE_MIME_PART) {
	if (ctx->nmd >= ctx->mdsize) {
	    ctx->mdsize += 16;
	    ctx->md = sresize(ctx->md, ctx->mdsize, struct mime_details);
	}

	copy_mime_details(&ctx->md[ctx->nmd++], &info->u.md);
    }
}

void display_internal(char *message, int msglen,
		      struct mime_details *md, int nmd,
		      int charset, int type, int full)
{
    int mindisp;
    struct display_output_ctx ctx;
    struct display_parse_ctx pctx;
    int i, *parents = NULL;
    int done_blank_line;

    /*
     * Parse the message and display the headers. In this parse
     * pass we also extract information about the MIME parts if we
     * don't already have it.
     */
    ctx.output_charset = charset;
    ctx.display_type = type;
    ctx.boring = FALSE;
    ctx.full = full;
    pctx.nmd = pctx.mdsize = 0;
    pctx.md = NULL;
    parse_message(message, msglen, display_output_fn, &ctx,
		  md ? null_info_fn : display_info_fn, &pctx, CS_NONE);
    printf("\n");		       /* separating blank line */
    done_blank_line = TRUE;

    if (!md) {
	md = pctx.md;
	nmd = pctx.nmd;
    }

    /*
     * Find the parent MIME parts. Also determine whether the
     * outermost part is multipart/mixed, in which case we
     * don't display an ID line for it ever.
     */
    parents = mime_part_parents(md, nmd);
    if (!full && nmd > 0 &&
	!istrcmp(md[0].major, "multipart") &&
	!istrcmp(md[0].minor, "mixed"))
	mindisp = 1;
    else
	mindisp = 0;

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
    for (i = mindisp; i < nmd; i++) {
	/*
	 * Heading line is of the format
	 *
	 *   --[1] filename: description [text/plain, ISO-8859-1]
	 *
	 * The separating colon-space between filename and
	 * description only applies when both are present.
	 *
	 * There are 2 hyphens per indentation level, which
	 * indicates the nesting depth of the part.
	 */
	if (full || md[i].disposition == ATTACHMENT) {
	    const char *charsetstr;
	    int buflen, k;
	    char *buf, *p, *sep;

	    charsetstr = charset_to_localenc(md[i].charset);
	    if (!charsetstr)
		charsetstr = "unknown charset";

	    buflen = 40+2*nmd;     /* the index and all the punctuation */
	    if (md[i].filename)
		buflen += strlen(md[i].filename);
	    if (md[i].description)
		buflen += strlen(md[i].description);
	    buflen += strlen(md[i].major);
	    buflen += strlen(md[i].minor);
	    buflen += strlen(charsetstr);

	    buf = snewn(buflen, char);
	    p = buf;
	    k = i;
	    while (parents[k] >= mindisp) {
		k = parents[k];
		p += sprintf(p, "--");
	    }
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
	    done_blank_line = FALSE;
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
		dbuffer = snewn(md[i].length, char);
		declen = qp_decode(message + md[i].offset, md[i].length,
				   dbuffer, FALSE);
		decoded = dbuffer;
		break;
	      case BASE64:
		dbuffer = snewn(base64_decode_length(md[i].length), char);
		declen = base64_decode(message + md[i].offset,
				       md[i].length, (unsigned char *)dbuffer);
		decoded = dbuffer;
		break;
	    }

	    if (!done_blank_line)
		printf("\n");
	    display_output_fn(&ctx, decoded, declen,
			      TYPE_BODY_TEXT, md[i].charset);
	    printf("\n");
	    done_blank_line = TRUE;
	}

	/*
	 * Go up the nesting tree to the ancestors of this
	 * part, and see which are terminating (i.e. the _next_
	 * part is not a subpart of them). Output end lines for
	 * those.
	 */
	{
	    int k, l;
	    k = i;
	    while (parents[k] >= mindisp) {
		k = parents[k];
		if (i+1 >= nmd ||
		    !mime_parts_overlap(&md[i+1], &md[k])) {
		    /*
		     * Part k has ended.
		     */
		    int buflen = 40+2*nmd;
		    char *buf = snewn(buflen, char);
		    char *p = buf;

		    l = k;
		    while (parents[l] >= mindisp) {
			l = parents[l];
			p += sprintf(p, "--");
		    }

		    p += sprintf(p, "[%d] ends\n", k);
		    display_output_fn(&ctx, buf, p - buf,
				      TYPE_ATTACHMENT_ID_LINE, CS_UTF8);
		    done_blank_line = FALSE;
		    sfree(buf);
		} else
		    break;
	    }

	}
    }

    if (pctx.md) {
	for (i = 0; i < pctx.nmd; i++)
	    free_mime_details(&pctx.md[i]);
	sfree(pctx.md);
    }
    sfree(parents);
}

void display_msgtext(char *message, int msglen,
		     int charset, int type, int full)
{
    display_internal(message, msglen, NULL, 0, charset, type, full);
}

void display_message(char *ego, int charset, int type, int full)
{
    char *location, *message, *separator;
    struct mime_details *md;
    int msglen, nmd, i;

    location = message_location(ego);
    if (!location)
	error(err_nosuchmsg, ego);
    else {
	message = store_retrieve(location, &msglen, &separator);
	if (!message)
	    fatal(err_internal, "mail mentioned in db is not in mail store");
	sfree(location);

	/*
	 * Retrieve the locations of the MIME parts from the
	 * database.
	 */
	md = find_mime_parts(ego, &nmd);
	if (!md) {
	    error(err_internal, "no-mime-parts");
	} else {
	    display_internal(message, msglen, md, nmd, charset, type, full);

	    for (i = 0; i < nmd; i++)
		free_mime_details(&md[i]);
	    sfree(md);
	}

	sfree(separator);
	sfree(message);
    }
}
