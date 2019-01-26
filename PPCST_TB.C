/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer	ファイル判別/キー割当て/マウス割当て/メニュー
-----------------------------------------------------------------------------*/
#pragma setlocale("Japanese")
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#pragma hdrstop

#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE // IE3
	#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif
#ifndef LVS_EX_FULLROWSELECT // IE3
#define LVS_EX_FULLROWSELECT 0x0020
#endif
#ifndef LVS_EX_LABELTIP // IE5
#define LVS_EX_LABELTIP 0x4000
#endif
#define TListView_GetFocusedItem(hwnd) (int)SendMessage(hwnd,LVM_GETNEXTITEM,(WPARAM)-1,TMAKELPARAM(LVNI_FOCUSED,0))
// ListView の lParam に設定する値。PPcust内で共通＆クリアしないが、通常の
// 使用範囲ではオーバーフローしないのでそのまま使う。
LPARAM ListViewCounter = 1;

typedef struct {
	HWND	hLVTypeWnd,hLVAlcWnd; // ListView
	int		ListViewLastSort;		// IDV_ALCLISTのソート状態(+昇順,-降順)
	TCHAR	key;					// 項目の識別子(E/K/m/M)
	int		helpID;					// [ヘルプ]を押したときに表示するヘルプ
	int		index_type;				// 種類のインデックス
	int		index_alc;				// 項目のインデックス
	TCHAR	name_type[MAX_PATH];	// 選択された種類の内容
} TABLEINFO;

TABLEINFO extinfo;
TABLEINFO keyinfo;
TABLEINFO mouseinfo;
TABLEINFO menuinfo;
TABLEINFO barinfo;

struct LABELTEXT {
 const TCHAR *name,*text;
} textlists[] = {
 {T("E_cr"),T("PPc [Enter]\0")},
 {T("E_scr"),T("PPc,PPv \\[Enter]\0")},
 {T("E_unpack2"),T("PPc [U]\0")},

 {T("K_edit"),T("一行編集・PPe共用\0line edit and PPe")},
 {T("K_lied"),T("一行編集(K_editも参照)\0line edit")},
 {T("K_ppe"),T("PPe(K_editも参照)\0PPe")},
 {T("K_tray"),T("PPtrayホットキー\0PPtray hotkey")},
 {T("K_tree"),T("一般ツリー\0general tree")},
 {T("KB_edit"),T("PPb\0")},
 {T("KC_main"),T("PPc\0")},
 {T("KC_tree"),T("PPcツリー\0PPc tree")},
 {T("KC_incs"),T("PPcインクリメンタルサーチ\0PPc search")},
 {T("K_list"),T("PPc AutoD&Dリスト\0PPc AutoD&D list")},
 {T("KV_main"),T("PPv 共通\0PPv general")},
 {T("KV_page"),T("PPvテキスト(ページ)追加設定\0PPv page mode")},
 {T("KV_crt"),T("PPvテキスト(キャレット)追加設定\0PPv caret mode")},
 {T("KV_img"),T("PPv画像追加設定\0PPv image mode")},

 {T("M_edit"), T("一行編集メニューバー\0line edit,menu bar")},
 {T("M_editc"),T("一行編集コンテキスト\0line edit,context")},
 {T("M_pjump"),T("PPc [0]パス移動\0PPc path jump[0]")},
 {T("MC_menu"),T("PPcメニューバー\0PPc menu bar")},
 {T("M_Ccr"),  T("PPc [Enter]\0")},
 {T("M_DirMenu"),T("PPc フォルダコンテキスト\0PPc folder context")},
 {T("M_mask"), T("ワイルドカード入力補助\0wildcards")},
 {T("MC_sort"),T("PPcソート\0PPc sort")},
 {T("M_wsrc"), T("PPc検索、検索対象入力補助\0PPc where is,source")},
 {T("MC_mdds"),T("PPc拡張D&D\0PPc ex-D&D")},
 {T("M_xpack"),T("PPc \\[P]\0")},
 {T("M_bin"),  T("PPc [B]バイナリ変換\0PPc [B]")},
 {T("M_tabc"), T("PPc タブメニュー&[-]\0PPc tab menu &[-]")},
 {T("MV_menu"),T("PPvメニューバー\0PPv menu bar")},
 {T("M_ppvc"), T("PPvコンテキスト追加分\0PPv context")},
 {T("ME_menu"),T("PPeメニューバー\0PPe menu bar")},

 {T("MC_click"),T("PPc\0")},
 {T("MV_click"),T("PPv\0")},
 {T("MT_icon"),T("PPtray\0")},

 {T("B_cdef"),T("PPc標準ツールバー\0PPc toolbar")},
 {T("B_vdef"),T("PPv標準ツールバー\0PPv toolbar")},
 {T("B_tree"),T("ツリーツールバー\0Tree toolbar")},

 {T("HM_ppc"),T("PPc隠しメニュー\0PPc hidden menu")},
 {T("HM_ppv"),T("PPv隠しメニュー\0PPv hidden menu")},
 {NULL,NULL}
};

const TCHAR *InsertMacroMenuString[] = {
	T(" %FCD	選択ファイル名(１つのみ)\0 %FCD	one file name"),
	T(" %#FCD	選択ファイル名(列挙)\0 %#FCD	enum. file names"),
	T(" %1	カレントディレクトリ\0 %1	current directory name"),
	T(" %2	反対窓ディレクトリ\0 %2	pair window directory name"),
	T(" %0	PPxディレクトリ\0 %0	PPx directory name"),
	T(" %M	メニュー...\0 %M	menus..."),
	T(" %ME	ファイル判別...\0 %ME	extension lists..."),
	T(" %'	環境変数・エイリアス...\0 %'	env. , alias"),
	T(" %{text%|%}	編集(textが既定値,カーソル末尾)\0 %{text%|%}	edit \'text\'"),
	T("%Ob 	PPbを使わずに実行指定(先頭に記載)\0%Ob	no use PPb(written top)"),
	T("%Or-	マークがあっても１つのみ実行指定(末尾に記載)\0%Or-	no enum.(written bottom)"),
	T(" %:	コマンド区切り\0 %:	command separator"),
	NULL
};

const TCHAR *InsertMenuSeparatorString[] = {
	T("--	区切り線を挿入\0--	insert separator"),
	T("||	縦線を挿入\0||	insert new column separator"),
	T("&	ショートカット印を挿入\0||	insert shortcut letter mark"),
	T("\\t	インデントを挿入\0||	insert indent"),
};

#define GESTUREID 0
const TCHAR *MouseButtonList[] = {
	T("RG 右ジェスチャ\0RG right gesture"),
	T("R 右\0R right button"),
	T("RD 右ダブル\0RD right double"),
	T("RH 右長押し\0RH right press & hold"),
	T("L 左\0L Left button"),
	T("LD 左ダブル\0LD left double"),
	T("LH 左長押し\0LH left press & hold"),
	T("M 中/ホイール\0M middle button"),
	T("MD 中/ホイールダブル\0MD middle double"),
	T("MH 中/ホイール長押し\0MH middle press & hold"),
	T("W 左右同時\0W R & L button"),
	T("WH 左右同時長押し\0WH R & L press & hold"),
	T("X 第4ボタン\0X 4th button"),
	T("XD 第4ダブル\0XD 4th double"),
	T("XH 第4長押し\0XH 4th press & hold"),
	T("Y 第5ボタン\0Y 5th button"),
	T("YD 第5ダブル\0YD 5th double"),
	T("YH 第5長押し\0YH 5th press & hold"),
	T("H 左チルト\0H tilt left"),
	T("I 右チルト\0I tilt right"),
	NULL
};
#define CE_SHIFT 8
#define CE_RG	B8
#define CE_R	B9
#define CE_RD	B10
#define CE_RH	B11
#define CE_L	B12
#define CE_LD	B13
#define CE_LH	B14
#define CE_M	B15
#define CE_MD	B16
#define CE_MH	B17
#define CE_W	B18
#define CE_WH	B19
#define CE_X	B20
#define CE_XD	B21
#define CE_XH	B22
#define CE_Y	B23
#define CE_YD	B24
#define CE_YH	B25
#define CE_H	B26
#define CE_I	B27

#define CE_ALL	0xfffffff0
#define CE_NC	(CE_R | CE_RD | CE_LD | CE_M | CE_X | CE_XD | CE_Y | CE_YD)
#define CE_CTRL	(CE_L | CE_R | CE_RD | CE_LD | CE_M | CE_X | CE_XD | CE_Y | CE_YD)

#define CE_PPC	B0
#define CE_PPV	B1
#define CE_PPCV	(CE_PPC | CE_PPV)
#define CE_TRAY	B2

struct MouseTypeListStruct {
	DWORD enables;
	const TCHAR *str;
} MouseTypeList[] ={
	{CE_PPCV | CE_ALL,T("SPC 空欄\0SPC blank area")},
	{CE_PPC | CE_ALL,T("ENT エントリ\0ENT entry area")},
	{CE_PPC | CE_ALL,T("MARK マーク\0MARK make symbol")},
	{CE_PPC | CE_ALL,T("PATH パス表示行\0PATH path bar")},
	{CE_PPC | CE_TRAY | CE_ALL,T("ICON アイコン表示\0ICON icon area")},
	{CE_PPC | CE_CTRL,T("TABB タブボタン\0TABB tab button")},
	{CE_PPC | CE_CTRL,T("TABS タブ空欄\0TABS tab space")},
	{CE_PPC | CE_CTRL,T("HEAD カラムヘッダ\0Column header")},

	{CE_PPCV | CE_CTRL,T("MENU メニュー\0MENU menu bar")},
	{CE_PPCV | CE_ALL,T("LINE ステータス行\0LINE status line")},
	{CE_PPC | CE_ALL,T("INFO 情報行\0INFO information lines")},

	{CE_PPCV | CE_NC ,T("FRAM 窓枠\0FRAM window frame")},
	{CE_PPCV | CE_NC ,T("SYSM 窓枠アイコン\0SYSM window icon")},
	{CE_PPCV | CE_NC ,T("TITL タイトルバー\0TITL title bar")},
	{CE_PPCV | CE_NC ,T("MINI 最小化ボタン\0MINI minimize button")},
	{CE_PPCV | CE_NC ,T("ZOOM 最大化ボタン\0ZOOM maximize button")},
	{CE_PPCV | CE_NC ,T("CLOS [閉じる]ボタン\0CLOS close button")},
	{CE_PPCV | CE_NC ,T("SCRL スクロールバー\0SCRL scroll bar")},
	{CE_PPCV | CE_ALL,T("HMNU 隠しメニュー\0HMNU Hidden menu")},
	{0,NULL}
};

const TCHAR GALLOW[] = T("LUDR");

// 機能・キー割当て一覧関連
#define GROUPCHAR '.'
TCHAR *KeyList = NULL;
const TCHAR *KeyListGroup;
HWND hOldKeyListDlg = NULL;
int OldKeyListID;
int SelectItemLV;

// ツールバー関連
void LoadBar(HWND hDlg);

struct {
	HBITMAP hBmp;			// ツールバーイメージ
	SIZE barsize;			// ツールバーイメージの大きさ
	int items;				// ボタンの種類数
	TCHAR filename[VFPS];	// 表示中ツールバーのファイル名
} BarBmpInfo;

#define ToolBarDefaultCommandCount 47
const TCHAR *ToolBarDefaultCommand[ToolBarDefaultCommandCount][2] = { // デフォルトツールバーのコマンド一覧
//00
	{T("@^LEFT"),		T("戻る\0previous")},
	{T("@^RIGHT"),		T("進む\0forward")},
	{T("@0"),			T("お気に入り\0favorites")},
	{T("*customize M_pjump:%{&name=%1%}"),	T("お気に入りに追加\0add favorites")},
	{T("@\\T"),			T("ツリー\0tree")}, // ???

	{T("@^X"),			T("切り取り\0cut")},
	{T("@^C"),			T("クリップ\0clip")},
	{T("@^V"),			T("貼り付け\0paste")},
	{T("*file undo"),	T("コピー・移動のアンドゥ\0undo")},
	{NilStr,			T("やり直し\0re")},
//10
	{T("@D"),			T("削除\0delete")},
	{T("@\\K"),			T("新規作成\0new entry")},
	{T("@L"),			T("開く\0open directory")},
	{T("@\\L"),			T("ドライブ選択\0select drive")},
	{T("@^W"),			T("検索\0find file")},

	{T("@&ENTER"),		T("プロパティ\0properties")},
	{T("@F1"),			T("ヘルプ\0help")},
	{T("@^W"),			T("検索\0find")},
	{T("@^F"),			T("検索(エクスプローラ)\0find with explorer")},
	{NilStr,			T("\0")}, // 印刷
//20
	{T("@';'"),			T("表示形式\0view style")}, // アイコン表示
	{T("@';'"),			T("表示形式\0view style")}, // カタログ表示
	{T("@';'"),			T("表示形式\0view style")}, // 小さいアイコン
	{T("@';'"),			T("表示形式\0view style")}, // 詳細
	{T("*sortentry 0,-1,-1,B11111,1"),	T("名前順\0sort by name")},

	{T("*sortentry 2,-1,-1,B11111,1"),	T("大きさ順\0sort by size")},
	{T("*sortentry 3,-1,-1,B11111,1"),	T("日付順\0sort by date")},
	{T("@\\S"),			T("順序変更\0hold sort")},
	{T("@BS"),			T("親へ移動\0jump to parent")},
	{T("NETUSE"),		T("ネットワークドライブ追加\0Allocate Network drive")},
//30
	{T("NETUNUSE"),		T("ネットワークドライブ切断\0Free Network drive")},
	{T("@K"),			T("ディレクトリ作成\0new directory")},
	{T("@';'"),			T("表示形式\0view style")}, // カタログ表示
	{NilStr,			T("\0")}, // 電話帳?
	{NilStr,			T("\0")}, // 電話帳?

	{NilStr,			T("\0")}, // 電話?
	{NilStr,			T("\0")}, // 電話?
	{T("@';'"),			T("表示形式\0view style")}, // 表示形式
	{T("@';'"),			T("表示形式\0view style")}, // カタログ表示
	{T("@';'"),			T("表示形式\0view style")}, // 小さいアイコン
//40
	{T("@';'"),			T("表示形式\0view style")}, // 詳細
	{T("@\\K"),			T("新規作成\0new entry")},
	{T("@\\K"),			T("新規作成\0new entry")}, // テンプレート?
	{T("@\\T"),			T("フォルダツリー\0tree")},
	{T("@M"),			T("移動\0move")},

	{T("@C"),			T("コピー\0copy")},
	{T("CUSTOMIZE"),	T("カスタマイズ\0customize")},
};
// 隠しメニュー関連
COLORREF CharColor,BackColor; // 隠しメニューの色

