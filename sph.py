# Python module to read and process spherical image files.

import string
import math
from crosspoint import crosspoint

def transform(matrix, point):
    xa, ya, za = point
    xb = matrix[0][0] * xa + matrix[0][1] * ya + matrix[0][2] * za
    yb = matrix[1][0] * xa + matrix[1][1] * ya + matrix[1][2] * za
    zb = matrix[2][0] * xa + matrix[2][1] * ya + matrix[2][2] * za
    return (xb, yb, zb)

class SphericalPic:
    def __init__(self, filename):
        self.shapes = []
        self.resolution = None
        self.background = None

        f = open(filename, "r")

        # States while reading file.
        HDR, BODY, SHAPE0, SHAPEN, PATH0, PATH1, PATH2, PATHN = range(8)

        state = HDR

        while 1:
            s = f.readline()
            if s == "":
                if state != BODY:
                    raise "unexpected end of file"
                break
            w = string.split(s)

            if w[0] == "background":
                if state != HDR:
                    raise "'background' keyword only valid in header"
                if len(w) != 4:
                    raise "'background' keyword expects three values"
                self.background = tuple(map(string.atof, w[1:4]))
            elif w[0] == "shape":
                if state != HDR and state != BODY:
                    raise "'shape' keyword not valid inside another shape"
                if len(w) != 4:
                    raise "'shape' keyword expects three values"
                shape = [tuple(map(string.atof, w[1:4]))]
                state = SHAPE0
            elif w[0] == "endshape":
                if state >= PATH0:
                    raise "'endshape' keyword not valid inside a path"
                elif state < SHAPE0:
                    raise "'endshape' keyword not valid outside a shape"
                elif state == SHAPE0:
                    raise "'endshape' keyword not valid before any paths"
                if len(w) != 1:
                    raise "'endshape' keyword expects no values"
                self.shapes.append(shape)
                state = BODY
            elif w[0] == "path":
                if state >= PATH0:
                    raise "'path' keyword not valid inside a path"
                elif state < SHAPE0:
                    raise "'path' keyword not valid outside a shape"
                if len(w) != 1:
                    raise "'path' keyword expects no values"
                path = []
                state = PATH0
            elif w[0] == "endpath":
                if state < PATH0:
                    raise "'endpath' keyword not valid outside a path"
                elif state < PATHN:
                    raise "'endpath' keyword not valid after fewer than 3 points"
                if len(w) != 1:
                    raise "'endpath' keyword expects no values"
                shape.append(path)
                state = SHAPEN
            elif w[0] == "point":
                if state < PATH0:
                    raise "'point' keyword not valid outside a path"
                if len(w) != 4:
                    raise "'point' keyword expects three values"
                pt = map(string.atof, w[1:4])
                pl2 = pt[0]**2 + pt[1]**2 + pt[2]**2
                pl = math.sqrt(pl2)
                pt = (pt[0]/pl, pt[1]/pl, pt[2]/pl)
                path.append(pt)
                if state < PATHN:
                    state = state + 1
            else:
                raise "undefined directive"

        f.close()

    def projection(self, proj, matrix=None):
        # The general code which performs a projection of the
        # spherical image on to a part of a plane. The "proj"
        # parameter must be a projection object supplying the
        # following methods:
	#  - mapline((x1,y1,z1),(x2,y2,z2)) takes two 3-D points,
	#    and returns a series of line segments representing the
	#    portion(s) of the line between them which are visible
	#    in the projected area. The exact return value is a
	#    3-tuple, containing two booleans and a list. The list
	#    is of 2-tuples containing pairs of 2-D coordinate
	#    objects; the first boolean is true iff the source end
	#    of the first of those pairs is actually a
	#    representation of the point (x1,y1,z1), and the second
	#    is true iff the destination end of the last pair is
	#    (x2,y2,z2). (Therefore, if both booleans are true then
	#    there must be precisely one line segment in the
	#    returned list.)
	#  - boundarypoint(p) finds the nearest boundary point to a
	#    given point. Both accepts and returns a 2-D coordinate
	#    object; the input one can be anything, but the output
	#    one must be acceptable input to boundsort() and
	#    boundaryto(). Used to clean up corner cases.
	#  - boundsort(list) takes a list composed of tuples, of
	#    which the first element of every tuple is a 2-D
	#    coordinate object representing a point on the boundary
	#    of the projected area, and sorts the list in place so
	#    that the points go in a cyclic order round that
	#    boundary.
        #  - newshape((r,g,b)) starts a new shape with a given
        #    colour.
        #  - moveto(p1) can assume it is passed a 2-D-coordinate
        #    object, and starts a new path at that point.
        #  - lineto(p1) can assume it is passed a 2-D-coordinate
        #    object, and adds that point to the current path.
        #  - boundaryto(p1,p2) can assume it is passed two 2-D-
        #    coordinate objects returned from mapline() and
        #    announced by mapline() to denote boundary points, and
        #    its job is to trace round the projection boundary from
        #    p1 to p2, on the assumption that the projected line
        #    _left_ the projected area at p1 and _returns_ at p2.
	#  - wholeboundary() creates a path tracing round the
	#    entire boundary.
        #  - endpath() ends the current path.
        #  - endshape() ends the current shape.
	#  - prologue() does any required preparation of the
	#    projection object.
	#  - epilogue() does any required cleanup of the projection
	#    object.
	proj.prologue()

	proj.newshape(self.background)
	proj.wholeboundary()
	proj.endshape()

        for shape in self.shapes:
	    initialised = 0

	    # We iterate over each subpath of a shape, calling
	    # mapline() on each line segment in the subpath. We
	    # join together returned line segments which share an
	    # endpoint, until we have a set of 2D paths within the
	    # projected area.
	    #
	    # Some of these 2D paths will be cyclic. Others may
	    # begin or end on the area boundary. If there are any
	    # of the latter type, we sort their entry and exit
	    # points in order around the boundary using
	    # boundsort(), and use this to draw a single overall
	    # path using lineto() and boundaryto().
	    #
	    # If there are _no_ paths beginning or ending on the
	    # area boundary (so they're either all cyclic, or our
	    # set of visible paths is completely empty), we must
	    # determine whether or not the _entire_ boundary needs
	    # to be outlined for this shape. This bit I haven't
	    # worked out yet [FIXME].
	    allpaths = []

            for path in shape[1:]:
                lastpoint = path[-1]
		if matrix:
		    lastpoint = transform(matrix, lastpoint)

		paths = []
		currpath = ["start"]

                for point in path:
		    if matrix:
			point = transform(matrix, point)
		    s, e, segments = proj.mapline(lastpoint, point)
		    #print "% mapline", lastpoint, point, s, e, segments

		    # There is just a possibility that something
		    # confusing has happened.
		    #
		    # If s is true, we expect currpath to be
		    # non-null (because we expect e to have been
		    # true when we processed the previous line
		    # segment). If s is true and currpath is null,
		    # it must be because the last line segment we
		    # processed went outside the projected area and
		    # a later one returned to a point exactly on
		    # the boundary; so in this situation we simply
		    # unset s.
		    if s and currpath == None:
			s = 0
			#print "% boundary-ifying start point "
			segments[0] = (proj.boundarypoint(segments[0][0]), \
			segments[0][1])
		    # Similarly and in reverse: if s is false and
		    # currpath is non-null, it is because the last
		    # line segment ended precisely on the boundary
		    # so e was not set. In that situation we must
		    # retrospectively clear up the mess.
		    if not s and (currpath != None and currpath != ["start"]):
			currpath[-1] = proj.boundarypoint(currpath[-1])
			#print "% boundary-ifying last endpoint"
			paths.append(currpath)
			#print "% appended", currpath
			currpath = None

		    for i in range(len(segments)):
			p1, p2 = segments[i]

			if i == 0 and s:
			    assert currpath != None
			    currpath.append(p2)
			else:
			    assert currpath == None or currpath == ["start"]
			    currpath = ["boundary", p1, p2]

			if i == len(segments)-1 and e:
			    pass # leave currpath to have more added to it
			else:
			    # currpath is complete; add it to path collection
			    #print "% appended", currpath
			    paths.append(currpath)
			    currpath = None

                    lastpoint = point

		# The result of processing a single subpath must be
		# _either_ precisely one cyclic path, _or_ a set of
		# boundary paths. In either case, we may have two
		# half-paths to join up at the end of processing.
		if currpath != None:
		    if len(paths) > 0:
			# Boundary paths at start and end need joining.
			assert paths[0][0] == "start"
			currpath.extend(paths[0][1:])
			del paths[0]
			#print "% joined boundary paths:", currpath
			paths.append(currpath)
		    else:
			# There is only one path and it is cyclic.
			# Or it might be completely nonexistent.
			assert currpath[0] == "start"
			if len(currpath) > 1:
			    currpath[0] = "cyclic"
			    #print "% made cyclic path:", currpath
			    paths.append(currpath)

		# One final case of confusion: there might _still_
		# be a `start' path at the beginning, in which case
		# it's because we had a boundary point right at the
		# end and therefore a borderline case at the
		# beginning. Correct it.
		if len(paths) > 0 and paths[0][0] == "start":
		    paths[0][1] = proj.boundarypoint(paths[0][1])
		    paths[0][0] = "boundary"

		allpaths.extend(paths)

	    # Now we've processed the entirety of a shape, and we
	    # have a collection of paths which are either cyclic or
	    # boundary. Process the cyclic paths (which are easy),
	    # and note down the entry and exit points of the
	    # boundary paths.
	    bpoints = []
	    for i in range(len(allpaths)):
		p = allpaths[i]
		if p[0] == "cyclic":
		    if not initialised:
			proj.newshape(shape[0])
			initialised = 1
		    proj.moveto(p[1])
		    for pt in p[2:]:
			proj.lineto(pt)
		    proj.lineto(p[1])
		    proj.endpath()
		else:
		    bpoints.append((p[1], "entry", i))
		    bpoints.append((p[-1], "exit", i))

	    #print "% allpaths", allpaths
	    #print "% bpoints", bpoints

	    # Now sort the boundary points.
	    proj.boundsort(bpoints)
	    #for bp in bpoints:
	    #    print "% ", bp
	    #print "% bpoints-sorted", bpoints

	    # Go through and match up each entry point to the exit
	    # point closest after it. Set seq[i] to the index of
	    # the path whose entry point needs to be connected to
	    # the exit point of the path with index i.
	    #
	    # In principle, bpoints should be arranged with entry
	    # and exit points in strict alternation. It is just
	    # possible that this won't be the case owing to an
	    # entry and exit point being _identical_; in this case
	    # we must cheat a little.
	    #
	    # What I do is
	    #
	    #  - assign the entry points in order throughout
	    # 	 bpoints to the exit points in order throughout
	    # 	 bpoints
	    #  - then try moving the exit points along by one so
	    # 	 that entry 0 maps to exit 1 etc
	    #  - at each point, measure the distance from each
	    # 	 entry to exit point modulo len(bpoints); stop
	    # 	 incrementing the shift when the distance goes up
	    # 	 rather than down.
	    exits = []
	    entries = []
	    for i in range(len(bpoints)):
		if bpoints[i][1] == "entry":
		    entries.append(i)
		else:
		    exits.append(i)
	    assert len(entries) == len(exits)
	    n = len(entries)
	    bn = len(bpoints)
	    shift = 0
	    prevdist = None
	    while 1:
		dist = 0
		for i in range(n):
		    exitpos = exits[i]
		    entrypos = entries[(i+shift) % n]
		    dist = dist + (entrypos - exitpos + bn) % bn
		    #print "% posns", exitpos, entrypos, (entrypos - exitpos + bn) % bn
		#print "% shift", shift, dist, prevdist
		if prevdist != None and prevdist <= dist:
		    # Stop at the previous shift value.
		    shift = shift - 1
		    break
		prevdist = dist
		shift = shift + 1
	    # Now we know the shift value; just do the mapping.
	    seq = {}
	    for i in range(n):
		exit = bpoints[exits[i]]
		entry = bpoints[entries[(i+shift) % n]]
		seq[exit[2]] = entry[2]
	    #print "% seq", seq

	    # Now draw the boundary paths.
	    while len(seq) > 0:
		if not initialised:
		    proj.newshape(shape[0])
		    initialised = 1
		i, j = seq.popitem()
		#print "% pop", i, j
		prevexit = None
		firstentry = None
		while 1:
		    #print "% got", i, j
		    path = allpaths[i]
		    #print "% boundaryto", prevexit, path
		    if prevexit:
			proj.boundaryto(prevexit, path[1])
		    else:
			firstentry = path[1]
			proj.moveto(path[1])
		    for pt in path[2:]:
			proj.lineto(pt)
		    if seq.has_key(j):
			prevexit = path[-1]
			i, j = j, seq[j]
			del seq[i] # was seq[j] until a moment ago
		    else:
			#print "% boundaryto2", path[-1], firstentry
			proj.boundaryto(path[-1], firstentry)
			proj.endpath()
			break

	    # FIXME: Outline the entire boundary if necessary.

	    if initialised:
		proj.endshape()
	proj.epilogue()

