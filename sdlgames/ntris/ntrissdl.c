/*
 * ntrissdl.c - SDL front end to the Ntris game engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdlstuff.h"
#include "beebfont.h"
#include "utils.h"

#include "ntris.h"

#define JOY_THRESHOLD 4096

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

#define plot(x,y,c) ( scrdata[(y)*640+(x)*2] = scrdata[(y)*640+(x)*2+1] = c )
#define plotc(x,y,c) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : plot(x,y,c) )

#define SCR_WIDTH 320
#define SCR_HEIGHT 240
#define SQUARE_SIDE 8
#define HIGHLIGHT 2
#define PLAY_HEIGHT (SCR_HEIGHT / SQUARE_SIDE - 1)
#define PLAY_WIDTH 10
#define LEFT_EDGE ((SCR_WIDTH - SQUARE_SIDE * PLAY_WIDTH) / 2)
#define RIGHT_EDGE (LEFT_EDGE + SQUARE_SIDE * PLAY_WIDTH)
#define BOTTOM_EDGE (PLAY_HEIGHT * SQUARE_SIDE)

static int no_quit_option = 0;

static int puttext(int x, int y, int c, char const *text)
{
    int i, ix, iy;
    int ch;

    for (i = 0; text[i]; i++) {
	ch = text[i];
	if (ch < 32 || ch > 127) ch = 127;
	ch = (ch-32) * 8;	       /* index into beebfont[] array */
	for (ix = 0; ix < 8; ix++)
	    for (iy = 0; iy < 8; iy++)
		if (beebfont[ch+iy] & (0x80>>ix))
		    plotc(x+ix, y+iy, c);
	x += 8;
    }
}

static int centretext(int y, int c, char const *text)
{
    puttext(160-4*(strlen(text)), y, c, text);
}

static SDL_Joystick *joys[2];

static void lineplotsimple(void *ctx, int x, int y)
{
    int colour = (int)ctx;
    plotc(x, y, colour);
}

/*
 * `blockpixels' holds the diagram of a fully highlighted block, in
 * accordance with the description in ntris.h. The indices in this
 * array are
 * 
 * 0001111223
 * 0001111234
 * 0001111344
 * 5556666777
 * 5556666777
 * 5556666777
 * 5556666777
 * 889BBBBCCC
 * 89ABBBBCCC
 * 9AABBBBCCC
 * 
 * (A=10, B=11, C=12, D=13). The `block' function computes which
 * actual colours need to be allocated to each of these indices,
 * and then plots straight from this array.
 */
static int blockpixels[SQUARE_SIDE*SQUARE_SIDE];

void init_blockpixels(void)
{
    int ix, iy;
    int *p = blockpixels;

    for (ix = 0; ix < SQUARE_SIDE; ix++)
	for (iy = 0; iy < SQUARE_SIDE; iy++) {
	    int left, right, top, bottom;
	    left = (ix < HIGHLIGHT);
	    right = (ix >= SQUARE_SIDE - HIGHLIGHT);
	    top = (iy < HIGHLIGHT);
	    bottom = (iy >= SQUARE_SIDE - HIGHLIGHT);
	    if (left && top) {
		*p = 0;
	    } else if (right && bottom) {
		*p = 0xC;
	    } else if (left && bottom) {
		int ty = SQUARE_SIDE-1 - iy;
		*p = (ix < ty ? 8 : ix > ty ? 0xA : 9);
	    } else if (right && top) {
		int tx = SQUARE_SIDE-1 - ix;
		*p = (tx < iy ? 4 : tx > iy ? 2 : 3);
	    } else if (left) {
		*p = 5;
	    } else if (bottom) {
		*p = 0xB;
	    } else if (right) {
		*p = 7;
	    } else if (top) {
		*p = 1;
	    } else {
		*p = 6;
	    }
	    p++;
	}
}

