# RGTP client module

import sys
import socket
import string
import time
import md5
import os

class RGTPConnection:
    "Class to manage an RGTP connection"
    # variable skt: socket
    # variable sfr: socket file for reading, so we can readline()
    # variable readaccess: boolean
    # variable writeaccess: boolean
    # variable loggedin: boolean
    # variable secrets: dictionary
    # variable plogfile: file to log protocol exchanges to

    class RGTPContribution:
	"Class that holds an RGTP contribution.\n"\
	"Contents are:\n"\
	"  `timestamp' - a Python floating point timestamp\n"\
	"  `sequence'  - the sequence number (numeric)\n"\
	"  `grogname'  - the grogname of the author, or \"\"\n"\
	"  `userid'    - the userid of the author, or \"\"\n"\
	"  `skip'      - the number of initial lines of `text' to skip\n"\
	"  `end'       - the index after the last useful line of `text'\n"\
	"  `text'      - a list of strings (with no line termination)"

    class RGTPItem:
	"Class that holds an RGTP item.\n"\
	"Contents are:\n"\
	"  `itemid'    - the itemid of this item\n"\
	"  `contfrom'  - the itemid this is a continuation of, or \"\"\n"\
	"  `contin'    - the itemid this is continued in, or \"\"\n"\
	"  `subject'   - the subject of this item\n"\
	"  `editseq'   - the sequence number of the last edit, or -1\n"\
	"  `replseq'   - the sequence number of the last reply, or -1\n"\
	"  `contribs'  - a list of RGTPContributions"

    class RGTPIndexEntry:
	"Class that holds an RGTP index entry.\n"\
	"Contents are:\n"\
	"  `itemid'    - the itemid of the item involved\n"\
	"  `sequence'  - the sequence number\n"\
	"  `timestamp' - a Python floating point timestamp\n"\
	"  `userid'    - the userid of the user who performed the action\n"\
	"  `subject'   - the subject of the item involved\n"\
	"  `type'      - the type of index entry [RICFEM]"

    def _timecvt(self, timestamp):
	"Convert a GROGGS timestamp (hex seconds since 1 Jan 1970 GMT)\n"\
	"into a Python timestamp (seconds since an unspecified epoch)"
	# First convert 1 Jan 1970 _local_ time into a Python timestamp
	epochtuple = (1970, 1, 1, 0, 0, 0, 0, 0, 0)
	try1 = time.mktime(epochtuple)
	# Now convert that back into GMT and see how far it went wrong
	errortuple = time.gmtime(try1)
	# Convert _that_, in local time, into a Python timestamp
	try2 = time.mktime(errortuple)
	# Now try2-try1 should equal try1-rightanswer. This should not
	# fail, even if daylight saving happens very near the start of
	# the year, because we have set the DST flag zero in both calls
	# to mktime.
	epoch = 2*try1-try2
	return epoch + string.atoi(timestamp, 16)

    def _getline(self):
	"Internal function to read line from socket. Removes \r\n"
	s = self.sfr.readline()
	while s[-1:] == "\r" or s[-1:] == "\n":
	    s = s[:-1]
	if self.plogfile != None:
	    self.plogfile.write("<<< "+s+"\n")
	return s

    def _putline(self, s):
	"Internal function to write line to socket. Adds \r\n"
	if self.plogfile != None:
	    self.plogfile.write(">>> "+s+"\n")
	self.skt.send(s + "\r\n")

    def _getdata(self):
	"Internal function to get multi-line data. Does not interpret ^\n" \
	"or ^-doubling; that's left to another function."
	ret = []
	while 1:
	    s = self._getline()
	    if s == ".":
		break # we're done
	    elif len(s) > 0 and s[0] == ".":
		s = s[1:] # remove dot-doubling
	    ret = ret + [s]
	return ret

    def _hex2bin(self, s):
	"Convert a string of hex digits, 2 by 2, to a string of binary bytes"
	ret = ""
	while len(s) > 0:
	    ret = ret + chr(string.atoi(s[0:2], 16))
	    s = s[2:]
	return ret

    def _bin2hex(self, s):
	"Convert a string of binary bytes to a string of hex digits"
	ret = ""
	while len(s) > 0:
	    h = hex(0x100 + ord(s[0]))
	    h = h[3:5] # strip off 0x1 and potential trailing L
	    ret = ret + h
	    s = s[1:]
	return string.upper(ret)

    def _bitrev(self, s):
	"Flip every bit in every byte in a string"
	s1 = ""
	s2 = ""
	for i in range(256):
	    s1 = s1 + chr(i)
	    s2 = chr(i) + s2
	return string.translate(s, string.maketrans(s1, s2))

    def __init__(self, host="rgtp-serv.groggs.group.cam.ac.uk", port=1431,\
    plogfile=None):
	self.plogfile = plogfile

	self.skt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	self.skt.connect(host, port)
	self.sfr = self.skt.makefile("r")

	greet = self._getline()
	if greet[0:3] == "230": # no access until we log in
	    self.readaccess = 0
	    self.writeaccess = 0
	elif greet[0:3] == "231": # readonly access (until we log in)
	    self.readaccess = 1
	    self.writeaccess = 0
	else:
	    throwerror() # 482 access denied, or maybe other oddities
	self.loggedin = 0
	self.secrets = {}

    def setsecret(self, filename):
	f = open(filename, "r")
	while 1:
	    line = f.readline()
	    if line == "":
		break
	    words = string.split(line)
	    if len(words) == 2:
		self.secrets[words[0]] = self._hex2bin(words[1])

    def login(self, user, level=2):
	self._putline("USER " + user + " " + "%d" % level)
	done = 0
	while not done:
	    s = self._getline()
	    if s[0:3] == "231":
		self.readaccess = 1 # we have acquired read access
		done = 1
	    elif s[0:3] == "232" or s[0:3] == "233":
		self.readaccess = 1 # we have acquired read access
		self.writeaccess = 1 # we have acquired write access
		# for 233, we also have edit access, but this client
		# doesn't support it
		done = 1
	    elif s[0:3] == "130":
		# get the algorithm - should be MD5
		if s[4:7] != "MD5":
		    throwerror() # MD5 it ain't
		# get the following 333 data
		s2 = self._getline()
		if s2[0:3] != "333" or len(s2) != 3+1+32:
		    throwerror() # it isn't there!
   	        servernonce = self._hex2bin(s2[4:4+32])
	        # invent a client nonce
	        d = md5.new()
	        d.update(time.asctime(time.gmtime(time.time())))
	        d.update(user)
	        d.update(servernonce) # might as well!
	        clientnonce = d.digest()
	        # now we can compute the server-hash
	        d = md5.new()
	        d.update(servernonce)
	        d.update(clientnonce)
	        d.update((user+"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")[0:16])
	        d.update(self.secrets["md5-secret"])
	        serverhash = d.digest()
	        # and we can compute the client-hash
	        d = md5.new()
	        d.update(clientnonce)
	        d.update(servernonce)
	        d.update((user+"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")[0:16])
	        d.update(self._bitrev(self.secrets["md5-secret"]))
	        clienthash = d.digest()
	        # now we can perform the AUTH exchange!
	        self._putline("AUTH " + self._bin2hex(clienthash) + " " + self._bin2hex(clientnonce))
	        s3 = self._getline()
	        if s3[0:3] != "133" or s3[4:] != self._bin2hex(serverhash):
		    throwerror() # server hash wasn't present and correct
	    elif s[0:3] == "330":
	        throwerror() # we shouldn't get this in MD5 mode
	    else:
	        throwerror() # 280 ok-will-email, 482 access-denied, other oddities

    def motd(self):
	"Retrieve the MOTD, as an RGTPContribution"
	if not self.readaccess:
	    throwerror()
	self._putline("MOTD")
	s = self._getline()
	if s[0:3] == "410":
	    return None # there ain't no motd
	elif s[0:3] == "250":
	    motd = self.RGTPContribution()
	    motd.data = self._getdata()
	    # First line of MOTD is a timestamp and sequence number.
	    if len(motd.data) == 0:
		throwerror()
	    words = string.split(motd.data[0])
	    motd.data[0:1] = []
	    # Protocol oddity: the RGTP server seems to return the MOTD
	    # timestamp before the sequence number, instead of after it
	    # as the protocol spec would suggest.
	    motd.timestamp = self._timecvt(words[0])
	    motd.sequence = string.atol(words[1], 16)
	    motd.grogname = ""
	    motd.userid = ""
	    # Rest of MOTD is ^-doubled
	    for i in range(len(motd.data)):
		if motd.data[i][0] == "^":
		    motd.data[i] = motd.data[i][1:]
	else:
	    throwerror() # oddities
	return motd

    def index(self, sequence=-1L, timestamp=0):
	"Retrieve the index. FIXME: not properly working yet"
	if not self.readaccess:
	    throwerror()
	if sequence != -1L:
	    self._putline("INDX #" + "%08x" % int(sequence & 0xFFFFFFFFL))
	elif timestamp != 0:
	    self._putline("INDX " + "%08x" % int(timestamp)) # FIXME
	else:
	    self._putline("INDX")
	s = self._getline()
	index = []
	if s[0:3] == "410":
	    return index
	elif s[0:3] == "250":
	    rawindex = self._getdata()
	    for s in rawindex:
		if len(s) < 104:
		    throwerror()
		i = self.RGTPIndexEntry()
		i.sequence = string.atol(s[0:8], 16)
		i.timestamp = self._timecvt(s[9:17])
		i.itemid = s[18:26]
		if i.itemid[0] == " ": i.itemid = ""
		i.userid = string.rstrip(s[27:102])
		i.type = s[103:104]
		i.subject = string.rstrip(s[105:])
		index = index + [i]
	else:
	    throwerror() # oddities
	return index

    def item(self, itemid):
	"Retrieve an item, as an RGTPItem"
	if not self.readaccess:
	    throwerror()
	self._putline("ITEM " + itemid)
	s = self._getline()
	if s[0:3] == "410":
	    return None # item doesn't exist
	elif s[0:3] == "250":
	    data = self._getdata()
	    # First line of data should be continuation info
	    if len(data) == 0:
		throwerror()
	    item = self.RGTPItem()
	    item.itemid = itemid
	    item.contfrom = data[0][0:8]
	    if item.contfrom[0] == " ": item.contfrom = ""
	    item.contin = data[0][9:17]
	    if item.contin[0] == " ": item.contin = ""
	    if data[0][18] == " ":
		item.editseq = -1L
	    else:
		item.editseq = string.atol(data[0][18:26], 16)
	    if data[0][27] == " ":
		item.replseq = -1L
	    else:
		item.replseq = string.atol(data[0][27:35], 16)
	    # Rest of data is ^-doubled list of RGTPContributions
	    data[0:1] = []
	    item.contribs = []
	    item.subject = "[notfound]"
	    while len(data) > 0:
		if data[0][0] != "^":
		    throwerror()
		contrib = self.RGTPContribution()
		words = string.split(data[0][1:])
		contrib.sequence = string.atol(words[0], 16)
		contrib.timestamp = self._timecvt(words[1])
		contrib.data = []
		for i in range(1,len(data)):
		    if len(data[i]) > 1 and data[i][0:2] == "^^":
			contrib.data = contrib.data + [data[i][1:]]
		    elif len(data[i]) > 0 and data[i][0] == "^":
			break
		    else:
			contrib.data = contrib.data + [data[i]]
		else:
		    i = len(data)
		data = data[i:]
		# Now we have our contribution in raw form, find the
		# grogname and userid.
		# Algorithm is :-
		#   Admin lines are up to and including first blank line.
		#   If there are 3 admin lines, the second contains
		#   the grogname preceded by "From ", while the first
		#   contains the userid as the last word before the
		#   standard GROGGS date format ("at 09.56 on Fri 14 Jan").
		#   If there are only 2 admin lines, the first contains
		#   the userid _in parens_ before the date, and the grogname
		#   is everything between the first " from " and the userid's
		#   opening paren. Unless there are no parens in the admin
		#   line, in which case the userid is stored as in the 3-line
		#   case.
		# The start of an item has one extra admin line just before
		# the blank one; this contains "Subject: " and then the subject
		# of the item.
		admin = 0
		while admin < len(contrib.data) and contrib.data[admin] != "":
		    admin = admin + 1
		if admin < len(contrib.data):
		    admin = admin + 1
		contrib.skip = admin
		if len(item.contribs) == 0:
		    admin = admin - 1 # offset for following tests
		    if contrib.data[admin-1][:9] == "Subject: ":
			item.subject = contrib.data[admin-1][9:]
		if admin == 3 and contrib.data[1][0:5] == "From ":
		    contrib.grogname = contrib.data[1][5:]
		elif admin == 2 and string.rfind(contrib.data[0], ")") > 0:
		    lparen = string.rfind(contrib.data[0], "(")
		    fr = string.find(contrib.data[0], "from ")
		    if lparen > 0 and fr >= 0:
			contrib.grogname = contrib.data[0][fr+5:lparen-1]
		else:
		    contrib.grogname = ""
		words = string.split(contrib.data[0])
		if len(words) > 6 and words[-6] == "at":
		    contrib.userid = words[-7]
		    if contrib.userid[0] == "(" and contrib.userid[-1] == ")":
			contrib.userid = contrib.userid[1:-1] # remove parens
		else:
		    contrib.userid = ""
		contrib.end = len(contrib.data)
		if contrib.end > 0 and contrib.data[contrib.end-1] == "":
		    contrib.end = contrib.end - 1
		item.contribs = item.contribs + [contrib]
	    if item.contin:
		# An item which has been continued elsewhere will have its
		# final contribution being small and blank. Check that this
		# is the case, and remove it.
		if item.contribs[-1].end <= item.contribs[-1].skip:
		    item.contribs[-1:] = []

	else:
	    throwerror() # oddities
	return item

    def close(self):
	self._putline("QUIT")
	s = self._getline()
	if s[0:3] != "280":
	    throwerror()
	self.skt.close()
