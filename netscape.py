# Module to read and write Netscape bookmark files.

import types
import string
import htmllib
import formatter

import bookmarks

def makedict(attrs):
    attrdict = {}
    for attr in attrs:
	attrdict[string.lower(attr[0])] = attr[1]
    return attrdict

class netscape_bookmark_formatter(formatter.NullFormatter):
    def __init__(self):
	formatter.NullFormatter.__init__(self)
	self.text = None

    def start_accumulating(self):
	self.text = ""

    def end_accumulating(self):
	ret = self.text
	self.text = None
	return ret

    def add(self, data):
	if type(self.text) == types.StringType:
	    self.text = self.text + data

    def add_flowing_data(self, data):
	self.add(data)

    def add_literal_data(self, data):
	self.add(data)

class netscape_bookmark_parser(htmllib.HTMLParser):
    def __init__(self):
	htmllib.HTMLParser.__init__(self, netscape_bookmark_formatter())
	self.level = -1
	self.retval = bookmarks.Bookmarks()

    def start_dl(self, attrs):
	self.level = self.level + 1

    def end_dl(self):
	self.level = self.level - 1

    def start_h3(self, attrs):
	self.localdata = attrs
	self.formatter.start_accumulating()

    def end_h3(self):
	s = self.formatter.end_accumulating()
	self.retval.heading(self.level, s, self.localdata)

    def start_a(self, attrs):
	a = makedict(attrs)
	if a.has_key("href"):
	    self.url = a["href"]
	    attrs = attrs[:]
	    attrs.remove(("href", self.url))
	    self.localdata = attrs
	    self.formatter.start_accumulating()
	else:
	    self.url = None

    def end_a(self):
	if self.url:
	    name = self.formatter.end_accumulating()
	    self.retval.bookmark(self.level, name, self.url, self.localdata)

    def do_hr(self, attrs):
	self.retval.separator(self.level, attrs)

def read(file):
    """Takes either a file name or a file object, reads a Netscape-formatted\
    bookmark file, and returns a bookmark collection."""

    if type(file) == types.StringType:
	f = open(file, "r")
    else:
	f = file

    p = netscape_bookmark_parser()
    while 1:
	s = f.read(1024)
	if s == "": break
	p.feed(s)

    if type(file) == types.StringType:
	f.close()

    return p.retval

def escape(s):
    s = string.replace(s, "&", "&amp;")
    s = string.replace(s, "\"", "&quot;")
    s = string.replace(s, "<", "&lt;")
    s = string.replace(s, ">", "&gt;")
    return s

def localdata(list):
    ret = ""
    for k, v in list:
	ret = ret + " " + string.upper(k) + "=\"" + v + "\""
    return ret

def write(file, bcollect):
    """Takes either a file name or a file object, and writes the given\
    bookmark collection out to it in Netscape format."""

    if type(file) == types.StringType:
	f = open(file, "w")
    else:
	f = file

    f.write("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n" + \
    "<!-- This is an automatically generated file.\n" + \
    "     It will be read and overwritten.\n" + \
    "     DO NOT EDIT! -->\n" + \
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n" + \
    "<TITLE>Bookmarks</TITLE>\n" + \
    "<H1>Bookmarks</H1>\n" + \
    "\n" + \
    "<DL><p>\n")

    currlev = 0

    things = bcollect.output_hierarchy() + [(-1,"end")]

    for t in things:
	thislev = t[0]
	while currlev < thislev:
	    f.write("    " * (currlev+1) + "<DL><p>\n")
	    currlev = currlev + 1
	while currlev > thislev:
	    currlev = currlev - 1
	    f.write("    " * (currlev+1) + "</DL><p>\n")

	if t[1] == "end":
	    break

	f.write("    " * (currlev+1))
	
	if t[1] == "separator":
	    f.write("<HR" + localdata(t[2]) + ">")
	elif t[1] == "heading":
	    f.write("<DT><H3" + localdata(t[3]) + ">" + escape(t[2]) + "</H3>")
	elif t[1] == "bookmark":
	    url = string.replace(t[3], "\"", "%22")
	    f.write("<DT><A HREF=\"" + url + "\"")
	    f.write(localdata(t[4]) + ">" + escape(t[2]) + "</A>")

	f.write("\n")

    if type(file) == types.StringType:
	f.close()

# Temporary test code
#bc = read("bookmarks.html")
#write("output.html", bc)
