#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "axe.h"

static DFA dfa = NULL;
static char *tmp = NULL;
static int dfa_size = 0, dfa_len = 0;

DFA build_dfa (char *pattern, int len) {
    int i, j, k, b;

    if (dfa_size < len) {
	dfa_size = len;
	dfa = (dfa ? realloc(dfa, dfa_size * sizeof(*dfa)) :
	       malloc(dfa_size * sizeof(*dfa)));
	if (!dfa)
	    return NULL;
	tmp = (tmp ? realloc(tmp, dfa_size) : malloc(dfa_size));
	if (!tmp)
	    return NULL;
    }

    memcpy (tmp, pattern, len);

    for (i=len; i-- ;) {
	j = i+1;
	for (b=0; b<256; b++) {
	    dfa[i][b] = 0;
	    if (memchr(pattern, b, len)) {
		tmp[j-1] = b;
		for (k=1; k<=j; k++)
		    if (!memcmp(tmp+j-k, pattern, k))
			dfa[i][b] = k;
	    }
	}
    }
    dfa_len = len;
    return dfa;
}

DFA last_dfa (void) {
    return dfa;
}

int last_len (void) {
    return dfa_len;
}
