/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library								PPx edit UI
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "PPD_EDL.H"
#pragma hdrstop

//-------------------------------------- ウィンドウのスタイル
#define PPESTYLE	WS_OVERLAPPEDWINDOW
#define PPESTYLEEX	WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME

//-------------------------------------- グローバル変数定義
typedef struct {
	HWND hEWnd;			// Edit Box の HWND
	HFONT hBoxFont;		// 等幅フォント
	int fontX,fontY;	// hBoxFont の寸法
	DWORD dpi;
	ThSTRUCT ThMenu;
	TCHAR CurDir[VFPS]; // カレントディレクトリ
	TCHAR PosName[5]; // 窓の位置を記憶する名前
} PPeditSTRUCT;

PPXINMENU PPebarFile[] = {
	{ K_c | 'O'	,T("Open\tCtrl+O")},
	{ K_c | 'S'	,T("Save\tCtrl+S")},
	{PPXINMENY_SEPARATE,NULL },
	{ K_c | 'W'	,T("E&xit\tAlt+F4")},
	{0,NULL}
};
PPXINMENU PPebarEdit[] = {
	{ K_c | 'Z'	,T("&Undo\tCtrl+Z")},
	{PPXINMENY_SEPARATE,NULL },
	{ K_c | 'X',	T("%G\"JMCU|Cut\"(&T)\tCtrl+X")},
	{ K_c | 'C',	T("%G\"JMCL|Copy\"(&C)\tCtrl+C")},
	{ K_c | 'V',	T("%G\"JMPA|Paste\"(&P)\tCtrl+V")},
	{ K_del,		T("%G\"JMDE|Delete\"(&D)\tDelete")},
	{PPXINMENY_SEPARATE,NULL },
	{ K_c | 'Q',	T("Edit menu\tCtrl+&Q")},
	{ K_c | 'K'	,T("Menu&2\tCtrl+K")},
	{PPXINMENY_SEPARATE,NULL },
	{ K_c | 'A'	,T("select &All\tCtrl+A")},
	{0,NULL}
};
PPXINMENU PPebarView[] = {
	{ K_F1		,T("ext FileMenu\tF1")},
	{ K_s | K_F2,T("ViewSettings\tShift+F2")},
	{0,NULL}
};
PPXINMENU PPebarHelp[] = {
	{ K_ex		,T("Paper Plane eUI")},
	{ K_ex		,T("Version ") T(FileProp_Version)},
	{ K_ex		,T("(") T(__DATE__) T(")")},
	{ K_ex		,T("(c)TORO")},
	{0,NULL}
};

PPXINMENUBAR ppebar[] = {
	{T("&File"),PPebarFile},
	{T("&Edit"),PPebarEdit},
	{T("&View"),PPebarView},
	{T("&Help"),PPebarHelp},
	{NULL,NULL}
};

const TCHAR K_ppe[] = T("K_ppe");

/*-----------------------------------------------------------------------------
	桁と幅に合わせてウィンドウを調節する
-----------------------------------------------------------------------------*/

HFONT CreatePPeFont(int dpi)
{
	LOGFONTWITHDPI cursfont;

	GetPPxFont(PPXFONT_F_mes,dpi,&cursfont);
	return CreateFontIndirect(&cursfont.font);
}

void FixEditWindowRect(HWND hWnd)
{
	int mx,my;//,x,y;
	RECT rect;
	PPeditSTRUCT *PES;

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	my = (int)SendMessage(PES->hEWnd,EM_GETLINECOUNT,0,0);
	if ( my < 10 ) my = 10;
	if ( my >= 30 ) my = 30;
/*	自動窓枠決定用のコード…使い心地が悪かった(^^;
	for ( y = 1 ; y <= my ; y++ ){
		x = SendMessage(PES->hEWnd, EM_LINEINDEX, y,0 );// 先頭からのオフセット
		x = SendMessage(PES->hEWnd, EM_LINELENGTH, x,0 );	// 桁数を得る
		if (x > mx) mx = x;
	}
*/
	mx = 80;

	GetWindowRect(PES->hEWnd,&rect);
	rect.right	= rect.left + (mx + 1) * PES->fontX + GetSystemMetrics(SM_CYHSCROLL);
	rect.bottom	= rect.top  + (my + 1) * PES->fontY + GetSystemMetrics(SM_CXVSCROLL);
	AdjustWindowRectEx(&rect,PPESTYLE | WS_HSCROLL | WS_VSCROLL,TRUE,PPESTYLEEX);
	MoveWindow(hWnd,rect.left,rect.top,
			rect.right - rect.left,rect.bottom - rect.top,TRUE);
}

