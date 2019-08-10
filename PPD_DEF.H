/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		内部定義
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#define UseTMessageBox 1 // MessageBox は 0:システムのを使う 1:自前
#define UsePPxType 1 // リソース調停に 0:Mutex を使う 1:自前

#define X_Mtask		32			// PPx の管理可能な動作数
#define X_MaxJob	32			// PPx の管理可能なJob動作数

#ifndef UNICODE
	#define HistoryID	"TH1"	// ヒストリファイルの ID
	#define CustID		"TC1"	// カスタマイズファイルの ID
#else
	#define HistoryID	"TH2"	// ヒストリファイルの ID
	#define CustID		"TC2"	// カスタマイズファイルの ID
#endif

#define TextStackSize 0x1000	// テキストスタックの大きさ

#define TIMERID_PREVENTSLEEP 0x7401 // スリープ抑制タイマ
#define TIMERRATE_PREVENTSLEEP 60*1000*3 // スリープ抑制タイマの間隔(3分)
#define TIMERID_MSGBOX_AUTORETRY 0x7402 // メッセージボックス表示中の自動再試行
#define TIMERRATE_MSGBOX_AUTORETRY 300 // (0.3秒)

//==================================================== 共有メモリ(PPCOMMON.MEM)
//#pragma pack(push, 4)

#define UsePPxSpinMaxTime 2000 // 専有可能な最大時間
#define UsePPxMaxTime 5000 // 専有可能な最大時間
typedef struct  {
	volatile LONG SpinLock;	// この構造体へのアクセス許可
	DWORD ThreadID;	// 利用中のThreadID
	int SpinCount;		// 再入回数
	DWORD dummy; // TickCount;		// 利用開始時間
} UsePPxSyncStruct;

typedef struct  {	// PPx Main Window 管理構造体
	HWND	hWnd;					// HWND
	HWND	UsehWnd;				// 占有している HWND
	DWORD	ProcessID;				// プロセスID
	DWORD	ActiveID;				// アクティブ順を表す数値
	TCHAR	ID[REGIDSIZE];			// +0, 1:Name, +2:R-ID, +3:\0
									// ここが "" なら使用していない
									//		ex) C_A / V_B / V_C
	TCHAR	path[VFPS];
} ShareX;

#define JOBFLAG_NAMEMASK	0x7f // JobX.state B0-6:実行コマンド
#define JOBFLAG_STATEMASK	0xff80 // JobX.state B7-15:ステータス
#define JOBFLAG_CHANGESTATE	B7 // STATE変更のみ、回数確認しない
#define JOBFLAG_ENABLEWAIT	B8 // Job待機が必要な処理
#define JOBFLAG_ENABLEHIDE	B9 // 隠すことが可能。JobList廃棄時に表示の必要有り
#define JOBFLAG_WAITEXEC	B10
#define JOBFLAG_WAITJOB		B11
#define JOBFLAG_PAUSE		B12
#define JOBFLAG_ERROR		B13
#define JOBFLAG_REG			B14
#define JOBFLAG_UNREG		B15
// 登録系
#define JOBSTATE_REGIST		(JOBFLAG_REG | 0) // 実行スレッドの登録(登録されているだけでは待ち対象にならない)
#define JOBSTATE_STARTJOB	(JOBFLAG_REG | JOBFLAG_ENABLEWAIT) // 1Job(待機が必要な処理)の開始
#define JOBSTATE_WAITEXEC	(JOBFLAG_REG | JOBFLAG_WAITEXEC) // Process終了待機中
#define JOBSTATE_WAITJOB	(JOBFLAG_REG | JOBFLAG_WAITJOB) // Job実行待機中
#define JOBSTATE_PAUSE		(JOBFLAG_REG | JOBFLAG_PAUSE) // 一時停止
#define JOBSTATE_ERROR		(JOBFLAG_REG | JOBFLAG_ERROR) // エラー発生中
// 登録解除系
#define JOBSTATE_UNREGIST	(JOBFLAG_UNREG | JOBSTATE_REGIST) // 実行スレッドの登録解除
#define JOBSTATE_ENDJOB		(JOBFLAG_UNREG | JOBSTATE_STARTJOB) // 1Jobの終了
#define JOBSTATE_FINWEXEC	(JOBFLAG_UNREG | JOBSTATE_WAITEXEC) // 待機完了
#define JOBSTATE_FINWJOB	(JOBFLAG_UNREG | JOBSTATE_WAITJOB) // 待機完了
#define JOBSTATE_UNPAUSE	(JOBFLAG_UNREG | JOBSTATE_PAUSE) // 再開
#define JOBSTATE_DEERROR	(JOBFLAG_UNREG | JOBSTATE_ERROR) // エラー発生中
// 経過
#define JOBSTATE_SETRATE	(0) // (cmd)進捗の通知(B0-B7:%)

