/*
 * loewlib.c  Golem library functions to call from the control language
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <time.h>

#include "golem.h"
#include "loew.h"
#include "search.h"

/* ---------------------------
 * Data management routines
 */
typedef enum _enum_golemdata_err {
  E_GOLEMDATA_OK = 0,
  E_GOLEMDATA_STACKEMPTY,
  E_GOLEMDATA_STACKFULL,
  E_GOLEMDATA_NULL,   /* A required pointer argument was NULL */
  E_GOLEMDATA_BADTYPE /* An item found was not of the requested type */
} golemdata_err;

/* Might want to implement _pop and _push as macros... */

/* golemdata_err stack_pop(Stack *stackref, Data **itemref)
 * Takes - a pointer to a Stack struct, and
 *         a pointer to a Data pointer.
 * Returns - E_GOLEMDATA_OK if on success
 *           E_GOLEMDATA_NULL if either of it's parameters are NULL
 *           E_GOLEMDATA_STACKEMPTY if there were no items on the stack
 * Sets - *itemref is set to be a pointer to the item on the top of the stack
 *        *itemref is set to be NULL if an error occurs (unless itemref == NULL)
 *        stackref->sp, the stack pointer, is decremented, unless an error occurs
 */
golemdata_err stack_pop(Stack *stackref, Data **itemref) {
  /* Check for NULL pointers */
  if(itemref == NULL) {
    return E_GOLEMDATA_NULL;
  }
  if(stackref == NULL) {
    *itemref = NULL;
    return E_GOLEMDATA_NULL;
  }
   
  /* Check that there are items on the stack */
  if(stackref->sp == 0) {
    *itemref = NULL;
    return E_GOLEMDATA_STACKEMPTY;
  }

  /* Pop item off stack */
  (stackref->sp)--;
  *itemref = stackref->stack[stackref->sp];
  return E_GOLEMDATA_OK;
}

/* golemdata_err stack_push(Stack *stackref, Data **itemref)
 * Takes - a pointer to a Stack struct, and
 *         a pointer to a Data pointer.
 * Returns - E_GOLEMDATA_OK if on success
 *           E_GOLEMDATA_NULL if either of it's parameters are NULL
 *           E_GOLEMDATA_STACKFULL if there was no more space on the stack
 * Sets - *itemref is pushed onto the top of the stack
 *        stackref->sp, the stack pointer, is incremented, unless an error occurs
 */
golemdata_err stack_push(Stack *stackref, Data **itemref) {
  /* Check for NULL pointers */
  if(itemref == NULL) {
    return E_GOLEMDATA_NULL;
  }
  if(stackref == NULL) {
    return E_GOLEMDATA_NULL;
  }
   
  /* Check that there is space on the stack */
  if(stackref->sp == stackref->stacklimit) {
    return E_GOLEMDATA_STACKFULL;
  }

  /* Push item onto stack */
  stackref->stack[stackref->sp] = *itemref;
  (stackref->sp)++;
  return E_GOLEMDATA_OK;
}

/*
 * Pop an item from the stack
 * Exactly like stack_pop(), but also takes a type argument.
 * The pop will be performed if possible, and then the popped value's type will be checked.
 * If it is not the correct type, the error value E_GOLEMDATA_BADTYPE will be returned
 */
golemdata_err stack_pop_type(Stack *stackref, Data **itemref, Data_type type) {
  golemdata_err result;

  result = stack_pop(stackref, itemref);
  if(result)
    return result;  /* Couldn't pop */
  if((*itemref)->type != type)
    return E_GOLEMDATA_BADTYPE;
  return result;
}

/* ---------------------------
 * Internal routines
 */

/*
 * Binary search for one string in an array of strings. Returns
 * index of match, or -1 for no match.
 */
int binsearch(char *needle, char **haystack, int minindex, int maxindex) {
    int index = (minindex + maxindex) / 2;
    int cmp = strcmp(needle, haystack[index]);
    if (cmp == 0)
	return index;
    else if (cmp < 0)
	return (minindex >= index ? -1 :
		binsearch (needle, haystack, minindex, index-1));
    else
	return (maxindex <= index ? -1 :
		binsearch (needle, haystack, index+1, maxindex));
}

