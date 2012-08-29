/*
 * error.c: caltrap error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "caltrap.h"

static void do_error(const char *fmt, ...)
{
    va_list ap;

    fputs("caltrap: ", stderr);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}

void do_fatal(void) NORETURN;
void do_fatal(void)
{
    exit(EXIT_FAILURE);
}

void fatalerr_nomemory(void)
{
    do_error("out of memory");
    do_fatal();
}

void err_optnoarg(const char *sp)
{
    do_error("option `-%s' requires an argument", sp);
}

void err_nosuchopt(const char *sp)
{
    do_error("unrecognised option `-%s'", sp);
}

void fatalerr_eventtype(const char *sp)
{
    do_error("unrecognised event type `%s'", sp);
    do_fatal();
}

void fatalerr_extraarg(const char *sp)
{
    do_error("unexpected additional argument `%s'", sp);
    do_fatal();
}

void fatalerr_addargno(void)
{
    do_error("`add' command expects between one and four arguments");
    do_fatal();
}

void fatalerr_listargno(void)
{
    do_error("`list' command expects at most two arguments");
    do_fatal();
}

void fatalerr_cronargno(void)
{
    do_error("`cron' command expects exactly two arguments");
    do_fatal();
}

void fatalerr_dumpargno(void)
{
    do_error("`dump' command expects no arguments");
    do_fatal();
}

void fatalerr_loadargno()
{
    do_error("`load' command expects at most one argument");
    do_fatal();
}

void fatalerr_infoargno(void)
{
    do_error("`info' command expects exactly one argument");
    do_fatal();
}

void fatalerr_editargno(void)
{
    do_error("`edit' command expects exactly one argument");
    do_fatal();
}

void fatalerr_delargno(void)
{
    do_error("`delete' command expects exactly one argument");
    do_fatal();
}

void fatalerr_loadfmt(void)
{
    do_error("unable to parse dump file when reloading");
    do_fatal();
}

void fatalerr_date(const char *sp)
{
    do_error("unable to parse date `%s'", sp);
    do_fatal();
}

void fatalerr_time(const char *sp)
{
    do_error("unable to parse time `%s'", sp);
    do_fatal();
}

void fatalerr_datetime(const char *sp)
{
    do_error("unable to parse date and time `%s'", sp);
    do_fatal();
}

void fatalerr_duration(const char *sp)
{
    do_error("unable to parse duration `%s'", sp);
    do_fatal();
}

void fatalerr_nodb(void)
{
    do_error("database `%s' does not exist; try `caltrap --init'", dbpath);
    do_fatal();
}

void fatalerr_dbexists(void)
{
    do_error("database `%s' already exists;"
             " remove it before trying again", dbpath);
    do_fatal();
}

void fatalerr_noopendb(const char *sp)
{
    do_error("unable to open database `%s': %s", dbpath, sp);
    do_fatal();
}

void fatalerr_dberror(const char *sp)
{
    do_error("database error: %s", sp);
    do_fatal();
}

void fatalerr_dbfull(void)
{
    do_error("no free entry id in database");
    do_fatal();
}

void fatalerr_dbconsist(const char *sp)
{
    do_error("database consistency check failed: %s", sp);
    do_fatal();
}

void fatalerr_cronpipe(void)
{
    do_error("error opening pipe: %s", strerror(errno));
    do_fatal();
}

void fatalerr_idnotfound(int i)
{
    do_error("no entry with id %d exists in database", i);
    do_fatal();
}
