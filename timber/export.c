/*
 * export.c: export a message from Timber in its transport form.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "timber.h"

void export_message(char *ego)
{
    char *location, *message;
    int msglen;

    location = message_location(ego);
    if (!location)
	error(err_nosuchmsg);
    else {
	message = store_retrieve(location, &msglen);
	if (!message)
	    fatal(err_internal, "mail mentioned in db is not in mail store");
	write_wrapped(1, message, msglen);
	sfree(location);
    }
}
