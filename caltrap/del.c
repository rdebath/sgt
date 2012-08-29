/*
 * del.c - delete a Caltrap calendar entry
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "caltrap.h"

void caltrap_del(int nargs, char **args, int nphysargs)
{
    int id;

    if (nargs != 1)
	fatalerr_delargno();
    assert(nargs <= nphysargs);

    id = atoi(args[0]);

    db_del(id);
}
