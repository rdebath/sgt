/*
 * Implementation of base64 for Timber.
 */

#include "timber.h"

#define isbase64(c) (    ((c) >= 'A' && (c) <= 'Z') || \
                         ((c) >= 'a' && (c) <= 'z') || \
                         ((c) >= '0' && (c) <= '9') || \
                         (c) == '+' || (c) == '/' || (c) == '=' \
                         )

static int base64_decode_atom(const char *atom, unsigned char *out)
{
    int vals[4];
    int i, v, len;
    unsigned word;
    char c;
    
    for (i = 0; i < 4; i++) {
	c = atom[i];
	if (c >= 'A' && c <= 'Z')
	    v = c - 'A';
	else if (c >= 'a' && c <= 'z')
	    v = c - 'a' + 26;
	else if (c >= '0' && c <= '9')
	    v = c - '0' + 52;
	else if (c == '+')
	    v = 62;
	else if (c == '/')
	    v = 63;
	else if (c == '=')
	    v = -1;
	else
	    return 0;		       /* invalid atom */
	vals[i] = v;
    }

    if (vals[0] == -1 || vals[1] == -1)
	return 0;
    if (vals[2] == -1 && vals[3] != -1)
	return 0;

    if (vals[3] != -1)
	len = 3;
    else if (vals[2] != -1)
	len = 2;
    else
	len = 1;

    word = ((vals[0] << 18) |
	    (vals[1] << 12) |
	    ((vals[2] & 0x3F) << 6) |
	    (vals[3] & 0x3F));
    out[0] = (word >> 16) & 0xFF;
    if (len > 1)
	out[1] = (word >> 8) & 0xFF;
    if (len > 2)
	out[2] = word & 0xFF;
    return len;
}

static void base64_encode_atom(const unsigned char *data, int n, char *out)
{
    static const char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    unsigned word;

    word = data[0] << 16;
    if (n > 1)
	word |= data[1] << 8;
    if (n > 2)
	word |= data[2];
    out[0] = base64_chars[(word >> 18) & 0x3F];
    out[1] = base64_chars[(word >> 12) & 0x3F];
    if (n > 1)
	out[2] = base64_chars[(word >> 6) & 0x3F];
    else
	out[2] = '=';
    if (n > 2)
	out[3] = base64_chars[word & 0x3F];
    else
	out[3] = '=';
}

int base64_decode_length(int input_length)
{
    return (input_length + 3) / 4 * 3;
}

int base64_encode_length(int input_length)
{
    int atoms = (input_length + 2) / 3;
    int lines = (atoms + 15) / 16;
    return 4 * atoms + lines;
}

int base64_decode(const char *input, int length, unsigned char *output)
{
    char atom[4];
    int n = 0;
    unsigned char *orig_output = output;

    while (length > 0) {
	if (isbase64(*input)) {
	    atom[n++] = *input;
	    if (n == 4) {
		output += base64_decode_atom(atom, output);
		n = 0;
	    }
	}
	input++, length--;
    }

    if (n > 0) {
	while (n < 4)
	    atom[n++] = '=';
	output += base64_decode_atom(atom, output);
    }

    return output - orig_output;
}

int base64_encode(const unsigned char *input, int length,
		  char *output, int multiline)
{
    int atoms_on_line = 0;
    char *orig_output = output;

    while (length > 0) {
	int n = (length > 3 ? 3 : length);
	base64_encode_atom(input, n, output);
	output += 4;
	if (multiline) {
	    if (++atoms_on_line >= 16) {
		*output++ = '\n';
		atoms_on_line = 0;
	    }
	}
	input += n, length -= n;
    }

    if (multiline && atoms_on_line > 0)
	*output++ = '\n';

    return output - orig_output;
}

#ifdef TESTMODE

#include <stdio.h>
#include <assert.h>

int main(void)
{
    char *teststrings[] = {
	"hello, world",
	"hello, world!",
	"hello, world!!",
	"hello, world!!!",
	"this text is a bit longer than the 48 chars per encoded line"
    };
    int i, elen, elen2, dlen, dlen2;
    char buf1[512], buf2[512];

    for (i = 0; i < lenof(teststrings); i++) {

	elen = base64_encode_length(strlen(teststrings[i]));
	assert(elen < sizeof(buf1));

	elen2 = base64_encode(teststrings[i], strlen(teststrings[i]), buf1, 1);
	assert(elen2 <= elen);

	dlen = base64_decode_length(elen2);
	assert(dlen < sizeof(buf2));

	dlen2 = base64_decode(buf1, elen2, buf2);
	assert(dlen2 <= dlen);

	assert(dlen2 == strlen(teststrings[i]));
	assert(memcmp(teststrings[i], buf2, dlen2) == 0);

	buf1[elen2] = '\0';
	buf2[dlen2] = '\0';

	printf("%s\n%s\n", buf2, buf1);
    }

    return 0;
}

#endif
