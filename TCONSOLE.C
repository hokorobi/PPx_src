/*-----------------------------------------------------------------------------
	Paper Plane bUI								～ コンソールライブラリ ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
//#include <winbase.h>
#include <wincon.h>
#include <string.h>
#include "PPX.H"
#include "VFS.H"
#include "PPB.H"
#include "TCONSOLE.H"
#pragma  hdrstop

#ifdef UNICODE
void LoadCharWideTable(void);
DWORD *CharWideTable = NULL;
#endif

typedef struct {
	ULONG cbSize;
	DWORD nFont;
	COORD dwFontSize;
	UINT FontFamily;
	UINT FontWeight;
	WCHAR FaceName[LF_FACESIZE];
} xCONSOLE_FONT_INFOEX;

DefineWinAPI(BOOL, GetConsoleScreenBufferInfoEx, (HANDLE hConsoleOutput, xCONSOLE_SCREEN_BUFFER_INFOEX *lpConsoleScreenBufferInfoEx));
DefineWinAPI(BOOL, SetConsoleScreenBufferInfoEx, (HANDLE hConsoleOutput, xCONSOLE_SCREEN_BUFFER_INFOEX *lpConsoleScreenBufferInfoEx));
DefineWinAPI(BOOL, GetCurrentConsoleFontEx, (HANDLE hConsoleOutput, BOOL bMaximumWindow, xCONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx)) = NULL;
DefineWinAPI(BOOL, SetCurrentConsoleFontEx, (HANDLE hConsoleOutput, BOOL bMaximumWindow, xCONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx));

HANDLE hStdin, hStdout;

CONSOLE_CURSOR_INFO OldCsrMod ={0, 0};	// 起動時のカーソルの状態
DWORD OldStdinMode, OldStdoutMode;		// 起動時のコンソールの状態

TSCREENINFO screen;

CONSOLE_CURSOR_INFO NowCsr;				// 現在のカーソルの状態

COORD WindowSize[2] = {{80, 200},{80, 30}};
CALLBACKMODULEENTRY KeyHookEntry = NULL;
SHORT DefaultFontY = 0; // 元のフォントサイズ(未設定:0)

#ifndef CONSOLE_FULLSCREEN_HARDWARE
#define CONSOLE_FULLSCREEN_HARDWARE B1	// ウィンドウでない
#endif
DefineWinAPI(BOOL, GetConsoleDisplayMode, (LPDWORD lpModeFlags)) = INVALID_VALUE(impGetConsoleDisplayMode);

COLORREF palettes[16] = {
	C_AUTO, C_AUTO, C_AUTO, C_AUTO,
	C_AUTO, C_AUTO, C_AUTO, C_AUTO,
	C_AUTO, C_AUTO, C_AUTO, C_AUTO,
	C_AUTO, C_AUTO, C_AUTO, C_AUTO
};

#ifdef WINEGCC
#include "signal.h"
#include "curses.h"

WINDOW *winewin = NULL;
short int DefPair, NegPair;
#endif
/*-----------------------------------------------------------------------------
	初期化・終了処理
-----------------------------------------------------------------------------*/
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS 0x80
#endif

#ifndef WINEGCC
void tInitCustomize(void)
{
	if ( DGetConsoleScreenBufferInfoEx == NULL ){
		GETDLLPROC(hKernel32, GetConsoleScreenBufferInfoEx);
		GETDLLPROC(hKernel32, SetConsoleScreenBufferInfoEx);
		GETDLLPROC(hKernel32, GetCurrentConsoleFontEx);
		GETDLLPROC(hKernel32, SetCurrentConsoleFontEx);
	}
	if ( DSetCurrentConsoleFontEx != NULL ){
		LOGFONT confont;
		xCONSOLE_FONT_INFOEX cfi;

		if ( GetCustData(T("F_con"), &confont, sizeof(confont)) == NO_ERROR ){
			cfi.cbSize = sizeof(cfi);
			cfi.nFont = 0;
			cfi.dwFontSize.X = (SHORT)confont.lfWidth;
			cfi.dwFontSize.Y = (SHORT)confont.lfHeight;
			cfi.FontFamily = (UINT)confont.lfPitchAndFamily;;
			cfi.FontWeight = (UINT)confont.lfWeight;
			strcpyToW(cfi.FaceName, confont.lfFaceName, LF_FACESIZE);
			DSetCurrentConsoleFontEx(hStdout, FALSE, &cfi);
		}
	}
	screen.info.cbSize = sizeof(screen.info);
	if ( (DGetConsoleScreenBufferInfoEx != NULL) &&
		 IsTrue(DGetConsoleScreenBufferInfoEx(hStdout, &screen.info)) ){
		int i;
		BOOL change = FALSE;

		GetCustData(T("CB_pals"), &palettes, sizeof(palettes));
		for ( i = 0 ; i < 16 ; i++ ){
			if ( palettes[i] != C_AUTO ){
				screen.info.ColorTable[i] = palettes[i];
				change = TRUE;
			}
		}
		if ( change ){
			DSetConsoleScreenBufferInfoEx(hStdout, &screen.info);
		}
	}
}
#endif

