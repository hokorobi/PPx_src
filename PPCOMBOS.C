/*-----------------------------------------------------------------------------
	Paper Plane cUI			Combo Window Sub
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#include "PPCUI.RH"
#include "PPXVER.H"
#pragma hdrstop

TypedefWinAPI(HRESULT, DwmGetColorizationColor, (DWORD*, BOOL*));

int X_lspc = 0;

void CreateAddressBar(void);
void CreateReportArea(void);

// ペインフレーム =============================================================
void USEFASTCALL ComboFrameHscroll(HWND hWnd, WORD scrollcode)
{
	switch ( scrollcode ){
								// 一番上 .............................
		case SB_TOP:
			Combo.Panes.delta.x = 0;
			break;
								// 一番下 .............................
		case SB_BOTTOM:
			Combo.Panes.delta.x = 100;
			break;
								// 一行上 .............................
		case SB_LINEUP:
			Combo.Panes.delta.x+= 100;
			break;
								// 一行下 .............................
		case SB_LINEDOWN:
			Combo.Panes.delta.x-= 100;
			break;
								// 一頁上 .............................
		case SB_PAGEUP:
			Combo.Panes.delta.x+= 500;
			break;
								// 一頁下 .............................
		case SB_PAGEDOWN:
			Combo.Panes.delta.x-= 500;
			break;
								// 特定位置まで移動中 .................
		case SB_THUMBTRACK:
			//SB_THUMBPOSITION へ続く
								// 特定位置まで移動 ...................
		case SB_THUMBPOSITION:{
			SCROLLINFO scri;

			scri.cbSize = sizeof(scri);
			scri.fMask = SIF_TRACKPOS;
			GetScrollInfo(hWnd, SB_HORZ, &scri);
			Combo.Panes.delta.x = scri.nTrackPos;
			break;
		}
	}
	SortComboWindows(SORTWIN_LAYOUTPAIN);
	InvalidateRect(hWnd, NULL, TRUE);
}

void PaintComboFrame(HWND hWnd)
{
	DRAWCOMBOSTRUCT dcs;

	BeginPaint(hWnd, &dcs.ps);
	dcs.hBr = NULL;

	DrawPaneArea(&dcs);
	if ( dcs.hBr != NULL ) DeleteObject(dcs.hBr);
	EndPaint(hWnd, &dcs.ps);
}

LRESULT ComboFrameLMouseDown(HWND hWnd, LPARAM lParam)
{
	POINT pos;
	int showindex;

	LPARAMtoPOINT(pos, lParam);
	if ( (pos.y < 0) || (pos.x < 0) ) return 0;
	SetCapture(hWnd);

	showindex = GetComboShowIndexFromPos(&pos);
	if ( showindex >= 0 ) MoveSplit = showindex;
	return 0;
}

LRESULT CALLBACK ComboFrameProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message){
							// 垂直スクロール
		case WM_VSCROLL:
							// 水平スクロール
		case WM_HSCROLL:
			ComboFrameHscroll(hWnd, LOWORD(wParam));
			break;

		case WM_PARENTNOTIFY:
			if ( Combo.hWnd == BADHWND ) break; // 終了動作中
			if ( LOWORD(wParam) == WM_DESTROY ){
				ComboProc(Combo.hWnd, message, wParam, lParam);
			}
			break;

		case WM_PAINT:
			PaintComboFrame(hWnd);
			break;

		case WM_LBUTTONDOWN:	return ComboFrameLMouseDown(hWnd, lParam);
		case WM_LBUTTONUP:		return ComboLMouseUp(lParam);
		case WM_MOUSEMOVE:		return ComboMouseMove(hWnd, lParam);

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// アドレスバー ==============================================================
void EnterAddressBar(void)
{
	TCHAR cmdline[CMDLINESIZE];

	SendMessage(Combo.hAddressWnd, WM_GETTEXT, CMDLINESIZE, (LPARAM)cmdline);
	SendMessage(hComboFocus, WM_PPXCOMMAND, KCW_enteraddress, (LPARAM)cmdline);
	SetFocus(hComboFocus);
}

void SetComboAddresBar(const TCHAR *path)
{
	if ( Combo.hAddressWnd != NULL ) SetWindowText(Combo.hAddressWnd, path);
	if ( comboDocks.t.hAddrWnd != NULL ) SetWindowText(comboDocks.t.hAddrWnd, path);
	if ( comboDocks.b.hAddrWnd != NULL ) SetWindowText(comboDocks.b.hAddrWnd, path);
}

// ログ窓 ====================================================================
void SetComboReportText(const TCHAR *text)
{
	if ( text == INVALID_HANDLE_VALUE ){
		text = (Combo.Report.hWnd == NULL) ? NULL : REPORTTEXT_CLOSE;
	}
	if ( text == REPORTTEXT_FOCUS ){
		SetFocus(Combo.Report.hWnd);
		return;
	}
	if ( text == REPORTTEXT_CLOSE ){
		PPxCommonExtCommand(K_SETLOGWINDOW, 0);
		PostMessage(Combo.Report.hWnd, WM_CLOSE, 0, 0);
		Combo.Report.hWnd = NULL;
		Combo.BottomAreaHeight = 0;
		resetflag(X_combos[0], CMBS_COMMONREPORT);
		SortComboWindows(SORTWIN_LAYOUTALL);
		return;
	}

	if ( Combo.Report.hWnd == NULL ){
		setflag(X_combos[0], CMBS_COMMONREPORT);
		CreateReportArea();
		SortComboWindows(SORTWIN_LAYOUTALL);
	}
	if ( text != NULL ) SetReportTextMain(Combo.Report.hWnd, text);
}

// 初期化関連 =================================================================
// フォント生成 -------------------------
void ComboInitFont(void)
{
	TEXTMETRIC tm;
	HDC hDC;
	HGDIOBJ hOldFont;	//一時保存用

	Combo.Font.handle = CreateMesFont(0, Combo.FontDPI);

	hDC = GetDC(Combo.hWnd);

	splitwide = (GetSystemMetrics(SM_CXSIZEFRAME) * Combo.FontDPI) / GetDeviceCaps(hDC, LOGPIXELSX);

	hOldFont = SelectObject(hDC, Combo.Font.handle);
										// フォント情報を入手
	GetAndFixTextMetrics(hDC, &tm);
//	Combo.Font.size.cx = tm.tmAveCharWidth;
	Combo.Font.size.cy = tm.tmHeight + X_lspc;
	SelectObject(hDC, hOldFont);
	ReleaseDC(Combo.hWnd, hDC);

	if ( X_combos[0] & CMBS_COMMONINFO ){
		InfoHeight = Combo.Font.size.cy * 2;
		if ( XC_ifix && (InfoHeight < XC_ifix) ) InfoHeight = XC_ifix;
	}
}

void LoadCombos(void)
{
	GetCustData(T("X_combos"), &X_combos, sizeof(X_combos));

	if ( (X_combos[0] & (CMBS_TABSEPARATE | CMBS_TABEACHITEM)) != (CMBS_TABSEPARATE | CMBS_TABEACHITEM) ){
		resetflag(X_combos[0], CMBS_TABEACHITEM);
	}

	if ( (X_combos[0] & (CMBS_VPANE | CMBS_QPANE)) == (CMBS_VPANE | CMBS_QPANE) ){
		resetflag(X_combos[0], CMBS_VPANE);
	}
}

void InitComboGUI(void)
{
	UINT ID;

	// 使っていたのを閉じる
	if ( Combo.ToolBar.hWnd != NULL ){
		Combo.ToolBar.Height = 0;
		CloseToolBar(Combo.ToolBar.hWnd);
		Combo.ToolBar.hWnd = NULL;
	}
//	BackupLog(); // ReportArea を再作成しないので無効
	if ( comboDocks.t.hWnd != NULL ) DestroyWindow(comboDocks.t.hWnd);
	if ( comboDocks.b.hWnd != NULL ) DestroyWindow(comboDocks.b.hWnd);

	// 初期化開始
	ThFree(&thGuiWork);
	ComboInitFont();

	if ( X_combos[0] & CMBS_TOOLBAR ) ComboCreateToolBar(Combo.hWnd);
	ID = ComboCommandIDFirst;
	DocksInit(&comboDocks, Combo.hWnd, NULL, ComboID, Combo.Font.handle, Combo.Font.size.cy, &thGuiWork, &ID);
	if ( X_combos[0] & CMBS_COMMONADDR ){
		if ( Combo.hAddressWnd == NULL ) CreateAddressBar();
	}else{
		if ( Combo.hAddressWnd != NULL ){
			DestroyWindow(Combo.hAddressWnd);
			Combo.hAddressWnd = NULL;
			AddrHeight = 0;
		}
	}
//	RestoreLog(); // ReportArea を再作成しないので無効
	KCW_FocusFix(Combo.hWnd, hComboFocus);
	SortComboWindows(SORTWIN_LAYOUTALL);
}

COLORREF USEFASTCALL GlassFix(COLORREF color)
{
	if ( (OSver.dwMajorVersion != 6) || (OSver.dwMinorVersion > 1) ){
		return color;
	}
	color = color + 0x50;
	if ( color >= 0x100 ) color = 0xff;
	return color;
}

void ComboCust(void)
{
	DWORD oldcombos;
	TCHAR buf[CMDLINESIZE];

	oldcombos = X_combos[0];

	LoadCombos();

	if ( (oldcombos ^ X_combos[0]) & CMBS_TABEACHITEM ){
		X_combos[0] = oldcombos;
		ReplyMessage(0);
		XMessage(Combo.hWnd, NULL, XM_FaERRld, MES_RTPC);
	}

	GetCustData(T("XC_ifix"), &XC_ifix, sizeof(XC_ifix));
	if ( X_combo == 2 ) X_combos[0] |= CMBS_TABALWAYS;
	GetCustData(T("X_lspc"), &X_lspc, sizeof(X_lspc));

	GetCustData(T("C_capt"), &C_capt, sizeof(C_capt));
	if ( C_ActiveCBack == C_AUTO ){
		C_ActiveCBack = GetSysColor(COLOR_ACTIVECAPTION);

		if ( hDwmapi != NULL ){
			ValueWinAPI(DwmGetColorizationColor);
			ValueWinAPI(DwmIsCompositionEnabled);

			GETDLLPROC(hDwmapi, DwmGetColorizationColor);
			GETDLLPROC(hDwmapi, DwmIsCompositionEnabled);
			if ( DDwmGetColorizationColor != NULL ){
				BOOL tmp = FALSE;

				DDwmIsCompositionEnabled(&tmp);
				if ( tmp && SUCCEEDED(DDwmGetColorizationColor(&C_ActiveCBack, &tmp)) ){
					C_ActiveCBack = (GlassFix(C_ActiveCBack & 0xff) << 16) | (GlassFix((C_ActiveCBack & 0xff00) >> 8) << 8) | GlassFix((C_ActiveCBack & 0xff0000) >> 16);
					if ( C_ActiveCText == C_AUTO ){
						C_ActiveCText = (GetColorBright(C_ActiveCBack) > 0x500) ? C_BLACK : C_WHITE;
					}
				}
			}
		}
	}
	if ( C_ActiveCText == C_AUTO ) C_ActiveCText = GetSysColor(COLOR_CAPTIONTEXT);
	if ( C_PairCText == C_AUTO ) C_PairCText = GetSysColor(COLOR_INACTIVECAPTIONTEXT);
	if ( C_InActiveCText == C_AUTO ) C_InActiveCText = GetSysColor(COLOR_CAPTIONTEXT);
	if ( C_PairCBack == C_AUTO ) C_PairCBack = GetSysColor(COLOR_INACTIVECAPTION);
	if ( C_InActiveCBack == C_AUTO ) C_InActiveCBack = GetSysColor(COLOR_INACTIVECAPTION);

	if ( TabCaptionText != NULL ){
		PPcHeapFree(TabCaptionText);
		TabCaptionText = NULL;
	}
	buf[0] = '\0';
	GetCustData(T("X_tcap"), &buf, sizeof(buf));
	if ( buf[0] == '\0' ){
		TabCaptionType = 0;
	}else if ( Isdigit(buf[0]) && (buf[1] == '\0') ){
		TabCaptionType = buf[0] - '0';
	}else{
		TabCaptionType = 0;
		TabCaptionText = PPcHeapAlloc(TSTROFF(CMDLINESIZE));
		if ( TabCaptionText != NULL ) tstrcpy(TabCaptionText, buf);
	}

	if ( (Combo.hWnd != NULL) && (Combo.hWnd != BADHWND) ){
		InitComboGUI();
	}
}

// Tree =======================================================================
void CreateBottomArea(void)
{
	TCHAR buf[8];
	DWORD tmp[2];

	if ( Combo.BottomAreaHeight >= AREAMINSIZE ) return;
	wsprintf(buf, T("%sL"), ComboID);
	tmp[1] = BOTTOM_AREAMINSIZE;
	GetCustTable(T("XC_tree"), buf, &tmp, sizeof(tmp));
	Combo.BottomAreaHeight = tmp[1];
	if ( Combo.BottomAreaHeight < AREAMINSIZE ) Combo.BottomAreaHeight = AREAMINSIZE;
}

void CreateReportArea(void)
{
	GetCustData(T("X_askp"), &X_askp, sizeof(X_askp));
	CreateBottomArea();
	InitEditColor();
	Combo.Report.hWnd = CreateWindowEx(0, T("EDIT"), NilStr,
			WS_CHILD | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL |
			ES_LEFT | ES_MULTILINE | ES_WANTRETURN,
			0, Combo.Report.box.y,
			ComboSize.cx, Combo.BottomAreaHeight,
			Combo.hWnd, CHILDWNDID(IDW_REPORTLOG), hInst, 0);
												// EditBox の拡張 -------------
	SendMessage(Combo.Report.hWnd, WM_SETFONT, (WPARAM)Combo.Font.handle, 0);
	PPxRegistExEdit(NULL, Combo.Report.hWnd, 0x100000, NULL, 0, 0,
			X_askp ?
				PPXEDIT_USEALT | PPXEDIT_WANTENTER | PPXEDIT_WANTEVENT |
				PPXEDIT_TABCOMP | PPXEDIT_NOWORDBREAK | PPXEDIT_PANEMODE:
				PPXEDIT_USEALT | PPXEDIT_WANTENTER | PPXEDIT_WANTEVENT |
				PPXEDIT_TABCOMP | PPXEDIT_NOWORDBREAK);
	ShowWindow(Combo.Report.hWnd, SW_SHOWNORMAL);
	PPxCommonExtCommand(K_SETLOGWINDOW, (WPARAM)Combo.hWnd);
}

void CreateJobArea(void)
{
	TCHAR buf[8];
	DWORD tmp[2];

	if ( Combo.hTreeWnd == NULL ) CreateBottomArea();

	wsprintf(buf, T("%sJ"), ComboID);
	tmp[1] = 200;
	GetCustTable(T("XC_tree"), buf, &tmp, sizeof(tmp));
	Combo.Joblist.JobAreaWidth = tmp[1];
	PPxCommonCommand(Combo.hWnd, (LPARAM)&Combo.Joblist.hWnd, K_GETJOBWINDOW);
	if ( Combo.Joblist.hWnd == NULL ){
		PPxCommonCommand(Combo.hWnd, 1, K_GETJOBWINDOW);
	}
}

void CreateLeftArea(DWORD mode, const TCHAR *initpath)
{
	PPCTREESETTINGS pts;

	pts.mode = PPCTREE_SELECT;
	pts.width = 200;
	pts.name[0] = '\0';

	GetCustTable(T("XC_tree"), ComboID, &pts, sizeof(pts));
	if ( !mode && (pts.mode == PPCTREE_OFF) ) return;	// 表示しない

	Combo.LeftAreaWidth = pts.width;
	if ( Combo.LeftAreaWidth < 20 ) Combo.LeftAreaWidth = 20;
	ComboTreeMode = mode ? mode : pts.mode;

	InitVFSTree();
	Combo.hTreeWnd = CreateWindow(Str_TreeClass, Str_TreeClass,
			WS_VISIBLE | WS_CHILD, 0, InfoBottom, Combo.LeftAreaWidth,
			Combo.Panes.box.bottom - InfoBottom - splitwide,
			Combo.hWnd, CHILDWNDID(IDW_COMMONTREE), hInst, 0);
	PPc_SetTreeFlags(Combo.hWnd, Combo.hTreeWnd);
	SendMessage(Combo.hTreeWnd, VTM_INITTREE, 0,
			(LPARAM)((pts.name[0] != '\0') ? pts.name : initpath) );
	ShowWindow(Combo.hTreeWnd, SW_SHOWNORMAL);
}

void CloseLeftArea(void)
{
	SaveTreeSettings(Combo.hTreeWnd, ComboID, PPCTREE_OFF, Combo.LeftAreaWidth);
	SendMessage(Combo.hTreeWnd, WM_CLOSE, 0, 0);
	Combo.hTreeWnd = 0;
	Combo.LeftAreaWidth = 0;
	SortComboWindows(SORTWIN_LAYOUTALL);
}

void CreateAddressBar(void)
{
	AddrHeight = Combo.Font.size.cy + 6;
	LoadAddressBitmap();
	if ( ADDRBMP_SIZE > AddrHeight ) AddrHeight = ADDRBMP_SIZE;
	InitEditColor();
	Combo.hAddressWnd = CreateWindowEx(WS_EX_CLIENTEDGE, T("EDIT"), NilStr,
			WS_CHILD | WS_VSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_LEFT,
			-10, -10, 10, 10, Combo.hWnd, CHILDWNDID(IDW_ADDRESS), hInst, 0);
												// EditBox の拡張 -------------
	SendMessage(Combo.hAddressWnd, WM_SETFONT, (WPARAM)Combo.Font.handle, 0);
	PPxRegistExEdit(NULL, Combo.hAddressWnd, CMDLINESIZE,
			NULL, PPXH_DIR_R | PPXH_COMMAND, PPXH_DIR_R,
			PPXEDIT_USEALT | PPXEDIT_WANTENTER | PPXEDIT_WANTEVENT | PPXEDIT_TABCOMP);
	ShowWindow(Combo.hAddressWnd, SW_SHOWNORMAL);
}

HWND InitCombo(PPCSTARTPARAM *psp)
{
	WNDCLASS wcClass;
	UINT ID;
	SIZE windowsize = { 640, 400 };
	HMENU hMenuBar = NULL;
	DWORD tmp[2];
	int index;

	if ( Combo.hWnd != NULL ){
		if ( Combo.hWnd != BADHWND ) return Combo.hWnd;
	}
	ComboInit = 1;
	memset(&Combo, 0, sizeof(COMBOSTRUCT));
	if ( (psp == NULL) || (psp->ComboID <= 'A') ){
		Combo.hWnd = PPxCombo(NULL);
		if ( Combo.hWnd != BADHWND ){ // 作成済み
			if ((psp != NULL) && psp->usealone) Combo.hWnd = NULL; // alone 禁止
			return Combo.hWnd;
		}
	}else{
		ComboID[2] = psp->ComboID;
		PPxRegist(BADHWND, ComboID, PPXREGIST_COMBO_IDASSIGN);
	}
	ComboThreadID = GetCurrentThreadId();

	Combo.base = PPcHeapAlloc(sizeof(COMBOITEMSTRUCT) * 2);
	Combo.show = PPcHeapAlloc(sizeof(COMBOPANES) * Combo_Max_Show);
	Combo.CloededItems = 10;
	Combo.closed = PPcHeapAlloc(sizeof(CLOSEDPPC) * Combo.CloededItems);
	for ( index = 0; index < Combo.CloededItems; index++ ){
		Combo.closed[index].ID[0] = '\0';
	}

	ComboCust();
									// ウィンドウ生成 -------------------------
	if ( NO_ERROR == GetCustTable(Str_WinPos, ComboID, &ComboWinPos, sizeof(ComboWinPos)) ){
		ComboSize.cx = windowsize.cx = ComboWinPos.pos.right - ComboWinPos.pos.left;
		ComboSize.cy = windowsize.cy = ComboWinPos.pos.bottom - ComboWinPos.pos.top;
	}
	if ( (psp != NULL) && (psp->show != SW_SHOWDEFAULT) ){
		ComboWinPos.show = (BYTE)psp->show;
	}

	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= ComboProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(Ic_PPC));
	wcClass.hCursor			= NULL; // カーソルを動的変更する
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= PPCOMBOWinClass;
	RegisterClass(&wcClass);

	InitDynamicMenu(&ComboDMenu, T("MC_menu"), ppcbar);
	if ( !(X_combos[0] & CMBS_NOMENU) ){
		hMenuBar = ComboDMenu.hMenuBarMenu;
	}
	Combo.hWnd = Combo.Panes.hWnd = CreateWindowEx(0,
		PPCOMBOWinClass, PPCOMBOWinClass,
		X_combos[0] & CMBS_NOTITLE ? WS_NOTITLEOVERLAPPED : WS_OVERLAPPEDWINDOW,
		ComboWinPos.pos.left, ComboWinPos.pos.top,
		windowsize.cx, windowsize.cy, NULL, hMenuBar, hInst, NULL);
	if ( Combo.hWnd == NULL ) return NULL;
	Combo.FontDPI = PPxCommonExtCommand(K_GETDISPDPI, (WPARAM)Combo.hWnd);
	InitSystemDynamicMenu(&ComboDMenu, Combo.hWnd);
	ComboInitFont();
	ThInit(&thGuiWork);
									// GUIパーツ生成 -------------------------
// 現在、左右の幅を拡げたりすると、ログオフする必要がある問題がある
//	setflag(X_combos[0], CMBS_TABFRAME );

	if ( X_combos[0] & CMBS_TABFRAME ){
		Combo.Panes.delta.x = 0;
		Combo.Panes.delta.y = 0;
		wcClass.lpfnWndProc		= ComboFrameProc;
		wcClass.lpszClassName	= PPCOMBOWinFrameClass;
		RegisterClass(&wcClass);

		Combo.Panes.hWnd = CreateWindowEx(0, PPCOMBOWinFrameClass, NilStr,
				WS_CHILD | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VISIBLE,
				0, 0, 300, 300, Combo.hWnd, CHILDWNDID(ComboFrameID), hInst, NULL);
	}

	ID = ComboCommandIDFirst;
	DocksInit(&comboDocks, Combo.hWnd, NULL, ComboID, Combo.Font.handle, Combo.Font.size.cy, &thGuiWork, &ID);
	if ( X_combos[0] & CMBS_COMMONREPORT )	CreateReportArea();
	if ( X_combos[0] & CMBS_TOOLBAR )		ComboCreateToolBar(Combo.hWnd);
	if ( X_combos[0] & CMBS_COMMONADDR )	CreateAddressBar();
	if ( X_combos[0] & CMBS_COMMONTREE )	CreateLeftArea(0, NilStr);
	if ( X_combos[0] & CMBS_TABALWAYS ){
		X_mpane.limit = 2;
		CreateTabBar(CREATETAB_FIRST);
	}
	tmp[0] = 0;
//	tmp[2] = 200;
	GetCustData(T("X_jlst"), tmp, sizeof(tmp));
	if ( tmp[0] >= 3 ) CreateJobArea();

	GetCustData(T("X_mpane"), &X_mpane, sizeof(X_mpane));

	ShowWindow(Combo.hWnd, ComboWinPos.show);
	if ( ComboID[2] == 'A' ){
		if ( Combo.hWnd != PPxCombo(Combo.hWnd) ){	// DLL に登録する
			PostMessage(Combo.hWnd, WM_CLOSE, 0, 0);
		}
	}else{
		PPxRegist(Combo.hWnd, ComboID, PPXREGIST_COMBO_IDASSIGN);
	}
	dd_combo_init();
	LoadWallpaper(NULL, Combo.hWnd, ComboID);
	return Combo.hWnd;
}

// *pane コマンド =============================================================
void PaneExecuteParam(int baseindex, const TCHAR *param)
{
	COPYDATASTRUCT copydata;

	SkipSpace(&param);

	copydata.dwData = 'H' + 0x100;
	copydata.cbData = TSTRSIZE32(param);
	copydata.lpData = (PVOID)param;
	SendMessage(Combo.base[baseindex].hWnd, WM_COPYDATA, 0, (LPARAM)&copydata);
}

int GetPaneBaseIndexParam(const TCHAR **paramptr, int *targetpane, int targetbase)
{
	int baseindex = -1;
	int index;
	BOOL hideindex = FALSE; // 非表示タブ
	BOOL usetabindex = FALSE;	// FALSEならshowindex;
	BOOL relative = FALSE;	// FALSEなら左端からの位置
	BOOL useactive = FALSE;
	HWND hWnd = hComboFocus;
	TCHAR paramu;
	const TCHAR *param;
	const TCHAR *p2;

	param = *paramptr;
	paramu = upper(*param);
	if ( paramu == 'H' ){
		param++;
		if ( Combo.Tabs ) hideindex = TRUE;
	}else if ( paramu == 'T' ){
		param++;
		if ( Combo.Tabs ) usetabindex = TRUE;
	}
	if ( *param == 'a' ){
		param++;
		useactive = TRUE;
	}

	paramu = upper(*param);
	if ( paramu == 'C' ){ // PPc ID ?
		TCHAR cid[REGEXTIDSIZE] = T("C_A"), cc;

		cc = *(param + 1);
		if ( cc == '_' ){
			cc = *(param + 2);
			param++;
		}
		if ( Isalpha(cc) ){
			cid[2] = upper(cc);
			if ( Islower(*(param + 2)) ){ // CZxyy
				int bindex = GetComboBaseIndex(hComboFocus);
				if ( bindex >= 0 ){
					PPC_APPINFO *cinfo = Combo.base[bindex].cinfo;
					if ( cinfo != NULL ){
						hWnd = GetPPxhWndFromID(&cinfo->info, &param, NULL);
					}
				}
			}else{
				param += 2;
				hWnd = PPxGetHWND(cid);
			}
		}
	}else if ( paramu == '~' ){
		param++;

		baseindex = GetPairPaneComboBaseIndex(hComboFocus);
		if ( baseindex < 0 ) return -1;
		hWnd = Combo.base[baseindex].hWnd;
	}else if ( paramu == 'L' ){
		param++;
		hWnd = Combo.base[Combo.show[0].baseNo].hWnd;
	}else if ( paramu == 'R' ){
		param++;
		hWnd = hComboRightFocus;
		if ( hWnd == NULL ) return -1;
	}else if ( upper(*param) == 'E' ){
		param++;
		if ( IsTrue(usetabindex) ){
			TC_ITEM tie;
			int tabindex = TabCtrl_GetItemCount(Combo.show[*targetpane].tab.hWnd);
			if ( tabindex < 0 ) return -1;
			tie.mask = TCIF_PARAM;
			if ( FALSE == TabCtrl_GetItem(Combo.show[*targetpane].tab.hWnd, tabindex - 1, &tie) ){
				return -1;
			}
			hWnd = (HWND)tie.lParam;
		}else{
			hWnd = Combo.base[Combo.show[Combo.ShowCount - 1].baseNo].hWnd;
		}
	}

	if ( (*param == '+') || (*param == '-') ) relative = TRUE;
	p2 = param;
	index = GetIntNumber(&param);
	if ( p2 == param ){ // 数値指定無し…現在窓
		if ( usetabindex ){
			baseindex = targetbase;
		}else{
			baseindex = GetComboBaseIndex(hWnd);
		}
	}else{
		if ( IsTrue(useactive) ){ // ax
			int bindex, activeid, activeid2, baseindex2 = -1, showindex = GetComboShowIndex(hWnd);
			#define ActShwCheck(bindex, showindex) ((X_combos[0] & CMBS_TABEACHITEM) ? (GetTabItemIndex(Combo.base[bindex].hWnd, showindex) >= 0) : (GetComboShowIndex(Combo.base[bindex].hWnd) < 0) )

			if ( showindex < 0 ) return -1;
			baseindex = -1;
			if ( index < 0 ){ // 一つ前のアクティブを探す
				activeid = activeid2 = Combo.Active.low - 1;
				// 最大値を求める
				for ( bindex = 0 ; bindex < Combo.BaseCount ; bindex++ ){
					int aid;

					aid = Combo.base[bindex].ActiveID;
					if ( (aid > activeid) && ActShwCheck(bindex, showindex) ){
						activeid = aid;
						baseindex2 = bindex;
					}
				}
				if ( baseindex2 < 0 ) return -1;

				// ２番目を求める
				for ( bindex = 0 ; bindex < Combo.BaseCount ; bindex++ ){
					int aid;

					aid = Combo.base[bindex].ActiveID;
					if ( (aid > activeid2) && (aid < activeid) && ActShwCheck(bindex, showindex) ){
						activeid2 = aid;
						baseindex = bindex;
					}
				}
				if ( baseindex < 0 ) return -1;

				Combo.base[baseindex2].ActiveID = --Combo.Active.low;
			}else{ // 一番古いアクティブを探す
				// 最小値を求める
				activeid = Combo.Active.high;

				for ( bindex = 0 ; bindex < Combo.BaseCount ; bindex++ ){
					int aid;

					aid = Combo.base[bindex].ActiveID;
					if ( (aid < activeid) && ActShwCheck(bindex, showindex) ){
						activeid = aid;
						baseindex = bindex;
					}
				}
				if ( baseindex < 0 ) return -1;
			}
		}else if ( IsTrue(hideindex) ){ // hx
			int count, offset = 1;

			if ( IsTrue(relative) ){
				if ( baseindex < 0 ){
					int tabindex;
					TC_ITEM tie;

					tabindex = TabCtrl_GetCurSel(Combo.show[*targetpane].tab.hWnd);
					tie.mask = TCIF_PARAM;
					TabCtrl_GetItem(Combo.show[*targetpane].tab.hWnd, tabindex, &tie);
					baseindex = GetComboBaseIndex((HWND)tie.lParam);
				}
				if ( index < 0 ){
					offset = -1;
					index = -index;
				}
			}
			if ( baseindex < 0 ) baseindex = 0;

			count = Combo.BaseCount;
			if ( index ){
				int tabcount = TabCtrl_GetItemCount(Combo.show[*targetpane].tab.hWnd);
				int tabindex = GetTabItemIndex(Combo.base[baseindex].hWnd, *targetpane);
				for ( ; ; ){
					TC_ITEM tie;

					tabindex += offset;
					if ( tabindex >= tabcount ){
						tabindex = 0;
					}else if ( tabindex < 0 ){
						tabindex = tabcount - 1;
					}

					tie.mask = TCIF_PARAM;
					TabCtrl_GetItem(Combo.show[*targetpane].tab.hWnd, tabindex, &tie);
					if ( GetComboShowIndex((HWND)tie.lParam) < 0 ){
						int bi = GetComboBaseIndex((HWND)tie.lParam);

						if ( bi >= 0 ){
							index--;
							if ( index == 0 ){
								baseindex = bi;
								break;
							}
						}
					}
					if ( !(--count) ) return -1; // ない
				}
			}
		}else if ( IsTrue(usetabindex) ){	// tx
			int tabindex;
			TC_ITEM tie;

			if ( relative == FALSE ){
				tabindex = index;
			}else{
				tabindex = GetTabItemIndex(hWnd, *targetpane);
				if ( tabindex < 0 ) return -1;
				tabindex += index;
			}

			if ( tabindex < 0 ) return -1;

			tie.mask = TCIF_PARAM;
			if ( FALSE == TabCtrl_GetItem(Combo.show[*targetpane].tab.hWnd, tabindex, &tie) ){
				return -1;
			}
			baseindex = GetComboBaseIndex((HWND)tie.lParam);
		}else{	// show pane 指定
			int showindex;

			if ( relative == FALSE ){
				showindex = index;
			}else{
				showindex = GetComboShowIndex(hWnd);
				if ( showindex < 0 ) return -1;
				showindex += index;
			}
			if ( (showindex < 0) || (showindex >= Combo.ShowCount) ) return -1;
			baseindex = Combo.show[showindex].baseNo;
			*targetpane = showindex;
		}
	}
	*paramptr = param;
	return baseindex;
}

void PaneColorCommand(int baseindex, const TCHAR *param)
{
	if ( Combo.base[baseindex].cinfo == NULL ) return;
	if ( SkipSpace(&param) != ',' ){
		Combo.base[baseindex].tabtextcolor = GetColor(&param, TRUE);
	}
	if ( SkipSpace(&param) == ',' ){
		param++;
		Combo.base[baseindex].tabbackcolor = GetColor(&param, TRUE);
	}
	SetTabColor(baseindex);
}

TCHAR PaneNextParam(const TCHAR **param)
{
	TCHAR c;

	c = SkipSpace(param);
	if ( (c != ':') && (c != ',') ) return c;
	(*param)++;
	return SkipSpace(param);
}

void PaneCloseCommand(int targetpane, int baseindex, int mode, const TCHAR *param)
{
	BOOL locked;

	locked = (PaneNextParam(&param) == 'a') ? TRUE : FALSE;
	ClosePanes(Combo.show[targetpane].tab.hWnd, baseindex, mode, locked);
}

// WmComboCommand から呼び出される
ERRORCODE PaneCommand(const TCHAR *paramptr, int targetbaseindex)
{
	TCHAR cmdname[MAX_PATH], *dst;
	const TCHAR *param;
	int baseindex, targetpane = -1;

//	CheckComboTable(T("PaneCommand-pre"));
	{
		int showc;

		for ( showc = 0 ; showc < Combo.ShowCount ; showc++ ){
			if ( Combo.show[showc].baseNo == targetbaseindex ){
				targetpane = showc;
				break;
			}
		}
	}
	if ( targetpane < 0 ){
		targetpane = GetComboShowIndex(hComboFocus);
		if ( targetpane < 0 ) targetpane = 0;
	}

	SkipSpace(&paramptr);
	dst = cmdname;
	while ( Isalpha(*paramptr) ){
		*dst++ = upper(*paramptr++);
	}
	*dst = '\0';
	PaneNextParam(&paramptr);
	param = paramptr;
	baseindex = GetPaneBaseIndexParam(&param, &targetpane, targetbaseindex);
	if ( baseindex < 0 ) return ERROR_INVALID_DATA;

	if ( !tstrcmp(cmdname, T("FOCUS")) ){
//		ChangeReason = T("CmdFocus");
		if ( GetComboShowIndex(Combo.base[baseindex].hWnd) < 0 ){
			CreateAndInitPane(baseindex);
		}
		SetFocus(Combo.base[baseindex].hWnd);
	}else if ( !tstrcmp(cmdname, T("FOCUSTAB")) ){
		if ( Combo.Tabs ){
			int showindex = GetComboShowIndex(Combo.base[baseindex].hWnd);

			if ( showindex >= 0 ) SetFocus(Combo.show[showindex].tab.hWnd);
		}
	}else if ( !tstrcmp(cmdname, T("COLOR")) ){
		PaneNextParam(&param);
		PaneColorCommand(baseindex, param);
	}else if ( !tstrcmp(cmdname, T("SELECT")) || !tstrcmp(cmdname, T("CHANGE"))){
//		ChangeReason = T("CmdSel/Chg");
		if ( PaneNextParam(&param) != '\0' ){
			int bi, tp = targetpane;

			bi = GetPaneBaseIndexParam(&param, &tp, targetbaseindex);
			if ( bi < 0 ) return ERROR_INVALID_DATA;
			targetpane = GetComboShowIndex(Combo.base[bi].hWnd);
			if ( targetpane < 0 ) return ERROR_INVALID_DATA;
		}
		SelectComboWindow(targetpane, Combo.base[baseindex].hWnd, cmdname[0] == 'S');
	}else if ( !tstrcmp(cmdname, T("HIDE")) ){
		int showindex;

//		ChangeReason = T("CmdHide");
		showindex = GetComboShowIndex(Combo.base[baseindex].hWnd);
		if ( showindex >= 0 ) HidePane(showindex);
	}else if ( !tstrcmp(cmdname, T("TABSHIFT")) || !tstrcmp(cmdname, T("SHIFT"))){
		int offset;

//		ChangeReason = T("CmdShift");
		PaneNextParam(&param);
		offset = GetIntNumber(&param);

		if ( !Combo.Tabs || !tstrcmp(cmdname, T("SHIFT")) ){ // 実体のみシフト
			int showindex, newshowindex;

			showindex = GetComboShowIndex(Combo.base[baseindex].hWnd);
			newshowindex = showindex + offset;
			if ( !offset || (showindex < 0) ||
				(newshowindex < 0) || (newshowindex >= Combo.ShowCount) ){
				return ERROR_INVALID_DATA;
			}
			if ( offset < 0 ){	// 左に移動
				memmove(&Combo.show[newshowindex + 1], &Combo.show[newshowindex],
				(BYTE *)&Combo.show[showindex] - (BYTE *)&Combo.show[newshowindex]);
			}else{ // 右に移動
				memmove(&Combo.show[showindex], &Combo.show[showindex + 1],
				(BYTE *)&Combo.show[newshowindex] - (BYTE *)&Combo.show[showindex]);
			}
			Combo.show[newshowindex].baseNo = baseindex;
//			CheckComboTable(T("PaneCommand-shift1"));
			SortComboWindows(SORTWIN_LAYOUTPAIN);
		}else{ // タブシフト
			int tabindex, newtabindex;
			TC_ITEM tie;
			TCHAR text[VFPS];

			tabindex = GetTabItemIndex(Combo.base[baseindex].hWnd, targetpane);
			newtabindex = tabindex + offset;
			if ( !offset || (tabindex < 0) ||
				(newtabindex < 0) || (newtabindex > Combo.BaseCount) ){
				return ERROR_INVALID_DATA;
			}
			tie.mask = TCIF_TEXT | TCIF_PARAM;
			tie.pszText = text;
			tie.cchTextMax = VFPS;
			TabCtrl_GetItem(Combo.show[targetpane].tab.hWnd, tabindex, &tie);
			TabCtrl_DeleteItem(Combo.show[targetpane].tab.hWnd, tabindex);
			SendMessage(Combo.show[targetpane].tab.hWnd, TCM_INSERTITEM, (WPARAM)newtabindex, (LPARAM)&tie);
			LastTabInsType = 7000;
			UseTabInsType |= B1;
//			CheckComboTable(T("PaneCommand-shift2"));
		}
	}else if ( !tstrcmp(cmdname, T("EJECT")) ){
//		ChangeReason = T("CmdEject");
		PostMessage(Combo.hWnd, WM_PPXCOMMAND, KCW_eject, (LPARAM)baseindex);
	}else if ( !tstrcmp(cmdname, T("CLOSE")) || !tstrcmp(cmdname, T("CLOSETAB")) ){
		PostMessage(Combo.base[baseindex].hWnd, WM_CLOSE, 0, 0);
	}else if ( !tstrcmp(cmdname, T("LOCK")) ){
		int mode;

		PaneNextParam(&param);
		mode = GetStringCommand(&param, T("OFF\0") T("ON\0"));
		if ( Combo.base[baseindex].cinfo != NULL ){
			if ( mode < 0 ) mode = !Combo.base[baseindex].cinfo->ChdirLock;
			Combo.base[baseindex].cinfo->ChdirLock = mode;
			SetTabInfo(-1, Combo.base[baseindex].hWnd); // ロック状態を文字で表現している間、用意
		}
	}else if ( !tstrcmp(cmdname, T("MENU")) ){
		ReplyMessage(ERROR_CANCELLED);
		TabMenu(NULL, baseindex, targetpane, NULL);
	}else if ( !tstrcmp(cmdname, T("SWAPPANE")) ){
		SwapPane(targetpane);
	}else if ( !tstrcmp(cmdname, T("SWAPTAB")) ){
		PostMessage(Combo.base[baseindex].hWnd, WM_PPXCOMMAND, K_raw | 'G', 0);
	}else if ( !tstrcmp(cmdname, T("CLOSELEFT")) ){
		PaneCloseCommand(targetpane, baseindex, -1, param);
	}else if ( !tstrcmp(cmdname, T("CLOSEPANE")) ){
		PaneCloseCommand(targetpane, baseindex, 0, param);
	}else if ( !tstrcmp(cmdname, T("CLOSERIGHT")) ){
		PaneCloseCommand(targetpane, baseindex, 1, param);
	}else if ( !tstrcmp(cmdname, T("CLOSEOTHER")) ){
		PaneCloseCommand(targetpane, baseindex, 1, param);
		PaneCloseCommand(targetpane, baseindex, -1, param);
//	}else if ( !tstrcmp(cmdname, T("TABTEXT")) ){
//	}else if ( !tstrcmp(cmdname, T("COLOR")) ){
	}else if ( !tstrcmp(cmdname, T("NEWPANE")) ){
		ReplyMessage(NO_ERROR); // ここで返事をしておかないと、CreateNewTabParam 内実行中(新しい窓登録時？)に、呼び出し元の SendMessage がエラー終了してしまうことがある
		SkipSpace(&param);
		NewPane(baseindex, param);
	}else if ( !tstrcmp(cmdname, T("NEWTAB")) ){
		ReplyMessage(NO_ERROR); // ここで返事をしておかないと、CreateNewTabParam 内実行中(新しい窓登録時？)に、呼び出し元の SendMessage がエラー終了してしまうことがある
		CreateNewTabParam(targetpane, baseindex, param);
	}else if ( !tstrcmp(cmdname, T("SHOWTABBAR")) ){
		if ( Combo.Tabs == 0 ){
			CreateTabBar(CREATETAB_APPEND);
			SortComboWindows(SORTWIN_LAYOUTPAIN);
			InvalidateRect(Combo.hWnd, NULL, TRUE);
		}
	}else if ( !tstrcmp(cmdname, T("EXECUTE")) ){
		PaneExecuteParam(baseindex, param);
	}else{
		ReplyMessage(ERROR_INVALID_DATA);
		XMessage(Combo.hWnd, NULL, XM_GrERRld, MES_EBPC, cmdname);
		return ERROR_INVALID_DATA;
	}
	return NO_ERROR;
}

// pane の状態チェック ========================================================

// 同じIDが存在していないか調べる
BOOL CheckPaneList(TCHAR *panelist)
{
	TCHAR *chkpane;
	TCHAR pane;

	for ( chkpane = panelist + 2 ; (pane = *chkpane) != '\0' ; chkpane++ ){
		if ( !Isupper(pane) || (pane == 'Z') ) continue;
		if ( tstrchr(chkpane + 1, pane) != NULL ) return FALSE;
	}
	return TRUE;
}

void SavePane_Base(TCHAR *dest, TCHAR *memo, BOOL save)
{
	TCHAR *panedst = dest;
	int showindex, baseindex;
	WINPOS WPos;
	TCHAR kbuf[16];

	WPos.show = 0;
	WPos.reserved = 0;
	// 表示分を列挙 + 窓枠の大きさ保存
	for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
		if ( cinfo != NULL ){
			if ( IsBadReadPtr( (BYTE *)cinfo + (sizeof(PPC_APPINFO) / 2), 1) ){
				Combo.base[Combo.show[showindex].baseNo].cinfo = NULL;
				Combo.base[Combo.show[showindex].baseNo].hWnd = BADHWND;
				continue;
			}else{
				panedst = tstpcpy(panedst, cinfo->RegSubCID + 1);
				if ( IsTrue(cinfo->ChdirLock) ) *panedst++ = '$';
				panedst += wsprintf(panedst, T("%d"), showindex);
			}
		}
		if ( IsTrue(save) ){
			WPos.pos = Combo.show[showindex].box;
			wsprintf(kbuf, T("%s%d"), ComboID, showindex + 1);
			SetCustTable(Str_WinPos, kbuf, &WPos, sizeof(WPos));
		}
	}
	*panedst = '\0';
	// 非表示を列挙
	for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[baseindex].cinfo;
		if ( cinfo != NULL ){
/* IDが重複することもあるときに使用するコード
			TCHAR *ptr;

			ptr = tstrchr(dest, cinfo->RegCID[1]);
			if ( ptr != NULL ){
				ptr++;
				if ( *ptr == '$' ) ptr++;
				if ( !Isdigit(*ptr) ) ptr = NULL; // 非表示
			}
			if ( ptr == NULL ){ // 表示していない
*/
			if ( IsBadReadPtr( (BYTE *)cinfo + (sizeof(PPC_APPINFO) / 2), 1) ){
				Combo.base[baseindex].cinfo = NULL;
				Combo.base[baseindex].hWnd = BADHWND;
				continue;
			}else{ // 表示していない(既に登録されていない)なら登録
				if ( cinfo->RegSubIDNo < 0 ){
					if ( tstrchr(dest, cinfo->RegCID[1]) == NULL ){
						*panedst++ = cinfo->RegCID[1];
						if ( IsTrue(cinfo->ChdirLock) ) *panedst++ = '$';
					}
				}else{
					if ( tstrstr(dest, cinfo->RegSubCID + 1) == NULL ){
						panedst = tstpcpy(panedst, cinfo->RegSubCID + 1);
						if ( IsTrue(cinfo->ChdirLock) ) *panedst++ = '$';
					}
				}
			}
		}
	}
	*panedst = '\0';
	if ( memo != NULL ){
		tstrcat(memo, T("(B)"));
		tstrcat(memo, dest);
	}
}

