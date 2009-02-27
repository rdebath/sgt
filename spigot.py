#!/usr/bin/env python

import sys
import string
import re
import errno
import os

# TODO:
#  - Rationalise command line syntax?
#  - Update comments significantly. We do quite a lot of new stuff!
#  - See if we can support any more algorithms. Gosper provides a
#    square root, for example.

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
#   spigot.py -C pi              # generates convergents for pi
#   spigot.py e                  # generates e
#   spigot.py phi                # generates phi
#   spigot.py root 3             # generates the square root of 3
#   spigot.py frac 3 15          # generates the rational 3/15
#
# You can also feed a stream of input to the program and it will
# convert between continued fraction terms and an arbitrary number
# base, or between number bases:
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
# 	If the integer parts of (3p+q)/(3r+s) and (4p+q)/(4r+s) are
# 	equal, then they are both equal to the next digit of pi.
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

class output_base:
    def __init__(self, base):
	self.base = base
	self.state = 0
    def consume(self, matrix, digit):
	# Update a matrix state to reflect the consumption of a
	# digit.
	if digit < 0:
	    # Special case that can occur on the very first call if
	    # our number is negative. In this situation we don't
	    # want to map x |-> base * (x - digit) as usual,
	    # because that would lead to us producing a fraction
	    # going _upwards_ from the number instead of downwards
	    # (think of it as (-1).12345 instead of -0.87655). So
	    # instead we map x |-> base * (digit+1 - x).
	    return mmul((-self.base, self.base*(digit+1), 0, 1), matrix)
	else:
	    return mmul((self.base, -self.base*digit, 0, 1), matrix)
    def output(self, digit):
	if self.state == 0:
	    # Special case: the first digit generated is the
	    # integer part of the target number. Therefore, we want
	    # to write a decimal point after it, and also we need
	    # special handling if `base' is too small.
	    #
	    # Also, this digit may be negative, in which case we
	    # print a minus sign and _subtract 1_ from it.
	    if digit < 0:
		digit = -1 - digit
		sys.stdout.write("-")
	    b = 1
	    x = digit
	    while digit >= b*self.base:
		b = b * self.base
	    while b > 0:
		d = x / b
		x = x % b
		sys.stdout.write("%c" % (48 + d + 39*(d>9)))
		b = b / self.base
	    self.state = 1
	else:
	    if self.state == 1:
		# Write the decimal point, once we know there's a digit
		# following it.
		sys.stdout.write(".")
		self.state = 2
	    # Once we're going, we just write out each digit in the
	    # requested base.
	    sys.stdout.write("%c" % (48 + digit + 39*(digit>9)))
	sys.stdout.flush()
    def run(self, spig, digitlimit, earlyterm):
	while digitlimit == None or digitlimit > 0:
	    digit = spig.gendigit(self, earlyterm)
	    assert digit != (-1,)
	    if digit == (-2,):
		print # a trailing newline is generally considered polite
		return
	    else:
		self.output(digit)
		if digitlimit != None:
		    digitlimit = digitlimit - 1
	# If we hit our digit limit, output a trailing newline.
	print

class output_confrac:
    def __init__(self, write_convergents = 0):
	self.write_convergents = write_convergents
	self.convergent_matrix = [0, 1, 1, 0]
    def consume(self, matrix, digit):
	# Update a matrix state to reflect the consumption of a
	# digit.
	return mmul((0, 1, 1, -digit), matrix)
    def output(self, digit):
	if self.write_convergents:
	    a = self.convergent_matrix
	    a = [a[1], a[1]*digit+a[0], a[3], a[3]*digit+a[2]]
	    self.convergent_matrix = a
	    sys.stdout.write("%d/%d\n" % (a[1], a[3]))
	else:
	    sys.stdout.write("%d\n" % digit)
	sys.stdout.flush()
    def run(self, spig, digitlimit, earlyterm):
	while digitlimit == None or digitlimit > 0:
	    digit = spig.gendigit(self, earlyterm)
	    assert digit != (-2,)
	    if digit == (-1,):
		return
	    else:
		self.output(digit)

