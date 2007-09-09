#!/bin/sh

perl -pe '/^<picture (.*)>/ and do {' \
      -e '  $fname = "$1.gif";' \
      -e '  $a=`identify $fname`; @a = split " ", $a;' \
      -e '  $size = (stat $fname)[7]; $size = int(($size + 1023) / 1024);' \
      -e '  $a[2] =~ /(\d+)x(\d+)/ or die "argh?"; $w=$1; $h=$2;' \
      -e '  $_ = "<a href=\"$fname\"><img src=\"$fname\" alt=\"" .' \
      -e '       "[$a[2] $a[1], ${size}KB]\" width=$w height=$h></a>\n";' \
      -e '};' < index.template > index.html
