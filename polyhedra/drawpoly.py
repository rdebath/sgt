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

vertices = {}
faces = {}
normals = {}

lineno = 0
while 1:
    s = infile.readline()
    if s == "": break
    sl = string.split(s)
    lineno = lineno + 1
    if sl[0] == "point" and len(sl) == 5:
	vertices[sl[1]] = \
	(string.atof(sl[2]), string.atof(sl[3]), string.atof(sl[4]))
    elif sl[0] == "face" and len(sl) == 3:
	if not vertices.has_key(sl[2]):
	    sys.stderr.write("line %d: vertex %s not defined\n" % \
	    (lineno, sl[2]))
	else:
	    if not faces.has_key(sl[1]):
		faces[sl[1]] = []
	    faces[sl[1]].append(sl[2])
    elif sl[0] == "normal" and len(sl) == 5:
	if not faces.has_key(sl[1]):
	    sys.stderr.write("line %d: face %s not defined\n" % \
	    (lineno, sl[1]))
	else:
	    normals[sl[1]] = \
	    (string.atof(sl[2]), string.atof(sl[3]), string.atof(sl[4]))
    else:
	sys.stderr.write("line %d: unrecognised line format\n" % lineno)
	continue
infile.close()

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
psprint("288 500 translate 150 dup scale 1 setlinejoin")

# Compute the perspective coordinates of each point.
pvertices = {}
for key, value in vertices.items():
    x, y, z = value
    xp = x / (z + 14) * 10
    yp = y / (z + 14) * 10
    pvertices[key] = (xp, yp)

# Draw each face, either in a thick or thin line depending on
# whether it's behind or in front. (Since we haven't rotated the
# polyhedron at all, this is simply determined by examining the z
# component of its surface normal.)
for key, vlist in faces.items():
    if normals[key][2] < 0:
	psprint("0.01 setlinewidth")
    else:
	psprint("0.0025 setlinewidth")
    psprint("newpath")
    cmd = "moveto"
    for p in vlist:
	v = pvertices[p]
	psprint("   ", v[0], v[1], cmd)
	cmd = "lineto"
    psprint("closepath stroke")

psprint("showpage grestore")
psprint("%%EOF")