const TCHAR StrTitleCommandBox[] = T("コマンド一覧\0command list");
const TCHAR StrMenuNew[] = T(" 新規\0 new");
const TCHAR StrMenuNewMemo[] = T("新規(設定ボタンの右クリックメニューでも新規追加ができます)\0new(Save button right click)");
const TCHAR StrLabelTypeName[] = T("判別名\0Type name");
const TCHAR StrLabelDetail[] = T("登録内容\0Detail");
const TCHAR StrLabelTargetKey[] = T("割当てキー\0Key");
const TCHAR StrLabelTargetKeyName[] = T("割当て説明\0Key name");
const TCHAR StrLabelTargetButton[] = T("割当て先\0Button");
const TCHAR StrLabelTargetButtonName[] = T("ボタン\0Button name");
const TCHAR StrLabelTargetArea[] = T("対象\0Target area");
const TCHAR StrLabelMenuItemName[] = T("表示項目\0Item name");
const TCHAR StrLabelItemName[] = T("項目名\0Item name");

const TCHAR StrTitleMenuName[] = T("メニュー名を指定してください(先頭のM_は必須です)\0Enter menu name (M_xxx)");
const TCHAR StrQueryKeyMapMenu[] =
		T("MC_menu,MV_menu のうち、内蔵コマンドを割り当てた\n")
		T("項目に、現在のキー割当てを書き込みます。\n")
		T("開始してかまいませんか？(時間がかかります)\0")
		T("Add comment key short cut to MC_menu and MV_menu ?");
const TCHAR StrWarnBadAssc[] = T("この割当ては機能しない可能性があります\0It seems disable");
const TCHAR StrQueryNoAdd[] = T("変更した内容が反映されていませんが、よろしいですか？\0Setting is not preserved. Really?");
const TCHAR StrQueryDeleteTable[] = T("%sを削除しますか？\0Delete %s ?");

HWND hCmdListWnd = NULL;

const TCHAR ArrowC[] =T("UDLR");
const TCHAR ArrowS[] =T("↑↓←→");

typedef struct {
	HWND hListViewWnd;
	int column;
	int order;
} LISTVIEWCOMPAREINFO;

const TCHAR StrOpenExe[] = T("アプリケーションを指定してください\0Select application.");
OPENFILENAME of_app = { sizeof(of_app),NULL,NULL,
	T("Executable File\0*.exe;*.com;*.bat;*.lnk;*.cmd\0")
	T("All Files\0*.*\0") T("\0"),
	NULL,0,0,NULL,VFPS,NULL,0,NULL,NULL,
	OFN_HIDEREADONLY | OFN_SHAREAWARE,0,0,T("*"),0,NULL,NULL OPENFILEEXTDEFINE
};

const TCHAR StrOpenBarImage[] = T("ボタン一覧の画像を指定してください\0Select toolbar image.");
const TCHAR StrOpenButtonImage[] = T("ボタン画像を指定してください\0Select toolbar button image.");

OPENFILENAME of_bar = { sizeof(of_bar),NULL,NULL,
	GetFileExtsStr,
	NULL,0,0,NULL,VFPS,NULL,0,NULL,NULL,
	OFN_HIDEREADONLY | OFN_SHAREAWARE,0,0,T("*"),0,NULL,NULL OPENFILEEXTDEFINE
};

#define MENUID_NEW 0x7000

enum { InsertMacroExt_LAUNCH = 0x100,InsertMacroExt_KEYCODE,InsertMacroExt_FILENAME,InsertMacroExt_HELP };
const TCHAR InsertMacroExt_LAUNCHstr[] = T("*launch \t指定したファイルがあるディレクトリをカレント/作業フォルダに指定して実行\0*launch \tset current dir. on file,and execute");
const TCHAR InsertMacroExt_KEYCODEstr[] = T("\tキー名称を挿入...\0\tinsert key code...");
const TCHAR InsertMacroExt_FILENAMEstr[] = T("\tファイル名をダイアログで選択して挿入...\0\tinsert selected filename");
const TCHAR InsertMacroExt_Helpstr[] = T("\tヘルプ\0\thelp");

const WCHAR StrCommentInfo[] = L"コメント\0comment";

int TrackButtonMenu(HWND hDlg,UINT ctrlID,HMENU hPopMenu)
{
	RECT box;

	GetWindowRect(GetDlgItem(hDlg,ctrlID),&box);
	return TrackPopupMenu(hPopMenu,TPM_TDEFAULT,box.left,box.bottom,0,hDlg,NULL);
}


BOOL ShowMouseSetting(HWND hDlg,BOOL normal)
{
	int idc;

	ShowDlgWindow(hDlg,IDC_ALCMOUSET,normal);
	for ( idc = IDB_ALCMOUSEL ; idc <= IDB_ALCMOUSER ; idc++ ){
		ShowDlgWindow(hDlg,idc,!normal);
	}
	return normal;
}

TCHAR *MakeMouseDetailText(HWND hDlg,const TCHAR *text,TCHAR *dest)
{
	TCHAR *p,*top,gbuf[200];
	size_t size;
	const TCHAR **blist,*button = NilStr,*area = NilStr;
	int count = 0;
							// 分割
	tstrcpy(dest,text);
	p = dest;
	while( !Isalpha(*p) ) p++;
	top = p;
	p = tstrchr(p,'_');
	if ( p != NULL ){
		*p++ = ' ';
		size = TSTROFF(p - top);
	}else{
		p = top;
		size = 0;
	}
							// ボタン名を取得
	blist = MouseButtonList;
	while( *blist ){
		if ( !memcmp(*blist,top,size) ){
			if ( hDlg != NULL ){
				SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_SETCURSEL,count,0);
			}else{
				button = GetCText(*blist) + (size / sizeof(TCHAR));
			}
			break;
		}
		blist++;
		count++;
	}
	if ( hDlg != NULL ){
		if ( *blist == NULL ){
			SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_SETCURSEL,1,0);
		}
		ShowMouseSetting(hDlg,count != GESTUREID);
	}
							// 対象を取得
	if ( count != GESTUREID ){
		const struct MouseTypeListStruct *mtl;

		if ( hDlg != NULL ){
			SendDlgItemMessage(hDlg,IDC_ALCMOUSET,CB_SETCURSEL,0,0);
		}
		size = TSTRLENGTH(p);
		mtl = MouseTypeList;
		count = 0;
		while( mtl->enables ){
			if ( !memcmp(mtl->str,p,size) ){
				if ( hDlg != NULL ){
					SendDlgItemMessage(hDlg,IDC_ALCMOUSET,CB_SETCURSEL,count,0);
				}else{
					if ( *(p + 1) != '\0' ){
						area = GetCText(mtl->str) + (size / sizeof(TCHAR)) + 1;
					}
				}
				break;
			}
			count++;
			mtl++;
		}
	}else if ( hDlg == NULL ){ // ジェスチャー
		TCHAR *gdest;

		area = gbuf;
		gdest = gbuf;
		while ( *p ){
			const TCHAR *cp;

			cp = ArrowC;
			while( *cp ){
				if ( *cp == *p ){
					#ifdef UNICODE
						*gdest++ = ArrowS[cp - ArrowC];
					#else
						const TCHAR *dp;

						dp = &ArrowS[(cp - ArrowC) * sizeof(WCHAR)];
						*gdest++ = *dp++;
						*gdest++ = *dp;
					#endif
				}
				cp++;
			}
			p++;
		}
		*gdest = '\0';
	}
	tstrcpy(dest,area);
	return (TCHAR *)button;
}

BOOL MakeKeyDetailListText(TABLEINFO *tinfo,const TCHAR **text,TCHAR *dest)
{
	int key;

	key = GetKeyCode(text);
	if ( key < 0 ) return FALSE;
	if ( key & (K_internal | K_ex) ) return FALSE;

	MakeKeyDetailText(key,dest,
			(tinfo == NULL) || tstrcmp(tinfo->name_type,T("K_tray")) );

	return TRUE;
}

void AddDefaultCmdList(HWND hLVAlcWnd)
{
	HWND hListViewWnd;
	TV_ITEM tvi;
	LV_ITEM lvi;
	TCHAR buf[CMDLINESIZE * 2],keydetail[64];
	const TCHAR *listp;

	hListViewWnd = hLVAlcWnd;
	SendMessage(hListViewWnd,WM_SETREDRAW,FALSE,0);

	listp = KeyList;
	for ( ;; ){ // 列挙開始
		TCHAR *namestr,*keystr;

		if ( *listp == '\0' ) break;
		tvi.lParam = (LPARAM)listp;
		if ( *listp == GROUPCHAR){
//		未実装
		}else{
			tstrcpy(buf,listp);
			namestr = tstrchr(buf,'\t');
			if ( namestr == NULL ) break;
			*namestr++ = '\0';
			keystr = tstrchr(namestr,'\t');
			if ( keystr == NULL ) break;
			*keystr++ = '\0';
			if ( UseLcid == LCID_PPXDEF ){
				namestr = buf;
			}
			if ( *keystr == '@' ){ // キーなら登録
				keystr++;
				lvi.pszText = keystr;
				*keystr = '\0';
				MakeKeyDetailListText(NULL,(const TCHAR **)&keystr,keydetail);
				if ( *keystr == '\0' ){ // キーが１つ記載のみ
					lvi.mask = LVIF_TEXT | LVIF_PARAM; // キー割当て
					lvi.lParam = ListViewCounter++;
					lvi.iItem = -1;
					lvi.iSubItem = 0;
					lvi.iItem = ListView_InsertItem(hListViewWnd,&lvi);

					lvi.mask = LVIF_TEXT;
					lvi.iSubItem = 1;		// キー説明
					lvi.pszText = keydetail;
					ListView_SetItem(hListViewWnd,&lvi);

					lvi.iSubItem = 2;		// 説明
					lvi.pszText = namestr;
					ListView_SetItem(hListViewWnd,&lvi);
				}
			}
		}
		listp += tstrlen(listp) + 2;

	}
	SendMessage(hListViewWnd,LVM_SETCOLUMNWIDTH,0,LVSCW_AUTOSIZE);
	SendMessage(hListViewWnd,LVM_SETCOLUMNWIDTH,1,LVSCW_AUTOSIZE);
	SendMessage(hListViewWnd,LVM_SETCOLUMNWIDTH,2,LVSCW_AUTOSIZE);
	SendMessage(hListViewWnd,WM_SETREDRAW,TRUE,0);
}

void EnableSetButton(HWND hDlg,BOOL enable)
{
	EnableDlgWindow(hDlg,IDB_ALCSET,enable);
}

void SetCommand(HWND hDlg,const TCHAR *str/*,TABLEINFO *tinfo*/)
{
	TCHAR buf[CMDLINESIZE];

	str = tstrrchr(str,'\t');
	if ( str == NULL ) return;
	str++;

	if ( (*str == '*') || (*str == '%') ){
	}else if ( *str == '>' ){
		str++;
	}else{
		wsprintf(buf,T("%%K\"%s\""),str);
		str = buf;
	}
	SetDlgItemText(hDlg,IDE_ALCCMD,str);
	EnableSetButton(hDlg,TRUE);
}

void InitCommandTree(void)
{
	TV_ITEM tvi;
	TV_INSERTSTRUCT tvins;
	HTREEITEM hParentTree;
	TCHAR *listp;
	TCHAR buf[CMDLINESIZE];

	tvi.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
	tvins.hInsertAfter = TVI_LAST;
	TreeInsertItemValue(tvins) = tvi;

	listp = KeyList;

	SendMessage(hCmdListWnd,WM_SETREDRAW,FALSE,0);
	for ( ;; ){
		TCHAR *p;

		if ( *listp == '\0' ) break;
		tvi.lParam = (LPARAM)listp;
		if ( *listp == GROUPCHAR ){
			tstrcpy(buf,listp + 1);
			p = tstrchr(buf,'\t');
			if ( p == NULL ){
				p = buf;
			}else{
				if ( UseLcid == LCID_PPXDEF ){
					*p = '\0';
					p = buf;
				}else{
					p++;
				}
			}
			tvi.mask = TVIF_TEXT | TVIF_PARAM;
			tvi.pszText = p;
			tvi.cchTextMax = tstrlen32(tvi.pszText);
			tvi.cChildren = 1;
			tvins.hParent = TVI_ROOT;
			TreeInsertItemValue(tvins) = tvi;

			hParentTree = (HTREEITEM)SendMessage(hCmdListWnd,TVM_INSERTITEM,
								0,(LPARAM)(LPTV_INSERTSTRUCT)&tvins);
		}else{
			tstrcpy(buf,listp);
			p = tstrchr(buf,'\t');
			if ( p == NULL ) break;
			*p = '\0';
			if ( UseLcid == LCID_PPXDEF ){
				p = buf;
			}else{
				TCHAR *p1;

				p++;
				p1 = tstrchr(p,'\t');
				if ( p1 != NULL ) *p1 = '\0';
			}
			tvi.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
			tvi.pszText = p;
			tvi.cchTextMax = tstrlen32(p);
			tvi.cChildren = 0;
			tvins.hParent = hParentTree;
			TreeInsertItemValue(tvins) = tvi;

			SendMessage(hCmdListWnd,TVM_INSERTITEM,
					0,(LPARAM)(LPTV_INSERTSTRUCT)&tvins);
		}
		listp += tstrlen(listp) + 2;
	}
	SendMessage(hCmdListWnd,WM_SETREDRAW,TRUE,0);
	InvalidateRect(hCmdListWnd,NULL,FALSE);
}

