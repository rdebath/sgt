#!/bin/sh

perl -e '$pi = 4*atan2(1,1);' \
     -e 'for ($i=0; $i<240; $i++) {' \
     -e '  printf "./newton -o frame%03d.bmp -s 320x256 -C no -B yes " .' \
     -e '         "-y 2 -f 10 -c 1,0,0:1,1,0:0,0.7,0 -- " .' \
     -e '         "+%f%+fi %+f%+fi %+f%+fi\n",' \
     -e '  $i, &lissa($i/240), &lissa(($i+80)/240), &lissa(($i+160)/240);' \
     -e '}' \
     -e 'sub lissa($) {' \
     -e '  my ($t) = @_;' \
     -e '  return (sin(2*$pi*$t), sin(4*$pi*$t));' \
     -e '}' \
> frames.def

sh frames.def

for i in frame*.bmp; do
  j=`echo $i | sed s/bmp/ppm/`
  convert $i $j && rm $i
done

cat << EOF > anim.param

INPUT
frame*.ppm [000-239]
END_INPUT

OUTPUT cascade.mpeg
INPUT_DIR .
BASE_FILE_FORMAT PPM
INPUT_CONVERT *
GOP_SIZE 16
SLICES_PER_FRAME 1
PIXEL HALF
PSEARCH_ALG LOGARITHMIC
BSEARCH_ALG CROSS2
IQSCALE 4
PQSCALE 6
BQSCALE 6
RANGE 12
REFERENCE_FRAME DECODED
PATTERN IBBPBBPBBPBBPBBP
FORCE_ENCODE_LAST_FRAME

EOF

ppmtompeg anim.param
