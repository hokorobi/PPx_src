/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer						GUI 関係
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <prsht.h>
#include <wincon.h>
#include "PPX.H"
#include "PPCUST.H"
#pragma hdrstop

const TCHAR StrCustTitle[] = T("PPx Customizer");
#if !NODLL
const TCHAR GetFileExtsStr[] = T("All Files\0*.*\0\0");
#endif
#define PROPPAGE	10	// 全ページ数
#define PROPTITLE	StrCustTitle	// プロパティ自体のタイトル

BOOL Restore = FALSE;	// カスタマイズ内容をもとに戻す必要があれば真
TCHAR Backup[VFPS];	// カスタマイズのバックアップ名
BOOL X_chidc = TRUE; // コンソールを隠すか
HWND hConsoleWnd;
LCID UseLcid = 0;		// 表示言語
#if NODLL
extern DWORD X_dss;
#else
DWORD X_dss = DSS_NOLOAD;	// 画面自動スケーリング
#endif
TCHAR *FirstTypeName = NULL;
TCHAR *FirstItemName = NULL;

TCHAR PPcustRegID[REGIDSIZE] = T(PPCUST_REGID) T("A");

#define DIALOGMARGIN 4
UINT PropMoveTarget[] = {IDOK, IDCANCEL, IDHELP, 12321, 0}; // プロパティシートの位置調整対象

#define EPFLAG_MOVEX B0 // 右に移動
#define EPFLAG_WIDE B1 // 幅を拡げる
#define EPFLAG_MOVEY B2 // 下に移動
#define EPFLAG_TALL B3 // 高さを拡げる
#define EPFLAG_MOVEY_PART B4 // 下の方にあるときだけ、下に移動
typedef struct {
	UINT id; // 対象コントロールID
	UINT flags; // 調整方法
} ExpandListStruct;

ExpandListStruct ExpandList[] = { // ダイアログ幅拡張時の対象
	{IDS_INFO, EPFLAG_WIDE | EPFLAG_TALL},
	{IDT_GENERAL, EPFLAG_TALL},
	{IDT_GENERALITEM, EPFLAG_WIDE | EPFLAG_TALL},
	{IDE_FIND, EPFLAG_WIDE | EPFLAG_MOVEY},
	{IDB_FIND, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDB_TEST, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDV_EXTYPE, EPFLAG_TALL},
	{IDV_ALCLIST, EPFLAG_WIDE | EPFLAG_TALL},
	{IDB_MEUP, EPFLAG_MOVEY},
	{IDB_MEDW, EPFLAG_MOVEY},
	{IDB_MEKEY, EPFLAG_MOVEY},
	{IDB_MECKEYS, EPFLAG_MOVEY},
	{IDS_EXITEM, EPFLAG_MOVEY},
	{IDS_EXTYPE, EPFLAG_MOVEY},
	{IDE_EXTYPE, EPFLAG_MOVEY_PART},
	{IDE_EXITEM, EPFLAG_MOVEY},
	{IDS_EXITEML, EPFLAG_MOVEY},
	{IDB_TB_DELITEM, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDE_ALCCMT, /*EPFLAG_WIDE |*/ EPFLAG_MOVEY},
	{IDB_ALCCMT, /*EPFLAG_MOVEX | */ EPFLAG_MOVEY},
	{IDB_ALCKEY, EPFLAG_MOVEY},
	{IDC_ALCKEYG, EPFLAG_MOVEY},
	{IDC_ALCKEYS, EPFLAG_WIDE | EPFLAG_MOVEY},
	{IDB_TB_SETITEM, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDC_ALCMOUSEB, EPFLAG_MOVEY},
	{IDC_ALCMOUSET, EPFLAG_MOVEY},
	{IDB_ALCMOUSEL, EPFLAG_MOVEY},
	{IDB_ALCMOUSEU, EPFLAG_MOVEY},
	{IDB_ALCMOUSED, EPFLAG_MOVEY},
	{IDB_ALCMOUSER, EPFLAG_MOVEY},
	{IDG_ALCCMD, EPFLAG_WIDE | EPFLAG_MOVEY},
	{IDE_ALCCMD, EPFLAG_WIDE | EPFLAG_MOVEY},
	{IDB_ALCCMDLIST, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDB_ALCCMDI, EPFLAG_MOVEX | EPFLAG_MOVEY},
	{IDE_AOSMASK, EPFLAG_WIDE},
	{IDL_BLIST, EPFLAG_MOVEY},
//	{IDX_BNOBMP, EPFLAG_MOVEY},
	{IDB_BREF, EPFLAG_MOVEY},
	{IDB_HMCHARC, EPFLAG_MOVEY},
	{IDB_HMBACKC, EPFLAG_MOVEY},
//	{IDL_GCTYPE, EPFLAG_TALL},
//	{IDL_GCITEM, EPFLAG_TALL}, // 高さが増えすぎる
	{IDB_GCADD, EPFLAG_MOVEX},
	{IDB_ADDDEFEXT, EPFLAG_MOVEX},
	{IDL_AOSUSIE, EPFLAG_TALL},
	{IDE_AOSINFO, EPFLAG_WIDE | EPFLAG_MOVEY},
	{IDL_EXITEM, EPFLAG_WIDE | EPFLAG_TALL},
	{IDB_REGPPC, EPFLAG_MOVEX},
	{IDB_REGOPEN, EPFLAG_MOVEX},
	{IDB_UNREGPPC, EPFLAG_MOVEX},
	{IDB_TESTKEY, EPFLAG_MOVEX},
	{IDX_CONSOLE, EPFLAG_MOVEX},
	{0, 0}
};

