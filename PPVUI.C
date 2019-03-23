/*-----------------------------------------------------------------------------
	Paper Plane viewUI											Main
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop
#include "PPV_GVAR.C"	// グローバルの実体定義
#define CARETSCROLLMARGINX 4 // キャレットのスクロール開始する桁
#define CARETSCROLLMARGINY 1 // キャレットのスクロール開始する行

int GetPointY(int posY);

#ifndef UNICODE
BOOL multichar = FALSE;
#endif

#define CSRTM_OFF 0			// timer 使用せず
#define CSRTM_OUT 1		// 範囲外スクロール(クリック中)
#define CSRTM_INERTIA 2	// 慣性スクロール(クリック後)
#define CSRTM_SELECT 3		// 範囲選択スクロール(クリック中)
int CursorTimerMode = CSRTM_OFF;
SIZE OffPos = {0,0}; // 慣性スクロール量
static LONG ClkTime;
static int KeyChar = 0;		// WM_KEYUP 中は 1
int PosChangeEntry = 0;
BOOL detailmode = FALSE;
TCHAR PPvMainThreadName[] = T("PPv main");

#ifdef USEDIRECTX
#pragma argsused
void CALLBACK DrawTimerProc(HWND hWnd,UINT unuse1,UINT_PTR unuse2,DWORD unuse3)
{
	InvalidateRect(hWnd,NULL,FALSE);
}
#endif

void Isearch(PPV_APPINFO *vinfo,HWND hEditWnd)
{
	VOsel.VSstring[0] = '\0';
	GetWindowText(hEditWnd,VOsel.VSstring,TSIZEOF(VOsel.VSstring));

	if ( BackupOffY < 0 ){
		if ( BackupOffY == -3 ){
			BackupOffY = -2;
			return;
		}
		BackupOffY = VOi->offY;
		BackupSelY = VOsel.now.y.line;
		BackuplastY = VOsel.lastY;
	}

	if ( VOsel.VSstring[0] == '\0' ){
		VOsel.highlight = FALSE;
		InvalidateRect(vinfo->info.hWnd,NULL,TRUE);
	}else{
		#ifdef UNICODE
			UnicodeToAnsi(VOsel.VSstringW,VOsel.VSstringA,VFPS);
		#else
			AnsiToUnicode(VOsel.VSstringA,VOsel.VSstringW,VFPS);
		#endif

		if ( VOsel.cursor != FALSE ){
			MoveCsrkey(0,BackupSelY - VOsel.now.y.line,FALSE);
		}else{
			MoveCsrkey(0,BackupOffY - VOi->offY,FALSE);
		}
		VOsel.lastY = BackuplastY;
		DoFind(vinfo->info.hWnd,2);
	}
}

void GetSelectText(TCHAR *destptr)
{
	const TCHAR *src;
	TMS_struct text = {{NULL,0,NULL},0};
	TCHAR *dest, *maxptr;

	if ( vo_.DModeBit & VO_dmode_SELECTABLE ){
		ClipMem(&text,-1,-1);
		src = text.p ? text.tm.p : NilStr;
		dest = destptr;
		maxptr = destptr + CMDLINESIZE / 2;
		while ( dest < maxptr ){
			TCHAR c;

			c = *src++;
			if ( c == '\r' ) continue;
			if ( c == '\n' ) continue;
			if ( c == '\0' ) break;
			*dest++ = c;
		}
		*dest = '\0';
		TM_kill(&text.tm);
	}else{
		*destptr = '\0';
	}
}

LRESULT PPvPPxCommand(PPV_APPINFO *vinfo, WPARAM wParam, LPARAM lParam)
{
	switch ( LOWORD(wParam) ){
		case K_EXTRACT: {
			TCHAR *p;

			p = MapViewOfFile((HANDLE)lParam,
					FILE_MAP_ALL_ACCESS,0,0,CMDLINESIZE);
			if ( p == NULL ) break;

			PP_ExtractMacro(vinfo->info.hWnd,&vinfo->info,NULL,p,p,0);
			UnmapViewOfFile(p);
			CloseHandle((HANDLE)lParam);
			break;
		}

		case KV_FOCUS:
			ForceSetForegroundWindow(lParam ? (HWND)lParam : hViewReqWnd);
			break;

		case K_SETPOPMSG:
		case K_SETPOPLINENOLOG:
			if ( (TCHAR *)lParam == NULL ){
				StopPopMsg(PMF_ALL);
			}else{
				SetPopMsg(POPMSG_NOLOGMSG,(TCHAR *)lParam);
				UpdateWindow_Part(vinfo->info.hWnd);
			}
			return 1;

		case KE_seltext:
			GetSelectText((TCHAR *)lParam);
			break;

		case K_EDITCHANGE:
			Isearch(vinfo,(HWND)lParam);
			break;

/* XWIN_HIDETASK で一体化PPcに埋め込む為の試験コード
		case K_HIDE:
			if ( (hCommonWnd != NULL) && IsWindow(hCommonWnd) ){
				ShowWindow(hWnd,SW_HIDE);
				return 0;
			}
			PPvCommand(vinfo,LOWORD(wParam));
			return 0;
*/
		case KCW_captureEx:
			Embed = TRUE;
			return (LRESULT)TMAKELPARAM(1,KCW_captureEx);

		case KV_Load:
			hLastViewReqWnd = hViewReqWnd = (HWND)lParam;
		// default へ

		default:
			PPvCommand(vinfo,LOWORD(wParam));
	}
	return 0;
}

int CallKeyHook(WORD key)
{
	PPXMKEYHOOKSTRUCT keyhookinfo;
	PPXMODULEPARAM pmp;

	keyhookinfo.key = key;
	pmp.keyhook = &keyhookinfo;
	return CallModule(&vinfo.info,PPXMEVENT_KEYHOOK,pmp,KeyHookEntry);
}

void ExecDualParam(PPV_APPINFO *vinfo, const TCHAR *param)
{
	if ( (UTCHAR)*param == EXTCMD_CMD ){
		PP_ExtractMacro(vinfo->info.hWnd, &vinfo->info, NULL, param + 1, NULL, 0);
	}else{
		const WORD *key;

		if ( (UTCHAR)*param == EXTCMD_KEY ) param++;
		key = (WORD *)param;
		while( *key ) PPvCommand(vinfo, *key++);
	}
}

void PPvToolbarCommand(int id,int orcode)
{
	RECT box;
	const TCHAR *ptr;

	box.bottom = -1;
	SendMessage(hToolBarWnd,TB_GETRECT,(WPARAM)id,(LPARAM)&box);
	if ( box.bottom == -1 ){
		PopupPosType = PPT_MOUSE;
	}else{
		PopupPos.x = box.left;
		PopupPos.y = box.bottom;
		ClientToScreen(hToolBarWnd,&PopupPos);
		PopupPosType = PPT_SAVED;
	}
	ptr = GetToolBarCmd(hToolBarWnd,&thGuiWork,id);
	if ( ptr == NULL ) return;
	if ( orcode ){
		WORD key;

		key = *(WORD *)(ptr + 1) | (WORD)orcode;
		if ( key == (K_s | K_raw | K_bs) ){
			key = K_raw | K_c | K_bs;
		}
		PPvCommand(&vinfo,key);
	}else{
		ExecDualParam(&vinfo, ptr);
	}
}

