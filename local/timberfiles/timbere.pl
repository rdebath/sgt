#!/usr/local/gnu/bin/perl

$c = pack 'C', shift @ARGV;
while (<>) { s/^/$c/e; print; }