typedef union {
	DWORD dw;
	struct {
		BYTE type, flags, rate, count;
	} b;
} JobXstate;

typedef struct	 {	// PPx Job 管理構造体
	HWND hWnd;
	DWORD ThreadID;
	JobXstate state;
} JobX;

typedef struct {	// PPv 通信用
	DWORD flags;
	TCHAR param[CMDLINESIZE];	// 表示ファイル名(size=0:なし)
} PPvShareX;

typedef struct {	// PPc 管理用
	HWND hComboWnd[26];	// Combo window
	HWND hLastFocusWnd;
	int PrevFocusID;	// LastFocusIDの一つ前
	int LastFocusID;	// 最後にフォーカス移動したPPc
} PPcShareX;

#define DLL_VersionValue ((DLL_Version << 18) | sizeof(ShareM))
#define PPbParamSize (CMDLINESIZE + 64)	// >H (option32) (;HWND10), 計46
#define PPbMaxSendSize (CMDLINESIZE - 1)
typedef struct {
											//-------------------------- COMMON
	int Version;					// 起動中の PPcommon の version
									//  DLL_VersionValue が入る
	UsePPxSyncStruct UsePPxSync;

	volatile DWORD CustomizeNameID;	// カスタマイズ領域名の付加ID
	volatile DWORD CustomizeWrite;	// カスタマイズ領域を書き換えて、ブロックがずれるときはインクリメントされる

	TCHAR Param[PPbParamSize];	// 通信用
	TCHAR ParamID[REGIDSIZE];		// Param の相手
	TCHAR TextStack[TextStackSize];	// Text Stack

	ShareX P[X_Mtask];				// xUI 管理テーブル
	struct {	// ActiveID の最小値、最大値
		int /*low, */high;
	} Active;

#define RUNAS_NOCHECK	 0	// 未判定
#define RUNAS_NORMAL	 1	// RunAs 使用せず
#define RUNAS_RUNAS		 2	// 別ユーザで起動中
//#define RUNAS_ELEVATION	 3	// 昇格中
//#define RUNAS_LIMITED	 4	// 制限中
	int RunAsMode;					// RunAs で起動したかどうかの判定結果
	HWND hCommonLogWnd;		// ログウィンドウ

	PPcShareX ppc;
	PPvShareX view;
	HHOOK hhookCBT; // Edit 拡張用フック

	JobX Job[X_MaxJob];				// Job 管理テーブル

	DWORD EnumNerServerThreadID;	// \\ / \\+ のPC一覧取得用スレッドのID

	BOOL NowShutdown;				// シャットダウン中かどうか

#define JOBLIST_OFF 0
#define JOBLIST_FIXED 1
#define JOBLIST_FLOAT 2
#define JOBLIST_INNER 3
#define JOBLIST_THREAD 4 // スレッド一覧も表示する

#define JOBLIST_SHOW 0
#define JOBLIST_HIDE 1
	struct {
		HWND hWnd; // Job一覧を表示するウィンドウ
		DWORD showmode, hidemode;
	} JobList;

#if PPXSYNCDEBUG
	char UsePPxFirst[96], UsePPxLast[96];
#endif
} ShareM;

