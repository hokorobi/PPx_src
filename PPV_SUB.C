/*-----------------------------------------------------------------------------
	Paper Plane vUI				Sub
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

#define PPV_GetDisplayTypeMenu() ( hDispTypeMenu = CreatePopupMenu() )

const TCHAR *imgDmes[] = {MES_GDAR, MES_GDFR, MES_GDAI, MES_GDFI, T("100%")};
const TCHAR *disptypestr[] = {MES_VTHE, MES_VTTX, MES_VTDC, MES_VTIM, MES_VTRI, MES_VTAI};
TCHAR ReceivedParam[CMDLINESIZE];

void SetimgD(HWND hWnd, int newD);
int SetFindString(char *strbin);
VT_TABLE ti_dummy = { (BYTE *)"", 0, 0, 0, 0, 0};

#define PPCLC_XWIN 4 // ～ +32

const TCHAR lstr_title[] = MES_LYTI;
const TCHAR lstr_menu[] = MES_LYME;
const TCHAR lstr_status[] = MES_LYSL;
const TCHAR lstr_toolbar[] = MES_LYST;
const TCHAR lstr_scroll[] = MES_LYSC;
//const TCHAR lstr_docktop[] = MES_LYDT;
//const TCHAR lstr_dockbottom[] = MES_LYDB;

const TCHAR *dtype[] = {T("?D"), T("XD"), T("YD"), T("ZD")};
const TCHAR *utype[] = {T("?"), T("X"), T("Y"), T("Z")};

const TCHAR StrTcodeESC[] = T("ESC");
const TCHAR StrTcodeMIME[] = T("MIME");
const TCHAR StrTcodeTag[] = T("<tag>");

const TCHAR StrTcodeSelLines[] = MES_TXSL;
const TCHAR StrTcodeAllText[] = MES_TXAT;

const TCHAR StrSHCM[] = MES_SHCM;
const TCHAR StrAICP[] = MES_AICP;
const TCHAR StrIGAN[] = MES_IGAN;
const TCHAR StrIGIC[] = T("ICC profile"); // MES_IGIC;
const TCHAR StrIGPP[] = MES_IGPP;
const TCHAR StrIGNP[] = MES_IGNP;
const TCHAR StrEDEC[] = MES_EDEC;
const TCHAR StrEDET[] = MES_EDET;
const TCHAR StrClip[] = MES_EDCP;
const TCHAR StrOPSA[] = MES_OPSA;

struct RAWCOLORLISTSTRUCT {
	const TCHAR *name;
	int type;
} RawColorList[] = {
	{ T("1bit"),	1 },
	{ T("4bit"),	4 },
	{ T("8bit"),	8 },
	{ T("16bit B5G5R5"), 16 },
	{ T("16bit R5G5B5"), 16 + B7 },
	{ T("16bit B5G6R5"), 16 + B6},
	{ T("16bit R5G6B5"), 16 + B6 + B7 },
	{ T("24bit BGR"),	24 },
//	{ T("24bit RGB"),	24 + B7 },
	{ T("32bit BGR"),	32 },
	{ T("32bit RGB"),	32 + B7 },
	{ T("32bit B10G10R10"),	32 + B6 },
	{ T("32bit R10G10B10"),	32 + B6 + B7 },
	{ NULL, 0 }
};

/*
↓単位 形態:既定値 ページ スクロール カーソル移動せずにスクロール
---------------------------------------------------------
±1行(上下)   0       1        2        3
±1桁(左右)   4       5        6        7
±1ページ     8       9       10       11
±1/10ページ 12      13       14       15


*/

void MoveCursor(int *param)
{
	int deltaX = 0, deltaY = 0;

	switch(param[0]){ // 選択無し
		// Y
		case 1:
			MoveCsr(0, param[1] * VO_stepY, FALSE);
			// Fall through
		case 0: // Fall through
		case 3:
			deltaY = param[1] * VO_stepY;
			break;
		case 2:
			MoveCsr(0, param[1], FALSE);
			return;
		// X
		case 5:
			MoveCsr(param[1] * VO_stepX, 0, FALSE);
			// Fall through
		case 4: // Fall through
		case 7:
			deltaX = param[1] * VO_stepX;
			break;
		case 6:
			MoveCsr(param[1], 0, FALSE);
			return;
		// Y page
		case 9:
			MoveCsr(0, param[1] * (VO_sizeY-1), FALSE);
			// Fall through
		case 8: // Fall through
		case 11:
			deltaY = param[1] * (VO_sizeY-1);
			break;
		case 10:
			MoveCsr(0, param[1] * (VO_sizeY-1), FALSE);
			return;
		// Y deci page
		case 13:
			MoveCsr(0, param[1] * (VO_sizeY-1) / 10, FALSE);
			// Fall through
		case 12: // Fall through
		case 15:
			deltaY = param[1] * (VO_sizeY-1) / 10;
			break;
		case 14:
			MoveCsr(0, param[1] * (VO_sizeY-1) / 10, FALSE);
			return;

		// X page
		case 17:
			MoveCsr(param[1] * (VO_sizeX-1), 0, FALSE);
			// Fall through
		case 16: // Fall through
		case 19:
			deltaX = param[1] * (VO_sizeX-1);
			break;
		case 18:
			MoveCsr(param[1] * (VO_sizeX-1), 0, FALSE);
			return;
		// X deci page
		case 21:
			MoveCsr(param[1] * (VO_sizeX-1) / 10, 0, FALSE);
			// Fall through
		case 20: // Fall through
		case 23:
			deltaX = param[1] * (VO_sizeX-1) / 10;
			break;
		case 22:
			MoveCsr(param[1] * (VO_sizeX-1) / 10, 0, FALSE);
			return;

		default:
			XMessage(vinfo.info.hWnd, StrPPvTitle, XM_GrERRld, T("*cursor param error"));
			return;
	}
	MoveCsrkey(deltaX, deltaY, param[2]);
}

void PPvFindCommand(PPV_APPINFO *vinfo, const TCHAR *param)
{
	TCHAR *more;
	TCHAR buf[CMDLINESIZE];
	UTCHAR code;
	int mode = 1;

	while( '\0' != (code = GetOptionParameter(&param, buf, &more)) ){
		if ( code == '-' ){
/*
			if ( !tstrcmp( buf + 1, T("FORWARD")) ){
				mode = 1;
				continue;
			}
*/
			if ( !tstrcmp( buf + 1, T("BACK")) ){
				mode = -1;
				continue;
			}
			if ( !tstrcmp( buf + 1, T("DIALOG")) ){
				mode = 0;
				continue;
			}
		}else{
			tstrcpy(VOsel.VSstring, buf) /*, TSIZEOF(VOsel.VSstring));*/;
		}
	}
	if ( mode == 0 ){
		FindInputBox(vinfo->info.hWnd, T("Find forward"));
	}else{
		DoFind(vinfo->info.hWnd, mode);
	}
}

