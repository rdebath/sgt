/*
 * display.c: Tring display code.
 */

#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "tring.h"

/*
 * This structure represents a piece of sprite data, as included
 * in the binary.
 */
struct sprite {
    unsigned short x, y, w, h;
    unsigned short pixels[0];
};

extern const struct sprite _binary_build_image001_spr_start;
extern const struct sprite _binary_build_image002_spr_start;
extern const struct sprite _binary_build_image003_spr_start;
extern const struct sprite _binary_build_image004_spr_start;
extern const struct sprite _binary_build_image005_spr_start;
extern const struct sprite _binary_build_image006_spr_start;
extern const struct sprite _binary_build_image007_spr_start;
extern const struct sprite _binary_build_image008_spr_start;
extern const struct sprite _binary_build_image009_spr_start;
extern const struct sprite _binary_build_image010_spr_start;
extern const struct sprite _binary_build_image011_spr_start;
extern const struct sprite _binary_build_image012_spr_start;
extern const struct sprite _binary_build_image013_spr_start;
extern const struct sprite _binary_build_image014_spr_start;
extern const struct sprite _binary_build_image015_spr_start;
extern const struct sprite _binary_build_image016_spr_start;
extern const struct sprite _binary_build_image017_spr_start;
extern const struct sprite _binary_build_image018_spr_start;
extern const struct sprite _binary_build_image019_spr_start;
extern const struct sprite _binary_build_image020_spr_start;
extern const struct sprite _binary_build_image021_spr_start;
extern const struct sprite _binary_build_image022_spr_start;
extern const struct sprite _binary_build_image023_spr_start;
extern const struct sprite _binary_build_image024_spr_start;
extern const struct sprite _binary_build_image025_spr_start;
extern const struct sprite _binary_build_image026_spr_start;
extern const struct sprite _binary_build_image027_spr_start;
extern const struct sprite _binary_build_image028_spr_start;
extern const struct sprite _binary_build_image029_spr_start;
extern const struct sprite _binary_build_image030_spr_start;
extern const struct sprite _binary_build_image031_spr_start;
extern const struct sprite _binary_build_image032_spr_start;
extern const struct sprite _binary_build_image033_spr_start;
extern const struct sprite _binary_build_image034_spr_start;
extern const struct sprite _binary_build_image035_spr_start;
extern const struct sprite _binary_build_image036_spr_start;
extern const struct sprite _binary_build_image037_spr_start;
extern const struct sprite _binary_build_image038_spr_start;
extern const struct sprite _binary_build_image039_spr_start;
extern const struct sprite _binary_build_image040_spr_start;
extern const struct sprite _binary_build_image041_spr_start;
extern const struct sprite _binary_build_image042_spr_start;
extern const struct sprite _binary_build_image043_spr_start;
extern const struct sprite _binary_build_image044_spr_start;
extern const struct sprite _binary_build_image045_spr_start;
extern const struct sprite _binary_build_image046_spr_start;
extern const struct sprite _binary_build_image047_spr_start;
extern const struct sprite _binary_build_image048_spr_start;
extern const struct sprite _binary_build_image049_spr_start;
extern const struct sprite _binary_build_image050_spr_start;
extern const struct sprite _binary_build_image051_spr_start;
extern const struct sprite _binary_build_image052_spr_start;
extern const struct sprite _binary_build_image053_spr_start;
extern const struct sprite _binary_build_image054_spr_start;
extern const struct sprite _binary_build_image055_spr_start;
extern const struct sprite _binary_build_image056_spr_start;
extern const struct sprite _binary_build_image057_spr_start;
extern const struct sprite _binary_build_image058_spr_start;
extern const struct sprite _binary_build_image059_spr_start;
extern const struct sprite _binary_build_image060_spr_start;
extern const struct sprite _binary_build_image061_spr_start;
extern const struct sprite _binary_build_image062_spr_start;
extern const struct sprite _binary_build_image063_spr_start;
extern const struct sprite _binary_build_image064_spr_start;
extern const struct sprite _binary_build_image065_spr_start;
extern const struct sprite _binary_build_image066_spr_start;
extern const struct sprite _binary_build_image067_spr_start;
extern const struct sprite _binary_build_image068_spr_start;
extern const struct sprite _binary_build_image069_spr_start;
extern const struct sprite _binary_build_image070_spr_start;
extern const struct sprite _binary_build_image071_spr_start;
extern const struct sprite _binary_build_image072_spr_start;
extern const struct sprite _binary_build_image073_spr_start;
extern const struct sprite _binary_build_image074_spr_start;
extern const struct sprite _binary_build_image075_spr_start;
extern const struct sprite _binary_build_image076_spr_start;
extern const struct sprite _binary_build_image077_spr_start;
extern const struct sprite _binary_build_image078_spr_start;
extern const struct sprite _binary_build_image079_spr_start;
extern const struct sprite _binary_build_image080_spr_start;
extern const struct sprite _binary_build_image081_spr_start;
extern const struct sprite _binary_build_image082_spr_start;
extern const struct sprite _binary_build_image083_spr_start;
extern const struct sprite _binary_build_image084_spr_start;
extern const struct sprite _binary_build_image085_spr_start;
extern const struct sprite _binary_build_image086_spr_start;
extern const struct sprite _binary_build_image087_spr_start;
extern const struct sprite _binary_build_image088_spr_start;
extern const struct sprite _binary_build_image089_spr_start;
extern const struct sprite _binary_build_image090_spr_start;
extern const struct sprite _binary_build_image091_spr_start;
extern const struct sprite _binary_build_image092_spr_start;
extern const struct sprite _binary_build_image093_spr_start;
extern const struct sprite _binary_build_image094_spr_start;
extern const struct sprite _binary_build_image095_spr_start;
extern const struct sprite _binary_build_image096_spr_start;
extern const struct sprite _binary_build_image097_spr_start;
extern const struct sprite _binary_build_image098_spr_start;
extern const struct sprite _binary_build_image099_spr_start;
extern const struct sprite _binary_build_image100_spr_start;

