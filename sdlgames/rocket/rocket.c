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
    int after, agrow;
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

static void show_scores(void)
{
    const int digits[56] = {
      0xFE,0x101,0x101,0x101,0x1F1,0xFE,0x1F,0x1FF,0x8E,0x11F,0x111,0x111,0x111,0xE1,
      0x82,0x111,0x111,0x111,0x11F,0xFE,0x1F8,0x4,0x4,0x7,0x1F,0x4,
      0xE2,0x111,0x111,0x111,0x11F,0x10E,0xFE,0x111,0x111,0x111,0x11F,0x8E,
      0x100,0x100,0x100,0x100,0x11F,0xFF,0xFE,0x1F1,0x111,0x111,0x111,0xFE,
      0xE0,0x110,0x110,0x110,0x11F,0xFF
    };
    const int start[11] = {0,6,8,14,20,26,32,38,44,50,56};

    int w[70];
    char s[20];
    int p, q, r, i, wp;

    for (p = 0; p < 2; p++) {
	struct player *pl = &players[p];

	for (q = 0; q < 70; q++)
	    w[q] = 0;
	wp = 34;
	sprintf(s, "%d", pl->score);
	for (q = 0; s[q]; q++) {
	    r = s[q] - '0';
	    for (i = start[r]; i < start[r+1]; i++) {
		wp++;
		w[wp] = digits[i];
	    }
	    wp++;
	    w[wp] = 0;
	}
	q = (p == 0 ? 0 : SCR_WIDTH - 36);
	wp = (p == 0 ? 34 : wp-35);
	for (r = 0; r <= 35; r++)
	    for (i = 0; i < 9; i++) {
		if (w[r+wp] & (256 >> i))
		    plot(r+q, SCR_HEIGHT - 10 + i, 1);
		else
		    plot(r+q, SCR_HEIGHT - 10 + i,
			 getimagepixel(rocket_bkgnd_image,
				       r+q, SCR_HEIGHT - 10 + i));
	    }
    }
}

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
    bar(0, 0, SCR_WIDTH - 1, SCR_HEIGHT - 1, 0);
    drawimage(rocket_bkgnd_image,0,0,-1);
    bk_after = makeimage(2, 9, 0, 0);
    pickimage(bk_after,SCR_WIDTH / 2, AFTERYTOP);
    if (space) {
	putsprite(players[0].spr,rocket_player_images[0][8],SSX1,SSY);
	putsprite(players[1].spr,rocket_player_images[1][24],SSX2,SSY);
	if (balls) {
	    putsprite(bullets[0][0].spr,rocket_ball_image,SSX1+BALLSTART,SSY);
	    putsprite(bullets[1][0].spr,rocket_ball_image,SSX2-BALLSTART,SSY);
	}
    } else {
	putsprite(players[0].spr,rocket_player_images[0][0],LSX1,LSY);
	putsprite(players[1].spr,rocket_player_images[1][0],LSX2,LSY);
    }

    bar(AFTERX1,AFTERYTOP,AFTERX1-1+MAXAFTER,AFTERYBOT,1);
    bar(AFTERX2,AFTERYTOP,AFTERX2+1-MAXAFTER,AFTERYBOT,1);
    show_scores();
    scr_done();
}

