#!/usr/bin/env python

# Routine to break a permutation up into cycles.
#
# To use:
#
#     $ python -i perm.py
#     >>> cycles(lambda x: (2*x) % 13, range(13))
#     [[0], [1, 2, 4, 8, 3, 6, 12, 11, 9, 5, 10, 7]]
#     >>> cycles(lambda x: (3*x) % 13, range(13))
#     [[0], [1, 3, 9], [2, 6, 5], [4, 12, 10], [7, 8, 11]]

def cycles(f, l):
    ret = []
    l = list(l)

    while len(l) > 0:
        x = l[0]
        del l[0]
        this = [x]
        y = x
        while 1:
            y = f(y)
            if y == x:
                break
            assert y in l
            l.remove(y)
            this.append(y)
        ret.append(this)

    return ret
