/*
 * icklang.c: implementation of icklang.h.
 */

/*
 * Potential future work:
 * 
 *  - track outermost operator priority when generating
 *    expressions in Javascript, so we can avoid writing out any
 *    parentheses that aren't actually required. (Not _entirely_
 *    cosmetic: ngs-js at least has a criminally small tolerance
 *    for excessive parens.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

#include "icklang.h"
#include "misc.h"

#define lenof(x) ( sizeof((x)) / sizeof(*(x)) )

/* ----------------------------------------------------------------------
 * Internal structure definitions.
 */

struct ick_srcpos {
    int line, col;
};

struct ick_token {
    int type;
    struct ick_srcpos pos;
    int ivalue;
    char *tvalue;
    int tvalue_allocsize;
};

struct ick_expr;
struct ick_statement;
struct ick_vardecl;
struct ick_vardecls;
struct ick_block;
struct ick_fndecl;

struct ick_expr {
    /*
     * Filled in at parse time.
     */
    int op;
    struct ick_expr *e1, *e2, *e3;
    char *sval;
    int ival;
    struct ick_srcpos startpos, oppos, op2pos;
    /*
     * Filled in at semantic analysis time.
     */
    int type;			       /* for all except TOK_RPAREN nodes */
    struct ick_vardecl *var;	       /* for TOK_IDENTIFIER and lvalues */
    struct ick_fndecl *fn;	       /* for TOK_LPAREN */
    int fnbelow;		       /* any function calls in/below here? */
    /*
     * Filled in at code generation time.
     */
    int e3addr, endaddr;
};

struct ick_statement {
    int type;
    struct ick_expr *e1, *e2, *e3;
    struct ick_statement *s1, *s2;
    struct ick_block *blk;
    struct ick_srcpos startpos;
    /*
     * Non-child link within the tree, pointing break and continue
     * statements at their containing loop.
     */
    struct ick_statement *auxs1;
    /*
     * Filled in at code generation time.
     */
    int breakpt, contpt, looptop;
};

struct ick_vardecl {
    char *identifier;
    int type;
    struct ick_expr *initialiser;
    /*
     * Filled in at code generation time.
     */
    int address;
    /*
     * Used during Javascript output.
     */
    const char *jsname;
};

struct ick_vardecls {
    struct ick_vardecl *decls;
    int ndecls, declsize;
};

struct ick_block {
    struct ick_vardecls vars;
    struct ick_statement **stmts;
    int nstmts, stmtsize;
};

struct ick_fndecl {
    char *identifier;
    int rettype;
    struct ick_vardecls params;
    struct ick_block *defn;
    icklib_fn_t native;		       /* for native library functions */
    icklib_js_fn_t js;		       /* for native library functions */
    struct ick_srcpos startpos;
    /*
     * Filled in at code generation time.
     */
    int address, retpoint, istack, sstack;
    /*
     * Used during Javascript output.
     */
    const char *jsname;
};

struct ick_parse_tree {
    struct ick_vardecls toplevel_vars;
    struct ick_fndecl *fns;
    int nfns, fnsize;
};

struct ick_insn {
    int opcode, p1, p2, p3;
};

struct icklib {
    struct ick_fndecl *fns;
    int nfns, fnsize;
};

struct ick_parse_ctx {
    /*
     * Input by user.
     */
    int (*read)(void *ctx);
    void *readctx;
    icklib *lib;
    char **err;
    /*
     * Used by lexer.
     */
    int lex_last;
    struct ick_srcpos lex_pos;
    struct ick_token tok;
    /*
     * Filled in during parsing.
     */
    struct ick_parse_tree *tree;
    /*
     * Used during semantic analysis.
     */
    struct ick_vardecl **varscope;
    int nvarscope, varscopesize;
    struct ick_statement *currloop;
    /*
     * Used during code generation.
     */
    int pass;
    struct ick_insn *insns;
    int ninsns, insnsize;
    int istkup, istkdn, istkmax, iframe;
    int sstkup, sstkdn, sstkmax, sframe;
    struct ick_fndecl *currfn;
    const char **stringlits;
    int nstringlits, stringlitsize;
};

/*
 * User-visible (though user-opaque) structure containing
 * everything we needed to _keep_ after compiling an Ick script.
 */
struct ickscript {
    /*
     * We keep the parse tree, for feeding to the Javascript
     * translator.
     */
    struct ick_parse_tree *tree;

    /*
     * Data for the execution engine.
     */
    int ivars, svars;           /* initial stack required for global vars */
    struct ick_insn *insns;	       /* actual executable VM code */
    int ninsns;			       /* for bounds checking */
    const char **stringlits;	       /* string literal pool */
    int nstringlits;		       /* for bounds checking */

    /*
     * We need the standard library, of course.
     */
    icklib *lib;
};

/* ----------------------------------------------------------------------
 * Ick lexer.
 */

/*
 * Defines the token types.
 */
#define TOKENTYPES(X) \
    X(TOK_KW_STRING, "string", 1), \
    X(TOK_KW_INT, "int", 1), \
    X(TOK_KW_BOOL, "bool", 1), \
    X(TOK_KW_VOID, "void", 1), \
    X(TOK_KW_RETURN, "return", 1), \
    X(TOK_KW_BREAK, "break", 1), \
    X(TOK_KW_CONTINUE, "continue", 1), \
    X(TOK_KW_IF, "if", 1), \
    X(TOK_KW_ELSE, "else", 1), \
    X(TOK_KW_FOR, "for", 1), \
    X(TOK_KW_WHILE, "while", 1), \
    X(TOK_KW_DO, "do", 1), \
    X(TOK_LBRACE, "{", 1), \
    X(TOK_RBRACE, "}", 1), \
    X(TOK_LPAREN, "(", 1), \
    X(TOK_RPAREN, ")", 1), \
    X(TOK_COMMA, ",", 1), \
    X(TOK_SEMI, ";", 1), \
    X(TOK_LT, "<", 1), \
    X(TOK_LE, "<=", 1), \
    X(TOK_GT, ">", 1), \
    X(TOK_GE, ">=", 1), \
    X(TOK_EQ, "==", 1), \
    X(TOK_NE, "!=", 1), \
    X(TOK_QUERY, "?", 1), \
    X(TOK_COLON, ":", 1), \
    X(TOK_ASSIGN, "=", 1), \
    X(TOK_PLUS, "+", 1), \
    X(TOK_MINUS, "-", 1), \
    X(TOK_INCREMENT, "++", 1), \
    X(TOK_DECREMENT, "--", 1), \
    X(TOK_MUL, "*", 1), \
    X(TOK_DIV, "/", 1), \
    X(TOK_LOGAND, "&&", 1), \
    X(TOK_LOGOR, "||", 1), \
    X(TOK_LOGNOT, "!", 1), \
    X(TOK_PLUSASSIGN, "+=", 1), \
    X(TOK_MINUSASSIGN, "-=", 1), \
    X(TOK_MULASSIGN, "*=", 1), \
    X(TOK_DIVASSIGN, "/=", 1), \
    X(TOK_LOGANDASSIGN, "&&=", 1), \
    X(TOK_LOGORASSIGN, "||=", 1), \
    X(TOK_IDENTIFIER, "identifier", 0), \
    X(TOK_STRINGLIT, "string literal", 0), \
    X(TOK_INTLIT, "integer literal", 0), \
    X(TOK_BOOLLIT, "boolean literal", 0), \
    X(TOK_ERROR, "unknown token", 0), \
    X(TOK_EOF, "end of file", 0)

#define TOKENENUM(x,y,z) x
#define TOKENNAME(x,y,z) #x
enum { TOKENTYPES(TOKENENUM) };

#define TOKENPRETTY(x,y,z) ((z) ? "'" y "'" : y)
static const char *const tokenpretty[] = { TOKENTYPES(TOKENPRETTY) };
#define TOKENBARE(x,y,z) y
static const char *const tokenbare[] = { TOKENTYPES(TOKENBARE) };

#define CHAR_TO_TYPE(c) ( (c) == 'S' ? TOK_KW_STRING : \
			  (c) == 'I' ? TOK_KW_INT : \
			  (c) == 'B' ? TOK_KW_BOOL : \
			  (c) == 'V' ? TOK_KW_VOID : -1 )

#define TYPETOSTRING(type) ( ((type) == TOK_KW_STRING ? "string" : \
			      (type) == TOK_KW_INT ? "integer" : \
			      (type) == TOK_KW_BOOL ? "boolean" : "void") )

static void ick_error(struct ick_parse_ctx *ctx, struct ick_srcpos srcpos,
		      char *msg, ...)
{
    if (*ctx->err) {
	/*
	 * If we already have an error message, don't record a
	 * second one.
	 *
	 * The only case I know of where this can occur is if the
	 * lexer returns TOK_ERROR after reporting an invalid
	 * escape in a string literal.
	 */
    } else {
	int pass, len, pos;
	char *p, *outmsg;
	const char *out;
	int outlen;
	struct ick_expr *a = NULL;
	const char *u = NULL;
	const char *deferred = NULL;
	char linebuf[160];
	va_list ap;

	/*
	 * I define my own printf-like mechanism here for
	 * reporting compile errors, with % escapes covering such
	 * useful things as token classes, type names and function
	 * parameter lists. The escapes are:
	 * 
	 *  - %s expects an ordinary string and prints it.
	 *  - %p expects an int containing a token type, and
	 *    produces a prettyprinted version of the token:
	 *    literal tokens are surrounded by single quotes (e.g.
	 *    '+=') whereas descriptive token names are not (e.g.
	 *    identifier).
	 *  - %b expects an int containing a token type, and
	 *    produces a bare version of the token, with no single
	 *    quotes ever. Probably best to use when you know
	 *    you're expecting a literal token.
	 *  - %T expects an int containing a token type which is
	 *    also a _type_ type, and names the type. (Differs
	 *    from %b in that `int' becomes `integer' and `bool'
	 *    `boolean', and non-type-name tokens aren't
	 *    supported.)
	 *  - %a expects an expression node which is the start of
	 *    a linked list of function call arguments, and
	 *    generates text describing the argument types
	 *    separated by commas.
	 *  - %u expects a string which is a user-provided
	 *    function prototype encoding, and generates text
	 *    describing the argument types separated by commas.
	 */

	len = pos = 0;
	outmsg = NULL;

	for (pass = 0; pass < 2; pass++) {
	    va_start(ap, msg);

	    if (srcpos.line >= 0) {
		sprintf(linebuf, "%d:%d: ", srcpos.line, srcpos.col);
	    } else {
		linebuf[0] = '\0';
	    }
	    deferred = linebuf;

	    for (p = msg; *p ;) {
		if (deferred) {
		    out = deferred;
		    outlen = strlen(out);
		    deferred = NULL;
		} else if (a) {
		    out = tokenbare[a->e1->type];
		    outlen = strlen(out);
		    a = a->e2;
		    if (a)
			deferred = ", ";
		} else if (u && *u) {
		    int t = CHAR_TO_TYPE(*u);
		    out = tokenbare[t];
		    outlen = strlen(out);
		    u++;
		    if (*u)
			deferred = ", ";
		    else
			u = NULL;
		} else if (*p != '%') {
		    out = p;
		    outlen = strcspn(p, "%");
		    p += outlen;
		} else {
		    if (p[1] == 's') {
			out = va_arg(ap, const char *);
			outlen = strlen(out);
		    } else if (p[1] == 'p' || p[1] == 'b') {
			int tok = va_arg(ap, int);
			out = (p[1] == 'p' ? tokenpretty[tok] :
			       tokenbare[tok]);
			outlen = strlen(out);
		    } else if (p[1] == 'T') {
			int tok = va_arg(ap, int);
			out = TYPETOSTRING(tok);
			outlen = strlen(out);
		    } else if (p[1] == 'a') {
			a = va_arg(ap, struct ick_expr *);
			if (!a) {
			    out = "void";
			    outlen = strlen(out);
			} else {
			    out = "";
			    outlen = 0;
			}
		    } else if (p[1] == 'u') {
			u = va_arg(ap, const char *);
			u++;	       /* skip return type */
			if (!*u) {
			    out = "void";
			    outlen = strlen(out);
			} else {
			    out = "";
			    outlen = 0;
			}
		    } else {
			out = "";
			outlen = 0;
		    }
		    p += 2;
		}

		if (pass == 0) {
		    len += outlen;
		} else {
		    memcpy(outmsg + pos, out, outlen);
		    pos += outlen;
		}
	    }

	    va_end(ap);

	    if (pass == 0) {
		outmsg = snewn(len+1, char);
	    } else {
		assert(pos == len);
		outmsg[pos] = '\0';
	    }
	}

	*ctx->err = outmsg;
    }
}

static void ick_lex_read(struct ick_parse_ctx *ctx)
{
    switch (ctx->lex_last) {
      case '\n':
	ctx->lex_pos.line++;
	ctx->lex_pos.col = 1;
	break;
      case '\t':
	ctx->lex_pos.col = ((ctx->lex_pos.col + 7) &~ 7) + 1;
	break;
      default:
	ctx->lex_pos.col++;
	break;
    }
    ctx->lex_last = ctx->read(ctx->readctx);
}

static void ick_lex_advance(struct ick_parse_ctx *ctx)
{
    redo:

    ctx->tok.pos = ctx->lex_pos;

    if (ctx->lex_last < 0) {
	ctx->tok.type = TOK_EOF;
	return;
    }

    switch (ctx->lex_last) {
      case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
	ick_lex_read(ctx);
	goto redo;		       /* skip whitespace */

      case '+':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_PLUS;
	if (ctx->lex_last == '+') {
	    ctx->tok.type = TOK_INCREMENT;
	    ick_lex_read(ctx);
	} else if (ctx->lex_last == '=') {
	    ctx->tok.type = TOK_PLUSASSIGN;
	    ick_lex_read(ctx);
	}
	return;
      case '-':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_MINUS;
	if (ctx->lex_last == '-') {
	    ctx->tok.type = TOK_DECREMENT;
	    ick_lex_read(ctx);
	} else if (ctx->lex_last == '=') {
	    ctx->tok.type = TOK_MINUSASSIGN;
	    ick_lex_read(ctx);
	}
	return;
      case '*':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_MUL;
	if (ctx->lex_last == '=') {
	    ctx->tok.type = TOK_MULASSIGN;
	    ick_lex_read(ctx);
	}
	return;
      case '{':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_LBRACE;
	return;
      case '}':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_RBRACE;
	return;
      case '(':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_LPAREN;
	return;
      case ')':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_RPAREN;
	return;
      case ',':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_COMMA;
	return;
      case ';':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_SEMI;
	return;
      case '?':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_QUERY;
	return;
      case ':':
	ick_lex_read(ctx);
	ctx->tok.type = TOK_COLON;
	return;
      case '<':
	ick_lex_read(ctx);
	if (ctx->lex_last == '=') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_LE;
	} else
	    ctx->tok.type = TOK_LT;
	return;
      case '>':
	ick_lex_read(ctx);
	if (ctx->lex_last == '=') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_GE;
	} else
	    ctx->tok.type = TOK_GT;
	return;
      case '=':
	ick_lex_read(ctx);
	if (ctx->lex_last == '=') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_EQ;
	} else
	    ctx->tok.type = TOK_ASSIGN;
	return;
      case '!':
	ick_lex_read(ctx);
	if (ctx->lex_last == '=') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_NE;
	} else
	    ctx->tok.type = TOK_LOGNOT;
	return;
      case '&':
	ick_lex_read(ctx);
	if (ctx->lex_last == '&') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_LOGAND;
	    if (ctx->lex_last == '=') {
		ctx->tok.type = TOK_LOGANDASSIGN;
		ick_lex_read(ctx);
	    }
	} else
	    ctx->tok.type = TOK_ERROR;
	return;
      case '|':
	ick_lex_read(ctx);
	if (ctx->lex_last == '|') {
	    ick_lex_read(ctx);
	    ctx->tok.type = TOK_LOGOR;
	    if (ctx->lex_last == '=') {
		ctx->tok.type = TOK_LOGORASSIGN;
		ick_lex_read(ctx);
	    }
	} else
	    ctx->tok.type = TOK_ERROR;
	return;
      case '/':
	ick_lex_read(ctx);
	if (ctx->lex_last == '/') {
	    /*
	     * Comment to end of line.
	     */
	    while (ctx->lex_last >= 0 && ctx->lex_last != '\n')
		ick_lex_read(ctx);
	    goto redo;		       /* that was effectively whitespace */
	} else if (ctx->lex_last == '*') {
	    /*
	     * Comment until * then /.
	     */
	    ick_lex_read(ctx);
	    while (1) {
		if (ctx->lex_last == '*') {
		    ick_lex_read(ctx);
		    if (ctx->lex_last == '/')
			break;
		} else if (ctx->lex_last < 0) {
		    break;
		}
		ick_lex_read(ctx);
	    }
	    ick_lex_read(ctx);   /* eat the final / */
	    goto redo;
	} else if (ctx->lex_last == '=') {
	    ctx->tok.type = TOK_DIVASSIGN;
	    ick_lex_read(ctx);
	} else {
	    ctx->tok.type = TOK_DIV;
	}
	return;
      case '"':
	{
	    int len = 0;

	    ick_lex_read(ctx);

	    while (1) {
		if (len >= ctx->tok.tvalue_allocsize) {
		    ctx->tok.tvalue_allocsize =
			(ctx->tok.tvalue_allocsize * 3 / 2) + 64;
		    ctx->tok.tvalue = sresize(ctx->tok.tvalue,
					      ctx->tok.tvalue_allocsize,
					      char);
		}

		if (ctx->lex_last < 0) {
		    ctx->tok.type = TOK_ERROR;
		    return;
		} else if (ctx->lex_last == '"') {
		    ctx->tok.tvalue[len] = '\0';
		    ctx->tok.type = TOK_STRINGLIT;
		    ick_lex_read(ctx);  /* eat quote */
		    return;
		} else if (ctx->lex_last == '\\') {
		    ick_lex_read(ctx);
		    if (ctx->lex_last < 0) {
			ctx->tok.type = TOK_ERROR;
			return;
		    }
		    switch (ctx->lex_last) {
		      case '\\': case '"':
			ctx->tok.tvalue[len++] = ctx->lex_last;
			ick_lex_read(ctx);
			break;
		      case 'a':
			ctx->tok.tvalue[len++] = '\a';
			ick_lex_read(ctx);
			break;
		      case 'b':
			ctx->tok.tvalue[len++] = '\b';
			ick_lex_read(ctx);
			break;
		      case 'f':
			ctx->tok.tvalue[len++] = '\f';
			ick_lex_read(ctx);
			break;
		      case 'n':
			ctx->tok.tvalue[len++] = '\n';
			ick_lex_read(ctx);
			break;
		      case 'r':
			ctx->tok.tvalue[len++] = '\r';
			ick_lex_read(ctx);
			break;
		      case 't':
			ctx->tok.tvalue[len++] = '\t';
			ick_lex_read(ctx);
			break;
		      case 'v':
			ctx->tok.tvalue[len++] = '\v';
			ick_lex_read(ctx);
			break;
		      case '\r':
		      case '\n':
			/* Ignore escaped newline. */
			{
			    int c = ctx->lex_last;
			    ick_lex_read(ctx);
			    if (ctx->lex_last == (('\r' ^ '\n') ^ c))
				ick_lex_read(ctx);
			}
			break;
		      case 'x': case 'X':
			/*
			 * Hex escape.
			 */
			{
#define fromxdigit(d) ((d)>='0'&&(d)<='9' ? (d)-'0' : \
		       (d)>='a'&&(d)<='f' ? (d)-('a'-10) : \
		       (d)>='A'&&(d)<='F' ? (d)-('A'-10) : 0)
			    int val = 0;
			    ick_lex_read(ctx);
			    if (isxdigit((unsigned char)ctx->lex_last)) {
				val = fromxdigit(ctx->lex_last);
				ick_lex_read(ctx);
				if (isxdigit((unsigned char)ctx->lex_last)) {
				    val = val * 16 + fromxdigit(ctx->lex_last);
				    ick_lex_read(ctx);
				}
			    }
			    ctx->tok.tvalue[len++] = val;
			}
			break;
		      default:
			if (ctx->lex_last >= '0' && ctx->lex_last <= '7') {
			    /*
			     * Octal escape.
			     */
			    int val = ctx->lex_last - '0';
			    ick_lex_read(ctx);
			    if (ctx->lex_last >= '0' && ctx->lex_last <= '7') {
				val = val * 8 + ctx->lex_last - '0';
				ick_lex_read(ctx);
			    }
			    if (ctx->lex_last >= '0' && ctx->lex_last <= '7') {
				val = val * 8 + ctx->lex_last - '0';
				ick_lex_read(ctx);
			    }
			    ctx->tok.tvalue[len++] = val;
			} else {
			    ick_error(ctx, ctx->lex_pos, "unrecognised escape"
				      " sequence in string literal");
			    ctx->tok.type = TOK_ERROR;
			    return;
			}
			break;
		    }
		} else {
		    ctx->tok.tvalue[len++] = ctx->lex_last;
		    ick_lex_read(ctx);
		}
	    }
	}
      default:
	if (ctx->lex_last >= '0' && ctx->lex_last <= '9') {
	    /*
	     * Integer literal.
	     */
	    int base = (ctx->lex_last == '0' ? 8 : 10);

	    ctx->tok.ivalue = ctx->lex_last - '0';
	    ick_lex_read(ctx);
	    if (base == 8 && (ctx->lex_last == 'x' ||
			      ctx->lex_last == 'X')) {
		base = 16;
		ick_lex_read(ctx);
	    }
	    while (1) {
		int digit = -1;
		if (base == 16) {
		    if (ctx->lex_last >= 'A' && ctx->lex_last <= 'F')
			digit = ctx->lex_last - 'A' + 10;
		    else if (ctx->lex_last >= 'a' && ctx->lex_last <= 'f')
			digit = ctx->lex_last - 'a' + 10;
		}
		if (ctx->lex_last >= '0' &&
		    ctx->lex_last <= (base == 8 ? '7' : '9'))
		    digit = ctx->lex_last - '0';
		if (digit >= 0) {
		    ctx->tok.ivalue = ctx->tok.ivalue * base + digit;
		    ick_lex_read(ctx);
		} else
		    break;
	    }
	    ctx->tok.type = TOK_INTLIT;
	    return;
	}

	if ((ctx->lex_last >= 'A' && ctx->lex_last <= 'Z') ||
	    (ctx->lex_last >= 'a' && ctx->lex_last <= 'z') ||
	    ctx->lex_last == '_' || ctx->lex_last == '$') {
	    /*
	     * Identifier, or keyword.
	     */
	    int len = 0;

	    while (1) {
		if (len+1 >= ctx->tok.tvalue_allocsize) {
		    ctx->tok.tvalue_allocsize =
			(ctx->tok.tvalue_allocsize * 3 / 2) + 64;
		    ctx->tok.tvalue = sresize(ctx->tok.tvalue,
					      ctx->tok.tvalue_allocsize,
					      char);
		}

		ctx->tok.tvalue[len++] = ctx->lex_last;
		ctx->tok.tvalue[len] = '\0';

		ick_lex_read(ctx);
		if ((ctx->lex_last >= 'A' && ctx->lex_last <= 'Z') ||
		    (ctx->lex_last >= 'a' && ctx->lex_last <= 'z') ||
		    (ctx->lex_last >= '0' && ctx->lex_last <= '9') ||
		    ctx->lex_last == '_' || ctx->lex_last == '$')
		    continue;
		else
		    break;
	    }

	    if (!strcmp(ctx->tok.tvalue, "string"))
		ctx->tok.type = TOK_KW_STRING;
	    else if (!strcmp(ctx->tok.tvalue, "int"))
		ctx->tok.type = TOK_KW_INT;
	    else if (!strcmp(ctx->tok.tvalue, "bool"))
		ctx->tok.type = TOK_KW_BOOL;
	    else if (!strcmp(ctx->tok.tvalue, "void"))
		ctx->tok.type = TOK_KW_VOID;
	    else if (!strcmp(ctx->tok.tvalue, "return"))
		ctx->tok.type = TOK_KW_RETURN;
	    else if (!strcmp(ctx->tok.tvalue, "break"))
		ctx->tok.type = TOK_KW_BREAK;
	    else if (!strcmp(ctx->tok.tvalue, "continue"))
		ctx->tok.type = TOK_KW_CONTINUE;
	    else if (!strcmp(ctx->tok.tvalue, "if"))
		ctx->tok.type = TOK_KW_IF;
	    else if (!strcmp(ctx->tok.tvalue, "else"))
		ctx->tok.type = TOK_KW_ELSE;
	    else if (!strcmp(ctx->tok.tvalue, "for"))
		ctx->tok.type = TOK_KW_FOR;
	    else if (!strcmp(ctx->tok.tvalue, "while"))
		ctx->tok.type = TOK_KW_WHILE;
	    else if (!strcmp(ctx->tok.tvalue, "do"))
		ctx->tok.type = TOK_KW_DO;
	    else if (!strcmp(ctx->tok.tvalue, "true"))
		ctx->tok.type = TOK_BOOLLIT, ctx->tok.ivalue = 1;
	    else if (!strcmp(ctx->tok.tvalue, "false"))
		ctx->tok.type = TOK_BOOLLIT, ctx->tok.ivalue = 0;
	    else
		ctx->tok.type = TOK_IDENTIFIER;
	    return;
	}

	ctx->tok.type = TOK_ERROR;
	return;
    }
}

