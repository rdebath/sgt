#!/usr/bin/env python

# Simulate the generalisation of the number-guessing puzzle which
# goes like this:

# I think of a whole number between one and twelve inclusive. I
# whisper to Fred the first letter of the English name of that
# number, and I whisper to Larry its last letter. Fred and Larry
# then have the following conversation:
#
#      Fred 'I don't know what the number is.'
#     Larry 'I don't know what the number is.'
#      Fred 'I don't know what the number is.'
#     Larry 'Now I know what the number is.'
#      Fred 'So do I.'
#
# What is the number?

# This implementation expects to receive an upper limit on the
# command line (defaulting to 12 as in the original puzzle). It
# then models what would happen given each possible starting
# number, and outputs the results in the form
#
#    answer: [conversation] [set]
#
# where `answer' was the number I wrote down, `conversation' is a
# list of booleans in which `False' indicates that the person whose
# turn it is to speak says that they do not know the number and
# `True' indicates that they say they do (Fred speaks first in all
# cases), and `set' is the list of answers between which an
# unprivileged observer listening to that conversation would have
# been unable to distinguish.

import sys
import string

englishdigits = ["zero", "one", "two", "three", "four", "five", "six",
"seven", "eight", "nine"]
englishteens = ["ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
"sixteen", "seventeen", "eighteen", "nineteen"]
englishtens = [None, "ten", "twenty", "thirty", "forty", "fifty", "sixty",
"seventy", "eighty", "ninety"]
# I have long since given up prolonging the death agonies of the
# British billion. The American system is the VHS of numeric
# nomenclature: it is technically hideous, but it has won.
#
# This array stops at "trillion" because, well, it has to stop
# somewhere. It's quite tempting to try to write a _real_ integer-
# to-English converter which can handle any integer at all, but
# that would take actual research and in this particular case it's
# wildly unnecessary. Even "thousand" is pushing it for this
# application.
englishthousands = ["", " thousand", " million", " billion", " trillion"]
def english(n):
    def englishthreedigits(n):
        assert n >= 1 and n < 1000
        s = sep = ""
        if n >= 100:
            s = englishdigits[n/100] + " hundred"
            sep = " and "
            n = n % 100
        if n >= 20:
            s = s + sep + englishtens[n/10]
            sep = "-"
            n = n % 10
        if n >= 10:
            s = s + sep + englishteens[n-10]
        elif n >= 1:
            s = s + sep + englishdigits[n]
        return s
    assert n >= 1 and n < 1000000000000000L
    s = sep = ""
    for th in englishthousands:
        npart = n % 1000
        n = n / 1000
        if npart != 0:
            s = englishthreedigits(npart) + th + sep + s
            if th == "" and npart < 100:
                sep = " and "
            else:
                sep = ", "
    return s

def first(s):
    return s[:1]

def last(s):
    return s[-1:]

def known(set, ofunc, value):
    n = 0
    for i in set:
        if ofunc(i) == value:
            n = n + 1
    assert n > 0
    return n == 1

def narrow(set, ofunc, known):
    origsize = len(set)

    unique = []
    nonunique = []
    values = {}
    for i in set:
        value = ofunc(i)
        values[value] = values.get(value, 0) + 1
    for i in set:
        value = ofunc(i)
        if values[value] == 1:
            unique.append(i)
        else:
            nonunique.append(i)
    if known:
        set = unique
    else:
        set = nonunique

    assert len(set) > 0
    return set, len(set) != origsize

def guess(numbers, number):

    statements = []
    set = numbers
    k1 = k2 = 0

    while not (k1 and k2):
        changed = 0

        k1 = known(set, first, first(number))
        statements.append(k1)
        set, c1 = narrow(set, first, k1)

        if k1 and k2:
            break

        k2 = known(set, last, last(number))
        statements.append(k2)
        set, c2 = narrow(set, last, k2)

        if not (c1 or c2):
            break

    print number + ":", statements, set

args = sys.argv[1:]
if len(args) > 0:
    i = string.atoi(args[0])
else:
    i = 12

numbers = map(english, xrange(1,i+1))
for n in numbers:
    guess(numbers, n)
