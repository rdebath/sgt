#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <netdb.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides vfork() and replaces it by fork(). Helpful when trying
 * to strace programs that vfork!
 *
 * compilation (linux):
 *   gcc -fpic -c no-vfork.c && ld -shared -o no-vfork.so no-vfork.o
 * use:
 *   export LD_PRELOAD=/full/path/name/to/no-vfork.so
 */

pid_t vfork(void)
{
    return fork();
}

pid_t __vfork(void)
{
    return fork();
}
