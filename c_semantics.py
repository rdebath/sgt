# Semantic analysis for C

# Reads a parse tree. Outputs a set of types, a flattened tree giving the
# interior of each function, and at every block level a description of all
# variable definitions and declarations. The code tree for each function
# has already had its integer promotions and so on performed, so all
# arithmetic operations are between correct (and explicitly specified) types
# and all implicit type conversions are made explicit.

import string

from c_lex import *
from c_parse import *

# Base code so I can declare a bunch of enumerations
_enumval = 0
def _enum(init=-1):
    global _enumval
    if init >= 0:
        _enumval = init
    else:
        _enumval = _enumval + 1
    return _enumval

# Enumeration of types.
t_BEGIN = _enum(0)
t_void = _enum()
t_char = _enum()
t_schar = _enum()
t_uchar = _enum()
t_short = _enum()
t_ushort = _enum()
t_bfint = _enum() # the implementation-defined int type used in bitfields
t_int = _enum()
t_uint = _enum()
t_long = _enum()
t_ulong = _enum()
t_longlong = _enum()
t_ulonglong = _enum()
t_float = _enum()
t_double = _enum()
t_ldouble = _enum()
t_bool = _enum()
t_cfloat = _enum()
t_cdouble = _enum()
t_cldouble = _enum()
t_ifloat = _enum()
t_idouble = _enum()
t_ildouble = _enum()
# Pseudo-type to be used as the last element in a list of argument types.
t_ellipsis = _enum()
# Derived type markers
t_pointer = _enum()
t_array = _enum()
t_function = _enum()
t_struct = _enum()
t_union = _enum()
t_END = _enum()

# Enumeration of type qualifiers as bits.
tq_const = 1 << _enum(0)
tq_volatile = 1 << _enum()
tq_restrict = 1 << _enum()

# Special array dimensions.
array_unspecified = -1
array_variable = -2

# Optree node types. Optree constant nodes have type constants as their
# node type, so the op_* constants must not overlap the t_* constants.
# The tuple expected in each node type is given in a comment.
op_BEGIN = _enum(t_END)
op_cast = _enum() # (srctype, desttype, operand)
op_deref = _enum() # (operand)
op_if = _enum() # (condition, thenclause, elseclause)
op_while = _enum() # (condition, loopbody)
op_END = _enum()

def _sortt(list):
    "Internal function that sorts a list and tuples it."
    newlist = list[:] # copy it
    newlist.sort()
    return tuple(newlist)

