/*-----------------------------------------------------------------------------
	Paper Plane vUI													～ Init ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

void InitPPvWindow(void);
int MultiBootMode = PPXREGIST_NORMAL;

const TCHAR *OptionNames[] = {
									// 全般
	T("R"),				// 窓の再利用
	T("PRINT"),			// 印刷処理
	T("PRINTS"),		// 一括印刷処理
									// 表示モード
	T("HEX"),			// 16進表示
	T("TEXT"),			// テキスト表示
	T("DOCUMENT"),		// 文書表示
	T("IMAGE"),			// 画像表示
	T("RAWIMAGE"),		// ベタ画像表示
									// テキスト表示形式
	T("IBM"),			// IBM PC font
	T("ANSI"),			// ANSI code set
//10
	T("JIS"),			// JIS
	T("SYSTEM"),		// system codepage
	T("EUC"),			// EUC
	T("UNICODE"),		// UNICODE
	T("UNICODEB"),		// UNICODE big endian
	T("SJISNEC"),		// S-JIS NEC PC-9801
	T("SJISB"),			// S-JIS big endian
	T("KANA"),			// KANA
	T("UTF8"),			// UTF-8
	T("UTF7"),			// UTF-7
//20
	T("RTF"),			// Rich text
	T("SISO"),
	T("ESC"),
	T("TAG"),
	T("BOOTID"),
	T("BOOTMAX"),
	T("CONVERT"),
	T("CONVERTSJIS"),
	T("CONVERTEUC"),
	T("FOCUS"),
//30
	T("K"),
	T("MIN"),
	T("MAX"),
	T("NOACTIVE"),
	T("WIDTH"),
	T("LINESPACE"),
	T("TAB"),
	T("ANIMATE"),
	T("CHECKEREDPATTERN"),
	T("CSS"),
//40
	T("SCRIPT"),
	T("TAIL"),
	T("CODEPAGE"),
	T("SJIS"),			// Shift_JIS
	T("US"),
	T("LATIN1"),
	T("UTF16"),
	T("UTF16BE"),
	T("PARENT"),
//	T("FILECASE"),
	NULL
};

const TCHAR *TagOffOn[] = { T("OFF"),T("ON"),T("TAG"),NULL };
const TCHAR *OffOn[] = { T("OFF"),T("ON"),NULL };
const TCHAR *AutoOffOn[] = { T("AUTO"),T("OFF"),T("ON"),NULL };
const TCHAR *NameFirst = NULL,*NameLast = NULL;
const TCHAR DefCID[] = T("VA");

PPXINMENU barFile[] = {
	{K_c |      'O',T("&Open...\tCtrl+O")},
	{K_c |      'S',T("&Save...\tCtrl+S")},
	{'X',T("E&xecute\tX")},
	{'P'			,T("P&lay\tP")},
	{K_c |      'P',T("&Print\tCtrl+P")},
	{K_s | 'E'		,T("PP&e\tShift+E")},
	{PPXINMENY_SEPARATE,NULL},
	{K_c | 'U'		,T("print set &Up...")},
	{PPXINMENY_SEPARATE,NULL},
	{'Q',T("E&xit\tAlt+F4")},
	{0,NULL}
};
// Edit メニュー
PPXINMENU barEdit[] = {
	{K_c | 'C'		,T("&Copy\tCtrl+C")},
	{K_c | 'V'		,T("&Paste\tCtrl+V")},
	{K_c | K_s | 'V',T("Pa&ste type...\tCtrl+Shift+V")},
	{PPXINMENY_SEPARATE,NULL},
	{'F',T("Find &F...\tF")},
	{'B',T("Find &B...\tB")},
	{PPXINMENY_SEPARATE,NULL},
	{']',T("Find Next\t&]")},
	{'[',T("Find Prev\t&[")},
	{PPXINMENY_SEPARATE,NULL},
	{'J'		,T("&Jump to line\tJ")},
	{K_c | 'D'		,T("&Set bookmark\tCtrl+D")},
	{K_c | 'G'		,T("&Goto bookmark\tCtrl+G")},
	{0,NULL}
};
// View メニュー
PPXINMENU barView[] = {
	{K_layout,T("&Layout")},
	{'R',T("R&everse back\tR")},
//	{0x3e81,T("Task display")},		//うまくいかない→PPcの同じ場所にメモあり
	{K_c | K_Pup,T("Prev page\tCtrl+PageUp")},
	{K_c | K_Pdw,T("Next page\tCtrl+PageDown")},
	{'<'		,T("Prev Div page\t&<")},
	{'>'		,T("Next Div page\t&>")},
	{K_c | K_s | K_Pup,T("Top Div pa&ge")},
	{K_c | K_s | K_Pdw,T("Last Div pa&ge")},
	{'K'		,T("Rotate left\tK")},
	{'L'		,T("Rotate right\tL")},
	{PPXINMENY_SEPARATE,NULL},
	{':'		,T("&View mode...\t:")},
	{'@'		,T("&Text code...\t@")},
	{'W'		,T("Font &width\tW")},
	{'T'		,T("Line number mode\tT")},
	{'U'		,T("Line number\tU")},
	{'C'		,T("&Control code\tC")},
	{';'		,T("C&olumns\t;")},
	{K_s | 'F'	,T("string &Highlight\tCtrl+Shift+F")},
	{PPXINMENY_SEPARATE,NULL},
	{K_c | 'L',T("Re&draw\tCtrl+L")},
	{'.',T("&Reload\tF5")},
	{0,NULL}
};
// Tool メニュー
PPXINMENU barTool[] = {
	{KC_Tvfs	,T("&VFS switch")},
	{K_cust,T("&Customizer")},
	{0,NULL}
};
// Help メニュー
PPXINMENU barHelp[] = {
	{K_F1     ,T("&Help\tF1")},
	{K_s | K_F1	,T("&Topic\tShift+F1")},
	{PPXINMENY_SEPARATE,NULL},
	{K_supot   ,T("&Support")},
	{(DWORD_PTR)T("*checkupdate"),T("Check &Update")},
	{PPXINMENY_SEPARATE,NULL},
	{K_about   ,T("&About")},
	{0,NULL}//T("&File")},
};

PPXINMENUBAR ppvbar[] = {
	{T("&File"),barFile},
	{T("&Edit"),barEdit},
	{T("&View"),barView},
	{T("&Tool"),barTool},
	{T("&Help"),barHelp},
	{NULL,NULL}
};

int CheckSubParam(TCHAR *param,const TCHAR *list[],int defvalue)
{
	int i = 0;

	Strupr(param);
	for ( ;; ){
		if ( *list == NULL ){
			i = defvalue;
			break;
		}
		if ( tstrcmp( param,*list) == 0 ) break;
		i++;
		list++;
	}
	return i;
}

void InitViewOptions(VIEWOPTIONS *viewopts)
{
#if 1
	memset(viewopts,0xff,sizeof(VIEWOPTIONS));
#else
	viewopts->dtype = -1;

	viewopts->T_code = -1;
	viewopts->T_siso = -1;
	viewopts->T_esc = -1;
	viewopts->T_tag = -1;
	viewopts->T_show_css = -1;
	viewopts->T_show_script = -1;
	viewopts->T_tab = -1;
	viewopts->T_width = -1;

	viewopts->I_animate = -1;
	viewopts->I_CheckeredPattern = -1;

	viewopts->linespace = -1;

	viewopts->tailflags = -1;
#endif
}

BOOL CheckParam(VIEWOPTIONS *viewopts,const TCHAR *param,TCHAR *filename)
{
	BOOL reuse = FALSE;
	TCHAR buf[CMDLINESIZE];

	InitViewOptions(viewopts);
	if ( filename != NULL ) filename[0] = '\0';
	for ( ;; ){
		TCHAR *more; // オプションのパラメータ部分
		int id;
		UTCHAR code;
		const TCHAR *oldp;

		oldp = param;
		code = GetOptionParameter(&param,buf,&more);
		if ( code == '\0' ) break;
		if ( code != '-' ){ // 開くファイル名
			if ( filename != NULL ){
				NameFirst = oldp;
				NameLast = param;
				VFSFixPath(filename,buf,NULL,VFSFIX_PATH | VFSFIX_VREALPATH);
			}
			continue;
		}
			// "-Pn" : 組み込み時の親ウィンドウ指定 / スクリーンセーバー指定
		if ( (*(buf + 1) == 'P') && Isdigit(*(buf + 2)) ){
			TCHAR *ptr;

			ptr = buf + 2;
			hViewParentWnd = (HWND)GetNumber((const TCHAR **)&ptr);
			continue;
		}
		{								// ラベルを調べる
			const TCHAR **label;

			label = OptionNames;
			id = 0;
			while ( *label ){
				if ( !tstrcmp( buf + 1, *label ) ) break;
				label++;
				id++;
			}
		}
		switch ( id ){
			case 0:				// R
				reuse = TRUE;
				break;
			case 1:				//	"PRINT",			// 印刷処理
			case 2:				//	"PRINTS",			// 一括印刷処理
				VO_PrintMode = (id - 1) + PRINTMODE_ONE;
				break;
								// 表示モード
			case 3:				//	"HEX",
			case 4:				//	"TEXT",
			case 5:				//	"DOCUMENT",
			case 6:				//	"IMAGE",
			case 7:				//	"RAWIMAGE",
				viewopts->dtype = id - 3 + DISPT_HEX;
				break;
							// テキスト表示形式
			case 8:				//	"IBM"
			case 9:				//	"ANSI"
			case 10:			//	"JIS"
			case 11:			//	"SYSTEM"
			case 12:			//	"EUC"
			case 13:			//	"UNICODE"
			case 14:			//	"UNICODEB"
			case 15:			//	"SJISNEC"
			case 16:			//	"SJISB"
			case 17:			//	"KANA"
			case 18:			//	"UTF8"
			case 19:			//	"UTF7"
			case 20:			//	"RTF"
				viewopts->T_code = id - 8;
				break;
			case 21:			//	"SISO:AUTO","SISO:OFF","SISO:ON",
				viewopts->T_siso = CheckSubParam(more,AutoOffOn,2);
				break;
			case 22:			//	"ESC:OFF","ESC:ON",
				viewopts->T_esc = CheckSubParam(more,OffOn,1);
				break;
			case 23:			//	"TAG:OFF","TAG:ON","TAG:TAG"
				viewopts->T_tag = CheckSubParam(more,TagOffOn,1);
				break;
			case 24:			//	"BOOTID:A-Z"
				if ( Isalpha(*more) ){
					MultiBootMode = PPXREGIST_IDASSIGN;
					if ( Isalpha(*(more + 1)) ){
						RegID[1] = upper(*more);
						RegID[2] = upper(*(more + 1));
					}else{
						RegID[2] = upper(*more);
					}
				}
				break;
			case 25:			//	"BOOTMAX:A-Z"
				if ( Isalpha(*more) ){
					MultiBootMode = PPXREGIST_MAX;
					RegID[2] = upper(*more);
				}
				break;
			case 26:			//	"CONVERT:filename"
			case 27:			//	"CONVERTSJIS:filename"
				convert = 1;
				more = *more ? more : T("con");
				convertname[0] = '\0';
				GetLineParam((const TCHAR **)&more,convertname);
				break;
			case 28:			//	"CONVERTEUC:filename"
				convert = 2;
				more = *more ? more : T("con");
				convertname[0] = '\0';
				GetLineParam((const TCHAR **)&more,convertname);
				break;
			case 29:			//	"focus:hwnd"
				hLastViewReqWnd = hViewReqWnd = (HWND)GetNumber((const TCHAR **)&more);
				break;
			case 30:			//	k
				FirstCommand = param;
				param = NilStr;
				break;
			case 31:			//	min
				WinPos.show = SW_SHOWMINNOACTIVE;
				break;
			case 32:			//	max
				WinPos.show = SW_SHOWMAXIMIZED;
				break;
			case 33:			//	noactive
				WinPos.show = SW_SHOWNOACTIVATE;
				break;
			case 34:			//	width
				viewopts->T_width = GetNumber((const TCHAR **)&more) + VIEWOPTIONS_WIDTHOFFSET;
				break;
			case 35:			//	linespace
				viewopts->linespace = GetNumber((const TCHAR **)&more);
				break;
			case 36:			//	tab
				viewopts->T_tab = GetNumber((const TCHAR **)&more);
				break;
			case 37:			//	"animate:OFF","animate:ON",
				viewopts->I_animate = CheckSubParam(more,OffOn,1);
				break;
			case 38:			//	"CheckeredPattern:OFF","CheckeredPattern:ON"
				viewopts->I_CheckeredPattern = CheckSubParam(more,OffOn,1);
				break;
			case 39:			//	css
				viewopts->T_show_css = CheckSubParam(more,OffOn,1);
				break;
			case 40:			//	script
				viewopts->T_show_script = CheckSubParam(more,OffOn,1);
				break;
			case 41:			//	tail
				viewopts->tailflags = CheckSubParam(more,OffOn,1);
				break;

			case 42:			//	codepage
				viewopts->T_code = GetNumber((const TCHAR **)&more);
				if ( viewopts->T_code < VTYPE_MAX ){
					viewopts->T_code = VTYPE_SYSTEMCP;
				}
				break;
			case 43:			//	"SJIS"
				viewopts->T_code =
						( VO_textS[VTYPE_SYSTEMCP] == textcp_sjis ) ?
						VTYPE_SYSTEMCP : CP__SJIS;
				break;
			case 44:			//	"US"
				viewopts->T_code = VTYPE_IBM;
				break;
			case 45:			//	"LATIN1"
				viewopts->T_code = VTYPE_ANSI;
				break;
			case 46:			//	"UTF16"
				viewopts->T_code = VTYPE_UNICODE;
				break;
			case 47:			//	"UTF16BE"
				viewopts->T_code = VTYPE_UNICODEB;
				break;
			case 48:			//	"PARENT"
				hViewParentWnd = (HWND)GetNumber((const TCHAR **)&more);
				break;
//			case 49:			//	filecase
//				FileCase = CheckSubParam(more,OffOn,1);
//				break;
			default:
				XMessage(NULL,NULL,XM_GrERRld,MES_EUOP,buf);
		}
	}
	return reuse;
}

void InitGui(void)
{
	UINT ID = 0x1000;

	if ( hToolBarWnd != NULL ){
		CloseToolBar(hToolBarWnd);
		ThFree(&thGuiWork);
		hToolBarWnd = NULL;
	}
	if ( X_win & XWIN_TOOLBAR ){
		hToolBarWnd = CreateToolBar(&thGuiWork, vinfo.info. hWnd, &ID, T("B_vdef"), PPvPath, 0);
	}
}

BOOL InitializePPv(int *result)
{
	BOOL reuse;
	TCHAR name[VFPS];
	TCHAR cmd[CMDLINESIZE];
	const TCHAR *cmdptr;
	const TCHAR *lpszCmdLine;
	int viewflag;

	PPxCommonExtCommand(K_CHECKUPDATE, 0);
	FixCharlengthTable(T_CHRTYPE);
	if ( GetACP() != CP__SJIS ) VO_textS[VTYPE_SYSTEMCP] = textcp_systemcp;

	GetModuleFileName(hInst, PPvPath, MAX_PATH);
	*VFSFindLastEntry(PPvPath) = '\0';
										// オプション解析 ---------------------
	lpszCmdLine = GetCommandLine();
	GetLineParam(&lpszCmdLine, name);
	reuse = CheckParam(&viewopt_def, lpszCmdLine, name);
										// -R 処理 ----------------------------
	if ( reuse ){
		viewflag = ((WinPos.show == SW_SHOWNOACTIVATE) ||
				(WinPos.show == SW_SHOWMINNOACTIVE)) ?
				PPV_DISABLEBOOT | PPV_PARAM | PPV_NOFOREGROUND :
				PPV_DISABLEBOOT | PPV_PARAM;

		if ( NameFirst == NULL ){ // ファイル名指定なし
			cmdptr = lpszCmdLine;
		}else{ // ファイル名指定有り→正規化したファイル名を挿入する
			TCHAR *p;
			size_t size;

			size = NameFirst - lpszCmdLine;
			memcpy(cmd, lpszCmdLine, size * sizeof(TCHAR));
			p = cmd + size;
			*p++ = '\"';
			tstrcpy(p, name);
			p += tstrlen(p);
			*p++ = '\"';
			*p++ = ' ';
			tstrcpy(p, NameLast);
			cmdptr = cmd;
		}
		if ( MultiBootMode == PPXREGIST_NORMAL ){
			*result = 1;
			if ( PPxView(NULL, cmdptr, viewflag) ){
				*result = 0;
				return FALSE;
			}
		}
	}
	if ( PPxRegist(PPXREGIST_DUMMYHWND, RegID, MultiBootMode) < 0 ){
		if ( hViewParentWnd == NULL ){
			*result = 1;
			if ( reuse ){
				if ( MultiBootMode == PPXREGIST_IDASSIGN ){
					setflag(viewflag,PPV_BOOTID | ((int)RegID[2] << 24));
				}
				if ( PPxView(NULL,cmd,viewflag) ) *result = 0;
			}else{
				SetForegroundWindow(PPxGetHWND(RegID));
				*result = 0;
			}
			return FALSE;
		}
	}

	InitPPvWindow();
										// コマンドライン指定ファイル読み込み
	if ( name[0] != '\0' ){
		WmWindowPosChanged(vinfo.info.hWnd); // まだ、情報がないことがある
		OpenViewObject(name,NULL,&viewopt_def,0);
		if ( vo_.DModeBit == DOCMODE_NONE ){
			*result = 0;
			if ( convert ) return FALSE;
		}else{
			FollowOpenView(&vinfo);
			if ( VO_PrintMode == PRINTMODE_ONE ){
				PostMessage(vinfo.info.hWnd,WM_PPXCOMMAND,K_c | 'P',0);
			}
			if ( hViewReqWnd != NULL ){
				PostMessage(vinfo.info.hWnd,WM_PPXCOMMAND,KV_FOCUS,0);
			}
		}
	}else{
		DWORD type;

		type = GetFileType(GetStdHandle(STD_INPUT_HANDLE));
		if ( (type == FILE_TYPE_DISK) || (type == FILE_TYPE_PIPE) ){
			OpenViewObject(T("stdin"),NULL,&viewopt_def,PPV__stdin);
			if ( vo_.DModeBit == DOCMODE_NONE ){
				*result = 0;
				return FALSE;
			}
			FollowOpenView(&vinfo);
		}
	}
	if ( convert ){
		ConvertMain();
		PostMessage(vinfo.info.hWnd,WM_CLOSE,0,0);
	}else{								// メインウインドウの表示を更新 -------
		int show = WinPos.show; // SetMenu で WinPos.show 破損
		SetMenu(vinfo.info.hWnd,
				(X_win & XWIN_MENUBAR) ? DynamicMenu.hMenuBarMenu : NULL);
		ShowWindow(vinfo.info.hWnd,show);
		UpdateWindow(vinfo.info.hWnd);
		if ( X_IME == 1 ) PostMessage(vinfo.info.hWnd,WM_PPXCOMMAND,K_IMEOFF,0);
//		SetIMEDefaultStatus(vinfo.info.hWnd);
		DragAcceptFiles(vinfo.info.hWnd,TRUE);

		SetCurrentDirectory(PPvPath);
	}
	return TRUE;
}

HFONT InitOtherFont(int mode)
{
	LOGFONT cursfont = {
/*Width,Height*/					0,0, \
/*Escapement,Orientation,Weight*/	0,0,FW_NORMAL, \
/*Italic,Underline,StrikeOut*/		FALSE,FALSE,FALSE, \
/*CharSet*/							SHIFTJIS_CHARSET, \
/*OutPrecision*/					OUT_DEFAULT_PRECIS, \
/*ClipPrecision*/					CLIP_DEFAULT_PRECIS, \
/*Quality*/							DEFAULT_QUALITY, \
/*PitchAndFamily*/					FIXED_PITCH | FF_MODERN,\
/*FaceName*/						T("Courier New")
	};

	cursfont.lfHeight	= fontY;
	cursfont.lfWidth	= fontX;
	switch (mode){
		case 0: // hANSIFont ANSIフォント作成
			cursfont.lfCharSet	= ANSI_CHARSET;
			break;

		case 1: // hIBMFont IBMフォント作成
			cursfont.lfCharSet	= OEM_CHARSET;
			break;

		default: // hSYMFont 記号フォント作成
			cursfont.lfWidth--; // fontX - 1;
			cursfont.lfCharSet	= SYMBOL_CHARSET;
			tstrcpy(cursfont.lfFaceName,T("Symbol"));
	}
	return CreateFontIndirect(&cursfont);
}

