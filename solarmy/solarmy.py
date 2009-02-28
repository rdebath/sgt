#!/usr/bin/env python

import math
import os
import sys

limit = 12

grid = {}

ON, OFF = 1, 0

debug = 0

def f(x, limit=limit):
    return math.tanh(x/(limit*0.75))

def g(x):
    return 1 - 2.0**-(x*6.0/limit)

def identity(x):
    return x

# Make a single move at a specific time instant.
def bounce(x, y, dx, dy, time, centre=OFF):
    grid[(x,y)] = grid.get((x,y),[]) + [(time, ON)]
    grid[(x+dx,y+dy)] = grid.get((x+dx,y+dy),[]) + [(time, centre)]
    grid[(x+dx*2,y+dy*2)] = grid.get((x+dx*2,y+dy*2),[]) + [(time, OFF)]
    if debug:
        print "%.10f: on (%d,%d), off (%d,%d), off (%d,%d)" % \
        (time, x, y, x+dx,y+dy, x+dx*2,y+dy*2)

# Make a double infinite sequence of moves, from time func(0) to
# func(1) (exclusive).
def whoosh(x, y, dx, dy, func):
    if debug:
        print "%.10f - %.10f: whoosh (%d,%d) +(%d,%d):" % (func(0), func(1), x, y, dx, dy)
    # Forward infinite sequence compressed into [0,0.5].
    i = 0
    X, Y = x+dx, y+dy
    while X >= -limit and X <= limit and Y >= -limit:
        bounce(X, Y, dx, dy, func(g(i+0.5)/2))
        X, Y = X+dx*2, Y+dy*2
        i = i + 1
    # Backward infinite sequence compressed into [0.5,1].
    i = 0
    X, Y = x, y
    while X >= -limit and X <= limit and Y >= -limit:
        bounce(X, Y, dx, dy, func(1 - g(i+0.5)/2))
        X, Y = X+dx*2, Y+dy*2
        i = i + 1
    if debug:
        print "whoosh done"

# Perform the complex infinite backward sequence of whooshes which
# turns a quarter-plane (x*dx > 0, y <= 0) into a single peg (0,3).
#
# Takes as input the desired value of t _after_ the move, and
# returns the vaue before it.
def megawhoosh(dx, t, index=-1, snapshots=[]):
    # The first infinite loop, doing downward whooshes.
    tloop = 0
    ttotal = 5
    for i in range(limit+2):
        if i == 0: snapshots.append((t-ttotal*f(-tloop,limit/2), t-ttotal*f(-tloop,limit/2)))
        bounce((2*i)*dx, (3-2*i), dx, 0, t-ttotal*f(-tloop+0.125,limit/2)); tloop = tloop - 0.25
        if i == 0 or i == 1: snapshots.append((t-ttotal*f(-tloop,limit/2), t-ttotal*f(-tloop,limit/2)))
        whoosh((2*i+1)*dx, (3-2*i), 0, -1, lambda T: t-ttotal*f(-(tloop+0.25*(T-1)),limit/2)); tloop = tloop - 0.25
        whoosh((2*i+2)*dx, (3-2*i), 0, -1, lambda T: t-ttotal*f(-(tloop+0.25*(T-1)),limit/2)); tloop = tloop - 0.25
        if i == 0 or i == 1 or i == 2: snapshots.append((t-ttotal*f(-tloop,limit/2), t-ttotal*f(-tloop,limit/2)))
    t = t - ttotal
    snapshots.append((t,t))

    # The second infinite loop, doing outward whooshes.
    tloop = 0
    ttotal = 5
    for i in range(limit+1):
        whoosh((2*i+1)*dx, (1-2*i), dx, 0, lambda T: t-ttotal+ttotal*f(tloop+T)); tloop = tloop + 1
    t = t - ttotal
    snapshots.append((t,t))

    # Fix up the columns with some downward bounces.
    if debug:
        print "fixup:"
    tloop = 0
    if index == 8:
        # When we do this in reverse for the illustration, we
        # do it in the other order, i.e. still starting with
        # the innermost column.
        ttotal = 15
        for x in range(3, limit+1):
            bottom = 5 - (x+x%2)
            for y in range(bottom, 3, 2):
                bounce(x*dx, y, 0, -1, t-ttotal*f(-tloop+0.5,limit*2)); tloop = tloop - 1
    else:
        ttotal = 5
        for x in range(3, limit+1):
            bottom = 5 - (x+x%2)
            for y in range(1, bottom-2, -2):
                bounce(x*dx, y, 0, -1, t-ttotal+ttotal*f(tloop+0.5,limit*(limit+1)/4)); tloop = tloop + 1
    snapshots.append((t-ttotal,t))
    t = t - ttotal
    if debug:
        print "fixup done"
    snapshots.append((t,t))

    return t

