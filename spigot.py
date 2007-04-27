#!/usr/bin/env python

import sys

# Spigot algorithm to produce the digits of pi, without having to
# commit in advance to the number of digits desired (so you can
# just keep going until memory or CPU time runs out). Implemented
# from the description given in
#
# http://web.comlab.ox.ac.uk/oucl/work/jeremy.gibbons/publications/spigot.pdf
#
# and enhanced to generate e as well by me.

# Usage:
#   spigot.py                    # generates pi in base 10
#   spigot.py 7                  # generates pi in another base, e.g. 7
#   spigot.py e                  # generates e in base 10
#   spigot.py e 7                # generates e in another base, e.g. 7

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
# once the image of [0,2] under one `partial sum' of this function
# series contains e, its image under subsequent partial sums will
# be a subinterval of that and hence will always contain e as well.
# So we need only check that no spurious `digits' are output before
# that happens, and then we're safe. And this is OK, because under
# the identity matrix [0,2] maps to [0,2] which doesn't have a
# constant integer part, under the first function it maps to [1,3]
# which doesn't either, and under the first two functions it maps
# to [2,3] which contains e.

def writedigit(digit):
    global start
    if start:
	# Special case: the first digit generated is the
	# integer part, and hence is always 3. Therefore, we
	# want to write a decimal point after it, and also we
	# need special handling if `base' is too small.
	if digit >= base:
	    assert digit < base*base
	    sys.stdout.write("%d%d" % (digit / base, digit % base))
	else:
	    sys.stdout.write("%d" % digit) # can't go above 9!
	sys.stdout.write(".")
	start = 0
    else:
	# Once we're going, we just write out each digit in the
	# requested base.
	sys.stdout.write("%c" % (48 + digit + 39*(digit>9)))
    sys.stdout.flush()

def mtrans(m, x):
    # Interpret a 2x2 matrix as a Mobius transformation.
    return (m[0]*x+m[1]) / (m[2]*x+m[3])

def mmul(m1, m2):
    # 2x2 matrix multiplication.
    return (m1[0]*m2[0] + m1[1]*m2[2], m1[0]*m2[1] + m1[1]*m2[3],
    m1[2]*m2[0] + m1[3]*m2[2], m1[2]*m2[1] + m1[3]*m2[3])

def pi_matrix(k):
    return (k, 4*k+2, 0, 2*k+1)

def e_matrix(k):
    return (1, k, 0, k)

bot = 3
top = 4
matrix = pi_matrix

base = 10
for arg in sys.argv[1:]:
    if arg == "e":
	bot = 0
	top = 2
	matrix = e_matrix
    else:
	base = int(arg)
	assert base >= 2

state = (1, 0, 0, 1)
k = 1
start = 1

while 1:
    digit = mtrans(state, bot)
    if digit == mtrans(state, top):
	# Generate a digit.
	writedigit(digit)
	state = mmul((base, -base*digit, 0, 1), state)
    else:
	# Fold in another matrix.
	state = mmul(state, matrix(k))
	k = k + 1
