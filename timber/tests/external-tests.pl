use warnings;
use strict;
use Carp;


BEGIN {
    my $binary = shift
	or die "Please name timber executable to test on command line\n";
    my $dir = "test." . time;
    mkdir $dir or die "mkdir $dir: $!";

    sub run_timber {
	my $stem = "$dir/" . shift;
	open IN, ">$stem.in" or die "open >$stem.in: $!";
	print IN shift;
	close IN or die "close >$stem.in: $!";
	my $r = system "$binary -D $stem <$stem.in >$stem.out 2>$stem.err @_";
	return ($r,
		scalar `cat $stem.out`,
		scalar `cat $stem.err`);
    }
}


sub a_test ($$@) {
    my $name = shift;
    my $line = shift;
    return {
	name => $name,
	line => $line,
	ret => 0,
	stdin => "",
	stdout => "",
	stderr => "",
    };
}

sub run_test {
    my $test = shift;
    my ($ret, $stdout, $stderr) = run_timber ($test->{name},
					      $test->{stdin},
					      $test->{line});
    my @wrong;
    push @wrong, "-----  Return code was $ret, should have been $test->{ret}\n"
	unless $test->{ret} == $ret;
    unless ($test->{stdout} eq $stdout) {
	push @wrong, ("-----  STDOUT was:\n$stdout\n"
		      . "-----  STDOUT should have been:\n$test->{stdout}\n");
    }
    unless ($test->{stderr} eq $stderr) {
	push @wrong, ("-----  STDERR was:\n$stderr\n"
		      . "-----  STDERR should have been:\n$test->{stderr}\n");
    }
    return unless @wrong;
    return join "", "=====  Failed $test->{name}:\n", @wrong;
}


my @test;

push @test, a_test init => "init";


my @failure = map { run_test($_) } @test;
if (@failure) {
    print @failure;
    printf "=====  Failed %d of %d tests.\n", scalar @failure, scalar @test;
} else {
    printf "=====  Passed all %d tests.\n", scalar @test;
}
