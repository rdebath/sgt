# Semantic analysis for C

# Reads a parse tree. Outputs a set of types, a flattened tree giving the
# interior of each function, and at every block level a description of all
# variable definitions and declarations. The code tree for each function
# has already had its integer promotions and so on performed, so all
# arithmetic operations are between correct (and explicitly specified) types
# and all implicit type conversions are made explicit.

# Big remaining FIXMEs are ...
#
# - Collecting variables. Nested scopes; find previous declaration and
#   warn/diagnose if it's redeclared; parse a primary_expression/identifier
#   by looking up the variable in the relevant scope. (NB we can't just
#   complain immediately if it's undeclared; we have to faff because it might
#   be an implicitly declared external function.) Careful of default storage
#   classes (different inside a function) and tentative definitions at
#   top level.
#
# - Structure and union declarations.
#
# - Postfix expressions. Array dereferences are easy (we can already do
#   binary+ and unary*), but function calls and struct/union member lookups
#   are more fun.

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
# Pseudo-type to be used in an argument type list when no prototype is given.
t_unknown = _enum()
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

# Storage classes.
_enum(0)
sc_extern = _enum()
sc_global = _enum()
sc_static = _enum()
sc_auto = _enum()
sc_register = _enum()

# Special array dimensions.
array_unspecified = -1
array_variable = -2

# Special function-argument count.
func_unspecified = -1

# Optree node types. Optree constant nodes have type constants as their
# node type, so the op_* constants must not overlap the t_* constants.
# The tuple expected in each node type is given in a comment.
op_BEGIN = _enum(t_END)
op_blk = _enum() # not finalised yet
op_variable = _enum() # (scope, varname)
op_cast = _enum() # (srctype, desttype, operand)
op_deref = _enum() # (operand)
op_if = _enum() # (condition, thenclause, elseclause)
op_while = _enum() # (condition, loopbody)
op_for = _enum() # (initialiser, test, increment, loopbody)
op_END = _enum()

def _sortt(list):
    "Internal function that sorts a list and tuples it."
    newlist = list[:] # copy it
    newlist.sort()
    return tuple(newlist)

class typesystem:
    "Class to hold a system of types."

    # Types are represented as tuples.
    # In all type tuples, the first element is a list of type qualifiers.
    # The second element determines how many more elements there are.
    #   - a basic type name (no further elements; it's a basic type)
    #   - t_pointer (followed by the referenced type)
    #   - t_array (followed by the element type and then the dimension;
    #              array dimensions can be numbers, or array_unspecified,
    #              or array_variable)
    #   - t_function (followed by the return type, the number of arguments,
    #                 and a tuple of argument types. The last argument type
    #                 may be (0,t_ellipsis). Argument types may also be
    #                 (0,t_unknown); the number of arguments itself may also
    #                 be func_unspecified.)

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
    def unprotofunc(self, returntype, nargs=func_unspecified):
        if nargs > 0:
            list = nargs * ((0, t_unknown),)
        else:
            list = ()
        return (0, t_function, returntype, nargs, list)
    def protofunc(self, returntype, args):
        return (0, t_function, returntype, len(args), tuple(args))
    def ellipsis(self):
        return (0, t_ellipsis)

    typenames = {
    t_void: "void",
    t_char: "plain char",
    t_schar: "signed char",
    t_uchar: "unsigned char",
    t_short: "signed short",
    t_ushort: "unsigned short",
    t_bfint: "bitfield-int",
    t_int: "signed int",
    t_uint: "unsigned int",
    t_long: "signed long",
    t_ulong: "unsigned long",
    t_longlong: "signed long long",
    t_ulonglong: "unsigned long long",
    t_float: "float",
    t_double: "double",
    t_ldouble: "long double",
    t_bool: "bool",
    t_cfloat: "complex float",
    t_cdouble: "complex double",
    t_cldouble: "complex long double",
    t_ifloat: "imaginary float",
    t_idouble: "imaginary double",
    t_ildouble: "imaginary long double",
    t_ellipsis: "ellipsis",
    t_unknown: "unknown type"}

    def str(self, type):
        s = ""
        if type[1] != t_array:
            if type[0] & tq_const: s = s + "const "
            if type[0] & tq_volatile: s = s + "volatile "
            if type[0] & tq_restrict: s = s + "restrict "
        if type[1] == t_pointer:
            s = s + "pointer to " + self.str(type[2])
        elif type[1] == t_array:
            s = s + "array["
            if type[3] == array_variable:
                s = s + "*"
            elif type[3] != array_unspecified:
                s = s + "%d" % type[3]
            s = s + "] of " + self.str(type[2])
        elif type[1] == t_function:
            s = s + "function("
            if type[3] == func_unspecified:
                s = s + "no prototype"
            elif type[3] == 0:
                s = s + "void"
            else:
                comma = 0
                for i in type[4]:
                    if comma: s = s + ", "
                    s = s + self.str(i)
                    comma = 1
            s = s + ") returning " + self.str(type[2])
        else:
            s = s + self.typenames[type[1]]
        return s

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

    def isvoid(self, type):
        "Determine whether a type is void."
        return type[1] == t_void
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
            raise "FIXME: can't yet do usual-arithmetic-conversions on floats"
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

