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
op_cast = _enum(t_END) # (srctype, desttype, operand)
op_END = _enum()

def _sortt(list):
    "Internal function that sorts a list and tuples it."
    newlist = list[:] # copy it
    newlist.sort()
    return tuple(newlist)

class typesystem:
    "Class to hold a system of types."

    def __init__(self):
        self.uniqueid = 0
        self.typedefs = {}

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

# The actual semantic analysis phase.

class semantics:
    "Class to perform, and hold the results of, semantic analysis of a C\n"\
    "translation unit."
    
    def __init__(self, parsetree, target):
        self.functions = []
        self.topdecls = []
        self.types = typesystem()
	self.target = target
        self.do_tran_unit(parsetree)

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
	    elif exp.list[0].type == pt_ident:
		pass # FIXME
	    elif exp.list[0].type == pt_charconst:
		pass # FIXME
	    elif exp.list[0].type == pt_stringconst:
		pass # FIXME
	    elif exp.list[0].type == pt_floatconst:
		pass # FIXME
	    else:
		FIXME()
	    pass
	elif exp.type == pt_postfix_expression:
	    pass
	elif exp.type == pt_unary_expression:
	    pass
	elif exp.type == pt_cast_expression:
	    pass
	elif exp.type == pt_binary_expression:
	    pass
	elif exp.type == pt_conditional_expression:
	    pass
	elif exp.type == pt_assignment_expression:
	    pass
	elif exp.type == pt_constant_expression:
	    type, value, lvalue, tree = self.expr(exp.list[0])
	    if value == None:
		ERROR() # non-constant constant expression
	    return type, value, lvalue, tree
	else:
	    assert 0, "unexpected node in expression"

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
        stg = None
        typequals = 0
        fnspecs = {}
        typespecs = []
        for i in specs.list:
            if i.type == pt_storage_class_qualifier:
                if stg != None:
                    ERROR() # more than one storage class qualifier
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
            ERROR() # unrecognised type specifier `typespecs'

        # We now have our base type. Go through the declarators processing
        # each one to create derived variants from the base type.
        defs = []
        assert decl.list[1].type == pt_init_declarator_list
        for i in decl.list[1].list:
            assert i.type == pt_init_declarator
            dcl = i.list[0]
            assert dcl.type == pt_declarator
            t = basetype
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
			    value = self.target.intconst(self.target.size_t(), value)
			    # WIBNI: see if `value' has changed, and warn if so
			    t = self.types.array(t, value)
			else:
			    t = self.types.vla(t, tree)
                if dirdcl.list[0].type == pt_declarator:
                    dcl = dirdcl.list[0]
                    continue
                else:
                    id = dirdcl.list[0]
                    assert id.type == lt_ident
                    break
            if stg == lt_typedef:
                self.types.addtypedef(id.text, t);
            else:
                defs = defs + [[id.text, stg, t]]
        return defs

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
	while stmt.type == pt_labeled_statement:
	    # FIXME: deal with label
	    stmt = stmt.list[1]
	if self.exprnodes.has_key(stmt.type):
	    type, value, lvalue, tree = self.expr(stmt)
	    return tree
	else:
	    FIXME()