class output_approx:
    def __init__(self, fd):
	self.file = os.fdopen(fd, "r")
	self.state = 0
    def consume(self, matrix, digit):
	# We never consume anything.
	return matrix
    def output(self, digit):
	sys.stdout.write("%d\n" % digit)
	sys.stdout.flush()
    def run(self, spig, digitlimit, earlyterm):
	while digitlimit == None or digitlimit > 0:
	    denominator = self.file.readline()
	    if denominator == "":
		return
	    denominator = long(denominator)
	    outmatrix = (denominator, 0, 0, 1)
	    digit = spig.gendigit(self, earlyterm, outmatrix)
	    assert digit != (-1,) and digit != (-2,)
	    self.output(digit)
	    if digitlimit != None:
		digitlimit = digitlimit - 1

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

class gosper_matrix:
    def __init__(self, numbers, spig1, spig2):
        self.numbers = numbers
        self.x = spig1
        self.y = spig2
	self.xdone = self.ydone = 0
    def consume(self, matrix, digit):
	# Update a matrix state to reflect the consumption of a
	# digit.
	return mmul((0, 1, 1, -digit), matrix)
    def getx(self):
        a, b, c, d, e, f, g, h = self.numbers
	p = self.x.gendigit(self, 0)
	q = 1
        if p == (-1,):
            self.numbers = (0, 0, a, b, 0, 0, e, f)
	    self.xdone = 1
        else:
            self.numbers = (p*a+c, p*b+d, q*a, q*b, p*e+g, p*f+h, q*e, q*f)
    def gety(self):
        a, b, c, d, e, f, g, h = self.numbers
	r = self.y.gendigit(self, 0)
	s = 1
        if r == (-1,):
            self.numbers = (0, a, 0, c, 0, e, 0, g)
	    self.ydone = 1
        else:
            self.numbers = (r*a+b, s*a, r*c+d, s*c, r*e+f, s*e, r*g+h, s*g)
    def output_term(self, t, u):
        a, b, c, d, e, f, g, h = self.numbers
        self.numbers = (u*e, u*f, u*g, u*h, a-t*e, b-t*f, c-t*g, d-t*h)
    def dostep(self):
        a, b, c, d, e, f, g, h = self.numbers
	# Termination check: if we have no remaining dependency on
	# x and y, and the remaining ratio d/h has become infinite,
	# then our output continued fraction has terminated.
	if a==0 and b==0 and c==0 and e==0 and f==0 and g==0 and h==0:
	    return -1, -1
        # The four extreme positions of z, as x and y vary between
        # 0 and infinity, are a/e, b/f, c/g and d/h. First we must
        # check whether the denominator changes sign or is zero.
        if (not self.xdone and not self.ydone and e<=0) or \
	(not self.xdone and f<=0) or (not self.ydone and g<=0) or h<=0:
            # I couldn't immediately work out the rule for which
            # fraction we should fetch a term from in this
            # situation, so I gave up and just did both :-)
            if not self.xdone: self.getx()
	    if not self.ydone: self.gety()
            return None
        # Now find the integer parts of the four ratios, and see
        # which ones aren't equal to decide whether to get another
        # term.
	term = None
	if not self.xdone and not self.ydone:
	    ra, rb, rc, rd = a/e, b/f, c/g, d/h
	    if ra == rb and rb == rc and rc == rd:
		term = ra
	elif not self.xdone:
	    rb, rd = b/f, d/h
	    if rb == rd:
		term = rb
	elif not self.ydone:
	    rc, rd = c/g, d/h
	    if rc == rd:
		term = rc
	else:
	    term = rd = d/h
	if term != None:
            # Output a term.
            self.output_term(term, 1)
            return term, 1
        # Otherwise, decide which of x and y to fetch another term
        # from.
        if not self.xdone and (rb != rd or (not self.ydone and ra != rc)):
            self.getx()
        if not self.ydone and (rc != rd or (not self.xdone and ra != rb)):
            self.gety()
    def __call__(self, k):
        while 1:
            term = self.dostep()
            if term != None:
                break
	if term == (-1, -1):
	    return (0, 1, 0, 0) # fraction terminated
	else:
	    assert term[1] == 1
	    return (term[0], 1, 1, 0)

class primitive_log_matrix:
    # We compute logs by summing the series for log(1+x). x will be
    # rational, in the general case (say a/b), and we will often
    # want to multiply our output log by some integer s.
    #
    #                           a   2 a^2   3 a^3
    # So s*log(1 + a/b) = s * ( - - ----- + ----- - ...
    #                           b    b^2     b^3
    #
    #                         a        a       2a
    #                   = s * - ( 1 - -- ( 1 - -- ( ... ) ) )
    #                         b       2b       3b
    #
    # so our matrices go
    #
    #   ( s*a 0 ) ( -a 2b ) ( -2a 3b ) ...
    #   (   0 b ) (  0 2b ) (   0 3b )

    def __init__(self, s, a, b):
	self.a = a
	self.b = b
	self.s = s
    def __call__(self, k):
	if k == 1:
	    return (self.s * self.a, 0, 0, self.b)
	else:
	    return (-(k-1)*self.a, k*self.b, 0, k*self.b)
