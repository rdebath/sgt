# Python script to generate the SHA-256, SHA-384 and SHA-512 
# Mysterious Constants.

def squareroot(N):
    d = N
    a = 0L
    b = 1L
    while b < N:
	b = b << 2
    while 1:
	a = a >> 1
	di = 2 * a + b
	if di <= d:
	    d = d - di
	    a = a + b
	b = b >> 2
	if b == 0:
	    break
    return a

def cuberoot(N):
    x = T1 = T2 = 0L
    as = 0L
    T0 = 1L
    while T0 < N:
	T0 = T0 * 8
	as = as + 1
    R = N

    while as >= 0:
        T = T0 + T1 + T2
	if R >= T:
            R = R - T
	    x = x + (1L<<as)
	    T2 = T2 + T1
	    T1 = T1 + 3*T0
	    T2 = T2 + T1
	T0 = T0 >> 3
	T1 = T1 >> 2
	T2 = T2 >> 1
	as = as - 1

    return x

def sieve(n):
    z = []
    list = []
    for i in range(n): z.append(1)
    for i in range(2,n):
        if z[i]:
            list.append(i)
            for j in range(i,n,i): z[j] = 0
    return list

def hexstr(n):
    s = hex(n)
    if s[-1] == "L":
	s = s[:-1]
    if s[0:2] != "0x":
	s = "0x" + s
    return s

primes = sieve(512)
assert len(primes) >= 80

print "SHA-256 initialisation vector:"
for p in primes[0:8]:
    print hexstr(0xFFFFFFFFL & squareroot(p * (1L << (2*32))))

print "SHA-256 per-round constants:"
for p in primes[0:64]:
    print hexstr(0xFFFFFFFFL & cuberoot(p * (1L << (3*32))))

print "SHA-512 initialisation vector:"
for p in primes[0:8]:
    print hexstr(0xFFFFFFFFFFFFFFFFL & squareroot(p * (1L << (2*64))))

print "SHA-512 per-round constants:"
for p in primes[0:80]:
    print hexstr(0xFFFFFFFFFFFFFFFFL & cuberoot(p * (1L << (3*64))))

print "SHA-384 initialisation vector:"
for p in primes[8:16]:
    print hexstr(0xFFFFFFFFFFFFFFFFL & squareroot(p * (1L << (2*64))))
