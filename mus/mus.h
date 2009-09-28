#ifndef MUS_MUS_H
#define MUS_MUS_H

#include "melody.h"
#include "contin.h"

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define MUS_VERSION	120	       /* file format version number */

/* maximum pitch value that can crop up, in +ve or -ve direction */
#define MAXPITCH	69

/* Pitches go from -MAXPITCH to +MAXPITCH. Another common internal
 * representation is from 0 to 2*MAXPITCH. That means Middle C is represented
 * by MAXPITCH, which is not zero (mod 7). So if you need C to be 0 mod 7,
 * then add PMOD to one of these pitches. */
#define PMOD		(7-(MAXPITCH)%7)

/* special values for opts.timesig below */
#define TIME_C		0x1FF	       /* standard (4/4) time */
#define TIME_CBAR	0x2FF	       /* alla breve (2/2) time */

/* values for options.barline_split below */
#define SPL_NEVER	0	       /* bar lines don't split */
#define SPL_BRACKET	1	       /* bar lines split at bracket level 0 */
#define SPL_NONBRACE	2	       /* bar lines split except in braces */
#define SPL_ALWAYS	3	       /* bar lines always split */

struct options {
    int timesig;		       /* time signature: 0x504 means 5/4 */
    int keysig;			       /* index into key-signature array */
    int barline_split;		       /* bar line splitting */
    int bar_numbers;		       /* boolean: 1=on, 0=off */
    int german_notes;		       /* boolean: for B-flat / B / H */
    int notes_as_key;		       /* default behaviour without [x#b] */
    int manual_breaks;		       /* disable line-breaking logic */
    double max_ax;		       /* maximum distance per AX unit */
    double ysquash;		       /* factor to compress y spacing */
};

/* values for staves[i].clef below */
#define CLEF_TREBLE	0
#define CLEF_SOPRANO	1
#define CLEF_MEZZSOPR	2	       /* mezzo-soprano */
#define CLEF_ALTO	3
#define CLEF_TENOR	4
#define CLEF_BARITONE	5
#define CLEF_BASS	6
#define CLEF_LINE	7	       /* single line, for drum parts */

/* the pitch (in diatonic notes above Middle C) of the centre line of a
 * stave bearing a given clef (not CLEF_LINE of course) */
#define centre_pitch(clef) (6-2*(clef))

/* Explanation of bracket levels (defined below) is probably required:
 *   "Bracket level" of a stave is stored in staves[i].bkt, and denotes
 * the number of nested brackets that stave is in. "Bracket level" also
 * applies to the space between stave i and stave i+1, stored in
 * staves[i].bkt1. Bracket level 0 denotes no brackets or braces. Bracket
 * level -1 denotes no brackets, but a brace.
 *   Hence placing a bracket between staves i and j inclusive increases
 * staves[k].bkt by 1 for all i<=k<=j, and increases staves[k].bkt1 by 1
 * for all i<=k<j. Similarly placing a brace on staves i and i+1 sets
 * staves[i].bkt = staves[i].bkt1 = staves[i+1].bkt = -1. */

struct stave {
    int clef;			       /* clefs given above */
    int exists;			       /* for use in parsing the header */
    int bkt, bkt1;		       /* bracket levels */
    int ditto;			       /* if we want a ditto mark, this bar */
    char *name;
};

/* values for wrk.repeat_ending below */
#define REP_ENDING_NONE 0
#define REP_ENDING_1ST  1
#define REP_ENDING_2ND  2

/* Working variables while processing. */
extern struct working {
    int repeat_ending;		       /* repeat ending on current bar */
    char *bar_right;		       /* text above and on right of bar */
    int same_bar;		       /* does this bar have same # as prev */
    int bars_rest;		       /* current bar is a multi-bar rest */
    int factor;			       /* current unit of time measurement */
    MelodyLine **mlines;	       /* the actual melody lines */
    int mline_count;		       /* and how many of 'em we have */
    int mline_size;		       /* also how many the array can take */
    MelodyEvent **mline_ptr;	       /* how far we are thru each line */
    int time_pos;		       /* position thru the bar, in time */
    int bar_min_width;		       /* this is nonzero if DITTO is used */
    char (*notes_used)[MAXPITCH*2+1];  /* 2-d array: [stave][pitch] */
    Contin (*ties)[MAXPITCH*2+1];      /* the current set of ties */
    char (*saveties)[MAXPITCH*2+1];    /* saved set of ties, if any */
    char (*accidental)[MAXPITCH*2+1];  /* stores the accidentals */
    Contin *hairpins;		       /* cresc/dim currently in effect */
    Contin *eightva;		       /* 8v[ab] markers */
} wrk;				       /* declared in drawing.c */

extern int xmax, ymax, lineheight, break_now;
extern int lmarg, rmarg, tmarg, bmarg; /* margins */
extern int stave_min, stave_max;       /* stave range actually being drawn */

extern void fatal(int line, int col, char *fmt, ...)
	__attribute__((format (printf, 3, 4)));
extern void error(int line, int col, char *fmt, ...)
	__attribute__((format (printf, 3, 4)));

extern void *mmalloc(size_t size);     /* our version of malloc() */
extern void *mrealloc(void *ptr, size_t size);   /* and realloc() */
extern void mfree(void *ptr);	       /* and last but not least, free() */
extern char *dupstr(char *ptr);	       /* strdup isn't standard: supply it */

#endif
