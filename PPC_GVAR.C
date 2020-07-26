/*-----------------------------------------------------------------------------
	Paper Plane cUI												Global Value
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#ifdef GLOBALEXTERN
	#define GVAR extern
	#define GPARAM(x)
	#define GPARAM2(x, y)
	#define DLLGVAR extern
	#define DLLGPARAM(x)
	#define DLLGPARAM2(x, y)
#else
	#undef GVAR
	#undef GPARAM
	#undef GPARAM2
	#define GVAR
	#define GPARAM(x) = x
	#define GPARAM2(x, y) = {x, y}
	#if NODLL
		#define DLLGVAR extern
		#define DLLGPARAM(x)
		#define DLLGPARAM2(x, y)
	#else
		#undef DLLGVAR
		#undef DLLGPARAM
		#undef DLLGPARAM2
		#define DLLGVAR
		#define DLLGPARAM(x) = x
		#define DLLGPARAM2(x, y) = {x, y}
	#endif
#endif

#define PPCMSGDEBUG 0

#if PPCMSGDEBUG
#define DEBUGLOGC(message, param) if (X_log==MAX32) XMessage(NULL, NULL, XM_DbgLOG, T("%s ") T(message), cinfo->RegCID, param);
#else
#define DEBUGLOGC(message, param)
#endif

#define TIMERID_SPECIAL			105 // おまけ用
#define TIMERID_DRAGSCROLL		106 // ドラッグによる窓スクロール処理用
#define TIMERID_COMBOHIDEMENU	107 // 隠しメニューを閉じるために使用
#define TIMERID_COMBODRAGSCROLL	108 // 一体化用
#define TIMERID_ENTRYTIP		109 // ファイル名チップ表示
#define TIMERID_DELAYCURSOR		110 // 遅延移動カーソル
#define TIMERID_DELAYLOGSHOW	111 // 遅延ログ更新
#define TIMERID_INFODOCK_MMOVE	112 // dock上情報行の隠しメニュー表示制御用
#define TIMERID_HOVERTIP		113 // チップ表示のホバー検出
// TIMERID_USER : 0xc000-0xdfff

#define TIMER_DRAGSCROLL	400		// TIMERID_DRAGSCROLLの間隔(ms)
#define TIMER_COMBOHIDEMENU	200		// TIMERID_COMBOHIDEMENUの間隔(ms)
#define TIMER_DELAYLOGSHOW	1000	// TIMERID_DELAYLOGSHOWによる遅延時間(ms)

#define dispNwait 1000			// 状況表示を行う間隔(ms)

// ※ 0x1000～0x3fff の範囲で使用する 動的 window id
#define IDW_REPORTLOG		(IDW_CONTROLMIN + 0) // ログ窓
#define IDW_REBAR			(IDW_CONTROLMIN + 1) // Dock
#define IDW_COMMONTREE		(IDW_CONTROLMIN + 2) // 一体時用共用ツリー
#define IDW_ADDRESS			(IDW_CONTROLMIN + 3) // アドレスバー
#define IDW_ADDTREE			(IDW_CONTROLMIN + 4) // AutoD&D用コピー先ツリー
#define IDW_PPCTREE			(IDW_CONTROLMIN + 5) // 窓用ツリー
#define IDW_SCROLLBAR		(IDW_CONTROLMIN + 6) // スクロールバー
#define IDW_HEADER			(IDW_CONTROLMIN + 8) // ヘッダ
#define IDW_DRIVEBAR		(IDW_CONTROLMIN + 9) // ドライブバー
#define IDW_DOCKMODULE		(IDW_CONTROLMIN + 11) // DockModule通信用
#define IDW_DOCKPPCINFO		(IDW_CONTROLMIN + 12) // Dock内情報窓
#define IDW_DOCKPPCSTATUS	(IDW_CONTROLMIN + 13) // Dock内ステータス窓
#define IDW_DOCKINPUT		(IDW_CONTROLMIN + 14) // Dock内汎用入力
#define IDW_TIPTREE			(IDW_CONTROLMIN + 15) // チップで表示するツリー
#define IDW_ADRBCL			(IDW_CONTROLMIN + 0x0e00) // パン屑アドレスバー
#define IDW_DRIVES			(IDW_CONTROLMIN + 0x0f60) // ドライブバー内ドライブ
// TipText(TTN_NEEDTEXT) / ToolBar / TAB 識別用ID
#define IDW_TABCONTROL		(IDW_CONTROLMIN + 0x0f80) // Tab Window 識別用
#define IDW_TOOLCOMMANDS	(IDW_CONTROLMIN + 0x1000) // ToolBar 等のコマンド

// メニュー用の特殊な項目
#define DSMD_NOMODE		0 // 指定無し		sort/view
#define DSMD_TEMP		1 // 一時設定		sort/(view)
#define DSMD_THIS_PATH	2 // このパス限定	sort/view
#define DSMD_THIS_BRANCH 3 // このパス以降	sort/view
#define DSMD_REGID		4 // 窓別設定		sort/view
#define DSMD_LISTFILE	5 // listfile内		sort/view
#define DSMD_PATH_BRANCH 6 // 指定パス以降	sort/view
#define DSMD_ARCHIVE	7 // 書庫内			sort/view

#define DSMD_MODELAST	8 // 区切り線 // ここまではモード切替選択肢

#define DSMD_CANCEL		9 // 中止			sort/view
#define DSMD_EDIT		10 // 編集			sort/view
#define DSMD_SAVE		11 // 保存			view
#define DSMD_PATHMASK	12 // 階層表示		view
#define DSMD_SPLITPATH	13 // パス部分の表示 view
#define DSMD_MAG_THUMBS	14 // サムネイル拡大 view
#define DSMD_MINI_THUMBS 15 // サムネイル縮小 view
#define DSMD_ASCENDING	14 // 昇順			sort
#define DSMD_DESCENDING	15 // 降順			sort
#define DSMD_MAX		15

// ※これらの値の大きさの順序に依存してコーティングをしている
#define CRID_EXTMENU	IDW_INTERNALMIN	// 拡張子別 M_Ccr メニュー

#define CRID_COMMENT			(IDW_INTERNALMIN + 0x1600)	// \[O]
#define CRID_COMPARE			(IDW_INTERNALMIN + 0x1800)	//  [O]
#define CRID_DRIVELIST			(IDW_INTERNALMIN + 0x1a00)	// \[L]
#define CRID_DRIVELIST_MAX		(CRID_DRIVELIST + 0x180)

#define CRID_DIROPT				(IDW_INTERNALMIN + 0x1be0)	//  *diroption
#define CRID_DIROPT_TEMP		(CRID_DIROPT + DSMD_TEMP)
#define CRID_DIROPT_THIS_PATH	(CRID_DIROPT + DSMD_THIS_PATH)
#define CRID_DIROPT_THIS_BRANCH	(CRID_DIROPT + DSMD_THIS_BRANCH)
#define CRID_DIROPT_LISTFILE	(CRID_DIROPT + DSMD_LISTFILE)
#define CRID_DIROPT_ARCHIVE		(CRID_DIROPT + DSMD_ARCHIVE)
#define CRID_DIROPT_PATH_BRANCH	(CRID_DIROPT + DSMD_PATH_BRANCH)
#define CRID_DIROPT_MODELAST	(CRID_DIROPT + DSMD_MAX)
#define CRID_DIROPT_MAX			(CRID_DIROPT + 0x1f)

#define CRID_SORT				(IDW_INTERNALMIN + 0x1c00)	//  [S]
#define CRID_SORTEX				(CRID_SORT + 0x1f0)
#define CRID_SORT_NOMODE		(CRID_SORTEX + DSMD_NOMODE)
#define CRID_SORT_TEMP			(CRID_SORTEX + DSMD_TEMP)
#define CRID_SORT_THIS_PATH		(CRID_SORTEX + DSMD_THIS_PATH)
#define CRID_SORT_THIS_BRANCH	(CRID_SORTEX + DSMD_THIS_BRANCH)
#define CRID_SORT_REGID			(CRID_SORTEX + DSMD_REGID)
#define CRID_SORT_PATH_BRANCH	(CRID_SORTEX + DSMD_PATH_BRANCH)
#define CRID_SORT_ARCHIVE		(CRID_SORTEX + DSMD_ARCHIVE)
#define CRID_SORT_LISTFILE		(CRID_SORTEX + DSMD_LISTFILE)
#define CRID_SORT_MODELAST		(CRID_SORTEX + DSMD_MODELAST)
#define CRID_SORT_CANCEL		(CRID_SORTEX + DSMD_CANCEL)
#define CRID_SORT_EDIT			(CRID_SORTEX + DSMD_EDIT)
#define CRID_SORT_ASCENDING		(CRID_SORTEX + DSMD_ASCENDING)
#define CRID_SORT_DESCENDING	(CRID_SORTEX + DSMD_DESCENDING)
#define CRID_SORT_MAX			(CRID_SORTEX + DSMD_MAX)

#define CRID_VIEWFORMAT			(IDW_INTERNALMIN + 0x1e00)	//  [;]
#define CRID_VIEWFORMATEX			(CRID_VIEWFORMAT + 0x1f0)
#define CRID_VIEWFORMAT_NOMODE		(CRID_VIEWFORMATEX + DSMD_NOMODE)
#define CRID_VIEWFORMAT_TEMP		(CRID_VIEWFORMATEX + DSMD_TEMP)
#define CRID_VIEWFORMAT_THIS_PATH	(CRID_VIEWFORMATEX + DSMD_THIS_PATH)
#define CRID_VIEWFORMAT_THIS_BRANCH	(CRID_VIEWFORMATEX + DSMD_THIS_BRANCH)
#define CRID_VIEWFORMAT_REGID		(CRID_VIEWFORMATEX + DSMD_REGID)
#define CRID_VIEWFORMAT_PATH_BRANCH	(CRID_VIEWFORMATEX + DSMD_PATH_BRANCH)
#define CRID_VIEWFORMAT_ARCHIVE		(CRID_VIEWFORMATEX + DSMD_ARCHIVE)
#define CRID_VIEWFORMAT_LISTFILE	(CRID_VIEWFORMATEX + DSMD_LISTFILE)
#define CRID_VIEWFORMAT_MODELAST	(CRID_VIEWFORMATEX + DSMD_MODELAST)
#define CRID_VIEWFORMAT_CANCEL		(CRID_VIEWFORMATEX + DSMD_CANCEL)
#define CRID_VIEWFORMAT_EDIT		(CRID_VIEWFORMATEX + DSMD_EDIT)
#define CRID_VIEWFORMAT_SAVE		(CRID_VIEWFORMATEX + DSMD_SAVE)
#define CRID_VIEWFORMAT_PATHMASK	(CRID_VIEWFORMATEX + DSMD_PATHMASK)
#define CRID_VIEWFORMAT_SPLITPATH	(CRID_VIEWFORMATEX + DSMD_SPLITPATH)
#define CRID_VIEWFORMAT_MAG_THUMBS	(CRID_VIEWFORMATEX + DSMD_MAG_THUMBS)
#define CRID_VIEWFORMAT_MINI_THUMBS	(CRID_VIEWFORMATEX + DSMD_MINI_THUMBS)
#define CRID_VIEWFORMAT_MAX			(CRID_VIEWFORMATEX + DSMD_MAX)

#define CRID_CCREXEC_MAX (IDW_INTERNALMIN + 0x1fff) // PP_ExtractMacro で実行可能な項目の最後

#define CRID_OPENWITH	(IDW_INTERNALMIN + 0x2000)	// OpenWith(プログラムを選択して実行)
#define CRID_OPENWITH_MAX	(IDW_INTERNALMIN + 0x21ff)

#define CRID_DIRTYPE	(IDW_INTERNALMIN + 0x2200)	// ディレクトリ読み込み方法
//      CRID_DIRTYPE～CRID_DIRTYPE_STREAM 各種書庫
#define CRID_DIRTYPE_STREAM		(CRID_DIRTYPE + 0x1f7) // 必ず CRID_DIRTYPEの次
#define CRID_DIRTYPE_SHORTCUT		(CRID_DIRTYPE + 0x1f8)
#define CRID_DIRTYPE_JUNCTION		(CRID_DIRTYPE + 0x1f9)
#define CRID_DIRTYPE_CLSID			(CRID_DIRTYPE + 0x1fa)
#define CRID_DIRTYPE_PAIR			(CRID_DIRTYPE + 0x1fb) // ここから先は反対窓
#define CRID_DIRTYPE_STREAM_PAIR	(CRID_DIRTYPE + 0x1fc)
#define CRID_DIRTYPE_SHORTCUT_PAIR	(CRID_DIRTYPE + 0x1fd)
#define CRID_DIRTYPE_JUNCTION_PAIR	(CRID_DIRTYPE + 0x1fe)
#define CRID_DIRTYPE_CLSID_PAIR		(CRID_DIRTYPE + 0x1ff)
#define CRID_DIRTYPE_MAX			(CRID_DIRTYPE + 0x1ff)
#define CRID_NEWENTRY	(IDW_INTERNALMIN + 0x2800)	// 新規 (NEWCMD_DIR も影響)
#define CRID_NEWENTRY_MAX	(CRID_NEWENTRY + 0x1ff)
#define CRID_EXTCMD			(IDW_INTERNALMIN + 0x2c00)	// 標準拡張子別の選択肢
#define CRID_EXTCMD_MAX		(CRID_EXTCMD + 0x1ff)

#define CRID_LISTSTART	(IDW_INTERNALMAX - 3) // スタートの全てに追加
#define CRID_NEWTAB		(IDW_INTERNALMAX - 2) // 新規タブ
#define CRID_SELECTEXE	(IDW_INTERNALMAX - 1) // Open With で選択
#define CRID_MAX		IDW_INTERNALMAX // 2fff

#define CEL(/*ENTRYINDEX*/ indexNo)  ((ENTRYCELL *)cinfo->e.CELLDATA.p)[((ENTRYINDEX *)cinfo->e.INDEXDATA.p)[indexNo]]
#define CELdata(/*ENTRYDATAOFFSET*/ dataNo) ((ENTRYCELL *)cinfo->e.CELLDATA.p)[dataNo]
#define CELt(/*ENTRYINDEX*/ indexNo) ((ENTRYINDEX *)cinfo->e.INDEXDATA.p)[indexNo]

