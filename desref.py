# Python reference implementation of DES.
# Includes the algorithm modifications used in Unix crypt(3).

def bitpos(bitnum, size):
    return 1L << (size - bitnum)

def bit(bitnum, value, size):
    bit = value & bitpos(bitnum, size)
    if bit != 0:
        bit = 1
    return bit

def initperm(data, direction):
    initperm_tuple = \
    (58, 50, 42, 34, 26, 18, 10, 2, \
    60, 52, 44, 36, 28, 20, 12, 4, \
    62, 54, 46, 38, 30, 22, 14, 6, \
    64, 56, 48, 40, 32, 24, 16, 8, \
    57, 49, 41, 33, 25, 17, 9, 1, \
    59, 51, 43, 35, 27, 19, 11, 3, \
    61, 53, 45, 37, 29, 21, 13, 5, \
    63, 55, 47, 39, 31, 23, 15, 7)
    result = 0
    if direction == +1:
        for i in range(64):
            result = (result << 1) | bit(initperm_tuple[i], data, 64)
    else:
        for i in range(64):
            if bit(i+1, data, 64) != 0:
                result = result | bitpos(initperm_tuple[i], 64)
    return result

def chop2x32(data):
    return ((data >> 32) & 0xFFFFFFFFL), (data & 0xFFFFFFFFL)

def glue2x32(L,R):
    return ((L & 0xFFFFFFFFL) << 32) | (R & 0xFFFFFFFFL)

class keysched:
    "Empty holding class"

def key_setup(key):

    def PC1(key): # 64 -> 56 bits; bits 8,16,24,32,40,48,56,64 omitted
        PC1_C = (57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
        10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36)
        PC1_D = (63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
        14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4)
        C = 0
        D = 0
        for i in range(28):
            C = (C << 1) | bit(PC1_C[i], key, 64)
            D = (D << 1) | bit(PC1_D[i], key, 64)
        return C, D

    def PC2(C, D): # 56 -> 48 bits
        PC2tuple = (14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10,
        23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
        41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
        44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32)
        ret = 0
        key = (C << 28) | D
        for i in PC2tuple:
            ret = (ret << 1) | bit(i, key, 56)
        return ret

    def leftrot(data, nshifts):
        return ( (data << nshifts) | (data >> (28-nshifts)) ) & 0x0FFFFFFF

    leftshifts = (1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1)
    C, D = PC1(key)
    ret = keysched()
    ret.k = range(16)
    for i in range(16):
        C = leftrot(C, leftshifts[i])
        D = leftrot(D, leftshifts[i])
        ret.k[i] = PC2(C, D)

    ret.saltbits = 0 # disables the crypt(3) modification

    return ret

# The function f used in each round of the algorithm.
# R is 32 bits; K is 48.
def f(R, K, saltbits):

    # f needs subfunctions:
    #   E(R) [32 bits -> 48]
    #   S(i, x) [6 bits -> 4; i is in [0,7]]
    #   P(y) [32 bits -> 32]

    def E(R):
        Etuple = (32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11, \
        12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21, \
        22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1)
        ret = 0
        for i in Etuple:
            ret = (ret << 1) | bit(i, R, 32)
        return ret

    def S(i, x):
        Stuple = (

        (14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
        0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
        4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
        15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13),

        (15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
        3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
        0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
        13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9),

        (10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
        13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
        13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
        1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12),

        (7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
        13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
        10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
        3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14),

        (2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
        14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
        4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
        11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3),

        (12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
        10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
        9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
        4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13),

        (4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
        13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
        1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
        6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12),

        (13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
        1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
        7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
        2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11))

        firstlast = ((x & 0x20) >> 4) | (x & 1)
        middle4 = (x & 0x1E) >> 1
        index = firstlast*16+middle4
        Sbox = Stuple[i]
        return Sbox[int(index)] # tuple index must be int not long

    def P(y):
        Ptuple = \
        (16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
        2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25)
        ret = 0
        for i in Ptuple:
            ret = (ret << 1) | bit(i, y, 32)
        return ret

    x = E(R)
    if saltbits != 0:
        # crypt(3) hack, not present in DES proper.
        # Exchange, between the two 24-bit halves of the output of E,
        # all bits specified by the 24-bit mask "saltbits".
        xv = (x ^ (x >> 24)) & saltbits
        x = x ^ (xv | (xv << 24))
    x = x ^ K
    y = 0
    for i in range(8):
        y = (y << 4) | S(i, (x >> (6*(7-i))) & 0x3F)
    return P(y)

