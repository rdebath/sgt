/*
 * golem.c   main module for Golem
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "golem.h"
#include "loew.h"

#define DEFUSERNAME "golem"

#define DEFCONNECTION "/home/richard/stuff/mono/golemn/connect"

#define TERMTYPE "vt220/s0alknxr256c256"

int rfd, wfd;			       /* read and write fds */

static void run_connection(void);

static int expect (char *, ...);
static void wstring (char *);

FILE *dfp;

char connection[100];
char *cargs[10];
int cnargs = 1;

Security *avatar;		       /* the root security descriptor */

Data *golem_user, *golem_pw;

int main(int ac, char **av) {
    int stat;
    char *initcode = NULL;
    FILE *fp;
    char buffer[1024];

    golem_user = malloc(sizeof(Data));
    golem_user->type = DATA_STR;
    strcpy(golem_user->string.data, DEFUSERNAME);
    golem_user->string.length = strlen(golem_user->string.data);

    golem_pw = malloc(sizeof(Data));
    golem_pw->type = DATA_STR;
    golem_pw->string.length = 0;
    golem_pw->string.data[0] = '\000';  /* means no pw given */

    strcpy(connection, DEFCONNECTION);
    cargs[0] = "mono";		       /* default argv[0] */

    while (--ac) {
	char *p = *++av;
	char c, *v;

	if (*p != '-' || !p[1]) break;
	switch (c = *++p) {
	  case 'C': case '0': case 'V': case 'U': case 'P':
	    /* options which take an argument */
	    v = p[1] ? p+1 : --ac ? *++av : NULL;
	    if (!v) {
		fprintf(stderr, "golem: option `-%c' requires an argument\n",
			c);
		return 1;
	    }
	    switch (c) {
	      case 'C':		       /* change connection program */
		strncpy(connection, v, sizeof(connection)-1);
		connection[sizeof(connection)-1] = '\0';
		break;
	      case '0':		       /* change connection argv[0] */
		cargs[0] = v;
		break;
	      case 'V':		       /* give an option to connection argv */
		cargs[cnargs++] = v;
		break;
	      case 'U':		       /* change user name */
		strncpy(golem_user->string.data, v, MAXSTR-1);
		golem_user->string.data[MAXSTR-1] = '\0';
		golem_user->string.length = strlen(golem_user->string.data);
		break;
	      case 'P':		       /* change default pw - read from file */
		fp = fopen(v, "r");
		fgets(buffer, sizeof(buffer), fp);
		fclose(fp);
		strncpy(golem_pw->string.data, buffer, MAXSTR-1);
		golem_pw->string.data[MAXSTR-1] = '\0';
		golem_pw->string.length = strlen(golem_pw->string.data);
		break;
	    }
	    break;
	  default:
	    fprintf(stderr, "golem: unrecognised option `-%c'\n", *p);
	    break;
	}
    }

    if (ac < 1) {
	fprintf(stderr, "usage: golem <cmdfile> [<cmdfile>...]\n");
	return (ac != 1);
    }
    
    /*
     * Open the diagnostics file. FIXME: this should be renameable.
     */
    dfp = fopen("diagnostics", "w");
    if (!dfp) {
	fprintf(stderr, "golem: unable to open file `diagnostics'\n");
	return 1;
    }
    setlinebuf(dfp);

    /*
     * Set up the initial tasks list.
     */
    add_task (LOGIN, NULL);
    add_task (LOGOUT, NULL);

    /*
     * Initialise the Loew subsystem. Set up a root user with all
     * privileges (we give this user asterisks in its name to
     * prevent collision with the mono user called `avatar').
     */
    loew_initdefs();
    loewlib_setup();
    avatar = loew_newusr ("*avatar*", PRIV_ROOT);

    /*
     * Read in the initialisation code from the given file(s).
     */
    {
	FILE *fp;
	char buffer[1024];
	int len = 0;
	while (ac--) {
	    if (av[0][0] == '-' && av[0][1] == '\0')
		fp = stdin;
	    else
		fp = fopen(av[0], "r");
	    while (fgets(buffer, sizeof(buffer), fp)) {
		int blen = strlen(buffer);
		initcode = (initcode ? realloc(initcode, len+blen+1) :
			    malloc(blen+1));
		if (!initcode) {
		    fprintf(stderr, "golem: out of memory at init\n");
		    return 2;
		}
		strcpy(initcode+len, buffer);
		len += blen;
	    }
	    if (fp != stdin)
		fclose(fp);
	    av++;
	}

	if (!initcode)		       /* fake blank initialisation */
	    *(initcode = malloc(1)) = '\0';
    }

    /*
     * Compile and execute the init code.
     */
    {
	char *err;

	Exec *e = loew_compile (initcode, avatar, &err);
	free (initcode);
	if (!e && err) {
	    fputs (err, stderr);
	    fputc ('\n', stderr);
	} else {
	    Thread *t = loew_initthread (avatar, e);
	    while (t->esp) {
		err = loew_step (t);
		if (err) {
		    fputs (err, stderr);
		    fputc ('\n', stderr);
		    break;
		}
	    }
	    loew_freethread (t);
	}
    }

    putenv("TERM=unknown");

    run_connection();

    while (1) {
	int i = expect("\nlogin: ",
		       "Account : ",
		       "Password : ",
		       "[Y]/[N] ?",
		       "Terminal : ",
		       NULL);
	char password[50];
	Defn *d;
	if (i == 4)
	    break;

	switch (i) {
	  case 0:
	    wstring ("mono\r");
	    fprintf(dfp, "Logged in as `mono'\n");
	    break;
	  case 1:
	    d = loew_finddefn("username", avatar);
	    golem_user = d->data;
	    if (golem_user->string.length < 8)
		golem_user->string.data[golem_user->string.length] = '\0';
	    else
		golem_user->string.data[8] = '\0';
	    wstring (golem_user->string.data);
	    wstring ("\r");
	    fprintf(dfp, "Sent user name `%s'\n", golem_user->string.data);
	    break;
	  case 2:
	    d = loew_finddefn("password", avatar);
	    golem_pw = d->data;
	    strncpy (password, golem_pw->string.data, 49);
	    if (golem_pw->string.length < 49)
		password[golem_pw->string.length] = '\0';
	    else
		password[49] = '\0';
	    if (!password[0]) {
		char prompt[256], *gotpw;
		sprintf(prompt, "enter mono password for `%s': ",
			golem_user->string.data);
		gotpw = getpass (prompt);
		strncpy(password, gotpw, sizeof(password)-1);
		password[sizeof(password)-1] = '\0';
		memset(gotpw, 0, strlen(gotpw));
	    }
	    wstring (password);
	    memset(password, 0, strlen(password));
	    golem_pw->string.length = 0;
	    memset(golem_pw->string.data, 0, MAXSTR);
	    wstring ("\r");
	    fprintf(dfp, "Sent password\n");
	    break;
	  case 3:
	    wstring ("n");
	    fprintf(dfp, "Declined to disconnect other satellite(s)\n");
	    break;
	}
    }

    /*
     * Now we set the terminal type. The type we're going to use is
     * vt220/s0alknxr256c256. The options mean:
     * 
     * /s0 means high speed - we get more information, and can also
     * react faster
     * 
     * /a means ANSI colour, so we can retrieve tribs in files
     * 
     * /l means no ANSI insert/delete line sequences, which we
     * wouldn't want to have to deal with anyway. Less effort.
     * 
     * /k means no control-character cursor keys (?) - whatever it
     * is, it's likely to mean more effort, so we ditch it.
     * 
     * /n means don't try to autodetect the terminal size - we'll
     * supply the size ourselves.
     * 
     * /x means use xterm title sequences - we use this for
     * navigational purposes.
     * 
     * /r256 and /c256 give the screen size. 256 columns is big
     * enough to eliminate talker ghosting, and will also get long
     * lines out of files. 256 lines is likely to fit _all_ the
     * currently logged in users on the screen at once.
     *
     * From Mono's POV, vt220 seems to be _more_ featureful than
     * vt320. Strange but true.
     */
    wstring ("\001\013" TERMTYPE "\r");
    fprintf(dfp, "Sent terminal type\n");

    main_loop();

    wait (&stat);
    if (stat) {
	fprintf(stderr, "golem: child exited with status %d\n", stat);
	return 1;
    }

    return 0;
}

