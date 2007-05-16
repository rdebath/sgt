#!/usr/bin/env python

# Program to solve the following problem: how many people do you
# need before _three_ all share the same birthday?
#
# For the sake of exactness we assume that nobody is born on 29th
# Feb, and that birthdays are distributed equiprobably across the
# remaining 365 possible dates.

# This cache stores, for a tuple (n,d), the number (as a Python
# long integer) of birthday arrangements for n people (i.e.
# functions from (1,...,n) to (1,...,365), so we treat everything
# as distinct) such that no three share a birthday and exactly d
# _pairs_ of people do.
cache = {}
cache[(0,0)] = 1

# Total number of days in a year.
Y = 365

def p(n,d):
    # Determine a new value for the cache, as follows:
    #
    # If we are to end up with no triple birthdays and exactly d
    # doubles, this requires us to either add a new single birthday
    # to the arrangement (n-1,d), or to add a new double birthday
    # by putting our new person on top of one of the singletons in
    # (n-1,d-1).
    p1 = cache.get((n-1,d), 0)
    p2 = cache.get((n-1,d-1), 0)

    # In the former case, we had n-1-d distinct birthdays in use
    # already, so there are (Y-(n-1-d)) ways in which our new
    # person can fall into an unused slot.
    #
    # In the latter case, we had n-1-2*(d-1) singletons, so our new
    # person can land on top of one of those in (n-1-2*(d-1)) ways.
    p = p1 * (Y-(n-1-d)) + p2 * (n-1-2*(d-1))

    return p

def P(n):
    # Assuming the cache is completely filled in for row n-1, fill
    # it in for row n and return the total probability of there
    # being no triple birthdays.

    # For n people, our minimum number of duplicate birthdays is
    # given by max(n-Y,0). (With up to Y people it's possible to
    # have no duplicates, but with Y+1 there must be at least one
    # duplicate, and so on.) Our maximum is given by min(n/2,Y).
    # (The number of duplicate birthdays can be at most half the
    # number of people, and can also be at most Y.)
    dmin = max(n-Y, 0)
    dmax = min(n/2, Y)
    total = 0
    for d in range(dmin, dmax+1):
	k = cache[(n,d)] = p(n,d)
	total = total + k
    return total

for n in range(1,Y*2+2):
    N = P(n)
    D = 365 ** n
    assert 0 <= N and N <= D
    prob = (D-N) * (10**25) / D
    print n, "%d.%025d" % (prob / 10**25, prob % 10**25)
    #print n, N, D
