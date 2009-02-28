#!/usr/bin/env python

import sys
import string

sampletext = [
"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG",
"the quick brown fox jumps over the lazy dog",
"0123456789`!\"£$%^&*()_+-={}[]:@~;'#<>?,./\\|"
]

class font:
    pass
class char:
    pass

def loadfont(file):
    "Load a font description from a text file."
    f = font()
    f.copyright = f.facename = f.height = f.ascent = None
    f.italic = f.underline = f.strikeout = 0
    f.weight = 400
    f.charset = 0
    f.pointsize = None
    f.chars = [None] * 256

    fp = open(file, "r")
    lineno = 0
    while 1:
        s = fp.readline()
        if s == "":
            break
        lineno = lineno + 1
        while s[-1:] == "\n" or s[-1:] == "\r":
            s = s[:-1]
        while s[0:1] == " ":
            s = s[1:]
        if s == "" or s[0:1] == "#":
            continue
        space = string.find(s, " ")
        if space == -1:
            space = len(s)
        w = s[:space]
        a = s[space+1:]
        if w == "copyright":
            if len(a) > 59:
                sys.stderr.write("Copyright too long\n")
                return None
            f.copyright = a
            continue
        if w == "height":
            f.height = string.atoi(a)
            continue
        if w == "facename":
            f.facename = a
            continue
        if w == "ascent":
            f.ascent = string.atoi(a)
            continue
        if w == "pointsize":
            f.pointsize = string.atoi(a)
            continue
        if w == "weight":
            f.weight = string.atoi(a)
            continue
        if w == "charset":
            f.charset = string.atoi(a)
            continue
        if w == "italic":
            f.italic = a == "yes"
            continue
        if w == "underline":
            f.underline = a == "yes"
            continue
        if w == "strikeout":
            f.strikeout = a == "yes"
            continue
        if w == "char":
            c = string.atoi(a)
            y = 0
            f.chars[c] = char()
            f.chars[c].width = 0
            f.chars[c].data = [0] * f.height
            continue
        if w == "width":
            f.chars[c].width = string.atoi(a)
            continue
        try:
            value = string.atol(w, 2)
            bits = len(w)
            if bits < f.chars[c].width:
                value = value << (f.chars[c].width - bits)
            elif bits > f.chars[c].width:
                value = value >> (bits - f.chars[c].width)
            f.chars[c].data[y] = value
            y = y + 1
        except ValueError:
            sys.stderr.write("Unknown keyword "+w+" at line "+"%d"%lineno+"\n")
            return None

    if f.copyright == None:
        sys.stderr.write("No font copyright specified\n")
        return None
    if f.height == None:
        sys.stderr.write("No font height specified\n")
        return None
    if f.ascent == None:
        sys.stderr.write("No font ascent specified\n")
        return None
    if f.facename == None:
        sys.stderr.write("No font face name specified\n")
        return None
    for i in range(256):
        if f.chars[i] == None:
            sys.stderr.write("No character at position " + "%d"%i + "\n")
            return None
    if f.pointsize == None:
        f.pointsize = f.height
    return f

f = loadfont(sys.argv[1])
image = []
maxwidth = 0
for s in sampletext:
    width = 0
    for c in map(ord, s):
        width = width + f.chars[c].width
    if maxwidth < width:
        maxwidth = width
    for y in range(f.height):
        value = 0L
        for c in map(ord, s):
            value = (value << f.chars[c].width) | f.chars[c].data[y]
        image.append((width, value))

print "/* XPM */"
print "static char *image[] = {"
print "/* columns rows colors chars-per-pixel */"
print "\"%d %d 2 1\"," % (maxwidth, len(image))
print "\"  c white\","
print "\"# c black\","
print "/* pixels */",
n = len(image)-1
for w, v in image:
    sys.stdout.write("\"")
    v = v << ((maxwidth - w)/2)
    for b in range(maxwidth-1, -1, -1):
        if (v & (1L << b)):
            sys.stdout.write("#")
        else:
            sys.stdout.write(" ")
    sys.stdout.write("\"");
    if n > 0:
        sys.stdout.write(",");
        n = n - 1
    sys.stdout.write("\n");
print "};"
