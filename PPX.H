//-----------------------------------------------------------------------------
//	Paper Plane xUI		共通ヘッダ
//-----------------------------------------------------------------------------
#include "CHRTYPE.H"
#include "TOROWIN.H"
#include "TKEY.H"
#include "PPX.RH"
#include "PPCOMMON.H"
#include "BMP.H"

#ifdef RELEASE
#undef Message
#undef MessageA
#undef MessageW
#undef Messagef
#endif

#define PPxSettingsRegPath "Software\\TOROid\\PPx"
#define PPxSettingsAppdataPath "TOROID\\PPx"
#define PPxProgramDataPath "PPx"

// GetMessageExtraInfo で取得できるポイントメッセージの種類
#define POINTTYPE_SIG_MASK 0xFFFFFF00
#define POINTTYPE_TOUCH_MASK 0xFFFFFF80
#define POINTTYPE_TOUCH 0xFF515700
#define POINTTYPE_TOUCH_TOUCH 0xFF515780
#define POINTTYPE_TOUCH_PEN 0xFF515700
//      POINTTYPE_TOUCH_MOUSE				// 0
// 値の例(windows10 2016/08)
// FF515702(touch screen pen top/side)
// FF515704(BAMBOO pen top)
// FF515705(BAMBOO pen bottom)
// FF515780-FF51578F(touch、下位4bitは、タッチ毎に増加)

// 各種設定 ===================================================================
#define USETEMPCONFIGFILE 0 // AppData に設定を作成しないようにするには 1を指定

#define WINHELPFILE "PPX.HLP"
#define HTMLHELPFILE "ppxhelp.html"
#define HTMLHELPPAGE "/ppxhelp.html"
#define HTMLHELPINDEX "/ppxhlp.html"
#define PPxWebPage "/slppx.html"

#if !defined(USEDIRECTX) && !defined(USEDIRECTWRITE)
  #define DRAWTYPESTRING
#else
  #ifdef USEDIRECTWRITE
	#define DRAWTYPESTRING T("DirectWrite,")
  #else
	#define DRAWTYPESTRING T("Direct9,")
  #endif
#endif

#ifdef WINEGCC
	#define USEWINESTRING T("Wine,")
#else
	#define USEWINESTRING
#endif

// DirectWrite,x64,UNICODE
#define RUNENVSTRINGS USEWINESTRING DRAWTYPESTRING T(BITSTRING) T(",") T(CODETYPESTRING)

#define SKIPBUTTONMENUTIME 500 // ボタンメニューを２度押ししたときの無視時間
// メモリ使用制限関連 =========================================================
#ifndef _WIN64 // 32bit版の設定
	// ファイルイメージのデフォルト読み込み限界、X_wsiz で変更可
#define IMAGESIZELIMIT (1024*1024*20)

	// PPcのスレッド間共用のアイコンのキャッシュ数。まだバグ有りなので200程度にして直すこと
#define ICONCACHELISTINDEXMAX 0x7ffff0
	// PPcの窓内のアイコンのキャッシュ数。
	// 32dotICON 7850個分(1600*1200*3画面 or 4000*2000*1画面 を埋め尽くし可能)
#define PPCWINDOWICONCACHEMAX (7850 * 32 * 32)

	// iconを先読みする量。表示画面＋先読みページの数
#define READCACHEPAGES 2


#else // 64bit版の設定
#define IMAGESIZELIMIT (1024*1024*50)
#define ICONCACHELISTINDEXMAX 0x7fffff0
#define PPCWINDOWICONCACHEMAX (7850 * 32 * 32 * 4)
#define READCACHEPAGES 3
#endif

#define LOGLINES_MAX 3000		// 共用ログの最大行数
#define LOGLINES_DELETE 1000	// 削除するときに残す行数
#define DEF_X_svsz (10000000)	// 10M ※lowsp なら 4000000
// ============================================================================
#if 0
#define DEBUGLOGF(message, param) XMessage(NULL, NULL, XM_DbgLOG, T(message), param)
#else
#define DEBUGLOGF(message, param)
#endif

#if defined(_MSC_VER) && !defined(RELEASE)
	#define C4701CHECK = 0 // C4701(未初期化) の検討済み
#endif
#ifndef C4701CHECK
	#define C4701CHECK
#endif

#define PAIRBIT B0 // PPc等のペアID算出に使う

// Key Alias/メニュー登録用
#define K_M  K_internal			// メニューのみ
#define K_Mv (K_internal | K_v)	// VK
#define K_Me (K_internal | K_e)	// 拡張SHIFT
#define K_Ms (K_internal | K_s)	// SHIFT
#define K_Mc (K_internal | K_c)	// CTRL
#define K_Ma (K_internal | K_a)	// ALT/GRPH

// X_dlf 用定数
#define XDLF_ROOTJUMP	B0
#define XDLF_DISPALIAS	B1
#define XDLF_DISPDRIVETITLE	B2
#define XDLF_NODRIVES	B3
#define XDLF_NODISPSHARE	B4
#define XDLF_NOM_PJUMP	B5
#define XDLF_UPDIRJUMP	B6
#define XDLF_USEMDRIVES	B7 // XDLF_NOOVERLAP と重複
#define XDLF_NOOVERLAP	B7
#define XDLF_DISPFREE	B8
#define XDLF_ADMINDRIVE	B9

typedef struct {
	RECT pos;		//MoveWindow
	BYTE show;		//ShowWindow
	BYTE reserved;
} WINPOS;

#define HiddenMenu_fg(base) *(COLORREF *)(base + TSTROFF(5))
#define HiddenMenu_bg(base) *(COLORREF *)(base + TSTROFF(5) + sizeof(COLORREF))
#define HiddenMenu_cmd(base) (TCHAR *)(base + TSTROFF(5) + sizeof(COLORREF) * 2)

#pragma pack(push, 1)
typedef struct {
	WORD offset;
	TCHAR name[5];
	COLORREF fg;
	COLORREF bg;
} HIDDENMENUSTRUCT;
#pragma pack(pop)

// タイトルバー無しにするときに制御する必要があるスタイル
#define WS_NOTITLEOVERLAPPED (WS_POPUP | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

// %F / %*name のオプション
//(出力形式)
#define FMOPT_FILENAME	B0	// C ファイル名+拡張子
#define FMOPT_DIR		B1	// D ディレクトリ
#define FMOPT_DRIVE		B2	// H ドライブ名		(%*nameのみ)
#define FMOPT_FILENOEXT	B3	// X ファイル名のみ
#define FMOPT_FILEEXT	B4	// T 拡張子			(%*nameのみ)
//(「"」の処理)
#define FMOPT_BLANKET	B5	// B ""で必ず括る
#define FMOPT_NOBLANKET	B6	// N ""は必ず無い
//(加工種類)
#define FMOPT_ENABLEVFS	B7	// V PPx独自指定を有効にする
#define FMOPT_REALPATH	B8	// R パスを実体にする(%*nameのみ)
#define FMOPT_USESFN	B9	// S SFNを使用する	(%*nameのみ)
#define FMOPT_UNIQUE	B10	// U 既存と重複しないように加工する (%*nameのみ)
#define FMOPT_MARK		B11	// M マークファイルのみ (%F, PPcのみ)
#define FMOPT_LASTSEPARATOR	B12	// P 末尾に \ を付加する (%*nameのみ)
//#define FMOPT_CURSOR	B12	// O カーソルファイルのみ まだ用意していない
//#define FMOPT_IDL		B14	// I IDL表記( ::{...}::{...} ) まだ用意していない

