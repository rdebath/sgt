#!/usr/bin/perl

# Take a Timber RFC822ish message on stdin, and send it.
#
# Timber RFC822ish messages consist of:
#  - A header section, which may contain Attach and Attach-Enclosed
#    headers. Terminated by a blank line just like proper RFC822. May
#    also contain Queue: headers.
#  - A body section. Terminated by EOF or a line beginning with a ^F.
#  - Attachment sections. Each new one is begun by a ^F line.
#
# Attach and Attach-Enclosed headers look like this:
#
#   Attach[-Enclosed]: word [word...]
#
# The first word of an Attach header is a filename. Subsequent words
# are the same as for an Attach-Enclosed header. (For Attach-Enclosed
# the data comes from an attachment section rather than a file.)
#
# The first word of an Attach-Enclosed header is a MIME type. `text/plain',
# `message/rfc822', `application/octet-stream', whatever. If it contains
# two slashes, the text after the second is taken as the character set.
# Hence `text/plain/iso-8859-1' becomes
# `Content-Type: text/plain; charset=iso-8859-1'.
#
# The second word, if present and non-empty, is the content description
# for the Content-Description: header.
#
# The third word, if present and non-empty, is the content name
# for the `name=' part of the Content-Type header. If not present, the
# `name=' field will be either omitted (for Attach-Enclosed) or filled in
# as the last path component of the source file name (for Attach). If
# present and empty, the `name=' part will be omitted unconditionally.
#
# Word syntax is like shell syntax with just " and \. I.e. any backslashed
# character is literal; double quotes are stripped from around their
# content; double-quoted spaces are literal but unquoted (and unbackslashed)
# spaces separate words.
#
# A Queue: header contains a date and time. Dates are 8-digit with optional
# - / or . separators; times are 4-digit 24-hour with optional colon.
# The message will be queued until the specified time instead of being sent
# immediately.

$headers = "";
$body = "";
@attachments = ();
@aheaders = ();
$queue = 0;

