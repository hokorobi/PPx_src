/*-----------------------------------------------------------------------------
	Paper Plane basicUI			Main
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <wincon.h>
#include <string.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "TCONSOLE.H"
#include "PPB.H"
#pragma hdrstop

HWND hMainWnd = NULL;	// コンソールウィンドウのハンドル
HWND hBackWnd = NULL;	// 最小化時に戻る Window
int RegNo = -1;			// PPx Register ID
DWORD ExitCode = EXIT_SUCCESS; // 実行したプロセスの終了コード
TCHAR RegID[REGIDSIZE] = T("B_");	// PPx Register ID
TCHAR *CurrentPath;		// 現在の実行パス(EditPath or ExtPath)
TCHAR PPxPath[VFPS];	// PPx 本体のあるパス
TCHAR EditPath[VFPS];	// PPb command line が保持するパス
TCHAR ExtPath[VFPS];	// 外部実行時に使用するパス
HANDLE hCommSendEvent;		// 外部からのコマンド受け付け用イベント
							// signal:受付可能
HANDLE hCommIdleEvent;		// 外部からのコマンド受け付け用イベント
							// signal:作業中
WINPOS WinPos;
HMODULE hKernel32;
ThSTRUCT LongParam;	// 長いコマンドライン用

TCHAR WndTitle[WNDTITLESIZE] = T("PPB[a]");
TCHAR RegCID[REGIDSIZE - 1] = T("BA");

const TCHAR PPbMainThreadName[] = T("PPb main");
const TCHAR PPbOptionError[] = T("Bad option\r\n");
const TCHAR PPbMainTitle[] = T("PPbui Version ") T(FileProp_Version) T("(")
		T(BITSTRING) T(",") T(CODETYPESTRING) T(") ") TCopyright T("\n");

THREADSTRUCT threadstruct = {PPbMainThreadName, XTHREAD_ROOT, NULL, 0, 0};
PPXAPPINFO ppbappinfo = {(PPXAPPINFOFUNCTION)PPbInfoFunc, T("PPb"), RegID, NULL};

/*-----------------------------------------------------------------------------
	SetConsoleCtrlHandler で登録されるハンドラ
-----------------------------------------------------------------------------*/
BOOL SysHotKey(DWORD dwCtrlType)
{
	switch (dwCtrlType){
		case CTRL_CLOSE_EVENT:				// 終了処理
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			ReleasePPB();
			return FALSE; // 次のハンドラへ…強制終了
		case CTRL_BREAK_EVENT:				// ^BREAK
		case CTRL_C_EVENT:
			return TRUE;
	}
	return FALSE;
}

BOOL InitPPb(void)
{
	TCHAR buf[CMDLINESIZE * 2];
	const TCHAR *p;
	DefineWinAPI(HWND, GetConsoleWindow, (void));
	HWND hRwnd = BADHWND; // 起動時同期の対象ウィンドウ
	BOOL silent = FALSE;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
#if NODLL
	InitCommonDll(GetModuleHandle(NULL));
#endif
	hKernel32 = GetModuleHandle(T("Kernel32.DLL"));
	PPxRegisterThread(&threadstruct);
	FixCharlengthTable(T_CHRTYPE);
//------------------------------------- カレントディレクトリ設定
	p = GetCommandLine();
	GetLineParam(&p, PPxPath);
										//---------------------------- PPxPath
	GetModuleFileName(NULL, PPxPath, TSIZEOF(PPxPath));
	*VFSFindLastEntry(PPxPath) = '\0';	// ファイル名部分を除去する
										//---------------------------- EditPath
	GetCurrentDirectory(TSIZEOF(EditPath), EditPath);
										//---------------------------- ExtPath
	tstrcpy(ExtPath, EditPath);
										//-------- プロセスのカレントを変更する
	SetCurrentDirectory(PPxPath);

	CurrentPath = EditPath;
	PPxRegGetIInfo(&ppbappinfo);

	if ( (SkipSpace(&p) == '-') && (*(p + 1) == 'P') ){
		p += 2;
		GetLineParam(&p, buf);
		VFSFixPath(EditPath, buf, EditPath, VFSFIX_FULLPATH | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
		SkipSpace(&p);	// 空白削除
	}
										// オプションチェック -----
	if ( (*p == '-') || (*p == '/') ){
		p++;
		switch ( TinyCharUpper(*p) ){
			case 'K':
				silent = TRUE;
			case 'C':
				p++;
				SkipSpace(&p);	// 空白削除
				PP_ExtractMacro(NULL, &ppbappinfo, NULL, p, NULL, (silent == FALSE) ?
						(XEO_CONSOLE | XEO_NOUSEPPB) :
						(XEO_CONSOLE | XEO_NOUSEPPB | XEO_SEQUENTIAL) );
				if ( silent == FALSE ) goto goexit; // EXIT_SUCCESS
				break;
			case 'I':
				p++;
				if ( *p == 'R' ){
					p++;
					hRwnd = (HWND)GetNumber(&p);
					break;
				}
			default:
				tputstr_noinit(PPbOptionError);
				ExitCode = EXIT_FAILURE;
				goto goexit;
		}
	}
	//------------------------------------- 初期化
	RegNo = PPxRegist(PPXREGIST_DUMMYHWND, RegID, PPXREGIST_NORMAL);

	SetErrorMode(SEM_FAILCRITICALERRORS);	// 致命的エラーを取得可能にする

	GETDLLPROC(hKernel32, GetConsoleWindow);
	if ( DGetConsoleWindow != NULL ){
		hMainWnd = DGetConsoleWindow();
	}else{
		wsprintf(buf, T("+%X)"), GetCurrentThreadId());
	}
	if ( tInit((DGetConsoleWindow != NULL) ? NULL : buf) == FALSE ){
		MessageBox(NULL, T("Can't execute PPb on this OS."), NULL, MB_OK);
		ExitCode = EXIT_FAILURE;
		goto goexit;
	}
	ThInit(&LongParam);
									// システム関連のイベント管理ハンドラの設定
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)SysHotKey, TRUE);
	if ( silent == FALSE ) tputstr(PPbMainTitle);
										// 自分のコンソールの HWND を取得する
	if ( DGetConsoleWindow == NULL ){
		int i;

		for ( i = 0 ; i < 30 ; i++ ){
			hMainWnd = FindWindow(NULL, buf);
			if ( hMainWnd != NULL ) break;
			Sleep(100);
		}
	}
	if ( hMainWnd == NULL ) hMainWnd = BADHWND;
	ppbappinfo.hWnd = hMainWnd;
										// PPCOMMON に登録
	PPxRegist(hMainWnd, RegID, PPXREGIST_SETHWND);
	RegCID[1] = RegID[2];
	WndTitle[4] = RegID[2];
	{
		const TCHAR *runasp;

		runasp = CheckRunAs();
		if ( runasp != NULL ) wsprintf(WndTitle + 6, T("(%s)"), runasp);
	}
										// ↓ SendPPb記載のフロー番号
										// [1] プロセス間通信用イベントの定義
	wsprintf(buf, T("%s%s"), PPxGetSyncTag(), RegID);

	hCommSendEvent = CreateEvent(NULL, TRUE, FALSE, buf); // nosignal
	tstrcat(buf, T("R"));
	hCommIdleEvent = CreateEvent(NULL, TRUE, FALSE, buf); // nosignal

	if ( hCommSendEvent == NULL ) tputstr(T("Event create error\n"));
											// ウィンドウ位置を再設定
	if ( NO_ERROR == GetCustTable(T("_WinPos"), RegCID, &WinPos, sizeof(WinPos)) ){
		MoveWindow(hMainWnd, WinPos.pos.left, WinPos.pos.top,
					WinPos.pos.right - WinPos.pos.left,
					WinPos.pos.bottom - WinPos.pos.top, TRUE);
	}
