# Python implementation of DES, gradually optimised away from the
# reference implementation.

# Description of DES
#
# Unlike the description in FIPS 46, I'm going to use _sensible_ indices:
# bits in an n-bit word are numbered from 0 at the LSB to n-1 at the MSB.
#
# The DES encryption routine requires a 64-bit input, and a key schedule K
# containing 16 48-bit elements.
#
#   First the input is permuted by the initial permutation IP.
#   Then the input is split into 32-bit words L and R. (L is the MSW.)
#   Next, 16 rounds. In each round:
#     (L, R) <- (R, L xor f(R, K[i]))
#   Then the pre-output words L and R are swapped.
#   Then L and R are glued back together into a 64-bit word. (L is the MSW,
#     again, but since we just swapped them, the MSW is the R that came out
#     of the last round.)
#   The 64-bit output block is permuted by the inverse of IP and returned.
#
# Decryption is identical except that the elements of K are used in the
# opposite order. (This wouldn't work if that word swap didn't happen.)
#
# The function f, used in each round, accepts a 32-bit word R and a
# 48-bit key block K. It produces a 32-bit output.
#
#   First R is expanded to 48 bits using the bit-selection function E.
#   The resulting 48-bit block is XORed with the key block K to produce
#     a 48-bit block X.
#   This block X is split into eight groups of 6 bits. Each group of 6
#     bits is then looked up in one of the eight S-boxes to convert
#     it to 4 bits. These eight groups of 4 bits are glued back
#     together to produce a 32-bit preoutput block.
#   The preoutput block is permuted using the permutation P and returned.
#
# Key setup maps a 64-bit key word into a 16x48-bit key schedule. Although
# the approved input format for the key is a 64-bit word, eight of the
# bits are discarded, so the actual quantity of key used is 56 bits.
#
#   First the input key is converted to two 28-bit words C and D using
#     the bit-selection function PC1.
#   Then 16 rounds of key setup occur. In each round, C and D are
#     rotated left by either 1 or 2 bits (depending on which round), and
#     then converted into a key schedule element using the bit-selection
#     function PC2.
#
# That's the actual algorithm. Now for the tedious details: all those
# painful permutations and lookup tables.
#
# IP is a 64-to-64 bit permutation. Its output contains the following
# bits of its input (listed in order MSB to LSB of output).
#
#    6 14 22 30 38 46 54 62  4 12 20 28 36 44 52 60
#    2 10 18 26 34 42 50 58  0  8 16 24 32 40 48 56
#    7 15 23 31 39 47 55 63  5 13 21 29 37 45 53 61
#    3 11 19 27 35 43 51 59  1  9 17 25 33 41 49 57
#
# E is a 32-to-48 bit selection function. Its output contains the following
# bits of its input (listed in order MSB to LSB of output).
#
#    0 31 30 29 28 27 28 27 26 25 24 23 24 23 22 21 20 19 20 19 18 17 16 15
#   16 15 14 13 12 11 12 11 10  9  8  7  8  7  6  5  4  3  4  3  2  1  0 31
#
# The S-boxes are arbitrary table-lookups each mapping a 6-bit input to a
# 4-bit output. In other words, each S-box is an array[64] of 4-bit numbers.
# The S-boxes are listed below. The first S-box listed is applied to the
# most significant six bits of the block X; the last one is applied to the
# least significant.
#
#   14  0  4 15 13  7  1  4  2 14 15  2 11 13  8  1
#    3 10 10  6  6 12 12 11  5  9  9  5  0  3  7  8
#    4 15  1 12 14  8  8  2 13  4  6  9  2  1 11  7
#   15  5 12 11  9  3  7 14  3 10 10  0  5  6  0 13
#
#   15  3  1 13  8  4 14  7  6 15 11  2  3  8  4 14
#    9 12  7  0  2  1 13 10 12  6  0  9  5 11 10  5
#    0 13 14  8  7 10 11  1 10  3  4 15 13  4  1  2
#    5 11  8  6 12  7  6 12  9  0  3  5  2 14 15  9
#
#   10 13  0  7  9  0 14  9  6  3  3  4 15  6  5 10
#    1  2 13  8 12  5  7 14 11 12  4 11  2 15  8  1
#   13  1  6 10  4 13  9  0  8  6 15  9  3  8  0  7
#   11  4  1 15  2 14 12  3  5 11 10  5 14  2  7 12
#
#    7 13 13  8 14 11  3  5  0  6  6 15  9  0 10  3
#    1  4  2  7  8  2  5 12 11  1 12 10  4 14 15  9
#   10  3  6 15  9  0  0  6 12 10 11  1  7 13 13  8
#   15  9  1  4  3  5 14 11  5 12  2  7  8  2  4 14
#
#    2 14 12 11  4  2  1 12  7  4 10  7 11 13  6  1
#    8  5  5  0  3 15 15 10 13  3  0  9 14  8  9  6
#    4 11  2  8  1 12 11  7 10  1 13 14  7  2  8 13
#   15  6  9 15 12  0  5  9  6 10  3  4  0  5 14  3
#
#   12 10  1 15 10  4 15  2  9  7  2 12  6  9  8  5
#    0  6 13  1  3 13  4 14 14  0  7 11  5  3 11  8
#    9  4 14  3 15  2  5 12  2  9  8  5 12 15  3 10
#    7 11  0 14  4  1 10  7  1  6 13  0 11  8  6 13
#
#    4 13 11  0  2 11 14  7 15  4  0  9  8  1 13 10
#    3 14 12  3  9  5  7 12  5  2 10 15  6  8  1  6
#    1  6  4 11 11 13 13  8 12  1  3  4  7 10 14  7
#   10  9 15  5  6  0  8 15  0 14  5  2  9  3  2 12
#
#   13  1  2 15  8 13  4  8  6 10 15  3 11  7  1  4
#   10 12  9  5  3  6 14 11  5  0  0 14 12  9  7  2
#    7  2 11  1  4 14  1  7  9  4 12 10 14  8  2 13
#    0 15  6 12 10  9 13  0 15  3  3  5  5  6  8 11
#
# P is a 32-to-32 bit permutation. Its output contains the following
# bits of its input (listed in order MSB to LSB of output).
#
#   16 25 12 11  3 20  4 15 31 17  9  6 27 14  1 22
#   30 24  8 18  0  5 29 23 13 19  2 26 10 21 28  7
#
# PC1 is a 64-to-56 bit selection function. Its output is in two words,
# C and D. The word C contains the following bits of its input (listed
# in order MSB to LSB of output).
#
#    7 15 23 31 39 47 55 63  6 14 22 30 38 46
#   54 62  5 13 21 29 37 45 53 61  4 12 20 28
#
# And the word D contains these bits.
#
#    1  9 17 25 33 41 49 57  2 10 18 26 34 42
#   50 58  3 11 19 27 35 43 51 59 36 44 52 60
#
# PC2 is a 56-to-48 bit selection function. Its input is in two words,
# C and D. These are treated as one 56-bit word (with C more significant,
# so that bits 55 to 28 of the word are bits 27 to 0 of C, and bits 27 to
# 0 of the word are bits 27 to 0 of D). The output contains the following
# bits of this 56-bit input word (listed in order MSB to LSB of output).
#
#   42 39 45 32 55 51 53 28 41 50 35 46 33 37 44 52 30 48 40 49 29 36 43 54
#   15  4 25 19  9  1 26 16  5 11 23  8 12  7 17  0 22  3 10 14  6 20 27 24

