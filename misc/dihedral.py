#!/usr/bin/env python

# Matthew Chadwick sent me e-mail about my polyhedra web page, and
# asked a question about dihedral angles of a solid versus cut edge
# angles on a net of that solid. (The cut edge angle at a vertex is
# the plane angle you would get if you cut along _one_ of the edges
# meeting at that vertex and unfolded it so all the faces lay flat:
# the faces would not entirely surround the vertex, and the angle
# they leave over is the cut edge angle at that vertex. It is equal
# to 360 degrees minus the sum of all the interior angles of faces
# meeting at that vertex, and thus is independent of which edge you
# choose to cut along.)
#
# Matthew Chadwick's question was whether the cut edge angle could
# be directly related to the dihedral angles around the vertex.
#
# This program proves the answer is no. It constructs, by 3D
# geometry and a rather nasty binary search, a vertex at which five
# faces meet in such a way that all five dihedral angles between
# them are exactly the same as the dihedral angle of the
# icosahedron. However, the faces' interior angles are not all the
# same, and in particular they sum to more than 300 degrees so that
# the cut edge angle is not the same as that at an icosahedron
# vertex.
#
# If run with the `-p' option, this program outputs two polyhedron
# description files describing pentagonal pyramids, exhibiting
# precisely the two vertices in question (the regular icosahedron
# vertex and the irregular one with the same dihedral angles).
# Anyone interested might enjoy feeding these to ../drawnet.py and
# actually making the two solids, so as to verify that the dihedral
# angles really are the same in both cases.

import sys
from math import *

def norm(v):
    "Norm (modulus) of a 3-vector."
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])

def dot(v1,v2):
    "Dot (scalar) product of two 3-vectors."
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]

def mult(v1,s):
    "Multiplication of a 3-vector by a scalar."
    return (v1[0]*s, v1[1]*s, v1[2]*s)

def add(v1,v2):
    "Addition of two 3-vectors."
    return (v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2])

def sub(v1,v2):
    "Subtraction of two 3-vectors."
    return (v1[0]-v2[0], v1[1]-v2[1], v1[2]-v2[2])

def cross(v1,v2):
    "Cross (vector) product of two 3-vectors."
    return (v1[1]*v2[2]-v2[1]*v1[2], \
            v1[2]*v2[0]-v2[2]*v1[0], \
            v1[0]*v2[1]-v2[0]*v1[1])

def normalise(v):
    "Normalise a vector into a unit vector."
    return mult(v, 1.0/norm(v))

def matmul(m1,m2):
    "Multiply two 3x3 matrices, stored as 3-tuples of 3-columns."
    c1 = (m1[0][0]*m2[0][0] + m1[1][0]*m2[0][1] + m1[2][0]*m2[0][2], \
          m1[0][1]*m2[0][0] + m1[1][1]*m2[0][1] + m1[2][1]*m2[0][2], \
          m1[0][2]*m2[0][0] + m1[1][2]*m2[0][1] + m1[2][2]*m2[0][2])
    c2 = (m1[0][0]*m2[1][0] + m1[1][0]*m2[1][1] + m1[2][0]*m2[1][2], \
          m1[0][1]*m2[1][0] + m1[1][1]*m2[1][1] + m1[2][1]*m2[1][2], \
          m1[0][2]*m2[1][0] + m1[1][2]*m2[1][1] + m1[2][2]*m2[1][2])
    c3 = (m1[0][0]*m2[2][0] + m1[1][0]*m2[2][1] + m1[2][0]*m2[2][2], \
          m1[0][1]*m2[2][0] + m1[1][1]*m2[2][1] + m1[2][1]*m2[2][2], \
          m1[0][2]*m2[2][0] + m1[1][2]*m2[2][1] + m1[2][2]*m2[2][2])
    return (c1,c2,c3)

def mattrans(m):
    "Transpose a 3x3 matrix."
    c1 = (m[0][0], m[1][0], m[2][0])
    c2 = (m[0][1], m[1][1], m[2][1])
    c3 = (m[0][2], m[1][2], m[2][2])
    return (c1,c2,c3)

