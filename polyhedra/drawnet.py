#!/usr/bin/env python

# Read in a polyhedron description, and draw a net in PostScript.

import sys
import string
import random
from math import pi, asin, atan2, cos, sin, sqrt
from crosspoint import crosspoint
import sph

args = sys.argv[1:]

firstface = None
facelabels = 0
cmpsign = +1
picture = None
adjpairs = {}
while len(args) > 0 and args[0][:1] == "-":
    a = args[0]
    args = args[1:]

    if a == "--":
	break
    elif a[:2] == "-s":
	firstface = a[2:]
    elif a == "-R":
	cmpsign = -1
    elif a == "-f":
	facelabels = 1
    elif a[:2] == "-p":
        # Undocumented option which allows you to specify a file
        # containing a spherical image file. This allows a net to
        # be drawn which folds up to produce a polyhedron covered
        # in a projection of the spherical image (e.g. an
        # icosahedral globe).
        picture = sph.SphericalPic(a[2:])
    elif a[:2] == "-a":
        # Undocumented option which attempts to force a pair of
        # faces to be placed adjacent to one another in the net.
        # Expects two face names separated by a colon.
        s = a[2:]
        comma = string.find(s, ",")
        if comma <= 0 or comma == len(s)-1:
            sys.stderr.write("-a option expects two face names separated"+\
            " by a comma\n")
        else:
            f1 = s[:comma]
            f2 = s[comma+1:]
            adjpairs[f1] = adjpairs.get(f1, ()) + (f2,)
            adjpairs[f2] = adjpairs.get(f2, ()) + (f1,)
    else:
	sys.stderr.write("ignoring unknown option \"%s\"\n", a)

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

def realprint(a, outfile=outfile):
    for i in range(len(a)):
	outfile.write(str(a[i]))
	if i < len(a)-1:
	    outfile.write(" ")
	else:
	    outfile.write("\n")

def psprint(*a):
    realprint(a)

def debug(*a):
    realprint(a, sys.stderr)

def rotatetotop(x, y, z):
    # Return an orthogonal matrix which rotates a given point to
    # the positive z-axis. (Will fail if passed 0,0,0!)
    d = sqrt(x**2+y**2+z**2)
    x = x/d
    y = y/d
    z = z/d
    # We first find the point's polar coordinates...
    theta = atan2(y, x)
    phi = asin(z)
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
    matrix = [
    [ cos(theta)*sin(phi),  sin(theta)*sin(phi),  -cos(phi) ],
    [     -sin(theta)    ,      cos(theta)     ,      0     ],
    [ cos(theta)*cos(phi),  sin(theta)*cos(phi),   sin(phi) ]]
    return matrix

def zrotate(theta):
    matrix = [
    [ cos(theta), -sin(theta), 0 ],
    [ sin(theta),  cos(theta), 0 ],
    [      0    ,      0     , 1 ]]
    return matrix

def matrixmult(m2, m1):
    matrix = []
    for y in range(3):
	mrow = []
	for x in range(3):
	    s = 0
	    for k in range(3):
		s = s + m2[y][k] * m1[k][x]
	    mrow.append(s)
	matrix.append(mrow)
    return matrix

def transform(matrix, xa, ya, za):
    xb = matrix[0][0] * xa + matrix[0][1] * ya + matrix[0][2] * za
    yb = matrix[1][0] * xa + matrix[1][1] * ya + matrix[1][2] * za
    zb = matrix[2][0] * xa + matrix[2][1] * ya + matrix[2][2] * za
    return xb, yb, zb

# Container class.
class struct:
    pass

# For each face, we store a load of data in facepos[face] about
# where the face has been positioned.
#  - facepos[face].face equals face. Handy back-reference.
#  - facepos[face].matrix is the matrix which rotates the
#    polyhedron so that the x and y components of the face's
#    vertices represent how it is displayed on the plane.
#  - facepos[face].pos is an (x,y) tuple indicating how far the
#    rotated polyhedron is translated to reach its final position.
#  - facepos[face].vpos is a hash mapping vertices to (x,y)
#    positions.
#  - facepos[face].bbox is an (xmin,ymin,xmax,ymax) 4-tuple.
#  - facepos[face].adjacent is a hash mapping other faces to
#    further container classes containing the above `matrix',
#    `pos', `vpos' and `bbox' elements.
#  - facepos[face].z is the perpendicular distance from the origin
#    to this face.
facepos = {}

def do_vpos_bbox(placement, face):
    # Find the position of each vertex in face f.
    vpos = {}
    xmin = ymin = xmax = ymax = None
    zsum, zn = 0, 0
    for v in faces[face]:
	xa, ya, za = vertices[v]
	xb, yb, zb = transform(placement.matrix, xa, ya, za)
	xb = xb + placement.pos[0]
	yb = yb + placement.pos[1]
	vpos[v] = (xb, yb)
	zsum = zsum + zb
	zn = zn + 1
	if xmin == None or xmin > xb: xmin = xb
	if ymin == None or ymin > yb: ymin = yb
	if xmax == None or xmax < xb: xmax = xb
	if ymax == None or ymax < yb: ymax = yb
    placement.vpos = vpos
    placement.bbox = (xmin, ymin, xmax, ymax)
    placement.z = zsum / zn

# Go through the edges of each face and build up some quick
# reference hashes: one mapping each face to a list of edges, the
# other mapping each edge to the face on its left. (This means we
# represent each edge once in each direction.)
faceedges = {}
edgeface = {}
for face, vlist in faces.items():
    elist = []
    for i in range(len(vlist)):
	elist.append((vlist[i-1], vlist[i])) # Python makes [-1],[0] work :-)
    faceedges[face] = elist
    for edge in elist:
	assert not edgeface.has_key(edge)
	edgeface[edge] = face

# This hash stores a list of unplaced faces.
unplaced = {}
for i in faces.keys():
    unplaced[i] = 1

