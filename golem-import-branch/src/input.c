/*
 * input.c    input analysis module for Golem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "golem.h"

/* FIXME - put these in sensible places... */
/* Make a data struct, containing this string */
void *make_data_str(char *p) {
  Data *data = malloc(sizeof(Data));
  data->type = DATA_STR;
  strncpy(data->string.data, p, MAXSTR);
  return data;
}
void *make_data_int(int v) {
  Data *data = malloc(sizeof(Data));
  data->type = DATA_INT;
  data->integer.value = v;
  return data;
}
/* Push a data item onto the stack */
char * thread_stack_push(Thread *thread, Data *data) {
  if(thread->sp >= thread->stacklimit) {
    return "Stack overflow.";
  }
  thread->stack[thread->sp++] = data;
  return NULL;
}
/* thread_stack_pop_str - pop a string from stack
 * If there's a string on the top of the stack, copies it into dest,
 * which is assumed to have space for MAXSTR characters.  Otherwise,
 * clears dest.
 * Returns: NULL if string copied, error string if wasn't a string there
 */
char *thread_stack_pop_str(char *dest, Thread *thread) {
  if (thread->sp > 0 &&
      thread->stack[thread->sp - 1]->type == DATA_STR) {
    strncpy(dest, thread->stack[thread->sp - 1]->string.data, MAXSTR);
    dest[MAXSTR - 1] = '\0';
  } else {
    dest[0] = '\0';
    return thread->sp > 0 ?
	"Incorrect type on stack - expected string" :
	"Empty stack - expected string";
  }
  free (thread->stack[thread->sp - 1]);
  thread->sp--;
  return NULL;
}
int is_account_char(char c) {
  if((c >= 97 && c <= 122) ||
     (c >= '0' && c <= '9') ||
     (c == '-'))
	return TRUE;
  return FALSE;
}

extern int rfd, wfd;

static void proc_char (int);

static enum {
    READING, MESSAGES, ALLMSGS, MENU, UTILS, SCANNING, KNOWNUSERS, ERRORS,
    MOD_NAME, EXAMINING, EDITOR, EDITNAME, TALKER
} monostate;

static void check_for_msg_line (unsigned long *);
static void trim_trailing_spaces (unsigned long *);
static void write_attr_string (unsigned long *, FILE *);

static FILE *outputfp = NULL;

static char editvscr[256*256];	       /* virtual screen for raw editing */
static char editvline[256];	       /* the relevant line thereof */
static int editvlen;		       /* line length */

void main_loop (void) {
    unsigned char buffer[1024];
    int len, i;

    while ( (len = read (rfd, buffer, sizeof(buffer))) > 0) {
	for (i=0; i<len; i++)
	    proc_char (buffer[i]);
    }

    if (len == 0) {
	fprintf(stderr, "golem: unexpected EOF on stdin. Terminating\n");
	exit(1);
    } else if (len < 0) {
	perror ("golem: read");
	exit(1);
    }
}

static enum {
    TOPLEVEL, SEEN_ESC, SEEN_CSI, SEEN_OSC, OSC_STRING
} termstate;

#define ARG_DEFAULT -1
#define ARGS_MAX 20
#define def(a,d) ( (a) == ARG_DEFAULT ? (d) : (a) )
static int esc_nargs, esc_args[ARGS_MAX];

#define OSC_STR_MAX 512
static int osc_strlen;
static char osc_string[OSC_STR_MAX+1];

static unsigned long curr_attr;
static int cursor_y, cursor_x;
static int just_positioned = FALSE;
static int talker_ignorestr = FALSE;

#define STRING_MAX 512
static unsigned long string[STRING_MAX];
static int stringlen;

#define ATTR_BOLD    0x00000100UL
#define ATTR_UNDER   0x00000200UL
#define ATTR_REVERSE 0x00000400UL
#define ATTR_FLASH   0x00000800UL
#define ATTR_FGMASK  0x00007000UL
#define ATTR_BGMASK  0x00070000UL
#define ATTR_FGSHIFT 12
#define ATTR_BGSHIFT 16
#define ATTR_DEFAULT 0x00007000UL
#define ATTR_MASK    0xFFFFFF00UL
#define CHAR_MASK    0x000000FFUL

#define MESSAGE_SEP  0x0004752DUL   /* message separator: attrs and char */
#define BLANK_SPACE  0x00007020UL   /* blank space: default attrs plus char */

static void clear_str (int even_if_blank, int with_crlf) {
#ifdef DIAGNOSTIC
    int i;
#endif
    if(talker_ignorestr) {
#ifdef DIAGNOSTIC
    fprintf(dfp, "Ignoring talker string output: `");
    for (i=0; i<stringlen; i++)
        fputc(string[i] & CHAR_MASK, dfp);
    fprintf(dfp, "'\n");
#endif
	talker_ignorestr = FALSE;
	stringlen = 0;
	return;
    }

    if (!stringlen && !even_if_blank)
	return;
#ifdef DIAGNOSTIC
    fprintf(dfp, "String output: `");
    for (i=0; i<stringlen; i++)
	fputc(string[i] & CHAR_MASK, dfp);
    fprintf(dfp, "'\n");
#endif
    if (cursor_y == 255 && just_positioned) {
	char buf[STRING_MAX];
	int i;

	for (i=0; i<stringlen; i++)
	    buf[i] = (char) string[i] & CHAR_MASK;
	buf[stringlen] = '\0';

	event (BOTTOM_LINE, buf);
	string[stringlen] = 0;
	event (BSTR, string);
    } else {
	string[stringlen] = 0;
	event (with_crlf ? CRLFSTRING : STRING, string);
    }

    stringlen = 0;
    just_positioned = FALSE;
}

