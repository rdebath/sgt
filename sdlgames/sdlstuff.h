/*
 * sdlstuff.h: interface to sdlstuff.c. Also includes SDL.h.
 */

#ifndef SDLSTUFF_H
#define SDLSTUFF_H

#include <SDL/SDL.h>

SDL_Surface *screen;

#define scrdata ((Uint8 *)screen->pixels)

void cleanup(void);
int setup(void);
int vsync(void);

#endif