void SavePane_Tab(TCHAR *dest, TCHAR *memo)
{
	TCHAR *panedst = dest;
	int tabid;
	TCHAR kbuf[16];

	if ( X_combos[0] & CMBS_TABEACHITEM ) *panedst++ = '-'; // 識別子

	for ( tabid = 0 ; tabid < Combo.ShowCount ; tabid++ ){
		HWND hTabWnd = Combo.show[tabid].tab.hWnd;
		int tabcount, tabindex;

		tabcount = TabCtrl_GetItemCount(hTabWnd);
		for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
			TC_ITEM tie;
			int baseindex;

			tie.mask = TCIF_PARAM;
			if ( TabCtrl_GetItem(hTabWnd, tabindex, &tie) == FALSE ) continue;

			baseindex = GetComboBaseIndex((HWND)tie.lParam);
			if ( baseindex >= 0 ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[baseindex].cinfo;
				if ( cinfo != NULL ){
					int showindex;

					panedst = tstpcpy(panedst, cinfo->RegSubCID + 1);
					if ( IsTrue(cinfo->ChdirLock) ) *panedst++ = '$';
					showindex = GetComboShowIndex((HWND)tie.lParam);
					if ( (showindex >= 0) &&
						 ( !(X_combos[0] & CMBS_TABEACHITEM) ||
						   (showindex == tabid) ) ){
						panedst += wsprintf(panedst, T("%d"), showindex);
					}
				}
			}else{
				if ( memo != NULL ){
					wsprintf(kbuf, T("[tab%d-?]"), tabindex);
					tstrcat(memo, kbuf);
				}
				TabCtrl_DeleteItem(hTabWnd, tabindex); // 不明タブを削除
			}
		}
		if ( X_combos[0] & CMBS_TABEACHITEM ){
			if ( (tabid + 1) < Combo.ShowCount ) *panedst++ = '-';
		}else{
			break; // 左端タブの内容だけで問題ない
		}
	}
	*panedst = '\0';
	if ( memo != NULL ){
		tstrcat(memo, T("(T)"));
		tstrcat(memo, dest);
	}
}

