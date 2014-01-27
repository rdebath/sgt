all: interfaces.html interfaces.txt smell.html aliases.html aliases.txt \
     svn.html cdescent/index.html

smell.html: smell.but
	$(HOME)/src/halibut/build/halibut --html=smell.html smell.but

svn.html: svn.but
	$(HOME)/src/halibut/build/halibut --html=svn.html svn.but

interfaces.html interfaces.txt: interfaces.but
	$(HOME)/src/halibut/build/halibut --html=interfaces.html \
	        --text=interfaces.txt interfaces.but

aliases.html aliases.txt: aliases.but
	$(HOME)/src/halibut/build/halibut --html=aliases.html \
	        --text=aliases.txt aliases.but

cdescent/index.html: cdescent.but cdescent.pl
	mkdir -p cdescent
	$(HOME)/src/halibut/build/halibut --html=- cdescent.but | ./cdescent.pl > cdescent/index.html

clean:
	rm -f smell.html
	rm -f interfaces.html interfaces.txt
	rm -f aliases.html aliases.txt