// X_pmc の第２項目以降の設定、X_pmcは負論理、touchmodeは正論理
#define TOUCH_LARGEWIDTH		B0	// T   タッチ領域の幅を拡大する
#define TOUCH_LARGEHEIGHT		B1	// T   タッチ領域の高を拡大する
#define TOUCH_DISABLEDRAG		B2	// T   ドラッグ開始を禁止する
#define TOUCH_DISABLEHIDDENMENU	B3	// T   隠しメニューを無効に
#define TOUCH_DBLCLICKTOSINGLE	B4	// T   ダブルクリックをシングルタッチで行う
#define TOUCH_AUTOPAIRDIST		B5	// T   最大化２窓の並びを縦／横自動変更
#define TOUCH_ACTIONWITHACTIVE	B6	// T   アクティブ化時は、タッチ操作を無効に
#define TOUCH_EDIT_COMMAND_BAR	B7	// TMP 一行編集にコマンドバーを表示

#define X_pmc_defvalue -8, 0, (B0|B1|B2|B3|B4|B5|B6|B10|B11), 0 // 負論理注意
// ※ mouse の B10, B11 は将来マウスでは使わない機能向けに確保
#define X_pmc_touch 1
#define X_pmc_mouse 2
#define X_pmc_pen 3

// SetPopMsg/StopPopMsg
#define PMF_WAITTIMER	B0	// 時間待ち中(PPc)
#define PMF_WAITKEY		B1	// キー入力中(PPc)
#define PMF_PROGRESS	B2	// 進捗状況表示中(PPc)
#define PMF_FLASH		B3	// フラッシュ表示有り(PPc)
#define PMF_BUSY		B4	// busy(操作ができない状態, PPc)
#define PMF_ALL -1
#define PMF_DISPLAYMASK (PMF_WAITTIMER | PMF_WAITKEY | PMF_PROGRESS | PMF_BUSY)

#pragma pack(push, 4)
typedef struct {
	int x, y, width, height;
} AREA;
#pragma pack(pop)

#define BADHWND ((HWND)(DWORD_PTR)0xfefffffe)
#define FILEATTRMASK_FILES 0xf
#define FILEATTRMASK_DIR_FILES 0x1f

#define EXCEPTION_PPX_DUMPSTACK 0x6106106f // デバッグ用にスタックダンプをする
#define PpxDumpstack() RaiseException(EXCEPTION_PPX_DUMPSTACK, 0, 0, NULL)

#define X_tray_Combo (B0 | B1 | B4)
#define X_tray_SinglePPc (B0 | B1 | B5)
#define X_tray_PPv (B0 | B1 | B6)

// F_mes, F_dlg, F_fix に使用するフォント
#define Default_F_mes { \
/*Width, Height*/					0, 0, \
/*Escapement, Orientation, Weight*/	0, 0, FW_NORMAL, \
/*Italic, Underline, StrikeOut*/	FALSE, FALSE, FALSE, \
/*CharSet*/							SHIFTJIS_CHARSET, \
/*OutPrecision*/					OUT_DEFAULT_PRECIS, \
/*ClipPrecision*/					CLIP_DEFAULT_PRECIS, \
/*Quality*/							DEFAULT_QUALITY, \
/*PitchAndFamily*/					FIXED_PITCH | FF_DONTCARE, \
/*FaceName*/						T("Courier New")}

#define Default_F_mes_JA T("ＭＳ ゴシック")

// F_unfix に使用するフォント
#define Default_F_unfix { \
/*Width, Height*/					0, 0, \
/*Escapement, Orientation, Weight*/	0, 0, FW_NORMAL, \
/*Italic, Underline, StrikeOut*/	FALSE, FALSE, FALSE, \
/*CharSet*/							SHIFTJIS_CHARSET, \
/*OutPrecision*/					OUT_DEFAULT_PRECIS, \
/*ClipPrecision*/					CLIP_DEFAULT_PRECIS, \
/*Quality*/							DEFAULT_QUALITY, \
/*PitchAndFamily*/					FF_DONTCARE, \
/*FaceName*/						T("Arial")}

#define Default_F_unfix_JA T("ＭＳ Ｐゴシック")

#define PPX_FONT_MIN_PIXEL 10 // 96dpi時の最小フォントサイズ
#define PPX_FONT_MIN_PT 8 // 96dpi時の最小フォントポイント(ダイアログ用)
#define DEFAULT_WIN_DPI 96
#define DEFAULT_DTP_DPI 72

#define ENUMOBJECTSFORFOLDERFLAG_VISTA (SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN | SHCONTF_FASTITEMS)
#define ENUMOBJECTSFORFOLDERFLAG_XP    (SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN)

#define ShortcutExt ".lnk" // Win2000/98まで:.LNK WinXP以降:.lnk

#define PPC_HIGHLIGHT_COLORS 7
#define PPV_HIGHLIGHT_COLORS 8

//------------------------------------- *file 関連
#pragma pack(push, 1)
typedef struct {			// カスタマイズ内容(X_fopt)
							// Same Name Action -------------------------------
	int		sameSW;			// (ALL&W)同名ファイルの対処方法が設定済みなら !0
	int		same;			// 同名ファイルの対処方法 FOPSAME_ 関連
							// Name Filter ------------------------------------
	int		chrcase;		// 1:大文字化 2:小文字化
	int		sfn;			// 1:8.3
	int		delspc;			// !0:空白削除
	DWORD	filter;			// VFSFOP_FILTER_
							// Attributes -------------------------------------
	DWORD	AtrMask;		// And 指定用
	DWORD	AtrFlag;		// Or 指定用
							// Etc --------------------------------------------
	int		mode;			// FOPMODE_ 関連
	DWORD	flags;			// VFSFOP_OPTFLAG_ 関連
	char	aall;			// !0:auto all check
	BYTE	firstsheet;		// 始めに開くシート FOPTAB_
	char	useburst;		// バーストモード
	BYTE	divide_unit;	// 分割大きさの単位
	DWORD	divide_num;		// 分割大きさ
	TCHAR	str[VFPS];		// ASCIIZs  1st:Dest 2nd:Rename 3rt:S.num
} VFSFOP_OPTIONS;
#pragma pack(pop)

enum { // same の内容
	FOPSAME_NEWDATE = 0,
	FOPSAME_RENAME,
	FOPSAME_OVERWRITE,
	FOPSAME_SKIP,
	FOPSAME_ARCHIVE,
	FOPSAME_ADDNUMBER,
	FOPSAME_APPEND,
	FOPSAME_SIZE,
	FOPSAME_MAX
};
enum { // mode の内容
	FOPMODE_MOVE = 0,
	FOPMODE_COPY,
	FOPMODE_MIRROR,
	FOPMODE_SHORTCUT,
	FOPMODE_LINK,
	FOPMODE_DELETE,
	FOPMODE_UNDO,
	FOPMODE_SYMLINK,
	FOPMODE_MAX
};
// flags の内容
#define VFSFOP_OPTFLAG_SKIPERROR	B0	// 軽微エラーの無視
#define VFSFOP_OPTFLAG_FLATMODE		B1	// 平坦化
#define VFSFOP_OPTFLAG_AUTOROFF		B2	// ReadOnlyの自動解除
#define VFSFOP_OPTFLAG_BACKUPOLD	B3	// 削除・上書きファイルの保存

