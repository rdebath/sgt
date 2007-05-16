#!/usr/bin/env python

import sys
import errno

# Spigot algorithm to produce the digits of pi and e, without
# having to commit in advance to the number of digits desired (so
# you can just keep going until memory or CPU time runs out).
# Implemented from the description given in
#
# http://web.comlab.ox.ac.uk/oucl/work/jeremy.gibbons/publications/spigot.pdf
#
# and enhanced (for e, different bases and continued fractions) by
# me.

# Usage:
#   spigot.py pi                 # generates pi in base 10
#   spigot.py -b7 pi             # generates pi in another base, e.g. 7
#   spigot.py -c pi              # generates continued fraction for pi
#   spigot.py e                  # generates e
#   spigot.py phi                # generates phi
#   spigot.py root 3             # generates the square root of 3
#   spigot.py frac 3 15          # generates the rational 3/15
#
# You can also feed a stream of input to the program and it will
# convert between continued fraction terms and an arbitrary number base, or between number bases:
#   spigot.py cfrac              # reads continued fraction terms from stdin
#   spigot.py base 9             # reads a base-9 number from stdin

# Method (hopefully a slightly more accessible description than the
# one in the paper cited above):
#
# Pi can be represented as the sum of a series that goes
#   2 + (1/3 * 2) + (1/3 * 2/5 * 2) + (1/3 * 2/5 * 3/7 * 2) + ...
# = 2 + 1/3 * (2 + 2/5 * (2 + 3/7 * (2 + 4/9 * (...))))
#
# Another way of writing this is as an infinite composition of
# _functions_:
#
#   (x |-> 2+x/3) o (x |-> 2+2x/5) o (x |-> 2+3x/7) o ...
#
# This infinite composition converges, in the sense that if you
# take all of its `partial sums' (obtained by cutting it off after
# n terms) and examine the effect of each of those on the interval
# [3,4], you find that they map that interval to a sequence of
# nested subintervals which converge to the single point pi.
#
# So if we could efficiently generate those `partial sums' one
# after another, we could examine their effect on the interval
# [3,4] and whenever the images under a `partial sum' of the two
# ends of that interval agreed to the first n decimal places, then
# we would know we had found the first n digits of pi.
#
# These functions are all Mobius transformations, i.e. functions of
# the form x |-> (px+q)/(rx+s). These have the property that we can
# represent them as 2x2 matrices, and compose them by matrix
# multiplication. So the functions become a sequence of 2x2 integer
# matrices:
#
#   ( 1 6 )  ( 2 10 )  ( 3 14 ) ... ( k 4k+2 ) ...
#   ( 0 3 )  ( 0  5 )  ( 0  7 )     ( 0 2k+1 )
#
# Thus, we can compute successive partial products of this sequence
# of matrices simply by starting with the identity matrix and
# repeatedly post-multiplying by the next matrix in the sequence,
# and that gives us our `partial sums' of the infinite function
# composition.
#
# We can also extract digits from the left of this as they are
# generated. Suppose we currently have a function f(x) which has
# the property that f(3) and f(4) have the same integer part i.
# Then i is the first digit of pi. So we could continue to shovel
# further matrices into the right of f and wait until f(3) and f(4)
# agree in the first decimal place as well as the integer part; but
# a better approach which requires no fractional arithmetic is, at
# this point, to stop looking at f and start looking at (f-i)*10,
# so that the next digit of pi will appear when (f(3)-i)*10 and
# (f(4)-i)*10 agree in the integer part.
#
# The function we left-composed with f there is x |-> 10*x-i, which
# is _also_ a Mobius transformation; so we can apply it once to our
# gradually evolving matrix that represents f, this time by _pre_-
# multiplying it into that matrix, and forget about it thereafter.
#
# Hence, our finished algorithm goes like this:
#
#   Maintain a 2x2 integer matrix (p, q, r, s), which starts out as
#   (1, 0, 0, 1).
#
#   For ever:
#       If the integer parts of (3p+q)/(3r+s) and (4p+q)/(4r+s) are equal, then 
#       they are both equal to the next digit of pi.
#           So output that digit d, and pre-multiply our matrix by
#           (10,-10*d,0,1).
#       Otherwise, we don't currently have enough precision to know the
#       next digit of pi.
#           So find the next matrix in the sequence given above, and
#           post-multiply that into the matrix we're maintaining.
#
# It's that simple!
#
# To adapt the algorithm to e, we use the obvious series for e
# obtained by expanding the Taylor series for exp(1):
#
#  e = 1 + 1/1! + 1/2! + 1/3! + 1/4! + ...
#    = 1 + 1/1 * (1 + 1/2 * (1 + 1/3 * (1 + 1/4 * *...)))
#
# which gives us the infinite function composition
#
#   (x |-> 1+x) o (x |-> 1+x/2) o (x |-> 1+x/3) o ...
#
# and hence the sequence of matrices
#
#   ( 1 1 ) ( 1 2 ) ( 1 3 ) ... ( 1 k ) ...
#   ( 0 1 ) ( 0 2 ) ( 0 3 )     ( 0 k )
#
# We must also adjust the limits 3 and 4. We picked the interval
# [3,4] for the pi expansion because we know that each of the
# functions in the infinite composition for pi maps [3,4] to a
# subinterval of itself, meaning that the images of the ends of
# that interval always bracket pi. However, this is not true for
# the above series of functions for e. In fact, there is _no_
# interval which all of the above functions map to a subinterval of
# itself, because the first function (x |-> 1+x) translates every
# interval upwards!
#
# Fortunately, all the functions _other_ than the first one have
# the property that they map [0,2] to a subinterval of itself. So
# once the image of [0,2] under a `partial sum' of at least two
# terms of this function series contains e, its image under
# subsequent partial sums will be a subinterval of that and hence
# will always contain e as well. So we need only check that no
# spurious `digits' are output before that happens, and then we're
# safe. And this is OK, because the identity matrix maps [0,2] to
# [0,2] which doesn't have a constant integer part, the first
# function maps it to [1,3] which doesn't either, and the first two
# functions map it to [2,3] which contains e and is late enough in
# the series to be safe.
#
# At the output end, we can obviously modify the algorithm to yield
# digits in any base we like instead of ten. Mathematically, our
# means of generating digits involves repeatedly subtracting off
# the integer part and then multiplying by ten; so, clearly, if we
# subtract off the integer part and then multiply by some other
# number, we'll get the digits of pi or e in that base instead. (Of
# course, the _first_ digit extracted will be the original integer
# part of the number, which won't be within the right range if the
# target base is 2 or 3. But that's easy to correct for, and all
# subsequent digits will be in range.)
#
# Another neat trick is that this algorithm can be trivially
# switched to generate the terms of the continued fraction for pi
# (or e, although e's continued fraction is boringly regular). A
# theoretical algorithm for finding the terms of the continued
# fraction for a number, given exact real arithmetic, is to
# subtract off the integer part, take the reciprocal of the result,
# and repeat. And just like `subtract n and multiply by ten', the
# operation `subtract n and take the reciprocal' can also be
# written as a Mobius transformation - so by simply premultiplying
# by a different matrix every time we output a `digit', we can
# generate the continued fraction for pi to arbitrary length using
# exactly the same core algorithm.
#
# (Doing this does mean we have to watch out for division by zero
# when computing the candidate digit (3p+q)/(3r+s), for all values
# of 3. Division by zero indicates that the image of our starting
# interval under our current function is an interval with one end
# at infinity, so when this occurs we simply decide that a digit
# cannot safely be extracted at this time, and keep going. The
# interval will shrink to finite size as soon as another matrix is
# folded in.)

