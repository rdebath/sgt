/*
 * main.c: command line parsing and top level
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "timber.h"
#include "charset.h"

char *dirpath;
int nosync = FALSE;

void run_command(int argc, char **argv)
{
    int j;

    static const struct {
	const char *name;
	void (*fn) (char *);
    } simple[] = {
	{ "import-mbox", import_mbox_folder },
	{ "export", export_message },
	{ "export-mbox", export_as_mbox }
    };

    for (j = 0; j < lenof(simple); ++j) {
	int i;
	if (strcmp (argv[0], simple[j].name)) continue;
	for (i = 1; i < argc; ++i) {
	    simple[j].fn(argv[i]);
	}
	return;
    }

    assert(argc > 0);

    if (!strcmp(argv[0], "init")) {
	int ret;
	ret = mkdir(dirpath, 0700);
	if (ret < 0)
	    fatal(err_perror, dirpath, "mkdir");
	sql_init_all();
	store_init();
    }

    if (!strcmp(argv[0], "display") ||
	!strcmp(argv[0], "display-full")) {
	int i;
	int charset = CS_ASCII;
	i = 1;
	if (i < argc)
	    charset = charset_from_localenc(argv[i++]);
	for (; i < argc; i++)
	    display_message(argv[i], charset, DISPLAY_ANSI,
			    !strcmp(argv[0], "display-full"));
    }

    if (!strcmp(argv[0], "send")) {
	int charset = CS_ASCII;
	if (argc > 1)
	    charset = charset_from_localenc(argv[1]);
	send_from_stdin(charset);
    }

    if (!strcmp (argv[0], "contact")) {
	int i;
	for (i = 2; i < argc; ++i) ab_display_attr (argv[i], argv[1]);
    }

    if (!strcmp (argv[0], "set-contact")) {
	if (2 < argc) {
	    ab_change_attr (argv[2], argv[1], (3 < argc) ? argv[3] : NULL);
	}
    }
}

int main(int argc, char **argv) {
    int nogo;
    int errs;
    int parsing_opts;
    char **arguments = NULL;
    int narguments = 0, argsize = 0;
    char *homedir;

    /*
     * Set up initial (default) parameters.
     */
    nogo = errs = FALSE;

    homedir = getenv("HOME");
    if (homedir == NULL) homedir = "/";
    dirpath = snewn(strlen(homedir) + 20, char);
    sprintf(dirpath, "%s%s.timber", homedir,
            homedir[strlen(homedir)-1] == '/' ? "" : "/");

    /*
     * Parse command line arguments.
     */
    while (--argc) {
	char *p = *++argv;
	if (*p == '-' && parsing_opts) {
	    /*
	     * An option.
	     */
	    while (p && *++p) {
		char c = *p;
		switch (c) {
		  case '-':
		    /*
		     * Long option.
		     */
		    {
			char *opt, *val;
			opt = p++;     /* opt will have _one_ leading - */
			while (*p && *p != '=')
			    p++;	       /* find end of option */
			if (*p == '=') {
			    *p++ = '\0';
			    val = p;
			} else
			    val = NULL;
			if (!strcmp(opt, "-help")) {
			    help();
			    nogo = TRUE;
			} else if (!strcmp(opt, "-version")) {
			    showversion();
			    nogo = TRUE;
			} else if (!strcmp(opt, "-nosync")) {
			    nosync = TRUE;
			} else if (!strcmp(opt, "-licence") ||
				   !strcmp(opt, "-license")) {
			    licence();
			    nogo = TRUE;
			}
			/*
			 * A sample option requiring an argument:
			 * 
			 * else if (!strcmp(opt, "-output")) {
			 *     if (!val)
			 *         errs = TRUE, error(err_optnoarg, opt);
			 *     else
			 *         ofile = val;
			 * }
			 */
			else {
			    errs = TRUE, error(err_nosuchopt, opt);
			}
		    }
		    p = NULL;
		    break;
		  case 'h':
		  case 'V':
		  case 'L':
		    /*
		     * Option requiring no parameter.
		     */
		    switch (c) {
		      case 'h':
			help();
			nogo = TRUE;
			break;
		      case 'V':
			showversion();
			nogo = TRUE;
			break;
		      case 'L':
			licence();
			nogo = TRUE;
			break;
		    }
		    break;
		  case 'D':
		    /*
		     * Option requiring parameter.
		     */
		    p++;
		    if (!*p && argc > 1)
			--argc, p = *++argv;
		    else if (!*p) {
			char opt[2];
			opt[0] = c;
			opt[1] = '\0';
			errs = TRUE, error(err_optnoarg, opt);
		    }
		    /*
		     * Now c is the option and p is the parameter.
		     */
		    switch (c) {
                      case 'D':
                        sfree(dirpath);
                        dirpath = dupstr(p);
                        break;
		    }
		    p = NULL;	       /* prevent continued processing */
		    break;
		  default:
		    /*
		     * Unrecognised option.
		     */
		    {
			char opt[2];
			opt[0] = c;
			opt[1] = '\0';
			errs = TRUE, error(err_nosuchopt, opt);
		    }
		}
	    }
	} else {
	    /*
	     * A non-option argument.
	     */
	    if (narguments >= argsize) {
		argsize = narguments + 32;
		arguments = sresize(arguments, argsize, char *);
	    }
	    arguments[narguments++] = p;
	    parsing_opts = FALSE;
	}
    }

    if (errs)
	exit(EXIT_FAILURE);
    if (nogo)
	exit(EXIT_SUCCESS);

    /*
     * Do the work.
     */
    if (narguments == 0)
	usage();
    else
	run_command(narguments, arguments);

    sql_close_all();

    return 0;
}
