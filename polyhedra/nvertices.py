#!/usr/bin/env python

# Given a set of points on a sphere, construct a polyhedron with
# those points as vertices, by determining the convex hull.

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

def polyprint(*a):
    realprint(a)

# First step: out of the n(n-1)/2 possible edges, determine which
# edges are actually part of the convex hull (and hence appear on
# the outside of the polyhedron).
hulledges = {}
for i in range(n-1):
    xi, yi, zi = points[i]
    for j in range(i+1, n):
	xj, yj, zj = points[j]

	# We begin by rotating our frame of reference so that both
	# points are in the x=0 plane, have the same z coordinate,
	# and that z coordinate is positive. In other words, we
	# rotate the sphere so that the radius bisecting the line
	# between the two points is the vector (0,0,1), and then
	# rotate around the z-axis so that the two points hit
	# opposite sides of the x-y plane. We expect to end up with
	# our two points being of the form (0,y,z) and (0,-y,z)
	# with z > 0.

	# Begin by rotating so that the midway point appears at
	# (0,0,1). To do this we must first find the midway point
	# and its polar coordinates...
	mx = (xi + xj) / 2
	my = (yi + yj) / 2
	mz = (zi + zj) / 2
	md = sqrt(mx**2 + my**2 + mz**2)
	# Very silly special case here: md might be zero. This
	# means that the midway point between the two points is the
	# origin, i.e. the points are exactly diametrically
	# opposite on the sphere. In this situation we can
	# legitimately pick _any_ point on the great circle half
	# way between them as a representative mid-way point; so
	# we'll simply take an arbitrary vector perpendicular to
	# point i.
	if md == 0:
	    # We'll take the vector product of point i with some
	    # arbitrarily chosen vector which isn't parallel to it.
	    # I'll find the absolute-smallest of the three
	    # coordinates of i, and choose my arbitrary vector to
	    # be the corresponding basis vector.
	    if abs(mx) <= abs(my) and abs(mx) <= abs(mz):
		mx, my, mz = 0, -zi, yi
	    elif abs(my) <= abs(mx) and abs(my) <= abs(mz):
		mx, my, mz = zi, 0, -xi
	    else: # abs(mz) <= abs(mx) and abs(mz) <= abs(my)
		mx, my, mz = -yi, xi, 0
	    # Now recompute the distance so we can normalise as
	    # before.
	    md = sqrt(mx**2 + my**2 + mz**2)
	mx = mx / md
	my = my / md
	mz = mz / md
	theta = atan2(my, mx)
	phi = asin(mz)
	# ... and construct a matrix which first rotates by -theta
	# about the z-axis, thus bringing the point to the
	# meridian, and then rotates by pi/2-phi about the y-axis
	# to bring the point to (0,0,1).
	#
	# That matrix is therefore
	#
	#  ( cos(pi/2-phi)  0 -sin(pi/2-phi) ) ( cos(-theta) -sin(-theta) 0 )
	#  (       0        1        0       ) ( sin(-theta)  cos(-theta) 0 )
	#  ( sin(pi/2-phi)  0  cos(pi/2-phi) ) (      0            0      1 )
	#
	# which comes to
	#
	#  ( cos(theta)*sin(phi)  sin(theta)*sin(phi)  -cos(phi) )
	#  (     -sin(theta)          cos(theta)           0     )
	#  ( cos(theta)*cos(phi)  sin(theta)*cos(phi)   sin(phi) )
	matrix1 = [
	[ cos(theta)*sin(phi),  sin(theta)*sin(phi),  -cos(phi) ],
	[     -sin(theta)    ,      cos(theta)     ,      0     ],
	[ cos(theta)*cos(phi),  sin(theta)*cos(phi),   sin(phi) ]]

	# Now pick an arbitrary point out of the two (point i will
	# do fine), rotate it via this matrix, determine its angle
	# psi from the y-axis, and construct the simple rotation
	# matrix
	#
	#  ( cos(-psi) -sin(-psi) 0 )
	#  ( sin(-psi)  cos(-psi) 0 )
	#  (     0          0     1 )
	#
	# which brings it back to the y-axis.
	xi1 = matrix1[0][0] * xi + matrix1[0][1] * yi + matrix1[0][2] * zi
	yi1 = matrix1[1][0] * xi + matrix1[1][1] * yi + matrix1[1][2] * zi
	# (no need to compute zi since we don't use it in this case)
	psi = atan2(-xi1, yi1)
	matrix2 = [
	[ cos(-psi), -sin(-psi), 0 ],
	[ sin(-psi),  cos(-psi), 0 ],
	[     0    ,      0    , 1 ]]

	# Now combine those matrices to produce the real one.
	matrix = []
	for y in range(3):
	    mrow = []
	    for x in range(3):
		s = 0
		for k in range(3):
		    s = s + matrix2[y][k] * matrix1[k][x]
		mrow.append(s)
	    matrix.append(mrow)

	# Test code to check that all that worked.
	#
	# This prints the transformed values of the two points, so
	# we can check that they have zero x coordinates, the y
	# coordinates are negatives of each other, and the z
	# coordinates are the same.
	#
	#xi1 = matrix[0][0] * xi + matrix[0][1] * yi + matrix[0][2] * zi
	#yi1 = matrix[1][0] * xi + matrix[1][1] * yi + matrix[1][2] * zi
	#zi1 = matrix[2][0] * xi + matrix[2][1] * yi + matrix[2][2] * zi
	#print (100000 + xi1) - 100000, yi1, zi1
	#xj1 = matrix[0][0] * xj + matrix[0][1] * yj + matrix[0][2] * zj
	#yj1 = matrix[1][0] * xj + matrix[1][1] * yj + matrix[1][2] * zj
	#zj1 = matrix[2][0] * xj + matrix[2][1] * yj + matrix[2][2] * zj
	#print (100000 + xj1) - 100000, yj1, zj1
	#
	# And this computes the product of the matrix and its
	# transpose, which should come to the identity matrix since
	# it's supposed to be orthogonal.
	#
	#testmatrix = []
	#for y in range(3):
	#    mrow = []
	#    for x in range(3):
	#	s = 0
	#	for k in range(3):
	#	    s = s + matrix[y][k] * matrix[x][k]
	#	mrow.append((10000000 + s) - 10000000)
	#    testmatrix.append(mrow)
	#print testmatrix

	# Whew. So after that moderately hairy piece of linear
	# algebra, we can now transform our point set so that when
	# projected into the x-z plane our two chosen points become
	# 1. Do so.
	ppoints = []
	for k in range(n):
	    xk, yk, zk = points[k]
	    xk1 = matrix[0][0] * xk + matrix[0][1] * yk + matrix[0][2] * zk
	    #yk1 = matrix[1][0] * xk + matrix[1][1] * yk + matrix[1][2] * zk
	    zk1 = matrix[2][0] * xk + matrix[2][1] * yk + matrix[2][2] * zk
	    ppoints.append((xk1, zk1))

	# The point of all that was to produce a plane projection
	# of the point set, under which the entire edge we're
	# considering becomes a single point. Now what we do is to
	# see whether that _point_ is on the 2-D convex hull of the
	# projected point set.
	#
	# To do this we will go through all the other points and
	# figure out their bearings from the fulcrum point. Then
	# we'll sort those bearings into angle order (which is of
	# course cyclic modulo 2pi). If the fulcrum is part of the
	# convex hull, we expect there to be a gap of size > pi
	# somewhere in that set of angles, indicating that a line
	# can be drawn through the fulcrum at some angle such that
	# all the other points are on the same side of it.

	# First, compensate for any rounding errors in the above
	# linear algebra by averaging the (theoretically exactly
	# equal) projected coordinates of points i and j to get
	# coords which we will use as our canonical fulcrum.
	fx = (ppoints[i][0] + ppoints[j][0]) / 2
	fz = (ppoints[i][1] + ppoints[j][1]) / 2
	# Now find the list of angles.
	angles = []
	for k in range(n):
	    if k == i or k == j: continue
	    x, z = ppoints[k]
	    angle = atan2(z - fz, x - fx)
	    angles.append(angle)
	# Sort them.
	angles.sort()
	# Now go through and look for a gap of size pi. There are
	# two possible ways this can happen: either two adjacent
	# elements in the list are separated by more than pi, or
	# the two end elements are separated by _less_ than pi.
	hull = 0
	for k in range(len(angles)-1):
	    if angles[k+1] - angles[k] > pi:
		hull = 1
		break
	if angles[-1] - angles[0] < pi:
	    hull = 1

	if hull:
	    hulledges[(i,j)] = 1