/* ----------------------------------------------------------------------
 * Ick syntax parser.
 */

#define is_nonvoid_type(x) ((x) == TOK_KW_STRING || \
			    (x) == TOK_KW_INT || \
			    (x) == TOK_KW_BOOL)
#define is_valid_type(x) (is_nonvoid_type(x) || \
			  (x) == TOK_KW_VOID)

static struct ick_expr *ick_make_exprnode
    (struct ick_srcpos oppos, struct ick_expr *e1,
     struct ick_expr *e2, int op)
{
    struct ick_expr *ret = snew(struct ick_expr);
    ret->e1 = e1;
    ret->e2 = e2;
    ret->e3 = NULL;
    ret->op = op;
    ret->ival = 0;
    ret->sval = NULL;
    ret->oppos = oppos;
    if (e1) {
	ret->startpos = e1->startpos;
    } else {
	ret->startpos.line = ret->startpos.col = -1;
    }
    ret->op2pos.line = ret->op2pos.col = -1;
    return ret;
}

static struct ick_statement *ick_make_statement
    (struct ick_srcpos startpos, int type,
     struct ick_expr *e1, struct ick_expr *e2, struct ick_expr *e3,
     struct ick_statement *s1, struct ick_statement *s2)
{
    struct ick_statement *ret = snew(struct ick_statement);
    ret->startpos = startpos;
    ret->type = type;
    ret->e1 = e1;
    ret->e2 = e2;
    ret->e3 = e3;
    ret->s1 = s1;
    ret->s2 = s2;
    ret->blk = NULL;
    return ret;
}

static void ick_free_expr(struct ick_expr *expr);
static void ick_free_statement(struct ick_statement *stmt);
static void ick_free_block(struct ick_block *blk);

static void ick_free_expr(struct ick_expr *expr)
{
    if (!expr)
	return;
    ick_free_expr(expr->e1);
    ick_free_expr(expr->e2);
    ick_free_expr(expr->e3);
    sfree(expr->sval);
    sfree(expr);
}

static void ick_free_statement(struct ick_statement *stmt)
{
    if (!stmt)
	return;
    ick_free_expr(stmt->e1);
    ick_free_expr(stmt->e2);
    ick_free_expr(stmt->e3);
    ick_free_statement(stmt->s1);
    ick_free_statement(stmt->s2);
    ick_free_block(stmt->blk);
    sfree(stmt);
}

static void ick_free_vardecls(struct ick_vardecls *vars)
{
    int i;

    for (i = 0; i < vars->ndecls; i++) {
	struct ick_vardecl *var = &vars->decls[i];
	sfree(var->identifier);
	ick_free_expr(var->initialiser);
    }
    sfree(vars->decls);
}

static void ick_free_block(struct ick_block *blk)
{
    int i;

    if (!blk)
	return;
    ick_free_vardecls(&blk->vars);
    for (i = 0; i < blk->nstmts; i++)
	ick_free_statement(blk->stmts[i]);
    sfree(blk->stmts);
    sfree(blk);
}

static void ick_free_parsetree(struct ick_parse_tree *tree)
{
    int i;

    ick_free_vardecls(&tree->toplevel_vars);
    for (i = 0; i < tree->nfns; i++) {
	struct ick_fndecl *fn = &tree->fns[i];
	sfree(fn->identifier);
	ick_free_vardecls(&fn->params);
	ick_free_block(fn->defn);
    }
    sfree(tree->fns);
    sfree(tree);
}

static struct ick_expr *ick_parse_expr0(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr1(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr2(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr3(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr4(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr5(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr6(struct ick_parse_ctx *ctx);
static struct ick_expr *ick_parse_expr7(struct ick_parse_ctx *ctx);
static struct ick_block *ick_parse_block(struct ick_parse_ctx *ctx);

static struct ick_expr *ick_parse_expr(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;

    a = ick_parse_expr0(ctx);
    if (!a)
	return NULL;

    while (ctx->tok.type == TOK_COMMA) {
	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr0(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, TOK_COMMA);
    }

    return a;
}

static struct ick_expr *ick_parse_expr0(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;

    /*
     * We don't check here that the left-hand side of an
     * assignment expression is an lvalue, because although we
     * could do so in this limited language (only a single
     * identifier can ever be an lvalue) it would involve
     * backtracking the parser by a token. So it's simpler not to:
     * we construct the parse tree just as if any expression could
     * be an lvalue, and police the constraint during the
     * subsequent semantic analysis phase.
     */
    a = ick_parse_expr1(ctx);
    if (!a)
	return NULL;

    if (ctx->tok.type == TOK_ASSIGN ||
	ctx->tok.type == TOK_PLUSASSIGN ||
	ctx->tok.type == TOK_MINUSASSIGN ||
	ctx->tok.type == TOK_MULASSIGN ||
	ctx->tok.type == TOK_DIVASSIGN ||
	ctx->tok.type == TOK_LOGANDASSIGN ||
	ctx->tok.type == TOK_LOGORASSIGN) {
	int type = ctx->tok.type;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr0(ctx);      /* right-associative, so recurse */
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, type);
    }

    return a;
}

static struct ick_expr *ick_parse_expr1(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b, *c;
    struct ick_srcpos pos, pos2;

    a = ick_parse_expr2(ctx);
    if (!a)
	return NULL;

    if (ctx->tok.type == TOK_QUERY) {
	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	if (ctx->tok.type != TOK_COLON) {
	    ick_error(ctx, ctx->tok.pos, "expected ':' after second part of"
		      " '?' expression; found %p", ctx->tok.type);
	    ick_free_expr(a);
	    ick_free_expr(b);
	    return NULL;
	}
	pos2 = ctx->tok.pos;
	ick_lex_advance(ctx);

	c = ick_parse_expr1(ctx);      /* right-associative, so recurse */

	a = ick_make_exprnode(pos, a, b, TOK_QUERY);
	a->op2pos = pos2;
	a->e3 = c;
    }

    return a;
}

static struct ick_expr *ick_parse_expr2(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;
    int toktype = -1;

    a = ick_parse_expr3(ctx);
    if (!a)
	return NULL;

    while (ctx->tok.type == TOK_LOGAND ||
	   ctx->tok.type == TOK_LOGOR) {
	if (toktype < 0)
	    toktype = ctx->tok.type;
	else if (toktype != ctx->tok.type) {
	    ick_free_expr(a);
	    ick_error(ctx, ctx->tok.pos, "expected parentheses disambiguating"
		      " a boolean expression containing both && and ||");
	    return NULL;
	}

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr3(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, toktype);
    }

    return a;
}

static struct ick_expr *ick_parse_expr3(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;

    a = ick_parse_expr4(ctx);
    if (!a)
	return NULL;

    while (ctx->tok.type == TOK_LT ||
	   ctx->tok.type == TOK_LE ||
	   ctx->tok.type == TOK_GT ||
	   ctx->tok.type == TOK_GE ||
	   ctx->tok.type == TOK_EQ ||
	   ctx->tok.type == TOK_NE) {
	int type = ctx->tok.type;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr4(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, type);
    }

    return a;
}

static struct ick_expr *ick_parse_expr4(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;

    a = ick_parse_expr5(ctx);
    if (!a)
	return NULL;

    while (ctx->tok.type == TOK_PLUS ||
	   ctx->tok.type == TOK_MINUS) {
	int type = ctx->tok.type;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr5(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, type);
    }

    return a;
}

static struct ick_expr *ick_parse_expr5(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a, *b;
    struct ick_srcpos pos;

    a = ick_parse_expr6(ctx);
    if (!a)
	return NULL;

    while (ctx->tok.type == TOK_MUL ||
	   ctx->tok.type == TOK_DIV) {
	int type = ctx->tok.type;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	b = ick_parse_expr6(ctx);
	if (!b) {
	    ick_free_expr(a);
	    return NULL;
	}

	a = ick_make_exprnode(pos, a, b, type);
    }

    return a;
}

static struct ick_expr *ick_parse_expr6(struct ick_parse_ctx *ctx)
{
    struct ick_expr *a;
    struct ick_srcpos pos;

    if (ctx->tok.type == TOK_MINUS ||
	ctx->tok.type == TOK_PLUS ||
	ctx->tok.type == TOK_LOGNOT) {
	int type = ctx->tok.type;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	a = ick_parse_expr6(ctx);
	a = ick_make_exprnode(pos, a, NULL, type);

	return a;
    } else if (ctx->tok.type == TOK_INCREMENT ||
	       ctx->tok.type == TOK_DECREMENT) {
	int type = ctx->tok.type;
	struct ick_srcpos pos;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);

	a = ick_parse_expr7(ctx);
	a = ick_make_exprnode(pos, a, NULL, type);
	a->startpos = pos;

	return a;
    } else {
	a = ick_parse_expr7(ctx);

	if (ctx->tok.type == TOK_INCREMENT ||
	    ctx->tok.type == TOK_DECREMENT) {
	    a = ick_make_exprnode(ctx->tok.pos, NULL, a, ctx->tok.type);
	    a->startpos = a->oppos;
	    ick_lex_advance(ctx);
	}
    }

    return a;
}

static struct ick_expr *ick_parse_expr7(struct ick_parse_ctx *ctx)
{
    struct ick_srcpos pos;

    pos = ctx->tok.pos;

    if (ctx->tok.type == TOK_STRINGLIT ||
	ctx->tok.type == TOK_INTLIT ||
	ctx->tok.type == TOK_BOOLLIT ||
	ctx->tok.type == TOK_IDENTIFIER) {
	struct ick_expr *a = ick_make_exprnode(pos, NULL, NULL, ctx->tok.type);
	a->startpos = a->oppos;
	if (ctx->tok.type == TOK_STRINGLIT ||
	    ctx->tok.type == TOK_IDENTIFIER) {
	    int len;
	    a->sval = ctx->tok.tvalue;
	    ctx->tok.tvalue = NULL;
	    ctx->tok.tvalue_allocsize = 0;
	    ick_lex_advance(ctx);
	    if (a->op == TOK_STRINGLIT) {
		len = strlen(a->sval);
		while (ctx->tok.type == TOK_STRINGLIT) {
		    /*
		     * Accept multiple adjacent string literal
		     * tokens and concatenate them into one big
		     * string literal.
		     */
		    int len2 = strlen(ctx->tok.tvalue);
		    a->sval = sresize(a->sval, len + len2 + 1, char);
		    strcpy(a->sval + len, ctx->tok.tvalue);
		    len += len2;
		    ick_lex_advance(ctx);
		}
	    }
	} else {
	    a->ival = ctx->tok.ivalue;
	    ick_lex_advance(ctx);
	}
	if (a->op == TOK_IDENTIFIER && ctx->tok.type == TOK_LPAREN) {
	    /*
	     * This isn't a variable reference after all; it's a
	     * function call.
	     */
	    a->op = TOK_LPAREN;      /* that'll do to signal a func call */
	    a->oppos = ctx->tok.pos;
	    ick_lex_advance(ctx);
	    if (ctx->tok.type != TOK_RPAREN) {
		/*
		 * ... with arguments. We'll put the arguments in
		 * a chain of strange-looking expression nodes
		 * hanging off a's e2 slot.
		 */
		struct ick_expr *curr = a;
		while (1) {
		    struct ick_expr *param = ick_parse_expr0(ctx);
		    if (!param) {
			ick_free_expr(a);   /* frees all previous params too */
			return NULL;
		    }
		    curr->e2 = ick_make_exprnode(ctx->tok.pos, param,
						 NULL, TOK_RPAREN);
		    curr = curr->e2;
		    if (ctx->tok.type == TOK_RPAREN) {
			break;
		    } else if (ctx->tok.type == TOK_COMMA) {
			ick_lex_advance(ctx);
			continue;
		    } else {
			ick_error(ctx, ctx->tok.pos, "expected closing parent"
				  "hesis or comma after function argument;"
				  " found %p", ctx->tok.type);
			ick_free_expr(a);
			return NULL;
		    }
		}
	    }
	    ick_lex_advance(ctx);  /* eat right paren */
	}
	return a;
    } else if (ctx->tok.type == TOK_LPAREN) {
	struct ick_expr *a;

	pos = ctx->tok.pos;
	ick_lex_advance(ctx);
	a = ick_parse_expr(ctx);
	a->startpos = pos;
	if (ctx->tok.type != TOK_RPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected closing parenthesis in "
		      "expression; found %p", ctx->tok.type);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);
	return a;
    } else {
	ick_error(ctx, ctx->tok.pos, "expected literal or identifier in"
		  " expression; found %p", ctx->tok.type);
	return NULL;
    }
}

static struct ick_statement *ick_parse_statement(struct ick_parse_ctx *ctx)
{
    int type;
    struct ick_expr *a, *b, *c;
    struct ick_statement *p, *q;
    struct ick_block *x;
    struct ick_srcpos pos;

    pos = ctx->tok.pos;

    switch (ctx->tok.type) {
      case TOK_KW_RETURN:
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_SEMI) {
	    a = ick_parse_expr(ctx);
	    if (!a)
		return NULL;
	    if (ctx->tok.type != TOK_SEMI) {
		ick_error(ctx, ctx->tok.pos, "expected semicolon after "
			  "'return' statement; found %p", ctx->tok.type);
		ick_free_expr(a);
		return NULL;
	    }
	    ick_lex_advance(ctx);
	} else {
	    ick_lex_advance(ctx);      /* eat semicolon */
	    a = NULL;
	}
	return ick_make_statement(pos, TOK_KW_RETURN, a, NULL,
				  NULL, NULL, NULL);

      case TOK_KW_BREAK:
      case TOK_KW_CONTINUE:
	type = ctx->tok.type;
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_SEMI) {
	    ick_error(ctx, ctx->tok.pos, "expected semicolon after %p"
		      " statement; found %p", type, ctx->tok.type);
	    return NULL;
	}
	ick_lex_advance(ctx);
	return ick_make_statement(pos, type, NULL, NULL, NULL, NULL, NULL);

      case TOK_KW_WHILE:
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_LPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected open parenthesis after"
		      " 'while'; found %p", ctx->tok.type);
	    return NULL;
	}
	ick_lex_advance(ctx);
	a = ick_parse_expr(ctx);
	if (!a)
	    return NULL;
	if (ctx->tok.type != TOK_RPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected closing parenthesis after"
		      " 'while' expression; found %p", ctx->tok.type);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);
	p = ick_parse_statement(ctx);
	if (!p) {
	    ick_free_expr(a);
	    return NULL;
	}
	return ick_make_statement(pos, TOK_KW_WHILE, a, NULL, NULL, p, NULL);

      case TOK_KW_IF:
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_LPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected open parenthesis after 'if'"
		      "; found %p", ctx->tok.type);
	    return NULL;
	}
	ick_lex_advance(ctx);
	a = ick_parse_expr(ctx);
	if (!a)
	    return NULL;
	if (ctx->tok.type != TOK_RPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected closing parenthesis after"
		      " 'if' expression; found %p", ctx->tok.type);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);
	p = ick_parse_statement(ctx);
	if (!p) {
	    ick_free_expr(a);
	    return NULL;
	}
	if (ctx->tok.type == TOK_KW_ELSE) {
	    ick_lex_advance(ctx);
	    q = ick_parse_statement(ctx);
	    if (!q) {
		ick_free_expr(a);
		ick_free_statement(p);
		return NULL;
	    }
	} else
	    q = NULL;
	
	return ick_make_statement(pos, TOK_KW_IF, a, NULL, NULL, p, q);

      case TOK_KW_FOR:
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_LPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected open parenthesis after"
		      " 'for'; found %p", ctx->tok.type);
	    return NULL;
	}
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_SEMI) {
	    a = ick_parse_expr(ctx);
	    if (!a)
		return NULL;
	    if (ctx->tok.type != TOK_SEMI) {
		ick_error(ctx, ctx->tok.pos, "expected semicolon after 'for' "
			  "initialisation expression; found %p",
			  ctx->tok.type);
		ick_free_expr(a);
		return NULL;
	    }
	} else
	    a = NULL;
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_SEMI) {
	    b = ick_parse_expr(ctx);
	    if (!b) {
		ick_free_expr(a);
		return NULL;
	    }
	    if (ctx->tok.type != TOK_SEMI) {
		ick_error(ctx, ctx->tok.pos, "expected semicolon after 'for' "
			  "termination condition expression; found %p",
			  ctx->tok.type);
		ick_free_expr(a);
		ick_free_expr(b);
		return NULL;
	    }
	} else
	    b = NULL;
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_RPAREN) {
	    c = ick_parse_expr(ctx);
	    if (!c) {
		ick_free_expr(a);
		ick_free_expr(b);
		return NULL;
	    }
	    if (ctx->tok.type != TOK_RPAREN) {
		ick_error(ctx, ctx->tok.pos, "expected closing parenthesis"
			  " after 'for' termination condition expression"
			  "; found %p", ctx->tok.type);
		ick_free_expr(a);
		ick_free_expr(b);
		ick_free_expr(c);
		return NULL;
	    }
	} else
	    c = NULL;
	ick_lex_advance(ctx);
	p = ick_parse_statement(ctx);
	if (!p) {
	    ick_free_expr(a);
	    ick_free_expr(b);
	    ick_free_expr(c);
	    return NULL;
	}
	
	return ick_make_statement(pos, TOK_KW_FOR, a, b, c, p, NULL);

      case TOK_KW_DO:
	ick_lex_advance(ctx);
	p = ick_parse_statement(ctx);
	if (!p)
	    return NULL;

	if (ctx->tok.type != TOK_KW_WHILE) {
	    ick_error(ctx, ctx->tok.pos, "expected 'while' after 'do'"
		      " statement; found %p", ctx->tok.type);
	    ick_free_statement(p);
	    return NULL;
	}
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_LPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected open parenthesis after "
		      "'do'...'while'; found %p", ctx->tok.type);
	    ick_free_statement(p);
	    return NULL;
	}
	ick_lex_advance(ctx);
	a = ick_parse_expr(ctx);
	if (!a)
	    return NULL;
	if (ctx->tok.type != TOK_RPAREN) {
	    ick_error(ctx, ctx->tok.pos, "expected closing parenthesis after"
		      " 'while' expression; found %p", ctx->tok.type);
	    ick_free_statement(p);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);
	if (ctx->tok.type != TOK_SEMI) {
	    ick_error(ctx, ctx->tok.pos, "expected semicolon after"
		      " 'do'...'while'; found %p", ctx->tok.type);
	    ick_free_statement(p);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);

	return ick_make_statement(pos, TOK_KW_DO, a, NULL, NULL, p, NULL);

      case TOK_LBRACE:
	ick_lex_advance(ctx);
	x = ick_parse_block(ctx);
	if (!x)
	    return NULL;
	assert(ctx->tok.type == TOK_RBRACE);
	ick_lex_advance(ctx);
	p = ick_make_statement(pos, TOK_LBRACE, NULL, NULL, NULL, NULL, NULL);
	p->blk = x;
	return p;

      default:
	a = ick_parse_expr(ctx);
	if (!a)
	    return NULL;
	if (ctx->tok.type != TOK_SEMI) {
	    ick_error(ctx, ctx->tok.pos, "expected semicolon after "
		      "expression-statement; found %p", ctx->tok.type);
	    ick_free_expr(a);
	    return NULL;
	}
	ick_lex_advance(ctx);
	return ick_make_statement(pos, -1, a, NULL, NULL, NULL, NULL);
    }
}

static int ick_parse_vardecl(struct ick_parse_ctx *ctx,
			     struct ick_vardecls *vars,
			     struct ick_srcpos pos, int type, char *varname)
{
    struct ick_expr *expr;

    if (type == TOK_KW_VOID) {
	ick_error(ctx, pos, "unable to declare variable with void type");
	sfree(varname);
	return 0;
    }

