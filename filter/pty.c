/*
 * filter/pty.c - pseudo-terminal handling
 */

#define _XOPEN_SOURCE
#include <features.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "filter.h"

/*
 * Allocate a pty. Returns a file handle to the master end of the
 * pty, and stores the full pathname to the slave end into `name'.
 * `name' is a buffer of size at least FILENAME_MAX.
 * 
 * Does not return on error.
 */
int pty_get(char *name)
{
    int fd;

    fd = open("/dev/ptmx", O_RDWR);

    if (fd < 0) {
	perror("/dev/ptmx: open");
	exit(1);
    }

    if (grantpt(fd) < 0) {
	perror("grantpt");
	exit(1);
    }
    
    if (unlockpt(fd) < 0) {
	perror("unlockpt");
	exit(1);
    }

    name[FILENAME_MAX-1] = '\0';
    strncpy(name, ptsname(fd), FILENAME_MAX-1);

    return fd;
}
