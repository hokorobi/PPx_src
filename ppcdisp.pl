#!/usr/bin/perl
# (c)TORO
#
	$dest = 'PPC_DISP.H';

	open(OUT, "> $dest");
	while (<DATA>) {
		chop;
		if ( $_ eq '' ){
			print OUT "\n";
			next;
		}
		$top = substr($_,0,1);
		if ( $top eq ';' ){ next; }
		if ( ($top eq '#') || ($top eq '/') ){
			print OUT "$_\n";
			next;
		}
		@tmp = split(/\t+/, $_);
		print OUT "#define @tmp[1]\t@tmp[0]\n";
		($list[$cnt][0],$list[$cnt][1],$list[$cnt][2],$list[$cnt][3]) = @tmp;
		$cnt++;
	}
	$max = $cnt - 1;
	print OUT "// max ID:$max";
# --------- ID順にソート
	for ( 0 .. ($cnt - 1) ){
		push(@sortkey,$list[$_][0]);
	}
	@list = @list[sort {$sortkey[$a] <=> $sortkey[$b]} 0..($cnt - 1)];
# --------- スキップテーブル
	print OUT "\n//スキップテーブル\n#define DE_SKIPTABLE	";
	for ( 0 .. ($cnt - 1) ){
		print OUT "$list[$_][2]";
		if ( $_ != ($cnt - 1) ){print OUT ",";}
	}
# --------- 属性テーブル
	print OUT "\n//属性テーブル\n#define DE_ATTRTABLE	";
	for ( 0 .. ($cnt - 1) ){
		print OUT $list[$_][3];
		if ( $_ != ($cnt - 1) ){print OUT ",";}
	}
# ---------
	print OUT "\n";
	close(OUT);

__END__
// PPc 表示書式関連(ppcdisp.plによる自動生成)
// 各定義一覧
// PPC_DISP 定義用定数

; ※1
#define DE_ATTR_STATIC	B0	// 画面構成要素など
#define DE_ATTR_ENTRY	B1	// エントリ情報
#define DE_ATTR_MARK	B2	// マーク情報
#define DE_ATTR_PAGE	B3	// ページ情報
#define DE_ATTR_DIR		B4	// ディレクトリ情報
#define DE_ATTR_PATH	B5	// ディレクトリパス行
#define DE_ATTR_ALL		0xff	// ディレクトリ情報

#define DE_ATTR_WIDEV	B16	// 桁数に合わせて窓枠調整が必要
#define DE_ATTR_WIDEW	B17	// 窓枠に合わせて桁数調整が必要

#define DE_ENABLE_FIXLENGTH	T("CFHRSVfns") // 窓枠可変長に対応

#define GetIcon2Len(iconsize) ((iconsize + 7) / 8) // アイコンpixを文字列長に変換

