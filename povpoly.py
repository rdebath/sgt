#!/usr/bin/env python

# Read in a polyhedron description, and output a POV-Ray fragment
# which describes it for raytracing purposes.

import sys
import os
import string
import random
from math import pi, asin, atan2, cos, sin, sqrt
from crosspoint import crosspoint

args = sys.argv[1:]

picfile = None
picres = 1024

while len(args) > 0 and args[0][:1] == "-":
    a = args[0]
    args = args[1:]

    # FIXME: IWBNI we could introduce some options here for
    # non-picture-based polyhedron colouring. For a start, simply
    # specifying a different solid colour would be good. Also, how
    # about something more fun, such as colouring different
    # _classes_ of face? By number of vertices, for instance. Or,
    # at the very least, permit individual faces to be coloured by
    # specifying them by name.

    if a == "--":
        break
    elif a[:2] == "-p":
        # Undocumented option which allows you to specify a file
        # containing a PostScript fragment. That file is included
        # at the top of the output, and is expected to define a
        # function called `picture'.
        #
        # `picture' will then be called once for each face of the
        # polyhedron, and passed two arguments. The first argument
        # will be an orthogonal matrix (a length-9 array listing
        # the elements of the matrix ordered primarily by columns)
        # and the second a height. When called, the origin and
        # clipping path will be set appropriately for `picture' to
        # transform some sort of spherical graphical description
        # using the given matrix, project it on to the plane
        # (z=height), and draw it. This allows a net to be drawn
        # which folds up to produce a polyhedron covered in a
        # projection of the original spherical image (e.g. an
        # icosahedral globe).
        picfile = a[2:]
    else:
        sys.stderr.write("ignoring unknown option \"%s\"\n", a)

if len(args) > 0:
    infile = open(args[0], "r")
    args = args[1:]
else:
    infile = sys.stdin

if len(args) > 0:
    imgbasename = args[0]
    outfile = open(args[0], "w")
    args = args[1:]
else:
    imgbasename = "povpoly." + str(os.getpid())
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

def realprint(a, outfile):
    for i in range(len(a)):
        outfile.write(str(a[i]))
        if i < len(a)-1:
            outfile.write(" ")
        else:
            outfile.write("\n")

def povprint(*a):
    realprint(a, outfile)

def psprint(*a):
    realprint(a, psfile)

def normalise(vec):
    vlen = sqrt(vec[0]**2 + vec[1]**2 + vec[2]**2)
    if vlen == 0:
        return None
    else:
        return [vec[0]/vlen, vec[1]/vlen, vec[2]/vlen]

def mattrans(matrix, vec):
    ret = [matrix[0]*vec[0] + matrix[1]*vec[1] + matrix[2]*vec[2],
    matrix[3]*vec[0] + matrix[4]*vec[1] + matrix[5]*vec[2],
    matrix[6]*vec[0] + matrix[7]*vec[1] + matrix[8]*vec[2]]
    if len(matrix) > 9:
        ret = [ret[0]+matrix[9], ret[1]+matrix[10], ret[2]+matrix[11]]
    return ret

