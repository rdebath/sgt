/* ----------------------------------------------------------------------
 * Declarations for generic command-line parsing functions.
 */

#ifndef CMDLINE_H
#define CMDLINE_H

#include "misc.h"

int parsestr(char *string, void *ret);
int parseint(char *string, void *ret);
int parsesize(char *string, void *ret);
int parseflt(char *string, void *ret);
int parsebool(char *string, void *ret);
int parsecol(char *string, void *ret);

struct Cmdline {
    int nlongopts;
    char *longopt;		       /* `nlongopts' NUL-separated strings */
    char shortopt;
    char *arghelp;
    char *deschelp;
    char *valname;		       /* for use in `cannot parse' messages */
    int (*parse)(char *string, void *ret);
    void *parse_ret;
    int *gotflag;
};

void parse_cmdline(char const *programname, int argc, char **argv,
		   struct Cmdline *options, int noptions);

void usage_message(char const *usageline,
		   struct Cmdline *options, int noptions,
		   char **extratext, int nextra);

#endif /* CMDLINE_H */
