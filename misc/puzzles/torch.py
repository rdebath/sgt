#!/usr/bin/env python

# Solve the generalisation of the quickest-crossing puzzle.
#
# The setup: you have a finite number of people a,b,c,... who all
# wish to cross a bridge at night. The bridge will only take at
# most two of them; they need a torch to cross at all and they only
# have one; they all walk at potentially different speeds a <= b <=
# c <= ... and any two people together walk at the speed of the
# slower. What is the optimal strategy?
#
# In general, the optimal strategy depends on the relative speeds.
# For example, with four people, if we have (a,b,c,d)=(1,3,4,10)
# then the optimal strategy is the obvious one where a escorts each
# other person over the bridge in turn and brings the torch back.
# However, if (a,b,c,d)=(1,2,5,10) then you can do better by having
# c and d cross together so that they take up 10 minutes between
# them rather than 5, and having a and b dash backwards and
# forwards before and afterwards so that neither c nor d has to
# make a second trip.
#
# It would be relatively easy, given the values of a,b,c,..., to
# write a program to BFS through the possible solution space and
# determine the fastest strategy. However, this program is more
# ambitious: it leaves a,b,c,... _unspecified_, and prints a list
# of all the _potentially_ optimal strategies together with their
# total duration as a function of the individual crossing times.
# For example, run for four people it will print
#
# 2a+b+c+d: ab> <a ac> <a ad>
# a+3b+d: ab> <a cd> <b ab>
#
# which shows the two strategies listed above, and their total time
# cost.

import heapq
import sys

args = sys.argv[1:]
if len(args) > 0:
    n = int(args[0])
else:
    n = 4

# Implementation notes:
#
# We need to define a partial order on algebraic time expressions
# of the form Aa + Bb + Cc + ... (where a,b,c are the unknown
# crossing times of the individual people and A,B,C,... are
# integers) such that we consider one expression less than another
# iff it must necessarily be smaller for all values of a,b,c,... .
#
# So the order must at least include the product order (Aa+Bb+Cc <=
# A'a+B'b+C'c if A<A',B<B',C<C'). However, we also know that
# a<b<c<... and so there are other things we can know for sure.
#
# The solution is to take your vector (A,B,C,D) and transform it
# into (A+B+C+D,B+C+D,C+D,D), so that instead of v[i] indicating
# the number of crossings of exactly time i, v[i] indicates the
# number of crossings of time i _or longer_. Then you can apply the
# product order to that.
#
# So we store our distances internally as vectors in the latter
# form, which means the greater-or-equal function is a trivial
# product order comparison...

def ge(t0, t1):
    n = len(t0)
    assert n == len(t1)
    for i in range(n):
        if t0[i] < t1[i]:
            return 0
    return 1

# ... but the function which converts a vector into a human-
# readable algebraic string expression needs to transform the
# vector into the other form first.

def stringtime(dist):
    s = ""
    # Untransform.
    ldist = list(dist)
    for k in range(1,len(dist)):
        ldist[k-1] -= ldist[k]
    # Format as a string.
    sep = ""
    for k in range(len(dist)):
        if ldist[k] > 0:
            s = s + sep
            sep = "+"
            if ldist[k] > 1:
                s = s + "%d" % ldist[k]
            s = s + chr(97+k)
    if s == "":
        s = "0" # trivial case
    return s

# Doing a breadth-first search over a partial order is quite fun.
#
# To begin with, our queue of positions still to be processed must
# be a priority queue with respect to the partial order: we must be
# able to throw random things into the queue and always pull out a
# minimal element of the available ones. That sounds hairy - sit
# down and try to work out an equivalent of a binary heap that
# works on partial orders and your head will probably tie itself in
# a knot - but in fact it's simple with the right insight. Just
# find a topological sort of the partial order (i.e. a total order
# which agrees with it wherever both are defined), and use an
# ordinary binary heap over that total order. In this case, our
# partial order is a product order, and the obvious way to extend
# that into a total order is to use the corresponding lexicographic
# order.
#
# The other wrinkle is that you can't just process every position
# once. If you can reach the same position in two ways and the
# resulting distances are incomparable, you have to process that
# position twice so you can preserve that choice of incomparable
# distances in all positions you reach from there.
#
# So when you've made a trial move and _reached_ a position, and
# want to know whether you've got something worth storing in the
# best-distances list, you must check whether your new distance is
# greater than any of the existing distances for that position. If
# so, throw it out. But if not, you must also check whether your
# new distance is less than any of the existing distances, and if
# so throw _them_ out. Thus you maintain the invariant that all
# possible distances for a position are mutually incomparable.

