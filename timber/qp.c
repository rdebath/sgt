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
	} else if (length >= 2 && input[0] == '=' && input[1] == '\n') {
	    input += 2, length -= 2;
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

/*
 * Timber does not use quoted-printable as an encoding for message
 * bodies. Therefore, the only reason we need a QP encoder at all
 * is for RFC2047 headers.
 * 
 * This function has the additional feature of just returning the
 * exact length of an encoded string if `output' is NULL.
 */
int qp_rfc2047_encode(const char *input, int length, char *output)
{
    /*
     * The RFC2047 dialect of quoted-printable is as follows:
     * 
     *  - Letters, numbers and !*+-/ are encoded literally.
     *  - Space is encoded as underscore.
     *  - Everything else is encoded as =xx.
     */
    int outlen = 0;

    while (length > 0) {
	if ((*input >= 'A' && *input <= 'Z') ||
	    (*input >= 'a' && *input <= 'z') ||
	    (*input >= '0' && *input <= '9') ||
	    *input == '!' || *input == '*' || *input == '+' ||
	    *input == '-' || *input == '/') {
	    outlen++;
	    if (output)
		*output++ = *input;
	} else if (*input == ' ') {
	    outlen++;
	    if (output)
		*output++ = '_';
	} else {
	    outlen += 3;
	    if (output) {
		static const char hex[16] = "0123456789ABCDEF";
		int val = (unsigned char)*input;
		*output++ = '=';
		*output++ = hex[(val >> 4) & 0xF];
		*output++ = hex[val & 0xF];
	    }
	}
	input++, length--;
    }

    return outlen;
}

#ifdef TESTMODE

#include <stdio.h>
#include <assert.h>

int main(void)
{
    char *teststrings[][2] = {
	{ "this=20is=20a=20test", "this is a test" },
	{ "=A32.78 is twice =A31.39", "\xA3""2.78 is twice \xA3""1.39" },
	{ "This breaks over =\ntwo lines", "This breaks over two lines" },
    };
    int i, dlen;
    char buf[512];

    for (i = 0; i < lenof(teststrings); i++) {

	dlen = qp_decode(teststrings[i][0], strlen(teststrings[i][0]), buf, 0);
	assert(dlen <= sizeof(buf));
	assert(dlen == strlen(teststrings[i][1]));
	assert(memcmp(teststrings[i][1], buf, dlen) == 0);

	buf[dlen] = '\0';
	printf("%s\n%s\n", teststrings[i][0], buf);
    }

    return 0;
}

#endif
