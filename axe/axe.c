/*
 * TODO before releasable quality:
 * 
 *  - Thorough testing.
 * 
 *  - Half decent build system.
 * 
 * TODO possibly after that:
 * 
 *  - Need to handle >2Gb files! A major limiting factor here is
 *    the B-tree property size limit :-( Either malloc a long long
 *    for each tree node, or stitch together two ints in some
 *    ghastly fashion.
 *
 *  - Multiple buffers, multiple on-screen windows.
 *     + ^X^F to open new file
 *     + ^X^R to open new file RO
 *     + ^X b to switch buffers in a window
 *     + ^X o to switch windows
 *     + ^X 2 to split a window
 *     + ^X 1 to destroy all windows but this
 *     + ^X 0 to destroy this window
 *     + ^X ^ to enlarge this window by one line
 *     + width settings vary per buffer (aha, _that's_ why I wanted
 * 	 a buffer structure surrounding the raw B-tree)
 *     + axe-style minibuffer for entering search terms, rather
 * 	 than the current rather crap one; in particular this
 * 	 enables pasting into the search string.
 *     + er, how exactly do we deal with the problem of saving over
 * 	 a file which we're maintaining references to?
 * 
 *  - Undo!
 *     + this actually doesn't seem _too_ horrid. For a start, one
 * 	 simple approach would be to clone the entire buffer B-tree
 * 	 every time we perform an operation! That's actually not
 * 	 _too_ expensive, if we maintain a limit on the number of
 * 	 operations we may undo.
 *     + I had also thought of cloning the tree we insert for each
 * 	 buf_insert_data and cloning the one removed for each
 * 	 buf_delete_data (both must be cloned for an overwrite),
 * 	 but I'm not convinced that simply cloning the entire thing
 * 	 isn't a superior option.
 * 
 *  - Reverse search.
 *     + need to construct a reverse DFA.
 *     + probably should construct both every time, so that you can
 * 	 search forward for a thing and then immediately change
 * 	 your mind and search backward for the same thing.
 * 
 *  - In-place editing.
 *     + this is an extra option when running in Fix mode. It
 * 	 causes a change of semantics when saving: instead of
 * 	 constructing a new backup file and writing it over the old
 * 	 one, we simply seek within the original file and write out
 * 	 all the pieces that have changed.
 *     + Primarily useful for editing disk devices directly
 * 	 (yikes!).
 *     + I had intended to suggest that in Fix mode this would be
 * 	 nice and easy, since every element of the buffer tree is
 * 	 either a literal block (needs writing) or a from-file
 * 	 block denoting the same file _in the same position_.
 * 	 However, this is not in fact the case because you can cut
 * 	 and paste, so it's not that easy.
 *     + So I'm forced to the conclusion that when operating in
 * 	 this mode, it becomes illegal to cut and paste from-file
 * 	 blocks: they must be loaded in full at some point.
 * 	  * Thinking ahead about multiple-buffer operation: it
 * 	    would be a bad idea to keep a from-file block
 * 	    referencing /dev/hda and paste it into another ordinary
 * 	    buffer. But _also_ it would be a bad idea to paste a
 * 	    from-file block referencing a file stored _on_ /dev/hda
 * 	    into the in-place buffer dealing with /dev/hda itself.
 * 	  * So I'm forced to another odd conclusion, which is that
 * 	    from-file blocks must be eliminated in _two_ places:
 * 	    when copying a cut buffer _from_ an in-place buffer,
 * 	    _and_ when pasting a cut buffer _into_ one.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#if defined(unix) && !defined(GO32)
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(MSDOS)
#include <dos.h>
#include <process.h>
#endif

#include "axe.h"

static void init(void);
static void done(void);
static void load_file (char *);

char toprint[256];		       /* LUT: printable versions of chars */
char hex[256][3];		       /* LUT: binary to hex, 1 byte */

char message[80];

char decstatus[] = "%s <+-AXE-+> "VER": %-18.18s %s posn=%-10d size=%-10d";
char hexstatus[] = "%s <+-AXE-+> "VER": %-18.18s %s posn=0x%-8X size=0x%-8X";
char *statfmt = hexstatus;