static const struct sprite *const sprites[] = {
    &_binary_build_image001_spr_start,
    &_binary_build_image002_spr_start,
    &_binary_build_image003_spr_start,
    &_binary_build_image004_spr_start,
    &_binary_build_image005_spr_start,
    &_binary_build_image006_spr_start,
    &_binary_build_image007_spr_start,
    &_binary_build_image008_spr_start,
    &_binary_build_image009_spr_start,
    &_binary_build_image010_spr_start,
    &_binary_build_image011_spr_start,
    &_binary_build_image012_spr_start,
    &_binary_build_image013_spr_start,
    &_binary_build_image014_spr_start,
    &_binary_build_image015_spr_start,
    &_binary_build_image016_spr_start,
    &_binary_build_image017_spr_start,
    &_binary_build_image018_spr_start,
    &_binary_build_image019_spr_start,
    &_binary_build_image020_spr_start,
    &_binary_build_image021_spr_start,
    &_binary_build_image022_spr_start,
    &_binary_build_image023_spr_start,
    &_binary_build_image024_spr_start,
    &_binary_build_image025_spr_start,
    &_binary_build_image026_spr_start,
    &_binary_build_image027_spr_start,
    &_binary_build_image028_spr_start,
    &_binary_build_image029_spr_start,
    &_binary_build_image030_spr_start,
    &_binary_build_image031_spr_start,
    &_binary_build_image032_spr_start,
    &_binary_build_image033_spr_start,
    &_binary_build_image034_spr_start,
    &_binary_build_image035_spr_start,
    &_binary_build_image036_spr_start,
    &_binary_build_image037_spr_start,
    &_binary_build_image038_spr_start,
    &_binary_build_image039_spr_start,
    &_binary_build_image040_spr_start,
    &_binary_build_image041_spr_start,
    &_binary_build_image042_spr_start,
    &_binary_build_image043_spr_start,
    &_binary_build_image044_spr_start,
    &_binary_build_image045_spr_start,
    &_binary_build_image046_spr_start,
    &_binary_build_image047_spr_start,
    &_binary_build_image048_spr_start,
    &_binary_build_image049_spr_start,
    &_binary_build_image050_spr_start,
    &_binary_build_image051_spr_start,
    &_binary_build_image052_spr_start,
    &_binary_build_image053_spr_start,
    &_binary_build_image054_spr_start,
    &_binary_build_image055_spr_start,
    &_binary_build_image056_spr_start,
    &_binary_build_image057_spr_start,
    &_binary_build_image058_spr_start,
    &_binary_build_image059_spr_start,
    &_binary_build_image060_spr_start,
    &_binary_build_image061_spr_start,
    &_binary_build_image062_spr_start,
    &_binary_build_image063_spr_start,
    &_binary_build_image064_spr_start,
    &_binary_build_image065_spr_start,
    &_binary_build_image066_spr_start,
    &_binary_build_image067_spr_start,
    &_binary_build_image068_spr_start,
    &_binary_build_image069_spr_start,
    &_binary_build_image070_spr_start,
    &_binary_build_image071_spr_start,
    &_binary_build_image072_spr_start,
    &_binary_build_image073_spr_start,
    &_binary_build_image074_spr_start,
    &_binary_build_image075_spr_start,
    &_binary_build_image076_spr_start,
    &_binary_build_image077_spr_start,
    &_binary_build_image078_spr_start,
    &_binary_build_image079_spr_start,
    &_binary_build_image080_spr_start,
    &_binary_build_image081_spr_start,
    &_binary_build_image082_spr_start,
    &_binary_build_image083_spr_start,
    &_binary_build_image084_spr_start,
    &_binary_build_image085_spr_start,
    &_binary_build_image086_spr_start,
    &_binary_build_image087_spr_start,
    &_binary_build_image088_spr_start,
    &_binary_build_image089_spr_start,
    &_binary_build_image090_spr_start,
    &_binary_build_image091_spr_start,
    &_binary_build_image092_spr_start,
    &_binary_build_image093_spr_start,
    &_binary_build_image094_spr_start,
    &_binary_build_image095_spr_start,
    &_binary_build_image096_spr_start,
    &_binary_build_image097_spr_start,
    &_binary_build_image098_spr_start,
    &_binary_build_image099_spr_start,
    &_binary_build_image100_spr_start,
};

