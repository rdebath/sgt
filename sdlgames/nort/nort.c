/*
 * nort - a game descended from the Tron light cycles game, but
 * modified almost out of all recognition
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdlstuff.h"
#include "beebfont.h"

#define ABSOLUTE_MAX_SPEED 25
#define JOY_THRESHOLD 4096

#define WEAPON_COLOUR 255

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

#define plot(x,y,c) ( scrdata[(y)*640+(x)*2] = scrdata[(y)*640+(x)*2+1] = c )
#define pixel(x,y) ( scrdata[(y)*640+(x)*2] )

#define plotc(x,y,c) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : plot(x,y,c) )
#define pixelc(x,y) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : pixel(x,y) )

int no_quit_option = 0;

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
		    plotc(x+ix, y+iy, c);
	x += 8;
    }
}

int centretext(int y, int c, char const *text)
{
    puttext(160-4*(strlen(text)), y, c, text);
}

int drawline(int x1, int y1, int x2, int y2, int c)
{
    int dx, dy, lx, ly, sx, sy, dl, ds, d, i;
    int dot, dotting;

    /*
     * Setting bit 8 in `c' causes the line to be dotted
     * alternately on and off.
     */
    dotting = c & 256;
    c &= 255;

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
    plotc(x1, y1, c);
    dot = 256;
    for (i = 0; i < dl; i++) {
	dot ^= dotting;
	d -= 2*ds;
	if (d < 0) {
	    d += 2*dl;
	    x1 += sx; y1 += sy;
	}
	x1 += lx; y1 += ly;
	if (dot) plotc(x1, y1, c);
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
#define doplot(Dy,Dx) ( plotc(x+(Dx),y+(Dy),c|pixelc(x+(Dx),y+(Dy))) )
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
    ACT_NOTHING,
    /* speed */
    ACT_ACCEL, ACT_DECEL,
    /* weapons */
    ACT_RIFLE, ACT_BOMB, ACT_MGUN, ACT_CAGE, ACT_BOLAS, ACT_FLIB_KRONT,
    /* functions */
    ACT_TELEPORT, ACT_CLOAK, ACT_EXTEND,
    /* terminator */
    ACT_LAST
};

SDL_Joystick *joys[2];

struct options {
    int own_fatal;
    int enemy_fatal;
    int auto_accel;
    int scrfill_v;
    int scrfill_h;
    int max_speed;
} opts;

struct player {
    int buttons[8];		       /* sq,x,tr,ci,l1,r1,l2,r2 */
    int colour, cloaked;
    int x, y, dx, dy, fire_dx, fire_dy, speedmod;
    int steering_delay, button_delay[8];
    int dead;
} players[2];

struct scores {
    int games;
    int draws;
    int p[2];
} scores;

int cloak(int players)
{
    SDL_Color colours[3];
    colours[0].r = colours[0].g = colours[0].b = 0;
    colours[1].r = colours[1].g = colours[1].b = 0;
    colours[2].r = colours[2].g = colours[2].b = 0;
    if (!(players & 1))
	colours[0].r = colours[2].r = 255;
    if (!(players & 2))
	colours[1].g = colours[2].g = 255;
    SDL_SetColors(screen, colours, 1, 3);
}

/*
 * Determine if player p2 and its trail should be fatal to player
 * p1 and its trail. Note that the game logic at present assumes
 * that this function is symmetric.
 */
int is_lethal_to(int p, int p2) {
    if (opts.own_fatal && p == p2)
	return 1;
    if (opts.enemy_fatal && p != p2)
	return 1;
    return 0;
}

