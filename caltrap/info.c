/*
 * info.c - display full information on a Caltrap calendar entry
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "caltrap.h"

void caltrap_info(int nargs, char **args, int nphysargs)
{
    int id;
    struct entry ent;
    char *dfmt, *tfmt;

    if (nargs != 1)
	fatalerr_infoargno();
    assert(nargs <= nphysargs);

    id = atoi(args[0]);

    db_fetch(id, &ent);

    printf("id: %d\n", ent.id);
    dfmt = format_date_full(ent.sd);
    tfmt = format_time(ent.st);
    printf("start: %s %s\n", dfmt, tfmt);
    sfree(dfmt);
    sfree(tfmt);
    dfmt = format_date_full(ent.ed);
    tfmt = format_time(ent.et);
    printf("end: %s %s\n", dfmt, tfmt);
    sfree(dfmt);
    sfree(tfmt);
    if (ent.period) {
	dfmt = format_duration(ent.length);
	printf("length: %s\n", dfmt);
	sfree(dfmt);
	dfmt = format_duration(ent.period);
	printf("period: %s\n", dfmt);
	sfree(dfmt);
    }
    printf("type: %s\n", type_to_name(ent.type));
    printf("description: %s\n", ent.description);

    sfree(ent.description);
}
