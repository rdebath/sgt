#ifndef TIMBER_TIMBER_H
#define TIMBER_TIMBER_H

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
typedef void (*parser_output_fn_t)(void *ctx, const char *text, int len,
				   int type, int charset);
enum {				       /* values for above `type' argument */
    TYPE_HEADER_NAME,
    TYPE_HEADER_TEXT,
    TYPE_BODY_TEXT
};
typedef void (*parser_info_fn_t)(void *ctx, int type,
				 const char *text, int len);
enum {				       /* values for above `type' argument */
    TYPE_SUBJECT,
    TYPE_FROM_ADDR,
    /* FIXME: fill in the rest of these... */
};
void null_output_fn(void *ctx, const char *text, int len,
		    int type, int charset);
void null_info_fn(void *ctx, int type, const char *text, int len);
void parse_message(const char *message, int msglen,
		   parser_output_fn_t output, void *outctx,
		   parser_info_fn_t info, void *infoctx);

void parse_for_db(const char *message, int msglen);

/*
 * rfc2047.c
 */
void rfc2047(const char *text, int length, parser_output_fn_t output,
	     void *outctx, int structured, int default_charset);

#endif
