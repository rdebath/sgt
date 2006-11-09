#!/usr/bin/env python

# Owen Dunn once asked me what the relationship was between the
# dihedral angles around a polyhedron's vertex, and the `cut angle'
# you see on paper when you cut down one edge and open the vertex
# out on to a plane during the construction of a net. (In other
# words, the cut angle is 360 degrees minus the sum of all the
# interior angles meeting at the vertex.)
#
# The answer is, there is no fixed relationship; and to prove it, I
# used this program to construct a vertex at which five faces meet,
# all with the same dihedral angle as they do on the icosahedron,
# but with a cut angle which is not the same as the 60 degrees
# you'd find in an icosahedron net.
#
# When run, the program first prints the coordinates of five points
# A,B,C,D,E which between them give rise to an icosahedron-like
# vertex at the origin; then it computes and lists the face angles,
# the cut angle and the dihedral angles. Next it finds five
# different points A',B',C',D',E' and lists the same data; the face
# angles have all changed, the cut edge angle is different, but the
# dihedral angles are still the same. QED.

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

def display_angles(ret, names):
    faceangles, cutangle, dihedrals = ret
    for i in range(len(names)):
        name1, name2 = names[i-1], names[i]
        print "Face angle O" + name1 + name2 + " =", faceangles[i]
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

print "A =",A
print "B =",B
print "C =",C
print "D =",D
print "E =",E

display_angles(compute_angles([A,B,C,D,E]), ["A","B","C","D","E"])

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

print "A' =",A
print "B' =",B
print "C' =",C
print "D' =",Dprime
print "E' =",Eprime

display_angles(compute_angles([A,B,C,Dprime,Eprime]), ["A'","B'","C'","D'","E'"])
