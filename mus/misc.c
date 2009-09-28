#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mus.h"
#include "drawing.h"
#include "header.h"
#include "measure.h"

/* sparse -- find string s in string array sa 
 *	(0,n-1)	matched to string
 *	-1	no match
 */
int sparse(const char *s,const char *const *sa) {
    int ptr = 0;

    while (sa[ptr]) {
	if (!strcmp(s, sa[ptr]))
	    return ptr;		       /* got one */
	ptr++;
    }
    return -1;			       /* no matches */
}

/* lcm - find lowest common multiple of given arguments */
int lcm (int a, int b) {
    int p = a, q = b;

    if (q > p)
	q = a, p = b;

    do {
	int temp = q;
	q = p % q;
	p = temp;
    } while (q);

    return (a / p) * b;
}

/* parse a time signature into its integer representation */
int parse_time_sig (char *buf) {
    char *p;

    /* special cases: C and CBAR */
    if (!strcmp(buf, "C"))
	return TIME_C;
    if (!strcmp(buf, "CBAR"))
	return TIME_CBAR;

    /* there should be exactly one / in the string */
    p = strchr(buf, '/');
    if (!p)			       /* not even one */
	return -1;
    if (p != strrchr(buf, '/'))	       /* more than one */
	return -1;

    /* the string should not contain anything other than
     * digits and the aforementioned / character */
    if (strspn(buf, "0123456789/") != strlen(buf))
	return -1;

    /* so now split the string in two at the slash */
    *p++ = '\0';

    return (atoi(buf) << 8) | atoi(p);
}

/* parse a key signature into its integer representation */
int parse_key_sig (char *word1, char *word2) {
    static const char *const keysigs[] = {
	"CB MAJOR", "AB MINOR", "7 FLATS",
	"GB MAJOR", "EB MINOR", "6 FLATS",
	"DB MAJOR", "BB MINOR", "5 FLATS",
	"AB MAJOR", "F MINOR",  "4 FLATS",
	"EB MAJOR", "C MINOR",  "3 FLATS",
	"BB MAJOR", "G MINOR",  "2 FLATS",
	"F MAJOR",  "D MINOR",  "1 FLAT",
	"C MAJOR",  "A MINOR",  "",    /* no alternative words */
	"G MAJOR",  "E MINOR",  "1 SHARP",
	"D MAJOR",  "B MINOR",  "2 SHARPS",
	"A MAJOR",  "F# MINOR", "3 SHARPS",
	"E MAJOR",  "C# MINOR", "4 SHARPS",
	"B MAJOR",  "G# MINOR", "5 SHARPS",
	"F# MAJOR", "D# MINOR", "6 SHARPS",
	"C# MAJOR", "A# MINOR", "7 SHARPS",
	0
    };
    char buffer[20];		       /* not an arbitrary limit */
    int i;

    if (strlen(word1)>2 || strlen(word2)>6)
	return -1;

    strcpy (buffer, word1);
    strcat (buffer, " ");
    strcat (buffer, word2);
    i = sparse (buffer, keysigs);

    return (i < 0 ? -1 : i/3);
}

int bar_len (int timesig) {
    if (timesig == TIME_C || timesig == TIME_CBAR)
	return 64;
    else
	return (((timesig >> 8) & 0xFF) * 64 / (timesig & 0xFF));
}

/* return the Y coordinate of the top/middle/bottom of a stave;
 * of course the staves are numbered from zero in this routine */
int stave_y (int stave, int posn) {
    int i;
    int ret = y;

    if (stave < max_staves && staves[stave].clef != CLEF_LINE)
	ret -= posn * 2 * nh;

    for (i=0; i<stave; i++)
	ret -= stave_spacing + (staves[i].clef == CLEF_LINE ? 0 : 4*nh);

    return ret;
}
