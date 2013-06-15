#!/bin/bash

loudly() { echo "$@" >&2; "$@"; }

scale=${1-10}
altitude=${2-0}
colour="${3-0 setgray}"
filename="${4-desc.bw}"

./makebmp $altitude $scale
convert -flip output.bmp outputup.bmp

xoff=0
for i in 1 2 3 4 5; do
  # Top row
  convert -crop $[9600/$scale]x$[8400/$scale]+$[16800/$scale]+0 \
    outputup.bmp piece${i}0.bmp
  potrace -c piece${i}0.bmp
  yes '' | loudly gs -dBATCH -dQUIET -sDEVICE=nullpage -ssgt-colour="$colour" \
    -dsgt-xmul=$scale -dsgt-ymul=-$scale \
    -dsgt-xoff=$[-4800-$xoff*$scale] -dsgt-yoff=8400 \
    sphunpack.ps piece${i}0.eps > piece${i}0.shape
  # Middle row
  convert -crop $[9600/$scale]x$[7200/$scale]+$[16800/$scale]+$[7200/$scale] \
    outputup.bmp piece${i}1.bmp
  potrace -c piece${i}1.bmp
  yes '' | loudly gs -dBATCH -dQUIET -sDEVICE=nullpage -ssgt-colour="$colour" \
    -dsgt-xmul=$scale -dsgt-ymul=-$scale \
    -dsgt-xoff=$[-4800-$xoff*$scale] -dsgt-yoff=14400 \
    sphunpack.ps piece${i}1.eps > piece${i}1.shape
  # Bottom row
  convert -crop $[9600/$scale]x$[8400/$scale]+$[16800/$scale]+$[13200/$scale] \
    outputup.bmp piece${i}2.bmp
  potrace -c piece${i}2.bmp
  yes '' | loudly gs -dBATCH -dQUIET -sDEVICE=nullpage -ssgt-colour="$colour" \
    -dsgt-xmul=$scale -dsgt-ymul=-$scale \
    -dsgt-xoff=$[-4800-$xoff*$scale] -dsgt-yoff=21600 \
    sphunpack.ps piece${i}2.eps > piece${i}2.shape
  # Now roll the bitmap around by 1/5 in the x direction.
  if test $i -lt 5; then
    newxoff=$[(43200/$scale) * $i / 5]
    convert -roll +$[$newxoff-$xoff]+0 outputup.bmp new.bmp
    mv new.bmp outputup.bmp
    xoff=$newxoff
  fi
done

cat piece{1,2,3,4,5}{0,1,2}.shape | grep -v 'showpage' > $filename
