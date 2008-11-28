/*
 * datetime.c - date and time handling functions for Caltrap
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "caltrap.h"

#define DURATION_WEEK (7*24*60*60)
#define DURATION_DAY (24*60*60)
#define DURATION_HOUR (60*60)
#define DURATION_MINUTE (60)
#define DURATION_SECOND (1)

static const struct {
    Duration d;
    char c;
} duration_chars[] = {
    {DURATION_WEEK, 'w'},
    {DURATION_DAY, 'd'},
    {DURATION_HOUR, 'h'},
    {DURATION_MINUTE, 'm'},
    {DURATION_SECOND, 's'},
};

static const int mtab[] = {
    0,				       /* buffer entry (months start at 1) */
    0,				       /* jan */
    31,				       /* feb */
    59,				       /* mar (feb==28 and we correct
					* for this elsewhere) */
    90,				       /* apr */
    120,			       /* may */
    151,			       /* jun */
    181,			       /* jul */
    212,			       /* aug */
    243,			       /* sep */
    273,			       /* oct */
    304,			       /* nov */
    334,			       /* dec */
};

static const char *weekdays[] = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

static Date ymd_to_date(int y, int m, int d)
{
    int days;

    days = (y - 1900) * (365*4+1) / 4;
    if (y % 4 == 0 && m < 3) days--;
    days += mtab[m];
    days += d-1;

    /*
     * FIXME Y2100 bug! (Leap years are simplistically assumed every 4.)
     */

    return days;
}

static void date_to_ymd(Date d, int *yy, int *mm, int *dd) {
    int yr, mn, dy;

    yr = 1900 + 4 * ((d+1) / (4 * 365 + 1));
    d = (d+1) % (4 * 365 + 1);
    /* now d runs from 0 (1 Jan leapyear) to 1460 (31 Dec leapyear+3) */
    if (d == 59) {		       /* leap day special case */
	mn = 2; dy = 29;
    } else {
	if (d > 59) d--;
	/* now leap years can be ignored */
	yr += d / 365;
	d %= 365;
	/* now d is day (0..364) within a nonleap year, and yr is correct */
	for (mn = 1; mn < 12; mn++) {
	    if (d < mtab[mn+1])
		break;
	}
	d -= mtab[mn];
	dy = d + 1;
    }
    *yy = yr;
    *mm = mn;
    *dd = dy;
}

static int weekday(Date d)
{
    /* It so happens that date==0 is a Monday! */
    return d % 7;
}

/* ----------------------------------------------------------------------
 * Date and time parsing functions as advertised in caltrap.h.
 */

Date parse_date(char *str)
{
    int y, m, d, y2, m2, d2, delta;
    Date ret;

    if (sscanf(str, "%d-%d-%d", &y, &m, &d) == 3) {
	if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1900)
	    return INVALID_DATE;

	ret = ymd_to_date(y, m, d);

	/*
	 * Double-check the user hasn't entered anything stupid such as
	 * 30th Feb.
	 */
	date_to_ymd(ret, &y2, &m2, &d2);
	if (y2 != y || m2 != m || d2 != d)
	    return INVALID_DATE;
    } else if (sscanf(str, "+%d", &delta) == 1) {
	return today() + delta;
    } else if (sscanf(str, "-%d", &delta) == 1) {
	return today() - delta;
    } else {
	return INVALID_DATE;
    }

    return ret;
}

int parse_partial_date(char *str, Date *start, Date *after)
{
    Date d1;
    int y, m;

    d1 = parse_date(str);
    if (d1 != INVALID_DATE) {
	if (start)
	    *start = d1;
	if (after)
	    *after = d1 + 1;
	return 1;
    }

    if (sscanf(str, "%d-%d", &y, &m) == 2) {
	if (m < 1 || m > 12 || y < 1900)
	    return 0;

	if (start)
	    *start = ymd_to_date(y, m, 1);
	if (after) {
	    if (m == 12)
		*after = ymd_to_date(y + 1, 1, 1);
	    else
		*after = ymd_to_date(y, m + 1, 1);
	}
	return 1;
    } else if (sscanf(str, "%d", &y) == 1) {
	if (y < 1900)
	    return 0;
	if (start)
	    *start = ymd_to_date(y, 1, 1);
	if (after)
	    *after = ymd_to_date(y + 1, 1, 1);
	return 1;
    } else {
	return 0;
    }

    return 0;
}

Time parse_time(char *str)
{
    int h, m, s, ret;

    s = 0;
    ret = sscanf(str, "%d:%d:%d", &h, &m, &s);

    if (ret < 2)
	return INVALID_TIME;

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
	return INVALID_TIME;

    return (h * 60 + m) * 60 + s;
}

int parse_datetime(char *str, Date *d, Time *t)
{
    char *tstr;
    Date rd;
    Time rt;

    tstr = str + strcspn(str, " ");
    if (*tstr)
	tstr++;

    rd = parse_date(str);
    rt = parse_time(tstr);

    if (rd == INVALID_DATE || rt == INVALID_TIME)
	return 0;

    *d = rd;
    *t = rt;
    return 1;
}

Duration parse_duration(char *str)
{
    Duration ret = 0;
    int got_one = FALSE;

    if (*str == '-' && str[1] == '\0')
	return 0;		       /* special case */

    while (1) {
	int n, i;

	str += strspn(str, " ,");
	if (!*str)
	    break;

	if (*str < '0' || *str > '9')
	    return INVALID_DURATION;

	n = atoi(str);
	str += strspn(str, "0123456789");

	for (i = 0; i < lenof(duration_chars); i++) {
	    if (duration_chars[i].c == *str) {
		ret += duration_chars[i].d * n;
		str++;
		got_one = TRUE;
		break;
	    }
	}

	if (i == lenof(duration_chars))
	    return INVALID_DURATION;
    }

    if (!got_one)
	return INVALID_DURATION;

    return ret;
}

