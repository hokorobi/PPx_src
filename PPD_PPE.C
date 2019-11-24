/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library								PPx edit UI
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
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
const TCHAR PPeName[] = T("PPe");

typedef struct {
	PPXAPPINFO info;		// PPx 共通情報 ※必ず先頭に配置
	HWND hEWnd;			// Edit Box の HWND
	HFONT hBoxFont;		// 等幅フォント
	int fontX, fontY;	// hBoxFont の寸法
	DWORD dpi;
	BOOL ShowModify;	// "*" 表示済み
	DYNAMICMENUSTRUCT DynamicMenu;
	TCHAR CurDir[VFPS]; // カレントディレクトリ
	TCHAR PosName[5]; // 窓の位置を記憶する名前
} PPeditSTRUCT;

PPXINMENU PPebarFile[] = {
	{ K_c | K_s | 'N'	, T("New\tCtrl+Shift+N")},
	{ K_c | 'O'	, T("Open...\tCtrl+O")},
	{ K_c | 'S'	, T("Save\tCtrl+S")},
	{ K_c | K_s | 'S'		, T("Save As...\tCtrl+Shift+S")},
	{ K_F1		, T("ext FileMenu\tF1")},
	{PPXINMENY_SEPARATE, NULL },
	{ KE_ec	, T("E&xit\tAlt+F4")},
	{0, NULL}
};
PPXINMENU PPebarEdit[] = {
	{ K_c | 'Z'	, T("&Undo\tCtrl+Z")},
	{PPXINMENY_SEPARATE, NULL },
	{ K_c | 'X',	T("%G\"JMCU|Cut\"(&T)\tCtrl+X")},
	{ K_c | 'C',	T("%G\"JMCL|Copy\"(&C)\tCtrl+C")},
	{ K_c | 'V',	T("%G\"JMPA|Paste\"(&P)\tCtrl+V")},
	{ K_del,		T("%G\"JMDE|Delete\"(&D)\tDelete")},
	{PPXINMENY_SEPARATE, NULL },
	{ K_c | 'F',	T("&Find...\tCtrl+&F")},
	{ K_F3,			T("Find &Next\tF3")},
	{ K_s | K_F3,	T("Find &Prev\tShift + F3")},
	{ K_F7,			T("&Replace...\tF7")},
	{ KE_qj,		T("&Goto line\tCtrl Q + J")},
	{PPXINMENY_SEPARATE, NULL },
	{ K_c | 'Q',	T("Edit menu\tCtrl+&Q")},
	{ K_c | 'K',	T("Menu&2\tCtrl+K")},
	{PPXINMENY_SEPARATE, NULL },
	{ K_c | 'A'		, T("select &All\tCtrl+A")},
	{ K_F5, T("Insert &date\tF5")},
	{0, NULL}
};
PPXINMENU PPebarView[] = {
	{ K_s | K_F2, T("ViewSettings\tShift+F2")},
	{(DWORD_PTR)T("??charset"), NilStr},
	{(DWORD_PTR)T("??returnset"), NilStr},
	{(DWORD_PTR)T("??tabset"), NilStr},
	{0, NULL}
};
PPXINMENU PPebarHelp[] = {
	{ K_ex, T("Paper Plane eUI")},
	{ K_ex, T("Version ") T(FileProp_Version)},
	{ K_ex, T("(") T(__DATE__) T(",") RUNENVSTRINGS T(")")},
	{ K_ex, T("(c)TORO")},
	{0, NULL}
};

PPXINMENUBAR ppebar[] = {
	{T("&File"), PPebarFile},
	{T("&Edit"), PPebarEdit},
	{T("&View"), PPebarView},
	{T("&Help"), PPebarHelp},
	{NULL, NULL}
};

const TCHAR K_ppe[] = T("K_ppe");

/*-----------------------------------------------------------------------------
	桁と幅に合わせてウィンドウを調節する
-----------------------------------------------------------------------------*/

HFONT CreatePPeFont(int dpi)
{
	LOGFONTWITHDPI cursfont;

	GetPPxFont(PPXFONT_F_mes, dpi, &cursfont);
	return CreateFontIndirect(&cursfont.font);
}