void USEFASTCALL InitCmdList(HWND hDlg)
{
	RECT box;

	if ( (hCmdListWnd != NULL) && IsWindow(hCmdListWnd) ){
		PostMessage(hCmdListWnd, WM_CLOSE, 0, 0);
		hCmdListWnd = NULL;
		return;
	}

	GetWindowRect(hDlg,&box);
	hCmdListWnd = CreateWindow(WC_TREEVIEW, GetCText(StrTitleCommandBox),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE |
		TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		box.left - 8, box.top - 8,
		(box.right - box.left) / 3, ((box.bottom - box.top) * 2) / 3,
		hDlg, NULL, hInst, NULL);
	if ( X_dss & DSS_COMCTRL ) SendMessage(hCmdListWnd, CCM_DPISCALE, TRUE, 0);
	InitCommandTree();
}

void CommandTreeDlgBox_Init(HWND hDlg)
{
	GetCustData(T("X_LANG"),&UseLcid,sizeof(UseLcid));
	if ( !UseLcid ) UseLcid = LOWORD(GetUserDefaultLCID());

	KeyList = LoadTextResource(MAKEINTRESOURCE(DEFKEYLIST));
	hCmdListWnd = GetDlgItem(hDlg,IDT_GENERAL);
	if ( X_dss & DSS_COMCTRL ) SendMessage(hCmdListWnd,CCM_DPISCALE,TRUE,0);
	InitCommandTree();
}

LRESULT CmdTreeNotify(HWND hDlg,NMHDR *nhdr/*,TABLEINFO *tinfo*/)
{
	if ( nhdr->code == TVN_SELCHANGED ){
		TV_ITEM tvi;

		tvi.hItem = ((NM_TREEVIEW *)nhdr)->itemNew.hItem;
		if ( tvi.hItem == NULL ){
			tvi.hItem = TreeView_GetSelection(hCmdListWnd);
			if ( tvi.hItem == NULL ) return 0;
		}
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hCmdListWnd,&tvi);
		if ( *(const TCHAR *)tvi.lParam != GROUPCHAR ){
			SetCommand(hDlg,(const TCHAR *)tvi.lParam/*,tinfo*/);
		}
	}
	return 0;
}

void RunTreeCommand(HWND hDlg)
{
	HWND hPPcWnd = PPcGetWindow(0,CGETW_GETFOCUS);
	TCHAR param[CMDLINESIZE];
	COPYDATASTRUCT copydata;

	if ( hPPcWnd == NULL ) return;
	param[0] = '\0';
	GetDlgItemText(hDlg,IDE_ALCCMD,param,CMDLINESIZE);
	if ( param[0] == '\0' ) return;

	copydata.dwData = 'H';
	copydata.cbData = TSTRSIZE32(param);
	copydata.lpData = (PVOID)param;
	SendMessage(hPPcWnd,WM_COPYDATA,0,(LPARAM)&copydata);
}

INT_PTR CALLBACK CommandTreeDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch ( iMsg ){
		case WM_INITDIALOG:
			CommandTreeDlgBox_Init(hDlg);
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			if ( NHPTR->hwndFrom == hCmdListWnd ){
				CmdTreeNotify(hDlg,NHPTR);
			}
			#undef NHPTR
			break;

		case WM_COMMAND:
			if ( LOWORD(wParam) == IDOK ) RunTreeCommand(hDlg);
			break;

		default:
			return PPxDialogHelper(hDlg,iMsg,wParam,lParam);
	}
	return TRUE;
}

int CALLBACK ListViewCompareFunc(LPARAM lParam1,LPARAM lParam2,LISTVIEWCOMPAREINFO *cmpinfo)
{
	LV_FINDINFO lvfi;
	LV_ITEM lvi;
	TCHAR str1[CMDLINESIZE],str2[CMDLINESIZE];

	if ( lParam1 == 0 ) return -1;
	if ( lParam2 == 0 ) return 1;

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lParam1;
	lvi.iItem = ListView_FindItem(cmpinfo->hListViewWnd,-1,&lvfi);
	lvi.mask = LVIF_TEXT;
	lvi.pszText = str1;
	lvi.cchTextMax = TSIZEOF(str1);
	lvi.iSubItem = cmpinfo->column;
	ListView_GetItem(cmpinfo->hListViewWnd,&lvi);

	lvfi.lParam = lParam2;
	lvi.iItem = ListView_FindItem(cmpinfo->hListViewWnd,-1,&lvfi);
	lvi.pszText = str2;
	ListView_GetItem(cmpinfo->hListViewWnd,&lvi);

	return (cmpinfo->order >= 0) ? tstrcmp(str1,str2) : tstrcmp(str2,str1);
}

/*-----------------------------------------------------------------------------
  実行ファイルを検索する
-----------------------------------------------------------------------------*/
enum {
	MENUID_REFNAME = 1,MENUID_DEFAULTLIST,MENUID_ADDBUTTONBMP,MENUID_DELBUTTONBMP
};
const TCHAR StrMenuBarRef[] = T("ボタン列画像ファイル(&R): %s\0&Reference of buttons list bitmap: %s");
const TCHAR StrMenuBarDefault[] = T("初期ボタン列画像(&L)\0Default button &List");
const TCHAR StrMenuBarAddButton[] = T("ボタン画像追加(&A)\0&Add button bitmap");
const TCHAR StrMenuBarDelButton[] = T("ボタン画像削除(&D)\0&Delete button");

const TCHAR StrLoadDefToolBar[] = T("ボタン列画像を再読み込みしたので、やり直してください\0Retry action by reload bitmap");
const TCHAR StrUnsupportDisp[] = T("この解像度では現在対応していません。\0Unsupport");

void SaveToolBarListBitmapPath(HWND hDlg,const TCHAR *path)
{
	TCHAR type[VFPS],buf[VFPS + 8];

	GetControlText(hDlg,IDE_EXTYPE,type,TSIZEOF(type));

	if ( path[0] == '\0' ){
		DeleteCustTable(type,T("@"),0);
	}else{
		*(DWORD *)buf = 0;
		*(TCHAR *)((BYTE *)buf + 4) = EXTCMD_CMD;
		tstrcpy( (TCHAR *)((BYTE *)buf + 4) + 1,path );

		SetCustTable(type,T("@"),buf,TSTRSIZE( (TCHAR *)((BYTE *)buf + 4) ) + 4);
	}
	LoadBar(hDlg);
	Changed(hDlg);
}

BOOL SaveToolBarBitmap(HWND hWnd,const TCHAR *path,HBITMAP hBmp)
{
	int memsize,infosize;
	BYTE *memdata;
	BITMAPFILEHEADER *bfh;
	BITMAPINFO *bi;
	HDC hDC,hMDC;
	HBITMAP hOldBmp;
	HANDLE hFile;
	BOOL result;
	BITMAP bmpi;

	GetObject(hBmp,sizeof(bmpi),&bmpi);

	infosize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	if ( bmpi.bmBitsPixel <= 8 ){
		XMessage(hWnd,StrCustTitle,XM_ImWRNld,GetCText(StrUnsupportDisp));
		return FALSE;
		// infosize += sizeof(RGBQUAD) * (1 << bmpi.bmBitsPixel);
	}
	memsize = infosize + bmpi.bmWidthBytes * bmpi.bmHeight;
	memdata = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,memsize);
	bfh = (BITMAPFILEHEADER *)memdata;
	bfh->bfType = 'B' + ('M' << 8);
	bfh->bfSize = memsize;
	bfh->bfOffBits = infosize;

	bi = (BITMAPINFO *)(BYTE *)(memdata + sizeof(BITMAPFILEHEADER));
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi->bmiHeader.biWidth = bmpi.bmWidth;
	bi->bmiHeader.biHeight = bmpi.bmHeight;
	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = bmpi.bmBitsPixel;
	bi->bmiHeader.biCompression = BI_RGB;
/*
	if ( bmpi.bmBitsPixel <= 8 ){
		GetDIBColorTable(hBmp,0,	(1 << bmpi.bmBitsPixel),bi->bmiColors);
	}
*/
	hDC = GetDC(hWnd);
	hMDC = CreateCompatibleDC(hDC);
	hOldBmp = SelectObject(hMDC,hBmp);
	GetDIBits(hMDC,hBmp,0,bmpi.bmHeight,memdata + infosize,bi,DIB_RGB_COLORS);
	SelectObject(hMDC,hOldBmp);
	DeleteDC(hMDC);
	ReleaseDC(hWnd,hDC);

	hFile = CreateFile(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
			FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		DWORD wsize;

		result = WriteFile(hFile,memdata,memsize,&wsize,NULL);
		CloseHandle(hFile);
		SaveToolBarListBitmapPath(hWnd,path);
	}else{
		result = FALSE;
	}
	HeapFree(GetProcessHeap(),0,memdata);
	return result;
}

BOOL InitEditToolBarBitmap(HWND hWnd)
{
	PPXDBINFOSTRUCT dbinfo;
	TCHAR path[VFPS],buf[VFPS];

	if ( BarBmpInfo.filename[0] != '\0' ){
		TCHAR *ext = BarBmpInfo.filename + FindExtSeparator(BarBmpInfo.filename);
		if ( tstricmp(ext,T(".bmp")) == 0 ) return TRUE; // 加工可能
		wsprintf(path,T("%s.bmp"),BarBmpInfo.filename);
	}else{
		dbinfo.structsize = sizeof dbinfo;
		dbinfo.custpath = buf;
		GetPPxDBinfo(&dbinfo);
		wsprintf(path,T("..\\toolbar%d.bmp"),BarBmpInfo.barsize.cy);
		VFSFullPath(NULL,path,buf);
	}

	//加工できないのでコピー用意

	if ( GetFileAttributesL(path) != MAX32 ){
		SaveToolBarListBitmapPath(hWnd,path);
		XMessage(hWnd,StrCustTitle,XM_ImWRNld,GetCText(StrLoadDefToolBar));
		return FALSE;
	}else{
		return SaveToolBarBitmap(hWnd,path,BarBmpInfo.hBmp);
	}
}

void AddToolBarButton(HWND hWnd,const TCHAR *path)
{
	HTBMP hTbmp;
	HBITMAP hDestBMP;
	HGDIOBJ hOldSrcBMP,hOldDstBMP;
	HDC hDC,hDstDC,hSrcDC;
	LPVOID lpBits;
	TCHAR buf[VFPS];
	BITMAPINFO bmpinfo;

	if ( LoadBMP(&hTbmp,path,BMPFIX_TOOLBAR) == FALSE ){
		XMessage(hWnd,path,XM_ImWRNld,T("load error"));
		return;
	}

	hDC = GetDC(hWnd);
	hDstDC = CreateCompatibleDC(hDC);
	hSrcDC = CreateCompatibleDC(hDC);
	ReleaseDC(hWnd,hDC);
	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.bmiHeader.biWidth = BarBmpInfo.barsize.cx + BarBmpInfo.barsize.cy;
	bmpinfo.bmiHeader.biHeight = BarBmpInfo.barsize.cy;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = 32;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;
	hDestBMP = CreateDIBSection(hDC,&bmpinfo,DIB_RGB_COLORS,&lpBits,NULL,0);

	// 元画像を複写
	hOldSrcBMP = SelectObject(hSrcDC,BarBmpInfo.hBmp);
	hOldDstBMP = SelectObject(hDstDC,hDestBMP);
	BitBlt(hDstDC,0,0,
			BarBmpInfo.barsize.cx,BarBmpInfo.barsize.cy,
			hSrcDC,0,0, SRCCOPY);
	SelectObject(hDC,hOldSrcBMP);
	DeleteDC(hSrcDC);

	// 追加画像を複写
	DrawBMP(hDstDC,&hTbmp,BarBmpInfo.barsize.cx,0);
	SelectObject(hDstDC,hOldDstBMP);
	DeleteDC(hDstDC);
	FreeBMP(&hTbmp);

	// 保存
	tstrcpy(buf,BarBmpInfo.filename);
	BarBmpInfo.filename[0] = '\0';
	SaveToolBarBitmap(hWnd,buf,hDestBMP);
	tstrcpy(BarBmpInfo.filename,buf);
	DeleteObject(hDestBMP);
}

void ToolBarMenu(HWND hDlg)
{
	TCHAR buf[VFPS];
	HMENU hPopMenu = CreatePopupMenu();
	int index;

	wsprintf(buf,GetCText(StrMenuBarRef),
			(BarBmpInfo.filename[0] == '\0') ?
				T("Default") : BarBmpInfo.filename);
	AppendMenu(hPopMenu,MF_ES,MENUID_REFNAME,buf);
	if ( BarBmpInfo.filename[0] != '\0' ){
		AppendMenu(hPopMenu,MF_ES,MENUID_DEFAULTLIST,GetCText(StrMenuBarDefault));
	}
/*
	AppendMenu(hPopMenu,MF_ES,MENUID_REFNAME,"16");
	AppendMenu(hPopMenu,MF_ES,MENUID_REFNAME,"24");
	AppendMenu(hPopMenu,MF_ES,MENUID_DELBUTTONBMP,GetCText(StrMenuBarDelButton));
*/
	AppendMenu(hPopMenu,MF_ES,MENUID_ADDBUTTONBMP,GetCText(StrMenuBarAddButton));

	index = TrackButtonMenu(hDlg,IDB_BREF,hPopMenu);
	DestroyMenu(hPopMenu);

	switch ( index ){
		case MENUID_REFNAME:
			tstrcpy(buf,BarBmpInfo.filename);
			of_bar.hwndOwner = hDlg;
			of_bar.hInstance = hInst;
			of_bar.lpstrFile = buf;
			of_bar.lpstrTitle = GetCText(StrOpenBarImage);
			if ( GetOpenFileName(&of_bar) ) SaveToolBarListBitmapPath(hDlg,buf);
			break;

		case MENUID_DEFAULTLIST:
			SaveToolBarListBitmapPath(hDlg,NilStr);
			break;

		case MENUID_ADDBUTTONBMP:
			if ( InitEditToolBarBitmap(hDlg) == FALSE ) break;

			buf[0] = '\0';
			of_bar.hwndOwner = hDlg;
			of_bar.hInstance = hInst;
			of_bar.lpstrFile = buf;
			of_bar.lpstrTitle = GetCText(StrOpenButtonImage);
			if ( GetOpenFileName(&of_bar) ) AddToolBarButton(hDlg,buf);
			break;
	}
}

