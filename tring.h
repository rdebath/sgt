/*
 * tring.h: Main header file for Tring.
 */

/*
 * A hardware driver module provides the following functions.
 */
extern int scr_width, scr_height;
typedef int (*pixelfn_t)(void *ctx, int x, int y);
enum {
    EV_NOTHING, /* just call get_event again */
    EV_TIMEOUT, /* time ran out */
    EV_PRESS,   /* touchscreen pressed; x and y were written */
    EV_RELEASE, /* touchscreen released */
};
void drivers_init(void);	       /* initialise everything at startup */
void update_display(int x, int y, int w, int h, pixelfn_t pix, void *ctx);
void dim_display(int dim);
void play_sound(const signed short *samples, int nsamples, int samplerate);
int get_event(int msec, int *x, int *y);

/*
 * Persistent state for Tring.
 * 
 * Times of day are stored as an integer in [0,86400), giving
 * seconds since midnight. The snooze period is an integer in the
 * same range, and simply counts the length of a time interval in
 * seconds.
 * 
 * offdays is a 7-bit mask, with 1=Monday and 64=Sunday. A set bit
 * indicates a potential alarm on that day; a clear bit indicates
 * the alarm is disabled.
 */
struct pstate {
    int defalarmtime;
    int resettime;
    int snoozeperiod;
    int offdays;
};

/*
 * Less persistent state.
 */
enum {
    AMODE_OFF, AMODE_CONFIRM, AMODE_ON
};
enum {
    DMODE_NORMAL,
    DMODE_CONFIGURE,
    DMODE_SETALARM,
    DMODE_SETSNOOZE,
    DMODE_SETDEFALARM,
    DMODE_SETRESET,
    DMODE_SETDEFSNOOZE,
    DMODE_SETOFFDAYS,
};
struct lstate {
    int amode;
    int amode_default;		       /* was last amode change a default one? */
    int alarm_time;		       /* only meaningful if amode==AMODE_ON */
    int snooze_time;		       /* if we're configuring a snooze */
    int alarm_sounding;
    int recent_touch;
    int dmode;
    int pressed_button_id;
    int saved_hours_digit;
    int network_fault;
};

#define TIMEOFDAY(h,m,s) (((h)*60+(m))*60+(s))

/*
 * The display function in display.c. Updates the display, given
 * the clock state and the time of day. Also returns a list of
 * button structures indicating the parts of the display which
 * should now be considered to be pressable buttons; those are
 * written into the "buttons" parameter, and the number of them is
 * returned. (It won't exceed MAXBUTTONS.)
 */
struct button {
    int x, y, w, h;
    int id;
};
#define MAXBUTTONS 16
int display_update(int timeofday, int dayofweek, int date,
		   const struct pstate *ps, const struct lstate *ls,
		   struct button *buttons);

/*
 * The event-handling functions that update the clock's state.
 * These are also in display.c because they share with it
 * knowledge of what set of display buttons exist. I'd like a more
 * modularised approach to this, but it'll do for now.
 */
void event_startup(int timeofday, int dayofweek, int date,
		   struct pstate *ps, struct lstate *ls);
void event_timeout(struct pstate *ps, struct lstate *ls);
int event_timetick(int lasttimeofday, int timeofday, int dayofweek, int date,
		   struct pstate *ps, struct lstate *ls);
void event_button(int button, int timeofday, int dayofweek, int date,
		  struct pstate *ps, struct lstate *ls);
/* Flags returned from event_timetick: */
enum {
    GET_EXCEPTIONS = 1
};

/*
 * Global flags set by command-line options.
 */
extern int always_bright;

int day_excluded(int date);

#ifdef INTERNAL
/*
 * Functions used inside display.c and only exposed when doing unit tests.
 */
void default_mode(int timeofday, int dayofweek, int date,
		  struct pstate *ps, struct lstate *ls);
#endif /* INTERNAL */
