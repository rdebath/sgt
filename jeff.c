/*
 * jeff.c: transform files containing multi-line records into a
 * one-line-per-record form by substituting CRs for the LFs and
 * adding new LFs as record separators. Useful for using normal
 * Unix text tools to do record-by-record processing (grep, sort,
 * uniq, tac, comm, whatever) and then doing the simple transform
 * inversion (remove LFs, convert CRs to LFs) to recover the normal
 * file format.
 * 
 * Usage:
 * 
 *   jeff [-e | -d] [<type>] [file [file...]]
 * 
 * where:
 * 
 *   -e      transform multiline records into one line (default)
 *   -d      transform one-line-per-record back to multiline
 * 
 * and <type> can be one of:
 * 
 *   --mono  process Mono messages with attributes, as downloaded by golem
 *   --mono-plain  process Mono messages in a text file without attributes
 *   --mbox  process an mbox mail folder
 *   --svnlog  process the output from `svn log' into individual log entries
 *   --gitlog  process the output from `git log' into individual log entries
 * 
 * note:
 * 
 *   - will read from standard input if no input files provided
 *   - output is written to stdout (of course)
 *   - in `-d' mode, the file type is ignored (the inverse
 *     transform is the same for all file types)
 *   - if you symlink `unjeff' to the binary, running it by that
 *     name assumes -d as the default.
 */

/*
 * TODO:
 * 
 *  - I'd quite like to make this a publish-worthy utility in the
 *    `utils' collection. However, the Mono message mode would
 *    require a lot of explanation, and also probably shouldn't be
 *    the default. Would it be worth it just for other modes?
 *     + perhaps one possibility would be to make the code modular:
 * 	 have a central jeff.c and a subfile containing each mode,
 * 	 and then I could keep the Mono modes somewhere else in svn
 * 	 and symlink them in for my own compiles?
 *     + alternatively, modularity at runtime? ~/.jeff containing
 * 	 .so modules (err, probably not) or regexp-based recogniser
 * 	 scripts (also a lot of work)?
 *     + but both of these options would diminish its utility to
 * 	 _me_, since half the virtue of the utils collection is its
 * 	 ease of installation on a new Unix box. Hmmm. Needs more
 * 	 thought.
 * 
 *  - Are there other record types which benefit from a good
 *    jeffing?
 * 
 *  - I'm idly wondering about automatic recognition of the file
 *    type. `jeff --mono-plain' is a lot of verbiage; you ideally
 *    want to type `jeff thingy | grep wossname | unjeff' and have
 *    things Just Work with a minimum of fiddly detail. The usual
 *    caveats about over-clever software may be assumed to have
 *    been considered, but nonetheless it might still be a useful
 *    mode. Perhaps the existing mode selectors should remain
 *    around to force a specific interpretation for the rare
 *    occasion the auto-recogniser makes an error.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

#define EXPECT(s) do { \
    if (strncmp(line, (s), sizeof((s))-1) != 0) \
	return FALSE; \
    line += sizeof((s))-1; \
} while (0)

int mono_start_line(char *line)
{
    EXPECT("\033[;1;7;44m-------------------------------"
	   "--------------------[\033[;7;44m");
    if (strlen(line) < 24)
	return FALSE;
    line += 24;
    EXPECT("\033[;1;7;44m]--\033[m\n");
    return TRUE;
}

int mono_plain_start_line(char *line)
{
    EXPECT("---------------------------------------------------[");
    if (strlen(line) < 24)
	return FALSE;
    line += 24;
    EXPECT("]--\n");
    return TRUE;
}

int mbox_start_line(char *line)
{
    EXPECT("From ");
    return TRUE;
}

int svnlog_start_line(char *line)
{
    /*
     * Internal state variable:
     * 
     *  - 0 means we are ready to interpret a line of dashes as
     *    beginning a new revision
     * 
     *  - -1 means we've seen a line of dashes and are expecting a
     *    line giving log length
     * 
     *  - -1-n (n>0) means we know the log message contains n
     *    lines, but it hasn't started yet because we haven't seen
     *    the blank line (perhaps it's `svn log -v' and contains
     *    the changed-paths section)
     * 
     *  - n (n>0) means we are ignoring n lines of log message
     *    before being ready to see a new revision again
     */
    static int state = 0;
    if (state == 0) {
        if (!strcmp(line, "----------------------------------------"
                    "--------------------------------\n")) {
            state = -1;
            return TRUE;
        } else {
            /*
             * In this situation we were expecting to see a line of
             * dashes, but we saw something else instead. This
             * _shouldn't_ happen. Perhaps there should be a
             * --strict option which throws errors on violation of
             * the expected input format? For robustness, though, I
             * think it might be better just to sit tight and see
             * if we see our dashes on the next line instead.
             */
            return FALSE;
        }
    } else if (state == -1) {
        /*
         * This is the line after a line of dashes. We expect to
         * see it ended by '| %d line[s]'.
         */
        char *bar = strrchr(line, '|');
        int nlines;
        if (bar == NULL ||
            sscanf(bar, "| %d lines", &nlines) < 1 ||
            sscanf(bar, "| %d line", &nlines) < 1) {
            /*
             * This line wasn't what we expected. Even more tempted
             * than above to throw an error here.
             */
            return FALSE;
        } else {
            /*
             * We have our count of log lines in nlines.
             */
            state = -1 - nlines;
            return FALSE;
        }
    } else if (state < -1) {
        /*
         * A blank line switches us into log-ignoring mode.
         */
        if (line[0] == '\n')
            state = -1 - state;
        return FALSE;
    } else if (state > 0) {
        /*
         * Now we're ignoring lines of log message, after which
         * we'll be ready to see a dashes introducer again.
         */
        state--;
        return FALSE;
    }
}