enum {
    NORM_BUTTON_SNOOZE,
    NORM_BUTTON_SETUP,
    NORM_BUTTON_SHUT_UP,
    NORM_BUTTON_ALARM_ON,
    NORM_BUTTON_CONFIRM,
    NORM_BUTTON_ALARM_OFF,
    NORM_BUTTON_ALARM_TIME,
    NORM_BUTTON_BACK,		       /* "exit setup" */
    NORM_BUTTON_RESET_TIME,
    NORM_BUTTON_SNOOZE_PERIOD,
    NORM_BUTTON_OFF_DAYS,
    ONDAY_BUTTON_MONDAY,
    ONDAY_BUTTON_TUESDAY,
    ONDAY_BUTTON_WEDNESDAY,
    ONDAY_BUTTON_THURSDAY,
    ONDAY_BUTTON_FRIDAY,
    ONDAY_BUTTON_SATURDAY,
    ONDAY_BUTTON_SUNDAY,
    ACTIVE_BUTTON_SNOOZE,
    ACTIVE_BUTTON_SETUP,
    ACTIVE_BUTTON_SHUT_UP,
    ACTIVE_BUTTON_ALARM_ON,
    ACTIVE_BUTTON_CONFIRM,
    ACTIVE_BUTTON_ALARM_OFF,
    ACTIVE_BUTTON_ALARM_TIME,
    ACTIVE_BUTTON_BACK,		       /* "exit setup" */
    ACTIVE_BUTTON_RESET_TIME,
    ACTIVE_BUTTON_SNOOZE_PERIOD,
    ACTIVE_BUTTON_OFF_DAYS,
    OFFDAY_BUTTON_MONDAY,
    OFFDAY_BUTTON_TUESDAY,
    OFFDAY_BUTTON_WEDNESDAY,
    OFFDAY_BUTTON_THURSDAY,
    OFFDAY_BUTTON_FRIDAY,
    OFFDAY_BUTTON_SATURDAY,
    OFFDAY_BUTTON_SUNDAY,
    TIME_FIXED,			       /* colon in main time display */
    TIME_HM_0,
    TIME_HM_1,
    TIME_HM_2,
    TIME_HM_3,
    TIME_HM_4,
    TIME_HM_5,
    TIME_HM_6,
    TIME_HM_7,
    TIME_HM_8,
    TIME_HM_9,
    TIME_S_0,
    TIME_S_1,
    TIME_S_2,
    TIME_S_3,
    TIME_S_4,
    TIME_S_5,
    TIME_S_6,
    TIME_S_7,
    TIME_S_8,
    TIME_S_9,
    NORM_ADJUST_H10_UP,
    NORM_ADJUST_H1_UP,
    NORM_ADJUST_M10_UP,
    NORM_ADJUST_M1_UP,
    NORM_ADJUST_S10_UP,
    NORM_ADJUST_S1_UP,
    NORM_ADJUST_H10_DN,
    NORM_ADJUST_H1_DN,
    NORM_ADJUST_M10_DN,
    NORM_ADJUST_M1_DN,
    NORM_ADJUST_S10_DN,
    NORM_ADJUST_S1_DN,
    ACTIVE_ADJUST_H10_UP,
    ACTIVE_ADJUST_H1_UP,
    ACTIVE_ADJUST_M10_UP,
    ACTIVE_ADJUST_M1_UP,
    ACTIVE_ADJUST_S10_UP,
    ACTIVE_ADJUST_S1_UP,
    ACTIVE_ADJUST_H10_DN,
    ACTIVE_ADJUST_H1_DN,
    ACTIVE_ADJUST_M10_DN,
    ACTIVE_ADJUST_M1_DN,
    ACTIVE_ADJUST_S10_DN,
    ACTIVE_ADJUST_S1_DN,
    WEEKDAY_MONDAY,
    WEEKDAY_TUESDAY,
    WEEKDAY_WEDNESDAY,
    WEEKDAY_THURSDAY,
    WEEKDAY_FRIDAY,
    WEEKDAY_SATURDAY,
    WEEKDAY_SUNDAY,
    ALARMTIME_FIXED,
    ALARMTIME_0,
    ALARMTIME_1,
    ALARMTIME_2,
    ALARMTIME_3,
    ALARMTIME_4,
    ALARMTIME_5,
    ALARMTIME_6,
    ALARMTIME_7,
    ALARMTIME_8,
    ALARMTIME_9,
    CONFIRM_ALARM,
    NBASESPRITES
};

#define NEXTRASPRITES 9 /* 3 time HM digits, 1 S, 5 alarm time */
#define MAXSPRITES (NBASESPRITES + NEXTRASPRITES)

/* From gconsts.c, output by graphics.ps */
extern const int time_offsets[4];
extern const int sec_offsets[2];
extern const int alarm_digit_offsets[6];

/*
 * This structure represents a sprite as eventually displayed: the
 * x and y coordinates might have been changed.
 */
struct displaysprite {
    int x, y, w, h;
    const unsigned short *pixels;
};

static struct displaysprite disp0[MAXSPRITES], disp1[MAXSPRITES];
static struct displaysprite *currdisp = disp0, *nextdisp = disp1;
static int currnsprites = -1, nextnsprites = 0;
static int currdim = -1;

void breakdown_time(int seconds, int *digits)
{
    digits[5] = seconds % 10; seconds /= 10;
    digits[4] = seconds % 6; seconds /= 6;
    digits[3] = seconds % 10; seconds /= 10;
    digits[2] = seconds % 6; seconds /= 6;
    digits[1] = seconds % 10; seconds /= 10;
    digits[0] = seconds;
}

