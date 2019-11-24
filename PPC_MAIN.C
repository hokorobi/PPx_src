/*-----------------------------------------------------------------------------
	Paper Plane cUI												main loop
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include <dbt.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define XTOUCH 0		// WM_TOUCH の解釈を有効にする
#define XSW 0			// スワイプ処理を有効にする

const CURSORMOVER MarkClick = { 0, 1, OUTTYPE_PAGE, OUTHOOK_MARK, OUTTYPE_STOP, OUTHOOK_EDGE};

typedef struct xtagGUITHREADINFO {
	DWORD cbSize;
	DWORD flags;
	HWND hwndActive;
	HWND hwndFocus;
	HWND hwndCapture;
	HWND hwndMenuOwner;
	HWND hwndMoveSize;
	HWND hwndCaret;
	RECT rcCaret;
} xGUITHREADINFO;

DefineWinAPI(BOOL, GetGUIThreadInfo, (DWORD idThread, xGUITHREADINFO *lpgui)) = INVALID_VALUE(impGetGUIThreadInfo);

#if XTOUCH
DefineWinAPI(BOOL, RegisterTouchWindow, (HWND hWnd, ULONG ulFlags));
DefineWinAPI(BOOL, GetTouchInputInfo, (HTOUCHINPUT hTouchInput, UINT cInputs, PTOUCHINPUT pInputs, int cbSize));
DefineWinAPI(BOOL, CloseTouchInputHandle, (HTOUCHINPUT hTouchInput));
#endif // XTOUCH

BOOL SkipTouchActive = FALSE; // WM_MOUSEACTIVE で、タッチ時に無意味な WM_MOUSEACTIVE がくるのをスキップするために使用する

BOOL IsCellDblClk(PPC_APPINFO *cinfo, LPARAM lParam)
{
	POINT pos;

	LPARAMtoPOINT(pos, lParam);
	return GetItemTypeFromPoint(cinfo, &pos, NULL) == PPCR_CELLTEXT;
}

BOOL USEFASTCALL IsTouchMessage(void)
{
	LPARAM info = GetMessageExtraInfo();

	if ( (info & POINTTYPE_SIG_MASK) != POINTTYPE_TOUCH ) return FALSE;
	return TRUE;
}

int USEFASTCALL GetPointType(void)
{
	LPARAM info = GetMessageExtraInfo();

	if ( (info & POINTTYPE_SIG_MASK) != POINTTYPE_TOUCH ) return LIT_MOUSE;
	if ( (info & POINTTYPE_TOUCH_MASK) == POINTTYPE_TOUCH_PEN ){
		return LIT_PEN;
	}
	return LIT_TOUCH;
}

#pragma argsused
VOID CALLBACK DelayLogShowProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	UnUsedParam(uMsg);UnUsedParam(dwTime);

	KillTimer(hWnd, idEvent);
	SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hWnd, NULL, TRUE);
	SendMessage(hWnd, EM_SCROLLCARET, 0, 0);
	return;
}

#pragma argsused
VOID CALLBACK HoverTipTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	PPC_APPINFO *cinfo;
	UnUsedParam(uMsg);UnUsedParam(dwTime);

	KillTimer(hWnd, idEvent);
	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if ( (cinfo->Tip.hTipWnd != NULL) && (cinfo->Tip.X_stip_mode == stip_mode_preview) ){
		return;
	}
	if ( GetFocus() == NULL ) return;

	if ( (cinfo->e.cellPoint >= 0) && (cinfo->e.cellPoint < cinfo->e.cellIMax) ){
		int mode = X_stip[TIP_HOVER_MODE];
		if ( mode == 0 ) mode = stip_mode_fileinfo;
		ShowEntryTip(cinfo, STIP_CMD_HOVER | STIP_CMD_MOUSE, mode, cinfo->e.cellPoint);
	}
	return;
}

void ReuseFix(PPCSTARTPARAM *psp)
{
	int singlecount = 0;
	DWORD size;
	PSPONE *pspo;

	pspo = psp->next;
	// パラメータがない→対象が１つのみなので補正不要
	// ※一体化窓の時は、前回の配置が必ず展開されるのでここでreturnしない
	if ( pspo == NULL ) return;
	for ( ; ; ){
		size = PSPONE_size(pspo);
		// ID が決まっていないなら割当てを行う
		if ( pspo->id.RegMode != PPXREGIST_IDASSIGN ){
			singlecount += PPxRegist(NULL, pspo->id.RegID, PPXREGIST_SEARCH_LIVEID + singlecount);
			pspo->id.RegMode = PPXREGIST_IDASSIGN;
		}
		pspo = (PSPONE *)(char *)((char *)pspo + size);
		if ( pspo->id.RegID[0] == '\0' ) break;
	}
}

void PostWindowClosed(void)
{
	RequestDestroyFlag = 1;
	if ( X_MultiThread || (MainThreadID != GetCurrentThreadId()) ){
		PostQuitMessage(EXIT_SUCCESS);
	}else{
		PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
	}
}

int CallKeyHook(PPC_APPINFO *cinfo, WORD key)
{
	PPXMKEYHOOKSTRUCT keyhookinfo;
	PPXMODULEPARAM pmp;

	keyhookinfo.key = key;
	pmp.keyhook = &keyhookinfo;
	return CallModule(&cinfo->info, PPXMEVENT_KEYHOOK, pmp, cinfo->KeyHookEntry);
}

void ExecDualParam(PPC_APPINFO *cinfo, const TCHAR *param)
{
	if ( (UTCHAR)*param == EXTCMD_CMD ){
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, param + 1, NULL, 0);
	}else{
		const WORD *key;

		cinfo->KeyRepeats = 0;
		if ( (UTCHAR)*param == EXTCMD_KEY ) param++;
		key = (WORD *)param;
		while( *key ) PPcCommand(cinfo, *key++);
	}
}

BOOL PPcToolbarCommand(PPC_APPINFO *cinfo, int id, int orcode)
{
	RECT box;
	TCHAR *p;

	box.bottom = -1;
	SendMessage(cinfo->hToolBarWnd, TB_GETRECT, (WPARAM)id, (LPARAM)&box);
	if ( box.bottom == -1 ){
		cinfo->PopupPosType = PPT_MOUSE;
	}else{
		cinfo->PopupPos.x = box.left;
		cinfo->PopupPos.y = box.bottom;
		ClientToScreen(cinfo->hToolBarWnd, &cinfo->PopupPos);
		cinfo->PopupPosType = PPT_SAVED;
	}
	p = GetToolBarCmd(cinfo->hToolBarWnd, &cinfo->thGuiWork, id);
	if ( p == NULL ) return FALSE;
	if ( orcode ){
		if ( orcode < 0x100 ){ // 右クリック
			if ( (UTCHAR)*p == EXTCMD_CMD ){
				if ( (*(p + 1) == '%') && (*(p + 2) == 'j') ){
					TCHAR path[VFPS];

					p += 3;
					GetLineParam((const TCHAR **)&p, path);
					PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, path, path, 0);
					PPcSHContextMenu(cinfo, path, NULL);
				}
			}
		}else{	// ドロップダウン
			WORD key;

			key = *(WORD *)(p + 1) | (WORD)orcode;
			if ( key == (K_s | K_raw | K_bs) ){
				key = K_raw | K_c | K_bs;
			}
			PPcCommand(cinfo, key);
		}
	}else{
		ExecDualParam(cinfo, p);
	}
	if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
	return TRUE;
}

LRESULT HeaderTrackCheck(PPC_APPINFO *cinfo, HWND hHeaderWnd, HD_NOTIFY *hdn)
{
	BYTE *fmt, *fmtp;
	HD_ITEM hdi;

	hdi.mask = HDI_LPARAM;
	if ( SendMessage(hHeaderWnd, HDM_GETITEM, hdn->iItem, (LPARAM)&hdi) == FALSE){
		return 1;
	}
	if ( hdi.lParam < 0 ) return 1;	// 未定義…トラック禁止
	fmt = cinfo->celF.fmt;
	fmtp = fmt + HIWORD(hdi.lParam);
	switch( *fmtp ){
		case DE_TIME1:
		case DE_SIZE2:
		case DE_SIZE3:
		case DE_SIZE4:
		case DE_SPC:	// 空白
		case DE_BLANK:	// 空白
		case DE_ATTR1:
		case DE_IMAGE:	// 画像
		case DE_LFN:
		case DE_SFN:
		case DE_LFN_MUL:
		case DE_LFN_LMUL:
		case DE_ICON2:
		case DE_COLUMN:
		case DE_MEMOEX:
		case DE_LFN_EXT:
		case DE_SFN_EXT:
			return 0;
		default:	// トラック禁止
			return 1;
	}
}

void HeaderTrack(PPC_APPINFO *cinfo, HWND hHeaderWnd, HD_NOTIFY *hdn)
{
	BYTE *fmt, *fmtp;
	int width, oldw = 0;
	HD_ITEM hdi;

	hdi.mask = HDI_LPARAM;
	if ( SendMessage(hHeaderWnd, HDM_GETITEM, hdn->iItem, (LPARAM)&hdi) == FALSE){
		return;
	}
	if ( hdi.lParam < 0 ) return;	// 未定義
	fmt = cinfo->celF.fmt;
	width = (hdn->pitem->cxy + (cinfo->fontX / 2)) / cinfo->fontX;
	if ( width == 0 ) width = 1;
	if ( width > 255 ) width = 255;

	fmtp = fmt + HIWORD(hdi.lParam);
	switch( *fmtp ){
		case DE_TIME1:
			if ( width > 21 ) width = 21;
			oldw = *(fmtp + 1);
			*(fmtp + 1) = (BYTE)width;
			oldw = width - oldw;
			break;

		case DE_ATTR1:
			if ( width > 10 ) width = 10;

		case DE_SIZE2:
		case DE_SIZE3:
		case DE_SIZE4:
			if ( width < 4 ) width = 4;
		case DE_SPC:	// 空白
		case DE_BLANK:	// 空白
		case DE_IMAGE:	// 画像
		case DE_MEMOEX:
			oldw = *(fmtp + 1);
			if ( (*fmtp == DE_IMAGE) && (width < 1) ) width = 1;
			*(fmtp + 1) = (BYTE)width;
			oldw = width - oldw;
			break;

		case DE_LFN:
		case DE_SFN:
		case DE_LFN_MUL:
		case DE_LFN_LMUL:
		case DE_LFN_EXT:
		case DE_SFN_EXT: {
			int ext;

			oldw = *(fmtp + 1);
			ext = *(fmtp + 2);
			if ( ext == 255 ) ext = 0;
			if ( width <= ext ) width = ext + 1;
			*(fmtp + 1) = (BYTE)(int)(width - ext);
			oldw = (width - ext) - oldw;
			break;
		}

		case DE_ICON2:
			if ( (hdn->pitem->cxy < (4 + ICONBLANK)) ||
				 (hdn->pitem->cxy > (128 + ICONBLANK)) ) break;
			oldw = *(fmtp + 1);
			*(fmtp + 1) = (BYTE)(hdn->pitem->cxy - ICONBLANK);
			oldw = hdn->pitem->cxy - ICONBLANK - oldw;
			break;

		case DE_COLUMN:
			oldw = ((DISPFMT_COLUMN *)(fmtp + 1))->width;
			((DISPFMT_COLUMN *)(fmtp + 1))->width = (BYTE)width;
			oldw = width - oldw;
			break;
	}
	cinfo->celF.width += oldw;
	InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
}

void HeaderRClick(PPC_APPINFO *cinfo)
{
	TCHAR buf[16];
	HD_HITTESTINFO hhti;
	HD_ITEM hdi;

	cinfo->PopupPosType = PPT_MOUSE;
	hdi.lParam = 0xffff; // -1, -1
	GetMessagePosPoint(hhti.pt);
	ScreenToClient(cinfo->hHeaderWnd, &hhti.pt);
	if ( 0 <= SendMessage(cinfo->hHeaderWnd, HDM_HITTEST, 0, (LPARAM)&hhti) ){
		hdi.mask = HDI_LPARAM | HDI_FORMAT;
		if ( SendMessage(cinfo->hHeaderWnd, HDM_GETITEM, hhti.iItem, (LPARAM)&hdi) == FALSE ){
			hdi.lParam = 0xffff; // -1, -1
		}
	}
	wsprintf(buf, T("%d"), (char)hdi.lParam);
	ThSetString(&cinfo->StringVariable, T("HeaderSortU"), buf);
	wsprintf(buf, T("%d"), (char)(hdi.lParam >> 8));
	ThSetString(&cinfo->StringVariable, T("HeaderSortD"), buf);

	if ( IsTrue(PPcMouseCommand(cinfo, T("R"), T("HEAD"))) ) return;
	PPcDirContextMenu(cinfo);
}

void HeaderClick(PPC_APPINFO *cinfo, HWND hHeaderWnd, int index, int button)
{
	if ( button == 0 ){	// 左クリック
		HD_ITEM hdi;
		XC_SORT sort;
		const TCHAR *filename;

		hdi.mask = HDI_LPARAM | HDI_FORMAT;
		if ( SendMessage(hHeaderWnd, HDM_GETITEM, index, (LPARAM)&hdi) == FALSE ){
			return;
		}
		if ( hdi.lParam < 0 ) return;	// 未定義

		// ソート
		memset(&sort, 0, sizeof(sort));
		if ( hdi.lParam & 0x8000 ){ // column拡張 sort
			cinfo->sort_columnindex = sort.option = hdi.lParam & 0x7fff;
			hdi.lParam = SORT_COLUMN_UP + (SORT_COLUMN_DOWN << 8);
		}else{
			if ( !(hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) ){
			// サイズ、日付は、降順を始めにする。他は、昇順が始め
				char modedat = (char)hdi.lParam;

				if ( (modedat >= 2) && (modedat <= 5) ){
					setflag(hdi.fmt, HDF_SORTUP);
				}
			}
			sort.option = cinfo->XC_sort.option;
		}
		if ( hdi.fmt & HDF_SORTUP ) hdi.lParam >>= 8;
//		sort.mode.dat[(GetShiftKey() & K_c) ? 1 : 0] = (char)hdi.lParam;
		sort.mode.dat[0] = (char)hdi.lParam;

		filename = CEL(cinfo->e.cellN).f.cFileName;
		sort.atr = cinfo->XC_sort.atr;
		CellSort(cinfo, &sort);
		cinfo->DrawTargetFlags = DRAWT_ALL;
		InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
		if ( XC_acsr[1] ) FindCell(cinfo, filename);
		FixHeader(cinfo); // ソート状態の表示を更新
	}else{
		cinfo->PopupPosType = PPT_MOUSE;
		PPcLayoutCommand(cinfo, NilStr);
	}
}

void SetCaptionCombo(PPC_APPINFO *cinfo, HWND hWnd)
{
	TCHAR buf[VFPS + 80];

	GetWindowText(cinfo->info.hWnd, buf, TSIZEOF(buf));
	SetWindowText(hWnd, buf);
}

#pragma argsused
void CALLBACK DDTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	PPC_APPINFO *cinfo;
	UnUsedParam(uMsg); UnUsedParam(idEvent); UnUsedParam(dwTime);

	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( !cinfo->DDpagemode ){
		MoveWinOff(cinfo, cinfo->DDpage);
	}else{
		SendMessage(cinfo->hTreeWnd, VTM_SCROLL, 0, 0);
	}
}

void SetDDScroll(PPC_APPINFO *cinfo, POINT *pos)
{
	int ddpage = 0;

	if ( pos != NULL ){
		if ( (cinfo->MousePush == MOUSE_MARK) ||
			 (pos->x >= cinfo->BoxEntries.left) ){

			cinfo->DDpagemode = 0;
			if ( cinfo->ScrollBarHV == SB_HORZ ){ // 水平
				if ( pos->x < (cinfo->BoxEntries.left + cinfo->fontX * 2) ){
					ddpage = -cinfo->cel.Area.cy;
				}else if ( pos->x >
							(cinfo->BoxEntries.right - (cinfo->fontX * 2)) ){
					ddpage = cinfo->cel.Area.cy;
				}
			}else{ // 垂直
				if ( pos->y < (cinfo->BoxEntries.top + cinfo->fontY * 2) ){
					ddpage = pos->y - (cinfo->BoxEntries.top + cinfo->fontY*2);
				}else if ( pos->y >
							(cinfo->BoxEntries.bottom - (cinfo->fontY * 2)) ){
					ddpage = pos->y -
							(cinfo->BoxEntries.bottom - (cinfo->fontY * 2));
				}
				ddpage /= (cinfo->fontY / 4);
			}
		}else{
			cinfo->DDpagemode = 1;
			ddpage = 1;
		}
	}
	if ( ddpage != 0 ){
		cinfo->DDpage = ddpage;
		if ( cinfo->ddtimer_id == 0 ){
			cinfo->ddtimer_id = SetTimer(cinfo->info.hWnd,
					TIMERID_DRAGSCROLL, TIMER_DRAGSCROLL, DDTimerProc);
		}
	}else{
		if ( cinfo->ddtimer_id != 0 ){
			KillTimer(cinfo->info.hWnd, TIMERID_DRAGSCROLL);
			cinfo->ddtimer_id = 0;
		}
	}
}

BOOL USEFASTCALL MoveWinOff(PPC_APPINFO *cinfo, int offset)
{
	return MoveCellCsr(cinfo, offset, &XC_mvSC);
}

void DoMouseAction(PPC_APPINFO *cinfo, const TCHAR *button, LPARAM lParam)
{
	POINT pos;
	int area;

	cinfo->PopupPosType = PPT_MOUSE;
	if ( GetFocus() != cinfo->info.hWnd ) return;
	LPARAMtoPOINT(pos, lParam);
	area = GetItemTypeFromPoint(cinfo, &pos, NULL);

	if ( cinfo->PushArea != area ) return;
	PPcMouseCommand(cinfo, button, (const TCHAR *)(LONG_PTR)area);
}

void H_WheelMouse(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	DoMouseAction(cinfo, (HISHORTINT(wParam) < 0) ? T("H") : T("I"), lParam);
}

void WheelMouse(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	int now;

	now = PPxWheelMouse(&cinfo->MouseStat, cinfo->info.hWnd, wParam, lParam);
	if ( now == 0 ) return;

#ifdef USEDIRECTWRITE
	if ( GetAsyncKeyState(VK_MENU) & KEYSTATE_PUSH ){
		SetRotate(cinfo->DxDraw, now * 2);
		InvalidateRect(cinfo->info.hWnd, NULL, TRUE);
		return;
	}
#endif
	if ( wParam & MK_CONTROL ){ // Ctrl…フォントサイズ・透明度変更
		if ( cinfo->MouseStat.PushButton != MOUSEBUTTON_CANCEL ){
			PPxCancelMouseButton(&cinfo->MouseStat);
		}
		if ( now != 0 ){
			if ( wParam & MK_SHIFT ){
				PPxCommonCommand(cinfo->info.hWnd, 0, (now > 0) ? (WORD)(K_c | K_s | K_v | VK_ADD) : (WORD)(K_c | K_s | K_v | VK_SUBTRACT));
			}else{
				SetMag(cinfo, (now > 0) ? 10 : -10 );
			}
		}
		return;
	}

	if ( cinfo->combo ){
		POINT pos;
		HWND hTwnd;

		LPARAMtoPOINT(pos, lParam);
		hTwnd = WindowFromPoint(pos);

		if ( wParam & MK_RBUTTON ){ // 右クリック…タブ切り替え
			if ( cinfo->MouseStat.PushButton != MOUSEBUTTON_CANCEL ){
				PPxCancelMouseButton(&cinfo->MouseStat);
			}
			PostMessage(cinfo->hComboWnd, WM_PPXCOMMAND,
					TMAKELPARAM(KCW_tabrotate, now), (LPARAM)hTwnd);
			return;
		}
		if ( (X_awhel == FALSE) && (hTwnd != cinfo->info.hWnd) ){
			if ( IDW_TABCONTROL == GetWindowLongPtr(hTwnd, GWLP_ID) ){
				SendMessage(hTwnd, WM_MOUSEWHEEL, wParam, lParam);
				return;
			}
		}
	}
	// スクロール動作
	if ( (now <= -3) || (now >= 3) ) now /= 3;
	if ( XC_page ){
		if ( wParam & MK_MBUTTON ){
			now = now * cinfo->cel.Area.cy;
		}else{
			int X_wheel = 3;

			GetCustData(T("X_wheel"), &X_wheel, sizeof(X_wheel));
			now = now * X_wheel;
		}
		MoveCellCsr(cinfo, -now, &XC_mvWH);
	}else{
		if ((XC_fwin == 2) || (wParam & MK_MBUTTON)) now *= cinfo->cel.Area.cx;
		MoveCellCsr(cinfo, now * -cinfo->cel.Area.cy, &XC_mvWH);
	}
}

/*
	マウス	WM_MOUSEACTIVATE → WM_xMOUSEDOWN → WM_xMOUSEUP
	タッチ	WM_MOUSEACTIVATE(種別判別不可) → WM_MOUSEACTIVATE(タッチ)
			→ WM_xMOUSEDOWN → WM_xMOUSEUP
*/

