#!/usr/bin/env python

# magic paragraph wrapping algorithm

import sys
import string

width = 72
verbose = 0

def wrap(list):
    class holder:
	"container class"
	pass
    if verbose:
	print "width =", width
	print "word lengths:"
	lens = []
	for i in list:
	    lens.append(len(i))
	print lens
    n = len(list)
    z = []
    for i in range(n-1, -1, -1):
	if verbose:
	    print "wrapping from word", i
	wid = -1
	best = None
	bestcost = None
	z = [holder()] + z
	for j in range(1, n-i+1):
	    wid = wid + 1 + len(list[i+j-1])
	    if wid > width:
		break
	    if j >= len(z):
		cost = 0
		if verbose:
		    print j, "words: end of line, cost", cost
	    else:
		cost = (width-wid) ** 2 + z[j].cost
		if verbose:
		    print j, "words: cost", (width-wid) ** 2, "+", z[j].cost, "=", cost
	    if bestcost == None or bestcost >= cost:
		bestcost = cost
		best = j
	if best == None:
	    z[0].cost = 0
	    z[0].n = n - i
	else:
	    z[0].cost = bestcost
	    z[0].n = best
	if verbose:
	    print "optimal:", z[0].n, "words, at cost", z[0].cost
    i = 0
    while i < len(list):
	n = z[i].n
	sys.stdout.write(string.join(list[i:i+n], " ") + "\n")
	i = i + n

def dofile(f):
    para = []
    while 1:
	s = f.readline()
	if s == "":
	    break
	k = string.split(s)
	if len(k) == 0:
	    if len(para) > 0:
		wrap(para)
	    para = []
	    sys.stdout.write(s)
	else:
	    para = para + k
    if len(para) > 0:
	wrap(para)

donefile = 0
for arg in sys.argv[1:]:
    if arg[0] == "-":
	if arg[:2] == "-w":
	    width = string.atoi(arg[2:])
	elif arg[:2] == "-v":
	    verbose = 1
	else:
	    print "wrap: unknown option " + arg[:2]
	    print "usage: wrap [-wWIDTH] [file [file...]]"
	    sys.exit(0)
    else:
	donefile = 1
	f = open(arg, "r")
	dofile(f)
	f.close()

if not donefile:
    dofile(sys.stdin)
