#!/usr/bin/perl
# (c)TORO
#
# メッセージ対応チェック
	open(IN, "< PPX.H");
	while (<IN>) {
		chop;

		while ( $_ =~ /MES_(\w\w\w\w)\t/g ){ $table{$1} = 1; }
	}
	close(IN);

	$check = 0;
	open(IN, "< resource/DEFCUST.CFG");
	while (<IN>) {
		chop;

		if ( $_ =~ /Mes0411/ ){
			$check = 1;
		}elsif ( $check ){
			if ( $_ =~ /^(\+\d\.\d\d\|)?(\w\w\w\w)/ ){
				if ( $table{$2} == 1 ){
					$table{$2} = 2;
				}elsif ( $table{$2} == 2 ){
					print "$2 : dup\n";
				}else{
					if ( $2 !~ /\d\w\w\w/ ){
						if ( $2 !~ /S[ORT]\w\w/ ){
							print "$2 : unuse\n";
						}
					}
				}
			}
		}
	}
	close(IN);

	while (($name,$value) = each(%table)) {
		if ( $value == 1 ){
			print "$name : undefine\n";
		}
	}

# map 整形
	opendir(handle,'.');
	@filelist = readdir(handle);
	closedir(handle);
	foreach $name (@filelist) {
		if ( $name =~ /\.map$/ ){
			$mode = 0;
			$map = "";
			open(IN, "< $name");
			while (<IN>) {
				chop;
				if ( $_ =~ /Publics by Name/ ){ $mode = 1; }
				if ( $mode && ($_ =~ /Publics by Value/) ){ $mode = 2; }
				if ( $mode != 1 ){ $map .= "$_\n"; }
			}
			close(IN);
			if ( $mode ){
				open(OUT, "> $name");
				print OUT $map;
				close(OUT);
			}
		}
	}