def bitpos(bitnum):
    return 1L << bitnum

def bit(bitnum, value):
    if bitnum < 0:
        return 0
    bit = value & bitpos(bitnum)
    if bit != 0:
        bit = 1
    return bit

def chop2x32(data):
    return ((data >> 32) & 0xFFFFFFFFL), (data & 0xFFFFFFFFL)

def glue2x32(L,R):
    return ((L & 0xFFFFFFFFL) << 32) | (R & 0xFFFFFFFFL)

class keysched:
    "Empty holding class"

def leftrot(data, nshifts, size):
    return ( (data << nshifts) | (data >> (size-nshifts)) ) & ((1L<<size)-1)

def key_setup(key):

    def PC1(key): # 64 -> 56 bits; bits 56,48,40,32,24,16,8,0 omitted
        PC1_C = (7, 15, 23, 31, 39, 47, 55, 63, 6, 14, 22, 30, 38, 46,
        54, 62, 5, 13, 21, 29, 37, 45, 53, 61, 4, 12, 20, 28)
        PC1_D = (1, 9, 17, 25, 33, 41, 49, 57, 2, 10, 18, 26, 34, 42,
        50, 58, 3, 11, 19, 27, 35, 43, 51, 59, 36, 44, 52, 60)
        C = 0
        D = 0
        for i in range(28):
            C = (C << 1) | bit(PC1_C[i], key)
            D = (D << 1) | bit(PC1_D[i], key)
        return C, D

    def PC2evens(C, D): # 56 -> 32 bits
        PC2even = (45, 32, 55, 51, -1, -1,
        33, 37, 44, 52, 30, 48, -1, -1,
        15, 4, 25, 19, 9, 1, -1, -1,
        12, 7, 17, 0, 22, 3, -1, -1,
        42, 39)
        ret = 0
        key = (C << 28) | D
        for i in PC2even:
            ret = (ret << 1) | bit(i, key)
        return ret

    def PC2odds(C, D): # 56 -> 32 bits
        PC2odd = (-1, -1,
        53, 28, 41, 50, 35, 46, -1, -1,
        40, 49, 29, 36, 43, 54, -1, -1,
        26, 16, 5, 11, 23, 8, -1, -1,
        10, 14, 6, 20, 27, 24)
        ret = 0
        key = (C << 28) | D
        for i in PC2odd:
            ret = (ret << 1) | bit(i, key)
        return ret

    leftshifts = (1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1)
    C, D = PC1(key)
    ret = keysched()
    ret.k = range(16)
    ret.s0246 = range(16)
    ret.s1357 = range(16)
    for i in range(16):
        C = leftrot(C, leftshifts[i], 28)
        D = leftrot(D, leftshifts[i], 28)
        ret.s0246[i] = PC2evens(C, D)
        ret.s1357[i] = PC2odds(C, D)
    return ret

