#!/usr/bin/env python

# Compute successive fractal frames for a Newton-Raphson fractal
# animation.
#
# This could easily be adapted into a general rendering manager
# which ran a subcommand to generate a PPM for each video frame and
# scheduled enough of them to keep the CPUs occupied. However, I'm
# doing this one specifically for N-R because I want the whole
# program to only have to parse the root choreography once.

import os
import sys
import string
import select
import fcntl
import cmath
from math import *

import polyfac

def atotime(s):
    i = string.find(s, ":")
    if i >= 0:
        return string.atof(s[:i]) * 60 + string.atof(s[i+1:])
    else:
        return string.atof(s)

fps = 25
width = 720
height = 576
aspect = 4.0/3.0
mode = "normal"

preview = 0
verbose = 0
dryrun = 0
maxframes = None
frameoffset = 0

for arg in sys.argv[1:]:
    if arg == "-p":
        preview = 1
    elif arg[:2] == "-o":
        frameoffset = atotime(arg[2:])
    elif arg[:2] == "-O":
        frameoffset = float(string.atoi(arg[2:])) / fps
    elif arg[:2] == "-l":
        maxframes = int(atotime(arg[2:]) * fps)
    elif arg[:2] == "-L":
        maxframes = string.atoi(arg[2:])
    elif arg[:2] == "-v":
        verbose = 1
    elif arg[:2] == "-n":
        dryrun = 1
    elif arg == "--ntsc":
        fps = 30
        height = 480
    elif arg == "--widescreen":
        aspect = 16.0/9.0
    elif arg[:2] == "-s":
        width = width / 2
        height = height / 2
    elif arg == "--chapters":
        mode = "chapters"
    elif arg == "--chaptermenu":
        mode = "chaptermenu"
    elif arg == "--total":
        mode = "total"
    elif arg == "--totalframes":
        mode = "totalframes"

# Root colours.
RED = (1, 0, 0)
ORANGE = (1, 0.7, 0)
YELLOW = (1, 1, 0)
GREEN = (0, 0.7, 0)
BLUE = (0, 0.7, 1)
INDIGO = (0, 0, 1)
VIOLET = (0.5, 0, 1)
# Special colours.
FADE = (None, 1) # governs the overall fading in and out of the image
POWER = (None, 2) # governs the -p option (convergence inhibitor)
TITLE = (None, 3) # produces an opening title card at this brightness
CREDITS = (None, 4) # produces the credits starting this far down the picture

intervals = {}

chapters = []

def chaptermark(T, s, p):
    chapters.append((T, s, p))

def fmttime(time):
    minutes = int(time / 60)
    return "%d:%05.2f" % (minutes, time - 60*minutes)

def polar(r,theta):
    return (r*cos(theta), r*sin(theta))

def addinterval(point, t0, t1, torig, coordfunc):
    intervals[point] = intervals.get(point, []) + [(t0, t1, torig, coordfunc)]

def preparedata():
    for v in intervals.values():
        v.sort()

def pointcoords(point, t):
    list = intervals[point]
    # Binary-search to find the last interval whose start point
    # comes before t.
    bottom = -1
    top = len(list)
    while top - bottom > 1:
        mid = int((bottom + top) / 2)
        if list[mid][0] <= t:
            bottom = mid
        else:
            top = mid
    # Now we expect between 0 and 2 intervals containing the point.
    i1 = i2 = None
    if bottom >= 0 and list[bottom][0] <= t and t < list[bottom][1]:
        i1 = list[bottom]
        if bottom >= 1 and list[bottom-1][0] <= t and t < list[bottom-1][1]:
            i2 = i1
            i1 = list[bottom-1]
    # If we have no intervals, return no coordinate.
    if i1 == None:
        return None
    # If we have one interval, return the coordinates given by that
    # one.
    c1 = i1[3](t - i1[2])
    if i2 == None:
        return c1
    # Otherwise, compute the coordinates given by both intervals,
    # and blend.
    c2 = i2[3](t - i2[2])
    assert i2[0] < i1[1] # this is the interval on which we blend
    return blendcoords(c1, c2, i1[1]-t, t-i2[0])

def blendcoords(c1, c2, w1, w2):
    p1 = w1 / (w1+w2)
    p1 = 3*p1**2 - 2*p1**3
    p2 = 1 - p1
    assert len(c1) == len(c2)
    ret = ()
    for i in range(len(c1)):
        ret = ret + (c1[i] * p1 + c2[i] * p2,)
    return ret

# To make it easier to bring points in from near infinity, we apply
# a transformation to the coordinate space so that a circle of
# radius 5 is expanded to cover the whole of C.
def project(c):
    norm = sqrt(sum([x*x for x in c]))
    if norm == 0:
        scale = 1
    else:
        nn = norm * pi / 10
        scale = tan(nn) / nn
    ret = ()
    for i in range(len(c)):
        ret = ret + (c[i] * scale,)
    return ret

