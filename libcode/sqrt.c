static unsigned long squarert(unsigned long n) {
    unsigned long d, a, b, di;

    d = n;
    a = 0;
    b = 1 << 30;		       /* largest available power of 4 */
    do {
        a >>= 1;
        di = 2*a + b;
        if (di <= d) {
            d -= di;
            a += b;
        }
        b >>= 2;
    } while (b);

    return a;
}