TCHAR *keynames[] = { T("Space"), T("Page Up"), T("Page Down"), T("End"), T("Home"),
	T("←"), T("↑"), T("→"), T("↓")};
// delete ins space

const TCHAR ImmersiveShellPatg[] = T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ImmersiveShell");
const TCHAR TabletMode[] = T("TabletMode");

const TCHAR PropNameInitSheet[] = T("iniSht");

//------------------------------------------------------------------ シート管理
typedef struct {	// 各ページの情報を保存するための構造体
										// あらかじめ初期化する必要があるもの
	int rID;		// リソースID
	DLGPROC proc;	// コールバック関数、NULL なら使わない
} PAGEINFO;

//-------------------------------------
// ※ページの増減があったら、FindKeyword を調整する必要がある。

 PAGEINFO PageInfo[PROPPAGE] = {
	{IDD_INFO, FilePage},
	{IDD_GENERAL, GeneralPage},
	{IDD_EXT, ExtPage},
	{IDD_KEYD, KeyPage},
	{IDD_MOUSED, MousePage},
	{IDD_MENUD, MenuPage},
	{IDD_BARD, BarPage},
	{IDD_COLOR, ColorPage},
	{IDD_ADDON, AddonPage},
	{IDD_ETCTREE, EtcPage},
};

PAGEINFO PageInfoEng[PROPPAGE] = {
	{IDD_INFOE, FilePage},
	{IDD_GENERALE, GeneralPage},
	{IDD_EXTE, ExtPage},
	{IDD_KEYDE, KeyPage},
	{IDD_MOUSEDE, MousePage},
	{IDD_MENUDE, MenuPage},
	{IDD_BARDE, BarPage},
	{IDD_COLORE, ColorPage},
	{IDD_ADDONE, AddonPage},
	{IDD_ETCTREEE, EtcPage},
};

int SubDialogDef[] = {IDD_KEYINPUT, IDD_DISPFOMAT};
int SubDialogEng[] = {IDD_KEYINPUTE, IDD_DISPFOMATE};
int *SubDialog = SubDialogEng;

const TCHAR * USEFASTCALL GetCTextEng(const TCHAR *text)
{
	const TCHAR *newtext = text + tstrlen(text) + 1;
	return (*newtext != '\0') ? newtext : text;
}

const TCHAR * USEFASTCALL GetCTextNative(const TCHAR *text)
{
	return text;
}

const TCHAR * (USEFASTCALL *GetCText)(const TCHAR *text) = GetCTextEng;