//------------------------------------------------ 種類名が正しいか
BOOL CheckTypeName(TCHAR key,const TCHAR *name)
{
	if ( key == 'm' ){
		if (!tstrcmp(name,T("MC_click")) ||
			!tstrcmp(name,T("MV_click")) ||
			!tstrcmp(name,T("MT_icon")) ){
			return TRUE;
		}
		return FALSE;
	}
	if ( key == 'B' ){
		if (!tstrcmp(name,T("HM_ppc")) ||
			!tstrcmp(name,T("HM_ppv")) ){
			return TRUE;
		}
	}
	if ( key == 'K' ){
		if (!tstrcmp(name,T("K_edit")) ||
			!tstrcmp(name,T("K_tray")) ||
			!tstrcmp(name,T("K_tree")) ||
			!tstrcmp(name,T("K_list")) ){
			return TRUE;
		}
	}
	if ( name[0] != key ) return FALSE;
	if ( name[1] != '_' ){
		if ( !(Isalpha(name[1]) && (name[2] == '_')) ) return FALSE;
		if ( key == 'M' ){
			if (!tstrcmp(name,T("MC_celS")) ||
				!tstrcmp(name,T("MC_click")) ||
				!tstrcmp(name,T("MV_click")) ||
				!tstrcmp(name,T("MT_icon")) ){
				return FALSE;
			}
		}
	}
	return TRUE;
}

void InsertAliasMacro(HWND hDlg,TCHAR *buf)
{
	DWORD index,X_mwid = 60,id = 1;
	int size;
	HMENU hMenu;
	TCHAR value[CMDLINESIZE],key[VFPS + CMDLINESIZE + 8],*envptr;
	const TCHAR *ep;

	GetCustData(T("X_mwid"),&X_mwid,sizeof(X_mwid));
	if ( X_mwid > (CMDLINESIZE - 10) ) X_mwid = CMDLINESIZE - 10;
	hMenu = CreatePopupMenu();

	// エイリアス一覧
	for( index = 0 ; ; index++ ){
		size = EnumCustTable(index,T("A_exec"),key,value,sizeof(value));
		if ( 0 > size ) break;
		tstrcpy(value + X_mwid,T("..."));
		tstrcat(key,T("\t"));
		tstrcat(key,value);
		AppendMenu(hMenu,MF_ES,id++,key);
	}
	AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
	// 環境変数一覧
	ep = envptr = GetEnvironmentStrings();
	if ( ep != NULL ){
		while ( *ep != '\0' ){
			TCHAR *p;

			if ( *ep != '=' ){
				tstrcpy(key,ep);
				tstrcpy(key + X_mwid,T("..."));
				p = tstrchr(key,'=');
				if ( p != NULL ){
					*p = '\t';
					AppendMenu(hMenu,MF_ES,id++,key);
				}
			}
			ep += tstrlen(ep) + 1;
		}
		FreeEnvironmentStrings(envptr);
	}

	id = TrackButtonMenu(hDlg,IDB_ALCCMDI,hMenu);
	if ( id > 0 ){
		TCHAR *p;

		GetMenuString(hMenu,id,buf,CMDLINESIZE,MF_BYCOMMAND);
		p = tstrchr(buf,'\t');
		if ( p != NULL ){
			*p = '\'';
			*(p + 1) = '\0';
		}
	}
	DestroyMenu(hMenu);
}

void InsertMenuMacro(HWND hDlg,TCHAR type)
{
	int count,size,id = 1;
	TCHAR name[VFPS],comment[VFPS],buf[CMDLINESIZE];
	HMENU hMenu;

	hMenu = CreatePopupMenu();
	AppendMenu(hMenu,MF_ES,MENUID_NEW,GetCText(StrMenuNew));
	AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
											// 使用済みを検索
	for( count = 0 ; ; count++ ){
		size = EnumCustData(count,name,NULL,0);
		if ( 0 > size ) break;

		if ( CheckTypeName(type,name) == FALSE ) continue;
		comment[0] = '\0';
		GetCustTable(T("#Comment"),name,comment,VFPS);
		wsprintf(buf,T("%s\t%s"),name,comment);
		AppendMenu(hMenu,MF_ES,id++,buf);
	}
	id = TrackButtonMenu(hDlg,IDB_ALCCMDI,hMenu);
	if ( id != 0 ){
		if ( id == MENUID_NEW ){
			tstrcpy(buf,(type == 'E') ? T("E_newlist") : T("M_newmenu"));
			if ( tInput(hDlg,GetCText(StrTitleMenuName),buf,
						VFPS,PPXH_GENERAL,PPXH_GENERAL) > 0 ){
				if ( (buf[0] == type) && (buf[1] == '_') ){
/*
					SetCustTable(buf,T("item"),NilStr,sizeof(TCHAR));
					LV_ITEM lvi;
					lvi.mask = LVIF_TEXT;
					lvi.iItem = -1;
					lvi.iSubItem = 0;
					lvi.pszText = buf;
					ListView_InsertItem(GetDlgItem(hDlg,IDV_ALCLIST),&lvi);
					Changed(hDlg);
*/
				}else{
					buf[0] = '\0';
				}
			}else{
				buf[0] = '\0';
			}
		}else{
			GetMenuString(hMenu,id,buf,CMDLINESIZE,MF_BYCOMMAND);
			*tstrchr(buf,'\t') = '\0';
		}
		if ( buf[0] != '\0' ){
			wsprintf(comment,(type == 'E') ? T(" %%M%s") :T(" %%%s"),buf);
			SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_REPLACESEL,0,(LPARAM)comment);
		}
	}
	DestroyMenu(hMenu);
}

void SetKeyComment(HWND hDlg,TABLEINFO *tinfo,TCHAR *label)
{
	TCHAR *p,buf[MAX_PATH];

	p = label;
	if ( MakeKeyDetailListText(tinfo,(const TCHAR **)&p,buf) ){
		p = buf;
	}else{
		p = label;
	}
	SetDlgItemText(hDlg,IDS_EXITEM,p);
}

//-------------------------------------------------------- キー入力
void ChooseKey(HWND hDlg,int id,TABLEINFO *tinfo)
{
	TCHAR temp[64];

	temp[0] = '\0';
	if ( KeyInput(GetParent(hDlg),temp) > 0 ){
		HWND hEdWnd = GetDlgItem(hDlg,id);
		if ( tinfo != NULL ){
			SetWindowText(hEdWnd,temp);
			SetKeyComment(hDlg,tinfo,temp);
		}else{
			SendMessage(hEdWnd,EM_REPLACESEL,0,(LPARAM)temp);
		}
	}
	return;
}

void InsertMacroString(HWND hDlg)
{
	HMENU hMenu;
	const TCHAR **menustr = InsertMacroMenuString;
	int id = 1;
	TCHAR buf[VFPS],*p;

	hMenu = CreatePopupMenu();
	for (  ; *menustr != NULL ; menustr++ ){
		AppendMenu(hMenu,MF_ES,id++,GetCText(*menustr));
	}
	AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
	AppendMenu(hMenu,MF_ES,InsertMacroExt_LAUNCH,GetCText(InsertMacroExt_LAUNCHstr));
	AppendMenu(hMenu,MF_ES,InsertMacroExt_KEYCODE,GetCText(InsertMacroExt_KEYCODEstr));
	AppendMenu(hMenu,MF_ES,InsertMacroExt_FILENAME,GetCText(InsertMacroExt_FILENAMEstr));
	AppendMenu(hMenu,MF_ES,InsertMacroExt_HELP,GetCText(InsertMacroExt_Helpstr));

	id = TrackButtonMenu(hDlg,IDB_ALCCMDI,hMenu);
	DestroyMenu(hMenu);
	switch (id){
		case InsertMacroExt_LAUNCH:
			tstrcpy(buf,InsertMacroExt_LAUNCHstr);
			break;

		case InsertMacroExt_KEYCODE:
			ChooseKey(hDlg,IDE_ALCCMD,NULL);
			return;

		case InsertMacroExt_FILENAME:
			buf[0] = '\0';
			of_app.hwndOwner = hDlg;
			of_app.hInstance = hInst;
			of_app.lpstrFile = buf;
			of_app.lpstrTitle = GetCText(StrOpenExe);
			if ( GetOpenFileName(&of_app) == FALSE ) return;
			break;

		case InsertMacroExt_HELP:
			PPxHelp(hDlg,HELP_KEY,(DWORD_PTR)T("macro"));
			return;

		default:
			if ( id <= 0 ) return;
			tstrcpy(buf,InsertMacroMenuString[id - 1]);
			if ( buf[2] == '\'' ){
				InsertAliasMacro(hDlg,buf + 3);
			}else if ( buf[2] == 'M' ){
				InsertMenuMacro(hDlg,(TCHAR)((buf[3] == 'E') ? 'E' : 'M'));
				return;
			}
			break;
	}
	p = tstrchr(buf,'\t');
	if ( p != NULL ) *p = '\0';
	SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_REPLACESEL,0,(LPARAM)buf);
}

void InsertMenuSeparator(HWND hDlg,BOOL toolbar)
{
	HMENU hMenu;
	const TCHAR **menustr = InsertMenuSeparatorString;
	int id = 1;

	hMenu = CreatePopupMenu();
	AppendMenu(hMenu,MF_ES,id,GetCText(*menustr));
	if ( !toolbar ){
		AppendMenu(hMenu,MF_ES,++id,GetCText(*++menustr));
		AppendMenu(hMenu,MF_ES,++id,GetCText(*++menustr));
		AppendMenu(hMenu,MF_ES,++id,GetCText(*++menustr));
	}

	id = TrackButtonMenu(hDlg,IDB_MECKEYS,hMenu);
	DestroyMenu(hMenu);
	if ( id ){
		TCHAR buf[MAX_PATH];

		tstrcpy(buf,InsertMenuSeparatorString[id - 1]);
		*tstrchr(buf,'\t') = '\0';
		if ( id <= 2 ){
			SetDlgItemText(hDlg,IDE_EXITEM,buf);
			SetDlgItemText(hDlg,IDE_ALCCMD,NilStr);
			SendMessage(hDlg,WM_COMMAND,IDB_ALCNEW,0);
		}else{
			SendDlgItemMessage(hDlg,IDE_EXITEM,EM_REPLACESEL,0,(LPARAM)buf);
		}
	}
}

BOOL KeymapMenu(HWND hWnd)
{
	if ( PMessageBox(hWnd,GetCText(StrQueryKeyMapMenu),StrCustTitle,
			MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES ){
		return FALSE;
	}
	PPxCommonExtCommand(K_menukeycust,0);
	return TRUE;
}

void SetKeyList(HWND hDlg,const TCHAR *listfirst)
{
	TCHAR buf[CMDLINESIZE];
	HWND hListWnd;

	hOldKeyListDlg = hDlg;
	hListWnd = GetDlgItem(hDlg,IDC_ALCKEYS);
	SendMessage(hListWnd,WM_SETREDRAW,FALSE,0);
	SendMessage(hListWnd,CB_RESETCONTENT,0,0);
	for ( ;; ){
		TCHAR *p;

		if ( (*listfirst == '\0') || (*listfirst == GROUPCHAR) ) break;
		tstrcpy(buf,listfirst);
		p = tstrchr(buf,'\t');
		if ( p == NULL ) break;
		*p = '\0';
		if ( UseLcid == LCID_PPXDEF ){
			p = buf;
		}else{
			TCHAR *p1;

			p++;
			p1 = tstrchr(p,'\t');
			if ( p1 != NULL ) *p1 = '\0';
		}
		SendMessage(hListWnd,CB_ADDSTRING,0,(LPARAM)p);
		listfirst += tstrlen(listfirst) + 2;
	}
	SendMessage(hListWnd,WM_SETREDRAW,TRUE,0);
	InvalidateRect(hListWnd,NULL,FALSE);
}

const TCHAR *GetKeyGroup(int id)
{
	const TCHAR *p;
	int i = 0;
							// 該当グループを検索
	p = KeyList;
	for ( ;; ){
		TCHAR c;

		c = *p;
		p += tstrlen(p) + 2;
		if ( c == GROUPCHAR ){
			if ( i == id ){
				KeyListGroup = p;
				return p;
			}
			i++;
		}
		if ( *p == '\0' ) return NULL; // 該当無し
	}
}

void SelectedKeySubID(HWND hDlg,int id/*,TABLEINFO *tinfo*/)
{
	const TCHAR *p = KeyListGroup;

	if ( hOldKeyListDlg != hDlg ){
		int i;

		i = (int)SendDlgItemMessage(hDlg,IDC_ALCKEYG,CB_GETCURSEL,0,0);
		if ( i == CB_ERR ) return;
		if ( GetKeyGroup(i) == NULL ) return;
		hOldKeyListDlg = hDlg;
		OldKeyListID = id;
	}
	{
		int i = 0;

		for ( ;; ){	// 該当IDを検索
			if ( (*p == '\0') || (*p == GROUPCHAR) ) return;
			if ( i == id ) break;
			i++;
			p += tstrlen(p) + 2;
		}
	}
	SetCommand(hDlg,p/*,tinfo*/);
}

void SelectedKeyGroup(HWND hDlg,int id)
{
	const TCHAR *p;

	if ( (hOldKeyListDlg == hDlg) && (OldKeyListID == id) ) return; // 変更不要
	p = GetKeyGroup(id);
	if ( p == NULL ) return;
	OldKeyListID = id;
	SetKeyList(hDlg,p);
	SendDlgItemMessage(hDlg,IDC_ALCKEYS,CB_SETCURSEL,(WPARAM)-1,0);
}

//------------------------------------------------ 項目を選択したときの表示処理
void SelectItemByIndex(TABLEINFO *tinfo,int index)
{
	LV_ITEM lvi;
	HWND hLVWnd;

	hLVWnd = tinfo->hLVAlcWnd;
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
	lvi.state = 0;
	SendMessage(hLVWnd,LVM_SETITEMSTATE,0,(WPARAM)&lvi);
	lvi.state = LVIS_FOCUSED | LVIS_SELECTED;
	SendMessage(hLVWnd,LVM_SETITEMSTATE,index,(WPARAM)&lvi);
	SendMessage(hLVWnd,LVM_ENSUREVISIBLE,index,FALSE);
}

void FixAddReturnCode(TCHAR *text)
{
	TCHAR *p;

	p = text;
	while ( (p = tstrchr(p,'\n')) != NULL ){ // 改行の補正。\n→\r\nに
		memmove((char *)(TCHAR *)(p + 1),p,TSTRSIZE(p));
		*p = '\r';
		p += 2;
	}
}

void SelectedItemMenu(HWND hDlg,TABLEINFO *tinfo,const TCHAR *keyword)
{
	TCHAR label[MAX_PATH];
	TCHAR para[CMDLINESIZE * 10];

	para[0] = '\0';
	label[0] = '\0';
	if ( keyword == NULL ){
		if ( EnumCustTable(tinfo->index_alc,tinfo->name_type,label,para,sizeof(para)) < 0 ) return;
	}else{
		tinfo->index_alc = 0;
		tstrcpy(label,keyword);
		GetCustTable(tinfo->name_type,label,para,sizeof(para));
	}
	SetDlgItemText(hDlg,IDE_EXITEM,label);
	FixAddReturnCode(para);
	SetDlgItemText(hDlg,IDE_ALCCMD,para);
	SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_SETSEL,EC_LAST,EC_LAST);
}

