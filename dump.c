/*
 * dump.c - ability to dump out the Caltrap database and reload it
 */

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "caltrap.h"

static void dump_callback(void *vctx, struct entry *ent)
{
    char *dfmt1, *dfmt2, *tfmt1, *tfmt2, *len, *per;

    dfmt1 = format_date(ent->sd);
    tfmt1 = format_time(ent->st);
    dfmt2 = format_date(ent->ed);
    tfmt2 = format_time(ent->et);
    len = format_duration(ent->length);
    per = format_duration(ent->period);

    /*
     * Forwards compatibility with what will be the new database
     * schema: our fields here are ID, start date, start time, end
     * date, end time, length, repeat period, type and message.
     */
    printf("%d %s %s %s %s %s %s %s %s\n", ent->id,
	   dfmt1, tfmt1, dfmt2, tfmt2, len, per, type_to_name(ent->type),
	   ent->description);
    sfree(dfmt1);
    sfree(tfmt1);
    sfree(dfmt2);
    sfree(tfmt2);
    sfree(len);
    sfree(per);
}

void caltrap_dump(int nargs, char **args, int nphysargs)
{
    if (nargs > 0)
	fatalerr_dumpargno();
    assert(nargs <= nphysargs);

    db_dump_entries(dump_callback, NULL);
}

/*
 * Read an entire line of text from a file. Return a buffer
 * malloced to be as big as necessary (caller must free).
 */
static char *fgetline(FILE *fp)
{
    char *ret = smalloc(512);
    int size = 512, len = 0;
    while (fgets(ret + len, size - len, fp)) {
	len += strlen(ret + len);
	if (ret[len-1] == '\n')
	    break;		       /* got a newline, we're done */
	size = len + 512;
	ret = srealloc(ret, size);
    }
    if (len == 0) {		       /* first fgets returned NULL */
	sfree(ret);
	return NULL;
    }
    ret[len-1] = '\0';
    return ret;
}

void caltrap_load(int nargs, char **args, int nphysargs)
{
    FILE *fp;
    char *text;
    int is_stdin;

    if (nargs > 1)
	fatalerr_loadargno();
    assert(nargs <= nphysargs);

    db_init();

    if (nargs == 0) {
	is_stdin = TRUE;
	fp = stdin;
    } else {
	is_stdin = FALSE;
	fp = fopen(args[0], "r");
    }

    while ( (text = fgetline(fp)) != NULL ) {
	char *p, *q;
	struct entry ent;

	p = text;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the id */
	ent.id = atoi(p);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the start date */
	ent.sd = parse_date(p);
	if (ent.sd == INVALID_DATE)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the start time */
	ent.st = parse_time(p);
	if (ent.st == INVALID_TIME)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the end date */
	ent.ed = parse_date(p);
	if (ent.ed == INVALID_DATE)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the end time */
	ent.et = parse_time(p);
	if (ent.et == INVALID_TIME)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the length */
	ent.length = parse_duration(p);
	if (ent.length == INVALID_DURATION)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the repeat period */
	ent.period = parse_duration(p);
	if (ent.period == INVALID_DURATION)
	    fatalerr_loadfmt();
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) *q++ = '\0';
	/* p now points to the type */
	ent.type = name_to_type(p);
	if (ent.type == INVALID_TYPE)
	    fatalerr_loadfmt();
	p = q;

	/*
	 * Now p points to the message, so we can go ahead and add
	 * the entry to our database.
	 */
	ent.description = p;
	db_add_entry(&ent);
    }

    if (!is_stdin)
	fclose(fp);
}
