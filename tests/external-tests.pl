use warnings;
use strict;
use Carp;

sub sep (;$) { local $_ = shift || $_; "-----  $_\n" }
sub SEP (;$) { local $_ = shift || $_; "=====  $_\n" }

sub summary ($$) {
    my ($failed, $taken) = @_;
    return "Failed $failed of $taken tests." if $failed;
    return "Passed all $taken tests.";
}


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


sub a_cmd ($@) {
    my ($line, %ret) = @_;
    $ret{line} = $line;
    $ret{ret} ||= 0;
    $ret{$_} ||= "" for qw (stdin stdout stderr);
    return \%ret;
}

sub run_cmd {
    my ($name, $cmd) = @_;
    my ($ret, $stdout, $stderr) = run_timber ($name,
					      $cmd->{stdin},
					      $cmd->{line});
    my @wrong;
    push @wrong, "Return code was $ret, should have been $cmd->{ret}"
	unless $cmd->{ret} == $ret;
    unless ($cmd->{stdout} eq $stdout) {
	push @wrong, ("STDOUT was:\n$stdout",
		      "STDOUT should have been:\n$cmd->{stdout}");
    }
    unless ($cmd->{stderr} eq $stderr) {
	push @wrong, ("STDERR was:\n$stderr",
		      "STDERR should have been:\n$cmd->{stderr}");
    }
    return @wrong;
}


sub a_test ($@) {
    my ($name, @cmd) = @_;
    return {
	name => $name,
	cmd => \@cmd,
    };
}

sub run_test {
    my $test = shift;
    my $line;
    for (@{$test->{cmd}}) {
	++$line;
	my @wrong = run_cmd $test->{name}, $_;
	return join ("",
		     SEP ("Failed $test->{name} at line $line: '$_->{line}'"),
		     map { sep } @wrong)
	    if @wrong;
    }
    return;
}


my @test;

push @test, a_test init => a_cmd "init";
push @test, a_test get_name => (a_cmd ("init"),
				a_cmd ("contact name 0", stdout => "0\n"));
push @test, a_test set_name => (a_cmd ("init"),
				a_cmd ("set-contact name 0 Dave"),
				a_cmd ("contact name 0",
				       stdout => "0:Dave\n"),
				a_cmd ("set-contact name 0 Eric"),
				a_cmd ("contact name 0",
				       stdout => "0:Eric\n"),
				a_cmd ("set-contact name 0"),
				a_cmd ("contact name 0", stdout => "0\n"));

my @failure = map { run_test($_) } @test;
print @failure, SEP summary @failure, @test;
