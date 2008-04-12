#!/usr/bin/env python

# Read in a polyhedron description, and output Flash ActionScript
# code suitable for compiling with MTASC (i.e. a .as file) into a
# dedicated viewer application for that polyhedron.

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

def realprint(a):
    for i in range(len(a)):
        outfile.write(str(a[i]))
        if i < len(a)-1:
            outfile.write(" ")
        else:
            outfile.write("\n")

def asprint(*a):
    realprint(a)

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

# We expect the template for the Flash source to be called
# flashpoly.as and to reside in the same directory as this script.
# Python helpfully lets us know that directory at all times.
template = os.path.join(sys.path[0], "flashpoly.as");
tempfile = open(template, "r")
while 1:
    line = tempfile.readline()
    if line == "": break
    if string.find(line, "--- BEGIN POLYHEDRON ---") >= 0:
        # Instead of the test polyhedron description in the
        # template file, substitute our own.
        for i in range(len(vertices)):
            v = vertices[vlist[i]]
            asprint("       var p%d = { x:%.17g, y:%.17g, z:%.17g };" % \
            (i, v[0], v[1], v[2]))
        for i in range(len(faces)):
            f = faces[flist[i]]
            n = normals[flist[i]]
            fstr = string.join(["p%d" % vindex[v] for v in f], ",")
            asprint(("       var f%d = { points: new Array(%s), x:%.17g, " + \
            "y:%.17g, z:%.17g };") % (i, fstr, n[0], n[1], n[2]))
        vstr = string.join(["p%d" % i for i in range(len(vertices))], ",")
        fstr = string.join(["f%d" % i for i in range(len(faces))], ",")
        asprint(("       poly = { points: new Array(%s), " + \
        "faces: new Array(%s) };") % (vstr, fstr))
        while 1:
            line = tempfile.readline()
            if line == "": break
            if string.find(line, "--- END POLYHEDRON ---") >= 0: break
    else:
        outfile.write(line)

tempfile.close()
