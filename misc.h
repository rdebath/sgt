#ifndef MUS_MISC_H
#define MUS_MISC_H

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

int sparse(const char *s,const char *const *sa);

int lcm (int a, int b);

int parse_time_sig (char *buf);
int parse_key_sig (char *word1, char *word2);

int bar_len (int timesig);

/* for the second argument to stave_y */
#define STAVE_TOP	0
#define STAVE_MIDDLE	1
#define STAVE_BOTTOM	2

int stave_y (int stave, int posn) __attribute__((const));

#endif
