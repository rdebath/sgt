#!/usr/bin/env python

# Given a set of points on a sphere, construct a polyhedron out of
# the tangent planes to the sphere at those points.

import sys
import string
import random
from math import pi, asin, atan2, cos, sin, sqrt
from crosspoint import crosspoint

printing_polygon = 1
printing_postscript = 0
args = sys.argv[1:]
while len(args) > 0 and args[0][:1] == "-":
    a = args[0]
    args = args[1:]

    if a == "--":
	break
    elif a == "-ps":
	printing_postscript = 1
	printing_polygon = 0
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

def psprint(*a):
    if printing_postscript:
	realprint(a)

def polyprint(*a):
    if printing_polygon:
	realprint(a)

def psdiag():
    # Produce a vaguely representative PS diagram. This code
    # preserved in case I need it again, but it's a truly awful
    # visualisation aid, so it didn't really help.

    psprint("%!PS-Adobe-1.0")
    psprint("288 400 translate 200 dup scale 0.005 setlinewidth")
    psprint("newpath 0 0 1 0 360 arc stroke")
    psprint("0.0025 setlinewidth")

    for x, y, z in points:
	sx = x
	sy = z - 0.2*y
	sr = sqrt(1 - z**2)
	psprint("newpath matrix currentmatrix")
	psprint("    1 0.2 scale 0", z / 0.2, sr, "0 360 arc")
	psprint("setmatrix stroke")
	psprint("newpath", sx, sy, "0.01 0 360 arc stroke")

    psprint("showpage")

def pointlabel(t):
    s = "vertex"
    for x in t:
	s = s + "_" + str(x)
    return s

