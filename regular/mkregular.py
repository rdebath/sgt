#!/usr/bin/env python

# Generate polyhedron description files for the regular polyhedra.

import sys
import math

class polyhedron:
    def __init__(self):
	self.vertices = {}
	self.vlist = []
	self.faces = {}
	self.normals = {}
	self.flist = []

    def vertex(self, x, y, z):
	vnum = len(self.vertices)
	vname = lettername(vnum)
	x, y, z = float(x), float(y), float(z)
	self.vertices[vname] = x, y, z
	self.vlist.append(vname)
	return vname

    def facelist(self, vlist):
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
            va = self.vertices[vlist[i]]
            vb = self.vertices[vlist[(i+1) % len(vlist)]]
            vc = self.vertices[vlist[(i+2) % len(vlist)]]
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
	self.faces[facename] = vlist
	self.normals[facename] = vprod
	self.flist.append(facename)

    def face(self, *list):
	self.facelist(list)

    def output(self, file):
	for v in self.vlist:
	    x, y, z = self.vertices[v]
	    file.write("point "+v+" "+repr(x)+" "+repr(y)+" "+repr(z)+"\n")
	for f in self.flist:
	    vlist = self.faces[f]
	    for s in vlist:
		file.write("face " + f + " " + s + "\n")
	    x, y, z = self.normals[f]
	    file.write("normal "+f+" "+repr(x)+" "+repr(y)+" "+repr(z)+"\n")

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


def tetrahedron():
    p = polyhedron()
    # Easiest way to define a tetrahedron is to take four vertices
    # from the cube.
    a = p.vertex(-1, -1, -1)
    b = p.vertex(-1, +1, +1)
    c = p.vertex(+1, +1, -1)
    d = p.vertex(+1, -1, +1)
    p.face(a, b, c)
    p.face(b, c, d)
    p.face(c, d, a)
    p.face(d, a, b)
    return p

def cube():
    p = polyhedron()
    a = p.vertex(-1, -1, -1)
    b = p.vertex(-1, -1, +1)
    c = p.vertex(-1, +1, -1)
    d = p.vertex(-1, +1, +1)
    e = p.vertex(+1, -1, -1)
    f = p.vertex(+1, -1, +1)
    g = p.vertex(+1, +1, -1)
    h = p.vertex(+1, +1, +1)
    p.face(a, b, d, c)
    p.face(e, f, h, g)
    p.face(a, c, g, e)
    p.face(b, d, h, f)
    p.face(a, e, f, b)
    p.face(c, g, h, d)
    return p

def octahedron():
    p = polyhedron()
    a = p.vertex(-1, 0, 0)
    b = p.vertex(+1, 0, 0)
    c = p.vertex(0, -1, 0)
    d = p.vertex(0, +1, 0)
    e = p.vertex(0, 0, -1)
    f = p.vertex(0, 0, +1)
    p.face(a, c, e)
    p.face(a, c, f)
    p.face(a, d, e)
    p.face(a, d, f)
    p.face(b, c, e)
    p.face(b, c, f)
    p.face(b, d, e)
    p.face(b, d, f)
    return p

def dodecahedron():
    p = polyhedron()
    # Simplest way to define a dodecahedron is to raise a roof on
    # each face of a cube. So we start with the cube vertices.
    a = p.vertex(-1, -1, -1)
    b = p.vertex(-1, -1, +1)
    c = p.vertex(-1, +1, -1)
    d = p.vertex(-1, +1, +1)
    e = p.vertex(+1, -1, -1)
    f = p.vertex(+1, -1, +1)
    g = p.vertex(+1, +1, -1)
    h = p.vertex(+1, +1, +1)
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

    ab = p.vertex(-z, -y, 0)
    cd = p.vertex(-z, +y, 0)
    ef = p.vertex(+z, -y, 0)
    gh = p.vertex(+z, +y, 0)
    ae = p.vertex(0, -z, -y)
    bf = p.vertex(0, -z, +y)
    cg = p.vertex(0, +z, -y)
    dh = p.vertex(0, +z, +y)
    ac = p.vertex(-y, 0, -z)
    eg = p.vertex(+y, 0, -z)
    bd = p.vertex(-y, 0, +z)
    fh = p.vertex(+y, 0, +z)

    p.face(a, ab, b, bf, ae)
    p.face(e, ef, f, bf, ae)
    p.face(c, cd, d, dh, cg)
    p.face(g, gh, h, dh, cg)
    p.face(a, ac, c, cd, ab)
    p.face(b, bd, d, cd, ab)
    p.face(e, eg, g, gh, ef)
    p.face(f, fh, h, gh, ef)
    p.face(a, ae, e, eg, ac)
    p.face(c, cg, g, eg, ac)
    p.face(b, bf, f, fh, bd)
    p.face(d, dh, h, fh, bd)

    return p