BOOL tInit(const TCHAR *title)
{
	if ( OldCsrMod.dwSize == 0 ){
		if( title != NULL ) SetConsoleTitle(title);		// タイトル表示
									// 起動時情報の取得 -----------------------
										// 標準入出力ハンドルの取得
		hStdin	= GetStdHandle(STD_INPUT_HANDLE);
		hStdout	= GetStdHandle(STD_OUTPUT_HANDLE);
										// 標準入出力状態の取得
		GetConsoleMode(hStdin, &OldStdinMode);
		GetConsoleMode(hStdout, &OldStdoutMode);
	#ifndef WINEGCC
		tInitCustomize();
									// 標準出力のカーソル状態
		if ( GetConsoleCursorInfo(hStdout, &OldCsrMod) == FALSE ){
			return FALSE; // Wineのとき
		}
	#endif
		NowCsr.dwSize = OldCsrMod.dwSize;
		NowCsr.bVisible = OldCsrMod.bVisible;
	}
	#ifdef WINEGCC // Wine の端末設定
	if ( winewin == NULL ){
		short int fc, bc;
		attr_t attrs;

		winewin = initscr();
		start_color();
		cbreak();
		keypad(winewin, TRUE);
		attr_get(&attrs, &DefPair, NULL);
		pair_content(DefPair, &fc, &bc);
		NegPair = (DefPair == 1) ? 2 : 1;
		init_pair(NegPair, bc, fc);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		halfdelay(1);
	}
	#endif
										// 標準入力の設定
	SetConsoleMode(hStdin,
//		 ENABLE_LINE_INPUT |			// 一行入力許可
//		 ENABLE_ECHO_INPUT |			// 入力文字のエコー
//		 ENABLE_PROCESSED_INPUT |		// ^C 等を入力文字にしない
		 ENABLE_WINDOW_INPUT |			// window 関連入力の許可
		 ENABLE_MOUSE_INPUT |			// mouse 関連入力の許可
		ENABLE_EXTENDED_FLAGS			// 簡易編集無し
	);
	GetConsoleWindowInfo(hStdout, &screen.basic.info);
	{
	#ifdef UNICODE
		DWORD XB_uwid = 0;

		GetCustData(T("XB_uwid"), &XB_uwid, sizeof(XB_uwid));
		if ( XB_uwid ) LoadCharWideTable();
	#else
		OSVERSIONINFO osver;

		osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		if ( IsTrue(GetVersionEx(&osver)) &&
			 (osver.dwPlatformId == VER_PLATFORM_WIN32_NT) )
	#endif
		{
												// ウィンドウの大きさを変更
			GetCustData(T("XB_size"), &WindowSize, sizeof(WindowSize));
			SetConsoleScreenBufferSize(hStdout, WindowSize[0]);

			screen.info.srWindow.Right= (SHORT)(screen.info.srWindow.Left + WindowSize[1].X - 1);
			screen.info.srWindow.Bottom=(SHORT)(screen.info.srWindow.Top  + WindowSize[1].Y - 1);
			SetConsoleWindowInfo(hStdout, TRUE, &screen.info.srWindow);
		}
	}
	GetConsoleWindowInfo(hStdout, &screen.basic.info);
	return TRUE;
}
void tRelease(void)
{
	#ifdef UNICODE
	if ( CharWideTable != NULL ){
		HeapFree( GetProcessHeap(), 0, CharWideTable);
		CharWideTable = NULL;
	}
	#endif
	SetConsoleTextAttribute(hStdout, screen.info.wAttributes);
	SetConsoleCursorPosition(hStdout, screen.info.dwCursorPosition);

	SetConsoleCursorInfo(hStdout, &OldCsrMod);
	SetConsoleMode(hStdin, OldStdinMode);
	SetConsoleMode(hStdout, OldStdoutMode);
	#ifdef WINEGCC
		if ( winewin != NULL ){
		nocbreak();
		keypad(winewin, FALSE);
		echo();
		endwin();
		winewin = NULL;

		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	#endif
}

#ifdef WINEGCC
void GetConsoleWindowInfo(HANDLE conout, CONSOLE_SCREEN_BUFFER_INFO *scrinfo)
{
	GetConsoleScreenBufferInfo(conout, &screen.basic.info);
	getmaxyx(winewin, screen.info.dwSize.Y, screen.info.dwSize.X);
	getyx(winewin, screen.info.dwCursorPosition.Y, screen.info.dwCursorPosition.X);
	screen.info.srWindow.Bottom = 0;
	screen.info.srWindow.Top = 20;
}
#endif

/*-----------------------------------------------------------------------------
	キー入力処理
-----------------------------------------------------------------------------*/
/*--------------------------------------
	キー入力

	inf は NULL でもよい
--------------------------------------*/
int tgetc(INPUT_RECORD *inf)
{
	INPUT_RECORD con;
	DWORD key = 0;

#ifndef WINEGCC
	DWORD read;
	HANDLE hWait[2];

	hWait[0] = hStdin;
	hWait[1] = hCommSendEvent;

	for ( ; ; ){
		read = WaitForMultipleObjects(2, hWait, FALSE, INFINITE);
		if ( read == WAIT_OBJECT_0 + 1 ){ // SendPPB [5]
			if ( inf != NULL ) inf->EventType = 0;
			return KEY_RECV;
		}else if ( read == WAIT_OBJECT_0 ){				// 読み込み
			con.EventType = 0;
			ReadConsoleInput(hStdin, &con, 1, &read);
			if ( read == 0 ) continue;
			switch( con.EventType ){
				case KEY_EVENT:
					if ( con.Event.KeyEvent.bKeyDown == FALSE ) continue;
					#ifdef UNICODE
						key = con.Event.KeyEvent.uChar.UnicodeChar;
						if ( key > 0xff ){
							key = (key & 0xff) | ((key & 0xff00) << 8);
						}
					#else
						key = con.Event.KeyEvent.uChar.AsciiChar;
						if ( key >= 0xffff80 ) key &= 0xff;	// S-JIS の処理
					#endif
					if ( (con.Event.KeyEvent.dwControlKeyState &
							(RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) &&
							(key > 0) && (key < 32) ){
						key += '@';
					}
					if ( key < 32 ){
						key = K_v | con.Event.KeyEvent.wVirtualKeyCode;
					}
					if ( con.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED){
						key |= K_s;
					}
					if ( con.Event.KeyEvent.dwControlKeyState &
							(RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED) ){
						if ( (key <= 'z') && ( key >= 'a' ) ) key -= 0x20;
						key |= K_a;
					}
					if ( con.Event.KeyEvent.dwControlKeyState &
							(RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED) ){
						if ( key < 32 ) key += '@';
						key |= K_c;
					}
					break;
				case MOUSE_EVENT:
					break;
				default:
					continue;
			}
			break;
		}else{
			xmessage(XM_FaERRld, MES_FBDW);
			PostMessage(hMainWnd, WM_CLOSE, 0, 0);
		}
	}
	if ( inf != NULL ) *inf = con;
	GetConsoleWindowInfo(hStdout, &screen.basic.info);
#else
	key = getch();
	if ( inf != NULL ){
		inf->EventType = KEY_EVENT;
	}
	switch ( key ){ // 1-31(^Cを除く) Ctrl+A
		case FFD: // 入力無し
			if ( WaitForSingleObject(hCommSendEvent, 0) == WAIT_OBJECT_0 ){ // SendPPB [5]
				if ( inf != NULL ) inf->EventType = 0;
				return KEY_RECV;
			}
			return 0;

		case 10:
		case KEY_ENTER:
			key = K_cr;
			break;

		case 0x14b: return K_ins;
		case 0x12e: return K_del;
		case 0x17f: return K_s | K_del;
		case 0x153: return K_Pup;
		case 0x152: return K_Pdw;
		case 0x161: return K_s | K_tab;

		case 27: {
			TCHAR extkeys[100], *extmax, *extp;

			extmax = extkeys + 99;
			for ( extp = extkeys ; extp < extmax ; extp++ ){
				key = getch();
				if ( key == FFD ){
					break;
				}
				*extp = key;
				if ( IsupperA(key) ){
					extp++;
					key = 0;
					break;
				}
			}
			*extp = '\0';
			if ( key == FFD ) return K_esc; // ESC 単独
			if ( tstrcmp(extkeys, T("[3;5~")) == 0 ) return K_c | K_del;
			if ( tstrcmp(extkeys, T("[5;5~")) == 0 ) return K_c | K_Pup;
			if ( tstrcmp(extkeys, T("[6;5~")) == 0 ) return K_c | K_Pdw;
			if ( tstrcmp(extkeys, T("[1;2A")) == 0 ) return K_s | K_up;
			if ( tstrcmp(extkeys, T("[1;2B")) == 0 ) return K_s | K_dw;
			if ( tstrcmp(extkeys, T("[1;5A")) == 0 ) return K_c | K_up;
			if ( tstrcmp(extkeys, T("[1;5B")) == 0 ) return K_c | K_dw;
			if ( tstrcmp(extkeys, T("[1;5D")) == 0 ) return K_c | K_lf;
			if ( tstrcmp(extkeys, T("[1;5C")) == 0 ) return K_c | K_ri;
			// 未知 ESC
			getyx(winewin, screen.info.dwCursorPosition.Y, screen.info.dwCursorPosition.X);
			tputposstr(40, 0, extkeys);
			tCsrPos(screen.info.dwCursorPosition.X, screen.info.dwCursorPosition.Y);
			return 0;
		}
//		case 'Q':
//			key = K_esc;
//			break;

		case KEY_DOWN:
			key = K_dw;
			break;

//		case KEY_SDOWN:
//			key = K_s | K_dw;
//			break;

		case KEY_UP:
			key = K_up;
			break;

//		case KEY_SUP:
//			key = K_s | K_up;
//			break;

		case KEY_LEFT:
			key = K_lf;
			break;

		case KEY_SLEFT:
			key = K_s | K_lf;
			break;

		case KEY_RIGHT:
			key = K_ri;
			break;

		case KEY_SRIGHT:
			key = K_s | K_ri;
			break;

		case KEY_HOME:
			key = K_home;
			break;

		case KEY_BACKSPACE:
			key = K_bs;
			break;

		case KEY_DC:
			key = K_del;
			break;

		case KEY_F(1):
			key = K_F1;
			break;

		case KEY_F(4):
			key = K_F4;
			break;

		case KEY_END:
			key = K_end;
			break;

		default:
			if ( (key >= 1) && (key <= 26) ){
				key += K_c | '@';
				break;
			}
	}
#if 0
	{
		TCHAR buf[100];

		getyx(winewin, screen.info.dwCursorPosition.Y, screen.info.dwCursorPosition.X);
		wsprintf(buf, T("%03d  0x%03x"), key, key);
		tputposstr(0, 0, buf);
		tCsrPos(screen.info.dwCursorPosition.X, screen.info.dwCursorPosition.Y);
	}
#endif
#endif
	return key;
}
/*-----------------------------------------------------------------------------
	カーソル処理
-----------------------------------------------------------------------------*/
/*--------------------------------------
	カーソルの座標を指定する
--------------------------------------*/
void tCsrPos(int x, int y)
{
#ifndef WINEGCC
	COORD xy;

	xy.X = (SHORT)x;
	xy.Y = (SHORT)y;
	SetConsoleCursorPosition(hStdout, xy);
#else
	move(y, x);
#endif
}
/*--------------------------------------
	カーソルを表示する
	-1		消去する
	 0		以前の大きさで表示
	 1-100	指定した割合で表示
--------------------------------------*/
void tCsrMode(int size)
{
	if ( size < 0 ){
		CONSOLE_CURSOR_INFO cur = { 1 , FALSE };
		SetConsoleCursorInfo(hStdout, &cur);
		return;
	}
	if ( size ) NowCsr.dwSize = size;
	SetConsoleCursorInfo(hStdout, &NowCsr);
}
/*-----------------------------------------------------------------------------
	文字表示処理
-----------------------------------------------------------------------------*/
/*--------------------------------------
	座標・属性を指定して表示
--------------------------------------*/
void tputatrstr(int x, int y, WORD atr, const TCHAR *str)
{
	#ifndef WINEGCC
		COORD xy;
		DWORD dummy;

		xy.X = (SHORT)x;
		xy.Y = (SHORT)y;
		SetConsoleCursorPosition(hStdout, xy);
		SetConsoleTextAttribute(hStdout, atr);
		WriteConsole(hStdout, str, tstrlen32(str), &dummy, NULL);
	#else
		move(y, x);
		tputstr(str);
	#endif
}

/*--------------------------------------
	座標を指定して表示
--------------------------------------*/
void tputposstr(int x, int y, const TCHAR *str)
{
	#ifndef WINEGCC
		COORD xy;
		DWORD dummy;

		xy.X = (SHORT)x;
		xy.Y = (SHORT)y;
		SetConsoleCursorPosition(hStdout, xy);
		WriteConsole(hStdout, str, tstrlen32(str), &dummy, NULL);
	#else
		move(y, x);
		tputstr(str);
	#endif
}

/*--------------------------------------
	文字列の表示
--------------------------------------*/
void tputstr(const TCHAR *str)
{
	#ifndef WINEGCC
		DWORD dummy;

		if ( WriteConsole(hStdout, str, tstrlen32(str), &dummy, NULL) == FALSE ){
			WriteFile(hStdout, str, TSTRLENGTH32(str), &dummy, NULL);
		}
	#else
		#ifdef UNICODE
			char strA[0x1000];

			strcpyToA(strA, str, sizeof(strA));
			addstr(strA);
			refresh();
		#else
			addstr(str);
			refresh();
		#endif
	#endif
}

void tputstr_noinit(const TCHAR *str)
{
	#ifndef WINEGCC
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	#endif
	tputstr(str);
}

/*-----------------------------------------------------------------------------
	ボックス処理
-----------------------------------------------------------------------------*/
/*--------------------------------------
	指定範囲の表示属性を変更する
		(x1, y1)-(x2, y2), atr
--------------------------------------*/
void tFillAtr(int x1, int y1, int x2, int y2, WORD atr)
{
	#ifndef WINEGCC
		COORD xy;
		DWORD dummy;

		xy.X = (SHORT)x1;
		xy.Y = (SHORT)y1;
		for ( ; xy.Y <= y2 ; xy.Y++ ){
			FillConsoleOutputAttribute(hStdout, atr, (DWORD)(x2 - x1) + 1, xy, &dummy);
		}
	#else
		BOOL negmode;

		negmode = ((atr >> 4)& 0xf) > (atr & 0xf);
		tCsrPos(x1, y1);
		chgat(x2 - x1 + 1, 0, negmode ? NegPair : DefPair, NULL);
		tCsrPos(screen.info.dwCursorPosition.X, screen.info.dwCursorPosition.Y);
	#endif
}
/*--------------------------------------
	指定範囲を指定文字で埋める
	（属性は変更しない）
		(x1, y1)-(x2, y2), chr
--------------------------------------*/
void tFillChr(int x1, int y1, int x2, int y2, TCHAR chr)
{
	COORD xy;
	DWORD dummy;

	xy.X = (SHORT)x1;
	xy.Y = (SHORT)y1;
	for ( ; xy.Y <= y2 ; xy.Y++ ){
		FillConsoleOutputCharacter(hStdout, chr, (DWORD)(x2 - x1) + 1, xy, &dummy);
	}
}

/*--------------------------------------
	ボックスを描く
--------------------------------------*/
void tBox(int x1, int y1, int x2, int y2)
{
	COORD xy;
	DWORD dummy;
	TCHAR tmp[500];
	int i;

	SetConsoleTextAttribute(hStdout, T_CYA);
										// 左右
	for ( i = y1 + 1 ; i < y2 ; i++ ){
		xy.Y = (SHORT)i;
		xy.X = (SHORT)x1;
		SetConsoleCursorPosition(hStdout, xy);
		WriteConsole(hStdout, T("\5"), 1, &dummy, NULL);
		xy.X = (SHORT)x2;
		SetConsoleCursorPosition(hStdout, xy);
		WriteConsole(hStdout, T("\5"), 1, &dummy, NULL);
	}

	for ( i = 1 ; i < (x2 - x1) ; i++)	tmp[i] = 6;
	tmp[i++] = 0;
										// 上
	xy.X = (SHORT)x1;
	xy.Y = (SHORT)y1;
	tmp[0] = 1;
	tmp[i - 1] = 2;
	SetConsoleCursorPosition(hStdout, xy);
	WriteConsole(hStdout, tmp, i, &dummy, NULL);
										// 下
	xy.Y = (SHORT)y2;
	tmp[0] = 3;
	tmp[i - 1] = 4;
	SetConsoleCursorPosition(hStdout, xy);
	WriteConsole(hStdout, tmp, i, &dummy, NULL);
}
/*--------------------------------------
	指定範囲を保存する
--------------------------------------*/
void tStore(int x1, int y1, int x2, int y2, CHAR_INFO **ptr)
{
	COORD xy0, xy1 = {0, 0};
	SMALL_RECT xy;

	xy0.X = (SHORT)(x2 - x1 + 1);
	xy0.Y = (SHORT)(y2 - y1 + 1);
	xy.Left = (SHORT)x1;
	xy.Right = (SHORT)x2;
	xy.Top = (SHORT)y1;
	xy.Bottom = (SHORT)y2;

	*ptr = HeapAlloc( GetProcessHeap(), 0 , xy0.X * xy0.Y * sizeof(CHAR_INFO));
	if ( *ptr == NULL ) return;
	ReadConsoleOutput(hStdout, *ptr, xy0, xy1, &xy);
}
/*--------------------------------------
	指定範囲を復旧する
--------------------------------------*/
void tRestore(int x1, int y1, int x2, int y2, CHAR_INFO **ptr)
{
	COORD xy0, xy1 = {0, 0};
	SMALL_RECT xy;

	if ( *ptr == NULL ) return;
	xy0.X = (SHORT)(x2 - x1 + 1);
	xy0.Y = (SHORT)(y2 - y1 + 1);
	xy.Left = (SHORT)x1;
	xy.Right = (SHORT)x2;
	xy.Top = (SHORT)y1;
	xy.Bottom = (SHORT)y2;

	WriteConsoleOutput(hStdout, *ptr, xy0, xy1, &xy);
	HeapFree( GetProcessHeap(), 0, *ptr);
}

int Select(TMENU *tmenu)
{
	INPUT_RECORD key;
	int h;		/*	縦の項目数  */
	int m;		/*	全項目数	*/
	int w, w1 = 0;	/*	幅			*/
	int b, n;	/*	選択項目	*/
	int i, l;
	int k;		/*	キー入力＆フラグ -1:正常 -2:中止 -3:ページャ */
	int x, y, xb, yb;
	int mb = 0;
	CONSOLE_SCREEN_BUFFER_INFO oldinfo;
	CONSOLE_CURSOR_INFO oldcsr;
	CHAR_INFO *win;
	TMENU *t;
	POINT oldpos;

	if ( DGetConsoleDisplayMode == INVALID_HANDLE_VALUE ){
		GETDLLPROC(hKernel32, GetConsoleDisplayMode);
	}
	if ( DGetConsoleDisplayMode != NULL ){
		DWORD displaymode;

		if ( IsTrue(DGetConsoleDisplayMode(&displaymode)) &&
			!(displaymode & CONSOLE_FULLSCREEN_HARDWARE) ){
			HMENU hMenu;

			hMenu = CreatePopupMenu();
			while ( tmenu->mes != NULL ){
				AppendMenu(hMenu, MF_ES, tmenu->id, MessageText(tmenu->mes));
				tmenu++;
			}
			k = (int)PPxCommonExtCommand(K_CPOPMENU, (WPARAM)hMenu);
			DestroyMenu(hMenu);
			return k;
		}
	}
	GetConsoleWindowInfo(hStdout, &oldinfo);
	GetConsoleCursorInfo(hStdout, &oldcsr);
	tCsrMode(-1);
	t = tmenu;
	n = 0;

	l = 1;
												/*	最大文字列長を求める  */
	for ( m = 0 ; t[m].mes ; m++ ){
		i = tstrlen32(t[m].mes + 5); // 漢字幅の計算がいるので、現在は英語のみ
		if ( i > w1 ) w1 = i;
	}
	h = --m;
	if ( m == 0 ) return 0;	/* 項目なし */
	w = w1 + 3;
	xb = screen.info.srWindow.Left;
	yb = screen.info.srWindow.Top;
	x = ((screen.info.srWindow.Right  - xb - w - 2) >> 1) + xb;
	if ( x < xb ) x = xb;
	y = ((screen.info.srWindow.Bottom - yb - h - 2) >> 1) + yb;
	if ( y < yb ) y = yb;

	tStore( x , y , x + w * l, y + h + 2, &win);
	tBox( x , y , x + w * l, y + h + 2);
	tFillAtr(x + 1, y + 1, x + w * l - 1, y + h + 1, T_WHI);
	tFillChr(x + 1, y + 1, x + w * l - 1, y + h + 1, ' ');
												/*	全項目の表示  */
	for ( i = 0 ; i <= m ; i++ ){
		tputatrstr(x + 2, y + i + 1, T_WHI, t[i].mes + 5);
	}
	b = n;
	oldpos.x = -1;
	do {
		if ( b != n ){
			tFillAtr(x + 1, y + b + 1, x + w - 1, y + b + 1, T_WHI);
			b = n;
		}
		tFillAtr(x + 1, y + n + 1, x + w - 1, y + n + 1, TR_WHI);
		do{
			k = tgetc(&key);
			if ( key.EventType == MOUSE_EVENT ){
				if ( ( key.Event.MouseEvent.dwMousePosition.X > x) &&
					 ( key.Event.MouseEvent.dwMousePosition.X < (x + w*l)) &&
					 ( key.Event.MouseEvent.dwMousePosition.Y > y) &&
					 ( key.Event.MouseEvent.dwMousePosition.Y <= (y + h + 1))){
					POINT pos;

					GetCursorPos(&pos);
					if ( (pos.x != oldpos.x) || (pos.y != oldpos.y) ){
						oldpos = pos;
						n = key.Event.MouseEvent.dwMousePosition.Y - y - 1;
					}

					k = K_s;
					if ( key.Event.MouseEvent.dwButtonState ){
						mb = 1;
						break;
					}else{
						if ( mb ){
							k = K_cr;
							break;
						}
					}
				}else{
					if ( key.Event.MouseEvent.dwButtonState ){
						k = K_esc;
						break;
					}
					mb = 0;
				}
			}
		}while( k == 0 );


		switch( k ){
			case K_cr:					// CR
				k = -1;
				break;

			case K_esc:					// ESC
				k = -2;
				break;

			case K_up:					// ↑
				if ( n ){
					n--;
				}else
					n = m;
				break;

			case K_dw:					// ↓
				if ( n < m ){
					n++;
				}else
					n = 0;
				break;

			case K_lf:					// ←
				if ( n > h ){
					n -= h;
					break;
				}

			case K_Pup:					// PgUP
			case K_s | K_up:			// \↑
				n = 0;
				break;

			case K_ri:					// →
				if ( (n + h) <= m ){
					n += h;
					break;
				}

			case K_Pdw:					// PgDW
			case K_s | K_dw:			// \↓
				n = m;
				break;

			default:
										// ショートカットキー
				for ( i = 0 ; i <= m ; i++ ){
					const TCHAR *p;

					p = tstrchr(t[i].mes + 5, '&');
					if ( (p != NULL) && ((k & 0x5f) == (*(p + 1) & 0x5f)) ){
						n = i;
						k = -1;
						break;
					}
				}
				break;
		}
	}while( k >= 0);
	tRestore( x, y, x + w * l, y + h + 2, &win);
	screen.info.dwCursorPosition = oldinfo.dwCursorPosition;
	tCsrPos(oldinfo.dwCursorPosition.X, oldinfo.dwCursorPosition.Y);
	SetConsoleCursorInfo(hStdout, &oldcsr);
	SetConsoleTextAttribute(hStdout, oldinfo.wAttributes);
	return (k == -1) ? t[n].id : 0;
}

void ConChangeFontSize(SHORT delta)
{
	xCONSOLE_FONT_INFOEX cfi;

	if ( DGetCurrentConsoleFontEx == NULL ){
		GETDLLPROC(hKernel32, GetCurrentConsoleFontEx);
		GETDLLPROC(hKernel32, SetCurrentConsoleFontEx);
		if ( DSetCurrentConsoleFontEx == NULL ) return;
	}
	cfi.cbSize = sizeof(cfi);
	DGetCurrentConsoleFontEx(hStdout, FALSE, &cfi);
	if ( DefaultFontY == 0 ) DefaultFontY = cfi.dwFontSize.Y;
	if ( delta != -2 ){
		if ( (cfi.dwFontSize.Y + delta) < 5 ) return;
		cfi.dwFontSize.Y += delta;
	}else{
		cfi.dwFontSize.Y = DefaultFontY;
	}
	DSetCurrentConsoleFontEx(hStdout, FALSE, &cfi);
}

#ifdef UNICODE
#define WIDETABLESIZE (0x10000 / 8)
#define TMPX 15

void LoadCharWideTable(void)
{
	int i, j;
	DWORD *dest, bits, size;
	COORD xy;
	WCHAR bufw[2];

	if ( CharWideTable != NULL ) return;
	CharWideTable = HeapAlloc( GetProcessHeap(), 0, WIDETABLESIZE);
	if ( CharWideTable == NULL ) return;
	if ( GetCustData(T("D_uwid"), CharWideTable, WIDETABLESIZE) == NO_ERROR ){
		return;
	}
											// テーブルを新規作成
	tputposstr(0, screen.info.dwCursorPosition.Y, L"Making table...");

	dest = CharWideTable;
	xy.X = TMPX;
	xy.Y = screen.info.dwCursorPosition.Y;

	bufw[0] = 0;
	bufw[1] = 0;

	for ( i = WIDETABLESIZE / 4 /* DWORD */ ; i ; i-- ){
		bits = 0;
		for ( j = 0 ; j < 32 ; j++, bufw[0]++ ){
			CONSOLE_SCREEN_BUFFER_INFO tmpscreen;

			SetConsoleCursorPosition(hStdout, xy);
			WriteConsoleW(hStdout, bufw, 1, &size, NULL);
			GetConsoleWindowInfo(hStdout, &tmpscreen);
								// TAB のときは 8 になるため TMP+2 で判別
			if ( tmpscreen.dwCursorPosition.X == (TMPX + 2) ){
				bits |= ((DWORD)1 << j);
			}
		}
		*dest++ = bits;
	}
	SetConsoleCursorPosition(hStdout, xy);
	SetCustData(T("D_uwid"), CharWideTable, WIDETABLESIZE);
}

/*-----------------------------------------------------------------------------
	指定された文字の文字幅を求める(Console & UNICODE 用)
	※ テーブルを使わない場合、細かい違いは判断していない
-----------------------------------------------------------------------------*/
int CCharWide(WCHAR c)
{
									// テーブル使用 ---------------------------
	if ( CharWideTable != NULL ){
		return (CharWideTable[c / 32] & (1 << (c & 0x1f))) ? 2 : 1;
	}
									// 簡易決定 -------------------------------
					//もう少し正しくするなら GetStringTypeEx C3_HALFWIDTH で。
	if ( c <= 0x390 ){				// 0000-0390 ASCII からギリシャ文字まで
		return 1;
	}
	if ( c < 0x4e00 ){				// 0391-4dff 漢字より前の記号類
		if ( c >= 0x3000 ){			// 3000-4dff カタカナ・ひらがな類
			return (c < 0x33ff) ? 2 : 1;
		}
		if ( c >= 0x2000 ){				// 2000-2fff 記号類
			if ( c >= 0x2103 ){
				return (c <= 0x27be) ? 2 : 1;	// 2103-27be 記号類2
			}else{
				return (c <= 0x203b) ? 2 : 1;	// 2000-203b 記号類1
			}
		}								// 0391-1fff
		if ( c <= 0x3c9 ) return 2;			// 0391-03c9 ギリシャ文字
		if ( c < 0x410 ) return 1;
		if ( c <= 0x451 ) return 2;			// 0410-0451 ロシア文字
		return 1;
	}else{							// 4e00-ffff 漢字など
		if ( c < 0xff01 ){
			if ( c <= 0x9fa5 ) return 2;	// 4e00-9fa5 漢字1
			if ( c >= 0xe000 ){
				if ( c <= 0xfa2d ) return 2; // e000-fa2d 漢字2
				return 1;
			}
			if ( c < 0xac00 ) return 1;
			if ( c <= 0xd7a3 ) return 2;	// ac00-d7a3 ハングル
			return 1;
		}
		if ( c <= 0xff5e ) return 2;	// ff01-ff5e 全角アルファベット
		if ( c < 0xffe0 ) return 1;		// ff5f-ffe0 半角カタカナ
		if ( c <= 0xffe6 ) return 2;	// ffe0-ffe6 通貨
		return 1;
	}
}
#endif
