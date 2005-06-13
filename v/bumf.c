/*
 * bumf.c: online help, licence and other such bumf for `v'
 */

#include <stdio.h>
#include "v.h"

static const char *const helptext[] = {
    "usage:   v [options] files",
    "options: -m, --maxsize         use maximum window size over all images",
    "         --help                display this text",
    "         --version             display version number",
    "         --licence             display licence text",
    NULL
};

static const char *const usagetext[] = {
    "usage: v [-m] imagefile [imagefile...]",
    NULL
};

static const char *const licencetext[] = {
    "v is copyright (c) 2005 Simon Tatham.",
    "",
    "Permission is hereby granted, free of charge, to any person",
    "obtaining a copy of this software and associated documentation files",
    "(the \"Software\"), to deal in the Software without restriction,",
    "including without limitation the rights to use, copy, modify, merge,",
    "publish, distribute, sublicense, and/or sell copies of the Software,",
    "and to permit persons to whom the Software is furnished to do so,",
    "subject to the following conditions:",
    "",
    "The above copyright notice and this permission notice shall be",
    "included in all copies or substantial portions of the Software.",
    "",
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,",
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF",
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND",
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS",
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN",
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN",
    "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE",
    "SOFTWARE.",
    NULL
};

void licence(void) {
    const char *const *p;
    for (p = licencetext; *p; p++)
	puts(*p);
}

void help(void) {
    const char *const *p;
    for (p = helptext; *p; p++)
	puts(*p);
}

void usage(void) {
    const char *const *p;
    for (p = usagetext; *p; p++)
	puts(*p);
}

void showversion(void) {
    printf("v has no version numbering (yet?)\n");
}
