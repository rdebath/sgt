/*
 * nulltrans.c - translation module for a filter which doesn't do
 * anything.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

tstate *tstate_init(void)
{
    return NULL;
}

void tstate_argument(tstate *state, char *arg)
{
    fprintf(stderr, "nullfilter: expected no arguments\n");
    exit(1);
}

char *translate(tstate *state, char *data, int inlen, int *outlen, int input)
{
    char *ret;

    ret = malloc(inlen);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    memcpy(ret, data, inlen);

    *outlen = inlen;
    return ret;
}