def icosahedron():
    p = polyhedron()
    # The classical easy way to define an icosahedron in Cartesian
    # coordinates is to imagine three interlocking cards along the
    # three axes, each in golden ratio.
    phi = (1.0 + math.sqrt(5))/2.0

    a = p.vertex(0, -1, -phi)
    b = p.vertex(0, -1, +phi)
    c = p.vertex(0, +1, -phi)
    d = p.vertex(0, +1, +phi)

    e = p.vertex(-1, -phi, 0)
    f = p.vertex(-1, +phi, 0)
    g = p.vertex(+1, -phi, 0)
    h = p.vertex(+1, +phi, 0)

    i = p.vertex(-phi, 0, -1)
    j = p.vertex(+phi, 0, -1)
    k = p.vertex(-phi, 0, +1)
    l = p.vertex(+phi, 0, +1)

    # Now. We have a bunch of faces which use the short edge of one
    # of the cards.
    p.face(a, c, i)
    p.face(a, c, j)
    p.face(b, d, k)
    p.face(b, d, l)
    p.face(e, g, a)
    p.face(e, g, b)
    p.face(f, h, c)
    p.face(f, h, d)
    p.face(i, k, e)
    p.face(i, k, f)
    p.face(j, l, g)
    p.face(j, l, h)

    # And then we have eight more faces which each use one vertex
    # from each card.
    p.face(a, e, i)
    p.face(a, g, j)
    p.face(b, e, k)
    p.face(b, g, l)
    p.face(c, f, i)
    p.face(c, h, j)
    p.face(d, f, k)
    p.face(d, h, l)

    return p

def edges(p):
    # Return a list of all the edges of a polyhedron, in the form
    # of a list of 2-tuples containing vertex names.
    elist = []
    for f in p.flist:
	vl = p.faces[f]
	for i in range(len(vl)):
	    v1, v2 = vl[i-1], vl[i]  # i==0 neatly causes wraparound
	    if v1 < v2: # only add each edge once
		elist.append((v1,v2))
    return elist

class edgedual:
    # This wrapper class takes as input two functions returning
    # dual polyhedron objects, and implements a callable object
    # which returns the edge dual of those polyhedra.
    def __init__(self, p1, p2):
	self.p1 = p1
	self.p2 = p2
    def __call__(self):
	p1 = self.p1()
	p2 = self.p2()
	pout = polyhedron()
	# Find the edges of each.
	e1 = edges(p1)
	e2 = edges(p2)
	# For each polyhedron, find the radius vectors from the
	# origin to the middle of each edge. As a by-produt of this
	# phase, we also find the scale factor which normalises
	# each polyhedron to the size where those radii have length
	# 1.
	r1 = {}
	r2 = {}
	for p, e, r in (p1,e1,r1),(p2,e2,r2):
	    sl = sn = 0
	    for v1, v2 in e:
		x1, y1, z1 = p.vertices[v1]
		x2, y2, z2 = p.vertices[v2]
		x, y, z = (x1+x2)/2, (y1+y2)/2, (z1+z2)/2
		l = math.sqrt(x*x+y*y+z*z)
		r[(v1,v2)] = x, y, z
		sl = sl + l
		sn = sn + 1
	    r[None] = sn / sl
	# Now match up the edges between the two polyhedra. As a
	# test of duality, we deliberately check that the mapping
	# we end up with is a bijection.
	emap = {}
	scale1 = r1[None]
	scale2 = r2[None]
	for e in e2:
	    n = r2[e]
	    n = (n[0]*scale2, n[1]*scale2, n[2]*scale2)
	    best = None
	    bestdist = None
	    for eprime in e1:
		nprime = r1[eprime]
		nprime = (nprime[0]*scale2, nprime[1]*scale2, nprime[2]*scale2)
		dist = (nprime[0]-n[0])**2 + \
		(nprime[1]-n[1])**2 + (nprime[2]-n[2])**2
		if bestdist == None or dist < bestdist:
		    bestdist = dist
		    best = eprime
	    # Ensure the function from e2 to e1 is injective, by
	    # making sure we never overwrite an entry in emap.
	    assert(not emap.has_key(best))
	    emap[best] = e
	# Ensure the function from e2 to e1 is surjective, by
	# making sure everything in e1 is covered.
	assert len(emap) == len(e1)

	# Output the edge dual's vertices.
	vmap1 = {}
	vmap2 = {}
	for p, r, vmap in (p1,r1,vmap1),(p2,r2,vmap2):
	    scale = r[None]
	    for v in p.vlist:
		x, y, z = p.vertices[v]
		vmap[v] = pout.vertex(x*scale, y*scale, z*scale)

	# And output the faces.
	for e1, e2 in emap.items():
	    pout.face(vmap1[e1[0]], vmap2[e2[0]], vmap1[e1[1]], vmap2[e2[1]])

	return pout

class output:
    # This wrapper class takes a function returning a polyhedron
    # object, and implements a callable object which outputs that
    # polyhedron to stdout.
    def __init__(self, fn):
	self.fn = fn
    def __call__(self, file=sys.stdout):
	p = self.fn()
	p.output(file)

def all():
    for name, fn in polyhedra.items():
	if name == "all":
	    continue
	file = open(name, "w")
	fn(file)
	file.close()

polyhedra = {
"tetrahedron": output(tetrahedron),
"cube": output(cube),
"octahedron": output(octahedron),
"dodecahedron": output(dodecahedron),
"icosahedron": output(icosahedron),
"rhombicdodecahedron": output(edgedual(cube, octahedron)),
"rhombictriacontahedron": output(edgedual(dodecahedron, icosahedron)),
"all": all,
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
