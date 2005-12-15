#ifndef FILTER_FILTER_H
#define FILTER_FILTER_H

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define FALSE 0
#define TRUE 1

typedef struct tstate tstate;

int pty_get(char *name);

tstate *tstate_init(void);
void tstate_argument(tstate *state, char *arg);
char *translate(tstate *state, char *data, int inlen, int *outlen,
		double *delay, int input);

#endif
