/*-----------------------------------------------------------------------------
	Paper Plane cUI			Combo Window TabBar / pane control
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define SIZEBAND 5 // 幅調整の認識幅

TCHAR tiptext[VFPS];

const TCHAR THSPROP[] = T("PcmbTabH");
typedef struct {
	HWND hWnd;		// 自分自身の hWnd
	MOUSESTATE ms;
	WNDPROC hOldProc;	// 親のウィンドウプロージャ
	struct {
#define THS_SHOW_NODRAG -2
#define THS_SHOW_CANCELDRAG -1
		int showindex;
		int tabindex, width;
	} DDinfo;
} TABHOOKSTRUCT;

HWND DeletePane(int showindex);

int PPxDownMouseButtonX(MOUSESTATE *ms, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	DWORD button;

	button = wParam & MOUSEBUTTONMASK;

	// 2つ以上ボタンを押しているか？
	if ( !((button > 0) && ((button & (button - 1)) == 0)) ){ //bitが一つのみ?
		PPxCancelMouseButton(ms);
		return MOUSEBUTTON_CANCEL;
	}else{	// 押しているボタンは１つのみ→新規
		int x = GetSystemMetrics(SM_CXDRAG);
		int y = GetSystemMetrics(SM_CYDRAG);

		ms->mode = MOUSEMODE_PUSH;
		if ( GetCapture() == NULL ) SetCapture(hWnd);
		PPxGetMouseButtonDownInfo(ms, hWnd, button, lParam);
		ms->DDRect.left   = ms->PushScreenPoint.x - x;
		ms->DDRect.right  = ms->PushScreenPoint.x + x;
		ms->DDRect.top    = ms->PushScreenPoint.y - y;
		ms->DDRect.bottom = ms->PushScreenPoint.y + y;
		ms->MovedClientPoint = ms->PushClientPoint;
		ms->MovedScreenPoint = ms->PushScreenPoint;
	}
	ms->PushTick = GetTickCount();
	return ms->PushButton;
}

BOOL TabMouseCommand(HWND hWnd, POINT *pos, const TCHAR *click, int index)
{
	TCHAR buf[CMDLINESIZE], *p;

	p = PutShiftCode(buf, GetShiftKey());
	wsprintf(p, T("%s_%s"), click, (index >= 0) ? T("TABB") : T("TABS") );
	if ( NO_ERROR == GetCustTable(T("MC_click"), buf, buf, sizeof(buf)) ){
		TC_ITEM tie;

		if ( index < 0 ) index = TabCtrl_GetCurSel(hWnd);
		tie.mask = TCIF_PARAM;
		if ( IsTrue(TabCtrl_GetItem(hWnd, index, &tie)) ){
			SendMessage((HWND)tie.lParam, WM_PPCEXEC,
					(WPARAM)buf, TMAKELPARAM(pos->x, pos->y));
			return TRUE;
		}
	}
	return FALSE;
}

void CreateNewTab(int showindex)
{
	HWND hWnd;
	TCHAR buf[20];

	if ( showindex < 0 ) showindex = 0;

	hWnd = Combo.base[Combo.show[showindex].baseNo].hWnd;
	SetFocus(hWnd);

	if ( X_combos[0] & CMBS_TABEACHITEM ){
		wsprintf(buf, T("-pane:%d"), showindex);
		CallPPcParam(Combo.hWnd, buf);
	}else{
		PostMessage(hWnd, WM_PPXCOMMAND, K_raw | K_F11, 0);
	}
}

void CreateNewTabParam(int showindex, int newbaseindex, const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE];

	int newshowindex;

	for ( newshowindex = 0 ; newshowindex < Combo.ShowCount ; newshowindex++ ){
		if ( Combo.show[newshowindex].baseNo == newbaseindex ){
			showindex = newshowindex;
			break;
		}
	}

	if ( SkipSpace(&param) == '\0' ){
		CreateNewTab(showindex);
		return;
	}
	if ( showindex < 0 ) showindex = 0;
	wsprintf(buf, T("-pane:%d %s"), showindex, param);

	CallPPcParam(Combo.hWnd, buf);
}

void TabDblClickMouse(HWND hWnd, MOUSESTATE *ms)
{
	TC_HITTESTINFO th;
	TCHAR click[3];
	int index;

	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ) return;
	th.pt = ms->PushClientPoint;
	index = TabCtrl_HitTest(hWnd, &th);

	click[0] = PPxMouseButtonChar[ms->PushButton];
	click[1] = 'D';
	click[2] = '\0';
	TabMouseCommand(hWnd, &ms->PushClientPoint, click, index);
}

int GetTabShowIndex(HWND hWnd)
{
	int tabpane;

	if ( Combo.Tabs <= 1 ){
		return GetComboShowIndex(hComboFocus);
	}

	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( Combo.show[tabpane].tab.hWnd == hWnd ) return tabpane;
	}
	return -1;
}

int GetTabFromPos(POINT *spos, POINT *cpos, int *TargetTab)
{
	HWND hTargetWnd = WindowFromPoint(*spos);
	int showindex = -1, tabpane;
	TC_HITTESTINFO th;

	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( Combo.show[tabpane].tab.hWnd == hTargetWnd ){
			showindex = tabpane;
			break;
		}
	}
	*TargetTab = showindex;
	if ( showindex < 0 ) return -1;
	th.pt = *spos;
	ScreenToClient(hTargetWnd, &th.pt);
	if ( cpos != NULL ) *cpos = th.pt;
	return TabCtrl_HitTest(hTargetWnd, &th);
}

// タブの DnD 完了処理
void TabUp_DnD_Mouse(HWND hWnd, TABHOOKSTRUCT *THS)
{
	int targetshow, targettab;
	int reqsort = 0;
	TC_HITTESTINFO th;

	GetMessagePosPoint(th.pt);
	if ( THS->DDinfo.width >= 0 ){ // 幅変更
		int width;

		SetWindowLongPtr(hWnd, GWL_STYLE,
				GetWindowLongPtr(hWnd, GWL_STYLE) | TCS_FIXEDWIDTH);
		setflag(X_combos[0], CMBS_TABFIXEDWIDTH);
		ScreenToClient(hWnd, &th.pt);
		width = th.pt.x - THS->DDinfo.width;
		if ( width < 16 ) width = 16;
		SendMessage(hWnd, TCM_SETITEMSIZE, 0, TMAKELPARAM(width, 0));
		InvalidateRect(hWnd, NULL, TRUE);
		if ( X_combos[0] & CMBS_TABMULTILINE ){
			SortComboWindows(SORTWIN_LAYOUTPAIN);
		}
		return;
	}

	targettab = GetTabFromPos(&th.pt, NULL, &targetshow);
	if ( (THS->DDinfo.showindex >= 0) && (targetshow >= 0) ){ // tab入替
		TC_ITEM tie;
		TCHAR cap[VFPS + 16];
		HWND hTargetTabWnd = Combo.show[targetshow].tab.hWnd;
		int srcselect;

		if ( (((X_combos[0] & (CMBS_TABSEPARATE | CMBS_TABEACHITEM)) == CMBS_TABSEPARATE) && (hTargetTabWnd != hWnd)) ||
			((hTargetTabWnd == hWnd) && (targettab == THS->DDinfo.tabindex)) ){
			return;
		}

		if ( targettab < 0 ) targettab = TabCtrl_GetItemCount(hWnd);
		if ( (hTargetTabWnd == hWnd) && (targettab == (THS->DDinfo.tabindex + 1))){
			targettab++;
		}
		srcselect = TabCtrl_GetCurSel(hWnd);

		tie.mask = TCIF_TEXT | TCIF_PARAM;
		tie.cchTextMax = VFPS + 16;
		tie.pszText = cap;
		if ( IsTrue(TabCtrl_GetItem(hWnd, THS->DDinfo.tabindex, &tie)) ){
			int srcpane = GetTabShowIndex(hWnd);

			SendMessage(hTargetTabWnd, TCM_INSERTITEM, (WPARAM)targettab, (LPARAM)&tie);
			LastTabInsType = 5000;
			UseTabInsType |= B0;
			if ( THS->DDinfo.tabindex == srcselect ){
				if ( hTargetTabWnd == hWnd ){ // ペイン内タブ移動
					TabCtrl_SetCurSel(hWnd, targettab);
				}else{ // ペイン間タブ移動
					int c = TabCtrl_GetItemCount(hWnd);
					HWND hTargetWnd = (HWND)tie.lParam;

					if ( (srcselect + 1) >= c ){
						srcselect--;
					}
					// ShowWindow の前に削除しないと、窓の整合性チェックに
					// 引っ掛かる
					TabCtrl_DeleteItem(hWnd, THS->DDinfo.tabindex);
					ShowWindow(hTargetWnd, SW_HIDE);

					if ( srcselect >= 0 ){
						if ( TabCtrl_GetItem(hWnd, srcselect, &tie) == FALSE ){
							srcselect = c - 2;
							TabCtrl_GetItem(hWnd, srcselect, &tie);
						}
						if ( hTargetWnd == hComboRightFocus ){
							hComboRightFocus = (HWND)tie.lParam;
						}
						if ( hTargetWnd == hComboFocus ){
							hComboFocus = (HWND)tie.lParam;
						}
						Combo.show[srcpane].baseNo = GetComboBaseIndex((HWND)tie.lParam);
						TabCtrl_SetCurSel(hWnd, srcselect);
						ShowWindow((HWND)tie.lParam, SW_SHOWNORMAL);
						reqsort = 1;
						ChangeReason = T("TabDnD");
					}else{
						reqsort = -1;
					}
				}
			}
			if ( (hTargetTabWnd == hWnd) && (targettab <= THS->DDinfo.tabindex) ){
				THS->DDinfo.tabindex++;
			}
			InvalidateRect(hWnd, NULL, TRUE);
			if ( reqsort == 0 ){
				TabCtrl_DeleteItem(hWnd, THS->DDinfo.tabindex);
			}else{
				if ( reqsort < 0 ) DeletePane(srcpane);
				SortComboWindows(SORTWIN_LAYOUTPAIN);
			}
		}
	}
}

void TabUpMouse(HWND hWnd, TABHOOKSTRUCT *THS, LPARAM lParam, int button)
{
	TC_HITTESTINFO th;
	TCHAR click[2];
	int index;

	if ( button <= MOUSEBUTTON_CANCEL ) return;

	LPARAMtoPOINT(th.pt, lParam);

	if ( THS->DDinfo.showindex != THS_SHOW_NODRAG ){ // タブ D&D
		TabUp_DnD_Mouse(hWnd, THS);
		return;
	}

	index = TabCtrl_HitTest(hWnd, &th);

	click[0] = PPxMouseButtonChar[button];
	click[1] = '\0';
	if ( IsTrue(TabMouseCommand(hWnd, &th.pt, click, index)) ) return;

	if ( button == MOUSEBUTTON_M ){
		if ( index >= 0 ){
			TC_ITEM tie;

			tie.mask = TCIF_PARAM;
			if ( IsTrue(TabCtrl_GetItem(hWnd, index, &tie)) ){
				PostMessage((HWND)tie.lParam, WM_CLOSE, 0, 0);
			}
		}else{
			CreateNewTab(GetTabShowIndex(hWnd));
		}
		return;
	}
	if ( button == MOUSEBUTTON_R ){
		POINT pos;
		TC_ITEM tie;
		int baseindex;

		GetMessagePosPoint(pos);
		// Tabが指すIndexを求める
		tie.mask = TCIF_PARAM;
		if ( IsTrue(TabCtrl_GetItem(hWnd, index, &tie)) ){
			baseindex = GetComboBaseIndex((HWND)tie.lParam);
		}else{
			baseindex = -1;	// 空白を右クリック
		}
		TabMenu(hWnd, baseindex, GetTabShowIndex(hWnd), &pos);
		return;
	}

	if ( button == MOUSEBUTTON_L ){
		TC_ITEM tie;
		int baseindex, showindex;

		// Tabが指すIndexを求める
		tie.mask = TCIF_PARAM;
		if ( IsTrue(TabCtrl_GetItem(hWnd, index, &tie)) ){
			baseindex = GetComboBaseIndex((HWND)tie.lParam);
			if ( baseindex < 0 ){ // 不明タブを削除
				TabCtrl_DeleteItem(hWnd, index);
				return;
			}

			if ( !(X_combos[1] & CMBS1_NOTABBUTTON) ){
				RECT box;

				SendMessage(hWnd, TCM_GETITEMRECT, index, (LPARAM)&box);
				if ( th.pt.x >= (box.right - (box.bottom - (box.top + 4)) - 2)){
					PostMessage(Combo.base[baseindex].hWnd, WM_CLOSE, 0, 0);
					return;
				}
			}
		}
		showindex = GetTabShowIndex(hWnd);
		if ( showindex >= 0 ){
			SetFocus(Combo.base[Combo.show[showindex].baseNo].hWnd);
		}
		return;
	}
}

BOOL TabKeyDown(HWND hWnd, int key)
{
	switch (key){
		case VK_ESCAPE: {
			int showindex;

			showindex = GetTabShowIndex(hWnd);
			if ( !(X_combos[0] & CMBS_TABBUTTON) ){
				int tabi = GetTabItemIndex(Combo.base[Combo.show[showindex].baseNo].hWnd, showindex);
				SendMessage(hWnd, TCM_SETCURSEL, (WPARAM)tabi, 0);
			}

			if ( showindex >= 0 ){
				SetFocus(Combo.base[Combo.show[showindex].baseNo].hWnd);
			}
			return TRUE;
		}
		case VK_F10:
		case VK_APPS: {
			TC_ITEM tie;
			int cursor;
			int baseindex;
			int showindex;

			cursor = (int)SendMessage(hWnd, TCM_GETCURFOCUS, 0, 0);
			if ( cursor >= 0 ){
				RECT box;
				POINT pos;

				SendMessage(hWnd, TCM_GETITEMRECT, cursor, (LPARAM)&box);
				pos.x = box.left;
				pos.y = box.bottom;
				ClientToScreen(hWnd, &pos);
				// Tabが指すIndexを求める
				tie.mask = TCIF_PARAM;
				TabCtrl_GetItem(hWnd, cursor, &tie);
				baseindex = GetComboBaseIndex((HWND)tie.lParam);
				showindex = GetTabShowIndex(hWnd);
				if ( TabMenu(hWnd, baseindex, showindex, &pos) ){
					if ( showindex >= 0 ){
						SetFocus(Combo.base[Combo.show[showindex].baseNo].hWnd);
					}
				}
			}
			return TRUE;
		}

		default:
			if ( !(X_combos[0] & CMBS_TABBUTTON) ) switch (key){
				case VK_SPACE:
				case VK_RETURN: {
					TC_ITEM tie;
					int cursor;

					cursor = (int)SendMessage(hWnd, TCM_GETCURFOCUS, 0, 0);
					if ( cursor >= 0 ){
						tie.mask = TCIF_PARAM;
						TabCtrl_GetItem(hWnd, cursor, &tie);
						SetFocus((HWND)tie.lParam);
					}
					return TRUE;
				}

				case VK_UP:
				case VK_DOWN:
					return TRUE; // 無効に

				case VK_LEFT: {
					int cursor;
					cursor = (int)SendMessage(hWnd, TCM_GETCURFOCUS, 0, 0);
					if ( cursor >= 1 ){
						SendMessage(hWnd, TCM_SETCURSEL, (WPARAM)(cursor -1 ), 0);
					}
					return TRUE;
				}

				case VK_RIGHT: {
					int cursor;
					cursor = (int)SendMessage(hWnd, TCM_GETCURFOCUS, 0, 0);
					if ( cursor >= 0 ){
						SendMessage(hWnd, TCM_SETCURSEL, (WPARAM)(cursor +1 ), 0);
					}
					return TRUE;
				}
			}
	}
	return FALSE;
}

void SetTabSizeCursor(HWND hWnd)
{
	TC_HITTESTINFO th;
	RECT box;
	int index;

//	GetMessagePosPoint(th.pt);
	GetCursorPos(&th.pt);
	ScreenToClient(hWnd, &th.pt);
	index = TabCtrl_HitTest(hWnd, &th);
	if ( index >= 0 ){
		TabCtrl_GetItemRect(hWnd, index, &box);
		if ( (box.right - SIZEBAND) < th.pt.x ){
			SetCursor( LoadCursor(NULL, IDC_SIZEWE) );
		}
	}
}

void TabMoveMouse(HWND hWnd, TABHOOKSTRUCT *THS)
{
	int tabindex;
	int showindex;
	RECT box;

//	if ( THS->ms.PushButton <= MOUSEBUTTON_CANCEL ) return; // キャンセル状態

	if ( THS->ms.PushButton == MOUSEBUTTON_W ){
		THS->DDinfo.showindex = THS_SHOW_CANCELDRAG;
		SetCursor( LoadCursor(NULL, IDC_ARROW) );
	}

	if ( THS->ms.mode == MOUSEMODE_NONE ) SetTabSizeCursor(hWnd);

	if ( THS->ms.mode != MOUSEMODE_DRAG ) return;

	if ( THS->DDinfo.showindex == THS_SHOW_NODRAG ){ // D&D 開始
		POINT cpos;

		THS->DDinfo.width = -1;
		THS->DDinfo.tabindex = GetTabFromPos(&THS->ms.PushScreenPoint, &cpos, &THS->DDinfo.showindex);
		if ( THS->DDinfo.tabindex < 0 ){
			THS->DDinfo.showindex = THS_SHOW_CANCELDRAG;
		}else{
			TabCtrl_GetItemRect(Combo.show[THS->DDinfo.showindex].tab.hWnd, THS->DDinfo.tabindex, &box);
			if ( (box.right - SIZEBAND) < cpos.x ){
				THS->DDinfo.width = box.left;
			}
		}
	}
	if ( THS->DDinfo.showindex < 0 ) return;
	tabindex = GetTabFromPos(&THS->ms.MovedScreenPoint, NULL, &showindex);
	if ( (((X_combos[0] & (CMBS_TABSEPARATE | CMBS_TABEACHITEM)) == CMBS_TABSEPARATE) && (showindex != THS->DDinfo.showindex)) ||

	((showindex == THS->DDinfo.showindex) && (tabindex == THS->DDinfo.tabindex)) ){
		showindex = -1;
	}
	SetCursor(LoadCursor(NULL, (THS->DDinfo.width >= 0 ) ? IDC_SIZEWE : (showindex < 0 ? IDC_NO : IDC_UPARROW) ) );
}

LRESULT CALLBACK TabHookProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	TABHOOKSTRUCT *THS;

	THS = (TABHOOKSTRUCT *)GetProp(hWnd, THSPROP);
	if ( THS == NULL ) return DefWindowProc(hWnd, iMsg, wParam, lParam);
	switch(iMsg){
		case WM_DESTROY: {
			WNDPROC hOldProc;

			hOldProc = THS->hOldProc;
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)hOldProc);
			RemoveProp(hWnd, THSPROP);
			HeapFree(hProcessHeap, 0, THS);
			return CallWindowProc(hOldProc, hWnd, iMsg, wParam, lParam);
		}
		case WM_NCHITTEST:
			return HTCLIENT;	// 非タブ領域上のマウス操作も取得可能にする

							// マウスのクライアント領域押下 ------
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
			if ( THS->DDinfo.showindex == THS_SHOW_NODRAG ){
				CallWindowProc(THS->hOldProc, hWnd, iMsg, wParam, lParam);
			}
			PPxDownMouseButtonX(&THS->ms, hWnd, wParam, lParam);
			SetTabSizeCursor(hWnd);
			return 0;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP:
			if ( THS->DDinfo.showindex == THS_SHOW_NODRAG ){
				 CallWindowProc(THS->hOldProc, hWnd, iMsg, wParam, lParam);
			}
			TabUpMouse(hWnd, THS, lParam, PPxUpMouseButton(&THS->ms, wParam));
			THS->DDinfo.showindex = THS_SHOW_NODRAG;
			return 0;

		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			PPxDoubleClickMouseButton(&THS->ms, hWnd, wParam, lParam);
			TabDblClickMouse(hWnd, &THS->ms);
			break;

		case WM_MOUSEMOVE:
			PPxMoveMouse(&THS->ms, hWnd, lParam);
			TabMoveMouse(hWnd, THS);
			break;

		case WM_MOUSEWHEEL: {
			int i;

			i = PPxWheelMouse(&THS->ms, hWnd, wParam, lParam);
			if ( i != 0 ){
				int key = i > 0 ? VK_LEFT : VK_RIGHT;
				PostMessage(hWnd, WM_KEYDOWN, key, 0);
				PostMessage(hWnd, WM_KEYUP, key, 0);
				PostMessage(hWnd, WM_KEYDOWN, VK_SPACE, 0);
				PostMessage(hWnd, WM_KEYUP, VK_SPACE, 0);
			}
			return 1;
		}

		case WM_KEYDOWN:
			if ( TabKeyDown(hWnd, (int)wParam) ) return 0;
			break;

	}
	return CallWindowProc(THS->hOldProc, hWnd, iMsg, wParam, lParam);
}

void NewTabBar(void)
{
	TABHOOKSTRUCT *THS;
	int tabpane;
	int item;
	HWND hTabWnd;
	DWORD style = WS_CHILD | WS_VISIBLE | TCS_TOOLTIPS |
			TCS_HOTTRACK | TCS_FOCUSNEVER | CCS_NODIVIDER;

	THS = HeapAlloc(hProcessHeap, 0, sizeof(TABHOOKSTRUCT));
	if ( THS == NULL ) return;

	tabpane = Combo.Tabs;
	if ( X_combos[0] & CMBS_TABFIXEDWIDTH ) setflag(style, TCS_FIXEDWIDTH);
	if ( X_combos[0] & CMBS_TABMULTILINE )  setflag(style, TCS_MULTILINE);
	if ( X_combos[0] & CMBS_TABBUTTON ){ // タブの行位置が変化しないようにする時の設定。但し、見た目が変わる
		setflag(style, TCS_BUTTONS | TCS_FLATBUTTONS);
	}
	if ( (X_combos[0] & CMBS_TABCOLOR) || !(X_combos[1] & CMBS1_NOTABBUTTON) ){
		setflag(style, TCS_OWNERDRAWFIXED);
	}
	hTabWnd = CreateWindowEx(0, WC_TABCONTROL, NilStr, style, -10, -10,
			10, 10, Combo.hWnd /* Combo.Panes.hWnd */,
			CHILDWNDID(IDW_TABCONTROL), hInst, NULL);
	if ( hTabWnd == NULL ) return;
	Combo.show[tabpane].tab.hWnd = hTabWnd;
	// ↓TCS_EX_REGISTERDROP は親でDrop処理するため不要
	SendMessage(hTabWnd, TCM_SETEXTENDEDSTYLE, 0, TCS_EX_FLATSEPARATORS);
	SendMessage(hTabWnd, WM_SETFONT, (WPARAM)GetControlFont(Combo.FontDPI, &Combo.cfs), TMAKELPARAM(TRUE, 0));

	Combo.show[tabpane].tab.hTipWnd = TabCtrl_GetToolTips(hTabWnd);

	THS->hWnd = hTabWnd;
	THS->DDinfo.showindex = THS_SHOW_NODRAG;
	PPxInitMouseButton(&THS->ms);
	SetProp(THS->hWnd, THSPROP, (HANDLE)THS);
	THS->hOldProc = (WNDPROC)
			SetWindowLongPtr(THS->hWnd, GWLP_WNDPROC, (LONG_PTR)TabHookProc);

	if ( X_combos[0] & CMBS_TABFIXEDWIDTH ){
		DWORD X_twid[2] = {96, 0};

		GetCustData(T("X_twid"), &X_twid, sizeof(X_twid));
		SendMessage(hTabWnd, TCM_SETITEMSIZE, 0, TMAKELPARAM(X_twid[0], X_twid[1]));
	}

	Combo.Tabs++;

	if ( Combo.BaseCount > 0 ){
		if ( X_combos[0] & CMBS_TABEACHITEM ){
			AddTabInfo(tabpane, Combo.base[Combo.show[tabpane].baseNo].hWnd);
		}else{
			for ( item = 0 ; item < Combo.BaseCount ; item++ ){
				AddTabInfo(tabpane, Combo.base[item].hWnd);
			}
		}
	}
}

