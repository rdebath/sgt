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
    err_perror,			       /* generic errno error (%s: %s) */
};

/*
 * malloc.c
 */
void *smalloc(int size);
void *srealloc(void *p, int size);
void sfree(void *p);

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
void db_init(void);
void db_close(void);
int cfg_get_int(char *key);
char *cfg_get_str(char *key);
void cfg_set_str(char *key, char *str);
void cfg_set_int(char *key, int val);

/*
 * main.c
 */
extern char *dirpath;

/*
 * store.c
 */
struct storage {
    char *(*store_literal)(char *message, int msglen);
    void (*store_init)(void);
};
char *store_literal(char *message, int msglen);
void store_init(void);

/*
 * mboxstore.c
 */
const struct storage mbox_store;

/*
 * config.c
 */
int cfg_default_int(char *key);
char *cfg_default_str(char *key);

/*
 * mboxread.c
 */
void import_mbox_folder(char *folder);

/*
 * base64.c
 */
int base64_decode_length(int input_length);
int base64_encode_length(int input_length);
int base64_decode(const char *input, int length, unsigned char *output);
int base64_encode(const unsigned char *input, int length,
		  char *output, int multiline);

/*
 * qp.c
 */
int qp_decode(const char *input, int length, char *output, int rfc2047);

/*
 * rfc822.c
 */
struct mime_details {
    /*
     * This structure describes details of a single MIME part.
     */
    char *major, *minor;	       /* dynamically allocated */
    enum { NO_ENCODING, QP, BASE64, UUENCODE } transfer_encoding;
    int charset;
    int cd_inline;
    /*
     * A file name can come from a `name' parameter in
     * Content-Type, or a `filename' parameter in
     * Content-Disposition. The former is deprecated in favour of
     * the latter (as stated in RFC 2046), so the latter takes
     * priority. Hence, we track at all times where our current
     * filename has come from.
     */
    enum { NO_FNAME, CT_NAME, CD_FILENAME } filename_location;
    char *filename;		       /* dynamically allocated */
    char *boundary;		       /* dynamically allocated */
    char *description;		       /* dynamically allocated */
    /*
     * These two define the substring of the message itself which
     * contains the actual data of the MIME part.
     */
    int offset;
    int length;
};
struct message_id {
    char *mid;
    int index;			       /* which of the References */
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
	struct message_id mid;
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
    TYPE_BODY_TEXT
};
typedef void (*parser_info_fn_t)(void *ctx, struct message_parse_info *info);

void null_output_fn(void *ctx, const char *text, int len,
		    int type, int charset);
void null_info_fn(void *ctx, struct message_parse_info *info);
void parse_message(const char *message, int msglen,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx);

void parse_for_db(const char *message, int msglen);

/*
 * rfc2047.c
 */
void rfc2047(const char *text, int length, parser_output_fn_t output,
	     void *outctx, int structured, int dequote, int default_charset);

/*
 * date.c
 */
time_t mktimegm(struct tm *tm);

#endif
