# Python module to lex C. A bit ambitious, but sounds like fun :-)

import re
import sys
import string

# Base code so I can declare a bunch of enumerations
_enumval = 0
def _enum(init=-1):
    global _enumval
    if init >= 0:
        _enumval = init
    else:
        _enumval = _enumval + 1
    return _enumval

lex_names = {}

# Enumeration of lexeme types
lt_BEGIN = _enum(0)
lt_comment = _enum()          ; lex_names[lt_comment] = "comment"
lt_white = _enum()            ; lex_names[lt_white] = "whitespace"
lt_ident = _enum()            ; lex_names[lt_ident] = "identifier"
lt_typedefname = _enum()      ; lex_names[lt_typedefname] = "typedef name"
lt_charconst = _enum()        ; lex_names[lt_charconst] = "char constant"
lt_stringconst = _enum()      ; lex_names[lt_stringconst] = "string constant"
lt_intconst = _enum()         ; lex_names[lt_intconst] = "int constant"
lt_floatconst = _enum()       ; lex_names[lt_floatconst] = "float constant"
lt_pragma = _enum()           ; lex_names[lt_pragma] = "pragma"
lt_lbracket = _enum()         ; lex_names[lt_lbracket] = "`['"
lt_rbracket = _enum()         ; lex_names[lt_rbracket] = "`]'"
lt_lparen = _enum()           ; lex_names[lt_lparen] = "`('"
lt_rparen = _enum()           ; lex_names[lt_rparen] = "`)'"
lt_lbrace = _enum()           ; lex_names[lt_lbrace] = "`{'"
lt_rbrace = _enum()           ; lex_names[lt_rbrace] = "`}'"
lt_dot = _enum()              ; lex_names[lt_dot] = "`,'"
lt_arrow = _enum()            ; lex_names[lt_arrow] = "`->'"
lt_increment = _enum()        ; lex_names[lt_increment] = "`++'"
lt_decrement = _enum()        ; lex_names[lt_decrement] = "`--'"
lt_bitand = _enum()           ; lex_names[lt_bitand] = "`&'"
lt_times = _enum()            ; lex_names[lt_times] = "`*'"
lt_plus = _enum()             ; lex_names[lt_plus] = "`+'"
lt_minus = _enum()            ; lex_names[lt_minus] = "`-'"
lt_bitnot = _enum()           ; lex_names[lt_bitnot] = "`~'"
lt_lognot = _enum()           ; lex_names[lt_lognot] = "`!'"
lt_slash = _enum()            ; lex_names[lt_slash] = "`/'"
lt_modulo = _enum()           ; lex_names[lt_modulo] = "`%'"
lt_lshift = _enum()           ; lex_names[lt_lshift] = "`<<'"
lt_rshift = _enum()           ; lex_names[lt_rshift] = "`>>'"
lt_less = _enum()             ; lex_names[lt_less] = "`<'"
lt_greater = _enum()          ; lex_names[lt_greater] = "`>'"
lt_lesseq = _enum()           ; lex_names[lt_lesseq] = "`<='"
lt_greateq = _enum()          ; lex_names[lt_greateq] = "`>='"
lt_equality = _enum()         ; lex_names[lt_equality] = "`=='"
lt_notequal = _enum()         ; lex_names[lt_notequal] = "`!='"
lt_bitxor = _enum()           ; lex_names[lt_bitxor] = "`^'"
lt_bitor = _enum()            ; lex_names[lt_bitor] = "`|'"
lt_logand = _enum()           ; lex_names[lt_logand] = "`&&'"
lt_logor = _enum()            ; lex_names[lt_logor] = "`||'"
lt_query = _enum()            ; lex_names[lt_query] = "`?'"
lt_colon = _enum()            ; lex_names[lt_colon] = "`:'"
lt_semi = _enum()             ; lex_names[lt_semi] = "`;'"
lt_ellipsis = _enum()         ; lex_names[lt_ellipsis] = "`...'"
lt_assign = _enum()           ; lex_names[lt_assign] = "`='"
lt_timeseq = _enum()          ; lex_names[lt_timeseq] = "`*='"
lt_slasheq = _enum()          ; lex_names[lt_slasheq] = "`/='"
lt_moduloeq = _enum()         ; lex_names[lt_moduloeq] = "`%='"
lt_pluseq = _enum()           ; lex_names[lt_pluseq] = "`+='"
lt_minuseq = _enum()          ; lex_names[lt_minuseq] = "`-='"
lt_lshifteq = _enum()         ; lex_names[lt_lshifteq] = "`<<='"
lt_rshifteq = _enum()         ; lex_names[lt_rshifteq] = "`>>='"
lt_bitandeq = _enum()         ; lex_names[lt_bitandeq] = "`&='"
lt_bitxoreq = _enum()         ; lex_names[lt_bitxoreq] = "`^='"
lt_bitoreq = _enum()          ; lex_names[lt_bitoreq] = "`|='"
lt_comma = _enum()            ; lex_names[lt_comma] = "`,'"
lt_hash = _enum()             ; lex_names[lt_hash] = "`#'"
lt_hashhash = _enum()         ; lex_names[lt_hashhash] = "`##'"
lt_auto = _enum()             ; lex_names[lt_auto] = "`auto'"
lt_break = _enum()            ; lex_names[lt_break] = "`break'"
lt_case = _enum()             ; lex_names[lt_case] = "`case'"
lt_char = _enum()             ; lex_names[lt_char] = "`char'"
lt_const = _enum()            ; lex_names[lt_const] = "`const'"
lt_continue = _enum()         ; lex_names[lt_continue] = "`continue'"
lt_default = _enum()          ; lex_names[lt_default] = "`default'"
lt_do = _enum()               ; lex_names[lt_do] = "`do'"
lt_double = _enum()           ; lex_names[lt_double] = "`double'"
lt_else = _enum()             ; lex_names[lt_else] = "`else'"
lt_enum = _enum()             ; lex_names[lt_enum] = "`enum'"
lt_extern = _enum()           ; lex_names[lt_extern] = "`extern'"
lt_float = _enum()            ; lex_names[lt_float] = "`float'"
lt_for = _enum()              ; lex_names[lt_for] = "`for'"
lt_goto = _enum()             ; lex_names[lt_goto] = "`goto'"
lt_if = _enum()               ; lex_names[lt_if] = "`if'"
lt_inline = _enum()           ; lex_names[lt_inline] = "`inline'"
lt_int = _enum()              ; lex_names[lt_int] = "`int'"
lt_long = _enum()             ; lex_names[lt_long] = "`long'"
lt_register = _enum()         ; lex_names[lt_register] = "`register'"
lt_restrict = _enum()         ; lex_names[lt_restrict] = "`restrict'"
lt_return = _enum()           ; lex_names[lt_return] = "`return'"
lt_short = _enum()            ; lex_names[lt_short] = "`short'"
lt_signed = _enum()           ; lex_names[lt_signed] = "`signed'"
lt_sizeof = _enum()           ; lex_names[lt_sizeof] = "`sizeof'"
lt_static = _enum()           ; lex_names[lt_static] = "`static'"
lt_struct = _enum()           ; lex_names[lt_struct] = "`struct'"
lt_switch = _enum()           ; lex_names[lt_switch] = "`switch'"
lt_typedef = _enum()          ; lex_names[lt_typedef] = "`typedef'"
lt_union = _enum()            ; lex_names[lt_union] = "`union'"
lt_unsigned = _enum()         ; lex_names[lt_unsigned] = "`unsigned'"
lt_void = _enum()             ; lex_names[lt_void] = "`void'"
lt_volatile = _enum()         ; lex_names[lt_volatile] = "`volatile'"
lt_while = _enum()            ; lex_names[lt_while] = "`while'"
lt__Bool = _enum()            ; lex_names[lt__Bool] = "`_Bool'"
lt__Complex = _enum()         ; lex_names[lt__Complex] = "`_Complex'"
lt__Imaginary = _enum()       ; lex_names[lt__Imaginary] = "`_Imaginary'"
lt_END = _enum()