/*-----------------------------------------------------------------------------
	パラメータ入手関数（PPLIBxxx.DLLから呼ばれる）
-----------------------------------------------------------------------------*/
DWORD_PTR USECDECL PPxGetIInfo(PPV_APPINFO *vinfo, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 0;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
		case PPXCMDID_ENDENUM:		//列挙終了
			uptr->enums.enumID = -1;
			return 0;

		case '1': { // %1
			int mode = VFSPT_UNKNOWN;

			tstrcpy(uptr->enums.buffer, vo_.file.name);
			VFSGetDriveType(uptr->enums.buffer, &mode, NULL);
			if ( (mode == VFSPT_DRIVE) || (mode == VFSPT_UNC) ){
				TCHAR *p;

				p = VFSFindLastEntry(uptr->enums.buffer);
				if ( p != uptr->enums.buffer ) *p = '\0';
				if ( *uptr->enums.buffer != '\0' ) break;
			}
		}
		// Fall through
		case 'C': // %C
			if ( uptr->enums.enumID == -1 ){
				*uptr->enums.buffer = '\0';
				break;
			}
		// Fall through
		case 'R': { // %R
			const TCHAR *p;

			p = VFSFindLastEntry(vo_.file.name);
			tstrcpy(uptr->enums.buffer, (*p == '\\') ? p + 1 : p);
			break;
		}
		case PPXCMDID_ENUMATTR:
			if ( uptr->enums.enumID == -1 ){
				*uptr->enums.buffer = '\0';
				break;
			}
			*(DWORD *)uptr->enums.buffer = GetFileAttributes(vo_.file.name);
			break;

		case 'L': { // %L
			int line;

			line = ( VOsel.cursor != FALSE ) ? VOsel.now.y.line : VOi->offY;
			if ( vo_.DModeBit == DOCMODE_TEXT ){
				line = VOi->ti[line].line;
			}
			wsprintf(uptr->enums.buffer, T("%u"), line);
			break;
		}
		case 'l': { // %l
			int line;

			line = ( VOsel.cursor != FALSE ) ? VOsel.now.y.line : VOi->offY;
			wsprintf(uptr->enums.buffer, T("%u"), line + 1);
			break;
		}

		case 0x100 + 'l': { // %lV
			int line;

			line = ( VOsel.cursor != FALSE ) ? VOsel.now.x.offset : VOi->offX;
			wsprintf(uptr->enums.buffer, T("%u"), line + 1);
			break;
		}

		case PPXCMDID_PPXCOMMAD:
			return PPvCommand(vinfo, uptr->key);

		case PPXCMDID_GETREQHWND:
			if ( (uptr == NULL) || (uptr->str[0] == 'C') ){
				if ( hViewParentWnd != NULL ){
					return (DWORD_PTR)hViewParentWnd;
				}else if ( (hLastViewReqWnd != NULL) && IsWindow(hLastViewReqWnd) ){
					return (DWORD_PTR)hLastViewReqWnd;
				}
			}
			return (DWORD_PTR)NULL;

		case PPXCMDID_CSRX:
			uptr->num = VOi->offX;
			break;
		case PPXCMDID_CSRY:
			uptr->num = VOi->offY;
			break;
		case PPXCMDID_CSRDISPW:
			uptr->num = VO_sizeX;
			break;
		case PPXCMDID_CSRDISPH:
			uptr->num = VO_sizeY;
			break;

		case PPXCMDID_CSRRECT: {
			int old = PopupPosType;

			PopupPosType = PPT_FOCUS;
			GetPopupPosition((POINT *)uptr);
			PopupPosType = old;
			uptr->nums[2] = fontX;
			uptr->nums[3] = fontY;
			break;
		}

		case PPXCMDID_CSRLOCATE:
			if ( VOsel.cursor != FALSE ) {
				uptr->nums[0] = VOsel.now.x.offset;
				uptr->nums[1] = VOsel.now.y.line;
			}else{
				uptr->nums[0] = VOi->offX;
				uptr->nums[1] = VOi->offY;
			}
			uptr->nums[2] = VO_maxX;
			uptr->nums[3] = VO_maxY;
			break;

		case PPXCMDID_CSRSETLOCATE:
			if ( VOsel.cursor != FALSE ) {
				MoveCsrkey(uptr->nums[0] - VOsel.now.x.offset, uptr->nums[1] - VOsel.now.y.line, uptr->nums[2]);
			}else{
				MoveCsrkey(uptr->nums[0] - VOi->offX, uptr->nums[1] - VOi->offY, uptr->nums[2]);
			}
			break;

		case PPXCMDID_CSRSTATE:
			uptr->num = vo_.DModeType;
			break;

		case PPXCMDID_PATHJUMP: {
			TCHAR buf[VFPS];

			VFSFixPath(buf, uptr->str, NULL, VFSFIX_PATH | VFSFIX_NOFIXEDGE);
			OpenAndFollowViewObject(vinfo, buf, NULL, NULL, 0);
			break;
		}

		case PPXCMDID_SETPOPLINE:
			if ( *uptr->str != '\0' ){
				SetPopMsg(0, uptr->str);
			}else{
				StopPopMsg(PMF_ALL);
			}
			break;

		case PPXCMDID_POPUPPOS:
			GetPopupPosition((POINT *)uptr);
			break;

		case PPXCMDID_SETPOPUPPOS:
			PopupPosType = PPT_SAVED;
			PopupPos.x = uptr->nums[0];
			PopupPos.y = uptr->nums[1];
			break;

		case PPXCMDID_MOVECSR:
			MoveCursor((int *)uptr);
			if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
			break;

		case PPXCMDID_ADDEXMENU:
			if ( !tstrcmp(((ADDEXMENUINFO *)uptr)->exname, T("displaytype")) ){
				if ( ((ADDEXMENUINFO *)uptr)->hMenu == NULL ){
					((ADDEXMENUINFO *)uptr)->hMenu = CreatePopupMenu();
				}
				PPvAddDisplayTypeMenu(((ADDEXMENUINFO *)uptr)->hMenu, FALSE);
				return PPXCMDID_ADDEXMENU;
			}
			return 0;

		case PPXCMDID_COMMAND:{
			const TCHAR *param;

			param = uptr->str + tstrlen(uptr->str) + 1;
			if ( !tstrcmp(uptr->str, T("ZOOM")) ){ // *zoom
				if ( vo_.DModeBit & VO_dmode_IMAGE ){
					SetimgD(vinfo->info.hWnd, GetIntNumber(&param));
				}
				break;
			}

			if ( !tstrcmp(uptr->str, T("REDUCEMODE")) ){
				XV.img.imgD[1] = GetNumber(&param);
				InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
				break;
			}

			if ( !tstrcmp(uptr->str, T("JUMPLINE")) ){
				JumpLine(param);
				break;
			}

			if ( !tstrcmp(uptr->str, T("LAYOUT")) ){
				PPvLayoutCommand();
				break;
			}

			if ( !tstrcmp(uptr->str, T("FIND")) ){
				PPvFindCommand(vinfo, param);
				break;
			}

			if ( !tstrcmp(uptr->str, T("PREVIEW")) ){
				TCHAR buf[VFPS];
				PPXAPPINFOUNION tmpuptr;

				tmpuptr.enums.buffer = buf;
				PPxGetIInfo(vinfo, '1', &tmpuptr);

				if ( PreviewCommand(&BackScreen, param, buf) ){
					FullDraw = X_fles | 1;
				}else{
					FullDraw = X_fles;
				}
				InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
				break;
			}

			if ( !tstrcmp(uptr->str, T("VIEWOPTION")) ){
				VIEWOPTIONS viewopt;

				CheckParam(&viewopt, param, NULL);
				SetOpts(&viewopt);
				VOi->img = NULL;
				FixChangeMode(vinfo->info.hWnd);
				break;
			}
			return ERROR_INVALID_FUNCTION ^ 1;
		}

		case PPXCMDID_REQUIREKEYHOOK:
			KeyHookEntry = FUNCCAST(CALLBACKMODULEENTRY, uptr);
			break;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

int LogicalLineToDisplayLine(DWORD line)
{
	if ( line < 1 ){
		line = 1;
	}else if ( line > 1 ){	// 検索
		DWORD dh, dl, c;

		c = dl = 0;
		dh = VOi->line;
		while ( (dl + 1) < dh ){
			c = (dh + dl) >> 1;
			if ( VOi->ti[c].line < line ){
				dl = c;
			}else{
				dh = c;
			}
		}
		while ( VOi->ti[c].line >= line ) c--;
		line = c + 2;
	}
	return line;
}

void JumpLine(const TCHAR *linestr)
{
	BOOL logical;
	const TCHAR *p;
	DWORD line;

	if ( vo_.DModeBit == DOCMODE_NONE ) return;
	logical = (vo_.DModeBit == DOCMODE_TEXT) ? XV_numt : FALSE;
	p = linestr;
	if ( TinyCharUpper(*p) == 'L' ){
		p++;
		if ( vo_.DModeBit == DOCMODE_TEXT ) logical = TRUE;
	}
	if ( TinyCharUpper(*p) == 'Y' ){
		p++;
		logical = FALSE;
	}
	line = GetDwordNumber(&p);
	if ( logical != FALSE ) line = LogicalLineToDisplayLine(line);
	mtinfo.PresetY = line - 1; // 読み込み完了後の表示位置を指定
	line = line - 1 - ((VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY);
	MoveCsrkey(0, line, FALSE);

	if ( IsTrue(BackReader) ){
		mtinfo.PresetY = VOi->offY;
		setflag(mtinfo.OpenFlags, PPV__NoGetPosFromHist);
	}
}

void PPvLayoutCommand(void)
{
	HMENU hMenu;
	int i;

	hMenu = CreatePopupMenu();
	AppendMenuCheckString(hMenu, PPCLC_XWIN + 8, lstr_title,
			!(X_win & XWIN_NOTITLE));
	AppendMenuCheckString(hMenu, PPCLC_XWIN + 0, lstr_menu,
			X_win & XWIN_MENUBAR);
	AppendMenuCheckString(hMenu, PPCLC_XWIN + 5, lstr_status,
			!(X_win & XWIN_NOSTATUS));
	AppendMenuCheckString(hMenu, PPCLC_XWIN + 4, lstr_toolbar,
			X_win & XWIN_TOOLBAR);
	AppendMenuCheckString(hMenu, PPCLC_XWIN + 2, lstr_scroll,
			!(X_win & XWIN_HIDESCROLL));

	i = PPvTrackPopupMenu(hMenu);
	DestroyMenu(hMenu);
	if ( i >= PPCLC_XWIN ){
		X_win ^= 1 << (i - PPCLC_XWIN);
		SetCustTable(T("X_win"), T("V"), &X_win, sizeof(X_win));
		if ( i == PPCLC_XWIN + 0){	// メニュー
			SetMenu(vinfo.info.hWnd, (X_win & XWIN_MENUBAR) ? DynamicMenu.hMenuBarMenu : NULL);
		}
		if ((i == PPCLC_XWIN + 2) || (i == PPCLC_XWIN + 3)){	// scrollbar
			SCROLLINFO sinfo;

			sinfo.cbSize = sizeof(sinfo);
			sinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
			sinfo.nMin = 0;
			sinfo.nMax = 0;
			sinfo.nPage = 1;
			sinfo.nPos = 0;
			SetScrollInfo(vinfo.info.hWnd, SB_HORZ, &sinfo, TRUE);
			SetScrollInfo(vinfo.info.hWnd, SB_VERT, &sinfo, TRUE);
		}
		if ( i == PPCLC_XWIN + 4 ){	// toolbar
			InitGui();
		}
		if ( i == PPCLC_XWIN + 8 ){ // titlebar
			SetWindowLong(vinfo.info.hWnd, GWL_STYLE,
					GetWindowLong(vinfo.info.hWnd, GWL_STYLE) ^
							(WS_OVERLAPPEDWINDOW ^ WS_NOTITLEOVERLAPPED));
			SetWindowPos(vinfo.info.hWnd, NULL, 0, 0, 0, 0,
					SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
		}
		WmWindowPosChanged(vinfo.info.hWnd);
		InvalidateRect(vinfo.info.hWnd, NULL, TRUE);
	}
}


void FixChangeMode(HWND hWnd)
{
	InitViewObject(NULL, NULL);
	if ( VOsel.select != FALSE ) ResetSelect(TRUE); // 選択を解除
	InvalidateRect(hWnd, NULL, TRUE);
	if ( XV_tmod ){
		if ( vo_.DModeBit & VO_dmode_SELECTABLE ){
			if ( vo_.DModeBit == DOCMODE_HEX ) XV_lleft = 0;
			if ( VOsel.now.y.line >= VO_minY ){
				CalcTextXPoint(0, VOsel.now.y.line, SELPOSCHANGE_OFFSET);
			}
			MoveCsrkey(0, 0, FALSE);
			ShowCaret(hWnd);
		}else{
			HideCaret(hWnd);
		}
	}
}

/*-----------------------------------------------------------------------------
	メッセージ表示の設定
-----------------------------------------------------------------------------*/
void SetPopMsg(ERRORCODE err, const TCHAR *msg)
{
	if ( msg != NULL ){
		tstrcpy(PopMsgStr, MessageText(msg));
	}else{
		PPErrorMsg(PopMsgStr, err);
	}
	PopMsgFlag = PMF_WAITKEY | PMF_PROGRESS;

	InvalidateRect(vinfo.info.hWnd, &BoxStatus, TRUE);

	if ( !(err & POPMSG_NOLOGFLAG) ){
		if ( X_evoc == 1 ){
			PPxCommonExtCommand(K_FLASHWINDOW, (WPARAM)vinfo.info.hWnd);
			setflag(PopMsgFlag, PMF_FLASH);
		}
		XMessage(NULL, NULL, XM_ETCl, PopMsgStr);
	}
}
/*-----------------------------------------------------------------------------
	メッセージ表示の停止
-----------------------------------------------------------------------------*/
void StopPopMsg(int mask)
{
	if ( PopMsgFlag & mask & PMF_FLASH ){
		PPxCommonExtCommand(K_STOPFLASHWINDOW, (WPARAM)vinfo.info.hWnd);
		resetflag(PopMsgFlag, PMF_FLASH);
	}
	if ( PopMsgFlag & PMF_DISPLAYMASK ){
		resetflag(PopMsgFlag, mask);
		// 表示が無くなるので描画する
		if ( !(PopMsgFlag & PMF_DISPLAYMASK) ){
			InvalidateRect(vinfo.info.hWnd, &BoxStatus, TRUE);
		}
	}
}

BOOL PPvMouseCommand(PPV_APPINFO *vinfo, POINT *pos, const TCHAR *click, const TCHAR *type)
{
	TCHAR buf[VFPS], *p;

	pos->x++;
	p = PutShiftCode(buf, GetShiftKey());
	wsprintf(p, T("%s_%s"), click, type);

	if ( NO_ERROR == GetCustTable(T("MV_click"), buf, buf, sizeof(buf)) ){
		ExecDualParam(vinfo, buf);
		return TRUE;
	}else{
		return FALSE;
	}
}

BOOL PPvMouseCommandPos(PPV_APPINFO *vinfo, POINT *pos, const TCHAR *click, int Y)
{
	return PPvMouseCommand(vinfo, pos, click,
			(Y >= BoxView.top) ? T("SPC") : T("LINE"));
}

LRESULT PPvNCMouseCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT pos;
	const TCHAR *click, *type;

	switch(wParam){
		case HTBOTTOM:
		case HTBOTTOMLEFT:
		case HTBOTTOMRIGHT:
		case HTLEFT:
		case HTRIGHT:
		case HTTOP:
		case HTTOPLEFT:
		case HTTOPRIGHT:
			type = T("FRAM");
			break;

		case HTCLOSE:
			type = T("CLOS");
			break;

		case HTHSCROLL:
		case HTVSCROLL:
			type = T("SCRL");
			break;

		case HTMENU:
			type = T("MENU");
			break;

		case HTREDUCE:
			type = T("MINI");
			break;

		case HTZOOM:
			type = T("ZOOM");
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	switch(message){
		case WM_NCLBUTTONDBLCLK:
			click = T("LD");
			break;

		case WM_NCLBUTTONUP:
			click = T("L");
			break;

		case WM_NCMBUTTONDBLCLK:
			click = T("MD");
			break;

		case WM_NCMBUTTONUP:
			click = T("M");
			break;

		case WM_NCRBUTTONDBLCLK:
			click = T("RD");
			break;

		case WM_NCRBUTTONUP:
			click = T("R");
			break;

		case WM_NCXBUTTONDBLCLK:
			click = dtype[CheckXMouseButton(wParam)];
			break;
		case WM_NCXBUTTONUP:
			click = utype[CheckXMouseButton(wParam)];
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	LPARAMtoPOINT(pos, lParam);
	if ( PPvMouseCommand(&vinfo, &pos, click, type) == FALSE ){
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
// 窓の大きさを調整 -----------------------------------------------------------
void FixWindowSize(HWND hWnd, int offx, int offy)
{
	int width, height;
	DWORD tick;

	tick = GetTickCount();	// オーバーフローは無視
	if ( (vo_.DModeType != DISPT_NONE) && (!(offx | offy) || ((tick - WindowFixTick) > 2000)) ){ // 間隔長い→定型
		RECT desktop;

		switch(vo_.DModeType){
			case DISPT_HEX:
			case DISPT_TEXT:
			case DISPT_DOCUMENT:
				if ( vo_.DModeType != DISPT_HEX ){
					if ( XV_lnum && !XV_lleft ){
						XV_lleft = fontX * DEFNUMBERSPACE;
					}
				}else{
					XV_lleft = 0;
				}
				width =  (VOi->width + 1) * fontX + 1 + XV_left + XV_lleft;
				height = ((WndSize.cy / LineY) + offy ) * LineY + BoxView.top - 1;
				break;
			case DISPT_IMAGE: {
				int zoom;

				zoom = XV.img.imgD[0];
				if ( zoom <= IMGD_NORMAL ) zoom = 100;
				width = vo_.bitmap.showsize.cx * zoom / 100 + 1;
				height = vo_.bitmap.showsize.cy * zoom / 100 + BoxView.top + 1;
				break;
			}
			default:
				width = fontX * 6;
				height = BoxView.top;
		}
		GetDesktopRect(hWnd, &desktop);

		width += winS.right - winS.left - WndSize.cx;
		if ( (winS.left + width) > desktop.right ){
			width = desktop.right - winS.left;
		}
		height += winS.bottom - winS.top - WndSize.cy;
		if ( (winS.top + height) > desktop.bottom ){
			height = desktop.bottom - winS.top;
		}
	}else{	// 間隔短い→微調整
		width = (winS.right - winS.left) + fontX * offx;
		height = (winS.bottom - winS.top) + fontY * offy;
	}
	WindowFixTick = tick;
	SetWindowPos(hWnd, NULL, 0, 0, width, height,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

// 表示形式の切り換えメニュー -------------------------------------------------
void ChangeSusie(PPV_APPINFO *vinfo, HMENU hPopupMenu, int index)
{
	TCHAR tmppath[VFPS + 0x40], *p, *popt;

	tstrcpy(tmppath, vo_.file.name);
	p = VFSFindLastEntry(tmppath);
	popt = tstrstr(p, T("::"));
	if ( popt == NULL ){
		popt = p + tstrlen(p);
	}
	if ( index == 0 ){ // 登録
		*popt = '\0';
		AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
		VFSCheckImage(tmppath, vo_.file.image, vo_.file.UseSize, hPopupMenu);
	}else{ // 選択
		*popt++ = ':';
		*popt++ = ':';
		if ( GetMenuString(hPopupMenu, index, popt, 0x40, MF_BYCOMMAND) > 0 ){
			OpenAndFollowViewObject(vinfo, tmppath, NULL, NULL, 0);
		}
	}
}

void PPvAddDisplayTypeMenu(HMENU hPopupMenu, BOOL extend)
{
	DWORD i;

	for ( i = 0 ; i < 4 ; i++ ){
		AppendMenu(hPopupMenu,
			MFT_STRING |
			 ( ((vo_.SupportTypeFlags >> i) & 1) ? MF_ENABLED : MFS_GRAYED) |
			 ( ((i + 1) == vo_.DModeType) ? MF_CHECKED : 0 ),
			MENUID_TYPE_FIRST + i, MessageText(disptypestr[i]) );
	}
	if ( (vo_.DModeType == DISPT_RAWIMAGE) || extend ){
		AppendMenu(hPopupMenu, (vo_.DModeType == DISPT_RAWIMAGE) ?
				MFT_STRING | MF_CHECKED : MFT_STRING,
				MENUID_TYPE_RAWIMAGE, MessageText(disptypestr[4]));
	}
	if ( (vo_.DModeType == DISPT_TEXT) || (vo_.DModeType == DISPT_DOCUMENT) ){
		AppendMenu(hPopupMenu,
			MFT_STRING | (TailModeFlags ? MF_CHECKED : 0 ),
			MENUID_TYPE_TAIL, MessageText(disptypestr[5]) );
	}
	ChangeSusie(&vinfo, hPopupMenu, 0);
}

void GetDivMax(DDWORDST *divptr)
{
	if ( (FileRealSize.HighPart != 0) || (FileRealSize.LowPart >= X_wsiz) ){
		DWORD lastsize;

		*divptr = FileRealSize;
		lastsize = X_wsiz - 0x100;
		SubDD(divptr->LowPart, divptr->HighPart, lastsize, 0);
		divptr->LowPart &= 0xffffff00; // アドレス正規化
	}else{
		divptr->LowPart = 0;
	}
}

void ToggleTailMode(PPV_APPINFO *vinfo)
{
	TailModeFlags = TailModeFlags ? 0 : 1;
	if ( TailModeFlags ){
		OldTailLine = VO_maxY - 1;
		setflag(ShowTextLineFlags, SHOWTEXTLINE_OLDTEXT);

		if ( FileDivideMode > FDM_NODIVMAX ){
			GetDivMax(&FileDividePointer);
			PPvReload(vinfo);
		}
	}
	InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
}

ERRORCODE PPV_DisplayType(HWND hWnd, BOOL extend)
{
	HMENU hPopupMenu;
	int index;

	hPopupMenu = CreatePopupMenu();
	PPvAddDisplayTypeMenu(hPopupMenu, extend);
	index = PPvTrackPopupMenu(hPopupMenu);
	if ( index >= VFSCHK_MENUID ){ // SPI直接指定
		ChangeSusie(&vinfo, hPopupMenu, index);
		DestroyMenu(hPopupMenu);
		return NO_ERROR;
	}
	DestroyMenu(hPopupMenu);

	if ( index == MENUID_TYPE_TAIL ){
		ToggleTailMode(&vinfo);
		return NO_ERROR;
	}

	if ( (index < MENUID_TYPE_FIRST) || (index > MENUID_TYPE_LAST) ){
		return ERROR_CANCELLED;
	}

	if ( vo_.DModeType != (DWORD)((index - MENUID_TYPE_FIRST) + 1) ){
		vo_.DModeType = (index - MENUID_TYPE_FIRST) + 1;
		FixChangeMode(hWnd);
	}
	return NO_ERROR;
}

HMENU TextCodeMenu(BOOL SelectMode)
{
	int i, nowcode;
	HMENU hPopupMenu;
	TCHAR buf[32];

	hPopupMenu = CreatePopupMenu();

	if ( IsTrue(SelectMode) ){
		if ( IsTrue(VOsel.select) ){
			nowcode = VOi->ti[VOsel.top.y.line].type;
		}else{
			nowcode = -1;
		}
	}else{
		nowcode = VOi->textC;
	}

	if ( vo_.DModeBit & VO_dmode_TEXTLIKE ){
		int textC;

		for ( i = 0 ; i < (VTYPE_MAX - 1) ; i++ ){
			AppendMenuCheckString(hPopupMenu, MENUID_TEXTCODE + i, VO_textS[i], i == nowcode);
		}
		if ( SelectMode == FALSE ){
			if ( VO_textS[VTYPE_SYSTEMCP] != textcp_sjis ){
				AppendMenuCheckString(hPopupMenu, MENUID_TEXTCODE + VTYPE_MENU_EXSJIS, textcp_sjis, VO_CodePage == CP__SJIS);
			}

			wsprintf(buf, VO_CodePage ?
				T("&other CP %d...") : T("&other CP..."), VO_CodePage);
			AppendMenuCheckString(hPopupMenu, MENUID_TEXTCODE + VTYPE_OTHER, buf, nowcode == VTYPE_OTHER);

			textC = GetPPvTextCode(vo_.file.image, vo_.file.UseSize);
		}else{
			textC = GetPPvTextCode(VOi->ti[VOsel.top.y.line].ptr,
					VOi->ti[VOsel.top.y.line + 1].ptr - VOi->ti[VOsel.top.y.line].ptr);
		}
		if ( textC < VTYPE_MAX ){
			wsprintf(buf, T("Detect: %s"), VO_textS[textC]);
		}else{
			wsprintf(buf, T("Detect: CP%d"), textC - VTYPE_MAX);
			textC = VTYPE_OTHER;
		}
		AppendMenuString(hPopupMenu, MENUID_TEXTCODE + textC, buf);
	}
	if ( vo_.DModeBit & DOCMODE_TEXT ){
		if ( SelectMode == FALSE ){
			AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
			for ( i = 0 ; i <= 2 ; i++ ){
				AppendMenuCheckString(hPopupMenu, MENUID_KANAMODE + i, VO_textM[i], i == VO_Tmode);
			}
			AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
			AppendMenuCheckString(hPopupMenu, MENUID_ESCSWITCH, StrTcodeESC, VO_Tesc);
			AppendMenuCheckString(hPopupMenu, MENUID_MIMESWITCH, StrTcodeMIME, VO_Tmime);
			AppendMenuCheckString(hPopupMenu, MENUID_TAGSWITCH, StrTcodeTag, VO_Ttag == 1);
		}
	}
	AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
	if ( IsTrue(VOsel.select) && (vo_.DModeBit & DOCMODE_TEXT) ){
		AppendMenuString(hPopupMenu, MENUID_SELECTEDCODE, StrTcodeSelLines);
	}

	if ( vo_.DModeType == DISPT_RAWIMAGE ){
		struct RAWCOLORLISTSTRUCT *rcl;

		for ( rcl = RawColorList ; rcl->type != 0 ; rcl++ ){
			AppendMenuCheckString(hPopupMenu, MENUID_TEXTCODE + rcl->type,
					rcl->name, nowcode == rcl->type);
		}
	}else{
		AppendMenuString(hPopupMenu, MENUID_ALLCODE, StrTcodeAllText);
		CheckMenuRadioItem(hPopupMenu, MENUID_SELECTEDCODE, MENUID_ALLCODE,
			(SelectMode ? MENUID_SELECTEDCODE : MENUID_ALLCODE), MF_BYCOMMAND);
	}
	return hPopupMenu;
}

// テキストコード体系の切り換えメニュー ---------------------------------------
ERRORCODE PPV_TextCode(HWND hWnd, int defaultindex)
{
	HMENU hMenu;
	int index;
	BOOL SelectMode = FALSE;

	for ( ;; ){
		if ( defaultindex == 0 ){
			hMenu = TextCodeMenu(SelectMode);
			index = PPvTrackPopupMenu(hMenu);
			DestroyMenu(hMenu);
		}else{
			SelectMode = (vo_.DModeBit & DOCMODE_HEX) ? FALSE : TRUE;
			index = defaultindex;
			defaultindex = 0;
		}

		switch (index){
			case MENUID_ESCSWITCH: // ESCシーケンス
				VO_Tesc = VO_Tesc ? 0 : 1;
				VO_CodePage = 0;	// ISO-8859 等で変更することがあるため
				break;

			case MENUID_MIMESWITCH: // MIME text
				VO_Tmime = VO_Tmime ? 0 : 1;
				break;

			case MENUID_TAGSWITCH: // html tag
				VO_Ttag = VO_Ttag ? (VO_Ttag ^ 3) : 1;
				break;

			case MENUID_SELECTEDCODE: // 選択範囲の変更
				SelectMode = TRUE;
				continue;

			case MENUID_ALLCODE: // 全範囲の変更
				SelectMode = FALSE;
				continue;

			default:
				if ( index < MENUID_TEXTCODE ) return ERROR_CANCELLED;
				if ( index < MENUID_TEXTCODEMAX ){ // 文字コード
					if ( SelectMode ){
						int top, line;

						top = VOsel.top.y.line;
						for ( line = VOsel.bottom.y.line ; line <= top ; line++ ){
							VOi->ti[line].type = (BYTE)(index - MENUID_TEXTCODE);
						}
						InvalidateRect(hWnd, NULL, TRUE);
						return NO_ERROR;
					}else{
						if ( index == (VTYPE_MENU_EXSJIS + MENUID_TEXTCODE) ){
							VO_CodePage = CP__SJIS;
							index = VTYPE_OTHER + MENUID_TEXTCODE;
						}else if ( index == (VTYPE_OTHER + MENUID_TEXTCODE) ){
							DWORD cp;

							cp = GetCodePageType(hWnd);
							if ( cp == MAX32 ) return ERROR_CANCELLED;
							VO_CodePage = cp;
						}else{
							VO_CodePage = 0;
						}
						VO_CodePageValid = IsValidCodePage(VO_CodePage);
						VOi->textC = (index - MENUID_TEXTCODE);
					}
				}else{ // カナシフト処理
					VO_Tmodedef = (index - MENUID_KANAMODE);
				}
				break;
		}
		break;
	}
	VOi->img = NULL;
	FixChangeMode(hWnd);
	return NO_ERROR;
}
//----------------------------------------------------------------------------
#define TAILMARGIN 1
void SetScrollBar(void)
{
	SCROLLINFO sinfo;

	switch ( vo_.DModeType ){
		case DISPT_HEX:
		case DISPT_TEXT:
		case DISPT_DOCUMENT:
			VO_minX = VO_minY = 0;
			VO_maxX = VOi->width + TAILMARGIN;
			if ( vo_.DModeType == DISPT_TEXT ){
				if ( (VOi->defwidth == WIDTH_NOWARP) && IsTrue(XV_unff) ){
					VO_maxX = ScrollWidth + TAILMARGIN;
				}
				VO_maxX += (XV_lleft + fontX - 1) / fontX;
			}

			VO_maxY = LineCount == LC_READY ? (VOi->line + TAILMARGIN) : LONGTEXTWIDTH ;

			VO_sizeX = (WndSize.cx / fontX);
			VO_sizeY = ((BoxView.bottom - BoxView.top) / LineY + 1);
			VO_stepX = VO_stepY = 1;
			if ( (vo_.DModeBit == DOCMODE_HEX) && XV_lnum ){
				VO_sizeX -= DEFNUMBERSPACE;
			}

			if ( (vo_.DModeType != DISPT_DOCUMENT) || (vo_.DocmodeType != DOCMODE_EMETA) ){
				break;
			}
			// DISPT_IMAGE (DOCMODE_EMETAのときだけ)
			// Fall through
		case DISPT_IMAGE: // falls through to DISPT_RAWIMAGE
		case DISPT_RAWIMAGE:
			if ( vo_.DModeType != DISPT_RAWIMAGE ){
				if ( XV.img.AspectRate != 0 ){
					if ( XV.img.imgD[0] == IMGD_NORMAL ){
						XV.img.imgD[0] = 100;
					}
				}
			}else{ // DISPT_RAWIMAGE
				XV.img.monomode = FALSE;
				InitRawImageRectSize();
			}
			VO_maxX = vo_.bitmap.showsize.cx;
			VO_maxY = vo_.bitmap.showsize.cy;
			VO_minX = VO_minY = 0;
			VO_sizeX = WndSize.cx;
			VO_sizeY = WndSize.cy - BoxView.top;
			if ( XV.img.imgD[0] > IMGD_NORMAL ){
				VO_maxX = (VO_maxX * XV.img.imgD[0]) / 100;
				VO_maxY = (VO_maxY * XV.img.imgD[0]) / 100;
				XV.img.MagMode = IMGD_MM_MAGNIFY;
			}else if ( (XV.img.imgD[0] == IMGD_WINDOWSIZE) ||
					   ( ((XV.img.imgD[0] == IMGD_AUTOWINDOWSIZE) ||
						  (XV.img.imgD[0] == IMGD_AUTOFIXWINDOWSIZE)) &&
						 ((VO_maxX >= VO_sizeX) || (VO_maxY >= VO_sizeY)) ) ){
				VO_maxX = VO_sizeX - 1;
				VO_maxY = VO_sizeY - 1;
				XV.img.MagMode = IMGD_MM_FRAME;
			}else{
				XV.img.MagMode = IMGD_MM_FULLSCALE;
			}
			VO_stepX = (VO_maxX >> 5) + 1;
			VO_stepY = (VO_maxY >> 5) + 1;
			if ( VO_stepX > VO_sizeX ) VO_stepX = VO_sizeX;
			if ( VO_stepY > VO_sizeY ) VO_stepY = VO_sizeY;
			if ( VO_sizeY < 0 ) VO_sizeY = 0;
			break;

		default:
			VO_minX = VO_minY = 0;
			VO_maxX = VO_maxY = 0;
			VO_sizeX = VO_sizeY = 0;
			VO_stepX = VO_stepY = 0;
	}
	if ( (VO_maxX - VO_sizeX) <= VOi->offX ) VOi->offX = VO_maxX - VO_sizeX;
	if ( VO_minX > VOi->offX ) VOi->offX = VO_minX;
	if ( (VO_maxY - VO_sizeY) <= VOi->offY ) VOi->offY = VO_maxY - VO_sizeY;
	if ( VO_minY > VOi->offY ) VOi->offY = VO_minY;

	if ( X_win & XWIN_HIDESCROLL ) return;
	sinfo.cbSize = sizeof(sinfo);
	sinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
	sinfo.nPage	= VO_sizeX;
	sinfo.nMin	= VO_minX;
	sinfo.nPos	= VOi->offX;
	sinfo.nMax	= VO_maxX;
	SetScrollInfo(vinfo.info.hWnd, SB_HORZ, &sinfo, TRUE);

	if ( (FileDivideMode >= FDM_DIV) && (vo_.DModeBit & VO_dmode_TEXTLIKE) ){ // 分割表示
		DWORD pos, posmax, npos;
		DDWORDST rsize;

		rsize = FileRealSize;
		if ( (rsize.HighPart != 0) || (rsize.LowPart >= X_wsiz) ){
			DWORD lastsize;

			lastsize = (X_wsiz - 1) & 0xffffff00;
			SubDD(rsize.LowPart, rsize.HighPart, lastsize, 0);
		}else{ // X_wsiz に収まるファイルサイズ
			rsize.LowPart = 0;
		}
		sinfo.nPage	= 1;
		sinfo.nMin	= 0;
		sinfo.nMax	= 0x4000;
		if ( (rsize.HighPart != 0) || (rsize.LowPart >= 0x80000000) ){
			if ( rsize.HighPart >= 0x10000 ){ // 0xXXXX XXXX ???? ????
				pos = FileDividePointer.HighPart;
				posmax = rsize.HighPart;
				npos = 0; // 精度不足
			}else{ // 0x0000 XXXX XXXX ????
				pos = (FileDividePointer.HighPart << 16) |
					  (FileDividePointer.LowPart >> 16);
				posmax = (rsize.HighPart << 16) |
						 (rsize.LowPart >> 16);
				npos = (X_wsiz >= 0x10000) ?
						CalcMulDiv(VOi->offY, X_wsiz >> 16, VO_maxY) : 0;
			}
		}else{
			// 0x0000 0000 XXXX XXXX
			if ( rsize.LowPart != 0 ){
				pos = FileDividePointer.LowPart;
				posmax = rsize.LowPart;
			}else{
				pos = 0;
				posmax = 1;
			}
			npos = CalcMulDiv(VOi->offY, X_wsiz, VO_maxY);
		}
		sinfo.nPos = CalcMulDiv(pos + npos, sinfo.nMax, posmax);
	}else{
		sinfo.nMax = VO_maxY;
		sinfo.nPos	= VOi->offY;
		if ( LineCount != LC_READY ){ // 行計算完了前
			sinfo.nPage	= 1;
			sinfo.nMin	= 0;
		}else{ // 行計算済み
			sinfo.nPage	= VO_sizeY;
			sinfo.nMin	= VO_minY;
		}
	}
	SetScrollInfo(vinfo.info.hWnd, SB_VERT, &sinfo, TRUE);
}
//----------------------------------------------------------------------------
void USEFASTCALL MoveCsr(int x, int y, BOOL detailmode)
{
	RECT box;

	if ( (y != 0) && (FileDivideMode >= FDM_DIV) && (vo_.DModeBit & VO_dmode_TEXTLIKE) ){
		if ( (y > 0) && (y < VO_sizeY) && (VOi->offY == (VO_maxY - VO_sizeY)) ){
			DivChange(1);
			return;
		}
		if ( (y < 0) && (y > -VO_sizeY) && (VOi->offY == VO_minY) ){
			DivChange(-1);
			return;
		}
	}

	x += VOi->offX;
	y += VOi->offY;

	if ( vo_.DModeBit & DOCMODE_TEXT ){
		GetAsyncKeyState(VK_PAUSE); // 読み捨て(最下位bit対策)
		while ( VOi->reading != FALSE ){
			if ( (y + READLINE_SUPP) < VOi->line ) break;
			MakeIndexTable(MIT_NEXT, READLINE_SUPP);
			if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_FULLPUSH ) break;
		}
	}

	if ( (VO_maxX - VO_sizeX) <= x ) x = VO_maxX - VO_sizeX;
	if ( VO_minX > x ) x = VO_minX;
	if ( (VO_maxY - VO_sizeY) <= y ) y = VO_maxY - VO_sizeY;
	if ( VO_minY > y ) y = VO_minY;

	if ( (x == VOi->offX) && (y == VOi->offY) ) return;

	box = BoxAllView;
	if ( (vo_.DModeBit == DOCMODE_EMETA) ||	// 全画面書換
		 ((vo_.memo.bottom != NULL) && (vo_.memo.top > 1)) ){ // 仮？
		VOi->offX = x;
		VOi->offY = y;
		InvalidateRect(vinfo.info.hWnd, &box, TRUE);
	}else if ( vo_.DModeBit & (DOCMODE_BMP | DOCMODE_RAWIMAGE) ){
		switch ( X_scrm ){
			case 0:				// 全画面書換
				VOi->offX = x;
				VOi->offY = y;
				InvalidateRect(vinfo.info.hWnd, &box, FALSE);
				break;
			case 1:				// 全ウィンドウスクロール
				ScrollWindow(vinfo.info.hWnd, VOi->offX - x, VOi->offY - y, NULL, &box);
				VOi->offX = x;
				VOi->offY = y;
				break;
			case 2: {			// 部分スクロール
				HDC hDC;
				RECT rp;

				hDC = GetDC(vinfo.info.hWnd);
				ScrollDC(hDC, VOi->offX - x, VOi->offY - y, &box, &box, NULL, &rp);
				ReleaseDC(vinfo.info.hWnd, hDC);
				VOi->offX = x;
				VOi->offY = y;
				InvalidateRect(vinfo.info.hWnd, &rp, TRUE);
			}
		}
	}else{
		switch( X_scrm ){
			case 0:				// 全画面書換
				box.top = 0;
				VOi->offX = x;
				VOi->offY = y;
				InvalidateRect(vinfo.info.hWnd, &box, FALSE);
				break;
			case 1:				// 全ウィンドウスクロール
				if ( VOsel.cursor != FALSE ) HideCaret(vinfo.info.hWnd);
				ScrollWindow(vinfo.info.hWnd, (VOi->offX - x) * fontX,
							(VOi->offY - y) * LineY, NULL, &box);
				VOi->offX = x;
				VOi->offY = y;
				UpdateWindow(vinfo.info.hWnd);
				box.bottom = box.top - 1;
				box.top = 0;
				InvalidateRect(vinfo.info.hWnd, &box, TRUE);
				UpdateWindow(vinfo.info.hWnd);
				if ( VOsel.cursor != FALSE ) ShowCaret(vinfo.info.hWnd);
				break;
			case 2: {			// 部分スクロール
				HDC hDC;
				RECT rp;

				if ( VOsel.cursor != FALSE ) HideCaret(vinfo.info.hWnd);
				hDC = GetDC(vinfo.info.hWnd);
				ScrollDC(hDC, (VOi->offX - x) * fontX, (VOi->offY - y) * LineY,
						&box, &box, NULL, &rp);
				ReleaseDC(vinfo.info.hWnd, hDC);
				VOi->offX = x;
				VOi->offY = y;
				InvalidateRect(vinfo.info.hWnd, &rp, TRUE);
				UpdateWindow(vinfo.info.hWnd);
				box.bottom = box.top - 1;
				box.top = 0;
				InvalidateRect(vinfo.info.hWnd, &box, FALSE);
				UpdateWindow(vinfo.info.hWnd);
				if ( VOsel.cursor != FALSE ) ShowCaret(vinfo.info.hWnd);
			}
		}
	}
	if ( detailmode == FALSE ) SetScrollBar();
}
//----------------------------------------------------------------------------
#define MAX_HISTOPT_SIZE 80

void SaveHistory(BOOL ForceSave)
{
	const TCHAR *vp;

	UsePPx();
	vp = SearchHistory(PPXH_PPVNAME, vo_.file.name);
	FreePPx();
	if ( (vp != NULL) || ForceSave || // 一旦記憶したときは、常に有効
		// ヒストリ記憶が有効の時の条件
		 ((VO_history != 0) &&
				// 常時保存の条件
		  ( (VO_history == 1) || (X_hisr[1] == 1) ||
				// 設定変更時保存の条件（表示位置、表示種の変更）
		   ( (X_hisr[1] == 2) && (
			 (VOi->offX != 0) || (VOi->offY != 0) ||
			 (vo_.DModeType != (DWORD)viewopt_opentime.dtype ) ||
			// text
			 ( (vo_.DModeType != DISPT_IMAGE) &&
			   ( (VOi->textC != viewopt_opentime.T_code) ||
				 (OldTailLine > 0) ||
				 (VOsel.cursor != FALSE) ||
				 (VOi->width != VOi->defwidth) ) ) ||
			// image
			 ( (vo_.DModeType == DISPT_IMAGE) && vo_.bitmap.rotate ) ||
			// etc
			 (FileDivideMode == FDM_DIV2ND) ) ) ) )
	){
		DWORD optsize = 0;
		int i;

		#pragma pack(push, 4)
		struct {
			int info, x, y;
			char opt[MAX_HISTOPT_SIZE];
		} addbin;
		#pragma pack(pop)

		addbin.info = vo_.SupportTypeFlags | (vo_.DModeType << HISTOPT_VMODESHIFT);
		if ( vo_.DModeType == DISPT_IMAGE ){
			addbin.info |= (vo_.bitmap.rotate & 3) << HISTOPT_IMAGEOPT_FLAGSHIFT;
		}else{
			addbin.info |= VOi->textC << HISTOPT_TEXTOPT_CODESHIFT;
			if ( VO_Tmime ) setflag(addbin.info, HISTOPT_TEXTOPT_MIME << HISTOPT_TEXTOPT_FLAGSHIFT);
			if ( !VO_Tesc ) setflag(addbin.info, HISTOPT_TEXTOPT_ESC << HISTOPT_TEXTOPT_FLAGSHIFT);
			setflag(addbin.info, (((VO_Ttag & 3) + 1) << (HISTOPT_TEXTOPT_TAGSHIFT + HISTOPT_TEXTOPT_FLAGSHIFT)));
		}
		addbin.x = VOi->offX;
		addbin.y = VOi->offY;
		// 読み込み中に中止したときは以前の値を再利用する
		if ( (BackReader != FALSE) && (vp != NULL) ){
			if ( GetHistoryDataSize(vp) >= 12 ){
				const BYTE *bp;

				bp = GetHistoryData(vp);
				if ( *bp == (BYTE)vo_.SupportTypeFlags ){
					addbin.x = *(int *)(bp + 4);
					addbin.y = *(int *)(bp + 8);
				}
			}
		}
		// オプション項目の用意 --------------
		// HISTOPTID_OLDTAIL ※要先頭、HISTOPTID_FileDIV より前
		if ( ((vo_.DModeType == DISPT_TEXT) || (vo_.DModeType == DISPT_DOCUMENT)) && TailModeFlags ){
			if ( (optsize + HISTOPTSIZE_OLDTAIL) < MAX_HISTOPT_SIZE ){
				addbin.opt[optsize + 0] =  HISTOPTSIZE_OLDTAIL;
				addbin.opt[optsize + 1] = (BYTE)HISTOPTID_OLDTAIL;
				*(DWORD *)&addbin.opt[optsize + 2] = VO_maxY - 1;

				if ( (FileDivideMode >= FDM_DIV) &&
					 ((optsize + HISTOPTSIZE_Bookmark_L) < MAX_HISTOPT_SIZE) ){
					addbin.opt[optsize + 0] = HISTOPTSIZE_OLDTAIL_L;
					*(DDWORDST *)&addbin.opt[optsize + 6] = FileDividePointer;
					GetDivMax((DDWORDST *)&addbin.opt[optsize + 6]);
					optsize += HISTOPTSIZE_OLDTAIL_L - HISTOPTSIZE_OLDTAIL;
				}
				optsize += HISTOPTSIZE_OLDTAIL;
			}
		}
		// HISTOPTID_FileDIV ※要先頭、HISTOPTID_OLDTAIL より後
		if ( FileDivideMode == FDM_DIV2ND ){
			if ( (optsize + HISTOPTSIZE_FileDIV) < MAX_HISTOPT_SIZE ){
				addbin.opt[optsize + 0] = HISTOPTSIZE_FileDIV;
				addbin.opt[optsize + 1] = (BYTE)HISTOPTID_FileDIV;
				*(DDWORDST *)&addbin.opt[optsize + 2] = FileDividePointer;
				optsize += HISTOPTSIZE_FileDIV;
			}
		}
		if ( (vo_.DModeType == DISPT_TEXT) && VO_CodePage ){
			if ( (optsize + HISTOPTSIZE_OTHERCP) < MAX_HISTOPT_SIZE ){
				addbin.opt[optsize + 0] = HISTOPTSIZE_OTHERCP;
				addbin.opt[optsize + 1] = (BYTE)HISTOPTID_OTHERCP;
				*(WORD *)&addbin.opt[optsize + 2] = (WORD)VO_CodePage;
				optsize += HISTOPTSIZE_OTHERCP;
			}
		}
		if ( ((vo_.DModeType == DISPT_TEXT) || (vo_.DModeType == DISPT_DOCUMENT)) ){
			if ( VOi->width != VOi->defwidth ){
				if ( (optsize + HISTOPTSIZE_WIDTH) < MAX_HISTOPT_SIZE ){
					addbin.opt[optsize + 0] = HISTOPTSIZE_WIDTH;
					addbin.opt[optsize + 1] = (BYTE)HISTOPTID_WIDTH;
					*(int *)&addbin.opt[optsize + 2] = VOi->width;
					optsize += HISTOPTSIZE_WIDTH;
				}
			}
			if ( VOsel.cursor != FALSE ){
				if ( (optsize + HISTOPTSIZE_CARET) < MAX_HISTOPT_SIZE ){
					addbin.opt[optsize + 0] = HISTOPTSIZE_CARET;
					addbin.opt[optsize + 1] = (BYTE)HISTOPTID_CARET;
					*(int *)&addbin.opt[optsize + 2] = VOsel.now.x.offset;
					*(int *)&addbin.opt[optsize + 6] = VOsel.now.y.line;
					optsize += HISTOPTSIZE_CARET;
				}
			}
		}
		for ( i = 0 ; i <= MaxBookmark ; i++ ){
			if ( Bookmark[i].pos.x >= 0 ){
				if ( FileDivideMode < FDM_NODIVMAX ){
					if ( (optsize + HISTOPTSIZE_Bookmark) > MAX_HISTOPT_SIZE ) break;
					addbin.opt[optsize + 0] = HISTOPTSIZE_Bookmark;
					addbin.opt[optsize + 1] = (BYTE)(i + HISTOPTID_BookmarkMin);
					*(POINT *)&addbin.opt[optsize + 2] = Bookmark[i].pos;
					optsize += HISTOPTSIZE_Bookmark;
				}else{
					if ( (optsize + HISTOPTSIZE_Bookmark_L) > MAX_HISTOPT_SIZE ) break;
					addbin.opt[optsize + 0] = HISTOPTSIZE_Bookmark_L;
					addbin.opt[optsize + 1] = (BYTE)(i + HISTOPTID_BookmarkMin);
					*(BookmarkInfo *)&addbin.opt[optsize + 2] = Bookmark[i];
					optsize += HISTOPTSIZE_Bookmark_L;
				}
			}
		}
		WriteHistory(PPXH_PPVNAME, vo_.file.name, (WORD)(sizeof(int) * 3 + optsize), &addbin);
	}
}


void CloseViewObject(void)
{
	VO_INFO *tmpVOi;

	if ( UsePlayWave ){
		UsePlayWave = FALSE;
		DsndPlaySound(NULL, 0);
	}
	mtinfo.MemSize = 0;
	KillTimer(vinfo.info.hWnd, TIMERID_READLINE);
	if ( hReadStream != NULL ){
		CloseHandle(hReadStream);
		hReadStream = NULL;
	}

	ThFree(&vo_.memo);
								// 初期設定値と異なっていたらヒストリ書込
	if ( (vo_.DModeBit != DOCMODE_NONE) && (vo_.file.memdata == FALSE) ){
		SaveHistory(FALSE);
	}
	BackReader = FALSE;
	ReadingStream = READ_NONE;
	LineY = fontY + X_lspc;
	if ( LineY < 1 ) LineY = 1;

	vo_.file.source[0] = '\0';
	vo_.file.sourcefrom = SOURCEFROM_NONE;
	tstrcpy(vo_.file.typeinfo, T("Unknown"));
	vo_.SupportTypeFlags = 0;
	vo_.DModeType = DISPT_NONE;
	vo_.DModeBit = DOCMODE_NONE;
	vo_.DocmodeType = DOCMODE_NONE;
	VOi = &VO_I[DISPT_NONE];
	VO_history = -1;
	VO_Tmodedef = 0;
	VO_Tmode = 0;
	VO_Tesc = 1;
	VO_Ttag = 0;
	VO_Tshow_script = 1;
	VO_Tshow_css = 1;
	VO_Tmime = 0;
	VO_CodePage = 0;
	XV_lleft = 0;
	viewopt_opentime.dtype = -1;
	vo_.bitmap.page.max = 0;
	vo_.bitmap.page.current = 0;
	vo_.bitmap.rotate = 0;
	vo_.bitmap.transcolor = -1;
	vo_.bitmap.UseICC = -1;

	memset(Bookmark, 0xff, sizeof(Bookmark)); // Bookmark[n].pos.x = -1;
	ShowTextLineFlags = 0;
	TailModeFlags = 0;

	if ( vo_.file.mapH != NULL ){
		GlobalUnlock(vo_.file.mapH);
		if ( GlobalFree(vo_.file.mapH) != NULL ){
			XMessage(NULL, NULL, XM_FaERRld, T("vo.file:free error"));
		}
	}
	if ( vo_.file.other.mapH != NULL ){
		GlobalUnlock(vo_.file.other.mapH);
		if ( GlobalFree(vo_.file.other.mapH) != NULL ){
			XMessage(NULL, NULL, XM_FaERRld, T("vo.file.other:free error"));
		}
	}

	vo_.file.mapH = NULL;
	vo_.file.image = NULL;
	vo_.file.other.mapH = NULL;
	vo_.file.other.image = NULL;

	if ( vo_.bitmap.bits.mapH != NULL ){
		LocalUnlock(vo_.bitmap.bits.mapH);
		LocalFree(vo_.bitmap.bits.mapH);
	}
	vo_.bitmap.bits.mapH = NULL;
	vo_.bitmap.bits.ptr = NULL;

	if ( vo_.bitmap.info_hlocal != NULL ){
		LocalUnlock(vo_.bitmap.info_hlocal);
		LocalFree(vo_.bitmap.info_hlocal);
	}
	vo_.bitmap.info_hlocal = NULL;
	vo_.bitmap.info = NULL;

	if ( vo_.bitmap.info_hlocal != NULL ){
		LocalUnlock(vo_.bitmap.info_hlocal);
		LocalFree(vo_.bitmap.info_hlocal);
	}
	vo_.bitmap.info_hlocal = NULL;
	vo_.bitmap.info = NULL;

	DIRECTXDEFINE(DxDrawFreeBMPCache(&vo_.bitmap.DxCache));

	if ( vo_.bitmap.AllocShowInfo != FALSE ){
		HeapFree(PPvHeap, 0, vo_.bitmap.ShowInfo);
		vo_.bitmap.AllocShowInfo = FALSE;
	}
	vo_.bitmap.ShowInfo = NULL;

	if ( vo_.text.text.mapH != NULL ){
		GlobalUnlock(vo_.text.text.mapH);
		if ( GlobalFree(vo_.text.text.mapH) != NULL ){
			XMessage(NULL, NULL, XM_FaERRld, T("vo.text:free error"));
		}
	}
	vo_.text.text.mapH = NULL;
	vo_.text.text.ptr = NULL;

	if ( vo_.text.document.mapH != NULL ){
		GlobalUnlock(vo_.text.document.mapH);
		if ( GlobalFree(vo_.text.document.mapH) != NULL ){
			XMessage(NULL, NULL, XM_FaERRld, T("vo.document:free error"));
		}
	}
	vo_.text.document.mapH = NULL;
	vo_.text.document.ptr = NULL;

	if ( PreFrameInfo.bitmapsize != 0 ){
		PreFrameInfo.bitmapsize = 0;
		HeapFree(PPvHeap, 0, PreFrameInfo.bitmap);
	}

	{
		int i;

		tmpVOi = VO_I;
		for ( i = DISPT_NONE ; i < DISPT_MAX ; i++ ){
			tmpVOi->offX = tmpVOi->offY = 0;
			tmpVOi->textC = VTYPE_SYSTEMCP;
			tmpVOi->width = DEFAULTCOLS;
			tmpVOi->cline = tmpVOi->line = 0;
			tmpVOi->reading = TRUE;
			tmpVOi->tab = DEFAULTTAB;
			tmpVOi->img = NULL;
			tmpVOi->ti = &ti_dummy;
			tmpVOi->MakeText = MakeDispText;
			tmpVOi++;
		}
	}
	ResetSelect(TRUE);
	vo_.file.ImageSize = vo_.file.UseSize = FileRealSize.LowPart = FileRealSize.HighPart = 0;

//-------------------------------------------------------- Object:Text
//-------------------------------------------------------- Object:Document
	if ( vo_.eMetafile.handle != NULL ){
		DeleteEnhMetaFile(vo_.eMetafile.handle);
		vo_.eMetafile.handle = NULL;
	}

//-------------------------------------------------------- Object:Image
	if ( vo_.bitmap.hPal != NULL ) DeleteObject(vo_.bitmap.hPal);
	vo_.bitmap.hPal = NULL;
	XV.img.AspectRate = 0;
	HeapCompact(PPvHeap, 0);
}

// 拡大縮小モードの保存 -------------------------------------------------------
void SetimgD(HWND hWnd, int newD)
{
	int oldVO_maxX, oldVO_maxY;
	int centerX, centerY;

	if ( newD == 100 ) newD = IMGD_NORMAL;
	XV.img.imgD[0] = newD;
	SetCustTable(T("XV_imgD"), (hViewParentWnd == NULL) ? RegCID : StrRegEmbed,
			&XV.img.imgD, sizeof(XV.img.imgD));

	oldVO_maxX = max(VO_maxX, 1);
	oldVO_maxY = max(VO_maxY, 1);

	SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
	SetScrollBar();

	if ( XV.img.imgD[0] <= IMGD_NORMAL ){
		SetPopMsg(POPMSG_NOLOGMSG, imgDmes[XV.img.imgD[0] - IMGD_AUTOFIXWINDOWSIZE]);
	}else{
		TCHAR buf[20];

		wsprintf(buf, T("%d%%"), XV.img.imgD[0]);
		SetPopMsg(POPMSG_NOLOGMSG, buf);
	}

	centerX = VOi->offX + (VO_sizeX / 2);
	centerY = VOi->offY + (VO_sizeY / 2);
	MoveCsr(CalcMulDiv(centerX, VO_maxX, oldVO_maxX) - centerX,
			CalcMulDiv(centerY, VO_maxY, oldVO_maxY) - centerY, FALSE);
	SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hWnd, NULL, TRUE);
}
#if !NODLL
void SkipSepA(char **p)
{
	for ( ;; ){
		char c;

		c = **p;
		if ( (c != ' ') && (c != '\t') && (c != '\r') && (c != '\n') ) break;
		(*p)++;
	}
}
#endif
void SetMag(int offset)
{
	if ( vo_.DModeBit & VO_dmode_IMAGE ){ // 画像
		int newD = XV.img.imgD[0];

		if ( newD == 100 ) newD = IMGD_NORMAL;
		if ( offset == IMGD_NORMAL ){
			if ( newD > IMGD_NORMAL ){		// % からModeへ移行
				newD = IMGD_NORMAL;
			}else{						// Mode トグル切替
				newD -= 1;
				if ( newD < IMGD_AUTOFIXWINDOWSIZE ) newD = 0;
			}
			SetimgD(vinfo.info.hWnd, newD);
			return;
		}
		if ( offset == IMGD_AUTOWINDOWSIZE_2 ){
			newD = (newD != 0) ? IMGD_NORMAL : IMGD_AUTOWINDOWSIZE;
			SetimgD(vinfo.info.hWnd, newD);
			return;
		}
		if ( newD < IMGD_NORMAL ){	// 非% → 等倍
			newD = IMGD_NORMAL;
		}else{
			if ( newD == IMGD_NORMAL ) newD = 100;	// 等倍→%
			if ( (vo_.bitmap.showsize.cx >= 1000) || (vo_.bitmap.showsize.cy >= 1000) ){
				if ( offset < 0 ){
					if ( newD <= -offset ){
						if ( newD <= 1 ) return;
						offset = -1;
					}
				}else{
					if ( newD < 10 ) offset = 1;
				}
				newD += offset;
			}else{
				newD += offset;
				if ( newD <= IMGD_NORMAL ){
					XV.img.imgD[0] = IMGD_MINMAG;
					return;
				}
			}
			if ( newD > IMGD_MAXMAG ){
				XV.img.imgD[0] = IMGD_MAXMAG;
				return;
			}
		}
		SetimgD(vinfo.info.hWnd, newD);
		return;
	}else{ // テキスト
		HDC hDC;

		if ( offset == IMGD_AUTOWINDOWSIZE_2 ) offset = 0;
		if ( offset == 0 ) X_textmag = 100;
		X_textmag += offset;
		if ( X_textmag <= IMGD_NORMAL ){
			X_textmag = IMGD_MINMAG;
			return;
		}
		if ( X_textmag > IMGD_MAXMAG ){
			X_textmag = IMGD_MAXMAG;
			return;
		}

		DeleteFonts();
		hDC = GetDC(vinfo.info.hWnd);
		MakeFonts(hDC, X_textmag);
		ReleaseDC(vinfo.info.hWnd, hDC);
		WmWindowPosChanged(vinfo.info.hWnd);
		InvalidateRect(vinfo.info.hWnd, NULL, FALSE);
	}
};

void FindFound(HWND hWnd, int y)
{
	VOsel.foundY = VOsel.lastY = y;

	// 表示位置を調整
	if ( VOsel.cursor != FALSE ){
		MoveCsrkey(0, y - VOsel.now.y.line, FALSE);
	}

	if ( ((VOi->offY + 2) > y) || ((VOi->offY + VO_sizeY - 4) < y) ){
		VOi->offY = y;
		if ( VO_sizeY > 4 ){
			if ( VOi->offY > 3 ){
				VOi->offY -= 3;
			}else{
				VOi->offY = 0;
			}
		}
	}

	if ( IsTrue(BackReader) ){
		mtinfo.PresetY = VOi->offY;
		setflag(mtinfo.OpenFlags, PPV__NoGetPosFromHist);
	}

	InvalidateRect(hWnd, NULL, TRUE);
	SetScrollBar();
	UpdateWindow_Part(hWnd);
}

void FindBackText(HWND hWnd, int mode)
{
	char strbin[VFPS * 2];
	char *of, *nf, *tf;
	int y, len;
	MSG msgs;

	len = SetFindString(strbin);
	if ( VOsel.lastY == -1 ){
		y = VOi->offY + VO_sizeY - 1;
		if ( y > VOi->line ) y = VOi->line - 1;
	}else{
		y = ((VOsel.lastY > VOi->offY) &&
			 (VOsel.lastY < (VOi->offY + VO_sizeY - 1)) ) ?
				VOsel.lastY : VOi->offY;
	}
	while ( y > 0 ){
		y--;
		if ( vo_.DModeBit == DOCMODE_HEX ){
			of = (char *)vo_.file.image + y * 16;
			nf = (char *)vo_.file.image + (y + 1) * 16;
		}else{
			of = (char *)VOi->ti[y].ptr;
			nf = (char *)VOi->ti[y + 1].ptr;
		}
		while ( of < nf ){
			tf = of;
			of = memchr(of, strbin[0], nf - tf);
			if ( of == NULL ) break;
			if ( len > 1 ){
				if ( memicmp(of, strbin, len) != 0 ){
					of++;
					continue;
				}
			}
			FindFound(hWnd, y);
			y = -1;	// for skip
			break;
		}
		if ( (y >= 0) && IsalphaA(strbin[0]) ){
			if ( vo_.DModeBit == DOCMODE_HEX ){
				of = (char *)vo_.file.image + y * 16;
			}else{
				of = (char *)VOi->ti[y].ptr;
			}
			while(of < nf){
				tf = of;
				of = memchr(of, strbin[0] ^ 0x20, nf - tf);
				if ( of == NULL ) break;
				if ( len > 1 ){
					if ( memicmp(of, strbin, len) != 0 ){
						of++;
						continue;
					}
				}
				FindFound(hWnd, y);
				y = -1;	// for skip
				break;
			}
		}
		if ( mode == -1 ) continue;
		if ( PeekMessage(&msgs, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) ){
			break;
		}
	}
}

void FindFowardText(HWND hWnd, int mode)
{
	char strbin[VFPS * 2];
	char *of, *nf, *tf;
	int y, len;
	MSG msgs;

	len = SetFindString(strbin);
	if ( VOsel.lastY == -1 ){	// 未検索→表示開始行から検索
		y = VOi->offY;
	}else{						// 以前の発見位置が画面内→以前の発見位置の次
		// (TT : 一回ローカル変数に入れないと (VOsel.lastY >= VOi->offY) が
		//		正しく評価してくれない…
		int lastY, offY;

		lastY = VOsel.lastY;
		offY = VOi->offY;
		y = ((lastY >= offY) && (lastY < (offY + VO_sizeY - 1)) ) ?
				lastY + 1 : offY;
	}
	while( (y+1) <= VOi->line ){
		if ( vo_.DModeBit == DOCMODE_HEX ){
			of = (char *)vo_.file.image + y * 16;
			nf = (char *)vo_.file.image + (y + 1) * 16;
		}else{
			of = (char *)VOi->ti[y].ptr;
			nf = (char *)VOi->ti[y+1].ptr;
		}
		while (of < nf){
			tf = of;
			of = memchr(of, strbin[0], nf - tf);
			if ( of == NULL ) break;
			if ( len > 1 ){
				if ( memicmp(of, strbin, len) != 0 ){
					of++;
					continue;
				}
			}
			FindFound(hWnd, y);
			y = VOi->line;	// for skip
			break;
		}
		if ( IsalphaA(strbin[0]) ){
			if ( vo_.DModeBit == DOCMODE_HEX ){
				of = (char *)vo_.file.image + y * 16;
			}else{
				of = (char *)VOi->ti[y].ptr;
			}
			while (of < nf){
				tf = of;
				of = memchr(of, strbin[0] ^ 0x20, nf - tf);
				if ( of == NULL ) break;
				if ( len > 1 ){
					if ( memicmp(of, strbin, len) != 0 ){
						of++;
						continue;
					}
				}
				FindFound(hWnd, y);
				y = VOi->line;	// for skip
				break;
			}
		}
		y++;
		if ( mode == 1 ) continue;
		if ( PeekMessage(&msgs, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) ){
			break;
		}
	}
}

void PPvReload(PPV_APPINFO *vinfo)
{
	TCHAR buf[VFPS];

	tstrcpy(buf, vo_.file.name);
	OpenAndFollowViewObject(vinfo, buf, NULL, NULL, PPV__reload);
}

void PPvReceiveRequest(HWND hWnd)
{
	int flags;

	flags = PPvViewName(ReceivedParam);
	if ( flags < 0 ) return;
	if ( flags & PPV_NOENSURE ){
		VFSFixPath(NULL, ReceivedParam, NULL, VFSFIX_PATH | VFSFIX_NOFIXEDGE | VFSFIX_VREALPATH);
		if ( !tstrcmp(ReceivedParam, vo_.file.name) ) return;
		OpenViewObject(ReceivedParam, NULL, NULL, flags);
	}else if ( flags & PPV_PARAM ){
		TCHAR name[VFPS];

		FirstCommand = NULL;
		WinPos.show = SW_SHOWDEFAULT;
		CheckParam(&viewopt_def, ReceivedParam, name);
		if ( name[0] != '\0' ) OpenViewObject(name, NULL, &viewopt_def, flags);
		if ( FirstCommand != NULL ){
			PostMessage(hWnd, WM_PPXCOMMAND, K_FIRSTCMD, 0);
		}
	}else{
		VFSFixPath(NULL, ReceivedParam, NULL, VFSFIX_PATH | VFSFIX_NOFIXEDGE | VFSFIX_VREALPATH);
		OpenViewObject(ReceivedParam, NULL, NULL, flags);
	}
	FollowOpenView(&vinfo);

	if ( WinPos.show != SW_SHOWDEFAULT ) ShowWindow(hWnd, WinPos.show);
	if ( flags & PPV_NOFOREGROUND ){
		if ( hViewReqWnd != NULL ) SetForegroundWindow(hViewReqWnd);
	}else{
		ForceSetForegroundWindow(hWnd);
		SetFocus(hWnd);
	}
	FixShowRectByShowStyle(NULL, NULL);
	if ( vo_.DModeBit != DOCMODE_NONE ) UpdateWindow_Part(hWnd);
}

ERRORCODE PPvExecute(HWND hWnd)
{
	TCHAR buf[VFPS], path[VFPS], *p;

	tstrcpy(path, vo_.file.name);
	p = tstrrchr(path, '\\');
	if ( p != NULL) *p = '\0';
	wsprintf(buf, T(" %s "), vo_.file.name);
	if ( tInput(hWnd, MES_TEXE, buf, TSIZEOF(buf),
			PPXH_COMMAND, PPXH_COMMAND) <= 0 ){
		return ERROR_CANCELLED;
	}
	PP_ExtractMacro(hWnd, &vinfo.info, NULL, buf, NULL, 0);
	return NO_ERROR;
}

ERRORCODE PPvShell(HWND hWnd)
{
	TCHAR buf[CMDLINESIZE];

	buf[0] = '\0';
	if ( tInput(hWnd, MES_TSHL, buf, TSIZEOF(buf),
			PPXH_COMMAND, PPXH_COMMAND) <= 0 ){
		return ERROR_CANCELLED;
	}
	PP_ExtractMacro(hWnd, &vinfo.info, NULL, buf, NULL, 0);
	return NO_ERROR;
}

void OpenBrowser(HWND hWnd, TCHAR *url)
{
	TCHAR browser[VFPS];
	HANDLE result;

	browser[0] = '\0';
	GetCustTable(T("A_exec"), T("browser"), browser, sizeof(browser));
	if ( browser[0] == '\0' ){
		result = PPxShellExecute(hWnd, NULL, url, NilStr, NilStr, 0, url);
	}else{
		result = PPxShellExecute(hWnd, NULL, browser, url, NilStr, 0, url);
	}
	if ( result == NULL ) SetPopMsg(POPMSG_NOLOGMSG, url);
}

void JumpToSelectedAddress(HWND hWnd)
{
	TMS_struct text = {{NULL, 0, NULL}, 0};
	const TCHAR *src;
	TCHAR buf[0x1000], *dest, *destmax, *hdr;

	if ( ClipMem(&text, -1, -1) == FALSE ) return;

	src = text.tm.p;
	dest = buf;
	destmax = buf + 0xff0;

	for ( ;; ){
		TCHAR c;

		c = *src;
		if ( (UTCHAR)c > ' ' ) break;
		if ( c == '\0' ) break;
		src++;
	}
	hdr = tstrchr(src, ':');
	if ( hdr != NULL ){
		if ( memcmp(src, httpstr + 1, 6 * sizeof(TCHAR)) == 0 ){
			src += 6;
			hdr = NULL;
		}
	}
	if ( hdr == NULL ){
		tstrcpy(dest, httpstr);
		dest += 7;
	}

	while ( dest < destmax ){
		TCHAR c;

		c = *src++;
		if ( c == '\0' ) break;
		if ( (UTCHAR)c <= ' ' ) continue; // 制御文字を廃棄
		*dest++ = c;
	}
	*dest = '\0';
	TM_kill(&text.tm);

	if ( GetShiftKey() & K_c ){
		OpenAndFollowViewObject(&vinfo, buf, NULL, NULL, 0);
	}else{
		OpenBrowser(hWnd, buf);
	}
}

void PPvEditFile(HWND hWnd)
{
	TCHAR buf[VFPS], exe[VFPS];
	TCHAR curdir[VFPS], q[100];
	int line;

	if ( vo_.DModeBit & DOCMODE_TEXT ){
		line = (VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY;
		if ( line >= VOi->line ) line = VOi->line;
		line = VOi->ti[line].line; // 論理行番号に変換
	}else{
		line = VOi->offY;
	}

	GetCustTable(T("A_exec"), T("editor"), exe, sizeof(exe));
	if ( NO_ERROR != GetCustTable(T("A_exec"), T("editorL"), q, sizeof(q)) ){
		tstrcpy(q, T("/J"));
		if ( tInput(hWnd, MES_TEDJ, q, TSIZEOF(q),
				PPXH_GENERAL, PPXH_GENERAL) <= 0 ){
			return;
		}
		SetCustStringTable(T("A_exec"), T("editorL"), q, 0);
	}
	if (*q){
		wsprintf(buf, T("%s \x22%s\x22 %s%d"), exe, vo_.file.name, q, line);
	}else{
		wsprintf(buf, T("%s \x22%s\x22"), exe, vo_.file.name);
	}
	GetCurrentDirectory(TSIZEOF(curdir), curdir);
	ComExec(hWnd, buf, curdir);
}

DWORD USEFASTCALL PPvContextMenuAddSub(PPV_APPINFO *vinfo, HMENU hMenu, ThSTRUCT *TH, const TCHAR *ext)
{
	DWORD ID;
	TCHAR ccrname[VFPS];

	AppendMenuString(hMenu, K_Mc | K_s | K_F10, StrSHCM);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	ID = MENUID_USER;
	wsprintf(ccrname, T("M_Ccr%s"), (*ext != '\0') ? ext : T("NoExt") );
	PP_AddMenu(&vinfo->info, vinfo->info.hWnd, hMenu, &ID, ccrname, TH);
	if ( ID != MENUID_USER ) AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	if ( vo_.DModeBit == DOCMODE_BMP ){
		int menucount;

		menucount = GetMenuItemCount(hMenu);
		if ( vo_.bitmap.transcolor >= 0 ){
			AppendMenuCheckString(hMenu, K_M | 'M', StrAICP, (viewopt_def.I_CheckeredPattern > 0) );
		}
		if ( vo_.bitmap.page.max != 0 ){
			if ( !memcmp(vo_.file.image, "GIF8", 4) || !memcmp(vo_.file.image, "\x89PNG", 4) ){
				AppendMenuCheckString(hMenu, K_M | 'P', StrIGAN, vo_.bitmap.page.animate);
			}
			if ( vo_.bitmap.page.current > 0 ){
				AppendMenuString(hMenu, K_Mc | K_Pup, StrIGPP);
			}
			if ( (vo_.bitmap.page.current + 1) < vo_.bitmap.page.max ){
				AppendMenuString(hMenu, K_Mc | K_Pdw, StrIGNP);
			}
		}
		if ( vo_.bitmap.UseICC >= 0 ){
			AppendMenuCheckString(hMenu, KV_ICC, StrIGIC, vo_.bitmap.UseICC == ICM_ON);
		}
		if ( menucount < GetMenuItemCount(hMenu) ){
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		}
	}
	PP_AddMenu(&vinfo->info, vinfo->info.hWnd, hMenu, &ID, T("M_ppvc"), TH);
	return ID;
}

void PPvContextMenu(PPV_APPINFO *vinfo)
{
	HMENU hMenu;
	int menuid;
	ThSTRUCT TH;
	PPXMENUDATAINFO pmdi;
	TCHAR *ext;

	ThInit(&TH);
	ThInit(&pmdi.th);
	pmdi.id = MENUID_EXT;
	hMenu = CreatePopupMenu();

	ext = VFSFindLastEntry(vo_.file.name);
	ext += FindExtSeparator(ext);
	GetExtentionMenu(hMenu, ext, &pmdi);
	if ( vo_.DModeBit & VO_dmode_SELECTABLE ){
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		if ( VOsel.select != FALSE ){
			AppendMenuString(hMenu, K_Mc | 'C', StrClip);
		}
		AppendMenu(hMenu, MF_EPOP, (UINT_PTR)PPV_GetDisplayTypeMenu(), MessageText(StrEDET));

		if ( vo_.DModeBit & DOCMODE_TEXT ){
			AppendMenu(hMenu, MF_EPOP, (UINT_PTR)TextCodeMenu(VOsel.select), MessageText(StrEDEC));
			PPvContextMenuAddSub(vinfo, hMenu, &TH, ext);
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			if ( VOsel.select != FALSE ){
				AppendMenuString(hMenu, MENUID_SELECTURI, StrOPSA);
			}
			MakeURIs(hMenu, MENUID_URI);
		}else if ( vo_.DModeBit & (DOCMODE_HEX | DOCMODE_RAWIMAGE) ){
			AppendMenu(hMenu, MF_EPOP, (UINT_PTR)TextCodeMenu(FALSE), MessageText(StrEDEC));
		}else{
			PPvContextMenuAddSub(vinfo, hMenu, &TH, ext);
		}
	}else{
		if ( vo_.DModeBit & (DOCMODE_HEX | DOCMODE_RAWIMAGE) ){
			AppendMenu(hMenu, MF_EPOP, (UINT_PTR)TextCodeMenu(FALSE), MessageText(StrEDEC));
		}
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_EPOP, (UINT_PTR)PPV_GetDisplayTypeMenu(), MessageText(StrEDET));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		PPvContextMenuAddSub(vinfo, hMenu, &TH, ext);
	}
	menuid = PPvTrackPopupMenu(hMenu);

	while ( menuid > 0 ){
		TCHAR buf[0x400];

		if ( menuid < K_M ){
			if ( menuid == KV_ICC ){
				vo_.bitmap.UseICC = (vo_.bitmap.UseICC == ICM_ON) ? ICM_OFF : ICM_ON;
				SetPopMsg(POPMSG_NOLOGMSG, (vo_.bitmap.UseICC == ICM_ON) ? T("ICM on") : T("ICM off"));
				InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
			}
			break;
		}else if ( menuid <= MENUID_KEYLAST ){
			PPvCommand(vinfo, (WORD)((menuid - K_M) | K_raw));
			break;
		}else if ( (menuid >= MENUID_TYPE_FIRST) && (menuid <= MENUID_TYPE_LAST) ){
			if ( vo_.DModeType != (DWORD)((menuid - MENUID_TYPE_FIRST) + 1) ){
				vo_.DModeType = (menuid - MENUID_TYPE_FIRST) + 1;
				FixChangeMode(vinfo->info.hWnd);
			}
			break;
		}else if ( menuid == MENUID_TYPE_TAIL ){
			ToggleTailMode(vinfo);
			break;
		}else if ( (menuid >= MENUID_TEXTCODE) && (menuid <= MENUID_ALLCODE) ){
			PPV_TextCode(vinfo->info.hWnd, menuid);
			break;
		}else if ( menuid >= MENUID_USER ){
			const TCHAR *p;

			if ( menuid >= VFSCHK_MENUID ){ // SPI直接指定
				ChangeSusie(vinfo, hMenu, menuid);
			}else{
				GetMenuDataMacro2(p, &TH, menuid - MENUID_USER);
				if ( p != NULL ) PP_ExtractMacro(vinfo->info.hWnd, &vinfo->info, NULL, p, NULL, 0);
			}
			break;
		}else if ( menuid == MENUID_SELECTURI ){
			JumpToSelectedAddress(vinfo->info.hWnd);
			break;
		}

		GetMenuString(hMenu, menuid, buf, TSIZEOF(buf), MF_BYCOMMAND);
		if ( (menuid >= MENUID_URI) && (menuid < MENUID_USER) ){
			DWORD shiftkey = GetShiftKey();

			if ( shiftkey & K_s ){
				HGLOBAL hm;

				hm = GlobalAlloc(GMEM_MOVEABLE, TSTRSIZE(buf));
				if ( hm == NULL ) break;
				tstrcpy(GlobalLock(hm), buf);
				GlobalUnlock(hm);
				OpenClipboardV(vinfo->info.hWnd);
				EmptyClipboard();
				SetClipboardData(CF_TTEXT, hm);
				CloseClipboard();
			}else if ( shiftkey & K_c ){
				OpenAndFollowViewObject(vinfo, buf, NULL, NULL, 0);
			}else{
				OpenBrowser(vinfo->info.hWnd, buf);
			}
			break;
		}
		{ // MENUID_EXT
			const TCHAR *action;

			action = (const TCHAR *)pmdi.th.bottom;
			if ( action != NULL ){
				menuid -= MENUID_EXT;
				while ( menuid-- ) action += tstrlen(action) + 1;
			}
//			verb = (buf[0] == '*') ? NULL : buf;
			if ( NULL == PPxShellExecute(vinfo->info.hWnd, action, vo_.file.name, NilStr, NilStr, 0, buf) ){
				SetPopMsg(POPMSG_NOLOGMSG, buf);
			}
		}
		break;
	}
	hDispTypeMenu = NULL;
	DestroyMenu(hMenu);
	ThFree(&pmdi.th);
	ThFree(&TH);
}

int SetFindString(char *strbin)
{
	BYTE *src, *dst;
	int len;

	switch ( VOi->textC ){
		case VTYPE_JIS:
			for ( src = (BYTE *)VOsel.VSstringA, dst = (BYTE *)strbin ; *src ; ){
				BYTE low, high;

				low = (BYTE)(*src++ << 1);
				high = *src++;
				if ( high < 0x9f ){
					low  = (BYTE)(( low  < 0x3f ) ?
								(low  + 0x1f) : (low  - 0x61));
					high = (BYTE)(( high > 0x7e ) ?
								(high - 0x20) : (high - 0x1f));
				}else{
					low  = (BYTE)(( low  < 0x3f ) ?
								(low  + 0x20) : (low  - 0x60));
					high = (BYTE)(high - 0x7e);
				}
				*dst++ = low;
				*dst++ = high;
			}
			*dst = '\0';
			len = dst - (BYTE *)strbin;
			break;

		case VTYPE_EUCJP:
			for ( src = (BYTE *)VOsel.VSstringA, dst = (BYTE *)strbin ; *src ; ){
				BYTE low, high;

				low = *src++;
				if ( low < 0x80 ){
					*dst++ = low;
					continue;
				}
				if ( !IskanjiA(low) ){
					*dst++ = 0x8e;
					*dst++ = low;
					continue;
				}
				low = (BYTE)(low << 1);
				high = *src++;
				if ( high < 0x9f ){
					low  = (BYTE)(( low  < 0x3f ) ?
								(low  - 0x61) : (low  + 0x1f));
					high = (BYTE)(( high > 0x7e ) ?
								(high + 0x60) : (high + 0x61));
				}else{
					low  = (BYTE)(( low  < 0x3f ) ?
								(low  - 0x60) : (low  + 0x20));
					high = (BYTE)(high + 2);
				}
				*dst++ = low;
				*dst++ = high;
			}
			*dst = '\0';
			len = dst - (BYTE *)strbin;
			break;

		case VTYPE_UTF8: {
			WCHAR *p;
			BYTE *maxptr;

			p = VOsel.VSstringW;
			dst = (BYTE *)strbin;
			maxptr = dst + VFPS - 4;
			while( *p != '\0' ){
				WORD c;

				if ( dst >= maxptr ) break;
				c = *p++;
				if ( c < 0x80 ){
					*dst++ = (BYTE)c;
					continue;
				}
				if ( c < 0x800 ){
					*dst++ = (BYTE)(((c >> 6) & 0x1f) | 0xc0);
					*dst++ = (BYTE)((c & 0x3f) | 0x80);
					continue;
				}
				if ( (c >= 0xd800) && (c < 0xdc00) ){ // サロゲートペア
					DWORD c2;

					c2 = (DWORD)*p;
					if ( (c2 >= 0xdc00) && (c2 < 0xe000) ){
						p++;
						c2 = (c2 & 0x3ff) + ((c & 0x3ff) << 10) + 0x10000;
						*dst++ = (BYTE)(((c2 >> 18) &  0x7) | 0xf0);
						*dst++ = (BYTE)(((c2 >> 12) & 0x3f) | 0x80);
						*dst++ = (BYTE)(((c2 >>  6) & 0x3f) | 0x80);
						*dst++ = (BYTE)((c2 & 0x3f) | 0x80);
						continue;
					}
				}
				{// if (c < 0x10000){
					*dst++ = (BYTE)(((c >> 12) &  0xf) | 0xe0);
					*dst++ = (BYTE)(((c >>  6) & 0x3f) | 0x80);
					*dst++ = (BYTE)((c & 0x3f) | 0x80);
					continue;
				}
			}
			*dst = '\0';
			len = dst - (BYTE *)strbin;
			break;
		}
		case VTYPE_UNICODE:
			strcpyW((WCHAR *)strbin, VOsel.VSstringW);
			len = strlenW32(VOsel.VSstringW);
			len *= 2;
			break;

		default:
			strcpy(strbin, VOsel.VSstringA);
			len = strlen32(strbin);
			break;
	}
	return len;
}

BOOL FindInputBox(HWND hWnd, const TCHAR *title)
{
	TINPUT tinput;

	if ( !(vo_.DModeBit & VO_dmode_TEXTLIKE) ){
		return FALSE;
	}
	VOsel.foundY = -1;

	BackupOffY = -3;

	tinput.hOwnerWnd= hWnd;
	tinput.hRtype	= PPXH_WILD_R;
	tinput.hWtype	= PPXH_SEARCH;
	tinput.title	= title;
	tinput.buff		= VOsel.VSstring;
	tinput.size		= TSIZEOF(VOsel.VSstring);
	tinput.flag		= TIEX_NCHANGE;

	if ( tInputEx(&tinput) <= 0 ){
		VOsel.highlight = FALSE;

		if ( BackupOffY >= 0 ){
			if ( VOsel.cursor != FALSE ){
				MoveCsrkey(0, BackupSelY - VOsel.now.y.line, FALSE);
			}else{
				MoveCsrkey(0, BackupOffY - VOi->offY, FALSE);
			}
		}
		return FALSE;
	}
	if ( VOsel.foundY != -1 ) return FALSE; // 検索済み

	#ifdef UNICODE
		UnicodeToAnsi(VOsel.VSstringW, VOsel.VSstringA, VFPS);
	#else
		AnsiToUnicode(VOsel.VSstringA, VOsel.VSstringW, VFPS);
	#endif
	return TRUE;
}

void DoFind(HWND hWnd, int mode)
{
	int OldfoundY = VOsel.foundY;
	VOsel.foundY = -1;
	if ( VOsel.VSstring[0] == '\0' ){
		VOsel.highlight = FALSE;
		return;
	}
	VOsel.highlight = TRUE;
	if ( vo_.DModeBit & VO_dmode_TEXTLIKE ){
		if ( mode > 0 ){
			FindFowardText(hWnd, mode);
			if ( (mode == 2) && (VOsel.foundY == -1) ){
				FindBackText(hWnd, -2);
			}
		}else{
			FindBackText(hWnd, mode);
		}
	}
	if ( VOsel.foundY == -1 ){
		VOsel.foundY = OldfoundY;
		InvalidateRect(hWnd, NULL, TRUE);
		SetPopMsg(POPMSG_NOLOGMSG, MES_NOTX);
	}
	if ( (XV_tmod == 0) && (VOsel.cursor != FALSE) ){
		HideCaret(vinfo.info.hWnd);
		VOsel.cursor = FALSE;
	}
}

BOOL PPvSave(HWND hWnd)
{
	TCHAR name[VFPS];
	HANDLE hFile;
	DWORD size;

	if ( vo_.file.name[0] == '\0' ) return FALSE;
	if ( FindPathSeparator(vo_.file.name) != NULL ){
		tstrcpy(name, vo_.file.name);
	}else{
		VFSFullPath(name, vo_.file.name, NULL);
	}
	for ( ;; ){
		int i;
		if ( tInput(hWnd, T("Save"), name, VFPS, PPXH_NAME_R, PPXH_PATH) <= 0 ){
			return FALSE;
		}
		VFSFixPath(NULL, name, NULL, VFSFIX_PATH);

		hFile = CreateFileL(name, GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ) break;
						// 同名ファイルあり
		CloseHandle(hFile);
		i = PMessageBox(hWnd, MES_QSAM, T("File exist"),
				MB_APPLMODAL | MB_YESNOCANCEL |
						MB_DEFBUTTON1 | MB_ICONQUESTION);
		if ( i == IDYES ) break;
		if ( i == IDNO ) continue;
		return FALSE;
	}
	hFile = CreateFileL(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
					FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		PPErrorBox(hWnd, T("File open error"), PPERROR_GETLASTERROR);
		return FALSE;
	}
	if ( FALSE == WriteFile(hFile, vo_.file.image, vo_.file.UseSize, &size, NULL)){
		PPErrorBox(hWnd, T("File write error"), PPERROR_GETLASTERROR);
	}
	CloseHandle(hFile);
	return TRUE;
}
//-----------------------------------------------------------------------------
// テキスト変換

void EucWrite(HANDLE hF, BYTE *str)
{
	DWORD size;
	BYTE buf[0x1000], *p, code1;

	if ( convert == 1){
		WriteFile(hF, str, strlen32((const char *)str), &size, NULL);
		return;
	}
	p = buf;
	do {
		code1 = *(BYTE *)str++;
		if ( code1 < 0x80 ){
			*p++ = code1;
		}else{
			if (IskanjiA(code1)){
				BYTE code2;

				code2 = *(BYTE *)str++;
				if ( code2 >= 0x9f ){
					code1 = (BYTE)(code1 * 2 - (code1 >= 0xe0 ? 0xe0 : 0x60));
					code2 += (BYTE)2;
				}else if ( code2 != '\0' ){
					code1 = (BYTE)(code1 * 2 - (code1 >= 0xe0 ? 0xe1 : 0x61));
					code2 += (BYTE)(0x60 + (code2 < 0x7f));
				}
				*p++ = code1;
				*p++ = code2;
			}else{
				*p++ = 0x8e;
				*p++ = code1;
			}
		}
	} while(code1);
	WriteFile(hF, buf, strlen32((char *)buf), &size, NULL);
}

void ConvertMain(void)
{
	HANDLE hF;

	hF = CreateFileL(convertname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hF == INVALID_HANDLE_VALUE ){
//		PPErrorBox(NULL, T("File open error"), PPERROR_GETLASTERROR);
		return;
	}
	if ( vo_.DModeBit == DOCMODE_TEXT ){
		BYTE *p;
		DWORD size, off;
		int XV_bctl3bk;
		MAKETEXTINFO mti;

		XV_bctl3bk = XV_bctl[2];
		XV_bctl[2] = 0;

		InitMakeTextInfo(&mti);
		mti.srcmax = mtinfo.img + mtinfo.MemSize; // vo->file.image + vo->file.UseSize;
		mti.writetbl = FALSE;
		mti.paintmode = FALSE;

		for ( off = 0 ; off < (DWORD)VOi->line ; off++ ){
			VOi->MakeText(&mti, &VOi->ti[off]);
			p = mti.destbuf;

			while ( *p != VCODE_END ) switch(*p){ // VCODE_SWITCH
				case VCODE_CONTROL:
					// Fall through
				case VCODE_ASCII:	// Text 表示 ---------------------
					EucWrite(hF, p + 1);
					p += strlen((const char *)p + 1) + 2;
					break;

				case VCODE_UNICODEF:
					p++;
					// Fall through
				case VCODE_UNICODE:{	// Text 表示 -----------------
					BYTE buf2[0x1000];

					p++;
					WideCharToMultiByte(CP_ACP, 0,
							(LPCWSTR)p, -1, (LPSTR)buf2, sizeof buf2, "・", NULL);
					EucWrite(hF, buf2);
					while ( *(WORD *)p ) p += 2;
					p += 2;
					break;
				}

				case VCODE_COLOR:			// Color -----------------
					p += 3;
					break;

				case VCODE_FCOLOR:			// F Color ---------------
					// Fall through
				case VCODE_BCOLOR:			// B Color ---------------
					p += 1 + sizeof(COLORREF);
					break;

				case VCODE_FONT:			// Font ------------------
					p += 2;
					break;

				case VCODE_TAB:				// Tab -------------------
					WriteFile(hF, "\t", 1, &size, NULL);
					p++;
					break;

				case VCODE_RETURN:			// return -----------------
					p++;
					// Fall through
				case VCODE_PAGE: // Fall through
				case VCODE_PARA:
					WriteFile(hF, "\r\n", 2, &size, NULL);
					p++;
					break;

				case VCODE_LINK:		// Link ------------------
					p++;
					break;

				default:		// 未定義コード ----------------------
					p = (BYTE *)"";
					break;
			}
		}
		XV_bctl[2] = XV_bctl3bk;
		ReleaseMakeTextInfo(&mti);
	}
	CloseHandle(hF);
}

// 範囲選択モードを解除する
void ResetSelect(BOOL csrOff)
{
	if ( IsTrue(csrOff) ){
		if ( !XV_tmod ){
			if ( VOsel.cursor != FALSE ){
				HideCaret(vinfo.info.hWnd);
				VOsel.cursor = FALSE;
			}
			VOsel.now.y.line = -1;	// カーソル位置を初期化
		}
	}
	VOsel.select = FALSE;
	VOsel.foundY = VOsel.lastY = -1;
	VOsel.bottom.y.line = VOsel.top.y.line = -1;
}

void FixSelectRange(void)
{
	if ( VOsel.start.y.line < VOsel.now.y.line ){
		VOsel.bottom = VOsel.start;
		VOsel.top    = VOsel.now;
	}else if ( VOsel.start.y.line == VOsel.now.y.line ){
		VOsel.bottom.y = VOsel.start.y;
		VOsel.top.y    = VOsel.start.y;
		if ( VOsel.start.x.offset < VOsel.now.x.offset ){
			VOsel.bottom.x = VOsel.start.x;
			VOsel.top.x    = VOsel.now.x;
		}else{
			VOsel.bottom.x = VOsel.now.x;
			VOsel.top.x    = VOsel.start.x;
		}
	}else{	// ( VOsel.start.y.line > VOsel.now.y.line )
		VOsel.top    = VOsel.start;
		VOsel.bottom = VOsel.now;
	}
}

void InitCursorMode(HWND hWnd, BOOL initpos)
{
	VOsel.cursor = TRUE;

	if ( initpos || (VOsel.now.y.line < 0) ){
		VOsel.now.x.offset = 0;
		VOsel.now.y.line = VOi->offY + (VO_sizeY >> 1);
	}else{
	}
	if ( VO_maxY <= VOsel.now.y.line ) VOsel.now.y.line = VO_maxY - 1;
	if ( VOsel.now.y.line < 0 ) VOsel.now.y.line = 0;
	VOsel.now.x.pix = 0;
	VOsel.now.y.pix = VOsel.now.y.line * LineY;
	VOsel.posdiffX = 0;
	VOsel.linemode = TRUE;
	CreateCaret(hWnd, NULL, fontX, fontY);
	if ( vo_.DModeBit == DOCMODE_HEX ) XV_lleft = 0;
	if ( VOsel.now.y.line >= VO_minY ){
		CalcTextXPoint(0, VOsel.now.y.line, SELPOSCHANGE_OFFSET);
	}
	MoveCsrkey(0, 0, FALSE);
	ShowCaret(hWnd);
}

void PPvMinimize(HWND hWnd)
{
	if ( hViewParentWnd != NULL ){
#if 1
		SetFocus(GetParent(GetParent(hViewParentWnd)));
		hViewReqWnd = NULL;
		return;
#else
		HWND hParent;

		hParent = GetParent(GetParent(hViewParentWnd));
		if ( hParent != NULL ){
			SetFocus(hParent);
		}else{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		hViewReqWnd = NULL;
		return;
#endif
	}

	if ( (hViewReqWnd != NULL) && (IsIconic(hViewReqWnd) == FALSE) ){
		DWORD XV_minf = 2;

		GetCustData(T("XV_minf"), &XV_minf, sizeof(XV_minf));
		if ( (XV_minf == 1) && (Embed == FALSE) ){
			SendMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, MAX32);
			ForceSetForegroundWindow(hViewReqWnd);
			SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0,
					SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
// 旧方法。最小化した後、hViewReqWndで新規プロセスを作り、戻ったとき、hWndに
// 来てしまう
//			ShowWindow(hWnd, SW_SHOWMINIMIZED);
//			SetForegroundWindow(hViewReqWnd);
			return;
		}else if ( XV_minf == 2 ){
			SetForegroundWindow(hViewReqWnd);
		}
	}
	if ( Embed == FALSE ){
		PPxCommonCommand(hWnd, 0, K_s | K_esc);
		hViewReqWnd = NULL;
	}else{
		SetFocus(hViewReqWnd ? hViewReqWnd : GetParent(hWnd)); // ●最初の１回目が失敗する 1.57
	}
}

// ポップアップに使用する座標を得る -------------------------------------------
void GetPopupPosition(POINT *pos)
{
	switch ( PopupPosType ){
		case PPT_MOUSE:
			GetCursorPos(pos);
			pos->x++;
			break;

		case PPT_SAVED:
			*pos = PopupPos;
			pos->x++;
			break;

//		case PPT_FOCUS: // default兼用
		default:
			if ( VOsel.cursor == FALSE ){	// ページモード／カーソル非表示
				pos->x = 0;
				pos->y = LineY;
			}else{
				pos->x = XV_left + XV_lleft + (VOi->offX * fontX) + VOsel.now.x.pix;
				pos->y = (VOsel.now.y.line - VOi->offY + 1) * LineY + BoxView.top;
			}
			ClientToScreen(vinfo.info.hWnd, pos);
			break;
	}
}

// TrackPopupMenu の簡易版 ----------------------------------------------------
int PPvTrackPopupMenu(HMENU hMenu)
{
	POINT pos;

	GetPopupPosition(&pos);
	return TrackPopupMenu(hMenu, TPM_TDEFAULT, pos.x, pos.y, 0, vinfo.info.hWnd, NULL);
}

typedef struct {
	DWORD count;
	TCHAR basename[VFPS];
} DUMPEMFSTRUCT;

void DumpBMPinEmf(ENHMETARECORD *MFR, DUMPEMFSTRUCT *des, DWORD bmihoffset)
{
	TCHAR filename[VFPS];
	HANDLE hFile;

	wsprintf(filename, T("%s_%04d.bmp"), des->basename, des->count);
	hFile = CreateFileL(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		BITMAPFILEHEADER bmfileh;
		BITMAPINFOHEADER *bmih;
		DWORD size;

		bmih = (BITMAPINFOHEADER *)((BYTE *)&MFR->dParm + bmihoffset);
		bmfileh.bfType = 0x4d42; // 'BM'
		bmfileh.bfSize = sizeof(BITMAPFILEHEADER) + (MFR->nSize - sizeof(DWORD)*2 - bmihoffset);
		bmfileh.bfReserved1 = bmfileh.bfReserved2 = 0;
		bmfileh.bfOffBits = sizeof(BITMAPFILEHEADER) + CalcBmpHeaderSize(bmih);
		WriteFile(hFile, &bmfileh, sizeof(BITMAPFILEHEADER), &size, NULL);
		WriteFile(hFile, (BYTE *)bmih, MFR->nSize - (sizeof(DWORD) * 2) - bmihoffset, &size, NULL);
		CloseHandle(hFile);
	}
}

#pragma argsused
int CALLBACK DumpEmfProc(HDC hDC, HANDLETABLE *HTable, ENHMETARECORD *MFR, int Obj, DUMPEMFSTRUCT *des)
{
	UnUsedParam(hDC);UnUsedParam(HTable);UnUsedParam(Obj);

	des->count++;
	switch (MFR->iType){
		case EMR_GDICOMMENT:
		case EMR_SELECTOBJECT:
		case EMR_DELETEOBJECT:
		case EMR_EXTTEXTOUTA:
		case EMR_EXTTEXTOUTW:
		case EMR_SETTEXTCOLOR:
		case EMR_SETBKCOLOR:
		case EMR_SETBKMODE:
			break;

		case EMR_MOVETOEX:
			XMessage(NULL, NULL, XM_DbgLOG, T("%04d : MoveTo %d,%d"), des->count, MFR->dParm[0], MFR->dParm[1]);
			break;

		case EMR_LINETO:
			XMessage(NULL, NULL, XM_DbgLOG, T("%04d : LineTo %d,%d"), des->count, MFR->dParm[0], MFR->dParm[1]);
			break;

		case EMR_STRETCHDIBITS:
			XMessage(NULL, NULL, XM_DbgLOG, T("%04d : StretchDIBlt"), des->count);
			DumpBMPinEmf(MFR, des, 0x48);
			break;

		case EMR_STRETCHBLT:
			XMessage(NULL, NULL, XM_DbgLOG, T("%04d : StretchBlt"), des->count);
			DumpBMPinEmf(MFR, des, 0x64);
			break;

		default:
			XMessage(NULL, NULL, XM_DbgLOG, T("%04d : %d - %d"), des->count, MFR->iType, MFR->nSize);
	}
	return 1;
}

void DumpEmf(void)
{
	HDC hDC;
	RECT box;
	DUMPEMFSTRUCT des;

	if ( vo_.file.name[0] == '\0' ) return;
	if ( FindPathSeparator(vo_.file.name) != NULL ){
		tstrcpy(des.basename, vo_.file.name);
	}else{
		VFSFullPath(des.basename, vo_.file.name, NULL);
	}
	if ( tInput(vinfo.info.hWnd, T("Save Base name"),
			des.basename, VFPS, PPXH_NAME_R, PPXH_PATH) <= 0 ){
		return;
	}
	des.count = 0;
	box.left = box.top = 0;
	box.right = vo_.bitmap.rawsize.cx;
	box.bottom = vo_.bitmap.rawsize.cy;
	hDC = GetDC(vinfo.info.hWnd);
	EnumEnhMetaFile(hDC, vo_.eMetafile.handle, (ENHMFENUMPROC)DumpEmfProc, (void *)&des, &box);
	ReleaseDC(vinfo.info.hWnd, hDC);
}

void ReverseForeground(HWND hWnd)
{
	C_info ^= 0x808080;
//	C_mes ^= 0x808080;
	CV_char[CV__deftext] ^= 0x808080;
	InvalidateRect(hWnd, NULL, TRUE);
}

void ReverseBackground(HWND hWnd)
{
	COLORREF tmpc;

	AutoColor(T("C_back"), &tmpc, COLOR_WINDOW);

	C_back ^= C_WHITE;
	if ( C_back == tmpc ){
		GetCustData (T("CV_char"), CV_char, sizeof(CV_char));
		if ( CV_char[CV__defback] == C_AUTO ) CV_char[CV__defback] = C_back;
		if ( CV_char[CV__deftext] == C_AUTO ){
			CV_char[CV__deftext] = GetSysColor(COLOR_WINDOWTEXT);
		}
	}else{
		CV_char[CV__deftext] = CV_char[CV__defback];
		CV_char[CV__defback] = C_back;
	}

	DeleteObject(C_BackBrush);
	C_BackBrush = CreateSolidBrush(C_back);
	InvalidateRect(hWnd, NULL, TRUE);
}

void ViewfileContextMenu(HWND hWnd)
{
	POINT pos;

	GetPopupPosition(&pos);
	VFSSHContextMenu(hWnd, &pos, NULL, vo_.file.name, NULL);
}

void DivChange(int offset)
{
	DWORD rate;

	if ( (FileDivideMode < FDM_NODIVMAX) || !(vo_.DModeBit & VO_dmode_ENABLEBACKREAD) ){
		return;
	}
	rate = X_wsiz - 0x100;
	if ( offset >= 0 ){
		DDWORDST temp;

		temp = FileDividePointer;
		AddDD(temp.LowPart, temp.HighPart, rate, 0);
		if ( (temp.HighPart > FileRealSize.HighPart) ||
			 ((temp.HighPart == FileRealSize.HighPart) && (temp.LowPart >= FileRealSize.LowPart)) ){
			return;
		}
		FileDividePointer = temp;
		VOi->offY = 0;
	}else{
		if ( !(FileDividePointer.LowPart | FileDividePointer.HighPart) ){
			return;
		}
		if ( (FileDividePointer.HighPart == 0) &&
			 (FileDividePointer.LowPart < X_wsiz) ){
			FileDividePointer.LowPart = 0;
		}else{
			SubDD(FileDividePointer.LowPart, FileDividePointer.HighPart, rate, 0);
		}
		VOi->offY = 0x7ffffffe;
	}
	PPvReload(&vinfo);
}

int USEFASTCALL FixedWidthRange(int cols)
{
	if ( cols < 2 ){
		if ( cols == WIDTH_AUTOFIX ){
			if ( XV_lnum && (XV_lleft == 0) ){
				XV_lleft = fontX * DEFNUMBERSPACE;
			}
			cols = ((BoxView.right - BoxView.left) / fontX) - 1 - XV_lleft / fontX;
			if ( cols < 2 ) cols = 2;
		}else if ( cols == WIDTH_NOWARP ){
			cols = MAXLINELENGTH;
		}else{
			cols = DEFAULTCOLS;
		}
	}
	return cols;
}

ERRORCODE ChangeWidth(HWND hWnd)
{
	TCHAR buf[32];
	const TCHAR *p;

	wsprintf(buf, T("%d"), VOi->defwidth);
	if ( tInput(hWnd, T("Width(0:No warp, -1:Auto)"), buf, TSIZEOF(buf), PPXH_NUMBER, PPXH_NUMBER) <= 0 ){
		return ERROR_CANCELLED;
	}

	VOi->img = NULL;
	p = buf;
	VOi->defwidth = GetNumber(&p);
	VOi->width = FixedWidthRange(VOi->defwidth);
	ReMakeIndexes(hWnd);
	return NO_ERROR;
}

void LoadWinpos(HWND hWnd)
{
	WINPOS WPos;
	TCHAR buf[8];

	wsprintf(buf, T("%s_"), RegCID);
	if ( NO_ERROR == GetCustTable(T("_WinPos"), buf, &WPos, sizeof(WPos)) ){
		MoveWindow(hWnd, WPos.pos.left, WPos.pos.top,
			WinPos.pos.right - WinPos.pos.left,
			WinPos.pos.bottom - WinPos.pos.top, TRUE);
	}else{
		CenterWindow(hWnd);
	}
}

void ReadSizeChange(PPV_APPINFO *vinfo)
{
	DWORD X_wsiz = IMAGESIZELIMIT;

	if ( !(vo_.DModeBit & VO_dmode_ENABLEBACKREAD) ) return;

	if ( FileDivideMode < FDM_NODIVMAX ){
		GetCustData(T("X_wsiz"), &X_wsiz, sizeof X_wsiz);
		if ( vo_.file.UseSize <= X_wsiz ) return; // 分割不要

		FileDivideMode = FDM_DIV;
	}else{
		if ( FileDividePointer.HighPart ||
			 (FileRealSize.LowPart >= ((DWORD)1 * GB)) ){
			return; // 1G 以上は必ず分割
		}

		FileDividePointer.LowPart = FileDividePointer.HighPart = 0;
		FileDivideMode = FDM_FORCENODIV;
	}
	PPvReload(vinfo);
}

void SaveBookmark(int index)
{
	if ( IsTrue(VOsel.cursor) ){
		Bookmark[index].pos.x = VOsel.now.x.offset;
		Bookmark[index].pos.y = -VOsel.now.y.line;
	}else{
		Bookmark[index].pos.x = VOi->offX;
		Bookmark[index].pos.y = VOi->offY;
	}
	Bookmark[index].div = FileDividePointer;
	SaveHistory(TRUE);
	setflag(ShowTextLineFlags, SHOWTEXTLINE_BOOKMARK);
}

void GotoBookmarkPos(PPV_APPINFO *vinfo, int index, int posY)
{
	if ( (FileDivideMode > FDM_NODIVMAX) &&
		 ((Bookmark[index].div.LowPart != FileDividePointer.LowPart) ||
		  (Bookmark[index].div.HighPart != FileDividePointer.HighPart)) ){
		FileDividePointer = Bookmark[index].div;
		PPvReload(vinfo);
	}
	if ( IsTrue(BackReader) ){
		mtinfo.PresetY = posY;
		setflag(mtinfo.OpenFlags, PPV__NoGetPosFromHist);
	}
	MoveCsrkey(	Bookmark[index].pos.x - VOi->offX,
				posY - VOi->offY,	FALSE);
}

BOOL GotoBookmark(PPV_APPINFO *vinfo, int index)
{
	if ( Bookmark[index].pos.x < 0 ) return FALSE; // 記憶していない

	if ( Bookmark[index].pos.y >= 0 ){ // ページ位置で記憶
		if ( (Bookmark[index].pos.x == VOi->offX) &&
			 (Bookmark[index].pos.y == VOi->offY) &&
			 (FileDivideMode < FDM_NODIVMAX) ){
			return FALSE; // 移動不要
		}
		if ( index != BookmarkID_undo ){
			Bookmark[BookmarkID_undo].pos.x = VOi->offX;
			Bookmark[BookmarkID_undo].pos.y = VOi->offY;
			Bookmark[BookmarkID_undo].div = FileDividePointer;
		}
		GotoBookmarkPos(vinfo, index, Bookmark[index].pos.y);
	}else{ // キャレット位置で記憶
		if ( !(vo_.DModeBit & VO_dmode_SELECTABLE) ) return FALSE;

		if ( (Bookmark[index].pos.x == VOsel.now.x.offset) &&
			 (-Bookmark[index].pos.y == VOsel.now.y.line) &&
			 (FileDivideMode < FDM_NODIVMAX) ){
			return FALSE; // 移動不要
		}
		if ( index != BookmarkID_undo ){
			Bookmark[BookmarkID_undo].pos.x = VOsel.now.x.offset;
			Bookmark[BookmarkID_undo].pos.y = -VOsel.now.y.line;
			Bookmark[BookmarkID_undo].div = FileDividePointer;
		}

		if ( XV_tmod == 0 ){
			XV_tmod = 1;
			SetCustData(T("XV_tmod"), &XV_tmod, sizeof(XV_tmod));
			InitCursorMode(vinfo->info.hWnd, FALSE);
		}
		GotoBookmarkPos(vinfo, index, -Bookmark[index].pos.y);
	}
	return TRUE;
}

#pragma argsused
ERRORCODE SaveBookmarkMenu(PPV_APPINFO *vinfo)
{
	HMENU hPopupMenu = CreatePopupMenu();
	TCHAR buf[MAX_PATH];
	int i, index;
	UnUsedParam(vinfo);

	for ( i = BookmarkID_1st ; i <= MaxBookmark ; i++ ){
		wsprintf(buf, T("&%d: [%d,%d] <--"), i, Bookmark[i].pos.x, Bookmark[i].pos.y);
		AppendMenuString(hPopupMenu, i, buf);
	}
	index = PPvTrackPopupMenu(hPopupMenu);
	DestroyMenu(hPopupMenu);
	if ( index < BookmarkID_1st ) return ERROR_CANCELLED;
	SaveBookmark(index);
	return NO_ERROR;
}

ERRORCODE GotoBookmarkMenu(PPV_APPINFO *vinfo)
{
	HMENU hPopupMenu = CreatePopupMenu();
	TCHAR buf[MAX_PATH];
	int i, index;

	#define TailID 1
	#define BookMarkID 2

	for ( i = 0 ; i <= MaxBookmark ; i++ ){
		if ( Bookmark[i].pos.x >= 0 ){
			wsprintf(buf, T("&%d: <-- [%d,%d]"), i, Bookmark[i].pos.x, Bookmark[i].pos.y);
			AppendMenuString(hPopupMenu, i + BookMarkID, buf);
		}
	}
	if ( TailModeFlags ){
		AppendMenuString(hPopupMenu, TailID, disptypestr[5]);
	}
	index = PPvTrackPopupMenu(hPopupMenu);
	DestroyMenu(hPopupMenu);
	if ( index <= 0 ) return ERROR_CANCELLED;
	if ( index == TailID ){
		MoveCsr(0, (OldTailLine - 2) - VOi->offY, FALSE);
	}else{
		GotoBookmark(vinfo, index - BookMarkID);
	}
	return NO_ERROR;
}

#pragma argsused
void WINAPI DummyNotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild)
{
	UnUsedParam(event);UnUsedParam(hwnd);UnUsedParam(idObject);UnUsedParam(idChild);
}

void InputAspectRate(HWND hWnd)
{
	TCHAR buf[64];
	const TCHAR *p;

	wsprintf(buf, T("%dx%d"), XV.img.AspectW, XV.img.AspectH);
	if ( tInput(hWnd, T("Aspect rate(X ppi x Y ppi)"), buf, TSIZEOF(buf), PPXH_NUMBER, PPXH_NUMBER) <= 0 ){
		return;
	}

	XV.img.AspectRate = ASPACTX;
	p = buf;
	XV.img.AspectW = GetNumber(&p);
	if ( SkipSpace(&p) == 'x' ){
		p++;
		XV.img.AspectH = GetNumber(&p);

		XV.img.AspectRate = !XV.img.AspectH ? 0 : ((XV.img.AspectW * ASPACTX) / XV.img.AspectH);
		if ( (XV.img.AspectRate > (ASPACTX * 10)) ||
			 (XV.img.AspectRate < (ASPACTX / 10)) ||
			 (XV.img.AspectRate == ASPACTX) ){
			XV.img.AspectRate = 0;
		}
		FixBitmapShowSize(&vo_);
	}
	InvalidateRect(hWnd, NULL, TRUE);
	SetScrollBar();
}

void FixBitmapShowSize(PPvViewObject *vo)
{
	if ( vo->bitmap.rawsize.cy < 0 ){
		vo->bitmap.rawsize.cy = -vo->bitmap.rawsize.cy; // トップダウン
	}
	vo->bitmap.showsize = vo->bitmap.rawsize;

	if ( XV.img.AspectRate != 0 ){
		if ( XV.img.AspectRate >= ASPACTX ){
			vo->bitmap.showsize.cy = (vo->bitmap.showsize.cy * XV.img.AspectRate) / ASPACTX;
		}else{
			vo->bitmap.showsize.cx = (vo->bitmap.showsize.cx * ASPACTX) / XV.img.AspectRate;
		}
	}
}

void PPvEnterTabletMode(HWND hWnd)
{
	TouchMode = ~X_pmc[1];
	PPxCommonCommand(hWnd, 0, K_E_TABLET);
}

// , 区切りのパラメータを１つ取得する ※PPD_CMDL.Cにも
#if !NODLL
UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen)
{
	const TCHAR *src;
	TCHAR *dest, *destmax;
	UTCHAR firstcode, code;

	firstcode = SkipSpace(commandline);
	if ( (firstcode == '\0') || (firstcode == ',') ){ // パラメータ無し
		*param = '\0';
		return firstcode;
	}
	if ( firstcode == '\"' ) return GetLineParam(commandline, param);
	src = *commandline + 1;
	dest = param;
	destmax = dest + paramlen - 1;
	code = firstcode;
	for ( ;; ){
		if ( dest < destmax ) *dest++ = code;
		code = *src;
		if ( (code == ',') || // (code == ' ') ||
			 ((code < ' ') && ((code == '\0') || (code == '\t') ||
							   (code == '\r') || (code == '\n'))) ){
			break;
		}
		src++;
	}
	while ( (dest > param) && (*(dest - 1) == ' ') ) dest--;
	*dest = '\0';
	*commandline = src;
	return firstcode;
}
#endif