# Sequences requiring very precise point positioning - Orbits and
# Wheels - need to specify their points in the form they should
# take _after_ project() is applied. Hence, they need to compute
# its inverse.
def antiproject(c):
    norm = sqrt(sum([x*x for x in c]))
    if norm == 0:
        scale = 1
    else:
        nn = norm * pi / 10
        scale = atan(nn) / nn
    ret = ()
    for i in range(len(c)):
        ret = ret + (c[i] * scale,)
    return ret

def findpoints(t):
    # Find all the points we currently know about.
    coords = []
    fadelevel = 1.0
    power = 1.0
    title = credits = None
    for point in intervals.keys():
        c = pointcoords(point, t)
        if c != None:
            # See if this is a normal point or a special one.
            if point[0] != None:
                # Normal point. Project it and add it to the list.
                coords.append((point, project(c)))
            elif point[1] == 1:
                # FADE.
                fadelevel = c[0]
            elif point[1] == 2:
                # POWER.
                power = c[0]
            elif point[1] == 3:
                # TITLE.
                title = c[0]
            elif point[1] == 4:
                # CREDITS.
                credits = c[0]
    # Now adjust the colours by the fade level, and return.
    return power, title, credits, [((r*fadelevel, g*fadelevel, b*fadelevel), x, y) for ((r,g,b),(x,y)) in coords]

# Actual choreography.

T = 0

chaptermark(T,"Opening Titles", 8)
# Fade the title card up, then down again.
addinterval(TITLE, T, T+2, T, lambda t: (0,))
addinterval(TITLE, T+1, T+7, T, lambda t: (1,))
addinterval(TITLE, T+6, T+8, T, lambda t: (0,))
T = T + 8

Tstart = T

# We open with just two points sitting at -1 and +1, and have the
# third come in from infinity to move around them in a figure-eight
# weave pattern.

addinterval(RED, T, T+7, T, lambda t: (0,4.99999))

chaptermark(T+7,"Weave", 10)
U = T + 120
addinterval(YELLOW, T, U+2, T, lambda t: (1, 0))
addinterval(GREEN, T, U+2, T, lambda t: (-1, 0))
addinterval(RED, T+4, U+2, T, lambda t: (1.5*sin(2*pi*t/10), sin(4*pi*t/10)))
T = U

# Shift into the classic cascade pattern: three points _all_ moving
# around a figure-eight curve.
chaptermark(T+2,"Cascade", 10)
U = T + 180
addinterval(RED, T-2, U+2, T, lambda t: (sin(2*pi*t/10), sin(4*pi*t/10)))
addinterval(YELLOW, T-2, U+2, T, lambda t: (sin(2*pi*(t/10 + 1.0/3.0)), sin(4*pi*(t/10 + 1.0/3.0))))
addinterval(GREEN, T-2, U+1, T, lambda t: (sin(2*pi*(t/10 + 2.0/3.0)), sin(4*pi*(t/10 + 2.0/3.0))))
T = U

# Improve the cascade->Mills transition.
addinterval(GREEN, T-1, T+2, T, lambda t: polar(1.5, (t+12)*pi/8))

chaptermark(T+2,"Mills", 10)
U = T + 122
addinterval(RED, T-2, U+2, T, lambda t: (sin(-2*pi*t/10), sin(-4*pi*t/10)))
addinterval(YELLOW, T-2, U+2, T, lambda t: (sin(-2*pi*(t/10 - 1.0/6.0)), sin(-4*pi*(t/10 - 1.0/6.0))))
addinterval(GREEN, T+1, U+2, T, lambda t: (sin(-2*pi*(t/10 - 2.0/6.0)), sin(-4*pi*(t/10 - 2.0/6.0))))
T = U

# Put two of the points back at stationary locations, and this time
# just loop the third around the outside.
chaptermark(T+2,"Loop", 10)
U = T + 118
addinterval(GREEN, T-2, U+2, T, lambda t: (1, 0))
addinterval(YELLOW, T-2, U+2, T, lambda t: (-1, 0))
addinterval(RED, T-2, U+2, T, lambda t: (lambda theta: polar(cos(theta)**2 + 0.6, -theta))((lambda t: t-0.3*sin(2*t))(2*pi*(t/10+0.4))))
T = U

# Back to the cascade temporarily in order to change over to the fountain.
chaptermark(T+2,"Cascade Reprise", 10)
U = T + 25
addinterval(RED, T-2, U+2, T-5, lambda t: (sin(2*pi*t/10), sin(4*pi*t/10)))
addinterval(YELLOW, T-2, U+2, T-5, lambda t: (sin(2*pi*(t/10 + 1.0/3.0)), sin(4*pi*(t/10 + 1.0/3.0))))
addinterval(GREEN, T-2, U+2, T-5, lambda t: (sin(2*pi*(t/10 + 2.0/3.0)), sin(4*pi*(t/10 + 2.0/3.0))))
T = U

