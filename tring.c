/*
 * tring.c: Main program for Tring.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <curl/curl.h>

#include "tring.h"

int always_bright = 0;
char *settings_file = NULL;
char *exception_url = NULL;

char *settings_tmpfile = NULL;
static struct pstate last_saved = { {-1,-1,-1,-1,-1,-1,-1}, -1, -1, -1 };

extern const signed short alarmsound[];
extern const size_t alarmsound_len;

static void gettime(int *timeofday, int *dayofweek, int *date, int *mstonext)
{
    struct timeval tv;
    struct tm tm;

    gettimeofday(&tv, NULL);
    tm = *localtime(&tv.tv_sec);

    if (mstonext)
	*mstonext = ((1000000 - tv.tv_usec) + 500) / 1000;

    if (timeofday)
	*timeofday = (tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec;

    if (dayofweek)
	*dayofweek = (tm.tm_wday + 6) % 7;

    if (date)
	*date = 10000 * (1900+tm.tm_year) + 100 * (1+tm.tm_mon) + tm.tm_mday;
}

static unsigned long long get_monoclk(void)
{
    unsigned long long ns;
    struct timeval ts;		       /* struct timespec ts; */
    gettimeofday(&ts, NULL); /* clock_gettime(CLOCK_MONOTONIC, &ts); */
    ns = ts.tv_sec;
    ns = (ns * 1000000 + ts.tv_usec) * 1000;   /* adjust for tv_nsec */
    return ns;
}

static void load_settings(struct pstate *ps)
{
    FILE *fp;
    char buf[4096];
    int day;

    if (!settings_file)
	return;

    fp = fopen(settings_file, "r");
    if (!fp)
	return;
    while (fgets(buf, sizeof(buf), fp)) {
	unsigned h, m, s;

	if (sscanf(buf, "defalarmtime %u:%u:%u", &h, &m, &s) == 3) {
            /* Legacy configuration format with only one default alarm time */
            for (day = 0; day < 7; day++) {
                ps->defalarmtime[day] = ((h*60+m)*60+s) % 86400;
                last_saved.defalarmtime[day] = ps->defalarmtime[day];
            }
	} else if (sscanf(buf, "defalarmtime %d %u:%u:%u",
                          &day, &h, &m, &s) == 4) {
            ps->defalarmtime[day] = ((h*60+m)*60+s) % 86400;
            last_saved.defalarmtime[day] = ps->defalarmtime[day];
	} else if (sscanf(buf, "resettime %u:%u:%u", &h, &m, &s) == 3) {
	    ps->resettime = ((h*60+m)*60+s) % 86400;
	    last_saved.resettime = ps->resettime;
	} else if (sscanf(buf, "snoozeperiod %u:%u:%u", &h, &m, &s) == 3) {
	    ps->snoozeperiod = ((h*60+m)*60+s) % 86400;
	    last_saved.snoozeperiod = ps->snoozeperiod;
	} else if (sscanf(buf, "offdays %u", &s) == 1) {
	    ps->offdays = s & 0x7F;
	    last_saved.offdays = ps->offdays;
	}
    }
    fclose(fp);
}

static void save_settings(struct pstate *ps)
{
    char buf[4096];
    FILE *fp;
    int day;

    if (!settings_file)
	return;

    /*
     * Don't save the same data we last saved.
     */
    for (day = 0; day < 7; day++)
        if (ps->defalarmtime[day] != last_saved.defalarmtime[day])
            goto save;
    if (ps->resettime != last_saved.resettime ||
	ps->snoozeperiod != last_saved.snoozeperiod ||
	ps->offdays != last_saved.offdays)
	goto save;
    return;
  save:

    if (!settings_tmpfile) {
	settings_tmpfile = malloc(10 + strlen(settings_file));
	if (!settings_tmpfile)
	    return;
	sprintf(settings_tmpfile, "%s.tmp", settings_file);
    }

    fp = fopen(settings_tmpfile, "w");
    if (!fp)
	return;

    for (day = 0; day < 7; day++) {
        sprintf(buf, "defalarmtime %d %02d:%02d:%02d\n", day,
                ps->defalarmtime[day] / 3600, ps->defalarmtime[day] / 60 % 60,
                ps->defalarmtime[day] % 60);
        if (fputs(buf, fp) < 0) {
            fclose(fp);
            return;
        }
    }

    sprintf(buf, "resettime %02d:%02d:%02d\n",
	    ps->resettime / 3600, ps->resettime / 60 % 60,
	    ps->resettime % 60);
    if (fputs(buf, fp) < 0) {
	fclose(fp);
	return;
    }

    sprintf(buf, "snoozeperiod %02d:%02d:%02d\n",
	    ps->snoozeperiod / 3600, ps->snoozeperiod / 60 % 60,
	    ps->snoozeperiod % 60);
    if (fputs(buf, fp) < 0) {
	fclose(fp);
	return;
    }

    sprintf(buf, "offdays %d\n", ps->offdays);
    if (fputs(buf, fp) < 0) {
	fclose(fp);
	return;
    }

    if (fclose(fp) < 0)
	return;

    if (rename(settings_tmpfile, settings_file) < 0)
	return;

    /*
     * Successfully written the new settings file over the old
     * one. We can now update our internal record of what we've
     * saved, so that we don't try again next time.
     */
    last_saved = *ps;		       /* structure copy */
}

