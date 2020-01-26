/*-----------------------------------------------------------------------------
	Paper Plane xUI											～ ツリー窓 ～
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <commctrl.h>
#include <shlobj.h>
#include <dbt.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#pragma hdrstop

HBRUSH hEx3dfaceBrush = NULL;

#ifndef TBSTYLE_CUSTOMERASE
#define TBSTYLE_CUSTOMERASE 0x2000
#define CDDS_PREERASE 0x00000003
#define CDRF_SKIPDEFAULT 0x00000004
#endif
#ifndef NM_CUSTOMDRAW
#define NM_CUSTOMDRAW (NM_FIRST-12)

typedef struct
{
	NMHDR hdr;
	DWORD dwDrawStage;
	HDC hdc;
	RECT rc;
	DWORD_PTR dwItemSpec;
	UINT uItemState;
	LPARAM lItemlParam;
} NMCUSTOMDRAW;
#endif

#define TreeCollapseCheck 0

#define VTM_I_CHANGENOTIFY	(WM_APP + 501) // 更新通知用
#define VTM_I_CHANGEPATH_MAIN	(WM_APP + 502) // 更新通知用

enum { TREETYPE_DIRECTORY = 1, TREETYPE_FAVORITE,
	TREETYPE_PPCLIST, TREETYPE_VFS, TREETYPE_FOCUSPPC,
	TREETYPE__CLOSE, TREETYPE__RELOAD,
	TREETYPE__SHOWICON, TREETYPE__SIMPLEICON, TREETYPE__SHOWOVERLAY,
	TREETYPE__SHOWTOOLBAR, TREETYPE__SYNCSELECT, TREETYPE__NOLINE,
	TREETYPE__SINGLENODE,
	TREETYPE__SHOWSUPERHIDEEN,
	TREETYPE__DIALOGRENAME,
	TREETYPE__NEWFOLDER, TREETYPE__NEWLIST, TREETYPE__LISTCUSTOMIZE,
	TREETYPE__RENAMEITEM, TREETYPE__DELETEITEM,
	TREETYPE__USERLIST
};
const BYTE NullTable[4] = {0, 0, 0, 0};

int splitbarwide = 4;		// 仕切の幅
int TreeIconSize = 16;		// アイコンの大きさ
int TreeIconSHflag = SHGFI_ICON | SHGFI_SMALLICON;	// SHGetFileInfo 用のアイコン大きさに合わせたフラグ
BOOL FixColor = FALSE;
DWORD X_dlf_ddt;

#define XTREE_SHOWICON B0	// アイコンを表示する
#define XTREE_SIMPLEICON B1	// アイコンの簡易表示
#define XTREE_SHOWTOOLBAR B2	// ツールバーを表示する
#define XTREE_DIALOGRENAME B3	// 名前変更ダイアログを使う
#define XTREE_SYNCSELECT B4	// 選択連動
#define XTREE_SHOWSUPERHIDEEN B5	// (保護されたオペレーティング システム ファイルを表示しない (推奨))を表示する
#define XTREE_SINGLENODE B6	// 現在フォルダ以外は畳む( TVS_SINGLEEXPAND )
#define XTREE_NOLINE B7	// 線を表示しない( !TVS_HASLINES )
#define XTREE_DISABLEOVERLAY B8	// アイコンオーバレイを表示しない
// TVS_TRACKSELECT マウスカーソルがアイテムの上に来たときに下線が付きます
//TVS_EX_FADEINOUTEXPANDOS

#define XTREE_DEFAULT (XTREE_SHOWICON | XTREE_SIMPLEICON | XTREE_DIALOGRENAME | XTREE_SYNCSELECT | XTREE_SHOWTOOLBAR)

#define PPCBUTTON 0
#define GRID 10 // オートスクロール領域の幅

#define TREEVOID (void) // コンパイル時にでる不要な警告をなくす。※一部のTreeViewメッセージ用

#ifndef SHGFI_ADDOVERLAYS
	#define SHGFI_ADDOVERLAYS 0x000000020
#endif
#ifndef TVS_TRACKSELECT
	#define TVS_TRACKSELECT 0x0200
#endif
#ifndef TVS_SINGLEEXPAND
	#define TVS_SINGLEEXPAND 0x0400
#endif
#ifndef TVS_EX_FADEINOUTEXPANDOS
	#define TVS_EX_FADEINOUTEXPANDOS 0x0040
#endif
#ifndef TVM_SETEXTENDEDSTYLE
	#define TVM_SETEXTENDEDSTYLE (TV_FIRST + 44)
#endif

DefineWinAPI(HIMAGELIST, ImageList_Create, (int cx, int cy, UINT flags, int cInitial, int cGrow)) = 0;
DefineWinAPI(BOOL, ImageList_Destroy, (HIMAGELIST himl));
DefineWinAPI(COLORREF, ImageList_SetBkColor, (HIMAGELIST himl, COLORREF clrBk));
DefineWinAPI(int, ImageList_ReplaceIcon, (HIMAGELIST himl, int i, HICON hicon));
#define DImageList_AddIcon(himl, hicon) DImageList_ReplaceIcon(himl, -1, hicon)

LOADWINAPISTRUCT ImageCtltDLL[] = {
	LOADWINAPI1(ImageList_Create),
	LOADWINAPI1(ImageList_Destroy),
	LOADWINAPI1(ImageList_SetBkColor),
	LOADWINAPI1(ImageList_ReplaceIcon),
	{NULL, NULL}
};

#ifndef CCS_VERT
#define CCS_VERT 0x00000080L
#endif
#ifndef TB_SETSTYLE
#define TB_SETSTYLE (WM_USER + 56)
#define TB_GETSTYLE (WM_USER + 57)
#endif

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
DefineWinAPI(ULONG, SHChangeNotifyRegister, (HWND hwnd, int fSources, LONG fEvents, UINT wMsg, int cEntries, const SHChangeNotifyEntry *pshcne));
DefineWinAPI(BOOL, SHChangeNotifyDeregister, (unsigned long ulID));
DefineWinAPI(HANDLE, SHChangeNotification_Lock, (HANDLE hChange, DWORD dwProcId, LPITEMIDLIST **pppidl, LONG *plEvent));
DefineWinAPI(BOOL, SHChangeNotification_Unlock, (HANDLE hLock));

LOADWINAPISTRUCT SHChangeNotifyDLL[] = {
	LOADWINAPI1(SHChangeNotifyRegister),
	LOADWINAPI1(SHChangeNotifyDeregister),
	LOADWINAPI1(SHChangeNotification_Lock),
	LOADWINAPI1(SHChangeNotification_Unlock),
	{NULL, NULL}
};
#endif

#define DefaultTreeListName PathJumpName
const TCHAR TreePPcListName[] = T("*ppclist");
const TCHAR TreeFocusPPcName[] = T("*focusppc");

const TCHAR StrTreeProp[] = T("PPxTree");
typedef struct {
	PPXAPPINFO vtinfo;	// PPx 共通情報(VFSTreeのhWndはここ) ※必ず先頭に配置
	HWND hTViewWnd;		// TreeView の hWnd
	HWND hBarWnd;		// ToolBar
	HWND hNotifyWnd;	// 通知先

	HWND hParentWnd;
	HIMAGELIST hImage;
	HTREEITEM hHitItem;	// D&D中にホバー動作で選択されている項目
	HFONT hTreeFont;
	WNDPROC OldTreeWndProc;

	ERRORCODE *resultCodePtr;
	TCHAR *resultStrPtr;

#if TreeCollapseCheck
	volatile DWORD CollapseCheck, CollapseID;
	#define CollapseCheckID 0xf0c08040
#endif
	DWORD flags;		// 動作指定 VFSTREE_
	int TreeType;
	ThSTRUCT itemdata; // ツリーの項目内容記憶
	ThSTRUCT cmdwork; // ツールバーなどの記憶領域
	SIZE BarSize; // ツールバーの大きさ
	DWORD X_tree;
	unsigned long ShNotifyID; // 更新通知

	volatile DWORD MainThreadID; // ツリーのスレッドのID
	volatile DWORD ActiveThreadID; // 現在有効なスレッドのID
	volatile int ThreadCount; // 生きているスレッド数(0:ウィンドウを閉じた)
	volatile int ExpandCount; // ExpandTree実行数
} VFSTREESTRUCT;

#define MaxTreeThreads 10

LRESULT CALLBACK TreeProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void VfsTreeSetFlagFix(VFSTREESTRUCT *VTS);
BOOL GetSelectedTreePath(VFSTREESTRUCT *VTS, TCHAR *path);
ERRORCODE TreeKeyCommand(VFSTREESTRUCT *VTS, WORD key);
BOOL InitTreeViewItems(VFSTREESTRUCT *VTS, const TCHAR *param);
BOOL SetPathItems(VFSTREESTRUCT *VTS, HTREEITEM hFirstItem, const TCHAR *path, DWORD EventType);
BOOL GetTreePath(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem, TCHAR *buf);
void AddTreeItemFromList(VFSTREESTRUCT *VTS, const TCHAR *list);
HTREEITEM AppendItemToTreeV(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent, int type, const TCHAR *datastr);
void TreeContextMenu(VFSTREESTRUCT *VTS, const TCHAR *cmd, WORD key);
BOOL ExpandTree(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem, const TCHAR *pathptr);
HTREEITEM AddItemToTree(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent);
int AddItemToTreePath(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent, const TCHAR *path);
void TreeTypeMenu(VFSTREESTRUCT *VTS, HTREEITEM hTItem, WORD key);
void TreeItemDelete(VFSTREESTRUCT *VTS, WORD key, HTREEITEM hTreeitem);

const UINT treedeletemesboxstyle[] = { 0, MB_QYES, MB_QNO };


typedef struct {
	VFSTREESTRUCT *VTS;
	TCHAR path[VFPS];
} InitTreeViewItems_Thread_Struct;

void WaitTreeViewThread(VFSTREESTRUCT *VTS, BOOL sub)
{
	int count = 3000 / 20;
	MSG msg;

	for ( ;; ){ // 少し待つ
		if ( VTS->ThreadCount <= 1 ) break;

		if ( sub == FALSE ){ // メッセージポンプを回して、同期等を処理する
			while ( PeekMessage(&msg, VTS->vtinfo.hWnd, 0, 0, PM_REMOVE) ){
				if ( msg.message == WM_QUIT ) break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		Sleep(20);
		if ( --count == 0 ){
//			XMessage(NULL, NULL, XM_DbgLOG, T("WaitTreeViewThread timeout"));
			return; // タイムアウト
		}
	}
	return;
}

void FreeVTS(VFSTREESTRUCT *VTS)
{
	ThFree(&VTS->cmdwork);
	ThFree(&VTS->itemdata);
	HeapFree(DLLheap, 0, VTS);
}

#if TreeCollapseCheck
void WMTreeDestroy_LOG(VFSTREESTRUCT *VTS)
{
	TCHAR buf[0x1000], *dest;
	int size = 32;
	BYTE *srcp = (BYTE *)VTS;

	dest = buf + wsprintf(buf, T("Tree管理メモリの破損:%x"), VTS->CollapseID);
	while( size ){
		dest += wsprintf(dest, T(" %02X"), *srcp++);
		size--;
	}
	wsprintf(dest, T("/") T(PTRPRINTFORMAT) T("/") T(PTRPRINTFORMAT), VTS->hImage, VTS->resultCodePtr);
	XMessage(NULL, NULL, XM_DbgLOG, T("%s"), buf);
}

void WMTreeDestroy_DUMP(VFSTREESTRUCT *VTS)
{
	TCHAR buf[0x1000], *dest;
	int size = 32;
	BYTE *srcp = (BYTE *)&VTS->hParentWnd;

	dest = buf + wsprintf(buf, T("Tree管理メモリ破損2:%x"), VTS->CollapseID);
	while( size ){
		dest += wsprintf(dest, T(" %02X"), *srcp++);
		size--;
	}
	wsprintf(dest, T("/") T(PTRPRINTFORMAT), VTS->hImage);
	PPxSendReport(buf);
}
#endif

void WMTreeDestroy(VFSTREESTRUCT *VTS)
{
	if ( VTS->MainThreadID == 0 ) return; // 既に終了中
	VTS->MainThreadID = VTS->ActiveThreadID = 0; // 中断要求

#if TreeCollapseCheck
	if ( (VTS->CollapseCheck != CollapseCheckID) &&
		 (VTS->CollapseID == 0) ){
		VTS->CollapseID = 3;
	}
#endif
	if ( !(VTS->flags & VFSTREE_PPC) ){ // サイズ記憶
		RECT box;
		DWORD data[5];

		memset(data, 0, sizeof(data));
		GetCustData(StrX_tree, data, sizeof(data));
		GetWindowRect(VTS->vtinfo.hWnd, &box);
		box.bottom -= box.top;
		if ( VTS->flags & VFSTREE_MENU ){ // %*tree
			data[3] = box.right - box.left;
			data[4] = box.bottom;
		}else{ // 一行編集 / *file の参照tree
			if ( GetParent(VTS->vtinfo.hWnd) != NULL ){ // 親と一体
				data[1] = box.bottom;
			}else{ // 独立
				data[2] = box.bottom;
			}
		}
		data[0] = VTS->X_tree;
		SetCustData(StrX_tree, data, sizeof(data));
	}

	// ImageList / Font は使用していた窓が廃棄されたので そのまま削除可
	if ( VTS->ShNotifyID != 0 ){
		DSHChangeNotifyDeregister(VTS->ShNotifyID);
		VTS->ShNotifyID = 0;
	}

	WaitTreeViewThread(VTS, FALSE);

#if TreeCollapseCheck
	if ( (VTS->CollapseCheck != CollapseCheckID) &&
		 (VTS->CollapseID == 0) ){
		VTS->CollapseID = 4;
	}
#endif

	DestroyWindow(VTS->hTViewWnd);

#if TreeCollapseCheck
	if ( (VTS->CollapseCheck != CollapseCheckID) &&
		 (VTS->CollapseID == 0) ){
		VTS->CollapseID = 5;
	}
#endif

	if ( VTS->hTreeFont != NULL ){
		DeleteObject(VTS->hTreeFont);
		VTS->hTreeFont = NULL;
	}

	SetWindowLongPtr(VTS->vtinfo.hWnd, GWLP_USERDATA, (LONG_PTR)0);
#if TreeCollapseCheck
	if ( VTS->CollapseCheck != CollapseCheckID ){
		WMTreeDestroy_DUMP(VTS);
		return;
	}
#endif
	if ( VTS->hImage != NULL ){
		if ( DImageList_Destroy != NULL ) DImageList_Destroy(VTS->hImage);
		VTS->hImage = NULL;
	}

	if ( VTS->resultCodePtr != NULL ){
		if ( *VTS->resultCodePtr == ERROR_BUSY ){
			*VTS->resultCodePtr = ERROR_CANCELLED;
		}
		VTS->resultCodePtr = NULL;
	}

	if ( VTS->ThreadCount > 0 ){
		// メモリリークになるが、他のスレッドが閉じれない時は解放しない
		if ( --VTS->ThreadCount == 0 ){
			DefWindowProc(VTS->vtinfo.hWnd, WM_DESTROY, 0, 0);
			FreeVTS(VTS);
		}
	}
}

DWORD WINAPI InitTreeViewItems_Thread(InitTreeViewItems_Thread_Struct *ITVI_TI)
{
	THREADSTRUCT threadstruct = {T("TreeBuild"), XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};

	PPxRegisterThread(&threadstruct);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	WaitTreeViewThread(ITVI_TI->VTS, TRUE);
	ITVI_TI->VTS->ThreadCount++;
	InitTreeViewItems(ITVI_TI->VTS, ITVI_TI->path);
	if ( --ITVI_TI->VTS->ThreadCount == 0 ) FreeVTS(ITVI_TI->VTS);
	HeapFree(DLLheap, 0, ITVI_TI);
	CoUninitialize();
	PPxUnRegisterThread();
	return 0;
}

void Start_InitTreeViewItems(VFSTREESTRUCT *VTS, const TCHAR *param)
{
	InitTreeViewItems_Thread_Struct *ITVI_TI;

	if ( VTS->ThreadCount > MaxTreeThreads ) return; // スレッドが多すぎるので中止する

	if ( (ITVI_TI = HeapAlloc(DLLheap, 0, sizeof(InitTreeViewItems_Thread_Struct))) == NULL ){
		return;
	}

	ITVI_TI->VTS = VTS;
	if ( param == NULL ){
		TreeProc(VTS->vtinfo.hWnd, VTM_GETSETTINGS, 0, (LPARAM)ITVI_TI->path);
	}else{
		tstrcpy(ITVI_TI->path, param);
	}

	#ifndef WINEGCC
	if ( ((VTS->TreeType == TREETYPE_DIRECTORY) ||
		  (VTS->TreeType == TREETYPE_VFS) )
	#ifndef _WIN64
		  && ( OSver.dwMajorVersion >= 5 ) // 次の行に続く
	#endif
	){
		HANDLE hThread;

		hThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)InitTreeViewItems_Thread, ITVI_TI, 0, (DWORD *)&VTS->ActiveThreadID);
		if ( hThread != NULL ){
			SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
			CloseHandle(hThread);
			return;
		}
	}
	#endif
	InitTreeViewItems(VTS, ITVI_TI->path);
	HeapFree(DLLheap, 0, ITVI_TI);
}

DWORD WINAPI TreeSetDirPath_Thread(InitTreeViewItems_Thread_Struct *ITVI_TI)
{
	THREADSTRUCT threadstruct = {T("TreeSetPath"), XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};

	PPxRegisterThread(&threadstruct);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	WaitTreeViewThread(ITVI_TI->VTS, TRUE);
	ITVI_TI->VTS->ThreadCount++;
	SetPathItems(ITVI_TI->VTS, TreeView_GetRoot(ITVI_TI->VTS->hTViewWnd), ITVI_TI->path, 0);
	if ( --ITVI_TI->VTS->ThreadCount == 0 ) FreeVTS(ITVI_TI->VTS);
	HeapFree(DLLheap, 0, ITVI_TI);
	CoUninitialize();
	PPxUnRegisterThread();
	return 0;
}

void Start_TreeSetDirPath(VFSTREESTRUCT *VTS, const TCHAR *param)
{
	InitTreeViewItems_Thread_Struct *ITVI_TI;

	if ( (ITVI_TI = HeapAlloc(DLLheap, 0, sizeof(InitTreeViewItems_Thread_Struct))) == NULL ){
		return;
	}
	ITVI_TI->VTS = VTS;

	tstrcpy(ITVI_TI->path, param);

	#ifndef WINEGCC
	if ( ((VTS->TreeType == TREETYPE_DIRECTORY) ||
		  (VTS->TreeType == TREETYPE_VFS) )
	#ifndef _WIN64
		  && ( OSver.dwMajorVersion >= 5 ) // 次の行に続く
	#endif
	){
		HANDLE hThread;

		hThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)TreeSetDirPath_Thread, ITVI_TI, 0, (DWORD *)&VTS->ActiveThreadID);
		if ( hThread != NULL ){
			SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
			CloseHandle(hThread);
			return;
		}
	}
	#endif
	SetPathItems(VTS, TreeView_GetRoot(VTS->hTViewWnd), ITVI_TI->path, 0);
	HeapFree(DLLheap, 0, ITVI_TI);
}

void VFSTreeClose(VFSTREESTRUCT *VTS)
{
	if ( VTS->hNotifyWnd != NULL ){
		PostMessage(VTS->hNotifyWnd, WM_PPXCOMMAND, KTN_close, 0);
	}else{
		PostMessage(VTS->hTViewWnd, WM_CLOSE, 0, 0);
	}
}

//-------------------------------------- キー拡張したときのスーパークラス
LRESULT CALLBACK TreeViewProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	VFSTREESTRUCT *VTS;

	VTS = (VFSTREESTRUCT *)GetProp(hWnd, StrTreeProp);
	switch(iMsg){
		case WM_MENUCHAR:
		case WM_MENUSELECT:
		case WM_MENUDRAG:
		case WM_MENURBUTTONUP:
			return PPxMenuProc(hWnd, iMsg, wParam, lParam);

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			if ( TreeKeyCommand(VTS, (WORD)(wParam | GetShiftKey() | K_v))
					!= ERROR_INVALID_FUNCTION ){
				return 0;
			}
			break;

		case WM_SYSCHAR:
		case WM_CHAR:
			if ( ((WORD)wParam < 0x80) ){
				if ( TreeKeyCommand(VTS, FixCharKeycode((WORD)wParam))
						!= ERROR_INVALID_FUNCTION ){
					return 0;
				}
			}
			break;

	}
	return CallWindowProc(VTS->OldTreeWndProc, hWnd, iMsg, wParam, lParam);
}

void TreeCommand(VFSTREESTRUCT *VTS, const TCHAR *param) // *tree 本体
{
	TCHAR code, code2, cmdstr[CMDLINESIZE];
	const TCHAR *treetype;

	GetCommandParameter(&param, cmdstr, TSIZEOF(cmdstr));
	if ( !tstrcmp(cmdstr, T("menu")) ){
		TreeTypeMenu(VTS, INVALID_HANDLE_VALUE, 0);
		return;
	}
	if ( !tstrcmp(cmdstr, T("on")) ) return; // 表示→表示済み
	if ( !tstrcmp(cmdstr, T("focus")) ){
		SetFocus(VTS->hTViewWnd);
		return;
	}
	if ( !tstrcmp(cmdstr, T("unfocus")) ){
		TreeKeyCommand(VTS, K_raw | VK_TAB);
		return;
	}

	code = cmdstr[0];
	code2 = cmdstr[1];
	if ( !tstrcmp(cmdstr, T("off")) || (code == '\0') ){
		VFSTreeClose(VTS);
		return;
	}

	if ( (code == 'M') && (cmdstr[1] == '_') ){
		VTS->TreeType = TREETYPE_FAVORITE;
		treetype = cmdstr;
	}else if ( ((code == '1') && (code2 == '\0')) || !tstrcmp(cmdstr, T("favorite")) ){
		VTS->TreeType = TREETYPE_FAVORITE;
		treetype = DefaultTreeListName;
	}else if ( ((code == '2') && (code2 == '\0')) || !tstrcmp(cmdstr, T("ppclist")) ){
		VTS->TreeType = TREETYPE_PPCLIST;
		treetype = NilStr;
	}else if ( !tstrcmp(cmdstr, T("focusppc")) ){
		VTS->TreeType = TREETYPE_FOCUSPPC;
		treetype = NilStr;
	}else if ( ((code == '0') && (code2 == '\0')) || !tstrcmp(cmdstr, T("root"))){
		VTS->TreeType = TREETYPE_DIRECTORY;
		treetype = NilStr;
	}else{
		VTS->TreeType = TREETYPE_VFS;
		treetype = cmdstr;
	}
	InitTreeViewItems(VTS, treetype);
}

//-------------------------------------- Command からのコールバック
DWORD_PTR USECDECL VFSTreeInfo(VFSTREESTRUCT *VTS, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case 'C':
			if ( GetSelectedTreePath(VTS, uptr->enums.buffer) == FALSE ){
				*uptr->enums.buffer = '\0';
			}
			break;

		case PPXCMDID_COMMAND:{
			if ( !tstrcmp(uptr->str, T("TREE")) ){
				TreeCommand(VTS, uptr->str + tstrlen(uptr->str) + 1);
				break;
			}
			return ERROR_INVALID_FUNCTION ^ 1;
		}

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

DWORD USEFASTCALL GetTreeWindowStyle(VFSTREESTRUCT *VTS)
{
	DWORD style = WS_VISIBLE | WS_CHILD | TVS_EDITLABELS | TVS_HASBUTTONS |
		TVS_SHOWSELALWAYS;

	if ( !(VTS->X_tree & XTREE_NOLINE) ){
		setflag(style, TVS_HASLINES | TVS_LINESATROOT);
	}
	if ( VTS->X_tree & XTREE_SINGLENODE ) setflag(style, TVS_SINGLEEXPAND);
	if ( GetCustDword(T("X_poshl"), 1) ) setflag(style, TVS_TRACKSELECT);
	return style;
}

void USEFASTCALL InitTreeIcon(VFSTREESTRUCT *VTS)
{
	if ( !(VTS->X_tree & XTREE_SHOWICON) ) return;
	if ( DImageList_Create != NULL ) return;
	// 初めての呼び出しなら関数準備
	LoadWinAPI(NULL, LoadCommonControls(ICC_TREEVIEW_CLASSES), ImageCtltDLL, LOADWINAPI_HANDLE);
}

#ifndef TVM_SETITEMHEIGHT
	#define TVM_SETITEMHEIGHT (TV_FIRST + 27)
	#define TVM_GETITEMHEIGHT (TV_FIRST + 28)
#endif

BOOL InitTreeFont(VFSTREESTRUCT *VTS, int dpi)
{
	LOGFONTWITHDPI cursfont;
	HDC hDC;
	BOOL changed = FALSE;

	// 区切り線の太さを決定
	hDC = GetDC(VTS->vtinfo.hWnd);
	splitbarwide = (GetSystemMetrics(SM_CXSIZEFRAME) * dpi) / GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(VTS->vtinfo.hWnd, hDC);

	// フォントを取得
	GetPPxFont(PPXFONT_F_tree, dpi, &cursfont);
	if ( cursfont.font.lfFaceName[0] != '\0' ){
		VTS->hTreeFont = CreateFontIndirect(&cursfont.font);
		SendMessage(VTS->hTViewWnd, WM_SETFONT,
				(WPARAM)VTS->hTreeFont, TMAKELPARAM(FALSE, 0));
		changed = TRUE;
	}

	if ( TouchMode & TOUCH_LARGEHEIGHT ){
		int nowheight = (int)(LRESULT)SendMessage(VTS->hTViewWnd, TVM_GETITEMHEIGHT, 0, 0);
		int minheight = (dpi * 60) >> 8; // 6.0mm ( 60 / 254 ) の近似値
		if ( nowheight < minheight ){
			SendMessage(VTS->hTViewWnd, TVM_SETITEMHEIGHT, minheight, 0);
		}
	}
	return changed;
}

void TreeDpiChanged(VFSTREESTRUCT *VTS, WPARAM wParam)
{
	HFONT hOldFont;

	hOldFont = (HFONT)SendMessage(VTS->hTViewWnd, WM_GETFONT, 0, 0);
	if ( InitTreeFont(VTS, HIWORD(wParam)) && (hOldFont != NULL) ){
		DeleteObject(hOldFont);
	}
}

void WMTreeCreate(HWND hWnd)
{
	VFSTREESTRUCT *VTS;

	if ( (VTS = HeapAlloc(DLLheap, HEAP_ZERO_MEMORY, sizeof(VFSTREESTRUCT))) == NULL ){
		return;
	}
	VTS->vtinfo.Function = (PPXAPPINFOFUNCTION)VFSTreeInfo;
	VTS->vtinfo.Name = T("Tree");
	VTS->vtinfo.RegID = NilStr;
	VTS->vtinfo.hWnd = hWnd;
//	VTS->flags = 0;
	VTS->TreeType = TREETYPE_DIRECTORY;
//	VTS->hNotifyWnd = NULL;
//	VTS->hImage = NULL;
//	VTS->hTreeFont = NULL;
//	VTS->OldTreeWndProc = NULL;
	VTS->MainThreadID = VTS->ActiveThreadID = GetCurrentThreadId();
	VTS->ThreadCount = 1;
	VTS->X_tree = GetCustDword(StrX_tree, XTREE_DEFAULT);
//	VTS->hBarWnd = NULL;
//	VTS->ShNotifyID = 0;
#if TreeCollapseCheck
	VTS->CollapseCheck = CollapseCheckID;
#endif
	ThInit(&VTS->itemdata);
	ThInit(&VTS->cmdwork);
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)VTS);
	InitSysColors();

	VTS->hTViewWnd = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NilStr,
			GetTreeWindowStyle(VTS), 0, 0, 100, 100, hWnd, NULL, DLLhInst, NULL);
	if ( X_dss & DSS_COMCTRL ) SendMessage(VTS->hTViewWnd, CCM_DPISCALE, TRUE, 0);

	InitTreeFont(VTS, GetMonitorDPI(hWnd));

	InitTreeIcon(VTS);
	ShowWindow(VTS->hTViewWnd, SW_SHOWNORMAL);
	#ifndef _WIN64
	if ( DSHChangeNotifyRegister == NULL ){
		LoadWinAPI(NULL, hShell32, SHChangeNotifyDLL, LOADWINAPI_HANDLE);
	}
	if ( DSHChangeNotifyRegister == NULL ) return;
	#endif
	{
		SHChangeNotifyEntry req_entry;

		SHGetSpecialFolderLocation(hWnd, CSIDL_DESKTOP, (LPITEMIDLIST *)&req_entry.pidl);
		req_entry.fRecursive = TRUE;
		VTS->ShNotifyID = DSHChangeNotifyRegister(hWnd,
				SHCNRF_ShellLevel | SHCNRF_NewDelivery,
				SHCNE_MKDIR | SHCNE_RMDIR | SHCNE_UPDATEDIR |
				SHCNE_RENAMEFOLDER | SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED,
				VTM_I_CHANGENOTIFY, 1, &req_entry);
		FreePIDL(req_entry.pidl);
	}
}

void SetPPcColor(HWND hTWnd)
{
	COLORREF CC_tree[2] = {C_AUTO, C_AUTO};

	if ( NO_ERROR != GetCustData(T("CC_tree"), &CC_tree, sizeof(CC_tree)) ){
		return;
	}
	if ( CC_tree[0] == C_AUTO ){
		if ( (CC_tree[1] == C_AUTO) && (FixColor == FALSE) ) return;
		CC_tree[0] = C_WindowText;
	}
	if ( CC_tree[1] == C_AUTO ) CC_tree[1] = C_WindowBack;
	(void)TreeView_SetTextColor(hTWnd, CC_tree[0]);
	(void)TreeView_SetBkColor(hTWnd, CC_tree[1]);
	FixColor = TRUE;
}

const TCHAR *GetTreeItemCustname(VFSTREESTRUCT *VTS, HTREEITEM hTitem, HTREEITEM *hParentTreeItem)
{
	HTREEITEM hParentItem;
	TV_ITEM tvi;

	hParentItem = TreeView_GetParent(VTS->hTViewWnd, hTitem);
	if ( hParentItem == NULL ){
		if ( hParentTreeItem != NULL ) *hParentTreeItem = TVI_ROOT;
		return ThPointerT(&VTS->itemdata, 0);
	}else{
		TCHAR *custname;

		if ( hParentTreeItem != NULL ) *hParentTreeItem = hParentItem;
		tvi.hItem = hParentItem;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(VTS->hTViewWnd, &tvi);
		custname = ThPointerT(&VTS->itemdata, tvi.lParam);
		if ( *custname == '%' ) return custname + 1;
		return NULL;
	}
}

int FindCustTarget(VFSTREESTRUCT *VTS, HTREEITEM hSrcItem, const TCHAR *srcname, TCHAR *custnamebuf, TCHAR *keyword, TCHAR *param, int *sizebuf)
{
	int index = 0, size;
	const TCHAR *custname;

	custname = GetTreeItemCustname(VTS, hSrcItem, NULL);
	if ( custname == NULL ) return -1;
	while( (size = EnumCustTable(index, custname, keyword, param, TSTROFF(CMDLINESIZE))) >= 0 ){
		if ( (keyword[1] != '\0') ?
			(tstrcmp(keyword, srcname) == 0) :
			((srcname[0] == keyword[0]) && (srcname[1] == ':') &&
			 (srcname[2] == ' ') && (tstrcmp(param, srcname + 3) == 0)) ){
			tstrcpy(custnamebuf, custname);
			*sizebuf = size;
			return index;
		}
		index++;
	}
	return -1;
}

BOOL TreeRenameMain(VFSTREESTRUCT *VTS, HTREEITEM hSrcItem, const TCHAR *srcname, const TCHAR *destname)
{
	TCHAR custname[MAX_PATH], keyword[CMDLINESIZE], param[CMDLINESIZE];

	TV_ITEM tvi;

	switch ( VTS->TreeType ){
		case TREETYPE_DIRECTORY: {
			#define srcbuf keyword
			#define destbuf param

			if ( GetTreePath(VTS, hSrcItem, srcbuf) == FALSE ) break;
			tstrcpy(destbuf, srcbuf);
			tstrcpy(FindLastEntryPoint(destbuf), destname);
			if ( MoveFileL(srcbuf, destbuf) == FALSE ){
				ERRORCODE result;

				result = GetLastError();
				PPErrorBox(VTS->hTViewWnd, NULL, result);
				if ( (result == ERROR_ALREADY_EXISTS) ||
					 (result == ERROR_INVALID_NAME) ){
					return FALSE;
				}
				return TRUE;
			}else{
				SHChangeNotify(SHCNE_RENAMEFOLDER, SHCNF_PATH, srcbuf, destbuf);
			}
			break;
			#undef srcbuf
			#undef destbuf
		}
		case TREETYPE_FAVORITE: {
			int size, index;

			index = FindCustTarget(VTS, hSrcItem, srcname, custname, keyword, param, &size);
			if ( index < 0 ){
				XMessage(VTS->hTViewWnd, NULL, XM_GrERRld, T("Can't rename"));
				return TRUE;
			}
			DeleteCustTable(custname, NULL, index);
			if ( param[0] == '\0' ){
				tstrcpy(param, keyword);
				size = TSTRSIZE32(param);
			}
			InsertCustTable(custname, destname, index, param, size);
			break;
		}
		default:
			return FALSE;
	}
	// 成功したので後処理をする
	tvi.mask = TVIF_TEXT;
	tvi.hItem = hSrcItem;
	tvi.pszText = (TCHAR *)destname;
	tvi.cchTextMax = tstrlen32(destname);
	TreeView_SetItem(VTS->hTViewWnd, &tvi);
	return TRUE;
}

void TreeItemEdit(VFSTREESTRUCT *VTS, TV_DISPINFO *dispinfo)
{
	TV_ITEM tvi;
	TCHAR tvibuf[VFPS];
								// 変更前の名前を取得
	tvi.mask = TVIF_TEXT | TVIF_PARAM;
	tvi.hItem = dispinfo->item.hItem;
	tvi.pszText = tvibuf;
	tvi.cchTextMax = TSIZEOF(tvibuf);
	TreeView_GetItem(VTS->hTViewWnd, &tvi);
								// 名前変更無し
	if ( (dispinfo->item.pszText == NULL) ||
		 (0 == tstrcmp(dispinfo->item.pszText, tvibuf)) ){
		return;
	}
	TreeRenameMain(VTS, dispinfo->item.hItem, tvibuf, dispinfo->item.pszText);
}

int StartTreeRename(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem, const TCHAR *name)
{
	if ( (VTS->TreeType == TREETYPE_FAVORITE) ||
		 (VTS->TreeType == TREETYPE_DIRECTORY) ){
		if ( VTS->TreeType == TREETYPE_DIRECTORY ){
			if ( TreeView_GetParent(VTS->hTViewWnd, hTreeitem) == NULL ){
				return 1; // : / \\ / drive: は変更できない
			}
		}

		if ( VTS->X_tree & XTREE_DIALOGRENAME ){
			TCHAR buf[VFPS];
			TINPUT tinput;

			tinput.hOwnerWnd = VTS->hTViewWnd;
			tinput.hWtype	= PPXH_FILENAME;
			tinput.hRtype	= PPXH_NAME_R;
			tinput.title	= MES_TREN;
			tinput.buff		= buf;
			tinput.size		= TSIZEOF(buf);
			tinput.info		= &VTS->vtinfo;

			tstrcpy(buf, name);

			for ( ; ; ){
				tinput.flag = TIEX_USEREFLINE | TIEX_USEINFO | TIEX_SINGLEREF | TIEX_FIXFORPATH;
				if ( tInputEx(&tinput) <= 0 ) break;
				if ( FALSE != TreeRenameMain(VTS, hTreeitem, name, buf) ){
					break;
				}
			}
			SetFocus(VTS->hTViewWnd);
			// return 1 扱い
		}else{
			return 0; // in Place 名前変更を許可
		}
	}
	return 1;
}


void USEFASTCALL SwitchXtree(VFSTREESTRUCT *VTS, DWORD flags)
{
	DWORD data[3];

	VTS->X_tree ^= flags;
	GetCustData(StrX_tree, data, sizeof(data));
	data[0] = VTS->X_tree;
	SetCustData(StrX_tree, data, sizeof(data));
}

void ChangeTreeWindowStyle(VFSTREESTRUCT *VTS, DWORD flags)
{
	SwitchXtree(VTS, flags);
	SetWindowLongPtr(VTS->hTViewWnd, GWL_STYLE, GetTreeWindowStyle(VTS));
	InvalidateRect(VTS->hTViewWnd, NULL, TRUE);
}

void TreeTypeMenu(VFSTREESTRUCT *VTS, HTREEITEM hTItem, WORD key)
{
	HTREEITEM hItem;
	HMENU hMenu, hSubMenu;
	POINT pos;
	int index;
	int enumcount = 0;

	TCHAR keyword[CMDLINESIZE], name[CMDLINESIZE];
	TCHAR custname[MAX_PATH];
	TCHAR itemstr[VFPS];

	hMenu = CreatePopupMenu();
	hSubMenu = CreatePopupMenu();

	if ( hTItem != INVALID_HANDLE_VALUE ){
		hItem = hTItem;
	}else{
		hItem = TreeView_GetSelection(VTS->hTViewWnd);
	}
	//if ( VTS->TreeType == TREETYPE_PPCLIST )
	// focus / close
	if ( (VTS->TreeType == TREETYPE_FAVORITE) ||
		 (VTS->TreeType == TREETYPE_DIRECTORY) ){
		if ( hItem != NULL ){
			if ( VTS->TreeType == TREETYPE_FAVORITE ){
				int size, findindex;
				TV_ITEM tvi;

				tvi.mask = TVIF_TEXT | TVIF_PARAM;
				tvi.hItem = hItem;
				tvi.pszText = itemstr;
				tvi.cchTextMax = TSIZEOF(itemstr);
				TreeView_GetItem(VTS->hTViewWnd, &tvi);
				findindex = FindCustTarget(VTS, hItem, itemstr, custname, keyword, name, &size);
				if ( findindex >= 0 ){
					AppendMenuString(hMenu, TREETYPE__RENAMEITEM, MES_TREN);
					AppendMenuString(hMenu, TREETYPE__DELETEITEM, MES_TCLR);
				}
			}
		}
		AppendMenuString(hMenu, TREETYPE__NEWFOLDER, MES_MTNF);
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	}
		// ツリーの表示形式
	AppendMenuCheckString(hMenu, TREETYPE_DIRECTORY, MES_MTFO, VTS->TreeType == TREETYPE_DIRECTORY);
	AppendMenuCheckString(hMenu, TREETYPE_FAVORITE, MES_MTFA, VTS->TreeType == TREETYPE_FAVORITE);
	AppendMenuCheckString(hMenu, TREETYPE_PPCLIST, MES_MTPL, VTS->TreeType == TREETYPE_PPCLIST);
	AppendMenuCheckString(hMenu, TREETYPE_FOCUSPPC, MES_MTFP, VTS->TreeType == TREETYPE_FOCUSPPC);

	while( EnumCustTable(enumcount, T("Mt_type"), keyword, name, sizeof(name)) >= 0 ){
		AppendMenuString(hMenu, enumcount + TREETYPE__USERLIST, keyword);
		enumcount++;
	}
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	AppendMenuString(hMenu, TREETYPE__NEWLIST, MES_MTNU);
	AppendMenuString(hMenu, TREETYPE__LISTCUSTOMIZE, MES_MTUE);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	// 設定
	AppendMenu(hMenu, MF_EPOP, (UINT_PTR)hSubMenu, MessageText(MES_MTST));
	AppendMenuCheckString(hSubMenu, TREETYPE__SHOWICON, MES_MTSI,
			VTS->X_tree & XTREE_SHOWICON);
	if ( VTS->X_tree & XTREE_SHOWICON ){
		AppendMenuCheckString(hSubMenu, TREETYPE__SIMPLEICON, MES_MTMI,
				VTS->X_tree & XTREE_SIMPLEICON);
		if ( !(VTS->X_tree & XTREE_SIMPLEICON) ){
			AppendMenuCheckString(hSubMenu, TREETYPE__SHOWOVERLAY, MES_MTOL,
					!(VTS->X_tree & XTREE_DISABLEOVERLAY) );
		}
	}
	if ( IsExistCustData(T("B_tree")) ){
		AppendMenuCheckString(hSubMenu, TREETYPE__SHOWTOOLBAR, MES_MTSB,
				VTS->X_tree & XTREE_SHOWTOOLBAR);
	}
	AppendMenuCheckString(hSubMenu, TREETYPE__NOLINE, MES_MTSL,
			!(VTS->X_tree & XTREE_NOLINE));
	if ( VTS->flags & VFSTREE_PPC ){
		AppendMenuCheckString(hSubMenu, TREETYPE__SYNCSELECT, MES_MTSS,
				VTS->X_tree & XTREE_SYNCSELECT);
	}
	AppendMenuCheckString(hSubMenu, TREETYPE__SINGLENODE, MES_MTSN,
			VTS->X_tree & XTREE_SINGLENODE);
	AppendMenuCheckString(hSubMenu, TREETYPE__DIALOGRENAME, MES_MTDR,
			VTS->X_tree & XTREE_DIALOGRENAME);
	AppendMenuCheckString(hSubMenu, TREETYPE__SHOWSUPERHIDEEN, MES_MTSH,
			VTS->X_tree & XTREE_SHOWSUPERHIDEEN);

	AppendMenuString(hMenu, TREETYPE__RELOAD, MES_MTRL);

	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuString(hMenu, TREETYPE__CLOSE, MES_MTCL);

	GetPopMenuPos(VTS->hTViewWnd, &pos, key);
	index = TrackPopupMenu(hMenu, TPM_TDEFAULT, pos.x, pos.y, 0, VTS->hTViewWnd, NULL);
	switch (index){
		case TREETYPE_DIRECTORY:
			VTS->TreeType = TREETYPE_DIRECTORY;
			Start_InitTreeViewItems(VTS, NilStr);
			break;

		case TREETYPE_FAVORITE:
			VTS->TreeType = TREETYPE_FAVORITE;
			Start_InitTreeViewItems(VTS, DefaultTreeListName);
			break;

		case TREETYPE_PPCLIST:
			VTS->TreeType = TREETYPE_PPCLIST;
			Start_InitTreeViewItems(VTS, NilStr);
			break;

		case TREETYPE_FOCUSPPC:
			VTS->TreeType = TREETYPE_FOCUSPPC;
			Start_InitTreeViewItems(VTS, NilStr);
			break;

		case TREETYPE__RELOAD:
			TreeKeyCommand(VTS, K_raw | VK_F5);
			break;

		case TREETYPE__CLOSE:
			VFSTreeClose(VTS);
			break;

		case TREETYPE__SHOWICON:
			SwitchXtree(VTS, XTREE_SHOWICON);
			InitTreeIcon(VTS);
			TreeKeyCommand(VTS, K_raw | VK_F5);
			break;

		case TREETYPE__SIMPLEICON:
			SwitchXtree(VTS, XTREE_SIMPLEICON);
			TreeKeyCommand(VTS, K_raw | VK_F5);
			break;

		case TREETYPE__SHOWTOOLBAR:
			SwitchXtree(VTS, XTREE_SHOWTOOLBAR);
			VfsTreeSetFlagFix(VTS);
			break;

		case TREETYPE__DIALOGRENAME:
			SwitchXtree(VTS, XTREE_DIALOGRENAME);
			break;

		case TREETYPE__SYNCSELECT:
			SwitchXtree(VTS, XTREE_SYNCSELECT);
			break;

		case TREETYPE__SINGLENODE:
			ChangeTreeWindowStyle(VTS, XTREE_SINGLENODE);
			break;

		case TREETYPE__NOLINE:
			ChangeTreeWindowStyle(VTS, XTREE_NOLINE);
			break;

		case TREETYPE__SHOWOVERLAY:
			SwitchXtree(VTS, XTREE_DISABLEOVERLAY);
			TreeKeyCommand(VTS, K_raw | VK_F5);
			break;

		case TREETYPE__SHOWSUPERHIDEEN:
			SwitchXtree(VTS, XTREE_SHOWSUPERHIDEEN);
			TreeKeyCommand(VTS, K_raw | VK_F5);
			break;

		case TREETYPE__NEWFOLDER:{
			TV_ITEM tvi;
			if ( VTS->TreeType == TREETYPE_DIRECTORY ){
				if( GetTreePath(VTS, hItem, itemstr) == FALSE ) break;
				PP_ExtractMacro(VTS->vtinfo.hWnd, &VTS->vtinfo, NULL, MessageText(MES_NWDN), name, 0);
				CatPath(NULL, itemstr, name);
				GetUniqueEntryName(itemstr);
				if ( IsTrue(CreateDirectoryL(itemstr, NULL)) ){
					AddItemToTree(VTS, FindLastEntryPoint(itemstr), hItem);
				}
			}else if ( VTS->TreeType == TREETYPE_FAVORITE ){
				int count = 1;
				const TCHAR *favname;

				favname = GetTreeItemCustname(VTS, hItem, NULL);
				if ( favname == NULL ) break;

				for (;;){
					wsprintf(name, T("%%M_treelist%02d"), count++);
					if ( tstricmp(name + 1, favname) == 0 ) continue;
					if ( !IsExistCustData(name + 1) ) break;
				}
				SetCustData(name + 1, &NullTable, 4);
				InsertCustTable(favname, T("Folder"), 0x7fffffff, name, TSTRSIZE(name));
				AppendItemToTreeV(VTS, T("Folder"), hItem, 0, name);
			}
			tvi.mask = TVIF_CHILDREN;
			tvi.hItem = hItem;
			tvi.cChildren = 1;
			TreeView_SetItem(VTS->hTViewWnd, &tvi);
			TreeView_Expand(VTS->hTViewWnd, hItem, TVE_EXPAND);
			break;
		}
		case TREETYPE__NEWLIST: {
			int count = 1;

			for(;;){
				wsprintf(name, T("M_treelist%02d"), count++);
				if ( !IsExistCustData(name) ) break;
			}
			VTS->TreeType = TREETYPE_FAVORITE;
			Start_InitTreeViewItems(VTS, name);
			break;
		}

		case TREETYPE__LISTCUSTOMIZE:
			PP_ExtractMacro(VTS->hTViewWnd, NULL, NULL, T("%Ob *ppcust /:Mt_type"), NULL, 0);
			break;

		case TREETYPE__RENAMEITEM:
			if ( StartTreeRename(VTS, hItem, itemstr) == 0 ){
				SendMessage(VTS->hTViewWnd, TVM_EDITLABEL, 0, (LPARAM)hItem);
			}
			break;

		case TREETYPE__DELETEITEM:
			TreeItemDelete(VTS, VK_DELETE, hItem);
			break;

		default:
			if ( index >= TREETYPE__USERLIST ){
				name[0] = '\0';
				EnumCustTable(index - TREETYPE__USERLIST, T("Mt_type"), keyword, name, sizeof(name));
				VTS->TreeType = TREETYPE_FAVORITE;
				Start_InitTreeViewItems(VTS, name);
			}
			break;
	}
	DestroyMenu(hMenu);
}

void FixTreeSize(VFSTREESTRUCT *VTS)
{
	RECT box;

	GetClientRect(VTS->vtinfo.hWnd, &box);
	if ( VTS->flags & VFSTREE_PPC ) box.top = PPCBUTTON;
	if ( VTS->flags & VFSTREE_SPLITR ){
		box.right -= splitbarwide;
		if ( box.right < 1 ) box.right = 1;
	}
	if ( VTS->hBarWnd != NULL ){
		if ( VTS->flags & VFSTREE_PPC ){ // 上側にツールバー
			SetWindowPos(VTS->hBarWnd, NULL,
					0, box.top, box.right, VTS->BarSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);
			if ( VTS->BarSize.cy < box.bottom ){
				box.top += VTS->BarSize.cy;
				box.bottom -= VTS->BarSize.cy;
			}
		}else{ // 左側にツールバー
			SetWindowPos(VTS->hBarWnd, NULL,
					0, box.top, VTS->BarSize.cy, box.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
			if ( VTS->BarSize.cy < box.right ){
				box.left += VTS->BarSize.cy;
				box.right -= VTS->BarSize.cy;
			}
		}
	}
	SetWindowPos(VTS->hTViewWnd, NULL,
			box.left, box.top, box.right, box.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
}

void VfsTreeSetFlagFix(VFSTREESTRUCT *VTS)
{
	VTS->X_tree = GetCustDword(StrX_tree, XTREE_DEFAULT);
	if ( VTS->X_tree & XTREE_SHOWTOOLBAR ){
		if ( VTS->hBarWnd == NULL ){
			UINT ID = IDW_INTERNALMIN;

			VTS->hBarWnd = CreateToolBar(&VTS->cmdwork, VTS->vtinfo.hWnd, &ID,
					T("B_tree"), DLLpath,
					((hWndBackBrush != NULL) ? TBSTYLE_CUSTOMERASE : 0) |
					((VTS->flags & VFSTREE_PPC) ? 0 : TBSTYLE_WRAPABLE)  );
			if ( VTS->hBarWnd != NULL ){
				RECT box;

				if ( VTS->flags & VFSTREE_PPC ){
					GetWindowRect(VTS->hBarWnd, &box);
					VTS->BarSize.cy = box.bottom - box.top;
				}else{
					SendMessage(VTS->hBarWnd,
							TB_SETROWS, TMAKEWPARAM(500, TRUE), (LPARAM)&box);
					VTS->BarSize.cy = box.right - box.left;
					FixTreeSize(VTS);
				}
			}
		}
	}else{
		if ( VTS->hBarWnd != NULL ){
			DestroyWindow(VTS->hBarWnd);
			VTS->hBarWnd = NULL;
//			FixTreeSize(VTS);
		}
			FixTreeSize(VTS); // *entrytip で大きさ調整されないことある
	}

	if ( VTS->flags & VFSTREE_PPC ) SetPPcColor(VTS->hTViewWnd);
	if ( VTS->flags & VFSTREE_SPLITR ) FixTreeSize(VTS);

	if ( IsExistCustData( (VTS->flags & VFSTREE_PPC) ?
			T("KC_tree") : T("K_tree")) ){ //TreeKeyカスタマイズ有り
		if ( VTS->OldTreeWndProc == NULL ){
			SetProp(VTS->hTViewWnd, StrTreeProp, (HANDLE)VTS);
			VTS->OldTreeWndProc = (WNDPROC)SetWindowLongPtr(
					VTS->hTViewWnd, GWLP_WNDPROC, (LONG_PTR)TreeViewProc);
		}
	}
}

// VTM_POINTPATH メイン:指定されたスクリーン座標上のアイテムのフルパスを得る --
LRESULT GetPathPoint(VFSTREESTRUCT *VTS, VTMPOINTPATHSTRUCT *vpps)
{
	HTREEITEM hTitem;
	TV_HITTESTINFO hit;
	HWND hTWnd = VTS->hTViewWnd;

	if ( vpps == NULL ){ // 選択解除
		TREEVOID TreeView_SelectDropTarget(hTWnd, NULL);
		return VTS->TreeType;
	}

	hit.pt = vpps->pos;
	ScreenToClient(hTWnd, &hit.pt);
	hTitem = TreeView_HitTest(hTWnd, &hit);
	if ( hTitem == (HTREEITEM)vpps->Titem ) return VTS->TreeType; // 変化無し

	vpps->Titem = (DWORD_PTR)hTitem;
	if ( hTitem == NULL ) goto notarget;
	if ( !(hit.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT)) ) goto notarget;
	TREEVOID TreeView_SelectDropTarget(hTWnd, hTitem);
	if ( GetTreePath(VTS, hTitem, vpps->path) == FALSE ) vpps->Titem = 0;
	return VTS->TreeType;
notarget:
	TREEVOID TreeView_SelectDropTarget(hTWnd, NULL);
	vpps->Titem = 0;
	return VTS->TreeType;
}

// VTM_SCROLL メイン:マウスカーソル位置に応じたスクロールを行う ---------------
void ScrollTreeView(HWND hTWnd, HTREEITEM *hHitItem)
{
	HTREEITEM hTitem;
	TV_HITTESTINFO hit;
	RECT box;
	int x = 0, y = 0;

	GetClientRect(hTWnd, &box);
	GetCursorPos(&hit.pt);
	ScreenToClient(hTWnd, &hit.pt);

	if ( hit.pt.x < GRID ){
		x = (hit.pt.x / GRID) * 3 - 3;
	}else if ( hit.pt.x > ((box.right - box.left) - GRID) ){
		x = ((hit.pt.x - (box.right - box.left)) / GRID) * 3 + 3 ;
	}
	if ( x ){
		if ( x < 0 ){
			for ( ; x ; x++ ){
				SendMessage(hTWnd, WM_HSCROLL, (WPARAM)SB_LINELEFT, 0);
			}
		}else{
			for ( ; x ; x-- ){
				SendMessage(hTWnd, WM_HSCROLL, (WPARAM)SB_LINERIGHT, 0);
			}
		}
	}

	if ( hit.pt.y < GRID ){
		y = (hit.pt.y / GRID) * 3 - 1;
	}else if ( hit.pt.y > ((box.bottom - box.top) - GRID) ){
		y = ((hit.pt.y - (box.bottom - box.top)) / GRID) * 3 + 1 ;
	}

	if ( y ){
		hTitem = TreeView_GetFirstVisible(hTWnd);
		if ( y > 0 ){
			for ( ; y ; y-- ) hTitem = TreeView_GetNextVisible(hTWnd, hTitem);
		}else{
			for ( ; y ; y++ ) hTitem = TreeView_GetPrevVisible(hTWnd, hTitem);
		}
		TREEVOID TreeView_SelectSetFirstVisible(hTWnd, hTitem);
	}

	if ( !y && !x ){
		hTitem = TreeView_HitTest(hTWnd, &hit);
		if ( (hTitem == NULL) ||
		  !(hit.flags & (TVHT_ONITEMBUTTON | TVHT_ONITEM | TVHT_ONITEMRIGHT))){
			*hHitItem = NULL;
		}else if ( *hHitItem != hTitem ){
			*hHitItem = hTitem;
		}else{
			TreeView_Expand(hTWnd, hTitem,
				(hit.flags & TVHT_ONITEMBUTTON) ? TVE_TOGGLE : TVE_EXPAND);
			*hHitItem = NULL;
		}
	}
}
//-------------------------------------- カーソルが見えるように補正する
void FixTreeCaret(HWND hTWnd)
{
	HTREEITEM hTreeitem;

	hTreeitem = TreeView_GetSelection(hTWnd);
	if ( hTreeitem == NULL ) return;
	SendMessage(hTWnd, TVM_ENSUREVISIBLE, 0, (LPARAM)hTreeitem);
}

//-------------------------------------- アイテムを登録する
HTREEITEM AddItemToTree(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent)
{
	TV_ITEM tvi;
	TV_INSERTSTRUCT tvins;

	tvi.pszText = (TCHAR *)itemname;
	tvi.cchTextMax = tstrlen32(itemname);
	tvi.cChildren = 0x7fffffff;
	tvi.lParam = MAXLPARAM;

	tvi.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvi.iImage = 0; // folder
	tvi.iSelectedImage = 0; // folder

	tvins.hParent = hParent;
	tvins.hInsertAfter = TVI_SORT;
	TreeInsertItemValue(tvins) = tvi;

	return (HTREEITEM)SendMessage(VTS->hTViewWnd,
			TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
}

HTREEITEM AppendItemToTreeV(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent, int icontype, const TCHAR *datastr)
{
	TV_ITEM tvi;
	TV_INSERTSTRUCT tvins;

	tvi.pszText = (TCHAR *)itemname;
	tvi.cchTextMax = tstrlen32(itemname);
	tvi.cChildren = 0x7fffffff;

	if ( VTS->X_tree & XTREE_SHOWICON ){
		tvi.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.iImage = icontype; // folder
		tvi.iSelectedImage = icontype; // folder
	}else{
		tvi.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
	}
	if ( VTS->TreeType == TREETYPE_FOCUSPPC ) resetflag(tvi.mask, TVIF_CHILDREN);
	if ( datastr == NULL ){
		tvi.lParam = MAXLPARAM;
	}else{
		tvi.lParam = VTS->itemdata.top;
		ThAddString(&VTS->itemdata, datastr);
	}

	tvins.hParent = hParent;
	tvins.hInsertAfter = TVI_LAST;
	TreeInsertItemValue(tvins) = tvi;

	return (HTREEITEM)SendMessage(VTS->hTViewWnd,
			TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
}

void SeletTreeItem(VFSTREESTRUCT *VTS, HTREEITEM hItem)
{
	DWORD id = GetCurrentThreadId();

	// スレッドがメインスレッドか、最後に始めたスレッドでなければ選択しない
	if ( (id != VTS->MainThreadID) && (id != VTS->ActiveThreadID) ) return;

	SendMessage(VTS->hTViewWnd, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hItem);
}

//-------------------------------------- 指定したパスを強制追加する
void AddItemToTreeForce(VFSTREESTRUCT *VTS, HTREEITEM hItem, const TCHAR *path, BOOL focusset)
{
	TCHAR *p, *np, buf[VFPS], buf2[VFPS];

	if ( hItem == NULL ) hItem = TVI_ROOT;
	tstrcpy(buf, path);
	p = buf;

	for ( ; ; ){
		np = FindPathSeparator(p);
		if ( np == NULL ){ // 末尾
			if ( *p != '\0' ) hItem = AddItemToTree(VTS, p, hItem);
			if ( IsTrue(focusset) ) SeletTreeItem(VTS, hItem);
			return;
		}
		if ( (hItem == TVI_ROOT) && (*np == '/') ){ // http://xxx や aux://xxx
			while ( *np == '/' ) np++; // "//" をスキップ
			while ( (*np != '\0') && (*np != '/') ) np++; // "xxx" をスキップ
		}
		*np = '\0';
		if ( *p != '\0' ){
			if ( (hItem == TVI_ROOT) && (Isalpha(*p)) && (*(p + 1) == ':') &&
				 (GetCustDword(StrX_dlf, 0) & XDLF_DISPDRIVETITLE) ){
				buf2[0] = *p;
				buf2[1] = ':';
				buf2[2] = '>';
				buf2[3] = ' ';
				GetDriveNameTitle(buf2 + 4, buf2[0]);
				p = buf2;
			}
			hItem = AddItemToTree(VTS, p, hItem);
		}
		p = np + 1;
	}
}

HICON LoadTreeIcon(VFSTREESTRUCT *VTS, const TCHAR *path, const TCHAR *name)
{
	TCHAR buf[VFPS];

	// ファイルに応じたアイコンを取得する処理
	if ( path[0] == '#' ){
		XLPEXTRACTICON pEI;

		pEI = GetPathInterface(GetFocus(), name, &IID_IExtractIcon, path);
		if ( pEI != NULL ){
			HICON hIcon = NULL;
			TCHAR iconname[MAX_PATH];
			int index;
			UINT flags;

			pEI->lpVtbl->GetIconLocation(pEI, 0, iconname, MAX_PATH, &index, &flags);
			if ( flags & GIL_NOTFILENAME ){
				if ( OSver.dwMajorVersion < 6 ){
					HICON hSmallIcon;

					if ( SUCCEEDED(pEI->lpVtbl->Extract(pEI, iconname, index,
							&hIcon, &hSmallIcon, TreeIconSize) ) ){
						DestroyIcon(hSmallIcon);
					}
				}else{
					pEI->lpVtbl->Extract(pEI, iconname, index,
							&hIcon, NULL, TreeIconSize);
				}
			}else{
				ExtractIconEx(iconname, index, NULL, &hIcon, 1);
			}
			pEI->lpVtbl->Release(pEI);
			if ( hIcon != NULL ) return hIcon;
		}
	}else{
		SHFILEINFO shfinfo;
		DWORD_PTR result;

		if ( VFSFullPath(buf, (TCHAR *)name, path) == NULL ) return NULL;
		// C6001OK. SHGFI_ATTR_SPECIFIED がなければ[in]ではない
		result = SHGetFileInfo(buf, 0, &shfinfo, sizeof(shfinfo),
				( VTS->X_tree & XTREE_DISABLEOVERLAY ) ?
				XTREE_DISABLEOVERLAY : (TreeIconSHflag | SHGFI_ADDOVERLAYS) );
		if ( result != 0 ) return shfinfo.hIcon;
	}
	return NULL;
}

//-------------------------------------- 現在パスに関するアイテム登録
// 関係あるパスだったら 1、そうでなければ 0
int AddItemToTreePath(VFSTREESTRUCT *VTS, const TCHAR *itemname, HTREEITEM hParent, const TCHAR *path)
{
	HTREEITEM hChild;
	size_t size;
	TCHAR *p;

	if ( !(VTS->X_tree & XTREE_SHOWICON) ){
		hChild = AddItemToTree(VTS, itemname, hParent);
	}else{								// アイコン表示有り
		if ( (hParent == TVI_ROOT) && (itemname[1] == ':') &&
			 ((itemname[2] == '\0') || (itemname[2] == '>') ||
			  ((itemname[2] == '\\') && (itemname[3] == '\0'))) ){
			HIMAGELIST hSysImage;
			SHFILEINFO shfinfo;
			TCHAR drivename[8];
			int icon;

			icon = 0;
			wsprintf(drivename, T("%c:\\"), itemname[0]);

			// C6001OK. SHGFI_ATTR_SPECIFIED がなければ[in]ではない
			hSysImage = (HIMAGELIST)SHGetFileInfo(drivename, 0, &shfinfo,
					sizeof(shfinfo), TreeIconSHflag);
			if ( hSysImage != NULL ){
				icon = DImageList_AddIcon(VTS->hImage, shfinfo.hIcon);
				DestroyIcon(shfinfo.hIcon);
			}
			hChild = AppendItemToTreeV(VTS, itemname, hParent, icon, NULL);
		}else if ( (VTS->X_tree & XTREE_SIMPLEICON) ){
			hChild = AddItemToTree(VTS, itemname, hParent);
		}else{
			HICON hIcon;
			int icon = 0;
			TCHAR fullpath[VFPS];

			fullpath[0] = '\0';
			if ( hParent != TVI_ROOT ) GetTreePath(VTS, hParent, fullpath);
			hIcon = LoadTreeIcon(VTS, fullpath, itemname);
			if ( hIcon != NULL ){
				icon = DImageList_AddIcon(VTS->hImage, hIcon);
				DestroyIcon(hIcon);
			}
			hChild = AppendItemToTreeV(VTS, itemname, hParent, icon, NULL);
		}
	}
	if ( path == NULL ) return 0;

	size = tstrlen(itemname);
	p = tstrchr(itemname, '>');
	if ( p != NULL ) size = p - itemname;

	if ( tstrnicmp(itemname, path, size) ) return 0; // 無関係だった

	path += size;
		 // 途中まで文字列が一致していても無関係
	if ( ((*itemname != '\\')&&(*itemname != '>')) && (*path != '\\') && (*path != '\0') ) return 0;
	if ( (*path == '\0') || ((*path == '\\') && (*(path + 1) == '\0'))  ){
		DEBUGLOGF("Tree select(AddItemToTreePath) %s", itemname);
		SeletTreeItem(VTS, hChild);
		return 1;
	}
	if ( *path != '\\' ){
		if (hParent != TVI_ROOT) return 1;
	}else{
		path++;
	}
	DEBUGLOGF("Tree Expand(AddItemToTreePath) %s", itemname);

	ExpandTree(VTS, hChild, path); // 下層を展開する
	return 1;
}

//-------------------------------------- 現在のアイテムからパスを得る
BOOL GetTreePath(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem, TCHAR *buf)
{
	TV_ITEM tvi;
	TCHAR tvibuf[MAX_PATH];
	HTREEITEM hTreeNextitem;

	tvi.mask = TVIF_TEXT | TVIF_PARAM;
	tvi.hItem = hTreeitem;
	tvi.pszText = tvibuf;
	tvi.cchTextMax = TSIZEOF(tvibuf);
	TreeView_GetItem(VTS->hTViewWnd, &tvi);

	if ( VTS->TreeType == TREETYPE_PPCLIST ){
		if ( tvi.lParam == MAXLPARAM ) return FALSE;
		tstrlimcpy(buf, tvibuf, VFPS);
		return TRUE;
	}

	if ( ((DWORD_PTR)tvi.lParam < (DWORD_PTR)VTS->itemdata.size) ){
		TCHAR *path;

		path = ThPointerT(&VTS->itemdata, tvi.lParam);
		if ( (*path == '\0') || (*path == '%') ) return FALSE;

		if ( VTS->TreeType == TREETYPE_FOCUSPPC ){
			wsprintf(tvibuf, T("*focus C%s"), path);
			PP_ExtractMacro(VTS->vtinfo.hWnd, &VTS->vtinfo, NULL, tvibuf, NULL, 0);
			return FALSE;
		}

		VFSFullPath(buf, path, NULL);
		return TRUE;
	}

	hTreeNextitem = TreeView_GetParent(VTS->hTViewWnd, hTreeitem);
	if ( hTreeNextitem == NULL ){ // Root ?
		TCHAR *p;

		p = tstrchr(tvibuf, '>');
		if ( p != NULL ) *p = '\0';
		if ( tstrchr(tvibuf, '/') == NULL ){
			CatPath(buf, tvibuf, NilStr);
		}else{
			tstrcpy(buf, tvibuf);
			tstrcat(buf, T("/"));
		}
		return TRUE;
	}
	if ( GetTreePath(VTS, hTreeNextitem, buf) == FALSE ) return FALSE;
	if ( tvi.pszText[0] == '\\' ){ // UNC 先頭
		tstrlimcpy(buf, tvi.pszText, VFPS);
	}else{
		if ( tstrchr(buf, '/') == NULL ){
			CatPath(NULL, buf, tvi.pszText);
		}else{
			TCHAR *p;

			p = tstrrchr(buf, '/');
			if ( (p != NULL) && (*(p + 1) != '\0') ) tstrcat(p + 1, T("/"));
			tstrcat(buf, tvi.pszText);
		}
	}
	return TRUE;
}

void TreeItemDelete(VFSTREESTRUCT *VTS, WORD key, HTREEITEM hTreeitem)
{
	DWORD X_wdel[4] = X_wdel_default;
	int result;
	TCHAR buf[CMDLINESIZE];
	TV_ITEM tvi;
	TCHAR tvibuf[VFPS];

	if ( (VTS->TreeType != TREETYPE_FAVORITE) &&
		 (VTS->TreeType != TREETYPE_DIRECTORY) ){
		return; // 対応していない
	}
	if ( hTreeitem == NULL ) return;

	if ( VTS->TreeType == TREETYPE_DIRECTORY ){
		if ( TreeView_GetParent(VTS->hTViewWnd, hTreeitem) == NULL ){
			return; // : / \\ / drive: は削除できない
		}
	}
	GetCustData(T("X_wdel"), &X_wdel, sizeof(X_wdel));

	tvi.mask = TVIF_TEXT;
	tvi.hItem = hTreeitem;
	tvi.pszText = tvibuf;
	tvi.cchTextMax = TSIZEOF(tvibuf);
	TreeView_GetItem(VTS->hTViewWnd, &tvi);

	wsprintf(buf, T("%s %s"), tvibuf,
			MessageText((VTS->TreeType == TREETYPE_DIRECTORY) ?
			MES_QDL1 : MES_QDL4));
	result = PMessageBox(VTS->hTViewWnd, buf, MES_TDEL, treedeletemesboxstyle[X_wdel[2]]);
	if ( (result != IDOK) && (result != IDYES) ) return;

	switch ( VTS->TreeType ){
		case TREETYPE_DIRECTORY: {
			DELETESTATUS dstat;

			if ( key & K_s ){
				if ( GetTreePath(VTS, hTreeitem, buf) == FALSE ) return;

				dstat.OldTime = GetTickCount();
				dstat.count = 0;
				dstat.useaction = 0;
				dstat.noempty = FALSE;
				dstat.flags = X_wdel[0];
				dstat.warnattr = X_wdel[1];
				dstat.info = &VTS->vtinfo;

				if ( VFSDeleteEntry(&dstat, buf, GetFileAttributes(buf)) != NO_ERROR){
					return;
				}
			}else{
				TreeContextMenu(VTS, T("delete"), key);
			}
			break;
		}

		case TREETYPE_FAVORITE: {
			int size, index;
			TCHAR custname[MAX_PATH], keyword[MAX_PATH];

			index = FindCustTarget(VTS, hTreeitem, tvibuf, custname, keyword, buf, &size);
			if ( index < 0 ){
				XMessage(VTS->hTViewWnd, NULL, XM_GrERRld, T("Can't delete"));
				return;
			}
			DeleteCustTable(custname, NULL, index);
			if ( (buf[0] == '%') && (buf[1] == 'M') ){
				wsprintf(tvibuf, T("%s %s"), buf + 1, MessageText(MES_QDL1));
				result = PMessageBox(VTS->hTViewWnd, tvibuf, MES_TDEL, treedeletemesboxstyle[X_wdel[2]]);
				if ( (result == IDOK) || (result == IDYES) ){
					DeleteCustData(buf + 1);
				}
			}
			break;
		}

		default:
			return;
	}
	// 成功したので後処理をする
	TreeView_DeleteItem(VTS->hTViewWnd, hTreeitem);
	return;
}

#define IsHidden(ff) (!(VTS->X_tree & XTREE_SHOWSUPERHIDEEN) && ((ff.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)) == (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))

const TCHAR LoadingStr[] = T(" loading... ");
HTREEITEM SetLoadingItem(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem)
{
	HTREEITEM hShowBusyItem;
	TV_INSERTSTRUCT tvins;
	TV_ITEM tvi;

	tvi.mask = TVIF_TEXT | TVIF_PARAM;
	tvi.pszText = (TCHAR *)LoadingStr;
	tvi.cchTextMax = tstrlen32(LoadingStr);
	tvi.lParam = MAXLPARAM;
	tvins.hParent = hTreeitem;
	tvins.hInsertAfter = TVI_SORT;
	TreeInsertItemValue(tvins) = tvi;
	hShowBusyItem = (HTREEITEM)SendMessage(VTS->hTViewWnd,
			TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);

	tvi.mask = TVIF_CHILDREN;
	tvi.hItem = hTreeitem;
	tvi.cChildren = 1;
	TreeView_SetItem(VTS->hTViewWnd, &tvi);
	TreeView_Expand(VTS->hTViewWnd, hTreeitem, TVE_EXPAND);
	return hShowBusyItem;
}
//-------------------------------------- 指定アイテムの下層を展開
BOOL ExpandTree(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem, const TCHAR *pathptr)
{
	TCHAR path[VFPS];
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	DWORD items = 0;
	TV_ITEM tvi;
	HWND hTWnd = VTS->hTViewWnd;
	int expanditem = 0; // 既に下層を展開済みなら 0 以外
	HTREEITEM hShowBusyItem = NULL;
										// 展開済みなら終了
	if ( TreeView_GetChild(hTWnd, hTreeitem) != NULL ) return TRUE;
										// パス取得
	if ( FALSE == GetTreePath(VTS, hTreeitem, path) ) return TRUE;
	if ( (tstrchr(path, '/') != NULL) && (pathptr != NULL) ){
		 // http:// ftp:// aux:// 等は列挙しない
		 AddItemToTreeForce(VTS, hTreeitem, pathptr, TRUE);
		 return TRUE;
	}
	CatPath(NULL, path, WildCard_All);

	// 手動展開＋時間が掛かりそうなパスなら、処理中表示を行う
	if ( (pathptr == NULL) &&
		 ((path[0] == '\\') || (path[0] == '#')) ){
		hShowBusyItem = SetLoadingItem(VTS, hTreeitem);
	}
										// 列挙
	hFF = VFSFindFirst(path, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) goto error;	// 失敗
	if ( pathptr == NULL ) expanditem = 1; // 下層はない
	for ( ; ; ){
		ERRORCODE errorcode;
		int chance;

		if ( VTS->ActiveThreadID == 0 ) break; // 中断要求有り
		if ( !IsRelativeDirectory(ff.cFileName) && !IsHidden(ff) &&
			 (ff.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER)) ){
			if ( expanditem ){
				if ( !(VTS->X_tree & XTREE_SIMPLEICON) ){
					AddItemToTreePath(VTS, ff.cFileName, hTreeitem, NULL);
				}else{
					AddItemToTree(VTS, ff.cFileName, hTreeitem);
				}
			}else{
				expanditem |=
					AddItemToTreePath(VTS, ff.cFileName, hTreeitem, pathptr);
			}
			items++;
		}

		if ( IsTrue(VFSFindNext(hFF, &ff)) ) continue;
		errorcode = GetLastError();
		chance = 10;
		while ( errorcode == ERROR_MORE_DATA ){
			Sleep(100);
			if ( IsTrue(VFSFindNext(hFF, &ff)) ){
				errorcode = NO_ERROR;
				break;
			}
			errorcode = GetLastError();
			chance--;
			if ( !chance ) break;
		}
		if ( errorcode != NO_ERROR ) break;
	}
	VFSFindClose(hFF);

	if ( (expanditem == 0) && (VTS->TreeType == TREETYPE_DIRECTORY) ){
										// 該当エントリがなかったので追加する
		AddItemToTreeForce(VTS, hTreeitem, pathptr, TRUE);
		items++;
	}
										// 「[+]」表示の再設定
	tvi.mask = TVIF_CHILDREN;
	tvi.hItem = hTreeitem;
	tvi.cChildren = items;
	TreeView_SetItem(hTWnd, &tvi);

	if ( pathptr == NULL ){
		if ( hShowBusyItem != NULL ){ // 処理中表示を削除
			TreeView_DeleteItem(VTS->hTViewWnd, hShowBusyItem);
		}
		TreeView_Expand(VTS->hTViewWnd, hTreeitem, TVE_EXPAND);
	}
	return TRUE;

error:
	if ( hShowBusyItem != NULL ){ // 処理中表示を削除
		TreeView_DeleteItem(VTS->hTViewWnd, hShowBusyItem);
	}
	return FALSE;
}

typedef struct {
	VFSTREESTRUCT *VTS;
	HTREEITEM hTreeitem;
} ExpandTree_Thread_Struct;

DWORD WINAPI ExpandTree_Thread(ExpandTree_Thread_Struct *ETTS)
{
	THREADSTRUCT threadstruct = {T("ExpandTree"), XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};

	PPxRegisterThread(&threadstruct);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	WaitTreeViewThread(ETTS->VTS, TRUE);

	ETTS->VTS->ThreadCount++;
	ExpandTree(ETTS->VTS, ETTS->hTreeitem, NULL);
	ETTS->VTS->ExpandCount--;
	if ( --ETTS->VTS->ThreadCount == 0 ) FreeVTS(ETTS->VTS);
	HeapFree(DLLheap, 0, ETTS);
	CoUninitialize();
	PPxUnRegisterThread();
	return 0;
}

void Start_ExpandTree(VFSTREESTRUCT *VTS, HTREEITEM hTreeitem)
{
	ExpandTree_Thread_Struct *ETTS;

	if ( VTS->ExpandCount > 0 ) return; // 既に実行中

	if ( VTS->ThreadCount > MaxTreeThreads ) return; // スレッドが多すぎるので中止する

	if ( (ETTS = HeapAlloc(DLLheap, 0, sizeof(ExpandTree_Thread_Struct))) == NULL ){
		return;
	}
	VTS->ExpandCount++;
	ETTS->VTS = VTS;
	ETTS->hTreeitem = hTreeitem;

	#ifndef WINEGCC
	if ( ((VTS->TreeType == TREETYPE_DIRECTORY) ||
		  (VTS->TreeType == TREETYPE_VFS)			)
	#ifndef _WIN64
		  && ( OSver.dwMajorVersion >= 5 ) // 次の行に続く
	#endif
	){
		HANDLE hThread;

		hThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)ExpandTree_Thread, ETTS, 0, (DWORD *)&VTS->ActiveThreadID);
		if ( hThread != NULL ){
			SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
			CloseHandle(hThread);
			return;
		}
	}
	#endif
	ExpandTree(VTS, hTreeitem, NULL);
	VTS->ExpandCount--;
	HeapFree(DLLheap, 0, ETTS);
}

//-------------------------------------- 窓の幅を調節する
void ResizeTreeWindow(VFSTREESTRUCT *VTS, int sizeX, int sizeY)
{
	RECT box;
	HWND hWnd;

	if ( VTS->flags & VFSTREE_PPC ){
		if ( sizeY != 0 ){
			PostMessage(VTS->hNotifyWnd, WM_PPXCOMMAND, (sizeY < 0) ?
					K_raw | K_s | K_a | K_up : K_raw | K_s | K_a | K_dw, 0);
			return;
		}
		hWnd = VTS->vtinfo.hWnd;
	}else if ( VTS->flags & VFSTREE_MENU ){
		hWnd = VTS->vtinfo.hWnd;
	}else{
		hWnd = GetParentCaptionWindow(VTS->vtinfo.hWnd);
	}

	GetWindowRect(hWnd, &box);

	box.right = box.right - box.left + sizeX;
	box.bottom = box.bottom - box.top + sizeY;
	if ( box.right < 8 ) box.right = 8;
	if ( box.bottom < 8 ) box.bottom = 8;
	SetWindowPos(hWnd, NULL, 0, 0, box.right, box.bottom, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
	return;
}
//-------------------------------------- 選択アイテムのパスを得る
BOOL GetSelectedTreePath(VFSTREESTRUCT *VTS, TCHAR *path)
{
	HTREEITEM hTreeitem;

	hTreeitem = TreeView_GetSelection(VTS->hTViewWnd);
	if ( hTreeitem == NULL ) return FALSE;
	return GetTreePath(VTS, hTreeitem, path);
}

void TreeContextMenu(VFSTREESTRUCT *VTS, const TCHAR *cmd, WORD key)
{
	TCHAR path[VFPS];
	POINT pos;

	if ( GetSelectedTreePath(VTS, path) == FALSE ){
		TreeTypeMenu(VTS, INVALID_HANDLE_VALUE, key);
	}
	GetPopMenuPos(VTS->hTViewWnd, &pos, key);
	VFSSHContextMenu(VTS->vtinfo.hWnd, &pos, path, T("."), cmd);
}

void TreeSendKey(VFSTREESTRUCT *VTS, WORD key)
{
	CallWindowProc(VTS->OldTreeWndProc, VTS->hTViewWnd, WM_KEYDOWN, key & 0xff, 0);
	CallWindowProc(VTS->OldTreeWndProc, VTS->hTViewWnd, WM_KEYUP, key & 0xff, 0);
}

void RemoveSendKey(void)
{
	MSG msg;

	PeekMessage(&msg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE);
}

//-------------------------------------- キー操作処理メイン
ERRORCODE TreeKeyCommand(VFSTREESTRUCT *VTS, WORD key)
{
	TCHAR path[VFPS];
	TCHAR buf[CMDLINESIZE];

	if ( !(key & K_raw) ){
		PutKeyCode(buf, key);

		if ( NO_ERROR == GetCustTable( (VTS->flags & VFSTREE_PPC) ?
				T("KC_tree") : T("K_tree"), buf, buf, sizeof(buf)) ){
			WORD *keyp, keypre;

			if ( (UTCHAR)buf[0] == EXTCMD_CMD ){
				PP_ExtractMacro(VTS->vtinfo.hWnd, &VTS->vtinfo, NULL, buf + 1, NULL, 0);
				return 0;
			}
			keypre = (WORD)K_raw | (key & (WORD)K_mouse);
			keyp = (WORD *)(((UTCHAR)buf[0] == EXTCMD_KEY) ? (buf + 1) : buf);
			key = *keyp;
			if ( key == 0 ) return 0;
			key |= keypre;
			while( *(++keyp) != 0 ){
				if ( TreeKeyCommand(VTS, key) ){
					if ( VTS->OldTreeWndProc != NULL ) TreeSendKey(VTS, key);
				}
				key = *keyp | keypre;
			}
			if ( VTS->OldTreeWndProc != NULL ){
				if ( TreeKeyCommand(VTS, key) ) TreeSendKey(VTS, key);
				return 0;
			}
			if ( !(key & K_raw) ){
				TreeKeyCommand(VTS, key);
				return 0;
			}
		}
	}

	switch ( key & ~(K_v | K_raw | K_mouse) ){ // エイリアスビットを無効にする
		case VK_F5:
			Start_InitTreeViewItems(VTS, NULL);
			break;

		case K_s | VK_DELETE:
		case VK_DELETE:
			TreeItemDelete(VTS, key, TreeView_GetSelection(VTS->hTViewWnd));
			break;

		case VK_F2: {
			HTREEITEM hTreeitem;

			hTreeitem = TreeView_GetSelection(VTS->hTViewWnd);
			if ( hTreeitem != NULL ){
				TV_ITEM tvi;

				tvi.mask = TVIF_TEXT;
				tvi.hItem = hTreeitem;
				tvi.pszText = buf;
				tvi.cchTextMax = TSIZEOF(buf);
				TreeView_GetItem(VTS->hTViewWnd, &tvi);
				if ( StartTreeRename(VTS, hTreeitem, buf) == 0 ){
					SendMessage(VTS->hTViewWnd, TVM_EDITLABEL, 0, (LPARAM)hTreeitem);
				}
			}
			break;
		}

		case VK_ESCAPE:
			if ( VTS->hNotifyWnd != NULL ){
				SendMessage(VTS->hNotifyWnd, WM_PPXCOMMAND, KTN_escape, 0);
			}
			if ( VTS->flags & VFSTREE_MENU ){
				PostMessage(VTS->vtinfo.hWnd, WM_CLOSE, 0, 0);
			}
			break;

		case VK_TAB:
			if ( VTS->hNotifyWnd != NULL ){
				SendMessage(VTS->hNotifyWnd, WM_PPXCOMMAND, KTN_focus, 0);
				PeekMessage((MSG *)path, VTS->hTViewWnd, WM_CHAR, WM_CHAR, PM_REMOVE); // [tab] の WM_CHAR を除去
			}
			break;

		case K_a | K_s | VK_LEFT:
		case K_c | K_s | VK_LEFT:
			ResizeTreeWindow(VTS, -8, 0);
			break;

		case K_a | K_s | VK_RIGHT:
		case K_c | K_s | VK_RIGHT:
			ResizeTreeWindow(VTS, +8, 0);
			break;

		case K_a | K_s | VK_UP:
		case K_c | K_s | VK_UP:
			ResizeTreeWindow(VTS, 0, -8);
			break;

		case K_a | K_s | VK_DOWN:
		case K_c | K_s | VK_DOWN:
			ResizeTreeWindow(VTS, 0, +8);
			break;

		case K_c | 'T':
			RemoveSendKey();
			TreeTypeMenu(VTS, INVALID_HANDLE_VALUE, key);
			break;

		case K_c | 'C':
			TreeContextMenu(VTS, T("copy"), key);
			break;

		case K_c | 'X':
			TreeContextMenu(VTS, T("cut"), key);
			break;

		case K_c | 'V':
			TreeContextMenu(VTS, T("paste"), key);
			break;

		case K_a | VK_UP:
		case K_a | VK_DOWN:
		case K_a | VK_LEFT:
		case K_a | VK_RIGHT:
			PPxCommonCommand(VTS->vtinfo.hWnd, 0, key);
			break;

		case K_s | VK_F10:
		case VK_APPS:
		case K_c | VK_RETURN:
			TreeContextMenu(VTS, NULL, key);
			break;

		case K_a | VK_RETURN:
			TreeContextMenu(VTS, T("properties"), key);
			break;

		case VK_RETURN:
			if ( VTS->hNotifyWnd != NULL ){
				if ( GetSelectedTreePath(VTS, path) == FALSE ) break;
				SendMessage(VTS->hNotifyWnd,
						WM_PPXCOMMAND, KTN_selected, (LPARAM)path);
				PeekMessage((MSG *)path, VTS->hTViewWnd, WM_CHAR, WM_CHAR, PM_REMOVE); // [enter] の WM_CHAR を除去
			}
			if ( VTS->resultStrPtr != NULL ){
				if ( GetSelectedTreePath(VTS, path) == FALSE ) break;
				tstrcpy(VTS->resultStrPtr, path);
			}
			if ( VTS->resultCodePtr != NULL ){
				*VTS->resultCodePtr = NO_ERROR;
				VTS->resultCodePtr = NULL;
			}
			if ( VTS->flags & VFSTREE_MENU ){
				PostMessage(VTS->vtinfo.hWnd, WM_CLOSE, 0, 0);
			}
			break;

			// default へ

		default:
			return ERROR_INVALID_FUNCTION;
	}
	return NO_ERROR;
}

void TreeRightContext(VFSTREESTRUCT *VTS)
{
	TV_HITTESTINFO hit;
	HTREEITEM hTreeitem;
	TCHAR path[VFPS];
	POINT pos;
	HWND hTWnd = VTS->hTViewWnd;

	GetPopMenuPos(hTWnd, &hit.pt, K_mouse);
	ScreenToClient(hTWnd, &hit.pt);
	hTreeitem = TreeView_HitTest(hTWnd, &hit);
	if ( (hTreeitem != NULL) &&
		 (FALSE != GetTreePath(VTS, hTreeitem, path)) &&
		 (hit.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT)) &&
		 !((VTS->TreeType == TREETYPE_FAVORITE) && (TreeView_GetParent(VTS->hTViewWnd, hTreeitem) == NULL)) ){
		GetPopMenuPos(hTWnd, &pos, K_mouse);
		VFSSHContextMenu(hTWnd, &pos, path, T("."), NULL);
	}else{
		TreeTypeMenu(VTS, hTreeitem, K_mouse);
	}
}

void TreeDragDrop(VFSTREESTRUCT *VTS, HTREEITEM hTitem)
{
	TCHAR path[VFPS];

	if ( VTS->flags & VFSTREE_DISABLEDRAG ) return;
	if ( FALSE == GetTreePath(VTS, hTitem, path) ) return;
	DragDropPath(VTS->hTViewWnd, path);
}

void AddTreeDriveRoot(VFSTREESTRUCT *VTS, int driveno, const TCHAR *selectpath)
{
	TCHAR name[VFPS];

	name[0] = (TCHAR)('A' + driveno);
	name[1] = ':';
	if ( X_dlf_ddt ){
		name[2] = '>';
		name[3] = ' ';
		GetDriveNameTitle(name + 4, name[0]);
	}else{
		name[2] = '\0';
	}
	AddItemToTreePath(VTS, name, TVI_ROOT, selectpath);
}

// ドライブが増えた／減ったときの処理
void WmTreeDEVICECHANGE(VFSTREESTRUCT *VTS, WPARAM type, LPARAM lParam)
{
	if ( ((type == DBT_DEVICEARRIVAL) || (type == DBT_DEVICEREMOVECOMPLETE)) &&
		(((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype == DBT_DEVTYP_VOLUME) ){
		int driveno, driveflags;

		driveflags = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
		for ( driveno = 0 ; driveno < 26 ; driveno++ ){
			if ( driveflags & LSBIT ){
				HTREEITEM hTreeitem;

				// ドライブ登録済みかを確認
				hTreeitem = TreeView_GetNextItem(VTS->hTViewWnd, NULL, TVGN_ROOT);
				while ( hTreeitem != NULL ){
					TV_ITEM tvi;
					TCHAR tvibuf[MAX_PATH];

					tvi.mask = TVIF_TEXT;
					tvi.hItem = hTreeitem;
					tvi.pszText = tvibuf;
					tvi.cchTextMax = TSIZEOF(tvibuf);
					TreeView_GetItem(VTS->hTViewWnd, &tvi);
					if ( (tvibuf[1] == ':') && (tvibuf[0] == (TCHAR)('A' + driveno)) ){
						TreeView_DeleteItem(VTS->hTViewWnd, hTreeitem);
						break;
					}
					hTreeitem = TreeView_GetNextItem(VTS->hTViewWnd, hTreeitem, TVGN_NEXT);
				}
				if ( type == DBT_DEVICEARRIVAL ){
					AddTreeDriveRoot(VTS, driveno, NULL); // 追加
				}
			}
			driveflags >>= 1;
		}
	}
}

void PPtReceiveChangeNotify(HWND hWnd, HANDLE hChange, DWORD dwProcId)
{
	LONG EventType;
	HANDLE hNotifyLock;
	LPITEMIDLIST *pidl_list;
	LPMALLOC pMA;

	hNotifyLock =
			DSHChangeNotification_Lock(hChange, dwProcId, &pidl_list, &EventType);
	if ( hNotifyLock == NULL ) return;

	if ( (EventType != 0) && SUCCEEDED(SHGetMalloc(&pMA)) ){
		LPITEMIDLIST pidl = DupIdl(pMA, pidl_list[0]);

		if ( EventType & SHCNE_UPDATEDIR ){
			PostMessage(hWnd, VTM_I_CHANGEPATH_MAIN, (WPARAM)SHCNE_RMDIR, (LPARAM)DupIdl(pMA, pidl_list[0]));
			EventType = SHCNE_MKDIR;
		}else if ( EventType == SHCNE_RENAMEFOLDER ){
			PostMessage(hWnd, VTM_I_CHANGEPATH_MAIN, (WPARAM)SHCNE_MKDIR, (LPARAM)DupIdl(pMA, pidl_list[1]));
			EventType = SHCNE_RMDIR;
		}
		PostMessage(hWnd, VTM_I_CHANGEPATH_MAIN, (WPARAM)EventType, (LPARAM)pidl);
		pMA->lpVtbl->Release(pMA);
	}
	DSHChangeNotification_Unlock(hNotifyLock);
}

void PPtChangeNotify(VFSTREESTRUCT *VTS, DWORD EventType, LPITEMIDLIST pidl)
{
	LPMALLOC pMA;
	LPSHELLFOLDER piDesktop;
	TCHAR path[VFPS];
	HTREEITEM hTreeRoot;

	if ( FAILED(SHGetMalloc(&pMA)) ) return;

	WaitTreeViewThread(VTS, FALSE);

	hTreeRoot = TreeView_GetRoot(VTS->hTViewWnd);
	if ( hTreeRoot != NULL ){
			// 一般パスの調整
		if ( SHGetPathFromIDList(pidl, path) != FALSE ){
			SetPathItems(VTS, hTreeRoot, path, EventType);
		}else{
			// エクスプローラパスを調整
			SHGetDesktopFolder(&piDesktop);
			GetIDLSub(path, piDesktop, pidl);
			piDesktop->lpVtbl->Release(piDesktop);
			SetPathItems(VTS, hTreeRoot, path, EventType);
		}
	}
	pMA->lpVtbl->Free(pMA, pidl);
	pMA->lpVtbl->Release(pMA);
}

void FixTreeHeight(VFSTREESTRUCT *VTS)
{
	RECT ParentBox, TreeBox, ItemBox;

	GetWindowRect(VTS->vtinfo.hWnd, &ParentBox);
	GetClientRect(VTS->hTViewWnd, &TreeBox);

	*(HTREEITEM *)&ItemBox = TreeView_GetRoot(VTS->hTViewWnd);
	if ( *(HTREEITEM *)&ItemBox == NULL ) return;
	SendMessage(VTS->hTViewWnd, TVM_SELECTITEM, (WPARAM)TVGN_CARET, (LPARAM)*(HTREEITEM *)&ItemBox);
	SendMessage(VTS->hTViewWnd, TVM_GETITEMRECT, (WPARAM)FALSE, (LPARAM)&ItemBox);

	SetWindowPos(VTS->vtinfo.hWnd, NULL, 0, 0,
			ParentBox.right - ParentBox.left,
			(ParentBox.bottom - ParentBox.top) - // 全体の高さ
			 (TreeBox.bottom - TreeBox.top) + // 元の高さを除去
				(ItemBox.bottom - ItemBox.top) * // １行の高さ
				 (int)(LRESULT)SendMessage(VTS->hTViewWnd, TVM_GETCOUNT, 0, 0), // 全アイテム数
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
}

void PPtToolbarCommand(VFSTREESTRUCT *VTS, int id, int orcode)
{
	TCHAR *param;

	param = GetToolBarCmd(VTS->hBarWnd, &VTS->cmdwork, id);
	if ( param == NULL ) return;
	if ( orcode ){
		if ( orcode < 0x100 ){ // 右クリック
		}else{	// ドロップダウン
			WORD key;

			key = *(WORD *)(param + 1) | (WORD)orcode;
			TreeKeyCommand(VTS, key | (WORD)K_mouse);
		}
	}else{
		if ( (UTCHAR)*param == EXTCMD_CMD ){
			PP_ExtractMacro(VTS->vtinfo.hWnd, &VTS->vtinfo, NULL, param + 1, NULL, 0);
		}else{
			const WORD *key;

			if ( (UTCHAR)*param == EXTCMD_KEY ) param++;
			key = (WORD *)param;
			while( *key ) TreeKeyCommand(VTS, *key++ | (WORD)K_mouse);
		}
	}
}

void WmTreePaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	RECT box;

	GetClientRect(hWnd, &box);
	box.left = box.right - splitbarwide;
	BeginPaint(hWnd, &ps);
	if ( ps.rcPaint.right > box.left ) ps.rcPaint.right = box.left;
	if ( (ps.rcPaint.right - ps.rcPaint.left) > 0 ){
		FillBox(ps.hdc, &ps.rcPaint, Get3dFaceBrush());
	}
	DrawSeparateLine(ps.hdc, &box, BF_RIGHT | BF_MIDDLE); // BF_LEFT はなし
	EndPaint(hWnd, &ps);
}

//--------------------------------------
LRESULT CALLBACK TreeProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	VFSTREESTRUCT *VTS;

	VTS = (VFSTREESTRUCT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( VTS == NULL ){
		if ( message == WM_CREATE ){
			WMTreeCreate(hWnd);
			return 0;
		}else{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}else{
#if TreeCollapseCheck
		if ( (VTS->CollapseCheck != CollapseCheckID) &&
			 (VTS->CollapseID == 0) ){
			VTS->CollapseID = message | 0x200000;
		//	WMTreeDestroy_LOG(VTS);
		}
#endif

	switch ( message ){
		case WM_PAINT:
			WmTreePaint(hWnd);
			break;

		case WM_SETFOCUS:
			SetFocus(VTS->hTViewWnd);
			break;

		case WM_DESTROY:
			WMTreeDestroy(VTS);
			break;

		case WM_WINDOWPOSCHANGED:
			if ( VTS->hNotifyWnd != NULL ){
				SendMessage(VTS->hNotifyWnd, WM_PPXCOMMAND,
						KTN_size, ((WINDOWPOS *)lParam)->cx);
			}
			FixTreeSize(VTS);
			goto defchk;
//			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_COPYDATA:
			return PPWmCopyData(&VTS->vtinfo, (COPYDATASTRUCT *)lParam);

		case VTM_SETFLAG:
			VTS->flags = (DWORD)lParam;
			if ( VTS->flags & VFSTREE_MENU ){
				VTS->hParentWnd = (HWND)wParam;
				if ( VTS->hParentWnd != NULL ){
					EnableWindow(VTS->hParentWnd, FALSE);
				}
			}else{
				VTS->hNotifyWnd = (HWND)wParam;
			}
			VfsTreeSetFlagFix(VTS);
			break;

		case VTM_SETPATH:
			if ( VTS->ThreadCount > MaxTreeThreads ) break;
			if ( (VTS->TreeType != TREETYPE_PPCLIST) && (VTS->TreeType != TREETYPE_FOCUSPPC) ){
				Start_TreeSetDirPath(VTS, (TCHAR *)lParam);
			}else{
				TreeKeyCommand(VTS, K_raw | VK_F5);
			}
			FixTreeCaret(VTS->hTViewWnd);
			break;

		case VTM_INITTREE:
			Start_InitTreeViewItems(VTS, (TCHAR *)lParam);
			break;

		case VTM_SCROLL:
			ScrollTreeView(VTS->hTViewWnd, &VTS->hHitItem);
			break;

		case VTM_POINTPATH:
			return GetPathPoint(VTS, (VTMPOINTPATHSTRUCT *)lParam);

		case VTM_ADDTREEITEM:
			AddTreeItemFromList(VTS, (const TCHAR *)lParam);
			break;

		case VTM_GETSETTINGS: {
			TCHAR *param;

			param = (TCHAR *)lParam;
			switch ( VTS->TreeType ){
				case TREETYPE_FAVORITE:
					*param = '%';
					tstrcpy(param + 1, ThPointerT(&VTS->itemdata, 0) );
					break;
				case TREETYPE_VFS:
					*param = '*';
					tstrcpy(param + 1, ThPointerT(&VTS->itemdata, 0) );
					break;
				case TREETYPE_PPCLIST:
					tstrcpy(param, TreePPcListName);
					break;
				case TREETYPE_FOCUSPPC:
					tstrcpy(param, TreeFocusPPcName);
					break;
//				case TREETYPE_DIRECTORY:
				default:
					*param = '\0';
			}
			break;
		}

		case VTM_TREECOMMAND:
			TreeCommand(VTS, (const TCHAR *)lParam);
			break;

		case VTM_SETRESULT:
			if ( VTS->flags & VFSTREE_MENU ){
				VTS->resultCodePtr = (ERRORCODE *)wParam;
				VTS->resultStrPtr = (TCHAR *)lParam;
				FixTreeHeight(VTS);
			}
			break;

		case VTM_CHANGEDDISPDPI:
			TreeDpiChanged(VTS, wParam);
			break;

		case VTM_I_CHANGENOTIFY:
			if ( VTS->TreeType != TREETYPE_DIRECTORY ) break;
			PPtReceiveChangeNotify(hWnd, (HANDLE)wParam, (DWORD)lParam);
			break;

		case VTM_I_CHANGEPATH_MAIN:
			PPtChangeNotify(VTS, (DWORD)wParam, (LPITEMIDLIST)lParam);
			break;

		case WM_NCHITTEST:
			if ( VTS->flags & VFSTREE_SPLITR ){
				LRESULT lr;

				lr = DefWindowProc(hWnd, message, wParam, lParam);
				if ( lr == HTCLIENT ) lr = HTRIGHT;
				return lr;
			}
			goto defchk;
	//		return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_DEVICECHANGE:
			WmTreeDEVICECHANGE(VTS, wParam, lParam);
			goto defchk;
//			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)

			switch ( NHPTR->code ){
				case NM_CUSTOMDRAW:
					if ( hWndBackBrush != NULL ){
						NMCUSTOMDRAW *csd = (NMCUSTOMDRAW *)lParam;

						if ( csd->dwDrawStage == CDDS_PREERASE ){
							FillRect(csd->hdc, &csd->rc, hWndBackBrush);
							return CDRF_SKIPDEFAULT;
						}
					}
					return 0;

				case TTN_NEEDTEXT:
					SetToolBarTipText(VTS->hBarWnd, &VTS->cmdwork, NHPTR);
					break;

				case TVN_KEYDOWN:
					if ( VTS->OldTreeWndProc == NULL ){
						TreeKeyCommand(VTS, (WORD)K_v | (WORD)GetShiftKey() |
								((TV_KEYDOWN *)lParam)->wVKey );
					}
					break;

				case NM_DBLCLK:
					TreeKeyCommand(VTS, VK_RETURN | K_mouse);
					break;

				case NM_RCLICK:
					TreeRightContext(VTS);
					break;

				case TVN_BEGINDRAG:
				case TVN_BEGINRDRAG:
					TreeDragDrop(VTS, ((NM_TREEVIEW *)lParam)->itemNew.hItem);
					break;

				case TVN_ITEMEXPANDING:
					Start_ExpandTree(VTS, ((NM_TREEVIEW *)lParam)->itemNew.hItem);
					break;

				case TVN_SELCHANGED:
					if ( (((NM_TREEVIEW *)lParam)->itemNew.hItem != NULL) &&
							(VTS->hNotifyWnd != NULL) ){
						if ( (!(VTS->flags & VFSTREE_PPC) ||
							  (VTS->X_tree & XTREE_SYNCSELECT)) &&
								!(GetShiftKey() & K_s) ){
							TCHAR path[VFPS];

							if ( FALSE != GetTreePath(VTS, ((NM_TREEVIEW *)
									lParam)->itemNew.hItem, path) ){
								SendMessage(VTS->hNotifyWnd, WM_PPXCOMMAND,
										KTN_select, (LPARAM)path);
							}
						}
					}
					break;

				case TVN_BEGINLABELEDIT:
					if ( VTS->X_tree & XTREE_DIALOGRENAME ) return 1; // 不可
					return StartTreeRename(VTS,
							((TV_DISPINFO *)lParam)->item.hItem,
							((TV_DISPINFO *)lParam)->item.pszText);

				case TVN_ENDLABELEDIT:
					TreeItemEdit(VTS, (TV_DISPINFO *)lParam);
					break;

//				default:
			}
			break;

		case WM_COMMAND:
			if ( lParam != 0 ){
				if ( (HWND)lParam == VTS->hBarWnd ){
					PPtToolbarCommand(VTS, LOWORD(wParam), 0);
					break;
				}
			}
			break;

		case WM_CLOSE:
			if ( VTS->hParentWnd != NULL ){
				EnableWindow(VTS->hParentWnd, TRUE);
			}
		// default へ

		default:
defchk:
#if TreeCollapseCheck
			if ( (VTS->CollapseCheck != CollapseCheckID) &&
				 (VTS->CollapseID == 0) ){
				VTS->CollapseID = message | 0x400000;
			//	WMTreeDestroy_LOG(VTS);
			}

		{
			LRESULT lr = DefWindowProc(hWnd, message, wParam, lParam);

			if ( (VTS->CollapseCheck != CollapseCheckID) &&
				 (VTS->CollapseID == 0) ){
				VTS->CollapseID = message | 0x900000;
			//	WMTreeDestroy_LOG(VTS);
			}
			return lr;
		}
#else
			return DefWindowProc(hWnd, message, wParam, lParam);
#endif
	}}
#if TreeCollapseCheck
	if ( (VTS->CollapseCheck != CollapseCheckID) &&
		 (VTS->CollapseID == 0) ){
		VTS->CollapseID = message | 0x800000;
	//	WMTreeDestroy_LOG(VTS);
	}
#endif
	return 0;
}

int AddFavoriteItem(VFSTREESTRUCT *VTS, HTREEITEM hParentTree, const TCHAR *custname)
{
	const TCHAR *p;
	int count = 0;
	TCHAR keyword[CMDLINESIZE], param[CMDLINESIZE];

	if ( VTS->itemdata.top == 0 ) ThAddString(&VTS->itemdata, custname);
	while( EnumCustTable(count, custname, keyword, param, sizeof(param)) >= 0 ){
		count++;
		if ( !tstrcmp(keyword, T("||")) ){		// 改桁 ======================
			continue;
		}else if ( !tstrcmp(keyword, T("--")) ){	// セパレータ ================
			continue;
		}
		p = param;							// 階層メニュー ==============
		SkipSpace(&p);
		if ( *p == '\0' ) tstrcpy(param, keyword);
		if ( (*p == '%') && (*(p+1) == 'M') && (*(p+2) != 'E') ){
			HTREEITEM hChildTree;

			hChildTree = AppendItemToTreeV(VTS, keyword, hParentTree, 0, p);
			AddFavoriteItem(VTS, hChildTree, p + 1);
			continue;
		}

		if ( keyword[0] != '\0' ){
			if ( keyword[1] == '\0' ){	// キーワードが１文字の処理
				keyword[1] = ':';
				keyword[2] = ' ';
				tstrcpy(&keyword[3], param);
			}else{
				TCHAR *wp;

				for ( wp = keyword ; *wp ; wp++ ){
					if ( *wp != '\\' ) continue;
					wp++;
					if ( *wp == 't' ){
						*(wp - 1) = ' ';
						memmove(wp, wp + 1, TSTRLENGTH(wp));
						break;
					}
					if ( (*wp == '\\') && (*(wp+1) == 't') ){
						memmove(wp, wp + 1, TSTRLENGTH(wp));
						break;
					}
				}
			}
		}
		if ( keyword[0] ){
			PP_ExtractMacro(NULL, NULL, NULL, keyword, keyword, XEO_DISPONLY);
		}else{
			tstrcpy(keyword, T("???"));
		}

		PP_ExtractMacro(NULL, NULL, NULL, param, param, XEO_DISPONLY);

		AppendItemToTreeV(VTS, keyword, hParentTree, 0, param);
	}

	if ( count == 0 ){
		if ( hParentTree == TVI_ROOT ){
			wsprintf(param, T("%s is not found"), custname);
			AppendItemToTreeV(VTS, param, hParentTree, 0, NilStr);
		}else{
			TV_ITEM tvi;
										// 「[+]」表示の再設定
			tvi.mask = TVIF_CHILDREN;
			tvi.hItem = hParentTree;
			tvi.cChildren = 0;
			TreeView_SetItem(VTS->hTViewWnd, &tvi);
		}
	}
	return count;
}

void InitVFSListTree(VFSTREESTRUCT *VTS, const TCHAR *filename)
{
	TCHAR path[VFPS];
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	DWORD attr;

	if ( VTS->itemdata.top == 0 ) ThAddString(&VTS->itemdata, filename);
	attr = GetFileAttributes(filename);
	attr = ((attr == BADATTR) || !(attr & FILE_ATTRIBUTE_DIRECTORY)) ?
			FILE_ATTRIBUTE_DIRECTORY : 0; // dir以外のときは全属性が対象
	CatPath(path, (TCHAR *)filename, WildCard_All);
										// 列挙
	hFF = VFSFindFirst(path, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ){
		wsprintf(path, T("%s read error"), filename);
		AddItemToTree(VTS, path, TVI_ROOT);
	}else{
		HTREEITEM hTreeItem;

		hTreeItem = AddItemToTree(VTS, filename, TVI_ROOT);
		for ( ; ; ){
			ERRORCODE errorcode;
			int chance;

			if ( VTS->ActiveThreadID == 0 ) break; // 中断要求有り
			if ( !IsRelativeDirectory(ff.cFileName) && !IsHidden(ff) &&
				 ((ff.dwFileAttributes | attr ) & FILE_ATTRIBUTE_DIRECTORY) ){
				if ( !(VTS->X_tree & XTREE_SIMPLEICON) ){
					AddItemToTreePath(VTS, ff.cFileName, hTreeItem, NULL);
				}else{
					AddItemToTree(VTS, ff.cFileName, hTreeItem);
				}
			}
			if ( IsTrue(VFSFindNext(hFF, &ff)) ) continue;
			errorcode = GetLastError();
			chance = 10;
			while ( errorcode == ERROR_MORE_DATA ){
				Sleep(100);
				if ( IsTrue(VFSFindNext(hFF, &ff)) ){
					errorcode = NO_ERROR;
					break;
				}
				errorcode = GetLastError();
				chance--;
				if ( !chance ) break;
			}
			if ( errorcode != NO_ERROR ) break;
		}
		VFSFindClose(hFF);
		TreeView_Expand(VTS->hTViewWnd, hTreeItem, TVE_EXPAND);
	}
}

void AddTreeItemFromList(VFSTREESTRUCT *VTS, const TCHAR *list)
{
	const TCHAR *custname, *listptr;
	HWND hTWnd = VTS->hTViewWnd;
	HTREEITEM hParentItem;
	TV_ITEM tvi;

	hParentItem = TreeView_GetNextItem(hTWnd, NULL, TVGN_DROPHILITE);
	TREEVOID TreeView_SelectDropTarget(hTWnd, NULL);
	if ( hParentItem == NULL ){ // 選択無し…親
		hParentItem = TVI_ROOT;
		custname = ThPointerT(&VTS->itemdata, 0);
	}else{
		tvi.hItem = hParentItem;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTWnd, &tvi);
		if ( (DWORD_PTR)tvi.lParam >= (DWORD_PTR)VTS->itemdata.size ){
			custname = NilStr;
		}else{
			custname = ThPointerT(&VTS->itemdata, tvi.lParam);
		}
		if ( custname[0] != '%' ){
			custname = GetTreeItemCustname(VTS, hParentItem, &hParentItem);
			if ( custname == NULL ){
				XMessage(VTS->hTViewWnd, NULL, XM_GrERRld, T("bad point"));
				return;
			}
		}else{
			custname++; // % をスキップ
		}
	}
	listptr = list;
	while ( *listptr != '\0' ){
		InsertCustTable(custname, listptr, 0x7fffffff, NilStr, sizeof(NilStr));
		AppendItemToTreeV(VTS, listptr, hParentItem, 0, listptr);

		listptr += tstrlen(listptr) + 1;
	}
	tvi.mask = TVIF_CHILDREN;
	tvi.hItem = hParentItem;
	tvi.cChildren = 1;
	TreeView_SetItem(hTWnd, &tvi);
	TreeView_Expand(hTWnd, hParentItem, TVE_EXPAND);

	if ( hParentItem == TVI_ROOT ){
		int count = 0;
		TCHAR keyword[CMDLINESIZE], name[CMDLINESIZE];

		while( EnumCustTable(count, T("Mt_type"), keyword, name, sizeof(name)) >= 0 ){
			if ( tstrcmp(name, custname) == 0 ) return;
			count++;
		}
		SetCustStringTable(T("Mt_type"), custname + 2, custname, 0);
		if ( VTS->hBarWnd != NULL ){
			struct {
				DWORD index;
				TCHAR cmd[VFPS];
			} bar;
			bar.index = 11;
			bar.cmd[0] = EXTCMD_CMD;
			wsprintf(bar.cmd + 1, T("*tree %s"), custname);
			InsertCustTable(T("B_tree"), custname + 2, 0x7fffffff, &bar, sizeof(DWORD) + TSTRSIZE(bar.cmd) );
			DestroyWindow(VTS->hBarWnd);
			VTS->hBarWnd = NULL;

			VfsTreeSetFlagFix(VTS);
		}
	}
}

void InitDirectroyTree(VFSTREESTRUCT *VTS, const TCHAR *selectpath)
{
	TCHAR buf[VFPS], rpath[VFPS];
	int driveno;
	TV_ITEM tvi;
	TV_INSERTSTRUCT tvins;
	TCHAR name[VFPS];
	DWORD drive;
										// #: ---------------------------------
	wsprintf(buf, T("#:> %s"), MessageText(MES_FEXP));
	AddItemToTreePath(VTS, buf, TVI_ROOT, selectpath);
										// : ----------------------------------
	tvi.cchTextMax = wsprintf(buf, T(":> %s"), MessageText(MES_FMYF));
	tvi.pszText = buf;
	tvi.lParam = MAXLPARAM;
	if ( VTS->X_tree & XTREE_SHOWICON ){
		tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.iImage = 0; // folder
		tvi.iSelectedImage = 0; // folder
	}else{
		tvi.mask = TVIF_TEXT | TVIF_PARAM;
	}
	tvins.hParent = TVI_ROOT;
	tvins.hInsertAfter = TVI_FIRST;
	TreeInsertItemValue(tvins) = tvi;
	SendMessage(VTS->hTViewWnd, TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
										// \\ ---------------------------------
	wsprintf(buf, T("\\\\> %s"), MessageText(MES_FNET));
	AddItemToTreePath(VTS, buf, TVI_ROOT, selectpath);
										// x: ---------------------------------
	name[1] = ':';
	X_dlf_ddt = GetCustDword(StrX_dlf, 0) & XDLF_DISPDRIVETITLE;
	drive = GetLogicalDrives();
	for ( driveno = 0 ; driveno < 26 ; driveno++ ){
		if ( !(drive & LSBIT) ){ // ドライブ無し…未接続ネットワークがないか確認
			rpath[0] = '\0';
			wsprintf(buf, T("Network\\%c"), (TCHAR)(driveno + 'A'));
			GetRegString(HKEY_CURRENT_USER, buf, RPATHSTR, rpath, TSIZEOF(rpath));
			if ( rpath[0] ) setflag(drive, LSBIT);
		}
		if ( drive & LSBIT ) AddTreeDriveRoot(VTS, driveno, selectpath);
		drive >>= 1;
	}
}

void InitPPcListTree(VFSTREESTRUCT *VTS)
{
	int i;
	int no;
	TCHAR disp[VFPS + 16];
	DWORD useppclist = 0;

	if ( hProcessComboWnd != NULL ){
		TCHAR *ppclist = (TCHAR *)SendMessage(hProcessComboWnd, WM_PPXCOMMAND, KCW_ppclist, 0);
		if ( ppclist != NULL ){
			TCHAR *listIDptr, *listPathPtr;
			HTREEITEM hParentItem = AppendItemToTreeV(VTS, NilStr, TVI_ROOT, 0, NULL);
			listIDptr = ppclist;
			for ( ; *listIDptr != '\0'; ){
				listPathPtr = listIDptr + tstrlen(listIDptr) + 1;
				if ( *listIDptr == '\t' ){ // タブ区切り
					TreeView_Expand(VTS->hTViewWnd, hParentItem, TVE_EXPAND);
					hParentItem = AppendItemToTreeV(VTS, NilStr, TVI_ROOT, 0, NULL);
				}else{
					if ( listIDptr[3] == '\0' ){ // ?Cx\0  CZxx でないとき
						setflag(useppclist, 1 << (listIDptr[2] - 'A'));
					}
					if ( VTS->TreeType == TREETYPE_PPCLIST ){
						AppendItemToTreeV(VTS, listPathPtr, hParentItem, 0, listPathPtr);
					}else{
						wsprintf( disp, T("[%s] %s"), listIDptr + 2, listPathPtr);
						AppendItemToTreeV(VTS, disp, hParentItem, 0, listIDptr + 2);
					}
				}
				listIDptr = listPathPtr + tstrlen(listPathPtr) + 1;
			}
			HeapFree(DLLheap, 0, ppclist);
			TreeView_Expand(VTS->hTViewWnd, hParentItem, TVE_EXPAND);
		}
	}

//	UsePPx(); // デッドロック防止のため同期を無効にしている
	for ( no = 'A' ; no <= 'Z' ; no++ ){
		if ( useppclist & (1 << (no - 'A')) ) continue;
		for ( i = 0 ; i < X_Mtask ; i++ ){
			if ( CheckPPcID(i) && (Sm->P[i].ID[2] == no) ){
				if ( VTS->TreeType == TREETYPE_PPCLIST ){
					tstrcpy(disp, Sm->P[i].path);
					AppendItemToTreeV(VTS, disp, TVI_ROOT, 0, NULL);
				}else{
					wsprintf(disp, T("[%c] %s"), no, Sm->P[i].path);
					AppendItemToTreeV(VTS, disp, TVI_ROOT, 0, Sm->P[i].ID + 2);
				}
				break;
			}
		}
	}
//	FreePPx();
}

HICON LoadDefaultDirTreeIcon(void)
{
	SHFILEINFO shfinfo;
	TCHAR X_dicn[VFPS];
	int retry = 10;

	GetCustData(T("X_dicn"), X_dicn, sizeof(X_dicn));

	if ( X_dicn[0] != '\0' ){
		VFSFixPath(NULL, X_dicn, DLLpath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
		#pragma warning(suppress:6001) // SHGFI_ATTR_SPECIFIED がなければ[in]ではない
		if ( SHGetFileInfo(X_dicn, 0, &shfinfo, sizeof(shfinfo), TreeIconSHflag) ){
			return shfinfo.hIcon;
		}
	}
	PP_ExtractMacro(NULL, NULL, NULL, T("%'WinDir'"), X_dicn, 0);
	while ( retry-- ){
		#pragma warning(suppress:6001) // SHGFI_ATTR_SPECIFIED がなければ[in]ではない
		if ( SHGetFileInfo(X_dicn, FILE_ATTRIBUTE_DIRECTORY, &shfinfo,
				sizeof(shfinfo), TreeIconSHflag | SHGFI_USEFILEATTRIBUTES) ){
			if ( shfinfo.hIcon != NULL ) break;
		}
		Sleep(10); // Errorcode 15106(リソース列挙の中断) 対策
	}
	return shfinfo.hIcon;
}

//-------------------------------------- ツリーアイテムを登録し直す
BOOL InitTreeViewItems(VFSTREESTRUCT *VTS, const TCHAR *param)
{
	HWND hTWnd = VTS->hTViewWnd;

	VTS->X_tree = GetCustDword(StrX_tree, XTREE_DEFAULT);
	if ( *param != '\0' ){
		if ( *param == '%' ){
			param++;
			VTS->TreeType = TREETYPE_FAVORITE;
		}else if ( tstrcmp(param, TreePPcListName) == 0 ){
			VTS->TreeType = TREETYPE_PPCLIST;
		}else if ( tstrcmp(param, TreeFocusPPcName) == 0 ){
			VTS->TreeType = TREETYPE_FOCUSPPC;
		}else if ( *param == '*' ){
			param++;
			VTS->TreeType = TREETYPE_VFS;
		}
	}

	SendMessage(hTWnd, WM_SETREDRAW, FALSE, 0);
	SendMessage(hTWnd, TVM_SELECTITEM, TVGN_CARET, (LPARAM)NULL); // 選択解除
	TreeView_DeleteAllItems(hTWnd); // 選択していると TVN_SELCHANGED が連続発行されるので注意！

	if ( VTS->X_tree & XTREE_SHOWICON ){
		if ( VTS->hImage == NULL ){
			HICON hIcon;
			COLORREF bkcolor;

			VTS->hImage = DImageList_Create(TreeIconSize, TreeIconSize, 24/* | ILC_MASK*/, 32, 0);
			bkcolor = (OSver.dwMajorVersion >= 5) ? TreeView_GetBkColor(hTWnd) : C_AUTO;
			if ( bkcolor == C_AUTO ) bkcolor = C_WindowBack;
			DImageList_SetBkColor(VTS->hImage, bkcolor);
			hIcon = LoadDefaultDirTreeIcon();
			DImageList_AddIcon(VTS->hImage, hIcon);
			DestroyIcon(hIcon);

			SendMessage(VTS->hTViewWnd, TVM_SETIMAGELIST, (WPARAM)TVSIL_NORMAL, (LPARAM)VTS->hImage);
		}
	}else{
		if ( VTS->hImage != NULL ){
			SendMessage(VTS->hTViewWnd, TVM_SETIMAGELIST, (WPARAM)TVSIL_NORMAL, (LPARAM)NULL);
			DImageList_Destroy(VTS->hImage);
			VTS->hImage = NULL;
		}
	}

	SendMessage(hTWnd, WM_SETREDRAW, TRUE, 0);
	ThFree(&VTS->itemdata);

	switch ( VTS->TreeType ){
		case TREETYPE_FAVORITE:
			AddFavoriteItem(VTS, TVI_ROOT, param);
			break;
		case TREETYPE_FOCUSPPC:
		case TREETYPE_PPCLIST:
			InitPPcListTree(VTS);
			break;
		case TREETYPE_VFS:
			InitVFSListTree(VTS, param);
			break;
//		case TREETYPE_DIRECTORY:
		default:
			InitDirectroyTree(VTS, param);
			break;
	}
	FixTreeCaret(hTWnd);
	return TRUE;
}

