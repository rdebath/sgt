#!/usr/bin/perl

($dirprefix = $0) =~ s/index\.pl$//;

chdir $dirprefix or die "chdir: $!\n";

$disttar = &getarcname("gonville-r*.tar.gz");
$distzip = &getarcname("gonville-r*.zip");
$srcarc = &getarcname("gonville-r*-src.tar.gz");

open IN, "<", "index.tmpl";
$out = "";

while (<IN>) {
  if (/img.*src=\"([^\"]*)\"/) {
    $a = `identify -format '%w %h %b' $dirprefix$1`;
    chomp $a;
    ($w, $h, $s) = split / /,$a;
    $s = int(($s + 1023) / 1024);
    s/WWW/$w/g;
    s/HHH/$h/g;
    s/SSS/$s/g;
  }
  s/DIST-TARBALL-NAME/$disttar/g;
  s/DIST-ZIPFILE-NAME/$distzip/g;
  s/SRC-ARCHIVE-NAME/$srcarc/g;
  $out .= $_;
}

close IN;

if (open CURR, "<", "index.html") {
  $curr = "";
  $curr .= $_ while <CURR>;
  close CURR;
  exit 0 if $curr eq $out;
}

open OUT, ">", "index.html";
print OUT $out;
close OUT;

sub getarcname {
  my $pattern = shift @_;
  my @names = glob $pattern;
  @names = map { ($a=$_) =~ y/0-9//cd; [ $a, $_ ] } @names;
  @names = sort { $a->[0] <=> $b->[0] } @names;
  return $names[$#names]->[1];
}
