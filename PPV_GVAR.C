/*-----------------------------------------------------------------------------
	Paper Plane vUI			Global Value
-----------------------------------------------------------------------------*/
#ifdef GLOBALEXTERN
	#define GVAR extern
	#define GPARAM(x)
	#define GPARAM2(x, y)
	#define DLLGVAR extern
	#define DLLGPARAM(x)
	#define DLLGPARAM2(x, y)
#else
	#undef GVAR
	#undef GPARAM
	#undef GPARAM2
	#define GVAR
	#define GPARAM(x) = x
	#define GPARAM2(x, y) = {x, y}
	#if NODLL
		#define DLLGVAR extern
		#define DLLGPARAM(x)
		#define DLLGPARAM2(x, y)
	#else
		#undef DLLGVAR
		#undef DLLGPARAM
		#undef DLLGPARAM2
		#define DLLGVAR
		#define DLLGPARAM(x) = x
		#define DLLGPARAM2(x, y) = {x, y}
	#endif
#endif

#define TIMERID_DRAGSCROLL	106		// ドラッグによる窓スクロール処理用ID
#define TIMERID_READLINE	107		// 遅延読み込み用タイマID
#define TIMERID_ANIMATE		108		// アニメーション用タイマID
#define TIMERID_DRAW		109		// DirextXアニメーション描画用タイマID
// TIMERID_USER : 0xc000-0xdfff
#define TIMER_DRAGSCROLL	110		// ドラッグスクロールの間隔

#define READLINE_FIRST	4000	// 最初に最低限読む行数
#define READLINE_TIMER	500		// 遅延読み込みする初期行数
#define READLINE_TIMERMIN	100		// 遅延読み込みする最小行数
#define READLINE_SUPP	1000	// 読み込み不足分を補充する行数
#define TIMER_READLINE	100		// 遅延読み込みを行う時間ms
#define READLINE_DISPS (300 / TIMER_READLINE) // 表示間隔(300ms)

// Ctrl + Enter Menu

// 8000-bfff : キーコード
#define MENUID_KEYLAST 0xbfff
#define MENUID_EXT 0xc000 // - 0xc6ff 拡張子別
#define MENUID_TEXTCODE 0xc700 // - 0xc7e7 文字コード
#define MENUID_TEXTCODEMAX 0xc7e7
#define MENUID_TYPE_FIRST	0xc7e8 //
#define MENUID_TYPE_HEX		0xc7e8 //
#define MENUID_TYPE_TEXT		0xc7e9 //
#define MENUID_TYPE_DOCUMENT	0xc7ea //
#define MENUID_TYPE_IMAGE		0xc7eb //
#define MENUID_TYPE_RAWIMAGE	0xc7ec //
#define MENUID_TYPE_LAST	0xc7ec //
#define MENUID_KANAMODE		0xc7f0 // - 0xc7f2 カナ切り替え
#define MENUID_ESCSWITCH	0xc7f3 // エスケープシーケンス解釈
#define MENUID_QUOTEDSWITCH	0xc7f4 // quoted解釈
#define MENUID_TAGSWITCH	0xc7f5 // TAG解釈
#define MENUID_SELECTEDCODE	0xc7f6 // 選択範囲の変更
#define MENUID_ALLCODE		0xc7f7 // 全範囲の変更
#define MENUID_TYPE_TAIL	0xc7fe // tailmode
#define MENUID_SELECTURI	0xc7ff // 選択範囲をURI
#define MENUID_URI 0xc800 // - MENUID_URIMAX 抽出URI
#define MENUID_URIMAX 0xd7ff
#define MENUID_USER 0xd800 // - 0xefff メニュー

GVAR HINSTANCE hInst;
GVAR PPV_APPINFO vinfo;
GVAR OSVERSIONINFO OSver;
GVAR TCHAR PPvPath[MAX_PATH];
GVAR HANDLE PPvHeap; // GetProcessHeap() の値

