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

/*
 * error.c
 */
void fatal(int code, ...) NORETURN;
void error(int code, ...);
enum {
    err_nomemory,		       /* no arguments */
    err_optnoarg,		       /* option `-%s' requires an argument */
    err_nosuchopt,		       /* unrecognised option `-%s' */
    err_nodb,                          /* db doesn't exist, try init-db */
    err_dbexists,                      /* db _does_ exist, can't init */
    err_noopendb,                      /* unable to open db */
    err_dberror,                       /* generic db error */
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
extern char *dbpath;
void db_init(void);
void db_close(void);

#endif
