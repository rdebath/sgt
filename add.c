/*
 * add.c - add an entry to a Caltrap calendar
 */

#include <stdio.h>
#include <assert.h>
#include "caltrap.h"

int caltrap_add(int nargs, char **args, int nphysargs)
{
    Date d;
    Time t;
    char msg[512];

    if (nargs < 1 || nargs > 2)
	fatal(err_addargno);
    assert(nargs <= nphysargs);

    d = parse_date(args[0]);
    if (d == INVALID_DATE)
	fatal(err_date, args[0]);

    if (nargs == 2) {
	t = parse_time(args[1]);
	if (t == INVALID_TIME)
	    fatal(err_time, args[1]);
    } else
	t = NO_TIME;

    if (isatty(fileno(stdin))) {
	char *dfmt, *tfmt;
	dfmt = format_date_full(d);
	tfmt = format_time(t);
	printf("New entry will be placed at %s %s\n", dfmt, tfmt);
	sfree(dfmt);
	sfree(tfmt);
	printf("Existing entries in the surrounding week:\n");
	list_entries(d - 3, d + 4);
	printf("Now enter message, on a single line,\n"
	       "or just press Return to cancel the operation.\n"
	       "> ");
	fflush(stdout);
    }

    if (!fgets(msg, sizeof(msg), stdin))
	return;
    msg[strcspn(msg, "\r\n")] = '\0';
    if (!msg[0])
	return;

    db_add_entry(d, t, msg);
}
