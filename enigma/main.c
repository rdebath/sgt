/*
 * enigma/main.c - main program for Enigma.
 * 
 * Copyright 2000 Simon Tatham. All rights reserved.
 * 
 * Enigma is licensed under the MIT licence. See the file LICENCE for
 * details.
 * 
 * - we are all amf -
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
	int i, action, n, saveslot = 0;
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
		gs->levnum = action;
		saveslot = 0;
	    } else if (action == -100) {
		break;		       /* direct quit from main menu */
	    } else if (action <= -10) {
		/* delete a saved position */
		n = -action-10;
		if (saves[n])
		    gamestate_free(saves[n]);
		saves[n] = NULL;
		savepos_del(set, user, n);
		gs = NULL;
	    } else {
		/* load a saved position */
		n = -action;
		if (!saves[n])   /* don't segfault */
		    continue;
		gs = gamestate_copy(saves[n]);
		l = set->levels[gs->levnum-1];
		saveslot = n;
	    }
	    if (gs) {
		screen_level_init();
		while (gs->status == PLAYING) {
		    gamestate *newgs;
		    int k;
		    screen_level_display(gs, NULL);
		    k = screen_level_getmove();
		    if (k == 'h' || k == 'j' || k == 'l' || k == 'k') {
			newgs = make_move(gs, k);
			gamestate_free(gs);
			gs = newgs;
		    } else if (k == 's') {
			n = screen_saveslot_ask('s', saves, saveslot);
			if (n >= 0) {
			    saveslot = n;
			    saves[saveslot] = gamestate_copy(gs);
			    savepos_save(set, user, saveslot, gs);
			}
		    } else if (k == 'r') {
			n = screen_saveslot_ask('r', saves, saveslot);
			if (n >= 0 && saves[n]) {
			    saveslot = n;
			    gamestate_free(gs);
			    gs = gamestate_copy(saves[saveslot]);
			    l = set->levels[gs->levnum-1];
			}
		    } else if (k == 'q') {
			break;
		    }
		}
		if (gs->status != PLAYING) {
		    int increased_level = FALSE;
		    char *msg;
		    if (gs->status == DIED) {
			msg = "GAME OVER";
		    } else if (gs->status == COMPLETED) {
			msg = "LEVEL COMPLETE";
			if (p.levnum < gs->levnum) {
			    p.levnum = gs->levnum;
			    p.date = time(NULL);
			    progress_save(set, user, p);
			    increased_level = TRUE;
			}
		    } else {
			msg = "!INTERNAL ERROR!";
		    }
		    screen_level_display(gs, msg);
		    screen_level_finish();
		    if (increased_level && p.levnum == set->nlevels) {
			screen_completed_game();
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