GVAR const TCHAR StrPPvTitle[] GPARAM(T("PPV"));
GVAR const TCHAR StrUser32DLL[] GPARAM(T("USER32.DLL"));
GVAR const TCHAR StrKernel32DLL[] GPARAM(T("KERNEL32.DLL"));
GVAR const TCHAR *StrLoading;
GVAR DWORD StrLoadingLength;

GVAR HMENU hDispTypeMenu	GPARAM(NULL);
GVAR int FontDPI GPARAM(DEFAULT_WIN_DPI);

GVAR BOOL Embed GPARAM(FALSE); // ウィンドウ埋め込み

//--------------------------------------------------------- 色関係
GVAR HBRUSH C_BackBrush GPARAM(NULL);	// 背景用ブラシ
GVAR int VideoBits;					// 画面の色数(4, 8, 16, 24, 32)

GVAR TCHAR PopMsgStr[VFPS] GPARAM(T(""));
GVAR int PopMsgFlag GPARAM(0);

//----------------------------------------------------------------- Window 管理
GVAR HWND hViewParentWnd GPARAM(NULL);
GVAR BOOL ParentPopup GPARAM(FALSE);
GVAR RECT winS;
GVAR SIZE WndSize GPARAM2(1, 1);
GVAR DYNAMICMENUSTRUCT DynamicMenu;
GVAR UINT WM_PPXCOMMAND;				// PPx 間通信用 Window Message
GVAR HWND hCommonWnd GPARAM(NULL);	// タスクを隠すときのPPtray

GVAR HWND hToolBarWnd GPARAM(NULL);
GVAR ThSTRUCT thGuiWork;		// DockやToolBarなどの記憶領域

GVAR int Use2ndView GPARAM(0); // 分割表示有り
GVAR RECT BoxStatus;	// ステータス行
GVAR RECT BoxAllView;	// 表示領域
GVAR RECT BoxView;	// 表示領域
GVAR RECT Box2ndView;	// 表示領域
GVAR CALLBACKMODULEENTRY KeyHookEntry GPARAM(NULL);	// キー入力拡張モジュール
#define ScrollWidth_MIN 16
GVAR int ScrollWidth; // WIDTH_NOWARP のときの横幅

//---------------------------------------------------------------------- 描画用
GVAR HFONT hBoxFont;
GVAR HFONT hUnfixedFont GPARAM(NULL);
GVAR HFONT hANSIFont GPARAM(INVALID_HANDLE_VALUE);
GVAR HFONT hIBMFont GPARAM(INVALID_HANDLE_VALUE);
GVAR HFONT hSYMFont GPARAM(INVALID_HANDLE_VALUE);
GVAR HBRUSH hStatusLine;
GVAR int fontX GPARAM(1), fontY GPARAM(1), LineY GPARAM(1);
GVAR int fontSYMY; // fontANSIY, fontIBMY;
GVAR int fontWW;		// 漢字のときの補正値
GVAR BOOL XV_unff GPARAM(FALSE);

GVAR int FullDraw;			// 全クライアント画面描画モードなら真
GVAR BGSTRUCT BackScreen;

GVAR int PopupPosType GPARAM(PPT_FOCUS);	// メニューなどを表示するとき使う座標の種類 (TKEY.H の PPT_ )
GVAR POINT PopupPos;		// メニューなどを表示するとき使う座標

	// 全角空白表示用
GVAR const char StrSJISSpace[] GPARAM("□");
GVAR const WCHAR StrWideSpace[] GPARAM(L"□");
//---------------------------------------------------------- オブジェクト共通部
GVAR TCHAR RegCID[REGIDSIZE] GPARAM(T("VA"));
GVAR TCHAR RegID[REGIDSIZE] GPARAM(T(PPV_REGID));