    /*
     * Parse a variable declaration. Due to the top-level syntax,
     * this function expects to be called _after_ the lexer has
     * advanced past the initial type name and first variable
     * name, and hence they're passed in as arguments. The current
     * token should be the first actual variable name being
     * declared.
     */
    while (1) {

	if (!varname) {
	    /*
	     * Expect the variable name.
	     */
	    if (ctx->tok.type != TOK_IDENTIFIER) {
		ick_error(ctx, ctx->tok.pos, "expected identifier in variable "
			  "declaration; found %p", ctx->tok.type);
		return 0;
	    }
	    varname = ctx->tok.tvalue;
	    ctx->tok.tvalue = NULL;
	    ctx->tok.tvalue_allocsize = 0;
	    ick_lex_advance(ctx);
	}

	/*
	 * See if there's an initialiser.
	 */
	if (ctx->tok.type == TOK_ASSIGN) {
	    ick_lex_advance(ctx);

	    expr = ick_parse_expr2(ctx);
	    if (!expr) {
		sfree(varname);
		return 0;
	    }
	} else
	    expr = NULL;

	/*
	 * Enter the variable definition (type, varname, expr) in
	 * our decls structure.
	 */
	if (vars->ndecls >= vars->declsize) {
	    vars->declsize = (vars->ndecls * 3 / 2) + 32;
	    vars->decls = sresize(vars->decls, vars->declsize,
				  struct ick_vardecl);
	}
	vars->decls[vars->ndecls].identifier = varname;
	varname = NULL;
	vars->decls[vars->ndecls].type = type;
	vars->decls[vars->ndecls].initialiser = expr;
	vars->ndecls++;

	/*
	 * Now we expect either a comma (in which case we loop
	 * round for another variable name), or a semicolon in
	 * which case we stop.
	 */
	if (ctx->tok.type == TOK_COMMA) {
	    ick_lex_advance(ctx);
	    continue;
	} else if (ctx->tok.type == TOK_SEMI) {
	    ick_lex_advance(ctx);
	    break;
	} else {
	    ick_error(ctx, ctx->tok.pos, "expected comma or semicolon after "
		      "variable declaration; found %p", ctx->tok.type);
	    return 0;
	}
    }

    return 1;
}

static struct ick_block *ick_parse_block(struct ick_parse_ctx *ctx)
{
    /*
     * This function is called after we advance past the
     * TOK_LBRACE at the start. We must terminate successfully
     * _only_ if we are staring at the closing TOK_RBRACE; any
     * other unrecognised token, including TOK_EOF, qualifies as
     * an error condition which _we_ must handle and return NULL.
     */
    struct ick_block *blk;
    struct ick_srcpos varpos;

    blk = snew(struct ick_block);
    blk->stmts = NULL;
    blk->nstmts = blk->stmtsize = 0;
    blk->vars.decls = NULL;
    blk->vars.ndecls = blk->vars.declsize = 0;

    /*
     * Process block-local variable declarations.
     */
    while (is_valid_type(ctx->tok.type)) {
	int type = ctx->tok.type;
	char *name;

	varpos = ctx->tok.pos;
	ick_lex_advance(ctx);	       /* eat type name */

	if (ctx->tok.type != TOK_IDENTIFIER) {
	    ick_error(ctx, ctx->tok.pos, "expected identifier after type name"
		      "; found %p", ctx->tok.type);
	    ick_free_block(blk);
	    return NULL;
	}
	name = ctx->tok.tvalue;
	ctx->tok.tvalue = NULL;
	ctx->tok.tvalue_allocsize = 0;
	ick_lex_advance(ctx);	       /* eat initial identifier */

	if (!ick_parse_vardecl(ctx, &blk->vars, varpos, type, name)) {
	    ick_free_block(blk);
	    return NULL;
	}
    }

    /*
     * Now process statements.
     */
    while (ctx->tok.type != TOK_RBRACE) {
	struct ick_statement *stmt = ick_parse_statement(ctx);
	if (!stmt) {
	    ick_free_block(blk);
	    return NULL;
	}
	if (blk->nstmts >= blk->stmtsize) {
	    blk->stmtsize = (blk->nstmts * 3 / 2) + 32;
	    blk->stmts = sresize(blk->stmts, blk->stmtsize,
				 struct ick_statement *);
	}
	blk->stmts[blk->nstmts++] = stmt;
    }

    return blk;
}

static struct ick_parse_tree *ick_parse_toplevel(struct ick_parse_ctx *ctx)
{
    struct ick_parse_tree *tree = snew(struct ick_parse_tree);
    struct ick_srcpos pos;
    tree->fns = NULL;
    tree->nfns = tree->fnsize = 0;
    tree->toplevel_vars.decls = NULL;
    tree->toplevel_vars.ndecls = tree->toplevel_vars.declsize = 0;

    while (ctx->tok.type != TOK_EOF) {
	/*
	 * Expect either a variable declaration or a function
	 * definition. Both of these start off with a type name
	 * followed by an identifier, so we can't tell the
	 * difference until after that when we find out if there's
	 * an open parenthesis.
	 */
	int type = ctx->tok.type;
	char *name;

	pos = ctx->tok.pos;

	if (!is_valid_type(type)) {
	    ick_error(ctx, ctx->tok.pos, "expected type name at top level"
		      "; found %p", ctx->tok.type);
	    ick_free_parsetree(tree);
	    return NULL;
	}
	ick_lex_advance(ctx);

	if (ctx->tok.type != TOK_IDENTIFIER) {
	    ick_error(ctx, ctx->tok.pos, "expected identifier after type name"
		      " at top level; found %p", ctx->tok.type);
	    ick_free_parsetree(tree);
	    return NULL;
	}
	name = ctx->tok.tvalue;
	ctx->tok.tvalue = NULL;
	ctx->tok.tvalue_allocsize = 0;
	ick_lex_advance(ctx);

	if (ctx->tok.type == TOK_LPAREN) {
	    struct ick_fndecl *fn;

	    /*
	     * We're doing a function definition.
	     */
	    if (tree->nfns >= tree->fnsize) {
		tree->fnsize = (tree->nfns * 3 / 2) + 32;
		tree->fns = sresize(tree->fns, tree->fnsize,
				    struct ick_fndecl);
	    }
	    fn = &tree->fns[tree->nfns++];

	    fn->identifier = name;
	    fn->rettype = type;
	    fn->params.decls = NULL;
	    fn->params.ndecls = fn->params.declsize = 0;
	    fn->defn = NULL;
	    fn->istack = fn->sstack = 0;
	    fn->native = NULL;
	    fn->js = NULL;
	    fn->startpos = pos;

	    ick_lex_advance(ctx);      /* eat LPAREN */

	    if (ctx->tok.type == TOK_KW_VOID) {
		/*
		 * Optional `void' marking a function with no
		 * parameters.
		 */
		ick_lex_advance(ctx);
		if (ctx->tok.type != TOK_RPAREN) {
		    ick_error(ctx, ctx->tok.pos, "expected closing parenthesis"
			      " after 'void'; found %p", ctx->tok.type);
		    ick_free_parsetree(tree);
		    return NULL;
		}
	    }
	    if (ctx->tok.type != TOK_RPAREN) {
		int partype;
		char *parname;

		/*
		 * The function has parameters.
		 */
		while (1) {
		    partype = ctx->tok.type;
		    if (!is_nonvoid_type(partype)) {
			ick_error(ctx, ctx->tok.pos, "expected type name in "
				  "function parameter list; found %p",
				  ctx->tok.type);
			ick_free_parsetree(tree);
			return NULL;
		    }
		    ick_lex_advance(ctx);

		    if (ctx->tok.type != TOK_IDENTIFIER) {
			ick_error(ctx, ctx->tok.pos, "expected identifier"
				  " after type name in function parameter list"
				  "; found %p", ctx->tok.type);
			ick_free_parsetree(tree);
			return NULL;
		    }
		    parname = ctx->tok.tvalue;
		    ctx->tok.tvalue = NULL;
		    ctx->tok.tvalue_allocsize = 0;
		    ick_lex_advance(ctx);

		    /*
		     * Enter the parameter in the function's
		     * parameter list.
		     */
		    if (fn->params.ndecls >= fn->params.declsize) {
			fn->params.declsize = (fn->params.ndecls * 3 / 2) + 32;
			fn->params.decls = sresize(fn->params.decls,
						   fn->params.declsize,
						   struct ick_vardecl);
		    }
		    fn->params.decls[fn->params.ndecls].identifier = parname;
		    fn->params.decls[fn->params.ndecls].type = partype;
		    fn->params.decls[fn->params.ndecls].initialiser = NULL;
		    fn->params.ndecls++;

		    /*
		     * If we're looking at a comma, advance past
		     * it and expect another parameter.
		     */
		    if (ctx->tok.type == TOK_COMMA) {
			ick_lex_advance(ctx);
			continue;
		    } else
			break;
		}

		if (ctx->tok.type != TOK_RPAREN) {
		    ick_error(ctx, ctx->tok.pos, "expected comma or closing "
			      "parenthesis in function parameter list"
			      "; found %p", ctx->tok.type);
		    ick_free_parsetree(tree);
		    return NULL;
		}
	    }
	    ick_lex_advance(ctx);      /* eat right paren */

	    if (ctx->tok.type != TOK_LBRACE) {
		ick_error(ctx, ctx->tok.pos, "expected open brace after "
			  "function parameter list; found %p", ctx->tok.type);
		ick_free_parsetree(tree);
		return NULL;
	    }
	    ick_lex_advance(ctx);

	    fn->defn = ick_parse_block(ctx);
	    if (!fn->defn) {
		ick_free_parsetree(tree);
		return NULL;
	    }

	    assert(ctx->tok.type == TOK_RBRACE);
	    ick_lex_advance(ctx);
	} else {
	    /*
	     * We're doing a variable declaration, so hand off to
	     * the code which handles those, passing in the type
	     * and name tokens which we've already gone past.
	     */
	    if (!ick_parse_vardecl(ctx, &tree->toplevel_vars,
				   pos, type, name)) {
		ick_free_parsetree(tree);
		return NULL;
	    }
	}
    }
    return tree;
}

/* ----------------------------------------------------------------------
 * Ick semantic analysis phase.
 * 
 * This takes place after parsing is complete. Its functions are:
 * 
 *  - to match up variable references and function calls to the
 *    actual variables and functions they refer to (in particular,
 *    to handle the scoping rules and decide which of a number of
 *    variables with the same name is the one referred to) and
 *    flag references to unknown variables or functions
 * 
 *  - to determine the type of every expression (and hence make it
 *    clear what operation is intended by every operator - whether
 *    + means integer addition or string concatenation, for
 *    instance) and flag type checking errors
 * 
 *  - to ensure that anything assigned to, incremented or
 *    decremented is an lvalue.
 */

static int ick_semantic_expr(struct ick_parse_ctx *ctx,
			     struct ick_expr *expr)
{
    int i, j;

    if (!expr)
	return 1;

    /*
     * First, recurse into subexpressions. We'll need their types
     * to have been sorted out before we can figure out the type
     * of this node.
     */
    if (!ick_semantic_expr(ctx, expr->e1) ||
	!ick_semantic_expr(ctx, expr->e2) ||
	!ick_semantic_expr(ctx, expr->e3))
	return 0;

    /*
     * Fill in the fnbelow field, which will be used during code
     * generation to choose between function argument generation
     * strategies.
     */
    expr->fnbelow = (expr->op == TOK_LPAREN ||
		     (expr->e1 && expr->e1->fnbelow) ||
		     (expr->e2 && expr->e2->fnbelow) ||
		     (expr->e3 && expr->e3->fnbelow));

    /*
     * Identify function and variable definitions and fill in
     * their cross-reference fields.
     */
    if (expr->op == TOK_IDENTIFIER) {
	/*
	 * Find the variable to which this is a reference.
	 */
	for (i = ctx->nvarscope; i-- > 0 ;)
	    if (!strcmp(ctx->varscope[i]->identifier, expr->sval))
		break;
	if (i < 0) {
	    ick_error(ctx, expr->oppos, "unable to find definition of "
		      "variable '%s'", expr->sval);
	    return 0;
	}
	expr->var = ctx->varscope[i];
    } else if (expr->op == TOK_LPAREN) {
	/*
	 * Find the function to which this is a call. We support
	 * function overloading by number and type of parameters,
	 * so we must check all of that.
	 */
	expr->fn = NULL;

	for (i = 0; i < ctx->tree->nfns + ctx->lib->nfns; i++) {
	    struct ick_fndecl *fn;
	    struct ick_expr *arg;

	    if (i < ctx->tree->nfns)
		fn = &ctx->tree->fns[i];
	    else
		fn = &ctx->lib->fns[i - ctx->tree->nfns];
	    if (strcmp(fn->identifier, expr->sval))
		continue;	       /* name doesn't match */
	    arg = expr->e2;
	    j = 0;
	    while (1) {
		if (j == fn->params.ndecls || !arg) {
		    /*
		     * One or other list has ended.
		     */
		    if (j == fn->params.ndecls && !arg) {
			/*
			 * Both have ended at the same time.
			 * Success!
			 */
			expr->fn = fn;
		    }
		    break;
		}

		if (fn->params.decls[j].type != arg->e1->type)
		    break;	       /* argument type mismatch */

		j++;
		arg = arg->e2;
	    }
	}

	if (!expr->fn) {
	    ick_error(ctx, expr->startpos, "unable to find definition "
		      "of function '%s(%a)'", expr->sval, expr->e2);
	    return 0;	    
	}
    }

    /*
     * Check that the lvalue operand to an assignment or
     * inc/decrement operator is actually an lvalue. Since all
     * lvalues are direct references to variables in this
     * language, we can also fill in the var field in the
     * expression node to indicate the location of the lvalue.
     */
    if (expr->op == TOK_ASSIGN ||
	expr->op == TOK_PLUSASSIGN ||
	expr->op == TOK_MINUSASSIGN ||
	expr->op == TOK_MULASSIGN ||
	expr->op == TOK_DIVASSIGN ||
	expr->op == TOK_LOGANDASSIGN ||
	expr->op == TOK_LOGORASSIGN ||
	expr->op == TOK_INCREMENT ||
	expr->op == TOK_DECREMENT) {

	struct ick_expr *lv;

	/*
	 * Identify the supposed lvalue.
	 */
	lv = expr->e1;
	if (!lv) {
	    assert(expr->op == TOK_INCREMENT || expr->op == TOK_DECREMENT);
	    lv = expr->e2;
	}

	/*
	 * It should simply be a TOK_IDENTIFIER node, and
	 * therefore (of course) have had its var field filled in
	 * already.
	 */
	if (lv->op != TOK_IDENTIFIER) {
	    ick_error(ctx, expr->oppos, "expected %soperand to %p operator to"
		      " be a variable",
		      (expr->op == TOK_INCREMENT || expr->op == TOK_DECREMENT
		       ? "" : "left "), expr->op);
	    return 0;
	}
	expr->var = lv->var;
    }

