# Python module to lex C. A bit ambitious, but sounds like fun :-)

import re
import sys

# Base code so I can declare a bunch of enumerations
_enumval = 0
def _enum(init=-1):
    global _enumval
    if init >= 0:
        _enumval = init
    else:
        _enumval = _enumval + 1
    return _enumval

# Enumeration of lexeme types
lt_UNUSED = _enum(0)
lt_comment = _enum()
lt_white = _enum()
lt_ident = _enum()
lt_typedefname = _enum()
lt_charconst = _enum()
lt_stringconst = _enum()
lt_intconst = _enum()
lt_floatconst = _enum()
lt_pragma = _enum()
lt_lbracket = _enum()
lt_rbracket = _enum()
lt_lparen = _enum()
lt_rparen = _enum()
lt_lbrace = _enum()
lt_rbrace = _enum()
lt_dot = _enum()
lt_arrow = _enum()
lt_increment = _enum()
lt_decrement = _enum()
lt_bitand = _enum()
lt_times = _enum()
lt_plus = _enum()
lt_minus = _enum()
lt_bitnot = _enum()
lt_lognot = _enum()
lt_slash = _enum()
lt_modulo = _enum()
lt_lshift = _enum()
lt_rshift = _enum()
lt_less = _enum()
lt_greater = _enum()
lt_lesseq = _enum()
lt_greateq = _enum()
lt_equality = _enum()
lt_notequal = _enum()
lt_bitxor = _enum()
lt_bitor = _enum()
lt_logand = _enum()
lt_logor = _enum()
lt_query = _enum()
lt_colon = _enum()
lt_semi = _enum()
lt_ellipsis = _enum()
lt_assign = _enum()
lt_timeseq = _enum()
lt_slasheq = _enum()
lt_moduloeq = _enum()
lt_pluseq = _enum()
lt_minuseq = _enum()
lt_lshifteq = _enum()
lt_rshifteq = _enum()
lt_bitandeq = _enum()
lt_bitxoreq = _enum()
lt_bitoreq = _enum()
lt_comma = _enum()
lt_hash = _enum()
lt_hashhash = _enum()
lt_auto = _enum()
lt_break = _enum()
lt_case = _enum()
lt_char = _enum()
lt_const = _enum()
lt_continue = _enum()
lt_default = _enum()
lt_do = _enum()
lt_double = _enum()
lt_else = _enum()
lt_enum = _enum()
lt_extern = _enum()
lt_float = _enum()
lt_for = _enum()
lt_goto = _enum()
lt_if = _enum()
lt_inline = _enum()
lt_int = _enum()
lt_long = _enum()
lt_register = _enum()
lt_restrict = _enum()
lt_return = _enum()
lt_short = _enum()
lt_signed = _enum()
lt_sizeof = _enum()
lt_static = _enum()
lt_struct = _enum()
lt_switch = _enum()
lt_typedef = _enum()
lt_union = _enum()
lt_unsigned = _enum()
lt_void = _enum()
lt_volatile = _enum()
lt_while = _enum()
lt__Bool = _enum()
lt__Complex = _enum()
lt__Imaginary = _enum()

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
        self.typedefs = {}
        self.line = 1
        self.col = 1
        self.pushback = []

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
        # FIXME: do this bit

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
            sys.stdout.write(lex.text)
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
                elif self.typedefs.has_key(matchtext):
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
