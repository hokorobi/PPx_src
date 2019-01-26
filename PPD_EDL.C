/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library					拡張エディット
-----------------------------------------------------------------------------*/
#pragma setlocale("Japanese")
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include <stddef.h>
#include <string.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#define PPEFINDREPLACESTRUCT
#include "PPD_EDL.H"
#include "VFS_STRU.H"
#pragma hdrstop

const TCHAR PPxED[] = T("PPxExEdit");
const TCHAR COMDLG32[] = T("COMDLG32.DLL");
const TCHAR StrReload[] = T("Reload text ?");

#ifndef UNICODE
int xpbug = 0;		// XP で EM_?ETSEL が WIDE に必ずなる bug なら負
#endif
int USEFASTCALL PPeFill1(PPxEDSTRUCT *PES);
int USEFASTCALL PPeDefaultMenu(PPxEDSTRUCT *PES);
ERRORCODE PPedCommand(PPxEDSTRUCT *PES,PPECOMMANDPARAM *param);
ERRORCODE PPedExtCommand(PPxEDSTRUCT *PES,PPECOMMANDPARAM *param,const TCHAR *command);
const TCHAR FINDMSGSTRINGstr[] = FINDMSGSTRING;
UINT ReplaceDialogMessage = 0xff77ff77;

void PPeReplaceStrCommand(PPxEDSTRUCT *PES,FINDREPLACE *freplace)
{
	DWORD firstC,lastC;

	if ( freplace->Flags & FR_FINDNEXT ){
		SearchStr(PES, (freplace->Flags & FR_DOWN) ? 1 : -1 );
	}else if ( freplace->Flags & FR_REPLACE ){
		SendMessage(PES->hWnd,EM_GETSEL,(WPARAM)&firstC,(LPARAM)&lastC);
		if ( firstC != lastC ){
			TEXTSEL ts;

			if ( SelectEditStrings(PES,&ts,TEXTSEL_CHAR) == FALSE ) return;
			if ( tstricmp(ts.word,freplace->lpstrFindWhat) == 0 ){
				SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)freplace->lpstrReplaceWith);
			}
		}
		SearchStr(PES, 1);
	}else if ( freplace->Flags & FR_REPLACEALL ){ // はじめから最後まで
		SendMessage(PES->hWnd,WM_SETREDRAW,FALSE,0);
		SendMessage(PES->hWnd,EM_SETSEL,0,0);
		for ( ;; ){
			if ( SearchStr(PES, 1) == FALSE ) break;
			SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)freplace->lpstrReplaceWith);
		}
		SendMessage(PES->hWnd,WM_SETREDRAW,TRUE,0);
		InvalidateRect(PES->hWnd,NULL,FALSE);
		PostMessage((HWND)freplace->lCustData,WM_CLOSE,0,0);
	}
}

void PPeReplaceStr(PPxEDSTRUCT *PES)
{
	HMODULE hCOMDLG32;
	DefineWinAPI(HWND,ReplaceText,(LPFINDREPLACE));
	HWND hReplaceWnd;
	FINDREPLACE freplace;

	if ( PES->findrep == NULL ){
		if ( InitPPeFindReplace(PES) == FALSE ) return;
	}

	hCOMDLG32 = LoadLibrary(COMDLG32);
	if ( hCOMDLG32 == NULL ) return;
	GETDLLPROCT(hCOMDLG32,ReplaceText);
	if ( DReplaceText == NULL ) return;

	ReplaceDialogMessage = RegisterWindowMessage(FINDMSGSTRINGstr);

	freplace.lStructSize = sizeof(FINDREPLACE);
	freplace.hwndOwner = PES->hWnd;
	freplace.Flags = FR_DOWN | FR_HIDEMATCHCASE | FR_HIDEWHOLEWORD;
	freplace.lpstrFindWhat = PES->findrep->findtext;
	freplace.lpstrReplaceWith = PES->findrep->replacetext;
	freplace.wFindWhatLen = VFPS;
	freplace.wReplaceWithLen = VFPS;

	hReplaceWnd = DReplaceText(&freplace);
	if ( hReplaceWnd != NULL ){
		MSG msg;

		freplace.lCustData = (LPARAM)hReplaceWnd;
		while( (int)GetMessage(&msg,NULL,0,0) > 0 ){
			if ( IsDialogMessage(hReplaceWnd,&msg) ) continue;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if ( !IsWindow(hReplaceWnd) || (freplace.Flags & FR_DIALOGTERM) ){
				// ダイアログを既に閉じた
				hReplaceWnd = NULL;
				break;
			}
		}
		if ( hReplaceWnd != NULL ) DestroyWindow(hReplaceWnd);
	}
	FreeLibrary(hCOMDLG32);
//	SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)T("\"\""));
}

typedef struct {
	HWND hParentWnd;
	RECT boxEdit, boxDialog, boxDesk;
	int delta;
} WindowExpandInfoStruct;

BOOL CALLBACK EnumChildExpandProc(HWND hWnd, LPARAM lParam)
{
	RECT box;
	POINT pos;

	GetWindowRect(hWnd,&box);
	if ( (box.top > ((WindowExpandInfoStruct *)lParam)->boxEdit.top) &&
		 (GetParent(hWnd) == ((WindowExpandInfoStruct *)lParam)->hParentWnd) ){
		pos.x = box.left;
		pos.y = box.top;
		ScreenToClient( ((WindowExpandInfoStruct *)lParam)->hParentWnd, &pos);

		SetWindowPos(hWnd, NULL,
				pos.x, pos.y + ((WindowExpandInfoStruct *)lParam)->delta,
				0,0,
				SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);
	}
	return TRUE;
}

