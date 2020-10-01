/*-----------------------------------------------------------------------------
	Paper Plane cUI												Cell 表示

 a------------------------------------------------------------a
 |b----b       lspc / 2                                       |
 ||    |c----------------------------------------------------c|
 ||    ||      entry info 1st line                           ||
 ||icon|+----------------------------------------------------+|
 ||    ||      entry info 2nd line                           ||
 ||    |c----------------------------------------------------c|
 |b----b       (lspc + 1) / 2                                 |
 a------------------------------------------------------------a

 a: cell表示領域 ( BaseBox )
 b: icon DE_ICON:文字扱い アイコン幅fontX * 1 - ICONBLANK  アイコン高さ fontY
		 DE_ICON2:アイコン高さ=書式値 (a.bottom - b.bottom == ICONBLANK)
		 DE_IMAGE:高さ=書式値*fontY
 c: entry info 高さ: fontY(フォント高さ + lspc)
-----------------------------------------------------------------------------*/
#pragma setlocale("Japanese")
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include <sys/stat.h>
#pragma hdrstop

#define ICONDEBUG 0

const TCHAR VolString[] = T("<vol>");
const TCHAR ReparseString[] = T("<link>");
const TCHAR MarkString[1] = {T('*')};
const TCHAR AttributeString[] = T("SDNGCA?");
const TCHAR AttributeNULLString[] = T("________  ");
const TCHAR StatString[] = T("rwxrwxrwx");
const TCHAR SpaceString[] = T(" ");
const TCHAR ErrorString[] = T("x");
const TCHAR *EMonth[] = {
	T("Jan"), T("Feb"), T("Mar"), T("Apr"), T("May"), T("Jun"),
	T("Jul"), T("Aug"), T("Sep"), T("Oct"), T("Nov"), T("Dec")
};
const TCHAR *EWeek[] = {
	T("Sun"), T("Mon"), T("Tue"), T("Wed"), T("Thu"), T("Fri"), T("Sat")};
const TCHAR *JWeek[] = {
	T("日"), T("月"), T("火"), T("水"), T("木"), T("金"), T("土")};
const TCHAR StrNoC[] = T("name error");
const TCHAR StrLoad[] = T("?");

BYTE endofformat[] = "";

const WCHAR CheckBoxCodeString[4][2] = {
	{0x20, 0}, {0x2713, 0}, {0x2610, 0}, {0x2611, 0}};

typedef struct tagDISPSTRUCT {
	PPC_APPINFO *cinfo;
#ifdef USEDIRECTX
	DXDRAWSTRUCT *DxDraw;
#endif
	HDC hDC;
	ENTRYCELL *cell;	// 表示セル
	const TCHAR *cfileptr; // ファイル名の複数行表示の時に使用する

#if DRAWMODE == DRAWMODE_GDI
	COLORREF(WINAPI *DSetCellTextColor)(HDC, COLORREF); // 文字色を変更するAPI
#define CTC_DXP
#define CTC_DX
#define CTC_DXD
#define CTC_DXDP
#else
	COLORREF(*DSetCellTextColor)(DXDRAWSTRUCT *, HDC, COLORREF);
#define CTC_DXP disp->DxDraw,
#define CTC_DX disp.DxDraw,
#define CTC_DXD DXDRAWSTRUCT *DxDraw,
#define CTC_DXDP DxDraw,
#endif

	HBRUSH hbbr;	// 空白を埋めるときに使う(disp.bcと同じ色)
	HBRUSH hfbr;	// 空白を埋めるときに使う(反転時など限定、通常はNULL)
	HBRUSH hback;	// 空白を埋めるときに使う(cinfo->BackColor固定)
	RECT backbox;	// 空白を埋めるときに使う

	RECT lbox;
	COLORREF fc, bc;
	COLORREF textc; // 文字描画色
	POINT LP; // 最後に描画した位置

	int LineTopX; // 行の描画開始位置 & 枠描画の左端(アイコン無いときは、BaseBox.left)
	int Xd; // 意図している表示位置
	int fontX;	// フォントの基準幅
	int NoBack;	// 背景描画が不要か(X_WallpaperType)
	BOOL IsCursor;	// カーソル上か
	int lspc;
	int Xright;

	TCHAR buf[VFPS];
} DISPSTRUCT;

#if USEGRADIENTFILL
#ifndef GRADIENT_FILL_RECT_H
typedef USHORT COLOR16;
#ifndef __GNUC__
typedef struct _TRIVERTEX {
	LONG x;
	LONG y;
	COLOR16 Red;
	COLOR16 Green;
	COLOR16 Blue;
	COLOR16 Alpha;
} TRIVERTEX, *PTRIVERTEX, *LPTRIVERTEX;

typedef struct _GRADIENT_TRIANGLE {
	ULONG Vertex1;
	ULONG Vertex2;
	ULONG Vertex3;
} GRADIENT_TRIANGLE, *PGRADIENT_TRIANGLE, *LPGRADIENT_TRIANGLE;

typedef struct _GRADIENT_RECT{
	ULONG UpperLeft;
	ULONG LowerRight;
} GRADIENT_RECT, *PGRADIENT_RECT, *LPGRADIENT_RECT;
#endif
#define GRADIENT_FILL_RECT_H	0x00000000
#define GRADIENT_FILL_RECT_V	0x00000001
#define GRADIENT_FILL_TRIANGLE	0x00000002
#define GRADIENT_FILL_OP_FLAG	0x000000ff
#endif

DefineWinAPI(BOOL, GradientFill, (HDC hdc, const PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode));
LOADWINAPISTRUCT GradientFillAPI[] = {
	LOADWINAPI1(GradientFill),
{NULL, NULL}
};
#endif

#define Check_None		Check[0]
#define Check_Reverse	Check[1]
#define Check_Negative	Check[2]
#define Check_Box		Check[3]
#define Check_Line		Check[4]
#define Check_Mark		Check[5]
#define Check_Focusbox	Check[6]
#define Check_BackColor	Check[7]

void DrawTextRight(DISPSTRUCT *disp, const TCHAR *str, int len);

HMODULE hATL = NULL;
const WCHAR *AtlClass;
const WCHAR WMPplayer4IID[] = L"{6BF52A52-394A-11D3-B153-00C04F79FAA6}";

DefineWinAPI(BOOL, AtlAxWinInit, (void)) = NULL;
DefineWinAPI(HRESULT, AtlAxGetControl, (HWND, IUnknown **)) = NULL;

// Tip 関連 ===================================================================
#define TIPMARGINWIDTH 2
#define TIPBORDERWIDTH 1
#define TIPTEXTLENGTH 0x400

const TCHAR FileTipClass[] = T("PPcTip");
#define GEN_DRAW_FLAGS (DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS)
#define NAME_DRAW_FLAGS (GEN_DRAW_FLAGS | DT_NOCLIP)
#define TIP_DRAW_FLAGS (DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS)
LRESULT CALLBACK TipWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#define WMPOpenState void
#define WMPPlayState void
#define IWMPCdromCollection void
#define IWMPClosedCaption void
#define IWMPControls void
#define IWMPDVD void
#define IWMPError void
#define IWMPMedia void
#define IWMPMediaCollection void
#define IWMPNetwork void
#define IWMPPlayerApplication void
#define IWMPPlaylist void
#define IWMPPlaylistCollection void
#define IWMPSettings void

const IID XIID_IWMPPlayer4 =
	{0x6C497D62, 0x8919, 0x413c, {0x82, 0xDB, 0xE9, 0x35, 0xFB, 0x3E, 0xC5, 0x84}};
#undef  INTERFACE
#define INTERFACE xIWMPPlayer4
DECLARE_INTERFACE_(xIWMPPlayer4, IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
	STDMETHOD_(ULONG, AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG, Release) (THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *pctinfo);
	STDMETHOD(GetTypeInfo)(THIS_ UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo);
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID riid, LPOLESTR *rgszNames,
			UINT cNames, LCID lcid, DISPID *rgDispId);
	STDMETHOD(Invoke)(THIS_ DISPID dispIdMember, REFIID riid, LCID lcid,
			WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
			EXCEPINFO *pExcepInfo, UINT *puArgErr);
	STDMETHOD(close)(void);
	STDMETHOD(get_URL)(THIS_ BSTR *pbstrURL);
	STDMETHOD(put_URL)(THIS_ BSTR bstrURL);
	STDMETHOD(get_openState)(THIS_ WMPOpenState *pwmpos);
	STDMETHOD(get_playState)(THIS_ WMPPlayState *pwmpps);
	STDMETHOD(get_controls)(THIS_ IWMPControls **ppControl);
	STDMETHOD(get_settings)(THIS_ IWMPSettings **ppSettings);
	STDMETHOD(get_currentMedia)(THIS_ IWMPMedia **ppMedia);
	STDMETHOD(put_currentMedia)(THIS_ IWMPMedia *pMedia);
	STDMETHOD(get_mediaCollection)(THIS_
			IWMPMediaCollection **ppMediaCollection);
	STDMETHOD(get_playlistCollection)(THIS_
			IWMPPlaylistCollection **ppPlaylistCollection);
	STDMETHOD(get_versionInfo)(THIS_ BSTR *pbstrVersionInfo);
	STDMETHOD(launchURL)(THIS_ BSTR bstrURL);
	STDMETHOD(get_network)(THIS_ IWMPNetwork **ppQNI);
	STDMETHOD(get_currentPlaylist)(THIS_ IWMPPlaylist **ppPL);
	STDMETHOD(put_currentPlaylist)(THIS_ IWMPPlaylist *pPL);
	STDMETHOD(get_cdromCollection)(THIS_
			IWMPCdromCollection **ppCdromCollection);
	STDMETHOD(get_closedCaption)(THIS_ IWMPClosedCaption **ppClosedCaption);
	STDMETHOD(get_isOnline)(THIS_ VARIANT_BOOL *pfOnline);
	STDMETHOD(get_error)(THIS_ IWMPError **ppError);
	STDMETHOD(get_status)(THIS_ BSTR *pbstrStatus);
	STDMETHOD(get_dvd)(THIS_ IWMPDVD **ppDVD);
	STDMETHOD(newPlaylist)(THIS_ BSTR bstrName, BSTR bstrURL,
			IWMPPlaylist **ppPlaylist);
	STDMETHOD(newMedia)(THIS_ BSTR bstrURL, IWMPMedia **ppMedia);
	STDMETHOD(get_enabled)(THIS_ VARIANT_BOOL *pbEnabled);
	STDMETHOD(put_enabled)(THIS_ VARIANT_BOOL bEnabled);
	STDMETHOD(get_fullScreen)(THIS_ VARIANT_BOOL *pbFullScreen);
	STDMETHOD(put_fullScreen)(THIS_ VARIANT_BOOL bFullScreen);
	STDMETHOD(get_enableContextMenu)(THIS_ VARIANT_BOOL *pbEnableContextMenu);
	STDMETHOD(put_enableContextMenu)(THIS_ VARIANT_BOOL bEnableContextMenu);
	STDMETHOD(put_uiMode)(THIS_ BSTR bstrMode);
	STDMETHOD(get_uiMode)(THIS_ BSTR *pbstrMode);
	STDMETHOD(get_stretchToFit)(THIS_ VARIANT_BOOL *pbEnabled);
	STDMETHOD(put_stretchToFit)(THIS_ VARIANT_BOOL bEnabled);
	STDMETHOD(get_windowlessVideo)(THIS_ VARIANT_BOOL *pbEnabled);
	STDMETHOD(put_windowlessVideo)(THIS_ VARIANT_BOOL bEnabled);
	STDMETHOD(get_isRemote)(THIS_ VARIANT_BOOL *pvarfIsRemote);
	STDMETHOD(get_playerApplication)(THIS_
			IWMPPlayerApplication **ppIWMPPlayerApplication);
	STDMETHOD(openPlayer)(THIS_ BSTR bstrURL);
};

BOOL CheckOcx(const TCHAR *filepath)
{
	TCHAR buf[0x1000];
	const TCHAR *ext = tstrrchr(filepath, '.');
	HKEY hReg;
	int cnt = 0;
	BOOL result = FALSE;

	if ( ext == NULL ) return FALSE;
	wsprintf(buf, T("%s\\OpenWithProgids"), ext);
	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, buf, 0, KEY_READ, &hReg) ){
		return FALSE;
	}

	for ( ; ; cnt++ ){			// 設定を取り出す ---------------------
		DWORD keySize, valueSize;
		TCHAR keyName[MAX_PATH];
		DWORD Rtyp;

		keySize = TSIZEOF(keyName);
		valueSize = TSIZEOF(buf);
		if ( ERROR_SUCCESS != RegEnumValue(hReg, cnt, keyName, &keySize, NULL,
			&Rtyp, (BYTE *)buf, &valueSize) ){
			break;
		}
		if ( (keyName[0] == 'W') && (keyName[1] == 'M') &&
			 (keyName[2] == 'P') && Isdigit(keyName[3]) ){
			result = TRUE;
			break;
		}
	}
	RegCloseKey(hReg);
	return result;
}

BOOL LoadOcx(HWND hParentWnd, TCHAR *filename, int showmode)
{
	HWND hOcxWnd;
	const WCHAR *WndName;

	#ifdef UNICODE
		#define FILENAMEW filename
	#else
		#define FILENAMEW filenamebufW
		WCHAR filenamebufW[0x2000];
		AnsiToUnicode(filename, filenamebufW, 0x2000);
	#endif

	if ( showmode != OCX_IE ){
		if ( CheckOcx(filename) == FALSE ) return FALSE;
	}

	if ( hATL == INVALID_HANDLE_VALUE ) return FALSE;
	if ( hATL == NULL ){ // atl / atl71, 80, 90, 100, 110
		hATL = LoadLibrary(T("atl110.dll"));
		if ( hATL == NULL ){
			hATL = LoadLibrary(T("atl.dll"));
			if ( hATL == NULL ) return FALSE;
			AtlClass = L"AtlAxWin";
		}else{
			AtlClass = L"AtlAxWin110";
		}
		GETDLLPROC(hATL, AtlAxWinInit);
		GETDLLPROC(hATL, AtlAxGetControl);
		if ( (DAtlAxGetControl == NULL) || (DAtlAxWinInit() == FALSE) ){
			FreeLibrary(hATL);
			hATL = INVALID_HANDLE_VALUE;
			return FALSE;
		}
	}

	WndName = (showmode == OCX_IE) ? FILENAMEW : WMPplayer4IID;
	hOcxWnd = CreateWindowExW(0, AtlClass, WndName, WS_CHILD | WS_CLIPCHILDREN,
			0, 0, X_stip[TIP_PV_WIDTH], X_stip[TIP_PV_HEIGHT],
			hParentWnd, CHILDWNDID(IDW_PVOCX), hInst, NULL);
	if ( hOcxWnd != NULL ){
		IUnknown *pUnknown;

		ShowWindow(hOcxWnd, SW_SHOWNORMAL);
		if ( (showmode != OCX_IE) && SUCCEEDED(DAtlAxGetControl(hOcxWnd, &pUnknown)) ){
			xIWMPPlayer4 *pWMPPlayer4;
			if ( SUCCEEDED(pUnknown->lpVtbl->QueryInterface(pUnknown, &XIID_IWMPPlayer4, (void**)&pWMPPlayer4)) ){
				pWMPPlayer4->lpVtbl->put_URL(pWMPPlayer4, FILENAMEW);
				pWMPPlayer4->lpVtbl->Release(pWMPPlayer4);
			}
			pUnknown->lpVtbl->Release(pUnknown);
		}
	}
	return TRUE;
	#undef FILENAMEW
}

void CreateTipWnd(PPC_APPINFO *cinfo)
{
	WNDCLASS wndClass;
	DWORD StyleEx = WS_EX_TOOLWINDOW;
	DWORD Style = WS_POPUP | WS_BORDER | WS_CLIPCHILDREN;

	wndClass.style = CS_DBLCLKS;
	wndClass.lpfnWndProc = TipWndProc;
	wndClass.cbClsExtra = 0;
#if DRAWMODE != DRAWMODE_DW
	wndClass.cbWndExtra = 0;
#else
	wndClass.cbWndExtra = sizeof(LONG_PTR);
#endif
	wndClass.hInstance = hInst;
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = FileTipClass;
	RegisterClass(&wndClass);

	if ( C_tip[0] == C_AUTO ){
		GetCustData(T("C_tip"), &C_tip, sizeof(C_tip));
		if ( C_tip[0] == C_AUTO ) C_tip[0] = GetSysColor(COLOR_INFOTEXT);
		if ( C_tip[1] == C_AUTO ) C_tip[1] = GetSysColor(COLOR_INFOBK);
	}

	if ( cinfo->Tip.X_stip_mode != stip_mode_preview ){
		if ( OSver.dwMajorVersion >= 5 ) setflag(StyleEx, WS_EX_NOACTIVATE);
	}else{
		setflag(Style, WS_THICKFRAME);
	}
	cinfo->Tip.hTipWnd = CreateWindowEx(StyleEx, FileTipClass, NilStr,
		Style, 0, 0, 1, 1, cinfo->info.hWnd,
		NULL, hInst, cinfo);
}

// 選択カーソル移動後表示するタイマ
#pragma argsused
VOID CALLBACK ShowTipDelayTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	PPC_APPINFO *cinfo;
	UnUsedParam(uMsg); UnUsedParam(dwTime);

	KillTimer(hWnd, idEvent);
	// フォーカスあり、まだ要求があれば表示
	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( (GetFocus() == hWnd) && (cinfo->Tip.states & STIP_CMD_DELAY) ){
		ShowWindow(cinfo->Tip.hTipWnd, SW_SHOWNA);
		setflag(cinfo->Tip.states, STIP_STATE_SHOW);
		resetflag(cinfo->Tip.states, STIP_CMD_MASK);
		return;
	}
	resetflag(cinfo->Tip.states, STIP_CMD_DELAY); // ここでの表示が不要だった
}

