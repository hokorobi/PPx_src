/*-----------------------------------------------------------------------------
	Paper Plane xUI											PPc,PPv共用
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPX_DRAW.H"
#include "PPX_CV.H"
#pragma hdrstop

extern OSVERSIONINFO OSver;
extern UINT WM_PPXCOMMAND;
extern COLORREF C_back;
extern int X_fles;
extern const TCHAR StrUser32DLL[];

BOOL X_awhel = TRUE;
HMODULE hDwmapi = NULL;

const TCHAR RegWallpaperName[] = T("Wallpaper");
const TCHAR RegWallpaperTile[] = T("TileWallpaper");
const TCHAR RegWallpaperStyle[] = T("WallpaperStyle");
const TCHAR RegIEWallpaperPath[] = T("Software\\Microsoft\\Internet Explorer\\Desktop\\General");
const TCHAR RegWallpaperPath[] = T("Control Panel\\Desktop");
const TCHAR StrX_bg[] = T("X_bg");
const TCHAR DwmapiName[] = T("dwmapi.dll");

const TCHAR arrow[] = T("UDLR");
const TCHAR *arrowJ[] = {T("↑"),T("↓"),T("←"),T("→")};

const TCHAR WallpaperError[] = T("Wallpaper file not found");

struct CLIPNAMES{
	UINT type;
	TCHAR *name;
} ClipNames[] = {
	{CF_TEXT,			T("Text")},
	{CF_BITMAP,			T("BITMAP")},
	{CF_METAFILEPICT,	T("Meta picture")},
	{CF_SYLK,			T("MS Symbolic data")},
	{CF_DIF,			T("Software Arts' Data Interchange Format")},
	{CF_TIFF,			T("TIFF image")},
	{CF_OEMTEXT,		T("OEM Text")},
	{CF_DIB,			T("DIB BITMAP")},
	{CF_PALETTE,		T("Palette")},
	{CF_PENDATA,		T("Pen data")},
	{CF_RIFF,			T("RIFF data")},
	{CF_WAVE,			T("WAVE")},
	{CF_UNICODETEXT,	T("UNICODE Text")},
	{CF_ENHMETAFILE,	T("Ex Meta picture")},
	{CF_HDROP,			T("Drag files")},
	{CF_LOCALE,			T("Locale")},
	{CF_DIBV5,			T("DIB BITMAP V5")},
	{CF_OWNERDISPLAY,	T("Application draw picture")},
	{CF_DSPTEXT,		T("Private Text")},
	{CF_DSPBITMAP,		T("Private BITMAP")},
	{CF_DSPMETAFILEPICT,T("Private Meta picture")},
	{CF_DSPENHMETAFILE,	T("Private Ex Meta picture")},
	{0,NULL}
};

#define xDWM_BB_ENABLE B0
#define xDWM_BB_BLURREGION B1
#define xDWM_BB_TRANSITIONONMAXIMIZED B2

typedef struct tagDWM_BLURBEHIND {
	DWORD dwFlags;
	BOOL fEnable;
	HRGN hRgnBlur;
	BOOL fTransitionOnMaximized;
} xDWM_BLURBEHIND;

typedef struct {
	BYTE BlendOp;
	BYTE BlendFlags;
	BYTE SourceConstantAlpha;
	BYTE AlphaFormat;
} xBLENDFUNCTION;

typedef struct
{
	DWORD attribute;
	PVOID pData;
	ULONG dataSize;
} xWINCOMPATTRDATA;

typedef struct
{
	int nAccentState;
	int nFlags;
	int nColor;
	int nAnimationId;
} xACCENTPOLICY;

const xDWM_BLURBEHIND dwm_opaque = { xDWM_BB_ENABLE, FALSE, NULL, FALSE};
const xDWM_BLURBEHIND dwm_transparent = { xDWM_BB_ENABLE, TRUE, NULL, FALSE};

xBLENDFUNCTION DefaultBlend = {
	/* BlendOp */ AC_SRC_OVER, /* BlendFlags */ 0,
	/* SourceConstantAlpha */ 255, /* AlphaFormat */ AC_SRC_ALPHA
};

xACCENTPOLICY BackAccent = {3, 0, 0, 0};
xWINCOMPATTRDATA BackComp = {19, &BackAccent, sizeof(xACCENTPOLICY)};

POINT ZeroPoint = {0, 0};

DefineWinAPI(HRESULT,DwmEnableBlurBehindWindow,(HWND,const xDWM_BLURBEHIND *)) = NULL;
TypedefWinAPI(BOOL,SetLayeredWindowAttributes,(HWND hwnd,COLORREF crKey,BYTE bAlpha,DWORD dwFlags));
DefineWinAPI(BOOL,UpdateLayeredWindow,(HWND hWnd,HDC hdcDst,POINT *pptDst,SIZE *psize,HDC hdcSrc,POINT *pptSrc,COLORREF crKey,xBLENDFUNCTION *pblend,DWORD dwFlags));
DefineWinAPI(BOOL,SetWindowCompositionAttribute,(HWND, xWINCOMPATTRDATA*)) = NULL; // Win7 以降

extern ValueWinAPI(SetLayeredWindowAttributes);
#if !NODLL
ValueWinAPI(SetLayeredWindowAttributes) = NULL;
#endif

