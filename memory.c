/*
 * enigma/memory.c - memory management routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "enigma.h"

char *fatal_error_string;
jmp_buf fatal_error_jmp_buf;

static void fatal(char *string) {
    fatal_error_string = string;
    longjmp(fatal_error_jmp_buf, 1);
}

void *smalloc(size_t size) {
    void *p = malloc(size);
    if (!p)
	fatal("Out of memory");
    return p;
}

void *srealloc(void *q, size_t size) {
    void *p = realloc(q, size);
    if (!p)
	fatal("Out of memory");
    return p;
}

char *dupstr(char *str) {
    char *p = (char *)smalloc(1+strlen(str));
    strcpy(p, str);
    return p;
}

#define mknew(type) ( (type *)smalloc(sizeof(type)) )
#define mknewn(type, n) ( (type *)smalloc(sizeof(type) * n) )
#define renewn(p, type, n) ( (type *)srealloc(p, sizeof(type) * n) )

gamestate *gamestate_new(int width, int height) {
    gamestate *p;

    p = mknew(gamestate);
    p->leveldata = mknewn(char, width * height);
    p->width = width;
    p->height = height;

    return p;
}

gamestate *gamestate_copy(gamestate *state) {
    gamestate *ret;

    ret = gamestate_new(state->width, state->height);
    ret->status = state->status;
    memcpy(ret->leveldata, state->leveldata, ret->width * ret->height);
    ret->player_x = state->player_x;
    ret->player_y = state->player_y;
    ret->gold_got = state->gold_got;
    ret->gold_total = state->gold_total;
    ret->movenum = state->movenum;

    return ret;
}

void gamestate_free(gamestate *p) {
    if (p) {
	if (p->leveldata) free(p->leveldata);
	free(p);
    }
}

level *level_new(void) {
    level *p;

    p = mknew(level);
    p->leveldata = NULL;

    return p;
}

void level_setsize(level *p, int width, int height) {
    assert(p != NULL);
    if (p) {
	p->leveldata = mknewn(char, width * height);
	p->width = width;
	p->height = height;
    }
}

void level_free(level *p) {
    if (p) {
	if (p->leveldata) free(p->leveldata);
	free(p);
    }
}

levelset *levelset_new(void) {
    levelset *p;

    p = mknew(levelset);
    p->levels = NULL;
    p->nlevels = 0;

    return p;
}

void levelset_nlevels(levelset *p, int n) {
    p->levels = renewn(p->levels, level *, n);
    while (p->nlevels < n)
	p->levels[p->nlevels++] = NULL;
}

void levelset_free(levelset *p) {
    if (p) {
	if (p->levels) {
	    int i;
	    for (i = 0; i < p->nlevels; i++)
		level_free(p->levels[i]);
	}
	free(p);
    }
}