def rotation(v,a):
    "3x3 matrix rotating by angle a about a 3-vector v."
    v = normalise(v)
    # Find one vector orthogonal to v, by crossing it with each of
    # three basis vectors and picking the one with the largest norm.
    v1 = cross(v,(1,0,0))
    v2 = cross(v,(0,1,0))
    v3 = cross(v,(0,0,1))
    if norm(v1) < norm(v2): v1 = v2
    if norm(v1) < norm(v3): v1 = v3
    v1 = normalise(v1)
    # Now find a second vector orthogonal to both.
    v2 = cross(v,v1)
    # Now we have a basis matrix.
    m = (v,v1,v2)

    # Now construct our basic rotation matrix.
    mr = ((1,0,0),(0,cos(a),-sin(a)),(0,sin(a),cos(a)))

    # And conjugate to get the matrix we really wanted.
    return matmul(m, matmul(mr, mattrans(m)))

def xform(m,v):
    "Transform vector v by matrix m."
    return (m[0][0]*v[0] + m[1][0]*v[1] + m[2][0]*v[2], \
            m[0][1]*v[0] + m[1][1]*v[1] + m[2][1]*v[2], \
            m[0][2]*v[0] + m[1][2]*v[1] + m[2][2]*v[2])

def compute_angles(pointlist):
    vs = map(normalise, pointlist)

    # Compute the face angles: each of these is just derived from
    # the dot product of two adjacent elements of vs.
    faceangles = []
    for i in range(len(vs)):
        v1, v2 = vs[i-1], vs[i]  # 0 -> -1 automatically wraps in Python
        faceangles.append(180/pi * acos(dot(v1,v2)))

    # Now the cut edge angle is 360 minus the sum of those angles.
    cutangle = 360
    for a in faceangles:
        cutangle = cutangle - a

    # Next, compute each dihedral angle. We do this by taking a set
    # of three adjacent vectors in the list and doing a projection
    # so as to remove any component along the middle one, so that
    # the outer two end up projected into a plane perpendicular to
    # the middle one. Then we measure the angle between them as
    # before.
    dihedrals = []
    for i in range(len(vs)):
        v1, v2, v3 = vs[i-2], vs[i-1], vs[i]  # wrapping happens again

        v1 = sub(v1, mult(v2, dot(v1,v2)))
        v3 = sub(v3, mult(v2, dot(v3,v2)))

        dihedrals.append(180/pi * acos(dot(v1,v3) / (norm(v1)*norm(v3))))

    return [faceangles, cutangle, dihedrals]

def display_angles(pointlist, names, filename):
    faceangles, cutangle, dihedrals = compute_angles(pointlist)

    # If given the `-p' command-line option, we output a polyhedron
    # description instead of this raw display.
    if len(sys.argv) > 1 and sys.argv[1] == "-p":
	f = open(filename, "w")

	# First, find a plausible vertical axis for the polyhedron,
	# by normalising the vectors and averaging the result.
	vsum = (0,0,0)
	for v in pointlist:
	    vsum = add(vsum, normalise(v))
	vsum = normalise(vsum)

	# Next, transform each edge vector so that its component in
	# the direction of vsum is the same.
	newpoints = []
	for v in pointlist:
	    component = dot(v, vsum)
	    v = mult(v, 1.0/component)
	    newpoints.append(v)

	# Now find a plausible centre point for the solid, by
	# adding up all those points and dividing by n+1 (so we
	# also `average' in the origin).
	centroid = (0,0,0)
	for v in newpoints:
	    centroid = add(v, centroid)
	centroid = mult(v, 1.0 / (1+len(pointlist)))

	# Now output the point coordinates.
	f.write("point apex %f %f %f\n" % mult(centroid,-1))
	for i in range(len(newpoints)):
	    v = newpoints[i]
	    f.write("point base%d %f %f %f\n" % ((i,) + sub(v, centroid)))

	# Next, write out each face description. First the base.
	for i in range(len(newpoints)-1,-1,-1):
	    f.write("face base base%d\n" % i)
	f.write("normal base %f %f %f\n" % vsum)

	for i in range(len(newpoints)):
	    j = (i+1) % len(newpoints)
	    facename = "face%dwith%d" % (i,j)
	    f.write("face "+facename+" apex\n")
	    f.write("face "+facename+" base%d\n" % i)
	    f.write("face "+facename+" base%d\n" % j)
	    # Now compute the face normal by taking the cross
	    # product of the vectors to those two points.
	    normal = normalise(cross(pointlist[i], pointlist[j]))
	    f.write("normal "+facename+" %f %f %f\n" % normal)

    else:
	for i in range(len(names)):
	    print names[i], "=", "%f %f %f" % pointlist[i]
	for i in range(len(names)):
	    name1, name2 = names[i-1], names[i]
	    print "Face angle " + name1 + "O" + name2 + " =", faceangles[i]
	print "Cut edge angle =", cutangle
	for i in range(len(names)):
	    name1, name2, name3 = names[i-2], names[i-1], names[i]
	    print "Dihedral angle of O" + name1 + name2 + \
	    " with O" + name2 + name3 + " =", dihedrals[i]