void CreateTabBar(int mode)
{
	LoadCommonControls(ICC_TAB_CLASSES);
	TabHeight = Combo.Font.size.cy + 10;

	NewTabBar();
	if ( mode == CREATETAB_APPEND ){
		if ( X_combos[0] & CMBS_TABSEPARATE ){
			while ( Combo.Tabs < Combo.ShowCount ) NewTabBar();
		}else{
			int showindex = 1;

			while ( showindex < Combo.ShowCount ){
				Combo.show[showindex].tab = Combo.show[0].tab;
				showindex++;
			}
		}
	}
}

void SetTabInfoMain(int setinfo, int showindex, HWND hItemWnd, TCHAR *buf)
{
	TC_ITEM tie;
	int tabpane, tabmin, tabmax;
	int msg;

	tie.mask = TCIF_TEXT | TCIF_PARAM;
	tie.pszText = buf;
	tie.lParam = (LPARAM)hItemWnd;

	msg = (setinfo == SETTABINFO_SET) ? TCM_SETITEM : TCM_INSERTITEM;
	if ( showindex < 0 ){
		tabmin = 0;
		tabmax = Combo.Tabs - 1;
	}else{
		tabmin = tabmax = showindex;
	}

	for ( tabpane = tabmin ; tabpane <= tabmax ; tabpane++ ){
		HWND hTabWnd;
		int tabid;

		TC_ITEM atie;
		int baseindex, abaseindex;
		PPC_APPINFO *cinfo;

		hTabWnd = Combo.show[tabpane].tab.hWnd;
		tabid = GetTabItemIndex(hItemWnd, tabpane);
		if ( setinfo == SETTABINFO_SET ){ // (SetTabInfo)
			if ( tabid < 0 ) continue; // 登録されていない
			LastTabInsType = 8000;
		}else{ // SETTABINFO_ADD(AddTabInfo) / SETTABINFO_ADDENTRY
			// ● 1.2x タブのペイン増加で表示するとき、タブが余分に追加されることがあるため、暫定でチェックしている。
			// 再現方法…タブをペイン毎表示、タブはペイン間共通、で F11/Q を繰り返して、タブバーを作成／廃棄を繰り返しているときに起きる
			if ( tabid >= 0 ) continue; // 既に登録されている

			tabid = Combo_Max_Base;
			if ( (setinfo == SETTABINFO_ADDENTRY) && (ComboInit == 0) ){
				if ( X_combos[0] & CMBS_TABDIRSORTNEW ){ // ディレクトリ順にする
					int atabid;
					TCHAR *newpath;

					baseindex = GetComboBaseIndex(hItemWnd);
					if ( (baseindex >= 0) &&
						 ((cinfo = Combo.base[baseindex].cinfo) != NULL) ){
						newpath = cinfo->path;

						for ( atabid = 0 ;  ; atabid++ ){
							atie.mask = TCIF_PARAM;
							if ( FALSE == TabCtrl_GetItem(hTabWnd, atabid, &atie) ){
								break;
							}
							abaseindex = GetComboBaseIndex((HWND)atie.lParam);
							if ( (abaseindex >= 0) && (Combo.base[abaseindex].cinfo != NULL) ){
								if ( tstrcmp(newpath, Combo.base[abaseindex].cinfo->path) < 0 ){
									tabid = atabid;
									break;
								}
							}
						}
					}
				}else if ( X_combos[0] & CMBS_TABNEXTTABNEW ){
					int sel = TabCtrl_GetCurSel(hTabWnd) + 1;
					if ( tabid > 0 ) tabid = sel;
				}
			}
			LastTabInsType = 9000;
		}

		if ( tabid != 0 ){
			atie.mask = TCIF_PARAM;
			if ( TabCtrl_GetItem(hTabWnd, 0, &atie) ){
				baseindex = GetComboBaseIndex(hItemWnd);
				if ( (baseindex >= 0) &&
						 ((cinfo = Combo.base[baseindex].cinfo) != NULL) ){
					abaseindex = GetComboBaseIndex((HWND)atie.lParam);
					if ( (abaseindex >= 0) && (Combo.base[abaseindex].cinfo != NULL) ){
						if ( Combo.base[abaseindex].cinfo == cinfo ){
							continue;
						}
						if ( tstrcmp(cinfo->RegSubCID, Combo.base[abaseindex].cinfo->RegSubCID) == 0 ){
							PPxCommonExtCommand(K_SENDREPORT, (WPARAM)
									((setinfo == SETTABINFO_SET) ?
									 T("同ID設定") : T("同ID追加")) );
							continue;
						}
					}
				}
			}
		}
		SendMessage(hTabWnd, msg, (WPARAM)tabid, (LPARAM)&tie);
	}
}