vertices = {}
def drawfaces():
    global vertices

    # Draw each face of the polyhedron.
    #
    # Originally this function produced a PostScript diagram of
    # each plane, showing the intersection lines with all the other
    # planes, numbering which planes they were, and outlining the
    # central polygon. This gives enough information to construct a
    # net of the solid. However, it now seems more useful to output
    # a 3D model of the polygon, but the PS output option is still
    # available if required.

    psprint("%!PS-Adobe-1.0")
    psprint("%%Pages:", len(points))
    psprint("%%EndComments")
    psprint("%%BeginProlog")
    psprint("%%BeginResource: procset foo")
    psprint("/cshow {")
    psprint("    /s exch def /y exch def /x exch def")
    psprint("    gsave")
    psprint("        0 0 moveto s true charpath flattenpath pathbbox 3 -1 roll")
    psprint("    grestore")
    psprint("    add 2 div y exch sub 3 1 roll add 2 div x exch sub exch moveto")
    psprint("    s show")
    psprint("} def")
    psprint("%%EndResource")
    psprint("%%EndProlog")

    faces = []

    for i in range(len(points)):
	psprint("%%Page:", i+1)
	psprint("gsave")
	psprint("288 400 translate 150 dup scale 0.0025 setlinewidth")
	psprint("/Helvetica findfont 0.1 scalefont setfont")

	x, y, z = points[i]

	# Begin by rotating the point set so that this point
	# appears at (0,0,1). To do this we must first find the
	# point's polar coordinates...
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

	rpoints = []
	for j in range(len(points)):
	    if j == i: continue
	    xa, ya, za = points[j]
	    xb = matrix[0][0] * xa + matrix[0][1] * ya + matrix[0][2] * za
	    yb = matrix[1][0] * xa + matrix[1][1] * ya + matrix[1][2] * za
	    zb = matrix[2][0] * xa + matrix[2][1] * ya + matrix[2][2] * za
	    rpoints.append((j, xb, yb, zb))

	# Now. For each point in rpoints, we find the tangent plane
	# to the sphere at that point, and find the line where it
	# intersects the uppermost plane Z=1.
	edges = []
	for j, x, y, z in rpoints:
	    # The equation of the plane is xX + yY + zZ = 1.
	    # Combining this with the equation Z=1 is trivial, and
	    # yields the linear equation xX + yY = (1-z). Two
	    # obvious points on this line are those with X=0 and
	    # Y=0, which have coordinates (0,(1-z)/y) and
	    # ((1-z)/x,0).
	    if x == 0 or y == 0:
		continue # this point must be diametrically opposite us
	    x1, y1 = 0, (1-z)/y
	    x2, y2 = (1-z)/x, 0

	    # Find the point of closest approach between this line
	    # and the origin. This is most easily done by returning
	    # to the original equation xX+yY=(1-z); this clearly
	    # shows the line to be perpendicular to the vector
	    # (x,y), and so the closest-approach point is where X
	    # and Y are in that ratio, i.e. X=kx and Y=ky. Thus
	    # kx^2+ky^2=(1-z), whence k = (1-z)/(x^2+y^2).
	    k = (1-z)/(x*x+y*y)
	    xx = k*x
	    yy = k*y

	    # Store details of this line.
	    edges.append((x1,y1, x2,y2, xx,yy, i, j))

	    # Find the intersection points of this line with the
	    # edges of the square [-2,2] x [-2,2].
	    xyl = crosspoint(x1, y1, x2, y2, -2, -2, -2, +2)
	    xyr = crosspoint(x1, y1, x2, y2, +2, -2, +2, +2)
	    xyu = crosspoint(x1, y1, x2, y2, -2, +2, +2, +2)
	    xyd = crosspoint(x1, y1, x2, y2, -2, -2, +2, -2)
	    # Throw out any which don't exist, or which are beyond
	    # the limits.
	    xys = []
	    for xy in [xyl, xyr, xyu, xyd]:
		if xy == None: continue
		if xy[0] < -2 or xy[0] > 2: continue
		if xy[1] < -2 or xy[1] > 2: continue
		xys.append(xy)
	    if len(xys) != 2:
		psprint("% unable to draw", "%d-%d" % (i+1, j+1), "edge")
	    else:
		psprint(xys[0][0], xys[0][1], "moveto",)
		psprint(xys[1][0], xys[1][1], "lineto stroke")
		# Move 0.1 beyond the point of closest approach and
		# print the number of the side.
		d = sqrt(xx*xx + yy*yy)
		xx = xx + (0.1*xx/d)
		yy = yy + (0.1*yy/d)
		psprint(xx, yy, "(%d)" % (j+1), "cshow")

	psprint("0 0", "(%d)" % (i+1), "cshow")

	# The diagram we have just drawn is going to be a complex
	# stellated thing, with many intersection lines shown that
	# aren't part of the actual face of the polyhedron because
	# they are beyond its edges. Now we narrow our focus to
	# find the actual edges of the polygon.

	# We begin by notionally growing a circle out from the
	# centre point until it touches one of the lines. This line
	# will be an edge of the polygon, and furthermore the point
	# of contact will be _on_ the edge of the polygon. In other
	# words, we pick the edge whose closest-approach point is
	# the shortest distance from the origin.
	best = None
	n = None
	for j in range(len(edges)):
	    xx,yy = edges[j][4:6]
	    d2 = xx * xx + yy * yy
	    if best == None or d2 < best:
		best = d2
		n = j

	assert n != None
	e = edges[n]
	startn = n
	# We choose to look anticlockwise along the edge. This
	# means mapping the vector (xx,yy) into (-yy,xx).
	v = (-e[5],e[4])
	p = (e[4],e[5])
	omit = -1  # to begin with we omit the intersection with no other edge
	poly = []
	while 1:
	    # Now we have an edge e, a point p on the edge, and a
	    # direction v in which to look along the edge. Examine
	    # this edge's intersection points with all other edges,
	    # and pick the one which is closest to p in the
	    # direction of v (discarding any which are _behind_ p).
	    xa1, ya1, xa2, ya2 = e[0:4]
	    best = None
	    n2 = None
	    xp = yp = None
	    for j in range(len(edges)):
		if j == omit or j == n:
		    continue # ignore this one
		xb1, yb1, xb2, yb2 = edges[j][0:4]
		xcyc = crosspoint(xa1, ya1, xa2, ya2, xb1, yb1, xb2, yb2)
		if xcyc == None:
		    continue # this edge is parallel to e
		xc, yc = xcyc
		dotprod = (xc - p[0]) * v[0] + (yc - p[1]) * v[1]
		if dotprod < 0:
		    continue
		if best == None or dotprod < best:
		    best = dotprod
		    n2 = j
		    xp, yp = xc, yc
	    assert n2 != None
	    # Found a definite corner of the polygon. Save its
	    # coordinates, and also save the numbers of the three
	    # planes at whose intersection the point lies.
	    poly.append((xp, yp, e[6], e[7], edges[n2][7]))
	    # Now move on. We must now look along the new edge.
	    e = edges[n2]
	    p = xp, yp     # start looking from the corner we've found
	    omit = n       # next time, ignore the corner we've just hit!
	    n = n2
	    # v is slightly tricky. We are moving anticlockwise
	    # around the polygon; so we first rotate the previous v
	    # 90 degrees left, and then we choose whichever
	    # direction along the new edge has a positive dot
	    # product with this vector.
	    vtmp = (-v[1], v[0])
	    v = (-e[5],e[4])
	    if v[0] * vtmp[0] + v[1] * vtmp[1] < 0:
		v = (e[5], -e[4])
	    # Terminate the loop if we have returned to our
	    # starting edge.
	    if n == startn:
		break

	# Draw round the polygon in thicker pen.
	#psprint("0.01 setlinewidth")
	#psprint("newpath")
	#cmd = "moveto"
	#for p in poly:
	#    psprint("   ", p[0], p[1], cmd)
	#    cmd = "lineto"
	#psprint("closepath stroke")
	psprint("showpage grestore")

	# Save everything we need to write out a 3D model later on.
	# In particular this involves keeping the coordinates of
	# the points, for which we will need to find the inverse of
	# the rotation matrix so as to put the points back where
	# they started.
	#
	# The inverse rotation matrix is
	#
	#  (  cos(-theta) sin(-theta) 0 ) (  cos(pi/2-phi)  0 sin(pi/2-phi) )
	#  ( -sin(-theta) cos(-theta) 0 ) (       0        1        0       )
	#  (      0            0      1 ) ( -sin(pi/2-phi)  0 cos(pi/2-phi) )
	#
	# which comes to
	#
	#  ( cos(theta)*sin(phi)  -sin(theta)  cos(theta)*cos(phi) )
	#  ( sin(theta)*sin(phi)   cos(theta)  sin(theta)*cos(phi) )
	#  (      -cos(phi)            0             sin(phi)      )
	
	imatrix = [
	[ cos(theta)*sin(phi),  -sin(theta),  cos(theta)*cos(phi) ],
	[ sin(theta)*sin(phi),   cos(theta),  sin(theta)*cos(phi) ],
	[      -cos(phi)     ,       0     ,        sin(phi)      ]]

	facelist = []
	for p in poly:
	    xa, ya = p[0:2]
	    za = 1
	    xb = imatrix[0][0] * xa + imatrix[0][1] * ya + imatrix[0][2] * za
	    yb = imatrix[1][0] * xa + imatrix[1][1] * ya + imatrix[1][2] * za
	    zb = imatrix[2][0] * xa + imatrix[2][1] * ya + imatrix[2][2] * za
	    planes = list(p[2:5])
	    planes.sort()
	    planes = tuple(planes)
	    if not vertices.has_key(planes):
		vertices[planes] = []
	    vertices[planes].append((xb, yb, zb))
	    facelist.append(planes)

	faces.append((i, facelist))

    psprint("%%EOF")

    # Now output the polygon description.
    #
    # Each polygon has been prepared in its own frame of reference,
    # so the absolute coordinates of the vertices will vary
    # depending on which polygon they were prepared in. For this
    # reason I have kept _every_ version of the coordinates of each
    # vertex, so we can now average them into a single canonical value.
    for key, value in vertices.items():
	xt = yt = zt = n = 0
	xxt = yyt = zzt = 0
	vlabel = pointlabel(key)
	for x, y, z in value:
	    xt = xt + x
	    yt = yt + y
	    zt = zt + z
	    xxt = xxt + x*x
	    yyt = yyt + y*y
	    zzt = zzt + z*z
	    n = n + 1
	polyprint("point", vlabel, xt/n, yt/n, zt/n)

    for i, vlist in faces:
	flabel = "face_" + str(i)
	for key in vlist:
	    vlabel = pointlabel(key)
	    polyprint("face", flabel, vlabel)
	# And the surface normal (pointing outwards), which is
	# simply the position vector of the original point i.
	polyprint("normal", flabel, points[i][0], points[i][1], points[i][2])

drawfaces()
