/*
 * chumby.c: Driver code for the various Chumby hardware.
 */

#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <alsa/asoundlib.h>

#include "tring.h"

#define SCR_WIDTH 320
#define SCR_HEIGHT 240

int scr_width = SCR_WIDTH;
int scr_height = SCR_HEIGHT;

static unsigned short *framebuf;
static int framebuf_fd;

static int touchscreen_fd;
int ts_left, ts_width, ts_top, ts_height;

static snd_pcm_t *pcm;
static const signed short *sound_playing;
int sound_length, sound_offset;

struct pollfd *pollfds;
int npollfds_nonaudio, npollfds_audio, npollfds_both;

void drivers_init(void)
{
    int err;

    /*
     * Open frame buffer device /dev/fb0, and mmap it so that we
     * have the frame buffer directly accessible in our address
     * space.
     */
    framebuf_fd = open("/dev/fb0", O_RDWR);
    if (framebuf_fd < 0) {
	perror("/dev/fb0: open");
	exit(1);
    }
    framebuf = (unsigned short *)mmap
	(NULL, 320*240*sizeof(unsigned short), PROT_READ | PROT_WRITE,
	 MAP_SHARED, framebuf_fd, (off_t)0);
    if (!framebuf) {
	perror("/dev/fb0: mmap");
	exit(1);
    }

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
     * Open the touchscreen device for reading.
     */
    touchscreen_fd = open("/dev/ts", O_RDONLY);
    if (touchscreen_fd < 0) {
	perror("/dev/ts: open");
	exit(1);
    }
    /*
     * Fetch the touchscreen calibration data.
     */
    {
	char buf[4096];
	int got = 0;
	int fd, ret;

	fd = open("/psp/ts_settings", O_RDONLY);
	if (fd >= 0) {
	    ret = read(fd, buf, sizeof(buf));
	    if (ret >= 0) {
		buf[ret] = '\0';
		buf[strcspn(buf, "\r\n")] = '\0';
		if (sscanf(buf, "%d,%d,%d,%d", &ts_left,
			   &ts_width, &ts_top, &ts_height) == 4) {
		    got = 1;
		} else {
		    fprintf(stderr, "/psp/ts_settings: data \"%s\""
			    " was malformatted\n", buf);
		}
	    } else {
		perror("/psp/ts_settings: read");
	    }
	    close(fd);
	} else {
	    perror("/psp/ts_settings: open");
	}

	if (!got) {
	    /*
	     * Default values pulled off ChumbyWiki:
	     * http://wiki.chumby.com/mediawiki/index.php/Chumby_tricks#Screwed_up_your_touchscreen_calibration.3F
	     */
	    ts_left = 136;
	    ts_width = 3768;
	    ts_top = 3835;
	    ts_height = -3540;
	}
    }

    /*
     * Allocate space for pollfds, and set up the non-audio ones.
     */
    npollfds_nonaudio = 1;	       /* the touchscreen */
    npollfds_audio = snd_pcm_poll_descriptors_count(pcm);
    assert(npollfds_audio > 0);
    npollfds_both = npollfds_nonaudio + npollfds_audio;
    pollfds = malloc(npollfds_both * sizeof(*pollfds));
    pollfds[0].fd = touchscreen_fd;
    pollfds[0].events = POLLIN;
}

void update_display(int x, int y, int w, int h, pixelfn_t pix, void *ctx)
{
    int xx, yy;

    for (yy = y; yy < y+h; yy++)
	for (xx = x; xx < x+w; xx++)
	    framebuf[SCR_WIDTH * yy + xx] = pix(ctx, xx, yy);
}