def consume_base(state, digit):
    global start

    if start == 0:
	# Special case: the first digit generated is the integer
	# part of the target number. Therefore, we want to write a
	# decimal point after it, and also we need special handling
	# if `base' is too small.
	b = 1
	x = digit
	while digit >= b*base:
	    b = b * base
	while b > 0:
	    d = x / b
	    x = x % b
	    sys.stdout.write("%c" % (48 + d + 39*(d>9)))
	    b = b / base
	start = 1
    else:
	if start == 1:
	    # Write the decimal point, once we know there's a digit
	    # following it.
	    sys.stdout.write(".")
	    start = 2
	# Once we're going, we just write out each digit in the
	# requested base.
	sys.stdout.write("%c" % (48 + digit + 39*(digit>9)))
    sys.stdout.flush()

    return mmul((base, -base*digit, 0, 1), state)

def consume_confrac(state, digit):
    global start

    sys.stdout.write("%d\n" % digit)
    sys.stdout.flush()

    return mmul((0, 1, 1, -digit), state)

def mtrans(m, x):
    # Interpret a 2x2 matrix as a Mobius transformation.
    try:
	if x == None:
	    if m[0]:
		return m[0] / m[2]
	    elif m[2]:
		return 0
	    else:
		return m[1] / m[3]
	else:
	    return (m[0]*x+m[1]) / (m[2]*x+m[3])
    except ZeroDivisionError, e:
	return None