#define VFSFOP_OPTFLAG_UNDOLOG		B4	// Undo用ログを出力
#define VFSFOP_OPTFLAG_LOGWINDOW	B5	// ログウィンドウに経過表示
#define VFSFOP_OPTFLAG_LOGRWAIT		B6	// ログ表示したときに結果表示待機しない
#define VFSFOP_OPTFLAG_QSTART		B7	// 順序待ちしない

#define VFSFOP_OPTFLAG_NOCOUNT		B8	// カウントしない
#define VFSFOP_OPTFLAG_SECURITY		B9	// セキュリティコピー
#define VFSFOP_OPTFLAG_SACL			B10	// 監査コピー
#define VFSFOP_OPTFLAG_LOWPRI		B11	// 優先順位を低くする

#define VFSFOP_OPTFLAG_NOWCREATEDIR	B12	// ディレクトリ作成警告無し
#define VFSFOP_OPTFLAG_PREVENTSLEEP	B13	// スリープ抑制
#define VFSFOP_OPTFLAG_ALLOWDECRYPT	B14	// 暗号化非対応ドライブへの暗号化ファイルのコピーを許可
#define VFSFOP_OPTFLAG_NOWDELEXIST	B15	// サブディレクトリ内の存在警告なし

#define VFSFOP_OPTFLAG_KEEPDIR		B16	// 作成元の階層を再現(srcがListFileの時)
#define VFSFOP_OPTFLAG_JOINBATCH	B17	// 分割時に元に戻す為のバッチを生成
#define VFSFOP_OPTFLAG_ADDNUMDEST	B18	// 上書きファイル元の名前変更
#define VFSFOP_OPTFLAG_NOFIRSTEXIST	B19	// 開始時に処理先存在チェックをしない

#define VFSFOP_OPTFLAG_SYMCOPY_SHIFT	20
#define VFSFOP_OPTFLAG_SYMCOPY_SYM	B20	// シンボリックリンクのリンク コピー
#define VFSFOP_OPTFLAG_SYMCOPY_FILE	B21	// シンボリックリンクのファイル コピー
#define VFSFOP_OPTFLAG_SYMDEL_SHIFT	22
#define VFSFOP_OPTFLAG_SYMDEL_SYM	B22	// シンボリックリンクを解除
#define VFSFOP_OPTFLAG_SYMDEL_FILE	B23	// シンボリックリンク内ファイルを削除

#define VFSFOP_OPTFLAG_WAIT_CLOSE	B24	// 移動時使用終了を待つ
#define VFSFOP_OPTFLAG_IFILEOP		B25	// IFileOperation を使用する

//#define VFSFOP_OPTFLAG_DELDELAY		// 未 Delaymode
//#define VFSFOP_OPTFLAG_SUBSTREAM		// 未 サブストリームも必ず処理
//#define VFSFOP_OPTFLAG_NOSUBSTREAM	// 未 サブストリームも必ず処理
// 未 FLATMODE/BACKUPOLD dir名付加

// filter の内容
#define VFSFOP_FILTER_DELNUM		B0
#define VFSFOP_FILTER_NOFILEFILTER	B1
#define VFSFOP_FILTER_NODIRFILTER	B2
#define VFSFOP_FILTER_EXTRACTNAME	B3
#define VFSFOP_FILTER_NOEXTFILTER	B4

//------------------------------------- PPb 関連
#define PPBBOOTSYNC "PPbS" // PPbの起動時同期に使用するイベント名

//	PPx 本体, PPCOMMON, PPx Module 間の情報交換用構造体
//## 0xff000～0xfffff:	内部用
#define PPXCMDID_PPBEXEC	0xff000 // *execute
#define PPXCMDID_PPBINSERT	0xff001 // *insert
#define PPXCMDID_PPBREPLACE	0xff002 // *replace
#define PPXCMDID_PPBSELECTTEXT	0xff003 // %*select
#define PPXCMDID_PPBEDITTEXT	0xff004 // %*edittext
#define PPXCMDID_PPBINSERTSEL	0xff005 // *insertsel
typedef struct {
	const TCHAR *name;
	int flag;
	const TCHAR *path;
} PPXCMD_PPBEXEC;

//------------------------------------- PPc 関連
#define CLSID_IDATAOBJECT_AUTODD {0x95D5EDD3, 0x8F9B, 0x4825, {0xA5, 0x70, 0x81, 0x5D, 0x74, 0xA1, 0xEA, 0xA0}} // AutoD&Dに使う相手のDropTarget取得用GUID

// カラム拡張、ユーザカラムの内容を記憶する領域で使用する識別ID
// カラム拡張用 : DFC_COLUMNEX ～DFC_COLUMNEX_MAX
// ユーザカラム用 : DFC_USERMIN～DFC_USERMAX ※DFC_USERMAXから順に使う
// 拡張コメント用 : DFC_COMMENTEX～DFC_COMMENTEX_MAX
#define DFC_UNDEF 0xffff // ID未割当て
#define DFC_FAULT 0xfffe // カラム拡張読み込み失敗
#define DFC_USERMAX (DFC_FAULT - 1)
#define DFC_USERMIN 0x8000
#define DFC_SHELLIDLIST 0x7f01
// #define DFC_COMMENTEX_MAX 0x7f00 // PPCOMMON.H で定義
// #define DFC_COMMENTEX (DFC_COMMENTEX_MAX - 10 + 1)
#define DFC_LONGNAME 0x7800
#define DFC_COLUMNEX_MAX (DFC_LONGNAME - 1)
#define DFC_COLUMNEX 0

#pragma pack(push, 1)
typedef struct { // DE_COLUMN
//	BYTE fmtID = DE_COLUMN
	BYTE width;
	WORD itemindex; // DFC_xxx
	BYTE fmtsize;
	TCHAR name[1];
} DISPFMT_COLUMN;
#pragma pack(pop)

typedef union {
	char dat[4];
	int block;
} XC_SORT_MODE;

typedef struct {
	XC_SORT_MODE mode;
	DWORD atr;
	DWORD option;
} XC_SORT;

#define DSET_CACHEONLY	B0	// キャッシュを使ってディレクトリ読み込みを抑制
#define DSET_NODIRCHECK	B1	// ディレクトリの監視を無効にする
#define DSET_ASYNCREAD	B2	// 非同期/遅延読み込みを有効にする
#define DSET_REFRESH_ACACHE	B3	// 非同期読み込み完了後、常にキャッシュを更新
#define DSET_NOSAVE_ACACHE	B4	// 非同期読み込み完了後のキャッシュ保存をしない
#define DSET_DEFAULT	MAX32

#define DSETI_NOSPACE	0	// (INFO)アイコンの場所を用意しない (CELL)info利用
#define DSETI_BLANK		1	// 空欄
#define DSETI_BOX		2	// (INFO)枠のみ (CELL)空欄
#define DSETI_EXTONLY	3	// 拡張子による判断のみ
#define DSETI_NORMAL	4	// 拡張子+α
#define DSETI_OVL		5	// 拡張子+α+オーバレイ
#define DSETI_OVLNOC	6	// 拡張子+α+オーバレイ+キャッシュ無し
#define DSETI_OVLSINGLE	7	// 拡張子+α+オーバレイ+キャッシュ無し+シングルThr
#define DSETI_DEFAULT	MAX16	// 初期値を使用

