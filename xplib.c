/*
 * Cross-platform functionality for ick-keys.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "icklang.h"
#include "xplib.h"
#include "misc.h"

static int in_init;

static int get_window_choice_order(const char *s)
{
    int n, used_mask, choices, this_choice, shift;
    const char *next;

    shift = 0;
    choices = 0;
    used_mask = 0;
    while (*s) {
        n = strcspn(s, ", ");
        next = s + n + strspn(s + n, ", ");
        if (n == 5 && !memcmp(s, "focus", n))
            this_choice = WINDOW_WITH_FOCUS;
        else if (n == 5 && !memcmp(s, "mouse", n))
            this_choice = WINDOW_UNDER_MOUSE;
        else
            return -1;                 /* parse error */
        if (used_mask & (1 << this_choice))
            return -1;                 /* another parse error */
        used_mask |= (1 << this_choice);
        choices |= (this_choice << shift);
        shift += CHOICE_SHIFT;
        s = next;
    }

    choices |= END_OF_OPTIONS << shift;
    return choices;
}

static int kl_kill_window(void *result, const char **sparams,
                          const int *iparams)
{
    int order;
    if (in_init) {
	error("attempt to kill a window during initialisation");
	return ICK_RTE_USER;
    }
    order = get_window_choice_order(sparams[0]);
    if (order < 0) {
	error("unable to parse window choice order string");
	return ICK_RTE_USER;
    }
    kill_window(order);
    return 0;
}

static int kl_maximise_window(void *result, const char **sparams,
			      const int *iparams)
{
    int order;
    if (in_init) {
	error("attempt to maximise-toggle a window during initialisation");
	return ICK_RTE_USER;
    }
    order = get_window_choice_order(sparams[0]);
    if (order < 0) {
	error("unable to parse window choice order string");
	return ICK_RTE_USER;
    }
    maximise_window(order);
    return 0;
}

static int kl_minimise_window(void *result, const char **sparams,
			      const int *iparams)
{
    int order;
    if (in_init) {
	error("attempt to minimise a window during initialisation");
	return ICK_RTE_USER;
    }
    order = get_window_choice_order(sparams[0]);
    if (order < 0) {
	error("unable to parse window choice order string");
	return ICK_RTE_USER;
    }
    minimise_window(order);
    return 0;
}

static int kl_window_to_back(void *result, const char **sparams,
			     const int *iparams)
{
    int order;
    if (in_init) {
	error("attempt to send a window to back during initialisation");
	return ICK_RTE_USER;
    }
    order = get_window_choice_order(sparams[0]);
    if (order < 0) {
	error("unable to parse window choice order string");
	return ICK_RTE_USER;
    }
    window_to_back(order);
    return 0;
}

static int kl_read_clipboard(void *result, const char **sparams,
			     const int *iparams)
{
    char *ret;
    if (in_init) {
	error("attempt to read the clipboard during initialisation");
	return ICK_RTE_USER;
    }
    ret = read_clipboard();
    if (!ret)
	return ICK_RTE_USER;	       /* error already reported */
    *(char **)result = ret;
    return 0;
}

static int kl_write_clipboard(void *result, const char **sparams,
			      const int *iparams)
{
    if (in_init) {
	error("attempt to write the clipboard during initialisation");
	return ICK_RTE_USER;
    }
    write_clipboard(sparams[0]);
    return 0;
}

static int kl_open_url(void *result, const char **sparams,
		       const int *iparams)
{
    if (in_init) {
	error("attempt to open a URL during initialisation");
	return ICK_RTE_USER;
    }
    open_url(sparams[0]);
    return 0;
}

static int kl_spawn(void *result, const char **sparams,
		    const int *iparams)
{
    if (in_init) {
	error("attempt to spawn a subprocess during initialisation");
	return ICK_RTE_USER;
    }
    spawn_process(sparams[0]);
    return 0;
}

static int kl_debug(void *result, const char **sparams,
		    const int *iparams)
{
    debug_message(sparams[0]);
    return 0;
}

static int kl_set_unix_url_opener(void *result, const char **sparams,
                                  const int *iparams)
{
    set_unix_url_opener_command(sparams[0]);
    return 0;
}

static int kl_register_hotkey(void *result, const char **sparams,
			      const int *iparams)
{
    /*
     * First-line parsing of hot key.
     */
    const char *key = sparams[0];
    int modifiers = 0;

    while (1) {
	if (!strncmp(key, "leftwindows ", 12)) {
	    key += 12;
	    modifiers |= LEFT_WINDOWS;
	} else if (!strncmp(key, "ctrl ", 5)) {
	    key += 5;
	    modifiers |= CTRL;
	} else if (!strncmp(key, "alt ", 4)) {
	    key += 4;
	    modifiers |= ALT;
	} else
	    break;
    }
    if (!register_hotkey(iparams[0], modifiers, key, sparams[0]))
	return ICK_RTE_USER;

    return 0;
}

static int kl_tr(void *result, const char **sparams, const int *iparams)
{
    const char *translatee = sparams[0]; 
    const char *translated = sparams[1];
    const char *string = sparams[2];
    char translation[UCHAR_MAX];
    int i;
    char *ret;

    if (strlen(translatee) != strlen(translated)) {
	error("first two arguments to tr() had different lengths");
	return ICK_RTE_USER;
    }

    for (i = 0; i < UCHAR_MAX; i++)
	translation[i] = i;
    for (i = 0; sparams[0][i]; i++)
	translation[(unsigned char)translatee[i]] = translated[i];

    ret = dupstr(string);
    for (i = 0; ret[i]; i++)
	ret[i] = translation[(unsigned char)ret[i]];

    *(char **)result = ret;

    return 0;
}