# Scopes for variables.

class scope:
    "Holding class to store variable declarations/definitions."
    def __init__(self, defsc, sclist, parent):
        self.defaultsc = defsc # default storage class
        self.allowedsc = sclist # allowed storage classes
        self.parent = parent # containing scope
        self.dict = {}
    def findvar(self, varname):
        if self.dict.has_key(varname):
            return self, self.dict[varname]
        elif self.parent == None:
            return None, None
        else:
            return self.parent.findvar(varname)
    def addvar(self, varname, stg, type):
        # FIXME: merge duplicate declarations
        self.dict[varname] = (stg, type)

# The actual semantic analysis phase.

class semantics:
    "Class to perform, and hold the results of, semantic analysis of a C\n"\
    "translation unit."
    
    def __init__(self, parsetree, target, errfunc=defaulterrfunc):
        self.functions = []
        self.topdecls = scope(sc_extern, [sc_extern,sc_global,sc_static], None)
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
            print "topdecl", i[0],
            if i[1] == None:
                print "(no-storage)",
            else:
                print lex_names[i[1]],
            print self.types.str(i[2])

    def do_tran_unit(self, parsetree):
        # Proceed through a translation_unit.
        assert parsetree.type == pt_translation_unit
        for i in parsetree.list:
            if i.type == pt_function_definition:
                self.functions.append(self.do_function(i))
            elif i.type == pt_declaration:
                self.do_declaration(self.topdecls, i)
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

    # Map storage class qualifier keywords to storage classes.
    typequalmap = {
    lt_extern: sc_extern,
    lt_static: sc_static,
    lt_register: sc_register,
    lt_auto: sc_auto,
    }

    # Parse tree nodes we expect in an expression.
    exprnodes = {pt_primary_expression: 1, pt_postfix_expression: 1,
    pt_unary_expression: 1, pt_cast_expression: 1, pt_binary_expression: 1,
    pt_conditional_expression: 1, pt_assignment_expression: 1}

    def expr(self, scope, exp):
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
                s, v = scope.findvar(exp.list[0].text)
                if v == None:
                    # Undeclared identifiers are bad news. Normally they're
                    # simply outright illegal. In a function call context,
                    # though, they cause an implicit declaration of the
                    # identifier as an unprototyped int-returning extern
                    # function.
                    raise "FIXME: undeclared identifier, should handle sanely"
                return v[1], None, 1, optree(op_variable, s, exp.list[0].text)
	    elif exp.list[0].type == pt_charconst:
                raise "FIXME: can't parse char constants yet"
	    elif exp.list[0].type == pt_stringconst:
                raise "FIXME: can't parse string constants yet"
	    elif exp.list[0].type == pt_floatconst:
                raise "FIXME: can't parse float constants yet"
	    else:
                assert 0, "unexpected node in primary_expression"
	elif exp.type == pt_postfix_expression:
            raise "FIXME: can't handle postfix expressions yet"
	elif exp.type == pt_unary_expression:
            if exp.list[0].type == lt_times:
                operand = self.expr(scope, exp.list[1])
                desttype = self.types.referenced(operand[0])
                if desttype == None:
                    self.error(exp, err_deref_nonpointer())
                return desttype, None, 1, optree(operand)
            else:
                #  increment decrement
                #  sizeof
                #  bitand
                #  plus minus bitnot lognot
                raise "FIXME: can't handle most unary operators yet"
        elif exp.type == pt_cast_expression:
            src = self.expr(scope, exp.list[1])
            desttype = self.typename(exp.list[0], scope)
            return self.typecvt(src, desttype, 1)
	elif exp.type == pt_binary_expression:
            lhs = self.expr(scope, exp.list[0])
            rhs = self.expr(scope, exp.list[2])
            return self.binop(exp.list[1].type, lhs, rhs)
	elif exp.type == pt_conditional_expression:
            raise "FIXME: can't handle conditional operator yet"
	elif exp.type == pt_assignment_expression:
            lhs = self.expr(scope, exp.list[0])
            rhs = self.expr(scope, exp.list[2])
            if not lhs[2]:
                self.error(exp, err_assign_nonlval())
            else:
                if lhs[0] != rhs[0]: rhs = self.typecvt(rhs, lhs[0], 0)
                return lhs[0], None, 0, optree(exp.list[1].type, lhs[0], lhs, rhs)
	elif exp.type == pt_constant_expression:
	    type, value, lvalue, tree = self.expr(scope, exp.list[0])
	    if value == None:
                self.error(exp, err_nonconst_expr())
	    return type, value, lvalue, tree
	else:
	    assert 0, "unexpected node in expression"

    def typename(self, tn, scope):
        stg, fnspecs, basetype = self.do_declspecs(tn.list[0])
        t, id = self.do_declarator(tn.list[1], scope, basetype)
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

    def do_declaration(self, scope, decl):
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
        assert decl.list[1].type == pt_init_declarator_list
        for i in decl.list[1].list:
            assert i.type == pt_init_declarator
            dcl = i.list[0]
            assert dcl.type == pt_declarator
            t, id = self.do_declarator(dcl, scope, basetype)
            assert id != None # this declarator may not be abstract
            if stg == lt_typedef:
                self.types.addtypedef(id.text, t);
            else:
                scope.addvar(id.text, stg, t)

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

    def do_declarator(self, dcl, scope, t):
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
            l = dirdcl.list[1:]
            while len(l) > 0:
                # Deal with array and function derivations
                if l[0].type == lt_lparen:
                    i = l[1]
                    # i is either None, or an identifier list, or a
                    # parameter list.
                    if i == None:
                        # Unprototyped function. All we know is that it's
                        # a function.
                        t = self.types.unprotofunc(t)
                    elif i.type == pt_identifier_list:
                        # Function with identifier list given. We now know
                        # the number of arguments at least.
                        t = self.types.unprotofunc(t, len(i.list))
                    elif i.type == pt_parameter_type_list:
                        # Function with genuine prototype.
                        args = []
                        for j in i.list:
                            if j.type == pt_parameter_declaration:
                                argstg, argfnspecs, argbasetype = self.do_declspecs(j.list[0])
                                if len(j.list) > 1:
                                    argtype, argid = self.do_declarator(j.list[1], argbasetype)
                                else:
                                    argtype = argbasetype
                                assert argstg == None and argfnspecs == {}
                                if self.types.isvoid(argtype):
                                    if len(args) > 0 or len(i.list) != 1:
                                        self.error(err_generic_semantic_error()) # void as part of parameter list
                                    else:
                                        break # finished arguments
                                args.append(argtype)
                            elif j.type == lt_ellipsis:
                                args.append(self.types.ellipsis())
                                assert len(args) == len(i.list) # should be last
                        t = self.types.protofunc(t, args)
                elif l[0].type == lt_lbracket:
                    i = l[1]
                    if i == None:
                        t = self.types.array(t, array_unspecified)
                    else:
                        # Attempt to parse i as a constant expression with
                        # integer type.
                        type, value, lvalue, tree = self.expr(scope, i)
                        if value != None:
                            # We have a constant which is our array dimension.
                            # Cast it to type size_t and use it.
                            value = self.target.intconst((0, self.target.size_t), value)
                            # WIBNI: see if `value' has changed, and warn if so
                            t = self.types.array(t, value)
                        else:
                            t = self.types.vla(t, tree)
                l = l[2:]
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
        # FIXME: need a scope contained by topdecls, which will
        # hold the arguments. Then pass _that_ scope to do_block
        # instead of topdecls itself.
	blk = self.do_block(self.topdecls, funcdef.list[3])

    def do_block(self, outerscope, blk):
	assert blk.type == pt_compound_statement
	decls = []
	stmts = []
        blkscope = scope(sc_auto, [sc_auto,sc_register,sc_static], outerscope)
	for i in blk.list:
	    if i.type == pt_declaration:
		self.do_declaration(blkscope, i)
	    else:
		stmts.extend(self.do_statement(blkscope, i))
	return optree(op_blk, decls, stmts)
    
    def do_statement(self, blkscope, stmt):
        ret = []
	while stmt.type == pt_labeled_statement:
            raise "FIXME: can't yet handle labels"
	    stmt = stmt.list[1]
	if self.exprnodes.has_key(stmt.type):
	    type, value, lvalue, tree = self.expr(blkscope, stmt)
            ret.append(tree)
	elif stmt.type == pt_selection_statement:
            condition = self.expr(blkscope, stmt.list[1])
            consequence = self.do_statement(blkscope, stmt.list[2])
            if stmt.list[0].type == lt_if:
                if len(stmt.list) > 3:
                    alternative = self.do_statement(stmt.list[3])
                else:
                    alternative = None
                ret.append(optree(op_if, condition, consequence, alternative))
            else:
                ret.append(optree(op_while, condition, consequence))
	elif stmt.type == pt_iteration_statement and stmt.list[0].type == lt_for:
            if stmt.list[1].type == pt_declaration:
                raise "FIXME: can't yet handle `for' with declaration"
            initialiser = self.expr(blkscope, stmt.list[1])
            test = self.expr(blkscope, stmt.list[2])
            increment = self.expr(blkscope, stmt.list[3])
            loopbody = self.do_statement(blkscope, stmt.list[4])
            ret.append(optree(op_for, initialiser, test, increment, loopbody))
        elif stmt.type == pt_compound_statement:
            newscope = scope(sc_auto, [sc_auto,sc_register,sc_static], blkscope)
            ret.append(self.do_block(newscope, stmt))
        else:
            stmt.display(0)
	    raise "FIXME: can't handle this statement type yet"
        return ret
