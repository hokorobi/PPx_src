/*-----------------------------------------------------------------------------
	Paper Plane xUI	 setup wizard
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPSETUP.RH"

#ifndef BIF_USENEWUI
#define BIF_USENEWUI 0x50
#endif

#define LOWSETTINGMEM_MB 384	// 低スペック用設定にするメモリ
								// (MB,指定値以下は低スペック設定になる)

#ifdef UNICODE			// PPSETUP は、現在 MultiByte のみなので、細工する
	#define XNAME	"PPW"
	#ifdef _WIN64
		#define COMMONDLL "PPLIB64W.DLL"
		#define ARCNAME "./ppx64"
	#else
		#define COMMONDLL "PPLIB32W.DLL"
		#define OLDCOMMONDLL "PPXLIB32.DLL"
		#define ARCNAME "./ppw"
	#endif
	#define CEXE	"PPCW.EXE"
	#define CSEXE	"PPCUSTW.EXE"
	#define CustID	"TC2"
	#define HistoryID "TH2"
#else
	#define XNAME	"PPX"
	#define COMMONDLL "PPLIB32.DLL"
	#define OLDCOMMONDLL "PPCOMMON.DLL"
	#define ARCNAME "./ppx"
	#define CEXE	"PPC.EXE"
	#define CSEXE	"PPCUST.EXE"
	#define CustID	"TC1"
	#define HistoryID "TH1"
#endif

#define PROPTYPE	1		// 0:prop 1:wizard
#define PROPPAGE	12		// 全ページ数
#define PROPTITLE	T("PPx Setup Wizard (") T(BITSTRING) T(",") T(CODETYPESTRING) T(")")
#define MENUDIRNAME		"PaperPlane xUI"

#if PROPTYPE
	#define PROPSTYLE	PSH_WIZARD
#else
	#define PROPSTYLE	PSH_PROPSHEETPAGE
#endif

//------------------------------------------------------------------ シート管理
typedef struct {	// 各ページの情報を保存するための構造体
										// あらかじめ初期化する必要があるもの
	int		rID;	// リソースID
	DLGPROC proc;	// コールバック関数、NULL なら使わない
} PAGEINFO;

typedef struct {
	const TCHAR *filename;
	const TCHAR *dispname;
} INSTALLFILES;

enum {	PAGE_GUIDE = 0,PAGE_SETTYPE,PAGE_UN_EXEC,PAGE_DEST,PAGE_REG,PAGE_PPCW,
		PAGE_KEY,PAGE_APP,PAGE_LINK,PAGE_READY,PAGE_COPY,PAGE_UP};

enum {	RESULT_NORMAL = 0,RESULT_FAULT,RESULT_SUCCESS,RESULT_ALLDONE,
		RESULTINFO_UNINSTALLDATA,RESULTINFO_INSTALL,
		MSG_UNICODE,MSG_SELECTPPX,MSG_OVERINSTALL,MSG_SELECTDIR,
		MSG_DEFAULTDIR,MSG_NOCOPY,MSG_SELECTEDITOR,MSG_SELECTVIEWER,
		MSG_SELECTSUSIE,MSG_ABORT,MSG_FAILACCESS,MSG_FAILCOPY,
		MSG_DLLLOADERROR,MSG_SITEERROR,MSG_RELOADARCHIVE,
		MSG_BACKUP,MSG_FOUND,MSG_RUNAS,MSG_UNZIP,MSG_BADSIG,
		MSG_COPYNOFILE,
		MSG_DLL_LOADERROR_PART,
};

//-----------------------------------
extern OSVERSIONINFO OSver;	// OS 情報
extern int XX_setupmode;				// セットアップ方法
extern TCHAR XX_setupedPPx[VFPS];		// セットアップ済みの PPx path
extern PAGEINFO *UsePageInfo;
extern const TCHAR **MessageStr;
//----------------------------------- インストール
extern int XX_instdestM;				// インストール先の種類
extern TCHAR XX_instdestP[VFPS];		// Programs File のインストール先
extern TCHAR XX_instdestS[VFPS];		// 指定のインストール先
extern TCHAR *XX_setupPPx;				// インストール/アップデート先

extern TCHAR XX_upsrc[VFPS];			// アップデート元
extern int XX_usereg;					// レジストリを利用するか
extern int XX_ppc_window;				// ppc の窓の形態
extern BOOL XX_ppc_tree;				// ppc の窓にツリー表示
extern int XX_ppc_pane;				// ppc の窓のペイン枚数
extern int XX_ppc_tab;					// ppc の窓のタブ形態
extern int XX_ppc_join;				// ppc の窓の連結形態
extern int XX_keytype;					// キー割り当て方法
extern BOOL XX_diakey;					// ダイアモンドカーソル
extern BOOL XX_emenu;					// 英語メニュー
extern BOOL XX_doscolor;				// DOS配色
extern TCHAR XX_editor[VFPS];			// テキストエディタ
extern TCHAR XX_viewer[VFPS];			// 汎用ビューア
extern TCHAR XX_susie[VFPS];			// Susie plug-in
extern BOOL XX_usesusie;				// Susie plug-in の設定を使用する
extern BOOL XX_resultquiet;				// エラーがなければ結果画面を省略
extern int XX_link_menu;				// スタートメニューに登録
extern int XX_link_cmenu;				// 共用スタートメニューに登録
extern int XX_link_boot;				// スタートアップに登録
extern int XX_link_desk;				// デスクトップに登録
extern int XX_link_app;				// 追加と削除に登録

extern const TCHAR *UNARCS[];

//------------------------------------------------------------------

// PPLIBxxx.DLL の定義
typedef int (PPXAPI * PASSETCUSTTABLEA)(const char *str, const char *sub, void *bin, DWORD b_size);
typedef int (PPXAPI * PASSETCUSTTABLEW)(const WCHAR *str, const WCHAR *sub, void *bin, DWORD b_size);
typedef HRESULT (PPXAPI * MAKESHORTCUTA)(const char *LinkedFile, const char *LinkFname, const char *DestPath);
typedef HRESULT (PPXAPI * MAKESHORTCUTW)(const WCHAR *LinkedFile, const WCHAR *LinkFname, const WCHAR *DestPath);
typedef void (PPXAPI * PASXSEND)(UINT uMsg,WPARAM wParam,LPARAM lParam);
typedef void (USECDECL * OLDXSEND)(UINT uMsg,WPARAM wParam,LPARAM lParam);

// WINAPI
ExternWinAPI(HRESULT,SHGetFolderPath,(HWND hwnd,int csidl,HANDLE hToken,DWORD dwFlags,LPTSTR pszPath));

// ダイアログ
INT_PTR CALLBACK StyleDlgProc(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);

INT_PTR CALLBACK GuideDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK SetTypeDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK UnExecDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK DestDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK RegDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK PPcwDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK KeyDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK AppDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK LinkDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK UpDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK CopyDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK ReadyDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);

#define AUTOSETUP_FIN	0
#define AUTOSETUP_COPYDIALOG	1
extern int AutoSetup(const TCHAR *param);
//------------------------------------------------------------------ PPSETUP.C
extern HINSTANCE hInst;
extern HWND hResultWnd; // 結果を書きこむボックス

#define SRESULT_NOERROR 0
#define SRESULT_FAULT 1
#define SRESULT_ACCESSERROR 2
extern int SetupResult;		// 処理が成功したかを記憶する
extern BOOL DeleteUpsrc;	// 終了時に XX_upsrc を削除する
extern TCHAR MyPath[VFPS];	// 自分自身のディレクトリ

extern INSTALLFILES InstallFiles[];
extern const TCHAR NilStr[];
extern const TCHAR Str_PPxRegPath[];
extern const TCHAR Str_InstallPPxPath[];
extern const TCHAR Set_InstallPPxName[];
extern const TCHAR msgboxtitle[];

//------------------------------------------------------------------ PPSET_S.C
extern void Cmd(TCHAR *param);
extern void CheckDlgButtons(HWND hDlg,int start,int end,int check);
extern void CloseAllPPxLocal(TCHAR *destpath,BOOL setuppath);
extern BOOL CopyPPxFiles(HWND hWnd,const TCHAR *srcpath,const TCHAR *destpath);
extern int GetDlgButtons(HWND hDlg,int start,int end);
extern BOOL GetRegStrLocal(HKEY hKey,const TCHAR *path,const TCHAR *name,_Out_ TCHAR *dest);
extern BOOL SearchPPx(void);
extern BOOL SelectDirectory(HWND hWnd,const TCHAR *title,UINT flag,BFFCALLBACK proc,TCHAR *path);
extern int CALLBACK SelDirProc(HWND hWnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
extern int CALLBACK SelInstPPxProc(HWND hWnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
extern void SetExecuteFile(HWND hWnd,int ID,const TCHAR *title);
extern void USEFASTCALL SMessage(const TCHAR *str);

extern void WriteResult(const TCHAR *str,int add);
extern BOOL DeleteDir(const TCHAR *path);
extern BOOL GetWinAppDir(int csidl,TCHAR *path);

#define ADMINMODE_NOADMIN	0
#define ADMINMODE_ADMIN		1
#define ADMINMODE_ELEVATE	2
extern int AdminCheck(void);

//------------------------------------------------------------------ PPSET_IN.C
extern BOOL InstallMain(HWND hWnd);

//------------------------------------------------------------------ PPSET_DG.C
extern ERRORCODE Execute_ExtractMacro(const TCHAR *param);