void add_to_datetime(Date *d, Time *t, Duration dur)
{
    *d += (dur / 86400);
    *t += (dur % 86400);
    *d += (*t / 86400);
    *t %= 86400;
}

Duration datetime_diff(Date d1, Time t1, Date d2, Time t2)
{
    Duration ret;
    ret = d1 - d2;
    ret *= 86400;
    ret += t1 - t2;
    return ret;
}

int datetime_cmp(Date d1, Time t1, Date d2, Time t2)
{
    if (d1 < d2)
	return -1;
    else if (d1 > d2)
	return +1;
    else if (t1 < t2)
	return -1;
    else if (t1 > t2)
	return +1;
    else
	return 0;
}

char *format_datetime(Date d, Time t)
{
    char *dfmt, *tfmt, *ret;

    dfmt = format_date(d);
    tfmt = format_time(t);
    ret = smalloc(2 + strlen(dfmt) + strlen(tfmt));
    sprintf(ret, "%s %s", dfmt, tfmt);
    sfree(dfmt);
    sfree(tfmt);

    return ret;
}

char *format_duration(Duration d)
{
    char result[512], *p, *ret;
    int i;

    p = result;

    if (d == 0) {
	*p++ = '-';
	p++;
    } else {

	for (i = 0; i < lenof(duration_chars); i++) {
	    if (d >= duration_chars[i].d) {
		p += sprintf(p, "%lld%c,", d / duration_chars[i].d,
			     duration_chars[i].c);
		d %= duration_chars[i].d;
	    }
	}

	assert(d == 0);
    }

    assert(p > result);

    ret = smalloc(p - result);
    memcpy(ret, result, p - result - 1);
    ret[p - result - 1] = '\0';

    return ret;
}

void now(Date *dd, Time *tt)
{
    time_t t;
    struct tm tm;

    t = time(NULL);
    tm = *localtime(&t);

    *dd = ymd_to_date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    assert(*dd != INVALID_DATE);

    *tt = (tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec;
}

Date today(void)
{
    Date ret;
    Time t;

    now(&ret, &t);
    return ret;
}

char *format_date_core(Date d, int full)
{
    char *s, *t;
    int yy, mm, dd;

    date_to_ymd(d, &yy, &mm, &dd); 

    s = smalloc(32);

    if (d == INVALID_DATE)
	sprintf(s, full ? "*INVALID DATE*" : "*INVALID!*");
    else if (d == NO_DATE)
	sprintf(s, "%*s", full ? 14 : 10, "");
    else {
	if (full) {
	    t = s + sprintf(s, "%3s ", weekdays[weekday(d)]);
	} else
	    t = s;
	sprintf(t, "%4d-%02d-%02d", yy, mm, dd);
    }

    return s;
}

char *format_date(Date d) { return format_date_core(d, FALSE); }
char *format_date_full(Date d) { return format_date_core(d, TRUE); }

char *format_time(Time t)
{
    char *s;

    s = smalloc(32);

    if (t == INVALID_TIME)
	sprintf(s, "*INVAL!*");
    else if (t == NO_TIME)
	sprintf(s, "        ");
    else {
	int hh, mm, ss;

	ss = t % 60;
	mm = t / 60;
	hh = mm / 60;
	mm %= 60;

	sprintf(s, "%02d:%02d:%02d", hh, mm, ss);
    }

    return s;
}

#ifdef UNITTEST
int main(void)
{
    Date sd, ed, d;
    struct { int y, m, d, ymd; } curr, prev;

    sd = ymd_to_date(1995, 1, 1);
    ed = ymd_to_date(2005, 1, 1);
    assert(ed - sd > 365 * 8);

    for (d = sd; d <= ed; d++) {
	date_to_ymd(d, &curr.y, &curr.m, &curr.d);
	curr.ymd = 10000 * curr.y + 100 * curr.m + curr.d;

	if (d == sd) {
	    prev = curr;
	    continue;
	}

	printf("prev: %4d-%02d-%02d  curr: %4d-%02d-%02d  d:%8d\n",
	       prev.y, prev.m, prev.d, curr.y, curr.m, curr.d, d);

	/* Monotonicity. */
	assert(prev.ymd < curr.ymd);

	/* Other obvious date properties. */
	if (curr.d != 1) {
	    /* Date increases by 1 except at start of new month. */
	    assert(prev.y == curr.y);
	    assert(prev.m == curr.m);
	    assert(prev.d == curr.d - 1);
	} else if (curr.m != 1) {
	    /* Month increases by 1 except at start of new year. */
	    assert(prev.y == curr.y);
	    assert(prev.m == curr.m - 1);

	    switch (prev.m) {
	      case 9: case 4: case 6: case 11:
		/* 30 days hath September, April, June and November ... */
		assert(prev.d == 30);
		break;
	      default:
		/* All the rest have 31 ... */
		assert(prev.d == 31);
		break;
	      case 2:
		/* except for February. */
		if (prev.y % 4)
		    assert(prev.d == 28);
		else
		    assert(prev.d == 29);
		break;
	    }
	} else {
	    /* Year increases by 1. */
	    assert(prev.y == curr.y - 1);
	}

	/* Round-trip conversion. */
	assert(ymd_to_date(curr.y, curr.m, curr.d) == d);

	/* And round to the next one. */
	prev = curr;
    }

    printf("tests passed ok\n");
    return 0;
}
#endif
