#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(unix) && !defined(GO32)
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(MSDOS)
#include <dos.h>
#include <process.h>
#endif

#include <slang.h>

#include "axe.h"
#include "nca.h"

static void init(void);
static void done(void);
static void load_file (char *);
static void get_screen_size (void);
#if defined(unix) && !defined(GO32)
static int sigwinch (int);
#endif

char toprint[256];		       /* LUT: printable versions of chars */
char hex[256][3];		       /* LUT: binary to hex, 1 byte */

char message[80];

char decstatus[] = "%s <+-AXE-+> "VER": %-18.18s %s posn=%-10d size=%-10d";
char hexstatus[] = "%s <+-AXE-+> "VER": %-18.18s %s posn=0x%-8X size=0x%-8X";
char *statfmt = hexstatus;

char last_char;
char *pname;
char *filename = NULL;
NCA filedata, cutbuffer = NULL;
int fix_mode = FALSE;
int look_mode = FALSE;
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
		"usage: %s [-f] [-l] filename\n"
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
		    newoffset = atoi(value);
		    break;
		  case 'w': case 'W':
		    newwidth = atoi(value);
		    break;
		}
		break;
	      case 'f': case 'F':
		fix_mode = TRUE;
		break;
	      case 'l': case 'L':
		look_mode = TRUE;
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

    filedata = nca_new (NCA_MINBLK, 1);

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
    if (3*width+11 > SLtt_Screen_Cols) {
	width = (SLtt_Screen_Cols-11) / 3;
	sprintf (message, "Width reduced to %d to fit on the screen", width);
    }
    if (4*width+14 > SLtt_Screen_Cols) {
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
 * SLang terminal and screen management library.
 */
static void init(void) {
    int i;

    SLtt_get_terminfo();

    if (SLang_init_tty (ABORT, 1, 0) == -1) {
	fprintf(stderr, "axe: SLang_init_tty: returned error code\n");
	exit (1);
    }
    SLang_set_abort_signal (NULL);

    get_screen_size ();
    SLtt_Use_Ansi_Colors = TRUE;
#if defined(unix) && !defined(GO32)
    signal (SIGWINCH, (void *) sigwinch);
#endif

    if (SLsmg_init_smg () < 0) {
	fprintf(stderr, "axe: SLsmg_init_smg: returned error code\n");
	SLang_reset_tty ();
	exit (1);
    }

    SLtt_set_color (COL_BUFFER, "buffer", "lightgray", "black");
    SLtt_set_color (COL_SELECT, "select", "black", "lightgray");
    SLtt_set_color (COL_STATUS, "status", "yellow", "blue");
    SLtt_set_color (COL_ESCAPE, "escape", "brightred", "black");
    SLtt_set_color (COL_INVALID, "invalid", "yellow", "black");

    for (i=0; i<256; i++) {
	sprintf(hex[i], "%02X", i);
	toprint[i] = (i>=32 && i<127 ? i : '.');
    }
}

/*
 * Clean up all the stuff that init() did.
 */
static void done(void) {
    SLsmg_reset_smg ();
    SLang_reset_tty ();
}

/*
 * Load the file specified on the command line.
 */
static void load_file (char *fname) {
    FILE *fp;

    file_size = 0;
    if ( (fp = fopen (fname, "rb")) ) {
	long len;
	static char buffer[NCA_MINBLK*3/2];
	/*
	 * We've opened the file. Load it.
	 */
	while ( (len = fread (buffer, 1, sizeof(buffer), fp)) ) {
	    nca_insert_array (filedata, buffer, file_size, len);
	    file_size += len;
	}
	fclose (fp);
	sprintf(message, "loaded %s (size %ld == 0x%lX).",
		fname, file_size, file_size);
	new_file = FALSE;
    } else {
	if (look_mode || fix_mode) {
	    fprintf(stderr, "%s: file %s not found, and %s mode active\n",
		    pname, fname, (look_mode ? "LOOK" : "FIX"));
	    exit (1);
	}
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

	    nca_get_array (buffer, filedata, pos, size);
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

    scrlines = SLtt_Screen_Rows - 2;
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

    nca_get_array (scrbuf, filedata, top_pos, scrsize);

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
	SLsmg_gotorc (i, 0);
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
		SLsmg_write_nchars(linebuf, 11+3*localstart);
		SLsmg_set_color(1);
		SLsmg_write_nchars(linebuf+11+3*localstart,
				   3*(localstop-localstart)-1);
		SLsmg_set_color(0);
		if (ascii_enabled) {
		    SLsmg_write_nchars(linebuf+10+3*localstop,
				       3+3*width+localstart-3*localstop);
		    SLsmg_set_color(1);
		    SLsmg_write_nchars(linebuf+13+3*width+localstart,
				       localstop-localstart);
		    SLsmg_set_color(0);
		    SLsmg_write_nchars(linebuf+13+3*width+localstop,
				       width-localstop);
		} else {
		    SLsmg_write_nchars(linebuf+10+3*localstop,
				       2+3*width-3*localstop);
		}
	    } else
		SLsmg_write_nchars(linebuf,
				   ascii_enabled ? 13+4*width : 10+3*width);
	}
	currpos += (currpos ? width : offset);
	SLsmg_erase_eol();
    }

    {
	char status[80];
	SLsmg_gotorc (SLtt_Screen_Rows-2, 0);
	SLsmg_set_color(2);
	sprintf(status, statfmt,
		(modified ? "**" : "  "),
		filename,
		(insert_mode ? "(Insert)" :
		 look_mode ? "(LOOK)  " :
		 fix_mode ? "(FIX)   " : "(Ovrwrt)"),
		cur_pos, file_size);
	SLsmg_write_nstring (status, SLtt_Screen_Cols);
	SLsmg_set_color(0);
    }

    SLsmg_gotorc (SLtt_Screen_Rows-1, 0);
    SLsmg_write_string (message);
    SLsmg_erase_eol();
    message[0] = '\0';

    i = cur_pos - top_pos;
    if (top_pos == 0)
	i += width - offset;
    j = (edit_type ? (i%width)*3+10+edit_type : (i%width)+13+3*width);
    if (j >= SLtt_Screen_Cols)
	j = SLtt_Screen_Cols-1;
    free (linebuf);
    SLsmg_gotorc (i/width, j);
    SLsmg_refresh ();
}

