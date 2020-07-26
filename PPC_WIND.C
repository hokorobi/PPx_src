/*-----------------------------------------------------------------------------
	Paper Plane cUI											Window 処理
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

#define HDR_FILENAME hdrstrings[0]
#define HDR_DATE hdrstrings[1]
#define HDR_SIZE hdrstrings[2]
#define HDR_ATTR hdrstrings[3]

const TCHAR *typestr1[] = {T("PATH"), T("LINE"), T("HMNU"), T("INFO"), T("ICON")};
const TCHAR *typestr2[] = {T("SPC"), T("MARK"), T("ENT"), T("TAIL")};
const TCHAR *XDtype[] = {T("?D"), T("XD"), T("YD"), T("ZD")};
const TCHAR *Xtype[] = {T("?"), T("X"), T("Y"), T("Z")};

typedef struct {
	const TCHAR *label;
	BYTE sortlow, sorthigh;
} TIME2HEADERSTRUCT;

TIME2HEADERSTRUCT Time2Header[] = {
	{ MES_HDRC, 4, 12 },
	{ MES_HDRE, 5, 13 },
	{ hdrstrings_eng_date, 3, 11 },
};

// マウス操作の解析・実行 -----------------------------------------------------

BOOL PPcMouseCommand(PPC_APPINFO *cinfo, const TCHAR *click, const TCHAR *type)
{
	TCHAR buf[CMDLINESIZE], *butptr;
									// クライアント領域の判別
	if ( (DWORD_PTR)type < 0x100 ){
		int inttype;

		inttype = PtrToValue(type);
		if ( (inttype >= PPCR_PATH) && (inttype <= PPCR_INFOICON) ){
			type = typestr1[inttype - PPCR_PATH];
		}else if ( (inttype >= PPCR_CELLBLANK) && (inttype <= PPCR_CELLTAIL) ){
			type = typestr2[inttype - PPCR_CELLBLANK];
		}
	}
									// 操作の確定
	butptr = PutShiftCode(buf, GetShiftKey());
	wsprintf(butptr, T("%s_%s"), click, type);
									// 実行
	if ( NO_ERROR == GetCustTable(T("MC_click"), buf, buf, sizeof(buf)) ){
		if ( type != typestr2[PPCR_CELLTAIL - PPCR_CELLBLANK] ){
			ExecDualParam(cinfo, buf); // 通常
		}else{ // PPCR_CELLTAIL の場合
			if ( (cinfo->e.cellPoint >= 0) && (cinfo->e.cellPoint < cinfo->e.cellIMax) ){
				ENTRYINDEX oldcelln;

				oldcelln = cinfo->e.cellN;
				cinfo->e.cellN = cinfo->e.cellPoint; // 一時的に変更
				ExecDualParam(cinfo, buf); // 通常
				cinfo->e.cellN = oldcelln;
			}
		}
		if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
		return TRUE;
	}else{
		return FALSE;
	}
}

HWND GetParentWnd(HWND hWnd)
{
	HWND hTmpWnd;

	hTmpWnd = GetParent(hWnd);
	return ( hTmpWnd == NULL ) ? hWnd : hTmpWnd;
}

// 非クライアント領域の判別 ---------------------------------------------------
LRESULT PPcNCMouseCommand(PPC_APPINFO *cinfo, UINT message, WPARAM wParam, LPARAM lParam)
{
	const TCHAR *click, *type;

	switch( LOWORD(wParam) ){
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

		case HTSYSMENU:
			type = T("SYSM");
			break;

		case HTCAPTION:
			type = T("TITL");
			break;

		case HTREDUCE:
			type = T("MINI");
			break;

		case HTZOOM:
			type = T("ZOOM");
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

		default:
			return DefWindowProc(GetParentWnd(cinfo->info.hWnd), message, wParam, lParam);
	}

	switch ( message ){
		case WM_NCLBUTTONDBLCLK:
			click = T("LD");
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
			click = XDtype[CheckXMouseButton(wParam)];
			break;

		case WM_NCXBUTTONUP:
			click = Xtype[CheckXMouseButton(wParam)];
			break;

//		case WM_NCLBUTTONUP:
		default:
			click = T("L");
			break;
	}
	cinfo->LastInputType = GetPointType();
	cinfo->PopupPosType = PPT_MOUSE;
	if ( !PPcMouseCommand(cinfo, click, type) ){
		return DefWindowProc(GetParentWnd(cinfo->info.hWnd), message, wParam, lParam);
	}
	return 0;
}

// 窓の大きさを調整 -----------------------------------------------------------
void FixWindowSize(PPC_APPINFO *cinfo, int offsetx, int offsety)
{
	int width, height;
	int widthFix, heightFix;	// 補正分

	widthFix = cinfo->wnd.NCArea.cx - cinfo->wnd.Area.cx +	// 枠分
			cinfo->BoxEntries.left - cinfo->fontX;	// 位置補正
	heightFix = cinfo->wnd.NCArea.cy - cinfo->wnd.Area.cy +	// 枠分
			cinfo->BoxEntries.top + cinfo->docks.b.client.bottom;	// 位置補正

	// スクロールバーが表示されていないときは、その分も加算する
	if ( cinfo->hScrollBarWnd != NULL ){
		if ( cinfo->ScrollBarHV == SB_HORZ ){
			heightFix += cinfo->ScrollBarSize;
		}else{
			widthFix += cinfo->ScrollBarSize;
		}
	}else if ( !(cinfo->X_win & XWIN_HIDESCROLL) &&
		!(GetWindowLongPtr(cinfo->info.hWnd, GWL_STYLE) &
				(WS_HSCROLL | WS_VSCROLL)) ){
		if ( cinfo->ScrollBarHV == SB_HORZ ){
			heightFix += GetSystemMetrics(SM_CYHSCROLL) % cinfo->cel.Size.cy;
		}else{
			if ( cinfo->celF.attr & DE_ATTR_WIDEW ){
				widthFix += GetSystemMetrics(SM_CXVSCROLL) % cinfo->fontX;
			}else{
				widthFix += GetSystemMetrics(SM_CXVSCROLL) % cinfo->cel.Size.cx;
			}
		}
	}

	if ( cinfo->celF.attr & DE_ATTR_WIDEW ){
		width = cinfo->cel.Area.cx *
				(cinfo->cel.Size.cx + offsetx * cinfo->fontX) + widthFix;
	}else{
		width = cinfo->cel.Area.cx * cinfo->cel.Size.cx + widthFix;
		if ( width != cinfo->wnd.NCArea.cx ){ // 微調整が必要
			if ( width > cinfo->wnd.NCArea.cx ){ // 1桁より少ない
				offsetx = 0;
			}else{
				if ( offsetx <= 0 ) offsetx += 1;
			}
		}
		width = max(1, cinfo->cel.Area.cx + offsetx) * cinfo->cel.Size.cx
				+ widthFix;
	}
	height = (cinfo->cel.Area.cy + offsety) * cinfo->cel.Size.cy + // cellArea分
			heightFix;

	if ( (width >= 16) && (height >= 8) ){
		SetWindowPos(cinfo->info.hWnd, NULL, 0, 0, width, height,
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	}
	InitCli(cinfo);
}

BOOL CalcJoinRate(int site, int mode, LONG *thispos, LONG *thissize, LONG *pairpos, LONG *pairsize, int offset, int mintype)
{
	int minvalue;

	if ( mode == FPS_RATE ){
		offset = (*thissize + *pairsize) * offset / 100 - *thissize;
	}

	if ( site & PAIRBIT ){	// 基準が(上/左)
		*thissize += offset;
		*pairpos  += offset;
		*pairsize -= offset;
	}else{				// 基準が(下/右)
		if ( mode == FPS_KEYBOARD ) offset = -offset;	// キーボード操作
		*thispos  -= offset;
		*thissize += offset;
		*pairsize -= offset;
	}
	minvalue = GetSystemMetrics(mintype);
	if ( *thissize < minvalue ) return FALSE; // これ以上はできない
	if ( *pairsize < minvalue ) return FALSE; // これ以上はできない
	return TRUE;
}

void FixPaneSize(PPC_APPINFO *cinfo, int offsetx, int offsety, int mode)
{
	if ( mode == FPS_RATE ){
		if ( (offsetx < 0) || (offsetx >= 100 ) ) return;
	}

	if ( cinfo->combo ){
		SCW_REQSIZE rs;

		rs.hWnd = cinfo->info.hWnd;
		rs.offsetx = offsetx;
		rs.offsety = offsety;
		rs.mode = mode;
		SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_reqsize, (LPARAM)&rs);
		return;
	}

	if ( (cinfo->swin & (SWIN_WBOOT | SWIN_FIXACTIVE)) && CheckReady(cinfo) ){
		HWND hPairWnd;
		HDWP hDWP;
		RECT thisbox, pairbox;
		int site;

		hPairWnd = GetJoinWnd(cinfo);
		if ( hPairWnd == NULL ) return;
		if ( cinfo->swin & SWIN_PILEJOIN ) return;// 重ねるときは処理しない

		thisbox = cinfo->wnd.NCRect;
		GetWindowRect(hPairWnd, &pairbox);

		thisbox.right -= thisbox.left;
		thisbox.bottom -= thisbox.top;
		pairbox.right -= pairbox.left;
		pairbox.bottom -= pairbox.top;

		site = cinfo->RegID[2];
		if ( cinfo->swin & SWIN_SWAPMODE ) site ^= PAIRBIT;

		if ( cinfo->swin & SWIN_UDJOIN ){	// 上
			if ( mode == FPS_KEYBOARD ){
				if ( offsetx ) return;
			}else{
				offsety = offsetx;
			}
			if ( mode != FPS_RATE ) offsety *= cinfo->fontY;
			if ( FALSE == CalcJoinRate(site, mode,
					&thisbox.top, &thisbox.bottom,
					&pairbox.top, &pairbox.bottom, offsety, SM_CYMIN) ){
				return;
			}
		}else{				// 左
			if ( (mode == FPS_KEYBOARD) && offsety ) return;
			if ( mode != FPS_RATE ) offsetx *= cinfo->fontX;
			if ( FALSE == CalcJoinRate(site, mode,
					&thisbox.left, &thisbox.right,
					&pairbox.left, &pairbox.right, offsetx, SM_CXMIN) ){
				return;
			}
		}

		hDWP = BeginDeferWindowPos(2);
		hDWP = DeferWindowPos(hDWP, cinfo->info.hWnd, 0,
				thisbox.left, thisbox.top, thisbox.right, thisbox.bottom,
				SWP_NOACTIVATE | SWP_NOZORDER);
		hDWP = DeferWindowPos(hDWP, hPairWnd, 0,
				pairbox.left, pairbox.top, pairbox.right, pairbox.bottom,
				SWP_NOACTIVATE | SWP_NOZORDER);
		if ( hDWP != NULL ) EndDeferWindowPos(hDWP);
	}
}

int GetTime2Len(char *format)
{
	int len = 0;

	for ( ;; ){
		char c;

		c = *format;
		switch ( c ){
			case '\0':
				return len;

			case 'I':
			case 'a':
			case 'g':
			case 'w':
				len += 3;
				break;

			case 'T':
			case 'Y':
				len += 4;
				break;

			case 'G':
				len += 6;
				break;

			default:
				if ( IsalphaA(c) ){
					len += 2;
				}else{
					len++;
				}
				break;
		}
		format++;
	}
}

int SortFmt(PPC_APPINFO *cinfo, char up, char down)
{
	int i, fmt = 0;
	char *mode;

	for ( i = 0, mode = cinfo->sort_nowmode.dat ; i < 3 ; i++, mode++ ){
		if ( *mode == down ){
			setflag(fmt, HDF_SORTDOWN);
			break;
		}else if ( *mode == up ){
			setflag(fmt, HDF_SORTUP);
			break;
		}
	}
	return fmt;
}
#define HEADERLPARAM(fmt, low, high) TMAKELPARAM(low + (high << 8), fmt - cinfo->celF.fmt)
void FixHeader(PPC_APPINFO *cinfo)
{
	HD_ITEM hdri;

	BYTE *fmt;
	int width = cinfo->fontX;
	int index = 0;
	BOOL UseCheckBox = OSver.dwMajorVersion >= 6;

								// 削除
	while ( SendMessage(cinfo->hHeaderWnd, HDM_DELETEITEM, 0, 0) );

	fmt = cinfo->celF.fmt;
	while( *fmt ){
		hdri.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT | HDI_LPARAM;
		hdri.fmt = HDF_STRING;
		hdri.lParam = -1;
		hdri.cchTextMax = 0;
		hdri.pszText = NilStrNC;
		switch( *fmt ){
			case DE_IMAGE:	// 画像
				hdri.cxy = width * *(fmt + 1);
				hdri.fmt = SortFmt(cinfo, 6, 14);
				hdri.lParam = HEADERLPARAM(fmt, 6, 14);
				break;

			case DE_CHECK:		// チェック
			case DE_CHECKBOX:	// チェックボックス
			case DE_ICON:	// アイコン
				hdri.cxy = width * 2;
				hdri.fmt = SortFmt(cinfo, 6, 14);
				hdri.lParam = HEADERLPARAM(fmt, 6, 14);
				break;

			case DE_ICON2:	// アイコン2
				hdri.cxy = *(fmt + 1) + ICONBLANK;
				hdri.fmt = SortFmt(cinfo, 6, 14);
				hdri.lParam = HEADERLPARAM(fmt, 6, 14);
				break;

			case DE_MARK:	// マーク
				hdri.fmt = SortFmt(cinfo, 6, 14);
				hdri.lParam = HEADERLPARAM(fmt, 6, 14);
				// DE_sepline へ
			case DE_sepline: // 区切り線
				hdri.cxy = width;
				break;

			case DE_SPC:	// 空白
			case DE_BLANK:	// 空白
				hdri.cxy = width * *(fmt + 1);
				break;

			case DE_LFN:
			case DE_SFN:
			case DE_LFN_MUL:
			case DE_LFN_LMUL:
			case DE_LFN_EXT:
			case DE_SFN_EXT:
			{
				int w;

				w = *(fmt + 2);
				if ( w >= 0xff ) w = 0;
				hdri.cxy = width * (*(fmt + 1) + w);
				hdri.pszText = HDR_FILENAME;
				hdri.fmt = SortFmt(cinfo, 0, 8);
				hdri.lParam = HEADERLPARAM(fmt, 0, 8);
				break;
			}
			case DE_SIZE1:
				hdri.cxy = width * 7;
				hdri.pszText = HDR_SIZE;
				hdri.fmt = SortFmt(cinfo, 2, 10);
				hdri.lParam = HEADERLPARAM(fmt, 2, 10);
				break;

			case DE_SIZE2:
			case DE_SIZE3:
			case DE_SIZE4:
				hdri.cxy = width * *(fmt + 1);
				hdri.pszText = HDR_SIZE;
				hdri.fmt = SortFmt(cinfo, 2, 10);
				hdri.lParam = HEADERLPARAM(fmt, 2, 10);
				break;

			case DE_ATTR1:
				hdri.cxy = width * *(fmt + 1);
				hdri.pszText = HDR_ATTR;
				hdri.fmt = SortFmt(cinfo, 16, 16);
				hdri.lParam = HEADERLPARAM(fmt, 16, 16);
				break;

			case DE_itemname:
			case DE_string:
			case DE_ivalue:
				hdri.cxy = width * tstrlen32((TCHAR *)fmt + 1);
				break;

			case DE_TIME1:
				hdri.cxy = width * *(fmt + 1);
				if ( UsePFont ){ // 可変長補正
					HDC hDC = GetDC(cinfo->info.hWnd);
					SIZE boxsize;
					HGDIOBJ hOldFont;
					hOldFont = SelectObject(hDC, cinfo->hBoxFont);

					GetTextExtentPoint32(hDC, T("00-00-00 00:00:00.000"),
							*(fmt + 1), &boxsize);
					hdri.cxy = boxsize.cx;
					SelectObject(hDC, hOldFont);	// フォント
					ReleaseDC(cinfo->info.hWnd, hDC);
				}

				hdri.pszText = HDR_DATE;
				hdri.fmt = SortFmt(cinfo, 3, 11);
				hdri.lParam = HEADERLPARAM(fmt, 3, 11);
				break;

			case DE_TIME2: {
				DWORD timetype;
				TIME2HEADERSTRUCT *hdrs;

				timetype = *(fmt + 1);
				if ( timetype > 2 ) timetype = 2;
				hdrs = &Time2Header[timetype];
				hdri.cxy = width * GetTime2Len((char *)fmt + 2);
				hdri.pszText = (TCHAR *)MessageText(hdrs->label);
				hdri.fmt = SortFmt(cinfo, hdrs->sortlow, hdrs->sorthigh);
				hdri.lParam = HEADERLPARAM(fmt, hdrs->sortlow, hdrs->sorthigh);
				break;
			}
			case DE_COLUMN:
			case DE_MEMOEX: {
				WORD itemindex;

				if ( *fmt == DE_COLUMN ){
					hdri.cxy = width * ((DISPFMT_COLUMN *)(fmt + 1))->width;
					hdri.pszText = ((DISPFMT_COLUMN *)(fmt + 1))->name;
					hdri.cchTextMax = tstrlen32(hdri.pszText);

					itemindex = ((DISPFMT_COLUMN *)(fmt + 1))->itemindex;
					if ( itemindex == DFC_UNDEF ){ // 未初期化
						itemindex = ((DISPFMT_COLUMN *)(fmt + 1))->itemindex =
								GetColumnExtItemIndex(cinfo,
									((DISPFMT_COLUMN *)(fmt + 1))->name);
					}
				}else{
					hdri.cxy = width * *(fmt + 1);
					itemindex = (WORD)(DFC_COMMENTEX_MAX - (*(fmt + 2) - 1));
				}

				if ( cinfo->sort_columnindex == (DWORD)itemindex ){
					if ( cinfo->sort_nowmode.dat[0] == SORT_COLUMN_UP ){
						setflag(hdri.fmt, HDF_SORTUP);
					}else if ( cinfo->sort_nowmode.dat[0] == SORT_COLUMN_DOWN){
						setflag(hdri.fmt, HDF_SORTDOWN);
					}
				}
				hdri.lParam =
					TMAKELPARAM( (0x8000 | itemindex), fmt - cinfo->celF.fmt);
				break;
			}

			case DE_MODULE:
				hdri.cxy = width * *(fmt + 1);
				break;

			case DE_NEWLINE:
				return;

			default:
				hdri.cxy = 0;
				break;
		}
		fmt += GetDispFormatSkip(fmt);
		if ( hdri.cxy ){
			if ( UseCheckBox && (hdri.cxy > (width * 4)) ){
				setflag(hdri.fmt, HDF_CHECKBOX);
				if ( cinfo->e.markC ) setflag(hdri.fmt, HDF_CHECKED);
				UseCheckBox = FALSE;
			}
			SendMessage(cinfo->hHeaderWnd,
					HDM_INSERTITEM, index++, (LPARAM)&hdri);
		}
	}
}

void CalcClickWidth(PPC_APPINFO *cinfo)
{
	BYTE *fmt;
	int width = 0;
	int tmpfontX = cinfo->fontX;

	fmt = cinfo->celF.fmt;
	while( *fmt ){
		switch( *fmt ){
			case DE_IMAGE:	// 画像
				width += *(fmt + 1);
				break;

			case DE_ICON:	// アイコン
			case DE_CHECK:		// チェック
			case DE_CHECKBOX:	// チェックボックス
				width += 2 * tmpfontX;
				break;

			case DE_MARK:	// マーク
			case DE_sepline: // 区切り線
				width += tmpfontX;
				break;

			case DE_SPC:	// 空白
			case DE_BLANK:	// 空白
			case DE_MODULE:
			case DE_MEMOEX:
				width += *(fmt + 1) * tmpfontX;
				break;

			case DE_LFN:
			case DE_SFN:
			case DE_LFN_MUL:
			case DE_LFN_LMUL:
			case DE_LFN_EXT:
			case DE_SFN_EXT:
			{
				int filew, extw;

				filew = *(fmt + 1);
				extw = *(fmt + 2);
				if ( extw >= 0xff ) extw = 0;
				width += (filew + extw) * tmpfontX;
				if ( filew ){
					if ( (DWORD)width < cinfo->CellNameWidth ){
						cinfo->CellNameWidth = width;
					}
					return;
				}
				break;
			}
			case DE_itemname:
			case DE_string:
			case DE_ivalue:
				fmt += GetDispFormatSkip_string(fmt);
				continue;

			case DE_TIME2:
				fmt += GetDispFormatSkip_time2(fmt);
				continue;

			case DE_COLUMN:
				width += *(fmt + 1 + sizeof(DWORD) + sizeof(BYTE));
				fmt += GetDispFormatSkip_column(fmt);
				continue;

			case DE_NEWLINE: // ファイル名がないまま行末に到達→次の行検索
				width = 0;
				break;

//			default: // 未使用
//				XMessage(NULL, NULL, XM_DbgDIA, T("%x %d"), fmt, *fmt);
		}
		{
			int skipsize;

			skipsize = DispFormatSkipTable[*fmt];
			if ( skipsize <= 0 ) break;
			fmt += skipsize;
		}
	}
}

// windowwidth の桁数に応じて DE_WIDEW の調整を行う(ステータス行、情報行用)
void FixInfoWideW(XC_CFMT *cfmt, int windowwidthLen)
{
	BYTE *fmt;

	fmt = cfmt->fmt;
	for ( ; ; ){
		if ( *fmt == DE_WIDEW ){
			int nameLen, cellminLen;

			cellminLen = cfmt->width - fmt[fmt[3]];
			nameLen = windowwidthLen - cellminLen;

			if ( nameLen < fmt[1] ) nameLen = fmt[1];
			if ( nameLen > 255 ) nameLen = 255;

			cfmt->width = cellminLen + nameLen;
			fmt[fmt[3]] = (BYTE)nameLen;
		}
		if ( *(WORD *)(fmt - 2) == 0 ) break;
		fmt += *(WORD *)(fmt - 2); // 次の行をチェックする
	}
}

// 窓枠内に収まるようにエントリ表示の桁数を調整する
void FixEntryWideW(PPC_APPINFO *cinfo, int fixPix, int clientPix)
{
	BYTE *fmt;
	int tmpfontX = cinfo->fontX;

	fmt = cinfo->celF.fmt;
	for ( ; ; ){
		if ( *fmt == DE_WIDEW ){
			int AllPix, nameLen, minLen;

			AllPix = clientPix + tmpfontX; // 全体の幅 pixel
			minLen = cinfo->celF.width - fmt[fmt[3]]; // ファイル名幅(拡張子除く)を除いた桁数
			if ( fmt[2] < 255 ){ // 最大値が 255未満の時は分割数を調整
				int VAreaXmax, VAreaXmin;

				// 最大桁数のときの分割数
				VAreaXmax = clientPix / (((minLen + fmt[2]) * tmpfontX) + fixPix);
				// 最小桁数のときの分割数
				VAreaXmin = clientPix / (((minLen + fmt[1]) * tmpfontX) + fixPix);
				// 最小桁数分割数が最大の時よりも多いなら、分割数を増やせば
				// 隙間が埋まる。※ぴったり埋まる時はこのままだと余計に
				// 分割するので AllPix - tmpfontX を AllPix2 として採用
				if ( VAreaXmin > VAreaXmax ) VAreaXmax++;
				if ( VAreaXmax <= 0 ) VAreaXmax = 1;
				AllPix = AllPix / VAreaXmax;
			}
			nameLen = (AllPix - fixPix) / tmpfontX - minLen;
			if ( nameLen > 255 ) nameLen = 255;
			if ( nameLen < fmt[1] ) nameLen = fmt[1]; // 下限チェック
			cinfo->celF.width = minLen + nameLen;
			cinfo->cel.Size.cx = (minLen + nameLen) * tmpfontX + fixPix;
			fmt[fmt[3]] = (BYTE)nameLen;
		}
		if ( *(WORD *)(fmt - 2) == 0 ) break;
		fmt += *(WORD *)(fmt - 2); // 次の行をチェックする
	}
	if ( cinfo->hHeaderWnd != NULL ) FixHeader(cinfo);
}

void SetScrollBar(PPC_APPINFO *cinfo, int mode)
{
	SCROLLINFO sinfo;
										// スクロールバー設定 -----------------
	cinfo->winOmax = (cinfo->e.cellIMax - 1) / cinfo->cel.Area.cy;
	if ( (cinfo->winOmax < 0) || (cinfo->cel.Area.cx > cinfo->winOmax) ){
		cinfo->winOmax = 0;	// １画面に納まる
	}else{
		if ( cinfo->cel.Area.cx > 2 ) cinfo->winOmax -= cinfo->cel.Area.cx - 2;
	}
	sinfo.nPage	= cinfo->cel.Area.cx * cinfo->cel.Area.cy;
	sinfo.nPos	= cinfo->cellWMin;
	if ( mode ){
		sinfo.nMax = cinfo->e.cellIMax - 1;
	}else{
		sinfo.nMax = (cinfo->winOmax + ((cinfo->cel.Area.cx < 2) ?
					1 : cinfo->cel.Area.cx - 1)) * cinfo->cel.Area.cy - 1;
	}

	if ( (sinfo.nPage != cinfo->oldspos.nPage) ||
		 (sinfo.nPos != cinfo->oldspos.nPos) ||
		 (sinfo.nMax != cinfo->oldspos.nMax) ){
		int bar;

		if ( cinfo->hScrollBarWnd != NULL ){
			if ( (sinfo.nPage != cinfo->oldspos.nPage) ||
				 (sinfo.nMax != cinfo->oldspos.nMax) ){
				ShowWindow(cinfo->hScrollBarWnd,
					// ※sinfo.nMax は通常正の範囲
					( (sinfo.nPage > (UINT)sinfo.nMax) ||
					 (cinfo->X_win & XWIN_HIDESCROLL)) ?
					SW_HIDE : SW_SHOWNOACTIVATE);
			}
		}

		cinfo->oldspos.nPage = sinfo.nPage;
		cinfo->oldspos.nPos  = sinfo.nPos;
		cinfo->oldspos.nMax  = sinfo.nMax;
		if ( cinfo->X_win & XWIN_HIDESCROLL ) return;

		sinfo.cbSize = sizeof(sinfo);
		sinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		sinfo.nMin = 0;

		if ( cinfo->hScrollBarWnd != NULL ){
			bar = SB_CTL;
		}else{
			bar = cinfo->ScrollBarHV;
		}
		SetScrollInfo(cinfo->hScrollTargetWnd, bar, &sinfo, TRUE);
	}
}

/*=============================================================================
 InitCli - クライアント領域の情報再設定
=============================================================================*/
void InitCli(PPC_APPINFO *cinfo)
{
	int clientPix, fixX = 0, tmpfontX = cinfo->fontX;
	int iconsize;

	if ( TouchMode & TOUCH_LARGEHEIGHT ){
		int TouchHeight;
		int orgY = cinfo->fontY - cinfo->X_lspc;

		TouchHeight = (cinfo->FontDPI * 60) >> 8; // 6.0mm ( 60 / 254 ) の近似値
		// iOS:7mm Android:9mm(7-10mm)
		if ( (cinfo->fontY * cinfo->celF.height) < TouchHeight ){
			cinfo->fontY = (TouchHeight + cinfo->celF.height - 1) / cinfo->celF.height;
			cinfo->X_lspc = cinfo->fontY - orgY;
		}else if ( cinfo->X_lspc > cinfo->X_lspcOrg ){
			if ( ((orgY + cinfo->X_lspcOrg) * cinfo->celF.height) >= TouchHeight ){
				cinfo->X_lspc = cinfo->X_lspcOrg;
				cinfo->fontY = orgY + cinfo->X_lspc;
			}
		}
	}

	cinfo->DrawTargetFlags = DRAWT_ALL;
										// 各表示領域を区分けする -------------
	// キャプション／ラベル
	cinfo->BoxStatus.left	= 0;
	cinfo->BoxStatus.top	= (cinfo->combo && !(X_combos[0] & CMBS_NOCAPTION)) ? cinfo->fontY : cinfo->docks.t.client.bottom;
	// ステータス行
	cinfo->BoxStatus.right	= cinfo->wnd.Area.cx;
	cinfo->BoxStatus.bottom	=
		 ((cinfo->X_win & XWIN_NOSTATUS) ||
		  (cinfo->docks.t.hStatusWnd != NULL) ||
		  (cinfo->docks.b.hStatusWnd != NULL))
			? cinfo->BoxStatus.top :
			cinfo->BoxStatus.top + cinfo->fontY * cinfo->stat.height;
	if ( cinfo->stat.attr & DE_ATTR_WIDEW ){
		FixInfoWideW(&cinfo->stat, (cinfo->BoxStatus.right - cinfo->BoxStatus.left) / tmpfontX + 1);
	}
	// 情報行
	cinfo->BoxInfo.left		= 0;
	cinfo->BoxInfo.right	= cinfo->BoxStatus.right;
	cinfo->BoxInfo.top		= cinfo->BoxStatus.bottom;
	if ( (cinfo->combo && (X_combos[0] & CMBS_COMMONINFO)) ||
		 (cinfo->X_win & XWIN_NOINFO) ||
		 (cinfo->docks.t.hInfoWnd != NULL) ||
		 (cinfo->docks.b.hInfoWnd != NULL) ){
		cinfo->BoxInfo.bottom = cinfo->BoxInfo.top;
	}else{
		int height = cinfo->fontY * (cinfo->inf1.height + cinfo->inf2.height);

		if ( height < (cinfo->XC_ifix_size.cy + 1) ){
			height = cinfo->XC_ifix_size.cy + 1;
		}
		cinfo->BoxInfo.bottom = cinfo->BoxInfo.top + height;

		if ( cinfo->inf1.attr & DE_ATTR_WIDEW ){
			FixInfoWideW(&cinfo->inf1, (cinfo->BoxInfo.right - cinfo->BoxInfo.left - cinfo->iconR) / tmpfontX + 1);
		}
		if ( cinfo->inf2.attr & DE_ATTR_WIDEW ){
			FixInfoWideW(&cinfo->inf2, (cinfo->BoxInfo.right - cinfo->BoxInfo.left - cinfo->iconR) / tmpfontX + 1);
		}
	}
	// エントリ行など
	cinfo->BoxEntries.left	= cinfo->TreeX;
	cinfo->BoxEntries.top	= cinfo->BoxInfo.bottom;
	cinfo->BoxEntries.right	= cinfo->BoxStatus.right;
	cinfo->BoxEntries.bottom= cinfo->wnd.Area.cy - cinfo->docks.b.client.bottom;

	if ( cinfo->hToolBarWnd != NULL ){
		cinfo->BoxEntries.top += cinfo->ToolbarHeight;
	}
	cinfo->BoxEntries.top += cinfo->HeaderHeight;

	if ( cinfo->hScrollBarWnd != NULL ){
		if ( cinfo->ScrollBarHV == SB_HORZ ){ // 水平
			cinfo->ScrollBarSize = GetSystemMetrics(SM_CYHSCROLL);
			cinfo->BoxEntries.bottom -= cinfo->ScrollBarSize;
			if ( cinfo->BoxEntries.bottom < cinfo->BoxEntries.top ){
				cinfo->BoxEntries.bottom = cinfo->BoxEntries.top;
			}
		}else{ // 垂直
			cinfo->ScrollBarSize = GetSystemMetrics(SM_CXVSCROLL);
			cinfo->BoxEntries.right -= cinfo->ScrollBarSize;
			if ( cinfo->BoxEntries.right < cinfo->BoxEntries.left ){
				cinfo->BoxEntries.right = cinfo->BoxEntries.left;
			}
		}
		cinfo->ScrollBarY = cinfo->BoxEntries.bottom;
	}

	cinfo->cel.Size.cx = tmpfontX * cinfo->celF.width;
	cinfo->cel.Size.cy = cinfo->fontY * cinfo->celF.height;

	if ( UsePFont ){ // 可変長補正
		SIZE boxsize;
		BYTE *fmt;
		HDC hDC = GetDC(cinfo->info.hWnd);
		HGDIOBJ hOldFont;

		hOldFont = SelectObject(hDC, cinfo->hBoxFont);
		fmt = cinfo->celF.fmt;
		for ( ;; ){
			BYTE fmtdata;

			fmtdata = *fmt;
			if ( (fmtdata == DE_END) || (fmtdata == DE_NEWLINE) ) break;
			if ( fmtdata == DE_TIME1 ){
				BYTE timelen = *(fmt + 1);

				GetTextExtentPoint32(hDC, T("00-00-00 00:00:00.000"),
						timelen, &boxsize);
				fixX = boxsize.cx - (tmpfontX * timelen);
				cinfo->cel.Size.cx += fixX;
			}
			fmt += GetDispFormatSkip(fmt);
		}
		SelectObject(hDC, hOldFont);	// フォント
		ReleaseDC(cinfo->info.hWnd, hDC);
	}

	{ // cinfo->cel.Size.cy を決定
		BYTE *fmt;

		fmt = cinfo->celF.fmt;
		if ( (*fmt == DE_WIDEV) || (*fmt == DE_WIDEW) ) fmt += DE_WIDEV_SIZE;
		if ( *fmt == DE_IMAGE ){
#if 0 // 丁寧な行数決定
			int imgY;

			imgY = cinfo->fontY * fmt[2];
			if ( cinfo->cel.Size.cy < imgY ) cinfo->cel.Size.cy = imgY;
#else
			cinfo->cel.Size.cy = cinfo->fontY * fmt[2];
#endif
		}else if ( *fmt == DE_ICON2 ){ // アイコン2 の幅補正
			int iconLen, iconFix;

			iconsize = fmt[1];
			if ( cinfo->X_textmag != 100 ){
				iconsize = (iconsize * cinfo->X_textmag) / 100;
			}
			if ( cinfo->FontDPI != DEFAULT_WIN_DPI ){
				iconsize = (iconsize * cinfo->FontDPI) / DEFAULT_WIN_DPI;
			}

			iconsize += ICONBLANK;
			if ( cinfo->cel.Size.cy < iconsize ) cinfo->cel.Size.cy = iconsize;

			iconLen = GetIcon2Len(fmt[1]);
			iconFix = iconsize - iconLen * tmpfontX;
			cinfo->cel.Size.cx += iconFix;
			fixX += iconFix;
		}
	}
	if ( cinfo->cel.Size.cy <= 0 ) cinfo->cel.Size.cy = 1;
										// 窓の幅を取得 -----------------------
	clientPix = cinfo->BoxEntries.right - cinfo->BoxEntries.left;
	if ( clientPix <= 0 ){
		cinfo->cel.VArea.cx = cinfo->cel.Area.cx = 1;
	}else{
		if ( cinfo->celF.attr & DE_ATTR_WIDEW ){ // 窓の幅に合わせて桁数調整
			FixEntryWideW(cinfo, fixX, clientPix);
		}
		if ( cinfo->cel.Size.cx <= 0 ) cinfo->cel.Size.cx = 1;
		cinfo->cel.VArea.cx = (clientPix - 1) / cinfo->cel.Size.cx + 1;
		cinfo->cel.Area.cx = (clientPix + tmpfontX) / cinfo->cel.Size.cx;
		if ( cinfo->cel.Area.cx <= 0 ) cinfo->cel.Area.cx = 1;
	}
										// セルの行数を決定 -------------------
	{
		int y;

		y = (cinfo->BoxEntries.bottom - cinfo->BoxEntries.top) /
				cinfo->cel.Size.cy;
		if ( y <= 0 ) y = 1;
		cinfo->BoxEntries.bottom =
				cinfo->BoxEntries.top + cinfo->cel.Size.cy * y;
		if ( y != cinfo->cel.Area.cy ){
			cinfo->cel.Area.cy = cinfo->cel.VArea.cy = y;
			InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
		}
	}
										// クリック反応幅を決定 ---------------
	cinfo->CellNameWidth = cinfo->cel.Size.cx - tmpfontX - 3;
	if ( XC_limc ) CalcClickWidth(cinfo);
	SetScrollBar(cinfo, XC_page);
}

