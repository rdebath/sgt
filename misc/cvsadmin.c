#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <grp.h>
#include <dlfcn.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides getgrnam() to pretend the `cvsadmin' group does not
 * exist. Good for persuading CVS that you _are_ allowed to do CVS
 * admin functions on your own private repositories!
 *
 * compilation (linux):
 *   gcc -fpic -c cvsadmin.c && ld -shared -o cvsadmin.so cvsadmin.o
 * use:
 *   LD_PRELOAD=/full/path/name/to/cvsadmin.so cvs admin <stuff>
 */

struct group *getgrnam(const char *name)
{
    if (!strcmp(name, "cvsadmin")) {
	return NULL;
    } else {
	char buf[FILENAME_MAX < 512 ? 512 : FILENAME_MAX];
	struct group *(*real_getgrnam) (const char *name);
	real_getgrnam = dlsym(RTLD_NEXT, "getgrnam");
	return real_getgrnam(name);
    }
}
