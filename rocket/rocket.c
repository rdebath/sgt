/*
 * Rocket Attack - a simple game in which two Thrust-style rockets
 * attempt to shoot one another down
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "sdlstuff.h"
#include "game256.h"
#include "rocket.h"
#include "utils.h"

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE 0
#define TRUE 1

#define JOY_THRESHOLD 4096
static SDL_Joystick *joys[2];

#ifdef DEBUG
FILE *debugfp;
#endif

struct player {
    int x,y,dx,dy;
    int a, aa;
    int score;
    int death, firedelay;
    int after, adraw, agrow;
    int invuln;
    int landed;
    Sprite spr;
};

struct pixel {
    int xp,yp,px,py,dxp,dyp;
    int colour, life, free;
};

struct bullet {
    int xb,yb,dxb,dyb;
    int life;
    Sprite spr;
    int free;
};

#define MAXPIX 400
#define MAXBUL 3
#define STRINGLEN 30
#define MAXAFTER 100
#define DEATHTIME 200
#define MINFIREDELAY 6
#define GRAVITY 1500

static const int sine[40] = {
    4000,3923,3696,3326,2828,2222,1531,780,
    0,-780,-1531,-2222,-2828,-3326,-3696,-3923,
    -4000,-3923,-3696,-3326,-2828,-2222,-1531,-780,
    0,780,1531,2222,2828,3326,3696,3923,
    4000,3923,3696,3326,2828,2222,1531,780
};

static int no_quit_option = 0;

struct pixel pixels[MAXPIX];
struct player players[2];
struct bullet bullets[2][MAXBUL];
int firstfreepixel;
Image bk_after;
int space, planet, balls;

static void init_game(void)
{
    int p, q;

    for (p = 0; p < MAXPIX; p++)
	pixels[p].free = TRUE;

    firstfreepixel = 0;

    for (p = 0; p < 2; p++) {
	for (q = 0; q < MAXBUL; q++) {
	    bullets[p][q].free = TRUE;
	    if (q == 0)
		bullets[p][q].spr = makesprite(7, 7, 0);
	    else
		bullets[p][q].spr = makesprite(3, 3, 0);
	}

	players[p].spr = makesprite(17, 17, 255);
	players[p].score = 0;
    }

    scr_prep();
    bar(0, 0, 319, 199, 0);
    drawimage(rocket_bkgnd_image,0,0,-1);
    bk_after = makeimage(2, 9, 0, 0);
    pickimage(bk_after,160,190);
    if (space) {
	putsprite(players[0].spr,rocket_player_images[0][8],80,95);
	putsprite(players[1].spr,rocket_player_images[1][24],240,95);
	if (balls) {
	    putsprite(bullets[0][0].spr,rocket_ball_image,100,95);
	    putsprite(bullets[1][0].spr,rocket_ball_image,220,95);
	}
    } else {
	putsprite(players[0].spr,rocket_player_images[0][0],85,180);
	putsprite(players[1].spr,rocket_player_images[1][0],234,180);
    }

    bar(37,190,36+MAXAFTER,198,1);
    bar(282,190,283-MAXAFTER,198,1);
    // drawimage(rocket_titletext_image,160,10,0); perhaps not
    // ShowScores; FIXME
    scr_done();
}

static void invsqgrav(int x, int y, int *dx, int *dy)
{
    int r, rr;
    
    x = abs(x-160) >> 16;
    y = abs(y-95) >> 16;
    rr = x*x+y*y;
    r = sqrt(rr) + 0.5;
    *dx -= (x >> 22) / (r*rr);
    *dy -= (y >> 22) / (r*rr);
}

static void newpix(int x, int y, int dx, int dy, int alife, int col, int rnd)
{
    if (firstfreepixel >= MAXPIX)
	return;

    pixels[firstfreepixel].xp = x;
    pixels[firstfreepixel].yp = y;
    pixels[firstfreepixel].dxp = dx;
    pixels[firstfreepixel].dyp = dy;
    if (rnd) {
	pixels[firstfreepixel].dxp += randupto(64000) - 32000;
	pixels[firstfreepixel].dyp += randupto(64000) - 32000;
    }
    pixels[firstfreepixel].life = alife;
    pixels[firstfreepixel].colour = col;
    pixels[firstfreepixel].free = FALSE;
    do {
	firstfreepixel++;
    } while (firstfreepixel < MAXPIX && !pixels[firstfreepixel].free);
}

static void fire(struct bullet *bullets, int x, int y, int dx, int dy)
{
    int p, q;

    q = -1;
    for (p = 0; p < MAXBUL; p++)
	if (bullets[p].free) {
	    q = p;
	    break;
	}

    if (q < 0)
	return;

    bullets[q].xb = x;
    bullets[q].yb = y;
    bullets[q].dxb = dx;
    bullets[q].dyb = dy;
    bullets[q].life = 500;
    bullets[q].free = FALSE;
}

static void check_tree(int x, int y, int *death)
{
    x = sign(x) * (abs(x) >> 16);
    y = sign(y) * (abs(y) >> 16);
    if (y < 180 && y >= 0 && x >= 0 && x <= 319 &&
	getimagepixel(rocket_bkgnd_image, x, y) < 192)
	*death = DEATHTIME;
}

static int check(Image img, int x, int y, int *death)
{
    x = sign(x) * (abs(x) >> 16);
    y = sign(y) * (abs(y) >> 16);
    if (x + XOFFSET(img) >= 0 && x + XOFFSET(img) < XSIZE(img) &&
	y + YOFFSET(img) >= 0 && y + YOFFSET(img) < YSIZE(img) &&
	getimagepixel(img, x, y) != 255)
	*death = DEATHTIME;
}

static int init_player(int p)
{
    if (space) {
	players[p].x = (p*160+80) << 16;
	players[p].y = 95 << 16;
	players[p].a = 32 * p + 16;
	players[p].landed = FALSE;
	players[p].invuln = !planet;
	if (balls) {
	    bullets[p][0].xb = players[p].x + (1-2*p) * (20<<16);
	    bullets[p][0].yb = players[p].y;
	    bullets[p][0].dxb = bullets[p][0].dyb = 0;
	}
    } else {
	players[p].x = (149*p + 85) << 16;
	players[p].y = 180 << 16;
	players[p].a = 0;
	players[p].landed = TRUE;
	players[p].invuln = FALSE;
    }
    players[p].aa = players[p].a >> 1;
    players[p].dx = players[p].dy = 0;
    players[p].after = MAXAFTER;
    players[p].adraw = 0;
    players[p].agrow = 0;
    players[p].firedelay = 0;
    players[p].death = 0;
}

static void play_game(void)
{
    int p;
    int c, scnew;
    int fx, bulletmaxy, q, r, s, sin, cos;
    int thrust;
    float rr;

    if (space)
	bulletmaxy = 188 << 16;
    else
	bulletmaxy = 180 << 16;

    for (p = 0; p < 2; p++)
	init_player(p);

    scnew = 0;

    while (1) {

	/*
	 * Listen to waiting events in case ESC was pressed.
	 */
	{
	    SDL_Event event;

	    while (SDL_PollEvent(&event)) {
		int action;

		switch(event.type) {
		  case SDL_KEYDOWN:
		    /* Standard catch-all quit-on-escape clause. */
		    if (event.key.keysym.sym == SDLK_ESCAPE)
			exit(1);
		}
	    }
	}

	scr_prep();

	for (p = 2; p-- ;)
	    erasesprite(players[p].spr);

	for (p = 2; p-- ;)
	    for (q = MAXBUL; q-- ;)
		erasesprite(bullets[p][q].spr);