int play_game(void)
{
    int x, y, c;
    int i, p, gameover;
    int scrfill;

    /*
     * Initially, clear the screen, draw a white border round it,
     * and put up the `press x to start' message.
     */

    scr_prep();
    for (x = 0; x < 320; x++)
	for (y = 0; y < 240; y++) {
	    if (x == 0 || x == 319 || y == 0 || y == 239)
		plot(x, y, WEAPON_COLOUR);
	    else
		plot(x, y, 0);
	}
    centretext(100, WEAPON_COLOUR, "BOTH PLAYERS PRESS \"START\" TO START");
    centretext(190, WEAPON_COLOUR, "(Press \"START\" to return to the");
    centretext(200, WEAPON_COLOUR, "menu once the game is over.)");
    scr_done();

    /*
     * Now wait until we see the Start button (joystick button 9)
     * both pressed and released on each controller.
     */
    {
	int flags = 0;
	SDL_Event event;

	while (flags != 15 && SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYBUTTONDOWN:
		if (event.jbutton.button == 9) {
		    if (event.jbutton.which == 0)
			flags |= 1;
		    else if (event.jbutton.which == 1)
			flags |= 2;
		}
		break;
	      case SDL_JOYBUTTONUP:
		if (event.jbutton.button == 9) {
		    if (event.jbutton.which == 0) {
			flags |= 4;
			scr_prep();
			centretext(120, 1, "Player 1 ready");
			scr_done();
		    } else if (event.jbutton.which == 1) {
			flags |= 8;
			scr_prep();
			centretext(130, 2, "Player 2 ready");
			scr_done();
		    }
		}
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
	    }
	}
    }

    /*
     * Wait for a second before starting the game, to give the
     * second player who pressed Start time to move back to the
     * main buttons.
     */
    scr_prep();
    centretext(160, WEAPON_COLOUR, "GAME STARTING");
    scr_done();

    for (i = 0; i < 50; i++) {
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
	vsync();
    }

    scr_prep();
    /* Re-clear the screen to wipe out all the administrative text. */
    for (x = 0; x < 320; x++)
	for (y = 0; y < 240; y++) {
	    if (x == 0 || x == 319 || y == 0 || y == 239)
		plot(x, y, WEAPON_COLOUR);
	    else
		plot(x, y, 0);
	}
    scr_done();

    /*
     * OK, we're ready to play the actual game. Set up the player
     * structures and options.
     */
    for (p = 0; p < 2; p++) {
	players[p].y = 120;
	players[p].x = (p == 0 ? 1 : 318);
	players[p].dy = 0;
	players[p].dx = (p == 0 ? +1 : -1);
	players[p].cloaked = 0;
	players[p].steering_delay = 0;
	for (i = 0; i < 8; i++)
	    players[p].button_delay[i] = 0;
	players[p].dead = 0;
    }

    /*
     * And begin the main game loop. Ooh!
     */
    gameover = 0;
    scrfill = 0;
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
	for (p = 0; p < 2; p++) if (!players[p].dead) {
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
	    players[p].fire_dx = sign(players[p].fire_dx/JOY_THRESHOLD);
	    players[p].fire_dy = SDL_JoystickGetAxis(joys[p], 1);
	    players[p].fire_dy = sign(players[p].fire_dy/JOY_THRESHOLD);

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
	    extended = 1;
	    players[p].speedmod = (opts.auto_accel ? +1 : 0);
	    for (i = 0; i < 8; i++)
		if (players[p].buttons[i] == ACT_EXTEND &&
		    SDL_JoystickGetButton(joys[p], i))
		    extended = 2;

#define pcoord(xx,yy,n) \
    ( (xx) = players[p].x + players[p].fire_dx * extended * (n), \
      (yy) = players[p].y + players[p].fire_dy * extended * (n) )

	    for (i = 0; i < 8; i++) {
		if (players[p].button_delay[i]) {
		    players[p].button_delay[i]--;
		} else if (SDL_JoystickGetButton(joys[p], i)) {
		    switch(players[p].buttons[i]) {
		      case ACT_ACCEL:
			{
			    players[p].speedmod = +1;
			}
			break;
		      case ACT_DECEL:
			{
			    if (players[p].speedmod == 0)
				players[p].speedmod = -1;
			}
			break;
		      case ACT_RIFLE:
			{
			    int bx, by, ex, ey;
			    pcoord(bx, by, 20);
			    pcoord(ex, ey, 96);
			    drawline(bx, by, ex, ey, WEAPON_COLOUR);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_BOMB:
			{
			    int x, y;
			    pcoord(x, y, 70);
			    fillcircle(x, y, 10, WEAPON_COLOUR);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_MGUN:
			{
			    int bx, by, ex, ey;
			    pcoord(bx, by, 20);
			    pcoord(ex, ey, 96);
			    drawline(bx, by, ex, ey, WEAPON_COLOUR | 256);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_CAGE:
			{
			    int cx, cy;
			    pcoord(cx, cy, 80);
			    drawline(cx-20, cy-15, cx-20, cy+15,WEAPON_COLOUR);
			    drawline(cx+20, cy-15, cx+20, cy+15,WEAPON_COLOUR);
			    drawline(cx-15, cy-20, cx+15, cy-20,WEAPON_COLOUR);
			    drawline(cx-15, cy+20, cx+15, cy+20,WEAPON_COLOUR);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_BOLAS:
			{
			    int bx, by, blx, bly, elx, ely, ex, ey;
			    pcoord(bx, by, 28);
			    pcoord(blx, bly, 32);
			    pcoord(elx, ely, 66);
			    pcoord(ex, ey, 70);
			    drawline(blx, bly, elx, ely, WEAPON_COLOUR);
			    drawline(bx-4, by-4, bx+4, by-4, WEAPON_COLOUR);
			    drawline(bx+4, by-4, bx+4, by+4, WEAPON_COLOUR);
			    drawline(bx+4, by+4, bx-4, by+4, WEAPON_COLOUR);
			    drawline(bx-4, by+4, bx-4, by-4, WEAPON_COLOUR);
			    drawline(ex-4, ey-4, ex+4, ey-4, WEAPON_COLOUR);
			    drawline(ex+4, ey-4, ex+4, ey+4, WEAPON_COLOUR);
			    drawline(ex+4, ey+4, ex-4, ey+4, WEAPON_COLOUR);
			    drawline(ex-4, ey+4, ex-4, ey-4, WEAPON_COLOUR);
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_FLIB_KRONT:
			{
			    int bx, by, j;
			    char const word[][2] = {
				"F","L","I","B"," ","K","R","O","N","T"
			    };
			    for (j = 0; j < lenof(word); j++) {
				pcoord(bx, by, 32+8*j);
				puttext(bx-4, by-3, WEAPON_COLOUR, word[j]);
			    }
			    players[p].button_delay[i] = 3;
			}
			break;
		      case ACT_TELEPORT:
			{
			    int nx, ny;
			    pcoord(nx, ny, 50);
			    players[p].x = (nx<0?0 : nx>319?319 : nx);
			    players[p].y = (ny<0?0 : ny>239?239 : ny);
			    players[p].button_delay[i] = 8;
			}
			break;
		      case ACT_CLOAK:
			{
			    players[p].cloaked = !players[p].cloaked;
			    players[p].button_delay[i] = 8;
			}
			break;
		    }
		}
	    }

#undef pcoord

	    /*
	     * Now do acceleration and deceleration, having decided
	     * which one to do while processing the button presses
	     * above.
	     */
	    if (players[p].speedmod > 0) {
		int speed;
		speed = abs(players[p].dx) + abs(players[p].dy);
		if (speed < opts.max_speed) speed++;
		players[p].dx = sign(players[p].dx) * speed;
		players[p].dy = sign(players[p].dy) * speed;
	    } else if (players[p].speedmod < 0) {
		players[p].dx = sign(players[p].dx);
		players[p].dy = sign(players[p].dy);
	    }
	}

	/*
	 * Move the players.
	 */
	for (i = ABSOLUTE_MAX_SPEED; i-- ;) {
	    /*
	     * Move each player on by one pixel, and in the process
	     * detect whether any player has had a head-on with
	     * another player's exact position.
	     */
	    for (p = 0; p < 2; p++) {
		int pix;
		if (!players[p].dead &&
		    i < abs(players[p].dx) || i < abs(players[p].dy)) {
		    players[p].x += sign(players[p].dx);
		    players[p].y += sign(players[p].dy);
		    if (players[p].x <= 0 || players[p].x >= 319 ||
			players[p].y <= 0 || players[p].y >= 239) {
			/* Clip hard to screen boundaries. */
			players[p].dead = 1;
		    } else {
			int p2;
			/* Check head-on collisions. */
			for (p2 = 0; p2 < p; p2++)
			    if (players[p2].x == players[p].x &&
				players[p2].y == players[p].y) {
				/* Player has hit another player head-on. */
				if (is_lethal_to(p, p2)) {
				    players[p].dead = 1;
				    players[p2].dead = 1;
				}
			    }
			/* If we survived that, check the pixel under us.  */
			if (!players[p].dead) {
			    pix = pixelc(players[p].x, players[p].y);
			    if (pix == WEAPON_COLOUR)
				players[p].dead = 1;
			    for (p2 = 0; p2 < 2; p2++)
				if ((pix & players[p2].colour) &&
				    is_lethal_to(p, p2))
				    players[p].dead = 1;
			}
		    }
		}
	    }

	    /*
	     * Now update each pixel to the player's real colour.
	     */
	    for (p = 0; p < 2; p++)
		if (!players[p].dead &&
		    i < abs(players[p].dx) || i < abs(players[p].dy))
		    plotc(players[p].x, players[p].y,
			  pixelc(players[p].x, players[p].y) | 
			  players[p].colour);
	}

	/*
	 * Draw splats for any dead players.
	 */
	for (p = 0; p < 2; p++)
	    if (players[p].dead)
		fillcircle(players[p].x, players[p].y, 20, players[p].colour);

	/*
	 * Move the screen boundaries inwards if screen filling is
	 * enabled.
	 */
	if (scrfill < 1000) {
	    scrfill++;
	    if (opts.scrfill_v && scrfill < 240) {
		int x, y, y2;
		y = scrfill/2;
		y2 = 239 - y;
		for (x = 0; x < 319; x++) {
		    plot(x, y, WEAPON_COLOUR);
		    plot(x, y2, WEAPON_COLOUR);
		}
	    }
	    if (opts.scrfill_h && scrfill < 320) {
		int x, x2, y;
		x = scrfill/2;
		x2 = 319 - x;
		for (y = 0; y < 239; y++) {
		    plot(x, y, WEAPON_COLOUR);
		    plot(x2, y, WEAPON_COLOUR);
		}
	    }
	}

	/*
	 * Set the colour palette for any cloaked players.
	 */
	i = 0;
	for (p = 0; p < 2; p++)
	    if (players[p].cloaked)
		i |= players[p].colour;
	cloak(i);

	scr_done();

	/*
	 * Check if the game is over. Currently this is simply a
	 * matter of seeing whether at least one player is dead; in
	 * future (if I can add multi-tap support) it might be a
	 * more interesting function that takes team play into
	 * account.
	 */
	for (p = 0; p < 2; p++)
	    if (players[p].dead)
		gameover = 1;

	vsync();
    }

    /*
     * Now the game is over. Restore the colour palette (in case
     * one or more players were cloaked at the time of the game
     * ending) and then just wait for a Start button press (either
     * player) before continuing.
     */
    cloak(0);
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
	    }
	}
    }

    /*
     * Update the scores.
     */
    scores.games++;
    if (!players[0].dead)
	scores.p[0]++;
    else if (!players[1].dead)
	scores.p[1]++;
    else
	scores.draws++;
}

