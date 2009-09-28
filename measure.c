#include "measure.h"

const int tiny_widths[] = {
    67,				       /* dot on dotted notes */
    326,			       /* breve */
    227,			       /* semibreve */
    170,			       /* minim */
    170,			       /* crotchet */
    307,			       /* quaver */
    307,			       /* semiquaver */
    307,			       /* demisemiquaver */
    307,			       /* hemidemisemiquaver */
    216,			       /* f */
    460,			       /* m */
    365,			       /* p */
    225,			       /* s */
    299,			       /* z */
};

const int rest_widths[] = {
    299,			       /* breve rest */
    313,			       /* semibreve rest */
    313,			       /* minim rest */
    256,			       /* crotchet rest */
    262,			       /* quaver rest */
    330,			       /* semiquaver rest */
    400,			       /* demisemiquaver rest */
    469,			       /* hemidemisemiquaver rest */
};

const int note_widths[] = {
    613,			       /* breve */
    415,			       /* semibreve */
    300,			       /* minim */
    299,			       /* crotchet or hemi?demi?semi?quaver */
    285,			       /* acciaccatura */
    279,			       /* appoggiatura */
    301,			       /* artificial harmonic */
};

const int sprout_left_x[] = {
    0, 0,			       /* not for [semi]breve */
    -139, -138,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};
const int sprout_right_x[] = {
    0, 0,			       /* not for [semi]breve */
    136, 137,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};

const int sprout_left_y[] = {
    0, 0,			       /* not for [semi]breve */
    -40, -48,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};
const int sprout_right_y[] = {
    0, 0,			       /* not for [semi]breve */
    37, 50,			       /* minim and crotchet/quaver */
    0, 0, 0,			       /* not for ornament notes */
};

const int flag_ht[] = {
    96,				       /* staccato */
    40,				       /* legato */
    160,			       /* staccatissimo */
    262,			       /* sforzando */
    352,			       /* down bow */
    416,			       /* up bow */
    304,			       /* stopping */
    352,			       /* fermata */
    120,			       /* natural harmonic */
    248,			       /* accent */
    384,			       /* trill */
    273,			       /* turn */
    0,				       /* (inverted turn) */
    240,			       /* mordent */
    704,			       /* inverted mordent */
    384,			       /* small sharp */
    376,			       /* small natural */
    336,			       /* small flat */
    0,				       /* (tremolo) */
    0,				       /* (fingering) */
    0,				       /* (arpeggio) */
};
