/*-----------------------------------------------------------------------------
	Paper Plane tray	Main
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPTRAY.RH"

#define WM_TRAY_T		(WM_APP + 1060)	// Task Tray からの受信に使用
#define TRAYID			0
#define HOTKEYID 0xe000

HINSTANCE hInst;
int		X_HookEdit = 0;
DWORD	WM_TASKBARCREATE = MAX32;				// タスクバーが作成された
DWORD	WM_PPXCOMMAND    = MAX32;

HWND	hCommonWnd = NULL;
int		UseCount = 0;
HWND	hForegroundWnd = NULL;

UINT	*HotKeys;	// 登録したホットキーを覚える領域
struct BUTTONS {	// 押したボタンのエイリアス定義テーブル
	UINT msg;
	TCHAR *name;
} buttonname[] = {
	{WM_LBUTTONDOWN,	T("L_ICON")},
	{WM_LBUTTONDBLCLK,	T("LD_ICON")},
	{WM_MBUTTONDOWN,	T("M_ICON")},
	{WM_MBUTTONDBLCLK,	T("MD_ICON")},
	{WM_RBUTTONDOWN,	T("R_ICON")},
	{WM_RBUTTONDBLCLK,	T("RD_ICON")},
	{WM_XBUTTONDOWN,	T("X_ICON")},
	{WM_XBUTTONDBLCLK,	T("XD_ICON")},
	{0,NULL}
};

const TCHAR TRAYCCLASS[] = T(PPTrayWinClass) T("C");
const TCHAR TRAYCLASS[] = T(PPTrayWinClass);

TCHAR PPTrayMainThreadName[] = T("PPtray main");
TCHAR PPTrayRegID[REGIDSIZE] = T(PPTRAY_REGID) T("A");
enum { HOOKSW = 100,EXITCMD };
const TCHAR HookStr[] = MES_CTHE;
const TCHAR ExitStr[] = MES_EXIT;

DWORD_PTR USECDECL PPtrayInfoFunc(PPXAPPINFO *info,DWORD cmdID,PPXAPPINFOUNION *uptr);
PPXAPPINFO PPtrayInfo = {(PPXAPPINFOFUNCTION)PPtrayInfoFunc,TRAYCLASS,PPTrayRegID,NULL};

#pragma argsused
DWORD_PTR USECDECL PPtrayInfoFunc(PPXAPPINFO *info,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	UnUsedParam(info);

	switch(cmdID){
		case PPXCMDID_GETFGWND:
			return (DWORD_PTR)hForegroundWnd;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
}

// ホットキー処理 =============================================================
//-------------------------------------- ホットキーの割り当て処理
void SetHotKey(HWND hWnd)
{
	TCHAR key[MAX_PATH],buf[MAX_PATH];
	CTCHAR *p;
	int keycode,vkey,i,max;
	UINT *hk;

	HotKeys = NULL;
	max = CountCustTable(T("K_tray"));
	if ( max == 0 ) return;
	hk = HotKeys = HeapAlloc(GetProcessHeap(),0,(max + 1) * sizeof hk[0]);

	for ( i = 0 ; i < max ; i++ ){
		if ( EnumCustTable(i,T("K_tray"),key,buf,0) < 0 ) break;
		p = key;
		*hk++ = keycode = GetKeyCode(&p);
		if ( keycode < 0 ){
			XMessage(hWnd,T("PPtray"),XM_FaERRd,MES_EBDK,key);
			continue;
		}
										// RegisterHotKey 用のキーコードを作成
		vkey = keycode & 0xff;
		if ( !(keycode & K_v) ){
			vkey = VkKeyScan((TCHAR)vkey);
			vkey &= 0xff;
		}
										// ホットキーを登録
		if ( FALSE == RegisterHotKey(hWnd,keycode,
						((keycode & K_e) ? MOD_WIN : 0) |
						((keycode & K_s) ? MOD_SHIFT : 0) |
						((keycode & K_c) ? MOD_CONTROL : 0) |
						((keycode & K_a) ? MOD_ALT : 0),vkey) ){
			XMessage(hWnd,T("PPtray"),XM_FaERRd,MES_EHKY,key);
		}
	}
	*hk = 0;
}
//-------------------------------------- ホットキー割り当てを全て解除
void FreeHotKey(HWND hWnd)
{
	UINT *hk;

	if ( HotKeys == NULL ) return;
	for ( hk = HotKeys ; *hk ; hk++ ) UnregisterHotKey(hWnd,*hk);
	HeapFree( GetProcessHeap(),0,HotKeys );
	HotKeys = NULL;
}

// PPx格納用Window管理 ========================================================
LRESULT CALLBACK CommonWindow(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg){
		case WM_SETFOCUS:
			SetForegroundWindow(PPcGetWindow(0,CGETW_GETFOCUS));
			break;
							// 窓の破棄,終了処理(w:未使用,l:未使用) -----------
		case WM_DESTROY:
			hCommonWnd = NULL;
			// default へ
		default:
			if ( uMsg == WM_PPXCOMMAND ){
				if ( (WORD)wParam == KRN_freecwnd ){
					UseCount--;
					if ( UseCount <= 0 ){
						UseCount = 0;
						SendMessage(hWnd,WM_CLOSE,0,0);
					}
				}
				break;
			}
			return DefWindowProc(hWnd,uMsg,wParam,lParam);
	}
	return 0;
}

HWND GetCommonWnd(void)
{
	if ( hCommonWnd == NULL ){
		WNDCLASS wndClass;

		wndClass.style			= 0;
		wndClass.lpfnWndProc	= CommonWindow;
		wndClass.cbClsExtra		= 0;
		wndClass.cbWndExtra		= 0;
		wndClass.hInstance		= hInst;
		wndClass.hIcon			= LoadIcon(hInst,MAKEINTRESOURCE(IDI_TRAY));
		wndClass.hCursor		= NULL;
		wndClass.hbrBackground	= NULL;
		wndClass.lpszMenuName	= NULL;
		wndClass.lpszClassName	= TRAYCCLASS;
											// クラスを登録する
		RegisterClass(&wndClass);
		hCommonWnd = CreateWindow(TRAYCCLASS,T("PPx"),
				WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX,
				0,0,0,0,NULL,NULL,hInst,0);
		ShowWindow(hCommonWnd,SW_SHOW);
	}
	UseCount++;
	return hCommonWnd;
}

// トレイ管理 =================================================================
// トレイからのメッセージを処理する -------------------------------------------
BOOL TrayMessage(HWND hWnd,DWORD Message,WORD Icon,const TCHAR *TipStr)
{
	NOTIFYICONDATA tnd;

	tnd.cbSize				= sizeof(NOTIFYICONDATA);
	tnd.hWnd				= hWnd;
	tnd.uID					= TRAYID;
	tnd.uFlags				= NIF_MESSAGE;
	tnd.uCallbackMessage	= WM_TRAY_T;
	if ( Icon ){
		tnd.hIcon			= LoadIcon(hInst,MAKEINTRESOURCE(Icon));
		setflag(tnd.uFlags,NIF_ICON);
	}
	if ( TipStr ){
		setflag(tnd.uFlags,NIF_TIP);
		tstrcpy(tnd.szTip,TipStr);
	}
	return Shell_NotifyIcon(Message,&tnd);
}
//-------------------------------------- PPtray のメインメニュー
void TrayMenu(HWND hWnd)
{
	POINT pos;
	HMENU popup;
	int i;

	SetForegroundWindow(hWnd);	// おまじない２
	GetCursorPos(&pos);

	popup = CreatePopupMenu();
	AppendMenu(popup,MF_CHKES(X_HookEdit),HOOKSW,MessageText(HookStr));
	AppendMenu(popup,MF_ES,EXITCMD,MessageText(ExitStr));

	i = TrackPopupMenu(popup,TPM_TDEFAULT,pos.x,pos.y,0,hWnd,NULL);
	DestroyMenu(popup);
	switch (i){
		case EXITCMD:	// Exit -----------------------------------
			PostMessage(hWnd,WM_CLOSE,0,0);
			break;

		case HOOKSW:	// X_HookEdit ------------------------------
			X_HookEdit = X_HookEdit ? 0 : 1;
			PPxHookEdit( X_HookEdit ? 0 : -1 );
			SetCustData(T("X_eedit"),&X_HookEdit,sizeof(X_HookEdit));
			break;
//		default: // 使用せず
	}
}

//-------------------------------------- PPtray の各コマンドの実行
void TrayCommand(HWND hWnd,UINT mes)
{
	struct BUTTONS *b;
	TCHAR buf[CMDLINESIZE];
										// 右クリック:メインメニュー ----------
	if ( mes == WM_RBUTTONDOWN ){
		TrayMenu(hWnd);
		return;
	}
										// ホットキー -------------------------
	if ( (mes & HOTKEYID) == HOTKEYID ){
		PutKeyCode(buf,mes & ~HOTKEYID);

		if ( NO_ERROR == GetCustTable(T("K_tray"),buf,&buf,sizeof buf) ){
			if ( (UTCHAR)buf[0] == EXTCMD_CMD ){
				hForegroundWnd = GetForegroundWindow();
				SetForegroundWindow(hWnd);
				PP_ExtractMacro(NULL,&PPtrayInfo,NULL,buf + 1,NULL,0);
			}
		}
		return;
	}
										// クリック実行 -----------------------
	for ( b = buttonname ; b->msg ; b++ ){
		if ( b->msg == mes ) break;
	}
	if ( !b->msg ) return;
	SetForegroundWindow(hWnd);	// おまじない２
	if ( NO_ERROR == GetCustTable(T("MT_icon"),b->name,&buf,sizeof buf) ){
		if ( (UTCHAR)buf[0] == EXTCMD_CMD ){
			PP_ExtractMacro(NULL,&PPtrayInfo,NULL,buf + 1,NULL,0);
		}
	}else{
		PP_ExtractMacro(hWnd,NULL,NULL,T("*SELECTPPX"),NULL,0);
	}
}

LRESULT CALLBACK TrayWindow(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg){
		case WM_CREATE:
			TrayMessage(hWnd,NIM_ADD,IDI_TRAY,T("PPtray"));
			break;

		case WM_HOTKEY:
			PostMessage(hWnd,WM_COMMAND,wParam | HOTKEYID,0); // おまじない１
			break;
							// 窓の破棄,終了処理(w:未使用,l:未使用) -----------
		case WM_DESTROY:
			FreeHotKey(hWnd);
			TrayMessage(hWnd,NIM_DELETE,0,NULL);
			PostQuitMessage(EXIT_SUCCESS);	// これで終り
			break;
							// 強制終了動作の報告 -----------------------------
		case WM_ENDSESSION:
			if ( wParam == 0 ) break;
							// TRUE:セッションの終了(WM_DESTROY は通知されない)
			FreeHotKey(hWnd);
			TrayMessage(hWnd,NIM_DELETE,0,NULL);
			break;

		case WM_COMMAND:
			TrayCommand( hWnd,LOWORD(wParam) );
			break;

		case WM_TRAY_T:
			// ダブルクリック時に既にメニューを開いているときはメニューを閉じる
			// →ダブルクリック時にウィンドウを開いてもフォーカス移動できない為
			if ( (lParam == WM_LBUTTONDBLCLK) ||
				 (lParam == WM_RBUTTONDBLCLK) ||
				 (lParam == WM_MBUTTONDBLCLK) ||
				 (lParam == WM_XBUTTONDBLCLK) ){
				HWND hMenuWnd;

				hMenuWnd = FindWindow(T("#32768"),NULL);
				if ( hMenuWnd != NULL ) PostMessage(hMenuWnd,WM_CLOSE,0,0);
			}
			PostMessage(hWnd,WM_COMMAND,lParam,0); // おまじない１
			break;

		default:
			if ( uMsg == WM_PPXCOMMAND ){
				if ( (WORD)wParam == K_Lcust ){	// 再カスタマイズ -------------
					FreeHotKey(hWnd);
					SetHotKey(hWnd);
				}
				if ( (WORD)wParam == KRN_getcwnd ){
					return (LRESULT)GetCommonWnd();
				}
				PPxCommonCommand(hWnd,0,(WORD)wParam);
				break;
			}
			if ( uMsg == WM_TASKBARCREATE ){
				TrayMessage(hWnd,NIM_ADD,IDI_TRAY,T("PPtray"));
				break;
			}
			return DefWindowProc(hWnd,uMsg,wParam,lParam);
	}
	return 0;
}
//-----------------------------------------------------------------------------
#pragma argsused
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASS wndClass;
	MSG msg;
	HWND hWnd;
	THREADSTRUCT threadstruct = {PPTrayMainThreadName,XTHREAD_ROOT,NULL,0,0};
	CTCHAR *p;

#ifdef UNICODE
	TCHAR buf[CMDLINESIZE];
	UnUsedParam(hInstance);UnUsedParam(hPrevInstance);UnUsedParam(lpCmdLine);UnUsedParam(nShowCmd);

	PPxRegisterThread(&threadstruct);
	p = GetCommandLine();
	GetLineParam((const TCHAR **)&p,buf);
#else
	UnUsedParam(hInstance);UnUsedParam(hPrevInstance);UnUsedParam(nShowCmd);

	PPxRegisterThread(&threadstruct);
	p = lpCmdLine;
#endif
	SkipSpace((const TCHAR **)&p);	// 空白削除
	if ( (*p == '-') || (*p == '/') ){		// オプション解析
		TCHAR option;

		CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
		PPxCommonCommand(NULL,JOBSTATE_NORMAL,K_ADDJOBTASK);
		option = TinyCharLower(*(p + 1));
		p += 2;
		SkipSpace(&p);
		if ( option == 'c' ){	// -c command コマンド実行
			PP_ExtractMacro(NULL,NULL,NULL,p,NULL,XEO_NOUSEPPB);
		}else if ( option == 'b' ){	// -b filename バッチファイル指定実行
			TCHAR *top = NULL,*text;

			if ( LoadTextImage(p,&top,&text,NULL) == NO_ERROR ){
				PP_ExtractMacro(NULL,NULL,NULL,text,NULL,XEO_NOUSEPPB);
				HeapFree(GetProcessHeap(),0,top);
			}
		}
		PPxCommonCommand(NULL,0,K_DELETEJOBTASK);
		CoUninitialize();
		return EXIT_SUCCESS;
	}
															// 二重起動の禁止
	if ( PPxRegist(PPXREGIST_DUMMYHWND,PPTrayRegID,PPXREGIST_IDASSIGN) < 0 ){
		return EXIT_FAILURE;
	}
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
	hInst = hInstance;
	WM_TASKBARCREATE = RegisterWindowMessageA(WMTASKBARCREATE);
	WM_PPXCOMMAND = RegisterWindowMessageA(PPXCOMMAND_WM);
										// ウインドウクラスを定義する ---------
	wndClass.style			= 0;
	wndClass.lpfnWndProc	= TrayWindow;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= 0;
	wndClass.hInstance		= hInst;
	wndClass.hIcon			= NULL;
	wndClass.hCursor		= NULL;
	wndClass.hbrBackground	= NULL;
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= TRAYCLASS;
	RegisterClass(&wndClass);

	PPtrayInfo.hWnd = hWnd = CreateWindow(TRAYCLASS,NULL,0,0,0,0,0,NULL,NULL,hInst,0);

	PPxRegist(hWnd,PPTrayRegID,PPXREGIST_SETHWND);
	GetCustData(T("X_eedit"),&X_HookEdit,sizeof(X_HookEdit));
	if ( X_HookEdit ) PPxHookEdit(0);

	ShowWindow(hWnd,SW_HIDE);
	SetHotKey(hWnd);

	while( (int)GetMessage(&msg,NULL,0,0) > 0 ){
//		TranslateMessage(&msg);	// ※ WM_CHAR を使用しないのでコメントアウト
		DispatchMessage(&msg);
	}
	PPxRegist(hWnd,PPTrayRegID,PPXREGIST_FREE);
	if ( X_HookEdit ) PPxHookEdit(-1);

	PPxCommonCommand(NULL,0,K_UNHIDEALL); //格納していたら追い出す
	PPxCommonCommand(NULL,0,K_CLEANUP);
	CoUninitialize();
	return EXIT_SUCCESS;
}