char last_char;
char *pname;
char *filename = NULL;
buffer *filedata, *cutbuffer = NULL;
int fix_mode = FALSE;
int look_mode = FALSE;
int eager_mode = FALSE;
int insert_mode = FALSE;
int edit_type = 1;		       /* 1,2 are hex digits, 0=ascii */
int finished = FALSE;
int marking = FALSE;
int modified = FALSE;
int new_file = FALSE;		       /* shouldn't need initialisation -
					* but let's not take chances :-) */
int width = 16;
int realoffset = 0, offset = 16;

int ascii_enabled = TRUE;

long file_size = 0, top_pos = 0, cur_pos = 0, mark_point = 0;

int scrlines;

/*
 * Main program
 */
int main(int argc, char **argv) {
    int newoffset = -1, newwidth = -1;

    /*
     * Parse command line arguments
     */
    pname = *argv;		       /* program name */
    if (argc < 2) {
	fprintf(stderr,
		"usage: %s [-f] [-l] [-e] filename\n"
		"    or %s -D to write default axe.rc to stdout\n",
		pname, pname);
	return 0;
    }

    while (--argc > 0) {
	char c, *p = *++argv, *value;

	if (*p == '-') {
	    p++;
	    while (*p) switch (c = *p++) {
	      case 'o': case 'O':
	      case 'w': case 'W':
		/*
		 * these parameters require arguments
		 */
		if (*p)
		    value = p, p = "";
		else if (--argc)
		    value = *++argv;
		else {
		    fprintf(stderr, "%s: option `-%c' requires an argument\n",
			    pname, c);
		    return 1;
		}
		switch (c) {
		  case 'o': case 'O':
		    newoffset = strtol(value, NULL, 0);   /* allow `0xXX' */
		    break;
		  case 'w': case 'W':
		    newwidth = strtol(value, NULL, 0);
		    break;
		}
		break;
	      case 'f': case 'F':
		fix_mode = TRUE;
		break;
	      case 'l': case 'L':
		look_mode = TRUE;
		break;
	      case 'e': case 'E':
		eager_mode = TRUE;
		break;
	      case 'D':
		write_default_rc();
		return 0;
		break;
	    }
	} else {
	    if (filename) {
		fprintf(stderr, "%s: multiple filenames specified\n", pname);
		return 1;
	    }
	    filename = p;
	}
    }

    if (!filename) {
	fprintf(stderr, "%s: no filename specified\n", pname);
	return 1;
    }

    read_rc();
    if (newoffset != -1)
	realoffset = newoffset;
    if (newwidth != -1)
	width = newwidth;
    load_file (filename);
    init();
    fix_offset();
    do {
	draw_scr ();
	proc_key ();
    } while (!finished);
    done();

    return 0;
}

/*
 * Fix up `offset' to match `realoffset'. Also, while we're here,
 * enable or disable ASCII mode and sanity-check the width.
 */
void fix_offset(void) {
    if (3*width+11 > display_cols) {
	width = (display_cols-11) / 3;
	sprintf (message, "Width reduced to %d to fit on the screen", width);
    }
    if (4*width+14 > display_cols) {
	ascii_enabled = FALSE;
	if (edit_type == 0)
	    edit_type = 1;	       /* force to hex mode */
    } else
	ascii_enabled = TRUE;
    offset = realoffset % width;
    if (!offset)
	offset = width;
}

/*
 * Initialise stuff at the beginning of the program: mostly the
 * display.
 */
static void init(void) {
    int i;

    display_setup();

    display_define_colour(COL_BUFFER, 7, 0);
    display_define_colour(COL_SELECT, 0, 7);
    display_define_colour(COL_STATUS, 11, 4);
    display_define_colour(COL_ESCAPE, 9, 0);
    display_define_colour(COL_INVALID, 11, 0);

    for (i=0; i<256; i++) {
	sprintf(hex[i], "%02X", i);
	toprint[i] = (i>=32 && i<127 ? i : '.');
    }
}

/*
 * Clean up all the stuff that init() did.
 */
static void done(void) {
    display_cleanup();
}

/*
 * Load the file specified on the command line.
 */
