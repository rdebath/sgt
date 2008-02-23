#!/usr/bin/perl 

# Script to output XPMs of the ick-proxy icon at four different sizes.

# Can be used to construct Windows icon files via the `icon.pl'
# .ICO creation script in PuTTY:
#
#   icon.pl -1 icon16.xpm > icksmall.ico
#   icon.pl -1 icon16.xpm icon32.xpm icon48.xpm > ickproxy.ico

# Can also be used to construct a Mac OS X icon file, by converting
# the four XPMs to a format acceptable to OS X Icon Composer (e.g.
# PNG) and dragging them on to the four spaces in the Icon Composer
# window.

$pi = 4*atan2(1,1);

foreach $size (16, 32, 48, 128) {
    $serad = int($size/16.0 + 0.5);
    $cent = ($serad % 2) * 0.5;
    $serad /= 2.0;
    $rnd = sub { int($_[0] - $cent + 0.5) + $cent; };

    open OUT, ">", "icon$size.xpm";
    select OUT;
    print "/* XPM */\n";
    print "static char *icon${size}[] = {\n";
    print "/* columns rows colors chars-per-pixel */\n";
    print "\"$size $size 3 1\",\n";
    print "\". c none\",\n";
    print "\"+ c #ffffff\",\n";
    print "\"# c #000000\",\n";
    print "\/* pixels \*/\n";

    $pix = "." x ($size*$size);

    &supercircle($size/2.0, $size/2.0, $size/2-int(($size+63)/64), 3, "+");
    &line($size*3.0/16, 0, sub { my ($x,$y)=@_; &supercircle($rnd->($size*5.5/16 - $x), $rnd->($size*4.5/16 - $y), $serad, 2, "#"); });
    &line($size*3.0/16, 0, sub { my ($x,$y)=@_; &supercircle($rnd->($size*10.5/16 + $x), $rnd->($size*4.5/16 - $y), $serad, 2, "#"); });
    &line($size*2.0/16, 1, sub { my ($x,$y)=@_; &supercircle($rnd->($size*5.5/16 - $x), $rnd->($size*4.5/16 - $y), $serad, 2, "#"); });
    &line($size*2.0/16, 1, sub { my ($x,$y)=@_; &supercircle($rnd->($size*10.5/16 + $x), $rnd->($size*4.5/16 - $y), $serad, 2, "#"); });
    &supercircle($size/2.0, int($size/2.1), int($size*0.09), 2.3, "#");
    $semidist = int($size*5.0/16)+0.5;
    for ($x = - $semidist; $x <= +$semidist; $x++) {
	$xx = $size/2.0 + $x;
	$xm = abs($x / $semidist);
	$yy = cos(3*$pi*$xm) * $size*0.7/16;
	&supercircle($rnd->($xx), $rnd->($size*11.0/16 - $yy), $serad, 2.5, "#");
    }
    &superellquad(0.5, int($size*1.5/16)+0.5, int($size*2.5/16)+0.5, 2.3, sub { my ($x,$y)=@_; &supercircle($rnd->($size/2 + $x), $rnd->($size*12.0/16 + $y), $serad, 2, "#"); });
    &superellquad(0.5, int($size*1.5/16)+0.5, int($size*2.5/16)+0.5, 2.3, sub { my ($x,$y)=@_; &supercircle($rnd->($size/2 - $x), $rnd->($size*12.0/16 + $y), $serad, 2, "#"); });

    for ($i = 0; $i < $size*$size; $i += $size) {
	print "\"";
	print substr($pix, $i, $size);
	print "\"";
	print "," unless $i+$size == $size*$size;
	print "\n";
    }

    print "};\n";
    close OUT;
}

sub supercircle($$$$$)
{
    my ($x, $y, $r, $power, $chr) = @_;
    my $i, $xx, $yy;
    for ($i = 0; $i < $size*$size; $i++) {
	$xx = $i % $size + 0.5;
	$yy = int($i / $size) + 0.5;
	$xx -= $x;
	$yy -= $y;
	if (abs($xx)**$power + abs($yy)**$power <= $r**$power) {
	    substr($pix,$i,1) = $chr;
	}
    }
}

sub line($$$)
{
    my ($xmax, $gradient, $proc) = @_;
    my $x, $y;
    for ($x = 0; $x <= $xmax; $x++) {
	$y = int(0.5 + $x * $gradient);
	$proc->($x, $y);
    }
}

sub superellquad($$$$)
{
    my ($xs, $xr, $yr, $power, $proc) = @_;
    my $x, $y, $ynew;
    $y = undef;
    for ($x = $xs; $x <= $xr; $x++) {
	$ynew = int((1.0 - ($x*1.0/$xr)**$power) ** (1.0/$power) * $yr);
printf STDERR "$x $ynew\n" if $size == 16;
	if ($ynew == $y or !defined $y) {
	    $y = $ynew;
printf STDERR " - $x $y\n" if $size == 16;
	    $proc->($x, $y);
	} else {
	    while ($y > $ynew) {
		$y--;
printf STDERR " + $x $y\n" if $size == 16;
		$proc->($x, $y);
	    }
	}
    }
}
