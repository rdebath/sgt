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

    def decrep(self, ndigits = 40):
	"Return a string showing the number as a decimal, to arbitrary" + \
	"precision. Rounds to nearest/even."
	digits = self.n * (10L**ndigits) / self.d

	# round appropriately
	remainder = self.n * (10L**ndigits) % self.d
	if remainder > self.d / 2:
	    digits = digits + 1
	elif remainder == self.d / 2:
	    # round to even
	    digits = digits + (digits & 1)

	string = fmtint(digits)
	if string[:1] == "-":
	    sign = "-"
	    string = string[1:]
	else:
	    sign = ""

	if ndigits > 0:
	    if len(string) > ndigits:
		string = string[:-ndigits] + "." + string[-ndigits:]
	    else:
		string = "0." + ("0"*ndigits+string)[-ndigits:]

	return sign + string

def sqrt(r, maxerr=2L**128, maxsteps=None):
    if r < 0:
        raise OverflowError, "rational square root of negative number"
    elif r == 0:
        return r

    # Euclid's algorithm expects the first number to be larger than
    # the second. Hence, we take square roots of things less than 1
    # by turning them upside down.
    if r < 1:
        r = 1/r
        invert = 1
    else:
        invert = 0

    # We obtain continued fraction coefficients by running Euclid's
    # algorithm on the square root of r, and 1. This is a fiddly
    # process; we have to maintain the numbers involved in the
    # algorithm as A + B sqrt(r), where A and B are rationals.
    # (However, notice that the actual numbers being divided by
    # each other in the algorithm will always be integers, since
    # the only transformation they undergo is having an integer
    # multiple of one subtracted from the other.)

    a = (0L, 1L)   # 0 + 1 sqrt(r)
    b = (1L, 0L)   # 1 + 0 sqrt(r)

    steps = 0
    while 1:
        # We must do a trial division of a by b, to find out the
        # next coefficient. Multiply top and bottom by the
        # conjugate of b in the usual way, so we are dividing
        #
        #   ar + as sqrt(r)   (ar + as sqrt(r)) (br - bs sqrt(r))
        #   --------------- = -----------------------------------
        #   br + bs sqrt(r)              br^2 - r bs^2
        #
        # (ar and br are the rational parts of a and b; as and bs
        # are the square-root parts, i.e. the coefficients of
        # sqrt(r).)
        #
        # So our resulting number is
        #
        #   ar br - r as bs   br as - ar bs
        #   --------------- + ------------- sqrt(r)
        #    br^2 - r bs^2    br^2 - r bs^2

        denom = b[0]*b[0] - r*b[1]*b[1]
        rpart = (a[0]*b[0] - r*a[1]*b[1]) / denom
        spart = (b[0]*a[1] - a[0]*b[1]) / denom

        # Having done the division, we now need to find the largest
        # integer below the result.
        #
        # We do this in the simplest possible manner: we have a
        # number of the form A + B sqrt(r), and we repeatedly
        # subtract 1 to it (easily done since it just goes straight
        # on to the rational part) and test whether it has become
        # less than zero.
        q = 0
        while 1:
            rpart = rpart - 1
            # Now (rpart + spart sqrt(r)) > 0
            # iff  rpart > -spart sqrt(r)
            #
            # which will be true iff either
            #  (a) rpart and spart are both positive
            #  (b) rpart > 0, spart < 0 and rpart^2 > r spart^2
            #  (c) rpart < 0, spart > 0 and rpart^2 < r spart^2
            if rpart > 0:
                if spart < 0:
                    r2 = rpart*rpart
                    s2 = r*spart*spart
                    if r2 <= s2:
                        break
            else: # rpart < 0
                if spart > 0:
                    r2 = rpart*rpart
                    s2 = r*spart*spart
                    if r2 >= s2:
                        break
                else:
                    break # rpart, spart both negative
            q = q + 1

        if r2 == s2:
            q = q + 1 # division was exact
            exact = 1
        else:
            exact = 0

        # Now we have q; so subtract q*b from a, and swap a and b
        # over.
        
        a = (a[0] - q*b[0], a[1] - q*b[1])
        b, a = a, b

        # Conveniently enough, we don't need to piddle about with
        # actually computing the convergents; they are constructed
        # in exactly the same form as we have been constructing a
        # and b! In fact abs(b[0])/abs(b[1]) will _be_ our current
        # convergent. So compute that, and see how far off it is
        # from the real answer.
        result = Rational(abs(b[0]), abs(b[1]))

        if exact:
            break

        steps = steps + 1
        if maxsteps != None and steps >= maxsteps:
            break

        # Error estimation. Difference of two squares tells us that
        # (A - sqrt(B))(A + sqrt(B)) = A^2 - B. We want A-sqrt(B),
        # which equals (A^2-B)/(A+sqrt(B)). But sqrt(B) ~ A, so
        # we'll compute (A^2-B) / 2A and call that good enough.
        error = (result * result - r) / (2*result)
        if maxerr != None and abs(error) < Rational(1,maxerr):
            break

    if invert:
        return 1/result
    else:
        return result