    /*
     * Now type-check the expression. For this there's nothing for
     * it but to switch over all the possible types.
     */
    switch (expr->op) {
      case TOK_COMMA:
	/*
	 * Comma operator: operand types need not match, and type
	 * of result is type of e2.
	 */
	expr->type = expr->e2->type;
	break;
      case TOK_QUERY:
	/*
	 * Conditional operator. e1 must be boolean; e2 and e3
	 * must match, and type of result is their shared type.
	 * Exception: if one of e2,e3 is void, then the other need
	 * not match it, and the result is unconditionally void.
	 */
	if (expr->e1->type != TOK_KW_BOOL) {
	    ick_error(ctx, expr->oppos, "expected boolean expression"
		      " before '?'; found %T", expr->e1->type);
	    return 0;
	}
	if (expr->e2->type == TOK_KW_VOID || expr->e3->type == TOK_KW_VOID) {
	    expr->type = TOK_KW_VOID;
	} else {
	    if (expr->e2->type != expr->e3->type) {
		ick_error(ctx, expr->op2pos, "expected same type of "
			  "expression before and after ':'; found %T and %T",
			  expr->e2->type, expr->e3->type);
		return 0;
	    }
	    expr->type = expr->e2->type;
	}
	break;
      case TOK_ASSIGN:
	/*
	 * Types must match, and type of result is the same. Can't
	 * be void, because we've already enforced that e1 is an
	 * lvalue, and lvalues correspond to variable
	 * declarations, which we enforced at parse time to be
	 * non-void.
	 */
	assert(expr->e1->type != TOK_KW_VOID);
	if (expr->e1->type != expr->e2->type) {
	    ick_error(ctx, expr->oppos, "expected same type of expression "
		      "before and after %p operator; found %T and %T",
		      expr->op, expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = expr->e1->type;
	break;
      case TOK_PLUSASSIGN:
	/*
	 * Types must be both integer or both string, and type of
	 * result is the same.
	 */
	if (expr->e1->type != expr->e2->type ||
	    (expr->e1->type != TOK_KW_INT &&
	     expr->e1->type != TOK_KW_STRING)) {
	    ick_error(ctx, expr->oppos, "expected two integer or two string "
		      "operands to %p operator; found %T and %T",
		      expr->op, expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = expr->e1->type;
	break;
      case TOK_MINUSASSIGN:
      case TOK_MULASSIGN:
      case TOK_DIVASSIGN:
	/*
	 * Types must both be integer, and type of result is the
	 * same.
	 */
	if (expr->e1->type != TOK_KW_INT || expr->e2->type != TOK_KW_INT) {
	    ick_error(ctx, expr->oppos, "expected integer operands to "
		      "%p operator; found %T and %T", expr->op,
		      expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = TOK_KW_INT;
	break;
      case TOK_LOGANDASSIGN:
      case TOK_LOGORASSIGN:
	/*
	 * Types must both be boolean, and type of result is the
	 * same.
	 */
	if (expr->e1->type != TOK_KW_BOOL || expr->e2->type != TOK_KW_BOOL) {
	    ick_error(ctx, expr->oppos, "expected boolean operands to "
		      "%p operator; found %T and %T", expr->op,
		      expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = TOK_KW_BOOL;
	break;
      case TOK_LOGAND:
      case TOK_LOGOR:
      case TOK_LOGNOT:
	/*
	 * Types must both be boolean, and type of result is also
	 * boolean.
	 */
	if (expr->e1->type != TOK_KW_BOOL ||
	    (expr->e2 && expr->e2->type != TOK_KW_BOOL)) {
	    if (expr->e2) {
		ick_error(ctx, expr->oppos, "expected boolean operands to "
			  "%p operator; found %T and %T", expr->op,
			  expr->e1->type, expr->e2->type);
	    } else {
		ick_error(ctx, expr->oppos, "expected boolean operand to "
			  "%p operator; found %T", expr->op, expr->e1->type);
	    }
	    return 0;
	}
	expr->type = TOK_KW_BOOL;
	break;
      case TOK_LT:
      case TOK_LE:
      case TOK_GT:
      case TOK_GE:
	/*
	 * Types must both be integer or string, and type of result is
	 * boolean.
	 */
	if (expr->e1->type != expr->e2->type ||
	    (expr->e1->type != TOK_KW_INT &&
	     expr->e1->type != TOK_KW_STRING)) {
	    ick_error(ctx, expr->oppos, "expected two integer or"
		      " two string operands to %p operator; found %T and %T",
		      expr->op, expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = TOK_KW_BOOL;
	break;
      case TOK_EQ:
      case TOK_NE:
	/*
	 * Types must match and not be void, and type of result is
	 * boolean.
	 */
	if (expr->e1->type != expr->e2->type) {
	    ick_error(ctx, expr->oppos, "expected same type of operands to "
		      "%p operator; found %T and %T", expr->op,
		      expr->e1->type, expr->e2->type);
	    return 0;
	}
	if (expr->e1->type == TOK_KW_VOID) {
	    ick_error(ctx, expr->oppos, "expected non-void operands to "
		      "%p operator", expr->op);
	    return 0;
	}
	expr->type = TOK_KW_BOOL;
	break;
      case TOK_PLUS:
	/*
	 * Unary: type must be integer, return type is integer.
	 * 
	 * Binary: types must be either both int or both string,
	 * return type is the same.
	 */
	if (!expr->e2) {
	    if (expr->e1->type != TOK_KW_INT) {
		ick_error(ctx, expr->oppos, "expected integer operand to "
			  "unary '+' operator; found %T", expr->e1->type);
		return 0;
	    }
	    expr->type = TOK_KW_INT;
	} else {
	    if (expr->e1->type != expr->e2->type ||
		(expr->e1->type != TOK_KW_INT &&
		 expr->e1->type != TOK_KW_STRING)) {
		ick_error(ctx, expr->oppos, "expected two integer or two"
			  " string operands to '+' operator; found %T and %T",
			  expr->e1->type, expr->e2->type);
		return 0;
	    }
	    expr->type = expr->e1->type;
	}
	break;
      case TOK_MINUS:
      case TOK_INCREMENT:
      case TOK_DECREMENT:
	/*
	 * Types must be integer (whether there's one or two of
	 * them), and result is also integer.
	 */
	if ((expr->e1 && expr->e1->type != TOK_KW_INT) ||
	    (expr->e2 && expr->e2->type != TOK_KW_INT)) {
	    if (expr->e1 && expr->e2) {
		ick_error(ctx, expr->oppos, "expected integer operands to "
			  "%p operator; found %T and %T", expr->op,
			  expr->e1->type, expr->e2->type);
	    } else {
		ick_error(ctx, expr->oppos, "expected integer operand to "
			  "%p operator; found %T", expr->op,
			  expr->e1 ? expr->e1->type : expr->e2->type);
	    }
	    return 0;
	}
	expr->type = TOK_KW_INT;
	break;
      case TOK_MUL:
      case TOK_DIV:
	/*
	 * Types must both be integer, and type of result is the
	 * same.
	 */
	if (expr->e1->type != TOK_KW_INT || expr->e2->type != TOK_KW_INT) {
	    ick_error(ctx, expr->oppos, "expected integer operands to "
		      "arithmetic operator; found %T and %T",
		      expr->e1->type, expr->e2->type);
	    return 0;
	}
	expr->type = TOK_KW_INT;
	break;
      case TOK_STRINGLIT:
	/*
	 * No input constraint. Type is string.
	 */
	expr->type = TOK_KW_STRING;
	break;
      case TOK_INTLIT:
	/*
	 * No input constraint. Type is integer.
	 */
	expr->type = TOK_KW_INT;
	break;
      case TOK_BOOLLIT:
	/*
	 * No input constraint. Type is boolean.
	 */
	expr->type = TOK_KW_BOOL;
	break;
      case TOK_IDENTIFIER:
	/*
	 * No input constraint. Type is the type of the referenced
	 * variable.
	 */
	expr->type = expr->var->type;
	break;
      case TOK_LPAREN:
	/*
	 * No input constraint. Type is the return type of the
	 * referenced function.
	 */
	expr->type = expr->fn->rettype;
	break;
      case TOK_RPAREN:
	/*
	 * No action at all; this is a dummy node.
	 */
	break;
    }

    return 1;
}

static int ick_semantic_block(struct ick_parse_ctx *ctx,
			      struct ick_fndecl *fn,
			      struct ick_block *blk);

static int ick_semantic_statement(struct ick_parse_ctx *ctx,
				  struct ick_fndecl *fn,
				  struct ick_statement *stmt)
{
    struct ick_statement *savedloop;

    if (!stmt)
	return 1;

    /*
     * Track the innermost loop, so we know what break and
     * continue do.
     */
    if (stmt->type == TOK_KW_WHILE ||
	stmt->type == TOK_KW_DO ||
	stmt->type == TOK_KW_FOR) {
	savedloop = ctx->currloop;
	ctx->currloop = stmt;
    } else
	savedloop = NULL;

    if (!ick_semantic_expr(ctx, stmt->e1) ||
	!ick_semantic_expr(ctx, stmt->e2) ||
	!ick_semantic_expr(ctx, stmt->e3) ||
	!ick_semantic_statement(ctx, fn, stmt->s1) ||
	!ick_semantic_statement(ctx, fn, stmt->s2) ||
	!ick_semantic_block(ctx, fn, stmt->blk))
	return 0;

    /*
     * Restore the previous value of currloop.
     */
    if (stmt->type == TOK_KW_WHILE ||
	stmt->type == TOK_KW_DO ||
	stmt->type == TOK_KW_FOR) {
	ctx->currloop = savedloop;
    }

    /*
     * Identify break and continue statements with their innermost
     * loop.
     */
    if (stmt->type == TOK_KW_BREAK || stmt->type == TOK_KW_CONTINUE) {
	if (ctx->currloop) {
	    stmt->auxs1 = ctx->currloop;
	} else {
	    ick_error(ctx, stmt->startpos, "expected %p "
		      "to occur inside a loop", stmt->type);
	    return 0;
	}
    }

    /*
     * Type-check the expressions used in compound statements.
     */
    switch (stmt->type) {
      case TOK_KW_RETURN:
	if (fn->rettype == TOK_KW_VOID) {
	    if (stmt->e1) {
		ick_error(ctx, stmt->startpos, "expected no expression after "
			  "'return' from void function");
		return 0;
	    }
	} else {
	    if (!stmt->e1) {
		ick_error(ctx, stmt->startpos, "expected expression after "
			  "'return' from non-void function");
		return 0;
	    }
	    if (stmt->e1->type != fn->rettype) {
		ick_error(ctx, stmt->e1->startpos, "expected expression after "
			  "'return' to match function return type of %T;"
			  " found %T", fn->rettype, stmt->e1->type);
		return 0;
	    }
	}
	break;
      case TOK_KW_WHILE:
      case TOK_KW_DO:
	if (stmt->e1->type != TOK_KW_BOOL) {
	    ick_error(ctx, stmt->e1->startpos,
		      "expected boolean expression after 'while'; found %T",
		      stmt->e1->type);
	    return 0;
	}
	break;
      case TOK_KW_IF:
	if (stmt->e1->type != TOK_KW_BOOL) {
	    ick_error(ctx, stmt->e1->startpos,
		      "expected boolean expression after 'if'; found %T",
		      stmt->e1->type);
	    return 0;
	}
	break;
      case TOK_KW_FOR:
	if (stmt->e2 && stmt->e2->type != TOK_KW_BOOL) {
	    ick_error(ctx, stmt->e2->startpos, "expected boolean expression"
		      " as 'for' termination condition; found %T",
		      stmt->e2->type);
	    return 0;
	}
	break;
    }

    return 1;
}

static int ick_semantic_vardecls(struct ick_parse_ctx *ctx,
				 struct ick_vardecls *vars)
{
    int i, n = ctx->nvarscope + vars->ndecls;

    if (ctx->varscopesize < n) {
	ctx->varscopesize = n * 3 / 2 + 32;
	ctx->varscope = sresize(ctx->varscope, ctx->varscopesize,
				struct ick_vardecl *);
    }

    for (i = 0; i < vars->ndecls; i++) {
	/*
	 * Process the initialiser for each variable _before_
	 * bringing that variable into scope. That way we can
	 * shadow a variable and initialise it to something based
	 * on its previous value:
	 * 
	 *    int x = ...;
	 *    // do stuff with x
	 *    {
	 *       int x = x+1;
	 *       // do stuff with modified x
	 *    }
	 * 
	 * which would be pointless if we brought x into scope
	 * first.
	 */
	if (!ick_semantic_expr(ctx, vars->decls[i].initialiser))
	    return 0;

	ctx->varscope[ctx->nvarscope++] = &vars->decls[i];
    }

    return 1;
}

static int ick_semantic_block(struct ick_parse_ctx *ctx,
			      struct ick_fndecl *fn,
			      struct ick_block *blk)
{
    int i, saved_nvarscope;

    if (!blk)
	return 1;

    /*
     * Bring the block's local variables into scope.
     */
    saved_nvarscope = ctx->nvarscope;
    ick_semantic_vardecls(ctx, &blk->vars);

    for (i = 0; i < blk->nstmts; i++)
	if (!ick_semantic_statement(ctx, fn, blk->stmts[i]))
	    return 0;

    /*
     * Take the block's local variables back out of scope.
     */
    ctx->nvarscope = saved_nvarscope;

    return 1;
}

/*
 * Main semantic analysis function. Returns zero if an error was
 * encountered, or nonzero on success.
 */
static int ick_semantic_analysis(struct ick_parse_ctx *ctx,
				 const char *mainfuncname,
				 const char *mainfuncproto)
{
    int i, j;
    int ret = 1;
    int saved_nvarscope;
    struct ick_parse_tree *tree = ctx->tree;

    /*
     * First of all, find the script's main function, and swap it
     * to the front of the fndecl list.
     */
    for (i = 0; i < tree->nfns; i++) {
	struct ick_fndecl *fn = &tree->fns[i];

	/*
	 * We distinguish functions by name and parameter types,
	 * but the return type is fixed given those. So if we find
	 * a function with the right name but wrong parameters, we
	 * ignore it and keep looking; but a function with the
	 * right name, right parameters and wrong return type is
	 * an error.
	 */
	if (strcmp(mainfuncname, fn->identifier))
	    continue;		       /* it's not this one */
	for (j = 0; j < fn->params.ndecls; j++) {
	    int reqtype = CHAR_TO_TYPE(mainfuncproto[j+1]);
	    /* reqtype can be -1 if we hit EOS, of course */
	    if (reqtype != fn->params.decls[j].type)
		break;
	}
	if (j < fn->params.ndecls || mainfuncproto[j+1])
	    continue;		       /* types didn't match */

	if (fn->rettype != CHAR_TO_TYPE(mainfuncproto[0])) {
	    ick_error(ctx, fn->startpos, "expected return type of %T from"
		      " main function; found %T",
		      CHAR_TO_TYPE(mainfuncproto[0]), fn->rettype);
	    return 0;
	}

	/*
	 * We've found it.
	 */
	break;
    }
    if (i == tree->nfns) {
	/*
	 * Line and column: very end of script, i.e. the TOK_EOF!
	 */
	ick_error(ctx, ctx->tok.pos, "expected definition of main function "
		  "'%b %s(%u)'", CHAR_TO_TYPE(mainfuncproto[0]), mainfuncname,
		  mainfuncproto);
	return 0;
    }
    if (i > 0) {
	/* If main function isn't first, swap it into place. */
	struct ick_fndecl tmp;
	tmp = tree->fns[0];
	tree->fns[0] = tree->fns[i];
	tree->fns[i] = tmp;
    }

    ctx->varscope = NULL;
    ctx->nvarscope = ctx->varscopesize = 0;
    ick_semantic_vardecls(ctx, &tree->toplevel_vars);

    for (i = 0; i < tree->nfns; i++) {
	struct ick_fndecl *fn = &tree->fns[i];

	/*
	 * Bring the function's parameters into scope.
	 */
	saved_nvarscope = ctx->nvarscope;
	ick_semantic_vardecls(ctx, &fn->params);

	/*
	 * Process the function body.
	 */
	ctx->currloop = NULL;
	if (!ick_semantic_block(ctx, fn, fn->defn)) {
	    ret = 0;
	    break;
	}

	/*
	 * Take the parameters back out of scope.
	 */
	ctx->nvarscope = saved_nvarscope;
    }

    sfree(ctx->varscope);

    return ret;
}

/* ----------------------------------------------------------------------
 * Code generation for the Ick virtual machine.
 */

/*
 * Defines the VM opcodes.
 */
#define OPCODES(X) \
    X(MOVESP, "", "", NULL), \
    X(JMP, "p", NULL, NULL), \
    X(JMPZ, "i", "p", NULL), \
    X(JMPNZ, "i", "p", NULL), \
    X(JMPI, "i", NULL, NULL), \
    X(SCONST, "s", "l", NULL), \
    X(ICONST, "i", "", NULL), \
    X(IMOV, "i", "i", NULL), \
    X(SMOV, "s", "s", NULL), \
    X(ADD, "i", "i", "i"), \
    X(SUB, "i", "i", "i"), \
    X(MUL, "i", "i", "i"), \
    X(DIV, "i", "i", "i"), \
    X(CONCAT, "s", "s", "s"), \
    X(ICMP, "i", "i", "i"), \
    X(SCMP, "i", "s", "s"), \
    X(LE0, "i", "i", NULL), \
    X(LT0, "i", "i", NULL), \
    X(GE0, "i", "i", NULL), \
    X(GT0, "i", "i", NULL), \
    X(NE0, "i", "i", NULL), \
    X(EQ0, "i", "i", NULL)

#define OPCODE_ENUM(x,a,b,c) OP_ ## x
enum { OPCODES(OPCODE_ENUM) };

/*
 * This code generator employs the convention on stack frames that
 * they are fixed-size within a function. That is, a stack frame
 * is established on function entry by a single MOVESP, and
 * restored by an equal and opposite MOVESP just before returning.
 * That stack frame must contain space for all the local variables
 * in use; all the temporary slots needed during expression
 * evaluation; and space to store the arguments, return values and
 * return addresses just below the stack pointers for any
 * subfunctions called.
 * 
 * (This gets a bit fiddly if function calls are required during
 * evaluation of the arguments to another function call. The
 * obvious code generation strategy for a function call is to
 * compute the arguments one by one and stick them just below the
 * stack pointers as they're generated, then load the return
 * address and jump to the function; but if another function call
 * is required half way through that process, then it will
 * overwrite all the arguments written so far. So we must detect
 * this and generate the function call a different way, by storing
 * arguments in temporaries elsewhere in the stack frame and only
 * moving them to the function argument zone at the last minute.)
 */

static void ick_emit(struct ick_parse_ctx *ctx,
		     int opcode, int p1, int p2, int p3)
{
    if (ctx->ninsns >= ctx->insnsize) {
	ctx->insnsize = ctx->ninsns * 3 / 2 + 128;
	ctx->insns = sresize(ctx->insns, ctx->insnsize, struct ick_insn);
    }

    ctx->insns[ctx->ninsns].opcode = opcode;
    ctx->insns[ctx->ninsns].p1 = p1;
    ctx->insns[ctx->ninsns].p2 = p2;
    ctx->insns[ctx->ninsns].p3 = p3;
    ctx->ninsns++;
}

static int ick_string_literal(struct ick_parse_ctx *ctx, const char *str)
{
    int i;

    for (i = 0; i < ctx->nstringlits; i++)
	if (!strcmp(str, ctx->stringlits[i]))
	    return i;

    if (ctx->nstringlits >= ctx->stringlitsize) {
	ctx->stringlitsize = ctx->nstringlits * 3 / 2 + 32;
	ctx->stringlits = sresize(ctx->stringlits, ctx->stringlitsize,
				  const char *);
    }

    /*
     * We don't need to dynamically allocate the strings in here,
     * because they all come from either the parse tree (which is
     * kept alongside the VM code) or real C string literals in
     * this file.
     */
    i = ctx->nstringlits++;
    ctx->stringlits[i] = str;
    return i;
}

/*
 * In code generation we'll be noting down a lot of values which
 * we expect not to have changed in the second pass. So here's a
 * convenience macro which sets such a value in pass one, and
 * verifies it by assertion in pass two.
 */
#define SET(var,value) do { \
    if (ctx->pass == 0) { \
	(var) = (value); \
    } else { \
	assert((var) == (value)); \
    } \
} while (0)

/*
 * Special value passed to ick_codegen_expr, indicating that the
 * result of the expression is not actually needed. (This allows
 * some optimisation during code gen, but the whole expression
 * must still be trawled for function calls since they may have
 * side effects. In particular, TOK_QUERY nodes must still be
 * evaluated in full.)
 */
#define ADDR_VOID (INT_MAX)

static void ick_chkstk(struct ick_parse_ctx *ctx)
{
    /*
     * After increasing any of {i,s}stk{up,dn}, we call this
     * trivial function to ensure the stkmax values are correct.
     */
    if (ctx->istkmax < ctx->istkup + ctx->istkdn)
	ctx->istkmax = ctx->istkup + ctx->istkdn;
    if (ctx->sstkmax < ctx->sstkup + ctx->sstkdn)
	ctx->sstkmax = ctx->sstkup + ctx->sstkdn;
}

#define OPTYPE(op,type) (((op)<<8) + (type))

static void ick_codegen_expr(struct ick_parse_ctx *ctx,
			     struct ick_expr *expr, int target)
{
    int temp, temp2, opcode, optype, i, spbase, ipbase;
    struct ick_expr *arg;

    /*
     * Generate code to evaluate an expression.
     */

    optype = OPTYPE(expr->op, expr->type);

    switch (optype) {
      case OPTYPE(TOK_COMMA, TOK_KW_STRING):
      case OPTYPE(TOK_COMMA, TOK_KW_INT):
      case OPTYPE(TOK_COMMA, TOK_KW_BOOL):
      case OPTYPE(TOK_COMMA, TOK_KW_VOID):
	/*
	 * Comma expression: we just evaluate e1 to void, then e2
	 * to our target.
	 */
	ick_codegen_expr(ctx, expr->e1, ADDR_VOID);
	ick_codegen_expr(ctx, expr->e2, target);
	break;

      case OPTYPE(TOK_QUERY, TOK_KW_STRING):
      case OPTYPE(TOK_QUERY, TOK_KW_INT):
      case OPTYPE(TOK_QUERY, TOK_KW_BOOL):
      case OPTYPE(TOK_QUERY, TOK_KW_VOID):
	/*
	 * A query expression looks like this:
	 * 
	 *          test e1
	 *          jump if zero to e3addr
	 *          evaluate e2 to target
	 *          jump to endaddr
	 * e3addr:  evaluate e3 to target
	 * endaddr:
	 */
	temp = ctx->istkup++ - ctx->iframe;
	ick_chkstk(ctx);
	ick_codegen_expr(ctx, expr->e1, temp);
	ick_emit(ctx, OP_JMPZ, temp, expr->e3addr, 0);
	ctx->istkup--;
	ick_codegen_expr(ctx, expr->e2, target);
	ick_emit(ctx, OP_JMP, expr->endaddr, 0, 0);
	SET(expr->e3addr, ctx->ninsns);
	ick_codegen_expr(ctx, expr->e3, target);
	SET(expr->endaddr, ctx->ninsns);
	break;

      case OPTYPE(TOK_LOGAND, TOK_KW_BOOL):
      case OPTYPE(TOK_LOGOR, TOK_KW_BOOL):
	/*
	 * Logical AND: we evaluate e1 to the target (or a
	 * temporary if we're going to void), then if it's
	 * non-zero we do the same with e2.
	 * 
	 * Logical OR: same, only `if zero'.
	 */
	temp = ctx->istkup++ - ctx->iframe;
	ick_chkstk(ctx);
	ick_codegen_expr(ctx, expr->e1, temp);
	ick_emit(ctx, (expr->op == TOK_LOGAND ? OP_JMPZ : OP_JMPNZ),
		 temp, expr->endaddr, 0);
	ick_codegen_expr(ctx, expr->e2, temp);
	SET(expr->endaddr, ctx->ninsns);
	ick_emit(ctx, OP_IMOV, target, temp, 0);
	ctx->istkup--;
	break;

      case OPTYPE(TOK_ASSIGN, TOK_KW_STRING):
      case OPTYPE(TOK_ASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_ASSIGN, TOK_KW_BOOL):
	/*
	 * Assignment: just evaluate e2 to our lvalue destination,
	 * then copy that to the target if we have one.
	 */
	ick_codegen_expr(ctx, expr->e2, expr->var->address);
	if (target != ADDR_VOID) {
	    ick_emit(ctx, (expr->type == TOK_KW_STRING ? OP_SMOV : OP_IMOV),
		     target, expr->var->address, 0);
	}
	break;

      case OPTYPE(TOK_PLUSASSIGN, TOK_KW_STRING):
      case OPTYPE(TOK_PLUSASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_MINUSASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_MULASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_DIVASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_LOGORASSIGN, TOK_KW_BOOL):
      case OPTYPE(TOK_LOGANDASSIGN, TOK_KW_BOOL):
	/*
	 * Assignment with arithmetic: evaluate e2 to a temporary,
	 * then combine with our lvalue destination, then copy
	 * that to the target if we have one.
	 */
	if (expr->type == TOK_KW_STRING)
	    temp = ctx->sstkup++ - ctx->sframe;
	else
	    temp = ctx->istkup++ - ctx->iframe;
	ick_chkstk(ctx);
	ick_codegen_expr(ctx, expr->e2, temp);
	switch (OPTYPE(expr->op, expr->type)) {
	  case OPTYPE(TOK_PLUSASSIGN, TOK_KW_STRING):
	    ick_emit(ctx, OP_CONCAT, expr->var->address,
		     expr->var->address, temp);
	    break;
	  case OPTYPE(TOK_PLUSASSIGN, TOK_KW_INT):
	    ick_emit(ctx, OP_ADD, expr->var->address,
		     expr->var->address, temp);
	    break;
	  case OPTYPE(TOK_MINUSASSIGN, TOK_KW_INT):
	    ick_emit(ctx, OP_SUB, expr->var->address,
		     expr->var->address, temp);
	    break;
	  case OPTYPE(TOK_MULASSIGN, TOK_KW_INT):
	  case OPTYPE(TOK_LOGANDASSIGN, TOK_KW_BOOL):
	    /*
	     * All booleans are strictly 0 or 1, thanks to our
	     * careful type checking, so we can implement logical
	     * AND using simple multiplication.
	     */
	    ick_emit(ctx, OP_MUL, expr->var->address,
		     expr->var->address, temp);
	    break;
	  case OPTYPE(TOK_DIVASSIGN, TOK_KW_INT):
	    ick_emit(ctx, OP_DIV, expr->var->address,
		     expr->var->address, temp);
	    break;
	  case OPTYPE(TOK_LOGORASSIGN, TOK_KW_BOOL):
	    /*
	     * Logical OR is marginally more annoying: we have to
	     * add, then renormalise by comparing against zero.
	     */
	    ick_emit(ctx, OP_ADD, expr->var->address,
		     expr->var->address, temp);
	    ick_emit(ctx, OP_NE0, expr->var->address, expr->var->address, 0);
	    break;
	  default:
	    assert(!"We shouldn't get here");
	}
	if (expr->type == TOK_KW_STRING)
	    ctx->sstkup--;
	else
	    ctx->istkup--;
	if (target != ADDR_VOID) {
	    ick_emit(ctx, (expr->type == TOK_KW_STRING ? OP_SMOV : OP_IMOV),
		     target, expr->var->address, 0);
	}
	break;

      case OPTYPE(TOK_LT, TOK_KW_BOOL):
      case OPTYPE(TOK_LE, TOK_KW_BOOL):
      case OPTYPE(TOK_GT, TOK_KW_BOOL):
      case OPTYPE(TOK_GE, TOK_KW_BOOL):
      case OPTYPE(TOK_EQ, TOK_KW_BOOL):
      case OPTYPE(TOK_NE, TOK_KW_BOOL):
	if (target == ADDR_VOID) {
	    /*
	     * If we're evaluating to void, simply evaluate both
	     * operands to void in case of side effects.
	     */
	    ick_codegen_expr(ctx, expr->e1, ADDR_VOID);
	    ick_codegen_expr(ctx, expr->e2, ADDR_VOID);
	} else {
	    /*
	     * Otherwise, evaluate to temporaries.
	     */
	    if (expr->e1->type == TOK_KW_STRING) {
		temp = ctx->sstkup++ - ctx->sframe;
	    } else {
		temp = ctx->istkup++ - ctx->iframe;
	    }
	    ick_chkstk(ctx);

	    ick_codegen_expr(ctx, expr->e1, temp);

	    if (expr->e1->type == TOK_KW_STRING) {
		temp2 = ctx->sstkup++ - ctx->sframe;
	    } else {
		temp2 = ctx->istkup++ - ctx->iframe;
	    }
	    ick_chkstk(ctx);

	    ick_codegen_expr(ctx, expr->e2, temp2);

	    ick_emit(ctx, expr->e1->type == TOK_KW_STRING ? OP_SCMP : OP_ICMP,
		     target, temp, temp2);

	    if (expr->e1->type == TOK_KW_STRING) {
		ctx->sstkup -= 2;
	    } else {
		ctx->istkup -= 2;
	    }

	    switch (expr->op) {
	      case TOK_LT: opcode = OP_LT0; break;
	      case TOK_LE: opcode = OP_LE0; break;
	      case TOK_GT: opcode = OP_GT0; break;
	      case TOK_GE: opcode = OP_GE0; break;
	      case TOK_EQ: opcode = OP_EQ0; break;
	      default /*case TOK_NE*/: opcode = OP_NE0; break;
	    }
	    ick_emit(ctx, opcode, target, target, 0);
	}
	break;

      case OPTYPE(TOK_PLUS, TOK_KW_STRING):
	/*
	 * String binary operation.
	 */
	if (target == ADDR_VOID) {
	    /*
	     * If we're evaluating to void, simply evaluate both
	     * operands to void in case of side effects.
	     */
	    ick_codegen_expr(ctx, expr->e1, ADDR_VOID);
	    ick_codegen_expr(ctx, expr->e2, ADDR_VOID);
	} else {
	    /*
	     * Otherwise, use one temporary.
	     */
	    temp = ctx->sstkup++ - ctx->sframe;
	    ick_chkstk(ctx);
	    ick_codegen_expr(ctx, expr->e1, temp);
	    ick_codegen_expr(ctx, expr->e2, target);
	    ick_emit(ctx, OP_CONCAT, target, temp, target);
	    ctx->sstkup--;
	}
	break;

      case OPTYPE(TOK_PLUS, TOK_KW_INT):
      case OPTYPE(TOK_MINUS, TOK_KW_INT):
      case OPTYPE(TOK_MUL, TOK_KW_INT):
      case OPTYPE(TOK_DIV, TOK_KW_INT):
      case OPTYPE(TOK_LOGNOT, TOK_KW_BOOL):
	/*
	 * Integer unary or binary operation.
	 */
	if (target == ADDR_VOID) {
	    /*
	     * If we're evaluating to void, simply evaluate both
	     * operands to void in case of side effects.
	     */
	    ick_codegen_expr(ctx, expr->e1, ADDR_VOID);
	    if (expr->e2)
		ick_codegen_expr(ctx, expr->e2, ADDR_VOID);
	} else {
	    /*
	     * Otherwise, evaluate in full.
	     */
	    if (expr->e2) {
		temp = ctx->istkup++ - ctx->iframe;
		ick_chkstk(ctx);
		ick_codegen_expr(ctx, expr->e1, temp);
		ick_codegen_expr(ctx, expr->e2, target);
		switch (expr->op) {
		  case TOK_PLUS: opcode = OP_ADD; break;
		  case TOK_MINUS: opcode = OP_SUB; break;
		  case TOK_MUL: opcode = OP_MUL; break;
		  default /*case TOK_DIV*/: opcode = OP_DIV; break;
		}
		ick_emit(ctx, opcode, target, temp, target);
		ctx->istkup--;
	    } else {
		ick_codegen_expr(ctx, expr->e1, target);
		switch (expr->op) {
		  case TOK_PLUS:
		    /* unary plus is a no-op */
		    break;
		  case TOK_LOGNOT:
		    /* logical NOT is neatly implemented by OP_EQ0 */
		    ick_emit(ctx, OP_EQ0, target, target, 0);
		    break;
		  case TOK_MINUS:
		    /* for negation we subtract from zero */
		    temp = ctx->istkup++ - ctx->iframe;
		    ick_chkstk(ctx);
		    ick_emit(ctx, OP_ICONST, temp, 0, 0);
		    ick_emit(ctx, OP_SUB, target, temp, target);
		    ctx->istkup--;
		    break;
		}
	    }
	}
	break;

      case OPTYPE(TOK_INCREMENT, TOK_KW_INT):
      case OPTYPE(TOK_DECREMENT, TOK_KW_INT):
	/*
	 * {Pre,post}-{increment,decrement}.
	 */
	if (target != ADDR_VOID && expr->e2)
	    ick_emit(ctx, OP_IMOV, target, expr->var->address, 0);
	temp = ctx->istkup++ - ctx->iframe;
	ick_chkstk(ctx);
	ick_emit(ctx, OP_ICONST, temp, 1, 0);
	ick_emit(ctx, (expr->op == TOK_INCREMENT ? OP_ADD : OP_SUB),
		 expr->var->address, expr->var->address, temp);
	ctx->istkup--;
	if (target != ADDR_VOID && expr->e1)
	    ick_emit(ctx, OP_IMOV, target, expr->var->address, 0);
	break;

      case OPTYPE(TOK_STRINGLIT, TOK_KW_STRING):
	/*
	 * String literal.
	 */
	if (target != ADDR_VOID)
	    ick_emit(ctx, OP_SCONST, target,
		     ick_string_literal(ctx, expr->sval), 0);
	break;

      case OPTYPE(TOK_INTLIT, TOK_KW_INT):
      case OPTYPE(TOK_BOOLLIT, TOK_KW_BOOL):
	/*
	 * Integer and boolean literal.
	 */
	if (target != ADDR_VOID)
	    ick_emit(ctx, OP_ICONST, target, expr->ival, 0);
	break;

      case OPTYPE(TOK_IDENTIFIER, TOK_KW_STRING):
      case OPTYPE(TOK_IDENTIFIER, TOK_KW_INT):
      case OPTYPE(TOK_IDENTIFIER, TOK_KW_BOOL):
	/*
	 * Variable reference.
	 */
	if (target != ADDR_VOID)
	    ick_emit(ctx, (expr->type == TOK_KW_STRING ? OP_SMOV : OP_IMOV),
		     target, expr->var->address, 0);
	break;

      case OPTYPE(TOK_LPAREN, TOK_KW_STRING):
      case OPTYPE(TOK_LPAREN, TOK_KW_INT):
      case OPTYPE(TOK_LPAREN, TOK_KW_BOOL):
      case OPTYPE(TOK_LPAREN, TOK_KW_VOID):
	/*
	 * Function call. Load the top of our stack frame with
	 * return address, followed by the arguments in order,
	 * then jump to the function address.
	 * 
	 * We have a choice of two strategies here. We can
	 * generate all the argument values straight into their
	 * final positions, or we can generate them into temps and
	 * load them all at the last minute. The former is safe to
	 * do if none of the argument expressions contains a
	 * function call in turn; otherwise we fall back to the
	 * latter.
	 */
	assert(ctx->istkdn == 0 && ctx->sstkdn == 0);

	ipbase = ctx->istkup;
	spbase = ctx->sstkup;

	if (expr->e2 && expr->e2->fnbelow) {
	    /*
	     * Indirect approach: generate the arguments in
	     * temps.
	     */
	    arg = expr->e2;
	    for (i = 0; i < expr->fn->params.ndecls; i++) {
		int argaddr;
		if (expr->fn->params.decls[i].type == TOK_KW_STRING)
		    argaddr = ctx->sstkup++ - ctx->sframe;
		else
		    argaddr = ctx->istkup++ - ctx->iframe;
		ick_chkstk(ctx);
		assert(arg && arg->e1);
		ick_codegen_expr(ctx, arg->e1, argaddr);
		arg = arg->e2;
	    }
	    assert(!arg);
	}

	/*
	 * Now begin constructing the function call itself.
	 */
	ctx->istkdn++; ick_chkstk(ctx);
	if (expr->e2 && expr->e2->fnbelow) {
	    /*
	     * Indirect mode. Transfer in the arguments from their
	     * temporary homes.
	     */
	    for (i = ipbase; i < ctx->istkup; i++) {
		ick_emit(ctx, OP_IMOV, -(++ctx->istkdn), i - ctx->iframe, 0);
		ick_chkstk(ctx);
	    }
	    for (i = spbase; i < ctx->sstkup; i++) {
		ick_emit(ctx, OP_SMOV, -(++ctx->sstkdn), i - ctx->sframe, 0);
		ick_chkstk(ctx);
	    }
	    ctx->istkup = ipbase;
	    ctx->sstkup = spbase;
	} else {
	    /*
	     * Direct mode. Generate the arguments directly on the
	     * stack.
	     */
	    arg = expr->e2;
	    for (i = 0; i < expr->fn->params.ndecls; i++) {
		int argaddr;
		if (expr->fn->params.decls[i].type == TOK_KW_STRING)
		    argaddr = -(++ctx->sstkdn);
		else
		    argaddr = -(++ctx->istkdn);
		ick_chkstk(ctx);
		assert(arg && arg->e1);
		ick_codegen_expr(ctx, arg->e1, argaddr);
		arg = arg->e2;
	    }
	    assert(!arg);
	}
	/*
	 * Make sure we've reserved space on the stack for the
	 * return value (even if we're ignoring it).
	 */
	if (expr->type == TOK_KW_STRING && ctx->sstkdn < 1)
	    ctx->sstkdn = 1;
	else if (expr->type != TOK_KW_VOID && ctx->istkdn < 2)
	    ctx->istkdn = 2;
	ick_chkstk(ctx);

	/*
	 * Set up the return address immediately before making the
	 * call. We could just as easily have done it before
	 * setting up the arguments, but it's nicer this way for
	 * humans reading the VM code.
	 */
	ick_emit(ctx, OP_ICONST, -1, expr->endaddr, 0);
	ick_emit(ctx, OP_JMP, expr->fn->address, 0, 0);
	SET(expr->endaddr, ctx->ninsns);
	/*
	 * And finally, copy the return value to where we want it.
	 */
	if (target != ADDR_VOID) {
	    assert(expr->type != TOK_KW_VOID);
	    ick_emit(ctx, (expr->type == TOK_KW_STRING ? OP_SMOV : OP_IMOV),
		     target, (expr->type == TOK_KW_STRING ? -1 : -2), 0);

	}
	ctx->istkdn = ctx->sstkdn = 0;
	break;

      default:
	assert(!"We shouldn't get here");
	break;
    }
}

static void ick_codegen_block(struct ick_parse_ctx *ctx,
			      struct ick_block *blk);

static void ick_codegen_statement(struct ick_parse_ctx *ctx,
				  struct ick_statement *stmt)
{
    int temp;

    /*
     * Generate code to execute a single statement. There's no
     * boilerplate or preparatory work left by now: we just dive
     * in and do something appropriate depending on the statement
     * type.
     */
    switch (stmt->type) {
      case TOK_KW_RETURN:
	/*
	 * Return statement. If it has an expression, place that
	 * just below our stack frame. in i-2 or s-1 as appropriate.
	 */
	if (stmt->e1)
	    ick_codegen_expr(ctx, stmt->e1,
			     stmt->e1->type == TOK_KW_STRING ?
			     -1 - ctx->sframe : -2 - ctx->iframe);
	/*
	 * And then jump to the function's return point.
	 */
	ick_emit(ctx, OP_JMP, ctx->currfn->retpoint, 0, 0);
	break;
      case TOK_KW_BREAK:
	/*
	 * Break statement. Emit a jump to the break point of the
	 * current containing loop.
	 */
	ick_emit(ctx, OP_JMP, stmt->auxs1->breakpt, 0, 0);
	break;
      case TOK_KW_CONTINUE:
	/*
	 * Continue statement. Emit a jump to the continue point
	 * of the current containing loop.
	 */
	ick_emit(ctx, OP_JMP, stmt->auxs1->contpt, 0, 0);
	break;
      case TOK_KW_IF:
	/*
	 * An if statement without else clause has the form
	 * 
	 *            test condition e1
	 *            jump if false to breakpt
	 *            perform then clause s1
	 *   breakpt:
	 * 
	 * With an else clause, it looks like this:
	 * 
	 *            test condition e1
	 *            jump if false to breakpt
	 *            perform then clause s1
	 *            jump to contpt
	 *   breakpt: perform else clause s2
	 *   contpt:
	 */
	temp = ctx->istkup++ - ctx->iframe; ick_chkstk(ctx);
	ick_codegen_expr(ctx, stmt->e1, temp);
	ick_emit(ctx, OP_JMPZ, temp, stmt->breakpt, 0);
	ctx->istkup--;
	ick_codegen_statement(ctx, stmt->s1);
	if (stmt->s2)
	    ick_emit(ctx, OP_JMP, stmt->contpt, 0, 0);
	SET(stmt->breakpt, ctx->ninsns);
	if (stmt->s2) {
	    ick_codegen_statement(ctx, stmt->s2);
	    SET(stmt->contpt, ctx->ninsns);
	}
	break;
      case TOK_KW_WHILE:
	/*
	 * A while statement has the form
	 * 
	 *   contpt:  test condition e1
	 *            jump if false to breakpt
	 *            perform loop body s1
	 *            jump to contpt
	 *   breakpt:
	 */
	SET(stmt->contpt, ctx->ninsns);
	temp = ctx->istkup++ - ctx->iframe; ick_chkstk(ctx);
	ick_codegen_expr(ctx, stmt->e1, temp);
	ick_emit(ctx, OP_JMPZ, temp, stmt->breakpt, 0);
	ctx->istkup--;
	ick_codegen_statement(ctx, stmt->s1);
	ick_emit(ctx, OP_JMP, stmt->contpt, 0, 0);
	SET(stmt->breakpt, ctx->ninsns);
	break;
      case TOK_KW_DO:
	/*
	 * A do-while statement has the form
	 * 
	 *   looptop: perform loop body s1
	 *   contpt:  test condition e1
	 *            jump if true to looptop
	 *   breakpt:
	 */
	SET(stmt->looptop, ctx->ninsns);
	ick_codegen_statement(ctx, stmt->s1);
	SET(stmt->contpt, ctx->ninsns);
	temp = ctx->istkup++ - ctx->iframe; ick_chkstk(ctx);
	ick_codegen_expr(ctx, stmt->e1, temp);
	ick_emit(ctx, OP_JMPNZ, temp, stmt->looptop, 0);
	ctx->istkup--;
	SET(stmt->breakpt, ctx->ninsns);
	break;
      case TOK_KW_FOR:
	/*
	 * A for statement has the form
	 * 
	 *            perform initialisation e1 (eval to void)
	 *   looptop: test condition e2
	 *            jump if false to breakpt
	 *            perform loop body s1
	 *   contpt:  perform increment e3 (eval to void)
	 *            jump to looptop
	 *   breakpt:
	 */
	if (stmt->e1)
	    ick_codegen_expr(ctx, stmt->e1, ADDR_VOID);
	SET(stmt->looptop, ctx->ninsns);
	temp = ctx->istkup++ - ctx->iframe; ick_chkstk(ctx);
	if (stmt->e2) {
	    ick_codegen_expr(ctx, stmt->e2, temp);
	    ick_emit(ctx, OP_JMPZ, temp, stmt->breakpt, 0);
	}
	ctx->istkup--;
	ick_codegen_statement(ctx, stmt->s1);
	SET(stmt->contpt, ctx->ninsns);
	if (stmt->e3)
	    ick_codegen_expr(ctx, stmt->e3, ADDR_VOID);
	ick_emit(ctx, OP_JMP, stmt->looptop, 0, 0);
	SET(stmt->breakpt, ctx->ninsns);
	break;
      case TOK_LBRACE:
	ick_codegen_block(ctx, stmt->blk);
	break;
      case -1:
	/* Expression statement. Just evaluate the expression to void. */
	ick_codegen_expr(ctx, stmt->e1, ADDR_VOID);
	break;
      default:
	assert(!"We shouldn't get here");
	break;
    }
}

static void ick_alloc_vars(struct ick_parse_ctx *ctx,
			   struct ick_vardecls *vars, int *ipos, int *spos)
{
    int i;

    for (i = 0; i < vars->ndecls; i++) {
	int addr;

	if (vars->decls[i].type == TOK_KW_STRING)
	    addr = (*spos)++;
	else
	    addr = (*ipos)++;

	vars->decls[i].address = addr;
    }
}

static void ick_codegen_varinit(struct ick_parse_ctx *ctx,
				struct ick_vardecls *vars)
{
    int i;

    /*
     * Go through the variable declarations and write out
     * initialisers for them. Variables without an explicit
     * initialiser are implicitly initialised to 0/false/"".
     */

    for (i = 0; i < vars->ndecls; i++) {
	if (vars->decls[i].initialiser) {
	    /*
	     * We have an initialiser; run it.
	     */
	    ick_codegen_expr(ctx, vars->decls[i].initialiser,
			     vars->decls[i].address);
	} else {
	    /*
	     * No explicit initialiser: make something up based on
	     * the variable type.
	     */
	    switch (vars->decls[i].type) {
	      case TOK_KW_INT:
	      case TOK_KW_BOOL:
		/*
		 * Initialise ints to 0, and bools to false, which
		 * have the same VM representation.
		 */
		ick_emit(ctx, OP_ICONST, vars->decls[i].address, 0, 0);
		break;
	      case TOK_KW_STRING:
		/*
		 * Initialise strings to "".
		 */
		ick_emit(ctx, OP_SCONST, vars->decls[i].address,
			 ick_string_literal(ctx, ""), 0);
		break;
	    }
	}
    }
}

static void ick_codegen_block(struct ick_parse_ctx *ctx,
			      struct ick_block *blk)
{
    int saved_istkup, saved_sstkup, ipos, spos, i;

    /*
     * Code generation for a braced block.
     */

    /*
     * The block's variables come into scope here, so we allocate
     * space for them at the bottom of the stack frame.
     */
    /* Save the old stkup values. */
    saved_istkup = ctx->istkup;
    saved_sstkup = ctx->sstkup;
    /* Find the actual position (relative to stack top) where the vars
     * will end up. */
    ipos = ctx->istkup - ctx->iframe;
    spos = ctx->sstkup - ctx->sframe;
    /* Do the allocation. */
    ick_alloc_vars(ctx, &blk->vars, &ipos, &spos);
    /* Adjust stkup as a result, and check stkmax. */
    ctx->istkup = ipos + ctx->iframe;
    ctx->sstkup = spos + ctx->sframe;
    ick_chkstk(ctx);

    /*
     * Initialise the block's variables.
     */
    ick_codegen_varinit(ctx, &blk->vars);

    /*
     * Generate the code for the block's statements.
     */
    for (i = 0; i < blk->nstmts; i++)
	ick_codegen_statement(ctx, blk->stmts[i]);

    /*
     * Now the block's variables go out of scope, so restore stkup.
     */
    ctx->istkup = saved_istkup;
    ctx->sstkup = saved_sstkup;
}

static void ick_codegen_fn(struct ick_parse_ctx *ctx,
			   struct ick_fndecl *fn,
			   struct ick_vardecls *extravars)
{
    int i, ip, sp;

    /*
     * Top-level code generation for a single function.
     */

    /*
     * We start by noting down the function's start address, for
     * calls to it from other functions.
     */
    SET(fn->address, ctx->ninsns);

    /*
     * Now emit the function prologue and set up internal stack
     * frame allocation tracking.
     */
    ick_emit(ctx, OP_MOVESP, fn->istack, fn->sstack, 0);
    ctx->istkup = ctx->istkdn = ctx->istkmax = 0;
    ctx->sstkup = ctx->sstkdn = ctx->sstkmax = 0;
    ctx->iframe = fn->istack;
    ctx->sframe = fn->sstack;

    ctx->currfn = fn;

    /*
     * Assign addresses to the function parameters.
     */
    ip = -2;
    sp = -1;
    for (i = 0; i < fn->params.ndecls; i++) {
	if (fn->params.decls[i].type == TOK_KW_STRING)
	    fn->params.decls[i].address = sp-- - ctx->sframe;
	else
	    fn->params.decls[i].address = ip-- - ctx->iframe;
    }

    /*
     * Generate the initialisers for the extra variables, if any.
     */
    if (extravars)
	ick_codegen_varinit(ctx, extravars);

    /*
     * Generate the code for the function body.
     */
    ick_codegen_block(ctx, fn->defn);

    /*
     * Now we know what the function's overall stack frame size
     * is.
     */
    SET(fn->istack, ctx->istkmax);
    SET(fn->sstack, ctx->sstkmax);

    /*
     * Write the function epilogue, and save its address for
     * return statements internal to the function.
     */
    SET(fn->retpoint, ctx->ninsns);
    ick_emit(ctx, OP_MOVESP, -fn->istack, -fn->sstack, 0);
    ick_emit(ctx, OP_JMPI, -1, 0, 0);
}

static struct ickscript *ick_codegen(struct ick_parse_ctx *ctx)
{
    struct ickscript *scr;
    int i;

    scr = snew(struct ickscript);
    scr->tree = ctx->tree;

    ctx->insns = NULL;
    ctx->insnsize = 0;
    ctx->stringlits = NULL;
    ctx->nstringlits = ctx->stringlitsize = 0;

    /*
     * First order of business is to allocate stack locations for
     * the global variables, and store the resulting initial stack
     * sizes so that the execution engine can set up the starting
     * stack.
     */
    scr->ivars = scr->svars = 0;
    ick_alloc_vars(ctx, &ctx->tree->toplevel_vars, &scr->ivars, &scr->svars);

    /*
     * We make two code generation passes over the script. In the
     * first one, we generate a lot of wrong instructions, but we
     * generate the right _number_ of them, and we fill in a bunch
     * of address fields in the parse tree so that in the second
     * pass we'll know where (e.g.) the jump targets are in
     * compound statements. Also we count up the size of the stack
     * frame for each function, so that in the second pass we can
     * get all the internal stack offsets right.
     */
    for (ctx->pass = 0; ctx->pass < 2; ctx->pass++) {
	ctx->ninsns = 0;

	for (i = 0; i < ctx->tree->nfns; i++) {
	    /*
	     * Do actual code generation for each function.
	     * Top-level variables' initialisers are run at the
	     * start of the main function, between its function
	     * prologue and its own local variable initialisers.
	     */
	    ick_codegen_fn(ctx, &ctx->tree->fns[i],
			   (i == 0 ? &ctx->tree->toplevel_vars : NULL));
	}
    }

    scr->insns = ctx->insns;
    scr->ninsns = ctx->ninsns;
    scr->stringlits = ctx->stringlits;
    scr->nstringlits = ctx->nstringlits;

    scr->lib = ctx->lib;

    return scr;
}

/* ----------------------------------------------------------------------
 * Top-level implementation of ick_compile().
 */

ickscript *ick_compile(char **err,
		       const char *mainfuncname, const char *mainfuncproto,
		       icklib *lib, int (*read)(void *ctx), void *readctx)
{
    struct ick_parse_ctx actx;
    struct ick_parse_ctx *ctx = &actx;
    struct ickscript *ret;

    ctx->read = read;
    ctx->readctx = readctx;
    ctx->err = err;
    *ctx->err = NULL;
    ctx->tok.tvalue = NULL;
    ctx->tok.type = -1;
    ctx->tok.tvalue_allocsize = 0;

    ctx->lex_pos.line = ctx->lex_pos.col = 0;
    ctx->lex_last = '\n';
    ick_lex_read(ctx);
    ick_lex_advance(ctx);

    ctx->lib = lib;

#ifdef TEST_LEX
    while (1) {
	static const char *const tokennames[] = { TOKENTYPES(TOKENNAME) };
	printf("%s", tokennames[ctx->tok.type]);
	if (ctx->tok.type == TOK_IDENTIFIER)
	    printf(" %s\n", ctx->tok.tvalue);
	else if (ctx->tok.type == TOK_STRINGLIT)
	    printf(" \"%s\"\n", ctx->tok.tvalue);   /* CBA with quoting! */
	else if (ctx->tok.type == TOK_BOOLLIT)
	    printf(" %s\n", ctx->tok.ivalue ? "true" : "false");
	else if (ctx->tok.type == TOK_INTLIT)
	    printf(" %d\n", ctx->tok.ivalue);
	else
	    printf("\n");
	if (ctx->tok.type == TOK_EOF || ctx->tok.type == TOK_ERROR)
	    break;
	ick_lex_advance(ctx);
    }
    return NULL;
#endif

    ctx->tree = ick_parse_toplevel(ctx);
    if (!ctx->tree) {
	sfree(ctx->tok.tvalue);
	return NULL;
    }

    if (!ick_semantic_analysis(ctx, mainfuncname, mainfuncproto)) {
	ick_free_parsetree(ctx->tree);
	sfree(ctx->tok.tvalue);
	return NULL;
    }

    /*
     * All the user errors have been caught in the previous
     * phases, so code generation cannot throw an error unless by
     * internal assertion failure.
     */
    ret = ick_codegen(ctx);

    sfree(ctx->tok.tvalue);
    return ret;
}

/* ----------------------------------------------------------------------
 * Standard library management, plus the internal standard library
 * function implementations.
 */

static int ick_libfn_len(void *result, const char **sparams,
			 const int *iparams)
{
    *(int *)result = strlen(sparams[0]);
    return 0;
}
static void ick_libjs_len(icklib_jswrite_fn_t write, void *ctx,
			  const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").length");
}

static int ick_libfn_substr3(void *result, const char **sparams,
			     const int *iparams)
{
    int inlen = strlen(sparams[0]);
    int start = iparams[0], end = iparams[1];
    if (start > inlen)
	start = inlen;
    if (end > inlen)
	end = inlen;
    if (start < 0)
	start = 0;
    if (end < 0)
	end = 0;
    if (end < start)
	return ICK_RTE_SUBSTR_BACKWARDS;
    *(char **)result = snewn(end-start+1, char);
    strncpy(*(char **)result, sparams[0] + start, end - start);
    (*(char **)result)[end-start] = '\0';
    return 0;
}
static void ick_libjs_substr3(icklib_jswrite_fn_t write, void *ctx,
			      const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").substring(");
    write(ctx, args[1]);
    write(ctx, ",");
    write(ctx, args[2]);
    write(ctx, ")");
}

static int ick_libfn_substr2(void *result, const char **sparams,
			     const int *iparams)
{
    int inlen = strlen(sparams[0]);
    int start = iparams[0], len;
    if (start > inlen)
	start = inlen;
    if (start < 0)
	start = 0;
    len = inlen - start;
    if (len < 0)
	len = 0;
    *(char **)result = snewn(len+1, char);
    strncpy(*(char **)result, sparams[0] + start, len);
    (*(char **)result)[len] = '\0';
    return 0;
}
static void ick_libjs_substr2(icklib_jswrite_fn_t write, void *ctx,
			      const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").substring(");
    write(ctx, args[1]);
    write(ctx, ")");
}

static int ick_libfn_atoi(void *result, const char **sparams,
			  const int *iparams)
{
    *(int *)result = atoi(sparams[0]);
    return 0;
}
static void ick_libjs_atoi(icklib_jswrite_fn_t write, void *ctx,
			   const char **args)
{
    write(ctx, "(parseInt(");
    write(ctx, args[0]);
    write(ctx, "))");
}

static int ick_libfn_itoa(void *result, const char **sparams,
			  const int *iparams)
{
    *(char **)result = snewn(80, char);
    sprintf(*(char **)result, "%d", iparams[0]);
    return 0;
}
static void ick_libjs_itoa(icklib_jswrite_fn_t write, void *ctx,
			   const char **args)
{
    write(ctx, "((~~");
    write(ctx, args[0]);
    write(ctx, ").toString())");
}

static int ick_libfn_ord(void *result, const char **sparams,
			 const int *iparams)
{
    *(int *)result = (unsigned char)(sparams[0][0]);
    return 0;
}
static void ick_libjs_ord(icklib_jswrite_fn_t write, void *ctx,
			  const char **args)
{
    write(ctx, "((");
    write(ctx, args[0]);
    write(ctx, ").charCodeAt(0))");
}

static int ick_libfn_chr(void *result, const char **sparams,
			 const int *iparams)
{
    *(char **)result = snewn(2, char);
    (*(char **)result)[0] = iparams[0];
    (*(char **)result)[1] = '\0';
    return 0;
}
static void ick_libjs_chr(icklib_jswrite_fn_t write, void *ctx,
			  const char **args)
{
    write(ctx, "(String.fromCharCode(");
    write(ctx, args[0]);
    write(ctx, "))");
}

static int ick_libfn_index2(void *result, const char **sparams,
			    const int *iparams)
{
    const char *found = strstr(sparams[0], sparams[1]);
    if (found)
	*(int *)result = found - sparams[0];
    else
	*(int *)result = -1;
    return 0;
}
static void ick_libjs_index2(icklib_jswrite_fn_t write, void *ctx,
			     const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").indexOf(");
    write(ctx, args[1]);
    write(ctx, ")");
}

static int ick_libfn_index3(void *result, const char **sparams,
			    const int *iparams)
{
    const char *found;
    int start = iparams[0];
    if (start < 0)
	start = 0;
    if (start > (int)strlen(sparams[0])) {
	*(int *)result = -1;	       /* it can't be there */
    } else {
	found = strstr(sparams[0] + start, sparams[1]);
	if (found)
	    *(int *)result = found - sparams[0];
	else
	    *(int *)result = -1;
    }
    return 0;
}
static void ick_libjs_index3(icklib_jswrite_fn_t write, void *ctx,
			     const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").indexOf(");
    write(ctx, args[1]);
    write(ctx, ",");
    write(ctx, args[2]);
    write(ctx, ")");
}

static int ick_libfn_rindex2(void *result, const char **sparams,
			     const int *iparams)
{
    int len2 = strlen(sparams[1]);
    int i = strlen(sparams[0]) - len2;
    while (i >= 0) {
	if (!strncmp(sparams[0] + i, sparams[1], len2))
	    break;
	i--;
    }
    *(int *)result = i;
    return 0;
}
static void ick_libjs_rindex2(icklib_jswrite_fn_t write, void *ctx,
			      const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").lastIndexOf(");
    write(ctx, args[1]);
    write(ctx, ")");
}

static int ick_libfn_rindex3(void *result, const char **sparams,
			     const int *iparams)
{
    int len2 = strlen(sparams[1]);
    int i = strlen(sparams[0]) - len2;
    if (i > iparams[0])
	i = iparams[0];
    while (i >= 0) {
	if (!strncmp(sparams[0] + i, sparams[1], len2))
	    break;
	i--;
    }
    if (i < 0)
	i = -1;
    *(int *)result = i;
    return 0;
}
static void ick_libjs_rindex3(icklib_jswrite_fn_t write, void *ctx,
			      const char **args)
{
    write(ctx, "(");
    write(ctx, args[0]);
    write(ctx, ").lastIndexOf(");
    write(ctx, args[1]);
    write(ctx, ",");
    write(ctx, args[2]);
    write(ctx, ")");
}

static int ick_libfn_min(void *result, const char **sparams,
			 const int *iparams)
{
    if (iparams[0] < iparams[1])
	*(int *)result = iparams[0];
    else
	*(int *)result = iparams[1];
    return 0;
}
static void ick_libjs_min(icklib_jswrite_fn_t write, void *ctx,
			  const char **args)
{
    write(ctx, "(Math.min(");
    write(ctx, args[0]);
    write(ctx, ",");
    write(ctx, args[1]);
    write(ctx, "))");
}

static int ick_libfn_max(void *result, const char **sparams,
			 const int *iparams)
{
    if (iparams[0] > iparams[1])
	*(int *)result = iparams[0];
    else
	*(int *)result = iparams[1];
    return 0;
}
static void ick_libjs_max(icklib_jswrite_fn_t write, void *ctx,
			  const char **args)
{
    write(ctx, "(Math.max(");
    write(ctx, args[0]);
    write(ctx, ",");
    write(ctx, args[1]);
    write(ctx, "))");
}

icklib *ick_lib_new(int empty)
{
    icklib *lib = snew(icklib);
    lib->fns = NULL;
    lib->nfns = lib->fnsize = 0;

    if (!empty) {
	ick_lib_addfn(lib, "len", "IS", ick_libfn_len, ick_libjs_len);
	ick_lib_addfn(lib, "substr", "SSII", ick_libfn_substr3, 
		      ick_libjs_substr3);
	ick_lib_addfn(lib, "substr", "SSI", ick_libfn_substr2,
		      ick_libjs_substr2);
	ick_lib_addfn(lib, "atoi", "IS", ick_libfn_atoi, ick_libjs_atoi);
	ick_lib_addfn(lib, "itoa", "SI", ick_libfn_itoa, ick_libjs_itoa);
	ick_lib_addfn(lib, "ord", "IS", ick_libfn_ord, ick_libjs_ord);
	ick_lib_addfn(lib, "chr", "SI", ick_libfn_chr, ick_libjs_chr);
	ick_lib_addfn(lib, "index", "ISS", ick_libfn_index2, ick_libjs_index2);
	ick_lib_addfn(lib, "index", "ISSI", ick_libfn_index3,
		      ick_libjs_index3);
	ick_lib_addfn(lib, "rindex", "ISS", ick_libfn_rindex2,
		      ick_libjs_rindex2);
	ick_lib_addfn(lib, "rindex", "ISSI", ick_libfn_rindex3,
		      ick_libjs_rindex3);
	ick_lib_addfn(lib, "min", "III", ick_libfn_min, ick_libjs_min);
	ick_lib_addfn(lib, "max", "III", ick_libfn_max, ick_libjs_max);
    }

    return lib;
}

void ick_lib_addfn(icklib *lib, const char *funcname, const char *funcproto,
		   icklib_fn_t func, icklib_js_fn_t jstranslate)
{
    struct ick_fndecl *fn;
    int i;

    if (lib->nfns >= lib->fnsize) {
	lib->fnsize = (lib->nfns * 3 / 2) + 32;
	lib->fns = sresize(lib->fns, lib->fnsize, struct ick_fndecl);
    }
    fn = &lib->fns[lib->nfns++];

    fn->identifier = snewn(strlen(funcname)+1, char);
    strcpy(fn->identifier, funcname);
    fn->rettype = CHAR_TO_TYPE(funcproto[0]);
    fn->params.ndecls = fn->params.declsize = strlen(funcproto)-1;
    fn->params.decls = snewn(fn->params.ndecls, struct ick_vardecl);
    for (i = 0; i < fn->params.ndecls; i++) {
	fn->params.decls[i].identifier = NULL;
	fn->params.decls[i].type = CHAR_TO_TYPE(funcproto[i+1]);
	fn->params.decls[i].initialiser = NULL;
	fn->params.decls[i].address = 0;
    }
    fn->defn = NULL;
    fn->native = func;
    fn->js = jstranslate;
    fn->istack = fn->sstack = 0;
    fn->address = -1 - lib->nfns;
}

void ick_lib_free(icklib *lib)
{
    int i;

    for (i = 0; i < lib->nfns; i++) {
	struct ick_fndecl *fn = &lib->fns[i];
	sfree(fn->identifier);
	ick_free_vardecls(&fn->params);
	ick_free_block(fn->defn);
    }
    sfree(lib->fns);
    sfree(lib);
}

/* ----------------------------------------------------------------------
 * Implementation of the internal Ick script execution engine.
 */

/*
 * The Ick virtual machine is register-free, using only a stack
 * for storage. However, it isn't stack-based in a RPN sense, as
 * the JVM is. Instead, instructions will directly read and write
 * specified locations on the stack, as if the stack was entirely
 * composed of registers. Positive or zero stack offsets are
 * interpreted relative to the _bottom_ of the stack (which is
 * where global variables are stored); most offsets are negative,
 * and are treated as specifying values just below the stack
 * pointer.
 *
 * There is not one stack, but two working independently. One is
 * used for storing integers, and the other stores strings. The
 * integer stack is used for everything other than a string during
 * execution: actual integer variables and values, boolean
 * variables and values, and return addresses. So a function will
 * typically establish a stack frame by moving _both_ pointers
 * down by different amounts (having worked out how many integer
 * and how many string temporaries it'll need), and then restore
 * them at the end.
 *
 * The program itself is a big array of instructions, numbered
 * from zero. Entry point is at zero. Jumping to negative
 * addresses is used to communicate to the execution engine that
 * it needs to do something unusual: address -1 terminates the VM
 * (although the program doesn't refer to that one explicitly -
 * it's set up as the return address of the main function, so that
 * codegen can just have that function return as usual) and lower
 * addresses gateway to native library functions.
 *
 * There's one other supporting structure required, which is a
 * list of string literals.
 *
 * So the actual ick_insn structure simply contains four integer
 * fields: opcode plus three operands. The operands may address
 * positions on the string stack (s), positions on the integer
 * stack (i), program indices (p), indices into the string literal
 * pool (l), or directly encode integer or boolean literals; each
 * opcode uniquely defines its number and meaning of operands.
 *
 * The list of opcodes is given above, in the enum defined by the
 * OPCODES macro.
 *
 * The calling standard needs to be documented as part of the VM,
 * since the VM itself implements it when setting up the call to
 * the main function and when processing calls to native
 * functions. (Of course, a compiler could choose to ue a
 * different calling standard for the program's _internal_ calls;
 * but mine doesn't, and why would anyone bother?)
 * 
 * On entry, a function expects i-1 (the topmost entry on the
 * integer stack) to contain its return address. i-2 is the first
 * integer or boolean parameter, if any; i-3 the next, and so on.
 * Meanwhile, s-1 (the topmost entry on the string stack) is its
 * first string parameter, if any; s-2 is the next, and so on. The
 * function may establish a stack frame of its own, if it wishes,
 * but when it returns the stack pointer should be exactly where
 * it was on entry. An integer or boolean return value, if any, is
 * left in i-2; a string return value in s-1.
 * 
 * At the start of execution, the VM begins by reserving space at
 * the very bottom of the stack for global variables as directed
 * by the script's ivars and svars values. Then it pushes the
 * arguments to the main function, plus the (negative) return
 * address, and then it begins executing from the start of main.
 * So global variable initialisation must happen _within_ main.
 * When main returns, it picks its negative return address off the
 * stack and jumps to it, causing the VM to terminate.
 */

struct ick_sstack_entry {
    char *str;
    int len, size;
};

struct ick_stacks {
    int *istack;
    int isp, istksize;
    struct ick_sstack_entry *sstack;
    int ssp, sstksize;
    int strtotal, strlimit;
};

static void ick_movesp(struct ick_stacks *stacks, int idelta, int sdelta)
{
    if (stacks->isp + idelta > stacks->istksize) {
	stacks->istksize = (stacks->isp + idelta) * 3 / 2 + 64;
	stacks->istack = sresize(stacks->istack, stacks->istksize, int);
    }
    stacks->isp += idelta;
    if (stacks->ssp + sdelta > stacks->sstksize) {
	stacks->sstksize = (stacks->ssp + sdelta) * 3 / 2 + 64;
	stacks->sstack = sresize(stacks->sstack, stacks->sstksize,
				 struct ick_sstack_entry);
    }
    if (sdelta > 0) {
	while (sdelta > 0) {
	    stacks->sstack[stacks->ssp].str = NULL;
	    stacks->sstack[stacks->ssp].len = 0;
	    stacks->sstack[stacks->ssp].size = 0;
	    stacks->ssp++;
	    sdelta--;
	}
    } else {
	while (sdelta < 0) {
	    stacks->ssp--;
	    sdelta++;
	    stacks->strtotal -= stacks->sstack[stacks->ssp].len;
	    sfree(stacks->sstack[stacks->ssp].str);
	}
    }
}

static void ick_free_stacks(struct ick_stacks *stacks)
{
    ick_movesp(stacks, -stacks->isp, -stacks->ssp);
    sfree(stacks->istack);
    sfree(stacks->sstack);
}

static void ick_sstack_ensure(struct ick_sstack_entry *entry, int len)
{
    if (entry->size < len+1) {
	entry->size = (len+1) * 3 / 2 + 64;
	entry->str = sresize(entry->str, entry->size, char);
    }
}

static int ick_sstack_set(struct ick_stacks *stacks, int index,
			  const char *str)
{
    struct ick_sstack_entry *entry = &stacks->sstack[index];
    int len = strlen(str);
    stacks->strtotal += len - entry->len;
    if (stacks->strlimit && stacks->strtotal > stacks->strlimit)
	return 0;
    ick_sstack_ensure(entry, len);
    strcpy(entry->str, str);
    entry->len = len;
    return 1;
}

#define IPOS(a,b) do { \
    if ((b) < 0) \
	(a) = (b) + stacks->isp; \
    else \
	(a) = (b); \
    assert((a) >= 0 && (a) < stacks->isp); \
} while (0)
#define SPOS(a,b) do { \
    if ((b) < 0) \
	(a) = (b) + stacks->ssp; \
    else \
	(a) = (b); \
    assert((a) >= 0 && (a) < stacks->ssp); \
} while (0)

int ick_exec_limited_v(void *result, int maxcycles, int maxstk, int maxstr,
		       ickscript *scr, va_list ap)
{
    struct ick_stacks astacks, *stacks = &astacks;
    int pc, i, sargs, iargs;
    int x1, x2, x3;
    int *iparams;
    int iparamsize;
    char **sparams;
    int sparamsize;

    /*
     * Initialise everything.
     */
    stacks->isp = stacks->istksize = 0;
    stacks->ssp = stacks->sstksize = 0;
    stacks->istack = NULL;
    stacks->sstack = NULL;
    stacks->strtotal = 0;
    stacks->strlimit = maxstr;
    iparams = NULL;
    sparams = NULL;
    sparamsize = iparamsize = 0;

    /*
     * Allocate space on the stacks for the global variables.
     */
    ick_movesp(stacks, scr->ivars, scr->svars);

    /*
     * Construct the arguments and return address for the main
     * function.
     */
    sargs = iargs = 0;
    for (i = 0; i < scr->tree->fns[0].params.ndecls; i++) {
	if (scr->tree->fns[0].params.decls[i].type == TOK_KW_STRING)
	    sargs++;
	else
	    iargs++;
    }
    /*
     * Be sure to leave space for the return value, if we haven't
     * already.
     */
    if (scr->tree->fns[0].rettype == TOK_KW_STRING && sargs < 1)
	sargs = 1;
    else if (scr->tree->fns[0].rettype != TOK_KW_VOID && iargs < 1)
	iargs = 1;
    ick_movesp(stacks, iargs+1, sargs);
    iargs = stacks->isp;
    sargs = stacks->ssp;
    stacks->istack[--iargs] = -1;   /* magic return address */
    for (i = 0; i < scr->tree->fns[0].params.ndecls; i++) {
	if (scr->tree->fns[0].params.decls[i].type == TOK_KW_STRING) {
	    const char *arg = va_arg(ap, const char *);
	    if (!ick_sstack_set(stacks, --sargs, arg)) {
		ick_free_stacks(stacks);
		return ICK_RTE_STRINGS_LIMIT_EXCEEDED;
	    }
	} else {
	    stacks->istack[--sargs] = va_arg(ap, int);
	}
    }

    /*
     * Point the PC at instruction zero, the start of the main
     * function.
     */
    pc = 0;

    /*
     * Go! Begin executing VM instructions. Whenever the PC
     * becomes negative, do something magic; -2 and below invoke
     * calls to standard library functions, and -1 terminates the
     * VM.
     */
    while (pc != -1) {
	if (maxcycles > 0 && --maxcycles <= 0) {
	    ick_free_stacks(stacks);
	    sfree(iparams);
	    sfree(sparams);
	    return ICK_RTE_CYCLE_LIMIT_EXCEEDED;
	}
	if (pc < 0) {
	    int fnindex;
	    struct ick_fndecl *fn;
	    int niparams,  nsparams;

	    /*
	     * Find the function being called.
	     */
	    fnindex = -2 - pc;
	    assert(fnindex >= 0 && fnindex < scr->lib->nfns);
	    fn = &scr->lib->fns[fnindex];
	    assert(pc == fn->address);

	    /*
	     * Retrieve its parameters from the VM stacks and
	     * marshal them into parameter blocks.
	     */
	    niparams = nsparams = 0;
	    for (i = 0; i < fn->params.ndecls; i++) {
		if (fn->params.decls[i].type == TOK_KW_STRING)
		    nsparams++;
		else
		    niparams++;
	    }
	    if (niparams >= iparamsize) {
		iparamsize = niparams;
		iparams = sresize(iparams, iparamsize, int);
	    }
	    if (nsparams >= sparamsize) {
		sparamsize = nsparams;
		sparams = sresize(sparams, sparamsize, char *);
	    }
	    niparams = nsparams = 0;
	    for (i = 0; i < fn->params.ndecls; i++) {
		if (fn->params.decls[i].type == TOK_KW_STRING) {
		    SPOS(x1, -1-nsparams);
		    sparams[nsparams++] = stacks->sstack[x1].str;
		} else {
		    IPOS(x1, -2-niparams);
		    iparams[niparams++] = stacks->istack[x1];
		}
	    }

	    /*
	     * Call the function, and transfer its return value
	     * back on to the internal stacks.
	     */
	    {
		char *sret;
		int iret;
		int ret;
		ret = fn->native(fn->rettype == TOK_KW_STRING ? (void *)&sret :
				 (void *)&iret, (const char **)sparams,
				 iparams);
		if (ret) {
		    ick_free_stacks(stacks);
		    sfree(iparams);
		    sfree(sparams);
		    return ret;
		}
		if (fn->rettype == TOK_KW_STRING) {
		    assert(sret != NULL);
		    SPOS(x1, -1);
		    if (!ick_sstack_set(stacks, x1, sret)) {
			ick_free_stacks(stacks);
			sfree(iparams);
			sfree(sparams);
			return ICK_RTE_STRINGS_LIMIT_EXCEEDED;
		    }
		    sfree(sret);
		} else if (fn->rettype != TOK_KW_VOID) {
		    if (fn->rettype == TOK_KW_BOOL)
			iret = (iret != 0);   /* normalise boolean values */
		    IPOS(x1, -2);
		    stacks->istack[x1] = iret;
		}
	    }

	    /*
	     * `Return' from the function by loading from i-1 to
	     * pc.
	     */
	    IPOS(x1, -1);
	    pc = stacks->istack[x1];

	} else {
	    struct ick_insn *insn;

	    assert(pc < scr->ninsns);
	    insn = &scr->insns[pc];
	    switch (insn->opcode) {
	      case OP_MOVESP:
		if (maxstk > 0 &&
		    stacks->isp + stacks->ssp + insn->p1 + insn->p2 > maxstk) {
		    ick_free_stacks(stacks);
		    sfree(iparams);
		    sfree(sparams);
		    return ICK_RTE_STACK_LIMIT_EXCEEDED;
		}
		ick_movesp(stacks, insn->p1, insn->p2);
		break;
	      case OP_JMP:
		/* All loads to pc are offset by 1 because we increment
		 * at the end of this switch. Inelegant, but simple. */
		pc = insn->p1 - 1;
		break;
	      case OP_JMPZ:
		IPOS(x1, insn->p1);
		if (stacks->istack[x1] == 0)
		    pc = insn->p2 - 1;
		break;
	      case OP_JMPNZ:
		IPOS(x1, insn->p1);
		if (stacks->istack[x1] != 0)
		    pc = insn->p2 - 1;
		break;
	      case OP_JMPI:
		IPOS(x1, insn->p1);
		pc = stacks->istack[x1] - 1;
		break;
	      case OP_SCONST:
		SPOS(x1, insn->p1);
		assert(insn->p2 >= 0 && insn->p2 < scr->nstringlits);
		if (!ick_sstack_set(stacks, x1, scr->stringlits[insn->p2])) {
		    ick_free_stacks(stacks);
		    sfree(iparams);
		    sfree(sparams);
		    return ICK_RTE_STRINGS_LIMIT_EXCEEDED;
		}
		break;
	      case OP_ICONST:
		IPOS(x1, insn->p1);
		stacks->istack[x1] = insn->p2;
		break;
	      case OP_IMOV:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = stacks->istack[x2];
		break;
	      case OP_SMOV:
		SPOS(x1, insn->p1);
		SPOS(x2, insn->p2);
		if (!ick_sstack_set(stacks, x1, stacks->sstack[x2].str)) {
		    ick_free_stacks(stacks);
		    sfree(iparams);
		    sfree(sparams);
		    return ICK_RTE_STRINGS_LIMIT_EXCEEDED;
		}
		break;
	      case OP_ADD:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		IPOS(x3, insn->p3);
		stacks->istack[x1] = stacks->istack[x2] + stacks->istack[x3];
		break;
	      case OP_SUB:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		IPOS(x3, insn->p3);
		stacks->istack[x1] = stacks->istack[x2] - stacks->istack[x3];
		break;
	      case OP_MUL:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		IPOS(x3, insn->p3);
		stacks->istack[x1] = stacks->istack[x2] * stacks->istack[x3];
		break;
	      case OP_DIV:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		IPOS(x3, insn->p3);
		if (stacks->istack[x3] == 0) {
		    ick_free_stacks(stacks);
		    sfree(iparams);
		    sfree(sparams);
		    return ICK_RTE_DIVBYZERO;
		}
		stacks->istack[x1] = stacks->istack[x2] / stacks->istack[x3];
		break;
	      case OP_CONCAT:
		SPOS(x1, insn->p1);
		SPOS(x2, insn->p2);
		SPOS(x3, insn->p3);
		{
		    int len2 = stacks->sstack[x2].len;
		    int len3 = stacks->sstack[x3].len;
		    ick_sstack_ensure(&stacks->sstack[x1], len2 + len3);
		    /*
		     * Be a little careful with the ordering here,
		     * to avoid overlapping copying in the case
		     * where x1 aliases one or both of x2,x3.
		     */
		    memmove(stacks->sstack[x1].str + len2,
			    stacks->sstack[x3].str, len3 + 1);
		    if (x1 != x2)
			memcpy(stacks->sstack[x1].str,
			       stacks->sstack[x2].str, len2);
		    stacks->sstack[x1].len = len2 + len3;
		}
		break;
	      case OP_ICMP:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		IPOS(x3, insn->p3);
		if (stacks->istack[x2] < stacks->istack[x3])
		    stacks->istack[x1] = -1;
		else if (stacks->istack[x2] > stacks->istack[x3])
		    stacks->istack[x1] = +1;
		else
		    stacks->istack[x1] = 0;
		break;
	      case OP_SCMP:
		IPOS(x1, insn->p1);
		SPOS(x2, insn->p2);
		SPOS(x3, insn->p3);
		stacks->istack[x1] = strcmp(stacks->sstack[x2].str,
					    stacks->sstack[x3].str);
		break;
	      case OP_LE0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] <= 0);
		break;
	      case OP_LT0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] < 0);
		break;
	      case OP_GE0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] >= 0);
		break;
	      case OP_GT0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] > 0);
		break;
	      case OP_NE0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] != 0);
		break;
	      case OP_EQ0:
		IPOS(x1, insn->p1);
		IPOS(x2, insn->p2);
		stacks->istack[x1] = (stacks->istack[x2] == 0);
		break;
	    }
	    pc++;
	}
    }

    /*
     * FIXME: limit execution time and/or stack depth and/or
     * string size, that being the whole point of having the VM
     * here at all.
     */

    /*
     * Retrieve the return value from the main function's
     * argument area.
     */
    if (scr->tree->fns[0].rettype == TOK_KW_STRING) {
	SPOS(x1, -1);
	*(char **)result = stacks->sstack[x1].str;
	stacks->sstack[x1].str = NULL;
    } else if (scr->tree->fns[0].rettype != TOK_KW_VOID) {
	IPOS(x1, -2);
	*(int *)result = stacks->istack[x1];
    }

    ick_free_stacks(stacks);
    sfree(iparams);
    sfree(sparams);
    return ICK_RTE_NO_ERROR;
}

