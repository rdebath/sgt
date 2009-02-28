#!/usr/bin/env python

# eight queens

import sys
import string

symm = 0

def symmtest(cols):
    if not symm:
        return 1 # everything is OK
    # Determine all symmetries of this solution (rotation plus
    # reflection gives eight possibilities) and see if this one is
    # the lexicographically least of them. This allows us to filter
    # a list down to unique-up-to-symmetry items.
    orig = cols
    n = len(cols)
    slist = []
    for i in range(8):
        if i % 2 == 1:
            # Flip vertically.
            new = ()
            for j in cols:
                new = (j,) + new
        elif i % 4 == 2:
            # Flip horizontally.
            new = ()
            for j in cols:
                new = new + (n-1-j,)
        elif i % 8 == 4:
            # Reflect in leading diagonal.
            new = [0] * n
            for i in range(len(cols)):
                new[cols[i]] = i
            new = tuple(new)
        else:
            # Do nothing (initial iteration).
            new = cols
        slist.append(new)
        cols = new
    slist.sort()
    return orig == slist[0]

def row(k, n, cols, left, right, straight, list):
    this = left | right | straight
    for i in range(n):
        b = 1L << i
        if (this & b) == 0:
            newcols = cols + (i,)
            if k+1 == n:
                if symmtest(newcols):
                    list.append(newcols)
            else:
                row(k+1, n, newcols, (left|b) << 1, (right|b) >> 1, straight|b, list)

def queens(n):
    list = []
    row(0, n, (), 0L, 0L, 0L, list)
    return list

args = sys.argv[1:]
if len(args) > 0 and args[0] == "-s":
    symm = 1
    args = args[1:]

if len(args) > 0:
    n = string.atoi(args[0])
else:
    n = 8

answers = queens(n)

for i in answers:
    for j in i:
        print "."*j + "Q" + "."*(n-j-1)
    print

print "total", len(answers)
