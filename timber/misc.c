/*
 * misc.c: miscellaneous helper functions for Timber.
 */

#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "timber.h"
#include "charset.h"

/*
 * At some point these functions will require inverses.
 */
static const struct idname {
    int id;
    const char *name;
} header_names[] = {
    { H_BCC, "Bcc" },
    { H_CC, "Cc" },
    { H_CONTENT_DESCRIPTION, "Content-Description" },
    { H_CONTENT_DISPOSITION, "Content-Disposition" },
    { H_CONTENT_TRANSFER_ENCODING, "Content-Transfer-Encoding" },
    { H_CONTENT_TYPE, "Content-Type" },
    { H_DATE, "Date" },
    { H_FROM, "From" },
    { H_IN_REPLY_TO, "In-Reply-To" },
    { H_MESSAGE_ID, "Message-ID" },
    { H_REFERENCES, "References" },
    { H_REPLY_TO, "Reply-To" },
    { H_RESENT_BCC, "Resent-Bcc" },
    { H_RESENT_CC, "Resent-Cc" },
    { H_RESENT_FROM, "Resent-From" },
    { H_RESENT_REPLY_TO, "Resent-Reply-To" },
    { H_RESENT_SENDER, "Resent-Sender" },
    { H_RESENT_TO, "Resent-To" },
    { H_RETURN_PATH, "Return-Path" },
    { H_SENDER, "Sender" },
    { H_SUBJECT, "Subject" },
    { H_TO, "To" },
}, encoding_names[] = {
    { QP, "qp" },
    { BASE64, "base64" },
    { NO_ENCODING, "clear" },
}, disposition_names[] = {
    { INLINE, "inline" },
    { ATTACHMENT, "attachment" },
    { UNSPECIFIED, "unspecified-disposition" },
};

static int name_to_id(const struct idname *table, int len, const char *name)
{
    int i;
    for (i = 0; i < len; i++)
	if (!strcmp(name, table[i].name))
	    return table[i].id;
    return -1;
}
static const char *id_to_name(const struct idname *table, int len,
			      int id, const char *def)
{
    int i;
    for (i = 0; i < len; i++)
	if (id == table[i].id)
	    return table[i].name;
    return def;
}

const char *header_name(int header_id)
{
    return id_to_name(header_names, lenof(header_names),
		      header_id, "[unknown-header]");
}

const char *encoding_name(int encoding)
{
    return id_to_name(encoding_names, lenof(encoding_names),
		      encoding, "clear");
}

const char *disposition_name(int disposition)
{
    return id_to_name(disposition_names, lenof(disposition_names),
		      disposition, "unspecified-disposition");
}

int header_val(const char *header_name)
{
    return name_to_id(header_names, lenof(header_names), header_name);
}

int encoding_val(const char *encoding_name)
{
    return name_to_id(encoding_names, lenof(encoding_names), encoding_name);
}

int disposition_val(const char *disposition_name)
{
    return name_to_id(disposition_names, lenof(disposition_names),
		      disposition_name);
}

/*
 * Wrapper on write(1) that guarantees to either write all its data
 * or die trying.
 */
int write_wrapped(int fd, char *data, int length)
{
    while (length > 0) {
	int ret = write(fd, data, length);
	if (ret <= 0) {
	    error(err_perror, "write", NULL);
	    return FALSE;
	}
	length -= ret;
	data += ret;
    }

    return TRUE;
}

/*
 * Function that comes in handy in a couple of places: read all of
 * stdin into an internal string.
 */
char *read_from_stdin(int *len)
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
	    *len = 0;
	    return NULL;
	}
	if (ret == 0)
	    break;

	for (i = j = 0; i < ret; i++) {
	    if (message[msglen + i] != '\r')
		message[msglen + j++] = message[msglen + i];
	}
	msglen += j;
    }

    *len = msglen;
    return message;
}

void init_mime_details(struct mime_details *md)
{
    md->major = dupstr("text");
    md->minor = dupstr("plain");
    md->transfer_encoding = NO_ENCODING;
    md->charset = CS_ASCII;
    md->disposition = UNSPECIFIED;
    md->filename = NULL;
    md->description = NULL;
    md->offset = 0;
    md->length = 0;
}

void free_mime_details(struct mime_details *md)
{
    sfree(md->major);
    sfree(md->minor);
    sfree(md->filename);
    sfree(md->description);
}

char *dupstr(const char *s)
{
    char *ret = snewn(1+strlen(s), char);
    strcpy(ret, s);
    return ret;
}

int istrlencmp(const char *s1, int l1, const char *s2, int l2)
{
    int cmp;

    while (l1 > 0 && l2 > 0) {
	cmp = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
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

int istrcmp(const char *s1, const char *s2)
{
    return istrlencmp(s1, strlen(s1), s2, strlen(s2));
}