LRESULT USEFASTCALL PPcMouseActive(HWND hWnd, PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	DEBUGLOGC("WM_MOUSEACTIVATE start", 0);

	if ( SkipTouchActive ){
		SkipTouchActive = FALSE;
		if ( IsTouchMessage() && (GetFocus() == hWnd) ){
			return MA_ACTIVATEANDEAT; // アクティブ & マウス メッセージを廃棄
		}
	}

	// 非アクティブ状態でのドラッグ検出処理
	if ( (HIWORD(lParam) == WM_LBUTTONDOWN) ||
		 (HIWORD(lParam) == WM_RBUTTONDOWN) ){
		POINT pos;
		int areatype;
		ENTRYINDEX celln;

		GetMessagePosPoint(pos);
		ScreenToClient(hWnd, &pos);
		areatype = GetItemTypeFromPoint(cinfo, &pos, &celln);
		if ( ((areatype >= PPCR_CELLTEXT) &&
			  ( !(X_askp || (TouchMode & TOUCH_ACTIONWITHACTIVE)) ||
				(celln == cinfo->e.cellN)) ) ||
			 (areatype == PPCR_INFOICON) ){
			cinfo->MouseUpFG = 1;

			DEBUGLOGC("WM_MOUSEACTIVATE noactive", 0);
			return MA_NOACTIVATE; // 非アクティブ & ドラッグ検出
		}
	}

	if ( X_askp || (TouchMode & TOUCH_ACTIONWITHACTIVE) ){ //要アクティブ化検出
		// アクティブ化のみ
		if ( (cinfo->X_inag & INAG_UNFOCUS) || (GetFocus() == NULL) ){
			cinfo->MouseStat.PushButton = MOUSEBUTTON_CANCEL_DOUBLE; // DblClickを無効
			// アクティブ時のクリック検出を無効にする
			if ( (TouchMode & TOUCH_ACTIONWITHACTIVE) && !IsTouchMessage() ){
				SkipTouchActive = TRUE;
			}
			if ( cinfo->combo ) SetFocus(hWnd);
			return MA_ACTIVATEANDEAT; // アクティブ & マウス メッセージを廃棄
		}else{
			return MA_ACTIVATE; // アクティブ継続
		}
	}else{
		// アクティブ化+選択
		if ( cinfo->combo ){
			if ( GetFocus() == hWnd ){
				return MA_ACTIVATE;
			}else{
				SetFocus(hWnd);
			}
		}
		DEBUGLOGC("WM_MOUSEACTIVATE end", 0);
		return DefWindowProc(hWnd, WM_MOUSEACTIVATE, wParam, lParam);
	}
}

void USEFASTCALL PPcMinMaxJoinFix(PPC_APPINFO *cinfo, MINMAXINFO *minfo)
{
	HWND jHWnd;

	jHWnd = GetJoinWnd(cinfo);
	// 連結反対窓有り／同時起動＆作業中（遅延対策）なら補正する
	if ( (jHWnd != NULL) ||
		 ((cinfo->swin & SWIN_WBOOT) && (cinfo->swin & SWIN_BUSY))){
		RECT deskrect;

		GetDesktopRect(cinfo->info.hWnd, &deskrect);
		if ( cinfo->WinPos.show == SW_SHOWNORMAL ){
			minfo->ptMaxPosition.x = 0;
			minfo->ptMaxPosition.y = 0;
		}else{
			minfo->ptMaxPosition.x = deskrect.left;
			minfo->ptMaxPosition.y = deskrect.top;
		}
		minfo->ptMaxSize.x = deskrect.right - deskrect.left;
		minfo->ptMaxSize.y = deskrect.bottom - deskrect.top;

		if (cinfo->swin & SWIN_UDJOIN){	// 上
			minfo->ptMaxSize.y >>= 1;
			if ( !((cinfo->RegID[2] & PAIRBIT) ^
					((cinfo->swin & SWIN_SWAPMODE) ? 1 : 0)) ){
				minfo->ptMaxPosition.y += minfo->ptMaxSize.y;
			}
		}else{
			minfo->ptMaxSize.x >>= 1;
			if ( !((cinfo->RegID[2] & PAIRBIT) ^
					((cinfo->swin & SWIN_SWAPMODE) ? 1 : 0)) ){
				minfo->ptMaxPosition.x += minfo->ptMaxSize.x;
			}
		}
	}
}

LRESULT USEFASTCALL PPcNotify(PPC_APPINFO *cinfo, NMHDR *nmh)
{
	if ( nmh->hwndFrom == NULL ) return 0;

	if ( nmh->hwndFrom == cinfo->hToolBarWnd ){
		// TB_HITTEST も検討
		if ( nmh->code == TBN_DROPDOWN ){
			PPcToolbarCommand(cinfo, ((LPNMTOOLBAR)nmh)->iItem, K_s);
		}
		if ( nmh->code == NM_RCLICK ){
			if ( ((LPNMTOOLBAR)nmh)->iItem > 0 ){
				if ( IsTrue(PPcToolbarCommand(cinfo, ((LPNMTOOLBAR)nmh)->iItem, 1)) ){
					return 0;
				}
			}
			cinfo->PopupPosType = PPT_MOUSE;
			PPcLayoutCommand(cinfo, NilStr);
		}
		return 0;
	}
	if ( IsTrue(DocksNotify(&cinfo->docks, nmh)) ){
		if ( nmh->code == RBN_HEIGHTCHANGE ){
			WmWindowPosChanged(cinfo);
		}
		return 0;
	}
	if ( nmh->code == TTN_NEEDTEXT ){
		if ( SetToolBarTipText(cinfo->hToolBarWnd, &cinfo->thGuiWork, nmh) ) return 0;
		if ( DocksNeedTextNotify(&cinfo->docks, nmh) ) return 0;
		return 0;
	}
	if ( nmh->hwndFrom == cinfo->hHeaderWnd ){
		switch (nmh->code){
			case NM_RCLICK:
				HeaderRClick(cinfo);
				return 0;

			case HDN_BEGINDRAG:
				return 1;	// ドラッグを禁止する

			case HDN_BEGINTRACK:
				return HeaderTrackCheck(cinfo, nmh->hwndFrom, (HD_NOTIFY *)nmh);

			case HDN_ENDTRACK:
				InitCli(cinfo);
				WmWindowPosChanged(cinfo);
				FixHeader(cinfo);
				InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
				break;

			case HDN_TRACK:
				HeaderTrack(cinfo, nmh->hwndFrom, (HD_NOTIFY *)nmh);
				break;
// HDN_DIVIDERDBLCLICK
			case HDN_ITEMCLICK:
//			case HDN_ITEMDBLCLICK:
				HeaderClick(cinfo, nmh->hwndFrom, ((HD_NOTIFY *)nmh)->iItem, ((HD_NOTIFY *)nmh)->iButton);
				break;
			case HDN_ITEMSTATEICONCLICK:
				cinfo->MarkMask = MARKMASK_DIRFILE;
				PPC_AllMark(cinfo);
				break;
		}
	}
	return 0;
}

