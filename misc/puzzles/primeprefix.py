#!/usr/bin/env python

# Exhaustively find, by BFS, all natural numbers with the property
# that every prefix of them is prime. Works in many bases.

import sys
import string

def isprime(n):
    if n < 2:
        return 0
    factor = 2L
    while factor * factor <= n:
        if n % factor == 0:
            return 0
        factor = factor + 1
    return 1

def tryit(list, t):
    n, s = t
    for i in range(radix):
        d = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"[i]
        m = n*radix + i
        if isprime(m):
            list.append((m, s + d))
            print m, "=", s+d

radix = 10
args = sys.argv[1:]
if len(args) > 0:
    radix = string.atoi(args[0])

list = []
head = 0
tryit(list, (0, ""))
while head < len(list):
    tryit(list, list[head])
    head = head + 1