static void load_file (char *fname) {
    FILE *fp;

    file_size = 0;
    if ( (fp = fopen (fname, "rb")) ) {
	if (eager_mode) {
	    long len;
	    static char buffer[4096];

	    filedata = buf_new_empty();

	    file_size = 0;

	    /*
	     * We've opened the file. Load it.
	     */
	    while ( (len = fread (buffer, 1, sizeof(buffer), fp)) > 0 ) {
		buf_insert_data (filedata, buffer, len, file_size);
		file_size += len;
	    }
	    fclose (fp);
	    assert(file_size == buf_length(filedata));
	    sprintf(message, "loaded %s (size %ld == 0x%lX).",
		    fname, file_size, file_size);
	} else {
	    filedata = buf_new_from_file(fp);
	    file_size = buf_length(filedata);
	    sprintf(message, "opened %s (size %ld == 0x%lX).",
		    fname, file_size, file_size);
	}
	new_file = FALSE;
    } else {
	if (look_mode || fix_mode) {
	    fprintf(stderr, "%s: file %s not found, and %s mode active\n",
		    pname, fname, (look_mode ? "LOOK" : "FIX"));
	    exit (1);
	}
	filedata = buf_new_empty();
	sprintf(message, "New file %s.", fname);
	new_file = TRUE;
    }
}

/*
 * Save the file. Return TRUE on success, FALSE on error.
 */
int save_file (void) {
    FILE *fp;
    long pos = 0;

    if (look_mode)
	return FALSE;		       /* do nothing! */

    if ( (fp = fopen (filename, "wb")) ) {
	static char buffer[SAVE_BLKSIZ];

	while (pos < file_size) {
	    long size = file_size - pos;
	    if (size > SAVE_BLKSIZ)
		size = SAVE_BLKSIZ;

	    buf_fetch_data (filedata, buffer, size, pos);
	    if (size != fwrite (buffer, 1, size, fp)) {
		fclose (fp);
		return FALSE;
	    }
	    pos += size;
	}
    } else
	return FALSE;
    fclose (fp);
    return TRUE;
}

/*
 * Make a backup of the file, if such has not already been done.
 * Return TRUE on success, FALSE on error.
 */
int backup_file (void) {
    char backup_name[FILENAME_MAX];

    if (new_file)
	return TRUE;		       /* unnecessary - pretend it's done */
    strcpy (backup_name, filename);
#if defined(unix) && !defined(GO32)
    strcat (backup_name, ".bak");
#elif defined(MSDOS)
    {
	char *p, *q;

	q = NULL;
	for (p = backup_name; *p; p++) {
	    if (*p == '\\')
		q = NULL;
	    else if (*p == '.')
		q = p;
	}
	if (!q)
	    q = p;
	strcpy (q, ".BAK");
    }
#endif
    remove (backup_name);	       /* don't care if this fails */
    return !rename (filename, backup_name);
}

static unsigned char *scrbuf = NULL;
static int scrbuflines = 0;

/*
 * Draw the screen, for normal usage.
 */