def realprint(a, outfile):
    for i in range(len(a)):
	outfile.write(str(a[i]))
	if i < len(a)-1:
	    outfile.write(" ")
	else:
	    outfile.write("\n")

def debug(*a):
    realprint(a, sys.stderr)

# This function will be useful more than once below. Given two 3-D
# coordinate sets x1,y1,z1 and x2,y2,z2 describing points on the
# unit sphere such that z1 and z2 have opposite signs. it returns
# the x,y coordinates (z is always 0) of the point on the sphere's
# z=0 equator through which passes the geodesic between the two
# input points.
def equator_crossing(p1, p2):
    xprod = (p1[1]*p2[2]-p1[2]*p2[1], \
    p1[2]*p2[0]-p1[0]*p2[2], \
    p1[0]*p2[1]-p1[1]*p2[0])
    # The equation (xprod . x == 0) now defines a plane which
    # is known not to be parallel to the z=0 plane (since xprod
    # is the cross product of two vectors differing in their z
    # component, hence they can't both have z=0). We wish to
    # find a vector pointing along the line of intersection
    # _between_ this plane and z=0.
    #
    # So we have ax+by+cz=0, and z=0. Substituting the latter
    # into the former gives ax+by=0, and vectors satisfying
    # this include any multiple of (-b,a,0), which we know to
    # be non-trivial because otherwise xprod would be vertical.
    v = (-xprod[1], xprod[0])
    vl2 = v[0]**2 + v[1]**2
    vl = math.sqrt(vl2)
    v1 = (v[0]/vl, v[1]/vl)
    v2 = (-v[0]/vl, -v[1]/vl)
    # Either v1 or v2 is the point we want. Pick the one closer
    # to the midpoint between p1 and p2.
    v1d = (2*v1[0]-p1[0]-p2[0], 2*v1[1]-p1[1]-p2[1])
    v2d = (2*v2[0]-p1[0]-p2[0], 2*v2[1]-p1[1]-p2[1])
    if v1d[0]**2+v1d[1]**2 < v2d[0]**2+v2d[1]**2:
	return v1
    else:
	return v2

