/*
 * enigma/main.c - main program for Enigma.
 */

#include <stdio.h>

#include "enigma.h"

int main(int argc, char **argv) {
    screen_init();
    if (setjmp(fatal_error_jmp_buf) == 0) {
	char *setname = DEFAULTSET;
	levelset *set;
	level *l;
	gamestate *gs;
	int action;

	/*
	 * FIXME: pick up argv[1] in case it describes an alternate
	 * level set.
	 */

	set = levelset_load(setname);
	action = screen_main_menu(set, 5, 4);
	if (action > 0) {
	    l = set->levels[action-1];
	    gs = init_game(l);
	} else if (action < -10) {
	    gs = NULL;		       /* direct quit from main menu */
	} else {
	    /* FIXME: load a saved posn */
	}
	if (gs) {
	    screen_level_init();
	    while (gs->status == PLAYING) {
		gamestate *newgs;
		int k;
		screen_level_display(gs, "");
		k = screen_level_getmove();
		if (k == 'h' || k == 'j' || k == 'l' || k == 'k') {
		    newgs = make_move(gs, k);
		    gamestate_free(gs);
		    gs = newgs;
		} else
		    break;
	    }
	}
    } else {
	screen_finish();
	fprintf(stderr, "Fatal error: %s\n", fatal_error_string);
	exit(2);
    }
    screen_finish();
    return 0;
}