void draw_scr (void) {
    int scrsize, scroff, llen, i, j;
    long currpos;
    int marktop, markbot, mark;
    char *p;
    unsigned char c, *q;
    char *linebuf;

    scrlines = display_rows - 2;
    if (scrlines > scrbuflines) {
	scrbuf = (scrbuf ?
		  realloc(scrbuf, scrlines*width) :
		  malloc(scrlines*width));
	if (!scrbuf) {
	    done();
	    fprintf(stderr, "%s: out of memory!\n", pname);
	    exit (2);
	}
	scrbuflines = scrlines;
    }

    linebuf = malloc(width*4+20);
    if (!linebuf) {
	done();
	fprintf(stderr, "%s: out of memory!\n", pname);
	exit (2);
    }
    memset (linebuf, ' ', width*4+13);
    linebuf[width*4+13] = '\0';

    if (top_pos == 0)
	scroff = width - offset;
    else
	scroff = 0;
    scrsize = scrlines * width - scroff;
    if (scrsize > file_size - top_pos)
	scrsize = file_size - top_pos;

    buf_fetch_data (filedata, scrbuf, scrsize, top_pos);

    scrsize += scroff;		       /* hack but it'll work */

    mark = marking && (cur_pos != mark_point);
    if (mark) {
	if (cur_pos > mark_point)
	    marktop = mark_point, markbot = cur_pos;
	else
	    marktop = cur_pos, markbot = mark_point;
    } else
	marktop = markbot = 0;	       /* placate gcc */

    currpos = top_pos;
    q = scrbuf;

    for (i=0; i<scrlines; i++) {
	display_moveto (i, 0);
	if (currpos<=cur_pos || currpos<file_size) {
	    p = hex[(currpos >> 24) & 0xFF];
	    linebuf[0]=p[0];
	    linebuf[1]=p[1];
	    p = hex[(currpos >> 16) & 0xFF];
	    linebuf[2]=p[0];
	    linebuf[3]=p[1];
	    p = hex[(currpos >> 8) & 0xFF];
	    linebuf[4]=p[0];
	    linebuf[5]=p[1];
	    p = hex[currpos & 0xFF];
	    linebuf[6]=p[0];
	    linebuf[7]=p[1];
	    for (j=0; j<width; j++) {
		if (scrsize > 0) {
		    if (currpos == 0 && j < width-offset)
			p = "  ", c = ' ';
		    else
			p = hex[*q], c = *q++;
		    scrsize--;
		} else {
		    p = "  ", c = ' ';
		}
		linebuf[11+3*j]=p[0];
		linebuf[12+3*j]=p[1];
		linebuf[13+3*width+j]=toprint[c];
	    }
	    llen = (currpos ? width : offset);
	    if (mark && currpos<markbot && currpos+llen>marktop) {
		/*
		 * Some of this line is marked. Maybe all. Whatever
		 * the precise details, there will be two regions
		 * requiring highlighting: a hex bit and an ascii
		 * bit.
		 */
		int localstart= (currpos<marktop?marktop:currpos) - currpos;
		int localstop = (currpos+llen>markbot ? markbot :
				 currpos+llen) - currpos;
		localstart += width-llen;
		localstop += width-llen;
		display_write_chars(linebuf, 11+3*localstart);
		display_set_colour(COL_SELECT);
		display_write_chars(linebuf+11+3*localstart,
				   3*(localstop-localstart)-1);
		display_set_colour(COL_BUFFER);
		if (ascii_enabled) {
		    display_write_chars(linebuf+10+3*localstop,
				       3+3*width+localstart-3*localstop);
		    display_set_colour(COL_SELECT);
		    display_write_chars(linebuf+13+3*width+localstart,
				       localstop-localstart);
		    display_set_colour(COL_BUFFER);
		    display_write_chars(linebuf+13+3*width+localstop,
				       width-localstop);
		} else {
		    display_write_chars(linebuf+10+3*localstop,
				       2+3*width-3*localstop);
		}
	    } else
		display_write_chars(linebuf,
				   ascii_enabled ? 13+4*width : 10+3*width);
	}
	currpos += (currpos ? width : offset);
	display_clear_to_eol();
    }

    {
	char status[80];
	int slen;
	display_moveto (display_rows-2, 0);
	display_set_colour(COL_STATUS);
	sprintf(status, statfmt,
		(modified ? "**" : "  "),
		filename,
		(insert_mode ? "(Insert)" :
		 look_mode ? "(LOOK)  " :
		 fix_mode ? "(FIX)   " : "(Ovrwrt)"),
		cur_pos, file_size);
	slen = strlen(status);
	if (slen > display_cols)
	    slen = display_cols;
	display_write_chars(status, slen);
	while (slen++ < display_cols)
	    display_write_str(" ");
	display_set_colour(COL_BUFFER);
    }

    display_moveto (display_rows-1, 0);
    display_write_str (message);
    display_clear_to_eol();
    message[0] = '\0';

    i = cur_pos - top_pos;
    if (top_pos == 0)
	i += width - offset;
    j = (edit_type ? (i%width)*3+10+edit_type : (i%width)+13+3*width);
    if (j >= display_cols)
	j = display_cols-1;
    free (linebuf);
    display_moveto (i/width, j);
    display_refresh ();
}

/*
 * Get a string, in the "minibuffer". Return TRUE on success, FALSE
 * on break. Possibly syntax-highlight the entered string for
 * backslash-escapes, depending on the "highlight" parameter.
 */