# The function f used in each round of the algorithm.
# R is 32 bits; K is 48.
def f(R, K0246, K1357):

    SPboxes = (
    (0x01010400L, 0x00000000L, 0x00010000L, 0x01010404L,
    0x01010004L, 0x00010404L, 0x00000004L, 0x00010000L,
    0x00000400L, 0x01010400L, 0x01010404L, 0x00000400L,
    0x01000404L, 0x01010004L, 0x01000000L, 0x00000004L,
    0x00000404L, 0x01000400L, 0x01000400L, 0x00010400L,
    0x00010400L, 0x01010000L, 0x01010000L, 0x01000404L,
    0x00010004L, 0x01000004L, 0x01000004L, 0x00010004L,
    0x00000000L, 0x00000404L, 0x00010404L, 0x01000000L,
    0x00010000L, 0x01010404L, 0x00000004L, 0x01010000L,
    0x01010400L, 0x01000000L, 0x01000000L, 0x00000400L,
    0x01010004L, 0x00010000L, 0x00010400L, 0x01000004L,
    0x00000400L, 0x00000004L, 0x01000404L, 0x00010404L,
    0x01010404L, 0x00010004L, 0x01010000L, 0x01000404L,
    0x01000004L, 0x00000404L, 0x00010404L, 0x01010400L,
    0x00000404L, 0x01000400L, 0x01000400L, 0x00000000L,
    0x00010004L, 0x00010400L, 0x00000000L, 0x01010004L),

    (0x80108020L, 0x80008000L, 0x00008000L, 0x00108020L,
    0x00100000L, 0x00000020L, 0x80100020L, 0x80008020L,
    0x80000020L, 0x80108020L, 0x80108000L, 0x80000000L,
    0x80008000L, 0x00100000L, 0x00000020L, 0x80100020L,
    0x00108000L, 0x00100020L, 0x80008020L, 0x00000000L,
    0x80000000L, 0x00008000L, 0x00108020L, 0x80100000L,
    0x00100020L, 0x80000020L, 0x00000000L, 0x00108000L,
    0x00008020L, 0x80108000L, 0x80100000L, 0x00008020L,
    0x00000000L, 0x00108020L, 0x80100020L, 0x00100000L,
    0x80008020L, 0x80100000L, 0x80108000L, 0x00008000L,
    0x80100000L, 0x80008000L, 0x00000020L, 0x80108020L,
    0x00108020L, 0x00000020L, 0x00008000L, 0x80000000L,
    0x00008020L, 0x80108000L, 0x00100000L, 0x80000020L,
    0x00100020L, 0x80008020L, 0x80000020L, 0x00100020L,
    0x00108000L, 0x00000000L, 0x80008000L, 0x00008020L,
    0x80000000L, 0x80100020L, 0x80108020L, 0x00108000L),

    (0x00000208L, 0x08020200L, 0x00000000L, 0x08020008L,
    0x08000200L, 0x00000000L, 0x00020208L, 0x08000200L,
    0x00020008L, 0x08000008L, 0x08000008L, 0x00020000L,
    0x08020208L, 0x00020008L, 0x08020000L, 0x00000208L,
    0x08000000L, 0x00000008L, 0x08020200L, 0x00000200L,
    0x00020200L, 0x08020000L, 0x08020008L, 0x00020208L,
    0x08000208L, 0x00020200L, 0x00020000L, 0x08000208L,
    0x00000008L, 0x08020208L, 0x00000200L, 0x08000000L,
    0x08020200L, 0x08000000L, 0x00020008L, 0x00000208L,
    0x00020000L, 0x08020200L, 0x08000200L, 0x00000000L,
    0x00000200L, 0x00020008L, 0x08020208L, 0x08000200L,
    0x08000008L, 0x00000200L, 0x00000000L, 0x08020008L,
    0x08000208L, 0x00020000L, 0x08000000L, 0x08020208L,
    0x00000008L, 0x00020208L, 0x00020200L, 0x08000008L,
    0x08020000L, 0x08000208L, 0x00000208L, 0x08020000L,
    0x00020208L, 0x00000008L, 0x08020008L, 0x00020200L),

    (0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L,
    0x00802080L, 0x00800081L, 0x00800001L, 0x00002001L,
    0x00000000L, 0x00802000L, 0x00802000L, 0x00802081L,
    0x00000081L, 0x00000000L, 0x00800080L, 0x00800001L,
    0x00000001L, 0x00002000L, 0x00800000L, 0x00802001L,
    0x00000080L, 0x00800000L, 0x00002001L, 0x00002080L,
    0x00800081L, 0x00000001L, 0x00002080L, 0x00800080L,
    0x00002000L, 0x00802080L, 0x00802081L, 0x00000081L,
    0x00800080L, 0x00800001L, 0x00802000L, 0x00802081L,
    0x00000081L, 0x00000000L, 0x00000000L, 0x00802000L,
    0x00002080L, 0x00800080L, 0x00800081L, 0x00000001L,
    0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L,
    0x00802081L, 0x00000081L, 0x00000001L, 0x00002000L,
    0x00800001L, 0x00002001L, 0x00802080L, 0x00800081L,
    0x00002001L, 0x00002080L, 0x00800000L, 0x00802001L,
    0x00000080L, 0x00800000L, 0x00002000L, 0x00802080L),

    (0x00000100L, 0x02080100L, 0x02080000L, 0x42000100L,
    0x00080000L, 0x00000100L, 0x40000000L, 0x02080000L,
    0x40080100L, 0x00080000L, 0x02000100L, 0x40080100L,
    0x42000100L, 0x42080000L, 0x00080100L, 0x40000000L,
    0x02000000L, 0x40080000L, 0x40080000L, 0x00000000L,
    0x40000100L, 0x42080100L, 0x42080100L, 0x02000100L,
    0x42080000L, 0x40000100L, 0x00000000L, 0x42000000L,
    0x02080100L, 0x02000000L, 0x42000000L, 0x00080100L,
    0x00080000L, 0x42000100L, 0x00000100L, 0x02000000L,
    0x40000000L, 0x02080000L, 0x42000100L, 0x40080100L,
    0x02000100L, 0x40000000L, 0x42080000L, 0x02080100L,
    0x40080100L, 0x00000100L, 0x02000000L, 0x42080000L,
    0x42080100L, 0x00080100L, 0x42000000L, 0x42080100L,
    0x02080000L, 0x00000000L, 0x40080000L, 0x42000000L,
    0x00080100L, 0x02000100L, 0x40000100L, 0x00080000L,
    0x00000000L, 0x40080000L, 0x02080100L, 0x40000100L),

    (0x20000010L, 0x20400000L, 0x00004000L, 0x20404010L,
    0x20400000L, 0x00000010L, 0x20404010L, 0x00400000L,
    0x20004000L, 0x00404010L, 0x00400000L, 0x20000010L,
    0x00400010L, 0x20004000L, 0x20000000L, 0x00004010L,
    0x00000000L, 0x00400010L, 0x20004010L, 0x00004000L,
    0x00404000L, 0x20004010L, 0x00000010L, 0x20400010L,
    0x20400010L, 0x00000000L, 0x00404010L, 0x20404000L,
    0x00004010L, 0x00404000L, 0x20404000L, 0x20000000L,
    0x20004000L, 0x00000010L, 0x20400010L, 0x00404000L,
    0x20404010L, 0x00400000L, 0x00004010L, 0x20000010L,
    0x00400000L, 0x20004000L, 0x20000000L, 0x00004010L,
    0x20000010L, 0x20404010L, 0x00404000L, 0x20400000L,
    0x00404010L, 0x20404000L, 0x00000000L, 0x20400010L,
    0x00000010L, 0x00004000L, 0x20400000L, 0x00404010L,
    0x00004000L, 0x00400010L, 0x20004010L, 0x00000000L,
    0x20404000L, 0x20000000L, 0x00400010L, 0x20004010L),

    (0x00200000L, 0x04200002L, 0x04000802L, 0x00000000L,
    0x00000800L, 0x04000802L, 0x00200802L, 0x04200800L,
    0x04200802L, 0x00200000L, 0x00000000L, 0x04000002L,
    0x00000002L, 0x04000000L, 0x04200002L, 0x00000802L,
    0x04000800L, 0x00200802L, 0x00200002L, 0x04000800L,
    0x04000002L, 0x04200000L, 0x04200800L, 0x00200002L,
    0x04200000L, 0x00000800L, 0x00000802L, 0x04200802L,
    0x00200800L, 0x00000002L, 0x04000000L, 0x00200800L,
    0x04000000L, 0x00200800L, 0x00200000L, 0x04000802L,
    0x04000802L, 0x04200002L, 0x04200002L, 0x00000002L,
    0x00200002L, 0x04000000L, 0x04000800L, 0x00200000L,
    0x04200800L, 0x00000802L, 0x00200802L, 0x04200800L,
    0x00000802L, 0x04000002L, 0x04200802L, 0x04200000L,
    0x00200800L, 0x00000000L, 0x00000002L, 0x04200802L,
    0x00000000L, 0x00200802L, 0x04200000L, 0x00000800L,
    0x04000002L, 0x04000800L, 0x00000800L, 0x00200002L),

    (0x10001040L, 0x00001000L, 0x00040000L, 0x10041040L,
    0x10000000L, 0x10001040L, 0x00000040L, 0x10000000L,
    0x00040040L, 0x10040000L, 0x10041040L, 0x00041000L,
    0x10041000L, 0x00041040L, 0x00001000L, 0x00000040L,
    0x10040000L, 0x10000040L, 0x10001000L, 0x00001040L,
    0x00041000L, 0x00040040L, 0x10040040L, 0x10041000L,
    0x00001040L, 0x00000000L, 0x00000000L, 0x10040040L,
    0x10000040L, 0x10001000L, 0x00041040L, 0x00040000L,
    0x00041040L, 0x00040000L, 0x10041000L, 0x00001000L,
    0x00000040L, 0x10040040L, 0x00001000L, 0x00041040L,
    0x10001000L, 0x00000040L, 0x10000040L, 0x10040000L,
    0x10040040L, 0x10000000L, 0x00040000L, 0x10001040L,
    0x00000000L, 0x10041040L, 0x00040040L, 0x10000040L,
    0x10040000L, 0x10001000L, 0x10001040L, 0x00000000L,
    0x10041040L, 0x00041000L, 0x00041000L, 0x00001040L,
    0x00001040L, 0x00040040L, 0x10000000L, 0x10041000L))

    s0246 = leftrot(R ^ K0246, 28, 32)
    s1357 = R ^ K1357
    y = 0
    y = y | SPboxes[0] [int((s0246 >> 24) & 0x3F)]
    y = y | SPboxes[1] [int((s1357 >> 24) & 0x3F)]
    y = y | SPboxes[2] [int((s0246 >> 16) & 0x3F)]
    y = y | SPboxes[3] [int((s1357 >> 16) & 0x3F)]
    y = y | SPboxes[4] [int((s0246 >>  8) & 0x3F)]
    y = y | SPboxes[5] [int((s1357 >>  8) & 0x3F)]
    y = y | SPboxes[6] [int((s0246 >>  0) & 0x3F)]
    y = y | SPboxes[7] [int((s1357 >>  0) & 0x3F)]
    return y