static int kl_subst(void *result, const char **sparams, const int *iparams)
{
    const char *replacee = sparams[0];
    const char *replacement = sparams[1];
    const char *string = sparams[2];
    int replacee_len = strlen(replacee);
    int replacement_len = strlen(replacement);
    int string_len = strlen(string);
    int maxlen;
    const char *p, *q;
    char *r, *ret;
    const char *string_end = string + string_len;

    if (replacement_len > replacee_len) {
	maxlen = string_len / replacee_len * replacement_len;
	maxlen += string_len % replacee_len;
    } else
	maxlen = string_len;
    ret = snewn(1 + maxlen, char);

    for (p = string, r = ret; p < string_end; p = q) {
	q = strstr(p, replacee);
	if (!q)
	    q = string_end;
	memcpy(r, p, q-p);
	r += q-p;
	if (q < string_end) {
	    memcpy(r, replacement, replacement_len);
	    r += replacement_len;
	    q += replacee_len;
	}
    }
    *r = '\0';

    *(char **)result = sresize(ret, 1+strlen(ret), char);

    return 0;
}

static void setup_lib(icklib *lib)
{
    ick_lib_addfn(lib, "tr", "SSSS", kl_tr, NULL);
    ick_lib_addfn(lib, "subst", "SSSS", kl_subst, NULL);
    ick_lib_addfn(lib, "kill_window", "VS", kl_kill_window, NULL);
    ick_lib_addfn(lib, "maximise_window", "VS", kl_maximise_window, NULL);
    ick_lib_addfn(lib, "minimise_window", "VS", kl_minimise_window, NULL);
    ick_lib_addfn(lib, "window_to_back", "VS", kl_window_to_back, NULL);
    ick_lib_addfn(lib, "read_clipboard", "S", kl_read_clipboard, NULL);
    ick_lib_addfn(lib, "write_clipboard", "VS", kl_write_clipboard, NULL);
    ick_lib_addfn(lib, "open_url", "VS", kl_open_url, NULL);
    ick_lib_addfn(lib, "spawn", "VS", kl_spawn, NULL);
    ick_lib_addfn(lib, "debug", "VS", kl_debug, NULL);
    ick_lib_addfn(lib, "register_hot_key", "VIS", kl_register_hotkey, NULL);
    ick_lib_addfn(lib, "set_unix_url_opener", "VS",
                  kl_set_unix_url_opener, NULL);
}

static const char **filenames = NULL;
static int nfilenames, filenamesize;

void add_config_filename(const char *filename)
{
    if (nfilenames >= filenamesize) {
	filenamesize = nfilenames * 3 / 2 + 16;
	filenames = sresize(filenames, filenamesize, const char *);
    }
    filenames[nfilenames++] = filename;
}

FILE *currfile;
int currindex;
int readerr;

static int config_getc(void *ctx)
{
    int c;

    if (!currfile) {
	if (currindex >= nfilenames) {
	    return EOF;
	} else {
	    currfile = fopen(filenames[currindex], "r");
	    if (!currfile) {
		error("unable to open ick script file '%s'",
		      filenames[currindex]);
		currindex = nfilenames;
		return EOF;
	    }
	    currindex++;
	}
    }

    assert(currfile);
    c = fgetc(currfile);
    if (c == EOF) {
	currfile = NULL;
	return '\n';
    } else
	return c;
}

static icklib *lib = NULL;
static ickscript *scr = NULL;

void configure(void)
{
    char *err;

    if (!lib) {
	lib = ick_lib_new(0);
	setup_lib(lib);
    }

    if (scr)
	ick_free(scr);

    currfile = NULL;
    currindex = 0;
    readerr = 0;
    scr = ick_compile(&err, "main", "VI", lib, config_getc, NULL);

    if (readerr) {
	if (scr)
	    ick_free(scr);
	scr = NULL;
    } else if (scr) {
	int rte;
	unregister_all_hotkeys();
	in_init = 1;
	rte = ick_exec_limited(NULL, 1000000, 0, 0, scr, -1);
	if (rte) {
	    if (rte != ICK_RTE_USER)
		error("runtime error: %s\n", ick_runtime_errors[rte]);
	    return;
	}
    } else {
	error("compile error: %s\n", err);
	sfree(err);
	return;
    }
}

void run_hotkey(int index)
{
    int rte;

    if (!scr) {
	/*
	 * scr can be NULL if the last run-time reconfiguration
	 * encountered a compile error. In this situation we don't
	 * want to bomb out of the whole program; we want to stay
	 * running, so the user can edit their scripts and do
	 * another reconfig to reload the working version.
	 */
	error("hotkey %d: no valid script loaded\n");
	return;
    }
    in_init = 0;
    rte = ick_exec_limited(NULL, 1000000, 0, 0, scr, index);
    if (rte) {
	if (rte != ICK_RTE_USER)
	    error("runtime error: %s\n", ick_runtime_errors[rte]);
	return;
    }
}
