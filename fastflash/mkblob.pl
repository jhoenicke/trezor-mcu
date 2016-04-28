#!/usr/bin/perl

open BIN, "<../bootloader/bootloader.bin";
$blob = '';
while(<BIN>) {
    $blob .= $_;
}
close BIN;
open BLOB, ">blob.h";
$blob =~ s/./sprintf("0x%02x,", ord($&))/gse;
$blob =~ s/(0x..,){16}/$&\n/g;
print BLOB "static const unsigned char blob[] = {\n${blob}0\n};\n";
close BLOB;
