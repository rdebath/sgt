/*
 * chumby.c: Driver code for the various Chumby hardware.
 */

/*
 * Documentation for the interfaces to the Chumby hardware, since
 * having gone to the effort of working it out I ought to write it
 * down somewhere and this seems a reasonable place:
 * 
 * The frame buffer
 * ----------------
 * 
 * /dev/fb0 contains a 320x240 bitmap in 16-bit little-endian true
 * colour. That is, it's 153600 bytes of data, consisting of 76800
 * (= 320*240) little-endian halfwords, each consisting of 16 bits
 * reading (from MSB to LSB) RRRRRGGGGGGBBBBB. One can read/write/
 * seek the framebuffer, but it's more sensible to mmap it.
 *
 * The touchscreen
 * ---------------
 * 
 * If you open /dev/ts and read from it, you will receive a stream
 * of 8-byte event records. Each one is a "struct ts_event" as
 * defined in the kernel's drivers/input/tsdev.c, which means it
 * consists of four little-endian halfwords which respectively
 * contain pressure, x, y and a timestamp field:
 *  - Pressure is zero for a release event, and non-zero for a
 *    press or drag. In principle you can also use it to
 *    distinguish a hard press from a soft press, but personally I
 *    wouldn't base any UI behaviour on that!
 *  - x and y are not measured in screen coordinates. The Chumby's
 *    own touchscreen calibration software leaves a text file in
 *    /psp/ts_settings that stores the mapping between the two.
 *    That text file consists of one line containing four integers
 *    P,Q,R,S separated by commas; now the touchscreen
 *    x-coordinates of the left and right display edges are
 *    respectively P and P+Q, and the y-coordinates of the top and
 *    bottom are respectively R and R+S. So you can convert the
 *    x,y fields of a touchscreen event into pixel coordinates by
 *    setting X = 320 * (x-P) / Q and Y = 240 * (y-R) / S. Note
 *    that S will typically be negative, because the framebuffer's
 *    y coordinates increase downwards whereas the touchscreen's
 *    increase upwards.
 *  - The timestamp field is derived by taking the tv_usec field
 *    of an ordinary gettimeofday and dividing it by 100.
 *
 * Dimming the LCD backlight
 * -------------------------
 * 
 * Open /proc/sys/sense1/dimlevel, and write ASCII '0', '1' or '2'
 * to it. That sets how _dim_ it is, not how bright; i.e. 2 turns
 * the display completely off, and 0 is full brightness.
 * 
 * Sound
 * -----
 * 
 * Just use ALSA. Don't try using the OSS-style /dev/dsp; I tried
 * it, and you get weird sound distortions. ALSA isn't much fun to
 * code, but the sound works properly if you use it.
 *
 * Squeeze sensor and accelerometer
 * --------------------------------
 * 
 * I've no idea how you program these - I don't use them.
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
int npollfds_nonaudio, npollfds_audio, npollfds_both, npollfds_curraudio;

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
	    ret = read(fd, buf, sizeof(buf)-1);
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
    if (!pollfds) {
	fprintf(stderr, "out of memory\n");
	exit(1);
    }
    pollfds[0].fd = touchscreen_fd;
    pollfds[0].events = POLLIN;
    npollfds_curraudio = 0;
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
    npollfds_curraudio = err;

    snd_pcm_start(pcm);
}

int get_event(int msec, int *x, int *y)
{
    int ret;
    int nfds;

    nfds = npollfds_nonaudio;
    if (sound_playing)
	nfds += npollfds_curraudio;

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
	    (pcm, pollfds + npollfds_nonaudio, npollfds_curraudio, &revents);
	if (err < 0) {
	    fprintf(stderr, "snd_pcm_poll_descriptors_revents: %s\n",
		    snd_strerror(err));
	    sound_playing = NULL;
	    sound_length = sound_offset = 0;
	} else if (revents & POLLOUT) {
            if (sound_offset == sound_length) {
                snd_pcm_drain(pcm);
                sound_playing = NULL;
                sound_offset = 0;
                sound_length = 0;
                /*
                 * We could return an event here indicating
                 * that we had successfully completed playing
                 * a sound.
                 */
            } else {
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
