#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <slang.h>
#include "axe.h"
#include "nca.h"

static void act_exit (void);
static void act_save (void);
static void act_exitsave (void);
static void act_top (void);
static void act_pgup (void);
static void act_up (void);
static void act_home (void);
static void act_left (void);
static void act_right (void);
static void act_end (void);
static void act_down (void);
static void act_pgdn (void);
static void act_bottom (void);
static void act_togins (void);
static void act_chmode (void);
extern void act_self_ins (void);       /* this one must be external */
static void act_delete (void);
static void act_delch (void);
static void act_mark (void);
static void act_cut (void);
static void act_copy (void);
static void act_paste (void);
static void act_susp (void);
static void act_goto (void);
static void act_togstat (void);
static void act_search (void);
static void act_recentre (void);
static void act_width (void);
static void act_offset (void);

keyact parse_action (char *name) {
    char *names[] = {
	"exit-axe", "top-of-file", "page-up", "move-up",
	"begin-line", "move-left", "move-right", "end-line",
	"move-down", "page-down", "bottom-of-file", "toggle-insert",
	"change-mode", "delete-left", "delete-right", "mark-place",
	"cut", "copy", "paste", "suspend-axe", "goto-position",
	"toggle-status", "search", "save-file", "exit-and-save",
	"screen-recentre", "new-width", "new-offset"
    };
    keyact actions[] = {
	act_exit, act_top, act_pgup, act_up, act_home, act_left,
	act_right, act_end, act_down, act_pgdn, act_bottom,
	act_togins, act_chmode, act_delete, act_delch, act_mark,
	act_cut, act_copy, act_paste, act_susp, act_goto,
	act_togstat, act_search, act_save, act_exitsave,
	act_recentre, act_width, act_offset
    };
    int i;

    for (i=0; i<sizeof(names)/sizeof(*names); i++)
	if (!strcmp(name, names[i]))
	    return actions[i];
    return NULL;
}

static int begline(int x) {
    int y = x + width-offset;
    y -= (y % width);
    y -= width-offset;
    if (y < 0)
	y = 0;
    return y;
}

static int endline(int x) {
    int y = x + width-offset;
    y -= (y % width);
    y += width-1;
    y -= width-offset;
    if (y < 0)
	y = 0;
    return y;
}

static void act_exit(void) {
    static char question[] = "File is modified. Save before quitting? [yn] ";
    if (modified) {
	int c;

	SLsmg_gotorc (SLtt_Screen_Rows-1, 0);
	SLsmg_erase_eol ();
	SLsmg_set_color (COL_MINIBUF);
	SLsmg_write_string (question);
	SLsmg_refresh();
	do {
#if defined(unix) && !defined(GO32)
	    if (update_required) {
		update();
		SLsmg_gotorc (SLtt_Screen_Rows-1, 0);
		SLsmg_erase_eol ();
		SLsmg_set_color (COL_MINIBUF);
		SLsmg_write_string (question);
		SLsmg_refresh();
	    }
	    safe_update = TRUE;
#endif
	    c = SLang_getkey();
#if defined(unix) && !defined(GO32)
	    safe_update = FALSE;
#endif
	    if (c >= 'a' && c <= 'z')
		c += 'A'-'a';
	} while (c != 'Y' && c != 'N' && c != '\007');
	if (c == 'Y') {
	    act_save();
	    if (modified)
		return;		       /* couldn't save, so don't quit */
	    draw_scr();		       /* update the ** on status line! */
	} else if (c == '\007') {
	    return;		       /* don't even quit */
	}
    }
    finished = TRUE;
}

static void act_save(void) {
    static int backed_up = FALSE;

    if (!backed_up) {
	if (!backup_file()) {
	    SLtt_beep();
	    strcpy (message, "Unable to back up file!");
	    return;
	}
	backed_up = TRUE;
    }
    if (!save_file()) {
	SLtt_beep();
	strcpy (message, "Unable to save file!");
	return;
    }
    modified = FALSE;
}