# Now we know the set of edges involved in the polyhedron, we need
# to combine them into faces. To do this we will have to consider
# each edge, going _both_ ways.
followedges = {}
for i in range(n):
    xi, yi, zi = points[i]
    for j in range(n):
	xj, yj, zj = points[j]
	if i == j: continue
	if not (hulledges.has_key((i,j)) or hulledges.has_key((j,i))): continue

	# So we have an edge from point i to point j. We imagine we
	# are walking along that edge from i to j with the
	# intention of circumnavigating the face to our left. So
	# when we reach j, we must turn left on to another edge,
	# and the question is which edge that is.
	#
	# To do this we will begin by rotating so that point j is
	# at (0,0,1). This has been done in several other parts of
	# this code base so I won't comment it in full yet again...
	theta = atan2(yj, xj)
	phi = asin(zj)
	matrix = [
	[ cos(theta)*sin(phi),  sin(theta)*sin(phi),  -cos(phi) ],
	[     -sin(theta)    ,      cos(theta)     ,      0     ],
	[ cos(theta)*cos(phi),  sin(theta)*cos(phi),   sin(phi) ]]

	# Now we are looking directly down on point j. We can see
	# some number of convex-hull edges coming out of it; we
	# determine the angle at which each one emerges, and find
	# the one which is closest to the i-j edge on the left.
	angles = []
	for k in range(n):
	    if k == j: continue
	    if not (hulledges.has_key((k,j)) or hulledges.has_key((j,k))):
		continue
	    xk, yk, zk = points[k]
	    xk1 = matrix[0][0] * xk + matrix[0][1] * yk + matrix[0][2] * zk
	    yk1 = matrix[1][0] * xk + matrix[1][1] * yk + matrix[1][2] * zk
	    #zk1 = matrix[2][0] * xk + matrix[2][1] * yk + matrix[2][2] * zk
	    angles.append((atan2(xk1, yk1), k))
	# Sort by angle, in reverse order.
	angles.sort(lambda a, b: a[0] < b[0] or (a[0] != b[0] and -1))
	# Search for i and take the next thing below it. Wrap
	# round, of course: if angles[0] is i then we want
	# angles[-1]. Conveniently this will be done for us by
	# Python's array semantics :-)
	k = None
	for index in range(len(angles)):
	    if angles[index][1] == i:
		k = angles[index-1][1]
		break
	assert k != None
	followedges[(i,j)] = (j,k)

