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

def shellquote(list):
    # Take a list of words, and produce a single string which
    # expands to that string of words under POSIX shell quoting
    # rules.
    ret = ""
    sep = ""
    for word in list:
	ok = 1
	for c in word:
	    if not (c in shelllist):
		ok = 0
		break
	if not ok:
	    word = "'" + string.replace(word, "'", "'\\''") + "'"
	ret = ret + sep + word
	sep = " "
    return ret

def rm_rf(dir):
    shutil.rmtree(dir, 1)
