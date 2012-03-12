all: test doc

test:
	gcc -Wall -W --std=c99 -pedantic -o mptest-c99 mptest.c
	g++ -Wall -W -x c++ -pedantic -o mptest-cpp mptest.c
	gcc -Wall -W --std=c89 -pedantic -o mptest-c89 mptest.c -DC89
	gcc -o mpexpected-c99 mptest.c -DEXPECTED
	gcc -o mpexpected-c89 mptest.c -DEXPECTED -DC89
	./mptest-c99 > real-c99.out
	./mptest-cpp > real-cpp.out
	./mptest-c89 > real-c89.out
	./mpexpected-c99 > expected-c99.out
	./mpexpected-c89 > expected-c89.out
	diff real-c99.out expected-c99.out
	diff real-cpp.out expected-c99.out
	diff real-c89.out expected-c89.out

doc: mp.html

mp.html: mp.but
	halibut --html=mp.html mp.but

clean:
	rm -f mptest-* mpexpected-* real*.out expected*.out mp.html