int game_setup_done = 0;

enum {
    MM_GAME, MM_RESET, MM_SETUP, MM_QUIT
};

int main_menu(void)
{
    const struct {
	int y;
	char const *text;
	int action;
    } menu[] = {
	{120, "Play Game", MM_GAME},
	{134, "Reset Scores", MM_RESET},
	{148, "Game Setup", MM_SETUP},
	{162, "Exit NORT", MM_QUIT},
    };

    int menumin, i, j, flags = 0, prevval = 0, redraw;
    int menulen = (no_quit_option ? lenof(menu)-1 : lenof(menu));
    int x, y, c1, c2;
    SDL_Event event;

    /*
     * Clear the screen and display the main menu.
     */

    scr_prep();
    for (x = 0; x < 320; x++)
	for (y = 0; y < 240; y++)
	    plot(x, y, 0);

    if (game_setup_done)
	menumin = 0;
    else
	menumin = 2;		       /* only Setup and Quit allowed */

    centretext(50, WEAPON_COLOUR, "NORT");
    centretext(66, WEAPON_COLOUR, "A SPL@ Production");

    for (i = menumin; i < menulen; i++)
	centretext(menu[i].y, WEAPON_COLOUR, menu[i].text);

    /*
     * Display the scores.
     */
    if (game_setup_done) {
	char text[40];
	int left, right;
	sprintf(text, "Player 1 %4d", scores.p[0]);
	left = 148 - 8*strlen(text);
	puttext(left, 200, 1, text);
	sprintf(text, "%-4d Player 2", scores.p[1]);
	puttext(172, 200, 2, text);
	right = 172 + 8*strlen(text);
	puttext(156, 200, WEAPON_COLOUR, "-");
	sprintf(text, "%d game%s", scores.games, scores.games==1?"":"s");
	puttext(left, 212, WEAPON_COLOUR, text);
	sprintf(text, "%d drawn", scores.draws);
	puttext(right - 8*strlen(text), 212, WEAPON_COLOUR, text);
    }

    scr_done();

    i = menumin;
    redraw = 1;

    /*
     * Push an initial event to ensure a redraw happens
     * immediately.
     */
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);

    while (flags != 3 && SDL_WaitEvent(&event)) {

	switch(event.type) {
	  case SDL_JOYBUTTONDOWN:
	    if (event.jbutton.button == 1)
		flags |= 1;
	    break;
	  case SDL_JOYBUTTONUP:
	    if (event.jbutton.button == 1)
		flags |= 2;
	    break;
	  case SDL_JOYAXISMOTION:
	    if (event.jaxis.axis == 1) {
		int val = event.jaxis.value / JOY_THRESHOLD;
		if (val < 0 && prevval >= 0) {
		    if (--i < menumin)
			i = menulen-1;
		    redraw = 1;
		} else if (val > 0 && prevval <= 0) {
		    if (++i >= menulen)
			i = menumin;
		    redraw = 1;
		}
		prevval = val;
	    }
	    break;
	  case SDL_KEYDOWN:
	    if (event.key.keysym.sym == SDLK_ESCAPE)
		exit(1);
	    break;
	}

	if (redraw) {
	    scr_prep();
	    for (j = menumin; j < menulen; j++) {
		c1 = (j == i ? 1 : 0);
		c2 = (j == i ? 2 : 0);
		drawline(159-52, menu[j].y-3, 159-48, menu[j].y-3, c1);
		drawline(159-52, menu[j].y-3, 159-52, menu[j].y+1, c1);
		drawline(159-52, menu[j].y+9, 159-48, menu[j].y+9, c2);
		drawline(159-52, menu[j].y+9, 159-52, menu[j].y+5, c2);
		drawline(159+52, menu[j].y-3, 159+48, menu[j].y-3, c2);
		drawline(159+52, menu[j].y-3, 159+52, menu[j].y+1, c2);
		drawline(159+52, menu[j].y+9, 159+48, menu[j].y+9, c1);
		drawline(159+52, menu[j].y+9, 159+52, menu[j].y+5, c1);
	    }
	    scr_done();
	    redraw = 0;
	}

    }

    return menu[i].action;
}

