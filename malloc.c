/*
 * malloc.c: safe wrappers around malloc, realloc, free, strdup
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "misc.h"

#ifdef LOGALLOC
#define LOGPARAMS char *file, int line,
static FILE *logallocfp = NULL;
static int logline = 2;		       /* off by 1: `null pointer is' */
static void loginc(void) { }
static void logallocinit(void) {
    if (!logallocfp) {
	logallocfp = fopen("malloc.log", "w");
	if (!logallocfp) {
	    fprintf(stderr, "panic: unable to open malloc.log\n");
	    exit(10);
	}
	setvbuf (logallocfp, NULL, _IOLBF, BUFSIZ);
	fprintf(logallocfp, "null pointer is %p\n", NULL);
    }
}
static void logprintf(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logallocfp, fmt, ap);
    va_end(ap);
}
#define LOGPRINT(x) ( logallocinit(), logprintf x )
#define LOGINC do { loginc(); logline++; } while (0)
#else
#define LOGPARAMS
#define LOGPRINT(x)
#define LOGINC ((void)0)
#endif

/*
 * smalloc should guarantee to return a useful pointer - Halibut
 * can do nothing except die when it's out of memory anyway.
 */
void *(smalloc)(LOGPARAMS int size) {
    void *p;
    LOGINC;
    LOGPRINT(("%s %d malloc(%ld)",
	      file, line, (long)size));
    p = malloc(size);
    if (!p) {
	platform_fatal_error("out of memory");
	exit(1);
    }
    LOGPRINT((" returns %p\n", p));
    return p;
}

/*
 * sfree should guaranteeably deal gracefully with freeing NULL
 */
void (sfree)(LOGPARAMS void *p) {
    if (p) {
	LOGINC;
	LOGPRINT(("%s %d free(%p)\n",
		  file, line, p));
	free(p);
    }
}

/*
 * srealloc should guaranteeably be able to realloc NULL
 */
void *(srealloc)(LOGPARAMS void *p, int size) {
    void *q;
    if (p) {
	LOGINC;
	LOGPRINT(("%s %d realloc(%p,%ld)",
		  file, line, p, (long)size));
	q = realloc(p, size);
	LOGPRINT((" returns %p\n", q));
    } else {
	LOGINC;
	LOGPRINT(("%s %d malloc(%ld)",
		  file, line, (long)size));
	q = malloc(size);
	LOGPRINT((" returns %p\n", q));
    }
    if (size && !q) {
	platform_fatal_error("out of memory");
	exit(1);
    }
    return q;
}

/*
 * dupstr is like strdup, but with the never-return-NULL property
 * of smalloc (and also reliably defined in all environments :-)
 */
char *dupstr(char const *s) {
    char *r = smalloc(1+strlen(s));
    strcpy(r,s);
    return r;
}

/*
 * dupfmt is a cut-down printf-alike, but does its own allocation
 * of the returned string.
 */
char *dupfmt(const char *fmt, ...)
{
    int pass;
    int totallen;
    char *ret = NULL, *rp = NULL;
    char datebuf[80];
    va_list ap;

    datebuf[0] = '\0';
    totallen = 0;

    for (pass = 0; pass < 2; pass++) {
	const char *p = fmt;

	va_start(ap, fmt);

	while (*p) {
	    const char *data = NULL;
	    int datalen = 0, stuffcr = 0;

	    if (*p == '%') {
		p++;
		if (*p == 'D') {
		    if (!datebuf[0]) {
			time_t t;
			struct tm tm;

			t = time(NULL);
			tm = *gmtime(&t);
			strftime(datebuf, lenof(datebuf),
				 "%a, %d %b %Y %H:%M:%S GMT", &tm);
		    }
		    data = datebuf;
		    datalen = strlen(data);
		} else if (*p == 'd') {
		    int i = va_arg(ap, int);
		    sprintf(datebuf, "%d", i);
		    data = datebuf;
		    datalen = strlen(data);
		} else if (*p == 's') {
		    data = va_arg(ap, const char *);
		    datalen = strlen(data);
		} else if (assert(*p == 'S'), 1) {
		    stuffcr = va_arg(ap, int);
		    data = va_arg(ap, const char *);
		    datalen = strlen(data);
		}
		p++;
	    } else {
		data = p;
		while (*p && *p != '%') p++;
		datalen = p - data;
	    }

	    if (pass == 0) {
		totallen += datalen;
		if (stuffcr) {
		    while (datalen > 0) {
			if (*data == '\n')
			    totallen++;
			data++, datalen--;
		    }
		}
	    } else {
		while (datalen > 0) {
		    if (stuffcr && *data == '\n')
			*rp++ = '\r';
		    *rp++ = *data++;
		    datalen--;
		}
	    }
	}

	va_end(ap);

	if (pass == 0) {
	    rp = ret = snewn(totallen+1, char);
	} else {
	    assert(rp - ret == totallen);
	    *rp = '\0';
	}
    }

    return ret;
}