void SetCountedDirectorySize(PPC_APPINFO *cinfo, struct dirinfo *di)
{
	ENTRYCELL *cell;
	ENTRYINDEX index;
	TCHAR dirpath[VFPS];
	BYTE *hist;
	WORD datasize;

	if ( tstrcmp(di->path, cinfo->path) != 0 ) return;

	for ( index = 0 ; index < cinfo->e.cellIMax ; index++ ){
		cell = &CEL(index);
		if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) continue;
 		if ( tstrcmp(CellFileName(cell), di->name) ) continue;

		if ( IsCellPtrMarked(cell) ){
			SubDD(cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
					cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
		}
		cell->f.nFileSizeLow = di->low;
		cell->f.nFileSizeHigh = di->high;
		if ( IsCellPtrMarked(cell) ){
			AddDD(cinfo->e.MarkSize.l, cinfo->e.MarkSize.h, di->low, di->high);
		}
		cell->attr = (BYTE)((cell->attr & ~ECA_DIRG) | ECA_DIRC);

		// コメントにサイズを登録
		if ( XC_szcm == 1 ){
			TCHAR buf[24];

			FormatNumber(buf, XFN_SEPARATOR, 22,
					cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
			SetComment(cinfo, 0, cell, buf);
		}

		// ヒストリにサイズを保存
		VFSFullPath(dirpath, CellFileName(cell), cinfo->path);
		UsePPx();
		hist = (BYTE *)SearchHistory(PPXH_PPCPATH, dirpath);
		if ( hist == NULL ){
			datasize = 0;
		}else{
			datasize = GetHistoryDataSize(hist);
		}

		if ( datasize < ( sizeof(DWORD) * 4 ) ){
			if ( XC_szcm == 2 ){ // ヒストリに保存する設定
				DWORD tmp[4];

				if ( datasize >= ( sizeof(DWORD) * 2 ) ){
					// カーソル位置を回収
					memcpy(&tmp, GetHistoryData(hist), sizeof(DWORD) * 2);
				}else{
					tmp[1] = MAX32; // カーソル位置情報無し
				}
				tmp[2] = cell->f.nFileSizeLow;
				tmp[3] = cell->f.nFileSizeHigh;
				WriteHistory(PPXH_PPCPATH, dirpath, sizeof(DWORD) * 4, &tmp);
			}
		}else{ // 既にサイズを保存しているので更新する
			DWORD *WritePtr = (DWORD *)(BYTE *)GetHistoryData(hist);

			WritePtr[2] = cell->f.nFileSizeLow;
			WritePtr[3] = cell->f.nFileSizeHigh;
		}
		FreePPx();
		return;
	}
}

BOOL USEFASTCALL WmCopyData(PPC_APPINFO *cinfo, COPYDATASTRUCT *copydata, WPARAM wParam)
{
	switch ( LOWORD(copydata->dwData) ){
		case KC_ADDENTRY: // 未使用？
			InsertEntry(cinfo, -1, NilStr, (WIN32_FIND_DATA *)copydata->lpData);
			return TRUE;

		case KC_MOREPPC:
			SendCallPPc(copydata);
			return TRUE;

		case K_WINDDOWLOG:
			WmPPxCommand(NULL, K_WINDDOWLOG, (LPARAM)copydata->lpData);
			return TRUE;

		case 0x100 + 'O': // (比較マーク)指定エントリにマークをする
			if ( HIWORD(copydata->dwData) == (WORD)cinfo->LoadCounter ){
				ENTRYINDEX index;

				for ( index = 0 ; index < cinfo->e.cellIMax ; index++ ){
					if ( !tstrcmp(CellFileNameIndex(cinfo, index),
							(TCHAR *)copydata->lpData) ){
						cinfo->MarkMask = MARKMASK_DIRFILE;
						CellMark(cinfo, index, MARK_CHECK);
						RefleshCell(cinfo, index);
						RefleshInfoBox(cinfo, DE_ATTR_MARK);
						break;
					}
				}
			}
			return TRUE;

		case 'O':{ // 比較マークを実行する
			COMPAREMARKPACKET cmp;

			memcpy((char *)&cmp, copydata->lpData, sizeof(cmp));

			if ( (cmp.mode & CMPDOWN) || !(cmp.mode & CMPWAIT) || ((cmp.mode & 0x7f) == CMP_BINARY) ){
				ReplyMessage(TRUE);
			}

			PPcCompareMain(cinfo, (HWND)wParam, &cmp);
			if ( !(cmp.mode & CMPDOWN) ){		// 上りだったので下り処理をする
				setflag(cmp.mode, CMPDOWN);
				PPcCompareSend(cinfo, &cmp, (HWND)wParam);
			}else{							// 処理後、終了処理
				DeleteFileL(cmp.filename);
			}
			return TRUE;
		}

		case 0x200 + '=': // 指定パスを表示 (PPcディレクトリ指定用)
			if ( IsTrue(cinfo->ChdirLock) ){
				PPCuiWithPathForLock(cinfo, (const TCHAR *)copydata->lpData);
				return TRUE;
			}
		// '=' へ
		case 0x100 + '=': // 指定パスを表示(swap用)
		case '=':{  // 指定パスを表示(非同期) ( '='用 )
			TCHAR *p;

			if ( copydata->cbData >= sizeof(cinfo->path) ) return FALSE;
			SetPPcDirPos(cinfo);
			tstrcpy(cinfo->path, (TCHAR *)copydata->lpData);
#ifndef UNICODE
			if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT )
#endif
			{
				if ( LOWORD(copydata->dwData) == '=' ) ReplyMessage(TRUE);
			}
			p = tstrrchr(cinfo->path, '\1');

			if ( p != NULL ){
				*p = '\0';
				tstrcpy(cinfo->Jfname, p + 1);
				read_entry(cinfo, RENTRY_JUMPNAME | RENTRY_NOHIST);
			}else{
				read_entry(cinfo, RENTRY_READ);
			}
			return TRUE;
		}

		case 0x100 + 'H': // コマンド実行
		case 'H': // コマンド実行(非同期)
			return RecvExecuteByWMCopyData(&cinfo->info, copydata);

		case KC_StDS:
			SetCountedDirectorySize(cinfo, (struct dirinfo *)copydata->lpData);
			return TRUE;

//		dafault:
	}
	return FALSE;
}

void ClearMark(PPC_APPINFO *cinfo)
{
	ENTRYINDEX index;
	DWORD attrmask;

	attrmask = ~cinfo->MarkMask & FILE_ATTRIBUTE_DIRECTORY;

	for ( index = 0 ; index < cinfo->e.cellIMax ; index++ ){
		if ( IsCEL_Marked(index) && !(CEL(index).f.dwFileAttributes & attrmask) ){
			CellMark(cinfo, index, MARK_REMOVE);
			RefleshCell(cinfo, index);
		}
	}
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
}

BOOL ExplorerTypeMark(PPC_APPINFO *cinfo, WPARAM wParam)
{
	if ( XC_msel[0] && !(wParam & (MK_SHIFT | MK_CONTROL)) ){
		cinfo->MarkMask = MARKMASK_DIRFILE;
		if ( cinfo->e.markC != 0 ) ClearMark(cinfo);
		return TRUE;
	}
	return FALSE;
}

// クリック位置がマークしていないときには、マークを全部解除して、クリック位置のみマークする
void ExplorerTypeMark_solo(PPC_APPINFO *cinfo, WPARAM wParam, int cellnum)
{
	cinfo->MarkMask = MARKMASK_DIRFILE;
	ExplorerTypeMark(cinfo, wParam);
	CellMark(cinfo, cellnum, MARK_CHECK);
	RefleshCell(cinfo, cellnum);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	if ( cinfo->FullDraw == 0 ) UpdateWindow_Part(cinfo->info.hWnd);
}

void CheckGesture(PPC_APPINFO *cinfo)
{
	TCHAR buf[GESTURETEXTSIZE];

	if ( PPxCheckMouseGesture(&cinfo->MouseStat, buf, T("MC_click")) == FALSE ){
		return;
	}
	SetPopMsg(cinfo, POPMSG_PROGRESSMSG, buf);
}