// EnumCustTable 等で使用する最終アクセス位置キャッシュ
typedef struct {
	BYTE *DataPtr;		// 該当 CustData の先頭
	BYTE *SubPtr;		// SubOffset 番目の Table の位置
	DWORD Counter;		// Sm->CustomizeWrite の控え
	int SubOffset;	// offset のメモ
} ENUMCUSTCACHESTRUCT;
extern ENUMCUSTCACHESTRUCT enumcustcache;

//#pragma pack(pop)
//================================================================ コマンド実行
#define CmdFuncMaxLen	64	// *commane / %*function の名称最大値 + 1

#define ST_PATHFIX		B0	// 次の編集はパス補完が必要
#define ST_SDIRREF		B1	// サブディレクトリを参照している
#define ST_CHKSDIRREF	B2	// ST_SDIRREF の判定を済ませている
#define ST_NOREAD		B3	// ディレクトリを読まない
#define ST_MULTIPARAM	B4	// %:か、行末に達するまで実行を避ける
#define ST_USECACHE		B5	// 編集キャッシュを使用(%$x)
#define ST_USETREE		B6	// 部分編集のRefはツリー形式
#define ST_EXECLOOP		B7	// 繰り返し実行中
#define ST_EDITHIST		B8	// 一行編集のヒストリ指定有り

typedef struct tagOLDSRCSTRUCT {	// 入れ子解析時に使用する
	const TCHAR *src;
	struct tagOLDSRCSTRUCT *backptr;
} OLDSRCSTRUCT;

#define TINPUT_EDIT_OPTIONS_use_refline B0
#define TINPUT_EDIT_OPTIONS_use_optionbutton B1
#define TINPUT_EDIT_OPTIONS_use_refext B2
typedef struct {
	WORD hist_readflags;
	BYTE hist_writetype;
	BYTE flags;
} TINPUT_EDIT_OPTIONS;

typedef struct {
	HWND hWnd;				// オーナーのウィンドウハンドル
	PPXAPPINFO *Info;		// 親からの情報取得関数
	POINT *posptr;			// メニュー表示に使用する座標
	const TCHAR *src;		// 解析先頭
	TCHAR *dst;				// 展開位置

	ThSTRUCT ExtendDst;		// 展開内用の拡張保存領域
	ThSTRUCT StringVariable;	// 文字列変数領域

	PPXCMDENUMSTRUCT IInfo;
	OLDSRCSTRUCT *oldsrc;
	ThSTRUCT ExpandCache;	// %M 展開内容の繰り返し実行用キャッシュ
							// 1ststr:キャッシュ項目
							// 2ndstr:キャッシュ展開内容
							// %M を複数回使ったときは、複数組みキャッシュされる

	ERRORCODE result;		// 戻り値の保存
	DWORD ExitCode;			// XEO_SEQUENTIAL 指定時の プロセス終了コード
	DWORD command;			// CID_ // ※表示用展開はCID_FILE_EXEC限定
	int status;				// 状態保存フラグ (上記ST_ を使用)
	int useppb;				// 専有している PPB の ID
	int flag;				// 各種フラグ XEO_????
	BOOL quotation;			// 関数モジュールのパラメータが " を使用中

	struct EXECSTRUCT_Edit {	// 編集関連
		TCHAR *ed;					// 編集開始位置(%!用)
		TCHAR *EdBottom;			// 編集開始位置(%{用)
		int   CsrBottom, CsrTop;		// カーソル／選択開始／終了位置(%|)

		struct EXECSTRUCT_Cache {
			const TCHAR *srcptr; // キャッシュ対象テキストのsrcの位置
			DWORD hash;			// キャッシュ対象テキストのハッシュ
		} cache;
		TINPUT_EDIT_OPTIONS options;
	} edit;

	TCHAR curdir[VFPS];			// カレントディレクトリ
	TCHAR DstBuf[CMDLINESIZE * 2];	// 変換後の内容を保存する一時領域
} EXECSTRUCT;

typedef struct {
	const TCHAR *title;
	const TCHAR *text;
	HFONT hDlgFont;
	HWND hOldFocusWnd, hOldForegroundWnd;
	TIMERPROC autoretryfunc;
	DWORD style;
} MESSAGEDATA;

