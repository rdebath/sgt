#include <stdio.h>

#define INTERNAL
#include "tring.h"

#define T(h,m,s) (((h)*60+(m))*60+(s))

int day_excluded(int date)
{
    return 0;
}
int always_bright = 0;

void default_mode(int timeofday, int dayofweek, int date,
		  struct pstate *ps, struct lstate *ls);

#define STR2(x) #x
#define STR(x) STR2(x)

#define TEST(expr) do                                                   \
    {                                                                   \
        tests++;                                                        \
        if (!(expr)) {                                                  \
            printf("%s:%d: fail! (%s)\n", __FILE__, __LINE__, STR(expr)); \
            fails++;                                                    \
        }                                                               \
    } while (0)

int main(int argc, char **argv)
{
    int tests = 0, fails = 0;

    /*
     * Tests in a normal-ish configuration, with alarm time early in
     * the morning and reset time at noon.
     */
    {
        const struct pstate cps = { T(8,0,0), T(12,0,0), T(0,9,0), 31 };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0
        };
        struct pstate ps;
        struct lstate ls;

        /*
         * Default mode at 10am should be AMODE_OFF.
         */
        ps = cps;
        ls = cls;
        default_mode(T(10,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);

        /*
         * Default mode at 12:10 should be AMODE_CONFIRM.
         */
        ps = cps;
        ls = cls;
        default_mode(T(12,10,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Default mode at 03:00 should be AMODE_CONFIRM too.
         */
        ps = cps;
        ls = cls;
        default_mode(T(12,10,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Ticking past the reset time should light us back up into
         * AMODE_CONFIRM.
         */
        ps = cps;
        ls = cls;
        event_timetick(T(11,59,59), T(12,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Ticking past the default alarm time while in AMODE_CONFIRM
         * should put us down into AMODE_OFF. Current alarm time is
         * ignored.
         */
        ps = cps;
        ls = cls;
        ls.amode = AMODE_CONFIRM;
        ls.alarm_time = T(8,20,0);
        event_timetick(T(7,59,59), T(8,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);

        /*
         * Ticking past the alarm time while in AMODE_ON should go to
         * AMODE_OFF and also sound the alarm. But it's the _current_,
         * not default, alarm time.
         */
        ps = cps;
        ls = cls;
        ls.amode = AMODE_ON;
        ls.alarm_time = T(8,20,0);
        event_timetick(T(8,19,59), T(8,20,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);
        TEST(ls.alarm_sounding);
    }

    /*
     * Tests in a reversed configuration, with alarm time late in
     * the evening and reset time still at noon.
     */
    {
        const struct pstate cps = { T(22,0,0), T(12,0,0), T(0,9,0), 31 };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0
        };
        struct pstate ps;
        struct lstate ls;

        /*
         * Default mode at 10am should be AMODE_OFF.
         */
        ps = cps;
        ls = cls;
        default_mode(T(10,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);

        /*
         * Default mode at 12:10 should be AMODE_CONFIRM.
         */
        ps = cps;
        ls = cls;
        default_mode(T(12,10,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Default mode at 03:00 should be AMODE_OFF.
         */
        ps = cps;
        ls = cls;
        default_mode(T(12,10,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Ticking past the reset time should light us back up into
         * AMODE_CONFIRM.
         */
        ps = cps;
        ls = cls;
        event_timetick(T(11,59,59), T(12,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_CONFIRM);

        /*
         * Ticking past the default alarm time while in AMODE_CONFIRM
         * should put us down into AMODE_OFF. Current alarm time is
         * ignored.
         */
        ps = cps;
        ls = cls;
        ls.amode = AMODE_CONFIRM;
        ls.alarm_time = T(8,20,0);
        event_timetick(T(21,59,59), T(22,0,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);

        /*
         * Ticking past the alarm time while in AMODE_ON should go to
         * AMODE_OFF and also sound the alarm. But it's the _current_,
         * not default, alarm time.
         */
        ps = cps;
        ls = cls;
        ls.amode = AMODE_ON;
        ls.alarm_time = T(22,20,0);
        event_timetick(T(22,19,59), T(22,20,0), 0, 20121105, &ps, &ls);
        TEST(ls.amode == AMODE_OFF);
        TEST(ls.alarm_sounding);
    }

    printf("%d tests : %d passed : %d failed\n", tests, tests-fails, fails);
    return fails != 0;
}