void DoubleClickMouse(PPC_APPINFO *cinfo, LPARAM lParam)
{
	TCHAR click[3];

	if ( cinfo->MouseStat.PushButton <= MOUSEBUTTON_CANCEL ) return;
	KillTimer(cinfo->info.hWnd, TIMERID_DRAGSCROLL);
	if ( GetFocus() != cinfo->info.hWnd ) return;

	if ( (cinfo->MouseStat.PushClientPoint.x < 0) ||
		 (cinfo->MouseStat.PushClientPoint.y < 0) ||
		 (cinfo->MouseStat.PushClientPoint.x >= cinfo->wnd.NCArea.cx) ||
		 (cinfo->MouseStat.PushClientPoint.y >= cinfo->wnd.NCArea.cy) ){
		return;
	}
	cinfo->MouseUpFG = 0;
	click[0] = PPxMouseButtonChar[cinfo->MouseStat.PushButton];
	click[1] = 'D';
	click[2] = '\0';

	if ( click[0] == 'L' ){
		int area;
		ENTRYINDEX n;

		area = GetItemTypeFromPoint(cinfo, &cinfo->MouseStat.PushClientPoint, &n);

		if ( cinfo->PushArea != area ) return;
		if ( (area == PPCR_CELLTEXT) || (area == PPCR_INFOICON) ){
			if ( (CEL(n).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					(GetShiftKey() & K_s) ){
				TCHAR buf[VFPS];

				tstrcpy(buf, T("EXPLORER /n,/e,"));
				if ( NULL != VFSFullPath(buf + tstrlen(buf),
						CellFileNameIndex(cinfo, n), cinfo->RealPath) ){
					ComExec(cinfo->info.hWnd, buf, cinfo->path);
				}else{
					SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
				}
				return;
			}
		}
	}
	DoMouseAction(cinfo, click, lParam);
}

void USEFASTCALL WmCapturechanged(PPC_APPINFO *cinfo)
{
	KillTimer(cinfo->info.hWnd, TIMERID_DRAGSCROLL);
	if ( cinfo->MousePush == MOUSE_MARK ){
		SetDDScroll(cinfo, NULL);
		cinfo->DrawTargetFlags = DRAWT_ALL;
		InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
	}
	if ( cinfo->MouseStat.mode != MOUSEMODE_NONE ){ // 意図しないReleaseCapture
		PPxCancelMouseButton(&cinfo->MouseStat);
		cinfo->MousePush = MOUSE_CANCEL;
		StopPopMsg(cinfo, PMF_DISPLAYMASK);
	}
}

void USEFASTCALL PPcScrollBar(PPC_APPINFO *cinfo, UINT message, WORD scrollcode)
{
	switch (scrollcode){
								// 一番上 .............................
		case SB_TOP:
			MoveWinOff(cinfo, -cinfo->winOmax * cinfo->cel.Area.cy);
			break;
								// 一番下 .............................
		case SB_BOTTOM:
			MoveWinOff(cinfo, cinfo->winOmax * cinfo->cel.Area.cy);
			break;
								// 一行上 .............................
		case SB_LINEUP:
			MoveWinOff(cinfo, XC_page ? -1 : -cinfo->cel.Area.cy);
			break;
								// 一行下 .............................
		case SB_LINEDOWN:
			MoveWinOff(cinfo, XC_page ? 1 : cinfo->cel.Area.cy);
			break;
								// 一頁上 .............................
		case SB_PAGEUP:
			MoveWinOff(cinfo, -cinfo->cel.Area.cx * cinfo->cel.Area.cy);
			break;
								// 一頁下 .............................
		case SB_PAGEDOWN:
			MoveWinOff(cinfo, cinfo->cel.Area.cx * cinfo->cel.Area.cy);
			break;
								// 特定位置まで移動中 .................
		case SB_THUMBTRACK:
			//SB_THUMBPOSITION へ続く
								// 特定位置まで移動 ...................
		case SB_THUMBPOSITION:{
			SCROLLINFO scri;

			scri.cbSize = sizeof(scri);
			scri.fMask = SIF_TRACKPOS | SIF_PAGE;
			GetScrollInfo(cinfo->hScrollTargetWnd,
					(cinfo->hScrollBarWnd != NULL ) ? SB_CTL :
					((message == WM_VSCROLL) ? SB_VERT : SB_HORZ), &scri);
			if ( XC_page ){
					MoveWinOff(cinfo, scri.nTrackPos - cinfo->cellWMin);
			}else{
/* なめらかスクロール開発コード
				if ( scrollcode == SB_THUMBPOSITION ){
					cinfo->EntriesOffset.x = 0;
				}else{
					cinfo->EntriesOffset.x = -(
						(scri.nTrackPos - cinfo->cellWMin) * cinfo->cel.Size.cx / (cinfo->cel.Area.cy) );
				}
				cinfo->DrawTargetFlags = DRAWT_ALL;
				InvalidateRect(cinfo->info.hWnd, NULL, TRUE);
*/
				MoveWinOff(cinfo, (scri.nTrackPos + cinfo->cel.Area.cy / 2) /
						cinfo->cel.Area.cy * cinfo->cel.Area.cy -
						cinfo->cellWMin);
			}
			break;
		}
		// default:
	}
}

void USEFASTCALL RunGesture(PPC_APPINFO *cinfo)
{
	TCHAR buf[CMDLINESIZE];

	StopPopMsg(cinfo, PMF_PROGRESS);
	wsprintf(buf, T("RG_%s"), cinfo->MouseStat.gesture.step);
	cinfo->MouseStat.gesture.count = 0;
	if ( NO_ERROR == GetCustTable(T("MC_click"), buf, buf, sizeof(buf)) ){
		ExecDualParam(cinfo, buf);
	}
}

void UpMouse(PPC_APPINFO *cinfo, int button, int oldmode, WPARAM wParam, LPARAM lParam)
{
	TCHAR click[4];
	POINT uppos;
	int newMpos;

	if ( cinfo->Tip.states & STIP_STATE_READYMASK ){
		if ( (cinfo->Tip.hTipWnd != NULL) && (cinfo->Tip.X_stip_mode == stip_mode_preview) ){ //preview は DestroyWindow で ReleaseCapture が生じるのでここで処理
			 EndEntryTip(cinfo);
		}
	}

	if ( cinfo->EntriesOffset.x | cinfo->EntriesOffset.y ){
		if ( button > MOUSEBUTTON_CANCEL ){
			if ( XC_page ){
				MoveWinOff(cinfo, -cinfo->EntriesOffset.y / cinfo->cel.Size.cy);
			}else{
				MoveWinOff(cinfo,
					-(cinfo->EntriesOffset.x /
						cinfo->cel.Size.cx) * cinfo->cel.Area.cy);
			}
		}
		cinfo->EntriesOffset.x = cinfo->EntriesOffset.y = 0;
		cinfo->DrawTargetFlags = DRAWT_ALL;
		InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
	}

	if ( button <= MOUSEBUTTON_CANCEL ){
		if ( cinfo->MouseStat.gesture.count ){
			cinfo->MouseStat.gesture.count = 0;
			StopPopMsg(cinfo, PMF_PROGRESS);
		}
		return;
	}

	if ( oldmode == MOUSEMODE_DRAG ){
		if ( (cinfo->MousePush == MOUSE_GESTURE) && cinfo->MouseStat.gesture.count ){
			RunGesture(cinfo);
		}
		return;
	}

	if ( GetFocus() != cinfo->info.hWnd ){
		if ( cinfo->MouseUpFG ){
			SetForegroundWindow(cinfo->info.hWnd);
		}else{
// ↓入れた理由を忘れてしまった。あるとdockにフォーカスがあるときからの復帰ができない
//			if ( cinfo->hTreeWnd == NULL ) return;
			SetFocus(cinfo->info.hWnd);
		}
	}
	cinfo->MouseUpFG = 0;

	click[0] = PPxMouseButtonChar[button];
	click[1] = '\0';

	LPARAMtoPOINT(uppos, lParam);
	cinfo->PopupPos = uppos;
	cinfo->PopupPosType = PPT_SAVED;
	ClientToScreen(cinfo->info.hWnd, &cinfo->PopupPos);

	if ( cinfo->PushArea != GetItemTypeFromPoint(cinfo, &uppos, &newMpos) ){
		return;
	}

	if ( (oldmode == MOUSEMODE_PUSH) &&
		 (cinfo->MouseStat.PushTick > MOUSE_LONG_PUSH_TIME) ){ // 長押しチェック
		click[1] = 'H';
		click[2] = '\0';
		if ( IsTrue(PPcMouseCommand(cinfo, click, (const TCHAR *)(LONG_PTR)cinfo->PushArea)) ){
			return;
		}
		click[1] = '\0';
	}

	if ( button == MOUSEBUTTON_L ){
		SetFocus(cinfo->info.hWnd);

		if ( cinfo->PushArea == PPCR_MENU ){
			if ( ((cinfo->DownMPos == newMpos) || TouchMode ) &&
				 (newMpos >= 0) &&
				 (newMpos < cinfo->HiddenMenu.item) ){
				const TCHAR *cmdp;

				cmdp = HiddenMenu_cmd(cinfo->HiddenMenu.data[newMpos]);
				ExecDualParam(cinfo, cmdp);
				return;
			}
		}
		// 単独左クリック解除時にマークを一つにする
		if ( (cinfo->PushArea == PPCR_CELLTEXT) ||
			 (cinfo->PushArea == PPCR_INFOICON)){
			if ( XC_msel[0] && !(wParam & (MK_SHIFT | MK_CONTROL)) ){
				ExplorerTypeMark_solo(cinfo, wParam, cinfo->e.cellN);
			}
		}
		// ダブルクリック / タッチ時タップ解除
		if ( PPcMouseCommand(cinfo, click, (const TCHAR *)(LONG_PTR)cinfo->PushArea) == FALSE ){
			if ( (TouchMode & TOUCH_DBLCLICKTOSINGLE) &&
				 IsTouchMessage() &&
				 (cinfo->PushArea == PPCR_CELLTEXT) &&
				 (cinfo->e.cellN == cinfo->cellNbeforePush) ){ // タッチをダブルクリックに変換
				PPcMouseCommand(cinfo, T("LD"), (const TCHAR *)(LONG_PTR)cinfo->PushArea);
			}else if ( (cinfo->PushArea == PPCR_CELLTAIL) && (GetShiftKey() == 0) ){
				if ( (cinfo->e.cellPoint >= 0) && (cinfo->e.cellPoint < cinfo->e.cellIMax) ){
					int mode = X_stip[TIP_TAIL_MODE];
					if ( mode == 0 ) mode = stip_mode_preview;

					ShowEntryTip(cinfo, STIP_CMD_MOUSE, mode, cinfo->e.cellPoint);
				}
			}
		}
		return;
	}

	if ( (button == MOUSEBUTTON_R) &&
		 ((cinfo->PushArea == PPCR_CELLTEXT) ||
		  (cinfo->PushArea == PPCR_INFOICON)) ){
		MoveCellCsr(cinfo, newMpos - cinfo->e.cellN, NULL);

		// マーク無しで、単独右クリック解除時にマークを一つにする
		if ( XC_msel[0] && !(wParam & (MK_SHIFT | MK_CONTROL)) ){
			if ( !IsCEL_Marked(newMpos) ){
				ExplorerTypeMark_solo(cinfo, wParam, newMpos);
			}
		}
	}

	if ( (button == MOUSEBUTTON_M) || (button == MOUSEBUTTON_W) ){
		StopPopMsg(cinfo, PMF_WAITKEY | PMF_FLASH);
													// View/ディレクトリ容量
		if ( (cinfo->PushArea == PPCR_CELLTEXT) ||
			 (cinfo->PushArea == PPCR_INFOICON)){
			MoveCellCsr(cinfo, newMpos - cinfo->e.cellN, NULL);
			if ( ((1 << button) & XC_cdc) && DispMarkSize(cinfo) ) return;
		}
	}
	PPcMouseCommand(cinfo, click, (const TCHAR *)(LONG_PTR)cinfo->PushArea);
}

void WMMouseMove(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	POINT pos;
	int posType;
	ENTRYINDEX itemNo, pn;

	LPARAMtoPOINT(pos, lParam);

	if ( (cinfo->oldmousemvpos.x == pos.x) &&
		 (cinfo->oldmousemvpos.y == pos.y) ){
		if ( cinfo->MouseStat.mode != MOUSEMODE_DRAG ) return;
	}else{
		cinfo->oldmousemvpos = pos;

		if ( X_askp && (cinfo->MousePush != MOUSE_DnDCHECK) &&
			 ((cinfo->X_inag & INAG_UNFOCUS) || (GetFocus() == NULL)) ){
			return;
		}

		posType = GetItemTypeFromPoint(cinfo, &pos, &itemNo);

		if ( X_poshl != 0 ){ // マウスカーソル上のエントリ背景色変更
//			setflag(cinfo->Tip.mode, STIP_SHOWTAILAREA);
			if ( (posType >= PPCR_CELLMARK) ){
				pn = itemNo;
			}else{
				pn = -1;
			}
			if ( cinfo->e.cellPoint != pn ){
				ENTRYINDEX oldn;

				oldn = cinfo->e.cellPoint;
				cinfo->e.cellPoint = pn;
				cinfo->e.cellPointType = posType;

				if ( pn >= 0 ) RefleshCell(cinfo, pn);
				if ( oldn >= 0 ) RefleshCell(cinfo, oldn);
				if ( X_stip[TIP_HOVER_TIME] ){
					if ( cinfo->Tip.states & STIP_CMD_HOVER ){
						HideEntryTip(cinfo);
					}
					if ( pn >= 0 ){
						SetTimer(cinfo->info.hWnd, TIMERID_HOVERTIP, X_stip[TIP_HOVER_TIME], HoverTipTimerProc);
					}
				}
				pn = -2;
			}
		}
								// 隠しメニュー処理(非ドラッグ時)
		if ( cinfo->MouseStat.mode != MOUSEMODE_DRAG ){
			if ( posType != PPCR_MENU ) itemNo = -1;
			if ( itemNo != cinfo->Mpos ){
				cinfo->Mpos = itemNo;
				InvalidateRect(cinfo->info.hWnd, &cinfo->BoxInfo, FALSE);	// 更新指定
			}
			return;
		}
	}
	//---------------------------------- 左ドラッグ(MOUSEMODE_DRAG)
	switch ( cinfo->MousePush ){
		case MOUSE_CANCEL:					// 何もしない
			break;
		case MOUSE_SWIPECHECK:
			cinfo->MousePush = MOUSE_SWIPE;
		case MOUSE_SWIPE:
			if ( XC_page ){
				cinfo->EntriesOffset.y += cinfo->MouseStat.MovedOffset.cy;
			}else{
				cinfo->EntriesOffset.x += cinfo->MouseStat.MovedOffset.cx;
			}
			InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
			break;

		case MOUSE_DnDCHECK:				// D&D開始検知
			cinfo->MouseStat.mode = MOUSEMODE_NONE;
			cinfo->MouseStat.PushButton = MOUSEBUTTON_CANCEL;

			cinfo->MousePush = MOUSE_NONE;
			cinfo->MouseUpFG = 0;

			if ( !((TouchMode & TOUCH_DISABLEDRAG) && IsTouchMessage()) ){
				PPcDragFile(cinfo);
			}
			break;

		case MOUSE_WNDMOVE: {				// 窓移動中
			RECT box, *b;
			HWND hWnd;

			if ( !(GetAsyncKeyState(VK_LBUTTON) & KEYSTATE_PUSH) &&
				 !(GetAsyncKeyState(MBtoVK(XC_drag)) & KEYSTATE_PUSH) ){
				PPxCancelMouseButton(&cinfo->MouseStat);
				cinfo->MousePush = MOUSE_CANCEL;
				break;
			}

			if ( cinfo->combo ){
				b = &box;
				hWnd = cinfo->hComboWnd;
				GetWindowRect(hWnd, b);
			}else{
				b = &cinfo->wnd.NCRect;
				hWnd = cinfo->info.hWnd;
			}
			SetWindowPos(hWnd, NULL,
					b->left + cinfo->MouseStat.MovedOffset.cx,
					b->top + cinfo->MouseStat.MovedOffset.cy,
					0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
			break;
		}
		case MOUSE_MARKCHECK:{				// Drag型範囲指定検知中
			RECT area;

			if ( cinfo->Mpos >= 0 ){ // 隠しメニューが有効の時
				cinfo->Mpos = -1;
				InvalidateRect(cinfo->info.hWnd, &cinfo->BoxInfo, FALSE);
			}

			if ( (cinfo->PushArea == PPCR_CELLMARK) && (wParam & MK_LBUTTON) ){
				CellMark(cinfo, cinfo->e.cellN, MARK_REVERSE);
			}
			// Drag型範囲指定 を有効に
			cinfo->MousePush = MOUSE_MARK;
			cinfo->MouseDragPoint = pos;
			CalcDragTarget(cinfo, &pos, &area);
			MarkDragArea(cinfo, &area, MARK_REVERSE);
			DrawDragFrame(cinfo->info.hWnd, &area);
			if ( cinfo->PushArea != PPCR_CELLMARK ){
				ExplorerTypeMark(cinfo, wParam);
			}
			InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
			break;
		}
		case MOUSE_MARK: {					// Drag型範囲指定中
			RECT area;
			int oldmarks;

			SetDDScroll(cinfo, &pos);
			if ( (pos.x == cinfo->MouseStat.PushClientPoint.x) &&
				 (pos.y == cinfo->MouseStat.PushClientPoint.y) ){
				break;
			}
			oldmarks = cinfo->e.markC;
			CalcDragTarget(cinfo, &cinfo->MouseDragPoint, &area);
			DrawDragFrame(cinfo->info.hWnd, &area);
			MarkDragArea(cinfo, &area, MARK_REVERSE);

			cinfo->MouseDragPoint = pos;
			CalcDragTarget(cinfo, &pos, &area);
			MarkDragArea(cinfo, &area, MARK_REVERSE);
			if ( ((X_poshl != 0) && (pn == -2)) || (oldmarks != cinfo->e.markC) ){ // C4701ok
				Repaint(cinfo);
				UpdateWindow(cinfo->info.hWnd);
			}
			DrawDragFrame(cinfo->info.hWnd, &area);
			break;
		}

		case MOUSE_GESTURE:					// ジェスチャー
			if ( PtInRect(&cinfo->MouseStat.DDRect, cinfo->MouseStat.MovedScreenPoint) == FALSE ){
				CheckGesture(cinfo);
			}
			break;

	}
}

void USEFASTCALL WmMouseDown(PPC_APPINFO *cinfo, WPARAM wParam)
{
	int area, itemno;

	cinfo->LastInputType = GetPointType();
	if ( cinfo->Tip.states & STIP_STATE_READYMASK ){
		if ( (cinfo->Tip.hTipWnd == NULL) || (cinfo->Tip.X_stip_mode != stip_mode_preview) ){ //preview は DestroyWindow で ReleaseCapture が生じるので無視
			 EndEntryTip(cinfo);
		}
	}

	if ( cinfo->MouseStat.PushButton <= MOUSEBUTTON_CANCEL ){
		if ( cinfo->MousePush != MOUSE_NONE ){
			cinfo->MousePush = MOUSE_CANCEL;
			cinfo->DrawTargetFlags = DRAWT_ALL;
			InvalidateRect(cinfo->info.hWnd, NULL, TRUE);
		}
		return;
	}

	if ( cinfo->hActiveWnd != NULL ){
		cinfo->hActiveWnd = NULL; // アクティブフォーカスを一覧に戻す
		SetFocus(cinfo->info.hWnd);
	}
	cinfo->cellNbeforePush = cinfo->e.cellN;
	cinfo->PushArea = area = GetItemTypeFromPoint(cinfo, &cinfo->MouseStat.PushClientPoint, &cinfo->DownMPos);
	itemno = cinfo->DownMPos;
	StopPopMsg(cinfo, PMF_WAITKEY | PMF_FLASH);

//↓本来は非アクティブ時に変なメッセージが来た場合の対策だが、
//  WM_MOUSEACTIVATE で return MA_NOACTIVATE; を行った場合にも機能して
//  しまうため、ためしに無効にした。
//	if ( GetFocus() != cinfo->info.hWnd ) break;

	if ( cinfo->MouseStat.PushButton == MOUSEBUTTON_L ){
		// Shift + Lclick : 直前のカーソル位置から範囲マーク ------------------
		// 本当は、カーソルも移動した方がいい
		if ( (wParam & MK_SHIFT) && (area >= PPCR_CELLMARK) ){
			ENTRYINDEX cellold;
			ENTRYINDEX i;
			int mode;

			cellold = cinfo->e.cellN;
			if ( XC_msel[1] && (cinfo->e.markC > 0) ){
				ENTRYDATAOFFSET olddata;

				olddata = cinfo->e.markLast;
				if ( CELt(itemno) == olddata ) olddata = cinfo->e.markTop;
				cellold = GetCellIndexFromCellData(cinfo, olddata);
			}
			cinfo->MarkMask = MARKMASK_DIRFILE;
			if ( XC_msel[0] == 2 ) ClearMark(cinfo);
			mode = !IsCEL_Marked(itemno);
			if ( itemno < cellold ){
				for ( i = itemno ; i <= cellold ; i++ ) CellMark(cinfo, i, mode);
			}else{
				for ( i = cellold ; i <= itemno ; i++ ) CellMark(cinfo, i, mode);
			}
			Repaint(cinfo);
			return;
		}
		// Ctrl + Lclick : マーク & カーソル移動 ------------------------------
		if ( (area == PPCR_CELLMARK) || (area == PPCR_CELLTAIL) ||
			 ((wParam & MK_CONTROL) &&
				((area >= PPCR_CELLTEXT) || (area == PPCR_INFOICON))) ){
			cinfo->MousePush = MOUSE_MARKCHECK;
			cinfo->MouseDragWMin = cinfo->cellWMin;

			MoveCellCsr(cinfo, itemno - cinfo->e.cellN, NULL);
			cinfo->MarkMask = MARKMASK_DIRFILE;
			if ( area != PPCR_CELLTAIL ){
				CellMark(cinfo, cinfo->e.cellN, MARK_REVERSE);
			}
			RefleshCell(cinfo, cinfo->e.cellN);
			RefleshInfoBox(cinfo, DE_ATTR_ENTRY | DE_ATTR_MARK);
			return;
		}
	}
	if ( (cinfo->MouseStat.PushButton == XC_drag) ||
		 (cinfo->MouseStat.PushButton == MOUSEBUTTON_W) ){
		if ( (cinfo->MouseStat.PushButton == MOUSEBUTTON_W) &&
			 ((cinfo->MousePush <= MOUSE_CANCEL) || (cinfo->MousePush == MOUSE_MARK) || ((cinfo->MousePush == MOUSE_GESTURE) && (cinfo->MouseStat.gesture.count != 0))) ){
			PPxCancelMouseButton(&cinfo->MouseStat);
			cinfo->MousePush = MOUSE_CANCEL;
			cinfo->DrawTargetFlags = DRAWT_ALL;
			InvalidateRect(cinfo->info.hWnd, NULL, TRUE);
			return;
		}
		if ( (cinfo->combo ? ComboWinPos.show : cinfo->WinPos.show) == SW_SHOWNORMAL ){
			cinfo->MousePush = MOUSE_WNDMOVE;
		}
		return;
	}
										// Cell point -------------------------
	if ( (cinfo->MouseStat.PushButton == MOUSEBUTTON_L) ||
		 (cinfo->MouseStat.PushButton == MOUSEBUTTON_R) ){
		if ( (area >= PPCR_CELLTEXT) || (area == PPCR_INFOICON) ){
			if ( XC_Gest && (wParam & MK_RBUTTON) &&
					!((XC_Gest == 2) && (itemno == cinfo->e.cellN)) ){
				cinfo->MousePush = MOUSE_GESTURE;
				cinfo->MouseStat.gesture.count = 0;
			}else{
#if XSW
				cinfo->MousePush = MOUSE_SWIPECHECK;
				cinfo->MouseDragWMin = cinfo->cellWMin;
				return;
#else
				cinfo->MousePush = MOUSE_DnDCHECK;
				MoveCellCsr(cinfo, itemno - cinfo->e.cellN, NULL);
#endif
			}
			return;
		}
		if ( XC_Gest && (cinfo->MouseStat.PushButton == MOUSEBUTTON_R) ){
			cinfo->MousePush = MOUSE_GESTURE;
			cinfo->MouseStat.gesture.count = 0;
			return;
		}
		if ( (area == PPCR_CELLBLANK) || (area == PPCR_CELLMARK) ||
			 ((cinfo->hHeaderWnd == NULL) &&
			  ( (area == PPCR_INFOTEXT) || (area == PPCR_MENU) ||
				( (cinfo->BoxInfo.top == cinfo->BoxInfo.bottom) &&
				  ((area == PPCR_PATH) || (area == PPCR_STATUS) )) )) ){
#if XSW
			cinfo->MousePush = MOUSE_SWIPECHECK;
			cinfo->MouseDragWMin = cinfo->cellWMin;
#else
			cinfo->MousePush = MOUSE_MARKCHECK;
			cinfo->MouseDragWMin = cinfo->cellWMin;
#endif
			return;
		}
	}
	cinfo->MousePush = MOUSE_PUSH;
}

void WMDpiChanged(PPC_APPINFO *cinfo, WPARAM wParam, RECT *newpos)
{
	DWORD newDPI = HIWORD(wParam);

	if ( !(X_dss & DSS_ACTIVESCALE) ) return;
	if ( cinfo->FontDPI == newDPI ) return; // 変更無し(起動時等)

	if ( newDPI != 0 ) cinfo->FontDPI = newDPI;

	DeleteObject(cinfo->hBoxFont);
	InitFont(cinfo);
	ClearCellIconImage(cinfo);

	if ( newpos != NULL ){
		SetWindowPos(cinfo->info.hWnd, NULL, newpos->left, newpos->top,
				newpos->right - newpos->left, newpos->bottom - newpos->top,
				SWP_NOACTIVATE | SWP_NOZORDER);
	}

	CloseGuiControl(cinfo);
	InitGuiControl(cinfo);

	if ( cinfo->hTreeWnd != NULL ){
		SendMessage(cinfo->hTreeWnd, VTM_CHANGEDDISPDPI, wParam, 0);
	}

	WmWindowPosChanged(cinfo);
	Repaint(cinfo);
}


LRESULT WMGesture(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{

	if ( TouchMode == 0 ){
		if ( X_pmc[0] < 0 ){
			PPcEnterTabletMode(cinfo);
			SetPopMsg(cinfo, POPMSG_MSG, T("enter touch mode"));
		}
	}

	switch ( wParam ){
		case GID_TWOFINGERTAP:
			PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_apps, 0);
			break;

		case GID_PRESSANDTAP:
			PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_apps, 0);
			break;
	}
#if 0
	{
		GESTUREINFO ginfo;
			TCHAR a[200], *id;

		TCHAR *idstr[] = {T("???"), T("begin"), T("end"), T("zoom"), T("pan"), T("rotate(reserved)"), T("twotap"), T("pressand")};
// GID_SCROLL, GID_HOLD, GID_SELECT, GID_DOUBLESELECT, GID_DIRECTMANIPULATION
		ginfo.cbSize = sizeof(GESTUREINFO);
		if ( DGetGestureInfo((HGESTUREINFO)lParam, &ginfo) ){

			if ( ginfo.dwID <= 7 ){
				id = idstr[ginfo.dwID];
			}else{
				id = idstr[0];
			}

			wsprintf(a, T("WMGesture : Flag:%03x  ID:%04x(%s)  (%4d,%4d) %x"),
					ginfo.dwFlags, ginfo.dwID, id,
					ginfo.ptsLocation.x, ginfo.ptsLocation.x,
					ginfo.cbExtraArgs);
			// DCloseGestureInfoHandle((HGESTUREINFO)lParam); // DefWindowProc を通すので不要
		SetPopMsg(cinfo, POPMSG_MSG, a);
		}else{
			wsprintf(a, T("WMGesture FALSE : %04x"), wParam);
		if( wParam != 2 ) SetPopMsg(cinfo, POPMSG_MSG, a);
		}
	}
#endif
	return DefWindowProc(cinfo->info.hWnd, WM_GESTURE, wParam, lParam);
}

/*
	1点タッチ…左クリック
	1点ホールド…右クリック

	1点スワイプ…スクロール

	2点ピンチ…拡大縮小
*/
#if XTOUCH
struct {
	POINT FirstPos;
	DWORD FirstTime;
	int touchs;
	int mode;
} Touch;

LRESULT WMTouch(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	UINT Touchs = LOWORD(wParam);
	PTOUCHINPUT TouchsData, TData;
	POINT pos;

	SetPopMsg(cinfo, POPMSG_MSG, T("WMTouch"));
	if ( Touchs == 0 ){
		if ( Touch.touchs != 0 ){
			int oldmode, button;

			Touch.touchs = 0;

			oldmode = cinfo->MouseStat.mode;
			button = PPxUpMouseButton(&cinfo->MouseStat, 0);
			UpMouse(cinfo, button, oldmode, 0, TMAKELPARAM(0, 0));
			cinfo->MousePush = MOUSE_CANCEL;
		}
	}else{
		Touch.touchs = Touchs;
		TouchsData = PPcHeapAlloc(sizeof(TOUCHINPUT) * Touchs);
		if ( TouchsData != NULL ){
			if ( IsTrue(DGetTouchInputInfo((HTOUCHINPUT)lParam, Touchs, TouchsData, sizeof(TOUCHINPUT))) ){
				TCHAR a[1000];
				UINT i = 0;
				for ( ; i < Touchs ; i++ ){
					TData = &TouchsData[i];

					pos.x = TData->x;
					pos.y = TData->y;
					ScreenToClient(cinfo->info.hWnd, &pos);

					wsprintf(a, T("Touch Time:%x ID:%x (%d,%d) ->%x"), TData->dwTime, TData->dwID, pos.x, pos.y, TData->dwFlags);
					SetPopMsg(cinfo, POPMSG_MSG, a);
					if ( TData->dwFlags & TOUCHEVENTF_PRIMARY ){
						if ( TData->dwFlags & TOUCHEVENTF_DOWN ){
							PPxDownMouseButton(&cinfo->MouseStat, cinfo->info.hWnd, MK_LBUTTON, TMAKELPARAM(pos.x, pos.y));
							WmMouseDown(cinfo, MK_LBUTTON);
						}else if ( TData->dwFlags & TOUCHEVENTF_UP ){
							int oldmode, i;

							oldmode = cinfo->MouseStat.mode;
							i = PPxUpMouseButton(&cinfo->MouseStat, 0);
							UpMouse(cinfo, i, oldmode, 0, TMAKELPARAM(pos.x, pos.y));
							cinfo->MousePush = MOUSE_CANCEL;
						}else if ( !(TData->dwFlags & TOUCHEVENTF_UP) ){
							PPxMoveMouse(&cinfo->MouseStat, cinfo->info.hWnd, TMAKELPARAM(pos.x, pos.y));
							WMMouseMove(cinfo, MK_LBUTTON, TMAKELPARAM(pos.x, pos.y));
						}
					}
				}
			}
		}
		PPcHeapFree(TouchsData);
		DCloseTouchInputHandle((HTOUCHINPUT)lParam);
	}
	return 0;
//	return DefWindowProc(cinfo->info.hWnd, WM_TOUCH, wParam, lParam);
}
#endif // XTOUCH

void RestartSubthread(PPC_APPINFO *cinfo, LPARAM lParam)
{
	DWORD tmp;
	ENTRYINDEX index;
	TCHAR buf[VFPS + 128];

	for ( index = 0 ; index < cinfo->e.cellIMax ; index++ ){
		if ( tstrcmp(CEL(index).f.cFileName, (TCHAR *)lParam) == 0 ){
			CEL(index).icon = ICONLIST_BROKEN;
			DIRECTXDEFINE(CEL(index).iconcache = 0);
			wsprintf(buf, T("Icon broken : %s"), (TCHAR *)lParam);
			SetPopMsg(cinfo, POPMSG_MSG, buf);
			break;
		}
	}

	cinfo->hSubThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)SubThread, cinfo, 0, &tmp);
}