static int zero_pixel(void *vctx, int x, int y) { return 0; }

char *excdata, excdatalocal[65536];
int excdatapos, excdatasize;
int subproc_started;
sig_atomic_t subproc_status = 0;
sigset_t sigchldset;
#define MAXEXCEPTS 32
int excepts[MAXEXCEPTS];
int nexcepts;

void sigchld(int signum)
{
    int status;

    waitpid(-1, &status, WNOHANG);

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
	if (WIFEXITED(status) && WEXITSTATUS(status) == 5)
	    subproc_status = -1;
	else
	    subproc_status = 0;
	sigprocmask(SIG_BLOCK, &sigchldset, NULL);
    }
}

size_t gotexcdata(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t ret = size * nmemb;

    if (ret > excdatasize-1 - excdatapos)
	ret = excdatasize-1 - excdatapos;

    memcpy(excdata + excdatapos, ptr, ret);
    excdatapos += ret;

    return ret;
}

void start_excsubproc(void)
{
    pid_t pid;

    /*
     * Start a subprocess that fetches the current exception list
     * from our exception server, if we've got one.
     */

    if (!exception_url || !excdata || subproc_started || subproc_status == 1)
	return;

    subproc_started = 1;

    pid = fork();
    if (pid > 0) {
	subproc_status = 1;
	sigprocmask(SIG_UNBLOCK, &sigchldset, NULL);
    } else if (pid == 0) {
	/*
	 * We are the child process. Faff about with libcurl.
	 */

	CURL *c;

	excdatapos = 0;
	excdata[0] = '\0';

	c = curl_easy_init();
	if (!c) exit(1);
	if (curl_easy_setopt(c, CURLOPT_URL, exception_url)
	    != CURLE_OK) exit(2);
	if (curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, gotexcdata)
	    != CURLE_OK) exit(3);
	if (curl_easy_setopt(c, CURLOPT_WRITEDATA, NULL)
	    != CURLE_OK) exit(4);
	if (curl_easy_perform(c) != 0) exit(5);
	curl_easy_cleanup(c);

	if (excdatapos >= 0 && excdatapos < excdatasize)
	    excdata[excdatapos] = '\0';

	exit(0);
    }
}

int day_excluded(int date)
{
    int i;

    for (i = 0; i < nexcepts; i++)
	if (excepts[i] == date)
	    return 1;

    return 0;
}

int process_excdata(char *data)
{
    int exc[MAXEXCEPTS];
    int nexc = 0;
    int ok = 0;

    while (*data) {
	unsigned y, m, d;

	if (sscanf(data, "%u-%u-%u", &y, &m, &d) == 3) {
	    exc[nexc] = 10000*y + 100*m + d;
	    nexc++;
	    if (nexc == MAXEXCEPTS)
		break;
	} else if (!strncmp(data, "done", 4)) {
	    memcpy(excepts, exc, sizeof(excepts));
	    nexcepts = nexc;
            ok = 1;
	}

	data += strcspn(data, "\r\n");
	data += strspn(data, "\r\n");
    }

    return ok;
}