# Read headers
while (<>) {
  chomp;
  last if $_ eq "";  # headers done
  if (/^Attach:/ or /^Attach-Enclosed:/) {
    /^([^:]*):\s+(.*)$/; $htype = $1; @hwords = (); $_ = $2;
    while (/^(([^ \t"\\]|\\.|"([^\\"]|\\.)*")+)((\s+)(.*))?$/) {
      $wd = $1; $_ = $6;
      $wd =~ s/(\\(.)|")/$2/g;
      push @hwords, $wd;
    }
    $a = [$htype, @hwords];
    push @aheaders, $a;
  } elsif (/^Queue:/) {
    $_ .= "!"; # ensure there's spacing around everything
    exit 1 unless /^(.*\D)(\d\d\d\d)[\-\/]?(\d\d)[\-\/]?(\d\d)(\D.*)$/;
    ($year, $month, $day) = ($2, $3, $4);
    $_ = $1 . $5;
    $mon = ('Jan','Feb','Mar','Apr','May','Jun',
            'Jul','Aug','Sep','Oct','Nov','Dec')[$month-1];
    exit 1 unless /\D(\d\d):?(\d\d)\D/;
    ($hour, $min) = ($1, $2);
    $qtime = sprintf "%02d:%02d %s %02d, %04d", $hour, $min, $mon, $day, $year;
    $queue = 1;
  } else {
    $headers .= $_ . "\n";
  }
}

$gotattach = 0;

# Read body
while (<>) {
  $gotattach=1, last if /^\006/;  # attachment begins
  $body .= $_;
}

# Read attachments
while ($gotattach) {
  $attach = "";
  $gotattach = 0;
  while (<>) {
    $gotattach=1, last if /^\006/;  # attachment begins
    $attach .= $_;
  }
  push @attachments, $attach;
}

# If there are no attachment headers at all, we should just spew the
# unchanged message (headers, \n, body) on stdout and leave.
if ($#aheaders < 0) {
  &mail($headers . "\n" . $body);
  exit 0;
}

# Right. Now we are a multipart. First, encode each attachment and
# compile the MIME headers for each part.

# Part one is the body. This gets special treatment: we don't encode it
# at all, and our sole concession to MIMEity is to choose charset and
# transfer encoding to be US-ASCII/7bit or ISO-8859-1/8bit.

$bodyhdrs = "Content-Type: text/plain; charset=CHARSET\n" .
            "Content-Transfer-Encoding: ENCODING\n" .
            "Content-Description: message body\n";
if (&contains_hibit_chars($body)) {
  $bodyhdrs =~ s/CHARSET/iso-8859-1/;
  $bodyhdrs =~ s/ENCODING/8bit/;
} else {
  $bodyhdrs =~ s/CHARSET/us-ascii/;
  $bodyhdrs =~ s/ENCODING/7bit/;
}
push @parts, [$bodyhdrs, $body];

# Remaining parts come from the @aheaders array, in order. For those
# that are Attach: headers, we must read a file to get the data; for
# those that are Attach-Enclosed: headers, we must take from the
# @attachments array.

foreach $a (@aheaders) {
  @a = @$a;
  $htype = shift @a;
  if ($htype eq "Attach") {
    $file = shift @a;
    die "Missing filename in Attach header\n" if $file eq "";
    $defname = $file; $defname =~ s/.*\///g;
    $file =~ s/^(~?)([^\/]*)//;
    if ($1 eq "~") { # tilde-expand
      @f = $2 eq "" ? getpwuid($<) : getpwnam($2);
      $file = $f[7] . $file;
    }
    open F, $file or die "Unable to open attachment file $file\n";
    $data = ""; $data .= $_ while <F>;
    close F;
  } else {
    $data = shift @attachments;
    die "Missing attachment for Attach-Enclosed header\n" if !defined $data;
    $defname = "";
  }
  $ctype = shift @a;
  $ctype = "application/octet-stream" unless defined $ctype;
  $charset = undef;
  if ($ctype =~ /^([^\/]+\/[^\/]+)(\/[^\/]+)/) {
    $ctype = $1;
    $charset = $2;
  }
  $descr = shift @a;
  $name = shift @a;
  $name = $defname unless defined $name;
  $parthdrs = "Content-Type: $ctype";
  $parthdrs .= "; charset=$charset" if $charset ne "";
  $parthdrs .= "; name=$name" if $name ne "";
  $parthdrs .= "\n";
  $parthdrs .= "Content-Description: $descr\n" if $descr ne "";
  if (&needsencode($data)) {
    $data = &canonlf($data) if $ctype =~ /^(text|message)\//;
    $data = &b64($data);
    $enc = "base64";
  } else {
    $enc = "7bit";
  }
  $parthdrs .= "Content-Transfer-Encoding: $enc\n";
  push @parts, [$parthdrs, $data];
}

# Now we have the final transit form of each attachment, invent a
# separator line that doesn't appear in any of the attachments.
$basesep = $sep = "MIME-Separator-Line";
$suffix = 'AAAAAAAAA';
while (&present($sep, @parts)) {
  $sep = $basesep . "-" . $suffix++;
}

# And now we have _that_, we can build the MIME headers for the
# containing message, and output the whole thing.
$exthdrs = "MIME-Version: 1.0\n" .
           "Content-Type: multipart/mixed; boundary=\"$sep\"\n";

$mail = $headers . $exthdrs . "\n";
$mail .= "  This message is in MIME format. If your mailer does not\n";
$mail .= "  support MIME, you may have trouble reading the attachments.\n";
foreach $i (@parts) {
  $mail .= "\n--$sep\n";
  $mail .= $i->[0];
  $mail .= "\n";
  $mail .= $i->[1];
}
$mail .= "\n--$sep--\n";

&mail($mail);

# and done!
exit 0;

sub needsencode {
  my ($data) = @_;
  return 1 if &contains_hibit_chars($data); # hibits need encoding
  return 1 if $data =~ /\nFrom /s || $data =~ /^From /s;
  return 1 if $data =~ /[^\n]{78}/s; # lines too long
  return 1 if $data =~ /\s\n/s || $data =~ /\s$/s; # trailing space
}

sub contains_hibit_chars {
  my ($data) = @_;
  $data =~ y/\0-\177//d;
  return length $data != 0;
}

sub b64 {
  my ($data) = @_;
  my $a, $b, $c, $d, $frag, $dashes, @frag, $i;
  my $col = 0;
  my $out = "";
  while (length $data > 0) {
    $dlast = $data;
    ($a, $b, $c, $data) = unpack "C3 a*", $data;
    $i = ($a << 16) | ($b << 8) | ($c);
    $frag = pack "C*", (($i >> 18) & 0x3F, ($i >> 12) & 0x3F,
                        ($i >> 6) & 0x3F, ($i) & 0x3F);
    $frag =~ y/\0-\77/A-Za-z0-9+\//;
    if ($col >= 60) { $out .= "\n"; $col = 0; }
    $out .= $frag; $col += 4;
  }
  $dashes = (length $dlast < 3 ? 3 - length $dlast : 0);
  substr($out, -$dashes) = "=" x $dashes if $dashes;
  return $out . "\n";
}

sub present {
  my ($sep, @parts) = @_;
  my $part, $data;
  foreach $part (@parts) {
    $data = "\n" . $part->[1];
    return 1 if $data =~ /\n--$sep/;
  }
  return 0;
}

sub canonlf {
  my ($data) = @_;
  $data =~ s/\n/\r\n/gs;
  $data;
}

sub mail {
  my ($msg) = @_;
  $msg .= "\n" unless substr($msg, -1) eq "\n";
  if ($queue) {
    $sep = 'AAAAAAAAA';
    while (&present($sep, ["", $msg])) { $sep++; }
    open OUTPUT, "|at $qtime";
    print OUTPUT "/usr/lib/sendmail -oem -t -oi << --$sep\n";
    print OUTPUT "${msg}--$sep\n";
    close OUTPUT;
  } else {
    open OUTPUT, "|/usr/lib/sendmail -oem -t -oi";
    print OUTPUT "$msg";
    close OUTPUT;
  }
}