# Pick an arbitrary face to start off with. Rotate so that its
# normal vector points upwards, and position it at the origin.
if firstface != None and not faces.has_key(firstface):
    sys.stderr.write("supplied initial face name does not exist\n");
    firstface = None
if firstface == None:
    firstface = faces.keys()[0]
nx, ny, nz = normals[firstface]
facepos[firstface] = struct()
facepos[firstface].matrix = rotatetotop(nx, ny, nz)
facepos[firstface].pos = (0,0)
facepos[firstface].face = firstface
do_vpos_bbox(facepos[firstface], firstface)
#debug("initial", firstface, facepos[firstface].vpos)
f = firstface

# List of possible places to put next face. Each one is a tuple of
# (face-to-be-placed, already-placed-face-to-put-it-next-to).
nextface = []

# List of folded edges. (In a net, every edge is either folded,
# meaning that the two faces it separates are adjacent on the
# paper, or cut, meaning that the two faces are some distance apart
# and only become adjacent when the net is assembled.)
folded = []

# The set of _leaf_ faces (i.e. faces at the fringes of the net,
# connected to only one other face by a folded edge). We start this
# off as the complete set of faces, and knock a face off it every
# time we use it as the starting point for placing another face.
#
# Special case: the _first_ time we place a face next to the
# starting face, we don't tag it as non-leaf!
leaffaces = {}
for ff in faces.keys(): leaffaces[ff] = 1