# Now shift into an asynchronous fountain: four points moving
# around two non-overlapping ellipses. For this we must bring in
# BLUE from the left-hand side.
chaptermark(T+3,"Fountain", 5)
U = T + 120
addinterval(BLUE, T-1, T+3, T, lambda t: (-4.999999, 0))
addinterval(RED, T-2, U+2, T, lambda t: (1.2+0.7*sin(2*pi*t/5), cos(2*pi*t/5)))
addinterval(YELLOW, T-2, U+2, T, lambda t: (1.2+0.7*sin(2*pi*(t/5+0.5)), cos(2*pi*(t/5+0.5))))
addinterval(GREEN, T-2, U+2, T, lambda t: (-1.2-0.7*sin(2*pi*(t/5+0.25)), cos(2*pi*(t/5+0.25))))
addinterval(BLUE, T+1, U+2, T, lambda t: (-1.2-0.7*sin(2*pi*(t/5+0.75)), cos(2*pi*(t/5+0.75))))
T = U

# Points interchanging between corners of a square.
chaptermark(T+2, "Square Dance", 10)
U = T + 184
sqd_K = sin(pi/4) / (sin(pi/4)-sin(pi/4)**3*0.75)
addinterval(BLUE, T-2, U+2, T, lambda t: (-sin(2*pi*t/10), -cos(2*pi*t/10)))
addinterval(GREEN, T-2, U+2, T, lambda t: (-sqd_K*(sin(2*pi*t/10)-sin(2*pi*t/10)**3*0.75), cos(2*pi*t/10)))
addinterval(YELLOW, T-2, U+2, T, lambda t: (sin(2*pi*t/10), -sqd_K*(cos(2*pi*t/10)-cos(2*pi*t/10)**3*0.75)))
addinterval(RED, T-2, U+2, T, lambda t: (sqd_K*(sin(2*pi*t/10)-sin(2*pi*t/10)**3*0.75), sqd_K*(cos(2*pi*t/10)-cos(2*pi*t/10)**3*0.75)))
T = U
# Then we shift into a version in which the two interchanges are
# translationally similar rather than reflectively.
chaptermark(T+2, "Square Dance II", 10)
U = T + 180
def sqd2(f, x, s):
    if f(x) * s < 0:
        return f(x)
    else:
        return sqd_K * (f(x) - f(x)**3*0.75)
addinterval(GREEN, T-2, U+2, T-1.25, lambda t: (-sqd2(sin,2*pi*t/10,+1), -sqd2(cos,2*pi*t/10,-1)))
addinterval(BLUE, T-2, U+2, T-1.25, lambda t: (-sqd2(sin,2*pi*t/10,-1), sqd2(cos,2*pi*t/10,+1)))
addinterval(RED, T-2, U+1, T-1.25, lambda t: (sqd2(sin,2*pi*t/10,-1), -sqd2(cos,2*pi*t/10,+1)))
addinterval(YELLOW, T-2, U+1, T-1.25, lambda t: (sqd2(sin,2*pi*t/10,+1), sqd2(cos,2*pi*t/10,-1)))
T = U
# And next we interchange diagonally, not quite allowing the points
# to intersect.
chaptermark(T+2, "Square Dance III", 10)
U = T + 190
def sqd3(f, x, threshold):
    k = f(x)
    sign = 1
    if k > threshold:
        k = k - threshold
    elif k < -threshold:
        k = k + threshold
        sign = -1
    else:
        k = 0
    return sign * abs(k) * abs(k)
addinterval(YELLOW, T-2, U+3.25, T-1.25, lambda t: (1.5*sin(2*pi*t/10)-10*sqd3(cos,2*pi*t/10,0.9), 1.5*sin(2*pi*t/10)+10*sqd3(cos,2*pi*t/10,0.9)))
addinterval(RED, T-3, U+3.25, T-1.25, lambda t: (-1.5*sin(2*pi*t/10)+10*sqd3(cos,2*pi*t/10,0.9), -1.5*sin(2*pi*t/10)-10*sqd3(cos,2*pi*t/10,0.9)))
addinterval(GREEN, T-3, U+3.25, T-1.25, lambda t: (1.5*cos(2*pi*t/10)-10*sqd3(sin,2*pi*t/10,0.9), -1.5*cos(2*pi*t/10)-10*sqd3(sin,2*pi*t/10,0.9)))
addinterval(BLUE, T-2, U+0.5, T-1.25, lambda t: (-1.5*cos(2*pi*t/10)+10*sqd3(sin,2*pi*t/10,0.9), 1.5*cos(2*pi*t/10)+10*sqd3(sin,2*pi*t/10,0.9)))
T = U

# Something a bit different. Let's put three points in a triangular
# formation, and have a fourth weaving in and out of them.

def clover(n, t): # one leaf every time t increments by pi
    return polar(sin(t), -(2*t - sin(2*t))/(2*n))

chaptermark(T+4,"Clover Simple", 10)
U = T + 120
addinterval(YELLOW, T-0.75, U+2, T, lambda t: polar(0.6, 3*pi/6))
addinterval(RED, T-0.75, U+2, T, lambda t: polar(0.6, 7*pi/6))
addinterval(GREEN, T-0.75, U+2, T, lambda t: polar(0.6, 11*pi/6))
# And the blue moves in a 3-leaf clover curve, 6 leaves in 10 seconds.
addinterval(BLUE, T-2, U+2, T, lambda t: clover(3, (6*t/10)*pi))
T = U