#define VO_type_HEX			B0
#define VO_type_TEXT		B1
#define VO_type_DOCUMENT	B2
#define VO_type_IMAGE		B3
#define VO_type_RAWIMAGE	B4
#define VO_type_ALLDOCUMENT	(VO_type_HEX | VO_type_TEXT | VO_type_DOCUMENT | VO_type_RAWIMAGE)
#define VO_type_ALLTEXT		(VO_type_HEX | VO_type_TEXT | VO_type_RAWIMAGE)
#define VO_type_ALLIMAGE	(VO_type_HEX | VO_type_TEXT | VO_type_IMAGE | VO_type_RAWIMAGE)

#ifdef GLOBALEXTERN
typedef enum {
	DOCMODE_NONE = 0, DOCMODE_TEXT = B0, DOCMODE_HEX = B1, DOCMODE_BMP = B2, DOCMODE_EMETA = B3, DOCMODE_RAWIMAGE = B4
} DOCMODELIST;
#endif

#define VO_dmode_ENABLEBACKREAD (DOCMODE_TEXT | DOCMODE_HEX)
#define VO_dmode_SELECTABLE (DOCMODE_TEXT | DOCMODE_HEX)
#define VO_dmode_TEXTLIKE (DOCMODE_TEXT | DOCMODE_HEX)
#define VO_dmode_IMAGE (DOCMODE_BMP | DOCMODE_EMETA | DOCMODE_RAWIMAGE)

GVAR DISPT VO_dtype GPARAM(DISPT_NONE);	// 表示形式

GVAR int VO_Tmodedef;//	GPARAM(0);
GVAR int VO_Tmode;	//GPARAM(0);	// かなコードの扱い 0:自動判別 1:バイナリ 2:カナ使用
GVAR int VO_Tesc;	//GPARAM(1);
GVAR int VO_Ttag;	//GPARAM(1); // 0:未処理 1:隠す 2:ハイライト
GVAR int VO_Tshow_script;	//GPARAM(1);
GVAR int VO_Tshow_css;	//GPARAM(1);
GVAR int VO_Tquoted;	//GPARAM(0);
GVAR VIEWOPTIONS *VO_opt, VO_optdata;
GVAR int VO_CodePage GPARAM(0);
GVAR BOOL VO_CodePageChanged; // テキスト中のコードページ指定による変更済
GVAR BOOL VO_CodePageValid; // 指定コードページが有効か

#ifdef GLOBALEXTERN
typedef enum {
	FDM_NODIV = 0,	// 一括表示
	FDM_FORCENODIV, // 強制一括表示
	FDM_NODIVMAX,
	FDM_DIV, // 分割表示
	FDM_DIV2ND // 分割表示+先頭以外を表示
} FDM;
#endif

GVAR FDM FileDivideMode GPARAM(FDM_NODIV);

GVAR DDWORDST FileRealSize;
GVAR DDWORDST FileDividePointer
#ifndef GLOBALEXTERN
={0, 0}
#endif
;

GVAR DWORD X_lspc GPARAM(0);

GVAR VO_INFO VO_I[DISPT_MAX], *VOi GPARAM(VO_I);

GVAR VIEWOPTIONS viewopt_def; // 初期値オプション
GVAR VIEWOPTIONS viewopt_opentime; // ファイルを開いたときに適用しているオプション

// GVAR BOOL FileCase GPARAM(FALSE);

#define PRINTMODE_OFF	0
#define PRINTMODE_ONE	1
#define PRINTMODE_FILES	2

GVAR int VO_PrintMode GPARAM(PRINTMODE_OFF);

#define VTYPE_MENU_EXSJIS (VTYPE_MAX + 1)
GVAR const TCHAR textcp_systemcp[] GPARAM(T("local codepage"));
GVAR const TCHAR textcp_sjis[] GPARAM(T("S-JIS"));

