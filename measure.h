/* Hopefully, *all* the measurements in MUS that depend on the particular
 * symbol set used. */

#ifndef MUS_MEASURE_H
#define MUS_MEASURE_H

/* A nasty mechanism to ensure that a macro exists to convert a number to a
 * string at preprocess time. stringise1() is fine, unless the argument
 * *contains* macros that need expanding. By the ANSI C standard, applying a
 * second level of macro definition causes the expansion of macros in the
 * parameter before stringisation. The reason this is in this header file
 * is that it gets applied to "text_ht" below. */
#define stringise1(x)	#x
#define stringise(x)	stringise1(x)

/* Text sizes: "maintitle_ht" is the size of text used for the main title of a
 * piece. "author_ht" is the size of text used for the name of the author, and
 * "subtitle_ht" any sub-title. "text_ht" is the size of text used for all
 * other text, within the piece. */
#define maintitle_ht	1200
#define author_ht	600
#define subtitle_ht	500
#define text_ht		500

/* General sizes: "indent" is the left margin allowed for brackets and braces
 * on staves; "first_indent" is the *extra* indent on top of that given to
 * the first line of the score; "scorespacing" is the spacing between
 * successive complete lines of the score; "stave_spacing" is the spacing
 * between staves, not counting the height of the actual staves. "nh" is the
 * spacing between adjacent lines of the same stave ("note height") and "ph"
 * is the spacing between successive diatonic pitches (by definition, half
 * of "nh"). "maximum_ax" is the absolute maximum distance per AX unit,
 * when auto line breaking is in force. */
#define indent 		360
#define first_indent	1500
#define scorespacing	3808
#define stave_spacing	1992
#define nh		250
#define ph		(nh/2)
#define maximum_ax	0.6

/* Bar lines and repeat (:) markers: "repeat_width" is the width (including
 * some spacing) for repeat marks, {thin,thick}_barline are the thicknesses
 * of bar lines, and line_bar_len is the length of a bar line placed solely
 * on a single-line stave. Double bars etc. are separated by "barline_ispace"
 * amount of space. Repeat endings (1st ending, 2nd ending) consist of a
 * square bracket ranging from height "rep_ending_bot" to "rep_ending_top"
 * above the topmost stave, plus a big digit at height "rep_ending_symy"
 * and horizontal offset "rep_ending_symx". */
#define repeat_width	122
#define thin_barline	24
#define thick_barline	100
#define line_bar_len	500
#define barline_ispace	75
#define rep_ending_bot	700
#define rep_ending_top	1300
#define rep_ending_symx	400
#define rep_ending_symy	750

/* Braces and brackets between staves: the hot spots of the brace symbols
 * should be "brace_xoffset" back from the start of the stave. The
 * "braceupper" symbol should be "brace_top_yoff" down from the bottom of the
 * upper stave, and "bracelower" should be "brace_bot_yoff" up from the top of
 * the lower stave. Bracket lines should begin "bracket_xoff" back from the
 * start of the stave and be spaced at interval "bracket_spacing". The lines
 * of brackets should be "bracket_thick" thick. The actual bracket top/bottom
 * *symbols* should be "bracket_s_xoff" back from the stave start position. */
#define brace_xoffset	100
#define brace_top_yoff	996
#define brace_bot_yoff	996
#define bracket_xoff	150
#define bracket_spacing	200
#define bracket_thick	150
#define bracket_s_xoff	12

/* Before drawing clefs, the X position should advance by "clef_width", and
 * the actual clefs should be "clef_back" back from the new X position. */
#define clef_width	980
#define clef_back	800

/* Key signatures: to draw a sharp, advance the X position by "sharp_width",
 * then draw the sharp with its hot spot "sharp_xoff" *back* from where we
 * ended up. Ditto for a flat, and a natural (when key signature changes). */
#define ks_sharp_width	250
#define ks_flat_width	250
#define ks_nat_width	250
#define ks_sharp_xoff	20
#define ks_flat_xoff	20
#define ks_nat_xoff	20

/* Time signatures: "time_space" is the spacing left before any time
 * signature. "time_common" is the width of a C (or C-bar) time signature.
 */
#define time_space	300
#define time_common	409

