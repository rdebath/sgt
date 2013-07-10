/*
 * asciify.c - translation module for a filter which you can use to
 * wrap a program which only accepts plain ASCII input and will choke
 * on high-bit-set character codes.
 *
 * It will infer your normal character set from the locale, attempt to
 * select good ASCII approximations for common non-ASCII characters on
 * input, and substitute <U+abcd> type text for the rest, so you can
 * paste into the ASCII application without fear of confusing it with
 * weird high-half character values.
 *
 * Currently it assumes that ASCII is a subset of your normal
 * character set, or close enough: if the program is one you would
 * have been happy to run without this wrapper, that's probably true.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

#include "charset.h"

struct tstate {
    int charset;
    charset_state instate;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->charset = charset_from_locale();
    state->instate = charset_init_state;

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    if (longopt) {
	if (!strcmp(longopt, "charset")) {
	    shortopt = 'c';
        } else {
	    return OPT_UNKNOWN;
        }
    }

    if (shortopt == 'c') {
        int cset;

        if (!value)
            return OPT_MISSINGARG;

        cset = charset_from_localenc(value);
        if (cset == CS_NONE) {
            fprintf(stderr, "asciify: unrecognised character set '%s'\n",
                    value);
            exit(1);
        }

        state->charset = cset;

        return OPT_OK;
    } else {
        return OPT_UNKNOWN;
    }
}

void tstate_argument(tstate *state, char *arg)
{
    fprintf(stderr, "asciify: expected no arguments\n");
    exit(1);
}

void tstate_ready(tstate *state, double *idelay, double *odelay)
{
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags)
{
    char *ret;
    int retsize, retlen, inret;
    wchar_t midbuf[256];
    const char *inptr;

    retsize = inlen + 512;
    ret = malloc(retsize);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    if (input) {
        retlen = 0;
        inptr = data;

        while ( (inret = charset_to_unicode(&inptr, &inlen, midbuf,
                                            lenof(midbuf), state->charset,
                                            &state->instate, NULL, 0)) > 0) {
            wchar_t *midptr = midbuf;
            int i;

            for (i = 0; i < inret; i++) {
                wchar_t wc = *midptr++;
                char localbuf[10];
                const char *outptr;
                int len;

                switch (wc) {
                    /*
                     * Initially generated from UnicodeData.txt using
                     * the script

perl -ne '
@_=split/;/;
$c = hex $_[0];
@cs = map{hex}split / /,$_[5];
@acs=grep{$_>=0x20 && $_<0x7F}@cs;
if (@acs) {
    $acs = pack "C*", @acs; $acs =~ s/[\\"]/\\$1/g;
    printf "                  case 0x%04x: outptr = \"%s\"; /"."* %s *"."/\n",
        $c, $acs, $_[1];
}
'

                     * and then edited by hand.
                     */
                  case 0x00a0: outptr = " "; break; /* NO-BREAK SPACE */
                  case 0x00aa: outptr = "a"; break; /* FEMININE ORDINAL INDICATOR */
                  case 0x00b2: outptr = "2"; break; /* SUPERSCRIPT TWO */
                  case 0x00b3: outptr = "3"; break; /* SUPERSCRIPT THREE */
                  case 0x00b9: outptr = "1"; break; /* SUPERSCRIPT ONE */
                  case 0x00ba: outptr = "o"; break; /* MASCULINE ORDINAL INDICATOR */
                  case 0x00bc: outptr = "1/4"; break; /* VULGAR FRACTION ONE QUARTER */
                  case 0x00bd: outptr = "1/2"; break; /* VULGAR FRACTION ONE HALF */
                  case 0x00be: outptr = "3/4"; break; /* VULGAR FRACTION THREE QUARTERS */
                  case 0x00c0: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH GRAVE */
                  case 0x00c1: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH ACUTE */
                  case 0x00c2: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
                  case 0x00c3: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH TILDE */
                  case 0x00c4: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH DIAERESIS */
                  case 0x00c5: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH RING ABOVE */
                  case 0x00c7: outptr = "C"; break; /* LATIN CAPITAL LETTER C WITH CEDILLA */
                  case 0x00c8: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH GRAVE */
                  case 0x00c9: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH ACUTE */
                  case 0x00ca: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
                  case 0x00cb: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH DIAERESIS */
                  case 0x00cc: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH GRAVE */
                  case 0x00cd: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH ACUTE */
                  case 0x00ce: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
                  case 0x00cf: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH DIAERESIS */
                  case 0x00d1: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH TILDE */
                  case 0x00d2: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH GRAVE */
                  case 0x00d3: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH ACUTE */
                  case 0x00d4: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
                  case 0x00d5: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH TILDE */
                  case 0x00d6: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH DIAERESIS */
                  case 0x00d9: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH GRAVE */
                  case 0x00da: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH ACUTE */
                  case 0x00db: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
                  case 0x00dc: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH DIAERESIS */
                  case 0x00dd: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH ACUTE */
                  case 0x00e0: outptr = "a"; break; /* LATIN SMALL LETTER A WITH GRAVE */
                  case 0x00e1: outptr = "a"; break; /* LATIN SMALL LETTER A WITH ACUTE */
                  case 0x00e2: outptr = "a"; break; /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
                  case 0x00e3: outptr = "a"; break; /* LATIN SMALL LETTER A WITH TILDE */
                  case 0x00e4: outptr = "a"; break; /* LATIN SMALL LETTER A WITH DIAERESIS */
                  case 0x00e5: outptr = "a"; break; /* LATIN SMALL LETTER A WITH RING ABOVE */
                  case 0x00e7: outptr = "c"; break; /* LATIN SMALL LETTER C WITH CEDILLA */
                  case 0x00e8: outptr = "e"; break; /* LATIN SMALL LETTER E WITH GRAVE */
                  case 0x00e9: outptr = "e"; break; /* LATIN SMALL LETTER E WITH ACUTE */
                  case 0x00ea: outptr = "e"; break; /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
                  case 0x00eb: outptr = "e"; break; /* LATIN SMALL LETTER E WITH DIAERESIS */
                  case 0x00ec: outptr = "i"; break; /* LATIN SMALL LETTER I WITH GRAVE */
                  case 0x00ed: outptr = "i"; break; /* LATIN SMALL LETTER I WITH ACUTE */
                  case 0x00ee: outptr = "i"; break; /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
                  case 0x00ef: outptr = "i"; break; /* LATIN SMALL LETTER I WITH DIAERESIS */
                  case 0x00f1: outptr = "n"; break; /* LATIN SMALL LETTER N WITH TILDE */
                  case 0x00f2: outptr = "o"; break; /* LATIN SMALL LETTER O WITH GRAVE */
                  case 0x00f3: outptr = "o"; break; /* LATIN SMALL LETTER O WITH ACUTE */
                  case 0x00f4: outptr = "o"; break; /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
                  case 0x00f5: outptr = "o"; break; /* LATIN SMALL LETTER O WITH TILDE */
                  case 0x00f6: outptr = "o"; break; /* LATIN SMALL LETTER O WITH DIAERESIS */
                  case 0x00f9: outptr = "u"; break; /* LATIN SMALL LETTER U WITH GRAVE */
                  case 0x00fa: outptr = "u"; break; /* LATIN SMALL LETTER U WITH ACUTE */
                  case 0x00fb: outptr = "u"; break; /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
                  case 0x00fc: outptr = "u"; break; /* LATIN SMALL LETTER U WITH DIAERESIS */
                  case 0x00fd: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH ACUTE */
                  case 0x00ff: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH DIAERESIS */
                  case 0x0100: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH MACRON */
                  case 0x0101: outptr = "a"; break; /* LATIN SMALL LETTER A WITH MACRON */
                  case 0x0102: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH BREVE */
                  case 0x0103: outptr = "a"; break; /* LATIN SMALL LETTER A WITH BREVE */
                  case 0x0104: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH OGONEK */
                  case 0x0105: outptr = "a"; break; /* LATIN SMALL LETTER A WITH OGONEK */
                  case 0x0106: outptr = "C"; break; /* LATIN CAPITAL LETTER C WITH ACUTE */
                  case 0x0107: outptr = "c"; break; /* LATIN SMALL LETTER C WITH ACUTE */
                  case 0x0108: outptr = "C"; break; /* LATIN CAPITAL LETTER C WITH CIRCUMFLEX */
                  case 0x0109: outptr = "c"; break; /* LATIN SMALL LETTER C WITH CIRCUMFLEX */
                  case 0x010a: outptr = "C"; break; /* LATIN CAPITAL LETTER C WITH DOT ABOVE */
                  case 0x010b: outptr = "c"; break; /* LATIN SMALL LETTER C WITH DOT ABOVE */
                  case 0x010c: outptr = "C"; break; /* LATIN CAPITAL LETTER C WITH CARON */
                  case 0x010d: outptr = "c"; break; /* LATIN SMALL LETTER C WITH CARON */
                  case 0x010e: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH CARON */
                  case 0x010f: outptr = "d"; break; /* LATIN SMALL LETTER D WITH CARON */
                  case 0x0112: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH MACRON */
                  case 0x0113: outptr = "e"; break; /* LATIN SMALL LETTER E WITH MACRON */
                  case 0x0114: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH BREVE */
                  case 0x0115: outptr = "e"; break; /* LATIN SMALL LETTER E WITH BREVE */
                  case 0x0116: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH DOT ABOVE */
                  case 0x0117: outptr = "e"; break; /* LATIN SMALL LETTER E WITH DOT ABOVE */
                  case 0x0118: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH OGONEK */
                  case 0x0119: outptr = "e"; break; /* LATIN SMALL LETTER E WITH OGONEK */
                  case 0x011a: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH CARON */
                  case 0x011b: outptr = "e"; break; /* LATIN SMALL LETTER E WITH CARON */
                  case 0x011c: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH CIRCUMFLEX */
                  case 0x011d: outptr = "g"; break; /* LATIN SMALL LETTER G WITH CIRCUMFLEX */
                  case 0x011e: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH BREVE */
                  case 0x011f: outptr = "g"; break; /* LATIN SMALL LETTER G WITH BREVE */
                  case 0x0120: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH DOT ABOVE */
                  case 0x0121: outptr = "g"; break; /* LATIN SMALL LETTER G WITH DOT ABOVE */
                  case 0x0122: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH CEDILLA */
                  case 0x0123: outptr = "g"; break; /* LATIN SMALL LETTER G WITH CEDILLA */
                  case 0x0124: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH CIRCUMFLEX */
                  case 0x0125: outptr = "h"; break; /* LATIN SMALL LETTER H WITH CIRCUMFLEX */
                  case 0x0128: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH TILDE */
                  case 0x0129: outptr = "i"; break; /* LATIN SMALL LETTER I WITH TILDE */
                  case 0x012a: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH MACRON */
                  case 0x012b: outptr = "i"; break; /* LATIN SMALL LETTER I WITH MACRON */
                  case 0x012c: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH BREVE */
                  case 0x012d: outptr = "i"; break; /* LATIN SMALL LETTER I WITH BREVE */
                  case 0x012e: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH OGONEK */
                  case 0x012f: outptr = "i"; break; /* LATIN SMALL LETTER I WITH OGONEK */
                  case 0x0130: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH DOT ABOVE */
                  case 0x0132: outptr = "IJ"; break; /* LATIN CAPITAL LIGATURE IJ */
                  case 0x0133: outptr = "ij"; break; /* LATIN SMALL LIGATURE IJ */
                  case 0x0134: outptr = "J"; break; /* LATIN CAPITAL LETTER J WITH CIRCUMFLEX */
                  case 0x0135: outptr = "j"; break; /* LATIN SMALL LETTER J WITH CIRCUMFLEX */
                  case 0x0136: outptr = "K"; break; /* LATIN CAPITAL LETTER K WITH CEDILLA */
                  case 0x0137: outptr = "k"; break; /* LATIN SMALL LETTER K WITH CEDILLA */
                  case 0x0139: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH ACUTE */
                  case 0x013a: outptr = "l"; break; /* LATIN SMALL LETTER L WITH ACUTE */
                  case 0x013b: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH CEDILLA */
                  case 0x013c: outptr = "l"; break; /* LATIN SMALL LETTER L WITH CEDILLA */
                  case 0x013d: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH CARON */
                  case 0x013e: outptr = "l"; break; /* LATIN SMALL LETTER L WITH CARON */
                  case 0x013f: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH MIDDLE DOT */
                  case 0x0140: outptr = "l"; break; /* LATIN SMALL LETTER L WITH MIDDLE DOT */
                  case 0x0143: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH ACUTE */
                  case 0x0144: outptr = "n"; break; /* LATIN SMALL LETTER N WITH ACUTE */
                  case 0x0145: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH CEDILLA */
                  case 0x0146: outptr = "n"; break; /* LATIN SMALL LETTER N WITH CEDILLA */
                  case 0x0147: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH CARON */
                  case 0x0148: outptr = "n"; break; /* LATIN SMALL LETTER N WITH CARON */
                  case 0x0149: outptr = "n"; break; /* LATIN SMALL LETTER N PRECEDED BY APOSTROPHE */
                  case 0x014c: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH MACRON */
                  case 0x014d: outptr = "o"; break; /* LATIN SMALL LETTER O WITH MACRON */
                  case 0x014e: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH BREVE */
                  case 0x014f: outptr = "o"; break; /* LATIN SMALL LETTER O WITH BREVE */
                  case 0x0150: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
                  case 0x0151: outptr = "o"; break; /* LATIN SMALL LETTER O WITH DOUBLE ACUTE */
                  case 0x0154: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH ACUTE */
                  case 0x0155: outptr = "r"; break; /* LATIN SMALL LETTER R WITH ACUTE */
                  case 0x0156: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH CEDILLA */
                  case 0x0157: outptr = "r"; break; /* LATIN SMALL LETTER R WITH CEDILLA */
                  case 0x0158: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH CARON */
                  case 0x0159: outptr = "r"; break; /* LATIN SMALL LETTER R WITH CARON */
                  case 0x015a: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH ACUTE */
                  case 0x015b: outptr = "s"; break; /* LATIN SMALL LETTER S WITH ACUTE */
                  case 0x015c: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH CIRCUMFLEX */
                  case 0x015d: outptr = "s"; break; /* LATIN SMALL LETTER S WITH CIRCUMFLEX */
                  case 0x015e: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH CEDILLA */
                  case 0x015f: outptr = "s"; break; /* LATIN SMALL LETTER S WITH CEDILLA */
                  case 0x0160: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH CARON */
                  case 0x0161: outptr = "s"; break; /* LATIN SMALL LETTER S WITH CARON */
                  case 0x0162: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH CEDILLA */
                  case 0x0163: outptr = "t"; break; /* LATIN SMALL LETTER T WITH CEDILLA */
                  case 0x0164: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH CARON */
                  case 0x0165: outptr = "t"; break; /* LATIN SMALL LETTER T WITH CARON */
                  case 0x0168: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH TILDE */
                  case 0x0169: outptr = "u"; break; /* LATIN SMALL LETTER U WITH TILDE */
                  case 0x016a: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH MACRON */
                  case 0x016b: outptr = "u"; break; /* LATIN SMALL LETTER U WITH MACRON */
                  case 0x016c: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH BREVE */
                  case 0x016d: outptr = "u"; break; /* LATIN SMALL LETTER U WITH BREVE */
                  case 0x016e: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH RING ABOVE */
                  case 0x016f: outptr = "u"; break; /* LATIN SMALL LETTER U WITH RING ABOVE */
                  case 0x0170: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
                  case 0x0171: outptr = "u"; break; /* LATIN SMALL LETTER U WITH DOUBLE ACUTE */
                  case 0x0172: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH OGONEK */
                  case 0x0173: outptr = "u"; break; /* LATIN SMALL LETTER U WITH OGONEK */
                  case 0x0174: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH CIRCUMFLEX */
                  case 0x0175: outptr = "w"; break; /* LATIN SMALL LETTER W WITH CIRCUMFLEX */
                  case 0x0176: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH CIRCUMFLEX */
                  case 0x0177: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH CIRCUMFLEX */
                  case 0x0178: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
                  case 0x0179: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH ACUTE */
                  case 0x017a: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH ACUTE */
                  case 0x017b: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH DOT ABOVE */
                  case 0x017c: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH DOT ABOVE */
                  case 0x017d: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH CARON */
                  case 0x017e: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH CARON */
                  case 0x017f: outptr = "s"; break; /* LATIN SMALL LETTER LONG S */
                  case 0x01a0: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH HORN */
                  case 0x01a1: outptr = "o"; break; /* LATIN SMALL LETTER O WITH HORN */
                  case 0x01af: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH HORN */
                  case 0x01b0: outptr = "u"; break; /* LATIN SMALL LETTER U WITH HORN */
                  case 0x01c4: outptr = "DZ"; break; /* LATIN CAPITAL LETTER DZ WITH CARON */
                  case 0x01c5: outptr = "Dz"; break; /* LATIN CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON */
                  case 0x01c6: outptr = "dz"; break; /* LATIN SMALL LETTER DZ WITH CARON */
                  case 0x01c7: outptr = "LJ"; break; /* LATIN CAPITAL LETTER LJ */
                  case 0x01c8: outptr = "Lj"; break; /* LATIN CAPITAL LETTER L WITH SMALL LETTER J */
                  case 0x01c9: outptr = "lj"; break; /* LATIN SMALL LETTER LJ */
                  case 0x01ca: outptr = "NJ"; break; /* LATIN CAPITAL LETTER NJ */
                  case 0x01cb: outptr = "Nj"; break; /* LATIN CAPITAL LETTER N WITH SMALL LETTER J */
                  case 0x01cc: outptr = "nj"; break; /* LATIN SMALL LETTER NJ */
                  case 0x01cd: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH CARON */
                  case 0x01ce: outptr = "a"; break; /* LATIN SMALL LETTER A WITH CARON */
                  case 0x01cf: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH CARON */
                  case 0x01d0: outptr = "i"; break; /* LATIN SMALL LETTER I WITH CARON */
                  case 0x01d1: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH CARON */
                  case 0x01d2: outptr = "o"; break; /* LATIN SMALL LETTER O WITH CARON */
                  case 0x01d3: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH CARON */
                  case 0x01d4: outptr = "u"; break; /* LATIN SMALL LETTER U WITH CARON */
                  case 0x01e6: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH CARON */
                  case 0x01e7: outptr = "g"; break; /* LATIN SMALL LETTER G WITH CARON */
                  case 0x01e8: outptr = "K"; break; /* LATIN CAPITAL LETTER K WITH CARON */
                  case 0x01e9: outptr = "k"; break; /* LATIN SMALL LETTER K WITH CARON */
                  case 0x01ea: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH OGONEK */
                  case 0x01eb: outptr = "o"; break; /* LATIN SMALL LETTER O WITH OGONEK */
                  case 0x01f0: outptr = "j"; break; /* LATIN SMALL LETTER J WITH CARON */
                  case 0x01f1: outptr = "DZ"; break; /* LATIN CAPITAL LETTER DZ */
                  case 0x01f2: outptr = "Dz"; break; /* LATIN CAPITAL LETTER D WITH SMALL LETTER Z */
                  case 0x01f3: outptr = "dz"; break; /* LATIN SMALL LETTER DZ */
                  case 0x01f4: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH ACUTE */
                  case 0x01f5: outptr = "g"; break; /* LATIN SMALL LETTER G WITH ACUTE */
                  case 0x01f8: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH GRAVE */
                  case 0x01f9: outptr = "n"; break; /* LATIN SMALL LETTER N WITH GRAVE */
                  case 0x0200: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH DOUBLE GRAVE */
                  case 0x0201: outptr = "a"; break; /* LATIN SMALL LETTER A WITH DOUBLE GRAVE */
                  case 0x0202: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH INVERTED BREVE */
                  case 0x0203: outptr = "a"; break; /* LATIN SMALL LETTER A WITH INVERTED BREVE */
                  case 0x0204: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH DOUBLE GRAVE */
                  case 0x0205: outptr = "e"; break; /* LATIN SMALL LETTER E WITH DOUBLE GRAVE */
                  case 0x0206: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH INVERTED BREVE */
                  case 0x0207: outptr = "e"; break; /* LATIN SMALL LETTER E WITH INVERTED BREVE */
                  case 0x0208: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH DOUBLE GRAVE */
                  case 0x0209: outptr = "i"; break; /* LATIN SMALL LETTER I WITH DOUBLE GRAVE */
                  case 0x020a: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH INVERTED BREVE */
                  case 0x020b: outptr = "i"; break; /* LATIN SMALL LETTER I WITH INVERTED BREVE */
                  case 0x020c: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH DOUBLE GRAVE */
                  case 0x020d: outptr = "o"; break; /* LATIN SMALL LETTER O WITH DOUBLE GRAVE */
                  case 0x020e: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH INVERTED BREVE */
                  case 0x020f: outptr = "o"; break; /* LATIN SMALL LETTER O WITH INVERTED BREVE */
                  case 0x0210: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH DOUBLE GRAVE */
                  case 0x0211: outptr = "r"; break; /* LATIN SMALL LETTER R WITH DOUBLE GRAVE */
                  case 0x0212: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH INVERTED BREVE */
                  case 0x0213: outptr = "r"; break; /* LATIN SMALL LETTER R WITH INVERTED BREVE */
                  case 0x0214: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH DOUBLE GRAVE */
                  case 0x0215: outptr = "u"; break; /* LATIN SMALL LETTER U WITH DOUBLE GRAVE */
                  case 0x0216: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH INVERTED BREVE */
                  case 0x0217: outptr = "u"; break; /* LATIN SMALL LETTER U WITH INVERTED BREVE */
                  case 0x0218: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH COMMA BELOW */
                  case 0x0219: outptr = "s"; break; /* LATIN SMALL LETTER S WITH COMMA BELOW */
                  case 0x021a: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH COMMA BELOW */
                  case 0x021b: outptr = "t"; break; /* LATIN SMALL LETTER T WITH COMMA BELOW */
                  case 0x021e: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH CARON */
                  case 0x021f: outptr = "h"; break; /* LATIN SMALL LETTER H WITH CARON */
                  case 0x0226: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH DOT ABOVE */
                  case 0x0227: outptr = "a"; break; /* LATIN SMALL LETTER A WITH DOT ABOVE */
                  case 0x0228: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH CEDILLA */
                  case 0x0229: outptr = "e"; break; /* LATIN SMALL LETTER E WITH CEDILLA */
                  case 0x022e: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH DOT ABOVE */
                  case 0x022f: outptr = "o"; break; /* LATIN SMALL LETTER O WITH DOT ABOVE */
                  case 0x0232: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH MACRON */
                  case 0x0233: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH MACRON */
                  case 0x02b0: outptr = "h"; break; /* MODIFIER LETTER SMALL H */
                  case 0x02b2: outptr = "j"; break; /* MODIFIER LETTER SMALL J */
                  case 0x02b3: outptr = "r"; break; /* MODIFIER LETTER SMALL R */
                  case 0x02b7: outptr = "w"; break; /* MODIFIER LETTER SMALL W */
                  case 0x02b8: outptr = "y"; break; /* MODIFIER LETTER SMALL Y */
                  case 0x02e1: outptr = "l"; break; /* MODIFIER LETTER SMALL L */
                  case 0x02e2: outptr = "s"; break; /* MODIFIER LETTER SMALL S */
                  case 0x02e3: outptr = "x"; break; /* MODIFIER LETTER SMALL X */
                  case 0x1d2c: outptr = "A"; break; /* MODIFIER LETTER CAPITAL A */
                  case 0x1d2e: outptr = "B"; break; /* MODIFIER LETTER CAPITAL B */
                  case 0x1d30: outptr = "D"; break; /* MODIFIER LETTER CAPITAL D */
                  case 0x1d31: outptr = "E"; break; /* MODIFIER LETTER CAPITAL E */
                  case 0x1d33: outptr = "G"; break; /* MODIFIER LETTER CAPITAL G */
                  case 0x1d34: outptr = "H"; break; /* MODIFIER LETTER CAPITAL H */
                  case 0x1d35: outptr = "I"; break; /* MODIFIER LETTER CAPITAL I */
                  case 0x1d36: outptr = "J"; break; /* MODIFIER LETTER CAPITAL J */
                  case 0x1d37: outptr = "K"; break; /* MODIFIER LETTER CAPITAL K */
                  case 0x1d38: outptr = "L"; break; /* MODIFIER LETTER CAPITAL L */
                  case 0x1d39: outptr = "M"; break; /* MODIFIER LETTER CAPITAL M */
                  case 0x1d3a: outptr = "N"; break; /* MODIFIER LETTER CAPITAL N */
                  case 0x1d3c: outptr = "O"; break; /* MODIFIER LETTER CAPITAL O */
                  case 0x1d3e: outptr = "P"; break; /* MODIFIER LETTER CAPITAL P */
                  case 0x1d3f: outptr = "R"; break; /* MODIFIER LETTER CAPITAL R */
                  case 0x1d40: outptr = "T"; break; /* MODIFIER LETTER CAPITAL T */
                  case 0x1d41: outptr = "U"; break; /* MODIFIER LETTER CAPITAL U */
                  case 0x1d42: outptr = "W"; break; /* MODIFIER LETTER CAPITAL W */
                  case 0x1d43: outptr = "a"; break; /* MODIFIER LETTER SMALL A */
                  case 0x1d47: outptr = "b"; break; /* MODIFIER LETTER SMALL B */
                  case 0x1d48: outptr = "d"; break; /* MODIFIER LETTER SMALL D */
                  case 0x1d49: outptr = "e"; break; /* MODIFIER LETTER SMALL E */
                  case 0x1d4d: outptr = "g"; break; /* MODIFIER LETTER SMALL G */
                  case 0x1d4f: outptr = "k"; break; /* MODIFIER LETTER SMALL K */
                  case 0x1d50: outptr = "m"; break; /* MODIFIER LETTER SMALL M */
                  case 0x1d52: outptr = "o"; break; /* MODIFIER LETTER SMALL O */
                  case 0x1d56: outptr = "p"; break; /* MODIFIER LETTER SMALL P */
                  case 0x1d57: outptr = "t"; break; /* MODIFIER LETTER SMALL T */
                  case 0x1d58: outptr = "u"; break; /* MODIFIER LETTER SMALL U */
                  case 0x1d5b: outptr = "v"; break; /* MODIFIER LETTER SMALL V */
                  case 0x1d62: outptr = "i"; break; /* LATIN SUBSCRIPT SMALL LETTER I */
                  case 0x1d63: outptr = "r"; break; /* LATIN SUBSCRIPT SMALL LETTER R */
                  case 0x1d64: outptr = "u"; break; /* LATIN SUBSCRIPT SMALL LETTER U */
                  case 0x1d65: outptr = "v"; break; /* LATIN SUBSCRIPT SMALL LETTER V */
                  case 0x1d9c: outptr = "c"; break; /* MODIFIER LETTER SMALL C */
                  case 0x1da0: outptr = "f"; break; /* MODIFIER LETTER SMALL F */
                  case 0x1dbb: outptr = "z"; break; /* MODIFIER LETTER SMALL Z */
                  case 0x1e00: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH RING BELOW */
                  case 0x1e01: outptr = "a"; break; /* LATIN SMALL LETTER A WITH RING BELOW */
                  case 0x1e02: outptr = "B"; break; /* LATIN CAPITAL LETTER B WITH DOT ABOVE */
                  case 0x1e03: outptr = "b"; break; /* LATIN SMALL LETTER B WITH DOT ABOVE */
                  case 0x1e04: outptr = "B"; break; /* LATIN CAPITAL LETTER B WITH DOT BELOW */
                  case 0x1e05: outptr = "b"; break; /* LATIN SMALL LETTER B WITH DOT BELOW */
                  case 0x1e06: outptr = "B"; break; /* LATIN CAPITAL LETTER B WITH LINE BELOW */
                  case 0x1e07: outptr = "b"; break; /* LATIN SMALL LETTER B WITH LINE BELOW */
                  case 0x1e0a: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH DOT ABOVE */
                  case 0x1e0b: outptr = "d"; break; /* LATIN SMALL LETTER D WITH DOT ABOVE */
                  case 0x1e0c: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH DOT BELOW */
                  case 0x1e0d: outptr = "d"; break; /* LATIN SMALL LETTER D WITH DOT BELOW */
                  case 0x1e0e: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH LINE BELOW */
                  case 0x1e0f: outptr = "d"; break; /* LATIN SMALL LETTER D WITH LINE BELOW */
                  case 0x1e10: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH CEDILLA */
                  case 0x1e11: outptr = "d"; break; /* LATIN SMALL LETTER D WITH CEDILLA */
                  case 0x1e12: outptr = "D"; break; /* LATIN CAPITAL LETTER D WITH CIRCUMFLEX BELOW */
                  case 0x1e13: outptr = "d"; break; /* LATIN SMALL LETTER D WITH CIRCUMFLEX BELOW */
                  case 0x1e18: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX BELOW */
                  case 0x1e19: outptr = "e"; break; /* LATIN SMALL LETTER E WITH CIRCUMFLEX BELOW */
                  case 0x1e1a: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH TILDE BELOW */
                  case 0x1e1b: outptr = "e"; break; /* LATIN SMALL LETTER E WITH TILDE BELOW */
                  case 0x1e1e: outptr = "F"; break; /* LATIN CAPITAL LETTER F WITH DOT ABOVE */
                  case 0x1e1f: outptr = "f"; break; /* LATIN SMALL LETTER F WITH DOT ABOVE */
                  case 0x1e20: outptr = "G"; break; /* LATIN CAPITAL LETTER G WITH MACRON */
                  case 0x1e21: outptr = "g"; break; /* LATIN SMALL LETTER G WITH MACRON */
                  case 0x1e22: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH DOT ABOVE */
                  case 0x1e23: outptr = "h"; break; /* LATIN SMALL LETTER H WITH DOT ABOVE */
                  case 0x1e24: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH DOT BELOW */
                  case 0x1e25: outptr = "h"; break; /* LATIN SMALL LETTER H WITH DOT BELOW */
                  case 0x1e26: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH DIAERESIS */
                  case 0x1e27: outptr = "h"; break; /* LATIN SMALL LETTER H WITH DIAERESIS */
                  case 0x1e28: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH CEDILLA */
                  case 0x1e29: outptr = "h"; break; /* LATIN SMALL LETTER H WITH CEDILLA */
                  case 0x1e2a: outptr = "H"; break; /* LATIN CAPITAL LETTER H WITH BREVE BELOW */
                  case 0x1e2b: outptr = "h"; break; /* LATIN SMALL LETTER H WITH BREVE BELOW */
                  case 0x1e2c: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH TILDE BELOW */
                  case 0x1e2d: outptr = "i"; break; /* LATIN SMALL LETTER I WITH TILDE BELOW */
                  case 0x1e30: outptr = "K"; break; /* LATIN CAPITAL LETTER K WITH ACUTE */
                  case 0x1e31: outptr = "k"; break; /* LATIN SMALL LETTER K WITH ACUTE */
                  case 0x1e32: outptr = "K"; break; /* LATIN CAPITAL LETTER K WITH DOT BELOW */
                  case 0x1e33: outptr = "k"; break; /* LATIN SMALL LETTER K WITH DOT BELOW */
                  case 0x1e34: outptr = "K"; break; /* LATIN CAPITAL LETTER K WITH LINE BELOW */
                  case 0x1e35: outptr = "k"; break; /* LATIN SMALL LETTER K WITH LINE BELOW */
                  case 0x1e36: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH DOT BELOW */
                  case 0x1e37: outptr = "l"; break; /* LATIN SMALL LETTER L WITH DOT BELOW */
                  case 0x1e3a: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH LINE BELOW */
                  case 0x1e3b: outptr = "l"; break; /* LATIN SMALL LETTER L WITH LINE BELOW */
                  case 0x1e3c: outptr = "L"; break; /* LATIN CAPITAL LETTER L WITH CIRCUMFLEX BELOW */
                  case 0x1e3d: outptr = "l"; break; /* LATIN SMALL LETTER L WITH CIRCUMFLEX BELOW */
                  case 0x1e3e: outptr = "M"; break; /* LATIN CAPITAL LETTER M WITH ACUTE */
                  case 0x1e3f: outptr = "m"; break; /* LATIN SMALL LETTER M WITH ACUTE */
                  case 0x1e40: outptr = "M"; break; /* LATIN CAPITAL LETTER M WITH DOT ABOVE */
                  case 0x1e41: outptr = "m"; break; /* LATIN SMALL LETTER M WITH DOT ABOVE */
                  case 0x1e42: outptr = "M"; break; /* LATIN CAPITAL LETTER M WITH DOT BELOW */
                  case 0x1e43: outptr = "m"; break; /* LATIN SMALL LETTER M WITH DOT BELOW */
                  case 0x1e44: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH DOT ABOVE */
                  case 0x1e45: outptr = "n"; break; /* LATIN SMALL LETTER N WITH DOT ABOVE */
                  case 0x1e46: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH DOT BELOW */
                  case 0x1e47: outptr = "n"; break; /* LATIN SMALL LETTER N WITH DOT BELOW */
                  case 0x1e48: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH LINE BELOW */
                  case 0x1e49: outptr = "n"; break; /* LATIN SMALL LETTER N WITH LINE BELOW */
                  case 0x1e4a: outptr = "N"; break; /* LATIN CAPITAL LETTER N WITH CIRCUMFLEX BELOW */
                  case 0x1e4b: outptr = "n"; break; /* LATIN SMALL LETTER N WITH CIRCUMFLEX BELOW */
                  case 0x1e54: outptr = "P"; break; /* LATIN CAPITAL LETTER P WITH ACUTE */
                  case 0x1e55: outptr = "p"; break; /* LATIN SMALL LETTER P WITH ACUTE */
                  case 0x1e56: outptr = "P"; break; /* LATIN CAPITAL LETTER P WITH DOT ABOVE */
                  case 0x1e57: outptr = "p"; break; /* LATIN SMALL LETTER P WITH DOT ABOVE */
                  case 0x1e58: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH DOT ABOVE */
                  case 0x1e59: outptr = "r"; break; /* LATIN SMALL LETTER R WITH DOT ABOVE */
                  case 0x1e5a: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH DOT BELOW */
                  case 0x1e5b: outptr = "r"; break; /* LATIN SMALL LETTER R WITH DOT BELOW */
                  case 0x1e5e: outptr = "R"; break; /* LATIN CAPITAL LETTER R WITH LINE BELOW */
                  case 0x1e5f: outptr = "r"; break; /* LATIN SMALL LETTER R WITH LINE BELOW */
                  case 0x1e60: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH DOT ABOVE */
                  case 0x1e61: outptr = "s"; break; /* LATIN SMALL LETTER S WITH DOT ABOVE */
                  case 0x1e62: outptr = "S"; break; /* LATIN CAPITAL LETTER S WITH DOT BELOW */
                  case 0x1e63: outptr = "s"; break; /* LATIN SMALL LETTER S WITH DOT BELOW */
                  case 0x1e6a: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH DOT ABOVE */
                  case 0x1e6b: outptr = "t"; break; /* LATIN SMALL LETTER T WITH DOT ABOVE */
                  case 0x1e6c: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH DOT BELOW */
                  case 0x1e6d: outptr = "t"; break; /* LATIN SMALL LETTER T WITH DOT BELOW */
                  case 0x1e6e: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH LINE BELOW */
                  case 0x1e6f: outptr = "t"; break; /* LATIN SMALL LETTER T WITH LINE BELOW */
                  case 0x1e70: outptr = "T"; break; /* LATIN CAPITAL LETTER T WITH CIRCUMFLEX BELOW */
                  case 0x1e71: outptr = "t"; break; /* LATIN SMALL LETTER T WITH CIRCUMFLEX BELOW */
                  case 0x1e72: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH DIAERESIS BELOW */
                  case 0x1e73: outptr = "u"; break; /* LATIN SMALL LETTER U WITH DIAERESIS BELOW */
                  case 0x1e74: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH TILDE BELOW */
                  case 0x1e75: outptr = "u"; break; /* LATIN SMALL LETTER U WITH TILDE BELOW */
                  case 0x1e76: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX BELOW */
                  case 0x1e77: outptr = "u"; break; /* LATIN SMALL LETTER U WITH CIRCUMFLEX BELOW */
                  case 0x1e7c: outptr = "V"; break; /* LATIN CAPITAL LETTER V WITH TILDE */
                  case 0x1e7d: outptr = "v"; break; /* LATIN SMALL LETTER V WITH TILDE */
                  case 0x1e7e: outptr = "V"; break; /* LATIN CAPITAL LETTER V WITH DOT BELOW */
                  case 0x1e7f: outptr = "v"; break; /* LATIN SMALL LETTER V WITH DOT BELOW */
                  case 0x1e80: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH GRAVE */
                  case 0x1e81: outptr = "w"; break; /* LATIN SMALL LETTER W WITH GRAVE */
                  case 0x1e82: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH ACUTE */
                  case 0x1e83: outptr = "w"; break; /* LATIN SMALL LETTER W WITH ACUTE */
                  case 0x1e84: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH DIAERESIS */
                  case 0x1e85: outptr = "w"; break; /* LATIN SMALL LETTER W WITH DIAERESIS */
                  case 0x1e86: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH DOT ABOVE */
                  case 0x1e87: outptr = "w"; break; /* LATIN SMALL LETTER W WITH DOT ABOVE */
                  case 0x1e88: outptr = "W"; break; /* LATIN CAPITAL LETTER W WITH DOT BELOW */
                  case 0x1e89: outptr = "w"; break; /* LATIN SMALL LETTER W WITH DOT BELOW */
                  case 0x1e8a: outptr = "X"; break; /* LATIN CAPITAL LETTER X WITH DOT ABOVE */
                  case 0x1e8b: outptr = "x"; break; /* LATIN SMALL LETTER X WITH DOT ABOVE */
                  case 0x1e8c: outptr = "X"; break; /* LATIN CAPITAL LETTER X WITH DIAERESIS */
                  case 0x1e8d: outptr = "x"; break; /* LATIN SMALL LETTER X WITH DIAERESIS */
                  case 0x1e8e: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH DOT ABOVE */
                  case 0x1e8f: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH DOT ABOVE */
                  case 0x1e90: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH CIRCUMFLEX */
                  case 0x1e91: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH CIRCUMFLEX */
                  case 0x1e92: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH DOT BELOW */
                  case 0x1e93: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH DOT BELOW */
                  case 0x1e94: outptr = "Z"; break; /* LATIN CAPITAL LETTER Z WITH LINE BELOW */
                  case 0x1e95: outptr = "z"; break; /* LATIN SMALL LETTER Z WITH LINE BELOW */
                  case 0x1e96: outptr = "h"; break; /* LATIN SMALL LETTER H WITH LINE BELOW */
                  case 0x1e97: outptr = "t"; break; /* LATIN SMALL LETTER T WITH DIAERESIS */
                  case 0x1e98: outptr = "w"; break; /* LATIN SMALL LETTER W WITH RING ABOVE */
                  case 0x1e99: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH RING ABOVE */
                  case 0x1e9a: outptr = "a"; break; /* LATIN SMALL LETTER A WITH RIGHT HALF RING */
                  case 0x1ea0: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH DOT BELOW */
                  case 0x1ea1: outptr = "a"; break; /* LATIN SMALL LETTER A WITH DOT BELOW */
                  case 0x1ea2: outptr = "A"; break; /* LATIN CAPITAL LETTER A WITH HOOK ABOVE */
                  case 0x1ea3: outptr = "a"; break; /* LATIN SMALL LETTER A WITH HOOK ABOVE */
                  case 0x1eb8: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH DOT BELOW */
                  case 0x1eb9: outptr = "e"; break; /* LATIN SMALL LETTER E WITH DOT BELOW */
                  case 0x1eba: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH HOOK ABOVE */
                  case 0x1ebb: outptr = "e"; break; /* LATIN SMALL LETTER E WITH HOOK ABOVE */
                  case 0x1ebc: outptr = "E"; break; /* LATIN CAPITAL LETTER E WITH TILDE */
                  case 0x1ebd: outptr = "e"; break; /* LATIN SMALL LETTER E WITH TILDE */
                  case 0x1ec8: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH HOOK ABOVE */
                  case 0x1ec9: outptr = "i"; break; /* LATIN SMALL LETTER I WITH HOOK ABOVE */
                  case 0x1eca: outptr = "I"; break; /* LATIN CAPITAL LETTER I WITH DOT BELOW */
                  case 0x1ecb: outptr = "i"; break; /* LATIN SMALL LETTER I WITH DOT BELOW */
                  case 0x1ecc: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH DOT BELOW */
                  case 0x1ecd: outptr = "o"; break; /* LATIN SMALL LETTER O WITH DOT BELOW */
                  case 0x1ece: outptr = "O"; break; /* LATIN CAPITAL LETTER O WITH HOOK ABOVE */
                  case 0x1ecf: outptr = "o"; break; /* LATIN SMALL LETTER O WITH HOOK ABOVE */
                  case 0x1ee4: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH DOT BELOW */
                  case 0x1ee5: outptr = "u"; break; /* LATIN SMALL LETTER U WITH DOT BELOW */
                  case 0x1ee6: outptr = "U"; break; /* LATIN CAPITAL LETTER U WITH HOOK ABOVE */
                  case 0x1ee7: outptr = "u"; break; /* LATIN SMALL LETTER U WITH HOOK ABOVE */
                  case 0x1ef2: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH GRAVE */
                  case 0x1ef3: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH GRAVE */
                  case 0x1ef4: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH DOT BELOW */
                  case 0x1ef5: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH DOT BELOW */
                  case 0x1ef6: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH HOOK ABOVE */
                  case 0x1ef7: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH HOOK ABOVE */
                  case 0x1ef8: outptr = "Y"; break; /* LATIN CAPITAL LETTER Y WITH TILDE */
                  case 0x1ef9: outptr = "y"; break; /* LATIN SMALL LETTER Y WITH TILDE */
                  case 0x2002: outptr = " "; break; /* EN SPACE */
                  case 0x2003: outptr = " "; break; /* EM SPACE */
                  case 0x2004: outptr = " "; break; /* THREE-PER-EM SPACE */
                  case 0x2005: outptr = " "; break; /* FOUR-PER-EM SPACE */
                  case 0x2006: outptr = " "; break; /* SIX-PER-EM SPACE */
                  case 0x2007: outptr = " "; break; /* FIGURE SPACE */
                  case 0x2008: outptr = " "; break; /* PUNCTUATION SPACE */
                  case 0x2009: outptr = " "; break; /* THIN SPACE */
                  case 0x200a: outptr = " "; break; /* HAIR SPACE */
                  case 0x2017: outptr = " "; break; /* DOUBLE LOW LINE */
                  case 0x2024: outptr = "."; break; /* ONE DOT LEADER */
                  case 0x2025: outptr = ".."; break; /* TWO DOT LEADER */
                  case 0x2026: outptr = "..."; break; /* HORIZONTAL ELLIPSIS */
                  case 0x202f: outptr = " "; break; /* NARROW NO-BREAK SPACE */
                  case 0x203c: outptr = "!!"; break; /* DOUBLE EXCLAMATION MARK */
                  case 0x203e: outptr = " "; break; /* OVERLINE */
                  case 0x2047: outptr = "??"; break; /* DOUBLE QUESTION MARK */
                  case 0x2048: outptr = "?!"; break; /* QUESTION EXCLAMATION MARK */
                  case 0x2049: outptr = "!?"; break; /* EXCLAMATION QUESTION MARK */
                  case 0x205f: outptr = " "; break; /* MEDIUM MATHEMATICAL SPACE */
                  case 0x2070: outptr = "0"; break; /* SUPERSCRIPT ZERO */
                  case 0x2071: outptr = "i"; break; /* SUPERSCRIPT LATIN SMALL LETTER I */
                  case 0x2074: outptr = "4"; break; /* SUPERSCRIPT FOUR */
                  case 0x2075: outptr = "5"; break; /* SUPERSCRIPT FIVE */
                  case 0x2076: outptr = "6"; break; /* SUPERSCRIPT SIX */
                  case 0x2077: outptr = "7"; break; /* SUPERSCRIPT SEVEN */
                  case 0x2078: outptr = "8"; break; /* SUPERSCRIPT EIGHT */
                  case 0x2079: outptr = "9"; break; /* SUPERSCRIPT NINE */
                  case 0x207a: outptr = "+"; break; /* SUPERSCRIPT PLUS SIGN */
                  case 0x207c: outptr = "="; break; /* SUPERSCRIPT EQUALS SIGN */
                  case 0x207d: outptr = "("; break; /* SUPERSCRIPT LEFT PARENTHESIS */
                  case 0x207e: outptr = ")"; break; /* SUPERSCRIPT RIGHT PARENTHESIS */
                  case 0x207f: outptr = "n"; break; /* SUPERSCRIPT LATIN SMALL LETTER N */
                  case 0x2080: outptr = "0"; break; /* SUBSCRIPT ZERO */
                  case 0x2081: outptr = "1"; break; /* SUBSCRIPT ONE */
                  case 0x2082: outptr = "2"; break; /* SUBSCRIPT TWO */
                  case 0x2083: outptr = "3"; break; /* SUBSCRIPT THREE */
                  case 0x2084: outptr = "4"; break; /* SUBSCRIPT FOUR */
                  case 0x2085: outptr = "5"; break; /* SUBSCRIPT FIVE */
                  case 0x2086: outptr = "6"; break; /* SUBSCRIPT SIX */
                  case 0x2087: outptr = "7"; break; /* SUBSCRIPT SEVEN */
                  case 0x2088: outptr = "8"; break; /* SUBSCRIPT EIGHT */
                  case 0x2089: outptr = "9"; break; /* SUBSCRIPT NINE */
                  case 0x208a: outptr = "+"; break; /* SUBSCRIPT PLUS SIGN */
                  case 0x208c: outptr = "="; break; /* SUBSCRIPT EQUALS SIGN */
                  case 0x208d: outptr = "("; break; /* SUBSCRIPT LEFT PARENTHESIS */
                  case 0x208e: outptr = ")"; break; /* SUBSCRIPT RIGHT PARENTHESIS */
                  case 0x2090: outptr = "a"; break; /* LATIN SUBSCRIPT SMALL LETTER A */
                  case 0x2091: outptr = "e"; break; /* LATIN SUBSCRIPT SMALL LETTER E */
                  case 0x2092: outptr = "o"; break; /* LATIN SUBSCRIPT SMALL LETTER O */
                  case 0x2093: outptr = "x"; break; /* LATIN SUBSCRIPT SMALL LETTER X */
                  case 0x2095: outptr = "h"; break; /* LATIN SUBSCRIPT SMALL LETTER H */
                  case 0x2096: outptr = "k"; break; /* LATIN SUBSCRIPT SMALL LETTER K */
                  case 0x2097: outptr = "l"; break; /* LATIN SUBSCRIPT SMALL LETTER L */
                  case 0x2098: outptr = "m"; break; /* LATIN SUBSCRIPT SMALL LETTER M */
                  case 0x2099: outptr = "n"; break; /* LATIN SUBSCRIPT SMALL LETTER N */
                  case 0x209a: outptr = "p"; break; /* LATIN SUBSCRIPT SMALL LETTER P */
                  case 0x209b: outptr = "s"; break; /* LATIN SUBSCRIPT SMALL LETTER S */
                  case 0x209c: outptr = "t"; break; /* LATIN SUBSCRIPT SMALL LETTER T */
                  case 0x20a8: outptr = "Rs"; break; /* RUPEE SIGN */
                  case 0x2100: outptr = "a/c"; break; /* ACCOUNT OF */
                  case 0x2101: outptr = "a/s"; break; /* ADDRESSED TO THE SUBJECT */
                  case 0x2102: outptr = "C"; break; /* DOUBLE-STRUCK CAPITAL C */
                  case 0x2103: outptr = "C"; break; /* DEGREE CELSIUS */
                  case 0x2105: outptr = "c/o"; break; /* CARE OF */
                  case 0x2106: outptr = "c/u"; break; /* CADA UNA */
                  case 0x2109: outptr = "F"; break; /* DEGREE FAHRENHEIT */
                  case 0x210a: outptr = "g"; break; /* SCRIPT SMALL G */
                  case 0x210b: outptr = "H"; break; /* SCRIPT CAPITAL H */
                  case 0x210c: outptr = "H"; break; /* BLACK-LETTER CAPITAL H */
                  case 0x210d: outptr = "H"; break; /* DOUBLE-STRUCK CAPITAL H */
                  case 0x210e: outptr = "h"; break; /* PLANCK CONSTANT */
                  case 0x2110: outptr = "I"; break; /* SCRIPT CAPITAL I */
                  case 0x2111: outptr = "I"; break; /* BLACK-LETTER CAPITAL I */
                  case 0x2112: outptr = "L"; break; /* SCRIPT CAPITAL L */
                  case 0x2113: outptr = "l"; break; /* SCRIPT SMALL L */
                  case 0x2115: outptr = "N"; break; /* DOUBLE-STRUCK CAPITAL N */
                  case 0x2116: outptr = "No"; break; /* NUMERO SIGN */
                  case 0x2119: outptr = "P"; break; /* DOUBLE-STRUCK CAPITAL P */
                  case 0x211a: outptr = "Q"; break; /* DOUBLE-STRUCK CAPITAL Q */
                  case 0x211b: outptr = "R"; break; /* SCRIPT CAPITAL R */
                  case 0x211c: outptr = "R"; break; /* BLACK-LETTER CAPITAL R */
                  case 0x211d: outptr = "R"; break; /* DOUBLE-STRUCK CAPITAL R */
                  case 0x2120: outptr = "(SM)"; break; /* SERVICE MARK */
                  case 0x2121: outptr = "TEL"; break; /* TELEPHONE SIGN */
                  case 0x2122: outptr = "(TM)"; break; /* TRADE MARK SIGN */
                  case 0x2124: outptr = "Z"; break; /* DOUBLE-STRUCK CAPITAL Z */
                  case 0x2128: outptr = "Z"; break; /* BLACK-LETTER CAPITAL Z */
                  case 0x212a: outptr = "K"; break; /* KELVIN SIGN */
                  case 0x212c: outptr = "B"; break; /* SCRIPT CAPITAL B */
                  case 0x212d: outptr = "C"; break; /* BLACK-LETTER CAPITAL C */
                  case 0x212f: outptr = "e"; break; /* SCRIPT SMALL E */
                  case 0x2130: outptr = "E"; break; /* SCRIPT CAPITAL E */
                  case 0x2131: outptr = "F"; break; /* SCRIPT CAPITAL F */
                  case 0x2133: outptr = "M"; break; /* SCRIPT CAPITAL M */
                  case 0x2134: outptr = "o"; break; /* SCRIPT SMALL O */
                  case 0x2139: outptr = "i"; break; /* INFORMATION SOURCE */
                  case 0x213b: outptr = "FAX"; break; /* FACSIMILE SIGN */
                  case 0x2145: outptr = "D"; break; /* DOUBLE-STRUCK ITALIC CAPITAL D */
                  case 0x2146: outptr = "d"; break; /* DOUBLE-STRUCK ITALIC SMALL D */
                  case 0x2147: outptr = "e"; break; /* DOUBLE-STRUCK ITALIC SMALL E */
                  case 0x2148: outptr = "i"; break; /* DOUBLE-STRUCK ITALIC SMALL I */
                  case 0x2149: outptr = "j"; break; /* DOUBLE-STRUCK ITALIC SMALL J */
                  case 0x2150: outptr = "1/7"; break; /* VULGAR FRACTION ONE SEVENTH */
                  case 0x2151: outptr = "1/9"; break; /* VULGAR FRACTION ONE NINTH */
                  case 0x2152: outptr = "1/10"; break; /* VULGAR FRACTION ONE TENTH */
                  case 0x2153: outptr = "1/3"; break; /* VULGAR FRACTION ONE THIRD */
                  case 0x2154: outptr = "2/3"; break; /* VULGAR FRACTION TWO THIRDS */
                  case 0x2155: outptr = "1/5"; break; /* VULGAR FRACTION ONE FIFTH */
                  case 0x2156: outptr = "2/5"; break; /* VULGAR FRACTION TWO FIFTHS */
                  case 0x2157: outptr = "3/5"; break; /* VULGAR FRACTION THREE FIFTHS */
                  case 0x2158: outptr = "4/5"; break; /* VULGAR FRACTION FOUR FIFTHS */
                  case 0x2159: outptr = "1/6"; break; /* VULGAR FRACTION ONE SIXTH */
                  case 0x215a: outptr = "5/6"; break; /* VULGAR FRACTION FIVE SIXTHS */
                  case 0x215b: outptr = "1/8"; break; /* VULGAR FRACTION ONE EIGHTH */
                  case 0x215c: outptr = "3/8"; break; /* VULGAR FRACTION THREE EIGHTHS */
                  case 0x215d: outptr = "5/8"; break; /* VULGAR FRACTION FIVE EIGHTHS */
                  case 0x215e: outptr = "7/8"; break; /* VULGAR FRACTION SEVEN EIGHTHS */
                  case 0x215f: outptr = "1"; break; /* FRACTION NUMERATOR ONE */
                  case 0x2160: outptr = "I"; break; /* ROMAN NUMERAL ONE */
                  case 0x2161: outptr = "II"; break; /* ROMAN NUMERAL TWO */
                  case 0x2162: outptr = "III"; break; /* ROMAN NUMERAL THREE */
                  case 0x2163: outptr = "IV"; break; /* ROMAN NUMERAL FOUR */
                  case 0x2164: outptr = "V"; break; /* ROMAN NUMERAL FIVE */
                  case 0x2165: outptr = "VI"; break; /* ROMAN NUMERAL SIX */
                  case 0x2166: outptr = "VII"; break; /* ROMAN NUMERAL SEVEN */
                  case 0x2167: outptr = "VIII"; break; /* ROMAN NUMERAL EIGHT */
                  case 0x2168: outptr = "IX"; break; /* ROMAN NUMERAL NINE */
                  case 0x2169: outptr = "X"; break; /* ROMAN NUMERAL TEN */
                  case 0x216a: outptr = "XI"; break; /* ROMAN NUMERAL ELEVEN */
                  case 0x216b: outptr = "XII"; break; /* ROMAN NUMERAL TWELVE */
                  case 0x216c: outptr = "L"; break; /* ROMAN NUMERAL FIFTY */
                  case 0x216d: outptr = "C"; break; /* ROMAN NUMERAL ONE HUNDRED */
                  case 0x216e: outptr = "D"; break; /* ROMAN NUMERAL FIVE HUNDRED */
                  case 0x216f: outptr = "M"; break; /* ROMAN NUMERAL ONE THOUSAND */
                  case 0x2170: outptr = "i"; break; /* SMALL ROMAN NUMERAL ONE */
                  case 0x2171: outptr = "ii"; break; /* SMALL ROMAN NUMERAL TWO */
                  case 0x2172: outptr = "iii"; break; /* SMALL ROMAN NUMERAL THREE */
                  case 0x2173: outptr = "iv"; break; /* SMALL ROMAN NUMERAL FOUR */
                  case 0x2174: outptr = "v"; break; /* SMALL ROMAN NUMERAL FIVE */
                  case 0x2175: outptr = "vi"; break; /* SMALL ROMAN NUMERAL SIX */
                  case 0x2176: outptr = "vii"; break; /* SMALL ROMAN NUMERAL SEVEN */
                  case 0x2177: outptr = "viii"; break; /* SMALL ROMAN NUMERAL EIGHT */
                  case 0x2178: outptr = "ix"; break; /* SMALL ROMAN NUMERAL NINE */
                  case 0x2179: outptr = "x"; break; /* SMALL ROMAN NUMERAL TEN */
                  case 0x217a: outptr = "xi"; break; /* SMALL ROMAN NUMERAL ELEVEN */
                  case 0x217b: outptr = "xii"; break; /* SMALL ROMAN NUMERAL TWELVE */
                  case 0x217c: outptr = "l"; break; /* SMALL ROMAN NUMERAL FIFTY */
                  case 0x217d: outptr = "c"; break; /* SMALL ROMAN NUMERAL ONE HUNDRED */
                  case 0x217e: outptr = "d"; break; /* SMALL ROMAN NUMERAL FIVE HUNDRED */
                  case 0x217f: outptr = "m"; break; /* SMALL ROMAN NUMERAL ONE THOUSAND */
                  case 0x2189: outptr = "0/3"; break; /* VULGAR FRACTION ZERO THIRDS */
                  case 0x2260: outptr = "=/="; break; /* NOT EQUAL TO */
                  case 0x226e: outptr = "!<"; break; /* NOT LESS-THAN */
                  case 0x226f: outptr = "!>"; break; /* NOT GREATER-THAN */
                  case 0x2460: outptr = "1"; break; /* CIRCLED DIGIT ONE */
                  case 0x2461: outptr = "2"; break; /* CIRCLED DIGIT TWO */
                  case 0x2462: outptr = "3"; break; /* CIRCLED DIGIT THREE */
                  case 0x2463: outptr = "4"; break; /* CIRCLED DIGIT FOUR */
                  case 0x2464: outptr = "5"; break; /* CIRCLED DIGIT FIVE */
                  case 0x2465: outptr = "6"; break; /* CIRCLED DIGIT SIX */
                  case 0x2466: outptr = "7"; break; /* CIRCLED DIGIT SEVEN */
                  case 0x2467: outptr = "8"; break; /* CIRCLED DIGIT EIGHT */
                  case 0x2468: outptr = "9"; break; /* CIRCLED DIGIT NINE */
                  case 0x2469: outptr = "10"; break; /* CIRCLED NUMBER TEN */
                  case 0x246a: outptr = "11"; break; /* CIRCLED NUMBER ELEVEN */
                  case 0x246b: outptr = "12"; break; /* CIRCLED NUMBER TWELVE */
                  case 0x246c: outptr = "13"; break; /* CIRCLED NUMBER THIRTEEN */
                  case 0x246d: outptr = "14"; break; /* CIRCLED NUMBER FOURTEEN */
                  case 0x246e: outptr = "15"; break; /* CIRCLED NUMBER FIFTEEN */
                  case 0x246f: outptr = "16"; break; /* CIRCLED NUMBER SIXTEEN */
                  case 0x2470: outptr = "17"; break; /* CIRCLED NUMBER SEVENTEEN */
                  case 0x2471: outptr = "18"; break; /* CIRCLED NUMBER EIGHTEEN */
                  case 0x2472: outptr = "19"; break; /* CIRCLED NUMBER NINETEEN */
                  case 0x2473: outptr = "20"; break; /* CIRCLED NUMBER TWENTY */
                  case 0x2474: outptr = "(1)"; break; /* PARENTHESIZED DIGIT ONE */
                  case 0x2475: outptr = "(2)"; break; /* PARENTHESIZED DIGIT TWO */
                  case 0x2476: outptr = "(3)"; break; /* PARENTHESIZED DIGIT THREE */
                  case 0x2477: outptr = "(4)"; break; /* PARENTHESIZED DIGIT FOUR */
                  case 0x2478: outptr = "(5)"; break; /* PARENTHESIZED DIGIT FIVE */
                  case 0x2479: outptr = "(6)"; break; /* PARENTHESIZED DIGIT SIX */
                  case 0x247a: outptr = "(7)"; break; /* PARENTHESIZED DIGIT SEVEN */
                  case 0x247b: outptr = "(8)"; break; /* PARENTHESIZED DIGIT EIGHT */
                  case 0x247c: outptr = "(9)"; break; /* PARENTHESIZED DIGIT NINE */
                  case 0x247d: outptr = "(10)"; break; /* PARENTHESIZED NUMBER TEN */
                  case 0x247e: outptr = "(11)"; break; /* PARENTHESIZED NUMBER ELEVEN */
                  case 0x247f: outptr = "(12)"; break; /* PARENTHESIZED NUMBER TWELVE */
                  case 0x2480: outptr = "(13)"; break; /* PARENTHESIZED NUMBER THIRTEEN */
                  case 0x2481: outptr = "(14)"; break; /* PARENTHESIZED NUMBER FOURTEEN */
                  case 0x2482: outptr = "(15)"; break; /* PARENTHESIZED NUMBER FIFTEEN */
                  case 0x2483: outptr = "(16)"; break; /* PARENTHESIZED NUMBER SIXTEEN */
                  case 0x2484: outptr = "(17)"; break; /* PARENTHESIZED NUMBER SEVENTEEN */
                  case 0x2485: outptr = "(18)"; break; /* PARENTHESIZED NUMBER EIGHTEEN */
                  case 0x2486: outptr = "(19)"; break; /* PARENTHESIZED NUMBER NINETEEN */
                  case 0x2487: outptr = "(20)"; break; /* PARENTHESIZED NUMBER TWENTY */
                  case 0x2488: outptr = "1."; break; /* DIGIT ONE FULL STOP */
                  case 0x2489: outptr = "2."; break; /* DIGIT TWO FULL STOP */
                  case 0x248a: outptr = "3."; break; /* DIGIT THREE FULL STOP */
                  case 0x248b: outptr = "4."; break; /* DIGIT FOUR FULL STOP */
                  case 0x248c: outptr = "5."; break; /* DIGIT FIVE FULL STOP */
                  case 0x248d: outptr = "6."; break; /* DIGIT SIX FULL STOP */
                  case 0x248e: outptr = "7."; break; /* DIGIT SEVEN FULL STOP */
                  case 0x248f: outptr = "8."; break; /* DIGIT EIGHT FULL STOP */
                  case 0x2490: outptr = "9."; break; /* DIGIT NINE FULL STOP */
                  case 0x2491: outptr = "10."; break; /* NUMBER TEN FULL STOP */
                  case 0x2492: outptr = "11."; break; /* NUMBER ELEVEN FULL STOP */
                  case 0x2493: outptr = "12."; break; /* NUMBER TWELVE FULL STOP */
                  case 0x2494: outptr = "13."; break; /* NUMBER THIRTEEN FULL STOP */
                  case 0x2495: outptr = "14."; break; /* NUMBER FOURTEEN FULL STOP */
                  case 0x2496: outptr = "15."; break; /* NUMBER FIFTEEN FULL STOP */
                  case 0x2497: outptr = "16."; break; /* NUMBER SIXTEEN FULL STOP */
                  case 0x2498: outptr = "17."; break; /* NUMBER SEVENTEEN FULL STOP */
                  case 0x2499: outptr = "18."; break; /* NUMBER EIGHTEEN FULL STOP */
                  case 0x249a: outptr = "19."; break; /* NUMBER NINETEEN FULL STOP */
                  case 0x249b: outptr = "20."; break; /* NUMBER TWENTY FULL STOP */
                  case 0x249c: outptr = "(a)"; break; /* PARENTHESIZED LATIN SMALL LETTER A */
                  case 0x249d: outptr = "(b)"; break; /* PARENTHESIZED LATIN SMALL LETTER B */
                  case 0x249e: outptr = "(c)"; break; /* PARENTHESIZED LATIN SMALL LETTER C */
                  case 0x249f: outptr = "(d)"; break; /* PARENTHESIZED LATIN SMALL LETTER D */
                  case 0x24a0: outptr = "(e)"; break; /* PARENTHESIZED LATIN SMALL LETTER E */
                  case 0x24a1: outptr = "(f)"; break; /* PARENTHESIZED LATIN SMALL LETTER F */
                  case 0x24a2: outptr = "(g)"; break; /* PARENTHESIZED LATIN SMALL LETTER G */
                  case 0x24a3: outptr = "(h)"; break; /* PARENTHESIZED LATIN SMALL LETTER H */
                  case 0x24a4: outptr = "(i)"; break; /* PARENTHESIZED LATIN SMALL LETTER I */
                  case 0x24a5: outptr = "(j)"; break; /* PARENTHESIZED LATIN SMALL LETTER J */
                  case 0x24a6: outptr = "(k)"; break; /* PARENTHESIZED LATIN SMALL LETTER K */
                  case 0x24a7: outptr = "(l)"; break; /* PARENTHESIZED LATIN SMALL LETTER L */
                  case 0x24a8: outptr = "(m)"; break; /* PARENTHESIZED LATIN SMALL LETTER M */
                  case 0x24a9: outptr = "(n)"; break; /* PARENTHESIZED LATIN SMALL LETTER N */
                  case 0x24aa: outptr = "(o)"; break; /* PARENTHESIZED LATIN SMALL LETTER O */
                  case 0x24ab: outptr = "(p)"; break; /* PARENTHESIZED LATIN SMALL LETTER P */
                  case 0x24ac: outptr = "(q)"; break; /* PARENTHESIZED LATIN SMALL LETTER Q */
                  case 0x24ad: outptr = "(r)"; break; /* PARENTHESIZED LATIN SMALL LETTER R */
                  case 0x24ae: outptr = "(s)"; break; /* PARENTHESIZED LATIN SMALL LETTER S */
                  case 0x24af: outptr = "(t)"; break; /* PARENTHESIZED LATIN SMALL LETTER T */
                  case 0x24b0: outptr = "(u)"; break; /* PARENTHESIZED LATIN SMALL LETTER U */
                  case 0x24b1: outptr = "(v)"; break; /* PARENTHESIZED LATIN SMALL LETTER V */
                  case 0x24b2: outptr = "(w)"; break; /* PARENTHESIZED LATIN SMALL LETTER W */
                  case 0x24b3: outptr = "(x)"; break; /* PARENTHESIZED LATIN SMALL LETTER X */
                  case 0x24b4: outptr = "(y)"; break; /* PARENTHESIZED LATIN SMALL LETTER Y */
                  case 0x24b5: outptr = "(z)"; break; /* PARENTHESIZED LATIN SMALL LETTER Z */
                  case 0x24b6: outptr = "A"; break; /* CIRCLED LATIN CAPITAL LETTER A */
                  case 0x24b7: outptr = "B"; break; /* CIRCLED LATIN CAPITAL LETTER B */
                  case 0x24b8: outptr = "C"; break; /* CIRCLED LATIN CAPITAL LETTER C */
                  case 0x24b9: outptr = "D"; break; /* CIRCLED LATIN CAPITAL LETTER D */
                  case 0x24ba: outptr = "E"; break; /* CIRCLED LATIN CAPITAL LETTER E */
                  case 0x24bb: outptr = "F"; break; /* CIRCLED LATIN CAPITAL LETTER F */
                  case 0x24bc: outptr = "G"; break; /* CIRCLED LATIN CAPITAL LETTER G */
                  case 0x24bd: outptr = "H"; break; /* CIRCLED LATIN CAPITAL LETTER H */
                  case 0x24be: outptr = "I"; break; /* CIRCLED LATIN CAPITAL LETTER I */
                  case 0x24bf: outptr = "J"; break; /* CIRCLED LATIN CAPITAL LETTER J */
                  case 0x24c0: outptr = "K"; break; /* CIRCLED LATIN CAPITAL LETTER K */
                  case 0x24c1: outptr = "L"; break; /* CIRCLED LATIN CAPITAL LETTER L */
                  case 0x24c2: outptr = "M"; break; /* CIRCLED LATIN CAPITAL LETTER M */
                  case 0x24c3: outptr = "N"; break; /* CIRCLED LATIN CAPITAL LETTER N */
                  case 0x24c4: outptr = "O"; break; /* CIRCLED LATIN CAPITAL LETTER O */
                  case 0x24c5: outptr = "P"; break; /* CIRCLED LATIN CAPITAL LETTER P */
                  case 0x24c6: outptr = "Q"; break; /* CIRCLED LATIN CAPITAL LETTER Q */
                  case 0x24c7: outptr = "R"; break; /* CIRCLED LATIN CAPITAL LETTER R */
                  case 0x24c8: outptr = "S"; break; /* CIRCLED LATIN CAPITAL LETTER S */
                  case 0x24c9: outptr = "T"; break; /* CIRCLED LATIN CAPITAL LETTER T */
                  case 0x24ca: outptr = "U"; break; /* CIRCLED LATIN CAPITAL LETTER U */
                  case 0x24cb: outptr = "V"; break; /* CIRCLED LATIN CAPITAL LETTER V */
                  case 0x24cc: outptr = "W"; break; /* CIRCLED LATIN CAPITAL LETTER W */
                  case 0x24cd: outptr = "X"; break; /* CIRCLED LATIN CAPITAL LETTER X */
                  case 0x24ce: outptr = "Y"; break; /* CIRCLED LATIN CAPITAL LETTER Y */
                  case 0x24cf: outptr = "Z"; break; /* CIRCLED LATIN CAPITAL LETTER Z */
                  case 0x24d0: outptr = "a"; break; /* CIRCLED LATIN SMALL LETTER A */
                  case 0x24d1: outptr = "b"; break; /* CIRCLED LATIN SMALL LETTER B */
                  case 0x24d2: outptr = "c"; break; /* CIRCLED LATIN SMALL LETTER C */
                  case 0x24d3: outptr = "d"; break; /* CIRCLED LATIN SMALL LETTER D */
                  case 0x24d4: outptr = "e"; break; /* CIRCLED LATIN SMALL LETTER E */
                  case 0x24d5: outptr = "f"; break; /* CIRCLED LATIN SMALL LETTER F */
                  case 0x24d6: outptr = "g"; break; /* CIRCLED LATIN SMALL LETTER G */
                  case 0x24d7: outptr = "h"; break; /* CIRCLED LATIN SMALL LETTER H */
                  case 0x24d8: outptr = "i"; break; /* CIRCLED LATIN SMALL LETTER I */
                  case 0x24d9: outptr = "j"; break; /* CIRCLED LATIN SMALL LETTER J */
                  case 0x24da: outptr = "k"; break; /* CIRCLED LATIN SMALL LETTER K */
                  case 0x24db: outptr = "l"; break; /* CIRCLED LATIN SMALL LETTER L */
                  case 0x24dc: outptr = "m"; break; /* CIRCLED LATIN SMALL LETTER M */
                  case 0x24dd: outptr = "n"; break; /* CIRCLED LATIN SMALL LETTER N */
                  case 0x24de: outptr = "o"; break; /* CIRCLED LATIN SMALL LETTER O */
                  case 0x24df: outptr = "p"; break; /* CIRCLED LATIN SMALL LETTER P */
                  case 0x24e0: outptr = "q"; break; /* CIRCLED LATIN SMALL LETTER Q */
                  case 0x24e1: outptr = "r"; break; /* CIRCLED LATIN SMALL LETTER R */
                  case 0x24e2: outptr = "s"; break; /* CIRCLED LATIN SMALL LETTER S */
                  case 0x24e3: outptr = "t"; break; /* CIRCLED LATIN SMALL LETTER T */
                  case 0x24e4: outptr = "u"; break; /* CIRCLED LATIN SMALL LETTER U */
                  case 0x24e5: outptr = "v"; break; /* CIRCLED LATIN SMALL LETTER V */
                  case 0x24e6: outptr = "w"; break; /* CIRCLED LATIN SMALL LETTER W */
                  case 0x24e7: outptr = "x"; break; /* CIRCLED LATIN SMALL LETTER X */
                  case 0x24e8: outptr = "y"; break; /* CIRCLED LATIN SMALL LETTER Y */
                  case 0x24e9: outptr = "z"; break; /* CIRCLED LATIN SMALL LETTER Z */
                  case 0x24ea: outptr = "0"; break; /* CIRCLED DIGIT ZERO */
                  case 0x2a74: outptr = "::="; break; /* DOUBLE COLON EQUAL */
                  case 0x2a75: outptr = "=="; break; /* TWO CONSECUTIVE EQUALS SIGNS */
                  case 0x2a76: outptr = "==="; break; /* THREE CONSECUTIVE EQUALS SIGNS */
                  case 0x2c7c: outptr = "j"; break; /* LATIN SUBSCRIPT SMALL LETTER J */
                  case 0x2c7d: outptr = "V"; break; /* MODIFIER LETTER CAPITAL V */
                  case 0x3000: outptr = " "; break; /* IDEOGRAPHIC SPACE */
                  case 0x3250: outptr = "PTE"; break; /* PARTNERSHIP SIGN */
                  case 0x3251: outptr = "21"; break; /* CIRCLED NUMBER TWENTY ONE */
                  case 0x3252: outptr = "22"; break; /* CIRCLED NUMBER TWENTY TWO */
                  case 0x3253: outptr = "23"; break; /* CIRCLED NUMBER TWENTY THREE */
                  case 0x3254: outptr = "24"; break; /* CIRCLED NUMBER TWENTY FOUR */
                  case 0x3255: outptr = "25"; break; /* CIRCLED NUMBER TWENTY FIVE */
                  case 0x3256: outptr = "26"; break; /* CIRCLED NUMBER TWENTY SIX */
                  case 0x3257: outptr = "27"; break; /* CIRCLED NUMBER TWENTY SEVEN */
                  case 0x3258: outptr = "28"; break; /* CIRCLED NUMBER TWENTY EIGHT */
                  case 0x3259: outptr = "29"; break; /* CIRCLED NUMBER TWENTY NINE */
                  case 0x325a: outptr = "30"; break; /* CIRCLED NUMBER THIRTY */
                  case 0x325b: outptr = "31"; break; /* CIRCLED NUMBER THIRTY ONE */
                  case 0x325c: outptr = "32"; break; /* CIRCLED NUMBER THIRTY TWO */
                  case 0x325d: outptr = "33"; break; /* CIRCLED NUMBER THIRTY THREE */
                  case 0x325e: outptr = "34"; break; /* CIRCLED NUMBER THIRTY FOUR */
                  case 0x325f: outptr = "35"; break; /* CIRCLED NUMBER THIRTY FIVE */
                  case 0x32b1: outptr = "36"; break; /* CIRCLED NUMBER THIRTY SIX */
                  case 0x32b2: outptr = "37"; break; /* CIRCLED NUMBER THIRTY SEVEN */
                  case 0x32b3: outptr = "38"; break; /* CIRCLED NUMBER THIRTY EIGHT */
                  case 0x32b4: outptr = "39"; break; /* CIRCLED NUMBER THIRTY NINE */
                  case 0x32b5: outptr = "40"; break; /* CIRCLED NUMBER FORTY */
                  case 0x32b6: outptr = "41"; break; /* CIRCLED NUMBER FORTY ONE */
                  case 0x32b7: outptr = "42"; break; /* CIRCLED NUMBER FORTY TWO */
                  case 0x32b8: outptr = "43"; break; /* CIRCLED NUMBER FORTY THREE */
                  case 0x32b9: outptr = "44"; break; /* CIRCLED NUMBER FORTY FOUR */
                  case 0x32ba: outptr = "45"; break; /* CIRCLED NUMBER FORTY FIVE */
                  case 0x32bb: outptr = "46"; break; /* CIRCLED NUMBER FORTY SIX */
                  case 0x32bc: outptr = "47"; break; /* CIRCLED NUMBER FORTY SEVEN */
                  case 0x32bd: outptr = "48"; break; /* CIRCLED NUMBER FORTY EIGHT */
                  case 0x32be: outptr = "49"; break; /* CIRCLED NUMBER FORTY NINE */
                  case 0x32bf: outptr = "50"; break; /* CIRCLED NUMBER FIFTY */
                  case 0x32cc: outptr = "Hg"; break; /* SQUARE HG */
                  case 0x32cd: outptr = "erg"; break; /* SQUARE ERG */
                  case 0x32ce: outptr = "eV"; break; /* SQUARE EV */
                  case 0x32cf: outptr = "LTD"; break; /* LIMITED LIABILITY SIGN */
                  case 0x3371: outptr = "hPa"; break; /* SQUARE HPA */
                  case 0x3372: outptr = "da"; break; /* SQUARE DA */
                  case 0x3373: outptr = "AU"; break; /* SQUARE AU */
                  case 0x3374: outptr = "bar"; break; /* SQUARE BAR */
                  case 0x3375: outptr = "oV"; break; /* SQUARE OV */
                  case 0x3376: outptr = "pc"; break; /* SQUARE PC */
                  case 0x3377: outptr = "dm"; break; /* SQUARE DM */
                  case 0x3378: outptr = "dm^2"; break; /* SQUARE DM SQUARED */
                  case 0x3379: outptr = "dm^3"; break; /* SQUARE DM CUBED */
                  case 0x337a: outptr = "IU"; break; /* SQUARE IU */
                  case 0x3380: outptr = "pA"; break; /* SQUARE PA AMPS */
                  case 0x3381: outptr = "nA"; break; /* SQUARE NA */
                  case 0x3382: outptr = "A"; break; /* SQUARE MU A */
                  case 0x3383: outptr = "mA"; break; /* SQUARE MA */
                  case 0x3384: outptr = "kA"; break; /* SQUARE KA */
                  case 0x3385: outptr = "KB"; break; /* SQUARE KB */
                  case 0x3386: outptr = "MB"; break; /* SQUARE MB */
                  case 0x3387: outptr = "GB"; break; /* SQUARE GB */
                  case 0x3388: outptr = "cal"; break; /* SQUARE CAL */
                  case 0x3389: outptr = "kcal"; break; /* SQUARE KCAL */
                  case 0x338a: outptr = "pF"; break; /* SQUARE PF */
                  case 0x338b: outptr = "nF"; break; /* SQUARE NF */
                  case 0x338c: outptr = "uF"; break; /* SQUARE MU F */
                  case 0x338d: outptr = "ug"; break; /* SQUARE MU G */
                  case 0x338e: outptr = "mg"; break; /* SQUARE MG */
                  case 0x338f: outptr = "kg"; break; /* SQUARE KG */
                  case 0x3390: outptr = "Hz"; break; /* SQUARE HZ */
                  case 0x3391: outptr = "kHz"; break; /* SQUARE KHZ */
                  case 0x3392: outptr = "MHz"; break; /* SQUARE MHZ */
                  case 0x3393: outptr = "GHz"; break; /* SQUARE GHZ */
                  case 0x3394: outptr = "THz"; break; /* SQUARE THZ */
                  case 0x3396: outptr = "m"; break; /* SQUARE ML */
                  case 0x3397: outptr = "d"; break; /* SQUARE DL */
                  case 0x3398: outptr = "k"; break; /* SQUARE KL */
                  case 0x3399: outptr = "fm"; break; /* SQUARE FM */
                  case 0x339a: outptr = "nm"; break; /* SQUARE NM */
                  case 0x339b: outptr = "m"; break; /* SQUARE MU M */
                  case 0x339c: outptr = "mm"; break; /* SQUARE MM */
                  case 0x339d: outptr = "cm"; break; /* SQUARE CM */
                  case 0x339e: outptr = "km"; break; /* SQUARE KM */
                  case 0x339f: outptr = "mm^2"; break; /* SQUARE MM SQUARED */
                  case 0x33a0: outptr = "cm^2"; break; /* SQUARE CM SQUARED */
                  case 0x33a1: outptr = "m^2"; break; /* SQUARE M SQUARED */
                  case 0x33a2: outptr = "km^2"; break; /* SQUARE KM SQUARED */
                  case 0x33a3: outptr = "mm^3"; break; /* SQUARE MM CUBED */
                  case 0x33a4: outptr = "cm^3"; break; /* SQUARE CM CUBED */
                  case 0x33a5: outptr = "m^3"; break; /* SQUARE M CUBED */
                  case 0x33a6: outptr = "km^3"; break; /* SQUARE KM CUBED */
                  case 0x33a7: outptr = "m/s"; break; /* SQUARE M OVER S */
                  case 0x33a8: outptr = "m/s^2"; break; /* SQUARE M OVER S SQUARED */
                  case 0x33a9: outptr = "Pa"; break; /* SQUARE PA */
                  case 0x33aa: outptr = "kPa"; break; /* SQUARE KPA */
                  case 0x33ab: outptr = "MPa"; break; /* SQUARE MPA */
                  case 0x33ac: outptr = "GPa"; break; /* SQUARE GPA */
                  case 0x33ad: outptr = "rad"; break; /* SQUARE RAD */
                  case 0x33ae: outptr = "rad/s"; break; /* SQUARE RAD OVER S */
                  case 0x33af: outptr = "rad/s^2"; break; /* SQUARE RAD OVER S SQUARED */
                  case 0x33b0: outptr = "ps"; break; /* SQUARE PS */
                  case 0x33b1: outptr = "ns"; break; /* SQUARE NS */
                  case 0x33b2: outptr = "us"; break; /* SQUARE MU S */
                  case 0x33b3: outptr = "ms"; break; /* SQUARE MS */
                  case 0x33b4: outptr = "pV"; break; /* SQUARE PV */
                  case 0x33b5: outptr = "nV"; break; /* SQUARE NV */
                  case 0x33b6: outptr = "uV"; break; /* SQUARE MU V */
                  case 0x33b7: outptr = "mV"; break; /* SQUARE MV */
                  case 0x33b8: outptr = "kV"; break; /* SQUARE KV */
                  case 0x33b9: outptr = "MV"; break; /* SQUARE MV MEGA */
                  case 0x33ba: outptr = "pW"; break; /* SQUARE PW */
                  case 0x33bb: outptr = "nW"; break; /* SQUARE NW */
                  case 0x33bc: outptr = "W"; break; /* SQUARE MU W */
                  case 0x33bd: outptr = "mW"; break; /* SQUARE MW */
                  case 0x33be: outptr = "kW"; break; /* SQUARE KW */
                  case 0x33bf: outptr = "MW"; break; /* SQUARE MW MEGA */
                  case 0x33c0: outptr = "k"; break; /* SQUARE K OHM */
                  case 0x33c1: outptr = "M"; break; /* SQUARE M OHM */
                  case 0x33c2: outptr = "a.m."; break; /* SQUARE AM */
                  case 0x33c3: outptr = "Bq"; break; /* SQUARE BQ */
                  case 0x33c4: outptr = "cc"; break; /* SQUARE CC */
                  case 0x33c5: outptr = "cd"; break; /* SQUARE CD */
                  case 0x33c6: outptr = "C/kg"; break; /* SQUARE C OVER KG */
                  case 0x33c7: outptr = "Co."; break; /* SQUARE CO */
                  case 0x33c8: outptr = "dB"; break; /* SQUARE DB */
                  case 0x33c9: outptr = "Gy"; break; /* SQUARE GY */
                  case 0x33ca: outptr = "ha"; break; /* SQUARE HA */
                  case 0x33cb: outptr = "HP"; break; /* SQUARE HP */
                  case 0x33cc: outptr = "in"; break; /* SQUARE IN */
                  case 0x33cd: outptr = "KK"; break; /* SQUARE KK */
                  case 0x33ce: outptr = "KM"; break; /* SQUARE KM CAPITAL */
                  case 0x33cf: outptr = "kt"; break; /* SQUARE KT */
                  case 0x33d0: outptr = "lm"; break; /* SQUARE LM */
                  case 0x33d1: outptr = "ln"; break; /* SQUARE LN */
                  case 0x33d2: outptr = "log"; break; /* SQUARE LOG */
                  case 0x33d3: outptr = "lx"; break; /* SQUARE LX */
                  case 0x33d4: outptr = "mb"; break; /* SQUARE MB SMALL */
                  case 0x33d5: outptr = "mil"; break; /* SQUARE MIL */
                  case 0x33d6: outptr = "mol"; break; /* SQUARE MOL */
                  case 0x33d7: outptr = "PH"; break; /* SQUARE PH */
                  case 0x33d8: outptr = "p.m."; break; /* SQUARE PM */
                  case 0x33d9: outptr = "PPM"; break; /* SQUARE PPM */
                  case 0x33da: outptr = "PR"; break; /* SQUARE PR */
                  case 0x33db: outptr = "sr"; break; /* SQUARE SR */
                  case 0x33dc: outptr = "Sv"; break; /* SQUARE SV */
                  case 0x33dd: outptr = "Wb"; break; /* SQUARE WB */
                  case 0x33de: outptr = "V/m"; break; /* SQUARE V OVER M */
                  case 0x33df: outptr = "A/m"; break; /* SQUARE A OVER M */
                  case 0x33ff: outptr = "gal"; break; /* SQUARE GAL */
                  case 0xfb00: outptr = "ff"; break; /* LATIN SMALL LIGATURE FF */
                  case 0xfb01: outptr = "fi"; break; /* LATIN SMALL LIGATURE FI */
                  case 0xfb02: outptr = "fl"; break; /* LATIN SMALL LIGATURE FL */
                  case 0xfb03: outptr = "ffi"; break; /* LATIN SMALL LIGATURE FFI */
                  case 0xfb04: outptr = "ffl"; break; /* LATIN SMALL LIGATURE FFL */
                  case 0xfb05: outptr = "t"; break; /* LATIN SMALL LIGATURE LONG S T */
                  case 0xfb06: outptr = "st"; break; /* LATIN SMALL LIGATURE ST */
                  case 0xfe4d: outptr = "_"; break; /* DASHED LOW LINE */
                  case 0xfe4e: outptr = "_"; break; /* CENTRELINE LOW LINE */
                  case 0xfe4f: outptr = "_"; break; /* WAVY LOW LINE */
                  case 0xfe50: outptr = ","; break; /* SMALL COMMA */
                  case 0xfe52: outptr = "."; break; /* SMALL FULL STOP */
                  case 0xfe54: outptr = ";"; break; /* SMALL SEMICOLON */
                  case 0xfe55: outptr = ":"; break; /* SMALL COLON */
                  case 0xfe56: outptr = "?"; break; /* SMALL QUESTION MARK */
                  case 0xfe57: outptr = "!"; break; /* SMALL EXCLAMATION MARK */
                  case 0xfe59: outptr = "("; break; /* SMALL LEFT PARENTHESIS */
                  case 0xfe5a: outptr = ")"; break; /* SMALL RIGHT PARENTHESIS */
                  case 0xfe5b: outptr = "{"; break; /* SMALL LEFT CURLY BRACKET */
                  case 0xfe5c: outptr = "}"; break; /* SMALL RIGHT CURLY BRACKET */
                  case 0xfe5f: outptr = "#"; break; /* SMALL NUMBER SIGN */
                  case 0xfe60: outptr = "&"; break; /* SMALL AMPERSAND */
                  case 0xfe61: outptr = "*"; break; /* SMALL ASTERISK */
                  case 0xfe62: outptr = "+"; break; /* SMALL PLUS SIGN */
                  case 0xfe63: outptr = "-"; break; /* SMALL HYPHEN-MINUS */
                  case 0xfe64: outptr = "<"; break; /* SMALL LESS-THAN SIGN */
                  case 0xfe65: outptr = ">"; break; /* SMALL GREATER-THAN SIGN */
                  case 0xfe66: outptr = "="; break; /* SMALL EQUALS SIGN */
                  case 0xfe68: outptr = "\\"; break; /* SMALL REVERSE SOLIDUS */
                  case 0xfe69: outptr = "$"; break; /* SMALL DOLLAR SIGN */
                  case 0xfe6a: outptr = "%"; break; /* SMALL PERCENT SIGN */
                  case 0xfe6b: outptr = "@"; break; /* SMALL COMMERCIAL AT */
                  case 0xff01: outptr = "!"; break; /* FULLWIDTH EXCLAMATION MARK */
                  case 0xff02: outptr = "\""; break; /* FULLWIDTH QUOTATION MARK */
                  case 0xff03: outptr = "#"; break; /* FULLWIDTH NUMBER SIGN */
                  case 0xff04: outptr = "$"; break; /* FULLWIDTH DOLLAR SIGN */
                  case 0xff05: outptr = "%"; break; /* FULLWIDTH PERCENT SIGN */
                  case 0xff06: outptr = "&"; break; /* FULLWIDTH AMPERSAND */
                  case 0xff07: outptr = "'"; break; /* FULLWIDTH APOSTROPHE */
                  case 0xff08: outptr = "("; break; /* FULLWIDTH LEFT PARENTHESIS */
                  case 0xff09: outptr = ")"; break; /* FULLWIDTH RIGHT PARENTHESIS */
                  case 0xff0a: outptr = "*"; break; /* FULLWIDTH ASTERISK */
                  case 0xff0b: outptr = "+"; break; /* FULLWIDTH PLUS SIGN */
                  case 0xff0c: outptr = ","; break; /* FULLWIDTH COMMA */
                  case 0xff0d: outptr = "-"; break; /* FULLWIDTH HYPHEN-MINUS */
                  case 0xff0e: outptr = "."; break; /* FULLWIDTH FULL STOP */
                  case 0xff0f: outptr = "/"; break; /* FULLWIDTH SOLIDUS */
                  case 0xff10: outptr = "0"; break; /* FULLWIDTH DIGIT ZERO */
                  case 0xff11: outptr = "1"; break; /* FULLWIDTH DIGIT ONE */
                  case 0xff12: outptr = "2"; break; /* FULLWIDTH DIGIT TWO */
                  case 0xff13: outptr = "3"; break; /* FULLWIDTH DIGIT THREE */
                  case 0xff14: outptr = "4"; break; /* FULLWIDTH DIGIT FOUR */
                  case 0xff15: outptr = "5"; break; /* FULLWIDTH DIGIT FIVE */
                  case 0xff16: outptr = "6"; break; /* FULLWIDTH DIGIT SIX */
                  case 0xff17: outptr = "7"; break; /* FULLWIDTH DIGIT SEVEN */
                  case 0xff18: outptr = "8"; break; /* FULLWIDTH DIGIT EIGHT */
                  case 0xff19: outptr = "9"; break; /* FULLWIDTH DIGIT NINE */
                  case 0xff1a: outptr = ":"; break; /* FULLWIDTH COLON */
                  case 0xff1b: outptr = ";"; break; /* FULLWIDTH SEMICOLON */
                  case 0xff1c: outptr = "<"; break; /* FULLWIDTH LESS-THAN SIGN */
                  case 0xff1d: outptr = "="; break; /* FULLWIDTH EQUALS SIGN */
                  case 0xff1e: outptr = ">"; break; /* FULLWIDTH GREATER-THAN SIGN */
                  case 0xff1f: outptr = "?"; break; /* FULLWIDTH QUESTION MARK */
                  case 0xff20: outptr = "@"; break; /* FULLWIDTH COMMERCIAL AT */
                  case 0xff21: outptr = "A"; break; /* FULLWIDTH LATIN CAPITAL LETTER A */
                  case 0xff22: outptr = "B"; break; /* FULLWIDTH LATIN CAPITAL LETTER B */
                  case 0xff23: outptr = "C"; break; /* FULLWIDTH LATIN CAPITAL LETTER C */
                  case 0xff24: outptr = "D"; break; /* FULLWIDTH LATIN CAPITAL LETTER D */
                  case 0xff25: outptr = "E"; break; /* FULLWIDTH LATIN CAPITAL LETTER E */
                  case 0xff26: outptr = "F"; break; /* FULLWIDTH LATIN CAPITAL LETTER F */
                  case 0xff27: outptr = "G"; break; /* FULLWIDTH LATIN CAPITAL LETTER G */
                  case 0xff28: outptr = "H"; break; /* FULLWIDTH LATIN CAPITAL LETTER H */
                  case 0xff29: outptr = "I"; break; /* FULLWIDTH LATIN CAPITAL LETTER I */
                  case 0xff2a: outptr = "J"; break; /* FULLWIDTH LATIN CAPITAL LETTER J */
                  case 0xff2b: outptr = "K"; break; /* FULLWIDTH LATIN CAPITAL LETTER K */
                  case 0xff2c: outptr = "L"; break; /* FULLWIDTH LATIN CAPITAL LETTER L */
                  case 0xff2d: outptr = "M"; break; /* FULLWIDTH LATIN CAPITAL LETTER M */
                  case 0xff2e: outptr = "N"; break; /* FULLWIDTH LATIN CAPITAL LETTER N */
                  case 0xff2f: outptr = "O"; break; /* FULLWIDTH LATIN CAPITAL LETTER O */
                  case 0xff30: outptr = "P"; break; /* FULLWIDTH LATIN CAPITAL LETTER P */
                  case 0xff31: outptr = "Q"; break; /* FULLWIDTH LATIN CAPITAL LETTER Q */
                  case 0xff32: outptr = "R"; break; /* FULLWIDTH LATIN CAPITAL LETTER R */
                  case 0xff33: outptr = "S"; break; /* FULLWIDTH LATIN CAPITAL LETTER S */
                  case 0xff34: outptr = "T"; break; /* FULLWIDTH LATIN CAPITAL LETTER T */
                  case 0xff35: outptr = "U"; break; /* FULLWIDTH LATIN CAPITAL LETTER U */
                  case 0xff36: outptr = "V"; break; /* FULLWIDTH LATIN CAPITAL LETTER V */
                  case 0xff37: outptr = "W"; break; /* FULLWIDTH LATIN CAPITAL LETTER W */
                  case 0xff38: outptr = "X"; break; /* FULLWIDTH LATIN CAPITAL LETTER X */
                  case 0xff39: outptr = "Y"; break; /* FULLWIDTH LATIN CAPITAL LETTER Y */
                  case 0xff3a: outptr = "Z"; break; /* FULLWIDTH LATIN CAPITAL LETTER Z */
                  case 0xff3b: outptr = "["; break; /* FULLWIDTH LEFT SQUARE BRACKET */
                  case 0xff3c: outptr = "\\"; break; /* FULLWIDTH REVERSE SOLIDUS */
                  case 0xff3d: outptr = "]"; break; /* FULLWIDTH RIGHT SQUARE BRACKET */
                  case 0xff3e: outptr = "^"; break; /* FULLWIDTH CIRCUMFLEX ACCENT */
                  case 0xff3f: outptr = "_"; break; /* FULLWIDTH LOW LINE */
                  case 0xff40: outptr = "`"; break; /* FULLWIDTH GRAVE ACCENT */
                  case 0xff41: outptr = "a"; break; /* FULLWIDTH LATIN SMALL LETTER A */
                  case 0xff42: outptr = "b"; break; /* FULLWIDTH LATIN SMALL LETTER B */
                  case 0xff43: outptr = "c"; break; /* FULLWIDTH LATIN SMALL LETTER C */
                  case 0xff44: outptr = "d"; break; /* FULLWIDTH LATIN SMALL LETTER D */
                  case 0xff45: outptr = "e"; break; /* FULLWIDTH LATIN SMALL LETTER E */
                  case 0xff46: outptr = "f"; break; /* FULLWIDTH LATIN SMALL LETTER F */
                  case 0xff47: outptr = "g"; break; /* FULLWIDTH LATIN SMALL LETTER G */
                  case 0xff48: outptr = "h"; break; /* FULLWIDTH LATIN SMALL LETTER H */
                  case 0xff49: outptr = "i"; break; /* FULLWIDTH LATIN SMALL LETTER I */
                  case 0xff4a: outptr = "j"; break; /* FULLWIDTH LATIN SMALL LETTER J */
                  case 0xff4b: outptr = "k"; break; /* FULLWIDTH LATIN SMALL LETTER K */
                  case 0xff4c: outptr = "l"; break; /* FULLWIDTH LATIN SMALL LETTER L */
                  case 0xff4d: outptr = "m"; break; /* FULLWIDTH LATIN SMALL LETTER M */
                  case 0xff4e: outptr = "n"; break; /* FULLWIDTH LATIN SMALL LETTER N */
                  case 0xff4f: outptr = "o"; break; /* FULLWIDTH LATIN SMALL LETTER O */
                  case 0xff50: outptr = "p"; break; /* FULLWIDTH LATIN SMALL LETTER P */
                  case 0xff51: outptr = "q"; break; /* FULLWIDTH LATIN SMALL LETTER Q */
                  case 0xff52: outptr = "r"; break; /* FULLWIDTH LATIN SMALL LETTER R */
                  case 0xff53: outptr = "s"; break; /* FULLWIDTH LATIN SMALL LETTER S */
                  case 0xff54: outptr = "t"; break; /* FULLWIDTH LATIN SMALL LETTER T */
                  case 0xff55: outptr = "u"; break; /* FULLWIDTH LATIN SMALL LETTER U */
                  case 0xff56: outptr = "v"; break; /* FULLWIDTH LATIN SMALL LETTER V */
                  case 0xff57: outptr = "w"; break; /* FULLWIDTH LATIN SMALL LETTER W */
                  case 0xff58: outptr = "x"; break; /* FULLWIDTH LATIN SMALL LETTER X */
                  case 0xff59: outptr = "y"; break; /* FULLWIDTH LATIN SMALL LETTER Y */
                  case 0xff5a: outptr = "z"; break; /* FULLWIDTH LATIN SMALL LETTER Z */
                  case 0xff5b: outptr = "{"; break; /* FULLWIDTH LEFT CURLY BRACKET */
                  case 0xff5c: outptr = "|"; break; /* FULLWIDTH VERTICAL LINE */
                  case 0xff5d: outptr = "}"; break; /* FULLWIDTH RIGHT CURLY BRACKET */
                  case 0xff5e: outptr = "~"; break; /* FULLWIDTH TILDE */
                  case 0x1d400: outptr = "A"; break; /* MATHEMATICAL BOLD CAPITAL A */
                  case 0x1d401: outptr = "B"; break; /* MATHEMATICAL BOLD CAPITAL B */
                  case 0x1d402: outptr = "C"; break; /* MATHEMATICAL BOLD CAPITAL C */
                  case 0x1d403: outptr = "D"; break; /* MATHEMATICAL BOLD CAPITAL D */
                  case 0x1d404: outptr = "E"; break; /* MATHEMATICAL BOLD CAPITAL E */
                  case 0x1d405: outptr = "F"; break; /* MATHEMATICAL BOLD CAPITAL F */
                  case 0x1d406: outptr = "G"; break; /* MATHEMATICAL BOLD CAPITAL G */
                  case 0x1d407: outptr = "H"; break; /* MATHEMATICAL BOLD CAPITAL H */
                  case 0x1d408: outptr = "I"; break; /* MATHEMATICAL BOLD CAPITAL I */
                  case 0x1d409: outptr = "J"; break; /* MATHEMATICAL BOLD CAPITAL J */
                  case 0x1d40a: outptr = "K"; break; /* MATHEMATICAL BOLD CAPITAL K */
                  case 0x1d40b: outptr = "L"; break; /* MATHEMATICAL BOLD CAPITAL L */
                  case 0x1d40c: outptr = "M"; break; /* MATHEMATICAL BOLD CAPITAL M */
                  case 0x1d40d: outptr = "N"; break; /* MATHEMATICAL BOLD CAPITAL N */
                  case 0x1d40e: outptr = "O"; break; /* MATHEMATICAL BOLD CAPITAL O */
                  case 0x1d40f: outptr = "P"; break; /* MATHEMATICAL BOLD CAPITAL P */
                  case 0x1d410: outptr = "Q"; break; /* MATHEMATICAL BOLD CAPITAL Q */
                  case 0x1d411: outptr = "R"; break; /* MATHEMATICAL BOLD CAPITAL R */
                  case 0x1d412: outptr = "S"; break; /* MATHEMATICAL BOLD CAPITAL S */
                  case 0x1d413: outptr = "T"; break; /* MATHEMATICAL BOLD CAPITAL T */
                  case 0x1d414: outptr = "U"; break; /* MATHEMATICAL BOLD CAPITAL U */
                  case 0x1d415: outptr = "V"; break; /* MATHEMATICAL BOLD CAPITAL V */
                  case 0x1d416: outptr = "W"; break; /* MATHEMATICAL BOLD CAPITAL W */
                  case 0x1d417: outptr = "X"; break; /* MATHEMATICAL BOLD CAPITAL X */
                  case 0x1d418: outptr = "Y"; break; /* MATHEMATICAL BOLD CAPITAL Y */
                  case 0x1d419: outptr = "Z"; break; /* MATHEMATICAL BOLD CAPITAL Z */
                  case 0x1d41a: outptr = "a"; break; /* MATHEMATICAL BOLD SMALL A */
                  case 0x1d41b: outptr = "b"; break; /* MATHEMATICAL BOLD SMALL B */
                  case 0x1d41c: outptr = "c"; break; /* MATHEMATICAL BOLD SMALL C */
                  case 0x1d41d: outptr = "d"; break; /* MATHEMATICAL BOLD SMALL D */
                  case 0x1d41e: outptr = "e"; break; /* MATHEMATICAL BOLD SMALL E */
                  case 0x1d41f: outptr = "f"; break; /* MATHEMATICAL BOLD SMALL F */
                  case 0x1d420: outptr = "g"; break; /* MATHEMATICAL BOLD SMALL G */
                  case 0x1d421: outptr = "h"; break; /* MATHEMATICAL BOLD SMALL H */
                  case 0x1d422: outptr = "i"; break; /* MATHEMATICAL BOLD SMALL I */
                  case 0x1d423: outptr = "j"; break; /* MATHEMATICAL BOLD SMALL J */
                  case 0x1d424: outptr = "k"; break; /* MATHEMATICAL BOLD SMALL K */
                  case 0x1d425: outptr = "l"; break; /* MATHEMATICAL BOLD SMALL L */
                  case 0x1d426: outptr = "m"; break; /* MATHEMATICAL BOLD SMALL M */
                  case 0x1d427: outptr = "n"; break; /* MATHEMATICAL BOLD SMALL N */
                  case 0x1d428: outptr = "o"; break; /* MATHEMATICAL BOLD SMALL O */
                  case 0x1d429: outptr = "p"; break; /* MATHEMATICAL BOLD SMALL P */
                  case 0x1d42a: outptr = "q"; break; /* MATHEMATICAL BOLD SMALL Q */
                  case 0x1d42b: outptr = "r"; break; /* MATHEMATICAL BOLD SMALL R */
                  case 0x1d42c: outptr = "s"; break; /* MATHEMATICAL BOLD SMALL S */
                  case 0x1d42d: outptr = "t"; break; /* MATHEMATICAL BOLD SMALL T */
                  case 0x1d42e: outptr = "u"; break; /* MATHEMATICAL BOLD SMALL U */
                  case 0x1d42f: outptr = "v"; break; /* MATHEMATICAL BOLD SMALL V */
                  case 0x1d430: outptr = "w"; break; /* MATHEMATICAL BOLD SMALL W */
                  case 0x1d431: outptr = "x"; break; /* MATHEMATICAL BOLD SMALL X */
                  case 0x1d432: outptr = "y"; break; /* MATHEMATICAL BOLD SMALL Y */
                  case 0x1d433: outptr = "z"; break; /* MATHEMATICAL BOLD SMALL Z */
                  case 0x1d434: outptr = "A"; break; /* MATHEMATICAL ITALIC CAPITAL A */
                  case 0x1d435: outptr = "B"; break; /* MATHEMATICAL ITALIC CAPITAL B */
                  case 0x1d436: outptr = "C"; break; /* MATHEMATICAL ITALIC CAPITAL C */
                  case 0x1d437: outptr = "D"; break; /* MATHEMATICAL ITALIC CAPITAL D */
                  case 0x1d438: outptr = "E"; break; /* MATHEMATICAL ITALIC CAPITAL E */
                  case 0x1d439: outptr = "F"; break; /* MATHEMATICAL ITALIC CAPITAL F */
                  case 0x1d43a: outptr = "G"; break; /* MATHEMATICAL ITALIC CAPITAL G */
                  case 0x1d43b: outptr = "H"; break; /* MATHEMATICAL ITALIC CAPITAL H */
                  case 0x1d43c: outptr = "I"; break; /* MATHEMATICAL ITALIC CAPITAL I */
                  case 0x1d43d: outptr = "J"; break; /* MATHEMATICAL ITALIC CAPITAL J */
                  case 0x1d43e: outptr = "K"; break; /* MATHEMATICAL ITALIC CAPITAL K */
                  case 0x1d43f: outptr = "L"; break; /* MATHEMATICAL ITALIC CAPITAL L */
                  case 0x1d440: outptr = "M"; break; /* MATHEMATICAL ITALIC CAPITAL M */
                  case 0x1d441: outptr = "N"; break; /* MATHEMATICAL ITALIC CAPITAL N */
                  case 0x1d442: outptr = "O"; break; /* MATHEMATICAL ITALIC CAPITAL O */
                  case 0x1d443: outptr = "P"; break; /* MATHEMATICAL ITALIC CAPITAL P */
                  case 0x1d444: outptr = "Q"; break; /* MATHEMATICAL ITALIC CAPITAL Q */
                  case 0x1d445: outptr = "R"; break; /* MATHEMATICAL ITALIC CAPITAL R */
                  case 0x1d446: outptr = "S"; break; /* MATHEMATICAL ITALIC CAPITAL S */
                  case 0x1d447: outptr = "T"; break; /* MATHEMATICAL ITALIC CAPITAL T */
                  case 0x1d448: outptr = "U"; break; /* MATHEMATICAL ITALIC CAPITAL U */
                  case 0x1d449: outptr = "V"; break; /* MATHEMATICAL ITALIC CAPITAL V */
                  case 0x1d44a: outptr = "W"; break; /* MATHEMATICAL ITALIC CAPITAL W */
                  case 0x1d44b: outptr = "X"; break; /* MATHEMATICAL ITALIC CAPITAL X */
                  case 0x1d44c: outptr = "Y"; break; /* MATHEMATICAL ITALIC CAPITAL Y */
                  case 0x1d44d: outptr = "Z"; break; /* MATHEMATICAL ITALIC CAPITAL Z */
                  case 0x1d44e: outptr = "a"; break; /* MATHEMATICAL ITALIC SMALL A */
                  case 0x1d44f: outptr = "b"; break; /* MATHEMATICAL ITALIC SMALL B */
                  case 0x1d450: outptr = "c"; break; /* MATHEMATICAL ITALIC SMALL C */
                  case 0x1d451: outptr = "d"; break; /* MATHEMATICAL ITALIC SMALL D */
                  case 0x1d452: outptr = "e"; break; /* MATHEMATICAL ITALIC SMALL E */
                  case 0x1d453: outptr = "f"; break; /* MATHEMATICAL ITALIC SMALL F */
                  case 0x1d454: outptr = "g"; break; /* MATHEMATICAL ITALIC SMALL G */
                  case 0x1d456: outptr = "i"; break; /* MATHEMATICAL ITALIC SMALL I */
                  case 0x1d457: outptr = "j"; break; /* MATHEMATICAL ITALIC SMALL J */
                  case 0x1d458: outptr = "k"; break; /* MATHEMATICAL ITALIC SMALL K */
                  case 0x1d459: outptr = "l"; break; /* MATHEMATICAL ITALIC SMALL L */
                  case 0x1d45a: outptr = "m"; break; /* MATHEMATICAL ITALIC SMALL M */
                  case 0x1d45b: outptr = "n"; break; /* MATHEMATICAL ITALIC SMALL N */
                  case 0x1d45c: outptr = "o"; break; /* MATHEMATICAL ITALIC SMALL O */
                  case 0x1d45d: outptr = "p"; break; /* MATHEMATICAL ITALIC SMALL P */
                  case 0x1d45e: outptr = "q"; break; /* MATHEMATICAL ITALIC SMALL Q */
                  case 0x1d45f: outptr = "r"; break; /* MATHEMATICAL ITALIC SMALL R */
                  case 0x1d460: outptr = "s"; break; /* MATHEMATICAL ITALIC SMALL S */
                  case 0x1d461: outptr = "t"; break; /* MATHEMATICAL ITALIC SMALL T */
                  case 0x1d462: outptr = "u"; break; /* MATHEMATICAL ITALIC SMALL U */
                  case 0x1d463: outptr = "v"; break; /* MATHEMATICAL ITALIC SMALL V */
                  case 0x1d464: outptr = "w"; break; /* MATHEMATICAL ITALIC SMALL W */
                  case 0x1d465: outptr = "x"; break; /* MATHEMATICAL ITALIC SMALL X */
                  case 0x1d466: outptr = "y"; break; /* MATHEMATICAL ITALIC SMALL Y */
                  case 0x1d467: outptr = "z"; break; /* MATHEMATICAL ITALIC SMALL Z */
                  case 0x1d468: outptr = "A"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL A */
                  case 0x1d469: outptr = "B"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL B */
                  case 0x1d46a: outptr = "C"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL C */
                  case 0x1d46b: outptr = "D"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL D */
                  case 0x1d46c: outptr = "E"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL E */
                  case 0x1d46d: outptr = "F"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL F */
                  case 0x1d46e: outptr = "G"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL G */
                  case 0x1d46f: outptr = "H"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL H */
                  case 0x1d470: outptr = "I"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL I */
                  case 0x1d471: outptr = "J"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL J */
                  case 0x1d472: outptr = "K"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL K */
                  case 0x1d473: outptr = "L"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL L */
                  case 0x1d474: outptr = "M"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL M */
                  case 0x1d475: outptr = "N"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL N */
                  case 0x1d476: outptr = "O"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL O */
                  case 0x1d477: outptr = "P"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL P */
                  case 0x1d478: outptr = "Q"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL Q */
                  case 0x1d479: outptr = "R"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL R */
                  case 0x1d47a: outptr = "S"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL S */
                  case 0x1d47b: outptr = "T"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL T */
                  case 0x1d47c: outptr = "U"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL U */
                  case 0x1d47d: outptr = "V"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL V */
                  case 0x1d47e: outptr = "W"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL W */
                  case 0x1d47f: outptr = "X"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL X */
                  case 0x1d480: outptr = "Y"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL Y */
                  case 0x1d481: outptr = "Z"; break; /* MATHEMATICAL BOLD ITALIC CAPITAL Z */
                  case 0x1d482: outptr = "a"; break; /* MATHEMATICAL BOLD ITALIC SMALL A */
                  case 0x1d483: outptr = "b"; break; /* MATHEMATICAL BOLD ITALIC SMALL B */
                  case 0x1d484: outptr = "c"; break; /* MATHEMATICAL BOLD ITALIC SMALL C */
                  case 0x1d485: outptr = "d"; break; /* MATHEMATICAL BOLD ITALIC SMALL D */
                  case 0x1d486: outptr = "e"; break; /* MATHEMATICAL BOLD ITALIC SMALL E */
                  case 0x1d487: outptr = "f"; break; /* MATHEMATICAL BOLD ITALIC SMALL F */
                  case 0x1d488: outptr = "g"; break; /* MATHEMATICAL BOLD ITALIC SMALL G */
                  case 0x1d489: outptr = "h"; break; /* MATHEMATICAL BOLD ITALIC SMALL H */
                  case 0x1d48a: outptr = "i"; break; /* MATHEMATICAL BOLD ITALIC SMALL I */
                  case 0x1d48b: outptr = "j"; break; /* MATHEMATICAL BOLD ITALIC SMALL J */
                  case 0x1d48c: outptr = "k"; break; /* MATHEMATICAL BOLD ITALIC SMALL K */
                  case 0x1d48d: outptr = "l"; break; /* MATHEMATICAL BOLD ITALIC SMALL L */
                  case 0x1d48e: outptr = "m"; break; /* MATHEMATICAL BOLD ITALIC SMALL M */
                  case 0x1d48f: outptr = "n"; break; /* MATHEMATICAL BOLD ITALIC SMALL N */
                  case 0x1d490: outptr = "o"; break; /* MATHEMATICAL BOLD ITALIC SMALL O */
                  case 0x1d491: outptr = "p"; break; /* MATHEMATICAL BOLD ITALIC SMALL P */
                  case 0x1d492: outptr = "q"; break; /* MATHEMATICAL BOLD ITALIC SMALL Q */
                  case 0x1d493: outptr = "r"; break; /* MATHEMATICAL BOLD ITALIC SMALL R */
                  case 0x1d494: outptr = "s"; break; /* MATHEMATICAL BOLD ITALIC SMALL S */
                  case 0x1d495: outptr = "t"; break; /* MATHEMATICAL BOLD ITALIC SMALL T */
                  case 0x1d496: outptr = "u"; break; /* MATHEMATICAL BOLD ITALIC SMALL U */
                  case 0x1d497: outptr = "v"; break; /* MATHEMATICAL BOLD ITALIC SMALL V */
                  case 0x1d498: outptr = "w"; break; /* MATHEMATICAL BOLD ITALIC SMALL W */
                  case 0x1d499: outptr = "x"; break; /* MATHEMATICAL BOLD ITALIC SMALL X */
                  case 0x1d49a: outptr = "y"; break; /* MATHEMATICAL BOLD ITALIC SMALL Y */
                  case 0x1d49b: outptr = "z"; break; /* MATHEMATICAL BOLD ITALIC SMALL Z */
                  case 0x1d49c: outptr = "A"; break; /* MATHEMATICAL SCRIPT CAPITAL A */
                  case 0x1d49e: outptr = "C"; break; /* MATHEMATICAL SCRIPT CAPITAL C */
                  case 0x1d49f: outptr = "D"; break; /* MATHEMATICAL SCRIPT CAPITAL D */
                  case 0x1d4a2: outptr = "G"; break; /* MATHEMATICAL SCRIPT CAPITAL G */
                  case 0x1d4a5: outptr = "J"; break; /* MATHEMATICAL SCRIPT CAPITAL J */
                  case 0x1d4a6: outptr = "K"; break; /* MATHEMATICAL SCRIPT CAPITAL K */
                  case 0x1d4a9: outptr = "N"; break; /* MATHEMATICAL SCRIPT CAPITAL N */
                  case 0x1d4aa: outptr = "O"; break; /* MATHEMATICAL SCRIPT CAPITAL O */
                  case 0x1d4ab: outptr = "P"; break; /* MATHEMATICAL SCRIPT CAPITAL P */
                  case 0x1d4ac: outptr = "Q"; break; /* MATHEMATICAL SCRIPT CAPITAL Q */
                  case 0x1d4ae: outptr = "S"; break; /* MATHEMATICAL SCRIPT CAPITAL S */
                  case 0x1d4af: outptr = "T"; break; /* MATHEMATICAL SCRIPT CAPITAL T */
                  case 0x1d4b0: outptr = "U"; break; /* MATHEMATICAL SCRIPT CAPITAL U */
                  case 0x1d4b1: outptr = "V"; break; /* MATHEMATICAL SCRIPT CAPITAL V */
                  case 0x1d4b2: outptr = "W"; break; /* MATHEMATICAL SCRIPT CAPITAL W */
                  case 0x1d4b3: outptr = "X"; break; /* MATHEMATICAL SCRIPT CAPITAL X */
                  case 0x1d4b4: outptr = "Y"; break; /* MATHEMATICAL SCRIPT CAPITAL Y */
                  case 0x1d4b5: outptr = "Z"; break; /* MATHEMATICAL SCRIPT CAPITAL Z */
                  case 0x1d4b6: outptr = "a"; break; /* MATHEMATICAL SCRIPT SMALL A */
                  case 0x1d4b7: outptr = "b"; break; /* MATHEMATICAL SCRIPT SMALL B */
                  case 0x1d4b8: outptr = "c"; break; /* MATHEMATICAL SCRIPT SMALL C */
                  case 0x1d4b9: outptr = "d"; break; /* MATHEMATICAL SCRIPT SMALL D */
                  case 0x1d4bb: outptr = "f"; break; /* MATHEMATICAL SCRIPT SMALL F */
                  case 0x1d4bd: outptr = "h"; break; /* MATHEMATICAL SCRIPT SMALL H */
                  case 0x1d4be: outptr = "i"; break; /* MATHEMATICAL SCRIPT SMALL I */
                  case 0x1d4bf: outptr = "j"; break; /* MATHEMATICAL SCRIPT SMALL J */
                  case 0x1d4c0: outptr = "k"; break; /* MATHEMATICAL SCRIPT SMALL K */
                  case 0x1d4c1: outptr = "l"; break; /* MATHEMATICAL SCRIPT SMALL L */
                  case 0x1d4c2: outptr = "m"; break; /* MATHEMATICAL SCRIPT SMALL M */
                  case 0x1d4c3: outptr = "n"; break; /* MATHEMATICAL SCRIPT SMALL N */
                  case 0x1d4c5: outptr = "p"; break; /* MATHEMATICAL SCRIPT SMALL P */
                  case 0x1d4c6: outptr = "q"; break; /* MATHEMATICAL SCRIPT SMALL Q */
                  case 0x1d4c7: outptr = "r"; break; /* MATHEMATICAL SCRIPT SMALL R */
                  case 0x1d4c8: outptr = "s"; break; /* MATHEMATICAL SCRIPT SMALL S */
                  case 0x1d4c9: outptr = "t"; break; /* MATHEMATICAL SCRIPT SMALL T */
                  case 0x1d4ca: outptr = "u"; break; /* MATHEMATICAL SCRIPT SMALL U */
                  case 0x1d4cb: outptr = "v"; break; /* MATHEMATICAL SCRIPT SMALL V */
                  case 0x1d4cc: outptr = "w"; break; /* MATHEMATICAL SCRIPT SMALL W */
                  case 0x1d4cd: outptr = "x"; break; /* MATHEMATICAL SCRIPT SMALL X */
                  case 0x1d4ce: outptr = "y"; break; /* MATHEMATICAL SCRIPT SMALL Y */
                  case 0x1d4cf: outptr = "z"; break; /* MATHEMATICAL SCRIPT SMALL Z */
                  case 0x1d4d0: outptr = "A"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL A */
                  case 0x1d4d1: outptr = "B"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL B */
                  case 0x1d4d2: outptr = "C"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL C */
                  case 0x1d4d3: outptr = "D"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL D */
                  case 0x1d4d4: outptr = "E"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL E */
                  case 0x1d4d5: outptr = "F"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL F */
                  case 0x1d4d6: outptr = "G"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL G */
                  case 0x1d4d7: outptr = "H"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL H */
                  case 0x1d4d8: outptr = "I"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL I */
                  case 0x1d4d9: outptr = "J"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL J */
                  case 0x1d4da: outptr = "K"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL K */
                  case 0x1d4db: outptr = "L"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL L */
                  case 0x1d4dc: outptr = "M"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL M */
                  case 0x1d4dd: outptr = "N"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL N */
                  case 0x1d4de: outptr = "O"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL O */
                  case 0x1d4df: outptr = "P"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL P */
                  case 0x1d4e0: outptr = "Q"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL Q */
                  case 0x1d4e1: outptr = "R"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL R */
                  case 0x1d4e2: outptr = "S"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL S */
                  case 0x1d4e3: outptr = "T"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL T */
                  case 0x1d4e4: outptr = "U"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL U */
                  case 0x1d4e5: outptr = "V"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL V */
                  case 0x1d4e6: outptr = "W"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL W */
                  case 0x1d4e7: outptr = "X"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL X */
                  case 0x1d4e8: outptr = "Y"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL Y */
                  case 0x1d4e9: outptr = "Z"; break; /* MATHEMATICAL BOLD SCRIPT CAPITAL Z */
                  case 0x1d4ea: outptr = "a"; break; /* MATHEMATICAL BOLD SCRIPT SMALL A */
                  case 0x1d4eb: outptr = "b"; break; /* MATHEMATICAL BOLD SCRIPT SMALL B */
                  case 0x1d4ec: outptr = "c"; break; /* MATHEMATICAL BOLD SCRIPT SMALL C */
                  case 0x1d4ed: outptr = "d"; break; /* MATHEMATICAL BOLD SCRIPT SMALL D */
                  case 0x1d4ee: outptr = "e"; break; /* MATHEMATICAL BOLD SCRIPT SMALL E */
                  case 0x1d4ef: outptr = "f"; break; /* MATHEMATICAL BOLD SCRIPT SMALL F */
                  case 0x1d4f0: outptr = "g"; break; /* MATHEMATICAL BOLD SCRIPT SMALL G */
                  case 0x1d4f1: outptr = "h"; break; /* MATHEMATICAL BOLD SCRIPT SMALL H */
                  case 0x1d4f2: outptr = "i"; break; /* MATHEMATICAL BOLD SCRIPT SMALL I */
                  case 0x1d4f3: outptr = "j"; break; /* MATHEMATICAL BOLD SCRIPT SMALL J */
                  case 0x1d4f4: outptr = "k"; break; /* MATHEMATICAL BOLD SCRIPT SMALL K */
                  case 0x1d4f5: outptr = "l"; break; /* MATHEMATICAL BOLD SCRIPT SMALL L */
                  case 0x1d4f6: outptr = "m"; break; /* MATHEMATICAL BOLD SCRIPT SMALL M */
                  case 0x1d4f7: outptr = "n"; break; /* MATHEMATICAL BOLD SCRIPT SMALL N */
                  case 0x1d4f8: outptr = "o"; break; /* MATHEMATICAL BOLD SCRIPT SMALL O */
                  case 0x1d4f9: outptr = "p"; break; /* MATHEMATICAL BOLD SCRIPT SMALL P */
                  case 0x1d4fa: outptr = "q"; break; /* MATHEMATICAL BOLD SCRIPT SMALL Q */
                  case 0x1d4fb: outptr = "r"; break; /* MATHEMATICAL BOLD SCRIPT SMALL R */
                  case 0x1d4fc: outptr = "s"; break; /* MATHEMATICAL BOLD SCRIPT SMALL S */
                  case 0x1d4fd: outptr = "t"; break; /* MATHEMATICAL BOLD SCRIPT SMALL T */
                  case 0x1d4fe: outptr = "u"; break; /* MATHEMATICAL BOLD SCRIPT SMALL U */
                  case 0x1d4ff: outptr = "v"; break; /* MATHEMATICAL BOLD SCRIPT SMALL V */
                  case 0x1d500: outptr = "w"; break; /* MATHEMATICAL BOLD SCRIPT SMALL W */
                  case 0x1d501: outptr = "x"; break; /* MATHEMATICAL BOLD SCRIPT SMALL X */
                  case 0x1d502: outptr = "y"; break; /* MATHEMATICAL BOLD SCRIPT SMALL Y */
                  case 0x1d503: outptr = "z"; break; /* MATHEMATICAL BOLD SCRIPT SMALL Z */
                  case 0x1d504: outptr = "A"; break; /* MATHEMATICAL FRAKTUR CAPITAL A */
                  case 0x1d505: outptr = "B"; break; /* MATHEMATICAL FRAKTUR CAPITAL B */
                  case 0x1d507: outptr = "D"; break; /* MATHEMATICAL FRAKTUR CAPITAL D */
                  case 0x1d508: outptr = "E"; break; /* MATHEMATICAL FRAKTUR CAPITAL E */
                  case 0x1d509: outptr = "F"; break; /* MATHEMATICAL FRAKTUR CAPITAL F */
                  case 0x1d50a: outptr = "G"; break; /* MATHEMATICAL FRAKTUR CAPITAL G */
                  case 0x1d50d: outptr = "J"; break; /* MATHEMATICAL FRAKTUR CAPITAL J */
                  case 0x1d50e: outptr = "K"; break; /* MATHEMATICAL FRAKTUR CAPITAL K */
                  case 0x1d50f: outptr = "L"; break; /* MATHEMATICAL FRAKTUR CAPITAL L */
                  case 0x1d510: outptr = "M"; break; /* MATHEMATICAL FRAKTUR CAPITAL M */
                  case 0x1d511: outptr = "N"; break; /* MATHEMATICAL FRAKTUR CAPITAL N */
                  case 0x1d512: outptr = "O"; break; /* MATHEMATICAL FRAKTUR CAPITAL O */
                  case 0x1d513: outptr = "P"; break; /* MATHEMATICAL FRAKTUR CAPITAL P */
                  case 0x1d514: outptr = "Q"; break; /* MATHEMATICAL FRAKTUR CAPITAL Q */
                  case 0x1d516: outptr = "S"; break; /* MATHEMATICAL FRAKTUR CAPITAL S */
                  case 0x1d517: outptr = "T"; break; /* MATHEMATICAL FRAKTUR CAPITAL T */
                  case 0x1d518: outptr = "U"; break; /* MATHEMATICAL FRAKTUR CAPITAL U */
                  case 0x1d519: outptr = "V"; break; /* MATHEMATICAL FRAKTUR CAPITAL V */
                  case 0x1d51a: outptr = "W"; break; /* MATHEMATICAL FRAKTUR CAPITAL W */
                  case 0x1d51b: outptr = "X"; break; /* MATHEMATICAL FRAKTUR CAPITAL X */
                  case 0x1d51c: outptr = "Y"; break; /* MATHEMATICAL FRAKTUR CAPITAL Y */
                  case 0x1d51e: outptr = "a"; break; /* MATHEMATICAL FRAKTUR SMALL A */
                  case 0x1d51f: outptr = "b"; break; /* MATHEMATICAL FRAKTUR SMALL B */
                  case 0x1d520: outptr = "c"; break; /* MATHEMATICAL FRAKTUR SMALL C */
                  case 0x1d521: outptr = "d"; break; /* MATHEMATICAL FRAKTUR SMALL D */
                  case 0x1d522: outptr = "e"; break; /* MATHEMATICAL FRAKTUR SMALL E */
                  case 0x1d523: outptr = "f"; break; /* MATHEMATICAL FRAKTUR SMALL F */
                  case 0x1d524: outptr = "g"; break; /* MATHEMATICAL FRAKTUR SMALL G */
                  case 0x1d525: outptr = "h"; break; /* MATHEMATICAL FRAKTUR SMALL H */
                  case 0x1d526: outptr = "i"; break; /* MATHEMATICAL FRAKTUR SMALL I */
                  case 0x1d527: outptr = "j"; break; /* MATHEMATICAL FRAKTUR SMALL J */
                  case 0x1d528: outptr = "k"; break; /* MATHEMATICAL FRAKTUR SMALL K */
                  case 0x1d529: outptr = "l"; break; /* MATHEMATICAL FRAKTUR SMALL L */
                  case 0x1d52a: outptr = "m"; break; /* MATHEMATICAL FRAKTUR SMALL M */
                  case 0x1d52b: outptr = "n"; break; /* MATHEMATICAL FRAKTUR SMALL N */
                  case 0x1d52c: outptr = "o"; break; /* MATHEMATICAL FRAKTUR SMALL O */
                  case 0x1d52d: outptr = "p"; break; /* MATHEMATICAL FRAKTUR SMALL P */
                  case 0x1d52e: outptr = "q"; break; /* MATHEMATICAL FRAKTUR SMALL Q */
                  case 0x1d52f: outptr = "r"; break; /* MATHEMATICAL FRAKTUR SMALL R */
                  case 0x1d530: outptr = "s"; break; /* MATHEMATICAL FRAKTUR SMALL S */
                  case 0x1d531: outptr = "t"; break; /* MATHEMATICAL FRAKTUR SMALL T */
                  case 0x1d532: outptr = "u"; break; /* MATHEMATICAL FRAKTUR SMALL U */
                  case 0x1d533: outptr = "v"; break; /* MATHEMATICAL FRAKTUR SMALL V */
                  case 0x1d534: outptr = "w"; break; /* MATHEMATICAL FRAKTUR SMALL W */
                  case 0x1d535: outptr = "x"; break; /* MATHEMATICAL FRAKTUR SMALL X */
                  case 0x1d536: outptr = "y"; break; /* MATHEMATICAL FRAKTUR SMALL Y */
                  case 0x1d537: outptr = "z"; break; /* MATHEMATICAL FRAKTUR SMALL Z */
                  case 0x1d538: outptr = "A"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL A */
                  case 0x1d539: outptr = "B"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL B */
                  case 0x1d53b: outptr = "D"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL D */
                  case 0x1d53c: outptr = "E"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL E */
                  case 0x1d53d: outptr = "F"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL F */
                  case 0x1d53e: outptr = "G"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL G */
                  case 0x1d540: outptr = "I"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL I */
                  case 0x1d541: outptr = "J"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL J */
                  case 0x1d542: outptr = "K"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL K */
                  case 0x1d543: outptr = "L"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL L */
                  case 0x1d544: outptr = "M"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL M */
                  case 0x1d546: outptr = "O"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL O */
                  case 0x1d54a: outptr = "S"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL S */
                  case 0x1d54b: outptr = "T"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL T */
                  case 0x1d54c: outptr = "U"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL U */
                  case 0x1d54d: outptr = "V"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL V */
                  case 0x1d54e: outptr = "W"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL W */
                  case 0x1d54f: outptr = "X"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL X */
                  case 0x1d550: outptr = "Y"; break; /* MATHEMATICAL DOUBLE-STRUCK CAPITAL Y */
                  case 0x1d552: outptr = "a"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL A */
                  case 0x1d553: outptr = "b"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL B */
                  case 0x1d554: outptr = "c"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL C */
                  case 0x1d555: outptr = "d"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL D */
                  case 0x1d556: outptr = "e"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL E */
                  case 0x1d557: outptr = "f"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL F */
                  case 0x1d558: outptr = "g"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL G */
                  case 0x1d559: outptr = "h"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL H */
                  case 0x1d55a: outptr = "i"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL I */
                  case 0x1d55b: outptr = "j"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL J */
                  case 0x1d55c: outptr = "k"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL K */
                  case 0x1d55d: outptr = "l"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL L */
                  case 0x1d55e: outptr = "m"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL M */
                  case 0x1d55f: outptr = "n"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL N */
                  case 0x1d560: outptr = "o"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL O */
                  case 0x1d561: outptr = "p"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL P */
                  case 0x1d562: outptr = "q"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL Q */
                  case 0x1d563: outptr = "r"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL R */
                  case 0x1d564: outptr = "s"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL S */
                  case 0x1d565: outptr = "t"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL T */
                  case 0x1d566: outptr = "u"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL U */
                  case 0x1d567: outptr = "v"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL V */
                  case 0x1d568: outptr = "w"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL W */
                  case 0x1d569: outptr = "x"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL X */
                  case 0x1d56a: outptr = "y"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL Y */
                  case 0x1d56b: outptr = "z"; break; /* MATHEMATICAL DOUBLE-STRUCK SMALL Z */
                  case 0x1d56c: outptr = "A"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL A */
                  case 0x1d56d: outptr = "B"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL B */
                  case 0x1d56e: outptr = "C"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL C */
                  case 0x1d56f: outptr = "D"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL D */
                  case 0x1d570: outptr = "E"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL E */
                  case 0x1d571: outptr = "F"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL F */
                  case 0x1d572: outptr = "G"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL G */
                  case 0x1d573: outptr = "H"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL H */
                  case 0x1d574: outptr = "I"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL I */
                  case 0x1d575: outptr = "J"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL J */
                  case 0x1d576: outptr = "K"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL K */
                  case 0x1d577: outptr = "L"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL L */
                  case 0x1d578: outptr = "M"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL M */
                  case 0x1d579: outptr = "N"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL N */
                  case 0x1d57a: outptr = "O"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL O */
                  case 0x1d57b: outptr = "P"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL P */
                  case 0x1d57c: outptr = "Q"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL Q */
                  case 0x1d57d: outptr = "R"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL R */
                  case 0x1d57e: outptr = "S"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL S */
                  case 0x1d57f: outptr = "T"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL T */
                  case 0x1d580: outptr = "U"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL U */
                  case 0x1d581: outptr = "V"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL V */
                  case 0x1d582: outptr = "W"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL W */
                  case 0x1d583: outptr = "X"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL X */
                  case 0x1d584: outptr = "Y"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL Y */
                  case 0x1d585: outptr = "Z"; break; /* MATHEMATICAL BOLD FRAKTUR CAPITAL Z */
                  case 0x1d586: outptr = "a"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL A */
                  case 0x1d587: outptr = "b"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL B */
                  case 0x1d588: outptr = "c"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL C */
                  case 0x1d589: outptr = "d"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL D */
                  case 0x1d58a: outptr = "e"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL E */
                  case 0x1d58b: outptr = "f"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL F */
                  case 0x1d58c: outptr = "g"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL G */
                  case 0x1d58d: outptr = "h"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL H */
                  case 0x1d58e: outptr = "i"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL I */
                  case 0x1d58f: outptr = "j"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL J */
                  case 0x1d590: outptr = "k"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL K */
                  case 0x1d591: outptr = "l"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL L */
                  case 0x1d592: outptr = "m"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL M */
                  case 0x1d593: outptr = "n"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL N */
                  case 0x1d594: outptr = "o"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL O */
                  case 0x1d595: outptr = "p"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL P */
                  case 0x1d596: outptr = "q"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL Q */
                  case 0x1d597: outptr = "r"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL R */
                  case 0x1d598: outptr = "s"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL S */
                  case 0x1d599: outptr = "t"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL T */
                  case 0x1d59a: outptr = "u"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL U */
                  case 0x1d59b: outptr = "v"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL V */
                  case 0x1d59c: outptr = "w"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL W */
                  case 0x1d59d: outptr = "x"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL X */
                  case 0x1d59e: outptr = "y"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL Y */
                  case 0x1d59f: outptr = "z"; break; /* MATHEMATICAL BOLD FRAKTUR SMALL Z */
                  case 0x1d5a0: outptr = "A"; break; /* MATHEMATICAL SANS-SERIF CAPITAL A */
                  case 0x1d5a1: outptr = "B"; break; /* MATHEMATICAL SANS-SERIF CAPITAL B */
                  case 0x1d5a2: outptr = "C"; break; /* MATHEMATICAL SANS-SERIF CAPITAL C */
                  case 0x1d5a3: outptr = "D"; break; /* MATHEMATICAL SANS-SERIF CAPITAL D */
                  case 0x1d5a4: outptr = "E"; break; /* MATHEMATICAL SANS-SERIF CAPITAL E */
                  case 0x1d5a5: outptr = "F"; break; /* MATHEMATICAL SANS-SERIF CAPITAL F */
                  case 0x1d5a6: outptr = "G"; break; /* MATHEMATICAL SANS-SERIF CAPITAL G */
                  case 0x1d5a7: outptr = "H"; break; /* MATHEMATICAL SANS-SERIF CAPITAL H */
                  case 0x1d5a8: outptr = "I"; break; /* MATHEMATICAL SANS-SERIF CAPITAL I */
                  case 0x1d5a9: outptr = "J"; break; /* MATHEMATICAL SANS-SERIF CAPITAL J */
                  case 0x1d5aa: outptr = "K"; break; /* MATHEMATICAL SANS-SERIF CAPITAL K */
                  case 0x1d5ab: outptr = "L"; break; /* MATHEMATICAL SANS-SERIF CAPITAL L */
                  case 0x1d5ac: outptr = "M"; break; /* MATHEMATICAL SANS-SERIF CAPITAL M */
                  case 0x1d5ad: outptr = "N"; break; /* MATHEMATICAL SANS-SERIF CAPITAL N */
                  case 0x1d5ae: outptr = "O"; break; /* MATHEMATICAL SANS-SERIF CAPITAL O */
                  case 0x1d5af: outptr = "P"; break; /* MATHEMATICAL SANS-SERIF CAPITAL P */
                  case 0x1d5b0: outptr = "Q"; break; /* MATHEMATICAL SANS-SERIF CAPITAL Q */
                  case 0x1d5b1: outptr = "R"; break; /* MATHEMATICAL SANS-SERIF CAPITAL R */
                  case 0x1d5b2: outptr = "S"; break; /* MATHEMATICAL SANS-SERIF CAPITAL S */
                  case 0x1d5b3: outptr = "T"; break; /* MATHEMATICAL SANS-SERIF CAPITAL T */
                  case 0x1d5b4: outptr = "U"; break; /* MATHEMATICAL SANS-SERIF CAPITAL U */
                  case 0x1d5b5: outptr = "V"; break; /* MATHEMATICAL SANS-SERIF CAPITAL V */
                  case 0x1d5b6: outptr = "W"; break; /* MATHEMATICAL SANS-SERIF CAPITAL W */
                  case 0x1d5b7: outptr = "X"; break; /* MATHEMATICAL SANS-SERIF CAPITAL X */
                  case 0x1d5b8: outptr = "Y"; break; /* MATHEMATICAL SANS-SERIF CAPITAL Y */
                  case 0x1d5b9: outptr = "Z"; break; /* MATHEMATICAL SANS-SERIF CAPITAL Z */
                  case 0x1d5ba: outptr = "a"; break; /* MATHEMATICAL SANS-SERIF SMALL A */
                  case 0x1d5bb: outptr = "b"; break; /* MATHEMATICAL SANS-SERIF SMALL B */
                  case 0x1d5bc: outptr = "c"; break; /* MATHEMATICAL SANS-SERIF SMALL C */
                  case 0x1d5bd: outptr = "d"; break; /* MATHEMATICAL SANS-SERIF SMALL D */
                  case 0x1d5be: outptr = "e"; break; /* MATHEMATICAL SANS-SERIF SMALL E */
                  case 0x1d5bf: outptr = "f"; break; /* MATHEMATICAL SANS-SERIF SMALL F */
                  case 0x1d5c0: outptr = "g"; break; /* MATHEMATICAL SANS-SERIF SMALL G */
                  case 0x1d5c1: outptr = "h"; break; /* MATHEMATICAL SANS-SERIF SMALL H */
                  case 0x1d5c2: outptr = "i"; break; /* MATHEMATICAL SANS-SERIF SMALL I */
                  case 0x1d5c3: outptr = "j"; break; /* MATHEMATICAL SANS-SERIF SMALL J */
                  case 0x1d5c4: outptr = "k"; break; /* MATHEMATICAL SANS-SERIF SMALL K */
                  case 0x1d5c5: outptr = "l"; break; /* MATHEMATICAL SANS-SERIF SMALL L */
                  case 0x1d5c6: outptr = "m"; break; /* MATHEMATICAL SANS-SERIF SMALL M */
                  case 0x1d5c7: outptr = "n"; break; /* MATHEMATICAL SANS-SERIF SMALL N */
                  case 0x1d5c8: outptr = "o"; break; /* MATHEMATICAL SANS-SERIF SMALL O */
                  case 0x1d5c9: outptr = "p"; break; /* MATHEMATICAL SANS-SERIF SMALL P */
                  case 0x1d5ca: outptr = "q"; break; /* MATHEMATICAL SANS-SERIF SMALL Q */
                  case 0x1d5cb: outptr = "r"; break; /* MATHEMATICAL SANS-SERIF SMALL R */
                  case 0x1d5cc: outptr = "s"; break; /* MATHEMATICAL SANS-SERIF SMALL S */
                  case 0x1d5cd: outptr = "t"; break; /* MATHEMATICAL SANS-SERIF SMALL T */
                  case 0x1d5ce: outptr = "u"; break; /* MATHEMATICAL SANS-SERIF SMALL U */
                  case 0x1d5cf: outptr = "v"; break; /* MATHEMATICAL SANS-SERIF SMALL V */
                  case 0x1d5d0: outptr = "w"; break; /* MATHEMATICAL SANS-SERIF SMALL W */
                  case 0x1d5d1: outptr = "x"; break; /* MATHEMATICAL SANS-SERIF SMALL X */
                  case 0x1d5d2: outptr = "y"; break; /* MATHEMATICAL SANS-SERIF SMALL Y */
                  case 0x1d5d3: outptr = "z"; break; /* MATHEMATICAL SANS-SERIF SMALL Z */
                  case 0x1d5d4: outptr = "A"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL A */
                  case 0x1d5d5: outptr = "B"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL B */
                  case 0x1d5d6: outptr = "C"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL C */
                  case 0x1d5d7: outptr = "D"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL D */
                  case 0x1d5d8: outptr = "E"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL E */
                  case 0x1d5d9: outptr = "F"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL F */
                  case 0x1d5da: outptr = "G"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL G */
                  case 0x1d5db: outptr = "H"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL H */
                  case 0x1d5dc: outptr = "I"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL I */
                  case 0x1d5dd: outptr = "J"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL J */
                  case 0x1d5de: outptr = "K"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL K */
                  case 0x1d5df: outptr = "L"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL L */
                  case 0x1d5e0: outptr = "M"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL M */
                  case 0x1d5e1: outptr = "N"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL N */
                  case 0x1d5e2: outptr = "O"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL O */
                  case 0x1d5e3: outptr = "P"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL P */
                  case 0x1d5e4: outptr = "Q"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL Q */
                  case 0x1d5e5: outptr = "R"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL R */
                  case 0x1d5e6: outptr = "S"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL S */
                  case 0x1d5e7: outptr = "T"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL T */
                  case 0x1d5e8: outptr = "U"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL U */
                  case 0x1d5e9: outptr = "V"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL V */
                  case 0x1d5ea: outptr = "W"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL W */
                  case 0x1d5eb: outptr = "X"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL X */
                  case 0x1d5ec: outptr = "Y"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL Y */
                  case 0x1d5ed: outptr = "Z"; break; /* MATHEMATICAL SANS-SERIF BOLD CAPITAL Z */
                  case 0x1d5ee: outptr = "a"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL A */
                  case 0x1d5ef: outptr = "b"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL B */
                  case 0x1d5f0: outptr = "c"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL C */
                  case 0x1d5f1: outptr = "d"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL D */
                  case 0x1d5f2: outptr = "e"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL E */
                  case 0x1d5f3: outptr = "f"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL F */
                  case 0x1d5f4: outptr = "g"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL G */
                  case 0x1d5f5: outptr = "h"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL H */
                  case 0x1d5f6: outptr = "i"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL I */
                  case 0x1d5f7: outptr = "j"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL J */
                  case 0x1d5f8: outptr = "k"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL K */
                  case 0x1d5f9: outptr = "l"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL L */
                  case 0x1d5fa: outptr = "m"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL M */
                  case 0x1d5fb: outptr = "n"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL N */
                  case 0x1d5fc: outptr = "o"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL O */
                  case 0x1d5fd: outptr = "p"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL P */
                  case 0x1d5fe: outptr = "q"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL Q */
                  case 0x1d5ff: outptr = "r"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL R */
                  case 0x1d600: outptr = "s"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL S */
                  case 0x1d601: outptr = "t"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL T */
                  case 0x1d602: outptr = "u"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL U */
                  case 0x1d603: outptr = "v"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL V */
                  case 0x1d604: outptr = "w"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL W */
                  case 0x1d605: outptr = "x"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL X */
                  case 0x1d606: outptr = "y"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL Y */
                  case 0x1d607: outptr = "z"; break; /* MATHEMATICAL SANS-SERIF BOLD SMALL Z */
                  case 0x1d608: outptr = "A"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL A */
                  case 0x1d609: outptr = "B"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL B */
                  case 0x1d60a: outptr = "C"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL C */
                  case 0x1d60b: outptr = "D"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL D */
                  case 0x1d60c: outptr = "E"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL E */
                  case 0x1d60d: outptr = "F"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL F */
                  case 0x1d60e: outptr = "G"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL G */
                  case 0x1d60f: outptr = "H"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL H */
                  case 0x1d610: outptr = "I"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL I */
                  case 0x1d611: outptr = "J"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL J */
                  case 0x1d612: outptr = "K"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL K */
                  case 0x1d613: outptr = "L"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL L */
                  case 0x1d614: outptr = "M"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL M */
                  case 0x1d615: outptr = "N"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL N */
                  case 0x1d616: outptr = "O"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL O */
                  case 0x1d617: outptr = "P"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL P */
                  case 0x1d618: outptr = "Q"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL Q */
                  case 0x1d619: outptr = "R"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL R */
                  case 0x1d61a: outptr = "S"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL S */
                  case 0x1d61b: outptr = "T"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL T */
                  case 0x1d61c: outptr = "U"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL U */
                  case 0x1d61d: outptr = "V"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL V */
                  case 0x1d61e: outptr = "W"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL W */
                  case 0x1d61f: outptr = "X"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL X */
                  case 0x1d620: outptr = "Y"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL Y */
                  case 0x1d621: outptr = "Z"; break; /* MATHEMATICAL SANS-SERIF ITALIC CAPITAL Z */
                  case 0x1d622: outptr = "a"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL A */
                  case 0x1d623: outptr = "b"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL B */
                  case 0x1d624: outptr = "c"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL C */
                  case 0x1d625: outptr = "d"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL D */
                  case 0x1d626: outptr = "e"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL E */
                  case 0x1d627: outptr = "f"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL F */
                  case 0x1d628: outptr = "g"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL G */
                  case 0x1d629: outptr = "h"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL H */
                  case 0x1d62a: outptr = "i"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL I */
                  case 0x1d62b: outptr = "j"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL J */
                  case 0x1d62c: outptr = "k"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL K */
                  case 0x1d62d: outptr = "l"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL L */
                  case 0x1d62e: outptr = "m"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL M */
                  case 0x1d62f: outptr = "n"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL N */
                  case 0x1d630: outptr = "o"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL O */
                  case 0x1d631: outptr = "p"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL P */
                  case 0x1d632: outptr = "q"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL Q */
                  case 0x1d633: outptr = "r"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL R */
                  case 0x1d634: outptr = "s"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL S */
                  case 0x1d635: outptr = "t"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL T */
                  case 0x1d636: outptr = "u"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL U */
                  case 0x1d637: outptr = "v"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL V */
                  case 0x1d638: outptr = "w"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL W */
                  case 0x1d639: outptr = "x"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL X */
                  case 0x1d63a: outptr = "y"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL Y */
                  case 0x1d63b: outptr = "z"; break; /* MATHEMATICAL SANS-SERIF ITALIC SMALL Z */
                  case 0x1d63c: outptr = "A"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL A */
                  case 0x1d63d: outptr = "B"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL B */
                  case 0x1d63e: outptr = "C"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL C */
                  case 0x1d63f: outptr = "D"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL D */
                  case 0x1d640: outptr = "E"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL E */
                  case 0x1d641: outptr = "F"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL F */
                  case 0x1d642: outptr = "G"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL G */
                  case 0x1d643: outptr = "H"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL H */
                  case 0x1d644: outptr = "I"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL I */
                  case 0x1d645: outptr = "J"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL J */
                  case 0x1d646: outptr = "K"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL K */
                  case 0x1d647: outptr = "L"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL L */
                  case 0x1d648: outptr = "M"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL M */
                  case 0x1d649: outptr = "N"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL N */
                  case 0x1d64a: outptr = "O"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL O */
                  case 0x1d64b: outptr = "P"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL P */
                  case 0x1d64c: outptr = "Q"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL Q */
                  case 0x1d64d: outptr = "R"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL R */
                  case 0x1d64e: outptr = "S"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL S */
                  case 0x1d64f: outptr = "T"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL T */
                  case 0x1d650: outptr = "U"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL U */
                  case 0x1d651: outptr = "V"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL V */
                  case 0x1d652: outptr = "W"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL W */
                  case 0x1d653: outptr = "X"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL X */
                  case 0x1d654: outptr = "Y"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL Y */
                  case 0x1d655: outptr = "Z"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL Z */
                  case 0x1d656: outptr = "a"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL A */
                  case 0x1d657: outptr = "b"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL B */
                  case 0x1d658: outptr = "c"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL C */
                  case 0x1d659: outptr = "d"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL D */
                  case 0x1d65a: outptr = "e"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL E */
                  case 0x1d65b: outptr = "f"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL F */
                  case 0x1d65c: outptr = "g"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL G */
                  case 0x1d65d: outptr = "h"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL H */
                  case 0x1d65e: outptr = "i"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL I */
                  case 0x1d65f: outptr = "j"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL J */
                  case 0x1d660: outptr = "k"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL K */
                  case 0x1d661: outptr = "l"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL L */
                  case 0x1d662: outptr = "m"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL M */
                  case 0x1d663: outptr = "n"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL N */
                  case 0x1d664: outptr = "o"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL O */
                  case 0x1d665: outptr = "p"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL P */
                  case 0x1d666: outptr = "q"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL Q */
                  case 0x1d667: outptr = "r"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL R */
                  case 0x1d668: outptr = "s"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL S */
                  case 0x1d669: outptr = "t"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL T */
                  case 0x1d66a: outptr = "u"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL U */
                  case 0x1d66b: outptr = "v"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL V */
                  case 0x1d66c: outptr = "w"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL W */
                  case 0x1d66d: outptr = "x"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL X */
                  case 0x1d66e: outptr = "y"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL Y */
                  case 0x1d66f: outptr = "z"; break; /* MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL Z */
                  case 0x1d670: outptr = "A"; break; /* MATHEMATICAL MONOSPACE CAPITAL A */
                  case 0x1d671: outptr = "B"; break; /* MATHEMATICAL MONOSPACE CAPITAL B */
                  case 0x1d672: outptr = "C"; break; /* MATHEMATICAL MONOSPACE CAPITAL C */
                  case 0x1d673: outptr = "D"; break; /* MATHEMATICAL MONOSPACE CAPITAL D */
                  case 0x1d674: outptr = "E"; break; /* MATHEMATICAL MONOSPACE CAPITAL E */
                  case 0x1d675: outptr = "F"; break; /* MATHEMATICAL MONOSPACE CAPITAL F */
                  case 0x1d676: outptr = "G"; break; /* MATHEMATICAL MONOSPACE CAPITAL G */
                  case 0x1d677: outptr = "H"; break; /* MATHEMATICAL MONOSPACE CAPITAL H */
                  case 0x1d678: outptr = "I"; break; /* MATHEMATICAL MONOSPACE CAPITAL I */
                  case 0x1d679: outptr = "J"; break; /* MATHEMATICAL MONOSPACE CAPITAL J */
                  case 0x1d67a: outptr = "K"; break; /* MATHEMATICAL MONOSPACE CAPITAL K */
                  case 0x1d67b: outptr = "L"; break; /* MATHEMATICAL MONOSPACE CAPITAL L */
                  case 0x1d67c: outptr = "M"; break; /* MATHEMATICAL MONOSPACE CAPITAL M */
                  case 0x1d67d: outptr = "N"; break; /* MATHEMATICAL MONOSPACE CAPITAL N */
                  case 0x1d67e: outptr = "O"; break; /* MATHEMATICAL MONOSPACE CAPITAL O */
                  case 0x1d67f: outptr = "P"; break; /* MATHEMATICAL MONOSPACE CAPITAL P */
                  case 0x1d680: outptr = "Q"; break; /* MATHEMATICAL MONOSPACE CAPITAL Q */
                  case 0x1d681: outptr = "R"; break; /* MATHEMATICAL MONOSPACE CAPITAL R */
                  case 0x1d682: outptr = "S"; break; /* MATHEMATICAL MONOSPACE CAPITAL S */
                  case 0x1d683: outptr = "T"; break; /* MATHEMATICAL MONOSPACE CAPITAL T */
                  case 0x1d684: outptr = "U"; break; /* MATHEMATICAL MONOSPACE CAPITAL U */
                  case 0x1d685: outptr = "V"; break; /* MATHEMATICAL MONOSPACE CAPITAL V */
                  case 0x1d686: outptr = "W"; break; /* MATHEMATICAL MONOSPACE CAPITAL W */
                  case 0x1d687: outptr = "X"; break; /* MATHEMATICAL MONOSPACE CAPITAL X */
                  case 0x1d688: outptr = "Y"; break; /* MATHEMATICAL MONOSPACE CAPITAL Y */
                  case 0x1d689: outptr = "Z"; break; /* MATHEMATICAL MONOSPACE CAPITAL Z */
                  case 0x1d68a: outptr = "a"; break; /* MATHEMATICAL MONOSPACE SMALL A */
                  case 0x1d68b: outptr = "b"; break; /* MATHEMATICAL MONOSPACE SMALL B */
                  case 0x1d68c: outptr = "c"; break; /* MATHEMATICAL MONOSPACE SMALL C */
                  case 0x1d68d: outptr = "d"; break; /* MATHEMATICAL MONOSPACE SMALL D */
                  case 0x1d68e: outptr = "e"; break; /* MATHEMATICAL MONOSPACE SMALL E */
                  case 0x1d68f: outptr = "f"; break; /* MATHEMATICAL MONOSPACE SMALL F */
                  case 0x1d690: outptr = "g"; break; /* MATHEMATICAL MONOSPACE SMALL G */
                  case 0x1d691: outptr = "h"; break; /* MATHEMATICAL MONOSPACE SMALL H */
                  case 0x1d692: outptr = "i"; break; /* MATHEMATICAL MONOSPACE SMALL I */
                  case 0x1d693: outptr = "j"; break; /* MATHEMATICAL MONOSPACE SMALL J */
                  case 0x1d694: outptr = "k"; break; /* MATHEMATICAL MONOSPACE SMALL K */
                  case 0x1d695: outptr = "l"; break; /* MATHEMATICAL MONOSPACE SMALL L */
                  case 0x1d696: outptr = "m"; break; /* MATHEMATICAL MONOSPACE SMALL M */
                  case 0x1d697: outptr = "n"; break; /* MATHEMATICAL MONOSPACE SMALL N */
                  case 0x1d698: outptr = "o"; break; /* MATHEMATICAL MONOSPACE SMALL O */
                  case 0x1d699: outptr = "p"; break; /* MATHEMATICAL MONOSPACE SMALL P */
                  case 0x1d69a: outptr = "q"; break; /* MATHEMATICAL MONOSPACE SMALL Q */
                  case 0x1d69b: outptr = "r"; break; /* MATHEMATICAL MONOSPACE SMALL R */
                  case 0x1d69c: outptr = "s"; break; /* MATHEMATICAL MONOSPACE SMALL S */
                  case 0x1d69d: outptr = "t"; break; /* MATHEMATICAL MONOSPACE SMALL T */
                  case 0x1d69e: outptr = "u"; break; /* MATHEMATICAL MONOSPACE SMALL U */
                  case 0x1d69f: outptr = "v"; break; /* MATHEMATICAL MONOSPACE SMALL V */
                  case 0x1d6a0: outptr = "w"; break; /* MATHEMATICAL MONOSPACE SMALL W */
                  case 0x1d6a1: outptr = "x"; break; /* MATHEMATICAL MONOSPACE SMALL X */
                  case 0x1d6a2: outptr = "y"; break; /* MATHEMATICAL MONOSPACE SMALL Y */
                  case 0x1d6a3: outptr = "z"; break; /* MATHEMATICAL MONOSPACE SMALL Z */
                  case 0x1d7ce: outptr = "0"; break; /* MATHEMATICAL BOLD DIGIT ZERO */
                  case 0x1d7cf: outptr = "1"; break; /* MATHEMATICAL BOLD DIGIT ONE */
                  case 0x1d7d0: outptr = "2"; break; /* MATHEMATICAL BOLD DIGIT TWO */
                  case 0x1d7d1: outptr = "3"; break; /* MATHEMATICAL BOLD DIGIT THREE */
                  case 0x1d7d2: outptr = "4"; break; /* MATHEMATICAL BOLD DIGIT FOUR */
                  case 0x1d7d3: outptr = "5"; break; /* MATHEMATICAL BOLD DIGIT FIVE */
                  case 0x1d7d4: outptr = "6"; break; /* MATHEMATICAL BOLD DIGIT SIX */
                  case 0x1d7d5: outptr = "7"; break; /* MATHEMATICAL BOLD DIGIT SEVEN */
                  case 0x1d7d6: outptr = "8"; break; /* MATHEMATICAL BOLD DIGIT EIGHT */
                  case 0x1d7d7: outptr = "9"; break; /* MATHEMATICAL BOLD DIGIT NINE */
                  case 0x1d7d8: outptr = "0"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT ZERO */
                  case 0x1d7d9: outptr = "1"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT ONE */
                  case 0x1d7da: outptr = "2"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT TWO */
                  case 0x1d7db: outptr = "3"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT THREE */
                  case 0x1d7dc: outptr = "4"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT FOUR */
                  case 0x1d7dd: outptr = "5"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT FIVE */
                  case 0x1d7de: outptr = "6"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT SIX */
                  case 0x1d7df: outptr = "7"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT SEVEN */
                  case 0x1d7e0: outptr = "8"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT EIGHT */
                  case 0x1d7e1: outptr = "9"; break; /* MATHEMATICAL DOUBLE-STRUCK DIGIT NINE */
                  case 0x1d7e2: outptr = "0"; break; /* MATHEMATICAL SANS-SERIF DIGIT ZERO */
                  case 0x1d7e3: outptr = "1"; break; /* MATHEMATICAL SANS-SERIF DIGIT ONE */
                  case 0x1d7e4: outptr = "2"; break; /* MATHEMATICAL SANS-SERIF DIGIT TWO */
                  case 0x1d7e5: outptr = "3"; break; /* MATHEMATICAL SANS-SERIF DIGIT THREE */
                  case 0x1d7e6: outptr = "4"; break; /* MATHEMATICAL SANS-SERIF DIGIT FOUR */
                  case 0x1d7e7: outptr = "5"; break; /* MATHEMATICAL SANS-SERIF DIGIT FIVE */
                  case 0x1d7e8: outptr = "6"; break; /* MATHEMATICAL SANS-SERIF DIGIT SIX */
                  case 0x1d7e9: outptr = "7"; break; /* MATHEMATICAL SANS-SERIF DIGIT SEVEN */
                  case 0x1d7ea: outptr = "8"; break; /* MATHEMATICAL SANS-SERIF DIGIT EIGHT */
                  case 0x1d7eb: outptr = "9"; break; /* MATHEMATICAL SANS-SERIF DIGIT NINE */
                  case 0x1d7ec: outptr = "0"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT ZERO */
                  case 0x1d7ed: outptr = "1"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT ONE */
                  case 0x1d7ee: outptr = "2"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT TWO */
                  case 0x1d7ef: outptr = "3"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT THREE */
                  case 0x1d7f0: outptr = "4"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT FOUR */
                  case 0x1d7f1: outptr = "5"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT FIVE */
                  case 0x1d7f2: outptr = "6"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT SIX */
                  case 0x1d7f3: outptr = "7"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT SEVEN */
                  case 0x1d7f4: outptr = "8"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT EIGHT */
                  case 0x1d7f5: outptr = "9"; break; /* MATHEMATICAL SANS-SERIF BOLD DIGIT NINE */
                  case 0x1d7f6: outptr = "0"; break; /* MATHEMATICAL MONOSPACE DIGIT ZERO */
                  case 0x1d7f7: outptr = "1"; break; /* MATHEMATICAL MONOSPACE DIGIT ONE */
                  case 0x1d7f8: outptr = "2"; break; /* MATHEMATICAL MONOSPACE DIGIT TWO */
                  case 0x1d7f9: outptr = "3"; break; /* MATHEMATICAL MONOSPACE DIGIT THREE */
                  case 0x1d7fa: outptr = "4"; break; /* MATHEMATICAL MONOSPACE DIGIT FOUR */
                  case 0x1d7fb: outptr = "5"; break; /* MATHEMATICAL MONOSPACE DIGIT FIVE */
                  case 0x1d7fc: outptr = "6"; break; /* MATHEMATICAL MONOSPACE DIGIT SIX */
                  case 0x1d7fd: outptr = "7"; break; /* MATHEMATICAL MONOSPACE DIGIT SEVEN */
                  case 0x1d7fe: outptr = "8"; break; /* MATHEMATICAL MONOSPACE DIGIT EIGHT */
                  case 0x1d7ff: outptr = "9"; break; /* MATHEMATICAL MONOSPACE DIGIT NINE */
                  case 0x1f100: outptr = "0."; break; /* DIGIT ZERO FULL STOP */
                  case 0x1f101: outptr = "0,"; break; /* DIGIT ZERO COMMA */
                  case 0x1f102: outptr = "1,"; break; /* DIGIT ONE COMMA */
                  case 0x1f103: outptr = "2,"; break; /* DIGIT TWO COMMA */
                  case 0x1f104: outptr = "3,"; break; /* DIGIT THREE COMMA */
                  case 0x1f105: outptr = "4,"; break; /* DIGIT FOUR COMMA */
                  case 0x1f106: outptr = "5,"; break; /* DIGIT FIVE COMMA */
                  case 0x1f107: outptr = "6,"; break; /* DIGIT SIX COMMA */
                  case 0x1f108: outptr = "7,"; break; /* DIGIT SEVEN COMMA */
                  case 0x1f109: outptr = "8,"; break; /* DIGIT EIGHT COMMA */
                  case 0x1f10a: outptr = "9,"; break; /* DIGIT NINE COMMA */
                  case 0x1f110: outptr = "(A)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER A */
                  case 0x1f111: outptr = "(B)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER B */
                  case 0x1f112: outptr = "(C)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER C */
                  case 0x1f113: outptr = "(D)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER D */
                  case 0x1f114: outptr = "(E)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER E */
                  case 0x1f115: outptr = "(F)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER F */
                  case 0x1f116: outptr = "(G)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER G */
                  case 0x1f117: outptr = "(H)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER H */
                  case 0x1f118: outptr = "(I)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER I */
                  case 0x1f119: outptr = "(J)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER J */
                  case 0x1f11a: outptr = "(K)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER K */
                  case 0x1f11b: outptr = "(L)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER L */
                  case 0x1f11c: outptr = "(M)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER M */
                  case 0x1f11d: outptr = "(N)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER N */
                  case 0x1f11e: outptr = "(O)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER O */
                  case 0x1f11f: outptr = "(P)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER P */
                  case 0x1f120: outptr = "(Q)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER Q */
                  case 0x1f121: outptr = "(R)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER R */
                  case 0x1f122: outptr = "(S)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER S */
                  case 0x1f123: outptr = "(T)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER T */
                  case 0x1f124: outptr = "(U)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER U */
                  case 0x1f125: outptr = "(V)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER V */
                  case 0x1f126: outptr = "(W)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER W */
                  case 0x1f127: outptr = "(X)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER X */
                  case 0x1f128: outptr = "(Y)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER Y */
                  case 0x1f129: outptr = "(Z)"; break; /* PARENTHESIZED LATIN CAPITAL LETTER Z */
                  case 0x1f12a: outptr = "S"; break; /* TORTOISE SHELL BRACKETED LATIN CAPITAL LETTER S */
                  case 0x1f12b: outptr = "C"; break; /* CIRCLED ITALIC LATIN CAPITAL LETTER C */
                  case 0x1f12c: outptr = "R"; break; /* CIRCLED ITALIC LATIN CAPITAL LETTER R */
                  case 0x1f12d: outptr = "CD"; break; /* CIRCLED CD */
                  case 0x1f12e: outptr = "WZ"; break; /* CIRCLED WZ */
                  case 0x1f130: outptr = "A"; break; /* SQUARED LATIN CAPITAL LETTER A */
                  case 0x1f131: outptr = "B"; break; /* SQUARED LATIN CAPITAL LETTER B */
                  case 0x1f132: outptr = "C"; break; /* SQUARED LATIN CAPITAL LETTER C */
                  case 0x1f133: outptr = "D"; break; /* SQUARED LATIN CAPITAL LETTER D */
                  case 0x1f134: outptr = "E"; break; /* SQUARED LATIN CAPITAL LETTER E */
                  case 0x1f135: outptr = "F"; break; /* SQUARED LATIN CAPITAL LETTER F */
                  case 0x1f136: outptr = "G"; break; /* SQUARED LATIN CAPITAL LETTER G */
                  case 0x1f137: outptr = "H"; break; /* SQUARED LATIN CAPITAL LETTER H */
                  case 0x1f138: outptr = "I"; break; /* SQUARED LATIN CAPITAL LETTER I */
                  case 0x1f139: outptr = "J"; break; /* SQUARED LATIN CAPITAL LETTER J */
                  case 0x1f13a: outptr = "K"; break; /* SQUARED LATIN CAPITAL LETTER K */
                  case 0x1f13b: outptr = "L"; break; /* SQUARED LATIN CAPITAL LETTER L */
                  case 0x1f13c: outptr = "M"; break; /* SQUARED LATIN CAPITAL LETTER M */
                  case 0x1f13d: outptr = "N"; break; /* SQUARED LATIN CAPITAL LETTER N */
                  case 0x1f13e: outptr = "O"; break; /* SQUARED LATIN CAPITAL LETTER O */
                  case 0x1f13f: outptr = "P"; break; /* SQUARED LATIN CAPITAL LETTER P */
                  case 0x1f140: outptr = "Q"; break; /* SQUARED LATIN CAPITAL LETTER Q */
                  case 0x1f141: outptr = "R"; break; /* SQUARED LATIN CAPITAL LETTER R */
                  case 0x1f142: outptr = "S"; break; /* SQUARED LATIN CAPITAL LETTER S */
                  case 0x1f143: outptr = "T"; break; /* SQUARED LATIN CAPITAL LETTER T */
                  case 0x1f144: outptr = "U"; break; /* SQUARED LATIN CAPITAL LETTER U */
                  case 0x1f145: outptr = "V"; break; /* SQUARED LATIN CAPITAL LETTER V */
                  case 0x1f146: outptr = "W"; break; /* SQUARED LATIN CAPITAL LETTER W */
                  case 0x1f147: outptr = "X"; break; /* SQUARED LATIN CAPITAL LETTER X */
                  case 0x1f148: outptr = "Y"; break; /* SQUARED LATIN CAPITAL LETTER Y */
                  case 0x1f149: outptr = "Z"; break; /* SQUARED LATIN CAPITAL LETTER Z */
                  case 0x1f14a: outptr = "HV"; break; /* SQUARED HV */
                  case 0x1f14b: outptr = "MV"; break; /* SQUARED MV */
                  case 0x1f14c: outptr = "SD"; break; /* SQUARED SD */
                  case 0x1f14d: outptr = "SS"; break; /* SQUARED SS */
                  case 0x1f14e: outptr = "PPV"; break; /* SQUARED PPV */
                  case 0x1f14f: outptr = "WC"; break; /* SQUARED WC */
                  case 0x1f16a: outptr = "MC"; break; /* RAISED MC SIGN */
                  case 0x1f16b: outptr = "MD"; break; /* RAISED MD SIGN */
                  case 0x1f190: outptr = "DJ"; break; /* SQUARE DJ */

                    /*
                     * Some more cases put in by hand.
                     */
                  case 0x00ae: outptr = "(R)"; break; /* REGISTERED SIGN */
                  case 0x00a9: outptr = "(C)"; break; /* COPYRIGHT SIGN */
                  case 0x00ab: outptr = "<<"; break; /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
                  case 0x00bb: outptr = ">>"; break; /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
                  case 0x2018: outptr = "'"; break; /* LEFT SINGLE QUOTATION MARK */
                  case 0x2019: outptr = "'"; break; /* RIGHT SINGLE QUOTATION MARK */
                  case 0x201a: outptr = ","; break; /* SINGLE LOW-9 QUOTATION MARK */
                  case 0x201b: outptr = "'"; break; /* SINGLE HIGH-REVERSED-9 QUOTATION MARK */
                  case 0x201c: outptr = "\""; break; /* LEFT DOUBLE QUOTATION MARK */
                  case 0x201d: outptr = "\""; break; /* RIGHT DOUBLE QUOTATION MARK */
                  case 0x201e: outptr = "\""; break; /* DOUBLE LOW-9 QUOTATION MARK */
                  case 0x201f: outptr = "\""; break; /* DOUBLE HIGH-REVERSED-9 QUOTATION MARK */
                  case 0x2039: outptr = "<"; break; /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
                  case 0x203a: outptr = ">"; break; /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
                  case 0x301d: outptr = "``"; break; /* REVERSED DOUBLE PRIME QUOTATION MARK */
                  case 0x301e: outptr = "''"; break; /* DOUBLE PRIME QUOTATION MARK */
                  case 0x301f: outptr = ",,"; break; /* LOW DOUBLE PRIME QUOTATION MARK */
                  case 0x2012: outptr = "-"; break; /* FIGURE DASH */
                  case 0x2013: outptr = "-"; break; /* EN DASH */
                  case 0x2014: outptr = "-"; break; /* EM DASH */
                  case 0x2015: outptr = "-"; break; /* HORIZONTAL BAR */
                  case 0x2e3a: outptr = "-"; break; /* TWO-EM DASH */
                  case 0x2e3b: outptr = "-"; break; /* THREE-EM DASH */
                  case 0xfe58: outptr = "-"; break; /* SMALL EM DASH */
                  case 0x00ad: outptr = "-"; break; /* SOFT HYPHEN */
                  case 0x2010: outptr = "-"; break; /* HYPHEN */
                  case 0x2011: outptr = "-"; break; /* NON-BREAKING HYPHEN */
                  case 0x2043: outptr = "-"; break; /* HYPHEN BULLET */
                  case 0x2022: outptr = "*"; break; /* BULLET */
                  case 0x00B7: outptr = "*"; break; /* MIDDLE DOT (which I choose to consider most likely to be used as a bullet) */
                  case 0x00A3: outptr = "GBP"; break; /* POUND SIGN (it's this or L) */
                  case 0x00A6: outptr = "|"; break; /* BROKEN BAR */
                  case 0x00B0: outptr = "deg"; break; /* DEGREE SIGN */
                  case 0x00D7: outptr = "x"; break; /* MULTIPLICATION SIGN (should this be * for maths contexts, or x for 1024x768 contexts?) */
                  case 0x00F7: outptr = "/"; break; /* DIVISION SIGN (here I have no better option, though it's a shame it doesn't match my choice for D7) */

                    /*
                     * Box drawing characters.
                     */
                  case 0x2508: /* BOX DRAWINGS LIGHT QUADRUPLE DASH HORIZONTAL */
                  case 0x2509: /* BOX DRAWINGS HEAVY QUADRUPLE DASH HORIZONTAL */
                  case 0x250C: /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
                  case 0x250D: /* BOX DRAWINGS DOWN LIGHT AND RIGHT HEAVY */
                  case 0x250E: /* BOX DRAWINGS DOWN HEAVY AND RIGHT LIGHT */
                  case 0x250F: /* BOX DRAWINGS HEAVY DOWN AND RIGHT */
                  case 0x2510: /* BOX DRAWINGS LIGHT DOWN AND LEFT */
                  case 0x2511: /* BOX DRAWINGS DOWN LIGHT AND LEFT HEAVY */
                  case 0x2512: /* BOX DRAWINGS DOWN HEAVY AND LEFT LIGHT */
                  case 0x2513: /* BOX DRAWINGS HEAVY DOWN AND LEFT */
                  case 0x2514: /* BOX DRAWINGS LIGHT UP AND RIGHT */
                  case 0x2515: /* BOX DRAWINGS UP LIGHT AND RIGHT HEAVY */
                  case 0x2516: /* BOX DRAWINGS UP HEAVY AND RIGHT LIGHT */
                  case 0x2517: /* BOX DRAWINGS HEAVY UP AND RIGHT */
                  case 0x2518: /* BOX DRAWINGS LIGHT UP AND LEFT */
                  case 0x2519: /* BOX DRAWINGS UP LIGHT AND LEFT HEAVY */
                  case 0x251A: /* BOX DRAWINGS UP HEAVY AND LEFT LIGHT */
                  case 0x251B: /* BOX DRAWINGS HEAVY UP AND LEFT */
                  case 0x251C: /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
                  case 0x251D: /* BOX DRAWINGS VERTICAL LIGHT AND RIGHT HEAVY */
                  case 0x251E: /* BOX DRAWINGS UP HEAVY AND RIGHT DOWN LIGHT */
                  case 0x251F: /* BOX DRAWINGS DOWN HEAVY AND RIGHT UP LIGHT */
                  case 0x2520: /* BOX DRAWINGS VERTICAL HEAVY AND RIGHT LIGHT */
                  case 0x2521: /* BOX DRAWINGS DOWN LIGHT AND RIGHT UP HEAVY */
                  case 0x2522: /* BOX DRAWINGS UP LIGHT AND RIGHT DOWN HEAVY */
                  case 0x2523: /* BOX DRAWINGS HEAVY VERTICAL AND RIGHT */
                  case 0x2524: /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
                  case 0x2525: /* BOX DRAWINGS VERTICAL LIGHT AND LEFT HEAVY */
                  case 0x2526: /* BOX DRAWINGS UP HEAVY AND LEFT DOWN LIGHT */
                  case 0x2527: /* BOX DRAWINGS DOWN HEAVY AND LEFT UP LIGHT */
                  case 0x2528: /* BOX DRAWINGS VERTICAL HEAVY AND LEFT LIGHT */
                  case 0x2529: /* BOX DRAWINGS DOWN LIGHT AND LEFT UP HEAVY */
                  case 0x252A: /* BOX DRAWINGS UP LIGHT AND LEFT DOWN HEAVY */
                  case 0x252B: /* BOX DRAWINGS HEAVY VERTICAL AND LEFT */
                  case 0x252C: /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
                  case 0x252D: /* BOX DRAWINGS LEFT HEAVY AND RIGHT DOWN LIGHT */
                  case 0x252E: /* BOX DRAWINGS RIGHT HEAVY AND LEFT DOWN LIGHT */
                  case 0x252F: /* BOX DRAWINGS DOWN LIGHT AND HORIZONTAL HEAVY */
                  case 0x2530: /* BOX DRAWINGS DOWN HEAVY AND HORIZONTAL LIGHT */
                  case 0x2531: /* BOX DRAWINGS RIGHT LIGHT AND LEFT DOWN HEAVY */
                  case 0x2532: /* BOX DRAWINGS LEFT LIGHT AND RIGHT DOWN HEAVY */
                  case 0x2533: /* BOX DRAWINGS HEAVY DOWN AND HORIZONTAL */
                  case 0x2534: /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
                  case 0x2535: /* BOX DRAWINGS LEFT HEAVY AND RIGHT UP LIGHT */
                  case 0x2536: /* BOX DRAWINGS RIGHT HEAVY AND LEFT UP LIGHT */
                  case 0x2537: /* BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY */
                  case 0x2538: /* BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT */
                  case 0x2539: /* BOX DRAWINGS RIGHT LIGHT AND LEFT UP HEAVY */
                  case 0x253A: /* BOX DRAWINGS LEFT LIGHT AND RIGHT UP HEAVY */
                  case 0x253B: /* BOX DRAWINGS HEAVY UP AND HORIZONTAL */
                  case 0x253C: /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
                  case 0x253D: /* BOX DRAWINGS LEFT HEAVY AND RIGHT VERTICAL LIGHT */
                  case 0x253E: /* BOX DRAWINGS RIGHT HEAVY AND LEFT VERTICAL LIGHT */
                  case 0x253F: /* BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY */
                  case 0x2540: /* BOX DRAWINGS UP HEAVY AND DOWN HORIZONTAL LIGHT */
                  case 0x2541: /* BOX DRAWINGS DOWN HEAVY AND UP HORIZONTAL LIGHT */
                  case 0x2542: /* BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT */
                  case 0x2543: /* BOX DRAWINGS LEFT UP HEAVY AND RIGHT DOWN LIGHT */
                  case 0x2544: /* BOX DRAWINGS RIGHT UP HEAVY AND LEFT DOWN LIGHT */
                  case 0x2545: /* BOX DRAWINGS LEFT DOWN HEAVY AND RIGHT UP LIGHT */
                  case 0x2546: /* BOX DRAWINGS RIGHT DOWN HEAVY AND LEFT UP LIGHT */
                  case 0x2547: /* BOX DRAWINGS DOWN LIGHT AND UP HORIZONTAL HEAVY */
                  case 0x2548: /* BOX DRAWINGS UP LIGHT AND DOWN HORIZONTAL HEAVY */
                  case 0x2549: /* BOX DRAWINGS RIGHT LIGHT AND LEFT VERTICAL HEAVY */
                  case 0x254A: /* BOX DRAWINGS LEFT LIGHT AND RIGHT VERTICAL HEAVY */
                  case 0x254B: /* BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL */
                  case 0x2552: /* BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE */
                  case 0x2553: /* BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE */
                  case 0x2554: /* BOX DRAWINGS DOUBLE DOWN AND RIGHT */
                  case 0x2555: /* BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE */
                  case 0x2556: /* BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE */
                  case 0x2557: /* BOX DRAWINGS DOUBLE DOWN AND LEFT */
                  case 0x2558: /* BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE */
                  case 0x2559: /* BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE */
                  case 0x255A: /* BOX DRAWINGS DOUBLE UP AND RIGHT */
                  case 0x255B: /* BOX DRAWINGS UP SINGLE AND LEFT DOUBLE */
                  case 0x255C: /* BOX DRAWINGS UP DOUBLE AND LEFT SINGLE */
                  case 0x255D: /* BOX DRAWINGS DOUBLE UP AND LEFT */
                  case 0x255E: /* BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE */
                  case 0x255F: /* BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE */
                  case 0x2560: /* BOX DRAWINGS DOUBLE VERTICAL AND RIGHT */
                  case 0x2561: /* BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE */
                  case 0x2562: /* BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE */
                  case 0x2563: /* BOX DRAWINGS DOUBLE VERTICAL AND LEFT */
                  case 0x2564: /* BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE */
                  case 0x2565: /* BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE */
                  case 0x2566: /* BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL */
                  case 0x2567: /* BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE */
                  case 0x2568: /* BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE */
                  case 0x2569: /* BOX DRAWINGS DOUBLE UP AND HORIZONTAL */
                  case 0x256A: /* BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE */
                  case 0x256B: /* BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE */
                  case 0x256C: /* BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL */
                  case 0x256D: /* BOX DRAWINGS LIGHT ARC DOWN AND RIGHT */
                  case 0x256E: /* BOX DRAWINGS LIGHT ARC DOWN AND LEFT */
                  case 0x256F: /* BOX DRAWINGS LIGHT ARC UP AND LEFT */
                  case 0x2570: /* BOX DRAWINGS LIGHT ARC UP AND RIGHT */
                  case 0x2571: /* BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT */
                  case 0x2572: /* BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT */
                    outptr = "+";
                    break;

                  case 0x2502: /* BOX DRAWINGS LIGHT VERTICAL */
                  case 0x2503: /* BOX DRAWINGS HEAVY VERTICAL */
                  case 0x2506: /* BOX DRAWINGS LIGHT TRIPLE DASH VERTICAL */
                  case 0x2507: /* BOX DRAWINGS HEAVY TRIPLE DASH VERTICAL */
                  case 0x250A: /* BOX DRAWINGS LIGHT QUADRUPLE DASH VERTICAL */
                  case 0x250B: /* BOX DRAWINGS HEAVY QUADRUPLE DASH VERTICAL */
                  case 0x254E: /* BOX DRAWINGS LIGHT DOUBLE DASH VERTICAL */
                  case 0x254F: /* BOX DRAWINGS HEAVY DOUBLE DASH VERTICAL */
                  case 0x2551: /* BOX DRAWINGS DOUBLE VERTICAL */
                  case 0x2575: /* BOX DRAWINGS LIGHT UP */
                  case 0x2577: /* BOX DRAWINGS LIGHT DOWN */
                  case 0x2579: /* BOX DRAWINGS HEAVY UP */
                  case 0x257B: /* BOX DRAWINGS HEAVY DOWN */
                  case 0x257D: /* BOX DRAWINGS LIGHT UP AND HEAVY DOWN */
                  case 0x257F: /* BOX DRAWINGS HEAVY UP AND LIGHT DOWN */
                    outptr = "|";
                    break;

                  case 0x2500: /* BOX DRAWINGS LIGHT HORIZONTAL */
                  case 0x2501: /* BOX DRAWINGS HEAVY HORIZONTAL */
                  case 0x2504: /* BOX DRAWINGS LIGHT TRIPLE DASH HORIZONTAL */
                  case 0x2505: /* BOX DRAWINGS HEAVY TRIPLE DASH HORIZONTAL */
                  case 0x254C: /* BOX DRAWINGS LIGHT DOUBLE DASH HORIZONTAL */
                  case 0x254D: /* BOX DRAWINGS HEAVY DOUBLE DASH HORIZONTAL */
                  case 0x2550: /* BOX DRAWINGS DOUBLE HORIZONTAL */
                  case 0x2574: /* BOX DRAWINGS LIGHT LEFT */
                  case 0x2576: /* BOX DRAWINGS LIGHT RIGHT */
                  case 0x2578: /* BOX DRAWINGS HEAVY LEFT */
                  case 0x257A: /* BOX DRAWINGS HEAVY RIGHT */
                  case 0x257C: /* BOX DRAWINGS LIGHT LEFT AND HEAVY RIGHT */
                  case 0x257E: /* BOX DRAWINGS HEAVY LEFT AND LIGHT RIGHT */
                    outptr = "-";
                    break;

                  default:
                    if (wc >= 0 && wc < 0x80) {
                        localbuf[0] = (char)wc;
                        localbuf[1] = '\0';
                        outptr = localbuf;
                    } else {
                        snprintf(localbuf, sizeof(localbuf), "<U+%04x>", wc);
                        outptr = localbuf;
                    }
                }

                len = strlen(outptr);
                retlen += len;
                if (retsize - retlen < 128) {
                    retsize = (retsize * 3) / 2;
                    ret = realloc(ret, retsize);
                    if (!ret) {
                        fprintf(stderr, "filter: out of memory!\n");
                        exit(1);
                    }
                }
                memcpy(ret + retlen - len, outptr, len);
            }
        }

        *outlen = retlen;
    } else {
        memcpy(ret, data, inlen);
        *outlen = inlen;
    }

    *delay = 0.0;
    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
