/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library			Sub
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
//#include <commctrl.h>
//#include <mmsystem.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "CALC.H"
#pragma hdrstop

BOOL LoadedCharlengthTable = FALSE; // 文字列長テーブルを初期化を行ったかどうか

int ExecKeyStack = 0; // MAXEXECKEYSTACK まで
#define MAXEXECKEYSTACK 50

const TCHAR StrTempExtractPath[] = T("EXTRACTTEMP");
const TCHAR StrDummyTempPath[] = T("C:\\PPXTEMP");
const TCHAR RegAppData[] =T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
const TCHAR RegAppDataName[] =T("AppData");

IID XCLSID_RegExp = {0x3F4DACA4, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
//IID XCLSID_Match = {0x3F4DACA5, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
//IID XCLSID_MatchCollection = {0x3F4DACA6, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
IID XIID_IRegExp =	{0x3F4DACA0, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
IID XIID_IMatch =	{0x3F4DACA1, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
IID XIID_IMatch2 =	{0x3F4DACB1, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
IID XIID_IMatchCollection = {0x3F4DACA2, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};
IID XIID_ISubMatches = {0x3F4DACB3, 0x160D, 0x11D2, {0xA8, 0xE9, 0x00, 0x10, 0x4B, 0x36, 0x5C, 0x9F}};

// Shell Extension
IID XIID_IContextMenu3 = {0xbcfce0a0, 0xec17, 0x11d0, {0x8d, 0x10, 0x0, 0xa0, 0xc9, 0xf, 0x27, 0x19}};
IID XIID_IColumnProvider = {0xe8025004, 0x1c42, 0x11d2, {0xbe, 0x2c, 0x00, 0xa0, 0xc9, 0xa8, 0x3d, 0xa1}};
IID XIID_IPropertySetStorage = {0x0000013A, 0x0000, 0x0000, {0xc0, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x46}};

#ifdef WINEGCC
IID XIID_IClassFactory ={1, 0, 0, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IUnknown = {0, 0, 0, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IDataObject = {0x0000010e, 0, 0, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IDropSource = {0x00000121, 0, 0, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

IID XIID_IStorage = {0x0000000b, 0x0000, 0x0000, {0xc0, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x46}};
IID XIID_IPersistFile =	{0x0000010b, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

// Shell Extension
IID XCLSID_ShellLink =	{0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IContextMenu =	{0x000214E4, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IShellFolder =	{0x000214E6, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IShellLink =	{0x000214EE, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
IID XIID_IContextMenu2 ={0x000214F4, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
#endif

#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x1a
#endif

DWORD AjiEnterCount = 0;


// レジストリから文字列を得る -------------------------------------------------
// ※ path と dest が同じ変数でも問題無い
_Success_(return)
PPXDLL BOOL PPXAPI GetRegString(HKEY hKey, const TCHAR *path, const TCHAR *name, _Out_ TCHAR *dest, DWORD size)
{
	HKEY HK;
	DWORD t, s;
	TCHAR buf[VFPS];

	if ( RegOpenKeyEx(hKey, path, 0, KEY_READ, &HK) == ERROR_SUCCESS ){
		s = size;
		if (RegQueryValueEx(HK, name, NULL, &t, (LPBYTE)dest, &s) == ERROR_SUCCESS){
			if ( (t == REG_EXPAND_SZ) && (s < sizeof(buf)) ){

				tstrcpy(buf, dest);
				ExpandEnvironmentStrings(buf, dest, size);
			}
			RegCloseKey(HK);
			return TRUE;
		}
		RegCloseKey(HK);
	}
	return FALSE;
}


//======================================================================== 同期
/*-----------------------------------------------------------------------------
 共有メモリ使用の排他処理

戻り値:0:正常終了 -1:20秒以上使用できず、ユーザーが無視した
-----------------------------------------------------------------------------*/
const TCHAR UsePPxTitle[] = T("PPx synchronize error(UsePPx)");
const TCHAR LockErrorMessage[] = T("Lock error, ignore ?\nロックエラーです。無視しますか？");

#define USE_SPINTIMEOUT B0
#define USE_SPINTIMEOUT_DIE B1
#define USE_SPINTIMEOUT_DIALOG B2
#define USE_SPINTIMEOUT_SAMETHREAD B3
#define USE_NOWSHUTDOWN B4
#define USE_CHECK_DIE B8
#define USE_CHECK_DIALOG B9
// #define USE_CHECK_DIALOG_SAMETHREAD B10
#define USE_CHECK_DIALOG_ERROR B11
#define USE_CHECK_SAMETHREAD B14
/*
0x200	通常待機、ダイアログ表示有り
0x205	通常待機ダイアログ表示有り & SPINTIMEOUT ダイアログ表示有り


*/
HANDLE SpinThreadAliveCheck(void)
{
	HANDLE hThread;

	// 利用中スレッドの生存確認
	if ( DOpenThread == INVALID_HANDLE_VALUE ){
		GETDLLPROC(hKernel32, OpenThread);
	}
	if ( DOpenThread != NULL ){
		hThread = DOpenThread(THREAD_QUERY_INFORMATION,
				FALSE, Sm->UsePPxSync.ThreadID);
		if ( hThread != NULL ) CloseHandle(hThread); // スレッドは有効
		return hThread;
	}else{
		return INVALID_HANDLE_VALUE;
	}
}

#define NO_SYNCDIALOG 1

#if PPXSYNCDEBUG
int UsePPxDialog(const TCHAR *text, const char *filename, int fileline, UINT flags)
{
	TCHAR titlebuf[200];

	wsprintf(titlebuf, T("PPx synchronize error(UsePPx)%hs(%d),%hs,%hs"),
			filename, fileline, Sm->UsePPxFirst, Sm->UsePPxLast);
	#if NO_SYNCDIALOG
		return 0;
	#else
		return MessageBox(NULL, text, titlebuf, flags);
	#endif
}

PPXDLL void PPXAPI UsePPxDebug(const char *filename, int fileline)
#else
	#if NO_SYNCDIALOG
		#define UsePPxDialog(text, filename, fileline, flags)
	#else
		#define UsePPxDialog(text, filename, fileline, flags) MessageBox(NULL, text, UsePPxTitle, flags)
	#endif
PPXDLL void PPXAPI UsePPx(void)
#endif
{
	DWORD OldThreadID = Sm->UsePPxSync.ThreadID;
	DWORD ThreadID = GetCurrentThreadId();
	DWORD Tick = GetTickCount();
	DWORD SpinCheckWait;
	DWORD checkflag = 0;
	#if !NO_SYNCDIALOG
	HWND hDlgWnd = NULL;
	MESSAGEDATA md;
	#endif

	for ( ; ; ){
		// 構造体へのアクセス権を確保
		SpinCheckWait = 0;
		while ( InterlockedExchange(&Sm->UsePPxSync.SpinLock, 1) != 0 ){
			DWORD TickDelta;

			Sleep(SpinCheckWait);
			if ( (TickDelta = (GetTickCount() - Tick)) != 0 ){
				SpinCheckWait = 10;
				if ( IsTrue(Sm->NowShutdown) && (TickDelta >= 100) ){
					TickDelta = UsePPxSpinMaxTime;
				}
			}
			// オーバーフローしたときはすぐにダイアログがでる→ok
			if ( TickDelta >= UsePPxSpinMaxTime ){
				setflag(checkflag, USE_SPINTIMEOUT);
				if ( Sm->UsePPxSync.ThreadID == ThreadID ){ // 再入
					setflag(checkflag, USE_SPINTIMEOUT_SAMETHREAD);
					break;
				}

				if ( SpinThreadAliveCheck() == NULL ){ // 利用中スレッドが死亡→奪う
					setflag(checkflag, USE_SPINTIMEOUT_DIE);
					Sm->UsePPxSync.ThreadID = 0; // 強制解放
					break;
				}

				if ( IsTrue(Sm->NowShutdown) ){
					setflag(checkflag, USE_SPINTIMEOUT_DIALOG | USE_NOWSHUTDOWN);
					Sm->UsePPxSync.ThreadID = 0; // 強制解放
					break;
				}

				setflag(checkflag, USE_SPINTIMEOUT_DIALOG);

				#if NO_SYNCDIALOG
					UsePPxDialog(LockErrorMessage, filename, fileline,
							MB_ICONEXCLAMATION | MB_RETRYCANCEL);
					Sm->UsePPxSync.ThreadID = 0; // 強制解放
					break;
				#else
				{
					int result;

					result = UsePPxDialog(LockErrorMessage, filename, fileline,
							MB_ICONEXCLAMATION | MB_RETRYCANCEL);
					if ( (result == 0) || (result == IDCANCEL) ){
						Sm->UsePPxSync.ThreadID = 0; // 強制解放
						break;
					}else{
						continue;
					}
				}
				#endif
			}
		}
		// 構造体へのアクセス権を確保完了

		if ( Sm->UsePPxSync.ThreadID == ThreadID ){ // 再入
			#if PPXSYNCDEBUG
				wsprintfA(Sm->UsePPxLast, "R:%s(%d)+%d", filename, fileline, Sm->UsePPxSync.SpinCount);
			#endif
			if ( Sm->UsePPxSync.SpinCount > 0xffff ){
				#if !NO_SYNCDIALOG
				if ( Sm->NowShutdown == FALSE ){
					UsePPxDialog(T("Reentrance error"), filename, fileline, MB_OK);
				}
				#endif
			}else{
				Sm->UsePPxSync.SpinCount++;
			}
			break; // 許可
		}

		if ( Sm->UsePPxSync.ThreadID != 0 ){ // 既に利用中
			HANDLE hThread;
			if ( (GetTickCount() - Tick) < (IsTrue(Sm->NowShutdown) ?
					(DWORD)(UsePPxMaxTime / 3) : UsePPxMaxTime) ){ // 待機時間内
				InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0); //spin解放
				Sleep(10);
				continue;
			}

			hThread = SpinThreadAliveCheck(); // 利用中スレッドの生存確認
			// 利用中スレッドが死亡→奪う
			if ( hThread == NULL ) setflag(checkflag, USE_CHECK_DIE);

			#if NO_SYNCDIALOG
				setflag(checkflag, USE_CHECK_DIALOG_ERROR);
				InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0); //spin解放
				Sleep(10);
				goto forceuse;
			#else
			// ダイアログがないなら表示
			if ( (Sm->NowShutdown == FALSE) && (hThread != NULL) ){
				if ( hDlgWnd == NULL ){
					#if PPXSYNCDEBUG
						TCHAR titlebuf[200];

						wsprintf(titlebuf, T("PPx synchronize error(UsePPx)(%hs:%d),Flag:%x,Spin:%d,NowThread:%s(%d),BlockThread:%s(%d),BlockFirst:%hs,BlockLast:%hs"),
								filename, fileline, checkflag, Sm->UsePPxSync.SpinCount, GetThreadName(ThreadID), ThreadID, GetThreadName(OldThreadID), OldThreadID, Sm->UsePPxFirst, Sm->UsePPxLast);
						md.title = titlebuf;
					#else
						md.title = UsePPxTitle;
					#endif
					InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0);
					setflag(checkflag, USE_CHECK_DIALOG);
					md.text = LockErrorMessage;
					md.style = MB_PPX_USEPPXCHECKOKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION;
					hDlgWnd = CreateDialogParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL), NULL, MessageBoxDxProc, (LPARAM)&md);
					if ( hDlgWnd == NULL ){
						setflag(checkflag, USE_CHECK_DIALOG_ERROR);
						goto forceuse;
					}
				}else{
					MSG msg;

					if ( IsWindow(hDlgWnd) == FALSE ) goto forceuse;
					InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0);
					while ( IsTrue(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) ){
						if ( msg.message == WM_QUIT ) break;
						if ( IsDialogMessage(hDlgWnd, &msg) == FALSE ){
							if ( msg.message == WM_PAINT ){
								ValidateRect(msg.hwnd, NULL);
							}
//							TranslateMessage(&msg);
//							DispatchMessage(&msg);
							// 破棄する
						}
					}
				}
				Sleep(10);
				continue;
			}
			#endif
		}
forceuse:
		// 新規利用
		Sm->UsePPxSync.ThreadID = ThreadID;
//		Sm->UsePPxSync.TickCount = GetTickCount();
		Sm->UsePPxSync.SpinCount = 1;
#if PPXSYNCDEBUG
		wsprintfA(Sm->UsePPxFirst, "%s(%d)", filename, fileline);
		Sm->UsePPxLast[0] = '\0';
#endif
		break;
	}
	InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0);
#if !NO_SYNCDIALOG
	if ( hDlgWnd != NULL ) DestroyWindow(hDlgWnd);
#endif
	if ( checkflag ){
#if PPXSYNCDEBUG
		XMessage(NULL, NULL, XM_DbgLOG, T("UsePPx %hs(%d): %x,%d,%s-%d,%s-%d,%hs,%hs"),
				filename, fileline, checkflag, Sm->UsePPxSync.SpinCount, GetThreadName(ThreadID), ThreadID, GetThreadName(OldThreadID), OldThreadID, Sm->UsePPxFirst, Sm->UsePPxLast);
#else
		XMessage(NULL, NULL, XM_DbgLOG, T("UsePPx : %x,%d,%s-%d,%s-%d"),
				checkflag, Sm->UsePPxSync.SpinCount, GetThreadName(ThreadID), ThreadID, GetThreadName(OldThreadID), OldThreadID);
#endif
	}
	return;
}
/*-----------------------------------------------------------------------------
 共有メモリ使用の排他処理の解除
-----------------------------------------------------------------------------*/
#if PPXSYNCDEBUG
int FreePPxDialog(const TCHAR *text, const char *filename, int fileline, UINT flags)
{
	TCHAR titlebuf[200];

	wsprintf(titlebuf, T("PPx synchronize error(FreePPx)%hs(%d)"), filename, fileline);
	return MessageBox(NULL, text, titlebuf, flags);
}

PPXDLL void PPXAPI FreePPxDebug(const char *filename, int fileline)
#else

const TCHAR FreePPxTitle[] = T("PPx synchronize error(FreePPx)");
#define FreePPxDialog(text, filename, fileline, flags) MessageBox(NULL, text, FreePPxTitle, flags)

PPXDLL void PPXAPI FreePPx(void)
#endif
{
	DWORD Tick = GetTickCount();
	DWORD SpinCheckWait = 0;

	// 構造体へのアクセス権を確保
	while ( InterlockedExchange(&Sm->UsePPxSync.SpinLock, 1) != 0 ){
		DWORD TickDelta;

		Sleep(SpinCheckWait);
		if ( (TickDelta = (GetTickCount() - Tick)) != 0 ){
			SpinCheckWait = 10;
			if ( IsTrue(Sm->NowShutdown) && (TickDelta >= 100) ){
				TickDelta = UsePPxSpinMaxTime;
			}
		}
		// オーバーフローしたときはすぐにダイアログがでる→ok
		if ( TickDelta >= UsePPxSpinMaxTime ){

			if ( Sm->UsePPxSync.ThreadID != GetCurrentThreadId() ){
				// 他のスレッドによって奪われていた
				if ( SpinThreadAliveCheck() == NULL ){ // 利用中スレッドが死亡→解放
					Sm->UsePPxSync.ThreadID = 0;
					break;
				}
				return;
			}
			#if NO_SYNCDIALOG
				break;
			#else
			{
				int result;
				if ( IsTrue(Sm->NowShutdown) ) break;
				result = FreePPxDialog(LockErrorMessage, filename, fileline,
						MB_ICONEXCLAMATION | MB_RETRYCANCEL);
				if ( (result != IDCANCEL) && (result != 0) ){
					Tick = GetTickCount();
					continue;
				}else{
					break;
				}
			}
			#endif
		}
	}

	// 構造体へのアクセス権を確保完了

	if ( Sm->UsePPxSync.ThreadID != GetCurrentThreadId() ){
		// 他のスレッドによって奪われていた
		Sm->UsePPxSync.ThreadID = 0; // 解放
	}else{
		Sm->UsePPxSync.SpinCount--;
		if ( Sm->UsePPxSync.SpinCount <= 0 ){
			Sm->UsePPxSync.ThreadID = 0; // 解放
		}
	}
	InterlockedExchange(&Sm->UsePPxSync.SpinLock, 0);
	return;
}
//=========================================================== Stack Heap 関連
PPXDLL void PPXAPI ThInit(ThSTRUCT *TH)
{
	TH->bottom = NULL;
	TH->top = 0;
	TH->size = 0;
}

PPXDLL BOOL PPXAPI ThFree(ThSTRUCT *TH)
{
	BOOL result = TRUE;
	char *bottom;

	bottom = TH->bottom;
	if ( bottom != NULL ){
		TH->bottom = NULL;
		TH->top = 0;
		TH->size = 0;
		result = HeapFree(DLLheap, 0, bottom);
	}
	return result;
}

#define ThAllocCheck()								\
	if ( TH->bottom == NULL ){						\
		TH->bottom = HeapAlloc(DLLheap, 0, ThSTEP);	\
		if ( TH->bottom == NULL ) return FALSE;		\
		TH->size = ThSTEP;							\
	}

#define ThSizecheck(CHECKSIZE)						\
	while ( (TH->top + (CHECKSIZE)) > TH->size ){	\
		char *p;									\
		DWORD nextsize;								\
													\
		nextsize = TH->size + ThNextAllocSizeM(TH->size < (256 * KB) ? TH->size : (TH->size * 2) );\
		p = HeapReAlloc(DLLheap, 0, TH->bottom, nextsize);\
		if (p == NULL) return FALSE;				\
		TH->bottom = (void *)p;						\
		TH->size = nextsize;						\
	}

PPXDLL BOOL PPXAPI ThSize(ThSTRUCT *TH, DWORD size)
{
	ThAllocCheck();
	ThSizecheck(size);
	return TRUE;
}

PPXDLL BOOL PPXAPI ThAppend(ThSTRUCT *TH, const void *data, DWORD size)
{
	ThAllocCheck();
	ThSizecheck(size);
	memcpy(ThLast(TH), data, size);
	TH->top += size;
	return TRUE;
}

PPXDLL BOOL PPXAPI ThAddString(ThSTRUCT *TH, const TCHAR *data)
{
	DWORD size;

	ThAllocCheck();
	size = TSTRSIZE32(data);
	ThSizecheck(size + 1);
	memcpy(ThLast(TH), data, size);
	TH->top += size;
	*(TCHAR *)ThLast(TH) = '\0'; //文字列の\0とは別の保護用
	return TRUE;
}

PPXDLL BOOL PPXAPI ThCatString(ThSTRUCT *TH, const TCHAR *data)
{
	DWORD size;

	ThAllocCheck();
	size = TSTRSIZE32(data);
	ThSizecheck(size);
	memcpy(ThLast(TH), data, size);
	TH->top += size - TSTROFF(1);
	return TRUE;
}
#ifdef UNICODE
PPXDLL BOOL PPXAPI ThCatStringA(ThSTRUCT *TH, const char *data)
{
	DWORD size;

	ThAllocCheck();
	size = strlen32(data) + 1;
	ThSizecheck(size);
	memcpy(ThLast(TH), data, size);
	TH->top += size - 1;
	return TRUE;
}
#endif

/*
	((DWORD 全体のサイズ=8+namesize+strsize+'\0')(WORD nameのサイズ)name str '\0')...
*/

#pragma pack(push, 1)
typedef struct {
	DWORD varsize;
	WORD namesize;
	char name[1];
} THVARS;
#pragma pack(pop)

PPXDLL BOOL PPXAPI ThSetString(ThSTRUCT *TH, const TCHAR *name, const TCHAR *str)
{
	DWORD namesize = TSTRLENGTH32(name), strsize = TSTRSIZE32(str);
	DWORD varsize;
	THVARS *nulltvs = NULL; // 途中で見つけた空きブロック
	THVARS *tvs, *maxtvs;

	varsize = (sizeof(DWORD) * 2) + namesize + strsize;
	if ( TH == NULL ) TH = &ProcessStringValue;
	tvs = (THVARS *)TH->bottom;
	maxtvs = (THVARS *)(char *)(TH->bottom + TH->top);
	while ( tvs < maxtvs ){
		DWORD tvsnamesize;

		tvsnamesize = tvs->namesize;
		if ( (tvsnamesize == namesize) &&
			 (memcmp(tvs->name, name, namesize) == 0) ){
			if ( tvs->varsize >= varsize ){ // 再利用が可能
				if ( strsize == sizeof(TCHAR) ){ // 削除
					tvs->namesize = 0;
				}else{
					memcpy( tvs->name + tvs->namesize, str, strsize );
				}
				return TRUE;
			}
			// 小さいので削除する
			tvs->namesize = 0;
		}else if ( (tvsnamesize == 0) && (tvs->varsize >= varsize) ){ // 再利用が可能
			nulltvs = tvs;
		}
		tvs = (THVARS *)(char *)( (char *)tvs + tvs->varsize );
	}
	if ( strsize == sizeof(TCHAR) ) return TRUE; // 削除...保存してなかった

	if ( nulltvs != NULL ){ // 再利用可能
		nulltvs->namesize = (WORD)namesize;
		memcpy(nulltvs->name, name, namesize);
		memcpy(nulltvs->name + namesize, str, strsize);
		return TRUE;
	}
	// 新規確保
	if ( ThSize(TH, varsize) == FALSE ) return FALSE;
	tvs = (THVARS *)(char *)(TH->bottom + TH->top);
	tvs->varsize = varsize;
	tvs->namesize = (WORD)namesize;
	memcpy(tvs->name, name, namesize);
	memcpy(tvs->name + namesize, str, strsize);
	TH->top += varsize;
	return TRUE;
}

PPXDLL TCHAR * PPXAPI ThAllocString(ThSTRUCT *TH, const TCHAR *name, DWORD strsize)
{
	DWORD namesize = TSTRLENGTH32(name);
	DWORD varsize;
	THVARS *nulltvs = NULL; // 途中で見つけた空きブロック
	THVARS *tvs, *maxtvs;

	varsize = (sizeof(DWORD) * 2) + namesize + strsize;

	if ( TH == NULL ) TH = &ProcessStringValue;
	tvs = (THVARS *)TH->bottom;
	maxtvs = (THVARS *)(char *)(TH->bottom + TH->top);
	while ( tvs < maxtvs ){
		DWORD tvsnamesize;

		tvsnamesize = tvs->namesize;
		if ( (tvsnamesize == namesize) &&
			 (memcmp(tvs->name, name, namesize) == 0) ){
			if ( tvs->varsize >= varsize ){ // 再利用が可能
				return (TCHAR *)(char *)(tvs->name + tvs->namesize);
			}
			// 小さいので削除する
			tvs->namesize = 0;
		}else if ( (tvsnamesize == 0) && (tvs->varsize >= varsize) ){ // 再利用が可能
			nulltvs = tvs;
		}
		tvs = (THVARS *)(char *)( (char *)tvs + tvs->varsize );
	}
	if ( nulltvs != NULL ){ // 再利用可能
		nulltvs->namesize = (WORD)namesize;
		memcpy(nulltvs->name, name, namesize);
		return (TCHAR *)(char *)(nulltvs->name + namesize);
	}
	// 新規確保
	if ( ThSize(TH, varsize) == FALSE ) return FALSE;
	tvs = (THVARS *)(char *)(TH->bottom + TH->top);
	tvs->varsize = varsize;
	tvs->namesize = (WORD)namesize;
	memcpy(tvs->name, name, namesize);
	TH->top += varsize;
	return (TCHAR *)(char *)(tvs->name + namesize);
}

PPXDLL const TCHAR * PPXAPI ThGetString(ThSTRUCT *TH, const TCHAR *name, TCHAR *str, DWORD strlength)
{
	DWORD namesize = TSTRLENGTH32(name);
	THVARS *tvs, *maxtvs;

	if ( TH == NULL ) TH = &ProcessStringValue;
	tvs = (THVARS *)TH->bottom;
	maxtvs = (THVARS *)(char *)(TH->bottom + TH->top);
	while ( tvs < maxtvs ){
		if ( (tvs->namesize == (WORD)namesize) &&
			 (memcmp(tvs->name, name, namesize) == 0) ){
			if ( str != NULL ){
				tstrlimcpy(str, (TCHAR *)(char *)(tvs->name + namesize), strlength - 1 );
			}
			return (TCHAR *)(char *)(tvs->name + namesize);
		}
		tvs = (THVARS *)(char *)( (char *)tvs + tvs->varsize );
	}
	// 保存してなかった
	if ( str != NULL ) *str = '\0';
	return NULL;
}

PPXDLL BOOL PPXAPI ThEnumString(ThSTRUCT *TH, int index, TCHAR *name, TCHAR *str, DWORD strlength)
{
	THVARS *tvs, *maxtvs;

	if ( TH == NULL ) TH = &ProcessStringValue;
	tvs = (THVARS *)TH->bottom;
	maxtvs = (THVARS *)(char *)(TH->bottom + TH->top);
	while ( tvs < maxtvs ){
		if ( tvs->namesize && (index-- == 0) ){
			memcpy(name, tvs->name, tvs->namesize);
			name[tvs->namesize / sizeof(TCHAR)] = '\0';
			tstrlimcpy(str, (TCHAR *)(char *)(tvs->name + tvs->namesize), strlength - 1);
			return TRUE;
		}
		tvs = (THVARS *)(char *)( (char *)tvs + tvs->varsize );
	}
	return FALSE;
}

//================================================================== エラー関係
/*-----------------------------------------------------------------------------
	エラーメッセージの取得

	str		格納バッファ(VFPS)
	number	エラーの内容、PPERROR_GETLASTERROR なら GetLastError() の値を用いる
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI PPErrorMsg(TCHAR *str, ERRORCODE code)
{
	TCHAR *p;

	*str = '\0';
	if ( code == PPERROR_GETLASTERROR ){
		code = GetLastError();
		if ( code == NO_ERROR ) return NO_ERROR;
	}
	FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
			(LPTSTR)str, VFPS, NULL);
	for ( p = str ; *p ; p++ ) if ( (UTCHAR)*p < ' ' ) *p = ' ';
	while ( (p > str) && (*(p - 1) == ' ') ) p--;
	if ( p < (str + 4) ){
		wsprintf(str, T("Unknown error : %d"), code);
	}else if ( (p - str) < (VFPS - 10) ){
		wsprintf(p, T("(%d)"), code);
	}else{
		*p = '\0';
	}
	return code;
}
/*-----------------------------------------------------------------------------
	エラーメッセージの表示

	hWnd	エラーを起こしたウィンドウ。NULL でもよい。
	code	エラーの内容、PPERROR_GETLASTERROR なら GetLastError() の値を用いる
	戻り値	エラー番号、成功なら 0
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI PPErrorBox(HWND hWnd, const TCHAR *title, ERRORCODE code)
{
	TCHAR mes[VFPS];

	code = PPErrorMsg(mes, code);
	if ( code != NO_ERROR ){
		PMessageBox(hWnd, mes, title, MB_APPLMODAL | MB_OK | MB_ICONEXCLAMATION);
	}
	return code;
}
//====================================================================== その他
/*-----------------------------------------------------------------------------
 カスタマイズ領域／ヒストリ領域用の名前を生成する
	src = 4バイトの英数字（名前に使用）
	idname = 使用するレジストリの位置
-----------------------------------------------------------------------------*/
void MakeUserfilename(TCHAR *dst, const TCHAR *src, const TCHAR *idname)
{
	TCHAR path[MAX_PATH];
	TCHAR name[MAX_PATH];
										// 固定ファイルがあるならそれを使用 ===
	wsprintf(dst, T("%s%sDEF.DAT"), DLLpath, src);
	if ( GetFileAttributes(dst) != BADATTR ) return;

	if ( !GetRegString(HKEY_CURRENT_USER, idname, src, dst, TSTROFF(MAX_PATH)) ){
		TCHAR *p;
		DWORD t;

		if ( ((DSHGetFolderPath != NULL) &&
			  SUCCEEDED(DSHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path)))
			|| GetRegString(HKEY_CURRENT_USER,
				RegAppData, RegAppDataName, path, TSTROFF(MAX_PATH)) ){
			CatPath(NULL, path, T(PPxSettingsAppdataPath));
		}else{
			tstrcpy(path, DLLpath);
			if ( path[0] == '\\' ) GetWindowsDirectory(path, MAX_PATH);
		}

		t = 0xa55a;
		p = UserName;
		while ( *p ) t = (t << 1) + (DWORD)*p++;
		wsprintf(name, T("%s%04X.DAT"), src, t & 0xffff);
		CatPath(dst, path, name);
#if USETEMPCONFIGFILE // 一発ネタ(きちんと設定を保存しない/仮の設定を作成)の時
		if ( GetFileAttributes(dst) == BADATTR ){
			wsprintf(dst, T("%s%sDEF.DAT"), DLLpath, src);
		}
#else	// 通常の時
		if ( GetFileAttributes(path) == BADATTR ) MakeDirectories(path, NULL);
#endif
	}
}
/*-----------------------------------------------------------------------------
	シフトキーの状態を入手する
-----------------------------------------------------------------------------*/
PPXDLL DWORD PPXAPI GetShiftKey(void)
{
	BYTE pbKeyState[256];

	GetKeyboardState((LPBYTE)&pbKeyState);
	return	(pbKeyState[VK_SHIFT]	& B7 ? K_s : 0) |
			(pbKeyState[VK_CONTROL]	& B7 ? K_c : 0) |
			(pbKeyState[VK_MENU]	& B7 ? K_a : 0) |
			(pbKeyState[X_es]		& B7 ? K_e : 0);
}

/*-----------------------------------------------------------------------------
	キーカスタマイズの解釈をする
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI ExecKeyCommand(const EXECKEYCOMMANDSTRUCT *ekcs, PPXAPPINFO *info, WORD key)
{
	TCHAR buf[CMDLINESIZE], *bufp;
	ERRORCODE result;
	const TCHAR *ekname;

	if ( ExecKeyStack >= MAXEXECKEYSTACK ){
		XMessage(NULL, NULL, XM_FaERRd, MES_RKAL);
		return ERROR_OUT_OF_STRUCTURES;
	}
	ExecKeyStack++;

	PutKeyCode(buf, key);
	buf[CMDLINESIZE - 1] = '\0';
	ekname = ekcs->CustName1;
	if ( NO_ERROR != GetCustTable(ekname, buf, buf, sizeof(buf)) ){
		ekname = ekcs->CustName2;
		if ( (ekname == NULL) || GetCustTable(ekname, buf, buf, sizeof(buf)) ){
			ERRORCODE cmdresult;

			cmdresult = ekcs->Command(info, key | (WORD)K_raw); // 該当無し
			ExecKeyStack--;
			return cmdresult;
		}
	}

	if ( buf[CMDLINESIZE - 1] != '\0' ){ // はみ出したので、確保し直し
		int size = GetCustTableSize(ekname, buf);

		PutKeyCode(buf, key);
		bufp = HeapAlloc(DLLheap, 0, size);
		if ( bufp == NULL ){
			ExecKeyStack--;
			return GetLastError();
		}
		GetCustTable(ekname, buf, bufp, size);
	}else{
		bufp = buf;
	}
	if ( (UTCHAR)bufp[0] == EXTCMD_CMD ){ // コマンド実行
		result = PP_ExtractMacro(info->hWnd, info, NULL, bufp + 1, NULL, 0);
	}else{ // キー
		WORD *keyp, readkey;

		if ( X_Keyra == 1 ){
			X_Keyra = 0;
			GetCustData(T("X_Keyra"), &X_Keyra, sizeof(X_Keyra));
			X_Keyra = (X_Keyra == 0) ? K_raw : 0;
		}
		keyp = (WORD *)(((UTCHAR)bufp[0] == EXTCMD_KEY) ? (bufp + 1) : bufp);
		for ( ;; ){
			readkey = *keyp;
			if ( readkey == 0 ){
				result = NO_ERROR;
				break;
			}
			result = ekcs->Command(info, readkey | (WORD)X_Keyra);
			if ( result != NO_ERROR ) break;
			keyp++;
		}
	}
	if ( bufp != buf ) HeapFree(DLLheap, 0, bufp);
	ExecKeyStack--;
	return result;
}

/*-----------------------------------------------------------------------------
	IME の状態を設定する
	status		0:IME off	1:IME on	2:IME onなら固定入力に切り換え(予定)
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI SetIMEStatus(HWND hWnd, int status)
{
#if !defined(WINEGCC)
	HIMC hIMC;
															// IME を強制解除
	hIMC = ImmGetContext(hWnd);
	if ( hIMC != 0 ){
/*
		if ( status == 2 ){
			if ( IsTrue(ImmGetOpenStatus(hIMC)) ){
				ImmSetConversionStatus(hIMC,
						IME_CMODE_ALPHANUMERIC, IME_SMODE_NONE);
			}
		}else{
*/
			// 希望状態でなければ変更
			if ( ImmGetOpenStatus(hIMC) ? !status : status ){
				ImmSetOpenStatus(hIMC, status);
			}
//		}
		ImmReleaseContext(hWnd, hIMC);
	}
#endif
}

/*-----------------------------------------------------------------------------
	IME の状態を初期設定に変更
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI SetIMEDefaultStatus(HWND hWnd)
{
	if ( (hWnd != NULL) && GetCustDword(T("X_IME"), 0) ) SetIMEStatus(hWnd, 0);
	// ↓押されっぱなしになった場合に備える
	if ( GetShiftKey() & K_e ) keybd_event((BYTE)X_es, 0, KEYEVENTF_KEYUP, 0);
}

BOOL CheckLoadSize(HWND hWnd, DWORD *sizeL)
{
	DWORD X_wsiz = IMAGESIZELIMIT;
	int result;

	GetCustData(T("X_wsiz"), &X_wsiz, sizeof X_wsiz);
	if ( *sizeL <= X_wsiz ) return TRUE;

	if ( hWnd == (HWND)LFI_ALWAYSLIMIT ){
		result = IDYES;
	}else if ( hWnd == (HWND)LFI_ALWAYSLIMITLESS ){
		return TRUE;
	}else{
		HWND hOldFocusWnd C4701CHECK;

		if ( hWnd != NULL ){
			hOldFocusWnd = GetForegroundWindow();
			ForceSetForegroundWindow(hWnd);
		}
		result = PMessageBox(hWnd, MES_QOSL, T("Warning"),
				MB_ICONEXCLAMATION | MB_YESNOCANCEL);
		if ( (hWnd != NULL) && (hOldFocusWnd != hWnd) ){
			ForceSetForegroundWindow(hOldFocusWnd); // C4701ok
		}
	}
	if ( result == IDYES ){
		*sizeL = X_wsiz;
	}else if ( result != IDNO ){
		return FALSE;
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
	filename で指定されたファイルをメモリに読み込む
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI LoadFileImage(const TCHAR *filename, DWORD margin, char **image, DWORD *imagesize, DWORD *filesize)
{
	BOOL useheap; // ヒープを確保したなら true
	HANDLE hFile;
	DWORD sizeL, sizeH;		// ファイルの大きさ
	ERRORCODE result;
	HWND hWnd = NULL;
										// ファイルを開く ---------------------
	hFile = CreateFileL(filename, GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		return GetLastError();
/*
		if ( tstrcmp(filename, T("<")) == 0 ){
			hFile = GetStdHandle(STD_INPUT_HANDLE);
		}
		if ( hFile == INVALID_HANDLE_VALUE ) return GetLastError();
*/
	}
										// ファイルサイズの確認 ---------------
	if ( (filesize == LFI_ALWAYSLIMIT) ||
		 (filesize == LFI_ALWAYSLIMITLESS) ){
		hWnd = (HWND)filesize;
		filesize = NULL;
	}
	sizeL = GetFileSize(hFile, &sizeH);
	if ( (sizeL == MAX32) && ((result = GetLastError()) != NO_ERROR) ){
		CloseHandle(hFile);
		return result;
	}

	if ( sizeH != 0 ) sizeL = MAX32;
	if ( filesize != NULL ) *filesize = sizeL;
										// 読み込み準備 -----------------------
	if ( *image != NULL ){	// メモリは確保済み
		DWORD imgsize;

		useheap = FALSE;
		imgsize = *imagesize;
		if ( imgsize < sizeL ) sizeL = imgsize;
		if ( imgsize < (sizeL + margin) ) margin = imgsize - sizeL;
	}else{						// 内部確保
		if ( CheckLoadSize(hWnd, &sizeL) == FALSE ){
			CloseHandle(hFile);
			return ERROR_CANCELLED;
		}

		useheap = TRUE;
		if ( (*image = HeapAlloc(ProcHeap, 0, sizeL + margin)) == NULL ){
			result = GetLastError();
			CloseHandle(hFile);
			return result;
		}
	}
										// 読み込み & 0 padding ---------------
	result = ReadFile(hFile, *image, sizeL, &sizeL, NULL) ?
			NO_ERROR : GetLastError();
	CloseHandle(hFile);
	if ( result != NO_ERROR ){
		if ( IsTrue(useheap) ) HeapFree(ProcHeap, 0, *image);
		return result;
	}
	if ( margin != 0 ) memset(*image + sizeL, 0, margin);
	if ( imagesize != NULL ) *imagesize = sizeL;
	return NO_ERROR;
}

struct CHARSETLISTSTRUCT {
	const char *name;
	int size;
	int type;
} charsetlist[] = {
	{"SHIFT_JIS", 9,	CP__SJIS},
	{"X-SJIS", 6,		CP__SJIS},
	{"EUC", 3,			VTYPE_EUCJP},
	{"X-EUC", 5,		VTYPE_EUCJP},
	{"UTF-7", 5,		VTYPE_UTF7},
	{"UTF-8", 5,		CP_UTF8},
	{NULL, 0, 0}
};

// Win95/NT4 eng RTM は VTYPE_IBM / VTYPE_ANSI / VTYPE_SYSTEMCP のみ使用可能
UINT VTypeToCPlist[VTypeToCPlist_max] = {
	CP__US,		// VTYPE_IBM
	CP__LATIN1,	// VTYPE_ANSI
	CP__JIS,	// VTYPE_JIS
	CP_ACP,		// VTYPE_SYSTEMCP
	CP__SJIS,	// VTYPE_EUCJP // ※ 殆ど CP__SJIS で処理するため
	CP__UTF16L,	// VTYPE_UNICODE
	CP__UTF16B,	// VTYPE_UNICODEB
	CP__SJIS,	// VTYPE_SJISNEC
	CP__SJIS,	// VTYPE_SJISB
	CP__SJIS,	// VTYPE_KANA
	CP_UTF8,	// VTYPE_UTF8
	CP_UTF7,	// VTYPE_UTF7
};

void SkipSepA(char **ptr)
{
	for ( ; ; ){
		char c;

		c = **ptr;
		if ( (c != ' ') && (c != '\t') && (c != '\r') && (c != '\n') ) break;
		(*ptr)++;
	}
}

#define DEFAULTCODEPAGE	VTYPE_SYSTEMCP // wine のときは CP_UTF8 の方が良さそう
#define CODECHECKSIZE (2 * KB)

// 戻り値 : VTypeToCPlist 又は codepage
// SJIS の場合、GetACP == CP__SJIS なら VTYPE_SYSTEMCP。そうでなければ CP__SJIS
// UNICODE の場合、BOM有りなら VTYPE_UNICODE/VTYPE_UNICODEB/VTYPE_UTF8
//                 BOM無しなら CP__UTF16L/CP__UTF16B/CP_UTF8
PPXDLL int PPXAPI GetTextCodeType(const BYTE *image, DWORD size)
{
	const BYTE *bottom, *ptr, *maximage;
	int cnt; // utf-8の２バイト目以降のバイト数カウント & 7bit限定チェッカ

	if ( *image >= 0xef ){ // UNICODE ヘッダの可能性
		if ( memcmp(image, UTF8HEADER, UTF8HEADERSIZE) == 0 ){
			return VTYPE_UTF8;
		}
		if ( memcmp(image, UCF2HEADER, UCF2HEADERSIZE) == 0 ){
			return VTYPE_UNICODE;
		}
		if ( memcmp(image, UCF2BEHEADER, UCF2HEADERSIZE) == 0 ){
			return VTYPE_UNICODEB;
		}
	}

	if ( size > 0x4000 ) size = 0x4000;
										// charset 指定があるか
	maximage = image + ((size > CODECHECKSIZE) ? CODECHECKSIZE : size); // このチェックは控えめに。
	for ( ptr = image ; ptr < maximage ; ptr++ ){
		BYTE c;

		if ( (*ptr == 'c') && !memcmp(ptr + 1, "harset", 6) ){
			struct CHARSETLISTSTRUCT *cl;

			ptr += 7;
			SkipSepA((char **)&ptr);
			if ( *ptr != '=' ) continue;

			ptr++;
			SkipSepA((char **)&ptr);
			if ( *ptr == '\"' ){
				ptr++;
				SkipSepA((char **)&ptr);
			}
			for ( cl = charsetlist ; cl->name ; cl++ ){
				if ( memicmp(ptr, cl->name, cl->size) == 0 ){
					if ( (cl->type == CP__SJIS) && (GetACP() == CP__SJIS) ){
						return VTYPE_SYSTEMCP;
					}
					return cl->type;
				}
			}
			continue;
		}
		c = *ptr;
		// バイナリ相当か、8bit文字が検出されたら検索を終了する
		if ( (c < '\t') || (c >= 0x80) ) break;
	}
	// UNICODE/B 検出
	if ( (ptr < maximage) && (*ptr == '\0') ){ // 上位バイトっぽいのを発見
		size_t len;
		const BYTE *up;
		BYTE c;

		len = ptr - image;
		if ( len < (size - 4) && !(IsalnumA(*image) && Isalnum(*(image + 1))) ){
			up = image + (len & 1);
			c = 8; // tab
			while ( up < maximage ){	// 上位バイトのみ調査
				c = *up;
				up += 2;
				// 通常 UNICODEに存在しない文字を見つけたら中止
				// 2b-2dは一部(4.0↑)あり, d8-dfはサロゲートだが、ここでは×に
				if ( (c == 8) || (c == 0x1c) || ((c >= 0x2b) && (c <= 0x2d)) ||
					 ((c >= 0xd8) && (c <= 0xf8)) ){
					c = 8;
					break;
				}
			}
			if ( c != 8 ) return (len & 1) ? CP__UTF16L : CP__UTF16B;
		}
	}
										// UTF-8 チェック
	bottom = ptr; // 少なくとも tabcode～0x80 の範囲内であったところは省略する
	// maximage = image + size;
	cnt = -1;
	for ( ptr = bottom ; ptr < maximage ; ){
		BYTE c;

		c = *ptr++;
		if ( c < 0x80 ) continue;		// 00-7f
		if ( c < 0xc0 ) goto noutf8;	// 80-bf 範囲外
		if ( c < 0xe0 ){ cnt = 1;		// C0     80-     7ff
		}else if ( c < 0xf0 ){ cnt = 2;	// E0    800-    ffff
		}else if ( c < 0xf8 ){ cnt = 3;	// F0  10000-  1fffff
//		}else if ( c < 0xfc ){ cnt = 4;	// F8 200000- 3ffffff  *現在、規格で使
//		}else if ( c < 0xfe ){ cnt = 5;	// FC 4000000-7fffffff *用されていない
		}else goto noutf8;				// fe-ff 範囲外
										// ２バイト以降のチェック
		while ( cnt ){
			if ( (*ptr < 0x80) || (*ptr >= 0xc0 ) ) goto noutf8; // 範囲外
			ptr++;
			cnt--;
		}
	}

		// テキスト最後まで、cnt が変更されていない→7bit コードのみだった
	if ( cnt < 0 ) return DEFAULTCODEPAGE;
	return CP_UTF8;					// 調べた範囲では utf-8 だった

noutf8:
	for ( ptr = bottom ;; ){				// EUC-JP チェック
		BYTE c;

		if ( ptr >= maximage ) return VTYPE_EUCJP;
		c = *ptr++;
		if ( c < 0x80 ) continue;		// 00-7f
		if ( (c != 0x8e) && (c < 0xa1) ) break;	// SS2, 1bytes KANA 以外
										// ２バイト目チェック
		c = *ptr;
		if ( ((c < 0xa1) && c) || (c >= 0xff) ) break; // 範囲外
		ptr++;
	}

	if ( GetACP() != CP__SJIS ){		// Shift_JIS チェック(日本以外の時)
		for ( ptr = bottom ;; ){
			BYTE c;

			if ( ptr >= maximage ) return CP__SJIS;
			c = *ptr++;
			if ( c <= 0x80 ) continue;		// 00-80

			if ( (c >= 0xa0) && (c < 0xe0) ) continue; // a0-df 1byte(KANA)
										// ２バイト目チェック
			c = *ptr;
			if ( (c < 0x40) || (c == 0x7f) || (c >= 0xfe) ) break; // 範囲外
			ptr++;
		}
	}
	return DEFAULTCODEPAGE; // 不明→system codepage
}

// 新しいイメージを作成したときは、負の値
int FixTextImage(const char *src, DWORD memsize, TCHAR **dest, int usecp)
{
	int charcode;
	DWORD size;
	BYTE *tmpimage = NULL;
	UINT cp;
	DWORD flags;

	if ( usecp > 0 ){
		charcode = usecp;
	}else{
		charcode = GetTextCodeType((const BYTE *)src, memsize);
	}
	switch (charcode){
		case VTYPE_UNICODE: // BOM あり
		case VTYPE_UNICODEB:
		case CP__UTF16L: // BOM なし
		case CP__UTF16B:
			if ( (charcode == VTYPE_UNICODE) || (charcode == CP__UTF16L) ){
				if ( memcmp(src, UCF2HEADER, UCF2HEADERSIZE) == 0 ){
					src += UCF2HEADERSIZE;
				}
			}else{ // VTYPE_UNICODEB / CP__UTF16B
				WCHAR *udest;
				if ( memcmp(src, UCF2BEHEADER, UCF2HEADERSIZE) == 0 ){
					src += UCF2HEADERSIZE;
				}
				size = strlenW((WCHAR *)src) + 1;
				tmpimage = HeapAlloc(DLLheap, 0, size * sizeof(WCHAR));
				if ( tmpimage != NULL ){ // バイトオーダ変換
					udest = (WCHAR *)tmpimage;
					while ( size-- ){
						WCHAR lc;

						lc = *((WCHAR *)src);
						src += sizeof(WCHAR) / sizeof(char);
						*udest++ = (WCHAR)((lc >> 8) | (lc << 8));
					}
					#ifdef UNICODE
						*dest = (WCHAR *)tmpimage;
						return -charcode;
					#else
						src = (char *)tmpimage;
					#endif
				}
			}
		#ifdef UNICODE
			*dest = (TCHAR *)src;
			return charcode;
		#else
			break;
		#endif

		case VTYPE_UTF8: // BOM あり
		case CP_UTF8: // BOM なし
			if ( memcmp(src, UTF8HEADER, UTF8HEADERSIZE) == 0 ){
				src += UTF8HEADERSIZE;
			}
			break;

		case VTYPE_EUCJP: {
			BYTE *srcp, *dstp;
			int c, d;

			size = strlen32(src) + 1;
			tmpimage = HeapAlloc(DLLheap, 0, size);
			if ( tmpimage == NULL ) break;
			memcpy(tmpimage, src, size);
			dstp = srcp = (BYTE *)tmpimage;
			while ( *srcp != '\0' ){
				c = *srcp++;
				if ( c < 0x80 ){
					*dstp++ = (BYTE)c;
					continue;
				}
				if ( c == 0x8e ){		//SS2, 1bytes KANA
					*dstp++ = *srcp++;
					continue;
				}
				d = c - 0x80;
				c = *srcp++ - 0x80;

				if ( d & 1 ){
					if ( c < 0x60 ){
						c += 0x1F;
					}else{
						c += 0x20;
					}
				}else{
					c += 0x7E;
				}
				if ( d < 0x5F ){
					d = (d + 0xE1) >> 1;
				}else{
					d = (d + 0x161) >> 1;
				}

				*dstp++ = (unsigned char)d;
				*dstp++ = (unsigned char)c;
			}
			*dstp = '\0';
			#ifdef UNICODE
				src = (char *)tmpimage;
				break;
			#else
				*dest = (char *)tmpimage;
				return -charcode;
			#endif
		}
#if 0
		case VTYPE_JIS: {
			BYTE *srcp, *dstp;
			int c, d, jismode = 0;

			size = strlen32(src) + 1;
			tmpimage = HeapAlloc(DLLheap, 0, size);
			if ( tmpimage == NULL ) break;
			memcpy(tmpimage, src, size);
			dstp = srcp = (BYTE *)tmpimage;
			while ( *srcp != '\0' ){
				if ( jismode == 0 ){ // 非JIS
					while ( *srcp != '\0' ){
						c = *srcp++;
						if ( (c == '\x1b') && (*srcp == '$') && (*(srcp + 1) == 'B') ){
							jismode = 1;
							srcp += 2;
							break;
						}
						*dstp++ = (BYTE)c;
						continue;
					}
					continue;
				}
				while ( *srcp != '\0' ){ // JIS
					d = *srcp;
					c = *(srcp + 1);
					if ( (d < 0x21) || (d >= 0x7f) || (c < 0x21) || (c >= 0x7f) ){
						if ( (d == '\x1b') && (c == '(') && (*(srcp + 2) == 'B') ){
							jismode = 0;
							srcp += 3;
							break;
						}
						*dstp++ = (unsigned char)d;
						srcp += 1;
						break;
					}
					srcp += 2;

					if ( d & 1 ){
						if ( c < 0x60 ){
							c += 0x1F;
						}else{
							c += 0x20;
						}
					}else{
						c += 0x7E;
					}
					if ( d < 0x5F ){
						d = (d + 0xE1) >> 1;
					}else{
						d = (d + 0x161) >> 1;
					}
					*dstp++ = (unsigned char)d;
					*dstp++ = (unsigned char)c;
				}
			}
			*dstp = '\0';
			charcode = CP__SJIS;
			#ifdef UNICODE
				src = (char *)tmpimage;
				break;
			#else
				*dest = (char *)tmpimage;
				return -charcode;
			#endif
		}
#endif
	}
						// S-JIS 等の MultiByte
	cp = (charcode < VTypeToCPlist_max) ? VTypeToCPlist[charcode] : charcode;
	flags = (cp == CP_UTF8) ? 0 : MB_PRECOMPOSED;
#ifdef UNICODE
	size = MultiByteToWideCharU8(cp, flags, src, -1, NULL, 0);
	*dest = HeapAlloc(ProcHeap, 0, TSTROFF(size));
	if ( *dest == NULL ){
		if ( tmpimage != NULL ){
			*dest = (TCHAR *)tmpimage;
			return -charcode;
		}else{
			*dest = (TCHAR *)src;
			return charcode;
		}
	}
	MultiByteToWideCharU8(cp, flags, src, -1, *dest, size);
	if ( tmpimage != NULL ) HeapFree(ProcHeap, 0, tmpimage);
	return -charcode;

#else // Multibyte
	if ( charcode == VTYPE_SYSTEMCP ){
		*dest = (char *)src;
		return charcode;
	}else{
		WCHAR *srcW;
		BOOL reqfree;

		if ( (cp != CP__UTF16L) && (cp != CP__UTF16B) ){ // 一旦UNICODEに変換
			size = MultiByteToWideCharU8(cp, flags, src, -1, NULL, 0);
			srcW = HeapAlloc(ProcHeap, 0, size * sizeof(WCHAR));
			if ( srcW == NULL ){
				*dest = (TCHAR *)src;
				return charcode;
			}
			MultiByteToWideCharU8(cp, flags, src, -1, srcW, size);
			reqfree = TRUE;
		}else{
			srcW = (WCHAR *)src;
			reqfree = FALSE;
		}
		// system cp に変換
		size = WideCharToMultiByte(CP_ACP, 0, srcW, -1, NULL, 0, NULL, NULL);
		*dest = HeapAlloc(ProcHeap, 0, size);
		if ( *dest == NULL ){
			HeapFree(ProcHeap, 0, srcW);
			*dest = (TCHAR *)src;
			if ( tmpimage != NULL ) HeapFree(ProcHeap, 0, tmpimage);
			return charcode;
		}
		WideCharToMultiByte(CP_ACP, 0, srcW, -1, *dest, size, NULL, NULL);
		if ( IsTrue(reqfree) ) HeapFree(ProcHeap, 0, srcW);
		if ( tmpimage != NULL ) HeapFree(ProcHeap, 0, tmpimage);
		return -charcode;
	}
#endif
}

PPXDLL ERRORCODE PPXAPI LoadTextImage(const TCHAR *filename, TCHAR **image, TCHAR **readpoint, TCHAR **maxptr)
{
	ERRORCODE result;
	TCHAR *newimage;
	DWORD size;

	if ( filename != NULL ){
		*image = NULL;
		result = LoadFileImage(filename, 0x40, (char **)image, &size, (maxptr != NULL) ? LFI_ALWAYSLIMITLESS : NULL);
		if ( result != NO_ERROR ) return result;
	}else{
		size = TSTROFF32(*maxptr - *image);
	}

	if ( FixTextImage((char *)*image, size, &newimage, 0) < 0 ){
		HeapFree(ProcHeap, 0, *image);
		*image = newimage;
	}
	*readpoint = newimage;
	if ( maxptr != NULL ) *maxptr = newimage + tstrlen(newimage);
	return NO_ERROR;
}

BOOL MakeTempEntrySub(TCHAR *tempath, DWORD attribute)
{
	TCHAR buf[VFPS], *p;
	int count = 16;

	tstrcpy(buf, tempath);
					// 長さ制限を行う
	p = buf;
	while( *p != '\0' ){
		if ( count-- <= 0 ){
			*p = '\0';
			break;
		}
		p += Ismulti(*p) ? 2 : 1;
	}

	CatPath(tempath, TempPath, buf);
	GetUniqueEntryName(tempath);
	if ( attribute & FILE_ATTRIBUTE_DIRECTORY ){
		return CreateDirectory(tempath, NULL);
	}
	return TRUE;
}

PPXDLL BOOL PPXAPI MakeTempEntry(DWORD bufsize, TCHAR *tempath, DWORD attribute)
{
	DWORD seed;
	int pathlen, i;

	if ( TempPath[0] == '\0' ){
		GetTempPath(MAX_PATH, TempPath);
		if ( MakeTempEntry(MAX_PATH, TempPath, FILE_ATTRIBUTE_DIRECTORY) == FALSE ){
			tstrcpy(TempPath, StrDummyTempPath); // TempPath が生成できなかったので、決め打ちでディレクトリを決める
		}
	}
	// 2: "\\" と "\n" 8:filename 4:ext
	if ( (tstrlen(TempPath) + ( 2 + 8 + 4 )) >= (size_t)bufsize ){
		if ( bufsize >= TSIZEOF(StrDummyTempPath) ){
			tstrcpy(tempath, StrDummyTempPath);
		}
		return FALSE;
	}

	if ( attribute & FILE_ATTRIBUTE_LABEL ){
		return MakeTempEntrySub(tempath, attribute);
	}

	CatPath(tempath, TempPath, NilStr);

	if ( attribute & FILE_ATTRIBUTE_COMPRESSED ){
		resetflag(attribute, FILE_ATTRIBUTE_COMPRESSED);
		if ( (NO_ERROR == GetCustTable(StrCustOthers, T("ExtractTemp"), tempath, MAX_PATH)) ||
			(GetEnvironmentVariable(StrTempExtractPath, tempath, MAX_PATH) != 0) ){
			CatPath(NULL, tempath, NilStr);
		}
	}
	if ( attribute == 0 ) return TRUE;

	pathlen = tstrlen32(tempath);
	seed = GetTickCount();
	for ( i = 0 ; i < 0x10000 ; i++ ){
		ERRORCODE result;

		wsprintf(tempath + pathlen, T("PPX%X.TMP"), LOWORD(seed));
		if ( attribute & FILE_ATTRIBUTE_DIRECTORY ){
			if ( IsTrue(CreateDirectory(tempath, NULL)) ) return TRUE;
		}else{
			HANDLE hF;

			hF = CreateFile(tempath, GENERIC_WRITE, 0, NULL, CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL, NULL);
			if ( hF != INVALID_HANDLE_VALUE ){
				CloseHandle(hF);
				return TRUE;
			}
		}
		result = GetLastError();

		if ( result == ERROR_PATH_NOT_FOUND ){
			if ( CreateDirectory(TempPath, NULL) == FALSE ) break;
			continue;
		}
		if ( result != ERROR_FILE_EXISTS ){
			if ( GetFileAttributes(tempath) == BADATTR ) break;
		}
		seed++;
	}
	return FALSE;
}

BOOL DeleteDirectories(const TCHAR *path, BOOL notify)
{
	TCHAR buf[VFPS];
	WIN32_FIND_DATA	ff;
	HANDLE hFF;

	if ( !(GetFileAttributes(path) & FILE_ATTRIBUTE_REPARSE_POINT) ){
		CatPath(buf, (TCHAR *)path, WildCard_All);
		hFF = FindFirstFileL(buf, &ff);
		if ( INVALID_HANDLE_VALUE != hFF ){
			do{
				if ( IsRelativeDirectory(ff.cFileName) ) continue;
				CatPath(buf, (TCHAR *)path, ff.cFileName);

				if ( ff.dwFileAttributes & FILE_ATTRIBUTE_READONLY ){
					SetFileAttributesL(buf, FILE_ATTRIBUTE_NORMAL);
				}
				if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
					DeleteDirectories(buf, FALSE);
				}else{
					DeleteFileL(buf);
				}
			}while(FindNextFile(hFF, &ff));
			FindClose(hFF);
		}
	}
	if( IsTrue(RemoveDirectoryL(path)) ){
		if ( IsTrue(notify) ) SHChangeNotify(SHCNE_RMDIR, SHCNF_PATH, path, NULL);
		return TRUE;
	}
	return FALSE;
}

PPXDLL const TCHAR * PPXAPI PPxGetSyncTag(void)
{
	return SyncTag;
}

#if 0
struct GETCHILD {
	RECT box;
	HWND hWnd;
	POINT pos;
};

BOOL CALLBACK GetWindowFromPointProc(HWND hWnd, LPARAM gc)
{
	RECT box;

	if ( !IsWindowVisible(hWnd) ) return TRUE; // HIDE状態
	GetWindowRect(hWnd, &box);
	if (PtInRect(&box, ((struct GETCHILD *)gc)->pos) &&
			(((struct GETCHILD *)gc)->box.left   <= box.left  ) &&
			(((struct GETCHILD *)gc)->box.top    <= box.top   ) &&
			(((struct GETCHILD *)gc)->box.right  >= box.right ) &&
			(((struct GETCHILD *)gc)->box.bottom >= box.bottom) ){
		((struct GETCHILD *)gc)->hWnd = hWnd;
	}
	return TRUE;
}

/* 指定座標のウィンドウを厳密に抽出する sPos:screen*/
HWND GetChildWindowFromPoint(HWND hWnd, POINT *pos)
{
	struct GETCHILD gc;
	HWND hPWnd;
										// まず、親を検索
	gc.hWnd = hWnd;
	if ( gc.hWnd == NULL ) return gc.hWnd;
							// 子を検索、隠れているのを探すため、
							// ChildWindowFromPoint をあえて使わない
	GetWindowRect(gc.hWnd, &gc.box);
	gc.pos = *pos;
	if ( !(GetWindowLong(gc.hWnd, GWL_STYLE) & ((WS_CAPTION | WS_SYSMENU) & ~WS_BORDER)) ){
		hPWnd = GetParent(gc.hWnd);
		if ( hPWnd != NULL ) gc.hWnd = hPWnd;
	}
	EnumChildWindows(gc.hWnd, GetWindowFromPointProc, (LPARAM)&gc);
	return gc.hWnd;
}
#endif


// 自か親のタイトル付きウィンドウを取得する -----------------------------------
HWND GetCaptionWindow(HWND hWnd)
{
	// タイトルバーを持っているかを確認
	while ( !(GetWindowLong(hWnd, GWL_STYLE) & (WS_CAPTION & ~WS_BORDER)) ){
		HWND htWnd;

		htWnd = GetParent(hWnd);
		if ( htWnd == NULL ) break;
		hWnd = htWnd;
	}
	return hWnd;
}

// 親ウィンドウ(タイトル付き)を推測する ---------------------------------------
HWND GetParentCaptionWindow(HWND hWnd)
{
	HWND nhWnd;

	nhWnd = hWnd;
	while ( (nhWnd = GetParent(nhWnd)) != NULL ){
		if ( GetWindowLong(nhWnd, GWL_STYLE) & (WS_CAPTION & ~WS_BORDER) ){
			return nhWnd;
		}
	}
	nhWnd = GetParent(hWnd);
	return nhWnd;
}

// ウィンドウを親ウィンドウのまん中に移動させる -------------------------------
PPXDLL void PPXAPI CenterWindow(HWND hWnd)
{
	MoveCenterWindow(hWnd, GetParentCaptionWindow(hWnd));
}

PPXDLL void PPXAPI MoveCenterWindow(HWND hWnd, HWND hParentWnd)
{
	RECT box, pbox, desk;
	int parentheight;

	GetDesktopRect( (hParentWnd != NULL) ? hParentWnd : hWnd, &desk);
	GetWindowRect(hWnd, &box);
	if ( hParentWnd != NULL ){
		GetWindowRect(hParentWnd, &pbox);
	}else{
		pbox = desk;
	}
								// 幅と高さに変換
	box.right -= box.left;
	box.bottom -= box.top;

								// 左右を中央に移動
	box.left = pbox.left + (((pbox.right  - pbox.left) - box.right) / 2);

	parentheight = pbox.bottom - pbox.top;
	if ( parentheight > box.bottom ){ // 親の方が背が高い…上下を中央に移動
		box.top  = pbox.top  + ((parentheight - box.bottom) / 2);
	}else{ // 親の方が背が低い…親の下に移動
		box.top  = pbox.bottom - (parentheight / 3);
	}
								// 左右がはみ出しているか？
	if ( (box.left + box.right) > desk.right ){
		box.left = desk.right - box.right;
	}
	if ( box.left < desk.left ) box.left = desk.left;
								// 上下がはみ出しているか？
	if ( (box.top + box.bottom) > desk.bottom ){
		box.top = desk.bottom - box.bottom;
	}
	if ( box.top < desk.top ) box.top = desk.top;

	MoveWindow(hWnd, box.left, box.top, box.right, box.bottom, TRUE);
}

typedef struct {
	DWORD dwSize;
	DWORD dwICC;
} INITCOMMONCONTROLSEXSTRUCT;
INITCOMMONCONTROLSEXSTRUCT UseCommonControls = {
	sizeof(INITCOMMONCONTROLSEXSTRUCT), 0
};

//  -------------------------------
PPXDLL HANDLE PPXAPI LoadCommonControls(DWORD usecontrol)
{
	DefineWinAPI(void, InitCommonControls, (void));
	DefineWinAPI(void, InitCommonControlsEx, (INITCOMMONCONTROLSEXSTRUCT *));

	if ( (X_dss == DSS_NOLOAD) && (WinType >= WINTYPE_VISTA) ){
		GetCustData(T("X_dss"), &X_dss, sizeof(X_dss));
	}

	if ( hComctl32 != NULL ){	// 実行済み
		if ( UseCommonControls.dwICC == 0 ) return hComctl32; // no Ex
	}else{
		hComctl32 = LoadSystemDLL(SYSTEMDLL_COMCTL32);
		if ( hComctl32 == NULL ) return NULL;
		GETDLLPROC(hComctl32, InitCommonControlsEx);
		if ( DInitCommonControlsEx != NULL ){
			UseCommonControls.dwICC = 0;
		}else{
			GETDLLPROC(hComctl32, InitCommonControls);
			if ( DInitCommonControls != NULL ) DInitCommonControls();
			return hComctl32;
		}
	}
	if ( (UseCommonControls.dwICC & usecontrol) != usecontrol ){	// まだ未ロードがある
		setflag(UseCommonControls.dwICC, usecontrol);
		GETDLLPROC(hComctl32, InitCommonControlsEx);
		DInitCommonControlsEx(&UseCommonControls);
	}
	return hComctl32;
}

#pragma argsused
BOOL CALLBACK EnumChildTextFixProc(HWND hWnd, LPARAM lParam)
{
	TCHAR name[8];
	const TCHAR *findtext;
	UnUsedParam(lParam);

	wsprintf(name, T("%04X"), GetWindowLongPtr(hWnd, GWLP_ID));
	findtext = SearchMessageText(name);
	if ( findtext != NULL ) SetWindowText(hWnd, findtext);
	return TRUE;
}

PPXDLL void PPXAPI LocalizeDialogText(HWND hDlg, DWORD titleID)
{
	if ( MessageTextTable == NULL ) LoadMessageTextTable();
	if ( MessageTextTable != NOMESSAGETEXT ){
		if ( titleID != 0 ){
			TCHAR name[8];
			const TCHAR *findtext;

			wsprintf(name, T("%04X"), titleID);
			findtext = SearchMessageText(name);
			if ( findtext != NULL ) SetWindowText(hDlg, findtext);
		}
		EnumChildWindows(hDlg, EnumChildTextFixProc, 0);
	}
}

/* ● 1.46+3 未使用みたいなので廃止
PPXDLL void PPXAPI MessageTextDialog(HWND hDlg)
{
	LocalizeDialogText(hDlg, 0);
}
*/

BOOL CheckAjiParam(DWORD mode, const TCHAR *type, const TCHAR *name, TCHAR *param)
{
	TCHAR keyword[64];

	wsprintf(keyword, T("%s%s%s"),
			name, (mode == AJI_SHOW) ? T("show") : T("comp"), type);
	GetCustTable(T("X_jinfc"), keyword, param, CMDLINESIZE);
	if ( param[0] != '\0' ) return TRUE;
	GetCustTable(T("X_jinfc"), keyword + tstrlen(name), param, CMDLINESIZE);
	if ( param[0] != '\0' ) return TRUE;
	return FALSE;
}

PPXDLL void PPXAPI ActionJobInfo(HWND hWnd, DWORD mode, const TCHAR *name)
{
	ActionInfo(hWnd, NULL, mode, name);
}

PPXDLL void PPXAPI ActionInfo(HWND hWnd, PPXAPPINFO *info, DWORD mode, const TCHAR *name)
{
	HWND hFWnd;
	DWORD flash, command;
	TCHAR param[CMDLINESIZE];

	if ( AjiEnterCount > 5 ) return;
	AjiEnterCount++;

	if ( X_jinfo[0] == MAX32 ){
		X_jinfo[0] = 1;
		GetCustData(T("X_jinfo"), X_jinfo, sizeof(X_jinfo));
	}
	hFWnd = GetForegroundWindow();
	flash = X_jinfo[mode * 2];
	if ( (flash > 1) || ((flash == 1) && (hWnd != hFWnd)) ){
		PPxFlashWindow(hWnd, PPXFLASH_NOFOCUS);
	}
	command = X_jinfo[mode * 2 + 1];
	if ( (command > 1) || ((command == 1) && (hWnd != hFWnd)) ){
		param[0] = '\0';
		if ( IsTrue(CheckAjiParam(mode, T("cmd"), name, param)) ){
			PP_ExtractMacro(hWnd, info, NULL, param, NULL, 0);
		}else{
			CheckAjiParam(mode, T("wav"), name, param);
			if ( name[0] != '\0' ) PlayWave(param);
		}
	}
	AjiEnterCount--;
}

PPXDLL BOOL PPXAPI GetCalc(const TCHAR *param, TCHAR *resultstr, int *resultnum)
{
	int num, i, j;
	const TCHAR *ptr;

	ptr = param;
	if ( CalcString(&ptr, &num) != CALC_NOERROR ) return FALSE;

	if ( resultnum != NULL ) *resultnum = num;
	if ( resultstr != NULL ){
		char str[6], *bptr;
#ifdef UNICODE
		WCHAR wstr[6];
		#define usestr wstr
#else
		#define usestr str
#endif

		bptr = str;
		j = num;
		for ( i = 0 ; i < 4 ; i++ ){
			*bptr = (char)j;
			if ( (BYTE)*bptr < (BYTE)' ' ) *bptr = '.';
			bptr++;
			j >>= 8;
		}
		str[4] = '\0';
		str[5] = '\0';
#ifdef UNICODE
		AnsiToUnicode(str, wstr, 6);
#endif
		wsprintf(resultstr, T("D:%ld X:%x U:%lu %s"), num, num, num, usestr);
	}
	return TRUE;
}

PPXDLL void PPXAPI FixCharlengthTable(char *table)
{
	if ( LoadedCharlengthTable == FALSE ){
		TCHAR localebuf[16];
		LCID userlcid;

		LoadedCharlengthTable = TRUE;

		if ( GetACP() != CP__SJIS ){
			char chrcode[] = "\x01\x41";
			BYTE *p;
			int i;

			p = (BYTE *)&T_CHRTYPE[1];
			for ( i = 1 ; i < 0x100 ; i++, p++ ){
				chrcode[0] = (char)i;
				*p = (BYTE)((*p & ~3) | (CharNextA(chrcode) - chrcode));
			}
		}
		userlcid = GetUserDefaultLCID();
		GetLocaleInfo(userlcid, LOCALE_STHOUSAND, localebuf, TSIZEOF(localebuf));
		NumberUnitSeparator = localebuf[0];
		GetLocaleInfo(userlcid, LOCALE_SDECIMAL, localebuf, TSIZEOF(localebuf));
		NumberDecimalSeparator = localebuf[0];
	}
	if ( table != NULL ) memcpy(table, T_CHRTYPE, 0x100);
}
#ifndef TokenElevationType
#define TokenElevationType 18
#endif
typedef enum  {
  xTokenElevationTypeDefault = 1,
  xTokenElevationTypeFull,
  xTokenElevationTypeLimited
} xTOKEN_ELEVATION_TYPE;

const TCHAR ElevationString[] = T("Elevate");
const TCHAR LimitString[] = T("Limit");

PPXDLL const TCHAR * PPXAPI CheckRunAs(void)
{
	if ( Sm->RunAsMode == RUNAS_NOCHECK ){
		// 別ユーザか確認
		if ( Sm->RunAsMode == RUNAS_NOCHECK ){
			HANDLE hProcess = NULL;
			HANDLE hToken = NULL;

			Sm->RunAsMode = RUNAS_NORMAL;
			#ifndef UNICODE
			if ( WinType != WINTYPE_9x )
			#endif
			for ( ;; ) {
				TCHAR tinfo[0x40], user[0x80], domain[0x80];
				DWORD pid, size, dsize;
				SID_NAME_USE snu;
				HWND hWnd;

				hWnd = FindWindow(T("Progman"), NULL);
				if ( hWnd == NULL ) break;
				GetWindowThreadProcessId(hWnd, &pid);

				Sm->RunAsMode = RUNAS_RUNAS;
				hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
				if ( hProcess == NULL ) break;

				if ( OpenProcessToken(hProcess, TOKEN_QUERY, &hToken) == FALSE){
					break;
				}
				if ( GetTokenInformation(hToken, TokenUser, tinfo,
							sizeof tinfo, &size) == FALSE ){
					break;
				}
				size = TSIZEOF(user);
				dsize = TSIZEOF(domain);
				if ( LookupAccountSid(NULL, ((PTOKEN_USER)tinfo)->User.Sid,
						user, &size, domain, &dsize, &snu) == FALSE ){
					break;
				}
				if ( !tstricmp(user, UserName) ) Sm->RunAsMode = RUNAS_NORMAL;
				break;
			}
			if ( hToken != NULL ) CloseHandle(hToken);
			if ( hProcess != NULL ) CloseHandle(hProcess);
		}
	}

	// UAC 状態を取得
	if ( WinType >= WINTYPE_VISTA ){
		HANDLE hCurToken;

		if ( OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hCurToken) ){
			DWORD tetsize = 0;
			xTOKEN_ELEVATION_TYPE tet = xTokenElevationTypeDefault;

			GetTokenInformation(hCurToken, TokenElevationType, &tet, sizeof(tet), &tetsize);
			CloseHandle(hCurToken);
			if ( tet == xTokenElevationTypeFull ){ // UAC 昇格状態
				return ElevationString;
			}else if ( tet == xTokenElevationTypeLimited ){
				// UAC 制限状態
				if ( GetCustDword(T("X_dlim"), 1) ){
					return LimitString;
				}
			}
		}
	}

	if ( Sm->RunAsMode <= RUNAS_NORMAL ) return NULL;
	return UserName;
}

