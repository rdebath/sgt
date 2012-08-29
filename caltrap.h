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
/* out of memory */
void fatalerr_nomemory(void) NORETURN;
/* option `-%s' requires an argument */
void err_optnoarg(const char *sp);
/* unrecognised option `-%s' */
void err_nosuchopt(const char *sp);
/* unrecognised event type `%s' */
void fatalerr_eventtype(const char *sp) NORETURN;
/* unexpected additional argument */
void fatalerr_extraarg(const char *sp) NORETURN;
/* `add' requires 1-4 args */
void fatalerr_addargno(void) NORETURN;
/* `list' requires 0-2 args */
void fatalerr_listargno(void) NORETURN;
/* `cron' requires 2 args */
void fatalerr_cronargno(void) NORETURN;
/* `dump' requires no args */
void fatalerr_dumpargno(void) NORETURN;
/* `load' requires 0-1 args */
void fatalerr_loadargno(void) NORETURN;
/* `info' requires 1 arg */
void fatalerr_infoargno(void) NORETURN;
/* `edit' requires 1 arg */
void fatalerr_editargno(void) NORETURN;
/* `delete' requires 1 arg */
void fatalerr_delargno(void) NORETURN;
/* error parsing dump file */
void fatalerr_loadfmt(void) NORETURN;
/* unable to parse date `%s' */
void fatalerr_date(const char *sp) NORETURN;
/* unable to parse time `%s' */
void fatalerr_time(const char *sp) NORETURN;
/* unable to parse date+time `%s' */
void fatalerr_datetime(const char *sp) NORETURN;
/* unable to parse duration `%s' */
void fatalerr_duration(const char *sp) NORETURN;
/* db doesn't exist, try --init */
void fatalerr_nodb(void) NORETURN;
/* db _does_ exist, danger for --init */
void fatalerr_dbexists(void) NORETURN;
/* unable to open db */
void fatalerr_noopendb(const char *sp) NORETURN;
/* generic db error */
void fatalerr_dberror(const char *sp) NORETURN;
/* db out of entry IDs */
void fatalerr_dbfull(void) NORETURN;
/* db consistency error */
void fatalerr_dbconsist(const char *sp) NORETURN;
/* error opening pipe for -C */
void fatalerr_cronpipe(void) NORETURN;
/* no entry with id %d exists in db */
void fatalerr_idnotfound(int i) NORETURN;

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
