/*
 * Implementation of quoted-printable for Timber.
 */

#include "timber.h"

#define ishex(c) ( ((c)>='0' && (c)<='9') || \
		   ((c)>='A' && (c)<='F') || \
		   ((c)>='a' && (c)<='f') )
#define hexval(c) ( (c) >= 'a' ? (c) - 'a' + 10 : \
		    (c) >= 'A' ? (c) - 'A' + 10 : \
		    (c) - '0' )

int qp_decode(const char *input, int length, char *output, int rfc2047)
{
    char *orig_output = output;

    while (length > 0) {
	if (length >= 3 && input[0] == '=' &&
	    ishex(input[1]) && ishex(input[2])) {
	    *output++ = 16 * hexval(input[1]) + hexval(input[2]);
	    input += 3, length -= 3;
	} else if (rfc2047 && *input == '_') {
	    /*
	     * RFC2047's slightly different `Q' encoding is
	     * identical to QP except that `_' represents a space.
	     */
	    *output++ = ' ';
	    input++, length--;
	} else {
	    *output++ = *input;
	    input++, length--;
	}
    }

    return output - orig_output;
}

#ifdef TESTMODE

#include <stdio.h>
#include <assert.h>

int main(void)
{
    char *teststrings[][2] = {
	{ "this=20is=20a=20test", "this is a test" },
	{ "=A32.78 is twice =A31.39", "\xA3""2.78 is twice \xA3""1.39" },
    };
    int i, dlen;
    char buf[512];

    for (i = 0; i < lenof(teststrings); i++) {

	dlen = qp_decode(teststrings[i][0], strlen(teststrings[i][0]), buf);
	assert(dlen <= sizeof(buf));
	assert(dlen == strlen(teststrings[i][1]));
	assert(memcmp(teststrings[i][1], buf, dlen) == 0);

	buf[dlen] = '\0';
	printf("%s\n%s\n", teststrings[i][0], buf);
    }

    return 0;
}

#endif