void FixEditWindowRect(HWND hWnd)
{
	int mx, my;//, x, y;
	RECT rect;
	PPeditSTRUCT *PES;

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	my = (int)SendMessage(PES->hEWnd, EM_GETLINECOUNT, 0, 0);
	if ( my < 10 ) my = 10;
	if ( my >= 30 ) my = 30;
/*	自動窓枠決定用のコード…使い心地が悪かった(^^;
	for ( y = 1 ; y <= my ; y++ ){
		x = SendMessage(PES->hEWnd, EM_LINEINDEX, y, 0 );// 先頭からのオフセット
		x = SendMessage(PES->hEWnd, EM_LINELENGTH, x, 0 );	// 桁数を得る
		if (x > mx) mx = x;
	}
*/
	mx = 80;

	GetWindowRect(PES->hEWnd, &rect);
	rect.right	= rect.left + (mx + 1) * PES->fontX + GetSystemMetrics(SM_CYHSCROLL);
	rect.bottom	= rect.top  + (my + 1) * PES->fontY + GetSystemMetrics(SM_CXVSCROLL);
	AdjustWindowRectEx(&rect, PPESTYLE | WS_HSCROLL | WS_VSCROLL, TRUE, PPESTYLEEX);
	MoveWindow(hWnd, rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top, TRUE);
}

DWORD_PTR USECDECL PPeInfoFunc(PPeditSTRUCT *PES, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	if ( cmdID <= PPXCMDID_FILL ){
		*uptr->enums.buffer = '\0';
	}else if ( cmdID == PPXCMDID_PPXCOMMAD ){
		SendMessage(PES->hEWnd, WM_PPXCOMMAND, uptr->key, 0);
	}else if ( cmdID == PPXCMDID_ADDEXMENU ){
		ADDEXMENUINFO *addmenu = (ADDEXMENUINFO *)uptr;
		PPxEDSTRUCT *PECS = (PPxEDSTRUCT *)GetProp(PES->hEWnd, PPxED);
		TCHAR buf[80];

		if ( PECS == NULL ) return 0;
		if ( !tstrcmp(addmenu->exname, T("tabset")) ){
			wsprintf(buf, T("&tab: %u"), PECS->tab);
			AppendMenuString(addmenu->hMenu, K_M | KE_2t, buf);
			return PPXCMDID_ADDEXMENU;
		}
		if ( !tstrcmp(addmenu->exname, T("charset")) ){
			UINT cp;
			PPXINMENU *pi;
			InitEditCharCode(PECS);

			cp = PECS->CharCode;
			if ( cp == 0 ) cp = GetACP();
			if ( cp == CP__SJIS ) cp = VTYPE_SYSTEMCP;

			wsprintf(buf, T("&charset : %u"), cp);
			pi = charmenu;
			while ( pi->key ){
				if ( pi->key == cp ){
					wsprintf(buf, T("&Character: %s"), pi->str);
					tstrreplace(buf + 5, T("&"), NilStr);
					break;
				}
				pi++;
			}
			AppendMenuString(addmenu->hMenu, K_M | KE_2c, buf);
			return PPXCMDID_ADDEXMENU;
		}
		if ( !tstrcmp(addmenu->exname, T("returnset")) ){
			const TCHAR *ptr;
			if ( (PECS->CrCode < 0) || (PECS->CrCode >= 3) ){
				ptr = T("?");
			}else{
				ptr = returnmenu[PECS->CrCode].str;
			}
			wsprintf(buf, T("&return: %s"), ptr);
			tstrreplace(buf + 5, T("&"), NilStr);
			AppendMenuString(addmenu->hMenu, K_M | KE_2r, buf);
			return PPXCMDID_ADDEXMENU;
		}
		return 0;
	}
	return 0;
}

