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

#endif
