/*
 * send.c: Timber mail-sending functionality.
 */

/*
 * TODO:
 * 
 *  - Instead of spewing the message straight to stdout, we should
 *    accumulate it in an in-memory buffer. Then our send command
 *    will be able to send the same data to sendmail and its Fcc;
 *    also this will make it trivial to implement proper error
 *    handling, in the sense of either failing completely or
 *    succeeding completely. (Currently we can output half a
 *    message and then discover we can't open one of the
 *    attachment files.)
 * 
 *  - At some point, we should actually implement _sending_, as
 *    well as just send filtering. Though we clearly ought to have
 *    a `send-filter' command, much like `display-filter', which
 *    just pipes stdin to stdout and formats it in RFC822.
 * 
 *  - process the Queue header.
 *     * I have a sort of a plan here which revolves around
 * 	 storing queued messages in a special place within the
 * 	 database together with their scheduled time, and then
 * 	 having a command `timber run-queue' which checks the
 * 	 current time and sends off anything it should be sending.
 * 	 Better yet, `timber run-queue <date>', to avoid messages
 * 	 failing to go because at fired a second early. Point of
 * 	 all this, anyway, is that we can then edit the queue
 * 	 later on, delete or unqueue or modify messages and
 * 	 suchlike, and any detritus in the at queue will simply be
 * 	 spare run-queue commands and so will clear itself up
 * 	 harmlessly when its time arrives.
 *     * This might also tie in to my other plan, which is to tag
 * 	 messages as `reply expected by <date>', and have them
 * 	 automatically moved into an `outstanding' folder or
 * 	 otherwise flagged for my attention if no reply has
 * 	 arrived (which we can tell _automatically_) by that time.
 * 
 *  - invent an extra attachment header, very similar to our
 *    current one, which reads a message out of the database by
 *    its ego. Used for message forwarding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <netdb.h>

#include "timber.h"
#include "charset.h"

struct buffer {
    char *text;
    int length;
    int size;
};

void buffer_init(struct buffer *buf)
{
    buf->text = NULL;
    buf->length = buf->size = 0;
}

void buffer_append(struct buffer *buf, char const *text, int length)
{
    if (buf->length + length >= buf->size) {
	buf->size = buf->length + length + 512;
	buf->text = sresize(buf->text, buf->size, char);
    }
    memcpy(buf->text + buf->length, text, length);
    buf->length += length;
    buf->text[buf->length] = '\0';
}

void buffer_cleanup(struct buffer *buf)
{
    sfree(buf->text);
    buffer_init(buf);
}

struct wrap_details {
    int linelen;
    int recentws;
};

void wrap_init(struct wrap_details *wrap)
{
    wrap->linelen = 0;
    wrap->recentws = FALSE;
}

#define WSP(c) ( (c)==' ' || (c)=='\t' )

/*
 * This wrap function relies on not receiving two halves of a word
 * in separate calls. Whitespace can be split, but not words.
 */
void append_wrap(struct buffer *buf, struct wrap_details *wrap,
		 char const *text, int len)
{
    char const *p;

    while (len > 0) {
	/*
	 * Look forward to find the first non-whitespace. (For
	 * these purposes, a newline doesn't count as whitespace;
	 * it's special, and resets the count.)
	 */
	p = text;
	while (p < text+len && (WSP(*p) || *p == '\n')) {
	    if (*p == '\n')
		wrap->linelen = 0;
	    else
		wrap->linelen++;
	    if (wrap->linelen == 76) {
		/*
		 * The whitespace alone has come to more than we
		 * can handle! Output a load of it.
		 */
		buffer_append(buf, text, p-text);
		buffer_append(buf, "\n", 1);
		wrap->linelen = 1;     /* the char we're about to step past */
		wrap->recentws = TRUE;
		len -= p-text;
		text = p;
	    }
	    p++;
	}

	/*
	 * Output the whitespace.
	 */
	if (p > text) {
	    buffer_append(buf, text, p-text);
	    len -= p-text;
	    text = p;
	    /*
	     * We have to set recentws to FALSE if the last thing
	     * we output was a newline. This will be true iff
	     * linelen==0.
	     */
	    wrap->recentws = (wrap->linelen > 0);
	}

	/*
	 * Now we have the start of a word. Scan forward to find
	 * the end of it.
	 */
	while (p < text+len && !WSP(*p) && *p != '\n')
	    p++;

	/*
	 * So we're trying to fit a word of length (p-text) on the
	 * current line. If we can't, we need to go back and add a
	 * fold (newline) _before_ the last whitespace character we
	 * output.
	 *
	 * This is only possible, of course, if we _did_ just
	 * output some whitespace.
	 */
	if (p-text + wrap->linelen > 76 && wrap->recentws) {
	    char c = buf->text[buf->length - 1];
	    buf->text[buf->length - 1] = '\n';
	    buffer_append(buf, &c, 1);
	    wrap->linelen = 1;
	}
	buffer_append(buf, text, p - text);
	wrap->linelen += p-text;
	wrap->recentws = FALSE;
	len -= p-text;
	text = p;
    }
}