// CD-ROM ********************************
#define ISO9660HEADER	"\1CD001"
#define JOLIETHEADER	"\2CD001"
#define UDFHEADER		"\0BEA01"
#define CDHEADERSIZE	6

//================================================================ Win32API
#if !defined(SEF_DACL_AUTO_INHERIT) || defined(__BORLANDC__)
	typedef LONG (NTAPI *PVECTORED_EXCEPTION_HANDLER)(
		struct _EXCEPTION_POINTERS *ExceptionInfo
	);
#endif

TypedefWinAPI(HRESULT, SHGetFolderPath, (HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath));
TypedefWinAPI(HANDLE, OpenThread, (DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId));
TypedefWinAPI(PVOID, AddVectoredExceptionHandler, (ULONG First, PVECTORED_EXCEPTION_HANDLER Handler));
TypedefWinAPI(ULONG, RemoveVectoredExceptionHandler, (PVOID Handle));
//================================================== グローバル変数(PPD_GVAR.C)
#define GLOBALEXTERN
#include "PPD_GVAR.C"
#undef GLOBALEXTERN

//==================================================================== 内部関数
//------------------------------------------------------------------ ppd_ccmd.c
#define EWX_EXFLAG B31
#define EWX_EX_SUSPEND (EWX_EXFLAG | B0)
#define EWX_EX_HIBERNATE EWX_EXFLAG
extern void ExitSession(HWND hWnd, UINT options);
extern void LockPC(void);
extern void SendWmSysyemMessage(WPARAM command, LPARAM lParam);
extern void Get_X_save_widthUI(TCHAR *path);
extern void WindowZPosition(HWND hWnd, HWND mode);
extern void ChangeOpaqueWindow(HWND hWnd, int opaque);

//------------------------------------------------------------------ ppd_cmd.c
extern DWORD_PTR USECDECL PPxDefInfoDummyFunc(PPXAPPINFO *ppxa, DWORD cmdID, PPXAPPINFOUNION *uptr);

extern void ExtractPPxCall(HWND hTargetWnd, EXECSTRUCT *Z, const TCHAR *macroparam);
extern void BackupSrc(EXECSTRUCT *Z, const TCHAR *newsrc);
extern void GetPopupPoint(EXECSTRUCT *Z, POINT *pos);
extern DWORD GetFmacroOption(const TCHAR **string);
extern void GetFmacroString(DWORD flag, TCHAR *src, TCHAR *dest);
extern DWORD GetModuleNameHash(const TCHAR *src, TCHAR *dest);
extern TCHAR * USEFASTCALL GetZCurDir(EXECSTRUCT *Z);
extern void Get_F_MacroData(PPXAPPINFO *info, PPXCMD_F *fbuf, PPXCMDENUMSTRUCT *work);
extern BOOL GetEditMode(const TCHAR **param, TINPUT_EDIT_OPTIONS *options);
extern void SetTInputOptionFlags(TINPUT *tinput, TINPUT_EDIT_OPTIONS *options);
extern UTCHAR USEFASTCALL IsEOL(const TCHAR **str);
extern const TCHAR *ZGetTitleName(EXECSTRUCT *Z);
extern BOOL ZTinput(EXECSTRUCT *Z, TINPUT *tinput);
extern void GetValue(EXECSTRUCT *Z, DWORD cmdID, TCHAR *dest);
extern void ZStringVariable(EXECSTRUCT *Z, const TCHAR **src, int mode);

extern const TCHAR HistType[];
extern const TCHAR Title_ValueName[];
extern void USEFASTCALL UserCommand(EXECSTRUCT *Z, const TCHAR *args, const TCHAR *cmdline, TCHAR *dest);
extern DWORD GetCacheHash(EXECSTRUCT *Z);

#define StringVariable_command	0
#define StringVariable_function	1
#define StringVariable_extract	2

//------------------------------------------------------------------ ppd_cmdc.c
extern void ZExec(EXECSTRUCT *Z);
extern void ZGetName(EXECSTRUCT *Z, TCHAR *dest, TCHAR cmd);
extern void GetWindowHashList(HMENU hMenuDest, DWORD *PopupID);

