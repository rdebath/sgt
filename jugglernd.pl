#!/usr/bin/perl 

# Generate a random valid siteswap of a specified length.
#
# Usage: jugglernd.pl [len [top [n]]]
# where: len     length of siteswap (default 3)
#        top     highest throw permissible in siteswap (default len-1)
#        n       number of siteswaps to generate (default 1)
#
# Output is given as a single concatenated string if the siteswap
# contains no throws above 9, otherwise as a list of
# comma-separated numbers. So `jugglernd.pl 3 9' will always
# generate things looking like integers, whereas `jugglernd.pl 3
# 10' will sometimes use commas.

srand time;

$len = shift @ARGV;
$len = 3 unless defined $len;

$top = shift @ARGV;
$top = $len-1 unless defined $top;

$n = shift @ARGV;
$n = 1 unless defined $n;

while ($n-- > 0) {

    @src = (0..$len-1);
    @dst = ();
    while (scalar @src > 0) {
	$idx = int(rand() * (scalar @src));
	push @dst, splice @src, $idx, 1;
    }

    $max = 0;
    @out = ();
    for ($i = 0; $i < $len; $i++) {
	$diff = $dst[$i] - $i;
	$diff += $len while $diff < 0;
	$diff += $len * int(rand() * (1 + int(($top - $diff) / $len)));
	$max = $diff if $max < $diff;
	push @out, $diff;
    }

    printf "%s\n", join +($max > 9 ? "," : ""), @out;
}
