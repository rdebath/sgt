/*
 * misc.h: miscellaneous ick-proxy definitions.
 */

#ifndef ICK_PROXY_MISC_H
#define ICK_PROXY_MISC_H

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#ifdef LOGALLOC
void *smalloc(char *file, int line, int size);
void *srealloc(char *file, int line, void *p, int size);
void sfree(char *file, int line, void *p);
#define smalloc(x) smalloc(__FILE__, __LINE__, x)
#define srealloc(x, y) srealloc(__FILE__, __LINE__, x, y)
#define sfree(x) sfree(__FILE__, __LINE__, x)
#else
void *smalloc(int size);
void *srealloc(void *p, int size);
void sfree(void *p);
#endif
char *dupstr(char const *s);
char *dupfmt(const char *fmt, ...);

#define snew(type) ( (type *) smalloc (sizeof (type)) )
#define snewn(number, type) ( (type *) smalloc ((number) * sizeof (type)) )
#define sresize(array, number, type) \
	( (type *) srealloc ((void *)(array), (number) * sizeof (type)) )

void platform_fatal_error(const char *s);

#endif /* ICK_PROXY_MISC_H */
