/*
 * enigma/screen.c - perform screen/keyboard handling. All
 * interaction with Curses is contained in this module, so an X
 * front end (for example) could be implemented just by replacing
 * this one file.
 */

#include <stdio.h>
#include <ncurses.h>

#include "enigma.h"

#define FIXME 0

/*
 * Attributes.
 */
static struct {
    int fg, bg, attrs;
} attrs[] = {
    /*
     * Attributes to use when displaying a level in play.
     */
#define T_WALL_STRAIGHT 0	       /* straight walls: - and | */
    { COLOR_CYAN, COLOR_BLUE, 0 },
#define T_WALL_CORNER 1		       /* corner walls: + */
    { COLOR_WHITE, COLOR_BLUE, 0 },
#define T_WALL_AMORPH 2		       /* amorphous walls: % */
    { COLOR_BLUE, COLOR_BLACK, 0 },
#define T_WALL_KILLER 3		       /* killer walls: & */
    { COLOR_GREEN, COLOR_BLACK, 0 },
#define T_BOMB 4		       /* bombs: W X Y Z */
    { COLOR_RED, COLOR_BLACK, A_BOLD },
#define T_ARROW 5		       /* arrows: > v < ^ */
    { COLOR_RED, COLOR_BLACK, 0 },
#define T_GOLD 6		       /* gold: $ */
    { COLOR_YELLOW, COLOR_BLACK, A_BOLD },
#define T_EARTH 7		       /* earth: . : */
    { COLOR_YELLOW, COLOR_BLACK, 0 },
#define T_SACK 8		       /* sacks: o 8 */
    { COLOR_CYAN, COLOR_BLACK, 0 },
#define T_PLAYER 9		       /* the player: @ */
    { COLOR_WHITE, COLOR_BLACK, A_BOLD },
#define T_TITLE 10		       /* title at top of screen */
    { COLOR_CYAN, COLOR_BLUE, A_BOLD },
#define T_SPACE 11		       /* empty space: ' ' */
    { COLOR_WHITE, COLOR_BLACK, 0 },
#define T_STATUS_1 12		       /* level number and slash at bottom */
    { COLOR_CYAN, COLOR_BLACK, 0 },
#define T_STATUS_2 13		       /* level title and gold counters */
    { COLOR_YELLOW, COLOR_BLACK, 0 },

    /*
     * Attributes to use when displaying the main game menu.
     */
#define T_ASCII_ART 14    	       /* the Enigma banner */
    { COLOR_CYAN, COLOR_BLUE, A_BOLD },
#define T_LIST_ELEMENT 15	       /* a playable level or saved game */
    { COLOR_WHITE, COLOR_BLACK, 0 },
#define T_LIST_SELECTED 16	       /* a selected level or saved game */
    { COLOR_WHITE, COLOR_BLUE, A_BOLD },
#define T_LIST_ADMIN 17		       /* "...more..." or "...remain unseen" */
    { COLOR_RED, COLOR_BLACK, 0 },
#define T_LIST_BOX 18		       /* outline of list box */
    { COLOR_CYAN, COLOR_BLUE, 0 },
#define T_INSTRUCTIONS 19	       /* instructions on what keys to press */
    { COLOR_YELLOW, COLOR_BLACK, 0 },
};

void screen_init(void) {
    int i;

    initscr();
    noecho();
    keypad(stdscr, 1);
    move(0,0);
    refresh();
    leaveok(stdscr, 1);
    if (has_colors()) {
	start_color();
	for (i = 0; i < lenof(attrs); i++) {
	    init_pair(i+1, attrs[i].fg, attrs[i].bg);
	    attrs[i].attrs |= COLOR_PAIR(i+1);
	}
    }
}

void screen_finish(void) {
    endwin();
}

void screen_prints(int x, int y, int attr, char *string) {
    wattrset(stdscr, attrs[attr].attrs);
    wmove(stdscr, y, x);
    waddstr(stdscr, string);
}

void screen_printc(int x, int y, int attr, int c) {
    wattrset(stdscr, attrs[attr].attrs);
    wmove(stdscr, y, x);
    waddch(stdscr, c);
}

void screen_level_init(void) {
    /*
     * Hook to make a windowing port of Enigma easier. This is
     * called when the player begins to play a level, and
     * screen_level_finish() is called when the level is finished.
     * The idea is that these routines can be reimplemented to open
     * and close a window of some sort.
     */

    /* In this implementation, we do nothing. */
}