static void act_exitsave(void) {
    act_save();
    draw_scr();			       /* update ** on status line */
    act_exit();
}

static void act_top (void) {
    cur_pos = top_pos = 0;
    edit_type = !!edit_type;
}

static void act_pgup(void) {
    cur_pos -= (scrlines-1)*width;
    if (cur_pos < 0) {
	cur_pos = 0;
	edit_type = !!edit_type;
    }
    if (top_pos > cur_pos)
	top_pos = begline(cur_pos);
}

static void act_up(void) {
    cur_pos -= width;
    if (cur_pos < 0) {
	cur_pos = 0;
	edit_type = !!edit_type;
    }
    if (top_pos > cur_pos)
	top_pos = begline(cur_pos);
}

static void act_home(void) {
    cur_pos = begline(cur_pos);
    if (cur_pos < 0)
	cur_pos = 0;
    if (top_pos > cur_pos)
	top_pos = begline(cur_pos);
    edit_type = !!edit_type;
}

static void act_left(void) {
    if (edit_type == 2) {
	edit_type = 1;
	return;
    } else {
	cur_pos--;
	edit_type = 2*!!edit_type;
	if (cur_pos < 0) {
	    cur_pos = 0;
	    edit_type = !!edit_type;
	}
	if (top_pos > cur_pos)
	    top_pos = begline(cur_pos);
    }
}

static void act_right(void) {
    int new_top;

    if (edit_type == 1) {
	if (cur_pos < file_size)
	    edit_type = 2;
	return;
    } else {
	cur_pos++;
	if (cur_pos > file_size)
	    cur_pos = file_size;
	new_top = cur_pos - (scrlines-1) * width;
	if (new_top < 0)
	    new_top = 0;
	new_top = begline(new_top);
	if (top_pos < new_top)
	    top_pos = new_top;
	edit_type = !!edit_type;
    }
}

