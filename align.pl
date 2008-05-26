#!/usr/bin/perl 

# Adjust an ELF object file by setting the alignment field in its
# data section(s) to 4 and making them constant. Irritatingly,
# objcopy appears unable to do this, so I have to do it by hand.

open OBJ, "+<", $ARGV[0] or die "$ARGV[0]: $!\n";

read OBJ, $hdr, 52;
($ident,$type,$mach,$ver,$entry,$phoff,$shoff,$flags,$ehsize,
    $phentsize,$phnum,$shentsize,$shnum,$shstrndx) =
  unpack "a16vvVVVVVvvvvvv", $hdr;

seek OBJ, $shoff, 0;
for ($sect = 0; $sect < $shnum; $sect++) {
    read OBJ, $shent, $shentsize;
    ($sh_name, $sh_type, $sh_flags, $sh_addr, $sh_offset,
	$sh_size, $sh_link, $sh_info, $sh_addralign, $sh_entsize) =
      unpack "VVVVVVVVVV", $shent;

    if ($sh_type == 1 && ($sh_flags & 6) == 2) {
	# SHT_PROGBITS; SHF_ALLOC, !SHF_EXECINSTR. Data section.
	$sh_addralign = 4 if $sh_addralign < 4;
	$sh_flags &= ~1; # clear SHF_WRITE
	seek OBJ, -$shentsize, 1;
	$shent = pack "VVVVVVVVVV",
	  ($sh_name, $sh_type, $sh_flags, $sh_addr, $sh_offset,
	      $sh_size, $sh_link, $sh_info, $sh_addralign, $sh_entsize);
	die "unexpected e_shentsize $shentsize\n"
	  unless $shentsize == length $shent;
	print OBJ $shent;
    }
}