/* ---------------------------
 * Actual exported primitives
 */

/*
 * `+' expects two integers on the stack. It pops them, adds them
 * and pushes the result.
 */
static char *plus(Thread *t) {
    if (t->sp < 2)
	return "Parameter stack underflow (+ expected 2 items on stack)";
    t->stack[t->sp - 2]->integer.value += t->stack[t->sp - 1]->integer.value;
    free (t->stack[--t->sp]);
    return NULL;
}

/*
 * `-' expects two integers on the stack. It pops them, adds them
 * and pushes the result.
 */
static char *minus(Thread *t) {
    if (t->sp < 2)
	return "Parameter stack underflow (- expected 2 items on stack)";
    t->stack[t->sp - 2]->integer.value -= t->stack[t->sp - 1]->integer.value;
    free (t->stack[--t->sp]);
    return NULL;
}

/*
 * `!' expects one integer on the stack. It replaces it with its
 * logical inversion (ie zero if it was nonzero, or 1 if it was
 * zero).
 */
static char *not(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (- expected 1 item on stack)";
    t->stack[t->sp-1]->integer.value = !t->stack[t->sp-1]->integer.value;
    return NULL;
}

/*
 * `random' expects one integer on the stack. It replaces it with a random
 * number less than it. (and >= 0)
 */
static char *loewlib_random(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (random expected 1 item on stack)";
    if (t->stack[t->sp - 1]->type != DATA_INT)
	return "Type mismatch (random expected integer on top of stack)";
    
    fprintf(dfp, "Generating random < %d\n", t->stack[t->sp-1]->integer.value);
    t->stack[t->sp-1]->integer.value =
	(int) ((t->stack[t->sp-1]->integer.value + 0.0) *
		   rand() / (RAND_MAX + 1.0));
    fprintf(dfp, "Generated random: %d\n", t->stack[t->sp-1]->integer.value);
    return NULL;
}

/*
 * `itos10' expects one integer on the stack. It replaces it with a
 * string representing it's value in decimal.
 */
static char *loewlib_itos10(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (itos10 expected 1 item on stack)";
    if (t->stack[t->sp - 1]->type != DATA_INT)
	return "Type mismatch (itos10 expected integer on top of stack)";

    snprintf(t->stack[t->sp-1]->string.data, MAXSTR - 1, "%d", t->stack[t->sp-1]->integer.value);
    t->stack[t->sp - 1]->type = DATA_STR;
    return NULL;
}

/*
 * `loew_dup' duplicates the object on top of the stack.
 */
static char *loew_dup(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (dup expected 1 item on stack)";
    if (t->sp == t->stacklimit)
	return "Parameter stack limit exceeded (dup needs 1 free space on stack)";
    t->stack[t->sp] = loew_dupdata (t->stack[t->sp-1]);
    t->sp++;
    return NULL;
}

/*
 * `cmp' tests for equality
 * Pops two items from top of stack
 * Pushes int result: 0 if equal, 1 if second is bigger than top, -1 else.
 * (For file descriptors, only 0 or 1)
 */
static char *loewlib_cmp(Thread *t) {
    int result = 0;
    if (t->sp < 2)
	return "Parameter stack underflow (cmp expects 2 items on stack)";
    if (t->stack[t->sp - 2]->type != t->stack[t->sp - 1]->type)
	return "Type mismatch: cmp expects two items of same type";
    switch (t->stack[t->sp - 1]->type) {
      case DATA_INT:
	result = (t->stack[t->sp - 2]->integer.value -
		  t->stack[t->sp - 1]->integer.value);
	break;
      case DATA_STR:
	result = strcmp(t->stack[t->sp - 2]->string.data,
			t->stack[t->sp - 1]->string.data);
	break;
      case DATA_FILE:
	result = (t->stack[t->sp - 2]->file.fd ==
		  t->stack[t->sp - 1]->file.fd) ? 0 : 1;
	break;
    }
    result = result ? (result > 0 ? 1 : -1 ) : 0;
    
    t->stack[t->sp - 2]->integer.value = result;
    t->stack[t->sp - 2]->type = DATA_INT;
    free (t->stack[--t->sp]);
    return NULL;
}