# Keywords
c_keywords = {}
c_keywords["auto"] = lt_auto
c_keywords["break"] = lt_break
c_keywords["case"] = lt_case
c_keywords["char"] = lt_char
c_keywords["const"] = lt_const
c_keywords["continue"] = lt_continue
c_keywords["default"] = lt_default
c_keywords["do"] = lt_do
c_keywords["double"] = lt_double
c_keywords["else"] = lt_else
c_keywords["enum"] = lt_enum
c_keywords["extern"] = lt_extern
c_keywords["float"] = lt_float
c_keywords["for"] = lt_for
c_keywords["goto"] = lt_goto
c_keywords["if"] = lt_if
c_keywords["inline"] = lt_inline
c_keywords["int"] = lt_int
c_keywords["long"] = lt_long
c_keywords["register"] = lt_register
c_keywords["restrict"] = lt_restrict
c_keywords["return"] = lt_return
c_keywords["short"] = lt_short
c_keywords["signed"] = lt_signed
c_keywords["sizeof"] = lt_sizeof
c_keywords["static"] = lt_static
c_keywords["struct"] = lt_struct
c_keywords["switch"] = lt_switch
c_keywords["typedef"] = lt_typedef
c_keywords["union"] = lt_union
c_keywords["unsigned"] = lt_unsigned
c_keywords["void"] = lt_void
c_keywords["volatile"] = lt_volatile
c_keywords["while"] = lt_while
c_keywords["_Bool"] = lt__Bool
c_keywords["_Complex"] = lt__Complex
c_keywords["_Imaginary"] = lt__Imaginary

