# Rationals module for Python.

import types
import math

def _gcd(a,b):
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

def mkratval(x):
    if isinstance(x, Rational):
        return x.n, x.d
    elif type(x) == types.FloatType:
        x, e = math.frexp(x)
        x, e = long(2.0**52 * x), e-52
        if e >= 0:
            return x * 2L**e, 1
        else:
            return x, 2L**-e
    elif type(x) == types.IntType or type(x) == types.LongType:
        return x, 1
    else:
        raise "bad type in Rational constructor"

class Rational:
    "Holds members n (a long) and d (a long, >= 1)."

    def __init__(self, x, y=1):
        xn, xd = mkratval(x)
        yn, yd = mkratval(y)
        x = xn * yd
        y = xd * yn
        assert y != 0
        g = _gcd(x, y)
        if (g < 0) != (y < 0):
            g = -g
        self.n = long(x/g)
        self.d = long(y/g)

    def __coerce__(self, r2):
        if isinstance(r2, Rational):
            return self, r2
        if type(r2) == types.IntType or type(r2) == types.LongType or type(r2) == types.FloatType:
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
        if not isinstance(r2, Rational):
            return 1
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
        if self.d != 1:
            return fmtint(self.n) + "/" + fmtint(self.d)
        else:
            return fmtint(self.n)

    def __repr__(self):
        return "Rational(" + repr(self.n) + "," + repr(self.d) + ")"

    def decrep(self, ndigits = 40):
        "Return a string showing the number as a decimal, to arbitrary " + \
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

    def fltrep(self, ndigits = 40):
        "Return a string showing the number in scientific notation, to " + \
        "arbitrary precision. Rounds to nearest/even."
        if self.n == 0:
            return "0"
        def numdigits(n):
            s = str(abs(n))
            if s[-1:] == "L": s = s[:-1]
            return len(s)
        nlen = numdigits(self.n)
        dlen = numdigits(self.d)
        if self.n < 0:
            sign = "-"
        else:
            sign = ""
        # So self.n is in the range [10^(nlen-1), 10^nlen), and
        # self.d is in the range [10^(dlen-1), 10^dlen). This means
        # that the value of the rational is in the range
        # (10^(nlen-dlen-1), 10^(nlen-dlen+1)). At the top of that
        # range, that could just about round up to exactly
        # 10^(nlen-dlen+1); hence there are three possible powers
        # of ten we can try.
        #
        # Our eventual aim is to multiply by a power of ten to get
        # a number in the range [1,10); so we must try dividing by
        # three powers of ten ranging from 10^(nlen-dlen-1) to
        # 10^(nlen-dlen+1). We try dividing by smaller powers of
        # ten first, because some numbers may reach the required
        # length at two different exponents due to rounding (99999,
        # for example, may be represented in 5-digit precision as
        # 9.9999 * 10^4, but if we had tried 10^5 first then we
        # would have found 0.99999 * 10^5 rounded up to 1.0000 *
        # 10^5 and ended up with less accuracy).
        for e in range(nlen-dlen-1, nlen-dlen+2):
            # Compute the value of the rational times
            # 10^(ndigits-1-e).
            exp = ndigits-1-e
            if exp >= 0:
                nexp = 10L ** exp
                dexp = 1L
            else:
                dexp = 10L ** -exp
                nexp = 1L

            nn = abs(self.n) * nexp
            dd = self.d * dexp

            digits = nn / dd

            # round appropriately
            remainder = nn % dd
            if remainder > dd / 2:
                digits = digits + 1
            elif remainder == dd / 2:
                # round to even
                digits = digits + (digits & 1)

            string = fmtint(digits)
            if len(string) == ndigits:
                break

        assert len(string) == ndigits
        return sign + string[0] + "." + string[1:] + "E" + ("%+d" % e)

    def ieeerep(self, floattype='d'):
        "Return the number in an IEEE format. Rounds to nearest/even."

        if floattype == 's' or floattype == 'f':
            expadj = 0x7f
            maxexp = 0xff
            mantbits = 23
            sign = 0x80000000L
        else:
            expadj = 0x3ff
            maxexp = 0x7ff
            mantbits = 52
            sign = 0x8000000000000000L

        def numbits(n):
            # first get an upper bound
            if n == 0:
                return 0
            u = 1
            while n >= (1L << u):
                u = u * 2
            # then binary-search within that
            ret = 1
            for k in range(u-1, -1, -1):
                if n >= (1L << k):
                    n = n >> k
                    ret = ret + k
            return ret

        nlen = numbits(abs(self.n))
        dlen = numbits(self.d)
        if self.n >= 0:
            sign = 0
        # So self.n is in the range [2^(nlen-1), 2^nlen), and
        # self.d is in the range [2^(dlen-1), 2^dlen). This means
        # that the value of the rational is in the range
        # (2^(nlen-dlen-1), 2^(nlen-dlen+1)). At the top of that
        # range, that could just about round up to exactly
        # 2^(nlen-dlen+1); hence there are three possible powers of
        # two we can try.
        #
        # Our eventual aim is to multiply by a power of two to get
        # a number in the range [1,2); so we must try dividing by
        # three powers of two ranging from 2^(nlen-dlen-1) to
        # 2^(nlen-dlen+1). We try dividing by smaller powers of two
        # first, because some numbers may reach the required length
        # at two different exponents due to rounding (31, for
        # example, may be represented in 5-digit binary precision
        # as 1.1111 * 2^4, but if we had tried 2^5 first then we
        # would have found 0.11111 * 2^5 rounded up to 1.0000 * 2^5
        # and ended up with less accuracy).
        for e in range(nlen-dlen-1, nlen-dlen+2):
            # Compute the value of the rational times
            # 2^(ndigits-1-e).
            exp = mantbits-e
            if exp >= 0:
                nexp = 2L ** exp
                dexp = 1L
            else:
                dexp = 2L ** -exp
                nexp = 1L

            nn = abs(self.n) * nexp
            dd = self.d * dexp
            digits = nn / dd

            # round appropriately
            remainder = nn % dd
            if remainder > dd / 2:
                digits = digits + 1
            elif remainder == dd / 2:
                # round to even
                digits = digits + (digits & 1)

            if numbits(digits) == mantbits+1:
                break

        assert numbits(digits) == mantbits+1

        exp = e + expadj
        if exp >= maxexp:
            # Overflow.
            return sign | (long(maxexp) << mantbits)
        if exp < 1:
            # Underflow. Try again, with a fixed denominator.
            nexp = 2L ** (expadj + mantbits - 1)
            dexp = 1L
            
            nn = abs(self.n) * nexp
            dd = self.d * dexp
            digits = nn / dd

            # round appropriately
            remainder = nn % dd
            if remainder > dd / 2:
                digits = digits + 1
            elif remainder == dd / 2:
                # round to even
                digits = digits + (digits & 1)

            assert numbits(digits) <= mantbits+1
            if numbits(digits) <= mantbits:
                exp = 0
            else:
                exp = 1

        return (digits & ((1L << mantbits)-1)) | (long(exp) << mantbits) | sign

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
        # subtract 1 from it (easily done since that just comes
        # straight off the rational part) and test whether it has
        # become less than zero.
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
            r2 = s2 = None
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

        if r2 != None and r2 == s2:
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

# Return the exact value of an IEEE 754 number.
def rat754(x):
    sign = (x >> 63) & 1
    exp = (x >> 52) & 0x7FF
    mant = x & 0x000FFFFFFFFFFFFFL
    if exp == 0x7FF:
        raise ValueError, "Cannot convert infinity or NaN to a rational"
    elif exp > 0:
        mant = mant | 0x0010000000000000L
    else:
        exp = exp + 1 # denorms have the same real exponent value as exp=1
    exp = exp - 0x3FF
    exp = exp - 52
    if exp < 0:
        return Rational(mant, 2**-exp)
    else:
        return Rational(mant * 2**exp)
