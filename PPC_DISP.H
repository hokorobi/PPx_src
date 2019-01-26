// PPc 表示書式関連(ppcdisp.plによる自動生成)
// 各定義一覧
// PPC_DISP 定義用定数

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

// static 系
#define DE_END	0
#define DE_SKIP	38
#define DE_SPC	1
#define DE_BLANK	2
#define DE_sepline	14
#define DE_hline	50
#define DE_NEWLINE	47
#define DE_itemname	16
#define DE_string	27
#define DE_ivalue	57
#define DE_fcolor	48
#define DE_bcolor	49
#define DE_fc_def	56
#define DE_bc_def	58
// エントリ系
#define DE_MARK	3
#define DE_ICON	36
#define DE_ICON2	46
#define DE_IMAGE	37
#define DE_CHECK	59
#define DE_CHECKBOX	60
#define DE_LFN	4
#define DE_SFN	5
#define DE_LFN_MUL	52
#define DE_LFN_LMUL	53
#define DE_LFN_EXT	62
#define DE_SFN_EXT	63
#define DE_SIZE1	6
#define DE_SIZE2	11
#define DE_SIZE3	12
#define DE_SIZE4	44
#define DE_TIME1	7
#define DE_TIME2	13
#define DE_ATTR1	8
#define DE_MEMO	9
#define DE_MEMOS	39
#define DE_MSKIP	10
#define DE_MEMOEX	61
#define DE_COLUMN	54
#define DE_MODULE	55

#define DE_WIDEV	34
#define DE_WIDEW	35
// マーク
#define DE_MNUMS	28
#define DE_MSIZE1	19
#define DE_MSIZE2	20
#define DE_MSIZE3	45
// ページ
#define DE_ALLPAGE	29
#define DE_NOWPAGE	30
// ディレクトリ情報
#define DE_FStype	15
#define DE_vlabel	42
#define DE_path	43
#define DE_pathmask	51

#define DE_ENTRYSET	17
#define DE_ENTRYA0	40
#define DE_ENTRYA1	41

#define DE_ENTRYV0	18
#define DE_ENTRYV1	31
#define DE_ENTRYV2	32
#define DE_ENTRYV3	33

#define DE_DFREE1	21
#define DE_DFREE2	22
#define DE_DUSE1	23
#define DE_DUSE2	24
#define DE_DTOTAL1	25
#define DE_DTOTAL2	26
// max ID:63

//各サイズ
#define DE_END_SIZE 0
#define DE_SKIP_SIZE 1
#define DE_SPC_SIZE 2
#define DE_BLANK_SIZE 2
#define DE_sepline_SIZE 1
#define DE_hline_SIZE 2
#define DE_NEWLINE_SIZE 3
#define DE_itemname_SIZE -1
#define DE_string_SIZE -1
#define DE_ivalue_SIZE -1
#define DE_fcolor_SIZE 5
#define DE_bcolor_SIZE 5
#define DE_fc_def_SIZE 2
#define DE_bc_def_SIZE 2
#define DE_MARK_SIZE 1
#define DE_ICON_SIZE 1
#define DE_ICON2_SIZE 2
#define DE_IMAGE_SIZE 3
#define DE_CHECK_SIZE 1
#define DE_CHECKBOX_SIZE 1
#define DE_LFN_SIZE 3
#define DE_SFN_SIZE 3
#define DE_LFN_MUL_SIZE 3
#define DE_LFN_LMUL_SIZE 3
#define DE_LFN_EXT_SIZE 3
#define DE_SFN_EXT_SIZE 3
#define DE_SIZE1_SIZE 1
#define DE_SIZE2_SIZE 2
#define DE_SIZE3_SIZE 2
#define DE_SIZE4_SIZE 2
#define DE_TIME1_SIZE 2
#define DE_TIME2_SIZE -2
#define DE_ATTR1_SIZE 2
#define DE_MEMO_SIZE 2
#define DE_MEMOS_SIZE 2
#define DE_MSKIP_SIZE 1
#define DE_MEMOEX_SIZE 3
#define DE_COLUMN_SIZE -3
#define DE_MODULE_SIZE 16
#define DE_WIDEV_SIZE 4
#define DE_WIDEW_SIZE 4
#define DE_MNUMS_SIZE 1
#define DE_MSIZE1_SIZE 2
#define DE_MSIZE2_SIZE 2
#define DE_MSIZE3_SIZE 2
#define DE_ALLPAGE_SIZE 1
#define DE_NOWPAGE_SIZE 1
#define DE_FStype_SIZE 1
#define DE_vlabel_SIZE 2
#define DE_path_SIZE 2
#define DE_pathmask_SIZE 2
#define DE_ENTRYSET_SIZE 1
#define DE_ENTRYA0_SIZE 1
#define DE_ENTRYA1_SIZE 1
#define DE_ENTRYV0_SIZE 2
#define DE_ENTRYV1_SIZE 1
#define DE_ENTRYV2_SIZE 1
#define DE_ENTRYV3_SIZE 1
#define DE_DFREE1_SIZE 2
#define DE_DFREE2_SIZE 2
#define DE_DUSE1_SIZE 2
#define DE_DUSE2_SIZE 2
#define DE_DTOTAL1_SIZE 2
#define DE_DTOTAL2_SIZE 2

//スキップテーブル
#define DE_SKIPTABLE	0,2,2,1,3,3,1,2,2,2,1,2,2,-2,1,1,-1,1,2,2,2,2,2,2,2,2,2,-1,1,1,1,1,1,1,4,4,1,3,1,2,1,1,2,2,2,2,2,3,5,5,2,2,3,3,-3,16,2,-1,2,1,1,3,3,3
//属性テーブル
#define DE_ATTRTABLE	1,1,1,6,2,2,2,2,2,2,2,2,2,2,1,16,1,16,16,4,4,16,16,16,16,16,16,1,4,8,8,16,16,16,2,2,6,6,1,2,16,16,16,32,2,4,6,1,1,1,1,16,2,2,2,2,1,1,1,6,6,2,2,2