int sprite_sort_cmp(const void *av, const void *bv)
{
    const struct displaysprite *a = (const struct displaysprite *)av;
    const struct displaysprite *b = (const struct displaysprite *)bv;
    if (a->y != b->y)
	return a->y < b->y ? -1 : +1;
    if (a->h != b->h)
	return a->h < b->h ? -1 : +1;
    if (a->x != b->x)
	return a->x < b->x ? -1 : +1;
    if (a->w != b->w)
	return a->w < b->w ? -1 : +1;
    if (a->pixels != b->pixels)
	return a->pixels < b->pixels ? -1 : +1;
    return 0;
}

int int_cmp(const void *av, const void *bv)
{
    const int *a = (const int *)av;
    const int *b = (const int *)bv;
    if (*a != *b)
	return *a < *b ? -1 : +1;
    return 0;
}

/*
 * Common code for functions that deal with the various modes in
 * which we're editing some value using the adjustment buttons.
 * Returns a pointer to the value in question, and optionally also
 * tells whether the adjustment buttons should wrap and which
 * indicator button should be displayed.
 */
int *editable_value(const struct pstate *ps, const struct lstate *ls,
		    int *wrapping, int *button)
{
    int btn, wrap;
    const int *ret;

    switch (ls->dmode) {
      case DMODE_SETALARM:
	ret = &ls->alarm_time;
	btn = ACTIVE_BUTTON_ALARM_ON;
	wrap = 1;
	break;
      case DMODE_SETSNOOZE:
	ret = &ls->snooze_time;
	btn = ACTIVE_BUTTON_SNOOZE;
	wrap = 0;
	break;
      case DMODE_SETDEFALARM:
	ret = &ps->defalarmtime;
	btn = ACTIVE_BUTTON_ALARM_TIME;
	wrap = 1;
	break;
      case DMODE_SETRESET:
	ret = &ps->resettime;
	btn = ACTIVE_BUTTON_RESET_TIME;
	wrap = 1;
	break;
      case DMODE_SETDEFSNOOZE:
	ret = &ps->snoozeperiod;
	btn = ACTIVE_BUTTON_SNOOZE_PERIOD;
	wrap = 0;
	break;
      default:
	assert(!"Shouldn't happen");
    }

    if (wrapping) *wrapping = wrap;
    if (button) *button = btn;
    return (int *)ret;
}

struct display_pixel_ctx {
    int nds;
    struct displaysprite **ds;
};

int display_pixel(void *vctx, int x, int y)
{
    struct display_pixel_ctx *ctx = (struct display_pixel_ctx *)vctx;
    int i;
    unsigned short pixel = 0;

    for (i = 0; i < ctx->nds; i++) {
	struct displaysprite *s = ctx->ds[i];
	int sx = x - s->x;
	int sy = y - s->y;
	/* we are guaranteed to be in range by our caller */
	pixel ^= s->pixels[sy * s->w + sx];
    }

    return pixel;
}

/*
 * The main display function.
 */
int display_update(int timeofday, int dayofweek, int date,
		   const struct pstate *ps, const struct lstate *ls,
		   struct button *buttons)
{
    int xext[2*MAXSPRITES+2], yext[2*MAXSPRITES+2];
    struct displaysprite *olds[MAXSPRITES], *news[MAXSPRITES];
    int nolds, nnews;
    int nxext, nyext, xe, ye;
    int i, j, digits[6];
    int dispdim;
    int nbuttons = 0;