void SetTabInfoData(int setinfo, int showindex, HWND hItemWnd)
{
	TCHAR caption[VFPS + 50], buf[VFPS + 50], base[64], *path, *p;
	int baseindex;
	PPC_APPINFO *cinfo;
	BOOL lock = FALSE;

	caption[0] = '\0';
	SendMessage(hItemWnd, WM_GETTEXT,
			(WPARAM)TSIZEOF(caption), (LPARAM)caption);
	if ( hItemWnd == hComboFocus ) SetWindowText(Combo.hWnd, caption);

	baseindex = GetComboBaseIndex(hItemWnd);
	if ( (baseindex >= 0) && ((cinfo = Combo.base[baseindex].cinfo) != NULL) ){
		lock = cinfo->ChdirLock;
		wsprintf(base, T("[%s]"), cinfo->RegSubCID + 1);

		if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
			 (cinfo->e.Dtype.BasePath[0] != '\0') ){
			path = cinfo->e.Dtype.BasePath;
		}else{
			path = cinfo->path;
		}

		p = VFSFindLastEntry(path + 3);	// 最終エントリを取り出す
		if ( (*p == '\0') || ((*p == '\\') && (*(p + 1) == '\0')) ){
			p = path;
		}
	}else{
		cinfo = NULL;
		path = tstrchr(caption, ']');
		if ( (path == NULL) || ((path - caption) > 10) ){
			base[0] = '\0';
			path = caption;
		}else{
			path++;
			p = tstrchr(caption, '[');
			if ( (p == NULL) || (p >= path) ){
				base[0] = '\0';
				path = caption;
			}else{
				memcpy(base, path, (path - p) * sizeof(TCHAR) );
				base[(path - p)] = '\0';
			}
		}

		p = VFSFindLastEntry(path);	// 最終エントリを取り出す
		if ( XC_dpmk ){				// マスクが付いているときは除去
			if ( *p == '\\' ){
				*p = '\0';
				p = VFSFindLastEntry(path);
			}
		}
	}
	if ( hItemWnd == hComboFocus ) SetComboAddresBar(path);
	if ( Combo.Tabs == 0 ) return;

	{
		TCHAR *dst;

		dst = buf;
		if ( lock ) *dst++ = '#';
		if ( (TabCaptionText != NULL) && (cinfo != NULL) ){
			PP_ExtractMacro(NULL, &cinfo->info, NULL, TabCaptionText, dst, XEO_DISPONLY);
		}else{
			if ( TabCaptionType == 1 ){
				tstrcpy(dst, base);
				dst += tstrlen(dst);
			}
			if ( *p != '\\' ){
				tstrcpy(dst, path);
			}else{
				tstrcpy(dst, p + 1);
			}
		}
	}
	if ( (X_combos[1] & (CMBS1_TABFIXLAYOUT | CMBS1_NOTABBUTTON)) == 0  ){ // タブ幅が可変長の時、閉じるボタン分の場所を確保
		tstrcat(buf, T("    ")); // " [x]"
	}
	SetTabInfoMain(setinfo, showindex, hItemWnd, buf);
}

// hWnd に該当する Tab item の index を取得
int GetTabItemIndex(HWND hWnd, int tabwndindex)
{
	TC_ITEM tie;
	int tabindex;
	int tabcount;
	HWND hTabWnd;

	hTabWnd = Combo.show[tabwndindex].tab.hWnd;
	tabcount = TabCtrl_GetItemCount(hTabWnd);
	for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
		tie.mask = TCIF_PARAM;
		if ( TabCtrl_GetItem(hTabWnd, tabindex, &tie) == FALSE ) continue;
		if ( tie.lParam == (LPARAM)hWnd ) return tabindex;
	}
	return -1;
}


#define CMENU_NEWTAB	1
#define CMENU_NEWPANE	2
#define CMENU_HIDE		3
#define CMENU_EJECT		4
#define CMENU_CLOSE		5
#define CMENU_LOCK		6
#define CMENU_COLOR		7
#define CMENU_AUTOCOLOR	8
#define CMENU_CLOSELEFT		9	// CLOSELEFT/CLOSEPANE/CLOSERIGHT は必ずこの順に
#define CMENU_CLOSEPANE		10
#define CMENU_CLOSERIGHT	11
#define CMENU_SWAPPANE		12
#define CMENU_SAVEWIDTH		13
#define CMENU_KEYSELECT		14
#define CMENU_CLOSEDLIST	30000
#define CMENU_ADDITEMS		100

