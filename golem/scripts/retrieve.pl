#!/usr/bin/perl -w
use strict;

# Hacky little script to run golem to retrieve some files, and then merge the
# files with old versions.

# These paths will need to be changed, most likely.
my $dataroot = "/home/richard/Data/monofiles/";  # This needs to match too.

my $username;	# Needs to match that used in retrieve.loew
open(USERNAME, "<$dataroot.username");
$username = <USERNAME>;
close(USERNAME);
$username =~ s/\n$//;

my $script_path = "./";
my $golem_path = "../src/golem";

my $tomono_path = $script_path . "tomono";

# Run golem
print "Retrieving files\n";
system ($golem_path,
	"-C", $script_path . "connect.ssh.el",
	$script_path . "retrieve.loew");

# Process messages into one file
print "Processing files\n";
open(SAVEOUT, ">&STDOUT");
1 if eof(SAVEOUT); # To suppress warning about only one use of SAVEOUT...
open(STDOUT, ">" . $dataroot . "messages/allmessages.tmp." . $username)
	|| die "Can't redirect stdout";
system ($tomono_path,
	"-m",
	$dataroot . "messages/allmessages." . $username,
	$dataroot . "messages/allmessages.new." . $username);
open(STDOUT, ">&SAVEOUT");

# Move messages to their new location, and remove temporary file.
rename($dataroot . "messages/allmessages.tmp." . $username,
       $dataroot . "messages/allmessages." . $username)
       || die "Can't rename messages file\n";
unlink($dataroot . "messages/allmessages.new." . $username)
       || warn "Couldn't delete new messages file after processing\n";

# Process files into one file
my $filename;
my $tmp = `find $dataroot/files/ -name \*.new.ansi`;
$tmp =~ s/\.new\.ansi\n/\n/g;
my @files = split(/\n/, $tmp);

foreach $filename (@files) {
	print "Processing: \"$filename\"\n";
	open(SAVEOUT, ">&STDOUT");
	open(STDOUT, ">" . $filename . ".new.cm")
	        || die "Can't redirect stdout to " .
		       $filename . ".new.cm";
	system ($tomono_path,
	        "-m",
	        $filename . ".cm",
	        $filename . ".new.ansi");
	open(STDOUT, ">&SAVEOUT");

	# Move messages to their new location, and remove temporary file.
	rename($filename . ".new.cm",
	       $filename . ".cm")
	       || die "Can't rename file $filename.new.cm to $filename.cm\n";
	unlink($filename . ".new.ansi")
	       || warn "Couldn't delete new file $filename.new.cm after processing\n";
}

print "Processed files\n";