void block(struct frontend_instance *inst, int x, int y, int col, int type)
{
    int ix, iy;
    int colours[0xD];
    int light, medium, dark;
    int *p;

    x--;			       /* playing area starts at 1 */
    y *= SQUARE_SIDE;
    x *= SQUARE_SIDE;
    x += LEFT_EDGE;

    if (col == -1) {
	for (ix = 0; ix < SQUARE_SIDE; ix++)
	    for (iy = 0; iy < SQUARE_SIDE; iy++)
		plotc(x+ix, y+iy, 0);
	return;
    }

    medium = 1 + 3*col;
    light = medium+2;
    dark = medium+1;

    /*
     * Compute highlight colours.
     */
    colours[0] = ((type & (FLAG_LEDGE|FLAG_TEDGE|FLAG_TLCORNER)) ?
		  light : medium);
    colours[0xC] = ((type & (FLAG_REDGE|FLAG_BEDGE|FLAG_BRCORNER)) ?
		    dark : medium);
    colours[5] = ((type & FLAG_LEDGE) ? light : medium);
    colours[1] = ((type & FLAG_TEDGE) ? light : medium);
    colours[7] = ((type & FLAG_REDGE) ? dark : medium);
    colours[0xB] = ((type & FLAG_BEDGE) ? dark : medium);
    colours[2] = ((type & FLAG_TEDGE) ? light :
		  (type & (FLAG_REDGE|FLAG_TRCORNER)) ? dark : medium);
    colours[4] = ((type & FLAG_REDGE) ? dark :
		  (type & (FLAG_TEDGE|FLAG_TRCORNER)) ? light : medium);
    colours[8] = ((type & FLAG_LEDGE) ? light :
		  (type & (FLAG_BEDGE|FLAG_BLCORNER)) ? dark : medium);
    colours[0xA] = ((type & FLAG_BEDGE) ? dark :
		  (type & (FLAG_LEDGE|FLAG_BLCORNER)) ? light : medium);
    colours[3] = (colours[2] == colours[4] ? colours[2] : medium);
    colours[9] = (colours[8] == colours[0xA] ? colours[8] : medium);
    colours[6] = medium;

    p = blockpixels;
    for (ix = 0; ix < SQUARE_SIDE; ix++)
	for (iy = 0; iy < SQUARE_SIDE; iy++)
	    plotc(x+ix, y+iy, colours[*p++]);
}

enum {
    ACT_LEFT, ACT_FASTLEFT,
    ACT_RIGHT, ACT_FASTRIGHT,
    ACT_SOFTDROP, ACT_REFLECT, ACT_ANTICLOCK, ACT_CLOCKWISE,
    ACT_HOLD, ACT_HARDDROP
};

#define repeating_action(a) \
    ( (a)==ACT_FASTLEFT || (a)==ACT_FASTRIGHT || (a)==ACT_SOFTDROP)

static const int actions[4+10] = {
    ACT_LEFT, ACT_RIGHT, -1, ACT_SOFTDROP,
    ACT_REFLECT, ACT_ANTICLOCK, -1, ACT_CLOCKWISE,
    -1, -1, -1, -1, -1, -1
};

static void play_game(void)
{
    struct ntris_instance *ti;
    int prevpressed[4+10], pressed[4+10];
    int dropinterval, untildrop;
    int dropinhibit;
    int i;

    scr_prep();
    ti = init_game(NULL, PLAY_WIDTH, PLAY_HEIGHT, NULL);
    init_shape(ti);
    line(LEFT_EDGE-2, 0, LEFT_EDGE-2, BOTTOM_EDGE+1,
	 lineplotsimple, (void *)255);
    line(LEFT_EDGE-2, BOTTOM_EDGE+1, RIGHT_EDGE+1, BOTTOM_EDGE+1,
	 lineplotsimple, (void *)255);
    line(RIGHT_EDGE+1, BOTTOM_EDGE+1, RIGHT_EDGE+1, 0,
	 lineplotsimple, (void *)255);
    scr_done();

    dropinterval = 20;		       /* frames */
    untildrop = dropinterval;
    for (i = 0; i < lenof(prevpressed); i++)
	prevpressed[i] = 1;
    dropinhibit = 1;

    /*
     * Main game loop. We go round this once every frame.
     */
    while (1) {
	int left, right, anticlock, clockwise, reflect, dropping;
	int dx, dy;
	SDL_Event event;

	/*
	 * Listen to waiting events in case ESC was pressed.
	 */
	while (SDL_PollEvent(&event)) {
	    int action;

	    switch(event.type) {
	      case SDL_KEYDOWN:
		/* Standard catch-all quit-on-escape clause. */
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
	    }
	}

	/*
	 * Check the controller.
	 */
	for (i = 0; i < lenof(pressed); i++)
	    pressed[i] = 0;

	dx = sign(SDL_JoystickGetAxis(joys[0], 0) / JOY_THRESHOLD);
	if (dx < 0)
	    pressed[0] = 1;
	else if (dx > 0)
	    pressed[1] = 1;
	dy = sign(SDL_JoystickGetAxis(joys[0], 1) / JOY_THRESHOLD);
	if (dy < 0)
	    pressed[2] = 1;
	else if (dy > 0)
	    pressed[3] = 1;

	for (i = 0; i < 10; i++)
	    if (SDL_JoystickGetButton(joys[0], i))
		pressed[4+i] = 1;

	left = right = anticlock = clockwise = reflect = dropping = 0;

	for (i = 0; i < lenof(pressed); i++) {
	    int tmp = pressed[i];
	    if (!repeating_action(actions[i]) && prevpressed[i])
		pressed[i] = 0;
	    prevpressed[i] = tmp;

	    if (pressed[i] &&
		(actions[i]==ACT_LEFT || actions[i]==ACT_FASTLEFT))
		left = 1;
	    if (pressed[i] &&
		(actions[i]==ACT_RIGHT || actions[i]==ACT_FASTRIGHT))
		right = 1;
	    if (pressed[i] && actions[i] == ACT_CLOCKWISE)
		clockwise = 1;
	    if (pressed[i] && actions[i] == ACT_ANTICLOCK)
		anticlock = 1;
	    if (pressed[i] && actions[i] == ACT_REFLECT)
		reflect = 1;
	    if (pressed[i] && actions[i] == ACT_SOFTDROP)
		dropping = 1;
	}

	if (dropinhibit) {
	    if (!dropping)
		dropinhibit = 0;
	    dropping = 0;
	}

	/*
	 * Check the drop timer.
	 */
	if (--untildrop < 0)
	    dropping = 1;

	/*
	 * Now do the various actions.
	 */
	scr_prep();
	if (left)
	    try_move_left(ti);
	if (right)
	    try_move_right(ti);
	if (clockwise)
	    try_clockwise(ti);
	if (anticlock)
	    try_anticlock(ti);
	if (reflect)
	    try_reflect(ti);
	if (dropping) {
	    int ret = drop(ti);
	    if (ret) {
		ret = init_shape(ti);
		if (!ret)
		    break;
		/* FIXME: count score, level up, change dropinterval */
		dropinhibit = 1;
	    }
	    untildrop = dropinterval;
	}
	scr_done();

	vsync();
    }

    puttext(0, 0, 255, "GAME OVER");

    /*
     * FIXME: this just waits for Escape. We should do something
     * more sensible here.
     */
    {
	int flags = 0;
	SDL_Event event;

	while (flags != 15 && SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
	    }
	}
    }

    /* FIXME: free ti */
}

