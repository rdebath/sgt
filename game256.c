/*
 * game256.c: a source file implementing functions which mimic my
 * old Borland Pascal Game256 API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "sdlstuff.h"
#include "game256.h"

#define SCRWIDTH 320		       /* we pretend it's 320 not 640 */
#define SCRHEIGHT 240

Sprite makesprite(int max_width, int max_height, int mask)
{
    Sprite ret;
    
    ret = malloc(sizeof(*ret));
    if (!ret) { fprintf(stderr, "out of memory!\n"); exit(1); }

    ret->back_size = max_width * max_height + 8;
    ret->background = malloc(ret->back_size);
    if (!ret->background) { fprintf(stderr, "out of memory!\n"); exit(1); }

    ret->mask = mask;
    ret->drawn = 0;

    return ret;
}

void freesprite(Sprite spr)
{
    free(spr->background);
    free(spr);
}

void putsprite(Sprite spr, Image image, int x, int y)
{
    assert(XSIZE(image) * YSIZE(image) + 8 <= spr->back_size);

    erasesprite(spr);

    memcpy(spr->background, image, 8);
    pickimage(spr->background, x, y);
    drawimage(image, x, y, spr->mask);
    spr->x = x;
    spr->y = y;
    spr->drawn = 1;
}

void erasesprite(Sprite spr)
{
    if (!spr->drawn)
	return;

    drawimage(spr->background, spr->x, spr->y, -1);
    spr->drawn = 0;
}

struct image_stuff {
    int x, y;
    int w, h;
    unsigned char *scrptr, *imgptr;
    int scrmod, imgmod;		       /* add to each ptr between scans */
};

static struct image_stuff prep_image(Image image, int x, int y)
{
    struct image_stuff ret;
    int realw;

    ret.x = x - XOFFSET(image);
    ret.y = y - YOFFSET(image);

    ret.w = XSIZE(image);
    ret.h = YSIZE(image);

    ret.imgmod = 0;
    ret.imgptr = IMGDATA(image);

    /* Clip hard if the image is right off the screen. */
    if (ret.x+ret.w <= 0 || ret.y+ret.h <= 0 ||
	ret.x > SCRWIDTH || ret.y > SCRHEIGHT) {
	ret.scrptr = NULL;
	return ret;
    }

    /* Clip by cutting bits off the image if it overlaps screen boundaries. */
    if (ret.x < 0) {
	int clip = -ret.x;
	ret.w -= clip;
	ret.imgmod += clip;
	ret.imgptr += clip;
	ret.x += clip;
    }
    if (ret.x + ret.w > SCRWIDTH) {
	int clip = ret.x + ret.w - SCRWIDTH;
	ret.w -= clip;
	ret.imgmod += clip;
    }
    if (ret.y < 0) {
	int clip = -ret.y;
	ret.imgptr += clip * XSIZE(image);
	ret.y += clip;
	ret.h -= clip;
    }
    if (ret.y + ret.h > SCRHEIGHT) {
	int clip = ret.y + ret.h - SCRHEIGHT;
	ret.h -= clip;
    }

    /* Now set up scrptr and scrmod. */
    ret.scrptr = scrdata + 640 * ret.y + 2 * ret.x;
    ret.scrmod = 640 - 2*ret.w;

    return ret;
}

void drawimage(Image image, int x, int y, int mask)
{
    struct image_stuff stuff = prep_image(image, x, y);
    int ix, iy;

    if (stuff.scrptr == NULL)
	return;

    for (iy = 0; iy < stuff.h; iy++) {
	for (ix = 0; ix < stuff.w; ix++) {
	    int c = *stuff.imgptr++;
	    if (c != mask)
		stuff.scrptr[0] = stuff.scrptr[1] = c;
	    stuff.scrptr += 2;
	}
	stuff.scrptr += stuff.scrmod;
	stuff.imgptr += stuff.imgmod;
    }
}

void pickimage(Image image, int x, int y)
{
    struct image_stuff stuff = prep_image(image, x, y);
    int ix, iy;

    if (stuff.scrptr == NULL)
	return;

    for (iy = 0; iy < stuff.h; iy++) {
	for (ix = 0; ix < stuff.w; ix++) {
	    *stuff.imgptr++ = *stuff.scrptr;
	    stuff.scrptr += 2;
	}
	stuff.scrptr += stuff.scrmod;
	stuff.imgptr += stuff.imgmod;
    }
}

void imagepixel(Image image, int x, int y, int colour)
{
    x += XOFFSET(image);
    y += YOFFSET(image);
    if (x >= 0 && x < XSIZE(image) && y >= 0 && y < YSIZE(image))
	IMGDATA(image)[y*XSIZE(image)+x] = colour;
}

int getimagepixel(Image image, int x, int y)
{
    x += XOFFSET(image);
    y += YOFFSET(image);
    if (x >= 0 && x < XSIZE(image) && y >= 0 && y < YSIZE(image))
	return IMGDATA(image)[y*XSIZE(image)+x];
    else
	return -1;		       /* out of bounds */
}

void imageonimage(Image canvas, Image brush, int x, int y, int mask)
{
    int ix, iy;
    int ox = XOFFSET(brush), oy = YOFFSET(brush);
    unsigned char *p = IMGDATA(brush);

    for (iy = 0; iy < YSIZE(brush); iy++)
	for (ix = 0; ix < XSIZE(brush); ix++) {
	    int colour = *p++;
	    if (colour != mask)
		imagepixel(canvas, x+ix-ox, y+iy-oy, colour);
	}
}

static long long frame_end;

void initframe(int microseconds)
{
    frame_end = bigclock() + microseconds;
}

void endframe(void)
{
    long long now = bigclock();
    if (now < frame_end)
	usleep(frame_end - now);
}
