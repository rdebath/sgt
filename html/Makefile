interfaces.html interfaces.txt: interfaces.but
	$(HOME)/src/halibut/build/halibut interfaces.but
	mv Manual.html interfaces.html
	mv output.txt interfaces.txt
	rm -f output.* Manual.html Contents.html Chapter*.html Section*.html IndexPage.html

clean:
	rm -f interfaces.html interfaces.txt
