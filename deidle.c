/*
 * deidle.c - translation module for a filter which sends some
 * sort of keepalive character after a certain idle time.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

struct tstate {
    double timeout;
    char *string;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->timeout = 60.0 * 25;	       /* 25 minutes is the default */
    state->string = "\014";

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    if (longopt) {
	if (!strcmp(longopt, "time") ||
	    !strcmp(longopt, "timeout")) {
	    shortopt = 't';
	} else if (!strcmp(longopt, "string")) {
	    shortopt = 's';
	} else
	    return OPT_UNKNOWN;
    }

    if (shortopt == 't') {
	if (!value)
	    return OPT_MISSINGARG;
	state->timeout = atof(value);
    } else if (shortopt == 's') {
	if (!value)
	    return OPT_MISSINGARG;
	state->string = value;
    } else
	return OPT_UNKNOWN;

    return OPT_OK;
}

void tstate_argument(tstate *state, char *arg)
{
    state->timeout = atof(arg);
}

void tstate_ready(tstate *state, double *idelay, double *odelay)
{
    *idelay = state->timeout;
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input)
{
    char *ret;

    if (input && !inlen) {
	/*
	 * Timeout on the input side. Send our string.
	 */
	ret = malloc(strlen(state->string));
	if (!ret) {
	    fprintf(stderr, "filter: out of memory!\n");
	    exit(1);
	}

	memcpy(ret, state->string, strlen(state->string));

	*outlen = strlen(state->string);
	*delay = state->timeout;
	return ret;
    }

    ret = malloc(inlen);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    memcpy(ret, data, inlen);

    *outlen = inlen;
    *delay = state->timeout;
    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