//HKEY_CURRENT_USER\Control Panel\Desktop\Wallpaper
//HKEY_CURRENT_USER\Control Panel\Desktop\WallpaperStyle
//HKEY_CURRENT_USER\Control Panel\Desktop\TitleWallpaper

const int PPxMouseButtonCode[] = {
	MOUSEBUTTON_L, MOUSEBUTTON_R, MOUSEBUTTON_ETC, MOUSEBUTTON_ETC,
	MOUSEBUTTON_M, MOUSEBUTTON_X, MOUSEBUTTON_Y, MOUSEBUTTON_Z,
	0
};
const TCHAR PPxMouseButtonChar[] = T(" LRMWXYZ?");

//-----------------------------------------------------------------------------
#if !NODLL
// 英小文字の大文字化
TCHAR USEFASTCALL upper(TCHAR c)
{
	if ( Islower(c) ) c -= (TCHAR)0x20;
	return c;
}
#endif
// クリップボードの種類名を求める
void GetClipboardTypeName(TCHAR *str, UINT cliptype)
{
	if ( !GetClipboardFormatName(cliptype, str, 64) ){
		struct CLIPNAMES *cp;

		wsprintf(str, T("Unknown type:%x"), cliptype);
		for ( cp = ClipNames ; cp->type ; cp++ ){
			if ( cp->type == cliptype ){
				tstrcpy(str, cp->name);
				break;
			}
		}
	}
}
//-----------------------------------------------------------------------------
// マウス操作の解析・実行
int CheckXMouseButton(WPARAM wParam)
{
	DWORD flags;

	flags = HIWORD(wParam);
	if ( flags & XBUTTON1 ) return 1;	// X
	if ( flags & XBUTTON2 ) return 2;	// Y
	if ( flags & B2 ) return 3;			// Z
	return 0;
}

// MOUSESTATE 構造体の初期化
void PPxInitMouseButton(MOUSESTATE *ms)
{
	ms->mode = MOUSEMODE_NONE;
	ms->PushButton = MOUSEBUTTON_CANCEL;
}

// ボタン押下のキャンセル処理
void PPxCancelMouseButton(MOUSESTATE *ms)
{
	ms->mode = MOUSEMODE_NONE;
	ms->PushButton = MOUSEBUTTON_CANCEL;
	ReleaseCapture(); // ここで WM_CAPTURECHANGED が出力
}

// 押下した任意ボタン情報を取得する
int PPxGetMouseButtonDownInfo(MOUSESTATE *ms, HWND hWnd, DWORD button, LPARAM lParam)
{
	const int *p;

	LPARAMtoPOINT(ms->PushClientPoint, lParam);
	ms->PushScreenPoint = ms->PushClientPoint;
	ClientToScreen(hWnd, &ms->PushScreenPoint);

	// ボタン判別
	ms->PushButton = MOUSEBUTTON_ETC;
	for ( p = PPxMouseButtonCode ; *p ; p++ ){
		if ( button & 1 ){
			ms->PushButton = *p;
			break;
		}
		button >>= 1;
	}
	return ms->PushButton;
}

// 任意ボタンの押下
// PPxDownMouseButtonX() も存在する。
int PPxDownMouseButton(MOUSESTATE *ms, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	DWORD button;

	button = wParam & MOUSEBUTTONMASK;

	// 2つ以上ボタンを押しているか？
	if ( !((button > 0) && ((button & (button - 1)) == 0)) ){ //bitが一つのみ?
		// 左右同時押し以外はキャンセル扱い
		// ドラッグ中左右同時押しもキャンセル?
		if ( button != (MK_LBUTTON | MK_RBUTTON) ){
			PPxCancelMouseButton(ms);
			return MOUSEBUTTON_CANCEL;
		}
		ms->PushButton = MOUSEBUTTON_W;
		ms->mode = MOUSEMODE_PUSH;
	}else{	// 押しているボタンは１つのみ→新規
		int x = GetSystemMetrics(SM_CXDRAG);
		int y = GetSystemMetrics(SM_CYDRAG);

		ms->mode = MOUSEMODE_PUSH;
		SetCapture(hWnd);
		PPxGetMouseButtonDownInfo(ms, hWnd, button, lParam);
		ms->DDRect.left   = ms->PushScreenPoint.x - x;
		ms->DDRect.right  = ms->PushScreenPoint.x + x;
		ms->DDRect.top    = ms->PushScreenPoint.y - y;
		ms->DDRect.bottom = ms->PushScreenPoint.y + y;
		ms->MovedClientPoint = ms->PushClientPoint;
		ms->MovedScreenPoint = ms->PushScreenPoint;
	}
	ms->PushTick = GetTickCount();
	return ms->PushButton;
}

// 任意ボタンの離上
int PPxUpMouseButton(MOUSESTATE *ms, WPARAM wParam)
{
	DWORD button;
	int pushbutton;

	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ) return MOUSEBUTTON_CANCEL;
	button = wParam & MOUSEBUTTONMASK;
								// 同時押しでまだ他のボタンが押されている
	if ( button != 0 ) return MOUSEBUTTON_CANCEL;
	pushbutton = ms->PushButton;
	ms->PushTick = GetTickCount() - ms->PushTick;
	PPxCancelMouseButton(ms);
	return pushbutton;
}