void EditBoxExpand(PPxEDSTRUCT *PES, WindowExpandInfoStruct *eis)
{
	int oldflags = PES->flag;

	/* ウィンドウ調整中に、再調整が発生しないよう、一時的に停止。
		※ DeferWindowPos は、親子関係のウィンドウ変更をまとめて行えない模様 */
	resetflag(PES->flag, PPXEDIT_LINE_MULTI);

	// ダイアログが下にはみ出すときは、上に移動させる
	if ( (eis->boxDialog.bottom + eis->delta) >= eis->boxDesk.bottom ){
		int diff = eis->boxDialog.bottom + eis->delta - eis->boxDesk.bottom;

		if ( (eis->boxDialog.top >= eis->boxDesk.top) && ((eis->boxDialog.top - diff) < eis->boxDesk.top) ){
			diff = eis->boxDialog.top - eis->boxDesk.top;
		}
		if ( diff <= 0 ) return;
		eis->boxDialog.top -= diff;
		eis->boxDialog.bottom -= diff;

		SetWindowPos(eis->hParentWnd, NULL,
				eis->boxDialog.left, eis->boxDialog.top, 0,0,
				SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);
	}

	// IDE_INPUT_LINE edit box
	SetWindowPos(PES->hWnd, NULL, 0,0,
			eis->boxEdit.right - eis->boxEdit.left,
			eis->boxEdit.bottom,
			SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);

	// IDE_INPUT_LINE より下のコントロールを修正
	EnumChildWindows(eis->hParentWnd, EnumChildExpandProc,(LPARAM)eis);

	// Dialog box
	SetWindowPos(eis->hParentWnd, NULL, 0,0,
			eis->boxDialog.right - eis->boxDialog.left,
			eis->boxDialog.bottom - eis->boxDialog.top + eis->delta,
			SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);

	if ( PES->FloatBar.hWnd != NULL ){
		SetWindowPos(PES->FloatBar.hWnd, NULL,
				eis->boxEdit.left, eis->boxEdit.top + eis->boxEdit.bottom,
				0,0,
				SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
	}
	PES->flag = oldflags;
}

void ChangeFontSize(PPxEDSTRUCT *PES,int delta)
{
	HWND hWnd = PES->hWnd;
	HFONT hFont = (HFONT)SendMessage(hWnd,WM_GETFONT,0,0);
	HFONT hNewFont;
	LOGFONT lfont;
	LONG oldheight;
	BOOL firstcreate = FALSE;

	GetObject(hFont, sizeof(LOGFONT), &lfont);
	if ( PES->OrgFontY == 0 ){
		PES->OrgFontY = lfont.lfHeight;
		firstcreate = TRUE;
	}
	oldheight = lfont.lfHeight;
	if ( delta == -9 ){
		lfont.lfHeight = PES->OrgFontY;
	}else{
		if ( lfont.lfHeight > 0 ){
			lfont.lfHeight += delta * 4;
			if ( lfont.lfHeight < 5 ) lfont.lfHeight = 5;
		}else{
			lfont.lfHeight -= delta * 4;
			if ( lfont.lfHeight > -5 ) lfont.lfHeight = -5;
		}
	}
	hNewFont = CreateFontIndirect(&lfont);
	SendMessage(hWnd,WM_SETFONT,(WPARAM)hNewFont,0);
	if ( !firstcreate ) DeleteObject(hFont);

	if ( PES->flag & PPXEDIT_LINEEDIT ) {
		WindowExpandInfoStruct eis;

		eis.delta = lfont.lfHeight - oldheight;
		if ( lfont.lfHeight < 0 ) eis.delta = -eis.delta;

		eis.hParentWnd = GetParent(hWnd);
		GetDesktopRect(hWnd,&eis.boxDesk);
		GetWindowRect(hWnd,&eis.boxEdit);
		GetWindowRect(eis.hParentWnd,&eis.boxDialog);
		eis.boxEdit.bottom += -eis.boxEdit.top + eis.delta;
		EditBoxExpand(PES, &eis);
		hWnd = eis.hParentWnd;
	}
	InvalidateRect(hWnd,NULL,TRUE);
}

PPXINMENU escmenu[] = {
	{ K_c | 'O',	T("&Open File") },
	{ KE_ec,		T("&Close") },
	{ K_F12,		T("&Save as")},
//	{ KE_en,		T("&New Files")},
//	{ ,				T("&Read Files")},
//	{ K_c | 'L',	T("&Load File")},
	{ KE_ea,		T("&Append to") },
//	{ ,				T("&Path Rename")},
	{ KE_ed,		T("&Duplicate")},
//	{ KE_eu,		T("&Undo Edit")},
	{ KE_ei,		T("&Insert File")},
//	{ ,				T("Close all(&X)")},
	{ KE_er,		T("&Run as admin")},
	{ KE_ee,		T("&Exec command")},
	{ KE_eq,		T("&Quit")},
	{ 0,NULL }
};

PPXINMENU f2menu[] = {
	{ KE_qj,	T("&Jump to Line")},
//	{ KE_2b,	T("&Block Top/End")},
//	{ KE_2l,	T("Restore &Line")},
//	{ KE_2s,	T("&Screen Lines")},
//	{ KE_kr,	T("&Read Only")},
	{ KE_qv,	T("&View Mode")},
//	{ KE_2I,	T("&Indent L/R")},
	{ KE_2t,	T("&TAB stop")},
	{ KE_2c,	T("&Char code")},
	{ KE_2r,	T("&Return code")},
//	{ KE_2p,	T("&Word warp")},
//	{ KE_2p,	T("&PPB")},
//	{ KE_2a,	T("&Auto Save")},
	{ 0,NULL }
};

PPXINMENU kmenu[] = {
//	{ KE_kp,	T("&Print")},
//	{ KE_ku,	T("&Undo Paste")},
//	{ KE_kc,	T("Paste(&Copy)")},
//	{ KE_kk,	T("Copy Line/Bloc&k")},
	{ KE_kd,	T("&Duplicate")},
	{ KE_kz,	T("&Zen/han convert")},
//	{ KE_kr,	T("&Read only")},
//	{ KE_kb,	T("Column &Block")},
//	{ KE_ky,	T("&Y clear stack")},
//	{ KE_k0,	T("Set Marker #&0")},
//	{ KE_k1,	T("Set Marker #&1")},
//	{ KE_k2,	T("Set Marker #&2")},
//	{ KE_k3,	T("Set Marker #&3")},
//	{ KE_k4,	T("Set Marker #&4")},
	{ 0,NULL }
};

PPXINMENU qmenu[] = {
//	{ KE_qh,	T("Cut word left(&H)")},
//	{ KE_qt,	T("Cu&t BOL")},
//	{ KE_qy,	T("Cut EOL(&Y)")},
	{ KE_qu,	T("Word case(&U)")},
	{ KE_qv,	T("&View Mode")},
//	{ KE_ql,	T("Restore &Line")},
//	{ KE_qf,	T("Set &Find string")},
//	{ KE_qo,	T("Replece Next(&O)")},
//	{ KE_qp,	T("&Put File Name")},
	{ K_c | K_s | 'I',T("&Insert selected filename")},
//	{ KE_qi,	T("&Insert find str")},

	{ KE_qj		,T("&Jump to Line")},
	{ K_c | K_Pup,T("Top of File(&R)")},
	{ K_c | K_Pdw,T("End of File(&C)")},
//	{ KE_qp,	T("Last &Position")},
//	{ KE_qo,	T("Replace Next(&O)")},
//	{ KE_qlw,	T("Left of Window(&[)")},
//	{ KE_qrw,	T("Right of Window(&])")},
//	{ KE_qs,	T("Top of Line(&S)")},
//	{ KE_qd,	T("End of Line(&D)")},
	{ K_c | K_up,T("Top of Window(&E)")},
//	{ K_c | K_dw,T("End of Window(&X)")},
//	{ KE_qk,	T("Jump Brac&ket")},
//	{ KE_qm,	T("Set &Marker #0")},
//	{ KE_q0,	T("Jump to Marker #&0")},
//	{ KE_q1,	T("Jump to Marker #&1")},
//	{ KE_q2,	T("Jump to Marker #&2")},
//	{ KE_q3,	T("Jump to Marker #&3")},
//	{ KE_q4,	T("Jump to Marker #&4")},
//	{ KE_qb,	T("&Block Top/End")},
	{ 0,NULL }
};

//abcdefg  j lmnopq stu w  z
PPXINMENU amenu[] = {
	{ K_c | 'Z',	T("&Undo\tCtrl+Z")},
	{PPXINMENY_SEPARATE,NULL},
	{ K_c | 'X',	T("%G\"JMCU|Cut\"(&T)\tCtrl+X")},
	{ K_c | 'C',	T("%G\"JMCL|Copy\"(&C)\tCtrl+C")},
	{ K_c | 'V',	T("%G\"JMPA|Paste\"(&P)\tCtrl+V")},
	{ K_del,		T("%G\"JMDE|Delete\"(&D)\tDelete")},
	{PPXINMENY_SEPARATE,NULL},
	{ K_c | 'A',	T("%G\"JMAL|Select All\"(&A)\tCtrl+A")},

	{ KE_defmenu | B31,	T("Default menu(&B)\tShift+F10")},
	{ K_c | ']',	T("File &menu\tCtrl+]")},
	{ K_c | 'Q',	T("Edit menu\tCtrl+&Q")},
	{ K_s | K_F2,	T("&Setting menu\tShift+F2")},
	{PPXINMENY_SEPARATE,NULL},
	{ KE_qu,		T("&Word case\tCtrl+Q-U")},
	{ KE_kz,		T("&Zen/han convert\tCtrl+K-Z")},
	{ KE_er,		T("&Run as admin\tESC-R")},
//	{ KE_ee,		T("&Execute command\tESC-E")},

	{ KE_qj | B31,	T("&Jump to Line\tCtrl+Q-J")},
	{ K_c | 'F',	T("&Find\tCtrl+F")},
	{PPXINMENY_SEPARATE,NULL},
	{ K_raw | K_s | K_c | 'P',T("Path List(&N)\tCtrl+Shift+P")},
	{ K_raw | K_s | K_c | 'L',T("PPc &List\tCtrl+Shift+L")},
	{ K_c | K_s | 'D',T("F&older dialog...\tCtrl+Shift+D")},
	{ K_c | K_s | 'I',T("Filename dialo&g...\tCtrl+Shift+I")},
	{ 0,NULL }
};

const TCHAR amenu2str[] = T("&Insert");
PPXINMENU amenu2[] = {
	{ K_c | 'N',	T("File&name\tCtrl+N")},
	{ K_c | 'P',	T("Full&path Filename\tCtrl+P")},
	{ K_c | 'E',	T("Filename without &ext\tCtrl+E")},
	{ K_c | 'T',	T("File ext\tCtrl+&T")},
	{ K_c | 'R',	T("Filename on curso&r\tCtrl+&R")},
	{ K_c | '0',	T("PPx path\tCtrl+&0")},
	{ K_c | '1',	T("Current path\tCtrl+&1")},
	{ K_c | '2',	T("Pair path\tCtrl+&2")},
	{ K_F5,			T("now &Date\tF5")},
	{ 0,NULL }
};

PPXINMENU returnmenu[] = {
	{ VTYPE_CRLF + 1,	T("C&RLF")},
	{ VTYPE_CR + 1,		T("&CR")},
	{ VTYPE_LF + 1,		T("&LF")},
	{ 0,NULL }
};

const TCHAR charmenustr_lcp[] = T("&local codepage");
const TCHAR charmenustr_other[] = T("&other...");

PPXINMENU charmenu[charmenu_items] = {
	{ CP__US,			T("US(&A)")},
	{ CP__LATIN1,		T("&Latin1")},
	{ VTYPE_SYSTEMCP,	T("&Shift_JIS")},
	{ VTYPE_EUCJP,		T("&EUC-JP")},
	{ CP__UTF16L,		T("UTF-1&6")},
	{ VTYPE_UNICODE,	T("&UTF-16LE(BOM)")},
	{ CP__UTF16B,		T("UTF-1&6BE")},
	{ VTYPE_UNICODEB,	T("UTF-1&6BE(BOM)")},
	{ CP_UTF8,			T("UTF-&8")},
	{ VTYPE_UTF8,		T("UTF-8(&BOM)")},
	{ 0,NULL }, //	{ VTYPE_SYSTEM/CP_UTF8,	charmenustr_lcp},
	{ 0,NULL }, //	{ VTYPE_OTHER		,T("codepage : %d")}
	{ 0,NULL }
};

typedef struct {
	PPXAPPINFO info;
	PPxEDSTRUCT *PES;
} EDITMODULEINFOSTRUCT;
const TCHAR EditInfoName[] = T("Edit");
const TCHAR GetFileExtsStr[] = T("All Files\0*.*\0\0");

LRESULT EdPPxWmCommand(PPxEDSTRUCT *PES,HWND hWnd,WPARAM wParam,LPARAM lParam);
void GetPPePopupPositon(PPxEDSTRUCT *PES,POINT *pos);

#define WIDEDELTA 32

#define CLOSELIST_NONE			0 // リストは表示されていなかった
#define CLOSELIST_MANUALLIST	1 // 手動表示リストを閉じた
#define CLOSELIST_AUTOLIST		2 // 自動リストを閉じた

DefineWinAPI(HRESULT,SHAutoComplete,(HWND hwndEdit,DWORD dwFlags)) = NULL;
#ifndef SHACF_FILESYSTEM
#define SHACF_AUTOAPPEND_FORCE_ON	0x40000000
#define SHACF_AUTOSUGGEST_FORCE_ON	0x10000000
#define SHACF_FILESYSTEM			1
#define SHACF_USETAB				8
#endif

LOADWINAPISTRUCT SHLWAPIDLL[] = {
	LOADWINAPI1(SHAutoComplete),
	{NULL,NULL}
};
#define FloatBarID 12266
const TCHAR FloatClassStr[] = T("PPxFloatBar");
const TCHAR FloatBarNameMouseStr[] = T("B_flm");
const TCHAR FloatBarNamePenStr[] = T("B_flp");
const TCHAR FloatBarNameTouchStr[] = T("B_flt");

LRESULT CALLBACK FloatProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PPxEDSTRUCT *PES;

	PES = (PPxEDSTRUCT *)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if ( PES == NULL ){
		if ( message == WM_CREATE ){
			PES = (PPxEDSTRUCT *)(((CREATESTRUCT *)lParam)->lpCreateParams);
			SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)PES);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	switch (message){
		case WM_COMMAND:
			if ( (HWND)lParam == PES->FloatBar.hToolBarWnd ){
				const TCHAR *ptr;

				ptr = GetToolBarCmd((HWND)lParam,&PES->FloatBar.Cmd,LOWORD(wParam));
				if ( ptr != NULL ){
					if ( ((UTCHAR)ptr[0] == EXTCMD_KEY) &&
						 (*(WORD *)(ptr + 1) == 'X') ){
						PostMessage(hWnd,WM_CLOSE,0,0);
					}else{
						PPECOMMANDPARAM param = {0,0};

						PPedExtCommand(PES,&param,ptr);
					}
				}
				return 0;
			}
			break;

		case WM_WINDOWPOSCHANGED: {
			RECT box;

			GetClientRect(hWnd,&box);
			SetWindowPos(PES->FloatBar.hToolBarWnd,NULL,
					0,box.top, box.right,32,SWP_NOACTIVATE | SWP_NOZORDER);
			break;
		}

		case WM_DESTROY:
			CloseToolBar(PES->FloatBar.hToolBarWnd);
			PES->FloatBar.hToolBarWnd = NULL;
			ThFree(&PES->FloatBar.Cmd);

			PES->FloatBar.hWnd = NULL;
			break;

		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE; // 非アクティブ
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#define BARSPACE 2
void InitFloatBar(PPxEDSTRUCT *PES,int posX)
{
	WNDCLASS FloatClass;
	UINT ID = FloatBarID;
	RECT box, editbox;
	LPARAM pointtype = GetMessageExtraInfo();
	const TCHAR *FloatBarNamePtr;
	int minheight, posY;
	HWND hParentWnd;

	if ( PES->FloatBar.Cmd.size == 1 ) return;

	if ( (pointtype & POINTTYPE_SIG_MASK) == POINTTYPE_TOUCH ){
		minheight = 32 - BARSPACE;
		FloatBarNamePtr = FloatBarNameTouchStr;
		if ( (pointtype & POINTTYPE_TOUCH_MASK) == POINTTYPE_TOUCH_PEN ){ //pen
			if ( (X_pmc[X_pmc_pen] & TOUCH_EDIT_COMMAND_BAR) ){ // 負論理注意
				PES->FloatBar.Cmd.size = 1;
				return;
			}
			if ( IsExistCustData(FloatBarNamePenStr) ){
				FloatBarNamePtr = FloatBarNamePenStr;
			}
		}else{ //touch
			if ( (X_pmc[X_pmc_touch] & TOUCH_EDIT_COMMAND_BAR) ){ // 負論理注意
				PES->FloatBar.Cmd.size = 1;
				return;
			}
		}
	}else{
		if ( (X_pmc[X_pmc_mouse] & TOUCH_EDIT_COMMAND_BAR) ){ // 負論理注意
			PES->FloatBar.Cmd.size = 1;
			return;
		}

		minheight = 16 - BARSPACE;
		FloatBarNamePtr = FloatBarNameMouseStr;
	}

	if ( !IsExistCustData(FloatBarNamePtr) ){
		PES->FloatBar.Cmd.size = 1;
		return;
	}

	FloatClass.style		= 0;
	FloatClass.lpfnWndProc	= FloatProc;
	FloatClass.cbClsExtra	= 0;
	FloatClass.cbWndExtra	= 0;
	FloatClass.hInstance	= DLLhInst;
	FloatClass.hIcon		= NULL;
	FloatClass.hCursor		= LoadCursor(NULL,IDC_ARROW);
	FloatClass.hbrBackground	= NULL;
	FloatClass.lpszMenuName	= NULL;
	FloatClass.lpszClassName	= FloatClassStr;
	RegisterClass(&FloatClass);

	// ツールバー親
	PES->FloatBar.hWnd = CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
		FloatClassStr,NilStr,WS_POPUP,
		0,0,0,0,PES->hWnd,NULL,DLLhInst,(LPVOID)PES);

	ThInit(&PES->FloatBar.Cmd);
	PES->FloatBar.hToolBarWnd = CreateToolBar(&PES->FloatBar.Cmd,PES->FloatBar.hWnd,&ID,FloatBarNamePtr,DLLpath,0);

	// ツールバー本体の位置調整と大きさ調整
	SetWindowPos(PES->FloatBar.hToolBarWnd,NULL,
			0,0, 0,0,SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

	SendMessage(PES->FloatBar.hToolBarWnd,TB_GETITEMRECT,
		SendMessage(PES->FloatBar.hToolBarWnd,TB_BUTTONCOUNT,0,0) - 1,
		(LPARAM)(RECT *)&box);

	minheight = (minheight * GetMonitorDPI(PES->hWnd)) / DEFAULT_WIN_DPI;
	if ( box.bottom < minheight ){
		box.bottom = minheight;
		SendMessage(PES->FloatBar.hToolBarWnd,TB_SETBUTTONSIZE,0,TMAKELPARAM(minheight,minheight));
	}
	box.bottom += BARSPACE;

	GetWindowRect(PES->hWnd,&editbox);

	posX = editbox.left + posX - box.right / 2;
	if ( posX < editbox.left ) posX = editbox.left;
	posY = editbox.bottom; // 通常の場所: EDIT の直下
	hParentWnd = GetParentCaptionWindow(PES->hWnd);
	if ( hParentWnd != PES->hWnd ){
		RECT tmpbox;

		GetWindowRect(hParentWnd, &tmpbox);
		if ( (tmpbox.bottom - posY) < (int)PES->fontY * 3 ) posY = tmpbox.bottom - 4;
		if ( PES->flag & PPXEDIT_COMBOBOX ){
			posY = editbox.top - box.bottom;
		}
	}

	SetWindowPos(PES->FloatBar.hWnd,NULL,
			posX,posY,  box.right,box.bottom,
			SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOZORDER);

	ShowWindow(PES->FloatBar.hWnd,SW_SHOWNOACTIVATE);
}

void WMGestureEdit(HWND hWnd,WPARAM wParam/*,LPARAM lParam*/)
{
/*
	if ( TouchMode == 0 ){
		GetCustData(T("X_pmc"),&X_pmc,sizeof(X_pmc));
		if ( X_pmc[0] < 0 ){
			TouchMode = ~X_pmc[1];
			PPxCommonCommand(hWnd,0,K_E_TABLET);
		}
	}else{
*/
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
}

void PPeRunas(PPxEDSTRUCT *PES,TCHAR *cmdline)
{
	TCHAR path[VFPS];
	TCHAR exefile[VFPS];
	const TCHAR *ptr;
	PPXCMDENUMSTRUCT work;

	PPxEnumInfoFunc(PES->info,'1',path,&work);
	ptr = cmdline;
	GetLineParam(&ptr,exefile);
	if ( PPxShellExecute(PES->hWnd,T("RUNAS"),exefile,ptr,path,0,cmdline) != NULL ){
		WriteHistory(PPXH_COMMAND,cmdline,0,NULL);
		PostMessage(GetParentCaptionWindow(PES->hWnd),WM_CLOSE,0,0);
	}else{
		if ( GetLastError() != ERROR_CANCELLED ){
			XMessage(PES->hWnd,NULL,XM_GrERRld,cmdline);
		}
	}
}

void WideWindowByKey(PPxEDSTRUCT *PES,int offset)
{
	RECT box;
	HWND hWnd;

	if ( !(PES->flag & PPXEDIT_LINEEDIT) ) return;
	hWnd = GetParentCaptionWindow(PES->hWnd);
	GetWindowRect(hWnd,&box);
	box.right -= box.left;
	if ( (offset > 0) || (box.right > 64) ){
		box.right += offset * WIDEDELTA;
		SetWindowPos(hWnd,NULL,0,0,
				box.right,box.bottom - box.top,SWP_NOZORDER | SWP_NOMOVE);
	}
}

void USEFASTCALL PPeConvertZenHanMain(PPxEDSTRUCT *PES,HWND hWnd,int mode)
{
	TEXTSEL ts;
	TCHAR buf[0x8000];
	TCHAR *buflast;

	if ( SelectEditStrings(PES,&ts,TEXTSEL_WORD) == FALSE ) return;

	{	// 簡易全半検出
		const TCHAR *first;

		for ( first = ts.word ; (UTCHAR)*first < ' ' ; first++ );

		if ( mode < 0 ){
		#ifdef UNICODE
			mode = ((*first >= L' ') && (*first <= L'~')) || ((*first >= L'｡') && (*first <= L'ﾟ'));
		#else
			mode = !IskanjiA(*first);
		#endif
		}
	}

	if ( mode ){
		buflast = Strsd(buf,ts.word);
	}else{
		buflast = Strds(buf,ts.word);
	}
#ifdef UNICODE
	if ( memcmp(buf,ts.word,TSTROFF(buflast - buf)) == 0 )
#else
	if ( (buflast - buf) == tstrlen(ts.word) )
#endif
	{
		if ( !mode ){
			buflast = Strsd(buf,ts.word);
		}else{
			buflast = Strds(buf,ts.word);
		}
	}
	SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)buf);
	if ( ts.cursororg.start != ts.cursororg.end ){
		ts.cursororg.end = buflast - buf;
#ifndef UNICODE
		if ( xpbug < 0 ) CaretFixToW(buf,&ts.cursororg.end);
#endif
		ts.cursororg.end += ts.cursororg.start;
	}
	SendMessage(hWnd,EM_SETSEL,ts.cursororg.start,ts.cursororg.end);
}

/* *completelist /list:on /double:on /textfile:"" /headsearch
*/

