/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		Main
-----------------------------------------------------------------------------*/
#define ONPPXDLL // PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "TMEM.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#pragma hdrstop
#include "PPD_GVAR.C" // グローバルの実体定義

FM_H SM_tmp;	// 作業用共有メモリ
FM_H SM_hist;	// ヒストリ用共有メモリ
BOOL InitProcessDLL(void);

/*-----------------------------------------------------------------------------
						pplib.dll の初期化／終了処理
-----------------------------------------------------------------------------*/
const TCHAR usernamestr[] = T("%USERNAME%");
const TCHAR MSG_SHAREBLOCKERROR[] = T("Share block create error");
const TCHAR MSG_DLLVERSIONERROR[] = T(PPCOMMONDLL) T(" version error");
const TCHAR MSG_HITORYAREAERROR[] = T("History file open error");
const TCHAR MSG_HISTORYIDCOLLAPSED[] = T("History ID collapsed, initialize now.\n(ヒストリ領域破損の為、初期化)");
const TCHAR MSG_HISTORYCOLLAPSED[] = T("History structure collapsed, fix now.\n(ヒストリ領域破損の為、初期化)");
const TCHAR MSG_CUSTOMIZETAREAERROR[] = T("Customize file open error");
const TCHAR MSG_CUSTOMIZEIDCOLLAPSED[] = T("Customize ID collapsed, initialize now.\n(カスタマイズ領域破損の為、初期化)");
const TCHAR MSG_CUSTOMIZESTRUCTUREBROKEN[] = T("Customize structure broken, fix now.\n(カスタマイズ領域破損の為、一部設定を廃棄)");
#if !defined(_WIN64) && defined(UNICODE)
const TCHAR MSG_NONTERROR[] = T("UNICODE版です。Windows9xでは動作しません。");
#endif

#pragma argsused
BOOL WINAPI DLLEntry(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
	UnUsedParam(reserved);

	switch (reason){
		// DLL 初期化 ---------------------------------------------------------
		case DLL_PROCESS_ATTACH:
			DLLhInst = hInst;
			GetModuleFileName(hInst,DLLpath,TSIZEOF(DLLpath));
			InitializeCriticalSection(&ThreadSection);
			return InitProcessDLL();

		// DLL の終了処理 -----------------------------------------------------
		case DLL_PROCESS_DETACH:
			PPxCommonCommand(NULL,0,K_CLEANUP);
			Sm = NULL;
			FileMappingOff(&SM_cust);
			FileMappingOff(&SM_hist);
			FileMappingOff(&SM_tmp);

			#ifndef _WIN64
			if ( DRemoveVectoredExceptionHandler == NULL ){
				SetUnhandledExceptionFilter(FUNCCAST(LPTOP_LEVEL_EXCEPTION_FILTER,UEFvec));
			}else
			#endif
			{
				DRemoveVectoredExceptionHandler(UEFvec);
			}
			DeleteCriticalSection(&ThreadSection);
			break;
/*
		case DLL_THREAD_ATTACH:
			GetThreadInfo();
			break;
*/
	}
	return TRUE;
}

