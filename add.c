/*
 * add.c - add an entry to a Caltrap calendar
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "caltrap.h"

void caltrap_add(int nargs, char **args, int nphysargs)
{
    Date d;
    Time t;
    char msg[512];
    struct entry ent;

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
	list_entries(d - 3, 0, d + 4, 0);
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

    /*
     * FIXME: Faff egregiously with this routine so as to work with
     * the generality of the new entry structure.
     */

    ent.id = -1;		       /* cause a new id to be allocated */
    ent.sd = ent.ed = d;
    ent.st = ent.et = t;
    if (t == NO_TIME) {
	ent.ed++;
	ent.st = ent.et = 0;
    }
    ent.length = ent.period = 0;
    ent.type = T_EVENT;
    ent.description = msg;
    db_add_entry(&ent);
}
