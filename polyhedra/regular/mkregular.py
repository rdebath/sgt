#!/usr/bin/env python

# Generate polyhedron description files for the regular polyhedra.

import sys
import math

nvertices = 0
vertices = {}

def lettername(n):
    chars = 1
    while n >= 26L ** chars:
        n = n - 26L ** chars
        chars = chars + 1
    s = ""
    for i in range(chars):
        k = n % 26L
        n = n / 26L
        if i == chars-1:
            s = chr(65 + k) + s
        else:
            s = chr(97 + k) + s
    return s

def vertex(x, y, z):
    global nvertices
    vnum = len(vertices)
    vname = lettername(vnum)
    x, y, z = float(x), float(y), float(z)
    print "point", vname, x, y, z
    vertices[vname] = x, y, z
    return vname

def facelist(vlist):
    # Construct a face, given an ordered list of vertices. We
    # assume for these polyhedra that all faces point away from the
    # origin, which means we can auto-compute the surface normal
    # and automatically arrange for the face list to be the right
    # way round.
    vlist = list(tuple(vlist))
    vprod = (0,0,0)
    centroid = (0,0,0)
    for i in range(len(vlist)):
        # Compute an approximation to the surface normal given
        # the three vertices starting at this point.
        va = vertices[vlist[i]]
        vb = vertices[vlist[(i+1) % len(vlist)]]
        vc = vertices[vlist[(i+2) % len(vlist)]]
        dx1, dy1, dz1 = vb[0]-va[0], vb[1]-va[1], vb[2]-va[2]
        dx2, dy2, dz2 = vc[0]-vb[0], vc[1]-vb[1], vc[2]-vb[2]
        vp = (dy1*dz2-dz1*dy2, dz1*dx2-dx1*dz2, dx1*dy2-dy1*dx2)
        vprod = (vprod[0]+vp[0], vprod[1]+vp[1], vprod[2]+vp[2])
        centroid = (centroid[0]+va[0], centroid[1]+va[1], centroid[2]+va[2])
    # If the normal vector points inwards, invert it and reverse the list.
    if vprod[0]*centroid[0] + vprod[1]*centroid[1] + vprod[2]*centroid[2] < 0:
        vprod = (-vprod[0], -vprod[1], -vprod[2])
        vlist.reverse()
    # Normalise the normal vector to unit length.
    vplen = math.sqrt(vprod[0]**2 + vprod[1]**2 + vprod[2]**2)
    vprod = (vprod[0]/vplen, vprod[1]/vplen, vprod[2]/vplen)
    # Compute the face name.
    facename = ""
    for s in vlist:
        facename = facename + s
    # Output.
    for s in vlist:
        print "face", facename, s
    print "normal", facename, vprod[0], vprod[1], vprod[2]

def face(*list):
    facelist(list)

def tetrahedron():
    # Easiest way to define a tetrahedron is to take four vertices
    # from the cube.
    a = vertex(-1, -1, -1)
    b = vertex(-1, +1, +1)
    c = vertex(+1, +1, -1)
    d = vertex(+1, -1, +1)
    face(a, b, c)
    face(b, c, d)
    face(c, d, a)
    face(d, a, b)

def cube():
    a = vertex(-1, -1, -1)
    b = vertex(-1, -1, +1)
    c = vertex(-1, +1, -1)
    d = vertex(-1, +1, +1)
    e = vertex(+1, -1, -1)
    f = vertex(+1, -1, +1)
    g = vertex(+1, +1, -1)
    h = vertex(+1, +1, +1)
    face(a, b, d, c)
    face(e, f, h, g)
    face(a, c, g, e)
    face(b, d, h, f)
    face(a, e, f, b)
    face(c, g, h, d)

def octahedron():
    a = vertex(-1, 0, 0)
    b = vertex(+1, 0, 0)
    c = vertex(0, -1, 0)
    d = vertex(0, +1, 0)
    e = vertex(0, 0, -1)
    f = vertex(0, 0, +1)
    face(a, c, e)
    face(a, c, f)
    face(a, d, e)
    face(a, d, f)
    face(b, c, e)
    face(b, c, f)
    face(b, d, e)
    face(b, d, f)

