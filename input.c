#include <stdio.h>
#include <stdlib.h>

#include "mus.h"
#include "input.h"

#define MAXLEN 512		       /* maximum buffered line length */

static char line[MAXLEN] = "\0";
static char *p = line;
static int quote = 0;
int line_no = 1;
int col_no = 1;
FILE *in_fp;

static void advance(void) {
    if (*p) {
	p++;
	if (!*p)
	    p = fgets(line, sizeof(line), in_fp);
    } else {
	p = fgets(line, sizeof(line), in_fp);
	if (*p)
	    p++;
    }
}

int nd_getchr(void) {
    int c;

    if (!p)
	return EOF;
    if (!*p)
	p = fgets(line, sizeof(line), in_fp);
    if (!p)
	return EOF;

    if (*p==';' && !quote)
	while (p && *p && *p!='\n')
	    advance();
    while (p && *p=='\r')
	advance();

    if (!p)
	return EOF;

    c = *p;
    if (c>='a' && c<='z' && !quote)
	c += 'A'-'a';

    return (c ? c : EOF);
}

int getchr(void) {
    int i = nd_getchr();
    if (i=='\n')
	line_no++, col_no = 0;
    if (i=='"')
	quote = !quote;
    advance();
    col_no++;
    if (i=='\t')
	col_no = (col_no+7)/8 + 1;
    return i;
}

static char *pushback = NULL;
#define wordlen 256		       /* steps to grow buffers by */

char *getword(char **buf) {
    char *p, *word;
    int size = wordlen;

    mfree (*buf);

    if (pushback) {
	*buf = pushback;
	pushback = NULL;
	return *buf;
    } else {
	int i;

	i = nd_getchr();
	while (i==' ' || i=='\t' || i=='\n')
	    getchr(), i = nd_getchr();

	if (i==EOF) {
	    *buf = NULL;
	    return NULL;
	}

	p = word = mmalloc (size * sizeof(*word));

	if (i=='"') {
	    do {
		if (p-word >= size-5) {
		    size += wordlen;
		    word = mrealloc (word, size * sizeof(*word));
		}

		*p++ = i, getchr(), i = nd_getchr();
		while (i!=EOF && i!='\n' && i!='"')
		    *p++ = i, getchr(), i = nd_getchr();
		if (i==EOF || i=='\n') {
		    error(line_no, -1, "unterminated string encountered");
		    break;
		}
		*p++ = '"';
		getchr(), i = nd_getchr();
		if (i=='"')
		    p--;	       /* grotty hack to allow doubled quotes */
	    } while (i=='"');
	} else {
	    if (p-word >= size-5) {
		size += wordlen;
		word = mrealloc (word, size * sizeof(*word));
	    }

	    while (i!=EOF && i!=' ' && i!='\t' && i!='\n')
		*p++ = i, getchr(), i = nd_getchr();
	}
    }
    *p = '\0';
    return *buf = word;
}

void ungetword(char **word) {
    mfree (pushback);
    pushback = *word;
    *word = NULL;
}
