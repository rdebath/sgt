/*
 * date.c: date handling code for Timber.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "timber.h"

/*
 * Routines to get around the fact that C's time handling is awful.
 * With no direct inverse to gmtime(), and no standards-compliant
 * way to read and write a time_t directly, what standard format
 * can there possibly be for times that ports across time zones?
 *
 * Well, it can just about be done. Here's how.
 */

/*
 * This completely ANSI-compliant function determines the timezone shift
 * in seconds. (E.g. if local time were 1 hour ahead of GMT, this routine
 * would return 3600.) This timezone shift is to normal local time, not to
 * DST local time.
 */
static long tzshift(void) {
    time_t t1, t2;
    struct tm tm;
    t1 = time(NULL);
    tm = *gmtime(&t1);
    tm.tm_isdst = 0;		       /* should already be; let's make sure */
    t2 = mktime(&tm);
    /*
     * So tm is t1 formatted as GMT, and is also t2 formatted as
     * local time. Hence difftime(t1,t2) gives the number of
     * seconds by which local time is ahead of GMT. We'll assume
     * here that the number of seconds will fit in a long, since
     * for it not to do so would have to imply a time zone 68
     * _years_ different from GMT.
     */
    return (long)difftime(t1,t2);
}

/*
 * This completely ANSI-compliant function adjusts a `struct tm' by
 * a given number of seconds.
 */
static void adjust_tm(struct tm *tm, long seconds) {
    int sign = seconds < 0 ? -1 : +1;
    seconds = labs(seconds);
    mktime(tm);			       /* normalise the tm structure */
    tm->tm_mday += sign * (seconds / 86400);
    tm->tm_hour += sign * (seconds / 3600) % 24;
    tm->tm_sec += sign * (seconds % 3600);
    mktime(tm);			       /* normalise the tm structure again */
}

/*
 * With the aid of the above two functions, _this_ completely ANSI-
 * compliant function is the equivalent of mktime() using GMT. In
 * other words, it's the inverse of gmtime().
 */
time_t mktimegm(struct tm *tm) {
    tm->tm_isdst = 0;		       /* GMT is never daylight-saving */
    /*
     * If local time is an hour ahead of GMT, then calling mktime
     * on tm will give us a result one hour _earlier_ than what we
     * want, so we would have to add an hour to tm before calling
     * mktime.
     */
    adjust_tm(tm, tzshift());
    return mktime(tm);
}

/* ----------------------------------------------------------------------
 * Now here's a pair of functions to format and read a date in UTC,
 * as a storage format which is independent of the internal time_t.
 */
void fmt_date(time_t t, char *buf)
{
    struct tm tm = *gmtime(&t);

    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
	    1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec);
}

time_t unfmt_date(const char *buf)
{
    struct tm tm;

    if (6 != sscanf(buf, "%d-%d-%d %d:%d:%d",
		    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		    &tm.tm_hour, &tm.tm_min, &tm.tm_sec)) {
	return (time_t) (-1);  /* same error return as mktime() */
    }

    tm.tm_year -= 1900;
    tm.tm_mon--;
    return mktimegm(&tm);
}