LRESULT PPvNotify(NMHDR *nmh)
{
	if ( nmh->hwndFrom == NULL ) return 0;
	if ( nmh->hwndFrom == hToolBarWnd ){
		if ( nmh->code == TBN_DROPDOWN ){
			PPvToolbarCommand(((LPNMTOOLBAR)nmh)->iItem,K_s);
		}
		if ( nmh->code == NM_RCLICK ){
			PopupPosType = PPT_MOUSE;
			PPvLayoutCommand();
		}
		return 0;
	}
	if ( nmh->code == TTN_NEEDTEXT ){
		if ( SetToolBarTipText(hToolBarWnd,&thGuiWork,nmh) ) return 0;
		return 0;
	}
	return 0;
}
//-----------------------------------------------------------------------------
#define RANGEOFFSET 1 // 少しずれても選べるようにする大きさ
#define COffX ParamX	// istate == SELPOSCHANGE_OFFSET なら、カーソルX移動量
#define CPosX ParamX	// istate == SELPOSCHANGE_PIX なら、ポイントX座標
int CalcTextXPoint(int ParamX, int csrY, int istate)
{
	char buf[TEXTBUFSIZE],*p;
#ifdef UNICODE
	TCHAR wbuf[TEXTBUFSIZE];
#endif
	int charX = 0;	// 現在の offset
	int MaxcX = MAX_MOVE_X; // 最大offset
	int PixX = 0;	// 現在の picel
	int leftX C4701CHECK;	// 残り幅
	int XV_bctl3bk;
	HDC hDC;
	HFONT hUseFont;
	HGDIOBJ hOldFont;
	SIZE textsize;
	MAKETEXTINFO mti;

	if ( istate == SELPOSCHANGE_PIX ){ // 左マージンと行番号の幅を除去
		CPosX -= (XV_left + XV_lleft) - VOi->offX * fontX;
		leftX = CPosX + RANGEOFFSET;
		if ( leftX < 0 ) leftX = 0;
	}
	// HEX 表示 ---------------------------------------------------------------
	if ( vo_.DModeBit == DOCMODE_HEX ){
		int adrX;

		if ( istate == SELPOSCHANGE_PIX ){ // 位置指定
			if ( leftX <= (HEXADRWIDTH * fontX) ){ // アドレス部分  // C4701ok
				adrX = 0;
			}else if ( leftX <= ((HEXADRWIDTH + HEXNUMSWIDTH) * fontX) ){ //HEX
				if ( leftX > ((HEXADRWIDTH + ((HEXSTRWIDTH / 2) * HEXNWIDTH)) * fontX) ){
					leftX -= fontX; // 中央の余白を除去
				}
				adrX = (leftX - (HEXADRWIDTH * fontX)) / (HEXNWIDTH * fontX);
			}else{ // ASCII
				adrX = 16;
			}
		}else{ // 移動
			if ( (COffX < 0) && !VOsel.now.x.offset && VOsel.now.y.line ){
				adrX = 15;
				VOsel.now.y.line--;
				VOsel.now.y.pix -= LineY;
			}else if ( (COffX > 0) && (VOsel.now.x.offset >= (HEXNWIDTH * (HEXSTRWIDTH - 1) + 1)) && (VOsel.now.y.line < VO_maxY) ){
				adrX = 0;
				VOsel.now.y.line++;
				VOsel.now.y.pix += LineY;
			}else if ( COffX >= MAX_MOVE_X ){
				adrX = 16;
			}else{
				COffX += CalcHexX(VOsel.now.x.offset);
				adrX = COffX & (HEXSTRWIDTH - 1);
			}
		}
		VOsel.now.x.offset = adrX * HEXNWIDTH + ((adrX >= 8) ? 1 : 0);
		VOsel.now.x.pix = (HEXADRWIDTH + VOsel.now.x.offset) * fontX;
		return 0;
	}
	// TEXT 表示 --------------------------------------------------------------
	if ( csrY >= VOi->line ) csrY = VOi->line;

	if ( istate != SELPOSCHANGE_PIX){
		leftX = MAX_MOVE_X;
		MaxcX = COffX + VOsel.now.x.offset;
		if ( (COffX < 0) && VOsel.posdiffX ) MaxcX++; // 文字の途中にカーソルだ
		VOsel.posdiffX = 0;
	}
	XV_bctl3bk = XV_bctl[2];
	XV_bctl[2] = 0;

	mti.destbuf = (BYTE *)buf;
	mti.srcmax = vo_.file.image + vo_.file.size.l;
	mti.writetbl = FALSE;
	mti.paintmode = FALSE;

	VOi->MakeText(&mti,&VOi->ti[csrY]);
	XV_bctl[2] = XV_bctl3bk;

	// 表示に使用するフォントを選択
	hDC = GetDC(vinfo.info.hWnd);
	switch ( VOi->ti[csrY].type ){
		case VTYPE_IBM:
			hUseFont = GetIBMFont();
			break;

		case VTYPE_ANSI:
			hUseFont = GetANSIFont();
			break;

		default:
			hUseFont = XV_unff ? hUnfixedFont : hBoxFont;
			break;
	}
	hOldFont = SelectObject(hDC,hUseFont);

	p = buf;
	while( *p != VCODE_END ) switch(*p++){ // VCODE_SWITCH
		case VCODE_ASCII: 	// ASCII Text ---------------------------------
		case VCODE_CONTROL: // 制御文字 ---------------------------------
#ifdef UNICODE
		{
			int alength,wlength,wresult;

			alength = strlen32(p);
			// フォントリンク対策のため、一旦UNICODEにする
			wlength = MultiByteToWideChar(CP_ACP,0,p,alength,wbuf,TEXTBUFSIZE);

			if ( istate != SELPOSCHANGE_PIX ){ // カーソル移動：文字列内にいる
				if ( (charX <= MaxcX) && ((charX + alength) >= MaxcX) ){
					{ // 文字の中間の場合の補正
						char *fp,*maxp;

						fp = p;
						maxp = p + MaxcX - charX;
						while( fp < maxp ){
							if ( IskanjiA(*fp) ){
								if ( (fp + 1) == maxp ){
									if ( COffX > 0 ){
										MaxcX++;
									}else{
										MaxcX--;
									}
								}
								fp++;
							}
							fp++;
						}
					}
					GetTextExtentPoint32W(hDC,wbuf,
						MultiByteToWideChar(CP_ACP,0,p,MaxcX - charX,NULL,0),
						&textsize);
					PixX += textsize.cx;
					charX = MaxcX;
					goto fin;
				}
			}
			GetTextExtentExPointW(hDC,wbuf,wlength,leftX,&wresult,NULL,&textsize);
			leftX -= textsize.cx;
			if ( wresult < wlength ){ // 座標指定：文字列内に座標がある
				charX += WideCharToMultiByte(CP_ACP,0,wbuf,wresult,NULL,0,NULL,NULL);
				GetTextExtentPoint32W(hDC,wbuf,wresult,&textsize);
				PixX += textsize.cx;
				goto fin;
			}
			charX += alength;
			PixX += textsize.cx;
			p += alength + 1;
			break;
		}
#else
		{
			int length,result;
			int addw;

			addw = XV_unff ? 0 : fontWW;
			length = strlen(p);
			if ( istate != SELPOSCHANGE_PIX ){ // カーソル移動：文字列内にいる
				if ( (charX <= MaxcX) && ((charX + length) >= MaxcX) ){
					{ // 文字の中間の場合の補正
						char *fp,*maxp;

						fp = p;
						maxp = p + MaxcX - charX;
						while( fp < maxp ){
							if ( IskanjiA(*fp) ){
								PixX += addw;
								if ( (fp + 1) == maxp ){
									if ( COffX > 0 ){
										MaxcX++;
									}else{
										MaxcX--;
									}
								}
								fp++;
							}
							fp++;
						}
					}
					GetTextExtentPoint32A(hDC,p,MaxcX - charX,&textsize);
					PixX += textsize.cx;
					charX = MaxcX;
					goto fin;
				}
			}
			if ( addw ){ // 漢字幅の補正付計算
				if ( charX == MaxcX ) goto fin;
				while( *p ){
					int width;

					width = ChrlenA(*p);
					GetTextExtentPoint32A(hDC,p,width,&textsize);
					if ( width > 1) textsize.cx += addw;
					leftX -= textsize.cx;
					if ( leftX < 0 ) goto fin;
					PixX += textsize.cx;
					charX += width;
					if ( charX >= MaxcX ) goto fin;
					p += width;
				}
				p++; // NUL skip
				break;
			}
			GetTextExtentExPointA(hDC,p,length,leftX,&result,NULL,&textsize);

			leftX -= textsize.cx;
			charX += result;
			if ( result < length ){ // 座標指定：文字列内に座標がある
				GetTextExtentPoint32A(hDC,p,result,&textsize);
				PixX += textsize.cx;
				goto fin;
			}
			PixX += textsize.cx;
			p += length + 1;
			break;
		}
#endif
		case VCODE_UNICODEF:
			p++;
		case VCODE_UNICODE:		// UNICODE Text ---------------------------
#ifdef UNICODE
		{
			int length,result;

			length = strlenW32((WCHAR *)p);
			if ( istate != SELPOSCHANGE_PIX ){ // カーソル移動：文字列内にいる
				if ( (charX <= MaxcX) && ((charX + length) >= MaxcX) ){
					GetTextExtentPoint32W(hDC,(WCHAR *)p,MaxcX - charX,&textsize);
					PixX += textsize.cx;
					charX = MaxcX;
					goto fin;
				}
			}
			GetTextExtentExPointW(hDC,(WCHAR *)p,length,leftX,&result,NULL,&textsize);
			leftX -= textsize.cx;
			charX += result;
			if ( result < length ){ // 座標指定：文字列内に座標がある
				GetTextExtentPoint32W(hDC,(WCHAR *)p,result,&textsize);
				PixX += textsize.cx;
				goto fin;
			}
			PixX += textsize.cx;
			p += (length + 1) * sizeof(WCHAR);
			break;
		}
#else
		{
			if ( charX == MaxcX ) goto fin;
			while(*(WORD *)p){
				GetTextExtentPoint32W(hDC,(WCHAR *)p,1,&textsize);
				leftX -= textsize.cx;
				if ( leftX < 0 ) goto fin;
				PixX += textsize.cx;
				charX++;

				if ( charX == MaxcX ) goto fin;
				p += 2;
			}
			p += 2; // NUL skip
			break;
		}
#endif
		case VCODE_COLOR:			// Color -----------------------------
			p += 2;
			break;
		case VCODE_BCOLOR:			// Color -----------------------------
		case VCODE_FCOLOR:			// Color -----------------------------
			p += 4;
			break;

		case VCODE_FONT:			// Font ------------------------------
			p++;
			break;

		case VCODE_TAB:{			// Tab -------------------------------
			int wide;
			int tabwidth;

			if ( charX == MaxcX ) goto fin;

			tabwidth = VOi->tab * fontX;
			wide = ((PixX / tabwidth) + 1) * tabwidth - PixX;
			leftX -= wide;
			if ( leftX <= 0 ){
				if ( leftX >= -(wide / 2) ){
					charX++;
					PixX += wide;
				}
				goto fin;
			}
			PixX += wide;
			charX++;
			break;
		}
		case VCODE_RETURN:		// CRLF/LF ------------------------------------
			p++;
			// 続く
		case VCODE_PAGE:		// New Page ----------------------------------
		case VCODE_PARA:		// New Paragraph -----------------------------
			if ( charX == MaxcX ) goto fin;
			leftX -= fontX;
			if ( leftX <= 0 ){
				goto fin;
			}
			// 改行は、範囲のカウントに含まれない
//			PixX += fontX;
//			charX++;
			break;

		case VCODE_SPACE:
			charX++;
		case VCODE_WSPACE:
			if ( charX == MaxcX ) goto fin;
#ifdef UNICODE
			GetTextExtentPoint32W(hDC,StrWideSpace,1,&textsize);
#else
			GetTextExtentPoint32A(hDC,StrSJISSpace,2,&textsize);
#endif
			leftX -= textsize.cx;
			if ( leftX <= 0 ){
				goto fin;
			}
			PixX += textsize.cx;
			charX++;
			break;

		case VCODE_LINK:			// Link ------------------------------
			break;

		default:		// 未定義コード -----------------------------------
			p = "";
			break;
	}
fin:
	SelectObject(hDC,hOldFont);
	ReleaseDC(vinfo.info.hWnd,hDC);

	VOsel.now.x.offset = charX;
	VOsel.now.x.pix = PixX;
	//
	if ( istate == SELPOSCHANGE_PIX ) VOsel.posdiffX = CPosX - PixX;
	if ( (MaxcX < MAX_MOVE_X) && (MaxcX > charX) ) return 1; // 次の行へ
	return 0;
}
#undef PosX
#undef COffX