# Punctuators to token types mapping
c_punctuators = { \
"[": lt_lbracket, "<:": lt_lbracket, "]": lt_rbracket, ":>": lt_rbracket, \
"(": lt_lparen, ")": lt_rparen, "{": lt_lbrace, "<%": lt_lbrace, \
"}": lt_rbrace, "%>": lt_rbrace, ".": lt_dot, "->": lt_arrow, \
"++": lt_increment, "--": lt_decrement, "&": lt_bitand, "*": lt_times, \
"+": lt_plus, "-": lt_minus, "~": lt_bitnot, "!": lt_lognot, "/": lt_slash, \
"%": lt_modulo, "<<": lt_lshift, ">>": lt_rshift, "<": lt_less, \
">": lt_greater, "<=": lt_lesseq, ">=": lt_greateq, "==": lt_equality, \
"!=": lt_notequal, "^": lt_bitxor, "|": lt_bitor, "&&": lt_logand, \
"||": lt_logor, "?": lt_query, ":": lt_colon, ";": lt_semi, \
"...": lt_ellipsis, "=": lt_assign, "*=": lt_timeseq, "/=": lt_slasheq, \
"%=": lt_moduloeq, "+=": lt_pluseq, "-=": lt_minuseq, "<<=": lt_lshifteq, \
">>=": lt_rshifteq, "&=": lt_bitandeq, "^=": lt_bitxoreq, "|=": lt_bitoreq, \
",": lt_comma, "#": lt_hash, "%:": lt_hash, "##": lt_hashhash, \
"%:%:": lt_hashhash }

class lexeme:
    "Class to hold a C lexeme. Members are:"
    "  text - holds the complete text of the lexeme"
    "  type - holds the lexeme type"
    def __init__(self, atype, atext):
        self.type = atype
        self.text = atext