def dodecahedron():
    # Simplest way to define a dodecahedron is to raise a roof on
    # each face of a cube. So we start with the cube vertices.
    a = vertex(-1, -1, -1)
    b = vertex(-1, -1, +1)
    c = vertex(-1, +1, -1)
    d = vertex(-1, +1, +1)
    e = vertex(+1, -1, -1)
    f = vertex(+1, -1, +1)
    g = vertex(+1, +1, -1)
    h = vertex(+1, +1, +1)
    # This cube has side length 2. So the side length of the
    # dodecahedron will be 2/phi.
    phi = (1.0 + math.sqrt(5))/2.0
    side = 2.0 / phi
    # For a given roof vertex, then...
    #
    #  +--------+
    #  |\      /|
    #  | \____/ |
    #  | /    \ |
    #  |/      \|
    #  +--------+
    #
    # ... the coordinates of the rightmost roof vertex are 0 in one
    # direction, side/2 in another, and z in a third, where z is 1
    # plus a length computed to make the diagonal roof edges equal
    # in length to side. Thus
    #
    #   1^2 + (1-side/2)^2 + (z-1)^2 == side^2
    z = 1 + math.sqrt(side**2 - (1-side/2)**2 - 1)
    y = side/2

    ab = vertex(-z, -y, 0)
    cd = vertex(-z, +y, 0)
    ef = vertex(+z, -y, 0)
    gh = vertex(+z, +y, 0)
    ae = vertex(0, -z, -y)
    bf = vertex(0, -z, +y)
    cg = vertex(0, +z, -y)
    dh = vertex(0, +z, +y)
    ac = vertex(-y, 0, -z)
    eg = vertex(+y, 0, -z)
    bd = vertex(-y, 0, +z)
    fh = vertex(+y, 0, +z)

    face(a, ab, b, bf, ae)
    face(e, ef, f, bf, ae)
    face(c, cd, d, dh, cg)
    face(g, gh, h, dh, cg)
    face(a, ac, c, cd, ab)
    face(b, bd, d, cd, ab)
    face(e, eg, g, gh, ef)
    face(f, fh, h, gh, ef)
    face(a, ae, e, eg, ac)
    face(c, cg, g, eg, ac)
    face(b, bf, f, fh, bd)
    face(d, dh, h, fh, bd)

def icosahedron():
    # The classical easy way to define an icosahedron in Cartesian
    # coordinates is to imagine three interlocking cards along the
    # three axes, each in golden ratio.
    phi = (1.0 + math.sqrt(5))/2.0

    a = vertex(0, -1, -phi)
    b = vertex(0, -1, +phi)
    c = vertex(0, +1, -phi)
    d = vertex(0, +1, +phi)

    e = vertex(-1, -phi, 0)
    f = vertex(-1, +phi, 0)
    g = vertex(+1, -phi, 0)
    h = vertex(+1, +phi, 0)

    i = vertex(-phi, 0, -1)
    j = vertex(+phi, 0, -1)
    k = vertex(-phi, 0, +1)
    l = vertex(+phi, 0, +1)

    # Now. We have a bunch of faces which use the short edge of one
    # of the cards.
    face(a, c, i)
    face(a, c, j)
    face(b, d, k)
    face(b, d, l)
    face(e, g, a)
    face(e, g, b)
    face(f, h, c)
    face(f, h, d)
    face(i, k, e)
    face(i, k, f)
    face(j, l, g)
    face(j, l, h)

    # And then we have eight more faces which each use one vertex
    # from each card.
    face(a, e, i)
    face(a, g, j)
    face(b, e, k)
    face(b, g, l)
    face(c, f, i)
    face(c, h, j)
    face(d, f, k)
    face(d, h, l)

polyhedra = {
"tetrahedron": tetrahedron,
"cube": cube,
"octahedron": octahedron,
"dodecahedron": dodecahedron,
"icosahedron": icosahedron,
}

args = sys.argv[1:]

if len(args) != 1 or not polyhedra.has_key(args[0]):
    sys.stderr.write("usage: mkregular.py <solid-name>\n")
    list = polyhedra.keys()
    list.sort()
    sys.stderr.write("valid solid names:\n")
    for i in list:
        sys.stderr.write("    " + i + "\n")
    sys.exit(len(args) > 0)

polyhedra[args[0]]()