// アクティブになった窓の位置関係や大きさを調節する ---------------------------
void FixTwinWindow(PPC_APPINFO *cinfo)
{
	int Oldswin;
	HWND PairHWnd;

	if ( !(cinfo->swin & (SWIN_WBOOT | SWIN_FIXACTIVE)) ) return;
	if ( !CheckReady(cinfo) ) return;

	PairHWnd = GetJoinWnd(cinfo);
	if ( PairHWnd == NULL ) return;

	Oldswin = cinfo->swin;
					// フォーカスを次回起動時/アクティブ位置調整のために保存
	if ( cinfo->RegID[2] & 1 ){
		resetflag(cinfo->swin, SWIN_BFOCUES);
	}else{
		setflag(cinfo->swin, SWIN_BFOCUES);
	}
	if ( Oldswin == cinfo->swin ) return;	// 前からアクティブなら何もしない
	SendX_win(cinfo);

	if ( cinfo->swin & SWIN_FIXACTIVE ){	// アクティブになったので位置調整
		DWORD flag;
		WINDOWPLACEMENT owp;
		WINDOWPLACEMENT wp;
		HDWP hdWins;

		flag = (cinfo->swin & SWIN_BFOCUES) | (cinfo->RegID[2] & PAIRBIT);
		if ( !((flag == 0) || (flag == (PAIRBIT | SWIN_BFOCUES))) ){ // 入替有
			owp.length = sizeof(owp);
			wp.length = sizeof(wp);
			GetWindowPlacement(cinfo->info.hWnd, &wp);
			GetWindowPlacement(PairHWnd, &owp);
			GetWindowRect(cinfo->info.hWnd, &wp.rcNormalPosition);
			GetWindowRect(PairHWnd, &owp.rcNormalPosition);

			if ( (owp.showCmd == SW_SHOWNORMAL) &&
				  (wp.showCmd == SW_SHOWNORMAL) ){
				wp.rcNormalPosition.right	-= wp.rcNormalPosition.left;
				wp.rcNormalPosition.bottom	-= wp.rcNormalPosition.top;
				owp.rcNormalPosition.right	-= owp.rcNormalPosition.left;
				owp.rcNormalPosition.bottom	-= owp.rcNormalPosition.top;

				if (cinfo->swin & SWIN_FIXPOSIZ){ // 大きさ+位置 --------------
					if (cinfo->swin & SWIN_JOIN){	// Joint 中なら位置交換
						cinfo->swin ^= SWIN_SWAPMODE;
					}
					SendX_win(cinfo);
					hdWins = BeginDeferWindowPos(2);
					// 自分の大きさを変更
					hdWins = DeferWindowPos(hdWins, cinfo->info.hWnd, NULL,
							owp.rcNormalPosition.left,
							owp.rcNormalPosition.top,
							owp.rcNormalPosition.right,
							owp.rcNormalPosition.bottom,
							SWP_NOACTIVATE | SWP_NOZORDER);
					// 相手の大きさを変更
					hdWins = DeferWindowPos(hdWins, PairHWnd, NULL,
							wp.rcNormalPosition.left,
							wp.rcNormalPosition.top,
							wp.rcNormalPosition.right,
							wp.rcNormalPosition.bottom,
							SWP_NOACTIVATE | SWP_NOZORDER);
					if ( hdWins != NULL ) EndDeferWindowPos(hdWins); // ←ここでSendMessageの受付
				}else{	// 大きさ ------------------------------------------
					SendX_win(cinfo);

					// 左右/上下 Joint中なら位置調整
					if ((cinfo->swin & (SWIN_JOIN | SWIN_PILEJOIN)) == SWIN_JOIN  ){
						int site;

						site = cinfo->RegID[2];
						if (cinfo->swin & SWIN_SWAPMODE) site ^= PAIRBIT;

						if (cinfo->swin & SWIN_UDJOIN){	// 上
							if (site & PAIRBIT){		// 基準が(上)
								owp.rcNormalPosition.top =
										wp.rcNormalPosition.top +
										owp.rcNormalPosition.bottom;
							}else{				// 基準が(下)
								wp.rcNormalPosition.top =
										owp.rcNormalPosition.top +
										wp.rcNormalPosition.bottom;
							}
						}else{				// 左
							if (site & PAIRBIT){		// 基準が(左)
								owp.rcNormalPosition.left =
										wp.rcNormalPosition.left +
										owp.rcNormalPosition.right;
							}else{				// 基準が(右)
								wp.rcNormalPosition.left =
										owp.rcNormalPosition.left +
										wp.rcNormalPosition.right;
							}
						}
					}

					hdWins = BeginDeferWindowPos(2);
						// 自分の大きさを変更
					hdWins = DeferWindowPos(hdWins, cinfo->info.hWnd, NULL,
							wp.rcNormalPosition.left,
							wp.rcNormalPosition.top,
							owp.rcNormalPosition.right,
							owp.rcNormalPosition.bottom,
							SWP_NOACTIVATE | SWP_NOZORDER);
					// 相手の大きさを変更
					hdWins = DeferWindowPos(hdWins, PairHWnd, NULL,
							owp.rcNormalPosition.left,
							owp.rcNormalPosition.top,
							wp.rcNormalPosition.right,
							wp.rcNormalPosition.bottom,
							SWP_NOACTIVATE | SWP_NOZORDER);
					if ( hdWins != NULL ) EndDeferWindowPos(hdWins); // ←ここでSendMessageの受付
				}
			}
		}
	}
}
void BootPairPPc(PPC_APPINFO *cinfo)
{
	TCHAR param[] = T("/bootid:A");

	if ( PPcGetWindow(cinfo->RegNo, CGETW_PAIR) != NULL ) return;
	param[8] = (TCHAR)(((cinfo->RegID[2] - 1) ^ 1) + 1);
	PPCui(cinfo->info.hWnd, param);
}

