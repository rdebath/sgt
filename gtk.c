/*
 * gtk.c: Driver code for a GTK port of Tring, for local testing
 * purposes.
 */

#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <alsa/asoundlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "tring.h"

#define SCR_WIDTH 320
#define SCR_HEIGHT 240

int scr_width = SCR_WIDTH;
int scr_height = SCR_HEIGHT;

static snd_pcm_t *pcm;
static const signed short *sound_playing;
int sound_length, sound_offset;

struct pollfd *pollfds;
int npollfds, currpollfds;
GSource **sources;

GtkWidget *win, *area;
GdkPixmap *pixmap;
guchar rgbbuf[SCR_WIDTH * SCR_HEIGHT * 3];

int tring_event, tring_x, tring_y;

static void repaint_rect(int x, int y, int w, int h)
{
    GdkGC *gc = gdk_gc_new(area->window);
    gdk_draw_pixmap(area->window, gc, pixmap, x, y, x, y, w, h);
    gdk_gc_unref(gc);
}

static gint button_event(GtkWidget *widget, GdkEventButton *event,
                         gpointer data)
{
    int button;

    if (event->type == GDK_BUTTON_PRESS) {
        tring_event = EV_PRESS;
        tring_x = event->x;
        tring_y = event->y;
        gtk_main_quit();
    } else if (event->type == GDK_BUTTON_RELEASE) {
        tring_event = EV_RELEASE;
        gtk_main_quit();
    }

    return TRUE;
}

static gint expose_area(GtkWidget *widget, GdkEventExpose *event,
                        gpointer data)
{
    repaint_rect(event->area.x, event->area.y,
                 event->area.width, event->area.height);
    return TRUE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
    tring_event = -1;
    gtk_main_quit();
}

void drivers_init(void)
{
    int err;

    /*
     * Open the sound output device via the ALSA library.
     */
    err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(err));
	exit(1);
    }
    err = snd_pcm_nonblock(pcm, 1);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_nonblock: %s\n", snd_strerror(err));
	exit(1);
    }
    sound_playing = NULL;
    sound_offset = sound_length = 0;

    /*
     * Allocate space for ALSA's pollfds.
     */
    npollfds = snd_pcm_poll_descriptors_count(pcm);
    assert(npollfds > 0);
    pollfds = malloc(npollfds * sizeof(*pollfds));
    sources = malloc(npollfds * sizeof(*sources));
    if (!pollfds || !sources) {
	fprintf(stderr, "out of memory\n");
	exit(1);
    }
    currpollfds = 0;

    /*
     * Start up GTK and create a window.
     */
    gtk_init(NULL, NULL);
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, SCR_WIDTH, SCR_HEIGHT);
    gtk_container_add(GTK_CONTAINER(win), area);
    gtk_widget_show(area);

    gtk_widget_realize(win);
    pixmap = gdk_pixmap_new(win->window, SCR_WIDTH, SCR_HEIGHT, -1);

    gtk_signal_connect(GTK_OBJECT(area), "expose_event",
		       GTK_SIGNAL_FUNC(expose_area), NULL);
    gtk_signal_connect(GTK_OBJECT(area), "button_press_event",
		       GTK_SIGNAL_FUNC(button_event), NULL);
    gtk_signal_connect(GTK_OBJECT(area), "button_release_event",
		       GTK_SIGNAL_FUNC(button_event), NULL);
    gtk_signal_connect(GTK_OBJECT(win), "destroy",
		       GTK_SIGNAL_FUNC(destroy), NULL);

    gtk_widget_add_events(GTK_WIDGET(area),
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK);

    gtk_widget_show(win);
}

