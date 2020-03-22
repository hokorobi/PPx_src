/*-----------------------------------------------------------------------------
	Paper Plane cUI			Combo Window ReBar
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include <dbt.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#include "PPCUI.RH"
#pragma hdrstop

#ifndef TBSTYLE_CUSTOMERASE
#define TBSTYLE_CUSTOMERASE 0x2000
#define CDDS_PREERASE 0x00000003
#define CDRF_SKIPDEFAULT 0x00000004
#endif
#ifndef NM_CUSTOMDRAW
#define NM_CUSTOMDRAW (NM_FIRST-12)
#endif

#define EnableAddressBreadCrumbListBar 0

HBITMAP hAddressBMP = NULL;

const BYTE AllowStr[] = {0x9c, 0x27};

const TCHAR X_dock[] = T("X_dock");

const TCHAR RMENUSTR_LINE[] = MES_LDOS;
const TCHAR RMENUSTR_LOCK[] = MES_LDOL;

const TCHAR RMENUSTR__ADDR[] = MES_LDAD;
const TCHAR RMENUSTR__ADDRBL[] = T("____|breadcrumb"); // MES_LDAD;
const TCHAR RMENUSTR__DRIVES[] = MES_LDDR;
const TCHAR RMENUSTR__INPUT[] = MES_LDIN;
const TCHAR RMENUSTR__STAT[] = MES_LDST;
const TCHAR RMENUSTR__INF[] = MES_LDSI;

#define RMENUSTR_ADDR   (RMENUSTR__ADDR + MESTEXTIDLEN + 1)
#define RMENUSTR_ADDRBL   (RMENUSTR__ADDRBL + MESTEXTIDLEN + 1)
#define RMENUSTR_DRIVES (RMENUSTR__DRIVES + MESTEXTIDLEN + 1)
#define RMENUSTR_STAT   (RMENUSTR__STAT + MESTEXTIDLEN + 1)
#define RMENUSTR_INF    (RMENUSTR__INF + MESTEXTIDLEN + 1)
#define RMENUSTR_INPUT  (RMENUSTR__INPUT + MESTEXTIDLEN + 1)
#define RMENUSTR_INPUT_SIZE (SIZEOFTSTR(RMENUSTR__INPUT) - (MESTEXTIDLEN + 1) * sizeof(TCHAR))
#define RMENUSTR_INPUT_LEN (RMENUSTR_INPUT_SIZE / sizeof(TCHAR))

const TCHAR RMENUSTR_BLANK[] = T("blank");
const TCHAR RMENUSTR_BRANK[] = T("brank");
const TCHAR RMENUSTR_LOG[] = T("log");
const TCHAR RMENUSTR_JOB[] = T("job");
#define RMENU_LOCK	1
#define RMENU_INF	2
#define RMENU_ADDR	3
#define RMENU_DRIVES	4
#define RMENU_INPUT	5
#define RMENU_STAT	6
#define RMENU_LINE	7
#define RMENU_BARS	10
#define RMENU_DEL	1000

const TCHAR DockInfoWinClass[] = T("PPcInfo");
const TCHAR DockStatusWinClass[] = T("PPcStatus");
const TCHAR DockModuleWinClass[] = T("DockModule");
const TCHAR DockAddressWinClass[] = T("Addressbar");
const TCHAR DockAddressBCLWinClass[] = T("AdrBCL");
const TCHAR DockInputWinClass[] = T("Inputbar");
const TCHAR RMPATHSTR[] = T("RemotePath");

const TCHAR DOCKINPUTBARPROP[] = T("PPxDockInput");

typedef struct {
	PPXAPPINFO info;
	PPXDOCK *dock;
} DOCKAPPINFO;

typedef struct {
	DOCKAPPINFO dockinfo;
	HWND hEditWnd;
} REBARADDRESS;

typedef struct {
	DOCKAPPINFO dockinfo; // PPx 共通情報 ※必ず先頭に配置
	HWND hEditWnd;
	TCHAR KeyCustName[64];
	WNDPROC hOldProc;
} REBARINPUT;

typedef struct {
	HMENU hMenu;
	DWORD *index;
	ThSTRUCT *TH;
	TCHAR locate;
} AppendDMenuS;

LRESULT DockStatusMouseButton(HWND hWnd, WORD type);

int DriveBarIconSize = 16;

void LoadAddressBitmap(void)
{
	if ( (hAddressBMP != NULL) || (OSver.dwMajorVersion != 5) ) return;

	hAddressBMP = LoadBitmap(
			GetModuleHandle(StrShell32DLL), MAKEINTRESOURCE(ADDRBMP_RESID));
}

DWORD_PTR USECDECL DockAppInfoProc(DOCKAPPINFO *di, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	if ( di->dock->cinfo != NULL ){
		return di->dock->cinfo->info.Function(
				POINTERCAST(PPXAPPINFO *, &di->dock->cinfo->info), cmdID, uptr);
	}
	if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
	return 0;
}

int GetHiddenMenuItemTypeFromPoint(PPC_APPINFO *cinfo, int bottomY, int height, POINT *pos, int *ItemNo)
{
	int cell, rc, i;
	int x, y;

	if ( cinfo == NULL ) return PPCR_UNKNOWN;
	x = pos->x;
	y = pos->y;
								// 範囲外チェック
	if ( (y < bottomY) || (x < 0) ) return PPCR_UNKNOWN;

									// Info line =================
	if ( x < cinfo->iconR ){	// Info icon
		rc = PPCR_INFOICON;
		cell = cinfo->e.cellN;
	}else{							// Info line / Hidden Menu ========
		x = (x - cinfo->iconR) / cinfo->fontX;
		i = (cinfo->HiddenMenu.item + 1) >> 1;
		if ( (x < (5 * i - 1)) && !(TouchMode & TOUCH_DISABLEHIDDENMENU) ){	// Self Popup Menu
			rc = PPCR_MENU;
			cell = (x / 5) +
					((y - (bottomY + height - cinfo->fontY * 2)) / cinfo->fontY) * i;
			if ( (cell < -1) || (cell >= cinfo->HiddenMenu.item) ){
				return PPCR_INFOTEXT;
			}
		}else{					// Info line
			return PPCR_INFOTEXT;
		}
	}
	if ( ItemNo != NULL ) *ItemNo = cell;
	return rc;
}

// モジュール関連 ===========================================================
LRESULT WmPaintDockModule(HWND hWnd)
{
	HWND hChildWnd = GetWindow(hWnd, GW_CHILD);
	int length;
	TCHAR textbuf[CMDLINESIZE];
	PAINTSTRUCT ps;

	if ( hChildWnd == NULL ){ // モジュールなし→デフォルトで
		return DefWindowProc(hWnd, WM_PAINT, 0, 0);
	}

	length = GetWindowTextLength(hWnd);
	if ( length == 0 ){ // モジュールが自前表示
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		InvalidateRect(hChildWnd, NULL, FALSE);
		return 0;
	}
					// 簡易表示
	BeginPaint(hWnd, &ps);
	GetWindowText(hWnd, textbuf, TSIZEOF(textbuf));
	SetTextColor(ps.hdc, C_info);
	SetBkColor(ps.hdc, C_back);
	TextOut(ps.hdc, 0, 0, textbuf, length);
	EndPaint(hWnd, &ps);
	return 0;
}

LRESULT CallSendParent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPXDOCK *dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if ( dock->cinfo != NULL ){
		SendMessage(dock->cinfo->info.hWnd, message, wParam, lParam);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK DockModuleProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message){
		case WM_CREATE:
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
			return 0;

		case WM_RBUTTONUP:
			DockModifyMenu(NULL, (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA), NULL);
			break;

		case WM_SIZE: {
			HWND hChildWnd = GetWindow(hWnd, GW_CHILD);

			if ( hChildWnd != NULL ){
				SetWindowPos(hChildWnd, 0, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
			}
			break;
		}
		case WM_PAINT:
			return WmPaintDockModule(hWnd);

		case WM_SETTEXT: {
			TCHAR *msg = (TCHAR *)lParam;

			if ( msg[0] == '!' ){ // コマンド実行
				PPXDOCK *dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

				if ( dock->cinfo != NULL ){
					PP_ExtractMacro(dock->cinfo->info.hWnd,
							&dock->cinfo->info, NULL, msg + 1, NULL, 0);
				}
				break;
			}else{
				InvalidateRect(hWnd, NULL, FALSE);
				return DefWindowProc(hWnd, WM_SETTEXT, 0, lParam);
			}
		}

		case WM_COPYDATA:
			return CallSendParent(hWnd, message, wParam, lParam);

		// キー入力(親へ中継する)
		case WM_CHAR:
		case WM_KEYDOWN: {
			PPXDOCK *dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			if ( dock->cinfo != NULL ){
				PostMessage(dock->cinfo->info.hWnd, message, wParam, lParam);
				return 0;
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		default:
			if ( message == WM_PPXCOMMAND ){
				return CallSendParent(hWnd, message, wParam, lParam);
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
// 情報行関連 =================================================================

LRESULT DockInfoMouseButton(HWND hWnd, LPARAM lParam, WORD type)
{
	POINT pos;
	PPXDOCK *dock;
	RECT box;
	int r, n;

	dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( (dock->cinfo == NULL) || IsBadReadPtr(dock->cinfo, 16) ) return 0;
	LPARAMtoPOINT(pos, lParam);
	GetClientRect(hWnd, &box);

	r = GetHiddenMenuItemTypeFromPoint(dock->cinfo, 0, box.bottom, &pos, &n);
	if ( r == PPCR_MENU ) dock->cinfo->DownMPos = n;
	PostMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND,
			KC_MOUSECMD + (type << 16), r + (n << 16));
	return 0;
}

#pragma argsused
VOID CALLBACK DockInfoMouseMoveTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	POINT pos;
	PPXDOCK *dock;
	UnUsedParam(uMsg);UnUsedParam(dwTime);

	GetCursorPos(&pos);
	if ( WindowFromPoint(pos) != hWnd ){	// 画面外なら隠しメニューを消去
		KillTimer(hWnd, idEvent);

		dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if ( (dock->cinfo == NULL) || IsBadReadPtr(dock->cinfo, 16) ) return;
		dock->cinfo->Mpos = -1;
		InvalidateRect(hWnd, NULL, FALSE);
	}
}

void DockInfoMouseMove(HWND hWnd, LPARAM lParam)
{
	int r, n;
	POINT pos;
	RECT box;
	PPXDOCK *dock;

	dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( (dock->cinfo == NULL) || IsBadReadPtr(dock->cinfo, 16) ) return;
	LPARAMtoPOINT(pos, lParam);
	GetClientRect(hWnd, &box);

	r = GetHiddenMenuItemTypeFromPoint(dock->cinfo, 0, box.bottom, &pos, &n);
	if ( r != PPCR_MENU ) n = -1;
	if ( n != dock->cinfo->Mpos ){
		dock->cinfo->Mpos = n;
		box.left = dock->cinfo->iconR;
		box.top = 0;
		box.right = ((dock->cinfo->HiddenMenu.item + 1) >> 1) * 5 *
											dock->cinfo->fontX + box.left;
		InvalidateRect(hWnd, &box, FALSE);
		if ( n >= 0 ){
			SetTimer(hWnd, TIMERID_INFODOCK_MMOVE, TIMER_COMBOHIDEMENU, DockInfoMouseMoveTimerProc);
		}
	}
}

void DockPaintLine(HWND hWnd, void (* PaintFunction)(PPC_APPINFO *cinfo, PAINTSTRUCT *ps, RECT *BoxStatus, ENTRYINDEX EI_No))
{
	PAINTSTRUCT ps;
	HBRUSH hBr;
	PPXDOCK *dock;

	BeginPaint(hWnd, &ps);

	dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( dock->cinfo == NULL ){
		hBr = CreateSolidBrush(C_back);
		FillBox(ps.hdc, &ps.rcPaint, hBr);
		DeleteObject(hBr);
	}else{
		if ( !IsBadReadPtr(dock->cinfo, 16) && !TinyCheckCellEdit(dock->cinfo) ){
			HGDIOBJ hOldFont;
			RECT box;

#ifndef USEDELAYCURSOR
			if ( dock->cinfo->bg.X_WallpaperType )
#endif
			{
				hBr = CreateSolidBrush(C_back);
				FillBox(ps.hdc, &ps.rcPaint, hBr);
				DeleteObject(hBr);
			}
			GetClientRect(hWnd, &box);
			hOldFont = SelectObject(ps.hdc, dock->font.h);
			SetTextAlign(ps.hdc, TA_LEFT | TA_TOP | TA_UPDATECP);

			PaintFunction(dock->cinfo, &ps, &box, DISPENTRY_NO_OUTPANE);
			SetTextAlign(ps.hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
			SelectObject(ps.hdc, hOldFont);
		}
	}
	EndPaint(hWnd, &ps);
}

LRESULT CALLBACK DockInfoProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPXDOCK *dock;

	switch (message){
		case WM_CREATE:
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
			return 0;

		case WM_DESTROY:
			dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			dock->hInfoWnd = NULL;
			break;

		case WM_LBUTTONUP:		return DockInfoMouseButton(hWnd, lParam, 'L');
		case WM_LBUTTONDBLCLK:	return DockInfoMouseButton(hWnd, lParam, 'L'+('D'<<8));
		case WM_RBUTTONUP:
			DockModifyMenu(NULL, (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA), NULL);
			break;

		case WM_RBUTTONDBLCLK:	return DockInfoMouseButton(hWnd, lParam, 'R'+('D'<<8));
		case WM_MBUTTONUP:		return DockInfoMouseButton(hWnd, lParam, 'M');
		case WM_MBUTTONDBLCLK:	return DockInfoMouseButton(hWnd, lParam, 'M'+('D'<<8));
		case WM_XBUTTONUP:		return DockInfoMouseButton(hWnd, lParam, 'X');
		case WM_XBUTTONDBLCLK:	return DockInfoMouseButton(hWnd, lParam, 'X'+('D'<<8));

		case WM_MOUSEMOVE:
			DockInfoMouseMove(hWnd, lParam);
			break;

		case WM_PAINT:
			DockPaintLine(hWnd, PaintInfoLine);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
// ステータス行関連 ===========================================================
LRESULT DockStatusMouseButton(HWND hWnd, WORD type)
{
	PPXDOCK *dock;

	dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( (dock->cinfo == NULL) || IsBadReadPtr(dock->cinfo, 16) ) return 0;

	PostMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND,
			KC_MOUSECMD + (type << 16), PPCR_STATUS);
	return 0;
}

LRESULT CALLBACK DockStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPXDOCK *dock;

	switch (message){
		case WM_CREATE:
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
			return 0;

		case WM_DESTROY:
			dock = (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			dock->hStatusWnd = NULL;
			break;

		case WM_LBUTTONUP:		return DockStatusMouseButton(hWnd, 'L');
		case WM_LBUTTONDBLCLK:	return DockStatusMouseButton(hWnd, 'L'+('D'<<8));
		case WM_RBUTTONDBLCLK:	return DockStatusMouseButton(hWnd, 'R'+('D'<<8));
		case WM_MBUTTONUP:		return DockStatusMouseButton(hWnd, 'M');
		case WM_MBUTTONDBLCLK:	return DockStatusMouseButton(hWnd, 'M'+('D'<<8));
		case WM_XBUTTONUP:		return DockStatusMouseButton(hWnd, 'X');
		case WM_XBUTTONDBLCLK:	return DockStatusMouseButton(hWnd, 'X'+('D'<<8));

		case WM_RBUTTONUP:
			DockModifyMenu(NULL, (PPXDOCK *)GetWindowLongPtr(hWnd, GWLP_USERDATA), NULL);
			break;

		case WM_PAINT:
			DockPaintLine(hWnd, PaintStatusLine);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
// アドレスバー関連 ===========================================================
void InitEditColor(void)
{
	GetCustData(T("CC_log"), &CC_log, sizeof(CC_log));
	if ( (CC_log[0] == C_AUTO) && (CC_log[1] == C_AUTO) ){
		Reportcolor = FALSE;
	}else{
		Reportcolor = TRUE;
		if ( CC_log[0] == C_AUTO) CC_log[0] = GetSysColor(COLOR_WINDOWTEXT);
		if ( CC_log[1] == C_AUTO) CC_log[1] = GetSysColor(COLOR_WINDOW);

		if ( Combo.Report.hBrush != NULL ) DeleteObject(Combo.Report.hBrush);
		Combo.Report.hBrush = CreateSolidBrush(CC_log[1]);
	}
}

void EnterRebarAddress(REBARADDRESS *ra)
{
	TCHAR cmdline[CMDLINESIZE];

	cmdline[0] = '\0';
	SendMessage(ra->hEditWnd, WM_GETTEXT, CMDLINESIZE, (LPARAM)cmdline);

	if ( ra->dockinfo.dock->cinfo == NULL ) return;

	SendMessage(ra->dockinfo.dock->cinfo->info.hWnd, WM_PPXCOMMAND, KCW_enteraddress, (LPARAM)cmdline);
	SetFocus(ra->dockinfo.dock->cinfo->info.hWnd);
}

void DrawAddressButton(HDC hDC, RECT *box)
{
	HBRUSH hBr;

	hBr = CreateSolidBrush(C_DGREEN);
	FillBox(hDC, box, hBr);
	DeleteObject(hBr);

	if ( hAddressBMP != NULL ){ // WinXP
		HDC hMemDC = CreateCompatibleDC(hDC);
		HGDIOBJ hOldBMP;

		hOldBMP = SelectObject(hMemDC, hAddressBMP);
		BitBlt(hDC,
			box->right - ((box->bottom - box->top) / 2) - (ADDRBMP_SIZE / 2),
			box->top + ((box->bottom - box->top) / 2) - (ADDRBMP_SIZE / 2),
			ADDRBMP_SIZE, ADDRBMP_SIZE, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBMP);
		DeleteDC(hMemDC);
	}else if ( OSver.dwMajorVersion >= 5 ){ // 矢印表示
		COLORREF oldc;
		HGDIOBJ hOldFont;

		oldc = SetTextColor(hDC, C_WHITE);
		SetBkMode(hDC, TRANSPARENT);
		if ( Combo.Font.size.cy > ADDRBMP_SIZE ) hOldFont = SelectObject(hDC, Combo.Font.handle);
		TextOutW(hDC, box->left, box->top, (const WCHAR *)&AllowStr, 1);
		#pragma warning(suppress: 4701) // 同条件の２行前で初期化
		if ( Combo.Font.size.cy > ADDRBMP_SIZE ) SelectObject(hDC, hOldFont);
		SetBkMode(hDC, OPAQUE);
		SetTextColor(hDC, oldc);
	}
}

int GetBarHeight(PPXDOCK *dock)
{
	int height, dpi;

	height = dock->font.Y + 6;
	dpi = PPxCommonExtCommand(K_GETDISPDPI, (WPARAM)dock->hWnd);
	if ( dpi != DEFAULT_WIN_DPI ) height = (height * dpi) / DEFAULT_WIN_DPI;
	return height;
}

LRESULT CALLBACK DockAddressBarProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	REBARADDRESS *ra;

	switch (message){
		case WM_CREATE: {
			ra = PPcHeapAlloc(sizeof(REBARADDRESS));
			if ( ra == NULL ) return -1;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ra);

			InitEditColor();
			ra->dockinfo.info.hWnd = hWnd;
			ra->dockinfo.info.Function = (PPXAPPINFOFUNCTION)DockAppInfoProc;
			ra->dockinfo.info.Name = T("Address");
			ra->dockinfo.info.RegID = NilStr;
			ra->dockinfo.info.hWnd = hWnd;

			ra->dockinfo.dock = (PPXDOCK *)((CREATESTRUCT *)lParam)->lpCreateParams;
			ra->dockinfo.dock->hAddrWnd = ra->hEditWnd =
				CreateWindowEx(WS_EX_CLIENTEDGE, T("EDIT"), NilStr,
				WS_CHILD | WS_VSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_LEFT,
				0, 0, 100, GetBarHeight(ra->dockinfo.dock), hWnd, NULL, hInst, 0);
												// EditBox の拡張 -------------
			SendMessage(ra->hEditWnd, WM_SETFONT, (WPARAM)ra->dockinfo.dock->font.h, 0);
			PPxRegistExEdit(&ra->dockinfo.info, ra->hEditWnd, CMDLINESIZE,
				NULL, PPXH_DIR_R | PPXH_COMMAND, PPXH_DIR_R,
				PPXEDIT_USEALT | PPXEDIT_WANTENTER | PPXEDIT_WANTEVENT | PPXEDIT_TABCOMP);
			ShowWindow(ra->hEditWnd, SW_SHOWNORMAL);
			return 0;
		}
		case WM_DESTROY:
			ra = (REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			ra->dockinfo.dock->hAddrWnd = NULL;
			PPcHeapFree(ra);
			break;

		case WM_SIZE:
			ra = (REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			SetWindowPos(ra->hEditWnd, 0, 0, 0,
					LOWORD(lParam) - HIWORD(lParam), HIWORD(lParam),
					SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
			break;

		case WM_COMMAND:
			ra = (REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			if ( (HWND)lParam == ra->hEditWnd ){
				if ( HIWORD(wParam) == 13 ) EnterRebarAddress(ra);
				if ( HIWORD(wParam) == 27 ){
					if ( ra->dockinfo.dock->cinfo != NULL ){
						SetFocus(ra->dockinfo.dock->cinfo->info.hWnd);
					}
				}
			}
			break;

		case WM_LBUTTONUP:
			EnterRebarAddress((REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA));
			break;

		case WM_RBUTTONUP:
			DockModifyMenu(NULL, ((REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA))->dockinfo.dock, NULL);
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			RECT box;

			BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &box);
			box.left = box.right - box.bottom;
			DrawAddressButton(ps.hdc, &box);
			EndPaint(hWnd, &ps);
			break;
		}

		case WM_CTLCOLOREDIT:
			if ( Reportcolor ){
				ra = (REBARADDRESS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
				if ( (HWND)lParam == ra->hEditWnd ){
					SetTextColor((HDC)wParam, CC_log[0]);
					SetBkColor((HDC)wParam, CC_log[1]);
					return (LRESULT)Combo.Report.hBrush;
				}
			}
		// default へ

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

#if EnableAddressBreadCrumbListBar
LRESULT CALLBACK DockAddressBCLBarProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message){
		case WM_SETTEXT:
			if ( tstrlen( (const TCHAR *)lParam ) < VFPS ){
				TCHAR path[VFPS], *pathsep;
				TBBUTTON tb;

				tstrcpy(path, (const TCHAR *)lParam);

				for (;;){
					if ( SendMessage(hWnd, TB_DELETEBUTTON, 0, 0) == FALSE ) break;
				}
#define BTNS_AUTOSIZE 0x0010
#define BTNS_DROPDOWN 0x0008
#define BTNS_WHOLEDROPDOWN 0x0080
#define TB_SETPADDING (WM_USER + 87)
#define TB_SETLISTGAP (WM_USER + 96)
				SendMessage(hWnd, TB_SETLISTGAP, 0, 0);

				tb.iBitmap = -2;
				tb.idCommand = IDW_ADRBCL + 1;
				tb.fsState = TBSTATE_ENABLED;
				tb.fsStyle = BTNS_SHOWTEXT | BTNS_DROPDOWN | BTNS_AUTOSIZE;
				tb.dwData = 0;
// WM_COMMAND / TBN_DROPDOWN
				pathsep = path;
				for ( ;; ){
					TCHAR *nextpathsep;

					if ( *pathsep == '\0' ) break;
					nextpathsep = FindPathSeparator(pathsep);
					if ( nextpathsep != NULL ) *nextpathsep++ = '\0';

					tb.iString = SendMessage(hWnd, TB_ADDSTRING, 0, (LPARAM)pathsep);
					SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&tb);
					if ( nextpathsep == NULL ) break;
					pathsep = nextpathsep;
				}
				SendMessage(hWnd, TB_AUTOSIZE, 0, 0);


			}
			break;
// 大きさ調整
// SendMessage(hWndToolBar, TB_GETMAXSIZE, 0, (LPARAM)&m_sizeToolbar);

		default:
			return CallWindowProc((WNDPROC)(LONG_PTR)
					GetProp(hWnd, DOCKINPUTBARPROP),
					hWnd, message, wParam, lParam);
	}
	return 0;
}
#endif
// ドライブバー関連 ===========================================================
void MakeDriveImage(HDC hMDC, int driveno, TCHAR *drivepath)
{
	HICON driveicon;

	wsprintf(drivepath, T("%c:\\"), driveno + 'A');

	EnterCriticalSection(&SHGetFileInfoSection);
	driveicon = LoadFileIcon(drivepath, FILE_ATTRIBUTE_DIRECTORY, SHGFI_ICON, DriveBarIconSize, NULL);
	LeaveCriticalSection(&SHGetFileInfoSection);
	if ( driveicon != NULL ){
		DrawIconEx(hMDC, driveno * DriveBarIconSize, 0, driveicon, DriveBarIconSize, DriveBarIconSize, 0, NULL, DI_NORMAL);
		DestroyIcon(driveicon);
	}
}

void ListDrives(HWND hToolBarWnd)
{
	TBBUTTON tb;
	int driveno;
	DWORD drive;
	TCHAR buf[VFPS], rpath[VFPS];
	TBADDBITMAP tbab;

	HDC hDC, hMDC;
	HBITMAP hBmp;
	HGDIOBJ hOldBmp;

	hDC = GetDC(hToolBarWnd);
	hMDC = CreateCompatibleDC(hDC);

	hBmp = CreateCompatibleBitmap(hDC, DriveBarIconSize * 26, DriveBarIconSize);
	hOldBmp = SelectObject(hMDC, hBmp);
	SetProp(hToolBarWnd, RMENUSTR_DRIVES, hBmp);

	drive = GetLogicalDrives();
	for ( driveno = 0 ; driveno < 26 ; driveno++ ){
		wsprintf(buf, T("Network\\%c"), (TCHAR)(driveno + 'A'));
		rpath[0] = '\0';
		GetRegString(HKEY_CURRENT_USER, buf, RMPATHSTR, rpath, TSIZEOF(rpath));

		if ( (drive & (B0 << driveno)) || rpath[0] ){
			tb.fsState = TBSTATE_ENABLED;
		}else{
			tb.fsState = TBSTATE_ENABLED | TBSTATE_HIDDEN;
		}
		tb.iBitmap = driveno;
		tb.idCommand = IDW_DRIVES + driveno;
		tb.fsStyle = BTNS_SHOWTEXT;
		tb.dwData = 0;

		if ( tb.fsState & TBSTATE_HIDDEN ){
			buf[0] = (TCHAR)(driveno + 'A');
			buf[1] = '*';
			buf[2] = '\0';
			buf[3] = '\0';
		}else{
			MakeDriveImage(hMDC, driveno, buf);
			buf[1] = '\0';
			buf[2] = '\0';
		}

		tb.iString = SendMessage(hToolBarWnd, TB_ADDSTRING, 0, (LPARAM)buf);
		SendMessage(hToolBarWnd, TB_ADDBUTTONS, 1, (LPARAM)&tb);
	}

	SelectObject(hMDC, hOldBmp);
	DeleteDC(hMDC);
	ReleaseDC(hToolBarWnd, hDC);

	tbab.hInst = NULL;
	tbab.nID   = (UINT_PTR)hBmp;
	SendMessage(hToolBarWnd, TB_ADDBITMAP, 26, (LPARAM)&tbab);
}

BOOL GetDriveBarPath(int id, TCHAR *path)
{
	if ( (id < IDW_DRIVES) || (id >= (IDW_DRIVES + 32)) ) return FALSE;

	wsprintf(path, T("%c:\\"), id - IDW_DRIVES + 'A');
	return TRUE;
}

void PushDriveButton(PPXDOCKS *docks, int id)
{
	TCHAR buf[32];

	if ( docks->t.cinfo == NULL ) return;
	wsprintf(buf, T("%c%%j"), EXTCMD_CMD);
	if ( FALSE != GetDriveBarPath(id, buf + 3) ){
		SendMessage(docks->t.cinfo->info.hWnd, WM_PPCEXEC, (WPARAM)buf, TMAKELPARAM(0, 0));
	}
}

void DriveBarNotify(PPXDOCK *dock, NMHDR *nmh)
{
	TCHAR buf[32];

	switch (nmh->code){
		case NM_RCLICK: {
			if ( FALSE != GetDriveBarPath(((LPNMTOOLBAR)nmh)->iItem, buf) ){
				POINT pos;

				GetMessagePosPoint(pos);
				VFSSHContextMenu(nmh->hwndFrom, &pos, NULL, buf, NULL);
			}else{
				DockModifyMenu(NULL, dock, NULL);
			}
			break;
		}
	}
}

// 入力バー関連 ===========================================================
LRESULT CALLBACK DockInputBarEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	REBARINPUT *ri;

	ri = (REBARINPUT *)GetProp(hWnd, DOCKINPUTBARPROP);
	switch (message){
		case WM_SETFOCUS:
			if ( ri->dockinfo.dock->cinfo != NULL ){
				ri->dockinfo.dock->cinfo->hActiveWnd = ri->hEditWnd;
			}
			break;

//		default:
	}
	return CallWindowProc(ri->hOldProc, hWnd, message, wParam, lParam);
}

void InitDockInputBarProc(HWND hWnd, REBARINPUT *ri)
{
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ri);

	ri->dockinfo.info.Function = (PPXAPPINFOFUNCTION)DockAppInfoProc;
	ri->dockinfo.info.Name = T("Input");
	ri->dockinfo.info.RegID = NilStr;
	ri->dockinfo.info.hWnd = hWnd;

	InitEditColor();

	ri->hEditWnd =
			CreateWindowEx(WS_EX_CLIENTEDGE, T("EDIT"), NilStr, WS_CHILD |
			WS_VISIBLE | WS_VSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_LEFT,
			0, 0, 100, GetBarHeight(ri->dockinfo.dock), hWnd, NULL, hInst, 0);
												// EditBox の拡張 -------------
	SendMessage(ri->hEditWnd, WM_SETFONT, (WPARAM)ri->dockinfo.dock->font.h, 0);
	PPxRegistExEdit(&ri->dockinfo.info, ri->hEditWnd, CMDLINESIZE,
			NULL, PPXH_DIR_R | PPXH_COMMAND, PPXH_COMMAND,
			PPXEDIT_USEALT | PPXEDIT_WANTENTER | PPXEDIT_TABCOMP); // PPXEDIT_WANTEVENT は手動
												// 更に拡張 -------------
	SetProp(ri->hEditWnd, DOCKINPUTBARPROP, (HANDLE)ri);
	ri->hOldProc = (WNDPROC)SetWindowLongPtr(ri->hEditWnd,
			GWLP_WNDPROC, (LONG_PTR)DockInputBarEditProc);

	if ( ri->KeyCustName[0] != '\0' ){
		SendMessage(ri->hEditWnd, WM_PPXCOMMAND, KE_setkeya, (LPARAM)ri->KeyCustName);
	}
}

void DockInputBarExecute(REBARINPUT *ri)
{
	TCHAR cmdline[CMDLINESIZE];
	PPC_APPINFO *cinfo;
	HWND hWnd;
	PPXAPPINFO *info;

	cmdline[0] = '\0';
	SendMessage(ri->hEditWnd, WM_GETTEXT, CMDLINESIZE, (LPARAM)cmdline);

	cinfo = ri->dockinfo.dock->cinfo;
	if ( cinfo != NULL ){
		hWnd = cinfo->info.hWnd;
		info = &cinfo->info;
	}else{
		hWnd = Combo.hWnd;
		info = NULL;
	}
	PP_ExtractMacro(hWnd, info, NULL, cmdline, NULL, 0);
	SetWindowText(ri->hEditWnd, NilStr);
}

LRESULT CALLBACK DockInputBarProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	REBARINPUT *ri;

	switch (message){
		case WM_CREATE:
			InitDockInputBarProc(hWnd, (REBARINPUT *)((CREATESTRUCT *)lParam)->lpCreateParams);
			return 0;

		case WM_DESTROY:
			ri = (REBARINPUT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			SetWindowLongPtr(ri->hEditWnd, GWLP_WNDPROC, (LONG_PTR)ri->hOldProc);
			PPcHeapFree(ri);
			break;

		case WM_SIZE:
			ri = (REBARINPUT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			SetWindowPos(ri->hEditWnd, 0, 0, 0, LOWORD(lParam), HIWORD(lParam),
					SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
			break;

		case WM_COMMAND:
			ri = (REBARINPUT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			if ( ((HWND)lParam == ri->hEditWnd) &&
				 (ri->dockinfo.dock->cinfo != NULL) ){
				if ( HIWORD(wParam) == VK_ESCAPE ){
					ri->dockinfo.dock->cinfo->hActiveWnd = NULL;
					SetFocus(ri->dockinfo.dock->cinfo->info.hWnd);
				}
				if ( HIWORD(wParam) == VK_RETURN ){
					DockInputBarExecute(ri);
				}
				return 1;
			}
			break;

		case WM_CTLCOLOREDIT:
			if ( Reportcolor ){
				ri = (REBARINPUT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
				if ( (HWND)lParam == ri->hEditWnd ){
					SetTextColor((HDC)wParam, CC_log[0]);
					SetBkColor((HDC)wParam, CC_log[1]);
					return (LRESULT)Combo.Report.hBrush;
				}
			}
		// default へ

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

#define DOCKCUSTSIZE 0x1000
void GetDockList(PPXDOCK *dock, TCHAR *cust)
{
	*cust = '\0';
	GetCustTable( X_dock, dock->cust, cust, DOCKCUSTSIZE );
	if ( cust[0] == '\0' ){	// CA_T 等がないときは、C_T を取得
		TCHAR tmp[16];
		const TCHAR *p;

		p = tstrchr(dock->cust, '_');
		if ( p != NULL ){
			tmp[0] = dock->cust[0];
			tstrcpy(tmp + 1, p);
			GetCustTable( X_dock, tmp, cust, DOCKCUSTSIZE );
		}
	}
}

// ============================================================================
// リバーを保存する
int SaveReBar(PPXDOCK *dock)
{
	ThSTRUCT th;
	TCHAR buf[CMDLINESIZE];
//	TCHAR cust[DOCKCUSTSIZE];
	REBARBANDINFO rbi;
	int i, count;

	count = SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);

	ThInit(&th);
	for ( i = 0 ; i < count ; i++ ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_STYLE | RBBIM_SIZE | RBBIM_ID;
		rbi.wID = NOSETWID;

		SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
		if ( rbi.wID != NOSETWID ){
			TCHAR *bar;

			bar = ThPointerT(dock->th, rbi.wID);
			if ( *bar != '%' ){
				wsprintf(buf, T("%s,%d,%d\n"), bar, rbi.cx, rbi.fStyle);
				ThCatString(&th, buf);
			}
		}
	}
/*
	GetDockList(dock, cust);
	if ( cust[0] ){
		TCHAR *linep, *lp;

		linep = cust;
		while ( *linep != '\0' ){
			lp = tstrchr(linep, '\n'); // 行末尾
			if ( *linep == '!' ){
				if ( lp != NULL ) *lp = '\0';
				ThCatString(&th, buf);
				ThCatString(&th, T("\n"));
			}
			if ( lp == NULL ) break;
			linep = lp + 1;
		}
	}
*/
	if ( th.top ){
		*((TCHAR *)ThLast(&th) - 1) = '\0';
		SetCustTable( X_dock, dock->cust, th.bottom, th.top);
	}else{
		DeleteCustTable( X_dock, dock->cust, 0);
	}
	ThFree(&th);
	return count;
}

