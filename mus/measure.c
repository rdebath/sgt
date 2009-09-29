#include "measure.h"

const int tiny_widths[] = {
    67,				       /* dot on dotted notes */
    351,			       /* breve */
    247,			       /* semibreve */
    178,			       /* minim */
    178,			       /* crotchet */
    293,			       /* quaver */
    293,			       /* semiquaver */
    293,			       /* demisemiquaver */
    293,			       /* hemidemisemiquaver */
    216,			       /* f */
    460,			       /* m */
    365,			       /* p */
    225,			       /* s */
    299,			       /* z */
};

const int rest_widths[] = {
    285,			       /* breve rest */
    329,			       /* semibreve rest */
    329,			       /* minim rest */
    222,			       /* crotchet rest */
    257,			       /* quaver rest */
    331,			       /* semiquaver rest */
    405,			       /* demisemiquaver rest */
    479,			       /* hemidemisemiquaver rest */
};

const int note_widths[] = {
    663,			       /* breve */
    455,			       /* semibreve */
    316,			       /* minim */
    316,			       /* crotchet or hemi?demi?semi?quaver */
    283,			       /* acciaccatura */
    247,			       /* appoggiatura */
    319,			       /* artificial harmonic */
};

const int sprout_left_x[] = {
    0, 0,			       /* not for [semi]breve */
    -148, -148,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};
const int sprout_right_x[] = {
    0, 0,			       /* not for [semi]breve */
    144, 144,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};

const int sprout_left_y[] = {
    0, 0,			       /* not for [semi]breve */
    -36, -36,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};
const int sprout_right_y[] = {
    0, 0,			       /* not for [semi]breve */
    36, 36,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};

const int flag_ht[] = {
    99,				       /* staccato */
    31,				       /* legato */
    184,			       /* staccatissimo */
    409,			       /* sforzando */
    353,			       /* down bow */
    410,			       /* up bow */
    314,			       /* stopping */
    353,			       /* fermata */
    164,			       /* natural harmonic */
    254,			       /* accent */
    376,			       /* trill */
    286,			       /* turn */
    0,				       /* (inverted turn) */
    253,			       /* mordent */
    413,			       /* inverted mordent */
    388,			       /* small sharp */
    333,			       /* small natural */
    311,			       /* small flat */
    0,				       /* (tremolo) */
    0,				       /* (fingering) */
    0,				       /* (arpeggio) */
};