void button_symbol(int x, int y, int button)
{
    unsigned char fourbuttons[8*4] = {
	0x7F, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7F, 0x00,
	0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00,
	0x08, 0x14, 0x14, 0x22, 0x22, 0x41, 0x7F, 0x00,
	0x1C, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1C, 0x00,
    };

    char const *otherfour[4] = { "L1", "R1", "L2", "R2" };

    int ix, iy;

    if (button < 4) {
	for (ix = 0; ix < 8; ix++)
	    for (iy = 0; iy < 8; iy++)
		if (fourbuttons[button*8+iy] & (0x80>>ix))
		    plotc(x+4+ix, y+iy, button+128);
    } else
	puttext(x, y, WEAPON_COLOUR, otherfour[button-4]);
}

void setup_game(void)
{
    struct {
	/* Coordinates of item for each player. */
	int x[2], y;
	/* Width of item (for showing selection box). */
	int w;
	/* Where to move (relative position in array indices) if the user
	 * moves the controller. */
	int up, down, left, right;
    } items[] = {
	{{160, 200}, 24+0*12, 2, 1, -2, 0, 0},
	{{160, 200}, 24+1*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+2*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+3*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+4*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+5*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+6*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+7*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+8*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+9*12, 2, 1, -1, 0, 0},
	{{160, 200}, 24+10*12, 2, 1, -1, 0, 0},
	{{160, 160}, 172+0*12, 8, 1, -1, 0, 0},
	{{160, 160}, 172+1*12, 14, 1, -1, 0, 0},
	{{160, 160}, 172+2*12, 3, 1, -1, 0, 0},
	{{160, 160}, 172+3*12, 1, 2, -1, 0, +1},
	{{240, 240}, 172+3*12, 1, 1, -2, -1, 0},
	{{104, 104}, 224, 14, 1, -1, 0, 0},
    };
    SDL_Event event;
    char speedstr[20];

    int i, p;
    int index[2];
    int prevjoy[2][2];

    for (p = 0; p < 2; p++) {
	index[p] = 0;
	for (i = 0; i < 2; i++)
	    prevjoy[p][i] = 0;
    }

    while (1) {
	/*
	 * Every time we redisplay, we'll redraw the whole menu. It
	 * seems simpler to do it that way.
	 */
	scr_prep();
	memset(scrdata, 0, 640*240);

	/*
	 * Draw selection crosshairs for each player's cursor.
	 */
	for (p = 0; p < 2; p++) {
	    int idx = index[p];
	    int x = items[idx].x[p], y = items[idx].y, w = items[idx].w*8;
	    int c = players[p].colour;

	    /* Special case: check if both crosshairs overlap. */
	    if (index[1-p] == index[p] && items[idx].x[0] == items[idx].x[1])
		c = WEAPON_COLOUR;

	    drawline(x-8, y-2, x-4, y-2, c);
	    drawline(x-8, y-2, x-8, y+2, c);
	    drawline(x-8, y+8, x-4, y+8, c);
	    drawline(x-8, y+8, x-8, y+4, c);
	    drawline(x+w+8, y-2, x+w+4, y-2, c);
	    drawline(x+w+8, y-2, x+w+8, y+2, c);
	    drawline(x+w+8, y+8, x+w+4, y+8, c);
	    drawline(x+w+8, y+8, x+w+8, y+4, c);
	}

	puttext(8, 8, WEAPON_COLOUR, "Assign buttons to weapons and actions:");
	puttext(16, 24+0*12, WEAPON_COLOUR, "Accelerate");
	puttext(16, 24+1*12, WEAPON_COLOUR, "Brake");
	puttext(16, 24+2*12, WEAPON_COLOUR, "Rifle");
	puttext(16, 24+3*12, WEAPON_COLOUR, "Bomb");
	puttext(16, 24+4*12, WEAPON_COLOUR, "Machine Gun");
	puttext(16, 24+5*12, WEAPON_COLOUR, "Cage");
	puttext(16, 24+6*12, WEAPON_COLOUR, "Bolas");
	puttext(16, 24+7*12, WEAPON_COLOUR, "FLIB KRONT");
	puttext(16, 24+8*12, WEAPON_COLOUR, "Teleport");
	puttext(16, 24+9*12, WEAPON_COLOUR, "Cloaking");
	puttext(16, 24+10*12, WEAPON_COLOUR, "Extend weapon");

	for (p = 0; p < 2; p++) {
	    for (i = 0; i < 8; i++) {
		int act = players[p].buttons[i];
		if (act != ACT_NOTHING) {
		    button_symbol(160+40*p, 24+12*(act - (ACT_NOTHING+1)), i);
		}
	    }
	}

	puttext(8, 156, WEAPON_COLOUR, "Select game options:");

	puttext(16, 172+0*12, WEAPON_COLOUR, "Safe trails:");
	puttext(160, 172+0*12, WEAPON_COLOUR,
		opts.enemy_fatal ?
		(opts.own_fatal ? "None" : "Your own") :
		(opts.own_fatal ? "Enemy" : "All"));

	puttext(16, 172+1*12, WEAPON_COLOUR, "Walls closing in:");
	puttext(160, 172+1*12, WEAPON_COLOUR,
		(!opts.scrfill_v && !opts.scrfill_h ? "None" :
		 opts.scrfill_v && !opts.scrfill_h ? "Top and bottom" :
		 !opts.scrfill_v && opts.scrfill_h ? "Left and right" :
		 /* opts.scrfill_v && opts.scrfill_h ? */ "All four"));

	puttext(16, 172+2*12, WEAPON_COLOUR, "Auto-accelerate:");
	puttext(160, 172+2*12, WEAPON_COLOUR, opts.auto_accel ? "On" : "Off");

	puttext(16, 172+3*12, WEAPON_COLOUR, "Maximum speed:");
	sprintf(speedstr, "%d", opts.max_speed);
	puttext(160, 172+3*12, WEAPON_COLOUR, "-");
	puttext(200, 172+3*12, WEAPON_COLOUR, speedstr);
	puttext(240, 172+3*12, WEAPON_COLOUR, "+");

	puttext(104, 224, WEAPON_COLOUR, "Finished Setup");

	scr_done();

	while (SDL_WaitEvent(&event)) {
	    switch(event.type) {
	      case SDL_JOYAXISMOTION:
		{
		    int axis = event.jaxis.axis;
		    int val = event.jaxis.value / JOY_THRESHOLD;
		    int displace = 0;
		    p = event.jaxis.which;
		    if (val < 0 && prevjoy[p][axis] >= 0) {
			displace = (axis ?
				    items[index[p]].down :
				    items[index[p]].left);
		    } else if (val > 0 && prevjoy[p][axis] <= 0) {
			displace = (axis ?
				    items[index[p]].up :
				    items[index[p]].right);
		    }
		    prevjoy[p][axis] = val;
		    index[p] = (index[p] + displace) % lenof(items);
		    if (displace) goto redraw;
		}
		break;
	      case SDL_JOYBUTTONDOWN:
		{
		    int button = event.jbutton.button;
		    p = event.jaxis.which;
		    if (index[p] >= 0 && index[p] < 11) {
			int act = ACT_NOTHING+1+index[p];
			/*
			 * Assign this button to this action.
			 */
			players[p].buttons[button] = act;
			for (i = 0; i < 8; i++)
			    if (button != i && players[p].buttons[i] == act)
				players[p].buttons[i] = ACT_NOTHING;
			goto redraw;
		    } else if (index[p] == 11) {
			/*
			 * Cycle the safe-trails option.
			 */
			opts.own_fatal = !opts.own_fatal;
			if (!opts.own_fatal)
			    opts.enemy_fatal = !opts.enemy_fatal;
			goto redraw;
		    } else if (index[p] == 12) {
			/*
			 * Cycle the screen-fill options..
			 */
			if (opts.scrfill_v)
			    opts.scrfill_h = !opts.scrfill_h;
			opts.scrfill_v = !opts.scrfill_v;
			goto redraw;
		    } else if (index[p] == 13) {
			/*
			 * Cycle the auto-accelerate option.
			 */
			opts.auto_accel = !opts.auto_accel;
			goto redraw;
		    } else if (index[p] == 14) {
			/*
			 * Reduce the maximum speed.
			 */
			if (opts.max_speed > 1)
			    opts.max_speed--;
			goto redraw;
		    } else if (index[p] == 15) {
			/*
			 * Increase the maximum speed.
			 */
			if (opts.max_speed < ABSOLUTE_MAX_SPEED)
			    opts.max_speed++;
			goto redraw;
		    } else if (index[p] == 16) {
			/*
			 * Finished game setup.
			 */
			game_setup_done = 1;
			return;
		    }
		}
		break;
	      case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		    exit(1);
		break;
	    }
	}
	redraw:
    }
}