/*
 * Get the screen size.
 */
static void get_screen_size (void) {
    int r = 0, c = 0;

#ifdef TIOCGWINSZ
    struct winsize wind_struct;

    if ((ioctl(1,TIOCGWINSZ,&wind_struct) == 0)
	|| (ioctl(0, TIOCGWINSZ, &wind_struct) == 0)
	|| (ioctl(2, TIOCGWINSZ, &wind_struct) == 0)) {
        c = (int) wind_struct.ws_col;
        r = (int) wind_struct.ws_row;
    }
#elif defined(MSDOS)
    union REGS regs;

    regs.h.ah = 0x0F;
    int86 (0x10, &regs, &regs);
    c = regs.h.ah;

    regs.x.ax = 0x1130, regs.h.bh = 0;
    int86 (0x10, &regs, &regs);
    r = regs.h.dl + 1;
#endif

    if ((r <= 0) || (r > 200)) r = 24;
    if ((c <= 0) || (c > 250)) c = 80;
    SLtt_Screen_Rows = r;
    SLtt_Screen_Cols = c;
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
	SLsmg_gotorc (SLtt_Screen_Rows-1, 0);
	SLsmg_set_color (COL_MINIBUF);
	SLsmg_write_string (prompt);
	if (highlight) {
	    char *q, *p = buf, *r = buf+len;
	    while (p<r) {
		q = p;
		if (*p == '\\') {
		    p++;
		    if (p<r && *p == '\\')
			p++, SLsmg_set_color(COL_ESCAPE);
		    else if (p>=r || !isxdigit (*p))
			SLsmg_set_color(COL_INVALID);
		    else if (p+1>=r || !isxdigit (p[1]))
			p++, SLsmg_set_color(COL_INVALID);
		    else
			p+=2, SLsmg_set_color(COL_ESCAPE);
		} else {
		    while (p<r && *p != '\\')
			p++;
		    SLsmg_set_color (COL_MINIBUF);
		}
		SLsmg_write_nchars (q, p-q);
	    }
	} else
	    SLsmg_write_nchars (buf, len);
	SLsmg_set_color (COL_MINIBUF);
	SLsmg_erase_eol();
	SLsmg_refresh();
#if defined(unix) && !defined(GO32)
	if (update_required)
	    update();
	safe_update = TRUE;
#endif
	c = SLang_getkey();
#if defined(unix) && !defined(GO32)
	safe_update = FALSE;
#endif
	if (c == 13) {
	    buf[len] = '\0';
	    return TRUE;
	} else if (c == 27 || c == 7) {
	    SLtt_beep();
	    SLKeyBoard_Quit = 0;
	    SLang_Error = 0;
	    strcpy (message, "User Break!");
	    return FALSE;
	}

	if (c >= 32 && c <= 126) {
	    if (len < maxlen)
		buf[len++] = c;
	    else
		SLtt_beep();
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
    SLtt_beep();
    strcpy(message, "Suspend function not yet implemented.");
#endif
}

#if defined(unix) && !defined(GO32)
volatile int safe_update, update_required;

void update (void) {
    SLsmg_reset_smg ();
    get_screen_size ();
    fix_offset ();
    SLsmg_init_smg ();
    draw_scr ();
}

static int sigwinch (int sigtype) {
    if (safe_update)
	update();
    else
	update_required = TRUE;
    signal (SIGWINCH, (void *) sigwinch);
    return 0;
}
#endif

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