def mmul(m1, m2):
    # 2x2 matrix multiplication.
    return (m1[0]*m2[0] + m1[1]*m2[2], m1[0]*m2[1] + m1[1]*m2[3],
    m1[2]*m2[0] + m1[3]*m2[2], m1[2]*m2[1] + m1[3]*m2[3])

def pi_matrix(k):
    return (k, 4*k+2, 0, 2*k+1)

def e_matrix(k):
    return (1, k, 0, k)

def phi_matrix(k):
    return (1, 1, 1, 0)

def cfrac_matrix(k):
    s = sys.stdin.readline()
    if s == "":
	# The continued fraction terminated. We therefore return
	# the matrix that always returns infinity, so that the last
	# term (k+1/x) becomes (k+1/infinity) == k.
	return (0, 1, 0, 0)
    i = long(s)
    return (i, 1, 1, 0)

class base_matrix:
    def __init__(self, base):
	self.base = base
	self.start = 1
    def val(self, c):
	if c >= '0' and c <= '9':
	    return ord(c) - ord('0')
	elif c >= 'A' and c <= 'Z':
	    return ord(c) - ord('A') + 10
	elif c >= 'a' and c <= 'z':
	    return ord(c) - ord('a') + 10
	else:
	    return None
    def __call__(self, k):
	if self.start:
	    self.start = 0
	    i = 0
	    while 1:
		c = sys.stdin.read(1)
		if c == "" or c == ".":
		    break
		else:
		    v = self.val(c)
		    if v != None:
			i = i * self.base + self.val(c)
	else:
	    while 1:
		c = sys.stdin.read(1)
		if c == "":
		    # The expansion terminated, so return a matrix
		    # which gives a constant.
		    return (0, 0, 0, 1)
		i = self.val(c)
		if i != None:
		    break
	return (1, i*self.base, 0, self.base)

def isqrt(n):
    d = long(n)
    a = 0L
    # b must start off as a power of 4 at least as large as n
    ndigits = len(hex(long(n)))
    b = 1L << (ndigits*4)
    while 1:
        a = a >> 1
        di = 2*a + b
        if di <= d:
            d = d - di
            a = a + b
        b = b >> 2
	if b == 0: break
    return a