def log_spigot(n, d = 1):
    # This is the real function that computes a log. We begin by
    # doing argument reduction: scale our input by a power of two
    # so that it's in the range [1/2,2].
    #
    # Unlike any other function in this family, we return a spigot
    # object rather than a raw matrix, because we need to vary our
    # interval bounds depending on whether we're doing nontrivial
    # argument reduction or not.
    def approxlog2(x):
	s = hex(x)
	if s[:2] == "0x": s = s[2:]
	if s[:1] == "0": s = s[1:]
	if s[-1:] == "L": s = s[:-1]
	return 4 * len(s)
    if n > d:
	k = approxlog2(n/d)
	assert n <= d << k
	while n <= d << k:
	    k = k - 1
	d = d << k
    else:
	k = approxlog2(d/n)
	assert d <= n << k
	while d <= n << k:
	    k = k - 1
	n = n << k
	k = -k
    # Now the value we want is log(n/d) + k log 2.
    #
    # If n/d > 1, flip round to compute -log(d/n), which will
    # converge a bit faster.
    if n > d:
	plm = primitive_log_matrix(-1, d-n, n) # log(1 + (d-n)/n) = log(d/n)
    else:
	plm = primitive_log_matrix(1, n-d, d) # log(1 + (n-d)/d) = log(n/d)
    pls = spigot(0, 2, plm)
    # And now scale back up, if necessary.
    if k != 0:
	scm = primitive_log_matrix(-k, -1, 2) # -k * log(1/2)
	scs = spigot(0, 2, scm)
	retm = gosper_matrix((0,1,1,0,0,0,0,1), pls, scs)
	return spigot(0, None, retm)
    else:
	return pls

class recip_matrix:
    def __init__(self, spig):
	self.spig = spig
	self.state = 0
    def consume(self, matrix, digit):
	# Update a matrix state to reflect the consumption of a
	# digit.
	return mmul((0, 1, 1, -digit), matrix)
    def __call__(self, k):
	if self.state == 0:
	    self.state = 1
	    return (0, 1, 1, 0)
	else:
	    d1 = self.spig.gendigit(self, 0)
	    if d1 == (-1,):
		return (0, 1, 0, 0) # fraction terminated
	    else:
		return (d1, 1, 1, 0)

class spigot:
    def __init__(self, bot, top, matrix):
	self.bot = bot
	self.top = top
	self.matrix = matrix
	self.state = (1, 0, 0, 1)
	self.k = 1
    def gendigit(self, consume, earlyterm=0, outmatrix=None):
	while 1:
	    if outmatrix:
		thisstate = mmul(outmatrix, self.state)
	    else:
		thisstate = self.state
	    digit = mtrans(thisstate, self.bot)
	    dcheck = mtrans(thisstate, self.top)
	    if digit == dcheck:
		if digit == None:
		    # This case arises when we have been asked to generate the
		    # continued fraction for a rational.
		    return (-1,)
		if earlyterm and self.state[0] == 0 \
		and self.state[1] == 0 and self.state[2] == 0:
		    # This case arises when we have been asked to generate
		    # a base-n fraction which terminates.
		    return (-2,)
		# Otherwise, generate a digit.
		self.state = consume.consume(self.state, digit)
		return digit
	    else:
		# Fold in another matrix.
		self.state = mmul(self.state, self.matrix(self.k))
		self.k = self.k + 1

