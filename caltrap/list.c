/*
 * list.c - add an entry to a Caltrap calendar
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "caltrap.h"
#include "tree234.h"

struct list_ctx {
    Date cd, last_date_printed, colour_date;
    Time ct;
    tree234 *future;
    char *day_str, *end_str;
    int line_width;
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
    else if (a->type < b->type)
	return -1;
    else if (a->type > b->type)
	return +1;
    else if (a->id < b->id)
	return -1;
    else if (a->id > b->id)
	return +1;
    else
	return 0;
}

static void print_line(struct list_ctx *ctx, char *fmt, ...)
{
    va_list ap;
    int n;

    if (ctx->cd != ctx->colour_date) {
	ctx->day_str = ctx->end_str = "";
    }

    printf("%s", ctx->day_str);

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);

    printf("%*s%s\n", (ctx->line_width > n ? ctx->line_width - n : 0), "",
	   ctx->end_str);
}

static void list_print(struct list_ctx *ctx, struct entry *ent)
{
    char *dfmt, *tfmt;

    if (ent->type == T_TODO || !*ent->description)
	return;

    assert(ctx->last_date_printed == ent->sd ||
	   ctx->last_date_printed == ent->sd - 1);

    if (ctx->last_date_printed != ent->sd) {
	dfmt = format_date_full(ent->sd);
	ctx->last_date_printed = ent->sd;
    } else {
	dfmt = smalloc(20);
	strcpy(dfmt, "              ");
    }

    /*
     * Special case in time formatting: an all-day or
     * all-multiple-days entry gets no time printed.
     */
    if (ent->st == 0 && ent->et == 0 && ent->ed > ent->sd) {
	tfmt = smalloc(20);
	strcpy(tfmt, "        ");
    } else {
	tfmt = format_time(ent->st);
    }
    print_line(ctx, "%s %s %s", dfmt, tfmt, ent->description);
    sfree(dfmt);
    sfree(tfmt);
}

static void list_upto(struct list_ctx *ctx, Date ed, Time et)
{
    struct entry *ent;
    Date prevdate = NO_DATE;

    while (datetime_cmp(ctx->cd, ctx->ct, ed, et) < 0) {
	Date nd = ed;
	Time nt = et;

	ent = index234(ctx->future, 0);
	if (ent && datetime_cmp(ent->sd, ent->st, ed, et) < 0) {
	    nd = ent->sd;
	    nt = ent->st;
	} else
	    ent = NULL;

	while (ctx->last_date_printed < nd-1) {
	    char *dfmt = format_date_full(++ctx->last_date_printed);
	    ctx->cd = ctx->last_date_printed;
	    ctx->ct = 0;
	    print_line(ctx, "%s", dfmt);
	    sfree(dfmt);
	}

	ctx->cd = nd;
	ctx->ct = nt;

	if (ent) {
	    /*
	     * If this is the first entry we have seen for this
	     * date, and it's a holiday entry, we use it to
	     * determine the background colour for this date.
	     */
	    if (nd != prevdate) {
		prevdate = nd;
		if (is_hol(ent->type) && isatty(fileno(stdout)) &&
		    is_ecma_term()) {
		    /*
		     * terminfo appears to be a pest to fly
		     * properly, so I'm going to cheat a little and
		     * deal only with ECMA-48 terminals. However, I
		     * make the one concession of checking whether
		     * the terminal is something weird and
		     * abandoning all use of colour if so.
		     */
		    ctx->end_str = "\033[K\033[39;49;0m";
		    switch (ent->type) {
		      case T_HOL1: ctx->day_str = "\033[44;37m"; break;
		      case T_HOL2: ctx->day_str = "\033[46;30m"; break;
		      case T_HOL3: ctx->day_str = "\033[42;30m"; break;
		    }
		    ctx->colour_date = nd;
		}
	    }

	    del234(ctx->future, ent);
	    list_print(ctx, ent);

	    if (is_hol(ent->type)) {
		struct entry *hol;

		/*
		 * We will need to see holiday events repeatedly
		 * throughout their run, so that we colour every
		 * day involved. Mostly we do this by incrementing
		 * the start date on the original entry and
		 * re-adding it; but if it's a repeating entry then
		 * we will also need to process it for its next
		 * repetition so we'll have to make a copy now.
		 */
		if (ent->period) {
		    hol = smalloc(sizeof(struct entry));
		    *hol = *ent;    /* structure copy */
		    /* Also here we must remove its periodicity. */
		    hol->period = 0;
		    hol->ed = hol->sd;
		    hol->et = hol->st;
		    add_to_datetime(&hol->ed, &hol->et, hol->length);
		    hol->length = 0;
		} else
		    hol = ent;

		hol->description = "";   /* prevent recurring text entry */
		hol->sd++;
		if (datetime_cmp(hol->sd, hol->st, hol->ed, hol->et) < 0)
		    add234(ctx->future, hol);
	    }

	    if (ent->period) {
		add_to_datetime(&ent->sd, &ent->st, ent->period);
		if (datetime_cmp(ent->sd, ent->st, ent->ed, ent->et) <= 0) {
		    /*
		     * This repeating entry has not come to the end
		     * of its repeat period yet, so re-add it.
		     */
		    add234(ctx->future, ent);
		} else {
		    /*
		     * This one is finished with; destroy it rather
		     * than putting it back in the pending tree.
		     */
		    sfree(ent);
		}
	    }
	}
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
    dupent = smalloc(sizeof(struct entry) + 1 + strlen(ent->description));
    *dupent = *ent;		       /* structure copy */
    dupent->description = (char *)dupent + sizeof(struct entry);
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
    ctx.colour_date = NO_DATE;
    ctx.future = newtree234(entry_cmp);
    ctx.day_str = ctx.end_str = "";
    ctx.line_width = get_line_width();

    db_list_entries(sd, st, ed, et, list_callback, &ctx);
    list_upto(&ctx, ed, et);

    while ((ent = delpos234(ctx.future, 0)) != NULL)
	sfree(ent);
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
