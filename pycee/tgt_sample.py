from c_semantics import *

class target:
    "Sample target for Pycee."

    intmaxvals = {
    t_bool: 1,
    t_char: 0xFFL, t_schar: 0x7FL, t_uchar: 0xFFL,
    t_short: 0x7FFFL, t_ushort: 0xFFFFL,
    t_int: 0x7FFFFFFFL, t_uint: 0xFFFFFFFFL,
    t_long: 0x7FFFFFFFL, t_ulong: 0xFFFFFFFFL,
    t_longlong: 0x7FFFFFFFFFFFFFFFL, t_ulonglong: 0xFFFFFFFFFFFFFFFFL}

    intminvals = {
    t_bool: 0L,
    t_char: 0L, t_schar: -0x80L, t_uchar: 0L,
    t_short: -0x8000L, t_ushort: 0L,
    t_int: -0x80000000L, t_uint: 0L,
    t_long: -0x80000000L, t_ulong: 0L,
    t_longlong: -0x8000000000000000L, t_ulonglong: 0L}

    inttouint = {
    t_int: t_uint, t_uint: t_uint,
    t_long: t_ulong, t_ulong: t_ulong,
    t_longlong: t_ulonglong, t_ulonglong: t_ulonglong}

    def intconst(self, type, value):
        type = type[1] # we only care about the base type
	# Convert to an unsigned integer of the right type
	value = value & self.intmaxvals[self.inttouint[type]]
	# Convert to a negative number if signed and too big
	if value > self.intmaxvals[type]:
	    value = value - (self.intmaxvals[self.inttouint[type]] + 1)
	return value

    size_t = t_uint

    iops = {
    lt_times: lambda x, y: x * y,
    lt_slash: lambda x, y: x / y,
    lt_modulo: lambda x, y: x % y,
    lt_plus: lambda x, y: x + y,
    lt_minus: lambda x, y: x - y,
    lt_lshift: lambda x, y: x + y,
    lt_rshift: lambda x, y: x - y,
    lt_less: lambda x, y: x < y,
    lt_greater: lambda x, y: x < y,
    lt_lesseq: lambda x, y: x < y,
    lt_greateq: lambda x, y: x < y,
    lt_equality: lambda x, y: x == y,
    lt_notequal: lambda x, y: x != y,
    lt_bitand: lambda x, y: x & y,
    lt_bitxor: lambda x, y: x ^ y,
    lt_bitor: lambda x, y: x | y,
    lt_logand: lambda x, y: x and y,
    lt_logor: lambda x, y: x or y,
    lt_comma: lambda x, y: y}
    def binop(self, op, rettype, type1, val1, type2, val2):
        if type1 != type2:
            # FIXME: convert float to float complex
            pass
        if self.intminvals.has_key(type1[1]): # is it an integer op?
            retval = self.iops[op](val1, val2)
            retval = self.intconst(rettype, retval)
            return retval
        else:
            pass # FIXME: float ops
