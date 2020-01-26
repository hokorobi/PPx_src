/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						Global Value
-----------------------------------------------------------------------------*/
#ifdef GLOBALEXTERN
	#define GVAR extern
	#define GPARAM(x)
	#define GPARAM2(x, y)
#else
	#undef	GVAR
	#undef	GPARAM
	#undef GPARAM2
	#define GVAR
	#define GPARAM(x) = x
	#define GPARAM2(x, y) = {x, y}
#endif

GVAR int X_pmc[4] GPARAM({X_pmc_defvalue});

GVAR int TouchMode GPARAM(0);
GVAR DWORD ButtonMenuTick GPARAM(0); // ボタンメニューの誤表示抑制用

GVAR const TCHAR NilStr[1] GPARAM(T(""));
GVAR TCHAR NilStrNC[1] GPARAM(T(""));
GVAR const TCHAR PPXJOBMUTEX[] GPARAM(T("PPxJOB"));	// 実行シリアライズ用Mutex
GVAR const TCHAR StrListFileid[] GPARAM(T("::listfile"));
GVAR const TCHAR StrPath[] GPARAM(T("PATH"));
GVAR const TCHAR StrCustOthers[] GPARAM(T("_others"));
GVAR const TCHAR StrCustSetup[] GPARAM(T("_Setup"));
GVAR const TCHAR PPxRegPath[] GPARAM(T(PPxSettingsRegPath));
GVAR const TCHAR IdlCacheName[] GPARAM(T("#IdlC"));
GVAR const TCHAR StrShortcutExt[] GPARAM(T(ShortcutExt));
GVAR const TCHAR StrX_dlf[] GPARAM(T("X_dlf"));
GVAR const TCHAR StrX_tree[] GPARAM(T("X_tree"));

GVAR const TCHAR PathJumpNameEx[] GPARAM(T("<%j>M_pjump"));
#define PathJumpName (PathJumpNameEx + 4)
GVAR const TCHAR StrMDrives[]	GPARAM(T("M_drives"));

GVAR const TCHAR BUTTONstr[]	GPARAM(T("BUTTON"));
GVAR const TCHAR EDITstr[]		GPARAM(T("EDIT"));
GVAR const TCHAR STATICstr[]	GPARAM(T("STATIC"));
GVAR const TCHAR LISTBOXstr[]	GPARAM(T("LISTBOX"));
GVAR const TCHAR SCROLLBARstr[]	GPARAM(T("SCROLLBAR"));
GVAR const TCHAR COMBOBOXstr[]	GPARAM(T("COMBOBOX"));
GVAR const TCHAR FullPathMacroStr[] GPARAM(T("NDC"));

GVAR const TCHAR TreeClassStr[] GPARAM(T(VFSTREECLASS));

GVAR const TCHAR STR_WAITOPERATION[] GPARAM(MES_MWOP);

GVAR const char StrUDF[]		GPARAM(UDFHEADER);
GVAR const char StrISO9660[]	GPARAM(ISO9660HEADER);

#ifdef UNICODE
	#define XNAME		T("PPW")
	#define HISTNAME	T("PPWH")
	#define CUSTNAME_	T("PPWC")
	#define HISTMEMNAME	T("PPWHIST.MEM")
	#define CUSTMEMNAME	T("PPWCUST.MEM")
#else
	#define XNAME		T("PPX")
	#define HISTNAME	T("PPXH")
	#define CUSTNAME_	T("PPXC")
	#define HISTMEMNAME	T("PPHIST.MEM")
	#define CUSTMEMNAME	T("PPCUST.MEM")
#endif
GVAR const TCHAR CUSTNAME[] GPARAM(CUSTNAME_);

GVAR const TCHAR *JobTypeNames[]
#ifndef GLOBALEXTERN
= {
	MES_JMJO, MES_JMAT, MES_JMCO, MES_JMCS, MES_JMRE,
	T("Thread"),
	MES_JMAP, MES_JMAE,
	MES_JMMO, MES_JMCP, MES_JMMR, MES_JMSH,
	MES_JMLI, MES_JMDE, MES_JMUN, MES_JMSL,
}
#endif
;
/*
GVAR const TCHAR MenuCacheItem_ValueName[]	GPARAM(T(StringVariable_Command_MenuCacheItem));
GVAR const TCHAR MenuCacheText_ValueName[]	GPARAM(T(StringVariable_Command_MenuCacheText));
*/