def des_cipher(data, keysched, direction):

    def initperm(data):
        def outerbridge(M, L, n, mask):
            swap = mask & ( (L >> n) ^ M )
            L = L ^ (swap << n)
            M = M ^ swap
            return M, L
        M, L = chop2x32(data)
        M, L = L, M
        M, L = outerbridge(M, L, n=4, mask=0x0F0F0F0FL)
        M, L = outerbridge(M, L, n=16, mask=0x0000FFFFL)
        M, L = L, M
        M, L = outerbridge(M, L, n=2, mask=0x33333333L)
        M, L = outerbridge(M, L, n=8, mask=0x00FF00FFL)
        M, L = L, M
        M, L = outerbridge(M, L, n=1, mask=0x55555555L)
        M, L = L, M
        data = glue2x32(M, L)
        return data

    def finalperm(data):
        def outerbridge(M, L, n, mask):
            swap = mask & ( (L >> n) ^ M )
            L = L ^ (swap << n)
            M = M ^ swap
            return M, L
        M, L = chop2x32(data)
        M, L = L, M
        M, L = outerbridge(M, L, n=1, mask=0x55555555L)
        M, L = L, M
        M, L = outerbridge(M, L, n=8, mask=0x00FF00FFL)
        M, L = outerbridge(M, L, n=2, mask=0x33333333L)
        M, L = L, M
        M, L = outerbridge(M, L, n=16, mask=0x0000FFFFL)
        M, L = outerbridge(M, L, n=4, mask=0x0F0F0F0FL)
        M, L = L, M
        data = glue2x32(M, L)
        return data

    data = initperm(data)
    L, R = chop2x32(data)
    if direction == +1: # encipher
        sched = range(16)
    else:
        sched = range(15,-1,-1)
    L = leftrot(L, 1, 32)
    R = leftrot(R, 1, 32)
    for i in sched:
        L,R = R, L ^ f(R, keysched.s0246[i], keysched.s1357[i])
    L = leftrot(L, 31, 32)
    R = leftrot(R, 31, 32)
    data = glue2x32(R, L)
    data = finalperm(data)
    return data

def des_test(key, plain, cipher):
    errstring = "key="+hex(key)
    errstring = errstring + " plain="+hex(plain)+" cipher="+hex(cipher)
    keysched = key_setup(key)
    mycipher = des_cipher(plain, keysched, +1)
    myplain = des_cipher(cipher, keysched, -1)
    if mycipher != cipher:
        print "func=encipher " + errstring + " wrongcipher="+hex(mycipher)
    if myplain != plain:
        print "func=decipher " + errstring + " wrongplain="+hex(myplain)

des_test(0x1234567887654321L, 0xabcddcbafedccdefL, 0x784eb1b76fd1a0fcL)
des_test(0x7a6b7d6c9e0d1e4cL, 0x53debb4b5fb921deL, 0L)
des_test(0x7a6b7d6c9e0d1e4cL, 0xb3c6a9f5d6e1f2c8L, 0xa68687f8719f7478L)
