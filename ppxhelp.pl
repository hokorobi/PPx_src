#!/usr/bin/perl
# Win Help 用簡易コンバータ (c)TORO
#
	$helpname = 'PPX';
	$src = 'xhelp.txt';
	$lines = 0;
	$myurl = 'http://toro.d.dooo.jp';

	($sec,$min,$hour,$mday,$mon,$copyyear,$wday) = localtime(time);
	$copyyear += 1900;

	$html = $ARGV[0];

	if ( $html ){
		$dest = $helpname.'help.html';
		$cnt = $helpname.'index.html';

		$tag_tail = '<br>';
		$tag_ul = "<hr>";
		$dest =~ tr/A-Z/a-z/;
		$cnt =~ tr/A-Z/a-z/;
		$tabmode = '&nbsp;&nbsp;';
	}else{
		$dest = $helpname.'temp.rtf';
		$cnt = $helpname.'.CNT';

		$tag_tail = '\par';
		$tag_ul = "\\pard\\brdrb\\brdrs\\par\\pard";
	}

	$indexcnt = 1;
	$titlecnt = 1;
	$anchorcnt = 65536;
#   $anchor{'keyword'} アンカーと ID の対応表
	$tail = "\n";
# 1 ならポップアップ用
	$popmode = 0;
# 1 なら一覧の下に書くコメントを保存する
	$grouptitlememomode = 0;