void reset_scores(void)
{
    scores.p[0] = scores.p[1] = 0;
    scores.games = scores.draws = 0;
}

int main(int argc, char **argv)
{
    int p;

    if (argc > 1 && !strcmp(argv[1], "-n"))
	no_quit_option = 1;

    /* Set up the SDL game environment */
    setup();

    joys[0] = SDL_JoystickOpen(0);
    joys[1] = SDL_JoystickOpen(1);

    /*
     * Set up the colour palette.
     */
#define setcolour(c,r,g,b) do { \
    static SDL_Color colour = { r, g, b }; \
    SDL_SetColors(screen, &colour, c, 1); \
} while (0)
    /*
     * These colours are for playing the actual game:
     *
     *  - colour 0 is the black background.
     *  - colour 1 is red (player 1's trail).
     *  - colour 2 is green (player 2's trail).
     *  - colour 3 is yellow (player 1 + player 2 combined trail)
     *
     *  - WEAPON_COLOUR is white (scenery plus weapons, fatal
     *    to everyone)
     */
    setcolour(0, 0, 0, 0);
    setcolour(1, 255, 0, 0);
    setcolour(2, 0, 255, 0);
    setcolour(3, 255, 255, 0);
    setcolour(WEAPON_COLOUR, 255, 255, 255);

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
     * Set up the initial controller button configuration and game
     * options, for the players to modify the first time they run
     * the game setup.
     */
    for (p = 0; p < 2; p++) {
	players[p].buttons[0] = ACT_DECEL;
	players[p].buttons[1] = ACT_ACCEL;
	players[p].buttons[3] = ACT_RIFLE;
	players[p].buttons[2] = ACT_TELEPORT;
	players[p].buttons[5] = players[p].buttons[4] = ACT_NOTHING;
	players[p].buttons[6] = players[p].buttons[7] = ACT_NOTHING;
	players[p].colour = p+1;
	opts.own_fatal = 0;
	opts.enemy_fatal = 1;
	opts.auto_accel = 0;
	opts.scrfill_v = 0;
	opts.scrfill_h = 0;
	opts.max_speed = 4;
    }

    reset_scores();

    while (1) {
	switch (main_menu()) {
	  case MM_QUIT:
	    return 0;		       /* finished */
	  case MM_SETUP:
	    setup_game();
	    reset_scores();
	    break;
	  case MM_RESET:
	    reset_scores();
	    break;
	  case MM_GAME:
	    play_game();
	    break;
	}
    }

    return 0;
}