# Now we're about ready to output our polyhedron definition. The
# only thing we're missing is the surface normals, and we'll do
# those as we go along.

# First the vertices.
for i in range(n):
    vlabel = "vertex_" + str(i)
    polyprint("point", vlabel, points[i][0], points[i][1], points[i][2])

# Now, the faces. We'll simply delete entries from followedges() as
# we go around, so as to avoid doing any face more than once.
while len(followedges) > 0:
    # Pick an arbitrary key in followedges.
    start = this = followedges.keys()[0]
    vertices = []
    while 1:
	vertices.append(this[0])
	next = followedges[this]
	del followedges[this]
	this = next
	if this == start:
	    break
    flabel = "face"
    for i in vertices:
	flabel = flabel + "_" + str(i)
    for i in vertices:
	vlabel = "vertex_" + str(i)
	polyprint("face", flabel, vlabel)
    # Now compute the surface normal. To do this I'm going to take
    # the vector product of each adjacent pair of edges; _all_ of
    # these should produce some non-zero vector which is normal to
    # the face. Then I'm simply going to sum those to get a single
    # canonical surface normal, normalise it to unit length, and
    # output it.
    vertices = vertices + vertices[0:2]
    xn = yn = zn = 0
    for p in range(len(vertices)-2):
	i, j, k = vertices[p:p+3]
	xij = points[j][0] - points[i][0]
	yij = points[j][1] - points[i][1]
	zij = points[j][2] - points[i][2]
	xjk = points[k][0] - points[j][0]
	yjk = points[k][1] - points[j][1]
	zjk = points[k][2] - points[j][2]
	xn = xn + yij*zjk - zij*yjk
	yn = yn + zij*xjk - xij*zjk
	zn = zn + xij*yjk - yij*xjk
    dn = sqrt(xn**2 + yn**2 + zn**2)
    xn = xn / dn
    yn = yn / dn
    zn = zn / dn
    polyprint("normal", flabel, xn, yn, zn)