void LoadErrorWinAPI(const char *DLLname)
{
	#ifdef UNICODE
		WCHAR nameW[MAX_PATH];

		AnsiToUnicode(DLLname, nameW, MAX_PATH);
		XMessage(NULL, NULL, XM_GrERRld, T("%s loaderror"), nameW);
	#else
		XMessage(NULL, NULL, XM_GrERRld, T("%s loaderror"), DLLname);
	#endif
}

PPXDLL HMODULE PPXAPI LoadWinAPI(const char *DLLname, HMODULE hDLL, LOADWINAPISTRUCT *apis, int mode)
{
	LOADWINAPISTRUCT *list;

	if ( mode & LOADWINAPI_LOAD ){
		hDLL = LoadLibraryA(DLLname);
		if ( hDLL == NULL ){
			if ( mode == LOADWINAPI_LOAD_ERRMSG ) LoadErrorWinAPI(DLLname);
			return NULL;
		}
	}else if ( mode == LOADWINAPI_GETMODULE ){
		hDLL = GetModuleHandleA(DLLname);
		if ( hDLL == NULL ) return NULL;
	}
	list = apis;
	while ( list->APIname != NULL ){
		*list->APIptr = (void (WINAPI *)())GetProcAddress(hDLL, list->APIname);
		if ( *list->APIptr == NULL ){
			if ( mode & LOADWINAPI_LOAD ){
				while ( apis != list ){
					*apis->APIptr = NULL;
					apis++;
				}
				FreeLibrary(hDLL);
			}
			return NULL;
		}
		list++;
	}
	return hDLL;
}