void SetTabColor(int baseindex)
{
	TCHAR id[16], value[32];
	PPC_APPINFO *cinfo;

	InvalidateRect(Combo.hWnd, NULL, TRUE);

	if ( (cinfo = Combo.base[baseindex].cinfo) != NULL ){
		wsprintf(id, T("%s_tabcolor"), (cinfo->RegSubIDNo < 0) ? cinfo->RegID : cinfo->RegSubCID);
		if ( (Combo.base[baseindex].tabbackcolor == C_AUTO) && (Combo.base[baseindex].tabtextcolor == C_AUTO) ){
			DeleteCustTable(T("_Path"), id, 0);
		}else{
			wsprintf(value, T("H%x, H%x"), Combo.base[baseindex].tabtextcolor, Combo.base[baseindex].tabbackcolor);
			SetCustStringTable(T("_Path"), id, value, 22);
		}
	}
}

const TCHAR comdlg32name[] = T("comdlg32.dll");
void SetTabColorDialog(int baseindex)
{
	CHOOSECOLOR cc;
	DefineWinAPI(BOOL, ChooseColor, (LPCHOOSECOLOR lpcc));
	COLORREF userColor[16];

	HMODULE hComdlg32 = LoadLibrary(comdlg32name);
	if ( hComdlg32 == NULL ) return;

	GETDLLPROCT(hComdlg32, ChooseColor);

	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = Combo.hWnd;
	cc.rgbResult = Combo.base[baseindex].tabbackcolor;
	cc.lpCustColors = userColor;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	if ( DChooseColor(&cc) ){
		if ( !(X_combos[0] & CMBS_TABCOLOR) ){
			setflag(X_combos[0], CMBS_TABCOLOR);
			SetCustData(T("X_combos"), &X_combos, sizeof(X_combos));
		}
		Combo.base[baseindex].tabtextcolor = C_AUTO;
		Combo.base[baseindex].tabbackcolor = cc.rgbResult;
		SetTabColor(baseindex);
	}

	FreeLibrary(hComdlg32);
}

void ClosePanes(HWND hTabWnd, int baseindex, int mode, BOOL closelocked)
{
	TC_ITEM tie;
	int tabindex;
	int tabcount;
	HWND hSWnd;
	int first = 0, last = TabCtrl_GetItemCount(hTabWnd) - 1;
	WPARAM closedata;

	closedata = TMAKEWPARAM(
			closelocked ? KCW_closealltabs : KCW_closetabs,
			GetTabShowIndex(hTabWnd));

	hSWnd = Combo.base[baseindex].hWnd;
	tabcount = last + 1;
	for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
		tie.mask = TCIF_PARAM;
		if ( TabCtrl_GetItem(hTabWnd, tabindex, &tie) == FALSE ) continue;
		if ( tie.lParam != (LPARAM)hSWnd ) continue;

		if ( mode < 0 ) last = tabindex - 1;
		if ( mode > 0 ) first = tabindex + 1;
		PostMessage(Combo.hWnd, WM_PPXCOMMAND, closedata, TMAKELPARAM(first, last));
		return;
	}
}

void NewPane(int baseindex, const TCHAR *param)
{
	HWND hPaneWnd = Combo.base[baseindex].hWnd;

	if ( (X_combos[0] & CMBS_TABEACHITEM) || (GetComboShowIndex(hPaneWnd) >= 0) ){
		CreateNewPane(param);
		return;
	}
	if ( baseindex < 0 ){
		int bi;

		if ( (Combo.ShowCount == 1) && (hComboRightFocus != NULL) ){
			baseindex = GetComboBaseIndex(hComboRightFocus);
		}else{
			for ( bi = Combo.BaseCount - 1 ; bi >= 0 ; bi-- ){
				if ( GetComboShowIndex(Combo.base[bi].hWnd) < 0 ){
					baseindex = bi;
					break;
				}
			}
		}
		if ( baseindex < 0 ){
			CreateNewPane(param);
			return;
		}
	}
	// ※ param 未サポート
//	ChangeReason = T("CMENU_NEWPANE");
	CreateAndInitPane(baseindex);
	SetFocus(hPaneWnd);
}

void SwapPane(int targetpane)
{
	int swapshow, rightpane = GetComboShowIndex(hComboRightFocus);
	COMBOPANES tmpshow;

	swapshow = GetComboShowIndex(hComboFocus);
	if ( swapshow == targetpane ){
		swapshow = rightpane;
		if ( swapshow == targetpane ){
			swapshow = 0;
		}
	}
	if ( (swapshow < 0) || (targetpane < 0) ) return;

	if ( targetpane == rightpane ){
		hComboRightFocus = Combo.base[Combo.show[swapshow].baseNo].hWnd;
	}else if ( swapshow == rightpane ){
		hComboRightFocus = Combo.base[Combo.show[targetpane].baseNo].hWnd;
	}
	ChangeReason = T("SwapPane");

	tmpshow = Combo.show[targetpane];
	Combo.show[targetpane] = Combo.show[swapshow];
	Combo.show[swapshow] = tmpshow;

	InvalidateRect(Combo.hWnd, NULL, TRUE);
	SortComboWindows(SORTWIN_LAYOUTPAIN);
}

void AddClosedList(HMENU hMenu)
{
	TCHAR buf[VFPS + 16];
	int index, id, count = 0;
	HMENU hSubMenu = CreatePopupMenu();

	id = Combo.ClosedIndex - 1;
	for ( index = 0; index < Combo.CloededItems; index++, id-- ){
		if ( id < 0 ) id = Combo.CloededItems - 1;
		if ( Combo.closed[id].ID[0] != '\0' ){
			int offset;

			offset = wsprintf(buf, T("%s : "), Combo.closed[id].ID + 1);
			GetCustTable(T("_Path"), Combo.closed[id].ID, buf + offset, VFPS);
			AppendMenuString(hSubMenu, CMENU_CLOSEDLIST + id, buf);
			count++;
		}
	}
	if ( count > 0 ){
		AppendMenu(hMenu, MF_EPOP, (UINT_PTR)hSubMenu, MessageText(MES_TABS));
	}else{
		DestroyMenu(hSubMenu);
	}
}

BOOL TabMenu(HWND hTabWnd, int baseindex, int targetpane, POINT *pos)
{
	HMENU hMenu;
	int menuindex, showindex = -1;
	HWND hPaneWnd = NULL;
	ThSTRUCT thMenuData;
	POINT temppos;
	TCHAR buf[VFPS + 32];
	DWORD exid = CMENU_ADDITEMS;

	if ( baseindex >= Combo.BaseCount ) return FALSE;
	if ( targetpane < 0 ){
		targetpane = GetComboShowIndex(Combo.base[baseindex].hWnd);
	}
	if ( hTabWnd == NULL ){
		if ( targetpane >= 0 ){
			hTabWnd = Combo.show[targetpane].tab.hWnd;
		}
	}

	ThInit(&thMenuData);
	hMenu = CreatePopupMenu();
	AppendMenuString(hMenu, CMENU_NEWTAB, MES_TABN);
	if ( baseindex >= 0 ){
		if ( Combo.base[baseindex].cinfo != NULL ){
			AppendMenuCheckString(hMenu, CMENU_LOCK, MES_TABL,
					Combo.base[baseindex].cinfo->ChdirLock);
			if ( Combo.BaseCount > 1 ){
				AppendMenuString(hMenu, CMENU_EJECT, MES_TABE);
			}
		}

		hPaneWnd = Combo.base[baseindex].hWnd;
		showindex = GetComboShowIndex(hPaneWnd);
		if ( showindex >= 0 ){
			if ( pos == NULL ){
				temppos.x = Combo.show[showindex].box.left;
				temppos.y = Combo.show[showindex].box.top;
				pos = &temppos;
				ClientToScreen(Combo.hWnd, &temppos);
			}
			if ( Combo.ShowCount > 1 ){
				if ( !(X_combos[0] & CMBS_TABEACHITEM) ){
					AppendMenuString(hMenu, CMENU_HIDE, MES_TABH);
				}else{
					AppendMenuString(hMenu, CMENU_CLOSEPANE, MES_TACP);
				}
			}
		}else{
			AppendMenuString(hMenu, CMENU_NEWPANE, MES_TABP);
		}

		if ( Combo.ShowCount >= 2 ){
			AppendMenuString(hMenu, CMENU_SWAPPANE, MES_TABW);
		}

		if ( hTabWnd != NULL ){
			AppendMenuString(hMenu, CMENU_KEYSELECT, MES_TABK);
			AppendMenuString(hMenu, CMENU_COLOR, MES_TABR);
			if ( Combo.base[baseindex].tabbackcolor != C_AUTO ){
				AppendMenuString(hMenu, CMENU_AUTOCOLOR, MES_TABD);
			}

			AppendMenuString(hMenu, CMENU_CLOSELEFT, MES_TACL);
			AppendMenuString(hMenu, CMENU_CLOSERIGHT, MES_TACR);
		}

		PP_AddMenu( (Combo.base[baseindex].cinfo != NULL) ?
				 &Combo.base[baseindex].cinfo->info : NULL,
				Combo.hWnd, hMenu, &exid, T("M_tabc"), &thMenuData);

		AddClosedList(hMenu);
		AppendMenuString(hMenu, CMENU_CLOSE, MES_TABC);
	}else{
		AppendMenuString(hMenu, CMENU_NEWPANE, MES_TABP);
		if ( X_combos[0] & CMBS_TABFIXEDWIDTH ){
			AppendMenuString(hMenu, CMENU_SAVEWIDTH, MES_TABI);
		}
		if ( hTabWnd != NULL ){
			AppendMenuString(hMenu, CMENU_KEYSELECT, MES_TABK);
		}
		AddClosedList(hMenu);
		if ( Combo.ShowCount >= 2 ){
			AppendMenuString(hMenu, CMENU_SWAPPANE, MES_TABW);
			AppendMenuString(hMenu, !(X_combos[0] & CMBS_TABEACHITEM) ?
					CMENU_HIDE : CMENU_CLOSEPANE, MES_TACP);
		}
	}

	if ( pos == NULL ){
		if ( Combo.Tabs && (hTabWnd != NULL) ){	// タブがあればタブに表示
			TC_ITEM tie;
			RECT box;
			int i;

			for ( i = 0 ; i < Combo.BaseCount ; i++ ){	// 検索
				tie.mask = TCIF_PARAM;
				if ( TabCtrl_GetItem(hTabWnd, i, &tie) == FALSE ) continue;
				if ( hPaneWnd != (HWND)tie.lParam ) continue;
				// タブの位置を取得し、変換
				TabCtrl_GetItemRect(hTabWnd, i, &box);
				temppos.x = box.left;
				temppos.y = box.bottom;
				pos = &temppos;
				ClientToScreen(hTabWnd, &temppos);
				break;
			}
		}
		if ( pos == NULL ){	// 該当場所がないので、左上に表示
			temppos.x = 0;
			temppos.y = 0;
			pos = &temppos;
			ClientToScreen(Combo.hWnd, &temppos);
		}
	}
	menuindex = TrackPopupMenu(hMenu, TPM_TDEFAULT, pos->x, pos->y, 0, Combo.hWnd, NULL);
	if ( menuindex >= CMENU_CLOSEDLIST ){
		tstrcpy(buf, T("-bootid: "));
		GetMenuString(hMenu, menuindex, buf + 8, VFPS + 8, MF_BYCOMMAND);
		*tstrchr(buf, ' ') = '\0';
		CreateNewTabParam(showindex, -1, buf);
	}
	DestroyMenu(hMenu);
	switch (menuindex){
		case CMENU_NEWTAB:
			CreateNewTab(targetpane);
			break;

		case CMENU_CLOSE:
			PostMessage(hPaneWnd, WM_CLOSE, 0, 0);
			break;

		case CMENU_HIDE:
			HidePane((showindex >= 0) ? showindex : targetpane);
			break;

		case CMENU_NEWPANE:
			NewPane(baseindex, NULL);
			break;

		case CMENU_EJECT:
			PostMessage(Combo.hWnd, WM_PPXCOMMAND, KCW_eject, (LPARAM)baseindex);
			break;

		case CMENU_AUTOCOLOR:
			if ( baseindex >= 0 ){
				Combo.base[baseindex].tabtextcolor = C_AUTO;
				Combo.base[baseindex].tabbackcolor = C_AUTO;
				SetTabColor(baseindex);
			}
			break;

		case CMENU_COLOR:
			if ( baseindex < 0 ) break;
			SetTabColorDialog(baseindex);
			break;

		case CMENU_LOCK:
			if ( baseindex < 0 ) break;
			if ( Combo.base[baseindex].cinfo != NULL ){
				Combo.base[baseindex].cinfo->ChdirLock =
						!Combo.base[baseindex].cinfo->ChdirLock;
				SetTabInfo(-1, Combo.base[baseindex].hWnd); // ロック状態を文字で表現している間、用意
			}
			break;

		case CMENU_CLOSEPANE:
			if ( baseindex < 0 ) baseindex = Combo.show[targetpane].baseNo;
		case CMENU_CLOSELEFT:
		case CMENU_CLOSERIGHT:
			ClosePanes(hTabWnd, baseindex,
					menuindex - CMENU_CLOSEPANE,
					GetShiftKey() & K_s);
			break;

		case CMENU_SWAPPANE:
			SwapPane(targetpane);
			break;

		case CMENU_SAVEWIDTH: {
			RECT box;

			TabCtrl_GetItemRect(hTabWnd, 0, &box);
			wsprintf(buf, T("%d"), box.right - box.left);
			if ( tInput(hTabWnd, T("Save Width"), buf, TSIZEOF(buf), PPXH_NUMBER, PPXH_NUMBER) > 0 ){
				int X_twid[2];
				const TCHAR *p;

				p = buf;
				X_twid[0] = GetNumber(&p);
				if ( X_twid[0] >= 16 ){
					X_twid[1] = 0;
					SetCustData(T("X_twid"), &X_twid, sizeof(X_twid));
					SendMessage(hTabWnd, TCM_SETITEMSIZE, 0, TMAKELPARAM(X_twid[0], X_twid[1]));
				}
			}
			break;
		}

		case CMENU_KEYSELECT:
			SetFocus(hTabWnd);
			break;

		default:
			if ( menuindex >= CMENU_CLOSEDLIST ) break;
			if ( menuindex >= CMENU_ADDITEMS ){
				const TCHAR *command;

				GetMenuDataMacro2(command, &thMenuData, menuindex - CMENU_ADDITEMS);
				if ( command != NULL ){
					PP_ExtractMacro(Combo.base[baseindex].hWnd,
							(Combo.base[baseindex].cinfo != NULL) ?
							 &Combo.base[baseindex].cinfo->info : NULL,
							NULL, command, NULL, 0);
				}
				break;
			}
			ThFree(&thMenuData);
			return FALSE;
	}
	ThFree(&thMenuData);
	return TRUE;;
}