# Loop round placing each face.
while 1:
    #debug("placed", f, "at", facepos[f].pos)

    # We have just placed face f. Mark it as placed.
    del unplaced[f]

    # Figure out where each adjacent face would go if we placed it
    # beside f. Form a list of possible next face placements.
    facepos[f].adjacent = {}
    for e in faceedges[f]:
	e2 = (e[1],e[0])  # the inverse of the edge
	f2 = edgeface[e2] # the face sharing that edge
	# First rotate the normal of f2 to the top.
	nx, ny, nz = normals[f2]
	matrix2 = rotatetotop(nx, ny, nz)
	# Now transform the edge vector, and figure out how much to
	# rotate it about the z-axis to bring it into alignment
	# with the same edge on the face we've just placed.
	xa1, ya1, za1 = vertices[e[0]]
	xb1, yb1, zb1 = transform(matrix2, xa1, ya1, za1)
	xa2, ya2, za2 = vertices[e[1]]
	xb2, yb2, zb2 = transform(matrix2, xa2, ya2, za2)
	dx1 = facepos[f].vpos[e[0]][0] - facepos[f].vpos[e[1]][0]
	dy1 = facepos[f].vpos[e[0]][1] - facepos[f].vpos[e[1]][1]
	dx2 = xb1 - xb2
	dy2 = yb1 - yb2
	theta1 = atan2(dy1, dx1)
	theta2 = atan2(dy2, dx2)
	matrix3 = zrotate(theta1 - theta2)
	matrix = matrixmult(matrix3, matrix2)
	# Now transform the two vertices in question using the new matrix.
	xc1, yc1, zc1 = transform(matrix, xa1, ya1, za1)
	xc2, yc2, zc2 = transform(matrix, xa2, ya2, za2)
	# (Verify that they are what they should be.)
	#debug(dx1, xc1-xc2  # should be equal)
	#debug(dy1, yc1-yc2  # these should be equal too)
        # Store the height of the face for convenience.
        facepos[f].height = (zc1 + zc2) / 2
	# Now figure out where to translate this face to so that
	# this edge is brought into exact conjunction with the
	# corresponding edge on face f.
	vx1 = facepos[f].vpos[e[0]][0] - xc1
	vx2 = facepos[f].vpos[e[1]][0] - xc2
	vx = (vx1 + vx2) / 2
	vy1 = facepos[f].vpos[e[0]][1] - yc1
	vy2 = facepos[f].vpos[e[1]][1] - yc2
	vy = (vy1 + vy2) / 2
	# Store the matrix and translation data, so that if we do
	# decide to place this face here we don't have to recompute
	# everything.
	facepos[f].adjacent[f2] = struct()
	facepos[f].adjacent[f2].matrix = matrix
	facepos[f].adjacent[f2].pos = (vx, vy)
	facepos[f].adjacent[f2].face = f2
	# Generate vpos and bbox.
	do_vpos_bbox(facepos[f].adjacent[f2], f2)
	#debug("possible", f2, "adjoining", f, facepos[f].adjacent[f2].vpos)
	# Add this to the list of possible next-face placements.
	nextface.append((f2,f))

    # Now we've finished with f, and can reuse the variable.

    # Go through the possible next-face placements and weed out any
    # which overlap an already-placed face, or which _are_ an
    # already-placed face.
    #debug(nextface)
    nfnew = []
    for n, p in nextface:
	if not unplaced.has_key(n): continue
	# For each face, go through all the already-placed faces
	# and see if there's an overlap.
	#debug("testing", n, "adjoining", p)
	placement = facepos[p].adjacent[n]
	overlap = 0
	for f, fplace in facepos.items():
	    # Special case: n and p cannot overlap, although
	    # they're depressingly likely to look as if they do.
	    if f == p: continue
	    
	    # Two faces definitely don't overlap if their bounding
	    # boxes are non-overlapping.
	    if placement.bbox[3] < fplace.bbox[1]: continue
	    if placement.bbox[1] > fplace.bbox[3]: continue
	    if placement.bbox[2] < fplace.bbox[0]: continue
	    if placement.bbox[0] > fplace.bbox[2]: continue
	    #debug("testing overlap between", n, "and", f)

	    # Now we have to figure out whether the polygons
	    # actually intersect. I reckon this will be the case
	    # iff one of two conditions holds: (a) an edge of
	    # polygon A intersects an edge of B, or (b) the
	    # epicentre of one polygon is within the other.
	    for vl1, vl2 in [(placement,fplace), (fplace,placement)]:
		for e1 in faceedges[vl1.face]:
		    # Special case again: if either edge ends at a
		    # vertex of n, an intersection will look very
		    # likely but must actually be ignored.
		    if vl1.face != placement.face and \
		    placement.vpos.has_key(e1[0]): break
		    if vl1.face != placement.face and \
		    placement.vpos.has_key(e1[1]): break
		    for e2 in faceedges[vl2.face]:
			if vl2.face != placement.face and \
			placement.vpos.has_key(e2[0]): break
			if vl2.face != placement.face and \
			placement.vpos.has_key(e2[1]): break
			xa1, ya1 = vl1.vpos[e1[0]]
			xa2, ya2 = vl1.vpos[e1[1]]
			xb1, yb1 = vl2.vpos[e2[0]]
			xb2, yb2 = vl2.vpos[e2[1]]
			ret = crosspoint(xa1,ya1,xa2,ya2,xb1,yb1,xb2,yb2)
			if ret == None: continue
			x, y = ret
			dxa, dya = xa2-xa1, ya2-ya1
			dxb, dyb = xb2-xb1, yb2-yb1
			# See if the crossing point is between the
			# ends of each line. This will be true if
			# the dot product (x-xa1,y-ya1).(dxa,dya)
			# divided by the squared length
			# (dxa,dya).(dxa,dya) is strictly between 0
			# and 1. Likewise for b1/b2.
			dp = ((x-xa1)*dxa+(y-ya1)*dya) / (dxa**2 + dya**2)
			if dp < 0 or dp > 1: continue
			dp = ((x-xb1)*dxb+(y-yb1)*dyb) / (dxb**2 + dyb**2)
			if dp < 0 or dp > 1: continue
			# We have an intersection.
			#debug(vl1.face, e1[0], xa1,ya1, e1[1], xa2,ya2)
			#debug(vl2.face, e2[0], xb1,yb1, e2[1], xb2,yb2)
			#debug(x, y)
			#debug(vl1.face, vl2.face, "intersect!")
			overlap = 1
			break
		    if overlap: break
		if overlap: break
		# Now try the epicentre trick, using winding
		# numbers.
		ex = ey = 0
		for x, y in vl1.vpos.values():
		    ex = ex + x
		    ey = ey + y
		ex = ex / len(vl1.vpos)
		ey = ey / len(vl1.vpos)
		winding = 0
		rx = vl1.bbox[2]-vl1.bbox[0] + vl2.bbox[2]-vl2.bbox[0]
		for e2 in faceedges[vl2.face]:
		    xb1, yb1 = vl2.vpos[e2[0]]
		    xb2, yb2 = vl2.vpos[e2[1]]
		    if not ((yb1<=ey and yb2>ey) or (yb1>ey and yb2<=ey)):
			continue # this edge is uninteresting
		    ret = crosspoint(ex,ey,ex+rx,ey,xb1,yb1,xb2,yb2)
		    if ret != None and ret[1] > ex:
			if yb1 <= ey:
			    winding = winding + 1
			else:
			    winding = winding - 1
		if winding:
		    #debug("winding!")
		    overlap = 1
		    break
	    if overlap: break
	if not overlap:
	    nfnew.append((n,p))
    nextface = nfnew
    #debug(nextface)

    # Select one out of the remainder, and go back round the loop.
    # Simplest thing here is to pick the one with the smallest
    # |pos|, to encourage the net to be roughly circular in shape.
    #
    # We also process the adjpairs data here. We tag each possible
    # placement (which itself is a pair of adjacent faces) as Yes,
    # No or Maybe. `Yes' means this face pair is explicitly listed
    # in adjpairs; `No' means this face pair _rules out_ one in
    # adjpairs (i.e. that there is a face already placed which the
    # user wants this one placed next to); `Maybe' means neither of
    # these things is the case. Then we place a Yes pair if
    # possible, a Maybe pair failing that, and if we really have
    # nothing but No pairs left to place then we place a No pair
    # and give a warning.
    bestscore = None
    best = None
    for n, p in nextface:
        brokenrule = None
        if p in adjpairs.get(n, ()):
            status = 2 # YES
        else:
            # Go through all the faces which n is requested to be
            # next to. If any is already placed (and isn't p, but
            # the above test has ruled that out already), our
            # status is No, otherwise it's Maybe.
            status = 1 # MAYBE
            for ff in adjpairs.get(n, ()):
                if facepos.has_key(ff):
                    status = 0 # NO
                    brokenrule = (n, ff)
                    break
	placement = facepos[p].adjacent[n]
	dist2 = placement.pos[0] ** 2 + placement.pos[1] ** 2
        score = (status, dist2 * -cmpsign, brokenrule)
	if bestscore == None or score > bestscore:
            bestscore = score
	    best = n, p
    if best == None:
	#debug("run out of faces to place")
	break
    if bestscore[2] != None:
        debug("!!! unable to place faces", bestscore[2][0], "and", \
        bestscore[2][1], "adjacent");
    n, p = best
    facepos[n] = facepos[p].adjacent[n]
    f = n
    folded.append((n,p))
    if leaffaces.has_key(p):
	if p == firstface:
	    firstface = None
	else:
	    del leaffaces[p]

if len(unplaced) > 0:
    debug("!!!", len(unplaced), "faces still unplaced!")

# At this stage every face has been placed. However, pairs of
# adjacent faces which share vertices will each contain their own
# version of the coordinates of those vertices, so now we go
# through and do a merge pass where we canonicalise all subtly
# different versions of the coordinates of the same point into
# _really_ the same thing. To do this we construct a new hash
# mapping arbitrarily chosen vertex IDs into coordinates, and each
# placement structure in facepos[] acquires a new member `vid'
# giving the vertex ID of each.
vids = {}
vaux = {}
currvid = 0
for face, placement in facepos.items():
    placement.vid = {}
    for v in faces[face]:
	vids[currvid] = placement.vpos[v]
	vaux[currvid] = (currvid, 1)
	placement.vid[v] = currvid
	currvid = currvid + 1
