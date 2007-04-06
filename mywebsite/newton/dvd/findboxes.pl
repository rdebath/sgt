#!/usr/bin/perl 

# Process a DVI file, using dvitype, to find and output the
# bounding boxes of all rules displayed in it.

open DVITYPE, "dvitype $ARGV[0] |";
$ppi = $ppu = 1;
$h = $v = 0;
while (<DVITYPE>) {
    /([\d\.]+) pixels per inch/ and $ppi = $1;
    /([\d\.]+) pixels per DVI unit/ and $ppu = $1;
    /(putrule|setrule) height (\d+), width (\d+)/ and do {
	printf "box left %g top %g right %g bottom %g\n",
	  $h * $ppu / $ppi,
	  ($v-$2) * $ppu / $ppi,
	  ($h+$3) * $ppu / $ppi,
	  $v * $ppu / $ppi;
    };
    /h:=([\d\-\+]+=)?(\d+)/ and do { $h = $2; };
    /v:=([\d\-\+]+=)?(\d+)/ and do { $v = $2; };
    /beginning of page/ and do { $h = $v = 0; }
}
close DVITYPE;
exit $?;
