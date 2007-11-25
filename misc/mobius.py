#!/usr/bin/env python

# Companion program to mobius.max which implements a physical
# approximation to the circular-edged Mobius strip by means of flat
# polygons joined at the edges. (I would say `polyhedral', but
# since it's non-orientable and has exposed edges, it isn't
# actually a polyhedron. However, it can be made by printing nets
# on cardboard, cutting, folding and gluing in the same way one
# makes polyhedra, so the distinction is only important for
# mathematical purposes and not practical ones.)
#
# Of course, the edge cannot be a perfect circle when constructing
# the model from flat polygons. Instead, it's a regular octagon.

# This program defines one single model, but will run in a number
# of modes:
#
#   ./mobius.py pov            generate a fragment of POV-Ray code which
#                              describes the model
#   ./mobius.py gnuplot        generate a string of gnuplot commands which
#                              create a 3D plot of the model
#   ./mobius.py net            generate a simple cut-and-fold net as PostScript
#   ./mobius.py net1           generate a coloured cut-and-fold PS net
#   ./mobius.py net2           generate a second coloured PS net (see below)

# The three net options - net, net1, net2 - are designed to work
# together to produce a double-sided colour model of the strip. The
# uncoloured one - `net' - should be printed on cardboard, to act
# as a skeleton giving the model strength. The two coloured ones,
# `net1' and `net2', are then printed on thin paper and glued to
# the surfaces of the cardboard skeleton. This gives a model which
# is coloured on all surfaces.
#
# The colour scheme is a transformation of the natural colouring of
# a conventional Mobius strip: it goes once round the primary
# colour wheel (from red through yellow, green, cyan, blue, magenta
# and back to red) in two trips round the strip. Thus, the
# colouring is continuous on the surface, and any given point on
# the surface has opposite colours on the two sides of the paper.
# Both these properties are preserved when the strip is transformed
# so that the edge lies flat.
#
# The `net1' and `net2' options produce a differently shaped net
# from `net' proper. This is so that no tabs are required to hold
# the model together (since the solid has no inside, tabs would be
# visible and unsightly wherever they appeared): the two nets have
# the cut edges in different places, so that the paper veneers hold
# the cardboard skeleton's cut edges together and vice versa.
#
# The `net1' and `net2' nets can also be adjusted in shape slightly
# to take account of the thickness of the card they're going over.
# To enable this, specify the card thickness in millimetres on the
# command line before the keyword: `./mobius.py 0.3 net1'. This is
# fundamentally an imprecise operation; I devised what seemed like
# a rigorous mathematical model of the amount of slack to allow,
# but it didn't really seem to come out quite right. In fact I use
# 0.2mm (200 micron) card, but I find that telling this program 0.3
# rather than 0.2 works better.
#
# To actually build the solid:
#
#  - first print out all three nets. `net' on card, and `net1' and
#    `net2' on paper. Provide a card thickness when generating net1
#    and net2 if you need to. My command line looks like this:
#     (./mobius.py net; ./mobius.py 0.3 net1; ./mobius.py 0.3 net2) | lpr
#
#  - now score, cut out, and fold. The folding is complex, since
#    some folds are mountain and some are valley.
#     * On the two coloured nets, `net1' and `net2', it's easy:
#       hold the net horizontally so that the central kite has its
#       blunt point (the one not attached to any other face of the
#       net) uppermost, and then all the folds to the left of the
#       kite are mountain folds and all those to the right are
#       valley folds.
#     * On the plain net `net', it's slightly more complex: it's
#       _mostly_ mountain folds to the left of the kite and valley
#       folds to the right (again, holding the kite with its blunt
#       point upwards, i.e. this time the sharp point with no other
#       face of the net attached is downwards), but the two top
#       edges of the kite itself are reversed. So going from the
#       leftmost end face to the rightmost end face the folds are
#       m,m,m,m,m,v,m,v,v,v,v,v.
#     * I find that scoring is unnecessary on the paper nets, but
#       important on the card one. Scoring for mountain folds is
#       easy, because you have the printed net to guide the knife;
#       but scoring for valley folds should be done on the reverse
#       side of the card. What I do is to poke a knife through the
#       card at the vertices at the ends of all the valley-fold
#       lines, and then turn the card over and use those holes to
#       guide my scoring.
#
#  - putting the whole lot together is _very hard_. I recommend
#    starting with the right-hand end of each of the coloured nets,
#    because those parts of the model will be impossible to reach
#    once the rest has gone together. So glue the RH two faces of
#    each coloured net to the corresponding faces on the skeleton,
#    then fold the whole thing into basically the right shape (this
#    is _hard_, but persevere and you'll hopefully get it in the
#    end), then continue to glue the coloured nets to the skeleton
#    working round until you eventually reach the LH end. Try to
#    maintain precision everywhere, particularly when gluing the
#    kite faces (which is notable for being the point at which you
#    fasten the skeleton into its final shape by bringing the long
#    edges of the kites together with the corresponding edges of
#    the adjacent triangular faces).

