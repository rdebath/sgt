/*
 * tasks.c   manage the list of to-do tasks for a running Golem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "golem.h"

struct Task {
    struct Task *next;
    int priority;
    enum TaskType type;
    struct TaskInfo ti;
    time_t activation;
};

static struct Task *head = NULL;

/*
 * A priority for each task type in the enum in golem.h
 */
static int priorities[] = {
    256,			       /* NO_TASK */
    0,				       /* LOGINMESSAGE - highest possible */
    0,				       /* LOGINERROR - highest possible */
    1,				       /* LOGIN - nearly highest possible */
    2,				       /* MESSAGE - just below login */
    2,				       /* ERROR - just below login */
    255,			       /* LOGOUT - stupidly low */
    10,				       /* READ_FILE - a standard one */
    10,				       /* READ_MSGS - a standard one */
    10,				       /* VISIT_FILE - a standard one */
    10,				       /* NAMELINE - a standard one */
    10,				       /* ADD_TO_FILE - a standard one */
    10,				       /* EDIT_FILE - a standard one */
    20,				       /* ENTER_TALKER - we sit in the talker when nothing else to do */
    10,				       /* CHECK_MENU - standard one */
};

void add_task (enum TaskType type, struct TaskInfo *ti) {
    int priority = priorities[type];
    struct Task **where, *newtask;

    where = &head;
    while (*where && (*where)->priority <= priority)
	where = &(*where)->next;

    newtask = malloc(sizeof(struct Task));
    newtask->next = *where;
    *where = newtask;

    newtask->type = type;
    newtask->priority = priority;
    if (ti)
	newtask->ti = *ti;	       /* structure copy */
    else {
	newtask->ti.edithook = NULL;
	newtask->ti.filename[0] = '\0';
	newtask->ti.keypath[0] = '\0';
	newtask->ti.nameline[0] = '\0';
    }
}

extern void set_task_data (struct TaskInfo *ti) {
    if (!head) {
	fprintf(stderr, "panic: set_task_data: task queue empty\n");
	abort();
    }
    if (ti)
	head->ti = *ti;		       /* structure copy */
}

void get_task (enum TaskType *type, struct TaskInfo *ti) {
    if (head) {
	*type = head->type;
	if (ti)
	    *ti = head->ti;	       /* structure copy */
    } else {
	*type = NO_TASK;
	if (ti) {
	    ti->edithook = NULL;
	    ti->filename[0] = '\0';
	    ti->keypath[0] = '\0';
	    ti->nameline[0] = '\0';
	}
    }
}

void finish_task (void) {
    struct Task *old;
    int type;

    if (!head) {
	fprintf(stderr, "panic: finish_task: task queue empty\n");
	abort();
    }
    type = head->type;
    old = head;
    head = head->next;
    free (old);
    if (type != MESSAGE)	       /* messages interrupt another task */
	event (NEW_TASK, NULL);
}