void dim_display(int dim)
{
    /*
     * Chumby display brightness setting: we open
     * /proc/sys/sense1/dimlevel and write a decimal integer to
     * it, which is one of
     * 
     *  - 0: undimmed (full brightness)
     *  - 1: dimmed (low brightness)
     *  - 2: completely dimmed (display off)
     */
    char c = (dim ? '1' : '0');
    int fd = open("/proc/sys/sense1/dimlevel", O_WRONLY);
    if (fd >= 0) {
	write(fd, &c, 1);
	close(fd);
    }
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

    err = snd_pcm_poll_descriptors(pcm, pollfds + npollfds_nonaudio,
				   npollfds_audio);
    if (err < 0) {
	fprintf(stderr, "snd_pcm_poll_descriptors: %s\n", snd_strerror(err));
	return;
    }

    snd_pcm_start(pcm);
}

int get_event(int msec, int *x, int *y)
{
    int ret;
    int nfds;

    if (sound_playing)
	nfds = npollfds_both;
    else
	nfds = npollfds_nonaudio;

    do {
	ret = poll(pollfds, nfds, msec);
    } while (ret < 0 && (errno == EINTR || errno == EAGAIN));

    if (ret < 0) {
	perror("poll");
	exit(1);		       /* I see nothing we can do here! */
    }

    if (ret == 0)
	return EV_TIMEOUT;

    if (sound_playing) {
	unsigned short revents;
	int err;

	err = snd_pcm_poll_descriptors_revents
	    (pcm, pollfds + npollfds_nonaudio, npollfds_audio, &revents);
	if (err < 0) {
	    fprintf(stderr, "snd_pcm_poll_descriptors_revents: %s\n",
		    snd_strerror(err));
	    sound_playing = NULL;
	    sound_length = sound_offset = 0;
	} else if (revents & POLLOUT) {
	    /*
	     * Do some sound output.
	     */
	    err = snd_pcm_writei(pcm, sound_playing + sound_offset,
				 sound_length - sound_offset);
	    if (err < 0) {
		fprintf(stderr, "snd_pcm_writei: %s\n",
			snd_strerror(err));
		sound_playing = NULL;
		sound_length = sound_offset = 0;
	    } else {
		sound_offset += err;
		if (sound_offset == sound_length) {
		    sound_playing = NULL;
		    sound_offset = 0;
		    sound_length = 0;
		    /*
		     * We could return an event here indicating
		     * that we had successfully completed playing
		     * a sound.
		     */
		}
	    }
	}
    }

    if (pollfds[0].revents & POLLIN) {
	/*
	 * Touchscreen event.
	 */
	struct ts_event {
	    signed short p, x, y, time;
	};
	struct ts_event ev;
	int ret;

	ret = read(touchscreen_fd, &ev, sizeof(ev));
	if (ret < 0) {
	    perror("/dev/ts: read");
	    exit(1);
	    /*
	     * FIXME: work out better error handling here! Perhaps
	     * we should put up an obvious indication on the
	     * display somehow, and then carry on running without
	     * touchscreen functionality? We'd at least still be
	     * displaying the time, and the user would know to
	     * reboot or try to figure out what had happened.
	     */
	} else if (ret < sizeof(ev)) {
	    /*
	     * According to my reading of the kernel device driver
	     * code, this should never happen.
	     */
	    fprintf(stderr, "/dev/ts: read: returned less than a full "
		    "record\n");
	    exit(1);		       /* FIXME: better error handling? */
	} else if (ev.p == 0) {
	    return EV_RELEASE;
	} else {
	    *x = (ev.x - ts_left) / (double)ts_width * SCR_WIDTH;
	    if (*x < 0) *x = 0;
	    if (*x >= SCR_WIDTH) *x = SCR_WIDTH - 1;
	    *y = (ev.y - ts_top) / (double)ts_height * SCR_HEIGHT;
	    if (*y < 0) *y = 0;
	    if (*y >= SCR_WIDTH) *y = SCR_WIDTH - 1;
	    return EV_PRESS;
	}
    }

    /*
     * If we reach here, it must be because a sound event
     * triggered and nothing else. Return the code which causes
     * the caller to recompute their timeout and go round again.
     */
    return EV_NOTHING;
}