    nextnsprites = 0;

#define PUTSPRITEAT(mindex, mxoff, myoff) do { \
    int index = (mindex), xoff = (mxoff), yoff = (myoff); \
    assert(nextnsprites < MAXSPRITES); \
    nextdisp[nextnsprites].x = sprites[index]->x + xoff; \
    nextdisp[nextnsprites].y = sprites[index]->y + yoff; \
    nextdisp[nextnsprites].w = sprites[index]->w; \
    nextdisp[nextnsprites].h = sprites[index]->h; \
    nextdisp[nextnsprites].pixels = sprites[index]->pixels; \
    nextnsprites++; \
} while (0)
#define PUTSPRITE(mindex) PUTSPRITEAT(mindex, 0, 0)
#define PUTBUTTON(mbindex, mindex) do { \
    int bindex = (mbindex), sindex = (mindex); \
    PUTSPRITEAT(sindex, 0, 0); \
    assert(nbuttons < MAXBUTTONS); \
    buttons[nbuttons].x = sprites[bindex]->x; \
    buttons[nbuttons].y = sprites[bindex]->y; \
    buttons[nbuttons].w = sprites[bindex]->w; \
    buttons[nbuttons].h = sprites[bindex]->h; \
    buttons[nbuttons].id = bindex; \
    nbuttons++; \
} while (0)
#define PUTPBUTTON(mbindex, mpindex) do { \
    int bbindex = (mbindex), ppindex = (mpindex); \
    PUTBUTTON(bbindex, (ls->pressed_button_id == bbindex ? \
			ppindex : bbindex)); \
} while (0)
    /*
     * Display policy section. Go through the state inputs and
     * work out what sprites we want to display where.
     */
    switch (ls->dmode) {
      case DMODE_NORMAL:
      case DMODE_CONFIGURE:
	/*
	 * Normal display mode, or top-level configuration menu.
	 * Always show the week day, and the time.
	 */
	PUTSPRITE(WEEKDAY_MONDAY + dayofweek);
	breakdown_time(timeofday, digits);
	for (i = 0; i < 4; i++)
	    if (i || digits[i])	       /* don't display leading 0 on hours */
		PUTSPRITEAT(TIME_HM_0 + digits[i], time_offsets[i], 0);
	for (i = 0; i < 2; i++)
	    PUTSPRITEAT(TIME_S_0 + digits[i+4], sec_offsets[i], 0);
	PUTSPRITE(TIME_FIXED);
	/*
	 * Depending on alarm mode, display the alarm time or the
	 * confirm message.
	 */
	if (ls->amode == AMODE_CONFIRM) {
	    /*
	     * The confirm-alarm message is not shown in configure
	     * mode, because it would seem strange to order the
	     * user to confirm the alarm while not displaying the
	     * buttons they need to press to do it.
	     */
	    if (ls->dmode == DMODE_NORMAL)
		PUTSPRITE(CONFIRM_ALARM);
	} else if (ls->amode == AMODE_ON) {
	    PUTSPRITE(ALARMTIME_FIXED);
	    breakdown_time(ls->alarm_time, digits);
	    for (i = 0; i < 6; i++)
		if (i || digits[i])    /* don't display leading 0 on hours */
		    PUTSPRITEAT(ALARMTIME_0 + digits[i],
				alarm_digit_offsets[i], 0);
	}
	if (ls->dmode == DMODE_CONFIGURE) {
	    /*
	     * In configure mode, always display all the configure
	     * buttons.
	     */
	    PUTPBUTTON(NORM_BUTTON_ALARM_TIME, ACTIVE_BUTTON_ALARM_TIME);
	    PUTPBUTTON(NORM_BUTTON_BACK, ACTIVE_BUTTON_BACK);
	    PUTPBUTTON(NORM_BUTTON_RESET_TIME, ACTIVE_BUTTON_RESET_TIME);
	    PUTPBUTTON(NORM_BUTTON_SNOOZE_PERIOD, ACTIVE_BUTTON_SNOOZE_PERIOD);
	    PUTPBUTTON(NORM_BUTTON_OFF_DAYS, ACTIVE_BUTTON_OFF_DAYS);
	    /*
	     * In configure mode, display is always bright.
	     */
	    dispdim = 0;
	} else {
	    /*
	     * Conditionally display the main buttons. All buttons
	     * are displayed if the touchscreen was recently
	     * active; SNOOZE and SHUT UP are displayed if the
	     * alarm is sounding; ALARM ON and ALARM OFF are
	     * displayed in confirm mode; otherwise, nothing is
	     * displayed.
	     */
	    if (ls->recent_touch || ls->alarm_sounding)
		PUTPBUTTON(NORM_BUTTON_SNOOZE, ACTIVE_BUTTON_SNOOZE);
	    if (ls->recent_touch)
		PUTPBUTTON(NORM_BUTTON_SETUP, ACTIVE_BUTTON_SETUP);
	    if (ls->recent_touch || ls->alarm_sounding)
		PUTPBUTTON(NORM_BUTTON_SHUT_UP, ACTIVE_BUTTON_SHUT_UP);
	    if (ls->recent_touch || ls->amode == AMODE_CONFIRM)
		PUTPBUTTON(NORM_BUTTON_ALARM_ON, ACTIVE_BUTTON_ALARM_ON);
	    if (ls->recent_touch)
		PUTPBUTTON(NORM_BUTTON_CONFIRM, ACTIVE_BUTTON_CONFIRM);
	    if (ls->recent_touch || ls->amode == AMODE_CONFIRM)
		PUTPBUTTON(NORM_BUTTON_ALARM_OFF, ACTIVE_BUTTON_ALARM_OFF);
	    /*
	     * Display is dim unless something interesting has
	     * happened.
	     */
	    dispdim = !(ls->recent_touch ||
			ls->alarm_sounding ||
			ls->amode == AMODE_CONFIRM);
	}
	break;

      case DMODE_SETALARM:
      case DMODE_SETSNOOZE:
      case DMODE_SETDEFALARM:
      case DMODE_SETRESET:
      case DMODE_SETDEFSNOOZE:
	/*
	 * In all of these modes, we are displaying some
	 * particular time from one of the configuration
	 * structures, the adjustment arrows, and the button to
	 * indicate which setting it was.
	 */
	{
	    int button;
	    const int *editable = editable_value(ps, ls, NULL, &button);
	    breakdown_time(*editable, digits);
	    PUTSPRITE(button);
	}
	PUTSPRITE(TIME_FIXED);
	for (i = 0; i < 4; i++)
	    PUTSPRITEAT(TIME_HM_0 + digits[i], time_offsets[i], 0);
	for (i = 0; i < 2; i++)
	    PUTSPRITEAT(TIME_S_0 + digits[i+4], sec_offsets[i], 0);
	PUTPBUTTON(NORM_ADJUST_H10_UP, ACTIVE_ADJUST_H10_UP);
	PUTPBUTTON(NORM_ADJUST_H1_UP, ACTIVE_ADJUST_H1_UP);
	PUTPBUTTON(NORM_ADJUST_M10_UP, ACTIVE_ADJUST_M10_UP);
	PUTPBUTTON(NORM_ADJUST_M1_UP, ACTIVE_ADJUST_M1_UP);
	PUTPBUTTON(NORM_ADJUST_S10_UP, ACTIVE_ADJUST_S10_UP);
	PUTPBUTTON(NORM_ADJUST_S1_UP, ACTIVE_ADJUST_S1_UP);
	PUTPBUTTON(NORM_ADJUST_H10_DN, ACTIVE_ADJUST_H10_DN);
	PUTPBUTTON(NORM_ADJUST_H1_DN, ACTIVE_ADJUST_H1_DN);
	PUTPBUTTON(NORM_ADJUST_M10_DN, ACTIVE_ADJUST_M10_DN);
	PUTPBUTTON(NORM_ADJUST_M1_DN, ACTIVE_ADJUST_M1_DN);
	PUTPBUTTON(NORM_ADJUST_S10_DN, ACTIVE_ADJUST_S10_DN);
	PUTPBUTTON(NORM_ADJUST_S1_DN, ACTIVE_ADJUST_S1_DN);
	/*
	 * Display the BACK button, to return to normal mode.
	 */
	PUTPBUTTON(NORM_BUTTON_BACK, ACTIVE_BUTTON_BACK);
	/*
	 * In setup modes, display is always bright.
	 */
	dispdim = 0;
	break;

      case DMODE_SETOFFDAYS:
	/*
	 * In off-days mode, we display the seven off-day buttons,
	 * plus the off-days button as a mode indicator.
	 */
	PUTSPRITE(ACTIVE_BUTTON_OFF_DAYS);
	for (i = 0; i < 7; i++) {
	    if (ps->offdays & (1 << i))
		PUTBUTTON(OFFDAY_BUTTON_MONDAY + i, ONDAY_BUTTON_MONDAY + i);
	    else
		PUTBUTTON(OFFDAY_BUTTON_MONDAY + i, OFFDAY_BUTTON_MONDAY + i);
	}
	/*
	 * Display the BACK button, to return to normal mode.
	 */
	PUTPBUTTON(NORM_BUTTON_BACK, ACTIVE_BUTTON_BACK);
	/*
	 * In setup modes, display is always bright.
	 */
	dispdim = 0;
    }

    /*
     * Sort the list of sprites.
     */
    qsort(nextdisp, nextnsprites, sizeof(*nextdisp), sprite_sort_cmp);

    /*
     * Find all the extremal x and y coordinates of all the
     * sprites, both old and new. This lets us divide the screen
     * into an irregular grid of subrectangles, within each of
     * which all pixels are part of the same subset of the
     * sprites. We can then process each subrectangle uniformly.
     */
    nxext = nyext = 0;
    xext[nxext++] = yext[nyext++] = 0;
    for (i = 0; i < nextnsprites; i++) {
	xext[nxext++] = nextdisp[i].x;
	xext[nxext++] = nextdisp[i].x + nextdisp[i].w;
	yext[nyext++] = nextdisp[i].y;
	yext[nyext++] = nextdisp[i].y + nextdisp[i].h;
    }
    for (i = 0; i < currnsprites; i++) {
	xext[nxext++] = currdisp[i].x;
	xext[nxext++] = currdisp[i].x + currdisp[i].w;
	yext[nyext++] = currdisp[i].y;
	yext[nyext++] = currdisp[i].y + currdisp[i].h;
    }
    qsort(xext, nxext, sizeof(*xext), int_cmp);
    qsort(yext, nyext, sizeof(*yext), int_cmp);
    for (i = j = 0; i < nxext; i++)
	if (j == 0 || xext[j-1] != xext[i])
	    xext[j++] = xext[i];
    nxext = j;
    xext[j] = scr_width;
    for (i = j = 0; i < nyext; i++)
	if (j == 0 || yext[j-1] != yext[i])
	    yext[j++] = yext[i];
    nyext = j;
    yext[j] = scr_height;

    /*
     * Now, for each subrectangle, go through the sprite list and
     * determine which sprites we need to draw in that
     * subrectangle. Also, do that for the old sprite list, and
     * see if the lists are the same.
     */
    for (ye = 0; ye < nyext; ye++) {
	int y0 = yext[ye], y1 = yext[ye+1];
	for (xe = 0; xe < nxext; xe++) {
	    int x0 = xext[xe], x1 = xext[xe+1];

	    nolds = 0;
	    for (i = 0; i < currnsprites; i++)
		if (x0 >= currdisp[i].x && x0 < currdisp[i].x+currdisp[i].w &&
		    y0 >= currdisp[i].y && y0 < currdisp[i].y+currdisp[i].h)
		    olds[nolds++] = &currdisp[i];
	    nnews = 0;
	    for (i = 0; i < nextnsprites; i++)
		if (x0 >= nextdisp[i].x && x0 < nextdisp[i].x+nextdisp[i].w &&
		    y0 >= nextdisp[i].y && y0 < nextdisp[i].y+nextdisp[i].h)
		    news[nnews++] = &nextdisp[i];

	    if (nolds == nnews) {
		/*
		 * The lists might be identical.
		 */
		for (i = 0; i < nolds; i++)
		    if (sprite_sort_cmp(olds[i], news[i]))
			break;

		if (i == nolds)
		    continue;	       /* they were! */
	    }

	    {
		struct display_pixel_ctx ctx;
		ctx.nds = nnews;
		ctx.ds = news;
		update_display(x0, y0, x1-x0, y1-y0, display_pixel, &ctx);
	    }
	}
    }

    currnsprites = nextnsprites;
    {
	struct displaysprite *tmp = currdisp;
	currdisp = nextdisp;
	nextdisp = tmp;
    }

    /*
     * Dim the display appropriately.
     */
    if (always_bright)
	dispdim = 0;
    if (currdim != dispdim) {
	dim_display(dispdim);
	currdim = dispdim;
    }

    /*
     * Expand the button rectangles a bit, to make it easier to
     * hit them. They may overlap as a result, but the next level
     * of code up from here will deal sensibly with apportioning
     * the overlap space fairly to the contending buttons.
     */
    for (i = 0; i < nbuttons; i++) {
	buttons[i].x -= buttons[i].w / 2;
	buttons[i].w *= 2;
	buttons[i].y -= buttons[i].h / 2;
	buttons[i].h *= 2;
    }

    return nbuttons;
}

