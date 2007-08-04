#ifndef TIMBER_TIMBER_H
#define TIMBER_TIMBER_H

#include <time.h>

#ifdef __GNUC__
#define NORETURN __attribute__((__noreturn__))
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define CAT2(x,y) x ## y
#define CAT1(x,y) CAT2(x,y)
#define cassert(x) enum { CAT1(c_assert_,__LINE__) = 1 / (x) }

/*
 * Explanation of one slightly odd piece of terminology below:
 * `ego' is the name for an internal identifier used to uniquely
 * refer to a message stored in a Timber mail store.
 * 
 * I had initially intended to index internally by Message-ID, but
 * on closer inspection this idea is clearly barmy for many
 * reasons:
 *  - multiple distinguishable copies of the same message can be
 *    present in the mail store with the same Message-ID. In
 *    particular, the Fcc of an outgoing message to a mailing list
 *    and the version received back from the mailing list will be a
 *    common pair of this type (and you'd quite _like_ to keep both
 *    copies, if only so you can spot any differences).
 *  - some messages in imported mail archives don't have a
 *    Message-ID at all because they were Fcced before it was put
 *    on, which is stupid but sadly not unheard of.
 *  - if a malicious attacker knew that deliberately reusing a
 *    Message-ID in a mail to me would splatter important parts of
 *    my mail archive, they would do it!
 * 
 * Thus, I invent my own internal identifiers, which are only
 * unique within the mail store and index database, but which I can
 * be _sure_ are unique in that context. I call them `ego', short
 * for `Message-Ego', which was Gareth Taylor's suggestion for a
 * snappy name that wasn't `Message-Id'. :-)
 */

/*
 * error.c
 */
void fatal(int code, ...) NORETURN;
void error(int code, ...);
enum {
    err_nomemory,		       /* no arguments */
    err_optnoarg,		       /* option `-%s' requires an argument */
    err_nosuchopt,		       /* unrecognised option `-%s' */
    err_direxists,		       /* dir exists, so can't init it */
    err_noopendb,                      /* unable to open db */
    err_dberror,                       /* generic db error */
    err_dbfull,			       /* more than 2^53 messages! */
    err_perror,			       /* generic errno error (%s: %s) */
    err_nosuchmsg,		       /* this message not present in db */
    err_internal,		       /* internal problem */
};

/*
 * malloc.c
 */
void *smalloc(int size);
void *srealloc(void *p, int size);
void sfree(void *p);

/*
 * Direct use of smalloc within the code should be avoided where
 * possible, in favour of these type-casting macros which ensure
 * you don't mistakenly allocate enough space for one sort of
 * structure and assign it to a different sort of pointer.
 */
#define snew(type) ((type *)smalloc(sizeof(type)))
#define snewn(n, type) ((type *)smalloc((n)*sizeof(type)))
#define sresize(ptr, n, type) ((type *)srealloc(ptr, (n)*sizeof(type)))

/*
 * help.c
 */
void help(void);
void usage(void);
void showversion(void);

/*
 * licence.c
 */
void licence(void);

/*
 * version.c
 */
extern const char *const version;

/*
 * sqlite.c
 */
void sql_init_all(void);
void sql_close_all(void);
int cfg_get_int(char *key);
char *cfg_get_str(char *key);
void cfg_set_str(char *key, char *str);
void cfg_set_int(char *key, int val);
void parse_for_db(const char *ego, const char *location,
		  const char *message, int msglen);
char *message_location(const char *ego);
struct mime_details *find_mime_parts(const char *ego, int *nparts);
void ab_display_attr_history (const char *contact_id, const char *attr_type);
void ab_display_attr (const char *contact_id, const char *attr_type);
void ab_change_attr (const char *contact_id, const char *attr_type,
		     const char *new_value);
void ab_display_contacts_for_attr_value (const char *attr_type,
					 const char *value);
void ab_add_contact(void);
void ab_list_contacts(void);

/*
 * main.c
 */
extern char *dirpath;
extern int nosync;

/*
 * store.c
 */
struct storage {
    char *(*store_literal)(char *message, int msglen, char *separator);
    void (*store_init)(void);
    char *(*store_retrieve)(const char *location, int *msglen,
			    char **separator);
};
char *store_literal(char *message, int msglen, char *separator);
void store_init(void);
char *store_retrieve(const char *location, int *msglen, char **separator);

/*
 * mboxstore.c
 */
/* This utility function is exported so that it can be used in export.c too. */
int write_mbox(int fd, char *data, int length);
const struct storage mbox_store;

/*
 * config.c
 */
int cfg_default_int(const char *key);
const char *cfg_default_str(const char *key);

/*
 * mboxread.c
 */
void import_mbox_folder(char *folder);

/*
 * base64.c
 */
int base64_decode_length(int input_length);
int base64_encode_length(int input_length, int multiline);
int base64_decode(const char *input, int length, unsigned char *output);
int base64_encode(const unsigned char *input, int length,
		  char *output, int multiline);