# Bring in a fifth point to follow the blue one around its arc.

addinterval(VIOLET, T-3, T+2, T, lambda t: (0, -4.999999))

chaptermark(T+2,"Clover Double", 10)
U = T + 120
addinterval(YELLOW, T-2, U+2, T, lambda t: polar(0.6, 3*pi/6))
addinterval(RED, T-2, U+2, T, lambda t: polar(0.6, 7*pi/6))
addinterval(GREEN, T-2, U+2, T, lambda t: polar(0.6, 11*pi/6))
addinterval(BLUE, T-2, U+2, T, lambda t: clover(3, (6*t/10)*pi))
addinterval(VIOLET, T-2, U+2, T, lambda t: clover(3, (6*t/10+1.5)*pi))
T = U

# Shift to a pattern in which all five points move around the same
# curve. This gets a bit frantic, so we'll slow it down at this
# point.

chaptermark(T+2,"Clover Complex", 20)
U = T + 120
addinterval(YELLOW, T-2, U+2, T, lambda t: clover(3, (6*t/20+1.2)*pi))
addinterval(RED, T-2, U+2, T, lambda t: clover(3, (6*t/20+1.8)*pi))
addinterval(GREEN, T-2, U+2, T, lambda t: clover(3, (6*t/20+2.4)*pi))
addinterval(BLUE, T-2, U+2, T, lambda t: clover(3, (6*t/20)*pi))
addinterval(VIOLET, T-2, U+2, T, lambda t: clover(3, (6*t/20+0.6)*pi))
T = U

# That's quite enough of that. Now let's have something simpler:
# three points moving around a circle one way, the other two in a
# smaller circle going the other way.
chaptermark(T+2,"Circles", 15)
U = T + 120
addinterval(RED, T-2, U+2, T, lambda t: polar(1.0,2*pi*t/15))
addinterval(YELLOW, T-2, U+2, T, lambda t: polar(1.0,2*pi*(t/15 + 1.0/3)))
addinterval(GREEN, T-2, U+2, T, lambda t: polar(1.0,2*pi*(t/15 + 2.0/3)))
addinterval(BLUE, T-2, U+2, T, lambda t: polar(0.4,-2*pi*t/15))
addinterval(VIOLET, T-2, U+2, T, lambda t: polar(0.4,-2*pi*(t/15 + 1.0/2)))
T = U

# Now let's try something a bit different, again. The fractals
# generated by the Newton process are often nicer when there's a
# point where three regions meet, rather than single dividing
# lines. This occurs, I've found, wherever the _derivative_ of the
# polynomial has a repeated root.
#
# So now I want to display five points moving in such a way that
# they _always_ maintain such a point at the origin.
#
# If we get Maxima to multiply out (z-a)(z-b)(z-c)(z-d)(z-e) and
# solve, we find that to have a _single_ root of the derivative at
# the origin we need 1/e = -1/a-1/b-1/c-1/d. To arrange a second
# root, we have to solve a quadratic in d given by the hefty
# formula
#
#       ab + bc + ab + sqrt(-3(b^2c^2+a^2b^2+a^2c^2)-2(abc^2+ab^2c+a^2bc))
#   d = ------------------------------------------------------------------
#                           -2(ab/c+ac/b+bc/a+a+b+c)
#
# and the trouble with that is that we need the use of sqrt() never
# to cross a branch cut or we'll have a discontinuously moving d.
#
# So what we have to do is to first decide on our paths for a,b,c;
# then plot the argument of the _operand_ to sqrt() against time,
# and see how it evolves. Then we can decide how to swing the
# branch cut so that we never run into it, and then we can compute
# a continuous d.

chaptermark(T+2, "Orbits", 20)
U = T + 120
orbits_epsilon = 0.5
def orbits_inner(theta, thetaoff):
    r = (1-orbits_epsilon**2)/(1 + orbits_epsilon*cos(theta))
    return r * cmath.exp(1j*(theta+thetaoff))
def orbits_a(t):
    return orbits_inner(t + 7*pi/6, 7*pi/6)
def orbits_b(t):
    return orbits_inner(t + 11*pi/6, 11*pi/6)
def orbits_c(t):
    return orbits_inner(t + 3*pi/6, 3*pi/6)
def orbits_sqrinner(t):
    # This is the operand to the square root function used in
    # computing d.
    a = orbits_a(t)
    b = orbits_b(t)
    c = orbits_c(t)
    return -3*b**2*c**2-2*a*b*c**2-3*a**2*c**2-2*a*b**2*c-2*a**2*b*c-3*a**2*b**2
