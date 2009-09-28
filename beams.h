#ifndef MUS_BEAMS_H
#define MUS_BEAMS_H

typedef struct Beam Beam;
typedef struct BeamEntry BeamEntry;
typedef struct DeferFlag DeferFlag;

#include "slurs.h"

struct Beam {
    int down;			       /* TRUE if stems go down */
    int minlen;			       /* stems' minimum length */
    int stave;			       /* stave number */
    int line, col;		       /* for error reporting */
    BeamEntry *head, *tail;
    BeamEntry *mark;
};

struct BeamEntry {
    BeamEntry *next;
    int is_break;		       /* TRUE if this is a beam break */
    int x, y, ax;
    int tails;
    DeferFlag *fhead, *ftail;	       /* deferred flag marks */
    SlurPoint *sptr;		       /* deferred slur coordinates */
};

struct DeferFlag {
    int sym, ht, above, outside, xpos, axpos;
    DeferFlag *next;
};

extern Beam *new_beam(int stave);
extern void beam_break (Beam *beam);

extern void add_to_beam (Beam *beam, int x, int ax, int y, int tails);
extern void deferred_flag (Beam *beam, int symbol, int height,
			   int above, int outside, int xpos, int axpos);
extern void beam_shift_mark (int rx);

extern void draw_beams (double hdsq);

#endif