# Now we've done that, we go through the list of folded edges
# (edges joining two faces on the net) and merge the vertices which
# are common between the two faces sharing that edge.
foldededges = []
for f1, f2 in folded:
    edge = ()
    for v in faces[f1]:
	if not (v in faces[f2]): continue
	vid1 = facepos[f1].vid[v]
	while vaux[vid1][0] != vid1:
	    vid1 = vaux[vid1][0]
	vid2 = facepos[f2].vid[v]
	while vaux[vid2][0] != vid2:
	    vid2 = vaux[vid2][0]
	sx = vids[vid1][0]*vaux[vid1][1] + vids[vid2][0]*vaux[vid2][1]
	sy = vids[vid1][1]*vaux[vid1][1] + vids[vid2][1]*vaux[vid2][1]
	sn = vaux[vid1][1] + vaux[vid2][1]
	vids[currvid] = (sx/sn, sy/sn)
	vaux[currvid] = (currvid, sn)
	vaux[vid1] = (currvid, None)
	vaux[vid2] = (currvid, None)
	if vids.has_key(vid1): del vids[vid1]
	if vids.has_key(vid2): del vids[vid2]
	edge = edge + (currvid,)
	currvid = currvid + 1
    foldededges.append(edge)
    assert len(edge) == 2

for face, placement in facepos.items():
    for v in faces[face]:
	vv = placement.vid[v]
	while vaux[vv][0] != vv:
	    vv = vaux[vv][0]
	placement.vid[v] = vv

isfolded = {}
for i in range(len(foldededges)):
    repl = ()
    for vx in foldededges[i]:
	vv = vx
	while vaux[vv][0] != vv:
	    vv = vaux[vv][0]
	repl = repl + (vv,)
    foldededges[i] = repl
    isfolded[repl] = isfolded[(repl[1],repl[0])] = 1

# Put together a single list of cut edges going right round the
# net.
graph = {} # maps vid to list of vids
for face, placement in facepos.items():
    for e in faceedges[face]:
	vid1 = placement.vid[e[0]]
	vid2 = placement.vid[e[1]]
	if isfolded.get((vid1,vid2), 0): continue
	graph[vid1] = graph.get(vid1, []) + [vid2]
	graph[vid2] = graph.get(vid2, []) + [vid1]
# Now go through the graph starting at an arbitrary point.
startvid = vid = graph.keys()[0]
outline = []
while 1:
    outline.append(vid)
    adj = graph[vid]
    del graph[vid]
    if len(outline) > 1 and outline[-2] == adj[0]:
	vid = adj[1]
    else:
	vid = adj[0]
    if vid == startvid:
	break
assert len(graph) == 0

# Begin to analyse tab placement. First, just identify all the cut
# edges.
cutedges = {}
for v1, v2 in edgeface.keys():
    if v1 > v2: continue  # get each edge in a single canonical form
    cutedges[(v1,v2)] = 1
for f1, f2 in folded:
    edge = ()
    for v in faces[f1]:
	if not (v in faces[f2]): continue
	edge = edge + (v,)
    assert len(edge) == 2
    if edge[1] < edge[0]: edge = (edge[1],edge[0])
    del cutedges[edge]
# Initialise our decision hash that tells us which of the two
# instances of a cut edge has the tab on it.
tabpos = {}
for e in cutedges.keys(): tabpos[e] = None
# To decide where to place tabs: first loop over each leaf face, in
# decreasing order of distance from the centre of the net's
# approximate bounding box, and try to arrange for it to be tab-free.
xmin = ymin = xmax = ymax = None
for face, placement in facepos.items():
    if xmin == None or xmin > placement.bbox[0]: xmin = placement.bbox[0]
    if xmax == None or xmax < placement.bbox[2]: xmax = placement.bbox[2]
    if ymin == None or ymin > placement.bbox[1]: ymin = placement.bbox[1]
    if ymax == None or ymax < placement.bbox[3]: ymax = placement.bbox[3]
xcentre = (xmax + xmin) / 2
ycentre = (ymax + ymin) / 2
leaflist = leaffaces.keys()[:]
def cmpfn(f1, f2):
    d1 = sqrt((facepos[f1].pos[0]-xcentre)**2+(facepos[f1].pos[1]-ycentre)**2)
    d2 = sqrt((facepos[f2].pos[0]-xcentre)**2+(facepos[f2].pos[1]-ycentre)**2)
    if d1 < d2:
	return +1
    elif d1 > d2:
	return -1
    else:
	return 0
leaflist.sort(cmpfn)
del cmpfn

for f in leaflist:
    cuts = []
    canbetabfree = 1
    for v1, v2 in faceedges[f]:
	if v2 > v1:
	    e = (v1,v2)
	else:
	    e = (v2,v1)
	if not cutedges.has_key(e): continue
	cuts.append(((v2,v1),e))
	if tabpos[e] == (v1,v2):
	    canbetabfree = 0
	    break
    if not canbetabfree:
	continue
    for ethere, e in cuts:
	tabpos[e] = ethere
# Now loop over each remaining cut edge and figure out whether to
# put a tab on it. I don't see any particularly helpful way to make
# this decision, so for the moment I'll just put the tab closer to
# the origin in all cases.
for v1, v2 in cutedges.keys():
    if tabpos[(v1,v2)] != None: continue # done this edge already
    f1 = edgeface[(v1,v2)]
    f2 = edgeface[(v2,v1)]
    ex1 = (vids[facepos[f1].vid[v1]][0] + vids[facepos[f1].vid[v2]][0]) / 2
    ey1 = (vids[facepos[f1].vid[v1]][1] + vids[facepos[f1].vid[v2]][1]) / 2
    ex2 = (vids[facepos[f2].vid[v1]][0] + vids[facepos[f2].vid[v2]][0]) / 2
    ey2 = (vids[facepos[f2].vid[v1]][1] + vids[facepos[f2].vid[v2]][1]) / 2
    if ex1**2+ey1**2 < ex2**2+ey2**2:
	tabpos[(v1,v2)] = (v1,v2)
    else:
	tabpos[(v1,v2)] = (v2,v1)