GVAR const TCHAR *VO_textS[]
#ifndef GLOBALEXTERN
={	T("IBM/US"),		// 0	VTYPE_IBM
	T("ANSI/Latin1"),	// 1	VTYPE_ANSI
	T("JIS"),		// 2	VTYPE_JIS
	textcp_sjis,	// 3	VTYPE_SYSTEMCP
	T("EUC-JP"),	// 4	VTYPE_EUCJP
	T("UTF-16LE"),	// 5	VTYPE_UNICODE
	T("UTF-16BE"),	// 6	VTYPE_UNICODEB
	T("S-JIS/NEC"),	// 7	VTYPE_SJISNEC
	T("S-JIS/BE"),	// 8	VTYPE_SJISB
	T("KANA"),		// 9	VTYPE_KANA
	T("UTF-8"),		// 10	VTYPE_UTF8 (CP 65000)
	T("UTF-7"),		// 11	VTYPE_UTF7 (CP 65001)
	T("RTF"),		// 12	VTYPE_RTF
	T("other"),		// 13	VTYPE_OTHER
};
#endif
;
GVAR const TCHAR *VO_textM[]
#ifndef GLOBALEXTERN
={	T("Auto"),
	T("&Binary"),
	T("&Kana text")};
#endif
;

GVAR int BackupSelY, BackupOffY, BackuplastY;
GVAR BOOL UseActiveEvent;

DLLGVAR const TCHAR NilStr[1] DLLGPARAM(T(""));
GVAR const TCHAR httpstr[8] GPARAM(T("http://"));
GVAR const TCHAR httpsstr[9] GPARAM(T("https://"));

GVAR PPvViewObject vo_;

#define SHOWTEXTLINE_BOOKMARK B0
#define SHOWTEXTLINE_OLDTEXT B1
#define SHOWTEXTLINE_NEWTEXT B2
GVAR DWORD ShowTextLineFlags;
GVAR DWORD TailModeFlags;
//---------------------------------------------- オブジェクト共通部(各形式共用)
GVAR int VO_sizeX,	VO_sizeY;
GVAR int VO_minX,	VO_minY;
GVAR int VO_maxX,	VO_maxY;
GVAR int VO_stepX,	VO_stepY;

//-------------------------------------------------------- その他
// DLL
GVAR HMODULE hComdlg32 GPARAM(NULL);
GVAR impPPRINTDLG PPrintDlg GPARAM(NULL);

DLLGVAR HMODULE hWinmm DLLGPARAM(NULL);
GVAR ValueWinAPI(sndPlaySound) GPARAM(NULL);
GVAR ValueWinAPI(NotifyWinEvent) GPARAM(DummyNotifyWinEvent);

// /convert 関連
GVAR int convert GPARAM(0);	// 1:text convert
GVAR TCHAR convertname[VFPS];

GVAR VO_SELECT VOsel	// 選択状態の保存
#ifndef GLOBALEXTERN
={
	FALSE,			// cursor

	FALSE, FALSE,	// select, line
	{{0, 0}, {0, 0}},	// start
	{{0, 0}, {0, 0}},	// now
	{{0, 0}, {0, 0}},	// bottom
	{{0, 0}, {0, 0}},	// top
	0, 0,			// bottomOY, topOY
	0,				// posdiffX

	FALSE,			// highlight
	-1, -1,			// lastY, foundY
	"", L""			// string
};
#endif
;
GVAR int OldPointCX; // 以前のマウスのX座標(X_vzs処理用)

GVAR HWND hViewReqWnd GPARAM(NULL);	// 最小化時の戻り先
GVAR HWND hLastViewReqWnd GPARAM(NULL);
GVAR const TCHAR *FirstCommand GPARAM(NULL);
GVAR int Mpos GPARAM(-1);	// Hidden menu
GVAR MOUSESTATE MouseStat;

