/*
 * misc.c - miscellaneous utility routines for Caltrap
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "caltrap.h"

static const char *const typenames[] = {
    "HOL1", "HOL2", "HOL3", "EVENT", "TODO"
};

int name_to_type(const char *name)
{
    int i;
    for (i = 0; i < lenof(typenames); i++)
	if (!strcmp(typenames[i], name))
	    return i;
    fatal(err_dbconsist, "invalid-type-name");
}

const char *type_to_name(int type)
{
    assert(type >= 0 && type < lenof(typenames));
    return typenames[type];
}

int get_line_width(void)
{
    struct winsize ws;
    char *cols;
    int ret;

    /*
     * First try fetching the terminal device's window size. This
     * is most likely to be accurate.
     */
    if (ioctl(0, TIOCGWINSZ, &ws) == 0)
	return ws.ws_col;

    /*
     * Failing that, try the environment variable COLUMNS.
     */
    if ((cols = getenv("COLUMNS")) != NULL &&
	(ret = atoi(cols)) > 0)
	return ret;

    /*
     * If all else fails, assume 80.
     */
    return 80;
}