void WmPPcTipPos(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int x, y, w, h;
#if DRAWMODE == DRAWMODE_DW
	DXDRAWSTRUCT *DxDraw;
#endif

	x = LOSHORTINT(wParam);
	y = HISHORTINT(wParam);
	w = LOSHORTINT(lParam);
	h = HISHORTINT(lParam);

	if ( message == WM_PPCPREVIEWPOS ){
		TCHAR filepath[0x1000], execpath[0x2000];
		PPC_APPINFO *cinfo;

		w = X_stip[TIP_PV_WIDTH];
		h = X_stip[TIP_PV_HEIGHT];
		cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		GetWindowText(hWnd, filepath, 0x1000);

		// 特殊環境変数 TipWnd に hWnd を保存
		wsprintf(execpath, T("%u"), hWnd);
		ThSetString(&cinfo->StringVariable, T("TipWnd"), execpath);
		if ( 0 <= PP_GetExtCommand(filepath, T("E_TipView"), execpath, NULL) ){
			ThSetString(&cinfo->StringVariable, T("TipTarget"), filepath);
			PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, execpath + 1, NULL, 0);
		}else if ( GetFileAttributesL(filepath) & FILE_ATTRIBUTE_DIRECTORY ){
			HWND hTreeWnd;
			InitVFSTree();

			hTreeWnd = CreateWindowEx(0, Str_TreeClass, Str_TreeClass,
				WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
				0, 0, w, h,
				hWnd, CHILDWNDID(IDW_PVTREE), hInst, 0);
			SendMessage(hTreeWnd, VTM_SETFLAG, (WPARAM)hWnd, (LPARAM)(VFSTREE_PATHNOTIFY));
			wsprintf(execpath, T("\"%s\""), filepath);
			SendMessage(hTreeWnd, VTM_TREECOMMAND, 0, (LPARAM)execpath);
			SetWindowText(hWnd, NilStr);
		}else if ( LoadOcx(hWnd, filepath, OCX_WMP) ){
			// LoadOcx ないで処理済み
		}else{
			#ifdef UNICODE
				wsprintf(execpath, L"ppvw.exe"
			#else
				wsprintf(execpath, "ppv.exe"
			#endif
					T(" -r -bootid:PX -P%u \"%s\""), hWnd, filepath);
			ComExec(hWnd, execpath, PPcPath);
		}
	}
//	XMessage(NULL, NULL, XM_DbgDIA, T("%d %d"), x, y);
	SetWindowPos(hWnd, NULL, x, y, w, h, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
	#if DRAWMODE == DRAWMODE_DW
		DxDraw = (DXDRAWSTRUCT *)GetWindowLongPtr(hWnd, 0);
{
		PPC_APPINFO *cinfo;
		cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		SetFontDxDraw(DxDraw, cinfo->hBoxFont , 0);
}
	#endif
}

LRESULT WmTipPPXCOMMAND(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	PPC_APPINFO *cinfo;
	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (LOWORD(wParam)){
		case KTN_escape:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			// KTN_focus へ
		case KTN_focus:
			SetFocus(cinfo->info.hWnd);
			break;

		case KTN_selected:
			// KTN_selected(本来のツリーを閉じる)は実行しない
			return SendMessage(cinfo->info.hWnd, WM_PPXCOMMAND, KTN_select, lParam);
		// default:
	}
	return NO_ERROR;
}

LRESULT CALLBACK TipWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#if DRAWMODE == DRAWMODE_DW
	DXDRAWSTRUCT *DxDraw;
#endif
	switch ( message ){
		#if DRAWMODE != DRAWMODE_DW
		case WM_SIZE:
			if ( (wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED) ){
				HWND hChildWnd = GetWindow(hWnd, GW_CHILD);
				if ( hChildWnd != NULL ){
					SetWindowPos(hChildWnd, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
				}
			}
			return 0;
		#endif

		case WM_PPCTIPPOS:
		case WM_PPCPREVIEWPOS:
			WmPPcTipPos(hWnd, message, wParam, lParam);
			return 0;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN: {
			POINT pos;
			HWND hParentWnd;

			LPARAMtoPOINT(pos, lParam);
			ClientToScreen(hWnd, &pos);
			ShowWindow(hWnd, SW_HIDE);
			hParentWnd = WindowFromPoint(pos);
			ScreenToClient(hParentWnd, &pos);
			SendMessage(hParentWnd, message, wParam, TMAKELPARAM(pos.x, pos.y));
			return 0;
		}

		// 「マウス ポインタをウィンドウに重ねたときにアクティブ」有効時、
		// アクティブにされるのを防止する
		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE;

		case WM_CREATE:
			SetWindowLongPtr(hWnd, GWLP_USERDATA,
					(LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
		#if DRAWMODE == DRAWMODE_DW
			CreateDxDraw(&DxDraw, hWnd);
			// ChangeSizeDxDraw(DxDraw, C_tip[1]);
			SetWindowLongPtr(hWnd, 0, (LONG_PTR)DxDraw);
		#endif
			return 0;

		#if DRAWMODE == DRAWMODE_DW
		case WM_DESTROY:
			DxDraw = (DXDRAWSTRUCT *)GetWindowLongPtr(hWnd, 0);
			CloseDxDraw(&DxDraw);
			return 0;

		case WM_DISPLAYCHANGE:
			InvalidateRect(hWnd, NULL, TRUE);
			// WM_SIZE へ
		case WM_SIZE:
			DxDraw = (DXDRAWSTRUCT *)GetWindowLongPtr(hWnd, 0);
			ChangeSizeDxDraw(DxDraw, C_tip[1]);
			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_PAINT: {
			TCHAR text[TIPTEXTLENGTH];
			RECT box;
			HBRUSH hBackBrush;
			PAINTSTRUCT ps;
			PPC_APPINFO *cinfo;

			cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			GetWindowText(hWnd, text, TSIZEOF(text));
			if ( text[0] == '\0') return 0;
			DxDraw = (DXDRAWSTRUCT *)GetWindowLongPtr(hWnd, 0);
			GetClientRect(hWnd, &box);
			box.left += TIPMARGINWIDTH;
			box.top += TIPMARGINWIDTH;

			if ( DxDraw == NULL ) BeginPaint(hWnd, &ps);
			if ( BeginDrawDxDraw(DxDraw, &ps) == DXSTART_NODRAW ) return 0;
			IfDXmode(ps.hdc)
			{
				DxSetTextColor(DxDraw, ps.hdc, C_tip[0]);
				DxSetBkColor(DxDraw, ps.hdc, C_tip[1]);
				DxMoveToEx(DxDraw, ps.hdc, box.left, box.top);
				DxDrawText(DxDraw, ps.hdc, text, -1, &box, DT_NOCLIP | TIP_DRAW_FLAGS);
				EndDrawDxDraw(DxDraw);
			} else{
				HGDIOBJ hOldFont;

				hOldFont = SelectObject(ps.hdc, cinfo->hBoxFont);
				hBackBrush = CreateSolidBrush(C_tip[1]);
				FillBox(ps.hdc, &ps.rcPaint, hBackBrush);
				DeleteObject(hBackBrush);

				SetTextColor(ps.hdc, C_tip[0]);
				SetBkColor(ps.hdc, C_tip[1]);
				DrawText(ps.hdc, text, -1, &box, DT_NOCLIP | TIP_DRAW_FLAGS);
				SelectObject(ps.hdc, hOldFont);
				EndPaint(hWnd, &ps);
			}
			return 0;
		}
		#else
		case WM_PAINT: {
			PAINTSTRUCT ps;

			TCHAR text[TIPTEXTLENGTH];
			RECT box;
			HGDIOBJ hOldFont;
			HBRUSH hBackBrush;
			PPC_APPINFO *cinfo;

			cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &box);
			box.left += TIPMARGINWIDTH;
			box.top += TIPMARGINWIDTH;

			GetWindowText(hWnd, text, TSIZEOF(text));
			hOldFont = SelectObject(ps.hdc, cinfo->hBoxFont);
			hBackBrush = CreateSolidBrush(C_tip[1]);
			FillBox(ps.hdc, &ps.rcPaint, hBackBrush);
			DeleteObject(hBackBrush);

			SetTextColor(ps.hdc, C_tip[0]);
			SetBkColor(ps.hdc, C_tip[1]);
			DrawText(ps.hdc, text, -1, &box, DT_NOCLIP | TIP_DRAW_FLAGS);
			SelectObject(ps.hdc, hOldFont);
			EndPaint(hWnd, &ps);
			return 0;
		}
		#endif

		default:
			if ( message == WM_PPXCOMMAND ){
				return WmTipPPXCOMMAND(hWnd, wParam, lParam);
			}
			break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void InitTipWnd(PPC_APPINFO *cinfo, const TCHAR *text, POINT *showpos, int w, int h)
{
	UINT message;
	RECT deskbox;

	message = (cinfo->Tip.X_stip_mode == stip_mode_preview) ? WM_PPCPREVIEWPOS : WM_PPCTIPPOS;

	GetDesktopRect(cinfo->info.hWnd, &deskbox);
	if ( (showpos->x + w) > deskbox.right ) showpos->x = deskbox.right - w;
	if ( showpos->x < deskbox.left ) showpos->x = deskbox.left;
	if ( (showpos->y + h) > deskbox.bottom ) showpos->y = showpos->y - cinfo->cel.Size.cy - h - (TIPMARGINWIDTH * 2 + TIPBORDERWIDTH);

	if ( cinfo->Tip.hTipWnd == NULL ) CreateTipWnd(cinfo);

	SetWindowText(cinfo->Tip.hTipWnd, text);

	if ( cinfo->Tip.states & STIP_CMD_NOW ){
		SendMessage(cinfo->Tip.hTipWnd, message, TMAKEWPARAM(showpos->x, showpos->y), TMAKELPARAM(w, h));
		ShowWindow(cinfo->Tip.hTipWnd, SW_SHOWNA);
		setflag(cinfo->Tip.states, STIP_STATE_SHOW);
		resetflag(cinfo->Tip.states, STIP_CMD_MASK);
	}else{ // STIP_CMD_DELAY
		PostMessage(cinfo->Tip.hTipWnd, message, TMAKEWPARAM(showpos->x, showpos->y), TMAKELPARAM(w, h));

		SetTimer(cinfo->info.hWnd, TIMERID_ENTRYTIP, X_stip[TIP_LONG_TIME], ShowTipDelayTimerProc);
		setflag(cinfo->Tip.states, STIP_STATE_DELAY);
	}
}

void SetFileNameTipMain(PPC_APPINFO *cinfo, HDC hDC, const TCHAR *filep, int nwid, int ext, POINT *showpos)
{
	RECT wbox;
	int length, result;
	SIZE textsize;
#ifdef USEDIRECTX
	BOOL gettdc = FALSE;
	HGDIOBJ hOldFont;
#endif

	length = tstrlen32(filep);

	if ( cinfo->Tip.X_stip_mode != stip_mode_preview ){
		#ifdef USEDIRECTX
			IfDXmode(hDC)
			{ // DirectX なら算出用のHDCを用意する
				gettdc = TRUE;
				hDC = GetDC(cinfo->info.hWnd);
				hOldFont = SelectObject(hDC, cinfo->hBoxFont);
			}
		#endif
		// ※１ nwid に収まる文字数と、全体のピクセル幅を算出
		GetTextExtentExPoint(hDC, filep, length,
				nwid * cinfo->fontX, &result, NULL, &textsize);
		wbox.left = 0;
		wbox.top = 0;
		wbox.right = textsize.cx;
		wbox.bottom = textsize.cy;
	}else{
		wbox.right = X_stip[TIP_PV_WIDTH];
		wbox.bottom = X_stip[TIP_PV_HEIGHT];
	}
	if ( !(cinfo->Tip.states & STIP_CMD_NOW) ){
		if ( cinfo->Tip.X_stip_mode == stip_mode_filename ){
			#pragma warning(suppress:4701) // stip_mode_filename != stip_mode_preview なので必ず※１を実行
			if ( result < ext ){ // エントリ名がはみ出すときだけ表示
				setflag( cinfo->Tip.states, STIP_CMD_DELAY );
			}else{ // 表示の必要が無い
				resetflag( cinfo->Tip.states, STIP_CMD_DELAY );
			}
		}else{
			setflag( cinfo->Tip.states, STIP_CMD_DELAY );
		}
	}

	if ( cinfo->Tip.states & (STIP_CMD_DELAY | STIP_CMD_NOW) ){
		int width = cinfo->cel.Size.cx - cinfo->fontX;
		if ( cinfo->celF.width < 41 ) width = cinfo->fontX * (41 - 1);
		// cell １つ分の幅よりはみ出すなら複数行表示
		if ( (wbox.right > width) || (tstrchr(filep, '\r') != NULL) ){
			wbox.right = width;
			DrawText(hDC, filep, length, &wbox, DT_CALCRECT | TIP_DRAW_FLAGS);
		}
// ↓状況判断して上にも表示する例…違和感一杯のためやめた
//		pos.y = disp->backbox.top + ((cinfo->e.cellN >= cinfo->e.cellNold) ?
//				- wbox.bottom - 3 : cinfo->cel.Size.y + 3);
		ClientToScreen(cinfo->info.hWnd, showpos);
//			XMessage(NULL, NULL, XM_DbgLOG, T(" %d,%d"), pos.x, pos.y);

		if ( cinfo->Tip.states & STIP_CMD_MOUSE ){
			DWORD pos;

			pos = GetMessagePos();
			showpos->x = LOSHORTINT(pos) - (wbox.right / 2);
			if ( (HISHORTINT(pos) + 32) > showpos->y ){
				showpos->y += 32; // マウスカーソルの大きさ分下げる
			}
		}
		InitTipWnd(cinfo, filep, showpos,
				wbox.right + TIPMARGINWIDTH * 2 + TIPBORDERWIDTH,
				wbox.bottom + TIPMARGINWIDTH * 2 + TIPBORDERWIDTH);
	}
#ifdef USEDIRECTX
	if ( IsTrue(gettdc) ){
		SelectObject(hDC, hOldFont);
		ReleaseDC(cinfo->info.hWnd, hDC);
	}
#endif
}

typedef struct {
	DWORD sizeAL, sizeAH;	// 加算サイズ
	DWORD files;		// ファイル数
	DWORD dirs;			// ディレクトリ数
	DWORD links;		// シンボリックリンク等
	DWORD skips;		// アクセスできなかったディレクトリ
	DWORD tick;
	BOOL countbreak;
} DIRSIZESTRUCT;

void PropDispMarkSizeDir(TCHAR *dir, DIRSIZESTRUCT *cs)
{
	TCHAR *t;
	HANDLE hFF;
	WIN32_FIND_DATA ff;

	(cs->dirs)++;
	t = dir + tstrlen(dir);
	CatPath(NULL, dir, T("*"));
	hFF = VFSFindFirst(dir, &ff);
	*t = '\0';
	if ( hFF == INVALID_HANDLE_VALUE ){
		cs->skips++;
	}else{
		do{
			if ( IsRelativeDir(ff.cFileName) ) continue;
			if ( (GetTickCount() - cs->tick) >= 500 ){
				cs->countbreak = TRUE;
				break;
			}
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
				cs->links++;
			}
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				CatPath(NULL, dir, ff.cFileName);
				PropDispMarkSizeDir(dir, cs);
				*t = 0;
				if ( cs->countbreak ) break;
			}else{
				(cs->files)++;
				AddDD(cs->sizeAL, cs->sizeAH, ff.nFileSizeLow, ff.nFileSizeHigh);
			}
		}while( IsTrue(VFSFindNext(hFF, &ff)) );
		VFSFindClose(hFF);
	}
}

void SetFileNameTipExt(PPC_APPINFO *cinfo, HDC hDC, const TCHAR *filep, int nwid, ENTRYCELL *cell, POINT *showpos)
{
	TCHAR buf[TIPTEXTLENGTH], buf2[0x400];
	TCHAR fsize[32];
	TCHAR Create[32], Write[32], Access[32];
	size_t len;
	DWORD CommentOffset;

	switch ( cinfo->Tip.X_stip_mode ){ // 消去
		case stip_mode_filename:
			tstrcpy(buf, GetCellFileName(cinfo, cell, buf2));
			break;

		case stip_mode_fileinfo:
			if ( filep == NULL ) filep = cell->f.cFileName;
			tstrlimcpy(buf, filep, TIPTEXTLENGTH);
			len = tstrlen(buf);
			tstrlimcpy(buf + len, T("\r\n"), TIPTEXTLENGTH - len);
			len += tstrlen(buf + len);
			CommentOffset = cell->comment;
			if ( CommentOffset != EC_NOCOMMENT ){
				tstrlimcpy(buf + len, ThPointerT(&cinfo->EntryComments, CommentOffset), TIPTEXTLENGTH - len);
				len += tstrlen(buf + len);
				tstrlimcpy(buf + len, T("\r\n"), TIPTEXTLENGTH - len);
				len += tstrlen(buf + len);
			}
			if ( (cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				 !(cell->attr & ECA_DIRC) ){
				DIRSIZESTRUCT cs = { 0, 0, 0, 0, 0, 0, 0, FALSE };
				TCHAR filename[VFPS];

				cs.tick = GetTickCount();

				GetCellRealFullName(cinfo, cell, filename);
				PropDispMarkSizeDir(filename, &cs);
				if ( !cs.countbreak ){
					cell->f.nFileSizeLow = cs.sizeAL;
					cell->f.nFileSizeHigh = cs.sizeAH;
					setflag(cell->attr, ECA_DIRC);
				}else{
					FormatNumber(fsize, XFN_SEPARATOR, 26, cs.sizeAL, cs.sizeAH);
					wsprintf(buf2, T("Size:\t>%s\r\nFiles:\t>%d\r\nFolders:\t>%d\r\n"), fsize, cs.files, cs.dirs);
					tstrlimcpy(buf + len, buf2, TIPTEXTLENGTH - len);
					len += tstrlen(buf + len);
				}
			}

			if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
				GetVistaFileProps(cinfo, buf + len, TIPTEXTLENGTH - len, cell);
				len += tstrlen(buf + len);
				if ( (TIPTEXTLENGTH - len) > 2 ){
					buf[len] = '\r';
					buf[len + 1] = '\n';
					len += 2;
				}
			}

			if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
				 (cell->attr & ECA_DIRC) ){
				FormatNumber(fsize, XFN_SEPARATOR, 26,
						cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
				wsprintf(buf2, T("Size:\t%s\r\n"), fsize);
				tstrlimcpy(buf + len, buf2, TIPTEXTLENGTH - len);
				len += tstrlen(buf + len);
			}

			CnvDateTime(Create, NULL, NULL, &cell->f.ftCreationTime);
			CnvDateTime(Write , NULL, NULL, &cell->f.ftLastWriteTime);
			CnvDateTime(Access, NULL, NULL, &cell->f.ftLastAccessTime);
			wsprintf(buf2, T("Create:\t%s\nWrite:\t%s\nAccess:\t%s"),
					Create, Write, Access);
			tstrlimcpy(buf + len, buf2, TIPTEXTLENGTH - len);
			break;

		case stip_mode_comment:
			buf[0] = '\0';
			CommentOffset = cell->comment;
			if ( CommentOffset != EC_NOCOMMENT ){
				tstrlimcpy(buf, ThPointerT(&cinfo->EntryComments, CommentOffset), TIPTEXTLENGTH);
			}
			break;

		case stip_mode_text:
			ThGetString(&cinfo->StringVariable, T("TipText"), buf, TIPTEXTLENGTH);
			break;

//		case stip_mode_preview:
		default:
			GetCellRealFullName(cinfo, cell, buf);
			break;
	}
	if ( buf[0] == '\0' ) return;
	SetFileNameTipMain(cinfo, hDC, buf, nwid, 0xfff, showpos);
}

void EndEntryTip(PPC_APPINFO *cinfo)
{
	KillTimer(cinfo->info.hWnd, TIMERID_ENTRYTIP);
	KillTimer(cinfo->info.hWnd, TIMERID_HOVERTIP);
	if ( cinfo->Tip.hTipWnd != NULL ){
		if ( cinfo->Tip.X_stip_mode == stip_mode_preview ){
			DestroyWindow(cinfo->Tip.hTipWnd);
			cinfo->Tip.hTipWnd = NULL;
		}else{
			ShowWindow(cinfo->Tip.hTipWnd, SW_HIDE);
		}
	}
	resetflag(cinfo->Tip.states, STIP_CMD_MASK | STIP_CMD_HOVER | STIP_STATE_READYMASK);
}

// WM_PAINT 経由でチップ表示の準備をする
void SetDelayEntryTip(DISPSTRUCT *disp, const TCHAR *filep, int nwid, int ext)
{
	POINT showpos;
	PPC_APPINFO *cinfo;

	cinfo = disp->cinfo;
	if ( !(cinfo->Tip.states & STIP_REQ_DELAY) || !disp->IsCursor ) return;
	resetflag(cinfo->Tip.states, STIP_REQ_DELAY);
	if ( TinyCheckCellEdit(cinfo) ) return; // セル変更中
	if ( cinfo->Tip.states & (STIP_CMD_NOW | STIP_STATE_READYMASK) ) return; // ShowEntryTip 表示準備中 / 遅延表示待機中 / 既に表示

	cinfo->Tip.X_stip_mode = stip_mode_filename;

	showpos.x = disp->Xd - TIPMARGINWIDTH - TIPBORDERWIDTH;
	showpos.y = disp->backbox.top + cinfo->cel.Size.cy + 3;

	if ( cinfo->Tip.X_stip_mode == stip_mode_filename ){
		SetFileNameTipMain(cinfo, disp->hDC, filep, nwid, ext, &showpos);
	}else{
		SetFileNameTipExt(cinfo, disp->hDC, filep, nwid, disp->cell, &showpos);
	}
}

// すぐにチップ表示する
void ShowEntryTip(PPC_APPINFO *cinfo, DWORD flags, int mode, ENTRYINDEX targetcell)
{
	POINT showpos;

	HideEntryTip(cinfo);
	cinfo->Tip.X_stip_mode = mode;

	// STIP_CMD_DELAY表示を無効化する
	cinfo->Tip.states = (cinfo->Tip.states & ~(STIP_CMD_DELAY | STIP_CMD_HOVER | STIP_CMD_MOUSE)) | STIP_CMD_NOW | flags;

	{
		HGDIOBJ hOldFont;
		HDC hDC;
		int deltaNo;

		deltaNo = targetcell - cinfo->cellWMin;
		showpos.x = CalcCellX(cinfo, deltaNo);
		showpos.y = CalcCellY(cinfo, deltaNo) + cinfo->cel.Size.cy;

		hDC = GetDC(cinfo->info.hWnd);
		hOldFont = SelectObject(hDC, cinfo->hBoxFont);
		SetFileNameTipExt(cinfo, hDC, CEL(targetcell).f.cFileName , 0, &CEL(targetcell), &showpos);
		SelectObject(hDC, hOldFont);
		ReleaseDC(cinfo->info.hWnd, hDC);
	}
}

//-----------------------------------------------------------------------------
#ifndef USEDIRECTX
#define DrawNumber(disp, str, len) {if ( IsTrue(UsePFont) ){ DrawTextRight(disp, str, len); }else{ DxTextOutRel(disp->DxDraw, disp->hDC, str, len);} }
#else
void DrawNumber(DISPSTRUCT *disp, const TCHAR *str, int len)
{
	if ( IsTrue(UsePFont) ){
		disp->backbox.left = disp->Xd;
		disp->backbox.right = disp->Xd + len * disp->fontX;
		DxDrawText(disp->DxDraw, disp->hDC, str, len, &disp->backbox,
				DT_RIGHT | DT_NOPREFIX | DT_SINGLELINE);
	} else{
		DxTextOutRel(disp->DxDraw, disp->hDC, str, len);
	}
}
#endif

void USEFASTCALL ModuleDraw(DISPSTRUCT *disp, const BYTE *fmt, ENTRYINDEX cellno)
{
	PPXMFILEDRAWSTRUCT fds;
	PPXMODULEPARAM pmp;
	WCHAR name[10];
	PPC_APPINFO *cinfo;
#ifndef UNICODE
	WIN32_FIND_DATAW ff;

	memcpy(&ff, &disp->cell->f, (BYTE *)&ff.cFileName - (BYTE *)&ff);
	AnsiToUnicode(disp->cell->f.cFileName, ff.cFileName, MAX_PATH);
	AnsiToUnicode(disp->cell->f.cAlternateFileName, ff.cAlternateFileName, 14);
	fds.finddata = &ff;
#else
	fds.finddata = &disp->cell->f;
#endif
	AnsiToUnicode((char *)fmt + 2 + 4, name, 10);
	fds.modulename = name;
	fds.commandhash = *(DWORD *)(fmt + 2);
	cinfo = disp->cinfo;

#ifndef USEDIRECTX
	fds.LoadCounter = cinfo->LoadCounter;
	fds.DrawArea.left = disp->Xd;
	fds.DrawArea.top = disp->LP.y;
	fds.DrawArea.right = fds.DrawArea.left + (*fmt * disp->fontX);
	fds.DrawArea.bottom = fds.DrawArea.top + (*(fmt + 1) * cinfo->fontY);
	fds.hDC = disp->hDC;
	fds.fontsize.cx = disp->fontX;
	fds.fontsize.cy = cinfo->fontY;
	fds.hBackBrush = disp->hfbr;
	fds.IsCursor = disp->IsCursor;
	fds.EntryIndex = cellno;
	pmp.draw = &fds;
	if ( PPXMRESULT_SKIP == CallModule(&cinfo->info, PPXMEVENT_FILEDRAW, pmp, NULL) ){
		DxFillRectColor(disp->DxDraw, fds.hDC, &fds.DrawArea, (HBRUSH)GetStockObject(DKGRAY_BRUSH), cinfo->BackColor);
	}
	disp->LP.x = fds.DrawArea.right;
	disp->Xd = fds.DrawArea.right;
#else
	{
		BITMAPINFOHEADER bmiHeader;
		LPVOID lpBits;
		HBITMAP hBMP;
		HGDIOBJ hOldBmp, hOldFont;
		HDC hDC, hDDC;

		memset(&bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmiHeader.biWidth = *fmt * disp->fontX;
		bmiHeader.biHeight = *(fmt + 1) * cinfo->fontY;
		bmiHeader.biPlanes = 1;
		bmiHeader.biBitCount = 32;

		hDDC = GetDC(cinfo->info.hWnd);
		hDC = CreateCompatibleDC(hDDC);

		hBMP = CreateDIBSection(hDDC, (BITMAPINFO *)&bmiHeader, DIB_RGB_COLORS, &lpBits, NULL, 0);

		hOldBmp = SelectObject(hDC, hBMP);

		fds.LoadCounter = cinfo->LoadCounter;
		fds.fontsize.cx = disp->fontX;
		fds.fontsize.cy = cinfo->fontY;
		fds.hBackBrush = disp->hfbr;
		fds.IsCursor = disp->IsCursor;
		fds.EntryIndex = cellno;

		fds.DrawArea.left = 0;
		fds.DrawArea.top = 0;
		fds.DrawArea.right = bmiHeader.biWidth;
		fds.DrawArea.bottom = bmiHeader.biHeight;
		fds.hDC = hDC;

		pmp.draw = &fds;
		SetTextColor(hDC, disp->fc);
		hOldFont = SelectObject(hDC, cinfo->hBoxFont);
		FillRect(hDC, &fds.DrawArea, disp->hback);

		if ( PPXMRESULT_SKIP == CallModule(&cinfo->info, PPXMEVENT_FILEDRAW, pmp, NULL) ){
			SelectObject(hDC, hOldBmp);
			DxFillRectColor(disp->DxDraw, fds.hDC, &fds.DrawArea, (HBRUSH)GetStockObject(DKGRAY_BRUSH), cinfo->BackColor);
		} else{
			IfDXmode(disp->hDC)
			{
				fds.DrawArea.left = disp->Xd;
				fds.DrawArea.top = disp->LP.y;
				fds.DrawArea.right = bmiHeader.biWidth;
				fds.DrawArea.bottom = bmiHeader.biHeight;
				DxDrawDIB(disp->DxDraw, &bmiHeader, lpBits, &fds.DrawArea, NULL, NULL);
			} else{
				BitBlt(disp->hDC, disp->Xd, disp->LP.y, bmiHeader.biWidth, bmiHeader.biHeight, hDC, 0, 0, SRCCOPY);
			}
			SelectObject(hDC, hOldBmp);
		}
		SelectObject(hDC, hOldFont);

		DeleteObject(hBMP);
		DeleteDC(hDC);
		ReleaseDC(cinfo->info.hWnd, hDDC);

		fds.DrawArea.right = bmiHeader.biWidth;
	}
	disp->Xd = disp->LP.x = disp->Xd + (*fmt * disp->fontX);
#endif
}

#if ICONDEBUG
void DrawIconListS(ICONCACHESTRUCT *icons, int base, int id, HDC hdcDst, int x, int y, TCHAR c)
{
	TCHAR a[100];
	int i;

	i = wsprintf(a, T("%3d"), base);
	MoveToEx(hdcDst, x, y, NULL);
	TextOut(hdcDst, 0, 0, a, i);
	i = wsprintf(a, T("%3d"), id);
	MoveToEx(hdcDst, x, y + 12, NULL);
	TextOut(hdcDst, 0, 0, a, i);
	i = wsprintf(a, T("%c%3d"), c, icons->size);
	MoveToEx(hdcDst, x, y + 24, NULL);
	TextOut(hdcDst, 0, 0, a, i);
}
#endif

BOOL DrawIconList(PPC_APPINFO *cinfo, ICONCACHESTRUCT *icons, int id, DIRECTXARG(int *cache) CTC_DXD HDC hdcDst, int x, int y, UINT fStyle)
{
#if ICONDEBUG
	int base;

	base = id;
#endif
	if ( (id < icons->minID) || (id > icons->maxID) ){
	#if ICONDEBUG
		DrawIconListS(icons, base, id, hdcDst, x, y, '*');
		return FALSE;
	#else
	#define TESTISIZE 32
	#ifndef USEDIRECTX // GDI
		DrawIconEx(hdcDst, x, y, LoadUnknownIcon(cinfo, TESTISIZE), TESTISIZE, TESTISIZE, 0, NULL, DI_NORMAL);
	#else // DirectX
		IfGDImode(hdcDst)
		{
			DrawIconEx(hdcDst, x, y, LoadUnknownIcon(cinfo, icons->height), icons->width, icons->height, 0, NULL, DI_NORMAL);
		} else{
			RECT box;

			box.left = x;
			box.top = y;
			box.right = icons->width;
			box.bottom = icons->height;
			DxDrawIcon(DxDraw, LoadUnknownIcon(cinfo, icons->height), &box, NOTIMPCACHE);
		}
	#endif
		return FALSE;
	#endif
	}
	id = (id - icons->minID) + icons->minIDindex;
	if ( id < 0 ) id += icons->size;
	if ( id >= icons->size ) id -= icons->size;
#ifndef USEDIRECTX // GDI
	DImageList_Draw(icons->hImage, id, hdcDst, x, y, fStyle);
#else // DirectX
	IfGDImode(hdcDst)
	{
		DImageList_Draw(icons->hImage, id, hdcDst, x, y, fStyle);
	} else{
		RECT box;
		box.left = x;
		box.top = y;
		box.right = icons->width;
		box.bottom = icons->height;

		if ( DxDrawAtlas_Check(DxDraw, cache) == FALSE ){
			LPVOID lpBits;
			BITMAPINFO bmi;
			HBITMAP hbmp;
			HGDIOBJ hOldBmp;
			HDC hDC;

			memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = box.right;
			bmi.bmiHeader.biHeight = -box.bottom;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			hbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);

			{
				DWORD *bp = (DWORD *)lpBits;
				int i;
				for ( i = box.right * box.bottom; i; i-- ){
					*bp++ = 0xfffffffe;
				}
			}

			hDC = CreateCompatibleDC(NULL);
			hOldBmp = SelectObject(hDC, hbmp);

			DImageList_Draw(icons->hImage, id, hDC, 0, 0, fStyle);

			SelectObject(hDC, hOldBmp);
			DeleteDC(hDC);

			{
				DWORD *bp = (DWORD *)lpBits;
				int i;

				for ( i = box.right * box.bottom; i; i-- ){
					if ( *bp == 0xfffffffe ){
						*bp = 0;
					} else{
						*bp = *bp | 0xff000000;
					}
					bp++;
				}
			}

			bmi.bmiHeader.biHeight = box.bottom;
			DxDrawAtlas(DxDraw, &bmi.bmiHeader, lpBits, &box);
			DeleteObject(hbmp); // ビットマップのメモリも削除
		} else{
			DxDrawAtlas(DxDraw, NULL, NULL, &box);
		}
	}
#endif
#if ICONDEBUG
	DrawIconListS(icons, base, id, hdcDst, x, y, 'o');
#endif
	return TRUE;
}

// フォーカス無し時のグレー処理 ===============================================
// いくらか色を残したグレーを生成する
#define GRAYTYPE 1
#if GRAYTYPE
// (128, 128, 128) に近づける
#define GetGraySubColorF(subcolor) ((subcolor + 128 * 5) / 6)
#define GetGraySubColorB(subcolor) ((subcolor * 3 + 128 ) / 4)
#else
// 色彩をなくす
#define GetGraySubColorF(subcolor, gray) ((subcolor + gray) / 4)
#define GetGraySubColorB(subcolor, gray) ((subcolor * 9 + gray ) / 12)
#endif

COLORREF GetGrayColorF(COLORREF color)
{
#if GRAYTYPE
	return RGB(
		GetGraySubColorF(GetRValue(color)),
		GetGraySubColorF(GetGValue(color)),
		GetGraySubColorF(GetBValue(color)));
#else
	DWORD gray;
	gray = GetRValue(color) + GetGValue(color) + GetBValue(color);

	return RGB(
		GetGraySubColorF(GetRValue(color), gray),
		GetGraySubColorF(GetGValue(color), gray),
		GetGraySubColorF(GetBValue(color), gray));
#endif
}

COLORREF GetGrayColorB(COLORREF color)
{
#if GRAYTYPE
	return RGB(
		GetGraySubColorB(GetRValue(color)),
		GetGraySubColorB(GetGValue(color)),
		GetGraySubColorB(GetBValue(color)));
#else
	DWORD gray;
	gray = GetRValue(color) + GetGValue(color) + GetBValue(color);
	return RGB(
		GetGraySubColorB(GetRValue(color), gray),
		GetGraySubColorB(GetGValue(color), gray),
		GetGraySubColorB(GetBValue(color), gray));
#endif
}

void DrawColumnFill(DISPSTRUCT *disp, int fmtlen)
{
	disp->lbox.left = disp->LP.x;
	disp->lbox.right = disp->lbox.left + fmtlen * disp->fontX;
	if ( disp->hfbr != NULL ){
		DxFillBack(disp->DxDraw, disp->hDC, &disp->lbox, disp->hfbr);
	}
	disp->Xd += fmtlen * disp->fontX;
	DxMoveToEx(disp->DxDraw, disp->hDC, disp->Xd, disp->lbox.top);
}

void DrawColumn(DISPSTRUCT *disp, DISPFMT_COLUMN *dfc)
{
	int len, fmtlen, displen;
	COLUMNDATASTRUCT cds, *cdsptr = NULL;
	const TCHAR *text = NULL, *p;
	WORD itemindex;
	PPC_APPINFO *cinfo = disp->cinfo;

	fmtlen = dfc->width;
	itemindex = dfc->itemindex;

	if ( disp->cell->cellcolumn < CCI_NOLOAD ){
		DrawColumnFill(disp, fmtlen);
		return;
	}
	if ( itemindex == DFC_UNDEF ){ // 未初期化
		itemindex = dfc->itemindex = GetColumnExtItemIndex(cinfo, dfc->name);
	}
	if ( disp->cell->cellcolumn < 0 ){ // 未取得
		if ( (disp->cell->attr & (ECA_PARENT | ECA_THIS)) ||
			(disp->cell->state < ECS_NORMAL) ){
			disp->cell->cellcolumn = CCI_NODATA;
			DrawColumnFill(disp, fmtlen);
			return;
		}
	} else{ // 既存のを取得
		EnterCellEdit(cinfo);
		cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + disp->cell->cellcolumn);
		for ( ; ; ){
			if ( (WORD)cdsptr->itemindex == itemindex ){
				if ( cdsptr->textoffset ){
					text = (TCHAR *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->textoffset);
				} else if ( itemindex < DFC_COMMENTEX ){ // column
					text = StrLoad; // まだ読み込んでいない
					if ( !(cinfo->SubTCmdFlags & SUBT_GETCOLUMNEXT) ){
						setflag(cinfo->SubTCmdFlags, SUBT_GETCOLUMNEXT);
						SetEvent(cinfo->SubT_cmd);
					}
				} else{ // ex. comment column
					text = NilStr;
				}
				break;
			}
			if ( cdsptr->nextoffset == 0 ) break;
			cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->nextoffset);
		}
		LeaveCellEdit(cinfo);
	}
	if ( text == NULL ){ // 新規取得
		if ( itemindex == DFC_FAULT ){ // 不明
			text = StrNoC;
		} else if ( itemindex < DFC_COMMENTEX ){ // system column
			EnterCellEdit(cinfo);
			if ( cdsptr == NULL ){ // 1つめ
				disp->cell->cellcolumn = cinfo->ColumnData.top;
			} else{ // 2つめ以降
				cdsptr->nextoffset = cinfo->ColumnData.top;
			}
			cds.nextoffset = 0;
			cds.itemindex = itemindex;
			cds.textoffset = 0;
			ThAppend(&cinfo->ColumnData, &cds, sizeof(COLUMNDATASTRUCT));
			LeaveCellEdit(cinfo);

			text = StrLoad;
			if ( !(cinfo->SubTCmdFlags & SUBT_GETCOLUMNEXT) ){
				setflag(cinfo->SubTCmdFlags, SUBT_GETCOLUMNEXT);
				SetEvent(cinfo->SubT_cmd);
			}
		} else{ // ex. comment column
			DWORD commentid, commentidflag;

			text = NilStr;
			commentid = DFC_COMMENTEX_MAX - itemindex + 1;
			commentidflag = 1 << commentid;
			if ( !(cinfo->UseCommentsFlag & commentidflag) ){
				setflag(cinfo->UseCommentsFlag, commentidflag);
				PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND,
					KC_COMMENTEVENT, commentid);
			}
		}
	}
	if ( *text == '\0' ){
		DrawColumnFill(disp, fmtlen);
		return;
	}
	len = tstrlen32(text);
	displen = min(len, fmtlen);
	p = text;
	while ( *p == ' ' ) p++;
	if ( Isdigit(*p) ){ // 数値っぽい文字列なら右詰に
	#ifndef USEDIRECTX
		if ( fmtlen > displen ){
			DrawColumnFill(disp, fmtlen - displen);
			fmtlen = displen;
		}
		DrawTextRight(disp, text, displen);
	#else
		disp->backbox.left = disp->Xd;
		disp->backbox.right = disp->Xd + len * disp->fontX;
		DxDrawText(disp->DxDraw, disp->hDC, text, displen, &disp->backbox,
				GEN_DRAW_FLAGS | DT_RIGHT);
	#endif
	} else{
	#if !defined(UNICODE) && !defined(USEDIRECTX)
		if ( !IsTrue(UsePFont) ){
			DxTextOutRel(disp->DxDraw, disp->hDC, text, displen);
		} else
		#endif
		{
			disp->backbox.left = disp->Xd;
			disp->backbox.right = disp->Xd + fmtlen * disp->fontX;
			DxDrawText(disp->DxDraw, disp->hDC, text, -1, &disp->backbox,
					GEN_DRAW_FLAGS);
		}
	}
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	disp->Xd += fmtlen * disp->fontX;
}

