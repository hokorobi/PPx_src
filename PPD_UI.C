/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library			UI 関係
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPD_DEF.H"
#pragma hdrstop

HMODULE hWinmm = NULL;
HMODULE hShcore = NULL;

DefineWinAPI(HRESULT, DwmGetWindowAttribute, (HWND, DWORD, PVOID, DWORD)) = NULL;
#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
  #define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif

typedef enum xMONITOR_DPI_TYPE {
	xMDT_EFFECTIVE_DPI = 0,
	xMDT_ANGULAR_DPI = 1,
	xMDT_RAW_DPI = 2,
	xMDT_DEFAULT = xMDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

DefineWinAPI(HMONITOR, MonitorFromWindow, (HWND hwnd, DWORD dwFlags)) = NULL;
DefineWinAPI(BOOL, GetMonitorInfoA, (HMONITOR hMonitor, LPMONITORINFO lpmi));
DefineWinAPI(BOOL, GetDpiForMonitor, (HMONITOR hMonitor, enum xMONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY)) = NULL;
DefineWinAPI(BOOL, PlaySound, (LPCTSTR, HMODULE, DWORD));
DefineWinAPI(BOOL, FlashWindowEx, (PFLASHWINFO pfwi));
DefineWinAPI(BOOL, FlashWindow, (HWND hWnd, BOOL invert)) = NULL;

LOADWINAPISTRUCT MONITORDLL[] = {
	LOADWINAPI1(MonitorFromWindow),
	LOADWINAPI1(GetMonitorInfoA),
	{NULL, NULL}
};

#define BeepType_Max 9
const UINT BeepType[BeepType_Max] = {
	MB_ICONHAND,		// 致命的エラー表示
	MB_ICONHAND,		// 一般エラー表示
	MB_OK,				// 重要でないエラー表示
	MB_ICONEXCLAMATION,	// 重要な警告

	MB_ICONEXCLAMATION,	// 重要でない警告
	MB_ICONASTERISK,	// 結果を表示する情報
	MB_ICONASTERISK,	// 好意で出力する情報/作業完了
	MB_OK,				// その他
	0xFFFFFFFF			// 範囲外
};

const TCHAR *MessageType[] = {
	T("Err! Dlg"), T("Err  Dlg"), T("Err? Dlg"), T("Warn!Dlg"), T("Warn Dlg"),
	T("Result D"), T("Info Dlg"), T("Etc  Dlg"),
	T("Err! Mes"), T("Err  Mes"), T("Err? Mes"), T("Warn!Mes"), T("Warn Mes"),
	T("Result M"), T("Info"), T("Etc  Mes"),
	T("Fault by"), T("Break by"), T("Error by"), T("Unknown1"), T("Unknown2"),
	T("Unknown3"), T("Unknown4"), T("Unknown5"),
	T("DebugDlg"), T("DebugSnd"), T("DebugLog"), T("Dump Dlg"), T("Dump Log"),
	T("Unknown6"), T("Unknown7"), T("Unknown8")};

typedef struct {
	const TCHAR *text;
	UINT ID;
} MESSAGEITEMS;

const MESSAGEITEMS MessageYesNo[] = {
	{MES_IMYE, IDYES},
	{MES_IMNO, IDNO},
	{NULL, 0}
};

const MESSAGEITEMS MessageYesNoCancel[] = {
	{MES_IMYE, IDYES},
	{MES_IMNO, IDNO},
	{MES_IMTC, IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageOkCancel[] = {
	{MES_IMYK, IDOK},
	{MES_IMCA, IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageOk[] = {
	{MES_IMOK, IDOK},
	{NULL, 0}
};

const MESSAGEITEMS MessageAbortRetryIgnore[] = {
	{MES_IMAB, IDABORT},
	{MES_IMRE, IDRETRY},
	{MES_IMIG, IDIGNORE},
	{NULL, 0}
};

const MESSAGEITEMS MessageRetryCancel[] = {
	{MES_IMRE, IDRETRY},
	{MES_IMTC, IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageAddAbortRetryIgnore[] = {
	{MES_IMDLx, IDB_QDADDLIST},
	{MES_IMAB, IDABORT},
	{MES_IMRE, IDRETRY},
	{MES_IMIG, IDIGNORE},
	{NULL, 0}
};

const MESSAGEITEMS MessageStartCancel[] = {
	{MES_IMST, IDYES},
	{MES_IMCA, IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageYesNoIgnoreCancel[] = {
	{MES_IMYE, IDYES},
	{MES_IMNO, IDNO},
	{MES_IMSK, IDIGNORE},
	{MES_IMTC, IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageRetryCancel_NoTrans[] = { // UsePPx で使うため、多言語化処理は無し
	{T("&Retry"), IDRETRY},
	{T("&Cancel"), IDCANCEL},
	{NULL, 0}
};

const MESSAGEITEMS MessageAbortRetryRenameIgnore[] = {
	{MES_IMAB, IDABORT},
	{MES_IMRE, IDRETRY},
	{T("7068|Rename file on dest.(&V)"), IDX_FOP_ADDNUMDEST},
	{MES_IMIG, IDIGNORE},
	{NULL, 0}
};

const MESSAGEITEMS *MessageTypes[] = {
	MessageOk, MessageOkCancel, MessageAbortRetryIgnore,
	MessageYesNoCancel, MessageYesNo, MessageRetryCancel,
	MessageAbortRetryIgnore/* CancelTryContinue */, //ここまでがWindows標準定義
	MessageAddAbortRetryIgnore,	//  7, MB_PPX_ADDABORTRETRYIGNORE
	MessageStartCancel,			//  8, MB_PPX_STARTCANCEL
	MessageYesNoIgnoreCancel,	//  9, MB_PPX_YESNOIGNORECANCEL
	MessageRetryCancel_NoTrans,	// 10, MB_PPX_USEPPXCHECKOKCANCEL
	MessageAbortRetryRenameIgnore	// 11, MB_PPX_ABORTRETRYRENAMEIGNORE
};

const TCHAR MessageAllReactionStr[] = T("7062|&Use all reaction");
const TCHAR CriticalTitle[] = T("Paper Plane xUI");

HMODULE hIEframe = NULL;
const TCHAR VistaBitmapBar[] = T("IDB_TB_SH_DEF_16");

PPXDLL void PPXAPI XBeep(UINT type)
{
	if ( X_beep > 0xff ){
		X_beep = 0xaf;
		GetCustData(T("X_beep"), &X_beep, sizeof(X_beep));
	}
	if ( X_beep & (1 << type) ){
		if ( type >= BeepType_Max ) type = BeepType_Max - 1;
		MessageBeep(BeepType[type]);
	}
}

BOOL PPxFlashWindow(HWND hWnd, int mode)
{
	if ( DFlashWindow == NULL ){
		HMODULE hUser32;

		hUser32 = GetModuleHandleA(User32DLL);
		#pragma warning(disable:4245) // User32.dll は常に読み込み済み
		GETDLLPROC(hUser32, FlashWindow);
		GETDLLPROC(hUser32, FlashWindowEx);
		#pragma warning(default:4245)
	}
	if ( mode == PPXFLASH_STOP ){
		DFlashWindow(hWnd, FALSE);
		return TRUE;
	}

	if ( DFlashWindowEx != NULL ){
		FLASHWINFO fwinfo;

		fwinfo.cbSize = sizeof(fwinfo);
		fwinfo.hwnd = hWnd;
		fwinfo.dwFlags = FLASHW_ALL;
		fwinfo.uCount = 5;
		fwinfo.dwTimeout = 500;
		DFlashWindowEx(&fwinfo);
		return FALSE;
	}else{
		DFlashWindow(hWnd, TRUE);
		return TRUE;
	}
}

PPXDLL void USECDECL XMessage(HWND hWnd, const TCHAR *title, UINT type, const TCHAR *message, ...)
{
	TCHAR buf[0x400];
	DWORD flag;
	va_list argptr;
	UINT icon;

	flag = 1 << type;
	//------------------------------------------------------ 表示内容を作成する
	va_start(argptr, message);
	if ( type <= XM_DbgLOG ){
		message = MessageText(message);
		if ( tstrlen(message) >= TSIZEOF(buf) ){
			CriticalMessageBox(T("Internal error : Message flow!"));
			return;
		}
		wvsprintf(buf, message, argptr);
	}else if ( type <= XM_DUMPLOG ){
		DWORD size;
		TCHAR *destp;
		const BYTE *srcp;
		int i;

		size = va_arg(argptr, DWORD);
		destp = buf;
		srcp = (const BYTE *)message;
		if ( type == XM_DUMPLOG ) destp += wsprintf(destp, T("%s\r\n"), title);
		while( size ){
			if ( destp >= (buf + TSIZEOF(buf) - (PTRLEN + 1 + 6)) ) break;
			destp += wsprintf(destp, T(PTRPRINTFORMAT) T(":"), srcp);
			for ( i = 0 ; i < 16 ; i++ ){
				if ( destp >= (buf + TSIZEOF(buf) - 6) ) break;
				destp += wsprintf(destp, T(" %02X"), *srcp++);
				size--;
				if ( size == 0 ) break;
			}
			destp += wsprintf(destp, T("\r\n"));
		}
	}else{
		tstrcpy(buf, T("Unknown type"));
	}
	va_end(argptr);
	//----------------------------------------------------- アイコン設定 ------
	if ( type < 16 ){
		icon = BeepType[type & 7];
	}else if ( type == 16 ){
		icon = MB_ICONSTOP;
	}else if ( type < 24 ){
		icon = MB_ICONEXCLAMATION;
	}else{
		icon = MB_ICONINFORMATION;
	}
	//---------------------------------------------------------- ログ出力を行う
	if ( X_log & flag ){
		TCHAR temp[MAX_PATH];
		DWORD size;
		HANDLE hFile;
		SYSTEMTIME ltime;

		temp[0] = '\0';
		GetCustData(T("X_save"), &temp, sizeof(temp));
		if ( temp[0] == '\0' ) tstrcpy(temp, DLLpath);
		VFSFixPath(NULL, temp, DLLpath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
		CatPath(NULL, temp, T("PPX.LOG"));
		GetLocalTime(&ltime);

		hFile = CreateFileL(temp, GENERIC_WRITE, 0,
				NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			DWORD len;
			#ifdef UNICODE
			if ( GetLastError() == NO_ERROR ){
				WriteFile(hFile, UCF2HEADER, UCF2HEADERSIZE, &size, NULL);
			}
			#endif
			size = 0;							// 末尾に移動 -----------------
			SetFilePointer(hFile, 0, (PLONG)&size, FILE_END);
												// 時刻を出力 -----------------
			wsprintf(temp, T("%4d-%02d-%02d %02d:%02d:%02d.%03d (%s) "),
					ltime.wYear, ltime.wMonth, ltime.wDay,
					ltime.wHour, ltime.wMinute, ltime.wSecond,
					ltime.wMilliseconds, MessageType[type]);
			WriteFile(hFile, temp, TSTRLENGTH32(temp), &size, NULL);
												// 内容を出力 -----------------
			len = TSTRLENGTH32(buf); // C6001 XM_DUMPLOG, size = 0 の時に。無視
			if ( (len > 4) && (buf[(len / sizeof(TCHAR)) - 2] == '\r') ){
				len -= sizeof(TCHAR) * 2;
			}
			WriteFile(hFile, buf, len, &size, NULL);
												// 改行 -------------------
			WriteFile(hFile, T("\r\n"), TSTROFF(2), &size, NULL);
			CloseHandle(hFile);
		}
	}
	//----------------------------------------------------- ダイアログ表示/Beep
	/* 0000 1001 1111 1111 0000 0000 1111 1111 */
	if ( 0x09ff00ff & flag ){
		PMessageBox(hWnd, buf, (title != NULL) ? title : PPxName,
				MB_APPLMODAL | MB_OK | icon);
	/* 0001 0010 1111 1111 1111 1111 0000 0000 */
	}else if ( X_beep & flag & 0x12ffff00 ){
		MessageBeep(icon);
	}
	return;
}

PPXDLL void PPXAPI GetDesktopRect(HWND hWnd, RECT *desktop)
{
	if ( GetSystemMetrics(SM_CMONITORS) > 1 ){	// マルチモニタ
		if ( DMonitorFromWindow == NULL ){
			LoadWinAPI(User32DLL, NULL, MONITORDLL, LOADWINAPI_GETMODULE);
		}
		if ( DMonitorFromWindow != NULL ){
			HMONITOR hMonitor;
			MONITORINFO MonitorInfo;

			hMonitor = DMonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MonitorInfo.cbSize = sizeof(MonitorInfo);
			DGetMonitorInfoA(hMonitor, &MonitorInfo);
			*desktop = MonitorInfo.rcWork;
		}
	}else{										// シングルモニタ
		SystemParametersInfo(SPI_GETWORKAREA, 0, desktop, 0);
	}
}

UINT USEFASTCALL GetGDIdpi(HWND hWnd)
{
	HDC hDC;
	UINT dpi;

	hDC = GetDC(hWnd);
	dpi = GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(hWnd, hDC);
	return dpi;
}

UINT GetMonitorDPI(HWND hWnd)
{
	UINT dpiX, dpiY;

	if ( hWnd == NULL ) hWnd = GetFocus();
	if ( (DGetDpiForMonitor == NULL) && (hShcore == NULL) ){
		if ( DMonitorFromWindow == NULL ){
			LoadWinAPI(User32DLL, NULL, MONITORDLL, LOADWINAPI_GETMODULE);
		}
		hShcore = LoadSystemDLL(SYSTEMDLL_SHCORE);
		if ( hShcore != NULL ){
			GETDLLPROC(hShcore, GetDpiForMonitor);
		}else{
			hShcore = INVALID_VALUE(HMODULE);
		}
	}
	if ( (DGetDpiForMonitor != NULL) && (DMonitorFromWindow != NULL) ){
		HMONITOR hMonitor;

		hMonitor = DMonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		if ( hMonitor != NULL ){
			DGetDpiForMonitor(hMonitor, xMDT_EFFECTIVE_DPI, &dpiX, &dpiY);
			return dpiX;
		}
	}
	return GetGDIdpi(hWnd);
}

const LOGFONT DefFontInfo[2] = { Default_F_mes , Default_F_unfix };
const TCHAR *DefFontInfoJA[2] = { Default_F_mes_JA , Default_F_unfix_JA };

const TCHAR *PPxFontList[] = {
	T("F_mes"),
	T("F_dlg"),
	T("F_fix"),
	T("F_unfix"),
	T("F_tree"),
	T("F_ctrl"),
};

PPXDLL void PPXAPI GetPPxFont(int type, DWORD dpi, LOGFONTWITHDPI *font)
{
	NONCLIENTMETRICS ncm;

	font->dpi = 0;
	if ( NO_ERROR != GetCustData(PPxFontList[type], font, sizeof(LOGFONTWITHDPI)) ){
		font->dpi = DEFAULT_WIN_DPI;
		switch ( type ){
			case PPXFONT_F_fix:
				GetPPxFont(PPXFONT_F_mes, dpi, font);
				return;

			case PPXFONT_F_tree:
				font->font.lfFaceName[0] = '\0';
				return;

			case PPXFONT_F_ctrl:
				ncm.cbSize = sizeof(ncm);
				SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
				font->font = ncm.lfStatusFont;
				// SPI_GETNONCLIENTMETRICS の dpi を取得
				font->dpi = GetGDIdpi(NULL);
				break;

			default: {
				int ftype = (type == PPXFONT_F_unfix) ? 1 : 0;

				font->font = DefFontInfo[ftype];
				if ( LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE ){
					tstrcpy(font->font.lfFaceName, DefFontInfoJA[ftype]);
				}
			}
		}
	}

	// 高さ指定がなければ適当に設定
	if ( font->font.lfHeight == 0 ){
		if ( type <= PPXFONT_F_fix ){ // F_mes, F_dlg, F_fix
			ncm.cbSize = sizeof(ncm);
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
			font->font.lfHeight = ncm.lfStatusFont.lfHeight;
			if ( font->font.lfHeight == 0 ){
				font->font.lfHeight = T_DEFAULTFONTSIZE;
			}

			if ( type != PPXFONT_F_dlg ){ // フォントを少し大きくする
				// ※PPXFONT_F_dlgはpoint表記のため、元から少し大きくなる
				font->font.lfHeight = font->font.lfHeight * 125 / 100;
			}
		}else{
			font->font.lfHeight = T_DEFAULTFONTSIZE;
		}
	}

	if ( dpi == 0 ) return;

	if ( X_dss == DSS_NOLOAD ){
		X_dss = DSS_DEFFLAGS;
		GetCustData(T("X_dss"), &X_dss, sizeof(X_dss));
	}

	if ( X_dss & DSS_FONT ){ // 画面密度自動スケーリング
		// dpi 未設定なら DEFAULT_WIN_DPI にする
		if ( font->dpi == 0 ) font->dpi = GetGDIdpi(NULL);
		if ( type != PPXFONT_F_dlg ){
			if ( dpi != font->dpi ){
				font->font.lfHeight = (font->font.lfHeight * (int)dpi) / (int)font->dpi;
				font->font.lfWidth  = (font->font.lfWidth  * (int)dpi) / (int)font->dpi;
			}
		}else{
			if ( (OSver.dwMajorVersion < 10) ||
				 ((OSver.dwMajorVersion == 10) && (OSver.dwBuildNumber < WINTYPE_10_BUILD_RS2) ) ){
				// Win10 RS2 以降は、マニフェストのPerMonitorV2で処理
				DWORD GDIdpi = GetGDIdpi(NULL);

				if ( dpi != GDIdpi ){
					font->font.lfHeight = (font->font.lfHeight * (int)dpi) / (int)GDIdpi;
					font->font.lfWidth  = (font->font.lfWidth  * (int)dpi) / (int)GDIdpi;
				}
			}
		}
	}

	if ( (X_dss & DSS_MINFONT) || ((TouchMode & TOUCH_LARGEWIDTH) && (type == PPXFONT_F_ctrl)) ){ // 最小値自動スケーリング
		int minHeight = (type != PPXFONT_F_dlg) ? // 約8pt
				(PPX_FONT_MIN_PIXEL * dpi) / DEFAULT_WIN_DPI :
				PPX_FONT_MIN_PT;

		if ( (TouchMode & TOUCH_LARGEWIDTH) && (type == PPXFONT_F_ctrl) ){
			minHeight = (dpi * 50) >> 8; // 5.0mm ( 50 / 254 ) の近似値
		}

		if ( font->font.lfHeight < 0 ){
			if ( font->font.lfHeight > -minHeight ){
				font->font.lfHeight = -minHeight;
				font->font.lfWidth  = 0;
			}
		}else{
			if ( font->font.lfHeight < minHeight ){
				font->font.lfHeight = minHeight;
				font->font.lfWidth  = 0;
			}
		}
	}
}

// 指定した方向へウィンドウをデスクトップの 1/32 だけ移動させる ---------------
PPXDLL void PPXAPI MoveWindowByKey(HWND hWnd, int offx, int offy)
{
	int x, y, stepX, stepY, fixX = 0, fixY = 0;
	RECT box, desktop;

	hWnd = GetCaptionWindow(hWnd);
	GetWindowRect(hWnd, &box);

	if ( DDwmGetWindowAttribute == NULL ){
		HANDLE hDWMAPI = GetModuleHandle(T("DWMAPI.DLL"));

		if ( hDWMAPI != NULL ){
			GETDLLPROC(hDWMAPI, DwmGetWindowAttribute);
		}
		if ( DDwmGetWindowAttribute == NULL ){
			DDwmGetWindowAttribute = INVALID_VALUE(impDwmGetWindowAttribute);
		}
	}
	if ( (DDwmGetWindowAttribute != INVALID_VALUE(impDwmGetWindowAttribute)) &&
		SUCCEEDED(DDwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &desktop, sizeof(desktop))) ){
		if ( desktop.left > box.left ){ // 実体の枠がウィンドウの枠より小さい（Windows10）
			fixX = box.left - desktop.left;
			box.left -= fixX;
			fixY = box.top - desktop.top;
			box.top -= fixY;
		}
	}
	GetDesktopRect(hWnd, &desktop);
											// X 移動
	stepX = (desktop.right - desktop.left) >> 5;
	if ( offx == 0 ){
		x = box.left;
	}else{
		x = ((box.left / stepX) + offx) * stepX;
	}
	stepY = (desktop.bottom - desktop.top) >> 5;
	if ( offy == 0 ){
		y = box.top;
	}else{
		y = ((box.top / stepY) + offy) * stepY;
	}

	if ( GetCustDword(T("X_mmon"), 0) == 0 ){
		if ( (x + box.right - box.left) <= (desktop.left + stepX) ){
			x = desktop.left + stepX - (box.right - box.left);
		}
		if ( x >= (desktop.right - stepX)) x = desktop.right - stepX;

		if ( (y + box.bottom - box.top) <= (desktop.top + stepY) ){
			y = desktop.top + stepY - (box.bottom - box.top);
		}
		if ( y >= (desktop.bottom - stepY)) y = desktop.bottom - stepY;
	}
	SetWindowPos(hWnd, NULL, x + fixX, y + fixY, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

PPXDLL void PPXAPI PPxHelp(HWND hWnd, UINT command, DWORD_PTR data)
{
	TCHAR path[VFPS], temp[VFPS];
	DWORD attr;

	if ( (data == 0) && (command != HELP_FINDER) ) return;
	if ( data == MAX16 ) return;
	if ( data == MAX32 ) return;

	// _others:winhelp が示す Winhelp32.exe を直接使用
	temp[0] = '\0';
	GetCustTable(StrCustOthers, T("winhelp"), temp, sizeof(temp));
	if ( temp[0] != '\0' ){
		if ( command == HELP_KEY ){
			wsprintf(path, T("-k%s %s\\%s"), data, DLLpath, T(WINHELPFILE));
		}else{
			wsprintf(path, T("%s\\%s"), DLLpath, T(WINHELPFILE));
		}
		if ( NULL != PPxShellExecute(hWnd, NULL, temp, path, NilStr, 0, path) ){
			return;
		}
	}
	CatPath(path, DLLpath,
		(WinType >= WINTYPE_VISTA) ? T(HTMLHELPFILE) : T(WINHELPFILE));
	if ( (attr = GetFileAttributesL(path)) == BADATTR ){
		tstrcpy(path, T(TOROsWEB) T(HTMLHELPPAGE));
	}
	// html help を開く
	if ( (WinType >= WINTYPE_VISTA) || (attr == BADATTR) ){
		HANDLE result;
		TCHAR *tail;

		tail = path + tstrlen(path);
		if ( (command == HELP_CONTEXT) || (command == HELP_CONTEXTPOPUP) ){
			wsprintf(tail, T("#%d"), (int)data);
		}else if ( command == HELP_KEY ){
			wsprintf(tail, T("#%s"), data);
		}else if ( attr == BADATTR ){
			tstrcpy(path, T(TOROsWEB) T(HTMLHELPINDEX));
		}
		if ( attr != BADATTR ){
			wsprintf(temp, T("file://%s"), path);
			tstrcpy(path, temp);
		}
		temp[0] = '\0';
		GetCustTable(T("A_exec"), T("browser"), temp, sizeof(temp));
		if ( temp[0] == '\0' ){
			result = PPxShellExecute(hWnd, NULL, path, NilStr, NilStr, 0, path);
		}else{
			result = PPxShellExecute(hWnd, NULL, temp, path, NilStr, 0, path);
		}
		if ( result == NULL ) PopupErrorMessage(hWnd, NULL, path);
	}else{ // ppx.hlp を開く
		WinHelp(hWnd, path, command, data);
	}
}

BOOL PPxDialogHelp(HWND hDlg)
{
	DWORD ID;

	ID = (DWORD)SendMessage(hDlg, WM_COMMAND, IDQ_GETDIALOGID, 0);
	if ( !ID ) return FALSE;
	PPxHelp(hDlg, HELP_CONTEXT, ID);
	return TRUE;
}

void DrawDialogBack(HWND hDlg, HDC hDC)
{
	RECT box;

	GetClientRect(hDlg, &box);
	FillRect(hDC, &box, hWndBackBrush);
}

#pragma argsused
PPXDLL BOOL PPXAPI PPxDialogHelper(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UnUsedParam(wParam); UnUsedParam(lParam);

	switch (message){
		case WM_ERASEBKGND:
			if ( hWndBackBrush == NULL ) return FALSE;
			DrawDialogBack(hDlg, (HDC)wParam);
			return TRUE;
/*
		case WM_CTLCOLORDLG:
			if ( hWndBackBrush == NULL ) return FALSE;
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (DWORD_PTR)hWndBackBrush);
			return TRUE;
*/
		case WM_DESTROY:
			SetIMEDefaultStatus(hDlg);
			return FALSE;

		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;

		case WM_HELP:
//		case WM_CONTEXTMENU:
			PPxDialogHelp(hDlg);
			break;
		default:
			if ( message == WM_PPXCOMMAND ) return ERROR_INVALID_FUNCTION;
			return FALSE;
	}
	return TRUE;
}

BOOL OpenClipboard2(HWND hWnd)
{
	int trycount = 6;

	for (;;){
		if ( IsTrue(OpenClipboard(hWnd)) ) return TRUE;
		if ( --trycount == 0 ) return FALSE;
		Sleep(20);
	}
}

void ClipTextData(HWND hWnd, const TCHAR *text)
{
	INT_PTR len;
	HGLOBAL hG;

	len = TSTRSIZE(text);
	hG = GlobalAlloc(GMEM_MOVEABLE, len);
	if ( hG == NULL ) return;
	memcpy(GlobalLock(hG), text, len);
	GlobalUnlock(hG);
	OpenClipboard2(hWnd);
	EmptyClipboard();
	SetClipboardData(CF_TTEXT, hG);
	CloseClipboard();
}

#define CLIPMSGTEXTSIZE 0x4000
#define CLIPMSGTITLESIZE 0x800

void ClipMessageBox(HWND hDlg)
{
	TCHAR text[CLIPMSGTEXTSIZE], *p;
	HMENU hPopupMenu;
	POINT pos;
	int index;

	hPopupMenu = CreatePopupMenu();
	AppendMenuString(hPopupMenu, 1, MES_MBCP);
	GetCursorPos(&pos);
	index = TrackPopupMenu(hPopupMenu, TPM_TDEFAULT, pos.x, pos.y, 0, hDlg, NULL);
	DestroyMenu(hPopupMenu);
	if ( index > 0 ){
		text[0] = '\0';
		GetWindowText(hDlg, text, CLIPMSGTITLESIZE);
		p = text + tstrlen(text);
		*p++ = '\r';
		*p++ = '\n';
		GetDlgItemText(hDlg, 0xffff, p, CLIPMSGTEXTSIZE - CLIPMSGTITLESIZE);
		ClipTextData(hDlg, text);
	}
}

void PlayWave(const TCHAR *name)
{
	TCHAR path[VFPS];

	if ( name[0] == '\0' ) name = NULL; // 再生停止
	if ( hWinmm == NULL ){
		hWinmm = LoadSystemDLL(SYSTEMDLL_WINMM);
		if ( hWinmm != NULL ) GETDLLPROCT(hWinmm, PlaySound);
	}
	if ( hWinmm != NULL ){
		if ( name != NULL ) VFSFullPath(path, CONSTCAST(TCHAR *, name), DLLpath);
		if ( IsTrue(DPlaySound(name, NULL,
				SND_FILENAME | SND_NOWAIT | SND_ASYNC)) ){
			return;
		}
	}
	MessageBeep(MB_ICONASTERISK);
}

BOOL MessageBoxInitDialog(HWND hDlg, MESSAGEDATA *md)
{
	HDC hDC;
	NONCLIENTMETRICS ncm;
	HFONT hOldFont;	//一時保存用
	int spaceH; // 各種基準につかう間隔(１行分の高さ)
	int WindowWide; // ウィンドウの幅
	const MESSAGEITEMS *mi, *mtype; // メッセージボックスボタンの種類
	int TextHeight;	// テキスト表示用空間の高さ
	int TextAreaLeft;
	int IconType; // アイコンの種類 0==未使用
	int IconWidth; // アイコンの幅(空白含む)
	int IconSize; // アイコンの幅(アイコンのみの大きさ)
	int ButtonCount = 0; // ボタンの数
	int ButtonWidth = 0; // ボタンの幅
	int ButtonHeight; // ボタンの高さ
	RECT msgbox = {0, 0, 0, 0};	// テキストの大きさ
	RECT buttonbox = {0, 0, 0, 0};	// ボタンの大きさ
	RECT tempbox = {0, 0, 0, 0};
	HWND hPWnd = GetParentCaptionWindow(hDlg), hDpiWnd;
	int dpi, gdi_dpi, minHeight;

	md->hOldForegroundWnd = NULL;
	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)md);
	SetWindowText(hDlg, md->title);

	// ダイアログボックスフォントを作成
	ncm.cbSize = sizeof(ncm);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

	hDpiWnd = ((hPWnd != NULL) && !(md->style & MB_PPX_NOCENTER)) ? hPWnd : hDlg;
	dpi = GetMonitorDPI(hDpiWnd);
	hDC = GetDC(hDpiWnd);
	gdi_dpi = GetDeviceCaps(hDC, LOGPIXELSX);

	ncm.lfMessageFont.lfHeight = (ncm.lfMessageFont.lfHeight * dpi) / gdi_dpi;
	minHeight = (PPX_FONT_MIN_PIXEL * dpi) / gdi_dpi; // 約8pt

	if ( ncm.lfMessageFont.lfHeight < 0 ){
		if ( ncm.lfMessageFont.lfHeight > -minHeight ){
			ncm.lfMessageFont.lfHeight = -minHeight;
			ncm.lfMessageFont.lfWidth  = 0;
		}
	}else{
		if ( ncm.lfMessageFont.lfHeight < minHeight ){
			ncm.lfMessageFont.lfHeight = minHeight;
			ncm.lfMessageFont.lfWidth  = 0;
		}
	}

	md->hDlgFont = CreateFontIndirect(&ncm.lfMessageFont);
	hOldFont = SelectObject(hDC, md->hDlgFont);

	{	// メッセージボックスの種類を決定
		int type;

		type = md->style & 0xf;
		if ( type >= MB_PPX_ADDABORTRETRYIGNORE ){
			setflag(md->style, MB_PPX_EXMSGBOX);
		}
		if ( type >= MB_PPX_MAX ) type = 0;
		mtype = MessageTypes[type];
	}
										// 大きさを計算 ------------
		// ボタンの大きさを決定
	for ( mi = mtype ; mi->text != NULL ; mi++ ){
		ButtonCount++;
		buttonbox.right = 0;
		DrawText(hDC, MessageText(mi->text), -1, &buttonbox, DT_CALCRECT | DT_NOCLIP);
		if ( ButtonWidth < buttonbox.right ) ButtonWidth = buttonbox.right;
	}
	spaceH = buttonbox.bottom;
	if ( ButtonWidth < (spaceH * 4) ) ButtonWidth = spaceH * 4;
	ButtonWidth += spaceH * 2;
	WindowWide = (ButtonWidth + spaceH) * ButtonCount + spaceH;

	IconType = (md->style >> 4) & 7;
	if ( IconType ){
	 	IconSize = GetSystemMetrics(SM_CXICON);
	 	IconWidth = IconSize + spaceH / 2; // アイコンサイズを決定
	}else{
	 	IconSize = 32;
		IconWidth = spaceH;
	}
	{
		// テキストの大きさを決定
		int textmaxW;

		DrawText(hDC, md->text, -1, &msgbox, DT_CALCRECT | DT_NOCLIP |
				DT_EXPANDTABS | DT_NOPREFIX | DT_LEFT | DT_EDITCONTROL );
		if ( msgbox.right > 600 ){
			msgbox.right = 600;
			DrawText(hDC, md->text, -1, &msgbox, DT_CALCRECT | DT_WORDBREAK |
				DT_EXPANDTABS | DT_NOPREFIX | DT_LEFT | DT_EDITCONTROL);
		}
		textmaxW = msgbox.right + spaceH * (1 + 1) + IconWidth;
		if ( WindowWide < textmaxW ) WindowWide = textmaxW;
		TextAreaLeft = ( ButtonCount == 1 ) ?
				(WindowWide - (textmaxW - spaceH * (1 + 1)) ) / 2 : spaceH;
		if ( msgbox.bottom < IconSize ){
			TextHeight = IconSize + spaceH * 2;
		}else{
			TextHeight = msgbox.bottom + spaceH * 2;
		}
	}
										// 作成 ------------
	if ( IconType ){ // アイコン
		HWND hWnd;

		hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, STATICstr, NilStr,
				SS_ICON | WS_CHILD | WS_VISIBLE, TextAreaLeft, spaceH,
				IconSize, IconSize, hDlg, CHILDWNDID(20), DLLhInst, 0);
		if ( hWnd != NULL ){
			SendMessage(hWnd, WM_SETFONT, (WPARAM)md->hDlgFont, 0);
			SendMessage(hWnd, STM_SETICON, (WPARAM)LoadImage(NULL,
					MAKEINTRESOURCE((int)IDI_HAND + IconType - 1),
					IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED), 0);
		}
	}
	{				// テキスト
		HWND hWnd;

#ifndef SS_EDITCONTROL
#define SS_EDITCONTROL 0x2000
#endif
		hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, STATICstr, md->text,
				SS_LEFT | SS_NOPREFIX | WS_CHILD | WS_VISIBLE | SS_EDITCONTROL,
				TextAreaLeft + IconWidth, (TextHeight - msgbox.bottom) / 2,
				msgbox.right, msgbox.bottom, hDlg, CHILDWNDID(0xffff), DLLhInst, 0);
		if ( hWnd != NULL ) SendMessage(hWnd, WM_SETFONT, (WPARAM)md->hDlgFont, 0);
	}
	if ( md->style & (MB_PPX_EXMSGBOX | MB_PPX_ALLCHECKBOX) ){
		setflag(md->style, MB_PPX_EXMSGBOX);
		if ( md->style & MB_PPX_ALLCHECKBOX ){	// チェックボックス
			HWND hWnd;
			const TCHAR *caption;

			caption = MessageText(MessageAllReactionStr);
			DrawText(hDC, caption, -1, &tempbox, DT_CALCRECT |
					DT_NOCLIP | DT_NOPREFIX | DT_LEFT | DT_EDITCONTROL);
			hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, BUTTONstr, caption,
					BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					TextAreaLeft + IconWidth / 2, TextHeight,
					tempbox.right + spaceH * 2, tempbox.bottom,
					hDlg, CHILDWNDID(IDX_QDALL), DLLhInst, 0);
			if ( hWnd != NULL ){
				SendMessage(hWnd, WM_SETFONT, (WPARAM)md->hDlgFont, 0);
					TextHeight += tempbox.bottom + spaceH;
			}
		}
	}
	SelectObject(hDC, hOldFont);
	ReleaseDC(hDpiWnd, hDC);
	{				// ボタン
		int left, focus = 0;

		if ( md->style & (MB_DEFBUTTON2 | MB_DEFBUTTON3 | MB_DEFBUTTON4) ){
			// MB_DEFBUTTON1～4 は 0x000～0x300
			focus = (md->style & MB_DEFBUTTON4) >> 8;
		}
		ButtonHeight = buttonbox.bottom + spaceH;
		if ( ButtonCount < 2 ){
			left = (WindowWide - ButtonWidth) / 2;
		}else{
			left = WindowWide - (ButtonWidth + spaceH) * ButtonCount;
		}
		for ( mi = mtype ; mi->text != NULL ; mi++ ){
			HWND hWnd;
			DWORD bstyle;

			bstyle = focus ? BS_PUSHBUTTON | BS_CENTER | WS_CHILD | WS_VISIBLE | WS_TABSTOP : BS_PUSHBUTTON | BS_CENTER | WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
			hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, BUTTONstr,
					MessageText(mi->text), bstyle,
					left, TextHeight, ButtonWidth, ButtonHeight, hDlg,
					(HMENU)(LONG_PTR)(int)mi->ID, DLLhInst, 0);
			if ( hWnd != NULL ){
				SendMessage(hWnd, WM_SETFONT, (WPARAM)md->hDlgFont, 0);
				if ( focus == 0 ) SetFocus(hWnd);
			}
			left += ButtonWidth + spaceH;
			focus--;
		}
	}
										// ウィンドウの大きさを調整 -----------
	GetWindowRect(hDlg, &msgbox);
	GetClientRect(hDlg, &buttonbox);
	SetWindowPos(hDlg, NULL, 0, 0,
			WindowWide + ((msgbox.right - msgbox.left) - buttonbox.right),
			TextHeight + ButtonHeight + spaceH +
					((msgbox.bottom - msgbox.top) - buttonbox.bottom),
			SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
	if ( !(md->style & MB_PPX_NOCENTER) ) MoveCenterWindow(hDlg, hPWnd);
	if ( md->style & MB_PPX_AUTORETRY ){
		SetTimer(hDlg, TIMERID_MSGBOX_AUTORETRY, TIMERRATE_MSGBOX_AUTORETRY, md->autoretryfunc);
	}
										// フォーカスを設定 -----------
	ActionInfo(hDlg, NULL, AJI_SHOW, T("msg"));
	InvalidateRect(hDlg, NULL, FALSE);
	return FALSE;
}

INT_PTR CALLBACK MessageBoxDxProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( iMsg ){
		case WM_INITDIALOG:
			return MessageBoxInitDialog(hDlg, (MESSAGEDATA *)lParam);

		case WM_ACTIVATE: {
			MESSAGEDATA *md;

			md = (MESSAGEDATA *)GetWindowLongPtr(hDlg, DWLP_USER);
			if ( LOWORD(wParam) == WA_INACTIVE ){
				md->hOldForegroundWnd = NULL; // 非アクティブになった
			}else{
				md->hOldForegroundWnd = (HWND)lParam; // 初めてのアクティブなら値が有効、2回目以降はNULLらしい
			}
			return FALSE;
		}

		case WM_CONTEXTMENU:
			ClipMessageBox(hDlg);
			break;

		case WM_COMMAND:
			if ( ((LOWORD(wParam) >= IDOK) && (LOWORD(wParam) <= IDNO)) ||
				 (LOWORD(wParam) == IDB_QDADDLIST) ||
				 (LOWORD(wParam) == IDX_FOP_ADDNUMDEST) ){
				MESSAGEDATA *md;

				md = (MESSAGEDATA *)GetWindowLongPtr(hDlg, DWLP_USER);
				if ( md->style & MB_PPX_ALLCHECKBOX ){
					if ( IsDlgButtonChecked(hDlg, IDX_QDALL) ){
						setflag(wParam, ID_PPX_CHECKED);
					}
				}
				if ( md->hOldForegroundWnd != NULL ){
					SetForegroundWindow(md->hOldForegroundWnd);
					SetActiveWindow(md->hOldForegroundWnd);
				}
				EndDialog(hDlg, LOWORD(wParam));
			}
			break;

		case WM_DESTROY: {
			MESSAGEDATA *md;

			md = (MESSAGEDATA *)GetWindowLongPtr(hDlg, DWLP_USER);
			if ( md->style & MB_PPX_AUTORETRY ){
				KillTimer(hDlg, TIMERID_MSGBOX_AUTORETRY);
			}
			DeleteObject(md->hDlgFont);
			if ( md->hOldFocusWnd != NULL ) SetFocus(md->hOldFocusWnd);
			break;
		}
		default:
			return FALSE;
	}
	return TRUE;

}

PPXDLL int PPXAPI PMessageBox(HWND hWnd, const TCHAR *text, const TCHAR *title, UINT style)
{
#if UseTMessageBox
	MESSAGEDATA md;

	md.title = MessageText((title != NULL) ? title : PPxName);
	if ( text == NULL ){
		if ( style & MB_ICONQUESTION ) text = MES_QABO;
	}
	md.text = MessageText(text);
	md.style = style;
	md.hOldFocusWnd = GetFocus();
	if ( hWnd == NULL ) hWnd = md.hOldFocusWnd;
	return (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL), hWnd, MessageBoxDxProc, (LPARAM)&md);
#else
	return MessageBox(hWnd, MessageText(text), MessageText(title), style);
#endif
}

// PMessageBox / XMessage が使えない時用
int CriticalMessageBox(const TCHAR *text)
{
	return MessageBox(NULL, text, CriticalTitle, MB_APPLMODAL | MB_OK | MB_ICONEXCLAMATION);
}

/*
	WORD	TipHelpTextへのオフセット
	BYTE[]	Executeデータ
	TCHAR[]	TipHelpText
*/

PPXDLL void PPXAPI LoadToolBarBitmap(HWND hWnd, const TCHAR *bmpname, TBADDBITMAP *tbab, SIZE *bmpsize)
{
	HBITMAP hBMP;
	HDC hDC;
	int videocolorbits;
	int IconSize;
	DWORD monitordpi;

	if ( bmpname[0] != '\0' ){ // bmpファイル有り
		HTBMP hTbmp;

		if ( IsTrue(LoadBMP(&hTbmp, bmpname, BMPFIX_TOOLBAR)) ){
			HPALETTE hOldPal C4701CHECK;

			hDC = GetDC(hWnd);
			if ( hTbmp.hPalette != NULL ){
				hOldPal = SelectPalette(hDC, hTbmp.hPalette, FALSE);
				RealizePalette(hDC);
			}

			hBMP = CreateDIBitmap(hDC, hTbmp.DIB, CBM_INIT,
					hTbmp.bits, (BITMAPINFO *)hTbmp.DIB, DIB_RGB_COLORS);
			if ( hTbmp.hPalette != NULL ) SelectPalette(hDC, hOldPal, FALSE);  // C4701ok
			ReleaseDC(hWnd, hDC);

			tbab->hInst = NULL;
			tbab->nID   = (UINT_PTR)hBMP;
			*bmpsize = hTbmp.size;
			FreeBMP(&hTbmp);
			return;
		}
	}
	// リソースから取得する
	bmpsize->cy = 16;

	hDC = GetDC(hWnd);
	videocolorbits = GetDeviceCaps(hDC, BITSPIXEL);
	IconSize = GetSystemMetrics(SM_CXICON);
	monitordpi = GetMonitorDPI(hWnd);
	if ( (X_dss & DSS_ICON) && (monitordpi > 105) ){
		int minsize = (monitordpi * 85) >> 8; // 8.5mm ( 85 / 254 ) の近似値
		// iOS:7mm Android:9mm(7-10mm)

		IconSize = (IconSize * (int)monitordpi) / DEFAULT_WIN_DPI;
		if ( IconSize < minsize ) IconSize = minsize;
	}

	ReleaseDC(hWnd, hDC);
	if ( videocolorbits > 16 ){
		if ( WinType <= WINTYPE_VISTA ){
										// Windows XP
			if ( WinType < WINTYPE_VISTA ){

				hBMP = LoadBitmap(hShell32, MAKEINTRESOURCE(217));
				if ( hBMP != NULL ){
					bmpsize->cx = 752;
					tbab->hInst = NULL;
					tbab->nID   = (UINT_PTR)hBMP;
					return;
				}
			}else{
										// Windows Vista
				hBMP = LoadBitmap(hShell32, VistaBitmapBar);
				if ( hBMP != NULL ){
					bmpsize->cx = 768;
					tbab->hInst = NULL;
					tbab->nID   = (UINT_PTR)hBMP;
					return;
				}
			}
		}else{
										// Windows 7, 8, 10
			if ( hIEframe == NULL ){
				hIEframe = LoadLibraryEx(T("IEFRAME.DLL"), NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
			}
			if ( hIEframe != NULL ){
				if ( IconSize >= 40 ){
					hBMP = LoadBitmap(hIEframe, MAKEINTRESOURCE(215));
					if ( hBMP != NULL ){
						bmpsize->cx = 1152;
						bmpsize->cy = 24;
						tbab->hInst = NULL;
						tbab->nID   = (UINT_PTR)hBMP;
						if ( IconSize != (24 * 2) ){
//							UINT gdidpi;

							bmpsize->cx = (1152 * IconSize) / (24 * 2);
							bmpsize->cy = IconSize / 2;
/*
							gdidpi = GetGDIdpi(NULL);
		if ( ((OSver.dwMajorVersion > 10) ||
			 ((OSver.dwMajorVersion == 10) && (OSver.dwBuildNumber >= WINTYPE_10_BUILD_RS2) )) &&
			((monitordpi == gdidpi) && (gdidpi != DEFAULT_WIN_DPI)) ){ // Win10 RS2 以降は、既に補正がかかっているので、其の分調整
			bmpsize->cx = (bmpsize->cx * DEFAULT_WIN_DPI) / gdidpi;
			bmpsize->cy = (bmpsize->cy * DEFAULT_WIN_DPI) / gdidpi;
		}
*/
							tbab->nID = (UINT_PTR)CopyImage(hBMP, IMAGE_BITMAP, bmpsize->cx, bmpsize->cy, LR_COPYDELETEORG | LR_COPYRETURNORG);
						}
						return;
					}
				}
				hBMP = LoadBitmap(hIEframe, MAKEINTRESOURCE(217));
				if ( hBMP != NULL ){
					bmpsize->cx = 768;
					tbab->hInst = NULL;
					tbab->nID   = (UINT_PTR)hBMP;
					return;
				}
			}
		}
	}
#if 0 // PPLIB.DLL にビットマップを用意したので使う必要が無くなった
										// Etc
	if ( hBrowseui == NULL ){
		hBrowseui = LoadLibraryEx(T("BROWSEUI.DLL"), NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
	}
	if ( hBrowseui != NULL ){
		hBMP = LoadBitmap(hBrowseui, MAKEINTRESOURCE(275));
		if ( hBMP != NULL ){
			DeleteObject(hBMP);
			bmpsize->cx = 736;
			tbab->hInst = hBrowseui;
			tbab->nID   = 275;
			return;
		}
		FreeLibrary(hBrowseui);
	}
#endif
						// 内蔵BMP
	bmpsize->cx = 752;
	// ※tbab->nID に直接 HBITMAP を載せると、紫色の背景色変換が効かない
	tbab->hInst = DLLhInst;
	tbab->nID   = TOOLBARIMAGE;

	if ( IconSize >= 4 ){ // マスク(紫)が表示されてしまうが小さいよりはまし。
		hBMP = LoadBitmap(DLLhInst, MAKEINTRESOURCE(TOOLBARIMAGE));

		bmpsize->cx = (752 * IconSize) / (16 * 2);
		bmpsize->cy = IconSize / 2;
		tbab->nID = (UINT_PTR)CopyImage(hBMP, IMAGE_BITMAP, bmpsize->cx, bmpsize->cy, LR_COPYDELETEORG |  LR_COPYRETURNORG);
		tbab->hInst = NULL;
	}
	return;
}

// style を追加可能に
PPXDLL HWND PPXAPI CreateToolBar(ThSTRUCT *thCmd, HWND hParentWnd, UINT *ID, const TCHAR *custname, const TCHAR *currentpath, DWORD style)
{
	HWND hTBWnd;
	TBADDBITMAP tbab;
	TCHAR buf[VFPS], *p;
	SIZE bmpsize;
	int buttons;
	int index = 0;
	TBBUTTON *tb, *tbp;
	TCHAR tiptext[CMDLINESIZE];
	TOOLBARCUSTTABLESTRUCT tbs;
	int bsize;
	ThSTRUCT buttontext;
	int textindex = 0;

	ThInit(&buttontext);

	buttons = CountCustTable(custname);
	if ( buttons <= 0 ) return NULL;
	buf[0] = '\0';
								// ツールバーに登録するボタンの配列を作成する
	tbp = tb = HeapAlloc(ProcHeap, 0, sizeof(TBBUTTON) * buttons);
	if ( tbp == NULL ) return NULL;
	for ( ; ; ){
		bsize = EnumCustTable(index++, custname, tiptext, &tbs, sizeof(tbs));
		if ( bsize < 0 ) break;

		if ( !tstrcmp(tiptext, T("@")) ){ // Bitmap
			VFSFixPath(buf, tbs.text + 1, currentpath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
			buttons--;
			continue;
		}

		tbp->iBitmap = tbs.index;
		tbp->fsState = TBSTATE_ENABLED;
		tbp->fsStyle = TBSTYLE_BUTTON;
		tbp->bReserved[0] = 0;
		if ( !tstrcmp(tiptext, T("--")) ){
			tbp->iBitmap = 0;
			tbp->fsStyle = TBSTYLE_SEP;
			tbp++;
			continue;
		}

		tbp->idCommand = (*ID)++;

		if ( (UTCHAR)tbs.text[0] != EXTCMD_CMD ){
			switch(*(WORD *)(&tbs.text[1])){
				case K_raw | K_c | K_lf:
				case K_raw | K_c | K_ri:
				case K_raw | K_bs:
					tbp->fsStyle = TBSTYLE_DROPDOWN;
					break;
				//default: なし
			}
		}

		ThAppend(thCmd, &bsize, sizeof(WORD)); // Size登録
		tbp->dwData = thCmd->top;	// Cmdの位置を記憶
		ThAppend(thCmd, tbs.text, bsize);	// Cmd登録

		PP_ExtractMacro(hParentWnd, NULL, NULL, tiptext, tiptext, XEO_DISPONLY);

		p = tstrchr(tiptext, '/'); // バー表示用のテキストがあるか
		if ( p == NULL ){
			p = tiptext;
			tbp->iString = 0;
		}else{
			*p++ = '\0';	// チップテキスト抽出
			ThAddString(&buttontext, tiptext);
			tbp->iString = textindex++;
			setflag(tbp->fsStyle, BTNS_SHOWTEXT | TBSTYLE_AUTOSIZE);
		}
		ThAddString(thCmd, p);
		tbp++;
	}
								// ツールバーのイメージを取得する
	LoadToolBarBitmap(hParentWnd, buf, &tbab, &bmpsize);
								// ツールバーウィンドウを作成する
	LoadCommonControls(ICC_BAR_CLASSES);
	// 編集可能にする : CCS_ADJUSTABLE | TBSTYLE_ALTDRAG
	style |= WS_CHILD | CCS_NOPARENTALIGN | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;
	if ( textindex ) setflag(style, TBSTYLE_LIST);	// テキスト表示に必要
	hTBWnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, style,
		0, 0, 0, 0, hParentWnd, CHILDWNDID(IDW_GENTOOLBAR), GetModuleHandle(NULL), NULL);
	if ( hTBWnd != NULL ){
//		if ( X_dss & DSS_COMCTRL ) SendMessage(hTBWnd, CCM_DPISCALE, TRUE, 0);

		SendMessage(hTBWnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

		if ( textindex ){
			SendMessage(hTBWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
			// buttontext は、ThAddString により必ず末尾が \0\0 になっている
			SendMessage(hTBWnd, TB_ADDSTRING, 0, (LPARAM)buttontext.bottom);
			ThFree(&buttontext);
		}else{
			SendMessage(hTBWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
		}
		SendMessage(hTBWnd, TB_SETBITMAPSIZE, 0, TMAKELPARAM(bmpsize.cy, bmpsize.cy));
		if ( tbab.nID ){
			SendMessage(hTBWnd, TB_ADDBITMAP, bmpsize.cx / bmpsize.cy, (WPARAM)&tbab);
		}
		// 使用ビットマップはウィンドウ廃棄時廃棄する？
		SendMessage(hTBWnd, TB_ADDBUTTONS, buttons, (LPARAM)tb);
		SendMessage(hTBWnd, TB_AUTOSIZE, 0, 0);
		ShowWindow(hTBWnd, SW_SHOW);
	}
	HeapFree(ProcHeap, 0, tb);
	return hTBWnd;
}
PPXDLL BOOL PPXAPI SetToolBarTipText(HWND hToolbarWnd, ThSTRUCT *thCmd, NMHDR *nmh)
{
	BYTE *p;
	TBBUTTON tb;
	UINT ID;

	ID = (UINT)SendMessage(hToolbarWnd, TB_COMMANDTOINDEX, nmh->idFrom, 0);
	if ( (int)ID < 0 ) return FALSE;
	if ( !SendMessage(hToolbarWnd, TB_GETBUTTON, ID, (LPARAM)&tb) ) return FALSE;
	if ( (thCmd->bottom == NULL) || (tb.dwData >= thCmd->top) ) return FALSE;
	p = (BYTE *)(thCmd->bottom + tb.dwData);
	((LPTOOLTIPTEXT)nmh)->lpszText = (TCHAR *)(BYTE *)(p + *(WORD *)(p - 2));
	((LPTOOLTIPTEXT)nmh)->hinst = NULL;
	return TRUE;
}

PPXDLL TCHAR * PPXAPI GetToolBarCmd(HWND hToolbarWnd, ThSTRUCT *thCmd, UINT ID)
{
	TBBUTTON tb;

	ID = (UINT)SendMessage(hToolbarWnd, TB_COMMANDTOINDEX, ID, 0);
	if ( !SendMessage(hToolbarWnd, TB_GETBUTTON, ID, (LPARAM)&tb) ) return NULL;
	SendMessage(hToolbarWnd, TB_GETBUTTON, ID, (LPARAM)&tb);
	return (TCHAR *)(BYTE *)(thCmd->bottom + tb.dwData);
}

void PopupErrorMessage(HWND hWnd, const TCHAR *title, const TCHAR *msg)
{
	DWORD Pid = 0;

	GetWindowThreadProcessId(hWnd, &Pid);

	if ( (Pid == GetCurrentProcessId()) && (SendMessage(hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_SETPOPMSG, POPMSG_MSG), (LPARAM)msg) != 1) ){
		XMessage(hWnd, title, XM_GrERRld, T("%s"), msg);
	}
}

void FillBox(HDC hDC, const RECT *box, HBRUSH hbr)
{
	HGDIOBJ hOldObj;

	hOldObj = SelectObject(hDC, hbr);
	PatBlt(hDC, box->left, box->top, box->right - box->left, box->bottom - box->top, PATCOPY);
	SelectObject(hDC, hOldObj);
}

//-----------------------------------------------------------------------------
#define CCH_RM_SESSION_KEY (sizeof(GUID) * 2)
#define CCH_RM_MAX_APP_NAME 255
#define CCH_RM_MAX_SVC_NAME 63
#define RmRebootReasonNone 0
typedef struct {
	DWORD dwProcessId;
	FILETIME ProcessStartTime;
} RM_UNIQUE_PROCESS;

typedef struct {
	RM_UNIQUE_PROCESS Process;
	WCHAR strAppName[CCH_RM_MAX_APP_NAME + 1];
	WCHAR strServiceShortName[CCH_RM_MAX_SVC_NAME + 1];
	int /*RM_APP_TYPE*/ ApplicationType;
	ULONG AppStatus;
	DWORD TSSessionId;
	BOOL bRestartable;
} RM_PROCESS_INFO;

DefineWinAPI(ERRORCODE, RmStartSession, (DWORD *pSessionHandle, DWORD dwSessionFlags, WCHAR *strSessionKey));
DefineWinAPI(ERRORCODE, RmRegisterResources, (DWORD dwSessionHandle, UINT nFiles, LPCWSTR *rgsFileNames, UINT nApplications, RM_UNIQUE_PROCESS rgApplications[], UINT nServices, LPCWSTR *rgsServiceNames));
DefineWinAPI(ERRORCODE, RmGetList, (DWORD dwSessionHandle, UINT *pnProcInfoNeeded, UINT *pnProcInfo, RM_PROCESS_INFO *rgAffectedApps, LPDWORD lpdwRebootReasons));
DefineWinAPI(ERRORCODE, RmEndSession, (DWORD dwSessionHandle));

LOADWINAPISTRUCT RSTRTMGRDLL[] = {
	LOADWINAPI1(RmStartSession),
	LOADWINAPI1(RmRegisterResources),
	LOADWINAPI1(RmGetList),
	LOADWINAPI1(RmEndSession),
	{NULL, NULL}
};

PPXDLL int PPXAPI GetAccessApplications(const TCHAR *checkpath, TCHAR *text)
{
	WCHAR SessionKey[CCH_RM_SESSION_KEY + 1];
	DWORD hRm;
	int infocount = 0;
	HANDLE hRSTRTMGR = LoadSystemWinAPI(SYSTEMDLL_RSTRTMGR, RSTRTMGRDLL);

#ifdef UNICODE
	#define path checkpath
#else
	WCHAR path[VFPS];

	AnsiToUnicode(checkpath, path, VFPS);
#endif
	*text = '\0';
	if ( hRSTRTMGR == NULL ) return -1;

	strcpyW(SessionKey, L"PPxVFS");
	if ( NO_ERROR == DRmStartSession(&hRm, 0, SessionKey) ){
		LPCWSTR PathPtr = path;

		if ( NO_ERROR == DRmRegisterResources(hRm, 1, &PathPtr, 0, NULL, 0, NULL) ){
			UINT pnProcInfoNeeded, pnProcInfo = 0;
			DWORD lpdwRebootReasons = RmRebootReasonNone;

			if ( ERROR_MORE_DATA == DRmGetList(hRm, &pnProcInfoNeeded,
					&pnProcInfo, NULL, &lpdwRebootReasons) ){ // 数計算
				RM_PROCESS_INFO *processInfo;

				infocount = pnProcInfo = pnProcInfoNeeded;
				processInfo = HeapAlloc(ProcHeap, HEAP_ZERO_MEMORY,
						sizeof(RM_PROCESS_INFO) * pnProcInfoNeeded);

				if ( processInfo != NULL ) {
					if ( (pnProcInfo > 0) &&
						 (NO_ERROR == DRmGetList(hRm, &pnProcInfoNeeded,
							&pnProcInfo, processInfo, &lpdwRebootReasons)) ) {
						DWORD i;
						TCHAR *textmax = text + VFPS;

						tstrcpy(text, MessageText(MES_ADHD));
						text += tstrlen(text);
						for ( i = 0; i < pnProcInfo; i++ ) {
							if ( i != 0 ) *text++ = '\r';
							text += wsprintf(text, T(" %d: "), processInfo[i].Process.dwProcessId);
							strcpyWToT(text, processInfo[i].strAppName, MAX_PATH);
							text += tstrlen(text);
							if (text >= textmax) break;
						}
						if (infocount - i) wsprintf(text, MessageText(MES_ACET), infocount - i);
					}
					HeapFree(ProcHeap, 0, processInfo);
				}
			}
		}
		DRmEndSession(hRm);
	}
	FreeLibrary(hRSTRTMGR);
	return infocount;
}

void InitSysColors_main(void)
{
	C_3dHighlight = GetSysColor(COLOR_BTNHIGHLIGHT);
	C_3dFace = GetSysColor(COLOR_BTNFACE);
	C_3dShadow = GetSysColor(COLOR_BTNSHADOW);
	C_WindowText = GetSysColor(COLOR_WINDOWTEXT);
	C_WindowBack = GetSysColor(COLOR_WINDOW);
	C_HighlightText = GetSysColor(COLOR_HIGHLIGHTTEXT);
	C_HighlightBack = GetSysColor(COLOR_HIGHLIGHT);
	C_GrayState = GetSysColor(COLOR_GRAYTEXT);
#if 0
	hWndBackBrush = Get3dShadowBrush(); //Get3dFaceBrush
#endif
}

void FreeSysColors(void)
{
	if ( h3dHighlight != NULL ){
		DeleteObject(h3dHighlight);
		h3dHighlight = NULL;
	}
	if ( h3dFace != NULL ){
		DeleteObject(h3dFace);
		h3dFace = NULL;
	}
	if ( h3dShadow != NULL ){
		DeleteObject(h3dShadow);
		h3dShadow = NULL;
	}
	if ( hHighlightBack != NULL ){
		DeleteObject(hHighlightBack);
		hHighlightBack = NULL;
	}
	if ( hGrayBack != NULL ){
		DeleteObject(hGrayBack);
		hGrayBack = NULL;
	}
#if 0
	if ( hWndBackBrush != NULL ){
		DeleteObject(hWndBackBrush);
		hWndBackBrush = NULL;
	}
#endif
	C_3dFace = C_AUTO;
}

HBRUSH Get3dHighlightBrush(void)
{
	if ( h3dHighlight != NULL ) return h3dHighlight;
	if ( C_3dFace == C_AUTO ) InitSysColors_main();
	h3dHighlight = CreateSolidBrush(C_3dHighlight);
	return h3dHighlight;
}

HBRUSH Get3dFaceBrush(void)
{
	if ( h3dFace != NULL ) return h3dFace;
	if ( C_3dFace == C_AUTO ) InitSysColors_main();
	h3dFace = CreateSolidBrush(C_3dFace);
	return h3dFace;
}

HBRUSH Get3dShadowBrush(void)
{
	if ( h3dShadow != NULL ) return h3dShadow;
	if ( C_3dFace == C_AUTO ) InitSysColors_main();
	h3dShadow = CreateSolidBrush(C_3dShadow);
	return h3dShadow;
}

HBRUSH GetHighlightBackBrush(void)
{
	if ( hHighlightBack != NULL ) return hHighlightBack;
	if ( C_3dFace == C_AUTO ) InitSysColors_main();
	hHighlightBack = CreateSolidBrush(C_HighlightBack);
	return hHighlightBack;
}

HBRUSH GetGrayBackBrush(void)
{
	if ( hGrayBack != NULL ) return hGrayBack;
	if ( C_3dFace == C_AUTO ) InitSysColors_main();
	hGrayBack = CreateSolidBrush(C_GrayState);
	return hGrayBack;
}

// 常に EDGE_RAISED
PPXDLL void PPXAPI DrawSeparateLine(HDC hDC, const RECT *box, UINT flags)
{
	RECT facebox, edgebox;

	edgebox = facebox = *box;
	if ( flags & BF_TOP ){
		edgebox.bottom = facebox.top = edgebox.top + 1;
		FillRect(hDC, &edgebox, Get3dHighlightBrush());
		edgebox.bottom = box->bottom;
	}
	if ( (flags & BF_BOTTOM) && ((facebox.bottom - facebox.top) >= 3) ){
		edgebox.top = facebox.bottom = edgebox.bottom - 1;
		FillRect(hDC, &edgebox, Get3dShadowBrush());
	}
	if ( flags & BF_LEFT ){
		edgebox.right = facebox.left = edgebox.left + 1;
		FillRect(hDC, &edgebox, Get3dHighlightBrush());
		edgebox.right = box->right;
	}
	if ( (flags & BF_RIGHT) && ((facebox.right - facebox.left) >= 3) ){
		edgebox.left = facebox.right = edgebox.right - 1;
		FillRect(hDC, &edgebox, Get3dShadowBrush());
	}
	FillRect(hDC, &facebox, Get3dFaceBrush());
}
