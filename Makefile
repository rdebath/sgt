-include make.vars

LIBJS=-ljs -lm -ldl

ick-proxy: ick-proxy.c
	$(CC) $(CFLAGS) -o ick-proxy ick-proxy.c $(LFLAGS) $(LIBJS)

clean:
	rm -f ick-proxy