void InitSymbolFont(HDC hDC)
{
	TEXTMETRIC tm;
	HGDIOBJ hOldFont;

	if ( hSYMFont != INVALID_HANDLE_VALUE ) return;
	hSYMFont = InitOtherFont(2);
	hOldFont = SelectObject(hDC,hSYMFont);
	GetTextMetrics(hDC,&tm);
	fontSYMY = tm.tmHeight;
	SelectObject(hDC,hOldFont);
}

HFONT GetANSIFont(void)
{
	if ( hANSIFont != INVALID_HANDLE_VALUE ) return hANSIFont;
	return (hANSIFont = InitOtherFont(0));
}
HFONT GetIBMFont(void)
{
	if ( hIBMFont != INVALID_HANDLE_VALUE ) return hIBMFont;
	return (hIBMFont = InitOtherFont(1));
}

void MakeFonts(HDC hDC,int mag)
{
	HGDIOBJ hOldFont;	//一時保存用
	TEXTMETRIC tm;
	LCID lcid;
	LOGFONTWITHDPI cursfont;
										// 汎用フォント作成
	GetPPxFont(PPXFONT_F_fix,FontDPI,&cursfont);
	if ( mag ){
		cursfont.font.lfHeight = (cursfont.font.lfHeight * mag) / 100;
		cursfont.font.lfWidth  = (cursfont.font.lfWidth  * mag) / 100;
	}
	hBoxFont = CreateFontIndirect(&cursfont.font);

										// 寸法/解像度情報を入手
	hOldFont = SelectObject(hDC,hBoxFont);
	GetAndFixTextMetrics(hDC,&tm);
	fontX = tm.tmAveCharWidth;

	GetCustData(T("X_lspc"),&X_lspc,sizeof(X_lspc));
	fontY = tm.tmHeight;
	LineY = fontY + X_lspc;

	lcid = LOWORD(GetUserDefaultLCID());
	if ( lcid == LCID_JAPANESE ){ // 全角が半角の２倍幅になっているかチェック
		SIZE ssize,dsize;		// ※WindowsXP より前のVerで起きる場合がある

		GetTextExtentPoint32(hDC,T("0"),1,&ssize);
		GetTextExtentPoint32(hDC,T("あ"),2 / sizeof(TCHAR),&dsize);
		fontWW = ssize.cx * 2 - dsize.cx;
	}else{
		fontWW = 0;
	}
	SelectObject(hDC,hOldFont);

	#if DRAWMODE != DRAWMODE_GDI
	{
		int w = SetFontDxDraw(DxDraw,hBoxFont,0);
		if ( w > fontX ) fontX = w;
	}
	#endif

	if ( XV_unff != FALSE ) MakeUnfixedFont();
}