//------------------------------------------------------------------ ppd_cmdl.c
extern void GetQuotedParameter(LPCTSTR *commandline, TCHAR *param, const TCHAR *parammax);
extern UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen);

//------------------------------------------------------------------ ppd_code.c
extern INT_PTR GetDigitNumber(const TCHAR **ptr);
extern INT_PTR USEFASTCALL GetHexNumber(const TCHAR **ptr);
extern int USEFASTCALL GetHexChar(TCHAR **p);
extern TCHAR USEFASTCALL upper(TCHAR c);
#ifdef UNICODE
extern INT_PTR GetNumberA(const char **line);
extern int USEFASTCALL GetHexCharA(char **p);
#define upperA(c) (char)upper((TCHAR)c)
#else
#define GetNumberA GetNumber
#define GetHexCharA GetHexChar
#endif

typedef struct { // 色エイリアス
	const TCHAR *str;	//	ラベル名
	COLORREF num;		//	値
} COLORLABEL;

extern COLORLABEL guicolor[];
#define color_s 21

#ifdef _WIN64
#define MultiByteToWideCharU8(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar) MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar)
#define WideCharToMultiByteU8(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar) WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar)
#else
ExternWinAPI (int, MultiByteToWideCharU8, (UINT CodePage, DWORD dwFlags, const char *lpMultiByteStr, int cchMultiByte, WCHAR *lpWideCharStr, int cchWideChar));
ExternWinAPI (int, WideCharToMultiByteU8, (UINT CodePage, DWORD dwFlags, const WCHAR *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cchMultiByte, const char *lpDefaultChar, BOOL *lpfUsedDefaultChar));
#define MultiByteToWideCharU8(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar) DMultiByteToWideCharU8(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar)
#define WideCharToMultiByteU8(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar) DWideCharToMultiByteU8(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar)
#endif

//------------------------------------------------------------------ ppd_cust.c
extern TCHAR PPcustCDumpText(const TCHAR *str, const TCHAR *sub, TCHAR *result);

//------------------------------------------------------------------ ppd_dlg.c
// PPXSTATIC で使用可能な制御文字
// ※キャプションを利用するため、\x0 はパラメータに使えない
#define PXSC_PAR		0x1b // 独自プログレスバー parsent(1-100), count(1-10)
#define PXSC_LEFT		0x1c // 左寄せ
#define PXSC_RIGHT		0x1d // 右寄せ
#define PXSC_NORMAL		0x1e // ハイライト解除
#define PXSC_HILIGHT	0x1f // ハイライト

extern LRESULT CALLBACK PPxStaticProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern HWND *CreateDialogWindow(HANDLE hinst, LPCTSTR lpszTemplate, HWND hParentWnd);
extern void EnableDlgWindow(HWND hDlg, int id, BOOL state);
extern void ShowDlgWindow(const HWND hDlg, const UINT id, BOOL show);
extern LPDLGTEMPLATE GetDialogTemplate(HWND hParentWnd, HANDLE hinst, LPCTSTR lpszTemplate);

//------------------------------------------------------------------ ppd_edl.c
extern void CleanUpEdit(void);
extern BOOL SetWindowTextWithSelect(HWND hED, const TCHAR *defstr);

//------------------------------------------------------------------ ppd_exec.c
extern BOOL ComExecEx(HWND hOwner, const TCHAR *line, const TCHAR *path, int *useppb, int flag, DWORD *ExitCode);
extern BOOL ComExecSelf(HWND hOwner, const TCHAR *line, const TCHAR *path, int flag, DWORD *ExitCode);
extern BOOL WaitJobDialog(HWND hWnd, HANDLE handle, const TCHAR *title, DWORD flags);

typedef struct {
	int user;
	MESSAGEDATA md;
} WAITDLGSTRUCT;

extern INT_PTR CALLBACK WaitDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);

