/*
 * ntris.h - interface between ntris.c and one of its front ends.
 */

#ifndef NTRIS_NTRIS_H
#define NTRIS_NTRIS_H

struct tetris_instance;
struct frontend_instance;

extern char *tetris_shapes[7+1];
extern char *pentris_shapes[18+1];

/*
 * The actual game logic. You call `init_game' to set up an
 * instance of an Ntris game (passing NULL as the shapeset if you
 * just want the default). This initialises most of the game; next
 * you call `init_shape', which sets up the first shape to start
 * dropping. Now you can call the various `try' functions in
 * response to player controls, and call `drop' either on a timer
 * or in response to the player pressing a drop button.
 * 
 * `frontend_instance' is used for the block-drawing callback below.
 * 
 * All the `try' functions return TRUE iff the attempt to move the
 * piece was successful (so a front end could, for instance, choose
 * what noise to make).
 * 
 * `drop' is a one-space soft drop, and returns TRUE if it caused
 * the piece to finish its dropping (in which case line checking
 * and an arena redraw will also have been done). Of course, after
 * `drop' returns TRUE, you will almost certainly want to call
 * `init_shape' again.
 * 
 * `init_shape' returns FALSE if there was no space to place the
 * new piece - which is of course the game-over condition.
 */
struct ntris_instance *init_game(struct frontend_instance *fe,
				 int width, int height, char **shapeset);
int init_shape(struct ntris_instance *inst);
int try_move_left(struct ntris_instance *inst);
int try_move_right(struct ntris_instance *inst);
int try_anticlock(struct ntris_instance *inst);
int try_clockwise(struct ntris_instance *inst);
int try_reflect(struct ntris_instance *inst);
int drop(struct ntris_instance *inst);

/*
 * The game logic will call back to this function in response to
 * most of the functions above. It's expected to draw a rectangular
 * block on the screen at the given coordinates (measured in block
 * widths), in the given colour. `type' specifies the borders on
 * the block, as a combination of the flags given below.
 */
void block(struct frontend_instance *inst, int x, int y, int col, int type);

#define FLAG_REDGE    0x01	       /* block border at right edge */
#define FLAG_LEDGE    0x02	       /* block border at left edge */
#define FLAG_TEDGE    0x04	       /* block border at top edge */
#define FLAG_BEDGE    0x08	       /* block border at bottom edge */
#define FLAG_TRCORNER 0x10	       /* nick in top right corner */
#define FLAG_TLCORNER 0x20	       /* nick in top left corner */
#define FLAG_BRCORNER 0x40	       /* nick in bottom right corner */
#define FLAG_BLCORNER 0x80	       /* nick in bottom left corner */

#endif /* NTRIS_NTRIS_H */