#define SORTTYPES	(24 + 20)		// ソート方法の数
#define SORTFULLTYPES	(SORTTYPES + 2)	// 内部用を含めたSORTTYPES
#define SORTFUNCTYPES	(24 + 2)	// ソート関数の数

#define SORT_COLUMNTYPE	(SORTFUNCTYPES - 2)	// ColumnID を必要とするソート方法の先頭
#define SORTFUNC_COLUMN_UP		SORT_COLUMNTYPE
#define SORTFUNC_COLUMN_DOWN	(SORT_COLUMNTYPE + 1)
#define SORT_COLUMN_UP			SORTTYPES
#define SORT_COLUMN_DOWN		(SORTTYPES + 1)

#ifdef GLOBALEXTERN
typedef enum {
	CHOOSEMODE_NONE = 0, CHOOSEMODE_EDIT, CHOOSEMODE_DD, CHOOSEMODE_CON,
	CHOOSEMODE_MULTICON, CHOOSEMODE_CON_UTF8, CHOOSEMODE_MULTICON_UTF8,
	CHOOSEMODE_CON_UTF16, CHOOSEMODE_MULTICON_UTF16
} CHOOSEMODE;
#endif


DLLGVAR const TCHAR NilStr[1] DLLGPARAM(T(""));
DLLGVAR TCHAR NilStrNC[1] DLLGPARAM(T(""));