//============================================================================
// ペイン操作
//============================================================================
// 現在－反対のペインの中身を交換 --------------------------------------------
void ComboSwap(void)
{
	int baseindexL, baseindexR, focusshowindex;
	int showindexL, showindexR;
	int tabL, tabR;
	int tabpane;
	TC_ITEM tieL, tieR;
	TCHAR strL[VFPS + 16], strR[VFPS + 16];
	HWND hNewFocusR;

	if ( Combo.BaseCount < 2 ) return; // １枚しかない
//	CheckComboTable(T("ComboSwap-pre"));

	focusshowindex = GetComboShowIndexDefault(hComboFocus);
	showindexL = 0;										// 左側
	baseindexL = Combo.show[showindexL].baseNo;
	showindexR = GetComboShowIndex(hComboRightFocus);	// 右側
	if ( showindexR > 0 ){
		baseindexR = Combo.show[showindexR].baseNo;
	}else{ // 右側が非表示・左右が同じになった
		if ( showindexR == 0 ) return;
		baseindexR = GetComboBaseIndex(hComboRightFocus);
		if ( baseindexR < 0 ){
			if ( Combo.ShowCount < 2 ){
				baseindexR = 1;
			}else{
				showindexR = 1;
				baseindexR = Combo.show[showindexR].baseNo;
			}
		}
		if ( baseindexR == baseindexL ) baseindexR = baseindexL ? 0 : 1;
	}
							// 表示入れ替え
	Combo.show[showindexL].baseNo = baseindexR;
	hNewFocusR = Combo.base[(showindexL == 0) ? baseindexL : baseindexR].hWnd;

	if ( showindexR >= 0 ){ // 反対ペイン有り
		Combo.show[showindexR].baseNo = baseindexL;
	}else{ // ペインは１つのみ…表示入れ替え
		HWND hNewLeftWnd;

		hNewLeftWnd = Combo.base[baseindexR].hWnd;

		//●この２行は整合性チェック(WM_SETFOCUS)の誤判定回避のときに必要1.29+5
		//	hComboFocus = hNewLeftWnd;
		//	hComboRightFocus = hNewFocusR;

		ShowWindow(Combo.base[baseindexL].hWnd, SW_HIDE);
		ShowWindow(hNewLeftWnd, SW_SHOWNORMAL);
	}

	if ( (showindexR > 0) && (X_combos[0] & CMBS_TABEACHITEM) ){ // タブ入れ替え(各ペイン間で入替え)
		tabL = GetTabItemIndex(Combo.base[baseindexL].hWnd, showindexL);
		tabR = GetTabItemIndex(Combo.base[baseindexR].hWnd, showindexR);
		if ( (tabL >= 0) && (tabR >= 0) ){
			tieL.mask = tieR.mask = TCIF_TEXT | TCIF_PARAM;
			tieL.cchTextMax = tieR.cchTextMax = VFPS + 16;
			tieL.pszText = strL;
			tieR.pszText = strR;
			TabCtrl_GetItem(Combo.show[showindexL].tab.hWnd, tabL, &tieL);
			TabCtrl_GetItem(Combo.show[showindexR].tab.hWnd, tabR, &tieR);
			TabCtrl_SetItem(Combo.show[showindexL].tab.hWnd, tabL, &tieR);
			TabCtrl_SetItem(Combo.show[showindexR].tab.hWnd, tabR, &tieL);
		}
	}else{ // タブ入れ替え(各ペイン毎に入れ替え)
		for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
			tabL = GetTabItemIndex(Combo.base[baseindexL].hWnd, tabpane);
			tabR = GetTabItemIndex(Combo.base[baseindexR].hWnd, tabpane);
			if ( (tabL >= 0) && (tabR >= 0) ){
				tieL.mask = tieR.mask = TCIF_TEXT | TCIF_PARAM;
				tieL.cchTextMax = tieR.cchTextMax = VFPS;
				tieL.pszText = strL;
				tieR.pszText = strR;
				TabCtrl_GetItem(Combo.show[tabpane].tab.hWnd, tabL, &tieL);
				TabCtrl_GetItem(Combo.show[tabpane].tab.hWnd, tabR, &tieR);
				TabCtrl_SetItem(Combo.show[tabpane].tab.hWnd, tabL, &tieR);
				TabCtrl_SetItem(Combo.show[tabpane].tab.hWnd, tabR, &tieL);
			}
		}
	}
	hComboRightFocus = hNewFocusR;
	ChangeReason = T("ComboSwap");
//	CheckComboTable(T("ComboSwap"));

	SortComboWindows(SORTWIN_LAYOUTPAIN);
	if ( focusshowindex >= 0 ){
		SetFocus(Combo.base[Combo.show[focusshowindex].baseNo].hWnd);
	}
}

void SelectTabByWindow(HWND hTargetWnd, int selectpane)
{
	int tabindex;

	tabindex = GetTabItemIndex(hTargetWnd, selectpane);
	if ( tabindex < 0 ) return;
	TabCtrl_SetCurSel(Combo.show[selectpane].tab.hWnd, tabindex);
}

void SetFocusWithBackupedRightWindow(HWND hNewFocusWnd)
{
	HWND hTmpRFocus = hComboRightFocus;

	SetFocus(hNewFocusWnd);

	// SetFocus により、hComboRightFocus が変化してしまった場合は元に戻す。
	// 但し、hNewFocusWndが新反対窓の場合を除く。
	if ( hNewFocusWnd != hComboRightFocus ){
		ChangeReason = T("SetFocusWithR");
		hComboRightFocus = hTmpRFocus;
	}
}

BOOL USEFASTCALL IsHideTabForMultiShow(void)
{
	int tabpane;

	if ( X_combos[1] & CMBS1_TABSHOWMULTI ){
		if ( Combo.ShowCount > 1 ) return FALSE; // 2ペイン以上なので表示
	}

	if ( !(X_combos[0] & CMBS_TABEACHITEM) ){ // タブ共通
		if ( Combo.ShowCount > X_mpane.limit ) return FALSE; // ペイン数が最大以上なら表示
		if ( Combo.BaseCount <= X_mpane.limit ) return TRUE; // 全タブが最大以下なら非表示
	}

	// ２タブ以上のペインがあれば表示
	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( TabCtrl_GetItemCount(Combo.show[tabpane].tab.hWnd) >= 2 ){
			return FALSE; // 2tab 以上のペインがある
		}
	}
	return TRUE; // 全ペイン１タブなので非表示
}

#define CutTableItem(table, index, max) memmove(&table[(index)], &table[(index)+1], (BYTE *)&table[max] - (BYTE *)&table[(index)+1]); // table[index] を除去して残りを詰める

