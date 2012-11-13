#include <stdio.h>

#define INTERNAL
#include "tring.h"

#define T(h,m,s) (((h)*60+(m))*60+(s))
#define ALL(x) {x,x,x,x,x,x,x} /* all alarm times the same */

void get_exception(int date, int *defalarmtime, int *enabled)
{
    int i;

    if (date == 20121116) {
        *defalarmtime = T(14,0,0);
        *enabled = 1;
    } else if (date == 20121117) {
        *defalarmtime = T(11,0,0);
        *enabled = 0;        
    }
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
        const struct pstate cps = { ALL(T(8,0,0)), T(12,0,0), T(0,9,0), 31 };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0, 0, 0
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
        const struct pstate cps = { ALL(T(22,0,0)), T(12,0,0), T(0,9,0), 31 };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0, 0, 0
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

    /*
     * Tests in a thoroughly confused configuration, with a different
     * alarm time every day, some late and some early.
     */
    {
        const struct pstate cps = {
            { T(22,0,0), T(8,0,0), T(21,0,0), T(9,0,0), T(14,0,0),
              T(11,0,0), T(7,0,0) },
            T(12,0,0), T(0,9,0), 31
        };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0, 0, 0
        };
        struct pstate ps;
        struct lstate ls;

        /*
         * Default modes at half past every hour through the week.
         */
#define DEFMODE(time, day, mode) do                             \
        {                                                       \
            ps = cps;                                           \
            ls = cls;                                           \
            default_mode(time, day, 20121105+day, &ps, &ls);    \
            TEST(ls.amode == mode);                             \
        } while (0)
        DEFMODE(T( 0,30,0), 0, AMODE_OFF);
        DEFMODE(T( 1,30,0), 0, AMODE_OFF);
        DEFMODE(T( 2,30,0), 0, AMODE_OFF);
        DEFMODE(T( 3,30,0), 0, AMODE_OFF);
        DEFMODE(T( 4,30,0), 0, AMODE_OFF);
        DEFMODE(T( 5,30,0), 0, AMODE_OFF);
        DEFMODE(T( 6,30,0), 0, AMODE_OFF);
        DEFMODE(T( 7,30,0), 0, AMODE_OFF);
        DEFMODE(T( 8,30,0), 0, AMODE_OFF);
        DEFMODE(T( 9,30,0), 0, AMODE_OFF);
        DEFMODE(T(10,30,0), 0, AMODE_OFF);
        DEFMODE(T(11,30,0), 0, AMODE_OFF);
        /* ticked past reset time on Monday */
        DEFMODE(T(12,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(14,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(15,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(16,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(17,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(18,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(19,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(20,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(21,30,0), 0, AMODE_CONFIRM);
        /* ticked past alarm time on Monday, but next alarm is before reset */
        DEFMODE(T(22,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(23,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T( 0,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 1,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 2,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 3,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 4,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 5,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 6,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 7,30,0), 1, AMODE_CONFIRM);
        /* ticked past alarm time on Tuesday */
        DEFMODE(T( 8,30,0), 1, AMODE_OFF);
        DEFMODE(T( 9,30,0), 1, AMODE_OFF);
        DEFMODE(T(10,30,0), 1, AMODE_OFF);
        DEFMODE(T(11,30,0), 1, AMODE_OFF);
        /* ticked past reset time on Tuesday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 1, AMODE_OFF);
        DEFMODE(T(13,30,0), 1, AMODE_OFF);
        DEFMODE(T(14,30,0), 1, AMODE_OFF);
        DEFMODE(T(15,30,0), 1, AMODE_OFF);
        DEFMODE(T(16,30,0), 1, AMODE_OFF);
        DEFMODE(T(17,30,0), 1, AMODE_OFF);
        DEFMODE(T(18,30,0), 1, AMODE_OFF);
        DEFMODE(T(19,30,0), 1, AMODE_OFF);
        DEFMODE(T(20,30,0), 1, AMODE_OFF);
        DEFMODE(T(21,30,0), 1, AMODE_OFF);
        DEFMODE(T(22,30,0), 1, AMODE_OFF);
        DEFMODE(T(23,30,0), 1, AMODE_OFF);
        DEFMODE(T( 0,30,0), 2, AMODE_OFF);
        DEFMODE(T( 1,30,0), 2, AMODE_OFF);
        DEFMODE(T( 2,30,0), 2, AMODE_OFF);
        DEFMODE(T( 3,30,0), 2, AMODE_OFF);
        DEFMODE(T( 4,30,0), 2, AMODE_OFF);
        DEFMODE(T( 5,30,0), 2, AMODE_OFF);
        DEFMODE(T( 6,30,0), 2, AMODE_OFF);
        DEFMODE(T( 7,30,0), 2, AMODE_OFF);
        DEFMODE(T( 8,30,0), 2, AMODE_OFF);
        DEFMODE(T( 9,30,0), 2, AMODE_OFF);
        DEFMODE(T(10,30,0), 2, AMODE_OFF);
        DEFMODE(T(11,30,0), 2, AMODE_OFF);
        /* ticked past reset time on Wednesday */
        DEFMODE(T(12,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(14,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(15,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(16,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(17,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(18,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(19,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(20,30,0), 2, AMODE_CONFIRM);
        /* ticked past alarm on Wednesday, but next alarm is before reset */
        DEFMODE(T(21,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(22,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(23,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T( 0,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 1,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 2,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 3,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 4,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 5,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 6,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 7,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 8,30,0), 3, AMODE_CONFIRM);
        /* ticked past alarm time on Thursday */
        DEFMODE(T( 9,30,0), 3, AMODE_OFF);
        DEFMODE(T(10,30,0), 3, AMODE_OFF);
        DEFMODE(T(11,30,0), 3, AMODE_OFF);
        /* ticked past reset on Thursday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 3, AMODE_OFF);
        DEFMODE(T(13,30,0), 3, AMODE_OFF);
        DEFMODE(T(14,30,0), 3, AMODE_OFF);
        DEFMODE(T(15,30,0), 3, AMODE_OFF);
        DEFMODE(T(16,30,0), 3, AMODE_OFF);
        DEFMODE(T(17,30,0), 3, AMODE_OFF);
        DEFMODE(T(18,30,0), 3, AMODE_OFF);
        DEFMODE(T(19,30,0), 3, AMODE_OFF);
        DEFMODE(T(20,30,0), 3, AMODE_OFF);
        DEFMODE(T(21,30,0), 3, AMODE_OFF);
        DEFMODE(T(22,30,0), 3, AMODE_OFF);
        DEFMODE(T(23,30,0), 3, AMODE_OFF);
        DEFMODE(T( 0,30,0), 4, AMODE_OFF);
        DEFMODE(T( 1,30,0), 4, AMODE_OFF);
        DEFMODE(T( 2,30,0), 4, AMODE_OFF);
        DEFMODE(T( 3,30,0), 4, AMODE_OFF);
        DEFMODE(T( 4,30,0), 4, AMODE_OFF);
        DEFMODE(T( 5,30,0), 4, AMODE_OFF);
        DEFMODE(T( 6,30,0), 4, AMODE_OFF);
        DEFMODE(T( 7,30,0), 4, AMODE_OFF);
        DEFMODE(T( 8,30,0), 4, AMODE_OFF);
        DEFMODE(T( 9,30,0), 4, AMODE_OFF);
        DEFMODE(T(10,30,0), 4, AMODE_OFF);
        DEFMODE(T(11,30,0), 4, AMODE_OFF);
        /* ticked past reset time on Friday */
        DEFMODE(T(12,30,0), 4, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 4, AMODE_CONFIRM);
        /* ticked past alarm time on Friday */
        DEFMODE(T(14,30,0), 4, AMODE_OFF);
        DEFMODE(T(15,30,0), 4, AMODE_OFF);
        DEFMODE(T(16,30,0), 4, AMODE_OFF);
        DEFMODE(T(17,30,0), 4, AMODE_OFF);
        DEFMODE(T(18,30,0), 4, AMODE_OFF);
        DEFMODE(T(19,30,0), 4, AMODE_OFF);
        DEFMODE(T(20,30,0), 4, AMODE_OFF);
        DEFMODE(T(21,30,0), 4, AMODE_OFF);
        DEFMODE(T(22,30,0), 4, AMODE_OFF);
        DEFMODE(T(23,30,0), 4, AMODE_OFF);
        DEFMODE(T( 0,30,0), 5, AMODE_OFF);
        DEFMODE(T( 1,30,0), 5, AMODE_OFF);
        DEFMODE(T( 2,30,0), 5, AMODE_OFF);
        DEFMODE(T( 3,30,0), 5, AMODE_OFF);
        DEFMODE(T( 4,30,0), 5, AMODE_OFF);
        DEFMODE(T( 5,30,0), 5, AMODE_OFF);
        DEFMODE(T( 6,30,0), 5, AMODE_OFF);
        DEFMODE(T( 7,30,0), 5, AMODE_OFF);
        DEFMODE(T( 8,30,0), 5, AMODE_OFF);
        DEFMODE(T( 9,30,0), 5, AMODE_OFF);
        DEFMODE(T(10,30,0), 5, AMODE_OFF);
        /* ticked past alarm time on Saturday, but it was an off day anyway */
        DEFMODE(T(11,30,0), 5, AMODE_OFF);
        /* ticked past reset time on Saturday, but tomorrow's an off day */
        DEFMODE(T(12,30,0), 5, AMODE_OFF);
        DEFMODE(T(13,30,0), 5, AMODE_OFF);
        DEFMODE(T(14,30,0), 5, AMODE_OFF);
        DEFMODE(T(15,30,0), 5, AMODE_OFF);
        DEFMODE(T(16,30,0), 5, AMODE_OFF);
        DEFMODE(T(17,30,0), 5, AMODE_OFF);
        DEFMODE(T(18,30,0), 5, AMODE_OFF);
        DEFMODE(T(19,30,0), 5, AMODE_OFF);
        DEFMODE(T(20,30,0), 5, AMODE_OFF);
        DEFMODE(T(21,30,0), 5, AMODE_OFF);
        DEFMODE(T(22,30,0), 5, AMODE_OFF);
        DEFMODE(T(23,30,0), 5, AMODE_OFF);
        DEFMODE(T( 0,30,0), 6, AMODE_OFF);
        DEFMODE(T( 1,30,0), 6, AMODE_OFF);
        DEFMODE(T( 2,30,0), 6, AMODE_OFF);
        DEFMODE(T( 3,30,0), 6, AMODE_OFF);
        DEFMODE(T( 4,30,0), 6, AMODE_OFF);
        DEFMODE(T( 5,30,0), 6, AMODE_OFF);
        DEFMODE(T( 6,30,0), 6, AMODE_OFF);
        DEFMODE(T( 7,30,0), 6, AMODE_OFF);
        /* ticked past alarm time on Sunday, but it was an off day anyway */
        DEFMODE(T( 8,30,0), 6, AMODE_OFF);
        DEFMODE(T( 9,30,0), 6, AMODE_OFF);
        DEFMODE(T(10,30,0), 6, AMODE_OFF);
        DEFMODE(T(11,30,0), 6, AMODE_OFF);
        /* ticked past reset time on Sunday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 6, AMODE_OFF);
        DEFMODE(T(13,30,0), 6, AMODE_OFF);
        DEFMODE(T(14,30,0), 6, AMODE_OFF);
        DEFMODE(T(15,30,0), 6, AMODE_OFF);
        DEFMODE(T(16,30,0), 6, AMODE_OFF);
        DEFMODE(T(17,30,0), 6, AMODE_OFF);
        DEFMODE(T(18,30,0), 6, AMODE_OFF);
        DEFMODE(T(19,30,0), 6, AMODE_OFF);
        DEFMODE(T(20,30,0), 6, AMODE_OFF);
        DEFMODE(T(21,30,0), 6, AMODE_OFF);
        DEFMODE(T(22,30,0), 6, AMODE_OFF);
        DEFMODE(T(23,30,0), 6, AMODE_OFF);

        /*
         * Test the tick-pasts of alarm and reset time on all seven days.
         */
#define TESTALARM(time, day, modeafter) do                              \
        {                                                               \
            ps = cps;                                                   \
            ls = cls;                                                   \
            ls.amode = AMODE_CONFIRM;                                   \
            ls.alarm_time = ps.defalarmtime[day];                       \
            event_timetick(time-1, time, day, 20121105+day, &ps, &ls);  \
            TEST(ls.amode == modeafter);                                \
        } while (0)
        TESTALARM(T(22,0,0), 0, AMODE_CONFIRM); /* another alarm follows */
        TESTALARM(T( 8,0,0), 1, AMODE_OFF);
        TESTALARM(T(21,0,0), 2, AMODE_CONFIRM); /* another alarm follows */
        TESTALARM(T( 9,0,0), 3, AMODE_OFF);
        TESTALARM(T(14,0,0), 4, AMODE_OFF);
        TESTALARM(T(11,0,0), 5, AMODE_OFF);
        TESTALARM(T( 7,0,0), 6, AMODE_OFF);

#define TESTRESET(time, day, modeafter) do                              \
        {                                                               \
            ps = cps;                                                   \
            ls = cls;                                                   \
            ls.amode = AMODE_OFF;                                       \
            event_timetick(time-1, time, day, 20121105+day, &ps, &ls);  \
            TEST(ls.amode == modeafter);                                \
        } while (0)
        TESTRESET(T(12,0,0), 0, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 1, AMODE_OFF); /* another reset follows */
        TESTRESET(T(12,0,0), 2, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 3, AMODE_OFF); /* another reset follows */
        TESTRESET(T(12,0,0), 4, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 5, AMODE_OFF); /* off-day follows */
        TESTRESET(T(12,0,0), 6, AMODE_OFF); /* another reset follows */
#undef DEFMODE
#undef TESTALARM
#undef TESTRESET
    }

    /*
     * Test the same confused configuration again, but with some of
     * the days configured via exceptions.
     */
    {
        const struct pstate cps = {
            { T(22,0,0), T(8,0,0), T(21,0,0), T(9,0,0), T(0,30,0),
              T(1,30,0), T(7,0,0) },
            T(12,0,0), T(0,9,0), 47
        };
        const struct lstate cls = {
            AMODE_OFF, 0, 0, 0, 0, 0, DMODE_NORMAL, 0, 0, 0, 0
        };
        struct pstate ps;
        struct lstate ls;

        /*
         * Default modes at half past every hour through the week.
         */
#define DEFMODE(time, day, mode) do                             \
        {                                                       \
            ps = cps;                                           \
            ls = cls;                                           \
            default_mode(time, day, 20121112+day, &ps, &ls);    \
            TEST(ls.amode == mode);                             \
        } while (0)
        DEFMODE(T( 0,30,0), 0, AMODE_OFF);
        DEFMODE(T( 1,30,0), 0, AMODE_OFF);
        DEFMODE(T( 2,30,0), 0, AMODE_OFF);
        DEFMODE(T( 3,30,0), 0, AMODE_OFF);
        DEFMODE(T( 4,30,0), 0, AMODE_OFF);
        DEFMODE(T( 5,30,0), 0, AMODE_OFF);
        DEFMODE(T( 6,30,0), 0, AMODE_OFF);
        DEFMODE(T( 7,30,0), 0, AMODE_OFF);
        DEFMODE(T( 8,30,0), 0, AMODE_OFF);
        DEFMODE(T( 9,30,0), 0, AMODE_OFF);
        DEFMODE(T(10,30,0), 0, AMODE_OFF);
        DEFMODE(T(11,30,0), 0, AMODE_OFF);
        /* ticked past reset time on Monday */
        DEFMODE(T(12,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(14,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(15,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(16,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(17,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(18,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(19,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(20,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(21,30,0), 0, AMODE_CONFIRM);
        /* ticked past alarm time on Monday, but next alarm is before reset */
        DEFMODE(T(22,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T(23,30,0), 0, AMODE_CONFIRM);
        DEFMODE(T( 0,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 1,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 2,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 3,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 4,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 5,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 6,30,0), 1, AMODE_CONFIRM);
        DEFMODE(T( 7,30,0), 1, AMODE_CONFIRM);
        /* ticked past alarm time on Tuesday */
        DEFMODE(T( 8,30,0), 1, AMODE_OFF);
        DEFMODE(T( 9,30,0), 1, AMODE_OFF);
        DEFMODE(T(10,30,0), 1, AMODE_OFF);
        DEFMODE(T(11,30,0), 1, AMODE_OFF);
        /* ticked past reset time on Tuesday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 1, AMODE_OFF);
        DEFMODE(T(13,30,0), 1, AMODE_OFF);
        DEFMODE(T(14,30,0), 1, AMODE_OFF);
        DEFMODE(T(15,30,0), 1, AMODE_OFF);
        DEFMODE(T(16,30,0), 1, AMODE_OFF);
        DEFMODE(T(17,30,0), 1, AMODE_OFF);
        DEFMODE(T(18,30,0), 1, AMODE_OFF);
        DEFMODE(T(19,30,0), 1, AMODE_OFF);
        DEFMODE(T(20,30,0), 1, AMODE_OFF);
        DEFMODE(T(21,30,0), 1, AMODE_OFF);
        DEFMODE(T(22,30,0), 1, AMODE_OFF);
        DEFMODE(T(23,30,0), 1, AMODE_OFF);
        DEFMODE(T( 0,30,0), 2, AMODE_OFF);
        DEFMODE(T( 1,30,0), 2, AMODE_OFF);
        DEFMODE(T( 2,30,0), 2, AMODE_OFF);
        DEFMODE(T( 3,30,0), 2, AMODE_OFF);
        DEFMODE(T( 4,30,0), 2, AMODE_OFF);
        DEFMODE(T( 5,30,0), 2, AMODE_OFF);
        DEFMODE(T( 6,30,0), 2, AMODE_OFF);
        DEFMODE(T( 7,30,0), 2, AMODE_OFF);
        DEFMODE(T( 8,30,0), 2, AMODE_OFF);
        DEFMODE(T( 9,30,0), 2, AMODE_OFF);
        DEFMODE(T(10,30,0), 2, AMODE_OFF);
        DEFMODE(T(11,30,0), 2, AMODE_OFF);
        /* ticked past reset time on Wednesday */
        DEFMODE(T(12,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(14,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(15,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(16,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(17,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(18,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(19,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(20,30,0), 2, AMODE_CONFIRM);
        /* ticked past alarm on Wednesday, but next alarm is before reset */
        DEFMODE(T(21,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(22,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T(23,30,0), 2, AMODE_CONFIRM);
        DEFMODE(T( 0,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 1,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 2,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 3,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 4,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 5,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 6,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 7,30,0), 3, AMODE_CONFIRM);
        DEFMODE(T( 8,30,0), 3, AMODE_CONFIRM);
        /* ticked past alarm time on Thursday */
        DEFMODE(T( 9,30,0), 3, AMODE_OFF);
        DEFMODE(T(10,30,0), 3, AMODE_OFF);
        DEFMODE(T(11,30,0), 3, AMODE_OFF);
        /* ticked past reset on Thursday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 3, AMODE_OFF);
        DEFMODE(T(13,30,0), 3, AMODE_OFF);
        DEFMODE(T(14,30,0), 3, AMODE_OFF);
        DEFMODE(T(15,30,0), 3, AMODE_OFF);
        DEFMODE(T(16,30,0), 3, AMODE_OFF);
        DEFMODE(T(17,30,0), 3, AMODE_OFF);
        DEFMODE(T(18,30,0), 3, AMODE_OFF);
        DEFMODE(T(19,30,0), 3, AMODE_OFF);
        DEFMODE(T(20,30,0), 3, AMODE_OFF);
        DEFMODE(T(21,30,0), 3, AMODE_OFF);
        DEFMODE(T(22,30,0), 3, AMODE_OFF);
        DEFMODE(T(23,30,0), 3, AMODE_OFF);
        DEFMODE(T( 0,30,0), 4, AMODE_OFF);
        DEFMODE(T( 1,30,0), 4, AMODE_OFF);
        DEFMODE(T( 2,30,0), 4, AMODE_OFF);
        DEFMODE(T( 3,30,0), 4, AMODE_OFF);
        DEFMODE(T( 4,30,0), 4, AMODE_OFF);
        DEFMODE(T( 5,30,0), 4, AMODE_OFF);
        DEFMODE(T( 6,30,0), 4, AMODE_OFF);
        DEFMODE(T( 7,30,0), 4, AMODE_OFF);
        DEFMODE(T( 8,30,0), 4, AMODE_OFF);
        DEFMODE(T( 9,30,0), 4, AMODE_OFF);
        DEFMODE(T(10,30,0), 4, AMODE_OFF);
        DEFMODE(T(11,30,0), 4, AMODE_OFF);
        /* ticked past reset time on Friday */
        DEFMODE(T(12,30,0), 4, AMODE_CONFIRM);
        DEFMODE(T(13,30,0), 4, AMODE_CONFIRM);
        /* ticked past alarm time on Friday */
        DEFMODE(T(14,30,0), 4, AMODE_OFF);
        DEFMODE(T(15,30,0), 4, AMODE_OFF);
        DEFMODE(T(16,30,0), 4, AMODE_OFF);
        DEFMODE(T(17,30,0), 4, AMODE_OFF);
        DEFMODE(T(18,30,0), 4, AMODE_OFF);
        DEFMODE(T(19,30,0), 4, AMODE_OFF);
        DEFMODE(T(20,30,0), 4, AMODE_OFF);
        DEFMODE(T(21,30,0), 4, AMODE_OFF);
        DEFMODE(T(22,30,0), 4, AMODE_OFF);
        DEFMODE(T(23,30,0), 4, AMODE_OFF);
        DEFMODE(T( 0,30,0), 5, AMODE_OFF);
        DEFMODE(T( 1,30,0), 5, AMODE_OFF);
        DEFMODE(T( 2,30,0), 5, AMODE_OFF);
        DEFMODE(T( 3,30,0), 5, AMODE_OFF);
        DEFMODE(T( 4,30,0), 5, AMODE_OFF);
        DEFMODE(T( 5,30,0), 5, AMODE_OFF);
        DEFMODE(T( 6,30,0), 5, AMODE_OFF);
        DEFMODE(T( 7,30,0), 5, AMODE_OFF);
        DEFMODE(T( 8,30,0), 5, AMODE_OFF);
        DEFMODE(T( 9,30,0), 5, AMODE_OFF);
        DEFMODE(T(10,30,0), 5, AMODE_OFF);
        /* ticked past alarm time on Saturday, but it was an off day anyway */
        DEFMODE(T(11,30,0), 5, AMODE_OFF);
        /* ticked past reset time on Saturday, but tomorrow's an off day */
        DEFMODE(T(12,30,0), 5, AMODE_OFF);
        DEFMODE(T(13,30,0), 5, AMODE_OFF);
        DEFMODE(T(14,30,0), 5, AMODE_OFF);
        DEFMODE(T(15,30,0), 5, AMODE_OFF);
        DEFMODE(T(16,30,0), 5, AMODE_OFF);
        DEFMODE(T(17,30,0), 5, AMODE_OFF);
        DEFMODE(T(18,30,0), 5, AMODE_OFF);
        DEFMODE(T(19,30,0), 5, AMODE_OFF);
        DEFMODE(T(20,30,0), 5, AMODE_OFF);
        DEFMODE(T(21,30,0), 5, AMODE_OFF);
        DEFMODE(T(22,30,0), 5, AMODE_OFF);
        DEFMODE(T(23,30,0), 5, AMODE_OFF);
        DEFMODE(T( 0,30,0), 6, AMODE_OFF);
        DEFMODE(T( 1,30,0), 6, AMODE_OFF);
        DEFMODE(T( 2,30,0), 6, AMODE_OFF);
        DEFMODE(T( 3,30,0), 6, AMODE_OFF);
        DEFMODE(T( 4,30,0), 6, AMODE_OFF);
        DEFMODE(T( 5,30,0), 6, AMODE_OFF);
        DEFMODE(T( 6,30,0), 6, AMODE_OFF);
        DEFMODE(T( 7,30,0), 6, AMODE_OFF);
        /* ticked past alarm time on Sunday, but it was an off day anyway */
        DEFMODE(T( 8,30,0), 6, AMODE_OFF);
        DEFMODE(T( 9,30,0), 6, AMODE_OFF);
        DEFMODE(T(10,30,0), 6, AMODE_OFF);
        DEFMODE(T(11,30,0), 6, AMODE_OFF);
        /* ticked past reset time on Sunday, but next reset is before alarm */
        DEFMODE(T(12,30,0), 6, AMODE_OFF);
        DEFMODE(T(13,30,0), 6, AMODE_OFF);
        DEFMODE(T(14,30,0), 6, AMODE_OFF);
        DEFMODE(T(15,30,0), 6, AMODE_OFF);
        DEFMODE(T(16,30,0), 6, AMODE_OFF);
        DEFMODE(T(17,30,0), 6, AMODE_OFF);
        DEFMODE(T(18,30,0), 6, AMODE_OFF);
        DEFMODE(T(19,30,0), 6, AMODE_OFF);
        DEFMODE(T(20,30,0), 6, AMODE_OFF);
        DEFMODE(T(21,30,0), 6, AMODE_OFF);
        DEFMODE(T(22,30,0), 6, AMODE_OFF);
        DEFMODE(T(23,30,0), 6, AMODE_OFF);

        /*
         * Test the tick-pasts of alarm and reset time on all seven days.
         */
#define TESTALARM(time, day, modeafter) do                              \
        {                                                               \
            ps = cps;                                                   \
            ls = cls;                                                   \
            ls.amode = AMODE_CONFIRM;                                   \
            ls.alarm_time = ps.defalarmtime[day];                       \
            event_timetick(time-1, time, day, 20121112+day, &ps, &ls);  \
            TEST(ls.amode == modeafter);                                \
        } while (0)
        TESTALARM(T(22,0,0), 0, AMODE_CONFIRM); /* another alarm follows */
        TESTALARM(T( 8,0,0), 1, AMODE_OFF);
        TESTALARM(T(21,0,0), 2, AMODE_CONFIRM); /* another alarm follows */
        TESTALARM(T( 9,0,0), 3, AMODE_OFF);
        TESTALARM(T(14,0,0), 4, AMODE_OFF);
        TESTALARM(T(11,0,0), 5, AMODE_OFF);
        TESTALARM(T( 7,0,0), 6, AMODE_OFF);

#define TESTRESET(time, day, modeafter) do                              \
        {                                                               \
            ps = cps;                                                   \
            ls = cls;                                                   \
            ls.amode = AMODE_OFF;                                       \
            event_timetick(time-1, time, day, 20121112+day, &ps, &ls);  \
            TEST(ls.amode == modeafter);                                \
        } while (0)
        TESTRESET(T(12,0,0), 0, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 1, AMODE_OFF); /* another reset follows */
        TESTRESET(T(12,0,0), 2, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 3, AMODE_OFF); /* another reset follows */
        TESTRESET(T(12,0,0), 4, AMODE_CONFIRM);
        TESTRESET(T(12,0,0), 5, AMODE_OFF); /* off-day follows */
        TESTRESET(T(12,0,0), 6, AMODE_OFF); /* another reset follows */
#undef DEFMODE
#undef TESTALARM
#undef TESTRESET
    }

    printf("%d tests : %d passed : %d failed\n", tests, tests-fails, fails);
    return fails != 0;
}
