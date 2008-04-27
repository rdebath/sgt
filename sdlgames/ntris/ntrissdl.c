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

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define SQUARE_SIDE 8
#define HIGHLIGHT 2
#define PLAY_HEIGHT (SCR_HEIGHT / SQUARE_SIDE - 1)
#define BOTTOM_EDGE (PLAY_HEIGHT * SQUARE_SIDE)

static int no_quit_option = 0;

static void puttext(int x, int y, int c, char const *text)
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

static void centretext(int y, int c, char const *text)
{
    puttext(160-4*(strlen(text)), y, c, text);
}

static SDL_Joystick *joys[2];
static int maxheight;
static int left_edge, right_edge;

static void lineplotsimple(void *ctx, int x, int y)
{
    int colour = (int)ctx;
    plotc(x, y, colour);
}

static void drawline(int x1, int y1, int x2, int y2, int c)
{
    line(x1, y1, x2, y2, lineplotsimple, (void *)c);
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
static int *blockpixels[SQUARE_SIDE+1];

static void init_blockpixels(void)
{
    int ix, iy;
    int *p;

    int j, highlight;

    for (j = 1; j <= SQUARE_SIDE; j++) {
	highlight = HIGHLIGHT * j / SQUARE_SIDE;
	blockpixels[j] = malloc(j*j*sizeof(int));
	p = blockpixels[j];

	for (ix = 0; ix < j; ix++)
	    for (iy = 0; iy < j; iy++) {
		int left, right, top, bottom;
		left = (ix < highlight);
		right = (ix >= j - highlight);
		top = (iy < highlight);
		bottom = (iy >= j - highlight);
		if (left && top) {
		    *p = 0;
		} else if (right && bottom) {
		    *p = 0xC;
		} else if (left && bottom) {
		    int ty = j-1 - iy;
		    *p = (ix < ty ? 8 : ix > ty ? 0xA : 9);
		} else if (right && top) {
		    int tx = j-1 - ix;
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
}

static struct options {
    int shapeset;
    int width;
    int hold;
    int queue;
} opts;

void ntris_fe_block(struct frontend_instance *inst, int area,
		    int x, int y, int col, int type)
{
    int ix, iy;
    int colours[0xD];
    int light, medium, dark;
    int *p;
    int topx, topy, size;

    if (area == AREA_MAIN) {
	topx = left_edge;
	topy = 0;
	size = SQUARE_SIDE;
    } else if (area == AREA_HOLD) {
	topx = left_edge - 6*SQUARE_SIDE;
	topy = 0;
	size = SQUARE_SIDE;
    } else if (area >= AREA_NEXT) {
	if (area - AREA_NEXT >= opts.queue)
	    return;		       /* limit length of queue */
	topx = right_edge + SQUARE_SIDE;
	topy = 0;
	size = SQUARE_SIDE;
	while (area > AREA_NEXT && size > 0) {
	    area--;
	    topy += size * (maxheight+1);
	    size--;
	}
	if (size <= 0 || topy + maxheight*size > SCR_HEIGHT)
	    return;		       /* this next-display doesn't fit */
    }
    y = y * size + topy;
    x = x * size + topx;

    if (col == -1) {
	for (ix = 0; ix < size; ix++)
	    for (iy = 0; iy < size; iy++)
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

    p = blockpixels[size];
    for (ix = 0; ix < size; ix++)
	for (iy = 0; iy < size; iy++)
	    plotc(x+ix, y+iy, colours[*p++]);
}

enum {
    ACT_LEFT, ACT_FASTLEFT,
    ACT_RIGHT, ACT_FASTRIGHT,
    ACT_SOFTDROP, ACT_REFLECT, ACT_ANTICLOCK, ACT_CLOCKWISE,
    ACT_HOLD, ACT_HARDDROP, ACT_PAUSE
};

#define repeating_action(a) \
    ( (a)==ACT_FASTLEFT || (a)==ACT_FASTRIGHT || (a)==ACT_SOFTDROP)

static const int actions[4+10] = {
    /* left, right, up, down */
    ACT_LEFT, ACT_RIGHT, -1, ACT_SOFTDROP,
    /* square, x, triangle, circle */
    ACT_HARDDROP, ACT_ANTICLOCK, ACT_REFLECT, ACT_CLOCKWISE,
    /* L1, R1, L2, R2 */
    ACT_HOLD, ACT_HOLD, -1, -1,
    /* select, start */
    -1, ACT_PAUSE
};

static const struct { int key, action; } keys[] = {
    {SDLK_z, ACT_LEFT},
    {SDLK_a, ACT_FASTLEFT},
    {SDLK_x, ACT_RIGHT},
    {SDLK_s, ACT_FASTRIGHT},
    {SDLK_SLASH, ACT_SOFTDROP},
    {SDLK_RETURN, ACT_REFLECT},
    {SDLK_QUOTE, ACT_ANTICLOCK},
    {SDLK_HASH, ACT_CLOCKWISE},
    {SDLK_LSHIFT, ACT_HOLD},
    {SDLK_TAB, ACT_HOLD},
    {SDLK_SPACE, ACT_HARDDROP},
    {SDLK_p, ACT_PAUSE},
};

static void do_score(int y, char *title, int score, int evenifzero)
{
    char buf[40];
    char buf2[40];
    int i;

    if (score == 0 && !evenifzero)
	return;
    sprintf(buf, "%-7.7s%5u", title, score);

    i = strlen(buf);
    buf2[i] = '\0';
    while (i--) buf2[i] = '\377';

    puttext(10, y, 0, buf2);
    puttext(10, y, 255, buf);
}

static void do_scores(struct ntris_instance *ti)
{
    do_score(100, "Lines", ntris_get_score(ti, SCORE_LINES), 1);
    do_score(116, "Single", ntris_get_score(ti, SCORE_SINGLE), 0);
    do_score(126, "Double", ntris_get_score(ti, SCORE_DOUBLE), 0);
    do_score(136, "Triple", ntris_get_score(ti, SCORE_TRIPLE), 0);
    do_score(146, "Tetris", ntris_get_score(ti, SCORE_TETRIS), 0);
    do_score(156, "Pentris", ntris_get_score(ti, SCORE_PENTRIS), 0);
}

static void do_pause_indicator(int paused)
{
    if (paused)
	puttext(10, 172, 255, "PAUSED");
    else
	puttext(10, 172, 0, "\377\377\377\377\377\377");
}

static void play_game(void)
{
    struct ntris_instance *ti;
    int prevkeystate[SDLK_LAST], keystate[SDLK_LAST];
    int prevpressed[4+10], pressed[4+10];
    int dropinterval, untildrop;
    int dropinhibit;
    int paused = FALSE;
    int i, j;

    scr_prep();
    memset(scrdata, 0, SCR_WIDTH*SCR_HEIGHT*XMULT*YMULT);
    ti = ntris_init(NULL, opts.width, PLAY_HEIGHT, opts.shapeset);
    left_edge = ((SCR_WIDTH - SQUARE_SIDE * opts.width) / 2);
    if (left_edge < 110)
	left_edge = 110;
    right_edge = (left_edge + SQUARE_SIDE * opts.width);
    maxheight = ntris_shape_maxsize(ti);
    ntris_newshape(ti);
    line(left_edge-2, 0, left_edge-2, BOTTOM_EDGE+1,
	 lineplotsimple, (void *)255);
    line(left_edge-2, BOTTOM_EDGE+1, right_edge+1, BOTTOM_EDGE+1,
	 lineplotsimple, (void *)255);
    line(right_edge+1, BOTTOM_EDGE+1, right_edge+1, 0,
	 lineplotsimple, (void *)255);
    do_scores(ti);
    scr_done();

    dropinterval = 20;		       /* frames */
    untildrop = dropinterval;
    for (i = 0; i < lenof(prevpressed); i++)
	prevpressed[i] = 1;
    for (i = 0; i < lenof(keystate); i++)
	prevkeystate[i] = keystate[i] = 0;
    dropinhibit = 1;

    /*
     * Main game loop. We go round this once every frame.
     */
    while (1) {
	int left, right, anticlock, clockwise, reflect;
	int dropsoft, drophard, hold, pause;
	int dx, dy;
	SDL_Event event;

	/*
	 * Listen to waiting events to handle keyboard input.
	 */
	while (SDL_PollEvent(&event)) {
	    int action;

	    switch(event.type) {
	      case SDL_KEYDOWN:
		/* Standard catch-all quit-on-escape clause. */
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
		if (event.key.keysym.sym >= 0 &&
		    event.key.keysym.sym < lenof(keystate))
		    keystate[event.key.keysym.sym] = 1;
		break;
	      case SDL_KEYUP:
		if (event.key.keysym.sym >= 0 &&
		    event.key.keysym.sym < lenof(keystate))
		    keystate[event.key.keysym.sym] = 0;
		break;
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

	left = right = anticlock = clockwise = 0;
	reflect = dropsoft = drophard = hold = pause = 0;

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
		dropsoft = 1;
	    if (pressed[i] && actions[i] == ACT_HARDDROP)
		drophard = 1;
	    if (pressed[i] && actions[i] == ACT_HOLD)
		hold = 1;
	    if (pressed[i] && actions[i] == ACT_PAUSE)
		pause = 1;

	    for (j = 0; j < lenof(keys); j++) {
		if (keys[j].key < 0 || keys[j].key > lenof(keystate))
		    continue;
		if (!keystate[keys[j].key] ||
		    (prevkeystate[keys[j].key] && !repeating_action(keys[j].action)))
		    continue;
		if (keys[j].action == ACT_LEFT || keys[j].action == ACT_FASTLEFT)
		    left = 1;
		if (keys[j].action == ACT_RIGHT || keys[j].action == ACT_FASTRIGHT)
		    right = 1;
		if (keys[j].action == ACT_CLOCKWISE)
		    clockwise = 1;
		if (keys[j].action == ACT_ANTICLOCK)
		    anticlock = 1;
		if (keys[j].action == ACT_REFLECT)
		    reflect = 1;
		if (keys[j].action == ACT_SOFTDROP)
		    dropsoft = 1;
		if (keys[j].action == ACT_HARDDROP)
		    drophard = 1;
		if (keys[j].action == ACT_HOLD)
		    hold = 1;
		if (keys[j].action == ACT_PAUSE)
		    pause = 1;
	    }
	    memcpy(prevkeystate, keystate, sizeof(prevkeystate));
	}

	if (pause) {
	    paused = !paused;
	    do_pause_indicator(paused);
	}

	if (!paused) {

	    if (dropinhibit) {
		if (!dropsoft)
		    dropinhibit = 0;
		dropsoft = 0;
	    }

	    /*
	     * Check the drop timer.
	     */
	    if (--untildrop < 0)
		dropsoft = 1;

	    /*
	     * Now do the various actions.
	     */
	    scr_prep();
	    if (left)
		ntris_try_left(ti);
	    if (right)
		ntris_try_right(ti);
	    if (clockwise)
		ntris_try_clockwise(ti);
	    if (anticlock)
		ntris_try_anticlock(ti);
	    if (reflect)
		ntris_try_reflect(ti);
	    if (hold && opts.hold)
		ntris_try_hold(ti);
	    if (dropsoft || drophard) {
		int ret;
		if (drophard) {
		    ret = 1;
		    ntris_harddrop(ti);
		} else {
		    ret = ntris_softdrop(ti);
		}
		if (ret) {
		    ret = ntris_newshape(ti);
		    if (!ret)
			break;
		    do_scores(ti);

		    /* FIXME: potentially level up and thus change dropinterval */
		    dropinhibit = 1;
		}
		untildrop = dropinterval;
	    }
	}

	scr_done();

	vsync();
    }

    puttext(0, 0, 255, "GAME OVER");

    /*
     * Now the game is over. Restore the colour palette (in case
     * one or more players were cloaked at the time of the game
     * ending) and then just wait for a Start button or Space press
     * (either player) before continuing.
     */
    {
	int done = 0;
	SDL_Event event;

	while (!done && SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYBUTTONDOWN:
		if (event.jbutton.button == 9)
		    done = 1;
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
		if (event.key.keysym.sym == SDLK_SPACE)
		    done = 1;
	    }
	}
    }

    /* FIXME: free ti */
}

enum { MM_QUIT, MM_GAME, MM_SETUP };

static int main_menu(void)
{
    const struct {
	int y;
	char const *text;
	int action;
    } menu[] = {
	{124, "Play Game", MM_GAME},
	{138, "Game Setup", MM_SETUP},
#ifdef PS2_FIXME_SCR_ADJUST_UNSUPPORTED_SO_FAR
	{152, "Adjust Screen", MM_SCR_ADJUST},
	{166, "Exit Ntris", MM_QUIT},
#else /* PS2 */
	{152, "Exit Ntris", MM_QUIT},
#endif /* PS2 */
    };

    int menumin, j, flags = 0, prevval = 0, redraw;
    int menulen = (no_quit_option ? lenof(menu)-1 : lenof(menu));
    int x, y;
    int action;
    int index;
    SDL_Event event;

    /*
     * Clear the screen and display the main menu.
     */

    scr_prep();

    memset(scrdata, 0, SCR_WIDTH*SCR_HEIGHT*XMULT*YMULT);

    menumin = 0;

    centretext(50, 255, "Ntris");
    centretext(66, 255, "by Simon Tatham");

    for (j = menumin; j < menulen; j++)
	centretext(menu[j].y, 255, menu[j].text);

    scr_done();

    index = menumin;
    redraw = 1;

    /*
     * Push an initial event to ensure a redraw happens
     * immediately.
     */
    event.type = SDL_USEREVENT;
    action = MM_QUIT;
    SDL_PushEvent(&event);

    while (flags != 3 && SDL_WaitEvent(&event)) {

	switch(event.type) {
	  case SDL_JOYBUTTONDOWN:
	    flags |= 1;
	    break;
	  case SDL_JOYBUTTONUP:
	    {
		int which = event.jbutton.which;
		if (which != 0) break;
		if (!(flags & 1)) break;
		flags |= 2;
		action = menu[index].action;
	    }
	    break;
	  case SDL_JOYAXISMOTION:
	    if (event.jaxis.axis == 1) {
		int val = event.jaxis.value / JOY_THRESHOLD;
		int which = event.jaxis.which;
		if (which != 0) break;
		if (val < 0 && prevval >= 0) {
		    if (--index < menumin)
			index = menulen-1;
		    redraw = 1;
		} else if (val > 0 && prevval <= 0) {
		    if (++index >= menulen)
			index = menumin;
		    redraw = 1;
		}
		prevval = val;
	    }
	    break;
	  case SDL_KEYDOWN:
	    if (event.key.keysym.sym == SDLK_ESCAPE)
		exit(1);
	    if (event.key.keysym.sym == SDLK_UP) {
		if (--index < menumin)
		    index = menulen-1;
		redraw = 1;
	    }
	    if (event.key.keysym.sym == SDLK_DOWN) {
		if (++index >= menulen)
		    index = menumin;
		redraw = 1;
	    }
	    if (event.key.keysym.sym == SDLK_RETURN) {
		flags = 3;
		action = menu[index].action;
	    }
	    break;
	}

	if (redraw) {
	    scr_prep();
	    for (j = menumin; j < menulen; j++) {
		int c = 0;
		if (j == index) c = 255;
		drawline(159-56, menu[j].y-3, 159-52, menu[j].y-3, c);
		drawline(159-56, menu[j].y-3, 159-56, menu[j].y+1, c);
		drawline(159-56, menu[j].y+9, 159-52, menu[j].y+9, c);
		drawline(159-56, menu[j].y+9, 159-56, menu[j].y+5, c);
		drawline(159+56, menu[j].y-3, 159+52, menu[j].y-3, c);
		drawline(159+56, menu[j].y-3, 159+56, menu[j].y+1, c);
		drawline(159+56, menu[j].y+9, 159+52, menu[j].y+9, c);
		drawline(159+56, menu[j].y+9, 159+56, menu[j].y+5, c);
	    }
	    scr_done();
	    redraw = 0;
	}

    }

    return action;
}

static void setup_game(int joy)
{
    struct {
	/* Coordinates of item for each player. */
	int x, y;
	/* Width of item (for showing selection box). */
	int w;
	/* Where to move (relative position in array indices) if the user
	 * moves the controller. */
	int up, down, left, right;
    } items[] = {
	{160, 40, 14, 1, -1, 0, 0},
	{160, 50, 15, 1, -1, 0, 0},
	{160, 60, 7, 1, -1, 0, 0},
	{160, 74, 7, 1, -1, 0, 0},
	{160, 86, 1, 2, -1, 0, +1},
	{200, 86, 1, 1, -2, -1, 0},
	{160, 98, 8, 1, -2, 0, 0},
	{160, 110, 1, 2, -1, 0, +1},
	{200, 110, 1, 1, -2, -1, 0},
	{104, 224, 14, 1, -2, 0, 0},
    };
    SDL_Event event;
    char speedstr[20];

    int i, p;
    int index;
    int prevjoy[2];
    int game_setup_done = 0;

    index = 0;
    for (i = 0; i < 2; i++)
	prevjoy[i] = 0;

    while (!game_setup_done) {
	/*
	 * Every time we redisplay, we'll redraw the whole menu. It
	 * seems simpler to do it that way.
	 */
	scr_prep();
	memset(scrdata, 0, SCR_WIDTH*SCR_HEIGHT*XMULT*YMULT);

	/*
	 * Draw selection crosshairs.
	 */
	{
	    int x = items[index].x, y = items[index].y, w = items[index].w*8;

	    drawline(x-8, y-2, x-4, y-2, 255);
	    drawline(x-8, y-2, x-8, y+2, 255);
	    drawline(x-8, y+8, x-4, y+8, 255);
	    drawline(x-8, y+8, x-8, y+4, 255);
	    drawline(x+w+8, y-2, x+w+4, y-2, 255);
	    drawline(x+w+8, y-2, x+w+8, y+2, 255);
	    drawline(x+w+8, y+8, x+w+4, y+8, 255);
	    drawline(x+w+8, y+8, x+w+8, y+4, 255);
	}

	puttext(96, 20, 255, "Ntris Game Setup");
	drawline(96, 29, 223, 29, 255);

	puttext(30, 40, 255, "Presets:");
	puttext(160, 40, 255, "Tetris Classic");
	puttext(160, 50, 255, "Tetris Enhanced");
	puttext(160, 60, 255, "Pentris");

	puttext(30, 74, 255, "Shape set:");
	puttext(160, 74, 255,
		opts.shapeset == TETRIS_SHAPES ? "Tetris" :
		opts.shapeset == PENTRIS_SHAPES ? "Pentris" :
		"INTERNAL ERROR");

	puttext(30, 86, 255, "Arena width:");
	puttext(160, 86, 255, "-");
	{
	    char buf[20];
	    sprintf(buf, "%2d", opts.width);
	    puttext(176, 86, 255, buf);
	}
	puttext(200, 86, 255, "+");

	puttext(30, 98, 255, "Hold function:");
	puttext(160, 98, 255, opts.hold ? "Enabled" : "Disabled");

	puttext(30, 110, 255, "Queue length:");
	puttext(160, 110, 255, "-");
	{
	    char buf[20];
	    sprintf(buf, "%2d", opts.queue);
	    puttext(176, 110, 255, buf);
	}
	puttext(200, 110, 255, "+");

	puttext(104, 224, 255, "Finished Setup");

	scr_done();

	while (SDL_WaitEvent(&event)) {
	    int move, axis, val;
	    int press, button;

	    move = press = 0;

	    switch(event.type) {
	      case SDL_JOYAXISMOTION:
		if (event.jaxis.which == joy) {
		    axis = event.jaxis.axis;
		    val = event.jaxis.value / JOY_THRESHOLD;
		    if (val != 0 && sign(val) != sign(prevjoy[axis]))
			move = 1;
		    prevjoy[axis] = val;
		}
		break;
	      case SDL_JOYBUTTONDOWN:
		if (event.jbutton.which == joy) {
		    button = event.jbutton.button;
		    press = 1;
		}
		break;
	      case SDL_KEYDOWN:
		switch (event.key.keysym.sym) {
		  case SDLK_ESCAPE:
		    exit(1);
		    break;
		  case SDLK_UP:    axis = 1; val = -1; move = 1; break;
		  case SDLK_DOWN:  axis = 1; val = +1; move = 1; break;
		  case SDLK_LEFT:  axis = 0; val = -1; move = 1; break;
		  case SDLK_RIGHT: axis = 0; val = +1; move = 1; break;
		  case SDLK_RETURN: button = -1; press = 1; break;
		}
		break;
	    }

	    /*
	     * Process menu movement.
	     */
	    if (move) {
		int displace = 0;
		if (val < 0) {
		    displace = (axis ?
				items[index].down :
				items[index].left);
		} else if (val > 0) {
		    displace = (axis ?
				items[index].up :
				items[index].right);
		}
		index = (index + displace + lenof(items)) % lenof(items);
		if (displace) break;
	    }

	    /*
	     * Process menu button presses.
	     */
	    if (press) {
		switch (index) {
		  case 0:	       /* Tetris Classic preset */
		    opts.shapeset = TETRIS_SHAPES;
		    opts.queue = 1;
		    opts.hold = 0;
		    opts.width = 10;
		    break;
		  case 1:	       /* Tetris Enhanced preset */
		    opts.shapeset = TETRIS_SHAPES;
		    opts.queue = 8;
		    opts.hold = 1;
		    opts.width = 10;
		    break;
		  case 2:	       /* Pentris preset */
		    opts.shapeset = PENTRIS_SHAPES;
		    opts.queue = 8;
		    opts.hold = 1;
		    opts.width = 10;
		    break;
		  case 3:
		    opts.shapeset = (opts.shapeset + 1) % NSHAPESETS;
		    break;
		  case 4:
		    if (opts.width > 5)
			opts.width--;
		    break;
		  case 5:
		    if (opts.width < 20)
			opts.width++;
		    break;
		  case 6:
		    opts.hold = !opts.hold;
		    break;
		  case 7:
		    if (opts.queue > 0)
			opts.queue--;
		    break;
		  case 8:
		    if (opts.queue < 8)
			opts.queue++;
		    break;
		  case 9:
		    game_setup_done = 1;
		    break;
		}
		break;		       /* cause a screen redraw */
	    }
	}
    }
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

    /*
     * Option defaults.
     */
    opts.shapeset = TETRIS_SHAPES;
    opts.width = 10;
    opts.hold = 1;
    opts.queue = 8;

    while (1) {
	switch (main_menu()) {
	  case MM_QUIT:
	    return 0;		       /* finished */
	  case MM_SETUP:
	    setup_game(0);
	    break;
	  case MM_GAME:
	    play_game();
	    break;
	}
    }
}