class root_matrix:
    # To compute the square root of an arbitrary non-square
    # integer, we use the following magic algorithm to find its
    # continued fraction. The continued fraction is periodic, so
    # this costs us a bounded amount of work, after which we can
    # cheerfully return the same sequence of matrices over and over
    # again.
    #
    # The algorithm, taken from Wikipedia
    # (http://en.wikipedia.org/wiki/Methods_of_computing_square_roots
    # as of 2007-04-28), is as follows:
    #
    #  - Let m <- 0, d <- 1, and a <- floor(sqrt(n)).
    #  - Repeatedly:
    #     + output a as a continued fraction term.
    #     + let m <- d*a-m.
    # 	  + let d <- (n-m^2)/d. (This division always yields an
    # 	    exact integer; see below.)
    # 	  + let a <- floor((sqrt(n)-m)/d). (We can safely replace
    # 	    sqrt(n) with floor(sqrt(n)) here without affecting the
    # 	    result.)
    # 	  + check if (m,d,a) repeats a set of values which it
    # 	    previously had at this point. If so, we need not
    # 	    continue calculating.
    #
    # Proof by induction that every value of d is an integer:
    #  - Base case: d_0 is an integer, because it's defined to be
    # 	 1.
    #  - Suppose d_i and all previous d_j are integers.
    # 	  + d_i was computed as (n-m_i^2)/d_{i-1}, and hence if
    # 	    it's an integer then d_{i-1} is a factor of (n-m_i^2),
    # 	    and so d_i is _also_ a factor of that.
    # 	  + Now m_{i+1} = d_i a_i - m_i. So n - m_{i+1}^2 is equal
    # 	    to n - (m_i - d_i a_i)^2 = n - m_i^2 + 2 d_i a_i m_i - d_i^2 a_i^2.
    # 	  + And d_i divides n - m_i^2 (by construction), and
    # 	    divides any multiple of d_i (obviously), so it divides
    # 	    that. Hence d_{i+1} will be an integer too. []

    def __init__(self, radicand):
	self.radicand = radicand
	self.a0 = isqrt(radicand)
	self.list = [(0, 1, self.a0)]
	self.resume = None
	self.listpos = 0

	# Special case of an actually square number.
	if self.a0 * self.a0 == radicand:
	    self.list.append((None, None, None))
	    self.resume = 1
    def __call__(self, k):
	if self.listpos >= len(self.list) and self.resume == None:
	    # Compute the next triple.
	    m, d, a = self.list[-1]
	    m = d * a - m
	    d = (self.radicand - m*m) / d
	    a = (self.a0 + m) / d
	    t = (m, d, a)
	    try:
		self.resume = self.list.index(t)
	    except ValueError, e:
		self.resume = None
		self.list.append(t)
	if self.listpos >= len(self.list):
	    assert self.resume != None
	    self.listpos = self.resume
	assert self.listpos < len(self.list)
	a = self.list[self.listpos][2]
	self.listpos = self.listpos + 1
	if a == None:
	    return (0, 1, 0, 0) # fraction terminated
	else:
	    return (a, 1, 1, 0)

class frac_matrix:
    def __init__(self, n, d):
	self.n = n
	self.d = d
    def __call__(self, k):
	return (0, self.n, 0, self.d)

bot, top, matrix = None, None, None
base, consume = 10, consume_base
earlyterm = 1
digitlimit = None