void ModifyReBar(PPXDOCK *dock)
{
	if ( SaveReBar(dock) <= 0 ){
		DestroyWindow(dock->hWnd);
		dock->hWnd = NULL;
		dock->client.bottom = 0;
	}
	if ( dock->cinfo != NULL ){
		WmWindowPosChanged(dock->cinfo); // WM_WINDOWPOSCHANGED 相当
		if ( dock->cinfo->combo ){
			SendMessage(dock->cinfo->hComboWnd, WM_PPXCOMMAND, KCW_layout, 0);
		}
	}
}

// 各コントロールの作成 =======================================================
BOOL CreateDockModuleBar(PPXDOCK *dock, UINT style, int cx, const TCHAR *exename)
{
	REBARBANDINFO rbi;
	WNDCLASS wcClass;
	HWND hWnd;
	TCHAR cmdline[CMDLINESIZE];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	RECT box;
	const TCHAR *ep;
	POINT minsize = {64, 16};
	BOOL once = FALSE;
									// オプション解析
	ep = exename;
	while ( SkipSpace(&ep) == '/' ){
		const TCHAR *more;

		GetOptionParameter(&ep, cmdline, CONSTCAST(TCHAR **, &more));

		if ( !tstrcmp(cmdline + 1, T("SIZE")) ){
			if ( Isdigit(*more) ) minsize.x = GetNumber(&more);
			if ( *more == '*'){
				more++;
				minsize.y = GetNumber(&more);
			}
			continue;
		}
		if ( !tstrcmp(cmdline + 1, T("ONCE")) ){
			once = TRUE;
			continue;
		}
		XMessage(NULL, NULL, XM_NiERRld, T("Unknown option %s"), cmdline);
	}

	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= DockModuleProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= DockModuleWinClass;
	RegisterClass(&wcClass);

	hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			DockModuleWinClass, NULL, WS_CHILD | WS_VISIBLE, 0, 0,
			minsize.x, minsize.y, dock->hWnd, CHILDWNDID(IDW_DOCKMODULE), hInst, dock);
	ShowWindow(hWnd, SW_SHOW);

	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;

	if ( tstrchr(ep, '\"') ){
		wsprintf(cmdline, T("%s /P%u"), ep, hWnd);
	}else{
		wsprintf(cmdline, T("\"%s\" /P%u"), ep, hWnd);
	}

	if ( IsTrue(CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
				CREATE_DEFAULT_ERROR_MODE, NULL, PPcPath, &si, &pi)) ){
		WaitForInputIdle(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}else{
		DestroyWindow(hWnd);
		PPErrorBox(dock->hWnd, ep, PPERROR_GETLASTERROR);
		return FALSE;
	}
	GetWindowRect(hWnd, &box);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.wID = dock->th->top;
	if ( IsTrue(once) ) ThAppend(dock->th, T("%"), TSTROFF(1));
	ThAddString(dock->th, exename);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cxMinChild = box.right - box.left;
	rbi.cyMinChild = box.bottom - box.top;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
	SetWindowLongPtr(hWnd, GWLP_ID, SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0) - 1); // リバーのインデックスを記憶する
	return TRUE;
}

void CreateDockBlankBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_BLANK);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = NULL;
	rbi.cxMinChild = dock->font.X;
	rbi.cyMinChild = dock->font.Y;
	rbi.cx = cx;

	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

void CreateDockToolBar(PPXDOCK *dock, const TCHAR *custname, UINT style, int cx)
{
	REBARBANDINFO rbi;
	HWND hToolBarWnd;
	RECT box;

	if ( UseCCDrawBack == 0 ){
		UseCCDrawBack = PPxCommonExtCommand(K_DRAWCCBACK, 0) ? 2 : 1;
	}

	rbi.wID = dock->th->top;
	ThAddString(dock->th, custname);
	hToolBarWnd = CreateToolBar(dock->th, dock->hWnd, &dock->IDmax, custname,
		PPcPath,
		((UseCCDrawBack > 1) ? TBSTYLE_CUSTOMERASE : 0) | CCS_NODIVIDER );
	if ( hToolBarWnd == NULL ) return;

	SetWindowLongPtr(hToolBarWnd, GWL_STYLE, GetWindowLongPtr(hToolBarWnd, GWL_STYLE) | CCS_NORESIZE);
	GetWindowRect(hToolBarWnd, &box);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hToolBarWnd;
	rbi.cxMinChild = box.right - box.left;
	rbi.cyMinChild = box.bottom - box.top;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

void CreateDockPPcInfoBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	WNDCLASS wcClass;
	HWND hInfoWnd;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_INF);

	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= DockInfoProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= DockInfoWinClass;
	RegisterClass(&wcClass);

	dock->hInfoWnd = hInfoWnd =
		CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			DockInfoWinClass, NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 100, 10, dock->hWnd, CHILDWNDID(IDW_DOCKPPCINFO), hInst, dock);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hInfoWnd;
	rbi.cxMinChild = dock->font.Y * 2;
	rbi.cyMinChild = dock->font.Y * 2;

	if ( dock->cinfo != NULL ){
		rbi.cyMinChild = dock->font.Y * (dock->cinfo->inf1.height + dock->cinfo->inf2.height);
	}

	rbi.cx = cx;
	ShowWindow(hInfoWnd, SW_SHOW);
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

void CreateDockPPcStatusBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	WNDCLASS wcClass;
	HWND hWnd;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_STAT);

	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= DockStatusProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= DockStatusWinClass;
	RegisterClass(&wcClass);

	dock->hStatusWnd = hWnd =
		CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			DockStatusWinClass, NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 100, 10, dock->hWnd, CHILDWNDID(IDW_DOCKPPCSTATUS), hInst, dock);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cxMinChild = dock->font.Y * 2;
	rbi.cyMinChild = dock->font.Y;
	rbi.cx = cx;

	if ( dock->cinfo != NULL ) rbi.cyMinChild *= dock->cinfo->stat.height;

	ShowWindow(hWnd, SW_SHOW);
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

void CreateDockLogBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	HWND hWnd;

	if ( (hCommonLog != NULL) && IsTrue(IsWindow(hCommonLog)) ){
		return;
	}
	if ( dock->cinfo == NULL ) return;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_LOG);

	hCommonLog = hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			T("EDIT"), NilStr,
			WS_CHILD | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL |
			ES_LEFT | ES_MULTILINE | ES_WANTRETURN,	// ウインドウの形式
			0, 0, 100, 200, dock->hWnd, CHILDWNDID(IDW_REPORTLOG), hInst, 0);
												// EditBox の拡張 -------------
	SendMessage(hWnd, WM_SETFONT, (WPARAM)dock->font.h, 0);
	PPxRegistExEdit(NULL, hWnd, 0x100000, NULL, 0, 0, PPXEDIT_USEALT | PPXEDIT_TABCOMP | PPXEDIT_NOWORDBREAK);
	ShowWindow(hWnd, SW_SHOWNORMAL);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cxMinChild = dock->font.Y * 20;
	rbi.cyMinChild = dock->font.Y * 10;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
	PPxCommonExtCommand(K_SETLOGWINDOW, (WPARAM)GetParent(dock->hWnd));
}