void SetCommandNameList(HWND hDlg,TABLEINFO *tinfo,const TCHAR *keyname,TCHAR *text)
{
	const TCHAR *listfirst = NULL;
	int id = 0,subid = 0;
	TCHAR type = '\0';

	// ファイル判別→すべてPPc
	// キー割当て→２文字目 + K_tray,K_edit
	if ( tinfo->name_type[0] == 'E' ){	// E_
		type = 'c';
	}else if ( tinfo->name_type[0] == 'H' ){	// HM_ppc/HM_ppv
		type = TinyCharLower(tinfo->name_type[5]);
	}else if ( Isalpha(tinfo->name_type[1]) ){	// ?B_ ,?C_ ,?V_
		type = TinyCharLower(tinfo->name_type[1]);
	}else if ( tinfo->name_type[2] == 'e' ){	// K_edit
		type = 'e';
	}
	if ( keyname != NULL ){
		const TCHAR *p;
		BOOL skip C4701CHECK;

		p = KeyList;
		for ( ;; ){
			if ( *p == GROUPCHAR ){
				// ※必ず始めに実行される
				listfirst = p;
				id++;
				subid = 0;

				if ( !type ){ // 対象が不明
					skip = FALSE;
				}else if ( !Isalpha(*(p + 1)) ){ // 英字でない→共用？
					skip = FALSE;
				}else{	// 英字→各PPx固有
					skip = *(p + 3) != type;
				}
			}else if ( skip == FALSE ){ // C4701ok
				TCHAR *q;

				q = tstrchr(p,'\t'); // Jpn/Eng 間
				if ( q != NULL ){
					while ( *q == '\t' ) q++;
					q = tstrchr(q,'\t'); // Eng/Keyname 間
					if ( q != NULL ){
						while ( *q == '\t' ) q++;
						if ( tstrcmp(q,keyname) == 0 ){
							if ( text != NULL ){
								if ( UseLcid != LCID_PPXDEF ){
									TCHAR *np = tstrchr(p, '\t');
									if ( np != NULL ) p = np + 1;
								}
								while ( (*p != '\t') && (*p != '\0') ){
									*text++ = *p++;
								}
								*text = '\0';
								return;
							}
							id--;
							break;
						}
						subid++;
					}
				}
			}
			p += tstrlen(p) + 2;
			if ( *p == '\0' ){
				keyname = NULL;
				break;
			}
		}
	}
	if ( text != NULL ) return;

	if ( keyname == NULL ){
		id = -1;
		subid = -1;
		listfirst = KeyList;
	}
	listfirst += tstrlen(listfirst) + 2;	// グループ名行をスキップ

	if ( (hOldKeyListDlg != hDlg) || (OldKeyListID != id) ){
		KeyListGroup = listfirst;
		OldKeyListID = id;
		SetKeyList(hDlg,listfirst);
	}
	SendDlgItemMessage(hDlg,IDC_ALCKEYG,CB_SETCURSEL,id,0);
	SendDlgItemMessage(hDlg,IDC_ALCKEYS,CB_SETCURSEL,subid,0);
}

void SelectedItemExecute(HWND hDlg,TABLEINFO *tinfo,const TCHAR *keyword)
{
	TCHAR label[MAX_PATH];
	TCHAR para[CMDLINESIZE * 10],*parap;
	TCHAR buf[CMDLINESIZE];
	const TCHAR *keyname = NULL;

	if ( keyword == NULL ){
		LV_ITEM lvi;
		int sel;

		sel = TListView_GetFocusedItem(tinfo->hLVAlcWnd);
		if ( sel >= 0 ){
			lvi.mask = LVIF_TEXT;
			lvi.pszText = label;
			lvi.cchTextMax = MAX_PATH;
			lvi.iItem = sel;
			lvi.iSubItem = 0;
			ListView_GetItem(tinfo->hLVAlcWnd,&lvi);
		}else{
			label[0] = '\0';
		}
	}else{
		tinfo->index_alc = 0;
		tstrcpy(label,keyword);
	}
	if ( (tinfo->key == 'E') && (label[0] == '/') ){
		 					// ファイル判別→ワイルドカードの調整
		SetDlgItemText(hDlg,IDE_EXITEM,label + 1);
	}else{
		SetDlgItemText(hDlg,IDE_EXITEM,label);
	}
	if ( tinfo->key == 'm' ){ // マウスの時は内容を解析
		MakeMouseDetailText(hDlg,label,buf);
	}else if ( tinfo->key == 'K' ){ // キー割当ての時はキーの説明
		SetKeyComment(hDlg,tinfo,label);
	}

	#define FILLSIZE 32
	if ( tinfo->key == 'B' ){
		parap = (TCHAR *)((BYTE *)para + ((tinfo->name_type[0] == 'H') ? 8 : 4));
	}else{
		parap = para;
	}
	memset(para,0,FILLSIZE);
	GetCustTable(tinfo->name_type,label,para,sizeof(para));
/*
	if ( tinfo->key == 'M' ){	// メニュー
		FixAddReturnCode(para);
		SetDlgItemText(hDlg,IDE_ALCCMD,para);
		SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_SETSEL,EC_LAST,EC_LAST);
		return;
	}
*/
	if ( tinfo->key == 'B' ){	// ボタン
		if ( tinfo->name_type[0] == 'B' ){
			int buttonindex = *(int *)para;

			buttonindex = (buttonindex < 0) ? 0 : buttonindex + 1;
			SendDlgItemMessage(hDlg,IDL_BLIST,LB_SETCURSEL,buttonindex,0);
		}else{
			CharColor = ((COLORREF *)para)[0];
			BackColor = ((COLORREF *)para)[1];
		}
	}else if ( tinfo->key == 'E' ){	// ファイル判別は、判別名検索
		VFSFILETYPE vft;
		TCHAR *ext;

		vft.flags = VFSFT_TYPETEXT;
		vft.typetext[0] = '\0';
		ext = tstrchr(label,'.');
		if ( (label[0] == ':') && (ext != NULL) ){
			VFSGetFileType(ext + 1,NULL,MAX32,&vft);
			if ( vft.typetext[0] == '\0' ){
				*ext = '\0';
				VFSGetFileType(label,NULL,MAX32,&vft);
			}
		}else{
			VFSGetFileType(label,NULL,MAX32,&vft);
		}
		SetDlgItemText(hDlg,IDS_EXITEM,vft.typetext);
	}
	if ( (UTCHAR)parap[0] == EXTCMD_CMD ){	// コマンド
		parap++;
		FixAddReturnCode(parap);
		SetDlgItemText(hDlg,IDE_ALCCMD,parap);
		SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_SETSEL,0,EC_LAST);
		keyname = parap;
	}else{	// コマンド以外→キー割当て
		TCHAR *destptr;
		WORD *keyptr,key;

		if ( (UTCHAR)parap[0] == EXTCMD_KEY ){
			keyptr = (WORD *)&parap[1];
		}else{
			keyptr = (WORD *)parap;
		}
		destptr = buf;
		*destptr++ = '%';
		*destptr++ = 'K';
		*destptr++ = '\"';
		for ( ;; ){
			key = *keyptr++;
			if ( key == 0 ) break;
			if ( destptr > (buf + 3) ) *destptr++ = ' ';
			keyname = destptr;
			PutKeyCode(destptr,key);
			destptr += tstrlen(destptr);
		}
		*destptr = '\0';

		SetDlgItemText(hDlg,IDE_ALCCMD,buf);
		SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_SETSEL,
				(WPARAM)(keyname - buf),(LPARAM)tstrlen(buf));
	}

	SetCommandNameList(hDlg,tinfo,keyname,NULL);
}

void SetListViewItemDetail(TABLEINFO *tinfo,int index,const TCHAR *label,const BYTE *param)
{
	HWND hLVAlcWnd = tinfo->hLVAlcWnd;
	TCHAR labelbuf[MAX_PATH];
	const BYTE *paramptr;
	LV_ITEM lvi;

	lvi.iItem = index;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;

	paramptr = param;
	switch ( tinfo->key ){
		case 'B':
			paramptr = (param + ((tinfo->name_type[0] == 'H') ? 8 : 4));
			break;

		case 'm': {
			lvi.pszText = MakeMouseDetailText(NULL,label,labelbuf);
			ListView_SetItem(hLVAlcWnd,&lvi);
			lvi.iSubItem = 2;
			lvi.pszText = labelbuf;
			ListView_SetItem(hLVAlcWnd,&lvi);
			lvi.iSubItem = 3;
			break;
		}

		case 'K': {
			const TCHAR *p;

			p = label;
			if ( MakeKeyDetailListText(tinfo,&p,labelbuf) ){
				lvi.pszText = labelbuf;
			}else{
				lvi.pszText = (TCHAR *)label;
			}
			ListView_SetItem(hLVAlcWnd,&lvi);
			lvi.iSubItem = 2;
			break;
		}
//		default:
	}

	if ( tinfo->key == 'M' ){ // メニューはそのまま
		lvi.pszText = (TCHAR *)param;
	}else{
		if ( (UTCHAR)paramptr[0] == EXTCMD_CMD ){	// コマンド
			const TCHAR *listp;

			listp = KeyList;
			lvi.pszText = (TCHAR *)paramptr + 1;
			for (;;){
				if ( *listp == '\0' ) break;
				if ( *listp != GROUPCHAR ){
					const TCHAR *p = tstrchr(listp,'\t'),*p2;

					if ( p != NULL ){
						p2 = tstrchr(p + 1,'\t');
						if ( p2 == NULL ) p2 = p;

						if ( tstrcmp(p2 + 1,lvi.pszText) == 0 ){
							if ( (p2 == p) || (UseLcid == LCID_PPXDEF) ){ //JP
								tstrcpy(labelbuf,listp);
								labelbuf[p - listp] = '\0';
							}else{ // EN
								tstrcpy(labelbuf,p + 1);
								labelbuf[p2 - (p + 1)] = '\0';
							}
							lvi.pszText = labelbuf;
							break;
						}
					}
				}
				listp += tstrlen(listp) + 2;
				if ( *listp == '\0' ) break;
			}
		}else{
			const WORD *kp;
			WORD key;
			TCHAR *p;

			lvi.pszText = labelbuf;
			kp = (WORD *)paramptr;
			if ( (UTCHAR)paramptr[0] == EXTCMD_KEY ){
				kp = (WORD *)&paramptr[sizeof(TCHAR)];
			}
			p = labelbuf;
			*p = '\0';
			for ( ;; ){
				key = *kp++;
				if ( !key ) break;
				if ( p != labelbuf ) *p++ = ' ';
				PutKeyCode(p,key);
				p += tstrlen(p);
			}
			SetCommandNameList(NULL,tinfo,labelbuf,labelbuf);
		}
	}
	ListView_SetItem(hLVAlcWnd,&lvi);
}

void SelectedItem(HWND hDlg,TABLEINFO *tinfo,TCHAR *keyword)
{
	if ( tinfo->key == 'M' ){
		SelectedItemMenu(hDlg,tinfo,keyword);
	}else{
		SelectedItemExecute(hDlg,tinfo,keyword);
	}
	EnableSetButton(hDlg,FALSE);
}

void SetButtonPositionList(HWND hDlg,const TCHAR *label)
{
	HWND hCmb;
	DWORD ppType,button;
	int bindex;
	const struct MouseTypeListStruct *mtl;

	bindex = (int)SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_GETCURSEL,0,0);
	button = 1 << (SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_GETITEMDATA,(WPARAM)bindex,0) + CE_SHIFT);

	hCmb = GetDlgItem(hDlg,IDC_ALCMOUSET);
	SendMessage(hCmb,WM_SETREDRAW,FALSE,0);
	SendMessage(hCmb,CB_RESETCONTENT,0,0);

	if ( label[2] == 't' ){
		ppType = CE_TRAY;
	}else{
		ppType = (label[2] == 'c') ? CE_PPC : CE_PPV;
	}

	for ( mtl = MouseTypeList ; mtl->enables ; mtl++ ){
		if ( (mtl->enables & ppType) && (mtl->enables & button) ){
			SendMessage(hCmb,CB_ADDSTRING,0,(LPARAM)GetCText(mtl->str));
		}
	}
	SendMessage(hCmb,WM_SETREDRAW,TRUE,0);
}


//------------------------------------------------ 種類を選択したときの表示処理
void SelectAlcView(TABLEINFO *tinfo,int index)
{
	LV_ITEM lvi;

	lvi.mask = LVIF_STATE;
	lvi.state = LVIS_FOCUSED | LVIS_SELECTED;
	lvi.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	SendMessage(tinfo->hLVTypeWnd,LVM_SETITEMSTATE,index,(LPARAM)&lvi);
	SendMessage(tinfo->hLVTypeWnd,LVM_ENSUREVISIBLE,index,TRUE);
}

