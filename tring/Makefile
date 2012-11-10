ifeq ($(GTK), yes)
PLATFORMMODULE = gtk
BUILDDIR = gtkbuild
PREFIX =
CCFLAGS = -g -O0 `pkg-config --cflags gtk+-2.0`
LIBS = `pkg-config --libs gtk+-2.0`
else
PLATFORMMODULE = chumby
BUILDDIR = build
PREFIX = arm-linux-#
endif

IMAGEINDICES = $(shell i=1; while test $$i -le 101; do printf %03d\\n $$i; i=$$[1+i]; done)
PNGS = $(patsubst %,$(BUILDDIR)/image%.png,$(IMAGEINDICES))
SPRITES = $(patsubst %,$(BUILDDIR)/image%.spr,$(IMAGEINDICES))
SPRITEOBJS = $(patsubst %,$(BUILDDIR)/image%.o,$(IMAGEINDICES))
SPRITESRCS = $(patsubst %,$(BUILDDIR)/image%.c,$(IMAGEINDICES))
MODULES = tring almsnd display gconsts $(PLATFORMMODULE)

IMAGEINDICES = $(shell bash -c 'i=1; while test $$i -le 101; do printf %03d\\n $$i; i=$$[1+i]; done')
PNGS = $(patsubst %,$(BUILDDIR)/image%.png,$(IMAGEINDICES))
SPRITES = $(patsubst %,$(BUILDDIR)/image%.spr,$(IMAGEINDICES))
SPRITEOBJS = $(patsubst %,$(BUILDDIR)/image%.o,$(IMAGEINDICES))

OBJECTS = $(patsubst %,$(BUILDDIR)/%.o,$(MODULES)) $(SPRITEOBJS)

$(BUILDDIR)/tring: $(OBJECTS)
	$(PREFIX)gcc -o $@ $(OBJECTS) -lasound -lcurl $(LIBS)

$(BUILDDIR)/tring.o: tring.c
	$(PREFIX)gcc -MM $< | sed s:^:$(BUILDDIR)/: > $(basename $@).d
	$(PREFIX)gcc -DBUILDDIR=$(BUILDDIR) $(CCFLAGS) -c -o $@ $<

$(BUILDDIR)/chumby.o: chumby.c
	$(PREFIX)gcc -MM $< | sed s:^:$(BUILDDIR)/: > $(basename $@).d
	$(PREFIX)gcc -DBUILDDIR=$(BUILDDIR) $(CCFLAGS) -c -o $@ $<

$(BUILDDIR)/gtk.o: gtk.c
	$(PREFIX)gcc -MM $< | sed s:^:$(BUILDDIR)/: > $(basename $@).d
	$(PREFIX)gcc -DBUILDDIR=$(BUILDDIR) $(CCFLAGS) -c -o $@ $<

$(BUILDDIR)/almsnd.o: $(BUILDDIR)/almsnd.c
	$(PREFIX)gcc -c -o $@ $<

$(BUILDDIR)/almsnd.c: $(BUILDDIR)/almsnd.dat mkarray.pl
	./mkarray.pl $< alarmsound > $@

$(BUILDDIR)/display.o: display.c
	$(PREFIX)gcc -MM $< | sed s:^:$(BUILDDIR)/: > $(basename $@).d
	$(PREFIX)gcc -DBUILDDIR=$(BUILDDIR) $(CCFLAGS) -c -o $@ $<

$(BUILDDIR)/gconsts.o: $(BUILDDIR)/gconsts.c
	$(PREFIX)gcc -MM $< | sed s:^:$(BUILDDIR)/: > $(basename $@).d
	$(PREFIX)gcc -DBUILDDIR=$(BUILDDIR) $(CCFLAGS) -c -o $@ $<

$(BUILDDIR)/almsnd.dat: $(BUILDDIR)/genalarm
	./$< > $@

$(BUILDDIR)/genalarm: genalarm.c
	gcc --std=c99 -o $@ $< -lm

$(BUILDDIR)/gconsts.c $(PNGS): graphics.ps
	gs -g960x720 -r216 -sDEVICE=png16m -sOutputFile=$(BUILDDIR)/image%03d.png -q -dBATCH -dNOPAUSE graphics.ps > $(BUILDDIR)/gconsts.c

$(SPRITES): $(BUILDDIR)/image%.spr : $(BUILDDIR)/image%.png mksprite.pl
	convert -scale 320x240 -depth 8 $< rgb:- | ./mksprite.pl > $@

$(SPRITEOBJS): $(BUILDDIR)/image%.o : $(BUILDDIR)/image%.c
	$(PREFIX)gcc -c -o $@ $<

$(SPRITESRCS): $(BUILDDIR)/image%.c : $(BUILDDIR)/image%.spr
	./mkarray.pl $< $(patsubst $(BUILDDIR)/image%.spr,image%,$<) > $@

clean:
	rm -f $(BUILDDIR)/*

-include $(BUILDDIR)/*.d