const TCHAR *SystemDLLlist[] = {
	T("COMDLG32.DLL"),
	T("COMCTL32.DLL"),
	T("OLEAUT32.DLL"),
	T("MPR.DLL"),
	T("WINMM.DLL"),
	T("IMAGEHLP.DLL"),
//	OLEPRO32
	T("PSAPI.DLL"),
	T("shcore.dll"),
	T("SHLWAPI.DLL"),
	T("RSTRTMGR.DLL"),
	T("ATL.DLL"),
	T("ATL100.DLL"),
//	T("DwmapiName.DLL");
	// susie - fullpath - LOAD_WITH_ALTERED_SEARCH_PATH が必要
	// ppxmodule
//	shcore
//	d2d1
//	dwrite
//	zip
};
#define xLOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
int LoadDLLflags = 1;
PPXDLL HMODULE PPXAPI LoadSystemDLL(DWORD dllID)
{
	if ( LoadDLLflags == 1 ){
		if ( (WinType >= WINTYPE_8) || (GetProcAddress(hKernel32, "SetDefaultDllDirectories") != NULL) ){
			LoadDLLflags = xLOAD_LIBRARY_SEARCH_SYSTEM32; // SetDefaultDllDirectories がない Win7/Vista では例外が発生する
		}else{
			LoadDLLflags = 0;
		}
	}
	return LoadLibraryEx(SystemDLLlist[dllID], NULL, LoadDLLflags);
}