//------------------------------------- 起動時同期
	if ( hRwnd != BADHWND ){
		HANDLE hRevent;

		wsprintf(buf, T(PPBBOOTSYNC) T("%x"), hRwnd);
		hRevent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);
		if ( hRevent != NULL ){
			SetEvent(hRevent);
			CloseHandle(hRevent);
		}
	}
	return TRUE;
goexit:
	CoUninitialize();
	return FALSE;
}

/*-----------------------------------------------------------------------------
	Main Routine
-----------------------------------------------------------------------------*/
// スタートアップコードを簡略にするため、void 指定
int USECDECL main(void)
{
	int breakflag = 0;		// 0:loop
	TCHAR Cparam[CMDLINESIZE]; // 入力されたコマンドライン
	TCHAR RecvParam[CMDLINESIZE + 128]; // 受信したコマンドライン

	if ( InitPPb() == FALSE ) return ExitCode;

	Cparam[0] = '\0';
	while ( breakflag >= 0 ){
											// カレントディレクトリの解放
		CurrentPath = EditPath;
		SetCurrentDirectory(PPxPath);
		TitleDisp(NULL);
		SetEvent(hCommIdleEvent); // [2]受付可能
		switch( tCInput(Cparam, TSIZEOF(Cparam),
				PPXH_GENERAL | PPXH_COMMAND | PPXH_PATH) ){
			case TCI_RECV: {	// 外部からのコマンド実行、[4][5] 内容設定通知
				*(DWORD *)RecvParam = ExitCode;
				if ( ReceiveStrings(RegID, RecvParam) == 0 ){ // [6]内容受領
					tFillChr(0, screen.dwCursorPosition.Y,
						screen.dwSize.X - 1, screen.dwCursorPosition.Y, ' ');
					CurrentPath = ExtPath;
					PPbExecuteRecv(RecvParam); // [7]実行
				}
				ResetEvent(hCommSendEvent); // [8] 返信
				break; // [9] 再受付へ
			}
			case TCI_EXECUTE: {	// CR 実行
				if ( Cparam[0] != '\0' ){
					ResetEvent(hCommIdleEvent); // 受け付け無しに変更 [1]
					tputstr(T("\n"));
					WriteHistory(PPXH_COMMAND, Cparam, 0, NULL);
					PPbExecuteInput(Cparam, tstrlen32(Cparam));
					Cparam[0] = '\0';
				}
				break;
			}
			case TCI_QUIT:		// ESC 終了
				tputstr(T("\n"));
				breakflag = -1;
				break;
		}
	}
	ReleasePPB();
	threadstruct.flag = XTHREAD_ROOT | XTHREAD_EXITENABLE;
	return EXIT_SUCCESS;
}