void USEFASTCALL ComboTableCheck(void)
{
	int baseindex, showindex;

	for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[baseindex].cinfo;
		if ( cinfo == NULL ) continue;
		if ( IsBadReadPtr(cinfo->RegCID, 4) ){
			Combo.base[baseindex].cinfo = NULL;
			Combo.base[baseindex].hWnd = BADHWND;
		}
	}

	for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
		if ( Combo.show[showindex].baseNo >= Combo.BaseCount ){
			Combo.show[showindex].baseNo = 0;
		}
	}
}

void WmComboDestroy(BOOL EndSession)
{
	TCHAR *panes, *panesptr;
	int scount = max(Combo.BaseCount, Combo.ShowCount);
	int showindex, rightbaseindex;

	if ( Combo.hWnd == BADHWND ) return;

	dd_combo_close();
//	if ( ComboID[2] == 'A' ){
//		PPxCombo(hWnd); // 登録解除
//	}else{
		PPxRegist(Combo.hWnd, ComboID, PPXREGIST_COMBO_FREE);
//	}
	if ( EndSession ){
		PPxCommonExtCommand(K_REPORTSHUTDOWN, 0);
	}else{
		PPxCommonExtCommand(K_SETLOGWINDOW, 0);
	}

	Combo.Report.hWnd = NULL;

	Combo.hWnd = BADHWND; // 再帰を禁止

	SetCustTable(Str_WinPos, ComboID, &ComboWinPos, sizeof(ComboWinPos));

	if ( !EndSession ) ComboTableCheck(); // 破損チェック

	// 終了時のペイン構成を保存する
	panes = PPcHeapAlloc(Panelistsize(scount));

	panesptr = panes;
	*panes = '?'; // Focus
	showindex = GetComboShowIndexDefault(hComboFocus);
	if ( showindex >= 0 ){ // Focus を記憶する
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
		if ( cinfo != NULL ){
			tstrcpy(panesptr, cinfo->RegSubCID + 1);
			panesptr += tstrlen(panesptr) - 1;
		}
	}

	*(++panesptr) = '?'; // 右
	rightbaseindex = GetComboBaseIndex(hComboRightFocus);
	if ( rightbaseindex >= 0 ){ // 右 を記憶する
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[rightbaseindex].cinfo;
		if ( cinfo != NULL ){
			tstrcpy(panesptr, cinfo->RegSubCID + 1);
			panesptr += tstrlen(panesptr) - 1;
		}
	}

	*(++panesptr) = '\0';
	if ( Combo.Tabs == 0 ){	// タブ無し
		SavePane_Base(panesptr, NULL, TRUE);
	}else{	// タブ有り
		SavePane_Base(panesptr, NULL, TRUE);
		SavePane_Tab(panesptr, NULL);
	}
	if ( ComboInit <= 0 ){
		SetCustStringTable(T("_Path"), ComboID, panes, MAX_PATH / 2);
	}
	PPcHeapFree(panes);

	if ( Combo.LeftAreaWidth > 10 ){
		SaveTreeSettings(Combo.hTreeWnd, ComboID, ComboTreeMode, Combo.LeftAreaWidth);
	}
	if ( Combo.BottomAreaHeight > 4 ){
		TCHAR buf[8];

		wsprintf(buf, T("%sL"), ComboID);
		SaveTreeSettings(NULL, buf, 0, Combo.BottomAreaHeight);
		if ( Combo.Joblist.hWnd != NULL ){
			wsprintf(buf, T("%sJ"), ComboID);
			SaveTreeSettings(NULL, buf, 0, Combo.Joblist.JobAreaWidth);
		}
	}
	{					// 一つづつ確実に終了させていく
		HWND hPaneWnd;
		int i;

		for ( i = 0 ; i < Combo.BaseCount ; i++ ){
			hPaneWnd = Combo.base[i].hWnd;
			if ( (hPaneWnd == NULL) ||
				 (((LONG_PTR)hPaneWnd & (LONG_PTR)HWND_BROADCAST) == (LONG_PTR)HWND_BROADCAST) ){
				continue;
			}
			if ( EndSession ){
				SendMessage(hPaneWnd, WM_ENDSESSION, TRUE, 0);
			}else{
			#ifndef UNICODE
				if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
					int cnt;
					// これだと、終了時にUSER.EXEで落ちない on Win9x
					// その代わり終了が遅くなる
					PostMessage(hPaneWnd, WM_CLOSE, 0, 0);
					cnt = 10;
					while ( cnt-- ){
						Sleep(30);
						if ( IsWindow(hPaneWnd) == FALSE ) break;
						PeekLoop();
					}
				}else
			#endif
				{
					SendMessage(hPaneWnd, WM_PPXCOMMAND, KC_PRECLOSE, 0);
				}
			}
		}
	}
	DeleteObject(Combo.Font.handle);
	if ( Combo.cfs.hFont != NULL ) DeleteObject(Combo.cfs.hFont);
	if ( X_combos[0] & CMBS_NOMENU ) DestroyMenu(ComboDMenu.hMenuBarMenu);
	if ( Combo.closed != NULL ){
		PPcHeapFree(Combo.closed);
		Combo.closed = NULL;
	}

	PostWindowClosed();
