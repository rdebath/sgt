#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <fcntl.h>
#include <unistd.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides open() and friends to pay attention to an environment
 * variable FILEREPL which consists of (from,to) pairs of file
 * names, and is used to map each `from' name to the `to' name
 * before passing them on to the real open().
 *
 * compilation (linux):
 *   gcc -fpic -c filerepl.c && ld -shared -o filerepl.so filerepl.o -ldl
 * use:
 *   export LD_PRELOAD=/full/path/name/to/filerepl.so
 *   export FILEREPL=/old/file1:/new/file1:/old/file2:/new/file2
 */

static int munge(const char *n, char *buf) {
    char *e = getenv("FILEREPL");

    if (!e)
	return 0;

    while (*e) {
	char *e1, *e2;
	int len1, len2;

	e1 = e;
	while (*e1 && *e1 != ':') e1++;
	len1 = e1 - e;
	if (*e1) e1++;
	e2 = e1;
	while (*e2 && *e2 != ':') e2++;
	len2 = e2 - e1;
	if (len2 > FILENAME_MAX-1)
	    len2 = FILENAME_MAX-1;

	if (len1 && len2 && strlen(n) == len1 && !strncmp(n, e, len1)) {
	    strncpy(buf, e1, len2);
	    buf[len2] = '\0';
	    return 1;
	}

	e = e2;
	if (*e) e++;
    }

    return 0;
}

#define dofunc(rettype,sym,proto,proto2) rettype sym proto { \
    char buf[FILENAME_MAX]; \
    rettype (*real) proto; \
    real = dlsym(RTLD_NEXT, #sym); \
    if (munge(fname, buf)) fname = buf; \
    return real proto2; \
}

dofunc(int,open,(const char *fname,int flags,mode_t mode),(fname,flags,mode))
dofunc(int,open64,(const char *fname,int flags,mode_t mode),(fname,flags,mode))
dofunc(int,__open,(const char *fname,int flags,mode_t mode),(fname,flags,mode))
dofunc(int,__open64,(const char *fname,int flags,mode_t mode),(fname,flags,mode))
dofunc(int,stat,(const char *fname,struct stat *sbuf),(fname,sbuf))
dofunc(int,lstat,(const char *fname,struct stat *sbuf),(fname,sbuf))
