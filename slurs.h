#ifndef MUS_SLURS_H
#define MUS_SLURS_H

typedef struct Slur Slur;
typedef struct SlurPoint SlurPoint;

struct Slur {
    SlurPoint *head, *tail, *mark;
    int first_down, first_stem, last_down, last_stem;
    int centre_y;
};

struct SlurPoint {
    SlurPoint *next;
    int x, ax, left, right, ymin, ymax;
};

#include "beams.h"

extern Slur *new_slur(int stave);
extern void record_slur (Slur *slur, Beam *beam, int x, int ax,
			 int left, int right, int ymin, int ymax,
			 int down, int stem, int is_tail);
extern void slur_shift_mark (int rx);

extern void save_slurs(void);
extern void restore_slurs(void);
extern void windup_slurs(void);
extern void restart_slurs(void);

extern void draw_slurs (double hdsq);

#endif