int gitlog_start_line(char *line)
{
    /*
     * git is really simple for this purpose: all but the header
     * information about each commit is indented. So we just have to
     * look for 'commit' at the start of the line, and we're done.
     */
    EXPECT("commit ");
    return TRUE;
}

char *fgetline(FILE *fp)
{
    char *ret = malloc(512);
    int size = 512, len = 0;

    if (!ret)
	return NULL;

    while (fgets(ret + len, size - len, fp)) {
	len += strlen(ret + len);
	if (ret[len-1] == '\n')
	    break;		       /* got a newline, we're done */
	size = len + 512;
	ret = realloc(ret, size);
	if (!ret)
	    return NULL;
    }
    if (len == 0) {		       /* first fgets returned NULL */
	free(ret);
	return NULL;
    }
    ret[len] = '\0';
    return ret;
}

void do_file(FILE *fp, int unjeff, int (*start_line)(char *line))
{
    if (unjeff) {
	/*
	 * Unjeffing does not depend on the start_line function. We
	 * destroy all \n, and replace all \r with \n.
	 */
	char buf[4096];
	int len;
	int i, j;

	while ((len = fread(buf, 1, sizeof(buf), stdin)) > 0) {
	    for (i = j = 0; i < len; i++) {
		if (buf[i] == '\r')
		    buf[j++] = '\n';
		else if (buf[i] != '\n')
		    buf[j++] = buf[i];
	    }
	    fwrite(buf, 1, j, stdout);
	}
    } else {
	/*
	 * To jeff, we read each line of the file and translate its
	 * trailing \r into a \n. Then we write a \n before output
	 * if the line matches the start_line() criterion and we're
	 * not right at the start of the file; finally, we write a
	 * trailing \n if we've output anything at all.
	 */
	int firstline = TRUE;

	while (1) {
	    char *p = fgetline(fp);
	    char *q;

	    if (!firstline && (!p || start_line(p)))
		fputc('\n', stdout);
	    if (!p)
		break;
	    for (q = p; *q; q++)
		if (*q == '\n')
		    *q = '\r';
	    fputs(p, stdout);
	    firstline = FALSE;

	    free(p);
	}
    }
}

int main(int argc, char **argv)
{
    int unjeff = FALSE;
    int (*start_line)(char *line) = mono_start_line;
    int doing_opts = TRUE;
    int got_args = FALSE;
    int errors = FALSE;
    char *pname;

    {
	int i = strlen(argv[0]) - 1;
	while (i > 0 && argv[0][i] != '/')
	    i--;
	if (argv[0][i] == '/')
	    i++;
	pname = argv[0]+i;
	if (!strcmp(pname, "jeff"))
	    unjeff = FALSE;
	else if (!strcmp(pname, "unjeff"))
	    unjeff = TRUE;
    }

    while (--argc) {
	char *p = *++argv;

	if (doing_opts && *p == '-' && p[1]) {
	    int c = *++p;

	    if (c == '-') {
		p++;
		if (!*p) {
		    doing_opts = FALSE;
		} else if (!strcmp(p, "mono")) {
		    start_line = mono_start_line;
		} else if (!strcmp(p, "plain-mono") ||
			   !strcmp(p, "mono-plain")) {
		    start_line = mono_plain_start_line;
		} else if (!strcmp(p, "mbox")) {
		    start_line = mbox_start_line;
		} else if (!strcmp(p, "svnlog")) {
		    start_line = svnlog_start_line;
		} else if (!strcmp(p, "gitlog")) {
		    start_line = gitlog_start_line;
		} else {
		    fprintf(stderr, "%s: unrecognised option '--%s'\n",
			    pname, p);
		    errors = TRUE;
		}
	    } else {
		do {
		    switch (c) {
		      case 'e':
			unjeff = FALSE;
			break;
		      case 'd':
			unjeff = TRUE;
			break;
		      default:
			fprintf(stderr, "%s: unrecognised option '-%c'\n",
				pname, c);
			errors = TRUE;
			break;
		    }
		} while ((c = *++p));
	    }
	} else {
	    if (!errors) {
		if (!strcmp(p, "-")) {
		    do_file(stdin, unjeff, start_line);
		} else {
		    FILE *fp = fopen(p, "r");
		    if (!fp) {
			fprintf(stderr, "%s: unable to open '%s': %s\n",
				pname, p, strerror(errno));
			errors = TRUE;
		    } else {
			do_file(fp, unjeff, start_line);
			fclose(fp);
		    }
		}
	    }
	    got_args = TRUE;
	}
    }
    if (!errors && !got_args) {
	do_file(stdin, unjeff, start_line);
    }

    return errors ? 1 : 0;
}