BOOL CreateDockJobBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	HWND hWnd;

	PPxCommonCommand(dock->hWnd, (LPARAM)&hWnd, K_GETJOBWINDOW);
	if ( hWnd == NULL ) return FALSE;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_JOB);
	ShowWindow(hWnd, SW_SHOWNORMAL);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cxMinChild = dock->font.Y * 10;
	rbi.cyMinChild = dock->font.Y * 10;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
	return TRUE;
}

void CreateDockAddressBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	WNDCLASS wcClass;
	HWND hWnd;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_ADDR);

	LoadAddressBitmap();
	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= DockAddressBarProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= DockAddressWinClass;
	RegisterClass(&wcClass);

	rbi.cxMinChild = rbi.cyMinChild = GetBarHeight(dock);

	hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			DockAddressWinClass, NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 100, rbi.cxMinChild, dock->hWnd, CHILDWNDID(IDW_ADDRESS), hInst, dock);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cx = cx;
	ShowWindow(hWnd, SW_SHOW);
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}
#if EnableAddressBreadCrumbListBar
void CreateDockAddressBreadCrumbListBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	int iconsize;
	UINT tstyle;
	HWND hToolBarWnd;
	RECT box;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_ADDRBL);

	LoadCommonControls(ICC_BAR_CLASSES);

	iconsize = GetSystemMetrics(SM_CXICON) / 2;
	{
		UINT dpi = PPxCommonExtCommand(K_GETDISPDPI, (WPARAM)dock->hWnd);
		if ( dpi != DEFAULT_WIN_DPI ){
			iconsize = (iconsize * (int)dpi) / DEFAULT_WIN_DPI;
		}
	}