LRESULT PPeWmCreate(HWND hWnd)
{
	PPeditSTRUCT *PES;
	HDC hDC;
	HGDIOBJ hOldFont;
	TEXTMETRIC tm;
										// 作業領域を確保 -------------
	PES = HeapAlloc(DLLheap, 0, sizeof(PPeditSTRUCT));
	if ( PES == NULL ) return -1;

	PES->info.Function = (PPXAPPINFOFUNCTION)PPeInfoFunc;
	PES->info.Name = PPeName;
	PES->info.RegID = NilStr;
	PES->info.hWnd = hWnd;

	PES->CurDir[0] = '\0';
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)PES);

										// EditBox を作成 -------------
	PES->hEWnd = CreateWindowEx(WS_EX_ACCEPTFILES, EDITstr, NilStr,
		WS_CHILD | /*WS_HSCROLL |*/ WS_VSCROLL |
		/*ES_AUTOHSCROLL |*/ ES_AUTOVSCROLL | ES_NOHIDESEL |
		ES_LEFT | ES_MULTILINE | ES_WANTRETURN,	// ウインドウの形式
		0, 0, 0, 0, hWnd, NULL, DLLhInst, 0);

	PES->dpi = GetMonitorDPI(hWnd);
	PES->ShowModify = FALSE;
	PES->hBoxFont = CreatePPeFont(PES->dpi);
	hDC = GetDC(hWnd);
	hOldFont = SelectObject(hDC, PES->hBoxFont);
	GetTextMetrics(hDC, &tm);
	PES->fontX = tm.tmAveCharWidth;
	PES->fontY = tm.tmHeight;
	PES->PosName[0] = '\0';
	SelectObject(hDC, hOldFont);
	ReleaseDC(hWnd, hDC);
										// EditBox の Font を等幅に ---
	SendMessage(PES->hEWnd, WM_SETFONT, (WPARAM)PES->hBoxFont, TRUE);
										// EditBox の拡張 -------------
	PPxRegistExEdit(NULL, PES->hEWnd, 0x100000, NULL, 0, 0,
			PPXEDIT_USEALT | PPXEDIT_WANTEVENT | PPXEDIT_NOWORDBREAK);
										// K_ppe を登録 ---
	SendMessage(PES->hEWnd, WM_PPXCOMMAND, KE_setkeya, (LPARAM)K_ppe);
	if ( IsExistCustTable(K_ppe, T("FIRSTEVENT")) &&
		!IsExistCustTable(T("K_edit"), T("FIRSTEVENT")) ){
		PostMessage(PES->hEWnd, WM_PPXCOMMAND, K_E_FIRST, 0);
	}
	ShowWindow(PES->hEWnd, SW_SHOWNORMAL);
	return 0;
}

void PPeWMDpiChanged(HWND hWnd, PPeditSTRUCT *PES, WPARAM wParam, RECT *newpos)
{
	DWORD newDPI = HIWORD(wParam);
	HDC hDC;
	HFONT hNewFont, hOldFont;
	TEXTMETRIC	tm;

	if ( !(X_dss & DSS_ACTIVESCALE) ) return;
	if ( PES->dpi == newDPI ) return; // 変更無し(起動時等)
	PES->dpi = newDPI;

	hNewFont = CreatePPeFont(newDPI);
	SendMessage(PES->hEWnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);
	DeleteObject(PES->hBoxFont);
	PES->hBoxFont = hNewFont;
	hDC = GetDC(hWnd);
	hOldFont = SelectObject(hDC, PES->hBoxFont);
	GetTextMetrics(hDC, &tm);
	PES->fontX = tm.tmAveCharWidth;
	PES->fontY = tm.tmHeight;
	PES->PosName[0] = '\0';
	SelectObject(hDC, hOldFont);
	ReleaseDC(hWnd, hDC);

	if ( newpos != NULL ){
		SetWindowPos(hWnd, NULL, newpos->left, newpos->top,
				newpos->right - newpos->left, newpos->bottom - newpos->top,
				SWP_NOACTIVATE | SWP_NOZORDER);
	}
	InvalidateRect(hWnd, NULL, TRUE);
}


void PPeFileOpen(HWND hWnd, HWND hEdWnd, const TCHAR *fname)
{
	if ( SendMessage(hEdWnd, WM_PPXCOMMAND, KE_openfile, (LPARAM)fname) == FALSE ){
		return;
	}
	SetWindowText(hWnd, fname);
	FixEditWindowRect(hWnd);
}

