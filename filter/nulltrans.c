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

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    return OPT_UNKNOWN;
}

void tstate_argument(tstate *state, char *arg)
{
    fprintf(stderr, "nullfilter: expected no arguments\n");
    exit(1);
}


void tstate_ready(tstate *state, double *idelay, double *odelay)
{
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags)
{
    char *ret;

    ret = malloc(inlen);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    memcpy(ret, data, inlen);

    *outlen = inlen;
    *delay = 0.0;
    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