int get_str (char *prompt, char *buf, int highlight) {
    int maxlen = 79 - strlen(prompt);  /* limit to 80 - who cares? :) */
    int len = 0;
    int c;

    for (EVER) {
	display_moveto (display_rows-1, 0);
	display_set_colour (COL_MINIBUF);
	display_write_str (prompt);
	if (highlight) {
	    char *q, *p = buf, *r = buf+len;
	    while (p<r) {
		q = p;
		if (*p == '\\') {
		    p++;
		    if (p<r && *p == '\\')
			p++, display_set_colour(COL_ESCAPE);
		    else if (p>=r || !isxdigit (*p))
			display_set_colour(COL_INVALID);
		    else if (p+1>=r || !isxdigit (p[1]))
			p++, display_set_colour(COL_INVALID);
		    else
			p+=2, display_set_colour(COL_ESCAPE);
		} else {
		    while (p<r && *p != '\\')
			p++;
		    display_set_colour (COL_MINIBUF);
		}
		display_write_chars (q, p-q);
	    }
	} else
	    display_write_chars (buf, len);
	display_set_colour (COL_MINIBUF);
	display_clear_to_eol();
	display_refresh();
	if (update_required)
	    update();
	safe_update = TRUE;
	c = display_getkey();
	safe_update = FALSE;
	if (c == 13 || c == 10) {
	    buf[len] = '\0';
	    return TRUE;
	} else if (c == 27 || c == 7) {
	    display_beep();
	    display_post_error();
	    strcpy (message, "User Break!");
	    return FALSE;
	}

	if (c >= 32 && c <= 126) {
	    if (len < maxlen)
		buf[len++] = c;
	    else
		display_beep();
	}

	if ((c == 127 || c == 8) && len > 0)
	    len--;

	if (c == 'U'-'@')	       /* ^U kill line */
	    len = 0;
    }
}

/*
 * Take a buffer containing possible backslash-escapes, and return
 * a buffer containing a (binary!) string. Since the string is
 * binary, it cannot be null terminated: hence the length is
 * returned from the function. The string is processed in place.
 * 
 * Escapes are simple: a backslash followed by two hex digits
 * represents that character; a doubled backslash represents a
 * backslash itself; a backslash followed by anything else is
 * invalid. (-1 is returned if an invalid sequence is detected.)
 */
int parse_quoted (char *buffer) {
    char *p, *q;

    p = q = buffer;
    while (*p) {
	while (*p && *p != '\\')
	    *q++ = *p++;
	if (*p == '\\') {
	    p++;
	    if (*p == '\\')
		*q++ = *p++;
	    else if (p[1] && isxdigit(*p) && isxdigit(p[1])) {
		char buf[3];
		buf[0] = *p++;
		buf[1] = *p++;
		buf[2] = '\0';
		*q++ = strtol(buf, NULL, 16);
	    } else
		return -1;
	}
    }
    return q - buffer;
}

/*
 * Suspend AXE. (Or shell out, depending on OS, of course.)
 */
void suspend_axe(void) {
#if defined(unix) && !defined(GO32)
    done();
    raise (SIGTSTP);
    init();
#elif defined(MSDOS)
    done();
    spawnl (P_WAIT, getenv("COMSPEC"), "", NULL);
    init();
#else
    display_beep();
    strcpy(message, "Suspend function not yet implemented.");
#endif
}

volatile int safe_update, update_required;

void update (void) {
    display_recheck_size();
    fix_offset ();
    draw_scr ();
}

void schedule_update(void) {
    if (safe_update)
	update();
    else
	update_required = TRUE;
}

long parse_num (char *buffer, int *error) {
    if (error)
	*error = FALSE;
    if (!buffer[strspn(buffer, "0123456789")]) {
	/* interpret as decimal */
	return atoi(buffer);
    } else if (buffer[0]=='0' && (buffer[1]=='X' || buffer[1]=='x') &&
	       !buffer[2+strspn(buffer+2,"0123456789ABCDEFabcdef")]) {
	return strtol(buffer+2, NULL, 16);
    } else if (buffer[0]=='$' &&
	       !buffer[1+strspn(buffer+1,"0123456789ABCDEFabcdef")]) {
	return strtol(buffer+1, NULL, 16);
    } else {
	return 0;
	if (error)
	    *error = TRUE;
    }
}
