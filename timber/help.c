/*
 * help.c: usage instructions
 */

#include <stdio.h>
#include "timber.h"

static char *helptext[] = {
    "usage:     timber <options> <command> [<arguments>]",
    "options:   -D <dirpath>          specify path to Timber directory",
    "           --nosync              unsafe but fast database",
    "                                 (recommended for large imports)",
    "commands:  init                  initialise Timber storage directory",
    "           import-mbox <file>    import an mbox full of messages",
    "           export <egos>         export message(s) in RFC822 form",
    "           display <charset> <egos> display message(s) legibly",
    "           display-full <charset> <egos> display with full headers etc",
    NULL
};

static char *usagetext[] = {
    "usage: timber <options> <command> [<arguments>]",
    "       timber --help for more detailed help",
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
    printf("Timber, %s\n", version);
}