#define TBSTYLE_EX_HIDECLIPPEDBUTTONS 0x10
	// 編集可能にする : CCS_ADJUSTABLE | TBSTYLE_ALTDRAG
	tstyle = CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_AUTOSIZE |
		TBSTYLE_FLAT | TBSTYLE_LIST | WS_CHILD | WS_VISIBLE;
	hToolBarWnd = CreateWindowEx(TBSTYLE_EX_HIDECLIPPEDBUTTONS,
		TOOLBARCLASSNAME, NULL, tstyle, 0, 0, iconsize, iconsize,
		dock->hWnd, (HMENU)IDW_ADRBCL, hInst, NULL);
	if ( hToolBarWnd == NULL ) return;

	dock->hAddrWnd = hToolBarWnd;

	if ( X_dss & DSS_COMCTRL ) SendMessage(hToolBarWnd, CCM_DPISCALE, TRUE, 0);

	SendMessage(hToolBarWnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

	SendMessage(hToolBarWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
	SendMessage(hToolBarWnd, TB_SETBITMAPSIZE, 0, TMAKELPARAM(iconsize, iconsize));

	SendMessage(hToolBarWnd, TB_AUTOSIZE, 0, 0);
	SendMessage(hToolBarWnd, TB_SETPADDING, 0, 0);
	SendMessage(hToolBarWnd, TB_SETLISTGAP, 0, 0);
	SetWindowLongPtr(hToolBarWnd, GWL_STYLE, GetWindowLongPtr(hToolBarWnd, GWL_STYLE) | CCS_NORESIZE);

	SetProp(hToolBarWnd, DOCKINPUTBARPROP, (HANDLE)GetWindowLongPtr(hToolBarWnd, GWLP_WNDPROC));
	SetWindowLongPtr(hToolBarWnd, GWLP_WNDPROC, (LONG_PTR)DockAddressBCLBarProc);

	GetWindowRect(hToolBarWnd, &box);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style | RBBS_USECHEVRON;
	rbi.hwndChild = hToolBarWnd;
	rbi.cxMinChild = box.right - box.left;
	rbi.cyMinChild = box.bottom - box.top;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}
#endif
void CreateDockDriveBar(PPXDOCK *dock, UINT style, int cx)
{
	REBARBANDINFO rbi;
	HWND hToolBarWnd;
	RECT box;
	UINT tstyle;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, RMENUSTR_DRIVES);

	LoadCommonControls(ICC_BAR_CLASSES);
	DriveBarIconSize = GetSystemMetrics(SM_CXICON) / 2;
	{
		UINT dpi = PPxCommonExtCommand(K_GETDISPDPI, (WPARAM)dock->hWnd);
		if ( dpi != DEFAULT_WIN_DPI ){
			DriveBarIconSize = (DriveBarIconSize * (int)dpi) / DEFAULT_WIN_DPI;
		}
	}
	// 編集可能にする : CCS_ADJUSTABLE | TBSTYLE_ALTDRAG
	tstyle = CCS_NODIVIDER | WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST;

	if ( UseCCDrawBack == 0 ){
		UseCCDrawBack = PPxCommonExtCommand(K_DRAWCCBACK, 0) ? 2 : 1;
	}
	if ( UseCCDrawBack > 1 ) tstyle |= TBSTYLE_CUSTOMERASE;

	hToolBarWnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, tstyle,
			0, 0, DriveBarIconSize, DriveBarIconSize,
			dock->hWnd, CHILDWNDID(IDW_DRIVEBAR), hInst, NULL);
	if ( hToolBarWnd == NULL ) return;

	if ( X_dss & DSS_COMCTRL ) SendMessage(hToolBarWnd, CCM_DPISCALE, TRUE, 0);

	SendMessage(hToolBarWnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

	SendMessage(hToolBarWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
	SendMessage(hToolBarWnd, TB_SETBITMAPSIZE, 0, TMAKELPARAM(DriveBarIconSize, DriveBarIconSize));

	ListDrives(hToolBarWnd);
	SendMessage(hToolBarWnd, TB_AUTOSIZE, 0, 0);

	SetWindowLongPtr(hToolBarWnd, GWL_STYLE, GetWindowLongPtr(hToolBarWnd, GWL_STYLE) | CCS_NORESIZE);
	GetWindowRect(hToolBarWnd, &box);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hToolBarWnd;
	rbi.cxMinChild = box.right - box.left;
	rbi.cyMinChild = box.bottom - box.top;
	rbi.cx = cx;
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

void CreateDockInputBar(PPXDOCK *dock, UINT style, int cx, const TCHAR *name)
{
	REBARBANDINFO rbi;
	WNDCLASS wcClass;
	HWND hWnd;
	REBARINPUT *ri;

	rbi.wID = dock->th->top;
	ThAddString(dock->th, name);

	LoadAddressBitmap();

	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= DockInputBarProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= DockInputWinClass;
	RegisterClass(&wcClass);

	ri = PPcHeapAlloc(sizeof(REBARINPUT));
	ri->dockinfo.dock = dock;
	name += RMENUSTR_INPUT_LEN;
	GetCommandParameter(&name, ri->KeyCustName, TSIZEOF(ri->KeyCustName));

	rbi.cxMinChild = rbi.cyMinChild = GetBarHeight(dock);

	hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			DockInputWinClass, NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 100, rbi.cxMinChild, dock->hWnd,
			CHILDWNDID(IDW_DOCKINPUT), hInst, ri);

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_ID;
	rbi.fStyle = style;
	rbi.hwndChild = hWnd;
	rbi.cxMinChild = rbi.cyMinChild = GetBarHeight(dock);
	rbi.cx = cx;
	ShowWindow(hWnd, SW_SHOW);
	SendMessage(dock->hWnd, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
	SendMessage(ri->hEditWnd, WM_PPXCOMMAND, K_E_FIRST, 0);
}


BOOL CreateDockBar(PPXDOCK *dock, const TCHAR *name, int cx, UINT style)
{
	if ( *name == 'B' ){
		CreateDockToolBar(dock, name, style, cx);
	}else if ( !tstrcmp(name, RMENUSTR_BLANK) || !tstrcmp(name, RMENUSTR_BRANK)){
		CreateDockBlankBar(dock, style, cx);
	}else if ( !tstrcmp(name, RMENUSTR_STAT) ){
		CreateDockPPcStatusBar(dock, style, cx);
	}else if ( !tstrcmp(name, RMENUSTR_INF) ){
		CreateDockPPcInfoBar(dock, style, cx);
	}else if ( !tstrcmp(name, RMENUSTR_ADDR) ){
		CreateDockAddressBar(dock, style, cx);
#if EnableAddressBreadCrumbListBar
	}else if ( !tstrcmp(name, RMENUSTR_ADDRBL) ){
		CreateDockAddressBreadCrumbListBar(dock, style, cx);
#endif
	}else if ( !tstrcmp(name, RMENUSTR_DRIVES) ){
		CreateDockDriveBar(dock, style, cx);
	}else if ( !memcmp(name, RMENUSTR_INPUT, RMENUSTR_INPUT_SIZE) && ((UTCHAR)*(name + RMENUSTR_INPUT_LEN) <= ' ') ){
		CreateDockInputBar(dock, style, cx, name);
	}else if ( !tstrcmp(name, RMENUSTR_LOG) ){
		CreateDockLogBar(dock, style, cx);
	}else if ( !tstrcmp(name, RMENUSTR_JOB) ){
		return CreateDockJobBar(dock, style, cx);
	}else if ( tstrchr(name, '.') ){
		return CreateDockModuleBar(dock, style, cx, name);
	}else{
		XMessage(NULL, NULL, XM_GrERRld, T("Not found %s bar."), name);
		return FALSE;
	}
	return TRUE;
}

// ============================================================================
void InitDock(HWND hWnd, PPXDOCK *dock, BOOL forcecreate)
{
	REBARINFO rbi;
	TCHAR cust[DOCKCUSTSIZE];

	dock->hWnd = NULL;
	dock->hStatusWnd = NULL;
	dock->hInfoWnd = NULL;
	dock->hAddrWnd = NULL;
	dock->client.bottom = 0;
	dock->IDmax = dock->IDmin;

	GetDockList(dock, cust);
	if ( (cust[0] == '\0') && (forcecreate == FALSE) ) return; // 作成不要

	LoadCommonControls(ICC_COOL_CLASSES);

	dock->hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES,
			REBARCLASSNAME, NULL,
			WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN |
			// WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			RBS_BANDBORDERS | CCS_NORESIZE | CCS_NODIVIDER |
			RBS_VARHEIGHT | RBS_DBLCLKTOGGLE /*| CCS_BOTTOM*/,
			0, 0, 0, 0, hWnd, CHILDWNDID(IDW_REBAR), hInst, NULL);
	if ( dock->hWnd == NULL ) return; // 作成できなかった／未対応

	if ( X_dss & DSS_COMCTRL ) SendMessage(dock->hWnd, CCM_DPISCALE, TRUE, 0);
										// 使用する構造体を通知
	rbi.cbSize = sizeof(REBARINFO);
	rbi.fMask  = 0;
	rbi.himl   = NULL;
	SendMessage(dock->hWnd, RB_SETBARINFO, 0, (LPARAM)&rbi);
	ShowWindow(dock->hWnd, SW_SHOWNORMAL);
										// バーを登録する
	if ( cust[0] ){
		TCHAR *linep, *lp;
		UINT style;
		int cx;

		linep = cust;
		while ( *linep != '\0' ){
			lp = tstrchr(linep, ','); // 名前末尾
			if ( lp != NULL ){
				*lp++ = '\0';
			}else{
				lp = linep + tstrlen(linep);
			}
			cx = GetIntNumber((const TCHAR **)&lp);
			if ( SkipSpace((const TCHAR **)&lp) == ',' ) lp++;
			style = GetNumber((const TCHAR **)&lp);
			CreateDockBar(dock, linep, cx, style);
			linep = tstrchr(lp, '\n');
			if ( linep == NULL ) break;
			linep++;
		}
	}
	dock->client.bottom = SendMessage(dock->hWnd, RB_GETBARHEIGHT, 0, 0);
}

// レイアウトメニュー =========================================================
void CheckMenuS(PPXDOCK *dock, HMENU hMenu, int ID, const TCHAR *name)
{
	int i, count;
	REBARBANDINFO rbi;

	if ( dock->hWnd != NULL ){
		count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
		for( i = 0 ; i < count ; i++ ){
			rbi.cbSize = sizeof(REBARBANDINFO);
			rbi.fMask = RBBIM_ID;
			rbi.wID = NOSETWID;

			SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
			if ( rbi.wID != NOSETWID ){
				if ( !tstrcmp(ThPointerT(dock->th, rbi.wID), name) ){
					// 登録済み→削除候補
					AppendMenu(hMenu, MF_ES | MF_CHECKED, RMENU_DEL + i, name);
					return;
				}
			}
		}
	}
	AppendMenuString(hMenu, ID, name);
	return;
}

void ChangeDockBarState(PPXDOCK *dock, int modifystyle)
{
	int i, count;
	REBARBANDINFO rbi;

	count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
	for ( i = 0 ; i < count; i++ ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_STYLE;
		SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
		rbi.fStyle = rbi.fStyle ^ modifystyle;
		SendMessage(dock->hWnd, RB_SETBANDINFO, i, (LPARAM)&rbi);
	}
	SaveReBar(dock);
}

BOOL DockAddBar(HWND hWnd, PPXDOCK *dock, const TCHAR *name)
{
	REBARBANDINFO rbi;
					// 1番目のスタイルを反映
	if ( dock->hWnd == NULL ){
		InitDock(hWnd, dock, TRUE);
		if ( dock->hWnd == NULL ) return FALSE; // 作成失敗
	}

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_STYLE;
	rbi.fStyle = 0; // レバーがないときに使用される
	SendMessage(dock->hWnd, RB_GETBANDINFO, 0, (LPARAM)&rbi);

	if ( FALSE == CreateDockBar(dock, name, 100,
	   (rbi.fStyle & (RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS | RBBS_CHILDEDGE)))){
		return FALSE;
	}
	ModifyReBar(dock);
	return TRUE;
}

HWND GetBarWnd(PPXDOCK *dock, const TCHAR *name, int *indexrecv)
{
	REBARBANDINFO rbi;
	int index, count;

	count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
	for( index = 0 ; index < count ; index++ ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_ID | RBBIM_CHILD;
		rbi.wID = NOSETWID;

		SendMessage(dock->hWnd, RB_GETBANDINFO, index, (LPARAM)&rbi);
		if ( (rbi.wID != NOSETWID) &&
			 (tstrstr(ThPointerT(dock->th, rbi.wID), name) != NULL) ){
			if ( indexrecv != NULL ) *indexrecv = index;
			return rbi.hwndChild;
		}
	}
	return NULL;
}

HWND GetBarChild(PPXDOCK *dock, const TCHAR *name)
{
	HWND hChildWnd;

	hChildWnd = GetBarWnd(dock, name, NULL);
	if ( hChildWnd == NULL ) return NULL;
	return GetWindow(hChildWnd, GW_CHILD);
}

BOOL DockDropBar(PPC_APPINFO *cinfo, PPXDOCK *dock, const TCHAR *name)
{
	HWND hChildWnd;

	hChildWnd = GetBarChild(dock, name);
	if ( hChildWnd == NULL ) return FALSE;
	StartAutoDD(cinfo, hChildWnd, NULL, AUTODD_LEFT);
	return TRUE;
}

BOOL DockSendKeyBar(PPXDOCK *dock, const TCHAR *name)
{
	HWND hChildWnd;
	TCHAR *code;
	int oldctrl, oldalt, oldshift;

	code = tstrchr(name, ',');
	if ( code == NULL ) return FALSE;
	*code++ = '\0';

	hChildWnd = GetBarChild(dock, name);
	if ( hChildWnd == NULL ) return FALSE;

	for ( ; ; ){
		UTCHAR c;
		int key;

		c = SkipSpace((const TCHAR **)&code);
		if ( c < ' ' ) break;
		key = GetKeyCode((const TCHAR **)&code);
		if ( key < 0 ){
			XMessage(NULL, T("sendkey"), XM_GrERRld, MES_EPRM);
			break;
		}
		if ( key == K_NULL ){
			Sleep(100);
			continue;
		}
		if ( !(key & K_v) && !IsalnumA(key) ){
			int vkey;

			vkey = (int)VkKeyScan((TCHAR)(key & 0xff));
			key = (vkey & 0xff) | (key & 0xff00);
			if ( vkey & B8 ) setflag(key, K_s);
		}
		oldctrl  = GetAsyncKeyState(VK_CONTROL);
		oldalt   = GetAsyncKeyState(VK_MENU);
		oldshift = GetAsyncKeyState(VK_SHIFT);

		if ( ((oldshift & KEYSTATE_PUSH) >> 15) ^ (key & K_s) ){
			PostMessage(hChildWnd, (key & K_s) ? WM_KEYDOWN : WM_KEYUP, VK_SHIFT, 0);
		}
		if ( ((oldctrl & KEYSTATE_PUSH) >> 14) ^ (key & K_c) ){
			PostMessage(hChildWnd, (key & K_c) ? WM_KEYDOWN : WM_KEYUP, VK_CONTROL, 0);
		}
		if ( ((oldalt & KEYSTATE_PUSH) >> 13 )^ (key & K_a) ){
			PostMessage(hChildWnd, (key & K_a) ? WM_KEYDOWN : WM_KEYUP, VK_MENU, 0);
		}
		PostMessage(hChildWnd, WM_KEYDOWN, key & 0xff, 0);
		PostMessage(hChildWnd, WM_KEYUP, key & 0xff, B31);

		if ( ((oldshift & KEYSTATE_PUSH) >> 15) ^ (key & K_s) ){
			PostMessage(hChildWnd, !(key & K_s) ? WM_KEYDOWN : WM_KEYUP, VK_SHIFT, 0);
		}
		if ( ((oldctrl & KEYSTATE_PUSH) >> 14) ^ (key & K_c) ){
			PostMessage(hChildWnd, !(key & K_c) ? WM_KEYDOWN : WM_KEYUP, VK_CONTROL, 0);
		}
		if ( ((oldalt & KEYSTATE_PUSH) >> 13 )^ (key & K_a) ){
			PostMessage(hChildWnd, !(key & K_a) ? WM_KEYDOWN : WM_KEYUP, VK_MENU, 0);
		}
	}
	return TRUE;
}

BOOL DockSizeBar(PPXDOCK *dock, const TCHAR *name)
{
	HWND hChildWnd;
	TCHAR *p;
	int index;
	RECT box;
	REBARBANDINFO rbi;

	p = tstrchr(name, ',');
	if ( p == NULL ) return FALSE;
	*p++ = '\0';

	hChildWnd = GetBarWnd(dock, name, &index);
	if ( hChildWnd == NULL ) return FALSE;
	hChildWnd = GetWindow(hChildWnd, GW_CHILD);
	if ( hChildWnd == NULL ) return FALSE;
	GetWindowRect(hChildWnd, &box);
	box.right -= box.left;
	box.bottom -= box.top;

	if ( Isdigit(*p) ) box.right = GetNumber((const TCHAR **)&p);
	if ( *p == '*' ){
		p++;
		box.bottom = GetNumber((const TCHAR **)&p);
	}

	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_CHILDSIZE;
	if ( SendMessage(dock->hWnd, RB_GETBANDINFO, index, (LPARAM)&rbi) != 0 ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_CHILDSIZE;
		rbi.cx = rbi.cxMinChild = box.right;
		rbi.cyMinChild = box.bottom;
		SendMessage(dock->hWnd, RB_SETBANDINFO, index, (LPARAM)&rbi);
	}
	return TRUE;
}

BOOL DockCommands(HWND hWnd, PPXDOCK *dock, int mode, const TCHAR *name)
{
	HWND hChildWnd;
	int index;

	switch (mode){
		case dock_add:
			return DockAddBar(hWnd, dock, name);

		case dock_toggle:
			if ( name[1] == '\0' ){
				if ( name[0] == 's' ){
					ChangeDockBarState(dock, RBBS_CHILDEDGE);
					return TRUE;
				}
				if ( name[0] == 'l' ){
					ChangeDockBarState(dock, RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS);
					return TRUE;
				}
			}
			if ( GetBarWnd(dock, name, &index) == NULL ){
				return DockAddBar(hWnd, dock, name);
			}
			// dock_delete へ

		case dock_delete:
			hChildWnd = GetBarWnd(dock, name, &index);
			if ( hChildWnd == NULL ) return FALSE;
			DestroyWindow(hChildWnd);
			SendMessage(dock->hWnd, RB_DELETEBAND, index, 0);
			ModifyReBar(dock);
			return TRUE;

		case dock_focus:
			hChildWnd = GetBarChild(dock, name);
			if ( hChildWnd == NULL ) return FALSE;
			SetFocus(hChildWnd);
			return TRUE;

		case dock_sendkey:
			return DockSendKeyBar(dock, name);

		case dock_size:
			return DockSizeBar(dock, name);

		default:
			return FALSE;
	}
}

void CheckDMenuS(PPXDOCK *dock, AppendDMenuS *ams, const TCHAR *name)
{
	int i, count, style = MF_ES;
	REBARBANDINFO rbi;
	TCHAR buf[CMDLINESIZE];
	const TCHAR *cmd = T("add"), *mname;

	mname = MessageText(name);
	if ( mname != name ) name += MESTEXTIDLEN + 1;
	if ( dock->hWnd != NULL ){
		count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
		for( i = 0 ; i < count ; i++ ){
			rbi.cbSize = sizeof(REBARBANDINFO);
			rbi.fMask = RBBIM_ID;
			rbi.wID = NOSETWID;

			SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
			if ( rbi.wID != NOSETWID ){
				if ( !tstrcmp(ThPointerT(dock->th, rbi.wID), name) ){
					// 登録済み→削除候補
					style = MF_ES | MF_CHECKED;
					cmd = T("delete");
					break;
				}
			}
		}
	}
	AppendMenu(ams->hMenu, style, *ams->index, mname);
	wsprintf(buf, T("*dock %s, %c, %s"), cmd, ams->locate, name);
	ThAddString(ams->TH, buf);
	(*ams->index)++;
	return;
}

HMENU MakeDockMenu(PPXDOCK *dock, HMENU hPopupMenu, DWORD *index, ThSTRUCT *TH)
{
	AppendDMenuS ams;
	int i, count;
	REBARBANDINFO rbi;
	TCHAR name[VFPS], buf[VFPS];

	if ( hPopupMenu == NULL ) hPopupMenu = CreatePopupMenu();
	ams.hMenu = hPopupMenu;
	ams.index = index;
	ams.TH = TH;
	ams.locate = dock->cust[tstrlen(dock->cust) - 1];

	CheckDMenuS(dock, &ams, RMENUSTR__ADDR);
#if EnableAddressBreadCrumbListBar
	CheckDMenuS(dock, &ams, RMENUSTR__ADDRBL);
#endif
	CheckDMenuS(dock, &ams, RMENUSTR__DRIVES);
	CheckDMenuS(dock, &ams, RMENUSTR__INPUT);
	CheckDMenuS(dock, &ams, RMENUSTR__STAT);
	CheckDMenuS(dock, &ams, RMENUSTR__INF);
		// ツールバー一覧
	count = 0;
	for ( ; ; ){
		if ( EnumCustData(count++, name, name, 0) < 0 ) break;
		if ( (name[0] != 'B') || (name[1] != '_') ) continue;
		CheckDMenuS(dock, &ams, name);
	}
		// アドイン
	if ( dock->hWnd != NULL ){
		count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
		for( i = 0 ; i < count ; i++ ){
			rbi.cbSize = sizeof(REBARBANDINFO);
			rbi.fMask = RBBIM_ID;
			rbi.wID = NOSETWID;

			SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
			if ( rbi.wID != NOSETWID ){
				TCHAR *ptr;

				ptr = ThPointerT(dock->th, rbi.wID);
				if ( tstrchr(ptr, '.') ||
					 (!memcmp(ptr, RMENUSTR_INPUT, RMENUSTR_INPUT_SIZE) &&
					  ((UTCHAR)*(ptr + RMENUSTR_INPUT_LEN) >= ' '))  ){
					AppendMenu(ams.hMenu, MF_ES | MF_CHECKED, *index, ptr);
					wsprintf(buf, T("*dock delete,%c,%s"), ams.locate, ptr);
					ThAddString(TH, buf);
					(*index)++;
				}
			}
		}
	}
	if ( dock->hWnd != NULL ){
		AppendMenu(ams.hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuString(ams.hMenu, *index, RMENUSTR_LINE);
		(*index)++;
		wsprintf(buf, T("*dock toggle,%c,s"), ams.locate);
		ThAddString(TH, buf);
		AppendMenuString(ams.hMenu, *index, RMENUSTR_LOCK);
		(*index)++;
		buf[15] = 'l';
		ThAddString(TH, buf);
	}
	return ams.hMenu;
}

void DockModifyMenu(HWND hWnd, PPXDOCK *dock, POINT *menupos)
{
	TCHAR buf[16];

	if ( dock->cinfo == NULL ) return;
	dock->cinfo->PopupPosType = PPT_SAVED;
	if ( menupos != NULL ){
		dock->cinfo->PopupPos = *menupos;
	}else{
		GetMessagePosPoint(dock->cinfo->PopupPos);
	}
	if ( hWnd == NULL ){
		PostMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND, K_layout, 0);
	}else{
		wsprintf(buf, T("%%M?dock%cmenu"),
				TinyCharLower(dock->cust[tstrlen(dock->cust) - 1]));
		PP_ExtractMacro(hWnd, &dock->cinfo->info, NULL, buf, NULL, 0);
	}
}

// WM_Notify など =============================================================
void RToolbarCommand(HWND hWnd, PPXDOCK *dock, int id, int orcode)
{
	RECT box;
	POINT pos;
	TCHAR *p;

	if ( dock->cinfo == NULL ) return;
	if ( !SendMessage(hWnd, TB_GETRECT, (WPARAM)id, (LPARAM)&box) ) return;
	pos.x = box.left;
	pos.y = box.bottom;
	ClientToScreen(hWnd, &pos);

	p = GetToolBarCmd(hWnd, dock->th, id);
	if ( p == NULL ) return;
	if ( orcode ){
		WORD key;

		key = *(WORD *)(p + 1) | (WORD)orcode;
		if ( key == (K_s | K_raw | K_bs) ){
			key = K_raw | K_c | K_bs;
		}
		SendMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND,
				TMAKELPARAM(K_POPOPS, PPT_SAVED), TMAKELPARAM(pos.x, pos.y));
		SendMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND, key, 0);
	}else{
		SendMessage(dock->cinfo->info.hWnd, WM_PPCEXEC,
				(WPARAM)p, TMAKELPARAM(pos.x, pos.y));
	}
}