/*
 * qp.c
 */
int qp_decode(const char *input, int length, char *output, int rfc2047);
int qp_rfc2047_encode(const char *input, int length, char *output);

/*
 * rfc822.c
 */
struct mime_details {
    /*
     * This structure describes details of a single MIME part.
     */
    char *major, *minor;	       /* dynamically allocated */
    enum { NO_ENCODING, QP, BASE64 } transfer_encoding;
    int charset;
    enum { UNSPECIFIED, INLINE, ATTACHMENT } disposition;
    char *filename;		       /* dynamically allocated */
    char *description;		       /* dynamically allocated */
    /*
     * These two define the substring of the message itself which
     * contains the actual data of the MIME part.
     */
    int offset;
    int length;
};
struct address {
    char *display_name;
    char *address;
};
struct message_parse_info {
    enum {
	PARSE_ADDRESS,
	PARSE_MESSAGE_ID,
	PARSE_SUBJECT,
	PARSE_DATE,
	PARSE_MIME_PART
    } type;
    enum {
	H_BCC,
	H_CC,
	H_CONTENT_DESCRIPTION,
	H_CONTENT_DISPOSITION,
	H_CONTENT_TRANSFER_ENCODING,
	H_CONTENT_TYPE,
	H_DATE,
	H_FROM,
	H_IN_REPLY_TO,
	H_MESSAGE_ID,
	H_REFERENCES,
	H_REPLY_TO,
	H_RESENT_BCC,
	H_RESENT_CC,
	H_RESENT_FROM,
	H_RESENT_REPLY_TO,
	H_RESENT_SENDER,
	H_RESENT_TO,
	H_RETURN_PATH,
	H_SENDER,
	H_SUBJECT,
	H_TO
    } header;
    union {
	struct address addr;
	char *string;
	time_t date;
	struct mime_details md;
    } u;
};

typedef void (*parser_output_fn_t)(void *ctx, const char *text, int len,
				   int type, int charset);
enum {				       /* values for above `type' argument */
    TYPE_HEADER_NAME,
    TYPE_HEADER_TEXT,
    TYPE_HEADER_DECODED_TEXT,
    TYPE_HEADER_DECODED_PHRASE,
    TYPE_HEADER_DECODED_COMMENT,
    /*
     * The following constants are not used by the rfc822.c
     * parser. They are included in the enumeration so that other
     * modules in Timber (such as display.c) can augment the
     * stream of output blocks coming from the RFC822 parser with
     * other blocks generated outside it.
     */
    TYPE_ATTACHMENT_ID_LINE,
    TYPE_BODY_TEXT
};
#define is_type_header_text(t)    ( (t) >= TYPE_HEADER_TEXT && \
				    (t) <= TYPE_HEADER_DECODED_COMMENT )
#define is_type_header_decoded(t) ( (t) >= TYPE_HEADER_DECODED_TEXT && \
				    (t) <= TYPE_HEADER_DECODED_COMMENT )
typedef void (*parser_info_fn_t)(void *ctx, struct message_parse_info *info);

void null_output_fn(void *ctx, const char *text, int len,
		    int type, int charset);
void null_info_fn(void *ctx, struct message_parse_info *info);
void parse_message(const char *message, int msglen,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx,
		   int default_charset);

/*
 * rfc2047.c
 */
void rfc2047_decode(const char *text, int length, parser_output_fn_t output,
		    void *outctx, int type, int display,
		    int default_charset);
char *rfc2047_encode(const char *text, int length, int input_charset,
		     const int *output_charsets, int ncharsets,
		     int structured, int firstlen);

/*
 * date.c
 */
time_t mktimegm(struct tm *tm);
#define DATEBUF_MAX 64
void fmt_date(time_t t, char *buf);
time_t unfmt_date(const char *buf);

/*
 * misc.c
 */
const char *header_name(int header_id);
const char *encoding_name(int encoding);
const char *disposition_name(int disposition);
int header_val(const char *header_name);
int encoding_val(const char *encoding_name);
int disposition_val(const char *disposition_name);
int write_wrapped(int fd, char *data, int length);
char *read_from_stdin(int *len);
void init_mime_details(struct mime_details *md);
void copy_mime_details(struct mime_details *dst, struct mime_details *src);
void free_mime_details(struct mime_details *md);
char *dupstr(const char *s);
int istrlencmp(const char *s1, int l1, const char *s2, int l2);
int istrcmp(const char *s1, const char *s2);


/*
 * export.c
 */
void export_message(char *ego);
void export_as_mbox(char *ego);

/*
 * display.c
 */
enum { DISPLAY_BARE, DISPLAY_ANSI };
void display_msgtext(char *message, int msglen,
		     int charset, int type, int full);
void display_message(char *ego, int charset, int type, int full);

/*
 * boringhdr.c
 */
int is_boring_hdr(const char *hdr, int len);

/*
 * send.c
 */
void send_from_stdin(int charset);

#endif