#define MaxBookmark 4
#define BookmarkID_undo		0
#define BookmarkID_1st		1
#define OldTailLine Bookmark[MaxBookmark + 1].pos.y // 前回の読み込み位置
#define NewReadTopLine Bookmark[MaxBookmark + 1].pos.x // 追加読込開始位置
// 0 : undo用／1 : D/G用／2～MaxBookmark : 一般用
GVAR BookmarkInfo Bookmark[MaxBookmark + 1 + 1];

// 遅延処理関連
GVAR BOOL BackReader GPARAM(FALSE);
GVAR HANDLE hReadStream GPARAM(NULL);

#ifdef GLOBALEXTERN
typedef enum {
	READ_NONE = 0, READ_FILE, READ_STDIN
} READSMODE;
#endif

GVAR READSMODE ReadingStream GPARAM(READ_NONE);

GVAR VO_MAKETEXTINFO mtinfo
#ifndef GLOBALEXTERN
= { NULL, NULL, 0, -1, 0, 0 }
#endif
;

GVAR DWORD WindowFixTick;		// ウィンドウサイズの大きさ調整タイミング用
				// ※ TRUE なら、ヒストリに設定を保存しない、拡張子補完しない

GVAR LOGFONT DefaultPPvFont GPARAM(Default_F_mes);
GVAR const TCHAR DefaultPPvFont_JA[] GPARAM(Default_F_mes_JA);

#ifdef USEDIRECTX
GVAR DXDRAWSTRUCT *DxDraw GPARAM(NULL);
#endif
//================================================================ Customize 系
GVAR PPVVAR XV
#ifndef GLOBALEXTERN
= {	{0, NULL},	// HiddenMenu
	{{IMGD_AUTOWINDOWSIZE, HALFTONE},	// imgD
	 IMGD_MM_FULLSCALE,	// MagMode
	 0, //AspectRate
	 0, 0 // AspectW, AspectH
	}
}
#endif
;
GVAR int XV_tmod GPARAM(0);
GVAR int X_vfs GPARAM(1);
GVAR int X_evoc GPARAM(0);
GVAR DWORD X_wsiz GPARAM(IMAGESIZELIMIT);
GVAR HILIGHTKEYWORD *X_hkey GPARAM(NULL);

GVAR int X_tlen GPARAM(400);
DLLGVAR DWORD X_dss DLLGPARAM(DSS_NOLOAD);	// 画面自動スケーリング
// 全体:画面 ------------------------------------------------------------------
GVAR DWORD X_win GPARAM(XWIN_MENUBAR);
GVAR int X_scrm GPARAM(2);
GVAR int X_tray GPARAM(0);
GVAR int X_IME GPARAM(0);

GVAR BOOL XV_pctl GPARAM(TRUE);
GVAR int XV_bctl[3]
#ifndef GLOBALEXTERN
= {3, 3, 0};	// tab, lf, space
#endif
;
GVAR int X_fles GPARAM(0);
GVAR int XV_numt GPARAM(1);
GVAR int XV_lnum GPARAM(0);
#define DEFNUMBERSPACE	6	// 行番号に使う幅(区切り1文字分付き)

GVAR int XV_lleft;				// 行番号表示に必要な幅
GVAR int XV_left GPARAM(2);
GVAR int X_textmag GPARAM(100);
GVAR int X_swmt GPARAM(0);

GVAR TCHAR X_pppv GPARAM(0);


#define CTRLSIG_CRLF	0
#define CTRLSIG_LF		1
#define CTRLSIG_CR		2
#define CTRLSIG_HTAB	3
#define CTRLSIG_VTAB	4
#define CTRLSIG_PAGE	5
GVAR WCHAR XV_ctls[6] // VCODE_RETURN で使用する文字
#ifndef GLOBALEXTERN
= {
	0x21b5, // L  ^M CR&LF Carriage Return
	0xffec, // ↓ ^J LF Line Feed
	0x2190, // ← ^M CR Carriage Return
	0x2192, // → ^I HT Horizontal Tabulation
	0x21a7, // T  ^K VT Vertical Tabulation
	0x2190,	// ← ^L FF Form Feed
}
#endif
;
// 全体:操作 ------------------------------------------------------------------
GVAR int X_alt GPARAM(1);
GVAR int X_iacc GPARAM(0);

