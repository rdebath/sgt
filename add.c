/*
 * add.c - add an entry to a Caltrap calendar
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "caltrap.h"

void caltrap_add(int nargs, char **args, int nphysargs, int type)
{
    Date sd, ed;
    Time st, et;
    int i;
    char msg[512], *p;
    struct entry ent;

    if (nargs < 1 || nargs > 4)
	fatal(err_addargno);
    assert(nargs <= nphysargs);

    /*
     * We MUST see a date as the first argument.
     */
    sd = parse_date(args[0]);
    if (sd == INVALID_DATE)
	fatal(err_date, args[0]);

    i = 1;

    /*
     * Now we might see a time.
     */
    st = NO_TIME;
    if (nargs > i) {
	st = parse_time(args[i]);
	if (st != INVALID_TIME)
	    i++;
	else
	    st = NO_TIME;
    }

    /*
     * Now we expect to see a second date or time or both.
     */
    if (nargs > i) {
	ed = parse_date(args[i]);
	if (ed != INVALID_DATE)
	    i++;

	if (nargs > i) {
	    et = parse_time(args[i]);
	    if (et == INVALID_TIME)
		fatal(err_time, args[i]);
	    i++;
	    if (ed == INVALID_DATE) {
		/*
		 * If an end time was specified but no end date, we
		 * assume ed == sd.
		 */
		ed = sd;
	    }
	} else
	    et = 0;

	/*
	 * In this case, we normalise a missing start time to
	 * midnight.
	 */
	if (st == NO_TIME)
	    st = 0;
    } else {
	/*
	 * If there was no end time, choose one appropriately. An
	 * entry specifying just a date as input is taken to last
	 * exactly one day; an entry specifying just a start
	 * date+time is taken to be instantaneous.
	 */
	if (st == NO_TIME) {
	    st = 0;
	    ed = sd + 1;
	    et = 0;
	} else {
	    ed = sd;
	    et = st;
	}
    }

    if (i < nargs)
	fatal(err_extraarg, args[i]);

    if (isatty(fileno(stdin))) {
	char *dfmt, *tfmt;
	dfmt = format_date_full(sd);
	tfmt = format_time(st);
	printf("New entry will run from %s %s\n", dfmt, tfmt);
	sfree(dfmt);
	sfree(tfmt);
	dfmt = format_date_full(ed);
	tfmt = format_time(et);
	printf("                     to %s %s\n", dfmt, tfmt);
	sfree(dfmt);
	sfree(tfmt);
	printf("Existing entries in the week surrounding the start point:\n");
	list_entries(sd - 3, 0, sd + 4, 0);
	printf("Now enter message, on a single line,\n"
	       "or press ^D or ^C to cancel the operation.\n"
	       "> ");
	fflush(stdout);
    }

    if (!fgets(msg, sizeof(msg), stdin))
	*msg = '\0';
    p = msg + strcspn(msg, "\r\n");
    if (!*p) {
	printf("\nOperation cancelled.\n");
	fflush(stdout);
	return;
    }
    *p = '\0';

    ent.id = -1;		       /* cause a new id to be allocated */
    ent.sd = sd;
    ent.st = st;
    ent.ed = ed;
    ent.et = et;
    ent.length = ent.period = 0;       /* FIXME: implement these */
    ent.type = type;
    ent.description = msg;
    db_add_entry(&ent);
}
