#!/usr/bin/env python

# Factorise an arbitrary complex polynomial numerically, by
# repeatedly using N-R to find a root r and then dividing off the
# (z-r) term.
#
# Expects the coefficients of the polynomial on the command line,
# starting with the constant term and working upward. Outputs a
# list of roots, one per line.
#
# Note that the output is not sufficient to reconstruct the input
# polynomial uniquely, because it doesn't tell you the leading
# coefficient. (x-a)(x-b) and 2(x-a)(x-b) will both output a and b
# and nothing else.

# Since this script necessarily contains code to differentiate a
# polynomial, I also bodge on a -d mode in which the coefficients
# of the differentiated polynomial are simply output, and (while
# I'm at it) a -a mode in which we `integrate' (anti-differentiate)
# the polynomial. There's also a -r mode to reconstitute a
# polynomial _from_ its list of factors.

import sys
import random
import string
import math

def cstr(z):
    # What do you mean `j'? Bloody engineers.
    s = str(z)
    s = string.replace(s, "j", "i")
    s = string.replace(s, "(", "")
    s = string.replace(s, ")", "")
    return s
def atoc(s):
    s = string.replace(s, "i", "j")
    return complex(s)

args = sys.argv[1:]
verbose = 0
mode = 0
while len(args) > 0:
    if args[0] == "-d":
        mode = 1
        args = args[1:]
    elif args[0] == "-a":
        mode = 2
        args = args[1:]
    elif args[0] == "-r":
        mode = 3
        args = args[1:]
    elif args[0] == "-v":
        verbose = verbose + 1
        args = args[1:]
    else:
        break

poly = []
for arg in args:
    arg = string.replace(arg, "i", "j")
    poly.append(atoc(arg))

if mode != 0:
    # Differentiate or `integrate'.
    if mode == 1:
        dpoly = []
        for i in range(1, len(poly)):
            dpoly.append(i * poly[i])
    elif mode == 2:
        dpoly = [0] # arbitrary constant
        for i in range(len(poly)):
            dpoly.append(poly[i] / complex(i+1))
    elif mode == 3:
        dpoly = [1]
        for root in poly:
            # Multiply by (z-root).
            dpoly = [0] + dpoly
            for i in range(len(dpoly)-1):
                dpoly[i] = dpoly[i] - root * dpoly[i+1]
    for z in dpoly:
        print cstr(z)
    sys.exit(0)

roots = []

while len(poly) > 1:
    if verbose >= 1:
        sys.stderr.write("beginning search, %d roots left\n" % (len(poly)-1))
    # Differentiate.
    dpoly = []
    for i in range(1, len(poly)):
        dpoly.append(i * poly[i])
    # Repeatedly try N-R, starting from a random point. (It might
    # fail, in which case we'll have to try again.)
    itimes = 0
    while 1:
        # Pick a starting N-R value.
        z = random.uniform(-1,1) + 1j * random.uniform(-1,1)
        # Sometimes try accelerating convergence in order to find
        # repeated roots.
        isk = int(math.ceil(math.sqrt(itimes+1)))
        k = 1 + max(isk - (isk*isk-itimes), 0)
        # Iterate.
        idone = 0
        iremaining = -1
        if verbose >= 2:
            sys.stderr.write("trying iteration from %s, k=%d\n" % (cstr(z), k))
        while 1:
            if iremaining == 0:
                if verbose >= 2:
                    sys.stderr.write("iteration converged to %s\n" % cstr(z))
                break
            # Compute P(x).
            p = 0
            for i in range(len(poly)-1,-1,-1):
                p = p * z + poly[i]
            # Compute P'(x).
            dp = 0
            for i in range(len(dpoly)-1,-1,-1):
                dp = dp * z + dpoly[i]
            if dp == 0:
                if verbose >= 2:
                    sys.stderr.write("iteration hit stationary point %s\n" % cstr(z))
                break # if we've reached a stationary point, might as well stop
            if verbose >= 3:
                sys.stderr.write("mid-iteration: z=%s k=%d p=%s dp=%s\n" % (cstr(z), k, cstr(p), cstr(dp)))
            # And compute z - P(z)/P'(z).
            zlast = z
            z = z - k * p / dp
            # If we've got within a sensible distance of the root,
            # begin a counter that gives us three more iterations.
            if iremaining < 0 and abs(zlast-z) < 1e-15*max(abs(z),abs(zlast)):
                iremaining = 3
            elif iremaining > 0:
                iremaining = iremaining - 1
            # And if we're really taking too long, give up and try
            # something else.
            idone = idone + 1
            if idone > 1000 and iremaining < 0:
                if verbose >= 2:
                    sys.stderr.write("iteration failed to converge\n")
                break
        # If we've converged to a sensible root, break.
        if abs(p) < 1e-10:
            root = z
            break
        if verbose >= 2:
            sys.stderr.write("did not find a root\n")
        itimes = itimes + 1
    if verbose >= 1:
        sys.stderr.write("found root %s\n" % cstr(root))
    roots.append(root)
    # Divide off (z-root) from the polynomial.
    newpoly = []
    for i in range(len(poly)-1,0,-1):
        newpoly = [poly[i]] + newpoly
        poly[i-1] = poly[i-1] + poly[i] * root
    poly = newpoly

for root in roots:
    print cstr(root)
