#!/bin/sh

if test "$1" != "pageonly"; then

  # Generate the images for the Newton-Raphson fractals web site.

  gcc -o newton newton.c -lm

  ./newton -o simple.bmp -s 256x256 -x 2 -f 1 -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o zoomed.bmp -s 256x256 -x 0.4 -X 1.05 -Y 1.05  -f 1 -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o rainbow.bmp -s 320x256 -x 5 -f 1 -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o rainbowz1.bmp -s 256x256 -Y 3.5125 -X 1.08125 -x 0.06 -f 1 -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o rainbowz2.bmp -s 256x256 -X 4.5275 -Y -1.5275 -x 0.12 -f 1 -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o shaded.bmp -s 256x256 -x 2 -f 13 -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o shaded2.bmp -s 256x256 -x 2 -f 10 -C no -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o shaded3.bmp -s 256x256 -x 2 -f 10 -C no -B yes -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i

  ./newton -o rainbowp1.bmp -s 320x256 -x 5 -B yes -C no -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o rainbowp0.75.bmp -p 0.75 -f 25 -s 320x256 -x 5 -B yes -C no -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o rainbowp0.6.bmp -p 0.6 -f 48 -s 320x256 -x 5 -B yes -C no -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o rainbowp0.52.bmp -p 0.52 -f 200 -s 320x256 -x 5 -B yes -C no -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3

  ./newton -o bigsimple.bmp -s 600x600 -x 2 -f 10 -C no -B yes -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o bigzoomed.bmp -s 600x600 -x 0.4 -X 1.05 -Y 1.05 -f 10 -C no -B yes -c 1,0,0:1,1,0:0,0.7,0:0,0.5,1 -- -1 +1 -i +i
  ./newton -o bigrainbow.bmp -s 800x600 -x 5 -f 10 -C no -B yes -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o bigrainbowz1.bmp -s 600x600 -Y 3.5125 -X 1.08125 -x 0.06 -f 10 -C no -B yes -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3
  ./newton -o bigrainbowz2.bmp -s 600x600 -X 4.5275 -Y -1.5275 -x 0.12 -f 10 -C no -B yes -c 1,0,0:1,0.7,0:1,1,0:0,1,0:0,0.7,1:0,0,1:0.5,0,1 -- -3 -2 -1 0 1 2 3

  rm newton

  for bmp in ; do
    jpeg=`echo $bmp | sed 's/\.bmp$/.jpeg/'`
    convert $bmp $jpeg && rm $bmp
  done

  for bmp in *.bmp; do
    png=`echo $bmp | sed 's/\.bmp$/.png/'`
    convert $bmp $png && rm $bmp
  done

fi

perl -pe '/^<picture (.*)>/ and do {' \
      -e '  $fname = -f "$1.png" ? "$1.png" : "$1.jpeg";' \
      -e '  $a=`identify $fname`; @a = split " ", $a;' \
      -e '  $size = (stat $fname)[7]; $size = int(($size + 1023) / 1024);' \
      -e '  $a[2] =~ /(\d+)x(\d+)/ or die "argh?"; $w=$1; $h=$2;' \
      -e '  $_ = "<a href=\"$fname\"><img src=\"$fname\" alt=\"" .' \
      -e '       "[$a[2] $a[1], ${size}KB]\" width=$w height=$h></a>\n";' \
      -e '};' < index.template > index.html