static void proc_char (int c) {
    static int cr_last = FALSE, new_cr_last;
    int i;

    /*
     * Ignore successive CRs.
     */
    if (termstate == TOPLEVEL && cr_last == 13 && c == 13)
	return;

    if (termstate == TOPLEVEL && cr_last && c != 23-cr_last) {
	if (cr_last == 10) {
	    cursor_y--;		       /* temporary */
	    clear_str(TRUE, TRUE);
	    cursor_y++;
	} else {
	    clear_str(FALSE, FALSE);
	}
    }

    new_cr_last = 0;

    switch (termstate) {
      case TOPLEVEL:
	do_toplevel:
	switch (c) {
	  case '\033':
	    termstate = SEEN_ESC;
	    break;
	  case 0233:
	    termstate = SEEN_CSI;
	    esc_nargs = 1;
	    esc_args[0] = ARG_DEFAULT;
	    break;
	  case 0235:
	    termstate = SEEN_OSC;
	    esc_args[0] = 0;
	    break;
	  case '\007':
	    clear_str(FALSE, FALSE);
	    fprintf(dfp, "Bleep!\n");
	    event(BLEEP, NULL);
	    break;
	  case 13:
	    if (cr_last == 10) {
		new_cr_last = 0;
		clear_str(TRUE, TRUE);
	    } else {
		new_cr_last = 13;
	    }
	    cursor_x = 0;
#ifdef DIAGNOSTIC
	    fprintf(dfp, "CR\n");
#endif
	    break;
	  case 10:
	    if (cr_last == 13) {
		new_cr_last = 0;
		clear_str(TRUE, TRUE);
	    } else {
		new_cr_last = 10;
	    }
#ifdef DIAGNOSTIC
	    fprintf(dfp, "LF\n");
#endif
	    cursor_y++;
	    cursor_x = 0;
	    break;
	  case 8:
	    clear_str(FALSE, FALSE);
#ifdef DIAGNOSTIC
	    fprintf(dfp, "BS\n");
#endif
	  default:
	    if (c >= ' ' && c <= '~' && stringlen < STRING_MAX) {
		string[stringlen++] = curr_attr | (c & CHAR_MASK);
		cursor_x++;
	    }
	    /*
	     * Nasty heuristic for talker:
	     * If cursor_x == 0, and we get a ',; or " character, then
	     * flush the string, because we're at a prompt.
	     * There'll be a load of gubbins to follow though, unfortunately,
	     * so we have to ignore this: FIXME - do this.
	     */
	    if (monostate == TALKER && cursor_x == 1 &&
		(c == '\'' || c == '"' || c == '.' || c == ';')) {
#ifdef DIAGNOSTIC
		fprintf(dfp, "Found talker prompt - flushing string\n");
#endif
		clear_str(FALSE,FALSE);
		talker_ignorestr = TRUE;
	    }
	    /*
	     * Nasty heuristic: bottom-line strings must terminate
	     * somewhere, and they _seem_ to terminate in a bracket
	     * (such as `(55%)') or a period (on Continue screens
	     * and menus). Exception is `[.]:Edit' in the middle,
	     * so we keep track of the previous character as
	     * well... yeuck.
	     *
	     * Further hack: we only do this if an ESC[H has _just_
	     * put the cursor on the bottom line. This is to cope
	     * with the behaviour when scrolling to the next
	     * screenful in a file. `just_positioned' is set by
	     * ESC[H and cleared by clear_str().
	     */
	    if ((c == ')' ||
		 (c == '.' && (string[stringlen-2]&CHAR_MASK) != '['))
		&& cursor_y == 255 && just_positioned)
		clear_str(FALSE, FALSE);
	}
	break;
      case SEEN_ESC:
	switch (c) {
	  case '\007': case '\033': case 0233: case 0235:
	    termstate = TOPLEVEL;
	    goto do_toplevel;	       /* hack... */
	  case '[':		       /* enter CSI mode */
	    termstate = SEEN_CSI;
	    esc_nargs = 1;
	    esc_args[0] = ARG_DEFAULT;
	    break;
	  case ']':		       /* xterm escape sequences */
	    termstate = SEEN_OSC;
	    esc_args[0] = 0;
	    break;
	  default:
	    termstate = TOPLEVEL;
	    clear_str(FALSE, FALSE);
	    fprintf(dfp, "ESC '\\x%02X' unusual?\n", c);
	    break;
	}
	break;
      case SEEN_CSI:
	termstate = TOPLEVEL;	       /* the default */
	switch (c) {
	  case '\007': case '\033': case 0233: case 0235:
	    termstate = TOPLEVEL;
	    goto do_toplevel;	       /* hack... */
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    if (esc_nargs <= ARGS_MAX) {
		if (esc_args[esc_nargs-1] == ARG_DEFAULT)
		    esc_args[esc_nargs-1] = 0;
		esc_args[esc_nargs-1] =
		    10 * esc_args[esc_nargs-1] + c - '0';
	    }
	    termstate = SEEN_CSI;
	    break;
	  case ';':
	    if (++esc_nargs <= ARGS_MAX)
		esc_args[esc_nargs-1] = ARG_DEFAULT;
	    termstate = SEEN_CSI;
	    break;
	  case '?':
	    termstate = SEEN_CSI;
	    break;
	  case 'H': case 'f':	       /* set cursor position */
	    if (esc_nargs < 2)
		esc_args[1] = ARG_DEFAULT;
	    clear_str(FALSE, FALSE);
	    just_positioned = TRUE;
#ifdef DIAGNOSTIC
	    fprintf(dfp, "Cursor position to %d,%d\n",
		   def(esc_args[1], 1) - 1, def(esc_args[0], 1) - 1);
#endif
	    cursor_x = def(esc_args[1], 1) - 1;
	    cursor_y = def(esc_args[0], 1) - 1;
	    event(CURSOR_Y, NULL);
	    break;
	  case 'J':		       /* erase screen or parts of it */
	    clear_str(FALSE, FALSE);
#ifdef DIAGNOSTIC
	    fprintf(dfp, "Clear screen\n");
#endif
	    break;
	  case 'K':		       /* erase line or parts of it */
	    clear_str(FALSE, FALSE);
#ifdef DIAGNOSTIC
	    fprintf(dfp, "Clear part of line\n");
#endif
	    break;
	  case 'L':		       /* ANSI insert line */
	    i = (esc_nargs > 0 ? def(esc_args[0], 1) : 1);
	    while (i--)
		event(ANSI_INSLINE, NULL);
	    break;
	  case 'M':		       /* ANSI delete line */
	    i = (esc_nargs > 0 ? def(esc_args[0], 1) : 1);
	    while (i--)
		event(ANSI_DELLINE, NULL);
	    break;
	  case 'm':		       /* set graphics rendition */
	    {
		int i;
		for (i=0; i<esc_nargs; i++) {
		    switch (def(esc_args[i], 0)) {
		      case 0:	       /* restore defaults */
			curr_attr = ATTR_DEFAULT; break;
		      case 1:	       /* enable bold */
			curr_attr |= ATTR_BOLD; break;
		      case 4:	       /* enable underline */
		      case 21:	       /* (enable double underline) */
			curr_attr |= ATTR_UNDER; break;
		      case 5:	       /* enable flash */
			curr_attr |= ATTR_FLASH; break;
		      case 7:	       /* enable reverse video */
			curr_attr |= ATTR_REVERSE; break;
		      case 22:	       /* disable bold */
			curr_attr &= ~ATTR_BOLD; break;
		      case 24:	       /* disable underline */
			curr_attr &= ~ATTR_UNDER; break;
		      case 25:	       /* disable flash */
			curr_attr &= ~ATTR_FLASH; break;
		      case 27:	       /* disable reverse video */
			curr_attr &= ~ATTR_REVERSE; break;
		      case 30: case 31: case 32: case 33:
		      case 34: case 35: case 36: case 37:
			/* foreground */
			curr_attr &= ~ATTR_FGMASK;
			curr_attr |= (esc_args[i] - 30) << ATTR_FGSHIFT;
			break;
		      case 40: case 41: case 42: case 43:
		      case 44: case 45: case 46: case 47:
			/* background */
			curr_attr &= ~ATTR_BGMASK;
			curr_attr |= (esc_args[i] - 40) << ATTR_BGSHIFT;
			break;
		    }
		}
	    }
	    break;
	}
	break;
      case SEEN_OSC:
	switch (c) {
	  case '\007': case '\033': case 0233: case 0235:
	    termstate = TOPLEVEL;
	    goto do_toplevel;	       /* hack... */
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    esc_args[0] = 10 * esc_args[0] + c - '0';
	    break;
	  default:
	    termstate = OSC_STRING;
	    osc_strlen = 0;
	}
	break;
      case OSC_STRING:
	if (c == '\007') {
	    osc_string[osc_strlen] = '\0';
	    clear_str(FALSE, FALSE);
	    fprintf(dfp, "Title set to `%s'\n", osc_string);
	    event (TITLE_SET, osc_string);
	    termstate = TOPLEVEL;
	} else if (osc_strlen < OSC_STR_MAX) {
	    osc_string[osc_strlen++] = c;
	}
	break;
    }
    cr_last = new_cr_last;
}

/* Talker output parsing functions */
/* I'm going to bother to write vaguely maintainable code, even if Simon isn't. :) */

/* talker_parseaccname()
 * Parse line, copying the name at the beginning of "line" to "user",
 * and returning a pointer to the end of the name in "line"
 */
char *talker_parse_accname(char *user, char *line) {
    int limit = MAXACC;
    while(is_account_char(*line) && limit--) {
	*user++ = *line++;
    }
    *user = '\0';
    return line;
}

/* talker_parse_sysmsg()
 * Parse system messages from the talker
 * (eg, user enters/leaves...)
 */
/* FIXME - understand more of these messages */
int talker_parse_sysmsg(char *line, char *user) {
    int type = 20;
    if(!strncmp(line, " <+ ", 4)) {
	line += 4;
	line = talker_parse_accname(user, line);
	if(*user == '\0') {
	    fprintf(stderr,
		    "account name missing in enter room message (<+ %s)\n",
		    line);
	}
	if(!strcmp(line, " enters the talker +>"))
	    type = 21;
	else
	    type = 22;
    }
    if(!strncmp(line, " <- ", 4)) {
	line += 4;
	line = talker_parse_accname(user, line);
	if(*user == '\0') {
	    fprintf(stderr,
		    "account name missing in leave room message (<- %s)\n",
		    line);
	}
	if(!strcmp(line, " leaves the talker ->"))
	    type = 25;
	else
	    type = 26;
    }
    return type;
}