GVAR int X_dds GPARAM(1);
GVAR DWORD X_askp GPARAM(0);
GVAR int XV_drag[4]
#ifndef GLOBALEXTERN
= {MOUSEBUTTON_L, MOUSEBUTTON_R, MOUSEBUTTON_M, 0}
#endif
;
#define XV_DragScr XV_drag[0]
#define XV_DragMov XV_drag[2]
#define XV_DragSel XV_drag[1]
#define XV_DragGes XV_drag[3]
GVAR BOOL X_vzs GPARAM(TRUE);
// 色 -------------------------------------------------------------------------
GVAR COLORREF C_back	GPARAM(C_BLACK);	// 背景色
GVAR COLORREF C_line	GPARAM(C_CYAN);		// 区切り線
GVAR COLORREF C_mes		GPARAM(C_YELLOW);
GVAR COLORREF C_info	GPARAM(C_DWHITE);
GVAR COLORREF CV_boun	GPARAM(C_DBLACK);	// 境界線
GVAR COLORREF CV_lcsr	GPARAM(C_DWHITE);
GVAR COLORREF CV_ctrl	GPARAM(C_DGREEN);
GVAR COLORREF CV_lf		GPARAM(C_YELLOW);
GVAR COLORREF CV_tab	GPARAM(C_DBLUE);
GVAR COLORREF CV_spc	GPARAM(C_DBLUE);
GVAR COLORREF CV_link	GPARAM(C_BLUE);
GVAR COLORREF CV_syn[2]
#ifndef GLOBALEXTERN
= {C_DGREEN, C_SBLUE};
#endif
;
GVAR COLORREF	C_res[2]
#ifndef GLOBALEXTERN
= {C_AUTO, C_AUTO};
#endif
;
GVAR COLORREF	CV_lnum[2]
#ifndef GLOBALEXTERN
= {C_CYAN, C_GRAY};
#endif
;

GVAR COLORREF	CV_lbak[3]
#ifndef GLOBALEXTERN
= {C_RED, C_CYAN, C_AUTO};
#endif
;

GVAR COLORREF	CV_char[17]
#ifndef GLOBALEXTERN
= {	C_DBLACK,	C_RED,		C_GREEN,	C_BLUE,
	C_YELLOW,	C_CYAN,		C_MAGENTA,	C_WHITE,
	C_BLACK,	C_DRED,		C_DGREEN,	C_DBLUE,
	C_DYELLOW,	C_DCYAN,	C_DMAGENTA,	C_DWHITE,
	C_BLUE }
#endif
;

GVAR COLORREF	CV_hili[9]
#ifndef GLOBALEXTERN
= {	C_BLUE,
	C_YELLOW,	C_GREEN,	C_CYAN,		C_SBLUE,
	C_MAGENTA,	C_RED,		C_MGREEN,	C_DMAGENTA}
#endif
;
// 内部用 ---------------------------------------------------------------------
GVAR WINPOS WinPos
#ifndef GLOBALEXTERN
= {{CW_USEDEFAULT, CW_USEDEFAULT, 640, 400}, 0, 0}			// 表示位置
#endif
;
GVAR int OpenEntryNow GPARAM(0);	// Open 再入防止
DLLGVAR int TouchMode DLLGPARAM(0);
DLLGVAR int X_pmc[4] DLLGPARAM({X_pmc_defvalue});

// 印刷 -----------------------------------------------------------------------
GVAR PRINTINFO PrintInfo
#ifndef GLOBALEXTERN
= {{{20, 15, 10, 10}, 0}, 1, 1, 80, 50}
#endif
;