args = sys.argv[1:]
while len(args) > 0:
    arg = args[0]
    args = args[1:]
    if arg == "-c":
	base, consume = None, consume_confrac
    elif arg[0:2] == "-b":
	val = arg[2:]
	if val == "":
	    if len(args) > 0:
		val = args[0]
		args = args[1:]
	    else:
		sys.stderr.write("option '-b' requires an argument\n")
		sys.exit(1)
	try:
	    base = long(val)
	except ValueError, e:
	    base = 0
	if base < 2:
	    sys.stderr.write("base '%s' is not an integer greater than 1\n" \
	    % val)
	    sys.exit(1)
	consume = consume_base
    elif arg[0:2] == "-d":
	val = arg[2:]
	if val == "":
	    if len(args) > 0:
		val = args[0]
		args = args[1:]
	    else:
		sys.stderr.write("option '-d' requires an argument\n")
		sys.exit(1)
        digitlimit = long(val)
    elif arg == "-n":
	earlyterm = 0
    elif arg == "pi":
	bot, top, matrix = 3, 4, pi_matrix
    elif arg == "e":
	bot, top, matrix = 0, 2, e_matrix
    elif arg == "phi":
	bot, top, matrix = 1, 2, phi_matrix
    elif arg == "root":
	if len(args) > 0:
	    radicand = args[0]
	    args = args[1:]
	else:
	    sys.stderr.write("input type 'root' requires an argument\n")
	    sys.exit(1)
	radicand = long(radicand)
	bot, top, matrix = 0, None, root_matrix(radicand)
    elif arg == "frac":
	if len(args) >= 2:
	    numerator = args[0]
	    denominator = args[1]
	    args = args[2:]
	else:
	    sys.stderr.write("input type 'frac' requires two arguments\n")
	    sys.exit(1)
	numerator = long(numerator)
	denominator = long(denominator)
	bot, top, matrix = numerator/denominator, \
	(numerator+denominator-1)/denominator, \
	frac_matrix(numerator, denominator)
    elif arg == "cfrac":
	# Read a list of continued fraction terms on standard
	# input, and spigot them into some ordinary number base on
	# output.
	
	# `None' here represents infinity; the general continued
	# fraction transformation x -> k+1/x always maps
	# [0,infinity] to a subinterval of itself.
	bot, top, matrix = 0, None, cfrac_matrix
    elif arg == "base":
	# Read a number in a specified integer base on standard
	# input, and spigot it into a different base or a continued
	# fraction.
	if len(args) > 0:
	    ibase = args[0]
	    args = args[1:]
	else:
	    sys.stderr.write("input type 'base' requires an argument\n")
	    sys.exit(1)
	ibase = long(ibase)
	bot, top, matrix = 0, ibase, base_matrix(ibase)
    else:
	sys.stderr.write("unrecognised argument '%s'\n" % arg)
	sys.exit(1)

if matrix == None:
    sys.stderr.write("usage: %s [ -c | -b <base> ] [ -n ] [ -d <digits> ] <number>\n" % sys.argv[0])
    sys.stderr.write("where: -b <base>        output in base <base> instead of 10\n")
    sys.stderr.write("       -c               output continued fraction terms, one per line\n")
    sys.stderr.write("       -n               continue writing digits even if fraction terminates\n")
    sys.stderr.write("       -d <digits>      terminate after writing that many digits\n")
    sys.stderr.write("and <number> is:\n")
    sys.stderr.write("       pi | e | phi     mathematical constants\n")
    sys.stderr.write("       root <n>         the square root of any positive integer <n>\n")
    sys.stderr.write("       cfrac            a continued fraction read from standard input\n")
    sys.stderr.write("       base <n>         a base-<n> number read from standard input\n")
    sys.exit(len(sys.argv) > 1)

state = (1, 0, 0, 1)
k = 1
start = 0

try:
    while digitlimit == None or digitlimit > 0:
	digit = mtrans(state, bot)
	dcheck = mtrans(state, top)
	if digit == dcheck:
	    if digit == None:
		# This case arises when we have been asked to generate the
		# continued fraction for a rational. Simply stop.
		assert consume == consume_confrac
		sys.exit(0)
	    if earlyterm and state[0] == 0 and state[1] == 0 and state[2] == 0:
		# This case arises when we have been asked to generate
		# a base-n fraction which terminates. Stop again.
		assert consume == consume_base
		print # a trailing newline is generally considered polite
		sys.exit(0)
	    # Otherwise, generate a digit.
	    state = consume(state, digit)
            if digitlimit != None:
                digitlimit = digitlimit - 1
	else:
	    # Fold in another matrix.
	    state = mmul(state, matrix(k))
	    k = k + 1
    # If we get here in base-conversion mode (probably because we
    # hit our digit limit), output a trailing newline.
    if consume == consume_base:
	print
except IOError, e:
    # This typically means `broken pipe', i.e. we've been piped
    # into head or some such. Don't panic; just calmly shut down.
    if e.errno == errno.EPIPE:
	sys.exit(0)
    else:
	raise e
except KeyboardInterrupt, e:
    sys.exit(0)