void update_display(int x, int y, int w, int h, pixelfn_t pix, void *ctx)
{
    GdkGC *gc;
    int xx, yy;

    for (yy = y; yy < y+h; yy++)
	for (xx = x; xx < x+w; xx++) {
	    int pixel = pix(ctx, xx, yy);
            int r = ((pixel >> 11) & 31) * 255 / 31;
            int g = ((pixel >> 5) & 63) * 255 / 31;
            int b = ((pixel >> 0) & 31) * 255 / 31;
            rgbbuf[3 * (SCR_WIDTH * yy + xx) + 0] = r;
            rgbbuf[3 * (SCR_WIDTH * yy + xx) + 1] = g;
            rgbbuf[3 * (SCR_WIDTH * yy + xx) + 2] = b;
        }

    gc = gdk_gc_new(area->window);
    gdk_draw_rgb_image(pixmap, gc, x, y, w, h,
                       GDK_RGB_DITHER_NORMAL,
                       rgbbuf + 3 * (SCR_WIDTH * y + x), 3 * SCR_WIDTH);
    gdk_gc_unref(gc);
    repaint_rect(x, y, w, h);
}

void dim_display(int dim)
{
    gtk_window_set_title(GTK_WINDOW(win),
                         dim ? "tring (dim)" : "TRING (BRIGHT)");
}

void play_sound(const signed short *samples, int nsamples, int samplerate)
{
    int err;
    snd_pcm_hw_params_t *hwp;

    if (sound_playing)
	return;
    snd_pcm_hw_params_alloca(&hwp);

    err = snd_pcm_hw_params_any(pcm, hwp);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params_any: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_set_format(pcm, hwp, SND_PCM_FORMAT_S16);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params_set_format: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_set_access(pcm, hwp, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params_set_access: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_set_channels(pcm, hwp, 1);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params_set_channels: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params_set_rate(pcm, hwp, samplerate, 0);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params_set_rate: %s\n", snd_strerror(err));
	return;
    }
    err = snd_pcm_hw_params(pcm, hwp);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_hw_params: %s\n", snd_strerror(err));
	return;
    }

    sound_playing = samples;
    sound_length = nsamples;
    sound_offset = 0;
    assert(nsamples > 0);

    err = snd_pcm_poll_descriptors(pcm, pollfds, npollfds);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_poll_descriptors: %s\n", snd_strerror(err));
	return;
    }
    currpollfds = err;

    snd_pcm_start(pcm);
}

static gboolean timeout(gpointer data)
{
    tring_event = EV_TIMEOUT;
    gtk_main_quit();
    return TRUE;
}

static gboolean alsapoll(GIOChannel *source, GIOCondition cond, gpointer data)
{
    int err;

    /*
     * Attempt some sound output.
     */
    if (sound_offset == sound_length) {
        snd_pcm_drain(pcm);
        sound_playing = NULL;
        sound_offset = 0;
        sound_length = 0;
        currpollfds = 0;
        /*
         * We could return an event here indicating
         * that we had successfully completed playing
         * a sound.
         */
    }
    err = snd_pcm_writei(pcm, sound_playing + sound_offset,
                         sound_length - sound_offset);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_writei: %s\n",
                snd_strerror(err));
        sound_playing = NULL;
        sound_length = sound_offset = 0;
        currpollfds = 0;
    } else {
        sound_offset += err;
    }
    gtk_main_quit();
    return TRUE;
}

int get_event(int msec, int *x, int *y)
{
    guint timeout_id;
    int i, thispollfds;

    tring_event = EV_NOTHING;

    timeout_id = g_timeout_add(msec, timeout, NULL);
    thispollfds = currpollfds;
    for (i = 0; i < thispollfds; i++) {
        GIOChannel *channel = g_io_channel_unix_new(pollfds[i].fd);
        sources[i] = g_io_create_watch(channel, pollfds[i].events);
        g_source_set_priority(sources[i], G_PRIORITY_HIGH);
        g_source_set_callback(sources[i], (GSourceFunc)alsapoll, NULL, NULL);
        g_io_channel_unref(channel);
        g_source_attach(sources[i], g_main_context_default());
    }

    gtk_main();

    for (i = 0; i < thispollfds; i++) {
        g_source_destroy(sources[i]);
    }
    g_source_remove(timeout_id);

    if (tring_event == -1)
        exit(0);
    if (tring_event == EV_PRESS) {
        *x = tring_x;
        *y = tring_y;
    }
    return tring_event;
}
