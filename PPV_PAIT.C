/*-----------------------------------------------------------------------------
	Paper Plane vUI		Paint Text
-----------------------------------------------------------------------------*/
#pragma setlocale("Japanese")
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

BOOL CheckReverse(int CharY,int CharX);
void DrawLFSymbol(_In_ HDC hDC,BOOL fErase,BOOL reverse);
void DrawTabSymbol(_In_ HDC hDC,int x,int y,COLORREF color);

const char VtabcodeA = 0xaf;	// ↓ ^K VT Vertical Tabulation
const char pagecodeA = 0xac;	// ← ^L FF Form Feed


#ifndef CP_LATIN1
#define CP_LATIN1 1252
#endif

#ifdef USEDIRECTX
	#ifdef UNICODE
	  void USEFASTCALL DxTextOutRelA(DXDRAWSTRUCT *DxDraw,HDC hDC,const char *str,int len)
	  {
		WCHAR text[0x1000];
		int lenW;

		lenW = MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,str,len,text,0x1000);
		DxTextOutBack(DxDraw,hDC,text,lenW);
	  }
	#endif

WCHAR CP437table[256] = {
0x0,	0x263A,	0x263B,	0x2665,	0x2666,	0x2663,	0x2660,	0x2022,	0x25D8,	0x25CB,	0x25D9,	0x2642,	0x2640,	0x266A,	0x266B,	0x263C,
0x25BA,	0x25C4,	0x2195,	0x203C,	0xB6,	0xA7,	0x25AC,	0x21A8,	0x2191,	0x2193,	0x2192,	0x2190,	0x221F,	0x2194,	0x25B2,	0x25BC,
0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,	0x28,	0x29,	0x2A,	0x2B,	0x2C,	0x2D,	0x2E,	0x2F,
0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,	0x38,	0x39,	0x3A,	0x3B,	0x3C,	0x3D,	0x3E,	0x3F,
0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,	0x48,	0x49,	0x4A,	0x4B,	0x4C,	0x4D,	0x4E,	0x4F,
0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,	0x58,	0x59,	0x5A,	0x5B,	0x5C,	0x5D,	0x5E,	0x5F,
0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,	0x68,	0x69,	0x6A,	0x6B,	0x6C,	0x6D,	0x6E,	0x6F,
0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,	0x78,	0x79,	0x7A,	0x7B,	0x7C,	0x7D,	0x7E,	0x2302,
0xC7,	0xFC,	0xe9,	0xe2,	0xe4,	0xe0,	0xe5,	0xe7,	0xEA,	0xEB,	0xe8,	0xEF,	0xEE,	0xEC,	0xC4,	0xC5,
0xC9,	0xe6,	0xC6,	0xF4,	0xF6,	0xF2,	0xFB,	0xF9,	0xFF,	0xD6,	0xDC,	0xA2,	0xA3,	0xA5,	0x20A7,	0x192,
0x0E1,	0xED,	0xF3,	0xFA,	0xF1,	0xD1,	0xAA,	0xBA,	0xBF,	0x2310,	0xAC,	0xBD,	0xBC,	0xA1,	0xAB,	0xBB,
0x2591,	0x2592,	0x2593,	0x2502,	0x2524,	0x2561,	0x2562,	0x2556,	0x2555,	0x2563,	0x2551,	0x2557,	0x255D,	0x255C,	0x255B,	0x2510,
0x2514,	0x2534,	0x252C,	0x251C,	0x2500,	0x253C,	0x255E,	0x255F,	0x255A,	0x2554,	0x2569,	0x2566,	0x2560,	0x2550,	0x256C,	0x2567,
0x2568,	0x2564,	0x2565,	0x2559,	0x2558,	0x2552,	0x2553,	0x256B,	0x256A,	0x2518,	0x250C,	0x2588,	0x2584,	0x258C,	0x2590,	0x2580,
0x03B1,	0xDF,	0x393,	0x03C0,	0x03A3,	0x03C3,	0xB5,	0x03C4,	0x03A6,	0x398,	0x03A9,	0x03B4,	0x221E,	0x03C6,	0x03B5,	0x2229,
0x2261,	0xB1,	0x2265,	0x2264,	0x2320,	0x2321,	0xF7,	0x2248,	0xB0,	0x2219,	0xB7,	0x221A,	0x207F,	0xB2,	0x25A0,	0xA0};
#endif

BYTE GetMarkColor(int bc,int fc)
{
	int c = (bc + (fc - bc) / 4);
	if ( c > 255 ) c = 255;
	if ( c < 0 ) c = 0;
	return (BYTE)c;
}

