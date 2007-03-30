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

  jcount=0
  for c in 0.28+0.528j -0.656+0.272j; do
    jcount=$[1+$jcount]
    if test "x$MJ" != "x"; then
      $MJ -o julia${jcount}.bmp -l 64 -s 256x256 -x 2 -B yes -- $c
    fi
    nargs=`python -c 'import sys; from cmath import *; c=complex(sys.argv[1]); a=(1+sqrt(1-4*c))/2; b=(1-sqrt(1-4*c))/2; print "%s/%s %s/%s" % (str(a), str(1/(b-a)), str(b), str(1/(a-b)))' $c | tr -d '()'`
    ./newton -o njulia${jcount}.bmp -s 256x256 -x 2 -B yes -C no -n 1,1,1 -- $nargs
  done

  # This one's hard. We have to plot the actual fractal, plus a
  # preview picture containing the locations of its roots, then
  # overlay the one on the other.
  ./newton -o droots0.bmp -s 256x256 -x 2 -B yes -C no -c 0,1,0:0,1,1:1,0,0:1,0.5,0:0,0,1 -- 0.320302227058+0.12985466313i -0.695647288911-0.946021797603i -0.128068630814+0.819190930479i 1.33906244269-0.0480476681743i -1.25231541669-0.163309461165i
  ./newton -o droots1.bmp -s 256x256 -x 2 -B yes -C no --preview -n 0,0,0 -c 1,1,1 -- 1 0.5i -0.666666667-0.333333333i
  composite -size 256x256 xc:black droots0.bmp droots1.bmp droots.bmp
  rm -f droots0.bmp droots1.bmp

  ./newton -o five.bmp -s 400x400 -x 2 -B yes -C no -c 0,1,0:1,0.66,0:0,0,1:1,0,0:0,1,1:1,1,0:0,0.66,1:0,0.33,1:1,0.33,0 -- 0i -1.48569768456-0.29500617292i 0.937745438049+0.654375200482i -0.937745438049+0.654375200482i 0.937745438049-0.654375200482i -0.937745438049-0.654375200482i 1.48569768456-0.29500617292i 1.48569768456+0.29500617292i -1.48569768456+0.29500617292i
  ./newton -o five2.bmp -s 400x400 -x 2 -B yes -C no -c 0,0.66,1:1,0.66,0:0,1,0:0,0,1:1,0.33,0:1,0,0:0,1,1:0,0.33,1:1,1,0 -- 1.59169460745-0.24546826907i -1.58121088151-0.201561838736i -0.070936955933-0.476017840204i 0.830784434235+0.762430645333i -1.46297013029+0.429219868125i -0.778282216776+0.73002011667i 1.06236637445-0.725626234823i 1.50612815098+0.411783287772i -1.09757338261-0.684779735067i

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
