/*
 * config.c: Timber default configuration values
 */

#include <assert.h>
#include <string.h>
#include "timber.h"

int cfg_default_int(char *key)
{
    static const struct {
	const char *key;
	int value;
    } defaults[] = {
	{"current-mbox", 0},
	{"mbox-maxsize", 16777216},    /* use 16Mb mboxes by default */
    };
    int i, j, k, c;

    i = -1;
    k = lenof(defaults);
    while (k - i > 1) {
	j = (i + k) / 2;
	c = strcmp(key, defaults[j].key);
	if (c > 0)
	    i = j;
	else if (c < 0)
	    k = j;
	else
	    return defaults[j].value;
    }

    assert(!"We shouldn't have got here");
    return -1;
}

char *cfg_default_str(char *key)
{
    assert(!"We shouldn't have got here");
    return NULL;
}
