# Reference Python implementation of Rijndael.

# Global variables expected :-
#   Nb == no of columns in cipher block (block size / 32; can be 4-8)
#   Nk == no of columns in cipher key (key len / 32)
#   Nr == no of rounds
#   State == array of Nb four-byte arrays

# ----------------------------------------------------------------------
# Diagnostic functions.

def hexbyte(x):
    s = hex(x)
    if s[0:2] == "0x": s = s[2:]
    if s[-1:] == "L": s = s[:-1]
    while len(s) < 2: s = "0" + s
    return s

def writebox(x):
    "Format an array of 4-byte vectors into a string."
    ret = ""
    comma = ""
    for i in x:
	ret = ret + comma + hexbyte(i[0]) + ":" + hexbyte(i[1]) + ":" + \
	hexbyte(i[2]) + ":" + hexbyte(i[3])
	comma = ", "
    return ret

def writestr(x):
    ret = ""
    colon = ""
    for i in x:
	ret = ret + colon + hexbyte(i)
	colon = ":"
    return ret

# ----------------------------------------------------------------------
# Implement the finite field GF(2^8), as polynomials in GF(2) modulo
# the polynomial x^8+x^4+x^3+x+1.

# We won't bother to implement addition because it's just XOR.

def pmul(x,y):
    "Multiplication in GF(2^8)."
    r = 0
    # Keep doubling x and adding it into r according to y.
    while y > 0:
	if y & 1:
	    r = r ^ x
	y = y >> 1
	x = x << 1
	if x & 0x100:
	    x = x ^ 0x11B
    return r

# Examples of GF(2^8) multiplication given in the spec.
assert pmul(0x57, 0x83) == 0xC1
assert pmul(0x57, 0x13) == 0xFE

# Now compute inverses.
inverses = range(256)
for i in range(1,256):
    for j in range(1,256):
	if pmul(i, j) == 1:
	    inverses[i] = j
	    inverses[j] = i

def pinv(x):
    return inverses[x]

# ----------------------------------------------------------------------
# Cipher components.

def setup_state(bytes):
    global State
    State = []
    bytes = tuple(bytes)
    for i in range(Nb):
	column = list(bytes[0:4])
	bytes = bytes[4:]
	State.append(column)

def extract_output():
    bytes = []
    for i in State:
	bytes.extend(i)
    return tuple(bytes)

def Sbox(a):
    a = pinv(a)
    a = a ^ (a << 1) ^ (a << 2) ^ (a << 3) ^ (a << 4)
    a = (a & 0xFF) ^ (a >> 8)
    return a ^ 0x63

Sboxinvtable = range(256)
for i in range(256):
    S = Sbox(i)
    Sboxinvtable[S] = i

def Sboxinv(x):
    return Sboxinvtable[x]

def bytesub():
    for i in State:
	for j in range(4):
	    i[j] = Sbox(i[j])

def subbyte(x):
    i = list(tuple(x))
    for j in range(4):
	i[j] = Sbox(i[j])
    return i

def shiftrow():
    if Nb == 4: Cj = 0, 1, 2, 3
    if Nb == 5: Cj = 0, 1, 2, 3
    if Nb == 6: Cj = 0, 1, 2, 3
    if Nb == 7: Cj = 0, 1, 2, 4
    if Nb == 8: Cj = 0, 1, 3, 4
    for j in range(4):
	foo = []
	for i in range(Nb):
	    foo.append(State[i][j])
	C = Cj[j]
	foo = foo[C:] + foo[:C]
	for i in range(Nb):
	    State[i][j] = foo[i]

def mixonecolumn(inv, a0, a1, a2, a3):
    if inv:
	p3, p2, p1, p0 = 0x0B, 0x0D, 0x09, 0x0E
    else:
	p3, p2, p1, p0 = 3, 1, 1, 2
    b0 = pmul(a0,p0) ^ pmul(a1,p3) ^ pmul(a2,p2) ^ pmul(a3,p1)
    b1 = pmul(a0,p1) ^ pmul(a1,p0) ^ pmul(a2,p3) ^ pmul(a3,p2)
    b2 = pmul(a0,p2) ^ pmul(a1,p1) ^ pmul(a2,p0) ^ pmul(a3,p3)
    b3 = pmul(a0,p3) ^ pmul(a1,p2) ^ pmul(a2,p1) ^ pmul(a3,p0)
    return [b0, b1, b2, b3]