void SelectedType(HWND hDlg,TABLEINFO *tinfo)
{
	int size;
	TCHAR label[MAX_PATH],labelbuf[MAX_PATH];
	HWND hLVTypeWnd = tinfo->hLVTypeWnd,hLVAlcWnd;
	LV_ITEM lvi;

	if ( hLVTypeWnd == NULL ) return;

	lvi.mask = LVIF_TEXT;
	lvi.pszText = label;
	lvi.cchTextMax = MAX_PATH;
	lvi.iSubItem = 0;
	if ( tinfo->index_type < 0 ){ // 選択されていないときは検索する
		TCHAR *p;

		GetControlText(hDlg,IDE_EXTYPE,labelbuf,TSIZEOF(labelbuf) - 2);
		lvi.iItem = 0;
		for ( ;; ){
			if ( ListView_GetItem(hLVTypeWnd,&lvi) == FALSE ) break;
			p = tstrchr(label,'/');
			if ( p != NULL ){
				p++;
			}else{
				p = label;
			}
			if ( !tstricmp(p,labelbuf) ){
				tinfo->index_type = lvi.iItem;
				break;
			}
			lvi.iItem++;
		}
		if ( tinfo->index_type >= 0 ){
			SelectAlcView(tinfo,tinfo->index_type);
		}else{
			return;
		}
	}
										// Type 設定
	label[0] = '\0';
	lvi.iItem = tinfo->index_type;
	ListView_GetItem(hLVTypeWnd,&lvi);
	if ( label[0] == '\0' ) return;

	{
		TCHAR *p = tstrchr(label,'/');
		if ( p != NULL ){
			p++;
		}else{
			p = label;
		}
		tstrcpy(tinfo->name_type,p);
		SetDlgItemText(hDlg,IDE_EXTYPE,p);
	}

	if ( tinfo->key == 'm' ){ // マウスのボタンリストを調整
		SetButtonPositionList(hDlg,label);
	}else if ( tinfo->key == 'B' ){ // ツールバーのボタン一覧を調整
		BOOL showH = TRUE,showB = FALSE;

		if ( tinfo->name_type[0] == 'B' ){
			LoadBar(hDlg);
			showB = TRUE;
			showH = FALSE;
		}
		ShowDlgWindow(hDlg,IDB_MECKEYS,showB);
		ShowDlgWindow(hDlg,IDX_BNOBMP,showB);
		ShowDlgWindow(hDlg,IDB_BREF,showB);
		ShowDlgWindow(hDlg,IDL_BLIST,showB);
		ShowDlgWindow(hDlg,IDB_HMCHARC,showH);
		ShowDlgWindow(hDlg,IDB_HMBACKC,showH);
	}
										// Item 設定
	hLVAlcWnd = tinfo->hLVAlcWnd;
	if ( hLVAlcWnd != NULL ){
		LV_COLUMN lvc;
		LV_ITEM lvi;
		TCHAR param[VFPS];
		int index,memoindex = 0;

		SendMessage(hLVAlcWnd,WM_SETREDRAW,FALSE,0);
		SendMessage(hLVAlcWnd,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
		ListView_DeleteAllItems(hLVAlcWnd);
		ListView_DeleteColumn(hLVAlcWnd,0);
		ListView_DeleteColumn(hLVAlcWnd,0);
		ListView_DeleteColumn(hLVAlcWnd,0);
		SelectItemLV = -1;

		if ( tinfo->key == 'K' ){
			memoindex = 2;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (TCHAR *)GetCText(StrLabelTargetKey);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,0,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelTargetKeyName);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,1,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelDetail);
			lvc.cx = 800;
			ListView_InsertColumn(hLVAlcWnd,2,&lvc);
		}else if ( tinfo->key == 'm' ){
			memoindex = 3;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (TCHAR *)GetCText(StrLabelTargetButton);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,0,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelTargetButtonName);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,1,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelTargetArea);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,2,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelDetail);
			lvc.cx = 800;
			ListView_InsertColumn(hLVAlcWnd,3,&lvc);
		}else if ( tinfo->key == 'M' ){
			memoindex = 1;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (TCHAR *)GetCText(StrLabelMenuItemName);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,0,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelDetail);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,1,&lvc);
		}else if ( tinfo->key == 'E' ){
			memoindex = 1;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (TCHAR *)GetCText(StrLabelTypeName);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,0,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelDetail);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,1,&lvc);
		}else if ( tinfo->key == 'B' ){
			memoindex = 1;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (TCHAR *)GetCText(StrLabelItemName);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,0,&lvc);
			lvc.pszText = (TCHAR *)GetCText(StrLabelDetail);
			lvc.cx = 100;
			ListView_InsertColumn(hLVAlcWnd,1,&lvc);
		}

		lvi.mask = LVIF_TEXT | LVIF_PARAM; // 「新規」欄
		lvi.lParam = 0;

		lvi.pszText = (TCHAR *)NilStr;
		lvi.iItem = 0;
		lvi.iSubItem = 0;
		lvi.iItem = ListView_InsertItem(hLVAlcWnd,&lvi);

		lvi.mask = LVIF_TEXT;
		lvi.pszText = (TCHAR *)GetCText(StrMenuNewMemo);
		lvi.iSubItem = memoindex;
		ListView_SetItem(hLVAlcWnd,&lvi);

		index = 0;
		ListViewCounter = 1;

		param[TSIZEOFSTR(param)] = '\0';
		for ( ;; ){
			memset(param,0,FILLSIZE);
			size = EnumCustTable(index,tinfo->name_type,label,param,SIZEOFTSTR(param));
			if ( 0 > size ) break;

			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.lParam = ListViewCounter++;
			lvi.pszText = label;
			lvi.iItem++;
			lvi.iSubItem = 0;

			SetListViewItemDetail(tinfo,
					ListView_InsertItem(hLVAlcWnd,&lvi),label,(BYTE *)param);
			index++;
		}
		SendMessage(hLVAlcWnd,LVM_SETCOLUMNWIDTH,0,LVSCW_AUTOSIZE);
		SendMessage(hLVAlcWnd,LVM_SETCOLUMNWIDTH,1,LVSCW_AUTOSIZE);
		SendMessage(hLVAlcWnd,WM_SETREDRAW,TRUE,0);

		lvi.mask = LVIF_STATE;
		lvi.iItem = 0;
		lvi.state = LVNI_FOCUSED | LVIS_SELECTED;
		lvi.stateMask = LVNI_FOCUSED | LVIS_SELECTED;
		lvi.iSubItem = 0;
		ListView_SetItem(hLVAlcWnd,&lvi);

		EnableDlgWindow(hDlg,IDB_MEUP,TRUE);
		EnableDlgWindow(hDlg,IDB_MEDW,TRUE);
	}

	SelectedItem(hDlg,tinfo,NULL);
										// コメント取得
	label[0] = '\0';
	GetCustTable(T("#Comment"),tinfo->name_type,label,MAX_PATH);
	SetDlgItemText(hDlg,IDE_ALCCMT,label);
	EnableDlgWindow(hDlg,IDB_ALCCMT,FALSE);
}

//---------------------------------------------------------- 種類の一覧登録処理
void EnumType(TABLEINFO *tinfo)
{
	int count,size;
	TCHAR name[MAX_PATH];
	struct LABELTEXT *list;
	HWND hLVTypeWnd = tinfo->hLVTypeWnd;
	LV_ITEM lvi;
	LV_COLUMN lvc;

	if ( hLVTypeWnd == NULL ) return;

	lvi.mask = LVIF_TEXT; // 「新規」欄
	lvi.pszText = name;
	lvi.iItem = 0;
	lvi.iSubItem = 0;

	SendMessage(hLVTypeWnd,WM_SETREDRAW,FALSE,0);
	SendMessage(hLVTypeWnd,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
	ListView_DeleteAllItems(hLVTypeWnd);
	ListView_DeleteColumn(hLVTypeWnd,0);
	lvc.mask = LVCF_WIDTH;
	lvc.cx = 200;
	ListView_InsertColumn(hLVTypeWnd,0,&lvc);
											// 使用済みを検索
	for( count = 0 ; ; count++ ){
		size = EnumCustData(count,name,NULL,0);
		if ( 0 > size ) break;

		if ( CheckTypeName(tinfo->key,name) == FALSE ) continue;
		for ( list = textlists ; list->name ; list++ ){
			if ( !tstrcmp( name,list->name ) ){
				wsprintf(name,T("%s /%s"),GetCText(list->text),list->name);
				break;
			}
		}
		ListView_InsertItem(hLVTypeWnd,&lvi);
	}
											// 未使用を検索
	for ( list = textlists ; list->name ; list++ ){
		if ( CheckTypeName(tinfo->key,list->name) == FALSE ) continue;
		if ( IsExistCustData(list->name) ) continue;
		wsprintf(name,T("%s /%s"),GetCText(list->text),list->name);
		ListView_InsertItem(hLVTypeWnd,&lvi);
	}
	SendMessage(hLVTypeWnd,LVM_SETCOLUMNWIDTH,0,LVSCW_AUTOSIZE);
	SendMessage(hLVTypeWnd,WM_SETREDRAW,TRUE,0);
}

void HideMenuColor(HWND hDlg,COLORREF *color)
{
	if ( PPxDialogBoxParam(hInst,MAKEINTRESOURCE(IDD_SCOLOR),
			hDlg,HideMenuColorDlgBox,(LPARAM)color) > 0 ){
		EnableSetButton(hDlg,TRUE);
	}
}

void FixCutReturnCode(TCHAR *text)
{
	TCHAR *src,*dest;

	src = tstrchr(text,'\r');
	if ( src == NULL ){
		dest = text + tstrlen(text);
	}else{ // \r を除去
		dest = src;
		src++;
		while ( *src ){
			if ( *src != '\r' ) *dest++ = *src;
			src++;
		}
	}
	while ( (dest > text) && (*(dest - 1) == '\n') ) dest--; //末尾の空行を削除
	*dest = '\0';
}

// ワイルドカード用の処理を行う
void FixWildItemName(TCHAR *item)
{
	TCHAR *sp;

	if ( !((item[0] == '*') && (item[1] == '\0')) ){
		sp = item;
		if ( *sp == ':' ) sp++;
		for ( ; *sp ; sp++ ){
			if ( (*sp >= '!') && (*sp <='?') ){
				if ( tstrchr(DetectWildcardLetter,*sp) != NULL ){
					memmove(&item[1],&item[0],TSTRSIZE(item));
					item[0] = '/';
					return;
				}
			}
		}
	}
	tstrupr(item);
}

//---------------------------------------------------------------- 項目登録処理
void AddItem(HWND hDlg,TABLEINFO *tinfo,DWORD mode)
{
	TCHAR typebuf[MAX_PATH],item[MAX_PATH],para[CMDLINESIZE * 10 + 1],*type;
	TCHAR olditemname[MAX_PATH];
	int index;
	size_t size;
	HWND hLVAlcWnd;
	LV_ITEM lvi;

	GetControlText(hDlg,IDE_EXITEM,item,TSIZEOF(item));
	type = typebuf + 2;
	GetControlText(hDlg,IDE_EXTYPE,type,TSIZEOF(typebuf) - 2);
	if ( *type == '\0' ) return;
	if ( *item == '\0' ) return;

	if ( CheckTypeName(tinfo->key,type) == FALSE ){
		typebuf[0] = tinfo->key;
		typebuf[1] = '_';
		type = typebuf;
	}
	hLVAlcWnd = tinfo->hLVAlcWnd;
	index = TListView_GetFocusedItem(hLVAlcWnd);

	if ( index >= 0 ){
		lvi.mask = LVIF_TEXT;
		lvi.pszText = olditemname;
		lvi.cchTextMax = MAX_PATH;
		lvi.iItem = index;
		lvi.iSubItem = 0;
		ListView_GetItem(hLVAlcWnd,&lvi);
	}else{
		index = 0;
		olditemname[0] = '\0';
	}

	if ( tinfo->key != 'M' ){
		TCHAR *parap;

		if ( tinfo->key == 'E' ) FixWildItemName(item);
		if ( tinfo->key == 'B' ){
			if ( tinfo->name_type[0] == 'H' ){ // 隠しメニューは色を保存
				((COLORREF *)para)[0] = CharColor;
				((COLORREF *)para)[1] = BackColor;
				parap = (TCHAR *)((BYTE *)para + 8);
			}else{ // ツールバーはアイコンIDを保存
				int buttonindex = SendDlgItemMessage(hDlg,IDL_BLIST,LB_GETCURSEL,0,0);
				if ( buttonindex == 0 ){ // 非表示
					size_t itemlen;

					// ボタンテキストがないときは、追加する
					itemlen = tstrlen(item);
					if ( (tstrchr(item,'/') == NULL) &&
						 (itemlen < (MAX_PATH - 2)) ){
						tstrcat(item,T("/"));
						SetDlgItemText(hDlg,IDE_EXITEM,item);
					}
					buttonindex = -2;
				}else{
					buttonindex = buttonindex - 1;
				}
				*(int *)para = buttonindex;
				parap = (TCHAR *)((BYTE *)para + sizeof(DWORD));
			}
		}else{
			parap = para;
		}
		{
			const TCHAR *src;
			WORD *dst,keybuf[CMDLINESIZE];

			parap[0] = EXTCMD_CMD;
			parap[1] = '\0';
			GetDlgItemText(hDlg,IDE_ALCCMD,parap + 1,TSIZEOF(para) - 1);
			FixCutReturnCode(parap + 1);
			size = TSTRSIZE(parap);

			src = parap + 1;
			if ( SkipSpace(&src) == '%' ){
				if ( *(src + 1) == 'K' ){
					src += 2;
					if ( SkipSpace(&src) == '\"' ){
						src++;
						dst = keybuf;

						for ( ;; ){
							int key;

							SkipSpace(&src);
							if ( *src == '\"' ){
								src++;
								if ( SkipSpace(&src) != '\0' ){ // \" は末尾？
									break;
								}
							}
							if ( *src == '\0' ){ // %K のみだったので key 扱い
								*dst++ = 0;
								*parap++ = EXTCMD_KEY;
								size = (dst - keybuf) * sizeof(WORD);
								memcpy(parap,keybuf,size);
								break;
							}
							key = GetKeyCode((const TCHAR **)&src);
							if ( key < 0 ) break;
							*dst++ = (WORD)key;
						}
					}
				}
			}
		}
		size += TSTROFF(parap - para); // ヘッダ分を加算
		if ( tinfo->key == 'K' ){
			int i;

			i = CheckRegistKey(item,item,type);
			if ( i < 0 ){
				XMessage(hDlg,StrCustTitle,XM_GrERRld,MES_EKFT);
				return;
			}else if ( i == CHECKREGISTKEY_WARNKEY ){
				XMessage(hDlg,StrCustTitle,XM_ImWRNld,GetCText(StrWarnBadAssc));
			}
		}
	}else{									// メニューのとき
		para[0] = '\0';
		GetDlgItemText(hDlg,IDE_ALCCMD,para,TSIZEOF(para) - 1);
		FixCutReturnCode(para);
		size = TSTRSIZE(para);
		if ( (index == 0) && (mode == IDB_ALCSET) ){
			index = 0x7fffffff; // 末尾に新規挿入
		}
	}
	if ( !IsExistCustTable(type,item) || (*item == '-') || (*item == '|') ){
		InsertCustTable(type,item,index,para,size);
	}else{
		SetCustTable(type,item,para,size);
	}

	if ( (mode == IDB_ALCSET) && (olditemname[0] != '\0') && tstricmp(olditemname,item) ){ // 項目名入替
		DeleteCustTable(type,olditemname,0); // 元は削除

		lvi.mask = LVIF_TEXT;
		lvi.pszText = item;
		lvi.iSubItem = 0;
		ListView_SetItem(hLVAlcWnd,&lvi);

		SetListViewItemDetail(tinfo,lvi.iItem,item,(BYTE *)para);
	}else{ // 新規
		LV_FINDINFO lvfi;

		EnumType(tinfo);
		SelectedType(hDlg,tinfo);

		lvfi.flags = LVFI_STRING;
		lvfi.psz = item;
		index = ListView_FindItem(hLVAlcWnd,-1,&lvfi);
		SelectItemByIndex(tinfo,index);
	}
	EnableSetButton(hDlg,FALSE);
	Changed(hDlg);
	SetFocus(hLVAlcWnd);
}

//---------------------------------------------------------------- 項目削除処理
void DeleteItem(HWND hDlg,TABLEINFO *tinfo)
{
	TCHAR type[MAX_PATH],item[MAX_PATH];
	int count,index;

	GetControlText(hDlg,IDE_EXTYPE,type,TSIZEOF(type));
	if ( CheckTypeName(tinfo->key,type) == FALSE ) return;
												// 項目が選択されているか確認
	// ※「新規」があるので -1
	count = ListView_GetItemCount(tinfo->hLVAlcWnd);
	if ( count < 0 ) return;
	index = TListView_GetFocusedItem(tinfo->hLVAlcWnd);
	if ( index < 0 ) return;
	if ( (index == 0) && (count > 1) ){
		wsprintf(item,GetCText(StrQueryDeleteTable),type);
		if ( PMessageBox(hDlg,item,StrCustTitle,
				MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES ){
			return;
		}
		count = 1; // 完全削除状態に
	}

	if ( count <= 2 ){ // 残り一つ(新規を含めると2)なら完全削除
		DeleteCustData(type);
		EnumType(tinfo);
	}else if ( tinfo->key != 'M' ){ // 一般削除
		GetControlText(hDlg,IDE_EXITEM,item,TSIZEOF(item));
		if ( tinfo->key == 'E' ) FixWildItemName(item);
		if ( *item ) DeleteCustTable(type,item,0);
	}else{ // メニューの削除
		DeleteCustTable(type,NULL,index - 1); // 「新規」分を減らす
	}
	SelectedType(hDlg,tinfo);
	if ( count > 2 ){
		SelectItemByIndex(tinfo,index);
	}
	Changed(hDlg);
	return;
}

//-------------------------------------------------------- 項目を一つ上下に移動
void ItemUpDown(HWND hDlg,TABLEINFO *tinfo,int offset)
{
	TCHAR label[MAX_PATH],para[CMDLINESIZE * 11];
	int size,newpos;

	if ( offset < 0 ){
		if ( tinfo->index_alc <= 0 ) return;
	}else{
		int maxi;

		maxi = ListView_GetItemCount(tinfo->hLVAlcWnd) - 2;
		if ( tinfo->index_alc < 0 ) return;
		if ( tinfo->index_alc >= maxi ) return;
	}
	newpos = tinfo->index_alc + offset;

	size = EnumCustTable(tinfo->index_alc,tinfo->name_type,label,para,sizeof(para));
	if ( size < 0 ) return;
	DeleteCustTable(tinfo->name_type,NULL,tinfo->index_alc);
	InsertCustTable(tinfo->name_type,label,newpos,para,size);

	EnumType(tinfo);
	SelectedType(hDlg,tinfo);
	Changed(hDlg);

	SelectItemByIndex(tinfo,newpos + 1);
	tinfo->index_alc = newpos;
}

//-------------------------------------------------------------------- 動作試験
void TestTable(HWND hDlg,TABLEINFO *tinfo)
{
	TCHAR label[MAX_PATH];
	HMENU hMenu;
	DWORD id = 1;

	if ( tinfo->key != 'M' ){
		Test();
		return;
	}
										// メニューの場合はメニュー表示
	GetControlText(hDlg,IDE_EXTYPE,label,TSIZEOF(label));

	hMenu = PP_AddMenu(NULL,hDlg,NULL,&id,label,NULL);
	if ( hMenu != NULL){
		TrackButtonMenu(hDlg,IDB_TEST,hMenu);
		DestroyMenu(hMenu);
	}
	return;
}

void InitMouseList(HWND hDlg)
{
	int index = 0,cbindex;

	for (;;){
		cbindex = SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_ADDSTRING,0,(LPARAM)GetCText(MouseButtonList[index]));
		SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_SETITEMDATA,(WPARAM)cbindex,(LPARAM)index);
		index++;
		if ( MouseButtonList[index] == NULL ) break;
	}
}

