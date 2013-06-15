#!/bin/bash

scale=${1-10}

domap() {
  ./makemap.sh "$@" desc.$n
  n=$[$n+1]
}

n=0

domap $scale -10000 '0 0 1 setrgbcolor'
domap $scale -500 '0 0.752941 0 setrgbcolor'
domap $scale 0 '0 1 0 setrgbcolor'
domap $scale 200 '0.752941 1 0 setrgbcolor'
domap $scale 400 '1 1 0 setrgbcolor'
domap $scale 1000 '1 0.752941 0 setrgbcolor'
domap $scale 1500 '0.752941 0.501961 0 setrgbcolor'
domap $scale 2000 '0.627451 0.439216 0 setrgbcolor'
domap $scale 3000 '0.313725 0.439216 0.690196 setrgbcolor'
domap $scale 4000 '0.470588 0.658824 0.847059 setrgbcolor'
domap $scale 6000 '0.627451 0.878431 1 setrgbcolor'

set --
m=0
while test $m -lt $n; do
  set -- "$@" desc.$m
  m=$[$m+1]
done

cat "$@" > desc.colour
