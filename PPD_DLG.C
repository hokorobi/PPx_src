/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						ダイアログ関連
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "PPD_DEF.H"
#pragma hdrstop

const TCHAR *ClassAtomString[] = {
	BUTTONstr,EDITstr,STATICstr,LISTBOXstr,SCROLLBARstr,COMBOBOXstr
};

LPDLGTEMPLATE GetDialogTemplate(HWND hParentWnd,HANDLE hinst,LPCTSTR lpszTemplate)
{
	BYTE *dialog;
	HRSRC hRc;
	DWORD dialogsize,fontlength;
	int i;
	BYTE *dp,*dpitem;
	WCHAR fontdata[LF_FACESIZE + 1];
	UINT monitordpi,gdidpi;
									// フォント情報の作成
	LOGFONTWITHDPI BoxFont;

	monitordpi = GetMonitorDPI(hParentWnd);
	GetPPxFont(PPXFONT_F_dlg,monitordpi,&BoxFont);

	// fontlength : lfFaceName の文字列長(\0含む) + word(fontsize)
#ifdef UNICODE
	fontlength = (wcslen(BoxFont.font.lfFaceName) + 2) * sizeof(WCHAR);
	memcpy(&fontdata[1],BoxFont.font.lfFaceName,fontlength - sizeof(WCHAR));
#else
	fontlength = (MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,
			BoxFont.font.lfFaceName,-1,&fontdata[1],
			sizeof(fontdata) / sizeof(WCHAR) - 2) + 1) * sizeof(WCHAR);
#endif
									// テンプレートの読み込み
	hRc = FindResource(hinst,lpszTemplate,RT_DIALOG);
	if ( hRc == NULL ) return NULL;
	dialogsize = SizeofResource(hinst,hRc);
	dialog = HeapAlloc(ProcHeap,0,dialogsize + fontlength + sizeof(WORD));
	memcpy(dialog,LockResource(LoadResource(hinst,hRc)),dialogsize);

									// メニュー、クラス名、タイトル名を飛ばす
	dp = dialog + (sizeof(DWORD) * 2 + sizeof(WORD) * 5); // DLGTEMPLATE 相当

	if ( (X_dss & DSS_DIALOGREDUCE) &&
		 !((hinst == DLLhInst) &&
		   ((lpszTemplate == MAKEINTRESOURCE(IDD_INPUT)) ||
		 	(lpszTemplate == MAKEINTRESOURCE(IDD_INPUT_OPT)) ||
		 	(lpszTemplate == MAKEINTRESOURCE(IDD_INPUTREF)) )) ){

		int DlgWidth = *(WORD *)(dialog + sizeof(DWORD) * 2 + sizeof(WORD) * 3);
		int DlgHeight = *(WORD *)(dialog + sizeof(DWORD) * 2 + sizeof(WORD) * 4);
		int heightPt;
		RECT deskbox;
		int minHeight = (PPX_FONT_MIN_PT * monitordpi) / DEFAULT_WIN_DPI; // 約8pt

		heightPt = BoxFont.font.lfHeight >= 0 ? BoxFont.font.lfHeight : -BoxFont.font.lfHeight;
		GetDesktopRect(hParentWnd,&deskbox);

		#define DLGBASE_W 7 // 少し小さめにしている
		#define DLGBASE_H 8
		gdidpi = GetGDIdpi(NULL);
		// Pt 変換
		deskbox.right = ((deskbox.right - deskbox.left) * DEFAULT_DTP_DPI) / DEFAULT_WIN_DPI;
		deskbox.bottom = ((deskbox.bottom - deskbox.top) * DEFAULT_DTP_DPI) / DEFAULT_WIN_DPI;

		if ( ((OSver.dwMajorVersion > 10) ||
			 ((OSver.dwMajorVersion == 10) && (OSver.dwBuildNumber >= WINTYPE_10_BUILD_RS2) )) &&
			((monitordpi == gdidpi) && (gdidpi != DEFAULT_WIN_DPI)) ){ // Win10 RS2 以降は、既に補正がかかっているので、其の分調整
			deskbox.right = deskbox.right * DEFAULT_WIN_DPI / gdidpi;
			deskbox.bottom = deskbox.bottom * DEFAULT_WIN_DPI / gdidpi;
		}

		// ダイアログの横幅がはみ出すなら、縮める
		if ( ((DlgWidth * heightPt) / DLGBASE_W) > deskbox.right ){
			heightPt = (deskbox.right * DLGBASE_W) / DlgWidth;
			if ( heightPt < minHeight ) heightPt = minHeight;
			BoxFont.font.lfHeight = BoxFont.font.lfHeight >= 0 ? heightPt : -heightPt;
		}

		if ( ((DlgHeight * heightPt) / DLGBASE_H) > deskbox.bottom ){
			heightPt = (deskbox.bottom * DLGBASE_H) / DlgHeight;
			if ( heightPt < minHeight ) heightPt = minHeight;
			BoxFont.font.lfHeight = BoxFont.font.lfHeight >= 0 ? heightPt : -heightPt;
		}
	}
	// フォントサイズ(pixelでなく、Pt)
	fontdata[0] = (WCHAR)BoxFont.font.lfHeight;

	for ( i = 0 ; i < 3 ; i++ ){
		WORD id;

		id = *(WORD *)dp;
		if ( id == 0 ){			// なし
			dp += sizeof(WORD);
		}else if ( id == 0 ){	// ATOM
			dp += sizeof(WORD) * 2;
		}else {
			dp += sizeof(WORD);
			while( *dp ) dp += sizeof(WORD);
			dp += sizeof(WORD);
		}
	}
	dpitem = dp;
									// 既定のフォントがあれば飛ばす
	if ( ((DLGTEMPLATE *)dialog)->style & DS_SETFONT ){
		dpitem += sizeof(WORD);
		while( *dpitem ) dpitem += sizeof(WORD);
		dpitem += sizeof(WORD);
	}
								// dword 補正1
	if ( ALIGNMENT_BITS(dpitem) & 3 ) dpitem += sizeof(WORD);
								// dword 補正2
	if ( (ALIGNMENT_BITS(dp) + fontlength) & 3 ) fontlength += sizeof(WORD);
									// テンプレートの修正
	setflag(((DLGTEMPLATE *)dialog)->style,DS_SETFONT);
	memmove(dp + fontlength,dpitem,dialogsize - (dpitem - dialog));
	memcpy(dp,fontdata,fontlength);

	return (LPDLGTEMPLATE)dialog;
}