void DrawCommentEx(DISPSTRUCT *disp, const BYTE *fmt)
{
	DISPFMT_COLUMN dfc;

	dfc.width = *fmt;
	dfc.itemindex = (WORD)(DFC_COMMENTEX_MAX - (*(fmt + 1) - 1));
	DrawColumn(disp, &dfc);
}

// 画像表示 ===================================================================
// 選択表示を行う
#ifndef USEDIRECTX
BOOL TImageList_SelectDraw(PPC_APPINFO *cinfo, ICONCACHESTRUCT *icons, int index, CTC_DXD HDC hDC, int x, int y, int imgsizeX, int imgsizeY)
{
	HBITMAP hTempBMP;
	HGDIOBJ hOldBMP;
	DWORD *bitmap = NULL, *bit;
	BITMAPINFO bmpinfo;
	HDC hTempDC;
	DWORD size;
	DWORD selcolor = C_eInfo[ECS_SELECT];
	DWORD R, G, B;
	BOOL result;

	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.bmiHeader.biWidth = imgsizeX;
	bmpinfo.bmiHeader.biHeight = imgsizeY;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = 32;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;

	hTempDC = CreateCompatibleDC(hDC);
	hTempBMP = CreateDIBSection(hTempDC, &bmpinfo, DIB_RGB_COLORS, (void **)&bitmap, NULL, 0);
	if ( hTempBMP == NULL ){
		DeleteDC(hTempDC);
		return DrawIconList(cinfo, icons, index, CTC_DXDP hDC, x, y, ILD_SELECTED);
	}

	hOldBMP = SelectObject(hTempDC, hTempBMP);
	result = DrawIconList(cinfo, icons, index, CTC_DXDP hTempDC, 0, 0, ILD_NORMAL);
	SelectObject(hTempDC, hOldBMP);
	DeleteDC(hTempDC);

	size = imgsizeX * imgsizeY;
	bit = bitmap;
	R = GetBValue(selcolor);	// bitmapとCOLORREFとは色の並びが違う
	G = GetGValue(selcolor);
	B = GetRValue(selcolor);
	while ( size-- ){
		DWORD bitc;

		bitc = *bit;
		*bit++ = RGB((R + GetRValue(bitc)) / 2, (G + GetGValue(bitc)) / 2, (B + GetBValue(bitc)) / 2);
	}

	SetDIBitsToDevice(hDC, x, y, imgsizeX, imgsizeY,
		0, 0, 0, imgsizeY, bitmap, &bmpinfo, DIB_RGB_COLORS);
	DeleteObject(hTempBMP);
	return result;
}
#endif

