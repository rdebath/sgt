all: cvt-utf8.1

%.1: %.but
	$(HOME)/src/halibut/build/halibut $< && mv output.1 $@
	-rm -f *.html output.*

clean:
	rm -f *.html output.* *.1
