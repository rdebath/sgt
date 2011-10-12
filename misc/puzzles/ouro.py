#!/usr/bin/env python

# Generate what Ian Stewart in one of his pop-maths books called an
# 'ouroborean ring', and mathematicians call a de Bruijn sequence. For
# positive integers n,k, this is a sequence of n^k elements of
# {0,...,n-1} with the property that every one of the n^k distinct
# k-tuples shows up exactly once as a contiguous subsequence (if the
# sequence is treated as cyclic).
#
# Takes two arguments: n (number of symbols in the alphabet), and k
# (length of string to make unique).

import sys, os, random

args = sys.argv[1:]
origarglen = 0
prog = os.path.basename(sys.argv[0])
short_output = 0
random_mode = 0

while len(args) > 0 and args[0][:1] == "-":
    if args[0] == "-s":
        short_output = 1
    elif args[0] == "-r":
        random_mode = 1
    else:
        sys.stderr.write("%s: unrecognised option '%s'\n" % (prog, args[0]))
        sys.exit(1)
    args = args[1:]

if len(args) != 2:
    sys.stderr.write("usage: %s [-s] [-r] <num-symbols> <substr-length>\n" \
                         % prog)
    sys.exit(origarglen > 0) # return 0 if called with no args for usage msg

n = int(args[0])
k = int(args[1])

# Finding a de Bruijn sequence can be seen as finding a Hamilton cycle
# on the directed graph whose vertices are k-tuples and which has an
# edge from one tuple A to another B if all but the first element of A
# match all but the last element of B.
#
# However, Hamilton cycles are hard, so a neater approach is to
# construct the same directed graph on (k-1)-tuples, in which case
# each _edge_ of the graph corresponds uniquely to a k-tuple (since it
# uniquely identifies two (k-1)-tuples which overlap to produce a
# k-tuple), and hence now we're looking for an Euler circuit which is
# much easier.
#
# It will also be convenient to label our edges with the last element
# of the tuple at the destination vertex, i.e. the element you might
# write down next in the sequence if the last k-1 elements matched the
# tuple at the source vertex.
#
# Start by explicitly constructing that graph.

def listtuples(n, k, prefix=(), ret=[]):
    if len(prefix) == k:
        ret.append(prefix)
    else:
        for i in range(n):
            listtuples(n, k, prefix+(i,), ret)
    return ret
vertices = listtuples(n, k-1)
vdict = {} # lists outgoing edges from each vertex, and their destinations
for v in vertices:
    vdict[v] = dict([(i, v[1:] + (i,)) for i in range(n)])

outlen = n**k # length of circuit we expect to end up with

# We support two algorithms for finding a de Bruijn sequence. One is
# the random approach of just finding any old Euler circuit of the
# above graph; the other is a deterministic algorithm which I first
# encountered in Ian Stewart's book but which can be traced back to
# the 1930s, which guarantees to find an Euler circuit first time.

if random_mode:
    # Randomly find an Euler circuit. Our approach is to start at a
    # random vertex and follow any old edge we feel like; since the
    # graph satisfies the basic requirements for an Euler circuit
    # (strongly connected and every vertex has the same in- and
    # out-degree), this can only terminate with us returning to the
    # starting vertex (we can't get stuck anywhere else). However, we
    # might not have used up all the edges in the process; if not, we
    # pick a vertex that we have visited but not used all the edges
    # of, start the same process there, and splice together the two
    # circuits obtained.
    #
    # We delete outgoing edges from vdict as we use them up, to ensure
    # we don't use them twice.
    def find_loop(v):
        vfirst = v
        ret = []
        while len(vdict[v]) > 0:
            key, vnew = random.choice(vdict[v].items())
            del vdict[v][key]
            v = vnew
            ret.append(v)
        assert vfirst == v
        return ret

    v = random.choice(vertices)
    loop = find_loop(v)

    while len(loop) < outlen:
        # Pick a position in the current loop to start a new one from.
        # We pick at random, subject to the constraint that the vertex
        # pointed at must be one with an outgoing edge remaining.
        copyloop = range(len(loop))
        random.shuffle(copyloop)
        ok = 0
        for i in copyloop:
            if len(vdict[loop[i]]) > 0:
                ok = 1
                break
        assert ok

        # Find a new loop starting there, and splice it in.
        loop2 = find_loop(loop[i])
        loop = loop[:i+1] + loop2 + loop[i+1:]

    assert len(loop) == outlen

    # The output sequence consists of the first element of each vertex
    # tuple.
    seq = [x[0] for x in loop]

else:
    # Deterministic algorithm. We start at the all-zeroes vertex, and
    # always follow the highest-numbered outgoing edge that isn't
    # already used.
    #
    # This algorithm is proved to work in the following paper:
    #
    #   M. H. Martin, "A problem in arrangements". Bull. Amer. Math.
    #   Soc. Volume 40, Number 12 (1934), 859-864.
    #
    #   http://projecteuclid.org/DPubS?service=UI&version=1.0&verb=Display&handle=euclid.bams/1183497876
    #
    # However, the proof in there is extremely wordy and can be
    # considerably shortened by the use of the graph-theoretic
    # language used in the comments in this code. So here's my
    # condensed version:
    #
    # We begin at the vertex corresponding to k-1 copies of zero.
    # Because the graph is Eulerian, we must return to that vertex
    # every time we leave it (we can't get stuck anywhere else). Each
    # time we leave the starting vertex and return to it, we use up
    # one of the n edges leaving it and one of the n edges returning.
    # So we must leave it, and return to it, n times in total before
    # we are left without a move.
    #
    # The algorithm causes us to use the edges leaving each vertex in
    # descending order. So if there is _any_ edge we never used at all
    # (== any tuple we failed to generate), then we must also have
    # failed to use the 0-labelled edge leaving the same source
    # vertex. Hence, the destination of that edge (some (k-1)-tuple
    # ending in 0) was entered at most n-1 times. Therefore, the
    # 0-labelled edge leaving that vertex can't have been used either,
    # and so we find a (k-1)-tuple ending in 00 which was also entered
    # at most n-1 times. Continuing this reasoning, we must sooner or
    # later end up at the zero vertex and conclude that _that_ was
    # entered fewer than n times. But that contradicts the fact that
    # we know we entered the starting vertex n times. So there can't
    # have been any edge we missed after all. []

    vfirst = v = (0,) * (k-1) # the all-zeroes vertex
    loop = []
    while len(vdict[v]) > 0:
        key, vnew = vdict[v].items()[-1]
        del vdict[v][key]
        v = vnew
        loop.append(v)
    assert vfirst == v
    assert len(loop) == outlen # must work first time

    # In this case, we set our output sequence to the _last_ element
    # of each vertex tuple, since that arranges that the output starts
    # prettily with all the (n-1)s and ends with all the 0s.
    seq = [x[-1] for x in loop]

# Make sure it worked, before we output!
seq2 = seq[:]
for i in range(k-1):
    seq2.append(seq2[i])
tuples = dict([(tuple(seq2[i:i+k]), 1) for i in range(len(seq))])
assert len(tuples) == len(seq)

if short_output:
    chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    assert n < len(chars)
    for i in seq:
        sys.stdout.write(chars[i])
    sys.stdout.write("\n")
else:
    print seq
