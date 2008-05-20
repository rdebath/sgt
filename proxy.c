/*
 * proxy.c: the main HTTP server logic in ick-proxy.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "icklang.h"
#include "proxy.h"
#include "misc.h"
#include "tree234.h"

icklib *lib = NULL;

struct listenctx {
    char *user;			       /* NULL means master socket */
    ickscript *scr;
    int port;
};

struct connctx {
    ickscript *scr;		       /* copied from listenctx on creation */
    char *data;
    int datalen, datasize;
};

/*
 * Function to build a valid proxyconfig.pac, given an input
 * proxyconfig.pac and a compiled Ick script.
 */
char *build_pac(char *userpac, ickscript *scr, int port)
{
    char *moduserpac;
    char *jsscript;
    char *mainfunc;
    char *fpfu;
    char *ret;
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
	moduserpac = snewn(modmaxsize + 1, char); /* allow for trailing NUL */
memset(moduserpac, 'X', modmaxsize);

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
			avoidids = sresize(avoidids, avoididsize, char *);
		    }
		    *out = '\0';   /* temporarily NUL-terminate for dupstr */
		    avoidids[navoidids] = dupstr(outtok);
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
     * More identifiers to avoid: these are the standard functions
     * defined as being available to proxy configuration files in
     * http://wp.netscape.com/eng/mozilla/2.0/relnotes/demo/proxy-live.html
     */
    {
	static const char *const stdfns[] = {
            "isPlainHostName",
            "dnsDomainIs",
            "localHostOrDomainIs",
            "isResolvable",
            "isInNet",
            "dnsResolve",
            "myIpAddress",
            "dnsDomainLevels",
            "shExpMatch",
            "weekdayRange",
            "dateRange",
            "timeRange",
            "ProxyConfig"
	};
	int i;

	if (navoidids + lenof(stdfns) > avoididsize) {
	    avoididsize = navoidids + lenof(stdfns);
	    avoidids = sresize(avoidids, avoididsize, char *);
	}
	for (i = 0; i < lenof(stdfns); i++)
	    avoidids[navoidids++] = dupstr(stdfns[i]);
    }

    /*
     * Now that we know what identifiers we want it to avoid
     * using, we can construct the JS translation of our Ick
     * script.
     */
    mainfunc = NULL;
    jsscript = ick_to_js(&mainfunc, scr, (const char **)avoidids, navoidids);
    assert(jsscript);

    /*
     * And free our avoidids list.
     */
    while (navoidids > 0)
	sfree(avoidids[--navoidids]);
    sfree(avoidids);

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
    fpfu = dupfmt("function FindProxyForURL(url, host)\n"
		  "{\n"
		  "    if (%s(url) != url)\n"
		  "        return \"PROXY localhost:%d\";\n"
		  "    return user_FindProxyForURL(url, host);\n"
		  "}\n", mainfunc, port);

    /*
     * Now we can simply concatenate our three components, and
     * return the result.
     */
    ret = dupfmt("/*\n"
		 " * PAC proxy configuration generated automatically by ick-proxy.\n"
		 " * Changes made directly to this file will be lost when it is\n"
		 " * next regenerated.\n"
		 " */\n\n%s\n%s\n%s", jsscript, moduserpac, fpfu);

    sfree(fpfu);
    sfree(mainfunc);
    sfree(jsscript);
    sfree(moduserpac);

    return ret;
}

/*
 * We have to store listenctxes in a tree here, because we reuse
 * them when asked to reconfigure for the same user again.
 */
tree234 *listenctxes;		       /* sorted by user */

static int listen_cmp(void *av, void *bv)
{
    struct listenctx *a = (struct listenctx *) av;
    struct listenctx *b = (struct listenctx *) bv;
    int cmp;
    if (a->user == NULL || b->user == NULL)
	cmp = (a->user != NULL) - (b->user != NULL);
    else
	cmp = strcmp(a->user, b->user);
    return cmp;
}
static int listen_find(void *av, void *bv)
{
    char *a = (char *) av;
    struct listenctx *b = (struct listenctx *) bv;
    int cmp;
    if (a == NULL || b->user == NULL)
	cmp = (a != NULL) - (b->user != NULL);
    else
	cmp = strcmp(a, b->user);
    return cmp;
}

