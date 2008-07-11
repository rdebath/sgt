#!/usr/bin/perl 

# Construct a 256x256 image in 16-bit grey-scale which is as
# accurate as possible a rendition of a Hilbert curve drawn filling
# the unit square ranging from black to white continuously along
# its length.

open OUT, "| convert -depth 16 -size 256x256 gray:- hilbert.png";

for ($y = 256; $y-- > 0 ;) {
    for ($x = 0; $x < 256; $x++) {
        $xx = $x;
        $yy = $y;
        $index = 0;
        $size = 256;

        while ($size > 1) {
            $size /= 2;
            $index *= 4;
            if ($xx < $size && $yy < $size) {
                ($xx, $yy) = ($yy, $xx);
            } elsif ($xx >= $size && $yy < $size) {
                $index += 3;
                $xx -= $size;
                ($xx, $yy) = ($size-1-$yy, $size-1-$xx);
            } else {
                if ($xx >= $size) {
                    $index += 2;
                    $xx -= $size;
                } else {
                    $index += 1;
                }
                $yy -= $size;
            }
        }

        print OUT pack "n", $index;
    }
}

close OUT;
