/* header.h  exports from header.c */

#ifndef MUS_HEADER_H
#define MUS_HEADER_H

#include "mus.h"

extern const char *const *const clefs;

extern struct options opts;
extern struct stave *staves;
extern int max_staves, real_max_staves;

void read_header(void);

#endif
