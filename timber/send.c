/*
 * send.c: Timber mail-sending functionality.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

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

struct send_output_ctx {
    charset_state main_instate, instate;
    int default_input_charset, curr_input_charset;
    charset_state outstate;
    int curr_output_charset;
    const int *charset_list;
    int ncharsets;
    struct buffer headers, pre2047;
};

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

    if (type == TYPE_HEADER_DECODED) {
	output_charset = CS_UTF8;
	buf = &ctx->pre2047;
    } else {
	if (ctx->pre2047.length > 0) {
	    char *post2047 =
		rfc2047_encode(ctx->pre2047.text, ctx->pre2047.length,
			       CS_UTF8, ctx->charset_list, ctx->ncharsets,
			       TRUE /* FIXME */, 75 /* FIXME */);
	    buffer_append(&ctx->headers, post2047, strlen(post2047));
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
	    buffer_append(buf, outbuf, midret);
	}
    }
}

void send(int charset, char *message, int msglen)
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

    parse_message(message, msglen, send_output_fn, &ctx,
		  null_info_fn, NULL, charset);
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
}

void send_from_stdin(int charset)
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
	    return;
	}
	if (ret == 0)
	    break;

	for (i = j = 0; i < ret; i++) {
	    if (message[msglen + i] != '\r')
		message[msglen + j++] = message[msglen + i];
	}
	msglen += j;
    }

    send(charset, message, msglen);
}
