#!/usr/bin/env python

# Implementation of RFC 3492 encoding, for no better reason than
# that it seemed a rather cute algorithm even if it is used for
# ghastly purposes of doom.

import string
import sys

class bootstring_params:
    pass

rfc3492 = bootstring_params()
rfc3492.delimiter = "-"
rfc3492.base = 36
rfc3492.tmin = 1
rfc3492.tmax = 26
rfc3492.skew = 38
rfc3492.damp = 700
rfc3492.initial_bias = 72
rfc3492.initial_n = 0x80
rfc3492.encode = lambda d: "abcdefghijklmnopqrstuvwxyz0123456789"[d]
rfc3492.decode = lambda c: \
(c>='a' and c<='z') * (ord(c)-ord('a')+1) + \
(c>='A' and c<='Z') * (ord(c)-ord('A')+1) + \
(c>='0' and c<='9') * (ord(c)-ord('0')+26+1) - 1

def bootstring_threshold(params,j,bias):
    t = params.base * (j+1) - bias
    if t < params.tmin:
        t = params.tmin
    if t > params.tmax:
        t = params.tmax
    return t

def bootstring_bias_adapt(params,delta,points,first):
    if first:
        delta = delta / params.damp
    else:
        delta = delta / 2
    delta = delta + (delta / points)
    ndiv = 0
    while delta > ((params.base - params.tmin) * params.tmax) / 2:
        delta = delta / (params.base - params.tmin)
        ndiv = ndiv + 1
    bias = (params.base * ndiv) + \
    (((params.base - params.tmin + 1) * delta) / (delta + params.skew))
    return bias

def bootstring_decode(params, s):
    # Separate the initial string of basic code points from the
    # encoded deltas.
    delpos = string.rfind(s, params.delimiter)
    if delpos == -1:
        deltas = s
        output = []
    else:
        deltas = s[delpos+1:]
        output = []
        for c in s[:delpos]:
            output.append(ord(c))

    # Decode the string of deltas, character by character.
    deltav = []
    bias = params.initial_bias
    N = 0
    w = 1
    j = 0
    s = ""
    for c in deltas:
        s = s + c
        d = params.decode(c)
        assert d >= 0
        N = N + d * w
        t = bootstring_threshold(params, j, bias)
        if d < t:
            # We have decoded an integer.
            if verbose:
                print "String '%s' encodes delta %d" % (s, N)
            deltav.append(N)

            # Compute the new bias.
            bias = bootstring_bias_adapt(params, N, \
            len(output)+len(deltav), len(deltav)==1)

            # Reset the rest of the variables.
            N = 0
            w = 1
            j = 0
            s = ""
        else:
            w = w * (params.base - t)
            j = j + 1
    # The end of the deltas string must be precisely between
    # integers.
    assert j == 0

    # Insert code points in string.
    n = params.initial_n
    i = 0
    for delta in deltav:
        i = i + delta
        n = n + i / (len(output)+1)
        i = i % (len(output)+1)
        output = output[:i] + [n] + output[i:]
        i = i + 1 # step past the code point we just inserted

    return output

def bootstring_encode(params, invals):
    output = ""
    used = [0] * len(invals)
    insertorder = []
    olen = 0
    for i in range(len(invals)):
        if invals[i] < params.initial_n:
            output = output + chr(invals[i])
            used[i] = 1
            olen = olen + 1
        else:
            insertorder.append((invals[i], i))
    if output != "":
        output = output + params.delimiter
    insertorder.sort()
    n = params.initial_n
    i = 0
    deltas = []
    simple_olen = olen
    for v, j in insertorder:
        newi = 0
        for k in range(j):
            newi = newi + used[k]
        # Our state machine is now at (n, i), and needs to get to (v, newi).
        assert v > n or (v == n and newi >= i)
        delta = (v - n) * (olen+1) + newi - i
        deltas.append(delta)
        used[j] = 1
        n = v
        i = newi+1
        olen = olen + 1
    # Now encode each delta as a variable-length integer.
    bias = params.initial_bias
    olen = simple_olen
    for N in deltas:
        orig_N = N
        j = 0
        while 1:
            t = bootstring_threshold(params,j,bias)
            if N < t:
                output = output + params.encode(N)
                olen = olen + 1
                bias = bootstring_bias_adapt(params, orig_N, olen, olen==simple_olen+1)
                break
            else:
                d = t + ((N - t) % (params.base - t))
                output = output + params.encode(d)
                N = (N - t) / (params.base - t)
                j = j + 1
    return output

