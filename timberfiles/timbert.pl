#!/usr/bin/perl

# Look up the MIME type for a file name.

$mimedottypes = shift @ARGV;
$fname = shift @ARGV;
$ext = "";
$ext = $1 if $fname =~ /\.([^\.\/]+)$/;

open MT, $mimedottypes;
while (<MT>) {
  chomp;
  next if /^\s*#/; # strip comments
  @words = split /\s+/, $_;
  $type = shift @words;
  do { print $type; exit 0; } if grep { $ext eq $_ } @words;
}
