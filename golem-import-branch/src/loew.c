/*
 * loew.c   implements Loew, the control language for Golem
 */

/*
 * To be done here:
 *
 * Memory freeing after a compiler error
 *
 * Full test, with logalloc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "loew.h"

char error[MAXSTR];

/* --------------------------------------------------------
 * Security descriptors.
 */

Security *loew_newusr (char *name, int privlevel) {
    Security *s = gmalloc(sizeof(Security));
    s->username = gstrdup(name);
    s->privlevel = privlevel;
    s->permissions = NULL;
    s->permsize = s->permnum = 0;
    return s;
}

/* ---------------------------------------------------------------
 * Control primitives. These, being language features, are defined
 * here in the language module, instead of somewhere in the
 * library.
 */

static char *loew_if (Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (if expects 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_INT)
	return "Type mismatch on stack in `if'";
    if (t->stack[t->sp-1]->integer.value == 0)
	t->estack[t->esp-1] = t->estack[t->esp-1]->jump->next;
    free (t->stack[--t->sp]);
    return NULL;
}

static char *loew_ifbrk (Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (if-break expects 1 item on stack)";
    if (t->stack[t->sp-1]->type != DATA_INT)
	return "Type mismatch on stack in `if-break'";
    if (t->stack[t->sp-1]->integer.value != 0)
	t->estack[t->esp-1] = t->estack[t->esp-1]->jump->next;
    free (t->stack[--t->sp]);
    return NULL;
}

static char *loew_jump (Thread *t) {
    t->estack[t->esp-1] = t->estack[t->esp-1]->jump->next;
    return NULL;
}

static char *loew_nop (Thread *t) {
    return NULL;
}

static char *loew_suspend (Thread *t) {
    t->suspended = 1;
    return NULL;
}

/* ---------------------------------------------------------
 * Definition and symbol management.
 */

#define NHASH 17

static struct Defn *defns[NHASH];

static int loew_hash (char *name) {
    int hash = 0;

    while (*name) {
	hash *= 3;
	hash += *name++;
    }

    return hash % NHASH;
}

char *loew_define (char *name, int type, void *data,
		   Security *user, int privlevel) {
    int h;
    Defn *d, *dd;

    h = loew_hash(name);
    for (d = defns[h]; d; d = d->next)
	if (!strcmp(name, d->name))
	    break;
    if (!d) {
	d = gmalloc(sizeof(Defn));
	d->next = defns[h];
	d->nextthis = NULL;
	defns[h] = d;
	d->name = gstrdup(name);
	d->user = user;
	if (!user)
	    d->privlevel = privlevel;
    } else {
	Defn **nt;

	dd = d;
	for (; d; d = d->nextthis) {
	    if (user == d->user)
		break;
	    nt = &d->nextthis;
	}
	if (!d) {
	    d = gmalloc(sizeof(Defn));
	    d->next = dd->next;
	    d->nextthis = NULL;
	    *nt = d;
	    d->name = dd->name;
	    d->user = user;
	    if (!user)
		d->privlevel = privlevel;
	} else {
	    if (d->type != DEFN_DATA) {
		sprintf(error, "attempted redefinition of %s `%s'\n",
			d->type == DEFN_PRIM ? "primitive" : "function",
			d->name);
		return error;
	    } else
		gfree (d->data);
	}
    }
    d->type = type;
    if (type == DEFN_PRIM)
	d->prim = data;
    else if (type == DEFN_FUNC)
	d->func = data;
    else if (type == DEFN_DATA)
	d->data = data;
    return NULL;
}

Defn *loew_finddefn (char *name, Security *user) {
    int h;
    Defn *d, *rootdef;

    h = loew_hash(name);
    for (d = defns[h]; d; d = d->next)
	if (!strcmp(name, d->name))
	    break;
    if (!d)
	return NULL;
    rootdef = NULL;
    for (; d; d = d->nextthis) {
	if (user == d->user)
	    return d;
	else if (!d->user)
	    rootdef = d;
    }
    return rootdef;
}