void SetKeyGroup(HWND hDlg)
{
	const TCHAR *p;
	TCHAR buf[MAX_PATH];

	p = KeyList;
	for ( ;; ){
		if ( *p == GROUPCHAR ){
			TCHAR *pb;

			tstrcpy(buf,p + 1);
			pb = tstrchr(buf,'\t');
			if ( pb == NULL ){
				pb = buf;
			}else{
				if ( UseLcid == LCID_PPXDEF ){
					*pb = '\0';
					pb = buf;
				}else{
					pb++;
				}
			}
			SendDlgItemMessage(hDlg,IDC_ALCKEYG,CB_ADDSTRING,0,(LPARAM)pb);
		}
		p += tstrlen(p) + 2;
		if ( *p == '\0' ) return;
	}
}

void SetComment(HWND hDlg,TABLEINFO *tinfo)
{
	TCHAR comment[MAX_PATH];

	comment[0] = '\0';
	GetControlText(hDlg,IDE_ALCCMT,comment,TSIZEOF(comment));
	if ( NO_ERROR == SetCustTable(T("#Comment"),tinfo->name_type,comment,TSTRSIZE(comment)) ){
		EnableDlgWindow(hDlg,IDB_ALCCMT,FALSE);
		Changed(hDlg);
	}
}

// ボタンの種類を指定
void SelectedMouseButton(HWND hDlg,TABLEINFO *tinfo,int index)
{
	TCHAR buf[MAX_PATH],item[MAX_PATH],*src,*dest;
	LV_ITEM lvi;

	GetControlText(hDlg,IDE_EXITEM,item,TSIZEOF(item));
	src = item;
	dest = buf;
	while ( (*src != '\0') && !Isalpha(*src) ) *dest++ = *src++;

	if ( (index == GESTUREID) && (tinfo->name_type[1] != 'C') && (tinfo->name_type[1] != 'V') ){
		SendDlgItemMessage(hDlg,IDC_ALCMOUSEB,CB_SETCURSEL,1,0);
		return;
	}

	tstrcpy(dest,GetCText(MouseButtonList[index]));
	dest = tstrchr(dest,' ');
	if ( dest == NULL ){
		dest = buf + tstrlen(buf);
	}
	*dest++= '_';
	*dest = '\0';
	if ( ShowMouseSetting(hDlg,index != GESTUREID) ){
		src = tstrchr(item,'_');
		if ( src != NULL ){
			tstrcpy(dest,src + 1);
		}
	}
	SetDlgItemText(hDlg,IDE_EXITEM,buf);

	lvi.mask = LVIF_TEXT;
	lvi.pszText = buf;
	lvi.cchTextMax = MAX_PATH;
	lvi.iItem = tinfo->index_type;
	lvi.iSubItem = 0;
	ListView_GetItem(tinfo->hLVTypeWnd,&lvi);
	SetButtonPositionList(hDlg,buf);
}

// クリックの場所を指定
void SelectedMouseType(HWND hDlg,int index)
{
	TCHAR item[MAX_PATH],*p;

	GetControlText(hDlg,IDE_EXITEM,item,TSIZEOF(item));
	p = tstrchr(item,'_');
	if ( p == NULL ){
		p = item + tstrlen(item);
		*p = '_';
	}
	p++;

	SendDlgItemMessage(hDlg,IDC_ALCMOUSET,CB_GETLBTEXT,(WPARAM)index,(LPARAM)p);
	p = tstrchr(p,' ');
	if ( p != NULL ) *p = '\0';
	SetDlgItemText(hDlg,IDE_EXITEM,item);
}

