/*
 * help.c: usage instructions
 */

#include <stdio.h>
#include "caltrap.h"

static char *helptext[] = {
    "FIXME: help text goes here",
    NULL
};

static char *usagetext[] = {
    "usage: caltrap -a <date> [<time>]    add entry (enter text on stdin)",
    "       caltrap -l <date> [<date>]    list entries (for a date or range)",
    "       caltrap -C <time> <cmd>       list entries in the next <time>",
    "                                         and pipe them into <cmd>",
    "       caltrap --init                set up an empty database",
    "       caltrap --dump                dump database contents as text",
    "       caltrap --load [<file>]       reload a database dump",
    "options:",
    "       -D <dbpath>                   use a db other than ~/.caltrapdb",
    NULL
};

void help(void) {
    char **p;
    for (p = helptext; *p; p++)
	puts(*p);
}

void usage(void) {
    char **p;
    for (p = usagetext; *p; p++)
	puts(*p);
}

void showversion(void) {
    printf("Caltrap, %s\n", version);
}
