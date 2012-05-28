#ifndef FILTER_FILTER_H
#define FILTER_FILTER_H

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define FALSE 0
#define TRUE 1

typedef struct tstate tstate;

int pty_get(char *name);
int pty_resize(int fd);

/* Return values from tstate_option */
#define OPT_UNKNOWN -1
#define OPT_SPURIOUSARG -2
#define OPT_MISSINGARG -3
#define OPT_OK 0

/* Special flags passed in to translate to indicate events. Always
 * accompanied with inlen==0. */
#define EV_TIMEOUT 1
#define EV_EOF 2

tstate *tstate_init(void);
int tstate_option(tstate *state, int shortopt, char *longopt, char *value);
void tstate_argument(tstate *state, char *arg);
void tstate_ready(tstate *state, double *idelay, double *odelay);
char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input, int flags);
void tstate_done(tstate *state);

#endif