/*
 * `.' concatenates two strings
 * Pops two items from top of stack
 * Pushes string result - second string on stack followed by top string
 */
static char *loewlib_concat(Thread *t) {
    if (t->sp < 2)
	return "Parameter stack underflow (eq expects 2 items on stack)";
    if (t->stack[t->sp - 1]->type != DATA_STR)
	return "Type mismatch: . expects string on top of stack";
    if (t->stack[t->sp - 2]->type != DATA_STR)
	return "Type mismatch: . expects string second on stack";

    strncat(t->stack[t->sp - 2]->string.data,
	    t->stack[t->sp - 1]->string.data,
	    MAXSTR - 1 - strlen(t->stack[t->sp - 2]->string.data));
    t->stack[t->sp - 2]->string.data[MAXSTR - 1] = '\0';

    free(t->stack[--t->sp]);
    return NULL;
}

/*
 * `split' splits the string second from top of the stack with spaces as
 * separator.
 * FIXME - make it ignore leading and trailing spaces.  Possibly multiple too.
 * FIXME - make it take a separator character / make a new function which does
 * The top item on the stack is the maximum number of parts to split into
 * (<1 for unlimited)
 * The resulting strings are pushed onto the stack, together with a count,
 * such that the count is at the top of the stack, followed by the words in
 * order (first word at top of stack).
 */
static char *loewlib_split(Thread *t) {
    int max;
    int count;
    char tosplit[MAXSTR];
    char *split;
    unsigned int firststr, laststr;

    if (t->sp < 2)
	return "Parameter stack underflow (split expected 2 items on stack)";
    if (t->stack[t->sp - 1]->type != DATA_INT)
	return "Type mismatch (split expected integer on top of stack)";
    if (t->stack[t->sp - 2]->type != DATA_STR)
	return "Type mismatch (split expected string second on stack)";

    max = t->stack[t->sp - 1]->integer.value;
    strncpy(tosplit, t->stack[t->sp - 2]->string.data, MAXSTR);
    split = tosplit;
    count = 0;

    free (t->stack[--t->sp]);
    free (t->stack[--t->sp]);

    firststr = t->sp;
fprintf(dfp, "Loew: splitting `%s' into: ", tosplit);
    while(*split != '\0') {
	char *start = split; 
	if(count < max - 1 || max <= 0) {
	    /* Look for a split position */
	    while(*split >= 33)
		split++;
	} else if(count == max - 1 && max > 0) {
	    split += strlen(split);
	}
	if (*split != '\0') {
	    *split = '\0';
	    split++;
	}

	if (t->sp == t->stacklimit)
	    return "Parameter stack limit exceeded (in loew split)";
	t->stack[t->sp] = gmalloc(sizeof(Data));
	t->stack[t->sp]->type = DATA_STR;
	strcpy(t->stack[t->sp]->string.data, start);
fprintf(dfp, "`%s' ", start);
	t->sp++;
	count++;
    }
    

    laststr = t->sp;
    while(firststr + 1 < laststr) {
	Data *tmp = t->stack[laststr - 1];
	t->stack[laststr - 1] = t->stack[firststr];
	t->stack[firststr] = tmp;
	firststr++;
	laststr--;
    }

    if (t->sp == t->stacklimit)
	return "Parameter stack limit exceeded (in loew split)";
    t->stack[t->sp] = gmalloc(sizeof(Data));
    t->stack[t->sp]->type = DATA_INT;
    t->stack[t->sp]->integer.value = count;
fprintf(dfp, " - %d parts\n", count);
    t->sp++;
    return NULL;
}

/*
 * `fileopen' opens a local file.
 * Pops mode descriptor from top of stack, and filename from next on stack.
 * Pushes file descriptor onto stack.
 */