# Draw each face. To do this we first transform the solid so that
# that face is in the x-y plane and contained within [0,1]; this is
# not strictly necessary when outputting a plain polyhedron, but
# becomes important when we map an image on to each face.
povprint("union {")
for f in faces.keys():
    n = len(faces[f])

    # Compute the matrix we're going to use to transform the [0,1]
    # square into our final polygon. We start by mapping the z unit
    # vector to our surface normal.
    zvec = normalise(normals[f])
    # Now we find an arbitrary vector perpendicular to that, by
    # taking the cross product with two unit vectors and picking
    # a result which is non-zero.
    xvec = normalise([0, -zvec[2], zvec[1]])
    if xvec == None:
        xvec = normalise([zvec[2], 0, -zvec[0]])
    # And now we take the cross product of those to find our final
    # vector.
    yvec = [xvec[1]*zvec[2]-xvec[2]*zvec[1],
    xvec[2]*zvec[0]-xvec[0]*zvec[2],
    xvec[0]*zvec[1]-xvec[1]*zvec[0]]
    # Now we've got our matrix.
    matrix = [xvec[0], yvec[0], zvec[0],
    xvec[1], yvec[1], zvec[1],
    xvec[2], yvec[2], zvec[2]]
    # Since this is an orthogonal matrix, its inverse is its
    # transpose.
    matrixinv = xvec + yvec + zvec

    # Applying the above matrix transformation to all points
    # _should_ have mapped all the z-coordinates to the same value
    # (although we don't know, or really care, what that value is).
    # Just to be sure, though, we'll average them out now and set
    # them all to _exactly_ the same value.
    #
    # In this loop we also determine the minimum and maximum x- and
    # y-coordinates, so we can stretch our face into [0,1]^2.
    plist = []
    ztotal = 0
    xmin = xmax = None
    ymin = ymax = None
    #zmin = zmax = None
    for v in faces[f]:
        pt = mattrans(matrixinv, vertices[v])
        plist.append(pt)
        ztotal += pt[2]
        if xmin == None or xmin > pt[0]: xmin = pt[0]
        if xmax == None or xmax < pt[0]: xmax = pt[0]
        if ymin == None or ymin > pt[1]: ymin = pt[1]
        if ymax == None or ymax < pt[1]: ymax = pt[1]
        #if zmin == None or zmin > pt[2]: zmin = pt[2]
        #if zmax == None or zmax < pt[2]: zmax = pt[2]
    zaverage = ztotal / float(n)
    #print "zspread", zmin-zaverage, zmax-zaverage

    scale = max(xmax,-xmin,ymax,-ymin) * 2.0

    # If we're drawing a picture on the polyhedron, we now have all
    # the information we need to make a sub-call to PostScript to
    # render the image for this face.
    if picfile != None:
        facebasename = imgbasename + "." + f
        facepsname = facebasename + ".ps"
        facepngname = facebasename + ".png"
        psfile = open(facepsname, "w")
        pic = open(picfile, "r")
        while 1:
            s = pic.readline()
            if s == "": break
            psfile.write(s)
        pic.close()
        psprint("36 dup translate")
        psprint("%.17g dup scale" % (72.0/scale))
        psprint("[")
        psprint("%.17g %.17g %.17g" % (matrixinv[0], matrixinv[3], matrixinv[6]))
        psprint("%.17g %.17g %.17g" % (matrixinv[1], matrixinv[4], matrixinv[7]))
        psprint("%.17g %.17g %.17g" % (matrixinv[2], matrixinv[5], matrixinv[8]))
        psprint("] %.17g picture" % zaverage)
        psprint("showpage")
        psfile.close()
        cmd = "gs -sDEVICE=png16m -sOutputFile=%s -r%d -g%dx%d -dBATCH -dNOPAUSE -q %s" % (facepngname, picres, picres, picres, facepsname)
        sys.stderr.write(cmd + "\n")
        os.system(cmd)
        pigment = "image_map { png \"%s\" }" % facepngname
    else:
        pigment = "solid White"

    # Now stretch the x and y coordinates so that the polygon fits
    # in [-1/2,1/2]^2, and then translate it by (1/2,1/2) so that
    # it fits in [0,1]^2. While we're at it, we might as well move
    # the z-coordinate to the origin.
    for k in range(3):
        matrixinv[k+0] = matrixinv[k+0] / scale
        matrixinv[k+3] = matrixinv[k+3] / scale
        matrix[k*3+0] = matrix[k*3+0] * scale
        matrix[k*3+1] = matrix[k*3+1] * scale
    trans = [-0.5,-0.5,zaverage]

    povprint("  polygon {");
    points = str(n+1)
    for i in range(-1,n):
        pt = mattrans(matrixinv, vertices[faces[f][i]])
        pt = [pt[0]-trans[0], pt[1]-trans[1], pt[2]-trans[2]]
        points = points + ", <%.17g,%.17g,0>" % (pt[0], pt[1])
    povprint("    %s" % points)
    povprint("    pigment { %s }" % pigment)
    povprint("    matrix <")
    povprint("      %.17g, %.17g, %.17g," % (matrix[0],matrix[3],matrix[6]))
    povprint("      %.17g, %.17g, %.17g," % (matrix[1],matrix[4],matrix[7]))
    povprint("      %.17g, %.17g, %.17g," % (matrix[2],matrix[5],matrix[8]))
    povprint("      %.17g, %.17g, %.17g" % tuple(mattrans(matrix, trans)))
    povprint("    >")
    povprint("  }")
povprint("}")