void event (enum EventType type, void *auxdat) {
    enum TaskType task;
    struct TaskInfo ti;
    static int lines_read, current_line;
    static int msg_lines_read, msg_current_line;
    static int err_lines_read, err_current_line;
    static char last_file_title[256] = "";

    get_task (&task, &ti);
    if (type == NEW_TASK) {
	if (task == LOGOUT) {
	    char buffer[200];
	    fprintf(dfp, "new task: logout. Doing so\n");
	    write(wfd, "\033xx", 3);
	    while (read(rfd, buffer, sizeof(buffer)) > 0);
	    exit(0);
	} else if (task == READ_FILE || task == READ_MSGS ||
		   task == VISIT_FILE || task == ADD_TO_FILE ||
		   task == EDIT_FILE || task == CHECK_MENU) {
	    fprintf(dfp, "new task: %s %s. Beginning at Utilities\n",
		    (task == ADD_TO_FILE ? "add to" :
		     task == EDIT_FILE ? "edit" :
		     task == VISIT_FILE ? "visit" : "read"),
		    (task == READ_MSGS ? "messages" : 
		     task == CHECK_MENU ? "menu" : "file"));
	    write(wfd, "\033", 1);
	    ti.i = 0;
	    lines_read = current_line = 0;
	    set_task_data (&ti);
	} else if (task == NAMELINE) {
	    fprintf(dfp, "new task: change nameline. Sending [ESC][Y][N]\n");
	    write(wfd, "\033yn", 3);
	    ti.i = 0;
	    set_task_data (&ti);
	} else if (task == ENTER_TALKER) {
	    fprintf(dfp, "new task: enter talker. Sending [ESC][T]\n");
	    write(wfd, "\033t", 3);
	    ti.i = 0;
	    set_task_data (&ti);
	}
	return;
    }

    if (type == CURSOR_Y) {
	if (task == EDIT_FILE && ti.i == -11 && ti.j == -2) {
	    ti.j = cursor_y;
	    set_task_data (&ti);
	}
	if (task == EDIT_FILE && ti.i == -11 && ti.j == cursor_y &&
	    editvlen < cursor_x)
	    editvlen = cursor_x;
    }

    if ((type == ANSI_INSLINE || type == ANSI_DELLINE) && task == EDIT_FILE) {
	if (type == ANSI_INSLINE) {
	    fprintf(dfp, "Insert line\n");
	    memmove(editvscr+256*(cursor_y+1), editvscr+256*cursor_y,
		    256*(255-cursor_y));
	    memset(editvscr+256*cursor_y, 0, 256);
	    if (ti.j > 0 && ti.j < 255)
		ti.j++;
	    fprintf(dfp, "ti.j=%d\n", ti.j);
	} else {
	    fprintf(dfp, "Delete line\n");
	    memmove(editvscr+256*cursor_y, editvscr+256*(cursor_y+1),
		    256*(255-cursor_y));
	    memset(editvscr+256*255, 0, 256);
/*	    if (ti.j > 0 && ti.j > 5)
 *		 ti.j--;
 *	     fprintf(dfp, "ti.j=%d\n", ti.j);
 */	}
	set_task_data (&ti);
    }

    if (type == TITLE_SET) {
	char *p = auxdat;

	while (*p && *p != ' ')
	    p++;		       /* skip over username@mono */
	while (*p == ' ')
	    p++;		       /* skip spaces too */

	if (!strncmp(p, "In", 2))
	    monostate = MENU;
	else if (!strcmp(p, "Reading View Errors [ESC][Y][V]"))
	    monostate = ERRORS;	       /* HACK! But it's the only way. */
	else if (!strcmp(p, "Modifying Name"))
	    monostate = MOD_NAME;
	else if (!strcmp(p, "Modifying Edit Name"))
	    monostate = EDITNAME;
	else if (!strncmp(p, "Logs", 4))
	    monostate = MENU;	       /* logs menu is still a menu... */
	else if (!strncmp(p, "Scanning", 4))
	    monostate = SCANNING;      /* simple wait-state */
	else if (!strncmp(p, "Listing Known Users", 19))
	    monostate = KNOWNUSERS;
	else if (!strncmp(p, "Reading Messages", 16))
	    monostate = MESSAGES;
	else if (!strncmp(p, "Reading All Messages", 20))
	    monostate = ALLMSGS;
	else if (!strncmp(p, "Reading", 7)) {
	    monostate = READING;
	    strncpy(last_file_title, p+8, sizeof(last_file_title)-1);
	    last_file_title[sizeof(last_file_title)-1] = '\0';
	} else if (!strncmp(p, "Editing", 7)) {
	    monostate = EDITOR;
	    strncpy(last_file_title, p+8, sizeof(last_file_title)-1);
	    last_file_title[sizeof(last_file_title)-1] = '\0';
	} else if (!strncmp(p, "Talker", 6)) {
	    monostate = TALKER;
	} else if (!strncmp(p, "Utilities", 9))
	    monostate = UTILS;
	else if (!strncmp(p, "Examining", 9))
	    monostate = EXAMINING;
	else if (!strncmp(p, "Exit", 4)) {
	    char buffer[200];
	    fprintf(dfp, "Exiting Monochrome\n");
	    write (wfd, "\033xx", 3);  /* best to be sure... */
	    while (read(rfd, buffer, sizeof(buffer)) > 0);
	    exit(1);
	} else if (!strcmp(p, last_file_title)) {
	    monostate = EDITOR;
        } else {
	    char buffer[200];
	    fprintf(stderr, "bogus monostate as yet: `%s' (lft=`%s')\n", p, last_file_title);
	    write (wfd, "\033xx", 3);
	    while (read(rfd, buffer, sizeof(buffer)) > 0);
	    exit(1);
	}

	if (monostate == MENU && task == LOGIN) {
	    fprintf(dfp, "Reached a menu: login completed.\n");
	    msg_lines_read = 0;	       /* FIXME! */
	    finish_task();
	}

	if ( (monostate == MESSAGES || monostate == ALLMSGS) &&
	    task == LOGIN) {
	    fprintf(dfp, "Thrown into messages on login\n");
	    ti.i = -2;
	    set_task_data (&ti);
	    add_task (LOGINMESSAGE, NULL);
	    event (NEW_TASK, NULL);
	}

	if (monostate == ERRORS && task == LOGIN) {
	    fprintf(dfp, "Thrown into errors on login\n");
	    ti.i = -2;
	    set_task_data (&ti);
	    add_task (LOGINERROR, NULL);
	    event (NEW_TASK, NULL);
	}

	if ( (monostate == MESSAGES || monostate == ALLMSGS) &&
	    task == READ_FILE) {
	    fprintf(dfp, "Thrown into messages while reading a file\n");
	    ti.i = -2;
	    set_task_data (&ti);
	    add_task (MESSAGE, NULL);
	    event (NEW_TASK, NULL);
	}

	if (monostate == ERRORS && (task == READ_FILE || task == READ_MSGS)) {
	    fprintf(dfp, "Thrown into errors while reading a file\n");
	    fprintf(dfp, "Sending [Z]\n");
	    write(wfd, "z", 1);
	}

	if (monostate == UTILS && (task == READ_FILE ||
				   task == ADD_TO_FILE ||
				   task == EDIT_FILE ||
				   task == VISIT_FILE ||
				   task == READ_MSGS ||
				   task == CHECK_MENU)) {
	    if (task == READ_MSGS) {
		fprintf(dfp, "Sending 'a' to get to all messages\n");
		write(wfd, "a", 1);
		ti.i = 0;
	    } else if (ti.keypath[ti.i] != '\033') {
		fprintf(dfp, "Sending 'g' to get to main menu\n");
		write(wfd, "g", 1);
		ti.i = 0;
	    } else if (ti.keypath[ti.i+1]) {
		fprintf(dfp, "Sending '%c' on keypath <%s>\n", ti.keypath[1],
			ti.keypath);
		write (wfd, ti.keypath+ti.i+1, 1);
		ti.i += 2;
	    } else {
		ti.i++;	       /* just to stop segfaulting */
	    }
	    set_task_data (&ti);
	}

	if (monostate != EDITOR && task == ADD_TO_FILE && ti.i < 0) {
	    fprintf(dfp, "Edit added successfully\n");
	    finish_task();
	} else if (monostate != EDITOR && task == EDIT_FILE && ti.i < 0) {
	    fprintf(dfp, "Editing completed successfully\n");
	    finish_task();
	} else if (monostate == MENU && (task == READ_FILE ||
					 task == VISIT_FILE ||
					 task == ADD_TO_FILE ||
					 task == EDIT_FILE)) {
	    fprintf(dfp, "Sending '%c' on keypath <%s>\n", ti.keypath[ti.i],
		    ti.keypath);
	    write (wfd, ti.keypath+ti.i, 1);
	    ti.i++;
	    set_task_data (&ti);
	} else if (monostate == MENU && task == CHECK_MENU && ti.i >= 0) {
	    if(ti.keypath[ti.i]) {
		fprintf(dfp, "Sending '%c' on keypath <%s>\n", ti.keypath[ti.i],
			ti.keypath);
		write (wfd, ti.keypath+ti.i, 1);
		ti.i++;
	    } else {
		fprintf(dfp, "Got to menu <%s>\n", ti.keypath);
		ti.i = -1;
	    }
	    set_task_data (&ti);
	}

	if (monostate == MOD_NAME && task == NAMELINE) {
	    fprintf(dfp, "Setting nameline to `%s'\n", ti.nameline);
	    write (wfd, "\001\013", 2);
	    write (wfd, ti.nameline, strlen(ti.nameline));
	    write (wfd, "\r", 1);
	    ti.i = 1;		       /* we're ready to finish */
	    set_task_data (&ti);
	}

	if (monostate == EXAMINING && task == NAMELINE && ti.i == 1) {
	    fprintf(dfp, "Nameline modification complete\n");
	    finish_task();
	}

	if (monostate != TALKER && task == ENTER_TALKER) {
	    if(ti.i == -1) {
		fprintf(dfp, "Exited talker\n");
		finish_task();
	    } else {
		fprintf(dfp, "Unexpected talker exit\n");
		finish_task();
	    }
	}
    } else if (monostate == MENU && task == CHECK_MENU && ti.i == -1) {
	/* Check output we've got from menu */
	char line[MAXSTR] = "";
	
	if (type == STRING || type == CRLFSTRING) {
	    /* Convert to char array */
	    unsigned long *q = auxdat;
	    unsigned int pos = 0;
	    
	    while (*q) {
		line[pos] = (char) *q;
		q++;
		if(++pos >= MAXSTR-1) break;
	    }
	    line[pos] = '\0';
	    
	    /* Diagnostics */
	    fprintf(dfp, "In menu: Got string %s'%s'\n",
		    type == CRLFSTRING ? "with CRLF " : "", line);
	} else if (type == BSTR) {
	    fprintf(dfp, "Got menu\n");
	    finish_task();
	} else {
	    fprintf(dfp, "In menu: Got event type %d\n", type);
	}
    } else if (monostate == TALKER && ti.i >= 0) {
	/* Perform all talker activities */
	char line[MAXSTR] = "";
	char *p;
	enum {
	    TTASK_NONE = 0,
	    TTASK_QUIT,
	    TTASK_SAY,
	    TTASK_EMOTE,
	    TTASK_TELL,
	    TTASK_COMMAND,
	    TTASK_SEND
	};
	/* States (in ti.i):
	 * -1 - quitting
	 * anything <0 - not normal state - don't process
	 * TSTATE_INIT = 0 - just entered talker for first time
	 * TSTATE_WAIT - in talker, seen no lines
	 * TSTATE_SAY - have just seen empty `' Say: ' line
	 * TSTATE_SAY_MUCK - have just seen `' Say: ' line with stuff on it
	 * TSTATE_MISC_LINE - have just had any other type of line
	 **/
	enum {
	    TSTATE_QUIT = -1,
	    TSTATE_INIT = 0,
	    TSTATE_WAIT, TSTATE_SAY, TSTATE_SAY_MUCK, TSTATE_MISC_LINE
	};

	if(ti.i == TSTATE_INIT) {
	    ti.i = TSTATE_WAIT;
	    ti.j = TTASK_NONE;
	    ti.sparetext[0] = '\0';
	}

	/* Check output we've got from talker */
	if (type == STRING || type == CRLFSTRING) {
	    /* Convert to char array */
	    unsigned long *q = auxdat;
	    unsigned int pos = 0;

	    while (*q) {
		line[pos] = (char) *q;
		q++;
		if(++pos >= MAXSTR-1) break;
	    }
	    line[pos] = '\0';

	    /* Diagnostics */
	    fprintf(dfp, "Got string %s'%s'\n",
		    type == CRLFSTRING ? "with CRLF " : "", line);
	}

	if (type == STRING) {
	    if(line[0] == '"' || line[0] == '\'' ||
	       line[0] == '.' || line[0] == ';') {
		if(strlen(line) == 1) {
		    ti.i = TSTATE_SAY;
		} else {
		    ti.i = TSTATE_SAY_MUCK;
		}
	    }
	}

	if (type == CRLFSTRING) {
	    ti.i = TSTATE_MISC_LINE;
	    strncpy(ti.sparetext, line, MAXSTR);
	    ti.sparetext[MAXSTR - 1] = '\0';
	}

	fprintf(dfp, "Talker: state %d\n", ti.i);
	if(ti.i == TSTATE_SAY || ti.i == TSTATE_SAY_MUCK) {
	    /* type - type of line received from terminal
	     * 0 - Nothing
	     * 1 - Say (from different user)
	     * 2 - Emote
	     * 3 - Tell
	     * 10 - Recieved message
	     * 20+ - System message - FIXME - implement all of these
	     *   21 - User entered talker (and this room)
	     *   22 - User entered room from a different room in the talker
	     *   25 - User left talker
	     *   26 - User left room for a different room, or left mono
	     *   31 - User has barred you - IMPLEMENT
	     *   32 - User has unbarred you - IMPLEMENT
	     *   33 - User has invited you - IMPLEMENT
	     *   34 - User has uninvited you - IMPLEMENT
	     */
	    int type;

	    /* user - user who said something: "" if not applicable */
	    char talker_user[9];
	    char *talker_line = ti.sparetext;

	    /* Parse (and modify) ti.sparetext, to get talker_user and type */
	    talker_user[0] = '\0';
	    type = 0;

	    if(*talker_line == ' ') {
		int pos = 0;
		int pos2 = 0;
		talker_line++;
		if(!strncmp(talker_line, "   *you*", 8)) {
		    strncpy(talker_user, golem_user->string.data, 8);
		    talker_user[8] = '\0';
		    pos2 = strlen(talker_user);
		    pos = 8;
		    talker_line += 8;
		}
		while(pos < 8) {
		    if(*talker_line != ' ') {
			talker_user[pos2++] = *talker_line;
		    }
		    if(pos2 > 0 && !is_account_char(*talker_line)) {
			fprintf(stderr,
			"Talker: invalid account name received (%s)\n",
			ti.sparetext);
		    }
		    talker_line++;
		    pos++;
		}
		talker_user[pos2] = '\0';
		if(pos2 == 0) {
		    type = talker_parse_sysmsg(talker_line, talker_user);
		} else if(*talker_line != ' ') {
		    fprintf(stderr,
		    "Talker: no space found after account name (%s)\n",
		    ti.sparetext);
		} else if(!strcmp(talker_user, golem_user->string.data)) {
		    /* line from self - ignore */
		    type = 0;
		    /* FIXME - what if two copies of self on - remember last line and check against that? */
		} else {
		    talker_line++;
		    type = 2;
		    if(*talker_line == '\'' &&
		       talker_line[strlen(talker_line) - 1] == '\'' &&
		       strlen(talker_line) >= 2) {
			/* A Say - can be faked using emotes, but don't care */
			talker_line[strlen(talker_line) - 1] = '\0';
			talker_line++;
			type = 1;
		    } else if(!strncmp(talker_line, "tells you '", 11) &&
		       talker_line[strlen(talker_line) - 1] == '\'' &&
		       strlen(talker_line) >= 12) {
			/* A Tell - can be faked using emotes, but don't care */
			talker_line[strlen(talker_line) - 1] = '\0';
			talker_line += 11;
			type = 3;
		    }
		    /* FIXME - deal with tells */
		}

		fprintf(dfp, "Talker: message type %d, `%s' received from user `%s'.\n", type, talker_line, talker_user);
	    }

	    /*
	     * Start the edithook thread if we need to
	     */
	    if (!ti.edithook_t) {
		ti.edithook_t = loew_initthread(ti.edithook_u,
						ti.edithook);
	    }

	    /*
	     * Push four parameters (room, user, type, text) on
	     * the thread stack.
	     */

	    /* text - text last said / told / messaged / etc... */
	    p = thread_stack_push(ti.edithook_t, make_data_str(talker_line));
	    if(p) {fprintf(stderr, "%s\n", p);}
	    
	    /* type - type of line received from terminal */
	    p = thread_stack_push(ti.edithook_t, make_data_int(type));
	    if(p) {fprintf(stderr, "%s\n", p);}
	    
	    /* user - user who said something: "" if not applicable */
	    p = thread_stack_push(ti.edithook_t, make_data_str(talker_user));
	    if(p) {fprintf(stderr, "%s\n", p);}
	    
	    /* room - room we're currently in: "Common" if common room */
	    p = thread_stack_push(ti.edithook_t, make_data_str(""));
	    if(p) {fprintf(stderr, "%s\n", p);}
	    
	    /*Run loew stuff */
	    do {
		p = loew_step (ti.edithook_t);
		if (p) {
		    fputs (p, stderr);
		    fputc ('\n', stderr);
		    break;
		}
	    } while (ti.edithook_t->esp && !ti.edithook_t->suspended);

	    fprintf(dfp, "Talker: stack has %d entries after executing loew.\n", ti.edithook_t->sp);
	    
	    /* Check stack for a command to run */
	    if (ti.edithook_t->sp > 0) {
		char command[MAXSTR];
		if(!thread_stack_pop_str(command, ti.edithook_t)) {
		    fprintf(dfp, "Talker: executing loew command `%s'.\n", command);
		    if (!strcmp(command, "Say")) {
			ti.j = TTASK_SAY;
			thread_stack_pop_str(ti.sparetext, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.sparetext);
			ti.sparetext[240] = '\0';
		    } else if (!strcmp(command, "Emote")) {
			ti.j = TTASK_EMOTE;
			thread_stack_pop_str(ti.sparetext, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.sparetext);
			ti.sparetext[240] = '\0';
		    } else if (!strcmp(command, "Tell")) {
			ti.j = TTASK_TELL;
			thread_stack_pop_str(ti.usernames, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.usernames);
			thread_stack_pop_str(ti.sparetext, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.sparetext);
			ti.sparetext[240] = '\0';
		    } else if (!strcmp(command, "Send")) {
/* Pop out of the talker and send a message to someone. */
/* usernames is a space separated list of userids */
/* FIXME - check size of text, and userid list */
			char *p1;
			ti.j = TTASK_SEND;
			thread_stack_pop_str(ti.usernames, ti.edithook_t);
			p1 = ti.usernames;
			while(*p1) {
			    if(*p1 == ',') *p1 = ' ';
			    p1++;
			}
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.usernames);

			thread_stack_pop_str(ti.sparetext, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.sparetext);
		    } else if (!strcmp(command, "Command")) {
			ti.j = TTASK_COMMAND;
			thread_stack_pop_str(ti.sparetext, ti.edithook_t);
			fprintf(dfp, "Talker: loew parameter `%s'.\n", ti.sparetext);
			ti.sparetext[240] = '\0';
		    } else if (!strcmp(command, "Quit")) {
			ti.j = TTASK_QUIT;
		    }
		} else
		    ti.j = TTASK_QUIT;
	    }
	    if (!ti.edithook_t->esp) {
		loew_freethread(ti.edithook_t);
		ti.edithook_t = NULL;
	    }
	}
	
	switch(ti.j) {
	  case TTASK_QUIT:
	    fprintf(dfp, "Quitting talker\n");
	    write (wfd, "\033", 1);   /* ESC */
	    if(ti.edithook_t) loew_freethread(ti.edithook_t);
	    ti.edithook_t = NULL;
	    ti.i = TSTATE_QUIT;
	    break;
	  case TTASK_SEND:
	    fprintf(dfp, "Sending `%s' message `%s'\n", ti.usernames, ti.sparetext);
	    /* FIXME - Implement this */
	    break;
	  case TTASK_TELL:
	    fprintf(dfp, "Telling `%s' `%s'\n", ti.usernames, ti.sparetext);
	    write (wfd, "\001\013\"", 3);
/*FIXME - Check for nasties in sparetext */
/*FIXME - Check for Text sent being too long for a talker line (~240 chars) */
	    write (wfd, ti.usernames, strlen(ti.usernames));
	    write (wfd, " ", 1);
	    write (wfd, ti.sparetext, strlen(ti.sparetext));
	    write (wfd, "\015", 1);
	    break;
	  case TTASK_EMOTE:
	    fprintf(dfp, "Emoteing `%s'\n", ti.sparetext);
	    write (wfd, "\001\013;", 3);
/*FIXME - Check for nasties in sparetext */
/*FIXME - Check for Text sent being too long for a talker line (~240 chars) */
	    write (wfd, ti.sparetext, strlen(ti.sparetext));
	    write (wfd, "\015", 1);
	    break;
	  case TTASK_SAY:
	    fprintf(dfp, "Saying `%s'\n", ti.sparetext);
	    write (wfd, "\001\013'", 3);
/*FIXME - Check for nasties in sparetext */
/*FIXME - Check for Text sent being too long for a talker line (~240 chars) */
	    write (wfd, ti.sparetext, strlen(ti.sparetext));
	    write (wfd, "\015", 1);
	    break;
	  case TTASK_COMMAND:
	    fprintf(dfp, "Sending talker command `.%s'\n", ti.sparetext);
	    write (wfd, "\001\013.", 3);
/*FIXME - Check for nasties in sparetext */
/*FIXME - Check for Text sent being too long for a talker line (~240 chars) */
	    write (wfd, ti.sparetext, strlen(ti.sparetext));
	    write (wfd, "\015", 1);
	    break;
	  default:
	    break;
	}
	ti.j = TTASK_NONE;
	
if(ti.edithook_t) fprintf(dfp, "Talker: stack has %d entries after performing commands.\n", ti.edithook_t->sp);
	set_task_data (&ti);

    } else if (type == BOTTOM_LINE && monostate != EDITOR) {
	char *p = auxdat;

	if (monostate == KNOWNUSERS && task == READ_FILE) {
	    fprintf(dfp, "In Listing Known Users screen\n");
	    /*
	     * Incantation - may become configurable later...
	     */
	    write (wfd, "SAF\001\013ANMDLTCOPR\r ", 17);
	    return;
	}

	if (!strncmp(p, "[C]:Continue", 12)) {
	    fprintf(dfp, "sending [C] for Continue\n");
	    write (wfd, "c", 1);
	} else if ((task == READ_FILE || task == READ_MSGS) &&
		   p[strlen(p)-2] == '%') {
	    if (monostate == ERRORS) {
		fprintf(dfp, "Reached the end of errors. Sending [Q]\n");
		write(wfd, "q", 1);
		ti.i = -2;	       /* means wait to return */
		set_task_data (&ti);
	    } else if (ti.i != -1) {
		if (lines_read == 0) {
		    outputfp = fopen(ti.filename, "w");
		    if(outputfp == NULL) {
			fprintf(dfp, "Unable to open local file `%s' to save contents of mono file.", ti.filename);
		    }
		    fprintf(dfp, "In the file, sending [0] to go to top\n");
		    write (wfd, "0", 1);
		    current_line = 0;
		} else {
		    int target = lines_read - 256;
		    fprintf(dfp, "In the file again after returning from"
			   " messages.\n");
		    if (target < 2) {
			fprintf(dfp, "Sending [0] to go back to top\n");
			write (wfd, "0", 1);
			current_line = 0;
		    } else {
			char buf[20];
			fprintf(dfp, "Sending [G]%d\n", target + SCRHEIGHT - 3);
			write (wfd, buf, sprintf(buf, "g%d\r",
						 target + SCRHEIGHT - 3));
			current_line = target;
		    }
		}

		ti.i = -1;
		set_task_data (&ti);
	    } else {
		int len = strlen(p);
		if (len > 6 && !strcmp(p+len-6, "(100%)")) {
		    fprintf(dfp, "Finished reading file\n");
		    if(outputfp != NULL) fclose(outputfp);
		    finish_task();
		} else {
		    fprintf(dfp, "Hitting [+] to scroll down\n");
		    write (wfd, "+", 1);
		}
	    }
	} else if ((task == MESSAGE || task == LOGINMESSAGE ||
		    task == ERROR || task == LOGINERROR) &&
		   p[strlen(p)-2] == '%') {
	    int errors = (task == ERROR || task == LOGINERROR);
	    if (ti.i != -1) {
		int target = msg_lines_read - 256;
		if(errors) target = err_lines_read - 256;

		fprintf(dfp, "In %s%s",
			(task == LOGINMESSAGE || task == LOGINERROR) ?
			    "login " : "",
			errors ? "errors" : "messages");

		if (target < 2) {
		    fprintf(dfp, "Sending [0] to go back to top\n");
		    write (wfd, "0", 1);
		    if(errors) err_current_line = 0;
		    else msg_current_line = 0;
		} else {
		    char buf[20];
		    fprintf(dfp, "Sending [G]%d\n", target + SCRHEIGHT - 3);
		    write (wfd, buf, sprintf(buf, "g%d\r",
					     target + SCRHEIGHT - 3));
		    if(errors) err_current_line = target;
		    else msg_current_line = target;
		}

		ti.i = -1;
		set_task_data (&ti);
	    } else {
		int len = strlen(p);
		if (len > 6 && !strcmp(p+len-6, "(100%)")) {
		    fprintf(dfp, "Finished reading %s: hitting [Q]\n",
			    errors ? "errors" : "messages");
		    write (wfd, "q", 1);
		    finish_task();
		} else {
		    fprintf(dfp, "Hitting [+] to scroll down\n");
		    write (wfd, "+", 1);
		}
	    }
	} else if (monostate == READING && task == VISIT_FILE) {
	    fprintf(dfp, "In the file we were supposed to visit\n");
	    finish_task();
	} else if (monostate == READING && task == ADD_TO_FILE) {
	    fprintf(dfp, "In the file we were supposed to add to\n");
	    fprintf(dfp, "Hitting [A] to add an edit\n");
	    write (wfd, "a", 1);
	    ti.i = -1;
	    set_task_data (&ti);
	} else if (monostate == READING && task == EDIT_FILE && ti.i > 0) {
	    fprintf(dfp, "In the file we were supposed to edit\n");
	    fprintf(dfp, "Hitting [.][A] to enter editor\n");
	    write (wfd, ".a", 2);
	    ti.i = -1;
	    set_task_data (&ti);
	} else if (task == LOGIN && p[strlen(p)-2] == '%') {
	    if (monostate == ERRORS && strlen(p) > 5 && !strcmp(p+strlen(p)-5, "100%)")) {
		fprintf(dfp, "Finished errors on login; sending [Q] to continue\n");
		write(wfd, "q", 1);
	    }
	}
    } else if (type == CRLFSTRING && monostate != EDITOR) {
	unsigned long *q = auxdat;

	if ((task == READ_FILE || task == READ_MSGS) &&
	    ti.i == -1 && cursor_y > 1) {
	    if (current_line >= lines_read) {
		check_for_msg_line(q);
		trim_trailing_spaces(q);
		if(outputfp != NULL) write_attr_string(q, outputfp);
		fprintf(dfp, "Writing line out: current=%d read=%d\n",
		       current_line, lines_read);
		lines_read++;
	    } else
		fprintf(dfp, "Skipping line: current=%d read=%d\n",
		       current_line, lines_read);
	    current_line++;
	}

	if (ti.i == -1 && cursor_y > 1 &&
	    (task == MESSAGE || task == LOGINMESSAGE)) {
	    if (msg_current_line >= msg_lines_read) {
		check_for_msg_line(q);
		trim_trailing_spaces(q);
		write_attr_string(q, stdout);
		fprintf(dfp, "Message line is valid: current=%d read=%d\n",
		       msg_current_line, msg_lines_read);
		msg_lines_read++;
	    } else
		fprintf(dfp, "Skipping message line: current=%d read=%d\n",
		       msg_current_line, msg_lines_read);
	    msg_current_line++;
	} else if (ti.i == -1 && cursor_y > 1 &&
		   (task == ERROR || task == LOGINERROR)) {
	    if (err_current_line >= err_lines_read) {
		check_for_msg_line(q);
		trim_trailing_spaces(q);
		write_attr_string(q, stdout);
		fprintf(dfp, "Error line is valid: current=%d read=%d\n",
		       err_current_line, err_lines_read);
		err_lines_read++;
	    } else
		fprintf(dfp, "Skipping error line: current=%d read=%d\n",
		       err_current_line, err_lines_read);
	    err_current_line++;
	}
    } else if (type == STRING ||
	       (monostate == EDITOR && (type == BSTR ||
					type == CRLFSTRING))) {
	unsigned long *q = auxdat;

	if (monostate == EDITOR && task == EDIT_FILE && ti.i != -2) {
	    enum {
		ETASK_NONE = 0,
		ETASK_SAVE,
		ETASK_QUIT,
		ETASK_INSAFTER,
		ETASK_INSBEFORE,
		ETASK_DELLINE,
		ETASK_REPLACE,
		ETASK_UP,
		ETASK_DOWN,
		ETASK_GOTO	       /* must be at end */
	    };
	    enum {
		ETYPE_TEXT,
		ETYPE_LINECOUNT,
		ETYPE_NONE
	    } etype = ETYPE_NONE;
	    int line, lines;

	    if (cursor_y == 0 && cursor_x < 40 && cursor_x > 5) {
		while (*q) {
		    if ((char)(*q & CHAR_MASK) == '/' &&
			(char)(q[1] & CHAR_MASK) != '|')   /* rule out |\/| */
			break;
		    q++;
		}
		if (*q) {
		    unsigned long *p = q-1;
		    while (p-1 >= (unsigned long *)auxdat &&
			   (char)(p[-1] & CHAR_MASK) >= '0' &&
			   (char)(p[-1] & CHAR_MASK) <= '9')
			p--;
		    line = 0;
		    while ((char)(*p & CHAR_MASK) >= '0' &&
			   (char)(*p & CHAR_MASK) <= '9')
			line = line * 10 + ((char)(*p++ & CHAR_MASK)) - '0';
		    q++;
		    lines = 0;
		    while ((char)(*q & CHAR_MASK) >= '0' &&
			   (char)(*q & CHAR_MASK) <= '9')
			lines = lines * 10 + ((char)(*q++ & CHAR_MASK)) - '0';
		    etype = ETYPE_LINECOUNT;
		    fprintf(dfp, "Line count event %d/%d\n", line, lines);
		}
	    } else if (cursor_y != 0) {
		etype = ETYPE_TEXT;
		q = auxdat;
	    }

	    if (etype == ETYPE_TEXT) {
		/*
		 * update virtual screen
		 */
		int i = 0, x = cursor_x;
		while (q[i])
		    i++;
		while (i--) {
		    x--;
		    editvscr[x+256*cursor_y] = (char) (q[i] & CHAR_MASK);
		    if (q[i] & ATTR_REVERSE)
			editvscr[x+256*cursor_y] &= 0x1F;   /* control chr */
		}
		if (ti.i == -11) {
		    if (cursor_y == ti.j) {
			memcpy(editvline, editvscr+256*cursor_y, 256);
			if (editvlen < cursor_x) editvlen = cursor_x;
		    } else
			ti.j = -1;
		}
fprintf(dfp, "ti.j=%d cursor=%d,%d editvlen=%d\n", ti.j, cursor_x, cursor_y, editvlen);
	    }

	    if (etype != ETYPE_NONE) switch (ti.i) {

	      case -1:		       /* just entered editor */
		/*
		 * Our strategy is to begin by adding a dummy line
		 * at the start, so that all `real' lines have a line
		 * before them.
		 */
		if (etype != ETYPE_LINECOUNT)
		    break;	       /* only interested in linecounts */
		fprintf(dfp, "Adding dummy line at start of text\n");
		write (wfd, "\017\020\001\015", 4);   /* ^O^P, ^A, ^M */
		ti.i = -1001-lines;    /* waiting for dummy line to happen */
		break;
	      default:
		if (etype == ETYPE_LINECOUNT && ti.i == -1000-lines) {
		    ti.i = -10;	       /* ordinary state */
		    goto edit_normal;  /* hack, but it'll work */
		} else if (ti.i < -1000)
		    ;		       /* do nothing - wait for dummyline */
		else {
		    fprintf(dfp, "WTF? ti.i=%d\n", ti.i);
		    abort();
		}
		break;

	      case -8:
		if (etype != ETYPE_LINECOUNT)
		    break;
		fprintf(dfp, "Ignoring first of two spurious line counts\n");
		ti.i = -9;	       /* ordinary state next time */
		break;
	      case -9:
		if (etype != ETYPE_LINECOUNT)
		    break;
		fprintf(dfp, "Ignoring line count\n");
		ti.i = -10;	       /* ordinary state next time */
		break;
	      case -10:		       /* ordinary state */
		edit_normal:	       /* hack to get here from default */
		if (etype != ETYPE_LINECOUNT)
		    break;	       /* only interested in linecounts */
		fprintf(dfp, "Ordinary state!\n");
		write(wfd, "\001\005\020", 3);   /* ^A^E^P to get line text */
		ti.i = -11;	       /* getting-line-text */
		ti.j = -2;	       /* to be filled in with line num */
		memcpy(editvline, editvscr+256*cursor_y, 256);
		editvlen = 0;
		break;
	      case -11:		       /* getting-line-text */
		if (etype == ETYPE_LINECOUNT) {
		    char *p;
		    int i;

		    p = ti.sparetext;
		    for (i=0; i<editvlen; i++)
			ti.sparetext[i] = editvline[i];
		    ti.sparetext[editvlen] = '\0';
		    fprintf(dfp, "Got line: %d of %d len %d is `%s'\n",
			    line, lines-1, editvlen, ti.sparetext);
		    ti.i = -12;	       /* moving-back-to-line */
		    write (wfd, "\016", 1);   /* ^N - next line */

		    /*
		     * Get an action/string pair
		     */
		    if (!ti.edithook_t) {
			ti.edithook_t = loew_initthread(ti.edithook_u,
							ti.edithook);
		    }
		    /*
		     * Push three parameters (line, lines, text) on
		     * the thread stack.
		     */
		    ti.edithook_t->stack[ti.edithook_t->sp] =
			malloc(sizeof(Data));
		    ti.edithook_t->stack[ti.edithook_t->sp]->type = DATA_STR;
		    strcpy(ti.edithook_t->stack[ti.edithook_t->sp]
			   ->string.data, ti.sparetext);
		    ti.edithook_t->sp++;
		    ti.edithook_t->stack[ti.edithook_t->sp] =
			malloc(sizeof(Data));
		    ti.edithook_t->stack[ti.edithook_t->sp]->type = DATA_INT;
		    ti.edithook_t->stack[ti.edithook_t->sp]->integer.value =
			lines-1;
		    ti.edithook_t->sp++;
		    ti.edithook_t->stack[ti.edithook_t->sp] =
			malloc(sizeof(Data));
		    ti.edithook_t->stack[ti.edithook_t->sp]->type = DATA_INT;
		    ti.edithook_t->stack[ti.edithook_t->sp]->integer.value =
			line;
		    ti.edithook_t->sp++;
			
		    do {
			p = loew_step (ti.edithook_t);
			if (p) {
			    fputs (p, stderr);
			    fputc ('\n', stderr);
			    break;
			}
		    } while (ti.edithook_t->esp && !ti.edithook_t->suspended);
		    p = NULL;
		    if (ti.edithook_t->sp > 0) {
			ti.edithook_t->sp--;
			if (ti.edithook_t->stack[ti.edithook_t->sp]->type
			    == DATA_STR) {
			    p = (ti.edithook_t->stack[ti.edithook_t->sp]
				 ->string.data);
			    if (!strcmp(p, "Goto")) {
				if (ti.edithook_t->sp > 0 &&
				    ti.edithook_t->stack[ti.edithook_t->sp-1]
				    ->type == DATA_INT) {
				    free (ti.edithook_t
					  ->stack[ti.edithook_t->sp--]);
				    ti.j = (ti.edithook_t
					    ->stack[ti.edithook_t->sp]
					    ->integer.value);
				    if (ti.j < 0)
					ti.j = 1;
				    if (ti.j > lines-1)
					ti.j = lines-1;
				} else
				    ti.j = 1;
				ti.j += ETASK_GOTO;
			    } else if (!strcmp(p, "Down")) {
				ti.j = ETASK_DOWN;
			    } else if (!strcmp(p, "Up")) {
				ti.j = ETASK_UP;
			    } else if (!strcmp(p, "Quit")) {
				ti.j = ETASK_QUIT;
			    } else if (!strcmp(p, "Save")) {
				ti.j = ETASK_SAVE;
			    } else if (!strcmp(p, "Delete")) {
				ti.j = ETASK_DELLINE;
			    } else if (!strcmp(p, "InsertAfter")) {
				ti.j = ETASK_INSAFTER;
			    } else if (!strcmp(p, "InsertBefore")) {
				ti.j = ETASK_INSBEFORE;
			    } else if (!strcmp(p, "Replace")) {
				ti.j = ETASK_REPLACE;
				if (ti.edithook_t->sp > 0 &&
				    ti.edithook_t->stack[ti.edithook_t->sp-1]
				    ->type == DATA_STR) {
				    free (ti.edithook_t
					  ->stack[ti.edithook_t->sp--]);
				    strncpy(ti.sparetext, ti.edithook_t
					    ->stack[ti.edithook_t->sp]
					    ->string.data, 79);
				    ti.sparetext[79] = '\0';
				} else
				    ti.sparetext[0] = '\0';
			    }
			} else
			    ti.j = ETASK_QUIT;  /* default to quit not save */
			free (ti.edithook_t->stack[ti.edithook_t->sp]);
		    }
		    if (!ti.edithook_t->esp) {
			loew_freethread(ti.edithook_t);
			ti.edithook_t = NULL;
			set_task_data (&ti);
		    }
		}
		break;

	      case -12:		       /* back to line, with action */
		ti.i = -10;	       /* ordinary state */
		switch (ti.j) {
		  case ETASK_SAVE:
		    fprintf(dfp, "SAVE action received: removing dummy line\n");
		    /* Note - this will fail if something's been put in the dummy line */
		    write (wfd, "\017\020\005\010 \001\013\016\010", 9);
			      /* ^O^P,   ^E, ^H,  ,^A,^K, ^N, ^H */

		    fprintf(dfp, "SAVE action received: saving and exiting editor\n");
		    write (wfd, "\017 ", 2);   /* ^O q */
		    break;
		  case ETASK_QUIT:
		    fprintf(dfp, "QUIT action received: exiting editor\n");
		    write (wfd, "\017q", 2);   /* ^O q */
		    break;
		  case ETASK_DOWN:
		    fprintf(dfp, "Moving down a line\n");
		    write (wfd, "\001\016", 2);   /* ^A^N */
		    break;
		  case ETASK_UP:
		    fprintf(dfp, "Moving up a line\n");
		    write (wfd, "\001\020", 2);   /* ^A^P */
		    break;
		  case ETASK_REPLACE:
		    fprintf(dfp, "Replacing line with `%s'\n", ti.sparetext);
		    write (wfd, "\001", 1);/* ^A */
		    if (editvlen > 0)
			write (wfd, "\013", 1);/* ^K */
		    write (wfd, ti.sparetext, strlen(ti.sparetext));
		    write (wfd, "\001\020\016", 3);/* ^A^P^N */
		    ti.i = -9;	       /* discard one line count */
		    break;
		  case ETASK_INSBEFORE:
		    fprintf(dfp, "Inserting a line before this one\n");
		    write (wfd, "\001\015\020", 3);   /* ^A^M^P */
		    ti.i = -9;	       /* discard one line count */
		    break;
		  case ETASK_INSAFTER:
		    if (line == lines) {
			fprintf(dfp, "Inserting a line after the last one\n");
			write(wfd, "\005\015\001\013", 4);   /* ^E^M^A^K */
		    } else {
			fprintf(dfp, "Inserting a line after this one\n");
			write (wfd, "\016\001\015\020", 4);   /* ^N^A^M^P */
			ti.i = -8;	       /* discard two line counts */
		    }
		    break;
		  case ETASK_DELLINE:
		    if (line == lines) {
			fprintf(dfp, "Deleting the last line\n");
			write(wfd, "\001\013\010", 3);   /* ^A^K^H */
		    } else {
			fprintf(dfp, "Deleting the current line\n");
			write(wfd, "\001\013\013", (editvlen>0 ? 3 : 2));
			/* ^A^K on a blank line, or ^A^K^K elsewhere */
		    }
		    break;
		  default:	       /* ETASK_GOTO + linenum */
		    fprintf(dfp, "Teleporting to line %d of %d\n",
			    ti.j-ETASK_GOTO, lines-1);
		    sprintf(ti.sparetext, "\017g%d\r",
			    ti.j-ETASK_GOTO+1);   /* ^O g <number> RET */
		    write(wfd, ti.sparetext, strlen(ti.sparetext));
		    break;
		}
		break;
	    }
	    set_task_data (&ti);
	}

	if (cursor_y == 0 && monostate == EDITOR &&
	    task == ADD_TO_FILE && ti.i != -2) {
	    char line[81];
	    char *p;

	    /*
	     * Add a line whenever we see the line-number counter
	     * change or appear. We can tell because it's on line
	     * zero (already checked) and has a slash in it.
	     */
	    while (*q) {
		if ((char)(*q & CHAR_MASK) == '/')
		    break;
		q++;
	    }
	    if (!*q) {
		fprintf(dfp, "Ignoring line-zero text not containing `/'\n");
		return;
	    }

	    /*
	     * Get a new line of text for the edit
	     */
	    if (!ti.edithook_t) {
		ti.edithook_t = loew_initthread(ti.edithook_u, ti.edithook);
		set_task_data (&ti);
	    }
	    do {
		p = loew_step (ti.edithook_t);
		if (p) {
		    fputs (p, stderr);
		    fputc ('\n', stderr);
		    break;
		}
	    } while (ti.edithook_t->esp && !ti.edithook_t->suspended);
	    p = NULL;
	    if (ti.edithook_t->sp > 0) {
		ti.edithook_t->sp--;
		if (ti.edithook_t->stack[ti.edithook_t->sp]->type==DATA_STR) {
		    p = ti.edithook_t->stack[ti.edithook_t->sp]->string.data;
		    strncpy(line, p, 79);
		    line[79] = '\0';
		    p = line;
		}
		free (ti.edithook_t->stack[ti.edithook_t->sp]);
	    }
	    if (!ti.edithook_t->esp) {
		loew_freethread(ti.edithook_t);
		ti.edithook_t = NULL;
		set_task_data (&ti);
	    }
	    if (p) {
		/*
		 * Parse out metacharacters, by assuming them all
		 * to be trib codes (done by ^G). Also strip high
		 * bits. Tabs become spaces because the expansion
		 * isn't quite obvious...
		 */
		for (p = line; *p; p++) {
		    *p &= 0x7F;
		    if (*p == '\t')
			*p = ' ';
		    else if (*p < ' ' || *p == '\177')
			*p = '\007';
		}

		/*
		 * Check for . and .quit special cases. Append ^Gb
		 * if we get them. (There are cleaner ways, but
		 * this will do for now.)
		 */
		if (*line == '.' && !line[1+strspn(line+1, " ")])
		    strcpy(line+1, "\007b");
		else if (!strncmp(line, ".quit", 5) &&
			 !line[5+strspn(line+5, " ")])
		    strcpy(line+5, "\007b");
		fprintf(dfp, "Entering line `%s'\n", line);
		write (wfd, "\001\013", 2);   /* ^A^K to combat auto-indent */
		write (wfd, line, strlen(line));
		write (wfd, "\r", 1);
	    } else {
		/*
		 * Finished adding edit. Set edit name if necessary.
		 */
		char nameline[48];
		if (ti.nameline[0] != '\1') {
		    strcpy(nameline, "\017 n\001\013");   /* ^O[space]n^A^K */
		    strncpy(nameline+5, ti.nameline, 40);
		    nameline[45] = '\0';
		    strcat(nameline+5, "\r ");   /* finish adding edit */
		    write (wfd, nameline, strlen(nameline));
		} else
		    write (wfd, "\017  ", 3);
		if (ti.edithook_t)
		    loew_freethread(ti.edithook_t);
		ti.edithook_t = NULL;
		ti.i = -2;
		set_task_data (&ti);
	    }
	}
    }
}