GVAR const TCHAR PPcRootMainThreadName[] GPARAM(T("PPc main,root"));
GVAR const TCHAR PPcMainThreadName[] GPARAM(T("PPc main,2nd"));
GVAR TCHAR PPcPath[VFPS];	// PPc があるディレクトリ

GVAR HINSTANCE hInst;			// Main hInstance
GVAR UINT WM_PPXCOMMAND;		// PPx 間通信用 Window Message
GVAR UINT WM_TaskbarButtonCreated; // タスクバーボタン関連の初期化(Win7)
GVAR OSVERSIONINFO OSver;		// OS 情報
GVAR DWORD MainThreadID; 		// このプロセスのメインスレッドID

GVAR CHOOSEMODE X_ChooseMode;// GPARAM(CHOOSEMODE_NONE);
GVAR HWND hChooseWnd;

GVAR HWND SelfDD_hWnd GPARAM(NULL);	// 自分がドラッグオーナーならその hWnd が入る
GVAR int SelfDD_Dtype GPARAM(0);

GVAR BOOL UsePFont GPARAM(FALSE);	// プロポーショナルフォントかどうか

	// ファイル名やカラム等の表示に使う形式
GVAR int EllipsisType GPARAM(0);
	// ファイル名の表示に DrawText を使用するか
	// ※ プロポーショナルフォントか、省略表示の変更を行っているときに有効
