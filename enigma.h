#ifndef ENIGMA_ENIGMA_H
#define ENIGMA_ENIGMA_H

#include <setjmp.h>
#include "config.h"

/* ----------------------------------------------------------------------
 * Standard useful macros.
 */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define lenof(array) ( sizeof((array)) / sizeof(*(array)) )

/* ----------------------------------------------------------------------
 * Data structures.
 */

/*
 * First, a structure describing the state of a game between two
 * moves. (At intermediate positions within one move, there is more
 * state than this: moving objects and exposed cells are held in
 * lists. However, all of that must be cleaned up and finished
 * before the move ends, so between moves these lists are not
 * visible.)
 */
typedef struct {
    enum { PLAYING, COMPLETED, DIED, BROKEN } status;
    char *leveldata;
    int width, height;
    int player_x, player_y;
    int gold_got, gold_total;
    int movenum;
} gamestate;

/*
 * A structure describing a level.
 */
typedef struct {
    char *title;
    char *leveldata;
    int width, height;
} level;

/*
 * A structure describing a complete set of levels.
 */
typedef struct {
    char *title;
    int nlevels;
    level **levels;
} levelset;

/* ----------------------------------------------------------------------
 * Function prototypes.
 */

/*
 * Memory allocation routines, in memory.c. Create and destroy a
 * gamestate, a levelset, and a level.
 * 
 * All of these can be assumed to succeed. They will bomb out with
 * longjmp if they fail.
 * 
 * level_new and levelset_new create no initial data buffer within
 * the structures. These must be set later using level_setsize and
 * levelset_nlevels.
 */
gamestate *gamestate_new(int width, int height);
void gamestate_free(gamestate *);

level *level_new(void);
void level_setsize(level *, int width, int height);
void level_free(level *);

levelset *levelset_new(void);
void levelset_nlevels(levelset *, int n);
void levelset_free(levelset *);

void *smalloc(size_t size);
void *srealloc(void *q, size_t size);
char *dupstr(char *str);

/*
 * From engine.c, the make_move() function. Accepts as input a move
 * ('h', 'j', 'k' or 'l') and a gamestate. Returns a modified
 * gamestate to reflect the consequences of the move. Also
 * init_game(), which sets up a gamestate from a level.
 */
gamestate *make_move (gamestate *, char);
gamestate *init_game (level *lev);

/*
 * From levelfile.c, the load_levels() function. Given the filename
 * stem of a level set, will load the level set into memory and
 * return a levelset structure.
 */
levelset *levelset_load(char *);

/* ----------------------------------------------------------------------
 * Global data.
 */

/*
 * A means of reporting memory allocation errors or other
 * fatalities. Will return to the main program just before
 * finalising curses, so that fatal errors don't leave the terminal
 * in a silly state. Also provided is a char * variable to leave an
 * error message in before longjmping.
 */
extern char *fatal_error_string;
extern jmp_buf fatal_error_jmp_buf;

#endif /* ENIGMA_ENIGMA_H */