/* ----------------------------------------------------------------------
 * Implementation of the Ick-to-Javascript translator.
 */

struct ick_js_ctx {
    /*
     * List of identifier names to avoid using.
     */
    const char **avoidids;
    int navoidids, avoididsize;
    /*
     * List of identifier names needing to be freed.
     */
    char **freeids;
    int nfreeids, freeidsize;
    /*
     * The output text we're accumulating.
     */
    char *out;
    int outlen, outsize;
};

static const char *ick_js_addid(struct ick_js_ctx *ctx,
				const char *id, int needsfree)
{
    if (needsfree) {
	if (ctx->nfreeids >= ctx->freeidsize) {
	    ctx->freeidsize = ctx->nfreeids * 3 / 2 + 64;
	    ctx->freeids = sresize(ctx->freeids, ctx->freeidsize, char *);
	}
	ctx->freeids[ctx->nfreeids++] = (char *)id;
    }
    if (ctx->navoidids >= ctx->avoididsize) {
	ctx->avoididsize = ctx->navoidids * 3 / 2 + 64;
	ctx->avoidids = sresize(ctx->avoidids, ctx->avoididsize, const char *);
    }
    ctx->avoidids[ctx->navoidids++] = id;
    return id;
}

static void ick_js_popids(struct ick_js_ctx *ctx, int navoid, int nfree)
{
    assert(ctx->navoidids >= navoid);
    ctx->navoidids = navoid;
    assert(ctx->nfreeids >= nfree);
    while (ctx->nfreeids > nfree) {
	ctx->nfreeids--;
	sfree(ctx->freeids[ctx->nfreeids]);
    }
}