GVAR UINT UseDrawText GPARAM(FALSE);

GVAR HWND hCommonLog GPARAM(NULL);			// 共用ログ
GVAR TCHAR *CommonLogBackup GPARAM(NULL);	// 共用ログの内容バックアップ(cust)
GVAR HWND hPropWnd GPARAM(NULL);	// 連動プロパティのhWnd

GVAR HWND hPreviewWnd GPARAM(NULL);	// プレビュー用hWnd

GVAR int RequestDestroyFlag GPARAM(0); // 閉じた後の後処理が必要なことをメインメッセージループに伝えるフラグ
GVAR int X_MultiThread GPARAM(1); // マルチスレッドなら１シングルスレッドなら０

GVAR int firstinit GPARAM(1); // PPc の初期化中の判断

GVAR int RegSubIDcounter GPARAM(-1); // SubID の割当て決定に使用するカウンタ

GVAR CRITICAL_SECTION FindFirstAsyncSection; // 非同期読み込みテーブル用
GVAR CRITICAL_SECTION SHGetFileInfoSection; // SHGetFileInfo のシングル化用

GVAR void *FaultInfoPtr GPARAM(NULL);

DLLGVAR DWORD ButtonMenuTick DLLGPARAM(0); // ボタンメニューの誤表示抑制用

GVAR HANDLE hProcessHeap; // GetProcessHeap() の値

GVAR MAINWINDOWSTRUCT MainWindows
#ifndef GLOBALEXTERN
 = {NULL, NULL, 0, FALSE, FALSE};
#endif
;

GVAR const TCHAR *StateText[]
#ifndef GLOBALEXTERN
 = { NilStr, T("readentry Init"), T("ReadStart"), T("ReadError"), T("UpdateStart"), T("Crmenu"), T("Celllook"), T("scmenu"), T("GetExtCommand"), T("?")};
#else
;
typedef enum {
	StateID_NoState = 0, StateID_Init, StateID_ReadStart, StateID_ReadError, StateID_UpdateStart, StateID_Crmenu, StateID_Celllook, StateID_Scmenu, StateID_GetExtCommand, StateID_MAX
} StateID;
#endif

GVAR DWORD DefDriveList GPARAM(0); // 起動時のドライブ一覧
GVAR const TCHAR StrMesCMPH[] GPARAM(MES_CMPH);

//--------------------------------------------------------- Win32 API
#ifdef UNICODE
#define DGetDiskFreeSpaceEx GetDiskFreeSpaceExW
#else
GVAR ValueWinAPI(GetDiskFreeSpaceEx) GPARAM(NULL);
#endif

GVAR ValueWinAPI(DwmIsCompositionEnabled) GPARAM(NULL);
GVAR ValueWinAPI(NotifyWinEvent) GPARAM(DummyNotifyWinEvent);
GVAR BOOL UseOffScreen GPARAM(FALSE);

#ifdef _INC_COMMCTRL
GVAR ValueWinAPI(ImageList_Create);
GVAR ValueWinAPI(ImageList_Destroy);
GVAR ValueWinAPI(ImageList_Add);
GVAR ValueWinAPI(ImageList_Replace);
GVAR ValueWinAPI(ImageList_ReplaceIcon);
GVAR ValueWinAPI(ImageList_SetBkColor);
												// DLL load 検出と兼用
GVAR ValueWinAPI(ImageList_Draw) GPARAM(NULL);
//GVAR ValueWinAPI(ImageList_DrawEx) GPARAM(NULL);

GVAR ValueWinAPI(SetGestureConfig);
GVAR ValueWinAPI(GetGestureInfo);

GVAR LOADWINAPISTRUCT ImageCtlDLL[]
#ifndef GLOBALEXTERN
 = {
	LOADWINAPI1(ImageList_Create),
	LOADWINAPI1(ImageList_Destroy),
	LOADWINAPI1(ImageList_Add),
	LOADWINAPI1(ImageList_Replace),
	LOADWINAPI1(ImageList_ReplaceIcon),
	LOADWINAPI1(ImageList_SetBkColor),
	LOADWINAPI1(ImageList_Draw),
//	LOADWINAPI1(ImageList_DrawEx),
	{NULL, NULL}
};
#endif
;
#endif

GVAR const TCHAR StrUser32DLL[] GPARAM(T("USER32.DLL"));
GVAR const TCHAR StrKernel32DLL[] GPARAM(T("KERNEL32.DLL"));
GVAR const TCHAR StrShell32DLL[] GPARAM(T("SHELL32.DLL"));
//================================================================ DATA
GVAR const TCHAR *DirString;
GVAR int DirStringLength;
GVAR const TCHAR StrThisDir[] GPARAM(T("."));
GVAR const TCHAR Str_WinPos[] GPARAM(T("_WinPos"));
GVAR const TCHAR Str_TreeClass[] GPARAM(T(VFSTREECLASS));

GVAR const TCHAR *StrBusy;
GVAR DWORD StrBusyLength;

