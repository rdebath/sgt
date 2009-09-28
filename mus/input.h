#ifndef MUS_INPUT_H
#define MUS_INPUT_H

#include <stdio.h>

extern int line_no, col_no;
extern FILE *in_fp;

extern int nd_getchr(void);
extern int getchr(void);
extern char *getword(char **buf);
extern void ungetword(char **word);

#endif