void PPeWmDROPFILES(HWND hWnd, PPeditSTRUCT *PES, HDROP hDrop)
{
	TCHAR name[VFPS];

	DragQueryFile( hDrop, 0, name, TSIZEOF(name) );
	DragFinish(hDrop);
	PPeFileOpen(hWnd, PES->hEWnd, name);
	SetForegroundWindow(hWnd);
}

void PPeClose(HWND hWnd, PPeditSTRUCT *PES)
{
	WINPOS WinPos;

	if ( (PES->PosName[0] != '\0') &&
		 !IsIconic(hWnd) /*&& IsWindowVisible(hWnd)*/ ){
		WinPos.show = 0;
		GetWindowRect(hWnd, &WinPos.pos);
		SetCustTable(T("_WinPos"), PES->PosName, &WinPos, sizeof(WinPos));
	}
	if ( SendMessage(PES->hEWnd, WM_PPXCOMMAND, KE_closecheck, 0) ){
		DestroyWindow(hWnd); // 窓の破棄(WM_DESTROY)
	}
}

void SetShowModify(PPeditSTRUCT *PES, BOOL modify)
{
	TCHAR buf[CMDLINESIZE];

	PES->ShowModify = modify;
	if ( GetWindowText(PES->info.hWnd, buf, TSIZEOF(buf)) ){
		size_t len = tstrlen(buf);

		if ( modify ){
			if ( (len >= 2) && (buf[len - 1] == '*') ) return;
			tstrcpy(buf + len, T(" *"));
		} else {

			if ( (len < 2) || (buf[len - 2] != ' ') || (buf[len - 1] != '*') ){
				return;
			}
			buf[len - 2] = '\0';
		}
		SetWindowText(PES->info.hWnd, buf);
	}
}