# Now we have the locations of all the tabs, we need to figure out
# what shape to make them. This is downright fiddly and
# aggravating.
#
# Preliminary step: concoct a complete list of which vids are
# connected together in the basic net.
vidconn = {} # maps one vid to list of other vids
for vid1, vid2 in foldededges:
    vidconn[vid1] = vidconn.get(vid1, []) + [vid2]
    vidconn[vid2] = vidconn.get(vid2, []) + [vid1]
vid1 = outline[-1]
for vid2 in outline:
    vidconn[vid1] = vidconn.get(vid1, []) + [vid2]
    vidconn[vid2] = vidconn.get(vid2, []) + [vid1]
    vid1 = vid2
tabpoints = {} # maps (v1,v2) to a list of in-between vertices for the tab
tabaxes = {} # maps (v1,v2) to a pair of unit vectors, along and across the tab
for v1, v2 in tabpos.values():
    f1 = edgeface[(v1,v2)] # the face on which we are placing the tab
    f2 = edgeface[(v2,v1)] # the face the tab will end up glued to by a human
    # Our original polyhedron description should have been set up
    # with all edges going anticlockwise around the described face.
    # Here's our chance to prove it! Rotate the edge 90 degrees
    # clockwise to produce a vector pointing out of f1 into f2.
    vid1 = facepos[f1].vid[v1]
    vid2 = facepos[f1].vid[v2]
    dx = vids[vid2][0] - vids[vid1][0]
    dy = vids[vid2][1] - vids[vid1][1]
    d = sqrt(dx**2 + dy**2)
    dx = dx / d
    dy = dy / d
    # Now (dx, dy) is a unit vector pointing from v1 to v2. Hence
    # (dy, -dx) is a unit vector pointing out of f1 into f2.

    # What we do here, at each end of the tab, is to find all
    # existing lines coming from that point. Add to that the
    # imaginary line that would have appeared if I'd placed face f2
    # adjacent to f1 and made this edge into a folded one.
    # 
    # Also, check the adjacent edge in f2 and see whether it also
    # has a tab glued on to it; if so, add the imaginary line to
    # the centroid of f2. (This ensures that tabs which are not
    # adjacent in the net, but which glue on to adjacent edges of
    # another face when assembled, do not overlap on that face.)
    # 
    # Then work out which of those many lines limits the angle of
    # the tab end most, trim to 90 degrees if nothing else has
    # already done so (tabs that flare back out _look_ silly!),
    # subtract a little bit for general safety margin, and that's
    # the angle at one end of the tab.
    tablines = []
    for v, vv in (v1,v2), (v2,v1):
	vid = facepos[f1].vid[v]
	vpos = vids[vid]
	pts = []
	for vid2 in vidconn[vid]:
	    if vid2 != facepos[f1].vid[vv]:
		pts.append(vids[vid2])
        # Look in the unused placement data for f2, and find the
        # location of the vertex on the other side of v from vv.
        if (v, vv) in faceedges[f2]:
            i = faceedges[f2].index((v,vv))
            v3 = faceedges[f2][i-1][0] # 0 -> -1 works OK
        else:
            i = faceedges[f2].index((vv,v))
            v3 = faceedges[f2][(i+1)%len(faceedges[f2])][1]
        assert v3 != v and v3 != vv
        pts.append(facepos[f1].adjacent[f2].vpos[v3])
	# See if another tab appears adjacent to this one on f2.
        if v == v1:
            # Our tabpos is (v,vv). Therefore the other tab appears
            # on f2 if (v3,v) is also in tabpos.values().
            otheredge = (v3,v)
        else:
            # Our tabpos is (vv,v); so we're looking for (v,v3);
            otheredge = (v,v3)
        if tabpos.get((v,v3), None) == otheredge or \
        tabpos.get((v3,v), None) == otheredge:
            # Add the centroid of f2 (average of all the vertices) to
            # the points-to-avoid list.
            cx = cy = cn = 0
            for vx, vy in facepos[f1].adjacent[f2].vpos.values():
                cx = cx + vx
                cy = cy + vy
                cn = cn + 1.0
            pts.append((cx/cn, cy/cn))
	# Finally, we take one more precaution. If any other tabbed
	# edges share a vertex with this one, we include the angle
	# bisector between this edge and that. This ensures that
	# the two tabs will not overlap.
	for v3, v4 in tabpos.values():
	    f3 = edgeface[(v3,v4)]
	    pvid3 = facepos[f3].vid[v3]
	    pvid4 = facepos[f3].vid[v4]
	    pvid1 = facepos[f1].vid[v]
	    pvid2 = facepos[f1].vid[vv]
	    if pvid3 == pvid1:
		pass
	    elif pvid4 == pvid1:
		pvid3, pvid4 = pvid4, pvid3
	    else:
		continue
	    if pvid4 == pvid2:
		continue
	    p1 = vids[pvid2]
	    p2 = vids[pvid4]
	    n1 = sqrt((p1[0]-vpos[0])**2 + (p1[1]-vpos[1])**2)
	    p1 = (vpos[0] + (p1[0]-vpos[0])/n1, vpos[1] + (p1[1]-vpos[1])/n1)
	    n2 = sqrt((p2[0]-vpos[0])**2 + (p2[1]-vpos[1])**2)
	    p2 = (vpos[0] + (p2[0]-vpos[0])/n2, vpos[1] + (p2[1]-vpos[1])/n2)
	    pts.append(((p1[0]+p2[0])/2, (p1[1]+p2[1])/2))
	# Now go through the points figuring out which have
	# interesting angles.
	if v == v1:
	    hx, hy = dx, dy
	else:
	    hx, hy = -dx, -dy
	bestangle = pi/2
	for px, py in pts:
	    along = (px-vpos[0]) * hx + (py-vpos[1]) * hy
	    outward = (px-vpos[0]) * dy - (py-vpos[1]) * dx
	    angle = atan2(outward, along)
	    if angle > 0 and angle < bestangle:
		bestangle = angle
	# Trim bestangle by 10 degrees, or by 1/3 of its size,
	# whichever is smaller.
	if bestangle > pi/6:
	    bestangle = bestangle - pi/18
	else:
	    bestangle = bestangle * 2 / 3
	vx = hx * cos(bestangle) + dy * sin(bestangle)
	vy = hy * cos(bestangle) - dx * sin(bestangle)
	tablines.append((vpos, (vpos[0]+vx, vpos[1]+vy)))
    # Now figure out the shape of the tab. We do this in three
    # stages.
    #
    # Stage 1: simply find the intersection of the two lines, to
    # make the tab a triangle.
    ((xa1,ya1),(xa2,ya2)), ((xb1,yb1),(xb2,yb2)) = tablines
    # This call to crosspoint cannot fail since we have arranged
    # for the tab edges to be non-parallel.
    xc1, yc1 = crosspoint(xa1,ya1,xa2,ya2,xb1,yb1,xb2,yb2)
    tabvertices = ((xa1,ya1),(xc1,yc1),(xb1,yb1))
    tabheight = (xc1-xa1) * dy - (yc1-ya1) * dx
    # Stage 2: shorten the tab into a trapezium if it goes anywhere
    # near the epicentre of the polygon it will be glued to.
    sx = sy = sn = 0
    for x, y in facepos[f1].adjacent[f2].vpos.values():
	sx, sy, sn = sx+x, sy+y, sn+1
    cx, cy = sx/sn, sy/sn
    maxtabheight = (cx-xa1) * dy - (cy - ya1) * dx
    maxtabheight = maxtabheight * 4 / 5 # stop a bit short of the centre
    if tabheight > maxtabheight:
	X1 = xa1 + maxtabheight * dy
	Y1 = ya1 - maxtabheight * dx
	X2 = xb1 + maxtabheight * dy
	Y2 = yb1 - maxtabheight * dx
	xc1, yc1 = crosspoint(xa1,ya1,xa2,ya2,X1,Y1,X2,Y2)
	xc2, yc2 = crosspoint(xb1,yb1,xb2,yb2,X1,Y1,X2,Y2)
	tabvertices = ((xa1,ya1),(xc1,yc1),(xc2,yc2),(xb1,yb1))
	tabheight = maxtabheight
    # Stage 3. At this point we have now successfully finished all
    # the 3D geometry: we know the precise shape we would _like_
    # our tab to be in an ideal world. Now there's just the 2D
    # geometry remaining, i.e. fitting the tab on to the page.
    #
    # So what we do is: we collect together a list of _every_ line
    # on the bare net, plus every edge of the phantom polygon f2,
    # that does not involve one of the tab endpoints. Then we see
    # whether any of the tab lines intersect with them. If so, we
    # find the minimum distance away from the tab baseline at which
    # such an intersection point occurs, and then shorten the tab
    # (possibly a second time) to just below that length.
    #
    # Also we include a bisecting edge between every other tabbed
    # edge and this one. This should ensure that any pair of tabs
    # which might otherwise have overlapped one another will be
    # shortened until they don't.
    lines = []
    for vid1, vids2 in vidconn.items():
	if vid1 == facepos[f1].vid[v1] or vid1 == facepos[f1].vid[v2]:
	    continue
	for vid2 in vids2:
	    if vid2 < vid1: continue # do each edge only once
	    if vid2 == facepos[f1].vid[v1] or vid2 == facepos[f1].vid[v2]:
		continue
	    lines.append((vids[vid1],vids[vid2]))
    for v, vv in faceedges[f2]:
	if v == v1 or v == v2 or vv == v1 or vv == v2: continue
	lines.append((facepos[f1].adjacent[f2].vpos[v], \
	facepos[f1].adjacent[f2].vpos[vv]))
    for v3, v4 in tabpos.values():
	f3 = edgeface[(v3,v4)]
	pvid1 = facepos[f1].vid[v1]
	pvid2 = facepos[f1].vid[v2]
	pvid3 = facepos[f3].vid[v3]
	pvid4 = facepos[f3].vid[v4]
	if pvid3==pvid1 or pvid4==pvid1 or pvid3==pvid2 or pvid4==pvid2:
	    continue
	x1, y1, x2, y2 = vids[pvid1] + vids[pvid2]
	x3, y3, x4, y4 = vids[pvid3] + vids[pvid4]
	dx34 = x4 - x3
	dy34 = y4 - y3
	# Only bother with this if both lines face one another.
	problempoints = 0
	for xa, ya in (x1,y1),(x2,y2):
	    if (xa-x3)*dy34 - (ya-y3)*dx34 > 0:
		problempoints = problempoints + 1
	for xb, yb in (x3,y3),(x4,y4):
	    if (xb-x1)*dy - (yb-y1)*dx > 0:
		problempoints = problempoints + 1
	if problempoints < 3:
	    continue
	# Figure out which way round to compute the bisecting line.
	dp3 = (x3-x1) * (y2-y1) - (y3-y1) * (x2-x1)
	dp4 = (x4-x1) * (y2-y1) - (y4-y1) * (x2-x1)
	if abs(dp4) < abs(dp3):
	    x3, y3, x4, y4 = x4, y4, x3, y3
	dp1 = (x1-x3) * (y4-y3) - (y1-y3) * (x4-x3)
	dp2 = (x2-x3) * (y4-y3) - (y2-y3) * (x4-x3)
	if abs(dp2) < abs(dp1):
	    x1, y1, x2, y2 = x2, y2, x1, y1
	x13, y13 = (x1+x3)/2, (y1+y3)/2
	x24, y24 = (x2+x4)/2, (y2+y4)/2
	x24, y24 = x24*2-x13, y24*2-y13
	lines.append(((x13,y13),(x24,y24)))

    obstacles = []
    for i in range(len(tabvertices)-1):
	(tx1,ty1),(tx2,ty2) = tabvertices[i:i+2]
	for (lx1,ly1),(lx2,ly2) in lines:
	    if tx1 > lx1 and tx1 > lx2 and tx2 > lx1 and tx2 > lx2: continue
	    if ty1 > ly1 and ty1 > ly2 and ty2 > ly1 and ty2 > ly2: continue
	    if tx1 < lx1 and tx1 < lx2 and tx2 < lx1 and tx2 < lx2: continue
	    if ty1 < ly1 and ty1 < ly2 and ty2 < ly1 and ty2 < ly2: continue
	    ret = crosspoint(tx1,ty1,tx2,ty2,lx1,ly1,lx2,ly2)
	    if ret != None:
		cx1, cy1 = ret
		# See if the crossing point is between the ends of
		# each line.
		dp = ((cx1-tx1)*(tx2-tx1)+(cy1-ty1)*(ty2-ty1)) / \
		((tx2-tx1)**2 + (ty2-ty1)**2)
		if dp < 0 or dp > 1: continue
		dp = ((cx1-lx1)*(lx2-lx1)+(cy1-ly1)*(ly2-ly1)) / \
		((lx2-lx1)**2 + (ly2-ly1)**2)
		if dp < 0 or dp > 1: continue
		# We have an intersection.
		obstacles.append((cx1,cy1))
    # Also check the actual endpoints of the lines.
    for (lx1,ly1),(lx2,ly2) in lines:
	for lx, ly in (lx1,ly1), (lx2,ly2):
	    dp = (lx-xa1) * dy - (ly-ya1) * dx
	    if dp < 0 or dp > tabheight:
		continue
	    # The point is between the base and the height of
	    # the tab. Now see if it lies between the tab side
	    # lines.
	    left = crosspoint(xa1,ya1,xa2,ya2,\
	    xa1+dp*dy,ya1-dp*dx, xb1+dp*dy,yb1-dp*dx)
	    right = crosspoint(xb1,yb1,xb2,yb2,\
	    xa1+dp*dy,ya1-dp*dx, xb1+dp*dy,yb1-dp*dx)
	    dpleft = left[0]*dx + left[1]*dy
	    dpright = right[0]*dx + right[1]*dy
	    dpthis = lx*dx + ly*dy
	    if dpleft < dpthis < dpright:
		# Intersection.
		obstacles.append((lx,ly))
    if len(obstacles) > 0:
	# The tab will not fit on the diagram as shown, because
	# there is some set of points in the way. This set of
	# points is given in the array `obstacles'.
	#
	# We consider three possibilities for reducing the size of
	# the tab.
	#
	#  (a) We cut the tab off horizontally at the height of the
	#      lowest point.
	#
	#  (b) We make the tab's left-hand edge slope more
	#      shallowly, so that it misses all the obstacle
	#      points. This may reduce the tab to a triangle, or it
	#      may leave it as a trapezium.
	#
	#  (c) Exactly as (b), but we alter the right-hand edge.
	#
	# We consider all three possibilities, and pick the one
	# which leaves the tab with the greatest area.
	truncheight = None
	lpoint = rpoint = None
	lmgrad = rmgrad = None
	for x, y in obstacles:
	    # Case (a).
	    thisheight = (x-xa1) * dy - (y - ya1) * dx
	    trimheight = thisheight * 4 / 5 # miss the point by a little
	    heightdiff = thisheight - trimheight
	    if truncheight == None or truncheight > trimheight:
		truncheight = trimheight
	    # Case (b).
	    lgrad = thisheight / ((x-xa1) * (xb1-xa1) + (y-ya1) * (yb1-ya1))
	    if lmgrad == None or lmgrad > lgrad:
		lmgrad = lgrad
		lpoint = (x - heightdiff*dy, y + heightdiff*dx)
	    # Case (c).
	    rgrad = thisheight / ((x-xb1) * (xa1-xb1) + (y-yb1) * (ya1-yb1))
	    if rmgrad == None or rmgrad > rgrad:
		rmgrad = rgrad
		rpoint = (x - heightdiff*dy, y + heightdiff*dx)
	assert truncheight != None and lpoint != None and rpoint != None
	# Assemble the new tabvertices array for case (a).
	X1 = xa1 + truncheight * dy
	Y1 = ya1 - truncheight * dx
	X2 = xb1 + truncheight * dy
	Y2 = yb1 - truncheight * dx
	xc1, yc1 = crosspoint(xa1,ya1,xa2,ya2,X1,Y1,X2,Y2)
	xc2, yc2 = crosspoint(xb1,yb1,xb2,yb2,X1,Y1,X2,Y2)
	tabvertices_a = ((xa1,ya1),(xc1,yc1),(xc2,yc2),(xb1,yb1))
	# Common code between cases (b) and (c).
	X1 = xa1 + tabheight * dy
	Y1 = ya1 - tabheight * dx
	X2 = xb1 + tabheight * dy
	Y2 = yb1 - tabheight * dx
	# Now find the crossing point of the two new tab sides, for
	# each of cases b and c.
	xcb, ycb = crosspoint(xa1,ya1,lpoint[0],lpoint[1],xb1,yb1,xb2,yb2)
	xcc, ycc = crosspoint(xa1,ya1,xa2,ya2,xb1,yb1,rpoint[0],rpoint[1])
	tabvertices_b = [(xa1,ya1),(xcb,ycb),(xb1,yb1)]
	tabvertices_c = [(xa1,ya1),(xcc,ycc),(xb1,yb1)]
	# For each of these, check whether it's too high, and trim
	# the tab horizontally at its original height if so.
	for tv in tabvertices_b, tabvertices_c:
	    xc, yc = tv[1]
	    dp = (xc-xa1) * dy - (yc - ya1) * dx
	    if dp > tabheight:
		xc1, yc1 = crosspoint(xa1,ya1,xc,yc,X1,Y1,X2,Y2)
		xc2, yc2 = crosspoint(xb1,yb1,xc,yc,X1,Y1,X2,Y2)
		tv[1:2] = [(xc1,yc1),(xc2,yc2)]
	# Now we have all three possibilities; compute the area of
	# each.
	tabvertices = None
	bestarea = None
	for tv in tabvertices_a, tabvertices_b, tabvertices_c:
	    area = 0
	    for i in range(len(tv)-1):
		(xc1,yc1), (xc2,yc2) = tv[i:i+2]
		h1 = (xc1-xa1) * dy - (yc1 - ya1) * dx
		h2 = (xc2-xa1) * dy - (yc2 - ya1) * dx
		w = abs((xc2-xc1) * dx + (yc2 - yc1) * dy)
		area = area + w * (h1+h2)/2
	    if bestarea == None or area > bestarea:
		bestarea = area
		tabvertices = tv
    # Done. Store the tab data.
    vid1 = facepos[f1].vid[v1]
    vid2 = facepos[f1].vid[v2]
    tabvertices = list(tabvertices[1:-1]) # remove the endpoints
    tabpoints[(vid1, vid2)] = tuple(tabvertices)
    tabvertices.reverse()
    tabpoints[(vid2, vid1)] = tuple(tabvertices)
    tabaxes[(vid1, vid2)] = ((dx,dy), (dy,-dx))
    tabaxes[(vid2, vid1)] = ((-dx,-dy), (dy,-dx))

