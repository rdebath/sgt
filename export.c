/*
 * export.c: export a message from Timber in its transport form.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "timber.h"

void export_message(char *ego)
{
    char *location, *message, *separator;
    int msglen;

    location = message_location(ego);
    if (!location)
	error(err_nosuchmsg, ego);
    else {
	message = store_retrieve(location, &msglen, &separator);
	if (!message)
	    fatal(err_internal, "mail mentioned in db is not in mail store");
	write_wrapped(1, message, msglen);
	sfree(location);
	sfree(message);
	sfree(separator);
    }
}

void export_as_mbox(char *ego)
{
    char *location, *message, *separator;
    int msglen;

    location = message_location(ego);
    if (!location)
	error(err_nosuchmsg, ego);
    else {
	message = store_retrieve(location, &msglen, &separator);
	if (!message)
	    fatal(err_internal, "mail mentioned in db is not in mail store");
	if (!separator) {
	    char fromline[80];
	    struct tm tm;
	    time_t t;

	    t = time(NULL);
	    tm = *gmtime(&t);
	    strftime(fromline, 80,
		     "From timber-mbox-export %a %b %d %H:%M:%S %Y", &tm);

	    separator = dupstr(fromline);
	}
	write_wrapped(1, separator, strlen(separator));
	write_wrapped(1, "\n", 1);
	write_mbox(1, message, msglen);
	sfree(location);
	sfree(message);
	sfree(separator);
    }
}