//------------------------------------------------------------------ ppd_falt.c
extern LONG WINAPI PPxUnhandledExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo);
extern THREADSTRUCT *GetCurrentThreadInfo(void);
extern THREADSTRUCT *GetThreadInfoFromID(THREADSTRUCT **prev, DWORD ThreadID);
extern void UnRegisterThread(DWORD ThreadID);
extern void PPxSendReport(const TCHAR *text);
extern const TCHAR *GetThreadName(DWORD id);
extern void CPUinfo(void);

//------------------------------------------------------------------ ppd_hist.c
#pragma pack(push, 1)
typedef struct {
	char HeaderID[4]; // HistoryID / CustID が格納
	DWORD FileSize; // ファイルの大きさ
} PPXDATAHEADER;

#define HistHeaderFromHisP ((PPXDATAHEADER *)(HisP - sizeof(PPXDATAHEADER)))
#define HistSizeFromHisP (*(DWORD *)(HisP - sizeof(DWORD)))

#define HIST_HEADER_SIZE (sizeof(PPXDATAHEADER))
#define HIST_HEADER_FOOTER_SIZE (sizeof(PPXDATAHEADER) + sizeof(WORD))

#define CustHeaderFromCustP ((PPXDATAHEADER *)(CustP - sizeof(PPXDATAHEADER)))
#define CustSizeFromCustP (*(DWORD *)(CustP - sizeof(DWORD)))

#define CUST_HEADER_SIZE (sizeof(PPXDATAHEADER))
#define CUST_FOOTER_SIZE (sizeof(DWORD))
#define CUST_TABLE_FOOTER_SIZE (sizeof(WORD))
#define CUST_HEADER_FOOTER_SIZE (sizeof(PPXDATAHEADER) + sizeof(DWORD) + 2)
// +2 はオーバラン対策用
#pragma pack(pop)

extern void DefCust(int mode);
extern int CountHistory(WORD type);
extern void LimitHistory(WORD type);
extern DWORD USEFASTCALL GetCustDword(const TCHAR *name, DWORD defaultvalue);
extern void LoadMessageTextTable(void);
extern void USEFASTCALL MakeCustMemSharename(TCHAR *custsharename, int id);
extern const TCHAR * USEFASTCALL SearchMessageText(const TCHAR *idstr);
extern void ReloadMessageText(void);

//------------------------------------------------------------------ ppd_mdl.c
extern int CommandModule(EXECSTRUCT *Z, const TCHAR *ptr);
extern void FunctionModule(EXECSTRUCT *Z);
extern void FreePPxModule(void);
extern void NestedFunction(EXECSTRUCT *Z);

//------------------------------------------------------------------ ppd_menu.c
#define MENUFLAG_SELECT		B0
#define MENUFLAG_NOEXTACT	B1
#define MENUFLAG_SETINDEX	B2
extern BOOL MenuCommand(EXECSTRUCT *Z, TCHAR *menuname, const TCHAR *def, int menuflag);
extern void USEFASTCALL AppendMenuString(HMENU hMenu, UINT id, const TCHAR *string);
extern void USEFASTCALL AppendMenuCheckString(HMENU hMenu, UINT id, const TCHAR *string, BOOL check);

extern int TTrackPopupMenu(EXECSTRUCT *Z, HMENU hMenu, PPXMENUINFO *xminfo);
extern void MakeRootMenu(ThSTRUCT *thMenuData, HMENU hRootMenu, const TCHAR *CustName, const PPXINMENUBAR *inmenu);
extern const TCHAR * USEFASTCALL GetMenuDataString(ThSTRUCT *thMenuData, int index);

//------------------------------------------------------------------ ppd_ppe.c
#define PPE_TEXT_OPENNEW ((const TCHAR *)1)
#define PPE_TEXT_CMDMODE ((const TCHAR *)2)
typedef struct {
	const TCHAR *dummy;
	const TCHAR *param;
	const TCHAR *curdir;
} PPE_CMDMODEDATA;

//------------------------------------------------------------------ ppd_str.c
#ifdef UNICODE
  #define bchrlen(str, off) 1
  #define SearchPipe(str) tstrchr(str, '|')
#else
  extern int bchrlen(TCHAR *str, int off);
  extern char *SearchPipe(const char *str);