/*-----------------------------------------------------------------------------
	メインルーチン
-----------------------------------------------------------------------------*/
LRESULT CALLBACK PPeProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPeditSTRUCT *PES;

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (message){
		case WM_MENUCHAR:
			return PPxMenuProc(hWnd, message, wParam, lParam);

		case WM_CREATE:
			return PPeWmCreate(hWnd);
/*
		case WM_NCCREATE:
			if ( (X_dss & DSS_COMCTRL) && (OSver.dwMajorVersion >= 10) ){
				PPxCommonCommand(hWnd, 0, K_ENABLE_NC_SCALE);
			}
			return 1;
*/
		case WM_DROPFILES: // D&D 処理
			PPeWmDROPFILES(hWnd, PES, (HDROP)wParam);
			break;

		case WM_SETFOCUS:
			SetFocus(PES->hEWnd);
			break;

		case WM_SIZE:
			MoveWindow(PES->hEWnd, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;

		case WM_CLOSE:	// 終了要求
			PPeClose(hWnd, PES);
			break;

		case WM_DESTROY:
													// 等幅フォントの破棄
			PostMessage(PES->hEWnd, WM_SETFONT, 0, TRUE);
			DeleteObject(PES->hBoxFont);
			FreeDynamicMenu(&PES->DynamicMenu);
			HeapFree(DLLheap, 0, PES);
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
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case K_Mc | 'W':
					PostMessage(hWnd, WM_CLOSE, 0, 0);
					break;

				default: {
					WORD cmd = LOWORD(wParam);

					if ( (cmd >= IDW_MENU) && (cmd <= IDW_MENUMAX) ){	// 0x4000～0x4fff Menu bar ========
						CommandDynamicMenu(&PES->DynamicMenu, &PES->info, wParam);
						return 0;
					}
					if ( cmd > K_M ){
						SendMessage(PES->hEWnd, WM_PPXCOMMAND, wParam - K_M, 0);
						break;
					}
					if ( (HIWORD(wParam) == EN_UPDATE) && (PES->ShowModify == FALSE) ){
						SetShowModify(PES, TRUE);
					}
				}
			}
			break;

		case WM_DPICHANGED:
			PPeWMDpiChanged(hWnd, PES, wParam, (RECT *)lParam);
			break;

		case WM_INITMENU:
			DynamicMenu_InitMenu(&PES->DynamicMenu, (HMENU)wParam, 1);
			break;

		case WM_INITMENUPOPUP:
			DynamicMenu_InitPopupMenu(&PES->DynamicMenu, (HMENU)wParam, &PES->info);
			break;

		default:
			if ( message == WM_PPXCOMMAND ){
				if ( LOWORD(wParam) == KE_getHWND ){
					return (LRESULT)PES->hEWnd;
				}
				if ( LOWORD(wParam) == KE_clearmodify ){
					SetShowModify(PES, FALSE);
					break;
				}
				return ERROR_INVALID_FUNCTION;
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
PPXDLL HWND PPXAPI PPEui(HWND hPWnd, const TCHAR *title, const TCHAR *text)
{
	PPeditSTRUCT *PES;
	HWND hPPeWnd, hFWnd;
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
	wcClass.hIcon			= LoadIcon(DLLhInst, MAKEINTRESOURCE(Ic_PPE));
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= T(PPeditWinClass);
	RegisterClass(&wcClass);
										// ウィンドウを生成する ---------------
	hPPeWnd = CreateWindowEx(PPESTYLEEX, T(PPeditWinClass), MessageText(title),
			PPESTYLE,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, DLLhInst, hFWnd);

	PES = (PPeditSTRUCT *)GetWindowLongPtr(hPPeWnd, GWLP_USERDATA);

	if ( hFWnd != NULL ){ // 位置調整
		RECT deskbox;
		const TCHAR *deftext;

		deftext = title;
		if ( (deftext[0] != '\0') && (deftext[1] != '\0') &&
			 (deftext[2] != '\0') && (deftext[3] != '\0') &&
			 (deftext[4] == '|') ){ // xxxx| なら、位置再現
			memcpy(PES->PosName, title, TSTROFF(4));
			PES->PosName[4] = '\0';
			if ( NO_ERROR == GetCustTable(T("_WinPos"), PES->PosName, &WinPos, sizeof(WinPos)) ){
				modify = 2;
			}
		}
		if ( modify == 0 ) GetWindowRect(hPPeWnd, &WinPos.pos);
		GetDesktopRect(hFWnd, &deskbox);
		// C4701ok (modifyで対応)
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
			SetWindowPos(hPPeWnd, NULL, WinPos. pos.left, WinPos.pos.top, 0, 0,
					SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
		}
	}

	InitDynamicMenu(&PES->DynamicMenu, T("ME_menu"), ppebar);
	SetMenu(hPPeWnd, PES->DynamicMenu.hMenuBarMenu);

	if ( text == PPE_TEXT_CMDMODE ){ // *edit, *ppe
		SendMessage(PES->hEWnd, WM_PPXCOMMAND, KE_excmdopen, (LPARAM)title);
	}else if ( text == PPE_TEXT_OPENNEW ){
		if ( 0 == SendMessage(PES->hEWnd, WM_PPXCOMMAND, KE_opennewfile, (LPARAM)title) ){
			SetMessageOnCaption(hPPeWnd, T("(new)"));
		}
	}else if ( text != NULL ){ // text あり ... 表示
		if ( hPWnd == BADHWND ){
			SendMessage(PES->hEWnd, WM_SETREDRAW, FALSE, 0);
			OpenMainFromMem(GetProp(PES->hEWnd, PPxED), PPE_OPEN_MODE_OPEN, NULL, text, TSTRLENGTH32(text), 0);
			SendMessage(PES->hEWnd, WM_SETREDRAW, TRUE, 0);
		}else{
			SetWindowText(PES->hEWnd, text);
		}
		SendMessage(PES->hEWnd, EM_SETMODIFY, FALSE, 0);
	}else{ // text == NULL ... ファイル読み込み
		PPeFileOpen(hPPeWnd, PES->hEWnd, title);
	}
	if ( modify > 1 ){
		MoveWindow(hPPeWnd, WinPos.pos.left, WinPos.pos.top,
			WinPos.pos.right - WinPos.pos.left,
			WinPos.pos.bottom - WinPos.pos.top, TRUE);
		MoveWindowByKey(hPPeWnd, 0, 0);
	}else{
		FixEditWindowRect(hPPeWnd);
	}

	DragAcceptFiles(hPPeWnd, TRUE);
	ShowWindow(hPPeWnd, SW_SHOWNORMAL);
	if ( hFWnd != NULL ) SetParent(hPPeWnd, NULL);
	return hPPeWnd;
}
