#
# Python module containing various useful integer-arithmetic maths
# functions. Can accept plain or long ints and will return the
# type they are passed.
#

import sys
import math

def fmt(n, func=str):
    "format number n without a trailing L; can be given hex or oct as func"
    s = func(n)
    if s[-1] == 'L':
        return s[:-1]
    else:
        return s

def invert(a,m):
    "compute multiplicative inverse of a modulo m"
    a, b = m, a
    p = 0
    i = 1

    while b != 1:
        q, a, b = a/b, b, a%b
        p, i = i, p - q * i

    if i < 0:
        # Euclid's algorithm has yielded a negative result. Make positive.
        i = ((i%m) + m) % m

    return i

def gcd(a,b):
    "Return the greatest common divisor of a modulo b"
    while b != 0:
        a, b = b, a % b
    return a

def intexp(f):
    "Render a finite float into the form integer * 2^exponent. Return (i,e)."
    m, e = math.frexp(f)
    m, e = long(2.0**52 * m), e-52
    return m, e

def modmin(a,b,c,r,t):
    "Return x in [r,t] which has maximal (a*x mod b) below c"

    # Reduce a and c mod b
    a = a % b
    c = c % b
    if a < 0: a = (a + b) % b
    if c < 0: c = (c + b) % b

    def bkt(s, a=a, b=b):
        "Paxson's bracket function: return a*s mod b"
        return a*s % b

    class container:
        "Empty holding class to contain persistent state"
    cont = container()
    cont.n = -2
    cont.qq = {}

    def firstmodbelow(c, a=a, b=b, cont=cont, bkt=bkt):
        "Find smallest n such that (a*n mod b) < c"

        # Special case. if a < c, just return 1 because it's got to be that.
        if a < c: return 1
        # And if c is 1, return the inverse of a mod b
        if c == 1: return invert(a,b)

        # Set up to calculate convergents */
        if cont.n == -2:
            cont.y = a
            cont.z = b
            cont.p = 1
            cont.q = 0
            while 1:
                if cont.n >= -1:
                    cont.rr = cont.rr * cont.q + cont.p
                    cont.p = cont.q
                    cont.q = cont.rr
                    if cont.n >= 0:
                        cont.qq[cont.n] = cont.q
                if cont.z == 0:
                    break
                cont.n = cont.n + 1
                t = cont.z
                cont.rr = cont.y / cont.z
                cont.z = cont.y % cont.z
                cont.y = t

        nn = -1
        while 1:
            nn = nn + 2
            if bkt(cont.qq[nn]) <= c:
                break

        if nn == 1:
            # Paxson missed this case: cont.qq[nn] might not be optimal in
            # this trivial case.
            cont.q = cont.qq[nn]
            u = (b-c + b-a - 1) / (b-a)
            if bkt(u) <= c and u < cont.q:
                cont.q = u
            return (cont.q)

        nn = nn - 2
        d = bkt(cont.qq[nn]) - c
        s = b - bkt(cont.qq[nn+1])
        j = (d+s-1)/s
        return cont.qq[nn] + j*cont.qq[nn+1]

    # Check a and b are coprime. Scale down if there is a common factor.
    i = gcd(a,b)
    if i > 1:
        a = a / i
        b = b / i
        c = c / i # round down: this is correct

    # Special case: an exact answer may exist.
    i = (invert(a,b) * c) % b
    i = r + (b - (r-i)%b) % b # next above r
    if i <= t:
        return i

    s = r
    while 1:
        d = c - bkt(s)
        if d < 0: d = d + b
        if d == 0: return s
        i = firstmodbelow(d)
        k = d / bkt(i)
        if (s + k*i) >= t:
            break
        s = s + k*i
    k = (t-s) / i  # Paxson says (t-s)/bkt(i). Bloody typos
    s = s + k*i
    return s

def confrac(n, d, e=1, output=sys.stdout):
    "Print continued fraction of n/d, optionally with error +/- e in n"
    terms = []
    n1, nk = 0, 1
    d1, dk = 1, 0
    nl, dl = n-e, d
    ng, dg = n+e, d
    while dl != 0 and dg != 0:
        i = nl / dl
        if i != ng / dg:
            break
        nl, dl = dl, nl % dl
        ng, dg = dg, ng % dg
        nk, n1 = nk * i + n1, nk
        dk, d1 = dk * i + d1, dk
        if output != None:
            output.write(fmt(i) + " : " + fmt(nk) + "/" + fmt(dk) + "\n")
        terms.append(i)
    return terms

def confracr(n, d, output=sys.stdout):
    "Print continued fraction of the exact rational n/d"
    return confrac(n, d, e=0, output=output)

def confracf(f, e=1, output=sys.stdout):
    "Print continued fraction of the float f, as far as it goes"
    i, ex = intexp(f)
    if ex > 0:
        i = i * (2L**ex)
        j = 1L
    else:
        j = 2L**(-ex)
    return confrac(i, j, e=e, output=output)

# pi times 10**1500
pi1500 = 3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949129833673362440656643086021394946395224737190702179860943702770539217176293176752384674818467669405132000568127145263560827785771342757789609173637178721468440901224953430146549585371050792279689258923542019956112129021960864034418159813629774771309960518707211349999998372978049951059731732816096318595024459455346908302642522308253344685035261931188171010003137838752886587533208381420617177669147303598253490428755468731159562863882353787593751957781857780532171226806613001927876611195909216420198938095257201065485863278865936153381827968230301952035301852968995773622599413891249721775283479131515574857242454150695950829533116861727855889075098381754637464939319255060400927701671139009848824012858361603563707660104710181942955596198946767837449448255379774726847104047534646208046684259069491293313677028989152104752162056966024058038150193511253382430035587640247496473263914199272604269922796782354781636009341721641219924586315030286182974555706749838505494588586926995690927210797509302955L