void ick_proxy_setup(void)
{
    lib = ick_lib_new(0);
    listenctxes = newtree234(listen_cmp);
}

void configure_master(int port)
{
    struct listenctx *ctx = snew(struct listenctx);

    ctx->user = NULL;
    ctx->scr = NULL;
    if (add234(listenctxes, ctx) != ctx) {
	/*
	 * Somehow, we've already got one.
	 */
	sfree(ctx);
    } else {
	char *err;

	if (create_new_listener(&err, port, ctx) < 0) {
	    char buf[256];
	    sprintf(buf, "Unable to bind to port %d: %s", port, err);
	    platform_fatal_error(buf);
	}
    }
}

char *configure_for_user(char *username)
{
    char *scriptsrc;
    ickscript *script;
    char *scripterrctx, *scripterr, *pacerr;
    struct listenctx *ctx;
    char *inpac, *outpac;

    /*
     * Look up the username and try to read its configuration.
     */
    scripterrctx = "loading user rewrite script";
    scriptsrc = get_script_for_user(&scripterr, username);
    if (scriptsrc) {
	scripterrctx = "compiling user rewrite script";
	script = ick_compile_str(&scripterr, "rewrite", "SS", lib, scriptsrc);
	sfree(scriptsrc);
    } else {
	script = NULL;
    }

    /*
     * Now either script is non-NULL, or script is NULL and
     * scripterr is an error message. In the latter case, we'll be
     * returning a .pac which never redirects to us, so we don't
     * need a socket. Otherwise, find or make the appropriate
     * listener.
     */
    if (script) {
	/*
	 * If we already have a listening socket for this user,
	 * reuse it. Otherwise, allocate a new one.
	 */
	ctx = find234(listenctxes, username, listen_find);

	if (ctx) {
	    ick_free(ctx->scr);
	    ctx->scr = script;
	} else {
	    ctx = snew(struct listenctx);
	    ctx->user = username ? dupstr(username) : NULL;
	    ctx->port = create_new_listener(&scripterr, -1, ctx);
	    ctx->scr = script;
	    if (ctx->port < 0) {
		ick_free(script);
		sfree(ctx);
		ctx = NULL;
		script = NULL;
		scripterrctx = "allocating a port to listen on";
	    } else {
		add234(listenctxes, ctx);
	    }
	}
    } else
	ctx = NULL;

    /*
     * Load the user's input .pac and construct their output one.
     */
    inpac = get_inpac_for_user(&pacerr, username);
    if (!inpac) {
	inpac = dupfmt("/*\n"
		       " * Error loading user .pac file:\n"
		       " * %s\n"
		       " * Substituting trivial .pac\n"
		       " */\n"
		       "function FindProxyForURL(url, host)\n"
		       "{\n"
		       "    return \"DIRECT\";\n"
		       "}\n", pacerr);
	sfree(pacerr);
    }

    if (ctx) {
	outpac = build_pac(inpac, ctx->scr, ctx->port);
    } else {
	outpac = dupfmt("/*\n"
			" * Error %s:\n"
			" * %s\n"
			" * No URL rewriting enabled in this .pac\n"
			" */\n"
			"%s", scripterrctx, scripterr, inpac);
    }

    return outpac;
}

/*
 * Called by the platform code when a new connection arrives on a
 * listening socket. Returns a connctx for the new connection,
 * given a listenctx for the listener.
 */
struct connctx *new_connection(struct listenctx *lctx)
{
    struct connctx *cctx = snew(struct connctx);
    cctx->scr = lctx->scr;
    cctx->data = NULL;
    cctx->datalen = cctx->datasize = 0;
    return cctx;
}

