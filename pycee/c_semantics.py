# Semantic analysis for C

# Reads a parse tree. Outputs a set of types, a flattened tree giving the
# interior of each function, and at every block level a description of all
# variable definitions and declarations. The code tree for each function
# has already had its integer promotions and so on performed, so all
# arithmetic operations are between correct (and explicitly specified) types
# and all implicit type conversions are made explicit.

# Remaining serious FIXMEs are ...
#
# - Castability.
#
# - Type unification (filling in blanks in parameter lists, adding dimension
#   to unspecified-length arrays).
#
# - Initialisers.
#
# - Floats.
#
# - Missing operators: sizeof, unary &, ?:.
#
# - Bitfields.
#
# - Old-style (K&R) argument declarations.
#
# - Missing statements: goto, break, continue, do...while, switch.
#
# - Labelled statements.

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
sc_none = _enum()

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
op_strconst = _enum() # (stringdata)
op_variable = _enum() # (scope, varname)
op_cast = _enum() # (srctype, desttype, operand)
op_deref = _enum() # (operand)
op_if = _enum() # (condition, thenclause, elseclause)
op_while = _enum() # (condition, loopbody)
op_for = _enum() # (initialiser, test, increment, loopbody)
op_return = _enum() # (value)
op_preinc = _enum() # (operand, incrementquantity)
op_postinc = _enum() # (operand, incrementquantity)
op_predec = _enum() # (operand, incrementquantity)
op_postdec = _enum() # (operand, incrementquantity)
op_structmember = _enum() # (operand, uid, s_or_u, membernumber)
op_funcall = _enum() # (function, argumenttuple)
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
    #   - t_struct (followed by a structure uid)
    #   - t_union (followed by a union uid)

    def __init__(self, target):
        self.uniqueid = 0
        self.structures = {}
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
    structunionmap = { lt_struct: t_struct, lt_union: t_union }
    structunionbackmap = { t_struct: lt_struct, t_union: lt_union }
    def structtype(self, s_or_u, uid):
        return (0, self.structunionmap[s_or_u], uid)

    def mkuid(self):
        self.uniqueid = self.uniqueid + 1
        return self.uniqueid
    def mkstruct(self, uid, s_or_u, members):
        self.structures[(s_or_u,uid)] = members
    def findstruct(self, uid, s_or_u):
        if self.structures.has_key((s_or_u,uid)):
            return self.structures[(s_or_u,uid)]
        else:
            return None
    def structfromtype(self, type):
        if self.structunionbackmap.has_key(type[1]):
            return self.findstruct(type[2], self.structunionbackmap[type[1]])
        return None

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
    t_struct: "struct",
    t_union: "union",
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
        elif type[1] == t_struct or type[1] == t_union:
            s = s + self.typenames[type[1]] + "(" + str(type[2]) + ")"
            members = self.findstruct(self.structunionbackmap[type[1]],type[2])
        else:
            s = s + self.typenames[type[1]]
        return s

    def display(self):
        for i in self.structures.keys():
            print self.typenames[self.structunionmap[i[0]]], i[1], ":",
            members = self.structures[i]
            if members:
                print "{",
                comma = 0
                for i in members:
                    if comma: print ",",
                    print i[1], ":", self.str(i[0]),
                    comma = 1
                print "}"
            else:
                print "incomplete"


    def qualify(self, type, quals):
        # Add in extra type qualifiers
        ret = (type[0] | quals,) + type[1:]
        return ret

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
    def isfunction(self, type):
        "Determine whether a type is a function."
        return type[1] == t_function

    def isarray(self, type):
        "Determine whether a type is an array."
        return type[1] == t_array
    def ispointer(self, type):
        "Determine whether a type is a pointer."
        return type[1] == t_pointer

    def referenced(self, type):
        "Return the referenced type of a pointer, or None if not a pointer."
        if type[1] == t_pointer:
            return type[2]
        else:
            return None

    def transmogrify(self, type):
        "Transmogrify function types into function pointer types, and\n"\
        "turn array types into pointers to their element type."
        if type[1] == t_array:
            # Merge qualifiers in from the original type.
            elttype = (type[0] | type[2][0],) + type[2][1:]
            # Now make a pointer to that.
            return self.ptrtype(elttype, 0)
        elif type[1] == t_function:
            # Just make a pointer.
            return self.ptrtype(type, 0)
        # Neither an array nor a function. Do nothing.
        return type

    def returned(self, type):
        "Return the returned type of a function, or None if not a function."
        if type[1] == t_function:
            return type[2]
        else:
            return None
    def isvariadic(self, type):
        "Determine whether a function type is explicitly variadic."
        return type[3] != func_unspecified and type[4][type[3]-1][1] == t_ellipsis
    def nargs(self, type):
        "Return the number of arguments of a function, or\n"\
        "func_unspecified. Can return None if not a function.\n"\
        "For variadic functions, returns the _minimum_ argument count."
        if type[1] == t_function:
            return type[3] - self.isvariadic(type)
        else:
            return None
    def argument(self, type, n):
        "Return an argument type of a function, or None if not a function."
        if type[1] == t_function:
            return type[4][n]
        else:
            return None
    def isunknown(self, type):
        "Determine whether a type (e.g. of a function argument) is unknown."
        return type[1] == t_unknown

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

    def unify(self, type1, type2):
        "Unify two types. Return the unified type, or None if incompatible."
        if type1[0] != type2[0]:
            return None
        if type1[1] != type2[1]:
            return None
        if type1[1] == t_pointer or type1[1] == t_struct or type1[1] == t_union:
            if type1[2] != type2[2]:
                return None
            return type1
        if type1[1] == t_array:
            if type1[2] != type2[2]:
                return None
            if type1[2] == array_unspecified:
                type1 = type1[:2] + (type2[2],)
            if type2[2] != array_unspecified and type2[2] != type1[2]:
                return None
        if type1[1] == t_function:
            if type1[2] != type2[2]:
                return None
            if type1[3] == func_unspecified:
                FIXME() # finish this stuff

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
        self.warning = 0
    def warning(self, errfunc):
        self.warning = 1
        errfunc(self)