void DeleteFonts(void)
{
	DeleteObject(hBoxFont);
	if ( hANSIFont != INVALID_HANDLE_VALUE ){
		DeleteObject(hANSIFont);
		hANSIFont = INVALID_HANDLE_VALUE;
	}
	if ( hIBMFont != INVALID_HANDLE_VALUE ){
		DeleteObject(hIBMFont);
		hIBMFont = INVALID_HANDLE_VALUE;
	}
	if ( hSYMFont != INVALID_HANDLE_VALUE ){
		DeleteObject(hSYMFont);
		hSYMFont = INVALID_HANDLE_VALUE;
	}
	if ( hUnfixedFont != NULL ){
		DeleteObject(hUnfixedFont);
		hUnfixedFont = NULL;
	}
}

void MakeUnfixedFont(void)
{
	LOGFONTWITHDPI cursfont;

	GetPPxFont(PPXFONT_F_unfix,0,&cursfont);

	cursfont.font.lfHeight = fontY;
	cursfont.font.lfWidth = 0;
	hUnfixedFont = CreateFontIndirect(&cursfont.font);
	SetFontDxDraw(DxDraw,hUnfixedFont,1);
}

int GetCustTableCID(const TCHAR *str,const TCHAR *sub,void *bin,size_t b_size)
{
	if ( NO_ERROR == GetCustTable(str,sub,bin,b_size) ) return NO_ERROR;
	return GetCustTable(str,(sub[1] == '\0') ? DefCID + 1 : DefCID,bin,b_size);
}