void GUILoadCust(void)
{
	GetCustData(T("X_LANG"), &UseLcid, sizeof(UseLcid));
	if ( UseLcid == 0 ) UseLcid = LOWORD(GetUserDefaultLCID());
	if ( UseLcid == LCID_PPXDEF ){
		GetCText = GetCTextNative;
		SubDialog = SubDialogDef;
	}else{
		GetCText = GetCTextEng;
		SubDialog = SubDialogEng;
	}
}

void Changed(HWND hWnd)
{
	PropSheet_Changed(GetParent(hWnd), hWnd);
	Restore = TRUE;
	GUILoadCust();
}
void Test(void)
{
	PPxPostMessage(WM_PPXCOMMAND, K_Lcust, GetTickCount());
	GUILoadCust();
}
#if !NODLL
void USEFASTCALL SetDlgFocus(HWND hDlg, int id)
{
	SetFocus(GetDlgItem(hDlg, id));
}

void EnableDlgWindow(HWND hDlg, int id, BOOL state)
{
	HWND hControlWnd;

	hControlWnd = GetDlgItem(hDlg, id);
	if ( hControlWnd == NULL ) return;
	EnableWindow(hControlWnd, state);
}
#endif
int GetListCursorIndex(WPARAM wParam, LPARAM lParam, int *indexptr)
{
	int index;

	if ( HIWORD(wParam) != LBN_SELCHANGE ) return 0;
	index = (int)SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return 0;
	if ( *indexptr == index ) return -1;
	*indexptr = index;
	return 1;
}

INT_PTR CALLBACK KeyInputDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg){
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
			SetDlgItemText(hDlg, IDP_KEYINPUT_W, (TCHAR *)lParam);
			return TRUE;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDX_KEYINPUT_C:
					SetDlgFocus(hDlg, IDP_KEYINPUT_W);
					break;

				case IDOK:{
					TCHAR *key;

					key = (TCHAR *)GetWindowLongPtr(hDlg, DWLP_USER);
					GetDlgItemText(hDlg, IDP_KEYINPUT_W, key, 32);
					if ( key[0] && (key[tstrlen(key) - 1] == ')') ){
						if ( IsDlgButtonChecked(hDlg, IDX_KEYINPUT_C) ){
							tstrcpy(key, tstrchr(key, '(') + 1);
							key[tstrlen(key) - 1] = '\0';
						}else{
							*tstrchr(key, '(') = '\0';
						}
					}
					EndDialog(hDlg, 1);
					break;
				}

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

int KeyInput(HWND hWnd, TCHAR *string)
{
	return (int)PPxDialogBoxParam(hInst, MAKEINTRESOURCE(SubDialog[DIALOG_KEYINPUT]), hWnd, KeyInputDlgBox, (LPARAM)string);
}

void SetKeyText(HWND hWnd, WORD Vkey, WORD CharKey)
{
	TCHAR CharKeyText[0x100], VKeyText[0x100];
	TCHAR keytext[0x100];

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)Vkey);
	if ( CharKey == 0 ){
		PutKeyCode(keytext, Vkey);
	}else{
		PutKeyCode(VKeyText, Vkey);
		PutKeyCode(CharKeyText, CharKey);
		wsprintf(keytext, T("%s(%s)"), CharKeyText, VKeyText);
	}
	SetWindowText(hWnd, keytext);

	MakeKeyDetailText(Vkey, keytext, FALSE);
	SetDlgItemText(GetParent(hWnd), IDS_EXITEM, keytext);
}

