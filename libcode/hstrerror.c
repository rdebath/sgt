#include <netdb.h>

/*
 * This module contains:
 * 
 *  - an implementation of hstrerror()
 * 
 *  - a main() designed to be compiled in the presence of an
 *    hstrerror(), which automatically generates an implementation
 *    on stdout by extracting the host implementation's message
 *    text.
 * 
 * Compile:
 * 
 *  - with no predefines to get just the hstrerror()
 * 
 *  - with -DGENERATE to get just the main(), so as to generate an
 *    hstrerror()
 * 
 *  - with -DTEST to compile both, so that you can verify that the
 *    generated hstrerror() actually works.
 */

/*
 * Include the implementation in TEST mode, in normal mode, but not
 * in GENERATE mode.
 */
#if defined TEST || !defined GENERATE

const char *hstrerror(int err)
{
    switch (err) {
      case HOST_NOT_FOUND: return "Unknown host";
      case TRY_AGAIN: return "Host name lookup failure";
      case NO_RECOVERY: return "Unknown server error";
      case NO_DATA: return "No address associated with name";
      default: return "Unknown error";
    }
}

#endif

/*
 * Include main() in GENERATE or TEST mode, but not in normal mode.
 */
#if defined TEST || defined GENERATE

#include <stdio.h>

#define ERRLIST(A) \
    A(HOST_NOT_FOUND), \
    A(TRY_AGAIN), \
    A(NO_RECOVERY), \
    A(NO_DATA)
#define I(x) x
#define S(x) #x

int errors[] = { ERRLIST(I) };
char *errnames[] = { ERRLIST(S) };

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

int main(void)
{
    int i;

    printf("const char *hstrerror(int err)\n");
    printf("{\n");
    printf("    switch (err) {\n");
    for (i = 0; i < lenof(errors); i++) {
	printf("      case %s: return \"%s\";\n",
	       errnames[i], hstrerror(errors[i]));
    }
    printf("      default: return \"Unknown error\";\n");
    printf("    }\n");
    printf("}\n");

    return 0;
}

#endif