/* Depends on the inputs being in the range 0..86399. */
#define TIMEAFTER(now,then) (((now)-(then) + 86400) % 86400)

/*
 * Determine whether we've just ticked past a given time of day.
 * In an ideal world, we could check this simply by testing
 * whether TIMEAFTER(now,that_time) had become smaller (because
 * it'd have ticked from 86399 to 0). This isn't an ideal world,
 * though, because that will also be true if the clock moves
 * backward - either for DST changes or simply because the
 * automated clock resync (which is non-monotonic on the Chumby)
 * moves it back a second or two. Hence, instead we check if the
 * new TIMEAFTER value is earlier than the previous one _by at
 * least twelve hours_, which means the clock can jump by up to
 * twelve hours at a time in either direction and we'll still
 * correctly spot whether we've moved backward or forward across
 * the alarm time.
 */
#define TICKEDPAST(ltod,tod,then) \
    (TIMEAFTER(ltod,then) > TIMEAFTER(tod,then) + 43200)

/*
 * Set up the default amode for a given time of day. That's
 * AMODE_OFF most of the time, except that if we're between the
 * default reset time and the default alarm time and the next
 * alarm time does not occur on an off-day, it's AMODE_CONFIRM.
 */
void default_mode(int timeofday, int dayofweek, int date,
		  struct pstate *ps, struct lstate *ls)
{
    int afterreset;
    