LRESULT CALLBACK KeyProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message){
		case WM_GETDLGCODE:
			return DLGC_WANTALLKEYS | DLGC_BUTTON;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			WORD VKey;

			VKey = (WORD)(wParam | GetShiftKey() | K_v);
			SetKeyText(hWnd, VKey, 0);
			break;
		}

		case WM_SYSCHAR:
		case WM_CHAR: {
			WORD CharKey, VKey;

			CharKey = (WORD)wParam;
			if ( CharKey < 0x7f ){
				if ( IslowerA(CharKey) ) CharKey -= (WORD)0x20;	// 小文字
				if ( IsalnumA(CharKey) || (CharKey <= 0x20) ){
					CharKey |= (WORD)GetShiftKey();
				}else{
					CharKey |= (WORD)(GetShiftKey() & ~K_s);
				}
				VKey = (WORD)GetWindowLongPtr(hWnd, GWLP_USERDATA);
																// ctrl + char
				if ( (CharKey & K_c) && ((CharKey & 0xff) < 0x20) ){
					if ( !IsupperA(VKey) ) break; // Enter とか Break とかを無視
					CharKey += (WORD)0x40;
				}
				if ( (CharKey & 0xff) >= ' ' ){
					SetKeyText(hWnd, VKey, CharKey);
				}
			}
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			int size;
			TCHAR buf[0x100];
			HGDIOBJ hOldFont;

			BeginPaint(hWnd, &ps);
			size = GetWindowText(hWnd, buf, TSIZEOF(buf));
			if ( size != 0 ){
				hOldFont = SelectObject(ps.hdc,
						(HFONT)SendMessage(GetParent(hWnd), WM_GETFONT, 0, 0));
				SetTextColor(ps.hdc, GetSysColor(COLOR_WINDOWTEXT));
				SetBkColor(ps.hdc, GetSysColor(COLOR_WINDOW));
				TextOut(ps.hdc, 4, 1, buf, size);
				SelectObject(ps.hdc, hOldFont);
			}
			EndPaint(hWnd, &ps);
			break;
		}
		case WM_SETTEXT:
			InvalidateRect(hWnd, NULL, TRUE);
//			default へ
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void MakeOneKeyDetailText(int key, TCHAR *dest)
{
	resetflag(key, K_e | K_a | K_c | K_s | K_raw );
	if ( key & K_v ){
		TCHAR *name;

		key &= 0xff;
		switch ( key ){
			case VK_INSERT: // Num Insert になるので置き換え
				name = T("Insert");
				break;
			case VK_DELETE:
				name = T("Delete");
				break;
			case VK_LWIN:
				name = T("LeftWin");
				break;
			case VK_RWIN:
				name = T("RightWin");
				break;
			case VK_APPS:
				name = T("Application");
				break;
			case VK_NUMLOCK:
				name = T("NumLock");
				break;
			default:
				if ( (key >= 0x20) && (key <= 0x28) ){
					name = keynames[key - 0x20];
					break;
				}
				*dest = '\0';
				GetKeyNameText(MapVirtualKey(key, 0) << 16, dest, 64);
				if ( *dest == '\0' ) wsprintf(dest, T("V_H%X"), key);
				return;
		}
		tstrcpy(dest, name);
	}else{
		if ( key == ' ' ){
			tstrcpy(dest, T("Space"));
		}else{
			*dest++ = (TCHAR)(key & 0x7f);
			*dest = '\0';
		}
	}
}

void MakeKeyDetailText(int key, TCHAR *dest, BOOL extype)
{
	if ( key & K_e ){
		int X_es = 0x1d;

		if ( extype ){
			GetCustData(T("X_es"), &X_es, sizeof(X_es));
			MakeOneKeyDetailText(X_es, dest);
		}else{
			tstrcpy(dest, T("Win"));
		}
		dest += tstrlen(dest);
		*dest++ = '+';
	}
	if ( key & K_a ) dest += wsprintf(dest, T("Alt+"));
	if ( key & K_c ) dest += wsprintf(dest, T("Ctrl+"));
	if ( key & K_s ) dest += wsprintf(dest, T("Shift+"));
	MakeOneKeyDetailText(key, dest);
}

//-------------------------------------------------------------------
INT_PTR CALLBACK StyleDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_DESTROY:
			if ( PPxGetHWND(PPcustRegID) != NULL ){ // この中は１回だけ呼び出される
				PPxRegist(NULL, PPcustRegID, PPXREGIST_FREE);
			}
			break;

		case WM_CONTEXTMENU:
			if ( (HWND)wParam == hDlg ) break;
		case WM_HELP:
			PPxHelp(hDlg, HELP_CONTEXT, wParam);
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch (NHPTR->code){
				case PSN_APPLY:
					if ( IsTrue(Restore) ){
						Restore = FALSE;
						Test();
						Print(T("*backup..."));
						CustDump(Backup);
					}
					break;

				case PSN_HELP:
					PPxHelp(NHPTR->hwndFrom, HELP_CONTEXT, wParam);
					break;

				default:
					return FALSE;
			}
			break;
			#undef NHPTR
		case WM_CLOSE:	// ※PPxDialogHelper では WM_CLOSE で EndDialogするから
			break;
		default:	// 特になし
			return PPxDialogHelper(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}