void EditCompleteListCommand(PPxEDSTRUCT *PES,const TCHAR *param)
{
	TCHAR *more;
	TCHAR buf[CMDLINESIZE];
	UTCHAR code;

	BOOL hist = FALSE;
	BOOL setmode = FALSE;

	while( '\0' != (code = GetOptionParameter(&param,buf,&more)) ){
		if ( code == '-' ){
			if ( !tstrcmp( buf + 1,T("RELOAD")) ){
				CleanUpEdit();
				continue;
			}

			if ( !tstrcmp( buf + 1,T("SET")) ){
				setmode = TRUE;
				continue;
			}

			if ( !tstrcmp( buf + 1,T("LIST")) ){
				hist = TRUE;
				continue;
			}

			if ( !tstrcmp( buf + 1,T("FILE")) ){
				ERRORCODE result;

				if ( *more == '\0' ){
					if ( PES->list.filltext_user.mem != NULL ){
						HeapFree(ProcHeap,0,PES->list.filltext_user.mem);
						PES->list.filltext_user.mem = NULL;
					}
					return;
				}

				VFSFullPath(NULL,more,DLLpath);

				if ( (result = LoadTextImage(more,&PES->list.filltext_user.mem,&PES->list.filltext_user.text,NULL)) != NO_ERROR ){
					PPErrorBox(PES->hWnd,T("*completelist /file"),result);
					return;
				}
				continue;
			}
		}
		XMessage(PES->hWnd,T("*completelist"),XM_GrERRld,StrOptionError,buf);
		return;
	}
	if ( IsTrue(setmode) ) return;

	KeyStepFill(PES,hist);
}

void EditTreeCommand(PPxEDSTRUCT *PES,const TCHAR *param)
{
	TCHAR cmdstr[VFPS];

	if ( PES->hTreeWnd == NULL ){
		GetLineParam(&param,cmdstr);
		if ( !tstrcmp(cmdstr,T("off")) ) return; // close 済み
		PPeTreeWindow(PES);
		if ( cmdstr[0] == '\0' ) return;
	}
	if ( PES->hTreeWnd != NULL ){
		SendMessage(PES->hTreeWnd,VTM_TREECOMMAND,0,(LPARAM)param); // 整形前を渡す
	}
}

BOOL EditModeParam(EDITMODESTRUCT *ems,const TCHAR *param,const TCHAR *more)
{
	// codepage
	if ( tstrcmp(param,T("SYSTEM")) == 0 ){
		ems->codepage = VTYPE_SYSTEMCP;
	}else if ( (tstrcmp(param,T("UNICODE")) == 0) || (tstrcmp(param,T("UTF16")) == 0) ){
		ems->codepage = CP__UTF16L;
	}else if ( (tstrcmp(param,T("UNICODEB")) == 0) || (tstrcmp(param,T("UTF16BE")) == 0) ){
		ems->codepage = CP__UTF16B;
	}else if ( tstrcmp(param,T("UNICODEBOM")) == 0 ){
		ems->codepage = VTYPE_UNICODE;
	}else if ( tstrcmp(param,T("UNICODEBBOM")) == 0 ){
		ems->codepage = VTYPE_UNICODEB;
	}else if ( tstrcmp(param,T("EUC")) == 0 ){
		ems->codepage = VTYPE_EUCJP;
	}else if ( tstrcmp(param,T("UTF8")) == 0 ){
		ems->codepage = CP_UTF8;
	}else if ( tstrcmp(param,T("UTF8BOM")) == 0 ){
		ems->codepage = VTYPE_UTF8;
	}else if ( tstrcmp(param,T("SJIS")) == 0 ){
		ems->codepage = (GetACP() == CP__SJIS) ? VTYPE_SYSTEMCP : CP__SJIS;
	}else if ( (tstrcmp(param,T("IBM")) == 0) || (tstrcmp(param,T("US")) == 0) ){
		ems->codepage = CP__US;
	}else if ( (tstrcmp(param,T("ANSI")) == 0) || (tstrcmp(param,T("LATIN1")) == 0)){
		ems->codepage = CP__LATIN1;
	}else if ( tstrcmp(param,T("CODEPAGE")) == 0 ){
		ems->codepage = GetNumber((const TCHAR **)&more);
	// crcide
	}else if ( tstrcmp(param,T("CR")) == 0 ){
		ems->crcode = VTYPE_CR;
	}else if ( tstrcmp(param,T("CRLF")) == 0 ){
		ems->crcode = VTYPE_CRLF;
	}else if ( tstrcmp(param,T("LF")) == 0 ){
		ems->crcode = VTYPE_LF;
	// tabwidth
	}else if ( tstrcmp(param,T("TAB")) == 0 ){
		ems->tabwidth = GetNumber((const TCHAR **)&more);
	}else{
		return FALSE;
	}
	return TRUE;
}

void EditSetModeCommand(PPxEDSTRUCT *PES, const TCHAR *param)
{
	TINPUT_EDIT_OPTIONS options;
	TCHAR buf[CMDLINESIZE], code, *more;
	EDITMODESTRUCT ems = {EDITMODESTRUCT_DEFAULT};

	for ( ;; ){
		code = GetOptionParameter(&param, buf, &more);
		if ( code == '\0' ) break;
		if ( code == '-' ){
			if ( IsTrue(EditModeParam(&ems, buf + 1, more)) ){
				if ( ems.codepage > 0 ) PES->CharCode = ems.codepage;
				if ( ems.crcode >= 0 ) PES->CrCode = ems.crcode;
				if ( ems.tabwidth >= 0 ){
					PPeSetTab(PES,ems.tabwidth);
					ems.tabwidth = -1;
				}
			}
			continue;
		}
		more = buf;
		if ( IsTrue(GetEditMode((const TCHAR **)&more, &options)) ){
			PES->list.RhistID = options.hist_readflags;
			PES->list.WhistID = HistWriteTypeflag[options.hist_writetype];
			PES->flag = (PES->flag & ~(TIEX_REFTREE | TIEX_SINGLEREF)) |
				TinputTypeflags[options.hist_writetype];
		}
	}
}

void GetCursorLocate(PPxEDSTRUCT *PES,DWORD *start,DWORD *end)
{
	ECURSOR cursor = {0,0};
	TCHAR buf[0x1000];

	if ( (DWORD)SendMessage(PES->hWnd,WM_GETTEXT,TSIZEOF(buf),(LPARAM)&buf) <
			TSIZEOF(buf) ){
		GetEditSel(PES->hWnd,buf,&cursor);
	}
	if ( start != NULL ) *start = cursor.start;
	if ( end != NULL ) *end = cursor.end;
}

// 一行編集時に使用する
DWORD_PTR USECDECL EditInfoFunc(PPXAPPINFO *info,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	PPxEDSTRUCT *PES;

	PES = ((EDITMODULEINFOSTRUCT *)info)->PES;
	switch (cmdID){
		case PPXCMDID_PPXCOMMAD: {
			PPECOMMANDPARAM param;

			param.key = uptr->key;
			param.repeat = 0;

			PPedCommand(PES,&param);
			return 0;
		}

		case PPXCMDID_REQUIREKEYHOOK:
			PES->KeyHookEntry = FUNCCAST(CALLBACKMODULEENTRY,uptr);
			break;

		case PPXCMDID_POPUPPOS:
			GetPPePopupPositon(PES,(POINT *)uptr);
			break;

		case PPXCMDID_SETPOPLINE: {
			if ( *uptr->str ){
				SetMessageForEdit(PES->hWnd,uptr->str);
			}else{
				SetMessageForEdit(PES->hWnd,NULL);
			}
			break;
		}

		case PPXCMDID_CSRX:
			GetCursorLocate(PES,&uptr->num,NULL);
			break;

		case PPXCMDID_CSRY:
			GetCursorLocate(PES,NULL,&uptr->num);
			break;

		case PPXCMDID_COMMAND:{
			const TCHAR *param = uptr->str + tstrlen(uptr->str) + 1;

			if ( !tstrcmp(uptr->str,T("TREE")) ){
				EditTreeCommand(PES,param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("EDITMODE")) ){
				EditSetModeCommand(PES,param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("SETCAPTION")) ){
				SetWindowText(GetParent(PES->hWnd),param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("COMPLETELIST")) ){
				EditCompleteListCommand(PES,param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("DEFAULTMENU")) ){
				PPeDefaultMenu(PES);
				break;
			}
			if ( !tstrcmp(uptr->str,T("REPLACEFILE")) ){
				OpenFromFile(PES,PPE_OPEN_MODE_CMDOPEN,param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("INSERTFILE")) ){
				OpenFromFile(PES,PPE_OPEN_MODE_CMDINSERT,param);
				break;
			}
			if ( !tstrcmp(uptr->str,T("ZENHAN")) ){
				int mode = -1;
				TCHAR c = *param;

				if ( (c == 'z') || (c == '1') ) mode = 1;
				if ( (c == 'h') || (c == '0') ) mode = 0;
				PPeConvertZenHanMain(PES,PES->hWnd,mode);
				break;
			}
			// default へ
		}

		default:
			return PPxInfoFunc(PES->info,cmdID,uptr);
	}
	return 1;
}

int CallEditKeyHook(PPxEDSTRUCT *PES,WORD key)
{
	PPXMKEYHOOKSTRUCT keyhookinfo;
	PPXMODULEPARAM pmp;
	EDITMODULEINFOSTRUCT ppxa;

	keyhookinfo.key = key;
	pmp.keyhook = &keyhookinfo;
	ppxa.info.Name = EditInfoName;
	ppxa.info.RegID = PES->info->RegID;
	ppxa.info.Function = (PPXAPPINFOFUNCTION)EditInfoFunc;
	ppxa.info.hWnd = PES->hWnd;
	ppxa.PES = PES;

	return CallModule(&ppxa.info,PPXMEVENT_KEYHOOK,pmp,PES->KeyHookEntry);
}

ERRORCODE PPXAPI EditExtractMacro(PPxEDSTRUCT *PES,const TCHAR *param,TCHAR *extract,int flags)
{
	EDITMODULEINFOSTRUCT ppxa;

	ppxa.info.Name = EditInfoName;
	ppxa.info.RegID = PES->info->RegID;
	ppxa.info.Function = (PPXAPPINFOFUNCTION)EditInfoFunc;
	ppxa.info.hWnd = PES->hWnd;
	ppxa.PES = PES;
	return PP_ExtractMacro(ppxa.info.hWnd,&ppxa.info,NULL,param,extract,flags);
}

void CmdSelText(PPxEDSTRUCT *PES,TCHAR *strbuf)
{
	TEXTSEL ts;

	if ( SelectEditStrings(PES,&ts,TEXTSEL_ALL) == FALSE ) return;
	if ( tstrlen(ts.word) >= (CMDLINESIZE - 1) ) return;
	tstrcpy(strbuf,ts.word);
}

HMENU MakePopupMenus(PPXINMENU *menus,DWORD check)
{
	HMENU hMenu;
	TCHAR strbuf[0x200];

	hMenu = CreatePopupMenu();
	while ( menus->key ){
		if ( menus->str != NULL ){
			const TCHAR *str;

			if ( *menus->str == '%' ){
				PP_ExtractMacro(NULL,NULL,NULL,menus->str,strbuf,0);
				str = strbuf;
			}else{
				str = menus->str;
			}

			AppendMenu(hMenu,
					((menus->key == check) ? MF_CHECKED : 0) |
					((menus->key & B31) ? MF_ES | MF_MENUBARBREAK : MF_ES),
					menus->key & ~B31,str);
		}else{
			AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
		}
		menus++;
	}
	return hMenu;
}

void GetPPePopupPositon(PPxEDSTRUCT *PES,POINT *pos)
{
	if ( PES->mousepos ){
		GetCursorPos(pos);
	}else{
		GetCaretPos(pos);
		pos->y += PES->fontY;
		ClientToScreen(PES->hWnd,pos);
	}
}

void PPeMenu(PPxEDSTRUCT *PES,PPECOMMANDPARAM *param,PPXINMENU *menus)
{
	POINT pos;
	HMENU popup;
	WORD key;

	GetPPePopupPositon(PES,&pos);
	popup = MakePopupMenus(menus,0);
	if ( menus == amenu ){
		AppendMenu(popup,MF_EPOP,(UINT_PTR)MakePopupMenus(amenu2,0),amenu2str);
	}
	key = (WORD)TrackPopupMenu(popup,TPM_TDEFAULT,pos.x,pos.y,0,PES->hWnd,NULL);
	DestroyMenu(popup);
	if ( key != 0 ){
		param->key = key;
		PPedCommand(PES,param);
	}
}

	// ^V
int USEFASTCALL PPePaste(PPxEDSTRUCT *PES)
{
	if ( !(PES->flag & PPXEDIT_TEXTEDIT) && IsTrue(OpenClipboard(PES->hWnd)) ){
		HGLOBAL hGMem;
		size_t maxlen;
		BOOL dopaste;

		dopaste = FALSE;
		maxlen = SendMessage(PES->hWnd,EM_GETLIMITTEXT,0,0);
		hGMem = GetClipboardData(CF_TTEXT);
		if ( hGMem != NULL ){
			TCHAR *src,*srcmax,*dest;
			size_t len;
			TCHAR *textp;

			src = GlobalLock(hGMem);
			len = tstrlen(src);
			if ( len >= maxlen ) len = maxlen;
			textp = HeapAlloc(DLLheap,0,TSTROFF(len + 1));
			if ( textp != NULL ){
				srcmax = src + len;
				dest = textp;
				while ( src < srcmax ){
					if ( (*src != '\r') && (*src != '\n') ){
						*dest++ = *src;
					}
					if ( src == srcmax ) break;
					src++;
				}

				if ( (size_t)(dest - textp) < len ){
					*dest = '\0';
					SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)textp);
					dopaste = TRUE;
				}

				HeapFree(DLLheap,0,textp);
			}
			GlobalUnlock(hGMem);
		}else if ( (hGMem = GetClipboardData(RegisterClipboardFormat(CFSTR_SHELLIDLIST))) != NULL ){
			TCHAR text[CMDLINESIZE];

			GetTextFromCF_SHELLIDLIST(text,CMDLINESIZE,hGMem,FALSE);
			SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)text);
			dopaste = TRUE;
		}
		CloseClipboard();
		if ( IsTrue(dopaste) ) return 0;
	}
	// 独自処理をしなかったのでもとの処理を行う
	CallWindowProc(PES->hOldED,PES->hWnd,WM_CHAR,'V'-'@',0);
	return 0;
}


		// ^S
int USEFASTCALL PPeSaveFile(PPxEDSTRUCT *PES)
{
	FileSave(PES,EDL_FILEMODE_NODIALOG);
	return 0;
}
		// F12 ,ESC-S
int USEFASTCALL PPeSaveAsFile(PPxEDSTRUCT *PES)
{
	FileSave(PES,EDL_FILEMODE_DIALOG);
	return 0;
}

int USEFASTCALL PPePPcList(PPxEDSTRUCT *PES)
{
	HMENU hMenu;
	int id;
	DWORD id2 = 1;
	POINT pos;
	TCHAR path[VFPS + 8];

	hMenu = CreatePopupMenu();
	GetPPxList(hMenu, GetPPcList_Path, NULL, &id2);

	GetPPePopupPositon(PES,&pos);
	id = TrackPopupMenu(hMenu,TPM_TDEFAULT,pos.x,pos.y,0,PES->hWnd,NULL);
	if ( id ){
		TCHAR *sep;

		path[0] = '\0';
		GetMenuString(hMenu,id,path,TSIZEOF(path),MF_BYCOMMAND);
		if ( PES->flag & PPXEDIT_SINGLEREF ){
			SendMessage(PES->hWnd,EM_SETSEL,0,EC_LAST);
		}
		sep = tstrchr(path,':'); // "&A: path"
		if ( sep != NULL ){
			SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)(sep + 2));
		}
	}
	DestroyMenu(hMenu);
	return 0;
}

			// ESC-A