void InitCellIcon(PPC_APPINFO *cinfo, const BYTE *fmt)
{
	int sizeX, sizeY;

	if ( DImageList_Draw == NULL ){ // 初めての呼び出しなら関数準備
		LoadWinAPI(NULL, LoadCommonControls(0), ImageCtlDLL, LOADWINAPI_HANDLE);
	}

	if ( *fmt == DE_IMAGE ){
		GetCustData(T("XC_ocig"), &XC_ocig, sizeof(XC_ocig));
		sizeX = cinfo->fontX * fmt[1];
		sizeY = cinfo->fontY * fmt[2];
		if ( (fmt[3] == DE_END) || (fmt[3] == DE_BLANK) ) sizeY -= cinfo->fontY;

		if ( sizeX < 1 ) sizeX = 1;
		if ( sizeY < 1 ) sizeY = 1;
	} else{
		if ( *fmt == DE_ICON2 ){
			sizeX = sizeY = max(fmt[1], 1);
			if ( cinfo->X_textmag != 100 ){
				sizeX = sizeY = (sizeX * cinfo->X_textmag) / 100;
			}
			if ( cinfo->FontDPI != DEFAULT_WIN_DPI ){
				sizeX = sizeY = (sizeX * cinfo->FontDPI) / DEFAULT_WIN_DPI;
			}

		} else{ // DE_ICON1
			sizeX = cinfo->fontX * 2 - ICONBLANK;
			sizeY = cinfo->X_lspc ? cinfo->fontY : cinfo->fontY - ICONBLANK;
		}
		if ( sizeX < 1 ) sizeX = 1;
		if ( sizeY < 1 ) sizeY = 1;

		if ( ((cinfo->X_textmag == 100) && (cinfo->dset.cellicon < DSETI_OVLNOC)) &&
			((CacheIcon.hImage == NULL) ||
			((CacheIconsX == sizeX) && (CacheIconsY == sizeY))) ){
			CacheIconsX = sizeX;
			CacheIconsY = sizeY;
			cinfo->EntryIcons.hImage = INVALID_HANDLE_VALUE; // CacheIcon を使用
			return;
		}
	}
	cinfo->EntryIconGetSize = sizeY;
	CreateIconList(cinfo, &cinfo->EntryIcons, sizeX, sizeY);
}

void PaintEntryCounts(DISPSTRUCT *disp)
{
	SIZE32_T len;
	TCHAR buf[64];
	PPC_APPINFO *cinfo = disp->cinfo;

	len = wsprintf(buf, T("%3u/%3u"), cinfo->e.cellIMax, cinfo->e.cellDataMax);
	DrawNumber(disp, buf, len);
	disp->Xd += len * disp->fontX;
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
}

void RequestGetIcon(PPC_APPINFO *cinfo /* , ICONCACHESTRUCT *icons */)
{
	if ( cinfo->SubTCmdFlags & SUBT_GETCELLICON ) return; // 要求済み
/*
	// 確保できたイメージの数が画面表示数未満の時、
	if ( (icons->hImage != NULL) && (icons->alloc == FALSE) &&
		 (icons->size < cinfo->cel.Area.cx * cinfo->cel.Area.cy) ){
		// カーソル位置のアイコンが表示できないときに限り、リクエストを行う
		int id = CEL(cinfo->e.cellN).icon;
		if ( (id >= icons->minID) || (id <= icons->maxID) ){
			return;
		}
	}
*/
	setflag(cinfo->SubTCmdFlags, SUBT_GETCELLICON);
	SetEvent(cinfo->SubT_cmd);
}

void CellImageIconList(DISPSTRUCT *disp, ICONCACHESTRUCT *icons, int cellicon, BOOL CheckMark, int boxtop, int imgsizeX, int imgsizeY)
{
	PPC_APPINFO *cinfo = disp->cinfo;
	HDC hDC = disp->hDC;
	RECT tempbox;

	if ( CheckMark == FALSE ){ // 通常
		if ( IsTrue(DrawIconList(cinfo, icons, cellicon,
				DIRECTXARG(&disp->cell->iconcache) CTC_DXP hDC,
				disp->Xd, boxtop, ILD_NORMAL)) ){
			return;
		}
		// 代用描画
		if ( !disp->NoBack && (icons->maskmode == ICONLIST_NOMASK) ){
			tempbox.left = disp->Xd;
			tempbox.top = boxtop;
			tempbox.right = tempbox.left + imgsizeX;
			tempbox.bottom = tempbox.top + imgsizeY;

			DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hback);
		}
	} else{ // マーク反転
		#ifndef USEDIRECTX
			if ( IsTrue(TImageList_SelectDraw(cinfo, icons, cellicon,
					CTC_DXP hDC, disp->Xd, boxtop, imgsizeX, imgsizeY)) ){
				return;
			}
		#else
			if ( IsTrue(DrawIconList(cinfo, icons, cellicon,
					DIRECTXARG(&disp->cell->iconcache) CTC_DXP hDC,
					disp->Xd, boxtop, ILD_SELECTED)) ){
				// 反転描画
				tempbox.left = disp->Xd;
				tempbox.top = boxtop;
				tempbox.right = tempbox.left + imgsizeX;
				tempbox.bottom = tempbox.top + imgsizeY;

				DxDrawBack(disp->DxDraw, hDC, &tempbox,
					C_eInfo[ECS_SELECT] | 0x80000000);
				return;
			}
		#endif
	}
	disp->cell->icon = ICONLIST_NOINDEX;
	RequestGetIcon(cinfo);
}

void PaintImageIcon(DISPSTRUCT *disp, const BYTE *fmt, int CheckM, int Ytop, int Ybtm, const XC_CFMT *cfmt)
{
	RECT box;
	PPC_APPINFO *cinfo = disp->cinfo;
	HDC hDC = disp->hDC;

	box.top = Ytop;
	box.bottom = Ybtm;
										// イメージリスト未作成なら作成指示
	if ( cinfo->EntryIcons.hImage == NULL ) InitCellIcon(cinfo, fmt - 1);
	box.right = disp->Xd + disp->fontX * *fmt;
										// 画像がないときは読み込み要求
	if ( disp->cell->icon < 0 ){
		if ( disp->cell->icon == ICONLIST_NOINDEX ){
			//RequestGetIcon(cinfo, &cinfo->EntryIcons);
			RequestGetIcon(cinfo);
		}
		box.left = disp->Xd;
		if ( !disp->NoBack ) DxFillBack(disp->DxDraw, hDC, &box, disp->hback);
		if ( disp->cell->icon == ICONLIST_LOADERROR ){
			DxMoveToEx(disp->DxDraw, hDC,
				box.left + (box.right - box.left - disp->fontX) / 2,
				box.top + (box.bottom - box.top - cinfo->fontY) / 2);
			DxTextOutRel(disp->DxDraw, hDC, ErrorString, 1);
		}
		if ( CheckM ){ IfGDImode(hDC) InvertRect(hDC, &box); }
	} else{	// 画像あるので表示
		if ( (cinfo->EntryIcons.maskmode != ICONLIST_NOMASK) && (disp->hfbr != NULL) ){ // 背景色変更中の時、背景描画
			box.left = disp->Xd;
			DxFillBack(disp->DxDraw, hDC, &box, disp->hfbr);
		}
		CellImageIconList(disp, &cinfo->EntryIcons, disp->cell->icon, CheckM, box.top, (box.right - disp->Xd), (box.bottom - box.top));
		if ( CheckM ) IfGDImode(hDC)
		{// "*" 表示
			HBRUSH hFrameBrush;
			RECT fbox;

			fbox.top = box.top + 2;
			fbox.left = disp->Xd + 2;
			fbox.right = box.right - 2;
			fbox.bottom = box.bottom - 2;
			if ( (*(fmt + 2) == DE_END) || (*(fmt + 2) == DE_BLANK) ){
				fbox.bottom -= cinfo->fontY;
			}
			hFrameBrush = CreateSolidBrush(C_eInfo[ECS_SELECT]);
			FrameRect(hDC, &fbox, hFrameBrush);
			DeleteObject(hFrameBrush);

			DxSetTextColor(disp->DxDraw, hDC, C_eInfo[CheckM]);
			DxSetBkColor(disp->DxDraw, hDC, cinfo->BackColor);
			DxTextOutRel(disp->DxDraw, hDC, MarkString, 1);
			DxGetCurrentPositionEx(disp->DxDraw, hDC, &disp->LP);
			DxSetTextColor(disp->DxDraw, hDC, disp->fc);
			DxSetBkColor(disp->DxDraw, hDC, disp->bc);
		}
		// LeaveCellEdit(cinfo);
	}
#ifdef USEDIRECTX
	if ( CheckM ) IfDXmode(hDC)
	{
		box.left = disp->Xd;
		DxDrawBack(disp->DxDraw, hDC, &box, C_eInfo[ECS_SELECT] | 0x80000000);
	}
#endif

// 画像のみ表示か、DE_BLANK ならファイル名表示を行う
	if ( (*(fmt + 2) == DE_END) || (*(fmt + 2) == DE_BLANK) ){
		box.left = disp->Xd;
		box.top += cinfo->fontY * (*(fmt + 1) - 1);

		if ( !disp->NoBack && (disp->lspc > 1) ){
			box.top += cinfo->fontY - disp->lspc;
			box.bottom -= disp->lspc / 2;
			DxFillBack(disp->DxDraw, hDC, &box, disp->hbbr);
			box.top -= cinfo->fontY - disp->lspc;
			box.bottom += disp->lspc / 2;
		}
		box.left += disp->IsCursor ? 1 : 0;
		DxMoveToEx(disp->DxDraw, hDC, box.left, box.top);

		SetDelayEntryTip(disp, disp->cell->f.cFileName, disp->Xd / disp->fontX, disp->cell->ext);

		// ※ DrawTextもSetTextAlignの影響を受ける
		DxDrawText(disp->DxDraw, hDC, disp->cell->f.cFileName, -1, &box,
				NAME_DRAW_FLAGS);

		if ( !disp->NoBack ){
			DxGetCurrentPositionEx(disp->DxDraw, hDC, &disp->LP);
			box.left = disp->LP.x;
			DxFillBack(disp->DxDraw, hDC, &box, disp->hbbr);
		}
	} else{
		disp->LineTopX += disp->fontX * *fmt;
	}
	disp->LP.x = disp->Xd = box.right;

	if ( (cfmt->nextline == 0) && (cfmt->width > (*fmt + 1) ) && !disp->NoBack ){ // １行 and 右に何か表示時は右下部を消去
		box.left = disp->Xd;
		box.right = disp->backbox.right;
		box.top = disp->backbox.top + cinfo->fontY - disp->lspc - 1;
		box.bottom = disp->backbox.bottom;
		if ( box.top < box.bottom ){
			DxFillBack(disp->DxDraw, hDC, &box, disp->hbbr);
		}
	}
}

void PaintIcon(DISPSTRUCT *disp, int imgsizeX, int imgsizeY, int CheckM, int Xe, int height)
{
	int cellicon, boxtop, icon_blank_height;
	HDC hDC = disp->hDC;
	RECT tempbox;
	PPC_APPINFO *cinfo = disp->cinfo;
	ICONCACHESTRUCT *icons;

	icons = (cinfo->EntryIcons.hImage == INVALID_HANDLE_VALUE) ?
		&CacheIcon : &cinfo->EntryIcons;

	if ( imgsizeY > imgsizeX ) imgsizeY = imgsizeX;

	icon_blank_height = disp->backbox.bottom - disp->backbox.top - imgsizeY;
	if ( icon_blank_height < 0 ) icon_blank_height = 0;
	boxtop = disp->backbox.top + (icon_blank_height / 2);

	if ( (icons->maskmode != ICONLIST_NOMASK) && (disp->hfbr != NULL) ){ // 背景色変更中の時、背景描画
		// アイコン背景の補修
		tempbox.left = disp->Xd;
		tempbox.top = disp->backbox.top;
		tempbox.right = tempbox.left + imgsizeX;
		tempbox.bottom = disp->backbox.bottom;
		DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hfbr);
	}

	cellicon = disp->cell->icon;
	if ( cellicon < 0 ){ // アイコン無し
		HICON hIcon;
										// アイコンがないときは読み込み要求
		if ( cellicon == ICONLIST_NOINDEX ){
			RequestGetIcon(cinfo);
		}
		if ( disp->cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			hIcon = LoadDefaultDirIcon(cinfo, cinfo->EntryIconGetSize);
		} else{
			hIcon = LoadUnknownIcon(cinfo, cinfo->EntryIconGetSize);
		}
	#ifdef USEDIRECTX
		IfGDImode(hDC)
		#endif
		{
			if ( CheckM ){
				tempbox.left = disp->Xd;
				tempbox.top = boxtop;
				tempbox.right = tempbox.left + imgsizeX;
				tempbox.bottom = tempbox.top + imgsizeY;
				DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hback);
			}
			DrawIconEx(hDC, disp->Xd, boxtop, hIcon, imgsizeX, imgsizeY, 0,
				(disp->NoBack || CheckM) ? NULL :
				((C_eInfo[ECS_EVEN] == C_AUTO) ? disp->hback : disp->hbbr),
				DI_NORMAL);
		}
	#ifdef USEDIRECTX
		else{
			tempbox.left = disp->Xd;
			tempbox.top = boxtop;
			tempbox.right = imgsizeX;
			tempbox.bottom = imgsizeY;
			DxDrawIcon(disp->DxDraw, hIcon, &tempbox, NOTIMPCACHE);
		}
	#endif
		if ( CheckM ){
			tempbox.left = disp->Xd;
			tempbox.top = boxtop;
			tempbox.right = tempbox.left + imgsizeX;
			tempbox.bottom = tempbox.top + imgsizeY;
		#ifndef USEDIRECTX
			InvertRect(hDC, &tempbox);
		#else
			IfGDImode(hDC)
			{
				InvertRect(hDC, &tempbox);
			} else{
				DxDrawBack(disp->DxDraw, hDC, &tempbox, C_eInfo[ECS_SELECT] | 0x80000000);
			}
		#endif
		}
	} else{ // アイコンあり
		// EnterCellEdit(cinfo); 表示時の同期は不要らしい
		CellImageIconList(disp, icons, cellicon, CheckM, boxtop, imgsizeX, imgsizeY);
	}
										// 未描画部分の描画
	if ( !disp->NoBack ){
		if ( (icons->maskmode == ICONLIST_NOMASK) || (disp->hfbr == NULL) ){
			// アイコン上の補修
			tempbox.left = disp->Xd;
			tempbox.right = disp->Xd + imgsizeX;
			tempbox.top = disp->backbox.top;
			tempbox.bottom = boxtop;
			DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hback);

			if ( (tempbox.top + imgsizeY) < disp->backbox.bottom ){ // アイコン下の補修
				// tempbox.left = disp->Xd;
				// tempbox.right = disp->Xd + imgsizeX;
				tempbox.top = boxtop + imgsizeY;
				tempbox.bottom = disp->backbox.bottom;
				DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hback);
			}
		}
		// アイコン右の補修
		tempbox.left = disp->Xd + imgsizeX;
		tempbox.right = tempbox.left + ICONBLANK;
		tempbox.top = disp->backbox.top;
		tempbox.bottom = disp->backbox.bottom;
		DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hbbr);
	}
	disp->LineTopX = disp->LP.x = (disp->Xd += imgsizeX + ICONBLANK);

	{
		int blankheight = imgsizeY - (height * cinfo->fontY);
												// アイコン以外のセル上部を消去
		if ( !disp->NoBack ){
			tempbox.left = disp->Xd;
			tempbox.right = Xe;
			tempbox.top = disp->backbox.top;
			tempbox.bottom = tempbox.top + ((blankheight + disp->lspc) >> 1);
			if ( tempbox.top < tempbox.bottom ){
				DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hbbr);
			}
			if ( height == 1 ){ // 下部を消去
				tempbox.top = tempbox.bottom + cinfo->fontY - disp->lspc - 1;
				tempbox.bottom = disp->backbox.bottom;
				if ( tempbox.top < tempbox.bottom ){
					DxFillBack(disp->DxDraw, hDC, &tempbox, disp->hbbr);
				}
			}
		}
		if ( blankheight >= 2 ){ // 以降を下げる
			blankheight >>= 1;
			disp->backbox.top += blankheight;
			disp->lbox.top += blankheight;
			disp->lbox.bottom += blankheight;
		}
	}
}

#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
#define S_IXGRP  00010
#define S_IXOTH  00001
#endif
#ifndef S_IFBLK
#define S_IFIFO  0010000
#define S_IFBLK  0060000
#define S_IXUSR  00100
#endif

void MakeStatString(TCHAR *buf, DWORD FileStat, int stateID, int displen)
{
	int i;
	DWORD flags;
	TCHAR c;

	tstrcpy(buf + 1, StatString);
								// 属性
	if ( stateID != ECS_NORMAL ){
		c = AttributeString[stateID];
	} else{
		switch ( FileStat & S_IFMT ){
			case S_IFSOCK:
				c = 's';
				break;
			case S_IFLNK:
				c = 'l';
				break;
			case S_IFREG:
				c = '-';
				break;
			case S_IFBLK:
				c = 'b';
				break;
			case S_IFDIR:
				c = 'd';
				break;
			case S_IFCHR:
				c = 'c';
				break;
			case S_IFIFO:
				c = 'p';
				break;
			default:
				c = '?';
				break;
		}
	}
	buf[0] = c;
									// グループ毎属性
	flags = FileStat;
	for ( i = 9; i >= 1; i-- ){
		if ( !(flags & LSBIT) ) buf[i] = '-';
		flags >>= 1;
	}
	if ( flags & S_ISVTX ) buf[9] = (TCHAR)((flags & S_IXOTH) ? 't' : 'T'); // sticky
	if ( flags & S_ISGID ) buf[6] = (TCHAR)((flags & S_IXGRP) ? 's' : 'S'); // setgid
	if ( flags & S_ISUID ) buf[3] = (TCHAR)((flags & S_IXUSR) ? 's' : 'S'); // setuid

	if ( displen > 10 ){
		wsprintf(buf + 10, T(" %d%d%d%d"),
			(flags >> 9) & 7,
			(flags >> 6) & 7,
			(flags >> 3) & 7,
			flags & 7);
	}
}

void USEFASTCALL MakeAttributesString(TCHAR *buf, ENTRYCELL *cell)
{
	DWORD FileAttributes;
	int stateID;

	tstrcpy(buf, AttributeNULLString);

	FileAttributes = cell->f.dwFileAttributes;
	if ( FileAttributes & FILE_ATTRIBUTE_READONLY )	buf[0] = 'R';
	if ( FileAttributes & FILE_ATTRIBUTE_HIDDEN )	buf[1] = 'H';
	if ( FileAttributes & FILE_ATTRIBUTE_SYSTEM )	buf[2] = 'S';
	if ( FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_LABEL | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_VIRTUAL) ){
		if ( FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
			buf[3] = 'P';
		} else if ( FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE ){
			buf[3] = 'f';
		} else if ( FileAttributes & FILE_ATTRIBUTE_VIRTUAL ){
			buf[3] = 'V';
		} else{
			buf[3] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_LABEL) ? 'L' : 'x');
		}
	}
	if ( FileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
		buf[4] = 'D';
	}
	if ( FileAttributes & FILE_ATTRIBUTE_ARCHIVE ) buf[5] = 'A';
	if ( FileAttributes & (FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) ){
		if ( FileAttributes & (FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) ){
			buf[6] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_RECALL_ON_OPEN) ? 'g' : 'w');
		} else{
			buf[6] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_TEMPORARY) ? 'T' : 'o');
		}
	}
	if ( FileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_INTEGRITY_STREAM | FILE_ATTRIBUTE_NO_SCRUB_DATA | FILE_ATTRIBUTE_PINNED | FILE_ATTRIBUTE_UNPINNED) ){
		if ( FileAttributes & (FILE_ATTRIBUTE_PINNED | FILE_ATTRIBUTE_UNPINNED) ){
			buf[7] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_PINNED) ? 'k' : 'u');
		} else if ( FileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED) ){
			buf[7] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_COMPRESSED) ? 'C' : 'e');
		} else{
			buf[7] = (TCHAR)((FileAttributes & FILE_ATTRIBUTE_INTEGRITY_STREAM) ? 'i' : 'B');
		}
	}
	if ( FileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) buf[8] = 'n';

	stateID = cell->state & ECS_STATEMASK;
	if ( cell->attr & ECA_GRAY ) stateID = ECS_GRAY;
	buf[9] = AttributeString[stateID];
}

const TCHAR JapaneseErasPath[] = T("SYSTEM\\CurrentControlSet\\Control\\Nls\\Calendars\\Japanese\\Eras"); // Windows7以降

const TCHAR ShortGENGOU[] = T("?MTSHR");
const TCHAR *LongGENGOU[] = {T("西"), T("明治"), T("大正"), T("昭和"), T("平成"), T("令和")};
const int OffsetGENGOU[] = {0, 1867, 1911, 1925, 1988, 2018};
DWORD ExtGENGOUyear = 0;

#define GENGOU_option_short 0
#define GENGOU_option_long 1
#define GENGOU_max_support 2019

