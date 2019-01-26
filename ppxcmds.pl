#!/usr/bin/perl
# (c)TORO
#
	$dest = 'PPXCMDS.C';
	$cmdcnt = 0;

	open(OUT, "> $dest");
	print OUT <<_last;
// �R�}���h�ꗗ(ppxcmds.pl�ɂ�鎩������)
_last

	while (<DATA>) {
		chop;
		$cmd[$cmdcnt++] = uc($_);
	}
	@cmd = sort @cmd;

	print OUT "// ID�ꗗ\n";
	print OUT "#define CID_FILE_EXEC 0 // �t�@�C�������s\n";
	print OUT "#define CID_USERNAME B31 // ���̈ꗗ�ɂȂ��R�}���h\n";
	print OUT "#define CID_MINID 0x100\n";
	print OUT "#define CID_COUNTS $cmdcnt\n";
	for ( $i = 0 ; $i < $cmdcnt ; $i++ ){
		$id = 0x100 + $i;
		print OUT "#define CID_$cmd[$i]\t$id\n";
	}

	print OUT "\n#ifdef DEFCMDNAME\nconst TCHAR *CmdName[] = {\n";
	for ( $i = 0 ; $i < $cmdcnt ; $i++ ){
		NameHash($cmd[$i]);
#		print OUT "\t{\"$cmd[$i]\",CID_$cmd[$i]\t$i},\n";
		print OUT "\tT(\"$cmd[$i]\"), // $hash\n";
	}
	print OUT "};\n#endif\n";

	close(OUT);

sub NameHash
{
	$hash = 0;
	@chrs = unpack("C*",$_[0]);
	for ( @chrs ){
		$hash = (($hash << 6) | ($hash >> (32 - 6)) | $_) & 0xffffffff
	}
	$hash = ($hash | 0x80000000);
	return $hash & 0xffffffff;
}


__END__
closeppx
screensaver
logoff
poweroff
reboot
shutdown
terminate
httpget
cliptext
selectppx
cd
cursor
ppe
edit
file
insert
replace
focus
set
forfile
alias
suspend
hibernate
customize
setcust
linecust
monitoroff
execute
ppc
ppv
ppb
pptray
ppffix
ppcust
freedriveuse
launch
job
tip
help
makedir
makefile
delete
rename
command
emulatekey
jumpentry
commandhash
noime
linemessage
ime
flashwindow
wait
sound
stop
start
string
chopdir
ifmatch
pack
setarchivecp
togglecustword
addhistory
checkupdate
lockpc
trimmark
checksignature
cpu
clearauth
if
nextitem
return