# Map from a position to its list of minimal distances. All
# elements of adist[p] must be mutually incomparable at all times.
adist = {}
# Map from a (dist, position) pair to the (dist, position) pair we
# reached it from, plus details of the move we made between the
# two. We could in principle generate our output without this, but
# it's much more long-winded to do so.
aprev = {} # (dist, position) |-> (dist, position, move)
# Our to-do list: a binary heap of (dist, position) pairs sorted by
# the _total_ (lexicographic) order.
aqueue = []

def add(pos, dist, prev):
    dists = adist.get(pos, [])
    newdists = []
    # Go through the existing list of distances.
    for d2 in dists:
        # If an existing distance is at least as good as the new
        # one, the new one is no use.
        if ge(dist, d2):
            return
        # But any distance which the new one is at least as good as
        # is no use either.
        if not ge(d2, dist):
            newdists.append(d2)
    newdists.append(dist)
    adist[pos] = newdists
    aprev[(dist, pos)] = prev
    heapq.heappush(aqueue, (dist, pos))

# We represent a position as a tuple of three elements. The first
# element is 0 if the torch is on the left (starting) side, or 1 if
# it's on the right (finishing) side. The second element is a
# sorted list of the people on the left side; the third is a sorted
# list of the people on the right. People are represented as
# integers with 0=a,1=b,2=c,... .

# Put the starting position into the structure, with distance zero
# and no previous move.
add((0, tuple(range(n)), ()), (0,)*n, (None, None, None))

while len(aqueue) > 0:
    dist, pos = heapq.heappop(aqueue)
    torch, left, right = pos
    lleft = list(left)
    lright = list(right)
    # Normalise the left and right banks of the river into source
    # and destination banks, to simplify the subsequent processing.
    if torch:
        lfrom, lto = lright, lleft
    else:
        lfrom, lto = lleft, lright
    flen = len(lfrom)
    # For full generality, we always iterate over all possible one-
    # and two-person crossings irrespective of direction. Of course
    # it will turn out that it's always most efficient for every
    # forward crossing to involve two people and every backward
    # crossing to involve one, but it's more elegant not to have to
    # assume that. So i ranges over all the people on the source
    # bank, and j ranges over all the people after i plus the
    # special null second person indicating a one-person crossing.
    for i in range(flen):
        lleft[:] = list(left)
        lright[:] = list(right)
        for j in range(i+1, flen+1):
            lleft[:] = list(left)
            lright[:] = list(right)
            time = lfrom[i]
            move = (lfrom[i],)
            # Since j > i, we can remove lfrom[j] without affecting
            # lfrom[i], but not vice versa.
            if j < len(lfrom):
                time = lfrom[j]
                move = move + (lfrom[j],)
                lto.append(lfrom[j])
                lfrom.remove(lfrom[j])
            lto.append(lfrom[i])
            lfrom.remove(lfrom[i])
            lfrom.sort()
            lto.sort()
            newpos = (torch ^ 1, tuple(lleft), tuple(lright))
            newdist = list(dist)
            # Update the distance to reflect a crossing at the
            # speed of person `time'. Since the distance vector is
            # in its transformed form, this involves adding 1 to
            # every element up to `time'.
            for t in range(0,time+1):
                newdist[t] += 1
            newdist = tuple(newdist)
            add(newpos, newdist, (dist, pos, (torch, move)))

# Done. Find all the possible distances to the end position, and
# output them along with the move sequences that got there.
pos = (1,(),tuple(range(n)))
for dist in adist[pos]:
    sys.stdout.write(stringtime(dist) + ":")
    path = []
    here = pos
    while 1:
        dist, here, move = aprev[(dist, here)]
        if here == None:
            break
        path.insert(0, move)
    for torch, people in path:
        # I tried outputting the direction of the move using > and
        # <, but that turned out to be less readable.
        sys.stdout.write(" ")
        for p in people:
            sys.stdout.write("%c" % (97+p))
    sys.stdout.write("\n")