/*
 * List of headers we treat specially when they appear in the
 * incoming message.
 */
enum {
    /*
     * Headers requiring special treatment.
     */
    SH_ATTACHMENT,
    SH_QUEUE,
    /*
     * Headers which we throw out completely, because they were
     * specific to the MIME structure of the incoming message, and
     * we don't want them confusing the MIME structure we're going
     * to construct on the outgoing one.
     */
    SH_IGNORE
};
#define STRANDLEN(x) x, sizeof(x)-1
struct specialhdr {
    const char *name;
    int len;
    int type;
};
static const struct specialhdr specialhdrs[] = {
    {STRANDLEN("Attachment"), SH_ATTACHMENT},
    {STRANDLEN("Content-Description"), SH_IGNORE},
    {STRANDLEN("Content-Disposition"), SH_IGNORE},
    {STRANDLEN("Content-Transfer-Encoding"), SH_IGNORE},
    {STRANDLEN("Content-Type"), SH_IGNORE},
    {STRANDLEN("Mime-Version"), SH_IGNORE},
    {STRANDLEN("Queue"), SH_QUEUE},
};

enum {
    ATTACH_FILE,		       /* `name' gives the filename */
    ATTACH_EGO			       /* `name' gives the ego string */
    /* ATTACH_ENCLOSED */              /* `name' is blank */
};
struct attachment {
    int where;
    char *name;
    char *major, *minor;
    int charset;
    char *description;
    char *filename;
};

struct send_output_ctx {
    charset_state main_instate, instate;
    int default_input_charset, curr_input_charset;
    charset_state outstate;
    int curr_output_charset;
    struct wrap_details wrap;
    const int *charset_list;
    int ncharsets;
    const struct specialhdr *specialhdr_desc;
    struct buffer headers, pre2047, specialhdr_text;
    int pre2047type;
    int gotdate, gotmid;
    struct mime_details *parts;
    int nparts, partsize;
    struct attachment *attachments;
    int nattach, attachsize;
};

static void send_info_fn(void *vctx, struct message_parse_info *info)
{
    struct send_output_ctx *ctx = (struct send_output_ctx *)vctx;
    if (info->type == PARSE_MESSAGE_ID && info->header == H_MESSAGE_ID)
	ctx->gotmid = TRUE;
    else if (info->type == PARSE_DATE)
	ctx->gotdate = TRUE;
    else if (info->type == PARSE_MIME_PART) {
	/*
	 * We have an explicit MIME part in the input message. Add
	 * it to the list of parts to be marshalled into the
	 * output one.
	 */
	if (ctx->nparts >= ctx->partsize) {
	    ctx->partsize = ctx->nparts * 3 / 2 + 16;
	    ctx->parts = sresize(ctx->parts, ctx->partsize,
				 struct mime_details);
	}
	copy_mime_details(&ctx->parts[ctx->nparts++], &info->u.md);
    }
}

static char *read_word(char **p)
{
    char *ret = *p, *q = *p;
    int c, quotes = FALSE;

    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (!**p)
	return NULL;
    while (**p && (quotes || !isspace((unsigned char)**p))) {
	if (**p == '"') {
	    quotes = !quotes;
	    (*p)++;
	    continue;
	}
	if (**p == '\\' && (*p)[1]) {
	    (*p)++;
	    if (**p >= '0' && **p < '8') {
		int dmax = 3;
		c = 0;
		while (**p && dmax > 0 && **p >= '0' && **p < '8') {
		    c = (c << 3) | ((**p - '0') & 7);
		    (*p)++;
		    dmax--;
		}
	    } else {
		c = *(*p)++;
	    }
	} else {
	    c = *(*p)++;
	}

	*q++ = c;
    }
    if (**p) (*p)++;
    *q = '\0';

    return ret;
}

/*
 * Rough-and-ready function to convert a string between charsets.
 * Dynamically allocates its return value.
 */