startpos = lambda x,y: y <= 0
xmin = -limit
xmax = +limit
ymin = -limit
ymax = +5

t = 0
tmin = tmax = None
reverse = 0
yaxis = xaxis = 0

args = sys.argv[1:]

if args[0] == "-d":
    # debug: print lots of output, and leave auxiliary files
    debug = 1
    args = args[1:]

arg = args[0]

if arg == "solution":
    yaxis = xaxis = 1
    # We construct our sequence backwards, from a single piece at y=5
    # at time zero.

    # Move (0,5) down to (0,4) and (0,3).
    bounce(0,5,0,-1,t-0.25); t = t - 0.5

    # Megawhoosh (0,3) to the right half-plane.
    t = megawhoosh(+1, t)

    # Move (0,4) down into (0,3) and (0,2).
    bounce(0,4,0,-1,t-0.25); t = t - 0.5

    # Whoosh (0,2) down into the central column.
    whoosh(0,2,0,-1,lambda T: t-1+T); t = t - 1

    # And megawhoosh (0,3) to the left half-plane.
    t = megawhoosh(-1, t)

elif arg[:10] == "megawhoosh":

    yaxis = xaxis = 1
    index = int(arg[10:])
    snapshots = []
    startpos = lambda x,y: y <= 0 and x < 0
    t = megawhoosh(-1, t, index, snapshots)
    tmin, tmax = snapshots[index]
    if index == 8:
        reverse = 1

elif arg == "startpoint":
    yaxis = xaxis = 1
    pass

elif arg == "randommoves":
    yaxis = xaxis = 1
    bounce(0, 1, 0, -1, t + 0.25); t = t + 0.5
    bounce(0, -1, 0, -1, t + 0.25); t = t + 0.5
    bounce(0, -2, -1, 0, t + 0.25); t = t + 0.5
    bounce(-1, -2, 0, 1, t + 0.25); t = t + 0.5
    bounce(-2, -2, 0, 1, t + 0.25); t = t + 0.5
    bounce(0, 0, 1, 0, t + 0.25); t = t + 0.5
    bounce(0, 2, 0, -1, t + 0.25); t = t + 0.5
    xmin = -5
    xmax = +5
    ymin = -5
    ymax = +3

elif arg == "parallel":
    yaxis = xaxis = 1
    for x in range(-limit,limit+1):
        bounce(x, 1, 0, -1, t + 0.25)
    t = t + 0.5
    ymin = -5
    ymax = +2

elif arg == "parskew":
    yaxis = xaxis = 1
    tloop = 0
    for xa in range(0,limit+1):
        bounce(xa, 1, 0, -1, f(tloop + 0.5)); tloop = tloop + 1
        if xa > 0:
            bounce(-xa, 1, 0, -1, f(tloop + 0.5)); tloop = tloop + 1
    t = t + 1
    ymin = -5
    ymax = +2

elif arg == "halfwhoosh1":
    startpos = lambda x,y: y == -5 and x <= 0
    ymin = -6
    ymax = -4
    xmin = -20
    xmax = +3
    for x in range(1, xmin-1, -2):
        bounce(x, -5, -1, 0, t + 0.25); t = t + 0.5

elif arg == "halfwhoosh2r":
    startpos = lambda x,y: y == -5 and x == 2
    ymin = -6
    ymax = -4
    xmin = -20
    xmax = +3
    for x in range(2, xmin-1, -2):
        bounce(x-2, -5, +1, 0, t + 0.25, centre=ON); t = t + 0.5

elif arg == "halfwhoosh2":
    startpos = lambda x,y: y == -5 and x <= 1 and x%2 == 1
    ymin = -6
    ymax = -4
    xmin = -20
    xmax = +3
    for x in range(2, xmin-1, -2):
        bounce(x, -5, -1, 0, t - 0.25); t = t - 0.5

elif arg == "whoosh":
    startpos = lambda x,y: y == -5 and x <= 0
    ymin = -6
    ymax = -4
    xmin = -20
    xmax = +3
    whoosh(2, -5, -1, 0, lambda T: t+T); t = t + 1

