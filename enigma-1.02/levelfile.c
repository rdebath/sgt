/*
 * enigma/levelfile.c - provide routines that load a level
 * configuration file (which points to a set of levels) and a level
 * file itself.
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

#include "enigma.h"

static level *level_load(char *filename) {
    FILE *fp;
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];
    level *level;
    int nlines;

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, LEVELDIR, sizeof(fname));
    strncpy(fname + strlen(fname), filename, sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	fatal("File name length overflow");
    }

    fp = fopen(fname, "r");
    if (!fp) {
	fatal("Unable to read level file");
    }

    level = level_new();
    level->title = NULL;
    level->width = level->height = -1;
    nlines = 0;

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    fatal("Line length overflow in level set file");
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (ishdr(buf, "Title: ")) {
	    if (level->title) {
		fatal("Multiple titles in level file");
	    }
	    level->title = dupstr(buf + 7);
	} else if (ishdr(buf, "Width: ")) {
	    level->width = atoi(buf + 7);
	} else if (ishdr(buf, "Height: ")) {
	    level->height = atoi(buf + 8);
	} else if (ishdr(buf, "Map: ")) {
	    if (level->leveldata == NULL) {
		fatal("Map before size in level file");
	    }
	    if ((int)strlen(buf + 5) != level->width) {
		fatal("Wrong length map line in level file");
	    }
	    if (nlines >= level->height) {
		fatal("Too many map lines in level file");
	    }
	    memcpy(level->leveldata + level->width * nlines,
		   buf + 5, level->width);
	    nlines++;
	} else {
	    fatal("Unrecognised keyword in level file");
	}
	if (level->width > 0 && level->height > 0 && level->leveldata == NULL)
	    level_setsize(level, level->width, level->height);
    }
    if (nlines < level->height) {
	fatal("Not enough map lines in level file");
    }

    fclose(fp);

    return level;
}

levelset *levelset_load(char *filename) {
    FILE *fp;
    char buf[FILENAME_MAX+10];
    char fname[FILENAME_MAX];
    levelset *set;

    fname[sizeof(fname)-1] = '\0';
    strncpy(fname, LEVELDIR, sizeof(fname));
    strncpy(fname + strlen(fname), filename, sizeof(fname)-strlen(fname));
    strncpy(fname + strlen(fname), ".set", sizeof(fname)-strlen(fname));
    if (fname[sizeof(fname)-1] != '\0') {
	fatal("File name length overflow");
    }

    fp = fopen(fname, "r");
    if (!fp) {
	fatal("Unable to read level set file");
    }

    set = levelset_new();
    set->title = NULL;
    set->name = filename;

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    fatal("Line length overflow in level set file");
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (ishdr(buf, "Title: ")) {
	    if (set->title) {
		fatal("Multiple titles in level set file");
	    }
	    set->title = dupstr(buf + 7);
	} else if (ishdr(buf, "Level: ")) {
	    levelset_nlevels(set, set->nlevels+1);
	    set->levels[set->nlevels-1] = level_load(buf + 7);
	} else {
	    fatal("Unrecognised keyword in level set file");
	}
    }

    fclose(fp);

    return set;
}
