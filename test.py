# import decimal
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
    iter = n // 14 + 1
    P1n, Q1n, R1n = binary_split(1, iter)

    return (426880 * math.isqrt((10 ** (2*n)) * 10005) * Q1n) // (13591409*Q1n + R1n)

print(f"{chudnovsky(20)}")
print(f"{chudnovsky(40)}")
print(f"{chudnovsky(1000)}")