HMODULE LoadSystemWinAPI(DWORD dllID, LOADWINAPISTRUCT *apis)
{
	HMODULE hDLL, hDLLresult;

	hDLL = LoadSystemDLL(dllID);
	if ( hDLL == NULL ) return NULL;
	hDLLresult = LoadWinAPI(NULL, hDLL, apis, LOADWINAPI_HANDLE);
	if ( hDLLresult == NULL ){
		FreeLibrary(hDLL);
		hDLL = NULL;
	}
	return hDLL;
}

void USEFASTCALL SetDlgFocus(HWND hDlg, int id)
{
	SetFocus(GetDlgItem(hDlg, id));
}

//------------------------------------- 文字列をバイナリに変換
DWORD ReadEString(TCHAR **string, void *destptr, DWORD destsize)
{
	TCHAR *r;
	BYTE *w;
	DWORD size = 0;

	r = *string;
	w = destptr;
	while( destsize ){
		DWORD seed;
		int i, l;

		seed = 0;
		for ( l = 0 ; l < 4 ; l++ ){
			BYTE c;

			c = (BYTE)*r;
			if ( (c < '0') || (c > ('z' + 1)) ) break;
			r++;
			if ( c >= 'a' ){
				c -= (BYTE)('a' - 10);
			}else if ( c >= 'A' ){
				c -= (BYTE)('A' -(10 + 27));
			}else{
				c -= (BYTE)'0';
			}
			seed = seed | ((DWORD)c << (l * 6));
		}
		l--;
		for ( i = 0 ; i < l ; i++ ){
			*w++ = (BYTE)seed;
			seed >>= 8;
			size++;
			destsize--;
			if ( destsize == 0 ) break;
		}
		if ( l < 3 ) break;
	}
	for ( ; destsize ; destsize-- ) *w++ = 0;

	*string = r;
	return size;
}
//------------------------------------- バイナリを文字列に変換
void WriteEString(TCHAR *dest, BYTE *src, DWORD srcsize)
{
	while( srcsize ){
		DWORD seed;
		int i, l;

		seed = 0;
		for ( l = 0 ; l < 3 ; l++ ){
			seed = seed | (*src << (8 * l));
			src++;
			srcsize--;
			if ( srcsize == 0 ){
				l++;
				break;
			}
		}
		l++;
		for ( i = 0 ; i < l ; i++ ){
			TCHAR r;

			r = (TCHAR)(seed & 0x3f) ;
			seed >>= 6;
			if ( r < 10 ){
				r += (TCHAR)'0';
			}else if ( r < (10 + 27) ){
				r += (TCHAR)('a' - 10);			// 意図的
			}else{
				r += (TCHAR)('A' -(10 + 27));	// 意図的
			}
			*dest++ = r;
		}
	}
	*dest = '\0';
}

