/*
 * yacc description of the Ick language grammar.
 *
 * This description is not used to generate the Ick parser. It's
 * merely documentation of the grammar itself, plus I can feed it
 * through yacc to verify that there are no conflicts and the
 * grammar is unambiguous.
 *
 * (Well, I say `no conflicts'; in fact we expect one shift/reduce
 * conflict from the if-else ambiguity, resolved in favour of shift
 * as usual.)
 */

%token TOK_KW_STRING
%token TOK_KW_INT
%token TOK_KW_BOOL
%token TOK_KW_VOID
%token TOK_KW_RETURN
%token TOK_KW_BREAK
%token TOK_KW_CONTINUE
%token TOK_KW_IF
%token TOK_KW_ELSE
%token TOK_KW_FOR
%token TOK_KW_WHILE
%token TOK_KW_DO
%token TOK_LBRACE
%token TOK_RBRACE
%token TOK_LPAREN
%token TOK_RPAREN
%token TOK_COMMA
%token TOK_SEMI
%token TOK_LT
%token TOK_LE
%token TOK_GT
%token TOK_GE
%token TOK_EQ
%token TOK_NE
%token TOK_QUERY
%token TOK_COLON
%token TOK_ASSIGN
%token TOK_PLUS
%token TOK_MINUS
%token TOK_INCREMENT
%token TOK_DECREMENT
%token TOK_MUL
%token TOK_DIV
%token TOK_LOGAND
%token TOK_LOGOR
%token TOK_LOGNOT
%token TOK_PLUSASSIGN
%token TOK_MINUSASSIGN
%token TOK_MULASSIGN
%token TOK_DIVASSIGN
%token TOK_LOGANDASSIGN
%token TOK_LOGORASSIGN
%token TOK_IDENTIFIER
%token TOK_STRINGLIT
%token TOK_INTLIT
%token TOK_BOOLLIT
%token TOK_EOF

%%

script         : topleveldecls TOK_EOF
               ;

topleveldecls  : topleveldecl
               | topleveldecls topleveldecl
	       ;

topleveldecl   : vardecl
               | funcdefn
	       ;

funcdefn       : type TOK_IDENTIFIER
                   TOK_LPAREN funcparams TOK_RPAREN bracedblock
               ;

funcparams     : /* nothing */
               | TOK_KW_VOID
               | somefuncparams
	       ;

somefuncparams : funcparam
               | somefuncparams TOK_COMMA funcparam
	       ;

funcparam      : type TOK_IDENTIFIER
               ;

type           : TOK_KW_STRING
               | TOK_KW_INT
	       | TOK_KW_BOOL
	       | TOK_KW_VOID
	       ;

bracedblock    : TOK_LBRACE vardecls statements TOK_RBRACE
               ;

vardecls       : /* nothing */
               | vardecls vardecl
	       ;

vardecl        : type decls TOK_SEMI
               ;

decls          : decl
               | decls TOK_COMMA decl
	       ;

decl           : TOK_IDENTIFIER
               | TOK_IDENTIFIER TOK_ASSIGN expr0
	       ;

statements     : statement
               | statements statement
	       ;

statement      : expr TOK_SEMI
               | TOK_KW_RETURN TOK_SEMI
               | TOK_KW_RETURN expr TOK_SEMI
               | TOK_KW_BREAK TOK_SEMI
               | TOK_KW_CONTINUE TOK_SEMI
               | bracedblock
               | TOK_KW_IF TOK_LPAREN expr TOK_RPAREN statement
               | TOK_KW_IF TOK_LPAREN expr TOK_RPAREN statement
	           TOK_KW_ELSE statement
	       | TOK_KW_WHILE TOK_LPAREN expr TOK_RPAREN statement
	       | TOK_KW_FOR TOK_LPAREN optexpr TOK_SEMI optexpr TOK_SEMI
	           optexpr TOK_RPAREN statement
	       | TOK_KW_DO statement TOK_KW_WHILE TOK_LPAREN expr TOK_RPAREN
	       ;

optexpr        : /* nothing */
               | expr
	       ;

expr           : expr0
               | expr TOK_COMMA expr0
	       ;

expr0          : expr1 TOK_ASSIGN expr1
               | expr1 TOK_PLUSASSIGN expr1
               | expr1 TOK_MINUSASSIGN expr1
               | expr1 TOK_MULASSIGN expr1
               | expr1 TOK_DIVASSIGN expr1
               | expr1 TOK_LOGANDASSIGN expr1
               | expr1 TOK_LOGORASSIGN expr1
               | expr1
	       ;

expr1          : expr2
               | expr2 TOK_QUERY expr TOK_COLON expr1
	       ;

expr2          : andexpr2
               | orexpr2
	       | expr3
	       ;

andexpr2       : expr3 TOK_LOGAND expr3
               | andexpr2 TOK_LOGAND expr3
	       ;

orexpr2        : expr3 TOK_LOGOR expr3
               | orexpr2 TOK_LOGOR expr3
	       ;

expr3          : expr4
               | expr3 TOK_LT expr4
               | expr3 TOK_LE expr4
               | expr3 TOK_GT expr4
               | expr3 TOK_GE expr4
               | expr3 TOK_EQ expr4
               | expr3 TOK_NE expr4
	       ;

expr4          : expr5
               | expr4 TOK_PLUS expr5
               | expr4 TOK_MINUS expr5
	       ;

expr5          : expr6
               | expr5 TOK_MUL expr6
               | expr5 TOK_DIV expr6
	       ;

expr6          : TOK_MINUS expr6
               | TOK_PLUS expr6
               | TOK_LOGNOT expr6
	       | TOK_INCREMENT expr7
	       | TOK_DECREMENT expr7
	       | expr7 TOK_INCREMENT
	       | expr7 TOK_DECREMENT
	       | expr7
	       ;

expr7          : TOK_STRINGLIT
               | TOK_INTLIT
	       | TOK_BOOLLIT
	       | TOK_IDENTIFIER
	       | TOK_IDENTIFIER TOK_LPAREN funcargs TOK_RPAREN
	       | TOK_LPAREN expr TOK_RPAREN
	       ;

funcargs       : /* nothing */
               | somefuncargs
	       ;

somefuncargs   : expr1
               | somefuncargs TOK_COMMA expr0
	       ;
