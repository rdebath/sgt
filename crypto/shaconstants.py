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

print hex(squareroot(2L << (2*64)))
print hex(cuberoot(2L << (3*64)))
