MODULES = tring chumby almsnd display gconsts

IMAGEINDICES = $(shell i=1; while test $$i -le 100; do printf %03d\\n $$i; i=$$[1+i]; done)
PNGS = $(patsubst %,build/image%.png,$(IMAGEINDICES))
SPRITES = $(patsubst %,build/image%.spr,$(IMAGEINDICES))
SPRITEOBJS = $(patsubst %,build/image%.o,$(IMAGEINDICES))

OBJECTS = $(patsubst %,build/%.o,$(MODULES)) $(SPRITEOBJS)

build/tring: $(OBJECTS)
	TMPDIR=build arm-linux-gcc -o $@ $(OBJECTS) -lasound -lcurl

build/tring.o: tring.c
	TMPDIR=build arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	TMPDIR=build arm-linux-gcc -c -o $@ $<

build/chumby.o: chumby.c
	TMPDIR=build arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	TMPDIR=build arm-linux-gcc -c -o $@ $<

build/display.o: display.c
	TMPDIR=build arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	TMPDIR=build arm-linux-gcc -c -o $@ $<

build/gconsts.o: build/gconsts.c
	TMPDIR=build arm-linux-gcc -MM $< | sed s:^:build/: > $(basename $@).d
	TMPDIR=build arm-linux-gcc -c -o $@ $<

build/almsnd.o: build/almsnd.dat align.pl
	TMPDIR=build arm-linux-objcopy -I binary $< \
		--binary-architecture=arm -O elf32-littlearm $@
	./align.pl $@

build/almsnd.dat: build/genalarm
	./$< > $@

build/genalarm: genalarm.c
	TMPDIR=build gcc --std=c99 -o $@ $< -lm

build/gconsts.c $(PNGS): graphics.ps
	gs -g960x720 -r216 -sDEVICE=png16m -sOutputFile=build/image%03d.png -q -dBATCH -dNOPAUSE graphics.ps > build/gconsts.c

$(SPRITES): build/image%.spr : build/image%.png mksprite.pl
	convert -scale 320x240 -depth 8 $< rgb:- | ./mksprite.pl > $@

$(SPRITEOBJS): build/image%.o : build/image%.spr
	TMPDIR=build arm-linux-objcopy -I binary $< \
		--binary-architecture=arm -O elf32-littlearm $@
	./align.pl $@

clean:
	rm -f build/*

-include build/*.d
