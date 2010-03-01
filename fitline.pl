#!/usr/bin/perl

# Fit a straight line of the form y = a+bx to data.
#
# Expects input data in the form of lines of text with two numbers
# per line, which are x and y respectively. (This is the same format
# used as input to gnuplot, which should be helpful.)
#
# Output is two numbers, a and b respectively.
#
# For example, suppose you have such a data file, and entering the
# gnuplot command 'plot "myfile"' gives a plausibly straight-looking
# line. Now you run "fitline.pl myfile", which prints (say) the
# output '-1.63584 * x + 0.573934'. Then you should find that going
# back into gnuplot and running
#   plot "myfile", -1.63584 + x * 0.573934
# produces a straight line that matches the data.
#
# This program will also do least-squares fits of larger
# polynomials: add the argument '-2' and you'll get a quadratic,
# '-3' for a cubic, and so on.

$order = 1;
while ($ARGV[0] =~ /^-/) {
    $opt = shift @ARGV;
    last if $opt eq "--";
    $order = $1 if $opt =~ /^-(\d+)$/;
}

@sxn = (0) x ($order*2+1);
@sxny = (0) x ($order+1);

while (<>) {
    split;
    if (2 == scalar @_) {
	$x = $_[0];
	$y = $_[1];
	$xn = 1;
	for ($i = 0; $i <= $order*2; $i++) {
	    $sxn[$i] += $xn;
	    $xn *= $x;
	}
	$xny = $y;
	for ($i = 0; $i <= $order; $i++) {
	    $sxny[$i] += $xny;
	    $xny *= $x;
	}
    }
}

$matrix = [];
for ($i = 0; $i < $order+1; $i++) {
    $row = [];
    for ($j = 0; $j < $order+1; $j++) {
	push @$row, $sxn[$i+$j];
    }
    push @$row, $sxny[$i];
    push @$matrix, $row;
}

@results = &solve($matrix);

# Format the output as a polynomial.
$text = sprintf "%.17g", $results[$#results];
$plevel = 2;
for ($i = $#results-1; $i >= 0; $i--) {
    $text = "($text)" if $plevel < 1;
    $text = sprintf "%.17g + x * %s", $results[$i], $text;
    $plevel = 0;
}
printf "%s\n", $text;

sub gaussianelimination {
    # Adapted from pseudocode found on Wikipedia, with partial
    # pivoting which Wikipedia claims is numerically stable enough
    # in practice though not in worst-case-principle.

    my $matrix = shift @_; # array of rows, each one an array of numbers
    my $m = scalar @$matrix;
    my $n = scalar @{$matrix->[0]};
    my $k, $maxi, $u, $tmp;
    my $i = 0;
    my $j = 0;
    while ($i < $m and $j < $n) {
	# Find pivot in column j, starting in row i
	$maxi = $i;
	for ($k = $i+1; $k < $m; $k++) {
	    if (abs($matrix->[$k]->[$j]) > abs($matrix->[$maxi]->[$j])) {
		$maxi = $k;
	    }
	}

	if ($matrix->[$maxi]->[$j] != 0) {
	    # swap rows i and maxi, but do not change the value of i
	    $tmp = $matrix->[$maxi];
	    $matrix->[$maxi] = $matrix->[$i];
	    $matrix->[$i] = $tmp;
	    # Now matrix[i,j] will contain the old value of matrix[maxi,j].

	    # divide each entry in row i by A[i,j]
	    $tmp = $matrix->[$i]->[$j];
	    for ($k = 0; $k < $n; $k++) {
		$matrix->[$i]->[$k] /= $tmp;
	    }
	    # Now matrix[i,j] will have the value 1.

	    for ($u = $i+1; $u < $m; $u++) {
		# subtract matrix[u,j] * row i from row u
		$tmp = $matrix->[$u]->[$j];
		for ($k = 0; $k < $n; $k++) {
		    $matrix->[$u]->[$k] -= $tmp * $matrix->[$i]->[$k];
		}
		# Now matrix[u,j] will be 0, since
		#    matrix[u,j] - matrix[i,j] * matrix[u,j]
		#  = matrix[u,j] -           1 * matrix[u,j] = 0.
	    }
	    $i++;
	}
	$j++;
    }
}

sub solve {
    my $matrix = shift @_; # array of rows, each one an array of numbers
    my $m = scalar @$matrix;
    my $n = scalar @{$matrix->[0]};
    my @results = (0) x $m;
    my $i, $j, $curr;
    &gaussianelimination($matrix);
    for ($i = $m-1; $i >= 0; $i--) {
	$curr = $matrix->[$i]->[$m];
	for ($j = $i+1; $j < $m; $j++) {
	    $curr -= $matrix->[$i]->[$j] * $results[$j];
	}
	die "singular matrix!\n" if $matrix->[$i]->[$i] == 0;
	$curr /= $matrix->[$i]->[$i];
	$results[$i] = $curr;
    }
    return @results;
}
