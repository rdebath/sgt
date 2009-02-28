# Miscellaneous support functions for my build system.

import string
import os
import shutil

import log

shelllist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
shelllist = shelllist + "%+,-./:=[]^_@"

class builderr:
    def __init__(self, s):
        self.s = s
    def __str__(self):
        return self.s

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

def numeric(s):
    return checkstr(s, "0123456789")

def rm_rf(dir):
    shutil.rmtree(dir, 1)