typedef struct {
	WORD flags, deflags; // DSET_
	WORD infoicon, cellicon; // DSETI_	情報行/セルのアイコン
	XC_SORT sort; // ソートの設定
} XC_DSET;

typedef struct {
	DWORD index;
	TCHAR text[CMDLINESIZE];
} TOOLBARCUSTTABLESTRUCT;

#define LOADSETTINGS_DISPLEN 0x80
#define LOADSETTINGS_MASKLEN 0x180
#define LOADSETTINGS_CMDLEN  0x200
typedef struct {
	XC_DSET dset;
	TCHAR disp[LOADSETTINGS_DISPLEN];	// [;]
	TCHAR mask[LOADSETTINGS_MASKLEN];
	TCHAR cmd[LOADSETTINGS_CMDLEN];
} LOADSETTINGS;

#define MC_CELS_TEMPNAME T("?Edit?") // 表示書式編集用に使う一時設定名

#define DetectWildcardLetter T("!*,/:;?")
#define DetectPathWildcardLetter T("!*,/;?")

// X_dss の指定
#define DSS_FONT	B0 // フォントサイズの変更を有効にする
#define DSS_MINFONT	B1 // フォントサイズの最小値調整を有効にする
#define DSS_ICON	B2 // アイコンサイズの変更を有効にする
#define DSS_COMCTRL	B3 // その他の部品(Tree, ListView, Header, Button, ToolBar, ImageList)のDSを有効にする
#define DSS_ACTIVESCALE	B4 // 動的変更を有効にする
#define DSS_DIALOGREDUCE	B5 // ダイアログの画面はみ出し抑制

#define DSS_DEFFLAGS	0xff
#define DSS_NOLOAD	(DSS_DEFFLAGS | 0xa00) // 設定未読み込み

typedef struct {
	int enter;
	int exectype;
} CRMENUSTACKCHECK;

//------------------------------------- PPv 関連
typedef enum {
	DISPT_NONE = 0, DISPT_HEX, DISPT_TEXT, DISPT_DOCUMENT, DISPT_IMAGE, DISPT_RAWIMAGE, DISPT_MAX, DISPT_UNDEFINED = -1
} DISPT;

//------------------------------------- 言語切り替え可能なメッセージ
#define MESTEXTIDLEN 4
#define MES_IDD_FOP	T("7020|")
#define MESN_UARE	T("7062|&Use all reaction")
#define MESN_REND	T("7068|Rename file on dest.(&V)")
#define MESN_FRONT	T("78C5|&Front")
#define MESN_ROMA	T("78C6|R&oma")

#define MES_ACET	T("ACET|\n, and %d application")
#define MES_ADHD	T("ADHD|This file is using by applications;\n")
#define MES_ADTE	T("ADTE|Enter a title")
#define MES_AICP	T("AICP|Add check pattern")
#define MES_ASTL	T("ASTL|Add start list")
#define MES_BRAK	T("BRAK|Job breaked")
#define MES_BREX	T("BREX|Need regexp library")
#define MES_BUSY	T("BUSY|Busy...")
#define MES_CAOF	T("CAOF|Cache : off")
#define MES_CAON	T("CAON|Cache : on")
#define MES_CCAN	T("CCAN|Cancel")
#define MES_CCMP	T("CCMP|Compare completed.")
#define MES_CMP1	T("CMP1|Same S&HA-1")
#define MES_CMPA	T("CMPA|S&ame size && time")
#define MES_CMPB	T("CMPB|Same &Binary")
#define MES_CMPC	T("CMPC|&Comment")	// 現在未使用
#define MES_CMPD	T("CMPD|&Detail")
#define MES_CMPE	T("CMPE|&Exist file")
#define MES_CMPF	T("CMPF|&Filename without ext.")
#define MES_CMPH	T("CMPH|Compare Hash to &Clipboard")
#define MES_CMPI	T("CMPI|file s&Ize(no name check)")
#define MES_CMPL	T("CMPL|&Large file")
#define MES_CMPN	T("CMPN|new &File")
#define MES_CMPQ	T("CMPQ|same size, any name(&Q)")
#define MES_CMPS	T("CMPS|file &Size")
#define MES_CMPT	T("CMPT|&Time stamp")
#define MES_CMPM	T("CMPM|co&mment(1window)")
#define MES_CMPW	T("CMPW|al&One file")
#define MES_CMPX	T("CMPX|E&xist(with dir)")
#define MES_CMPZ	T("CMPZ|same file si&Ze(1window)")
#define MES_COMP	T("COMP|completed.")

#define MES_CPDN	T("CPDN|Clip done")
#define MES_CPML	T("CPML|Leave Select mode")
#define MES_CPMS	T("CPMS|Select mode")
#define MES_CPPN	T("CPPN|Clip dir done")
#define MES_CPTX	T("CPTX|Clip")
#define MES_CRSS	T("CRSS|Save setting")
#define MES_CTHE	T("CTHE|Hook&Edit")
#define MES_CUTR	T("CUTR|Cut ready")
#define MES_DDAB	T("DDAB|Add button")
#define MES_DDAI	T("DDAI|Add item to tree")
#define MES_DDAL	T("DDAL|Add item to listfile")
#define MES_DDCM	T("DDCM|Can\'t move on")
#define MES_DDCP	T("DDCP|&Copy to")
#define MES_DDDP	T("DDDP|&Dup on")
#define MES_DDDT	T("DDDT|Det&ail")
#define MES_DDET	T("DDET|&etc...")
#define MES_DDJP	T("DDJP|&Jump from")
#define MES_DDLI	T("DDLI|&Shortcut to")
#define MES_DDML	T("DDML|Make &List File")
#define MES_DDMV	T("DDMV|&Move to")
#define MES_DDOP	T("DDOP|&Open to")
#define MES_DDSC	T("DDSC|&Shortcut on")
#define MES_DDSW	T("DDSW|S&wap to")
#define MES_DDTX	T("DDTX|&text")
#define MES_DEXT	T("DEXT|Delete %s")
#define MES_DEFA	T("DEFA|default")
#define MES_DICD	T("DICD|CD/DVD/BD...")
#define MES_DIFI	T("DIFI|Fixed")
#define MES_DIRA	T("DIRA|Ramdisk")
#define MES_DIRE	T("DIRE|Remote")
#define MES_DIRM	T("DIRM|Removable")
#define MES_DIRS	T("DIRS|<dir>")
#define MES_DREN	T("DREN|d&irectory")
#define MES_DSAR	T("DSAR|&Asynchronous read")
#define MES_DSCA	T("DSCA|&Cancel")
#define MES_DSCV	T("DSCV|&Cache view")
#define MES_DSFO	T("DSFO|&0:Force")
#define MES_DSHO	T("DSHO|&0:Default")
#define MES_DSEI	T("DSEI|Entry icon(&Y)")