# Actually output the net.
psprint("%!PS-Adobe-1.0")
psprint("%%Pages: 1")
psprint("%%EndComments")
psprint("%%Page: 1")

psprint("gsave")

# Compute the overall bbox.
xmin = ymin = xmax = ymax = None
for face, placement in facepos.items():
    if xmin == None or xmin > placement.bbox[0]: xmin = placement.bbox[0]
    if xmax == None or xmax < placement.bbox[2]: xmax = placement.bbox[2]
    if ymin == None or ymin > placement.bbox[1]: ymin = placement.bbox[1]
    if ymax == None or ymax < placement.bbox[3]: ymax = placement.bbox[3]
for list in tabpoints.values():
    for x, y in list:
	if xmin == None or xmin > x: xmin = x
	if xmax == None or xmax < x: xmax = x
	if ymin == None or ymin > y: ymin = y
	if ymax == None or ymax < y: ymax = y
# Determine scale factor.
xscale = 550.0 / (xmax-xmin)
yscale = 550.0 / (ymax-ymin)
if xscale < yscale:
    scale = xscale
else:
    scale = yscale
psprint("288 400 translate")
psprint(scale, "dup scale")
# Now centre the bounding box at the origin.
psprint(-(xmax+xmin)/2, -(ymax+ymin)/2, "translate")
psprint(0.5 / scale, "setlinewidth 1 setlinejoin 1 setlinecap")
# Draw the picture, if we have one.
if picture:
    for f in faces.keys():
	facepoints = []
        placement = facepos[f]
        for v in faces[f]:
            vid = placement.vid[v]
            facepoints.append(((vids[vid][0] - placement.pos[0])/placement.z, \
	    (vids[vid][1] - placement.pos[1])/placement.z))
	picture.projection(sph.GnomonicPolygon(outfile, facepoints, \
	origin=(placement.pos[0], placement.pos[1]), scale=placement.z), \
	matrix=placement.matrix)