// 現在の所、終了時の異常終了を避けるため、意図的にリーク。
//	PPcHeapFree(Combo.show);
//	PPcHeapFree(Combo.base);
}

#pragma argsused
void ResetR(TCHAR *buf)
{
	int index;
	UnUsedParam(buf);

	if ( Combo.BaseCount <= 1 ){ // 窓が１つのみ
		hComboRightFocus = NULL;
	}else if ( Combo.ShowCount == 1 ){ // ペインが１つのみ
		hComboRightFocus = Combo.base[0].hWnd;
		if ( GetComboShowIndex(hComboRightFocus) >= 0 ){
			hComboRightFocus = Combo.base[1].hWnd;
		}
	}else{ // ペインが複数
		hComboRightFocus = Combo.base[Combo.show[1].baseNo].hWnd;
		#if 0
		if ( GetComboShowIndex(hComboRightFocus) < 0 ){
			if ( X_combos[0] & CMBS_TABEACHITEM ){
				TC_ITEM tie;

				tie.mask = TCIF_PARAM;
				if ( TabCtrl_GetItem(Combo.show[1].tab.hWnd, 0, &tie) ){
					hComboRightFocus = (HWND)tie.lParam;
					Combo.show[1].baseNo = GetComboBaseIndex(hComboRightFocus);
					ShowWindow(hComboRightFocus, SW_SHOWNORMAL);
				}
			}else{
				int i;

				for ( i = 0 ; i < Combo.BaseCount ; i++ ){
					if ( GetComboShowIndex(Combo.base[i].hWnd) < 0 ){
						hComboRightFocus = Combo.base[i].hWnd;
						Combo.show[1].baseNo = i;
						ShowWindow(hComboRightFocus, SW_SHOWNORMAL);
					}
				}
			}
		}
		#endif
	}
	ChangeReason = T("ResetR");

	#if VersionP
	if ( buf != NULL ){
		PPxCommonExtCommand(K_SENDREPORT, (WPARAM)((*buf == '@') ? buf + 1 : buf));
	}
	#endif

	for ( index = 0 ; index < Combo.Tabs ; index++ ){
		InvalidateRect(Combo.show[index].tab.hWnd, NULL, TRUE);
	}
}

