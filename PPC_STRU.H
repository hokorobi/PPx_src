/*-----------------------------------------------------------------------------
	Paper Plane cUI												構造体定義
-----------------------------------------------------------------------------*/
#include "TM_MEM.H"
#include "PPX_DRAW.H"
#include "PPX_CV.H"
#include "PPX_DOCK.H"
#include "PPC_DISP.H"

#ifdef __cplusplus
extern "C" {
#endif

// おまけ設定
#define FREEPOSMODE 0			// FreePositon 可能なエントリ数
#define USEGRADIENTFILL 0		// =1 グラデーションカーソル
#define USEDELAYCURSOR	0		// =1 遅延移動カーソル(要USEGRADIENTFILL=1)
#define SHOWFRAMERATE	0		// =1 表示速度を表示 & 常時描画

#if defined(USEDIRECTX) && !defined(USEDIRECTWRITE)
	#undef USEDELAYCURSOR
	#define USEDELAYCURSOR 1
	#undef SHOWFRAMERATE
	#define SHOWFRAMERATE 1
#endif

// 各種定義
#define NOFREEPOS 0x7fffffff	// 位置変更無し

// WndProc
#define WM_PPCEXEC	(WM_APP+1064) // ツールバーから座標付きでコマンド実行
#define WM_PPCSETFOCUS	(WM_APP+1065) // D&Dからフォーカスを変えるときに使用
#define WM_PPCFOLDERCHANGE	(WM_APP+1066) // explorer shell からの更新通知
#define WM_PPCSETCOMMENT	(WM_APP+1067) // コメント登録
// TipWndProc
#define WM_PPCTIPPOS	(WM_APP+1071) // チップの表示位置を通知
#define WM_PPCPREVIEWPOS	(WM_APP+1072) // プレビューチップの表示位置を通知
// ComboProcMain
#define WM_PPCOMBO_NESTED_ENTRY (WM_APP+1075) // KCW_entry で再入になってしまった窓を再登録させる
#define WM_PPCOMBO_NESTED_DESTROY (WM_APP+1076) // destroy window で再入になってしまった窓を再登録解除させる
#define WM_PPCOMBO_PRECLOSE (WM_APP+1077) // １窓を閉じる通知

#define CLIPTYPES 4	// クリップボードで扱う種類の数
#define CLIPTYPES_SHN 0
#define CLIPTYPES_HDROP 1
#define CLIPTYPES_TEXT 2
#define CLIPTYPES_DROPEFFECT (CLIPTYPES - 1) // 一番最後にする必要有り
#define MAXPATHTRACKINGLIST	30000	// ディレクトリ履歴一覧の最大文字数

#define SubTs		2	// サブスレッドの同期用に使うオブジェクト数
#define SubT_cmd	SubT[0] // 指令発行用イベント(SubTf)
#define SubT_dir	SubT[1] // ディレクトリ書き込み監視
							// SubTCmdFlags(SubT_cmd)に使用するフラグ
#define SUBT_EXIT			B0	// サブスレッド終了
#define SUBT_INITDIRCHECK	B1	// ディレクトリ書き込み監視の初期化
#define SUBT_STOPDIRCHECK	B2	// ディレクトリ書き込み監視の停止
#define SUBT_GETINFOICON	B3	// 情報行のアイコン取得
#define SUBT_GETCELLICON	B4	// 一覧のアイコン取得
#define SUBT_REQUESTRELOAD	B5	// 時間経ってから更新
#define SUBT_GETDIRSIZECACHE	B6	// ディレクトリサイズのキャッシュ取得
#define SUBT_GETCOLUMNEXT	B7	// カラム拡張内容取得
#define SUBT_GETFREESPACE	B8	// 空き容量取得
#define SUBT_FOLDERCHANGE	B9

#define SENDCOMBO_OK	106	// 正常に通信が成功したときの値

#define REMOTE_READ_WIDTH 20 // 一度に転送する量
#define HDN_BEGINDRAG           (HDN_FIRST-10)

#define ICONBLANK	1	// アイコン周囲の余白量
#define MARKOVERX	3	// マーククリック領域がマーク／アイコンからはみ出す量

extern TCHAR ComboID[REGIDSIZE];
// 起動時パラメータ指定 -------------------------------------------------------
typedef struct tagPSPONE {	// 各ID のオプション
	struct {
		TCHAR RegID[REGIDSIZE]; // 使用予定 ID(必ず先頭にすること)
		int  RegMode;	// IDの登録方法 PPXREGIST_xxx
		int  SubID;		// ID Zciii の sub ID(iii部分)
		BOOL Pair;		// 反対窓指定
	} id;
	struct {	// 一体化関連
		int use;		// 一体型にするか(X_comboの値)
		BOOL dirlock;	// ディレクトリ移動のロック
		BOOL select;	// タブ選択動作を行う

		#define PSPONE_PANE_DEFAULT 0 // デフォルト
		#define PSPONE_PANE_PAIR 1	// 反対窓
		#define PSPONE_PANE_NEWPANE 2 // 新規ペイン
		#define PSPONE_PANE_RIGHTPANE 3 // 右/上側ペイン
		#define PSPONE_PANE_SETPANE 4 // 指定ペイン
		int pane; // 登録先ペイン PSPONE_PANE_xx
	} combo;
	TCHAR path[VFPS];	// 表示ディレクトリ(必ず末尾にすること)
} PSPONE;
#define PSPONE_size(pspo) ((char *)&(pspo)->path[0] - (char *)&(pspo)->id.RegID + TSTRSIZE((pspo)->path))
#define PSPONE_next(pspo) ((PSPONE *)(char *)((char *)pspo + (DWORD)PSPONE_size(pspo)))

typedef struct tagPPCSTARTPARAM {	// 共通オプション
	BOOL SingleProcess;	// シングルプロセス -single
	BOOL Reuse;			// 再利用 -r
	BOOL UseCmd;		// cmd を参照する必要があるか
	BOOL AllocCmd;		// cmd は動的メモリであるか
	const TCHAR *cmd;	// -k の内容
	BOOL usealone;		// -alone をつかわないなら 0、そうでなければ ID
	int show;			// 表示方法 ※要初期化

	ThSTRUCT th; // PSPONE / cmd 用メモリ
	TCHAR Focus;
	TCHAR ComboID;
	PSPONE *next;
} PPCSTARTPARAM;

// メインスレッドで開いているウィンドウの管理 ---------------------------------
typedef struct tagMAINWINDOWSTRUCT {
	struct tagMAINWINDOWSTRUCT *next;
	struct tagPPC_APPINFO *cinfo; // ウィンドウに関連付いているメモリ... 使用していないときはNULL
	DWORD ThreadID;
	int NowClose; // Close処理中なら 1、Close処理完了しDestroy待ちなら 2
	BOOL DestroryRequest; // Windowを改めて閉じる必要があるなら TRUE
} MAINWINDOWSTRUCT;

// ----------------------------------------------------------------------------
typedef struct {
	RECT NCRect;	// 非クライアント領域を含むウィンドウのスクリーン座標
	SIZE NCArea;	// 非クライアント領域を含むウィンドウの丈＆幅
//	RECT Rect;	// クライアント領域のスクリーン座標
	SIZE Area;	// クライアント領域の丈＆幅
} WNDINFO;

typedef struct {
	SIZE VArea;	// セルの表示桁数＆行数
	SIZE Area;	// セルのカーソル移動範囲桁数＆行数
	SIZE Size;	// セルの大きさ/dot
} CELLINFO;

// カーソル移動の挙動 ---------------------------------------------------------
#define CMOVEUNIT_TYPEMASK (3 | 16)
#define CMOVEUNIT_TYPEDEFAULT	0 // 規定値
#define CMOVEUNIT_TYPEPAGE		1
#define CMOVEUNIT_TYPESCROLL	2
#define CMOVEUNIT_MARKUPDOWN 	16
#define CMOVEUNIT_MARKPREVNEXT 	17
#define CMOVEUNIT_TYPELOCKSCROLL 3
#define CMOVEUNIT_RATEDIV 4
#define CMOVEUNIT_RATEUPDOWN	0
#define CMOVEUNIT_RATECOLUMN	1
#define CMOVEUNIT_RATEPAGE		2
#define CMOVEUNIT_RATEDECIPAGE	3

#define OUTTYPE_STOP			0	// 停止
#define OUTTYPE_WL				1	// 画面反対(線対称)
#define OUTTYPE_WP				2	// 画面反対(点対称)
#define OUTTYPE_LINESCROLL		3	// 行スクロール
#define OUTTYPE_COLUMNSCROLL	4	// 桁スクロール
#define OUTTYPE_PAGE			5	// ページ切替
#define OUTTYPE_PARENT			6	// [BS]
#define OUTTYPE_DRIVE			7	// \[L]
#define OUTTYPE_LRKEY			8	// [←]/[→]使用
#define OUTTYPE_PAGEKEY			9	// page up/down使用
#define OUTTYPE_FPPC			10	// 前のPPcへ
#define OUTTYPE_NPPC			11	// 次のPPcへ
#define OUTTYPE_FDRIVE			12	// 前のドライブへ
#define OUTTYPE_NDRIVE			13	// 次のドライブへ
#define OUTTYPE_PAIR			14	// 反対窓のPPcへ
#define OUTTYPE_HALFPAGE		15	// 半ページ切替

#define OUTTYPE_RANGEEVENT12	16	// RANGEEVENT1 / RANGEEVENT2 発行
#define OUTTYPE_RANGEEVENT34	17	// RANGEEVENT3 / RANGEEVENT4 発行
#define OUTTYPE_RANGEEVENT56	18	// RANGEEVENT5 / RANGEEVENT6 発行
#define OUTTYPE_RANGEEVENT78	19	// RANGEEVENT7 / RANGEEVENT8 発行

#define OUTHOOK_HOOK			B0 // 引っかけ
#define OUTHOOK_EDGE			B1 // 先頭/末尾へ移動
#define OUTHOOK_WINDOWJUMP		B2 // 二枚窓時は窓間移動を優先
#define OUTHOOK_MARK			B3 // マークする

typedef struct {
	int unit; // 移動単位 下位2bit:CMOVEUNIT_TYPExxx 上位2bit:CMOVEUNIT_RATExxx
	int offset; // 移動量
					// 画面外に移動する場合の処理
	int outw_type;	// OUTTYPE_xxx
	int outw_hook;	// OUTHOOK_xxx
					// 範囲外に移動する場合の処理
	int outr_type;	// OUTTYPE_xxx
	int outr_hook;	// OUTHOOK_xxx
} CURSORMOVER;

// 各セルの情報 ---------------------------------------------------------------
// type / C_entry 用 定義
#define ECT_SYSMSG	0	// System message  ※f.nFileSizeLow に ERRORCODE
#define ECT_THISDIR	1	// This directory
#define ECT_UPDIR	2	// Up directory
#define ECT_LABEL	3	// Label
#define ECT_SUBDIR	4	// Sub directory
#define ECT_SYSTEM	5	// System
#define ECT_HIDDEN	6	// Hidden
#define ECT_RONLY	7	// Read only
#define ECT_NORMAL	8	// Normal

#define ECT_COMPRES	9	// Compressed & Spase
#define ECT_REPARSE	10	// Reparse Point
#define ECT_VIRTUAL 11	// Virtual & Offline
#define ECT_ENCRYPT 12	// Encrypted
#define ECT_SPECIAL 13	// Temporary & Device

#define ECT_NOFILEDIRMAX ECT_LABEL // ファイル・ディレクトリ以外の最大値

// state 用 定義
#define ECS_MESSAGE	0	// System Message
#define ECS_DELETED	1	// Deleted
#define ECS_NORMAL	2	// Normal
#define ECS_GRAY	3	// Gray
#define ECS_CHANGED	4	// Changed
#define ECS_ADDED	5	// Added
#define ECS_NOFOCUS	6	// No Focus
#define ECS_BOX		7	// 枠
#define ECS_UDCSR	8	// 下線カーソル
#define ECS_MARK	9	// 「*」
#define ECS_EVEN	10	// Normal/Even
#define ECS_SELECT	11	// Selected
#define ECS_SEPARATE	12	// Separate h line
#define ECS_HILIGHT	13	// hilight1-7
#define ECS_HOVER	(ECS_HILIGHT + ECS_HLMAX)	// mouse hover

#define ECS_HLMAX	PPC_HIGHLIGHT_COLORS	// highlight max
#define ECS_HLBIT	32	// highlight
#define ECS_STATEMASK	7
#define ECS_HLMASK	0x1f

// attr 用 定義
#define ECA_PARENT	B0	// ..
#define ECA_THIS	B1	// .
#define ECA_DIRNOH	B2	// directory(キャッシュ確認済み)
#define ECA_DIRG	B3	// directory(Gray)
#define ECA_DIRC	B4	// directory(使用量算出済み)
#define ECA_DIR		B5	// directory
#define ECA_ETC		B6	// ファイルやディレクトリ以外のオブジェクト
#define ECA_GRAY	B7	// Gray / 状態未確定
#define ECAX_FORCER	B31	// ./.. の強制表示 ※rdirmaskのみ使用

typedef INT ENTRYINDEX; // INDEXDATA 内の位置。表示エントリ順の位置
typedef INT ENTRYDATAOFFSET; // CELLDATA 内の位置。エントリ格納順の位置
typedef INT ENTRYCOUNT; // エントリ数

// 列挙状態
#define CELLENUMID_JUMPFIRST -4 // 最初のマークに戻す(*fixmark用)
#define CELLENUMID_ONCURSOR -3 // カーソル上(%Rなど)
#define CELLENUMID_END -2 // 列挙終了

// マークの状態
#define ENDMARK_ID -2 // マークの末端
#define NO_MARK_ID -1 // マークされていないとき

//#define NextEntryData(edo) (edo++)
//#define GetNextEntryData(edo) (edo + 1)

#pragma pack(push, 1)
typedef struct {
									// マーク用ポインタ（テーブルは経由せず）--
	ENTRYDATAOFFSET mark_fw; // 次のマーク位置 (NO_MARK_ID:マーク無し, ENDMARK_ID:末端)
	ENTRYDATAOFFSET mark_bk; // 前のマーク位置 (NO_MARK_ID:(未使用)  , ENDMARK_ID:先頭)
									// 拡張子・ファイル種類関係
	COLORREF extC;	// 拡張子の色

	WORD ext;	// 拡張子位置(ピリオド含む/offset)

	BYTE type;	// ファイルの属性種類(前景色に使用 ECT_
	BYTE state;	// ファイルの状態(背景色 & ハイライトに使用) ECS_

//dword境界
#define EC_NOCOMMENT MAX32
	DWORD comment;// != EC_NOCOMMENT:コメントあり(EntryComments 内の offset)
	DWORD cellhash;	// ソート、同一名確認用ハッシュ
#define CCI_NOLOAD	-1 // 未取得
#define CCI_NODATA	-2 // カラム情報無し
	int cellcolumn;	// カラムインデックス(ColumnData 内の offset)
	int icon;		// アイコンインデックス ICONLIST_xx
	DIRECTXDEFINE(int iconcache) // DirectX BMP 用index
									// ファイル情報 ---------------------------
	WIN32_FIND_DATA f;		// ファイル情報(MultiByte なら WORD分余分)
//MultiByte ならword境界、UNICODE なら dword境界
	BYTE attr;			// ファイルの属性 ECA_
	BYTE attrsortkey;	// 属性ソート用値
#ifdef UNICODE
	WORD udummy;
#endif
// Multibyte : 0x160	UNICODE(32/64) : 0x274
#if FREEPOSMODE
	POINT pos;		// 移動量(移動しているならx!=0)
#endif
} ENTRYCELL;
#pragma pack(pop)
#define ENTRYCELL_LIMIT ((int)(DWORD_PTR)(((DWORD_PTR)2 * GB) / sizeof(ENTRYCELL) - 10)) // 最大エントリ数、x32, M:600万 / x32 x64, U:340万

/* 参考：ファイル属性
 DOS 互換系
	b0	READONLY		b1	HIDDEN			b2	SYSTEM			b3	label
	b4	DIRECTORY		b5	ARCHIVE

 Win32 追加分
	b6	DEVICE			b7	NORMAL

	b8	TEMPORARY		b9	SPARSE_FILE
	b10	REPARSE_POINT	b11	COMPRESSED
	b12	OFFLINE			b13	NOT_CONTENT_INDEXED
	b14	ENCRYPTED		b15	INTEGRITY_STREAM

	b16	VIRTUAL			b17	NO_SCRUB_DATA
	b18	EA/RECALL_ON_OPEN	b19	PINNED
	b20	UNPINNED		b21	---
	b22	RECALL_ON_DATA_ACCESS

 createfile flags
						b19 FIRST_PIPE_INSTANCE
	b20 OPEN_NO_RECALL	b21 OPEN_REPARSE_POINT
						b23 SESSION_AWARE
	b24 POSIX_SEMANTICS	b25 BACKUP_SEMANTICS
	b26 DELETE_ON_CLOSE	b27 SEQUENTIAL_SCAN
	b28 RANDOM_ACCESS	b29 NO_BUFFERING
	b30 OVERLAPPED		b31 WRITE_THROUGH

 ppx
						b25 LF_MARK
	b26 LF_COMMENT		b27 FOLDER
	b28 VIRTUAL			b29 NODETAIL
*/
// ----------------------------------------------------------------------------
/* ２窓間比較の方法
	B0-B7	比較方法
	B8-B14	マーク属性
	B15		1:サブディレクトリ内も比較
*/
#define CMP_DETAIL  1	// 詳細メニュー
#define CMP_1HASH 10
#define CMP_1WINDOW 11
#define CMP_2WINDOW 21
#define CMPTYPEMASK 0x7f
#define CMPMARKSHIFT 8
#define CMPMARKMASK 0x1f // ビットシフト後
#define CMPWITHOUT_EXT	B6 // 拡張子を無視
#define CMPDOWN			B7 // コマンド実行元窓、コマンド処理窓の反対窓
#define CMPSUBDIR		B15 // サブディレクトリ以下
#define CMPWAIT			B31 // 実行完了まで待つ、なければ非同期
enum {
	CMP_1SIZE = CMP_1WINDOW, CMP_1COMMENT, CMP_1END
};
enum {
	CMP_NEW = CMP_2WINDOW, CMP_TIME, CMP_EXIST, CMP_SIZE, CMP_LARGE,
	CMP_SAMESIZETIME, CMP_BINARY, CMP_SIZEONLY, CMP_NEWONLY, CMP_SHA1,
	CMP_2END
};
// ↓ +1 は、CMP_2END(CMP_EXIST | CMPWITHOUT_EXT) 分
#define CMP2TYPES (CMP_2END - CMP_2WINDOW + 1)	 // ２窓間比較方法の数

#define USEWINHASH 0
//------------------------------------------------------------------
/* 設定ファイル内の構造体
typedef struct {
	COLORREF	fc;		// 前景色
	COLORREF	bc;		// 背景色
	int			csr;	// カーソルの表示方法
	int			mark;	// マークの表示方法
	WORD		width;	// 表示幅
	WORD		nextline; // 次の行へのオフセット
	BYTE		fmt[];	// 書式
} XC_CFMT_SETTING;
※ XC_celDは fc, bc, csr, mark, XC_celF/MC_celSはwidth, fmt が入る
*/
typedef struct {
	COLORREF	fc;		// 前景色
	COLORREF	bc;		// 背景色
	int			csr;	// カーソルの表示方法
	int			mark;	// マークの表示方法
	int			width;	// 表示幅
	int			height;	// 表示丈
	int			nextline; // 次の行へのオフセット
	int			attr;	// 表示する属性(DE_ATTR_STATICなど)
	BYTE		*fmt;	// 書式
	BYTE		*fmtbase;	// 書式
} XC_CFMT;

typedef struct {
	#define MASKEXTATTR_DIR		B16 // ディレクトリをマスク判断の対象に
	#define MASKEXTATTR_WORDMATCH	B17 // 単語検索(部分一致)
	#define MASKEXTATTR_MASK	0xffff
	DWORD attr;				// 表示する属性
	TCHAR file[MAX_PATH];	// 表示するファイル
} XC_MASK;

typedef BOOL (APIENTRY *impGetDiskFreeSpaceEx)(LPCTSTR lpDirectoryName,
					PULARGE_INTEGER lpFreeBytesAvailableToCaller,
					PULARGE_INTEGER lpTotalNumberOfBytes,
					PULARGE_INTEGER lpTotalNumberOfFreeBytes);

typedef struct {
	DWORD StartTime;
	DWORD OldTime;
	DWORD count;
//	DWORD useaction;
//	DWORD action;
} JOBINFO;

#define PPCTREE_OFF		0
#define PPCTREE_SELECT	1	// 選択後終了
#define PPCTREE_SYNC	2	// 連動モード
#define PPCTREECOMMAND	3	// comboへのコマンド送信用
typedef struct {
	DWORD mode;
	int width;
} XC_TREE;

#define CELLHASH_NONE	0	// 未使用
#define CELLHASH_NAME	1	// ^F5 用同一名チェック

struct dirinfo { // SetCountedDirectorySize 用
	TCHAR path[VFPS];
	TCHAR name[MAX_PATH];
	DWORD low, high;
};

#if defined(_INC_COMMCTRL)
TypedefWinAPI(HIMAGELIST, ImageList_Create, (int cx, int cy, UINT flags, int cInitial, int cGrow));
TypedefWinAPI(BOOL, ImageList_Destroy, (HIMAGELIST himl));
TypedefWinAPI(int, ImageList_Add, (HIMAGELIST himl, HBITMAP hbmImage, HBITMAP hbmMask));
TypedefWinAPI(int, ImageList_Replace, (HIMAGELIST himl, int i, HBITMAP hbmImage, HBITMAP hbmMask));
TypedefWinAPI(int, ImageList_ReplaceIcon, (HIMAGELIST himl, int i, HICON hicon));
TypedefWinAPI(COLORREF, ImageList_SetBkColor, (HIMAGELIST himl, COLORREF clrBk));
TypedefWinAPI(BOOL, ImageList_Draw, (HIMAGELIST himl, int i, HDC hdcDst, int x, int y, UINT fStyle));
TypedefWinAPI(BOOL, ImageList_DrawEx, (HIMAGELIST himl, int i, HDC hdcDst, int x, int y, int dx, int dy, COLORREF rgbBk, COLORREF rgbFg, UINT fStyle));
#define DImageList_AddIcon(himl, hicon) DImageList_ReplaceIcon(himl, -1, hicon)
#else
	#define HIMAGELIST void *
#endif

typedef void *pCPPcAccServer;

enum {
	MOUSE_NONE,			// マウス操作：なし
	MOUSE_CANCEL,		// 押下中(キャンセル)
	MOUSE_PUSH,			// 押下中(何もしない)
	MOUSE_DnDCHECK,		// 左・右クリックOLE2 D&D 開始検知中
//	MOUSE_RDDCHECK,		// 右クリックOLE2 D&D 開始検知中
//	MOUSE_MOVECHECK,	// 窓移動開始検知中
	MOUSE_WNDMOVE,		// 窓移動中
	MOUSE_MARKCHECK,	// マーク開始検知中
	MOUSE_MARK,			// マーク中
//	MOUSE_GESTURECHECK,	// ジェスチャー開始検知中
	MOUSE_GESTURE,		// ジェスチャー中
	MOUSE_SWIPECHECK,	// スワイプチェック中
	MOUSE_SWIPE			// スワイプ中
};

struct ddwndstruct {
	HWND hTreeWnd;
	HWND hTreeViewWnd;
	HWND hTargetWnd;
	HIMAGELIST hTreeImage;
	WNDPROC OldTreeWndProc;
	struct tagPPC_APPINFO *cinfo;
};

typedef struct {
	int nMax;
	UINT nPage;
	int nPos;
} OLDSCROLLPOS;

#define WDEXM_UNKNOWN 0
#define WDEXM_PLAINTEXT 1
#define WDEXM_COMMENT 2
#define WDEXM_TIPTEXT 3
#define WDEXM_COLUMNMIN 4
#define WDEXM_MODULE 5
typedef struct {
	struct tagPPC_APPINFO *cinfo;
	BOOL marked; // -marked
	BOOL dir; // -dir
	DWORD sdir; // -subdir, 0 か FILE_ATTRIBUTE_DIRECTORY で判断
	DWORD exmode; // -type, 検索対象 WDEXM_xx
	TCHAR mask1[VFPS]; // -mask1
	TCHAR mask2[VFPS]; // -mask2
	TCHAR text[VFPS]; // -text
} WHERESETTINGS;

typedef struct {
	HWND hWnd; // WhereIsMain only
	HWND hEdWnd; // dialog only

	const TCHAR *listfilename; // -listfile, 出力するlistfile
	TCHAR srcpath[VFPS]; // -path

	DWORD hitcount; // WhereIsMain only
	DWORD OldTime; // 経過表示用 WhereIsMain only
	DWORD imgsize; // X_wsiz の値 WhereIsMain only
	DWORD baselen; // パスの省略可能な長さ WhereIsMain only

	int pausecount; // 操作割り込み間隔のカウンタ WhereIsMain only
	int indexEc; // 使用するカラム拡張
	int wlfc_flags; // -name, -utf8, -utf8bom

	BOOL readlistfile; // -search, 終了後読み込み
//	BOOL appendmode; // 直接挿入
	BOOL vfs; // -vfs
	BOOL UseFnText; // -text で 単純一致 / 正規表現

	ThSTRUCT thEcdata; // カラム情報記憶用

	FN_REGEXP fn1, fn2; // -mask1,-mask2 WhereIsMain only
	FN_REGEXP fnText; // -text(正規表現時)
	WHERESETTINGS st;
} WHERESTRUCT;

typedef struct {
	CRITICAL_SECTION section;	// ref を変更するときにシリアライズする為に使用
	volatile int ref;			// 編集中なら 0 以外
} LOADINGSTRUCT;

// swin の設定
#define SWIN_JOIN		B0	// 連結処理をする
#define SWIN_FIT		B1	// 連結する窓をそろえる
#define SWIN_WBOOT		B2	// 同時起動
#define SWIN_WQUIT		B3	// 同時終了
#define SWIN_FIXACTIVE	B4	// アクティブ窓を固定
#define SWIN_FIXPOSIZ	B5	// B4=1のとき、=0:大きさ =1:大きさ+位置
#define SWIN_JOINMASK	(B6 | B7)	// (B6, B7)	左右連結
#define SWIN_LRJOIN		0	// (B6, B7)	左右連結
#define SWIN_UDJOIN		B6	// (B6, B7)	上下連結
#define SWIN_PILEJOIN	B7	// (B6, B7)	重ね
#define SWIN_WACTIVE	B8	//  B8:=1 同時前面
#define SWIN_SWAPMODE	B16	//	B16:[G]Swap Sw		=0:Normal	=1:Swap
#define SWIN_BFOCUES	B17	//	B17:Focus PPC		=0:A	=1:B
#define SWIN_BUSY		B18	//	B18:作業中			=0:Idle	=1:Busy

// FDirWrite の設定
#define FDW_NORMAL		0	// ディレクトリ更新チェック中
#define FDW_WAITREQUEST	1	// dir更新検出済、一覧操作中のため、送信を１秒待機
#define FDW_SENDREQUEST	2	// main threadへ通知済み
#define FDW_REQUEST		3	// グレー化済み、表示更新要求(^[F5])が必要
#define FDW_REQUESTED	4	// 表示更新要求を既に行った

// X_inag
#define INAG_USEGRAY	B0
#define INAG_GRAY		B1
#define INAG_UNFOCUS	B2

typedef struct tagFREEPOSPOINT {
	DWORD index; // 該当entry
} FREEPOSPOINT;

#define ICONLIST_NOMASK		0
#define ICONLIST_MASK		1

#define ICONCACHEINDEX_FIRSTMAX (ICONCACHELISTINDEXMAX * 2)
#define ICONLIST_LOADERROR	-1
#define ICONLIST_NOINDEX	-2
#define ICONLIST_UNKNOWN	-1
#define ICONLIST_BROKEN		-3 // 異常終了の元になった
typedef struct {
	HIMAGELIST hImage; // NULL:アイコン無し、INVALID_HANDLE_VALUE:CacheIcon使用
	int maskmode; // マスク処理の有無
	int minID, maxID;	// 有効なindexの範囲
	int minIDindex;		// minIDに相当するindex
	int nextfixID;		// maxIDがこれに到達したら, minを変更する
	int shift; // １回のシフト量
	int size;		// キャッシュ可能な個数
	int writeIndex;	// alloc==FALSE時に、キャッシュに書き込みする位置
	BOOL alloc; // True:キャッシュ領域拡張中 False:キャッシュ領域の大きさが固定
	DIRECTXDEFINE(int width) // 表示サイズ
	DIRECTXDEFINE(int height)
} ICONCACHESTRUCT;

/* エントリ別のカラム用文字列をたどる為の構造体。
１つ目の場所は、cell->cellcolumn が示す。
*/
typedef struct {
	DWORD nextoffset; // 次の COLUMNDATASTRUCT のオフセット。0なら末端
	WORD itemindex; // この COLUMNDATASTRUCT が保存しているカラム ID
	WORD size; // text の大きさ
	DWORD textoffset; // 保存しているテキストへのオフセット。0なら未取得
} COLUMNDATASTRUCT;

typedef struct tagDelayLoadMenuStruct{ // 遅延設定メニュー
	HMENU hMenu;
	LPARAM lParam;
	void (WINAPI *initfunc)(struct tagDelayLoadMenuStruct *);
	DWORD ID;
	ThSTRUCT *thMenuStr;
} DelayLoadMenuStruct;

#ifdef DEFINEDnDDROP
	class FAR CDropTarget;
	class FAR ComboDropTarget;
#else
	typedef char CDropTarget;
	typedef char ComboDropTarget;
#endif

typedef struct {
	void *ExtData; // 今のドライブの種類
	TCHAR Name[VFSGETFFINFO_TYPESIZE];	// 今のドライブの種類
	TCHAR BasePath[VFPS];
	int mode;		// 今のドライブの種類(VFSDT_)
	int pathtype;	// 今のパスの種類(VFSPT_)
} VFSDIRTYPEINFO;

typedef struct {
	HFONT hFont;
	DWORD FontDPI;
} ControlFontStruct;

typedef struct {
	DWORD l, h;
} D_DWORD;

typedef struct { // ディレクトリの情報
	VFSDIRTYPEINFO Dtype;

	ENTRYCOUNT AllRelativeDirs;// 相対ディレクトリ(.. .)の数(非表示含む)
	ENTRYCOUNT RelativeDirs;	// 相対ディレクトリ(.. .)の数(表示分のみ)
	ENTRYCOUNT Directories;	// ディレクトリの数

	TM_struct CELLDATA;		// ディレクトリエントリ情報(cellDataMax x ENTRYCELL)
	TM_struct INDEXDATA;	// ディレクトリエントリインデックス((cellIMax + cellStack) x ENTRYINDEX)

			// エクスプローラ風マークをおこなう基準位置 -1:基準位置無し
	ENTRYINDEX cellNref;
	ENTRYINDEX cellN;		// 参照中の Cell 位置
	ENTRYINDEX cellIMax;		// Cell の個数、INDEXDATA の有効数、表示個数
	ENTRYDATAOFFSET cellDataMax; // Cell の個数、CELLDATA の有効数、格納数
	int cellStack;	// スタックされたセルの数
	ENTRYINDEX cellPoint;	// マウスカーソルが示す位置
	int cellPointType; // マウスカーソルが示す位置の種類 PPCR_xxx
	D_DWORD MarkSize;

	 //マークリスト管理。-1:なし, -2:最終, 他:そこへ繋がる
	ENTRYDATAOFFSET markTop;	// マークの先頭
	ENTRYDATAOFFSET markLast;	// 最後のマーク
	ENTRYCOUNT markC;			// マーク数
} PPC_DIRECTORYDATA;

typedef struct tagPPC_APPINFO {
											// PPc 情報
	PPXAPPINFO info;		// PPx 共通情報 ※必ず先頭に配置
	MAINWINDOWSTRUCT mws;	// ウィンドウ登録情報
	int Ref;		// 参照カウンタ
	int RegNo;		// 登録 No
	int RegSubIDNo;			// ID が Z の時の補助ID
	TCHAR RegCID[REGIDSIZE];	// Cx 形式
	TCHAR RegID[REGIDSIZE];		// C_x 形式
	// TCHAR RegSubID[REGEXTIDSIZE]; // C_x / C_Zyz 形式
	TCHAR RegSubCID[REGEXTIDSIZE];	 // Cx / CZyzz 形式
	HWND hHeaderWnd;			// 一覧のカラムヘッダ

	#define LIT_UNKNOWN		0
	#define LIT_KEYBOARD	1
	#define LIT_MOUSE		2
	#define LIT_TOUCH		3
	#define LIT_PEN			4
	#define LIT_POINTUI_MIN	LIT_MOUSE
	#define LIT_DIRECTPOINTUI_MIN	LIT_TOUCH
	int LastInputType; // LIT_xxx
	int PopupPosType;			// メニューなどを表示するとき使う座標の種類
								//  (TKEY.H の PPT_ )
	POINT PopupPos;		// メニューなどを表示するとき使う座標
											// 同期
	LOADINGSTRUCT edit;	// セルを編集中かどうかを監視する
	CDropTarget *pDropTarget;

	volatile BOOL BreakFlag;				// [PAUSE] が押されたら TRUE

						// サブスレッド関連
	HANDLE hSubThread; // サブスレッドのハンドル
	HANDLE SubT[SubTs]; // 通信用イベント
	DWORD SubThreadID; // サブスレッドのスレッドID
	unsigned long ShNotifyID; // SHNパスの更新通知ID
	volatile int SubTCmdFlags; // サブスレッドへの指令

	TCHAR		Caption[VFPS];	// タイトルバー表示用文字列
	ThSTRUCT	thGuiWork;		// DockやToolBarなどの記憶領域
	ThSTRUCT	StringVariable;	// %s(i)用領域

											// ディレクトリ情報
	DWORD		LoadCounter;	// 新規読み込みする度に+1されるカウンタ
	DWORD		CellModified;	// 更新状態
	#define CELLMODIFY_NONE	0		// 最新の読み込み
	#define CELLMODIFY_FIXED	1	// 更新後、非表示を行った
	#define CELLMODIFY_MODIFIED	2	// 更新した
	ThSTRUCT	PathTrackingList;	// パス追跡リスト
	BOOL		SlowMode;		// 低速モードが有効かのフラグ

	TCHAR		path[VFPS], RealPath[VFPS]; // 現在の読み込みパス、VFSでないパス
	TCHAR		ArcPathMask[VFPS];	// パス表示マスク
	TCHAR		OrgPath[VFPS];	// テンポラリの元となっているパス
	TCHAR		PathSeparater;	// パス区切りに用いる文字

	BOOL		UseSplitPathName;
	#define ARCPATHMASK_OFF			0
	#define ARCPATHMASK_ARCHIVE		1
	#define ARCPATHMASK_DIRECTORY	2
	int			UseArcPathMask;
	int			ArcPathMaskDepth;
	int			MinArcPathMaskDepth;

	PPC_DIRECTORYDATA e;

	struct {
		ULARGE_INTEGER free, total, used;
	} DiskSizes;
	TCHAR		CommentFile[VFPS];	// 使用したコメントファイル
											// Cell の実体
	ThSTRUCT	EntryComments;	// コメント領域 ENTRYCELL.comment
	ThSTRUCT	ColumnData;		// カラムの取得中身 COLUMNDATASTRUCT
	ThSTRUCT	ColumnExtDlls;	// カラム拡張の項目一覧(COLUMNEXTDATAINFO)保存

	ENTRYINDEX	winOmax;
	ENTRYINDEX	cellWMin;		// 窓の cell 開始番号

#if USEDELAYCURSOR | FREEPOSMODE
	POINT		cellNpos, TargetNpos; // カーソル移動のアニメーション位置
	BOOL		freeCell;
	COLORREF	cellNbc;
#endif
	int			combo;			// !0 : 一体化内。 -1: 親へサイズ変更通知しない
											// Window
	HWND		hActiveWnd;		// フォーカスが来たときにアクティブにする窓。
								//		NULL ならファイル一覧に
	HFONT		hBoxFont;		// オリジナルフォントのハンドル
	int			fontX, fontY;	// フォント大きさ(X, Y)
	DWORD		FontDPI;		// 表示解像度
	ControlFontStruct cfs;		// ヘッダ用フォント
	int			X_textmag;		// フォントの拡大率
	COLORREF	BackColor, CursorColor;
	HBRUSH		C_BackBrush;	// 背景用ブラシ
	HBRUSH		linebrush;		// 枠用ペン先

	int			FullDraw;		// 全クライアント画面描画モードなら真
	DWORD		DrawTargetFlags;	// 描画が必要な箇所 0 なら全面描画
	RECT		DrawTargetCell;
#define DRAWT_1ENTRY		B0
#define DRAWT_ENTRY			B1
#define DRAWT_STATUSLINE	B2
#define DRAWT_INFOLINE		B3
#define DRAWT_PATHLINE		B4
#define DRAWT_ALL	(DRAWT_PATHLINE | DRAWT_STATUSLINE | DRAWT_INFOLINE | DRAWT_ENTRY)

	BGSTRUCT	bg;				// 背景
											// 表示領域用RECT
	WINPOS		WinPos;			// 表示位置
	WNDINFO		wnd;			// ウィンドウ関係
									// 各区画
	RECT		BoxStatus;		// ステータス行
	RECT		BoxInfo;		// 情報行
	RECT		BoxEntries;		// エントリ
	POINT		EntriesOffset;	// エントリ表示位置補正値(スワイプスクロール用)
	int			ToolbarHeight;	// 標準ツールバーの高さ
	int			HeaderHeight;	// ヘッダの高さ

	CELLINFO	cel;			// エントリの寸法詳細
											// Pop Message
	TCHAR		PopMsgStr[CMDLINESIZE];
	int			PopMsgTimer;	// PMF_WAITTIMER のカウンタ
	int			PopMsgFlag;
											// 内部動作
	//	#define MARKMASK_DIR 0x10 // 誤動作する
		#define MARKMASK_FILE 0xf
		#define MARKMASK_DIRFILE 0x1f
	int			MarkMask;			// マークする属性
	TCHAR		UserInfo[0x60];		// タイトルバーに表示するユーザ名

	int			KeyRepeats;
	int			multicharkey;
	int			usereentry; // PPcFileOperation の再入チェック
	int			sizereentry; // ファイルサイズ算出の再入チェック
	int			CommentRentry; // コメントの再入チェック
	int			oldU;			// 情報窓のUsed 欄の表示があるなら1

	OLDSCROLLPOS oldspos;	// スクロールバー更新チェック用
	struct ddwndstruct dds;	// AutoD&D 処理用
	BOOL		ChdirLock;		// ディレクトリ変更ロック(タブのロック)
									// 未整理
	DWORD		CellNameWidth;	// cell中のクリック反応する幅
	WORD		PrevCommand;	// 一つ前に実行したコマンド(インクリメンタルサーチ用)

	HWND		FOPlog;			// エラー用のログ
	BOOL		UnpackFix;		// (暫定)書庫内ファイルの参照中
	TCHAR		*UnpackFixOldPath, *UnpackFixOldRealPath; // (暫定)
	TCHAR		IncSearchString[MAX_PATH];
	TCHAR		Jfname[VFPS];	// readentry 時に移動する場所

	int			NormalViewFlag;	// [N]
	int			SyncViewFlag;	// [Y]
	CALLBACKMODULEENTRY KeyHookEntry;		// キー入力拡張モジュール

	BOOL		IncSearchMode;
#define INCSEARCH_NOTICK MAX32
	DWORD		IncSearchTick;
	ENTRYINDEX	IncSearchOldN;

	int			NowJoint;		// 連結作業中は偽
	HWND		hSyncInfoWnd;

	struct {
		HWND hTipWnd;
			#define stip_mode_filename	1
			#define stip_mode_fileinfo	2
			#define stip_mode_comment	3
			#define stip_mode_preview	4
			#define stip_mode_text		5
			#define stip_mode_togglemax	stip_mode_preview
		int X_stip_mode;
			// 表示指示＆指示状況
			#define STIP_CMD_DELAY	B0	// WM_PAINT 中に表示指示
			#define STIP_CMD_NOW	B1	// コマンド/HOVER/TAILによる
			#define STIP_CMD_HOVER	B2 // HOVERによる表示のため、マウスカーソル移動時に消去が必要
			#define STIP_CMD_MOUSE	B3 // マウス操作(HOVER/TAIL)による表示のため、表示位置をマウスに合わせる。なければCELL位置
			#define STIP_CMD_MASK	(B0 | B1 | B3)
			// 状態
			#define STIP_STATE_SHOW	B4	// 表示中
			#define STIP_STATE_DELAY	B5	// 遅延表示待機中
			#define STIP_STATE_READYMASK	(B4 | B5)
			// その他
			#define STIP_REQ_DELAY		B6	// 遅延表示要求
			#define STIP_SHOWTAILAREA	B7	// TAIL 範囲の表示
		DWORD states;
	} Tip;

	BOOL		ModifyComment;	// コメント編集がされたかどうか
	PPCSTARTPARAM *FirstCommand; // /k で実行する内容を参照する

	MOUSESTATE	MouseStat;

	ENTRYINDEX	MouseDragWMin;	// ドラッグ開始位置のWmin(ドラッグ選択用)
	POINT		MouseDragPoint;	// 前回の枠(ドラッグ選択用)
	POINT		oldmousemvpos;	// 直前の隠しメニューの項目
	int			MousePush;		// D&D動作の種類 MOUSE_xxx
	int			MouseUpFG;		// Button Up 時に SetFG が必要なら 1
	int			PushArea;		// 押下位置の種類の保存
	int			KeyChar;		// WM_KEYUP 中は 1
	ENTRYINDEX	cellNbeforePush;	// クリック前のcellNの値

#define DDPAGEMODE_NONE 0
#define DDPAGEMODE_LIST 1
#define DDPAGEMODE_TREE 2
#define DDPAGEMODE_DIR_ENTER 3 // ホバーによる移動
#define DDPAGEMODE_DIR_COUNTDOWN (DDPAGEMODE_DIR_ENTER + 5) // カウントダウン開始
							// ホバーによる移動検知の開始
#define DDPAGEMODE_DIR_DEFWAIT 5000
#define DDPAGEMODE_DIR_FIRSTWAIT (DDPAGEMODE_DIR_ENTER + ((X_ddex.DDenterTime - 1) / TIMER_DRAGSCROLL))
#define DDPAGEMODE_DIR_NEXTWAIT (DDPAGEMODE_DIR_FIRSTWAIT + (3000 / TIMER_DRAGSCROLL))

	int			DDpagemode, DDpage;
	UINT_PTR	ddtimer_id;

	HIDDENMENUS	HiddenMenu;
	int			swin;			//	b5:B4=1のとき、=0:大きさ =1:大きさ+位置
	XC_MASK mask;			// 表示マスク
	XC_CFMT stat;			// ステータス表示行
	XC_CFMT inf1;			// 情報表示１行目
	XC_CFMT inf2;			// 情報表示２行目
	XC_CFMT celF;			// セル表示書式
	BOOL FixcelF;		// XC_dsetで celF が変更中の時は TRUE

	XC_DSET dset;			// ドライブ毎の設定
	int iconR;			// アイコンの右端のX座標(0:表示せず)
	SIZE XC_ifix_size;	// 情報行アイコンの大きさ
	int X_lspc, X_lspcOrg;

	WHERESETTINGS WhereIsSettings;	// Where is の設定
	volatile int FDirWrite;		// ディレクトリ更新要求管理
	HGLOBAL		CLIPDATAS[CLIPTYPES];	// クリップボードにコピーする内容
	DWORD		X_win;			// ウィンドウ関連のカスタマイズ
	HWND hTrayWnd;		// PPTray Window
	HWND hComboWnd;		// Combo Window
	HWND hToolBarWnd;
	HWND hScrollBarWnd;	// スクロールバーウィンドウ
	HWND hScrollTargetWnd;	// スクロール表示を行うウィンドウ(info.hWndかhScrollBarWnd)
	int ScrollBarSize;
	int ScrollBarY;
	int ScrollBarHV; // バーの向き 水平 SB_HORZ(0) or 垂直 SB_VERT(1)
							// ※ SBS_HORZ / SBS_VERT としても使用可能

	int CellHashType;	// ENTRYCELL.hash の形式
	int DownMPos;		// 押したHidden Menu
	int Mpos;			// 選択中の Hidden Menu
	XC_SORT		XC_sort;		// 保持ソート用
	XC_SORT_MODE sort_nowmode;
	DWORD		sort_columnindex;

	DWORD		UseCommentsFlag; // どのコメント領域を使用しているか(EVENT発生用 B0:通常コメント B1～B31:拡張コメント)

	int			X_inag;
										// Tree 関連
	HWND		hTreeWnd;
	XC_TREE		XC_tree;		// Tree 処理用
	int			TreeX;			// Tree窓の幅

	ICONCACHESTRUCT EntryIcons;	// 各cellに表示するアイコン／画像
	int EntryIconGetSize; // hEntryIcons の為に取得するアイコンの大きさ
								// ※ INVALID_HANDLE_VALUE ならstaticを使用
	TCHAR OldIconsPath[VFPS];
	HICON hInfoIcon;
	DIRECTXDEFINE(BOOL InfoIcon_DirtyCache)
	DIRECTXDEFINE(DXBMPCACHE *InfoIcon_Cache)

	BOOL UseLoadEvent;
	BOOL UseSelectEvent;
	BOOL UseActiveEvent;

	BOOL RequestVolumeLabel;
	TCHAR VolumeLabel[MAX_PATH];

	int AcceptReload;	// キャッシュ後再読込の動作(read_entryの値)
	int AloadCount;		// 非同期読み込み量保存

	PPXDOCKS docks;
	DYNAMICMENUSTRUCT DynamicMenu;

	volatile DWORD TerminateThreadID;

	struct {
		volatile DWORD state;
		volatile DWORD tick;
//		volatile HWND hStateWnd;
	} StateInfo;

	DelayLoadMenuStruct *DelayMenus;
	pCPPcAccServer PPcAccServer;

	DIRECTXDEFINE(DXDRAWSTRUCT *DxDraw)

	#define FREEZE_NONE 0
	#define FREEZE_read_entry 1
	#define FREEZE_CRMENU 2
	#define FREEZE_CRMENU_CELLLOOK 3
	#define FREEZE_CRMENU_SCMENU 4
	#define FREEZE_CRMENU_GetExtCommand 5
	#define FREEZE_TYPELAST 6

	DWORD FreezeType;

#if FREEPOSMODE
	FREEPOSPOINT FreePosList[FREEPOSMODE];
	DWORD FreePosEntries;
#endif
#if SHOWFRAMERATE
	DWORD FrameCount, FrameTime, FrameRate;
#endif
} PPC_APPINFO;

typedef struct {
	DWORD realtime, dirmask, wordmatch;
} WildCardOptions;

typedef struct {
	PPC_APPINFO *cinfo;
	const TCHAR *title;
	XC_MASK *mask;
	const TCHAR *filename;	// 開始時にカーソルがあったエントリのファイル名
	const TCHAR *settingkey;
	const TCHAR *path;
	HWND hEdWnd;
	BOOL maskmode;
	int mode;	// DSMD_
	WildCardOptions option;
} FILEMASKDIALOGSTRUCT;

typedef struct {
	int mode;	// 比較方法
	int DirType; // VFSDT_xx
	DWORD LoadCounter; // 反対窓のパスが変更されていないかを確認するための値
	TCHAR filename[MAX_PATH];	// 比較ディレクトリの保存ファイル名
} COMPAREMARKPACKET;

typedef struct {
	HICON h16;
	HICON h32;
	HICON h48;
	HICON h256;
} ANYSIZEICON;

typedef struct {
	DWORD mode, width;
	TCHAR name[VFPS];
} PPCTREESETTINGS;

typedef struct {
	const TCHAR *comment;
	ENTRYDATAOFFSET dataindex;
	DWORD CommentID;
	DWORD LoadCounter;
} SENDSETCOMMENT;

// AutoD&D や SHN copy で使用するコピー方法
#define DROPTYPE_LEFT 0
#define DROPTYPE_RIGHT B0
#define DROPTYPE_HOOK B1
#define DROPTYPE_POST B2
#define DROPTYPE_COM B3

#ifdef _SHLOBJ_H_
#ifndef SHCNRF_ShellLevel
#if defined(__BORLANDC__) || defined(__GNUC__)
typedef struct _SHChangeNotifyEntry
{
	LPCITEMIDLIST pidl;
	BOOL fRecursive;
} SHChangeNotifyEntry;
#endif
#define SHCNRF_ShellLevel 0x0002
#define SHCNRF_NewDelivery 0x8000
#endif

#ifdef _WIN64
#define DSHChangeNotifyRegister SHChangeNotifyRegister
#define DSHChangeNotifyDeregister SHChangeNotifyDeregister
#define DSHChangeNotification_Lock SHChangeNotification_Lock
#define DSHChangeNotification_Unlock SHChangeNotification_Unlock
#else
ExternWinAPI(ULONG, SHChangeNotifyRegister, (HWND hwnd, int fSources, LONG fEvents, UINT wMsg, int cEntries, const SHChangeNotifyEntry *pshcne));
ExternWinAPI(BOOL, SHChangeNotifyDeregister, (unsigned long ulID));
ExternWinAPI(HANDLE, SHChangeNotification_Lock, (HANDLE hChange, DWORD dwProcId, LPITEMIDLIST **pppidl, LONG *plEvent));
ExternWinAPI(BOOL, SHChangeNotification_Unlock, (HANDLE hLock));
#endif
#endif

typedef struct tagWriteTextStruct {
	HANDLE hFile;
	BOOL (WINAPI *Write)(struct tagWriteTextStruct *sts, const TCHAR *text, size_t textlen);
	int wlfc_flags;
} WriteTextStruct;
extern BOOL WINAPI WriteTextNative(WriteTextStruct *sts, const TCHAR *text, size_t textlen);
extern BOOL WINAPI WriteTextUTF8(WriteTextStruct *sts, const TCHAR *text, size_t textlen);

#ifndef SORT_DIGITSASNUMBERS
#define SORT_DIGITSASNUMBERS 0x08 // Win7
#endif

#define SORTE_IGNORECASE	NORM_IGNORECASE		// B0 0x01 Sort:on a→Aに変えてソート
#define SORTE_NONSPACE		NORM_IGNORENONSPACE	// B1 0x02 Sort:on CASE同時不可
#define SORTE_SYMBOLS		NORM_IGNORESYMBOLS	// B2 0x04 Sort:on CASE同時不可  !(@ とかを削除してソート)
#define SORTE_NUMBERS		SORT_DIGITSASNUMBERS // B3 0x08 Sort:of 指定可能(Win7)
// #define LINGUISTIC_IGNOREDIACRITIC		// B5 0x20
// #define LCMAP_LOWERCASE					// B8 0x100
// #define LCMAP_UPPERCASE  				// B9 0x200
// #define LCMAP_TITLECASE					//    0x300 // Win7
// #define LCMAP_SORTKEY					// B10 0x400
// #define LCMAP_BYTEREV					// B11 0x800
// #define SORT_STRINGSORT					// B12 0x1000 // Sort:of CASE同時不可
#define SORTE_KANATYPE	NORM_IGNOREKANATYPE	// B16 0x1 0000 Sort:on CASE同時不可
#define SORTE_WIDE		NORM_IGNOREWIDTH	// B17 0x2 0000 Sort:on CASE同時不可
// #define LCMAP_HASH						// B18 0x4 0000 // Win8
// #define LCMAP_HIRAGANA					// B20 0x10 0000
// #define LCMAP_KATAKANA					// B21 0x20 0000
// #define LCMAP_HALFWIDTH					// B22 0x40 0000
// #define LCMAP_FULLWIDTH					// B23 0x80 0000
// #define LCMAP_LINGUISTIC_CASING			// B24 0x100 0000
// #define LCMAP_SIMPLIFIED_CHINESE			// B25 0x200 0000
// #define LCMAP_TRADITIONAL_CHINESE		// B26 0x400 0000
// #define NORM_LINGUISTIC_CASING			// B27 0x800 0000
// #define LCMAP_SORTHANDLE					// B29 0x2000 0000 // Win8
#define SORTE_DEFAULT_VALUE SORTE_IGNORECASE
#define SORTE_DEFAULT_MASK (SORTE_IGNORECASE | SORTE_NONSPACE | SORTE_SYMBOLS | SORTE_NUMBERS | SORTE_KANATYPE | SORTE_WIDE)
#define SORTE_NUMBER	B24		// 独自数値考慮 compare
#define SORTE_LASTENTRY	B25		// ディレクトリ無視
#define SORTE_EXMASK (SORTE_NUMBER | SORTE_LASTENTRY)


#define WLFC_DETAIL 	0	// PPx独自形式
#define WLFC_NAMEONLY	B0	// ファイル名のみにする
#define WLFC_OPTIONSTR	B1	// 末尾に csv用の文字列を追加する
#define WLFC_WITHMARK	B2	// マークがあるときに、M:1,を記載
#define WLFC_COMMENT	B3	// コメント出力を有効にする
#define WLFC_SYSMSG		B4	// システムメッセージを出力する
#define WLFC_MARKEDONLY	B5	// マーク付きのみ出力
#define WLFC_NOHEADER	B6	// ヘッダ無し
#define WLFC_UTF8		B7	// UTF-8
#define WLFC_BOM		B8	// unicode BOM

#define WLFC_COMMAND_DEFAULT	(WLFC_DETAIL | WLFC_OPTIONSTR | WLFC_COMMENT)
#define WLFC_WRITEDIR_DEFAULT	(WLFC_DETAIL | WLFC_OPTIONSTR | WLFC_COMMENT)
#define FormatResetWLFC(flags) (((flags) & ~(WLFC_NAMEONLY | WLFC_OPTIONSTR | WLFC_COMMENT)) )

/*
#define WLF_NORMAL 0 // 全エントリ、マーク情報無し
#define WLF_MARKEDENTRY 1 // マークしたエントリのみ、マーク情報無し
#define WLF_WITHMARK 2 // 全エントリ、マーク情報有り
#define WLF_NAME 3 // 名前のみ
*/


typedef struct {
	PPC_APPINFO *cinfo;
	XC_SORT *xc;
} PPCSORTDIALOGPARAM;

typedef struct {
	DWORD DDenterTime;
} DDEXSETTINGS;

extern LOADWINAPISTRUCT OLEAUT32APIS[];
extern HANDLE hOleaut32;
extern HRESULT (STDAPICALLTYPE *DRegisterActiveObject)(IUnknown * punk, REFCLSID rclsid, DWORD dwFlags, DWORD *pdwRegister);
extern HRESULT (STDAPICALLTYPE *DRevokeActiveObject)(DWORD dwRegister, void *pvReserved);
extern HRESULT (STDAPICALLTYPE *DGetActiveObject)(REFCLSID rclsid, void * pvReserved, IUnknown ** ppunk);

ExternWinAPI(HRESULT, DwmIsCompositionEnabled, (BOOL *pfEnabled));
ExternWinAPI(void, NotifyWinEvent, (DWORD event, HWND hwnd, LONG idObject, LONG idChild));

ExternWinAPI(BOOL, SetGestureConfig, (HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize));
ExternWinAPI(BOOL, GetGestureInfo, (HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo));

extern const IID XIID_IShellItem;

extern PPXINMENUBAR ppcbar[];

extern char sortIDtable[];
extern const TCHAR *SortItems[];

#ifndef SHGFI_ADDOVERLAYS
	#define SHGFI_ADDOVERLAYS  0x000000020
	#define SHGFI_OVERLAYINDEX 0x000000040
#endif

#ifndef HDF_SORTDOWN
#define HDF_SORTDOWN B9	 // 逆順ソート▼
#define HDF_SORTUP   B10 // 正順ソート▲
#endif
#ifndef HDF_CHECKBOX
#define HDF_CHECKBOX B6
#define HDF_CHECKED  B7
#define HDS_CHECKBOXES 0x0400
#endif
#ifndef HDN_ITEMSTATEICONCLICK
#define HDN_ITEMSTATEICONCLICK  (HDN_FIRST-16)
#endif

extern IID XIID_IFileOperation;
extern CLSID XCLSID_IFileOperation;

#define GLOBALEXTERN
#include "PPC_GVAR.C"
#undef GLOBALEXTERN

#ifdef __cplusplus
}
#endif
