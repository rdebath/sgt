/*
 * nort - a game descended from the Tron light cycles game, but
 * modified almost out of all recognition
 */

#include <stdio.h>
#include <stdlib.h>

#include "sdlstuff.h"
#include "beebfont.h"

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

#define plot(x,y,c) ( scrdata[(y)*640+(x)*2] = scrdata[(y)*640+(x)*2+1] = c )
#define pixel(x,y) ( scrdata[(y)*640+(x)*2] )

int puttext(int x, int y, int c, char const *text)
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
		    plot(x+ix, y+iy, c);
	x += 8;
    }
}

int drawline(int x1, int y1, int x2, int y2, int c)
{
    int dx, dy, lx, ly, sx, sy, dl, ds, d, i;

    /*
     * Simple Bresenham's line drawing should be adequate here.
     * First sort out what octant we're in.
     */
    dx = x2 - x1; dy = y2 - y1;

    if (dx < 0 || (dx == 0 && dy < 0)) {
	int tmp;
	/*
	 * Canonify the order of the endpoints, so that drawing a
	 * line from point A to point B and then drawing in a
	 * different colour from B back to A can't ever leave any
	 * pixels of the first colour.
	 */
	tmp = x1; x1 = x2; x2 = tmp; dx = -dx;
	tmp = y1; y1 = y2; y2 = tmp; dy = -dy;
    }

    if (abs(dx) > abs(dy)) {
	lx = sign(dx); ly = 0;	       /* independent variable on x axis */
	sx = 0; sy = sign(dy);	       /* dependent variable on y axis */
	dl = abs(dx); ds = abs(dy);
    } else {
	lx = 0; ly = sign(dy);	       /* independent variable on y axis */
	sx = sign(dx); sy = 0;	       /* dependent variable on x axis */
	dl = abs(dy); ds = abs(dx);
    }

    /*
     * Now we've normalised the octants, draw the line. Decision
     * variable starts at dl, decrements by 2*ds, and we add on
     * 2*dl and move up one pixel when it's gone past zero.
     */
    d = dl;
    plot(x1, y1, c);
    for (i = 0; i < dl; i++) {
	d -= 2*ds;
	if (d < 0) {
	    d += 2*dl;
	    x1 += sx; y1 += sy;
	}
	x1 += lx; y1 += ly;
	plot(x1, y1, c);
    }
}

/*
 * Note that this fill-circle routine does a _logical OR_ plot
 * rather than a simple pixel-assignment plot.
 */
int fillcircle(int x, int y, int r, int c)
{
    int dx, dy, d, d2, i;

    /*
     * Bresenham's again.
     */

    dy = r;
    dx = 0;
    d = 0;
    while (dy >= dx) {

	/* Plot at (dy,dx) in all eight octants. */
#define doplot(Dy,Dx) ( plot(x+(Dx),y+(Dy),c|pixel(x+(Dx),y+(Dy))) )
	for (i = 0; i < dy; i++) {
	    doplot(+i,+dx); doplot(+i,-dx);
	    doplot(-i,+dx); doplot(-i,-dx);
	    doplot(+dx,+i); doplot(-dx,+i);
	    doplot(+dx,-i); doplot(-dx,-i);
	}
#undef doplot

	d += 2*dx + 1;
	d2 = d - (2*dy - 1);
	if (abs(d2) < abs(d)) {
	    d = d2;
	    dy--;
	}
	dx++;
    }
}

/*
 * Actions for controller buttons to perform.
 */
enum {
    /* weapons */
    ACT_RIFLE, ACT_BOMB, ACT_MGUN, ACT_CAGE, ACT_BOLAS, ACT_FLIB_KRONT,
    /* functions */
    ACT_TELEPORT, ACT_CLOAK, ACT_EXTEND,
    /* speed */
    ACT_ACCEL, ACT_DECEL
};

SDL_Joystick *joys[2];