static char *fileopen(Thread *t) {
    int filedesc;
    char *mode;
    int modeflags;

    /* Check stack */
    if (t->sp < 2)
	return "Parameter stack underflow (fileopen expected 2 items on stack)";
    if (t->stack[t->sp-1]->type != DATA_STR)
	return "Type mismatch on stack in `fileopen' (expected mode string)";
    if (t->stack[t->sp-2]->type != DATA_STR)
	return "Type mismatch on stack in `fileopen' (expected filename string)";

    /* Check mode */
    mode = t->stack[t->sp-1]->string.data;
    modeflags = 0;
    if (!strcmp(mode, "r"))
	modeflags = O_RDONLY;
    else if (!strcmp(mode, "w"))
	modeflags = O_WRONLY | O_TRUNC | O_CREAT;
    else if (!strcmp(mode, "a"))
	modeflags = O_WRONLY | O_APPEND | O_CREAT;
    else
	return "Invalid mode in fileopen (valid modes are \"r\", \"w\" or \"a\")";

    /* Open file */
    fprintf (dfp, "File '%s' with mode '%s' ",
		   t->stack[t->sp-2]->string.data,
		   t->stack[t->sp-1]->string.data);
    filedesc = open(t->stack[t->sp-2]->string.data, modeflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    /* Update stack */
    free (t->stack[--t->sp]); /* Remove one item */
    t->stack[t->sp - 1]->type = DATA_FILE;
    t->stack[t->sp - 1]->file.fd = filedesc;

    /* Print diagnostics */
    if (filedesc == -1) {
	fprintf (dfp, "- unable to open.\n");
    } else {
	fprintf (dfp, " opened as fd '%d'.\n", filedesc);
    }
    return NULL;
} 

/*
 * `fileread' reads a line from an open file descriptor.
 * Pops file descriptor from stack.
 * Pushes line read onto stack.
 */
static char *fileread(Thread *t) {
    int filedesc;
    char *p;
    int length;
    ssize_t bytes;

    if (t->sp < 1)
	return "Parameter stack underflow (fileread expected 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_FILE)
	return "Type mismatch on stack in `fileread' (expected file descriptor)";

    filedesc = t->stack[t->sp - 1]->file.fd;
    p        = t->stack[t->sp - 1]->string.data;
    length   = 0;
    
    if(filedesc < 0) {
	p[length] = '\0';
	fprintf (dfp, "Failed to read line - invalid fd %d.\n", filedesc);
	return "Invalid file descriptor in `fileread'";
    } else {
	while((bytes = read(filedesc, p + length, 1)) == 1) {
	    if(p[length] == '\n') break;
	    length++;
	    if(length >= MAXSTR - 1) break;
	}
	if(bytes == -1)
	    return "Error reading from file in `fileread'";
	p[length] = '\0';
	fprintf (dfp, "Read line '%s' from fd %d.\n", p, filedesc);
    }

    t->stack[t->sp - 1]->type = DATA_STR;
    t->stack[t->sp - 1]->string.length = length;
    return NULL;
}

/*
 * `filewrite' writes a line to an open file descriptor.
 * Pops file descriptor from top of stack.
 * Pops line to write from next on stack.
 */
static char *filewrite(Thread *t) {
    int filedesc;
    char *p;
    ssize_t bytes;

    if (t->sp < 2)
	return "Parameter stack underflow (filewrite expected 2 items on stack)";
    if (t->stack[t->sp - 1]->type != DATA_FILE)
	return "Type mismatch on stack in `filewrite' (expected file descriptor)";
    if (t->stack[t->sp - 2]->type != DATA_STR)
	return "Type mismatch on stack in `filewrite' (expected string)";

    filedesc = t->stack[t->sp - 1]->file.fd;
    p        = t->stack[t->sp - 2]->string.data;

    if(filedesc < 0) {
	fprintf (dfp, "Failed to write line - invalid fd %d.\n", filedesc);
	return "Invalid file descriptor in `filewrite'";
    } else {
	fprintf (dfp, "Writing line '%s' to fd %d.\n", p, filedesc);
	p[strlen(p) + 1] = '\0';
	p[strlen(p)] = '\n';
	bytes = write(filedesc, p, strlen(p));
    }

    if(bytes == -1)
	return "Error writing to file in `filewrite'";

    free(t->stack[--t->sp]);
    free(t->stack[--t->sp]);
    return NULL;
}

/*
 * `fileclose' closes a file descriptor.
 * Pops file descriptor from stack.
 */
static char *fileclose(Thread *t) {
    int filedesc;

    if (t->sp < 1)
	return "Parameter stack underflow (fileclose expected 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_FILE)
	return "Type mismatch on stack in `fileclose' (expected file descriptor)";

    filedesc = t->stack[t->sp - 1]->file.fd;

    /* Update stack */
    free (t->stack[--t->sp]); /* Remove one item */

    /* Close file */
    if(close(filedesc)) {
	fprintf (dfp, "Failed to close fd '%d'.\n", filedesc);
/*	return "Error attempting to close file."; - Fail silently */
    }

    fprintf (dfp, "Closed fd '%d'.\n", filedesc);
    return NULL;
}

/*
 * `fileeof' checks for eof on a file descriptor.
 * Pops file descriptor from stack.
 * Pushes 1 onto stack
 */
static char *fileeof(Thread *t) {
    int filedesc;
    off_t filepos;
    off_t filelen;

    if (t->sp < 1)
	return "Parameter stack underflow (fileeof expected 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_FILE)
	return "Type mismatch on stack in `fileeof' (expected file descriptor)";

    filedesc = t->stack[t->sp - 1]->file.fd;

    /* Print diagnostics */
    fprintf (dfp, "Checking for eof on fd '%d'.\n", filedesc);

    /* Get current position */
    filepos = lseek(filedesc, 0, SEEK_CUR);
    if(filepos == -1)
	return "Error seeking in file to determine eof (SEEK_CUR).";

    /* Get length of file */
    filelen = lseek(filedesc, 0, SEEK_END);
    if(filelen == -1)
	return "Error seeking in file to determine eof (SEEK_END).";

    /* Reset position in file */
    if(lseek(filedesc, filepos, SEEK_SET) == -1)
	return "Error seeking in file to determine eof (SEEK_SET).";

    /* Update stack */
    t->stack[t->sp - 1]->type = DATA_INT;
    t->stack[t->sp - 1]->integer.value = (filepos == filelen ? 1 : 0);

    return NULL;
}

/*
 * `filegood' checks whether a file descriptor is valid.
 * Pops file descriptor from stack.
 * Pushes 0 onto stack if not valid, or 1 if valid.
 */
static char *filegood(Thread *t) {
    int filedesc;
    int result = 0;

    if (t->sp < 1)
	return "Parameter stack underflow (filegood expected 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_FILE)
	return "Type mismatch on stack in `filegood' (expected file descriptor)";

    filedesc = t->stack[t->sp - 1]->file.fd;

    /* Print diagnostics */
    fprintf (dfp, "Checking for validity of fd '%d'.\n", filedesc);
    
    if(filedesc != -1) {
	result = 1;  /* FIXME - check here for other possible problems
		      * Also - possibly update so that it returns value
		      * describing read / write capabilities of file
		      */
    }

    /* Update stack */
    t->stack[t->sp - 1]->type = DATA_INT;
    t->stack[t->sp - 1]->integer.value = result;

    return NULL;
}

/*
 * `rawtime' puts the UNIX time, as an integer, onto the stack
 */
static char *loewlib_rawtime(Thread *t) {
    if (t->sp == t->stacklimit)
	return "Parameter stack limit exceeded (rawtime needs 1 free space on stack)";
    t->stack[t->sp] = gmalloc(sizeof(Data));
    t->stack[t->sp]->type = DATA_INT;
    t->stack[t->sp]->integer.value = time(NULL);
    t->sp++;
    return NULL;
}


/*
 * `search' runs a search
 * Pops index name and search string from stack.
 * Pushes results onto stack. (Number of strings, then strings.)
 */
static char *loewlib_search(Thread *t) {
    char *index, *query, **results;
    unsigned int count, count1;

    if (t->sp < 2)
	return "Parameter stack underflow (search expected 2 items on stack)";
    if (t->stack[t->sp-1]->type != DATA_STR)
	return "Type mismatch on stack in `search' (expected string)";
    if (t->stack[t->sp-2]->type != DATA_STR)
	return "Type mismatch on stack in `search' (expected string)";

    index = t->stack[t->sp - 1]->string.data;
    query = t->stack[t->sp - 2]->string.data;

    fprintf (dfp, "Running search on index `%s': `%s'\n", index, query);

    results = search_run(index, query);

    free(t->stack[--t->sp]);
    free(t->stack[--t->sp]);

    count = 0;
    while(results[count] != NULL) {
	count++;
    }

    for(count1 = count; count1; count1--) {
	if (t->sp == t->stacklimit)
	    return "Parameter stack limit exceeded (in loew search)";
	t->stack[t->sp] = gmalloc(sizeof(Data));
	t->stack[t->sp]->type = DATA_STR;
	strncpy(t->stack[t->sp]->string.data, results[count1 - 1], MAXSTR - 1);
	t->stack[t->sp]->string.length = strlen(results[count1 - 1]);
	t->sp++;

	free(results[count1 - 1]);
    }
    free(results);

    /* Number of results at top of stack */
    if (t->sp == t->stacklimit)
	return "Parameter stack limit exceeded (in loew search)";
    t->stack[t->sp] = gmalloc(sizeof(Data));
    t->stack[t->sp]->type = DATA_INT;
    t->stack[t->sp]->integer.value = count;
    t->sp++;

    return NULL;
}

/*
 * `addtask' adds a task to the system task queue. It expects its
 * arguments to be, from stack bottom upwards:
 * 
 * (*) A string giving the command. Currently supported commands
 * are
 *      'Visit     visits a file quickly (for e.g. leaving log traces)
 *      'Read      reads a file and saves a local copy
 *      'Setname   sets the robot's nameline
 *      'Messages  reads [esc][a] and saves a local copy
 *      'Add       adds an edit to a file
 *      'Edit      raw-edits a file ([.][a])
 *      'Talker    enters talker
 * 
 * (*) (value,key) pairs. Current keys are:
 * 
 *      'Keypath   for files to be visited/read/edited/added to
 *      'Filename  for saving local copies of things
 *      'Nameline  for setting namelines or adding edits
 *      'Edithook  for passing a function name to return edit lines
 *
 * E.g.
 * 
 *   'Add "MGL1CE" 'Keypath "Special name" 'Nameline 'myfunc 'Edithook addtask
 *   'Setname "golem: another bloody silly nameline" 'Nameline addtask
 *   'Read "MGL1CE" 'Keypath "cam-easy-chat.dump" 'Filename addtask
 */
static char *addtask(Thread *t) {
    /* REMEMBER - cmdkeys and paramkeys _MUST_ be in order*/
    static char *cmdkeys[] = {
	"Add", "Edit", "Menu", "Messages", "Read", "Setname",
	"Talker", "Visit"
    };
    static enum TaskType cmdvals[] = {
	ADD_TO_FILE, EDIT_FILE, CHECK_MENU, READ_MSGS, READ_FILE,
	NAMELINE, ENTER_TALKER, VISIT_FILE
    };
    static char *paramkeys[] = {
	"Edithook", "Filename", "Keypath", "Nameline"
    };
    int s = t->sp - 1;
    int i;
    char *err;
    struct TaskInfo ti;

    ti.edithook = NULL;
    ti.edithook_t = NULL;
    ti.edithook_u = t->user;
    ti.keypath[0] = '\0';
    ti.filename[0] = '\0';
    ti.nameline[1] = '\0';
    ti.nameline[0] = '\1';	       /* indicate no special nameline */

    while (1) {
	if (s < 0)
	    return "Parameter stack underflow (Missing command name for Edithook)";
	i = binsearch (t->stack[s]->string.data, paramkeys,
		       0, sizeof(paramkeys)/sizeof(*paramkeys)-1);
	if (i < 0)
	    break;
	if (s < 1)
	    return "Parameter stack underflow (Missing parameter value for Edithook)";
	switch (i) {
	  case 0:
	    err = NULL;
	    ti.edithook = loew_onefunc(t->stack[s-1]->string.data, t->user,
				       &err);
	    if (err || !ti.edithook)
		return err ? err : "Unable to use edit hook";
	    break;
	  case 1:
	    strncpy(ti.filename, t->stack[s-1]->string.data,
		    sizeof(ti.filename)-1);
	    ti.filename[sizeof(ti.filename)-1] = '\0';
	    break;
	  case 2:
	    strncpy(ti.keypath, t->stack[s-1]->string.data,
		    sizeof(ti.keypath)-1);
	    ti.keypath[sizeof(ti.keypath)-1] = '\0';
	    break;
	  case 3:
	    strncpy(ti.nameline, t->stack[s-1]->string.data,
		    sizeof(ti.nameline)-1);
	    ti.nameline[sizeof(ti.nameline)-1] = '\0';
	    break;
	}
	s -= 2;
    }

    i = binsearch (t->stack[s]->string.data, cmdkeys,
		   0, sizeof(cmdkeys)/sizeof(*cmdkeys)-1);

    if (i < 0)
	return "Command/parameter key not recognised";

    while (t->sp > s)
	free (t->stack[--t->sp]);

    add_task (cmdvals[i], &ti);
    return NULL;
}

void loewlib_setup(void) {
    /* Conversion operations */
    loew_define ("itos10", DEFN_PRIM, (void *)loewlib_itos10, NULL, PRIV_UNRESTRICTED); /* FIXME - this should be a general convertor... */

    /* General operations */
    loew_define ("dup", DEFN_PRIM, (void *)loew_dup, NULL, PRIV_UNRESTRICTED);
    loew_define ("cmp", DEFN_PRIM, (void *)loewlib_cmp, NULL, PRIV_UNRESTRICTED);

    /* String operations */
    loew_define ("split", DEFN_PRIM, (void *)loewlib_split, NULL, PRIV_UNRESTRICTED);
    loew_define (".", DEFN_PRIM, (void *)loewlib_concat, NULL, PRIV_UNRESTRICTED);

    /* Integer operations */
    loew_define ("+", DEFN_PRIM, (void *)plus, NULL, PRIV_UNRESTRICTED);
    loew_define ("-", DEFN_PRIM, (void *)minus, NULL, PRIV_UNRESTRICTED);
    loew_define ("!", DEFN_PRIM, (void *)not, NULL, PRIV_UNRESTRICTED);
    
    loew_define ("random", DEFN_PRIM, (void *)loewlib_random, NULL, PRIV_UNRESTRICTED);

    /* Wierd operations */
    loew_define ("search", DEFN_PRIM, (void *)loewlib_search, NULL, PRIV_STD);
    loew_define ("rawtime", DEFN_PRIM, (void *)loewlib_rawtime, NULL, PRIV_UNRESTRICTED);

    /* Task control operations */
    loew_define ("addtask", DEFN_PRIM, (void *)addtask, NULL, PRIV_STD);
 
    /* File operations */
    loew_define ("fileopen", DEFN_PRIM, (void *)fileopen, NULL, PRIV_ROOT);
    loew_define ("fileread", DEFN_PRIM, (void *)fileread, NULL, PRIV_ROOT);
    loew_define ("filewrite", DEFN_PRIM, (void *)filewrite, NULL, PRIV_ROOT);
    loew_define ("fileclose", DEFN_PRIM, (void *)fileclose, NULL, PRIV_ROOT);
    loew_define ("fileeof", DEFN_PRIM, (void *)fileeof, NULL, PRIV_ROOT);
    loew_define ("filegood", DEFN_PRIM, (void *)filegood, NULL, PRIV_ROOT);

    /* Username and password data */
    loew_define ("username", DEFN_DATA, golem_user, NULL, PRIV_ROOT);
    loew_define ("password", DEFN_DATA, golem_pw, NULL, PRIV_ROOT);
}