void InitExtGENGOU(void)
{
	HKEY hRegEras;
	int cnt = 0;

	ExtGENGOUyear = MAX32;
	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, JapaneseErasPath, 0, KEY_READ, &hRegEras) ){
		return;
	}

	for ( ; ; cnt++ ){			// 設定を取り出す ---------------------
		DWORD keySize, valueSize;
		TCHAR keyName[MAX_PATH];	// "1989 01 08"
		TCHAR ErasValue[MAX_PATH];	// "平成_平_Heisei_H"
		DWORD Rtyp;
		const TCHAR *ptr;
		DWORD srdata;

		keySize = TSIZEOF(keyName);
		valueSize = TSIZEOF(ErasValue);
		if ( ERROR_SUCCESS != RegEnumValue(hRegEras, cnt, keyName, &keySize,
				NULL, &Rtyp, (BYTE *)ErasValue, &valueSize) ){
			break;
		}
		ptr = keyName;
		srdata = GetNumber(&ptr); // year
		if ( srdata <= GENGOU_max_support ) continue;

		ptr += 1; // '_' skip
		srdata = (srdata * 100) + GetNumber(&ptr); // month
		ptr += 1; // '_' skip
		srdata = (srdata * 100) + GetNumber(&ptr); // day
		if ( ExtGENGOUyear > srdata ) ExtGENGOUyear = srdata;
	}
	RegCloseKey(hRegEras);
}

int GetExtGENGOU(const SYSTEMTIME *sTime, TCHAR *dest, const TCHAR option)
{
	HKEY hRegEras;
	int cnt = 0, len = 0;
	DWORD srTime;

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, JapaneseErasPath, 0, KEY_READ, &hRegEras) ){
		return 0;
	}
	if ( option <= ' ' ){
		srTime = (((DWORD)sTime->wYear) * 10000) + (sTime->wMonth * 100) + sTime->wDay;
	}

	for ( ; ; cnt++ ){				// 設定を取り出す ---------------------
		DWORD keySize, valueSize;
		TCHAR keyName[MAX_PATH];	// "1989 01 08"
		TCHAR ErasValue[MAX_PATH];	// "平成_平_Heisei_H"
		DWORD Rtyp;
		const TCHAR *ptr;
		DWORD srdata;

		keySize = TSIZEOF(keyName);
		valueSize = TSIZEOF(ErasValue);
		if ( ERROR_SUCCESS != RegEnumValue(hRegEras, cnt, keyName, &keySize,
				NULL, &Rtyp, (BYTE *)ErasValue, &valueSize) ){
			break;
		}
		if ( option <= ' ' ){
			ptr = keyName;
			srdata = GetNumber(&ptr); // year
			ptr += 1; // '_' skip
			srdata = (srdata * 100) + GetNumber(&ptr); // month
			ptr += 1; // '_' skip
			srdata = (srdata * 100) + GetNumber(&ptr); // day
			if ( srTime >= srdata ){
				if ( option == GENGOU_option_short ){
					len = wsprintf(dest, T("%c%02d"),
						ErasValue[(6 / sizeof(TCHAR)) + 2],
						sTime->wYear - srdata / 10000 + 1);
				} else{
					ErasValue[4 / sizeof(TCHAR)] = '\0';
					len = wsprintf(dest, T("%s%02d"),
						ErasValue, sTime->wYear - srdata / 10000 + 1);
				}
			}
		} else{
			if ( option == ErasValue[(6 / sizeof(TCHAR)) + 2] ){
				ptr = keyName;
				len = GetNumber(&ptr) - 1; // year
				break;
			}
		}
	}
	RegCloseKey(hRegEras);
	return len;
}

int GetGENGOU(const SYSTEMTIME *sTime, TCHAR *dest, const TCHAR option)
{
	DWORD srTime = (((DWORD)sTime->wYear) * 10000) + (sTime->wMonth * 100) + sTime->wDay;
	int GENGOUoffset;

	if ( srTime >= ExtGENGOUyear ){
		if ( ExtGENGOUyear == 0 ){
			InitExtGENGOU();
			if ( srTime < ExtGENGOUyear ) return GetGENGOU(sTime, dest, option);
		}
		return GetExtGENGOU(sTime, dest, option);
	} else if ( srTime < 20190501 ){
		if ( srTime >= 19890108 ){ // 平成
			GENGOUoffset = 4;
		}else if ( srTime >= 19261225 ){ // 昭和
			GENGOUoffset = 3;
		} else if ( srTime >= 19120730 ){ // 大正
			GENGOUoffset = 2;
		} else if ( srTime >= 18680908 ){ // 明治
			GENGOUoffset = 1;
		} else{ // 明治より前
			GENGOUoffset = 0;
		}
	} else{ // 令和
		GENGOUoffset = 5;
	}

	if ( option == GENGOU_option_short ){
		return wsprintf(dest, T("%c%02d"), ShortGENGOU[GENGOUoffset], sTime->wYear - OffsetGENGOU[GENGOUoffset]);
	} else{
		return wsprintf(dest, T("%s%02d"), LongGENGOU[GENGOUoffset], sTime->wYear - OffsetGENGOU[GENGOUoffset]);
	}
}

int GetUpperGENGOU(const SYSTEMTIME *sTime, TCHAR *dest, const TCHAR option, const TCHAR type)
{
	int GENGOUoffset = ((sizeof(ShortGENGOU) / sizeof(TCHAR)) - 2);

	for (;;){
		if ( (type == ShortGENGOU[GENGOUoffset]) || (GENGOUoffset == 0) ){
			break;
		}
		GENGOUoffset--;
	}

	if ( option == GENGOU_option_short ){
		return wsprintf(dest, T("%c%02d"), ShortGENGOU[GENGOUoffset], sTime->wYear - OffsetGENGOU[GENGOUoffset]);
	} else{
		return wsprintf(dest, T("%s%02d"), LongGENGOU[GENGOUoffset], sTime->wYear - OffsetGENGOU[GENGOUoffset]);
	}
}

// 時刻表示 ===================================================================
void ShowTimeFormat(DISPSTRUCT *disp, const BYTE **format)
{
	WIN32_FIND_DATA *file;
	FILETIME lTime, *target;
	SYSTEMTIME sTime;
	TCHAR *p, buf[0x80];
	const BYTE *fmt;
	int displen;
#ifdef UNICODE
	int extdisplen = 0;
#endif
	fmt = *format;
	file = &disp->cell->f;
	switch ( *fmt++ ){
		case 0:
			target = &file->ftCreationTime;
			break;
		case 1:
			target = &file->ftLastAccessTime;
			break;
		default:
			target = &file->ftLastWriteTime;
	}
	FileTimeToLocalFileTime(target, &lTime);
	FileTimeToSystemTime(&lTime, &sTime);
	p = buf;
	while ( *fmt != '\0' ){
		switch ( *fmt++ ){
		// 年
			case 'y':
				p += wsprintf(p, T("%02d"), sTime.wYear % 100);
				break;
			case 'Y':
				p += wsprintf(p, T("%04d"), sTime.wYear % 10000);
				break;
			case 'g':
				p += GetGENGOU(&sTime, p, GENGOU_option_short);
				break;
			case 'G':
				p += GetGENGOU(&sTime, p, GENGOU_option_long);
			#ifdef UNICODE
				extdisplen += 4 / 2;
			#endif
				break;
			case 'j':
				if ( *fmt == '\0' ) break;
				p += GetUpperGENGOU(&sTime, p, GENGOU_option_short, *fmt++);
				break;
			case 'J':
				if ( *fmt == '\0' ) break;
				p += GetUpperGENGOU(&sTime, p, GENGOU_option_long, *fmt++);
			#ifdef UNICODE
				extdisplen += 4 / 2;
			#endif
				break;
		// 月
			case 'n':
				p += wsprintf(p, T("%2d"), sTime.wMonth);
				break;
			case 'N':
				p += wsprintf(p, T("%02d"), sTime.wMonth);
				break;
			case 'a':
				p = tstpcpy(p, EMonth[sTime.wMonth - 1]);
				break;
		// 日
			case 'd':
				p += wsprintf(p, T("%2d"), sTime.wDay);
				break;
			case 'D':
				p += wsprintf(p, T("%02d"), sTime.wDay);
				break;
		// 週
			case 'w':
				p = tstpcpy(p, EWeek[sTime.wDayOfWeek]);
				break;
			case 'W':
				p = tstpcpy(p, JWeek[sTime.wDayOfWeek]);
			#ifdef UNICODE
				extdisplen += 2 / 2;
			#endif
				break;
		// 時
			case 'h':
				p += wsprintf(p, T("%2d"), sTime.wHour);
				break;
			case 'H':
				p += wsprintf(p, T("%02d"), sTime.wHour);
				break;
			case 'u': {
				int i;
				i = sTime.wHour;
				if ( i >= 12 ) i -= 12;
				p += wsprintf(p, T("%2d"), i);
				break;
			}
			case 'U': {
				int i;
				i = sTime.wHour;
				if ( i >= 12 ) i -= 12;
				p += wsprintf(p, T("%02d"), i);
				break;
			}
		// 分
			case 'm':
				p += wsprintf(p, T("%2d"), sTime.wMinute);
				break;
			case 'M':
				p += wsprintf(p, T("%02d"), sTime.wMinute);
				break;
		// 秒
			case 's':
				p += wsprintf(p, T("%2d"), sTime.wSecond);
				break;
			case 'S':
				p += wsprintf(p, T("%02d"), sTime.wSecond);
				break;
		// μ秒
			case 'I':
				p += wsprintf(p, T("%03d"), sTime.wMilliseconds);
				break;
		// 午
			case 't':
				p = tstpcpy(p, (sTime.wHour < 12) ? T("am") : T("pm"));
				break;
			case 'T':
				p = tstpcpy(p, (sTime.wHour < 12) ? T("午前") : T("午後"));;
			#ifdef UNICODE
				extdisplen += 4 / 2;
			#endif
				break;
			default:
				*p++ = *(fmt - 1);
		}
	}
	*format = fmt + 1;

	displen = p - buf;
	DxTextOutRel(disp->DxDraw, disp->hDC, buf, displen);
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
#ifdef UNICODE
	disp->Xd += (displen + extdisplen) * disp->fontX;
#else
	disp->Xd += displen * disp->fontX;
#endif
}

void DrawPathText(CTC_DXD HDC hDC, PPC_APPINFO *cinfo)
{
	TCHAR buf[VFPS * 2];
	int len;

	if ( cinfo->path[0] == ':' ){
		VFSFullPath(buf, CEL(cinfo->e.cellN).f.cFileName, NULL);
	} else{
		tstrcpy(buf, cinfo->path);
	}
	if ( cinfo->UseArcPathMask && cinfo->ArcPathMask[0] ){
		CatPath(NULL, buf, cinfo->ArcPathMask);
	}
	if ( XC_dpmk ) CatPath(NULL, buf, (cinfo->DsetMask[0] == MASK_NOUSE) ? cinfo->mask.file : cinfo->DsetMask);

	len = tstrlen32(buf);
	DxTextOutRel(DxDraw, hDC, buf, len);
}

void DrawPathMaskText(CTC_DXD HDC hDC, PPC_APPINFO *cinfo)
{
	const TCHAR *mask = (cinfo->DsetMask[0] == MASK_NOUSE) ? cinfo->mask.file : cinfo->DsetMask;

	DxTextOutRel(DxDraw, hDC, mask, tstrlen32(mask));
}

#ifdef UNICODE
int CCharWide(WCHAR c) //もう少し正しくするなら GetStringTypeEx C3_HALFWIDTH で。
{
									// 簡易決定 -------------------------------
	if ( c <= 0x390 ){				// 0000-0390 ASCII からギリシャ文字まで
		return 1;
	}
	if ( c < 0x4e00 ){				// 0391-4dff 漢字より前の記号類
		if ( c >= 0x3000 ){			// 3000-4dff カタカナ・ひらがな類
			return (c < 0x33ff) ? 2 : 1;
		}
		if ( c >= 0x2000 ){				// 2000-2fff 記号類
			if ( c >= 0x2103 ){
				return (c <= 0x27be) ? 2 : 1;	// 2103-27be 記号類2
			} else{
				return (c <= 0x203b) ? 2 : 1;	// 2000-203b 記号類1
			}
		}								// 0391-1fff
		if ( c <= 0x3c9 ) return 2;			// 0391-03c9 ギリシャ文字
		if ( c < 0x410 ) return 1;
		if ( c <= 0x451 ) return 2;			// 0410-0451 ロシア文字
		return 1;
	} else{							// 4e00-ffff 漢字など
		if ( c < 0xff01 ){
			if ( c <= 0x9fa5 ) return 2;	// 4e00-9fa5 漢字1
			if ( c >= 0xe000 ){
				if ( c <= 0xfa2d ) return 2; // e000-fa2d 漢字2
				return 1;
			}
			if ( c < 0xac00 ) return 1;
			if ( c <= 0xd7a3 ) return 2;	// ac00-d7a3 ハングル
			return 1;
		}
		if ( c <= 0xff5e ) return 2;	// ff01-ff5e 全角アルファベット
		if ( c < 0xffe0 ) return 1;		// ff5f-ffe0 半角カタカナ
		if ( c <= 0xffe6 ) return 2;	// ffe0-ffe6 通貨
		return 1;
	}
}
#endif

const TCHAR * FindDispLastEntry(const TCHAR *src)
{
	const TCHAR *rp, *tp;
	TCHAR rpchr;

	rp = src;
	rpchr = *rp;
	if ( (rpchr == '/') || (rpchr == '\\') ){
		rp++;
		rpchr = *rp;
	}
	tp = rp;
	for (;;) {
		if ( rpchr == '\0' ) break;
		if ( (rpchr == '\\') || (rpchr == '/') ){
			if ( *(tp + 1) != '\0' ) rp = tp;
#ifndef UNICODE
		}else{
			if ( Iskanji(rpchr) ) tp++;
#endif
		}
		tp++;
		rpchr = *tp;
	}
	return rp;
}

/* 指定幅を越える文字列を算出する ---------------------------------------------
	sisize : 表示対象の文字列長
	size  : 表示幅
*/
int LimStrcpy(PPC_APPINFO *cinfo, TCHAR *buf, LPCTSTR *drawptr, const TCHAR *nameptr, int name_len, int maxwidth)
{
	TCHAR *diend;
	const TCHAR *sp;

	if ( maxwidth >= name_len ){ // 表示する文字列が表示幅より少ないとき -------
		*drawptr = nameptr;
		return name_len;
	}
								// はみ出すとき -------------------------------
							// 最後のパス区切りを検索し、そこを先頭にする
	sp = nameptr;
	if ( cinfo->e.Dtype.mode != VFSDT_DLIST ){
		int slen;
		const TCHAR *spd;

		spd = FindDispLastEntry(sp);
		slen = name_len - (spd - nameptr);
		if ( slen > 0 ){
			if ( maxwidth >= slen ){
				*drawptr = spd;
				return slen;
			}
			sp = spd;
			name_len = slen;
		}
	}

	if ( IsTrue(UsePFont) && (EllipsisType == 0) ){ // プロポーショナルフォントはDrawTextまかせ
		*drawptr = sp;
		return name_len;
	}
	*drawptr = buf;
	diend = buf + maxwidth - 1;
	if ( EllipsisType != 0 ){ // 前部・両端を省略
		if ( maxwidth >= 4 ){
			*buf++ = '.';
			*buf++ = '.';
			name_len += 2;
			diend -= 2;
		}
		name_len -= maxwidth;
		if ( EllipsisType == 2 ){ // 両端のときは前部の省略量を半分に
			name_len /= 2;
		}

		while ( name_len > 0 ){
		#ifdef UNICODE
			WCHAR c;
			c = *sp++;
			if ( CCharWide(c) > 1 ){
				sp++;
				name_len--;
			}
		#else
			if ( IskanjiA(*sp++) ){
				sp++;
				name_len--;
			}
		#endif
			name_len--;
		}
		if ( EllipsisType == 1 ){
			tstrcpy(buf, sp);
			maxwidth += name_len; // name_len は 0 か -1
		}
	}

	if ( EllipsisType != 1 ){ // 両端・末尾を省略
		while ( buf < diend ){
		#ifdef UNICODE
			WCHAR c;
			c = *sp++;
			*buf++ = c;
			if ( CCharWide(c) > 1 ){
				if ( buf == diend ){
					*(buf - 1) = '.';
					break;
				}
				diend--;
				maxwidth--;
			}
		#else
			if ( IskanjiA(*buf++ = *sp++) ){
				if ( buf == diend ){
					*(buf - 1) = '.';
					break;
				}
				*buf++ = *sp++;
			}
		#endif
		}
		*buf = '.';
		*(buf + 1) = '\0';
	}
	return maxwidth;
}

#define SUBCOLORATTRIBUTES (FILE_ATTRIBUTE_REPARSE_POINT |  FILE_ATTRIBUTE_VIRTUAL | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_INTEGRITY_STREAM | FILE_ATTRIBUTE_NO_SCRUB_DATA)
COLORREF USEFASTCALL GetTextSubColor(DWORD attr)
{
	int index;

	if ( attr & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_VIRTUAL | FILE_ATTRIBUTE_OFFLINE) ){
		index = (attr & FILE_ATTRIBUTE_REPARSE_POINT) ? ECT_REPARSE : ECT_VIRTUAL;
	} else if ( attr & (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_INTEGRITY_STREAM | FILE_ATTRIBUTE_NO_SCRUB_DATA) ){
		index = (attr & FILE_ATTRIBUTE_ENCRYPTED) ? ECT_ENCRYPT : ECT_SPECIAL;
	} else{ // FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE
		index = ECT_COMPRES;
	}
	return C_entry[index];
}

void USEFASTCALL SetTextOtherColor(DISPSTRUCT *disp)
{
	COLORREF tmpc;

	if ( XC_fexc > 1 ){ // サイズ欄と同じ色
		DWORD atr;

		tmpc = C_AUTO;
		atr = disp->cell->f.dwFileAttributes;
		if ( atr & SUBCOLORATTRIBUTES ){
			tmpc = GetTextSubColor(disp->cell->f.dwFileAttributes);
		}
	} else{ // 拡張子と同じ色
		tmpc = disp->cell->extC;
	}
	if ( tmpc != C_AUTO ){
		if ( disp->cinfo->X_inag & INAG_GRAY ){
			tmpc = GetGrayColorF(tmpc);
		}
		disp->DSetCellTextColor(CTC_DXP disp->hDC, tmpc);
	}
}

