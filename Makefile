test: real.out expected.out
	diff -u real.out expected.out

real.out: mptest
	./mptest > real.out

expected.out: mpexpected
	./mpexpected > expected.out

mptest: mptest.c mp.h
	gcc -Wall -W --std=c99 -pedantic -o mptest mptest.c

mpexpected: mptest.c mp.h
	gcc --std=c99 -o mpexpected mptest.c -DEXPECTED

clean:
	rm -f mptest mpexpected real.out expected.out

doc: mp.html

mp.html: mp.but
	halibut --html=mp.html mp.but
