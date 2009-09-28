#ifndef MUS_DRAWING_H
#define MUS_DRAWING_H

extern int xpos, ypos, axpos, y;

extern void begin_line(int left_indent, int barline_type);
extern void redraw_params(int break_now);
extern int process_bar(int *eof);
extern void do_bar_line (int type, int eol);
extern void draw_staves (void);
extern int report_bar_num (void);

/* Types of bar line: returned from process_bar */
enum {
    BL_NORMAL, BL_DOUBLE, BL_THIN_THICK,
    BL_REP_END, BL_REP_START, BL_REP_BOTH,
    BL_EOF
};

#endif
