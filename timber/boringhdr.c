/*
 * boringhdr.c: maintain an idea of which RFC822 headers are
 * considered `boring' and not shown except in full-headers mode.
 */

#include "timber.h"

int is_boring_hdr(const char *hdr, int len)
{
    int len2;

    for (len2 = 0; len2 < len && hdr[len2] && hdr[len2] != ':'; len2++);
    len = len2;

    if (len > 5 && !memcmp(hdr, "From ", 5))
	return TRUE;

    /*
     * For the moment, I'm just going to import the boring-headers
     * list from Timber 0 and use it directly. I haven't yet worked
     * out how much of this wants to be configuration material, how
     * much wants to be _default_ config material and how much
     * wants to be built in; but for the moment this will serve.
     */
    {
	static const char *const boringheaders[] = {
	    "Return-path",
	    "Envelope-to",
	    "Delivery-date",
	    "Received",
	    "X-Priority",
	    "X-MSMail-Priority",
	    "X-Mailer",
	    "MIME-Version",
	    "Content-Type",
	    "In-Reply-To",
	    "Message-Id",
	    "X-UIDL",
	    "Status",
	    "Return-Path",
	    "X-ESMTP-Daemon",
	    "X-PMFLAGS",
	    "X-Attachments",
	    "X-Status",
	    "Sender",
	    "Organization",
	    "Comments",
	    "X-Sender",
	    "References",
	    "Priority",
	    "X-MailServer",
	    "X-Article",
	    "Content-Length",
	    "X-Loop",
	    "Content-Id",
	    "Posted-Date",
	    "Received-Date",
	    "Lines",
	    "X-Newsreader",
	    "Precedence",
	    "Errors-To",
	    "X-X-Sender",
	    "X-URL",
	    "X-Juno-Line-Breaks",
	    "X-Confirm-Reading-To",
	    "X-pmrqc",
	    "Return-receipt-to",
	    "X-Newsgroups",
	    "X-Followup-To",
	    "Keywords",
	    "Summary",
	    "X-Automatically-Sent-By",
	    "X-MIME-Autoconverted",
	    "X-MimeOLE",
	    "X-Envelope-To",
	    "X-Originating-IP",
	    "X-VM-VHeader",
	    "X-VM-Last-Modified",
	    "X-VM-IMAP-Retrieved",
	    "X-VM-POP-Retrieved",
	    "X-VM-Bookmark",
	    "X-VM-v5-Data",
	    "X-VM-Labels",
	    "X-VM-Summary-Format",
	    "X-Lotus-FromDomain",
	    "Content-Disposition",
	    "X-MIMETrack",
	    "Content-Class",
	    "Thread-Topic",
	    "Thread-Index",
	    "List-Id",
	    "Content-Transfer-Encoding",
	    "X-MS-Has-Attach",
	    "X-MS-TNEF-Correlator",
	    "X-Mailman-Version",
	    "X-BeenThere",
	    "User-Agent",
	    "Delivered-To",
	    "X-Notes-Item",
	    "X-Spam-Level",
	    "X-Spam-Status",
	    "List-Help",
	    "List-Post",
	    "List-Unsubscribe",
	    "List-Subscribe",
	    "List-Archive",
	    "X-MailScanner",
	    "X-MailScanner-SpamCheck",
	    "Importance",
	    "X-OriginalArrivalTime",
	    "X-Accept-Language",
	    "X-nlh_msg",
	    "X-nlh_thread",
	};
	int i;

	for (i = 0; i < lenof(boringheaders); i++)
	    if (!istrlencmp(hdr, len, boringheaders[i],
			    strlen(boringheaders[i])))
		return TRUE;
    }

    return FALSE;
}