# A projection class which generates a circle showing one
# hemisphere of the image, and outputs PostScript to a supplied
# file object. For the moment, I'll do the really simple thing and
# simply take the z>0 hemisphere, leaving matrix transformations to
# be Somebody Else's Problem.
class Hemisphere:
    def __init__(self, file):
        self.f = file

    def psprint(self, *a):
        realprint(a, self.f)

    # `visible' and `map' are internal methods used only to
    # implement the externally used `mapline'.
    def visible(self, point):
        return point[2] > 0

    def map(self, point):
        return point[0], point[1]

    # mapline() is reasonably easy in an entire-hemisphere
    # projection, because there is no way that a single
    # less-than-180-degrees section of a great circle can require
    # more than one line segment visible on the hemisphere. So we
    # need only test the visibility of the two end points, and find
    # a crossing point between them if exactly one is visible.
    def mapline(self, p1, p2):
	v1 = self.visible(p1)
	v2 = self.visible(p2)
	if not (v1 or v2):
	    return (0, 0, [])
	if not (v1 and v2):
	    c = equator_crossing(p1, p2)
	if v1:
	    r1 = self.map(p1)
	else:
	    r1 = c
	if v2:
	    r2 = self.map(p2)
	else:
	    r2 = c
	return (v1, v2, [(r1, r2)])

    def boundarypoint(self, p):
	pl = math.sqrt(p[0]**2 + p[1]**2)
	return p[0]/pl, p[1]/pl

    def boundsort(self, list):
	def qval(x, y):
	    if y >= 0 and x > 0:
		return (1, y)
	    elif y >= 0:
		return (2, -x)
	    elif x < 0:
		return (3, -y)
	    else:
		return (4, x)
	def boundcmp(t1, t2, qval=qval):
	    x1, y1 = t1[0]
	    x2, y2 = t2[0]
	    q1 = qval(x1, y1)
	    q2 = qval(x2, y2)
	    if q1 < q2:
		return -1
	    elif q1 > q2:
		return +1
	    else:
		return 0
	list.sort(boundcmp)

    def prologue(self):
	self.psprint("gsave 2 dict begin")
	self.psprint("/greatcircleto {")
	self.psprint("  10 dict begin")
	self.psprint("  currentpoint /y1 exch def /x1 exch def")
	self.psprint("  /y2 exch def /x2 exch def")
	self.psprint("  /lmod 1 x1 x2 mul add y1 y2 mul add")
	self.psprint("  1 x1 x1 mul sub y1 y1 mul sub")
	self.psprint("  1 x2 x2 mul sub y2 y2 mul sub mul dup 0 lt {pop 0} {sqrt} ifelse add")
	self.psprint("  2 div sqrt def")
	self.psprint("  /xm x1 x2 add 2 div def /ym y1 y2 add 2 div def")
	self.psprint("  xm ym transform xm lmod mul ym lmod mul transform")
	self.psprint("  3 -1 roll sub abs 3 1 roll sub abs add")
	self.psprint("  currentflat lt {")
	self.psprint("    x2 y2 lineto")
	self.psprint("  } {")
	self.psprint("    xm lmod div ym lmod div greatcircleto")
	self.psprint("    x2 y2 greatcircleto")
	self.psprint("  } ifelse")
	self.psprint("  end")
	self.psprint("} bind def")

    def epilogue(self):
	self.psprint("end grestore")

    def newshape(self, colour):
        self.psprint("newpath", colour[0], colour[1], \
        colour[2], "setrgbcolor")

    def moveto(self, p2):
        self.psprint(p2[0], p2[1], "moveto")

    def lineto(self, p2):
        self.psprint(p2[0], p2[1], "greatcircleto")

    def boundaryto(self, p2a, p2b):
        aa = math.atan2(p2a[1], p2a[0]) * 180 / math.pi
        ab = math.atan2(p2b[1], p2b[0]) * 180 / math.pi
        if ab < aa:
            ab = ab + 360
        self.psprint("0 0 1", aa, ab, "arc")

    def wholeboundary(self):
        self.psprint("0 0 1 0 360 arc closepath")

    def endpath(self):
        self.psprint("closepath")

    def endshape(self):
        self.psprint("fill")