void Ascroll(HWND hWnd,BOOL reverse)
{
	POINT pos;
	int x = 0,y = 0;

	GetCursorPos(&pos);
	ScreenToClient(hWnd,&pos);

	if ( pos.y < (BoxView.top + LineY) ){
		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			y = -(pos.y - (BoxView.top + LineY));
		}else{
			y = 2 - ((pos.y - BoxView.top) * 2) / LineY;
		}
	}else if ( pos.y >= (BoxView.bottom - (LineY * 2)) ){
		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			y = -(pos.y - (BoxView.bottom - (LineY * 2)));
		}else{
			y = -4 - ((pos.y - BoxView.bottom) * 2) / LineY;
		}
	}
	if ( pos.x < fontX ){
		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			x = fontX - pos.x;
		}else{
			x = 2 - (pos.x * 2) / fontX - 1;
		}
	}else if ( pos.x >= (WndSize.cx - fontX) ){
		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			x = (WndSize.cx - fontX) - pos.x;
		}else{
			x = 1 - ( (pos.x - (WndSize.cx - fontX)) * 2) / fontX;
		}
	}
	if (reverse){
		x = -x;
		y = -y;
	}
	if ( x || y ){
		MoveCsr(x,y,FALSE);
		if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
	}
}

int GetPointY(int posY)
{
	int pointY;

	if ( vo_.DModeBit & VO_dmode_TEXTLIKE ){
		if ( posY < BoxView.top ){
			pointY = -2;
		}else if ( posY >= WndSize.cy ){
			pointY = -3;
		}else{
			pointY = ((posY - BoxView.top) / LineY) + VOi->offY;
			if ( pointY >= VOi->line ) pointY = -4;
		}
	}else{
		pointY = -1;
	}
	return pointY;
}

void SetCursorCaret(SELPOSINFO *posinfo)
{
	if ( GetFocus() == vinfo.info.hWnd ){
		SetCaretPos(posinfo->x.pix - (VOi->offX * fontX) + XV_left + XV_lleft,
				posinfo->y.pix - (VOi->offY * LineY) + BoxView.top);
	}
	DNotifyWinEvent(EVENT_OBJECT_FOCUS,vinfo.info.hWnd,OBJID_CLIENT,posinfo->y.line + 1);
}

void MoveCsrkey(int offx, int offy, BOOL select)
{
	int newY;

	if ( vo_.DModeBit == DOCMODE_NONE ) return;

	if ( (VOsel.cursor == FALSE) || !(vo_.DModeBit & VO_dmode_TEXTLIKE) ){
		// ページモード／カーソル非表示
		MoveCsr(offx, offy, FALSE);
		if ( GetFocus() == vinfo.info.hWnd ){
			SetCaretPos(0, (LineY >> 1) + BoxView.top); // アクセシビリティ通知用
		}
		DNotifyWinEvent(EVENT_OBJECT_FOCUS, vinfo.info.hWnd, OBJID_CLIENT, VOi->offY + 1);
		return;
	}
									// カーソルモード／カーソル表示中
	if ( select == FALSE ){ // 非選択モード
		if ( VOsel.select != FALSE ){ // 選択中なら解除
			ResetSelect(FALSE);
			InvalidateRect(vinfo.info.hWnd, NULL, TRUE);
			if ( VOsel.cursor != FALSE ){	// カーソルが外なら呼び戻す
				if ( VOsel.now.y.line < VOi->offY ){
					VOsel.now.y.line = VOi->offY;
				}
				if ( VOsel.now.y.line >= (VOi->offY + VO_sizeY - 1) ){
					VOsel.now.y.line = (VOi->offY + VO_sizeY - 1);
				}
			}
		}
	}else{ // 選択モード
		if ( VOsel.select == FALSE ){ // 非選択なら初期化
			VOsel.select = TRUE;
			VOsel.start = VOsel.now;
			// FixSelectRange は後で実行されるので不要
			InvalidateRect(vinfo.info.hWnd, NULL, FALSE);
		}
	}
									// y 範囲検査
	newY = VOsel.now.y.line + offy;
	if ( newY >= VOi->line ) newY = VOi->line - 1;
	if ( newY < VO_minY ) newY = VO_minY;

									// 結果として位置がずれた場合、更新
	if ( offx || (newY != VOsel.now.y.line) ){
		RECT box;
		int oldY;
		BOOL newlinemode;

		newlinemode = offx ? FALSE : X_vzs;
		if ( VOsel.linemode != newlinemode ){
			VOsel.linemode = newlinemode;
			InvalidateRect(vinfo.info.hWnd, NULL, FALSE);
		}
		// 表示を更新する領域を決定
		oldY = VOsel.now.y.line;

		VOsel.now.y.pix  = newY * LineY;
		VOsel.now.y.line = newY;
		if ( newY != oldY ){ // Y変更
			int oldpixx;

			oldpixx = VOsel.now.x.pix;
			CalcTextXPoint(VOsel.now.x.pix + XV_left + XV_lleft - VOi->offX * fontX, newY, SELPOSCHANGE_PIX);
			VOsel.now.x.pix = oldpixx; // キャレットのX座標を維持する
			// ↓選択しているときは、キャレット位置の補正をした方がよいか？
//			if ( (VOsel.select != FALSE) && !X_vzs ) VOsel.posdiffX = 0;
		}else{ // X変更
			if ( (vo_.DModeBit & DOCMODE_TEXT) && ((VOsel.now.x.offset + offx) < 0) ){ // 上の行に移動する？
				if ( VOsel.now.y.line <= VO_minY ){ // 最上位行
					VOsel.now.x.offset = 0;
					VOsel.now.x.pix = 0;
				}else{
					VOsel.now.y.line--;
					VOsel.now.y.pix -= LineY;
					CalcTextXPoint(MAX_MOVE_X, VOsel.now.y.line, SELPOSCHANGE_PIX);
				}
				VOsel.posdiffX = 0;
			}else{
				if ( CalcTextXPoint(offx, newY, SELPOSCHANGE_OFFSET) ){
					if ( newY < (VOi->line - 1) ){ // 次の行に移動する
						VOsel.now.y.line++;
						VOsel.now.y.pix += LineY;
						VOsel.now.x.offset = 0;
						VOsel.now.x.pix = 0;
					}
				}
			}
		}
		newY = VOsel.now.y.line;
		if ( VOsel.select != FALSE ){
			FixSelectRange();
			if ( newY < oldY ){
				box.top =    (newY - VOi->offY) * LineY + BoxView.top;
				box.bottom = (oldY - VOi->offY + 1) * LineY + BoxView.top;
			}else{
				box.top =    (oldY - VOi->offY) * LineY + BoxView.top;
				box.bottom = (newY - VOi->offY + 1) * LineY + BoxView.top;
			}
		}else{
			box.top = (oldY - VOi->offY) * LineY + BoxView.top;
			box.bottom = box.top + LineY;
		}
		box.left = 0;
		box.right = WndSize.cx;
		InvalidateRect(vinfo.info.hWnd, &box, TRUE);
		if ( !FullDraw ){ // 旧カーソルを消去
			UpdateWindow_Part(vinfo.info.hWnd);
		}
		// 表示範囲チェック
		{
			int cX, leftbottom;

			cX = (XV_lleft + VOsel.now.x.pix + XV_left) / fontX;
			if ( vo_.DModeBit == DOCMODE_HEX ){
				leftbottom = HEXADRWIDTH;
			}else{
				leftbottom = XV_lleft / fontX;
			}
			if ( cX < (VOi->offX + leftbottom + CARETSCROLLMARGINX) ){ // 表示範囲より左
				int delta = cX - (VOi->offX + leftbottom + CARETSCROLLMARGINX);
				if ( delta < offx ) offx = delta;
			}else if ( cX >= (VOi->offX + VO_sizeX - 1 - CARETSCROLLMARGINX) ){ // 表示範囲より右
				int delta = cX - (VOi->offX + VO_sizeX - 1 - CARETSCROLLMARGINX);
				// カーソルの表示X をできるだけ変えないように調整
				if ( (delta > offx) || (offx == MAX_MOVE_X) ){
					offx = delta;
				}
			}else{ // 表示範囲内
				offx = 0;
			}
		}
		if ( newY < (VOi->offY + CARETSCROLLMARGINY) ){ // 表示範囲より上
			// カーソルの表示Y をできるだけ変えないように調整
			int delta = newY - (VOi->offY + CARETSCROLLMARGINY);
			if ( delta < offy ) offy = delta;
		}else if ( newY >= (VOi->offY + VO_sizeY - 2 - CARETSCROLLMARGINY) ){ // 表示範囲より下
			// カーソルの表示Y をできるだけ変えないように調整
			int delta = newY - (VOi->offY + VO_sizeY - 2 - CARETSCROLLMARGINY);
			if ( delta > offy ) offy = delta;
		}else{
			offy = 0;
		}
		if( offy || offx ){ // スクロール有り
			InvalidateRect(vinfo.info.hWnd, &box, FALSE);
			MoveCsr(offx, offy, FALSE);	// スクロール
		}
		box.top = (newY - VOi->offY) * LineY + BoxView.top;
		box.bottom = box.top + LineY;
		InvalidateRect(vinfo.info.hWnd, &box, FALSE); // 新カーソルを表示
		if ( !FullDraw ) UpdateWindow_Part(vinfo.info.hWnd);
		InvalidateRect(vinfo.info.hWnd, &BoxStatus, FALSE); // ステータス変更
		if ( FullDraw ) UpdateWindow_Part(vinfo.info.hWnd);
	}
	SetCursorCaret(&VOsel.now);
}

