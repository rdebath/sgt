/*
 * datetime.c - date and time handling functions for Caltrap
 */

#include <time.h>
#include <assert.h>
#include "caltrap.h"

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
    int y, m, d, y2, m2, d2;
    Date ret;
    
    if (sscanf(str, "%d-%d-%d", &y, &m, &d) != 3)
	return INVALID_DATE;
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

    return ret;
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

Date today()
{
    time_t t;
    struct tm tm;
    Date ret;

    t = time(NULL);
    tm = *localtime(&t);

    ret = ymd_to_date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    assert(ret != INVALID_DATE);

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
