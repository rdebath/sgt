#!/usr/bin/perl 

# Take a 320x240 raw RGB image on standard input. Find its maximum
# extent, auto-crop to that, convert to 16-bit true colour, and
# generate on standard output the cropped data with an eight-byte
# (x, y, width, height) header.

$miny = $minx = 999;
$maxy = $maxx = -1;

@lines = ();
for ($y = 0; $y < 240; $y++) {
    $line = '';
    for ($x = 0; $x < 320; $x++) {
	read STDIN, $pix, 3;
	($r, $g, $b) = unpack "CCC", $pix;
	$pix = pack "v", (($b>>3) | (($g&0xFC)<<3) | (($r&0xF8)<<8));
	$line .= $pix;
	if ($pix ne "\0\0") {
	    $miny = $y if $miny > $y;
	    $minx = $x if $minx > $x;
	    $maxy = $y if $maxy < $y;
	    $maxx = $x if $maxx < $x;
	}
    }
    push @lines, $line;
}

if ($maxx < 0) {
    # No pixels at all!
    $minx = $miny = 0;
    $maxy = $maxx = -1;
}

print pack "vvvv", $minx, $miny, $maxx+1-$minx, $maxy+1-$miny;
for ($y = $miny; $y <= $maxy; $y++) {
    print substr($lines[$y], $minx * 2, ($maxx+1-$minx) * 2);
}
