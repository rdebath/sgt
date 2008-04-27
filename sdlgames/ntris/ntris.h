/*
 * ntris.h - interface between ntris.c and one of its front ends.
 */

#ifndef NTRIS_NTRIS_H
#define NTRIS_NTRIS_H

struct tetris_instance;
struct frontend_instance;

/*
 * The actual game logic. You call `ntris_init' to set up an
 * instance of an Ntris game (passing NULL as the shapeset if you
 * just want the default). This initialises most of the game; next
 * you call `ntris_newshape', which sets up the first shape to
 * start dropping. Now you can call the various `try' functions in
 * response to player controls, and call `drop' either on a timer
 * or in response to the player pressing a drop button.
 * 
 * `frontend_instance' is used for the block-drawing callback below.
 * 
 * All the `try' functions return TRUE iff the attempt to move the
 * piece was successful (so a front end could, for instance, choose
 * what noise to make).
 * 
 * `ntris_softdrop' is a one-space soft drop, and returns TRUE if
 * it caused the piece to finish its dropping (in which case line
 * checking and an arena redraw will also have been done). Of
 * course, after `ntris_softdrop' returns TRUE, you will almost
 * certainly want to call `ntris_newshape' again.
 * 
 * `ntris_harddrop' is an all-the-way instantaneous drop, and
 * therefore it is always successful and returns nothing.
 * 
 * `ntris_newshape' returns FALSE if there was no space to place the
 * new piece - which is of course the game-over condition.
 * 
 * `ntris_try_hold' moves a piece into the hold cell, or swaps the
 * piece in the hold cell with the current falling piece. It can
 * never cause game-over, because it will fail rather than do
 * anything lethal; like the other `try' functions, it will return
 * TRUE if successful.
 */
struct ntris_instance *ntris_init(struct frontend_instance *fe,
				  int width, int height, int shapeset);
int ntris_newshape(struct ntris_instance *inst);
int ntris_try_left(struct ntris_instance *inst);
int ntris_try_right(struct ntris_instance *inst);
int ntris_try_anticlock(struct ntris_instance *inst);
int ntris_try_clockwise(struct ntris_instance *inst);
int ntris_try_reflect(struct ntris_instance *inst);
int ntris_try_hold(struct ntris_instance *inst);
int ntris_softdrop(struct ntris_instance *inst);
void ntris_harddrop(struct ntris_instance *inst);

/* Shapeset enum used in ntris_init. */
enum { TETRIS_SHAPES, PENTRIS_SHAPES, NSHAPESETS };

/*
 * `get_score' returns some aspect of the score in the current
 * game. There are a variety of score counters which count a
 * variety of different conditions, and the `which' parameter
 * requests a particular one.
 */
enum {
    SCORE_LINES,		       /* basic line count */
    SCORE_SINGLE,		       /* lines cleared one at a time */
    SCORE_DOUBLE,		       /* lines cleared two at a time */
    SCORE_TRIPLE,		       /* lines cleared three at a time */
    SCORE_TETRIS,		       /* lines cleared four at a time */
    SCORE_PENTRIS,		       /* lines cleared five at a time */
    NUM_SCORES			       /* (used internally in ntris.c) */
};
int ntris_get_score(struct ntris_instance *inst, int which);

/*
 * This query function tells the front end the largest dimension of
 * any shape in the shapeset, for purposes such as allocating
 * enough screen space for a hold cell or next-piece display.
 */
int ntris_shape_maxsize(struct ntris_instance *inst);

/*
 * The game logic will call back to this function in response to
 * most of the functions above. It's expected to draw a rectangular
 * block on the screen at the given coordinates (measured in block
 * widths), in the given colour. `type' specifies the borders on
 * the block, as a combination of the flags given below.
 * 
 * `area' indicates which of several different drawing areas the
 * block is to be drawn in. The possible values are enumerated below.
 */
void ntris_fe_block(struct frontend_instance *inst, int area,
		    int x, int y, int col, int type);

enum {
    AREA_MAIN,			       /* the main playing area */
    AREA_HOLD,			       /* the holding cell */
    AREA_NEXT,			       /* the next-piece display */
    /* Further future-piece displays appear after this one. */
};