COLORREF GetLineMarkColor(COLORREF bg,COLORREF mark)
{
	return RGB(
		GetMarkColor(GetRValue(bg),GetRValue(mark)),
		GetMarkColor(GetRValue(bg),GetGValue(mark)),
		GetMarkColor(GetBValue(bg),GetBValue(mark)) );
}

void DrawSymbol(PAINTSTRUCT *ps,HFONT *nowfont,RECT *box,const char *ansi,const WCHAR *wide)
{
	#ifndef USEDIRECTX
	if ( XV_bctl[1] == 1 ){
		InitSymbolFont(ps->hdc);
		if ( (ps->fErase != FALSE) && (fontY != fontSYMY) ){
			POINT LP;

			DxGetCurrentPositionEx(DxDraw,ps->hdc,&LP);
			box->left   = LP.x;
			box->right  = box->left + fontX;
			DxFillBack(DxDraw,ps->hdc,box,C_BackBrush);
		}
		DxSetTextColor(DxDraw,ps->hdc,CV_lf);
		DxSetBkColor(DxDraw,ps->hdc,C_back);
		if ( *nowfont != hSYMFont ){
			*nowfont = hSYMFont;
			IfGDImode(ps->hdc) SelectObject(ps->hdc,*nowfont);
		}
		DxTextOutRelA(DxDraw,ps->hdc,ansi,1);
	}else
	#endif
	{
		DxSetTextColor(DxDraw,ps->hdc,CV_lf);
		DxSetBkColor(DxDraw,ps->hdc,C_back);
		DxTextOutRelW(DxDraw,ps->hdc,wide,1);
	}
}

