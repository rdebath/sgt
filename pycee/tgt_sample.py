from c_semantics import *

class target:
    "Sample target for Pycee."

    intmaxvals = {
    t_int: 0x7FFFFFFFL, t_uint: 0xFFFFFFFFL,
    t_long: 0x7FFFFFFFL, t_ulong: 0xFFFFFFFFL,
    t_longlong: 0x7FFFFFFFFFFFFFFFL, t_ulonglong: 0xFFFFFFFFFFFFFFFFL}

    intminvals = {
    t_int: -0x80000000L, t_uint: 0L,
    t_long: -0x80000000L, t_ulong: 0L,
    t_longlong: -0x8000000000000000L, t_ulonglong: 0L}

    inttouint = {
    t_int: t_uint, t_uint: t_uint,
    t_long: t_ulong, t_ulong: t_ulong,
    t_longlong: t_ulonglong, t_ulonglong: t_ulonglong}

    def intconst(self, type, value):
	# Convert to an unsigned integer of the right type
	value = value & self.intmaxvals[self.inttouint[type]]
	# Convert to a negative number if signed and too big
	if value > self.intmaxvals[type]:
	    value = value - (self.intmaxvals[self.inttouint[type]] + 1)
	return value

    def size_t(self):
	return t_uint
