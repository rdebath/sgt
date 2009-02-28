#!/usr/bin/env python

# Burrows-Wheeler transform.

import sys

def bw(s):
    list=[]
    for i in range(len(s)):
        list.append((s,i==0))
        s = s[1:] + s[0]
    l = len(list[0])
    list.sort()
    s=""
    for i in range(len(list)):
        t = list[i]
        s = s + t[0][-1]
        if t[1]:
            start = i
    return (start, s)

def mtf(s):
    order = range(256)
    ret = ""
    for c in s:
        i = ord(c)
        found = 0
        for k in range(len(order)):
            if order[k] == i:
                found = 1
                break
        assert found
        order = order[k:k+1] + order[0:k] + order[k+1:]
        ret = ret + chr(k)
    return ret

def invbw(pair):
    start, s = pair
    list = []
    for i in range(len(s)):
        list.append((s[i], i, start==i))
    list.sort()
    ret = ""
    pos = 0
    for i in range(len(s)):
        ret = ret + list[pos][0]
        if list[pos][2]:
            start = len(ret)
        pos = list[pos][1]
    ret = ret[start:] + ret[:start] # rotate to correct position
    return ret

def test(s):
    b = bw(s)
    print s
    print b
    print repr(mtf(b[1]))
    s2 = invbw(b)
    if s != s2:
        print "error: inverted to", s2

if len(sys.argv) > 1:
    f = open(sys.argv[1])
    s = f.read()
    f.close()
    test(s)
else:
    test("squiggles")
    test("abracadabra")
    test("the quick brown fox jumps over the lazy dog")
    test("pease porridge hot, pease porridge cold, pease porridge in the pot, nine days old")