GVAR const TCHAR PPxName[] GPARAM(T("PPx"));
GVAR const TCHAR PPcExeName[] GPARAM(T(PPCEXE));
GVAR const TCHAR PPvExeName[] GPARAM(T(PPVEXE));
GVAR const TCHAR XEO_OptionString[] GPARAM(T(XEO_STRINGS));
GVAR const TCHAR WildCard_All[] GPARAM(T("*"));
GVAR const TCHAR ShellVerb_open[] GPARAM(T("open"));
GVAR const TCHAR StrFtp[] GPARAM(T("ftp://"));
#define StrFtpSize TSTROFF(6)
GVAR const TCHAR StrAux[] GPARAM(T("aux:"));
#define StrAuxSize TSTROFF(4)

GVAR const WORD HistReadTypeflag[15]
#ifndef GLOBALEXTERN
 = {
	PPXH_GENERAL | PPXH_COMMAND | PPXH_FILENAME,
					PPXH_NUMBER,	PPXH_COMMAND,	PPXH_DIR_R,	// gnhd
	PPXH_FILENAME,	PPXH_NAME_R,	PPXH_SEARCH,	PPXH_WILD_R, // cfsm
	PPXH_PPCPATH,	PPXH_PPVNAME,	0,							// pve
	PPXH_USER1,		PPXH_USER1,		PPXH_USER2,		PPXH_USER2	// uUxX
}
#endif
;
GVAR const WORD HistWriteTypeflag[15]
#ifndef GLOBALEXTERN
 = {
	PPXH_GENERAL,	PPXH_NUMBER,	PPXH_COMMAND,	PPXH_DIR,	// gnhd
	PPXH_FILENAME,	PPXH_PATH,		PPXH_SEARCH,	PPXH_MASK,	// cfsm
	PPXH_PPCPATH,	PPXH_PPVNAME,	0,							// pve
	PPXH_USER1,		PPXH_USER1,		PPXH_USER2,		PPXH_USER2	// uUxX
}
#endif
;
GVAR const DWORD TinputTypeflags[15]
#ifndef GLOBALEXTERN
 = {
	0,	TIEX_SINGLEREF,	0,	TIEX_REFTREE | TIEX_SINGLEREF,		// gnhd
	TIEX_SINGLEREF,	TIEX_SINGLEREF,	TIEX_SINGLEREF,	0,			// cfsm
	TIEX_SINGLEREF,	TIEX_SINGLEREF,	0,							// pve
	TIEX_SINGLEREF,	0,		TIEX_REFTREE | TIEX_SINGLEREF, 0		// uUxX
}
#endif
;
GVAR const TCHAR StrPackZipFolderTitle[] GPARAM(T("zip - zipfldr"));
GVAR const TCHAR StrPackZipFolderCommand[] GPARAM(T("%uzipfldr.dll,A \"%2\""));
GVAR const TCHAR EditCache_ValueName[] GPARAM(T(StringVariable_Command_EditCache));

GVAR const TCHAR StrOptionError[] GPARAM(T("Option error: %s"));

GVAR const TCHAR StrUserCommand[] GPARAM(T("_Command"));

GVAR const char User32DLL[] GPARAM("USER32.DLL");

//----------------------------------------------------- PPLIBxxx.DLL 固有の情報
GVAR HINSTANCE DLLhInst;		// DLL のインスタンス
GVAR TCHAR DLLpath[MAX_PATH];	// DLL の所在地

GVAR HANDLE ProcHeap;			// DLL の GetProcessHeap 値
#define DLLheap ProcHeap		// DLL内のHeap(現在はGetProcessHeap)
GVAR UINT  WM_PPXCOMMAND;		// WM_PPXCOMMAND の登録値
GVAR TCHAR TempPath[MAX_PATH];	// このプロセス専用のテンポラリパス
GVAR TCHAR SyncTag[16];			// 同期オブジェクト名に使用する "PPXxxxxx"