#ifndef _WIN64
GVAR const TCHAR MemWarnStr[] GPARAM(T("プロセスの空きメモリが500M未満です。動作が不安定になる恐れがあります。"));
#endif

GVAR const char ListFileHeaderStrW[] GPARAM(UCF2HEADER LHEADERU);
GVAR const char ListFileHeaderStrA[] GPARAM(LHEADER);
GVAR const char ListFileHeaderStr8[] GPARAM(UTF8HEADER LHEADER);

#define ListFileHeaderSizeW sizeof(UCF2HEADER LHEADERU)
#define ListFileHeaderSizeA (sizeof(LHEADER) - sizeof(char))
#define ListFileHeaderSize8 (sizeof(UTF8HEADER LHEADER) - sizeof(char))

#ifdef UNICODE
#define ListFileHeaderStr ListFileHeaderStrW
#define ListFileHeaderSize ListFileHeaderSizeW
#else
#define ListFileHeaderStr ListFileHeaderStrA
#define ListFileHeaderSize ListFileHeaderSizeA
#endif
GVAR const TCHAR ListFileFormatStr[] GPARAM(T("\",\"%s\",A:H%x,C:%u.%u,L:%u.%u,W:%u.%u,S:%u.%u"));

GVAR const TCHAR hdrstrings_eng_date[] GPARAM(MES_HDRD);

GVAR const TCHAR *hdrstrings_eng[]
#ifndef GLOBALEXTERN
 = {
	MES_HDRN, hdrstrings_eng_date, MES_HDRS, MES_HDRA, NULL
}
#endif
;

GVAR TCHAR *hdrstrings[4];
GVAR UINT CF_xSHELLIDLIST;

// 表示書式の各書式をスキップするバイト数
GVAR const int DispFormatSkipTable[] GPARAM({DE_SKIPTABLE});

GVAR const TCHAR LOADDISPSTR[6] GPARAM(T("disp:"));
GVAR const TCHAR LOADMASKSTR[6] GPARAM(T("mask:"));
GVAR const TCHAR LOADCMDSTR[5] GPARAM(T("cmd:"));

GVAR const TCHAR FileOperationMode_Copy[] GPARAM(T("Copy"));
GVAR const TCHAR FileOperationMode_Move[] GPARAM(T("Move"));
GVAR const TCHAR FileOperationMode_Rename[] GPARAM(T("Rename"));
GVAR const TCHAR PasteMode_Copy[] GPARAM(T("PasteCopy"));
GVAR const TCHAR PasteMode_Move[] GPARAM(T("PasteMove"));

GVAR const TCHAR EntryImageThumbName[] GPARAM(T(":thumbnail.jpg"));
GVAR const TCHAR StrArchiveMode[] GPARAM(T("archive")); // XC_dest に登録する書庫専用設定
GVAR const TCHAR StrListfileMode[] GPARAM(T("listfile")); // XC_dest に登録するListfile専用設定

GVAR const TCHAR StrBadOption[] GPARAM(MES_EUOP);

GVAR const TCHAR StrNoEntries[] GPARAM(MES_NOEL);
GVAR const TCHAR StrNotSupport[] GPARAM(MES_USTD);

GVAR const TCHAR StrFopCaption_Copy[] GPARAM(MES_JMCP);
GVAR const TCHAR StrFopCaption_Move[] GPARAM(MES_JMMO);

GVAR DWORD ColumnExtUserID GPARAM(DFC_USERMAX); // 外部データ保存のときの割振ID

GVAR const TCHAR StrKC_main[] GPARAM(T("KC_main"));
GVAR const TCHAR StrXC_rmsk[] GPARAM(T("XC_rmsk"));
GVAR const TCHAR StrXC_dset[] GPARAM(T("XC_dset"));
GVAR const TCHAR StrWordMatchWildCard[6] GPARAM(T("o:ew,"));
GVAR const TCHAR StrMaskDlalogWildCard[5] GPARAM(T("o:e,"));
GVAR const TCHAR Str_others[] GPARAM(T("_others"));

GVAR const EXECKEYCOMMANDSTRUCT PPcExecKey
#ifndef GLOBALEXTERN
 = {(EXECKEYCOMMANDFUNCTION)PPcCommand, StrKC_main, NULL}
#endif
;
GVAR const TCHAR CacheErrorTitle[] GPARAM(T("Dir Cache save error"));
DLLGVAR const TCHAR StrShortcutExt[] DLLGPARAM(T(ShortcutExt));