/*-----------------------------------------------------------------------------
	組みとなる窓の連結処理
	src		基準
	dest	結合対象
	dir
-----------------------------------------------------------------------------*/
void JoinWindow(PPC_APPINFO *cinfo)
{
	HWND PairHWnd;

	if ( cinfo->combo || !CheckReady(cinfo)) return;
	PairHWnd = GetJoinWnd(cinfo);
	if ( PairHWnd != NULL ){				// 反対窓が存在する→連結処理 -----
		PostMessage(PairHWnd, WM_PPXCOMMAND, KC_DoJW, (LPARAM)cinfo->info.hWnd);
	}
}

#if !NODLL
DefineWinAPI(HRESULT, DwmGetWindowAttribute, (HWND, DWORD, PVOID, DWORD)) = NULL;
#else
ExternWinAPI(HRESULT, DwmGetWindowAttribute, (HWND, DWORD, PVOID, DWORD));
#endif

void JointWindowMain(PPC_APPINFO *cinfo, HWND PairHWnd)
{
	WINDOWPLACEMENT nwp, owp;
	int fixX = 0, fixY = 0;

	nwp.length = sizeof(nwp);
	owp.length = sizeof(owp);
	GetWindowPlacement(cinfo->info.hWnd, &nwp);
	GetWindowPlacement(PairHWnd, &owp);
	GetWindowRect(cinfo->info.hWnd, &nwp.rcNormalPosition);

	if ( DDwmGetWindowAttribute == NULL ){
		if ( hDwmapi != NULL ){
			GETDLLPROC(hDwmapi, DwmGetWindowAttribute);
		}
		if ( DDwmGetWindowAttribute == NULL ){
			DDwmGetWindowAttribute = INVALID_VALUE(impDwmGetWindowAttribute);
		}
	}
	if ( (DDwmGetWindowAttribute != INVALID_VALUE(impDwmGetWindowAttribute)) &&
		SUCCEEDED(DDwmGetWindowAttribute(cinfo->info.hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &owp.rcNormalPosition, sizeof(RECT))) ){
		if ( owp.rcNormalPosition.left > nwp.rcNormalPosition.left ){ // 実体の枠がウィンドウの枠より小さい（Windows8/10）
			fixX = (nwp.rcNormalPosition.left - owp.rcNormalPosition.left) +
				  -(nwp.rcNormalPosition.right - owp.rcNormalPosition.right);
			fixY = (nwp.rcNormalPosition.top - owp.rcNormalPosition.top) +
				  -(nwp.rcNormalPosition.bottom - owp.rcNormalPosition.bottom);
		}
	}

	GetWindowRect(PairHWnd, &owp.rcNormalPosition);

											// 最小化状態等を一致させる -------
	if ( !(cinfo->swin & SWIN_BUSY) && (nwp.showCmd != owp.showCmd) ){
		setflag(cinfo->swin, SWIN_BUSY);
		IOX_win(cinfo, TRUE);
		ShowWindow(cinfo->info.hWnd, owp.showCmd);
		GetWindowPlacement(cinfo->info.hWnd, &nwp);
		GetWindowRect(cinfo->info.hWnd, &nwp.rcNormalPosition);
		resetflag(cinfo->swin, SWIN_BUSY);
		IOX_win(cinfo, TRUE);
		SetForegroundWindow( PairHWnd );
	}
											// 位置情報の作成(通常表示なら) ---
	if ( owp.showCmd == SW_SHOWNORMAL ){
		RECT NewRect;
		int width, height, update = 0;

		NewRect.right = nwp.rcNormalPosition.right -
						nwp.rcNormalPosition.left;
		NewRect.bottom= nwp.rcNormalPosition.bottom -
						nwp.rcNormalPosition.top;
		width 	=	owp.rcNormalPosition.right -
					owp.rcNormalPosition.left;
		height	=	owp.rcNormalPosition.bottom -
					owp.rcNormalPosition.top;

		if ( cinfo->swin & SWIN_PILEJOIN ){	// 重ねる
			NewRect.left= owp.rcNormalPosition.left;
			NewRect.top	= owp.rcNormalPosition.top;
			if ( (cinfo->swin & SWIN_FIT) && ((NewRect.right != width) ||
								   (NewRect.bottom != height)) ){
				update = 1;
				NewRect.right = width;
				NewRect.bottom= height;
			}
		}else{				// 上下／左右
			int site;

			site = cinfo->RegID[2];
			if ( cinfo->swin & SWIN_SWAPMODE ) site ^= PAIRBIT;

			if ( cinfo->swin & SWIN_UDJOIN ){	// 上
				if ( site & PAIRBIT ){		// 基準が(上)
					NewRect.left= owp.rcNormalPosition.left;
					NewRect.top	= owp.rcNormalPosition.top - NewRect.bottom - fixY;
				}else{				// 基準が(下)
					NewRect.left= owp.rcNormalPosition.left;
					NewRect.top	= owp.rcNormalPosition.bottom + fixY;
				}
				if ((cinfo->swin & SWIN_FIT) && (NewRect.right != width) ){
					update = 1;
					NewRect.right = width;
				}
			}else{				// 左
				if ( site & PAIRBIT ){		// 基準が(左)
					NewRect.left= owp.rcNormalPosition.left - NewRect.right - fixX;
					NewRect.top	= owp.rcNormalPosition.top;
				}else{				// 基準が(右)
					NewRect.left= owp.rcNormalPosition.right + fixX;
					NewRect.top	= owp.rcNormalPosition.top;
				}
				if ( (cinfo->swin & SWIN_FIT) && (NewRect.bottom != height) ){
					update = 1;
					NewRect.bottom= height;
				}
			}
		}

		if ( (NewRect.left != nwp.rcNormalPosition.left) ||
			 (NewRect.top  != nwp.rcNormalPosition.top) ||
			 update){

			MoveWindow(cinfo->info.hWnd, NewRect.left , NewRect.top,
								NewRect.right, NewRect.bottom, TRUE);
		}
	}
}