class typesystem:
    "Class to hold a system of types."

    def __init__(self, target):
        self.uniqueid = 0
        self.typedefs = {}
        self.target = target

    def basictype(self, t, quals):
        return (quals, t)
    def ptrtype(self, referencedtype, quals):
        return (quals, t_pointer, referencedtype)
    def array(self, elementtype, dimension):
        return (elementtype[0], t_array, elementtype, dimension)
    def function(self, returntype, args, quals):
        return (quals, t_function, returntype, args)

    def addtypedef(self, name, type):
        self.typedefs[name] = type
    def findtypedef(self, name, quals):
        tuple = self.typedefs[name]
        # Add in the extra type qualifiers
        tuple2 = (tuple[0] | quals,) + tuple[1:]
        return tuple2

    inttypes = [t_char, t_schar, t_uchar, t_short, t_ushort, t_bfint, t_int,
    t_uint, t_long, t_ulong, t_longlong, t_ulonglong, t_bool]
    flttypes = [t_float, t_double, t_ldouble, t_cfloat, t_cdouble, t_cldouble,
    t_ifloat, t_idouble, t_ildouble]

    def isarith(self, type):
        "Determine whether a type is arithmetic."
        return self.isint(type) or self.isfloat(type)
    inttypes = {
    t_bool: 1, t_char: 1, t_uchar: 1, t_schar: 1, t_short: 1, t_ushort: 1,
    t_int: 1, t_uint: 1, t_long: 1, t_ulong: 1, t_longlong: 1, t_ulonglong: 1
    }
    def isint(self, type):
        "Determine whether a type is integer."
        return self.inttypes.has_key(type[1])
    floattypes = {
    t_float: 1, t_double: 1, t_ldouble: 1, t_cfloat: 1, t_cdouble: 1,
    t_cldouble: 1, t_ifloat: 1, t_idouble: 1, t_ildouble: 1,
    }
    def isfloat(self, type):
        "Determine whether a type is floating."
        return self.floattypes.has_key(type[1])
    # FIXME: target must specify whether char is signed
    unsignedtypes = {
    t_bool: 1, t_uchar: 1, t_ushort: 1, t_uint: 1, t_ulong: 1, t_ulonglong: 1
    }
    def isunsigned(self, type):
        "Determine whether a type is unsigned."
        return self.unsignedtypes.has_key(type[1])

    def referenced(self, type):
        "Return the referenced type of a pointer, or None if not a pointer."
        if type[1] == t_pointer:
            return type[2]
        else:
            return None

    def intcontains(self, type1, type2):
        "Determine whether integer type2 can hold all the values of type1."
        if self.target.intmaxvals[type1[1]] > self.target.intmaxvals[type2[1]]:
            return 0
        if self.target.intminvals[type1[1]] < self.target.intminvals[type2[1]]:
            return 0
        return 1

    def intpromote(self, type1):
        basetype = type1[1]
        if self.intcontains((0, basetype), (0, t_int)):
            basetype = t_int
        elif self.intcontains((0, basetype), (0, t_uint)):
            basetype = t_uint
        return type1[:1] + (basetype,) + type1[2:]

    ranks = {
    t_bool: 0,
    t_char: 1, t_schar: 1, t_uchar: 1,
    t_short: 1, t_ushort: 1,
    t_int: 1, t_uint: 1,
    t_long: 2, t_ulong: 2,
    t_longlong: 3, t_ulonglong: 3,
    }
    inttouint = {
    t_int: t_uint, t_uint: t_uint,
    t_long: t_ulong, t_ulong: t_ulong,
    t_longlong: t_ulonglong, t_ulonglong: t_ulonglong}
    def usualarith(self, type1, type2):
        "Perform the usual arithmetic conversions on two types to return\n"\
        "a third type."
        if self.isfloat(type1) or self.isfloat(type2):
            pass # FIXME
        else:
            type1 = self.intpromote(type1)
            type2 = self.intpromote(type2)
            u1 = self.isunsigned(type1)
            u2 = self.isunsigned(type2)
            if u1 == u2:
                if self.ranks[type1[1]] < self.ranks[type2[1]]:
                    ret = type2
                else:
                    ret = type1
            else:
                # Let type1 be the unsigned one.
                if u2:
                    type1, type2 = type2, type1
                if self.ranks[type1[1]] >= self.ranks[type2[1]]:
                    ret = type1
                elif self.intcontains(type1, type2):
                    ret = type2
                else:
                    base = self.inttouint(type2[1])
                    ret = type2[:1] + (base,) + type2[2:]
            return ret, ret, ret # ltype, rtype and outtype all the same

    # Operations required on a type are
    #  - determine whether two types are compatible
    #  - construct a composite type from two (each fills in blanks in other)
    #  - generate treeops for an implicit conversion
    #  - generate treeops for an explicit conversion (ie a cast)
    #  - determine whether a type is pointer
    #  - determine whether a type is integer
    #  - determine whether a type is arithmetic
    # Operations required on arithmetic types are
    #  - perform usual arithmetic conversions

class optree:
    "Class to hold the operation-tree output from semantic analysis."
    def __init__(self, type, *extras):
	self.type = type
	self.tuple = extras

# Error reporting.

class err_generic_semantic_error:
    def __init__(self):
        self.message = "semantic error"