// 任意ボタンのダブルクリック
int PPxDoubleClickMouseButton(MOUSESTATE *ms, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// MOUSEBUTTON_CANCEL_DOUBLE か 各ボタンを押している状態なら操作無効
	if ( ms->PushButton != MOUSEBUTTON_CANCEL ) return MOUSEBUTTON_CANCEL;
	ms->mode = MOUSEMODE_NONE;
	ReleaseCapture();
	return PPxGetMouseButtonDownInfo(ms, hWnd, wParam & MOUSEBUTTONMASK, lParam);
}

// マウスカーソルの移動
BOOL PPxMoveMouse(MOUSESTATE *ms, HWND hWnd, LPARAM lParam)
{
	POINT clipos,scrpos;
	BOOL result = FALSE;

	if ( ms->PushButton <= MOUSEBUTTON_CANCEL ) return FALSE; // キャンセル状態

	LPARAMtoPOINT(clipos, lParam);
	scrpos = clipos;
	ClientToScreen(hWnd, &scrpos);

	if ( ms->mode == MOUSEMODE_PUSH ){	// ドラッグ検出中
		if ( IsTrue(PtInRect(&ms->DDRect, scrpos)) ) return FALSE; // 変化無し
		result = TRUE;
		ms->mode = MOUSEMODE_DRAG;
	}
	ms->MovedClientPoint = clipos;
	// ドラッグ量検出
	ms->MovedOffset.cx = scrpos.x - ms->MovedScreenPoint.x;
	ms->MovedOffset.cy = scrpos.y - ms->MovedScreenPoint.y;
	ms->MovedScreenPoint = scrpos;
	return result;
}

#define WHEEL_REENTRYFLAG 0x4000 // 再入チェック用
// ホイール操作
int PPxWheelMouse(MOUSESTATE *ms, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	int now, delta;

	if ( X_awhel && !(wParam & WHEEL_REENTRYFLAG) ){
		POINT pos;
		HWND hTwnd;

		LPARAMtoPOINT(pos, lParam);
		hTwnd = WindowFromPoint(pos);
		if ( hTwnd != hWnd ){
			SendMessage(hTwnd, WM_MOUSEWHEEL, wParam | WHEEL_REENTRYFLAG, lParam);
			return 0;
		}
	}
	now = (short)HIWORD(wParam);
	if ( (ms->WheelDelta ^ now) & B31 ){
		ms->WheelDelta = 0; // Overflow
	}
	ms->WheelDelta += now;
	delta = ms->WheelDelta / (WHEEL_DELTA / 3);
	ms->WheelDelta -= delta * (WHEEL_DELTA / 3);
	return delta;
}

BOOL PPxCheckMouseGesture(MOUSESTATE *stat, TCHAR *textbuf, const TCHAR *keyname)
{
	POINT delta, abs;
	TCHAR GestureStep;
	TCHAR cmd[64];

	if ( (stat->gesture.count + 1) >= MAXGESTURES ) return FALSE;
												// 方向検出
	delta.x = stat->MovedScreenPoint.x - stat->PushScreenPoint.x;
	delta.y = stat->MovedScreenPoint.y - stat->PushScreenPoint.y;
	abs.x = delta.x >= 0 ? delta.x : -delta.x;
	abs.y = delta.y >= 0 ? delta.y : -delta.y;
	if ( abs.x > abs.y ){
		GestureStep = ( delta.x >= 0 ) ? (TCHAR)'R' : (TCHAR)'L';
	}else{
		GestureStep = ( delta.y >= 0 ) ? (TCHAR)'D' : (TCHAR)'U';
	}
												// DD 再設定
	delta.x = GetSystemMetrics(SM_CXDOUBLECLK) * 2;
	delta.y = GetSystemMetrics(SM_CYDOUBLECLK) * 2;

	stat->PushClientPoint = stat->MovedClientPoint;
	stat->PushScreenPoint = stat->MovedScreenPoint;
	stat->DDRect.left   = stat->PushScreenPoint.x - delta.x;
	stat->DDRect.right  = stat->PushScreenPoint.x + delta.x;
	stat->DDRect.top    = stat->PushScreenPoint.y - delta.y;
	stat->DDRect.bottom = stat->PushScreenPoint.y + delta.y;
												// 同じ方向は無視
	if ( stat->gesture.count &&
		(stat->gesture.step[stat->gesture.count - 1] == GestureStep) ){
		return FALSE;
	}

	stat->gesture.step[stat->gesture.count++] = GestureStep;
	stat->gesture.step[stat->gesture.count] = '\0';

	if ( LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE ){
		TCHAR *src,*dest;

		for ( src = stat->gesture.step,dest = textbuf ; *src ;  ){
			int direction;
			switch ( *src++ ){
				case 'U':
					direction = 0;
					break;
				case 'D':
					direction = 1;
					break;
				case 'L':
					direction = 2;
					break;
//				case 'R':	// default と兼用
				default:
					direction = 3;
					break;
			}
			*dest++ = arrowJ[direction][0];
			#ifndef UNICODE
			*dest++ = arrowJ[direction][1];
			#endif
		}
		*dest = '\0';
	}else{
		tstrcpy(textbuf, stat->gesture.step);
	}
	wsprintf(cmd, T("RG_%s"), stat->gesture.step);
	if ( IsExistCustTable(keyname, cmd) ){
		tstrcat(textbuf, T(" - hit"));
	}
	return TRUE;
}

