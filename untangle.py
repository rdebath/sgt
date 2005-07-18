#!/usr/bin/env python

# Convert a polyhedron description into a descriptive game ID for
# `Untangle', a member of my puzzle collection.

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

# Collect the edge list.
edges = []
for i in faces.keys():
    vs = faces[i]
    for k in range(len(vs)):
	v1 = vs[k-1]
	v2 = vs[k]
	if v1 < v2:
	    edges.append((v1,v2))
edges.sort()

# Shuffle the vertices.
vertexnumbers = range(len(vertices))
vertexnumber = {}
for i in vertices.keys():
    x = random.choice(vertexnumbers)
    vertexnumber[i] = x
    vertexnumbers.remove(x)

# Make sure at least two edges cross (so the puzzle isn't already
# solved). If not, swap round two vertex numbers so they do.
#
# When points are arranged on a circle, any two lines cross iff the
# endpoints of the two lines alternate on the circle. That is, if
# the circle is rotated so that one endpoint of one line is at
# zero, then the other endpoint of that line is _between_ the
# endpoints of the other line.
nv = len(vertices)
crossing = 0
noncrosses = []
for i in xrange(len(edges)-1):
    for j in xrange(i+1, len(edges)):
	i1, i2 = map(lambda x: vertexnumber[x], edges[i])
	j1, j2 = map(lambda x: vertexnumber[x], edges[j])
	if i1 == j1 or i1 == j2 or i2 == j1 or i2 == j2:
	    continue
	i2 = (i2 + nv - i1) % nv
	j1 = (j1 + nv - i1) % nv
	j2 = (j2 + nv - i1) % nv
	if (j1 - i2 > 0) != (j2 - i2 > 0):
	    crossing = 1
	else:
	    noncrosses.append((i, j))
if not crossing:
    i, j = random.choice(noncrosses)
    iv = random.choice(edges[i])
    jv = random.choice(edges[j])
    a, b = vertexnumbers[iv], vertexnumbers[jv]
    vertexnumbers[iv], vertexnumbers[jv] = b, a

edges = map(lambda t: (vertexnumber[t[0]], vertexnumber[t[1]]), edges)
edges = map(lambda t: (min(t[0],t[1]), max(t[0],t[1])), edges)
edges.sort()

output = "%d:" % len(vertices)
sep = ""
for v1, v2 in edges:
    output = output + sep + "%d-%d" % (v1, v2)
    sep = ","
print output
