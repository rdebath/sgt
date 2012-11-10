#!/usr/bin/perl

# Make a C source file containing a const byte array corresponding to
# a given data file.
#
# Usage: mkarray.pl infile symbolname
#
# outputs an array called symbolname and a size_t called
# symbolname_len.

die "usage: mkarray.pl infile symbolname\n" unless @ARGV == 2;

open DATA, "<", $ARGV[0] or die "$ARGV[0]: open: $!\n";
binmode DATA;
1 while read DATA, $data, 16384, length $data;
close DATA;

$datalen = length $data;
$data .= "\0" x ((-$datalen) & 7);
@data = unpack "C*", $data;

$sym = $ARGV[1];

print "#include <stddef.h>\n"; # for size_t
print "union aligned { unsigned char x[8]; unsigned long long a; };\n";
print "const union aligned ${sym}[] = {\n";
$line = "";
while (@data) {
    printf "{%s},\n", join ",", map { sprintf "0x%02x", $_ } splice @data,0,8;
}
printf "};\nconst size_t ${sym}_len = %d;\n", $datalen;