int oldpaneerr = 0;

TCHAR * nestedinfo(void)
{
	TCHAR *nested = PPcHeapAlloc(512), *dest;
	int i;

	dest = nested + wsprintf(nested, T("[%d:"), ComboProcNested);
	for ( i = 0 ; (i < ComboProcNested) && (i < NestedMsgs) ; i++ ){
		if ( LOWORD(ComboProcMsg[i]) == (WORD)WM_PPXCOMMAND ){
			switch ( HIWORD(ComboProcMsg[i]) ){
				case KCW_entry:
					dest += wsprintf(dest, T("entry:"));
					break;
				case KCW_ready:
					dest += wsprintf(dest, T("ready:"));
					break;
				default:
					dest += wsprintf(dest, T("PPx%X:"), HIWORD(ComboProcMsg[i]));
			}
		}else{
			switch ( LOWORD(ComboProcMsg[i]) ){
				case WM_SETFOCUS:
					dest += wsprintf(dest, T("SETFOCUS%x:"), HIWORD(ComboProcMsg[i]));
					break;
				case WM_ACTIVATE:
					dest += wsprintf(dest, T("ACTIVATE%x:"), HIWORD(ComboProcMsg[i]));
					break;
				default:
					dest += wsprintf(dest, T("%X.%X:"), LOWORD(ComboProcMsg[i]), HIWORD(ComboProcMsg[i]));
			}
		}
	}
	*(dest - 1) = ']';
	*dest = '\0';
	return nested;
}

