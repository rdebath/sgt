#ifndef CALTRAP_CALTRAP_H
#define CALTRAP_CALTRAP_H

#ifdef __GNUC__
#define NORETURN __attribute__((__noreturn__))
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define lenof(a) ( (sizeof((a))) / (sizeof(*(a))) )

/*
 * error.c
 */
void fatal(int code, ...) NORETURN;
void error(int code, ...);
enum {
    err_nomemory,		       /* no arguments */
    err_optnoarg,		       /* option `-%s' requires an argument */
    err_nosuchopt,		       /* unrecognised option `-%s' */
    err_addargno,		       /* `add' requires 1 or 2 args */
    err_listargno,		       /* `list' requires 0-2 args */
    err_cronargno,		       /* `cron' requires 2 args */
    err_date,		               /* unable to parse date `%s' */
    err_time,		               /* unable to parse time `%s' */
    err_nodb,		               /* db doesn't exist, try --init */
    err_dbexists,		       /* db _does_ exist, danger for --init */
    err_noopendb,		       /* unable to open db */
    err_dberror,		       /* generic db error */
    err_cronpipe,		       /* error opening pipe for -C */
};

/*
 * datetime.c
 */
typedef long Date;
typedef long Time;
#define INVALID_DATE ((Date)-2)
#define NO_DATE ((Date)-1)
#define INVALID_TIME ((Time)-2)
#define NO_TIME ((Time)-1)
Date parse_date(char *str);
Time parse_time(char *str);
char *format_date(Date d);
char *format_date_full(Date d);
char *format_time(Time t);
void now(Date *dd, Time *tt);
Date today(void);

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
const char *const version;

/*
 * add.c
 */
int caltrap_add(int nargs, char **args, int nphysargs);

/*
 * list.c
 */
int list_entries(Date sd, Date ed);
int caltrap_list(int nargs, char **args, int nphysargs);

/*
 * cron.c
 */
int caltrap_cron(int nargs, char **args, int nphysargs);

/*
 * main.c
 */
char *dbpath;

/*
 * Interface to database (sqlite.c, but potentially other back ends
 * some day).
 */
void db_init(void);
void db_add_entry(Date d, Time t, char *msg);
typedef void (*list_callback_fn_t)(void *, Date, Time, char *);
void db_list_entries(Date start, Time starttime,
                     Date end, Time endtime,
                     list_callback_fn_t fn, void *ctx);

#endif