void loew_initdefs(void) {
    int i;

    for (i=0; i<NHASH; i++)
	defns[i] = NULL;
    /*
     * Define the control primitives here.
     */
    loew_define ("if", DEFN_PRIM, (void *)loew_if, NULL, PRIV_UNRESTRICTED);
    loew_define ("else", DEFN_PRIM, (void *)loew_jump, NULL, PRIV_UNRESTRICTED);
    loew_define ("endif", DEFN_PRIM, (void *)loew_nop, NULL, PRIV_UNRESTRICTED);
    loew_define ("then", DEFN_PRIM, (void *)loew_nop, NULL, PRIV_UNRESTRICTED);

    loew_define ("suspend", DEFN_PRIM, (void *)loew_suspend, NULL, PRIV_UNRESTRICTED);

    loew_define ("loop", DEFN_PRIM, (void *)loew_nop, NULL, PRIV_LOOP_CONTROL);
    loew_define ("break", DEFN_PRIM, (void *)loew_jump, NULL, PRIV_LOOP_CONTROL);
    loew_define ("if-break", DEFN_PRIM, (void *)loew_ifbrk, NULL, PRIV_LOOP_CONTROL);
    loew_define ("endloop", DEFN_PRIM, (void *)loew_jump, NULL, PRIV_LOOP_CONTROL);
}

Data *loew_dupdata(Data *d) {
    Data *dd;

    dd = gmalloc(sizeof(Data));
    *dd = *d;			       /* structure copy */
    return dd;
}

/* --------------------------------------------------------
 * The scanner and compiler routines.
 */

/*
 * Token returned from the scanner. TOK_ID just has `string' set to
 * the name. TOK_NUM has `string' set to the text representation
 * and `ivalue' the numeric value. TOK_STR has `string' pointing to
 * the string contents and `ivalue' the length. TOK_ERROR is an
 * error message...
 */
typedef struct Token {
    enum {
	TOK_EOF, TOK_ID, TOK_NUM, TOK_STR, TOK_FUNC, TOK_LBRACE,
	TOK_RBRACE, TOK_VAR, TOK_EXPORT, TOK_ERROR
    } type;
    char string[MAXSTR];
    int ivalue;
} Token;
#define loew_isspace(x)  ( (x) <= ' ' || (x) > '~' )

Token loew_scan (char **program) {
    Token t;

    while (**program && (loew_isspace(**program) || **program == '#')) {

	if (**program == '#') {
	    (*program)++;
	    while (**program && **program != '\n')
		(*program)++;
	} else {
	    (*program)++;
	}
    }

    if (!**program) {
	t.type = TOK_EOF;
	return t;
    }


    if (**program == '"') {
	char *p = t.string;

	(*program)++;
	while (**program && **program != '"') {
	    char c, d = *(*program)++;
	    if (d >= ' ' && d <= '~' && d != '\\')
		c = d;
	    else if (d == '\\') {
		if (!**program) {
		    strcpy(t.string, "Unexpected EOF in string constant");
		    t.type = TOK_ERROR;
		    return t;
		}
		d = *(*program)++;
		if (d == 'n')
		    c = '\n';
		else if (d == 'r')
		    c = '\r';
		else if (d == 'e')
		    c = '\033';
		else if (d == 'x') {
		    int dd = 2, val = 0, cc;
		    while (dd>0 && **program && isxdigit(**program)) {
			dd--;
			cc = *(*program)++;
			val = val * 16 + (cc > '9' ? (cc&0xDF)-55 : cc-'0');
		    }
		    c = val;
		} else if (d >= '0' && d <= '7') {
		    int dd = 3;
		    c = 0;
		    while (dd>0 && d>='0' && d<='7') {
			dd--;
			c = c * 8 + d - '0';
			d = *(*program);
			if (d && dd)
			    (*program)++;
		    }
		} else
		    c = d;
	    } else
		continue;
	    if (p - t.string < MAXSTR-1)
		*p++ = c;
	}
	if (**program == '"') {
	    t.ivalue = p - t.string;
	    t.type = TOK_STR;
	    (*program)++;
	} else {
	    strcpy(t.string, "Unexpected EOF in string constant");
	    t.type = TOK_ERROR;
	    return t;
	}
    } else if (**program == '\'') {
	char *p = t.string;

	(*program)++;
	t.ivalue = 0;

	while (**program && !loew_isspace(**program))
	    if (p - t.string < MAXSTR-1) {
		*p++ = *(*program)++;
		t.ivalue++;
	    }
	*p++ = '\0';
	t.type = TOK_STR;
    } else {
	char *p = t.string;

	while (**program && !loew_isspace(**program))
	    if (p - t.string < MAXSTR-1)
		*p++ = *(*program)++;

	*p++ = '\0';

	if (!t.string[strspn(t.string, "0123456789")] ||
	    (t.string[0] == '-' && t.string[1] &&
	     !t.string[1+strspn(t.string+1, "0123456789")])) {
	    t.type = TOK_NUM;
	    t.ivalue = atoi(t.string);
	} else {
	    if (!strcmp(t.string, "{"))
		t.type = TOK_LBRACE;
	    else if (!strcmp(t.string, "}"))
		t.type = TOK_RBRACE;
	    else if (!strcmp(t.string, "function"))
		t.type = TOK_FUNC;
	    else if (!strcmp(t.string, "var"))
		t.type = TOK_VAR;
	    else if (!strcmp(t.string, "export"))
		t.type = TOK_EXPORT;
	    else
		t.type = TOK_ID;
	}
    }
    return t;
}