#pragma argsused
void CALLBACK DragProc(HWND hWnd,UINT msg,UINT_PTR id,DWORD work)
{
	UnUsedParam(msg);UnUsedParam(work);

	switch (CursorTimerMode){
		case CSRTM_OUT:	// 範囲外
			if ( !X_dds && (VOsel.select == FALSE) ) break;
			Ascroll(hWnd,FALSE);
			return;

		case CSRTM_INERTIA:	// 慣性処理
			MoveCsr(-OffPos.cx,-OffPos.cy,FALSE);
			if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
			OffPos.cx /= 2; OffPos.cy /= 2;
			if ( !OffPos.cx && !OffPos.cy ){
				CursorTimerMode = CSRTM_OFF;
				KillTimer(hWnd,id);
			}
			break;

		case CSRTM_SELECT:{	// 範囲選択
			POINT pos;
			int pointX,oldline,newline,oldoffset;

			if ( VOsel.select == FALSE ) break;
			Ascroll(hWnd,TRUE);
			GetCursorPos(&pos);
			ScreenToClient(hWnd,&pos);
			#ifdef USEDIRECTWRITE
			{
				LPARAM lParam = TMAKELPARAM(pos.x,pos.y);
				DxTransformPoint(DxDraw,&lParam);
				pos.x = (short)LOWORD(lParam);
				pos.y = (short)HIWORD(lParam);
			}
			#endif
										// y 計算
			oldline = VOsel.now.y.line;
			newline = GetPointY(pos.y);
			switch( newline ){
				case -1:	// テキストでない
					ResetSelect(TRUE);
					return;
				case -2:	// 上にはみ出している
					newline = VOi->offY;
					if ( VOsel.linemode == FALSE ){
						InvalidateRect(hWnd,NULL,FALSE);
					}
					VOsel.linemode = X_vzs;
					break;
				case -3:	// 下にはみ出している
				case -4:
					if ( VOsel.linemode == FALSE ){
						InvalidateRect(hWnd,NULL,FALSE);
					}
					VOsel.linemode = X_vzs;
					newline = VOi->offY + VO_sizeY - 1;
					if ( newline >= VOi->line ){
						newline = VOi->line - 1;
					}
				break;
			}
										// x 計算
			VOsel.now.y.pix = newline * LineY;
			VOsel.now.y.line = newline;

			oldoffset = VOsel.now.x.offset;
			CalcTextXPoint(pos.x,newline,SELPOSCHANGE_PIX);
			SetCursorCaret(&VOsel.now);
			pointX = (pos.x / fontX) - OldPointCX; // Xのポイント位置の移動量を算出
			OldPointCX += pointX; // 反映
			if ( oldoffset != VOsel.now.x.offset ) pointX = 1;
										// 表示更新
			if ( oldline != VOsel.now.y.line ){
				RECT box;

				if ( VOsel.linemode == FALSE ) InvalidateRect(hWnd,NULL,FALSE);
				VOsel.linemode = X_vzs;
				box.left = 0;
				box.right = WndSize.cx;
				if ( oldline < VOsel.now.y.line ){
					box.top = (oldline - VOi->offY) * LineY + BoxView.top;
					box.bottom = (VOsel.now.y.line - VOi->offY + 1) * LineY + BoxView.top;
				}else{
					box.top = (VOsel.now.y.line - VOi->offY) * LineY + BoxView.top;
					box.bottom = (oldline - VOi->offY + 1) * LineY + BoxView.top;
				}
				InvalidateRect(hWnd,&box,TRUE);
			}else{
				if ( pointX ) VOsel.linemode = FALSE;
			}
			if ( !VOsel.linemode && pointX ){
				InvalidateRect(hWnd,NULL,FALSE);
			}
			FixSelectRange();
			return;
		}
	}
}

BOOL USEFASTCALL WmCopyData(HWND hWnd,COPYDATASTRUCT *copydata)
{
	switch(LOWORD(copydata->dwData)){
		case 'H':{
			TCHAR cmd[0x1000];

			if ( copydata->cbData >= sizeof(cmd) ) return FALSE;
			tstrcpy(cmd,(TCHAR *)copydata->lpData);
			ReplyMessage(TRUE);
			PP_ExtractMacro(hWnd,&vinfo.info,NULL,cmd,NULL,0);
			return TRUE;
		}
	}
	return FALSE;
}

void USEFASTCALL DoDropFiles(HWND hWnd,HDROP hDrop)
{
	if ( VO_PrintMode == PRINTMODE_FILES ){
		if ( IsIconic(hWnd) || !IsWindowVisible(hWnd) ){
			PostMessage(hWnd,WM_SYSCOMMAND,SC_RESTORE,0xffff0000);
		}
		InvalidateRect(hWnd,NULL,TRUE);
		SetForegroundWindow(hWnd);
		PPVPrintFiles(hWnd,hDrop);
	}else{
		TCHAR name[VFPS];

		DragQueryFile(hDrop,0,name,TSIZEOF(name));
		DragFinish(hDrop);
		OpenAndFollowViewObject(&vinfo,name,NULL,NULL,0);
		SetForegroundWindow(hWnd);
		if ( VO_PrintMode == PRINTMODE_ONE ) PPVPrint(hWnd);
	}
}

void WmWindowPosChanged(HWND hWnd)
{
	WINDOWPLACEMENT wp;

	PosChangeEntry++;
	if ( PosChangeEntry > 1 ) return; // 再入有
	do {
		wp.length = sizeof(wp);
		GetWindowPlacement(hWnd,&wp);
		WinPos.show = (BYTE)wp.showCmd;

		GetClientRect(hWnd,&wp.rcNormalPosition);

		if ( ((wp.showCmd != SW_SHOWMINIMIZED) && (wp.showCmd != SW_HIDE)) &&
			 ( (WndSize.cx != wp.rcNormalPosition.right) ||
			   (WndSize.cy != wp.rcNormalPosition.bottom) ) ){
			WndSize.cx = wp.rcNormalPosition.right;
			WndSize.cy = wp.rcNormalPosition.bottom;
			FreeOffScreen(&BackScreen);
			InvalidateRect(hWnd,NULL,TRUE);
		}

		GetWindowRect(hWnd,&winS);
		if ( !OpenEntryNow && wp.showCmd == SW_SHOWNORMAL ){
			if ( (XV.img.imgD[0] <= IMGD_FIXWINDOWSIZE) &&
				 (  (vo_.DModeBit == DOCMODE_NONE ) ||
					(vo_.DModeBit & VO_dmode_IMAGE) )	){
				// 窓枠大きさが可変の時は前の大きさを維持する
				WinPos.pos.right = winS.left + (WinPos.pos.right - WinPos.pos.left);
				WinPos.pos.bottom = winS.top + (WinPos.pos.bottom - WinPos.pos.top);
				WinPos.pos.left = winS.left;
				WinPos.pos.top = winS.top;
			}else{
				WinPos.pos = winS;
			}
		}

		BoxStatus.left = 0;
		BoxStatus.top = 0;
		BoxStatus.right = WndSize.cx;
		BoxStatus.bottom = X_win & XWIN_NOSTATUS ? 0 : LineY;

		BoxView.left = 0;
		BoxView.top = BoxStatus.bottom;
		BoxView.right = WndSize.cx;
		BoxView.bottom = WndSize.cy;

		if ( hToolBarWnd != NULL ){
			RECT box;

			GetWindowRect(hToolBarWnd,&box);
			SetWindowPos(hToolBarWnd,NULL,0,BoxStatus.bottom,
					WndSize.cx,box.bottom - box.top,
					SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOZORDER);

			// 横幅が変化するときは、ツールバーが下にずれることがあるため、
			// もう一度位置設定を行う
			SetWindowPos(hToolBarWnd,NULL,0,BoxStatus.bottom,
					WndSize.cx,box.bottom - box.top,
					SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOZORDER);
			BoxView.top += box.bottom - box.top;
		}

		if ( VOi->defwidth == WIDTH_AUTOFIX ){
			int newwidth = FixedWidthRange(VOi->defwidth);

			if ( newwidth != VOi->width ){
				VOi->width = newwidth;
				ReMakeIndexes(hWnd); // ここで再入が起きる
			}
		}

		BoxAllView = BoxView;
		if ( Use2ndView ){
			Box2ndView = BoxView;
			if ( Use2ndView == 1 ){ // 左右に並べる
				BoxView.right = (BoxView.left + BoxView.right) / 2;
						// BoxView.left + (BoxView.right - BoxView.left) / 2;
				Box2ndView.left = BoxView.right;
			}else{ // 上下に並べる
				BoxView.bottom = (BoxView.top + BoxView.bottom) / 2;
				Box2ndView.top = BoxView.bottom;
			}
		}

		SetScrollBar(); // ここで再入が起きる
		if ( X_fles || BackScreen.X_WallpaperType ){
			InvalidateRect(hWnd,NULL,TRUE);
		}

		if ( (wp.showCmd == SW_SHOWMINIMIZED) && (X_tray & X_tray_PPv) ){
			PostMessage(hWnd,WM_PPXCOMMAND,K_HIDE,0);
		}
		ChangeSizeDxDraw(DxDraw,C_back);
		if ( PosChangeEntry > 4 ) PosChangeEntry = 1; // 過度の再入
	}while ( --PosChangeEntry );
}

