/*
 * edit.c - modify a Caltrap calendar entry
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "caltrap.h"

void caltrap_edit(int nargs, char **args, int nphysargs, struct entry *e)
{
    if (nargs != 1)
	fatalerr_editargno();
    assert(nargs <= nphysargs);

    e->id = atoi(args[0]);

    db_update(e);
}
