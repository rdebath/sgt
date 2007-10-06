#!/usr/bin/env python2.1

import sys
import string
from math import sin, cos, acos, pi, sqrt
import gzip
import xmllib

def read(fname, xrotation, yrotation, xmod=+1):
    class container:
        pass
    c = container()
    def open_object(attributes, c=c):
        typ = attributes["http://www.lysator.liu.se/~alla/dia/ type"]
        if typ == "Standard - Beziergon":
            c.typ = "bez"
            c.bezpoints = None
            del c.bezpoints
        elif typ == "Standard - Ellipse":
            c.typ = "ell"
            c.corner = c.width = c.height = None
            del c.corner, c.width, c.height
        else:
            c.typ = "UNKNOWN"
        c.fillcolour = "1 setgray"
        c.use = 1
    def close_object(c=c):
        if not c.use:
            return
        if c.typ == "bez":
            c.shapes.append(("bez", c.fillcolour, c.bezpoints))
        elif c.typ == "ell":
            c.shapes.append(("ell", c.fillcolour, c.corner, c.width, c.height))
    def open_attr(attributes, c=c):
        c.attr = attributes["http://www.lysator.liu.se/~alla/dia/ name"]
        c.pointlist = []
        c.real = None
        c.bool = None
        c.colour = None
    def close_attr(c=c):
        if c.attr == "elem_corner":
            assert len(c.pointlist) == 1
            c.corner = c.pointlist[0]
        elif c.attr == "elem_width":
            assert c.real != None
            c.width = c.real
        elif c.attr == "elem_height":
            assert c.real != None
            c.height = c.real
        elif c.attr == "show_background":
            assert c.bool != None
            c.use = c.bool
        elif c.attr == "bez_points":
            assert len(c.pointlist) > 0 and len(c.pointlist) % 3 == 0
            c.bezpoints = c.pointlist
        elif c.attr == "inner_color":
            assert c.colour != None
            c.fillcolour = c.colour
    def open_point(attributes, c=c):
        s = attributes["http://www.lysator.liu.se/~alla/dia/ val"]
        i = s.index(",")
        x = string.atof(s[:i])
        y = string.atof(s[i+1:])
        c.pointlist.append((x,y))
    def open_real(attributes, c=c):
        s = attributes["http://www.lysator.liu.se/~alla/dia/ val"]
        r = string.atof(s)
        c.real = r
    def open_bool(attributes, c=c):
        s = attributes["http://www.lysator.liu.se/~alla/dia/ val"]
        b = (s == "true")
        c.bool = b
    def open_colour(attributes, c=c):
        s = attributes["http://www.lysator.liu.se/~alla/dia/ val"]
        assert len(s) == 7 and s[0] == "#"
        r = string.atoi(s[1:3], 16)
        g = string.atoi(s[3:5], 16)
        b = string.atoi(s[5:7], 16)
        if r == g == b:
            c.colour = repr(r) + " setgray"
        else:
            c.colour = repr(r) + " " + repr(g) + " " + repr(b) + " setrgbcolor"
    x = xmllib.XMLParser()
    x.elements = {}
    x.elements["http://www.lysator.liu.se/~alla/dia/ object"] = (open_object, close_object)
    x.elements["http://www.lysator.liu.se/~alla/dia/ attribute"] = (open_attr, close_attr)
    x.elements["http://www.lysator.liu.se/~alla/dia/ real"] = (open_real, None)
    x.elements["http://www.lysator.liu.se/~alla/dia/ point"] = (open_point, None)
    x.elements["http://www.lysator.liu.se/~alla/dia/ boolean"] = (open_bool, None)
    x.elements["http://www.lysator.liu.se/~alla/dia/ color"] = (open_colour, None)
    c.shapes = []
    f = gzip.open(fname)
    while 1:
        s = f.readline()
        if s == "": break
        x.feed(s)
    f.close()

    for shape in c.shapes:

        # List of points for the flattened form of this shape.
        points = []

        if shape[0] == "ell":
            name, colour, (x, y), width, height = shape
            rx = width/2
            ry = height/2
            rm = max(rx,ry)
            x = x + rx
            y = y + ry  # so (x,y) is now the centre rather than the top left
            angle = 2*acos(1 - flatness/rm)
            n = 2*pi / angle
            n = int(n/4+1)*4
            for i in range(n):
                theta = i * 2*pi / n
                points.append((x + rx * cos(theta), y + ry * sin(theta)))
        elif shape[0] == "bez":
            name, colour, bezpoints = shape
            points = []

            def docurve(a,b,c,d,points,docurve):
                # To render a Bezier curve, we repeatedly subdivide
                # it until it's in sufficiently flat pieces.
                #
                # A Bezier curve is given by the equation
                #
                #   bez(a,b,c,d,t) = (1-t)^3 a
                #                  + 3 t (1-t)^2 b
                #                  + 3 t^2 (1-t) c
                #                  + t^3 d
                #
                # as t ranges over [0,1]. So we can obtain a Bezier
                # curve which is identical to the first half of
                # this one by simply writing down the equation
                #
                #   bez(a,b,c,d,t) = bez(p,q,r,s,2t)
                #
                # and solving it for p,q,r,s to give
                #
                #   p = a
                #   q = (a+b)/2
                #   r = (a+2b+c)/4
                #   s = (a+3b+3c+d)/8
                #
                # Similarly, to get the second half of the curve,
                # we solve bez(a,b,c,d,t) = bez(p,q,r,s,2t-1) to
                # obtain the symmetric counterpart to the above
                #
                #   p = (a+3b+3c+d)/8
                #   q = (b+2c+d)/4
                #   r = (c+d)/2
                #   s = d

                # First see whether this curve is small enough to
                # be approximated by a straight line anyway. We
                # know the curve lies entirely within the convex
                # hull of the point set {a,b,c,d}, so a sufficient
                # condition for this is if both b and c are within
                # the tolerance distance of the line between a and
                # d.
                dpb = (b[0]-a[0]) * (d[1]-a[1]) - (b[1]-a[1]) * (d[0]-a[0])
                dpc = (c[0]-a[0]) * (d[1]-a[1]) - (c[1]-a[1]) * (d[0]-a[0])
                dist = max(abs(dpb), abs(dpc)) / sqrt((d[0]-a[0])**2 + (d[1]-a[1])**2)
                if dist <= flatness:
                    points.append(d)
                    return
                p = a
                q = (a[0]+b[0])/2, (a[1]+b[1])/2
                r = (a[0]+2*b[0]+c[0])/4, (a[1]+2*b[1]+c[1])/4
                s = (a[0]+3*b[0]+3*c[0]+d[0])/8, (a[1]+3*b[1]+3*c[1]+d[1])/8
                docurve(p, q, r, s, points, docurve)
                p = (a[0]+3*b[0]+3*c[0]+d[0])/8, (a[1]+3*b[1]+3*c[1]+d[1])/8
                q = (b[0]+2*c[0]+d[0])/4, (b[1]+2*c[1]+d[1])/4
                r = (c[0]+d[0])/2, (c[1]+d[1])/2
                s = d
                docurve(p, q, r, s, points, docurve)

            points.append(bezpoints[0])
            for i in range(len(bezpoints)/3):
                a = bezpoints[i*3]
                b = bezpoints[i*3+1]
                c = bezpoints[i*3+2]
                d = bezpoints[(i*3+3) % len(bezpoints)]
                docurve(a,b,c,d,points,docurve)

        if len(points) == 0:
            continue

        sx = sy = sz = 0
        points3d = []
        for x, y in points:
            # Scale down to radius 1.
            x, y = x / radius, y / radius
            # Flip the x coordinate.
            x = x * xmod
            # The y coordinate always needs flipping, since Dia
            # works top to bottom and PS works bottom to top.
            y = -y
            # Invent the z coordinate.
            z = -sqrt(1 - x**2 - y**2)
            # Rotate about the x-axis.
            phi = xrotation * pi/180
            y, z = y*cos(phi)+z*sin(phi), -y*sin(phi)+z*cos(phi)
            # Rotate about the y-axis.
            phi = yrotation * pi/180
            x, z = x*cos(phi)+z*sin(phi), -x*sin(phi)+z*cos(phi)
            points3d.append((x,y,z))
            sx, sy, sz = sx+x, sy+y, sz+z

        # Find the centroid of the shape by normalising the sum of
        # the points.
        sl = sqrt(sx*sx+sy*sy+sz*sz)
        cx, cy, cz = sx/sl, sy/sl, sz/sl

        print "{"+colour+"}", cx, cy, cz, "newshape"
        for x, y, z in points3d:
            print x, y, z, "point"

flatness = 0.001
radius = 5.0
read("b.dia", 0, 0)
read("c.dia", 0, 90)
read("c.dia", 0, -90, -1)
read("d.dia", 0, 180)
read("e.dia", 46, 35)
read("e.dia", 46, -35, -1)
print "{} 0 0 0 newshape"
