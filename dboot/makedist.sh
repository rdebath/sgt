#!/bin/sh
mkdir dist dist/dboot
for i in \
  Makefile bootsect bootsect.asm dboot.c dboot.doc loader loader.asm \
  makedist.sh makeloader.c test.bootsect test.loader \
; do
  ln -s ../../$i dist/dboot
done
cd dist
tar chzvf ../dboot.tar.gz dboot
cd ..
rm -rf dist