BOOL FindPathSeparator_NoTml(const TCHAR *path)
{
	TCHAR *np = FindPathSeparator(path);

	if ( np == NULL ) return TRUE;
	if ( *(np + 1) == '\0' ) return TRUE;
	return FALSE;
}

//-------------------------------------- 指定パスに移動(なければ追加)
BOOL SetPathItems(VFSTREESTRUCT *VTS, HTREEITEM hFirstItem, const TCHAR *path, DWORD EventType)
{
	HTREEITEM hItem, hCurrentItem;
	TCHAR tvibuf[MAX_PATH];
	const TCHAR *p;
	TV_ITEM tvi;
	HWND hTWnd = VTS->hTViewWnd;

	hCurrentItem = hFirstItem;
	while ( hCurrentItem != NULL ){
		size_t len;

		tvi.mask = TVIF_TEXT;
		tvi.hItem = hCurrentItem;
		tvi.pszText = tvibuf;
		tvi.cchTextMax = TSIZEOF(tvibuf);
		TreeView_GetItem(hTWnd, &tvi);
		{
			TCHAR *ps;

			ps = tstrchr(tvibuf, '>');
			if ( ps != NULL ) *ps = '\0';
		}
		len = tstrlen(tvibuf);
		while ( tstrnicmp(tvibuf, path, len) == 0 ){
			p = path + len;
			if ( *p == '\\' ){
				p++;
			}else if ( *p == '/' ){
				p++;
			}else{
				if ( (*tvibuf != '\\') && (*p != '\0') ) break;
			}
			if ( *p == '\0' ){ // *** 最終エントリまで見つけた ***
				if ( EventType == 0 ){ // パス移動なら、フォーカス設定
					SeletTreeItem(VTS, hCurrentItem);
				}else if ( EventType & (SHCNE_RMDIR | SHCNE_DRIVEREMOVED) ){ // パス削除なら、削除
					if ( (EventType == SHCNE_RMDIR) && (path[1] == ':') && (path[3] == '\0') ){ // ドライブ追加なのに SHCNE_RMDIR で送信 Win10 2016/04
						return TRUE;
					}
					SendMessage(hTWnd, TVM_DELETEITEM, 0, (LPARAM)hCurrentItem);
				}
				return TRUE;
			}

			hItem = TreeView_GetChild(hTWnd, hCurrentItem);
			if ( hItem == NULL ){ // *** 次の子エントリがあるが、子ツリーが無い
				// パス移動なら子ツリーを生成＆フォーカス設定
				if ( EventType == 0 ){
					BOOL result;

					if ( VTS->ExpandCount == 0 ){
						VTS->ExpandCount++;
						result = ExpandTree(VTS, hCurrentItem, p);
						VTS->ExpandCount--;
						return result;
					}
				}
				return TRUE;
			}
			return SetPathItems(VTS, hItem, p, EventType); // 下位を検索
		}
		hCurrentItem = TreeView_GetNextSibling(hTWnd, hCurrentItem);
	}
	// *** 存在しないエントリ ***
	// パス移動ならエントリ作成後フォーカス設定
	// フォルダ作成ならエントリ作成のみ
	if ( ((EventType == 0) && (VTS->TreeType == TREETYPE_DIRECTORY)) ||
		 ((EventType & (SHCNE_MKDIR | SHCNE_DRIVEADD)) && IsTrue(FindPathSeparator_NoTml(path)))
	){
		tvi.mask = TVIF_CHILDREN;
		tvi.hItem = TreeView_GetParent(hTWnd, hFirstItem);
		TreeView_GetItem(hTWnd, &tvi);
		if ( (tvi.cChildren == 0) && (tvi.hItem != TVI_ROOT) ){
			tvi.cChildren = 1;
			TreeView_SetItem(hTWnd, &tvi);
		}
		AddItemToTreeForce(VTS, tvi.hItem, path, (EventType == 0));
	}
	return TRUE;
}
//-------------------------------------- 使用できるように準備
VFSDLL void PPXAPI InitVFSTree(void)
{
	WNDCLASS TreeClass;

	LoadCommonControls(ICC_TREEVIEW_CLASSES);
										// ウインドウクラスを定義する ---------
	TreeClass.style			= 0;
	TreeClass.lpfnWndProc	= TreeProc;
	TreeClass.cbClsExtra	= 0;
	TreeClass.cbWndExtra	= 0;
	TreeClass.hInstance		= NULL;
	TreeClass.hIcon			= NULL;
	TreeClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	TreeClass.hbrBackground	= NULL;
	TreeClass.lpszMenuName	= NULL;
	TreeClass.lpszClassName	= TreeClassStr;
											// クラスを登録する
	RegisterClass(&TreeClass);

	TreeIconSize = GetSystemMetrics(SM_CXICON) / 2;
	TreeIconSHflag = (TreeIconSize >= 32) ?
			(SHGFI_ICON | SHGFI_LARGEICON) : (SHGFI_ICON | SHGFI_SMALLICON);
}