void screen_level_finish(void) {
    /*
     * This is called after the last game position (GAME OVER,
     * COMPLETED, ABANDONED) has been displayed. Its task is to
     * allow the user time to read everything, and then close the
     * window.
     */
    getch();
}

void screen_level_display(gamestate *s, char *message) {
    int sw, sh;
    int i, j;
    int dx, dy;
    char buf[20];

    getmaxyx(stdscr, sh, sw);

    werase(stdscr);

    dx = (sw - s->width) / 2;
    dy = (sh - s->height) / 2;

    for (j = 0; j < s->height; j++)
	for (i = 0; i < s->width; i++) {
	    char c = s->leveldata[j*s->width+i];
	    attr_t attr = 0;
	    switch (c) {
	      case '-': case '|':      attr = T_WALL_STRAIGHT; break;
	      case '+':                attr = T_WALL_CORNER; break;
	      case '%':                attr = T_WALL_AMORPH; break;
	      case '&':                attr = T_WALL_KILLER; break;
	      case 'W': case 'X':
	      case 'Y': case 'Z':      attr = T_BOMB; break;
	      case '>': case 'v':
	      case '<': case '^':      attr = T_ARROW; break;
	      case '$':                attr = T_GOLD; break;
	      case '.': case ':':      attr = T_EARTH; break;
	      case 'o': case '8':      attr = T_SACK; break;
	      case '@':                attr = T_PLAYER; break;
	      default:                 attr = T_SPACE; break;
	    }
	    screen_printc(i+dx, j+dy, attr, c);
	}

    /*
     * Display title.
     */
    screen_prints((sw-9)/2, dy-1, T_TITLE, " Enigma ");

    /*
     * Display status line.
     */
    sprintf(buf, "%d) ", 1 /* FIXME: s->levelnum */);
    screen_prints(dx, dy+s->height, T_STATUS_1, buf);
    screen_prints(dx+strlen(buf), dy+s->height, T_STATUS_2,
		  "Level Title" /* FIXME: s->title */);
    sprintf(buf, "%2d", s->gold_got);
    screen_prints(dx+s->width-5, dy+s->height, T_STATUS_2, buf);
    screen_printc(dx+s->width-3, dy+s->height, T_STATUS_1, '/');
    sprintf(buf, "%2d", s->gold_total);
    screen_prints(dx+s->width-2, dy+s->height, T_STATUS_2, buf);

    refresh();
}

/*
 * Get a move. Can return 'h','j','k','l', or 'q', or '0'-'9' for
 * saves, or 's'.
 */
int screen_level_getmove(void) {
    int i;
    do {
	i = getch();
	if (i == KEY_UP) i = 'k';
	if (i == KEY_DOWN) i = 'j';
	if (i == KEY_LEFT) i = 'h';
	if (i == KEY_RIGHT) i = 'l';
	if (i >= 'A' && i <= 'Z')
	    i += 'a' - 'A';
    } while (i != 'h' && i != 'j' && i != 'k' && i != 'l' && i != 'q' &&
	     i != 's' && (i < '0' || i > '9'));
    return i;
}

/*
 * Display the levels-or-saves main menu. Returns a level number (1
 * or greater), a save number (0 to -9), or a `quit' signal (-100).
 */
