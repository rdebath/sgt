from c_lex import *

class node:
    "Node in the raw parse tree"
    def __init__(self,type):
        self.type = type
        self.list = []
    def append(self,item):
        self.list.append(item)
    def extend(self,items):
        self.list.extend(items)
    def display(self, level):
        print "  " * level + self.type
        for i in self.list:
            if i == None:
                print "  " * (level+1) + "None"
            elif self.__class__ == i.__class__:
                i.display(level+1)
            else:
                print "  " * (level+1) + "%d" % i.type + " `" + i.text + "'"
    def ret(self):
        self.display(0)
        return self

def parse(lex):
    "C parser. You feed it a lexer and it returns you a parse tree."
    
    class parserclass:
        "Class used to share data between bits of the parser"
        def __init__(self, lex):
            self.lex = lex

        def peek(self):
            return self.lex.peek()

        def peektype(self):
            lexeme = self.lex.peek()
            if lexeme == None:
                return None
            else:
                return lexeme.type

        def get(self):
            return self.lex.get()

        def translation_unit(self):
            n = node("translation_unit")
            while self.peek() != None:
                n.append(self.external_declaration())
            return n.ret()

        def external_declaration(self):
            n = node("external_declaration")
            # function_definition: declaration_specifiers declarator
            #                      [declaration_list] compound_statement
            # declaration: declaration_specifiers init_declarator_list
            # init_declarator_list: init_declarator [, init_declarator_list]
            # init_declarator: declarator [= initializer]
            #
            # Hence: we expect to see declaration_specifiers followed by
            # declarator. If we then see '=' or ',' or ';' we are parsing a
            # declaration, otherwise we are parsing a function_definition.
            ds = self.declaration_specifiers()
            dcl = self.declarator()
            t = self.peektype()
            if t == lt_assign or t == lt_comma or t == lt_semi:
                n.append(self.declaration(ds, dcl))
            else:
                n.append(self.function_definition(ds, dcl))
            return n.ret()

        def declaration(self, ds=None, dcl=None):
            n = node("declaration")
            if ds == None: ds = self.declaration_specifiers()
            n.append(ds)
            if dcl != None or self.peektype() != lt_semi:
                n.append(self.init_declarator_list(dcl))
            self.get() # eat the semicolon
            return n.ret()

        def init_declarator_list(self, dcl=None):
            n = node("init_declarator_list")
            n.append(self.init_declarator(dcl))
            while self.peektype() == lt_comma:
                self.get() # eat the comma
                n.append(self.init_declarator())
            return n.ret()
        
        def init_declarator(self, dcl=None):
            n = node("init_declarator")
            if dcl == None: dcl = self.declarator()
            n.append(dcl)
            if self.peektype() == lt_assign:
                self.get() # eat the =
                n.append(self.initializer())
            return n.ret()

        def declaration_specifiers(self):
            n = node("declaration_specifiers")
            while 1:
                if self.storage_class_qualifier_peek():
                    n.append(self.storage_class_qualifier())
                elif self.function_specifier_peek():
                    n.append(self.function_specifier())
                elif self.type_qualifier_peek():
                    n.append(self.type_qualifier())
                elif self.type_specifier_peek():
                    n.append(self.type_specifier())
                else:
                    break
            if n.list == []:
                ERROR()
            return n.ret()

        _storage_class_qualifiers = { \
        lt_typedef: 1, lt_extern: 1, lt_static: 1, lt_auto: 1, lt_register: 1}
        def storage_class_qualifier_peek(self):
            return self._storage_class_qualifiers.has_key(self.peektype())
        def storage_class_qualifier(self):
            n = node("storage_class_qualifier")
            n.append(self.get())
            return n.ret()

        _type_qualifiers = { lt_const: 1, lt_restrict: 1, lt_volatile: 1 }
        def type_qualifier_peek(self):
            return self._type_qualifiers.has_key(self.peektype())
        def type_qualifier(self):
            n = node("type_qualifier")
            n.append(self.get())
            return n.ret()
        def type_qualifier_list(self):
            n = node("type_qualifier_list")
            while self.type_qualifier_peek():
                n.append(self.type_qualifier())
            return n.ret()

        _function_specifiers = { lt_inline: 1}
        def function_specifier_peek(self):
            return self._function_specifiers.has_key(self.peektype())
        def function_specifier(self):
            n = node("function_specifier")
            n.append(self.get())
            return n.ret()

        _simple_type_specifiers = { lt_void: 1, lt_char: 1, lt_short: 1,
        lt_int: 1, lt_long: 1, lt_float: 1, lt_double: 1, lt_signed: 1,
        lt_unsigned: 1, lt__Bool: 1, lt__Complex: 1, lt__Imaginary: 1,
        lt_typedefname: 1}
        _other_type_specifiers = { lt_struct: 1, lt_union: 1, lt_enum: 1}
        def type_specifier_peek(self):
            if self._simple_type_specifiers.has_key(self.peektype()):
                return 1
            if self._other_type_specifiers.has_key(self.peektype()):
                return 1
        def type_specifier(self):
            n = node("type_specifier")
            t = self.peektype()
            if self._simple_type_specifiers.has_key(t):
                n.append(self.get())
            elif t == lt_enum:
                n.append(self.enum_specifier())
            elif t == lt_struct or t == lt_union:
                n.append(self.struct_or_union_specifier())
            return n.ret()

        def declarator(self):
            n = node("declarator")
            if self.peektype() == lt_times:
                n.append(self.pointer())
            n.append(self.direct_declarator())
            return n.ret()

        def pointer(self):
            n = node("pointer")
            while self.peektype() == lt_times:
                self.get()
                if self.type_qualifier_peek():
                    n.append(self.type_qualifier_list())
                else:
                    n.append(None)
            return n.ret()

        def direct_declarator(self):
            n = node("direct_declarator")
            t = self.peektype()
            if t == lt_ident:
                n.append(self.get())
            elif t == lt_lparen:
                self.get() # eat lparen
                n.append(self.declarator())
                if self.peektype() != lt_rparen:
                    ERROR()
                self.get() # eat rparen

            while 1:
                t = self.peektype()
                if t != lt_lparen and t != lt_lbracket:
                    break
                self.get()
                if t == lt_lparen:
                    # either parameter_type_list or identifer_list
                    if self.peektype() == lt_ident:
                        n.append(self.identifier_list())
                    else:
                        n.append(self.parameter_type_list())
                else:
                    if self.peektype() != lt_rbracket:
                        n.append(self.assignment_expression())
                    else:
                        n.append(None)
                    if self.peektype() != lt_rbracket:
                        ERROR()
                    self.get() # eat rbracket

            return n.ret()

    p = parserclass(lex)
    return p.translation_unit()
