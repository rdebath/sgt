/*
 * main.c: command line parsing and top level
 */

#include <stdio.h>
#include <stdlib.h>
#include "caltrap.h"

int main(int argc, char **argv) {
    int nogo;
    int errs;

    /*
     * Set up initial (default) parameters.
     */
    nogo = errs = FALSE;

    /*
     * Parse command line arguments.
     */
    while (--argc) {
	char *p = *++argv;
	if (*p == '-') {
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
		    /*
		     * FIXME. Put cases here for single-char
		     * options that require parameters. An example
		     * is shown, commented out.
		     */
		  /* case 'o': */
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
			/*
			 * A sample option requiring an argument:
			 *
		         * case 'o':
			 *   ofile = p;
			 *   break;
			 */
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
	}
    }

    if (errs)
	exit(EXIT_FAILURE);
    if (nogo)
	exit(EXIT_SUCCESS);

    /*
     * Do the work.
     */
    printf("FIXME: put some activity here\n");

    return 0;
}