LRESULT WMGesture(HWND hWnd,WPARAM wParam,LPARAM lParam)
{
	if ( TouchMode == 0 ){
		if ( X_pmc[0] < 0 ) PPvEnterTabletMode(hWnd);
	}
	{
		switch ( wParam ){
			case GID_TWOFINGERTAP:
				PostMessage(hWnd,WM_PPXCOMMAND,K_apps,0);
				break;

			case GID_PRESSANDTAP:
				PostMessage(hWnd,WM_PPXCOMMAND,K_apps,0);
				break;
		}
	}
	return DefWindowProc(hWnd,WM_GESTURE,wParam,lParam);
}

void WMDpiChanged(HWND hWnd,WPARAM wParam,RECT *newpos)
{
	int newDPI = HIWORD(wParam);
	HDC hDC;

	if ( !(X_dss & DSS_ACTIVESCALE) ) return;
	if ( FontDPI == newDPI ) return; // 変更無し(起動時等)

	if ( vo_.DModeBit & VO_dmode_IMAGE ){ // 画像
		if ( (XV.img.imgD[0] > 0) && (XV.img.imgD[0] != 100) ){
			XV.img.imgD[0] = (XV.img.imgD[0] * newDPI) / FontDPI;
		}
	}

	FontDPI = newDPI;

	DeleteFonts();
	hDC = GetDC(hWnd);
	MakeFonts(hDC,X_textmag);
	ReleaseDC(hWnd,hDC);

	if ( newpos != NULL ){
		SetWindowPos(hWnd,NULL,newpos->left,newpos->top,
				newpos->right - newpos->left, newpos->bottom - newpos->top,
				SWP_NOACTIVATE | SWP_NOZORDER);
	}

	WmWindowPosChanged(hWnd);
	InvalidateRect(hWnd,NULL,FALSE);
}

/*=============================================================================
	マウス関連
=============================================================================*/
void CheckGesture(MOUSESTATE *ms)
{
	TCHAR buf[GESTURETEXTSIZE];

	if ( PPxCheckMouseGesture(ms,buf,T("MV_click")) == FALSE ) return;
	SetPopMsg(POPMSG_NOLOGMSG,buf);
}

void PPvMouseMove(HWND hWnd,MOUSESTATE *ms,LPARAM lParam)
{
	POINT pos;

	//---------------------------------- 隠しメニュー処理(非ドラッグ時)
	if ( (ms->mode != MOUSEMODE_DRAG) && !(TouchMode & TOUCH_DISABLEHIDDENMENU) ){
		int menus = -1;

		LPARAMtoPOINT(pos,lParam);
		if ( (pos.x >= BoxStatus.left) &&
			 (pos.y >= BoxStatus.top) &&
			 (pos.y < BoxStatus.bottom) ){
			menus = pos.x / (fontX * 5);
			if ( menus >= XV.HiddenMenu.item ) menus = -1;
		}
		if ( menus != Mpos ){
			Mpos = menus;
			InvalidateRect(hWnd,&BoxStatus,TRUE);	// 更新指定
		}
		return;
	}

	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ) return;

	ClkTime = GetMessageTime();

	//---------------------------------- ジェスチャ
	if ( ms->PushButton == XV_DragGes ){
		if ( PtInRect(&ms->DDRect,ms->MovedScreenPoint) == FALSE ){
			CheckGesture(ms);
		}
		return;
	}

	//---------------------------------- 表示内容スクロール(左ドラッグ)
	if ( (ms->PushButton == XV_DragScr) &&
		 ((ms->PushButton != XV_DragSel) || (VOsel.select == FALSE)) ){
		pos = ms->MovedClientPoint;
		if ( pos.y >= BoxView.top ){
			if ( !X_dds || !((pos.y < (LineY + BoxView.top)) ||
					(pos.y >= (WndSize.cy - BoxView.top)) || (pos.x < fontX) ||
					(pos.x >= (WndSize.cx - fontX))) ){
				MoveCsr(-ms->MovedOffset.cx,-ms->MovedOffset.cy,FALSE);
				if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
			}
		}
		return;
	}
	//---------------------------------- ウィンドウドラッグ(中ドラッグ)
	if ( ms->PushButton == XV_DragWnd ){
		SetWindowPos(hWnd,NULL,
			winS.left + ms->MovedOffset.cx, winS.top + ms->MovedOffset.cy,
			0,0,SWP_NOSIZE | SWP_NOZORDER);
		return;
	}
}

void SetCursorIcon(HWND hWnd,LPCTSTR csr)
{
	HCURSOR hCsr;

	hCsr = LoadCursor(NULL,csr);
	SetClassLongPtr(hWnd,GCLP_HCURSOR,(LONG_PTR)hCsr);
	SetCursor(hCsr);
}

void CancelMouse(HWND hWnd)
{
	SetCursorIcon(hWnd,IDC_ARROW);
}

void USEFASTCALL RunGesture(PPV_APPINFO *vinfo, MOUSESTATE *ms)
{
	TCHAR buf[CMDLINESIZE];

	StopPopMsg(PMF_PROGRESS);
	wsprintf(buf,T("RG_%s"),ms->gesture.step);
	ms->gesture.count = 0;
	if ( NO_ERROR == GetCustTable(T("MV_click"),buf,buf,sizeof(buf)) ){
		ExecDualParam(vinfo, buf);
	}
}

void PPvDownMouse(HWND hWnd,MOUSESTATE *ms,WPARAM wParam)
{
	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ){
		CancelMouse(hWnd);
		if ( IsTrue(VOsel.select) ){
			ResetSelect(TRUE);
			InvalidateRect(hWnd,NULL,FALSE);
		}
		return;
	}
	if ( (ms->PushClientPoint.x < 0) ||
		 (ms->PushClientPoint.y < 0) ||
		 (ms->PushClientPoint.x >= WndSize.cx) ||
		 (ms->PushClientPoint.y >= WndSize.cy) ){
		PPxCancelMouseButton(ms);
		return;
	}
	KillTimer(hWnd,TIMERID_DRAGSCROLL);
	StopPopMsg(PMF_PROGRESS | PMF_WAITKEY);
	if ( (GetFocus() != hWnd) && (hViewParentWnd == NULL) ){
		PPxCancelMouseButton(ms);
		return;
	}
	if ( ms->PushButton == XV_DragGes ){
		ms->mode = MOUSEMODE_DRAG;
		ms->gesture.count = 0;
		return;
	}

	// 選択開始
	if ( ms->PushButton == XV_DragSel ){
		if ( vo_.DModeBit & VO_dmode_SELECTABLE ){
			int pointY = GetPointY(ms->PushClientPoint.y);

			if ( (ms->PushClientPoint.y >= BoxView.top) && (pointY >= 0) && (pointY < VOi->line) ){
				ms->mode = MOUSEMODE_DRAG;
				SetCursorIcon(NULL,IDC_IBEAM);

				VOsel.linemode = X_vzs;
				VOsel.cursor = TRUE;
				VOsel.select = TRUE;

				OldPointCX = ms->PushClientPoint.x / fontX;

				VOsel.now.y.pix  = pointY * LineY;
				VOsel.now.y.line = pointY;
				CalcTextXPoint(ms->PushClientPoint.x,pointY,SELPOSCHANGE_PIX);

				if ( !( (wParam & MK_SHIFT) && IsTrue(VOsel.cursor) ) ){
					VOsel.start = VOsel.now;
				}

				FixSelectRange();
				InvalidateRect(hWnd,NULL,FALSE);

				SetCursorCaret(&VOsel.now);
				UpdateWindow_Part(hWnd);

				CursorTimerMode = CSRTM_SELECT;
				SetTimer(hWnd,TIMERID_DRAGSCROLL,TIMER_DRAGSCROLL,DragProc);
			}
			return;
		}
	}
	//---------------------------------- 表示内容スクロール(左ドラッグ)
	if ( (ms->PushButton == XV_DragScr) || (ms->PushButton == XV_DragSel) ){
		if ( ms->PushClientPoint.y >= BoxView.top ){
			ms->mode = MOUSEMODE_DRAG;
			SetCursorIcon(NULL,IDC_SIZEALL);
			CursorTimerMode = CSRTM_OUT;
			SetTimer(hWnd,TIMERID_DRAGSCROLL,TIMER_DRAGSCROLL,DragProc);
		}
		return;
	}
	if ( ms->PushButton == MOUSEBUTTON_W ){
		if ( IsTrue(VOsel.select) ){
			CancelMouse(hWnd);
			ResetSelect(TRUE);
			InvalidateRect(hWnd,NULL,FALSE);
			PPxCancelMouseButton(ms);
			ms->mode = MOUSEMODE_NONE;
//			return;
		}

		if ( (ms->mode == MOUSEMODE_PUSH) && (WinPos.show == SW_SHOWNORMAL) ){
			ms->PushButton = XV_DragWnd;
		}
		return;
	}
}

