/*
 * main.c: command line parsing and top level
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "caltrap.h"

char *dbpath;

int main(int argc, char **argv) {
    int nogo;
    int errs;
    enum { NONE, INIT, ADD, LIST, CRON, DUMP, LOAD } command;
    char *args[4];
    int nargs = 0;
    char *homedir;

    /*
     * Set up initial (default) parameters.
     */
    nogo = errs = FALSE;

    homedir = getenv("HOME");
    if (homedir == NULL) homedir = "/";
    dbpath = smalloc(strlen(homedir) + 20);
    sprintf(dbpath, "%s%s.caltrapdb", homedir,
	    homedir[strlen(homedir)-1] == '/' ? "" : "/");

    command = NONE;

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
			} else if (!strcmp(opt, "-init")) {
			    command = INIT;
			} else if (!strcmp(opt, "-dump")) {
			    command = DUMP;
			} else if (!strcmp(opt, "-load")) {
			    command = LOAD;
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
		  case 'a':
		  case 'l':
		  case 'C':
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
		      case 'a':
			command = ADD;
			break;
		      case 'l':
			command = LIST;
			break;
		      case 'C':
			command = CRON;
			break;
		    }
		    break;
		    /*
		     * FIXME. Put cases here for single-char
		     * options that require parameters. An example
		     * is shown, commented out.
		     */
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
			sfree(dbpath);
			dbpath = smalloc(1+strlen(p));
			strcpy(dbpath, p);
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
	    if (nargs < lenof(args))
		args[nargs] = p;
	    nargs++;
	}
    }

    if (errs)
	exit(EXIT_FAILURE);
    if (nogo)
	exit(EXIT_SUCCESS);

    /*
     * Do the work.
     */
    switch (command) {
      case NONE:
	usage();
	break;
      case INIT:
	db_init();
	break;
      case ADD:
	caltrap_add(nargs, args, lenof(args));
	break;
      case LIST:
	caltrap_list(nargs, args, lenof(args));
	break;
      case CRON:
	caltrap_cron(nargs, args, lenof(args));
	break;
      case DUMP:
	caltrap_dump(nargs, args, lenof(args));
	break;
      case LOAD:
	caltrap_load(nargs, args, lenof(args));
	break;
    }

    db_close();

    return 0;
}
