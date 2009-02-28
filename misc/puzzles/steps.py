#!/usr/bin/env python

# Solve by bfs a puzzle given on rec.puzzles:
#
# Determine the minimum number of moves needed to cross N steps.
#
# The first move starts with one step and every subsequent step can be one
# more or less than its previous move. You have to land on the final step
# exactly.
#
# e.g. 3 = 1 + 2 (3 moves)
# e.g. 7 = 1 + 2 + 1 + 2 + 1 (5 moves)
#
# (Note that each move _must_ be one more or less than the last.)
#
# The question doesn't make it clear whether zero is a permitted
# move, so we cover both cases below.

# We bfs over (position,lastmove) pairs.
moves = {}
moves[(1,1)] = (1,)
list = [(1,1)]
pos = 0

numbers = {}
o = 1

while pos < len(list):
    n, last = list[pos]
    cmoves = moves[(n, last)]
    if n > 0 and not numbers.has_key(n):
        numbers[n] = cmoves

    # Output. We output the results for numbers in increasing
    # order. Anything with an entry in numbers[] has its shortest
    # path known, so as long as numbers[o] exists we can keep
    # incrementing o. But some numbers may end up unreachable, so
    # we have to have a way of knowing when we're never going to
    # get somewhere.
    #
    # With zero-moves allowed, any number n can be reached in at
    # most 2n-1 steps by alternating 1s and 0s. Without zero moves,
    # no number n can possibly be reached in more than n steps
    # since each step is at least one. So we need only inspect the
    # length of `cmoves': if it exceeds 2o-1 and numbers[o] doesn't
    # exist, then it's never going to exist.
    while numbers.has_key(o) or len(cmoves) > 2*o-1:
        if numbers.has_key(o):
            movelist = numbers[o]
            s = "%d: %d [" % (o, len(movelist))
            sp = ""
            for m in movelist:
                s = s + sp + "%d" % m
                sp = " "
            s = s + "]"
        else:
            s = "%d: unreachable" % o
        print s
        o = o+1

    pos = pos + 1
    for move in (last+1, last-1):
        if move < 0:
            continue
        dest = (n+move, move)
        if not moves.has_key(dest):
            moves[dest] = cmoves + (move,)
            list.append(dest)
