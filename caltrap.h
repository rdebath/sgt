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
    err_eventtype,		       /* unrecognised event type `%s' */
    err_extraarg,		       /* unexpected additional argument */
    err_addargno,		       /* `add' requires 1-4 args */
    err_listargno,		       /* `list' requires 0-2 args */
    err_cronargno,		       /* `cron' requires 2 args */
    err_dumpargno,		       /* `dump' requires no args */
    err_loadargno,		       /* `load' requires 0-1 args */
    err_infoargno,		       /* `info' requires 1 arg */
    err_editargno,		       /* `edit' requires 1 arg */
    err_delargno,		       /* `delete' requires 1 arg */
    err_loadfmt,		       /* error parsing dump file */
    err_date,		               /* unable to parse date `%s' */
    err_time,		               /* unable to parse time `%s' */
    err_datetime,		       /* unable to parse date+time `%s' */
    err_duration,		       /* unable to parse duration `%s' */
    err_nodb,		               /* db doesn't exist, try --init */
    err_dbexists,		       /* db _does_ exist, danger for --init */
    err_noopendb,		       /* unable to open db */
    err_dberror,		       /* generic db error */
    err_dbconsist,		       /* db consistency error */
    err_dbfull,			       /* db out of entry IDs */
    err_cronpipe,		       /* error opening pipe for -C */
    err_idnotfound,		       /* no entry with id %d exists in db */
};

typedef long Date;
typedef long Time;
typedef long long Duration;

struct entry {
    int id;
    Date sd, ed;
    Time st, et;
    Duration length;
    Duration period;
    int type;
    char *description;
};
enum {
    /*
     * This ordering is purely internal, since the database and the
     * dump files both store entry types as strings. Hence I choose
     * the ordering to suit the implementation: the holidays come
     * before everything else so that when we come to print events
     * in list mode we already know what colour to paint the day
     * line.
     */
    T_HOL1, T_HOL2, T_HOL3, T_EVENT, T_TODO,
    INVALID_TYPE = -1
};
#define is_hol(type) ( (type) >= T_HOL1 && (type) <= T_HOL3 )

/*
 * datetime.c
 */
#define INVALID_DATE ((Date)-2)
#define NO_DATE ((Date)-1)
#define INVALID_TIME ((Time)-2)
#define NO_TIME ((Time)-1)
#define INVALID_DURATION ((Duration)-2)
Date parse_date(char *str);
int parse_partial_date(char *str, Date *start, Date *after);
Time parse_time(char *str);
int parse_datetime(char *str, Date *d, Time *t);  /* returns zero on failure */
Duration parse_duration(char *str);
void add_to_datetime(Date *d, Time *t, Duration dur);
Duration datetime_diff(Date d1, Time t1, Date d2, Time t2);
int datetime_cmp(Date d1, Time t1, Date d2, Time t2);
char *format_date(Date d);
char *format_date_full(Date d);
char *format_time(Time t);
char *format_datetime(Date d, Time t);
char *format_duration(Duration d);
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
void caltrap_add(int nargs, char **args, int nphysargs, struct entry *e);

/*
 * list.c
 */
void list_entries(Date sd, Time st, Date ed, Time et);
void caltrap_list(int nargs, char **args, int nphysargs, int verbose);

/*
 * cron.c
 */
void caltrap_cron(int nargs, char **args, int nphysargs);

/*
 * dump.c
 */
void caltrap_dump(int nargs, char **args, int nphysargs);
void caltrap_load(int nargs, char **args, int nphysargs);

/*
 * info.c
 */
void caltrap_info(int nargs, char **args, int nphysargs);

/*
 * del.c
 */
void caltrap_del(int nargs, char **args, int nphysargs);

/*
 * edit.c
 */
void caltrap_edit(int nargs, char **args, int nphysargs, struct entry *e);

/*
 * main.c
 */
char *dbpath;

/*
 * misc.c
 */
int name_to_type(const char *name);
const char *type_to_name(int type);
int get_line_width(void);
int is_ecma_term(void);

/*
 * Interface to database (sqlite.c, but potentially other back ends
 * some day).
 */
void db_init(void);
void db_add_entry(struct entry *);
typedef void (*list_callback_fn_t)(void *, struct entry *);
void db_list_entries(Date sd, Time st, Date ed, Time et,
                     list_callback_fn_t fn, void *ctx);
void db_dump_entries(list_callback_fn_t fn, void *ctx);
void db_fetch(int id, struct entry *ent);   /* smallocs ent->description */
void db_del(int id);
void db_update(struct entry *e);
void db_close(void);

#endif
