/*
 * help.c: usage instructions
 */

#include <stdio.h>
#include "caltrap.h"

static char *helptext[] = {
    "usage: caltrap -a [<options>] <date> [<time>]",
    "                 [<date> [<time>]]   add entry (enter text on stdin)",
    "       caltrap -l <date> [<date>]    list entries (for a date or range)",
    "       caltrap -C <time> <cmd>       list entries in the next <time>",
    "                                         and pipe them into <cmd>",
    "       caltrap --info <id>           display details of a specific entry",
    "       caltrap --edit <id>           display details of a specific entry",
    "       caltrap --delete <id>         delete an entry",
    "       caltrap --init                set up an empty database",
    "       caltrap --dump                dump database contents as text",
    "       caltrap --load [<file>]       reload a database dump",
    "Global options:",
    "       -D <dbpath>                   use a db other than ~/.caltrapdb",
    "Options to -a and --edit:",
    "       -t (HOL1|HOL2|HOL3|EVENT)     set type of calendar entry",
    "           [suggest: HOL1=weekend HOL2=public holiday HOL3=personal leave]",
    "       -R <period>[/<length>]        make this a repeating entry",
    "Options to --edit only:",
    "       -S '<date> <time>'            change start date of entry",
    "       -F '<date> <time>'            change finish date of entry",
    "       -m 'message'                  set message for entry",
    "Options to -l:",
    "       -v                            show hidden entries and entry IDs",
    NULL
};

static char *usagetext[] = {
    "type `caltrap --help' for help",
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