#if !NODLL
WORD FixCharKeycode(WORD key)
{
	if ( IslowerA(key) ) key -= (WORD)0x20;	// 小文字
	if ( IsalnumA(key) || (key <= 0x20)){
		key |= (WORD)GetShiftKey();
	}else{
		key |= (WORD)(GetShiftKey() & ~K_s);
	}
																// ctrl + char
	if ( (key & K_c) && ((key & 0xff) < 0x20) ) key += (WORD)0x40;
	return key;
}
#endif
//-----------------------------------------------------------------------------
// 背景画像を解放する
void UnloadWallpaper(BGSTRUCT *bg)
{
	if ( bg->X_WallpaperType == 0 ) return;
	if ( bg->X_WPbmp.DIB != NULL ){
		FreeBMP(&bg->X_WPbmp);
		DxDrawFreeBMPCache(&bg->WPbmpCache);
	}
	bg->X_WallpaperType = 0;
/*
	if ( hDwmapi != NULL ){
		DDwmEnableBlurBehindWindow = NULL;
		FreeLibrary(hDwmapi);
		hDwmapi = NULL;
	}
*/
}

// 背景画像の読み込みを行う
void LoadWallpaper(BGSTRUCT *bg, HWND hWnd, const TCHAR *regid)
{
	TCHAR buf[VFPS];
	TCHAR rpath[VFPS];
	const TCHAR *ptr;
	int opaque,mode;
	int bright = 100;
	int dispW,dispH;

	if ( X_fles == 2 ){ // ●仮
		if ( DSetLayeredWindowAttributes == NULL ){
			HMODULE hUser32;

			hUser32 = GetModuleHandle(StrUser32DLL);
			GETDLLPROC(hUser32, SetLayeredWindowAttributes);
			GETDLLPROC(hUser32, UpdateLayeredWindow);
		}
		if ( DSetLayeredWindowAttributes != NULL ){
			SetWindowLong(hWnd, GWL_EXSTYLE,
					GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		}
		return;
	}
	// 透明度の設定 -----
	opaque = 100;
	buf[0] = '\0';
	wsprintf(rpath,T("O_%s"),regid);
	if ( NO_ERROR != GetCustTable(StrX_bg, rpath, &buf, sizeof(buf)) ){
		GetCustTable(StrX_bg, T("Opaque"), &buf, sizeof(buf));
	}
	if ( buf[0] != '\0' ){
		ptr = buf;
		opaque = GetIntNumber(&ptr);
	}
	if ( opaque >= 100 ){ // 通常表示
		if ( DSetLayeredWindowAttributes != NULL ){
			SetWindowLong(hWnd, GWL_EXSTYLE,
					GetWindowLong(hWnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		}
		if ( DDwmEnableBlurBehindWindow != NULL ){
			DDwmEnableBlurBehindWindow(hWnd, &dwm_opaque);
		}
	}else{ // 半透明表示
		if ( opaque < 0 ){ // aero 以降対応版
			if ( hDwmapi == NULL ) hDwmapi = LoadLibrary(DwmapiName);
			if ( hDwmapi != NULL ){
				GETDLLPROC(hDwmapi, DwmEnableBlurBehindWindow);
				DDwmEnableBlurBehindWindow(hWnd, &dwm_transparent);
				if ( OSver.dwMajorVersion >= 10 ){
					HMODULE hUser32;

					hUser32 = GetModuleHandle(StrUser32DLL);
					GETDLLPROC(hUser32, SetWindowCompositionAttribute);
					if ( DSetWindowCompositionAttribute != NULL ){
						// Win8以降だが、殆ど機能しない。
						// Win10のみ子ウィンドウ・タイトルバーが無い窓で使用可能
						DSetWindowCompositionAttribute(hWnd, &BackComp);
					}
				}
			}
		}else{
			if ( DSetLayeredWindowAttributes == NULL ){
				HMODULE hUser32;

				hUser32 = GetModuleHandle(StrUser32DLL);
				GETDLLPROC(hUser32, SetLayeredWindowAttributes);
				GETDLLPROC(hUser32, UpdateLayeredWindow);
			}
			if ( (DSetLayeredWindowAttributes != NULL) && (GetParent(hWnd) == NULL) ){
				SetWindowLong(hWnd, GWL_EXSTYLE,
						GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);
				if ( X_fles != 2 ){
					if ( opaque ){ // 半透明
						DSetLayeredWindowAttributes(hWnd, 0,
							(BYTE)((255 * opaque)/ 100), LWA_ALPHA);
					}else{ // 背景色のみ透明
						DSetLayeredWindowAttributes(hWnd, C_back, 0, LWA_COLORKEY);
					}
				}
			}
		}
	}
	if ( bg == NULL ) return;
	// 壁紙の設定 -----
							// 壁紙を使用するかどうかを判別
	buf[0] = '\0';
	wsprintf(rpath,T("T_%s"),regid);
	if ( NO_ERROR != GetCustTable(StrX_bg, rpath, &buf, sizeof(buf)) ){
		if ( NO_ERROR != GetCustTable(StrX_bg, T("Type"), &buf, sizeof(buf)) ){
			UnloadWallpaper(bg);
			return;
		}
	}

	ptr = buf;
	mode = GetIntNumber(&ptr);	// 空欄なら 0
	if ( mode == 0 ){
		UnloadWallpaper(bg);
		return;
	}

	if ( mode == 10 ){
		buf[0] = '\0';
		GetRegString(HKEY_CURRENT_USER,
				RegIEWallpaperPath, RegWallpaperTile, buf, TSIZEOF(buf));
		if ( buf[0] == '1' ) mode = 11;
	}
	if ( mode == 20 ){
		if ( GetParent(hWnd) == NULL ) mode = 1;
	}
							// 明るさ取得
	buf[0] = '\0';
	wsprintf(rpath,T("B_%s"),regid);
	if ( NO_ERROR != GetCustTable(StrX_bg ,rpath, &buf, sizeof(buf)) ){
		GetCustTable(StrX_bg ,T("Bright"), &buf, sizeof(buf));
	}
	if ( buf[0] != '\0' ){
		ptr = buf;
		bright = GetIntNumber(&ptr);
	}
							// 使用する壁紙を決定する
	buf[0] = '\0';
	wsprintf(rpath,T("P_%s"),regid);
	if ( NO_ERROR != GetCustTable(StrX_bg ,rpath, &buf, sizeof(buf)) ){
		GetCustTable(StrX_bg ,T("Path"), &buf, sizeof(buf));
	}
	if ( buf[0] != '\0' ){
		VFSFixPath(NULL,buf,NULL,VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}else{
		GetRegString(HKEY_CURRENT_USER,
				RegIEWallpaperPath, RegWallpaperName, buf, TSIZEOF(buf));
		if ( GetFileAttributesL(buf) == BADATTR ) buf[0] = '\0';
		if ( buf[0] == '\0' ){
			GetRegString(HKEY_CURRENT_USER,
					RegWallpaperPath, RegWallpaperName, buf, TSIZEOF(buf));
		}
	}

	if ( (buf[0] != '\0') && (GetFileAttributesL(buf) == BADATTR) ){
		SendMessage(hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_SETPOPMSG, POPMSG_MSG),
			(LPARAM)WallpaperError);
		return;
	}

	DIRECTXDEFINE(bg->WPbmpCache = NULL);
	bg->X_WallpaperType = LoadBMP(&bg->X_WPbmp, buf, bright) == FALSE ? 0 : mode;
	if ( bg->X_WallpaperType != 10 ) return;

	buf[0] = '\0';
	GetRegString(HKEY_CURRENT_USER,
			RegWallpaperPath, RegWallpaperStyle, buf, TSIZEOF(buf));
	if ( buf[0] <= '0' ) return; // スクリーンサイズに拡大する必要なし
	// 2:とにかく調整 6:縦幅を一致 たりない横幅は空白 10:横幅一致 縦ははみ出る

	{
		HDC hDC;

		hDC = GetDC(hWnd);
		dispW = GetDeviceCaps(hDC, HORZRES); // 横
		dispH = GetDeviceCaps(hDC, VERTRES); // 縦
		ReleaseDC(hWnd, hDC);
	}

	if ( ( dispW == bg->X_WPbmp.size.cx ) &&
		 ( dispH == bg->X_WPbmp.size.cy ) ){
		return; // 同サイズなので変更不要
	}

	if ( (OSver.dwMajorVersion >= 6) || (bg->X_WPbmp.DIB->biBitCount > 8) ){
		HDC hDC, hMDC;
		LPVOID lpBits;
		BITMAPINFO *bmpinfo;
		int olds, bmpsize, bitsize;
		HBITMAP hBMP;
		HGDIOBJ hOldBMP;
		int srcx, srcy, srcw, srch;
		int dstx, dsty, dstw, dsth;
		HPALETTE hOldPal C4701CHECK;

		bmpsize = bg->X_WPbmp.DIB->biClrUsed;
		bmpsize = bg->X_WPbmp.PaletteOffset +
			( bmpsize ? bmpsize * sizeof(RGBQUAD) :
			  ((bg->X_WPbmp.DIB->biBitCount <= 8) ? (DWORD)(1 << bg->X_WPbmp.DIB->biBitCount) * sizeof(RGBQUAD) : 0));
		bitsize = DwordBitSize(dispW * bg->X_WPbmp.DIB->biBitCount) * dispH;

		hDC = GetDC(hWnd);
		hMDC = CreateCompatibleDC(hDC);
		bmpinfo = (BITMAPINFO *)LocalAlloc(LMEM_FIXED,bmpsize + bitsize);

		memcpy(bmpinfo,bg->X_WPbmp.DIB,bmpsize);
		bmpinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpinfo->bmiHeader.biWidth = dispW;
		bmpinfo->bmiHeader.biHeight = dispH;
		bmpinfo->bmiHeader.biCompression = BI_RGB;

		srcx = srcy = dstx = dsty = 0;
		srcw = bg->X_WPbmp.size.cx;
		srch = bg->X_WPbmp.size.cy;
		dstw = dispW;
		dsth = dispH;

		if ( buf[0] == '1' ){ // 横幅一致
			dsth = (srch * dstw) / srcw;
			dsty = (dispH - dsth) / 2;
		}else if ( buf[0] == '6' ){ // 縦幅一致
			dstw = (srcw * dsth) / srch;
			dstx = (dispW - dstw) / 2;
		}

		// 拡大縮小
		hBMP = CreateDIBSection(hDC, bmpinfo, DIB_RGB_COLORS, &lpBits, NULL, 0);
		hOldBMP = SelectObject(hMDC, hBMP);
		olds = SetStretchBltMode(hMDC, HALFTONE);

		if ( bg->X_WPbmp.hPalette != NULL ){
			hOldPal = SelectPalette(hMDC, bg->X_WPbmp.hPalette, FALSE);
			RealizePalette(hMDC);
		}
		StretchDIBits(hMDC, dstx,dsty,dstw,dsth, srcx,srcy,srcw,srch,
			bg->X_WPbmp.bits,(BITMAPINFO *)bg->X_WPbmp.DIB,
			DIB_RGB_COLORS,SRCCOPY);

		if ( bg->X_WPbmp.hPalette != NULL ){
			SelectPalette(hDC,hOldPal,FALSE); // C4701ok
		}

		SetStretchBltMode(hMDC,olds);
		SelectObject(hMDC,hOldBMP);

		// 新しいビットマップを設定
		if ( bg->X_WPbmp.info != NULL ){
			LocalUnlock(bg->X_WPbmp.info);
			LocalFree(bg->X_WPbmp.info);
		}
		bg->X_WPbmp.DIB = &bmpinfo->bmiHeader;
		bg->X_WPbmp.info = bmpinfo;
		bg->X_WPbmp.bits = (BYTE *)bmpinfo + bmpsize;
		memcpy(bg->X_WPbmp.bits, lpBits, bitsize);
		bg->X_WPbmp.size.cx = dispW;
		bg->X_WPbmp.size.cy = dispH;

		DeleteObject(hBMP);
		DeleteDC(hMDC);
		ReleaseDC(hWnd, hDC);
	}
}

// 背景画像の表示を行う
void DrawWallPaper(DIRECTXARG(DXDRAWSTRUCT *DxDraw) BGSTRUCT *bg, HWND hWnd, PAINTSTRUCT *ps, int Areax)
{
	int x, y, xm;
	POINT pos;
	RECT desk;
	#ifdef USEDIRECTX
		RECT client;
	#endif

	switch ( bg->X_WallpaperType & ~B30 ){
		case 3:		// 左上
			xm = -(bg->X_WPbmp.size.cx - (Areax % bg->X_WPbmp.size.cx) );
			y = 0;
			break;

		case 20:	// 一体化(左上)
			GetWindowRect(GetParent(hWnd), &desk);
			pos.x = desk.left;
			pos.y = desk.top;
			ScreenToClient(hWnd, &pos);
			xm = (pos.x % bg->X_WPbmp.size.cx);
			if ( xm > 0 ) xm -= bg->X_WPbmp.size.cx;
			y = (pos.y % bg->X_WPbmp.size.cy);
			if ( y > 0 ) y -= bg->X_WPbmp.size.cy;
			break;

		case 10:	// デスクトップ同期(Tile mode)
			GetDesktopRect(hWnd, &desk);
			pos.x = desk.left;
			pos.y = desk.top;
			ScreenToClient(hWnd, &pos);
			xm = (pos.x % bg->X_WPbmp.size.cx);
			if ( xm > 0 ) xm -= bg->X_WPbmp.size.cx;
			y = (pos.y % bg->X_WPbmp.size.cy);
			if ( y > 0 ) y -= bg->X_WPbmp.size.cy;
			break;

		case 11:	// デスクトップ同期(中央)
			GetDesktopRect(hWnd, &desk);
			pos.x = desk.left +
				((desk.right - desk.left) - bg->X_WPbmp.size.cx) / 2;
			pos.y = desk.top +
				((desk.bottom - desk.top) - bg->X_WPbmp.size.cy) / 2;
			ScreenToClient(hWnd,&pos);
			xm = (pos.x % bg->X_WPbmp.size.cx);
			if ( xm > 0 ) xm -= bg->X_WPbmp.size.cx;
			y = (pos.y % bg->X_WPbmp.size.cy);
			if ( y > 0 ) y -= bg->X_WPbmp.size.cy;
			break;

		default:	// (0)右上
			xm = y = 0;
	}
	if ( bg->X_WallpaperType & ~B30 ){
		#ifdef USEDIRECTX
		GetClientRect(hWnd, &client);
		#endif
		for ( ; y < ps->rcPaint.bottom ; y += bg->X_WPbmp.size.cy ){
			for ( x = xm ; x < ps->rcPaint.right ; x += bg->X_WPbmp.size.cx ){
			#ifdef USEDIRECTX
				IfDXmode(ps->hdc){
					desk.left = x;
					desk.top = y;
					desk.right = bg->X_WPbmp.size.cx;
					desk.bottom = bg->X_WPbmp.size.cy;
					DxDrawDIB(DxDraw, bg->X_WPbmp.DIB, bg->X_WPbmp.bits, &desk, &client, &bg->WPbmpCache);
				}else
			#endif
				{
					DrawBMP(ps->hdc, &bg->X_WPbmp, x, y);
				}
			}
		}
	}
}

void InitOffScreen(BGSTRUCT *bg, HDC hWndDC, SIZE *WndSize)
{
	HBITMAP hMemBmp;

	if ( bg->hOffScreenDC != NULL ){
		if ( X_fles == 2 ){
			int size;
			BYTE *p;
			DWORD newbg;

			newbg = 0xffffffff; // C_back;
			size = WndSize->cx * WndSize->cy;
			for ( p = bg->DCBitsPtr ; size ; size-- ){
				*(COLORREF *)p = newbg;
				p += 4;
			}
		}

		return;
	}

	bg->hOffScreenDC = CreateCompatibleDC(hWndDC);
	if ( X_fles != 2 ){
		X_fles = 1;
		hMemBmp = CreateCompatibleBitmap(hWndDC, WndSize->cx, WndSize->cy);
	}else{
		BITMAPINFOHEADER bi;

		memset(&bi, 0, sizeof(BITMAPINFOHEADER));
		bi.biSize = sizeof(BITMAPINFOHEADER);
		bi.biWidth = WndSize->cx;
		bi.biHeight = WndSize->cy;
		bi.biPlanes = 1;
		bi.biBitCount = 32;

		hMemBmp = CreateDIBSection(bg->hOffScreenDC, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&bg->DCBitsPtr, NULL, 0);
	}
	bg->hOldBitmap = SelectObject(bg->hOffScreenDC, hMemBmp);
}

void OffScreenToScreen(BGSTRUCT *bg, HWND hWnd, HDC hWndDC, RECT *WndRect, SIZE *WndSize)
{
	if ( (X_fles != 2) || (WndRect == NULL) ){
		BitBlt(hWndDC, 0,0, WndSize->cx,WndSize->cy, bg->hOffScreenDC, 0, 0, SRCCOPY);
	}else{
		HDC hScreenDC;
		POINT wndPos;

		hScreenDC = GetDC(hWnd);
		wndPos.x = WndRect->left;
		wndPos.y = WndRect->top;

		if ( DUpdateLayeredWindow == NULL ){
			HMODULE hUser32;
			hUser32 = GetModuleHandle(StrUser32DLL);
			GETDLLPROC(hUser32,UpdateLayeredWindow);
			if ( DUpdateLayeredWindow == NULL ){
				BitBlt(hWndDC, 0,0, WndSize->cx,WndSize->cy, bg->hOffScreenDC, 0,0, SRCCOPY);
				return;
			}
		}

		{
			int size;
			BYTE *pixp;
			DWORD c, bgc, NewBgc;

			#define TRPAR 0x80
#if 0		// ABGR
			bgc = C_back;
			NewBgc = (((((C_back & 0xff00ff) * TRPAR) & 0xff00ff00) | (((C_back & 0xff00) * TRPAR) & 0xff0000)) >> 8) | (TRPAR << 24);
#else		// ARGB
			bgc = ((C_back & 0xff) << 16) | (C_back & 0xff00) | ((C_back & 0xff0000) >> 16);
			NewBgc = ((((C_back & 0x0000ff) * TRPAR) & 0x00ff00) << 8) |
					((((C_back & 0x00ff00) * TRPAR) & 0xff0000) >> 8) |
					(((C_back & 0xff0000) * TRPAR) >> 24) |
					(TRPAR << 24);
#endif
			#undef TRPAR

			#define TRPAR 0xf0
			size = WndSize->cx * WndSize->cy;
			for ( pixp = bg->DCBitsPtr ; size ; size-- ){
				c = *(DWORD *)pixp;
				if ( c == bgc ){
					*(DWORD *)pixp = NewBgc;
				}else{
					*(DWORD *)pixp = (((((c & 0xff00ff) * TRPAR) & 0xff00ff00) + (((c & 0xff00) * TRPAR) & 0xff0000)) >> 8) | (TRPAR << 24);
				}
				pixp += sizeof(DWORD);
			}
			#undef TRPAR
		}

		DUpdateLayeredWindow(hWnd, hScreenDC, &wndPos, WndSize, bg->hOffScreenDC, &ZeroPoint, 0, &DefaultBlend, ULW_ALPHA);
		ReleaseDC(hWnd, hScreenDC);
	}
}

// 仮想スクリーンを解放する
void FreeOffScreen(BGSTRUCT *bg)
{
	if ( bg->hOffScreenDC == NULL ) return;

	DeleteObject(SelectObject(bg->hOffScreenDC, bg->hOldBitmap));
	DeleteDC(bg->hOffScreenDC);
	bg->hOffScreenDC = NULL;
}

int PreviewCommand(BGSTRUCT *bg, const TCHAR *param, const TCHAR *path)
{
	if ( param[0] == '\0' ){
		UnloadWallpaper(bg);
		return 0;
	}else{
		TCHAR buf[VFPS];
		int result;

		VFSFullPath(buf, (TCHAR *)param, path);
		result = LoadBMP(&bg->X_WPbmp, buf, 50);
		bg->X_WallpaperType = result;
		return result;
	}
}

DWORD CalcBmpHeaderSize(BITMAPINFOHEADER *dumpdata)
{
	DWORD offset, color, palette;

	offset = dumpdata->biSize;
	if ( offset < sizeof(BITMAPINFOHEADER) ){
												// OS/2 形式
		color = ((BITMAPCOREHEADER *)dumpdata)->bcBitCount;
		if ( color <= 8 ) offset += (DWORD)(1 << color) * sizeof(RGBTRIPLE);
	}else{								// WINDOWS 形式
		color = dumpdata->biBitCount;
		palette = dumpdata->biClrUsed;
		if ( (offset < (sizeof(BITMAPINFOHEADER) + 12) ) && (dumpdata->biCompression == BI_BITFIELDS) ){
			offset += 12;	// 16/32bit のときはビット割り当てがある
		}

		palette = palette ? palette * sizeof(RGBQUAD) :
				((color <= 8) ? (DWORD)(1 << color) * sizeof(RGBQUAD) : 0);

		if ( offset == 0x7c/*BITMAPV5HEADER*/ ){
			DWORD ProfileData = *(DWORD *)((BYTE *)dumpdata + 0x70); //bV5ProfileData
			if ( (offset + palette) == ProfileData ){
				offset += *(DWORD *)((BYTE *)dumpdata + 0x74); //bV5ProfileSize
			}
		}
		offset += palette;
	}
	return offset;
}


// 色が _AUTO ならシステムの色を取得 ------------------------------------------
void AutoColor(const TCHAR *str, COLORREF *clr, int def)
{
	GetCustData(str, clr, sizeof(COLORREF));
	if ( *clr == C_AUTO ) *clr = GetSysColor(def);
}

void LoadHiddenMenu(HIDDENMENUS *hms, const TCHAR *name, HANDLE heap, COLORREF mes)
{
	int size;
	HIDDENMENUSTRUCT *tbl;
	char **hm;
	int i;

	if ( hms->data ) HeapFree(heap, 0, hms->data);
	hms->item = 0;
	hms->data = NULL;

	size = GetCustDataSize(name);
	i = CountCustTable(name);
	if ( i <= 0 ) return;

	hms->data = hm = HeapAlloc(heap, 0, size + sizeof(TCHAR *) * i);
	if ( hm == NULL ) return;
	tbl = (HIDDENMENUSTRUCT *)((char *)hm + sizeof(TCHAR *) * i);
	GetCustData(name, tbl, size);

	if ( tbl->offset && (*((TCHAR *)(HIDDENMENUSTRUCT *)(tbl + 1) + 1) != '\0') ){
		for ( ; ; ){
			DWORD w;

			w = tbl->offset;
			if ( !w ) break;

			if ( tbl->fg == C_AUTO ) tbl->fg = mes;
			if ( tbl->bg == C_AUTO ) tbl->bg = C_back;

			*hm = (char *)tbl->name;
			tbl = (HIDDENMENUSTRUCT *)((char *)tbl + w);
			hms->item++;
			hm++;
		}
	}
}

#if !NODLL
/*
	FillRect の置き換え版。これよりも FillRect の方が遅い。
	ExtTextOut で塗りつぶすのもFillRectより早いが、PatBltに今一歩及ばず
*/
void FillBox(HDC hDC, const RECT *box, HBRUSH hbr)
{
	HGDIOBJ hOldObj;

	hOldObj = SelectObject(hDC,hbr);
	PatBlt(hDC, box->left, box->top,
			box->right - box->left, box->bottom - box->top,
			PATCOPY);
	SelectObject(hDC, hOldObj);
}
#endif

// AppendMenu の文字列専用版 --------------------------------------------------
#if !NODLL
void USEFASTCALL AppendMenuString(HMENU hMenu, UINT id, const TCHAR *string)
{
	AppendMenu(hMenu, MF_ES, id, MessageText(string));
}
void USEFASTCALL AppendMenuCheckString(HMENU hMenu, UINT id, const TCHAR *string, BOOL check)
{
	AppendMenu(hMenu, MF_CHKES(check), id, MessageText(string));
}
#endif

BOOL GetAndFixTextMetrics(HDC hDC, TEXTMETRIC *tm)
{
	GetTextMetrics(hDC, tm);
	if ( tm->tmAveCharWidth < 1 ) tm->tmAveCharWidth = 1;
		// ※ TMPF_FIXED_PITCH は意味が反対になっている
	if ( !(tm->tmPitchAndFamily & TMPF_FIXED_PITCH) ) return FALSE; // 等幅フォント
	{ // 不等幅フォント
		int NumWidth[10], *Nptr, i;

		GetCharWidth32(hDC, '0', '9', NumWidth);
		for ( i = 0,Nptr = NumWidth ; i < 9 ; i++,Nptr++ ){
			if ( *Nptr > tm->tmAveCharWidth ) tm->tmAveCharWidth = *Nptr;
		}
		return TRUE;
	}
}