#define MES_DSIA	T("DSIA|overlay(safety)")
#define MES_DSIB	T("DSIB|blank")
#define MES_DSIF	T("DSIF|frame")
#define MES_DSIH	T("DSIH|overlay(no cache)")
#define MES_DSII	T("DSII|info. setting")
#define MES_DSIL	T("DSIL|&Info. line icon")
#define MES_DSIN	T("DSIN|normal")
#define MES_DSIO	T("DSIO|with overlay")
#define MES_DSIS	T("DSIS|simple")
#define MES_DSIT	T("DSIT|through")
#define MES_DSIW	T("DSIW|no show")

#define MES_DSMA	T("DSMA|Magnify thumbnail")
#define MES_DSMI	T("DSMI|Minify thumbnail")
#define MES_DSSD	T("DSSD|-save cache to &Disk")
#define MES_DSSE	T("DSSE|-save cache &Every time")
#define MES_DSSM	T("DSSM|&Slow mode")
#define MES_DSTA	T("DSTA|&4:Archive")
#define MES_DSTD	T("DSTD|&5:Directory mode")
#define MES_DSTE	T("DSTE|&3:Temporality")
#define MES_DSTF	T("DSTF|&4:Listfile")
#define MES_DSTL	T("DSTL|&2:This path and subdir")
#define MES_DSTP	T("DSTP|&1:This path")
#define MES_DSTS	T("DSTS|&6:Path separate")
#define MES_DSWM	T("DSWM|&Watch modified")
// エラー関係
#define MES_EACO	T("EACO|Already counting")
#define MES_EBDK	T("EBDK|%s is bad key name")
#define MES_EBPC	T("EBPC|bad *pane command : %s")
#define MES_EBRV	T("EBRV|Require Windows Vista or later.")
#define MES_EBSG	T("EBSG|%s is bad signature")
#define MES_EBWS	T("EBWS|Bad filesystem")
#define MES_EBRK	T("EBRK|Braket error")
#define MES_EC32	T("EC32|comdlg32.dll load error")
#define MES_ECDR	T("ECDR|Can\'t unpack")
#define MES_ECFL	T("ECFL|Customize area flow")
#define MES_ECUD	T("ECUD|Can\'t execute undo.")

#define MES_EDAD	T("EDAD|D&D faulture")
#define MES_EDAL	T("EDAL|Select &All")
#define MES_EDBL	T("EDBL|&Block mode")
#define MES_EDCA	T("EDCA|Ca&ncel")
#define MES_EDCP	T("EDCP|&Copy")
#define MES_EDCU	T("EDCU|Cu&t")
#define MES_EDDL	T("EDDL|Delete F.O. error :%s")
#define MES_EDEC	T("EDEC|&encode")
#define MES_EDEL	T("EDEL|Can not delete")
#define MES_EDET	T("EDET|&types")
#define MES_EDIT	T("EDIT|Edit")
#define MES_EDTD	T("EDTD|Temporary directory delete faulture")
#define MES_EDPA	T("EDPA|&Paste")
#define MES_EECR	T("EECR|Item '%s' not find in %s")
#define MES_EEFT	T("EEFT|Can't execute %s")
#define MES_EENF	T("EENF|Entry not found")
#define MES_EENM	T("EENM|e number error")
#define MES_EFLW	T("EFLW|Buffer flow")
#define MES_EHKY	T("EHKY|Hot key(%s) register error")
#define MES_EKCD	T("EKCD|Key error")
#define MES_EKFT	T("EKFT|Key code error")
#define MES_EKNF	T("EKNF|Keyword not found")
#define MES_ELST	T("ELST|Erase from &list")
#define MES_ENFF	T("ENFF|Not found %s")
#define MES_ENOM	T("ENOM|Not enough memories")
#define MES_ENSO	T("ENSO|Not support")
#define MES_ENSP	T("ENSP|Separetor \'%c\' not found")
#define MES_ENUD	T("ENUD|%s is not UnXXX")
#define MES_EOEC	T("EOEC|Can't register more PPc.")
#define MES_EOLT	T("EOLT|Size too long.")
#define MES_EPRM	T("EPRM|Parameter error")
#define MES_EPTH	T("EPTH|Path Error")
#define MES_EREG	T("EREG|Regular expression error : %s")
#define MES_ESCT	T("ESCT|Short cut error")
#define MES_ESEP	T("ESEP|Separator not found")
#define MES_ESHO	T("ESHO|Can't access to now open archive.")
#define MES_ETMW	T("ETMW|Too many format character 'W'/'w'")
#define MES_EUFM	T("EUFM|Unknown format")
#define MES_EUKW	T("EUKW|Unknown keyword")
#define MES_EUOP	T("EUOP|Unknown option:%s")
#define MES_EUXC	T("EUXC|Unknown command:*%s")
#define MES_EWFT	T("EWFT|W format error")
#define MES_EWNF	T("EWNF|Can\'t support on NTFS")
#define MES_EXIT	T("EXIT|E&xit")
#define MES_EXEX	T("EXEX|file exists")
#define MES_EXNF	T("EXNF|New creates")
#define MES_EXNW	T("EXNW|New file exists")
#define MES_EXOL	T("EXOL|Old file exists")

#define MES_FBAC	T("FBAC|Act&ion:")
#define MES_FBAK	T("FBAK|Backup fault.")
#define MES_FBDW	T("FBDW|PPb critical error")
#define MES_FDLW	T("FDLW|") T(PPCOMMONDLL) T(" area broken %04X")
#define MES_FEWE	T("FEWE|Found file seems extracting in the wrong place")
#define MES_FEXP	T("FEXP|Explorer Folder")
#define MES_FIXE	T("FIXE|This file seems type is %s and %s.\nThis file extension is not necessary to modify.")
#define MES_FIXH	T("FIXH|PPffix check extension type, when dropped on this icon.")
#define MES_FIXM	T("FIXM|This file seems type is %s and %s.\nModify file extension ?")
#define MES_FIXU	T("FIXU|File type is unknown.")
#define MES_FLAC	T("FLAC|all completed")
#define MES_FLCA	T("FLCA|Cancelled")
#define MES_FLER	T("FLER|errors")
#define MES_FLEX	T("FLEX|existed")
#define MES_FLLS	T("FLLS|skipped")
#define MES_FLNO	T("FLNO|No operate")
#define MES_FLOD	T("FLOD|<loading>")
#define MES_FLPC	T("FLPC|partially completed")
#define MES_FLTC	T("FLTC|No exist")
#define MES_FLTE	T("FLTE|Exist")
#define MES_FMYF	T("FMYF|Drive list")
#define MES_FNET	T("FNET|Network PC")
#define MES_FOCA	T("FOCA|Cancel")
#define MES_FOCL	T("FOCL|Close")
#define MES_FOCO	T("FOCO|Continue")
#define MES_FOCP	T("FOCP|Pause")
#define MES_FOCS	T("FOCS|Skip")
#define MES_FOFC	T("FOFC|F&C")
#define MES_FOOK	T("FOOK|Ok")
#define MES_FOST	T("FOST|Start")
#define MES_FUPD	T("FUPD|Directory not found, and move parent directory")
#define MES_FUPR	T("FUPR|Directory not found, and move root directory")
#define MES_GDAI	T("GDAI|Auto Fill")
#define MES_GDAR	T("GDAR|Auto Frame")
#define MES_GDFI	T("GDFI|Fill")
#define MES_GDFR	T("GDFR|Frame")
#define MES_GSIG	T("GSIG|Signature is good.")
#define MES_HDRA	T("HDRA|Attributes")
#define MES_HDRC	T("HDRC|Date Created")
#define MES_HDRD	T("HDRD|Date")
#define MES_HDRE	T("HDRE|Date Accessed")
#define MES_HDRN	T("HDRN|Name")
#define MES_HDRS	T("HDRS|Size")

