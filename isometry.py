#!/usr/bin/env python

# Rotate and reflect a polyhedron description. That is, we are
# given an orthogonal matrix on the command line (nine arguments,
# each of which is a Python expression), and we transform the
# polyhedron using that matrix.
#
# The matrix is given in column-by-column form. That is, the
# arguments A B C D E F G H I map into the matrix
#
#    ( A D G )
#    ( B E H )
#    ( C F I )
#
# so that (A,B,C) is the image vector of (1,0,0), (D,E,F) that of
# (0,1,0), and (G,H,I) that of (0,0,1).

import sys
import string
import random
from math import *

args = sys.argv[1:]

if len(args) < 9:
    sys.stderr.write("usage: isometry.py a b c d e f g h i" + \
    " [infile [outfile]]\n")
    sys.exit(len(args) > 0)
else:
    matrix = []
    for i in range(9):
        s = args[0]
        args = args[1:]
        matrix.append(eval(s))

# Check the matrix is orthogonal, or close enough.
def check(x, y):
    if abs(x - y) > 1e-6:
        sys.stderr.write("input matrix is not orthogonal\n")
        sys.exit(1)
check(matrix[0]*matrix[0] + matrix[1]*matrix[1] + matrix[2]*matrix[2], 1)
check(matrix[0]*matrix[3] + matrix[1]*matrix[4] + matrix[2]*matrix[5], 0)
check(matrix[0]*matrix[6] + matrix[1]*matrix[7] + matrix[2]*matrix[8], 0)
check(matrix[3]*matrix[0] + matrix[4]*matrix[1] + matrix[5]*matrix[2], 0)
check(matrix[3]*matrix[3] + matrix[4]*matrix[4] + matrix[5]*matrix[5], 1)
check(matrix[3]*matrix[6] + matrix[4]*matrix[7] + matrix[5]*matrix[8], 0)
check(matrix[6]*matrix[0] + matrix[7]*matrix[1] + matrix[8]*matrix[2], 0)
check(matrix[6]*matrix[3] + matrix[7]*matrix[4] + matrix[8]*matrix[5], 0)
check(matrix[6]*matrix[6] + matrix[7]*matrix[7] + matrix[8]*matrix[8], 1)

# Find the matrix's determinant, to see if it's a reflection.
det = \
matrix[0] * matrix[4] * matrix[8] + \
matrix[1] * matrix[5] * matrix[6] + \
matrix[2] * matrix[3] * matrix[7] - \
matrix[0] * matrix[5] * matrix[7] - \
matrix[1] * matrix[3] * matrix[8] - \
matrix[2] * matrix[4] * matrix[6]
check(abs(det), 1)
reflection = (det < 0)

if len(args) > 0:
    infile = open(args[0], "r")
    args = args[1:]
else:
    infile = sys.stdin

if len(args) > 0:
    outfile = open(args[0], "w")
    args = args[1:]
else:
    outfile = sys.stdout

def realprint(a):
    for i in range(len(a)):
	outfile.write(str(a[i]))
	if i < len(a)-1:
	    outfile.write(" ")
	else:
	    outfile.write("\n")

def polyprint(*a):
    realprint(a)

def transform(point, matrix=matrix):
    x, y, z = point
    a, b, c, d, e, f, g, h, i = matrix
    x1 = a*x + d*y + g*z
    y1 = b*x + e*y + h*z
    z1 = c*x + f*y + i*z
    return x1, y1, z1

lineno = 0
currface = None
facestr = ""
while 1:
    s = infile.readline()
    if s == "": break
    sl = string.split(s)
    lineno = lineno + 1
    if (sl[0] == "point" or sl[0] == "normal") and len(sl) == 5:
        if facestr != "":
            outfile.write(facestr)
            facestr = ""
        point = (string.atof(sl[2]), string.atof(sl[3]), string.atof(sl[4]))
        p2 = transform(point)
        x, y, z = p2
        polyprint(sl[0], sl[1], x, y, z)
    elif sl[0] == "face" and reflection:
        if sl[1] != currface:
            outfile.write(facestr)
            facestr = ""
        facestr = s + facestr
        currface = sl[1]
    else:
        if facestr != "":
            outfile.write(facestr)
            facestr = ""
        outfile.write(s)
outfile.write(facestr)
infile.close()
