/*
 * sdlstuff.c: helpful interface routines to talk to SDL on behalf
 * of simple game programs.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <linux/ps2/dev.h>
#include <sys/time.h>

#include <SDL/SDL.h>

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

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

/* ----------------------------------------------------------------------
 * PS2 `dx'-setting code.
 */

static int ps2gs_open(void) {
    int ps2gs_fd = open("/dev/ps2gs", O_RDWR);
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

/* ----------------------------------------------------------------------
 * main program.
 */

int dx;
long long sparetime;
int evfd = -1;
SDL_Surface *screen;

int cleaned_up = 0;

void cleanup(void)
{
    if (!cleaned_up) {
	SDL_Quit();
	set_dx(dx);
	if (evfd >= 0) close(evfd);
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
    evfd = open("/dev/ps2event", O_RDONLY);
    if (evfd < 0) { perror("/dev/ps2event: open"); return 1; }
    ioctl(evfd, PS2IOC_ENABLEEVENT, PS2EV_VSYNC);

    dx = get_dx();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    cleaned_up = 0;
    atexit(cleanup);

    /*
     * Set the video mode up.
     */
    screen = SDL_SetVideoMode(640, 240, 8, SDL_SWSURFACE);
    if (!screen) {
	printf("SDL screen initialisation error: %s\n", SDL_GetError());
	return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
}

int vsync(void)
{
    ioctl(evfd, PS2IOC_WAITEVENT, PS2EV_VSYNC);
}

#if 0
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
    screen = SDL_SetVideoMode(640, 240, 8, SDL_SWSURFACE);
    if (!screen) {
	printf("SDL screen initialisation error: %s\n", SDL_GetError());
	return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);

    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);
    for (x = 0; x < 640; x++) {
	for (y = 0; y < 240; y++) {
	    putpixel(screen, x, y,
		     SDL_MapRGB(screen->format, (x*10)&255,
				(y*10)&255, ((x+y+frame)*10)&255));
	}
    }
    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);

    SDL_PauseAudio(0);

    for (frame = 1; frame < 256; frame++) {
	SDL_Rect r1 = { 0, 0, 639, 240 };
	SDL_Rect r2 = { 1, 0, 639, 240 };
	
	if (SDL_MUSTLOCK(screen))
	    SDL_LockSurface(screen);
//	for (x = 0; x < 640; x++) {
	    for (y = 0; y < 240; y++) {
		memmove(screen->pixels + y * screen->pitch,
			screen->pixels + y * screen->pitch + screen->format->BytesPerPixel,
			639 * screen->format->BytesPerPixel);
		putpixel(screen, 639, y, 0);
		//putpixel(screen, x, y, (x==639 ? 0 :
		//			getpixel(screen, x+1, y)));
	    }
//	}
	if (SDL_MUSTLOCK(screen))
	    SDL_UnlockSurface(screen);
	//SDL_UpdateRect(screen, frame*2, 0, 2, 240);
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

