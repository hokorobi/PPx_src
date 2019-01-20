#!/usr/bin/perl
#
	$src = 'PPCOMMON.H';
	$dest = 'pplib.dll.spec';
	$count = 1;

	if ( $ARGV[0] ne '' ){
		$src = $ARGV[0];
	}

	open(IN, "< $src");
	open(OUT, "> $dest");

	while (<IN>) {
		chop;

		if ( $_ =~ /PPXDLL .*\s(\S+)\(/ ){
			print OUT "$count stdcall $1()\n";
			$count++;
		}
	}
	close(OUT);
	close(IN);
