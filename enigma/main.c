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

#ifdef _WIN32
    /* chdir to the executable location in case SAVEDIR and LEVELDIR
     * are relative locations */
    char startdir[MAX_PATH];
    char *p, *q;

    GetModuleFileName(GetModuleHandle(NULL), startdir, sizeof(startdir));
    p = strrchr(startdir, '\\');
    if ((q = strrchr(p ? p+1 : startdir, '/')))
	p = q;
    if (p) {
	p[0] = '\0';
	chdir(startdir);
    }
#endif

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
		    } else if (k == 'w') {
			char *fname = screen_ask_movefile(1);
			if (!fname)
			    continue;
			sequence_save(fname, gs);
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
		    } else if (k == 'm') {
			char *fname;
			char *sequence;
			gamestate **movie;
			int nframes;
			int frame;
			char msg[80];
			int km;

			fname = screen_ask_movefile(0);
			if (!fname)
			    continue;
			sequence = sequence_load(fname);
			if (!sequence) {
			    screen_error_box("Unable to load move sequence");
			    continue;
			}
			sfree(fname);
			movie = smalloc(sizeof(*movie) * (strlen(sequence)+1));
			movie[0] = gamestate_copy(gs);
			frame = 0;
			while (sequence[frame]) {
			    movie[frame+1] = make_move(movie[frame],
						       sequence[frame]);
			    frame++;
			    nframes = frame;
			    if (movie[frame]->status != PLAYING)
				break;
			}
			sfree(sequence);
			frame = 0;
			while (1) {
			    char *status;
			    status = "";
			    if (movie[frame]->status == DIED)
				status = " - GAME OVER";
			    if (movie[frame]->status == COMPLETED)
				status = " - LEVEL COMPLETE";
			    sprintf(msg, "MOVIE MODE - %3d / %3d%s",
				    frame, nframes, status);
			    screen_level_display(movie[frame], msg);
			    km = screen_movie_getmove();
			    if (km == 'f' && frame < nframes) {
				frame++;
			    } else if (km == 'b' && frame > 0) {
				frame--;
			    } else if (km == '+') {
				frame += 10;
				if (frame > nframes) frame = nframes;
			    } else if (km == '-') {
				frame -= 10;
				if (frame < 0) frame = 0;
			    } else if (km == '>') {
				frame = nframes;
			    } else if (km == '<') {
				frame = 0;
			    } else if (km == 'q') {
				break;
			    }
			}
			gamestate_free(gs);
			gs = movie[nframes];
			for (i = 0; i < nframes; i++)
			    gamestate_free(movie[i]);
			sfree(movie);
		    } else if (k == 'q') {
			break;
		    }
		}
		if (gs->status != PLAYING) {
		    int increased_level = FALSE;
		    char *msg;
		    int k;

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
		    k = screen_finish_getmove();
		    if (k == 'w') {
			char *fname = screen_ask_movefile(1);
			if (!fname)
			    continue;
			sequence_save(fname, gs);
		    }
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
