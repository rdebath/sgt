#!/usr/bin/env python

import sys
import os
import string
import popen2

# Requires my mathlib.py module.
import mathlib

shelllist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
shelllist = shelllist + "%+,-./:=[]^_@"

def checkstr(s, allowed_chars):
    # Return true iff the string s is composed entirely of
    # characters in the string `allowed_chars'.
    for c in s:
        if not (c in allowed_chars):
            return 0
    return 1

def shellquote(list):
    # Take a list of words, and produce a single string which
    # expands to that string of words under POSIX shell quoting
    # rules.
    ret = ""
    sep = ""
    for word in list:
        if not checkstr(word, shelllist):
            word = "'" + string.replace(word, "'", "'\\''") + "'"
        ret = ret + sep + word
        sep = " "
    return ret

spigot = "../spigot.py"
base = 10
a = None
b = None

args = sys.argv[1:]
doingargs = 1
while len(args) > 0:
    arg = args[0]
    del args[0]
    if doingargs:
        if arg[:2] == "-s" or arg[:2] == "-b":
            val = arg[2:]
            if val == "":
                if len(args) > 0:
                    val = args[0]
                    del args[0]
                else:
                    sys.stderr.write("powbegin.py: '%s' requires an argument\n" % arg[2:])
                    sys.exit(1)
            if arg[:2] == "-s":
                spigot = val
            elif arg[:2] == "-b":
                base = long(val)
            continue
        elif arg == "--":
            doingargs = 0
            continue
        elif arg[:1] == "-":
            sys.stderr.write("powbegin.py: unrecognised option '%s'\n" % arg)
            sys.exit(1)
    if a == None:
        a = string.atol(arg, base)
    elif b == None:
        b = string.atol(arg, base)
    else:
        sys.stderr.write("powbegin.py: unexpected extra argument '%s'\n" % arg)
        sys.exit(1)

if a == None or b == None:
    sys.stderr.write("usage: powbegin.py <a> <b>\n")
    sys.exit(1)

# Start receiving continued fraction convergents for log a.
acmd = shellquote([spigot, "-C", "div", "log", str(a), "log", str(base)])
apipe = os.popen(acmd, "r")

# Be prepared to compute approximations of log b and log(b+1).
bcmd = shellquote([spigot, "-a0", "div", "log", str(b), "log", str(base)])
bread, bwrite = popen2.popen2(bcmd)
b1cmd = shellquote([spigot, "-a0", "div", "log", str(b+1), "log", str(base)])
b1read, b1write = popen2.popen2(b1cmd)

# Repeatedly read a convergent, and try to work with it.
while 1:
    as = apipe.readline()
    if as == "": break
    i = string.find(as, "/")
    assert i >= 0
    n = long(as[:i])
    d = long(as[i+1:])
    if d < b*b:
        continue # not enough resolution, try again
    bwrite.write("%d\n" % d)
    bwrite.flush()
    bmin = bread.readline()
    assert bmin != ""
    bmin = long(bmin)
    b1write.write("%d\n" % d)
    b1write.flush()
    bmax = b1read.readline()
    assert bmax != ""
    bmax = long(bmax)
    if bmin >= bmax or n == 0 or n < b*b:
        continue # not enough resolution, try again
    ni = mathlib.invert(n, d)
    x = mathlib.modmin(-ni, d, 0, bmin, bmax+1)
    y = ni*x % d
    if y == 0:
        continue # still not enough resolution
    # Now we have our candidate power y. Test it with yet more
    # subinvocations of spigot. Hopefully this won't fail too
    # often.
    mincmd = shellquote([spigot, "div", "log", str(b), "log", str(base)])
    maxcmd = shellquote([spigot, "div", "log", str(b+1), "log", str(base)])
    powcmd = shellquote([spigot, "mul", str(y), "div", "log", str(a), "log", str(base)])
    minpipe = os.popen(mincmd, "r")
    maxpipe = os.popen(maxcmd, "r")
    powpipe = os.popen(powcmd, "r")
    # Discard integer parts.
    while 1:
        s = minpipe.read(1)
        assert s != ""
        if s == ".": break
    while 1:
        s = maxpipe.read(1)
        assert s != ""
        if s == ".": break
    while 1:
        s = powpipe.read(1)
        assert s != ""
        if s == ".": break
    # And read decimal digits until we find a discrepancy.
    mincmp = 0
    maxcmp = 0
    while 1:
        mindigit = minpipe.read(1)
        maxdigit = maxpipe.read(1)
        powdigit = powpipe.read(1)
        assert mindigit != ""
        assert maxdigit != ""
        assert powdigit != ""
        if mincmp == 0:
            mincmp = int(mindigit) - int(powdigit)
        if maxcmp == 0:
            maxcmp = int(maxdigit) - int(powdigit)
        if mincmp > 0 or maxcmp < 0 or (mincmp != 0 and maxcmp != 0):
            break
    minpipe.close()
    maxpipe.close()
    powpipe.close()
    if mincmp < 0 and maxcmp > 0:
        print y
        sys.exit(1)
