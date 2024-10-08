from decimal import Decimal as d, getcontext as g
g().prec = 1000
import math
def binary_split(a, b):
    if b == a + 1:
        Pab = -(6*a - 5)*(2*a - 1)*(6*a - 1)
        Qab = 10939058860032000 * a**3
        Rab = Pab * (545140134*a + 13591409)
    else:
        m = (a + b) // 2
        Pam, Qam, Ram = binary_split(a, m)
        Pmb, Qmb, Rmb = binary_split(m, b)
        
        Pab = Pam * Pmb
        Qab = Qam * Qmb
        Rab = Qmb * Ram + Pam * Rmb
    return Pab, Qab, Rab


def chudnovsky(n):
    """Chudnovsky algorithm."""
    # precision per iteration = log10((640320**3)/(24*6*2*6))
    # or log10(53660**3) = 14.1889520050077605971726493174695353
    iter = int(n / 14.18) + 1
    # iter = n
    P1n, Q1n, R1n = binary_split(1, iter)

    return (426880 * math.isqrt((10 ** (2*n)) * 10005) * Q1n) // (13591409*Q1n + R1n)

def euler(n):
    one = 10 ** n
    denom = 2
    term = 1
    res = one * 2
    for i in range(3, n):
        term = one // denom
        if term == 0: return res
        res += term
        denom *= i

def cos(x, pi):
    exp = int(math.log10(pi))
    if x > pi:
        x -= (x // pi) * pi 
    one = 10 ** exp
    res = one
    denom = 2
    num = x * x // one
    for i in range(3, exp, 4):
        term = num // denom
        if term == 0: break
        res -= term

        num *= (x * x)
        num //= (one * one)
        denom *= i * (i + 1)

        term = num // denom
        if term == 0: break
        res += term

        num *= (x * x)
        num //= (one * one)
        denom *= (i + 2) * (i + 3)
    return res

pi = chudnovsky(1000)
one_exp = int(math.log10(pi))

def add_leading_zero(n):
    result = '-' if n < 0 else ''
    n = abs(n)
    tmp_exp = int(math.log10(n))
    if tmp_exp < one_exp: 
        result += '0.'
        tmp_exp += 1
    result += '0'*(one_exp - tmp_exp)
    result += str(n)
    return result


for i in range(0, 100):
    res = cos((pi * i) // 100, pi)
    print(add_leading_zero(res))


