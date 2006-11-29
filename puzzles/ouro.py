#!/usr/bin/env python

# Generate an ouroborean ring. Takes two arguments: number of
# symbols in the alphabet, and length of string to make unique.

import sys

args = sys.argv[1:]
short_output = 0

if len(args) > 0 and args[0] == "-s":#
    short_output = 1
    args = args[1:]

if len(args) != 2:
    sys.stderr.write("usage: " + sys.argv[0] + " [-s] " + \
    "<num-symbols> <substr-length>\n")
    sys.exit(len(args) > 0) # return 0 if called with no args for usage msg

n = int(args[0])
k = int(args[1])

used = {}

tmplist = [n-1] * (k-1) + [0] * k
for i in range(k):
    used[tuple(tmplist[i:i+k])] = 1

list = [0] * k

while 1:
    prefix = tuple(list[-(k-1):])   # last k-1 elements of the list
    next = -1
    for j in range(n):
        if used.get(prefix + (j,), 0) == 0:
            next = j
            break
    if next == -1:
        assert len(list) == n ** k
        assert list[-(k-1):] == [n-1] * (k-1)
        break
    list.append(next)
    used[prefix + (next,)] = 1

if short_output:
    chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    assert n < len(chars)
    for i in list:
        sys.stdout.write(chars[i])
    sys.stdout.write("\n")
else:
    print list