int USEFASTCALL PPeAppendFile(PPxEDSTRUCT *PES)
{
	FileSave(PES, EDL_FILEMODE_APPEND);
	return 0;
}

			// ^O ,ESC-O
int USEFASTCALL PPeOpenFileCmd(PPxEDSTRUCT *PES)
{
	FileOpen(PES, PPE_OPEN_MODE_OPEN);
	return 0;
}

// ESC-I
int USEFASTCALL PPeInsertFile(PPxEDSTRUCT *PES)
{
	FileOpen(PES, PPE_OPEN_MODE_INSERT);
	return 0;
}

							// ESC-C
int USEFASTCALL PPeCloseFile(PPxEDSTRUCT *PES)
{
	HWND hWnd = PES->hWnd;

	if ( EdPPxWmCommand(PES,hWnd,KE_closecheck,0) ){
		SendMessage(hWnd,EM_SETMODIFY,FALSE,0);
		PostMessage(GetParentCaptionWindow(hWnd),WM_CLOSE,0,0);
	}
	return 0;
}
//----------------------------------------------- ファイル検索
int USEFASTCALL PPeFillMain(PPxEDSTRUCT *PES,HWND hWnd)
{
	ECURSOR cursor;
	TCHAR *ptr;
	TCHAR buf[0x800];
	DWORD mode;

	mode = PES->ED.cmdsearch & (CMDSEARCH_FLOAT | CMDSEARCH_ROMA);
												// サイズチェック
	buf[0] = '\0';
	if ( (DWORD)SendMessage(hWnd,WM_GETTEXT,TSIZEOF(buf),(LPARAM)&buf) >=
			TSIZEOF(buf) ){
		return 0;
	}
												// 編集文字列(全て)を取得
	GetEditSel(hWnd,buf,&cursor);
	SendMessage(hWnd,WM_SETREDRAW,FALSE,0);
	mode |= ((PES->list.WhistID & PPXH_COMMAND) ?
						CMDSEARCH_CURRENT : CMDSEARCH_OFF) |
			((PES->flag & PPXEDIT_SINGLEREF) ? 0 : CMDSEARCH_MULTI) |
			CMDSEARCH_EDITBOX;

	if ( (PES->list.WhistID & (PPXH_DIR | PPXH_PPCPATH)) &&
		 IsTrue(GetCustDword(T("X_fdir"),TRUE)) ){
		setflag(mode,CMDSEARCH_DIRECTORY);
	}

	ptr = SearchFileIned(&PES->ED,buf,&cursor,mode);
	if ( ptr != NULL ){
		size_t len;

		SendMessage(hWnd,EM_SETSEL,cursor.start,cursor.end);
								// ※↑SearchFileIned 内で加工済み
		SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)ptr);
		SendMessage(hWnd,EM_SETSEL,0,0);	// 表示開始桁を補正させる

		len = tstrlen(ptr);
		if ( len && (*(ptr + len - 1) == '\"') ) len--;
#ifndef UNICODE
		if ( xpbug < 0 ) CaretFixToW(ptr,(DWORD *)&len);
#endif
		SendMessage(hWnd,EM_SETSEL,cursor.start + len,cursor.start + len);
		SendMessage(hWnd,WM_SETREDRAW,TRUE,0);
		InvalidateRect(hWnd,NULL,FALSE);
	}else{
		SendMessage(hWnd,WM_SETREDRAW,TRUE,0);
	}
	return 0;
}

int USEFASTCALL PPeFillInit(PPxEDSTRUCT *PES)
{
	if ( PES->flag & PPXEDIT_LISTCOMP ){
		if ( PES->list.mode >= LIST_FILL ){
			if ( PES->list.ListWindow != LISTU_NOLIST ){
				PES->oldkey2 = 1;
				if ( ListUpDown(PES->hWnd,PES,1,0) == FALSE ) return 0;
				return 0;
			}
		}
		FloatList(PES,2);
		return 0;
	}
	return PPeFillMain(PES,PES->hWnd);
}

int USEFASTCALL PPeFill1(PPxEDSTRUCT *PES)
{
	resetflag(PES->ED.cmdsearch,CMDSEARCH_FLOAT);
	return PPeFillInit(PES);
}

int USEFASTCALL PPeTabChar(PPxEDSTRUCT *PES)
{
	if ( !(PES->flag & PPXEDIT_TABCOMP) ) return 1; // TAB割当て機能無効
	return 0;
}

int USEFASTCALL PPeShiftTab(PPxEDSTRUCT *PES)
{
	if ( PES->flag & PPXEDIT_TEXTEDIT ) return 1;	// マルチなら本来のTAB
	if ( !(PES->flag & PPXEDIT_TABCOMP) ){
		HWND hWnd = PES->hWnd;

		if ( PES->flag & PPXEDIT_LINE_MULTI ) return 1; // 複数行一行編集のとき
		SetFocus(GetNextDlgTabItem(GetParentCaptionWindow(hWnd), hWnd, GW_HWNDPREV));
		return 0;
	}
	if ( (PES->list.hWnd != NULL) && (PES->list.ListWindow == LISTU_FOCUSMAIN) ){
		PES->oldkey2 = 1;
		ListUpDown(PES->hWnd, PES, (PES->list.direction >= 0) ? -1 : 1, 0);
	}
	return 0;
}

int USEFASTCALL PPeTab(PPxEDSTRUCT *PES)
{
	DWORD mode;

	if ( PES->flag & PPXEDIT_TEXTEDIT ) return 1;	// マルチなら本来のTAB
	if ( !(PES->flag & PPXEDIT_TABCOMP) ){
		HWND hWnd = PES->hWnd;

		if ( PES->flag & PPXEDIT_LINE_MULTI ) return 1; // 複数行一行編集のとき
		SetFocus(GetNextDlgTabItem(GetParentCaptionWindow(hWnd), hWnd, GW_HWNDNEXT));
		return 0;
	}
	// 補完
	mode = GetCustDword(T("X_ltab"), 0);
	if ( mode >= 2 ){
		setflag(PES->ED.cmdsearch, CMDSEARCH_FLOAT);
		if ( mode == 3 ){
			setflag(PES->flag, CMDSEARCHI_SELECTPART | PPXEDIT_LISTCOMP);
			if ( X_flst[0] == 0 ) X_flst[0] = 1;
		}else if ( mode == 4 ){
			setflag(PES->ED.cmdsearch, CMDSEARCH_ROMA);
		}
	}else{
		resetflag(PES->ED.cmdsearch, CMDSEARCH_FLOAT);
	}
	return PPeFillInit(PES);
}

int USEFASTCALL DeleteSub(PPxEDSTRUCT *PES,int mode,int key)
{
	TEXTSEL ts;

	if ( SelectEditStrings(PES,&ts,mode) == FALSE ) return 1;
	PushTextStack((key == VK_DELETE) ? (TCHAR)B0 : (TCHAR)0,ts.word);
	CallWindowProc(PES->hOldED,PES->hWnd,WM_KEYDOWN,key,0);
	return 0;
}

int USEFASTCALL PPeBackSpace(PPxEDSTRUCT *PES)
{
	return DeleteSub(PES,TEXTSEL_BACK,VK_BACK);
}

int USEFASTCALL PPeDeleteBackLine(PPxEDSTRUCT *PES)
{
	return DeleteSub(PES,TEXTSEL_BEFORE,VK_BACK);
}

int USEFASTCALL PPeDeleteAfterLine(PPxEDSTRUCT *PES)
{
	return DeleteSub(PES,TEXTSEL_AFTER,VK_DELETE);
}

int USEFASTCALL PPeDelete(PPxEDSTRUCT *PES)
{
	int result = DeleteSub(PES,TEXTSEL_CHAR,VK_DELETE);

	if ( !( PES->flag & PPXEDIT_TEXTEDIT ) ){
		// X_flst[0]=3,4の自動補完機能付きの一覧表示
		if ( (PES->list.mode >= LIST_FILL) ||
			 ((X_flst[0] >= 3) && !(PES->flag & PPXEDIT_NOINCLIST) &&
				  (PES->list.hWnd == NULL)) ){
			KeyStepFill(PES, FALSE);
		}
		// Combo box list のインクリメンタルサーチ
		if ( PES->style & WS_VSCROLL ){
			int len;

			len = SendMessage(PES->hWnd,WM_GETTEXTLENGTH,0,0);
			ListSearch(PES->hWnd, PES, len);
		}
	}
	return result;
}

int USEFASTCALL PPeGetFileName(PPxEDSTRUCT *PES)
{
	TCHAR buf[VFPS],path[VFPS];
	PPXCMDENUMSTRUCT work;
	HMODULE hCOMDLG32;
	impGetOpenFileName DGetOpenFileName;
	HWND hWnd = PES->hWnd;
	OPENFILENAME ofile = {sizeof(ofile),NULL,NULL,GetFileExtsStr,NULL,0,0,
		NULL,VFPS,NULL,0,NULL,NULL,OFN_HIDEREADONLY | OFN_SHAREAWARE,
		0,0,WildCard_All,0,NULL,NULL OPENFILEEXTDEFINE };

	ofile.lpstrTitle = MessageText(MES_TSFN);
	PPxEnumInfoFunc(PES->info,'1',path,&work);
	hCOMDLG32 = LoadLibrary(COMDLG32);
	if ( hCOMDLG32 == NULL ) return 0;
	GETDLLPROCT(hCOMDLG32,GetOpenFileName);
	if ( DGetOpenFileName == NULL ) return 0;
	tstrcpy(buf,NilStr);  // 例えば、d:\\winnt\\*.*
	ofile.hwndOwner = hWnd;
	ofile.lpstrFile = buf;
	ofile.lpstrInitialDir = path;
	if ( !DGetOpenFileName(&ofile) ){
		FreeLibrary(hCOMDLG32);
		return 0;
	}
	FreeLibrary(hCOMDLG32);
	if ( PES->flag & PPXEDIT_SINGLEREF ){
		SendMessage(hWnd,EM_SETSEL,0,EC_LAST);
	}else{
		DWORD wP,lP;

		SendMessage(hWnd,EM_GETSEL,(WPARAM)&wP,(LPARAM)&lP);
		if ( tstrchr(buf,' ') != NULL ){
			SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)T("\"\""));
			SendMessage(hWnd,EM_SETSEL,wP + 1,wP + 1);
			PostMessage(hWnd,WM_KEYDOWN,VK_RIGHT,0);
		}
	}
	SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)buf);
	return 0;
}

int USEFASTCALL PPeDuplicate(PPxEDSTRUCT *PES)
{
	HANDLE hEdMem;
	TCHAR *EdMem,temp[0x7ffe];

	hEdMem = (HANDLE)SendMessage(PES->hWnd,EM_GETHANDLE,0,0);
	if ( hEdMem != NULL ){
		EdMem = LocalLock(hEdMem);
	}else{
		EdMem = temp;
		GetWindowText(PES->hWnd,temp,TSIZEOF(temp));
	}
	PPEui(PES->hWnd,T("DupText"),EdMem);
	if ( hEdMem != NULL ) LocalUnlock(hEdMem);
	return 0;
}

			// ^[K][Z] 変換 ----------------------
int USEFASTCALL PPeConvertZenHan(PPxEDSTRUCT *PES)
{
	PPeConvertZenHanMain(PES,PES->hWnd,-1);
	return 0;
}

//----------------------------------------------- ^Q-[U] 大小変換
int USEFASTCALL PPeConvertCase(PPxEDSTRUCT *PES)
{
	TEXTSEL ts;
	TCHAR *q;
	int f = 0;

	if ( SelectEditStrings(PES,&ts,TEXTSEL_WORD) == FALSE ) return 0;
	for ( q = ts.word ; *q ; q++ ){
		if (Isalpha(*q)){
			f = Isupper(*q) ? 1 : 2;
			break;
		}
#ifndef UNICODE
		if ( IskanjiA(*q) ){
			if ( (BYTE)*q == 0x82 ){
				BYTE c;
				c = (BYTE)*(q + 1);
				if ( (c >= 0x60) && (c <= 0x79) ){
					f = 1;
					break;
				}else if ( (c >= 0x81) && (c <= 0x9A) ){
					f = 2;
					break;
				}
			}
			q++;
		}
#endif
	}
	if ( f == 1 ) Strlwr(ts.word);
	if ( f == 2 ) Strupr(ts.word);
	SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)ts.word);
	SendMessage(PES->hWnd,EM_SETSEL,ts.cursororg.start,ts.cursororg.end);
	return 0;
}

int USEFASTCALL PPeDeleteLine(PPxEDSTRUCT *PES)
{
	HWND hWnd = PES->hWnd;

	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		DWORD line,wP,lP;

#ifndef UNICODE
		if ( xpbug < 0 ) return 0;
#endif
		line = SendMessage(hWnd,EM_LINEFROMCHAR,(WPARAM)-1,0);
		wP = SendMessage(hWnd,EM_LINEINDEX,(WPARAM)line,0);
		lP = SendMessage(hWnd,EM_LINEINDEX,(WPARAM)line + 1,0);
		SendMessage(hWnd,EM_SETSEL,wP,lP);
	}else{
		SendMessage(hWnd,EM_SETSEL,0,EC_LAST);
	}
	return DeleteSub(PES,TEXTSEL_CHAR,VK_DELETE);
}

int USEFASTCALL PPeUndoChar(PPxEDSTRUCT *PES)
{
	TCHAR mode,buf[0x1000];
	DWORD wP,lP;
	HWND hWnd = PES->hWnd;

	PopTextStack(&mode,buf);
	if ( buf[0] == '\0' ) return 0;

	SendMessage(hWnd,EM_GETSEL,(WPARAM)&wP,(LPARAM)&lP);
	SendMessage(hWnd,EM_SETSEL,wP,wP);
	SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)buf);
	if ( !(mode & B0) ){
		lP = tstrlen32(buf);
#ifndef UNICODE
		if ( xpbug < 0 ) CaretFixToW(buf,&lP);
#endif
		wP += lP;
	}
	SendMessage(hWnd,EM_SETSEL,wP,wP);
	return 0;
}

int USEFASTCALL PPeSelectExtension(PPxEDSTRUCT *PES)
{
	ECURSOR cursor,cursorRange;
	TCHAR *p;
	TCHAR buf[0x800];
	DWORD extoffset;
	HWND hWnd = PES->hWnd;

												// サイズチェック
	buf[0] = '\0';

	if ( (DWORD)SendMessage(hWnd,WM_GETTEXT,TSIZEOF(buf),(LPARAM)&buf) >=
			(TSIZEOF(buf) - 2) ){
		return 0;
	}
	if ( buf[0] == '\0' ) return 0;
												// 編集文字列(全て)を取得
	GetEditSel(hWnd,buf,&cursor);
	if ( PES->flag & PPXEDIT_SINGLEREF ){	// １項目のみ
		cursorRange.start = 0;
		cursorRange.end = tstrlen(buf);
	}else{	// 複数項目
		cursorRange = cursor;
		GetWordStrings(buf,&cursorRange);
	}
	// 拡張子の位置を確定
	p = FindLastEntryPoint(buf + cursorRange.start);
	cursorRange.start = p - buf;
	p += FindExtSeparator(p);
	extoffset = p - buf;

	if ( cursor.start == cursor.end ){ // 未選択→名前指定
		cursor.start = cursorRange.start;
		cursor.end = extoffset;
	}else if ( cursor.start == cursorRange.start ){	// 全範囲指定 or 名前指定
		if ( cursor.end > extoffset ){ // 全範囲指定→名前指定
			cursor.end = extoffset;
		}else{	// 名前指定→拡張子指定
			if ( buf[extoffset] == '.' ) extoffset++;
			cursor.start = extoffset;
			cursor.end = cursorRange.end;
		}
	}else{ // その他→全範囲指定
		cursor.start = cursorRange.start;
		cursor.end = cursorRange.end;
	}
	SetEditSel(hWnd,buf,cursor.start,cursor.end);
	return 0;
}