// カスタマイズ内容を取得 -----------------------------------------------------
void PPvLoadCust(void)
{
	GetCustTable(T("X_win"),T("V"),&X_win,sizeof(X_win));
	GetCustData (T("X_alt")		,&X_alt,sizeof(X_alt));
	GetCustData (T("X_iacc")	,&X_iacc,sizeof(X_iacc));
	GetCustData (T("X_evoc")	,&X_evoc,sizeof(X_evoc));
	GetCustData (T("X_scrm")	,&X_scrm,sizeof(X_scrm));
	GetCustData (T("X_IME")		,&X_IME,sizeof(X_IME));
	GetCustData (T("X_swmt")	,&X_swmt,sizeof(X_swmt));
	AutoColor	(T("C_back")	,&C_back,COLOR_WINDOW);
	GetCustData (T("C_line")	,&C_line,sizeof(C_line));
	AutoColor	(T("C_mes")		,&C_mes,COLOR_WINDOWTEXT);
	AutoColor	(T("C_info")	,&C_info,COLOR_WINDOWTEXT);
	GetCustData (T("CV_boun")	,&CV_boun,sizeof(CV_boun));
	GetCustData (T("CV_ctrl")	,&CV_ctrl,sizeof(CV_ctrl));
	GetCustData (T("CV_lf")		,&CV_lf,sizeof(CV_lf));
	GetCustData (T("CV_tab")	,&CV_tab,sizeof(CV_tab));
	GetCustData (T("CV_spc")	,&CV_spc,sizeof(CV_spc));
	GetCustData (T("CV_link")	,&CV_link,sizeof(CV_link));
	GetCustData (T("CV_syn")	,&CV_syn,sizeof(CV_syn));
	GetCustData (T("CV_lbak")	,&CV_lbak,sizeof(CV_lbak));
	CV_char[CV__olddefback] = C_BLACK;
	CV_char[CV__olddeftext] = C_DWHITE;
	CV_char[CV__olddeftext + 1] = 0xe0e0e0e0; // = CV__defback
	GetCustData (T("CV_char"),CV_char,sizeof(CV_char));
	if ( CV_char[CV__olddeftext + 1] == 0xe0e0e0e0 ){
		CV_char[CV__defback] = CV_char[CV__olddefback];
		CV_char[CV__deftext] = CV_char[CV__olddeftext];
		CV_char[CV__olddefback] = C_DBLACK;
		CV_char[CV__olddeftext] = C_WHITE;
//		CV_char[CV__olddeftext + 1] = C_DBLACK;
		SetCustData (T("CV_char"),CV_char,sizeof(COLORREF) * 16);
	}
	GetCustData (T("CV_lnum")	,CV_lnum,sizeof(CV_lnum));
	GetCustData (T("CV_hili")	,&CV_hili,sizeof(CV_hili));
	CV_char[CV__highlight] = CV_hili[0];
	GetCustData (T("CV_lcsr")	,&CV_lcsr,sizeof(CV_lcsr));

	if ( hViewParentWnd == NULL ){
		XV.img.MagMode = IMGD_MM_FULLSCALE;
		GetCustTableCID(T("XV_imgD"),RegCID,&XV.img.imgD,sizeof(XV.img.imgD));
	}
	GetCustData (T("XV_dds")	,&X_dds,sizeof(X_dds));
	GetCustData (T("X_awhel")	,&X_awhel,sizeof(X_awhel));
	GetCustData (T("XV_bctl")	,&XV_bctl,sizeof(XV_bctl));
	if ( OSver.dwMajorVersion < 6 ){
		if ( XV_bctl[0] > 2 ) XV_bctl[0] = 1;
		if ( XV_bctl[1] > 2 ) XV_bctl[1] = 1;
	}
	GetCustData (T("XV_ctls")	,&XV_ctls,sizeof(XV_ctls));

	GetCustData (T("XV_pctl")	,&XV_pctl,sizeof(XV_pctl));
	GetCustData (T("XV_lnum")	,&XV_lnum,sizeof(XV_lnum));
	GetCustData (T("XV_numt")	,&XV_numt,sizeof(XV_numt));
	GetCustData (T("C_res")		,&C_res,sizeof(C_res));
	GetCustData (T("X_tray")	,&X_tray,sizeof(X_tray));
	GetCustData (T("X_fles")	,&X_fles,sizeof(X_fles));
	GetCustData (T("XV_tmod")	,&XV_tmod,sizeof(XV_tmod));
	GetCustData (T("X_vzs")		,&X_vzs,sizeof(X_vzs));
	GetCustData (T("XV_drag")	,&XV_drag,sizeof(XV_drag));
	GetCustData (T("XV_left")	,&XV_left,sizeof(XV_left));
	GetCustData (T("XV_unff")	,&XV_unff,sizeof(XV_unff));
	GetCustData(T("X_askp"),&X_askp,sizeof(X_askp));
	if ( C_res[0] == C_AUTO) C_res[0] = GetSysColor(COLOR_HIGHLIGHTTEXT);
	if ( C_res[1] == C_AUTO) C_res[1] = GetSysColor(COLOR_HIGHLIGHT);
	if ( CV_char[CV__defback] == C_AUTO ) CV_char[CV__defback] = C_back;
	if ( CV_char[CV__deftext] == C_AUTO ){
		CV_char[CV__deftext] = GetSysColor(COLOR_WINDOWTEXT);
	}
	if ( CV_ctrl == C_AUTO ) CV_ctrl = CV_char[CV__deftext];
	if ( X_win & XWIN_HIDETASK ) X_tray = 1;

	if ( OSver.dwMajorVersion >= 6 ){
		GetCustData(T("X_dss"),&X_dss,sizeof(X_dss));
	}
	GetCustTable(T("_others"),T("pppv"),&X_pppv,sizeof(X_pppv));

	StrLoading = MessageText(MES_FLOD);
	StrLoadingLength = tstrlen32(StrLoading);

	LoadHiddenMenu(&XV.HiddenMenu,T("HM_ppv"),PPvHeap,C_mes);

	UseActiveEvent = IsExistCustTable(T("KV_main"),T("ACTIVEEVENT")) ||
		IsExistCustTable(T("KV_img"),T("ACTIVEEVENT")) ||
		IsExistCustTable(T("KV_crt"),T("ACTIVEEVENT")) ||
		IsExistCustTable(T("KV_page"),T("ACTIVEEVENT"));

	GetCustData(T("X_pmc"),&X_pmc,sizeof(X_pmc));
	if ( X_pmc[0] > 0 ) PPvEnterTabletMode(vinfo.info.hWnd);

#ifdef USEDIRECTX
	X_scrm = 0;
#endif
}
// カスタマイズ内容を保存 -----------------------------------------------------
void PPvSaveCust(void)
{
	SetCustTable(T("_WinPos"),RegCID,&WinPos,sizeof(WinPos));
}
/*=============================================================================
	Close - すべてのインスタンスが行う終了処理
=============================================================================*/
const EXECKEYCOMMANDSTRUCT PPvExecKeyMain =
	{(EXECKEYCOMMANDFUNCTION)PPvCommand,T("KV_main"),NULL};