def mixcolumn(inv):
    for i in range(Nb):
	a0, a1, a2, a3 = State[i]
	State[i] = mixonecolumn(inv, a0, a1, a2, a3)

def addroundkey(roundkey):
    #print "rk: " + writebox(roundkey)
    for i in range(Nb):
	for j in range(4):
	    State[i][j] = State[i][j] ^ roundkey[i][j]

# ----------------------------------------------------------------------
# Key schedule. Takes a 4*Nk byte key, and produces Nr+1 round keys of
# Nb 4-byte vectors each.

def keyschedule(key):
    W = []
    for i in range(Nk):
	W.append(key[0:4])
	key = key[4:]
    rcon = 1
    for i in range(Nk, Nb * (Nr+1)):
	temp = list(tuple(W[i-1]))
	if i % Nk == 0:
	    temp = temp[1:] + temp[:1]
	    temp = subbyte(temp)
	    temp[0] = temp[0] ^ rcon
	    rcon = pmul(rcon, 2)
	elif i % Nk == 4 and Nk > 6:
	    temp = subbyte(temp)
	temp2 = W[i-Nk]
	for i in range(4):
	    temp[i] = temp[i] ^ temp2[i]
	W.append(temp)

    # That's expanded the key into Nb * (Nr+1) vectors. Now separate into
    # round keys.
    roundkeys = []
    for i in range(Nr+1):
	roundkeys.append(W[0:Nb])
	W = W[Nb:]

    # Done.
    return roundkeys

# ----------------------------------------------------------------------
# Actual cipher.

def one_round(roundkey, is_final):
    bytesub()
    #print "st: " + writebox(State)
    shiftrow()
    #print "st: " + writebox(State)
    if not is_final:
	mixcolumn(0)
	#print "st: " + writebox(State)
    addroundkey(roundkey)
    #print "st: " + writebox(State)

def rijndael(bytes, roundkeys):
    setup_state(bytes)
    #print "st: " + writebox(State)
    addroundkey(roundkeys[0])
    #print "st: " + writebox(State)
    for i in range(1, Nr):
	one_round(roundkeys[i], 0)
    one_round(roundkeys[Nr], 1)
    return extract_output()

# ----------------------------------------------------------------------
# Output the tables for an optimised C version.

def sboxtables():
    print "SBOX:"
    for i in range(256):
	print "0x" + hexbyte(Sbox(i)) + ","
    print "SBOXINV:"
    for i in range(256):
	print "0x" + hexbyte(Sboxinv(i)) + ","

def maintable(inv, n):
    line = [0, 0, 0, 0]
    for i in range(256):
	if inv:
	    line[n] = Sboxinv(i)
	else:
	    line[n] = Sbox(i)
	column = mixonecolumn(inv, line[0], line[1], line[2], line[3])
	print "0x" + hexbyte(column[0]) + hexbyte(column[1]) + \
	hexbyte(column[2]) + hexbyte(column[3]) + ","

def maintables():
    print "E0:"; maintable(0, 0)
    print "E1:"; maintable(0, 1)
    print "E2:"; maintable(0, 2)
    print "E3:"; maintable(0, 3)
    print "D0:"; maintable(1, 0)
    print "D1:"; maintable(1, 1)
    print "D2:"; maintable(1, 2)
    print "D3:"; maintable(1, 3)

# ----------------------------------------------------------------------
# Test encryption of every block size with every key size. The
# input block and the key are both all zeros in every case.

for Nb in range(4,9):
    for Nk in range(4,9):
	Nr = max(Nb,Nk) + 6
	sched = keyschedule((0,0,0,0) * Nk)
	pt = (0,0,0,0) * Nb
	ct = rijndael(pt, sched)
	print writestr(ct)

# Write out the tables required for a C implementation.
#sboxtables()
#maintables()