int USEFASTCALL PPeDuplicateLine(PPxEDSTRUCT *PES)
{
	DWORD line,wP,lP,index;
	TCHAR buf[0x6000];
	DWORD len;
	HWND hWnd = PES->hWnd;

	SendMessage(hWnd,EM_GETSEL,(WPARAM)&wP,(LPARAM)&lP);

	*(WORD *)buf = (WORD)TSIZEOF(buf);
	line = SendMessage(hWnd,EM_LINEFROMCHAR,(WPARAM)-1,0);
	len = SendMessage(hWnd,EM_GETLINE,(WPARAM)line,(LPARAM)buf);

	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		buf[len++] = '\r';
		buf[len++] = '\n';
	}
	buf[len] = '\0';

	index = SendMessage(hWnd,EM_LINEINDEX,(WPARAM)line + 1,0);
	SendMessage(hWnd,EM_SETSEL,index,index);
	SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)buf);
	SendMessage(hWnd,EM_SETSEL,wP,lP);
	return 0;
}

int USEFASTCALL PPeGetFullPath(PPxEDSTRUCT *PES)
{
	PPXCMD_F fbuf;
	PPXCMDENUMSTRUCT work;

	PPxEnumInfoFunc(PES->info,PPXCMDID_STARTENUM,fbuf.dest,&work);
	fbuf.source = FullPathMacroStr;
	fbuf.dest[0] = '\0';
	Get_F_MacroData(PES->info,&fbuf,&work);
	SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)fbuf.dest);
	PPxEnumInfoFunc(PES->info,PPXCMDID_ENDENUM,fbuf.dest,&work);
	return 0;
}

int USEFASTCALL PPeUnSelect(PPxEDSTRUCT *PES)
{
	DWORD lPos,rPos,nPos,tPos;
	HWND hWnd = PES->hWnd;
	BYTE states[256];
	int oldshift;

	SendMessage(hWnd,EM_GETSEL,(WPARAM)&lPos,(LPARAM)&rPos);
	if ( lPos == rPos ) return 0; // 選択していない
	SendMessage(hWnd,WM_SETREDRAW,FALSE,0);

	oldshift = GetAsyncKeyState(VK_SHIFT);
	if ( !(oldshift & KEYSTATE_PUSH) ){
		GetKeyboardState(states);
		states[VK_SHIFT] |= B7;
		SetKeyboardState(states);
	}
	CallWindowProc(PES->hOldED,hWnd,WM_KEYDOWN,VK_RIGHT,0); // →で選択のどちらの端が変わるかを調べる
	if ( !(oldshift & KEYSTATE_PUSH) ){
		states[VK_SHIFT] &= (BYTE)~B7;
		SetKeyboardState(states);
	}

	SendMessage(hWnd,EM_GETSEL,(WPARAM)&nPos,(LPARAM)&tPos);
	if ( nPos > lPos ){ // 左側が動いた→カーソルは左
		rPos = lPos;
	} // そうでなければカーソルは右
	SendMessage(hWnd,WM_SETREDRAW,TRUE,0);
	SendMessage(hWnd,EM_SETSEL,(WPARAM)rPos,(LPARAM)rPos);
	return 0;
}

int USEFASTCALL PPeDefaultMenu(PPxEDSTRUCT *PES)
{
	SendMessage(PES->hWnd,WM_RBUTTONDOWN,MK_SHIFT,0);
	SendMessage(PES->hWnd,WM_RBUTTONUP,MK_SHIFT,0);
	return 0;
}

const KEYCOMMANDS ppecommands[] = {
	{K_c | K_s | 'A',	PPeUnSelect},

	{K_c | 'O',			PPeOpenFileCmd},
	{K_c | 'S',			PPeSaveFile},
	{K_c | 'U',			PPeUndoChar},
	{K_c | 'V',			PPePaste},

	{K_s | K_c | 'F',	PPeGetFileName},
	{K_s | K_c | 'L',	PPePPcList},

	{KE_ea,				PPeAppendFile},
	{KE_ei,				PPeInsertFile},
	{KE_ec,				PPeCloseFile},
	{KE_ed,				PPeDuplicate},

	{KE_kd,				PPeDuplicateLine},
	{KE_kz,				PPeConvertZenHan},

	{KE_qu,				PPeConvertCase},

	{KE_defmenu,		PPeDefaultMenu},

	{K_tab,				PPeTab},
	{K_s | K_tab,		PPeShiftTab},
	{K_ins,				PPeFill1},
//	{K_s | ' ',			PPeFill},	0.35 廃止
	{'\t',				PPeTabChar},
	{K_s | '\t',		PPeTabChar},

	{K_bs,				PPeBackSpace},
	{K_del,				PPeDelete},
	{K_c | K_del,		PPeDeleteAfterLine},
	{K_s | K_del,		PPeDeleteAfterLine},
	{K_s | K_bs,		PPeDeleteBackLine},
	{K_c | 0x7f,		PPeDeleteBackLine},
	{K_c | 'Y',			PPeDeleteLine},

	{K_c | 'P',			PPeGetFullPath},
	{K_a | 'P',			PPeGetFullPath},

	{K_F2,				PPeSelectExtension},
	{K_F12,				PPeSaveAsFile},
	{K_c | K_s | 'S',	PPeSaveAsFile},
	{0,NULL}
};

void XEditSetModify(PPxEDSTRUCT *PES)
{
	SendMessage(PES->hWnd, EM_SETMODIFY, TRUE, 0);
	SendMessage(GetParent(PES->hWnd), WM_COMMAND, TMAKEWPARAM(0,EN_UPDATE), 0);
}

void XEditClearModify(PPxEDSTRUCT *PES)
{
	SendMessage(PES->hWnd, EM_SETMODIFY, FALSE, 0);
	SendMessage(GetParent(PES->hWnd), WM_PPXCOMMAND, KE_clearmodify, 0);
}

void InsertTime(PPxEDSTRUCT *PES,WORD key)
{
	TCHAR buf[64];
	SYSTEMTIME nowTime;

	GetLocalTime(&nowTime);
	if ( (key == K_F5) && !(PES->flag & PPXEDIT_TEXTEDIT) ){
		wsprintf(buf,T("%04d-%02d-%02d"),nowTime.wYear,nowTime.wMonth,nowTime.wDay);
	}else{
		wsprintf(buf,T("%04d-%02d-%02d %02d:%02d:%02d"),nowTime.wYear,nowTime.wMonth,nowTime.wDay,nowTime.wHour,nowTime.wMinute,nowTime.wSecond);
	}
	SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)buf);
}

int GetWordStrings(TCHAR *line,ECURSOR *cursor)
{
	int bcnt,bk C4701CHECK;
	DWORD tmpb;
	int braket = BRAKET_NONE;
											// 「"」で括っているか判定 --------
											// カーソルまでの「"」の数を求める
	bcnt = 0;
	for ( tmpb = 0 ; tmpb <= cursor->start ; tmpb++ ){
		if ( line[tmpb] == '\"' ){
						// カーソル(の直前)が「"」で、
										// その次が// 空白の場合は、先頭か、
										// 末尾かの判断を行う
			if ( ( (tmpb + 1) >= cursor->start) &&
				 ((UTCHAR)line[tmpb + 1] <= (UTCHAR)' ') ){
				if ( bcnt & 1 ) break;	// 奇数→末尾括り→カウントせず
			}
			bk = tmpb;
			bcnt++;
		}
	}
	if ( bcnt & 1 ){	// 奇数→括りあり→前後の括りを検出
		TCHAR *p;

		cursor->start = bk + 1; // C4701ok
		p = tstrchr(line + cursor->start,'\"');
		if ( p == NULL ){
			cursor->end = cursor->start + tstrlen(line + cursor->start);
			braket = BRAKET_LEFT;
		}else{
			cursor->end = p - line;
			braket = BRAKET_LEFTRIGHT;
		}
	}else{				// 偶数→括り無し→前後の空白を検出
		while ( cursor->start &&
				((UTCHAR)line[cursor->start - 1] > (UTCHAR)' ') ){
			(cursor->start)--;
		}
		while ((UTCHAR)line[cursor->end] > (UTCHAR)' ') (cursor->end)++;
	}
	line[cursor->end] = '\0';
	return braket;
}

void PPeSetCharCode(PPxEDSTRUCT *PES)
{
	POINT pos;
	HMENU hMenu;
	int index,oof = charmenu_other;
	PPXINMENU menus[charmenu_items];
	TCHAR otherstr[20];
	UINT cp;

	InitEditCharCode(PES);

	GetPPePopupPositon(PES, &pos);
	memcpy(menus, charmenu, sizeof(charmenu));
	if ( GetACP() != CP__SJIS ){
		menus[charmenu_sjis].key = CP__SJIS;
		menus[oof].key = VTYPE_SYSTEMCP;
		menus[oof].str = charmenustr_lcp;
		oof++;
	}
	cp = (PES->CharCode < VTypeToCPlist_max) ? VTypeToCPlist[PES->CharCode] : PES->CharCode;
	wsprintf(otherstr,T("codepage %d..."),cp);
	menus[oof].key = VTYPE_OTHER;
	menus[oof].str = otherstr;

	hMenu = MakePopupMenus(menus,PES->CharCode);
	index = TrackPopupMenu(hMenu,TPM_TDEFAULT,pos.x,pos.y,0,PES->hWnd,NULL);
	DestroyMenu(hMenu);
	if ( index != 0 ){
		if ( index == VTYPE_OTHER ){
			const TCHAR *ptr;

			wsprintf(otherstr,T("%u"),cp);
			if ( tInput(PES->hWnd,T("codepage"),otherstr,10,PPXH_NUMBER,PPXH_NUMBER) <= 0 ){
				return;
			}
			ptr = otherstr;
			index = GetIntNumber(&ptr);
			if ( index == 0 ) return;
		}
		PES->CharCode = index;
		if ( (PES->filename[0] != '\0') && (SendMessage(PES->hWnd,EM_GETMODIFY,0,0) == FALSE) && (PMessageBox(PES->hWnd,StrReload,T("PPe"),MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1) == IDYES) ){ // 指定した文字コードで再読み込み
			DWORD memsize;
			TCHAR *textimage = NULL;

			if ( NO_ERROR == LoadFileImage(PES->filename,0x40,(char **)&textimage,&memsize,NULL) ){
				SendMessage(PES->hWnd,WM_SETREDRAW,FALSE,0);
				OpenMainFromMem(PES, PPE_OPEN_MODE_OPEN, NULL, textimage, memsize, PES->CharCode);
				HeapFree(ProcHeap, 0, textimage);
				SendMessage(PES->hWnd,WM_SETREDRAW,TRUE,0);
				InvalidateRect(PES->hWnd,NULL,TRUE);
			}
		}else{
			XEditSetModify(PES);
		}
	}
}

void PPeSetReturnCode(PPxEDSTRUCT *PES)
{
	POINT pos;
	HMENU hMenu;
	int index;

	GetPPePopupPositon(PES,&pos);
	hMenu = MakePopupMenus(returnmenu,PES->CrCode + 1);
	index = TrackPopupMenu(hMenu,TPM_TDEFAULT,pos.x,pos.y,0,PES->hWnd,NULL);
	DestroyMenu(hMenu);
	if ( index ){
		PES->CrCode = index - 1;
		XEditSetModify(PES);
	}
}

void PPeSetTab(PPxEDSTRUCT *PES,int usetab)
{
	int count = 0,tab = 8;
	TCHAR keyword[VFPS],param[VFPS];

	if ( usetab > 0 ){
		tab = usetab;
	}else if ( usetab < 0 ){
		while( EnumCustTable(count++,T("XV_tab"),keyword,param,sizeof(param)) >= 0){
			FN_REGEXP fn;
			const TCHAR *p;
			int ctab;

			p = keyword;
			ctab = GetIntNumber(&p);
			MakeFN_REGEXP(&fn,param);
			if ( FilenameRegularExpression(VFSFindLastEntry(PES->filename),&fn) ){
				tab = ctab;
				FreeFN_REGEXP(&fn);
				break;
			}
			FreeFN_REGEXP(&fn);
		}
	}else{ // == 0
		const TCHAR *ptr;

		wsprintf(param,T("%u"),PES->tab);
		if ( tInput(PES->hWnd,MES_TTAB,param,10,PPXH_NUMBER,PPXH_NUMBER) <= 0 ){
			return;
		}
		ptr = param;
		tab = GetIntNumber(&ptr);
		if ( tab <= 0 ) return;
	}
	PES->tab = tab;
	tab <<= 2 ; // ダイアログボックス単位に変換(x4)
	SendMessage(PES->hWnd,EM_SETTABSTOPS,1,(LPARAM)&tab);
	InvalidateRect(PES->hWnd,NULL,FALSE);
}

void PPeGetString(PPxEDSTRUCT *PES,TCHAR cmd)
{
	PPXCMDENUMSTRUCT work;
	TCHAR buf[VFPS];

	PPxEnumInfoFunc(PES->info,PPXCMDID_STARTENUM,buf,&work);
	PPxEnumInfoFunc(PES->info,cmd,buf,&work);
	SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)buf);
	PPxEnumInfoFunc(PES->info,PPXCMDID_ENDENUM,buf,&work);
}

void CloseUpperList(PPxEDSTRUCT *PES)
{
	if ( PES->list.hWnd == NULL ) return;
	if ( PES->list.direction > 0 ) return; // 下向きなので無視
	if ( SendMessage(PES->list.hWnd,LB_GETCURSEL,0,0) >= 0 ) return; // 選択中
	PostMessage(PES->list.hWnd,WM_CLOSE,0,0);
}

int CloseLineList(PPxEDSTRUCT *PES)
{
	int result = CLOSELIST_NONE;

	if ( PES->list.hSubWnd != NULL ){
		result = (PES->list.mode == LIST_MANUAL) ?
				CLOSELIST_MANUALLIST : CLOSELIST_AUTOLIST;
		DestroyWindow(PES->list.hSubWnd);
	}
	if ( PES->list.hWnd != NULL ){
		result = (PES->list.mode == LIST_MANUAL) ?
				CLOSELIST_MANUALLIST : CLOSELIST_AUTOLIST;
		DestroyWindow(PES->list.hWnd);
	}
	return result;
}
void USEFASTCALL EnterFix(PPxEDSTRUCT *PES)
{
	TCHAR buf[CMDLINESIZE];

	GetWindowText(PES->hWnd,buf,TSIZEOF(buf));
	WriteHistory(PES->list.WhistID,buf,0,NULL);
	CloseLineList(PES);
}