//-------------------------------------------------------------------- 共有資源
GVAR ShareM	*Sm GPARAM(NULL);		// 共有メモリへのポインタ
GVAR BYTE *HisP;					// ヒストリ
GVAR BYTE *CustP;					// カスタマイズ領域
GVAR DWORD X_Hsize GPARAM(0x10000);	// History size
GVAR DWORD X_Csize GPARAM(0x10000);	// Customize size
#ifdef TMEM_H
GVAR FM_H SM_cust;					// カスタマイズ領域のファイルマップ
#endif
GVAR ThSTRUCT ProcessStringValue	// プロセス内限定特殊環境変数
#ifndef GLOBALEXTERN
	= { 0, 0, 0 };
#endif
;
GVAR CRITICAL_SECTION ThreadSection; // 汎用スレッドセーフ用

//------------------------------------------------------------------- Customize
GVAR HWND hUpdateResultWnd GPARAM(NULL); // カスタマイズログ(終了時に閉じる為)
GVAR int X_es	GPARAM(0x1d);	// 拡張シフトのコード
GVAR int X_mwid	GPARAM(60);		// メニューの最大桁数(要PPx再起動)
GVAR int X_dss	GPARAM(DSS_NOLOAD);	// 画面自動スケーリング
GVAR DWORD X_flst[3]
#ifndef GLOBALEXTERN
	= { 0, 1, 0 };
#endif
;
#define NOMESSAGETEXT (BYTE *)BADPTR	// 読み込みできない場合の値
GVAR BYTE *MessageTextTable GPARAM(NULL);	// 表示メッセージ用テーブル
GVAR DWORD X_jinfo[4]
#ifndef GLOBALEXTERN
= {MAX32, 1, 1, 1}
#endif
;
//---------------------------------------------------------------------- OS情報
GVAR OSVERSIONINFO OSver;	// OS 情報
#define WINTYPE_9x		0

#ifdef UNICODE
	#define If_WinNT_Block
#else
	#define If_WinNT_Block if (OSver.dwPlatformId == VER_PLATFORM_WIN32_NT)
#endif

GVAR DWORD WinType GPARAM(WINTYPE_9x);	// Windows の Version 情報

#ifndef UNLEN
#define UNLEN 256
#endif
GVAR TCHAR UserName[UNLEN + 1];	// ログオンユーザ名

GVAR TCHAR NumberUnitSeparator GPARAM(','); // 桁区切り記号
GVAR TCHAR NumberDecimalSeparator GPARAM('.'); // 小数点記号

//---------------------------------------------------------------------- その他
GVAR DWORD X_beep GPARAM(0xfff);	/* Beep の出力フラグ 1010 1111
									 B0:致命的エラー表示
									 B1:一般エラー表示
									 B2:重要でないエラー表示
									 B3:重要な警告
									 B4:重要でない警告
									 B5:結果を表示する情報
									 B6:好意で出力する情報/作業完了
									 B7:その他
								*/
GVAR DWORD X_log GPARAM(0);	// 各種ログを出力
GVAR int X_execs GPARAM(-1);
GVAR DWORD X_Keyra GPARAM(1);
GVAR int X_jlst[2] GPARAM2(-1, 1);
GVAR int X_prtg GPARAM(-1);

GVAR DWORD StartTick; // 実行開始時のTick
GVAR DWORD CustTick GPARAM(0);
GVAR BOOL NowExit GPARAM(FALSE);
GVAR void **FaultOptionInfo GPARAM(NULL); // 異常終了時の追加情報ポインタへのポインタ
GVAR PVOID UEFvec; // 例外処理ハンドラ情報

GVAR HWND hTipWnd GPARAM(NULL);
GVAR HWND hProcessComboWnd GPARAM(NULL);

GVAR int X_combos_[2] GPARAM2(-1, -1);
#define CMBS_THREAD			B7	// メインスレッド共通化
#define CMBS1_DIALOGNOPANE	B4	// ダイアログを一体化窓の中心に

GVAR CRMENUSTACKCHECK CrmenuCheck // スタック異常等の検出用
#ifndef GLOBALEXTERN
= {0, 0}
#endif
;
GVAR TCHAR AuthHostCache[0x100];
GVAR TCHAR AuthUserCache[100];
GVAR TCHAR AuthPassCache[100];

