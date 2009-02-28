#!/usr/bin/env python

# Given a list of integers with an integer average, find a
# permutation of them which is a valid siteswap.
#
# Algorithm taken directly from a proof of the theorem that it can
# always be done. The proof depends on a central lemma which says
# that given one list of integers with an integer average which
# _can_ be arranged into a siteswap, if you replace two of those
# integers with two arbitrary other ones (without destroying the
# integer-average condition) then the new list can also be arranged
# into a siteswap. This lemma is fully constructive, so it provides
# an algorithm for finding the new permutation given the old one;
# thus we can apply it successively to transform a string of zeroes
# into the desired list of integers, and at the end of the process
# we have the permutation we are after.
#
# The proof is available (as of 2006-11-08) at
#   http://tartarus.org/gareth/maths/juggling_theorem.ps
#
# Note that the algorithm it describes takes O(n^2) time. This is a
# huge improvement on the naive approach of trying all possible
# permutations, which would take O(n!) time. So this is a practical
# algorithm as well as being an adjunct to the proof.

import string
import sys

# Compensate for older versions of Python.
try:
    s = sum([1,2])
except NameError:
    def sum(list):
        return reduce(lambda x,y:x+y, list)

debugging = 0
randomise = 0

def debug(*a):
    if debugging:
        print string.join(" ", map(str, a))

def siteswaptest(list):
    # Return true iff list is a valid siteswap.
    n = len(list)
    landing = [(i + list[i]) % n for i in range(n)]
    landing.sort()
    return landing == range(n)

def lemma(list, perm, p1, n1, p2, n2):
    # Given the list of integers `list', and the permutation `perm'
    # which arranges them into a valid siteswap (i.e.
    # [list[perm[i]] for i in range(len(list))] is a valid
    # siteswap), this function returns a new value of `perm' which
    # works for the altered `list' in which list[p1]=n1 and
    # list[p2]=n2.

    n = len(list)
    assert n == len(perm)
    assert 0 <= p1 and p1 < n
    assert 0 <= p2 and p2 < n
    assert siteswaptest([list[perm[i]] for i in range(n)])

    tmplist = list[:]
    tmplist[p1] = n1
    tmplist[p2] = n2

    invperm = [None] * n
    for i in perm: invperm[perm[i]] = i
    pp1 = invperm[p1]
    pp2 = invperm[p2]

    # OK. Now tmplist is our list of integers; perm is our current
    # permutation of them; and pp1 and pp2 are the indices in the
    # _permuted_ list of the two new integers n1 and n2. Work out
    # the landing places of the two throws we've replaced.
    ol1 = (pp1 + list[perm[pp1]]) % n
    ol2 = (pp2 + list[perm[pp2]]) % n

    # Now begin the main rearrangement loop.
    debug(">", list, perm, p1, n1, p2, n2)
    while 1:
        debug("  >", tmplist, perm, [tmplist[perm[i]] for i in range(n)], pp1, pp2, ol1, ol2)
        # Work out the landing places of those throws, and also the
        # landing places they used to have.
        nl1 = (pp1 + tmplist[perm[pp1]]) % n
        nl2 = (pp2 + tmplist[perm[pp2]]) % n
        debug("  ! nl1 =", nl1, "nl2 =", nl2)

        # If the two new landing places are the same as the old ones,
        # in either order, we're done.
        if (ol1==nl1 and ol2==nl2) or (ol2==nl1 and ol1==nl2):
            debug("  * done")
            break

        # If we can swap the two throws to get two landing places
        # the same as the old ones, in either order, we do so and
        # then we're also done.
        sl1 = (pp2 + tmplist[perm[pp1]]) % n
        sl2 = (pp1 + tmplist[perm[pp2]]) % n
        debug("  ! sl1 =", sl1, "sl2 =", sl2)
        if (ol1==sl1 and ol2==sl2) or (ol2==sl1 and ol1==sl2):
            perm[pp1], perm[pp2] = perm[pp2], perm[pp1]
            debug("  * swap")
            break

        # Otherwise, we're going to move throw 1 to where it will
        # need to be in order to land in place ol1. Find out where
        # that is.
        npp1 = (ol1 + n - tmplist[perm[pp1]]) % n
        debug("  * iterate", npp1)
        assert npp1 != pp1 and npp1 != pp2

        # Find the landing place of the throw currently in that
        # position.
        nl3 = (npp1 + tmplist[perm[npp1]]) % n
        debug("  ! new landing place", nl3)

        # Swap throw 1 into place.
        perm[pp1], perm[npp1] = perm[npp1], perm[pp1]

        # And change our target landing places.
        ol1, ol2 = ol2, nl3

        # That's it; now loop round and try again. This iteration
        # can never repeat a value of npp1 and must eventually
        # terminate in one of the easy cases: a proof of that is
        # part of the proof of the theorem (see URL at top of
        # file).

    assert siteswaptest([tmplist[perm[i]] for i in range(n)])
    return perm

def makeperm(list):
    n = len(list)
    tmplist = [0] * n
    perm = range(n)
    for i in range(n-1):
        p1 = i
        n1 = list[i]
        p2 = i+1
        if i == n-2:
            n2 = list[i+1]
        else:
            n2 = n - (sum(tmplist) + list[i] - tmplist[i] - tmplist[i+1]) % n
        perm = lemma(tmplist, perm, p1, n1, p2, n2)
        tmplist[p1] = n1
        tmplist[p2] = n2
    assert tmplist == list
    assert siteswaptest([list[perm[i]] for i in range(n)])
    return perm

args = sys.argv[1:]

while len(args) > 0 and args[0][0] == "-":
    if args[0] == "-d":
        debugging = 1
    elif args[0] == "-r":
        randomise = 1
    else:
        sys.stderr.write("unrecognised option '%s'\n" % args[0])
        sys.exit(1)
    args = args[1:]

if len(args) < 1:
    sys.stderr.write("usage: juggleperm.py [-d] [-r] <numbers>\n")
    sys.exit(1)

if len(args) == 1:
    # Split a single argument into digits.
    def digitval(c):
        if c >= 'a' and c <= 'z': return ord(c) - ord('a') + 10
        if c >= 'A' and c <= 'Z': return ord(c) - ord('A') + 10
        if c >= '0' and c <= '9': return ord(c) - ord('0')
        sys.stderr.write("unable to determine numeric value of '%c'\n" % c)
        sys.exit(1)
    list = [digitval(c) for c in args[0]]
    split = 1
else:
    list = map(string.atoi, args)
    split = 0

if randomise:
    import random
    random.shuffle(list)

if sum(list) % len(list) != 0:
    sys.stderr.write(repr(list) + " averages to the non-integer %f\n" % \
    (float(sum(list))/len(list)))
    sys.exit(1)
else:
    perm = makeperm(list)
    outlist = [list[perm[i]] for i in range(len(list))]
    if split:
        def mkdigit(v):
            assert v < 36
            if v >= 10: return chr(ord('a') + v - 10)
            return chr(ord('0') + v)
        print "".join(map(mkdigit, outlist))
    else:
        print " ".join(map(str, outlist))
