/*
 * list.c - add an entry to a Caltrap calendar
 */

#include <assert.h>
#include <stdio.h>
#include "caltrap.h"

static void list_callback(void *ctx, Date d, Time t, char *msg)
{
    char *dfmt, *tfmt;

    dfmt = format_date_full(d);
    tfmt = format_time(t);
    printf("%s %s %s\n", dfmt, tfmt, msg);
    sfree(dfmt);
    sfree(tfmt);
}

int caltrap_list(int nargs, char **args, int nphysargs)
{
    Date sd, ed;
    Time t;

    if (nargs > 2)
	fatal(err_addargno);
    assert(nargs <= nphysargs);

    if (nargs == 0) {
	sd = today();
	ed = sd + 21;		       /* three weeks */
    } else if (nargs == 1) {
	sd = parse_date(args[0]);
	if (sd == INVALID_DATE)
	    fatal(err_date, args[0]);
	ed = sd;
    } else if (nargs == 2) {
	sd = parse_date(args[0]);
	if (sd == INVALID_DATE)
	    fatal(err_date, args[0]);
	ed = parse_date(args[1]);
	if (ed == INVALID_DATE)
	    fatal(err_date, args[1]);
	if (sd > ed) {
	    Date tmp = sd; sd = ed; ed = tmp;
	}
    }

    db_list_entries(sd, ed, list_callback, NULL);
}