void GUIcustomizer(int startpage, const TCHAR *param)
{
	PROPSHEETHEADER head;
	PROPSHEETPAGE page[PROPPAGE];
	PAGEINFO *pinfo = PageInfoEng;
	TCHAR temp[VFPS];
	TCHAR firstitem[CMDLINESIZE];
	int i;
	WNDCLASS wcClass;
	DWORD TabletModeValue = 0;

	OSver.dwOSVersionInfoSize = sizeof(OSver);
	GetVersionEx(&OSver);
	SetErrorMode(SEM_FAILCRITICALERRORS); // 致命的エラーを取得可能にする
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	LoadCommonControls(ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES);

	hInst = GetModuleHandle(NULL);
	PPxCommonExtCommand(K_SETAPPID, 'c');

	GUILoadCust();
	if ( UseLcid == LCID_PPXDEF ) pinfo = PageInfo;

#ifndef WINEGCC				// コンソールウィンドウが開いているか確認する
	GetRegString(HKEY_CURRENT_USER, ImmersiveShellPatg, TabletMode, (TCHAR *)&TabletModeValue, sizeof(TabletModeValue));
	if ( TabletModeValue != 0 ){
		hConsoleWnd = NULL; // TabletMode時はウィンドウが隠れてしまうので無効に
	}else{
		DefineWinAPI(HWND, GetConsoleWindow, (void));

		GETDLLPROC(GetModuleHandle(T("Kernel32.DLL")), GetConsoleWindow);
		if ( DGetConsoleWindow != NULL ){
			hConsoleWnd = DGetConsoleWindow();
		}else{
			wsprintf(temp, T("+%X)"), GetCurrentThreadId());
			SetConsoleTitle(temp);
			for( i = 0 ; i < 30 ; i++ ){
				if ( (hConsoleWnd = FindWindow(NULL, temp)) != NULL ) break;
				Sleep(100);
			}
		}
	}

	GetCustData(T("X_chidc"), &X_chidc, sizeof(X_chidc));
	if ( IsTrue(X_chidc) && (hConsoleWnd != NULL) ){
		ShowWindow(hConsoleWnd, SW_HIDE);
	}
#else
	X_chidc = FALSE;
	hConsoleWnd = NULL;
#endif
	SetConsoleTitle(T("PPcust log"));

	if ( OSver.dwMajorVersion >= 6  ){
		GetCustData(T("X_dss"), &X_dss, sizeof(X_dss));
	}

	if ( (param != NULL) && (*param >= ' ') ){
		GetLineParam(&param, firstitem);
		FirstTypeName = firstitem;
		if ( *FirstTypeName == 'X' ){ // general
			startpage = 1;
		}else{ // menu
			FirstItemName = tstrchr(firstitem, ':');
			if ( FirstItemName != NULL ) *FirstItemName++ = '\0';
			startpage = 5;
		}
	}
	if ( startpage >= 0 ){
		wcClass.style			= 0;
		wcClass.lpfnWndProc		= KeyProc;
		wcClass.cbClsExtra		= 0;
		wcClass.cbWndExtra		= 0;
		wcClass.hInstance		= hInst;
		wcClass.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IC_CUST));
		wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wcClass.hbrBackground	= WNDCLASSBRUSH(COLOR_WINDOW + 1);
		wcClass.lpszMenuName	= NULL;
		wcClass.lpszClassName	= T(PPXKEYCLASS);
											// クラスを登録する
		RegisterClass(&wcClass);
												// 初期設定
		head.dwSize		= sizeof(PROPSHEETHEADER);
		head.dwFlags	= PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_HASHELP;
		head.hwndParent	= NULL;
		head.hInstance	= hInst;
		UNIONNAME(head, pszIcon) = MAKEINTRESOURCE(IC_CUST);
		head.pszCaption	= PROPTITLE;
		head.nPages		= PROPPAGE;
		UNIONNAME2(head, nStartPage) = startpage;
		UNIONNAME3(head, ppsp) = page;

		for ( i = 0 ; i < PROPPAGE ; i++, pinfo++ ){
			page[i].dwSize		= sizeof(PROPSHEETPAGE);
			page[i].dwFlags		= PSP_HASHELP;
			page[i].hInstance	= hInst;
			UNIONNAME(page[i], pszTemplate) = MAKEINTRESOURCE(pinfo->rID);
			page[i].pfnDlgProc	= pinfo->proc ? pinfo->proc : StyleDlgProc;
			page[i].lParam		= (LPARAM)i;
		}
		MakeTempEntry(VFPS, Backup, FILE_ATTRIBUTE_NORMAL);
		wsprintf(temp, T("*Customize backup file:%s\r\n"), Backup);
		Print(temp);
		Print(T("*backup..."));
		CustDump(Backup);
