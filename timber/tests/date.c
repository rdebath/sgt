#include <time.h>
/*  Helper functions  */


void set_example_time (struct tm *tm)
{
    tm->tm_year = 100;
    tm->tm_mon = 3;
    tm->tm_mday = 9;
    tm->tm_hour = 12;
    tm->tm_min = 34;
    tm->tm_sec = 56;
    tm->tm_isdst = 0;
}



#include <check.h>
#include "timber.h"
/*  Check functions in the interface  */


START_TEST(unfmt_date_valid)
{
    struct tm tm;

    set_example_time(&tm);
    fail_unless (mktime(&tm) == unfmt_date("2000-04-09 12:34:56"), NULL);
}
END_TEST;


START_TEST(unfmt_date_invalid)
{
    fail_unless (unfmt_date("Not a timestamp") == (time_t) (-1), NULL);
}
END_TEST;


#include "date.c"
/*  Check internal functions  */


START_TEST(adjust_tm_works)
{
    time_t t1, t2;
    struct tm tm;
    const long offset = 123456;

    set_example_time(&tm);
    t1 = mktime(&tm);

    adjust_tm (&tm, offset);
    t2 = mktime(&tm);

    fail_unless (offset == difftime (t2, t1), NULL);
}
END_TEST;


Suite *date_suite (void)
{
    Suite *s = suite_create ("date");
    TCase *misc = tcase_create ("misc");

    suite_add_tcase (s, misc);
    tcase_add_test (misc, unfmt_date_valid);
    tcase_add_test (misc, unfmt_date_invalid);
    tcase_add_test (misc, adjust_tm_works);

    return s;
}
