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


# Process return codes
sub success { 0 }
sub failure { 256 }

sub a_cmd ($@) {
    my ($line, %ret) = @_;
    $ret{line} = $line;
    $ret{ret} ||= success;
    $ret{$_} ||= "" for qw (stdin stdout stderr);
    return \%ret;
}

sub stream_errors ($$$) {
    my ($stream, $actual, $desired) = @_;
    $actual =~ s/\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d/<DATETIME>/g;
    return if $actual eq $desired;
    return ("$stream was:\n$actual", "$stream should have been:\n$desired");
}

sub run_cmd {
    my ($name, $cmd) = @_;
    my ($ret, $stdout, $stderr) = run_timber ($name,
					      $cmd->{stdin},
					      $cmd->{line});
    my @wrong;
    push @wrong, "Return code was $ret, should have been $cmd->{ret}"
	unless $cmd->{ret} == $ret;
    push @wrong, stream_errors "STDOUT", $stdout, $cmd->{stdout};
    push @wrong, stream_errors "STDERR", $stderr, $cmd->{stderr};
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
    local $_ = shift @{$test->{cmd}} or return;
    ++$test->{lineno};
    my @wrong = run_cmd $test->{name}, $_;
    return join ("",
		 SEP ("Failed $test->{name} at line $test->{lineno}:"
		      . " '$_->{line}'"),
		 map { sep } @wrong)
	if @wrong;
    return $test if @{$test->{cmd}};
    return;
}


BEGIN {
    my @test;

    sub test ($@) {
	my $name = shift;
	my @cmd = map { &a_cmd (@$_) } @_;
	push @test, a_test $name, @cmd;
    }

    END {
	my $ntests = @test;
	my $nfailures = 0;
	while (@test) {
	    @test = map { run_test($_) } @test;
	    my @failure = grep { not ref } @test;
	    @test = grep { ref } @test;
	    print @failure;
	    $nfailures += @failure;
	    sleep 1 if @test;
	}
	print SEP summary $nfailures, $ntests;
    }
}



#  Add tests after here.

test init => ["init"];
test get_name => (["init"],
		  ["contact name 0", stdout => "0\n"]);
test set_name => (["init"],
		  ["set-contact name 0 Dave"],
		  ["contact name 0", stdout => "0:Dave\n"],
		  ["set-contact name 0 Eric"],
		  ["contact name 0", stdout => "0:Eric\n"],
		  ["set-contact name 0"],
		  ["contact name 0", stdout => "0\n"]);
test get_history => (["init"],
		     ["set-contact name 0 Dave"],
		     ["set-contact name 0 Eric"],
		     ["contact-history name 0",
		      stdout => ("0;<DATETIME>;;Eric\n"
				 . "0;<DATETIME>;<DATETIME>;Dave\n")]);
test set_unknown_attr => (["init"],
                          ["set-contact zxcv 0 Henry",
			   ret => failure,
			   stderr => "timber: internal problem: No such " .
			             "attribute in address book database\n"]);