void LoadBar(HWND hDlg)
{
	TCHAR type[VFPS],name[VFPS];
	HWND hListWnd;
	int i;
	TBADDBITMAP tbab;

	tstrcpy(((TOOLBARCUSTTABLESTRUCT *)name)->text + 1,T("toolbar.bmp"));
	GetControlText(hDlg,IDE_EXTYPE,type,TSIZEOF(type));
	if ( NO_ERROR != GetCustTable(type,T("@"),name,sizeof(name)) ){
		name[0] = '\0';
	}else{
		VFSFixPath(name,((TOOLBARCUSTTABLESTRUCT *)name)->text + 1,NULL,VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}
	if ( tstrcmp(BarBmpInfo.filename,name) == 0 ) return;
	DeleteObject(BarBmpInfo.hBmp);
								// ツールバーのイメージを取得する
	LoadToolBarBitmap(hDlg,name,&tbab,&BarBmpInfo.barsize);
	if ( tbab.hInst != NULL ){
		BarBmpInfo.hBmp = LoadBitmap(tbab.hInst,MAKEINTRESOURCE(tbab.nID));
	}else{
		BarBmpInfo.hBmp = (HBITMAP)tbab.nID;
	}
	tstrcpy(BarBmpInfo.filename,name);
	BarBmpInfo.items = BarBmpInfo.barsize.cx / BarBmpInfo.barsize.cy;

	hListWnd = GetDlgItem(hDlg,IDL_BLIST);
	SendMessage(hListWnd,LB_RESETCONTENT,0,0);

	SendMessage(hListWnd,LB_SETCOLUMNWIDTH,(WPARAM)(BarBmpInfo.barsize.cy + 2),0);
	SendMessage(hListWnd,LB_SETITEMHEIGHT,0,(LPARAM)((BarBmpInfo.barsize.cy + 2) ));
	for ( i = BarBmpInfo.items + 1 ; i ; i-- ){ // 非表示分を含めて追加
		SendMessage(hListWnd,LB_ADDSTRING,0,(LONG)1);
	}
}

void DrawBar(DRAWITEMSTRUCT *lpdis)
{
	RECT rect;

	if ( lpdis->itemAction & (ODA_DRAWENTIRE | ODA_SELECT) ){
		// ビットマップがないとき用の枠の描画
		FrameRect(lpdis->hDC,&lpdis->rcItem,GetStockObject(WHITE_BRUSH));

		if ( BarBmpInfo.hBmp == NULL ) return;
		if ( lpdis->itemID > 0 ){
			HGDIOBJ hOldBmp;
			HDC hDC;

			hDC = CreateCompatibleDC(lpdis->hDC);
			hOldBmp = SelectObject(hDC,BarBmpInfo.hBmp);

			BitBlt(lpdis->hDC,lpdis->rcItem.left + 1,lpdis->rcItem.top + 1,
					BarBmpInfo.barsize.cy,BarBmpInfo.barsize.cy,hDC,
					(lpdis->itemID - 1) * BarBmpInfo.barsize.cy,0,SRCCOPY);

			SelectObject(hDC,hOldBmp);
			DeleteDC(hDC);
		}
		if ( lpdis->itemState & ODS_SELECTED ){ // 選択枠(2pixel幅)を描画
			FrameRect(lpdis->hDC,&lpdis->rcItem,GetStockObject(BLACK_BRUSH));

			rect.left = lpdis->rcItem.left + 1;
			rect.top = lpdis->rcItem.top + 1;
			rect.right = lpdis->rcItem.right - 1;
			rect.bottom = lpdis->rcItem.bottom - 1;
			FrameRect(lpdis->hDC,&rect,GetStockObject(LTGRAY_BRUSH));
		}
	}
	if ( lpdis->itemAction & ODA_FOCUS ){ // フォーカス枠(2pixel幅の点線)を描画
		int i;

		for ( i = 0 ; i < 2 ; i++ ){
			rect.left = lpdis->rcItem.left + i;
			rect.top = lpdis->rcItem.top + i;
			rect.right = lpdis->rcItem.right - i;
			rect.bottom = lpdis->rcItem.bottom - i;
			DrawFocusRect(lpdis->hDC,&rect);
		}
	}
}

void SetButtonCommand(HWND hDlg,WPARAM wParam,LPARAM lParam,TABLEINFO *tinfo)
{
	int index = -1;
	TCHAR buf[CMDLINESIZE];
	const TCHAR *command;

	if ( GetListCursorIndex(wParam,lParam,&index) == 0 ) return;

	// 既定のコマンド・コメントを設定する
	index--;
	if ( (index < 0) || (index >= ToolBarDefaultCommandCount) ) return;
	command = ToolBarDefaultCommand[index][0];

	SetDlgItemText(hDlg,IDE_EXITEM,GetCText(ToolBarDefaultCommand[index][1]));
	SetCommandNameList(hDlg,tinfo,command,NULL);
	if ( command[0] != '*' ){ // キー割当て
		wsprintf(buf,T("%%K\"%s"),command);
		command = buf;
	}
	SetDlgItemText(hDlg,IDE_ALCCMD,command);
}

void InitTablePage(HWND hDlg,TABLEINFO *tinfo)
{
	tinfo->hLVAlcWnd = GetDlgItem(hDlg,IDV_ALCLIST);
	tinfo->hLVTypeWnd = GetDlgItem(hDlg,IDV_EXTYPE);
	tinfo->ListViewLastSort = 0;
	if ( KeyList == NULL ){
		KeyList = LoadTextResource(MAKEINTRESOURCE(DEFKEYLIST));
#if 0		// キーコードの正当性チェック
		{
			const TCHAR *p,*group;
			TCHAR buf[100];
			int key;
			const TCHAR *err = NULL;

			p = KeyList;
			for ( ;; ){
				if ( *p == GROUPCHAR ){
					group = p;
				}else{
					TCHAR *q,*r;

					q = tstrchr(p,'\t');
					if ( q != NULL ){
						while ( *q == '\t' ) q++;
						q = tstrchr(q,'\t');
						if ( q != NULL ){
							while ( *q == '\t' ) q++;
							r = q;
							if ( (*q != '%') && (*q != '*') && (*q != '>') ){
								key = GetKeyCode((const TCHAR **)&q);
								if ( *q ) err = T("InitTablePage Error - KeyCode");
//								↓登録済みキーでなければエラー
								if ( !(key & (K_raw | K_ex)) ){
									if ( (key != 'P') &&
										(key != (K_s | 'P')) && (key != 'E') ){
										err = T("InitTablePage Error - UnknownKey");
									}
								}
								PutKeyCode(buf,key);
								if ( tstrcmp(buf,r) ){
									err = T("InitTablePage Error - KeyUnmatch");
								}
							}
						}
					}
					if ( err != NULL ){
						XMessage(NULL,NULL,XM_DbgDIA,T("%s\n%s\n%s"),err,group,p);
						err = FALSE;
					}
				}
				p += tstrlen(p) + 2;
				if ( !*p ) break;
			}
		}
#endif
	}
	if ( tinfo->key == 'm' ) InitMouseList(hDlg);
	SetKeyGroup(hDlg);

	SendDlgItemMessage(hDlg,IDE_EXTYPE,EM_LIMITTEXT,MAX_PATH - 1,0);
	SendDlgItemMessage(hDlg,IDE_ALCCMT,EM_LIMITTEXT,MAX_PATH - 1,0);
	SendDlgItemMessage(hDlg,IDE_EXITEM,EM_LIMITTEXT,MAX_PATH - 1,0);
	SendDlgItemMessage(hDlg,IDE_ALCCMD,EM_LIMITTEXT,(WPARAM)CMDLINESIZE - 1,0);
	if ( OSver.dwMajorVersion >= 6 ){
		PPxRegistExEdit(NULL,GetDlgItem(hDlg,IDE_ALCCMD),CMDLINESIZE - 1,NULL,
				PPXH_GENERAL, 0, PPXEDIT_USEALT | PPXEDIT_NOWORDBREAK);
	}

	SendDlgItemMessage(hDlg,IDE_ALCCMT,EM_SETCUEBANNER,0,(LPARAM)GetCTextW(StrCommentInfo));

	EnumType(tinfo);

	if ( FirstTypeName == NULL ){
		const WCHAR *iteminfo;

		switch ( tinfo->key ){
			case 'm':
				FirstTypeName = T("MC_click");
				iteminfo = L"x_xxx\0x_xxx";
				break;

			case 'M':
				FirstTypeName = T("M_pjump");
				iteminfo = L"表示項目\0item name";
				break;

			case 'B':
				FirstTypeName = T("B_cdef");
				iteminfo = L"[ボタンテキスト/]項目名\0[btn. text/]tip name";
				break;

			case 'E':
				FirstTypeName = T("E_cr");
				iteminfo = L"ext\0ext";
				break;

			case 'K':
				FirstTypeName = T("KC_main");
				iteminfo = L"キー割当て\0key name";
				break;

			default:
				iteminfo = NULL;
		}
		if ( iteminfo != NULL ){
			SendDlgItemMessage(hDlg,IDE_EXITEM,EM_SETCUEBANNER,0,(LPARAM)GetCTextW(iteminfo));
		}
	}

	tinfo->index_type = 0;
	tinfo->index_alc = 0;
	if ( FirstTypeName == NULL ){
		SelectAlcView(tinfo,tinfo->index_type);
		SelectedType(hDlg,tinfo);
	}else{
		tinfo->index_type = LB_ERR;
		SetDlgItemText(hDlg,IDE_EXTYPE,FirstTypeName);
		SelectedType(hDlg,tinfo);
		if ( FirstItemName == NULL ){
			tinfo->index_alc = 0;
		}else{
			TCHAR *p;

			p = FirstItemName;
			if ( *p == '#' ){
				p++;
				tinfo->index_alc = GetNumber((const TCHAR **)&p);
				if ( tinfo->index_alc == LB_ERR ){
					SetDlgItemText(hDlg,IDE_EXITEM,FirstItemName);
				}else{
					tinfo->index_alc++;  // 新規枠
				}
			}
		}
		FirstTypeName = NULL;
		FirstItemName = NULL;
	}
	SelectItemByIndex(tinfo,tinfo->index_alc);
}

INT_PTR CALLBACK TablePage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam,TABLEINFO *tinfo)
{
	LV_ITEM lvi;
	TCHAR buf[CMDLINESIZE];

	switch (msg){
		case WM_INITDIALOG:
			InitTablePage(hDlg,tinfo);
			break;

		case WM_CONTEXTMENU:
			if ( GetDlgCtrlID((HWND)wParam) == IDB_ALCSET ){
				HMENU hPopMenu = CreatePopupMenu();

				AppendMenu(hPopMenu,MF_ES,1,GetCText(StrMenuNew));

				if ( 0 < TrackButtonMenu(hDlg,IDB_ALCSET,hPopMenu) ){
					AddItem(hDlg,tinfo,IDB_ALCNEW);
				}
				DestroyMenu(hPopMenu);
				break;
			}
			if ( (HWND)wParam == hDlg ) break;
			// WM_HELP へ
		case WM_HELP:
			PPxHelp(hDlg,HELP_CONTEXT,tinfo->helpID);
			break;

		case WM_DRAWITEM:
			if ( wParam == IDL_BLIST ) DrawBar((DRAWITEMSTRUCT *)lParam);
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_ALCCMT:
					if (HIWORD(wParam) == BN_CLICKED) SetComment(hDlg,tinfo);
					break;

				case IDE_ALCCMT:
					if ( HIWORD(wParam) == EN_CHANGE ){
						EnableDlgWindow(hDlg,IDB_ALCCMT,TRUE);
					}
					break;

				case IDC_ALCKEYG:
					if ( HIWORD(wParam) == CBN_SELCHANGE ){
						int index;

						index = (int)SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
						if ( index != CB_ERR ) SelectedKeyGroup(hDlg,index);
					}
					break;

				case IDC_ALCKEYS:
					if ( HIWORD(wParam) == CBN_SELCHANGE ){
						int index;

						index = (int)SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
						if ( index != CB_ERR ) SelectedKeySubID(hDlg,index);
					}
					break;
										// コマンド関連
				case IDB_ALCCMDI:
					InsertMacroString(hDlg);
					SetDlgFocus(hDlg,IDE_ALCCMD);
					break;

				case IDE_ALCCMD:
					// IDE_EXITEM へ
				case IDE_EXITEM:
					if ( HIWORD(wParam) == EN_CHANGE ){
						EnableSetButton(hDlg,TRUE);
					}
					break;

				case IDB_ALCSET:
				case IDB_ALCNEW:
					AddItem( hDlg,tinfo,LOWORD(wParam) );
					break;

				case IDB_ALCDEL:
					DeleteItem(hDlg,tinfo);
					break;

				case IDB_TEST:
					TestTable(hDlg,tinfo);
					break;

				case IDB_ALCCMDLIST:
					InitCmdList(hDlg);
					break;
// Item 位置操作関連
				case IDB_MEUP:
					ItemUpDown(hDlg,tinfo,-1);
					break;

				case IDB_MEDW:
					ItemUpDown(hDlg,tinfo,+1);
					break;
// キー割当て専用
				case IDB_ALCKEY:
					ChooseKey(hDlg,IDE_EXITEM,tinfo);
					break;
// メニュー・ツールバー専用
				case IDB_MECKEYS:
					InsertMenuSeparator(hDlg,tinfo->key == 'B');
					SetFocus(tinfo->hLVAlcWnd);
					break;
// メニュー専用
				case IDB_MEKEY:
					if ( IsTrue(KeymapMenu(hDlg)) ){
						EnumType(tinfo);
						SelectedType(hDlg,tinfo);
						Changed(hDlg);
					}
					break;
// 隠しメニュー専用
				case IDB_HMCHARC:
					HideMenuColor(hDlg,&CharColor);
					break;
				case IDB_HMBACKC:
					HideMenuColor(hDlg,&BackColor);
					break;
// マウス専用
				case IDB_ALCMOUSEL:
				case IDB_ALCMOUSEU:
				case IDB_ALCMOUSED:
				case IDB_ALCMOUSER:
					SendDlgItemMessage(hDlg,IDE_EXITEM,EM_SETSEL,
							EC_LAST,EC_LAST);
					SendDlgItemMessage(hDlg,IDE_EXITEM,WM_CHAR,
							(LPARAM)GALLOW[LOWORD(wParam) - IDB_ALCMOUSEL],0);
					break;

				case IDC_ALCMOUSEB:
					if ( HIWORD(wParam) == CBN_SELCHANGE ){
						int index;

						index = (int)SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
						if ( index != CB_ERR ) SelectedMouseButton(hDlg,tinfo,index);
					}
					break;

				case IDC_ALCMOUSET:
					if ( HIWORD(wParam) == CBN_SELCHANGE ){
						int index;

						index = (int)SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
						if ( index != CB_ERR ){
							SelectedMouseType(hDlg,index);
						}
					}
					break;
// ツールバー専用
				case IDL_BLIST:
					if ( HIWORD(wParam) == LBN_SELCHANGE ){
						EnableSetButton(hDlg,TRUE);
						SetButtonCommand(hDlg,wParam,lParam,tinfo);
					}
					break;

				case IDB_BREF:
					ToolBarMenu(hDlg);
					break;

				case IDX_ALCEXLIST:
					AddDefaultCmdList(tinfo->hLVAlcWnd);
					break;
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			if ( wParam == IDV_ALCLIST ){
				#define PNM ((NM_LISTVIEW *)lParam)
				switch ( NHPTR->code ){
					case LVN_ITEMCHANGED:
						if ( SelectItemLV == PNM->iItem ) break;
						SelectItemLV = PNM->iItem;
						lvi.mask = LVIF_TEXT | LVIF_PARAM;
						lvi.pszText = buf;
						lvi.cchTextMax = CMDLINESIZE;
						lvi.iItem = SelectItemLV;
						lvi.iSubItem = 0;
						ListView_GetItem(PNM->hdr.hwndFrom,&lvi);
						SelectedItem(hDlg,tinfo,buf);
						tinfo->index_alc = (int)lvi.lParam - 1;
						EnableSetButton(hDlg,FALSE);
						break;

					case LVN_COLUMNCLICK: {
						LISTVIEWCOMPAREINFO lvfi;
						int neworder;

						neworder = PNM->iSubItem + 1;
						if ( tinfo->ListViewLastSort >= 0 ){
							if ( tinfo->ListViewLastSort == neworder ){
								neworder = -neworder;
							}
						}
						lvfi.hListViewWnd = PNM->hdr.hwndFrom;
						lvfi.column = PNM->iSubItem;
						lvfi.order = neworder;
						ListView_SortItems(PNM->hdr.hwndFrom,(PFNLVCOMPARE)
								ListViewCompareFunc,(LPARAM)&lvfi);
						tinfo->ListViewLastSort = neworder;
						EnableDlgWindow(hDlg,IDB_MEUP,FALSE);
						EnableDlgWindow(hDlg,IDB_MEDW,FALSE);
						break;
					}
				}
				#undef PNM
				return TRUE;
			}
			if ( wParam == IDV_EXTYPE ){
				#define PNM ((NM_LISTVIEW *)lParam)
				switch ( NHPTR->code ){
					case LVN_ITEMCHANGED:
						if ( tinfo->index_type != PNM->iItem ){
							tinfo->index_type = PNM->iItem;
							SelectedType(hDlg,tinfo);
						}
						break;
				}
				#undef PNM
				return TRUE;
			}

			if ( NHPTR->code == PSN_SETACTIVE ){
				InitWndIcon(hDlg,IDB_ALCCMDI);
				if ( (hCmdListWnd != NULL) && IsWindow(hCmdListWnd) ){
					PostMessage(hCmdListWnd,WM_CLOSE,0,0);
					hCmdListWnd = NULL;
					return 0;
				}
			}
			// Ok を選んだときに、"設定"を忘れていないか確認する
			if ( NHPTR->code == PSN_APPLY ){
				if ( IsWindowEnabled(GetDlgItem(hDlg,IDB_ALCSET)) ){
					if ( PMessageBox(hDlg,GetCText(StrQueryNoAdd),StrCustTitle,
						MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES ){
						SetWindowLongPtr(hDlg,DWLP_MSGRESULT,TRUE);
					}
					return TRUE;
				}
			}

			if ( (hCmdListWnd != NULL) && (NHPTR->hwndFrom == hCmdListWnd) ){
				return CmdTreeNotify(hDlg,NHPTR/*,tinfo*/);
			}
			return StyleDlgProc(hDlg,msg,tinfo->helpID,lParam);
			#undef NHPTR
		default:
			return StyleDlgProc(hDlg,msg,wParam,lParam);
	}
	return TRUE;
}

INT_PTR CALLBACK KeyPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if ( msg == WM_INITDIALOG ){
		keyinfo.key = 'K';
		keyinfo.helpID = IDD_KEYD;
	}
	return TablePage(hDlg,msg,wParam,lParam,&keyinfo);
}

INT_PTR CALLBACK MousePage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if ( msg == WM_INITDIALOG ){
		mouseinfo.key = 'm';
		mouseinfo.helpID = IDD_MOUSED;
	}
	return TablePage(hDlg,msg,wParam,lParam,&mouseinfo);
}

INT_PTR CALLBACK ExtPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if ( msg == WM_INITDIALOG ){
		extinfo.key = 'E';
		extinfo.helpID = IDD_EXT;
	}
	return TablePage(hDlg,msg,wParam,lParam,&extinfo);
}

INT_PTR CALLBACK MenuPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if ( msg == WM_INITDIALOG ){
		menuinfo.key = 'M';
		menuinfo.helpID = IDD_MENUD;
	}
	return TablePage(hDlg,msg,wParam,lParam,&menuinfo);
}

INT_PTR CALLBACK BarPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if ( msg == WM_INITDIALOG ){
		barinfo.key = 'B';
		barinfo.helpID = IDD_BARD;
		BarBmpInfo.hBmp = NULL;
		BarBmpInfo.filename[0] = '\1';
	}
	return TablePage(hDlg,msg,wParam,lParam,&barinfo);
}