/* Markings around staves: "sign_yoff" is the distance above the stave at
 * which a segno or coda sign is placed. "coda_break_wid" is the width of
 * the break in the stave lines at the point of a coda. "coda_width" is the
 * width of the coda symbol itself. "barnum_yoff" is the distance above the
 * top stave to put bar numbers. "text_yoff" is the distance above the top
 * stave to put text. "text_below_yoff" is the distance below each stave
 * to put "B" text; "dynamic_yoff" is the distance below each stave to put
 * dynamics, and "lyrics_yoff" is the distance below each stave to put
 * lyrics. "sname_xoff" is the offset backwards from the left end of the
 * first score line to draw the stave names. */
#define sign_yoff	200
#define coda_break_wid	300
#define coda_width	457
#define barnum_yoff	600
#define text_yoff	750
#define text_below_yoff	800
#define dynamic_yoff	1142
#define lyrics_yoff	1250
#define sname_xoff	600

/* Multiple bars rest: the symbol for this takes the form of a couple of
 * rotated T shapes with a number in between. So "barsrest_cross" is the
 * length of the crosspiece of each T, "barsrest_stem" is the length of
 * the stem of each T. "barsrest_ispace" is the space left between the end
 * of the stem of the T and the first digit of the number, and equally the
 * space left between the last digit and the stem of the right-hand T.
 * "barsrest_ospace" is the space left on the left and right of the whole
 * construct. "barsrest_thick" is the thickness of the lines used to draw
 * the Ts. "barsrest_whole" is the width of the whole thing, up to the
 * crosspiece of the right T, assuming it contains only one digit. */
#define barsrest_cross	500
#define barsrest_stem	300
#define barsrest_ispace	350
#define barsrest_ospace	300
#define barsrest_thick	100
#define barsrest_whole	(2*(barsrest_ispace+barsrest_stem)+barsrest_ospace)

/* Dotted notes and ntuplets: dots are drawn using the same symbol as is used
 * as a marker above notes flagged "staccato". The hot spot on this symbol is
 * at the bottom. So the dot must be lowered by "dot_halfht", half the height
 * of the dot symbol. It must also be "dot_xoff" to the right of the note.
 * Further dots should be spaced by "dot_ispace", and horizontal space
 * "dot_ospace" should be allowed after all dots. Ntuplet numbers are written
 * "ntuplet_space" away from the notes. "ntuplet_vspace" should be allowed
 * around the number when recording its position for slurs. "ntuplet_clear" is
 * the minimum vertical clearance between the number and the stave. */
#define dot_halfht	51
#define dot_xoff	150
#define dot_ispace	150
#define dot_ospace	200
#define ntuplet_space	200
#define ntuplet_vspace	50
#define ntuplet_clear	125

/* Accidentals: the hot spot of the rightmost accidental on a note should
 * be "acc_space" to the left of the leftmost part of the note; successive
 * accidentals, should they need stacking, are stacked at "acc_width" distance
 * apart. */
#define acc_space	50
#define acc_width	250

/* Note stems: the minimum stem length for an unbeamed note is "stem_norm_len"
 * on normal staves, and "stem_line_len" on single-line staves. Stems gain
 * "stem_tail_extra" in length for every quaver tail. Quaver tails are spaced
 * vertically by the same "tail_vspace", and have width "tail_width". */
#define stem_norm_len	850
#define stem_line_len	600
#define stem_tail_extra	150
#define tail_vspace	150
#define tail_width	286

/* Note flags: "turn_inv_space" and "turn_inv_thick" give measurements
 * concerning the vertical line through the middle of an inverted turn - see
 * flag() in notes.c for details. "flag_ispace" is the spacing between
 * successive flags. "flag_ospace" is the minimum space above or below the
 * stave, for those flags which must be outside the stave lines. "trem_space"
 * is the vertical space allowed between tremolo lines on the stem.
 * "trem_xspread" is the horizontal distance that the tremolo lines extend
 * on either side of the stem; "trem_yspread" is the vertical distance that
 * is covered on each side. "trem_thick" is the thickness of tremolo lines.
 * "arp_width" and "arp_height" give the size of an arpeggio symbol, including
 * "arp_space" amount of space on the right. */
#define turn_inv_space	80
#define turn_inv_thick	30
#define flag_ispace	50
#define flag_ospace	200
#define trem_space	100
#define trem_xspread	150
#define trem_yspread	50
#define trem_thick	50
#define arp_width	139
#define arp_height	536
#define arp_space	80