// １セル内にエントリ名が全て表示できる桁数を算出する ※１行目のみ機能する
void FixCellWideV(PPC_APPINFO *cinfo)
{
	int MaxNameLength;
	ENTRYINDEX i;
	ENTRYCELL *cell;
	ENTRYDATAOFFSET *tbl;

	MaxNameLength = 0;
	tbl = &CELt(0);
	#ifndef UNICODE
	if ( IsTrue(UsePFont) )
	#endif						// 不等幅フォント/UNICODE版の処理 -------------
	{
		HDC hDC;
		HGDIOBJ hOldFont;
		SIZE textsize;
		int MaxPix = 0;

		hDC = GetDC(cinfo->info.hWnd);
		hOldFont = SelectObject(hDC, cinfo->hBoxFont);
												//拡張子はファイル名と一体
		if ( cinfo->celF.fmt[cinfo->celF.fmt[3] + 1] == 0xff ){
			for ( i = cinfo->e.cellIMax ; i ; i--, tbl++ ){
				int fulllen;

				cell = &CELdata(*tbl);
				fulllen = cell->ext + tstrlen32(cell->f.cFileName + cell->ext);
				if ( MaxNameLength < fulllen ) MaxNameLength = fulllen;

				GetTextExtentPoint32(hDC, cell->f.cFileName, fulllen, &textsize);
				if ( MaxPix < textsize.cx ) MaxPix = textsize.cx;
			}
		}else{
			for ( i = cinfo->e.cellIMax ; i ; i--, tbl++ ){
				cell = &CELdata(*tbl);
				if ( MaxNameLength < cell->ext ) MaxNameLength = cell->ext;

				GetTextExtentPoint32(hDC, cell->f.cFileName, cell->ext, &textsize);
				if ( MaxPix < textsize.cx ) MaxPix = textsize.cx;
			}
		}
		SelectObject(hDC, hOldFont);
		ReleaseDC(cinfo->info.hWnd, hDC);
		MaxPix = (MaxPix + cinfo->fontX - 1) / cinfo->fontX; // pix→桁変換
		if ( MaxNameLength < MaxPix ) MaxNameLength = MaxPix;
	#ifndef UNICODE
	}else{
												//拡張子はファイル名と一体
		if ( cinfo->celF.fmt[cinfo->celF.fmt[3] + 1] == 0xff ){
			for ( i = cinfo->e.cellIMax ; i ; i--, tbl++ ){
				int fulllen;

				cell = &CELdata(*tbl);
				fulllen = cell->ext + tstrlen32(cell->f.cFileName + cell->ext);
				if ( MaxNameLength < fulllen ) MaxNameLength = fulllen;
			}
		}else{
			for ( i = cinfo->e.cellIMax ; i ; i--, tbl++ ){
				cell = &CELdata(*tbl);
				if ( MaxNameLength < cell->ext ) MaxNameLength = cell->ext;
			}
		}
	#endif
	}

	if ( MaxNameLength < cinfo->celF.fmt[1] ){
		MaxNameLength = cinfo->celF.fmt[1];
	}else if ( MaxNameLength > cinfo->celF.fmt[2] ){
		MaxNameLength = cinfo->celF.fmt[2];
	}
	cinfo->celF.width += -cinfo->celF.fmt[cinfo->celF.fmt[3]] + MaxNameLength;
	cinfo->celF.fmt[cinfo->celF.fmt[3]] = (BYTE)MaxNameLength;
}
