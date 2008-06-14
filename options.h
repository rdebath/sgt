/*
 * options.h: simple option-parsing framework.
 */

#ifndef UMLWRAP_OPTIONS_H
#define UMLWRAP_OPTIONS_H

/*
 * Function type provided to parse_opts() to process an option.
 * 
 * Arguments are:
 *  - ctx is the same context value passed to parse_opts().
 *  - optchr is the short-option character, or '\0' if this isn't
 *    a short option.
 *  - longopt is the long-option name string, or NULL if this
 *    isn't a long option. (If optchr=='\0' and longopt==NULL,
 *    this is a non-option argument word.)
 *  - value is the value that goes with the option, if one is even
 *    potentially available.
 *
 * Return value should be:
 *  - 1 for successful processing of a valueless option.
 *    (parse_opts will throw an error if a value was nonetheless
 *    unambiguously provided.)
 *  - 2 for successful processing of an option with a value. (Must
 *    never be returned if value was NULL.)
 *  - -1 for an unrecognised option
 *  - -2 for an option which should have had a value but there
 *    wasn't one (i.e. value==NULL).
 */
typedef int (*option_fn_t)(void *ctx, char optchr, char *longopt, char *value);

/*
 * Actual option parsing function. You give it the program name,
 * and the option part of argc/argv; it will break the arguments
 * up into options and pass them to the supplied function,
 * throwing errors to stderr where appropriate. Usual usage,
 * therefore, would be something like
 * 
 *   parse_opts(argv[0], argc-1, argv+1, my_option_fn, my_option_ctx);
 * 
 * Expects to be able to modify the argument strings, though not
 * the program name.
 * 
 * Return value is 0 for a fatal error (the option list was
 * malformed enough that the program should not attempt to take
 * any real action based on the result), or 1 for success.
 */
int parse_opts(const char *pname, int nargs, char **args,
	       option_fn_t option, void *optctx);

#endif /* UMLWRAP_OPTIONS_H */