;ID	Macro名  size  ※1  コメント
// static 系
0	DE_END		0	1	末尾
38	DE_SKIP		1	1	境界合わせ用のダミーコード(UNICODE版用)
1	DE_SPC		2	1	Sn	空白				(BYTE:幅)
2	DE_BLANK	2	1	sn	空欄				(BYTE:幅)
14	DE_sepline	1	1	L	区切り線
50	DE_hline	2	1	Hn	水平線				(BYTE:幅)
47	DE_NEWLINE	3	1	/	改行				(WORD:次の行へのオフセット)
16	DE_itemname	-1	1	I"str"	アイテム名		(z string)
27	DE_string	-1	1	i"str"	文字列			(z string)
57	DE_ivalue	-1	1	vi"str" id別特殊環境変数	(z string)
48	DE_fcolor	5	1	O"color" 文字色			(COLORREF:色)
49	DE_bcolor	5	1	Ob"color" 背景色		(COLORREF:色)
56	DE_fc_def	2	1	Odn 文字色を定義済み色に	(BYTE:種類)
58	DE_bc_def	2	1	Og 背景色を定義済み色に	(BYTE:種類)
// エントリ系
; マーク/アイコン表示
3	DE_MARK		1	6	M	*
36	DE_ICON		1	6	N	アイコン
46	DE_ICON2	2	6	Nn	アイコン			(BYTE:大きさ)
37	DE_IMAGE	3	6	nm,n	画像			(BYTE:幅,BYTE:丈)
59	DE_CHECK	1	6	b	チェック
60	DE_CHECKBOX	1	6	B	チェックボックス
; エントリ名
4	DE_LFN		3	2	Fm,n	LFNファイル名	(ファイル名幅,拡張子幅)
5	DE_SFN		3	2	fm,n	SFNファイル名	(ファイル名幅,拡張子幅)
52	DE_LFN_MUL	3	2	FMm,n	LFN複数行(最終行以外)
53	DE_LFN_LMUL	3	2	FMm,n	LFN複数行(最終行)
62	DE_LFN_EXT	3	2	FEm,n	LFNファイル名(拡張子優先)
63	DE_SFN_EXT	3	2	fEm,n	SFNファイル名(拡張子優先)
; ファイルサイズ
6	DE_SIZE1	1	2	Z	ファイルサイズ1(7)
11	DE_SIZE2	2	2	zn	ファイルサイズ2		(BYTE:幅)
12	DE_SIZE3	2	2	Zn	ファイルサイズ3		(BYTE:幅)
44	DE_SIZE4	2	2	zKn	ファイルサイズ4,k	(BYTE:幅)
; 時刻
7	DE_TIME1	2	2	Tn	更新時刻			(BYTE:幅)
13	DE_TIME2	-2	2	t"s"時刻(強化版)		(種別,ASCIIZ)
; 属性
8	DE_ATTR1	2	2	An	表示属性			(BYTE:幅)
; コメント
9	DE_MEMO		2	2	Cn	コメント			(BYTE:幅)
39	DE_MEMOS	2	2	Cn	コメント(スキップ有)(BYTE:幅)
10	DE_MSKIP	1	2	c	コメントスキップ
61	DE_MEMOEX	3	2	u	拡張コメント		(BYTE:幅,BYTE:ID)
; カラム
54	DE_COLUMN	-3	2	U	カラム拡張			(BYTE:幅,WORD:項目memo,BYTE:自身の大きさ,TCHARZ 名称)
; 拡張
55	DE_MODULE	16	2	X	Module拡張			(BYTE:幅,BYTE:丈,DWORD:ハッシュ BYTE[9]:文字列)

; ※先頭配置 (BYTE:最小値、BYTE:最大値、BYTE:幅加工off)
34	DE_WIDEV	4	2	W	可変長処理
35	DE_WIDEW	4	2	w	可変長処理(窓枠依存型)
// マーク
28	DE_MNUMS	1	4	mn	マーク数
19	DE_MSIZE1	2	4	mSn	マークサイズ「,」なし(BYTE:幅)
20	DE_MSIZE2	2	4	msn	マークサイズ「,」あり(BYTE:幅)
45	DE_MSIZE3	2	4	mKn	マークサイズ「,」あり,k(BYTE:幅)
// ページ
29	DE_ALLPAGE	1	8	P	全ページ数
30	DE_NOWPAGE	1	8	p	現在ページ
// ディレクトリ情報
15	DE_FStype	1	16	Y	ファイルシステム名	3桁固定
42	DE_vlabel	2	16	V	ボリュームラベル	(BYTE:幅)
43	DE_path		2	32	R	現在のディレクトリ	(BYTE:幅)
51	DE_pathmask	2	16	RM	ディレクトリマスク	(BYTE:幅)

17	DE_ENTRYSET	1	16	E	エントリ数 表示/全て
40	DE_ENTRYA0	1	16	E0	エントリ数 全て
41	DE_ENTRYA1	1	16	E1	エントリ数 全て(./..を除く)

18	DE_ENTRYV0	2	16	en	エントリ数 表示		(BYTE:幅)		S2
31	DE_ENTRYV1	1	16	e1	エントリ数 表示(./..を除く)	S3
32	DE_ENTRYV2	1	16	e2	エントリ数 表示 dir	S4
33	DE_ENTRYV3	1	16	e3	エントリ数 表示 file	S5

21	DE_DFREE1	2	16	DF	空き容量「,」なし	(BYTE:幅)
22	DE_DFREE2	2	16	Df	空き容量「,」あり	(BYTE:幅)
23	DE_DUSE1	2	16	DU	使用容量「,」なし	(BYTE:幅)
24	DE_DUSE2	2	16	Du	使用容量「,」あり	(BYTE:幅)
25	DE_DTOTAL1	2	16	DT	総容量「,」なし		(BYTE:幅)
26	DE_DTOTAL2	2	16	Dt	総容量「,」あり		(BYTE:幅)
