/*-----------------------------------------------------------------------------
	コンソール向け定義集
-----------------------------------------------------------------------------*/
#include <wincon.h>

// Win32 SDK で登録されていない可能性があるもの -------------------------------
#ifndef	COMMON_LVB_UNDERSCORE
#define	COMMON_LVB_UNDERSCORE	0x8000
#endif
#ifndef	COMMON_LVB_REVERSE_VIDEO
#define	COMMON_LVB_REVERSE_VIDEO	0x4000
#endif

extern HMODULE hKernel32;
/*-----------------------------------------------------------------------------
		色の定義
-----------------------------------------------------------------------------*/
#define	T_BLA	0
#define	T_GRY	(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
#define	T_DCY	(FOREGROUND_BLUE  | FOREGROUND_GREEN)
#define	T_DGR	FOREGROUND_INTENSITY
#define	T_BLU	(FOREGROUND_BLUE  | FOREGROUND_INTENSITY)
#define	T_RED	(FOREGROUND_RED   | FOREGROUND_INTENSITY)
#define	T_MAG	(FOREGROUND_BLUE  | FOREGROUND_RED | FOREGROUND_INTENSITY)
#define	T_GRE	(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define	T_CYA	(FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define	T_BRO	(FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define	T_WHI	(FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)

#define	TR_BLA	0
#define	TR_DGR	BACKGROUND_INTENSITY
#define	TR_DCY	(BACKGROUND_BLUE  | BACKGROUND_GREEN)
#define	TR_GRY	(BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED)
#define	TR_BLU	BACKGROUND_BLUE
#define	TR_RED	BACKGROUND_RED
#define	TR_MAG	(BACKGROUND_BLUE  | BACKGROUND_RED | BACKGROUND_INTENSITY)
#define	TR_GRE	(BACKGROUND_GREEN | BACKGROUND_INTENSITY)
#define	TR_CYA	(BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_INTENSITY)
#define	TR_BRO	(BACKGROUND_RED   | BACKGROUND_GREEN | BACKGROUND_INTENSITY)
#define	TR_WHI	(BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY)

#define	T_UL	COMMON_LVB_GRID_HORIZONTAL			// 上線
#define	T_DL	COMMON_LVB_UNDERSCORE				// 下線
#define	T_LL	COMMON_LVB_GRID_LVERTICAL			// 左線
#define	T_RL	COMMON_LVB_GRID_RVERTICAL			// 右線
#define	T_R		COMMON_LVB_REVERSE_VIDEO			// 反転

extern BOOL tInit(const TCHAR *title);
extern void tRelease(void);

#ifndef WINEGCC
#define GetConsoleWindowInfo(conout,scrinfo) GetConsoleScreenBufferInfo(conout,scrinfo)
#else
extern void GetConsoleWindowInfo(HANDLE conout,CONSOLE_SCREEN_BUFFER_INFO *scrinfo);
#endif

#define KEY_RECV -1
#define KEY_DUMMY -2
extern int tgetc(INPUT_RECORD *inf);

/*--------------------------------------
	キー入力バッファをクリア
--------------------------------------*/
#define killkey()	FlushConsoleInputBuffer(hStdin)
/*--------------------------------------
	表示属性を変更する
--------------------------------------*/
#define tatr(atr)	SetConsoleTextAttribute(hStdout,atr)

extern void tCsrPos(int x,int y);
extern void tCsrMode(int size);
extern void tputatrstr(int x,int y,WORD atr,const TCHAR *str);
extern void tputposstr(int x,int y,const TCHAR *str);
extern void tputstr(const TCHAR *str);
extern void tputstr_noinit(const TCHAR *str);
extern void tFillAtr(int x1,int y1,int x2,int y2,WORD atr);
extern void tFillChr(int x1,int y1,int x2,int y2,TCHAR chr);
extern void tBox(int x1,int y1,int x2,int y2);
extern void tStore(int x1,int y1,int x2,int y2,CHAR_INFO **ptr);
extern void tRestore(int x1,int y1,int x2,int y2,CHAR_INFO **ptr);

extern int CCharWide(WCHAR c);
extern void ClearFormLine(void);

typedef struct {
	DWORD id;
	TCHAR *mes;	// 表示内容
}TMENU;

extern int Select(TMENU *tmenu);

extern CONSOLE_SCREEN_BUFFER_INFO screen;		// 現在の画面状態
extern HANDLE hStdin,hStdout;
extern CALLBACKMODULEENTRY KeyHookEntry;		// キー入力拡張モジュール

#ifndef FROM_LEFT_1ST_BUTTON_PRESSED
#define FROM_LEFT_1ST_BUTTON_PRESSED B0
#define RIGHTMOST_BUTTON_PRESSED B1
#endif
