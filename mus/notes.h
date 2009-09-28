#ifndef MUS_NOTES_H
#define MUS_NOTES_H

#include "beams.h"

extern void stream_controls (void);
extern void process_notes (void);

extern void flag (int stave, Beam *beam, int *nmin, int *nmax, int *smin,
		  int *smax, int symbol, int height, int above, int outside,
		  int xpos, int axpos);

#endif