static void act_end(void) {
    int new_top;

    cur_pos = endline(cur_pos);
    edit_type = !!edit_type;
    if (cur_pos >= file_size)
	cur_pos = file_size;
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_down(void) {
    int new_top;

    cur_pos += width;
    if (cur_pos >= file_size) {
	cur_pos = file_size;
	edit_type = !!edit_type;
    }
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_pgdn(void) {
    int new_top;

    cur_pos += (scrlines-1) * width;
    if (cur_pos >= file_size) {
	cur_pos = file_size;
	edit_type = !!edit_type;
    }
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_bottom (void) {
    int new_top;

    cur_pos = file_size;
    edit_type = !!edit_type;
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_togins(void) {
    if (look_mode || fix_mode) {
	SLtt_beep();
	sprintf(message, "Can't engage Insert mode when in %s mode",
		(look_mode ? "LOOK" : "FIX"));
	insert_mode = FALSE;	       /* safety! */
    } else
	insert_mode = !insert_mode;
}

static void act_chmode(void) {
    if (ascii_enabled)
	edit_type = !edit_type;	       /* 0 -> 1, [12] -> 0 */
    else if (edit_type == 0)	       /* just in case */
	edit_type = 1;
}

void act_self_ins(void) {
    int insert = insert_mode;
    unsigned char c;

    if (look_mode) {
	SLtt_beep();
	strcpy (message, "Can't modify file in LOOK mode");
	return;
    }

    if (edit_type) {
	if (last_char >= '0' && last_char <= '9')
	    last_char -= '0';
	else if (last_char >= 'A' && last_char <= 'F')
	    last_char -= 'A'-10;
	else if (last_char >= 'a' && last_char <= 'f')
	    last_char -= 'a'-10;
	else {
	    SLtt_beep();
	    strcpy(message, "Not a valid character when in hex editing mode");
	    return;
	}
    }

    if ( (!insert || edit_type == 2) && cur_pos == file_size) {
	SLtt_beep();
	strcpy(message, "End of file reached");
	return;
    }

    switch (edit_type) {
      case 0:			       /* ascii mode */
	c = last_char;
	break;
      case 1:			       /* hex, first digit */
	if (insert)
	    c = 0;
	else
	    c = * (char *) nca_element (filedata, cur_pos);
	c &= 0xF;
	c |= 16 * last_char;
	break;
      case 2:			       /* hex, second digit */
	c = * (char *) nca_element (filedata, cur_pos);
	c &= 0xF0;
	c |= last_char;
	insert = FALSE;
	break;
    }

    if (insert) {
	nca_insert_array(filedata, &c, cur_pos, 1);
	file_size++;
	modified = TRUE;
    } else if (cur_pos < file_size) {
	nca_repl_array(filedata, &c, cur_pos, 1);
	modified = TRUE;
    } else {
	SLtt_beep();
	strcpy(message, "End of file reached");
    }
    act_right();
}

static void act_delete(void) {
    if (!insert_mode || (edit_type!=2 && cur_pos==0)) {
	SLtt_beep();
	strcpy (message, "Can't delete while not in Insert mode");
    } else if (cur_pos > 0 || edit_type == 2) {
	act_left();
	nca_delete (filedata, cur_pos, 1);
	file_size--;
	edit_type = !!edit_type;
	modified = TRUE;
    }
}

static void act_delch(void) {
    if (!insert_mode) {
	SLtt_beep();
	strcpy (message, "Can't delete while not in Insert mode");
    } else if (cur_pos < file_size) {
	nca_delete (filedata, cur_pos, 1);
	file_size--;
	edit_type = !!edit_type;
	modified = TRUE;
    }
}

static void act_mark (void) {
    if (look_mode) {
	SLtt_beep();
	strcpy (message, "Can't cut or paste in LOOK mode");
	marking = FALSE;	       /* safety */
	return;
    }
    marking = !marking;
    mark_point = cur_pos;
}

static void act_cut (void) {
    long marktop, marksize;

    if (!marking || mark_point==cur_pos) {
	SLtt_beep();
	strcpy (message, "Set mark first");
	return;
    }
    if (!insert_mode) {
	SLtt_beep();
	strcpy (message, "Can't cut while not in Insert mode");
	return;
    }
    marktop = cur_pos;
    marksize = mark_point - cur_pos;
    if (marksize < 0) {
	marktop += marksize;
	marksize = -marksize;
    }
    if (cutbuffer)
	nca_free (cutbuffer);
    cutbuffer = nca_cut (filedata, marktop, marksize);
    file_size -= marksize;
    cur_pos = marktop;
    if (cur_pos < 0)
	cur_pos = 0;
    if (top_pos > cur_pos)
	top_pos = begline(cur_pos);
    edit_type = !!edit_type;
    modified = TRUE;
    marking = FALSE;
}

static void act_copy (void) {
    int marktop, marksize;

    if (!marking) {
	SLtt_beep();
	strcpy (message, "Set mark first");
	return;
    }
    marktop = cur_pos;
    marksize = mark_point - cur_pos;
    if (marksize < 0) {
	marktop += marksize;
	marksize = -marksize;
    }
    if (cutbuffer)
	nca_free (cutbuffer);
    cutbuffer = nca_copy (filedata, marktop, marksize);
    marking = FALSE;
}

static void act_paste (void) {
    NCA temp;
    int cutsize, new_top;

    cutsize = nca_size (cutbuffer);
    temp = nca_copy (cutbuffer, 0, cutsize);
    if (!insert_mode) {
	if (cur_pos + cutsize > file_size) {
	    SLtt_beep();
	    strcpy (message, "Too close to end of file to paste");
	    return;
	}
	nca_delete (filedata, cur_pos, cutsize);
	file_size -= cutsize;
    }
    nca_merge (filedata, temp, cur_pos);
    modified = TRUE;
    cur_pos += cutsize;
    file_size += cutsize;
    edit_type = !!edit_type;
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_susp (void) {
    suspend_axe();
}

static void act_goto (void) {
    char buffer[80];
    long position, new_top;
    int error;

    if (!get_str("Enter position to go to: ", buffer, FALSE))
	return;			       /* user break */

    position = parse_num (buffer, &error);
    if (error) {
	SLtt_beep();
	strcpy (message, "Unable to parse position value");
	return;
    }

    if (position < 0 || position > file_size) {
	SLtt_beep();
	strcpy (message, "Position is outside bounds of file");
	return;
    }

    cur_pos = position;
    edit_type = !!edit_type;
    new_top = cur_pos - (scrlines-1) * width;
    if (new_top < 0)
	new_top = 0;
    new_top = begline(new_top);
    if (top_pos > cur_pos)
	top_pos = begline(cur_pos);
    if (top_pos < new_top)
	top_pos = new_top;
}

static void act_togstat (void) {
    if (statfmt == decstatus)
	statfmt = hexstatus;
    else
	statfmt = decstatus;
}

static void act_search (void) {
    char buffer[80];
    int len, posn, dfapos;
    DFA dfa;
    static unsigned char sblk[SEARCH_BLK];
    static char withdef[] = "Search for (default=last): ";
    static char withoutdef[] = "Search for: ";

    dfa = last_dfa();

    if (!get_str(dfa ? withdef : withoutdef, buffer, TRUE))
	return;			       /* user break */
    if (!dfa && !*buffer) {
	strcpy (message, "Search aborted.");
	return;
    }

    if (!*buffer) {
	len = last_len();
    } else {
	len = parse_quoted (buffer);
	if (len == -1) {
	    SLtt_beep();
	    strcpy (message, "Invalid escape sequence in search string");
	    return;
	}
	dfa = build_dfa (buffer, len);
    }

    dfapos = 0;

    for (posn = cur_pos+1; posn < file_size; posn++) {
	unsigned char *q;
	int size = SEARCH_BLK;

	if (size > file_size-posn)
	    size = file_size-posn;
	nca_get_array (sblk, filedata, posn, size);
	q = sblk;
	while (size--) {
	    posn++;
	    dfapos = dfa[dfapos][*q++];
	    if (dfapos == len) {
		int new_top;

		cur_pos = posn - len;
		edit_type = !!edit_type;
		new_top = cur_pos - (scrlines-1) * width;
		new_top = begline(new_top);
		if (top_pos < new_top)
		    top_pos = new_top;
		return;
	    }
	}
    }
    strcpy (message, "Not found.");
}

static void act_recentre (void) {
    top_pos = cur_pos - (SLtt_Screen_Rows-2)/2 * width;
    if (top_pos < 0)
	top_pos = 0;
    top_pos = begline(top_pos);
    SLsmg_touch_lines (0, SLtt_Screen_Rows);
}

static void act_width (void) {
    char buffer[80];
    char prompt[80];
    long w;
    long new_top;
    int error;

    sprintf (prompt, "Enter screen width in bytes (now %d): ", width);
    if (!get_str (prompt, buffer, FALSE))
	return;
    w = parse_num (buffer, &error);
    if (error) {
	SLtt_beep();
	strcpy (message, "Unable to parse width value");
	return;
    }
    if (w > 0) {
	width = w;
	fix_offset();
	new_top = cur_pos - (scrlines-1) * width;
	new_top = begline(new_top);
	if (top_pos < new_top)
	    top_pos = new_top;
    }
}

static void act_offset (void) {
    char buffer[80];
    char prompt[80];
    long o;
    long new_top;
    int error;

    sprintf (prompt, "Enter start-of-file offset in bytes (now %d): ",
	     realoffset);
    if (!get_str (prompt, buffer, FALSE))
	return;
    o = parse_num (buffer, &error);
    if (error) {
	SLtt_beep();
	strcpy (message, "Unable to parse offset value");
	return;
    }
    if (o >= 0) {
	realoffset = o;
	fix_offset();
	new_top = cur_pos - (scrlines-1) * width;
	new_top = begline(new_top);
	if (top_pos < new_top)
	    top_pos = new_top;
    }
}