Exec *loew_compile (char *program, Security *user, char **err) {
    Token t;
    Exec *head = NULL, **tail = &head, *curr, *e;
    char *erroor;		       /* :) */
    struct CompileCtx {
	struct CompileCtx *next, *nextloop;
	enum { IF, ELSE, LOOP, FN } type;
	Exec *ref, *ref2, **tail;
	char *fname;
    } *cstk = NULL, *loop = NULL, *ctmp;

    *err = NULL;		       /* init value */

    while (1) {
	t = loew_scan (&program);

	if (t.type == TOK_EOF)
	    break;

	if (t.type == TOK_NUM || t.type == TOK_STR || t.type == TOK_ID) {
	    *tail = curr = gmalloc(sizeof(Exec));
	    tail = &curr->next;
	    *tail = NULL;
	}

	if (t.type == TOK_ERROR) {
	    strcpy (error, t.string);
	    *err = error;
	    goto panic;
	}

	if (t.type == TOK_NUM) {
	    curr->type = EXEC_CONSTANT;
	    curr->data = gmalloc(sizeof(Data));
	    curr->data->type = DATA_INT;
	    curr->data->integer.value = t.ivalue;
	} else if (t.type == TOK_STR) {
	    curr->type = EXEC_CONSTANT;
	    curr->data = gmalloc(sizeof(Data));
	    curr->data->type = DATA_STR;
	    curr->data->string.length = t.ivalue;
	    memcpy (curr->data->string.data, t.string, t.ivalue);
	    curr->data->string.data[t.ivalue] = '\0';
	} else if (t.type == TOK_ID) {
	    char *p = t.string;
	    if (*p == '=' && p[1]) {
		p++;
		curr->type = EXEC_ASSIGN;
	    } else
		curr->type = EXEC_OBJECT;
	    curr->defn = loew_finddefn (p, user);
	    if (!curr->defn) {
		sprintf(error, "undefined identifier `%s'\n", p);
		*err = error;
		goto panic;
	    }
	    if (curr->type == EXEC_OBJECT) {
		if (!strcmp(p, "if")) {
		    ctmp = gmalloc(sizeof(*ctmp));
		    ctmp->next = cstk;
		    ctmp->nextloop = loop;
		    ctmp->type = IF;
		    ctmp->ref = curr;
		    cstk = ctmp;
		} else if (!strcmp(p, "else")) {
		    if (!cstk || cstk->type != IF) {
			sprintf(error, "unmatched `else'");
			*err = error;
			goto panic;
		    }
		    cstk->type = ELSE;
		    cstk->ref->jump = curr;
		    cstk->ref = curr;
		} else if (!strcmp(p, "endif")) {
		    if (!cstk || (cstk->type != IF && cstk->type != ELSE)) {
			sprintf(error, "unmatched `endif'");
			*err = error;
			goto panic;
		    }
		    cstk->ref->jump = curr;
		    ctmp = cstk;
		    cstk = cstk->next;
		    free (ctmp);
		} else if (!strcmp(p, "loop")) {
		    ctmp = gmalloc(sizeof(*ctmp));
		    ctmp->next = cstk;
		    ctmp->nextloop = loop;
		    ctmp->type = LOOP;
		    ctmp->ref = ctmp->ref2 = curr;
		    cstk = ctmp;
		    loop = ctmp;
		} else if (!strcmp(p, "break")) {
		    if (!loop) {
			sprintf(error, "`break' outside `loop'");
			*err = error;
			goto panic;
		    }
		    curr->jump = loop->ref;
		    loop->ref = curr;
		} else if (!strcmp(p, "if-break")) {
		    if (!loop) {
			sprintf(error, "`if-break' outside `loop'");
			*err = error;
			goto panic;
		    }
		    curr->jump = loop->ref;
		    loop->ref = curr;
		} else if (!strcmp(p, "endloop")) {
		    if (!cstk || cstk->type != LOOP) {
			sprintf(error, "unmatched `endloop'");
			*err = error;
			goto panic;
		    }
		    e = loop->ref;
		    while (e != loop->ref2) {
			Exec *f = e->jump;
			e->jump = curr;
			e = f;
		    }
		    curr->jump = e;
		    ctmp = cstk;
		    cstk = cstk->next;
		    free (ctmp);
		}
	    }
	} else if (t.type == TOK_VAR) {
	    t = loew_scan (&program);

	    if (cstk) {
		sprintf(error, "variable definition not at top level");
		*err = error;
		goto panic;
	    }

	    if (t.type != TOK_ID) {
		sprintf(error, "expected identifier after `var'");
		*err = error;
		goto panic;
	    }

	    erroor = loew_define (t.string, DEFN_DATA, NULL, user, 0);
	    if (erroor) {
		*err = erroor;
		goto panic;
	    }
	} else if (t.type == TOK_EXPORT) {
	    Defn *d;

	    t = loew_scan (&program);

	    if (cstk) {
		sprintf(error, "`export' statement not at top level");
		*err = error;
		goto panic;
	    }

	    if (t.type != TOK_ID) {
		sprintf(error, "expected identifier after `export'");
		*err = error;
		goto panic;
	    }

	    d = loew_finddefn (t.string, user);
	    if (!d) {
		sprintf(error, "unable to find object `%s' to export",
			t.string);
		*err = error;
		goto panic;
	    }
	    if (d->user != user) {
		sprintf(error,
			"unable to find local object `%s' to export",
			t.string);
		*err = error;
		goto panic;
	    }
	    if (loew_finddefn (t.string, NULL)) {
		sprintf(error, "global object `%s' already exists",
			t.string);
		*err = error;
		goto panic;
	    }
	    if (user->privlevel > PRIV_EXPORT) {
		sprintf(error, "Security violation: attempt to export");
		*err = error;
		goto panic;
	    }
	    d->user = NULL;

	    t = loew_scan (&program);
	    if (t.type != TOK_NUM) {
		sprintf(error,
			"expected numeric privilege level for `export'");
		*err = error;
		goto panic;
	    }
	    d->privlevel = t.ivalue;

	    if (d->type == DEFN_DATA) {
		t = loew_scan (&program);
		if (t.type != TOK_NUM) {
		    sprintf(error,
			    "expected numeric write-privilege level"
			    " for `export'");
		    *err = error;
		    goto panic;
		}
		d->wprivlevel = t.ivalue;
	    }
	} else if (t.type == TOK_FUNC) {
	    char name[MAXSTR];

	    if (cstk) {
		sprintf(error, "function definition not at top level");
		*err = error;
		goto panic;
	    }

	    t = loew_scan (&program);
	    if (t.type != TOK_ID) {
		sprintf(error, "expected identifier after `func'");
		*err = error;
		goto panic;
	    }
	    strcpy (name, t.string);

	    t = loew_scan (&program);
	    if (t.type != TOK_LBRACE) {
		sprintf(error, "expected `{' in function definition");
		*err = error;
		goto panic;
	    }

	    ctmp = gmalloc(sizeof(*ctmp));
	    ctmp->next = cstk;
	    ctmp->nextloop = loop;
	    ctmp->type = FN;
	    ctmp->tail = tail;
	    ctmp->fname = gstrdup(name);
	    ctmp->ref = NULL;
	    tail = &ctmp->ref;
	    cstk = ctmp;
	} else if (t.type == TOK_RBRACE) {
	    if (!cstk || cstk->type != FN) {
		sprintf(error, "unmatched `}'");
		*err = error;
		goto panic;
	    }
	    tail = cstk->tail;
	    erroor = loew_define (cstk->fname, DEFN_FUNC, cstk->ref, user, 0);
	    if (erroor) {
		*err = erroor;
		goto panic;
	    }
	    gfree(cstk->fname);
	    ctmp = cstk;
	    cstk = cstk->next;
	    gfree(ctmp);
	}
    }
    return head;

    /*
     * We come here on error; we must free all the memory we have
     * so far allocated (FIXME do this!) and return NULL.
     */
    panic:
    return NULL;
}