static const char *ick_js_makename(struct ick_js_ctx *ctx, const char *name)
{
    char *ret;
    int namelen = strlen(name);
    int suffix = 0, i, ndigits;

    /*
     * Work out the maximum number of digits we might need. The
     * maximum suffix we might conceivably need is ctx->navoidids
     * (because after we try that, we've tried one more name than
     * there is in the list of things to avoid, so one _must_ have
     * been free).
     */
    i = ctx->navoidids;
    ndigits = 0;
    while (i > 0)
	i /= 10, ndigits++;

    ret = snewn(namelen + ndigits + 1, char);
    strcpy(ret, name);

    do {
	assert(suffix <= ctx->navoidids);
	sprintf(ret + namelen, "%.0d", suffix++);
	for (i = 0; i < ctx->navoidids; i++)
	    if (!strcmp(ctx->avoidids[i], ret))
		break;
    } while (i < ctx->navoidids);

    return ick_js_addid(ctx, ret, 1);
}

static void ick_js_write(void *vctx, const char *buf)
{
    struct ick_js_ctx *ctx = (struct ick_js_ctx *)vctx;
    int buflen = strlen(buf);

    if (ctx->outlen + buflen + 1 > ctx->outsize) {
	ctx->outsize = (ctx->outlen + buflen + 1) * 3 / 2 + 1024;
	ctx->out = sresize(ctx->out, ctx->outsize, char);
    }
    strcpy(ctx->out + ctx->outlen, buf);
    ctx->outlen += buflen;
}

