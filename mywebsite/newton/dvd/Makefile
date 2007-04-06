dvd: dvd.xml mainmenu.mpg aspectmenu.mpg newton.mpg newtonws.mpg newtonp.mpg newtonwsp.mpg chapter1menu.mpg chapter2menu.mpg chapter3menu.mpg chapter4menu.mpg chapter5menu.mpg chapter6menu.mpg
	rm -rf dvd
	dvdauthor -x dvd.xml

dvd.xml: dvdtemplate.xml newtonvideo.py credits.png
	perl -e '$$a.=$$_ while<>; $$a=~s/\[\[perl:(.*?)\]\]/eval $$1/egs;print$$a' $< > $@

newton.mpg: newton.mp2v newton.ac3
	mplex -f 8 -r 9800 -o $@ $^

newtonws.mpg: newtonws.mp2v newtonws.ac3
	mplex -f 8 -r 9800 -o $@ $^

newtonp.mpg: newtonp.mp2v newton.ac3
	mplex -f 8 -r 9800 -o $@ $^

newtonwsp.mpg: newtonwsp.mp2v newtonws.ac3
	mplex -f 8 -r 9800 -o $@ $^

newton.ac3: newtonvideo.py title.png credits.png
	rm -f newton.ac3
	dd if=/dev/zero bs=3840 count=`./newtonvideo.py --totalframes` | sox -t raw -wsr 48000 - -t wav -r 48000 - | ffmpeg -i - -ab 64 -ar 48000 -ac 2 newton.ac3

# Need separate audio tracks for normal and widescreen, because
# they're slightly different lengths due to the different time
# taken for the credits to roll.
newtonws.ac3: newtonvideo.py title.png credits.png
	rm -f newtonws.ac3
	dd if=/dev/zero bs=3840 count=`./newtonvideo.py --widescreen --totalframes` | sox -t raw -wsr 48000 - -t wav -r 48000 - | ffmpeg -i - -ab 64 -ar 48000 -ac 2 newtonws.ac3

newton.mp2v: makevid.sh newtonvideo.py title.png credits.png
	OUTFILE=$@ ./makevid.sh $(NTSC)

newtonws.mp2v: makevid.sh newtonvideo.py title.png credits.png
	OUTFILE=$@ ./makevid.sh $(NTSC) --widescreen

newtonp.mp2v: makevid.sh newtonvideo.py ptitle.png credits.png
	OUTFILE=$@ ./makevid.sh -p $(NTSC)

newtonwsp.mp2v: makevid.sh newtonvideo.py ptitle.png credits.png
	OUTFILE=$@ ./makevid.sh -p $(NTSC) --widescreen

credits.png: credits.dvi
	dvipng -D 360 -o $@ -T 6in,19in $<

credits.dvi: credits.tex
	tex credits

title.png: title.dvi
	dvipng -D 360 -o $@ -T 6in,4.5in $<

title.dvi: title.tex
	tex title

ptitle.png: ptitle.dvi
	dvipng -D 360 -o $@ -T 6in,4.5in $<

ptitle.dvi: ptitle.tex
	tex ptitle

mainmenu.mpg: mainmenu.tex Makefile.menu
	$(MAKE) -f Makefile.menu NAME=main NTSC=$(NTSC) $@

aspectmenu.mpg: aspectmenu.tex Makefile.menu
	$(MAKE) -f Makefile.menu NAME=aspect NTSC=$(NTSC) $@

chapter1menu.mpg: chapter1menu.tex chapter1menu.overlay fullmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter1 NTSC=$(NTSC) BUTTONS=fullmenu.buttons $@
chapter2menu.mpg: chapter2menu.tex chapter2menu.overlay fullmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter2 NTSC=$(NTSC) BUTTONS=fullmenu.buttons $@
chapter3menu.mpg: chapter3menu.tex chapter3menu.overlay fullmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter3 NTSC=$(NTSC) BUTTONS=fullmenu.buttons $@
chapter4menu.mpg: chapter4menu.tex chapter4menu.overlay fullmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter4 NTSC=$(NTSC) BUTTONS=fullmenu.buttons $@
chapter5menu.mpg: chapter5menu.tex chapter5menu.overlay fullmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter5 NTSC=$(NTSC) BUTTONS=fullmenu.buttons $@
chapter6menu.mpg: chapter6menu.tex chapter6menu.overlay halfmenu.buttons Makefile.menu credits.png
	$(MAKE) -f Makefile.menu NAME=chapter6 NTSC=$(NTSC) BUTTONS=halfmenu.buttons $@

inlay: inlay.ps

inlay.ps:
	$(MAKE) -f Makefile.inlay inlay.ps

clean:
	rm -f *.png *.dvi *.log *.mpg *.ppm *.pyc *.ac3 *.mp2v *.dat
	rm -f inlay.ps inlay.eps
	rm -f *menu.xml dvd.xml