/*
 * These flags indicate the highlighting required on each block
 * that forms part of a shape or the playing area.
 * 
 * The way these flags are actually computed is by checking whether
 * the block in question has a neighbour in each of the eight
 * surrounding spaces. So FLAG_TRCORNER, for example, is present in
 * any block whose upper-left neighbour is not part of the same
 * shape; and FLAG_TEDGE is present in any block whose _upper_
 * neighbour is not part of the same shape.
 * 
 * The recommended actual highlights that should appear on the
 * blocks as a result are:
 *
 *  - each corner flag causes a nick in the appropriate corner. At
 *    TL, that's a light square; at BR a dark square. In the other
 *    two corners it's a half-light half-dark square (possibly with
 *    a diagonal of medium pixels to prevent asymmetry, although
 *    this may work badly if the highlight is only one pixel wide).
 *
 *  - the effect of an edge flag is dependent on whether the
 *    _adjacent_ edges exist too. FLAG_LEDGE, in the absence of
 *    FLAG_TEDGE or FLAG_BEDGE, should be a full light rectangle
 *    along the whole of the left edge. But if FLAG_BEDGE is
 *    present as well, it changes - in this case it must be
 *    bevelled at the bottom so as not to overwrite the shadow
 *    below it.
 *     + Of course each edge only actually needs to watch out for
 *       _one_ adjacent edge, namely the one in the other colour.
 *       LEDGE and TEDGE can cheerfully ignore each other, since
 *       either one causes the whole top-left corner to be light
 *       and it doesn't matter if they overwrite each other.
 *
 *  - edge flags override corner flags. If the block has
 *    FLAG_LEDGE, we _do not care_ whether it has FLAG_TLCORNER,
 *    FLAG_BLCORNER, both or neither. Ever.
 *
 * In other words, we can diagram the block as follows:
 *
 * aaattttdde
 * aaattttdef
 * aaatttteff
 * lllxxxxrrr
 * lllxxxxrrr
 * lllxxxxrrr
 * lllxxxxrrr
 * gghbbbbccc
 * ghibbbbccc
 * hiibbbbccc
 *
 * and then we can allocate colours to the various letters as follows:
 *
 *  - a is light if any of LEDGE, TEDGE or TLCORNER, and medium
 *    otherwise.
 *  - c is dark if any of REDGE, BEDGE or BRCORNER, and medium
 *    otherwise.
 *  - l is light if LEDGE, medium otherwise.
 *  - t is light if TEDGE, medium otherwise.
 *  - b is dark if BEDGE, medium otherwise.
 *  - r is dark if REDGE, medium otherwise.
 *  - d is light if TEDGE, otherwise dark if REDGE or TRCORNER,
 *    otherwise medium.
 *  - f is dark if REDGE, otherwise light if TEDGE or TRCORNER,
 *    otherwise medium.
 *  - g is light if LEDGE, otherwise dark if BEDGE or BLCORNER,
 *    otherwise medium.
 *  - i is dark if BEDGE, otherwise light if LEDGE or BLCORNER,
 *    otherwise medium.
 *  - e is equal to d and f if d and f are themselves equal to each
 *    other (i.e. if exactly one of TEDGE and REDGE). Failing that,
 *    it's medium, _unless_ we're doing asymmetric highlighting, in
 *    which case it's chosen to be one of d and f. Unsure which.
 *    Perhaps top-left-priority so that e=d; or perhaps light wins,
 *    so that e is light in this case.
 *  - h, similarly, is equal to g and i if they are themselves
 *    equal (exactly one of LEDGE and BEDGE), else it's medium or
 *    chosen from g and i as above.
 *  - x is always medium.
 */
#define FLAG_REDGE    0x01	       /* block border at right edge */
#define FLAG_LEDGE    0x02	       /* block border at left edge */
#define FLAG_TEDGE    0x04	       /* block border at top edge */
#define FLAG_BEDGE    0x08	       /* block border at bottom edge */
#define FLAG_TRCORNER 0x10	       /* nick in top right corner */
#define FLAG_TLCORNER 0x20	       /* nick in top left corner */
#define FLAG_BRCORNER 0x40	       /* nick in bottom right corner */
#define FLAG_BLCORNER 0x80	       /* nick in bottom left corner */

#endif /* NTRIS_NTRIS_H */