def orbits_sqrfollow(t):
    # This is a continuous function of t with the property that its
    # square is never a negative real multiple of sqrinner. I found
    # this by temporarily plotting sqrinner as a moving point, and
    # judging by eye what to use for sqrfollow.
    return cmath.exp(1j*(5*t - pi/2) / 2)
def orbits_d(t):
    a = orbits_a(t)
    b = orbits_b(t)
    c = orbits_c(t)
    sqrinner = orbits_sqrinner(t)
    sqrfollow = orbits_sqrfollow(t)
    # We now find a square root of sqrinner in such a way that the
    # branch cut is opposite sqrfollow^2, and by construction of
    # sqrfollow this means we never cross it.
    root = sqrfollow * cmath.sqrt(sqrinner / sqrfollow**2)
    # And now work out d proper, using the square root we just
    # computed.
    return (root+b*c+a*c+a*b)/(-2*b*c/a-2*a*c/b-2*c-2*a*b/c-2*b-2*a)
def orbits_e(t):
    a = orbits_a(t)
    b = orbits_b(t)
    c = orbits_c(t)
    d = orbits_d(t)
    return -1/(1.0/a+1.0/b+1.0/c+1.0/d)
def c2xy(z):
    return antiproject((z.real, z.imag))
addinterval(RED, T, U+2, T, lambda t: c2xy(orbits_a(2*pi*t/10.0)))
addinterval(YELLOW, T, U+2, T, lambda t: c2xy(orbits_b(2*pi*t/10.0)))
addinterval(GREEN, T, U+2, T, lambda t: c2xy(orbits_c(2*pi*t/10.0)))
# By plotting this temporarily...
# addinterval(BLUE, T, U, T, lambda t: c2xy(orbits_sqrinner(2*pi*t/10.0)))
# ... I determine this value which follows it around so that the
# two points are never on exactly opposite sides of zero...
# addinterval(VIOLET, T, U, T, lambda t: c2xy(orbits_sqrfollow(2*pi*t/10.0)**2))
# ... and then I can compute d and e and plot those instead.
addinterval(BLUE, T, U+2, T, lambda t: c2xy(orbits_d(2*pi*t/10.0)))
addinterval(VIOLET, T, U+2, T, lambda t: c2xy(orbits_e(2*pi*t/10.0)))
T = U

# Bring in a sixth point, and go into a hexagonal version of one of
# the Square Dances.
def pmsqrt(x):
    if x < 0:
        return -sqrt(-x)
    else:
        return sqrt(x)
def wibble(x, n):
    return x - sin(x*n)/n
def gc_r(theta):
    return 1 + 0.3 * pmsqrt(sin(theta * 3))
def pointpos(t, offset):
    theta = wibble(t, 6)
    return polar(gc_r(theta), theta + offset)
chaptermark(T+2,"Grand Chain", 10)
U = T + 120
addinterval(ORANGE, T-3, T+2, T, lambda t: (0, -4.999999))
addinterval(RED, T-2, U+2, T, lambda t: pointpos(2*pi*t/10.0, 0))
addinterval(YELLOW, T-2, U+2, T, lambda t: pointpos(2*pi*t/10.0, 2*pi/3))
addinterval(BLUE, T-2, U+2, T, lambda t: pointpos(2*pi*t/10.0, 4*pi/3))
addinterval(ORANGE, T-2, U+2, T, lambda t: pointpos(-2*pi*t/10.0, pi/3))
addinterval(GREEN, T-2, U+2, T, lambda t: pointpos(-2*pi*t/10.0, 3*pi/3))
addinterval(VIOLET, T-2, U+2, T, lambda t: pointpos(-2*pi*t/10.0, 5*pi/3))
T = U

# Now here's a pattern I constructed by integration. I want to
# maintain two 4-meeting-points at -1 and +1 at all times, which
# means in a degree-7 polynomial that the entire derivative of the
# polynomial is nailed down completely. So I construct that
# derivative, integrate it, and then adjust its constant term (the
# only remaining degree of freedom) to arrange one root moving
# along a path of my choosing. Then I continuously identify the
# other roots, and I have my pattern.