//		PPxHookEdit(1);	// 入れると異常動作するため保留
		PropertySheet(&head);
//		PPxHookEdit(-1);
	}else{
		PPxDialogBoxParam(hInst, MAKEINTRESOURCE(SubDialog[DIALOG_DISPFOMAT]),
				PPcGetWindow(0, CGETW_GETFOCUS),
				DispFormatDialogBox, (LPARAM)NULL);
	}

	if ( IsTrue(X_chidc) && (hConsoleWnd != NULL) ){
		ShowWindow(hConsoleWnd, SW_SHOW);
		if ( startpage >= 0 ){
			SetForegroundWindow(hConsoleWnd);
		}else{
			SetForegroundWindow(PPcGetWindow(0, CGETW_GETFOCUS));
		}
	}

	if ( startpage >= 0 ){
		if ( Restore ){
			Print(T("*Customize restoring...") TNL);
			InitCust();
			CustStore(Backup, PPXCUSTMODE_temp_RESTORE, NULL);
		}
		DeleteFile(Backup);
		Print(T("*Deleted customize backup file."));
	}
	CoUninitialize();
}
#if !NODLL
void ShowDlgWindow(const HWND hDlg, const UINT id, BOOL show)
{
	ShowWindow(GetDlgItem(hDlg, id), show ? SW_SHOW : SW_HIDE);
}
#endif
TCHAR *LoadTextResource(LPCTSTR rname)
{
	TCHAR *ptr, *text;
	HRSRC hRcust;
	DWORD size;
#ifdef UNICODE
	DWORD rsize;
	char *rp;
	UINT cp;

	hRcust = FindResource(hInst, rname, RT_RCDATA);
	rsize = SizeofResource(hInst, hRcust);
	rp = LockResource(LoadResource(hInst, hRcust));

	cp = IsValidCodePage(CP__SJIS) ? CP__SJIS : CP_ACP;
	size = TSTROFF(MultiByteToWideChar(cp, MB_PRECOMPOSED, rp, rsize, NULL, 0));
	text = HeapAlloc(GetProcessHeap(), 0, size + 4);
	if ( text == NULL ) return L"";
	MultiByteToWideChar(cp, MB_PRECOMPOSED, rp, rsize, text, size);
	memset((char *)text + size, 0, 4);
#else
	hRcust = FindResource(hInst, rname, RT_RCDATA);
	size = SizeofResource(hInst, hRcust);
	text = HeapAlloc(GetProcessHeap(), 0, size + 4);
	if ( text == NULL ) return "";
	memcpy(text, LockResource(LoadResource(hInst, hRcust)), size);
	memset(text + size, 0, 4);
#endif
	ptr = text;
	for ( ;; ){
		ptr = tstrchr(ptr, '\r');
		if ( ptr == NULL ) break;
		*ptr++ = '\0';
	}
	return text;
}