    ls->amode = AMODE_OFF;

    afterreset = TIMEAFTER(timeofday, ps->resettime);
    if (afterreset < TIMEAFTER(ps->defalarmtime, ps->resettime)) {
	/*
	 * When's the next alarm time? It might be today or
	 * tomorrow.
	 */
	if (timeofday >= ps->defalarmtime) {
	    dayofweek = (dayofweek+1) % 7;
	    date++;
	}
	if ((ps->offdays & (1 << dayofweek)) && !day_excluded(date))
	    ls->amode = AMODE_CONFIRM;
    }

    ls->amode_default = 1;
}

void event_startup(int timeofday, int dayofweek, int date,
		   struct pstate *ps, struct lstate *ls)
{
    default_mode(timeofday, dayofweek, date, ps, ls);
}

void event_updated_excdata(int timeofday, int dayofweek, int date,
			   struct pstate *ps, struct lstate *ls)
{
    /*
     * Exceptions are updated, so readjust our default mode if
     * we're in it.
     */
    if (ls->amode_default)
	default_mode(timeofday, dayofweek, date, ps, ls);
}

void event_timeout(struct pstate *ps, struct lstate *ls)
{
    ls->dmode = DMODE_NORMAL;
    ls->saved_hours_digit = -1;
}

int event_timetick(int lasttimeofday, int timeofday, int dayofweek, int date,
		   struct pstate *ps, struct lstate *ls)
{
    int ret = 0;

    /*
     * If we've just ticked past the reset time while in AMODE_OFF
     * or a default mode, we call default_mode() to optionally go
     * into AMODE_CONFIRM.
     * 
     * If we've just ticked past the alarm time, we should
     * _unconditionally_ reset to default mode: that's our strong
     * guarantee that everything done in the amodes has temporary
     * effect. However, _which_ alarm time is slightly more
     * subtle: it should be the one-off alarm time in AMODE_ON
     * (and in that case we also go beep!), or the default alarm
     * time in any other mode.
     */
    if ((ls->amode == AMODE_OFF || ls->amode_default) &&
	TICKEDPAST(lasttimeofday, timeofday, ps->resettime)) {
	default_mode(timeofday, dayofweek, date, ps, ls);
    } else if (ls->amode == AMODE_ON &&
	       TICKEDPAST(lasttimeofday, timeofday, ls->alarm_time)) {
	default_mode(timeofday, dayofweek, date, ps, ls);
	/*
	 * Special case: if we're configuring the immediate snooze
	 * time, the alarm doesn't sound. This is so we can cancel
	 * a snooze by dialling the snooze timer down to zero.
	 */
	if (ls->dmode != DMODE_SETSNOOZE)
	    ls->alarm_sounding = 1;
    } else if (ls->amode != AMODE_ON &&
	       TICKEDPAST(lasttimeofday, timeofday, ps->defalarmtime)) {
	default_mode(timeofday, dayofweek, date, ps, ls);
    }

    /*
     * If we're a decent half-hour before the reset time, suggest
     * that this might be a good moment to go to our exception
     * server and download the current list of exceptions.
     */
    {
	int exctime = (ps->resettime + 86400 - 30*60) % 86400;
	if (TICKEDPAST(lasttimeofday, timeofday, exctime))
	    ret |= GET_EXCEPTIONS;
    }

    return ret;
}

static void set_snooze(int timeofday, struct lstate *ls)
{
    ls->amode = AMODE_ON;
    ls->amode_default = 0; 
    ls->alarm_time = (timeofday + ls->snooze_time) % 86400;
}