void PaintText(PPVPAINTSTRUCT *pps,PPvViewObject *vo)
{
	int y,CharX;
	BYTE buf[TEXTBUFSIZE],*drawp;
	WCHAR wbuf[TEXTBUFSIZE];
	POINT LP,FP;
	int Rx;
	RECT box;
	HFONT textfont C4701CHECK,nowfont = NULL,normalfont;
	int NowY;
	MAKETEXTINFO mti;

	NowY = pps->drawYtop + VOi->offY;
	if ( NowY >= VOi->line  ){
		if ( pps->ps.fErase != FALSE ){
			if ( Use2ndView ){
				box = pps->view;
			}else{
				box = pps->ps.rcPaint;
			}
			if ( box.top < pps->view.top ) box.top = pps->view.top;
			DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
		}
		return;
	}
	DxSetTextAlign(pps->ps.hdc,TA_LEFT | TA_TOP | TA_UPDATECP);	// CP を有効に
												// 色の設定 -------------------
	pps->si.fg = CV_char[VOi->ti[NowY].Fclr];
	pps->si.bg = CV_char[VOi->ti[NowY].Bclr];
	if ( VOi->ti[NowY].attrs & (VTTF_TAG | VTTF_LINK) ){
		if ( VOi->ti[NowY].attrs & VTTF_TAG ){
			pps->si.fg = CV_syn[0];
		}else{
			pps->si.fg = CV_link;
		}
	}

	FP.x = pps->view.left - VOi->offX * fontX;
	XV_lleft = 0;
	if ( XV_lnum ){	// 行番号
		DWORD line,newline;

		LP.x = FP.x;
		XV_lleft = fontX * DEFNUMBERSPACE;
		if ( XV_numt ){
			line = (NowY > 0) ? VOi->ti[NowY - 1].line : 0;
		}else{
			line = 0;
		}
		for ( y = pps->drawYtop ; y <= pps->drawYbottom ; y++ ){
			if ( (y + VOi->offY) >= VOi->line ){ // 空行
				if ( pps->ps.fErase != FALSE ){
					box.top = y * LineY + pps->view.top;
					box.bottom = (pps->drawYbottom + 1) * LineY + pps->view.top;
					box.left = 0;
					box.right = FP.x + XV_lleft;
					DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
				}
				break;
			}

			DxMoveToEx(DxDraw,pps->ps.hdc,FP.x,y * LineY + pps->view.top);
			if ( XV_numt ){
				newline = VOi->ti[y + VOi->offY].line;
			}else{
				newline = y + VOi->offY + 1;
			}
			DxSetTextColor(DxDraw,pps->ps.hdc,(line == newline) ? CV_lnum[1] : CV_lnum[0]);
			DxTextOutRel(DxDraw,pps->ps.hdc,(TCHAR *)buf,
				wsprintf((TCHAR *)buf,T("%5d "),newline) );
			line = newline;

			if ( y == pps->drawYtop ){
				DxGetCurrentPositionEx(DxDraw,pps->ps.hdc,&LP);
				XV_lleft = LP.x - FP.x;
			}

			if ( (pps->ps.fErase != FALSE) && (LineY > fontY) ){ // 行番号下の行間
				DxGetCurrentPositionEx(DxDraw,pps->ps.hdc,&LP);
				box.left   = pps->ps.rcPaint.left;
				box.right  = LP.x;
				box.bottom = (y + 1) * LineY + pps->view.top;
				box.top = box.bottom - (LineY - fontY);
				DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
			}
		}

		{	// 行番号右の線
			HBRUSH hB;

			hB = CreateSolidBrush(CV_boun);
			box.left = LP.x - (fontX >> 2) - 1;
			box.right = box.left + 1;
			box.top = pps->view.top;
			box.bottom = pps->ps.rcPaint.bottom;

			DxFillRectColor(DxDraw,pps->ps.hdc,&box,hB,CV_boun);
			DeleteObject(hB);
		}
		FP.x += XV_lleft;
	}
	FP.x += XV_left;
	box.top = max(pps->ps.rcPaint.top,pps->view.top);
	Rx = FP.x + fontX * VOi->width;

	if ( pps->ps.fErase != FALSE ){
		box.bottom = pps->ps.rcPaint.bottom;
		if ( pps->ps.rcPaint.left < FP.x ){	// 左マージンの背景
			box.right = FP.x;
			box.left  = FP.x - XV_left;
			DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
		}
		if ( pps->ps.rcPaint.right > Rx ){	// 右端より右の背景
			box.left  = Rx + 1;	// 境界線分
			box.right = pps->ps.rcPaint.right;
			DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
		}
	}
												// 右端の境界線 ---------------
	if ( pps->ps.rcPaint.right >= Rx ){
		HBRUSH hBrush;

		box.left = Rx;
		box.right = box.left + 1;
		box.bottom = pps->ps.rcPaint.bottom;
		if ( fontY >= 48 ) box.right += fontY / 48;

		hBrush = CreateSolidBrush(CV_boun);
		DxFillRectColor(DxDraw,pps->ps.hdc,&box,hBrush,CV_boun);
		DeleteObject(hBrush);
	}
	normalfont = XV_unff ? hUnfixedFont : hBoxFont;

	mti.destbuf = buf;
	mti.srcmax = vo->file.image + vo->file.UseSize;
	mti.writetbl = FALSE;
	mti.paintmode = TRUE;

	for ( y = pps->drawYtop ; y <= pps->drawYbottom ; y++ ){
		int showy;

		CharX = 0;
		{
			int top;

			top = y * LineY + pps->view.top;
			box.top = top;
			box.bottom = top + fontY;
			DxMoveToEx(DxDraw,pps->ps.hdc,FP.x,top);
		}
		showy = y + VOi->offY;

		if ( ShowTextLineFlags != 0 ){
			if ( (CV_lbak[0] != C_AUTO) &&
				 (ShowTextLineFlags & SHOWTEXTLINE_OLDTEXT) &&
				 TailModeFlags ){
				if ( showy <= OldTailLine ){
					pps->si.bg = CV_char[VOi->ti[showy].Bclr];
					if ( showy < OldTailLine ){
						pps->si.bg = GetLineMarkColor(pps->si.bg,CV_lbak[0]);
					}
				}
			}

			if ( CV_lbak[2] != C_AUTO ){
				int bki;

				for ( bki = BookmarkID_1st ; bki <= MaxBookmark ; bki++ ){
					int booky = Bookmark[bki].pos.y;

					if ( booky < 0 ) continue;

					if ( ((booky == showy) || ((booky + 1) == showy)) &&
						 ( (FileDivideMode < FDM_DIV) ||
						   ((Bookmark[bki].div.LowPart == FileDividePointer.LowPart) &&
						   (Bookmark[bki].div.HighPart == FileDividePointer.HighPart)) )
					 ){
						pps->si.bg = CV_char[VOi->ti[showy].Bclr];
					 	if ( booky == showy ){
							pps->si.bg = GetLineMarkColor(pps->si.bg,CV_lbak[2]);
							break;
						}
					}
				}
			}
		}
												// 書体の設定 -----------------

		if ( showy >= VOi->line ){
			drawp = (BYTE *)"";
		}else{
			textfont = normalfont;
			DxSelectFont(DxDraw,XV_unff ? 1 : 0);
			if ( VOi->ti[showy].type == VTYPE_IBM ){
				textfont = GetIBMFont();
			}else if ( VOi->ti[showy].type == VTYPE_ANSI ){
				textfont = GetANSIFont();
			}

			if ( textfont != nowfont ){
				nowfont = textfont;
				IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
				#ifdef USEDIRECTWRITE
					if ( (textfont == hIBMFont) || (textfont == hANSIFont) ){
						SetFontDxDraw(DxDraw,textfont,1);
						DxSelectFont(DxDraw,2);
					}
				#endif
			}
			VOi->MakeText(&mti,&VOi->ti[showy]);
			drawp = buf;
		}

		while( *drawp != VCODE_END ){
		switch( *drawp++ ){ // VCODE_SWITCH
			case VCODE_CONTROL:
			case VCODE_ASCII:	// Text 表示 ---------------------------------
			{
				int length;

				if ( nowfont != textfont ){ // C4701ok ここに来るときは必ず textfont = normalfont; 済
					DxSelectFont(DxDraw,XV_unff ? 1 : 0);
					nowfont = textfont;
					IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
				}
				if ( (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ){
					if ( pps->si.bgmode ){
						DxSetBkMode(DxDraw,pps->ps.hdc,OPAQUE);
					}
					DxSetBkColor(DxDraw,pps->ps.hdc,C_info);		// 文字色
					DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
				}else{
					if ( pps->si.bgmode ){
						DxSetBkMode(DxDraw,pps->ps.hdc,TRANSPARENT);
					}
					DxSetTextColor(DxDraw,pps->ps.hdc,
						(*(drawp - 1) == VCODE_ASCII) ? pps->si.fg : CV_ctrl);
					DxSetBkColor(DxDraw,pps->ps.hdc,pps->si.bg);
				}
				length = strlen32((char *)drawp);
				if ( length ){
					if ( (pps->ps.fErase != FALSE) && (nowfont != normalfont) ){
						// 通常以外のフォントの時は、高さが合わないときがある
						// ので、一旦背景消去する
						DxGetCurrentPositionEx(DxDraw,pps->ps.hdc,&LP);
						box.left   = LP.x;
						box.right  = box.left + length * fontX;
						if ( XV_unff && (box.right > Rx) ) box.right = Rx;
						DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
					}
					if ( (nowfont == hIBMFont)
#ifndef UNICODE
						&& ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT )
#endif
					){
						BYTE *s;
						WCHAR *d;
						int j;

						s = drawp;
						d = wbuf;
						for ( j = 0 ; j < length ; j++ ){
#ifndef USEDIRECTX
							*d++ = *s++;
#else
							*d++ = CP437table[*s++];
#endif
						}
						*d = '\0';
						DrawSelectedTextW(pps->ps.hdc,&pps->si,(WCHAR *)wbuf,length,CharX,y);
#if defined(UNICODE) || defined(USEDIRECTX)
					}else if ( nowfont == hANSIFont ){
						int wlength;

						wlength = MultiByteToWideChar(CP_LATIN1, 0,
								(LPCSTR)drawp, length, wbuf, TEXTBUFSIZE);
						DrawSelectedTextW(pps->ps.hdc, &pps->si, (WCHAR *)wbuf, wlength, CharX, y);
#endif
					}else if ( VO_CodePage && VO_CodePageValid ){
						int wlength;
						// UTF7/8を表示するには、フラグ指定をなくす必要がある
						// UTF7/8指定はコード変換専用のようだ
						wlength = MultiByteToWideChar(VO_CodePage, 0,
								(LPCSTR)drawp, length, wbuf, TEXTBUFSIZE);
						DrawSelectedTextW(pps->ps.hdc, &pps->si, (WCHAR *)wbuf, wlength, CharX, y);

					}else{
						DrawSelectedTextA(pps->ps.hdc, &pps->si, (char *)drawp, length, CharX, y);
					}
				}
				drawp += length + 1;
				CharX += length;
				break;
			}
			case VCODE_UNICODEF:	// Dummy(WORD境界合わせ用) ---------------
				drawp++;
			case VCODE_UNICODE:		// UNICODE Text 表示 ---------------------
			{
				int length;

				if ( nowfont != textfont ){
					nowfont = textfont;
					IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
				}
				if ( (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ){
					if ( pps->si.bgmode ) DxSetBkMode(DxDraw,pps->ps.hdc,OPAQUE);
					DxSetBkColor(DxDraw,pps->ps.hdc,C_info);		// 文字色
					DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
				}else{
					if ( pps->si.bgmode ) DxSetBkMode(DxDraw,pps->ps.hdc,TRANSPARENT);
					DxSetTextColor(DxDraw,pps->ps.hdc,pps->si.fg);
					DxSetBkColor(DxDraw,pps->ps.hdc,pps->si.bg);
				}
				length = strlenW32((WCHAR *)drawp);
				DrawSelectedTextW(pps->ps.hdc,&pps->si,(WCHAR *)drawp,length,CharX,y);
				drawp += (length + 1) * 2;
				CharX += length;
				break;
			}

			case VCODE_COLOR:	// Color -------------------------------------
				pps->si.fg = CV_char[*drawp];
				pps->si.bg = CV_char[*(drawp + 1)];
				if ( BackScreen.X_WallpaperType ){
					pps->si.bgmode = (*(drawp + 1) == CV__defback);
					DxSetBkMode(DxDraw,pps->ps.hdc,pps->si.bgmode ? TRANSPARENT : OPAQUE);
				}
				drawp += 2;
				break;

			case VCODE_FCOLOR:	// F Color -----------------------------------
				pps->si.fg = *(COLORREF *)drawp;
				drawp += sizeof(COLORREF);
				break;

			case VCODE_BCOLOR:	// B Color -----------------------------------
				pps->si.bg = *(COLORREF *)drawp;
				drawp += sizeof(COLORREF);
				break;

			case VCODE_FONT:	// Font --------------------------------------
				switch ( *drawp++ ){
					case 0:
						textfont = GetIBMFont();
						break;
					case 1:
						textfont = GetANSIFont();
						break;
					default:
						textfont = normalfont;
						DxSelectFont(DxDraw,XV_unff ? 1 : 0);
						break;
				}
				if ( textfont != nowfont ){
					nowfont = textfont;
					IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
				}
				break;

			case VCODE_TAB:{	// Tab ---------------------------------------
				BOOL reverse = FALSE;
				int tabwidth;

				DxGetCurrentPositionEx(DxDraw,pps->ps.hdc,&LP);
				box.left = LP.x;
				tabwidth = VOi->tab * fontX;
				box.right = FP.x + (((LP.x - FP.x) / tabwidth) + 1) * tabwidth;

				if ( (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ){
					reverse = CheckReverse(y,CharX);
				}
				if ( !reverse ){
					if ( pps->ps.fErase != FALSE ){
						DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
					}
				}else{
					HBRUSH hB;

					hB = CreateSolidBrush(C_info);
					DxFillRectColor(DxDraw,pps->ps.hdc,&box,hB,C_info);
					DeleteObject(hB);
				}
				if ( XV_bctl[0] ){
					#ifndef USEDIRECTX
					if ( XV_bctl[0] == 1 ){
						DrawTabSymbol(pps->ps.hdc,box.left,box.top + LineY / 2,
								reverse ? C_back : CV_tab);
					}else
					#endif
					{
						if ( reverse ){
							DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
							DxSetBkColor(DxDraw,pps->ps.hdc,C_info);
						}else{
							DxSetTextColor(DxDraw,pps->ps.hdc,CV_tab);
							DxSetBkColor(DxDraw,pps->ps.hdc,C_back);
						}

						if ( nowfont != normalfont ){
							DxSelectFont(DxDraw,XV_unff ? 1 : 0);
							nowfont = normalfont;
							IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
						}
						DxTextOutRelW(DxDraw,pps->ps.hdc,XV_ctls + CTRLSIG_HTAB,1);
					}
				}
				DxMoveToEx(DxDraw,pps->ps.hdc,box.right,box.top);
				CharX++;
				break;
			}

			case VCODE_RETURN:	// CRLF/LF ------------------------------------
				if ( XV_bctl[1] ){
					BOOL reverse = FALSE;

					 // 最終行の改行は、必ず選択範囲外
					if ( (VOsel.bottomOY <= y) && (VOsel.topOY > y) ){
						reverse = CheckReverse(y,CharX);
					}
					#ifndef USEDIRECTX
					if ( XV_bctl[1] == 1 ){
						DrawLFSymbol(pps->ps.hdc,pps->ps.fErase,reverse);
					}else
					#endif
					{
						if ( reverse ){
							DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
							DxSetBkColor(DxDraw,pps->ps.hdc,CV_lf);
						}else{
							DxSetTextColor(DxDraw,pps->ps.hdc,CV_lf);
							DxSetBkColor(DxDraw,pps->ps.hdc,C_back);
						}
						if ( nowfont != normalfont ){
							DxSelectFont(DxDraw,XV_unff ? 1 : 0);
							nowfont = normalfont;
							IfGDImode(pps->ps.hdc) SelectObject(pps->ps.hdc,nowfont);
						}
						DxTextOutRelW(DxDraw,pps->ps.hdc,XV_ctls + *drawp,1);
					}
				}
				drawp++;
				break;

			case VCODE_PARA:	// ^K ----------------------------------------
				if ( XV_bctl[1] != 0 ){
					DrawSymbol(&pps->ps,&nowfont,&box,&VtabcodeA,XV_ctls + CTRLSIG_VTAB);
					CharX++;
				}
				break;

			case VCODE_PAGE:	// ^L ----------------------------------------
				if ( XV_bctl[1] != 0 ){
					DrawSymbol(&pps->ps,&nowfont,&box,&pagecodeA,XV_ctls + CTRLSIG_PAGE);
				}
				break;

			case VCODE_SPACE:{	// 全角空白 -----------------------------------
				BOOL reverse = FALSE;

				if ( (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ){
					reverse = CheckReverse(y,CharX);
				}
				if ( !reverse ){
					DxSetTextColor(DxDraw,pps->ps.hdc,CV_spc);
					DxSetBkColor(DxDraw,pps->ps.hdc,C_back);
				}else{
					DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
					DxSetBkColor(DxDraw,pps->ps.hdc,C_info);
				}
				DxTextOutRelA(DxDraw,pps->ps.hdc,StrSJISSpace,2);
				CharX += 2;
				break;
			}
			case VCODE_WSPACE:{	// UNICODE 全角空白 ---------------------------
				BOOL reverse = FALSE;

				if ( (VOsel.bottomOY <= y) && (VOsel.topOY >= y) ){
					reverse = CheckReverse(y,CharX);
				}
				if ( !reverse ){
					DxSetTextColor(DxDraw,pps->ps.hdc,CV_spc);
					DxSetBkColor(DxDraw,pps->ps.hdc,C_back);
				}else{
					DxSetTextColor(DxDraw,pps->ps.hdc,C_back);
					DxSetBkColor(DxDraw,pps->ps.hdc,C_info);
				}
				DxTextOutRelW(DxDraw,pps->ps.hdc,StrWideSpace,1);
				CharX++;
				break;
			}
			case VCODE_LINK:	// Link --------------------------------------
				pps->si.fg = CV_link;
				pps->si.bg = C_back;
				break;

			default:		// 未定義コード -----------------------------------
				DxSetTextColor(DxDraw,pps->ps.hdc,C_CYAN);
				DxTextOutRelA(DxDraw,pps->ps.hdc,"MakeTextError",13);
				drawp = (BYTE *)"";
				break;
		}
		}
		if ( pps->ps.fErase != FALSE ){
			DxGetCurrentPositionEx(DxDraw,pps->ps.hdc,&LP);
			if ( LineY > fontY ){ // 描画文字の行間の消去
				box.left   = FP.x;
				box.right  = LP.x;
				box.top    = box.bottom;
				box.bottom += LineY - fontY;
				DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
				box.top    = box.bottom - LineY;
			}
			if ( LP.x < Rx ){ // 右側余白の消去
				box.left = LP.x;
				box.right = Rx;
				DxFillBack(DxDraw,pps->ps.hdc,&box,C_BackBrush);
			}
		}
		// 最終行ライン表示
		if ( (y + VOi->offY ) == VOi->line  ){
			HBRUSH hBrush;

			box.left   = pps->view.left;
			box.right  = Rx;
			box.bottom = box.top + 1;
			if ( fontY >= 48 ) box.top -= fontY / 48;
			hBrush = CreateSolidBrush(CV_boun);
			DxFillRectColor(DxDraw,pps->ps.hdc,&box,hBrush,CV_boun);
			DeleteObject(hBrush);
		}
		// ラインカーソル
		if ( (VOsel.foundY == (y + VOi->offY )) ||
			 ((VOsel.cursor != FALSE) && ((y + VOi->offY ) == VOsel.now.y.line)) ){
			HBRUSH hB;

			hB = CreateSolidBrush(CV_lcsr);
			box.left = pps->ps.rcPaint.left;
			box.right = pps->ps.rcPaint.right;
			box.top = box.bottom - 1;
			if ( fontY >= 48 ) box.top -= fontY / 48;
			DxFillRectColor(DxDraw,pps->ps.hdc,&box,hB,CV_lcsr);
			DeleteObject(hB);
		}
	}
	DxSetTextAlign(pps->ps.hdc,TA_LEFT | TA_TOP | TA_NOUPDATECP);	// CP を無効に
}

BOOL CheckReverse(int CharY,int CharX)
{
	if ( VOsel.linemode ) return TRUE;

	if ( VOsel.bottomOY == CharY ){		// 上端
		if ( VOsel.topOY == CharY ){ // 同じ位置
			if ( (CharX >= VOsel.bottom.x.offset) && (CharX < VOsel.top.x.offset)){
				return TRUE;
			}
		}else{
			if ( CharX >= VOsel.bottom.x.offset ) return TRUE;
		}
	}else if ( VOsel.topOY > CharY ){	// 中間
		return TRUE;
	}else{							// 下端
		if ( CharX < VOsel.top.x.offset ) return TRUE;
	}
	return FALSE;
}

void DrawSelectedTextW(HDC hDC,SELINFO *si,WCHAR *text,int length,int charX,int offsetY)
{
	int left,right;

	if ( !length ) return;

	// 選択範囲の途中か、非選択範囲
	if ( VOsel.linemode ||
		(VOsel.select == FALSE) ||
		!( (offsetY == VOsel.bottomOY) || (offsetY == VOsel.topOY) )
	){
		DxTextOutRelW(DxDraw,hDC,text,length);
		return;
	}

	left = ( offsetY != VOsel.bottomOY ) ? 0 : VOsel.bottom.x.offset;
	right = ( offsetY != VOsel.topOY ) ? TEXTBUFSIZE : VOsel.top.x.offset;
									// 選択範囲より左…非選択で表示
	if ( (charX + length) <= left ){
		DxSetTextColor(DxDraw,hDC,si->fg);
		DxSetBkColor(DxDraw,hDC,si->bg);
		DxTextOutRelW(DxDraw,hDC,text,length);
		return;
	}
									// 途中で選択範囲が始まる…始まる以前を表示
	if ( (charX <= left) && ((charX + length) > left) ){
		int d;

		d = left - charX;
		if ( d > 0 ){
			DxSetTextColor(DxDraw,hDC,si->fg);
			DxSetBkColor(DxDraw,hDC,si->bg);
			DxTextOutRelW(DxDraw,hDC,text,d);
			length -= d;
			charX += d;
			text += d;
		}
	}
									// 途中で選択範囲が終わる
	if ( (charX + length) > right ){
		int d;

		d = right - charX;
		if ( d > 0 ){ // 範囲が終わるまで表示
			if ( d > length ) d = length;
			if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,OPAQUE);
			DxSetBkColor(DxDraw,hDC,C_info);
			DxSetTextColor(DxDraw,hDC,C_back);
			DxTextOutRelW(DxDraw,hDC,text,d);
			if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,TRANSPARENT);
			length -= d;
			text += d;
		}
		// 終わった後を表示
		DxSetTextColor(DxDraw,hDC,si->fg);
		DxSetBkColor(DxDraw,hDC,si->bg);
		DxTextOutRelW(DxDraw,hDC,text,length);
		return;
	}
										// 選択範囲より前で終わる
	if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,OPAQUE);
	DxSetBkColor(DxDraw,hDC,C_info);
	DxSetTextColor(DxDraw,hDC,C_back);
	DxTextOutRelW(DxDraw,hDC,text,length);
	if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,TRANSPARENT);
}