void free_connection(struct connctx *cctx)
{
    /*
     * We don't free cctx->script because it was an additional
     * reference to the one in the corresponding listenctx. (So if
     * we do ever start freeing listenctxes, we're going to have
     * to make sure we only do so when all cctxes have
     * disappeared, or start ref-counting, or something else
     * horrid.)
     */
    sfree(cctx->data);
    sfree(cctx);
}

static char *http_error(char *code, char *errmsg, char *errtext, ...)
{
    return dupfmt("HTTP/1.1 %s\r\n"
		  "Date: %D\r\n"
		  "Server: ick-proxy\r\n"
		  "Connection: close\r\n"
		  "Content-Type: text/html; charset=US-ASCII\r\n"
		  "\r\n"
		  "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		  "<HTML><HEAD>\r\n"
		  "<TITLE>%s</TITLE>\r\n"
		  "</HEAD><BODY>\r\n"
		  "<H1>%s</H1>\r\n"
		  "<P>%s</P>\r\n"
		  "</BODY></HTML>\r\n", errmsg, errmsg, errmsg, errtext);
}

static char *http_rewrite(char *newurl)
{
    return dupfmt("HTTP/1.1 302 ick-proxy redirection\r\n"
		  "Date: %D\r\n"
		  "Server: ick-proxy\r\n"
		  "Location: %s\r\n"
		  "Connection: close\r\n"
		  "Content-Type: text/html; charset=US-ASCII\r\n"
		  "\r\n"
		  "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		  "<HTML><HEAD>\r\n"
		  "<TITLE>302 ick-proxy redirection</TITLE>\r\n"
		  "</HEAD><BODY>\r\n"
		  "<H1>302 ick-proxy redirection</H1>\r\n"
		  "<P>The document has moved <A HREF=\"%s\">here</A>.</P>\r\n"
		  "</BODY></HTML>\r\n", newurl, newurl);
}

static char *http_success(char *mimetype, int stuff_cr, char *document)
{
    return dupfmt("HTTP/1.1 200 OK\r\n"
		  "Date: %D\r\n"
		  "Server: ick-proxy\r\n"
		  "Connection: close\r\n"
		  "Content-Type: %s\r\n"
		  "\r\n"
		  "%S", mimetype, stuff_cr, document);
}

/*
 * Internal URL-rewriting function shared between the connection
 * management code and the URL-rewriting test mode.
 */
enum {
    REWRITE_ERR_UNCHANGED = ICK_RTE_USER,
    REWRITE_ERR_CASCADE,
    REWRITE_ERR_CASCTEST_BASE
};
static int do_rewrite(char **out, ickscript *scr, const char *url)
{
    int rte;
    char *rewritten = NULL;
    char *rewritten2 = NULL;

    rte = ick_exec_limited(&rewritten, 65536, 4096, 65536, scr, url);

    if (rte != ICK_RTE_NO_ERROR)
	return rte;

    if (!strcmp(rewritten, url)) {
	sfree(rewritten);
	return REWRITE_ERR_UNCHANGED;
    }

    *out = rewritten;

    rte = ick_exec_limited(&rewritten2, 65536, 4096, 65536, scr, url);

    if (rte != ICK_RTE_NO_ERROR)
	return rte + REWRITE_ERR_CASCTEST_BASE;

    if (strcmp(rewritten, rewritten2)) {
	sfree(rewritten2);
	return REWRITE_ERR_CASCADE;
    }

    sfree(rewritten2);
    return ICK_RTE_NO_ERROR;
}

/*
 * Called by the platform code when data comes in on a connection.
 * 
 * If this function returns NULL, the platform code continues
 * reading from the socket. Otherwise, it returns some dynamically
 * allocated data which the platform code will then write to the
 * socket before closing it.
 */