/*
 * Restricted form of `compile': passed a single function name.
 */
Exec *loew_onefunc (char *func, Security *user, char **err) {
    Exec *result;
    Defn *d;

    *err = NULL;		       /* init value */

    d = loew_finddefn (func, user);
    if (!d) {
	sprintf(error, "undefined identifier `%s'\n", func);
	*err = error;
	return NULL;
    }

    result = gmalloc(sizeof(Exec));
    result->next = result->jump = NULL;
    result->type = EXEC_OBJECT;
    result->defn = loew_finddefn (func, user);

    return result;
}

void loew_freeexec (Exec *e) {
    Exec *tmp;
    while (e) {
	tmp = e;
	e = e->next;
	if (tmp->type == EXEC_CONSTANT)
	    gfree(tmp->data);
	gfree(tmp);
    }
}

/* --------------------------------------------------------------
 * The thread management and run-time routines.
 */

Thread *loew_initthread (Security *user, Exec *start) {
    Thread *t = gmalloc(sizeof(Thread));

    t->cycles = 0;
    t->cyclelimit = 0;		       /* FIXME */
    t->stacklimit = 128;	       /* FIXME */
    t->estacklimit = 32;	       /* FIXME */
    t->sp = 0;
    t->esp = 1;
    t->suspended = 0;
    t->stack = gmalloc(t->stacklimit * sizeof(*t->stack));
    t->estack = gmalloc(t->estacklimit * sizeof(*t->estack));
    t->estack[0] = start;
    t->user = user;

    return t;
}

