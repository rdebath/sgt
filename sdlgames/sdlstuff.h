/*
 * sdlstuff.h: interface to sdlstuff.c. Also includes SDL.h.
 */

#ifndef SDLSTUFF_H
#define SDLSTUFF_H

#include <SDL/SDL.h>

extern SDL_Surface *screen;

#define scrdata ((Uint8 *)screen->pixels)

long long bigclock(void);
void cleanup(void);
int setup(void);
void vsync(void);

#define JOY_THRESHOLD 4096

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

#define sign(x) ( (x) < 0 ? -1 : (x) > 0 ? +1 : 0 )

#ifdef PS2
/* On the PS2 the real resolution is 640x240, so each pixel is two. */
#define plot(x,y,c) ( scrdata[(y)*640+(x)*2] = scrdata[(y)*640+(x)*2+1] = c )
#define pixel(x,y) ( scrdata[(y)*640+(x)*2] )
#define plotc(x,y,c) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : plot(x,y,c) )
#define pixelc(x,y) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : pixel(x,y) )
#define XMULT 2
#define YMULT 1
#else /* PS2 */
/* Elsewhere we use 640x480, so each pixel is four. */
#define plot(x,y,c) ( scrdata[(y)*1280+(x)*2] = \
		      scrdata[(y)*1280+(x)*2+1] = \
		      scrdata[(y)*1280+(x)*2+640] = \
		      scrdata[(y)*1280+(x)*2+641] = c )
#define pixel(x,y) ( scrdata[(y)*1280+(x)*2] )
#define plotc(x,y,c) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : plot(x,y,c) )
#define pixelc(x,y) ( (x)<0||(x)>319||(y)<0||(y)>239 ? 0 : pixel(x,y) )
#define XMULT 2
#define YMULT 2
#endif /* PS2 */

#define SCR_WIDTH 320
#define SCR_HEIGHT 240

#endif