#endif
extern TCHAR *SkipSpaceAndFix(TCHAR *p);
extern TCHAR *tstristr(const TCHAR *target, const TCHAR *findstr);

extern char *strlimcpy(char *deststr, const char *srcstr, size_t maxlength);
#ifndef UNICODE
#define tstrlimcpy strlimcpy
#else
#define tstrlimcpy wcslimcpy
extern WCHAR *wcslimcpy(WCHAR *deststr, const WCHAR *srcstr, size_t maxlength);
#endif
extern void tstrreplace(TCHAR *text, const TCHAR *targetword, const TCHAR *replaceword);
extern const TCHAR *EscapeMacrochar(const TCHAR *string, TCHAR *buf);

//------------------------------------------------------------------ ppd_sub.c
extern void MakeUserfilename(TCHAR *dst, const TCHAR *src, const TCHAR *idname);
extern HWND GetCaptionWindow(HWND hWnd);
extern HWND GetParentCaptionWindow(HWND hWnd);
extern BOOL DeleteDirectories(const TCHAR *path, BOOL notify);
extern int FixTextImage(const char *src, DWORD memsize, TCHAR **dest, int usecp);
extern BOOL CheckLoadSize(HWND hWnd, DWORD *sizeL);
extern void USEFASTCALL SetDlgFocus(HWND hDlg, int id);
extern DWORD ReadEString(TCHAR **string, void *ptr, DWORD maxsize);
extern void WriteEString(TCHAR *string, BYTE *ptr, DWORD size);
extern WORD FixCharKeycode(WORD key);
extern DWORD crc32(const BYTE *bin, DWORD size, DWORD r);
extern UINT VTypeToCPlist[VTypeToCPlist_max];

#ifdef UNICODE
	#define WriteFileZTSIZE (VFPS * 3) // この長さまでの文字列に対応
	extern BOOL WriteFileZT(HANDLE fileh, const WCHAR *str, DWORD *wrote);
	extern LRESULT SendUTextMessage_U(HWND hWnd, UINT uMsg, WPARAM wParam, const TCHAR *text);
#else
	#define WriteFileZT(fileh, str, wrote) WriteFile(fileh, str, strlen(str), wrote, NULL)
#endif
#define K_mouse K_internal // マウスによる操作(メニュー位置判定用)
extern void GetPopMenuPos(HWND hWnd, POINT *pos, WORD key);
extern HMODULE LoadSystemWinAPI(DWORD dllID, LOADWINAPISTRUCT *apis);

//------------------------------------------------------------------ ppd_task.c
extern void FixTask(void);
extern int SearchPPx(TCHAR *ID);
extern HWND PPxGetWindow(const TCHAR *refid, int d);
extern int CountPPc(void);
extern BOOL USEFASTCALL CheckPPcID(int no);
extern BOOL BootPPB(const TCHAR *path, const TCHAR *line, int flags);
extern int UsePPb(HWND hOwner);
extern int SendPPB(HWND hOwner, const TCHAR *param, int useID);
extern DWORD RecvPPBExitCode(HWND hOwner, int useID);
extern void FreePPb(HWND hOwner, int useID);
extern void DeleteJobTask(void);
extern BOOL SetJobTask(HWND hWnd, DWORD state);
extern BOOL JobListMenu(EXECSTRUCT *Z, TCHAR param);
extern void CheckJobList(void);
extern BOOL CheckJobWait(HANDLE hHandle);
extern void SetAppID(void);
extern void CreateJobListWnd(void);
extern void USEFASTCALL ClosePPxOne(HWND hWnd);

#define GetPPxList_hWnd 0
#define GetPPcList_Path 1	// Pathをitem/thMenuDataから取得
#define GetPPxList_Select 2	// *focusコマンドをthMenuDataから取得
#define GetPPxList_Id 3		// idをthMenuDataから取得
extern int GetPPxList(HMENU hPopupMenu, int mode, ThSTRUCT *th, DWORD *menuid);
ExternWinAPI(BOOL, SetDllDirectory, (LPCTSTR lpPathName));