PPXDLL INT_PTR PPXAPI PPxDialogBoxParam(HANDLE hinst,const TCHAR *lpszTemplate,HWND hwndOwner,DLGPROC dlgprc,LPARAM lParamInit)
{
#if 1
	LPDLGTEMPLATE dialog;
	INT_PTR result;

	dialog = GetDialogTemplate(hwndOwner, hinst, lpszTemplate);
	if ( dialog == NULL ) return -1;
	result = DialogBoxIndirectParam(hinst, dialog, hwndOwner, dlgprc, lParamInit);
/*
#ifdef WINEGCC
	if ( (result == -1) && (GetLastError() == ERROR_OPEN_FAILED) ){
		result = DialogBoxParam(hinst, lpszTemplate, hwndOwner, dlgprc, lParamInit);
	}
#endif
*/
	HeapFree(ProcHeap, 0, dialog);
	return result;
#else
	return DialogBoxParam(hinst, lpszTemplate, hwndOwner, dlgprc, lParamInit);
#endif
}

#ifdef UNICODE
const TCHAR *GetDialogTemplateText(BYTE **dpitem)
#else
const TCHAR *GetDialogTemplateText(BYTE **dpitem,char *text)
#endif
{
	if ( *((WORD *)*dpitem) == 0xffff ){ // atom
		WORD atom;

		atom = *(WORD *)((BYTE *)(*dpitem + sizeof(WORD)));
		*dpitem += sizeof(WORD) * 2;
		if ( (atom >= 0x80) && (atom <= 0x85) ){
			return ClassAtomString[atom - 0x80];
		}
		return (const TCHAR *)(DWORD_PTR)atom;
	}else{
		#ifdef UNICODE
			WCHAR *text;
			DWORD size;

			text = (WCHAR *)*dpitem;
			size = strlenW(text) + 1;
			*dpitem += sizeof(WORD) * size;
		#else
			WCHAR *resulttextW;
			DWORD size;

			resulttextW = (WCHAR *)*dpitem;
			size = strlenW(resulttextW) + 1;
			*dpitem += sizeof(WORD) * size;
			if ( text != NULL ) UnicodeToAnsi(resulttextW,text,MAX_PATH);
		#endif
		return text;
	}
}

