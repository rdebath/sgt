/*
 * enigma/levelfile.c - provide routines that load a level
 * configuration file (which points to a set of levels) and a level
 * file itself.
 */

#include <stdio.h>
#include <stdlib.h>

#include "enigma.h"

static int ishdr(char *line, char *header) {
    return !strncmp(line, header, strlen(header));
}

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
	fatal_error_string = "File name length overflow";
	return NULL;
    }

    fp = fopen(fname, "r");
    if (!fp) {
	fatal_error_string = "Unable to read level file";
	return NULL;
    }

    level = level_new();
    level->title = NULL;
    level->width = level->height = -1;
    nlines = 0;

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    fatal_error_string = "Line length overflow in level set file";
	    return NULL;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (ishdr(buf, "Title: ")) {
	    if (level->title) {
		fatal_error_string = "Multiple titles in level file";
		return NULL;
	    }
	    level->title = dupstr(buf + 7);
	} else if (ishdr(buf, "Width: ")) {
	    level->width = atoi(buf + 7);
	} else if (ishdr(buf, "Height: ")) {
	    level->height = atoi(buf + 8);
	} else if (ishdr(buf, "Map: ")) {
	    if (level->leveldata == NULL) {
		fatal_error_string = "Map before size in level file";
		return NULL;
	    }
	    if (strlen(buf + 5) != level->width) {
		fatal_error_string = "Wrong length map line in level file";
		return NULL;
	    }
	    if (nlines >= level->height) {
		fatal_error_string = "Too many map lines in level file";
		return NULL;
	    }
	    memcpy(level->leveldata + level->width * nlines,
		   buf + 5, level->width);
	    nlines++;
	} else {
	    fatal_error_string = "Unrecognised keyword in level file";
	    return NULL;
	}
	if (level->width > 0 && level->height > 0 && level->leveldata == NULL)
	    level_setsize(level, level->width, level->height);
    }
    if (nlines < level->height) {
	fatal_error_string = "Not enough map lines in level file";
	return NULL;
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
	fatal_error_string = "File name length overflow";
	return NULL;
    }

    fp = fopen(fname, "r");
    if (!fp) {
	fatal_error_string = "Unable to read level set file";
	return NULL;
    }

    set = levelset_new();
    set->title = NULL;

    while (fgets(buf, sizeof(buf), fp)) {
	if (buf[strlen(buf)-1] != '\n') {
	    fatal_error_string = "Line length overflow in level set file";
	    return NULL;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	if (ishdr(buf, "Title: ")) {
	    if (set->title) {
		fatal_error_string = "Multiple titles in level set file";
		return NULL;
	    }
	    set->title = dupstr(buf + 7);
	} else if (ishdr(buf, "Level: ")) {
	    levelset_nlevels(set, set->nlevels+1);
	    set->levels[set->nlevels-1] = level_load(buf + 7);
	} else {
	    fatal_error_string = "Unrecognised keyword in level set file";
	    return NULL;
	}
    }

    fclose(fp);

    return set;
}