char *loew_step (Thread *t) {
    Exec *e;
    char *result = NULL;

    t->suspended = 0;

    if (t->esp == 0)
	return "Thread had already terminated";
    while (t->estack[t->esp-1] == NULL)
	if (--t->esp == 0)
	    return NULL;	       /* termination */
    e = t->estack[t->esp-1];
    switch (e->type) {
      case EXEC_OBJECT:
	if (e->defn->user != t->user && e->defn->user)
	    return "Panic: security descriptor mismatch in thread";
	/*
	 * This `if' doesn't have to worry about t->user being
	 * NULL, since the previous `if' will have ruled that
	 * possibility out.
	 */
	if (!e->defn->user && e->defn->privlevel < t->user->privlevel) {
	    /*
	     * FIXME: search for a specific permission before
	     * returning this error.
	     */
	    return "Security violation: attempt to execute forbidden object";
	}
	if (e->defn->type == DEFN_PRIM) {
	    result = e->defn->prim (t);
	    if (e == t->estack[t->esp-1])
		t->estack[t->esp-1] = e->next;
	} else if (e->defn->type == DEFN_FUNC) {
	    if (t->esp == t->estacklimit)
		return "Execution stack limit exceeded";
	    t->estack[t->esp-1] = e->next;
	    t->estack[t->esp++] = e->defn->func;
	} else if (e->defn->type == DEFN_DATA) {
	    if (t->sp == t->stacklimit)
		return "Parameter stack limit exceeded";
	    if (!e->defn->data)
		return "Variable referenced before initialisation";
	    t->stack[t->sp++] = loew_dupdata (e->defn->data);
	    t->estack[t->esp-1] = e->next;
	}
	break;
      case EXEC_ASSIGN:
	if (t->sp == 0)
	    return "Parameter stack underflow (assignment expects 1 item on stack)";
	if (e->defn->type != DEFN_DATA)
	    return "Attempt to assign to a non-data object";
	if (e->defn->wprivlevel < t->user->privlevel)
	    return "Security violation: attempt to assign to a forbidden"
	    " object";
	gfree (e->defn->data);
	e->defn->data = t->stack[--t->sp];
	t->estack[t->esp-1] = e->next;
	break;
      case EXEC_CONSTANT:
	if (t->sp == t->stacklimit)
	    return "Parameter stack limit exceeded";
	t->stack[t->sp++] = loew_dupdata (e->data);
	t->estack[t->esp-1] = e->next;
	break;
    }
    return result;
}