WORD FixCharKeycode(WORD key)
{
	if ( IslowerA(key) ) key -= (WORD)0x20;	// 小文字
	if ( IsalnumA(key) || (key <= 0x20) ){
		key |= (WORD)GetShiftKey();
	}else{
		key |= (WORD)(GetShiftKey() & ~K_s);
	}
																// ctrl + char
	if ( (key & K_c) && ((key & 0xff) < 0x20) ) key += (WORD)0x40;
	return key;
}

const LONG crc32_half[16] = {
	0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
	0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
	0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
	0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

DWORD crc32(const BYTE *bin, DWORD size, DWORD r)
{
	DWORD crc = ~r;
	const BYTE *ptr = bin;
	BYTE chr;

	if ( size == MAX32 ) size = TSTRLENGTH32((TCHAR *)bin);
	while ( size-- ){
		chr = *ptr++;
		crc = crc32_half[(crc ^  chr      ) & 0x0F] ^ (crc >> 4);
		crc = crc32_half[(crc ^ (chr >> 4)) & 0x0F] ^ (crc >> 4);
	}
	return ~crc;
}

#ifdef UNICODE
BOOL WriteFileZT(HANDLE fileh, const WCHAR *str, DWORD *wrote)
{
	char bufA[WriteFileZTSIZE];

	UnicodeToAnsi(str, bufA, WriteFileZTSIZE);
	return WriteFile(fileh, bufA, strlen32(bufA), wrote, NULL);
}

// アラインメントがあっていないテキストをリストやコンボボックスに登録
LRESULT SendUTextMessage_U(HWND hWnd, UINT uMsg, WPARAM wParam, const TCHAR *text)
{
	TCHAR buf[0x1000];

	if ( !((DWORD)(DWORD_PTR)text & 1) ){
		return SendMessage(hWnd, uMsg, wParam, (LPARAM)text);
	}else {  // アライメントがずれている
		strcpyW(buf, text);
		return SendMessage(hWnd, uMsg, wParam, (LPARAM)buf);
	}
}
#endif

void GetPopMenuPos(HWND hWnd, POINT *pos, WORD key)
{
	RECT box;

	GetMessagePosPoint(*pos);
	if ( key & K_mouse ) return;
	GetWindowRect(hWnd, &box);
	if ( (box.left > pos->x) || (box.top > pos->y) ||
		 (box.right < pos->x) || (box.bottom < pos->y) ){
		pos->x = (box.left + box.right) / 2;
		pos->y = (box.top + box.bottom) / 2;
	}
}

int GetNowTime(TCHAR *text, int mode)
{
	SYSTEMTIME nowTime;

	GetLocalTime(&nowTime);
	return wsprintf(text,
			(mode == 0) ?	T("%04d-%02d-%02d %02d:%02d:%02d") :
							T("%04d-%02d-%02d"),
			nowTime.wYear, nowTime.wMonth, nowTime.wDay,
			nowTime.wHour, nowTime.wMinute, nowTime.wSecond);
}

BOOL PPRecvExecuteByWMCopyData(PPXAPPINFO *info, COPYDATASTRUCT *copydata)
{
	TCHAR cmd[CMDLINESIZE], *cmdbuf;

	if ( copydata->cbData >= 0x7fffffff ) return FALSE;
	if ( copydata->cbData > TSTROFF(CMDLINESIZE) ){
		cmdbuf = HeapAlloc(GetProcessHeap(), 0, copydata->cbData);
		if ( cmdbuf == NULL ) return FALSE;
	}else{
		cmdbuf = cmd;
	}
	memcpy(cmdbuf, copydata->lpData, copydata->cbData);
	if ( LOWORD(copydata->dwData) == 'H' ) ReplyMessage(TRUE);
	PP_ExtractMacro(info->hWnd, info, NULL, cmdbuf, NULL, 0);
	if ( cmdbuf != cmd ) HeapFree(GetProcessHeap(), 0, cmdbuf);
	return TRUE;
}

BOOL PPWmCopyData(PPXAPPINFO *info, COPYDATASTRUCT *copydata)
{
	if ( LOWORD(copydata->dwData) == 'H' ){ // コマンド実行(非同期)
		return PPRecvExecuteByWMCopyData(info, copydata);
	}
	return FALSE;
}