LRESULT PPeWmCreate(HWND hWnd)
{
	PPeditSTRUCT *PES;
	HDC hDC;
	HGDIOBJ hOldFont;
	TEXTMETRIC tm;
										// 作業領域を確保 -------------
	PES = HeapAlloc(DLLheap,0,sizeof(PPeditSTRUCT));
	if ( PES == NULL ) return -1;

	PES->CurDir[0] = '\0';
	SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)PES);

										// EditBox を作成 -------------
	PES->hEWnd = CreateWindowEx(WS_EX_ACCEPTFILES,EDITstr,NilStr,
		WS_CHILD | /*WS_HSCROLL |*/ WS_VSCROLL |
		/*ES_AUTOHSCROLL |*/ ES_AUTOVSCROLL | ES_NOHIDESEL |
		ES_LEFT | ES_MULTILINE | ES_WANTRETURN,	// ウインドウの形式
		0,0,0,0,hWnd,NULL,DLLhInst,0);

	PES->dpi = GetMonitorDPI(hWnd);
	PES->hBoxFont = CreatePPeFont(PES->dpi);
	hDC = GetDC(hWnd);
	hOldFont = SelectObject(hDC,PES->hBoxFont);
	GetTextMetrics(hDC,&tm);
	PES->fontX = tm.tmAveCharWidth;
	PES->fontY = tm.tmHeight;
	PES->PosName[0] = '\0';
	SelectObject(hDC, hOldFont);
	ReleaseDC(hWnd,hDC);
										// EditBox の Font を等幅に ---
	SendMessage(PES->hEWnd,WM_SETFONT,(WPARAM)PES->hBoxFont,TRUE);
										// EditBox の拡張 -------------
	PPxRegistExEdit(NULL,PES->hEWnd,0x100000,NULL,0,0,
			PPXEDIT_USEALT | PPXEDIT_WANTEVENT | PPXEDIT_NOWORDBREAK);
										// K_ppe を登録 ---
	SendMessage(PES->hEWnd,WM_PPXCOMMAND,KE_setkeya,(LPARAM)K_ppe);
	if ( IsExistCustTable(K_ppe,T("FIRSTEVENT")) &&
		!IsExistCustTable(T("K_edit"),T("FIRSTEVENT")) ){
		PostMessage(PES->hEWnd,WM_PPXCOMMAND,K_E_FIRST,0);
	}
	ShowWindow(PES->hEWnd,SW_SHOWNORMAL);
	return 0;
}

void PPeWMDpiChanged(HWND hWnd,PPeditSTRUCT *PES,WPARAM wParam,RECT *newpos)
{
	DWORD newDPI = HIWORD(wParam);
	HDC hDC;
	HFONT hNewFont,hOldFont;
	TEXTMETRIC	tm;

	if ( !(X_dss & DSS_ACTIVESCALE) ) return;
	if ( PES->dpi == newDPI ) return; // 変更無し(起動時等)
	PES->dpi = newDPI;

	hNewFont = CreatePPeFont(newDPI);
	SendMessage(PES->hEWnd,WM_SETFONT,(WPARAM)hNewFont,TRUE);
	DeleteObject(PES->hBoxFont);
	PES->hBoxFont = hNewFont;
	hDC = GetDC(hWnd);
	hOldFont = SelectObject(hDC,PES->hBoxFont);
	GetTextMetrics(hDC,&tm);
	PES->fontX = tm.tmAveCharWidth;
	PES->fontY = tm.tmHeight;
	PES->PosName[0] = '\0';
	SelectObject(hDC, hOldFont);
	ReleaseDC(hWnd,hDC);

	if ( newpos != NULL ){
		SetWindowPos(hWnd,NULL,newpos->left,newpos->top,
				newpos->right - newpos->left, newpos->bottom - newpos->top,
				SWP_NOACTIVATE | SWP_NOZORDER);
	}
	InvalidateRect(hWnd,NULL,TRUE);
}


