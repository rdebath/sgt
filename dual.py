#!/usr/bin/env python

# Take a polyhedron description and generate its dual solid.

# It's reasonably well known that the dual of a polyhedron has the
# same number of edges and somehow interchanges faces and vertices;
# and it's also fairly well known that you can position a solid and
# its dual through one another in such a way that corresponding
# pairs of edges always intersect at right angles. But the precise
# means of constructing it in 3D space is less immediately obvious;
# given a solid with multiple different types of face, how do you
# arrange for each vertex of the dual to be positioned _exactly_
# right relative to the others so that all the right planes meet at
# all the right points? And _can_ you always have all the edges
# intersect at right angles?
#
# As a prelude to actually doing it, I discuss the theory behind
# the concept.

# Dual solids arise from the general geometric Principle of
# Duality. This states that, in 3D space, all the axioms of
# geometry can be stated in a form which is entirely symmetric in a
# transformation which interchanges the words `point' and `plane'.
# Thus, for example, it takes three planes with no line in common
# to define a unique point, and it takes three points with no line
# in common to define a unique plane. And since any _theorem_ in
# geometry can be deduced from those axioms, it must be the case
# that every theorem in geometry has a dual form in which points
# and planes are interchanged, and which is just as true.
#
# This hints that there might exist some _transformation_ of
# general 3D space which maps points to planes, planes to points,
# and lines to other lines, such that the relationships between all
# these things are preserved. For example, if you start off with
# two lines intersecting a plane at the same point, what you're
# saying is that the plane has the same point in common with both
# lines - so after the transformation, the point (which is the
# image of the plane) has the same _plane_ (the image of the point)
# in common with both (images of the original) lines.
#
# So. Going back to dual polyhedra for a moment, one obvious way
# you can construct a dual of a _Platonic_ (perfectly regular)
# solid is to take the tangent plane to its circumscribed sphere at
# each vertex. Because the solid is so completely symmetric, this
# automatically guarantees that for every set of vertices around a
# single face of the original solid, the corresponding set of
# planes all meet at a single point in the dual. So this hints that
# in general, the dual transformation might turn a point into a
# plane whose closest approach to the origin is that point, and
# might turn a plane into the point where it most closely
# approaches the origin.
#
# But this isn't good enough. Consider three planes, all with the
# same closest-approach distance from the origin, meeting in a
# point. The closer the planes get to being identical (i.e. the
# closer together their closest-approach points are, and the nearer
# their normal vectors are to being parallel), the _nearer_ their
# common point is to the origin. In the limit, the distance of the
# common point from the origin approaches the distance of the
# planes, from above. However, in the dual scenario you have three
# points at the same distance from the origin, describing a plane;
# and the closer the points get to being identical, the _further_
# the closest-approach point of the plane is from the origin, and
# in the limit the plane's distance approaches the distance of the
# points from _below_. And in particular, this means that if you
# simply map closest-approach points of planes to points and vice
# versa, the image of the common point to the planes is not the
# image of the common plane to the points.
#
# The above scenario hints, however, that distances from the origin
# might work in an inverse fashion. So now let's try again with an
# inversion in place. We map a point with position vector x to the
# plane whose closest approach point to the origin is x / (x.x) -
# i.e. its distance from the origin is the reciprocal of that of
# the original point.
#
# Now consider a point x and a plane with closest-approach point y.
# The point meets the plane if and only if x.y = y.y. So let's
# transform the point x into a plane with closest-approach point
# x', and transform the plane y into a point y'. Then our
# definition says that x' = x/(x.x) and y' = y/(y.y). So under what
# circumstances does the point y' meet the plane x'?
#
# This happens iff                x'.y' = x'.x'
#              <=>     x.y / (x.x)(y.y) = x.x / (x.x)^2
#              <=>     x.y / (x.x)(y.y) = 1 / x.x
#              <=>                  x.y = y.y
#
# _Now_ we've got it. The point x meets the plane y if and only if
# the point y' meets the plane x'. So this transformation _works_,
# and always preserves the relation `does this point meet that
# plane'.
#
# Now let's look at lines. A line can be seen as the union of a
# whole load of points, but it can also be seen as the intersection
# of a whole load of planes. The closest-approach points of those
# planes (it turns out) form a circle; the origin and the closest-
# approach point of the line itself are diametrically opposite
# points on the circle's circumference, and the circle lies in a
# plane perpendicular to the line.
#
# Under our transformation above, conveniently enough, a set of
# points forming a circle through the origin maps to a set of
# points forming a straight line, and vice versa. So this transform
# really does map straight lines in the original 3D space into
# perpendicular straight lines in the transformed one: the set of
# points contained within the original line transform into a set of
# planes with exactly one line common to them all, and vice versa.
#
# This also tells us that the closest-approach points of the two
# (before and after) lines have distances from the origin which are
# reciprocals of one another. This implies that for a _general_
# polyhedron, although every edge of the original solid corresponds
# to a perpendicular edge of the dual, it's not true in general
# that you can place the solids `through' one another in such a way
# that all the edge pairs actually intersect at once. This is only
# true if every edge has the _same_ closest-approach distance from
# the origin. (As it happens, this is true of all the Platonic and
# Archimedean solids.)

import sys
import string
from math import sqrt

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

# The vertices of the new polyhedron are constructed from the faces
# of the original.
for key in faces.keys():
    # We already have a normal vector for the face. So take the dot
    # product of that normal vector with each vertex on the face,
    # take the average (just in case), invert the length, and
    # there's our vertex.
    nx, ny, nz = normals[key]
    nl = sqrt(nx*nx + ny*ny + nz*nz)
    nx, ny, nz = nx/nl, ny/nl, nz/nl
    dps = dpn = 0
    for v in faces[key]:
	x, y, z = vertices[v]
	dps = dps + x*nx + y*ny + z*nz
	dpn = dpn + 1
    dist = dpn / dps   # reciprocal of average of dot products
    nx, ny, nz = nx * dist, ny * dist, nz * dist
    polyprint("point", key, nx, ny, nz)

# Now we go through and build up a database of edges. For each
# original face, we find every vertex pair connected by an edge of
# that face. Then we construct a hash mapping that (ordered) vertex
# pair to the face in question. (So we end up including each edge
# in _both_ directions, once pointing at each face.)
edges = {}
for key in faces.keys():
    for i in range(len(faces[key])):
	v1, v2 = faces[key][i-1], faces[key][i] # -1 neatly causes wraparound
	edges[(v1,v2)] = key

# Now we can output the faces, one (of course) for each vertex of
# the old polyhedron.
for key in vertices.keys():
    # Start by finding one face of the old polyhedron which
    # contains this vertex. This gives us one vertex of the new one
    # which is on this face.
    fstart = None
    for f in faces.keys():
	if key in faces[f]:
	    fstart = f
	    break
    assert fstart != None

    f = fstart
    while 1:
	polyprint("face", key, f)
	# Now find the edge of that face which comes _in_ to that
	# vertex, and then look its reverse up in the edge database.
	# This gives us the next face going round.
	fl = faces[f]
	i = fl.index(key)
	v1, v2 = fl[i-1], fl[i]
	f = edges[(v2,v1)]
	if f == fstart:
	    break

    # Finally, the normal vector to this face must be the position
    # vector of the corresponding original vertex, normalised to
    # length 1. It's that simple.
    nx, ny, nz = vertices[key]
    nl = sqrt(nx*nx + ny*ny + nz*nz)
    nx, ny, nz = nx/nl, ny/nl, nz/nl
    polyprint("normal", key, nx, ny, nz)
