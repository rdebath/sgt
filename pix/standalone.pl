#!/usr/bin/perl 

# Create single standalone C files for the various programs here.
#
# This is absolutely horrid, but until now I've been distributing
# single C files, and it does seem like the simplest way to avoid
# having to distribute an enormous Makefile structure and
# everything...

$outpfx = shift @ARGV;

open RECIPE, "Recipe";
while (<RECIPE>) {
    if (/: \[U\] (\S+)/) {
	&munge("$1.c");
    }
}
close RECIPE;

sub load {
    my ($file) = @_;
    my @lines = ();

    open IN, "<$file";
    push @lines, $_ while <IN>;
    close IN;

    return @lines;
}

sub munge {
    my ($infile) = @_;
    my $outfile = $outpfx . $infile;

    my @lines = &load($infile);

    my $i, $line;
    my $cmdline_loaded = 0;
    my $bmpwrite_loaded = 0;
    my $misc_loaded = 0;

    $i = 0;
    while ($i <= $#lines) {
	$line = $lines[$i];
	if ($line =~ /^#include "cmdline.h"/) {
	    splice @lines, $i, 1;
	    if (!$cmdline_loaded) {
		splice @lines, $i, 0, &load("cmdline.h"), &load("cmdline.c");
	    }
	    $cmdline_loaded = 1;
	} elsif ($line =~ /^#include "bmpwrite.h"/) {
	    splice @lines, $i, 1;
	    if (!$bmpwrite_loaded) {
		splice @lines, $i, 0, &load("bmpwrite.h"), &load("bmpwrite.c");
	    }
	    $bmpwrite_loaded = 1;
	} elsif ($line =~ /^#include "misc.h"/) {
	    splice @lines, $i, 1;
	    if (!$misc_loaded) {
		splice @lines, $i, 0, &load("misc.h");
	    }
	    $misc_loaded = 1;
	} elsif ($line =~ /^#ifndef \S+_H/ ||
	    $line =~ /^#define \S+_H/ ||
	    $line =~ /^#endif \/\* \S+_H \*\//) {
	    splice @lines, $i, 1;
	} else {
	    $i++;
	}
    }

    open OUT, ">$outfile";
    foreach $line (@lines) {
	print OUT $line;
    }
    close OUT;
}
