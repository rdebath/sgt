/*
 * nhtrans.c - translation module for a filter which translate
 * ESC-x into Meta-x (x with high bit set). Useful for running
 * NetHack (which needs high-bit-meta) within PuTTY (which only
 * supports esc-prefix-meta).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

struct tstate {
    int seen_esc;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->seen_esc = FALSE;

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    return OPT_UNKNOWN;
}

void tstate_argument(tstate *state, char *arg)
{
    fprintf(stderr, "nhfilter: expected no arguments\n");
    exit(1);
}

void tstate_ready(tstate *state, double *idelay, double *odelay)
{
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags)
{
    char *ret;
    char *p;

    ret = malloc(inlen+1);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    if (input) {
	p = ret;

	if (flags & EV_TIMEOUT) {
	    /*
	     * We have timed out after returning a positive delay
	     * value after processing a previous piece of input. This
	     * means that our last piece of input ended with an ESC,
	     * and we've given up waiting for a following character,
	     * so we're going to output the ESC on its own.
	     */
	    if (state->seen_esc) {
		*p++ = '\033';
		state->seen_esc = FALSE;
	    }
	}

	while (inlen > 0) {
	    char c;
	    inlen--;
	    c = *data++;

	    if (state->seen_esc) {
		state->seen_esc = FALSE;

		/*
		 * Process the character straight after an ESC.
		 */
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
		    /*
		     * ESC+alphabetic becomes the alphabetic with
		     * high bit set.
		     */
		    *p++ = c | 0x80;
		    continue;
		} else {
		    /*
		     * Anything else, we output the ESC we're
		     * holding on to, and fall through to normal
		     * behaviour.
		     */
		    *p++ = '\e';
		}
	    }

	    if (c == '\e')
		state->seen_esc = TRUE;
	    else
		*p++ = c;
	}

	*outlen = p - ret;
	if (state->seen_esc)
	    *delay = 0.05;	       /* wait 1/20 s for letter after ESC */
	else
	    *delay = 0.0;
    } else {
        memcpy(ret, data, inlen);
	*outlen = inlen;
	*delay = 0.0;
    }

    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