class err_string_bogusesc(err_generic_semantic_error):
    def __init__(self, char):
        self.message = "invalid escape sequence in string: `\\" + char + "'"
class err_implicitdecl(err_generic_semantic_error):
    def __init__(self, fname):
        self.message = "implicitly declaring `extern int " + fname + "()'"
class err_unop_arith(err_generic_semantic_error):
    def __init__(self, op):
        self.message = "operator " + lex_names[op] + " requires operand of arithmetic type"
class err_unop_arithptr(err_generic_semantic_error):
    def __init__(self, op):
        self.message = "operator " + lex_names[op] + " requires operand of arithmetic or pointer type"
class err_unop_int(err_generic_semantic_error):
    def __init__(self, op):
        self.message = "operator " + lex_names[op] + " requires operand of integer type"
class err_deref_nonpointer(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to dereference a non-pointer"
class err_subscript_nonpointer(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to apply array subscript to non-pointers"
class err_arrow_nonpointer(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to apply `->' operator to a non-pointer"
class err_call_nonfnptr(err_generic_semantic_error):
    def __init__(self):
        self.message = "attempt to make a function call to a non-function-pointer"
class err_structure_nosuchmember(err_generic_semantic_error):
    def __init__(self, membername):
        self.message = "structure type has no field called `"+membername+"'"
class err_narg_mismatch(err_generic_semantic_error):
    def __init__(self, variadic, funccount, callcount):
        self.message = "function of " + "%d"%funccount
        if variadic:
            self.message = self.message + " or more"
        self.message = self.message + " arguments called with " + "%d"%callcount
class err_undecl_ident(err_generic_semantic_error):
    def __init__(self, membername):
        self.message = "undeclared identifier `"+membername+"'"
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
    if err.warning:
        warning = "warning: "
    else:
        warning = ""
    sys.stderr.write("<input>:" + "%d:%d: " % (err.line, err.col) + warning + err.message + "\n")

# Scopes for variables, typedefs, struct and union tags, etc.

class scope:
    "Holding class to store variable declarations/definitions."
    def __init__(self, defsc, sclist, parent, types=None):
        self.defaultsc = defsc # default storage class
        self.allowedsc = sclist # allowed storage classes
        self.parent = parent # containing scope
        if parent != None:
            self.types = parent.types
        else:
            self.types = types # type system
        self.dict = {}
        self.typedefs = {}
        self.structdict = {}
        self.uniondict = {}
        self.sudict = {lt_struct: self.structdict, lt_union: self.uniondict}
    def findvar(self, varname):
        if self.dict.has_key(varname):
            return self, self.dict[varname]
        elif self.parent == None:
            return None, None
        else:
            return self.parent.findvar(varname)
    def addvar(self, varname, stg, type):
        if self.dict.has_key(varname):
            FIXME() # merge duplicate declarations
        self.dict[varname] = (stg, type)

    def addtypedef(self, name, type):
        self.typedefs[name] = type
    def findtypedef(self, name, quals):
        if self.typedefs.has_key(name):
            type = self.typedefs[name]
            return self.types.qualify(type, quals)
        elif self.parent != None:
            return self.parent.findtypedef(name, quals)
        else:
            return None

    def defstructtag(self, s_or_u, name, members):
        d = self.sudict[s_or_u]
        if d.has_key(name):
            uid = d[name]
            if self.types.findstruct(uid, s_or_u):
                return 0 # failure
        else:
            uid = self.types.mkuid()
            d[name] = uid
        self.types.mkstruct(uid, s_or_u, members)
        return 1
    def findstructtag(self, s_or_u, name, create=1):
        d = self.sudict[s_or_u]
        if d.has_key(name):
            return self.types.structtype(s_or_u, d[name])
        else:
            if self.parent != None:
                ret = self.parent.findstructtag(s_or_u, name, 0)
            else:
                ret = None
            if ret != None:
                return ret
            elif create:
                uid = self.types.mkuid()
                d[name] = uid
                return self.types.structtype(s_or_u, d[name])
            else:
                return None # failure

    def display(self):
        for i in self.dict.keys():
            print "decl", i,
            j = self.dict[i]
            if j[0] == None:
                print "(no-storage)",
            else:
                print lex_names[j[0]],
            print self.types.str(j[1])

# The actual semantic analysis phase.

class semantics:
    "Class to perform, and hold the results of, semantic analysis of a C\n"\
    "translation unit."

    def __init__(self, parsetree, target, errfunc=defaulterrfunc):
        self.functions = []
	self.target = target
        self.types = typesystem(target)
        self.topdecls = scope(sc_extern, [sc_extern,sc_global,sc_static], None, self.types)
        self.errfunc = errfunc
        try:
            self.do_tran_unit(parsetree)
        except err_generic_semantic_error, err:
            self.errfunc(err)

    def error(self, object, error):
        error.line = object.line
        error.col = object.col
        raise error

    def warning(self, object, error):
        error.line = object.line
        error.col = object.col
        error.warning(self.errfunc)

    def display(self):
        self.types.display()
        self.topdecls.display()

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

    strconstmap = {"a":"\007","b":"\010","f":"\014","n":"\012",
    "r":"\015","t":"\011","v":"\013"}

    def strconst(self, exp, instr):
        "Parse the inside of a string constant, returning a string."
        ret = ""
        while instr != "":
            if instr[0] == "\\":
                if len(instr) == 0:
                    raise "Should never happen! Backslash before close quote."
                if instr[1] in "xX":
                    i = 2
                    while i < len(instr) and instr[i] in instr.hexdigits:
                        i = i + 1
                    ret = ret + chr(string.atoi(instr[2:i], 16))
                elif instr[1] in "01234567":
                    i = 2
                    while i < len(instr) and i < 4 and instr[i] in "01234567":
                        i = i + 1
                    ret = ret + chr(string.atoi(instr[1:i], 8))
                elif instr[1] in "abfnrtv":
                    i = 2
                    ret = ret + self.strconstmap[instr[1]]
                else:
                    self.error(exp, err_string_bogusesc(instr[1]))
            else:
                i = 1
                ret = ret + instr[0]
            instr = instr[i:]
        return ret

    def expr(self, scope, exp, funccontext=0, wholevarcontext=0):
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
                    # Undeclared identifiers are normally simply outright
                    # illegal. In a function call context, though, they
                    # cause an implicit declaration of the identifier as
                    # an unprototyped int-returning extern function.
                    if funccontext:
                        self.warning(exp, err_implicitdecl(exp.list[0].text))
                        ftype = self.types.basictype(t_int, 0)
                        ftype = self.types.unprotofunc(ftype)
                        scope.addvar(exp.list[0].text, sc_extern, ftype)
                        s, v = scope.findvar(exp.list[0].text)
                    else:
                        self.error(exp, err_undecl_ident(exp.list[0].text))
                # Fix up types. Functions and arrays must become pointers
                # unless we're in whole-variable context.
                type = v[1]
                if not wholevarcontext:
                    type = self.types.transmogrify(type)
                return type, None, 1, optree(op_variable, s, exp.list[0].text)
	    elif exp.list[0].type == lt_charconst:
                text = exp.list[0].text
                assert len(text) >= 2 and text[0] == "'" and text[-1] == "'"
                const = self.strconst(exp, text[1:-1])
                if len(const) != 1:
                    self.error(exp, err_charconst_notlen1())
                return (0, t_char), ord(const), 0, optree(t_char, ord(const))
	    elif exp.list[0].type == lt_stringconst:
                text = exp.list[0].text
                assert len(text) >= 2 and text[0] == '"' and text[-1] == '"'
                const = self.strconst(exp, text[1:-1]) + "\0"
                # CONFIGURABLE: could make strings writable here
                type = self.types.ptrtype((tq_const, t_char), 0)
                return type, None, 0, optree(op_strconst, const)
	    elif exp.list[0].type == lt_floatconst:
                raise "FIXME: can't parse float constants yet"
	    else:
                assert 0, "unexpected node in primary_expression"
	elif exp.type == pt_postfix_expression:
            if exp.list[0].type == pt_type_name:
                operand = self.expr(scope, exp.list[0])
                raise "FIXME: C9X (type_name){initializer_list} NYI"
            elif exp.list[1].type in [lt_increment, lt_decrement]:
                operand = self.expr(scope, exp.list[0])
                return self.incdec(operand, exp.list[1].type, op_postinc, op_postdec)
            elif exp.list[1].type == pt_argument_expression_list:
                # For _this_ recursive call to expr, we set the `funccontext'
                # parameter, because a pure identifier at the next level down
                # gets special treatment if it's undeclared: any other
                # undeclared identifier is shot on sight, but this one is
                # implicitly declared as an unprototyped extern function
                # returning int.
                operand = self.expr(scope, exp.list[0], funccontext=1)
                # Now we must look up the type of the function, and perform
                # type conversions on each argument before constructing the
                # final optree.
                functype = self.types.referenced(operand[0])
                if functype == None or not self.types.isfunction(functype):
                    self.error(exp, err_call_nonfnptr())
                rettype = self.types.returned(functype)
                nargs = self.types.nargs(functype)
                npar = len(exp.list[1].list)
                variad = self.types.isvariadic(functype)
                # If the function is prototyped, check the argument count.
                if nargs != func_unspecified:
                    if npar != nargs and (npar < nargs or not variad):
                        self.error(exp, err_narg_mismatch(variad, nargs, npar))
                # Evaluate each argument and convert to a sensible type.
                # (For arguments whose type is given in the prototype, this
                # is a simple implicit cast. For others, we must be
                # a bit more flexible.)
                argexprs = ()
                for i in range(npar):
                    arg = self.expr(scope, exp.list[1].list[i])
                    if nargs == func_unspecified:
                        # Argument to an unprototyped function.
                        # Do the usual arithmetic conversions.
                        if self.types.isarith(arg[0]):
                            type = self.types.usualarith(arg[0], arg[0])
                            assert type != None
                            if type != arg[0]:
                                arg = self.typecvt(arg, type, 0)
                    elif i >= nargs:
                        # Argument to a variadic function.
                        # Do the usual arithmetic conversions.
                        if self.types.isarith(arg[0]):
                            type = self.types.usualarith(arg[0], arg[0])
                            assert type != None
                            if type != arg[0]:
                                arg = self.typecvt(arg, type, 0)
                    else:
                        type = self.types.argument(functype, i)
                        if not self.types.isunknown(type) and arg[0] != type:
                            arg = self.typecvt(arg, type, 0)
                        else:
                            # Argument to a partially prototyped function.
                            # Do the usual arithmetic conversions.
                            if self.types.isarith(arg[0]):
                                type = self.types.usualarith(arg[0], arg[0])
                                assert type != None
                                if type != arg[0]:
                                    arg = self.typecvt(arg, type, 0)
                return rettype, None, 0, optree(op_funcall, operand, argexprs)
            elif exp.list[1].type in [lt_dot, lt_arrow]:
                operand = self.expr(scope, exp.list[0])
                structtype = operand[0]
                if exp.list[1].type == lt_arrow:
                    structtype = self.types.referenced(structtype)
                    if structtype == None:
                        self.error(exp, err_arrow_nonpointer())
                    operand = (structtype, None, 1, optree(op_deref, operand))
                structure = self.types.structfromtype(structtype)
                for i in range(len(structure)):
                    member = structure[i]
                    if member[1] == exp.list[2].text:
                        break
                else:
                    self.error(exp, err_structure_nosuchmember(exp.list[2].text))
                ret = member[0], None, operand[2], \
                optree(op_structmember, operand, structure, i)
                return ret
            else:
                # FIXME. Could insert warning here that says the _outer_
                # thing should be the pointer and not the subscript.
                operand = self.expr(scope, exp.list[0])
                subscript = self.expr(scope, exp.list[1])
                sum = self.binop(exp, lt_plus, operand, subscript)
                # Now dereference.
                desttype = self.types.referenced(sum[0])
                if desttype == None:
                    self.error(exp, err_subscript_nonpointer())
                # Transmogrify types: function -> function ptr, and
                # array -> ptr to element type.
                if not wholevarcontext:
                    desttype = self.types.transmogrify(desttype)
                return desttype, None, 1, optree(op_deref, sum)
	elif exp.type == pt_unary_expression:
            if exp.list[0].type == lt_times:
                operand = self.expr(scope, exp.list[1])
                desttype = self.types.referenced(operand[0])
                if desttype == None:
                    self.error(exp, err_deref_nonpointer())
                # Transmogrify types: function -> function ptr, and
                # array -> ptr to element type.
                if not wholevarcontext:
                    desttype = self.types.transmogrify(desttype)
                return desttype, None, 1, optree(op_deref, operand)
            elif exp.list[0].type in [lt_increment, lt_decrement]:
                operand = self.expr(scope, exp.list[1])
                return self.incdec(operand, exp.list[0].type, op_preinc, op_predec)
            elif exp.list[0].type in [lt_plus, lt_minus, lt_bitnot, lt_lognot]:
                operand = self.expr(scope, exp.list[1]) 
                return self.unop(exp, exp.list[0].type, operand)
            elif exp.list[0].type == lt_sizeof:
                raise "FIXME: sizeof NYI"
            elif exp.list[0].type == lt_bitand:
                raise "FIXME: address-take NYI"
        elif exp.type == pt_cast_expression:
            src = self.expr(scope, exp.list[1])
            desttype = self.typename(exp.list[0], scope)
            return self.typecvt(src, desttype, 1)
	elif exp.type == pt_binary_expression:
            lhs = self.expr(scope, exp.list[0])
            rhs = self.expr(scope, exp.list[2])
            return self.binop(exp, exp.list[1].type, lhs, rhs)
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
    
    def incdec(self, operand, which, opinc, opdec):
        if which == lt_increment:
            op = opinc
        else:
            op = opdec
        if not operand[2]:
            self.error(exp, err_assign_nonlval())
        if self.types.isarith(operand[0]):
            lt, rt, outt = self.types.usualarith(operand[0], operand[0])
            assert outt != None
            increment = 1
        elif self.types.ispointer(operand[0]):
            outt = operand[0]
            increment = self.target.typesize(self.types.referenced(operand[0]))
        else:
            raise "FIXME: ++ or -- on wrong type, need proper error"
        if operand[0] != outt: operand = self.typecvt(operand, outt, 0)
        return operand[0], None, 0, optree(op, operand, increment)

    def typename(self, tn, scope):
        stg, fnspecs, basetype = self.do_declspecs(tn.list[0], scope)
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

    def binop(self, exp, op, lhs, rhs):
        "Process a binary operator."
        # Must perform type conversion.
        if self.types.isarith(lhs[0]) and self.types.isarith(rhs[0]):
            lt, rt, outt = self.types.usualarith(lhs[0], rhs[0])
            if outt == None:
                self.error(lhs, err_incompat_types_binop(op))
            if lhs[0] != lt: lhs = self.typecvt(lhs, lt, 0)
            if rhs[0] != rt: rhs = self.typecvt(lhs, rt, 0)
        elif op in [lt_plus, lt_minus] and \
        (self.types.isarith(lhs[0]) and self.types.ispointer(rhs[0])) or \
        (self.types.isarith(rhs[0]) and self.types.ispointer(lhs[0])):
            # Canonify so pointer is on the left.
            if self.types.ispointer(rhs[0]):
                rhs, lhs = lhs, rhs
            # Integer-promote the integer.
            discard1, discard2, intt = self.types.usualarith(rhs[0], rhs[0])
            # Find the element size of the pointer.
            esize = self.target.typesize(self.types.referenced(lhs[0]))
            # Multiply the integer by that.
            val = self.target.intconst(intt, esize)
            esizeoptree = intt, val, 0, optree(intt, val)
            distance = optree(lt_times, intt, rhs, esizeoptree)
            # Now do the addition.
            return lhs[0], None, 0, optree(op, lhs[0], lhs, distance)
        else:
            self.error(exp, err_binop_type_mismatch)
        # Perform constant arithmetic if possible.
        if lhs[1] != None and rhs[1] != None:
            newval = self.target.binop(op, outt, lt, lhs[1], rt, rhs[1])
            return outt, newval, 0, optree(outt, newval)
        else:
            return outt, None, 0, optree(op, outt, lhs, rhs)

    def unop(self, exp, op, operand):
        "Process a unary arithmetic operator: + - ! ~"
        # Must perform type conversion.
        if self.types.isarith(operand[0]):
            lt, rt, outt = self.types.usualarith(operand[0], operand[0])
            assert outt != None
            if operand[0] != outt: operand = self.typecvt(operand, outt, 0)
        # Type checking: for + and - we need an arithmetic type,
        # for ~ we need an _integer_ type, and for ! we need either
        # an arithmetic type or a pointer.
        if op in [lt_plus, lt_minus] and not self.types.isarith(operand[0]):
            self.error(err_unop_arith(op))
        if op == lt_bitnot and not self.types.isint(operand[0]):
            self.error(err_unop_int(op))
        if op == lt_lognot and not self.types.isarith(operand[0]) \
        and not self.types.ispointer(operand[0]):
            self.error(err_unop_arithptr(op))
        # Perform constant arithmetic if possible.
        if operand[1] != None:
            newval = self.target.unop(op, outt, operand[1])
            return outt, newval, 0, optree(outt, newval)
        else:
            return outt, None, 0, optree(op, outt, operand)

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
        stg, fnspecs, basetype = self.do_declspecs(specs, scope)
        # We now have our base type. Go through the declarators processing
        # each one to create derived variants from the base type.
        if len(decl.list) < 2:
            # FIXME: absence of declarations is OK when it's `struct Foo;'
            # but not OK when it's just `int;'. The former has an effect.
            return # there were no declarations
        assert decl.list[1].type == pt_init_declarator_list
        for i in decl.list[1].list:
            assert i.type == pt_init_declarator
            dcl = i.list[0]
            assert dcl.type == pt_declarator
            t, id = self.do_declarator(dcl, scope, basetype)
            assert id != None # this declarator may not be abstract
            if stg == lt_typedef:
                scope.addtypedef(id.text, t);
            else:
                scope.addvar(id.text, stg, t)

    def do_declspecs(self, specs, scope):
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
            basetype = self.structuniontype(tspec, scope)
        elif sts == (pt_enum_specifier,):
            basetype = self.enumtype(tspec)
        elif sts == (lt_typedefname,):
            basetype = scope.findtypedef(tspec.text, typequals)
        else:
            self.error(specs, err_invalid_type(typespecs))
        return stg, fnspecs, basetype

    def structuniontype(self, typespec, vscope):
        # Convert a pt_struct_or_union_specifier into a structure type.
        if typespec.list[1] == None:
            # Name is left blank, so a list must be specified. This
            # type is unique to the declaration: contrive a unique name.
            name = "+" + str(self.uniqueids)
            self.uniqueids = self.uniqueids + 1
        else:
            name = typespec.list[1].text
        # We have the structure name. See if the structure is defined.
        if typespec.list[2] != None:
            # Create the structure tag, to ensure that structure types
            # can self-refer.
            vscope.findstructtag(typespec.list[0].type, name)
            # Process the list of structure members, in a subscope.
            subscope = scope(sc_none, [sc_none], vscope)
            decls = typespec.list[2]
            members = ()
            for decl in decls.list:
                stg, fnspecs, basetype = self.do_declspecs(decl.list[0], subscope)
                for dcl in decl.list[1].list:
                    if len(dcl.list) > 1:
                        FIXME() # bitfields not yet supported
                    t, id = self.do_declarator(dcl.list[0], subscope, basetype)
                    members = members + ((t, id.text),)
            if not vscope.defstructtag(typespec.list[0].type, name, members):
                FIXME() # attempted to redefine structure in same scope
        type = vscope.findstructtag(typespec.list[0].type, name)
        return type

    def do_declarator(self, dcl, vscope, t, argscope=None):
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
                        if argscope == None:
                            argscope = scope(sc_auto, [sc_auto,sc_register], vscope)
                        for j in i.list:
                            if j.type == pt_parameter_declaration:
                                argstg, argfnspecs, argbasetype = self.do_declspecs(j.list[0], argscope)
                                if len(j.list) > 1:
                                    argtype, argid = self.do_declarator(j.list[1], argscope, argbasetype)
                                    if argid != None:
                                        argscope.addvar(argid.text, argstg, argtype)
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
                        type, value, lvalue, tree = self.expr(vscope, i)
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
        stg, fnspecs, basetype = self.do_declspecs(funcdef.list[0], self.topdecls)
        assert funcdef.list[1].type == pt_declarator
        argscope = scope(sc_auto, [sc_auto,sc_register], self.topdecls)
        t, id = self.do_declarator(funcdef.list[1], self.topdecls, basetype, argscope)
        if not self.types.isfunction(t):
            FIXME() # function declaration with non-function declarator
        rettype = self.types.returned(t)
        # FIXME: go through potential list-of-oldstyle-argument-decls
	blk = self.do_block(argscope, rettype, funcdef.list[3])
        # FIXME: assign `value' of function

    def do_block(self, outerscope, rettype, blk):
	assert blk.type == pt_compound_statement
	stmts = []
        blkscope = scope(sc_auto, [sc_auto,sc_register,sc_static], outerscope)
	for i in blk.list:
	    if i.type == pt_declaration:
		self.do_declaration(blkscope, i)
	    else:
		stmts.extend(self.do_statement(blkscope, rettype, i))
	return optree(op_blk, blkscope, stmts)
    
    def do_statement(self, blkscope, rettype, stmt):
        ret = []
	while stmt.type == pt_labeled_statement:
            raise "FIXME: can't yet handle labels"
	    stmt = stmt.list[1]
	if self.exprnodes.has_key(stmt.type):
	    type, value, lvalue, tree = self.expr(blkscope, stmt)
            ret.append(tree)
	elif stmt.type == pt_selection_statement and stmt.list[0].type == lt_if:
            condition = self.expr(blkscope, stmt.list[1])
            consequence = self.do_statement(blkscope, rettype, stmt.list[2])
            if len(stmt.list) > 3:
                alternative = self.do_statement(blkscope, rettype, stmt.list[3])
            else:
                alternative = None
            ret.append(optree(op_if, condition, consequence, alternative))
	elif stmt.type == pt_iteration_statement and stmt.list[0].type == lt_for:
            if stmt.list[1].type == pt_declaration:
                raise "FIXME: can't yet handle `for' with declaration"
            initialiser = self.expr(blkscope, stmt.list[1])
            test = self.expr(blkscope, stmt.list[2])
            increment = self.expr(blkscope, stmt.list[3])
            loopbody = self.do_statement(blkscope, rettype, stmt.list[4])
            ret.append(optree(op_for, initialiser, test, increment, loopbody))
	elif stmt.type == pt_iteration_statement and stmt.list[0].type == lt_while:
            condition = self.expr(blkscope, stmt.list[1])
            loopbody = self.do_statement(blkscope, rettype, stmt.list[2])
            ret.append(optree(op_while, condition, loopbody))
	elif stmt.type == pt_jump_statement and stmt.list[0].type == lt_return:
            if len(stmt.list) == 1:
                # Void return. FIXME: warn if rettype isn't void
                retval = None
            else:
                retval = self.expr(blkscope, stmt.list[1])
                retval = self.typecvt(retval, rettype, 0)
            ret.append(optree(op_return, retval))
        elif stmt.type == pt_compound_statement:
            newscope = scope(sc_auto, [sc_auto,sc_register,sc_static], blkscope)
            ret.append(self.do_block(newscope, rettype, stmt))
        else:
            stmt.display(0)
	    raise "FIXME: can't handle this statement type yet"
        return ret