void PPeFileOpen(HWND hWnd,HWND hEdWnd,const TCHAR *fname)
{
	if ( SendMessage(hEdWnd,WM_PPXCOMMAND,KE_openfile,(LPARAM)fname) == FALSE ){
		return;
	}
	SetWindowText(hWnd,fname);
	FixEditWindowRect(hWnd);
}

void PPeWmDROPFILES(HWND hWnd,PPeditSTRUCT *PES,HDROP hDrop)
{
	TCHAR name[VFPS];

	DragQueryFile( hDrop,0,name,TSIZEOF(name) );
	DragFinish(hDrop);
	PPeFileOpen(hWnd,PES->hEWnd,name);
	SetForegroundWindow(hWnd);
}

void PPeClose(HWND hWnd,PPeditSTRUCT *PES)
{
	WINPOS WinPos;

	if ( (PES->PosName[0] != '\0') &&
		 !IsIconic(hWnd) /*&& IsWindowVisible(hWnd)*/ ){
		WinPos.show = 0;
		GetWindowRect(hWnd,&WinPos.pos);
		SetCustTable(T("_WinPos"),PES->PosName,&WinPos,sizeof(WinPos));
	}
	if ( SendMessage(PES->hEWnd,WM_PPXCOMMAND,KE_closecheck,0) ){
		DestroyWindow(hWnd); // 窓の破棄(WM_DESTROY)
	}
}

