/*
 * list.c - add an entry to a Caltrap calendar
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "caltrap.h"
#include "tree234.h"

struct list_ctx {
    Date cd, last_date_printed;
    Time ct;
    tree234 *future;
};

static int entry_cmp(void *av, void *bv)
{
    struct entry *a = (struct entry *)av;
    struct entry *b = (struct entry *)bv;

    if (a->sd < b->sd)
	return -1;
    else if (a->sd > b->sd)
	return +1;
    else if (a->st < b->st)
	return -1;
    else if (a->st > b->st)
	return +1;
    else if (a->id < b->id)
	return -1;
    else if (a->id > b->id)
	return +1;
    else
	return 0;
}

static void list_print(struct list_ctx *ctx, struct entry *ent)
{
    char *dfmt, *tfmt;

    if (ent->type != T_EVENT)
	return;			       /* currently we ignore all others */

    assert(ctx->last_date_printed == ent->sd ||
	   ctx->last_date_printed == ent->sd - 1);

    if (ctx->last_date_printed != ent->sd) {
	dfmt = format_date_full(ent->sd);
	ctx->last_date_printed = ent->sd;
    } else {
	dfmt = smalloc(20);
	strcpy(dfmt, "              ");
    }

    tfmt = format_time(ent->st);
    printf("%s %s %s\n", dfmt, tfmt, ent->description);
    sfree(dfmt);
    sfree(tfmt);
}

static void list_upto(struct list_ctx *ctx, Date ed, Time et)
{
    struct entry *ent;

    while (ctx->cd < ed || (ctx->cd == ed && ctx->ct < et)) {
	Date nd = ed;
	Time nt = et;

	ent = index234(ctx->future, 0);
	if (ent && (ent->sd < ed || (ent->sd == ed && ent->st < et))) {
	    nd = ent->sd;
	    nt = ent->st;
	} else
	    ent = NULL;

	while (ctx->last_date_printed < nd-1) {
	    char *dfmt = format_date_full(++ctx->last_date_printed);
	    printf("%s\n", dfmt);
	    sfree(dfmt);
	}

	if (ent) {
	    del234(ctx->future, ent);
	    list_print(ctx, ent);
	    add_to_datetime(&ent->sd, &ent->st, ent->period);
	    if (ent->period &&
		(ent->sd < ent->ed ||
		 (ent->sd == ent->ed && ent->st <= ent->et))) {
		/*
		 * This repeating entry has not come to the end of
		 * its repeat period yet, so re-add it.
		 */
		add234(ctx->future, ent);
	    } else {
		/*
		 * This one is finished with; destroy it rather
		 * than putting it back in the pending tree.
		 */
		sfree(ent->description);
		sfree(ent);
	    }
	}

	ctx->cd = nd;
	ctx->ct = nt;
    }
}

static void list_callback(void *vctx, struct entry *ent)
{
    struct list_ctx *ctx = (struct list_ctx *)vctx;
    struct entry *dupent;

    /*
     * If this is a repeating event which started before our list
     * period, we must begin by bringing it up to date.
     */
    if (datetime_cmp(ent->sd, ent->st, ctx->cd, ctx->ct) < 0) {
	if (ent->period > 0) {
	    Duration d = datetime_diff(ctx->cd, ctx->ct, ent->sd, ent->st);
	    d = ((d + ent->period - 1) / ent->period) * ent->period;
	    add_to_datetime(&ent->sd, &ent->st, d);
	    assert(datetime_cmp(ent->sd, ent->st, ctx->cd, ctx->ct) >= 0);
	} else
	    /*
	     * This is an ordinary event which started before our
	     * search begins. Ignore it.
	     */
	    return;
    }

    /*
     * Now add this to the list of future events.
     */
    dupent = smalloc(sizeof(struct entry));
    *dupent = *ent;		       /* structure copy */
    dupent->description = smalloc(1+strlen(ent->description));
    strcpy(dupent->description, ent->description);
    add234(ctx->future, dupent);
}

void list_entries(Date sd, Time st, Date ed, Time et)
{
    struct list_ctx ctx;
    struct entry *ent;

    ctx.cd = sd;
    ctx.ct = st;
    ctx.last_date_printed = sd - 1;
    ctx.future = newtree234(entry_cmp);

    db_list_entries(sd, st, ed, et, list_callback, &ctx);
    list_upto(&ctx, ed, et);

    while ((ent = delpos234(ctx.future, 0)) != NULL) {
	sfree(ent->description);
	sfree(ent);
    }
    freetree234(ctx.future);
}

void caltrap_list(int nargs, char **args, int nphysargs)
{
    Date sd, ed;

    if (nargs > 2)
	fatal(err_listargno);
    assert(nargs <= nphysargs);

    if (nargs == 0) {
	sd = today();
	ed = sd + 14;		       /* two weeks */
    } else if (nargs == 1) {
	sd = parse_date(args[0]);
	if (sd == INVALID_DATE)
	    fatal(err_date, args[0]);
	ed = sd + 1;
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

    list_entries(sd, 0, ed, 0);
}