int main(int argc, char **argv)
{
    int p;

    if (argc > 1 && !strcmp(argv[1], "-n"))
	no_quit_option = 1;

    /* Set up the SDL game environment */
    setup();

    joys[0] = SDL_JoystickOpen(0);
    /* joys[1] = SDL_JoystickOpen(1);  (only one needed for *tris!) */

    /*
     * Set up the colour palette.
     */
#define setcolour(c,R,G,B) do { \
    SDL_Color colour; \
    colour.r = R; colour.g = G; colour.b = B; \
    SDL_SetColors(screen, &colour, c, 1); \
} while (0)
    {
	const static int colours[] = {
	    0xcc0000, 0x880000, 0xff0000,
	    0xcc6600, 0x884400, 0xff7f00,
	    0xcc9900, 0x886600, 0xffbf00,
	    0xcccc00, 0x888800, 0xffff00,
	    0x00cc00, 0x008800, 0x00ff00,
	    0x008400, 0x005800, 0x00b000,
	    0x008484, 0x005858, 0x00b0b0,
	    0x00cccc, 0x008888, 0x00ffff,
	    0x0066cc, 0x004488, 0x007fff,
	    0x9900cc, 0x660088, 0xbf00ff,
	    0xcc00cc, 0x880088, 0xff00ff,
	    0xcc9999, 0x886666, 0xffbfbf,
	    0xcccc99, 0x888866, 0xffffbf,
	    0x99cc99, 0x668866, 0xbfffbf,
	    0x9999cc, 0x666688, 0xbfbfff,
	    0x757575, 0x4e4e4e, 0x9c9c9c,
	    0x999999, 0x666666, 0xbfbfbf,
	    0xcccccc, 0x888888, 0xffffff,
	};
	int i;
	for (i = 0; i < lenof(colours); i++)
	    setcolour(1+i, (colours[i]>>16)&0xFF,
		      (colours[i]>>8)&0xFF, colours[i]&0xFF);
	setcolour(255, 255, 255, 255);
    }

    /*
     * Extra colours for the game setup screen: the colours
     * for the PS2 button symbols, in order of the way the
     * controller numbers them (sq,x,tr,ci).
     */
    setcolour(128, 255, 128, 170);     /* square is purpley pink */
    setcolour(129, 170, 255, 255);     /* x is pale cyan */
    setcolour(130, 0, 255, 128);       /* triangle is slightly bluey green */
    setcolour(131, 255, 128, 128);     /* circle is pinky red */

#undef setcolour

    /*
     * Get rid of the initial event queue from opening the
     * joysticks (a bunch of button-up events).
     */
    {
	SDL_Event event;
	while (SDL_PollEvent(&event));
    }

    /*
     * Set up the `blockpixels' array, used to compute the edge
     * highlights on the pieces.
     */
    init_blockpixels();

    play_game();

    return 0;
}