class lexer:
    "C lexer object. You feed it string data and it returns you a stream\n"\
    "of lexemes."

    re_ident = re.compile(r"^[a-zA-Z_][a-zA-Z0-9_]*")
    re_integer = re.compile(r"^([1-9][0-9]*|0[0-7]*|0[xX][0-9A-Fa-f]+)" + \
    r"([uU]?[lL]?[lL]?|[lL]?[lL]?[uU]?)")
    re_float = re.compile(r"^[0-9]*\.[0-9]+([eE][+\-]?[0-9]+)?[flFL]?")
    re_comment = re.compile(r"^/\*[^\*]*\*+([^/\*][^\*]*\*+)*/")
    re_white = re.compile(r"^[ \t\n\r\v\f]+")
    re_char = re.compile(r"^L?'([^'\\\n]|\\(['" + '"' + r"\?\\abfnrtv]|[0-7][0-7]?[0-7]?|x[0-9a-fA-F]+))+'")
    re_str = re.compile(r'^L?"([^"\\\n]|\\(["' + "'" + r'\?\\abfnrtv]|[0-7][0-7]?[0-7]?|x[0-9a-fA-F]+))+"')
    re_punct = re.compile(r"^(\[|\]|{|}|\(|\)|\.|->|\+\+|--|&|\*|\+|-|~|!|/|%|" +
    r"<<|>>|<|>|<=|>=|==|!=|\^|\||&&|\?|:|;|\.\.\.|=|\*=|/=|%=|\+=|-=|<<=|>>=|&=|" +
    r"\^=|\|=|\|\||,|#|##|<:|:>|<%|%>|%:|%:%:)")

    def __init__(self):
        "Create a new lexer."
        self.text = ""
        self.wantcomments = 0
        self.wantwhite = 0
        self.typedefs = [{}]
        self.line = 1
        self.col = 1
        self.pushback = []

    def pushtypedef(self):
        "Push the list of typedef identifiers on a stack."
        self.typedefs[:0] = [self.typedefs[0].copy()]

    def poptypedef(self):
        "Pop the list of typedef identifiers from the stack."
        self.typedefs[:1] = []

    def addtypedef(self, word):
        "Add a typedef name to the list."
        self.typedefs[0][word] = 1

    def feed(self, text):
        "Feed the lexer some text."
        self.text = self.text + text
        # FIXME: here is a good place to do trigraphs

    def hungry(self):
        "The main lexer code calls this when it hits the end of the lexer's\n"\
        "text. You can override hungry() so that it calls feed(). hungry()\n"\
        "should return 0 if it has provided no extra text, or 1 if it has."
        return 0

    def seen(self, text):
        "Called after removing text from the input buffer. Should update\n"\
        "the line and column specifiers."
        while 1:
            i = string.find(text, "\n")
            if i == -1: break
            self.line = self.line + 1
            self.col = 1
            text = text[i+1:]
        while 1:
            i = string.find(text, "\t")
            if i == -1:
                self.col = self.col + len(text)
                break
            else:
                self.col = self.col + i
                self.col = ((self.col - 1) &~ 7) + 8

    def unget(self, lexeme):
        "Return a lexeme to the input."
        self.pushback.append(lexeme)

    def peek(self):
        "Peek at the next lexeme, non-destructively."
        ret = self.get()
        self.unget(ret)
        return ret

    def get(self):
        "Return the next lexeme if one is available, or None if the string\n"\
        "contains no complete lexeme. Note that you should be sure you've\n"\
        "provided enough text, if you have not overridden hungry(): if you\n"\
        "feed '&' and call get(), it will return & even if the next feed()\n"\
        "was going to provide another '&'. It will call hungry() first,\n"\
        "just in case, but if hungry() is not overridden it will just return."

        if self.pushback != []:
            ret = self.pushback[-1]
            self.pushback[-1:] = []
            return ret

        while 1:
            lex = self._realget()
            if lex == None:
                break
            if lex.type == lt_comment and not self.wantcomments:
                continue
            if lex.type == lt_white and not self.wantwhite:
                continue
            break
        return lex

    def _realget(self):
        "The real work of get()"
        for attempt in (1,2):
            matches = [
            lexer.re_ident.search(self.text),
            lexer.re_float.search(self.text),
            lexer.re_integer.search(self.text),
            lexer.re_comment.search(self.text),
            lexer.re_white.search(self.text),
            lexer.re_char.search(self.text),
            lexer.re_str.search(self.text),
            lexer.re_punct.search(self.text)]
            for i in range(len(matches)):
                if matches[i]:
                    matchnum = i
                    match = matches[i]
                    break
            else:
                match = None
            hungry = 0
            if match:
                matchtext = match.group(0)
                if len(matchtext) == len(self.text):
                    hungry = 1
            else:
                hungry = 1
            if hungry and attempt == 1 and self.hungry():
                continue
            else:
                break
        if not match:
            return None
        else:
            matchtext = matches[i].group(0)
            if matchtext == "":
                print "!" + self.text
                Die()
            type = None
            if i == 0:
                if c_keywords.has_key(matchtext):
                    type = c_keywords[matchtext]
                elif self.typedefs[0].has_key(matchtext):
                    type = lt_typedefname
                else:
                    type = lt_ident
            elif i == 1:
                type = lt_floatconst
            elif i == 2:
                type = lt_intconst
            elif i == 3:
                type = lt_comment
            elif i == 4:
                type = lt_white
                if matches[7]:
                    print "<" + self.text + ">"
            elif i == 5:
                type = lt_charconst
            elif i == 6:
                type = lt_stringconst
            elif i == 7:
                if not c_punctuators.has_key(matchtext):
                    Die()
                else:
                    type = c_punctuators[matchtext]
            self.text = self.text[len(matchtext):]
            ret = lexeme(type, matchtext)
            ret.line = self.line
            ret.col = self.col
            self.seen(matchtext)
            return ret
