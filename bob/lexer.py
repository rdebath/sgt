# Lexer and variable-expander for the scripts used in my build system.

import string
import os
import types
import time

import misc
import log
import checkout

whitespace = " \t"

onecharvars = {}
multicharvars = {}

removenewlines = string.maketrans("\r\n", "  ")

builddate = None

def save_vars():
    return (onecharvars.copy(), multicharvars.copy())

def restore_vars(t):
    onecharvars, multicharvars = t

def set_onecharvar(var, val):
    onecharvars[var] = val

def set_multicharvar(var, val):
    multicharvars[var] = val

def get_multicharvar(var, default=None):
    ret = multicharvars.get(var, default)
    if type(ret) == types.TupleType:
        # Function and parameter.
        ret = ret[0](ret[1])
    return ret

def unset_multicharvar(var):
    if multicharvars.has_key(var):
        del multicharvars[var]

def expand_varfunc(var, cfg):
    global builddate

    # Expand a variable or function enclosed in $(...). Takes a
    # string containing the text from inside the parentheses (after
    # any further expansion has been done on that); returns a
    # string containing the expansion.

    if var[0] == "!":
        # `$(!' introduces a special function.
        try:
            pos = var.index(" ")
            fn, val = var[1:pos], var[pos+1:]
        except ValueError:
            fn, val = var[1:], ""

        if fn == "numeric":
            # The entire function call has already been lexed, so
            # don't lex it again.
            log.logmsg("testing numericity of `%s'" % val)
            if misc.numeric(val):
                return "yes"
            else:
                return "no"
        elif fn == "builddate":
            if val != "":
                raise misc.builderr("$(!builddate) expects no arguments")
            if builddate is None:
                # Invent a build date.
                cachefile = os.path.join(cfg.builddatecache, cfg.mainmodule)
                builddate = time.strftime("%Y%m%d",time.localtime(time.time()))
                verdata = checkout.verdata()
                new = True
                if os.path.exists(cachefile):
                    with open(cachefile, "r") as f:
                        last_builddate = f.readline().rstrip("\r\n")
                        last_verdata = f.read()
                    if verdata == last_verdata:
                        new = False
                if new:
                    try:
                        os.mkdir(cfg.builddatecache, 0700)
                    except OSError as e:
                        if e.errno != os.errno.EEXIST:
                            raise
                    with open(cachefile + ".tmp", "w") as f:
                        f.write(builddate + "\n" + verdata)
                    os.rename(cachefile + ".tmp", cachefile)
                    log.logmsg("using new build date " + builddate)
                else:
                    builddate = last_builddate
                    log.logmsg("reusing last build date " + builddate)
            return builddate
        else:
            raise misc.builderr("unexpected string function `%s'" % var)
    else:
        # Just look up var in our list of variables, and return its
        # value.
        return get_multicharvar(var, "")

def internal_lex(s, terminatechars, permit_comments, cfg):
    # Lex string s until a character in `terminatechars', or
    # end-of-string, is encountered. Return a tuple consisting of
    #  - the lexed and expanded text before the terminating character
    #  - the remainder of the string, including the terminating
    #    character if any

    out = ""
    n = 0
    while n < len(s):
        c = s[n]
        if permit_comments and c == "#":
            break # effectively end-of-string
        if c in terminatechars:
            return (out, s[n:])

        n = n + 1
        if c == "$":
            # Things beginning with a dollar sign are special.
            c2 = s[n]
            n = n + 1
            if c2 == "$" or c2 == "#":
                out = out + c2
            elif c2 == "(":
                # We have a $(...) construct. Recurse to parse the
                # stuff inside the parentheses.
                varname, s2 = internal_lex(s[n:], ")", 0, cfg)
                if len(s2) == 0:
                    raise misc.builderr("`$(' without terminating `)'")
                out = out + expand_varfunc(varname, cfg)
                s = s2
                n = 1  # eat the terminating paren
            elif onecharvars.has_key(c2):
                out = out + onecharvars[c2]
            else:
                raise misc.builderr("unrecognised character `%c' after `$'" % c2)
        else:
            out = out + c

    # If we reach here, this is the end of the string.
    return (out, "")

def get_word(s, cfg):
    # Extract one word from the start of a given string.
    # Returns a tuple containing
    #  - the expanded word, or None if there wasn't one. (Words may
    #    be empty, which isn't the same as being absent.)
    #  - the rest of the string

    # Skip initial whitespace.
    while len(s) > 0 and s[0] in whitespace:
        s = s[1:]

    # If there's nothing left in the string at all, return no word.
    if len(s) == 0:
        return (None, "")

    # If there's a double quote, lex until the closing quote, and
    # treat doubled quotes in mid-string as literal quotes. We
    # expect whitespace or EOS immediately after the closing quote.
    if s[0] == '"':
        word = ""
        sep = ""
        while s[0] == '"':
            out, s = internal_lex(s[1:], '"', 0, cfg)
            word = word + sep + out
            if s[0] != '"':
                raise misc.builderr("unterminated double quote")
            s = s[1:]
            sep = '"'
        if len(s) > 0 and not (s[0] in whitespace):
            raise misc.builderr("expected whitespace after closing double quote")
        return (word, s)

    # Otherwise, just scan until whitespace.
    return internal_lex(s, whitespace, 1, cfg)

def lex_all(s, cfg):
    # Lex the entirety of a string. Returns the lexed version.
    ret, empty = internal_lex(s, "", 1, cfg)
    assert empty == ""
    return ret

def trim(s):
    # Just trim whitespace from the front of a string.
    while len(s) > 0 and s[0] in whitespace:
        s = s[1:]
    return s