GVAR const TCHAR StrRegFolder[] GPARAM(T("Folder")); // レジストリのフォルダの設定名
GVAR const TCHAR RegDeviceNamesStr[] GPARAM(T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\DOS Devices")); // subst の記録

//================================================================ Customize 系
//------------------------------------------------------------ 表示書式の初期値
#ifndef GLOBALEXTERN
BYTE CFMT_statfmt[] = {0};
BYTE CFMT_inf1fmt[] = {DE_LFN, 255, 255, 0};
BYTE CFMT_inf2fmt[] = {DE_SFN_EXT, 8, 5, DE_SIZE2, 10, DE_SPC, 1, DE_TIME1, 17, DE_ATTR1, 10, 0};
BYTE CFMT_celFfmt[] = {DE_MARK, DE_LFN, 14, 5, DE_SIZE2, 10, DE_SPC, 1, DE_TIME1, 14, DE_BLANK, 1, 0};
#endif

GVAR const XC_CFMT CFMT_stat	//  XC_stat
#ifndef GLOBALEXTERN
= {C_AUTO, C_AUTO, 0, 0, 0, 1, 0, DE_ATTR_STATIC | DE_ATTR_MARK | DE_ATTR_DIR, CFMT_statfmt, NULL}
#endif
;

GVAR const XC_CFMT CFMT_inf1	//  XC_inf1
#ifndef GLOBALEXTERN
= {C_DWHITE, C_BLACK, 0, 0, 0x200, 1, 0, DE_ATTR_ENTRY, CFMT_inf1fmt, NULL}
#endif
;

GVAR const XC_CFMT CFMT_inf2	//  XC_inf2
#ifndef GLOBALEXTERN
= {C_AUTO, C_AUTO, 0, 0, 42, 1, 0, DE_ATTR_ENTRY, CFMT_inf2fmt, NULL}
#endif
;

GVAR const XC_CFMT CFMT_celF	//  XC_celF
#ifndef GLOBALEXTERN
= {C_AUTO, C_AUTO, 1, 5, 46, 1, 0, DE_ATTR_ENTRY, CFMT_celFfmt, NULL}
#endif
;
// グローバル -----------------------------------------------------------------
GVAR int X_sps GPARAM(0);
GVAR int X_combo GPARAM(0);
DLLGVAR DWORD X_dss DLLGPARAM(DSS_NOLOAD);	// 画面自動スケーリング
DLLGVAR DWORD X_log DLLGPARAM(0);
DLLGVAR int X_pmc[4] DLLGPARAM({X_pmc_defvalue});

DLLGVAR int TouchMode DLLGPARAM(0);

#define CMBS_COMMONINFO		B0	// 情報行共用
#define CMBS_COMMONADDR		B1	// アドレスバー共用
#define CMBS_COMMONTREE		B2	// ツリー共用
#define CMBS_COMMONREPORT	B3	// 報告窓共用

#define CMBS_VPANE			B4	// ペインを上下に配列
#define CMBS_TABALWAYS		B5	// タブを常時表示
#define CMBS_VALWINSIZE		B6	// 全体の大きさを完全可変
#define CMBS_THREAD			B7	// メインスレッド共通化


#define CMBS_TOOLBAR		B8	// ツールバー使用
#define CMBS_HEADER			B9	// ヘッダー使用
#define CMBS_NOCAPTION		B10	// 個別のキャプション表示の無効化
#define CMBS_NOTITLE		B11	// タイトルバーなし

#define CMBS_NOMENU			B12	// メニューバーなし
#define CMBS_TABSEPARATE	B13	// タブをペイン毎に
#define CMBS_NOFORCEREPORT	B14	// *fileのログ等をログ指定が無くても報告窓表示
#define CMBS_DELAYLOGSHOW	B15	// 大量ログの間引き表示を無効


#define CMBS_TABFIXEDWIDTH	B16	// タブの幅を固定
#define CMBS_TABMULTILINE	B17	// タブを複数行対応に
#define CMBS_TABCOLOR		B18	// タブを着色
#define CMBS_TABEACHITEM	B19	// タブ内容記憶をペイン毎に

#define CMBS_TABNEXTTABNEW	B20	// 新規タブをアクティブタブの右隣に
#define CMBS_TABDIRSORTNEW	B21	// 新規タブをディレクトリ順に
#define CMBS_TABUPDIRCLOSE	B22	// 閉じたとき、親ディレクトリのタブへ
//#define CMBS_TABPREVCLOSE	B23	// 閉じたとき、前回アクティブタブへ

#define CMBS_TABMAXALONE	B24	// 最大数以上なら新規一体窓
#define CMBS_TABMINCLOSE	B25	// 最小数未満なら閉じる
#define CMBS_REUSETLOCK		B26	// ロック時に、同パスペインの再利用をする
#define CMBS_DEFAULTLOCK	B27	// ロック状態を初期状態にする

#define CMBS_TABBUTTON		B28	// タブをボタン形状に＆複数行時にボタンを固定
#define CMBS_TABFRAME		B29	// フレーム(カラム表示)を使用する
#define CMBS_QPANE			B30	// 田の字表示

#define CMBS1_TABFIXLAYOUT	B0	// タブ領域の幅を固定する
#define CMBS1_TABSHOWMULTI	B1	// 2ペイン以上のときはタブを常時表示
#define CMBS1_NOSLIMSCROLL	B2	// 独立スクロールバーを使用しない
#define CMBS1_NORESTORETAB	B3	// タブの復元を行わない

#define CMBS1_DIALOGNOPANE	B4	// ダイアログを一体化窓の中心に
#define CMBS1_TABBOTTOM		B5	// タブを下に
#define CMBS1_NOTABBUTTON	B6	// タブの閉じるボタンを無くす

GVAR int X_combos[2] GPARAM2(CMBS_COMMONINFO | CMBS_TABSEPARATE | CMBS_TABCOLOR, 0);
GVAR int X_iacc GPARAM(0);
GVAR int X_alt GPARAM(1);
GVAR int X_ardir[2] GPARAM2(5, 5);
GVAR int X_tray GPARAM(0);
GVAR int XC_awid GPARAM(0);
GVAR int XC_emov GPARAM(0);
GVAR int XC_sdir GPARAM(0);
GVAR int XC_Gest GPARAM(0);
GVAR int XC_page GPARAM(0);
GVAR int XC_smar GPARAM(2);
GVAR int XC_fwin GPARAM(1);
GVAR int XC_msel[2] GPARAM2(0, 0);
GVAR int XC_nsbf GPARAM(1);
GVAR int XC_dpmk GPARAM(0);
GVAR int X_evoc GPARAM(0);
GVAR int XC_drag GPARAM(MOUSEBUTTON_M);
GVAR DWORD X_extl GPARAM(4 + 1);	// 内部では +1 される
GVAR BOOL X_acr[2] GPARAM2(TRUE, TRUE);
GVAR int X_fles GPARAM(0);
GVAR int X_vfs GPARAM(1);
GVAR DWORD X_Slow GPARAM(50);
GVAR DWORD X_askp GPARAM(0);

GVAR DWORD XC_isea[3]
#ifndef GLOBALEXTERN
= {0, 1, 0}
#endif
;

GVAR int XC_gmod GPARAM(0);
GVAR DWORD X_svsz GPARAM(DEF_X_svsz);
GVAR BOOL X_wnam GPARAM(1);
GVAR BOOL XC_szcm GPARAM(0);
GVAR int XC_exem GPARAM(0);
GVAR BOOL XC_limc GPARAM(FALSE);

GVAR BOOL XC_pmsk[2] GPARAM2(0, 0);

#define OCIG_WITHICON 0
#define OCIG_EXPLORERMODE 1
#define OCIG_INARCHIVE 2
#define OCIG_SAVEANS 3
#define OCIG_SHOWTEXT 4
#define OCIG_THUMBAPISIZE 5
#define OCIG_SCALEMODE 6
GVAR int XC_ocig[7]
#ifndef GLOBALEXTERN
= {1, 2, 0, 0, 4, 256, 0}
#endif
;
GVAR int X_lddm[3]
#ifndef GLOBALEXTERN
= {-1, 1, 0}
#endif
;
GVAR DDEXSETTINGS X_ddex GPARAM({DDPAGEMODE_DIR_DEFWAIT});

GVAR int XC_ifix GPARAM(0);
GVAR int X_dsst[2] GPARAM2(DSMD_REGID, DSMD_TEMP);

GVAR BOOL XC_fexc GPARAM(FALSE);
GVAR int XC_cdc GPARAM(B3 | B4);

#define TIP_LONG_TIME 0
#define TIP_LONG_MODE 1
#define TIP_HOVER_TIME 2
#define TIP_HOVER_MODE 3
#define TIP_TAIL_WIDTH 4
#define TIP_TAIL_MODE 5
#define TIP_PV_WIDTH 6
#define TIP_PV_HEIGHT 7
GVAR DWORD X_stip[8]
#ifndef GLOBALEXTERN
= { 2000, stip_mode_filename,
	 0, stip_mode_fileinfo,
	 32, stip_mode_preview,
	 256, 256};
#endif
;

//GVAR int X_lspc GPARAM(0);
GVAR int XC_ulh GPARAM(2);

GVAR DWORD XC_ito GPARAM(900);

GVAR int X_poshl GPARAM(1);
GVAR int XC_alac GPARAM(1);

GVAR int X_IME GPARAM(0);

#define ALST_ATTRIBUTES	0
#define ALST_COPYMOVE	1
#define ALST_DELETE		2
#define ALST_RENAME		3
#define ALST_ACTIVATE	4

#define ALSTV_NONE					-1
#define ALSTV_UPD					0
#define ALSTV_UPD_HIDE				1
#define ALSTV_UPD_AND_RELOAD		2
#define ALSTV_UPD_HIDE_AND_RELOAD	3
#define ALSTV_RELOAD				4
GVAR int XC_alst[5]
#ifndef GLOBALEXTERN
= {ALSTV_UPD_HIDE_AND_RELOAD, ALSTV_UPD, ALSTV_UPD_HIDE_AND_RELOAD, ALSTV_UPD_HIDE_AND_RELOAD, ALSTV_UPD};
#endif
;

GVAR int XC_acsr[2] GPARAM2(1, 0);
GVAR int XC_rrt[2] GPARAM2(2, 5);

GVAR DWORD rdirmask;	// 相対dir「.」「..」の表示を制御する(XC_tdir)
GVAR TCHAR X_dicn[MAX_PATH];

#define CXFN_FILE_NOSEP X_nfmt[0]	// エントリファイルサイズ、桁区切り無し
#define CXFN_FILE X_nfmt[1]			// エントリファイルサイズ、桁区切り有り
#define CXFN_SUM_NOSEP X_nfmt[2]	// ドライブ容量・空き、桁区切り無し
#define CXFN_SUM X_nfmt[3]			// ドライブ容量・空き、桁区切り有り
GVAR DWORD X_nfmt[4]
#ifndef GLOBALEXTERN
= {	XFN_RIGHT | XFN_UNITSPACE | XFN_HEXUNIT,
	XFN_SEPARATOR | XFN_RIGHT,
	XFN_RIGHT | XFN_UNITSPACE | XFN_HEXUNIT,
	XFN_SEPARATOR | XFN_RIGHT};
#endif
;

// Combo window ---------------------------------------------------------------
GVAR DWORD ComboThreadID GPARAM(MAX32);
GVAR TCHAR *ComboPaneLayout GPARAM(NULL);
GVAR DYNAMICMENUSTRUCT ComboDMenu;

// 色 -------------------------------------------------------------------------
GVAR COLORREF C_back	GPARAM(C_BLACK);	// 背景色
GVAR COLORREF C_mes		GPARAM(C_YELLOW);
GVAR COLORREF C_info	GPARAM(C_DWHITE);
GVAR COLORREF C_res[2]	GPARAM2(C_AUTO, C_AUTO);

#define LINE_NORMAL C_line[0]
#define LINE_GRAY C_line[1]
GVAR COLORREF C_line[2]	GPARAM2(C_CYAN, C_RED);	// 区切り線

GVAR COLORREF C_entry[14]
#ifndef GLOBALEXTERN
 ={		C_WHITE,	// ECT_SYSMSG	System message
		C_WHITE,	// ECT_THISDIR	This directory
		C_CYAN,		// ECT_UPDIR	Up directory
		C_RED,		// ECT_LABEL	Label
		C_CYAN,		// ECT_SUBDIR	Sub directory
		C_MAGENTA,	// ECT_SYSTEM	System
		C_BLUE,		// ECT_HIDDEN	Hidden
		C_GREEN,	// ECT_RONLY	Read only
		C_DWHITE,	// ECT_NORMAL	Normal

		C_SBLUE,	// ECT_COMPRES	Compressed
		C_DYELLOW,	// ECT_REPARSE	Reperse
		C_GREEN,	// ECT_VIRTUAL	Virtual & Offline
		C_RED,		// ECT_ENCRYPT	Encrypted
		C_MAGENTA,	// ECT_SPECIAL	Temporary & Device
};
#endif
;
GVAR COLORREF C_eInfo[21]
#ifndef GLOBALEXTERN
 ={		C_AUTO,		// ECS_MESSAGE	System message
		C_DRED,		// ECS_DELETED	Deleted
		C_AUTO,		// ECS_NORMAL	Normal
		C_DBLACK,	// ECS_GRAY		Grey
		C_DBLUE,	// ECS_CHANGED	Changed
		C_DCYAN,	// ECS_ADDED	Added
		C_DBLACK,	// ECS_NOFOCUS	NoFocus
		C_YELLOW,	// ECS_BOX		Frame
		C_GREEN,	// ECS_UDCSR	UnderLine
		C_WHITE,	// ECS_MARK		Mark
		C_AUTO,		// ECS_EVEN		Normal/Even
		C_DGREEN,	// ECS_SELECT	Select
		C_AUTO,		// ECS_SEPARATE	Separate grid line
		C_SBLUE,	// ECS_HILIGHT	Hilight 1
		C_YELLOW,	//	Hilight 2
		C_CYAN,		//	Hilight 3
		C_GREEN,	//	Hilight 4
		C_RED,		//	Hilight 5
		C_MAGENTA,	//	Hilight 6
		C_DRED,		//	Hilight 7
		C_AUTO,		// ECS_HOVER mouse hover
};
#endif
;
#define CSR_MARK_OFFSET 4

						// キャプション行の文字色／背景色
GVAR COLORREF C_capt[8]
#ifndef GLOBALEXTERN
 ={C_AUTO, C_AUTO, C_AUTO, C_AUTO, C_AUTO, C_AUTO, C_AUTO, C_AUTO}
#endif
;
#define C_ActiveCText	C_capt[0]	// アクティブ時前景色
#define C_ActiveCBack	C_capt[1]	// アクティブ時背景色
#define C_PairCText		C_capt[2]	// 反対窓前景色
#define C_PairCBack		C_capt[3]	// 反対窓背景色
#define C_InActiveCText	C_capt[4]	// 非アクティブ時前景色
#define C_InActiveCBack	C_capt[5]	// 非アクティブ時背景色
#define C_TabCloseCText	C_capt[6]	// タブ閉じるボタン前景色
#define C_TabCloseCBack	C_capt[7]	// タブ閉じるボタン背景色
GVAR COLORREF C_defextC GPARAM(C_AUTO);
GVAR COLORREF C_tip[2] GPARAM2(C_AUTO, C_AUTO);

// カーソル移動カスタマイズ ---------------------------------------------------
GVAR CURSORMOVER XC_mvUD
#ifndef GLOBALEXTERN
= {0, 1, OUTTYPE_COLUMNSCROLL, 0, OUTTYPE_STOP, B1};
#endif
;
GVAR CURSORMOVER XC_mvLR
#ifndef GLOBALEXTERN
= {4, 1, OUTTYPE_COLUMNSCROLL, 0, OUTTYPE_STOP, B1};
#endif
;
GVAR CURSORMOVER XC_mvPG
#ifndef GLOBALEXTERN
= {8, 1, OUTTYPE_PAGE, 0, OUTTYPE_STOP, B1};
#endif
;
GVAR CURSORMOVER XC_mvSC
#ifndef GLOBALEXTERN
= { 3, 0, OUTTYPE_COLUMNSCROLL, 0, OUTTYPE_STOP, B1};
#endif
;
GVAR CURSORMOVER XC_mvWH;
//-------------------------------------- アイコン
GVAR ANYSIZEICON DirIcon // ディレクトリアイコン(X_dicn)
#ifndef GLOBALEXTERN
= {NULL, NULL, NULL, NULL}
#endif
;

GVAR ANYSIZEICON UnknownIcon // 未定義アイコン(X_uicn)
#ifndef GLOBALEXTERN
= {NULL, NULL, NULL, NULL}
#endif
;

GVAR ICONCACHESTRUCT CacheIcon
#ifndef GLOBALEXTERN
 = {NULL, ICONLIST_NOMASK, 0, 0, 0, 0, 0, 0, 0, TRUE}
#endif
;
GVAR int CacheIconsX, CacheIconsY;
GVAR BOOL Use_X_icnl GPARAM(FALSE);
//-------------------------------------- 開発中
GVAR BOOL exdset GPARAM(FALSE); // XC_dset で、ワイルドカードや SHN指定を有効に
GVAR int UseCCDrawBack GPARAM(0); // ツールバー等のダークモード対応

#ifdef __cplusplus
}
#endif
