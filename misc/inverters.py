#!/usr/bin/env python

# Exhaustive search program to solve the two-inverters problem.
#
# The problem is: using any number of AND and OR gates but only two
# inverters, construct a logic circuit which takes three inputs
# A,B,C and inverts them all to give three outputs ~A,~B,~C. (And
# no other cheaty things like NAND, NOR, XOR, XNOR - the two NOTs
# are the only _non-increasing_ logic functions you're permitted.)

import sys

def closure(r):
    r = list(r)
    while 1:
        new = []
        for i in range(256):
            if r[i] == "": continue
            for j in range(256):
                if r[j] == "": continue
                new.append((i & j, "(" + r[i] + "&" + r[j] + ")"))
                new.append((i | j, "(" + r[i] + "|" + r[j] + ")"))
        reallynew = 0
        for k, s in new:
            if r[k] == "" or len(r[k]) > len(s):
                if r[k] == "":
                    reallynew = 1
                r[k] = s
        if not reallynew:
            break
    return r

def hexstr(x):
    s = hex(x)
    if s[:2] == "0x" or s[:2] == "0X": s = s[2:]
    while len(s) < 2: s = "0" + s
    return s

def display(r):
    for y in range(0,256,16):
        pfx = ""
        for x in range(y, y+16):
            if r[x]:
                sys.stdout.write(pfx + hexstr(x))
            else:
                sys.stdout.write(pfx + "  ")
            pfx = " "
        sys.stdout.write("\n")

# Compute every minterm which can be reached by ANDing and ORing
# together any combination of a,b,c.
r = [""] * 256
r[0] = "0"
r[255] = "1"
r[0x0F] = "a"
r[0x33] = "b"
r[0x55] = "c"

r = closure(r)
display(r)

# Now add a single inverter on each possible minterm.
inv1 = []
for k in range(256):
    if r[k] != "" and r[255 ^ k] == "":
        z = list(r)
        ks = "K = !" + z[k]
        z[255 ^ k] = "K"
        z = closure(z)
        inv1.append([k,ks,z])

# Now add a second inverter.

for (k0,ks0,r) in inv1:
    for k in range(256):
        if r[k] != "" and r[255 ^ k] == "":
            z = list(r)
            ks = "L = !" + z[k]
            z[255 ^ k] = "L"
            z = closure(z)
            success = (z[0xF0] != "" and z[0xCC] != "" and z[0xAA] != "")
            #print hexstr(k0), hexstr(k), success
            if success:
                print hexstr(k0), hexstr(k)
                print ks0
                print ks
                print "!a =", z[0xF0]
                print "!b =", z[0xCC]
                print "!c =", z[0xAA]
                #sys.exit(0)

#K = !((b|(a&c))&(a|c))
#L = !((c|((((b&c)|(a&K))|b)&K))&((b&(a&c))|K))
#
#~a = (((c&K)|((b&K)|L))&(((b&K)|((b&c)&L))|K))
#~b = (((c&K)|((a&K)|L))&(((a&K)|((a&c)&L))|K))
#~c = (((b&K)|((a&K)|L))&(((a&K)|((a&b)&L))|K))

# K = !((c|b)&((c|a)&(b|a)))
# L = !((K|(c&(b&a)))&((c|b)|(c|a)))
# ((K|(c&b))&(L|(K&(c|b))))
# ((K|(c&a))&(L|(K&(c|a))))
# ((K|(b&a))&(L|(K&(b|a))))

# K = !(((c|b)&(c|a))&((c|b)&(b|a)))
# L = !((K|((c&b)&(c&a)))&((c|b)|(c|a)))
# ((L&(K|(c&b)))|(K&(c|b)))
# ((L&(K|(c&a)))|(K&(c|a)))
# ((L&(K|(b&a)))|(K&(b|a)))
