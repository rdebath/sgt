/*
 * dump.c - ability to dump out the Caltrap database and reload it
 */

#include <assert.h>
#include <stdio.h>
#include "caltrap.h"

struct dump_ctx {
    int id;
};

static void dump_callback(void *vctx, Date d, Time t, char *msg)
{
    struct dump_ctx *ctx = (struct dump_ctx *)vctx;

    char *dfmt1, *dfmt2, *tfmt1, *tfmt2;
    if (t != NO_TIME) {
	dfmt1 = format_date(d);
	tfmt1 = format_time(t);
	dfmt2 = format_date(d);
	tfmt2 = format_time(t);
    } else {
	dfmt1 = format_date(d);
	tfmt1 = format_time(0);
	dfmt2 = format_date(d+1);
	tfmt2 = format_time(0);
    }
    /*
     * Forwards compatibility with what will be the new database
     * schema: our fields here are ID, start date, start time, end
     * date, end time, length, repeat period, type and message.
     */
    printf("%d %s %s %s %s - - EVENT %s\n", ctx->id++, 
	   dfmt1, tfmt1, dfmt2, tfmt2, msg);
    sfree(dfmt1);
    sfree(tfmt1);
    sfree(dfmt2);
    sfree(tfmt2);
}

int caltrap_dump(int nargs, char **args, int nphysargs)
{
    struct dump_ctx ctx;

    if (nargs > 0)
	fatal(err_dumpargno);
    assert(nargs <= nphysargs);

    ctx.id = 0;
    db_dump_entries(dump_callback, &ctx);
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

int caltrap_load(int nargs, char **args, int nphysargs)
{
    FILE *fp;
    char *text;
    int is_stdin;

    if (nargs > 1)
	fatal(err_loadargno);
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
	Date d1, d2;
	Time t1, t2;

	p = text;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the id; we ignore */
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the start date */
	d1 = parse_date(p);
	if (d1 == INVALID_DATE)
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the start time */
	t1 = parse_time(p);
	if (t1 == INVALID_TIME)
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the end date */
	d2 = parse_date(p);
	if (d2 == INVALID_DATE)
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the end time */
	t2 = parse_time(p);
	if (t2 == INVALID_TIME)
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the length; we expect `-' for none */
	if (!strcmp(p, "-"))
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the repeat period; we expect `-' for none */
	if (!strcmp(p, "-"))
	    fatal(err_loadfmt);
	p = q;

	q = p; while (*q && !isspace(*q)) q++; if (*q) q++;
	/* p now points to the type; we expect "EVENT" */
	if (!strcmp(p, "EVENT"))
	    fatal(err_loadfmt);
	p = q;

	/*
	 * Now p points to the message, so we can go ahead and add
	 * the entry to our database.
	 */
	if (d2 == d1+1 && t1 == 0 && t2 == 0) {
	    db_add_entry(d1, NO_TIME, p);
	} else if (d2 == d1 && t2 == t1) {
	    db_add_entry(d1, t1, p);
	} else {
	    fatal(err_loadfmt);
	}
    }

    if (!is_stdin)
	fclose(fp);
}
