/*-----------------------------------------------------------------------------
	Paper Plane vUI			Print
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

//=============================================================================
int		DefaultLinePerPage = 43;
BOOL	PrintAbort;
HWND	hDlgPrint;
const TCHAR MAKETEXTERROR[] = T("MakeTextError");
const TCHAR CprinttitleString[] = T("PPV Continuation printing");
const TCHAR comdlg32Name[] = T("comdlg32.dll");

void LoadPrint(void)
{
	if ( hComdlg32 != NULL ) return;
	hComdlg32 = LoadLibrary(comdlg32Name);
	if ( hComdlg32 == NULL ) return;
	PPrintDlg = (impPPRINTDLG)GetProcAddress(hComdlg32, "PrintDlg" TAPITAIL);
}

static PRINTDLG PDef = {
	sizeof(PRINTDLG),	// lStructSize
	NULL,				// hwndOwner
	NULL,				// hDevMode
	NULL,				// hDevNames
	NULL,				// hDC
	PD_ALLPAGES | PD_COLLATE | PD_RETURNDC | PD_NOSELECTION,
	1,					// nFromPage
	1,					// nToPage
	1,					// nMinPage
	1,					// nMaxPage
	1,					// nCopies
	NULL,				// hInstance
	0,					// lCustData
	NULL,				// lpfnPrintHook
	NULL,				// lpfnSetupHook
	NULL,				// lpPrintTemplateName
	NULL,				// lpSetupTemplateName
	NULL,				// hPrintTemplate
	NULL				// hSetupTemplate
};
int PrintPage(HDC hDC, int pageno, PRINTINFO *pinfo);
int PrintMain(HDC hDC, RECT *PrintRect, int pageno, PRINTINFO *pinfo);

void PPvDrawText(PAINTSTRUCT *ps, PPVPAINTSTRUCT *pps)
{
	int y, startY, i;
	BYTE *ptr;
	COLORREF fg, bg;
	POINT LP;
	RECT box;
	HFONT font;
	int XV_bctl3bk;
	MAKETEXTINFO mti;

	if ( pps->drawYtop ) pps->drawYtop--;
	if ( pps->drawYbottom ) pps->drawYbottom--;
	if ( (pps->drawYtop + pps->shift.cy) >= VOi->line  ) return;

	XV_bctl3bk = XV_bctl[2];
	XV_bctl[2] = 0;

	SetTextAlign(ps->hdc, TA_LEFT | TA_TOP | TA_UPDATECP);	// CP を有効に
												// 色の設定 -------------------
	fg = VOi->ti[pps->drawYtop + pps->shift.cy].Fclr;
	fg = (fg == CV__deftext) ? C_BLACK : CV_char[fg];
	bg = VOi->ti[pps->drawYtop + pps->shift.cy].Bclr;
	bg = (bg == CV__defback) ? C_WHITE : CV_char[bg];

	InitMakeTextInfo(&mti);
	mti.srcmax = mtinfo.img + mtinfo.MemSize;
	mti.writetbl = FALSE;
	mti.paintmode = TRUE;

	for ( y = pps->drawYtop ; y <= pps->drawYbottom ; y++ ){
		startY = y + pps->shift.cy;
		box.top = y * pps->lineY + pps->shiftdot.cy;
		box.bottom = box.top + pps->fontsize.cy;
		MoveToEx(ps->hdc, pps->shiftdot.cx, box.top, NULL);

		if ( startY >= VOi->line ){
			ptr = (BYTE *)"";
		}else{
												// 書体の設定 -----------------
			font = pps->hBoxFont;
			if ( VOi->ti[startY].type == VTYPE_IBM ) font = GetIBMFont();
			if ( VOi->ti[startY].type == VTYPE_ANSI ) font = GetANSIFont();
			SelectObject(ps->hdc, font);

			VOi->MakeText(&mti, &VOi->ti[startY]);
			ptr = mti.destbuf;
		}
		while( *ptr != VCODE_END ){
			switch(*ptr++){ // VCODE_SWITCH
				case VCODE_CONTROL:
				case VCODE_ASCII:	// Text 表示 ------------------------------
					SetTextColor(ps->hdc, fg);
					SetBkColor(ps->hdc, bg);
					i = strlen32((char *)ptr);
					TextOutA(ps->hdc, 0, 0, (char *)ptr, i);
					ptr += i + 1;
					break;

				case VCODE_UNICODEF:	// Dummy(WORD境界合わせ用) ------------
					ptr++;
				case VCODE_UNICODE:{	// Text 表示 --------------------------
					DWORD size = 0;
					WCHAR *last, *w;

					w = (WCHAR *)ptr;
					for ( last = w ; *last++ ; size++ );
					TextOutW(ps->hdc, 0, 0, w, size);
					ptr = (BYTE *)last;
					break;
				}

				case VCODE_COLOR:	// Color ----------------------------------
					fg = ( *ptr == CV__deftext ) ? C_BLACK : CV_char[*ptr];
					bg = ( *(ptr + 1) == CV__defback ) ? C_WHITE : CV_char[*(ptr + 1)];
					ptr += 2;
					break;

				case VCODE_FCOLOR:	// F Color --------------------------------
					fg = *(COLORREF *)ptr;
					ptr += sizeof(COLORREF);
					break;

				case VCODE_BCOLOR: {// B Color --------------------------------
					COLORREF nbg = *(COLORREF *)ptr;
					ptr += sizeof(COLORREF);
//					if ( nbg == C_BLACK ) nbg = C_BLACK;
					bg = nbg;
					break;
				}

				case VCODE_FONT:	// Font -----------------------------------
					switch(*ptr){
						case 0:
							font = GetIBMFont();
							break;
						case 1:
							font = GetANSIFont();
							break;
						default:
							font = pps->hBoxFont;
							break;
					}
					SelectObject(ps->hdc, font);
					ptr++;
					break;

				case VCODE_TAB: {	// Tab ------------------------------------
					int tabwidth;

					GetCurrentPositionEx(ps->hdc, &LP);
					box.left = LP.x;
					tabwidth = VOi->tab * pps->fontsize.cx;
					box.right = (((LP.x - pps->shiftdot.cx) / tabwidth) + 1) *
							tabwidth + pps->shiftdot.cx;
					MoveToEx(ps->hdc, box.right, box.top, NULL);
					break;
				}

				case VCODE_RETURN: // return ----------------------------------
					ptr++;
				case VCODE_PARA:
				case VCODE_PAGE:
					break;

				case VCODE_LINK:// Link ---------------------------------------
					SetTextColor(ps->hdc, CV_link);
					break;

				default:		// 未定義コード -------------------------------
					SetTextColor(ps->hdc, C_CYAN);
					TextOut(ps->hdc, 0, 0, MAKETEXTERROR, 13);
					ptr = (BYTE *)"";
					break;
			}
		}
		GetCurrentPositionEx(ps->hdc, &LP);
	}
	ReleaseMakeTextInfo(&mti);
	SetTextAlign(ps->hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);	// CP を無効に
	XV_bctl[2] = XV_bctl3bk;
}
// 印刷・描画部分 -------------------------------------------------------------
// joint = 1	複数のファイル印刷をまとめて１文書とみなす
int PrintOneDocument(HDC hDC, DOCINFO *dinfo, BOOL joint, PRINTINFO *pinfo)
{
	int CopyDoc, CopyDocMax;
	int CopyPage, CopyPageMax;
	int Page;
	int status = 0;
							// 部数設定
//	if ( PDef.Flags & PD_COLLATE ){	// ドライバによっては反応しないので部数限定
		CopyDocMax	= PDef.nCopies;
		CopyPageMax	= 1;
//	}else{
//		CopyDocMax	= 1;
//		CopyPageMax	= PDef.nCopies;
//	}
	if (!joint){
		status = StartDoc(hDC, dinfo);
		if ( status <= 0 ) return status;
	}
	for ( CopyDoc = 0 ; CopyDoc < CopyDocMax ; CopyDoc++ ){
		for ( Page = PDef.nFromPage - 1 ; Page < PDef.nToPage ; Page++ ){
			for ( CopyPage = 0 ; CopyPage < CopyPageMax ; CopyPage++ ){
				status = PrintPage(hDC, Page, pinfo);
				if ( status <= 0 ) return status;
						// 同じものを複数印刷する時は一旦初期化→両面印刷を考慮
				if ( !joint && (CopyPageMax > 1) ){
					status = EndDoc(hDC);
					if ( status <= 0 ) return status;
					if ( PrintAbort ) return -1;
					status = StartDoc(hDC, dinfo);
					if ( status <= 0 ) return status;
				}
			}
		}
						// 同じものを複数印刷する時は一旦初期化→両面印刷を考慮
		if ( !joint && (CopyDocMax > 1) && ((CopyDoc + 1) < CopyDocMax) ){
			status = EndDoc(hDC);
			if ( status <= 0 ) return status;
			if ( PrintAbort ) return -1;
			status = StartDoc(hDC, dinfo);
			if ( status <= 0 ) return status;
		}
	}
	if (!joint) status = EndDoc(hDC);
	return status;
}

// 1page分を印刷する
int PrintPage(HDC hDC, int pageno, PRINTINFO *pinfo)
{
	int status;
	RECT PrintRect;
										// 頁開始を通知 -----------------------
	status = StartPage(hDC);
	if ( status <= 0 ) return status;
	status = ExtEscape(hDC, NEXTBAND, 0, NULL, sizeof(PrintRect), (LPSTR)&PrintRect);
	if ( status <= 0 ){					// バンド非サポート処理 ---------------
		PrintRect.top		= 0;
		PrintRect.left		= 0;
		PrintRect.right		= GetDeviceCaps(hDC, HORZRES);
		PrintRect.bottom	= GetDeviceCaps(hDC, VERTRES);
		status = PrintMain(hDC, &PrintRect, pageno, pinfo);
		if ( status < 0 ) return status;
	}else{								// バンド処理 -------------------------
		while( !IsRectEmpty(&PrintRect) && !PrintAbort ){
			status = PrintMain(hDC, &PrintRect, pageno, pinfo);
			if ( status < 0 ) return status;
			status = ExtEscape(hDC, NEXTBAND, 0, NULL, sizeof(PrintRect), (LPSTR)&PrintRect);
			if ( status < 0 ) return status;
		}
	}
	status = EndPage(hDC);
	return status;
}

HFONT InitPrintFont(HDC hDC, RECT *PrintRect, int pageno, PPVPAINTSTRUCT *pps)
{
	HGDIOBJ hOldFont;
	TEXTMETRIC tm;
	LOGFONTWITHDPI lBoxFont;

	GetPPxFont(PPXFONT_F_fix, 0, &lBoxFont);

	lBoxFont.font.lfHeight = 0;
	pps->print = TRUE;
	pps->hBoxFont = CreateFontIndirect(&lBoxFont.font);
						// 現在のディスプレイコンテキストにフォントを設定する
	hOldFont = SelectObject(hDC, pps->hBoxFont);
										// フォント情報を入手
	GetAndFixTextMetrics(hDC, &tm);

	pps->shiftdot.cx = ((PrintInfo.X_prts.margin.left * 10 * GetDeviceCaps(hDC, LOGPIXELSX)) / 254) - GetDeviceCaps(hDC, PHYSICALOFFSETX);
	pps->shiftdot.cy = ((PrintInfo.X_prts.margin.top * 10 *  GetDeviceCaps(hDC, LOGPIXELSY)) / 254) - GetDeviceCaps(hDC, PHYSICALOFFSETY);

	pps->fontsize.cx = tm.tmAveCharWidth;
	pps->fontsize.cy = tm.tmHeight;
	pps->lineY = pps->fontsize.cy + pps->fontsize.cy / 2;
	if ( pps->lineY <= 0 ) pps->lineY = 1;

	pps->drawYtop = max(PrintRect->top - pps->shiftdot.cy, 0) / pps->lineY;
	pps->drawYbottom = max(PrintRect->bottom - pps->shiftdot.cy * 2,
			pps->drawYtop) / pps->lineY;
	pps->shift.cx = 0;
	pps->shift.cy = pageno *
		max((GetDeviceCaps(hDC, VERTRES) - pps->shiftdot.cy * 2), 0) / pps->lineY;
	return hOldFont;
}

// 本体
int PrintMain(HDC hDC, RECT *PrintRect, int pageno, PRINTINFO *pinfo)
{
	PAINTSTRUCT ps;
	PPVPAINTSTRUCT pps;
	HFONT hOldFont;
	int xDPI, yDPI;

	hOldFont = InitPrintFont(hDC, PrintRect, pageno, &pps);
										// 表示環境の設定 ---------------------
	ps.hdc = hDC;
	ps.fErase = FALSE;
//	ps.rcPaint	背景描画の時しか使わないので未定義
	pps.si.bgmode = TRUE;
	pps.ps = ps;

	xDPI = GetDeviceCaps(hDC, LOGPIXELSX);
	yDPI = GetDeviceCaps(hDC, LOGPIXELSY);

	SetBkColor(hDC, C_WHITE);
	SetTextColor(hDC, CV_char[CV__deftext]);		// 文字色
										// ２行目以降内容 --------------------
	if ( PrintRect->bottom > pps.lineY) switch(vo_.DModeType){
		case DISPT_HEX:					//=====================================
			DrawHex(&pps, &vo_);
			break;

		case DISPT_DOCUMENT:			//=====================================
			if ( vo_.DocmodeType == DOCMODE_EMETA ){ // metafile
				RECT box;
				HPALETTE hOldPal C4701CHECK;
				int DPI;

				if ( vo_.bitmap.hPal != NULL ){
					hOldPal = SelectPalette(hDC, vo_.bitmap.hPal, FALSE);
								// 論理パレットを現在のＤＣにマッピングする。
					RealizePalette(hDC);
				}

				DPI = pinfo->X_prts.imagedivision;
				if (DPI <= 0) DPI = 95;

				box.right = vo_.bitmap.showsize.cx * xDPI / DPI;
				box.bottom = vo_.bitmap.showsize.cy * yDPI / DPI;
				box.left = (GetDeviceCaps(hDC, HORZRES) - box.right) >> 1;
				box.top = (GetDeviceCaps(hDC, VERTRES) - box.bottom) >> 1;
				box.right += box.left;
				box.bottom += box.top;
				PlayEnhMetaFile(hDC, vo_.eMetafile.handle, &box);
				if ( vo_.bitmap.hPal != NULL ) SelectPalette(hDC, hOldPal, FALSE); // C4701ok
				break;
			}
		// DISPT_TEXT へ
		case DISPT_TEXT:				//=====================================
			PPvDrawText(&ps, &pps);
			break;

		case DISPT_IMAGE: {				//=====================================
			POINT oldpos;
			HPALETTE hOldPal C4701CHECK;
			int DPI, xOff, yOff, biWidth, biHeight, i;
			SIZE printsize;
											// hPalの論理パレットに変更する。
			if ( vo_.bitmap.hPal != NULL ){
				hOldPal = SelectPalette(hDC, vo_.bitmap.hPal, FALSE);
								// 論理パレットを現在のＤＣにマッピングする。
				RealizePalette(hDC);
			}
			if ( vo_.bitmap.UseICC >= 0 ) SetICMMode(hDC, vo_.bitmap.UseICC);
			printsize.cx = GetDeviceCaps(hDC, HORZRES) - (pps.shiftdot.cx - GetDeviceCaps(hDC, PHYSICALOFFSETX));
			printsize.cy = GetDeviceCaps(hDC, VERTRES) - (pps.shiftdot.cy - GetDeviceCaps(hDC, PHYSICALOFFSETX));
											// DIBを画面に転送
			DPI = pinfo->X_prts.imagedivision;
			if ( DPI == -1 ){ // 用紙一杯にする
				int DPI2;

				DPI  = vo_.bitmap.showsize.cx * xDPI * 100 / printsize.cx + 1;
				DPI2 = vo_.bitmap.showsize.cy * yDPI * 100 / printsize.cy + 1;
				if ( DPI < DPI2 ) DPI = DPI2;

				biWidth  = (vo_.bitmap.showsize.cx * 100 * xDPI) / DPI;
				biHeight = (vo_.bitmap.showsize.cy * 100 * yDPI) / DPI;
			}else{
				if ( DPI <= 0 ){
					DPI = 200;
					if (	  (vo_.bitmap.showsize.cx > 1200) ||
							  (vo_.bitmap.showsize.cy > 1933) ){
						DPI = 400;
					}else if ((vo_.bitmap.showsize.cx > 1800) ||
							  (vo_.bitmap.showsize.cy > 2890) ){
						DPI = 300;
					}
				}
				biWidth  = vo_.bitmap.showsize.cx * xDPI / DPI;
				biHeight = vo_.bitmap.showsize.cy * yDPI / DPI;
			}
			xOff = pps.shiftdot.cx + (printsize.cx - biWidth) / 2;
			yOff = pps.shiftdot.cy + (printsize.cy - biHeight) / 2;

			i = SetStretchBltMode(hDC, XV.img.imgD[1]);
			SetBrushOrgEx(hDC, 0, 0, &oldpos);
			StretchDIBits(hDC, xOff, yOff, biWidth, biHeight, 0, 0,
					vo_.bitmap.rawsize.cx, vo_.bitmap.rawsize.cy,
					vo_.bitmap.bits.ptr, (BITMAPINFO *)vo_.bitmap.ShowInfo, DIB_RGB_COLORS, SRCCOPY);
			SetBrushOrgEx(hDC, oldpos.x, oldpos.y, NULL);
			SetStretchBltMode(hDC, i);
			if ( vo_.bitmap.UseICC >= 0 ) SetICMMode(hDC, ICM_OFF);
											// 前のパレットに戻す。
			if ( vo_.bitmap.hPal != NULL ) SelectPalette(hDC, hOldPal, FALSE); // C4701ok
		}
	}
											// 後始末 -------------------------
	SelectObject(hDC, hOldFont);	// フォント
	DeleteObject(pps.hBoxFont);
	return 1;
}

// "Printing.." dialog-message ------------------------------------------------
#pragma argsused
INT_PTR CALLBACK PrintDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UnUsedParam(lParam);

	switch (msg){
		case WM_CLOSE:
			PrintAbort = TRUE;
			EnableWindow(GetParent(hDlg), TRUE);
			DestroyWindow(hDlg);
			hDlgPrint = NULL;
			return TRUE;

		case WM_INITDIALOG:
			SetWindowText(hDlg, StrPPvTitle);
			LocalizeDialogText(hDlg, IDD_PRINTDLG);
			CenterWindow(hDlg);
			return TRUE;

		case WM_COMMAND :
			if ( LOWORD(wParam) == IDCANCEL ){
				PostMessage(hDlg, WM_CLOSE, 0, 0);
			}
			return TRUE;
	}
	return PPxDialogHelper(hDlg, msg, wParam, lParam);
}
// "Printing.." dialog-message loop -------------------------------------------
#pragma argsused
int CALLBACK AbortProc(HDC hdcPrn, int iCode)
{
	MSG msg;
	UnUsedParam(hdcPrn);UnUsedParam(iCode);

	while( !PrintAbort && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( !hDlgPrint || !IsDialogMessage (hDlgPrint, &msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return !PrintAbort;
}

void FixPrintPages(void)
{
	HFONT hOldFont;
	RECT PrintRect;
	PPVPAINTSTRUCT pps;

	PrintRect.top		= 0;
	PrintRect.left		= 0;
	PrintRect.right		= GetDeviceCaps(PDef.hDC, HORZRES);
	PrintRect.bottom	= GetDeviceCaps(PDef.hDC, VERTRES);
	hOldFont = InitPrintFont(PDef.hDC, &PrintRect, 1, &pps);
	SelectObject(PDef.hDC, hOldFont);
	DeleteObject(pps.hBoxFont);
	DefaultLinePerPage = pps.shift.cy;
	if ( pps.shift.cy <= 0 ){
		PDef.nToPage = 1;
	}else{
		PDef.nToPage = min( PDef.nToPage,
				(WORD)((VOi->line + pps.shift.cy - 1) / pps.shift.cy) );
	}
}

void InitPrintPages(void)
{
	PDef.nToPage = PDef.nMaxPage = 1;
	switch(vo_.DModeType){
		case DISPT_DOCUMENT:
			if ( vo_.DocmodeType == DOCMODE_EMETA ) break;
		case DISPT_TEXT:
		case DISPT_HEX:
			PDef.nToPage = 9999;
			FixPrintPages();
			PDef.nMaxPage = PDef.nToPage;
			break;
	}
}

BOOL InitPrint(HWND hWnd, UINT DialogID)
{
	LoadPrint();
	if ( PPrintDlg == NULL ){
		SetPopMsg(POPMSG_MSG, MES_EC32);
		return FALSE;
	}
	GetCustData(T("X_prts"), &PrintInfo.X_prts, sizeof(PrintInfo.X_prts));
	BusyPPx(hWnd, hWnd);
	PDef.hwndOwner = hWnd;
	PDef.nToPage = PDef.nMaxPage = 1;
	switch(vo_.DModeType){
		case DISPT_DOCUMENT:
			if ( vo_.DocmodeType == DOCMODE_EMETA ) break;
		case DISPT_TEXT:
		case DISPT_HEX:
			PDef.nMaxPage = 9999;
			PDef.nToPage = (WORD)((VOi->line + DefaultLinePerPage - 1)
									/ DefaultLinePerPage);
			break;
	}
	if ( !PPrintDlg(&PDef) ){
		BusyPPx(hWnd, INVALID_HANDLE_VALUE);
		return FALSE;
	}
	if ( PDef.hDevMode != NULL ){ // 印刷部数を取得
		DEVMODE *dm;

		dm = GlobalLock(PDef.hDevMode);
		if ( dm->dmCopies != DMCOLLATE_FALSE ){ // ドライバの複数部印刷未対応
												// →アプリ側で対応する
			PDef.nCopies = max(dm->dmCopies, PDef.nCopies);
			dm->dmCopies = 1;
		}
		GlobalUnlock(PDef.hDevMode);
	}
	if ( !(PDef.Flags & (PD_PAGENUMS | PD_SELECTION)) ){
		PDef.nFromPage = 1;
		PDef.nToPage = PDef.nMaxPage;
	}
	if ( PDef.nMaxPage > 1 ) FixPrintPages(); // ページ数再算出
										// 中止ダイアログの表示 ---------------
	EnableWindow(hWnd, FALSE);
	PrintAbort = FALSE;
	hDlgPrint = CreateDialog(hInst, MAKEINTRESOURCE(DialogID), hWnd, PrintDlgProc);
	return TRUE;
}

BOOL EndPrint(HWND hWnd)
{
	BusyPPx(hWnd, INVALID_HANDLE_VALUE);
	if (!PrintAbort){
		EnableWindow(hWnd, TRUE);
		DestroyWindow(hDlgPrint);
		DeleteDC(PDef.hDC);
		return TRUE;
	}else{
		DeleteDC(PDef.hDC);
		return FALSE;
	}
}

// 通常印刷 -------------------------------------------------------------------
BOOL PPVPrint(HWND hWnd)
{
	DOCINFO dinfo = {sizeof(DOCINFO), NULL, NULL, NULL, 0};
	TCHAR buf[VFPS];
	int status;

	if ( InitPrint(hWnd, IDD_PRINTDLG) == FALSE ) return FALSE;

	wsprintf(buf, T("PPV:%s"), vo_.file.name);
	dinfo.lpszDocName = buf;
	SetAbortProc(PDef.hDC, AbortProc);
	status = PrintOneDocument(PDef.hDC, &dinfo, FALSE, &PrintInfo);
	if (status < 0) PPErrorBox(hWnd, T("Print"), PPERROR_GETLASTERROR);

	return EndPrint(hWnd);
}

// 1document化印刷 ------------------------------------------------------------
BOOL PPVPrintFiles(HWND hWnd, HDROP hDrop)
{
	DOCINFO dinfo = {sizeof(DOCINFO), CprinttitleString, NULL, NULL, 0};
	TCHAR name[VFPS], buf[VFPS];
	int document, documentleft, status = 1, Copy, CopyMax;

	if ( InitPrint(hWnd, IDD_MULTIPRINTDLG) == FALSE ) return FALSE;

	// ファイル一覧を抽出
	documentleft = DragQueryFile(hDrop, MAX32, NULL, 0);
	for ( document = 0 ; document < documentleft ; document++ ){
		DragQueryFile(hDrop, document, name, TSIZEOF(name));
		SendDlgItemMessage(hDlgPrint, IDL_PT_LIST, LB_ADDSTRING, 0, (LPARAM)name);
	}
	DragFinish(hDrop);

	if ( SetAbortProc(PDef.hDC, AbortProc) < 0 ){
		PPErrorBox(hWnd, T("Print"), PPERROR_GETLASTERROR);
	}

	CopyMax = PDef.nCopies;
	PDef.nCopies = 1;
	if (StartDoc(PDef.hDC, &dinfo) >= 0){
		for ( Copy = 0 ; Copy < CopyMax ; Copy++ ){
			for ( document = 0 ; document < documentleft ; document++ ){
											// 読み込み処理
				SendDlgItemMessage(hDlgPrint,
						IDL_PT_LIST, LB_SETCURSEL, (WPARAM)document, 0);
				SendDlgItemMessage(hDlgPrint,
						IDL_PT_LIST, LB_GETTEXT, (WPARAM)document, (LPARAM)name);
				wsprintf(buf, T("Printing [%d/%d]"), document + 1, documentleft);
				SetDlgItemText(hDlgPrint, IDS_PRINTDLGTXT, buf);

				if ( GetFileAttributesL(name) & FILE_ATTRIBUTE_DIRECTORY ){
					HANDLE hFF;
					WIN32_FIND_DATA ff;
					int cnt;

					cnt = 0;
					CatPath(buf, name, T("*"));
					hFF = FindFirstFileL(buf, &ff);
					if ( hFF == INVALID_HANDLE_VALUE ){
						status = 1;
						break;
					}else do{
						if (!(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
							CatPath(buf, name, ff.cFileName);
							OpenViewObject(buf, NULL, NULL, 0);
							InvalidateRect(hWnd, NULL, TRUE);
							UpdateWindow(hWnd);
							InitPrintPages();
							if ( vo_.DModeBit & VO_dmode_IMAGE ){
								status = PrintOneDocument(PDef.hDC,
												&dinfo, TRUE, &PrintInfo);
								if ( status <= 0 ) break;
								cnt++;
							}
						}
					}while(FindNextFile(hFF, &ff) != FALSE);
					FindClose(hFF);
					if ( cnt && ((document + 1) < documentleft) ){
						EndDoc(PDef.hDC);
						if (StartDoc(PDef.hDC, &dinfo) < 0){
							status = -1;
							break;
						}
					}
				}else{
					OpenViewObject(name, NULL, NULL, 0);
					InvalidateRect(hWnd, NULL, TRUE);
					UpdateWindow(hWnd);
					InitPrintPages();
					status = PrintOneDocument(PDef.hDC, &dinfo, TRUE, &PrintInfo);
				}
											// 印刷処理
				if (status <= 0) break;
			}
			if (status <= 0) break;
		}
		if (status >= 0) EndDoc(PDef.hDC);
	}
	PDef.nCopies = (WORD)CopyMax;
	if ( status < 0 ) PPErrorBox(hWnd, T("Print"), PPERROR_GETLASTERROR);
	return EndPrint(hWnd);
}