def des_cipher(data, keysched, direction):
    data = initperm(data, +1)
    L,R = chop2x32(data)
    if direction == +1: # encipher
        sched = range(16)
    else:
        sched = range(15,-1,-1)
    for i in sched:
        L,R = R, L ^ f(R, keysched.k[i], keysched.saltbits)
    data = glue2x32(R, L)
    data = initperm(data, -1)
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

# ----------------------------------------------------------------------
# Unix crypt(3) wrapper

def crypt_asctobin(c):
    i = ord(c)
    if i >= ord('a'):
        n = i - ord('a') + 38   # 'a'..'z' denote 38..63
    elif i >= ord('A'):
        n = i - ord('A') + 12   # 'A'..'Z' denote 12..37
    else:
        n = i - ord('.') +  0   # '.','/','0'..'9' denote 0..11
    return n & 0x3F

def crypt_bintoasc(n):
    n = n & 0x3F
    if n >= 38:
        return chr(n - 38 + ord('a'))
    elif n >= 12:
        return chr(n - 12 + ord('A'))
    else:
        return chr(n -  0 + ord('.'))

def crypt(passwd, salt):
    key = 0L
    # The salt bits come up in a weird order.
    saltorig = crypt_asctobin(salt[1])
    saltorig = (saltorig << 6) | crypt_asctobin(salt[0])
    saltbits = 0
    for i in range(12):
        saltbits = saltbits << 1
        if saltorig & (1 << i): saltbits = saltbits | 1
    passwd = passwd + "\0\0\0\0\0\0\0\0"
    # Ordinary DES key usage ignores the low bit of each byte of the key.
    # When encrypting a password, the high bit is far less likely to
    # contain entropy than the low bit. So we shift the whole key left by
    # 1, so that the seven useful bits of each password char go into the
    # seven bits actually used in each key byte.
    for i in range(8):
        key = (key << 8) | ((ord(passwd[i]) & 0x7F) << 1)
    keysched = key_setup(key)
    keysched.saltbits = saltbits << 12
    result = 0
    for i in range(25):
        result = des_cipher(result, keysched, +1)
    # Output conversion
    result = result << 2
    output = ""
    for i in range(11):
        output = crypt_bintoasc(result & 0x3F) + output
        result = result >> 6
    output = crypt_bintoasc((saltorig >> 6) & 0x3F) + output
    output = crypt_bintoasc(saltorig & 0x3F) + output
    return output

def crypttest(passwd, salt, result):
    myresult = crypt(passwd, salt)
    if myresult != result:
        print "func=crypt passwd=" + passwd + " salt=" + salt + \
        " result=" + result + " wrongresult=" + myresult

# ----------------------------------------------------------------------

des_test(0x1234567887654321L, 0xabcddcbafedccdefL, 0x784eb1b76fd1a0fcL)
des_test(0x7a6b7d6c9e0d1e4cL, 0x53debb4b5fb921deL, 0L)
des_test(0x7a6b7d6c9e0d1e4cL, 0xb3c6a9f5d6e1f2c8L, 0xa68687f8719f7478L)

crypttest("_JRa{IkM","T5","T5rcEcD3VBZzw")
crypttest("DWPV}aph","ll","llUhimnUIKl2s")
crypttest("sHgSUy`X","am","am7YV6OuLhvLE")
crypttest("zvvjodWU","Ao","AoCO1MDUngqCQ")
crypttest("Y\}GL|nY","lk","lk/aHEsYalukw")
crypttest("|@SoNdA|","I1","I1.obEi2dv2lM")
crypttest("[ycFcO^N","SN","SNNTOizmtuyeA")
crypttest("vZUWAI`k","4N","4N81tqdiGfjxw")
crypttest("pNTMRRwA","dN","dNijIMI8qTjVg")
crypttest("jm{Mt|pZ","fn","fn4gDknJdjyuQ")