const TCHAR * USEFASTCALL GetCaptionText(int id)
{
	TCHAR name[8];

	if ( id < 0 ) return NULL;
	wsprintf(name,T("%04X"),id);
	return SearchMessageText(name);
}

// hParentWnd で指定したウィンドウに lpszTemplate のコントロール群を貼り付ける
HWND *CreateDialogWindow(HANDLE hinst,LPCTSTR lpszTemplate,HWND hParentWnd)
{
	HRSRC hrDialog;
	DWORD controls;
	BYTE *dialog;
	BYTE *dp,*dpitem;
	DLGITEMTEMPLATE *dtp;
	RECT box;
	int i;
	HFONT hFont;
	HWND *hCtrlWnds,*hCtrlWndDst;

	hFont = (HFONT)SendMessage(hParentWnd,WM_GETFONT,0,0);
	hrDialog = FindResource(hinst,lpszTemplate,RT_DIALOG);

	dialog = LockResource(LoadResource(hinst,hrDialog));

	controls = ((DLGTEMPLATE *)dialog)->cdit;
	hCtrlWnds = hCtrlWndDst = HeapAlloc(DLLheap,0,(controls + 1)* sizeof(HWND));

	dp = dialog + (sizeof(DWORD) * 2 + sizeof(WORD) * 5); // DLGTEMPLATE 相当
									// メニュー、クラス名、タイトル名を飛ばす
	for ( i = 0 ; i < 3 ; i++ ){
		WORD id;

		id = *(WORD *)dp;
		if ( id == 0 ){			// なし
			dp += sizeof(WORD);
		}else if ( id == 0 ){	// ATOM
			dp += sizeof(WORD) * 2;
		}else {
			dp += sizeof(WORD);
			while( *dp ) dp += sizeof(WORD);
			dp += sizeof(WORD);
		}
	}
	dpitem = dp;
									// 既定のフォントがあれば飛ばす
	if ( ((DLGTEMPLATE *)dialog)->style & DS_SETFONT ){
		dpitem += sizeof(WORD);
		while( *dpitem ) dpitem += sizeof(WORD);
		dpitem += sizeof(WORD);
	}
	for ( ; controls ; controls-- ){
		const TCHAR *ClassName,*CaptionName;
#ifndef UNICODE
		char ClassNameA[MAX_PATH];
		char CaptionNameA[MAX_PATH];
#endif
		WORD extrasize;
		HWND hCtrlWnd;

		if (ALIGNMENT_BITS(dpitem) & 3 ) dpitem += sizeof(WORD); // アライメント補正

		dtp = (DLGITEMTEMPLATE *)dpitem;
		box.left	= dtp->x;
		box.top		= dtp->y;
		box.right	= dtp->cx;
		box.bottom	= dtp->cy;
		MapDialogRect(hParentWnd,&box);

		dpitem += (sizeof(DWORD) * 2 + sizeof(WORD) * 5); //DLGITEMTEMPLATE相当
#ifdef UNICODE
		ClassName = GetDialogTemplateText(&dpitem);
		CaptionName = GetCaptionText(dtp->id);
		if ( CaptionName != NULL ){
			GetDialogTemplateText(&dpitem);
		}else{
			CaptionName = GetDialogTemplateText(&dpitem);
		}
#else
		ClassName = GetDialogTemplateText(&dpitem,ClassNameA);
		CaptionName = GetCaptionText(dtp->id);
		if ( CaptionName != NULL ){
			GetDialogTemplateText(&dpitem,NULL);
		}else{
			CaptionName = GetDialogTemplateText(&dpitem,CaptionNameA);
		}
#endif
		extrasize = *(WORD *)dpitem;

		hCtrlWnd = CreateWindowEx(dtp->dwExtendedStyle | WS_EX_NOPARENTNOTIFY,
				ClassName,CaptionName,dtp->style | WS_CHILD | WS_VISIBLE,
				box.left,box.top,box.right,box.bottom,hParentWnd,
				(HMENU)(DWORD_PTR)dtp->id,hinst,extrasize ? dpitem : NULL);
		if ( hCtrlWnd == NULL ){
			PPErrorBox(hParentWnd,NULL,PPERROR_GETLASTERROR);
			break;
		}
		SendMessage(hCtrlWnd,WM_SETFONT,(WPARAM)hFont,TMAKELPARAM(TRUE,0));
		*hCtrlWndDst++ = hCtrlWnd;
		dpitem += extrasize ? extrasize : sizeof(WORD);
	}
	*hCtrlWndDst = NULL;
	return hCtrlWnds;
}

