/*
 * `linuxrc' program, suitable for booting a standalone PS2 game
 * collection off an initial RAM disk loaded from a memory card.
 */

#include <stdio.h>
#include <fcntl.h>
#include <linux/ps2/dev.h>
#include <sys/mount.h>

#include "beebfont.h"
#include "swash.h"
#include "sdlstuff.h"
#include "game256.h"

#define JOY_THRESHOLD 4096

static char const *modules[] = {
    "ps2rtc",
    "ps2debuglog",
    "soundcore",
    "sound",
    "ps2sd",
    "tst_dev",
    "smap",
    "mousedev",
    "joystick",
    "ps2pad",
};

#define lenof(x) (sizeof((x))/sizeof(*(x)))

/* This bit uses a gcc preprocessor extension. */
#define simplesystem(cmd...) do { \
    int pid = fork(); \
    int stat; \
    if (pid < 0) { write(1, "unable to fork!\r\n", 17); return 0; } \
    if (pid == 0) { execlp(cmd); return 127; } \
    if (pid > 0) { wait(&stat); } \
} while (0)

#define setcolour(c,r,g,b) do { \
    static SDL_Color colour = { r, g, b }; \
    SDL_SetColors(screen, &colour, c, 1); \
} while (0)

struct menuitem {
    void (*draw_text)(int x, int y);
    char *programname;
};

static void plot_point(void *ignored, int x, int y)
{
    scrdata[640*y+2*x] = scrdata[640*y+2*x+1] = 1;
}

static void draw_nort(int cx, int cy)
{
    char *text = "NORT";
    int i, x, y;

    cx -= 4*4;
    cy -= 4;

    for (i = 0; i < 4; i++) {
	for (x = 0; x < 8; x++)
	    for (y = 0; y < 8; y++)
		if (beebfont[(text[i]-32)*8+y] & (128 >> x)) {
		    plot_point(NULL, cx+x, cy+y);
		}
	cx += 8;
    }
}

static void draw_sumo(int cx, int cy)
{
    char *text = "SUMO";
    swash_text(cx - swash_width(text)/2, cy - 5, text,
	       plot_point, NULL);
}

static struct menuitem menu[] = {
    {draw_nort, "/bin/nort"},
    {draw_sumo, "/bin/sumo"},
};

static SDL_Joystick *joys[2];

int main(void)
{
    int i;
    SDL_Event event;
    int flags, redraw, mpos, val, prevval;

    /* Set up kernel modules. */
    for (i = 0; i < lenof(modules); i++) {
	printf("inserting module %s\n", modules[i]);
	simplesystem("/bin/insmod.static", "insmod", modules[i], 0);
	printf("done\n");
    }

    /*
     * Mount /proc, which we'll need for finding /dev/ps2gs once
     * the games are running.
     */
    mount("none", "/proc", "proc", MS_MGC_VAL, NULL);

    /*
     * Now we will start up a small SDL menu system which lets the
     * user choose a game. When they do so, we shut down SDL and
     * use simplesystem() to run the game, then start the menu back
     * up when control returns to us.
     */

    /*
     * The first few times I tried to run this menu system
     * standalone, it exited with some sort of error, and I didn't
     * see what the error was because all sorts of weird kernel
     * messages scrolled it off the screen extremely fast. So I
     * inserted the following fork code, so that the actual menu
     * would run in a child process while the original process just
     * sat in wait(), and then if the menu exited the parent would
     * see it and could report something about what happened.
     * 
     * Somewhat annoyingly, when I put the fork code in it suddenly
     * started working. I don't know if some magic happens at
     * kernel level if you're pid 1 which was confusing SDL, or
     * what; but I'm just going to leave the fork code here on the
     * grounds that (a) if it carries on working that's fine, and
     * (b) if it stops working then the fork code can serve its
     * original debugging purpose.
     */
    {
	int pid = fork();
	int stat;

	if (pid != 0) {
	    if (pid < 0) perror("unable to fork");
	    wait(&stat);
	    printf("child returned error code %d\n", stat);
	    printf("now entering tight loop\n");
	    while (1);
	}
    }

    while (1) {
	/* Set up the SDL game environment */
	setup();
	joys[0] = SDL_JoystickOpen(0);
	joys[1] = SDL_JoystickOpen(1);
	setcolour(0, 0, 0, 0);
	setcolour(1, 255, 255, 255);
	setcolour(2, 128, 0, 0);

	/*
	 * Get rid of the initial event queue from opening the
	 * joysticks (a bunch of button-up events).
	 */
	while (SDL_PollEvent(&event));

	/*
	 * Menu selection loop.
	 */
	flags = 0;
	redraw = 1;
	mpos = 0;
	prevval = 0;
	while (1) {

	    if (redraw) {
		scr_prep();
		for (i = 0; i < lenof(menu); i++) {
		    bar(100, 80 + i * 30, 220, 100 + i * 30, (i==mpos ? 2 : 0));
		    menu[i].draw_text(160, 90 + i * 30);
		}
		scr_done();
	    }

	    if (!SDL_WaitEvent(&event))
		exit(1);

	    switch(event.type) {
	      case SDL_JOYBUTTONDOWN:
		flags |= 1;
		break;
	      case SDL_JOYBUTTONUP:
		if (flags & 1)
		    flags |= 2;
		break;
	      case SDL_JOYAXISMOTION:
		if (event.jaxis.axis != 1) break;
		val = event.jaxis.value / JOY_THRESHOLD;
		if (val < 0 && prevval >= 0) {
		    mpos = (mpos + lenof(menu) - 1) % lenof(menu);
		    flags = 0;
		    redraw = 1;
		} else if (val > 0 && prevval <= 0) {
		    mpos = (mpos + 1) % lenof(menu);
		    flags = 0;
		    redraw = 1;
		}
		prevval = val;
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
		break;
	    }

	    if (flags == 3)
		break;
	}
	cleanup();
	simplesystem(menu[mpos].programname, menu[mpos].programname, NULL);
    }
}