# Draw the cut edges, including tabs.
psprint("newpath")
vid0 = outline[-1]
cmd = "moveto"
for vid in outline:
    tp = tabpoints.get((vid0,vid), None)
    if tp != None:
	# Shade the tab.
	psprint("gsave newpath", vids[vid0][0], vids[vid0][1], "moveto")
	for x, y in tp:
	    psprint(x, y, "lineto")
	psprint(vids[vid][0], vids[vid][1], "lineto")
	psprint("closepath 0.9 setgray fill grestore")
	for x, y in tp:
	    psprint(x, y, cmd)
	    cmd = "lineto"
    psprint(vids[vid][0], vids[vid][1], cmd)
    cmd = "lineto"
    vid0 = vid
psprint("closepath stroke")
# Draw the folded edges, and the tabbed edges.
psprint("newpath")
for vid1, vid2 in foldededges:
    psprint(vids[vid1][0], vids[vid1][1], "moveto")
    psprint(vids[vid2][0], vids[vid2][1], "lineto")
for v1, v2 in tabpos.values():
    f = edgeface[(v1,v2)]
    vid1 = facepos[f].vid[v1]
    vid2 = facepos[f].vid[v2]
    psprint(vids[vid1][0], vids[vid1][1], "moveto")
    psprint(vids[vid2][0], vids[vid2][1], "lineto")
psprint("[", 1 / scale, 4 / scale, "] 0 setdash stroke [] 0 setdash")

if facelabels:
    psprint("/Helvetica findfont", 4/scale, "scalefont setfont")
    for face, placement in facepos.items():
	sx = sy = sn = 0
	for x, y in placement.vpos.values():
	    sx, sy, sn = sx+x, sy+y, sn+1
	psprint(sx/sn, sy/sn, "moveto (%s)" % face)
	psprint("dup stringwidth pop 2 div neg 0 rmoveto show")

psprint("showpage grestore")
psprint("%%EOF")