/*-----------------------------------------------------------------------------
	拡張エディットボックスコマンド処理部
-----------------------------------------------------------------------------*/
ERRORCODE PPedExtCommand(PPxEDSTRUCT *PES,PPECOMMANDPARAM *param,const TCHAR *command)
{
	const WORD *ptr;

	if ( param->key == K_cr ) EnterFix(PES);
	// コマンド実行
	if ( (UTCHAR)command[0] == EXTCMD_CMD ){
		return EditExtractMacro(PES,command + 1,NULL,0);
	}

	// キー実行
	ptr = (WORD *)(((UTCHAR)command[0] == EXTCMD_KEY) ? (command + 1) : command);
	param->key = *ptr;
	if ( param->key == 0 ) return NO_ERROR;

	for( ; *(++ptr) ; param->key = *ptr ){ // 最後以外のキーを実行
		if ( PES->AKey != NULL ){
			if ( SendMessage(GetParent(PES->hWnd),WM_PPXCOMMAND,param->key,0) == ERROR_SEEK ){
				continue;
			}
		}
		EdPPxWmCommand(PES,PES->hWnd,param->key,0);
//		PPedCommand(PES,param);
	}
	// 最後のキーを実行
	if ( PES->AKey != NULL ){
		if ( SendMessage(GetParent(PES->hWnd),WM_PPXCOMMAND,param->key,0) == ERROR_SEEK ){
			return NO_ERROR;
		}
	}
	EdPPxWmCommand(PES,PES->hWnd,param->key,0);
	return NO_ERROR;
}

BOOL USEFASTCALL CtrlESCFix(PPxEDSTRUCT *PES)
{
	MSG msg;
	HWND hParentWnd = GetParent(PES->hWnd);

	if ( hParentWnd == NULL ) return FALSE;
	if ( PES->flag & PPXEDIT_WANTENTER ){
		PostMessage(hParentWnd,WM_COMMAND,TMAKELPARAM(0,27),(LPARAM)PES->hWnd);
		return TRUE;
	}
	// ダイアログで ESC を押したあとに WM_CHAR が来るケース対応
	if ( PeekMessage(&msg,hParentWnd,WM_CLOSE,WM_CLOSE,PM_NOREMOVE) ){
		return TRUE; // 終了しかけなのでメニュー表示しない
	}
	return FALSE;
}

// 未実行=ERROR_INVALID_FUNCTION ,実行有り=NO_ERROR
ERRORCODE PPedCommand(PPxEDSTRUCT *PES, PPECOMMANDPARAM *param)
{
	TCHAR buf[CMDLINESIZE];

	PES->oldkey = PES->oldkey2;
	if ( !(param->key & K_raw) ){
		PutKeyCode(buf, param->key);
		if ( (PES->AKey != NULL) && (NO_ERROR == GetCustTable(PES->AKey, buf, buf, sizeof(buf))) ){
			return PPedExtCommand(PES, param, buf);
		}else if ( NO_ERROR == GetCustTable(T("K_edit"), buf, buf, sizeof(buf)) ){
			return PPedExtCommand(PES, param, buf);
		}
	}
	PES->oldkey2 = 0;
	resetflag(param->key, K_raw);	// エイリアスビットを無効にする
	{
		const KEYCOMMANDS *cms = ppecommands;

		while ( cms->key ){
			if ( cms->key == param->key ) return cms->func(PES);
			cms++;
		}
	}

	switch (param->key) {
case K_c | 'M':			// ^[M]
	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)T("\r\n"));
	}else{
		PostMessage(GetParentCaptionWindow(PES->hWnd),WM_COMMAND,TMAKELPARAM(IDOK,BN_CLICKED),0);
	}
	break;
//-----------------------------------------------
case K_c | 'W':			// ^[W]
	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		PPeCloseFile(PES);
		break;
	}else{
		return ERROR_INVALID_FUNCTION;
	}
//-----------------------------------------------
case K_c | K_s | 'N':			// ^\[N]
	EditExtractMacro(PES,T("*ppe"),NULL,0);
	break;
//-----------------------------------------------
case K_c | 'N':			// ^[N]
case K_a | 'C':			// &[C]
	PPeGetString(PES,'C');
	break;
//-----------------------------------------------
case K_c | 'E':			// ^[E]
case K_a | 'X':			// &[X]
	PPeGetString(PES,'X');
	break;
//-----------------------------------------------
case K_c | 'T':			// ^[T]
case K_a | 'T':			// &[T]
	PPeGetString(PES,'T');
	break;
//-----------------------------------------------
case K_c | 'R':			// ^[R]
case K_a | 'R':			// &[R]
	PPeGetString(PES,'R');
	break;
//-----------------------------------------------
case K_s | K_c | 'P':	// ^\[P]
	EditExtractMacro(PES,T("*replace %M_pjump"),NULL,0);
	break;
//------------------------------------------------------
case K_c | K_v | '0':	// ^[0]
case K_c | '0':			// ^[0]
case K_a | '0':			// &[0]
	PPeGetString(PES,'0');
	break;
//-----------------------------------------------
case K_c | K_v | '1':	// ^[1]
case K_c | '1':			// ^[1]
case K_a | '1':			// &[1]
case K_a | 'Q':			// &[Q]
	PPeGetString(PES,'1');
	break;
//-----------------------------------------------
case K_c | K_v | '2':	// ^[2]
case K_c | '2':			// ^[2]
case K_a | '2':			// &[2]
case K_a | 'W':			// &[W]
	PPeGetString(PES,'2');
	break;
//-----------------------------------------------
case K_s | K_c | 'D':	// ^\[D] ディレクトリ
	PPeTreeWindow(PES);
	break;
//-----------------------------------------------
case K_s | K_c | 'I':	// ^\[I] ファイル取得
	param->key = (WORD)((PES->flag & PPXEDIT_REFTREE) ?
			(K_raw | K_s | K_c | 'D') : (K_raw | K_s | K_c | 'F'));
	PPedCommand(PES,param);
	break;
//-----------------------------------------------
case K_c | 'A':			// ^[A] すべてを選択
	SendMessage(PES->hWnd,EM_SETSEL,0,EC_LAST);
	break;
//-----------------------------------------------
case K_s | K_up:		// \[↑]
	if ( !(PES->flag & PPXEDIT_NOINCLIST) ) return ERROR_INVALID_FUNCTION;
	// K_up へ
case K_up:				// [↑]
	PES->oldkey2 = 1;
	if ( ListUpDown(PES->hWnd,PES,-1,param->repeat) == FALSE ) return ERROR_INVALID_FUNCTION;
	break;
//-----------------------------------------------
case K_s | K_dw:		// \[↓]
	if ( !(PES->flag & PPXEDIT_NOINCLIST) ) return ERROR_INVALID_FUNCTION;
	// K_dw へ
case K_dw:				// [↓]
	PES->oldkey2 = 1;
	if ( ListUpDown(PES->hWnd,PES,1,param->repeat) == FALSE ) return ERROR_INVALID_FUNCTION;
	break;
//-----------------------------------------------
case K_F3:
	SearchStr(PES,1);
	break;
case K_s | K_F3:
	SearchStr(PES,-1);
	break;
case K_F5:
case K_s | K_F5:
	InsertTime(PES,param->key);
	break;
case K_F6:
case K_c | 'F':
	SearchStr(PES,0);
	break;
case K_F7:
	PPeReplaceStr(PES);
	break;
case K_s | K_F7:
	if ( PES->findrep == NULL ) break;
	SendMessage(PES->hWnd,EM_REPLACESEL,0,(LPARAM)
		((PES->findrep->replacetext[0] != '\0') ? PES->findrep->replacetext : PES->findrep->findtext));
	break;
//-----------------------------------------------
case K_s | K_Pup:		// \[PgUp]
case K_Pup:				// [PgUp]
	return ListPageUpDown(PES, -1);
//-----------------------------------------------
case K_s | K_Pdw:		// \[PgDw]
case K_Pdw:				// [PgDw]
	return ListPageUpDown(PES, 1);
//-----------------------------------------------
case K_a | K_up:			// &[↑]
	MoveWindowByKey(GetParentCaptionWindow(PES->hWnd), 0, -1);
	break;
case K_a | K_dw:			// &[↓]
	MoveWindowByKey(GetParentCaptionWindow(PES->hWnd), 0, 1);
	break;
case K_a | K_lf:			// &[←]
	MoveWindowByKey(GetParentCaptionWindow(PES->hWnd), -1, 0);
	break;
case K_a | K_ri:			// &[→]
	MoveWindowByKey(GetParentCaptionWindow(PES->hWnd), 1, 0);
	break;
case K_a | K_s | K_lf:			// &\[←]
	WideWindowByKey(PES, -1);
	break;
case K_a | K_s | K_ri:			// &\[→]
	WideWindowByKey(PES, 1);
	break;
//-----------------------------------------------
case K_s | K_esc:	// \[ESC]
	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		HWND hPWnd;

		hPWnd = GetParentCaptionWindow(PES->hWnd);
		ShowWindow(hPWnd, SW_MINIMIZE);
		PostMessage(hPWnd, WM_LBUTTONUP,0,MAX32);
		PostMessage(hPWnd, WM_RBUTTONUP,0,MAX32);
	}
	break;
case K_F4:
	if ( PES->flag & PPXEDIT_TEXTEDIT ) return ERROR_INVALID_FUNCTION;
	if ( !(PES->style & WS_VSCROLL) ) return ERROR_INVALID_FUNCTION;
	if ( PES->list.ListWindow != LISTU_NOLIST ){
		CloseLineList(PES);
		break;
	}else{
		if ( X_flst[0] >= 4 ){
			KeyStepFill(PES,TRUE);
		}else{
			FloatList(PES,1);
		}
	}
	return NO_ERROR;
//------------------------------------
/*
case K_esc:			// [ESC]
	if ( CloseLineList(PES) != CLOSELIST_NONE ) break;
	return ERROR_INVALID_FUNCTION;	// デフォルト処理を行わせる
*/
case K_c | ']':		// ^[]]
case K_F1:			// [F1]
case '\x1b': /*K_esc だと WM_CHAR で閉じて、メニュー表示できない */	// [ESC]
	if ( CloseLineList(PES) != CLOSELIST_NONE ) break;
	if ( param->key == '\x1b' ){
		if ( CtrlESCFix(PES) ) break;
	}
	PPeMenu(PES,param,escmenu);
	break;
//------------------------------------
case K_s | K_F2:			// \[F2]
	PPeMenu(PES,param,f2menu);
	break;
//------------------------------------
case K_c | 'K':				// ^[K]
	PPeMenu(PES,param,kmenu);
	break;
//------------------------------------
case K_c | 'Q':				// ^[Q]
	PPeMenu(PES,param,qmenu);
	break;
//------------------------------------
case K_apps:
case K_a | ' ':				// &[ ]
	PPeMenu(PES,param,amenu);
	break;
//------------------------------------
case KE_ee:					// ESC-exec
	buf[0] = '\0';
	if ( tInput(PES->hWnd,MES_TSHL,buf,TSIZEOF(buf),
				PPXH_COMMAND,PPXH_COMMAND) > 0 ){
		EditExtractMacro(PES,buf,NULL,0);
	}
	break;
//------------------------------------
case KE_er:					// ESC-Run as admin
	if ( (DWORD)SendMessage(PES->hWnd,WM_GETTEXT,TSIZEOF(buf),(LPARAM)&buf) <
			TSIZEOF(buf) ){
		PPeRunas(PES,buf);
	}
	break;
//------------------------------------
case KE_eq:		// ESC-Quit
//case KE_ex:		// ESC-X CloseAll
	PostMessage(GetParentCaptionWindow(PES->hWnd), WM_CLOSE, 0, 0);
	break;
//-----------------------------------------------
case KE_qj:		// ^Q-J Jump to Line
	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		int line;

		line = SendMessage(PES->hWnd,EM_LINEFROMCHAR,(WPARAM)-1,0);
		wsprintf(buf,T("%u"),line + 1);
		if ( tInput(PES->hWnd,MES_TLNO,buf,10,PPXH_NUMBER,PPXH_NUMBER) > 0 ){
			const TCHAR *p;

			p = buf;
			line = GetIntNumber(&p);
			if ( line != 0 ) JumptoLine(PES->hWnd,line - 1);
		}
	}
	break;
//------------------------------------
case K_home:
	if ( !(PES->flag & PPXEDIT_LINE_MULTI) ) return ERROR_INVALID_FUNCTION;
	// K_c | K_Pup へ
case K_c | K_Pup:	// Top of file
	SendMessage(PES->hWnd,EM_SETSEL,0,0);
	SendMessage(PES->hWnd,EM_SCROLLCARET,0,0);
	break;

//-----------------------------------------------
case K_end:
	if ( !(PES->flag & PPXEDIT_LINE_MULTI) ) return ERROR_INVALID_FUNCTION;
	// K_c | K_Pdw へ
case K_c | K_Pdw:	// End of file
	SendMessage(PES->hWnd,EM_SETSEL,EC_LAST,EC_LAST);
	SendMessage(PES->hWnd,EM_SCROLLCARET,0,0);
	break;

//-----------------------------------------------
case K_c | K_up:{	// Top of window
	int line;

	if ( PES->style & ES_MULTILINE ){
		line = SendMessage(PES->hWnd,EM_GETFIRSTVISIBLELINE,0,0);
		JumptoLine(PES->hWnd,line);
	}
	break;
}
//-----------------------------------------------
//case K_c | K_dw:	// End of window
//	SendMessage(hWnd,EM_SETSEL,EC_LAST,EC_LAST);
//	SendMessage(hWnd,EM_SCROLLCARET,0,0);
//	break;

case KE_2t:	// tabstop
	PPeSetTab(PES,0);
	break;

case KE_2r:	// Return code
	PPeSetReturnCode(PES);
	break;
/*
case KE_2p:	// Word warp ... ES_AUTOHSCROLL は作成時のみ設定可能
	SetWindowLong(PES->hWnd,GWL_STYLE,GetWindowLong(PES->hWnd,GWL_STYLE) ^ (ES_AUTOHSCROLL | WS_HSCROLL) );
	break;
*/
case KE_2c:	// Char code
	PPeSetCharCode(PES);
	break;

case KE_qv:	// Q-View mode
	SendMessage(PES->hWnd,EM_SETREADONLY,
			(PES->style & ES_READONLY) ? FALSE : TRUE,0);
	PES->style = GetWindowLong(PES->hWnd,GWL_STYLE);
	break;

case K_c | 'C':
case K_c | 'V':
case K_c | 'X':
case K_c | 'Z':
	CallWindowProc(PES->hOldED,PES->hWnd,WM_CHAR,(param->key & 0xff) - '@',0);
	break;

