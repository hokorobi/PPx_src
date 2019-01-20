#!/usr/bin/perl
use Time::Local;

	$verstr = $ARGV[0];

	if ( $verstr =~ /(\d)(\d)(\d)p(\d*)/i ){
		$ver1 = $1;
		$ver2 = $2;
		$ver3 = $3;
		$ver4 = $4;
		$propver = "$ver1.$ver2$ver3+$ver4";
		$docname = "PPX$ver1$ver2$ver3"."P$ver4.TXT";
		if ( !-w $docname ){ $docname = "PPX.TXT"; }
	}elsif ( $verstr =~ /(\d)(\d)(\d)/ ){
		$ver1 = $1;
		$ver2 = $2;
		$ver3 = $3;
		$ver4 = '0';
		$propver = "$ver1.$ver2$ver3";
		$docname = "PPX.TXT";
	}
	($sec,$min,$hour,$mday,$mon,$year,$wday) = localtime(time);
	$year += 1900;

	open(OUT, "> PPXVER.H");
	print OUT <<_last;
//
// PPx Version (ppxver.pl‚É‚æ‚éŽ©“®¶¬)
//
#ifndef WS_CAPTION // for resource include
#include <windows.h>
#include <winuser.h>
#endif

#define VersionH		$ver1
#define VersionM		$ver2
#define VersionL		$ver3
#define VersionP		$ver4
#define FileProp_Version	"$propver"		// for Propaty sheet
#define FileCfg_Version	"$ver1.$ver2$ver3"	// for CFGver
#define X_Version		"$ver1.$ver2.$ver3.$ver4"	// for XML
#define Q_Version		$ver1,$ver2,$ver3,$ver4	// for Quick view
#define COM_ver			Q_Version	// Common
#define BUI_ver			Q_Version	// bUI
#define CUI_ver			Q_Version	// cUI
#define VUI_ver			Q_Version	// vUI
#define ReleaseYear		"$year"
_last
	close(OUT);

	($sec,$min,$hours,$mday,$mon,$year) = localtime(time);
	if ( $ver2 >= 6 ){ $ver2 = 5; }
	$fixtime = timelocal($ver4,$ver2 * 10 + $ver3,$ver1,$mday,$mon,$year);
	utime($fixtime,$fixtime,$docname);
