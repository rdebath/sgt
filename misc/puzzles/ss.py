#!/usr/bin/env python

# Experimental solver for the `Skyscrapers' puzzle. This is a
# Latin-square puzzle not totally dissimilar to Sudoku. The rules
# are:
#
#  - The solution to the puzzle is a plain Latin square (without
#    Sudoku's additional block constraint).
#
#  - Clues are written around the edge of the grid. Each clue gives
#    the length of the longest _increasing sequence_ in the
#    indicated row or column from that direction. The intuitive
#    rationale for this clue form is that the numbers in the Latin
#    square indicate skyscrapers of different heights, and each
#    clue tells you how many distinct skyscrapers you can _see_
#    looking into the block from a particular direction. Thus, a
#    clue of 1 immediately tells you the highest possible number is
#    right next to it, whereas a clue of N nails down the whole row
#    or column as a simple increasing sequence.
#
#  - Some clues may be missing.
#
#  - (I've also seen puzzles of this type which give additional
#    clues by filling in a few grid elements as well as providing
#    the clues round the edge; this solver doesn't currently handle
#    that, but easily could.)

# Test puzzle. Each 2-tuple gives the two clues for a given row or
# column in opposite directions. A clue value of zero means the
# clue is actually missing.
n = 6
colclues = [(3,0), (2,3), (0,3), (0,3), (2,0), (4,0)]
rowclues = [(2,0), (0,3), (0,0), (4,0), (4,0), (0,1)]

def perms(n):
    def rperms(prefix, a):
        if len(a) <= 1:
            return [prefix + a]
        ret = []
        for i in xrange(len(a)):
            ret = ret + rperms(prefix + a[i:i+1], a[:i] + a[i+1:])
        return ret
    return rperms((), tuple(range(1,n+1)))

def end(perm):
    left = 0
    highest = 0
    for i in perm:
        if i > highest:
            highest = i
            left = left + 1
    return left

def ends(perm):
    p = list(perm)
    p.reverse()
    return (end(perm), end(p))

def check(perm, clue):
    e = ends(perm)
    if clue[0] != 0 and clue[0] != e[0]:
        return 0
    if clue[1] != 0 and clue[1] != e[1]:
        return 0
    return 1

# Set up perm lists for each row and column.
rows = []
for i in xrange(n):
    rows = rows + [filter(lambda x: check(x, rowclues[i]), perms(n))]
cols = []
for i in xrange(n):
    cols = cols + [filter(lambda x: check(x, colclues[i]), perms(n))]

# Set up a list of possibilities for each square.
squares = [range(1,n+1) for x in xrange(n*n)]

def check_row(row, squares, start, step, n=n):
    done_something = 0
    newrow = []
    for perm in row:
        # Rule out any permutation which is inconsistent with the
        # possibles sets.
        ok = 1
        for i in xrange(n):
            if not (perm[i] in squares[start+i*step]):
                ok = 0
        if ok:
            newrow.append(perm)
    if len(newrow) < len(row):
        done_something = 1
        row[:] = newrow # modify in place

    # Now go the other way: winnow the possibles sets depending on
    # the possible permutations.
    newposs = [[] for i in xrange(n)]
    for perm in row:
        for i in xrange(n):
            if not (perm[i] in newposs[i]):
                newposs[i] = newposs[i] + [perm[i]]
    for i in xrange(n):
        if len(newposs[i]) < len(squares[start+i*step]):
            squares[start+i*step][:] = newposs[i]
            done_something = 1
    return done_something

# Now loop.
while 1:
    done_something = 0

    for i in xrange(n):
        done_something = done_something + check_row(rows[i], squares, i*n, 1)
        done_something = done_something + check_row(cols[i], squares, i, n)

    if done_something:
        continue

    # Now try set analysis.
    for i in xrange(1<<n):
        # Find the set bits; need >1 and <n-1.
        bits = []
        for j in xrange(n):
            if i & (1 << j):
                bits.append(j)
        if len(bits) <= 1 or len(bits) >= n-1:
            continue
        # Take the column-wise union of the possibles sets in each
        # of these rows.
        union = []
        for x in xrange(n):
            set = []
            for y in bits:
                for j in squares[y*n+x]:
                    if not (j in set):
                        set.append(j)
            union.append(set)
        # Now go through and count occurrences of each number. See
        # if any number occurs only as many times as we've used
        # rows.
        for j in xrange(1,n+1):
            gotcols = []
            for x in xrange(n):
                if j in union[x]:
                    gotcols.append(x)
            if len(gotcols) == len(bits):
                # We've got a set. Now see if we can rule anything
                # out by means of removing all references to n in
                # any column in gotcols and any row _not_ in bits.
                for y in xrange(n):
                    if y in bits:
                        continue
                    for x in gotcols:
                        if j in squares[y*n+x]:
                            squares[y*n+x].remove(j)
                            done_something = 1

    if done_something:
        continue

    break

for y in xrange(n):
    for x in xrange(n):
        k = reduce(lambda x,y:x+y, map(lambda x:chr(48+x), squares[y*n+x]))
        k = (k + " "*n)[:n]
        print k,
    print

