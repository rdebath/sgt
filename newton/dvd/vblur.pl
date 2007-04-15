#!/usr/bin/perl 

# Accept a ppm on stdin and apply a vertical blur to it, to combat
# TV interlacing twitter.
#
# Method and rationale (suggested by Ben Harris):
#
# When displaying a picture on an interlaced TV which contains a
# very thin horizontal line, the biggest problem with the
# interlacing effect is that the line is present in one field and
# absent in the other, so that it doesn't just move slightly up and
# down (which is an inevitable consequence of interlacing) but
# actually flickers _on and off_.
#
# So we want to arrange that any feature of the picture is present
# in both fields, and to roughly the same extent (a 3-pixel line
# flicking between 2 and 1 pixels isn't very nice either). In other
# words, for any given column of pixels and any given RGB channel,
# we'd like to arrange that the _integral_ of brightness over that
# column is the same in both fields.
#
# Now. Suppose you have an idealised continuous graph of brightness
# against y-coordinate, and you're trying to represent it in
# finitely many pixels. An obvious approach is to divide your y
# range into subsections, one subsection per pixel, and set each
# pixel's brightness to be the average brightness within its
# subsection.
#
# An obvious adaptation of this which satisfies the integral
# condition is to make the subsections twice as wide as the gaps
# between their starting points. So all the even subsections
# between them cover the entire y range, and all the odd
# subsections between them _also_ cover the entire y range. (Modulo
# some missing data at one end or other, depending on parity, but
# we ignore that.)
#
# |00000|22222|44444|66666|88888|
#    |11111|33333|55555|77777|99999|
#
# Now if we were trying to represent the same brightness curve on a
# _non_-interlaced display with the same overall number of pixels,
# we would keep the same gap between subsection starting
# coordinates, but narrow the subsections themselves to the same
# width:
#
# |00|11|22|33|44|55|66|77|88|99|
#
# And the relationship between the result we'd get here and the one
# described above would be that each pixel in the interlaced
# version is precisely equal to the average of two adjacent pixels
# in the non-interlaced version.
#
# Hence, we can take any perfectly ordinary input image and
# de-twitter it by applying a fixed small vertical blurring effect:
# simply average each pair of vertically adjacent pixels. This
# causes it to satisfy the above integral condition while still
# retaining as much vertical detail as we reasonably can.

$ppm = $_;
while (!($ppm =~ /P6\D+(\d+)\D+(\d+)\D+(\d+)\D/s)) {
    $_ = <>;
    $ppm .= $_;
}
$ppm =~ /P6\D+(\d+)\D+(\d+)\D+(\d+)\D/s;
$w = $1;
$h = $2;
$bpp = ($3 > 255 ? 6 : 3);
$pattern = ($3 > 255 ? "n*" : "C*");

print $ppm;

@buffer = map { 0 } 1..($bpp*$w);

for ($i = 0; $i < $h; $i++) {
    $line = "";
    $len = read STDIN, $line, $bpp*$w;
    die "unexpected EOF\n" if $len < $bpp*$w;
    @newbuffer = unpack $pattern, $line;
    for ($j = 0; $j <= $#newbuffer; $j++) {
	$outbuffer[$j] = ($newbuffer[$j] + $buffer[$j]) >> 1;
    }
    print pack $pattern, @outbuffer;
    @buffer = @newbuffer;
}