//---------------------------------------------------------------------- 通信用
GVAR PPXAPPINFO PPxDefInfoDummy
#ifndef GLOBALEXTERN
= {(PPXAPPINFOFUNCTION)PPxDefInfoDummyFunc, PPxName, NilStr, NULL}
#endif
;
GVAR PPXAPPINFO *PPxDefInfo GPARAM(&PPxDefInfoDummy);
//---------------------------------------------------------------------- DLL
GVAR HMODULE hBregexpDLL GPARAM(NULL);
GVAR HMODULE hBregonigDLL GPARAM(NULL);
GVAR HMODULE hMigemoDLL GPARAM(NULL);

GVAR HMODULE hComctl32 GPARAM(NULL);	// COMCTL32.DLL のハンドル
GVAR HMODULE hShell32 GPARAM(NULL);
GVAR HMODULE hKernel32 GPARAM(NULL);
GVAR ValueWinAPI(SHGetFolderPath) GPARAM(NULL);
GVAR ValueWinAPI(OpenThread) GPARAM(INVALID_VALUE(impOpenThread));
#ifdef _WIN64
#define DAddVectoredExceptionHandler AddVectoredExceptionHandler
#define DRemoveVectoredExceptionHandler RemoveVectoredExceptionHandler
#else
GVAR ValueWinAPI(AddVectoredExceptionHandler) GPARAM(NULL);
GVAR ValueWinAPI(RemoveVectoredExceptionHandler) GPARAM(NULL);
#endif
//---------------------------------------------------------------------
// 窓
GVAR COLORREF C_3dHighlight GPARAM(C_AUTO); // COLOR_BTNHIGHLIGHT
GVAR COLORREF C_3dFace GPARAM(C_AUTO); // COLOR_BTNFACE
GVAR COLORREF C_3dShadow GPARAM(C_AUTO); // COLOR_BTNSHADOW
// 文字
GVAR COLORREF C_WindowText GPARAM(C_AUTO); // COLOR_WINDOWTEXT
GVAR COLORREF C_HighlightText GPARAM(C_AUTO); // COLOR_HIGHLIGHTTEXT
// 背景
GVAR COLORREF C_WindowBack GPARAM(C_AUTO); // COLOR_WINDOW
GVAR COLORREF C_HighlightBack GPARAM(C_AUTO); // COLOR_HIGHLIGHT
GVAR COLORREF C_GrayState GPARAM(C_AUTO); // COLOR_GRAYTEXT 非アクティブ時の文字・背景

GVAR HBRUSH h3dHighlight GPARAM(NULL); // 左・上
GVAR HBRUSH h3dFace GPARAM(NULL); // 中央
GVAR HBRUSH h3dShadow GPARAM(NULL); // 右
GVAR HBRUSH hHighlightBack GPARAM(NULL);
GVAR HBRUSH hGrayBack GPARAM(NULL);

GVAR HBRUSH hWndBackBrush GPARAM(NULL);

//--------------------------------------------------------------------- COM定義
// RegExp
extern IID XCLSID_RegExp;
//extern IID XCLSID_Match;
//extern IID XCLSID_MatchCollection;
extern IID XIID_IRegExp;
extern IID XIID_IMatch;
extern IID XIID_IMatch2;
extern IID XIID_IMatchCollection;
extern IID XIID_ISubMatches;

// Shell Extension
extern IID XIID_IContextMenu3;
extern IID XIID_IColumnProvider;
extern IID XIID_IPropertySetStorage;

// Etc
#ifdef WINEGCC
extern IID XIID_IClassFactory;
extern IID XIID_IUnknown;
extern IID XIID_IDataObject;
extern IID XIID_IDropSource;

extern IID XIID_IStorage;
extern IID XIID_IPersistFile;

// Shell Extension
extern IID XCLSID_ShellLink;
extern IID XIID_IContextMenu;
extern IID XIID_IShellFolder;
extern IID XIID_IShellLink;
extern IID XIID_IContextMenu2;
#else
#define XCLSID_ShellLink CLSID_ShellLink
#define XIID_IClassFactory IID_IClassFactory
#define XIID_IContextMenu IID_IContextMenu
#define XIID_IContextMenu2 IID_IContextMenu2
#define XIID_IDataObject IID_IDataObject
#define XIID_IDropSource IID_IDropSource
#define XIID_IPersistFile IID_IPersistFile
#define XIID_IShellFolder IID_IShellFolder
#define XIID_IShellLink IID_IShellLink
#define XIID_IStorage IID_IStorage
#define XIID_IUnknown IID_IUnknown
#define XIID_IPicture IID_IPicture
#endif