void ClosePPv(void)
{
	KillTimer(vinfo.info.hWnd,TIMERID_DRAGSCROLL);
	ExecKeyCommand(&PPvExecKeyMain,&vinfo.info,K_E_CLOSE);
	PPxRegist(vinfo.info.hWnd,RegID,PPXREGIST_FREE);

	CloseViewObject();
	vo_.file.name[0] = '\0';
	UnloadWallpaper(&BackScreen);

	if ( X_hkey != NULL ){
		HeapFree( PPvHeap,0,X_hkey);
		X_hkey = NULL;
	}
	if ( convert == 0 ) PPvSaveCust();
	if ( XV.HiddenMenu.data ){
		HeapFree( PPvHeap,0,XV.HiddenMenu.data);
		XV.HiddenMenu.data = NULL;
	}

	if ( X_win & XWIN_HIDETASK ){
		if ( hCommonWnd != NULL ){
			PostMessage(hCommonWnd,WM_PPXCOMMAND,KRN_freecwnd,0);
		}
	}
	if ( !(X_win & XWIN_MENUBAR) ) DestroyMenu(DynamicMenu.hMenuBarMenu);

	CloseDxDraw(&DxDraw);
	FreeAccServer();
	FreeDynamicMenu(&DynamicMenu);

	DeleteFonts();
	DeleteObject(hStatusLine);
	VFSOff();
	FreeOffScreen(&BackScreen);
}

