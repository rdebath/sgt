#ifndef GOLEM_GOLEM_H
#define GOLEM_GOLEM_H

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define SCRHEIGHT 256
#define SCRWIDTH 256

#define MAXACC 8 /* Max length of an account name */

#include "loew.h"

/*
 * Exported from golem.c:
 */
extern FILE *dfp;		       /* diagnostics file */
extern Data *golem_user, *golem_pw;

/*
 * Exported from input.c:
 */
enum EventType {
    TITLE_SET, NEED_CONTINUE, BOTTOM_LINE, STRING, CRLFSTRING, BLEEP,
    NEW_TASK, CURSOR_Y, ANSI_INSLINE, ANSI_DELLINE, BSTR
};
extern void main_loop(void);
extern void event (enum EventType, void *);

/*
 * Exported from tasks.c:
 */
enum TaskType {
    NO_TASK, LOGINMESSAGE, LOGINERROR, LOGIN, MESSAGE, ERROR, LOGOUT,
    READ_FILE, READ_MSGS, VISIT_FILE, NAMELINE, ADD_TO_FILE, EDIT_FILE,
    ENTER_TALKER, CHECK_MENU
};  /* NB this enum _must_ match priorities[] in tasks.c */
struct TaskInfo {
    int i, j;			       /* progress reports */
    char sparetext[MAXSTR];	       /* spare space */
    char usernames[MAXSTR];	       /* usernames to send to */
    char nameline[41];
    char keypath[30];		       /* sensible maximum */
    Exec *edithook;
    Thread *edithook_t;
    Security *edithook_u;
    char filename[FILENAME_MAX];
};
extern void add_task (enum TaskType, struct TaskInfo *);
extern void set_task_data (struct TaskInfo *);
extern void get_task (enum TaskType *, struct TaskInfo *);
extern void finish_task (void);

/*
 * Exported from loewlib.c:
 */
extern void loewlib_setup(void);

#endif