void PPvUpMouse(HWND hWnd,MOUSESTATE *ms,int button,int oldmode,LPARAM lParam)
{
	TCHAR click[4];
	POINT pos;
	int posY;

	if ( button <= MOUSEBUTTON_CANCEL ) return;
	CancelMouse(hWnd);
	if ( GetFocus() != hWnd ) SetFocus(hWnd);
	LPARAMtoPOINT(pos,lParam);
	posY = pos.y;
	ClientToScreen(hWnd,&pos);

	click[0] = PPxMouseButtonChar[button];
	click[1] = '\0';

	PopupPos = pos;
	PopupPosType = PPT_SAVED;

	KillTimer(hWnd,TIMERID_DRAGSCROLL);
	if ( oldmode == MOUSEMODE_DRAG ){
		if ( button == XV_DragGes ){
			if ( ms->gesture.count ) RunGesture(&vinfo ,ms);
			return;
		}

		//---------------------------------- 表示内容スクロール(左ドラッグ)
		if ( (button == XV_DragScr) &&
			 ((button != XV_DragSel) || (VOsel.select == FALSE)) ){
							//※ timer overlap 問題は無視
			if ( (ClkTime + 60) > GetMessageTime() ){
				CursorTimerMode = CSRTM_INERTIA;
				OffPos = ms->MovedOffset;
				SetTimer(hWnd,TIMERID_DRAGSCROLL,TIMER_DRAGSCROLL,DragProc);
				return;
			}else{
				CursorTimerMode = CSRTM_OFF;
				if ( (ms->PushScreenPoint.x != pos.x) || (ms->PushScreenPoint.y != pos.y) ) return;
			}
		}
							// マウスの右クリック解除(w:ﾎﾞﾀﾝ,lH:Y,lL:X)--------
		if ( button == XV_DragSel ){
			if ( VOsel.select != FALSE ){
				int pointY;

				pointY = GetPointY(posY);
				switch(pointY){
					case -1:	// テキストでない
						ResetSelect(TRUE);
						break;
					case -2:	// 上にはみ出している
						pointY = VOi->offY;
						break;
					case -3:	// 下にはみ出している
					case -4:
						pointY = VOi->offY + VO_sizeY - 1;
						if ( pointY >= VOi->line ) pointY = VOi->line - 1;
						break;
				}
				if ( pointY >= 0 ) FixSelectRange();
				if ( pointY != VOsel.now.y.line ){
					RECT box;

					box.left = 0;
					box.right = WndSize.cx;
					if ( pointY < VOsel.now.y.line ){
						box.top = (pointY - VOi->offY) * LineY + BoxView.top;
						box.bottom = (VOsel.now.y.line - VOi->offY + 1) * LineY + BoxView.top;
					}else{
						box.top = (VOsel.now.y.line - VOi->offY) * LineY + BoxView.top;
						box.bottom = (pointY - VOi->offY + 1) * LineY + BoxView.top;
					}
					InvalidateRect(hWnd,&box,FALSE);
					UpdateWindow_Part(hWnd);
					VOsel.now.y.line = pointY;
				}
			}
			if ( !X_vzs &&
				 (VOsel.now.y.line   == VOsel.start.y.line ) &&
				 (VOsel.now.x.offset == VOsel.start.x.offset) ){
				ResetSelect(TRUE);
				InvalidateRect(hWnd,NULL,FALSE);
			}else{
				oldmode = MOUSEMODE_PUSH;
			}
		}
		if ( posY < BoxView.top ) posY = BoxView.top;
	}

	if ( (oldmode != MOUSEMODE_NONE) &&
		 (ms->PushTick > MOUSE_LONG_PUSH_TIME) ){ // 長押しチェック
		click[1] = 'H';
		click[2] = '\0';
		if ( IsTrue(PPvMouseCommandPos(&vinfo,&pos,click,posY)) ) return;
		click[1] = '\0';
	}

	if ( (button == MOUSEBUTTON_L) && (Mpos >= 0) &&
			(Mpos < XV.HiddenMenu.item) ){
		const TCHAR *p;

		p = HiddenMenu_cmd(XV.HiddenMenu.data[Mpos]);
		ExecDualParam(&vinfo, p);
		return;
	}
	if ( oldmode != MOUSEMODE_DRAG ){
		if ( IsTrue(PPvMouseCommandPos(&vinfo,&pos,click,posY)) ){
			if ( button == XV_DragSel ){
				if ( VOsel.select != FALSE ){
					ResetSelect(TRUE);
					InvalidateRect(hWnd,NULL,FALSE);
				}
			}
		}
	}
}

void DoubleClickMouse(HWND hWnd,MOUSESTATE *ms)
{
	TCHAR type[3];

	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ) return;

	CursorTimerMode = CSRTM_OFF;
	KillTimer(hWnd,TIMERID_DRAGSCROLL);
	if ( GetFocus() != hWnd ) SetFocus(hWnd);
	if ( (ms->PushClientPoint.x < 0) ||
		 (ms->PushClientPoint.y < 0) ||
		 (ms->PushClientPoint.x >= WndSize.cx) ||
		 (ms->PushClientPoint.y >= WndSize.cy) ){
		return;
	}
	type[0] = PPxMouseButtonChar[ms->PushButton];
	type[1] = 'D';
	type[2] = '\0';
	PPvMouseCommandPos(&vinfo,&ms->PushScreenPoint,type,ms->PushClientPoint.y);
}

void WheelMouse(HWND hWnd,MOUSESTATE *ms,WPARAM wParam,LPARAM lParam)
{
	int now;
	int X_wheel = 3;

	now = PPxWheelMouse(ms,hWnd,wParam,lParam);
	if ( now == 0 ) return;
	GetCustData(T("X_wheel"),&X_wheel,sizeof(X_wheel));
	CursorTimerMode = CSRTM_OFF;
	KillTimer(hWnd,TIMERID_DRAGSCROLL);
	CancelMouse(hWnd);

#ifdef USEDIRECTWRITE
	if ( GetAsyncKeyState(VK_MENU) & KEYSTATE_PUSH ){
		SetRotate(DxDraw,now * 2);
		InvalidateRect(hWnd,NULL,TRUE);
		return;
	}
#endif
	if ( wParam & MK_CONTROL ){ // Ctrl…フォントサイズ・透明度変更
		if ( VOsel.select != FALSE ){
			ResetSelect(TRUE);
			InvalidateRect(hWnd,NULL,TRUE);
		}

		if ( now != 0 ){
			if ( wParam & MK_SHIFT ){
				PPxCommonCommand(hWnd,0,(now > 0) ? (WORD)(K_c | K_s | K_v | VK_ADD) : (WORD)(K_c | K_s | K_v | VK_SUBTRACT));
			}else{
				SetMag( (now > 0) ? IMGD_OFFSET : -IMGD_OFFSET );
			}
		}
		return;
	}else if ( wParam & MK_LBUTTON ){
		MoveCsrkey(-now * X_wheel / 3 * VO_stepX, 0, VOsel.select);
	}else if ( wParam & MK_MBUTTON ){
		MoveCsrkey(0, -now * VO_sizeY / 3, VOsel.select);
	}else{
		if ( X_pppv == '1' ){
			HWND hPPcWnd;
			const TCHAR *p = T("C");

			hPPcWnd = (hLastViewReqWnd != NULL) ?
					hLastViewReqWnd : GetPPxhWndFromID(&vinfo.info,&p,NULL);
			if ( hPPcWnd != NULL ){
				PostMessage(hPPcWnd,WM_PPXCOMMAND,
					now < 0 ? (K_raw | K_dw) : (K_raw | K_up),0);
					PostMessage(hPPcWnd,WM_PPXCOMMAND,K_raw | 'N',0);
					return;
			}
		}

		MoveCsrkey(0, -now * X_wheel / 3 * VO_stepY, VOsel.select);
	}
}

void H_WheelMouse(PPV_APPINFO *vinfo, WPARAM wParam, LPARAM lParam)
{
	POINT pos;

	LPARAMtoPOINT(pos,lParam);
	PPvMouseCommandPos(vinfo, &pos,((short)HIWORD(wParam) < 0) ? T("H") : T("I"),pos.y);
}