char *got_data(struct connctx *ctx, char *data, int length)
{
    char *line, *p, *q, *z1, *z2, c1, c2;

    if (ctx->datasize < ctx->datalen + length) {
	ctx->datasize = (ctx->datalen + length) * 3 / 2 + 4096;
	ctx->data = sresize(ctx->data, ctx->datasize, char);
    }
    memcpy(ctx->data + ctx->datalen, data, length);
    ctx->datalen += length;

    /*
     * See if we have enough of an HTTP request to work out our
     * response.
     */
    line = ctx->data;
    /*
     * RFC 2616 section 4.1: `In the interest of robustness, [...]
     * if the server is reading the protocol stream at the
     * beginning of a message and receives a CRLF first, it should
     * ignore the CRLF.'
     */
    while (line - ctx->data < ctx->datalen &&
	   (*line == '\r' || *line == '\n'))
	line++;

    q = line;
    while (q - ctx->data < ctx->datalen && *q != '\r' && *q != '\n')
	q++;
    if (q - ctx->data >= ctx->datalen)
	return NULL;	       /* not got request line yet */

    /*
     * We've got the first line of the request, which is enough
     * for us to work out what to say in response and start
     * sending it. The platform side will keep reading data, but
     * it'll ignore it.
     *
     * So, zero-terminate our line and parse it.
     */
    *q = '\0';
    z1 = z2 = q;
    c1 = c2 = *q;
    p = line;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) {
	z1 = p++;
	c1 = *z1;
	*z1 = '\0';
    }
    while (*p && isspace((unsigned char)*p)) p++;
    q = p;
    while (*q && !isspace((unsigned char)*q)) q++;
    z2 = q;
    c2 = *z2;
    *z2 = '\0';

    /*
     * Now `line' points at the method name; p points at the URL,
     * if any.
     */
    
    /*
     * There should _be_ a URL, on any request type at all.
     */
    if (!*p) {
	char *ret, *text;
	*z2 = c2;
	*z1 = c1;
	text = dupfmt("<code>ick-proxy</code> received the HTTP request"
		      " \"<code>%s</code>\", which contains no URL.",
		      line);
	ret = http_error("400", "Bad request", text);
	sfree(text);
	return ret;
    }

    /*
     * Diverge based on what this connection was about.
     */
    if (ctx->scr) {
	int rte;
	char *rewritten = NULL;

	/*
	 * This connection has an associated script (meaning that
	 * it came in to a listener created by
	 * configure_for_user). Try to rewrite the URL.
	 */

	rte = do_rewrite(&rewritten, ctx->scr, p);

	if (rte != ICK_RTE_NO_ERROR) {
	    char *ret, *text;
	    if (rte < ICK_RTE_USER) {
		text = dupfmt("While trying to rewrite the URL \"%s\", "
			      "the <code>ick-proxy</code> rewrite script "
			      "suffered a run-time error: \"%s\".",
			      p, ick_runtime_errors[rte]);
	    } else if (rte == REWRITE_ERR_UNCHANGED) {
		text = dupfmt("The URL \"%s\" was passed to "
			      "<code>ick-proxy</code> for rewriting, but the "
			      "rewrite script did not change it. Your browser-"
			      "side configuration is probably out of sync "
			      "with <code>ick-proxy</code>'s configuration.",
			      p);
	    } else if (rte == REWRITE_ERR_CASCADE) {
		text = dupfmt("The URL \"%s\" was passed to "
			      "<code>ick-proxy</code> for rewriting; the "
			      "rewrite script rewrote it to \"%s\", but that "
			      "would have changed again on a further rewrite, "
			      "causing a cascade of HTTP redirects.",
			      p, rewritten);
		sfree(rewritten);
	    } else if (assert(rte >= REWRITE_ERR_CASCTEST_BASE), 1) {
		int realerr = rte - REWRITE_ERR_CASCTEST_BASE;
		text = dupfmt("The URL \"%s\" was passed to "
			      "<code>ick-proxy</code> for rewriting; the "
			      "rewrite script rewrote it to \"%s\", but "
			      "rewriting that again would have caused a run-"
			      "time error: \"%s\".",
			      p, rewritten, ick_runtime_errors[realerr]);
		sfree(rewritten);
	    }
	    ret = http_error("500", "Internal Server Error", text);
	    sfree(text);
	    return ret;
	}

	/*
	 * This is conceptually an assert, except that actually
	 * crashing ick-proxy in mid-run tends to be a bad idea.
	 * So I'll treat it gently if it does come up.
	 */
	if (!rewritten) {
	    char *ret, *text;
	    text = dupfmt("While trying to rewrite the URL \"%s\", "
			  "the <code>ick-proxy</code> rewrite script "
			  "returned no run-time error but also no "
			  "output. This is a bug in ick-proxy!", p);
	    ret = http_error("500", "Internal Server Error", text);
	    sfree(text);
	    return ret;
	}

	{
	    char *ret = http_rewrite(rewritten);
	    sfree(rewritten);
	    return ret;
	}
    } else {
	/*
	 * This connection came in to the master listener created
	 * by configure_master. So we are in multi-user mode, and
	 * we should serve users' PACs.
	 */
	if (!strncmp(p, "/pac/", 5)) {
	    char *pac = configure_for_user(p + 5);
	    char *ret;

	    /*
	     * FIXME: possibly it would be good to extract the
	     * Host header from the incoming request, and use it
	     * in place of localhost when constructing the
	     * outgoing PAC? That way, ick-proxy running on one
	     * machine could be used by a client on another.
	     */

	    /*
	     * configure_for_user never returns NULL: there will
	     * always be a valid PAC, even if it's trivial in
	     * function and full of comments containing error
	     * messages.
	     */
	    ret = http_success("application/x-ns-proxy-autoconfig", 1, pac);
	    sfree(pac);
	    return ret;
	} else {
	    /*
	     * FIXME: perhaps we should also serve a front page
	     * with useful data on it? And perhaps some
	     * configuration stuff, under some circumstances?
	     */
	    char *ret;
	    ret = http_error("404", "Not Found",
			     "This is the <code>ick-proxy</code> master "
			     "port. The only documents served from this "
			     "server are proxy configuration files in the "
			     "\"<code>pac</code>\" subdirectory.");
	    return ret;
	}
    }
}