int main(int argc, char **argv)
{
    int tod, wd, date, day;
    struct pstate aps, *ps = &aps;
    struct lstate als, *ls = &als;
    struct button buttons[MAXBUTTONS];
    unsigned long long recent_touch_timeout;
    int nbuttons;
    int buttonid = -1, buttonactive = 0;

    sigemptyset(&sigchldset);
    sigaddset(&sigchldset, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigchldset, NULL);
    signal(SIGCHLD, sigchld);

    excdatasize = sizeof(excdatalocal);
    excdata = mmap(NULL, excdatasize, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    /*
     * Process arguments.
     */
    while (--argc > 0) {
	char *p = *++argv;
	if (!strncmp(p, "-f", 2)) {
	    char *v = p+2;
	    if (!*v) {
		if (--argc > 0)
		    v = *++argv;
		else
		    fprintf(stderr, "option '-f' expects a filename\n");
	    }

	    settings_file = v;
	} else if (!strncmp(p, "-u", 2)) {
	    char *v = p+2;
	    if (!*v) {
		if (--argc > 0)
		    v = *++argv;
		else
		    fprintf(stderr, "option '-u' expects a filename\n");
	    }

	    exception_url = v;
	} else if (!strcmp(p, "-b"))
	    always_bright = 1;
	else
	    fprintf(stderr, "unrecognised option '%s'\n", p);
    }

    /*
     * Core default settings are arranged to be _slightly_ silly:
     * if I set them to my own actual preferred settings then I
     * wouldn't be able to tell whether the load-from-config
     * function was working :-)
     */
    for (day = 0; day < 7; day++)
        ps->defalarmtime[day] = TIMEOFDAY(7,59,59);
    ps->resettime = TIMEOFDAY(11,59,59);
    ps->snoozeperiod = TIMEOFDAY(0,8,59);
    ps->offdays = 0x7F;

    load_settings(ps);

    ls->amode = AMODE_OFF;
    ls->amode_default = 1;
    ls->alarm_time = 0;
    ls->snooze_time = 0;
    ls->alarm_sounding = 0;
    ls->recent_touch = 0;
    ls->dmode = DMODE_NORMAL;
    ls->pressed_button_id = -1;
    ls->saved_hours_digit = -1;
    ls->network_fault = 0;

    curl_global_init(CURL_GLOBAL_ALL);

    drivers_init();

    /* Clear the screen. */
    update_display(0, 0, scr_width, scr_height, zero_pixel, NULL);

    gettime(&tod, &wd, &date, NULL);
    event_startup(tod, wd, date, ps, ls);
    nbuttons = display_update(tod, wd, date, ps, ls, buttons);

    /* On bootup, look for our exception list. */
    start_excsubproc();

    while (1) {
	int ms, ev, x, y;
	int ltod, lwd;

	ltod = tod;
	lwd = wd;
	gettime(&tod, &wd, &date, &ms);
	if ((tod+1) % 86400 == ltod) {
	    /*
	     * Special case: we've already ticked the clock on
	     * from tod to ltod, but the system still thinks it's
	     * ltod. In that case, we pretend we're right and the
	     * system is wrong.
	     */
	    wd = lwd;
	    tod = ltod;
	    ms += 1000;		       /* now wait until the _next_ second */
	}

	if (ls->recent_touch) {
	    unsigned long long ns = get_monoclk();
	    if (ns > recent_touch_timeout) {
		ls->recent_touch = 0;
		event_timeout(ps, ls);
	    }
	}

	ev = get_event(ms, &x, &y);

	if (ev == EV_TIMEOUT) {
	    tod = (tod + 1) % 86400;
	    if (tod == 0)
		wd = (wd + 1) % 7;
	} else if (ev == EV_PRESS) {
	    int bpress, bdist, i;

	    /*
	     * See if the touch was on an actual button. If two
	     * buttons overlap, we disambiguate by awarding the
	     * press to the button whose centre is at the smallest
	     * Euclidean distance from the press point.
	     */
	    bpress = -1;
	    for (i = 0; i < nbuttons; i++) {
		int bx = x - buttons[i].x;
		int by = y - buttons[i].y;
		if (bx >= 0 && bx < buttons[i].w &&
		    by >= 0 && by < buttons[i].h) {
		    int cx = 2*bx - buttons[i].w;
		    int cy = 2*by - buttons[i].h;
		    int dist = cx * cx + cy * cy;
		    if (bpress < 0 || bdist > dist) {
			bpress = buttons[i].id;
			bdist = dist;
		    }
		}
	    }
	    if (bpress >= 0) {
		/*
		 * Activate or deactivate a button.
		 */
		if (buttonid < 0)
		    buttonid = bpress;
		buttonactive = (buttonid == bpress);
	    } else {
		buttonactive = 0;
	    }
	} else if (ev == EV_RELEASE) {
	    if (buttonid >= 0 && buttonactive) {
		/*
		 * There was a button press. Process it.
		 */
		event_button(buttonid, tod, wd, date, ps, ls);

		/*
		 * In modes other than DMODE_NORMAL, this extends
		 * the recent-touch timeout so that the mode
		 * persists for as long as the user keeps pressing
		 * things.
		 */
		if (ls->dmode != DMODE_NORMAL) {
		    ls->recent_touch = 1;
		    recent_touch_timeout = get_monoclk() + 5000000000ULL;
		}
	    } else {
		/*
		 * A touch which was on no button at all, in
		 * DMODE_NORMAL, causes the display to light up
		 * and the main buttons to appear.
		 */
		ls->recent_touch = 1;
		recent_touch_timeout = get_monoclk() + 5000000000ULL;
	    }
	    buttonactive = 0;
	    buttonid = -1;
	}

	if (ltod != tod) {
	    int ret = event_timetick(ltod, tod, wd, date, ps, ls);
	    if (ls->alarm_sounding) {
		play_sound(alarmsound,
                           alarmsound_len / sizeof(signed short), 44100);
	    }
	    if ((ret & GET_EXCEPTIONS))
		start_excsubproc();

	    if (subproc_started && subproc_status != 1) {
		subproc_started = 0;
		memcpy(excdatalocal, excdata, excdatasize);
		excdatalocal[excdatasize-1] = '\0';
		if (subproc_status < 0 || !process_excdata(excdatalocal)) {
                    ls->network_fault = 1;
                } else {
                    event_updated_excdata(tod, wd, date, ps, ls);
                }
	    }
	}

	ls->pressed_button_id = (buttonactive ? buttonid : -1);
	nbuttons = display_update(tod, wd, date, ps, ls, buttons);

	if (ls->dmode == DMODE_NORMAL)
	    save_settings(ps);
    }

    dim_display(1);

    return 0;
}