LRESULT USEFASTCALL PPvWmCommand(HWND hWnd,WPARAM wParam,LPARAM lParam)
{
	if ( lParam ){
		if ( (HWND)lParam == hToolBarWnd ){
			PPvToolbarCommand(LOWORD(wParam),0);
			return 0;
		}
	}
	switch ( LOWORD(wParam) >> 12){
		case (IDW_MENU >> 12):
			PopupPosType = PPT_MOUSE;
			CommandDynamicMenu(&DynamicMenu,&vinfo.info,wParam);
			break;

		case 0xf: {				// f000-ffff System Menu ==============
			WORD cmdID;
			cmdID = (WORD)(wParam & 0xfff0);

			if ( (cmdID == SC_KEYMENU) || (cmdID == SC_MOUSEMENU) ){
				DynamicMenu.Sysmenu = TRUE;
			}

			if ( (cmdID == SC_MINIMIZE) && (X_tray & X_tray_PPv) ){
				PPxCommonCommand(hWnd,0,K_HIDE);
				break;
			}

			return DefWindowProc(hWnd,WM_SYSCOMMAND,wParam,lParam);
		}

		default:
			XMessage(NULL,NULL,XM_NiERRld,T("Unknown WM_COMMAND:%x"),wParam);
			break;
	}
	return 0;
}

void USEFASTCALL PPvWmHscroll(HWND hWnd,WORD scrollcode)
{
	switch ( scrollcode ){
								// 一番上 .............................
		case SB_TOP:
			MoveCsr(-VO_maxX,0,FALSE);
			break;
								// 一番下 .............................
		case SB_BOTTOM:
			MoveCsr(VO_maxX,0,FALSE);
			break;
								// 一行上 .............................
		case SB_LINEUP:
			MoveCsr(-VO_stepX,0,FALSE);
			break;
								// 一行下 .............................
		case SB_LINEDOWN:
			MoveCsr(VO_stepX,0,FALSE);
			break;
								// 一頁上 .............................
		case SB_PAGEUP:
			MoveCsr(1-VO_sizeX,0,FALSE);
			break;
								// 一頁下 .............................
		case SB_PAGEDOWN:
			MoveCsr(VO_sizeX-1,0,FALSE);
			break;
								// 特定位置まで移動中 .................
		case SB_THUMBTRACK:
			//SB_THUMBPOSITION へ続く
								// 特定位置まで移動 ...................
		case SB_THUMBPOSITION:{
			SCROLLINFO	scri;

			scri.cbSize = sizeof(scri);
			scri.fMask = SIF_TRACKPOS;
			GetScrollInfo(hWnd, SB_HORZ, &scri);
			MoveCsr(scri.nTrackPos - VOi->offX,0,FALSE);
			break;
		}
	}
	if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
}

void USEFASTCALL PPvWmVscroll(HWND hWnd,WORD scrollcode) // 垂直スクロール
{
	int delta = 0;

	switch ( scrollcode ){
										// 一番上 .............................
		case SB_TOP:
			delta = -VO_maxY;
			break;
										// 一番下 .............................
		case SB_BOTTOM:
			delta = VO_maxY;
			break;
										// 一行上 .............................
		case SB_LINEUP:
			delta = -VO_stepY;
			break;
										// 一行下 .............................
		case SB_LINEDOWN:
			delta = VO_stepY;
			break;
										// 一頁上 .............................
		case SB_PAGEUP:
			delta = 1 - VO_sizeY;
			break;
										// 一頁下 .............................
		case SB_PAGEDOWN:
			delta = VO_sizeY - 1;
			break;
										// 特定位置まで移動中 .................
		case SB_THUMBTRACK:
			//SB_THUMBPOSITION へ続く
										// 特定位置まで移動 ...................
		case SB_THUMBPOSITION:{
			SCROLLINFO scri;

			scri.cbSize = sizeof(scri);
			scri.fMask = SIF_ALL | SIF_TRACKPOS;
			GetScrollInfo(hWnd,SB_VERT,&scri);

			delta = scri.nTrackPos - VOi->offY;
			if ( scri.nPage && WndSize.cy ){
				int barrange;

				barrange = scri.nMax / scri.nPage;
				if ( barrange > WndSize.cy ){ // 詳細が必要？
					if ( (GetKeyState(VK_SHIFT) | GetKeyState(VK_RBUTTON)) & KEYSTATE_PUSH ){
						detailmode = TRUE;
					}
					if ( detailmode ){
						delta = (scri.nPos - VOi->offY)
								- ((scri.nMax / 2) - scri.nTrackPos) /
									(barrange / WndSize.cy);
					}
				}
			}
			break;
		}

		case SB_ENDSCROLL:
			if ( detailmode ){
				detailmode = FALSE;
				SetScrollBar();
			}
			break;
	}
	if ( delta ){
		MoveCsr(0,delta,detailmode);
		if ( VOsel.cursor != FALSE ) SetCursorCaret(&VOsel.now);
	}
}

/*=============================================================================
	WinMain
=============================================================================*/
#pragma argsused
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	MSG msg;
	int result;
	THREADSTRUCT threadstruct = {PPvMainThreadName,XTHREAD_ROOT,NULL,0,0};
	UnUsedParam(hPrevInstance);UnUsedParam(lpCmdLine);

	hInst = hInstance;
#if NODLL
	InitCommonDll((HINSTANCE)hInst);
#endif
	PPxRegisterThread(&threadstruct);
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
	WinPos.show = (BYTE)nShowCmd;
	PPvHeap = GetProcessHeap();

	if ( InitializePPv(&result) == FALSE ) return result;
	PPxCommonExtCommand(K_TBB_INIT,1);
#if USEDELAYCURSOR || SHOWFRAMERATE
	SetTimer(vinfo.info.hWnd, TIMERID_DRAW, TIMERRATE_DRAW, DrawTimerProc);
#endif
	if ( FirstCommand != NULL ){
		PostMessage(vinfo.info.hWnd,WM_PPXCOMMAND,K_FIRSTCMD,0);
	}
	if ( IsExistCustTable(T("KV_main"),T("FIRSTEVENT")) ){
		PostMessage(vinfo.info.hWnd,WM_PPXCOMMAND,K_E_FIRST,0);
	}
										// メインループ -----------------------
	while( (int)GetMessage(&msg,NULL,0,0) > 0 ){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if ( hComdlg32 != NULL ) FreeLibrary(hComdlg32);
	if ( hWinmm != NULL ) FreeLibrary(hWinmm);
	PPxCommonCommand(NULL,0,K_CLEANUP);
	CoUninitialize();
	return EXIT_SUCCESS;
}

