/*
 * options.c: implementation of options.h.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "options.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifdef TEST_OPTIONS
/*
 * Nasty hack for the test rig: redirect stderr to point to the
 * temporary test file.
 */
#undef stderr
#define stderr ((FILE *)optctx)
#endif

int parse_opts(const char *pname, int nargs, char **args,
	       option_fn_t option, void *optctx)
{
    int doing_opts = TRUE;

    while (nargs > 0) {
	char *p = *(nargs--, args++);
	int ret;

	if (doing_opts && *p == '-') {
	    char *value;

	    /*
	     * Process an option.
	     */
	    if (p[1] == '-') {
		int valcertain;

		p += 2;
		if (!*p) {
		    doing_opts = FALSE;
		    continue;
		}

		value = strchr(p, '=');
		if (value) {
		    *value++ = '\0';
		    /*
		     * This was certainly intended as a value for
		     * that option.
		     */
		    valcertain = TRUE;
		} else if (nargs > 0) {
		    value = *args;
		    /*
		     * But this might perfectly well be an
		     * unrelated next argument.
		     */
		    valcertain = FALSE;
		} else {
		    value = NULL;
		    valcertain = TRUE;
		}

		ret = option(optctx, '\0', p, value);

		if (ret == 1 && value && valcertain) {
		    fprintf(stderr, "%s: ignoring unexpected value after"
			    " option '--%s'\n", pname, p);
		} else if (ret == -2) {
		    fprintf(stderr, "%s: expected a value after option"
			    " '--%s'\n", pname, p);
		    return 0;
		} else if (ret == -1) {
		    fprintf(stderr, "%s: unrecognised option '--%s'\n",
			    pname, p);
		    return 0;
		}

		if (ret == 2 && !valcertain)
		    nargs--, args++;    /* eat the separate option argument */
	    } else {
		int valtrailing;

		p++;
		while (*p) {
		    char c = *p++;

		    if (*p) {
			value = p;
			valtrailing = TRUE;
		    } else if (nargs > 0) {
			value = *args;
			valtrailing = FALSE;
		    } else {
			value = NULL;
			valtrailing = TRUE;
		    }

		    ret = option(optctx, c, NULL, value);

		    if (ret == -2) {
			fprintf(stderr, "%s: expected a value after option"
				" '-%c'\n", pname, c);
			return 0;
		    } else if (ret == -1) {
			fprintf(stderr, "%s: unrecognised option '-%c'\n",
				pname, c);
			return 0;
		    }

		    if (ret == 2) {
			if (valtrailing)
			    p += strlen(p);   /* advance to zero terminator */
			else
			    (nargs--, args++);/* eat the separate option arg */
		    }
		}
	    }
	} else {
	    ret = option(optctx, '\0', NULL, p);
	    if (ret < 0) {
		fprintf(stderr, "%s: unexpected non-option argument '%s'\n",
			pname, p);
		return 0;
	    }
	    doing_opts = FALSE;
	}
    }

    return 1;
}

#ifdef TEST_OPTIONS

/*
 * Standalone unit test.

gcc -g -O0 -DTEST_OPTIONS -o options options.c && ./options

 */

static int narguments;

static int option(void *ctx, char optchr, char *longopt, char *value)
{
    FILE *fp = (FILE *)ctx;

    if ((longopt && !strcmp(longopt, "test1")) || optchr == 't') {
	if (!value) return -2;
	fprintf(fp, "test1: '%s'\n", value);
	return 2;
    }
    if ((longopt && !strcmp(longopt, "test2")) || optchr == 'T') {
	fprintf(fp, "test2\n", value);
	return 1;
    }
    if (!optchr && !longopt) {
	if (narguments >= 2)
	    return -1;
	fprintf(fp, "arg: '%s'\n", value);
	narguments++;
	return 1;
    }
    return -1;
}

#define MAXARGS 16
#define MAXSPACE 4096

int makeargs(char *output, char **outptrs, const char *input, int insize)
{
    int i;
    char *p;

    assert(insize < MAXSPACE);
    memcpy(output, input, insize);     /* render it writable */

    for (i = 0, p = output; *p; p += 1+strlen(p)) {
	assert(i < MAXARGS);
	outptrs[i++] = p;
    }

    return i;
}

int passes = 0, fails = 0;

int test(const char *argdata, int argsize, const char *outdata)
{
    FILE *fp = tmpfile();
    int nargs, ret;
    char argstr[MAXSPACE], *argptrs[MAXARGS];
    char outreal[MAXSPACE];

    narguments = 0;
    parse_opts("testprog", makeargs(argstr, argptrs, argdata, argsize),
	       argptrs, option, fp);

    rewind(fp);

    ret = fread(outreal, 1, MAXSPACE, fp);
    assert(ret >= 0);
    if (ret != strlen(outdata) ||
	memcmp(outdata, outreal, ret) != 0) {
	const char *p;

	fails++;
	printf("Failed test for arguments:\n");
	for (p = argdata; *p; p += 1+strlen(p))
	    printf("  '%s'\n", p);
	printf("Expected output data:\n");
	fputs(outdata, stdout);
	printf("Actual output data:\n");
	fwrite(outreal, 1, ret, stdout);
    } else
	passes++;

    fclose(fp);
}

#define TEST(argdata, outdata) test(argdata, sizeof(argdata), outdata)

int main(void)
{
    /*
     * Basic tests.
     */
    TEST("", "");
    TEST("-tflibble\0", "test1: 'flibble'\n");
    TEST("-t\0flibble\0", "test1: 'flibble'\n");
    TEST("-T\0", "test2\n");
    TEST("--test1=flibble\0", "test1: 'flibble'\n");
    TEST("--test1\0flibble\0", "test1: 'flibble'\n");
    TEST("--test2\0", "test2\n");
    TEST("wibble\0", "arg: 'wibble'\n");

    /*
     * Multiple short options jammed together.
     */
    TEST("-TT\0", "test2\ntest2\n");
    TEST("-TTtflibble\0", "test2\ntest2\ntest1: 'flibble'\n");
    TEST("-TTt\0flibble\0", "test2\ntest2\ntest1: 'flibble'\n");

    /*
     * Error tests for missing values.
     */
    TEST("-t\0", "testprog: expected a value after option '-t'\n");
    TEST("--test1\0", "testprog: expected a value after option '--test1'\n");

    /*
     * Error tests for unexpected values and arguments.
     */
    TEST("--test2=wobble\0", "test2\ntestprog: ignoring unexpected value"
	 " after option '--test2'\n");
    TEST("--test2=wobble\0continue", "test2\ntestprog: ignoring unexpected"
	 " value after option '--test2'\narg: 'continue'\n");
    TEST("wibbly\0wobbly\0woo\0", "arg: 'wibbly'\narg: 'wobbly'\ntestprog:"
	 " unexpected non-option argument 'woo'\n");

    /*
     * Error tests for unrecognised options.
     */
    TEST("--weeble", "testprog: unrecognised option '--weeble'\n");
    TEST("-w", "testprog: unrecognised option '-w'\n");
    TEST("-Tw", "test2\ntestprog: unrecognised option '-w'\n");
    TEST("-wT", "testprog: unrecognised option '-w'\n");

    printf("Passed %d, failed %d\n", passes, fails);

    return fails != 0;
}

#endif
