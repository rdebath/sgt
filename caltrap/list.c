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
    char *day_str, *clr_str, *end_str;
    int line_width;
    int verbose;
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
	ctx->day_str = ctx->clr_str = ctx->end_str = "";
    }

    printf("%s", ctx->day_str);

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);

    printf("%s%*s%s\n", ctx->clr_str,
	   (*ctx->end_str && ctx->line_width > n ? ctx->line_width - n : 0),
	   "", ctx->end_str);
}

static void list_print(struct list_ctx *ctx, struct entry *ent)
{
    char *dfmt, *tfmt;

    if (ent->type == T_TODO || !ent->description ||
	(!ctx->verbose && !*ent->description))
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
    if (ctx->verbose) {
	print_line(ctx, "%s %s [%d] %s", dfmt, tfmt, ent->id,
		   ent->description);
    } else {
	print_line(ctx, "%s %s %s", dfmt, tfmt, ent->description);
    }
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
		    ctx->clr_str = "\033[K";
		    ctx->end_str = "\033[39;49;0m";
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

		hol->description = NULL;/* prevent recurring text entry */
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

static struct entry *dup_entry(struct entry *ent)
{
    struct entry *dupent;
    int desclen;

    if (ent->description)
	desclen = 1 + strlen(ent->description);
    else
	desclen = 0;
    dupent = smalloc(sizeof(struct entry) + desclen);
    *dupent = *ent;		       /* structure copy */
    if (ent->description) {
	dupent->description = (char *)dupent + sizeof(struct entry);
	strcpy(dupent->description, ent->description);
    } else
	dupent->description = NULL;

    return dupent;
}

static void list_callback(void *vctx, struct entry *ent)
{
    struct list_ctx *ctx = (struct list_ctx *)vctx;

    /*
     * If this is a repeating event, or a holiday event, which
     * started before our list period, we must begin by bringing it
     * up to date.
     */
    if (datetime_cmp(ent->sd, ent->st, ctx->cd, ctx->ct) < 0) {
	if (ent->period > 0) {
	    Duration d;

	    /*
	     * If this is a repeating _holiday_ event, we might
	     * also require its first occurrence whose _end_ date
	     * falls within the list period. For example, starting
	     * a calendar listing on a Sunday should certainly
	     * highlight it as being weekend.
	     */
	    if (is_hol(ent->type)) {
		struct entry *hol = dup_entry(ent);
		d = datetime_diff(ctx->cd, ctx->ct, hol->sd, hol->st);
		d -= hol->length;
		d = ((d + hol->period - 1) / hol->period) * hol->period;
		add_to_datetime(&hol->sd, &hol->st, d);

		/*
		 * Now de-repeatify it.
		 */
		hol->period = 0;
		hol->ed = hol->sd;
		hol->et = hol->st;
		add_to_datetime(&hol->ed, &hol->et, hol->length);
		hol->length = 0;

		/*
		 * Now if this entry's end date is strictly within
		 * the list period (i.e. not merely coincident with
		 * its beginning), and its start date is _before_
		 * the start of the list period, we need to nudge
		 * this duplicate entry's start point up to the
		 * list period beginning and add it into our queue.
		 */
		if (datetime_cmp(hol->sd, hol->st, ctx->cd, ctx->ct) < 0 &&
		    datetime_cmp(hol->ed, hol->et, ctx->cd, ctx->ct) > 0) {
		    hol->sd = ctx->cd;
		    hol->st = ctx->ct;
		    hol->description = NULL;/* prevent recurring text entry */
		    add234(ctx->future, hol);
		} else
		    sfree(hol);
	    }

	    /*
	     * Find the first occurrence of the repeating event
	     * whose start date falls within the list period.
	     */
	    d = datetime_diff(ctx->cd, ctx->ct, ent->sd, ent->st);
	    d = ((d + ent->period - 1) / ent->period) * ent->period;
	    add_to_datetime(&ent->sd, &ent->st, d);
	    assert(datetime_cmp(ent->sd, ent->st, ctx->cd, ctx->ct) >= 0);
	} else if (is_hol(ent->type)) {
	    ent->sd = ctx->cd;
	    ent->st = ctx->ct;
	    ent->description = NULL;   /* prevent recurring text entry */
	} else {
	    /*
	     * This is an ordinary event which started before our
	     * search begins. Ignore it.
	     */
	    return;
	}
    }

    /*
     * Now add this to the list of future events.
     */
    add234(ctx->future, dup_entry(ent));
}

static void real_list_entries(Date sd, Time st, Date ed, Time et, int verbose)
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
    ctx.verbose = verbose;

    db_list_entries(sd, st, ed, et, list_callback, &ctx);
    list_upto(&ctx, ed, et);

    while ((ent = delpos234(ctx.future, 0)) != NULL)
	sfree(ent);
    freetree234(ctx.future);
}

void list_entries(Date sd, Time st, Date ed, Time et)
{
    real_list_entries(sd, st, ed, et, FALSE);
}

void caltrap_list(int nargs, char **args, int nphysargs, int verbose)
{
    Date sd, ed;

    if (nargs > 2)
	fatalerr_listargno();
    assert(nargs <= nphysargs);

    if (nargs == 0) {
	sd = today();
	ed = sd + 14;		       /* two weeks */
    } else if (nargs == 1) {
	if (!parse_partial_date(args[0], &sd, &ed))
	    fatalerr_date(args[0]);
    } else if (nargs == 2) {
	if (!parse_partial_date(args[0], &sd, NULL))
	    fatalerr_date(args[0]);
	if (!parse_partial_date(args[1], &ed, NULL))
	    fatalerr_date(args[1]);
	if (sd > ed) {
	    Date tmp = sd; sd = ed; ed = tmp;
	}
    }

    real_list_entries(sd, 0, ed, 0, verbose);
}
