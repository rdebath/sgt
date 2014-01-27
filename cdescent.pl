#!/usr/bin/perl

# Postprocess the HTML output from running Halibut on cdescent.but so
# as to convert paragraphs starting MEMDIAGRAM into illustrative
# diagrams of the memory layout of common C constructss.

use strict;
use warnings;

my $diag_index = 0;

while (<>) {
    if (/^MEMDIAGRAM:(\d+)\.\.(\d+) ?(.*)$/) {
        my $extras = $3;
        my $first = $1;
        my $last = $2;
        my $left = $first - 0.75 - 0.1;
        my $right = $last + 1 + 0.75 + 0.1;
        my $bottom = -0.1;
        my $top = 1.4;
        my $ps = "";
        $ps .= sprintf "/Helvetica findfont 0.3 scalefont setfont ";
        $ps .= sprintf "0.02 setlinewidth ";
        $ps .= sprintf "newpath %d 0 moveto -0.5 0 rlineto 0.25 0.25 rlineto -0.5 0.5 rlineto 0.25 0.25 rlineto 0.5 0 rlineto closepath gsave 0.9 setgray fill grestore stroke ", $first;
        for my $x ($first..$last) {
            $ps .= sprintf "newpath %d 0 moveto 1 0 rlineto 0 1 rlineto -1 0 rlineto ", $x;
            $ps .= sprintf "closepath gsave 0.9 setgray fill grestore stroke ";
            $ps .= sprintf "%f 1.1 moveto (%d) dup stringwidth pop 2 div neg 0 rmoveto show ", $x + 0.5, $x;
        }
        $ps .= sprintf "newpath %d 0 moveto 0.5 0 rlineto 0.25 0.25 rlineto -0.5 0.5 rlineto 0.25 0.25 rlineto -0.5 0 rlineto closepath gsave 0.9 setgray fill grestore stroke ", $last+1;

        $ps .= sprintf "/Helvetica findfont 0.5 scalefont setfont ";
        $ps .= sprintf "newpath 0 0 moveto (0123456789) true charpath flattenpath pathbbox 2 index add 2 div neg /ydisp exch def pop pop pop\n";

        while ($extras =~ s/(\d+)\.\.(\d+)([_^]?)\(([^\)]*)\) *//) {
            my $first = $1;
            my $last = $2;
            my $middle = ($first + $last+1) / 2.0;
            my $pos = $3;
            my $str = $4;
            if ($pos eq "") {
                $ps .= sprintf "newpath %d 0 moveto %d 0 lineto %d 1 lineto %d 1 lineto ", $first, $last+1, $last+1, $first;
                $ps .= sprintf "closepath gsave 1 setgray fill grestore stroke ";
                $ps .= sprintf "%f 0.5 ydisp add moveto (%s) dup stringwidth pop 2 div neg 0 rmoveto show ", ($first+$last+1)/2.0, $str;
            } elsif ($pos eq "_") {
                $bottom = -0.8;
                $ps .= sprintf "newpath %f -0.1 moveto %f -0.5 lineto %f -0.5 lineto %f -0.1 lineto stroke ", $first+0.1, $first+0.1, $last+1-0.1, $last+1-0.1;
                $ps .= sprintf "(%s) stringwidth pop 2 div dup %f exch sub -0.4 moveto 0 -0.2 rlineto %f add -0.6 lineto 0 0.2 rlineto gsave 1 setgray fill grestore ", $str, $middle-0.2, $middle+0.2;
                $ps .= sprintf "%f -0.5 ydisp add moveto (%s) dup stringwidth pop 2 div neg 0 rmoveto show ", ($first+$last+1)/2.0, $str;
            } elsif ($pos eq "^") {
                $bottom = -0.8;
                $ps .= sprintf "newpath %f -0.1 moveto -0.075 -0.25 rlineto 0.15 0 rlineto closepath fill ", $first+0.5;
                $ps .= sprintf "newpath %f -0.2 moveto %f -0.5 lineto %f -0.5 lineto stroke ", $first+0.5, $first+0.5, $first+1;
                $ps .= sprintf "%f -0.5 ydisp add moveto (%s) show ", $first+1.2, $str;
            }
        }

        $ps .= sprintf "showpage\n";
        $ps = sprintf "%f %f translate %s", -$left, -$bottom, $ps;
        my $width = $right - $left;
        my $height = $top - $bottom;
        my $hires = 96;
        my $hiwidth = int($width * $hires);
        my $hiheight = int($height * $hires);
        my $lores = 32;
        my $lowidth = int($width * $lores);
        my $loheight = int($height * $lores);
        my $filename = sprintf "diagram-%d.png", $diag_index++;
        my $filepath = "cdescent/$filename";
        open my $gs, "|-", "gs -sDEVICE=pnggray -sOutputFile=- -r@{[$hires*72]} -g${hiwidth}x${hiheight} -dBATCH -dNOPAUSE -q - | convert -resize ${lowidth}x${loheight} png:- $filepath";
        print $gs $ps;
        close $gs;
        print "<img src=\"$filename\">\n";
    } else {
        print;
    }
}
