/*
 * list.c - add an entry to a Caltrap calendar
 */

#include <assert.h>
#include <stdio.h>
#include "caltrap.h"

struct list_ctx {
    Date last;
};

static void list_print(Date d, Time t, char *msg)
{
    char *dfmt, *tfmt;
    dfmt = format_date_full(d);
    tfmt = format_time(t);
    printf("%s %s %s\n", dfmt, tfmt, msg);
    sfree(dfmt);
    sfree(tfmt);
}

static void list_upto(struct list_ctx *ctx, Date d)
{
    Date dd;
    for (dd = ctx->last + 1; dd < d; dd++)
	list_print(dd, NO_TIME, "");
}

static void list_callback(void *vctx, Date d, Time t, char *msg)
{
    struct list_ctx *ctx = (struct list_ctx *)vctx;

    list_upto(ctx, d);
    list_print(ctx->last == d ? NO_DATE : d, t, msg);

    ctx->last = d;
}

int list_entries(Date sd, Date ed)
{
    struct list_ctx ctx;
    ctx.last = sd - 1;
    db_list_entries(sd, 0, ed, 0, list_callback, &ctx);
    list_upto(&ctx, ed);
}

int caltrap_list(int nargs, char **args, int nphysargs)
{
    Date sd, ed;
    Time t;

    if (nargs > 2)
	fatal(err_listargno);
    assert(nargs <= nphysargs);

    if (nargs == 0) {
	sd = today();
	ed = sd + 21;		       /* three weeks */
    } else if (nargs == 1) {
	sd = parse_date(args[0]);
	if (sd == INVALID_DATE)
	    fatal(err_date, args[0]);
	ed = sd;
    } else if (nargs == 2) {
	sd = parse_date(args[0]);
	if (sd == INVALID_DATE)
	    fatal(err_date, args[0]);
	ed = parse_date(args[1]);
	if (ed == INVALID_DATE)
	    fatal(err_date, args[1]);
	if (sd > ed) {
	    Date tmp = sd; sd = ed; ed = tmp;
	}
    }

    list_entries(sd, ed);
}