/*
 * This routine checks whether a line is a message separator, and
 * if so it shortens it to the canonical 79 characters; an external
 * script will become available to modify this for re-viewing of
 * files on different width screens, but IMHO most people are going
 * to want 79.
 *
 * The heuristic (yes, another one...) used here is that we check
 * to see if the first (255-79) characters of the line are minus
 * signs in the correct message-separator attribute, and that if so
 * we remove them and shunt the rest up.
 */
static void check_for_msg_line (unsigned long *str) {
    unsigned long *s2;
    int i = 255-79;

    s2 = str;
    while (i--)
	if (*str++ != MESSAGE_SEP)
	    return;
    while ( (*s2++ = *str++) );
}

static void trim_trailing_spaces (unsigned long *str) {
    unsigned long *p = str;
    while (*p)
	p++;
    while (p > str && *--p == BLANK_SPACE)
	*p = 0L;
}

static void write_attr_string (unsigned long *str, FILE *fp) {
    unsigned long attr = ATTR_DEFAULT, attr2;

    while (*str) {
	attr2 = *str & ATTR_MASK;
	if (attr2 != attr) {
	    int fg, bg;
	    fputs("\033[", fp);
	    if (attr2 & ATTR_BOLD) fputs(";1", fp);
	    if (attr2 & ATTR_UNDER) fputs(";4", fp);
	    if (attr2 & ATTR_FLASH) fputs(";5", fp);
	    if (attr2 & ATTR_REVERSE) fputs(";7", fp);
	    fg = (attr2 >> ATTR_FGSHIFT) & 7;
	    bg = (attr2 >> ATTR_BGSHIFT) & 7;
	    if (fg != 7) fprintf(fp, ";%d", 30+fg);
	    if (bg != 0) fprintf(fp, ";%d", 40+bg);
	    fputc('m', fp);
	    attr = attr2;
	}
	fputc (*str & CHAR_MASK, fp);
	str++;
    }
    if (attr != ATTR_DEFAULT)
	fputs("\033[m", fp);
    fputc('\n', fp);
}
