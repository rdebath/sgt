#!/usr/bin/env python

# Sanitise a polyhedron description, by merging vertices and faces
# which `obviously' ought to be merged.

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

def polyprint(*a):
    realprint(a)

global_done_something = 0

# Constants.
vthreshold = 1e-3    # two vertices closer than this are considered identical
nthreshold = 1e-3    # two normals closer than this are considered parallel

# Merge vertices.
vweight = {}
while 1:
    done_something = 0
    for i in vertices.keys():
	xi, yi, zi = vertices[i]
	for j in vertices.keys():
	    xj, yj, zj = vertices[j]
	    if i == j:
		continue
	    d = sqrt((xj - xi) ** 2 + (yj - yi) ** 2 + (zj - zi) ** 2)
	    if d < vthreshold:
		# Merge vertices i and j. This means
		#  - we replace them with a single vertex which has
		#    their mean position. (The mean is weighted by
		#    the number of other vertices we have already
		#    merged into them, so that merging five
		#    vertices always ends up with the _unweighted_
		#    mean of the original five.)
		#  - we replace every reference to the two original
		#    vertices with the new one. In particular, any
		#    face on whose boundary the two vertices are
		#    adjacent has them both replaced with a single
		#    occurrence of the new one.
		#  - surface normals are left alone. It's simplest.
		newname = "merged_" + i + "_" + j
		while vertices.has_key(newname):
		    newname = "x" + newname
		wi = vweight.get(i, 1)
		wj = vweight.get(j, 1)
		xn = (xi * wi + xj * wj) / (wi + wj)
		yn = (yi * wi + yj * wj) / (wi + wj)
		zn = (zi * wi + zj * wj) / (wi + wj)
		vertices[newname] = (xn, yn, zn)
		vweight[newname] = wi + wj
		for f in faces.keys():
		    flist = faces[f]
		    for k in range(len(flist)):
			if flist[k] == i or flist[k] == j:
			    flist[k] = newname
		    for k in range(len(flist)):
			if flist[k] == newname and flist[k-1] == newname:
			    del flist[k]
			    break
		del vertices[i]
		del vertices[j]
		sys.stderr.write("Merged vertices: %s, %s -> %s\n" % \
		(i, j, newname))
		done_something = global_done_something = 1
	    if done_something:
		break
	if done_something:
	    break
    if not done_something:
	break

# Merge faces. We can only do this to a pair of faces sharing an
# edge.
fweight = {}
while 1:
    done_something = 0
    for i in faces.keys():
	for j in faces.keys():
	    if j == i: continue
	    fi = faces[i]
	    fj = faces[j]
	    # Find one shared vertex.
	    v1 = None
	    for v in fi:
		if v in fj:
		    v1 = v
		    break
	    if v1 == None:
		continue # no shared vertex at all
	    # Now see if a second shared vertex lurks either side
	    # of that one.
	    vi = fi.index(v1)
	    vj = fj.index(v1)
	    if fi[(vi+1) % len(fi)] == fj[(vj-1) % len(fj)]:
		v2 = fi[(vi+1) % len(fi)]
		vj = (vj-1) % len(fj)
	    elif fi[(vi-1) % len(fi)] == fj[(vj+1) % len(fj)]:
		v2 = v1
		v1 = fi[(vi-1) % len(fi)]
		vi = (vi-1) % len(fi)
	    else:
		continue # no pair of shared vertices
	    # Now we have a pair of shared vertices: fi[vi] and
	    # fi[vi+1] correspond to fj[vj] and fj[vj+1], in the
	    # reverse of that order.
	    assert fi[vi] == fj[(vj+1) % len(fj)]
	    assert fj[vj] == fi[(vi+1) % len(fi)]
	    assert v1 == fi[vi]
	    assert v2 == fj[vj]

	    # Compare the surface normals. We do this by ensuring
	    # they are both of unit length, and then simply
	    # subtracting them.
	    xni, yni, zni = normals[i]
	    xnj, ynj, znj = normals[j]
	    dni = sqrt(xni**2 + yni**2 + zni**2)
	    xni, yni, zni = xni/dni, yni/dni, zni/dni
	    dnj = sqrt(xnj**2 + ynj**2 + znj**2)
	    xnj, ynj, znj = xnj/dnj, ynj/dnj, znj/dnj
	    dn = sqrt((xni-xnj)**2 + (yni-ynj)**2 + (zni-znj)**2)
	    if dn < nthreshold:

		# Aha! We have two faces to merge. This means
		#  - our new face consists of the vertex lists of
		#    both other ones, cut at the shared edge and
		#    spliced together. In other words, if one edge
		#    list is [r,s,X,Y,p,q] and the other is
		#    [X,a,b,c,Y], we output [p,q,r,s,X,a,b,c,Y].
		#  - our new face's surface normal is the mean of
		#    the existing two, weighted as before to take
		#    account of how many other faces we've already
		#    merged together.

		newname = "merged_" + i + "_" + j
		while faces.has_key(newname):
		    newname = "x" + newname
		wi = fweight.get(i, 1)
		wj = fweight.get(j, 1)
		xn = (xni * wi + xnj * wj) / (wi + wj)
		yn = (yni * wi + ynj * wj) / (wi + wj)
		zn = (zni * wi + znj * wj) / (wi + wj)
		normals[newname] = (xn, yn, zn)
		fweight[newname] = wi + wj
		if vi == len(fi)-1:
		    # This face is of the form [X,a,b,c,Y].
		    fi = fi[1:-1]
		else:
		    # This face is of the form [r,s,Y,X,p,q].
		    fi = fi[vi+2:] + fi[:vi]
		if vj == len(fj)-1:
		    # This face is of the form [X,a,b,c,Y].
		    fj = fj[1:-1]
		else:
		    # This face is of the form [r,s,Y,X,p,q].
		    fj = fj[vj+2:] + fj[:vj]
		faces[newname] = fi + [v1] + fj + [v2]
		del faces[i]
		del faces[j]
		del normals[i]
		sys.stderr.write("Merged faces: %s, %s -> %s\n" % \
		(i, j, newname))
		done_something = global_done_something = 1
	    if done_something:
		break
	if done_something:
	    break
    if not done_something:
	break

# Output the cleaned-up polyhedron.
for key, value in vertices.items():
    polyprint("point", key, value[0], value[1], value[2])
for key in faces.keys():
    for vertex in faces[key]:
	polyprint("face", key, vertex)
    polyprint("normal", key, normals[key][0], normals[key][1], normals[key][2])

# Return success iff we actually did anything.
sys.exit(not global_done_something)