static char *convcs(const char *str, int incs, int outcs)
{
    const char *ci;
    const wchar_t *wi;
    int cilen, wilen;
    wchar_t *ubuf;
    int ulen, ul2;
    char *obuf;
    int olen, ol2;

    ci = str;
    cilen = strlen(ci);
    ulen = charset_to_unicode(&ci, &cilen, NULL, -1, incs, NULL, NULL, 0);
    ubuf = snewn(ulen, wchar_t);
    ci = str;
    cilen = strlen(ci);
    ul2 = charset_to_unicode(&ci, &cilen, ubuf, ulen, incs, NULL, NULL, 0);
    assert(ulen == ul2 && cilen == 0);

    wi = ubuf;
    wilen = ulen;
    olen = charset_from_unicode(&wi, &wilen, NULL, -1, outcs, NULL, NULL);
    obuf = snewn(olen+1, char);
    wi = ubuf;
    wilen = ulen;
    ol2 = charset_from_unicode(&wi, &wilen, obuf, olen, outcs, NULL, NULL);
    assert(olen == ol2 && wilen == 0);

    obuf[olen] = '\0';
    sfree(ubuf);

    return obuf;
}

static void send_output_fn(void *vctx, const char *text, int len,
			   int type, int charset)
{
    struct send_output_ctx *ctx = (struct send_output_ctx *)vctx;
    wchar_t midbuf[256];
    wchar_t const *midptr;
    char outbuf[256];
    int output_charset;
    int inret, midret, midlen;
    struct buffer *buf;

    if (charset == CS_NONE)
	charset = CS_ASCII;
    charset = charset_upgrade(charset);

    if (charset == ctx->default_input_charset) {
	ctx->instate = ctx->main_instate;
    } else if (charset != ctx->curr_input_charset) {
	ctx->instate = charset_init_state;
    }
    ctx->curr_input_charset = charset;

    /*
     * Some headers which come through here need special
     * treatment, because we're going to extract them and use them
     * for some other purpose (Attachment, Queue). Other headers
     * we ignore completely.
     */
    if (type == TYPE_HEADER_NAME) {
	/*
	 * I haven't bothered with a binary search here because
	 * there just aren't enough headers requiring special
	 * treatment to make it worth the effort. If we got
	 * another ten or twenty, I might change my mind.
	 */
	int len2, i;

	for (len2 = 0; len2 < len && text[len2] && text[len2] != ':'; len2++);

	for (i = 0; i < lenof(specialhdrs); i++)
	    if (!istrlencmp(text, len2, specialhdrs[i].name,
			    specialhdrs[i].len))
		break;

	if (i < lenof(specialhdrs)) {
	    /*
	     * This is a special header. Prepare to accumulate its
	     * text separately.
	     */
	    assert(ctx->specialhdr_desc == NULL);
	    ctx->specialhdr_desc = &specialhdrs[i]; /* doesn't need freeing */
	    buffer_cleanup(&ctx->specialhdr_text);
	} else {
	    if (ctx->specialhdr_desc) {
		char *w[4], *minor, *cs, *h = ctx->specialhdr_text.text;
		int i;

		/*
		 * Process a completed special header.
		 */
		switch (ctx->specialhdr_desc->type) {
		  case SH_ATTACHMENT:
		    /*
		     * The Attachment: header is formatted as
		     * follows:
		     * 
		     * 	- The first word gives the pathname of the
		     * 	  file to attach. Spaces, tabs and other
		     * 	  special characters can be escaped with
		     * 	  backslashes; double quotes escape spaces
		     * 	  inside them. A backslash followed by
		     * 	  three octal digits allows any other
		     * 	  character to be represented.
		     * 
		     * 	- After that, the next word gives the MIME
		     * 	  type of the file if present. If a second
		     * 	  slash is present, the data after that is
		     * 	  used as a character set.
		     * 
		     * 	- The word after that, if present and
		     * 	  non-empty ("" can be used to specify an
		     * 	  empty word) is used as a content
		     * 	  description.
		     * 
		     * 	- The word after _that_ is a filename for
		     * 	  the Content-Disposition header.
		     */

		    h += strcspn(h, ":");
		    assert(*h);
		    h++;
		    for (i = 0; i < 4; i++)
			w[i] = read_word(&h);
		    if (!w[0] || !w[1]) {
			fprintf(stderr, "expected at least two words in"
				" Attachment: header\n");
			exit(1);
		    }
		    minor = w[1];
		    minor += strcspn(minor, "/");
		    if (!*minor) {
			fprintf(stderr, "expected Attachment: header to"
				" contain a two-part MIME type\n");
			exit(1);
		    }
		    *minor++ = '\0';
		    cs = minor;
		    cs += strcspn(cs, "/");
		    if (*cs)
			*cs++ = '\0';
		    else
			cs = NULL;
		    if (ctx->nattach >= ctx->attachsize) {
			ctx->attachsize = ctx->nattach * 3 / 2 + 16;
			ctx->attachments = sresize(ctx->attachments,
						   ctx->attachsize,
						   struct attachment);
		    }
		    ctx->attachments[ctx->nattach].where = ATTACH_FILE;
		    ctx->attachments[ctx->nattach].name = dupstr(w[0]);
		    ctx->attachments[ctx->nattach].major = dupstr(w[1]);
		    ctx->attachments[ctx->nattach].minor = dupstr(minor);
		    if (cs) {
			ctx->attachments[ctx->nattach].charset =
			    charset_from_localenc(cs);
			if (ctx->attachments[ctx->nattach].charset==CS_NONE) {
			    fprintf(stderr, "unrecognised character set '%s'"
				    " in Attachment: header\n", cs);
			    exit(1);
			}
		    } else {
			ctx->attachments[ctx->nattach].charset = CS_NONE;
		    }
		    ctx->attachments[ctx->nattach].description =
			w[2] && *w[2] ? dupstr(w[2]) : NULL;
		    {
			char *q, *filename = w[3];
			if (!filename || !*filename) {
			    filename = w[0];
			    while ((q = strchr(filename, '/')) != NULL)
				filename = q + 1;
			}
			ctx->attachments[ctx->nattach].filename =
			    convcs(filename, ctx->default_input_charset,
				   CS_ASCII);
		    }
		    ctx->nattach++;
		    break;
		  case SH_QUEUE:
		    /*
		     * FIXME
		     */
		    break;
		}
	    }
	    ctx->specialhdr_desc = NULL;
	}
    }

    if (is_type_header_decoded(type)) {
	/*
	 * No special header which we deal with by any method
	 * other than ignoring it is defined by RFC822. Hence we
	 * don't expect to see any RFC2047-decoded text for a
	 * non-ignored special header.
	 */
	assert(!ctx->specialhdr_desc ||
	       ctx->specialhdr_desc->type == SH_IGNORE);

	output_charset = CS_UTF8;
	buf = &ctx->pre2047;
	ctx->pre2047type = type;
    } else {
	if (ctx->specialhdr_desc) {
	    output_charset = CS_NONE;
	    buf = &ctx->specialhdr_text;
	} else {
	    if (ctx->pre2047.length > 0) {
		char *post2047 =
		    rfc2047_encode(ctx->pre2047.text, ctx->pre2047.length,
				   CS_UTF8, ctx->charset_list, ctx->ncharsets,
				   ctx->pre2047type,
				   /* Reduce first RFC2047 word to fit on line
				    * with a shortish header. */
				   ctx->wrap.linelen <= 17 ?
				   74 - ctx->wrap.linelen : 74);
		append_wrap(&ctx->headers, &ctx->wrap,
			    post2047, strlen(post2047));
		sfree(post2047);
		buffer_cleanup(&ctx->pre2047);
	    }

	    output_charset = CS_ASCII;
	    buf = &ctx->headers;
	}
    }

    if (buf == &ctx->specialhdr_text) {
	buffer_append(buf, text, len);
    } else {
	if (output_charset != ctx->curr_output_charset) {
	    ctx->outstate = charset_init_state;
	}
	ctx->curr_output_charset = output_charset;

	while ( (inret = charset_to_unicode(&text, &len, midbuf,
					    lenof(midbuf), charset,
					    &ctx->instate, NULL, 0)) > 0) {
	    midlen = inret;
	    midptr = midbuf;
	    while ( (midret = charset_from_unicode(&midptr, &midlen, outbuf,
						   lenof(outbuf),
						   output_charset,
						   &ctx->outstate,
						   NULL)) > 0) {
		if (buf == &ctx->headers)
		    append_wrap(buf, &ctx->wrap, outbuf, midret);
		else
		    buffer_append(buf, outbuf, midret);
	    }
	}
    }
}

