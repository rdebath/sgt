all: base64.1 base64

base64: base64.c
	$(CC) $(CFLAGS) -o $@ $<

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 base64