char *rewrite_url(char **err, const char *user, const char *url)
{
    char *scriptsrc;
    ickscript *script;
    char *myerr;
    char *rewritten;
    int rte;

    scriptsrc = get_script_for_user(&myerr, user);
    if (!scriptsrc) {
	*err = dupfmt("Error loading script for user \"%s\": %s", user, myerr);
	sfree(myerr);
	return NULL;
    }

    script = ick_compile_str(&myerr, "rewrite", "SS", lib, scriptsrc);
    if (!script) {
	*err = dupfmt("Error compiling script for user \"%s\": %s", user,
		      myerr);
	sfree(myerr);
	return NULL;
    }

    rte = do_rewrite(&rewritten, script, url);

    if (rte != ICK_RTE_NO_ERROR) {
	if (rte < ICK_RTE_USER) {
	    *err = dupfmt("Run-time error: %s", ick_runtime_errors[rte]);
	} else if (rte == REWRITE_ERR_UNCHANGED) {
	    *err = dupfmt("URL was unchanged");
	} else if (rte == REWRITE_ERR_CASCADE) {
	    *err = dupfmt("Rewritten URL \"%s\" causes rewrite cascade",
			  rewritten);
	    sfree(rewritten);
	} else if (rte >= REWRITE_ERR_CASCTEST_BASE) {
	    int realerr = rte - REWRITE_ERR_CASCTEST_BASE;
	    *err = dupfmt("Rewritten URL \"%s\" causes run-time error: %s",
			  rewritten, ick_runtime_errors[realerr]);
	    sfree(rewritten);
	}
	rewritten = NULL;
    }

    sfree(scriptsrc);
    ick_free(script);

    return rewritten;
}
