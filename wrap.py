#!/usr/bin/env python

# magic paragraph wrapping algorithm

import sys
import string

def wrap(list):
    class holder:
	"container class"
	pass
    n = len(list)
    z = []
    for i in range(n-1, -1, -1):
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
	    else:
		cost = (width-wid) ** 2 + z[j].cost
	    if bestcost == None or bestcost > cost:
		bestcost = cost
		best = j
	if best == None:
	    z[0].cost = 0
	    z[0].n = n - i
	else:
	    z[0].cost = bestcost
	    z[0].n = best
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
	    sys.stdout.write(s)
	else:
	    para = para + k
    if len(para) > 0:
	wrap(para)

width = 72

donefile = 0
for arg in sys.argv[1:]:
    if arg[0] == "-":
	if arg[:2] == "-w":
	    width = string.atoi(arg[2:])
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
