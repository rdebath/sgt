/*
 * record.c - translation module for a filter which records output.
 * Equivalent to any of `script', `ttyrec' or `nh-recorder',
 * depending on options, and has a more sensible invocation syntax
 * than either.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "filter.h"

#define PUT_32BIT_LSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)(value), \
  (cp)[1] = (unsigned char)((value) >> 8), \
  (cp)[2] = (unsigned char)((value) >> 16), \
  (cp)[3] = (unsigned char)((value) >> 24) )

struct tstate {
    char *outfile;
    enum { TTYREC, NHREC, SCRIPT } mode;
    FILE *fp;
    int started;
    int realtime;
    long long first_timestamp;
};

tstate *tstate_init(void)
{
    tstate *state;

    state = (tstate *)malloc(sizeof(tstate));
    if (!state) {
	fprintf(stderr, "filter: out of memory!\n");
	exit(1);
    }

    state->outfile = NULL;
    state->mode = SCRIPT;
    state->started = 0;
    state->realtime = 0;

    return state;
}

int tstate_option(tstate *state, int shortopt, char *longopt, char *value)
{
    if (longopt) {
	if (!strcmp(longopt, "ttyrec")) {
	    shortopt = 'T';
	} else if (!strcmp(longopt, "script")) {
	    shortopt = 'S';
	} else if (!strcmp(longopt, "nhrecorder") ||
		   !strcmp(longopt, "nh-recorder") ||
		   !strcmp(longopt, "nh_recorder") ||
		   !strcmp(longopt, "nhrecording") ||
		   !strcmp(longopt, "nh-recording") ||
		   !strcmp(longopt, "nh_recording")) {
	    shortopt = 'N';
	} else
	    return OPT_UNKNOWN;
    }

    if (shortopt == 'S') {
	if (value)
	    return OPT_SPURIOUSARG;
	state->mode = SCRIPT;
    } else if (shortopt == 'T') {
	if (value)
	    return OPT_SPURIOUSARG;
	state->mode = TTYREC;
    } else if (shortopt == 'N') {
	if (value)
	    return OPT_SPURIOUSARG;
	state->mode = NHREC;
    } else if (shortopt == 'r') {
	if (value)
	    return OPT_SPURIOUSARG;
	state->realtime = 1;
    } else if (shortopt == 'o') {
	if (!value)
	    return OPT_MISSINGARG;
	state->outfile = value;
    } else
	return OPT_UNKNOWN;

    return OPT_OK;
}

void tstate_argument(tstate *state, char *arg)
{
    state->outfile = arg;
}

void tstate_ready(tstate *state, double *idelay, double *odelay)
{
    if (!state->outfile) {
	fprintf(stderr, "record: no output file name specified\n");
	exit(1);
    }
    state->fp = fopen(state->outfile, "wb");
    if (!state->fp) {
	fprintf(stderr, "record: unable to open '%s': %s\n",
		state->outfile, strerror(errno));
	exit(1);
    }
}

static void nhr_timestamp(tstate *state)
{
    struct timeval tv;
    long long timestamp;

    gettimeofday(&tv, NULL);
    timestamp = tv.tv_sec;
    timestamp = timestamp * 1000000 + tv.tv_usec;

    if (state->started) {
	/*
	 * Output nh-recorder timing mark: a NUL followed by a
	 * 4-byte centisecond timestamp.
	 */
	char buf[5];
	int cs;

	buf[0] = '\0';
	cs = (timestamp - state->first_timestamp) / 10000;
	PUT_32BIT_LSB_FIRST(buf+1, cs);
	fwrite(buf, 1, 5, state->fp);
    } else {
	state->first_timestamp = timestamp;
	state->started = 1;
    }
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

    if (input)
	return ret;		       /* we don't touch input data */

    if (state->mode == TTYREC) {
	struct timeval tv;
	long long timestamp;
	char buf[12];

	/*
	 * Output ttyrec 12-byte header containing block length and
	 * timestamp.
	 */
	gettimeofday(&tv, NULL);
	timestamp = tv.tv_sec;
	timestamp = timestamp * 1000000 + tv.tv_usec;
	if (!state->started) {
	    state->first_timestamp = timestamp;
	    state->started = 1;
	}
	if (!state->realtime) {
	    /*
	     * ttyrec itself writes these timestamps using the
	     * genuine _absolute_ time of the event, so that a
	     * ttyrec file contains internal information which
	     * could be used to determine the date and time when
	     * the action took place. I can conceive of that
	     * information being a privacy leak, so by default I'm
	     * going to normalise all _my_ ttyrecs so that they
	     * pretend to start at epoch + 0x10000000 seconds.
	     */
	    timestamp -= state->first_timestamp;
	    timestamp += 0x10000000LL * 1000000LL;
	}
	tv.tv_sec = timestamp / 1000000;
	tv.tv_usec = timestamp % 1000000;
	PUT_32BIT_LSB_FIRST(buf+0, tv.tv_sec);
	PUT_32BIT_LSB_FIRST(buf+4, tv.tv_usec);
	PUT_32BIT_LSB_FIRST(buf+8, inlen);
	fwrite(buf, 1, 12, state->fp);
    } else if (state->mode == NHREC) {
	int i, j;

	/*
	 * nh-recorder has a fundamental inability to store NULs.
	 * We will filter them out of the data we pass through to
	 * the client as well as the data we write to the logfile,
	 * in the interests of detecting trouble early rather than
	 * late.
	 */
	for (i = j = 0; i < inlen; i++)
	    if (ret[i])
		ret[j++] = ret[i];
	*outlen = j;

	nhr_timestamp(state);
    }

    /*
     * Log the actual data.
     */
    fwrite(ret, 1, *outlen, state->fp);

    return ret;
}

void tstate_done(tstate *state)
{
    /*
     * In nh-recorder mode, output the final timestamp.
     */
    if (state->mode == NHREC)
	nhr_timestamp(state);

    fclose(state->fp);
    free(state);
}
