/*
 * error.c: caltrap error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "caltrap.h"

/*
 * Error flags
 */
#define PREFIX 0x0001		       /* give `caltrap:' prefix */

static void do_error(int code, va_list ap) {
    char error[1024];
    char *sp;
    int i;
    int flags;

    switch(code) {
      case err_nomemory:	       /* no arguments */
	sprintf(error, "out of memory");
	flags = PREFIX;
	break;
      case err_optnoarg:
	sp = va_arg(ap, char *);
	sprintf(error, "option `-%.200s' requires an argument", sp);
	flags = PREFIX;
	break;
      case err_nosuchopt:
	sp = va_arg(ap, char *);
	sprintf(error, "unrecognised option `-%.200s'", sp);
	flags = PREFIX;
	break;
      case err_eventtype:
	sp = va_arg(ap, char *);
	sprintf(error, "unrecognised event type `%.200s'", sp);
	flags = PREFIX;
	break;
      case err_extraarg:
	sp = va_arg(ap, char *);
	sprintf(error, "unexpected additional argument `%.200s'", sp);
	flags = PREFIX;
	break;
      case err_addargno:
	sprintf(error, "`add' command expects between one and four arguments");
	flags = PREFIX;
	break;
      case err_listargno:
	sprintf(error, "`list' command expects at most two arguments");
	flags = PREFIX;
	break;
      case err_cronargno:
	sprintf(error, "`cron' command expects exactly two arguments");
	flags = PREFIX;
	break;
      case err_dumpargno:
	sprintf(error, "`dump' command expects no arguments");
	flags = PREFIX;
	break;
      case err_loadargno:
	sprintf(error, "`load' command expects at most one argument");
	flags = PREFIX;
	break;
      case err_infoargno:
	sprintf(error, "`info' command expects exactly one argument");
	flags = PREFIX;
	break;
      case err_loadfmt:
	sprintf(error, "unable to parse dump file when reloading");
	flags = PREFIX;
	break;
      case err_date:
	sp = va_arg(ap, char *);
	sprintf(error, "unable to parse date `%.200s'", sp);
	flags = PREFIX;
	break;
      case err_time:
	sp = va_arg(ap, char *);
	sprintf(error, "unable to parse time `%.200s'", sp);
	flags = PREFIX;
	break;
      case err_duration:
	sp = va_arg(ap, char *);
	sprintf(error, "unable to parse duration `%.200s'", sp);
	flags = PREFIX;
	break;
      case err_nodb:
	sprintf(error,
		"database `%.200s' does not exist; try `caltrap --init'",
		dbpath);
	flags = PREFIX;
	break;
      case err_dbexists:
	sprintf(error, "database `%.200s' already exists;"
		" remove it before trying again", dbpath);
	flags = PREFIX;
	break;
      case err_noopendb:
	sp = va_arg(ap, char *);
	sprintf(error,
		"unable to open database `%.200s': %.200s",
		dbpath, sp);
	flags = PREFIX;
	break;
      case err_dberror:
	sp = va_arg(ap, char *);
	sprintf(error, "database error: %.200s", sp);
	flags = PREFIX;
	break;
      case err_cronpipe:
	sprintf(error, "error opening pipe: %.200s", strerror(errno));
	flags = PREFIX;
	break;
      case err_idnotfound:
	i = va_arg(ap, int);
	sprintf(error, "no entry with id %d exists in database", i);
	flags = PREFIX;
	break;
    }

    if (flags & PREFIX)
	fputs("caltrap: ", stderr);
    fputs(error, stderr);
    fputc('\n', stderr);
}

void fatal(int code, ...) {
    va_list ap;
    va_start(ap, code);
    do_error(code, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void error(int code, ...) {
    va_list ap;
    va_start(ap, code);
    do_error(code, ap);
    va_end(ap);
}