// １窓終了時の後処理 --------------------------------------------
void DestroyedPaneWindow(HWND hComboWnd, HWND hPaneWnd)
{
	int showindex, baseindex;
	HWND hNewFocusWnd = NULL;
	BOOL DecPane = FALSE;
	TCHAR buf[VFPS];

	PPC_APPINFO *cinfo;

	baseindex = GetComboBaseIndex(hPaneWnd);
	if ( baseindex < 0 ) return; // 存在しない

	showindex = GetComboShowIndex(hPaneWnd);
	cinfo = Combo.base[baseindex].cinfo;

	if ( comboDocks.t.cinfo == cinfo ){
		comboDocks.t.cinfo = NULL;
		comboDocks.b.cinfo = NULL;
	}

	if ( (Combo.closed != NULL) && (Combo.CloededItems > 0) && (cinfo != NULL) ){
		tstrcpy(Combo.closed[Combo.ClosedIndex++].ID, cinfo->RegSubCID);
		if ( Combo.ClosedIndex >= Combo.CloededItems ) Combo.ClosedIndex = 0;
	}

	if ( showindex >= 0 ){						// 表示ペインから削除 ---------
		int newppc = -1;

		if ( Combo.Tabs >= 1 ){ // タブがあるなら、別のタブを表示する
			HWND hTabWnd;
			int tabcount;

			if ( X_combos[0] & CMBS_TABEACHITEM ) newppc = -2; // 未使用割当てを禁止
			hTabWnd = Combo.show[showindex].tab.hWnd;
			tabcount = TabCtrl_GetItemCount(hTabWnd);

			if ( tabcount > 1 ){
				int tabindex, tabbase;

				for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
					TC_ITEM tie;

					tie.mask = TCIF_PARAM;
					if ( IsTrue(TabCtrl_GetItem(hTabWnd, tabindex, &tie)) ){
						tabbase = GetComboBaseIndex((HWND)tie.lParam);
						if ( (tabbase >= 0) &&
							 (GetComboShowIndex((HWND)tie.lParam) < 0) ){
							newppc = tabbase;
						}
						if ( ((HWND)tie.lParam == hPaneWnd) && (newppc >= 0) ){
							break;
						}
					}
				}
			}
		}

		// 未使用窓(別のペインでも可)があるなら割り当てる
		if ( (newppc == -1) && (Combo.BaseCount > X_mpane.limit) && (Combo.ShowCount >= X_mpane.limit) ){
			int newbase;

			for ( newbase = 0 ; newbase < Combo.BaseCount ; newbase++ ){
				int showc;

				for ( showc = 0 ; showc < Combo.ShowCount ; showc++ ){
					if ( Combo.show[showc].baseNo == newbase ) break;
				}
				if ( showc == Combo.ShowCount ){
					newppc = newbase;
					break;
				}
			}
		}

		if ( newppc < 0 ){
			WINPOS WPos;

			if ( (X_combos[0] & CMBS_TABMINCLOSE) && (Combo.ShowCount > 1) && (Combo.ShowCount == X_mpane.first) ){
				PostMessage(hComboWnd, WM_CLOSE, 0, 0);
				return; // タブ並びを保存するために、以降の処理をしない
			}
			wsprintf(buf, T("%s%d"), ComboID, showindex);
			WPos.show = 0;
			WPos.reserved = 0;
			WPos.pos = Combo.show[showindex].box;
			SetCustTable(Str_WinPos, buf, &WPos, sizeof(WPos));

			hNewFocusWnd = DeletePane(showindex);
			DecPane = TRUE;
		}else{ // 新しいPPcを設定
			SendMessage(Combo.hWnd, WM_SETREDRAW, FALSE, 0);
			Combo.show[showindex].baseNo = newppc;
			if ( hPaneWnd == hComboRightFocus ){
				hComboRightFocus = Combo.base[newppc].hWnd;
				ChangeReason = T("DestroyedPaneWindow92");
			}
			if ( hPaneWnd == hComboFocus ){
				hNewFocusWnd = Combo.base[newppc].hWnd;
			}else if ( Combo.Tabs >= 2 ){ // 現在窓以外の表示タブが変わった
				// →タブの再選択が必要
				int tabpane, focuspane;

				if ( X_combos[0] & CMBS_TABEACHITEM ){
					focuspane = GetComboShowIndex(hComboFocus);
					for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
						if ( tabpane == focuspane ) continue;
						SelectTabByWindow(Combo.base[newppc].hWnd, tabpane);
					}
				}else{
					SelectTabByWindow(Combo.base[newppc].hWnd, showindex);
				}
			}
			if ( (hComboRightFocus == hNewFocusWnd) && (showindex == 0) ){
				if ( Combo.ShowCount > 1 ){
					hComboRightFocus = Combo.base[Combo.show[1].baseNo].hWnd;
					ChangeReason = T("DestroyedPaneWindow91");
				}else{
					if ( Combo.BaseCount <= 1 ){
						hComboRightFocus = NULL;
					}else{
						hComboRightFocus = (Combo.base[0].hWnd == hNewFocusWnd)
							? Combo.base[1].hWnd : Combo.base[0].hWnd;
					}
				}
			}
			CheckComboTable(T("@DestroyedPaneWindow1"));
			ShowWindow(Combo.base[newppc].hWnd, SW_SHOWNOACTIVATE);
			SendMessage(Combo.hWnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(Combo.hWnd, NULL, TRUE);
		}
	}

	// COMBOITEMSTRUCT を使用できないようにする
	Combo.base[baseindex].hWnd = (HWND)(DWORD_PTR)(-2);
	Combo.base[baseindex].cinfo = NULL;

	// 親ディレクトリのタブを捜す
	if ( (X_combos[0] & CMBS_TABUPDIRCLOSE) && (cinfo != NULL) ){
		if ( VFSFullPath(buf, T(".."), cinfo->path) != NULL ){
			int bi;

			for ( bi = 0 ; bi < Combo.BaseCount ; bi++ ){
				if ( (Combo.base[bi].cinfo != NULL) &&
						!tstrcmp(buf, Combo.base[bi].cinfo->path) ){
					hNewFocusWnd = Combo.base[bi].hWnd;
					break;
				}
			}
		}
	}

	if ( (hNewFocusWnd == NULL) && (hPaneWnd == hComboFocus) ){
		if ( (showindex > 0) && (Combo.ShowCount > 1) && (Combo.base[Combo.show[1].baseNo].hWnd != hNewFocusWnd) ){ // 左
			hNewFocusWnd = Combo.base[Combo.show[1].baseNo].hWnd;
		}else if ( Combo.base[Combo.show[0].baseNo].hWnd != hNewFocusWnd ){
			hNewFocusWnd = Combo.base[Combo.show[0].baseNo].hWnd;
		}
	}

	if ( hPaneWnd == hComboRightFocus ){
		hComboRightFocus = NULL;
		ChangeReason = T("DestroyedPaneWindow92");
	}

	{ // 全てのタブからこの窓を削除
		int tabindex, tabpane;

		for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
			tabindex = GetTabItemIndex(hPaneWnd, tabpane);
			if ( tabindex >= 0 ){
				HWND hTabWnd = Combo.show[tabpane].tab.hWnd;

				// 削除するタブが画面左端の場合に削除すると、より左側のタブが
				// 画面に出てこないことがあるので、一旦左端を選択して、
				// より多く表示させるようにする
				// ※削除タブが最終タブの場合、タブが選択されていないと
				//   操作ができなくなる対策も含む
				if ( tabindex > 0 ){
					RECT box;

					if ( TabCtrl_GetItemRect(hTabWnd, tabindex, &box) ){
						if ( box.left < 8 ){ // 非選択:0 画面左端:2
							TabCtrl_SetCurSel(hTabWnd, 0);
						}
					}
				}
				TabCtrl_DeleteItem(hTabWnd, tabindex);
			}
		}
	}
									// 全体テーブルから削除

	if ( (baseindex + 1) < Combo.BaseCount ){
		CutTableItem(Combo.base, baseindex, Combo.BaseCount);
	}
	Combo.BaseCount--;
	{							// 表示テーブルを補正する
		int i;

		for ( i = 0 ; i < Combo.ShowCount ; i++ ){
			if ( Combo.show[i].baseNo > baseindex ) Combo.show[i].baseNo--;
		}
	}
									// フォーカス補正／終了処理
	if ( Combo.BaseCount ){
		SendMessage(Combo.hWnd, WM_SETREDRAW, FALSE, 0);
		if ( Combo.ShowCount == 0 ){
			CreatePane(0);
			ShowWindow(Combo.base[0].hWnd, SW_SHOWNORMAL);
		}
		// タブが不要になったので、なくす
		if ( !(X_combos[0] & CMBS_TABALWAYS) && Combo.Tabs &&
			 	IsHideTabForMultiShow() ){
			int tabpane;

			for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
				DestroyWindow(Combo.show[tabpane].tab.hWnd);
				Combo.show[tabpane].tab.hWnd = NULL;
			}
			Combo.Tabs = 0;
			Combo.show[0].box.bottom += TabHeight; // CMBS_VALWINSIZE が有効の時、これがないと高さが縮む
			TabHeight = 0;
		}

		if ( hNewFocusWnd != NULL ){
			SetFocusWithBackupedRightWindow(hNewFocusWnd);
		}

		if ( hComboRightFocus == NULL ) ResetR(NULL);
		SortComboWindows( (DecPane && (X_combos[0] & CMBS_VALWINSIZE)) ?
				0 : SORTWIN_LAYOUTPAIN);
		SendMessage(Combo.hWnd, WM_SETREDRAW, TRUE, 0);
		{
			int i;

			for ( i = 0 ; i < Combo.ShowCount ; i++ ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[Combo.show[i].baseNo].cinfo;
				if ( cinfo != NULL ){
					if ( cinfo->hHeaderWnd != NULL ){
						InvalidateRect(cinfo->hHeaderWnd, NULL, FALSE);
					}
					if ( cinfo->hScrollBarWnd != NULL ){
						InvalidateRect(cinfo->hScrollBarWnd, NULL, FALSE);
					}
				}
			}
		}
		InvalidateRect(Combo.hWnd, NULL, TRUE);

	}else{
		PostMessage(hComboWnd, WM_CLOSE, 0, 0);
	}
}

