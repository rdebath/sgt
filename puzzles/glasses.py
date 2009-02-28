#!/usr/bin/env python

# The glasses problem. You have an N-floor skyscraper and K
# glasses. You wish to determine how high, to the nearest floor, a
# glass has to be dropped from before it will break. All glasses
# can be assumed to be identical. You wish to optimise your
# strategy for the worst case: minimise the maximum possible number
# of drops. All your glasses are expendable.

# Suppose that at some point you currently have K glasses, and you
# are working with a range of N floors: that is, you know that the
# glass will not break if dropped from some height x, and will
# break if dropped from some height x+N. Let F(N,K) denote the
# maximum number of drops required if the optimal strategy is
# employed from this point.
#
# Boundary conditions: F(1,K) equals zero for all K (because if you
# know the glass doesn't break at some height x, and does break at
# x+1, the problem is already solved), and F(N,1) equals N-1 for
# all N (the only way you can guarantee to find the exact floor is
# to go up one floor at a time starting from x, and there are N-1
# floors to investigate between 0 and N).
#
# For any other values of N and K, the optimal strategy must
# involve dropping a glass from some height H. (Well, x+H where x
# is the known safe height, but we can call that zero for
# notational purposes.) If you drop a glass from height H and it
# breaks, you now have one fewer glass, and a gap of H between the
# known-safe and known-unsafe heights, so you need a further
# F(H,K-1) drops after this one. Whereas if you drop a glass from
# height H and it _doesn't_ break, you still have the same number
# of glasses, and the gap between known-safe and known-unsafe is
# now N-H. Thus, _given_ a drop height H, the maximum number of
# drops is 1 + max(F(H,K-1),F(N-H,K)). And the optimal strategy is
# defined to be that which minimises this value - so the correct
# initial drop height is whatever H minimises the above.
#
# Given this recurrence formula, you can draw up a table of N
# against K and go through it deriving the value of F (and also H)
# in every cell. This we now do.

nmax = 101
kmax = 8

f = [[0] * (nmax+1) for i in range(kmax+1)]
h = [[0] * (nmax+1) for i in range(kmax+1)]

for n in range(1,nmax+1):
    outputline = "%3d: " % n
    for k in range(1,kmax+1):
        # Boundary cases.
        if n == 1:
            fval = 0
            hval = None
        elif k == 1:
            fval = n - 1
            hval = 1
        else:
            # General case.
            fbest = None
            hbest = None
            for hthis in range(1,n):
                fthis = 1 + max(f[k-1][hthis], f[k][n - hthis])
                if fbest == None or fbest > fthis:
                    fbest = fthis
                    hbest = hthis
            assert fbest != None
            fval = fbest
            hval = hbest
        f[k][n] = fval
        h[k][n] = hval

        if hval != None:
            outputline = outputline + "%4d(%3d)" % (fval, hval)
        else:
            outputline = outputline + "%4d     " % fval

    print outputline
