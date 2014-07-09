import random
from mathlib import primes

ps = primes(1024)

def isprime(p):
    pwr = p-1
    n = 0
    while pwr % 2 == 0:
        pwr = pwr/2
        n += 1
    #print "test", p, "(%d)" % n
    for k in range(100):
        a = random.randrange(2, p-2)
        if pow(a, p-1, p) != 1:
            return False # easy case
        pw = pow(a, pwr, p)
        #print a, "->", pw,
        if pw != 1:
            for k in range(n):
                if pw == p-1:
                    #print "ok",
                    break
                pw = pw*pw % p
                #print pw,
                if pw == 1:
                    #print "!"
                    return False
        #print
    return True

def genprime(lo, hi, xform):
    while True:
        proto_p = random.randrange(lo, hi)
        while True:
            p = xform(proto_p)
            if [] == [q for q in ps if p % q == 0]: break
            proto_p += 1
        if isprime(p): return p