twirlpos = {}
def doubletwirl(colour, t):
    global twirlpos
    if not twirlpos.has_key(t):
        # Compute the point positions for t, and store them in the
        # twirlpos cache.

        # First decide the location of the root we can choose.
        rootpos = 1.6 * sin(t) * cmath.exp(cos(t)*1j)

        # Then add a constant term to the polynomial to make it
        # zero at that position.
        poly = [0, -35, 0, 35, 0, -21, 0, 5]
        pval = 0
        for j in range(len(poly)-1, -1, -1):
            pval = rootpos * pval + poly[j]
        poly[0] = -pval

        # Now we know our polynomial, factorise it.
        roots = polyfac.factorise(poly)

        # Now identify the roots reliably by colour.
        posns = {}

        # First pick out the closest one to rootpos; that's our
        # moving root.
        moving = 0
        bestdist = abs(roots[0] - rootpos)
        for i in range(1,len(roots)):
            dist = abs(roots[i] - rootpos)
            if dist < bestdist:
                bestdist = dist
                moving = i
        posns[GREEN] = antiproject((roots[moving].real, roots[moving].imag))

        # Now our strategy for identifying the remaining roots is
        # to separately deal with the left and right half-planes.
        # In each half-plane, we compute the bearing of all the
        # points in that half-plane from the multiple point (+1 or
        # -1) relative to the bearing of the moving root, and then
        # assign colours round in order. This strategy is derived
        # from having actually watched the behaviour of the roots
        # in practice.
        dividingline = -0.4*roots[moving].real
        for sign, colours in [(-1, (RED, ORANGE, YELLOW)), (+1, (VIOLET, INDIGO, BLUE))]:
            subroots = []
            for i in range(len(roots)):
                sr = sign * roots[i]
                if i != moving and sr.real > sign*dividingline:
                    indicator = (sr - 1) / (sign*roots[moving] - 1)
                    iarg = atan2(indicator.imag, indicator.real)
                    while iarg < 0:
                        iarg = iarg + 2*pi
                    subroots.append((iarg, i, roots[i].real, roots[i].imag))
            assert len(subroots) == len(colours)
            subroots.sort()
            for i in range(len(subroots)):
                posns[colours[i]] = antiproject(subroots[i][2:4])

        # Done.
        twirlpos[t] = posns

    return twirlpos[t][colour]

addinterval(INDIGO, T-3, U+2, T, lambda t: polar(4.999999, 5*pi/3))
chaptermark(T+2,"Wheels", 20)
U = T + 120
addinterval(RED, T-2, U+2, T, lambda t: doubletwirl(RED, 2*pi*t/20.0))
addinterval(ORANGE, T-2, U+2, T, lambda t: doubletwirl(ORANGE, 2*pi*t/20.0))
addinterval(YELLOW, T-2, U+2, T, lambda t: doubletwirl(YELLOW, 2*pi*t/20.0))
addinterval(GREEN, T-2, U+2, T, lambda t: doubletwirl(GREEN, 2*pi*t/20.0))
addinterval(BLUE, T-2, U+2, T, lambda t: doubletwirl(BLUE, 2*pi*t/20.0))
addinterval(INDIGO, T-2, U+2, T, lambda t: doubletwirl(INDIGO, 2*pi*t/20.0))
addinterval(VIOLET, T-2, U+2, T, lambda t: doubletwirl(VIOLET, 2*pi*t/20.0))
T = U

# Now shift into a chorus-line arrangement: seven points strung out
# in a line, oscillating in a sine wave.

chaptermark(T+2,"Chorus Line", 10)
U = T + 120
addinterval(RED, T-2, U+2, T, lambda t: (-1.5, 1.5*sin(2*pi*t/10)))
addinterval(ORANGE, T-2, U+2, T, lambda t: (-1.0, 1.5*sin(2*pi*(t/10 - 1.0/7))))
addinterval(YELLOW, T-2, U+2, T, lambda t: (-0.5, 1.5*sin(2*pi*(t/10 - 2.0/7))))
addinterval(GREEN, T-2, U+2, T, lambda t: (0.0, 1.5*sin(2*pi*(t/10 - 3.0/7))))
addinterval(BLUE, T-2, U+2, T, lambda t: (0.5, 1.5*sin(2*pi*(t/10 - 4.0/7))))
addinterval(INDIGO, T-2, U+2, T, lambda t: (1.0, 1.5*sin(2*pi*(t/10 - 5.0/7))))
addinterval(VIOLET, T-2, U+2, T, lambda t: (1.5, 1.5*sin(2*pi*(t/10 - 6.0/7))))
T = U

# Now shift the y phases so the points are out of phase.

chaptermark(T+2,"Chorus Line II", 30)
U = T + 120
addinterval(RED, T-2, U+2, T, lambda t: (-1.5, sin(6*pi*t/30)))
addinterval(ORANGE, T-2, U, T, lambda t: (-1.0, sin(6*pi*(t/30 - 1.0/7))))
addinterval(YELLOW, T-2, U+2, T, lambda t: (-0.5, sin(6*pi*(t/30 - 2.0/7))))
addinterval(GREEN, T-2, U, T, lambda t: (0.0, sin(6*pi*(t/30 - 3.0/7))))
addinterval(BLUE, T-2, U+2, T, lambda t: (0.5, sin(6*pi*(t/30 - 4.0/7))))
addinterval(INDIGO, T-2, U+2, T, lambda t: (1.0, sin(6*pi*(t/30 - 5.0/7))))
addinterval(VIOLET, T-2, U+2, T, lambda t: (1.5, sin(6*pi*(t/30 - 6.0/7))))
T = U

# Then shift the x-coordinates into a 2x3 Lissajous curve, so the
# points begin to interchange.

