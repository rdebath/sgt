/*
 * cstrans.c - translation module for a filter which converts
 * between character sets using libcharset.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

#include "charset.h"

struct tstate {
    int nargs;
    int incset;			       /* charset used inside the wrapper */
    int outcset;		       /* charset used outside the wrapper */
    charset_state instate1;	       /* for input (outcset -> Unicode) */
    charset_state instate2;	       /* for input (Unicode -> incset) */
    charset_state outstate1;	       /* for output (incset -> Unicode) */
    charset_state outstate2;	       /* for output (Unicode -> outcset) */
    int escapes;           /* send ESC % G / ESC % @ around session */
    int sent_start_escape;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->incset = charset_from_locale();
    state->outcset = CS_UTF8;

    state->instate1 = state->outstate1 = charset_init_state;
    state->instate2 = state->outstate2 = charset_init_state;

    state->nargs = 0;

    state->escapes = state->sent_start_escape = FALSE;

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    if (shortopt == 'e') {
        state->escapes = TRUE;
        return OPT_OK;
    }
    return OPT_UNKNOWN;
}

void tstate_argument(tstate *state, char *arg)
{
    int cset;

    if (state->nargs >= 2) {
	fprintf(stderr, "csfilter: expected at most two character set"
		" arguments\n");
	exit(1);
    }

    cset = charset_from_localenc(arg);

    if (cset == CS_NONE) {
	fprintf(stderr, "csfilter: unrecognised character set '%s'\n", arg);
	exit(1);
    }

    if (state->nargs == 0)
	state->incset = cset;
    else
	state->outcset = cset;

    state->nargs++;
}

void tstate_ready(tstate *state, double *idelay, double *odelay)
{
}

char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags)
{
    char *ret;
    int retsize, retlen, inret, midlen, midret;
    int fromcs, tocs;
    charset_state *state1, *state2;
    wchar_t midbuf[256];
    const char *inptr;
    const wchar_t *midptr;

    retsize = inlen + 512;
    ret = malloc(retsize);
    if (!ret) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    retlen = 0;
    inptr = data;

    if (input) {
	fromcs = state->outcset;
	tocs = state->incset;
	state1 = &state->instate1;
	state2 = &state->instate2;
    } else {
	fromcs = state->incset;
	tocs = state->outcset;
	state1 = &state->outstate1;
	state2 = &state->outstate2;

        /*
         * Hacky: we assume that the lengths of these escape sequences
         * will not add up to more than the initial buffer size we
         * allocated above (which is at least 512).
         */
        if (!state->sent_start_escape) {
            state->sent_start_escape = TRUE;

            if (state->escapes && state->outcset == CS_UTF8)
                retlen += sprintf(ret + retlen, "\033%%G");
        }
        if (flags & EV_EOF) {
            if (state->escapes && state->outcset == CS_UTF8)
                retlen += sprintf(ret + retlen, "\033%%@");
        }
    }

    while ( (inret = charset_to_unicode(&inptr, &inlen, midbuf,
					lenof(midbuf), fromcs,
					state1, NULL, 0)) > 0) {
	midlen = inret;
	midptr = midbuf;
	while ( (midret = charset_from_unicode(&midptr, &midlen, ret+retlen,
					       retsize - retlen, tocs,
					       state2, NULL)) > 0) {
	    retlen += midret;
	    if (retsize - retlen < 128) {
		retsize = (retsize * 3) / 2;
		ret = realloc(ret, retsize);
		if (!ret) {
		    fprintf(stderr, "filter: out of memory!\n");
		    exit(1);
		}
	    }
	}
    }

    *outlen = retlen;
    *delay = 0.0;
    return ret;
}

void tstate_done(tstate *state)
{
    free(state);
}
