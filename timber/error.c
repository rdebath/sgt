/*
 * error.c: timber error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "timber.h"

/*
 * Error flags
 */
#define PREFIX 0x0001		       /* give `timber:' prefix */

static void do_error(int code, va_list ap) {
    char error[1024];
    char *sp, *sp2;
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
      case err_direxists:
        sprintf(error, "directory `%.200s' already exists;"
                " remove it before trying again", dirpath);
        flags = PREFIX;
        break;
      case err_noopendb:
        sp = va_arg(ap, char *);
        sp2 = va_arg(ap, char *);
        sprintf(error,
                "unable to open database `%.200s': %.200s",
                sp, sp2);
        flags = PREFIX;
        break;
      case err_dbfull:
        sprintf(error, "database contains an unmanageable number of messages");
        flags = PREFIX;
        break;
      case err_dberror:
        sp = va_arg(ap, char *);
        sprintf(error, "database error: %.200s", sp);
        flags = PREFIX;
        break;
      case err_perror:
        sp = va_arg(ap, char *);
	sp2 = va_arg(ap, char *);
        sprintf(error, "%.200s%s%.200s: %.200s",
		sp, sp2 ? ": " : "", sp2 ? sp2 : "", strerror(errno));
        flags = PREFIX;
        break;
      case err_nosuchmsg:
        sp = va_arg(ap, char *);
	sprintf(error, "message `%.200s' does not exist in database", sp);
	flags = PREFIX;
	break;
      case err_internal:
        sp = va_arg(ap, char *);
	sprintf(error, "internal problem: %s", sp);
	flags = PREFIX;
	break;
    }

    if (flags & PREFIX)
	fputs("timber: ", stderr);
    fputs(error, stderr);
    fputc('\n', stderr);
}

void fatal(int code, ...) {
    va_list ap;
    va_start(ap, code);
    do_error(code, ap);
    va_end(ap);
    sql_close_all();
    exit(EXIT_FAILURE);
}

void error(int code, ...) {
    va_list ap;
    va_start(ap, code);
    do_error(code, ap);
    va_end(ap);
}