// ラベル, 未算出ディレクトリ, リパースポイントの時にラベル表示
BOOL WriteAtrLabel(DISPSTRUCT *disp, int len)
{
	DWORD atr;
	int spacelen;
	int labellen;
	const TCHAR *labelptr;

	atr = disp->cell->f.dwFileAttributes;
	if ( (disp->cell->attr & ECA_DIRC) ||
		!(atr & (FILE_ATTRIBUTE_LABEL | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ){
		return FALSE;
	}
	if ( atr & FILE_ATTRIBUTE_DIRECTORY ){
		labelptr = DirString;
		labellen = DirStringLength;
	} else if ( atr & FILE_ATTRIBUTE_REPARSE_POINT ){
		if ( disp->cell->f.nFileSizeLow >= 0x100 ) return FALSE; // ●仮判定。ReparseTag 記載のファイルサイズでなさそうなら、ファイルサイズとして表示
		// REPARSE+SPARSE 属性の時は、参照される前はリンクのみで、ファイルを読み込むとハードリンクになるっぽい？
		labelptr = ReparseString;
		labellen = TSIZEOFSTR(ReparseString);
	} else{ // FILE_ATTRIBUTE_LABEL
		labelptr = VolString;
		labellen = TSIZEOFSTR(VolString);
	};
	spacelen = len - labellen;
	if ( spacelen > 0 ){ // 空白有り
		disp->lbox.right = disp->lbox.left + spacelen * disp->fontX;
		if ( disp->hfbr != NULL ) DxFillBack(disp->DxDraw, disp->hDC, &disp->lbox, disp->hfbr);
		DxMoveToEx(disp->DxDraw, disp->hDC, disp->lbox.right, disp->lbox.top);
		spacelen = 0;
	}
	if ( atr & SUBCOLORATTRIBUTES ){
		disp->DSetCellTextColor(CTC_DXP disp->hDC, GetTextSubColor(atr));
		DxTextOutRel(disp->DxDraw, disp->hDC, labelptr, labellen + spacelen);
		disp->DSetCellTextColor(CTC_DXP disp->hDC, disp->textc);
	} else{
		DxTextOutRel(disp->DxDraw, disp->hDC, labelptr, labellen + spacelen);
	}
	return TRUE;
}

#ifndef USEDIRECTX
// 右詰表示
void DrawTextRight(DISPSTRUCT *disp, const TCHAR *str, int len)
{
	SIZE fsize;
	RECT box;
	int slen = len;

	while ( *str == ' ' ){
		str++;
		slen--;
	}
	GetTextExtentPoint32(disp->hDC, str, slen, &fsize);
	box.left = disp->Xd;
	box.right = box.left + len * disp->fontX - fsize.cx;
	if ( box.left < box.right ){
		box.top = disp->lbox.top;
		box.bottom = box.top + fsize.cy;
		if ( disp->hfbr != NULL ){
			DxFillBack(disp->DxDraw, disp->hDC, &box, disp->hfbr);
		}
		DxMoveToEx(disp->DxDraw, disp->hDC, box.right, box.top);
	}
	DxTextOutRel(disp->DxDraw, disp->hDC, str, slen);
}
#endif

void PaintFileSize(DISPSTRUCT *disp, int wid, int flags)
{
	TCHAR buf[64];

	disp->lbox.left = disp->Xd;
	if ( WriteAtrLabel(disp, wid) == FALSE ){
		DWORD atr;

		FormatNumber(buf, flags, wid, disp->cell->f.nFileSizeLow, disp->cell->f.nFileSizeHigh);
		if ( disp->cell->attr & ECA_DIRG ){
			COLORREF oldbk, oldfg;
			int oldbmod;

			oldbmod = SetBkMode(disp->hDC, OPAQUE);
			oldfg = DxSetTextColor(disp->DxDraw, disp->hDC, disp->textc);
			oldbk = DxSetBkColor(disp->DxDraw, disp->hDC, C_eInfo[ECS_GRAY]);
			DrawNumber(disp, buf, wid);
			SetBkMode(disp->hDC, oldbmod);
			DxSetBkColor(disp->DxDraw, disp->hDC, oldbk);
			DxSetTextColor(disp->DxDraw, disp->hDC, oldfg);
		} else if ( (atr = disp->cell->f.dwFileAttributes) & SUBCOLORATTRIBUTES ){
			disp->DSetCellTextColor(CTC_DXP disp->hDC, GetTextSubColor(atr));
			DrawNumber(disp, buf, wid);
			disp->DSetCellTextColor(CTC_DXP disp->hDC, disp->textc);
		} else{
			DrawNumber(disp, buf, wid);
		}
	}
	disp->Xd += wid * disp->fontX;
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
}

void PaintDefaultTimeStamp(DISPSTRUCT *disp, int displen)
{
	FILETIME lTime;
	SYSTEMTIME sTime;

	if ( disp->cell->f.ftLastWriteTime.dwLowDateTime |
		disp->cell->f.ftLastWriteTime.dwHighDateTime ){

		FileTimeToLocalFileTime(&disp->cell->f.ftLastWriteTime, &lTime);
		FileTimeToSystemTime(&lTime, &sTime);
		if ( displen <= 14 ){
			wsprintf(disp->buf, T("%02d-%02d-%02d %02d:%02d"),
				sTime.wYear % 100, sTime.wMonth, sTime.wDay,
				sTime.wHour, sTime.wMinute);
		} else{
			wsprintf(disp->buf, T("%02d-%02d-%02d %02d:%02d:%02d.%03d"),
				sTime.wYear % 100, sTime.wMonth, sTime.wDay,
				sTime.wHour, sTime.wMinute, sTime.wSecond,
				sTime.wMilliseconds);
		}
	} else{
		tstrcpy(disp->buf, T("**-**-** **:**:**"));
	}
	DxTextOutRel(disp->DxDraw, disp->hDC, disp->buf, displen);
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	if ( UsePFont ){
		disp->Xd = disp->LP.x;
	} else{
		disp->Xd += displen * disp->fontX;
	}
}

void PaintInfoNumber(DISPSTRUCT *disp, DWORD num)
{
	int len;

	len = wsprintf(disp->buf, T("%3u"), num);
	DrawNumber(disp, disp->buf, len);
	disp->Xd += len * disp->fontX;
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	if ( UsePFont ) disp->Xd = disp->LP.x;
}

void PaintInfoSize(DISPSTRUCT *disp, DWORD low, DWORD high, DWORD flags, int len)
{
	if ( high != MAX32 ){
		FormatNumber(disp->buf, flags, len, low, high);
	} else{
		int i;
		TCHAR *ptr = disp->buf;

		for ( i = len; i; i-- ) *ptr++ = '-';
		*ptr = '\0';
	}
	DxTextOutRel(disp->DxDraw, disp->hDC, disp->buf, len);
	disp->Xd += len * disp->fontX;
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	if ( UsePFont ) disp->Xd = disp->LP.x;
}


void PaintFileAttributes(DISPSTRUCT *disp, int displen)
{
#ifndef WINEGCC
	MakeAttributesString(disp->buf, disp->cell);
#else
	if ( disp->cinfo->e.Dtype.mode != VFSDT_SLASH ){
		MakeAttributesString(disp->buf, disp->cell);
	} else{
		int stateID;

		stateID = disp->cell->state & ECS_STATEMASK;
		if ( disp->cell->attr & ECA_GRAY ) stateID = ECS_GRAY;
		MakeStatString(disp->buf, disp->cell->f.dwReserved0, stateID, displen);
	}
#endif
	DxTextOutRel(disp->DxDraw, disp->hDC, disp->buf, displen);
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	disp->Xd += displen * disp->fontX;
}

void PaintMultiLineFilename(DISPSTRUCT *disp, const BYTE *fmt)
{
	const TCHAR *filep;		// 表示開始位置
	int nwid, ewid, fullwid;	// ファイル名/拡張子の表示幅
	int widthpixel;
	int length, result;
	SIZE textsize;

	if ( disp->cfileptr != NULL ){
		filep = disp->cfileptr;
	} else{
		filep = disp->cell->f.cFileName;
	}
	length = tstrlen32(filep);

	// ●1.2x fmt を変化させないようにすると、描画がおかしくなる…全体検証の必要有り
	nwid = *fmt++;
	ewid = *fmt++;
	fullwid = (ewid == DE_FN_WITH_EXT) ? nwid : nwid + ewid;

	if ( length ){
		if ( XC_fexc ) SetTextOtherColor(disp); // 拡張子別色を使用する

		// nwid に収まる文字数と、全体のピクセル幅を算出
		widthpixel = fullwid * disp->fontX;
		DxGetTextExtentExPoint(disp->DxDraw, disp->hDC, filep, length,
			widthpixel, &result, NULL, &textsize);
		if ( (textsize.cx > widthpixel) &&
			(*(fmt - 3) == DE_LFN_LMUL) ){
			disp->backbox.left = disp->Xd;
			disp->backbox.right = disp->Xd + widthpixel;
			DxDrawText(disp->DxDraw, disp->hDC, filep, length, &disp->backbox,
					NAME_DRAW_FLAGS);

			SetDelayEntryTip(disp, disp->cell->f.cFileName, nwid, disp->cell->ext);

		} else{
			DxTextOutRel(disp->DxDraw, disp->hDC, filep, result);
			disp->cfileptr = filep + result;
		}
		disp->Xd += widthpixel;
	}
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	if ( XC_fexc ) disp->DSetCellTextColor(CTC_DXP disp->hDC, disp->textc);
}

void PaintFilename(DISPSTRUCT *disp, const BYTE *fmt, int Xe)
{
	PPC_APPINFO *cinfo = disp->cinfo;
	const TCHAR *fileptr, *extptr;	// ファイル名、拡張子の先頭
	const TCHAR *dispf;		// 表示開始位置
	int extoffset;			// 表示拡張子
	int nwid, ewid;	// ファイル名/拡張子の表示幅
	int drawlen;

	extoffset = disp->cell->ext;
	fileptr = disp->cell->f.cFileName;
	if ( *fileptr == '>' ){ // MAX_PATH 越えファイル名処理
		const TCHAR *longname = (const TCHAR *)EntryExtData_GetDATAptr(cinfo, DFC_LONGNAME, disp->cell);
		if ( longname != NULL ){
			fileptr = longname;
			extoffset = tstrlen32(fileptr);
			if ( !(disp->cell->attr & (ECA_PARENT | ECA_THIS)) ){
				TCHAR *extp;

				extp = tstrrchr(VFSFindLastEntry(fileptr + 1), '.');
				if ( extp != NULL ) extoffset = extp - fileptr;
			}
		}
	}
	if ( cinfo->UseArcPathMask &&	// 書庫内のパス省略処理
		!(disp->cell->attr & (ECA_PARENT | ECA_THIS)) ){
		SIZE32_T len;

		len = tstrlen32(cinfo->ArcPathMask);
		if ( *(fileptr + len) == '\\' ) len++;
		fileptr += len;
		if ( (extoffset -= len) < 0 ) extoffset = tstrlen32(fileptr);
	} else if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) && (cinfo->e.Dtype.BasePath[0] != '\0') ){ // listfileの共通パスの省略処理
		int len = tstrlen32(cinfo->RealPath);

		if ( (memcmp(fileptr, cinfo->RealPath, TSTROFF(len)) == 0) &&
			(*(fileptr + len) == '\\') ){
			fileptr += len + 1;
			extoffset -= len + 1;
		}
	}

	nwid = *fmt;
	ewid = *(fmt + 1);
	if ( *(fmt - 1) != DE_LFN ){ // DE_SFN / DE_LFN_EXT / DE_SFN_EXT ==========
		for (;;) {
			// DE_LFN_EXT / DE_SFN_EXT 拡張子が枠より長い場合はファイル名を短く
			if ( (*(fmt - 1) != DE_SFN) &&
				 !(disp->cell->attr & (ECA_PARENT | ECA_THIS | ECA_DIR)) ){
				const TCHAR *extp;
				int extlen;

				if ( *fileptr != '\0' ){
					extp = fileptr + extoffset;
					if ( *extp == '.' ){
						extlen = tstrlen(extp);

						if ( nwid == DE_FN_ALL_WIDTH ){
							int xwid = disp->Xright - disp->Xd;
							nwid = xwid / disp->fontX;
						}

						if ( ewid == DE_FN_WITH_EXT ){
							extoffset = extp - fileptr;

							if ( (extoffset + extlen) > nwid ){ // ファイル名+拡張子全体表示できない
								if ( extlen <= (nwid / 2) ){ // 拡張子が幅内
									nwid -= extlen;
									ewid = extlen;
								}else{ // 拡張子長すぎ
									ewid = extoffset < (nwid / 2) ? (nwid - extoffset) : (nwid / 2);
									nwid -= ewid;
								}
							} // else 全体表示できるならそのままで OK
						}else if ( extlen > ewid ){ // 拡張子欄に入らない
							extoffset = extp - fileptr;

							if ( (extlen <= ((nwid + ewid) / 2)) ||
								 ((extoffset + extlen) <= (nwid + ewid)) ){ // ファイル名+拡張子全体表示可能
								nwid -= extlen - ewid;
								ewid = extlen;
							}else{
								int fullwid = nwid + ewid;

								ewid = extoffset < (fullwid / 2) ? (fullwid - extoffset) : (fullwid / 2);
								nwid = fullwid - ewid;
							}
							break;
						} // else 欄内ならそのままで OK
					}
				}
			}
			// DE_SFN / DE_SFN_EXT SFNがあればSFNを表示
			if ( *(fmt - 1) != DE_LFN_EXT ){
				const TCHAR *extp;

				if ( (disp->cell->f.cAlternateFileName[0] == '\0') || (disp->cell->attr & (ECA_PARENT | ECA_THIS)) ){
					break;
				}
				fileptr = disp->cell->f.cAlternateFileName;
				// 拡張子位置を決定
				extp = tstrrchr(fileptr, '.');

				if ( extp != NULL ){
					extoffset = extp - fileptr;
				} else{
					extoffset = tstrlen32(fileptr);
				}
			}
			break;
		}
	}
									// 表示幅を取得 ===============
	extptr = fileptr + extoffset;
					// 拡張子なし or 分離しない設定のdirなら連結
	if ( *extptr == '\0' ){
		if ( nwid ){
			extoffset += tstrlen32(extptr);
			if ( ewid != DE_FN_WITH_EXT ) nwid += ewid;
			ewid = 0;
		#if 0 // サイズと連結するときに利用する
			if ( (extoffset > nwid) &&
				(disp->cell->f.dwFileAttributes &
				(FILE_ATTRIBUTE_LABEL | FILE_ATTRIBUTE_DIRECTORY)) &&
				!(disp->cell->attr & ECA_DIRC) &&
				((*(fmt + 2) == DE_SIZE1) || (*(fmt + 2) == DE_SIZE2) ||
				(*(fmt + 2) == DE_SIZE3)) ){
				nwid += *(fmt + 3);
			}
		#endif
		}
	}
	if ( XC_fexc ) SetTextOtherColor(disp); // 拡張子別色を使用する

	// Tip 表示
	SetDelayEntryTip(disp, fileptr, nwid, extoffset);
									// ファイル名 表示 ============
	if ( cinfo->UseSplitPathName ){
		TCHAR *sp = VFSFindLastEntry(fileptr);
		if ( sp != fileptr ){
			extoffset -= sp - fileptr;
			fileptr = sp;
		}
	}
	drawlen = LimStrcpy(cinfo, disp->buf, &dispf, fileptr, extoffset, nwid);
#ifndef UNICODE
	if ( IsTrue(UseDrawText) ){
#endif
		disp->backbox.left = disp->Xd;
		disp->backbox.right = disp->Xd + nwid * disp->fontX;

		DxDrawText(disp->DxDraw, disp->hDC, dispf, drawlen, &disp->backbox, NAME_DRAW_FLAGS);
#ifndef UNICODE
	} else{
		DxTextOutRel(disp->DxDraw, disp->hDC, dispf, drawlen);
	}
#endif
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	disp->Xd += nwid * disp->fontX;
	if ( ewid == 0 ) goto fix;
									// 拡張子表示 =================
	if ( ewid == DE_FN_WITH_EXT ){ // 拡張子はファイル名と一体にする
		if ( disp->LP.x >= Xe ) goto fix;
		ewid = nwid - drawlen;
		if ( ewid <= 0 ) goto fix; // プロポーショナルのとき、nwid < drawlen の場合あり
	} else{
		if ( disp->Xd >= Xe ) goto fix;
									// 余白調整
		if ( (disp->LP.x < disp->Xd) && (disp->hfbr != NULL) ){
			COLORREF tmpc;

			disp->lbox.left = disp->LP.x;
			disp->lbox.right = disp->Xd;
			if ( (XC_fexc == 1) && (disp->textc != disp->fc) && ((tmpc = disp->cell->extC) != C_AUTO) ){
				HBRUSH hB;

				hB = CreateSolidBrush(tmpc);
				DxFillBack(disp->DxDraw, disp->hDC, &disp->lbox, hB);
				DeleteObject(hB);
			} else{
				DxFillBack(disp->DxDraw, disp->hDC, &disp->lbox, disp->hbbr);
			}
		}
		DxMoveToEx(disp->DxDraw, disp->hDC, disp->Xd, disp->lbox.top);
		disp->LP.x = disp->Xd;
		disp->Xd += ewid * disp->fontX;
	}
	// disp->LP.x ～ disp->Xd 間に表示
	{
		COLORREF tmpc;

		tmpc = disp->cell->extC;
		if ( tmpc != C_AUTO ){
		#ifdef USEDIRECTX	// DirectX 時は、拡張子もグレー化対応させる。
							// 通常時はパフォーマンス改善のため、変更しない
			if ( cinfo->X_inag & INAG_GRAY ) tmpc = GetGrayColorF(tmpc);
		#endif
			disp->DSetCellTextColor(CTC_DXP disp->hDC, tmpc);
		}
	}

	// ※ DrawText で拡張子を表示すると、文字列が短すぎるため「ext...」が「ext」になることがある。このため、予め文字長制限が必要
	drawlen = tstrlen32(extptr);
	dispf = extptr;
	IfGDImode(disp->hDC) if ( ewid < drawlen ){
		dispf = disp->buf;
		drawlen = ewid;
	#ifdef UNICODE
		memcpy(disp->buf, extptr, ewid * sizeof(TCHAR));
		disp->buf[ewid - 1] = '.';
		disp->buf[ewid] = '\0';
	#else
		{
			const char *extsrc = extptr;
			char *extdest = disp->buf, *destend = disp->buf + ewid - 1;

			while ( extdest < destend ){
				if ( IskanjiA(*extdest++ = *extsrc++) ){
					if ( extdest == destend ){
						*(extdest - 1) = '.';
						break;
					}
					*extdest++ = *extsrc++;
				}
			}
			*extdest = '.';
			*(extdest + 1) = '\0';
		}
	#endif
	}
#ifndef UNICODE
	#if DRAWMODE == DRAWMODE_DW
	if ( (disp->hDC == DXMODEVALUE_DX) || IsTrue(UsePFont) )
	#else
	if ( IsTrue(UsePFont) )
	#endif // DRAWMODE
	{
#endif // UNICODE
		disp->backbox.left = disp->LP.x;
		disp->backbox.right = disp->Xd;

		DxDrawText(disp->DxDraw, disp->hDC, dispf, drawlen, &disp->backbox, NAME_DRAW_FLAGS);
#ifndef UNICODE
	} else{
		DxTextOutBack(disp->DxDraw, disp->hDC, dispf, drawlen);
	}
#endif
	DxGetCurrentPositionEx(disp->DxDraw, disp->hDC, &disp->LP);
	disp->DSetCellTextColor(CTC_DXP disp->hDC, disp->textc);
	return;
fix:
	if ( XC_fexc ) disp->DSetCellTextColor(CTC_DXP disp->hDC, disp->textc);
	return;
}

#if USEGRADIENTFILL
BOOL DrawGradientBox(HDC hDC, int left, int top, int right, int bottom, COLORREF c1, COLORREF c2)
{
	TRIVERTEX vt[2];
	GRADIENT_RECT gr;

	if ( DGradientFill == NULL ){			// 初めての呼び出しなら関数準備
		if ( NULL == LoadWinAPI("msimg32.dll", NULL, GradientFillAPI, LOADWINAPI_LOAD) ){
			return FALSE;
		}
	}
	vt[0].x = left;
	vt[0].y = top;
	vt[0].Red = (COLOR16)(GetRValue(c1) * 0x100);
	vt[0].Green = (COLOR16)(GetGValue(c1) * 0x100);
	vt[0].Blue = (COLOR16)(GetBValue(c1) * 0x100);
	vt[0].Alpha = 0xffff;
	vt[1].x = right;
	vt[1].y = bottom;
	vt[1].Red = (COLOR16)(GetRValue(c2) * 0x100);
	vt[1].Green = (COLOR16)(GetGValue(c2) * 0x100);
	vt[1].Blue = (COLOR16)(GetBValue(c2) * 0x100);
	vt[1].Alpha = 0xffff;
	gr.UpperLeft = 1;
	gr.LowerRight = 0;
	DGradientFill(hDC, vt, 2, &gr, 1, GRADIENT_FILL_RECT_V);
	return TRUE;
}
#endif

BYTE GetPointColor(int bc, int fc)
{
	int c = (bc + (fc - bc) / 4);
	if ( c > 255 ) c = 255;
	if ( c < 0 ) c = 0;
	return (BYTE)c;
}