void TextFixOut(HDC hDC,char *str,int len)
{
	if ( len == 0 ) len = strlen32(str);
#ifdef UNICODE
	{ // ※ 欧文フォントを使っていると、日本語フォントをフォントリンクで
	  //    指定しても全て欧文フォントが使用されるのでUNICODEに変換する
		WCHAR wbuf[TEXTBUFSIZE];

		len = MultiByteToWideChar(CP_ACP,0,str,len,wbuf,TEXTBUFSIZE);
		DxTextOutRelW(DxDraw,hDC,wbuf,len);
	}
	return;
#else
	if ( (fontWW == 0) || XV_unff ){
		DxTextOutRelA(DxDraw,hDC,str,len);
		return;
	}
	while(len){
		if ( IskanjiA(*str) ){
			char *p;
			int l = 0;

			for ( p = str ; len && IskanjiA(*str) ; str+=2,l+=2,len-=2);
			SetTextCharacterExtra(hDC,fontWW);
			DxTextOutRelA(DxDraw,hDC,p,l);
			SetTextCharacterExtra(hDC,0);
			if ( len <= 0 ) break;
		}else{
			char *p;
			int l = 0;

			for ( p = str ; len && !IskanjiA(*str) ; str++,l++,len--);
			DxTextOutRelA(DxDraw,hDC,p,l);
		}
	}
#endif
}