static void run_connection(void) {
    int r[2], w[2];
    int pid;

    if (pipe(r) < 0 || pipe(w) < 0) {
	perror("golem: pipe");
	exit(1);
    }

    rfd = r[0];
    wfd = w[1];

    pid = fork();
    if (pid < 0) {
	perror("golem: fork");
	exit(1);
    } else if (pid == 0) {
	/*
	 * we're the child; shift our fds around and then exec the
	 * connection program
	 */
	close(0);
	close(1);
	close(r[0]);
	close(w[1]);
	dup2 (r[1], 1);
	dup2 (w[0], 0);
	close(r[1]);
	close(w[0]);
	cargs[cnargs] = NULL;
	execv(connection, cargs);
	perror("golem: exec");
	exit(1);
    }
    close(r[1]);
    close(w[0]);
}

/*
 * expect and wstring: a very simple request/response analyser. Not
 * used in the main Mono code.
 */

static int expect (char *s1, ...) {
    char buf[1024];
    char *strings[8];		       /* 8 ought to be enough */
    int returns[8];
    int nstr, slen = 0;
    int ret = 1;
    int len, pos = 0;
    va_list ap;

    nstr = 0;
    if (*s1)
	strings[nstr] = s1, returns[nstr++] = 0;
    va_start (ap, s1);
    while ( (strings[nstr] = va_arg(ap, char *)) != NULL ) {
	if (slen < strlen(strings[nstr]))
	    slen = strlen(strings[nstr]);
	if (*strings[nstr])	       /* empty strings are ignored */
	    returns[nstr++] = ret;
	ret++;
    }

    while ( (len = read(rfd, buf+pos, sizeof(buf)-pos)) > 0 ) {
	while (len--) {
	    int i;
	    pos++;
	    for (i=0; i<nstr; i++) {
		if (pos >= strlen(strings[i]) &&
		    !memcmp(buf+pos-strlen(strings[i]),
			    strings[i], strlen(strings[i])))
		    return returns[i]; /* got it */
	    }
	}
	if (pos > slen) {
	    memmove(buf, buf+pos-slen, slen);
	    pos = slen;
	}
    }

    if (!len) {
	fprintf(stderr, "golem: unexpected EOF while waiting"
		" for login prompts\n");
	exit(1);
    } else if (len < 0) {
	perror("golem: read");
	exit(1);
    }
    return -1;			       /* panic */
}

static void wstring (char *s) {
    int len;

    while ( (len = write (wfd, s, strlen(s))) > 0)
	s += len;
    if (len < 0) {
	perror("golem: write");
	exit(1);
    }
}