void DockFixPPcBarSize(PPXDOCK *dock)
{
	int i, count;
	REBARBANDINFO rbi;

	if ( dock->cinfo == NULL ) return;

	count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
	for( i = 0 ; i < count ; i++ ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_ID | RBBIM_CHILDSIZE;
		rbi.wID = NOSETWID;

		SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
		if ( rbi.wID != NOSETWID ){
			if ( !tstrcmp(ThPointerT(dock->th, rbi.wID), RMENUSTR_STAT) ){
				rbi.cyMinChild = dock->font.Y;
				rbi.cyMinChild *= dock->cinfo->stat.height;

				SendMessage(dock->hWnd, RB_SETBANDINFO, i, (LPARAM)&rbi);
				continue;
			}
			if ( !tstrcmp(ThPointerT(dock->th, rbi.wID), RMENUSTR_INF) ){
				rbi.cyMinChild = dock->font.Y * 2;
				rbi.cyMinChild = dock->font.Y * (dock->cinfo->inf1.height + dock->cinfo->inf2.height);
				SendMessage(dock->hWnd, RB_SETBANDINFO, i, (LPARAM)&rbi);
				continue;
			}
		}
	}
	return;
}

int DockNotify(PPXDOCK *dock, NMHDR *nmh)
{
	if ( nmh->idFrom == IDW_DRIVEBAR ){
		DriveBarNotify(dock, nmh);
		return TRUE;
	}

	switch (nmh->code){
		case NM_RCLICK: {
			REBARBANDINFO rbi;
			int i, count;
			TCHAR *bar;

			count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
			for ( i = 0 ; i < count ; i++ ){
				rbi.cbSize = sizeof(REBARBANDINFO);
				rbi.fMask = RBBIM_CHILD | RBBIM_ID;
				rbi.wID = NOSETWID;

				SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
				if ( rbi.hwndChild != nmh->hwndFrom ) continue;
				if ( rbi.wID == NOSETWID ) break;

				bar = ThPointerT(dock->th, rbi.wID);
				if ( (bar[0] == 'B') && (bar[1] == '_') ){
					if ( IsTrue(ToolBarDirectoryButtonRClick(dock->hWnd, nmh, dock->th)) ){
						return 1;
					}
				}
				break; // 不明コントロール
			}
			DockModifyMenu(NULL, dock, NULL);
			return 1;
		}
		case RBN_LAYOUTCHANGED:
			SaveReBar(dock);
			return 1;

		case RBN_HEIGHTCHANGE:
			dock->client.bottom = SendMessage(dock->hWnd, RB_GETBARHEIGHT, 0, 0);
			return 1;

		case TBN_DROPDOWN:
			RToolbarCommand(nmh->hwndFrom, dock, ((LPNMTOOLBAR)nmh)->iItem, K_s);
			return 1;
	}
	return 0;
}