elif arg == "badmega":
    startpos = lambda x,y: y <= 0 and x <= -2
    ttotal = 5
    tloop = 0
    for i in range(-2, -limit-2, -1):
        whoosh(i, 2, 0, -1, lambda T: t+ttotal*f(tloop+T)); tloop = tloop + 1
    t = t + ttotal
    whoosh(0, 2, -1, 0, lambda T: t+T); t = t + 1
    yaxis = xaxis = 1

elif arg == "fwargle":
    startpos = lambda x,y: 0
    def recurse(y,t0,t1):
        if y < -limit:
            return
        bounce(0,y,0,-1,t0+(t1-t0)*0.95)
        recurse(y-2, t0+(t1-t0)*0.45, t0+(t1-t0)*0.9)
        recurse(y-1, t0, t0+(t1-t0)*0.45)
    ymin = -limit
    ymax = +1
    xmin = -1
    xmax = +1
    recurse(0,0,10)
    t = 10

# Now check that we have a consistent story for each individual peg
# position within the region we'll end up displaying. In
# chronological order, we expect to alternate ONs and OFFs; we
# expect to start with an OFF in any position which has a peg to
# begin with, and an ON otherwise.
#
# Also in this loop, build up a list of the times at which visible
# moves take place.
times = {}
if tmin == None:
    tmin = min(t, 0)
if tmax == None:
    tmax = max(t, 0)
for x in range(-limit,limit+1):
    for y in range(-limit,5):
        list = grid.get((x,y), [])
        if list == []: # completely unused space
            continue
        list.sort()
        if debug:
            print x, y, list
        if startpos(x, y):
            assert list[0][1] == OFF
        else:
            assert list[0][1] == ON
        for i in range(len(list)-1):
            assert list[i][0] < list[i+1][0] # _strictly_ monotonic in time
            assert list[i][1] != list[i+1][1] # alternate ON with OFF
        for time, switch in list:
            if reverse:
                transtime = int((tmax-time)*100)
            else:
                transtime = int((time-tmin)*100)
            times[transtime] = time

if reverse:
    times[(tmax-tmin)*100] = tmin
else:
    times[0] = tmin
timelist = filter(lambda tround: times[tround] >= tmin and \
times[tround] <= tmax, times.keys())
timelist.sort()

images = []
tilesize = 12
centre = (tilesize-1)/2
rangex = (xmax+1-xmin) * tilesize - 1
rangey = (ymax+1-ymin) * tilesize - 1
for tround in timelist:
    time = times[tround]
    filename = arg + "%06d.png" % tround
    images.append((tround, filename))
    p = os.popen("convert -depth 8 -size %dx%d rgb:- %s" % (rangex, rangey, filename), "w")
    for yp in xrange(rangey):
        y = ymax - (yp / tilesize)
        yc = yp % tilesize
        for xp in xrange(rangex):
            x = xmin + (xp / tilesize)
            xc = xp % tilesize
            # Determine whether there's a peg at x,y.
            list = grid.get((x,y), [])
            if list == []: # completely unused space
                peg = startpos(x,y)
            elif list[0][0] > time:
                peg = not list[0][1]
            else:
                i = 0
                while i+1 < len(list) and list[i+1][0] <= time:
                    i = i + 1
                peg = list[i][1]
            # Draw the pixel.
            d2 = (yc-centre)**2+(xc-centre)**2
            bg = "\xC0\xC0\xC0"
            if (yaxis and x==0 and xc==centre) or (xaxis and y==0 and yc==centre):
                bg = "\xA0\xA0\xA0"
            if peg:
                if d2 < (centre**2/2):
                    p.write("\x00\x00\xFF")
                else:
                    p.write(bg)
            else:
                if d2 < (centre**2/5):
                    p.write("\x80\x80\x80")
                else:
                    p.write(bg)
    p.close()
    if debug:
        print "wrote", filename, "(%d of %d)" % (len(images), len(timelist))

cmdline = "convert"
for i in range(len(images)):
    if i == 0 or i == len(images)-1:
        delay = 200
    else:
        if reverse:
            delay = images[i][0] - images[i-1][0]
        else:
            delay = images[i+1][0] - images[i][0]
    if len(images) > 1:
        cmdline = cmdline + " -delay %d" % delay
    cmdline = cmdline + " %s" % images[i][1]
cmdline = cmdline + " gif:- > %s.gif" % arg
if debug:
    print cmdline
os.system(cmdline)

if not debug:
    for time, filename in images:
        os.remove(filename)
