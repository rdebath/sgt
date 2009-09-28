#ifndef MUS_CONTIN_H
#define MUS_CONTIN_H

typedef struct Contin Contin;

/* This structure is the same for all three types of marking. The "down"
 * member is a bit multi-purpose: for a tie it is TRUE if the tie curves
 * downwards, for a hairpin it is TRUE if the hairpin is a diminuendo rather
 * than a crescendo, and for an 8v[ab] it is TRUE for an 8vb. */
struct Contin {
    int exists;			       /* boolean */
    int x, ax;
    int down;			       /* boolean */
};

extern void windup_ties(void);
extern void restart_ties(void);
extern void save_ties(void);
extern void restore_ties(void);

extern void windup_hairpins(void);
extern void restart_hairpins(void);
extern void begin_hairpin(int stave, int type);
extern void end_hairpin(int stave);

extern void windup_eightva(void);
extern void restart_eightva(void);
extern void begin_eightva(int stave, int type);
extern void end_eightva(int stave);

#endif