//------------------------------------------------------------------ ppd_ui.c
#define PPXFLASH_STOP -1
#define PPXFLASH_FLASH 0
#define PPXFLASH_NOFOCUS 1
extern BOOL PPxFlashWindow(HWND hWnd, int mode);
extern UINT GetMonitorDPI(HWND hWnd);
extern UINT USEFASTCALL GetGDIdpi(HWND hWnd);
#define ID_PPX_CHECKED B7

#define MB_PPX_EXMSGBOX    B31
#define MB_PPX_ALLCHECKBOX B30
#define MB_PPX_AUTORETRY   B29
#define MB_PPX_NOCENTER    B28
#define MB_PPX_ADDABORTRETRYIGNORE 7
#define MB_PPX_STARTCANCEL 8
#define MB_PPX_YESNOIGNORECANCEL 9
#define MB_PPX_USEPPXCHECKOKCANCEL 10
#define MB_PPX_ABORTRETRYRENAMEIGNORE 11
#define MB_PPX_MAX 12
extern BOOL MessageBoxInitDialog(HWND hDlg, MESSAGEDATA *md);
extern INT_PTR CALLBACK MessageBoxDxProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
extern void PopupErrorMessage(HWND hWnd, const TCHAR *title, const TCHAR *msg);

extern void ClipTextData(HWND hWnd, const TCHAR *text);
extern int CriticalMessageBox(const TCHAR *text);
extern BOOL PPxDialogHelp(HWND hDlg);
// FillRect 置き換え
extern void FillBox(HDC hDC, const RECT *box, HBRUSH hbr);
extern void PlayWave(const TCHAR *name);
extern BOOL OpenClipboard2(HWND hWnd);

//------------------------------------------------------------------ ppd_wild.c
extern void FreeRMatch(void);
#ifndef REXPREPLACE
typedef struct {int dummy;} RXPREPLACESTRING;

extern BOOL InitRegularExpressionReplace(RXPREPLACESTRING **rxps, TCHAR *rxstring, BOOL slash);
extern BOOL RegularExpressionReplace(RXPREPLACESTRING *rxps, TCHAR *target);
extern void FreeRegularExpressionReplace(RXPREPLACESTRING *rxps);
extern void GetRegularExpressionName(TCHAR *str);
#endif
extern BOOL InitMigemo(void);
extern void FreeMigemo(void);
extern BOOL LoadRegExp(void);


extern LOADWINAPISTRUCT OLEAUT32_SysStr[];
extern HRESULT (STDAPICALLTYPE *DGetActiveObject)(REFCLSID rclsid, void * pvReserved, IUnknown ** ppunk);
extern HRESULT (STDAPICALLTYPE *DRegisterActiveObject)(IUnknown * punk, REFCLSID rclsid, DWORD dwFlags, DWORD *pdwRegister);
extern HRESULT (STDAPICALLTYPE *DRevokeActiveObject)(DWORD dwRegister, void *pvReserved);

extern LOADWINAPISTRUCT OLEAUT32_Variant[];
extern void (STDAPICALLTYPE *DVariantInit)(VARIANTARG * pvarg);
extern HRESULT (STDAPICALLTYPE *DVariantClear)(VARIANTARG * pvarg);
extern HRESULT (STDAPICALLTYPE *DVariantChangeType)(VARIANTARG * pvargDest, VARIANTARG *pvarSrc, USHORT wFlags, VARTYPE vt);

typedef struct {
	DWORD value, mask;
} GETPARAMFLAGSSTRUCT;

extern void GetParamFlags(GETPARAMFLAGSSTRUCT *gpfs, const TCHAR **param, const TCHAR *flagnames);
extern const TCHAR AttrLabelString[];

//---------------------------------------------------------------- ppd_subp.cpp
extern void DragDropPath(HWND hWnd, const TCHAR *path);
extern LRESULT CALLBACK AutoDDDLLProc(int nCode, WPARAM wParam, CWPSTRUCT *lParam);
extern HRESULT GetLink(HWND hWnd, const TCHAR *LinkFile, TCHAR *Original);

#ifdef __cplusplus
}
#endif