int play_game(void)
{
    int x, y, c;
    int i, p, gameover;
    char const startstring[] = "BOTH PLAYERS PRESS X TO START";

    struct player {
	int buttons[8];		       /* sq,x,tr,ci,l1,r1,l2,r2 */
	int colour;
	int x, y, dx, dy, fire_dx, fire_dy;
	int steering_delay, button_delay[8];
	int dead;
    };

    struct player players[2];

    /*
     * Initially, clear the screen, draw a white border round it,
     * and put up the `press x to start' message.
     */

    scr_prep();
    for (x = 0; x < 320; x++)
	for (y = 0; y < 240; y++) {
	    if (x == 0 || x == 319 || y == 0 || y == 239)
		plot(x, y, 3);
	    else
		plot(x, y, 0);
	}
    puttext(160 - 4*(sizeof(startstring)-1), 100, 3, startstring);

    scr_done();

    /*
     * Now wait until we see the X button (joystick button 1) both
     * pressed and released on each controller.
     */
    {
	int flags = 0;
	SDL_Event event;

	while (flags != 15 && SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYBUTTONDOWN:
		if (event.jbutton.button == 1) {
		    if (event.jbutton.which == 0)
			flags |= 1;
		    else if (event.jbutton.which == 1)
			flags |= 2;
		}
		break;
	      case SDL_JOYBUTTONUP:
		if (event.jbutton.button == 1) {
		    if (event.jbutton.which == 0)
			flags |= 4;
		    else if (event.jbutton.which == 1)
			flags |= 8;
		}
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
	    }
	}
    }

    scr_prep();
    for (x = 0; x < sizeof(startstring)-1; x++)
	puttext(160 - 4*(sizeof(startstring)-1) + x*8, 100, 0, "\x7F");
    scr_done();

    /*
     * OK, we're ready to play the actual game. Set up the player
     * structures.
     */
    for (p = 0; p < 2; p++) {
	players[p].y = 120;
	players[p].x = (p == 0 ? 1 : 319);
	players[p].dy = 0;
	players[p].dx = (p == 0 ? +1 : -1);
	players[p].colour = p+1;
	players[p].buttons[0] = players[p].buttons[3] = ACT_DECEL;
	players[p].buttons[1] = players[p].buttons[2] = ACT_ACCEL;
	players[p].buttons[4] = players[p].buttons[6] = ACT_RIFLE;
	players[p].buttons[5] = players[p].buttons[7] = ACT_TELEPORT;
	players[p].steering_delay = 0;
	for (i = 0; i < 8; i++)
	    players[p].button_delay[i] = 0;
	players[p].dead = 0;
    }

    /*
     * And begin the main game loop. Ooh!
     */
    gameover = 0;
    while (!gameover) {
	SDL_Event event;

	scr_prep();
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
	 * Respond to controller activity.
	 */
	for (p = 0; p < 2; p++) {
	    int extended;

	    /*
	     * Check directional motion. If the player is moving
	     * horizontally, only check the vertical axis of
	     * motion, and vice versa. Also we delay by
	     * `steering_delay' to prevent ludicrously quick
	     * direction changes.
	     * 
	     * However, we do _read_ both axes unconditionally so
	     * we can determine the firing direction if the player
	     * activates any directional weapons or functions.
	     */
	    players[p].fire_dx = SDL_JoystickGetAxis(joys[p], 0);
	    players[p].fire_dx = sign(players[p].fire_dx/4096);
	    players[p].fire_dy = SDL_JoystickGetAxis(joys[p], 1);
	    players[p].fire_dy = sign(players[p].fire_dy/4096);

	    if (players[p].steering_delay) {
		players[p].steering_delay--;
	    } else if (players[p].dx == 0) {
		if (players[p].fire_dx) {
		    players[p].dx = players[p].fire_dx;
		    players[p].dy = 0;
		    players[p].steering_delay = 3;
		}
	    } else {
		if (players[p].fire_dy) {
		    players[p].dy = players[p].fire_dy;
		    players[p].dx = 0;
		    players[p].steering_delay = 3;
		}
	    }

	    /*
	     * Now normalise firing direction to movement direction
	     * if it's completely unset.
	     */
	    if (players[p].fire_dx == 0 && players[p].fire_dy == 0) {
		players[p].fire_dx = sign(players[p].dx);
		players[p].fire_dy = sign(players[p].dy);
	    }

	    /*
	     * Check fire buttons, and for each button perform the
	     * necessary action. Note the exceptional code for
	     * ACT_EXTEND, which acts as a shift key so we need to
	     * know if it's pressed _before_ doing anything else.
	     */
	    extended = 0;
	    for (i = 0; i < 8; i++)
		if (players[p].buttons[i] == ACT_EXTEND &&
		    SDL_JoystickGetButton(joys[p], i))
		    extended = 1;

#define pcoord(xx,yy,n) ( (xx) = players[p].x + players[p].fire_dx * n, \
			  (yy) = players[p].y + players[p].fire_dy * n )

	    for (i = 0; i < 8; i++) {
		if (players[p].button_delay[i]) {
		    players[p].button_delay[i]--;
		} else if (SDL_JoystickGetButton(joys[p], i)) {
		    switch(players[p].buttons[i]) {
		      case ACT_ACCEL:
			{
			    int speed;
			    speed = abs(players[p].dx) + abs(players[p].dy);
			    if (speed < 4) speed++;
			    players[p].dx = sign(players[p].dx) * speed;
			    players[p].dy = sign(players[p].dy) * speed;
			}
			break;
		      case ACT_DECEL:
			players[p].dx = sign(players[p].dx);
			players[p].dy = sign(players[p].dy);
			break;
		      case ACT_RIFLE:
			{
			    int bx, by, ex, ey;
			    pcoord(bx, by, 20);
			    pcoord(ex, ey, 95);
			    drawline(bx, by, ex, ey, 3);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_BOMB:
			{
			    int x, y;
			    pcoord(x, y, 70);
			    fillcircle(x, y, 10, 3);
			    players[p].button_delay[i] = 3;
			}
			break;
		    }
		}
	    }
	}

	/*
	 * Move the players.
	 */
	for (i = 0;
	     i < abs(players[0].dx) || i < abs(players[0].dy) ||
	     i < abs(players[1].dx) || i < abs(players[1].dy); i++) {
	    /*
	     * Move each player on by one pixel and mark a
	     * preliminary spot where they're moving to. These are
	     * used to detect both players moving to the same spot
	     * at the same instant, so that neither player is given
	     * an advantage by coding details.
	     */
	    for (p = 0; p < 2; p++) {
		int pix;
		if (!players[p].dead) {
		    if (i < abs(players[p].dx) || i < abs(players[p].dy)) {
			players[p].x += sign(players[p].dx);
			players[p].y += sign(players[p].dy);
		    }
		    pix = pixel(players[p].x, players[p].y);
		    if (pix >= 4) {
			/* Player has hit another player head-on. */
			players[p].dead = 1;
			players[pix-4].dead = 1;
		    } else if (pix &~ players[p].colour) {
			/* Player has run into an obstacle. */
			players[p].dead = 1;
		    } else {
			plot(players[p].x, players[p].y, 4+p);
		    }
		}
	    }

	    /*
	     * Now update each pixel to the player's real colour.
	     */
	    for (p = 0; p < 2; p++)
		if (!players[p].dead)
		    plot(players[p].x, players[p].y, players[p].colour);
	}

	/*
	 * Draw splats for any dead players.
	 */
	for (p = 0; p < 2; p++)
	    if (players[p].dead)
		fillcircle(players[p].x, players[p].y, 20, players[p].colour);

	scr_done();

	/*
	 * Check if the game is over. Currently this is simply a
	 * matter of seeing whether at least one player is dead; in
	 * future (if I can add multi-tap support) it might be a
	 * more interesting function that takes team play into
	 * account.,
	 */
	for (p = 0; p < 2; p++)
	    if (players[p].dead)
		gameover = 1;

	vsync();
    }

    /*
     * Now the game is over, just wait for a button press (either
     * player) before continuing.
     */
    {
	int done = 0;
	SDL_Event event;

	while (!done && SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYBUTTONDOWN:
		done = 1;
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
	    }
	}
    }
}

int main(void)
{
    /* Set up the SDL game environment */
    setup();

    joys[0] = SDL_JoystickOpen(0);
    joys[1] = SDL_JoystickOpen(1);

    /*
     * Set up the colour palette. For a Tron game we'll need:
     * 
     *  - colour 0 is the black background.
     *  - colour 1 is red (player 1's trail).
     *  - colour 2 is green (player 2's trail).
     *  - colour 3 is white (scenery and weapons fatal to both
     *    players).
     */
    {
	SDL_Color colours[4];
	colours[0].r = colours[0].g = colours[0].b = 0;
	colours[1].r = 255; colours[1].g = colours[1].b = 0;
	colours[2].r = colours[0].b = 0; colours[2].g = 255;
	colours[3].r = colours[3].g = colours[3].b = 255;
	SDL_SetColors(screen, colours, 0, 4);
    }

    /*
     * Get rid of the initial event queue from opening the
     * joysticks (a bunch of button-up events).
     */
    {
	SDL_Event event;
	while (SDL_PollEvent(&event));
    }

    play_game();

    return 0;
}
