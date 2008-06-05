# Cookie-supporting wrapper around urllib.

import urllib
import urlparse
import string
import os

cookiedb = {} # maps (domain, cookiename) to value

def do_cookies(headers):
    cookies = {}
    if headers == None: return
    for i in headers.getallmatchingheaders("Set-Cookie"):
        assert i[:12] == "Set-Cookie: "
        cookie = i[12:]
        parts = map(string.strip, string.split(cookie, ";"))
        domain = None
        for part in parts[1:]:
            if part[:7] == "domain=":
                domain = part[7:]
        j = string.find(parts[0], "=")
        key = parts[0][:j]
        value = parts[0][j+1:]
        if value == "":
            try:
                del cookiedb[(domain, key)]
            except KeyError:
                pass
        else:
            cookiedb[(domain, key)] = value

def setup_cookies(opener, url):
    p = urlparse.urlparse(url)
    host = p[1]
    j = string.find(host, ":")
    if j >= 0:
        host = host[:j]
    cookielist = []
    for (domain, key), value in cookiedb.items():
        if domain == None or (len(domain) <= len(host) and host[-len(domain):] == domain):
            cookielist.append(key+"="+value)
    opener.addheaders = filter(lambda x: x[0] != "Cookie", opener.addheaders)
    opener.addheaders.append(("Cookie", string.join(cookielist, "; ")))

# Process cookies in HTTP error responses
class MyURLopener(urllib.FancyURLopener):
    def http_error(self, url, fp, errcode, errmsg, headers, data=None):
        do_cookies(headers)
        if errcode == 302:
            location = headers.getheader("Location")
            setup_cookies(self, location)
        return urllib.FancyURLopener.http_error(self, url, fp, errcode, errmsg, headers, data)
myurlopener = MyURLopener()
def urlopen(url, data=None):
    setup_cookies(myurlopener, url)
    ret = myurlopener.open(url, data)
    do_cookies(ret.info())
    return ret

def chomp(s):
    while s[-1:] == "\n" or s[-1:] == "\r":
        s = s[:-1]
    return s

def loadcookies(file):
    try:
        sessf = open(file, "r")
        while 1:
            cookie = sessf.readline()
            if cookie == "": break
            cookie = chomp(cookie)
            index = string.find(cookie, " ")
            index2 = string.find(cookie, " ", index+1)
            if index >= 0 and index2 >= 0:
                domain = cookie[:index]
                key = cookie[index+1:index2]
                value = cookie[index2+1:]
                cookiedb[(domain, key)] = value
        sessf.close()
    except IOError, e:
        pass # if we couldn't find it, then fair enough

def savecookies(file):
    fd = os.open(file, os.O_TRUNC | os.O_CREAT | os.O_WRONLY, 0600)
    file = os.fdopen(fd, "w")
    for (domain, key), value in cookiedb.items():
        file.write("%s %s %s\n" % (domain, key, value))
    file.close()