case K_a | K_del: {
	if ( (DWORD)SendMessage(PES->hWnd,WM_GETTEXT,TSIZEOF(buf),(LPARAM)&buf) <
			TSIZEOF(buf) ){
		if ( DeleteHistory(PES->list.WhistID,buf) ){
			SetWindowText(PES->hWnd,NilStr);
			SetMessageForEdit(PES->hWnd,T("Delete history"));
			if ( PES->list.hWnd != NULL ){
				int index;

				index = SendMessage(PES->list.hWnd,LB_GETCURSEL,0,0);
				if ( index >= 0 ){
					SendMessage(PES->list.hWnd,LB_DELETESTRING,index,0);
					SendMessage(PES->list.hWnd,LB_SETCURSEL,index,0);
				}
			}
		}
	}
	break;
}

//----------------------------------------------- Zoom in
case K_c | K_v | VK_ADD:
case K_c | K_v | VK_OEM_PLUS:
	ChangeFontSize(PES,1);
	break;
//----------------------------------------------- Zoom out
case K_c | K_v | VK_SUBTRACT:
case K_c | K_v | VK_OEM_MINUS:
	ChangeFontSize(PES,-1);
	break;
//----------------------------------------------- Zoom mode change
case K_c | K_v | VK_NUMPAD0:
	ChangeFontSize(PES,-9);
	break;

case K_c | K_s | K_v | VK_ADD:
case K_c | K_s | K_v | VK_OEM_PLUS: // US[=/+] JIS[;/+]
	ChangeOpaqueWindow(PES->hWnd,1);
	break;
case K_c | K_s | K_v | VK_SUBTRACT:
case K_c | K_s | K_v | VK_OEM_MINUS: // US[-/_] JIS[-/=]
	ChangeOpaqueWindow(PES->hWnd,-1);
	break;

case K_cr:		//
	if ( PES->flag & PPXEDIT_WANTENTER ){
		CloseLineList(PES);
		PostMessage(GetParent(PES->hWnd),WM_COMMAND,TMAKELPARAM(0,13),(LPARAM)PES->hWnd);
		break;
	}
	// 再度 enter を入力し、閉じたりさせる(WM_GETDLGCODEを一旦通過しているので、ダイアログを閉じたりすることは現時点ではもうできないため)
	if ( CloseLineList(PES) == CLOSELIST_AUTOLIST ){
		if ( !( PES->flag & PPXEDIT_TEXTEDIT ) ){
			PostMessage(PES->hWnd,WM_KEYDOWN,VK_RETURN,0);
			// WM_CHAR の 13 (Enter) を廃棄
			PeekMessage((MSG *)buf,PES->hWnd,WM_CHAR,WM_CHAR,PM_REMOVE);
		}
		break;
	}
	// default へ
//-----------------------------------------------
default:
	return ERROR_INVALID_FUNCTION; // 何も実行しなかった
//-----------------------------------------------
	}
	return NO_ERROR; // 実行済み
}

// LineMulti 時は、スクロールバーをそのまま借用できないので、WM_NCLBUTTONDOWN で処理する
void LineMulti_VScrollDown(HWND hWnd,int fontY,LPARAM lParam)
{
	POINT pos;

	LPARAMtoPOINT(pos,lParam);
	ScreenToClient(hWnd,&pos);
	EDsHell(hWnd,WM_VSCROLL,( pos.y <= (fontY / 2) ) ? SB_LINEUP : SB_LINEDOWN,0);
}

void SetDropFiles(HWND hWnd,HDROP hDrop)
{
	TCHAR name[VFPS];

	DragQueryFile(hDrop,0,name,TSIZEOF(name));
	DragFinish(hDrop);
	SetWindowText(hWnd,name);
}

// WM_SYSKEYDOWN で英数字キーの処理を行ったとき、後続の WM_SYSCHAR を
// 処理しないため警告音が出る問題に対処する
void SkipAltLetterKeyDef(HWND hWnd)
{
	MSG WndMsg;

	if ( IsTrue(PeekMessage(&WndMsg,hWnd,WM_SYSCHAR,WM_SYSCHAR,PM_NOREMOVE)) ){
		if ( IsalnumA(WndMsg.wParam) ){ // 該当したら WM_SYSCHAR を破棄
			PeekMessage(&WndMsg,hWnd,WM_SYSCHAR,WM_SYSCHAR,PM_REMOVE);
		}
	}
}

void LineExpand(HWND hWnd,PPxEDSTRUCT *PES)
{
	DWORD line;
	WindowExpandInfoStruct eis;

	line = CallWindowProc(PES->hOldED,PES->hWnd,EM_GETLINECOUNT,0,0);
	if ( PES->caretLY == line ) return;

	eis.hParentWnd = GetParent(hWnd);
	GetDesktopRect(hWnd,&eis.boxDesk);
	GetWindowRect(hWnd,&eis.boxEdit);
	GetWindowRect(eis.hParentWnd,&eis.boxDialog);

	eis.delta = (line - PES->caretLY) * PES->fontY;
	eis.boxEdit.bottom += -eis.boxEdit.top + eis.delta;
	if ( eis.boxEdit.bottom < (int)PES->fontY ) return; // １行未満なので中止

	PES->caretLY = line;
	EditBoxExpand(PES, &eis);
	InvalidateRect(eis.hParentWnd,NULL,TRUE);
}

LRESULT CharProc(PPxEDSTRUCT *PES,HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	if ( !(PES->flag & PPXEDIT_TEXTEDIT) ){ // 一行編集特有
		// X_flst[0]=3,4の自動補完機能付きの一覧表示
		if ( ( (wParam != '\t') && (wParam != '\xd') && (wParam != '\x1b')) && // TAB/CR/ESCでない
				((PES->list.mode >= LIST_FILL) ||
				 ((X_flst[0] >= 3) && !(PES->flag & PPXEDIT_NOINCLIST) &&
				  (PES->list.hWnd == NULL)) ) )
		{
			LRESULT lr;

			lr = CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);
			KeyStepFill(PES,FALSE);
			return lr;
		}
		// インクリメンタルサーチ
		if ( PES->style & WS_VSCROLL ){
			int len;
			LRESULT lr;

			if ( wParam == '\t' ) return 1;
			len = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);
			lr = CallWindowProc(PES->hOldED, hWnd, iMsg, wParam, lParam);
			ListSearch(hWnd, PES, len);
			return lr;
		}
	}
	// ※PPXEDIT_LINECSRは、ES_MULTILINE のときしか有効になっていない
	if ( PES->flag & (PPXEDIT_LINECSR | PPXEDIT_LINE_MULTI) ){
		LRESULT lr = CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		if ( PES->flag & PPXEDIT_LINE_MULTI ){
			LineExpand(hWnd,PES);
		}else{
			LineCursor(PES,iMsg);
		}
		return lr;
	}
	return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);
}

/*-----------------------------------------------------------------------------
	拡張エディットボックスメッセージ処理
-----------------------------------------------------------------------------*/
LRESULT CALLBACK EDsHell(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	PPxEDSTRUCT *PES;
	PPECOMMANDPARAM cparam;

	PES = (PPxEDSTRUCT *)GetProp(hWnd,PPxED);
	if ( PES == NULL ) return DefWindowProc(hWnd,iMsg,wParam,lParam);
	switch (iMsg){
		case WM_NCLBUTTONDOWN:
			if ( (PES->flag & PPXEDIT_LINE_MULTI) && (wParam == HTVSCROLL) ){
				LineMulti_VScrollDown(hWnd,PES->fontY,lParam);
				return 0;
			}
			return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		case WM_IME_NOTIFY:
			// ウィンドウの位置が変化したときに通知される→
			// リストウィンドウの位置変更に利用する
			if ( wParam == IMN_SETCOMPOSITIONWINDOW ){
				if ( PES->list.hWnd != NULL ) FixListPosition(PES,hWnd);
			}
			return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		case WM_DROPFILES:		// ドロップファイルの名前取込
			SetDropFiles(hWnd,(HDROP)wParam);
			break;

		case WM_KILLFOCUS:
			if ( PES->FloatBar.hWnd != NULL ) DestroyWindow(PES->FloatBar.hWnd);
		// WM_LBUTTONDOWN へ続く
		case WM_LBUTTONDOWN:
									// ヒストリなどのリストがあるなら閉じる
			if ((PES->list.hWnd != NULL) && ((HWND)wParam != PES->list.hWnd) ){
				PostMessage(PES->list.hWnd,WM_CLOSE,0,0);
			}
			if ((PES->list.hSubWnd != NULL) && ((HWND)wParam != PES->list.hSubWnd) ){
				PostMessage(PES->list.hSubWnd,WM_CLOSE,0,0);
			}
			break;

		case WM_COMMAND:
			if ( HIWORD(wParam) == LBN_SELCHANGE ){	// リスト選択？
				ListSelect(PES,(HWND)lParam);
			}
			break;

		case WM_MENUCHAR:
		case WM_MENUSELECT:
		case WM_MENUDRAG:
		case WM_MENURBUTTONUP:
			return PPxMenuProc(hWnd,iMsg,wParam,lParam);

		case WM_LBUTTONUP:
			if ( PES->FloatBar.hWnd == NULL ){
				InitFloatBar(PES,(int)(short)LOWORD(lParam));
			}
			break;

		case WM_RBUTTONUP:
			// ctrl/shift有りなら元の処理を行う
			if ( (wParam & (MK_SHIFT | MK_CONTROL)) != 0 ) break;
			cparam.key = K_a | ' ';
			cparam.repeat = 0;
			PES->mousepos = TRUE;
			PPedCommand(PES,&cparam);
			return 0;

		case WM_SYSKEYDOWN:
														// Alt 系の使用を禁止
			if ( !(PES->flag & PPXEDIT_USEALT) &&
				 ((WORD)wParam >= '0') &&
				 ((WORD)wParam <= VK_DIVIDE)){
				break;
			}
			// WM_KEYDOWN へ
		case WM_KEYDOWN:
			cparam.key = (WORD)(wParam | GetShiftKey() | K_v);
			cparam.repeat = lParam & B30;
			PES->mousepos = FALSE;

#ifndef WINEGCC
			/* ※ WM_GETDLGCODE の処理で、リストがないと WM_SYSCHAR がこなく、
			 リストがあると WM_SYSCHAR が来るため、調整する */
			if ( (cparam.key & K_a) && IsalnumA(cparam.key) &&
				 !( PES->flag & PPXEDIT_TEXTEDIT ) &&
				 (PES->list.ListWindow == LISTU_NOLIST) ){
				resetflag(cparam.key,K_v);
			}
#endif
			if ( PES->KeyHookEntry != NULL ){
				if ( CallEditKeyHook(PES,cparam.key) != PPXMRESULT_SKIP ){
					return 0;
				}
			}
			if ( PPedCommand(PES,&cparam) != ERROR_INVALID_FUNCTION ){
				if ( (cparam.key & K_a) && (T_CHRTYPE[(unsigned char)
						(cparam.key)] & (T_IS_DIG | T_IS_UPP)) ){
					SkipAltLetterKeyDef(hWnd);
				}
				return 0;
			}
			if ( wParam == VK_MENU ){ // [ALT] はメニュー移行を禁止
				if ( GetCustDword(T("X_alt"),1) ) return 0;
			}
			break;

		#ifdef UNICODE
		case WM_IME_CHAR:
			#ifdef UNICODE
				return CharProc(PES,hWnd,WM_CHAR,wParam,lParam);
			#else // Win9x,WinNTで漢字が入力できないため、休止中
				unsigned char charcode;

				charcode = (BYTE)((wParam >> 8) & 0xff);
				if ( charcode != '\0' ){
					CharProc(PES,hWnd,WM_CHAR,charcode,lParam);
				}
				return CharProc(PES,hWnd,WM_CHAR,wParam & 0xff,lParam);
			#endif
		#endif

		case WM_SYSCHAR:
			if ( !(PES->flag & PPXEDIT_USEALT) ) break;	// Alt 系の使用を禁止
		//	WM_CHAR へ

		case WM_CHAR:
			if ( (WORD)wParam < 0x80 ){
				cparam.key = FixCharKeycode((WORD)wParam);
				cparam.repeat = lParam & B30;
				PES->mousepos = FALSE;

				if ( PES->KeyHookEntry != NULL ){
					if ( CallEditKeyHook(PES,cparam.key) != PPXMRESULT_SKIP ){
						return 0;
					}
				}
				if ( PPedCommand(PES,&cparam) != ERROR_INVALID_FUNCTION ) return 0;
			}
			return CharProc(PES,hWnd,iMsg,wParam,lParam);

		case WM_DESTROY: {
			WNDPROC hOldED;

			CancelListThread(PES);
			if ( PES->list.ListWindow != LISTU_NOLIST ) CloseLineList(PES);
			FreeBackupText(PES);

			SearchFileIned(&PES->ED, NilStrNC, NULL, 0);
			hOldED = PES->hOldED;
			if ( PES->list.filltext_user.mem != NULL ){
				HeapFree(ProcHeap,0,PES->list.filltext_user.mem);
			}
			if ( PES->findrep != NULL ){
				HeapFree(ProcHeap,0,PES->findrep);
			}
			if ( PES->FloatBar.hWnd != NULL ){
				DestroyWindow(PES->FloatBar.hWnd);
			}
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)hOldED );
			RemoveProp(hWnd, PPxED);
			HeapFree(DLLheap, 0, PES);
			return CallWindowProc(hOldED, hWnd, iMsg, wParam, lParam);
		}

		case WM_SETFONT:
			GetHeight(PES,(HFONT)wParam);
			break;

		case WM_GETDLGCODE:
			if ( PES->list.ListWindow != LISTU_NOLIST ) return DLGC_WANTALLKEYS;
			if ( !(PES->flag & PPXEDIT_TABCOMP) ) break;
			return CallWindowProc(PES->hOldED, hWnd, iMsg, wParam, lParam) | DLGC_WANTTAB;

		case WM_VSCROLL:
			if ( PES->flag & PPXEDIT_TEXTEDIT ) break;
			SetFocus(hWnd);
			switch ( GET_WM_HSCROLL_CODE(wParam, lParam) ){
				case SB_LINEUP:
					if ( X_flst[0] >= 4 ){
						if ( PES->list.ListWindow != LISTU_NOLIST ){
							CloseLineList(PES);
						}else{
							KeyStepFill(PES, TRUE);
						}
					}else{
						FloatList(PES, -1);
					}
					break;

				case SB_LINEDOWN:
					FloatList(PES, 1);
					break;

				// default:
			}
			return 0;

		case WM_MOUSEWHEEL:
			if ( wParam & MK_CONTROL ){
				if ( wParam & MK_SHIFT ){
					ChangeOpaqueWindow(hWnd,(wParam & B31) ? -1 : 1);
				}else{
					ChangeFontSize(PES,(wParam & B31) ? -1 : 1);
				}
				return 0;
			}

			if ( PES->flag & PPXEDIT_COMBOBOX ){
				return SendMessage(GetParent(hWnd),WM_MOUSEWHEEL,wParam,lParam);
			}
			if ( PES->list.ListWindow != LISTU_NOLIST ){
				return SendMessage( (PES->list.ListWindow == LISTU_FOCUSSUB) ?
					PES->list.hSubWnd : PES->list.hWnd,
					WM_MOUSEWHEEL,wParam,lParam);
			}
			if ( !( PES->flag & PPXEDIT_TEXTEDIT ) ){
				if ( (short)HIWORD(wParam) >= 0 ){
					cparam.key = K_up;
				}else{
					cparam.key = K_dw;
				}
				cparam.repeat = 0;
				PES->mousepos = TRUE;
				PPedCommand(PES,&cparam);
				return 0;
			}
			break;

		case WM_ENABLE:
			if ( (BOOL)wParam == FALSE ) ClosePPeTreeWindow(PES);
			return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		case WM_GESTURE:
			WMGestureEdit(hWnd,wParam/*,lParam*/);
			return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		default:
			if ( iMsg == WM_PPXCOMMAND ){
				LRESULT result;

				result = EdPPxWmCommand(PES,hWnd,wParam,lParam);
				if ( result != (LRESULT)MAX32 ) return result;
			}else if ( iMsg == ReplaceDialogMessage ){
				PPeReplaceStrCommand(PES,(FINDREPLACE *)lParam);
			}
			break;
	}
	// ※PPXEDIT_LINECSRは、ES_MULTILINE のときしか有効になっていない
	if ( (PES->flag & (PPXEDIT_LINECSR | PPXEDIT_LINE_MULTI)) &&
		// 後、edit message と wm_paint の時に使用する
		 ( ((iMsg >= EM_SETSEL) && (iMsg <= EM_SETREADONLY)) ||
		   (iMsg == WM_PAINT) ||
		   (iMsg == WM_KEYDOWN) ||
		   (iMsg == WM_LBUTTONDOWN) ) ){
		LRESULT lr = CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);

		if ( PES->flag & PPXEDIT_LINECSR ){
			LineCursor(PES,iMsg);
		}else{
			LineExpand(hWnd,PES);
		}
		return lr;
	}else{
		return CallWindowProc(PES->hOldED,hWnd,iMsg,wParam,lParam);
	}
}