def get_spig(arg, args):
    if arg == "pi":
	return spigot(3, 4, pi_matrix)
    elif arg == "e":
	return spigot(0, 2, e_matrix)
    elif arg == "phi":
	return spigot(1, 2, phi_matrix)
    elif arg == "root":
	if len(args) > 0:
	    radicand = args[0]
	    del args[0]
	else:
	    sys.stderr.write("input type 'root' requires an argument\n")
	    sys.exit(1)
	radicand = long(radicand)
	return spigot(0, None, root_matrix(radicand))
    elif arg == "log":
	if len(args) > 0:
	    logarg = args[0]
	    del args[0]
	else:
	    sys.stderr.write("input type 'log' requires an argument\n")
	    sys.exit(1)
	logarg = long(logarg)
	return log_spigot(logarg)
    elif arg == "frac":
	if len(args) >= 2:
	    numerator = args[0]
	    denominator = args[1]
	    del args[0:2]
	else:
	    sys.stderr.write("input type 'frac' requires two arguments\n")
	    sys.exit(1)
	numerator = long(numerator)
	denominator = long(denominator)
	return spigot(numerator/denominator, \
	(numerator+denominator-1)/denominator, \
	frac_matrix(numerator, denominator))
    elif arg == "cfrac":
	# Read a list of continued fraction terms on standard
	# input, and spigot them into some ordinary number base on
	# output.
	
	# `None' here represents infinity; the general continued
	# fraction transformation x -> k+1/x always maps
	# [0,infinity] to a subinterval of itself.
	return spigot(0, None, cfrac_matrix)
    elif arg == "base":
	# Read a number in a specified integer base on standard
	# input, and spigot it into a different base or a continued
	# fraction.
	if len(args) > 0:
	    ibase = args[0]
	    del args[0]
	else:
	    sys.stderr.write("input type 'base' requires an argument\n")
	    sys.exit(1)
	ibase = long(ibase)
	return spigot(0, ibase, base_matrix(ibase))
    elif arg == "add" or arg == "sub" or arg == "mul" or arg == "div":
	# Read two other spigot definitions, and produce a spigot
	# giving their sum.
	if len(args) < 1:
	    sys.stderr.write("input type 'add' requires operands\n")
	    sys.exit(1)
	x = args[0]
	del args[0]
	spig1 = get_spig(x, args)
	if len(args) < 1:
	    sys.stderr.write("input type 'add' requires two operands\n")
	    sys.exit(1)
	x = args[0]
	del args[0]
	spig2 = get_spig(x, args)
	if arg == "add":
	    numbers = (0,1,1,0,0,0,0,1)
	elif arg == "sub":
	    numbers = (0,1,-1,0,0,0,0,1)
	elif arg == "mul":
	    numbers = (1,0,0,0,0,0,0,1)
	elif arg == "div":
	    numbers = (0,1,0,0,0,0,1,0)
	return spigot(0, None, gosper_matrix(numbers, spig1, spig2))
    elif arg == "reciprocal":
	# Read another spigot definition, and produce a spigot
	# giving its reciprocal.
	if len(args) < 1:
	    sys.stderr.write("input type 'reciprocal' requires operands\n")
	    sys.exit(1)
	x = args[0]
	del args[0]
	spig1 = get_spig(x, args)
	return spigot(0, None, recip_matrix(spig1))
    elif arg == "ieee" or arg == "ieeef" or arg == "ieeed":
        # Interpret the input as an IEEE floating-point bit pattern
        # expressed in hex.
	if len(args) < 1:
	    sys.stderr.write("input type '" + arg + "' requires an operand\n")
	    sys.exit(1)
	x = args[0]
	del args[0]
        if x[:2] == "0x" or x[:2] == "0X":
            del x[:2]
        x = string.replace(x, ":", "")
        x = string.replace(x, "_", "")
        x = string.replace(x, ".", "")
        x = string.replace(x, ",", "")
        x = string.replace(x, "-", "")
        if arg == "ieee":
            if len(x) != 8 and len(x) != 16:
                sys.stderr.write("input type 'ieee' requires a hex string 8 or 16 digits long\n");
                sys.exit(1)
            double = (len(x) == 16)
        elif arg == "ieeef":
            x = x + "0"*8
            x = x[:8]
            double = 0
        elif arg == "ieeed":
            x = x + "0"*16
            x = x[:16]
            double = 1
        value = long(x, 16)
        if double:
            sign = 0x8000000000000000L
            expbias = 0x3FF
            expshift = 52
            manttop = 0x0010000000000000L
        else:
            sign = 0x80000000L
            expbias = 0x7F
            expshift = 23
            manttop = 0x00800000L
        sign = sign & value
        value = value & ~sign
        exp = value >> expshift
        mant = value & (manttop - 1)
        if exp > 0:
            mant = mant + manttop
        else:
            exp = exp + 1
        exp = exp - expbias - expshift
        numerator = mant
        if exp > 0:
            denominator = 1
            numerator = numerator * 2L**exp
        else:
            denominator = 2L**-exp
	return spigot(numerator/denominator, \
	(numerator+denominator-1)/denominator, \
	frac_matrix(numerator, denominator))
    else:
        # Interpret the input as a C-style decimal or hex
        # floating-point literal.
        dfloatre = re.compile(r'^(-?)([0-9]*)(?:\.([0-9]*))?(?:[eE]([-\+]?[0-9]+))?$');
        hfloatre = re.compile(r'^(-?)0[xX]([0-9a-fA-F]*)(?:\.([0-9a-fA-F]*))?(?:[pP]([-\+]?[0-9]+))?$');
        dm = dfloatre.match(arg)
        hm = hfloatre.match(arg)
        if dm == None and hm == None:
	    sys.stderr.write("unrecognised argument '%s'\n" % arg)
	    sys.exit(1)
        if dm != None:
            integer = dm.group(1) + dm.group(2)
            exponent = 0
            if dm.group(3) != None:
                integer = integer + dm.group(3)
                exponent = exponent - len(dm.group(3))
            if dm.group(4) != None:
                exponent = exponent + int(dm.group(4))
            numerator = long(integer)
            base = 10L
        elif hm != None:
            integer = hm.group(1) + hm.group(2)
            exponent = 0
            if hm.group(3) != None:
                integer = integer + hm.group(3)
                exponent = exponent - 4*len(hm.group(3))
            if hm.group(4) != None:
                exponent = exponent + int(hm.group(4))
            numerator = long(integer, 16)
            base = 2L
        if exponent > 0:
            denominator = 1
            numerator = numerator * base**exponent
        else:
            denominator = base**-exponent
	return spigot(numerator/denominator, \
	(numerator+denominator-1)/denominator, \
	frac_matrix(numerator, denominator))

