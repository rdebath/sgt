/*
 * misc.c: miscellaneous helper functions for Timber.
 */

#include "timber.h"

/*
 * At some point these two functions will require inverses.
 */
const char *header_name(int header_id)
{
    if (header_id == H_BCC) return "Bcc";
    if (header_id == H_CC) return "Cc";
    if (header_id == H_CONTENT_DESCRIPTION) return "Content-Description";
    if (header_id == H_CONTENT_DISPOSITION) return "Content-Disposition";
    if (header_id == H_CONTENT_TRANSFER_ENCODING)
	return "Content-Transfer-Encoding";
    if (header_id == H_CONTENT_TYPE) return "Content-Type";
    if (header_id == H_DATE) return "Date";
    if (header_id == H_FROM) return "From";
    if (header_id == H_IN_REPLY_TO) return "In-Reply-To";
    if (header_id == H_MESSAGE_ID) return "Message-ID";
    if (header_id == H_REFERENCES) return "References";
    if (header_id == H_REPLY_TO) return "Reply-To";
    if (header_id == H_RESENT_BCC) return "Resent-Bcc";
    if (header_id == H_RESENT_CC) return "Resent-Cc";
    if (header_id == H_RESENT_FROM) return "Resent-From";
    if (header_id == H_RESENT_REPLY_TO) return "Resent-Reply-To";
    if (header_id == H_RESENT_SENDER) return "Resent-Sender";
    if (header_id == H_RESENT_TO) return "Resent-To";
    if (header_id == H_RETURN_PATH) return "Return-Path";
    if (header_id == H_SENDER) return "Sender";
    if (header_id == H_SUBJECT) return "Subject";
    if (header_id == H_TO) return "To";
    return "[unknown-header]";
}

const char *encoding_name(int encoding)
{
    if (encoding == QP) return "qp";
    if (encoding == BASE64) return "base64";
    if (encoding == UUENCODE) return "uuencode";
    return "clear";
}