int screen_main_menu(levelset *set, int maxlev, int startlev) {
    const int colwidth = 26, colgap = 8;
    const int height = 21, llines = height-5;
    int sx, sy, dx, dy, dx2;
    int save = 0;
    int level = startlev;
    int levtop = 0;
    int i, k;
    int unseen = 0;

    getmaxyx(stdscr, sy, sx);

    werase(stdscr);

    dx = (sx - 2*colwidth - colgap) / 2;
    dy = (sy - height) / 2;
    dx2 = dx + colwidth + colgap;

    while (1) {
	/*
	 * Display the ASCII art banner in the top right.
	 */
	screen_prints(dx2, dy+0, T_ASCII_ART, "                          ");
	screen_prints(dx2, dy+1, T_ASCII_ART, "    ._                    ");
	screen_prints(dx2, dy+2, T_ASCII_ART, "    |_ ._ o _ ._ _  _.    ");
	screen_prints(dx2, dy+3, T_ASCII_ART, "    |_ | ||(_|| | |(_|    ");
	screen_prints(dx2, dy+4, T_ASCII_ART, "            _|            ");
	screen_prints(dx2, dy+5, T_ASCII_ART, "                          ");

	/*
	 * Display the saved position list.
	 */
	for (i = 0; i < 10; i++) {
	    char buf[40];
	    sprintf(buf, "%d) [empty save slot]      ", (i+1)%10);
	    screen_prints(dx2, dy+7+i,
			  i == save ? T_LIST_SELECTED : T_LIST_ELEMENT, buf);
	}

	/*
	 * Display the level title list box.
	 */
	wattrset(stdscr, attrs[T_LIST_BOX].attrs);
	for (i = 1; i < colwidth-1; i++) {
	    screen_printc(dx+i, dy, T_LIST_BOX, '-');
	    screen_printc(dx+i, dy+height-5, T_LIST_BOX, '-');
	}
	screen_printc(dx, dy, T_LIST_BOX, '+');
	screen_printc(dx, dy+height-5, T_LIST_BOX, '+');
	screen_printc(dx+colwidth-1, dy, T_LIST_BOX, '+');
	screen_printc(dx+colwidth-1, dy+height-5, T_LIST_BOX, '+');
	for (i = 1; i < height-5; i++) {
	    char buf[50];
	    int attr = T_LIST_ELEMENT;
	    if (i == 1 && levtop > 0) {
		sprintf(buf, "(more)");
		attr = T_LIST_ADMIN;
	    } else if (i+levtop-1 == maxlev && maxlev < set->nlevels) {
		sprintf(buf, "(%d remain unseen)", set->nlevels - maxlev);
		unseen = TRUE;
		attr = T_LIST_ADMIN;
	    } else if (i == height-6 && i+levtop-1 < maxlev && !unseen) {
		sprintf(buf, "(more)");
		attr = T_LIST_ADMIN;
	    } else {
		if (i+levtop-1 < maxlev)
		    sprintf(buf, "%2d) %.40s", i+levtop,
			    set->levels[i+levtop-1]->title);
		else
		    *buf = '\0';
		if (i+levtop-1 == level)
		    attr = T_LIST_SELECTED;
	    }
	    if (strlen(buf) < colwidth-2)
		sprintf(buf+strlen(buf), "%*s", colwidth-2-strlen(buf), "");
	    buf[colwidth-2] = '\0';
	    screen_prints(dx+1, dy+i, attr, buf);
	    screen_printc(dx, dy+i, T_LIST_BOX, '|');
	    screen_printc(dx+colwidth-1, dy+i, T_LIST_BOX, '|');
	}
	screen_prints(dx, dy+height-3, T_INSTRUCTIONS, "Press Up/Down to choose a");
	screen_prints(dx, dy+height-2, T_INSTRUCTIONS, "level, and RET to play from");
	screen_prints(dx, dy+height-1, T_INSTRUCTIONS, "the start of that level.");
	screen_prints(dx2+colwidth-26, dy+height-3, T_INSTRUCTIONS, "Press 1-9 or 0 to choose a");
	screen_prints(dx2+colwidth-24, dy+height-2, T_INSTRUCTIONS, "saved position, and R to");
	screen_prints(dx2+colwidth-26, dy+height-1, T_INSTRUCTIONS, "resume playing from there.");

	refresh();
	k = getch();
	if (k >= 'A' && k <= 'Z') k += 'a'-'A';
	if (k >= '0' && k <= '9')
	    save = (k - '1' + 10) % 10;
	if ((k == 'k' || k == KEY_UP) && level > 0) {
	    level--;
	    if (!(level > levtop || (level == 0 && levtop == 0)))
		levtop = (level == 0 ? level : level-1);
	}
	if ((k == 'j' || k == KEY_DOWN) && level < maxlev-1) {
	    level++;
	    if (!(level < levtop+llines-1 || (level == levtop+llines-1 &&
					      level == set->nlevels))) {
		if (level == set->nlevels)
		    levtop = level - (llines-1);
		else
		    levtop = level - (llines-2);
	    }
	}
	if (k == 'q')
	    return -100;
	if (k == '\r' || k == '\n')
	    return level+1;
	if (k == 'r')
	    return -save;
    }
}
