#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <netdb.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides gethostbyname() and friends to pay attention to a
 * user-land file `~/.hostaliases' which consists of lines of the
 * form `alias hostname', eg `foovax foovax.mycorp.com', and is
 * used to transform hostnames before passing them on to the real
 * resolver.
 *
 * compilation (linux):
 *   gcc -fpic -c hostlib.c && ld -shared -o hostlib.so hostlib.o
 * use:
 *   export LD_PRELOAD=/full/path/name/to/hostlib.so
 *   echo foovax foovax.mycorp.com > ~/.hostaliases
 */

static const char *munge(const char *n, char *buf) {
    FILE *fp;
    char *e;
    strcpy(buf, (e = getenv("HOME")) ? e : "");
    strcat(buf, "/.hostaliases");
    fp = fopen(buf, "r");
    if (fp) {
        while (fgets(buf, 512, fp)) {
            char *c;
            buf[strcspn(buf, "\r\n")] = '\0';
            c = buf + strcspn(buf, " \t");
            if (!*c)
                continue;
            *c++ = '\0';
            c += strspn(c, " \t");
            if (!strcmp(n, buf))
                return c;
        }
    }
    return n;
}

#define dofunc(rettype,sym,proto,proto2) rettype sym proto { \
    char buf[FILENAME_MAX < 512 ? 512 : FILENAME_MAX]; \
    rettype (*real) proto; \
    real = dlsym(RTLD_NEXT, #sym); \
    n = munge(n, buf); \
    return real proto2; \
}

dofunc(struct hostent *,gethostbyname,(const char *n),(n))
dofunc(struct hostent *,gethostbyname2,(const char *n,int af),(n,af))
dofunc(int,__gethostbyname_r,
   (const char *n, struct hostent *__result_buf, char *__buf,
    size_t __buflen, struct hostent **__result,
    int *__h_errnop), (n,__result_buf,__buf,__buflen,__result,__h_errnop))
dofunc(int, gethostbyname_r,
   (const char *n, struct hostent *__result_buf, char *__buf,
    size_t __buflen, struct hostent **__result,
    int *__h_errnop), (n,__result_buf,__buf,__buflen,__result,__h_errnop))
dofunc(int,__gethostbyname2_r,
   (const char *n, int af,
    struct hostent *__result_buf, char *__buf,
    size_t __buflen, struct hostent **__result,
    int *__h_errnop), (n,af,__result_buf,__buf,__buflen,__result,__h_errnop))
dofunc(int,gethostbyname2_r,
   (const char *n, int af,
    struct hostent *__result_buf, char *__buf,
    size_t __buflen, struct hostent **__result,
    int *__h_errnop), (n,af,__result_buf,__buf,__buflen,__result,__h_errnop))