/*-----------------------------------------------------------------------------
	メインルーチン
-----------------------------------------------------------------------------*/
LRESULT CALLBACK PPeProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	PPeditSTRUCT *PES;

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	switch (message){
		case WM_MENUCHAR:
			return PPxMenuProc(hWnd,message,wParam,lParam);

		case WM_CREATE:
			return PPeWmCreate(hWnd);
/*
		case WM_NCCREATE:
			if ( (X_dss & DSS_COMCTRL) && (OSver.dwMajorVersion >= 10) ){
				PPxCommonCommand(hWnd,0,K_ENABLE_NC_SCALE);
			}
			return 1;
*/
		case WM_DROPFILES: // D&D 処理
			PPeWmDROPFILES(hWnd,PES,(HDROP)wParam);
			break;

		case WM_SETFOCUS:
			SetFocus(PES->hEWnd);
			break;

		case WM_SIZE:
			MoveWindow(PES->hEWnd,0,0,LOWORD(lParam),HIWORD(lParam),TRUE);
			break;

		case WM_CLOSE:	// 終了要求
			PPeClose(hWnd,PES);
			break;

		case WM_DESTROY:
			ThFree(&PES->ThMenu);
													// 等幅フォントの破棄
			PostMessage(PES->hEWnd,WM_SETFONT,0,TRUE);
			DeleteObject(PES->hBoxFont);
			HeapFree(DLLheap,0,PES);
			break;

		case WM_NCLBUTTONDBLCLK:
			switch(wParam){
				case HTBOTTOM:
				case HTBOTTOMLEFT:
				case HTBOTTOMRIGHT:
				case HTLEFT:
				case HTRIGHT:
				case HTTOP:
				case HTTOPLEFT:
				case HTTOPRIGHT:
					FixEditWindowRect(hWnd);
					break;
				default:
					return DefWindowProc(hWnd,message,wParam,lParam);
			}
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case K_Mc | 'W':
					PostMessage(hWnd,WM_CLOSE,0,0);
					break;

				default: {
					WORD cmd = LOWORD(wParam);

					if ( (cmd >= IDW_MENU) && (cmd <= IDW_MENUMAX) ){	// 0x4000～0x4fff Menu bar ========
						const TCHAR *param;

						param = GetMenuDataString(&PES->ThMenu,
								LOWORD(wParam) - IDW_MENU);
						PP_ExtractMacro(hWnd,NULL,NULL,param,NULL,0);
						break;
					}
					if ( cmd > K_M ){
						SendMessage(PES->hEWnd,WM_PPXCOMMAND,wParam - K_M,0);
						break;
					}
				}
			}
			break;

		case WM_DPICHANGED:
			PPeWMDpiChanged(hWnd,PES,wParam,(RECT *)lParam);
			break;

		default:
			if ( message == WM_PPXCOMMAND ){
				if ( LOWORD(wParam) == KE_getHWND ){
					return (LRESULT)PES->hEWnd;
				}
			}
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
PPXDLL HWND PPXAPI PPEui(HWND hPWnd,const TCHAR *title,const TCHAR *text)
{
	PPeditSTRUCT *PES;
	HWND hPPeWnd,hFWnd;
	WNDCLASS wcClass;
	int modify = 0;
	WINPOS WinPos;

	hFWnd = hPWnd;
	if ( hFWnd == BADHWND ) hFWnd = GetFocus();
										// ウインドウクラスを定義する ---------
	wcClass.style			= CS_BYTEALIGNCLIENT | CS_DBLCLKS;
	wcClass.lpfnWndProc		= PPeProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= DLLhInst;
	wcClass.hIcon			= LoadIcon(DLLhInst,MAKEINTRESOURCE(Ic_PPE));
	wcClass.hCursor			= LoadCursor(NULL,IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= T(PPeditWinClass);
	RegisterClass(&wcClass);
										// ウィンドウを生成する ---------------
	hPPeWnd = CreateWindowEx(PPESTYLEEX,T(PPeditWinClass),MessageText(title),
			PPESTYLE,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
			NULL,NULL,DLLhInst,hFWnd);

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hPPeWnd,GWLP_USERDATA);

	if ( hFWnd != NULL ){ // 位置調整
		RECT deskbox;
		const TCHAR *deftext;

		deftext = title;
		if ( (deftext[0] != '\0') && (deftext[1] != '\0') &&
			 (deftext[2] != '\0') && (deftext[3] != '\0') &&
			 (deftext[4] == '|') ){ // xxxx| なら、位置再現
			memcpy(PES->PosName,title,TSTROFF(4));
			PES->PosName[4] = '\0';
			if ( NO_ERROR == GetCustTable(T("_WinPos"),PES->PosName,&WinPos,sizeof(WinPos)) ){
				modify = 2;
			}
		}
		if ( modify == 0 ) GetWindowRect(hPPeWnd,&WinPos.pos);
		GetDesktopRect(hFWnd,&deskbox);
		// C4701ok
		if ( (WinPos.pos.left < deskbox.left) ||
			 (WinPos.pos.left >= deskbox.right) ){
			WinPos.pos.left = deskbox.left;
			modify++;
		}
		if ( (WinPos.pos.top < deskbox.top) ||
			 (WinPos.pos.top >= deskbox.bottom) ){
			WinPos.pos.top = deskbox.top;
			modify++;
		}
		if ( modify == 1 ){ // 位置調整のみ。サイズ調整の時は後で
			SetWindowPos(hPPeWnd,NULL,WinPos.pos.left,WinPos.pos.top,0,0,
					SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
		}
	}

	{
		HMENU hMenu = CreateMenu();
		ThInit(&PES->ThMenu);
		MakeRootMenu(&PES->ThMenu,hMenu,T("ME_menu"),ppebar);
		SetMenu(hPPeWnd,hMenu);
	}

	if ( text == PPE_TEXT_CMDMODE ){ // *edit,*ppe
		SendMessage(PES->hEWnd,WM_PPXCOMMAND,KE_excmdopen,(LPARAM)title);
	}else if ( text == PPE_TEXT_OPENNEW ){
		if ( 0 == SendMessage(PES->hEWnd,WM_PPXCOMMAND,KE_opennewfile,(LPARAM)title) ){
			SetMessageOnCaption(hPPeWnd,T("(new)"));
		}
	}else if ( text != NULL ){ // text あり ... 表示
		if ( hPWnd == BADHWND ){
			OpenMainFromMem(GetProp(PES->hEWnd,PPxED),0,NULL,text,TSTRLENGTH32(text),0);
		}else{
			SetWindowText(PES->hEWnd,text);
		}
		SendMessage(PES->hEWnd,EM_SETMODIFY,FALSE,0);
	}else{ // text == NULL ... ファイル読み込み
		PPeFileOpen(hPPeWnd,PES->hEWnd,title);
	}
	if ( modify > 1 ){
		MoveWindow(hPPeWnd,WinPos.pos.left,WinPos.pos.top,
			WinPos.pos.right - WinPos.pos.left,
			WinPos.pos.bottom - WinPos.pos.top,TRUE);
		MoveWindowByKey(hPPeWnd,0,0);
	}else{
		FixEditWindowRect(hPPeWnd);
	}

	DragAcceptFiles(hPPeWnd,TRUE);
	ShowWindow(hPPeWnd,SW_SHOWNORMAL);
	if ( hFWnd != NULL ) SetParent(hPPeWnd,NULL);
	return hPPeWnd;
}
