#!/usr/bin/env python

# Read in a polyhedron description, and output a JavaScript list
# suitable for use in the "data" field of a canvas used by
# canvaspoly.js.

import sys
import os
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

# Normalise the radius of our polyhedron so that it just fits
# inside the unit sphere.
maxlen = 0
for v in vertices.values():
    vlen = sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    if maxlen < vlen: maxlen = vlen
for v in vertices.keys():
    vv = vertices[v]
    vertices[v] = (vv[0]/maxlen, vv[1]/maxlen, vv[2]/maxlen)

# Assign integer indices to vertices and faces.
vindex = {}
findex = {}
vlist = []
flist = []
for v in vertices.keys():
    vindex[v] = len(vlist)
    vlist.append(v)
for f in faces.keys():
    findex[f] = len(flist)
    flist.append(f)

s = "[%d" % len(vertices)
for i in range(len(vertices)):
    v = vertices[vlist[i]]
    s = s + ",%.17g,%.17g,%.17g" % (v[0], v[1], v[2])
s = s + ",%d" % len(faces)
for i in range(len(faces)):
    f = faces[flist[i]]
    n = normals[flist[i]]
    s = s + ",%d" % len(f)
    for v in f:
        s = s + ",%d" % vindex[v]
    s = s + ",%.17g,%.17g,%.17g" % (n[0], n[1], n[2])
s = s + "]"
print s
