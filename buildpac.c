/*
 * buildpac.c: code to build a valid proxyconfig.pac, given an
 * input proxyconfig.pac and a compiled Ick script.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "icklang.h"

char *build_pac(char *userpac, ickscript *scr, int port)
{
    char *moduserpac;
    char *jsscript;
    char fpfu[1024];
    char **avoidids;
    int navoidids, avoididsize;

    avoidids = NULL;
    navoidids = avoididsize = 0;

    /*
     * First we go through the user .PAC and semi-lex it. We
     * aren't concerned with all of the lexical structure of
     * Javascript, and certainly not the syntax; we have to
     * understand comments and string literals in order to skip
     * them, but we're only actually _interested_ in unqualified
     * identifier tokens (i.e. not preceded by a period token).
     * When we find one, we do two things:
     * 
     * 	- if the identifier is called "FindProxyForURL" then we
     * 	  prepend "user_". We also do this if the identifier is
     * 	  called "FindProxyForURL" with any number of copies of
     * 	  "user_" prepended (to prevent our renamed identifier
     * 	  from clashing with another one).
     * 
     * 	- after that, we add the token to our list of tokens used
     * 	  in the user .PAC, which we then pass to the Ick->JS
     * 	  converter to ensure the URL rewriting script can coexist
     * 	  in the same JS source file.
     */
    {
	char *p, *tok, *outtok, *out;
	int lastdot = 0;
	int modmaxsize;

	/*
	 * Preallocate our output buffer for the modified version
	 * of the user PAC. Rather than dynamically growing it as
	 * we go along, we'll simply figure out the worst possible
	 * case and use that, because it isn't too bad: if the
	 * entire input script consists of copies of the word
	 * "FindProxyForURL" plus one separating character, then
	 * each of those will become "user_FindProxyForURL" plus
	 * the separator, turning 16 characters into 21.
	 */
	modmaxsize = (21 * strlen(userpac) + 15) / 16;   /* round up */
	moduserpac = malloc(modmaxsize + 1);   /* and allow for trailing NUL */

	p = userpac;
	out = moduserpac;
	while (*p) {
	    if (*p == '_' || (*p && isalnum((unsigned char)*p))) {
		/*
		 * Numeric literal or identifier.
		 */
		tok = p;
		outtok = out;
		while (*p && (*p == '_' || (*p && isalnum((unsigned char)*p))))
		    p++;

		if (!lastdot && !isdigit((unsigned char)tok[0])) {
		    /*
		     * Identifier not preceded by a dot.
		     */
		    int toklen = p - tok;
		    int outtoklen;

		    if (toklen >= 15 && (toklen % 5 == 0)) {
			/*
			 * Right length to be one of our
			 * rename-requiring tokens. Is it?
			 */
			if (!memcmp(p - 15, "FindProxyForURL", 15)) {
			    int i;
			    for (i = toklen - 20; i >= 0; i -= 5)
				if (memcmp(tok + i, "user_", 5))
				    break;
			    if (i < 0) {
				/*
				 * Yes! Rename this token.
				 */
				strcpy(out, "user_");
				out += 5;
			    }
			}
		    }

		    memcpy(out, tok, toklen);
		    out += toklen;
		    outtoklen = out - outtok;

		    /*
		     * Having optionally renamed the identifier,
		     * we now add it to our avoid list.
		     */
		    if (navoidids >= avoididsize) {
			avoididsize = (navoidids * 3 / 2) + 32;
			avoidids = realloc(avoidids, avoididsize *
					   sizeof(*avoidids));
		    }
		    avoidids[navoidids] = malloc(outtoklen + 1);
		    memcpy(avoidids[navoidids], outtok, outtoklen);
		    avoidids[navoidids][outtoklen] = '\0';
		    navoidids++;
		} else {
		    /*
		     * Numeric literal, or identifier after a dot.
		     * Just copy.
		     */
		    int toklen = p - tok;
		    memcpy(out, tok, toklen);
		    out += toklen;
		}
		lastdot = 0;	       /* an identifier isn't a dot! */
	    } else if (*p == '/' && p[1] == '*') {
		/*
		 * Skip C-style comment.
		 */
		*out++ = *p++;
		*out++ = *p++;
		while (*p) {
		    if (*p == '*' && p[1] == '/') {
			*out++ = *p++;
			*out++ = *p++;
			break;
		    }
		    *out++ = *p++;
		}
	    } else if (*p == '/' && p[1] == '/') {
		/*
		 * Skip C++-style comment.
		 */
		*out++ = *p++;
		*out++ = *p++;
		while (*p && *p != '\n')
		    *out++ = *p++;
	    } else if (*p == '\'' || *p == '"') {
		/*
		 * Skip string literal.
		 */
		int c = *p;
		*out++ = *p++;
		while (*p && *p != c) {
		    if (*p == '\\')
			*out++ = *p++; /* avoid escaped quotes */
		    *out++ = *p++;
		}
		lastdot = 0;
	    } else {
		if (!isspace((unsigned char)*p)) {
		    /*
		     * Check the non-whitespace character before any
		     * identifier to see if it's a period.
		     */
		    lastdot = (*p == '.');
		}
		*out++ = *p++;
	    }
	}
	assert(out - moduserpac <= modmaxsize);
	*out = '\0';
    }

    /*
     * Now that we know what identifiers we want it to avoid
     * using, we can construct the JS translation of our Ick
     * script.
     */
    {
	char *mainfunc = "RewriteURL";
	jsscript = ick_to_js(&mainfunc, scr, (const char **)avoidids,
			     navoidids);
	assert(jsscript);
    }

    /*
     * And free our avoidids list.
     */
    while (navoidids > 0)
	free(avoidids[--navoidids]);
    free(avoidids);

    /*
     * The third component of our output .PAC, after the
     * translated Ick script and the slightly modified input .PAC,
     * is the output implementation of the actual FindProxyForURL
     * function. This is an absolutely standard piece of
     * boilerplate in all cases: it simply calls the rewriter
     * script, tests if it made a difference, and then either
     * returns a PROXY string pointing at us or hands off to the
     * user's renamed FindProxyForURL function. So we parametrise
     * this boilerplate by the port number, but that's all.
     */
    sprintf(fpfu,
	    "function FindProxyForURL(url, host)\n"
	    "{\n"
	    "    if (RewriteURL(url) != url)\n"
	    "        return \"PROXY localhost:%d\";\n"
	    "    return user_FindProxyForURL(url, host);\n"
	    "}\n", port);

    /*
     * Now we can simply concatenate our three components, and
     * return the result.
     */
    {
	int totallen = strlen(jsscript) + strlen(moduserpac) + strlen(fpfu);
	char *ret;

	ret = malloc(totallen + 10);
	sprintf(ret, "%s\n%s\n%s", jsscript, moduserpac, fpfu);

	free(moduserpac);
	free(jsscript);

	return ret;
    }
}

#ifdef TEST

#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *fp;
    char buf[65536];
    char *pac, *err;
    int len;
    assert(argc > 2);
    icklib *lib;
    ickscript *scr;

    fp = fopen(argv[1], "r");
    len = fread(buf, 1, 65536, fp);
    fclose(fp);
    buf[len] = '\0';

    fp = fopen(argv[2], "r");
    lib = ick_lib_new(0);
    scr = ick_compile_fp(&err, "rewrite", "SS", lib, fp);
    fclose(fp);
    if (!scr) {
	printf("%s:%s\n", argv[2], err);
	free(err);
	return 0;
    }

    pac = build_pac(buf, scr, 12345);

    ick_free(scr);
    ick_lib_free(lib);

    fputs(pac, stdout);

    free(pac);

    return 0;
}
#endif