void InitPPvWindow(void)
{
	const TCHAR *p;
	WNDCLASS wcClass;
	HDC hDC;
	TCHAR buf[VFPS];
	DWORD style;
	HWND hUseParentWnd;

	vinfo.info.Function = (PPXAPPINFOFUNCTION)PPxGetIInfo;
	vinfo.info.Name = T("PPv");
	vinfo.info.RegID = RegID;
	vinfo.info.hWnd = NULL;

	WM_PPXCOMMAND = RegisterWindowMessageA(PPXCOMMAND_WM);
	OSver.dwOSVersionInfoSize = sizeof(OSver);
	GetVersionEx(&OSver);

	memset(&vo_,0,sizeof(vo_));
	ThInit(&vo_.memo);

	SetErrorMode(SEM_FAILCRITICALERRORS);	// 致命的エラーを取得可能にする
	RegCID[1] = RegID[2];

	PPxInitMouseButton(&MouseStat);
	CloseViewObject();
										// レジストリから情報を入手 -----------
	{
		BYTE show;

		show = WinPos.show;
		if ( NO_ERROR != GetCustTable(T("_WinPos"),RegCID,&WinPos,sizeof(WinPos)) ){
			WinPos.pos.right += WinPos.pos.left;
			WinPos.pos.bottom += WinPos.pos.top;
		}
		if ( (show == SW_SHOWDEFAULT) || (show == SW_SHOWNORMAL) ){
			if ( (WinPos.show == SW_SHOWMINIMIZED) ||
				 (WinPos.show == SW_HIDE) ){
				WinPos.show = SW_SHOWNORMAL;
			}
		}else{
			WinPos.show = show;
		}
	}

	PPvLoadCust();

	if ( X_win & XWIN_HIDETASK ){
		int testtime = 200;

		hCommonWnd = PPxGetHWND(T(PPTRAY_REGID) T("A"));
		if ( hCommonWnd == NULL ) ComExec(NULL,T(PPTRAYEXE),PPvPath);
		while( (hCommonWnd == NULL) || (hCommonWnd == BADHWND) ){
			Sleep(50);
			hCommonWnd = PPxGetHWND(T(PPTRAY_REGID) T("A"));
			if ( --testtime == 0 ) break;
		}
		hCommonWnd = (HWND)SendMessage(hCommonWnd,WM_PPXCOMMAND,KRN_getcwnd,0);
	}
	C_BackBrush = CreateSolidBrush(C_back);
	hStatusLine = CreateSolidBrush(C_line);
										// search 呼び出し
	UsePPx();
	p = EnumHistory(PPXH_SEARCH,0);
	if ( p ){
		tstrcpy(VOsel.VSstring,p);
		#ifdef UNICODE
			UnicodeToAnsi(VOsel.VSstringW,VOsel.VSstringA,VFPS);
		#else
			AnsiToUnicode(VOsel.VSstringA,VOsel.VSstringW,VFPS);
		#endif
	}
	FreePPx();
										// ウインドウクラスを定義する ---------
	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= WndProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(Ic_PPV));
	wcClass.hCursor			= LoadCursor(NULL,IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= T(PPVWinClass);
											// クラスを登録する
	RegisterClass(&wcClass);
	InitDynamicMenu(&DynamicMenu,T("MV_menu"),ppvbar);
										// ウィンドウを作成 -------------------
	if ( hViewParentWnd == NULL ){
		style = (X_win & XWIN_NOTITLE) ? WS_NOTITLEOVERLAPPED : WS_OVERLAPPEDWINDOW;
		hUseParentWnd = hCommonWnd;
	}else{
		GetClientRect(hViewParentWnd,&WinPos.pos);
		style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
		WinPos.pos.right -= WinPos.pos.left;
		WinPos.pos.bottom -= WinPos.pos.top;
		WinPos.pos.left = WinPos.pos.top = 0;
		hUseParentWnd = hViewParentWnd;
		setflag(X_win,XWIN_HIDESCROLL);
		XV.img.MagMode = IMGD_AUTOWINDOWSIZE;
		viewopt_def.I_animate = 1;
	}
	vinfo.info.hWnd = CreateWindowEx(0, T(PPVWinClass), NilStr, style,
		WinPos.pos.left, WinPos.pos.top,
		WinPos.pos.right - WinPos.pos.left, WinPos.pos.bottom - WinPos.pos.top,
		hUseParentWnd, NULL, hInst, NULL);
	PPxRegist(vinfo.info.hWnd, RegID, PPXREGIST_SETHWND);
	wsprintf(buf,T("PPV[%c]"),RegID[2]);
	SetWindowText(vinfo.info.hWnd,buf);

	hDC = GetDC(vinfo.info.hWnd);
	CreateDxDraw(&DxDraw,vinfo.info.hWnd);
	MakeFonts(hDC,X_textmag);
	VideoBits = GetDeviceCaps(hDC,BITSPIXEL);
	ReleaseDC(vinfo.info.hWnd,hDC);
	InitSystemDynamicMenu(&DynamicMenu,vinfo.info.hWnd);
	VFSOn(VFS_DIRECTORY | VFS_BMP);

	BackScreen.hOffScreenDC = NULL;
	BackScreen.X_WPbmp.DIB = NULL;
	LoadWallpaper(&BackScreen,vinfo.info.hWnd,RegCID);
	FullDraw = X_fles | BackScreen.X_WallpaperType;
	if ( BackScreen.X_WallpaperType ) X_scrm = 0;
	InitGui();

	PPxRegGetIInfo(&vinfo.info);
}