// Cell 表示を行う ------------------------------------------------------------
int DispEntry(PPC_APPINFO *cinfo, HDC hDC, const XC_CFMT *cfmt, ENTRYINDEX EI_No, int maxX, const RECT *BaseBox)
{
	DISPSTRUCT disp;
	const BYTE *fmt, *nextfmt;
	int Xe, Xleft, Ytop, Ybottom;	// 表示位置の基準/先頭/末端
	RECT tempbox;	// 一時的に使用する
	BOOL bkdraw = FALSE;	// 背景描画が必要なら TRUE
	int Check[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cellno, oldBkMode = 0;
	int line = 0; // 描画行(複数行時)

	disp.hDC = hDC;
	disp.cinfo = cinfo;
	disp.fontX = cinfo->fontX;
	disp.NoBack = cinfo->bg.X_WallpaperType;
	disp.cfileptr = NULL;
	disp.lspc = cinfo->X_lspc;

#if DRAWMODE == DRAWMODE_GDI
	disp.DSetCellTextColor = SetTextColor;
#else
	disp.DxDraw = cinfo->DxDraw;
	disp.DSetCellTextColor = DxSetTextColor;
	IfDXmode(hDC) disp.NoBack = 1;
#endif

#if USEDELAYCURSOR
	if ( IsTrue(cinfo->freeCell) ) disp.NoBack = 1;
#endif
							// 表示位置の決定 ---------------------------------
	disp.LineTopX = disp.LP.x = disp.Xd = BaseBox->left;
	Ytop = disp.backbox.top = disp.LP.y = BaseBox->top;
	disp.backbox.bottom = BaseBox->bottom;
	if ( EI_No >= 0 ){						// Entry
		if ( cinfo->EntriesOffset.x | cinfo->EntriesOffset.y ){
			disp.LineTopX = disp.LP.x = disp.Xd = BaseBox->left + cinfo->EntriesOffset.x;
			Ytop = disp.backbox.top = disp.LP.y = BaseBox->top + cinfo->EntriesOffset.y;;
		}

		Xe = BaseBox->right;
		if ( (Xe + disp.fontX) < maxX ) maxX = Xe;
		cellno = EI_No;
	#if USEDELAYCURSOR
		if ( cellno == cinfo->e.cellN ){ // 遅延移動の時、移動先を記憶
			cinfo->TargetNpos = disp.LP;
		}
	#endif
	} else{
		cellno = cinfo->e.cellN;
		Xe = maxX;

		if ( EI_No == DISPENTRY_NO_OUTPANE ){
			TEXTMETRIC tm;
			GetAndFixTextMetrics(hDC, &tm);
			disp.fontX = tm.tmAveCharWidth;
		}
	}
	disp.lbox.top = disp.backbox.top + (disp.lspc >> 1);
	disp.lbox.bottom = disp.lbox.top + cinfo->fontY - disp.lspc;
	Ybottom = Ytop + cfmt->height * cinfo->fontY;
	if ( Ybottom < disp.backbox.bottom ) Ybottom = disp.backbox.bottom;
	Xleft = disp.LineTopX;
	disp.Xright = Xe;
	if ( Xe > maxX ) Xe = maxX;
							// 空欄処理 ---------------------------------------
	if ( cellno >= cinfo->e.cellIMax ){
		disp.backbox.left = disp.Xd;
		disp.backbox.right = Xe;
		if ( !disp.NoBack ){
			DxFillBack(disp.DxDraw, hDC, &disp.backbox, cinfo->C_BackBrush);
		}
		return Xe;
	}
	disp.hback = cinfo->C_BackBrush;
							// 配色の決定 -------------------------------------
	disp.cell = &CEL(cellno);
										// 前景色 =============================
	disp.fc = cfmt->fc;
	if ( disp.fc == C_AUTO ) disp.fc = C_entry[disp.cell->type];
										// 背景色 =============================
	disp.bc = cfmt->bc;
	if ( disp.bc == C_AUTO ){
		DWORD colorID;

		colorID = disp.cell->state;
		if ( (disp.cell->attr & ECA_GRAY) && (C_eInfo[ECS_GRAY] != cinfo->BackColor) ){
			colorID = ECS_GRAY; // 不確定状態
		}
		if ( colorID == ECS_NORMAL ){
			if ( cinfo->BackColor != C_back ){
				disp.bc = cinfo->BackColor;
			} else if ( (C_eInfo[ECS_EVEN] != C_AUTO) && (EI_No >= 0) &&
				(((EI_No - cinfo->cellWMin) % cinfo->cel.Area.cy) & 1) )
//↓スクロールでストライプ毎動かすとき
//			if ( (C_eInfo[ECS_EVEN] != C_AUTO) && (No >= 0) &&
//				 ((((No - cinfo->cellWMin) % cinfo->cel.Area.cy) ^
//				 (XC_page ? (cinfo->cellWMin & 1) : 0)) & 1) )
			{
				disp.bc = C_eInfo[ECS_EVEN];
			} else{
				disp.bc = C_eInfo[ECS_NORMAL];
			}
		} else{
			if ( colorID >= ECS_HLBIT ){ // ハイライト有り
				colorID = (colorID / ECS_HLBIT) + ECS_HILIGHT - 1;
			}
			if ( (C_eInfo[colorID] != cinfo->BackColor) ) bkdraw = TRUE;
			disp.bc = C_eInfo[colorID];
		}
	}

	if ( IsCellPtrMarked(disp.cell) ){	// マーク位置 =================
		Check[cfmt->mark] = cfmt->mark + CSR_MARK_OFFSET;
	}
										// カーソル位置 =======================
	if ( (cellno == cinfo->e.cellN) && cfmt->csr ){
	#if USEDELAYCURSOR && defined(USEDIRECTX)
		int cursorcolor;
	#endif
		disp.IsCursor = TRUE;

		if ( ((cinfo->XC_tree.mode == PPCTREE_OFF) && !cinfo->combo) ?
				(GetForegroundWindow() != cinfo->info.hWnd) :
//				!GetFocus() :	←これだとIME等にスレッドが行くとGrayになる
				(GetFocus() != cinfo->info.hWnd) ){ // 非アクティブ時
		#if USEDELAYCURSOR
			Check_Line = ECS_NOFOCUS;
		#if DRAWMODE != DRAWMODE_GDI
			cursorcolor = ECS_NOFOCUS;
		#endif
		#endif

		#if DRAWMODE != DRAWMODE_D3D // GDI/DirectX
			if ( cfmt->csr >= 3 ){
				Check[cfmt->csr] = ECS_NOFOCUS;
			} else{
				disp.bc = C_eInfo[ECS_NOFOCUS];
				bkdraw = TRUE;
			}
		#else // DirectX
			IfGDImode(hDC)
			{
				if ( cfmt->csr >= 3 ){
					Check[cfmt->csr] = ECS_NOFOCUS;
				} else{
					disp.bc = C_eInfo[ECS_NOFOCUS];
					bkdraw = TRUE;
				}
			}
		#endif
		} else{ // アクティブ時
		#if USEDELAYCURSOR
			Check_Line = ECS_UDCSR;
		#if DRAWMODE != DRAWMODE_GDI
			cursorcolor = ECS_SELECT;
		#endif
		#endif
		#if DRAWMODE != DRAWMODE_D3D
			Check[cfmt->csr] = cfmt->csr + CSR_MARK_OFFSET;
		#else
			IfGDImode(hDC) Check[cfmt->csr] = cfmt->csr + CSR_MARK_OFFSET;
		#endif
		}
	#if USEDELAYCURSOR
		DxDrawCursor(disp.DxDraw, hDC, BaseBox, C_eInfo[cursorcolor]);
	#endif
	} else{
		disp.IsCursor = FALSE;
	}

	if ( cinfo->X_inag & INAG_GRAY ){
		disp.fc = GetGrayColorF(disp.fc);
		disp.bc = GetGrayColorB(disp.bc);
	}

	if ( Check_BackColor ){				// 背景選択色 =======================
		bkdraw = TRUE;
		disp.bc = C_eInfo[
			(Check_BackColor == ECS_NOFOCUS) ? ECS_NOFOCUS : ECS_SELECT];
		if ( cinfo->CursorColor != C_AUTO ) disp.bc = cinfo->CursorColor;
	}
	disp.textc = disp.fc;
	if ( Check_Reverse ){				// 前背色反転 =========================
		bkdraw = TRUE;
		disp.fc = disp.bc;
		disp.bc = disp.textc;
	#if DRAWMODE == DRAWMODE_GDI
		disp.DSetCellTextColor = SetBkColor;
	#else
		disp.DSetCellTextColor = DxSetBkColor;
	#endif
	}
										// マウスホバ－ハイライト ============
	if ( (EI_No >= 0) && (cellno == cinfo->e.cellPoint) ){
		if ( (cinfo->e.cellPointType >= PPCR_CELLMARK) && (X_poshl > 0) ){
			COLORREF usecolor;

			bkdraw = TRUE;
			usecolor = (C_eInfo[ECS_HOVER] != C_AUTO) ? C_eInfo[ECS_HOVER] : disp.fc;
			disp.bc = (X_poshl > 1) ? usecolor :
				RGB(GetPointColor(GetRValue(disp.bc), GetRValue(usecolor)),
					GetPointColor(GetGValue(disp.bc), GetGValue(usecolor)),
					GetPointColor(GetBValue(disp.bc), GetBValue(usecolor)));
			if ( Check_Reverse ) disp.textc = disp.bc; // 前背色反転中
		}
	}
	if ( Check_Negative ){				// ネガポジ反転 =======================
		bkdraw = TRUE;
		disp.fc ^= 0xffffff;
		disp.bc ^= 0xffffff;
		disp.textc = disp.fc;
	}

	DxSetTextColor(disp.DxDraw, hDC, disp.fc);
	DxSetBkColor(disp.DxDraw, hDC, disp.bc);

	disp.hbbr = (disp.bc == cinfo->BackColor) ? cinfo->C_BackBrush : CreateSolidBrush(disp.bc);
//	if ( disp.hback == NULL ) disp.hback = disp.hbbr;	// スクロールストライプ用
	disp.hfbr = (bkdraw || !disp.NoBack) ? disp.hbbr : NULL;

#ifdef USEDIRECTX
	IfDXmode(hDC) if ( disp.bc != cinfo->BackColor ){ // (a)
		DxDrawBack(disp.DxDraw, hDC, BaseBox,
			disp.bc | ((!disp.IsCursor && cinfo->bg.X_WallpaperType) ? 0x30000000 : 0xff000000));
	}
#endif
	if ( Check_Line ){	// 下線 (b)
	#if USEGRADIENTFILL // グラデーション表示
		#if USEDELAYCURSOR
		cinfo->e.cellNbc = disp.bc;
		if ( IsTrue(cinfo->freeCell) || DrawGradientBox(hDC, disp.LineTopX, disp.backbox.top, Xe, disp.backbox.bottom, disp.bc, C_eInfo[ECS_UDCSR]) )
		#else
		if ( DrawGradientBox(hDC, disp.LineTopX, disp.backbox.top, Xe, disp.backbox.bottom, disp.bc, C_eInfo[ECS_UDCSR]) )
		#endif
		{
			disp.hfbr = NULL;
			bkdraw = FALSE;
			disp.NoBack = 1;
			oldBkMode = SetBkMode(hDC, TRANSPARENT);
		#if USEDELAYCURSOR
			if ( IsTrue(cinfo->freeCell) ) Check_Focusbox = 1;
		#endif
		}
	#else	// 通常表示
		HBRUSH hB;
		COLORREF color;

		disp.hfbr = NULL;
		oldBkMode = DxSetBkMode(disp.DxDraw, hDC, TRANSPARENT);
							// 背景塗りつぶし
		tempbox.left = disp.LineTopX;
		tempbox.right = Xe;
		tempbox.top = disp.backbox.top;
		tempbox.bottom = disp.lbox.top;
		if ( !disp.NoBack ) DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
		tempbox.top = disp.lbox.top;
		tempbox.bottom = disp.backbox.bottom - XC_ulh;
		if ( bkdraw || !disp.NoBack ){
			DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
		}
							// 下線描画
		tempbox.top = tempbox.bottom;
		tempbox.bottom = disp.backbox.bottom;
		color = (cinfo->CursorColor == C_AUTO) ? C_eInfo[Check_Line] : cinfo->CursorColor;
		hB = CreateSolidBrush(color);
		DxFillRectColor(disp.DxDraw, hDC, &tempbox, hB, color);
		DeleteObject(hB);
		disp.NoBack = 1;
	#endif
	} else{
		if ( disp.NoBack && bkdraw ){
			oldBkMode = DxSetBkMode(disp.DxDraw, hDC, OPAQUE);
		}
	#if USEGRADIENTFILL
		if ( !disp.NoBack && !bkdraw ){
			oldBkMode = DxSetBkMode(disp.DxDraw, hDC, OPAQUE);
		}
	#endif
	}

	fmt = cfmt->fmt;
	if ( cfmt->nextline ){
		nextfmt = fmt + cfmt->nextline;
		if ( disp.lspc && (!disp.NoBack || bkdraw) ){
			tempbox.left = disp.Xd;
			tempbox.right = maxX;
			tempbox.bottom = disp.lbox.top + cinfo->fontY;
			tempbox.top = tempbox.bottom - disp.lspc;
			DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
		}
	} else{
		nextfmt = NULL;
	}

	if ( disp.lspc && !disp.NoBack ){ // 行間の空白処理 ------
		disp.backbox.left = disp.Xd;
		disp.backbox.right = maxX;
		disp.backbox.bottom = disp.lbox.top;
		if ( disp.backbox.top < disp.backbox.bottom ){
			DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hbbr); // 上側
		}
		if ( disp.lspc > 0 ){
			disp.backbox.bottom = Ybottom;
			disp.backbox.top = disp.backbox.bottom - (disp.lspc - (disp.lspc >> 1));
			DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hbbr); // 下側
			disp.backbox.top = Ytop;
		}
	}
							// システムメッセージ -----------------------------
	if ( (disp.cell->type == ECT_SYSMSG) && !(cfmt->attr & (DE_ATTR_DIR | DE_ATTR_PATH)) ){
		const TCHAR *namep;

		DxMoveToEx(disp.DxDraw, hDC, disp.Xd, disp.lbox.top);
		DxTextOutRel(disp.DxDraw, hDC, SpaceString, 1);
		namep = disp.cell->f.cFileName;
		DxTextOutRel(disp.DxDraw, hDC, namep, tstrlen32(namep));
		DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
		if ( (!disp.NoBack || bkdraw) && (disp.lbox.bottom < disp.backbox.bottom) ){
			disp.backbox.top = disp.lbox.bottom;
			disp.backbox.left = disp.Xd;
			disp.backbox.right = disp.LP.x;
			DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
			disp.backbox.top = Ytop;
		}
		fmt = endofformat;
		// 枠表示のための表示位置調整
		disp.LineTopX += disp.fontX / 2;
		disp.Xright -= disp.fontX / 2;
	} else {
	while ( *fmt ){
		DxMoveToEx(disp.DxDraw, hDC, disp.Xd, disp.lbox.top); // ●1.61ここに必要？→個別に移す必要がある

		switch ( *fmt++ ){
			case DE_NEWLINE: // 改行
				fmt += DE_NEWLINE_SIZE - 1;
				disp.backbox.right = disp.Xright - disp.fontX * 1;
				if ( !disp.NoBack || (disp.hfbr != NULL) ){
					if ( disp.LP.x < disp.backbox.right ){
						disp.backbox.left = disp.LP.x;
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hfbr);

						disp.LP.x = disp.backbox.right;
						disp.Xd = Xe;
					}
					if ( disp.LP.x < disp.Xd ){
						disp.backbox.left = disp.LP.x;
						disp.backbox.right = disp.Xd;
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
						disp.LP.x = disp.Xd;
					}
				}
				disp.Xd = Xe;
				break;

			case DE_SPC: {	// 空白(行内用) ----------------------------------
				BYTE len = *fmt++;

				if ( len != 0 ){
					disp.Xd += disp.fontX * len;
				} else{
					disp.Xd = Xe;
				}
				break;
			}

			case DE_BLANK:{	// 空白(行端用)  ---------------------------------
				BYTE len, nextdata;

				len = *fmt++;
				nextdata = *fmt;
				if ( (nextdata == 0) || (nextdata == DE_NEWLINE) ){
					disp.backbox.right = disp.Xright - disp.fontX * len;
					if ( nextdata == 0 ) disp.Xright = disp.backbox.right;
					if ( disp.LP.x <= disp.backbox.right ){
						if ( (disp.LP.x < disp.backbox.right) && !disp.NoBack ){
							disp.backbox.left = disp.LP.x;
							DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hfbr);
						}
						disp.LP.x = disp.backbox.right;
						disp.Xd = Xe;
					}
				} else if ( len ){
					disp.Xd += disp.fontX * len;
				} else{
					disp.Xd = Xe;
				}

				if ( disp.LP.x < disp.Xd ){
					if ( !disp.NoBack ){
						disp.backbox.left = disp.LP.x;
						disp.backbox.right = disp.Xd;
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
					}
					disp.LP.x = disp.Xd;
				}
				break;
			}
			case DE_IMAGE:	// 画像 ---------------------------------------
				PaintImageIcon(&disp, fmt, Check_Mark, disp.backbox.top, disp.backbox.bottom, cfmt);
				// if ( disp.IsCursor ) Check_Box = ECS_BOX; // 枠強制表示
				fmt += DE_IMAGE_SIZE - 1;
				break;

			case DE_ICON:	// アイコン ---------------------------------------
										// イメージリスト未作成なら作成指示
				if ( cinfo->EntryIcons.hImage == NULL ){
					InitCellIcon(cinfo, fmt - 1);
				}
				PaintIcon(&disp,
						disp.fontX * 2 - ICONBLANK,
						disp.lspc ? cinfo->fontY : cinfo->fontY - ICONBLANK,
						Check_Mark, Xe, cfmt->height);
				break;

			case DE_ICON2: {// アイコン ---------------------------------------
				int imgsize;
										// イメージリスト未作成なら作成指示
				if ( cinfo->EntryIcons.hImage == NULL ){
					InitCellIcon(cinfo, fmt - 1);
				}
				imgsize = *fmt++;
				if ( cinfo->X_textmag != 100 ){
					imgsize = (imgsize * cinfo->X_textmag) / 100;
				}
				if ( cinfo->FontDPI != DEFAULT_WIN_DPI ){
					imgsize = (imgsize * cinfo->FontDPI) / DEFAULT_WIN_DPI;
				}

				PaintIcon(&disp, imgsize, imgsize, Check_Mark, Xe, cfmt->height);
				// 複数行時の行間を改めて消去
				if ( (nextfmt != NULL) && (line == 0) ){
					if ( disp.lspc && (!disp.NoBack || bkdraw) ){
						tempbox.left = disp.Xd;
						tempbox.right = maxX;
						tempbox.bottom = disp.lbox.top + cinfo->fontY;
						tempbox.top = tempbox.bottom - disp.lspc;
						DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
					}
				}
				break;
			}

			case DE_MARK:	// マーク -----------------------------------------
				if ( Check_Mark ){
					if ( disp.lspc && !disp.NoBack ){
						disp.backbox.left = disp.Xd;
						disp.backbox.right = disp.Xd + disp.fontX;
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
					}
					DxSetTextColor(disp.DxDraw, hDC, C_eInfo[Check_Mark]);
					DxSetBkColor(disp.DxDraw, hDC, cinfo->BackColor);
					DxTextOutRel(disp.DxDraw, hDC, MarkString, 1);
					DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
					DxSetTextColor(disp.DxDraw, hDC, disp.fc);
					DxSetBkColor(disp.DxDraw, hDC, disp.bc);
				}
				disp.Xd += disp.fontX;
				disp.LineTopX += disp.fontX; // ●1.61枠のためにずらす／字下げを止めたい
				if ( disp.LP.x < disp.Xd ){
					disp.backbox.left = disp.LP.x;
					disp.backbox.right = disp.Xd;
					if ( !disp.NoBack ){
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
					}
					disp.LP.x = disp.Xd;
				}
				break;

			case DE_CHECKBOX:	// チェックボックス ---------------------------
			case DE_CHECK:	// チェック
				if ( (disp.cell->attr & (ECA_PARENT | ECA_THIS | ECA_ETC)) ){
					disp.backbox.left = disp.LP.x;
					disp.LP.x = disp.Xd = disp.backbox.right =
						disp.backbox.left + disp.fontX * 2;
					if ( !disp.NoBack ){
						DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
					}
					break;
				}

				if ( disp.lspc && !disp.NoBack ){
					disp.backbox.left = disp.Xd;
					disp.backbox.right = disp.Xd + disp.fontX * 2;
					DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
				}

				DxSetTextColor(disp.DxDraw, hDC, C_eInfo[ECS_MARK]);
				DxSetBkColor(disp.DxDraw, hDC, cinfo->BackColor);
			#ifdef UNICODE
				DxTextOutRel(disp.DxDraw, hDC, CheckBoxCodeString[(Check_Mark ? 1 : 0) + (*(fmt - 1) - DE_CHECK) * 2], 1);
			#else
				TextOutW(hDC, 0, 0, CheckBoxCodeString[(Check_Mark ? 1 : 0) + (*(fmt - 1) - DE_CHECK) * 2], 1);
			#endif
				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				DxSetTextColor(disp.DxDraw, hDC, disp.fc);
				DxSetBkColor(disp.DxDraw, hDC, disp.bc);
				disp.Xd += 2 * disp.fontX;
				break;

			case DE_LFN:	// ファイル名 -------------------------------------
			case DE_SFN:	// SFNファイル名
			case DE_LFN_EXT:	// ファイル名(拡張子を優先表示)
			case DE_SFN_EXT:	// SFNファイル名(拡張子を優先表示)
				PaintFilename(&disp, fmt, Xe);
				fmt += DE_LFN_SIZE - 1;
				break;

			case DE_LFN_MUL: // エントリ名複数行 ------------------------------
			case DE_LFN_LMUL: // エントリ名複数行(最終行)
				PaintMultiLineFilename(&disp, fmt);
				fmt += DE_LFN_MUL_SIZE - 1;
				break;

			case DE_SIZE1:	// ファイルサイズ ---------------------------------
				PaintFileSize(&disp, 7, CXFN_FILE_NOSEP);
				break;

			case DE_SIZE2:	// ファイルサイズ(桁区切あり) ---------------------
				PaintFileSize(&disp, *fmt++, CXFN_FILE);
				break;

			case DE_SIZE3:	// ファイルサイズ(桁区切なし) ---------------------
				PaintFileSize(&disp, *fmt++, CXFN_FILE_NOSEP);
				break;

			case DE_SIZE4:	// ファイルサイズ(桁有り、k表示) ------------------
				PaintFileSize(&disp, *fmt++,
					XFN_SEPARATOR | XFN_RIGHT | XFN_HEXUNIT | XFN_MINKILO);
				break;

			case DE_TIME1:	// ファイル時刻 -----------------------------------
				PaintDefaultTimeStamp(&disp, *fmt++);
				break;

			case DE_TIME2:	// ファイル時刻 -----------------------------------
				ShowTimeFormat(&disp, &fmt);
				break;

			case DE_ATTR1:	// ファイル属性 -----------------------------------
				PaintFileAttributes(&disp, *fmt++);
				break;

			case DE_MEMOS:	// C コメント(コメント無しの時は幅が0) ------------
				if ( disp.cell->comment == EC_NOCOMMENT ){
					fmt++;
					if ( cfmt->width > cfmt->org_width ){
						disp.Xd += disp.fontX * (cfmt->width - cfmt->org_width);
						break;
					}
					continue;
				}
				// DE_MEMO へ
			case DE_MEMO:	// C コメント -------------------------------------
				if ( disp.cell->comment != EC_NOCOMMENT ){
					TCHAR *cmtptr;

					cmtptr = ThPointerT(&cinfo->EntryComments, disp.cell->comment);
				#if !defined(UNICODE) && !defined(USEDIRECTX)
					if ( !IsTrue(UsePFont) ){
						DxTextOutRel(disp.DxDraw, hDC, cmtptr,
							min((int)tstrlen(cmtptr), (int)*fmt));
					} else
					#endif
					{
						disp.backbox.left = disp.Xd;
						disp.backbox.right = disp.Xd + *fmt * disp.fontX;
						DxDrawText(disp.DxDraw, disp.hDC, cmtptr, -1,
							&disp.backbox, GEN_DRAW_FLAGS);
					}
				}
				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += (*fmt++) * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;

			case DE_MSKIP:	// c コメントスキップ -----------------------------
				if ( disp.cell->comment != EC_NOCOMMENT ){
					fmt += GetDispFormatSkip(fmt);
				}
				continue;

			case DE_MEMOEX:		// u 拡張コメント -----------------------------
				DrawCommentEx(&disp, fmt);
				fmt += 2;
				break;

			case DE_sepline: {	// 区切り線 -----------------------------------
				int halflspc;

				if ( !disp.NoBack ){
					disp.lbox.left = disp.Xd;
					disp.lbox.right = disp.Xd + disp.fontX;
					DxFillBack(disp.DxDraw, hDC, &disp.lbox, disp.hbbr);
				}
				halflspc = disp.lspc >> 1;
				if ( line == 0 ){
					tempbox.top = Ytop;
				} else{
					tempbox.top = disp.lbox.top - halflspc;
				}
				tempbox.bottom = disp.lbox.bottom + disp.lspc - halflspc;
				tempbox.left = disp.Xd + (disp.fontX >> 1);
				tempbox.right = tempbox.left + 1;
				DxFillRectColor(disp.DxDraw, hDC, &tempbox, cinfo->linebrush, LINE_NORMAL);
				disp.Xd += disp.fontX;
				disp.LP.x += disp.fontX;
				continue;
			}
			case DE_hline:	// 水平線 -----------------------------------
				disp.lbox.left = disp.Xd;
				disp.lbox.right = disp.Xd + disp.fontX * *fmt++;
				if ( disp.lbox.left == disp.lbox.right ) disp.lbox.right = Xe;
				if ( !disp.NoBack ){
					DxFillBack(disp.DxDraw, hDC, &disp.lbox, disp.hbbr);
				}

				tempbox = disp.lbox;
				tempbox.top += (cinfo->fontY >> 1);
				tempbox.bottom = tempbox.top + 1;
				DxFillRectColor(disp.DxDraw, hDC, &tempbox, cinfo->linebrush, LINE_NORMAL);
				disp.Xd = disp.lbox.right;
				disp.LP.x += disp.lbox.right;
				continue;

			case DE_fcolor:	// O"color" 色 -----------------------------------
				disp.textc = disp.fc = *(COLORREF *)fmt;
				fmt += sizeof(COLORREF);
				disp.DSetCellTextColor(CTC_DX hDC, disp.textc);
				continue;

			case DE_bcolor:	// Ob"color" 色 -----------------------------------
				DxSetBkColor(disp.DxDraw, hDC, *(COLORREF *)fmt);
				fmt += sizeof(COLORREF);
				continue;

			case DE_fc_def:	// Odn 色 -----------------------------------
				disp.textc = disp.fc = C_info;
				fmt += DE_fc_def_SIZE - 1;
				disp.DSetCellTextColor(CTC_DX hDC, disp.textc);
				continue;

			case DE_bc_def:	// Ogn 色 -----------------------------------
				DxSetBkColor(disp.DxDraw, hDC, C_info);
				fmt += DE_bc_def_SIZE - 1;
				continue;

			case DE_FStype: { // ファイルシステム名 -------------------------
				int len;

				len = tstrlen32(cinfo->e.Dtype.Name);
				if ( len > 0 ){
					DxTextOutRel(disp.DxDraw, hDC, cinfo->e.Dtype.Name, len - 1);
					disp.Xd += disp.fontX * len - 1;
					DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
					if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				}
				break;
			}
			case DE_itemname:{	// アイテム名 -------------------------
				SIZE32_T len;
				COLORREF old;

				len = tstrlen32((TCHAR *)fmt);
				old = DxSetTextColor(disp.DxDraw, hDC, C_mes);
				DxTextOutRel(disp.DxDraw, hDC, (TCHAR *)fmt, len);
				DxSetTextColor(disp.DxDraw, hDC, old);
				fmt += TSTROFF(len + 1);

				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += len * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;
			}
			case DE_string:{	// 一般文字列 -------------------------
				size_t len;

				len = tstrlen((TCHAR *)fmt);
				DxTextOutRel(disp.DxDraw, hDC, (TCHAR *)fmt, len);
				fmt += TSTROFF(len + 1);

				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += len * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;
			}
			case DE_ivalue:{	// id別特殊環境変数 -------------------------
				SIZE32_T len;
				const TCHAR *ptr;

				ptr = ThGetString(&cinfo->StringVariable, (TCHAR *)fmt, NULL, 0);
				if ( ptr != NULL ){
					len = tstrlen32(ptr);
					DxTextOutRel(disp.DxDraw, hDC, ptr, len);
					DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
					disp.Xd += len * disp.fontX;
					if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				}
				fmt += TSTRSIZE((TCHAR *)fmt);
				break;
			}
			case DE_MNUMS:	// マーク数 -----------------------
				PaintInfoNumber(&disp, cinfo->e.markC);
				break;

			case DE_ALLPAGE:	// 全ページ数 -----------------------
				PaintInfoNumber(&disp, (cinfo->e.cellIMax /
					(cinfo->cel.Area.cx * cinfo->cel.Area.cy)) + 1);
				break;

			case DE_NOWPAGE:	// 現在ページ -----------------------
				PaintInfoNumber(&disp, (cinfo->cellWMin /
					(cinfo->cel.Area.cx * cinfo->cel.Area.cy)) + 1);
				break;

			case DE_MSIZE1:	// マークサイズ(桁区切りなし)------------------
				PaintInfoSize(&disp, cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
					CXFN_SUM_NOSEP, *fmt++);
				break;

			case DE_MSIZE2:	// マークサイズ(桁区切りあり)------------------
				PaintInfoSize(&disp, cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
					CXFN_SUM, *fmt++);
				break;

			case DE_MSIZE3:	// マークサイズ(桁あり, K)------------------
				PaintInfoSize(&disp, cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
					XFN_SEPARATOR | XFN_RIGHT | XFN_HEXUNIT | XFN_MINKILO,
					*fmt++);
				break;

			case DE_ENTRYSET:	// エントリ数----------------------------------
				PaintEntryCounts(&disp);
				break;

			case DE_ENTRYA0:	// エントリ数全部 -----------------------------
				PaintInfoNumber(&disp, cinfo->e.cellDataMax);
				break;

			case DE_ENTRYA1:	// エントリ数全部, ./.. 除く ------------------
				PaintInfoNumber(&disp,
						cinfo->e.cellDataMax - cinfo->e.AllRelativeDirs);
				break;

			case DE_ENTRYV0:	// エントリ数表示分のみ -----------------------
				PaintInfoNumber(&disp, cinfo->e.cellIMax);
				break;

			case DE_ENTRYV1:	// エントリ数表示分, ./.. 除く ----------------
				PaintInfoNumber(&disp, cinfo->e.cellIMax - cinfo->e.RelativeDirs);
				break;

			case DE_ENTRYV2:	// エントリ数表示分, dir のみ -----------------
				PaintInfoNumber(&disp, cinfo->e.Directories);
				break;

			case DE_ENTRYV3:	// エントリ数表示分, file のみ ----------------
				PaintInfoNumber(&disp, cinfo->e.cellIMax - cinfo->e.Directories - cinfo->e.RelativeDirs);
				break;

			case DE_DFREE1:	// 空き容量(桁区切りなし)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.free.u.LowPart, cinfo->DiskSizes.free.u.HighPart, CXFN_SUM_NOSEP, *fmt++);
				break;

			case DE_DFREE2:	// 空き容量(桁区切りあり)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.free.u.LowPart, cinfo->DiskSizes.free.u.HighPart, CXFN_SUM, *fmt++);
				break;

			case DE_DUSE1:	// 使用容量(桁区切りなし)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.used.u.LowPart, cinfo->DiskSizes.used.u.HighPart, CXFN_SUM_NOSEP, *fmt++);
				break;

			case DE_DUSE2:	// 使用容量(桁区切りあり)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.used.u.LowPart, cinfo->DiskSizes.used.u.HighPart, CXFN_SUM, *fmt++);
				break;

			case DE_DTOTAL1:	// 総容量(桁区切りなし)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.total.u.LowPart, cinfo->DiskSizes.total.u.HighPart, CXFN_SUM_NOSEP, *fmt++);
				break;

			case DE_DTOTAL2:	// 総容量(桁区切りあり)------------------
				PaintInfoSize(&disp, cinfo->DiskSizes.total.u.LowPart, cinfo->DiskSizes.total.u.HighPart, CXFN_SUM, *fmt++);
				break;

			case DE_WIDEV:	// 可変長指定(ここでは処理せず)------------------
			case DE_WIDEW:
				fmt += DE_WIDEV_SIZE - 1;
				break;

			case DE_vlabel:{ // ボリュームラベル ------------------------------
				int len;

				GetDriveVolumeName(cinfo);
				len = (int)tstrlen(cinfo->VolumeLabel);
				DxTextOutRel(disp.DxDraw, hDC, cinfo->VolumeLabel, len);
				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += *fmt++ * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;
			}
			case DE_path: // 現在のディレクトリ ------------------------------
				DrawPathText(CTC_DX hDC, cinfo);
				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += *fmt++ * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;

			case DE_pathmask: // 現在のディレクトリマスク --------------------
				DrawPathMaskText(CTC_DX hDC, cinfo);
				DxGetCurrentPositionEx(disp.DxDraw, hDC, &disp.LP);
				disp.Xd += *fmt++ * disp.fontX;
				if ( disp.Xd < disp.LP.x ) disp.Xd = disp.LP.x;
				break;

			case DE_COLUMN: // カラム拡張 --------------------
				DrawColumn(&disp, (DISPFMT_COLUMN *)fmt);
				fmt += ((DISPFMT_COLUMN *)fmt)->fmtsize - 1;
				break;

			case DE_MODULE: // MODULE拡張 --------------------
				disp.LP.y = disp.backbox.top;
				ModuleDraw(&disp, fmt, cellno);
				fmt += DE_MODULE_SIZE - 1;
				break;
		}
									// 後始末 ---------------------------------
		if ( disp.LP.x < disp.Xd ){ // 未描画の右側空欄を消去
			disp.lbox.left = disp.LP.x;
			disp.lbox.right = disp.Xd;
			if ( disp.hfbr != NULL ){
				DxFillBack(disp.DxDraw, hDC, &disp.lbox, disp.hfbr);
			}
		} else if ( disp.LP.x != disp.Xd ){
			DxMoveToEx(disp.DxDraw, hDC, disp.Xd, disp.lbox.top);
		}
		disp.LP.x = disp.Xd;

		if ( cfmt->nextline && bkdraw && disp.NoBack && !Check_Line && (disp.Xd < BaseBox->right) ){ // 複数行、壁紙あり、反転中表示の時、最終行の未描画部分を消去
			tempbox.left = disp.Xd;
			tempbox.right = BaseBox->right;
			tempbox.top = disp.lbox.top;
			tempbox.bottom = disp.lbox.top + cinfo->fontY;
			DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
		}

		if ( disp.Xd >= Xe ){
			if ( nextfmt ){ // 改行指定有り…次の行へ
				disp.LP.x = disp.Xd = disp.LineTopX;
				disp.backbox.top += cinfo->fontY;
				disp.lbox.top += cinfo->fontY;
				disp.lbox.bottom += cinfo->fontY;
				fmt = nextfmt;
				nextfmt = fmt + *(WORD *)(fmt - 2);
				line++;
				if ( nextfmt == fmt ){ // 最終行
					nextfmt = NULL;
					// 最終行より下が空欄になっているときは埋める
					if ( (disp.lbox.bottom < Ybottom) && (!disp.NoBack || bkdraw) ){
						tempbox.left = disp.Xd;
						tempbox.right = Xe;
						tempbox.top = disp.lbox.bottom;
						tempbox.bottom = Ybottom;
						if ( Check_Line ){
							tempbox.bottom -= XC_ulh;
							if ( tempbox.bottom <= tempbox.top ) continue;
						}
						DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
					}
						// 改行後の下側と、次の行の上側の行間の空白処理
				} else if ( disp.lspc && (!disp.NoBack || bkdraw) ){
					tempbox.left = disp.Xd;
					tempbox.right = maxX;
					tempbox.bottom = disp.lbox.top + cinfo->fontY;
					tempbox.top = tempbox.bottom - disp.lspc;
					DxFillBack(disp.DxDraw, hDC, &tempbox, disp.hbbr);
				}
				continue;
			}
			break;
		}
	}
	}
	if ( (disp.LP.x < maxX) && (disp.hfbr != NULL) ){ // 右側に空間が残った場合の処理
		disp.backbox.left = disp.LP.x;
		disp.backbox.right = maxX;
		DxFillBack(disp.DxDraw, hDC, &disp.backbox, disp.hback);
	}

	if ( (C_eInfo[ECS_SEPARATE] != C_AUTO) && (EI_No >= 0) ){ // 水平区切り線
		HBRUSH hB;

		hB = CreateSolidBrush(C_eInfo[
			(Check_Line && (XC_ulh <= 1)) ? Check_Line : ECS_SEPARATE]);
	// ※ 区切り線 と 線幅1のときの下線 は完全に重なる
		tempbox.left = BaseBox->left;
		tempbox.top = BaseBox->bottom - 1;
		tempbox.right = BaseBox->right;
		tempbox.bottom = BaseBox->bottom;
		DxFillRectColor(disp.DxDraw, hDC, &tempbox, hB, C_eInfo[Check_Line ? Check_Line : ECS_SEPARATE]);
		DeleteObject(hB);
	}

	if ( Check_Box || Check_Focusbox ){	// 枠 / 点線枠
		disp.backbox.top = Ytop;
		disp.backbox.left = disp.LineTopX;
		disp.backbox.right = disp.Xright;
		disp.backbox.bottom = Ybottom;
		if ( disp.LineTopX > Xleft ) disp.backbox.left--;

		if ( (*fmt == DE_BLANK) && (*(fmt + 2) == '\0') ){
			disp.backbox.right -= *(fmt + 1) * disp.fontX;
		}

		if ( (cfmt->fmt[0] != DE_IMAGE) && (disp.backbox.left > BaseBox->left) ){
			disp.backbox.left--;
		}

		if ( Check_Box ){	// 枠
			HBRUSH hB;

			hB = CreateSolidBrush((cinfo->CursorColor == C_AUTO) ?
					C_eInfo[Check_Box] : cinfo->CursorColor);
			DxFrameRectColor(disp.DxDraw, hDC, &disp.backbox, hB, C_eInfo[ECS_BOX]);
			DeleteObject(hB);
		}else{	// 点線枠
		#if DRAWMODE == DRAWMODE_GDI
			SetTextColor(hDC, C_WHITE);
			SetBkColor(hDC, C_BLACK);
			DrawFocusRect(hDC, &disp.backbox);
		#else
			IfGDImode(hDC)
			{
				SetTextColor(hDC, C_WHITE);
				SetBkColor(hDC, C_BLACK);
				DrawFocusRect(hDC, &disp.backbox);
			} else{
				DxDrawFrameRect(disp.DxDraw, hDC, &disp.backbox, C_eInfo[ECS_BOX]);
			}
		#endif
		}
	}
			// エントリ末尾のクリック可能エリア表示
	if ( X_stip[TIP_TAIL_WIDTH] /*&& ((cinfo->PopupPosType != PPT_FOCUS) || (cinfo->Tip.states & STIP_SHOWTAILAREA))*/ ){
//		resetflag(cinfo->Tip.states, STIP_SHOWTAILAREA);
		if ( (EI_No >= 0) &&
			 (cellno == cinfo->e.cellPoint) &&
			 (cinfo->MouseStat.mode != MOUSEMODE_DRAG) ){
			HBRUSH hB;
			int w;
			CELLRIGHTRANGES ranges;

			GetCellRightRanges(cinfo, &ranges);
			tempbox.right = BaseBox->right - ranges.TailRightOffset;
			if ( tempbox.right >= ranges.RightBorder ){
				tempbox.right = ranges.RightBorder;
			}
			tempbox.left = tempbox.right - ranges.TailWidth;

		#if 1 // 色固定
			hB = CreateSolidBrush( C_eInfo[ECS_UDCSR] );
			w = 1;
		#else // 色混合
		{
			COLORREF usecolor;

			usecolor = C_eInfo[ECS_UDCSR];
			hB = CreateSolidBrush( usecolor,
				RGB(GetPointColor(GetRValue(disp.bc), GetRValue(usecolor)),
					GetPointColor(GetGValue(disp.bc), GetGValue(usecolor)),
					GetPointColor(GetBValue(disp.bc), GetBValue(usecolor))));
			w = (Check_Focusbox || Check_Box) ? XC_ulh : 1;
		}
		#endif
			tempbox.top = BaseBox->top;
			tempbox.bottom = BaseBox->top + w;
			DxFillRectColor(disp.DxDraw, hDC, &tempbox, hB, C_eInfo[ECS_UDCSR]);
			tempbox.top = BaseBox->bottom - w;
			tempbox.bottom = BaseBox->bottom;
			DxFillRectColor(disp.DxDraw, hDC, &tempbox, hB, C_eInfo[ECS_UDCSR]);
			DeleteObject(hB);
		}
	}

	if ( disp.bc != cinfo->BackColor ) DeleteObject(disp.hbbr);
	if ( oldBkMode ) DxSetBkMode(disp.DxDraw, hDC, oldBkMode);
	return disp.LP.x;
}

int USEFASTCALL GetDispFormatSkip(const BYTE *fmt)
{
	int skipsize;

	skipsize = DispFormatSkipTable[*fmt];
	switch ( skipsize ){
		case -1: // DE_itemname, DE_string, DE_ivalue
			return GetDispFormatSkip_string(fmt);

		case -2: // DE_TIME2
			return GetDispFormatSkip_time2(fmt);

		case -3: // DE_COLUMN
			return GetDispFormatSkip_column(fmt);

		default: // その他
			return skipsize;
	}
}