/*=============================================================================
=============================================================================*/
LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch (message){
		case WM_NCCREATE:
			if ( (X_dss & DSS_COMCTRL) && (OSver.dwMajorVersion >= 10) ){
				PPxCommonCommand(hWnd,0,K_ENABLE_NC_SCALE);
			}
			return 1;

		case WM_COPYDATA:
			return WmCopyData(hWnd,(COPYDATASTRUCT *)lParam);

		case WM_DROPFILES:
			DoDropFiles(hWnd,(HDROP)wParam);
			break;

		case WM_MENUCHAR:
			if ( KeyChar ){
				KeyChar = 0;
				return 0;
			}
			// default なし
		case WM_MENUSELECT:
		case WM_MENUDRAG:
		case WM_MENURBUTTONUP:
			return PPxMenuProc(hWnd,message,wParam,lParam);

		case WM_INITMENU:
			DynamicMenu_InitMenu(&DynamicMenu,(HMENU)wParam,X_win & XWIN_MENUBAR);
			break;

		case WM_INITMENUPOPUP:
			if ( (hDispTypeMenu != NULL) && ((HMENU)wParam == hDispTypeMenu) ){
				PPvAddDisplayTypeMenu(hDispTypeMenu,FALSE);
				hDispTypeMenu = NULL;
				break;
			}
			DynamicMenu_InitPopupMenu(&DynamicMenu,(HMENU)wParam,&vinfo.info);
			break;

		case WM_SETFOCUS:
			if ( VOsel.cursor != FALSE ){
				CreateCaret(hWnd,NULL,fontX,fontY);
				SetCaretPos(
					VOsel.now.x.pix - (VOi->offX * fontX) + XV_left + XV_lleft,
					VOsel.now.y.pix - (VOi->offY * LineY) + BoxView.top);
				ShowCaret(hWnd);
			}else{
				CreateCaret(hWnd,NULL,0,0);
				SetCaretPos(0,(LineY >> 1) + BoxView.top);
			}
			if ( X_IME == 1 ) PostMessage(hWnd,WM_PPXCOMMAND,K_IMEOFF,0);
			if ( IsTrue(UseActiveEvent) ){
				PostMessage(hWnd,WM_PPXCOMMAND,K_E_ACTIVE,0);
			}
			if ( IsTrue(Embed) ){
				SendMessage(GetParent(hWnd),WM_PPXCOMMAND,KCW_focus,(LPARAM)hWnd);
			}
			InvalidateRect(hWnd,NULL,FALSE); // XPでゴミが残る問題の対策
			break;

		case WM_KILLFOCUS:
			DestroyCaret();
			break;
							// マウスの移動(w:ﾎﾞﾀﾝ,lH:Y,lL:X)---------
		case WM_MOUSEMOVE:
			DxTransformPoint(DxDraw,&lParam);
			ClkTime = GetMessageTime();
			PPxMoveMouse(&MouseStat,hWnd,lParam);
			PPvMouseMove(hWnd,&MouseStat,lParam);
			break;
							// マウスの左ダブルクリック(w:ﾎﾞﾀﾝ,lH:Y,lL:X)------
		case WM_NCLBUTTONUP:
		case WM_NCRBUTTONUP:
		case WM_NCMBUTTONUP:
		case WM_NCXBUTTONUP:
			if ( GetFocus() != hWnd ) break;
		case WM_NCLBUTTONDBLCLK:
		case WM_NCMBUTTONDBLCLK:
		case WM_NCRBUTTONDBLCLK:
		case WM_NCXBUTTONDBLCLK:
			return PPvNCMouseCommand(hWnd,message,wParam,lParam);

		case WM_NCRBUTTONDOWN:
			if ( wParam == HTCAPTION ) DynamicMenu.Sysmenu = TRUE;
		// WM_NCLBUTTONDOWN へ
		case WM_NCLBUTTONDOWN:
			PopupPosType = PPT_MOUSE;
			return DefWindowProc(hWnd,message,wParam,lParam);

							// マウスのXクリック(w:ﾎﾞﾀﾝ,lH:Y,lL:X)--------
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDOWN:
			DxTransformPoint(DxDraw,&lParam);
			PPxDownMouseButton(&MouseStat,hWnd,wParam,lParam);
			PPvDownMouse(hWnd,&MouseStat,wParam);
			break;

		case WM_CAPTURECHANGED:
			wParam = 0;
			lParam = 0;
			// WM_XBUTTONUP へ
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP: {
			int oldmode,i;

			oldmode = MouseStat.mode;
			DxTransformPoint(DxDraw, &lParam);
			i = PPxUpMouseButton(&MouseStat, wParam);
			PPvUpMouse(hWnd, &MouseStat, i, oldmode, lParam);
			break;
		}
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			DxTransformPoint(DxDraw,&lParam);
			PPxDoubleClickMouseButton(&MouseStat,hWnd,wParam,lParam);
			DoubleClickMouse(hWnd,&MouseStat);
			break;

		case WM_MOUSEWHEEL:
//			DxTransformPoint(DxDraw,&lParam); 不要
			WheelMouse(hWnd,&MouseStat,wParam,lParam);
			break;
							// ホイールの移動(左右,Vista以降)
		case WM_MOUSEHWHEEL:
			DxTransformPoint(DxDraw, &lParam);
			H_WheelMouse(&vinfo, wParam, lParam);
			break;

		case WM_MOUSEACTIVATE:
			if ( X_askp ){
				if ( GetFocus() != hWnd ) return MA_ACTIVATEANDEAT;
			}
			if ( IsTrue(Embed) ) SetFocus(hWnd);
			return DefWindowProc(hWnd,message,wParam,lParam);

							// 各種コントロール (w:ID,lH:message,lL:CtrlHandle)
		case WM_SYSCOMMAND:
		case WM_COMMAND:
			return PPvWmCommand(hWnd,wParam,lParam);

							// 窓状態の変更
		case WM_WINDOWPOSCHANGED:
			WmWindowPosChanged(hWnd);
			break;
							// 垂直スクロール
		case WM_VSCROLL:
			PPvWmVscroll(hWnd,LOWORD(wParam));
			break;
							// 水平スクロール
		case WM_HSCROLL:
			PPvWmHscroll(hWnd,LOWORD(wParam));
			break;
							// ALT+? が押された(w:仮想 key,lH:値,lL:repeat数)--
		case WM_SYSKEYDOWN:
			if ( !(lParam & B29) ) SetFocus(hWnd);
			// WM_KEYDOWN へ
							// key   が押された(w:仮想 key,lH:値,lL:repeat数)--
		case WM_KEYDOWN:{
			WORD key;

			StopPopMsg(PMF_ALL);
			KeyChar = 1;
										// [ALT] は メニュー移行を禁止
			key = (WORD)(wParam | GetShiftKey() | K_v);
			PopupPosType = PPT_FOCUS;

			if ( KeyHookEntry != NULL ){
				if ( CallKeyHook(key) != PPXMRESULT_SKIP ) break;
			}

			if ( PPvCommand(&vinfo,key) == ERROR_INVALID_FUNCTION ){
				if ( X_alt && (wParam == VK_MENU) ) break;
				return DefWindowProc(hWnd,message,wParam,lParam);
			}
			break;
		}
							// key   が押された(w:key,lH:値,lL:repeat数)--
		case WM_SYSCHAR:
		case WM_CHAR: {
			WORD key;

			KeyChar = 0;
			key = (WORD)wParam;

#ifndef UNICODE
			if ( multichar != FALSE ){
				multichar = FALSE;
				break;
			}
			if ( IskanjiA(key) ){
				multichar = TRUE;
				SetPopMsg(POPMSG_NOLOGMSG,MES_EKCD);
			}
#endif
			if ( key < 0x80 ){
				PopupPosType = PPT_FOCUS;

				if ( KeyHookEntry != NULL ){
					if ( CallKeyHook(FixCharKeycode(key)) != PPXMRESULT_SKIP ){
						break;
					}
				}

				if ( PPvCommand(&vinfo,FixCharKeycode(key)) == NO_ERROR ){
					break;
				}
			}
			return DefWindowProc(hWnd,message,wParam,lParam);
		}
							// 窓の再描画(w:未使用,l:未使用) ------------------
		case WM_PAINT:
			Paint(hWnd);
			break;

		case WM_CLOSE:
			if ( (hViewReqWnd != NULL) && (GetFocus() != FALSE) ){
				SetForegroundWindow(hViewReqWnd);
			}
			return DefWindowProc(hWnd,message,wParam,lParam);
							// 窓の破棄,終了処理(w:未使用,l:未使用) -----------
		case WM_DESTROY:
			ClosePPv();		// 必要な後処理を行う
			PostQuitMessage(EXIT_SUCCESS);	// これで終り
			break;
							// 強制終了要求(w:未使用,l:未使用) ----------------
// w=0 logoff   w != 0 ユーザによるこのソフトの強制終了
//		case WM_QUERYENDSESSION:
//			return (long)TRUE;	// TRUE ならば終了,FALSE なら終了の拒否
							// 強制終了動作の報告 -----------------------------
		case WM_ENDSESSION:
			if ( wParam ){	// TRUE:セッションの終了(WM_DESTROY は通知されない)
				PPxCommonExtCommand(K_REPORTSHUTDOWN,0);
				ClosePPv();		// 必要な後処理を行う
								// 時間がないのでここで全体の後始末もおこなう
				PostQuitMessage(EXIT_SUCCESS);
			}
			break;
//		case WM_PALETTEISCHANGING: // パレットの実体化を通知:処理不要

		case WM_PALETTECHANGED:
			if ( (HWND)wParam == hWnd) break;
			// WM_QUERYNEWPALETTE へ
		case WM_QUERYNEWPALETTE:
			if ( vo_.bitmap.hPal != NULL ){
				HDC hDC;
				HPALETTE hOldPal;
				int redraw;

				hDC = GetDC(hWnd);
				hOldPal = SelectPalette(hDC,vo_.bitmap.hPal,0);
				redraw = RealizePalette(hDC);
				SelectPalette(hDC,hOldPal,0);
				ReleaseDC(hWnd,hDC);
				if ( redraw ) InvalidateRect(hWnd,NULL,FALSE);
			}
			break;
/*
		case WM_SETTINGCHANGE:
			PPxCommonCommand(hWnd,lParam,K_SETTINGCHANGE);
			return DefWindowProc(hWnd,message,wParam,lParam);
*/
		case WM_SYSCOLORCHANGE:
			PPvCommand(&vinfo, K_Scust);
			PPvCommand(&vinfo, K_Lcust);
			FreeOffScreen(&BackScreen);
			break;

		case WM_DISPLAYCHANGE:{
			HDC hDC;

			hDC = GetDC(hWnd);
			VideoBits = GetDeviceCaps(hDC,BITSPIXEL);
			ReleaseDC(hWnd,hDC);
			ChangeSizeDxDraw(DxDraw,C_back); // DXで必要。DWでは不要
			InvalidateRect(hWnd,NULL,TRUE);
			break;
		}

		case WM_NOTIFY:
			return PPvNotify((NMHDR *)lParam);

//		case WM_DESTROYCLIPBOARD:	// 現在は TEXT,DIB のみのため、不要

		// 注意 WM_GETOBJECTのwParam/lParamは64bit環境では上位32bitにゴミが入る
		case WM_GETOBJECT:
			if ( ((LONG)lParam == OBJID_CLIENT) && X_iacc ){
				return WmGetObject((WPARAM)(DWORD)wParam);
			}
			return DefWindowProc(hWnd,message,wParam,lParam);

		case WM_GESTURE:
			return WMGesture(hWnd,wParam,lParam);

		case WM_DPICHANGED:
			WMDpiChanged(hWnd,wParam,(RECT *)lParam);
			break;

							// 知らないメッセージ -----------------------------
		default:
			if ( message == WM_PPXCOMMAND ){
				return PPvPPxCommand(&vinfo, wParam, lParam);
			}
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}
