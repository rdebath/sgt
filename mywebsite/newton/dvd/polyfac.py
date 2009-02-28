#!/usr/bin/env python

# Factorise an arbitrary complex polynomial numerically, by
# repeatedly using N-R to find a root r and then dividing off the
# (z-r) term.
#
# Note that the output is not sufficient to reconstruct the input
# polynomial uniquely, because it doesn't tell you the leading
# coefficient. (x-a)(x-b) and 2(x-a)(x-b) will both output a and b
# and nothing else.

import random
import math

def factorise(poly):
    # Expects the polynomial as a list of coefficients, from the
    # constant term upwards. Outputs a list of roots.
    roots = []

    while len(poly) > 1:
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
            while 1:
                if iremaining == 0:
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
                    break # if we've reached a stationary point, might as well stop
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
                    break
            # If we've converged to a sensible root, break.
            if abs(p) < 1e-10:
                root = z
                break
            itimes = itimes + 1
        roots.append(root)
        # Divide off (z-root) from the polynomial.
        newpoly = []
        for i in range(len(poly)-1,0,-1):
            newpoly = [poly[i]] + newpoly
            poly[i-1] = poly[i-1] + poly[i] * root
        poly = newpoly
    return roots
