/*
 * misc.c - miscellaneous utility routines for Caltrap
 */

#include <string.h>
#include <assert.h>
#include "caltrap.h"

static const char *const typenames[] = {
    "EVENT", "HOL1", "HOL2", "HOL3", "HOL4", "TODO"
};

int name_to_type(const char *name)
{
    int i;
    for (i = 0; i < lenof(typenames); i++)
	if (!strcmp(typenames[i], name))
	    return i;
    fatal(err_dbconsist, "invalid-type-name");
}

const char *type_to_name(int type)
{
    assert(type >= 0 && type < lenof(typenames));
    return typenames[type];
}
