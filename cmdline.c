/* ----------------------------------------------------------------------
 * Generic command-line parsing functions.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "cmdline.h"

int parsestr(char *string, void *vret) {
    char **ret = (char **)vret;
    *ret = string;
    return 1;
}

int parseint(char *string, void *vret) {
    int *ret = (int *)vret;
    int i;
    i = strspn(string, "0123456789");
    if (i > 0) {
	*ret = atoi(string);
	string += i;
    } else
	return 0;
    if (*string)
	return 0;
    return 1;
}

int parsesize(char *string, void *vret) {
    struct Size *ret = (struct Size *)vret;
    int i;
    i = strspn(string, "0123456789");
    if (i > 0) {
	ret->w = atoi(string);
	string += i;
    } else
	return 0;
    if (*string++ != 'x')
	return 0;
    i = strspn(string, "0123456789");
    if (i > 0) {
	ret->h = atoi(string);
	string += i;
    } else
	return 0;
    if (*string)
	return 0;
    return 1;
}

int parseflt(char *string, void *vret) {
    double *d = (double *)vret;
    char *endp;
    *d = strtod(string, &endp);
    if (endp && *endp) {
	return 0;
    } else
	return 1;
}

int parsebool(char *string, void *vret) {
    int *d = (int *)vret;
    if (!strcmp(string, "yes") ||
        !strcmp(string, "true") ||
        !strcmp(string, "verily")) {
        *d = 1;
        return 1;
    } else if (!strcmp(string, "no") ||
               !strcmp(string, "false") ||
               !strcmp(string, "nowise")) {
        *d = 0;
        return 1;
    }
    return 0;
}

/*
 * Read a colour into an RGB structure.
 */
int parsecol(char *string, void *vret) {
    struct RGB *ret = (struct RGB *)vret;
    char *q;

    ret->r = strtod(string, &q);
    string = q;
    if (!*string) {
	return 0;
    } else
	string++;
    ret->g = strtod(string, &q);
    string = q;
    if (!*string) {
	return 0;
    } else
	string++;
    ret->b = strtod(string, &q);

    return 1;
}

static void process_option(char const *programname,
			   struct Cmdline *option, char *arg)
{
    assert((arg != NULL) == (option->arghelp != NULL));

    if (arg) {
	if (!option->parse(arg, option->parse_ret)) {
	    fprintf(stderr, "%s: unable to parse %s `%s'\n", programname,
		    option->valname, arg);
	    exit(EXIT_FAILURE);
	}
	if (option->gotflag)
	    *option->gotflag = TRUE;
    } else {
	assert(option->gotflag);
	*option->gotflag = TRUE;
    }
}

void parse_cmdline(char const *programname, int argc, char **argv,
		   struct Cmdline *options, int noptions)
{
    int doing_options = TRUE;
    int i;
    struct Cmdline *argopt = NULL;

    for (i = 0; i < noptions; i++) {
	if (options[i].gotflag)
	    *options[i].gotflag = FALSE;
    }

    while (--argc > 0) {
	char *arg = *++argv;

	if (!strcmp(arg, "--")) {
	    doing_options = FALSE;
	} else if (!doing_options || arg[0] != '-') {
	    if (!argopt) {
		for (i = 0; i < noptions; i++) {
		    if (!options[i].nlongopts && !options[i].shortopt) {
			argopt = &options[i];
			break;
		    }
		}

		if (!argopt) {
		    fprintf(stderr, "%s: no argument words expected\n",
			    programname);
		    exit(EXIT_FAILURE);
		}
	    }

	    process_option(programname, argopt, arg);
	} else {
	    char c = arg[1];
	    char *val = arg+2;
	    int i, j, len, done = FALSE;
	    for (i = 0; i < noptions; i++) {
		/*
		 * Try a long option.
		 */
		if (c == '-') {
		    char *opt = options[i].longopt;
		    for (j = 0; j < options[i].nlongopts;
			 j++, opt += 1 + strlen(opt)) {
			 len = strlen(opt);
			if (!strncmp(arg, opt, len) &&
			    len == strcspn(arg, "=")) {
			    if (options[i].arghelp) {
				if (arg[len] == '=') {
				    process_option(programname, &options[i],
						   arg + len + 1);
				} else if (--argc > 0) {
				    process_option(programname, &options[i],
						   *++argv);
				} else {
				    fprintf(stderr, "%s: option `%s' requires"
					    " an argument\n", programname,
					    arg);
				    exit(EXIT_FAILURE);
				}
			    } else {
				process_option(programname, &options[i], NULL);
			    }
			    done = TRUE;
			}
		    }
		} else if (c == options[i].shortopt) {
		    if (options[i].arghelp) {
			if (*val) {
			    process_option(programname, &options[i], val);
			} else if (--argc > 0) {
			    process_option(programname, &options[i], *++argv);
			} else {
			    fprintf(stderr, "%s: option `%s' requires an"
				    " argument\n", programname, arg);
			    exit(EXIT_FAILURE);
			}
		    } else {
			process_option(programname, &options[i], NULL);
		    }
		    done = TRUE;
		}
	    }
	    if (!done && c == '-') {
		fprintf(stderr, "%s: unknown option `%.*s'\n",
			programname, (int)strcspn(arg, "="), arg);
		exit(EXIT_FAILURE);
	    }
	}
    }
}

void usage_message(char const *usageline,
		   struct Cmdline *options, int noptions,
		   char **extratext, int nextra)
{
    int i, maxoptlen = 0;
    char *prefix;

    printf("usage: %s\n", usageline);

    /*
     * Work out the alignment for the help display.
     */
    for (i = 0; i < noptions; i++) {
	int optlen = 0;

	if (options[i].shortopt)
	    optlen += 2;	       /* "-X" */

	if (options[i].nlongopts) {
	    if (optlen > 0)
		optlen += 2;	       /* ", " */
	    optlen += strlen(options[i].longopt);
	}

	if (options[i].arghelp) {
	    if (optlen > 0)
		optlen++;	       /* " " */
	    optlen += strlen(options[i].arghelp);
	}

	if (maxoptlen < optlen)
	    maxoptlen = optlen;
    }

    /*
     * Display the option help.
     */
    prefix = "where: ";
    for (i = 0; i < noptions; i++) {
	int optlen = 0;

	printf("%s", prefix);

	if (options[i].shortopt)
	    optlen += printf("-%c", options[i].shortopt);

	if (options[i].nlongopts) {
	    if (optlen > 0)
		optlen += printf(", ");
	    optlen += printf("%s", options[i].longopt);
	}

	if (options[i].arghelp) {
	    if (optlen > 0)
		optlen += printf(" ");
	    optlen += printf("%s", options[i].arghelp);
	}

	printf("%*s  %s\n", maxoptlen-optlen, "", options[i].deschelp);

	prefix = "       ";
    }

    for (i = 0; i < nextra; i++) {
	printf("%s\n", extratext[i]);
    }

    exit(EXIT_FAILURE);
}
