#!/usr/bin/env python

# Read in a polyhedron description, and output a textual description
# of each face including the vertex angles and edge lengths.

import sys
import string
import random
from math import pi, acos, sqrt
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

def oprint(*a):
    realprint(a)

# Iterate over each face building a list of ordered pairs of vertices
# (i.e. directed edges) it owns.
diredges = {}
for key, vlist in faces.items():
    for i in range(len(vlist)):
        n0, n1 = [vlist[i+j] for j in range(-1,1)]
        diredges[(n0, n1)] = key

# Iterate over each face and describe it.
for key, vlist in faces.items():
    oprint("Face", key)
    # Iterate over every edge and vertex.
    for i in range(len(vlist)):
        n0, n1, n2 = [vlist[i+j] for j in range(-2,1)]
        v0, v1, v2 = [vertices[vlist[i+j]] for j in range(-2,1)]

        # Use the vectors from v1 to {v0,v2} to determine the interior
        # angle of the face at v1.
        #
        # This isn't rigorous in the case of non-convex polyhedra,
        # because we don't pay attention to the direction of turning.
        # This polyhedron description format has no convention for the
        # direction of traversal of each face, so we'd have to cope
        # with both directions. If this did need doing, then probably
        # the simplest approach would be:
        #
        #  - reduce to two dimensions by transforming the coordinate
        #    system so as to put the face normal along the z-axis
        #  - compute the _exterior_ angle at each vertex _with sign_,
        #    by rotating the vector v0->v1 into the positive x
        #    direction and computing atan2 of the vector v1->v2. (The
        #    usual convention for atan2, that it returns in the
        #    interval [-pi,+pi], is the right one here.)
        #  - add up all the exterior angles. They should sum to either
        #    +2pi or -2pi in total. If the latter, negate them all.
        #  - now convert every exterior angle into an interior one by
        #    subtracting from pi, at which point the negative exterior
        #    angles (representing concave corners) translate into
        #    interior angles greater than pi.
        #
        # As a byproduct, that analysis also tells us the direction of
        # traversal of each face.

        v1v0 = tuple([v0[i]-v1[i] for i in range(3)])
        v1v2 = tuple([v2[i]-v1[i] for i in range(3)])
        sqrlen01 = sum([v1v0[i]**2 for i in range(3)])
        sqrlen12 = sum([v1v2[i]**2 for i in range(3)])
        dotprod = sum([v1v0[i]*v1v2[i] for i in range(3)])
        cosine = dotprod / sqrt(sqrlen01 * sqrlen12)
        angle = acos(cosine) * 180/pi
        oprint("    Angle at vertex", n1, "=", angle, "degrees")

        # Also give the length of the edge leading to the next vertex.
        length = sqrt(sqrlen12)
        oprint("    Length of edge", n1, "-", n2, "=", length)

        # And the dihedral angle between that and the adjacent face.
        otherface = diredges[(n2, n1)]
        thisnormal = normals[key]
        thatnormal = normals[otherface]
        # Be cautious, just in case due to some annoying bug the
        # normals aren't actually normalised.
        sqrlenthis = sum([thisnormal[i]**2 for i in range(3)])
        sqrlenthat = sum([thatnormal[i]**2 for i in range(3)])
        dotprod = sum([thisnormal[i]*thatnormal[i] for i in range(3)])
        cosine = dotprod / sqrt(sqrlenthis * sqrlenthat)
        # Dihedral angles seem to be typically quoted not as the angle
        # between normal vectors but as 180 minus that, i.e. the angle
        # as measured _inside_ the solid. For example, the
        # dodecahedron's dihedral angle is generally quoted as 116.6
        # degrees, not 63.4.
        dihedral = acos(-cosine) * 180/pi
        oprint("    Dihedral angle with face", otherface, "=", dihedral,
               "degrees")