static void ick_js_indent(struct ick_js_ctx *ctx, int indent)
{
    int i;

    for (i = 0; i < indent; i++)
	ick_js_write(ctx, "    ");
}

static void ick_js_namevars(struct ick_js_ctx *ctx, struct ick_vardecls *vars)
{
    int i;

    for (i = 0; i < vars->ndecls; i++) {
	vars->decls[i].jsname =
	    ick_js_makename(ctx, vars->decls[i].identifier);
    }
}

static void ick_js_expr(struct ick_js_ctx *ctx,
			struct ick_expr *expr)
{
    int i, optype;
    const char *op = NULL;

    optype = OPTYPE(expr->op, expr->type);

    switch (optype) {
      case OPTYPE(TOK_COMMA, TOK_KW_STRING):
      case OPTYPE(TOK_COMMA, TOK_KW_INT):
      case OPTYPE(TOK_COMMA, TOK_KW_BOOL):
      case OPTYPE(TOK_COMMA, TOK_KW_VOID):
	op = ", ";
	break;

      case OPTYPE(TOK_QUERY, TOK_KW_STRING):
      case OPTYPE(TOK_QUERY, TOK_KW_INT):
      case OPTYPE(TOK_QUERY, TOK_KW_BOOL):
      case OPTYPE(TOK_QUERY, TOK_KW_VOID):
	ick_js_write(ctx, "(");
	ick_js_expr(ctx, expr->e1);
	ick_js_write(ctx, " ? ");
	ick_js_expr(ctx, expr->e2);
	ick_js_write(ctx, " : ");
	ick_js_expr(ctx, expr->e3);
	ick_js_write(ctx, ")");
	return;

      case OPTYPE(TOK_LOGAND, TOK_KW_BOOL):
      case OPTYPE(TOK_LOGOR, TOK_KW_BOOL):
	op = expr->op == TOK_LOGAND ? " && " : " || ";
	break;

      case OPTYPE(TOK_ASSIGN, TOK_KW_STRING):
      case OPTYPE(TOK_ASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_ASSIGN, TOK_KW_BOOL):
	op = " = ";
	break;

      case OPTYPE(TOK_PLUSASSIGN, TOK_KW_STRING):
      case OPTYPE(TOK_PLUSASSIGN, TOK_KW_INT):
	op = " += ";		       /* same operator in JS as well */
	break;
      case OPTYPE(TOK_MINUSASSIGN, TOK_KW_INT):
	op = " -= ";
	break;
      case OPTYPE(TOK_MULASSIGN, TOK_KW_INT):
	op = " *= ";
	break;
      case OPTYPE(TOK_DIVASSIGN, TOK_KW_INT):
      case OPTYPE(TOK_LOGORASSIGN, TOK_KW_BOOL):
      case OPTYPE(TOK_LOGANDASSIGN, TOK_KW_BOOL):
	/*
	 * These compound assignments don't work in JS: division
	 * is float and we need to coerce it to int, and JS lacks
	 * &&= and ||=.
	 * 
	 * This _could_ be a total pain, but fortunately the Ick
	 * language contains nothing which is both an lvalue and
	 * an expression with side effects, so we can simply
	 * replace "a @= b" for some operator @ with "a = a @ b".
	 */
	ick_js_write(ctx, "(");
	ick_js_expr(ctx, expr->e1);
	ick_js_write(ctx, " = ");
	if (expr->op == TOK_DIVASSIGN)
	    ick_js_write(ctx, "~~(");  /* see TOK_DIV below */
	ick_js_expr(ctx, expr->e1);
	ick_js_write(ctx, (expr->op == TOK_DIVASSIGN ? " / " :
			   expr->op == TOK_LOGANDASSIGN ? " && " : " || "));
	ick_js_expr(ctx, expr->e2);
	if (expr->op == TOK_DIVASSIGN)
	    ick_js_write(ctx, ")");
	ick_js_write(ctx, ")");
	return;

      case OPTYPE(TOK_LT, TOK_KW_BOOL):
	op = " < ";
	break;
      case OPTYPE(TOK_LE, TOK_KW_BOOL):
	op = " <= ";
	break;
      case OPTYPE(TOK_GT, TOK_KW_BOOL):
	op = " > ";
	break;
      case OPTYPE(TOK_GE, TOK_KW_BOOL):
	op = " >= ";
	break;
      case OPTYPE(TOK_EQ, TOK_KW_BOOL):
	op = " == ";
	break;
      case OPTYPE(TOK_NE, TOK_KW_BOOL):
	op = " != ";
	break;

      case OPTYPE(TOK_PLUS, TOK_KW_STRING):
      case OPTYPE(TOK_PLUS, TOK_KW_INT):
	if (!expr->e2) {
	    ick_js_write(ctx, "(+");   /* might as well */
	    ick_js_expr(ctx, expr->e1);
	    ick_js_write(ctx, ")");
	    return;
	}
	op = " + ";
	break;
      case OPTYPE(TOK_MINUS, TOK_KW_INT):
	if (!expr->e2) {
	    ick_js_write(ctx, "(-");
	    ick_js_expr(ctx, expr->e1);
	    ick_js_write(ctx, ")");
	    return;
	}
	op = " - ";
	break;
      case OPTYPE(TOK_MUL, TOK_KW_INT):
	op = " * ";
	break;
      case OPTYPE(TOK_DIV, TOK_KW_INT):
	/*
	 * Javascript division is floating-point only. In order to
	 * coerce the result to an integer, I cheat shamelessly by
	 * applying the bitwise NOT operator (which has
	 * coerce-to-integer as a side effect), and then applying
	 * it again.
	 */
	ick_js_write(ctx, "(~~(");
	ick_js_expr(ctx, expr->e1);
	ick_js_write(ctx, " / ");
	ick_js_expr(ctx, expr->e2);
	ick_js_write(ctx, "))");
	return;
      case OPTYPE(TOK_LOGNOT, TOK_KW_BOOL):
	ick_js_write(ctx, "(!");
	ick_js_expr(ctx, expr->e1);
	ick_js_write(ctx, ")");
	return;
	break;

      case OPTYPE(TOK_INCREMENT, TOK_KW_INT):
      case OPTYPE(TOK_DECREMENT, TOK_KW_INT):
	ick_js_write(ctx, "(");
	op = (expr->op == TOK_INCREMENT ? "++" : "--");
	if (expr->e1) {
	    ick_js_write(ctx, op);
	    ick_js_expr(ctx, expr->e1);
	} else {
	    ick_js_expr(ctx, expr->e2);
	    ick_js_write(ctx, op);
	}
	ick_js_write(ctx, ")");
	return;

      case OPTYPE(TOK_STRINGLIT, TOK_KW_STRING):
	ick_js_write(ctx, "\"");
	for (i = 0; expr->sval[i]; i++) {
	    char buf[10];
	    if (expr->sval[i] >= 32 && expr->sval[i] < 127 &&
		expr->sval[i] != '\\' && expr->sval[i] != '"') {
		buf[0] = expr->sval[i];
		buf[1] = '\0';
	    } else {
		sprintf(buf, "\\x%02x", (unsigned char)expr->sval[i]);
	    }
	    ick_js_write(ctx, buf);
	}
	ick_js_write(ctx, "\"");
	return;

      case OPTYPE(TOK_INTLIT, TOK_KW_INT):
	{
	    char buf[80];	       /* I assume that's big enough */

	    sprintf(buf, "(%d)", expr->ival);
	    ick_js_write(ctx, buf);
	}
	return;

      case OPTYPE(TOK_BOOLLIT, TOK_KW_BOOL):
	ick_js_write(ctx, expr->ival ? "true" : "false");
	return;

      case OPTYPE(TOK_IDENTIFIER, TOK_KW_STRING):
      case OPTYPE(TOK_IDENTIFIER, TOK_KW_INT):
      case OPTYPE(TOK_IDENTIFIER, TOK_KW_BOOL):
	ick_js_write(ctx, expr->var->jsname);
	return;

      case OPTYPE(TOK_LPAREN, TOK_KW_STRING):
      case OPTYPE(TOK_LPAREN, TOK_KW_INT):
      case OPTYPE(TOK_LPAREN, TOK_KW_BOOL):
      case OPTYPE(TOK_LPAREN, TOK_KW_VOID):
	if (expr->fn->js) {
	    char **args;
	    char *saved_out;
	    int saved_outlen, saved_outsize;
	    struct ick_expr *arg;

	    /*
	     * Library function with JS translation supplied. We
	     * have to do something moderately disgusting to
	     * construct the argument strings.
	     */
	    saved_out = ctx->out;
	    saved_outlen = ctx->outlen;
	    saved_outsize = ctx->outsize;

	    args = snewn(expr->fn->params.ndecls, char *);
	    arg = expr->e2;
	    for (i = 0; i < expr->fn->params.ndecls; i++) {
		ctx->out = NULL;
		ctx->outlen = ctx->outsize = 0;
		assert(arg && arg->e1);
		ick_js_expr(ctx, arg->e1);
		arg = arg->e2;
		args[i] = ctx->out;
	    }
	    assert(!arg);

	    ctx->out = saved_out;
	    ctx->outlen = saved_outlen;
	    ctx->outsize = saved_outsize;

	    ick_js_write(ctx, "(");
	    expr->fn->js(ick_js_write, ctx, (const char **)args);
	    ick_js_write(ctx, ")");

	    for (i = 0; i < expr->fn->params.ndecls; i++)
		sfree(args[i]);
	    sfree(args);
	} else {
	    struct ick_expr *arg;

	    ick_js_write(ctx, expr->fn->jsname);
	    ick_js_write(ctx, "(");
	    arg = expr->e2;
	    for (i = 0; i < expr->fn->params.ndecls; i++) {
		if (i > 0)
		    ick_js_write(ctx, ", ");
		assert(arg && arg->e1);
		ick_js_expr(ctx, arg->e1);
		arg = arg->e2;
	    }
	    assert(!arg);
	    ick_js_write(ctx, ")");
	}
	return;

      default:
	assert(!"We shouldn't get here");
	break;
    }

    /*
     * For anything which didn't have a special case above, here's
     * the basic standard binary operator output.
     */
    ick_js_write(ctx, "(");
    ick_js_expr(ctx, expr->e1);
    ick_js_write(ctx, op);
    ick_js_expr(ctx, expr->e2);
    ick_js_write(ctx, ")");
}

static void ick_js_block(struct ick_js_ctx *ctx, struct ick_block *blk,
			 int indent, struct ick_vardecls *extravars);

static void ick_js_statement(struct ick_js_ctx *ctx,
			     struct ick_statement *stmt,
			     int indent, int substmt)
{
    switch (stmt->type) {
      case TOK_KW_RETURN:
	if (stmt->e1) {
	    ick_js_indent(ctx, indent);
	    ick_js_write(ctx, "return ");
	    ick_js_expr(ctx, stmt->e1);
	    ick_js_write(ctx, ";\n");
	} else {
	    ick_js_indent(ctx, indent);
	    ick_js_write(ctx, "return;\n");
	}
	break;
      case TOK_KW_BREAK:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "break;\n");
	break;
      case TOK_KW_CONTINUE:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "continue;\n");
	break;
      case TOK_KW_IF:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "if (");
	ick_js_expr(ctx, stmt->e1);
	ick_js_write(ctx, ")\n");
	ick_js_statement(ctx, stmt->s1, indent+1, 1);
	if (stmt->s2) {
	    ick_js_indent(ctx, indent);
	    ick_js_write(ctx, "else\n");
	    ick_js_statement(ctx, stmt->s2, indent+1, 1);
	}
	break;
      case TOK_KW_WHILE:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "while (");
	ick_js_expr(ctx, stmt->e1);
	ick_js_write(ctx, ")\n");
	ick_js_statement(ctx, stmt->s1, indent+1, 1);
	break;
      case TOK_KW_DO:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "do\n");
	ick_js_statement(ctx, stmt->s1, indent+1, 1);
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "while (");
	ick_js_expr(ctx, stmt->e1);
	ick_js_write(ctx, ");\n");
	break;
      case TOK_KW_FOR:
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "for (");
	if (stmt->e1)
	    ick_js_expr(ctx, stmt->e1);
	else
	    ick_js_write(ctx, " ");
	ick_js_write(ctx, "; ");
	if (stmt->e2)
	    ick_js_expr(ctx, stmt->e2);
	ick_js_write(ctx, "; ");
	if (stmt->e3)
	    ick_js_expr(ctx, stmt->e3);
	ick_js_write(ctx, ")\n");
	ick_js_statement(ctx, stmt->s1, indent+1, 1);
	break;
      case TOK_LBRACE:
	ick_js_block(ctx, stmt->blk, indent+1 - substmt, NULL);
	break;
      case -1:
	ick_js_indent(ctx, indent);
	ick_js_expr(ctx, stmt->e1);
	ick_js_write(ctx, ";\n");
	break;
      default:
	assert(!"We shouldn't get here");
	break;
    }

}

static void ick_js_declarevars(struct ick_js_ctx *ctx,
			       struct ick_vardecls *vars, int indent)
{
    int i;

    ick_js_namevars(ctx, vars);

    if (vars->ndecls) {
	ick_js_indent(ctx, indent);
	ick_js_write(ctx, "var ");
	for (i = 0; i < vars->ndecls; i++) {
	    if (i > 0)
		ick_js_write(ctx, ", ");
	    ick_js_write(ctx, vars->decls[i].jsname);
	}
	ick_js_write(ctx, ";\n");
    }
}

static void ick_js_initvars(struct ick_js_ctx *ctx,
			    struct ick_vardecls *vars, int indent)
{
    int i;

    if (vars->ndecls) {
	for (i = 0; i < vars->ndecls; i++) {
	    ick_js_indent(ctx, indent);
	    ick_js_write(ctx, vars->decls[i].jsname);
	    ick_js_write(ctx, " = ");
	    if (vars->decls[i].initialiser) {
		ick_js_expr(ctx, vars->decls[i].initialiser);
	    } else switch (vars->decls[i].type) {
	      case TOK_KW_STRING:
		ick_js_write(ctx, "\"\"");
		break;
	      case TOK_KW_INT:
		ick_js_write(ctx, "0");
		break;
	      case TOK_KW_BOOL:
		ick_js_write(ctx, "false");
		break;
	    }
	    ick_js_write(ctx, ";\n");
	}
	ick_js_write(ctx, "\n");
    }
}

static void ick_js_block(struct ick_js_ctx *ctx, struct ick_block *blk,
			 int indent, struct ick_vardecls *extravars)
{
    int savedavoid = ctx->navoidids, savedfree = ctx->nfreeids;
    int i;

    ick_js_indent(ctx, indent-1);
    ick_js_write(ctx, "{\n");

    ick_js_declarevars(ctx, &blk->vars, indent);

    if (extravars)
	ick_js_initvars(ctx, extravars, indent);
    ick_js_initvars(ctx, &blk->vars, indent);

    for (i = 0; i < blk->nstmts; i++)
	ick_js_statement(ctx, blk->stmts[i], indent, 0);

    ick_js_popids(ctx, savedavoid, savedfree);

    ick_js_indent(ctx, indent-1);
    ick_js_write(ctx, "}\n");
}

char *ick_to_js(char **mainfunc, ickscript *scr,
		const char **avoidids, int navoidids)
{
    struct ick_js_ctx actx, *ctx = &actx;
    int i, j;

    /*
     * Set up.
     */
    ctx->navoidids = ctx->avoididsize = 0;
    ctx->nfreeids = ctx->freeidsize = 0;
    ctx->outlen = ctx->outsize = 0;
    ctx->avoidids = NULL;
    ctx->freeids = NULL;
    ctx->out = NULL;

    /*
     * Avoid Javascript reserved words and type names.
     */
    {
	static const char *const reserved[] = {
	    /*
	     * Keywords (ECMA-262 section 7.5.2).
	     */
            "break", "case", "catch", "continue", "default",
	    "delete", "do", "else", "finally", "for", "function",
	    "if", "in", "instanceof", "new", "return", "switch",
	    "this", "throw", "try", "typeof", "var", "void",
	    "while", "with",
	    /*
	     * Future reserved words (ECMA-262 section 7.5.3).
	     */
	    "abstract", "boolean", "byte", "char", "class", "const",
	    "debugger", "double", "enum", "export", "extends",
	    "final", "float", "goto", "implements", "import", "int",
	    "interface", "long", "native", "package", "private",
	    "protected", "public", "short", "static", "super",
	    "synchronized", "throws", "transient", "volatile",
	    /*
	     * Standard values (ECMA-262 section 8).
	     */
	    "undefined", "null", "true", "false", "Infinity", "NaN",
	    /*
	     * Built-in functions ("function properties of the
	     * global object", ECMA-262 sections 15.1.2 and
	     * 15.1.3).
	     */
	    "eval", "parseInt", "parseFloat", "isNaN", "isFinite",
	    "decodeURI", "decodeURIComponent",
	    "encodeURI", "encodeURIComponent",
	    /*
	     * Standard type objects ("constructor properties of
	     * the global object", ECMA-262 section 15.1.4).
	     */
	    "Undefined", "Null", "Boolean", "String", "Number", "Object",
	    "Function", "Array", "Date", "RegExp", "Error", "EvalError",
	    "RangeError", "ReferenceError", "SyntaxError", "TypeError",
	    "URIError",
	    /*
	     * Built-in objects ("other properties of the global
	     * object", ECMA-262 section 15.1.5).
	     */
	    "Math",
	    /*
	     * Additional nonstandard properties of the global
	     * object (ECMA-262 appendix B).
	     */
	    "escape", "unescape",
	    /*
	     * Other stuff I've seen defined in the global object
	     * which isn't mentioned anywhere in ECMA-262.
	     */
	    "System",
	};

	for (i = 0; i < lenof(reserved); i++)
	    ick_js_addid(ctx, reserved[i], 0);
    }

    /*
     * Avoid anything the user asked us to avoid.
     */
    for (i = 0; i < navoidids; i++)
	ick_js_addid(ctx, avoidids[i], 0);

    /*
     * Think up names for functions. Note in particular that the
     * name of the main function is the very first thing we think
     * up a name for, so unless the user has been gormless by
     * allocating a main function name that clashes with a
     * Javascript reserved ID or something in their own avoid list
     * (which we enforce by assertion that they haven't), they
     * will be guaranteed to get the main function name they
     * wanted.
     */
    for (i = 0; i < scr->tree->nfns; i++) {
	char *identifier = scr->tree->fns[i].identifier;
	if (i == 0 && *mainfunc)
	    identifier = *mainfunc;
	scr->tree->fns[i].jsname =
	    ick_js_makename(ctx, identifier);
	if (i == 0 && !*mainfunc) {
	    *mainfunc = dupstr(scr->tree->fns[i].jsname);
	}
    }

    /*
     * Generate top-level variable declarations (includes thinking
     * up their names, avoiding all identifiers we've so far
     * defined).
     */
    ick_js_declarevars(ctx, &scr->tree->toplevel_vars, 0);

    /*
     * Define functions.
     */
    for (i = 0; i < scr->tree->nfns; i++) {
	struct ick_fndecl *fn = &scr->tree->fns[i];
	int savedavoid = ctx->navoidids, savedfree = ctx->nfreeids;

	/*
	 * Think up names for the function's parameters.
	 */
	ick_js_namevars(ctx, &fn->params);

	/*
	 * Write out the function prototype.
	 */
	ick_js_write(ctx, "function ");
	ick_js_write(ctx, fn->jsname);
	ick_js_write(ctx, "(");
	for (j = 0; j < fn->params.ndecls; j++) {
	    if (j)
		ick_js_write(ctx, ",");
	    ick_js_write(ctx, fn->params.decls[j].jsname);
	}
	ick_js_write(ctx, ")\n");

	/*
	 * Write out the function body block. Don't forget to have
	 * the main function also initialise the global variables.
	 */
	ick_js_block(ctx, fn->defn, 1,
		     i == 0 ? &scr->tree->toplevel_vars : NULL);

	ick_js_write(ctx, "\n");

	ick_js_popids(ctx, savedavoid, savedfree);
    }

    /*
     * Since we must have had _some_ main function or the parse
     * tree would never have been valid, we can be confident we
     * have produced _some_ output.
     */
    assert(ctx->out != NULL);

    /*
     * Clean up.
     */
    ick_js_popids(ctx, 0, 0);
    sfree((char **)ctx->avoidids);
    sfree((char **)ctx->freeids);

    return ctx->out;
}

