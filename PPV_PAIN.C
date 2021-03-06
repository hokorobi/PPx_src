/*-----------------------------------------------------------------------------
	Paper Plane vUI													Paint
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

DWORD countbits(DWORD bits)
{
	bits = bits - ((bits >> 1) & 0x55555555);
	bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
	bits = (bits + (bits >> 4)) & 0x0f0f0f0f;
	bits = bits + (bits >> 8);
	bits = bits + (bits >> 16);
	return bits & 0x3f;
}

const TCHAR *GetColorInfo(BITMAPINFOHEADER *bmpinfo, TCHAR *buf)
{
	int r, g, b, a;

	if ( bmpinfo->biCompression != BI_BITFIELDS ){
		wsprintf(buf, T("%dbits"), bmpinfo->biBitCount);
		return buf;
	}

	r = countbits(*(DWORD *)(BYTE *)((BYTE *)bmpinfo + sizeof(BITMAPINFOHEADER)));
	g = countbits(*(DWORD *)(BYTE *)((BYTE *)bmpinfo + sizeof(BITMAPINFOHEADER) + sizeof(DWORD) ));
	b = countbits(*(DWORD *)(BYTE *)((BYTE *)bmpinfo + sizeof(BITMAPINFOHEADER) + sizeof(DWORD) * 2));
	if ( bmpinfo->biSize >= (sizeof(BITMAPINFOHEADER) + sizeof(DWORD) * 4) ){
		a = countbits(*(DWORD *)(BYTE *)((BYTE *)bmpinfo + sizeof(BITMAPINFOHEADER) + sizeof(DWORD) * 3));
		wsprintf(buf, T("%d(R%dG%dB%dA%d)"), bmpinfo->biBitCount, r, g, b, a);
	}else{
		wsprintf(buf, T("%d(R%dG%dB%d)"), bmpinfo->biBitCount, r, g, b);
	}
	return buf;
}

void DrawEMetafile(PPVPAINTSTRUCT *pps, PPvViewObject *vo)
{
	HPALETTE hOldPal;
	RECT box;

	if ( pps->ps.fErase != FALSE ){			// 背景表示
		box = pps->ps.rcPaint;
		if ( box.top < pps->view.top ) box.top = pps->view.top;
		DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
	}
	if ( vo->bitmap.hPal != NULL ){
		hOldPal = SelectPalette(pps->ps.hdc, vo->bitmap.hPal, FALSE);
								// 論理パレットを現在のＤＣにマッピングする。
		RealizePalette(pps->ps.hdc);
	}
	box.left = -VOi->offX * fontX;
	box.top = pps->view.top - VOi->offY * LineY;
	if ( (XV.img.MagMode == IMGD_MM_FULLSCALE) && (XV.img.AspectRate == 0) ){ // 等倍表示 --------

		box.right = box.left + vo->bitmap.showsize.cx;
		box.bottom = box.top + vo->bitmap.showsize.cy;
	}else{
#if 0
		XFORM xf;

		SetGraphicsMode(pps->ps.hdc, GM_ADVANCED);
		xf.eM11 = xf.eM12 = XV.img.imgD[0] / 100.0;
		xf.eM21 = xf.eM22 = xf.eDx = xf.eDy = 0.0;
		SetWorldTransform(pps.ps.hdc, &xf);

		xf.eM11 = xf.eM12 = 1.0;
		SetWorldTransform(pps->ps.hdc, &xf);
		SetGraphicsMode(pps->ps.hdc, GM_COMPATIBLE);
#endif
		int lwidth, lheight;

		if ( XV.img.MagMode == IMGD_MM_FRAME ){ // 窓枠の大きさ
			lwidth = WndSize.cx;
			lheight = pps->view.bottom - pps->view.top;
			if ( ((lwidth << 15)  / vo->bitmap.ShowInfo->biWidth) >
				 ((lheight << 15) / vo->bitmap.ShowInfo->biHeight) ){
				lwidth = (vo->bitmap.ShowInfo->biWidth * lheight) / vo->bitmap.ShowInfo->biHeight;
			}else{
				lheight = (vo->bitmap.ShowInfo->biHeight * lwidth) / vo->bitmap.ShowInfo->biWidth;
			}
		}else{ // 固定倍率 ---------------------------------------
			lwidth = VO_maxX;
			lheight = VO_maxY;
		}
		if ( lwidth == 0 ) lwidth = 1;
		if ( lheight == 0 ) lheight = 1;

		box.left += (lwidth >= pps->view.right - pps->view.left) ?
				pps->view.left :
				((pps->view.left + pps->view.right) - lwidth) / 2;
		box.top += (lheight >= pps->view.bottom - pps->view.top) ?
			pps->view.top :
			((pps->view.bottom + pps->view.top) - lheight) / 2;

		box.right = box.left + lwidth;
		box.bottom = box.top + lheight;
	}
//XMessage(NULL, NULL, XM_DbgLOG, T("%d %d %d %d"), box.left, box.top, box.right, box.bottom);
	PlayEnhMetaFile(pps->ps.hdc, vo->eMetafile.handle, &box);

	if ( vo->bitmap.hPal != NULL ){
		SelectPalette(pps->ps.hdc, hOldPal, FALSE);
	}
}

// Win95/NT4 eng RTM は VTYPE_IBM / VTYPE_ANSI / VTYPE_SYSTEMCP のみ使用可能
UINT VVTypeToCPlist[VTypeToCPlist_max] = {
	CP__US,		// VTYPE_IBM
	CP__LATIN1,	// VTYPE_ANSI
	CP__JIS,	// VTYPE_JIS
	CP_ACP,		// VTYPE_SYSTEMCP
	CP__SJIS,	// VTYPE_EUCJP // ※ 殆ど CP__SJIS で処理するため
	CP__UTF16L,	// VTYPE_UNICODE
	CP__UTF16B,	// VTYPE_UNICODEB
	CP__SJIS,	// VTYPE_SJISNEC
	CP__SJIS,	// VTYPE_SJISB,
	CP__SJIS,	// VTYPE_KANA
	CP_UTF8,	// VTYPE_UTF8
	CP_UTF7,	// VTYPE_UTF7
};

void LongHex(TCHAR *buf, DDWORDST lsize)
{
	if ( lsize.HighPart != 0 ){
		wsprintf(buf, T("%x%08x"), lsize.HighPart, lsize.LowPart );
	}else{
		wsprintf(buf, T("%04x"), lsize.LowPart);
	}
}

void HexSize(TCHAR *buf, DDWORDST lsize)
{
	TCHAR *p;

	p = buf + tstrlen(buf);
	*p++ = '[';
	LongHex(p, lsize);
	p += tstrlen(p);
	*p++ = 'H';
	*p++ = ']';
	*p = '\0';
}

void DrawHex(PPVPAINTSTRUCT *pps, PPvViewObject *vo)
{
	RECT box;
	int x, y, of, hoff;
	TCHAR buf[100];
	WCHAR bufW[100];
	POINT LP;
	UINT codepage;

	if ( OSver.dwMajorVersion < 5 ){
		codepage = 0;
	}else{
		codepage = VOi->textC;
		switch ( codepage ){
			case VTYPE_JIS:
			case VTYPE_UNICODEB:
			case VTYPE_KANA:
			case VTYPE_UTF7:
				codepage = 0;
				break;

			default:
				codepage = (codepage < VTypeToCPlist_max) ? VVTypeToCPlist[codepage] : codepage;
				if ( codepage == CP_ACP ) codepage = 0;
				break;
		}
		if ( IsValidCodePage(codepage) == FALSE ) codepage = 0;
	}

	of = (pps->shift.cy + pps->drawYtop) * 16;
										// 背景表示 ===================
	if ( pps->ps.fErase != FALSE ){
		// アドレス文字列より左
		box.left   = pps->ps.rcPaint.left;
		box.right  = -pps->shift.cx * pps->fontsize.cx + pps->shiftdot.cx;
		box.top    = pps->shiftdot.cy;
		box.bottom = pps->ps.rcPaint.bottom;
		DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		// 文字列より右
		box.left   = box.right + HEXWIDTH * pps->fontsize.cx;
		box.right  = pps->ps.rcPaint.right;
		DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
	}
	DxSetTextAlign(pps->ps.hdc, TA_LEFT | TA_TOP | TA_UPDATECP); // CP を有効に
	pps->si.fg = CV_char[CV__deftext];
	pps->si.bg = CV_char[CV__defback];

	y = pps->drawYtop;
	LP.y = y * pps->lineY + pps->shiftdot.cy;
	for ( ; y <= pps->drawYbottom ; y++, LP.y += pps->lineY ){
		if ( (DWORD)of >= vo->file.UseSize ) break; // 空行

		box.top = LP.y;
		DxMoveToEx(DxDraw, pps->ps.hdc,
				-pps->shift.cx * pps->fontsize.cx + pps->shiftdot.cx, LP.y);

		// アドレス部分 -------------------------
		wsprintf(buf, T("%08X "), of + FileDividePointer.LowPart);
		if ( !pps->print ){
			DxSetTextColor(DxDraw, pps->ps.hdc, CV_lnum[0]);
			DxSetBkColor(DxDraw, pps->ps.hdc, C_back);
		}
		DxTextOutRel(DxDraw, pps->ps.hdc, buf, HEXADRWIDTH);
		if ( !pps->print ){
			DxSetTextColor(DxDraw, pps->ps.hdc, pps->si.fg);

			if ( VOsel.select || VOsel.highlight ){
				if ( ((VOsel.select != FALSE) &&
						  (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ) ||
					 ((y + pps->shift.cy) == VOsel.foundY)
				){
					if ( pps->si.bgmode ){
						DxSetBkMode(DxDraw, pps->ps.hdc, OPAQUE);
					}
					DxSetBkColor(DxDraw, pps->ps.hdc, C_info);
					DxSetTextColor(DxDraw, pps->ps.hdc, C_back);
				}else{
					if ( pps->si.bgmode ){
						DxSetBkMode(DxDraw, pps->ps.hdc, TRANSPARENT);
					}
					DxSetTextColor(DxDraw, pps->ps.hdc, CV_char[CV__deftext]);
					DxSetBkColor(DxDraw, pps->ps.hdc, CV_char[CV__defback]);
				}
			}
		}
										// 16進部分
		hoff = of;
		for ( x = 0 ; x < (HEXNUMSWIDTH - 1) ; x += HEXNWIDTH ){
			if ( (DWORD)of >= vo->file.UseSize ){
				tstrcpy(&buf[x], T("   "));
			}else{
				wsprintf(&buf[x], T("%02X "), *(vo->file.image + of));
				of++;
			}
			if ( x == HEXNWIDTH * 7 ){
				buf[x + 3] = ' ';
				x++;
			}
		}

		buf[HEXNUMSWIDTH - 1] = '\0';

		DrawSelectedText(pps->ps.hdc, &pps->si, buf, HEXNUMSWIDTH - 1, 0, y);

		if ( pps->ps.fErase != FALSE ){ // １６進-文字列間
			DxGetCurrentPositionEx(DxDraw, pps->ps.hdc, &LP);

			box.left   = LP.x;
			box.right   = (HEXADRWIDTH + HEXNUMSWIDTH - pps->shift.cx) *
					pps->fontsize.cx + pps->shiftdot.cx;
			box.bottom = box.top + pps->fontsize.cy;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
		// 文字列部分 ---------------------------
		{
			HFONT font;
			int len;

			len = of - hoff;
			font = pps->hBoxFont;

			if ( codepage == 0 ){
				if ( VOi->textC == VTYPE_IBM ){
					font = GetIBMFont();
					SetFontDxDraw(DxDraw, font, 1);
				}else if ( VOi->textC == VTYPE_ANSI ){
					font = GetANSIFont();
					SetFontDxDraw(DxDraw, font, 1);
				}else{
					DxSelectFont(DxDraw, 0);
				}
			}else{
				if ( (codepage == CP_UTF8) && len ){ // １文字途中の時は延長
					int extlen;

					extlen = 5;
					while ( (DWORD)(hoff + len) < vo->file.UseSize ){
						if ( (*(char *)(vo->file.image + hoff + len) & 0xc0) != 0x80 ){
							break;
						}
						len++;
						if ( --extlen == 0 ) break;
					}
				}
				DxSelectFont(DxDraw, 0);
			}
#if 0
			if ( (VOi->textC == VTYPE_SYSTEMCP) && len ){
				BYTE c;

				c = *(char *)(vo->file.image + hoff + len - 1);
				if ( c >= 0x80 ) len++;
			}
#endif
			IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc, font);
			if ( (pps->ps.fErase != FALSE) && (font != pps->hBoxFont) ){
				box.left   = box.right; // ※１６進-文字列間 で設定した内容
				box.right  = box.left + len * pps->fontsize.cx;
				// box.bottom は １６進-文字列間 で設定済み
				DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
			}
			DxMoveToEx(DxDraw, pps->ps.hdc,
					(HEXADRWIDTH + HEXNUMSWIDTH - pps->shift.cx) *
					pps->fontsize.cx + pps->shiftdot.cx, box.top);
			if ( VOi->textC == VTYPE_UNICODE ){
				DxTextOutRelW(DxDraw, pps->ps.hdc, (WCHAR *)(vo->file.image + hoff), len / 2);
			}else if ( codepage != 0 ){
				DWORD size;

				size = MultiByteToWideChar(codepage,
						(codepage == CP_UTF8) ? 0 : MB_PRECOMPOSED,
						(char *)(vo->file.image + hoff),
						len, bufW, 100);
				DxTextOutRelW(DxDraw, pps->ps.hdc, bufW, size);
			}else {
				DxTextOutRelA(DxDraw, pps->ps.hdc, (char *)vo->file.image + hoff, len);
			}

			if ( pps->ps.fErase != FALSE ){
				DxGetCurrentPositionEx(DxDraw, pps->ps.hdc, &LP);
				box.right = (HEXWIDTH - pps->shift.cx) * pps->fontsize.cx + pps->shiftdot.cx;
				if ( pps->lineY > pps->fontsize.cy ){ // 行下の未表示部分
					box.left   = pps->ps.rcPaint.left;
					box.top    = LP.y + pps->fontsize.cy;
					box.bottom = LP.y + pps->lineY;
					DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
				}
				box.left = LP.x;
				if ( box.left < box.right ){	// 末端より右
					box.top    = LP.y;
					box.bottom = box.top + pps->fontsize.cy;
					DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
				}
			}
			IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc, pps->hBoxFont);
		}
		// ラインカーソル
		if ( (VOsel.cursor != FALSE) && ((pps->shift.cy + y) == VOsel.now.y.line) ){
			HBRUSH hB;

			hB = CreateSolidBrush(CV_lcsr);
			box.left = pps->ps.rcPaint.left;
			box.right = pps->ps.rcPaint.right;
			box.bottom = LP.y + pps->fontsize.cy;
			box.top = box.bottom - 1;
			if ( fontY >= 48 ) box.top -= fontY / 48;
			DxFillRectColor(DxDraw, pps->ps.hdc, &box, hB, CV_lcsr);
			DeleteObject(hB);
		}
	}
	DxSetTextAlign(pps->ps.hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP); // CP を無効

	if ( pps->ps.fErase != FALSE ){			// 背景表示 ===================
										// 未使用行 -------------------
		if ( pps->ps.rcPaint.bottom > LP.y ){
			box.left   = pps->ps.rcPaint.left;
			box.top    = LP.y;
			box.right  = pps->ps.rcPaint.right;
			box.bottom = pps->ps.rcPaint.bottom;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
	}
}

const TCHAR BitmapErrorText[] = T("Bitmap show error");
void ShowBitmapErrorText(PPVPAINTSTRUCT *pps)
{
	DxTextOutAbs(DxDraw, pps->ps.hdc,
			(pps->view.left + pps->view.right) / 2,
			(pps->view.top + pps->view.bottom) / 2,
			BitmapErrorText, TSIZEOFSTR(BitmapErrorText));
}

void DrawBitmap(PPVPAINTSTRUCT *pps, PPvViewObject *vo)
{
	HPALETTE hOldPal;
	int lwidth, lheight, lleft, ltop;
	RECT box, tmpbox,*clipbox;

	IfGDImode(pps->ps.hdc){
					// hPalの論理パレットに変更する。
		 if ( vo->bitmap.hPal != NULL ){
			hOldPal = SelectPalette(pps->ps.hdc, vo->bitmap.hPal, FALSE);
					// 論理パレットを現在のＤＣにマッピングする。
			RealizePalette(pps->ps.hdc);
		}
		if ( vo->bitmap.UseICC >= 0 ) SetICMMode(pps->ps.hdc, vo->bitmap.UseICC);
	}
								// DIBを画面に転送
	if ( (XV.img.MagMode == IMGD_MM_FULLSCALE) && (XV.img.AspectRate == 0) ){ // 等倍表示 --------
		lwidth = vo->bitmap.showsize.cx;
		lheight = vo->bitmap.showsize.cy;
		lleft = (lwidth >= pps->view.right - pps->view.left) ?
			pps->view.left :
			((pps->view.left + pps->view.right) - lwidth) / 2;
		ltop = (lheight >= pps->view.bottom - pps->view.top) ?
			pps->view.top :
			((pps->view.bottom + pps->view.top) - lheight) / 2;
#ifdef USEDIRECTX
		IfDXmode(pps->ps.hdc) {
			RECT box;

			box.left = lleft - VOi->offX;
			box.top = ltop - VOi->offY;
			box.right = lwidth;
			box.bottom = lheight;
			if ( FALSE == DxDrawDIB(DxDraw, vo->bitmap.ShowInfo, vo->bitmap.bits.ptr, &box, &pps->view, &vo->bitmap.DxCache) ){
				ShowBitmapErrorText(pps);
			}
		}else // SetDIBitsToDevice へ
#endif
		{
			if ( FALSE == SetDIBitsToDevice(pps->ps.hdc,
					lleft, ltop, lwidth, lheight,
					VOi->offX, -VOi->offY,  0, lheight,
					vo->bitmap.bits.ptr,
					(BITMAPINFO *)vo->bitmap.ShowInfo, DIB_RGB_COLORS) ){
				ShowBitmapErrorText(pps);
			}
		}
	}else{									// 拡大縮小表示 -------
		int wx, wy;

		if ( XV.img.MagMode == IMGD_MM_FRAME ){ // 窓枠の大きさ
			lwidth = WndSize.cx;
			lheight = pps->view.bottom - pps->view.top;
			if ( ((lwidth << 15)  / vo->bitmap.showsize.cx) >
				 ((lheight << 15) / vo->bitmap.showsize.cy) ){
				lwidth = (vo->bitmap.showsize.cx * lheight) / vo->bitmap.showsize.cy;
			}else{
				lheight = (vo->bitmap.showsize.cy * lwidth) / vo->bitmap.showsize.cx;
			}
			wx = wy = 0;
		}else{ // 固定倍率 ---------------------------------------
			lwidth = VO_maxX;
			lheight = VO_maxY;
			if ( XV.img.MagMode != IMGD_MM_FULLSCALE ){
				wx = VOi->offX * 100 / XV.img.imgD[0];
				wy = -VOi->offY * 100 / XV.img.imgD[0];
			}else{
				wx = VOi->offX;
				wy = -VOi->offY;
			}

			if ( XV.img.AspectRate != 0 ){
				if ( XV.img.AspectRate >= ASPACTX ){
					wy = (wy * ASPACTX) / XV.img.AspectRate;
				}else{
					wx = (wx * XV.img.AspectRate) / ASPACTX;
				}
			}
		}
		if ( lwidth == 0 ) lwidth = 1;
		if ( lheight == 0 ) lheight = 1;

		lleft = (lwidth >= pps->view.right - pps->view.left) ?
			pps->view.left :
			((pps->view.left + pps->view.right) - lwidth) / 2;
		ltop = (lheight >= pps->view.bottom - pps->view.top) ?
			pps->view.top :
			((pps->view.bottom + pps->view.top) - lheight) / 2;
#ifdef USEDIRECTX
		IfDXmode(pps->ps.hdc){
			box.left = lleft - VOi->offX;
			box.top = ltop - VOi->offY;
			box.right = lwidth;
			box.bottom = lheight;
			if ( FALSE == DxDrawDIB(DxDraw, vo->bitmap.ShowInfo,
					vo->bitmap.bits.ptr, &box, &pps->view,
					&vo->bitmap.DxCache) ){
				ShowBitmapErrorText(pps);
			}
		}else // GDI時はSetDIBitsToDevice へ
#endif
		{
			POINT oldpos;
			int oldbm = SetStretchBltMode(pps->ps.hdc,
					XV.img.monomode ? BLACKONWHITE : XV.img.imgD[1]);

			SetBrushOrgEx(pps->ps.hdc, 0, 0, &oldpos);
			if ( FALSE == StretchDIBits(pps->ps.hdc,
					lleft, ltop, lwidth, lheight, wx, wy,
					vo->bitmap.rawsize.cx, vo->bitmap.rawsize.cy,
					vo->bitmap.bits.ptr, (BITMAPINFO *)vo->bitmap.ShowInfo,
					DIB_RGB_COLORS, SRCCOPY) ){
				ShowBitmapErrorText(pps);
			}
			SetBrushOrgEx(pps->ps.hdc, oldpos.x, oldpos.y, NULL);
			SetStretchBltMode(pps->ps.hdc, oldbm);
		}
	}
	if ( pps->ps.fErase != FALSE ){	// 背景表示
		if ( Use2ndView ){
			tmpbox.left = max(pps->ps.rcPaint.left, pps->view.left);
			tmpbox.top = max(pps->ps.rcPaint.top, pps->view.top);
			tmpbox.right = max(pps->ps.rcPaint.right, pps->view.right);
			tmpbox.bottom = max(pps->ps.rcPaint.bottom, pps->view.bottom);
			clipbox = &tmpbox;
		}else{
			clipbox = &pps->ps.rcPaint;
		}

		if ( lleft > clipbox->left ){ // 左真ん中消去
			box.left   = clipbox->left;
			box.top    = ltop;
			box.right  = lleft;
			box.bottom = ltop + lheight;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
		if ( ltop > clipbox->top ){ // 上消去
			box.left   = clipbox->left;
			box.top    = max(clipbox->top, pps->view.top);
			box.right  = clipbox->right;
			box.bottom = ltop;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
		if ( clipbox->right > (lleft + lwidth) ){
			box.left   = lleft + lwidth;  // 右真ん中消去
			box.top    = ltop;
			box.right  = clipbox->right;
			box.bottom = ltop + lheight;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
		if ( clipbox->bottom > (ltop + lheight) ){
			box.left   = clipbox->left;  // 下消去
			box.top    = ltop + lheight;
			box.right  = clipbox->right;
			box.bottom = clipbox->bottom;
			DxFillBack(DxDraw, pps->ps.hdc, &box, C_BackBrush);
		}
	}
									// 前のパレットに戻す。
	IfGDImode(pps->ps.hdc) {
		if ( vo->bitmap.UseICC >= 0 ) SetICMMode(pps->ps.hdc, ICM_OFF);

		if ( vo->bitmap.hPal != NULL ) SelectPalette(pps->ps.hdc, hOldPal, FALSE);
	}
}

void DrawViewObject(PPVPAINTSTRUCT *pps, PPvViewObject *vo)
{
	HRGN hClipRgn;

	if ( pps->ps.rcPaint.bottom <= pps->view.top ) return;

	pps->drawYtop = (pps->ps.rcPaint.top - pps->view.top) / LineY;
	if ( pps->drawYtop < 0 ) pps->drawYtop = 0;
	pps->drawYbottom = (pps->ps.rcPaint.bottom - pps->view.top) / LineY;

	if ( Use2ndView ){
		IfGDImode(pps->ps.hdc) {
			hClipRgn = CreateRectRgnIndirect(&pps->view);
			SelectClipRgn(pps->ps.hdc, hClipRgn);
		}
	}

	switch( vo->DModeType ){
		case DISPT_HEX:	//=================================================
			pps->print = FALSE;
			pps->shift.cx = VOi->offX;
			pps->shift.cy = VOi->offY;
			pps->shiftdot.cx = XV_left;
			pps->shiftdot.cy = pps->view.top;
			pps->fontsize.cx = fontX;
			pps->fontsize.cy = fontY;
			pps->lineY = LineY;
			pps->hBoxFont = hBoxFont;
			DrawHex(pps, vo);
			break;

		case DISPT_DOCUMENT: //============================================
			if ( vo->DocmodeType == DOCMODE_EMETA ){ // Meta file
				DrawEMetafile(pps, vo);
				break;
			}
			// DISPT_TEXT へ
		case DISPT_TEXT:// ================================================
			PaintText(pps, vo);
			break;

		case DISPT_RAWIMAGE: //============================================
		case DISPT_IMAGE:
			DrawBitmap(pps, vo);
			break;

		default:							// 表示対象が無い場合
			if ( pps->ps.fErase != FALSE ){
				DxFillBack(DxDraw, pps->ps.hdc, &pps->view, C_BackBrush);
			}
	}

	if ( Use2ndView ){
		IfGDImode(pps->ps.hdc) {
			SelectClipRgn(pps->ps.hdc, NULL);
			DeleteObject(hClipRgn);
		}
	}

	if ( X_swmt && (vo_.memo.bottom != NULL) && (vo_.memo.top > 0) ){
		DxSetTextColor(DxDraw, pps->ps.hdc, C_info);
		DxSetBkMode(DxDraw, pps->ps.hdc, TRANSPARENT);
#ifdef USEDIRECTX
		DxMoveToEx(DxDraw, pps->ps.hdc, pps->view.left, pps->view.top);
#endif
		DxDrawText(DxDraw, pps->ps.hdc,(const TCHAR *)vo_.memo.bottom, -1, &pps->view, DT_NOCLIP | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL);
		if ( !pps->si.bgmode ) DxSetBkMode(DxDraw, pps->ps.hdc, OPAQUE);
	}
}

void Paint(HWND hWnd)
{
	PPVPAINTSTRUCT pps;
	int x;
	HGDIOBJ hOldFont;
	TCHAR buf[VFPS];
	TCHAR buf2[VFPS];
	TCHAR buf3[VFPS];
	POINT LP;
	RECT box;
	int oldBKmode;
	HDC hOldDC;
										// 表示環境の設定 ---------------------
#ifndef USEDIRECTX
	BeginPaint(hWnd, &pps.ps);
#else
	if ( (vo_.DocmodeType == DOCMODE_EMETA) || (DxDraw == NULL) ){
		BeginPaint(hWnd, &pps.ps);
	}else if ( BeginDrawDxDraw(DxDraw, &pps.ps) != DXSTART_NODRAW ){
		if ( (vo_.DModeBit & VO_dmode_TEXTLIKE) && (VOsel.cursor != FALSE) ){
			HideCaret(hWnd);
		}
	}else{
		return;
	}
#endif
	pps.si.bgmode = FALSE;
	if ( X_fles ) IfGDImode(pps.ps.hdc) {
		InitOffScreen(&BackScreen, pps.ps.hdc, &WndSize);
		hOldDC = pps.ps.hdc;
		pps.ps.hdc = BackScreen.hOffScreenDC;

		if ( X_fles == 2 ){
			pps.ps.rcPaint.top = 0;
			pps.ps.rcPaint.bottom = WndSize.cy;
			pps.ps.fErase = FALSE;
			pps.si.bgmode = TRUE;
		}
	}

	IfGDImode(pps.ps.hdc){
		hOldFont = SelectObject(pps.ps.hdc, hBoxFont);	// フォント
	}

	if ( BackScreen.X_WallpaperType && !(vo_.DModeBit & VO_dmode_IMAGE) ){
		pps.si.bgmode = TRUE;

		DrawWallPaper(DIRECTXARG(DxDraw) &BackScreen, hWnd, &pps.ps, WndSize.cx);
		oldBKmode = DxSetBkMode(DxDraw, pps.ps.hdc, TRANSPARENT);
		pps.ps.fErase = FALSE;
	}

										// １行目 -----------------------------
	if ( pps.ps.rcPaint.top < BoxStatus.bottom ){
		DxSetTextAlign(pps.ps.hdc, TA_LEFT | TA_TOP | TA_UPDATECP); // CP を有効に
		DxMoveToEx(DxDraw, pps.ps.hdc, 0, 0);
		LP.x = 0;
		box.top = 0;
		box.bottom = fontY - 1;

										// 報告文字列表示 *********************
		if ( PopMsgFlag & PMF_DISPLAYMASK ){
			DxSetTextColor(DxDraw, pps.ps.hdc, C_res[0]);		// 文字色
			DxSetBkColor(DxDraw, pps.ps.hdc, C_res[1]);

			if ( pps.si.bgmode ) DxSetBkMode(DxDraw, pps.ps.hdc, OPAQUE);
			DxTextOutBack(DxDraw, pps.ps.hdc, PopMsgStr, tstrlen32(PopMsgStr));
			if ( pps.si.bgmode ) DxSetBkMode(DxDraw, pps.ps.hdc, TRANSPARENT);

			DxGetCurrentPositionEx(DxDraw, pps.ps.hdc, &LP);
			box.left  = LP.x;
			box.right = LP.x += fontX / 2;
			if ( pps.ps.fErase != FALSE ){
				DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
			}
			DxMoveToEx(DxDraw, pps.ps.hdc, box.right, 0);
		}else if ( Mpos >= 0 ){			// Menu +++++++++++++++++++++++++++++++
			int i;

			box.top = 0;
			box.bottom = fontY;
			box.left = 0;
			for ( i = 0 ; i < XV.HiddenMenu.item ; i++, box.left = box.right ){
				DxMoveToEx(DxDraw, pps.ps.hdc, box.left, 0);
				box.right = box.left + fontX * 5;

				if ( i == Mpos ){
					if ( pps.si.bgmode ) DxSetBkMode(DxDraw, pps.ps.hdc, OPAQUE);
					DxSetTextColor(DxDraw, pps.ps.hdc, HiddenMenu_bg(XV.HiddenMenu.data[i]));
					DxSetBkColor(DxDraw, pps.ps.hdc, HiddenMenu_fg(XV.HiddenMenu.data[i]));
				}else{
					DxSetTextColor(DxDraw, pps.ps.hdc, HiddenMenu_fg(XV.HiddenMenu.data[i]));
					DxSetBkColor(DxDraw, pps.ps.hdc, HiddenMenu_bg(XV.HiddenMenu.data[i]));
				}
				DxTextOutBack(DxDraw, pps.ps.hdc,(TCHAR *)XV.HiddenMenu.data[i], 4);
				DxGetCurrentPositionEx(DxDraw, pps.ps.hdc, &LP);
				if ( (i == Mpos) && pps.si.bgmode ){
					DxSetBkMode(DxDraw, pps.ps.hdc, TRANSPARENT);
				}
				box.left = LP.x;
				if ( pps.ps.fErase != FALSE ){
					DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
				}
			}
			#pragma warning(suppress:4701) // XV.HiddenMenu.item は必ず1以上
			LP.x = box.right;
		}
		buf3[0] = '\0';
		if ( FileDivideMode < FDM_NODIVMAX ){
			FormatNumber(buf2, XFN_SEPARATOR, 26, vo_.file.UseSize, 0);
			if ( vo_.DModeType == DISPT_HEX ){
				HexSize(buf2, FileRealSize);
				wsprintf(buf3, T("%04x"),
						(VOsel.cursor != FALSE) ?
								((VOsel.now.y.line * 16) + (VOsel.now.x.offset / HEXNWIDTH)) :
								(VOi->offY * 16) );
			}
		}else{
			TCHAR *ptr;

			FormatNumber(buf2, XFN_SEPARATOR, 7, vo_.file.UseSize, 0);
			ptr = buf2 + tstrlen(buf2);
			*ptr++ = '/';
			FormatNumber(ptr, XFN_SEPARATOR, 7, FileRealSize.LowPart, FileRealSize.HighPart);
			if ( vo_.DModeType == DISPT_HEX ) HexSize(ptr, FileRealSize);
			buf3[0] = '+';
			if ( IsTrue(EnableFileTrackPointer) ){
				if ( vo_.DModeType == DISPT_HEX ){
					LongHex(buf3, FileTrackPointer);
				}else{
					FormatNumber(buf3 + 1, XFN_SEPARATOR, 7, FileTrackPointer.LowPart, FileTrackPointer.HighPart);
				}
				tstrcat(buf3, T("(pause read)"));
				EnableFileTrackPointer = FALSE;
			}else{
				FormatNumber(buf3 + 1, XFN_SEPARATOR, 7, FileDividePointer.LowPart, FileDividePointer.HighPart);
			}
		}
		switch( vo_.DModeType ){				// Status +++++++++++++++++++++
			case DISPT_NONE:
				tstrcpy(buf, vo_.file.typeinfo);
				break;
			case DISPT_HEX: {
				if ( buf3[0] == '+' ){
					DDWORDST rsize;
					DWORD line;

					line = (VOsel.cursor != FALSE) ?
							(DWORD)((VOsel.now.y.line * 16) + (VOsel.now.x.offset / HEXNWIDTH)) :
							(DWORD)(VOi->offY * 16);
					rsize = FileDividePointer;
					AddDD(rsize.LowPart, rsize.HighPart, line, 0);
					LongHex(buf3, rsize);
				}
				wsprintf(buf, T("Address:%sH TextType:%s  Type:%s Filesize:%s"),
						buf3, VO_textS[VOi->textC],  vo_.file.typeinfo, buf2);
				break;
			}
			case DISPT_TEXT: {
				int line, maxline;
				TCHAR LineChar;

				line = (VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY;
				if ( line > VOi->line ) line = VOi->line;
				if ( !XV_numt ){	// 表示行番号
					line = line + 1;
					maxline = VOi->cline;
					LineChar = 'Y';
				}else{				// 論理行番号
					line = VOi->ti[line].line;
					maxline = VOi->ti[VOi->cline].line - 1;
					LineChar = 'L';
				}
				if ( VOi->textC >= 0 ){
					wsprintf(buf,( (BackReader != FALSE) ?
						T("%c-Line:%u/(%u)%s TextType:%s  Type:%s Filesize:%s") :
						T("%c-Line:%u/%u%s TextType:%s  Type:%s Filesize:%s")),
						LineChar, line, maxline, buf3, VO_textS[VOi->textC],
						vo_.file.typeinfo, buf2);
				}else{
					buf[0] = '\0';
				}
				break;
			}
			case DISPT_DOCUMENT:
				if ( vo_.DocmodeType == DOCMODE_EMETA ){ // Meta file
					wsprintf(buf, T("Size:%dx%d  Type:%s Filesize:%s"),
						vo_.bitmap.ShowInfo->biWidth,
						vo_.bitmap.ShowInfo->biHeight,
						vo_.file.typeinfo, buf2);
				}else{
					int line, maxline;
					TCHAR LineChar;

					line = (VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY;
					if ( line >= VOi->line ) line = VOi->line;
					if ( !XV_numt ){	// 表示行番号
						line = line + 1;
						maxline = VOi->cline;
						LineChar = 'Y';
					}else{				// 論理行番号
						if ( VOi->ti != NULL ){
							line = VOi->ti[line].line;
							maxline = VOi->ti[VOi->cline].line - 1;
						}else{
							maxline = VOi->cline;
						}
						LineChar = 'L';
					}
					wsprintf(buf,(T("%c-Line:%u/%u  Type:%s Filesize:%s")),
							LineChar, line, maxline , vo_.file.typeinfo, buf2);
				}
				break;
			case DISPT_RAWIMAGE:
			case DISPT_IMAGE: {
				int i;

				i = wsprintf(buf, T("Size:%dx%d(%dx%d) Color:%s"),
						vo_.bitmap.ShowInfo->biWidth,
						vo_.bitmap.ShowInfo->biHeight,
						// pixel/m → pixel/inch 変換 (39 = 1000 / 25.4)
						vo_.bitmap.ShowInfo->biXPelsPerMeter / 39,
						vo_.bitmap.ShowInfo->biYPelsPerMeter / 39,
						GetColorInfo(vo_.bitmap.ShowInfo, buf3));
				if ( vo_.bitmap.transcolor >= 0 ){
					i += wsprintf(buf + i, T("(transparent)"));
				}
				if ( vo_.bitmap.page.max > 1 ){
					i += wsprintf(buf + i, T(" Page:%3d/%3d"),
							vo_.bitmap.page.current + 1,
							vo_.bitmap.page.max);
				}
				wsprintf(buf + i, T("  Type:%s Filesize:%s"),
						vo_.file.typeinfo, buf2);
				break;
			}
		}
		DxSetTextColor(DxDraw, pps.ps.hdc, C_info);		// 文字色
		DxSetBkColor(DxDraw, pps.ps.hdc, C_back);
											// 内容
		x = tstrlen32(buf);
		DxTextOutRel(DxDraw, pps.ps.hdc, buf, x);

		if ( (vo_.memo.bottom != NULL) && (vo_.memo.top > 1) ){
			DxSetTextColor(DxDraw, pps.ps.hdc, C_mes);
			DxSetBkColor(DxDraw, pps.ps.hdc, C_back);
			DxTextOutRel(DxDraw, pps.ps.hdc, T(" MEMO"), 5);
		}

		if ( BackReader ){
			DxGetCurrentPositionEx(DxDraw, pps.ps.hdc, &LP);
			if ( LP.x >= (BoxStatus.right - (fontX * (int)StrLoadingLength)) ){
				LP.x = BoxStatus.right - fontX * (int)StrLoadingLength;
				if ( pps.ps.fErase == FALSE ){
					box.left  = LP.x;
					box.right = BoxStatus.right;
					DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
				}
			}
			box.left  = LP.x;
			box.right = LP.x += fontX / 2;
			if ( pps.ps.fErase != FALSE ){
				DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
			}
			DxMoveToEx(DxDraw, pps.ps.hdc, box.right, 0);

			DxSetTextColor(DxDraw, pps.ps.hdc, C_info);		// 文字色
			DxSetBkColor(DxDraw, pps.ps.hdc, C_back);
			DxTextOutBack(DxDraw, pps.ps.hdc, StrLoading, StrLoadingLength);
		}
		if ( LP.x < pps.ps.rcPaint.right ){
			DxGetCurrentPositionEx(DxDraw, pps.ps.hdc, &LP);
			if ( LineY > fontY ){
				box.left   = pps.ps.rcPaint.left;
				box.right  = LP.x;
				box.bottom = box.top + LineY - 1;
				box.top    = box.bottom - (LineY - 1 - fontY);
				DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
				box.top    = box.bottom - LineY;
			}
			box.left   = LP.x;
			box.right  = pps.ps.rcPaint.right;
			DxFillBack(DxDraw, pps.ps.hdc, &box, C_BackBrush);
		}

		DxSetTextAlign(pps.ps.hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP); // CP を無効
	}

	DxSetTextColor(DxDraw, pps.ps.hdc, CV_char[CV__deftext]);		// 文字色
	DxSetBkColor(DxDraw, pps.ps.hdc, C_back);
										// 仕切り線の表示 ---------------------
	box.left = pps.ps.rcPaint.left;
	box.right = pps.ps.rcPaint.right;
	box.bottom = BoxStatus.bottom;
	box.top = box.bottom - 1;
	if ( fontY >= 48 ) box.top -= fontY / 48;
	DxFillRectColor(DxDraw, pps.ps.hdc, &box, hStatusLine, C_line);
												// 範囲選択情報 ---------------
	VOsel.bottomOY = VOsel.bottom.y.line - VOi->offY;
	VOsel.topOY = VOsel.top.y.line - VOi->offY;
										// ２行目以降内容 --------------------
	pps.view = BoxView;
	DrawViewObject(&pps, &vo_);
	if ( Use2ndView ){
		pps.view = Box2ndView;
		DrawViewObject(&pps, &vo_);
	}
											// 後始末 -------------------------
	IfGDImode(pps.ps.hdc) {
		if ( pps.si.bgmode ) SetBkMode(pps.ps.hdc, oldBKmode);
		SelectObject(pps.ps.hdc, hOldFont);
		if ( X_fles ){
			pps.ps.hdc = hOldDC;
			OffScreenToScreen(&BackScreen, hWnd, hOldDC, &winS, &WndSize);
		}
		EndPaint(hWnd, &pps.ps);
#ifdef USEDIRECTX
	}else{
		EndDrawDxDraw(DxDraw);
		if ( (vo_.DModeBit & VO_dmode_TEXTLIKE) && (VOsel.cursor != FALSE) ){
			ShowCaret(hWnd);
		}
#endif
	}
}
