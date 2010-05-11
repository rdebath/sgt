#!/usr/bin/perl 

open IN, "index.tmpl";
open OUT, ">index.html";

while (<IN>) {
    s/<poly name="([^\"]+)">/&dopoly($1)/eg;
    s/<image name="([^\"]+)">/&doimage($1)/eg;
    print OUT $_;
}

close IN;
close OUT;

sub dopoly($) {
    my $name = shift @_;
    chomp($desc = `cat ${name}.desc`);
    $img = &doimage($name); # HACK HACK HACK: sets globals $x and $y
    return "<span class=\"polyhedron\" width=\"$x\" height=\"$y\" ".
      "data=\"$desc\">\n" .
      "$img\n" .
      "</span>";
}

sub doimage($) {
    my $name = shift @_;
    $image = "${name}.png";
    $k = `identify $image`;
    $k =~ /PNG (\d+)x(\d+)/ and do { $x = $1; $y = $2; };
    @st = stat $image;
    $alt = sprintf "[%dx%d greyscale PNG, %dKB]",
      $x, $y, int(($st[7] + 1023) / 1024);
    return "<a href=\"$image\"><img src=\"$image\" alt=\"$alt\" ".
      "width=\"$x\" height=\"$y\" border=\"0\"></a>";
}
