int main(int ac, char **av) {
    int n = atoi(av[1]);
    int d, a, b, di;

    d = n;
    a = 0;
    b = 1 << 30;
    do {
        a >>= 1;
printf("Iteration: d=0x%08x a=0x%08x b=0x%08x", d, a, b);
        di = 2*a + b;
printf(" (di=0x%08x)\n", di);
        if (di <= d) {
            d -= di;
            a += b;
        }
        b >>= 2;
    } while (b);

    printf("Result = %d\n", a);

    return 0;
}
