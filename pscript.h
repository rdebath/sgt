/* pscript.h  exports from pscript.c */

#ifndef MUS_PSCRIPT_H
#define MUS_PSCRIPT_H

#include <stdio.h>

#define F_ROMAN		1
#define F_ITALIC	2

/* the actual symbol names */
enum {
    s_accent, s_acciaccatura, s_appoggiatura, s_arpeggio, s_big0,
    s_big1, s_big2, s_big3, s_big4, s_big5, s_big6, s_big7, s_big8,
    s_big9, s_bowdown, s_bowup, s_bracelower, s_braceupper,
    s_bracketlower, s_bracketupper, s_breath, s_breve, s_clefC,
    s_clefF, s_clefG, s_coda, s_ditto, s_doubleflat, s_doublesharp,
    s_dynamicf, s_dynamicm, s_dynamicp, s_dynamics, s_dynamicz,
    s_fermata, s_flat, s_harmart, s_harmnat, s_headcrotchet,
    s_headminim, s_legato, s_mordentlower, s_mordentupper,
    s_natural, s_repeatmarks, s_restbreve, s_restcrotchet,
    s_restdemi, s_resthemi, s_restminim, s_restquaver, s_restsemi,
    s_segno, s_semibreve, s_sforzando, s_sharp, s_small0, s_small1,
    s_small2, s_small3, s_small4, s_small5, s_small6, s_small7,
    s_small8, s_small9, s_smallflat, s_smallnatural, s_smallsharp,
    s_staccatissdn, s_staccatissup, s_staccato, s_stopping,
    s_taildemidn, s_taildemiup, s_tailhemidn, s_tailhemiup,
    s_tailquaverdn, s_tailquaverup, s_tailsemidn, s_tailsemiup,
    s_timeCbar, s_timeC, s_trill, s_turn, s_INVALID
};

#define bigdigit(i)	(s_big0 + (i))
#define smalldigit(i)	(s_small0 + (i))

#define ALIGN_LEFT	0
#define ALIGN_CENTRE	1
#define ALIGN_RIGHT	2

extern FILE *out_fp;

extern void use_font(int font);

extern void ps_init(void);
extern void ps_done(void);

extern void ps_init_page(int page);
extern void ps_end_page(void);
extern void ps_throw_page(void);

extern void clear_syms(void);
extern void put_symbol(int x, int ax, int y, int sym);
extern void put_line(int x1, int ax1, int y1,
		     int x2, int ax2, int y2, int thickness);
extern void put_tie(int x1, int ax1, int x2, int ax2, int y, int down);
extern void put_beam(int x1, int y1, int x2, int y2);
extern void put_text(int x, int ax, int y, char *text, int alignment);

extern void draw_all(double hdsq);

extern void put_mark(void);
extern void shift_mark(int rx);

#endif