/* ----------------------------------------------------------------------
 * Miscellaneous boilerplate stuff.
 */

void ick_free(ickscript *scr)
{
    if (!scr)
	return;

    sfree((char **)scr->stringlits);
    sfree(scr->insns);
    ick_free_parsetree(scr->tree);
    sfree(scr);
}

/*
 * Convenience versions of ick_compile().
 */
static int ick_read_fp(void *vctx)
{
    FILE *fp = (FILE *)vctx;
    return fgetc(fp);
}
ickscript *ick_compile_fp(char **err,
			  const char *mainfuncname, const char *mainfuncproto,
			  icklib *lib, FILE *fp)
{
    return ick_compile(err, mainfuncname, mainfuncproto, lib, ick_read_fp, fp);
}
struct ick_read_str_ctx {
    const char *script;
    int pos, len;
};
static int ick_read_str(void *vctx)
{
    struct ick_read_str_ctx *ctx = (struct ick_read_str_ctx *)vctx;
    if (ctx->pos < ctx->len)
	return (unsigned char)(ctx->script[ctx->pos++]);
    else
	return -1;
}
ickscript *ick_compile_str(char **err,
			   const char *mainfuncname, const char *mainfuncproto,
			   icklib *lib, const char *script)
{
    struct ick_read_str_ctx ctx;
    ctx.script = script;
    ctx.pos = 0;
    ctx.len = strlen(script);
    return ick_compile(err, mainfuncname, mainfuncproto,
		       lib, ick_read_str, &ctx);
}

/*
 * Outer variadic and unlimited forms of the execution function.
 */
int ick_exec(void *result, ickscript *script, ...)
{
    int ret;
    va_list ap;
    va_start(ap, script);
    ret = ick_exec_limited_v(result, 0, 0, 0, script, ap);
    va_end(ap);
    return ret;
}
int ick_exec_limited(void *result, int a, int b, int c, ickscript *script, ...)
{
    int ret;
    va_list ap;
    va_start(ap, script);
    ret = ick_exec_limited_v(result, a, b, c, script, ap);
    va_end(ap);
    return ret;
}
int ick_exec_v(void *result, ickscript *script, va_list ap)
{
    return ick_exec_limited_v(result, 0, 0, 0, script, ap);
}

/*
 * Error message list.
 */
#define ICK_RTE_ERRTEXT_DEF(x,y) y
const char *const ick_runtime_errors[] = {
    ICK_RTE_ERRORLIST(ICK_RTE_ERRTEXT_DEF)
};

/* ----------------------------------------------------------------------
 * Unit tests.
 */

#if defined TEST_LEX || defined TEST_PARSE || defined TEST_COMPILE || \
    defined TEST_EXECUTE || defined TEST_JS || defined TEST_ICKLANG

void platform_fatal_error(const char *err)
{
    fprintf(stderr, "fatal error: %s\n", err);
    exit(1);
}

static const char *const tokennames[] = { TOKENTYPES(TOKENNAME) };

static void dump_block(struct ick_block *blk, int level);

static void dump_expr(struct ick_expr *expr, int level)
{
    int spaces;
    char *prefix;

    printf("%*sexpr node, operator %s", level*4, "", tokennames[expr->op]);
    spaces = 0;
    prefix = ", ";
    if (expr->sval) {
	printf("%*s%swith sval \"%s\"\n", spaces, "", prefix, expr->sval);
	spaces = level*4;
	prefix = "and ";
    }
    if (expr->e1) {
	printf("%*s%swith e1:\n", spaces, "", prefix);
	dump_expr(expr->e1, level+1);
	spaces = level*4;
	prefix = "and ";
    }
    if (expr->e2) {
	printf("%*s%swith e2:\n", spaces, "", prefix);
	dump_expr(expr->e2, level+1);
	spaces = level*4;
	prefix = "and ";
    }
    if (expr->e3) {
	printf("%*s%swith e3:\n", spaces, "", prefix);
	dump_expr(expr->e3, level+1);
	spaces = level*4;
	prefix = "and ";
    }
    if (!spaces) {
	printf(", with ival %d\n", expr->ival);
    }
}

static void dump_statement(struct ick_statement *stmt, int level)
{
    switch (stmt->type) {
      case TOK_KW_RETURN:
	if (stmt->e1) {
	    printf("%*sreturn statement with expression:\n",
		   level*4, "");
	    dump_expr(stmt->e1, level+1);
	} else {
	    printf("%*sreturn statement with no expression\n",
		   level*4, "");
	}
	break;
      case TOK_KW_BREAK:
	printf("%*sbreak statement\n", level*4, "");
	break;
      case TOK_KW_CONTINUE:
	printf("%*scontinue statement\n", level*4, "");
	break;
      case TOK_KW_WHILE:
	printf("%*swhile statement with loop condition:\n", level*4, "");
	dump_expr(stmt->e1, level+1);
	printf("%*sand loop body:\n", level*4, "");
	dump_statement(stmt->s1, level+1);
	break;
      case TOK_KW_DO:
	printf("%*sdo-while statement with loop body:\n", level*4, "");
	dump_statement(stmt->s1, level+1);
	printf("%*sand loop condition:\n", level*4, "");
	dump_expr(stmt->e1, level+1);
	break;
      case TOK_KW_IF:
	printf("%*sif statement with condition:\n", level*4, "");
	dump_expr(stmt->e1, level+1);
	printf("%*sand then clause:\n", level*4, "");
	dump_statement(stmt->s1, level+1);
	if (stmt->s2) {
	    printf("%*sand else clause:\n", level*4, "");
	    dump_statement(stmt->s2, level+1);
	} else {
	    printf("%*sand no else clause\n", level*4, "");
	}
	break;
      case TOK_KW_FOR:
	printf("%*sfor statement with initialisation expression:\n",
	       level*4, "");
	if (stmt->e1)
	    dump_expr(stmt->e1, level+1);
	else
	    printf("%*s(none)\n", level*4+4, "");
	printf("%*sand test expression:\n", level*4, "");
	if (stmt->e3)
	    dump_expr(stmt->e3, level+1);
	else
	    printf("%*s(none)\n", level*4+4, "");
	printf("%*sand increment expression:\n", level*4, "");
	if (stmt->e3)
	    dump_expr(stmt->e3, level+1);
	else
	    printf("%*s(none)\n", level*4+4, "");
	printf("%*sand loop body:\n", level*4, "");
	dump_statement(stmt->s1, level+1);
	break;
      case TOK_LBRACE:
	printf("%*sbraced block statement with block:\n",
	       level*4, "");
	dump_block(stmt->blk, level+1);
	break;
      default:
	printf("%*spure expression statement with expression:\n",
	       level*4, "");
	dump_expr(stmt->e1, level+1);
	break;
    }
}

static void dump_vardecls(struct ick_vardecls *decls, int level)
{
    int i;

    for (i = 0; i < decls->ndecls; i++) {
	printf("%*sVariable: %s, type %s\n", level*4, "",
	       decls->decls[i].identifier, tokennames[decls->decls[i].type]);
	if (decls->decls[i].initialiser)
	    dump_expr(decls->decls[i].initialiser, level+1);
    }
}

static void dump_block(struct ick_block *blk, int level)
{
    int i;

    dump_vardecls(&blk->vars, level);

    for (i = 0; i < blk->nstmts; i++)
	dump_statement(blk->stmts[i], level);
}

static void dump_tree(struct ick_parse_tree *tree)
{
    int i, j;

    dump_vardecls(&tree->toplevel_vars, 0);

    for (i = 0; i < tree->nfns; i++) {
	printf("Function: %s, return type %s, %d param%s\n",
	       tree->fns[i].identifier, tokennames[tree->fns[i].rettype],
	       tree->fns[i].params.ndecls,
	       (tree->fns[i].params.ndecls == 1 ? "" : "s"));
	for (j = 0; j < tree->fns[i].params.ndecls; j++)
	    printf("    Parameter %d: %s, type %s\n",
		   j, tree->fns[i].params.decls[j].identifier,
		   tokennames[tree->fns[i].params.decls[j].type]);
	dump_block(tree->fns[i].defn, 1);
    }
}

#define OPCODE_DUMPDATA(x,a,b,c) { #x, a, b, c }
static const struct opcode_data {
    const char *name, *t1, *t2, *t3;
} opcode_dumpdata[] = { OPCODES(OPCODE_DUMPDATA) };

static void dump_vmcode(struct ickscript *scr)
{
    int i;

    for (i = 0; i < scr->ninsns; i++) {
	const struct opcode_data *od = &opcode_dumpdata[scr->insns[i].opcode];
	printf("p%-5d %-8s", i, od->name);
	if (od->t1)
	    printf("%s%d", od->t1, scr->insns[i].p1);
	if (od->t2) {
	    printf(", %s%d", od->t2, scr->insns[i].p2);
	    if (od->t2[0] == 'l') {
		printf(" \"%s\"", scr->stringlits[scr->insns[i].p2]);
	    }
	}
	if (od->t3)
	    printf(", %s%d", od->t3, scr->insns[i].p3);
	printf("\n");
    }
}

int main(int argc, char **argv)
{
    icklib *lib = ick_lib_new(0);

    if (argc > 1) {
	int i;

	for (i = 1; i < argc; i++) {
	    FILE *fp;
	    ickscript *scr;
	    char *err;

	    fp = fopen(argv[i], "r");
	    if (!fp) {
		perror(argv[i]);
		continue;
	    }
	    scr = ick_compile_fp(&err, "rewrite", "SS", lib, fp);
	    if (scr) {
#if defined TEST_PARSE || defined TEST_ICKLANG
		dump_tree(scr->tree);
#endif
#if defined TEST_COMPILE || defined TEST_ICKLANG
		dump_vmcode(scr);
#endif
#if defined TEST_JS || defined TEST_ICKLANG
		{
		    char *mainfunc = "mainfunc";
		    char *js = ick_to_js(&mainfunc, scr, NULL, 0);
		    fputs(js, stdout);
		    sfree(js);
		}
#endif
#if defined TEST_EXECUTE || defined TEST_ICKLANG
		{
		    char *ret;
		    int rte;
		    rte = ick_exec(&ret, scr, (i+1 < argc ? argv[++i] : ""));
		    if (rte) {
			printf("runtime error: %s\n", ick_runtime_errors[rte]);
		    } else {
			printf("return value: \"%s\"\n", ret);
			sfree(ret);
		    }
		}
#endif
	    } else {
		printf("compile error: %s\n", err);
		sfree(err);
	    }
	    ick_free(scr);
	}
    } else {
	static const char *const testscripts[] = {
	    /* Tests for lex errors. */
	    "string test(void) { return \"ab\\k\"; }",
	    /* Tests for parse errors. */
	    "string test(void) { return false ? \"wha?\" ; 12345; }",
	    "bool f(bool a, bool b, bool c) { return a && b || c; }",
	    "string test(string x) { return index(\"abc\", x; }",
	    "string test(string x) { return (\"abc\"+\"d\"; }",
	    "string test(string x) { return ~; }",
	    "bool test(void) { 1; return false }",
	    "bool test(void) { 1; break }",
	    "bool test(void) { 1; continue }",
	    "bool test(void) { 1; while }",
	    "bool test(void) { 1; while (1 }",
	    "bool test(void) { 1; if }",
	    "bool test(void) { 1; if (1 }",
	    "bool test(void) { 1; if (1 }",
	    "bool test(void) { 1; for }",
	    "bool test(void) { 1; for (1 }",
	    "bool test(void) { 1; for (1; true }",
	    "bool test(void) { 1; for (1; true; 2 }",
	    "bool test(void) { 1; do 1; }",
	    "bool test(void) { 1; do 1; while }",
	    "bool test(void) { 1; do 1; while (1 }",
	    "bool test(void) { 1; do 1; while (1) }",
	    "bool test(void) { 1; 2 }",
	    "bool test(void) { void a; }",
	    "bool test(void) { int; }",
	    "bool test(void",
	    "bool test(!) { return false; }",
	    "bool test(int !) { return false; }",
	    "bool test(int a!) { return false; }",
	    "bool test(int a) !",
	    "void b;",
	    "void ;",
	    "int a",
	    "int",
	    "!",
	    /* Tests for semantic analysis errors. */
	    "string test() { return x; }",
	    "string test() { return x(); }",
	    "string test() { return x(1,false,\"ooh\"); }",
	    "string test() { 1++; }",
	    "string test() { ++2; }",
	    "string test() { 4 = 5; }",
	    "string test() { \"1234\" -= 5; }",
	    "string test() { return 1 ? \"hi\" : \"there\"; }",
	    "string test() { return true ? \"hi\" : 20; }",
	    "string test() { int a; a = \"erm\"; }",
	    "string test() { int a; a += \"erm\"; }",
	    "string test() { int a; a -= \"erm\"; }",
	    "string test() { int a; a &&= \"erm\"; }",
	    "string test() { int a; a && \"erm\"; }",
	    "string test() { return !\"erm\"; }",
	    "string test() { return 1 < \"1\"; }",
	    "void a() {} void b() {} string test() { return a()==2; }",
	    "void a() {} void b() {} string test() { return a()==b(); }",
	    "string test() { return +\"hello\"; }",
	    "string test() { return \"hello\" + 2; }",
	    "string test() { return -\"hello\"; }",
	    "string test() { return \"hello\" - \"o\"; }",
	    "string test() { return \"hello\" * 2; }",
	    "string test() { break; }",
	    "string test() { continue; }",
	    "void a() { return \"ooh\"; } string test() { }",
	    "string test() { return; }",
	    "string test() { return false; }",
	    "string test() { while (1) return \"a\"; }",
	    "string test() { if (1) return \"a\"; }",
	    "string test() { for (1; 2; 3) return \"a\"; }",
	    "bool test() { return false; }",
	    "string test(int x) { return chr(x); }",
	    /*
	     * Basic simple test.
	     */
	    "string test() { return \"hello, world\"; }",
	    /*
	     * Test of all lexical elements.
	     */
            "string test(void)\n"
            "{\n"
            "    int a = 2, b = 3, c = 4, d = 5, e = 6;\n"
	    "    // line comment\n"
            "    int x;\n"
	    "    /* multiline\n"
	    "    /* non-nesting comment */\n"
            "    bool p, q;\n"
	    "    p = q = false;\n"
	    "    //* just to see if you're paying attention */ ~~~\n"
	    "    a += 2, a -= 2, b *= 5, b /= 5;\n"
            "    x = c / a + b * d - e;\n"
            "    while (a > 0) {\n"
            "        x = -x;\n"
            "        --a;\n"
	    "        continue;\n"
            "    }\n"
            "    do {\n"
            "        x = -x;\n"
            "        b--;\n"
	    "        if (q) break;\n"
            "    } while (b > 0);\n"
	    "    x = -x;\n"
	    "    for (;;) break;\n"
            "    for (a = 0; a < c; a++)\n"
            "        x = p ? 12345 : -x;\n"
            "    p = x >= 11 && x > 0xA && x <= 013 && x < 12 && x == 11 && true;\n"
            "    q = x >= 12 || x > 11 || x <= 10 || x < 11 || x != 11 || false;\n"
	    "    p &&= !q;\n"
	    "    q ||= !p;\n"
            "    if (p && !q)\n"
            "        return \"ok\";\n"
            "    else\n"
            "        return \"arrgh\";\n"
            "}\n",
	    /*
	     * Test of the standard library.
	     */
	    "int passes = 0, fails = 0;\n"
            "void testi(int a, int b)\n"
	    "{ if (a == b) passes++; else fails++; }\n"
            "void tests(string a, string b)\n"
	    "{ if (a == b) passes++; else fails++; }\n"
	    "string test(void) {\n"
	    "    testi(len(\"\"), 0);\n"
	    "    testi(len(\"a\"), 1);\n"
	    "    testi(len(\"abcdefghij\"), 10);\n"
	    "    tests(substr(\"abcde\",-3,-1), \"\");\n"
	    "    tests(substr(\"abcde\",-1,2), \"ab\");\n"
	    "    tests(substr(\"abcde\",0,2), \"ab\");\n"
	    "    tests(substr(\"abcde\",3,5), \"de\");\n"
	    "    tests(substr(\"abcde\",3,6), \"de\");\n"
	    "    tests(substr(\"abcde\",6,8), \"\");\n"
	    "    tests(substr(\"abcde\",3,3), \"\");\n"
	    "    tests(substr(\"abcde\",-1), \"abcde\");\n"
	    "    tests(substr(\"abcde\",0), \"abcde\");\n"
	    "    tests(substr(\"abcde\",4), \"e\");\n"
	    "    tests(substr(\"abcde\",5), \"\");\n"
	    "    tests(substr(\"abcde\",6), \"\");\n"
	    "    testi(atoi(\"0\"), 0);\n"
	    "    testi(atoi(\"2147483647\"), 2147483647);\n"
	    "    testi(atoi(\"-2147483648\"), -2147483648);\n"
	    "    testi(atoi(\"11.6\"), 11);\n"
	    "    testi(atoi(\"-11.6\"), -11);\n"
	    "    tests(itoa(0), \"0\");\n"
	    "    tests(itoa(100), \"100\");\n"
	    "    tests(itoa(2147483647), \"2147483647\");\n"
	    "    tests(itoa(-2147483648), \"-2147483648\");\n"
	    "    testi(ord(\"A\"), 65);\n"
	    "    testi(ord(\"BA\"), 66);\n"
	    "    testi(ord(\"\"), 0);\n"
	    "    tests(chr(65), \"A\");\n"
	    "    tests(chr(97), \"a\");\n"
	    "    testi(index(\"ababc\", \"abc\"), 2);\n"
	    "    testi(index(\"ababc\", \"abd\"), -1);\n"
	    "    testi(index(\"ababc\", \"ababcd\"), -1);\n"
	    "    testi(index(\"ababc\", \"abc\", -1), 2);\n"
	    "    testi(index(\"ababc\", \"abc\", 0), 2);\n"
	    "    testi(index(\"ababc\", \"abc\", 1), 2);\n"
	    "    testi(index(\"ababc\", \"abc\", 2), 2);\n"
	    "    testi(index(\"ababc\", \"abc\", 3), -1);\n"
	    "    testi(index(\"ababc\", \"abc\", 4), -1);\n"
	    "    testi(index(\"ababc\", \"abc\", 5), -1);\n"
	    "    testi(index(\"ababc\", \"abc\", 6), -1);\n"
	    "    testi(index(\"abababa\", \"ababa\"), 0);\n"
	    "    testi(rindex(\"ababc\", \"abc\"), 2);\n"
	    "    testi(rindex(\"ababc\", \"abd\"), -1);\n"
	    "    testi(rindex(\"ababc\", \"ababcd\"), -1);\n"
	    "    testi(rindex(\"ababc\", \"abc\", -1), -1);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 0), -1);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 1), -1);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 2), 2);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 3), 2);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 4), 2);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 5), 2);\n"
	    "    testi(rindex(\"ababc\", \"abc\", 6), 2);\n"
	    "    testi(rindex(\"abababa\", \"ababa\"), 2);\n"
	    "    testi(min(-2, -100), -100);\n"
	    "    testi(min(-2, 100), -2);\n"
	    "    testi(max(-2, -100), -2);\n"
	    "    testi(max(-2, 100), 100);\n"
	    "    return itoa(passes+fails) + \" tests, \" +\n"
	    "        itoa(passes) + \" passed, \" +\n"
	    "        itoa(fails) + \" failed\";\n"
	    "}\n",
	};
	int i;

	for (i = 0; i < (int)lenof(testscripts); i++) {
	    ickscript *scr;
	    char *err;
	    printf("test program %d:\n", i);
	    scr = ick_compile_str(&err, "test", "S", lib, testscripts[i]);
	    if (scr) {
#if defined TEST_PARSE || defined TEST_ICKLANG
		dump_tree(scr->tree);
#endif
#if defined TEST_COMPILE || defined TEST_ICKLANG
		dump_vmcode(scr);
#endif
#if defined TEST_JS || defined TEST_ICKLANG
		{
		    char *mainfunc = "mainfunc";
		    char *js = ick_to_js(&mainfunc, scr, NULL, 0);
		    fputs(js, stdout);
		    sfree(js);
		}
#endif
#if defined TEST_EXECUTE || defined TEST_ICKLANG
		{
		    char *ret;
		    int rte;
		    rte = ick_exec(&ret, scr);
		    if (rte) {
			printf("runtime error: %s\n", ick_runtime_errors[rte]);
		    } else {
			printf("return value: \"%s\"\n", ret);
			sfree(ret);
		    }
		}
#endif
	    } else {
		printf("compile error: %s\n", err);
		sfree(err);
	    }
	    ick_free(scr);
	}
    }

    ick_lib_free(lib);

    return 0;
}

#endif
