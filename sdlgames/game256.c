/*
 * game256.c: a source file implementing functions which mimic my
 * old Borland Pascal Game256 API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sdlstuff.h"
#include "game256.h"

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
	ret.x > SCR_WIDTH || ret.y > SCR_HEIGHT) {
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
    if (ret.x + ret.w > SCR_WIDTH) {
	int clip = ret.x + ret.w - SCR_WIDTH;
	ret.w -= clip;
	ret.imgmod += clip;
    }
    if (ret.y < 0) {
	int clip = -ret.y;
	ret.imgptr += clip * XSIZE(image);
	ret.y += clip;
	ret.h -= clip;
    }
    if (ret.y + ret.h > SCR_HEIGHT) {
	int clip = ret.y + ret.h - SCR_HEIGHT;
	ret.h -= clip;
    }

    /* Now set up scrptr and scrmod. */
    ret.scrptr = scrdata + SCR_WIDTH*XMULT*YMULT * ret.y + XMULT * ret.x;
    ret.scrmod = SCR_WIDTH*XMULT*YMULT - XMULT*ret.w;

    return ret;
}

void drawimage(Image image, int x, int y, int mask)
{
    struct image_stuff stuff = prep_image(image, x, y);
    int ix, iy, iymult;

    if (stuff.scrptr == NULL)
	return;

    for (iy = 0; iy < stuff.h; iy++) {
	for (ix = 0; ix < stuff.w; ix++) {
	    int c = *stuff.imgptr++;
	    if (c != mask) {
#if XMULT==2
		stuff.scrptr[0] = stuff.scrptr[1] = c;
# if YMULT==2
		stuff.scrptr[SCR_WIDTH*XMULT] =
		    stuff.scrptr[SCR_WIDTH*XMULT+1] = c;
# endif
#else
		stuff.scrptr[0] = c;
# if YMULT==2
		stuff.scrptr[SCR_WIDTH*XMULT] = c;
# endif
#endif
	    }
	    stuff.scrptr += XMULT;
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
	    stuff.scrptr += XMULT;
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

Image makeimage(int x, int y, int xoff, int yoff)
{
    Image ret;

    ret = malloc(8+x*y);
    if (ret == NULL)
	abort();

    ret[0] = xoff%256; ret[1] = xoff/256;
    ret[2] = yoff%256; ret[3] = yoff/256;
    ret[4] = x%256; ret[5] = x/256;
    ret[6] = y%256; ret[7] = y/256;

    memset(ret+8, 0, x*y);

    return ret;
}

void bar(int x1, int y1, int x2, int y2, int c)
{
    int x, y;

    if (x2 < x1) { int tmp = x1; x1 = x2; x2 = tmp; }
    if (y2 < y1) { int tmp = y1; y1 = y2; y2 = tmp; }
    if (x2 < 0 || x1 >= SCR_WIDTH || y2 < 0 || y1 >= SCR_HEIGHT) return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > SCR_WIDTH-1) x2 = SCR_WIDTH-1;
    if (y2 > SCR_HEIGHT-1) y2 = SCR_HEIGHT-1;

    for (y = y1; y <= y2; y++) {
	memset(scrdata+SCR_WIDTH*XMULT*YMULT*y+XMULT*x1, c, XMULT*(x2+1-x1));
#if YMULT==2
	memset(scrdata+SCR_WIDTH*XMULT*(YMULT*y+1)+XMULT*x1,c,XMULT*(x2+1-x1));
#endif
    }
}
