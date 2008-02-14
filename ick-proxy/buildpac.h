/*
 * buildpac.h: header for buildpac.c.
 */

#include "icklang.h"

/*
 * Construct a proxyconfig.pac, and return a pointer to a
 * dynamically allocated string containing it. Inputs are:
 * 
 *  - userpac contains the user's original proxyconfig.pac
 * 
 *  - scr contains an Ick URL-rewriting script
 * 
 *  - port is the port number on which ick-proxy will be
 *    listening.
 */
char *build_pac(char *userpac, ickscript *scr, int port);
