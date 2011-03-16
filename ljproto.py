# Python module implementing basics of the LJ protocol.

import urllib
import os
import string

# Restore a sensible response to HTTP errors.

class MyURLopener(urllib.FancyURLopener):
    def http_error_default(self, url, fp, errcode, errmsg, headers):
        """Default error handler: close the connection and raise IOError."""
        void = fp.read()
        fp.close()
        raise IOError, str(errcode) + " " + errmsg

urllib._urlopener = MyURLopener()

def chomp(s):
    while s[-1:] == "\n" or s[-1:] == "\r":
        s = s[:-1]
    return s

LJ = {'url':"http://www.livejournal.com/interface/flat",
      'file':".ljscan/login",
      'abbr':"lj",
      'ABBR':"LJ",
      'checkfriends':"checkfriends",
      'logincookies':"/.ljscan/ljsesscookies",
      'friendssuffix':".livejournal.com/friends"}
DW = {'url':"http://www.dreamwidth.org/interface/flat",
      'file':".ljscan/dwlogin",
      'abbr':"dw",
      'ABBR':"DW",
      'checkfriends':"checkforupdates",
      'logincookies':"/.ljscan/dwsesscookies",
      'friendssuffix':".dreamwidth.org/read"}

def ljbasiccall(dict, site=LJ):
    "Do a basic LJ call."
    postdata = urllib.urlencode(dict)
    results = {}
    f = urllib.urlopen(site['url'], postdata)
    while 1:
        s = f.readline()
        if s == "":
            break
        s2 = f.readline()
        if s2 == "":
            break
        s = chomp(s)
        s2 = urllib.unquote_plus(chomp(s2))
        results[s] = s2
    f.close()
    return results

def ljcall(dict, site=LJ):
    "Do an LJ call, but find the user's login details and encode those first."
    cfg = os.environ["HOME"] + "/.ljscan/login"
    f = open(cfg, "r")
    while 1:
        s = f.readline()
        if s == "":
            break
        s = chomp(s)
        i = string.find(s, " ")
        assert i > 0
        key = s[:i]
        value = s[i+1:]
        dict[key] = value
    return ljbasiccall(dict, site)

def ljok(dict):
    "Determine whether an LJ call was successful."
    if dict.get("success", "OK") == "OK":
        return 1
    return 0

def ljerror(dict):
    "Return the error message from an unsuccessful LJ call."
    return dict.get("errmsg", "Server failed to return an error code")

def ljusername(site=LJ):
    "Retrieve the user's LJ login name."
    cfg = os.environ["HOME"] + "/" + site['file']
    f = open(cfg, "r")
    try:
        while 1:
            s = f.readline()
            if s == "":
                break
            s = chomp(s)
            i = string.find(s, " ")
            assert i > 0
            if s[:i] == "user":
                return s[i+1:]
    finally:
        f.close()
    return None

def ljuserhost(site=LJ):
    "Retrieve the user's LJ host name."
    name = ljusername(site)
    return string.replace(name, "_", "-")