static void invsqgrav(int x, int y, int *dx, int *dy)
{
    int r, rr;
    
    x -= PLANETX << 16;
    y -= PLANETY << 16;
    x = sign(x) * (abs(x) >> 16);
    y = sign(y) * (abs(y) >> 16);
    rr = x*x+y*y;
    r = sqrt(rr) + 0.5;
    *dx -= (x << 22) / (r*rr);
    *dy -= (y << 22) / (r*rr);
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
    if (y < GROUNDHEIGHT && y >= 0 && x >= 0 && x < SCR_WIDTH &&
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
	players[p].x = (p ? SSX2 : SSX1) << 16;
	players[p].y = SSY << 16;
	players[p].a = 32 * p + 16;
	players[p].landed = FALSE;
	players[p].invuln = !planet;
	if (balls) {
	    bullets[p][0].xb = players[p].x + (1-2*p) * (BALLSTART<<16);
	    bullets[p][0].yb = players[p].y;
	    bullets[p][0].dxb = bullets[p][0].dyb = 0;
	}
    } else {
	players[p].x = (p ? LSX2 : LSX1) << 16;
	players[p].y = LSY << 16;
	players[p].a = 0;
	players[p].landed = TRUE;
	players[p].invuln = FALSE;
    }
    players[p].aa = players[p].a >> 1;
    players[p].dx = players[p].dy = 0;
    players[p].after = MAXAFTER;
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
	bulletmaxy = (SCR_HEIGHT-12) << 16;
    else
	bulletmaxy = (SCR_HEIGHT-20) << 16;

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

	/*
	 * Draw the afterburner bars.
	 */
	bar(AFTERX1, AFTERYTOP, AFTERX1 - 1 + players[0].after, AFTERYBOT, 1);
	drawimage(bk_after, AFTERX1 + players[0].after, AFTERYTOP, -1);
	bar(AFTERX2, AFTERYTOP, AFTERX2 + 1 - players[1].after, AFTERYBOT, 1);
	drawimage(bk_after, AFTERX2 - players[1].after, AFTERYTOP, -1);

	/*
	 * Erase and move pixels.
	 */
	for (p = 0; p < MAXPIX; p++) {
	    struct pixel *pix = &pixels[p];

	    if (!pix->free) {
		if (pix->px >= 0 && pix->py >= 0 &&
		    pix->px < (SCR_WIDTH<<16) &&
		    pix->py < ((SCR_HEIGHT-10)<<16))
		    plot(pix->px >> 16, pix->py >> 16,
			 getimagepixel(rocket_bkgnd_image,
				       pix->px >> 16, pix->py >> 16));
		pix->px = pix->xp;
		pix->py = pix->yp;
		pix->xp += pix->dxp;
		pix->yp += pix->dyp;
		if (!space) {
		    pix->dyp += GRAVITY;
		    if (pix->yp > (GROUNDHEIGHT << 16)) {
			pix->yp = (GROUNDHEIGHT << 16);
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
		    pix->xp < (SCR_WIDTH << 16) &&
		    pix->yp < ((SCR_HEIGHT-10) << 16)) {
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
		if (bul->xb > ((SCR_WIDTH-9) << 16)) {
		    bul->xb = (SCR_WIDTH-9) << 16;
		    bul->dxb = 0;
		}
		if (bul->yb > ((SCR_HEIGHT-19) << 16)) {
		    bul->yb = (SCR_HEIGHT-19) << 16;
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
			  bul->xb < 0 || bul->xb >= (SCR_WIDTH << 16))))
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

	    kfire = SDL_JoystickGetButton(joys[p], 3);   /* Circle */
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
		} else
		    thrust = 1;
	    } else {
		pl->agrow++;
		if (pl->agrow >= 4 && pl->after < MAXAFTER) {
		    pl->after++;
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
		if (pl->y >= (GROUNDHEIGHT << 16) &&
		    pl->dy > 0 && !pl->landed) {
		    if (pl->aa == 0 && abs(pl->dx)+pl->dy<=50000) {
			pl->y = GROUNDHEIGHT << 16;
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
		if (pl->x > ((SCR_WIDTH-9) << 16)) {
		    pl->x = (SCR_WIDTH-9) << 16;
		    pl->dx = 0;
		}
		if (pl->y > ((SCR_HEIGHT-19) << 16)) {
		    pl->y = (SCR_HEIGHT-19) << 16;
		    pl->dy = 0;
		}
	    } else {
		if (pl->y < (9 << 16)) {
		    pl->y = 9 << 16;
		    pl->dy = 0;
		}
		while (pl->x < -(9 << 16))
		    pl->x += (SCR_HEIGHT+18) << 16;
		while (pl->x > ((SCR_HEIGHT+9) << 16))
		    pl->x -= (SCR_HEIGHT+18) << 16;
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
		scnew = 1;
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
	    show_scores();
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