void loew_freethread (Thread *t) {
    while (t->sp > 0) {
	t->sp--;
	gfree (t->stack[t->sp]);
    }
    gfree (t->stack);
    gfree (t->estack);
    gfree (t);
}

#ifdef TESTMODE

char *plus(Thread *t) {
    if (t->sp < 2)
	return "Parameter stack underflow (+ expects 2 items on stack)";
    t->stack[t->sp - 2]->integer.value += t->stack[t->sp - 1]->integer.value;
    free (t->stack[--t->sp]);
    return NULL;
}

char *not(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (- expects 2 items on stack)";
    t->stack[t->sp-1]->integer.value = !t->stack[t->sp-1]->integer.value;
    return NULL;
}

char *equal(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (= expects 1 item on stack)";
    switch (t->stack[--t->sp]->type) {
      case DATA_INT:
	printf("%d\n", t->stack[t->sp]->integer.value);
	break;
      case DATA_STR:
	printf("%s\n", t->stack[t->sp]->string.data);
	break;
      case DATA_FILE:
	printf("<fd%d>\n", t->stack[t->sp]->file.fd);
	break;
    }
    gfree(t->stack[t->sp]);
    return NULL;
}

char *dup(Thread *t) {
    if (t->sp < 1)
	return "Parameter stack underflow (dup expects 1 item on stack)";
    if (t->sp == t->stacklimit)
	return "Parameter stack limit exceeded";
    t->stack[t->sp] = loew_dupdata (t->stack[t->sp-1]);
    t->sp++;
    return NULL;
}

char buffer[16384];
int main(void) {
    int len = 0;
    Exec *e;
    Thread *t;
    Security *s;
    char *err;

    loew_initdefs();
    loew_define ("+", DEFN_PRIM, plus, NULL, PRIV_UNRESTRICTED);
    loew_define ("=", DEFN_PRIM, equal, NULL, PRIV_UNRESTRICTED);
    loew_define ("!", DEFN_PRIM, not, NULL, PRIV_UNRESTRICTED);
    loew_define ("dup", DEFN_PRIM, dup, NULL, PRIV_UNRESTRICTED);

    s = loew_newusr ("anakin", PRIV_ROOT);

    while (len < sizeof(buffer) &&
	   fgets(buffer+len, sizeof(buffer)-len, stdin))
	len += strlen(buffer+len);

    e = loew_compile (buffer, s, &err);
    if (!e) {
	fputs (err, stderr);
	fputc ('\n', stderr);
	return 0;
    }
    t = loew_initthread (s, e);
    while (t->esp) {
	err = loew_step (t);
	if (err) {
	    fputs (err, stderr);
	    fputc ('\n', stderr);
	    break;
	}
    }
    loew_freethread (t);
    loew_freeexec(e);

    return 0;
}

#endif