spig = None
output = output_base(10)
earlyterm = 1
digitlimit = None

args = sys.argv[1:]
while len(args) > 0:
    arg = args[0]
    del args[0]
    if arg == "-c":
	output = output_confrac()
    elif arg == "-C":
	output = output_confrac(1)
    elif arg[0:2] == "-b":
	val = arg[2:]
	if val == "":
	    if len(args) > 0:
		val = args[0]
		del args[0]
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
	output = output_base(base)
    elif arg[0:2] == "-d":
	val = arg[2:]
	if val == "":
	    if len(args) > 0:
		val = args[0]
		del args[0]
	    else:
		sys.stderr.write("option '-d' requires an argument\n")
		sys.exit(1)
        digitlimit = long(val)
    elif arg[0:2] == "-a":
	val = arg[2:]
	if val == "":
	    if len(args) > 0:
		val = args[0]
		del args[0]
	    else:
		sys.stderr.write("option '-d' requires an argument\n")
		sys.exit(1)
	output = output_approx(int(val))
    elif arg == "-n":
	earlyterm = 0
    else:
	spig = get_spig(arg, args)
	if spig == None:
	    sys.stderr.write("unrecognised argument '%s'\n" % arg)
	    sys.exit(1)

if spig == None:
    sys.stderr.write("usage: %s [ -c | -b <base> ] [ -n ] [ -d <digits> ] <number>\n" % sys.argv[0])
    sys.stderr.write("where: -b <base>        output in base <base> instead of 10\n")
    sys.stderr.write("       -c               output continued fraction terms, one per line\n")
    sys.stderr.write("       -n               continue writing digits even if fraction terminates\n")
    sys.stderr.write("       -d <digits>      terminate after writing that many digits\n")
    sys.stderr.write("       -a <fd>          read a stream of denominators from <fd>,\n")
    sys.stderr.write("                          write out numerators of approximations\n")
    sys.stderr.write("and <number> is:\n")
    sys.stderr.write("       pi | e | phi     mathematical constants\n")
    sys.stderr.write("       root <n>         the square root of any positive integer <n>\n")
    sys.stderr.write("       cfrac            a continued fraction read from standard input\n")
    sys.stderr.write("       base <n>         a base-<n> number read from standard input\n")
    sys.exit(len(sys.argv) > 1)

try:
    output.run(spig, digitlimit, earlyterm)
except IOError, e:
    # This typically means `broken pipe', i.e. we've been piped
    # into head or some such. Don't panic; just calmly shut down.
    if e.errno == errno.EPIPE:
	sys.exit(0)
    else:
	raise e
except KeyboardInterrupt, e:
    sys.exit(0)
