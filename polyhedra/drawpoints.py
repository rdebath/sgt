#!/usr/bin/env python

# Read in a polyhedron description, and draw the polyhedron in
# wireframe PostScript.

import sys
import string
import random
from math import pi, asin, atan2, cos, sin, sqrt
from crosspoint import crosspoint

args = sys.argv[1:]

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

points = []

lineno = 0
while 1:
    s = infile.readline()
    if s == "": break
    sl = string.split(s)
    lineno = lineno + 1
    if len(sl) != 3:
	sys.stderr.write("line %d: expected three fields per line, got %d\n" \
	% (lineno, len(sl)))
	continue
    points.append((string.atof(sl[0]), string.atof(sl[1]), string.atof(sl[2])))
infile.close()

n = len(points)

def realprint(a):
    for i in range(len(a)):
	outfile.write(str(a[i]))
	if i < len(a)-1:
	    outfile.write(" ")
	else:
	    outfile.write("\n")

def psprint(*a):
    realprint(a)

psprint("%!PS-Adobe-1.0")
psprint("%%Pages: 1")
psprint("%%EndComments")
psprint("%%Page: 1")
psprint("gsave")
psprint("288 400 translate 150 dup scale 0.0025 setlinewidth")
psprint("newpath 0 0 1 0 360 arc stroke")

s = 0.02
for x, y, z in points:
    if z > 0:
	# X denotes a point at the back.
	psprint("newpath", x+s, y+s, "moveto", x-s, y-s, "lineto")
	psprint(x+s, y-s, "moveto", x-s, y+s, "lineto stroke")
    else:
	# O denotes a point at the front.
	psprint("newpath", x, y, s, "0 360 arc stroke")

psprint("showpage grestore")
psprint("%%EOF")