void CheckComboTable_NowPaneError(const TCHAR *fmt, const TCHAR *p1)
{
	TCHAR *nested;

	nested = nestedinfo();
	XMessage(NULL, NULL, XM_DbgLOG, fmt, p1, hComboFocus, nested);
	PPcHeapFree(nested);
}

void CheckComboTable_PairPaneError(const TCHAR *fmt, const TCHAR *p1)
{
	TCHAR *buf, *nested;

	buf = PPcHeapAlloc(Panelistsize(max(Combo.BaseCount, Combo.ShowCount)) * 3 + 512);
	nested = nestedinfo();

	wsprintf(buf, fmt, p1, ChangeReason, nested,
			Combo.ShowCount, Combo.BaseCount, X_combos[0], Combo.Tabs);

	ResetR(buf);
	PPcHeapFree(nested);
	PPcHeapFree(buf);
}

void CheckComboTable(const TCHAR *p1)
{
	TCHAR *panes, *memo;
	int scount = max(Combo.BaseCount, Combo.ShowCount);
	int paneerr = 0;

	ComboTableCheck(); // 破損チェック

	// 並びチェック
	memo = PPcHeapAlloc(Panelistsize(scount) * 3);
	panes = PPcHeapAlloc(Panelistsize(scount));
	memo[0] = '\0';
	panes[0] = ' ';
	panes[1] = ' ';
	if ( Combo.Tabs == 0 ){	// タブ無し
		SavePane_Base(panes + 2, memo, FALSE);
		if ( CheckPaneList(panes) == FALSE ) paneerr += 1;
	}else{	// タブ有り
		SavePane_Base(panes + 2, memo, FALSE);
		if ( CheckPaneList(panes) == FALSE ) paneerr += 10;
		SavePane_Tab(panes + 2, memo);
		if ( CheckPaneList(panes) == FALSE ) paneerr += 100;
	}
	PPcHeapFree(panes);

	if ( paneerr ){
		if ( paneerr != oldpaneerr ){
			#if VersionP
			TCHAR *buf, *nested;

			buf = PPcHeapAlloc(Panelistsize(scount) * 3);
			nested = nestedinfo();
			oldpaneerr = paneerr;
			wsprintf(buf, T("並び順破壊%s.%d(%x.%d.%d.%d)%s%s-%s"), p1,
				paneerr + LastTabInsType + (UseTabInsType * 10000),
				X_combos[0], Combo.Tabs, Combo.ShowCount, Combo.BaseCount,
				nested, ChangeReason, memo);
			PPxCommonExtCommand(K_SENDREPORT, (WPARAM)buf);
			PPcHeapFree(nested);
			PPcHeapFree(buf);
			#endif
		}
	}
	PPcHeapFree(memo);

	if ( (p1 != NULL) && (*p1 != '@') && !ComboInit && (*ChangeReason != '@') ){
		// 現在窓チェック
		if ( Combo.BaseCount > 1 ){
			if ( GetComboBaseIndex(hComboFocus) < 0 ){
				CheckComboTable_NowPaneError(T("現在窓base異常%s,%x%s"), p1);
				hComboFocus = Combo.base[0].hWnd;
			}else if ( GetComboShowIndex(hComboFocus) < 0 ){
				CheckComboTable_NowPaneError(T("現在窓show異常%s,%x%s"), p1);
			}
		}

		// 反対窓チェック
		if ( Combo.BaseCount > 1 ){
			if ( hComboRightFocus == NULL ){
				ResetR(NULL);
//				CheckComboTable_PairPaneError(T("反対窓未設定%s,%s%s%d/%d,%x,%d"), p1);
			}else{
				if ( GetComboBaseIndex(hComboRightFocus) >= 0 ){
					int showindex = GetComboShowIndex(hComboRightFocus);

					if ( Combo.ShowCount <= 1 ){
						if ( showindex >= 0 ){
							CheckComboTable_PairPaneError(T("反対窓表示%s,%s%s%d/%d,%x,%d"), p1);
						}
					}else{
						if ( showindex <= 0 ){
							CheckComboTable_PairPaneError(
								(showindex < 0) ?
									T("反対窓異常%s,%s%s%d/%d,%x,%d") :
									T("反対窓左%s,%s%s%d/%d,%x,%d"), p1);
						}
					}
				}
			}
		}
	}
}

int GetComboRegZID(void)
{
	PPC_APPINFO *cinfo;
	int baseindex;
	int regid;
	BOOL retry;

	regid = (RegSubIDcounter + 1) & 0x1ff;
	for (;;){
		retry = FALSE;
		for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
			cinfo = Combo.base[baseindex].cinfo;
			if ( cinfo == NULL ) continue;
			if ( regid == cinfo->RegSubIDNo ){
				regid = (regid + 1) & 0x1ff;
				retry = TRUE;
				break;
			}
		}
		if ( retry == FALSE ) break;
	}
	RegSubIDcounter = regid;
	return regid;
}