# A projection class which generates a gnomonic projection into a
# polygon of the user's choice, and outputs PostScript to a
# supplied file object. As above, I'm going to be simplistic and
# assume the polygon to be in the z=1 plane.
class GnomonicPolygon:
    def __init__(self, file, polypoints, origin=(0,0), scale=1):
        self.f = file
	self.origin = origin
	self.scale = scale
	self.poly = polypoints[:] # simple list of (x,y) 2-tuples
	maxd2 = 0
	for x, y in self.poly:
	    d2 = x*x + y*y
	    if maxd2 < d2:
		maxd2 = d2
	self.maxdist = math.sqrt(maxd2 + 1)
	# Make sure the points are going the right way round the
	# polygon. We do this by finding the rightmost edge and
	# seeing whether it's going up or down.
	best = None
	for i in range(len(self.poly)):
	    iprev = i-1
	    if iprev < 0:
		iprev = len(self.poly) - 1
	    px1, py1 = self.poly[iprev]
	    px2, py2 = self.poly[i]
	    if (py1 < 0) ^ (py2 < 0):
		cx, cy = crosspoint(px1, py1, px2, py2, \
		min(px1,px2)-1, 0, max(px1,px2)+1, 0)
		if best == None or best[0] < cx:
		    best = (cx, py2-py1)
	if best[1] < 0:
	    self.poly.reverse()

    def psprint(self, *a):
        realprint(a, self.f)

    def inside(self, x, y):
	ret = 0
	for i in range(len(self.poly)):
	    px1, py1 = self.poly[i-1] # 0 -> -1 wraparound
	    px2, py2 = self.poly[i]
	    if (py1 < y) ^ (py2 < y):
		cx, cy = crosspoint(x, y, x+self.maxdist*2, y, \
		px1, py1, px2, py2)
		if cx > x:
		    ret = ret ^ 1
	return ret

    # For this type of projection, I'm going to encode more
    # information in my 2D coordinate objects than just the
    # coordinates: I'm also going to include information on whether
    # or not they're boundary points, and how far round the
    # boundary they are if so.
    #
    # My 2-D coordinate object will be a tuple containing
    #
    #  - the x coordinate
    #  - the y coordinate
    #  - if not a boundary point, None
    #  - if a boundary point: the index of the preceding polypoint,
    # 	 followed by a number in [0,1) indicating how far along the
    # 	 line between that and the next polypoint we are.

    def mapline(self, p1, p2):
	if p1[2] <= 0 and p2[2] <= 0:
	    return (0, 0, [])

	if p1[2] <= 0 or p2[2] <= 0:
	    # We have a great circle segment crossing the z=0
	    # equator. We compute the equator-crossing point and
	    # use that as a vector to determine a reasonable
	    # far-end point for this line segment.
	    ex, ey = equator_crossing(p1, p2)
	    if p1[2] < 0:
		nx, ny = p2[:2]
	    else:
		nx, ny = p1[:2]
	    fx = nx + ex * self.maxdist * 2
	    fy = ny + ey * self.maxdist * 2

	if p1[2] <= 0:
	    x1, y1 = fx, fy
	else:
	    x1, y1 = p1[0]/p1[2], p1[1]/p1[2]

	if p2[2] <= 0:
	    x2, y2 = fx, fy
	else:
	    x2, y2 = p2[0]/p2[2], p2[1]/p2[2]

	# Conceptually the d values in these tuples are 0 and 1,
	# since the two points are at the beginning and end of the
	# line. We set them to -1 and 2 to avoid tiny rounding
	# errors making the end points sort anywhere other than the
	# real ends of the list.
	points = [(-1, (x1, y1, None)), (2, (x2, y2, None))]

	dmax = abs(x2-x1) + abs(y2-y1)

	# Now we have a 2-D line going from x1,y1 to x2,y2.
	# Intersect it with all the boundary lines.
	for i in range(len(self.poly)):
	    iprev = i-1
	    if iprev < 0:
		iprev = len(self.poly) - 1
	    px1, py1 = self.poly[iprev]
	    px2, py2 = self.poly[i]
	    cp = crosspoint(x1, y1, x2, y2, px1, py1, px2, py2)
	    if cp != None:
		cx, cy = cp
		if cx < min(px1,px2) or cx > max(px1,px2) or \
		cy < min(py1,py2) or cy > max(py1,py2) or \
		cx < min(x1,x2) or cx > max(x1,x2) or \
		cy < min(y1,y2) or cy > max(y1,y2):
		    continue
		d = (abs(cx-x1)+abs(cy-y1))/dmax
		pd = (abs(cx-px1)+abs(cy-py1))/(abs(px2-px1)+abs(py2-py1))
		points.append((d, (cx, cy, iprev, pd)))

	# And now we have a set of points dividing up our original
	# line segment into pieces. Sort out which sections are
	# inside the polygon and which outside, and construct our
	# final list of return values.
	points.sort()
	list = []
	for i in range(1, len(points)):
	    d1, p1 = points[i-1]
	    d2, p2 = points[i]
	    xm, ym = (p1[0]+p2[0])/2, (p1[1]+p2[1])/2
	    if self.inside(xm, ym):
		list.append((p1, p2))

	#print "% mapline-internal", list
	if len(list) > 0:
	    s = (list[0][0][2] == None)
	    e = (list[-1][1][2] == None)
	else:
	    s = e = 0
	return (s, e, list)

    def boundarypoint(self, p):
	#print "% boundarypoint in", p

	if p[2] == None:
	    x, y = p[:2]

	    best = None

	    for i in range(len(self.poly)):
		iprev = i-1
		if iprev < 0:
		    iprev = len(self.poly) - 1
		px1, py1 = self.poly[iprev]
		px2, py2 = self.poly[i]

		# To determine the perpendicular distance between a
		# point r and the line between points a and b, we
		# take the dot product of (r-a) with a vector
		# perpendicular to (b-a). This gives the distance
		# multiplied by the length of (b-a); so we must
		# then divide by the modulus of (b-a).
		dp = (x-px1) * (py2-py1) + (y-py1) * (px1-px2)
		mod = math.sqrt((px2-px1)**2 + (py2-py1)**2)
		dp = dp / mod

		if best == None or best[0] < dp:
		    pd = (abs(x-px1)+abs(y-py1))/(abs(px2-px1)+abs(py2-py1))
		    bx = px1 + (px2-px1) * pd
		    by = py1 + (py2-py1) * pd
		    best = (dp, bx, by, iprev, pd)

	    p = best[1:]

	#print "% boundarypoint out", p
	return p

    def boundsort(self, list):
	def boundcmp(t1, t2):
	    q1 = t1[0][2:]
	    q2 = t2[0][2:]
	    if q1 < q2:
		return -1
	    elif q1 > q2:
		return +1
	    else:
		return 0
	list.sort(boundcmp)

    def newshape(self, colour):
        self.psprint("newpath", colour[0], colour[1], \
        colour[2], "setrgbcolor")

    def moveto(self, p2):
        self.psprint(p2[0] * self.scale + self.origin[0], \
	p2[1] * self.scale + self.origin[1], "moveto")

    def lineto(self, p2):
        self.psprint(p2[0] * self.scale + self.origin[0], \
	p2[1] * self.scale + self.origin[1], "lineto")

    def boundaryto(self, p2a, p2b):
	assert p2a[2] != None and p2b[2] != None
	i = p2a[2]
	while i != p2b[2]:
	    i = (i + 1) % len(self.poly)
	    self.psprint(self.poly[i][0] * self.scale + self.origin[0], \
	    self.poly[i][1] * self.scale + self.origin[1], "lineto")
	self.psprint(p2b[0] * self.scale + self.origin[0], \
	p2b[1] * self.scale + self.origin[1], "lineto")

    def wholeboundary(self):
        self.psprint(self.poly[-1][0]*self.scale + self.origin[0], \
	self.poly[-1][1]*self.scale + self.origin[1], "moveto")
	for i in range(len(self.poly)):
	    self.psprint(self.poly[i][0] * self.scale + self.origin[0], \
	    self.poly[i][1] * self.scale + self.origin[1], "lineto")
	self.psprint("closepath")

    def endpath(self):
        self.psprint("closepath")

    def endshape(self):
        self.psprint("fill")

    def prologue(self):
	self.psprint("gsave")
    def epilogue(self):
	self.psprint("grestore")

# test code
#import sys
#if len(sys.argv) > 1:
#    filename = sys.argv[1]
#else:
#    filename = "test.sph"
#sph = SphericalPic(filename)
#print "%!PS-Adobe-3.0 EPSF-3.0"
#print "%%BoundingBox: 0 0 500 500"
#print "%%Pages: 1"
#print "%%EndComments"
#print "%%Page: 1 1"
#print "gsave 250 dup translate 250 dup scale"
##sph.projection(Hemisphere(sys.stdout))
#sph.projection(GnomonicPolygon(sys.stdout, [(-1,-1),(1,1),(1,-1)]))
#print "grestore"
#print "%%EOF"
