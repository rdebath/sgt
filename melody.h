/* melody.h  header for melody.c */

#ifndef MUS_MELODY_H
#define MUS_MELODY_H

#include <limits.h>
#include "beams.h"
#include "slurs.h"

/* possible values for NoteHead.pitch */
#define PITCH_NONE (INT_MAX)
#define PITCH_REST (INT_MIN)

/* flags for NoteHead.flags */
#define NH_ARTHARM	1
#define NH_TIE		2

/* this enumeration must be declared separately because it is used twice */
enum Ntuplet {
    NTUP_START = -1, NTUP_NONE = 0
};

/* first declare all the structure tags as typedefs, to prevent defining
 * of structures only within an inner scope */
typedef struct MelodyLine MelodyLine;
typedef struct MelodyEvent MelodyEvent;
typedef struct NoteStem NoteStem;
typedef struct NoteHead NoteHead;
typedef struct NoteFlags NoteFlags;
typedef struct StreamControl StreamControl;

struct MelodyLine {
    MelodyEvent *head, *tail;
    int stave_number;
    int stems_force, stems_down;
    int time_pos;
    Beam *beam;			       /* if we're currently beaming */
    Slur *slur;			       /* if we're currently slurring */
    int ntup_x, ntup_ax, max_y, min_y; /* for ntuplets */
    enum Ntuplet ntuplet;	       /* also for ntuplets */
    int last_stem_down, just_done;     /* for processing */
    char *lyrics;
};

struct NoteStem {
    int duration;		       /* in 1/factor hemidemisemiquavers */
    enum {
	HEAD_BREVE, HEAD_SEMIBREVE, HEAD_MINIM, HEAD_CROTCHET,
	HEAD_AC, HEAD_AP, HEAD_ARTHARM
    } heads_type;
    enum {
	STEM_NONE, STEM_PLAIN, STEM_1TAIL, STEM_2TAIL, STEM_3TAIL, STEM_4TAIL
    } stem_type;
    enum {
	REST_B, REST_SB, REST_M, REST_C, REST_Q,
	REST_SQ, REST_DQ, REST_HQ, REST_INVALID
    } rest_type;
    int stem_down;		       /* gets filled in by preprocessor */
    int dots;
    enum Ntuplet ntuplet;	       /* gets filled in by preprocessor */
    NoteHead *hhead, *htail;	       /* pitches */
    NoteFlags *fhead, *ftail;	       /* flags */
};

struct NoteHead {
    int pitch;			       /* diatonic notes above Middle C, or
					* possibly PITCH_NONE or PITCH_REST */
    enum {
	DOUBLE_SHARP, SHARP,
	NATURAL,
	FLAT, DOUBLE_FLAT,
	NO_ACCIDENTAL
    } accidental;
    int flags;
    NoteHead *next, *prev;
};

struct NoteFlags {
    enum {
	NF_STACCATO, NF_LEGATO, NF_STACCATISSIMO,
	NF_SFORZANDO, NF_DOWNBOW, NF_UPBOW, NF_STOPPING,
	NF_FERMATA, NF_NATHARM, NF_ACCENT,
	NF_TRILL, NF_TURN, NF_INVTURN, NF_MORDENT, NF_INVMORDENT,
	NF_SMALLSHARP, NF_SMALLNATURAL, NF_SMALLFLAT,
	NF_TREMOLO, NF_FINGERING, NF_ARPEGGIO
    } type;
    int extra;			       /* TREMOLO (1-4) or FINGERING (0-5) */
    NoteFlags *next, *prev;
};

struct StreamControl {
    enum {
	SC_ZERONOTE,		       /* the "z" directive */
	SC_BEAMSTART, SC_BEAMBREAK, SC_BEAMSTOP,
	SC_SLURSTART, SC_SLURSTOP,
	SC_FORCE_UP, SC_FORCE_DOWN,
	SC_NTUPLET, SC_NTUPLET_END,
	SC_8VA, SC_8VB,
	SC_BREATH,		       /* flute breathing mark */
	SC_GENPAUSE,		       /* general pause */
	SC_TURN,		       /* turn *between* two notes */
	SC_INVTURN,		       /* and an inverted one */
	SC_DYNAMIC,
	SC_TEXT_ABOVE, SC_TEXT_BELOW,
	SC_LYRICS,
	SC_CRESC, SC_DIM	       /* crescendo / diminuendo */
    } type;
    char *text;			       /* for dynamics, text, lyrics */
    int ntuplet_num;		       /* e.g. 2 for a triplet (2/3 length) */
    int ntuplet_denom;		       /* e.g. 3 for a triplet */
};

struct MelodyEvent {
    enum {
	EV_STREAM,		       /* means MelodyEvent.u.s is in use */
	EV_NOTE,		       /* means MelodyEvent.u.n is in use */
	EV_SGARBAGE,		       /* preprocessor generates these */
	EV_NGARBAGE		       /* preprocessor generates these */
    } type;
    int line, col;		       /* for error reporting */
    MelodyEvent *next;
    MelodyEvent *prev;
    union {
	NoteStem n;
	StreamControl s;
    } u;
};

extern MelodyLine *readML(void);

#endif /* MUS_MELODY_H */
