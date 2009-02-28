#!/usr/bin/env python

# http://www.otuzoyun.com/pqrst/puzzle1408.html poses the question
# of how many distinct single closed loops you can draw on an array
# of 5x4 dots.

# Each loop defines a distinct subset of the 4x3 array of
# _squares_, but not all such subsets define a loop. Thus, the
# simplest thing is to iterate through each of the 2^12 subsets and
# see whether each works.

def sq(x, y, i):
    if x < 0 or x >= 4 or y < 0 or y >= 3:
        return 0
    if i & (1 << (y*4+x)):
        return 1
    return 0
def edge(x1, y1, x2, y2, i):
    if x1 == x2:
        if y2 == y1+1:
            return sq(x1-1,y1,i) != sq(x1,y1,i)
        elif y1 == y2+1:
            return sq(x1-1,y2,i) != sq(x1,y2,i)
    elif y1 == y2:
        if x2 == x1+1:
            return sq(x1,y1-1,i) != sq(x1,y1,i)
        elif x1 == x2+1:
            return sq(x2,y1-1,i) != sq(x2,y1,i)
    raise "arrgh, bad edge"

nsolns = 0
for i in range(4096):

    # Count the edges, and also search for a starting edge.
    sx = sy = None
    edges = 0
    for y in range(4):
        for x in range(5):
            if edge(x,y,x+1,y,i):
                edges = edges+1
                if sx == None:
                    sx = x
                    sy = y
            if edge(x,y,x,y+1,i):
                edges = edges+1
    if sx == None:
        print "no starting edge:", i
        continue # invalid

    # (sx,sy)-(sx+1,sy) is an edge. Trace round the loop from here.
    ox = sx
    oy = sy
    nx = sx+1
    ny = sy
    looplen = 1
    while (nx, ny) != (sx, sy):
        fx = fy = None
        number = 0
        #print "iterating at %d,%d" % (nx,ny)
        for tx,ty in ((nx+1,ny),(nx-1,ny),(nx,ny+1),(nx,ny-1)):
            #print "trying to go to %d,%d" % (tx,ty)
            if (tx,ty) == (ox,oy):
                continue # can't double back
            if edge(nx,ny,tx,ty,i):
                #print "found edge at %d,%d" % (tx,ty)
                fx = tx
                fy = ty
                number = number + 1
        if number != 1:
            print "vertex at %d,%d with degree %d:" % (nx,ny,number+1), i
            break # invalid
        #print "going to %d,%d" % (fx,fy)
        ox, oy = nx, ny
        nx, ny = fx, fy
        looplen = looplen + 1
    if (nx, ny) != (sx, sy):
        continue # interrupted
    if looplen != edges:
        print "multiple disjoint loops (%d/%d):" % (looplen, edges), i
        continue # multiple loops
    nsolns = nsolns+1
    print "solution:", i

print "solutions:", nsolns