static char *get_fqdn(void)
{
    char hostname[512];
    struct hostent *h;

    if (gethostname(hostname, lenof(hostname))) {
	perror("gethostname");
	exit(1);
    }
    h = gethostbyname(hostname);
    if (!h) {
	herror("gethostbyname");
	exit(1);
    }
    return dupstr(h->h_name);
}

void send_message(int charset, char *message, int msglen)
{
    struct send_output_ctx ctx;
    static const int charset_list[] = {
	/*
	 * FIXME: We really need this list to be user-configurable.
	 */
	CS_ASCII, CS_ISO8859_1, CS_ISO8859_15, CS_UTF8
    };

    ctx.main_instate = ctx.instate = ctx.outstate = charset_init_state;
    ctx.default_input_charset = ctx.curr_input_charset = charset;
    ctx.curr_output_charset = CS_ASCII;
    ctx.charset_list = charset_list;
    ctx.ncharsets = lenof(charset_list);
    buffer_init(&ctx.headers);
    buffer_init(&ctx.pre2047);
    buffer_init(&ctx.specialhdr_text);
    ctx.specialhdr_desc = NULL;
    ctx.pre2047type = TYPE_HEADER_DECODED_TEXT;
    ctx.gotmid = ctx.gotdate = FALSE;
    ctx.parts = NULL;
    ctx.nparts = ctx.partsize = 0;
    ctx.attachments = NULL;
    ctx.nattach = ctx.attachsize = 0;
    wrap_init(&ctx.wrap);

    parse_message(message, msglen, send_output_fn, &ctx,
		  send_info_fn, &ctx, charset);
    /*
     * Send a blank TYPE_HEADER_NAME record, which will ensure
     * that the final header's text has finished being processed.
     */
    send_output_fn(&ctx, "", 0, TYPE_HEADER_NAME, CS_ASCII);

    /*
     * No matter what happened just now, we should expect to see a
     * single newline at the end of the headers.
     */
    assert(ctx.headers.length > 0 &&
	   ctx.headers.text[ctx.headers.length-1] == '\n');
    assert(!(ctx.headers.length > 1 &&
	     ctx.headers.text[ctx.headers.length-2] == '\n'));

    printf("%.*s", ctx.headers.length, ctx.headers.text);

    /*
     * Invent a Date header if it isn't already present.
     */
    if (!ctx.gotdate) {
	time_t t, lt, gt;
	struct tm tm, tm2, gtm;
	int totaldiff;

	static const char weekdays[4*7] =
	    "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat\0";
	static const char months[4*12] =
	    "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec\0";

	t = time(NULL);
	tm = *localtime(&t);
	/*
	 * Finding the difference between this and UTC is fiddly,
	 * because the DST and the actual time difference must be
	 * handled separately. First find the DST difference:
	 */
	tm2 = tm;
	tm2.tm_isdst = 0;
	lt = mktime(&tm2);
	/* Now the difference between t and lt is the DST difference. */
	gtm = *gmtime(&lt);
	gtm.tm_isdst = 0;	       /* just to make absolutely sure! */
	gt = mktime(&gtm);
	/* Now the difference between lt and gt is the TZ difference. */
	totaldiff = (int)difftime(t, lt) - (int)difftime(lt, gt);

	printf("Date: %s, %d %s %d %02d:%02d:%02d %c%02d%02d\n",
	       weekdays+4*tm.tm_wday, tm.tm_mday, months+4*tm.tm_mon,
	       1900+tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec,
	       (totaldiff > 0 ? '-' : '+'),
	       abs(totaldiff)/3600, abs(totaldiff)/60%60);
    }

    /*
     * Invent a Message-ID as well.
     */
    if (!ctx.gotmid) {
	/*
	 * Our Message-IDs begin with `Timber.' to distinguish them
	 * from anyone else's; after that I'm just going to display
	 * the current time, the pid, and a disambiguator.
	 */
	struct tm tm, tm2;
	char idbuf[128];
	char *fqdn;
	time_t t1, t2, now;
	static int disambiguator = 0;

	tm.tm_year = 70;
	tm.tm_mon = 0;
	tm.tm_mday = 1;
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	tm.tm_isdst = 0;

	t1 = mktime(&tm);
	tm2 = *gmtime(&t1);
	tm2.tm_isdst = 0;	       /* can't be too careful */
	t2 = mktime(&tm2);

	now = time(NULL);

	sprintf(idbuf, "Timber.%.0f.%d", difftime(now, t1) + difftime(t2, t1),
		getpid());

	if (disambiguator)
	    sprintf(idbuf + strlen(idbuf), "-%d", disambiguator);
	disambiguator++;

	fqdn = get_fqdn();
	printf("Message-ID: <%s@%s>\n", idbuf, fqdn);
	sfree(fqdn);
    }

    /*
     * Now construct the body of the message, by going through all
     * the MIME parts.
     */
    {
	struct finalpart {
	    char *headers;
	    char *data;
	    int datalen;
	    int freedata;
	} *fps;
	int i, nfps;

	nfps = ctx.nparts + ctx.nattach;
	assert(nfps > 0);   /* should have an empty message body at least */
	fps = snewn(nfps, struct finalpart);

	for (i = 0; i < nfps; i++) {
	    char *major, *minor; int charset;   /* Content-Type */
	    int encoding;	          /* Content-Transfer-Encoding */
	    int disp; char *filename;     /* Content-Disposition */
	    char *desc; int desccs;       /* Content-Description */
	    char *data;
	    int len, maxhdrlen;
	    int freedata;
	    int eightbit;
	    char *p;

	    if (i < ctx.nparts) {
		/*
		 * If we're replicating a MIME part from the input
		 * into the output, then most of this is already
		 * done for us.
		 */
		major = ctx.parts[i].major;
		minor = ctx.parts[i].minor;
		encoding = ctx.parts[i].transfer_encoding;
		charset = ctx.parts[i].charset;
		disp = ctx.parts[i].disposition;
		filename = ctx.parts[i].filename;
		desc = ctx.parts[i].description;
		desccs = CS_UTF8;      /* rfc822.c always gives this */
		data = message + ctx.parts[i].offset;
		len = ctx.parts[i].length;
		freedata = FALSE;

		/*
		 * However, we now have to adjust the character
		 * set on any text/ input part to the first of our
		 * list of preferred charsets in which it fits.
		 */
		if (!istrcmp(major, "text")) {
		    /*
		     * First convert the entire text part into
		     * Unicode.
		     */
		    wchar_t *wdata;
		    const char *inbuf;
		    int inlen, wlen, wl2;
		    int k;

		    inbuf = data;
		    inlen = len;
		    wlen = charset_to_unicode(&inbuf, &inlen, NULL, -1,
					      charset, NULL, NULL, 0);
		    wdata = snewn(wlen, wchar_t);
		    inbuf = data;
		    inlen = len;
		    wl2 = charset_to_unicode(&inbuf, &inlen, wdata, wlen,
					     charset, NULL, NULL, 0);
		    assert(wl2 == wlen && inlen == 0);

		    /*
		     * Now do a dry-run conversion back into each
		     * of the charsets in the list. As soon as we
		     * find one which doesn't cause an error, we
		     * redo the run for real.
		     * 
		     * For the last charset in the list, we ignore
		     * errors, because we have nothing left to
		     * fall back to if one occurs.
		     */
		    for (k = 0; k < ctx.ncharsets; k++) {
			wchar_t const *winbuf;
			int winlen, olen, ol2, error;
			int cs = ctx.charset_list[k];

			winbuf = wdata;
			winlen = wlen;
			error = FALSE;
			olen = charset_from_unicode
			    (&winbuf, &winlen, NULL, -1, cs, NULL,
			     (k == ctx.ncharsets-1 ? NULL : &error));

			if (!error) {
			    assert(!freedata);   /* no need to free it here */

			    data = snewn(olen+1, char);
			    winbuf = wdata;
			    winlen = wlen;
			    ol2 = charset_from_unicode(&winbuf, &winlen,
						       data, olen,
						       cs, NULL, NULL);
			    assert(olen == ol2 && winlen == 0);
			    data[ol2] = '\0';
			    len = olen;
			    charset = cs;
			    freedata = TRUE;
			    break;
			}
		    }
		    assert(freedata);
		}
	    } else {
		/*
		 * If we're constructing an attachment from an
		 * Attachment header, we must start by acquiring
		 * the data.
		 */
		int j = i - ctx.nparts;
		int binary;

		major = ctx.attachments[j].major;
		minor = ctx.attachments[j].minor;
		binary = (istrcmp(major, "text") && istrcmp(major, "message"));
		freedata = TRUE;

		switch (ctx.attachments[j].where) {
		  case ATTACH_FILE:
		    {
			int ret, datasize;
			FILE *fp;

			datasize = len = 0;
			data = NULL;
			fp = fopen(ctx.attachments[j].name,
				   binary ? "rb" : "r");
			while (1) {
			    if (datasize < len + 4096) {
				datasize = (len + 4096) * 3 / 2;
				data = sresize(data, datasize, char);
			    }
			    ret = fread(data + len, 1, 4096, fp);
			    if (ret < 0) {
				perror(ctx.attachments[j].name);
				exit(1);
			    } else if (ret == 0) {
				break; /* clean EOF */
			    } else {
				len += ret;
			    }
			}
			fclose(fp);
		    }
		    break;
		  default: assert(!"NYI"); break;
		}

		/*
		 * Base64 encode if necessary. The rules are:
		 * 
		 *  - binary attachments are always encoded, to
		 *    prevent the mail system's line end
		 *    canonicalisation from affecting the content
		 *  - text/ attachments are encoded if they
		 *    contain any unprintable characters or fail
		 *    to terminate in a newline; their line ends
		 *    are canonicalised to the Internet-standard
		 *    CRLF before encoding
		 *  - all other text/ attachments are left alone
		 *  - message/ attachments are never encoded.
		 */
		if (binary)
		    encoding = BASE64;
		else if (!istrcmp(major, "text")) {
		    int k, m, newlines;
		    /*
		     * Search the data for non-printable
		     * characters. If we find any, or if the text
		     * doesn't terminate in a newline, we encode.
		     */
		    encoding = NO_ENCODING;
		    newlines = 0;
		    for (k = 0; k < len; k++) {
			unsigned char c = data[k];
			if ((c < 32 && c != '\n' && c != '\t') ||
			    (c >= 127 && c < 160))
			    encoding = BASE64;
			if (c == '\n')
			    newlines++;
		    }
		    if (len > 0 && data[len-1] != '\n')
			encoding = BASE64;
		    if (encoding == BASE64) {
			char *newdata = snewn(len + newlines, char);
			for (k = m = 0; k < len; k++) {
			    if (data[k] == '\n')
				newdata[m++] = '\r';
			    newdata[m++] = data[k];
			}
			sfree(data);
			data = newdata;
			len = m;
		    }
		} else
		    encoding = NO_ENCODING;

		if (encoding == BASE64) {
		    int maxlen = len * 65 / 48 + 80;
		    char *newdata = snewn(maxlen, char);
		    len = base64_encode((unsigned char *)data, len,
					newdata, TRUE);
		    assert(len < maxlen);
		    sfree(data);
		    data = newdata;
		}

		/*
		 * Now set all the other fiddly bits of MIME.
		 */
		charset = ctx.attachments[j].charset;   /* can be CS_NONE */
		disp = ATTACHMENT;  /* fixed policy for Attachment: headers */
		filename = ctx.attachments[j].filename;   /* can be NULL */
		desc = ctx.attachments[j].description;   /* can be NULL */
		desccs = charset == CS_NONE ?
		    charset_upgrade(CS_ASCII) : charset;
	    }

	    /*
	     * NO_ENCODING must become either "7bit" or "8bit",
	     * depending on whether there are any high-bit-set
	     * characters in the data.
	     */
	    if (encoding == NO_ENCODING) {
		int k;
		eightbit = FALSE;
		for (k = 0; k < len; k++)
		    if ((unsigned char)data[k] > 127) {
			eightbit = TRUE;
			break;
		    }
	    }

	    /*
	     * Now we can construct the MIME headers and data for
	     * the final transmission form of the MIME part.
	     */
	    maxhdrlen = 4096 + strlen(major) + strlen(minor) +
		(filename ? 2*strlen(filename) : 0) +
		(desc ? 10*strlen(desc) : 0);
	    fps[i].headers = snewn(maxhdrlen, char);
	    p = fps[i].headers;

	    p += sprintf(p, "Content-Type: %s/%s", major, minor);
	    if (charset != CS_NONE)
		p += sprintf(p, "; charset=%s", charset_to_mimeenc(charset));
	    p += sprintf(p, "\nContent-Transfer-Encoding: %s\n",
			 (encoding == BASE64 ? "base64" :
			  eightbit ? "8bit" : "7bit"));
	    if (disp != UNSPECIFIED || filename) {
		if (disp == UNSPECIFIED)
		    disp = ATTACHMENT; /* *shrug*, got to do _something_ */
		p += sprintf(p, "Content-Disposition: %s",
			     disp == INLINE ? "inline" : "attachment");
		if (filename) {
		    int k;

		    p += sprintf(p, "; filename=\"");

		    /*
		     * The filename is always in pure ASCII by
		     * this point (if we received it from
		     * rfc822.c, it already was, otherwise our
		     * parsing of the Attachment: header will have
		     * converted it).
		     * 
		     * Now we need to quote it as a "value", in
		     * accordance with RFC2045 section 5.1. This
		     * means we must put it in double quotes if it
		     * contains any of the characters
		     * ()<>@,;:\"/[]?= (but it's easier just to
		     * double-quote it always), and
		     * backslash-escape \ and " in particular.
		     */
		    for (k = 0; filename[k]; k++) {
			if (filename[k] == '\\' || filename[k] == '"')
			    *p++ = '\\';
			*p++ = filename[k];
		    }

		    p += sprintf(p, "\"");
		}
		p += sprintf(p, "\n");
	    }
	    if (desc) {
		char *post2047 =
		    rfc2047_encode(desc, strlen(desc), desccs,
				   ctx.charset_list, ctx.ncharsets,
				   TYPE_HEADER_DECODED_TEXT,
				   53 /* 74-len("Content-Description: ") */);
		p += sprintf(p, "Content-Description: %s\n", post2047);
		sfree(post2047);
	    }
	    assert(p - fps[i].headers < maxhdrlen);

	    /*
	     * And we already have the actual data.
	     */
	    fps[i].data = data;
	    fps[i].datalen = len;
	    fps[i].freedata = freedata;
	}

	/*
	 * Now actually do the output. This divides into two
	 * cases, one for multipart messages and one for
	 * single-part messages.
	 */
	printf("MIME-Version: 1.0\n");
	if (nfps > 1) {
	    char sepbuf[80], *sep;
	    int sepindex = 0;

	    /*
	     * Figure out what we'll use as a separator line.
	     */
	    sprintf(sepbuf, "--MIME-Separator-Line");
	    while (1) {
		int seplen = strlen(sepbuf);
		for (i = 0; i < nfps; i++) {
		    char *p = fps[i].data;
		    int len = fps[i].datalen - seplen;
		    if (len <= 0)
			continue;
		    do {
			if (!memcmp(p, sepbuf, seplen))
			    goto endloop;   /* double break */
			while (len > 0 && *p != '\n')
			    len--, p++;
			len--, p++;
		    } while (len > 0);
		}
		endloop:
		if (i == nfps)
		    break;
		sprintf(sepbuf, "--MIME-Separator-Line-%d", sepindex++);
	    }
	    sep = sepbuf+2;

	    /*
	     * Output the main MIME headers.
	     */
	    printf("Content-Type: multipart/mixed; boundary=\"%s\"\n", sep);

	    /*
	     * Terminate the header section.
	     */
	    printf("\n");

	    /*
	     * Output the MIME preamble.
	     */
	    printf("  This message is in MIME format. If your mailer does"
		   " not\n  support MIME, you may have trouble reading"
		   " the attachments.\n\n");

	    /*
	     * Now output each part.
	     */
	    for (i = 0; i < nfps; i++) {
		printf("--%s\n", sep);
		fputs(fps[i].headers, stdout);
		printf("\n");
		fwrite(fps[i].data, 1, fps[i].datalen, stdout);
		printf("\n");
	    }

	    /*
	     * And output the trailing separator, and we're done.
	     */
	    printf("--%s--\n", sep);
	} else {
	    /*
	     * The single-part case is much simpler. Just output
	     * the single part's MIME headers and data.
	     */
	    fputs(fps[0].headers, stdout);
	    printf("\n");
	    fwrite(fps[0].data, 1, fps[0].datalen, stdout);
	}

	for (i = 0; i < nfps; i++) {
	    sfree(fps[i].headers);
	    if (fps[i].freedata)
		sfree(fps[i].data);
	}
	sfree(fps);
    }

    buffer_cleanup(&ctx.headers);
    buffer_cleanup(&ctx.pre2047);
    buffer_cleanup(&ctx.specialhdr_text);
    if (ctx.parts) {
	int i;

	for (i = 0; i < ctx.nparts; i++)
	    free_mime_details(&ctx.parts[i]);
	sfree(ctx.parts);
    }
    if (ctx.attachments) {
	int i;

	for (i = 0; i < ctx.nattach; i++) {
	    sfree(ctx.attachments[i].name);
	    sfree(ctx.attachments[i].major);
	    sfree(ctx.attachments[i].minor);
	    sfree(ctx.attachments[i].description);
	    sfree(ctx.attachments[i].filename);
	}
	sfree(ctx.attachments);
    }
}

void send_from_stdin(int charset)
{
    char *message;
    int msglen;

    message = read_from_stdin(&msglen);

    send_message (charset, message, msglen);
}