// 戻り値が MAX32 のときは、何もしなかった扱いになる。→ラインカーソル処理有り
LRESULT EdPPxWmCommand(PPxEDSTRUCT *PES, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	switch ( LOWORD(wParam) ){
/*
		case K_cr:
			CloseLineList(PES);
			break;
*/
		case KE_insert:
			SendMessage(hWnd,EM_REPLACESEL,0,lParam);
			break;

		case KE_replace:
			SetWindowText(hWnd,(TCHAR *)lParam);
			SendMessage(hWnd,EM_SETSEL,0,EC_LAST);
			break;

		case KE_closecheck:
			if ( SendMessage(hWnd,EM_GETMODIFY,0,0) == FALSE ){
				return 1; // 変更無し→そのまま終了を許可
			}else{
				int result;

				result = PMessageBox(hWnd,MES_QSAV,T("PPe"),
					MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
				if ( result == IDYES ){
					if ( FileSave(PES,EDL_FILEMODE_NODIALOG) ) return 1;
				}else if ( result == IDNO ){
					return 1;
				}
			}
			return 0;

		case KE_openfile:
			return OpenFromFile(PES,PPE_OPEN_MODE_OPEN,(const TCHAR *)lParam);

		case KE_opennewfile:
			return OpenFromFile(PES,PPE_OPEN_MODE_OPENNEW,(const TCHAR *)lParam);
		case KE_excmdopen:
			return OpenFromFile(PES,PPE_OPEN_MODE_EXCMDOPEN,(const TCHAR *)lParam);

		case KE_seltext:
			CmdSelText(PES,(TCHAR *)lParam);
			break;

		case KE_edtext:
			GetWindowText(hWnd,(TCHAR *)lParam,CMDLINESIZE);
			break;

		case KE__FREE: {
			WNDPROC hOldED;

			SearchFileIned(&PES->ED, NilStrNC, NULL, 0);
			hOldED = PES->hOldED;
			RemoveProp(hWnd,PPxED);
			HeapFree(DLLheap,0,PES);
			SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LONG_PTR)hOldED);
			return 0;
		}

		case KE_setkeya:
			PES->AKey = (const TCHAR *)lParam;
			break;

		case KTN_close:
			ClosePPeTreeWindow(PES);
//		case KTN_focus: へ
		case KTN_escape:
		case KTN_focus:
			SetFocus(hWnd);
			break;

		case KTN_select:
		case KTN_selected:
			if ( PES->flag & PPXEDIT_SINGLEREF ){
				SetWindowText(hWnd,(TCHAR *)lParam);
				SendMessage(hWnd,EM_SETSEL,EC_LAST,EC_LAST);
			}else{
				DWORD wP,lP;

				SendMessage(hWnd,EM_GETSEL,(WPARAM)&wP,(LPARAM)&lP);
				lP = tstrlen((TCHAR *)lParam);
#ifndef UNICODE
				if ( xpbug < 0 ) CaretFixToW((TCHAR *)lParam,&lP);
#endif
				if ( tstrchr((TCHAR *)lParam,' ') != NULL ){
					SendMessage(hWnd,EM_REPLACESEL,0,(LPARAM)T("\"\""));
					SendMessage(hWnd,EM_SETSEL,wP + 1,wP + 1);
					lP += 2;
				}
				SendMessage(hWnd,EM_REPLACESEL,0,lParam);
				SendMessage(hWnd,EM_SETSEL,wP,wP + lP);
			}
			if ( LOWORD(wParam) == KTN_selected ){
				ClosePPeTreeWindow(PES);
				SetFocus(hWnd);
			}
			break;

		case KE_closeUList:
			CloseUpperList(PES);
			break;

		default: {
			PPECOMMANDPARAM param;
			ERRORCODE result;

			param.key = (WORD)wParam;
			param.repeat = 0;
			PES->mousepos = FALSE;
			result = PPedCommand(PES,&param);
			if ( (result == ERROR_INVALID_FUNCTION) && ((wParam & (K_v | K_ex | K_internal)) == K_v) ) { // 未実行の仮想キー？
				CallWindowProc(PES->hOldED, hWnd, WM_KEYDOWN, wParam & 0xff, 0);
				CallWindowProc(PES->hOldED, hWnd, WM_KEYUP, wParam & 0xff, B30);
			}
			return result;
		}
	}
	return MAX32;
}

//------------------------------------
void RegisterSHAutoComplete(HWND hRealED)
{
	if ( DSHAutoComplete == NULL ){
		if ( LoadWinAPI("SHLWAPI.DLL",NULL,SHLWAPIDLL,LOADWINAPI_LOAD) == NULL ){
			return;
		}
	}
	DSHAutoComplete(hRealED,SHACF_FILESYSTEM | // SHACF_USETAB | ←これを付けると、W2kでBS/DELの挙動がおかしくなる？
			SHACF_AUTOAPPEND_FORCE_ON | SHACF_AUTOSUGGEST_FORCE_ON);
}

BOOL SetWindowTextWithSelect(HWND hEditWnd,const TCHAR *defstr)
{
	DWORD firstC,lastC;
	TCHAR *firstp = SearchPipe(defstr),*lastp;

	if ( firstp == NULL ){
		SetWindowText(hEditWnd,defstr);
		return FALSE;
	}else{
		TCHAR strbuf[CMDLINESIZE];

		tstrcpy(strbuf,defstr);
		firstp = strbuf + (firstp - defstr);
		memmove( firstp , firstp + 1 , TSTRSIZE(firstp + 1) );

		lastp = SearchPipe(strbuf);
		if ( lastp != NULL ){
			memmove( lastp , lastp + 1 , TSTRSIZE(lastp + 1) );
		}else{
			lastp = firstp;
		}

		SetWindowText(hEditWnd,strbuf);
		firstC = firstp - strbuf;
		lastC = lastp - strbuf;
#ifndef UNICODE
		if ( xpbug == 0 ){
			xpbug = IsWindowUnicode(hEditWnd) ? -1 : 1;
		}
		if ( xpbug < 0 ){
			CaretFixToW(strbuf,(DWORD *)&firstC);
			CaretFixToW(strbuf,(DWORD *)&lastC);
		}
#endif
		SendMessage(hEditWnd,EM_SETSEL,(WPARAM)firstC,(LPARAM)lastC);
		return TRUE;
	}
}

int CALLBACK LineEditWordBreakProcW(LPWSTR lpch,int ichCurrent,int cch,int action)
{
	UnUsedParam(lpch);UnUsedParam(cch);UnUsedParam(action);
	switch ( action ){
		case WB_LEFT:
			if ( (ichCurrent > 0) && (*(lpch + ichCurrent - 1) <= ' ') ){
				for (;;){
					if ( ichCurrent <= 0 ) return 0;
					ichCurrent--;
					if ( *(lpch + ichCurrent) > ' ' ) break;
				}
			}
			for (;;){
				if ( ichCurrent <= 0 ) return 0;
				if ( *(lpch + ichCurrent - 1) <= ' ' ) return ichCurrent;
				ichCurrent--;
			}
		case WB_RIGHT:
			if ( (ichCurrent < cch) && (*(lpch + ichCurrent) > ' ') ){
				for (;;){
					if ( ichCurrent >= cch ) return cch;
					ichCurrent++;
					if ( *(lpch + ichCurrent) <= ' ' ) break;
				}
			}

			for (;;){
				if ( ichCurrent >= cch ) return cch;
				if ( *(lpch + ichCurrent) > ' ' ) return ichCurrent;
				ichCurrent++;
			}
	}

	//	WB_ISDELIMITER
	return TRUE;
}

void InitExEdit(PPxEDSTRUCT *PES)
{
	PES->tab	= 8 * 4;
	PES->CrCode	= VTYPE_CRLF;
	PES->CharCode = 0; // InitEditCharCode() で初期化される

	PES->style = GetWindowLong(PES->hWnd, GWL_STYLE);
	PES->exstyle = GetWindowLong(PES->hWnd, GWL_EXSTYLE);
	PES->list.hWnd = NULL;
	PES->list.hSubWnd = NULL;

	if ( PES->style & ES_MULTILINE ){
		if ( PES->flag & PPXEDIT_LINE_MULTI ){
			PES->caretLY = 1;
			if ( WinType >= WINTYPE_VISTA ){
				SendMessage(PES->hWnd, EM_SETWORDBREAKPROC, 0, (LPARAM)LineEditWordBreakProcW);
			}
		}else{
			setflag(PES->flag,PPXEDIT_TEXTEDIT);
			if ( GetCustDword(T("X_ucsr"),1) ){
				setflag(PES->flag,PPXEDIT_LINECSR);
			}
			if ( (WinType >= WINTYPE_VISTA) && (PES->flag & PPXEDIT_NOWORDBREAK) ){
				SendMessage(PES->hWnd, EM_SETWORDBREAKPROC, 0, (LPARAM)LineEditWordBreakProcW);
			}
		}
	}
	SetProp(PES->hWnd,PPxED,(HANDLE)PES);

	GetCustData(T("X_pmc"),&X_pmc,sizeof(X_pmc));
}

/*-----------------------------------------------------------------------------
	拡張エディットボックスを登録する
-----------------------------------------------------------------------------*/
PPXDLL HWND PPXAPI PPxRegistExEdit(PPXAPPINFO *info,HWND hEditWnd,int maxlen,const TCHAR *defstr,WORD rHist,WORD wHist,int flags)
{
	PPxEDSTRUCT *PES;
	HWND hRealED;
	DWORD X_ltab;

	X_ltab = GetCustDword(T("X_ltab"),0);
	if ( X_ltab ) setflag(flags,PPXEDIT_TABCOMP);
	GetCustData(T("X_flst"),&X_flst,sizeof(X_flst));
	if ( X_flst[0] != 0 ) setflag(flags,PPXEDIT_LISTCOMP);

	if ( !(flags & PPXEDIT_COMBOBOX) ){	// 対象がエディットボックス ===========
		if ( maxlen != 0 ){
			#ifndef UNICODE
			if ( (maxlen > 0x8000) && (WinType == WINTYPE_9x) ){
				SendMessage(hEditWnd,EM_LIMITTEXT,0x8000 - 1,0);
			}else
			#endif
			{
				SendMessage(hEditWnd,EM_LIMITTEXT,maxlen - 1,0);
			}
		}
		if ( (defstr != NULL) && (*defstr != '\0') ){
			if ( flags & PPXEDIT_INSTRSEL ){
				SetWindowTextWithSelect(hEditWnd,defstr);
			}else{
				SetWindowText(hEditWnd,defstr);
				SendMessage(hEditWnd,EM_SETSEL,0,EC_LAST);
			}
		}
		hRealED = hEditWnd;
	}else{								// 対象がコンボボックス ===============
		hRealED = PPxRegistExEditCombo(hEditWnd,maxlen,defstr,rHist,wHist);
	}
#ifndef UNICODE
	if ( xpbug == 0 ){
		xpbug = IsWindowUnicode(hRealED) ? -1 : 1;
	}
#endif
	PES = GetProp(hRealED,PPxED);
	if ( PES == NULL ){					// 作業領域を確保 ---------------------
		PES = HeapAlloc(DLLheap,HEAP_ZERO_MEMORY,sizeof(PPxEDSTRUCT));
		if ( PES == NULL ) return NULL;
		PES->hWnd = hRealED;
//		PES->hTreeWnd = NULL;
//		PES->hOldED = NULL;
//		PES->AKey = NULL;
//		PES->KeyHookEntry = NULL;
//		PES->filename[0] = '\0';
//		PES->findstring[0] = '\0';
		PES->ED.hF = NULL;
		if ( X_ltab == 4 ) PES->ED.cmdsearch = CMDSEARCH_ROMA;
	}	// PES != NULL ... 登録済み→設定の変更のみ行う

										// プロージャを設定 -------------------
	PES->info = PES->ED.info = (info != NULL) ? info : PPxDefInfo;
//	PES->list.index = 0;
	PES->list.WhistID = wHist;
	PES->list.RhistID = rHist;
	PES->list.OldString = NULL;
	PES->list.ListWindow = LISTU_NOLIST;
//	PES->list.filltext_user.filename = NULL;
//	PES->list.filltext_user.mem = NULL;
//	PES->list.filltext_user.text = NULL;

	PES->flag = flags;
	PES->oldkey2 = 0;

	GetHeight(PES,(HFONT)SendMessage(hRealED,WM_GETFONT,0,0));
	InitExEdit(PES);
	if ( PES->hOldED == NULL ){
		PES->hOldED = (WNDPROC)
				SetWindowLongPtr(hRealED,GWLP_WNDPROC,(LONG_PTR)EDsHell);
/*
		if ( PES->hOldED == NULL ){
			xmessage(XM_GrERRld,T("PPxRegistExEdit error"));
		}
*/
		if ( !( PES->flag & PPXEDIT_TEXTEDIT ) && (X_flst[0] == 2) ){
			// SHAutoComplete は相対ディレクトリの補完に対応していないらしい
			RegisterSHAutoComplete(hRealED);
		}
		if ( flags & PPXEDIT_WANTEVENT ){
			if ( IsExistCustTable(T("K_edit"),T("FIRSTEVENT")) ){
				PostMessage(hRealED,WM_PPXCOMMAND,K_E_FIRST,0);
			}
		}
	}
	return hRealED;
}
