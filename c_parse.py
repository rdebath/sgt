from c_lex import *

# Base code so I can declare a bunch of enumerations
_enumval = 0
def _enum(init=-1):
    global _enumval
    if init >= 0:
        _enumval = init
    else:
        _enumval = _enumval + 1
    return _enumval

pt_BEGIN = _enum(lt_END) # prevent type code overlap
pt_translation_unit = _enum()
pt_function_definition = _enum()
pt_declaration_list = _enum()
pt_declaration = _enum()
pt_init_declarator_list = _enum()
pt_init_declarator = _enum()
pt_declaration_specifiers = _enum()
pt_storage_class_qualifier = _enum()
pt_type_qualifier = _enum()
pt_type_qualifier_list = _enum()
pt_function_specifier = _enum()
pt_type_specifier = _enum()
pt_specifier_qualifier_list = _enum()
pt_struct_or_union_specifier = _enum()
pt_struct_declaration_list = _enum()
pt_struct_declaration = _enum()
pt_struct_declarator_list = _enum()
pt_struct_declarator = _enum()
pt_enum_specifier = _enum()
pt_enumerator_list = _enum()
pt_enumerator = _enum()
pt_parameter_type_list = _enum()
pt_parameter_declaration = _enum()
pt_identifier_list = _enum()
pt_initializer = _enum()
pt_initializer_list = _enum()
pt_designation = _enum()
pt_designator = _enum()
pt_type_name = _enum()
pt_declarator = _enum()
pt_pointer = _enum()
pt_direct_declarator = _enum()
pt_null_statement = _enum()
pt_compound_statement = _enum()
pt_labeled_statement = _enum()
pt_selection_statement = _enum()
pt_iteration_statement = _enum()
pt_jump_statement = _enum()
pt_primary_expression = _enum()
pt_postfix_expression = _enum()
pt_argument_expression_list = _enum()
pt_unary_expression = _enum()
pt_cast_expression = _enum()
pt_binary_expression = _enum()
pt_conditional_expression = _enum()
pt_assignment_expression = _enum()
pt_constant_expression = _enum()
pt_END = _enum()

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
        print "  " * level + repr(self.type)
        for i in self.list:
            if i == None:
                print "  " * (level+1) + "None"
            elif self.__class__ == i.__class__:
                i.display(level+1)
            else:
                print "  " * (level+1) + "%d" % i.type + " `" + i.text + "'"

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
        def unget(self, token):
            self.lex.unget(token)
        def eat(self, type):
            if self.peektype() == type:
                return self.lex.get()
            else:
                ERROR()

        def translation_unit(self):
            n = node(pt_translation_unit)
            while self.peek() != None:
                n.append(self.external_declaration())
            return n

        def external_declaration(self):
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
            dcl = self.declarator(1) # non-abstract only
            t = self.peektype()
            if t == lt_assign or t == lt_comma or t == lt_semi:
                n = self.declaration(ds, dcl)
            else:
                n = self.function_definition(ds, dcl)
            return n

        def function_definition(self, ds=None, dcl=None):
            n = node(pt_function_definition)
            if ds == None: ds = self.declaration_specifiers()
            n.append(ds)
            if dcl == None: dcl = self.declarator(1) # non-abstract only
            n.append(dcl)
            if self.peektype() != lt_lbrace:
                n.append(self.declaration_list())
            else:
                n.append(None)
            n.append(self.compound_statement())
            return n

        def declaration_list(self):
            # We know a declaration_list is terminated by an open brace,
            # because it only occurs just before the compound_statement
            # in an old style function_definition. This saves us a lot
            # of parser-theoretic hassle :-)
            n = node(pt_declaration_list)
            while self.peektype() != lt_lbrace:
                n.append(self.declaration())
            return n

        def declaration(self, ds=None, dcl=None):
            n = node(pt_declaration)
            if ds == None: ds = self.declaration_specifiers()
            n.append(ds)
            if dcl != None or self.peektype() != lt_semi:
                n.append(self.init_declarator_list(dcl))
            self.eat(lt_semi)
            # Special case: parser-to-lexer feedback. We must notice
            # the `typedef' specifier within ds, if present, and we must
            # enter all names defined in declarators into the lexer's
            # typedef list.
            is_typedef = 0
            for i in ds.list: # declaration specifiers
                if i.type == pt_storage_class_qualifier:
                    if i.list[0].type == lt_typedef:
                        is_typedef = 1
                        break
            if is_typedef:
                for i in n.list[1].list: # init_declarators
                    decl = i.list[0] # a declarator
                    while decl.type == pt_declarator:
                        dirdcl = decl.list[-1]
                        decl = dirdcl.list[0]
                    # now decl is an identifier token
                    self.lex.addtypedef(decl.text)
            return n

        def init_declarator_list(self, dcl=None):
            n = node(pt_init_declarator_list)
            n.append(self.init_declarator(dcl))
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                n.append(self.init_declarator())
            return n
        
        def init_declarator(self, dcl=None):
            n = node(pt_init_declarator)
            if dcl == None: dcl = self.declarator(1) # non-abstract only
            n.append(dcl)
            if self.peektype() == lt_assign:
                self.eat(lt_assign)
                n.append(self.initializer())
            return n

        def declaration_specifiers(self):
            n = node(pt_declaration_specifiers)
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
            return n

        def declaration_specifiers_peek(self):
            if self.storage_class_qualifier_peek():
                return 1
            elif self.function_specifier_peek():
                return 1
            elif self.type_qualifier_peek():
                return 1
            elif self.type_specifier_peek():
                return 1
            else:
                return 0

        _storage_class_qualifiers = { \
        lt_typedef: 1, lt_extern: 1, lt_static: 1, lt_auto: 1, lt_register: 1}
        def storage_class_qualifier_peek(self):
            return self._storage_class_qualifiers.has_key(self.peektype())
        def storage_class_qualifier(self):
            n = node(pt_storage_class_qualifier)
            n.append(self.get())
            return n

        _type_qualifiers = { lt_const: 1, lt_restrict: 1, lt_volatile: 1 }
        def type_qualifier_peek(self):
            return self._type_qualifiers.has_key(self.peektype())
        def type_qualifier(self):
            n = node(pt_type_qualifier)
            n.append(self.get())
            return n
        def type_qualifier_list(self):
            n = node(pt_type_qualifier_list)
            while self.type_qualifier_peek():
                n.append(self.type_qualifier())
            return n

        _function_specifiers = { lt_inline: 1}
        def function_specifier_peek(self):
            return self._function_specifiers.has_key(self.peektype())
        def function_specifier(self):
            n = node(pt_function_specifier)
            n.append(self.get())
            return n

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
            n = node(pt_type_specifier)
            t = self.peektype()
            if self._simple_type_specifiers.has_key(t):
                n.append(self.get())
            elif t == lt_enum:
                n.append(self.enum_specifier())
            elif t == lt_struct or t == lt_union:
                n.append(self.struct_or_union_specifier())
            return n

        def specifier_qualifier_list(self):
            n = node(pt_specifier_qualifier_list)
            while 1:
                if self.type_qualifier_peek():
                    n.append(self.type_qualifier())
                elif self.type_specifier_peek():
                    n.append(self.type_specifier())
                else:
                    break
            if n.list == []:
                ERROR()
            return n

        def struct_or_union_specifier(self):
            n = node(pt_struct_or_union_specifier)
            t = self.peektype()
            if t != lt_struct and t != lt_union:
                ERROR()
            n.append(self.get())
            if self.peektype() == lt_ident:
                n.append(self.get())
                bracemandatory = 0
            else:
                n.append(None)
                bracemandatory = 1
            if self.peektype() == lt_lbrace:
                self.eat(lt_lbrace)
                n.append(self.struct_declaration_list())
                self.eat(lt_rbrace)
            elif bracemandatory:
                ERROR()
            return n

        def struct_declaration_list(self):
            n = node(pt_struct_declaration_list)
            while self.peektype() != lt_rbrace:
                n.append(self.struct_declaration())
            if n.list == []:
                ERROR()
            return n

        def struct_declaration(self):
            n = node(pt_struct_declaration)
            n.append(self.specifier_qualifier_list())
            n.append(self.struct_declarator_list())
            self.eat(lt_semi)
            return n
        
        def struct_declarator_list(self):
            n = node(pt_struct_declarator_list)
            n.append(self.struct_declarator())
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                n.append(self.struct_declarator())
            return n

        def struct_declarator(self):
            n = node(pt_struct_declarator)
            if self.peektype() != lt_colon:
                n.append(self.declarator(1)) # non-abstract only
            else:
                n.append(None)
            if self.peektype() == lt_colon:
                self.eat(lt_colon)
                n.append(self.constant_expression())
            return n

        def enum_specifier(self):
            n = node(pt_enum_specifier)
            self.eat(lt_enum)
            if self.peektype() == lt_ident:
                n.append(self.get())
            else:
                n.append(None)
                if self.peektype() != lt_lbrace:
                    ERROR()
            if self.peektype() == lt_lbrace:
                self.eat(lt_lbrace)
                n.append(self.enumerator_list())
                if (self.peektype() == lt_comma):
                    self.eat(lt_comma)
                self.eat(lt_rbrace)
            return n
        
        def enumerator_list(self):
            n = node(pt_enumerator_list)
            n.append(self.enumerator())
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                n.append(self.enumerator())
            return n

        def enumerator(self):
            n = node(pt_enumerator)
            n.append(self.eat(lt_ident))
            if self.peektype() == lt_assign:
                self.eat(lt_assign)
                n.append(self.constant_expression())
            return n

        def parameter_type_list(self):
            n = node(pt_parameter_type_list)
            n.append(self.parameter_declaration())
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                if self.peektype() == lt_ellipsis:
                    n.append(self.get())
                    break
                n.append(self.parameter_declaration())
            return n

        def parameter_declaration(self):
            n = node(pt_parameter_declaration)
            n.append(self.declaration_specifiers())#
            t = self.peektype()
            if t == lt_comma or t == lt_rparen:
                return n
            n.append(self.declarator(3)) # abstract OR non-abstract
            return n

        def identifier_list(self):
            n = node(pt_identifier_list)
            n.append(self.eat(lt_ident))
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                n.append(self.eat(lt_ident))
            return n

        def initializer(self):
            n = node(pt_initializer)
            if self.peektype() == lt_lbrace:
                self.eat(lt_lbrace)
                n.append(self.initializer_list())
                if self.peektype() == lt_comma:
                    self.eat(lt_comma)
                self.eat(lt_rbrace)
            else:
                n.append(self.assignment_expression())
            return n

        def initializer_list(self):
            n = node(pt_initializer_list)
            while 1:
                t = self.peektype()
                if t == lt_lbracket or t == lt_dot:
                    n.append(self.designation())
                else:
                    n.append(None)
                n.append(self.initializer())
                if self.peektype() != lt_comma:
                    break
                self.eat(lt_comma)
            return n

        def designation(self):
            n = node(pt_designation)
            n.append(self.designator())
            while self.peektype() != lt_assign:
                n.append(self.designator())
            self.eat(lt_assign)
            return n

        def designator(self):
            n = node(pt_designator)
            token = self.get()
            n.append(token)
            if token.type == lt_rbracket:
                n.append(self.constant_expression())
                self.eat(lt_rbracket)
            elif token.type == lt_dot:
                n.append(self.eat(lt_ident))
            else:
                self.unget(n)
                ERROR()
            return n

        def type_name_peek(self):
            if self.type_qualifier_peek():
                return 1
            elif self.type_specifier_peek():
                return 1
            else:
                return 0

        def type_name(self):
            n = node(pt_type_name)
            n.append(self.specifier_qualifier_list())
            n.append(self.declarator(2))
            return n

        def declarator(self, abstract):
            "`abstract' should have bit 1 set if a non-abstract declarator\n"\
            "is acceptable, and bit 2 set if an abstract one is acceptable"
            n = node(pt_declarator)
            if self.peektype() == lt_times:
                n.append(self.pointer())
            n.append(self.direct_declarator(abstract))
            return n

        def pointer(self):
            n = node(pt_pointer)
            while self.peektype() == lt_times:
                self.get()
                if self.type_qualifier_peek():
                    n.append(self.type_qualifier_list())
                else:
                    n.append(None)
            return n

        def direct_declarator(self, abstract):
            "`abstract' should have bit 1 set if a non-abstract direct-dcl\n"\
            "is acceptable, and bit 2 set if an abstract one is acceptable"
            n = node(pt_direct_declarator)
            t = self.peektype()
            if t == lt_ident:
                if not (abstract & 1):
                    ERROR() # identifier in abstract declarator
                n.append(self.get())
            elif t == lt_lparen:
                self.eat(lt_lparen)
                # Even if we accept an abstract-declarator in here we don't
                # accept a blank one.
                if self.peektype() == lt_rparen:
                    ERROR()
                n.append(self.declarator(abstract))
                self.eat(lt_rparen)
            else:
                if not (abstract & 2):
                    ERROR() # no identifier in non-abstract declarator
                n.append(None)
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
                    self.eat(lt_rparen)
                else:
                    if self.peektype() != lt_rbracket:
                        n.append(self.assignment_expression())
                    else:
                        n.append(None)
                    self.eat(lt_rbracket)
            return n

        def compound_statement(self):
            # Parse tree compression: we don't have an explicit subnode
            # for a block-item-list or for block-items.
            n = node(pt_compound_statement)
            self.eat(lt_lbrace)
            self.lex.pushtypedef()
            while self.peektype() != lt_rbrace:
                if self.declaration_specifiers_peek():
                    n.append(self.declaration())
                else:
                    n.append(self.statement())
            self.eat(lt_rbrace)
            self.lex.poptypedef()
            return n
        
        def statement(self):
            # Parse tree compression: we don't bother with a level
            # for `statement'.
            type = self.peektype()
            if type == lt_ident:
                ident = self.get()
                next = self.peektype()
                self.unget(ident)
                if next == lt_colon:
                    return self.labeled_statement()
                else:
                    return self.expression_statement()
            elif type == lt_case or type == lt_default:
                return self.labeled_statement()
            elif type == lt_if or type == lt_switch:
                return self.selection_statement()
            elif type == lt_while or type == lt_do or type == lt_for:
                return self.iteration_statement()
            elif type == lt_goto or type == lt_break or type == lt_continue or type == lt_return:
                return self.jump_statement()
            elif type == lt_lbrace:
                return self.compound_statement()
            else:
                return self.expression_statement()

        def expression_statement(self):
            if self.peektype() == lt_semi:
                ret = node(pt_null_statement)
            else:
                ret = self.expression()
            self.eat(lt_semi)
            return ret

        def labeled_statement(self):
            n = node(pt_labeled_statement)
            t = self.peektype()
            if ident == None and t == lt_ident:
                ident = self.get()
            if ident != None:
                n.append(ident)
            elif t == lt_default:
                self.get()
                n.append(None)
            elif t == lt_case:
                self.get()
                n.append(self.constant_expression())
            else:
                ERROR()
            if self.peektype() != lt_colon:
                ERROR()
                self.get()
            n.append(self.statement())
            return n

        def selection_statement(self):
            n = node(pt_selection_statement)
            token = self.get()
            n.append(token)
            if token.type != lt_if and token.type != lt_switch:
                ERROR()
            self.eat(lt_lparen)
            n.append(self.expression())
            self.eat(lt_rparen)
            n.append(self.statement())
            # Note that the way this routine is coded naturally provides
            # a correct resolution of the if-else ambiguity
            if token.type == lt_if and self.peektype() == lt_else:
                self.eat(lt_else)
                n.append(self.statement())
            return n

        def iteration_statement(self):
            n = node(pt_iteration_statement)
            token = self.get()
            n.append(token)
            if token.type == lt_while:
                self.eat(lt_lparen)
                n.append(self.expression())
                self.eat(lt_rparen)
                n.append(self.statement())
            elif token.type == lt_do:
                n.append(self.statement())
                self.eat(lt_while)
                self.eat(lt_lparen)
                n.append(self.expression())
                self.eat(lt_rparen)
                self.eat(lt_semi)
            elif token.type == lt_for:
                self.eat(lt_lparen)
                # First argument to for can be a declaration
                if self.declaration_specifiers_peek():
                    n.append(self.declaration())
                elif self.peektype() == lt_semi:
                    n.append(None)
                else:
                    n.append(self.expression())
                    self.eat(lt_semi)
                # Second argument to for is an expression or blank
                if self.peektype() != lt_semi:
                    n.append(self.expression())
                self.eat(lt_semi)
                # Third argument to for is an expression or blank
                if self.peektype() != lt_semi:
                    n.append(self.expression())
                # Closing parenthesis, then statement
                self.eat(lt_rparen)
                n.append(self.statement())
            else:
                ERROR()
            return n
        
        def jump_statement(self):
            n = node(pt_jump_statement)
            token = self.get()
            n.append(token)
            if token.type == lt_goto:
                if self.peektype() != lt_ident:
                    ERROR()
                n.append(self.get())
            elif token.type == lt_break or token.type == lt_continue:
                pass # these are OK
            elif token.type == lt_return:
                if self.peektype() != lt_semi:
                    n.append(self.expression())
            else:
                ERROR()
            self.eat(lt_semi)
            return n

        def primary_expression(self):
            t = self.peektype()
            if t == lt_lparen:
                self.eat(lt_lparen)
                ret = self.expression()
                self.eat(lt_rparen)
                return ret
            n = node(pt_primary_expression)
            if t == lt_ident or t == lt_charconst or t == lt_intconst or t == lt_floatconst:
                n.append(self.get())
            elif t == lt_stringconst:
                while self.peektype() == lt_stringconst:
                    n.append(self.get())
            else:
                ERROR()
            return n
        
        def postfix_expression(self):
            if self.peektype() == lt_lparen:
                lpar = self.eat(lt_lparen)
                if not self.type_name_peek():
                    self.unget(lpar)
                    n = self.primary_expression()
                else:
                    n = node(pt_postfix_expression)
                    n.append(self.type_name())
                    self.eat(lt_rparen)
                    self.eat(lt_lbrace)
                    n.append(self.initializer_list())
                    if self.peektype() == lt_comma:
                        self.eat(lt_comma)
                    self.eat(lt_rbrace)
            else:
                n = self.primary_expression()
            while 1:
                t = self.peektype()
                if t == lt_lbracket:
                    self.eat(lt_lbracket)
                    m = node(pt_postfix_expression)
                    m.append(n)
                    m.append(self.expression())
                    self.eat(lt_rbracket)
                    n = m
                elif t == lt_lparen:
                    self.eat(lt_lparen)
                    m = node(pt_postfix_expression)
                    m.append(n)
                    if self.peektype() != lt_rparen:
                        m.append(self.argument_expression_list())
                    self.eat(lt_rparen)
                    n = m
                elif t == lt_dot or t == lt_arrow:
                    m = node(pt_postfix_expression)
                    m.append(n)
                    m.append(self.get())
                    m.append(self.eat(lt_ident))
                    n = m
                elif t == lt_increment or t == lt_decrement:
                    m = node(pt_postfix_expression)
                    m.append(n)
                    m.append(self.get())
                    n = m
                else:
                    break
            return n
        
        def argument_expression_list(self):
            n = node(pt_argument_expression_list)
            n.append(self.assignment_expression())
            while self.peektype() == lt_comma:
                self.eat(lt_comma)
                n.append(self.assignment_expression())
            return n
        
        def unary_expression(self):
            t = self.peektype()
            if t == lt_increment or t == lt_decrement:
                n = node(pt_unary_expression)
                n.append(t)
                n.append(self.unary_expression())
                return n
            elif t == lt_sizeof:
                n = node(pt_unary_expression)
                n.append(t)
                if self.peektype() == lt_lparen:
                    # Either a unary-expression or ( type-name ).
                    # Need to find out which.
                    t = self.eat(lt_lparen)
                    if self.type_name_peek():
                        n.append(self.type_name())
                    else:
                        self.unget(t)
                        n.append(self.unary_expression())
                else:
                    n.append(self.unary_expression())
                return n
            elif t == lt_bitand or t == lt_times or t == lt_plus or t == lt_minus or t == lt_bitnot or t == lt_lognot:
                n = node(pt_unary_expression)
                n.append(self.get())
                n.append(self.cast_expression())
                return n
            else:
                return self.postfix_expression()

        def cast_expression(self, uexp = None):
            if uexp != None:
                return uexp
            t = self.get()
            if t.type == lt_lparen and self.type_name_peek():
                n = node(pt_cast_expression)
                n.append(self.type_name())
                self.eat(lt_rparen)
                n.append(self.cast_expression())
                return n
            else:
                self.unget(t)
                return self.unary_expression()

        def binary_expr(self, operators, nextfunc, uexp = None):
            expr = nextfunc(uexp)
            while 1:
                t = self.peektype()
                if t in operators:
                    n = node(pt_binary_expression)
                    n.append(expr)
                    n.append(self.get())
                    n.append(nextfunc())
                    expr = n
                else:
                    break
            return expr

        def multiplicative_expression(self, uexp = None):
            return self.binary_expr([lt_times,lt_slash,lt_modulo], self.cast_expression, uexp)
        def additive_expression(self, uexp = None):
            return self.binary_expr([lt_plus,lt_minus], self.multiplicative_expression, uexp)
        def shift_expression(self, uexp = None):
            return self.binary_expr([lt_lshift,lt_rshift], self.additive_expression, uexp)
        def relational_expression(self, uexp = None):
            return self.binary_expr([lt_less,lt_greater,lt_lesseq,lt_greateq], self.shift_expression, uexp)
        def equality_expression(self, uexp = None):
            return self.binary_expr([lt_equality,lt_notequal], self.relational_expression, uexp)
        def AND_expression(self, uexp = None):
            return self.binary_expr([lt_bitand], self.equality_expression, uexp)
        def exclusive_OR_expression(self, uexp = None):
            return self.binary_expr([lt_bitxor], self.AND_expression, uexp)
        def inclusive_OR_expression(self, uexp = None):
            return self.binary_expr([lt_bitor], self.exclusive_OR_expression, uexp)
        def logical_AND_expression(self, uexp = None):
            return self.binary_expr([lt_logand], self.inclusive_OR_expression, uexp)
        def logical_OR_expression(self, uexp = None):
            return self.binary_expr([lt_logor], self.logical_AND_expression, uexp)

        def conditional_expression(self, uexp = None):
            expr = self.logical_OR_expression(uexp)
            if self.peektype() == lt_query:
                self.eat(lt_query)
                n = node(pt_conditional_expression)
                n.append(expr)
                n.append(self.expression())
                self.eat(lt_colon)
                n.append(self.conditional_expression())
                return n
            else:
                return expr

        def assignment_expression(self, uexp=None):
            if uexp != None:
                ERROR() # shouldn't ever happen
            # Generally, we expect to see a unary-expression here. The
            # only way we might not is if we see an lparen followed by a
            # type-name.
            if self.peektype() == lt_lparen:
                lpar = self.get() # we may have to unget this
                is_cast_exp = self.type_name_peek()
                self.unget(lpar)
                if is_cast_exp: # we're a cast-expression
                    return self.conditional_expression()
            uexp = self.unary_expression()
            if self.peektype() in [lt_assign, lt_timeseq, lt_slasheq,
            lt_moduloeq, lt_pluseq, lt_minuseq, lt_lshifteq, lt_rshifteq,
            lt_bitandeq, lt_bitxoreq, lt_bitoreq]:
                n = node(pt_assignment_expression)
                n.append(uexp)
                n.append(self.get())
                n.append(self.assignment_expression())
                return n
            else:
                # The unary-expression we have now obtained is the initial
                # component of a conditional-expression. Argh.
                return self.conditional_expression(uexp)
        
        def expression(self):
            return self.binary_expr([lt_comma], self.assignment_expression, None)

        def constant_expression(self):
            n = node(pt_constant_expression)
            n.append(self.conditional_expression())
            return n

    p = parserclass(lex)
    return p.translation_unit()
