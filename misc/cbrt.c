static unsigned long cubert(unsigned long N) {
    unsigned long x, R, T0, T1, T2;
    int as;

    x = T1 = T2 = 0;
    as = 10;			       /* largest available power of 8 */
    T0 = 1 << (3*as);
    R = N;

    while (as >= 0) {
        unsigned long T = T0 + T1 + T2;
        if (R >= T) {
            R -= T;
            x += 1<<as;
            T2 += T1;
            T1 += 3*T0;
            T2 += T1;
        }
        T0 >>= 3;
        T1 >>= 2;
        T2 >>= 1;
        as--;
    }

    return x;
}
