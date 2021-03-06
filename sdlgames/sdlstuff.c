/*
 * sdlstuff.c: helpful interface routines to talk to SDL on behalf
 * of simple game programs.
 */

#include <stdlib.h>
#include <fcntl.h>
#ifdef unix
#include <sys/time.h>
#endif

#ifdef PS2
#include <linux/ps2/dev.h>
#endif /* PS2 */

#include <SDL/SDL.h>

#include "sdlstuff.h"

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#ifdef PS2

/* ----------------------------------------------------------------------
 * PS2 `dx'-setting code. This is to fiddle with the screen
 * alignment if it doesn't quite match up to the positioning of the
 * physical screen.
 */

static int ps2gs_open(void) {
    int i, ps2gs_fd;

    /*
     * /dev/ps2gs is a single-access device, so if we want to shift
     * dx in mid-game then we can't just open(2) it because we will
     * get EBUSY. So instead, we start by looking in /proc/self/fd
     * and see if we can find any fd which is already pointing at
     * that device; if we do, we'll dup it (so our copy can be
     * closed) and that will do the right thing.
     */
    for (i = 0; i < 1024; i++) {
	char buf[40], outbuf[80];
	int ret;
	sprintf(buf, "/proc/self/fd/%d", i);
	ret = readlink(buf, outbuf, sizeof(outbuf));
	outbuf[sizeof(outbuf)-1] = 0;
	if (ret > 0 && ret < sizeof(outbuf)) {
	    outbuf[ret] = 0;
	    if (!strcmp(outbuf, "/dev/ps2gs"))
		return dup(i);
	}
    }
    ps2gs_fd = open("/dev/ps2gs", O_RDWR);
    if (ps2gs_fd < 0) { perror("/dev/ps2gs: open"); exit(1); }
    return ps2gs_fd;
}

int get_dx(void)
{
    struct ps2_display disp;
    int ret, ps2gs_fd;
    ps2gs_fd = ps2gs_open();
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_GDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_GDISPLAY)"); exit(1); }
    close(ps2gs_fd);
    return disp.dx;
}

int get_dy(void)
{
    struct ps2_display disp;
    int ret, ps2gs_fd;
    ps2gs_fd = ps2gs_open();
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_GDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_GDISPLAY)"); exit(1); }
    close(ps2gs_fd);
    return disp.dy;
}

void set_dx(int dx)
{
    struct ps2_display disp;
    int ret, ps2gs_fd;
    ps2gs_fd = ps2gs_open();
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_GDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_GDISPLAY)"); exit(1); }
    disp.dx = dx;
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_SDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_SDISPLAY)"); exit(1); }
    close(ps2gs_fd);
}

void set_dy(int dy)
{
    struct ps2_display disp;
    int ret, ps2gs_fd;
    ps2gs_fd = ps2gs_open();
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_GDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_GDISPLAY)"); exit(1); }
    disp.dy = dy;
    disp.ch = 0;
    ret = ioctl(ps2gs_fd, PS2IOC_SDISPLAY, &disp);
    if (ret < 0) { perror("/dev/ps2gs: ioctl(PS2IOC_SDISPLAY)"); exit(1); }
    close(ps2gs_fd);
}

#endif /* PS2 */

/* ----------------------------------------------------------------------
 * main program.
 */

#ifdef PS2
int dx, dy;
int evfd = -1;
#endif /* PS2 */
long long sparetime;
SDL_Surface *screen;

int cleaned_up = 0;

void cleanup(void)
{
    if (!cleaned_up) {
	SDL_Quit();
#ifdef PS2
	set_dx(dx);
	set_dy(dy);
	if (evfd >= 0) close(evfd);
#endif /* PS2 */
    }
    cleaned_up = 1;
}

/*
 * The audio function callback takes the following parameters:
 * stream:  A pointer to the audio buffer to be filled
 * len:     The length (in bytes) of the audio buffer
 */
void fill_audio(void *udata, Uint8 *stream, int len)
{
    int i;

    for (i = 0; i < len; i++) stream[i] = (i & 64 ? 0x40 : 0);
}

int setup(void)
{
#ifdef PS2
    evfd = open("/dev/ps2event", O_RDONLY);
    if (evfd < 0) { perror("/dev/ps2event: open"); return 1; }
    ioctl(evfd, PS2IOC_ENABLEEVENT, PS2EV_VSYNC);
    dx = get_dx();
    dy = get_dy();
#endif /* PS2 */

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    cleaned_up = 0;
    atexit(cleanup);

    /*
     * Set the video mode up.
     */
    screen = SDL_SetVideoMode(SCR_WIDTH*XMULT, SCR_HEIGHT*YMULT,
			      8, SDL_SWSURFACE);
    if (!screen) {
	printf("SDL screen initialisation error: %s\n", SDL_GetError());
	return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
}

void vsync(void)
{
#ifdef PS2
    ioctl(evfd, PS2IOC_WAITEVENT, PS2EV_VSYNC);
#else
    static int last_vsync, last_vsync_used = 0;
    int time_now, next_vsync;
#ifdef unix
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = 1000000 * tv.tv_sec + tv.tv_usec;
#else
    time_now = 1000 * SDL_GetTicks();
#endif
    if (!last_vsync_used)
	last_vsync = time_now;
    next_vsync = last_vsync + 20000;
    if (next_vsync - time_now > 0) {
#ifdef unix
	usleep(next_vsync - time_now);
#else
	SDL_Delay((next_vsync - time_now) / 1000);
#endif
    }
    last_vsync = next_vsync;
    last_vsync_used = 1;
#endif
}

void scr_prep(void)
{
    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);
}