chaptermark(T+2,"Finale", 30)
addinterval(RED, T-2, T+223, T, lambda t: (1.5*sin(4*pi*t/30), sin(6*pi*t/30)))
addinterval(ORANGE, T-4, T+144, T, lambda t: (1.5*sin(4*pi*(t/30 - 1.0/7)), sin(6*pi*(t/30 - 1.0/7))))
addinterval(YELLOW, T-2, T+122, T, lambda t: (1.5*sin(4*pi*(t/30 - 2.0/7)), sin(6*pi*(t/30 - 2.0/7))))
addinterval(GREEN, T-4, T+122, T, lambda t: (1.5*sin(4*pi*(t/30 - 3.0/7)), sin(6*pi*(t/30 - 3.0/7))))
addinterval(BLUE, T-2, T+204, T, lambda t: (1.5*sin(4*pi*(t/30 - 4.0/7)), sin(6*pi*(t/30 - 4.0/7))))
addinterval(INDIGO, T-2, T+164, T, lambda t: (1.5*sin(4*pi*(t/30 - 5.0/7)), sin(6*pi*(t/30 - 5.0/7))))
addinterval(VIOLET, T-2, T+184, T, lambda t: (1.5*sin(4*pi*(t/30 - 6.0/7)), sin(6*pi*(t/30 - 6.0/7))))

# [Commented out because the lighting went screwy]
## A little way into the finale, begin shifting the convergence
## rate, just to prove it can be done. Then shift it back again,
## because it gets pretty freaky out there.
#addinterval(POWER, T, T+60, T, lambda t: (1.0,))
#addinterval(POWER, T+40, T+105, T, lambda t: (0.55,))
#addinterval(POWER, T, T+85, T, lambda t: (1.0,))

chaptermark(T+122,"Outro", 30)
# Move the yellow and green points to their final positions.
addinterval(YELLOW, T+118, T+227, T, lambda t: (-0.75, 0))
addinterval(GREEN, T+118, T+227, T, lambda t: (0.75, 0))
# Remove the other points, one by one, in the appropriate directions.
addinterval(ORANGE, T+139, T+145, T, lambda t: polar(4.999999, 5*pi/3))
addinterval(INDIGO, T+158, T+165, T, lambda t: polar(4.999999, 5*pi/3))
addinterval(VIOLET, T+178, T+185, T, lambda t: polar(4.999999, pi/3))
addinterval(BLUE, T+198, T+205, T, lambda t: polar(4.999999, pi/3))
addinterval(RED, T+215, T+227, T, lambda t: (0, -4.999999))
T = T + 227

# And that's it. Fade in and out at the beginning and end.
Tend = T
addinterval(FADE, Tstart, Tstart+2, Tstart, lambda t:(0,))
addinterval(FADE, Tstart+1, Tend-1, 0, lambda t:(1,))
addinterval(FADE, Tend-2, Tend, 0, lambda t:(0,))

# Roll the credits.
p = os.popen("identify -format '%w\n%h' credits.png")
w = p.readline()
h = p.readline()
p.close()
cwidth = string.atoi(w)
cheight = string.atoi(h)
pheight = cwidth / aspect
crollheight = cheight - pheight
# We want the credits to roll at such a speed that any given item
# takes about eight seconds between scrolling on and scrolling off
# again, on a wide-screen display. However, it's also vital to have
# an integer number of pixels per frame to prevent jerky scrolling.
crollspeed = int(0.5 + (cwidth * 9.0/16.0) / (8*fps))
crolltime = int(crollheight / crollspeed) / fps
U = T + crolltime
chaptermark(T, "End Credits", U-T)
addinterval(CREDITS, T, U, T, lambda t:(int(0.5+t*fps) * crollspeed,))
T = U

totaltime = T

maxjobs = 2
maxbacklog = 16
totalframes = int(0.5 + totaltime * fps)
if maxframes == None:
    maxframes = totalframes
maxframes = min(maxframes, totalframes - frameoffset)
head = tail = 0
queue = {}
rqueue = {}
cache = []
cachesize = 500

shelllist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
shelllist = shelllist + "%+,-./:=[]^_@"
def checkstr(s, allowed_chars):
    # Return true iff the string s is composed entirely of
    # characters in the string `allowed_chars'.
    for c in s:
        if not (c in allowed_chars):
            return 0
    return 1
def shellquote(list):
    # Take a list of words, and produce a single string which
    # expands to that string of words under POSIX shell quoting
    # rules.
    ret = ""
    sep = ""
    for word in list:
        if len(word) == 0 or not checkstr(word, shelllist):
            word = "'" + string.replace(word, "'", "'\\''") + "'"
        ret = ret + sep + word
        sep = " "
    return ret

