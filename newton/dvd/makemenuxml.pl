#!/usr/bin/perl 

# Generate a spumux file describing a DVD menu, given a list of box
# locations on input. Box locations are given in inches, with the
# total size of the image being 6 inches by 4.5.

if ($ARGV[0] eq "--ntsc") {
    $ymult = 480 / 4.5;
    shift @ARGV;
} else {
    $ymult = 576 / 4.5;
}
$xmult = 720 / 6;

# Two or three arguments: the names of the highlight and select
# images, and optionally a file containing LRUD targets for each
# button.
$highlightpng = shift @ARGV;
$selectpng = shift @ARGV;

if (defined ($lrud = shift @ARGV)) {
    $i = 1;
    open LRUD, "<$lrud";
    while (<LRUD>) {
	split;
	$lrud{$i} = [@_];
	$i++;
    }
}

print "<subpictures>\n";
print "<stream>\n";
print "<spu start=\"00:00:00.0\" end=\"00:02:20.0\" "; # 200 seconds
print "highlight=\"$highlightpng\" select=\"$selectpng\" force=\"yes\">\n";
while (<STDIN>) {
    @dim = split;
    next unless $dim[0] eq "box";
    push @buttons, [int($dim[2]*$xmult), int($dim[6]*$xmult), int($dim[4]*$ymult), int($dim[8]*$ymult)];
}
sort { $a->[2] <=> $b->[2] } @buttons;
$nbutt = scalar @buttons;
for ($i = 0; $i < $nbutt; $i++) {
    ($x0, $x1, $y0, $y1) = @{$buttons[$i]};
    print "<button x0=\"$x0\" x1=\"$x1\" y0=\"$y0\" y1=\"$y1\" ";
    $dispi = $i+1;
    if (defined $lrud{$dispi}) {
	$left = $lrud{$dispi}->[0];
	$right = $lrud{$dispi}->[1];
	$up = $lrud{$dispi}->[2];
	$down = $lrud{$dispi}->[3];
    } else {
	$next = ($i + 1) % $nbutt;
	$prev = ($i + $nbutt - 1) % $nbutt;
	$down = $next+1;
	$up = $prev+1;
	$left = $right = $dispi;
    }
    print "up=\"$up\" down=\"$down\" left=\"$left\" right=\"$right\" />\n";
}
print "</spu>\n";
print "</stream>\n";
print "</subpictures>\n";