# ID→数値 変換テーブルを作成
	if ( $html ){
		open(IN, "< $helpname.rh");
		while (<IN>) {
			chop;

			if ( $_ =~ /^#define[\s\t]*([\S]*)[\s\t]*([\S]*)/ ){
				$helpid{$1} = eval($2);
			}
		}
		close(IN);
	}else{ # CustIDチェック用テーブルを作成
		open(IN, "< PPD_CSTD.C");
		while (<IN>) {
			chop;

			while ( $_ =~ /([A-Z]+\_[0-9A-Za-z]+)/g ){
				$CustID{$1} = 1;
			}
		}
		close(IN);
	}

	open(IN, "< $src");
	open(OUT, "> $dest");
	open(OUTCNT, "> $cnt");

	if ( $html ){
		print OUT <<_last;
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd"><html lang="ja">
<head><meta http-equiv="Content-Type" content="text/html; charset=Shift_JIS">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style type="text/css"><!--
hr { border:solid 1px; }
pre { font-size:100%; }
img { max-width:100%; height:auto; }
--></style>
<title>PPx help</title></head>
<body bgcolor="#eeee99" text="#000000" link="#0000CC" vlink="#8800cc" alink="#440088">
_last

		print OUTCNT <<_last;
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd"><html lang="ja">
<head><meta http-equiv="Content-Type" content="text/html; charset=Shift_JIS">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>index</title></head>
<body bgcolor="#eeee99" text="#000000" link="#0000CC" vlink="#8800cc" alink="#440088">
_last
	}else{
		print OUT <<_last;
{\\rtf\\deff1
{\\fonttbl{\\f0\\froman\\fcharset128\\fprq1 明朝{\\*\\falt ＭＳ 明朝};}
{\\f1\\froman\\fcharset128\\fprq1 ＭＳ Ｐゴシック;}
{\\f2\\froman\\fcharset128\\fprq1 ＭＳ Ｐ明朝;}
{\\f3\\fmodern\\fcharset128\\fprq1 ＭＳ ゴシック;}
{\\f4\\fmodern\\fcharset128\\fprq1 ＭＳ 明朝;}
{\\f5\\fnil\\fcharset2\\fprq2 Wingdings;}
{\\f6\\froman\\fcharset0\\fprq2 Century;}
{\\f7\\froman\\fcharset0\\fprq3 Arial;}
{\\f8\\froman\\fcharset0\\fprq3 Courier New;}
{\\f9\\froman\\fcharset0\\fprq3 Times New Roman;}}\\fs22
{\\colortbl;
\\red000\\green000\\blue000;\\red128\\green128\\blue128;\\red175\\green175\\blue175;
\\red255\\green000\\blue000;\\red128\\green000\\blue000;\\red000\\green255\\blue000;
\\red000\\green128\\blue000;\\red000\\green000\\blue255;\\red000\\green000\\blue128;
\\red255\\green255\\blue000;\\red128\\green128\\blue000;\\red000\\green255\\blue255;
\\red000\\green128\\blue128;\\red255\\green000\\blue255;\\red128\\green000\\blue128;
\\red255\\green255\\blue255;}
_last
		print OUTCNT ":Base ".$helpname.".HLP\n";
	}

	while (<IN>) {
		chop;
		$lines++;

		if ( $html ){
			$_ =~ s/\\\{/\{/g;
			$_ =~ s/\\\}/\}/g;
			$_ =~ s/\\\\/\\/g;
			$_ =~ s/<blue>/<b>/g;
			$_ =~ s/<\/blue>/<\/b>/g;
			$_ =~ s/&/&amp;/g;
			$_ =~ s/"/&quot;/g;
			$_ =~ s/< /&lt;/g;
			$_ =~ s/ >/&gt;/g;
		}else{
			$_ =~ s/<blue>/\{\\cf8 /g;
			$_ =~ s/<\/blue>/\}/g;
			$retry = 1;
			while ( $retry ){
				$retry = 0;
				if ( $_ =~ /<u>/ ){
					if ( $ulcount ){printf("%d : '<u>' over written from %d\n",$lines,$ulcount);}
					$ulcount = $lines;
					$_ =~ s/<u>/{\\ul /;
					$retry = 1;
				}
				if ( $_ =~ /<\/u>/ ){
					if ( !$ulcount ){printf("%d : '<\/u>' over written\n",$lines);}
					$ulcount = 0;
					$_ =~ s/<\/u>/\}/;
					$retry = 1;
				}
				if ( $_ =~ /<b>/ ){
					if ( $boldcount ){printf("%d : '<b>' over written from %d\n",$lines,$boldcount);}
					$boldcount = $lines;
					$_ =~ s/<b>/{\\b /;
					$retry = 1;
				}
				if ( $_ =~ /<\/b>/ ){
					if ( !$boldcount){printf("%d : '<\/b>' over written\n",$lines);}
					$boldcount = 0;
					$_ =~ s/<\/b>/\}/;
					$retry = 1;
				}
				if ( $_ =~ /<i>/ ){
					if ( $itccount ){printf("%d : '<i>' over written from %d\n",$lines,$itccount);}
					$itccount = $lines;
					$_ =~ s/<i>/{\\i /;
					$retry = 1;
				}
				if ( $_ =~ /<\/i>/ ){
					if ( !$itccount ){printf("%d : '<\/i>' over written\n",$lines);}
					$itccount = 0;
					$_ =~ s/<\/i>/\}/;
					$retry = 1;
				}
			}
			$_ =~ s/< /</g;
			$_ =~ s/ >/>/g;
		}

		# 行末の処理方法 :tail=\par 本文
		if ( $_ =~ /^\:tail=(.*)/ ){
			$tail = $tag_tail."\n";
			next;
		}
		# :popmode ポップアップ用データ開始位置
		if ( $_ =~ /:popmode/ ){
			if ( $html ){
				last;
			}
			$popmode = 1;
			next;
		}

		if ( $_ =~ /<grouptitlememo>/ ){
			CheckTermTag();
			$grouptitlememo = $tail;
			$grouptitlememomode = 1;
			next;
		}
		# <grouptitle:title> グループ一覧
		if ( $_ =~ /<grouptitle:([^>]*)>/ ){
			$grouptitlememomode = 0;
			DumpGroupItem();
			$tab = 2000;

			if ( $html ){
				print OUT "<a href=\"#$anchorcnt\">$1<\/a>$tail";
				$grouplist .= "$tag_ul$tail\n".
						"<a name=\"$anchorcnt\"><b>$1<\/b><\/a>$tail";
				$groupitem .= "$tag_ul$tail\n"."<b>$1<\/b>$tail";
			}else{
				print OUT "\{\\uldb $1\}\{\\v $anchorcnt\}$tail\n";
				$grouplist .= "$tag_ul$tail\n".
						"#\{\\footnote  $anchorcnt\}\{\\b $1\}$tail$tail\n".
						"\\pard \\li$tab\\fi-$tab\\tx$tab";
				$groupitem .= "$tag_ul$tail\n"."\{\\b $1\}$tail\n";
			}
			$anchorcnt++;
			next;
		}
		# <group:name::title> グループアイテム(二種表記)
		if ( $_ =~ /<group:(.*)::(.*)>/ ){
			DumpGroupItem();
			$groupitem1 = $1;
			$groupitem2 = $2;
			$groupitemA = "";
			next;
		}
		if ( $_ =~ /<groupa:(.*)::(.*)::(.*)>/ ){
			DumpGroupItem();
			$groupitem1 = $1;
			$groupitem2 = $2;
			$groupitemA = $3;
			next;
		}
		# <group:title> グループアイテム
		if ( $_ =~ /<group:(.*)>/ ){
			DumpGroupItem();
			if ( $1 eq "dump" ){
				print OUT $grouptitlememo,$grouplist,"\n",$groupitem,"\n";
				$grouplist = "";
				$groupitem = "";
				$grouptitlememomode = 0;
				$grouptitlememo = "";
			}else{
				$groupitem1 = $1;
				$groupitem2 = "";
				$groupitemA = "";
			}
			next;
		}
		if ( $_ =~ /<groupa:(.*)::(.*)>/ ){
			DumpGroupItem();
			$groupitem1 = $1;
			$groupitem2 = "";
			$groupitemA = $2;
			next;
		}

		# <a:keyword> アンカー生成
		while ( $_ =~ /<a:([^>]*)>/ ){
			if ( $anchorset{$1} ){
				printf("%d : '<a:%s>' red(%d)\n",$lines,$1,$anchorset{$1});
			}else{
				$anchorset{$1} = $lines;
			}
			if ( $anchor{$1} < 1 ){
				$anchor{$1} = $anchorcnt++;
			}
			if ( $html ){
				$_ =~ s/<a:([^>]*)>/<a name="$1"><\/a>/;
			}else{
				$_ =~ s/<a:([^>]*)>/#\{\\footnote  $anchor{$1}\}/;
			}
		}
		# <atk:keyword> アンカー&タイトル生成
		if ( $_ =~ /<atk:([^>]*)>/ ){
			if ( $anchorset{$1} ){
				printf("%d : '<jmpa:%s>' red(%d)\n",$lines,$1,$anchorset{$1});
			}else{
				$anchorset{$1} = $lines;
			}
			if ( $anchor{$1} < 1 ){
				$anchor{$1} = $anchorcnt++;
			}
			if ( $html ){
				$_ =~ s/<atk:([^>]*)>/<a name="$1"><b>$1<\/b><\/a>/;
			}else{
				$_ =~ s/<atk:([^>]*)>/#\{\\footnote  $anchor{$1}\} K\{\\footnote  $1\}{\\b $1}/;
			}
		}
		# <jmpa:string&keyword> アンカーへのリンク
		while ( $_ =~ /<jmpa:([^>:]*)>/ ){
			if ( $anchor{$1} < 1 ){ $anchor{$1} = $anchorcnt++; }
			if ( $html ){
				$_ =~ s/<jmpa:([^>:]*)>/<a href="#$1">$1<\/a>/;
			}else{
				$_ =~ s/<jmpa:([^>:]*)>/\{\\uldb $1\}\{\\v $anchor{$1}\}/;
			}
		}
		# <jmpa:string:keyword> アンカーへのリンク
		while ( $_ =~ /<jmpa:([^>:]*):([^>]*)>/ ){
			if ( $anchor{$2} < 1 ){ $anchor{$2} = $anchorcnt++; }
			if ( $html ){
				$_ =~ s/<jmpa:([^>:]*):([^>]*)>/<a href="#$2">$1<\/a>/;
			}else{
				$_ =~ s/<jmpa:([^>:]*):([^>]*)>/\{\\uldb $1\}\{\\v $anchor{$2}\}/;
			}
		}
		# <img:filepath> 画像
		if ( $_ =~ /<img:([^>]*)>/ ){
			if ( $html ){
				print OUT "<img src=\"./$1\"><br>";
			}
			next;
		}
		# <myurl>
		$_ =~ s/<myurl>/$myurl/g;
		# <copyyear>
		$_ =~ s/<copyyear>/$copyyear/g;

		# .n=title 新しいブックとそのタイトル
		if ( $_ =~ /^\.(\d*)\=(.*)/ ){
			if ( $html ){
				print OUTCNT "<b>$2<\/b><br>\n";
			}else{
				print OUTCNT "$1 $2\n";
			}
			$titlestr[$1] = $2;
			next;
		}
		# .n:title[=id] 新しいページとそのタイトル
		if ( $_ =~ /^\.(\d*)\:(.*)/ ){
			$depth = $1;
			$name = $2;
			if ( $2 =~ /^(.*)\=(.*)/ ){
				$name = $1;
				$id = $2;
			}else{
				$id = sprintf("ID%03d",$titlecnt++);
			}
			if ( $html ){
				print OUTCNT "<a href=\"$dest#$id\" target=\"main\">$name<\/a><br>\n";
			}else{
				print OUTCNT "$depth $name=$id\n";
			}
			$titlestr[$depth] = $name;
			$title = $titlestr[1];
			for ( $i = 2 ; $i <= $depth ; $i++ ){
				$title = $title." - ".$titlestr[$i];
			}
			if ( $html ){
				$_ = "<a name=\"$id\"></a><a name=\"$name\"></a><h3>$title</h3>";
			}else{
				$_ = '#{\\footnote  '.$id.'}${\\footnote  '.$name.
					'}K{\\footnote  '.$name.'}+{\\footnote 0}\\pard{\\b '.
					$title."}\n\\sl280\\par";
			}
		}
		# +footnote の連番処理
		if ( $_ =~ /\+\{\\footnote .*\}/ ){
			$id = sprintf("index:%03d",$indexcnt++);
			if ( $html ){
				$_ = "";
			}else{
				$_ =~ s/\+\{\\footnote [0-9A-Za-z\:]\}/\+\{\\footnote $id\}/g;
			}
		}

		if ( $html ){
			if ( $_ =~ /\\page/ ){
				CheckTermTag();
				$_ =~ s/\\page/<hr>/g;
			}
			$_ =~ s/<key:([^>]*)>/<a name="$1"><\/a>/g;			# <key:keyword>
			$_ =~ s/<tk:([^>]*)>/<a name="$1"><b>$1<\/b><\/a>/g;	# <tk:title>
			while ( $_ =~ /<hid:([^>]*)>/ ){				# <hid:ResourceId>
				$hid = $1;
				if ( $helpid{$hid} ne '' ){ $hid = $helpid{$hid};}
				$_ =~ s/<hid:([^>]*)>/<a name="$hid"><\/a>/;
			}
			if ( $_ =~ /<tb:0>/ ){							# <tb:0> 列挙終了
				if ( !$listmode ){printf("%d : '<tb:0>' over written\n",$lines);}
				$listmode = 0;
				$_ =~ s/^<tb:0>/<\/dt><\/dl>/g;
				$_ =~ s/<tb:0>/<\/dl>/g;
				$tabcode='&nbsp;&nbsp;';
				$tail="<br>\n";
			}
			if ( $_ =~ /<tb:[^>]*>/ ){						# <tb:n> 列挙開始
				if ( $listmode ){printf("%d : '<tb:>' over written from %d\n",$lines,$listmode);}
				$listmode = $lines;
				$_ =~ s/<tb:[^>]*>/<dl compact>/g;
				$tabcode = '</dt><dd>';
				$tail = '';
			}
			if ( $listmode ){
				if ( $_ =~ /\t/ ){
					$_ =~ s/\t/<\/dt><dd>/;		# tab(最初のみ)
					$_ =~ s/\t/<\/dd><dd>/g;	# tab(２つ目以降)
					if ( $_ ne '' ){
						$_ .= "</dd>\n<dt>";
					}else{
						$_ .= "</dt>\n<dt>";
					}
				}else{
					$_ .= "</dt>\n<dt>";
				}
				$_ =~ s/<dl compact><\/dd>/<dl compact>/g;
				$_ =~ s/<dl compact><\/dt>/<dl compact>/g;
			}else{
				$_ =~ s/\t/$tabcode/g;								# tab
			}

			# nothing											# <hr>
			if ( $_ =~ /<pre>/ ){
				if ( $precount ){printf("%d : '<pre>' over written from %d\n",$lines,$precount);}
				$precount = $lines;
				$tail = "\n";
			}
			if ( $_ =~ /<\/pre>/ ){
				if ( !$precount ){printf("%d : '</pre>' over written\n",$lines);}
				$precount = 0;
				$tail = "<br>\n";
			}
			$_ =~ s/<http:([^>]*)>/<a href="http:$1" target="_top">http:$1<\/a>/g;
			$_ =~ s/<https:([^>]*)>/<a href="https:$1" target="_top">https:$1<\/a>/g;
			$_ =~ s/<mailto:([^>]*)>/<a href="mailto:$1"><i>$1<\/i><\/a>/g;
			$_ =~ s/<url:([^>]*)>/<a href="$1">$1<\/a>/g;
			$_ =~ s/<copyright:([^>]*)>/<i>$1<\/i>/g;
			$_ =~ s/<windowskey>/<font face="Wingdings">&#255;<\/font>/g;
		}else{
			$_ =~ s/<key:([^>]*)>/K\{\\footnote  $1\}/g;		# <key:keyword>
			$_ =~ s/<tk:([^>]*)>/K\{\\footnote  $1\}{\\b $1}/g;	# <tk:title>
			$_ =~ s/<hid:([^>]*)>/#\{\\footnote  $1\}/g;	# <hid:ResourceId>

			$_ =~ s/<tb:0>/\\pard /g;							# <tb:0>
			$_ =~ s/<tb:([^>]*)>/\\pard \\li$1\\fi-$1\\tx$1 /g;	# <tb:n>
			$_ =~ s/\t/\\tab /g;								# tab

			$_ =~ s/<hr>/$tag_ul/g;								# <hr>
			$_ =~ s/<pre>/{\\f3 /g;								# <pre>
			$_ =~ s/<\/pre>/ }/g;								# </pre>
			$_ =~ s/<http:([^>]*)>/{\\uldb http:$1}{\\v !ExecFile(http:$1)}/g;
			$_ =~ s/<https:([^>]*)>/{\\uldb https:$1}{\\v !ExecFile(https:$1)}/g;
			$_ =~ s/<mailto:([^>]*)>/{\\uldb $1}{\\v !ExecFile(mailto:$1)}/g;
			$_ =~ s/<url:([^>]*)>/\{\\uldb $1\}\{\\v !ExecFile($1)\}/g;
			$_ =~ s/<copyright:([^>]*)>/\{\\cf8 $1\}/g;
			$_ =~ s/<windowskey>/\{\\f5\\fs30 \\'ff\}/g;

			foreach ( $_ =~ /([ABEFPV]|HM|[CKMX][BCETV]?)\_[0-9A-Za-z]+/g ){
				$cid = $&;
				if ( ($cid eq '') | ($cid =~ /V_H[0-9A-F]|[ACMP]_[A-Z]/) ) {next}
				if ( !$CustID{$cid} && ($cid !~ /XX|xxx|yyy|sample|test|E_ATTRIBUTE|E_FILE|E_POINT|B_DATA/) ){
					print "$lines : No custid = $cid\n";
				}
			}
		}

		if ( $grouptitlememomode ){
			$grouptitlememo .= "$_$tail\n";
			next;
		}
		if ( $groupitem1 ne "" ){
			$groupdata .= "$_$tail";
			next;
		}

		if ( $popmode ){
			if ( $_ =~ /^\#\{\\footnote/ ){
				print OUT $1,"\\page\n";
			}
		}
		if ( $_ =~ /^\}(.+)/ ){ # }xxx 行末処理の無効化
			print OUT $1,"\n";
		}else{
			print OUT $_,$tail;
		}
	}
	if ( $html ){
		print OUT $1,'</body></html>';
		print OUTCNT $1,'</body></html>';
	}else{
		print OUT $1,'}';
	}
	close(OUTCNT);
	close(OUT);
	close(IN);

	if ( $html ){
		$online = "../script/ppxwhelp.pl";
		if (-f $online){
			system("perl $online $dest");
		}
	}else{
		system("hcrtf /x ".$helpname.".HPJ"); # /xh なら作った help を開く
	}
	while (($name, $value) = each(%anchor)) {
		if ( !$anchorset{$name} ){ printf("anchor $name not defined\n",$name)};
	}
0;

sub DumpGroupItem
{
	CheckTermTag();
	if ( $groupitem1 ne "" ){
		local($footnote);

		if ( $groupitemA ne "" ){
			if ( $anchorset{$groupitemA} ){
				printf("%d : '<a:%s>' red(%d)\n",$lines,$groupitemA,$anchorset{$groupitemA});
			}else{
				$anchorset{$groupitemA} = $lines;
			}
			if ( $anchor{$groupitemA} < 1 ){
				$anchor{$groupitemA} = $anchorcnt;
				$anchorcnt++;
			}
			if ( $html ){
				$ancname = $groupitemA;
			}else{
				$ancname = $anchor{$groupitemA};
			}
		}else{
			$ancname = $anchorcnt;
			$anchorcnt++;
		}

		if ( $groupitem1 =~ /^\#(.*)/ ){
			if ( $html ){
#				$footnote = "<a name=\"$1\"><\/a>";
#				print $footnote,"\n";
			}else{
				$footnote = "K\{\\footnote  $1\}";
			}
			$groupitem1 = $1;
		}else{
			$footnote = "";
		}
		if ( $groupitem2 eq "" ){
			$groupitem2 = $groupitem1;
			$groupitem1 = "";
		}else{
			if ( $html ){
				$groupitem1 .= "";
			}else{
				$groupitem1 .= "\\tab";
			}
		}
		if ( $groupdata ne "" ){
			if ( $html ){
				$grouplist .= "・<a href=\"#$ancname\">$groupitem1 $groupitem2<\/a>$tail";
				$groupitem .= "$tag_ul$tail"."<a name=\"$ancname\"><b>$groupitem1 $groupitem2<\/b><\/a>$tail".$groupdata;
			}else{
				$grouplist .= "・$groupitem1\{\\uldb $groupitem2\}\{\\v $ancname\}$tail\n";
				$groupitem .= "$tag_ul$tail\n"."$footnote#\{\\footnote $ancname\}$groupitem1\{\\b $groupitem2\}$tail".$groupdata;
			}
			$groupdata = "";
		}else{
			$grouplist .= "$footnote・$groupitem1 $groupitem2$tail\n";
		}
		$groupitem1 = "";
		$groupitemA = "";
	}
}

sub CheckTermTag
{
	if ( $listmode ){
		printf("%d : '<tb:' not terminate\n",$lines); $listmode = 0;
	}
	if ( $html && ($tail eq "\n") ){
		printf("%d : '<pre>' not terminate\n",$lines); $tail = '<br>\n';
	}
	if ( $precount ){
		printf("%d : '<pre>' not terminate %d\n",$lines,$precount); $precount = 0;
	}
	if ( $ulcount ){
		printf("%d : '<u>' not terminate %d\n",$lines,$ulcount); $ulcount = 0;
	}
	if ( $boldcount ){
		printf("%d : '<b>' not terminate %d\n",$lines,$boldcount); $boldcount = 0;
	}
	if ( $itccount ){
		printf("%d : '<i>' not terminate %d\n",$lines,$itccount); $itccount = 0;
	}
	if ( $listmode ){
		printf("%d : '<tb:>' not terminate %d\n",$lines,$listmode); $listmode = 0;
	}
}
