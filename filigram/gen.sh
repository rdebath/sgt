#!/bin/sh

# Generate the images for the filigram web site.

gcc -o filigram filigram.c

./filigram -s 320x256 -x 10 -b 320x0 -p x2+y2 -O 2 -o circles.bmp
./filigram -s 320x256 -x 10 -b 320x0 -p x2y2 -O 0.125 -o original.bmp
./filigram -s 320x256 -x 8 -b 320x0 -p 4x4-17x2y2+4y4 -O 0.015625 -o newfunc.bmp
./filigram -s 640x512 -x 8 -b 320x0 -p 4x4-17x2y2+4y4 -O 0.015625 -o enlarged.bmp
./filigram -s 320x256 -x 8 -b 320x0 -p 4x4+17x2y2+4y4 -O 0.015625 -o complex.bmp
./filigram -s 320x256 -x 8 -b 320x0 -p 4x2y+y3 -O 0.5 -o semicomplex.bmp
./filigram -s 320x256 -x 8 -b 320x0 -p 4x4-17x2y2+4y4 -O 0.015625 -f -o faded.bmp
./filigram -s 320x256 -x 8 -b 320x0 -p 4x4-17x2y2+4y4 -O 0.015625 -f -c 0.7,0,1:0,0.63,0 -o coloured.bmp
./filigram -s 800x600 -x 10 -b 640x0 -p 4x4-17x2y2+4y4 -O 0.015625 -f -c 0.7,0,1:0,0.63,0 -o checkers.bmp
./filigram -s 800x600 -x 8 -b 640x0 -p x2y2 -f -c 1,0,0:1,1,0:0,1,0:0,1,1:0,0,1:1,0,1 -o rainbow.bmp
./filigram -s 800x600 -x 10 -b 640x0 -p 4x4-17x2y2+4y4 -O 0.015625 -f -c 2+0.61,0.0,0.0:0.48,0.0,0.56:0.82,0.64,0.0:0.0,0.7,0.0 -o jewels.bmp
./filigram -s 800x600 -x 10 -b 640x0 -p 4x4+17x2y2+4y4 -O 0.015625 -f -c 1,1,1:0,0,0 -o wormhole.bmp
./filigram -s 800x600 -x 10 -b 640x0 -p 4x2y+y3 -O 0.5 -f -c 0,0.4,1:0,0.7,0.5 -o deepsea.bmp

rm filigram

for bmp in *.bmp; do
  jpeg=`echo $bmp | sed 's/\.bmp$/.jpeg/'`
  convert $bmp $jpeg && rm $bmp
done
