#!/usr/bin/env python

import string

XXXXXX = -1

win1252_plus_vt100 = [
0x0020, 0x2666, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0,
0x00b1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534,
0x252c, 0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7,
0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
0x20ac, XXXXXX, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, XXXXXX, 0x017d, XXXXXX,
XXXXXX, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, XXXXXX, 0x017e, 0x0178,
0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff]

cp437 = [
0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
0x25d8, 0x25e6, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
0x25b6, 0x25c0, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
0x2191, 0x2193, 0x2192, 0x2190, 0x2310, 0x2194, 0x25b2, 0x25bc,
0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0]

class bdf:
    pass

def readbdf(f):
    ret = bdf()
    ret.height = 0
    ret.chars = {}

    while 1:
	s = f.readline()
	if s == "":
	    break;
	if s[-1:] == "\n":
	    s = s[:-1]

	if s[:10] == "COPYRIGHT ":
	    ret.copyright = s[10:]
	    if ret.copyright[:1] == "\"" and ret.copyright[-1:] == "\"":
		ret.copyright = ret.copyright[1:-1]

	sl = string.split(s)
	if len(sl) < 1:
	    break

	if sl[0] == "PIXEL_SIZE" and len(sl) == 2:
	    ret.height = string.atoi(sl[1])

	if sl[0] == "POINT_SIZE" and len(sl) == 2:
	    ret.pointsize = string.atoi(sl[1])

	if sl[0] == "FONT_ASCENT" and len(sl) == 2:
	    ret.ascent = string.atoi(sl[1])

	if sl[0] == "AVERAGE_WIDTH" and len(sl) == 2:
	    ret.width = string.atoi(sl[1]) / 10

	if sl[0] == "ENCODING" and len(sl) == 2:
	    enc = string.atoi(sl[1])
	    bits = []
	    width = 0

	if sl[0] == "DWIDTH" and len(sl) == 3:
	    width = string.atoi(sl[1])

	if sl[0] == "BBX" and len(sl) == 5:
	    # FIXME: Right now I can't be bothered to deal properly
	    # with the possibility of the bbx being smaller than
	    # the character's full width and height, because I
	    # happen to know the BDF I'm initially working with is
	    # well-behaved in this respect.
	    assert width == string.atoi(sl[1])

	if len(sl) == 1:
	    try:
		bits_int = string.atol(sl[0], 16)
	    except ValueError, e:
		continue # it wasn't a hex number
	    shift = 4 * len(sl[0])
	    bits_str = ""
	    for i in range(width):
		shift = shift - 1
		if bits_int & (1L << shift):
		    bits_str = bits_str + "1"
		else:
		    bits_str = bits_str + "0"
	    bits.append(bits_str)
	    if len(bits) >= ret.height:
		ret.chars[enc] = bits

    return ret

def writefd(bdf, encoding, f, facename, charset=0):
    f.write("facename " + facename + "\n")
    f.write("copyright " + bdf.copyright + "\n\n")
    f.write("height %d\nascent %d\npointsize %d\n\n" % \
    (bdf.height, bdf.ascent, bdf.pointsize/10))
    f.write("charset %d\n\n" % charset)

    for i in range(len(encoding)):
	j = encoding[i]
	if j < 0 or not bdf.chars.has_key(j):
	    char = ["0" * bdf.width] * bdf.height
	else:
	    char = bdf.chars[j]

	f.write("char %d\nwidth %d\n" % (i, len(char[0])))

	for k in char:
	    f.write(k + "\n")

	f.write("\n")

f = open("6x13.bdf", "r")
bdf = readbdf(f)
f.close()
# Hack the apostrophes.
bdf.chars[39] = ["000000","000000",
"001100",
"001000",
"010000",
"000000","000000","000000","000000","000000","000000","000000","000000"]
bdf.chars[96] = ["000000","000000",
"001100",
"000100",
"000010",
"000000","000000","000000","000000","000000","000000","000000","000000"]

f = open("fixed.fd", "w")
writefd(bdf, win1252_plus_vt100, f, "fixed")
f.close()

f = open("fixedc.fd", "w")
writefd(bdf, cp437, f, "Terminal", 255)
f.close()