#define MES_HPDN	T("HPDN|Http Done.")
#define MES_IANI	T("IANI|Animate : [P]")
#define MES_IDIR	T("IDIR|&Directory")
#define MES_IDIV	T("IDIV|Divide page mode([<][>], All read:[/])")

#define MES_IFCN	T("IFCN|File counting...")
#define MES_IGAN	T("IGAN|Ani&mate")
#define MES_IGNP	T("IGNP|&Next page")
#define MES_IGPP	T("IGPP|&Prev page")

#define MES_ILFI	T("ILFI|&List File")

#define MES_IMAB	T("IMAB|C&ancel")
#define MES_IMCA	T("IMCA|Ca&ncel")
#define MES_IMDLx	T("7063|add &Delay list")
#define MES_IMIG	T("IMIG|&Igonre")
#define MES_IMNO	T("IMNO|&No")
#define MES_IMOK	T("IMOK|OK")
#define MES_IMRE	T("IMRE|&Retry")
#define MES_IMSK	T("IMSK|Sk&Ip")
#define MES_IMST	T("IMST|Start(&Y)")
#define MES_IMTC	T("IMTC|&Cancel")
#define MES_IMYE	T("IMYE|&Yes")
#define MES_IMYK	T("IMYK|OK(&Y)")

#define MES_IPAG	T("IPAG|multi page")
#define MES_ISEA	T("ISEA|Incremental search mode")
#define MES_ITCM	T("ITCM|Comment(&C)")
#define MES_ITFE	T("ITFE|name ext.(&E)")
#define MES_ITFN	T("ITFN|Filename(&N)")
#define MES_ITMA	T("ITMA|All Mark(&A)")
#define MES_ITMO	T("ITMO|Check Mark(&M)")

#define MES_JLER	T("JLER|ERROR>")
#define MES_JLEX	T("JLEX|exec>")
#define MES_JLPA	T("JLPA|pause>")
#define MES_JLWA	T("JLWA|wait>")

#define MES_JMAE	T("JMAE|Extract")
#define MES_JMAP	T("JMAP|Archive")
#define MES_JMAT	T("JMAT|Attributes")
#define MES_JMCO	T("JMCO|Compare")
#define MES_JMCP	T("JMCP|Copy")
#define MES_JMCS	T("JMCO|Size Count")
#define MES_JMDE	T("JMDE|Delete")
#define MES_JMJO	T("JMJO|Job")
#define MES_JMLI	T("JMLI|Link/Junction")
#define MES_JMMO	T("JMMO|Move")
#define MES_JMMR	T("JMMR|Mirror")
#define MES_JMRE	T("JMRE|Rename")
#define MES_JMRU	T("JMRU|Execute")
#define MES_JMSH	T("JMSH|Shortcut")
#define MES_JMSL	T("JMSL|Symlink")
#define MES_JMUN	T("JMUN|Undo")

#define MES_JNCL	T("JNCL|&CLSID")
#define MES_JNJL	T("JNJL|&link")
#define MES_JNSH	T("JNSH|&shortcut")
#define MES_JNST	T("JNST|s&tream")
#define MES_JPCL	T("JPCL|CLSID(pa&ir window)")
#define MES_JPDI	T("JPDI|show &pair window")
#define MES_JPJL	T("JPJL|link(pai&r window)")
#define MES_JPSH	T("JPSH|shortcut(&pair window)")
#define MES_JPST	T("JPST|stream(pair &window)")

#define MES_JPLN	T("JPLN|Jump link")
#define MES_JWOF	T("JWOF|Join window: off")
#define MES_JWON	T("JWON|Join window: on")

#define MES_KWAT	T("KWAT|\nPress any key to continue.")

#define MES_LDAD	T("LDAD|address")
#define MES_LDDR	T("LDDR|drives")
#define MES_LDIN	T("LDIN|input")
#define MES_LDOL	T("LDOL|Lock")
#define MES_LDOS	T("LDOS|Separator")
#define MES_LDSI	T("LDSI|XC_inf")
#define MES_LDST	T("LDST|XC_stat")

#define MES_LERE	T("LERE|Erased Entry from list.")
#define MES_LGAP	T("LGAP|Append")
#define MES_LGCR	T("LGCR|Create")
#define MES_LGDE	T("LGDE|Delete")
#define MES_LGER	T("LGER|Error")
#define MES_LGOW	T("LGOW|Overwrite")
#define MES_LGWA	T("LGWA|Warning")

#define MES_LHIL	T("LHIL|Highlight")
#define MES_LHIO	T("LHIO|Highlight off")
#define MES_LMAK	T("LMAK|Mark")
#define MES_LOAD	T("LOAD|loading")
#define MES_LRMA	T("LRMA|Reverse mark")
#define MES_LUMA	T("LUMA|Unmark")

#define MES_LYCA	T("LYCA|&Caption")
#define MES_LYDB	T("LYDB|Dock &Bottom")
#define MES_LYDT	T("LYDT|&Dock Top")
#define MES_LYHA	T("LYHA|&Header")
#define MES_LYIL	T("LYIL|&Information line")
#define MES_LYJL	T("LYJL|&Job list")
#define MES_LYLG	T("LYLG|Lo&g")
#define MES_LYME	T("LYME|&Menu Bar")
#define MES_LYOT	T("LYOT|&Others")
#define MES_LYSA	T("LYSA|Standerd &address bar")
#define MES_LYSC	T("LYSC|scr&oll bar")
#define MES_LYSL	T("LYSL|&Status line")
#define MES_LYSS	T("LYSS|Swa&p scroll bar")
#define MES_LYST	T("LYST|Standerd &toolbar")
#define MES_LYTI	T("LYTI|Tit&le Bar")
#define MES_LYTR	T("LYTR|T&ree")
#define MES_LYWM	T("LYWM|&Window...")