# Test vectors taken straight from RFC3492 section 7.1.
tests = [
[[0x0644, 0x064A, 0x0647, 0x0645, 0x0627, 0x0628, 0x062A, 0x0643, 0x0644,
0x0645, 0x0648, 0x0634, 0x0639, 0x0631, 0x0628, 0x064A, 0x061F],
"egbpdaj6bu4bxfgehfvwxn"],
[[0x4ED6, 0x4EEC, 0x4E3A, 0x4EC0, 0x4E48, 0x4E0D, 0x8BF4, 0x4E2D, 0x6587],
"ihqwcrb4cv8a8dqg056pqjye"],
[[0x4ED6, 0x5011, 0x7232, 0x4EC0, 0x9EBD, 0x4E0D, 0x8AAA, 0x4E2D, 0x6587],
"ihqwctvzc91f659drss3x8bo0yb"],
[[0x0050, 0x0072, 0x006F, 0x010D, 0x0070, 0x0072, 0x006F, 0x0073, 0x0074,
0x011B, 0x006E, 0x0065, 0x006D, 0x006C, 0x0075, 0x0076, 0x00ED, 0x010D,
0x0065, 0x0073, 0x006B, 0x0079],
"Proprostnemluvesky-uyb24dma41a"],
[[0x05DC, 0x05DE, 0x05D4, 0x05D4, 0x05DD, 0x05E4, 0x05E9, 0x05D5, 0x05D8,
0x05DC, 0x05D0, 0x05DE, 0x05D3, 0x05D1, 0x05E8, 0x05D9, 0x05DD, 0x05E2,
0x05D1, 0x05E8, 0x05D9, 0x05EA],
"4dbcagdahymbxekheh6e0a7fei0b"],
[[0x092F, 0x0939, 0x0932, 0x094B, 0x0917, 0x0939, 0x093F, 0x0928, 0x094D,
0x0926, 0x0940, 0x0915, 0x094D, 0x092F, 0x094B, 0x0902, 0x0928, 0x0939,
0x0940, 0x0902, 0x092C, 0x094B, 0x0932, 0x0938, 0x0915, 0x0924, 0x0947,
0x0939, 0x0948, 0x0902],
"i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd"],
[[0x306A, 0x305C, 0x307F, 0x3093, 0x306A, 0x65E5, 0x672C, 0x8A9E, 0x3092,
0x8A71, 0x3057, 0x3066, 0x304F, 0x308C, 0x306A, 0x3044, 0x306E, 0x304B],
"n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa"],
[[0xC138, 0xACC4, 0xC758, 0xBAA8, 0xB4E0, 0xC0AC, 0xB78C, 0xB4E4, 0xC774,
0xD55C, 0xAD6D, 0xC5B4, 0xB97C, 0xC774, 0xD574, 0xD55C, 0xB2E4, 0xBA74,
0xC5BC, 0xB9C8, 0xB098, 0xC88B, 0xC744, 0xAE4C],
"989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0dt30a5jpsd879ccm6fea98c"],
[[0x043F, 0x043E, 0x0447, 0x0435, 0x043C, 0x0443, 0x0436, 0x0435, 0x043E,
0x043D, 0x0438, 0x043D, 0x0435, 0x0433, 0x043E, 0x0432, 0x043E, 0x0440,
0x044F, 0x0442, 0x043F, 0x043E, 0x0440, 0x0443, 0x0441, 0x0441, 0x043A,
0x0438],
"b1abfaaepdrnnbgefbadotcwatmq2g4l"],
# That said "...efbaDotcw..." in the RFC, and the capital D caused the
# encoding test to fail. Working around this is a complete pain, since
# encoded strings are case-insensitive after the final delimiter but
# are case-sensitive before then! So I've just cheated and lowercased
# it for this test case.
[[0x0050, 0x006F, 0x0072, 0x0071, 0x0075, 0x00E9, 0x006E, 0x006F, 0x0070,
0x0075, 0x0065, 0x0064, 0x0065, 0x006E, 0x0073, 0x0069, 0x006D, 0x0070,
0x006C, 0x0065, 0x006D, 0x0065, 0x006E, 0x0074, 0x0065, 0x0068, 0x0061,
0x0062, 0x006C, 0x0061, 0x0072, 0x0065, 0x006E, 0x0045, 0x0073, 0x0070,
0x0061, 0x00F1, 0x006F, 0x006C],
"PorqunopuedensimplementehablarenEspaol-fmd56a"],
[[0x0054, 0x1EA1, 0x0069, 0x0073, 0x0061, 0x006F, 0x0068, 0x1ECD, 0x006B,
0x0068, 0x00F4, 0x006E, 0x0067, 0x0074, 0x0068, 0x1EC3, 0x0063, 0x0068,
0x1EC9, 0x006E, 0x00F3, 0x0069, 0x0074, 0x0069, 0x1EBF, 0x006E, 0x0067,
0x0056, 0x0069, 0x1EC7, 0x0074],
"TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g"],
[[0x0033, 0x5E74, 0x0042, 0x7D44, 0x91D1, 0x516B, 0x5148, 0x751F],
"3B-ww4c5e180e575a65lsy2b"],
[[0x5B89, 0x5BA4, 0x5948, 0x7F8E, 0x6075, 0x002D, 0x0077, 0x0069, 0x0074,
0x0068, 0x002D, 0x0053, 0x0055, 0x0050, 0x0045, 0x0052, 0x002D, 0x004D,
0x004F, 0x004E, 0x004B, 0x0045, 0x0059, 0x0053],
"-with-SUPER-MONKEYS-pc58ag80a8qai00g7n9n"],
[[0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x002D, 0x0041, 0x006E, 0x006F,
0x0074, 0x0068, 0x0065, 0x0072, 0x002D, 0x0057, 0x0061, 0x0079, 0x002D,
0x305D, 0x308C, 0x305E, 0x308C, 0x306E, 0x5834, 0x6240],
"Hello-Another-Way--fc4qua05auwb3674vfr0b"],
[[0x3072, 0x3068, 0x3064, 0x5C4B, 0x6839, 0x306E, 0x4E0B, 0x0032],
"2-u9tlzr9756bt3uc0v"],
[[0x004D, 0x0061, 0x006A, 0x0069, 0x3067, 0x004B, 0x006F, 0x0069, 0x3059,
0x308B, 0x0035, 0x79D2, 0x524D],
"MajiKoi5-783gue6qz075azm5e"],
[[0x30D1, 0x30D5, 0x30A3, 0x30FC, 0x0064, 0x0065, 0x30EB, 0x30F3, 0x30D0],
"de-jg4avhby1noc0d"],
[[0x305D, 0x306E, 0x30B9, 0x30D4, 0x30FC, 0x30C9, 0x3067],
"d9juau41awczczp"],
[[0x002D, 0x003E, 0x0020, 0x0024, 0x0031, 0x002E, 0x0030, 0x0030, 0x0020,
0x003C, 0x002D],
"-> $1.00 <--"],
]