def cmdline(t):
    power, title, credits, points = findpoints(frameoffset + float(t) / fps)
    cmdline2 = cmdline3 = None
    if title != None:
        if preview:
            titlename = "ptitle.png"
        else:
            titlename = "title.png"
        border = int(max(round(width * (1.0 - (4.0/3.0) / aspect)) / 2.0, 0))
        if border > 0:
            # What we really want here is to say `--border 90x0',
            # but ImageMagick appears to go screwy if we do that
            # and just writes out a totally blank image. So we'll
            # use 90x1, and then crop off the top and bottom rows
            # of pixels. Bah.
            cmdline = ["convert", "-negate", "-modulate", "%f" % (100.0*title),
            "-scale", "%dx%d!" % (width - 2*border, height),
            "-bordercolor", "black", "-border", "%dx1" % border,
            titlename, "ppm:-"]
            cmdline2 = ["convert", "-crop", "%dx%d+0+1" % (width, height),
            "ppm:-", "ppm:-"]
        else:
            cmdline = ["convert", "-negate", "-modulate", "%f" % (100.0*title),
            "-scale", "%dx%d!" % (width, height), titlename, "ppm:-"]
        cmdline3 = ["./vblur.pl"]
    elif credits != None:
        cmdline = ["convert", "-negate", "-crop",
        "%dx%d+0+%d" % (cwidth, pheight, credits),
        "credits.png", "ppm:-"]
        cmdline2 = ["convert", "-scale", "%dx%d!" % (width, height),
        "ppm:-", "ppm:-"]
        cmdline3 = ["./vblur.pl"]
    else:
        cmdline = ["./newton"]
        if preview:
            cmdline.append("--preview")
        if power < 1.0:
            cmdline.append("-p")
            cmdline.append("%.3f" % power)
        # Total _area_ covered by screen is 20. Use this to
        # determine the x- and y-extents. The deal is that xext *
        # yext * 4 = area, and since xext = aspect * yext, this
        # gives us yext = sqrt(area / (aspect * 4)).
        yext = sqrt(20.0 / (aspect * 4))
        xext = aspect * yext
        cmdline.extend(["--ppm", "-o", "-", "-s", "%dx%d" % (width,height),
        "-C", "no", "-B", "yes", "-X", "0.00001", "-Y", "0.00001",
        "-y", str(yext), "-x", str(xext), "-f", "10", "-n", "1,1,1", "-c",
        ":".join(["%g,%g,%g" % (r,g,b) for ((r,g,b),x,y) in points]),
        "--"] + ["%+g%+gi" % (x,y) for ((r,g,b),x,y) in points])
    ret = shellquote(cmdline)
    if cmdline2 != None:
        ret = ret + " | " + shellquote(cmdline2)
    if cmdline3 != None:
        ret = ret + " | " + shellquote(cmdline3)
    return ret

def startjobs():
    global head, tail, queue

    # Start up as many jobs as we can. We limit ourselves to having
    # at most `maxjobs' jobs running at once, and having at most
    # jobs go out `maxbacklog' distance in front of the hindmost
    # running job.
    while tail < maxframes and len(queue) < maxjobs and tail - head < maxbacklog:
        # Start a new job.
        cmd = cmdline(tail)
        if dryrun:
            sys.stdout.write("%d / %.2f : %s\n" % (tail, frameoffset + float(tail)/fps, cmd))
        else:
            got = 0
            for cachecmd, cachedata in cache:
                if cachecmd == cmd:
                    if verbose:
                        sys.stderr.write("cached %d: %s\n" % (tail, cmd))
                    queue[tail] = [None, cachedata, cmd]
                    got = 1
                    break
            if not got:
                if verbose:
                    sys.stderr.write("starting %d: %s\n" % (tail,cmd))
                p = os.popen(cmd, "r")
                flags = fcntl.fcntl(p.fileno(), fcntl.F_GETFL)
                fcntl.fcntl(p.fileno(), fcntl.F_SETFL, flags | os.O_NONBLOCK)
                queue[tail] = [p, "", cmd]
                rqueue[p.fileno()] = tail
        tail = tail + 1

def checkjobs():
    global head, tail, queue, cache

    if dryrun:
        head = tail
        return

    if len(rqueue.keys()) > 0:
        r, w, x = select.select(rqueue.keys(), [], [])
        for fd in r:
            n = rqueue[fd]
            data = queue[n][0].read()
            queue[n][1] = queue[n][1] + data
            if data == "":
                # Job has finished.
                queue[n][0].close()
                queue[n][0] = None
                if verbose:
                    sys.stderr.write("reaping %d\n" % n)
                del rqueue[fd]

    # See how many jobs we can reap from the start of the queue.
    while head < tail and queue[head][0] == None:
        sys.stdout.write(queue[head][1])
        if verbose:
            sys.stderr.write("clearing %d\n" % head)
        cache = cache[-cachesize:] + [(queue[head][2], queue[head][1])]
        del queue[head]
        head = head + 1

if mode == "chapters":
    # Just write the chapter points to standard output.
    for time, name, period in chapters:
        print fmttime(time), name
    sys.exit(0)
if mode == "chaptermenu":
    # Write the chapter start and period, in frames, to standard output.
    for time, name, period in chapters:
        print int(round(time*fps)), int(round(period*fps))
    sys.exit(0)
elif mode == "total":
    # Write the total time to standard output.
    print totaltime
    sys.exit(0)
elif mode == "totalframes":
    # Write the total number of frames to standard output.
    print maxframes
    sys.exit(0)

while head < maxframes:
    startjobs()
    checkjobs()
