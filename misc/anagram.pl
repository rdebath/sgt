#!/usr/bin/perl 

# usage: anagram.pl <words>
#
# anagram.pl will treat each line of its stdin as a candidate
# anagram of the words on its command line. It will either print
# "ok", meaning the anagram is perfect, or some combination of
# "missing:" and "left:" indicating which letters were
# (respectively) present on the command line but absent in the
# input line, and vice versa.
#
# I tried to write this as a Perl one-liner, but it ended up four
# lines long so I thought I might as well expand it into a halfway
# legible program.

$text = join "", @ARGV;
$text =~ y/a-z/A-Z/;
$text =~ y/A-Z//cd;
$text = pack "C*", sort {$a <=> $b} unpack "C*", $text;

while (<STDIN>) {
    y/a-z/A-Z/;
    y/A-Z//cd;
    $_ = pack "C*", sort {$a <=> $b} unpack "C*", $_;
    $a = $text;
    $b = "";
    foreach $i (unpack "C*", $_) {
	$c = pack "C",$i;
	$b .= $c unless $a =~ s/$c//;
    }
    print "missing: $b " if length $b;
    print "left: $a " if length $a;
    print "ok" if not (length $a or length $b);
    print "\n";
}