from math import *
import sys

def f(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y*sin(pi*x/2) + z*cos(pi*x/2), -y*cos(pi*x/2) + z*sin(pi*x/2)]
def g(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y, z/1.5]
def h(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y, z+x**2]

def finv(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y*sin(pi*x/2) - z*cos(pi*x/2), y*cos(pi*x/2) + z*sin(pi*x/2)]
def ginv(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y, z*1.5]
def hinv(x,y=None,z=None):
    if y == None:
        x,y,z = x
    return [x, y, z-x**2]

def hgf(v):
    return h(g(f(v)))
def fghinv(v):
    return finv(ginv(hinv(v)))
def circle(t):
    return [cos(pi*t), sin(pi*t), 0]
def mult(v,s):
    return [x*s for x in v]
def add(v,u):
    return [v[i]+u[i] for i in range(len(v))]

xpos = {}
def ms(v,u,name):
    xpos[name] = v
    return fghinv(add(mult(hgf(circle(v)),u), mult(hgf(mult(circle(v),-1)),(1-u)))) + [name]

def cross(u,v):
    return [u[(i+1)%3]*v[(i+2)%3] - u[(i+2)%3]*v[(i+1)%3] for i in range(3)]
def vlen(v):
    return sqrt(sum([v[i]**2 for i in range(3)]))
def smult(v, s):
    return [v[i]*s for i in range(3)]
def normalise(v):
    return smult(v, 1.0/vlen(v))

args = sys.argv[1:]
cardthickness = 0.0
scale = 72*1.5
if len(args) > 0 and args[0][0] in "0123456789":
    # Just to be awkward, we measure the card thickness in
    # millimetres but the main scale is in points.
    cardthickness = float(args[0]) * 72/25.4 / scale
    args = args[1:]

if len(args) > 0 and args[0] == "pov":
    def polygon(*a):
        print "  polygon {"
        s = "    %d" % (len(a)+1)
        for i in range(-1,len(a)):
            p = a[i]
            s = s + ", <%.17g,%.17g,%.17g>" % tuple(p[:3])
        print s
        print "    pigment { color White }"
        print "  }"
    triangle = quadrilateral = polygon
    def go():
        pass

elif len(args) > 0 and args[0] == "gnuplot":
    print "set urange [0:1]"
    print "set vrange [0:1]"
    print "set parametric"
    print "set hidden"
    print "set key noautotitles"
    print "set isosamples 2; splot",
    sep = ""
    def triangle(a,b,c):
        global sep
        for k in range(3):
            print sep,"%.17g+%.17g*u+%.17g*u*v" % (a[k], b[k]-a[k], c[k]-b[k]),
            sep = ","
    def quadrilateral(a,b,c,d):
        global sep
        for k in range(3):
            print sep,"u*(v*%.17g+(1-v)*%.17g)+(1-u)*(v*%.17g+(1-v)*%.17g)" % (a[k], b[k], d[k], c[k]),
            sep = ","
    def go():
        pass

elif len(args) > 0 and args[0] == "polyhedron":
    points = {}
    faces = []
    def polygon(*a):
        for p in a:
            points[p[3]] = p
        faces.append(a)
    triangle = quadrilateral = polygon
    def go():
        for n,p in points.items():
            print "point %s %.17g %.17g %.17g" % (n, p[0], p[1], p[2])
        for f in faces:
            fn = "".join([p[3] for p in f])
            for p in f:
                print "face %s %s" % (fn, p[3])
            fv1 = [f[2][i]-f[1][i] for i in range(3)]
            fv2 = [f[0][i]-f[1][i] for i in range(3)]
            fnv = normalise(cross(fv1,fv2))
            print "normal %s %.17g %.17g %.17g" % (fn, fnv[0], fnv[1], fnv[2])

