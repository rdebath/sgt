# Burrows-Wheeler transform.

def bw(s):
    list=[]
    for i in range(len(s)):
	list.append((s,i==0))
	s = s[1:] + s[0]
    l = len(list[0])
    list.sort()
    s=""
    for i in range(len(list)):
	t = list[i]
	s = s + t[0][-1]
	if t[1]:
	    start = i
    return (start, s)

def invbw(pair):
    start, s = pair
    list = []
    for i in range(len(s)):
	list.append((s[i], i, start==i))
    list.sort()
    ret = ""
    pos = 0
    for i in range(len(s)):
	ret = ret + list[pos][0]
	if list[pos][2]:
	    start = len(ret)
	pos = list[pos][1]
    ret = ret[start:] + ret[:start] # rotate to correct position
    return ret

def test(s):
    b = bw(s)
    print s
    print b
    s2 = invbw(b)
    if s != s2:
	print "error: inverted to", s2

test("squiggles")
test("abracadabra")
test("the quick brown fox jumped over the lazy dog")
test("pease pudding hot, pease pudding cold, pease pudding in the pot, nine days old")