class err_deref_nonpointer(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to dereference a non-pointer"
class err_assign_nonlval(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to assign to a non-lvalue"
class err_nonconst_expr(err_generic_semantic_error):
    def __init__(self):
        self.message = "expected constant expression"
class err_incompat_types_binop(err_generic_semantic_error):
    def __init__(self, token):
        self.message = "incompatible types for " + lex_names[token]
class err_invalid_type(err_generic_semantic_error):
    def __init__(self, typespecs):
        self.message = "unrecognised type `"
        for i in typespecs:
            text = lex_names[i][1:-1] # strip ` and '
            if self.message[-1] != '`': self.message = self.message + " "
            self.message = self.message + text
        self.message = self.message + "'"
class err_multi_scq(err_generic_semantic_error):
    def __init__(self, one, two):
        self.message = "multiple storage class qualifiers " + \
        lex_names[one] + " and " + lex_names[two]

def defaulterrfunc(err):
    sys.stderr.write("<input>:" + "%d:%d: " % (err.line, err.col) + err.message + "\n")

# The actual semantic analysis phase.

class semantics:
    "Class to perform, and hold the results of, semantic analysis of a C\n"\
    "translation unit."
    
    def __init__(self, parsetree, target, errfunc=defaulterrfunc):
        self.functions = []
        self.topdecls = []
	self.target = target
        self.types = typesystem(target)
        self.errfunc = errfunc
        try:
            self.do_tran_unit(parsetree)
        except err_generic_semantic_error, err:
            self.errfunc(err)

    def error(self, object, error):
        error.line = object.line
        error.col = object.col
        raise error

    def display(self):
        for i in self.topdecls:
            print "topdecl", i

    def do_tran_unit(self, parsetree):
        # Proceed through a translation_unit.
        assert parsetree.type == pt_translation_unit
        for i in parsetree.list:
            if i.type == pt_function_definition:
                self.functions.append(self.do_function(i))
            elif i.type == pt_declaration:
                self.topdecls.extend(self.do_declaration(i))
            else:
                assert 0, "unexpected external declaration"

    # Type specifier lists and their corresponding base type.
    typespeclists = {
    _sortt([lt_void]): t_void,
    _sortt([lt_char]): t_char,
    _sortt([lt_signed, lt_char]): t_schar,
    _sortt([lt_unsigned, lt_char]): t_uchar,
    _sortt([lt_short]): t_short,
    _sortt([lt_signed, lt_short]): t_short,
    _sortt([lt_short, lt_int]): t_short,
    _sortt([lt_signed, lt_short, lt_int]): t_short,
    _sortt([lt_unsigned, lt_short]): t_ushort,
    _sortt([lt_unsigned, lt_short, lt_int]): t_ushort,
    _sortt([lt_int]): t_bfint,
    _sortt([lt_signed]): t_int,
    _sortt([lt_signed, lt_int]): t_int,
    _sortt([lt_unsigned]): t_uint,
    _sortt([lt_unsigned, lt_int]): t_uint,
    _sortt([lt_long]): t_long,
    _sortt([lt_signed, lt_long]): t_long,
    _sortt([lt_long, lt_int]): t_long,
    _sortt([lt_signed, lt_long, lt_int]): t_long,
    _sortt([lt_unsigned, lt_long]): t_ulong,
    _sortt([lt_unsigned, lt_long, lt_int]): t_ulong,
    _sortt([lt_long, lt_long]): t_longlong,
    _sortt([lt_signed, lt_long, lt_long]): t_longlong,
    _sortt([lt_long, lt_long, lt_int]): t_longlong,
    _sortt([lt_signed, lt_long, lt_long, lt_int]): t_longlong,
    _sortt([lt_unsigned, lt_long, lt_long]): t_ulonglong,
    _sortt([lt_unsigned, lt_long, lt_long, lt_int]): t_ulonglong,
    _sortt([lt_float]): t_float,
    _sortt([lt_double]): t_double,
    _sortt([lt_long, lt_double]): t_ldouble,
    _sortt([lt__Bool]): t_bool,
    _sortt([lt_float, lt__Complex]): t_cfloat,
    _sortt([lt_double, lt__Complex]): t_cdouble,
    _sortt([lt_long, lt_double, lt__Complex]): t_cldouble,
    _sortt([lt_float, lt__Imaginary]): t_ifloat,
    _sortt([lt_double, lt__Imaginary]): t_idouble,
    _sortt([lt_long, lt_double, lt__Imaginary]): t_ildouble,
    }

    # Map type qualifier keywords to type qualifier bits.
    typequalmap = {
    lt_const: tq_const,
    lt_volatile: tq_volatile,
    lt_restrict: tq_restrict
    }

    # Parse tree nodes we expect in an expression.
    exprnodes = {pt_primary_expression: 1, pt_postfix_expression: 1,
    pt_unary_expression: 1, pt_cast_expression: 1, pt_binary_expression: 1,
    pt_conditional_expression: 1, pt_assignment_expression: 1}

    def expr(self, exp):
	# Process an expression.
	# Return a tuple (type, value, lvalue, tree).
	# `type' describes the intrinsic type of the expression (there will
	# probably be another type we want to assign it to later).
	# `value' describes the constant value of the expression, or None
	# if the expression is non-constant.
	# `lvalue' is true if the expression is an lvalue.
	# `tree' gives the output optree form of the expression.
	#
	# The optree does not extend into constant parts of the parse tree.
	if exp.type == pt_primary_expression:
	    if exp.list[0].type == lt_intconst:
		# Strip off Ls and Us from the end until we know what
		# integer type we're dealing with. Then parse the actual
		# integer, and then pass to the target module for trimming.
		t = 0
		text = string.upper(exp.list[0].text)
		while text[-1:] in "LU":
		    c = text[-1]
		    text = text[:-1]
		    if c == "L":
			t = t + 2
		    else:
			t = t | 1
		type = (t_int,t_uint,t_long,t_ulong,t_longlong,t_ulonglong)[t]
                type = (0, type)
		val = 0L
		if text[0:2] == "0X":
		    base = 16
		    text = text[2:]
		elif text[:1] == "0":
		    base = 8
		    text = text[1:]
		else:
		    base = 10
		while len(text) > 0:
		    cval = ord(text[0]) - ord('0')
		    text = text[1:]
		    if cval > 9: cval = cval - 7
		    val = val * base + cval
		val = self.target.intconst(type, val)
		return type, val, 0, optree(type, val)
	    elif exp.list[0].type == lt_ident:
		pass # FIXME
	    elif exp.list[0].type == pt_charconst:
		pass # FIXME
	    elif exp.list[0].type == pt_stringconst:
		pass # FIXME
	    elif exp.list[0].type == pt_floatconst:
		pass # FIXME
	    else:
		FIXME()
	elif exp.type == pt_postfix_expression:
            FIXME()
	elif exp.type == pt_unary_expression:
            if exp.list[0].type == lt_times:
                operand = self.expr(exp.list[1])
                desttype = self.types.referenced(operand[0])
                if desttype == None:
                    self.error(exp, err_deref_nonpointer())
                return desttype, None, 1, optree(operand)
            else:
                #  increment decrement
                #  sizeof
                #  bitand
                #  plus minus bitnot lognot
                FIXME()
	elif exp.type == pt_cast_expression:
            src = self.expr(exp.list[1])
            desttype = self.typename(exp.list[0])
            # FIXME: verify explicit castability
            return self.typecvt(src, desttype, 1)
	elif exp.type == pt_binary_expression:
            lhs = self.expr(exp.list[0])
            rhs = self.expr(exp.list[2])
            return self.binop(exp.list[1].type, lhs, rhs)
	elif exp.type == pt_conditional_expression:
            FIXME()
	elif exp.type == pt_assignment_expression:
            exp.display(0)
            lhs = self.expr(exp.list[0])
            rhs = self.expr(exp.list[2])
            if not lhs[2]:
                self.error(exp, err_assign_nonlval())
            else:
                if lhs[0] != rhs[0]: rhs = self.typecvt(rhs, lhs[0], 0)
                return lhs[0], None, 0, optree(exp.list[1].type, lhs[0], lhs, rhs)
	elif exp.type == pt_constant_expression:
	    type, value, lvalue, tree = self.expr(exp.list[0])
	    if value == None:
                self.error(exp, err_nonconst_expr())
	    return type, value, lvalue, tree
	else:
	    assert 0, "unexpected node in expression"

    def typename(self, tn):
        stg, fnspecs, basetype = self.do_declspecs(tn.list[0])
        t, id = self.do_declarator(tn.list[1], basetype)
        assert stg == None and fnspecs == {}
        assert id == None # this must be an abstract declarator
        return t

    def typecvt(self, src, desttype, explicit):
        "Process a type conversion, either explicit or implicit."
        # FIXME: check castability. Implicit castability is quite fun;
        # in particular an integer is implicitly castable to a pointer
        # if and only if the integer is known at compile time to be zero.
        #
        # FIXME: compile-time conversion when feasible.
        return desttype, None, 0, optree(op_cast, src[0], desttype, src)

    def binop(self, op, lhs, rhs):
        "Process a binary operator."
        # Must perform type conversion.
        if self.types.isarith(lhs[0]) and self.types.isarith(rhs[0]):
            lt, rt, outt = self.types.usualarith(lhs[0], rhs[0])
            if outt == None:
                self.error(lhs, err_incompat_types_binop(op))
            if lhs[0] != lt: lhs = self.typecvt(lhs, lt)
            if rhs[0] != rt: rhs = self.typecvt(lhs, rt)
        elif 0: # plus or minus, and exactly one is pointer
            pass
        # Perform constant arithmetic if possible.
        if lhs[1] != None and rhs[1] != None:
            newval = self.target.binop(op, outt, lt, lhs[1], rt, rhs[1])
            return outt, newval, 0, optree(outt, newval)
        else:
            return outt, None, 0, optree(op, outt, lhs, rhs)

    def do_declaration(self, decl):
        # The declaration specifiers can contain storage class qualifiers,
        # function specifiers, type qualifiers, and type specifiers.
        #
        # There may be at most one storage class qualifier. Save it.
        # The list of type specifiers must be one of a fixed set of lists
        # (although the specifiers may be in any order).
        # The list of type qualifiers must be kept, although duplicates are
        # removed (they're legal, just pointless).
        # The list of function specifiers must be kept, just like type
        # qualifiers. It's illegal to have them on nonfunctions.
        specs = decl.list[0]
        assert specs.type == pt_declaration_specifiers
        stg, fnspecs, basetype = self.do_declspecs(specs)
        # We now have our base type. Go through the declarators processing
        # each one to create derived variants from the base type.
        defs = []
        assert decl.list[1].type == pt_init_declarator_list
        for i in decl.list[1].list:
            assert i.type == pt_init_declarator
            dcl = i.list[0]
            assert dcl.type == pt_declarator
            t, id = self.do_declarator(dcl, basetype)
            assert id != None # this declarator may not be abstract
            if stg == lt_typedef:
                self.types.addtypedef(id.text, t);
            else:
                defs = defs + [[id.text, stg, t]]
        return defs

    def do_declspecs(self, specs):
        stg = None
        typequals = 0
        fnspecs = {}
        typespecs = []
        for i in specs.list:
            if i.type == pt_storage_class_qualifier:
                if stg != None:
                    self.error(i, err_multi_scq(stg, i.list[0].type))
                else:
                    stg = i.list[0].type
            elif i.type == pt_type_qualifier:
                typequals = typequals | self.typequalmap[i.list[0].type]
            elif i.type == pt_function_specifier:
                fnspecs[i.list[0].type] = 1
            elif i.type == pt_type_specifier:
                tspec = i.list[0]
                typespecs.append(tspec.type)
            else:
                assert 0, "unexpected declaration specifier"
        # Determine the type specifier list and deduce the base type.
        sts = _sortt(typespecs)
        if self.typespeclists.has_key(sts):
            # Base type is a genuine basic type.
            which = self.typespeclists[sts]
            # In this situation we are not declaring a bitfield, so bfint
            # is the same as int.
            if which == t_bfint: which = t_int
            basetype = self.types.basictype(which, typequals)
        elif sts == (pt_struct_or_union_specifier,):
            basetype = self.structuniontype(tspec)
        elif sts == (pt_enum_specifier,):
            basetype = self.enumtype(tspec)
        elif sts == (lt_typedefname,):
            basetype = self.types.findtypedef(tspec.text, typequals)
        else:
            self.error(specs, err_invalid_type(typespecs))
        return stg, fnspecs, basetype

    def do_declarator(self, dcl, t):
        while 1:
            # Process a declarator
            if dcl.list[0].type == pt_pointer:
                for i in dcl.list[0].list:
                    quals = 0
                    if i != None:
                        assert i.type == pt_type_qualifier_list
                        for j in i.list:
                            assert j.type == pt_type_qualifier
                            quals = quals | self.typequalmap[j.list[0].type]
                    t = self.types.ptrtype(t, quals)
                dirdcl = dcl.list[1]
            else:
                dirdcl = dcl.list[0]
            # Process a direct-declarator
            assert dirdcl.type == pt_direct_declarator
            for i in dirdcl.list[1:]:
                # Deal with array and function derivations
                if i == None:
                    t = self.types.array(t, array_unspecified)
                elif i.type == pt_identifier_list or i.type == pt_parameter_type_list:
                    pass # FIXME: function type
                else:
                    # Attempt to parse i as a constant expression with
                    # integer type.
                    type, value, lvalue, tree = self.expr(i)
                    if value != None:
                        # We have a constant which is our array dimension.
                        # Cast it to type size_t and use it.
                        value = self.target.intconst((0, self.target.size_t), value)
                        # WIBNI: see if `value' has changed, and warn if so
                        t = self.types.array(t, value)
                    else:
                        t = self.types.vla(t, tree)
            if dirdcl.list[0] != None and dirdcl.list[0].type == pt_declarator:
                dcl = dirdcl.list[0]
                continue
            else:
                id = dirdcl.list[0]
                break
        return t, id

    def do_function(self, funcdef):
	assert funcdef.type == pt_function_definition
	# funcdef.list gives declaration_specifiers, declarator,
	# optional list of declarations of argument types, and
	# finally a compound statement.
	
	blk = self.do_block(funcdef.list[3])

    def do_block(self, blk):
	assert blk.type == pt_compound_statement
	decls = []
	stmts = []
	for i in blk.list:
	    if i.type == pt_declaration:
		decls.extend(self.do_declaration(i))
	    else:
		stmts.extend(self.do_statement(i))
	return [decls, stmts]
    
    def do_statement(self, stmt):
        ret = []
	while stmt.type == pt_labeled_statement:
	    # FIXME: deal with label
	    stmt = stmt.list[1]
	if self.exprnodes.has_key(stmt.type):
	    type, value, lvalue, tree = self.expr(stmt)
            ret.append(tree)
	elif stmt.type == pt_selection_statement:
            condition = self.expr(stmt.list[1])
            consequence = self.do_statement(stmt.list[2])
            if stmt.list[0].type == lt_if:
                if len(stmt.list) > 3:
                    alternative = self.do_statement(stmt.list[3])
                else:
                    alternative = None
                stmt = optree(op_if, condition, consequence, alternative)
            else:
                stmt = optree(op_while, condition, consequence)
        else:
            stmt.display(0)
	    FIXME()
        return ret
