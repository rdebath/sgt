/*
 * idletrans.c - translation module for a filter which simply
 * terminates connections after a certain idle time.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

struct tstate {
    double timeout;
    int quiet;
    int last;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->timeout = 60.0;	       /* one minute is the default */
    state->quiet = FALSE;

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    if (longopt) {
	if (!strcmp(longopt, "time") ||
	    !strcmp(longopt, "timeout")) {
	    shortopt = 't';
	} else if (!strcmp(longopt, "quiet")) {
	    shortopt = 'q';
	} else
	    return OPT_UNKNOWN;
    }

    if (shortopt == 't') {
	if (!value)
	    return OPT_MISSINGARG;
	state->timeout = atof(value);
    } else if (shortopt == 'q') {
	if (!value)
	    return OPT_MISSINGARG;
	state->quiet = TRUE;
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
    *idelay = *odelay = state->timeout;
    state->last = -1;
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags)
{
    char *ret;

    if (flags & EV_TIMEOUT) {
	/*
	 * We've had a timeout. Since the filter framework runs
	 * input and output timers separately, we must check
	 * whether the timeout was for the same type of event that
	 * we last set one for.
	 */
	if (state->last != !input) {
	    if (!state->quiet)
		fprintf(stderr, "idlewrapper: timed out\n");
	    exit(0);
	}
    }

    ret = malloc(inlen);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    memcpy(ret, data, inlen);

    *outlen = inlen;
    *delay = state->timeout;
    state->last = !!input;
    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