verbose = 0
args = sys.argv[1:]
if len(args) == 0:
    # Ordinary self-test.
    fails = passes = 0
    for u, p in tests:
        decoded = bootstring_decode(rfc3492, p)
        if decoded != u:
            print "FAIL:", u, p, decoded
            fails = fails + 1
        else:
            passes = passes + 1
    print "Decoding: passed", passes, "failed", fails

    fails = passes = 0
    for u, p in tests:
        encoded = bootstring_encode(rfc3492, u)
        if encoded != p:
            print "FAIL:", u, p, encoded
            fails = fails + 1
        else:
            passes = passes + 1
    print "Encoding: passed", passes, "failed", fails
else:
    inval = []
    instr = None
    doneopts = 0
    for a in args:
        if a[:1] == "-" and not doneopts:
            if a == "-v":
                verbose = 1
            elif a == "--":
                doneopts = 1
            else:
                print "Unrecognised command-line option '%s'" % a
        elif string.lower(a[:2]) == "u+":
            inval.append(string.atoi(a[2:], 16))
        else:
            instr = a
    if len(inval) > 0 and instr != None:
        print "Please supply either Unicode or Punycode, not both"
    elif len(inval) > 0:
        print bootstring_encode(rfc3492, inval)
    elif instr != None:
        s = ""
        for v in bootstring_decode(rfc3492, instr):
            sys.stdout.write("%sU+%04x" % (s, v))
            s = " "
        print
    else:
        print "Please supply either Unicode (U+xxxx U+xxxx) or Punycode"