# One vertex of a regular icosahedron, and the five vertices
# surrounding it.

O = [ 0.0, -1.0, -1.6180339887498949 ]
A = sub([ -1.6180339887498949, 0.0, -1.0 ], O)
B = sub([ 0.0, 1.0, -1.6180339887498949 ], O)
C = sub([ 1.6180339887498949, 0.0, -1.0 ], O)
D = sub([ 1.0, -1.6180339887498949, 0.0 ], O)
E = sub([ -1.0, -1.6180339887498949, 0.0 ], O)

display_angles([A,B,C,D,E], ["A","B","C","D","E"], "dihedral1")

# Now transform this. We're going to:
#  - keep vector B fixed
#  - rotate A by x degrees towards B, keeping it in the plane OAB
#  - rotate C by x degrees towards B, keeping it in the plane OCB
#  - (thus preserving the dihedral angle between OAB and OCB)
#  - rotate E exactly as we rotated A (preserving the dihedral
#    between OAB and OAE)
#  - rotate D exactly as we rotated C (preserving the dihedral
#    between OCB and OCD)

angle1 = pi/20

# Determine the axis about which to rotate A and E: it's the cross
# product of B and A.
AEaxis = cross(B,A)
# Construct a rotation matrix about that axis.
AEmatrix = rotation(AEaxis, angle1)
# And rotate A and E via that matrix.
A = xform(AEmatrix, A)
E = xform(AEmatrix, E)
# Now do the same, rotating C and D about an axis which is the
# cross product of B and C.
CDaxis = cross(B,C)
CDmatrix = rotation(CDaxis, angle1)
C = xform(CDmatrix, C)
D = xform(CDmatrix, D)

# Now transform again. This time we're going to:
#  - rotate D away from C keeping it in the plane OCD
#  - rotate E away from A keeping it in the plane OEA
#  - thus frobbing the dihedral angles between the plane ODE and
#    each of the planes OAE and ODC
#  - while preserving the three other dihedrals.
#
# We choose the angle of rotation by binary search so as to end up
# with the same dihedral angles as the original icosahedron.

def attempt(angle2):
    Daxis = cross(D,C)
    Dmatrix = rotation(Daxis, angle2)
    Dprime = xform(Dmatrix, D)
    Eaxis = cross(E,A)
    Ematrix = rotation(Eaxis, angle2)
    Eprime = xform(Ematrix, E)
    a = compute_angles([A,B,C,Dprime,Eprime])
    return a[2][0] - a[2][1]

bot = pi/6
top = pi/4

assert attempt(top) > 0
assert attempt(bot) < 0

while top - bot > 1e-15:
    mid = (top + bot) / 2
    a = attempt(mid)
    if a > 0:
        top = mid
    else:
        bot = mid

angle2 = (top + bot) / 2

Daxis = cross(D,C)
Dmatrix = rotation(Daxis, angle2)
Dprime = xform(Dmatrix, D)
Eaxis = cross(E,A)
Ematrix = rotation(Eaxis, angle2)
Eprime = xform(Ematrix, E)

display_angles([A,B,C,Dprime,Eprime], ["A'","B'","C'","D'","E'"], "dihedral2")