void scr_done(void)
{
    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);
    SDL_Flip(screen);
}

#if 0

/*
 * This might be handy for measuring how much CPU time is spare out
 * of each frame of a game.
 */
long long bigclock(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec) * 1000000LL + tv.tv_usec;
}

int main(int argc, char **argv)
{
    SDL_Surface *screen;
    SDL_AudioSpec audio_wanted;
    int frame;
    int evfd;
    int x, y;

    evfd = open("/dev/ps2event", O_RDONLY);
    if (evfd < 0) { perror("/dev/ps2event: open"); return 1; }
    ioctl(evfd, PS2IOC_ENABLEEVENT, PS2EV_VSYNC);

    dx = get_dx();
    dy = get_dy();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    atexit(cleanup);

    if (argc > 1 && !strcmp(argv[1], "-j")) {
	int i;
	SDL_Event event;
	SDL_Joystick *joystick;

	printf("%d joysticks were found.\n\n", SDL_NumJoysticks() );
	printf("The names of the joysticks are:\n");
	for (i=0; i < SDL_NumJoysticks(); i++) {
	    printf("    %s\n", SDL_JoystickName(i));
	}

	joystick = SDL_JoystickOpen(0);
	printf("Joystick 0 has %d axes\n", SDL_JoystickNumAxes(joystick));
	printf("Joystick 0 has %d buttons\n", SDL_JoystickNumButtons(joystick));
	printf("Joystick 0 has %d balls\n", SDL_JoystickNumBalls(joystick));
	printf("Joystick 0 has %d hats\n", SDL_JoystickNumHats(joystick));

	while(SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYAXISMOTION:
		printf("Axis %d %d\n", event.jaxis.axis, event.jaxis.value);
		break;
	      case SDL_JOYBUTTONDOWN:
		printf("Button %d down\n", event.jbutton.button);
		break;
	      case SDL_JOYBUTTONUP:
		printf("Button %d up\n", event.jbutton.button);
		break;
	      case SDL_KEYDOWN:
		goto done;
		break;
	    }
	}
	done:

	return 0;
    }

    /*
     * Set the audio device up.
     */
    audio_wanted.freq = 22050;
    audio_wanted.format = AUDIO_S8;
    audio_wanted.channels = 1;    /* 1 = mono, 2 = stereo */
    audio_wanted.samples = 1024;  /* Good low-latency value for callback */
    audio_wanted.callback = fill_audio;
    audio_wanted.userdata = NULL;
    if ( SDL_OpenAudio(&audio_wanted, NULL) < 0 ) {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }

    /*
     * Set the video mode up.
     */
    screen = SDL_SetVideoMode(SCR_WIDTH*XMULT, SCR_HEIGHT*YMULT,
			      8, SDL_SWSURFACE);
    if (!screen) {
	printf("SDL screen initialisation error: %s\n", SDL_GetError());
	return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);

    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);
    for (x = 0; x < SCR_WIDTH*XMULT; x++) {
	for (y = 0; y < SCR_HEIGHT*YMULT; y++) {
	    putpixel(screen, x, y,
		     SDL_MapRGB(screen->format, (x*10)&255,
				(y*10)&255, ((x+y+frame)*10)&255));
	}
    }
    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);

    SDL_PauseAudio(0);

    for (frame = 1; frame < 256; frame++) {
	SDL_Rect r1 = { 0, 0, SCR_WIDTH*XMULT-1, SCR_HEIGHT*YMULT };
	SDL_Rect r2 = { 1, 0, SCR_WIDTH*XMULT-1, SCR_HEIGHT*YMULT };
	
	if (SDL_MUSTLOCK(screen))
	    SDL_LockSurface(screen);
//	for (x = 0; x < SCR_WIDTH*XMULT; x++) {
	    for (y = 0; y < SCR_HEIGHT*YMULT; y++) {
		memmove(screen->pixels + y * screen->pitch,
			screen->pixels + y * screen->pitch + screen->format->BytesPerPixel,
			(SCR_WIDTH*XMULT-1) * screen->format->BytesPerPixel);
		putpixel(screen, SCR_WIDTH*XMULT-1, y, 0);
		//putpixel(screen, x, y, (x==SCR_WIDTH*XMULT-1 ? 0 :
		//			getpixel(screen, x+1, y)));
	    }
//	}
	if (SDL_MUSTLOCK(screen))
	    SDL_UnlockSurface(screen);
	//SDL_UpdateRect(screen, frame*2, 0, 2, SCR_HEIGHT*YMULT);
	SDL_Flip(screen);
	{
	    long long t = bigclock();
	    ioctl(evfd, PS2IOC_WAITEVENT, PS2EV_VSYNC);
	    sparetime += bigclock() - t;
	}
    }

    return 0;
}
#endif

