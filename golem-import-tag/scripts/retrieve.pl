#!/usr/bin/perl -w
use strict;

# Hacky little script to run golem to retrieve some files, and then merge the
# files with old versions.

# These paths will need to be changed, most likely.
my $username = "l";	# Needs to match that in retrieve.loew
my $dataroot = "/home/richard/Data/monofiles/";  # This needs to match too.
my $script_path = "./";
my $golem_path = "../src/golem";

my $tomono_path = $script_path . "tomono";

# Run golem
system ($golem_path,
	"-C", $script_path . "connect.ssh.el",
	$script_path . "retrieve.loew")
or die "Error running golem: $!\n";

open(SAVEOUT, ">&STDOUT");
1 if eof(SAVEOUT); # To suppress warning about only one use of SAVEOUT...
open(STDOUT, ">" . $dataroot . "messages/allmessages.tmp." . $username)
	|| die "Can't redirect stdout";
system ($tomono_path,
	"-m",
	$dataroot . "messages/allmessages." . $username,
	$dataroot . "messages/allmessages.new." . $username);
open(STDOUT, ">&SAVEOUT");

rename($dataroot . "messages/allmessages.tmp." . $username,
       $dataroot . "messages/allmessages." . $username)
       || die "Can't rename messages file\n";
print "Hello\n";