BOOL InitProcessDLL(void)
{
	TCHAR buf[VFPS];
	TCHAR buf2[VFPS];
	DWORD tempsize;

	StartTick = GetTickCount();
								// OS 情報の取得 ======================
	OSver.dwOSVersionInfoSize = sizeof(OSver);
	GetVersionEx(&OSver);

	If_WinNT_Block {
		WinType = (OSver.dwMajorVersion << 16) | (WORD)OSver.dwMinorVersion;
	}
	#if !defined(_WIN64) && defined(UNICODE)
		if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
			CriticalMessageBox(MSG_NONTERROR);
			return FALSE;
		}
	#endif
	if ( WinType != WINTYPE_VISTA ){
		tempsize = TSIZEOF(UserName);
		if( GetUserName(UserName,&tempsize) == FALSE ){
			UserName[0] = '\0';
		}
	}else{ // Vistaではこの段階でGetUserNameを使うと,runasによる実行時+
		// その後の GetUserName 実行時にアクセス違反が発生する。
		// そのため、環境変数経由で取得してごまかし中…
		UserName[0] = '\0';
		ExpandEnvironmentStrings(usernamestr,UserName,TSIZEOF(UserName));
	}
								// DLL 情報の取得 =====================
	*(tstrrchr(DLLpath,'\\') + 1) = '\0';
	ProcHeap = GetProcessHeap();
	WM_PPXCOMMAND = RegisterWindowMessageA(PPXCOMMAND_WM);
	TempPath[0] = '\0';
	{
		TCHAR *p;
		DWORD i = 0;

		for ( p = UserName ; *p ; p++ ) i += (UTCHAR)*p;
		wsprintf(SyncTag,XNAME T("%05X"),i);
	}
								// 例外処理の組み込み =================
	hKernel32 = GetModuleHandle(T("KERNEL32.DLL"));

	#ifndef _WIN64
		if ( OSver.dwMajorVersion >= 6 ){
			GETDLLPROC(hKernel32,AddVectoredExceptionHandler);
			GETDLLPROC(hKernel32,RemoveVectoredExceptionHandler);
		}
		if ( DAddVectoredExceptionHandler == NULL ){
			UEFvec = FUNCCAST(PVOID,SetUnhandledExceptionFilter(PPxUnhandledExceptionFilter));
		}else
	#endif
		{
			UEFvec = DAddVectoredExceptionHandler(0,PPxUnhandledExceptionFilter);
		}
								// 作業用共有メモリの用意 =============
	{
		TCHAR *p;
		int mapresult;

		GetTempPath(TSIZEOF(buf),buf);
		p = buf + tstrlen(buf);
		wsprintf(p,T("%s.TMP"),SyncTag);
		mapresult = FileMappingOn(&SM_tmp,buf,p,sizeof(ShareM),
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE);
		if ( mapresult < 0 ){
			CriticalMessageBox(MSG_SHAREBLOCKERROR);
			return FALSE;
		}

		Sm = (ShareM *)SM_tmp.ptr;
		if ( mapresult != 0 ){	// 作業領域の初期化 ===================
			memset(Sm,0,sizeof(ShareM));
			UsePPx();
			Sm->Version = DLL_VersionValue;
			// Sm->CustomizeWrite = 0; // 初期化済み
			// Sm->RunAsMode = RUNAS_NOCHECK; // 初期化済み
			// Sm->EnumNerServerThreadID = 0; // 初期化済み
			// Sm->CustomizeNameID = 0; // 初期化済み
			// Sm->NowShutdown = FALSE; // 初期化済み
			// Sm->hCommonLogWnd = NULL; // 初期化済み
			FreePPx();
		}else{ // 作業領域のバージョンチェック
			if ( Sm->Version != DLL_VersionValue ){
				CriticalMessageBox(MSG_DLLVERSIONERROR);
				return FALSE;
			}
		}
	}

	hShell32 = GetModuleHandle(T("SHELL32.DLL"));
	GETDLLPROCT(hShell32,SHGetFolderPath);
								// ヒストリのオープン =================
	{
		int mapresult;

		MakeUserfilename(buf,HISTNAME,PPxRegPath);
		UsePPx();
		mapresult = FileMappingOn(&SM_hist,buf,HISTMEMNAME,
				X_Hsize,FILE_ATTRIBUTE_NORMAL);
		if ( mapresult < 0 ){
			CriticalMessageBox(MSG_HITORYAREAERROR);
			FreePPx();
			return FALSE;
		}
											// 正当性チェック
		HisP = (BYTE *)SM_hist.ptr + HIST_HEADER_SIZE;
		if ( mapresult & (FMAP_CREATEFILE | FMAP_FILEOPENERROR) ){
			InitHistory();			// 初期化
		}
	}
	if ( strcmp(HistHeaderFromHisP->HeaderID,HistoryID) ){
		if ( strcmp(HistHeaderFromHisP->HeaderID,"RST") ){
			CriticalMessageBox(MSG_HISTORYIDCOLLAPSED);
		}
		InitHistory();
	}
	if ( X_Hsize != HistSizeFromHisP ){
		X_Hsize = HistSizeFromHisP;
		FileMappingOff(&SM_hist);
		FileMappingOn(&SM_hist,buf,HISTMEMNAME,
				X_Hsize,FILE_ATTRIBUTE_NORMAL);
		HisP = (BYTE *)SM_hist.ptr + HIST_HEADER_SIZE;
	}
	{
		BYTE *p,*histmax;

		p = HisP;
		histmax = HisP + X_Hsize - HIST_HEADER_FOOTER_SIZE;
		for( ; ; ){
			WORD size;

			size = *(WORD *)p;
			if ( !size ) break;
			if ( (p + size) > histmax ){
				CriticalMessageBox(MSG_HISTORYCOLLAPSED);
				*(WORD *)p = 0;
				break;
			}
			p += size;
		}
	}
	FreePPx();
								// カスタマイズ領域のオープン =========
	{
		int mapresult;

		UsePPx();
		MakeUserfilename(buf,CUSTNAME,PPxRegPath);
		MakeCustMemSharename(buf2,Sm->CustomizeNameID);
		mapresult = FileMappingOn(&SM_cust,buf,buf2,X_Csize,FILE_ATTRIBUTE_NORMAL);
		if ( mapresult < 0 ){
			CriticalMessageBox(MSG_CUSTOMIZETAREAERROR);
			FreePPx();
			return FALSE;
		}
											// 正当性チェック
		CustP = (BYTE *)SM_cust.ptr + CUST_HEADER_SIZE;
		if ( mapresult & (FMAP_CREATEFILE | FMAP_FILEOPENERROR) ){
			InitCust();			// 初期化
		}
	}
	if ( strcmp(CustHeaderFromCustP->HeaderID,CustID) ){
		if ( strcmp(CustHeaderFromCustP->HeaderID,"RST") ){;
			CriticalMessageBox(MSG_CUSTOMIZEIDCOLLAPSED);
		}
		InitCust();
	}
	if ( X_Csize != CustSizeFromCustP ){
		X_Csize = CustSizeFromCustP;
		FileMappingOff(&SM_cust);
		FileMappingOn(&SM_cust,buf,CUSTMEMNAME,X_Csize,FILE_ATTRIBUTE_NORMAL);
		CustP = (BYTE *)SM_cust.ptr + CUST_HEADER_SIZE;
	}
	{
		BYTE *p,*custmax;

		p = CustP;
		custmax = CustP + X_Csize - CUST_HEADER_FOOTER_SIZE;
		for ( ; ; ){
			DWORD size;

			size = *(DWORD *)p;
			if ( !size ) break;
			if ( (p + size) > custmax ){
				CriticalMessageBox(MSG_CUSTOMIZESTRUCTUREBROKEN);
				*(DWORD *)p = 0;
				break;
			}
			p += size;
		}
	}
	FreePPx();
								// カスタマイズ内容の取り出し =========
	GetCustData(T("X_mwid"),&X_mwid,sizeof(X_mwid));
	GetCustData(T("X_es"),&X_es,sizeof(X_es));
	X_es = X_es & 0xff;
								// マルチバイト文字長さテーブルの補正
	FixCharlengthTable(NULL);

	GetCustData(T("X_log"),&X_log,sizeof(X_log));
	return TRUE;
}

#if NODLL
extern void InitCommonDll(HINSTANCE hInst)
{
	DllMain(hInst,DLL_PROCESS_ATTACH,NULL);
}
#endif
