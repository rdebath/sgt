-include make.vars

LIBJS=-ljs -lm -ldl

ick-proxy: ick-proxy.c
	$(CC) $(CFLAGS) -o ick-proxy ick-proxy.c $(LFLAGS) $(LIBJS)

ick-proxy.1: ick-proxy.but
	halibut --man=$@ $<

clean:
	rm -f ick-proxy ick-proxy.1
