/*
 * enigma/savefile.c - provide routines that load and save a user's
 * progress details and saved positions.
 * 
 * Copyright 2000 Simon Tatham. All rights reserved.
 * 
 * Enigma is licensed under the MIT licence. See the file LICENCE for
 * details.
 * 
 * - we are all amf -
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "enigma.h"

gamestate *savepos_load(levelset *set, char *user, int savenum) {
    FILE *fp;
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];
    gamestate *state;
    int nlines, levnum;
    int i, j;

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, SAVEDIR, sizeof(fname));
    strncpy(fname + strlen(fname), set->name, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".", sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), user, sizeof(fname)-strlen(fname));
    sprintf(buf, ".%d", savenum+1);
    strncpy(fname + strlen(fname), buf, sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	/* filename length overflow */
	return NULL;
    }

    fp = fopen(fname, "r");
    if (!fp) {
	/* can't open save file */
	return NULL;
    }

    state = NULL;
    nlines = 0;

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    /* line length overflow in save file */
	    return NULL;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (state == NULL && !ishdr(buf, "Level: ")) {
	    /* Level: line not first in save file */
	    return NULL;
	}
	if (ishdr(buf, "Level: ")) {
	    levnum = atoi(buf+7);
	    state = gamestate_new(set->levels[levnum-1]->width,
				  set->levels[levnum-1]->height);
	    state->levnum = levnum;
	    state->title = set->levels[levnum-1]->title;
	    state->status = PLAYING;
	} else if (ishdr(buf, "Moves: ")) {
	    state->movenum = atoi(buf + 7);
	} else if (ishdr(buf, "Gold: ")) {
	    state->gold_got = atoi(buf + 6);
	} else if (ishdr(buf, "TotalGold: ")) {
	    state->gold_total = atoi(buf + 11);
	} else if (ishdr(buf, "Map: ")) {
	    if (state->leveldata == NULL) {
		/* map before size in save file */
		return NULL;
	    }
	    if ((int)strlen(buf + 5) != state->width) {
		/* wrong length map line in save file */
		return NULL;
	    }
	    if (nlines >= state->height) {
		/* too many map lines in save file */
		return NULL;
	    }
	    memcpy(state->leveldata + state->width * nlines,
		   buf + 5, state->width);
	    nlines++;
	} else {
	    /* unrecognised keyword in save file */
	    return NULL;
	}
    }
    if (nlines < state->height) {
	/* not enough map lines in save file */
	return NULL;
    }

    fclose(fp);

    /*
     * Find the player.
     */
    for (j = 0; j < state->height; j++) {
	for (i = 0; i < state->width; i++) {
	    if (state->leveldata[j*state->width+i] == '@') {
		state->player_x = i;
		state->player_y = j;
	    }
	}
    }

    return state;
}

void savepos_del(levelset *set, char *user, int savenum) {
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, SAVEDIR, sizeof(fname));
    strncpy(fname + strlen(fname), set->name, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".", sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), user, sizeof(fname)-strlen(fname));
    sprintf(buf, ".%d", savenum+1);
    strncpy(fname + strlen(fname), buf, sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	/* file name length overflow */
	return;
    }
    remove(fname);
}

void savepos_save(levelset *set, char *user, int savenum, gamestate *state) {
    FILE *fp;
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];
    int i;

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, SAVEDIR, sizeof(fname));
    strncpy(fname + strlen(fname), set->name, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".", sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), user, sizeof(fname)-strlen(fname));
    sprintf(buf, ".%d", savenum+1);
    strncpy(fname + strlen(fname), buf, sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	/* File name length overflow */
	return;
    }

    /* For writing this file we want umask 077, so we get file mode 0600. */
    umask(077);
    fp = fopen(fname, "w");
    if (!fp) {
	/* unable to write save file */
	return;
    }
    /* Now let's be very sure the file mode came out right. */
    fchmod(fileno(fp), 0600);

    fprintf(fp, "Level: %d\nMoves: %d\nGold: %d\nTotalGold: %d\n",
	    state->levnum, state->movenum, state->gold_got,
	    state->gold_total);
    for (i = 0; i < state->height; i++) {
	fprintf(fp, "Map: %.*s\n", state->width,
		state->leveldata + i * state->width);
    }

    fclose(fp);
}

progress progress_load(levelset *set, char *user) {
    FILE *fp;
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];
    progress p;

    p.levnum = 0;
    p.date = -1;

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, SAVEDIR, sizeof(fname));
    strncpy(fname + strlen(fname), set->name, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".", sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), user, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".progress", sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	/* file name length overflow */
	return p;
    }

    fp = fopen(fname, "r");
    if (!fp) {
	/* unable to read progress file */
	return p;
    }

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    /* line length overflow in save file */
	    return p;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (ishdr(buf, "Level: ")) {
	    p.levnum = atoi(buf+7);
	} else if (ishdr(buf, "Date: ")) {
	    p.date = parse_date(buf+6);
	} else {
	    /* unrecognised keyword in progress file */
	    return p;
	}
    }

    fclose(fp);

    return p;
}

void progress_save(levelset *set, char *user, progress p) {
    FILE *fp;
    char fname[FILENAME_MAX];
    char datebuf[40];

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, SAVEDIR, sizeof(fname));
    strncpy(fname + strlen(fname), set->name, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".", sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), user, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".progress", sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	/* file name length overflow */
	return;
    }

    /* For writing this file we want umask 037, so we get file mode 0640. */
    umask(037);
    fp = fopen(fname, "w");
    if (!fp) {
	/* unable to write progress file */
	return;
    }
    /* Now let's be very sure the file mode came out right. */
    fchmod(fileno(fp), 0640);

    fmt_date(datebuf, p.date);
    fprintf(fp, "Level: %d\nDate: %s\n", p.levnum, datebuf);

    fclose(fp);
}
