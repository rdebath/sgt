#!/usr/bin/env python

import string
import sys

# Disambiguating infix expression reader. We don't actually _parse_
# the expression, as such; we just classify every symbol into a
# syntactic category and output it highlighted to indicate this.
# Actual parsing would be an easy job after that, though.

# Expression classes and other constants.
ATOM, BINARYOP, UNARYOP, START, NO = range(5)
mATOM, mBINARYOP, mUNARYOP, mSTART, mNO = [1 << i for i in range(5)]

# Token list. Any alphanumeric string not in this list is treated
# as an unambiguous atom.
tokens = {
# Unambiguous binary operators.
'*': mBINARYOP,
'/': mBINARYOP,
'^': mBINARYOP,
'&': mBINARYOP,
'|': mBINARYOP,
'&&': mBINARYOP,
'||': mBINARYOP,
# Unambiguous unary operators.
'!' : mUNARYOP,
'~' : mUNARYOP,
# Unary/binary operators.
'+': mBINARYOP | mUNARYOP,
'-': mBINARYOP | mUNARYOP,
# Binary operators / atoms.
'and': mBINARYOP | mATOM,
'or': mBINARYOP | mATOM,
# Unary operators / atoms.
'not': mUNARYOP | mATOM,
'sin': mUNARYOP | mATOM,
'cos': mUNARYOP | mATOM,
'tan': mUNARYOP | mATOM,
# 3-way ambiguous.
'minus': mUNARYOP | mATOM | mBINARYOP,
# Non-operators.
'(': 0,
')': 0,
}

alphanum = "_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
whitespace = " \r\n\t\v"
def lex(s):
    ret = []
    while len(s) > 0:
        if s[0] in whitespace:
            s = s[1:]
            continue
        token = None
        if s[0] in alphanum:
            i = 1
            while i < len(s) and s[i] in alphanum:
                i = i + 1
            token = s[:i]
            s = s[i:]
        else:
            for k, v in tokens.items():
                if s[:len(k)] == k:
                    token = k
                    s = s[len(k):]
                    break
        if token == None:
            sys.stderr.write("Unable to lex: " + s)
            sys.exit(1)
        classes = tokens.get(string.lower(token), mATOM)
        ret.append((token, classes))
    return ret

def disambiguate(syms):
    n = len(syms)
    prevrole = [[None]*3 for i in range(n)]
    for i in range(n):
        for r in range(3):
            table_entry = NO
            if syms[i][1] & (1 << r):
                if r == BINARYOP:
                    possible_prev_roles = [ATOM]
                else:
                    possible_prev_roles = [BINARYOP, UNARYOP, START]
                for p in possible_prev_roles:
                    ok = 0
                    if p == START:
                        if i == 0:
                            ok = 1
                    else:
                        if i > 0 and prevrole[i-1][p] != NO:
                            ok = 1
                    if ok:
                        table_entry = p
                        break
            prevrole[i][r] = table_entry
    if prevrole[n-1][ATOM] == 'n':
        return None
    output = [None] * n
    r = ATOM
    for i in range(n-1,-1,-1):
        output[i] = (syms[i][0], 1 << r)
        r = prevrole[i][r]
    return output

def parse(syms):
    # Basically a shift-reduce sort of style. We accumulate tokens
    # in an output buffer; every time we close a parenthesis we
    # call the disambiguator to handle everything inside that, and
    # then reduce the parentheses to a single notional atom.
    stack = []
    for sym in [('(',0)] + syms + [(')',0)]:
        stack.append(sym) # shift
        if stack[-1][0] == ")":
            # Reduce. First search backward for the open paren.
            found=0
            for i in range(len(stack)-1, -1, -1):
                if stack[i][0] == "(":
                    found=1
                    break
            if not found:
                sys.stderr.write("Parse error (unmatched ')')\n")
                sys.exit(1)
            newlist = disambiguate(stack[i+1:-1])
            if newlist == None:
                sys.stderr.write("Parse error (unable to classify symbols)\n")
                sys.exit(1)
            stack = stack[:i] + [(newlist, mATOM)]
    # Now we expect our stack to contain exactly one item,
    # otherwise we have another kind of parse error.
    if len(stack) != 1:
        sys.stderr.write("Parse error (unmatched '(')\n")
        sys.exit(1)
    return stack[0][0]

col_delim = "\033[1;33m"
col_binary = "\033[1;31m"
col_unary = "\033[1;32m"
col_atom = "\033[1;34m"
col_finish = "\033[39;0m"
def printsyms(syms):
    sep = ""
    for sym in syms:
        sys.stdout.write(sep)
        sep = " "
        if sym[1] == mATOM:
            if type(sym[0]) != type("string"):
                sys.stdout.write(col_delim + "(" + col_finish)
                # recurse into parens
                printsyms(sym[0])
                sys.stdout.write(col_delim + ")" + col_finish)
            else:
                sys.stdout.write(col_atom + sym[0] + col_finish)
        elif sym[1] == mBINARYOP:
            sys.stdout.write(col_binary + sym[0] + col_finish)
        elif sym[1] == mUNARYOP:
            sys.stdout.write(col_unary + sym[0] + col_finish)
        else:
            raise "huh?"

while 1:
    s = sys.stdin.readline()
    if s == "": break
    syms = lex(s)
    syms = parse(syms)
    printsyms(syms)
    sys.stdout.write("\n")
