# Rationals module for Python.

import types

def gcd(a,b):
    "Return the greatest common divisor of a modulo b"
    while b != 0:
        a, b = b, a % b
    return a

def fmtint(a):
    "Format an integer without a trailing L."
    s = str(a)
    if s[-1:] == "L" or s[-1:] == "l":
        s = s[:-1]
    return s

class Rational:
    "Holds members n (a long) and d (a long, >= 1)."

    def __init__(self, x, y=1):
        assert y != 0
        g = gcd(x, y)
        if (g < 0) != (y < 0):
            g = -g
        self.n = long(x/g)
        self.d = long(y/g)

    def __coerce__(self, r2):
        if isinstance(r2, Rational):
            return self, r2
        if type(r2) == types.IntType or type(r2) == types.LongType:
            return self, Rational(r2)
        return None # failure

    def __add__(self, r2):
        return Rational(self.n * r2.d + r2.n * self.d, self.d * r2.d)

    def __radd__(self, r2):
        return Rational(self.n * r2.d + r2.n * self.d, self.d * r2.d)

    def __sub__(self, r2):
        return Rational(self.n * r2.d - r2.n * self.d, self.d * r2.d)

    def __rsub__(self, r2):
        return Rational(r2.n * self.d - self.n * r2.d, self.d * r2.d)

    def __mul__(self, r2):
        return Rational(self.n * r2.n, self.d * r2.d)

    def __rmul__(self, r2):
        return Rational(self.n * r2.n, self.d * r2.d)

    def __div__(self, r2):
        return Rational(self.n * r2.d, self.d * r2.n)

    def __rdiv__(self, r2):
        return Rational(r2.n * self.d, r2.d * self.n)

    def __neg__(self):
        return Rational(-self.n, self.d)

    def __pos__(self):
        return self

    def __abs__(self):
        return Rational(abs(self.n), self.d)

    def __int__(self):
        return int(self.n / self.d)

    def __long__(self):
        return self.n / self.d

    def __float__(self):
        l1 = len(str(self.n))
        l2 = len(str(self.d))
        l = min(l1,l2)
        if l > 300:
            tp = 10L**(l-300)
        else:
            tp = 1L            
        return float(self.n/tp) / float(self.d/tp)

    def __cmp__(self, r2):
        n = self.n * r2.d - r2.n * self.d
        if n < 0: return -1
        if n > 0: return +1
        return 0

    def __pow__(self, r2):
        assert r2.d == 1
        if r2.n == 0:
            return Rational(1)
        elif r2.n < 0:
            return Rational(self.d ** -r2.n, self.n ** -r2.n)
        else:
            return Rational(self.n ** r2.n, self.d ** r2.n)

    def __rpow__(self, r2):
        assert self.d == 1
        if self.n == 0:
            return Rational(1)
        elif self.n < 0:
            return Rational(r2.d ** -self.n, r2.n ** -self.n)
        else:
            return Rational(r2.n ** self.n, r2.d ** self.n)

    def __hash__(self):
        return hash(self.n) ^ hash(self.d)

    def __nonzero__(self):
        return self.n != 0

    def __str__(self):
        return fmtint(self.n) + "/" + fmtint(self.d)

    def __repr__(self):
        return "Rational(" + repr(self.n) + "," + repr(self.d) + ")"
