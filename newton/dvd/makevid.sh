#!/bin/sh

doingopts=true

fps=25
aspect=2
bitrate=9700

for a in "$@"; do
  case "$a" in
    --widescreen) aspect=3;;
    --ntsc) fps=30;;
    -p) bitrate=500;;
  esac
done

./newtonvideo.py "$@" \
 | ppmtoy4m -F $fps:1 -A 59:54 -I p -S 420mpeg2 \
 | mpeg2enc -f 8 -a $aspect -b $bitrate -o ${OUTFILE:-newton.mp2v}
