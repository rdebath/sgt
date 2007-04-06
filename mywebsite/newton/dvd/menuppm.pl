#!/usr/bin/perl 

$name = shift @ARGV;

if (! -f "${name}menu.overlay") {
    # Easy case.
    exec "convert ${name}menu-main-final.png ppm:-";
}

$w = `identify -format %w ${name}menu-main-final.png`;
$h = `identify -format %h ${name}menu-main-final.png`;

if ($h == 480) {
    $ntsc = "--ntsc";
} else {
    $ntsc = "";
}

@coords = ();
open BOXES, "<${name}menu-oboxes.dat";
while (<BOXES>) {
    split;
    $x = $_[2] * $w / 6;
    $y = $_[4] * $h / 4.5;
    push @coords, [int($x), int($y)];
}
close BOXES;

@ovl = ();
open OVL, "sh ${name}menu.overlay $ntsc |";
while (<OVL>) {
    split;
    $ppms = [];
    open PPMS, "./newtonvideo.py $ntsc -s -s -O$_[0] -L$_[1] |";
    while (<PPMS>) {
	$ppm = $_;
	while (!($ppm =~ /P6\D+(\d+)\D+(\d+)\D+(\d+)\D/s)) {
	    $_ = <PPMS>;
	    $ppm .= $_;
	}
	$ppm =~ /P6\D+(\d+)\D+(\d+)\D+(\d+)\D/s;
	# print STDERR "got ppm $1 x $2 x $3 :$ppm:\n";
	$bpp = ($3 > 255 ? 6 : 3);
	read PPMS, $ppm, $1 * $2 * $bpp, length $ppm;
	push @$ppms, $ppm;
    }
    push @ovl, $ppms;
}
close OVL;

for ($frame = 0; $frame < 5000; $frame++) {
    $cmdline = "convert ${name}menu-main-final.png ppm:-";
    for ($i = 0; $i <= $#coords && $i <= $#ovl; $i++) {
	$ppms = $ovl[$i];
	open TMP, ">tmpovl${i}.ppm";
	print TMP $ppms->[$frame % scalar @$ppms];
	close TMP;
	($x, $y) = @{$coords[$i]};
	$cmdline .= " | composite -geometry +${x}+${y} tmpovl${i}.ppm ppm:- ppm:-";
    }
    # print STDERR $cmdline;
    system $cmdline;
}
