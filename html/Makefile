all: interfaces.html interfaces.txt smell.html aliases.html aliases.txt

smell.html: smell.but
	$(HOME)/src/halibut/build/halibut smell.but
	mv Manual.html smell.html
	rm -f output.* Manual.html Contents.html Chapter*.html Section*.html IndexPage.html

interfaces.html interfaces.txt: interfaces.but
	$(HOME)/src/halibut/build/halibut interfaces.but
	mv Manual.html interfaces.html
	mv output.txt interfaces.txt
	rm -f output.* Manual.html Contents.html Chapter*.html Section*.html IndexPage.html

aliases.html aliases.txt: aliases.but
	$(HOME)/src/halibut/build/halibut aliases.but
	mv Manual.html aliases.html
	mv output.txt aliases.txt
	rm -f output.* Manual.html Contents.html Chapter*.html Section*.html IndexPage.html

clean:
	rm -f smell.html
	rm -f interfaces.html interfaces.txt
	rm -f aliases.html aliases.txt