void DrawSelectedTextA(HDC hDC,SELINFO *si,char *text,int length,int charX,int offsetY)
{
	int left,right;

	if ( length == 0 ) return;

	// 選択範囲の途中か、非選択範囲
	if ( VOsel.linemode ||
		(VOsel.select == FALSE) ||
		!( (offsetY == VOsel.bottomOY) || (offsetY == VOsel.topOY) )
	){
		TextFixOut(hDC,text,length);
		return;
	}

	left = ( offsetY != VOsel.bottomOY ) ? 0 : VOsel.bottom.x.offset;
	right = ( offsetY != VOsel.topOY ) ? TEXTBUFSIZE : VOsel.top.x.offset;
									// 選択範囲より左…非選択で表示
	if ( (charX + length) <= left ){
		DxSetTextColor(DxDraw,hDC,si->fg);
		DxSetBkColor(DxDraw,hDC,si->bg);
		TextFixOut(hDC,text,length);
		return;
	}
									// 途中で選択範囲が始まる…始まる以前を表示
	if ( (charX <= left) && ((charX + length) > left) ){
		int d;

		d = left - charX;
		if ( d > 0 ) d = SelColA(text,d) - text;
		if ( d > 0 ){
			DxSetTextColor(DxDraw,hDC,si->fg);
			DxSetBkColor(DxDraw,hDC,si->bg);
			TextFixOut(hDC,text,d);
			length -= d;
			charX += d;
			text += d;
		}
	}
									// 途中で選択範囲が終わる
	if ( (charX + length) > right ){
		int d;

		d = right - charX;
		if ( d > 0 ) d = SelColA(text,d) - text;
		if ( d > 0 ){ // 範囲が終わるまで表示
			if ( d > length ) d = length;
			if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,OPAQUE);
			DxSetBkColor(DxDraw,hDC,C_info);
			DxSetTextColor(DxDraw,hDC,C_back);
			TextFixOut(hDC,text,d);
			if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,TRANSPARENT);
			length -= d;
			text += d;
		}
		// 終わった後を表示
		DxSetTextColor(DxDraw,hDC,si->fg);
		DxSetBkColor(DxDraw,hDC,si->bg);
		TextFixOut(hDC,text,length);
		return;
	}
										// 選択範囲より前で終わる
	if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,OPAQUE);
	DxSetBkColor(DxDraw,hDC,C_info);
	DxSetTextColor(DxDraw,hDC,C_back);
	TextFixOut(hDC,text,length);
	if ( si->bgmode ) DxSetBkMode(DxDraw,hDC,TRANSPARENT);
	return;
}
// 改行記号を表示(低速版)
void DrawLFSymbol(_In_ HDC hDC,BOOL fErase,BOOL reverse)
{
	int y,wide,height;
	POINT LP;
	HGDIOBJ hOldPen;
	RECT box;

	DxGetCurrentPositionEx(DxDraw,hDC,&LP);
	box.right = LP.x + fontX;
	box.top = LP.y;

	if ( reverse ){
		HBRUSH hB;

		hB = CreateSolidBrush(C_info);
		box.left   = LP.x;
		box.bottom = box.top + fontY;
		DxFillRectColor(DxDraw,hDC,&box,hB,C_info);
		DeleteObject(hB);
	}else if ( fErase != FALSE ){
		box.left   = LP.x;
		box.bottom = box.top + fontY;
		DxFillBack(DxDraw,hDC,&box,C_BackBrush);
	}

	y = LP.y + fontY / 2;
	wide = fontX * 1 / 8 + 1;
	height = fontY * 1 / 8 + 1;
	hOldPen = SelectObject(hDC,CreatePen(PS_SOLID,0,CV_lf));
	DxMoveToEx(DxDraw,hDC,LP.x + fontX - wide,y - height * 2);
	LineTo  (hDC,LP.x + fontX - wide,y);
	LineTo  (hDC,LP.x + wide ,y);
	LineTo  (hDC,LP.x + wide * 3,y - height);
	DxMoveToEx(DxDraw,hDC,LP.x + wide ,y);
	LineTo  (hDC,LP.x + wide * 3,y + height);
	DeleteObject(SelectObject(hDC,hOldPen));
	DxMoveToEx(DxDraw,hDC,box.right,box.top);
}

// tab記号を表示(低速版)
void DrawTabSymbol(_In_ HDC hDC,int x,int y,COLORREF color)
{
	int wide,height;
	HGDIOBJ hOldPen;

	wide = fontX * 1 / 8 + 1;
	height = fontY * 1 / 8 + 1;
	hOldPen = SelectObject(hDC,CreatePen(PS_SOLID,0,color));
	DxMoveToEx(DxDraw,hDC,x + wide, y);
	LineTo  (hDC,x + fontX - wide ,y);
	LineTo  (hDC,x + fontX - wide * 3,y - height);
	DxMoveToEx(DxDraw,hDC,x + fontX - wide ,y);
	LineTo  (hDC,x + fontX - wide * 3,y + height);
	DeleteObject(SelectObject(hDC,hOldPen));
}
