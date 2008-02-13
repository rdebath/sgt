-include make.vars

ick-proxy: ick-proxy.o icklang.o buildpac.o
	$(CC) -o ick-proxy $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

ick-proxy.1: ick-proxy.but
	halibut --man=$@ $<

clean:
	rm -f ick-proxy ick-proxy.1 *.o

