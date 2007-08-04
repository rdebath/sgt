/*
 * send.c: Timber mail-sending functionality.
 */

/*
 * TODO:
 * 
 *  - instead of accumulating the received headers in a single
 *    buffer, we should break them down and store them one by one,
 *    because some of them we're going to interpret ourselves.
 *     + Attach and Attachment headers should add external data to
 * 	 our list of MIME parts to be sent. Probably another one
 * 	 which extracts a message from the database by ego, for
 * 	 forwarding.
 *     + Queue header should do something, although as yet I'm
 * 	 uncertain of exactly what.
 *     + MIME headers from the input should be thrown away,
 * 	 because we're going to construct our own.
 *     + I think it would be good to sort the remaining headers
 * 	 into a sensible order.
 * 
 *  - Then we should construct our outgoing message by means of:
 *     + enumerating the MIME parts (both from the input message
 * 	 and from attachment headers)
 *     + if there's exactly one, add its content-type headers to
 * 	 the main message and use its data as the message body
 *     + otherwise, add multipart headers to the main message and
 * 	 construct a multipart message body.
 * 
 *  - At some point we need to distinguish `send-format' (given a
 *    message on input in the form the user will have written it,
 *    format it into a proper RFC822 message for sending) from
 *    `send' (do that, then actually _send_ it, and also Fcc it).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

struct send_output_ctx {
    charset_state main_instate, instate;
    int default_input_charset, curr_input_charset;
    charset_state outstate;
    int curr_output_charset;
    struct wrap_details wrap;
    const int *charset_list;
    int ncharsets;
    struct buffer headers, pre2047;
    int pre2047type;
    int gotdate, gotmid;
    struct mime_details *parts;
    int nparts, partsize;
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

    if (is_type_header_decoded(type)) {
	output_charset = CS_UTF8;
	buf = &ctx->pre2047;
	ctx->pre2047type = type;
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
					       &ctx->outstate, NULL)) > 0) {
	    if (buf == &ctx->headers)
		append_wrap(buf, &ctx->wrap, outbuf, midret);
	    else
		buffer_append(buf, outbuf, midret);
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

void send_message (int charset, char *message, int msglen)
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
    ctx.pre2047type = TYPE_HEADER_DECODED_TEXT;
    ctx.gotmid = ctx.gotdate = FALSE;
    ctx.parts = NULL;
    ctx.nparts = ctx.partsize = 0;
    wrap_init(&ctx.wrap);

    parse_message(message, msglen, send_output_fn, &ctx,
		  send_info_fn, &ctx, charset);
    /* Ensure the last RFC2047 string has been finished, just in case. */
    send_output_fn(&ctx, "", 0, TYPE_HEADER_TEXT, CS_ASCII);

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

	sprintf(idbuf, "Timber-%.0f-%d", difftime(now, t1) + difftime(t2, t1),
		getpid());

	if (disambiguator)
	    sprintf(idbuf + strlen(idbuf), "-%d", disambiguator);
	disambiguator++;

	fqdn = get_fqdn();
	printf("Message-ID: <%s@%s>\n", idbuf, fqdn);
	sfree(fqdn);
    }

    if (ctx.parts) {
	int i;

	for (i = 0; i < ctx.nparts; i++)
	    free_mime_details(&ctx.parts[i]);
	sfree(ctx.parts);
    }
}

void send_from_stdin(int charset)
{
    char *message;
    int msglen;

    message = read_from_stdin(&msglen);

    send_message (charset, message, msglen);
}
