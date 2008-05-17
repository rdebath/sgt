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
    int verbose;
    struct entry e;
    enum { NONE, INIT, ADD, LIST, CRON, DUMP, LOAD, INFO, EDIT, DEL } command;
    char *args[4];
    int nargs = 0;
    char *homedir;

    /*
     * Set up initial (default) parameters.
     */
    nogo = errs = verbose = FALSE;
    e.type = INVALID_TYPE;
    e.length = e.period = INVALID_DURATION;
    e.sd = e.ed = INVALID_DATE;
    e.st = e.et = INVALID_TIME;
    e.description = NULL;

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
			} else if (!strcmp(opt, "-info")) {
			    command = INFO;
			} else if (!strcmp(opt, "-edit")) {
			    command = EDIT;
			} else if (!strcmp(opt, "-delete")) {
			    command = DEL;
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
		  case 'v':
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
		      case 'v':
			verbose = TRUE;
			break;
		    }
		    break;
		    /*
		     * Single-char options that require parameters.
		     */
		  case 'D':
		  case 't':
		  case 'R':
		  case 'S':
		  case 'E':
		  case 'F':
		  case 'm':
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
		      case 't':
			e.type = name_to_type(p);
			if (e.type == INVALID_TYPE)
			    fatal(err_eventtype, p);
			break;
		      case 'R':
			{
			    char *q = p + strcspn(p, "/");
			    if (*q)
				*q++ = '\0';
			    else
				q = NULL;
			    e.period = parse_duration(p);
			    if (e.period == INVALID_DURATION)
				fatal(err_duration, p);
			    if (q) {
				e.length = parse_duration(q);
				if (e.length == INVALID_DURATION)
				    fatal(err_duration, q);
			    } else
				e.length = 0;
			}
			break;
		      case 'S':
			if (!parse_datetime(p, &e.sd, &e.st))
			    fatal(err_datetime, p);
			break;
		      case 'E':
		      case 'F':
			if (!parse_datetime(p, &e.ed, &e.et))
			    fatal(err_datetime, p);
			break;
		      case 'm':
			e.description = p;
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
	caltrap_add(nargs, args, lenof(args), &e);
	break;
      case LIST:
	caltrap_list(nargs, args, lenof(args), verbose);
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
      case INFO:
	caltrap_info(nargs, args, lenof(args));
	break;
      case EDIT:
	caltrap_edit(nargs, args, lenof(args), &e);
	break;
      case DEL:
	caltrap_del(nargs, args, lenof(args));
	break;
    }

    db_close();

    return 0;
}
