MODULES = tring chumby almsnd display gconsts

IMAGEINDICES = $(shell i=1; while test $$i -le 101; do printf %03d\\n $$i; i=$$[1+i]; done)
PNGS = $(patsubst %,build/image%.png,$(IMAGEINDICES))
SPRITES = $(patsubst %,build/image%.spr,$(IMAGEINDICES))
SPRITEOBJS = $(patsubst %,build/image%.o,$(IMAGEINDICES))
SPRITESRCS = $(patsubst %,build/image%.c,$(IMAGEINDICES))

OBJECTS = $(patsubst %,build/%.o,$(MODULES)) $(SPRITEOBJS)

build/tring: $(OBJECTS)
	arm-linux-gcc -o $@ $(OBJECTS) -lasound -lcurl

build/tring.o: tring.c
	arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	arm-linux-gcc -c -o $@ $<

build/chumby.o: chumby.c
	arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	arm-linux-gcc -c -o $@ $<

build/display.o: display.c
	arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	arm-linux-gcc -c -o $@ $<

build/gconsts.o: build/gconsts.c
	arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	arm-linux-gcc -c -o $@ $<

build/almsnd.o: build/almsnd.c
	arm-linux-gcc -c -o $@ $<

build/almsnd.c: build/almsnd.dat mkarray.pl
	./mkarray.pl $< alarmsound > $@

build/almsnd.dat: build/genalarm
	./$< > $@

build/genalarm: genalarm.c
	gcc --std=c99 -o $@ $< -lm

build/gconsts.c $(PNGS): graphics.ps
	gs -g960x720 -r216 -sDEVICE=png16m -sOutputFile=build/image%03d.png -q -dBATCH -dNOPAUSE graphics.ps > build/gconsts.c

$(SPRITES): build/image%.spr : build/image%.png mksprite.pl
	convert -scale 320x240 -depth 8 $< rgb:- | ./mksprite.pl > $@

$(SPRITEOBJS): build/image%.o : build/image%.c
	arm-linux-gcc -c -o $@ $<

$(SPRITESRCS): build/image%.c : build/image%.spr
	./mkarray.pl $< $(patsubst build/image%.spr,image%,$<) > $@

clean:
	rm -f build/*

-include build/*.d