//===================================== 簡易整形表示するウィンドウクラス
void PaintPPxStatic(HWND hWnd)
{
	PAINTSTRUCT ps;
	TCHAR buf[0x800],*max;
	RECT rect;

	BeginPaint(hWnd,&ps);
	max = buf + GetWindowText(hWnd,buf,TSIZEOF(buf));

	if ( max != buf ){
		TCHAR *first,*p;
		int X = 0,Y = 0;
		int align = 0;
		HGDIOBJ hOldFont;
		TEXTMETRIC tm;
		int baseW,baseH;

		#if 0
		{	// 呼び出し間隔計測用
			DWORD oldtick,nowtick = GetTickCount();

			oldtick = (DWORD)GetProp(hWnd,T("IntervalCheck"));
			if ( (nowtick - oldtick) < 100 ){
				XMessage(NULL,NULL,XM_DbgLOG,T("Static %x %3dm %s"),(DWORD)hWnd & 0xff,(nowtick - oldtick),buf);
			}
			SetProp(hWnd,T("IntervalCheck"),(HANDLE)nowtick);
		}
		#endif

		rect.top = 0;
		rect.bottom = 0;
		rect.left = 0;
		rect.right = 1;
		MapDialogRect(GetParent(hWnd),&rect);
		baseW = rect.right;
		GetClientRect(hWnd,&rect);
		rect.left += 2;
		SetTextColor(ps.hdc,GetSysColor(COLOR_WINDOWTEXT));
		SetBkColor(ps.hdc,GetSysColor(COLOR_3DFACE));
		hOldFont = SelectObject(ps.hdc,
				(HFONT)GetWindowLongPtr(hWnd,GWLP_USERDATA));

		GetTextMetrics(ps.hdc,&tm);
		baseH = tm.tmHeight;

		p = buf;
		while ( p < max ){
			SIZE ssize;
			int strlength;

			first = p;
			while ( p < max ){
				if ( (UTCHAR)*p < ' ' ) break;
				p++;
			}
			strlength = p - first;
			if ( strlength ){
				RECT box;

				GetTextExtentPoint32(ps.hdc,first,strlength,&ssize);
				if ( align == 0 ){
					ssize.cx = ( ssize.cx >= (rect.right - X) ) ?
							rect.right - X - ssize.cx :	// はみ出す時
							X;							// はみ出さない時
				}else{
					ssize.cx = ( ssize.cx >= (rect.right - X) ) ?
							X :							// はみ出す時
							rect.right - X - ssize.cx;	// はみ出さない時
				}
				box.left = X;
				box.top = Y;
				box.right = rect.right;
				box.bottom = min(rect.bottom,Y + baseH);

				ExtTextOut(ps.hdc,ssize.cx,Y,ETO_CLIPPED | ETO_OPAQUE,
						&box,first,strlength,NULL);
				if ( p == max ) break;
			}
			switch ( *p ){
				case PXSC_NORMAL: {				// 通常
					SetTextColor(ps.hdc,GetSysColor(COLOR_WINDOWTEXT));
					SetBkColor(ps.hdc,GetSysColor(COLOR_3DFACE));
					break;
				}
				case PXSC_HILIGHT: {
					SetTextColor(ps.hdc,GetSysColor(COLOR_HIGHLIGHTTEXT));
					SetBkColor(ps.hdc,GetSysColor(COLOR_HIGHLIGHT));
					break;
				}
				case '\n':			// 改行
					X = 0;
					Y += baseH;
					break;
				case PXSC_PAR: {
/*
----------------------+
3                     |
  +----+  +--+5 +--+  |
x1| x10|x1|…|x2|…|x1|
  +----+  +--+  +--+  |
2                     |
----------------------+
*/
					int i;
					RECT box;
					HBRUSH hB;
					int count,par,drawbar;

					par = (DWORD)*((BYTE *)(p + 1)) - 1;	// 1-100(-1)
					count = *(BYTE *)(p + 2) - 1;			// 1-11(-1)
					p += 2;

					box.top = rect.top + (baseW + (baseW / 2));
					box.bottom = min(rect.bottom,Y + baseH) - baseW;
											// 進捗グラフ(プログレスバー) -----
					hB = CreateSolidBrush(GetSysColor((count <= 10) ? COLOR_HIGHLIGHT : COLOR_GRAYTEXT ));
														// 最後直前まで
					box.left  = X + baseW;
					box.right = X + baseW * 11;
					for ( i = 0 ; (i + 10) < par ; i += 10 ){
						FillBox(ps.hdc,&box,hB);
						box.left = box.right + baseW;
						if ( i == 40 ) box.left += baseW;
						box.right = box.left + baseW * 10;
					}
														// 最後
					box.right = box.left + ((par - i) * baseW);
					FillBox(ps.hdc,&box,hB);
					DeleteObject(hB);
													// 空白 -------------------
					hB = CreateSolidBrush(GetSysColor(COLOR_3DFACE));

					box.top = rect.top;
					box.bottom += baseW;
					box.left = box.right;
					box.right = X + baseW * 112;
					FillBox(ps.hdc,&box,hB);

					drawbar = (int)GetWindowLongPtr(hWnd,0);
					if ( (drawbar > 0) && (drawbar < box.left) ){
						// 直前の処理中バーを消去
						box.left = drawbar;
						box.right = drawbar + baseW;
						FillBox(ps.hdc,&box,hB);
					}
					DeleteObject(hB);
											// 処理中バー -----------
					if ( (count >= 0) && (count <= 10) ){
						hB = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
						box.left = X + ( count * 11 + 11 ) * baseW;
						if ( count >= 4 ) box.left += baseW;
						box.right = box.left + baseW;
						FillBox(ps.hdc,&box,hB);
						DeleteObject(hB);
						SetWindowLongPtr(hWnd,0,(LONG_PTR)box.left);
					}

					X += baseW * 112;
					break;
				}
				case PXSC_LEFT:
					align = 0;
					break;
				case PXSC_RIGHT:
					align = 1;
					break;
				default:	// 未定義
					p = max;
			}
			p++;
		}
		SelectObject(ps.hdc,hOldFont);
	}
	EndPaint(hWnd,&ps);
}

LRESULT CALLBACK PPxStaticProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch ( message ){
		case WM_PAINT:
			PaintPPxStatic(hWnd);
			return 0;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			PostMessage(GetParent(hWnd),WM_COMMAND,GetDlgCtrlID(hWnd) | (WM_CONTEXTMENU << 16),(LPARAM)hWnd);
			return 0;

		case WM_SETTEXT:
			InvalidateRect(hWnd,NULL,FALSE);
			break;

		case WM_SETFONT:
			SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)wParam);
			if ( LOWORD(lParam) ) InvalidateRect(hWnd,NULL,FALSE);
			break;
	}
	return DefWindowProc(hWnd,message,wParam,lParam);
}

void ShowDlgWindow(const HWND hDlg,const UINT id,BOOL show)
{
	HWND hControlWnd;

	hControlWnd = GetDlgItem(hDlg,id);
	if ( hControlWnd == NULL ) return;
	ShowWindow(hControlWnd,show ? SW_SHOW : SW_HIDE);
}

void EnableDlgWindow(HWND hDlg,int id,BOOL state)
{
	HWND hControlWnd;

	hControlWnd = GetDlgItem(hDlg,id);
	if ( hControlWnd == NULL ) return;
	EnableWindow(hControlWnd,state);
}