#if 0   /* FIXME: weird afterburner bar drawing from hell */
      with Players[1] do if ADraw>0 then case ADraw of
        1,2: begin
          if After>1 then
            Bar(35+After,190,36+After,198,1)
          else
            Bar(36+After,190,36+After,198,1);
          DrawImage(BkAfter,37+After,190,-1);
          Dec(ADraw);
        end;
        3,4: begin
          Bar(37,190,36+After,198,1);
          DrawImage(BkAfter,37+After,190,-1);
          Dec(ADraw);
          if ADraw=2 then ADraw:=0;
        end;
      end;
      with Players[2] do if ADraw>0 then case ADraw of
        1,2: begin
          if After>1 then
            Bar(284-After,190,283-After,198,1)
          else
            Bar(283-After,190,283-After,198,1);
          DrawImage(BkAfter,281-After,190,-1);
          Dec(ADraw);
        end;
        3,4: begin
          Bar(282,190,283-After,198,1);
          DrawImage(BkAfter,281-After,190,-1);
          Dec(ADraw);
          if ADraw=2 then ADraw:=0;
        end;
      end;
#endif

	/*
	 * Erase and move pixels.
	 */
	for (p = 0; p < MAXPIX; p++) {
	    struct pixel *pix = &pixels[p];

	    if (!pix->free) {
		if (pix->px >= 0 && pix->py >= 0 &&
		    pix->px < (320<<16) && pix->py < (190<<16))
		    plot(pix->px >> 16, pix->py >> 16,
			 getimagepixel(rocket_bkgnd_image,
				       pix->px >> 16, pix->py >> 16));
		pix->px = pix->xp;
		pix->py = pix->yp;
		pix->xp += pix->dxp;
		pix->yp += pix->dyp;
		if (!space) {
		    pix->dyp += GRAVITY;
		    if (pix->yp > (180 << 16)) {
			pix->yp = (180 << 16);
			pix->dyp = -abs(pix->dyp);
		    }
		}
		pix->life--;
		if (pix->life <= 0) {
		    pix->free = TRUE;
		    if (firstfreepixel > p)
			firstfreepixel = p;
		}
		if (pix->life > 1 &&
		    pix->yp >= 0 &&
		    pix->xp >= 0 &&
		    pix->xp < (320 << 16) &&
		    pix->yp < (190 << 16)) {
		    plot(pix->xp >> 16, pix->yp >> 16, pix->colour);
		}
	    }
	}

	if (balls) {
	    for (p = 0; p < 2; p++) {
		struct player *pl = &players[p];
		struct bullet *bul = &bullets[p][0];

		q = (pl->x - bul->xb) >> 16;
		r = (pl->y - bul->yb) >> 16;
		s = q*q + r*r;
		if (s > STRINGLEN * STRINGLEN) {
		    fx = ((pl->dx-bul->dxb)*q+(pl->dy-bul->dyb)*r);
		    if (fx>0) {
			fx = fx / (2*s);
			bul->dxb += fx*q;
			bul->dyb += fx*r;
			pl->dx -= fx*q;
			pl->dy -= fx*r;
		    }
		    rr = 1/sqrt(s);
		    bul->xb = 0.5 + (pl->x-(STRINGLEN << 16)*rr*q);
		    bul->yb = 0.5 + (pl->y-(STRINGLEN << 16)*rr*r);
		}

		bul->xb += bul->dxb;
		bul->yb += bul->dyb;
		if (bul->xb < (9 << 16)) {
		    bul->xb = 9 << 16;
		    bul->dxb = 0;
		}
		if (bul->yb < (9 << 16)) {
		    bul->yb = 9 << 16;
		    bul->dyb = 0;
		}
		if (bul->xb > (311 << 16)) {
		    bul->xb = 311 << 16;
		    bul->dxb = 0;
		}
		if (bul->yb > (181 << 16)) {
		    bul->yb = 181 << 16;
		    bul->dyb = 0;
		}

		if (!pl->death)
		    putsprite(bul->spr, rocket_ball_image,
			      bul->xb >> 16, bul->yb >> 16);
	    }
	} else {
	    for (p = 0; p < 2; p++) for (q = 0; q < MAXBUL; q++)
	        if (!bullets[p][q].free) {
		    struct player *pl = &players[p];
		    struct bullet *bul = &bullets[p][q];

		    bul->xb += bul->dxb;
		    bul->yb += bul->dyb;
		    if (space && planet)
			invsqgrav(bul->xb, bul->yb, &bul->dxb, &bul->dyb);
		    c = 0;
		    check_tree(bul->xb, bul->yb, &c);
		    if (bul->life > 2 && 
			(c != 0 ||
			 (bul->yb < 0 || bul->yb >= bulletmaxy ||
			  bul->xb < 0 || bul->xb >= (320 << 16))))
			bul->life = 2;
		    bul->life--;
		    if (bul->life <= 0)
			bul->free = TRUE;
		    if (bul->life > 2)
			putsprite(bul->spr, rocket_bullet_images[p],
				  bul->xb >> 16, bul->yb >> 16);
		}
	}

	for (p = 0; p < 2; p++)
	    if (players[p].death == 0)
		putsprite(players[p].spr,
			  rocket_player_images[p][players[p].aa],
			  players[p].x >> 16,
			  players[p].y >> 16);

	for (p = 0; p < 2; p++) {
	    struct player *pl = &players[p];
	    int axis, kthrust, kburner, kfire;

	    if (pl->death)
		continue;

	    kfire = SDL_JoystickGetButton(joys[p], 2);   /* Circle */
	    axis = SDL_JoystickGetAxis(joys[p], 0) / JOY_THRESHOLD;
	    kthrust = SDL_JoystickGetButton(joys[p], 1);   /* X */
	    kburner = SDL_JoystickGetButton(joys[p], 4);   /* L1 */

	    if (kfire || axis || kthrust || kburner)
		pl->invuln = FALSE;

	    if (axis && !pl->landed)
		pl->a = (pl->a - sign(axis)) & 63;

	    pl->aa = pl->a >> 1;
	    cos = sine[pl->aa+8];
	    sin = -sine[pl->aa];

	    thrust = 0;
	    if (kthrust)
		thrust = 1;

	    if (kburner) {
		if (pl->after > 1) {
		    thrust = 2;
		    pl->after--;
		    // FIXME: bugger about with pl->adraw
		} else
		    thrust = 1;
	    } else {
		pl->agrow++;
		if (pl->agrow >= 4 && pl->after < MAXAFTER) {
		    pl->after++;
		    // FIXME: bugger about with pl->adraw
		    pl->agrow = 0;
		}
	    }

	    if (thrust) {
		pl->landed = FALSE;
		newpix(pl->x-51*cos,pl->y-51*sin,pl->dx-5*cos,pl->dy-5*sin,
		       60,139+thrust+2*randupto(2),TRUE);
		pl->dx += thrust*cos;
		pl->dy += thrust*sin;
	    }

	    pl->x += pl->dx;
	    pl->y += pl->dy;

	    if (space && planet)
		invsqgrav(pl->x, pl->y, &pl->dx, &pl->dy);
	    else if (!space && !pl->landed)
		pl->dy += GRAVITY;

	    if (!space) {
		if (pl->y >= (180 << 16) && pl->dy > 0 && !pl->landed) {
		    if (pl->aa == 0 && abs(pl->dx)+pl->dy<=50000) {
			pl->y = 180 << 16;
			pl->landed = TRUE;
			pl->dy = 0;
			pl->dx = 0;
			pl->a = 0;
		    } else
			pl->death = DEATHTIME;
		}
	    }

	    if (space) {
		if (pl->x < (9 << 16)) {
		    pl->x = 9 << 16;
		    pl->dx = 0;
		}
		if (pl->y < (9 << 16)) {
		    pl->y = 9 << 16;
		    pl->dy = 0;
		}
		if (pl->x > (311 << 16)) {
		    pl->x = 311 << 16;
		    pl->dx = 0;
		}
		if (pl->y > (181 << 16)) {
		    pl->y = 181 << 16;
		    pl->dy = 0;
		}
	    } else {
		if (pl->y < (9 << 16)) {
		    pl->y = 9 << 16;
		    pl->dy = 0;
		}
		while (pl->x < -(9 << 16))
		    pl->x += 338 << 16;
		while (pl->x > (329 << 16))
		    pl->x -= 338 << 16;
		pl->dx -= sign(pl->dx) * (abs(pl->dx) >> 6);
		pl->dy -= sign(pl->dy) * (abs(pl->dy) >> 6);
	    }

	    if (kfire && !pl->landed && !pl->firedelay && !balls) {
		fire(bullets[p],pl->x+165*cos,pl->y+165*sin,
		     pl->dx+45*cos,pl->dy+45*sin);
		pl->firedelay = MINFIREDELAY;
	    }
	    if (!kfire)
		pl->firedelay = 0;
	    if (pl->firedelay > 0)
		pl->firedelay--;
	    check_tree(pl->x+147*cos,pl->y+147*sin, &pl->death);
	    check_tree(pl->x-48*cos-63*sin,pl->y-48*sin+63*cos,&pl->death);
	    check_tree(pl->x-48*cos+63*sin,pl->y-48*sin-63*cos,&pl->death);
	}

	if (players[0].death == 0 && players[1].death == 0) {
	    for (p = 0; p < 2; p++) {
		cos = sine[players[1-p].aa+8];
		sin = -sine[players[1-p].aa];
		check(rocket_player_images[p][players[p].aa],
		      players[1-p].x+131*cos-players[p].x,
		      players[1-p].y+131*sin-players[p].y,
		      &players[0].death);
		check(rocket_player_images[p][players[p].aa],
		      players[1-p].x-48*cos-63*sin-players[p].x,
		      players[1-p].y-48*sin+63*cos-players[p].y,
		      &players[0].death);
		check(rocket_player_images[p][players[p].aa],
		      players[1-p].x-48*cos+63*sin-players[p].x,
		      players[1-p].y-48*sin-63*cos-players[p].y,
		      &players[0].death);
	    }
	    players[1].death = players[0].death;
	}

	if (balls) {
	    if (players[0].death == 0 && players[1].death == 0) {
		for (p = 0; p < 2; p++) {
		    struct player *pl = &players[p];
		    struct bullet *ball = &bullets[1-p][0];
		    check(rocket_player_images[p][pl->aa],
			  ball->xb+(2 << 16)-pl->x,
			  ball->yb+(2 << 16)-pl->y,&pl->death);
		    check(rocket_player_images[p][pl->aa],
			  ball->xb+(2 << 16)+pl->x,
			  ball->yb+(2 << 16)-pl->y,&pl->death);
		    check(rocket_player_images[p][pl->aa],
			  ball->xb+(2 << 16)-pl->x,
			  ball->yb+(2 << 16)+pl->y,&pl->death);
		    check(rocket_player_images[p][pl->aa],
			  ball->xb+(2 << 16)+pl->x,
			  ball->yb+(2 << 16)+pl->y,&pl->death);
		}
	    }
	} else {
	    for (p = 0; p < 2; p++) {
		struct player *pl = &players[p];
		if (pl->death)
		    continue;

		for (q = 0; q < 2; q++)
		    for (r = 0; r < MAXBUL; r++) {
			struct bullet *bul = &bullets[q][r];
			if (bul->free)
			    continue;
			check(rocket_player_images[p][pl->aa],
			      bul->xb-pl->x,bul->yb-pl->y,&pl->death);
		    }
	    }
	}

	for (p = 0; p < 2; p++) {
	    struct player *pl = &players[p];

	    if (pl->invuln)
		pl->death = 0;
	    if (pl->death == DEATHTIME) {
		players[1-p].score++;
		scnew = 2;
		for (q = -8; q <= 8; q++)
		    for (r = -8; r <= 8; r++) {
			c=getimagepixel(rocket_player_images[p][pl->aa],q,r);
			if (c != 255)
			    newpix(pl->x+(q << 16),
				   pl->y+(r << 16),
				   q << 13,r << 13,
				   100+randupto(100),c,TRUE);
		    }
	    }

	    if (pl->death > 0) {
		pl->death--;
		if (pl->death == 0)
		    init_player(p);
	    }
	}

	if (scnew > 0) {
	    scnew--;
	    // ShowScores;
	}

	scr_done();
	vsync();
    }
}

int main(int argc, char **argv)
{
    int p;

    if (argc > 1 && !strcmp(argv[1], "-n"))
	no_quit_option = 1;

#ifdef DEBUG
debugfp = fdopen(3, "w");
setbuf(debugfp, NULL);
#endif

    rocket_make_player_images();

    /* Set up the SDL game environment */
    setup();

    joys[0] = SDL_JoystickOpen(0);
    joys[1] = SDL_JoystickOpen(1);

    space = planet = balls = 0;	       /* FIXME: menu system */

    rocket_do_palette(space);
    rocket_make_background(space, planet);
    init_game();
    
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
