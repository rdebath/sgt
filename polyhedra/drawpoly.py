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
psprint("288 400 translate 150 dup scale 1 setlinejoin")

# Scale the solid so that it fits within a fixed-size sphere (so
# that every picture output from this code will be roughly the same
# size).
absmax = 0
for key, value in vertices.items():
    x, y, z = value
    d2 = x**2 + y**2 + z**2
    if absmax < d2: absmax = d2
scale = 1.3 / sqrt(absmax)

def ptransform(x, y, z):
    xp = x*scale / (z*scale + 14) * 10
    yp = y*scale / (z*scale + 14) * 10
    return xp, yp

# Compute the perspective coordinates of each point.
pvertices = {}
for key, value in vertices.items():
    x, y, z = value
    xp, yp = ptransform(x, y, z)
    pvertices[key] = (xp, yp)

# Figure out which faces are facing forward, and which backward. To
# do this, we draw the vector from the eye point (0,0,0) to the
# centre of the face, take the dot product of this vector with the
# surface normal, and test the sign of that.
forward = {}
for key, vlist in faces.items():
    xt = yt = zt = 0
    for p in vlist:
	xt = xt + vertices[p][0]
	yt = yt + vertices[p][1]
	zt = zt + vertices[p][2] + 14
    xt = xt / len(vlist)
    yt = yt / len(vlist)
    zt = zt / len(vlist)
    dp = xt * normals[key][0] + yt * normals[key][1] + zt * normals[key][2]
    if dp > 0:
	forward[key] = 0
    else:
	forward[key] = 1
    pass

def drawface(vlist):
    psprint("newpath")
    cmd = "moveto"
    for p in vlist:
	v = pvertices[p]
	psprint("   ", v[0], v[1], cmd)
	cmd = "lineto"
    psprint("closepath stroke")

# Draw rear-facing faces in a thin line. (Since we haven't rotated
# the polyhedron at all, rear-facing-ness is simply determined by
# examining the z component of its surface normal.)
for key, vlist in faces.items():
    if not forward[key]:
	psprint("0.001 setlinewidth 0 setgray")
	drawface(vlist)

# Draw forward-facing faces in a very thick _white_ line (so that
# hidden lines have gaps in to make it clear that the
# forward-facing lines go in front).
for key, vlist in faces.items():
    if forward[key]:
	psprint("0.03 setlinewidth 1 setgray")
	drawface(vlist)

# Draw forward-facing faces again, in a medium-thick black line.
for key, vlist in faces.items():
    if forward[key]:
	psprint("0.01 setlinewidth 0 setgray")
	drawface(vlist)

psprint("showpage grestore")
psprint("%%EOF")