void event_button(int button, int timeofday, int dayofweek, int date,
		  struct pstate *ps, struct lstate *ls)
{
    int *p, wrap, adj;

    switch (button) {
      case NORM_BUTTON_SNOOZE:
	ls->alarm_sounding = 0;
	ls->dmode = DMODE_SETSNOOZE;
	ls->snooze_time = ps->snoozeperiod;
	set_snooze(timeofday, ls);
	ls->saved_hours_digit = -1;
	break;
      case NORM_BUTTON_SETUP:
	ls->dmode = DMODE_CONFIGURE;
	break;
      case NORM_BUTTON_SHUT_UP:
	ls->alarm_sounding = 0;
	default_mode(timeofday, dayofweek, date, ps, ls);
	break;
      case NORM_BUTTON_ALARM_ON:
	ls->dmode = DMODE_SETALARM;
	ls->alarm_time = ps->defalarmtime;
	ls->amode = AMODE_ON;
	ls->amode_default = 0; 
	ls->saved_hours_digit = -1;
	break;
      case NORM_BUTTON_CONFIRM:
	ls->amode = AMODE_CONFIRM;
	ls->amode_default = 0; 
	break;
      case NORM_BUTTON_ALARM_OFF:
	ls->amode = AMODE_OFF;
	ls->amode_default = 0; 
	ls->recent_touch = 0;	       /* immediately darken the display */
	break;
      case NORM_BUTTON_ALARM_TIME:
	ls->dmode = DMODE_SETDEFALARM;
	ls->saved_hours_digit = -1;
	break;
      case NORM_BUTTON_BACK:
	ls->dmode = DMODE_NORMAL;
	break;
      case NORM_BUTTON_RESET_TIME:
	ls->dmode = DMODE_SETRESET;
	ls->saved_hours_digit = -1;
	break;
      case NORM_BUTTON_SNOOZE_PERIOD:
	ls->dmode = DMODE_SETDEFSNOOZE;
	ls->saved_hours_digit = -1;
	break;
      case NORM_BUTTON_OFF_DAYS:
	ls->dmode = DMODE_SETOFFDAYS;
	break;
      case NORM_ADJUST_H10_UP:
      case NORM_ADJUST_H1_UP:
      case NORM_ADJUST_M10_UP:
      case NORM_ADJUST_M1_UP:
      case NORM_ADJUST_S10_UP:
      case NORM_ADJUST_S1_UP:
      case NORM_ADJUST_H10_DN:
      case NORM_ADJUST_H1_DN:
      case NORM_ADJUST_M10_DN:
      case NORM_ADJUST_M1_DN:
      case NORM_ADJUST_S10_DN:
      case NORM_ADJUST_S1_DN:
	switch (button) {
	  case NORM_ADJUST_H10_UP: adj = +36000; break;
	  case NORM_ADJUST_H1_UP: adj = +3600; break;
	  case NORM_ADJUST_M10_UP: adj = +600; break;
	  case NORM_ADJUST_M1_UP: adj = +60; break;
	  case NORM_ADJUST_S10_UP: adj = +10; break;
	  case NORM_ADJUST_S1_UP: adj = +1; break;
	  case NORM_ADJUST_H10_DN: adj = -36000; break;
	  case NORM_ADJUST_H1_DN: adj = -3600; break;
	  case NORM_ADJUST_M10_DN: adj = -600; break;
	  case NORM_ADJUST_M1_DN: adj = -60; break;
	  case NORM_ADJUST_S10_DN: adj = -10; break;
	  case NORM_ADJUST_S1_DN: adj = -1; break;
	}
	p = editable_value(ps, ls, &wrap, NULL);
	if (wrap) {
	    /*
	     * Special case when dealing with the upper digit of
	     * the hours: if the digit becomes 2 and the old units
	     * digit of the hours is greater than 3 and hence
	     * undisplayable, we save it, and restore if the next
	     * action is to change this digit again.
	     */
	    if (abs(adj) == 36000) {
		int h1 = *p / 36000, h2 = *p / 3600 % 10;
		h1 += adj / 36000;
		h1 = (h1 + 3) % 3;
		if (h1 == 2 && h2 > 3) {
		    ls->saved_hours_digit = h2;
		    h2 = 3;
		} else {
		    if (h1 != 2 && ls->saved_hours_digit > 0)
			h2 = ls->saved_hours_digit;
		    ls->saved_hours_digit = -1;
		}
		*p = (h1 * 10 + h2) * 3600 + (*p % 3600);
	    } else {
		/*
		 * The normal case is simple.
		 */
		*p += adj;
		*p = (*p + 86400) % 86400;
	    }
	} else {
	    *p += adj;
	    if (*p < 0)
		*p = 0;
	    else if (*p >= 86400)
		*p = 86399;
	}
	if (ls->dmode == DMODE_SETSNOOZE)
	    set_snooze(timeofday, ls);
	break;
      case OFFDAY_BUTTON_MONDAY:
      case OFFDAY_BUTTON_TUESDAY:
      case OFFDAY_BUTTON_WEDNESDAY:
      case OFFDAY_BUTTON_THURSDAY:
      case OFFDAY_BUTTON_FRIDAY:
      case OFFDAY_BUTTON_SATURDAY:
      case OFFDAY_BUTTON_SUNDAY:
	ps->offdays ^= 1 << (button - OFFDAY_BUTTON_MONDAY);
	break;
    }
}