BOOL FixControlSize(HWND hWnd, SIZE *lsize, RECT *basebox)
{
	RECT box;

	if ( hWnd == NULL ) return FALSE;
	GetWindowRect(hWnd, &box);
	if ( (basebox == NULL) || (box.right < basebox->right) || (box.bottom < basebox->bottom) ){
		SetWindowPos(hWnd, NULL, 0, 0,
				box.right - box.left + lsize->cx,
				box.bottom - box.top + lsize->cy,
				SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
		return TRUE;
	}
	return FALSE;
}

void InitWndIcon(HWND hDlg, UINT baseControlID)
{
	HWND hBaseWnd, hPropWnd;
	RECT DialogBox, DeskBox, BaseBox, TempBox;
	POINT pos;
	SIZE lsize = { 0, 0 };
	UINT *ctrlID;
	int ExtendWidth, ExtendHeight;

	hPropWnd = GetParent(hDlg);

	// 右下のコントロールが表示されないときは、表示されるように大きさ調整する
	if ( baseControlID == 0 ) return;
	hBaseWnd = GetDlgItem(hDlg, baseControlID);
	if ( hBaseWnd == NULL ) return;

		// Task 登録
	PPxRegist(hPropWnd, PPcustRegID, PPXREGIST_NORMAL);

		// ウィンドウのアイコンを設定する
	SetClassLongPtr(hPropWnd, GCLP_HICON,
			(LONG_PTR)LoadIcon(hInst, MAKEINTRESOURCE(IC_CUST)));

	GetWindowRect(hDlg, &DialogBox);
	GetWindowRect(hBaseWnd, &BaseBox);
	GetDesktopRect(hPropWnd, &DeskBox);

	if ( (OSver.dwMajorVersion >= 5) &&
		 (GetProp(hBaseWnd, PropNameInitSheet) == NULL) ){
		int DialogAWidth, DialogMaxWidth, DialogExtendWidth;
		int DialogAHeight, DialogMaxHeight, DialogExtendHeight;

		SetProp(hBaseWnd, PropNameInitSheet, INVALID_HANDLE_VALUE);

		// 大まかなダイアログサイズを算出
		DialogAWidth = BaseBox.right - DialogBox.left;
		DialogAWidth += DialogAWidth / 10; // ダイアログの大きさを少し小さく
		DialogAHeight = BaseBox.bottom - DialogBox.top;
		DialogAHeight += DialogAHeight / 10; // ダイアログの大きさを少し小さく
		// 大まかなダイアログボックス幅 x 1.3 <= デスクトップ幅まで拡張
		DialogMaxWidth = DialogAWidth + DialogAWidth / 3;
		DialogExtendWidth = DeskBox.right - DeskBox.left;
		if ( DialogAWidth > DialogExtendWidth ){
			DialogExtendWidth = DialogAWidth;
		}
		// 大まかなダイアログボックス高 x 1.2 <= デスクトップ高まで拡張
		DialogMaxHeight = DialogAHeight + DialogAHeight / 5;
		DialogExtendHeight = DeskBox.bottom - DeskBox.top;
		if ( DialogAHeight > DialogExtendHeight ){
			DialogExtendHeight = DialogAHeight;
		}
		if ( (DialogAWidth < DialogExtendWidth) ||
			 (DialogAHeight < DialogExtendHeight) ){
			ExpandListStruct *exl;

			if ( DialogExtendWidth >= DialogMaxWidth ){
				DialogExtendWidth = DialogMaxWidth;
			}
			ExtendWidth = DialogExtendWidth - DialogAWidth;
			BaseBox.right += ExtendWidth;
			if ( DialogExtendHeight >= DialogMaxHeight ){
				DialogExtendHeight = DialogMaxHeight;
			}
			ExtendHeight = DialogExtendHeight - DialogAHeight;
			BaseBox.bottom += ExtendHeight;
			ExtendHeight -= 4;
			if ( ExtendHeight < 0 ) ExtendHeight = 0;

			for ( exl = ExpandList ; exl->id != 0 ; exl++ ){
				HWND hWnd;

				hWnd = GetDlgItem(hDlg, exl->id);
				if ( hWnd == NULL ) continue;

				GetWindowRect(hWnd, &TempBox);
				pos.x = TempBox.left - DialogBox.left;
				pos.y = TempBox.top - DialogBox.top;
				if ( exl->flags & EPFLAG_MOVEX ) pos.x += ExtendWidth;
				if ( exl->flags & EPFLAG_WIDE ) TempBox.right += ExtendWidth;
				if ( exl->flags & EPFLAG_MOVEY ) pos.y += ExtendHeight;
				if ( exl->flags & EPFLAG_TALL ) TempBox.bottom += ExtendHeight;
				if ( exl->flags & EPFLAG_MOVEY_PART ){ // マウス割当て以外のみ
					if ( pos.y > (DialogAHeight / 5) ){
						pos.y += ExtendHeight;
					}
				}
				SetWindowPos(hWnd, NULL, pos.x, pos.y,
						TempBox.right - TempBox.left, TempBox.bottom - TempBox.top,
						SWP_NOACTIVATE | SWP_NOZORDER);
			}
		}
	}
	if ( (DialogBox.right >= BaseBox.right) &&
		 (DialogBox.bottom >= BaseBox.bottom) ){
		return; // 大きさ調整の必要なし
	}
	// 足りない大きさを算出
	if ( DialogBox.right < BaseBox.right ){
		lsize.cx = BaseBox.right + DIALOGMARGIN - DialogBox.right;
		DialogBox.right += lsize.cx;
	}
	if ( DialogBox.bottom < BaseBox.bottom ){
		lsize.cy = BaseBox.bottom + DIALOGMARGIN - DialogBox.bottom;
		DialogBox.bottom += lsize.cy;
	}
	FixControlSize(hDlg, &lsize, &DialogBox);		// プロパティシートの大きさ修正
	FixControlSize(GetDlgItem(hPropWnd, 12320), &lsize, &DialogBox); // tab control
	FixControlSize(hPropWnd, &lsize, NULL);	// 親ウィンドウの大きさ修正

	GetWindowRect(hPropWnd, &TempBox);
	if ( TempBox.right > DeskBox.right ){ // プロパティシートを画面内に移動
		TempBox.left -= TempBox.right - DeskBox.right;
		if ( TempBox.left < DeskBox.left ) TempBox.left = DeskBox.left;
		SetWindowPos(hPropWnd, NULL, TempBox.left, TempBox.top,
				0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
	}

	// 各種ボタンの位置修正
	for ( ctrlID = PropMoveTarget ; *ctrlID != 0 ; ctrlID++ ){
		HWND hWnd;

		hWnd = GetDlgItem(hPropWnd, *ctrlID);
		if ( hWnd == NULL ) continue;
		GetWindowRect(hWnd, &TempBox);
		pos.x = TempBox.left;
		pos.y = TempBox.top;
		ScreenToClient(hPropWnd, &pos);
		SetWindowPos(hWnd, NULL, pos.x + lsize.cx, pos.y + lsize.cy,
				0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
	}
	InvalidateRect(hPropWnd, NULL, TRUE);
}

#ifndef UNICODE
const WCHAR *GetCTextW(const WCHAR *text)
{
	const WCHAR *newtext;

	if ( UseLcid == LCID_PPXDEF ){
		return text;
	}

	newtext = text + strlenW(text) + 1;
	return (*newtext != '\0') ? newtext : text;
}
#endif

void GetControlText(HWND hDlg, UINT ID, TCHAR *text, DWORD len)
{
	TCHAR *texttop, *textlast;
	size_t toplen;

	text[0] = '\0';
	GetDlgItemText(hDlg, ID, text, len);
	texttop = text;
	for (;;){
		UTCHAR chr;

		chr = (UTCHAR)*texttop;
		if ( (chr == '\0') || (chr > ' ') ) break;
		texttop++;
	}
	toplen = tstrlen(texttop);
	if ( texttop > text ) memmove(text, texttop, (toplen + 1) * sizeof(TCHAR));
	textlast = text + toplen;
	while ( textlast > text ){
		if ( (UTCHAR)*(textlast - 1) > ' ' ) break;
		textlast--;
		*textlast = '\0';
	}
}