#define MES_MBCP	T("MBCP|Clip message text")
#define MES_MCOP	T("MCOP|&open")
#define MES_MCPL	T("MCPL|&play")
#define MES_MCPR	T("MCPR|&print")
#define MES_MCRK	T("MCRK|Run &as")
#define MES_MCRV	T("MCRV|Run as &administrator")
#define MES_MCRC	T("MCRC|Edit &common menu")
#define MES_MCRD	T("MCRD|Set &default select")
#define MES_MCRE	T("MCRE|Set d&efault command")
#define MES_MCRX	T("MCRX|Edit filename-e&xtention menu")
#define MES_MDLC	T("MDLC|With shift : Delete")
#define MES_MEAD	T("MEAD|&Add Item")
#define MES_MEDE	T("MEDE|&Delete Item")
#define MES_MEDT	T("MEDT|&Edit Menu")
#define MES_MEMD	T("MEMD|&Modify Item")
#define MES_MERE	T("MERE|&Rename Item name")
#define MES_MGNR	T("MGNR|&General")
#define MES_MIGE	T("MIGE|Need MIGEMO.DLL")
#define MES_MTCL	T("MTCL|Close Tree(&X)")
#define MES_MWOP	T("MWOP|Wait")
#define MES_MTDR	T("MTDR|use Rename &Dialog")
#define MES_MTFA	T("MTFA|F&avorites")
#define MES_MTFO	T("MTFO|Folder &tree")
#define MES_MTFP	T("MTFP|&Focus PPc")
#define MES_MTMI	T("MTMI| Si&mple icon")
#define MES_MTOL	T("MTOL| icon &overlay")
#define MES_MTNF	T("MTNF|&Create folder")
#define MES_MTNU	T("MTNU|Create &User tree")
#define MES_MTPL	T("MTPL|&PPc list")
#define MES_MTRL	T("MTRL|&Reload")
#define MES_MTSB	T("MTSB|Show tool &bar")
#define MES_MTSH	T("MTSH|S&how proteced folder")
#define MES_MTSI	T("MTSI|&Show icon")
#define MES_MTSL	T("MTSL|&Line")
#define MES_MTSN	T("MTSN|Single &Node")
#define MES_MTSO	T("MTSO|Sort")
#define MES_MTSS	T("MTSS|S&ync-select")
#define MES_MTST	T("MTST|&Settings")
#define MES_MTUE	T("MTUE|Custo&mize User list")
#define MES_MTVF	T("MTVF|View")
#define MES_MSAV	T("MSAV|Save setting")
#define MES_NEWD	T("NEWD|Enter new disk")
#define MES_NITX	T("NITX|&Text File\t.txt")
#define MES_NOEL	T("NOEL|File any mach")
#define MES_NOIC	T("NOIC|Setup is not completed. It is better to execute a SETUP.EXE.")
#define MES_NOTX	T("NOTX|String not found.")
#define MES_NWDN	T("NWDN|TEMPDIR")
#define MES_NWFN	T("NWFN|TEMPFILE")
#define MES_OPSA	T("OPSA|Open selected &Address")
#define MES_OWDT	T("OWDT|Directory t&ypes")
#define MES_OWIT	T("OWIT|Se&nd to")
#define MES_PACI	T("PACI|individual archive")
#define MES_PACO	T("PACO|other")
#define MES_PACT	T("PACT|Archive type")
#define MES_PJAD	T("PJAD|Add path")

// 確認関係
#define MES_QABO	T("QABO|Break job?")
#define MES_QBAK	T("QBAK|Backup path?")
#define MES_QCOM	T("QCOM|Continue action With marked entry off screen?")
#define MES_QCRD	T("QCRD|Create new directory?")
#define MES_QDDL	T("QDDL|Delete Delayed list?")
#define MES_QDDO	T("QDDO|Start delayed list task?")
#define MES_QDFA	T("QDFA|Delete file of special attributes?")
#define MES_QDL1	T("QDL1|delete?")
#define MES_QDL2	T("QDL2|,,,\n")
#define MES_QDL3	T("QDL3|entries delete?\n")
#define MES_QDL4	T("QDL4|erase form tree?")
#define MES_QDRC	T("QDRC|Yes\tDuplicate reparse point\nNo\tCopy files")
#define MES_QDRD	T("QDRD|Move to recycle bin?")
#define MES_QDRP	T("QDRP|Yes\tDelete reparse point\nNo\tDelete reparse point  & files")
#define MES_QEFD	T("QEFD|%s\nexisted, delete?")
#define MES_QEWE	T("QEWE|This file seems extracting in the wrong place.Extract?")
#define MES_QICU	T("QICU|Initialize customize, ok?")
#define MES_QIHI	T("QIHI|Initialize history, ok?")
#define MES_QISD	T("QISD|%s is drive name ?\n")\
						T("Yes\tdrive name\n")\
						T("No\tdirectory name\n")
#define MES_QMCF	T("QMCF|Create comments file?")
#define MES_QOSL	T("QOSL|File size too large. Read file image partially?")
#define MES_QPPC	T("QPPC|Quit PPc?")
#define MES_QPPV	T("QPPV|Quit PPv?")
#define MES_QSAM	T("QSAM|Over write ok?")
#define MES_QSAN	T("QSAN|Found %d same name files. Select processing method.")
#define MES_QSAO	T("QSAO|Found same name file. Select processing method.")
#define MES_QSAV	T("QSAV|Save edited data?")
#define MES_QSDP	T("QSDP|Setting as dest path of pair windows?")
#define MES_QSIZ	T("QSIZ|Need PPx's shutdown, ok?")
#define MES_QSP2	T("QSP2|Seems need to divide by 2G, file.Divide file?")
#define MES_QSPT	T("QSPT|No free space.Divide file?")
#define MES_QSPW	T("QSPW|Seems no free space.Divide file?")
#define MES_QSUD	T("QSUD|Start undo?")
#define MES_QSXS	T("QSXS|Waiting synchronizing...")
#define MES_QTWP	T("QTWP|Connect TORO's Library(") T(TOROsWEBPAGE) T(")?")
#define MES_QUPD	T("QUPD|Update customize ?\n\n")	\
						T("Yes\tBackup & update\n") \
						T("No\tUpdate without backup\n") \
						T("Cancel\tNo update\n")
#define MES_QUSV	T("QUSV|Write using viewer application")
#define MES_QWRD	T("QWRD|Order write?")
#define MES_REGP	T("REGP|Select and Regist program")
#define MES_RKAL	T("RKAL|Key command reentry error.")
#define MES_RRPC	T("RRPC|Require restart PPc")
#define MES_RTPC	T("RTPC|Require restart PPc, soon")
#define MES_SACO	T("SACO|Size count... [PAUSE]:Break")
#define MES_SAVE	T("SAVE|Save")
#define MES_SAVP	T("SAVP|Save position")
#define MES_SCMP	T("SCMP|Start Compare")
#define MES_SDPF	T("SDPF|Set dump file")
#define MES_SHCM	T("SHCM|&ShellContext")
#define MES_SIOF	T("SIOF|Sync Information:Off")
#define MES_SION	T("SION|Sync Information:On")
#define MES_SLMD	T("SLMD|Enter slow mode")
#define MES_SNAN	T("SNAN|(add number)")
#define MES_SNAP	T("SNAP|(append)")
#define MES_SNBK	T("SNBK|(backup)")
#define MES_SNCN	T("SNCN|(change name)")
#define MES_SNCP	T("SNCP|(copy)")
#define MES_SNDE	T("SNDE|(delete)")
#define MES_SNKP	T("SNKP|(keep)")
#define MES_SNLN	T("SNLN|(link)")
#define MES_SNMV	T("SNMV|(move)")
#define MES_SNOW	T("SNOW|(overwrite)")
#define MES_SNRE	T("SNRE|(restore)")
#define MES_SNSK	T("SNSK|(skip)")
#define MES_SPOF	T("SPOF|Sync properties : off")
#define MES_SPON	T("SPON|Sync properties : on")
#define MES_SSRP	T("SSRP|Input program path(%s)")
#define MES_SSRT	T("SSRT|sorting...")
#define MES_SVOF	T("SVOF|SyncView : off")
#define MES_SVON	T("SVON|SyncView : on")
#define MES_SWPD	T("SWPD|move window to desktop")
#define MES_TABC	T("TABC|&Close")
#define MES_TABD	T("TABD|&Default Color")
#define MES_TABE	T("TABE|&Eject")
#define MES_TABH	T("TABH|&Hide")
#define MES_TABI	T("TABI|&Save tab width")
#define MES_TABK	T("TABK|select tab by &key")
#define MES_TABL	T("TABL|&Lock")
#define MES_TABN	T("TABN|&New tab")
#define MES_TABP	T("TABP|new &Pane")
#define MES_TABR	T("TABR|c&Olor")
#define MES_TABS	T("TABS|clo&Sed PPc")
#define MES_TABW	T("TABW|s&Wap pane")
#define MES_TACL	T("TACL|close le&Ft")
#define MES_TACP	T("TACP|close p&Ane")
#define MES_TACR	T("TACR|close &Right")

