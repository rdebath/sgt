#!/usr/bin/perl

use Fcntl;

$inbox = $ENV{"MAIL"};
$inbox = "/var/spool/mail/" . ((getpwuid($<))[0]) unless $inbox;

open(INBOX,$inbox) or exit 1;
open(MBOX,">>$ARGV[0]");

flock(INBOX, LOCK_EX);
print MBOX $_ while (<INBOX>);
seek(INBOX,0,0);
open(INBOX2,">$inbox");
close(INBOX2);
flock(INBOX, LOCK_UN);

close INBOX;
close MBOX;
