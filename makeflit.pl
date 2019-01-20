#!/usr/bin/perl
	while(<>){
		if ( $_ =~ /^make ver/i ){ next; }
# BCC
		if ( $_ =~ /^Borland / ){ next; }
		if ( $_ =~ /^Copyright / ){ next; }
		if ( $_ =~ /^Loaded / ){ next; }
		if ( $_ =~ /^Warning (\S+) (\d+): (.*)/ ){
			if ( $3 =~ /^Restarting compile/ ){
				next;
			}
		}
# CL
		if ( $_ =~ /^Microsoft / ){ next; }
		print $_;
	}
