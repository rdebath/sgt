#!/usr/bin/perl 

open IN, "index.html";
open OUT, ">index2.html";

while (<IN>) {
    chomp;
    /<img src=\"([^\"]+)\"/ and do {
	$src = $1;
	$k = `identify $src`;
	$k =~ /PNG (\d+)x(\d+)/ and do { $x = $1; $y = $2; };
	@st = stat $src;
	$alt = sprintf "[%dx%d greyscale PNG, %dKB]",
	  $x, $y, int(($st[7] + 1023) / 1024);
    };
    /^(.*alt=\")[^\"]+(\".*width=)\d+(.*height=)\d+(.*)$/ and do {
	$_ = $1 . $alt . $2 . $x . $3 . $y . $4; };
    print OUT "$_\n";
}

close IN;
close OUT;

rename "index.html", "index.html~";
rename "index2.html", "index.html";