void ResizeGuiParts(PPC_APPINFO *cinfo)
{
	if ( cinfo->hHeaderWnd != NULL ){
		SetWindowPos(cinfo->hHeaderWnd, NULL,
			cinfo->BoxEntries.left, cinfo->BoxEntries.top - cinfo->HeaderHeight,
			cinfo->BoxStatus.right - cinfo->BoxEntries.left, cinfo->HeaderHeight,
			SWP_NOACTIVATE | SWP_NOZORDER);
	}
	if ( cinfo->hScrollBarWnd != NULL ){
		if ( cinfo->ScrollBarHV == SB_HORZ ){ // 水平
			SetWindowPos(cinfo->hScrollBarWnd, NULL,
				cinfo->BoxEntries.left, cinfo->ScrollBarY,
				cinfo->BoxEntries.right - cinfo->BoxEntries.left, cinfo->ScrollBarSize,
				SWP_NOACTIVATE | SWP_NOZORDER);
		}else{ // 垂直
			SetWindowPos(cinfo->hScrollBarWnd, NULL,
				cinfo->BoxEntries.right, cinfo->BoxEntries.top,
				cinfo->ScrollBarSize, cinfo->ScrollBarY - cinfo->BoxEntries.top,
				SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
}

LRESULT WmPPxCommand(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	DEBUGLOGC("WmPPxCommand %4x", wParam);
	switch (LOWORD(wParam)){
		case KC_POPTENDFIX:
			SetPopMsg(cinfo, POPMSG_MSG, T("PPcFile: Fixed Thread terminate lock"));
			break;

		// Tree関連
		case KTN_close:
			PPC_CloseTree(cinfo);
			break;

		case KTN_escape:
			if ( cinfo->XC_tree.mode == PPCTREE_SELECT ){
				PPC_CloseTree(cinfo);
				break;
			}
			// KTN_focus へ
		case KTN_focus:
			SetFocus(cinfo->info.hWnd);
			break;

		case KCW_enteraddress:
			if ( VFSFixPath(NULL, (TCHAR *)lParam, cinfo->path, VFSFIX_VFPS) != NULL ){
				SetPPcDirPos(cinfo);
				tstrcpy(cinfo->path, (TCHAR *)lParam);
				read_entry(cinfo, RENTRY_READ);
			}
			break;

		case KTN_select:
		case KTN_selected:
			if ( tstricmp(cinfo->path, (TCHAR *)lParam) ){
				SetPPcDirPos(cinfo);
				tstrcpy(cinfo->path, (TCHAR *)lParam);
				read_entry(cinfo, RENTRY_READ);
			}
			if ( LOWORD(wParam) == KTN_selected ){
				SetFocus(cinfo->info.hWnd);
				if ( cinfo->XC_tree.mode == PPCTREE_SELECT ){
					PPC_CloseTree(cinfo);
					break;
				}
			}
			break;

		case KTN_size:
			if ( (int)lParam != cinfo->TreeX ){
				cinfo->TreeX = cinfo->XC_tree.width = (int)lParam;
				InitCli(cinfo);
				ResizeGuiParts(cinfo);
				InvalidateRect(cinfo->hTreeWnd, NULL, FALSE);
				InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
				ChangeSizeDxDraw(cinfo->DxDraw, cinfo->BackColor);
			}
			break;

		//通信
		case KC_DoJW:
			cinfo->NowJoint = FALSE;
			JointWindowMain(cinfo, (HWND)lParam);
			cinfo->NowJoint = TRUE;
			break;

		case KC_MOUSECMD: {
			int ppcr, mpos;
			TCHAR cmd[3];

			ppcr = LOWORD(lParam);
			mpos = HIWORD(lParam);
			cmd[0] = (BYTE)(wParam >> 16);
			cmd[1] = (BYTE)(wParam >> 24);
			cmd[2] = '\0';

			cinfo->PopupPosType = PPT_MOUSE;
			if ( ppcr == PPCR_MENU ){
				if ( (wParam == (KC_MOUSECMD + ('L' << 16))) ){
					if ( cinfo->DownMPos == mpos ){
						TCHAR *p;

						p = HiddenMenu_cmd(cinfo->HiddenMenu.data[mpos]);
						ExecDualParam(cinfo, p);
					}
					break;
				}else{
					ppcr = PPCR_INFOTEXT;
				}
			}
			PPcMouseCommand(cinfo, cmd, (const TCHAR *)(LONG_PTR)ppcr);
			break;
		}

		case K_ENDATTR:
			if ( cinfo->RealPath[0] == (TCHAR)lParam ){
				RefreshAfterList(cinfo, ALST_ATTRIBUTES);
			}
			break;
		case K_ENDCOPY:
			if ( cinfo->RealPath[0] == (TCHAR)lParam ){
				RefreshAfterList(cinfo, ALST_COPYMOVE);
			}
			break;
		case K_ENDDEL:
			if ( cinfo->RealPath[0] == (TCHAR)lParam ){
				RefreshAfterList(cinfo, ALST_DELETE);
			}
			break;

		case K_EXTRACT:
			return ReceiveExtractCall(&cinfo->info, wParam, lParam);

		case KC_RELOAD:
			// 現在パスと異なる場合は無視
			if ( cinfo->LoadCounter != (DWORD)lParam ) break;
			if ( HIWORD(wParam) ){	// 読み込み経過の表示
				cinfo->AloadCount = HIWORD(wParam);
				RefleshStatusLine(cinfo);
			}else{					// 非同期読み込み完了のため、更新
				cinfo->SlowMode = FALSE; // 低速モードを解除
				// 前回はエラー／キャッシュ無しなので、前回の動作を維持
				if ( (cinfo->e.cellDataMax == 1) && (CELdata(0).type == ECT_SYSMSG) ){
					read_entry(cinfo,
							cinfo->AcceptReload & ~RENTRY_FLAGS_ARELOAD);
				}else{	// キャッシュ等があるので更新のみ
					read_entry(cinfo, RENTRY_SAVEOFF | RENTRY_UPDATE);
				}
			}
			break;

		case KC_DRIVERELOAD:
			read_entry(cinfo, RENTRY_CACHEREFRESH | RENTRY_DRIVEFIX);
			break;

		case K_FREEDRIVEUSE:
			if ( !lParam || (cinfo->RealPath[0] == lParam) ){
				setflag(cinfo->SubTCmdFlags, SUBT_STOPDIRCHECK); // 監視停止
				SetEvent(cinfo->SubT_cmd);
			}
			break;

		case K_SETPOPMSG:
		case K_SETPOPLINENOLOG:
			if ( (TCHAR *)lParam == NULL ){
				StopPopMsg(cinfo, PMF_ALL);
			}else{
				SetPopMsg(cinfo, ((LOWORD(wParam) == K_SETPOPMSG) ?
						POPMSG_MSG : POPMSG_NOLOGMSG), (const TCHAR *)lParam);
			}
			return 1;

		case K_WINDDOWLOG:
			if ( Combo.hWnd != NULL ){
				SendMessage(Combo.hWnd, WM_PPXCOMMAND, wParam, lParam);
			}else{
				if ( lParam != 0 ) SetReportText((const TCHAR *)lParam);
				if ( HIWORD(wParam) && (hCommonLog != NULL) &&
						!(X_combos[0] & CMBS_DELAYLOGSHOW) ){
					if ( HIWORD(wParam) == 2 ){ // 強制表示を行う
						DelayLogShowProc(hCommonLog,
								WM_TIMER, TIMERID_DELAYLOGSHOW, 0);
						break;
					}
					if ( GetWindowLongPtr(hCommonLog, GWL_STYLE) & WS_VISIBLE ){
						// ログの遅延表示を開始／画面描画を止める
						SendMessage(hCommonLog, WM_SETREDRAW, FALSE, 0);
						SetTimer(hCommonLog, TIMERID_DELAYLOGSHOW,
								TIMER_DELAYLOGSHOW, DelayLogShowProc);
					}
				}
			}
			return 1;

		case K_POPOPS:
			cinfo->PopupPosType = HIWORD(wParam);
			LPARAMtoPOINT(cinfo->PopupPos, lParam);
			break;

		case KC_UNFOCUS:
			if ( (cinfo->X_inag & INAG_USEGRAY) && (GetFocus() != cinfo->info.hWnd) ){
				setflag(cinfo->X_inag, INAG_GRAY | INAG_UNFOCUS);
				DeleteObject(cinfo->C_BackBrush);
				cinfo->C_BackBrush = CreateSolidBrush(GetGrayColorB(cinfo->BackColor));
				ChangeSizeDxDraw(cinfo->DxDraw, GetGrayColorB(cinfo->BackColor));
				cinfo->DrawTargetFlags = DRAWT_ALL;
				InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
				break;
			}else{
				setflag(cinfo->X_inag, INAG_UNFOCUS);
				break;
			}

		case KC_GETSITEHWND: {
			int site;

			if ( (int)lParam <= 0 ){ // KC_GETSITEHWND_BASEWND / KC_GETSITEHWND_CURRENT
				return (LRESULT)( (cinfo->combo != 0) ? cinfo->hComboWnd : cinfo->info.hWnd);
			}
			if ( (int)lParam == KC_GETSITEHWND_PAIR ){
				return (LRESULT)GetPairWnd(cinfo);
			}
			if ( (int)lParam >= KC_GETSITEHWND_LEFTENUM ){
				if ( cinfo->combo == 0 ) return (LRESULT)NULL;
				return SendMessage(Combo.hWnd, WM_PPXCOMMAND, KC_GETSITEHWND, lParam);
			}

			// KC_GETSITEHWND_LEFT / KC_GETSITEHWND_RIGHT
			site = PPcGetSite(cinfo);
			if ( site == PPCSITE_SINGLE ) site = PPCSITE_LEFT;
			if ( (int)lParam == site ){ // 自窓側
				return (LRESULT)cinfo->info.hWnd;
			}else{ // 反対窓側
				return (LRESULT)GetPairWnd(cinfo);
			}
		}

		case K_SETACTIVEFOCUS:
			cinfo->hActiveWnd = (HWND)lParam;
			break;

		case KC_ENDCOMPARE:
			StopPopMsg(cinfo, PMF_PROGRESS);
			SetPopMsg(cinfo, POPMSG_MSG, MES_CCMP);
			ActionInfo(cinfo->info.hWnd, &cinfo->info, AJI_COMPLETE, T("compare")); // 通知
			break;

		case K_THREADRESTART:
			RestartSubthread(cinfo, lParam);
			break;

		case KC_PRECLOSE:
			if ( IsTrue(cinfo->mws.NowClose) || IsTrue(cinfo->mws.DestroryRequest) ){
				break; // 既に終了処理中
			}
			setflag(cinfo->swin, SWIN_BUSY);
			IOX_win(cinfo, TRUE);
			PreClosePPc(cinfo);
			RequestDestroyFlag = 1;
			break;

		case KC_COMMENTEVENT:
			ExtCommentExecute(cinfo, lParam);
			break;

		case KCW_ready:
			if ( cinfo->combo < 0 ) cinfo->combo = 1; // Size 変更通知を有効にする
			break;

		case K_CHENGEDDISPDPI:
			WMDpiChanged(cinfo, lParam, NULL);
			break;
/*
		case K_E_TABLET:
			if ( X_pmc[0] < 0 ){
				TouchMode = ~X_pmc[1];
			}
			break;

		case K_E_PC:
			if ( X_pmc[0] < 0 ){
				TouchMode = 0;
			}
			break;
*/
		case K_Lcust:
			PPcReloadCustomize(cinfo, lParam);
			break;

		default:
			PPcCommand(cinfo, LOWORD(wParam));
	}
	DEBUGLOGC("WmPPxCommand %4x end", wParam);
	return NO_ERROR;
}

void WmClose(PPC_APPINFO *cinfo)
{
	if ( IsTrue(cinfo->mws.NowClose) || IsTrue(cinfo->mws.DestroryRequest) ){
		return; // 既に終了処理中
	}
								// 終了処理でフォーカスが変わるためFixを禁止
	if ( (cinfo->swin & SWIN_WQUIT) && CheckReady(cinfo) ){
		HWND hPairWnd;

		setflag(cinfo->swin, SWIN_BUSY);
		IOX_win(cinfo, TRUE);
		hPairWnd = GetJoinWnd(cinfo);
		if ( hPairWnd != NULL ) SendMessage(hPairWnd, WM_CLOSE, 0, 0);
	}else{
		setflag(cinfo->swin, SWIN_BUSY);
		IOX_win(cinfo, TRUE);
	}

	if ( cinfo->Ref <= 1 ){
		if ( (cinfo->combo != 0) && (Combo.BaseCount == 1) ){
			if ( Combo.hWnd != BADHWND ) PostMessage(Combo.hWnd, WM_CLOSE, 0, 0);
		}else{
			DestroyWindow(cinfo->info.hWnd);
		}
	}else{
		PreClosePPc(cinfo);
		ShowWindow(cinfo->info.hWnd, SW_HIDE);
		if ( (cinfo->combo != 0) && (Combo.hWnd != BADHWND) ){
			SendMessage(Combo.hWnd, WM_PARENTNOTIFY, WM_DESTROY, (LPARAM)cinfo->info.hWnd);
		}
		RequestDestroyFlag = 1;
	}
}

void WmWindowPosChanged(PPC_APPINFO *cinfo)
{
	WINDOWPLACEMENT wp;
	int oldx, oldy;
	HWND hWnd;
	RECT box;

	hWnd = cinfo->info.hWnd;
	GetWindowRect(hWnd, &cinfo->wnd.NCRect); //※wpの座標はタスクバーの考慮無
	cinfo->wnd.NCArea.cx = cinfo->wnd.NCRect.right  - cinfo->wnd.NCRect.left;
	cinfo->wnd.NCArea.cy = cinfo->wnd.NCRect.bottom - cinfo->wnd.NCRect.top;

										// 窓の最大化・最小化状態を取得
	wp.length = sizeof(wp);
	GetWindowPlacement(hWnd, &wp);

	oldx = cinfo->wnd.Area.cx;
	oldy = cinfo->wnd.Area.cy;
	// wp.showCmd は SW_SHOWMAXIMIZED/SW_SHOWMINIMIZED/SW_SHOWNORMAL
	// のいずれかである
	// SW_HIDE は WS_VISIBLE が無くなった状態なので前回のを維持
	if ( (wp.showCmd != SW_SHOWMINIMIZED) && (wp.showCmd != SW_HIDE) ){
		cinfo->WinPos.show = (BYTE)wp.showCmd;
		GetClientRect(hWnd, &box);
		cinfo->wnd.Area.cx = box.right;
		cinfo->wnd.Area.cy = box.bottom;
	}
										// 終了時の窓を保存
	if ( wp.showCmd == SW_SHOWNORMAL ){
		cinfo->WinPos.pos = cinfo->wnd.NCRect;
	}

//	if ( (cinfo->wnd.Area.cx == oldx) && (cinfo->wnd.Area.cy == oldy) ) return;
//	XMessage(NULL, NULL, XM_DbgLOG, T("size %d %d<-%d, %d<-%d"), hWnd, cinfo->wnd.Area.cx , oldx, cinfo->wnd.Area.cy , oldy);
										// クライアント情報の再設定
	InitCli(cinfo);

	if ( (cinfo->celF.attr & DE_ATTR_WIDEW) || cinfo->bg.X_WallpaperType || X_fles ){
		InvalidateRect(hWnd, NULL, FALSE);
		if ( (oldx != cinfo->wnd.Area.cx) || (oldy != cinfo->wnd.Area.cy) ){
			FreeOffScreen(&cinfo->bg);
		}
	}
	MoveCellCsr(cinfo, 0, NULL);
	if ( cinfo->combo ){
		// cinfo->combo = -1 の時は通知しない
		// (起動時の大きさ調整中の内容まで送信しないようにする)
		if ( (cinfo->combo > 0) && (wp.showCmd != SW_HIDE) ){
			PostMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_size, (LPARAM)hWnd);
		}
	}else{					// 必要なら連結処理
		if ( (cinfo->swin & SWIN_JOIN) && cinfo->NowJoint ) JoinWindow(cinfo);
		if ( (wp.showCmd == SW_SHOWMINIMIZED) && (X_tray & X_tray_SinglePPc) ){
			// ※最小化のときと、次の隠したときと２回続けて呼ばれる
			PostMessage(hWnd, WM_PPXCOMMAND, K_HIDE, 0);
			return;
		}
	}
	if ( cinfo->hTreeWnd != NULL ){
		MoveWindow(cinfo->hTreeWnd,
				0, cinfo->BoxEntries.top - cinfo->HeaderHeight,
				cinfo->BoxEntries.left,
				cinfo->wnd.Area.cy - cinfo->BoxEntries.top
					- cinfo->docks.b.client.bottom + cinfo->HeaderHeight, TRUE);
	}
	if ( cinfo->docks.t.hWnd != NULL ){
		MoveWindow(cinfo->docks.t.hWnd,
				0, 0,
				cinfo->wnd.Area.cx + REBARFIXWIDTH,
				cinfo->docks.t.client.bottom, TRUE);
	}
	if ( cinfo->hToolBarWnd != NULL ){
		GetWindowRect(cinfo->hToolBarWnd, &box);
		cinfo->ToolbarHeight = box.bottom - box.top;
		SetWindowPos(cinfo->hToolBarWnd, NULL, 0, cinfo->BoxInfo.bottom,
				cinfo->wnd.Area.cx, cinfo->ToolbarHeight,
				SWP_NOACTIVATE | SWP_NOZORDER);
		// 横幅が変化するときは、ツールバーが下にずれることがあるため、
		// もう一度位置設定を行う
		if ( cinfo->wnd.Area.cx != (box.right - box.left) ){
			SetWindowPos(cinfo->hToolBarWnd, NULL, 0, cinfo->BoxInfo.bottom,
					cinfo->wnd.Area.cx, cinfo->ToolbarHeight,
					SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
	if ( cinfo->docks.b.hWnd != NULL ){
		MoveWindow(cinfo->docks.b.hWnd,
				0, cinfo->wnd.Area.cy - cinfo->docks.b.client.bottom,
				cinfo->wnd.Area.cx + REBARFIXWIDTH,
				cinfo->docks.b.client.bottom, TRUE);
	}
	ResizeGuiParts(cinfo);
	ChangeSizeDxDraw(cinfo->DxDraw, cinfo->BackColor);
}

BOOL WmChar(PPC_APPINFO *cinfo, WORD key, LPARAM lParam)
{
	cinfo->KeyChar = 0;

	if ( cinfo->multicharkey ){
		cinfo->multicharkey = 0;
		return TRUE;
	}

	if ( Ismulti(key) ){
		cinfo->multicharkey = 1;
		SetPopMsg(cinfo, POPMSG_MSG, MES_EKCD);
	}

	if ( key < 0x80 ){
		cinfo->KeyRepeats = (int)lParam & B30;
		cinfo->PopupPosType = PPT_FOCUS;
		cinfo->LastInputType = LIT_KEYBOARD;

		if ( cinfo->KeyHookEntry != NULL ){
			if ( CallKeyHook(cinfo, FixCharKeycode(key)) != PPXMRESULT_SKIP ){
				return TRUE;
			}
		}

		if ( PPcCommand(cinfo, FixCharKeycode(key)) != ERROR_INVALID_FUNCTION ){
			if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
			return TRUE;
		}
	}
	return FALSE;
}

void USEFASTCALL SetTextClipboardData(PPC_APPINFO *cinfo)
{
	if ( cinfo->CLIPDATAS[CLIPTYPES_TEXT] == NULL ) return;
	SetClipboardData(CF_TTEXT, cinfo->CLIPDATAS[CLIPTYPES_TEXT]);
}

void SetSHNClipboardData(PPC_APPINFO *cinfo)
{
	TCHAR classname[VFPS];

	if ( DGetGUIThreadInfo == INVALID_HANDLE_VALUE ){
		GETDLLPROC(GetModuleHandle(StrUser32DLL), GetGUIThreadInfo);
	}
	if ( DGetGUIThreadInfo != NULL ){
		xGUITHREADINFO guiinfo;

		guiinfo.cbSize = sizeof(guiinfo);
		if ( FALSE != DGetGUIThreadInfo(
				GetWindowThreadProcessId(GetForegroundWindow(), NULL),
				&guiinfo) ){
			HWND hTargetWnd;
			hTargetWnd = guiinfo.hwndFocus ? guiinfo.hwndFocus : guiinfo.hwndActive;
			if ( GetClassName(hTargetWnd, classname, VFPS) ){
				if ( !tstricmp(classname, T("edit")) || (tstrstr(classname, T(".EDIT.")) != NULL) ){
					return;
				}
			}
		}
	}
	SetClipboardData(CF_xSHELLIDLIST, cinfo->CLIPDATAS[CLIPTYPES_SHN]);
}

LRESULT USEFASTCALL PPcWmCommand(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam)
{
	DWORD cmdID;
	DEBUGLOGC("WM_COMMAND %4x", wParam);
	cinfo->KeyRepeats = 0;

	if ( lParam != 0 ){ // コントロールからの送信
		if ( (HWND)lParam == cinfo->hToolBarWnd ){
			PPcToolbarCommand(cinfo, LOWORD(wParam), 0);
			return 0;
		}
		if ( DocksWmCommand(&cinfo->docks, wParam, lParam) ){
			return 0;
		}
		if ( (HWND)lParam == hCommonLog ) return 0;
	}
	cmdID = LOWORD(wParam);
	if ( cmdID >= 0xf000 ){	// f000-ffff System Menu ==============
		cmdID = cmdID & 0xfff0;
		if ( cmdID == SC_MINIMIZE ){
			cinfo->PopupPosType = PPT_FOCUS;
			PPcCommand(cinfo, K_s | K_esc | K_raw);
			return 0;
		}
		if ( (cinfo->combo) && (
				(cmdID == SC_MAXIMIZE) || (cmdID == SC_RESTORE) ||
				(cmdID == SC_SIZE) || (cmdID == SC_MOVE) ) ){
			PostMessage(cinfo->hComboWnd, WM_SYSCOMMAND, wParam, lParam);
			return 0;
		}
		if ( (cmdID == SC_KEYMENU) || (cmdID == SC_MOUSEMENU) ){
			cinfo->DynamicMenu.Sysmenu = TRUE;
		}
		return DefWindowProc(cinfo->info.hWnd, WM_SYSCOMMAND, wParam, lParam);
	}
	{
		DYNAMICMENUSTRUCT *dms;

		dms = (cinfo->combo == 0) ? &cinfo->DynamicMenu : &ComboDMenu;
											// 0x4000～0x4fff Menu bar ========
		if ( (cmdID >= IDW_MENU) && (cmdID <= IDW_MENUMAX) ){
			CommandDynamicMenu(dms, &cinfo->info, wParam);
			if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
			return 0;
		}
											// Ex Menu ========
		if ( (cmdID > CRID_COMMENT) && (cmdID < CRID_NEWTAB) ){
			EXECEXMENUINFO execmenu;

			execmenu.hMenu = dms->hMenuBarMenu;
			execmenu.index = (int)wParam;
			PPcExecExMenu(cinfo, &execmenu);
			return 0;
		}
	}
	if ( wParam == 0xa220 ){	// XP Explorer のアイコン更新IDらしい
		if ( cinfo->EntryIcons.hImage != NULL ){
			ClearCellIconImage(cinfo);
			if ( cinfo->dset.infoicon == DSETI_OVL ){
				cinfo->dset.infoicon = DSETI_OVLNOC;
			}
			if ( cinfo->dset.cellicon == DSETI_OVL ){
				cinfo->dset.cellicon = DSETI_OVLNOC;
			}
		}
		return 0;
	}
	{
		TCHAR buf[64];

		wsprintf(buf, T("Unknown WM_xxxCOMMAND:%x"), (int)wParam);
		SetPopMsg(cinfo, POPMSG_MSG, buf);
	}
	return 0;
}

void USEFASTCALL PPcWmSetFocus(HWND hWnd, PPC_APPINFO *cinfo)
{
	DEBUGLOGC("WM_SETFOCUS start", 0);

	resetflag(cinfo->X_inag, INAG_GRAY | INAG_UNFOCUS);
	if ( cinfo->X_inag & INAG_USEGRAY ){
		DeleteObject(cinfo->C_BackBrush);
		cinfo->C_BackBrush = CreateSolidBrush(cinfo->BackColor);
		ChangeSizeDxDraw(cinfo->DxDraw, cinfo->BackColor);
	}
	if ( cinfo->combo ){
		SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_focus, (LPARAM)hWnd);
		SetCaptionCombo(cinfo, cinfo->hComboWnd);
	}else if ( IsWindowVisible(hWnd) == FALSE ){
		ShowWindow(hWnd, SW_SHOW);
	}

	PPcGetWindow(cinfo->RegNo, CGETW_SAVEFOCUS);
	PPxRegist(hWnd, NULL, PPXREGIST_SETPPcFOCUS);
	CreateCaret(hWnd, NULL, 0, 0);
	FixTwinWindow(cinfo);

	if ( cinfo->hActiveWnd != NULL ) SetFocus(cinfo->hActiveWnd);

	cinfo->Mpos = -1;
	RefleshCell(cinfo, cinfo->e.cellN); // カーソルの色・キャレットの位置を元に戻す
	cinfo->DrawTargetFlags = DRAWT_ALL;
	InvalidateRect(cinfo->info.hWnd, NULL, FALSE); // 残ったゴミを消去するため再描画
	// ●一体化時に、１枚目の表示に失敗するようなので入れてみた 1.69+3
	if ( cinfo->combo ) UpdateWindow(cinfo->info.hWnd);
	if ( !(cinfo->swin & SWIN_BUSY) &&
			 (cinfo->swin & SWIN_WACTIVE) && cinfo->NowJoint ){
		HWND hPairWnd;

		hPairWnd = GetJoinWnd(cinfo);
		if ( hPairWnd != NULL ){
			HDWP hDWP;

			hDWP = BeginDeferWindowPos(2);
			hDWP = DeferWindowPos(hDWP, hWnd, HWND_TOP,
					0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			hDWP = DeferWindowPos(hDWP, hPairWnd, hWnd,
					0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			if ( hDWP != NULL ) EndDeferWindowPos(hDWP);
		}
	}
	if ( ((XC_alst[ALST_ACTIVATE] >= ALSTV_UPD) || (cinfo->FDirWrite != FDW_NORMAL)) &&
		 (cinfo->SlowMode == FALSE) ){
		PostMessage(hWnd, WM_PPXCOMMAND, KC_CHECKRELOAD, 0);
	}
	if ( X_IME == 1 ) PostMessage(hWnd, WM_PPXCOMMAND, K_IMEOFF, 0);
	if ( IsTrue(cinfo->UseActiveEvent) ){
		PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_E_ACTIVE, 0);
	}
	DEBUGLOGC("WM_SETFOCUS end", 0);
}

int WINAPI PPcMain(PPCSTARTPARAM *psp)
{
	MSG msg;
	THREADSTRUCT threadstruct = {PPcRootMainThreadName, XTHREAD_ROOT, NULL, 0, 0};

	PPxRegisterThread(&threadstruct);
	if ( MainThreadID != GetCurrentThreadId() ){
		threadstruct.flag = 0;
		threadstruct.ThreadName = PPcMainThreadName;
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	}

	if ( CreatePPcWindow(psp, &MainWindows) == FALSE ) goto fin;
	PPxCommonExtCommand(K_TBB_INIT, 1);
										// メインループ -----------------------
	for ( ; ; ){
		if ( (int)GetMessage(&msg, NULL, 0, 0) <= 0 ) break;
/*
		// 極一部の環境で無効なSetTimerが設定されることの対策
		if ( (msg.message == WM_TIMER) && (msg.wParam > 0xfff) && msg.lParam ){
			if ( IsBadReadPtr((const void *)msg.lParam, 1) == FALSE ) continue;
		}
*/
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if ( RequestDestroyFlag ){	// ウィンドウの終了処理
			if ( FixPPcWindowList(&MainWindows, FALSE) == FALSE ){
				// 管理 PPc ウィンドウなし
				if ( Combo.hWnd == NULL ) break;

				if ( Combo.hWnd != BADHWND ){
					// メインスレッドで、Combo がまだ生きている→継続
					if ( threadstruct.flag & XTHREAD_ROOT ) continue;
				}else{
					// サブスレッドで、Combo が終わっている→終了指示
					if ( !(threadstruct.flag & XTHREAD_ROOT) ){
						RequestDestroyFlag = 1;
						PostThreadMessage(MainThreadID, WM_NULL, 0, 0);
					}
				}
				break;
			}
		}
	}
	FixPPcWindowList(&MainWindows, TRUE); // サブスレッドを強制終了してもよい
fin:
	if ( !(threadstruct.flag & XTHREAD_ROOT) ) CoUninitialize();
	PPxUnRegisterThread();
	return EXIT_SUCCESS;
}

/*===========================================================================*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPC_APPINFO *cinfo;

	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( cinfo == NULL ){
		if ( message == WM_CREATE ){
			SetWindowLongPtr(hWnd, GWLP_USERDATA,
					(LONG_PTR)(((CREATESTRUCT *)lParam)->lpCreateParams));
			return 0;
		}
		if ( message == WM_NCCREATE ){
			if ( (X_dss & DSS_COMCTRL) && (OSver.dwMajorVersion >= 10) ){
				PPxCommonCommand(hWnd, 0, K_ENABLE_NC_SCALE);
			}
		}
/* 今のところ不要
		if ( message == WM_TaskbarButtonCreated ){
			PPxCommonExtCommand(K_TBB_INIT, 0);
		}
*/
		// WM_CREATE の前にWM_GETMINMAXINFO,
		// WM_NCCREATE, WM_NCDESTROY, WM_NCCALCSIZE等が来る(^^;
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	switch (message){
		case WM_COPYDATA:
			return WmCopyData(cinfo, (COPYDATASTRUCT *)lParam, wParam);
							// マウスの移動(w:ﾎﾞﾀﾝ, lH:Y, lL:X)---------
		case WM_MOUSEMOVE:
			DxTransformPoint(cinfo->DxDraw, &lParam);
			PPxMoveMouse(&cinfo->MouseStat, hWnd, lParam);
			WMMouseMove(cinfo, wParam, lParam);
			break;
							// マウスの非クライアント領域クリック ------
		case WM_NCLBUTTONUP:
		case WM_NCRBUTTONUP:
		case WM_NCMBUTTONUP:
		case WM_NCXBUTTONUP:
			if ( GetFocus() != hWnd ) break;
		case WM_NCLBUTTONDBLCLK:
		case WM_NCRBUTTONDBLCLK:
		case WM_NCMBUTTONDBLCLK:
		case WM_NCXBUTTONDBLCLK:
			return PPcNCMouseCommand(cinfo, message, wParam, lParam);

		case WM_NCRBUTTONDOWN:
			if ( wParam == HTCAPTION ) cinfo->DynamicMenu.Sysmenu = TRUE;
		// WM_NCLBUTTONDOWN へ
		case WM_NCLBUTTONDOWN:
			cinfo->PopupPosType = PPT_MOUSE;
			return DefWindowProc(hWnd, message, wParam, lParam);

							// マウスのクライアント領域押下 ------
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
			DxTransformPoint(cinfo->DxDraw, &lParam);
			PPxDownMouseButton(&cinfo->MouseStat, hWnd, wParam, lParam);
			WmMouseDown(cinfo, wParam);
			break;
							// キャプチャ解除(ReleaseCapture実行時に来る) -----
		case WM_CAPTURECHANGED:
			WmCapturechanged(cinfo);
			break;
							// マウスのクリック解除 --------
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP: {
			int oldmode, button;

			DxTransformPoint(cinfo->DxDraw, &lParam);
			oldmode = cinfo->MouseStat.mode;
			button = PPxUpMouseButton(&cinfo->MouseStat, wParam);
			UpMouse(cinfo, button, oldmode, wParam, lParam);
			cinfo->MousePush = MOUSE_CANCEL;
			break;
		}
							// マウスのダブルクリック ------
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			DxTransformPoint(cinfo->DxDraw, &lParam);
			if ( !((TouchMode & TOUCH_DBLCLICKTOSINGLE) && IsTouchMessage() && IsCellDblClk(cinfo, lParam)) ){ // マウス
				PPxDoubleClickMouseButton(&cinfo->MouseStat, hWnd, wParam, lParam);
				DoubleClickMouse(cinfo, lParam);
			}else{ // タッチ
				PPxDownMouseButton(&cinfo->MouseStat, hWnd, wParam, lParam);
				WmMouseDown(cinfo, wParam);
				break;
			}
			break;
							// ホイールの移動(上下)
		case WM_MOUSEWHEEL:
//			DxTransformPoint(DxDraw, &lParam); 不要
			WheelMouse(cinfo, wParam, lParam);
			break;
							// ホイールの移動(左右, Vista以降)
		case WM_MOUSEHWHEEL:
			DxTransformPoint(cinfo->DxDraw, &lParam);
			H_WheelMouse(cinfo, wParam, lParam);
			break;
							// 各種コントロール (w:ID, lH:message, lL:CtrlHandle)
		case WM_SYSCOMMAND:
		case WM_COMMAND:
			return PPcWmCommand(cinfo, wParam, lParam);

		case WM_MENUCHAR:
			if ( cinfo->KeyChar ){
				cinfo->KeyChar = 0;
				return 0;
			}
			// break なし
		case WM_MENUSELECT:
		case WM_MENUDRAG:
		case WM_MENURBUTTONUP:
			return PPxMenuProc(hWnd, message, wParam, lParam);

							// フォーカスを取得 (w:old HWND)
		case WM_SETFOCUS:
			PPcWmSetFocus(hWnd, cinfo);
			break;

		case WM_KILLFOCUS:
			DEBUGLOGC("WM_KILLFOCUS start", 0);
			DestroyCaret();
			RefleshCell(cinfo, cinfo->e.cellN);
			if ( cinfo->BoxStatus.top ){
				UpdateWindow_Part(hWnd);
				RefleshInfoBox(cinfo, DE_ATTR_PATH);
			}
			DEBUGLOGC("WM_KILLFOCUS end", 0);
			break;

		case WM_MOUSEACTIVATE:
			return PPcMouseActive(hWnd, cinfo, wParam, lParam);

#ifdef USEDIRECTX
		case WM_ACTIVATE:
			if ( LOWORD(wParam) != WA_INACTIVE ){
				cinfo->DrawTargetFlags = DRAWT_ALL;
				InvalidateRect(hWnd, NULL, TRUE);
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
#endif
		case WM_GETMINMAXINFO:
			DEBUGLOGC("WM_GETMINMAXINFO start", 0);
									// 上下／左右連結中は全画面の半分に修正
			if ( (cinfo->swin & SWIN_JOIN) && !(cinfo->swin & SWIN_PILEJOIN) ){
				PPcMinMaxJoinFix(cinfo, (MINMAXINFO *)lParam);
			}
			DEBUGLOGC("WM_GETMINMAXINFO end", 0);
			return 0;
							// 窓状態の変更 -----------------------------------
		case WM_WINDOWPOSCHANGED:
			DEBUGLOGC("WM_WINDOWPOSCHANGED start", 0);
			WmWindowPosChanged(cinfo);
			DEBUGLOGC("WM_WINDOWPOSCHANGED end", 0);
			break;

		case WM_HSCROLL:	// 水平スクロール ---
		case WM_VSCROLL:	// 垂直スクロール ---
			PPcScrollBar(cinfo, message, LOWORD(wParam));
			break;
							// ALT+? が押された --
		case WM_SYSKEYDOWN:
			if ( !(lParam & B29) ){
				DEBUGLOGC("WM_SYSKEYDOWN - fix unfocus", 0) ;
				SetFocus(hWnd);
			}
							// key   が押された --
		case WM_KEYDOWN:{
			WORD key;

			HideEntryTip(cinfo);

			key = (WORD)(wParam | GetShiftKey() | K_v);
			if ( cinfo->IncSearchMode ){
				if ( IsTrue(IncSearchKeyDown(cinfo, key, wParam, lParam)) ){
					break;
				}
			}

			cinfo->KeyChar = 1;
			StopPopMsg(cinfo, PMF_WAITKEY | PMF_FLASH);
										// [ALT] は メニュー移行を禁止
			cinfo->PopupPosType = PPT_FOCUS;
			cinfo->LastInputType = LIT_KEYBOARD;
			cinfo->KeyRepeats = (int)lParam & B30;

			if ( cinfo->KeyHookEntry != NULL ){
				if ( CallKeyHook(cinfo, key) != PPXMRESULT_SKIP ) break;
			}

			if ( PPcCommand(cinfo, key) == ERROR_INVALID_FUNCTION ){
				if ( X_alt && (wParam == VK_MENU) ) break;
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
			if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
			break;
		}
							// key   が押された(w:key, lH:値, lL:repeat数)--
		case WM_SYSCHAR:
		case WM_CHAR:
			if ( cinfo->IncSearchMode ){
				if ( IsTrue(WmCharSearch(cinfo, (WORD)wParam)) ) break;
			}
			if ( IsTrue(WmChar(cinfo, (WORD)wParam, lParam)) ) break;
			return DefWindowProc(hWnd, message, wParam, lParam);

							// 窓の再描画(w:未使用, l:未使用) ------------------
		case WM_PAINT:
			DEBUGLOGC("WM_PAINT start", 0);
			Paint(cinfo);
			DEBUGLOGC("WM_PAINT end", 0);
			break;
							// 終了要求(w:未使用, l:未使用) --------------------
		case WM_CLOSE:	//defaultはDestroyWindowを呼び出す
			WmClose(cinfo);
			break;
							// 窓の破棄, 終了処理(w:未使用, l:未使用) -----------
		case WM_DESTROY:
			ClosePPc(cinfo);		// 必要な後処理を行う
			// combo 時は、combo thread の時は、comboで PostQuitMessage
			if ( cinfo->combo ){
				if ( ComboThreadID == GetCurrentThreadId() ) break;
			}
			PostWindowClosed();
			break;
							// 強制終了要求(w:未使用, l:未使用) ----------------
// w=0 logoff   w != 0 ユーザによるこのソフトの強制終了
//		case WM_QUERYENDSESSION:
//			return (long)TRUE;	// TRUE ならば終了, FALSE なら終了の拒否
							// 強制終了動作の報告 -----------------------------
		case WM_ENDSESSION:
			if ( wParam ){ // TRUE:セッションの終了(WM_DESTROY は通知されない)
				if ( (cinfo->mws.NowClose == 0) &&
					 (cinfo->mws.DestroryRequest == FALSE) ){
					PPxCommonExtCommand(K_REPORTSHUTDOWN, 0);
					cinfo->BreakFlag = TRUE;

					PreClosePPc(cinfo); // 設定の記憶を済ませる
					RequestDestroyFlag = 1;

								// 時間がないのでここで全体の後始末もおこなう
					if ( cinfo->combo == 0 ) PostWindowClosed();
				}
			}
			break;
#if 0
		case WM_SETTINGCHANGE: // ※一体化時は直接届かない
			#ifdef UNICODE
			if ( (wParam == 0) && (lParam != 0) && (((BYTE *)lParam)[0] != '\0') && (((BYTE *)lParam)[1] == '\0') )
			#else
			if ( (wParam == 0) && (lParam != 0) && (((BYTE *)lParam)[0] != '\0') && (((BYTE *)lParam)[1] != '\0') )
			#endif
			{
				if ( tstricmp((TCHAR *)lParam, T("ImmersiveColorSet")) == 0 ){
					PPcCommand(cinfo, K_Scust);
					PPcCommand(cinfo, K_Lcust);
					FreeOffScreen(&cinfo->bg);
				}
			}
			//PostMessage(hWnd, lParam, K_SETTINGCHANGE);
			return DefWindowProc(hWnd, message, wParam, lParam);
#endif
		case WM_SYSCOLORCHANGE:
			PPcCommand(cinfo, K_Scust);
			PPcCommand(cinfo, K_Lcust);
			FreeOffScreen(&cinfo->bg);
			break;
#ifdef USEDIRECTX
		case WM_DISPLAYCHANGE:
			ChangeSizeDxDraw(cinfo->DxDraw, cinfo->BackColor); // DXで必要。DWでは不要
			InvalidateRect(hWnd, NULL, TRUE);
			return DefWindowProc(hWnd, message, wParam, lParam);
#endif
		case WM_RENDERFORMAT:
			if ( wParam == CF_TTEXT ){
				SetTextClipboardData(cinfo);
			}else if ( wParam == CF_xSHELLIDLIST ){
				SetSHNClipboardData(cinfo);
			}
			break;

		case WM_RENDERALLFORMATS:
			SetTextClipboardData(cinfo);
			SetSHNClipboardData(cinfo);
			break;

		case WM_DESTROYCLIPBOARD: {
			int i;

			// CFSTR_PREFERREDDROPEFFECT はここで破棄してはいけないようだ
			for ( i = 0 ; i < CLIPTYPES_DROPEFFECT/*CLIPTYPES-1*/ ; i++ ){
				if ( cinfo->CLIPDATAS[i] != NULL ){
					GlobalFree(cinfo->CLIPDATAS[i]);
					cinfo->CLIPDATAS[i] = NULL;
				}
			}
			break;
		}

		case WM_NOTIFY:
			DEBUGLOGC("WM_NOTIFY", 0);
			return PPcNotify(cinfo, (NMHDR *)lParam);

		case WM_PPCEXEC:
			cinfo->PopupPosType = PPT_SAVED;
			LPARAMtoPOINT(cinfo->PopupPos, lParam);
			ExecDualParam(cinfo, (TCHAR *)wParam);
			break;

		case WM_PPCSETFOCUS:
			ForceSetForegroundWindow(hWnd);
			break;

		case WM_PPCFOLDERCHANGE: // SHChangeNotifyRegister 通知
			setflag(cinfo->SubTCmdFlags, SUBT_FOLDERCHANGE);
			SetEvent(cinfo->SubT_cmd);
			break;

		case WM_PPCSETCOMMENT: {
			ENTRYCELL *cell;
			SENDSETCOMMENT *ssc = (SENDSETCOMMENT *)wParam;

			if ( cinfo->LoadCounter != ssc->LoadCounter ) break;
			cell = &CELdata(ssc->dataindex);
			SetComment(cinfo, ssc->CommentID, cell, ssc->comment);
			break;
		}

		case WM_INITMENU:
			if ( cinfo->combo == 0 ){
				DynamicMenu_InitMenu(&cinfo->DynamicMenu, (HMENU)wParam, cinfo->X_win & XWIN_MENUBAR);
			}
			break;

		case WM_INITMENUPOPUP:
			if ( cinfo->combo == 0 ){
				if ( FALSE != DynamicMenu_InitPopupMenu(&cinfo->DynamicMenu, (HMENU)wParam, &cinfo->info) ){
					break;
				}
			}
			if ( cinfo->DelayMenus != NULL ){
				DelayLoadMenuStruct *menus = cinfo->DelayMenus;

				for ( ; menus->hMenu != NULL ; menus++ ){
					if ( menus->hMenu == (HMENU)wParam ){
						menus->initfunc(menus);
						return 0;
					}
				}
			}
			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_DWMCOMPOSITIONCHANGED:
			if ( DDwmIsCompositionEnabled != NULL ){
				DDwmIsCompositionEnabled(&UseOffScreen);
			}
			break;

		case WM_DEVICECHANGE:
			if ( ( ((UINT)wParam == DBT_DEVICEARRIVAL) ||
				   ((UINT)wParam == DBT_DEVICEREMOVECOMPLETE) ) &&
				 (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype
						== DBT_DEVTYP_VOLUME) ){
				if ( cinfo->hTreeWnd != NULL ){
					SendMessage(cinfo->hTreeWnd, WM_DEVICECHANGE, wParam, lParam);
				}
				DocksWmDevicechange(&cinfo->docks, wParam, lParam);

				// 追加されたドライブが、現在表示できないドライブだったら、
				// 再読み込み / 更新を行う
				if ( ((UINT)wParam == DBT_DEVICEARRIVAL) &&
					(cinfo->path[1] == ':') &&
					( ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask &
					  (1 << (TinyCharUpper(cinfo->path[0]) - 'A')) ) ){
					// 一度も読み込んでなさそうなら再読み込み、
					// 一度成功しているなら更新
					PostMessage(hWnd, WM_PPXCOMMAND,
						((cinfo->e.cellDataMax == 1) &&
						 (CELdata(0).type == ECT_SYSMSG)) ?
						KC_DRIVERELOAD : (K_raw | K_c | K_F5), 0);
				}
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		// 注意 WM_GETOBJECTのwParam/lParamは64bit環境では上位32bitにゴミが入る
		case WM_GETOBJECT:
			if ( ((LONG)lParam == OBJID_CLIENT) && X_iacc ){
				return WmGetObject(cinfo, (WPARAM)(DWORD)wParam);
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
#if XTOUCH
		case WM_TOUCH:
			return WMTouch(cinfo, wParam, lParam);
#endif

		case WM_GESTURE:
			return WMGesture(cinfo, wParam, lParam);

		case WM_DPICHANGED:
			WMDpiChanged(cinfo, wParam, (RECT *)lParam);
			break;
							// 知らないメッセージ -----------------------------
		default:
			if ( message == WM_PPXCOMMAND ){
				return WmPPxCommand(cinfo, wParam, lParam);
			}else if ( message == WM_TaskbarButtonCreated ){
				PPxCommonExtCommand(K_TBB_INIT, 0);
			}
#if 0
			if ( ((message >= 0x10d) && (message <= 0x10f)) ){
				TCHAR *ime1[] = {T("WM_IME_STARTCOMPOSITION"), T("WM_IME_ENDCOMPOSITION"), T("WM_IME_COMPOSITION"), T("WM_IME_KEYLAST")};
				XMessage(NULL, NULL, XM_DbgLOG, T("%s - %x - %x"), ime1[message - 0x10d], wParam, lParam);

			}else if ( ((message >= 0x281) && (message <= 0x291)) ){
				TCHAR *ime2[] = {T("WM_IME_SETCONTEXT"), T("WM_IME_NOTIFY"), T("WM_IME_CONTROL"), T("WM_IME_COMPOSITIONFULL"), T("WM_IME_SELECT"), T("WM_IME_CHAR"), T("WM_IME_???"), T("WM_IME_REQUEST"), T("WM_IME_0x289"), T("WM_IME_0x28a"), T("WM_IME_0x28b"), T("WM_IME_0x28c"), T("WM_IME_0x28d"), T("WM_IME_0x28e"), T("WM_IME_0x28f"), T("WM_IME_KEYDOWN"), T("WM_IME_KEYUP")};
				XMessage(NULL, NULL, XM_DbgLOG, T("%s - %x - %x"), ime2[message - 0x281], wParam, lParam);
			}
#endif
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