// 指定のペインに指定の窓を設定 --------------------------------------------
void SelectComboWindow(int showindex, HWND hTargetWnd, BOOL focus)
{
	int targetshowindex, targetbaseindex;

	CheckComboTable(T("SelectComboWindow-pre"));

	if ( showindex < 0 ) showindex = 0;
	if ( showindex >= Combo.ShowCount ) showindex = Combo.ShowCount - 1;

	targetshowindex = GetComboShowIndex(hTargetWnd);
	if ( targetshowindex >= 0 ){
		targetbaseindex = Combo.show[targetshowindex].baseNo;
	}else{
		targetbaseindex = GetComboBaseIndex(hTargetWnd);
		if ( targetbaseindex < 0 ) return; // 該当無し
	}

	// フォーカスがあるペインを変える為、フォーカスを常に設定する必要がある
	if ( (focus == FALSE) && (showindex == GetComboShowIndex(hComboFocus)) ){
		focus = TRUE;
	}

	if ( Combo.base[targetbaseindex].cinfo == NULL ){
		TCHAR path[VFPS];

		path[0] = '\0';
		GetWindowText(hTargetWnd, path, VFPS);
		SetComboAddresBar(path);
	}

	if ( Combo.Tabs <= 1 ){
		if ( targetshowindex >= 0 ){
			ChangeReason = T("SelectComboWindow@1");
			if ( focus ) SetFocus(hTargetWnd);
			return;
		}
		// 表示されていないタブなので切換
		SelectHidePane(showindex, hTargetWnd);
	}else{ // タブの分離表示中
		int showindexBaseindex;

		if ( X_combos[0] & CMBS_TABEACHITEM ){
			// 表示しようとしているペインに該当タブがない…追加する
			if ( GetTabItemIndex(hTargetWnd, showindex) < 0 ){
				// 既存ペインに該当タブがあれば削除
				int tabindex;
				for ( tabindex = 0 ; tabindex < Combo.Tabs ; tabindex++ ){
					int tabii;

					tabii = GetTabItemIndex(hTargetWnd, tabindex);
					if ( tabii >= 0 ){ // 登録されていたので削除
						TabCtrl_DeleteItem(Combo.show[tabindex].tab.hWnd, tabii);
					}
				}
				AddTabInfo(showindex, hTargetWnd);
			}
		}

		if ( targetshowindex >= 0 ){
			if ( targetshowindex != showindex ){ // 別のタブに表示中
				HWND hSwapTargetWnd;

				// 情報入れ替え
				Combo.show[targetshowindex].baseNo = showindexBaseindex = Combo.show[showindex].baseNo;
				Combo.show[showindex].baseNo = targetbaseindex;
				if ( hComboRightFocus == Combo.base[showindexBaseindex].hWnd ){
					hComboRightFocus = Combo.base[targetbaseindex].hWnd;
					ChangeReason = T("SelectComboWindow1");

				}else if ( hComboRightFocus == Combo.base[targetbaseindex].hWnd ){
					hComboRightFocus = Combo.base[showindexBaseindex].hWnd;
					ChangeReason = T("SelectComboWindow2");

				}

				if ( (hComboRightFocus == NULL) || (Combo.ShowCount == 1) ){
					ResetR(NULL); // １窓になった／反対窓が消えたので反対窓を再設定
					ChangeReason = T("SelectComboWindow2a");
				}

				CheckComboTable(T("SelectComboWindow1"));
				SortComboWindows(SORTWIN_LAYOUTPAIN);
				InvalidateRect(Combo.hWnd, &Combo.Panes.box, TRUE);
				hSwapTargetWnd = Combo.base[showindexBaseindex].hWnd;
				InvalidateRect(hSwapTargetWnd, NULL, TRUE);

				// 入れ替え先のタブを切り替え
				SelectTabByWindow(hSwapTargetWnd, targetshowindex);
			}
		}else{ // 表示していない窓
			SendMessage(Combo.hWnd, WM_SETREDRAW, FALSE, 0);
			showindexBaseindex = Combo.show[showindex].baseNo;
			Combo.show[showindex].baseNo = targetbaseindex;
			if ( Combo.base[showindexBaseindex].hWnd == hComboRightFocus ){
				hComboRightFocus = hTargetWnd;
				ChangeReason = T("SelectComboWindow3");
			}
			// SW_HIDE 先が focus を持っている場合、foucs 変化が生じるので、
			// 先にfocusをいじっておく
			if ( Combo.base[showindexBaseindex].hWnd == GetFocus() ){
//				ShowWindow(hTargetWnd, SW_SHOWNA);
				// 明後日の位置に表示させて、SortComboWindows による位置移動のときに子ウィンドウ(ツリー)を正しく描画させる
				SetWindowPos(hTargetWnd, NULL, -10, -10, 0, 0,
					SWP_SHOWWINDOW | SWP_NOACTIVATE);
				SetFocus(hTargetWnd);
				ShowWindow(Combo.base[showindexBaseindex].hWnd, SW_HIDE);
				CheckComboTable(T("@SelectComboWindow2a"));
			}else{
				ShowWindow(Combo.base[showindexBaseindex].hWnd, SW_HIDE);
				CheckComboTable(T("@SelectComboWindow2b"));
//				ShowWindow(hTargetWnd, SW_SHOWNA);
				// 明後日の位置に表示させて、SortComboWindows による位置移動のときに子ウィンドウを正しく描画させる
				SetWindowPos(hTargetWnd, NULL, -10, -10, 0, 0,
					SWP_SHOWWINDOW | SWP_NOACTIVATE);
			}
			SendMessage(Combo.hWnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(Combo.hWnd, NULL, TRUE);
			SortComboWindows(SORTWIN_LAYOUTPAIN);
		}
	}

	if ( focus ){
		ChangeReason = T("SelectComboWindow@2");
		SetFocus(hTargetWnd);
	}

	if ( Combo.Tabs &&
		 ( (focus == FALSE) || (Combo.base[targetbaseindex].capture != CAPTURE_WINDOWEX) ) ){
		int tabindex;
		int tabwndindex;

		tabwndindex = Combo.Tabs > 1 ? GetComboShowIndex(hTargetWnd) : 0;
		tabindex = GetTabItemIndex(hTargetWnd, tabwndindex);
		if ( tabindex < 0 ){
			SetTabInfo(tabwndindex, hTargetWnd);
			tabindex = GetTabItemIndex(hTargetWnd, tabwndindex);
		}
		if ( tabindex >= 0 ){
			TabCtrl_SetCurSel(Combo.show[tabwndindex].tab.hWnd, tabindex);
		}
		CheckComboTable(T("SelectComboWindow3"));
	}
}

// 表示中の窓を隠し、ペインを削除 --------------------------------------------
void HidePane(int showindex)
{
	HWND hNewFocusWnd;

	if ( (showindex < 0) || (Combo.ShowCount <= 1) || (Combo.ShowCount <= showindex) ){
		return;
	}

	if ( X_combos[0] & CMBS_TABEACHITEM ) return;

	if ( (Combo.Tabs == 0) && (Combo.ShowCount > 1) ){
		CreateTabBar(CREATETAB_APPEND);
	}

	if ( Combo.base[Combo.show[showindex].baseNo].hWnd == hComboFocus ){
		if ( Combo.ShowCount > 1 ){
			if ( showindex == 0 ){
//				ChangeReason = T("HidePane@1");
				SetFocus(hComboRightFocus);
			}else{
//				ChangeReason = T("HidePane@2");
				SetFocus(Combo.base[Combo.show[0].baseNo].hWnd);
			}
		}
	}
	ShowWindow(Combo.base[Combo.show[showindex].baseNo].hWnd, SW_HIDE);
									// 表示テーブルから削除
	hNewFocusWnd = DeletePane(showindex);
	if ( hNewFocusWnd != NULL ){
		SetFocusWithBackupedRightWindow(hNewFocusWnd);
	}

//	CheckComboTable(T("HidePane"));
	SortComboWindows(SORTWIN_LAYOUTPAIN);
	InvalidateRect(Combo.hWnd, NULL, TRUE);
}

// 隠れていたペインを既存のペインに表示する ----------------------------------
void SelectHidePane(int showindex, HWND hWnd)
{
	int baseindex;

	if ( showindex < 0 ) showindex = 0;

	baseindex = GetComboBaseIndex(hWnd);
	if ( baseindex < 0 ) return;

	if ( Combo.ShowCount == 0 ){ // ペインがない→ペインを追加
		CreatePane(baseindex);
//		CheckComboTable(T("SelectHidePaneA"));
		ShowWindow(hWnd, SW_SHOWNORMAL);
		SortComboWindows(SORTWIN_LAYOUTPAIN);
	}else{
		int tempindex;
		BOOL focus = FALSE;
		HWND hHideWnd;

		tempindex = Combo.show[showindex].baseNo;
		hHideWnd = Combo.base[tempindex].hWnd;
		if ( hHideWnd == hComboFocus ) focus = TRUE;

		// 表示ペインにフォーカス→入れ替え後にフォーカス設定
		if ( hWnd == hComboRightFocus ){
			// 表示ペインが右→入れ替え後に右設定
			hComboRightFocus = hHideWnd;
		}else if ( hHideWnd == hComboRightFocus ){
			// これから表示するペインが右→入れ替え後に右設定
			hComboRightFocus = hWnd;
		}

		SendMessage(Combo.hWnd, WM_SETREDRAW, FALSE, 0);
		Combo.show[showindex].baseNo = baseindex;
		ShowWindow(hHideWnd, SW_HIDE);
		// 明後日の位置に表示させて、SortComboWindows による位置移動のときに子ウィンドウ(ツリー)を正しく描画させる
//		ShowWindow(hWnd, SW_SHOWNOACTIVATE);
		SetWindowPos(hWnd, NULL, -10, -10, 0, 0,
			SWP_SHOWWINDOW | SWP_NOACTIVATE);

		ChangeReason = T("SelectHidePane@1");
		if ( IsTrue(focus) ) SetFocus(hWnd);
//		CheckComboTable(T("SelectHidePaneB"));
		SendMessage(Combo.hWnd, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(Combo.hWnd, NULL, TRUE);
		SortComboWindows(SORTWIN_LAYOUTPAIN);
		ShowWindow(hWnd, SW_SHOWNORMAL);
	}
}

// 表示中の窓を独立化(KCW_eject経由で呼び出さないと、paneのメッセージループ
// 内から呼び出す場合が生じ、トラブルが起きる)
void EjectPane(int baseindex)
{
	PPC_APPINFO *cinfo;
	TCHAR param[MAX_PATH], RegID[10];
	int i;

	if ( (Combo.BaseCount <= 1) || (baseindex >= Combo.BaseCount) ) return;
	cinfo = Combo.base[baseindex].cinfo;
	if ( cinfo == NULL ) return;
	tstrcpy(RegID, cinfo->RegID);
	wsprintf(param, T("/single /show /bootid:%c"), RegID[2]);
	if ( Combo.BaseCount == X_mpane.first ){
		X_mpane.first--;
	}
	PostMessage(Combo.base[baseindex].hWnd, WM_CLOSE, 0, 0);
	for ( i = 0 ; i < 20 ; i++ ){
		PeekLoop();
		Sleep(50);
		if ( PPxGetHWND(RegID) == NULL ) break;
	}
	PPCui(Combo.hWnd, param); // 新規プロセスで生成
	for ( i = 0 ; i < 500 ; i++ ){
		HWND hNewPPcWnd;

		PeekLoop();
		Sleep(50);
		hNewPPcWnd = PPxGetHWND(RegID);
		if ( hNewPPcWnd != NULL ){
			ShowWindow(hNewPPcWnd, SW_SHOW);
			SetForegroundWindow(hNewPPcWnd);
			break;
		}
	}
//	CheckComboTable(T("EjectPane"));
}

// ペインを作成＋登録 --------------------------------------------
void CreatePane(int baseindex)
{
	COMBOPANES *ncs;

	if ( Combo.ShowCount >= Combo_Max_Show ) return;
	if ( (ncs = HeapReAlloc( hProcessHeap, 0, Combo.show, sizeof(COMBOPANES) * (Combo.ShowCount + 2) )) == NULL ){ // +2... 増加する+1 と、予備兼処理簡略化用+1
		return; // ペインを確保する余裕がない
	}
	Combo.show = ncs;
	Combo.show[Combo.ShowCount++].baseNo = baseindex;

	if ( Combo.ShowCount == 2 ) hComboRightFocus = Combo.base[baseindex].hWnd;

	if ( Combo.Tabs == 0 ) return;

	if ( (Combo.ShowCount > 1) && (X_combos[0] & CMBS_TABSEPARATE) ){
		int tabpane;

		if ( !(X_combos[0] & CMBS_TABEACHITEM) ){ // 追加したペイン以外にタブを追加
			for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
				int tabii;

				tabii = GetTabItemIndex(Combo.base[baseindex].hWnd, tabpane);
				if ( tabii < 0 ){ // 登録されていないので追加
					AddTabInfo(tabpane, Combo.base[baseindex].hWnd);
				}
			}
		}else{ // 既存ペインに該当タブがあれば削除
			for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
				int tabii;

				if ( tabpane >= (Combo.ShowCount - 1) ) break;
				tabii = GetTabItemIndex(Combo.base[baseindex].hWnd, tabpane);
				if ( tabii >= 0 ){ // 登録されていたので削除
					TabCtrl_DeleteItem(Combo.show[tabpane].tab.hWnd, tabii);
				}
			}
		}
		NewTabBar();
		SelectTabByWindow(Combo.base[baseindex].hWnd, Combo.ShowCount - 1);
	}else{
		// ペインが２以上なら、タブhwndを設定
		if ( Combo.ShowCount > 1 ){
			Combo.show[Combo.ShowCount - 1].tab = Combo.show[0].tab;
		}
		// タブ未登録なら登録
		if ( GetTabItemIndex(Combo.base[baseindex].hWnd, 0) < 0 ){
			AddTabInfo(0, Combo.base[baseindex].hWnd);
		}
	}
}