#define MES_TAPN	T("TAPN|Append Filename")
#define MES_TATR	T("TATR|Attribute")
#define MES_TC22	T("TC22|SHA-2&24")
#define MES_TC25	T("TC25|SHA-2&56")
#define MES_TC32	T("TC32|&CRC32")
#define MES_TCFC	T("TCFC|File &type")
#define MES_TCHL	T("TCHL|&Hard links")
#define MES_TCHP	T("TCHP|Unpack(http)")
#define MES_TCIT	T("TCIT|Info Ti&p text")
#define MES_TCLE	T("TCLE|Column Ex&t")
#define MES_TCLR	T("TCLR|C&lear")
#define MES_TCM5	T("TCM5|&MD5")
#define MES_TCME	T("TCME|&Edit comment")
#define MES_TCMT	T("TCMT|Commen&t")
#define MES_TCS1	T("TCS1|&SHA-1")
#define MES_TCOW	T("TCOW|&Owner")
#define MES_TDAD	T("TDAD|D&D Window List")
#define MES_TDEL	T("TDEL|Delete")
#define MES_TDFO	T("TDFO|Delayed file operation")
#define MES_TDUP	T("TDUP|Dup")
#define MES_TEDJ	T("TEDJ|Enter editor\'s line locate option")
#define MES_TENN	T("TENN|Enter new name")
#define MES_TEIF	T("TEIF|Entry information")
#define MES_TEXE	T("TEXE|Execute")
#define MES_TFAC	T("TFAC|PPc file action")
#define MES_TFCP	T("TFCP|Compare")
#define MES_TFEM	T("TFEM|Find mask")
#define MES_TFMK	T("TFMK|FindMark")
#define MES_TFUM	T("TFUM|FindUnMark")
#define MES_THIL	T("THIL|Highlight words")
#define MES_TINS	T("TINS|Input new menu item name")
#define MES_TISD	T("TISD|Path fix")
#define MES_TLDS	T("TLDS|Logdisk")
#define MES_TLNO	T("TLNO|Line number")
#define MES_TMCF	T("TMCF|Create comments file")
#define MES_TMHL	T("TMHL|Hard link")
#define MES_TMKD	T("TMKD|maKe directory")
#define MES_TMKF	T("TMKF|maKe file")
#define MES_TPAC	T("TPAC|Pack file:")
#define MES_TPCL	T("TPCL|PPc log")
#define MES_TPTX	T("TPTX|Plain &Text")
#define MES_TRDE	T("TRDE|Read entry")
#define MES_TREC	T("TREC|Continuous rename")
#define MES_TREN	T("TREN|Rename")
#define MES_TSCP	T("TSCP|Select text code page")
#define MES_TSDR	T("TSDR|Select directory")
#define MES_TSEI	T("TSEI|Sync Entry information")
#define MES_TSFN	T("TSFN|Select file")
#define MES_TSHL	T("TSHL|sHell")
#define MES_TSMD	T("TSMD|Module(&T)")
#define MES_TSVN	T("TSVN|Save Filename")
#define MES_TTAB	T("TTAB|TAB SIZE")
#define MES_TTRE	T("TTRE|Tree")
#define MES_TTWP	T("TTWP|Jump web site")
#define MES_TUPA	T("TUPA|Unpack archive")
#define MES_TWRD	T("TWRD|Write entry order")
#define MES_TXAT	T("TXAT|All text")
#define MES_TXSL	T("TXSL|Selected lines")
#define MES_UCMD	T("UCMD|:Unknown command")
#define MES_UDED	T("UDED|Update entry data(&.)")
#define MES_UNKN	T("UNKN|Unknown")
#define MES_UPED	T("UPED|Updated %s")
#define MES_USTD	T("USTD|Action is unsupported this directory")
#define MES_VFSL	T("VFSL|VFS load")
#define MES_VFSU	T("VFSU|VFS unload")
#define MES_VTAI	T("VTAI|T&ail mode")
#define MES_VTDC	T("VTDC|Document")
#define MES_VTHE	T("VTHE|Hex")
#define MES_VTIM	T("VTIM|Image")
#define MES_VTRI	T("VTRI|Raw Image")
#define MES_VTTX	T("VTTX|Text")
#define MES_WITD	T("WITD|Need WININET.DLL")
#define MES_WKSI	T("WKSI|Keyname seems invalid")
#define MES_WLLP	T("WLLP|Long path directory")
#define MES_WNOA	T("WNOA|No \'@\' prefix")
#define MES_WUML	T("WUML|width unmatch line %d(this width:%d, max width:%d)")
#define MES_XUUE	T("XUUE|Server access error.")
#define MES_XUUN	T("XUUN|Not found update file.")
#define MES_XUUQ	T("XUUQ|Find version %s. Update from %s ?")

//#define XUUB	T("XUUB|%s is not known TORO's site.\nBegin download?") 未使用
//#define QECS	There are few customize area. Extend? 0.50廃止
//#define IECS	Customize area extension was ready. 0.50廃止
//#define NOES	any mach 1.28廃止
//#define ERED	Read error 1.28廃止
//#define 7883	一時設定 1.70 ボタン→ボタンメニューに変更
//#define 78C5	前方一致&F 1.74 廃止
//#define ITFC	file+c 1.74 廃止

/* 初期カスタマイズ内で使用している項目
	MES_CDAT	MES_CDCM	MES_CDD1	MES_CDD2	MES_CDFN	MES_CDI1	.
	MES_CDD2	MES_CDFN	MES_CDI1	MES_CDI2	MES_CDI3	MES_CDID	.
	MES_CDL1	MES_CDL2	MES_CDL3	MES_CDT1	MES_CDT2	MES_CDT3	.
	MES_JMAL	MES_JMCL	MES_JMCR	MES_JMCU	MES_JMPA	MES_JMPR	.
	MES_JMPS	MES_JMPV	MES_JMTE	MES_MTDL	MES_MTDS	MES_MTED	.
	MES_MTNW	MES_MTRS	MES_MTRT	.
	MES_MTSC	MES_PJCP	MES_PJDE	MES_PJDL	.
	MES_PJMD	MES_PJRB	*/

#define POPMSG_MSG	0
#define POPMSG_NOLOGFLAG	B29
#define POPMSG_NOLOGMSG		(POPMSG_NOLOGFLAG+0)
#define POPMSG_PROGRESSMSG	(POPMSG_NOLOGFLAG+1)	// 進捗状況
#define POPMSG_PROGRESSBUSYMSG	(POPMSG_NOLOGFLAG+2) // 進捗状況+完了待ち中
#define POPMSG_GETLASTERROR	(POPMSG_NOLOGFLAG-1)	// エラーコード表示