/* Beams on quavers or less: double or triple beams are spaced by
 * "beam_vspace" - the actual vertical thickness of a beam is only stored
 * in the prologue code - and beams have maximum gradient "beam_grad_max". */
#define beam_vspace	200
#define beam_grad_max	0.13

/* Slurs: the minimum slur height is three quarters of "slur_ht_min".
 * (The 3/4 factor is for mathematical reasons, explained in slurs.c.)
 * The vertical height of the actual slur line is "slur_thick_min" at the
 * ends, and "slur_thick_max" in the middle. */
#define slur_ht_min	300
#define slur_thick_min	24
#define slur_thick_max	100

/* Ties, hairpins and 8v[ab]: "tie_space" is the space left between a note
 * and the tie marking. "tie_norm_ht" is the usual height of a tie symbol.
 * "hairpin_yoff" is the vertical distance from the centreline of a hairpin
 * to the bottom of the stave above. "hairpin_width" is the distance between
 * the open ends of a hairpin. "eightva_yoff" is the distance between the
 * horizontal line of an 8v[ab] and the top/bottom of the relevant stave.
 * "eightva_fwd" is the distance forward from the start point at which to draw
 * the '8', "eightva_back" is the distance back from the end point at which
 * to draw the vertical line, and "eightva_lstart" is the distance forward
 * from the start point at which to begin the horizontal. */
#define tie_space	50
#define tie_norm_ht	124
#define hairpin_yoff	1000
#define hairpin_width	400
#define eightva_yoff	650
#define eightva_back	150
#define eightva_fwd	50
#define eightva_lstart	181

/* Stream-control symbols: "breath_width" is the width of a flute breathing
 * mark. A general pause sign consists of two diagonal lines, each centred
 * vertically on the top line of the stave, each "genpause_thick" thick, and
 * covering horizontal distance "genpause_dx" and vertical "genpause_dy". The
 * two lines are separated horizontally by distance "genpause_xoff". A turn
 * symbol has width "turn_width". */
#define breath_width	500
#define genpause_thick	100
#define genpause_dx	400
#define genpause_dy	600
#define genpause_xoff	200
#define turn_width	621

/* General: "bigdigit_wid" is the width of a big digit such as is used in
 * time signatures, multiple bars rest and first/second repeat endings.
 * "bigdigit_ht" is its height. "smalldigit_ht" is the height of the small
 * digits used in fingering markings, Ntuplets and 8v[ab], and
 * "smalldigit_wid" is the width of same (including spacing). "ditto_width" is
 * the width of a ditto mark. "leger_width" is the length (== width) of a
 * leger line. "sb_rest_ht" is the height of a semibreve (or minim) rest.
 * "bar_end_space" is the space at each end of a bar, between the actual
 * notes and the bar line. */
#define bigdigit_wid	400
#define bigdigit_ht	432
#define smalldigit_ht	313
#define smalldigit_wid	275
#define ditto_width	900
#define leger_width	450
#define sb_rest_ht	125
#define bar_end_space	300

/* The following arrays are all defined in "measure.c". */

/* The widths of all the "tiny" symbols: small notes as used in "crotchet=100"
 * notation, and dynamic font characters as used in "mf", "mp" etc. Indices
 * into this array are obtained by subtracting '\200' from the character
 * involved. */
extern const int tiny_widths[];

/* The widths of all the rests. The indices in this correspond to the "enum"
 * definition of the member "rest_type" of structure "NoteStem" defined in
 * "melody.h". */
extern const int rest_widths[];

/* The widths of all the note heads. The indices in this correspond to the
 * "enum" definition of the member "heads_type" of structure "NoteStem"
 * defined in "melody.h". */
extern const int note_widths[];

/* Sprouting points: the coordinates, relative to the centre of a note, at
 * which the stem line must begin. "sprout_left_[xy]" is for stems on the
 * left of the note head - usually because the stem is going down, but
 * occasionally because the note head has flipped to the other side of the
 * stem - and "sprout_right_[xy]" is for stems on the right. Indices are
 * as in note_widths[] above. */
extern const int sprout_left_x[];
extern const int sprout_left_y[];
extern const int sprout_right_x[];
extern const int sprout_right_y[];

/* Heights of all the note flags. Indices match the "enum" definition of
 * member "type" of structure "NoteFlags" in "melody.h". */
extern const int flag_ht[];

#endif