BOOL DocksNotify(PPXDOCKS *docks, NMHDR *nmh)
{
	HWND hParentWnd;

	hParentWnd = GetParent(nmh->hwndFrom);
	if ( (nmh->hwndFrom == docks->t.hWnd) || (hParentWnd == docks->t.hWnd) ){
		DockNotify(&docks->t, nmh);
		return TRUE;
	}
	if ( (nmh->hwndFrom == docks->b.hWnd) || (hParentWnd == docks->b.hWnd) ){
		DockNotify(&docks->b, nmh);
		return TRUE;
	}
	return FALSE;
}

BOOL DockNeedTextNotify(PPXDOCK *dock, NMHDR *nmh)
{
	int i, count;

#if 0
	if ( (nmh->idFrom >= IDW_DRIVES) && (nmh->idFrom < (IDW_DRIVES + 32) ) ){
		((LPTOOLTIPTEXT)nmh)->lpszText = T("text");
		((LPTOOLTIPTEXT)nmh)->hinst = NULL;
		return TRUE;
	}
#endif
	count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);
	for ( i = 0 ; i < count; i++ ){
		REBARBANDINFO rbi;

		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_ID | RBBIM_CHILD;
		rbi.wID = NOSETWID;
		SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
		if ( rbi.wID == NOSETWID ) continue;

		if ( *ThPointerT(dock->th, rbi.wID) != 'B' ) continue;
		if ( SetToolBarTipText(rbi.hwndChild, dock->th, nmh) ) return TRUE;
	}
	return FALSE;
}

BOOL DocksNeedTextNotify(PPXDOCKS *docks, NMHDR *nmh)
{
	if ( docks->t.hWnd != nmh->hwndFrom ){
		if ( DockNeedTextNotify(&docks->t, nmh) ) return TRUE;
	}
	if ( docks->b.hWnd != nmh->hwndFrom ){
		if ( DockNeedTextNotify(&docks->b, nmh) ) return TRUE;
	}
	return FALSE;
}

void DockWmCommand(PPXDOCK *dock, WPARAM wParam, LPARAM lParam)
{
	int orcode = 0;

	RECT box;
	POINT pos;
	const TCHAR *p;

	if ( dock->cinfo == NULL ) return;
	SendMessage((HWND)lParam, TB_GETRECT, (WPARAM)LOWORD(wParam), (LPARAM)&box);
	pos.x = box.left;
	pos.y = box.bottom;
	ClientToScreen((HWND)lParam, &pos);

	p = GetToolBarCmd((HWND)lParam, dock->th, LOWORD(wParam));
	if ( p == NULL ) return;
	if ( orcode ){
		WORD key;

		key = *(WORD *)(p + 1) | (WORD)orcode;
		if ( key == (K_s | K_raw | K_bs) ){
			key = K_raw | K_c | K_bs;
		}
		SendMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND,
				TMAKELPARAM(K_POPOPS, PPT_SAVED), TMAKELPARAM(pos.x, pos.y));
		SendMessage(dock->cinfo->info.hWnd, WM_PPXCOMMAND, key, 0);
	}else{
		SendMessage(dock->cinfo->info.hWnd, WM_PPCEXEC,
				(WPARAM)p, TMAKELPARAM(pos.x, pos.y));
	}
}

BOOL DocksWmCommand(PPXDOCKS *docks, WPARAM wParam, LPARAM lParam)
{
	UINT id = (UINT)LOWORD(wParam);

	if ( (id >= docks->t.IDmin) && (id < docks->t.IDmax) ){
		DockWmCommand(&docks->t, wParam, lParam);
		return TRUE;
	}
	if ( (id >= docks->b.IDmin) && (id < docks->b.IDmax) ){
		DockWmCommand(&docks->b, wParam, lParam);
		return TRUE;
	}
	if ( (id >= IDW_DRIVES) && (id < (IDW_DRIVES + 32)) ){
		PushDriveButton(docks, id);
		return TRUE;
	}
	return FALSE;
}

void DockWmDevicechange(PPXDOCK *dock, WPARAM wParam, LPARAM lParam)
{
	REBARBANDINFO rbi;
	int i, count;
	TCHAR *bar;

	if ( dock->hWnd == NULL ) return;
	count = (int)SendMessage(dock->hWnd, RB_GETBANDCOUNT, 0, 0);

	for( i = 0 ; i < count ; i++ ){
		int driveno, drives;
		TBREPLACEBITMAP trb;
		SIZE BarSize;

		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_CHILD | RBBIM_ID;
		rbi.wID = NOSETWID;
		SendMessage(dock->hWnd, RB_GETBANDINFO, i, (LPARAM)&rbi);
		if ( rbi.wID == NOSETWID ) break;

		bar = ThPointerT(dock->th, rbi.wID);
		if ( tstrcmp(bar, RMENUSTR_DRIVES) != 0 ) continue;

		trb.nIDNew = (UINT_PTR)GetProp(rbi.hwndChild, RMENUSTR_DRIVES);
		drives = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
		for ( driveno = 0 ; driveno < 26 ; driveno++ ){
			if ( drives & (B0 << driveno) ){
				if ( wParam == DBT_DEVICEARRIVAL ){
					HDC hDC, hMDC;
					HGDIOBJ hOldBmp;
					TCHAR buf[8];

					hDC = GetDC(dock->hWnd);
					hMDC = CreateCompatibleDC(hDC);

					hOldBmp = SelectObject(hMDC, (HBITMAP)trb.nIDNew);
					MakeDriveImage(hMDC, driveno, buf);
					SelectObject(hMDC, hOldBmp);
					DeleteDC(hMDC);
					ReleaseDC(dock->hWnd, hDC);
				}
				SendMessage(rbi.hwndChild, TB_HIDEBUTTON,
						IDW_DRIVES + driveno, (wParam != DBT_DEVICEARRIVAL));
			}
		}
		if ( wParam == DBT_DEVICEARRIVAL ){
			trb.hInstOld = NULL;
			trb.hInstNew = NULL;
			trb.nIDOld = trb.nIDNew;
			trb.nButtons = 26;
			SendMessage(rbi.hwndChild, TB_REPLACEBITMAP, 0, (LPARAM)&trb);
		}

		if ( SendMessage(rbi.hwndChild, TB_GETMAXSIZE, 0, (LPARAM)&BarSize) ){
			SendMessage(dock->hWnd, RB_SETBANDWIDTH, i, BarSize.cx);
		}
	}
}

void DocksWmDevicechange(PPXDOCKS *docks, WPARAM wParam, LPARAM lParam)
{
	DockWmDevicechange(&docks->t, wParam, lParam);
	DockWmDevicechange(&docks->b, wParam, lParam);
}

void DocksInfoRepaint(PPXDOCKS *docks)
{
	if ( docks->t.hInfoWnd != NULL ){
		InvalidateRect(docks->t.hInfoWnd, NULL, FALSE);
	}
	if ( docks->b.hInfoWnd != NULL ){
		InvalidateRect(docks->b.hInfoWnd, NULL, FALSE);
	}
}

void DocksStatusRepaint(PPXDOCKS *docks)
{
	if ( docks->t.hStatusWnd != NULL ){
		InvalidateRect(docks->t.hStatusWnd, NULL, FALSE);
	}
	if ( docks->b.hStatusWnd != NULL ){
		InvalidateRect(docks->b.hStatusWnd, NULL, FALSE);
	}
}

void DocksInit(PPXDOCKS *docks, HWND hWnd, struct tagPPC_APPINFO *cinfo, const TCHAR *RegID, HFONT hFont, int fontY, ThSTRUCT *th, UINT *CommandID)
{
										// Dock
	docks->t.cinfo = docks->b.cinfo = cinfo;
	docks->t.th = docks->b.th = th;
	docks->t.font.h = docks->b.font.h = hFont;
	docks->t.font.Y = docks->b.font.Y = fontY;

	docks->t.IDmin = *CommandID;
	wsprintf(docks->t.cust, T("%s_T"), RegID);
	InitDock(hWnd, &docks->t, FALSE);

	docks->b.IDmin = docks->t.IDmax;
	wsprintf(docks->b.cust, T("%s_B"), RegID);
	InitDock(hWnd, &docks->b, FALSE);
	*CommandID = docks->b.IDmax;
}

BOOL ToolBarDirectoryButtonRClick(HWND hParentWnd, NMHDR *nmh, ThSTRUCT *th)
{
	TCHAR *p;

	p = GetToolBarCmd(nmh->hwndFrom, th, ((LPNMTOOLBAR)nmh)->iItem);
	if ( p != NULL ){
		if ( (UTCHAR)*p == EXTCMD_CMD ){
			if ( (*(p + 1) == '%') && (*(p + 2) == 'j') ){
				TCHAR path[VFPS];
				POINT pos;

				p += 3;
				GetLineParam((const TCHAR **)&p, path);
				PP_ExtractMacro(hParentWnd, NULL, NULL, path, path, 0);
				GetMessagePosPoint(pos);
				VFSSHContextMenu(hParentWnd, &pos, NULL, path, NULL);
				return TRUE;
			}
		}
	}
	return FALSE;
}