void CreateAndInitPane(int baseindex)
{
	CreatePane(baseindex);
//	CheckComboTable(T("@CreateAndInitPane"));
	ShowWindow(Combo.base[baseindex].hWnd, SW_SHOWNORMAL);
	SortComboWindows(SORTWIN_LAYOUTPAIN);
//	CheckComboTable(T("CreateAndInitPane2"));
}

// ペインを削除 --------------------------------------------
HWND DeletePane(int showindex)
{
	HWND hNewFocusWnd = NULL;

	CheckComboTable(T("DeletePane-pre"));
	if ( Combo.ShowCount <= 1 ){ // 最後の１つ
		hComboFocus = NULL;

		// 削除するペインにフォーカスが当たっている場合
	}else if ( Combo.base[Combo.show[showindex].baseNo].hWnd == hComboFocus ){
		hNewFocusWnd = Combo.base[Combo.show[(showindex == 0) ? 1 : 0].baseNo].hWnd;
		if ( hNewFocusWnd == hComboRightFocus ) hComboRightFocus = NULL;
	}

	if ( Combo.base[Combo.show[showindex].baseNo].hWnd == hComboRightFocus ){
		hComboRightFocus = NULL;
	}
	if ( (showindex == 0) && (Combo.ShowCount > 1) && (Combo.base[Combo.show[1].baseNo].hWnd == hComboRightFocus) ){
		hComboRightFocus = NULL;
	}
									// 該当タブを消す
	if ( Combo.Tabs > 1 ){
		HWND hTabWnd;

		hTabWnd = Combo.show[showindex].tab.hWnd;
		Combo.Tabs--;
		DestroyWindow(hTabWnd);
		InvalidateRect(Combo.hWnd, NULL, TRUE);
	}

									// 表示テーブルから削除
	if ( (showindex + 1) < Combo.ShowCount ){
		CutTableItem(Combo.show, showindex, Combo.ShowCount);
	}
	Combo.ShowCount--;

	if ( Combo.ShowCount > 0 ){
		if ( Combo.ShowCount == 1 ){ // １窓になったので反対窓を再設定
			ChangeReason = T("DeletePane@@1");
			if ( Combo.BaseCount <= 1 ){ // 窓が１つのみ
				hComboRightFocus = NULL;
			}else{
				if ( hNewFocusWnd == NULL ){
					hComboRightFocus = Combo.base[Combo.show[0].baseNo ? 0 : 1].hWnd;
				}else{
					hComboRightFocus = Combo.base[0].hWnd;
					if ( hNewFocusWnd == hComboRightFocus ){
						hComboRightFocus = Combo.base[1].hWnd;
					}
				}
			}
		}else if ( hComboRightFocus == NULL ){// 反対窓が消えたので反対窓を再設定
			ChangeReason = T("DeletePane@@2");
			hComboRightFocus = Combo.base[Combo.show[1].baseNo].hWnd;
		}
		CheckComboTable(T("@DeletePane"));
	}
	if ( hComboRightFocus == NULL ){
		ChangeReason = T("DeletePane51");
	}
	return hNewFocusWnd;
}

//============================================================================
// 現在ペイン・indexなどの情報を求める関数群
//============================================================================
// 指定のタブの座標から PPC_APPINFO を求める(D&D用)
PPC_APPINFO *GetComboTarget(HWND hTargetWnd, POINT *pos)
{
	int baseindex, tabpane;
	HWND hWnd;

	hWnd = hTargetWnd;
	for (;;){
		if ( hWnd == NULL ) break;
		if ( hWnd == Combo.hWnd ) break;

		// Combo でないのに Caption →別用途のダイアログか Capture window
		if ( GetWindowLongPtr(hWnd, GWL_STYLE) & WS_CAPTION ) return NULL;
		hWnd = GetParent(hWnd);
	}

	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( hTargetWnd == Combo.show[tabpane].tab.hWnd ){
			TC_HITTESTINFO th;
			int tabindex;

			th.pt = *pos;
			ScreenToClient(Combo.show[tabpane].tab.hWnd, &th.pt);
			tabindex = TabCtrl_HitTest(Combo.show[tabpane].tab.hWnd, &th);
			if ( tabindex >= 0 ){
				TC_ITEM tie;

				tie.mask = TCIF_PARAM;
				if ( IsTrue(TabCtrl_GetItem(Combo.show[tabpane].tab.hWnd, tabindex, &tie)) ){
					baseindex = GetComboBaseIndex((HWND)tie.lParam);
					goto getdata;
				}
				return NULL;
			}
		}
	}
	baseindex = GetComboBaseIndex(hComboFocus);
getdata:
	if ( baseindex < 0 ) return NULL;
	return Combo.base[baseindex].cinfo;
}

// hWnd から ItemNo baseindex を取得
int GetComboBaseIndex(HWND hWnd)
{
	int baseindex;

	for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
		if ( Combo.base[baseindex].hWnd == hWnd ) return baseindex;
	}
	return -1;
}

// hWnd から showindex を取得
int GetComboShowIndex(HWND hWnd)
{
	int showindex;

	for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
		if ( Combo.base[Combo.show[showindex].baseNo].hWnd == hWnd ){
			return showindex;
		}
	}
	return -1;
}

// hWnd から showindex を取得(該当がない場合は 0 を返す)
int GetComboShowIndexDefault(HWND hWnd)
{
	int showindex;

	for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
		if ( Combo.base[Combo.show[showindex].baseNo].hWnd == hWnd ){
			return showindex;
		}
	}
	return (Combo.ShowCount ? 0 : -1);
}

// pos から showindex を取得
int GetComboShowIndexFromPos(POINT *pos)
{
	int showindex;

	if ( !Combo.ShowCount ) return -1;
	if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
		int x, y;

		x = pos->x;
		y = pos->y;
		for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
			if ( (Combo.show[showindex].box.top <= y) &&
				 (Combo.show[showindex].box.bottom > y) &&
				 ((Combo.show[showindex].box.right + splitwide) > x) ){
				break;
			}
		}
	}else{
		int y;

		y = pos->y;
		for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
			if ( (Combo.show[showindex].box.bottom + splitwide) > y ){
				break;
			}
		}
	}
	if ( showindex == Combo.ShowCount ) showindex--;
	return showindex;
}

// hWnd から反対ペインの showindex を取得
int GetPairPaneComboShowIndex(HWND hTargetWnd)
{
	int showindex;

	if ( Combo.ShowCount < 2 ) return 0;

	showindex = GetComboShowIndex(hTargetWnd);
	if ( showindex < 0 ) return 1; // 該当無し…とりあえず左から２番目。

	if ( showindex ){	// 1以降は右窓→左を選択
		return 0;
	}else{
		showindex = GetComboShowIndex(hComboRightFocus);
		return (showindex < 1) ? 1 : showindex;
	}
}

// hWnd から反対ペインの baseindex を取得
int GetPairPaneComboBaseIndex(HWND hTargetWnd)
{
	int baseindex;

	baseindex = GetComboBaseIndex(hTargetWnd);
	if ( baseindex < 0 ) return -1;
	if ( Combo.BaseCount < 2 ) return -1;
	if ( hTargetWnd == hComboFocus ){ // 現在窓→反対窓
		if ( baseindex == Combo.show[0].baseNo ){
			baseindex = GetComboBaseIndex(hComboRightFocus);
		}else{
			baseindex = Combo.show[0].baseNo;
		}
	}else{ // 現在窓以外→現在窓
		baseindex = GetComboBaseIndex(hComboFocus);
	}
	return baseindex;
}

BOOL SetTabTipText(NMHDR *nmh)
{
	int tabpane;

	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( nmh->hwndFrom == Combo.show[tabpane].tab.hTipWnd ){
			TC_ITEM tie;
			HWND hTabWnd;

			hTabWnd = Combo.show[tabpane].tab.hWnd;
														// 表示中タブにある？
			tie.mask = TCIF_PARAM;
			if ( IsTrue(TabCtrl_GetItem(hTabWnd, nmh->idFrom, &tie)) ){
				GetWindowText((HWND)tie.lParam, tiptext, TSIZEOF(tiptext));
				((LPTOOLTIPTEXT)nmh)->lpszText = tiptext;
				((LPTOOLTIPTEXT)nmh)->hinst = NULL;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void SelectChangeTab(NMHDR *nmh)
{
	int tabpane;

	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( nmh->hwndFrom == Combo.show[tabpane].tab.hWnd ){
			int tabindex;
			TC_ITEM tie;

			if ( !(X_combos[0] & CMBS_TABSEPARATE) ){
				int si;

				si = GetComboShowIndex(hComboFocus);
				if ( si >= 0 ) tabpane = si;
			}
														// 表示中タブにある？
			tabindex = TabCtrl_GetCurSel(nmh->hwndFrom);
			tie.mask = TCIF_PARAM;
			if ( IsTrue(TabCtrl_GetItem(nmh->hwndFrom, tabindex, &tie)) ){
				SelectComboWindow(tabpane, (HWND)tie.lParam, TRUE);
			}
			return;
		}
	}
}

HWND GetHwndFromIDCombo(const TCHAR *regid)
{
	int baseindex;
	PPC_APPINFO *combo_cinfo;

	if ( Combo.hWnd == NULL ) return NULL;
	for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
		combo_cinfo = Combo.base[baseindex].cinfo;
		if ( combo_cinfo == NULL ) continue;
		if ( tstricmp(regid, combo_cinfo->RegSubCID) == 0 ){
			return combo_cinfo->info.hWnd;
		}
	}
	return NULL;
}
