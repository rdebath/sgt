/*
 * enigma/main.c - main program for Enigma.
 */

#include <stdio.h>

#include "enigma.h"

int main(int argc, char **argv) {
    char user[64];

    screen_init();
    if (setjmp(fatal_error_jmp_buf) == 0) {
	char *setname = DEFAULTSET;
	levelset *set;
	level *l;
	gamestate *gs;
	progress p;
	int i, action;
	gamestate *saves[10];

	get_user(user, sizeof(user));

	/*
	 * Pick up argv[1] in case it describes an alternate level
	 * set.
	 */
	if (argc > 1)
	    setname = argv[1];

	set = levelset_load(setname);
	p = progress_load(set, user);
	for (i = 0; i < 10; i++)
	    saves[i] = savepos_load(set, user, i);

	while (1) {
	    action = screen_main_menu(set, saves, p.levnum+1, p.levnum);
	    if (action > 0) {
		l = set->levels[action-1];
		gs = init_game(l);
	    } else if (action < -10) {
		break;		       /* direct quit from main menu */
	    } else {
		/* load a saved position */
		gs = gamestate_copy(saves[-action]);
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
		    } else if (k == 's') {
			/* FIXME: save the game */
		    } else if (k == 'r') {
			/* FIXME: restore the game */
		    } else if (k == 'q') {
			break;
		    }
		}
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