elif len(args) > 0 and args[0][:3] == "net":
    class face:
        pass
    faces = []
    def polygon(*a):
        f = face()
        f.points = a
        f.posns = {}
        f.index = {}
        for p in a:
            f.posns[p[3]] = p
        fv1 = [a[2][i]-a[1][i] for i in range(3)]
        fv2 = [a[0][i]-a[1][i] for i in range(3)]
        f.normal = normalise(cross(fv1,fv2))
        faces.append(f)
        return f
    triangle = quadrilateral = polygon
    def trans(matrix, point):
        return [sum([matrix[3*i+j]*point[j] for j in range(3)]) for i in range(3)]
    def add(point1, point2):
        return [point1[i]+point2[i] for i in range(3)]
    def sub(point1, point2):
        return [point1[i]-point2[i] for i in range(3)]
    def dot(point1, point2):
        return reduce(lambda x,y:x+y, [point1[i]*point2[i] for i in range(3)])
    def matmul(mx1, mx2):
        return [sum([mx1[3*i+j]*mx2[3*j+k] for j in range(3)]) for i in range(3) for k in range(3)]
    allfaces = []
    pointpositions = []
    def site(f, matrix, origin, xoffs):
        f.placed = {}
        f.xpos = {}
        for p in f.points:
            i = f.index[p[3]] = f.index.get(p[3], len(pointpositions))
            f.placed[p[3]] = add(origin, trans(matrix, p))
            if i == len(pointpositions):
                pointpositions.append([])
            pointpositions[i].append(f.placed[p[3]])
            f.xpos[p[3]] = xoffs[p[3]] + xpos[p[3]]
        allfaces.append(f)
    def basematrix(normal):
        v1 = cross(normal, [1,0,0])
        v2 = cross(normal, [0,1,0])
        if vlen(v1) > 0:
            v1 = normalise(v1)
        else:
            v1 = normalise(v2)
        v2 = cross(normal, v1)
        return v1 + v2 + normal # transpose == inverse
    def centre(f, xoffs, flip=1):
        f.normal = smult(f.normal, flip)
        mx = basematrix(f.normal)
        sum = [0,0,0]
        for p in f.points:
            sum = add(sum, trans(mx, p))
        origin = smult(sum, 1.0/len(f.points))
        site(f, mx, origin, xoffs)
    def place(f, of, xoffs, flip=1):
        # Find the two points in common between the two faces.
        vs = [p[3] for p in f.points]
        vs = filter(lambda s: s in vs, [p[3] for p in of.points])
        assert len(vs) == 2
        v0, v1 = vs
        f.index[v0] = of.index[v0]
        f.index[v1] = of.index[v1]
        # Establish a base matrix transforming the face into the plane.
        f.normal = smult(f.normal, flip)
        mx = basematrix(f.normal)
        # Determine the angle of the v0-v1 line in each face.
        oav = sub(of.placed[v1], of.placed[v0])
        oa = atan2(oav[1], oav[0])
        av = sub(trans(mx, f.posns[v1]), trans(mx, f.posns[v0]))
        a = atan2(av[1], av[0])
        # Rotate to turn the latter into the former.
        m2 = [cos(oa-a), -sin(oa-a), 0, sin(oa-a), cos(oa-a), 0, 0, 0, 1]
        mx = matmul(m2, mx)
        # And translate so that v0 meets v0.
        origin = sub(of.placed[v0], trans(mx, f.posns[v0]))

        # Now adjust the face's position by a small fiddle factor
        # to take account of the thickness of card it's wrapped
        # around. First, determine whether we're producing a
        # mountain or valley fold, by taking the dot product of the
        # distance between the faces' centroids with one face's
        # normal vector.
        s1 = smult(reduce(add, f.points), 1.0/len(f.points))
        s2 = smult(reduce(add, of.points), 1.0/len(of.points))
        ds = sub(s2, s1)
        dds = dot(ds, f.normal)
        if dds < 0:
            # If it's a mountain fold, continue. Next we compute
            # the dihedral angle by taking the dot product of the
            # two normal vectors. (They're already normalised.)
            angle = acos(dot(f.normal, of.normal))
            # Our model is that the paper wraps around that much of
            # a cylinder whose radius is the card's entire
            # thickness (because the card bends about its far
            # edge). So the additional distance required is simply
            # equal to the angle in radians times the card
            # thickness.
            displacement = angle * cardthickness
            # Now we need to displace the new face by that amount
            # in a direction perpendicular to the joining line
            # between the two faces. So first find that direction
            # vector...
            dvec = sub(of.placed[v0], of.placed[v1])
            dvec = [-dvec[1], dvec[0], 0] # turn through 90 degrees
            # ... then make sure we've got the sense right by
            # checking that the centroid of the old face is in the
            # negative direction from here...
            sp = smult(reduce(add, of.placed.values()), 1.0/len(of.placed))
            sp = sub(sp, of.placed[v0])
            if dot(sp, dvec) > 0:
                dvec = smult(dvec, -1)
            # ... and now we can do the translation.
            origin = add(origin, smult(dvec, displacement))
        site(f, mx, origin, xoffs)
    def moveverts(f, v0, v1, vaway, dist):
        # Fudge the placed face f by moving vertices v0 and v1 away
        # from vertex v1, along a vector perpendicular to the line
        # between v0 and v1, by a distance dist.
        
        # So find that vector...
        dvec = sub(f.placed[v0], f.placed[v1])
        dvec = [-dvec[1], dvec[0], 0] # turn through 90 degrees
        dvec = normalise(dvec)
        # ... then make sure we've got the sense right by checking
        # that the reference vertex is in the negative direction
        # from here...
        sp = sub(f.placed[vaway], f.placed[v0])
        if dot(sp, dvec) > 0:
            dvec = smult(dvec, -1)
        # ... and now do the translation.
        f.placed[v0][:] = add(f.placed[v0], smult(dvec, dist))
        f.placed[v1][:] = add(f.placed[v1], smult(dvec, dist))
    def go():
        if args[0] == "net":
            centre(gkci, {"G":1,"K":1,"C":1,"I":1}, -1)
            place(bck, gkci, {"B":1,"C":1,"K":1}, -1)
            place(icd, gkci, {"I":1,"C":1,"D":1}, -1)
            place(jbk, bck, {"J":1,"B":1,"K":1}, -1)
            place(dji, icd, {"D":1,"J":2,"I":1}, -1)
            place(jdk, jbk, {"J":1,"D":0,"K":1})
            place(ibj, dji, {"I":1,"B":2,"J":2})
            place(kde, jdk, {"K":1,"D":0,"E":1})
            place(abi, ibj, {"A":2,"B":2,"I":1})
            place(ekf, kde, {"E":1,"K":1,"F":1}, -1)
            place(hia, abi, {"H":1,"I":1,"A":2}, -1)
            place(fkg, ekf, {"F":1,"K":1,"G":1}, -1)
            place(gih, hia, {"G":1,"I":1,"H":1}, -1)
        else:
            centre(gkci, {"G":0,"K":0,"C":0,"I":0})
            place(fkg, gkci, {"F":0,"K":0,"G":0})
            place(gih, gkci, {"G":0,"I":0,"H":0})
            place(ekf, fkg, {"E":0,"K":0,"F":0})
            place(hia, gih, {"H":0,"I":0,"A":+1})
            place(kde, ekf, {"K":0,"D":-1,"E":0}, -1)
            place(abi, hia, {"A":+1,"B":+1,"I":0}, -1)
            place(jdk, kde, {"J":0,"D":-1,"K":0}, -1)
            place(ibj, abi, {"I":0,"B":+1,"J":+1}, -1)
            place(dji, jdk, {"D":-1,"J":0,"I":-1}, -1)
            place(jbk, ibj, {"J":+1,"B":+1,"K":+1}, -1)
            place(icd, dji, {"I":-1,"C":-1,"D":-1}, -1)
            place(bck, jbk, {"B":+1,"C":+1,"K":+1}, -1)
            moveverts(jbk, "J", "K", "B", 2*cardthickness)
            moveverts(bck, "K", "C", "B", 2*cardthickness)

        # Now average out the position of each point which occurs
        # multiple times. (They won't all be in exactly the same
        # place if we're compensating for card thickness.)
        for list in pointpositions:
            vec = smult(reduce(add, list), 1.0/len(list))
            for v in list:
                v[:] = vec
        
        # Compute the centre of the bounding box.
        xmin = ymin = xmax = ymax = None
        for f in allfaces:
            for p in f.placed.values():
                if xmin == None or xmin > p[0]: xmin = p[0]
                if xmax == None or xmax < p[0]: xmax = p[0]
                if ymin == None or ymin > p[1]: ymin = p[1]
                if ymax == None or ymax < p[1]: ymax = p[1]
        xc = (xmax+xmin)/2.0
        yc = (ymax+ymin)/2.0
        print "%!PS-Adobe-1.0"
        print "clippath flattenpath pathbbox"
        print "3 -1 roll add 2 div %.17g sub" % (yc*scale)
        print "3 1 roll add 2 div %.17g sub" % (xc*scale)
        print "exch translate"
        print "0.01 setlinewidth"
        for f in allfaces:
            print "newpath"
            op = "moveto"
            for p in f.points:
                print "%.17g %.17g %s" % (f.placed[p[3]][0]*scale, f.placed[p[3]][1]*scale, op)
                op = "lineto"
            print "closepath"
            if args[0] != "net":
                if args[0] == "net2":
                    offset = 12
                    mult = -1
                else:
                    offset = 0
                    mult = +1
                print "gsave clip"
                corners = [(f.placed[p[3]][0]*scale, f.placed[p[3]][1]*scale, f.xpos[p[3]]) for p in f.points]
                if len(corners) < 4:
                    corners = [corners[0]] + corners
                assert len(corners) == 4
                N = 64
                def interp(corners, x, y):
                    p0 = add(smult(corners[0], x), smult(corners[1], 1-x))
                    p1 = add(smult(corners[3], x), smult(corners[2], 1-x))
                    return add(smult(p0, y), smult(p1, 1-y))
                def colour(x):
                    x = x * 3.0 # scale up so spectrum runs 0..6
                    x = x * mult + offset
                    xi = int(floor(x))
                    x = x - xi
                    xi = ((xi % 6) + 6) % 6
                    ret = [0,0,0]
                    ret[((xi+1)/2) % 3] = 1
                    if xi % 2: x = 1-x
                    ret[(7-xi) % 3] = x
                    return ret
                for cx in range(N):
                    x0 = cx / float(N)
                    x1 = (cx+0.5) / float(N)
                    x2 = (cx+1) / float(N)
                    for cy in range(N):
                        y0 = cy / float(N)
                        y1 = (cy+0.5) / float(N)
                        y2 = (cy+1) / float(N)
                        p0 = interp(corners, x0, y0)
                        p1 = interp(corners, x0, y2)
                        p2 = interp(corners, x2, y2)
                        p3 = interp(corners, x2, y0)
                        pc = interp(corners, x1, y1)
                        print "%g %g %g setrgbcolor" % tuple(colour(pc[2]))
                        print "newpath"
                        print "%.17g %.17g moveto" % (p0[0], p0[1])
                        print "%.17g %.17g lineto" % (p1[0], p1[1])
                        print "%.17g %.17g lineto" % (p2[0], p2[1])
                        print "%.17g %.17g lineto" % (p3[0], p3[1])
                        print "closepath fill"
                print "grestore"
            print "stroke"
        print "showpage"

A = ms(0.00, 0.00, "A")
B = ms(0.25, 0.00, "B")
C = ms(0.50, 0.00, "C")
D = ms(0.75, 0.00, "D")
E = ms(0.00, 1.00, "E")
F = ms(0.25, 1.00, "F")
G = ms(0.50, 1.00, "G")
H = ms(0.75, 1.00, "H")
I = ms(0.80, 0.70, "I")
J = ms(0.00, 0.50, "J")
K = ms(0.20, 0.70, "K")

gkci = quadrilateral(G,K,C,I)
abi = triangle(A,B,I)
ibj = triangle(I,B,J)
jbk = triangle(J,B,K)
ekf = triangle(E,K,F)
bck = triangle(B,C,K)
icd = triangle(I,C,D)
fkg = triangle(F,K,G)
gih = triangle(G,I,H)
hia = triangle(H,I,A)
dji = triangle(D,J,I)
jdk = triangle(J,D,K)
kde = triangle(K,D,E)

go()
