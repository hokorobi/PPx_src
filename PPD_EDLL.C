/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library			拡張エディット/tInput
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "PPD_EDL.H"
#include "VFS_STRU.H"
#pragma hdrstop

int X_calc = -1;
DWORD X_esel[3] = {1,1,1};
const TCHAR StrContinuousMenu[] = MES_TREC;
const TCHAR StrK_lied[] = T("K_lied");
const TCHAR StrK_edit[] = T("K_edit");

typedef struct {
	HWND hEdWnd;
	ThSTRUCT ThMenu;
	TINPUT *tinput;
} TINPUTSTRUCT;

void ExecPreCommand(TINPUTSTRUCT *ts)
{
	TCHAR buf[CMDLINESIZE];

	if ( !(ts->tinput->flag & TIEX_EXECPRECMD) ) return;
	buf[0] = '\0';
	ThGetString(ts->tinput->StringVariable,T("Input_FirstCmd"),buf,CMDLINESIZE);
	if ( buf[0] != '\0' ){
		EditExtractMacro((PPxEDSTRUCT *)GetProp(ts->hEdWnd,PPxED),buf,NULL,0);
	}
}

void TinputExtractMacro(TINPUTSTRUCT *ts,const TCHAR *param/*,TCHAR *extract,int flags*/)
{
//	EditExtractMacro((PPxEDSTRUCT *)GetProp(ts->hEdWnd,PPxED),param,extract,flags);
	EditExtractMacro((PPxEDSTRUCT *)GetProp(ts->hEdWnd,PPxED),param,NULL,0);
}

void FixInputbox(HWND hDlg,HWND hEdWnd,TINPUT *tinput)
{
	HWND hRefWnd, hTreeWnd = NULL;
	RECT ncbox, clientbox, refbox;
	int editwidth, editheight, dpi;
	PPxEDSTRUCT *PES;

	PES = (PPxEDSTRUCT *)GetProp(hEdWnd,PPxED);
	if ( (PES != NULL) && (PES->hTreeWnd != NULL) && (PES->flags & PPXEDIT_JOINTTREE) ){
		hTreeWnd = PES->hTreeWnd;
	}

	GetWindowRect(hEdWnd,&ncbox);
	GetClientRect(hEdWnd,&clientbox);
	dpi = GetMonitorDPI(hDlg);

	hRefWnd = GetDlgItem(hDlg,IDB_REF);
	GetWindowRect(hRefWnd,&refbox);
	refbox.bottom -= refbox.top;
	refbox.top -= ncbox.top;
	refbox.right -= refbox.left;
	ncbox.bottom -= ncbox.top;
	editheight = ncbox.bottom;

	GetClientRect(hDlg,&clientbox);
	editwidth = clientbox.right;

	if ( !(tinput->flag & (TIEX_USEREFLINE | TIEX_USEOPTBTN)) ){
		// ボタンは右
		if ( TouchMode & TOUCH_LARGEWIDTH ){
			refbox.right = (dpi * 120) >> 8; // 12.0mm ( 120 / 254 ) の近似値
			editwidth -= /* OK */ refbox.right + /* X */ refbox.bottom;
		}else{
			editwidth -= refbox.right * 3;
		}
	}else{
		// ボタンは下
		if ( TouchMode & TOUCH_LARGEWIDTH ){
			int nw = (dpi * 120) >> 8; // 12.0mm ( 120 / 254 ) の近似値
			if ( refbox.right < nw ) refbox.right = nw;
		}
	}
	if ( editwidth < 64 ) editwidth = 64;

		// 大きさ変更不要 ?
	if ( (hTreeWnd == NULL) && (editwidth == (ncbox.right - ncbox.left)) ){
		return;
	}

	if ( (TouchMode & TOUCH_LARGEWIDTH) &&
		!(tinput->flag & (TIEX_USEREFLINE | TIEX_USEOPTBTN)) ){
		// タッチ用ボタン(ボタンは右)
		SetWindowPos(GetDlgItem(hDlg,IDOK), NULL,
				editwidth, refbox.top,
				refbox.right, refbox.bottom,
				SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
		SetWindowText(GetDlgItem(hDlg,IDCANCEL), T("×"));
		SetWindowPos(GetDlgItem(hDlg,IDCANCEL), NULL,
				editwidth + refbox.right, refbox.top,
				refbox.bottom, refbox.bottom,
				SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
		SetWindowPos(hRefWnd,NULL,
				editwidth + refbox.right + refbox.bottom, refbox.top,
				0, refbox.bottom,
				SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
	}else{
		int buttonleft;

		if ( tinput->flag & TIEX_LINE_MULTI ){ // 行数が変化することあるので再取得
			GetWindowRect(hRefWnd, &refbox);
			refbox.bottom -= refbox.top;
			refbox.top -= ncbox.top;
			refbox.right -= refbox.left;
		}
		buttonleft = editwidth;
		if ( tinput->flag & (TIEX_USEREFLINE | TIEX_USEOPTBTN) ){ // ボタンは下
			if ( tinput->flag & TIEX_USEREFLINE ){
				RECT reflinebox;
				HWND hEref = GetDlgItem(hDlg,IDE_INPUT_REF);

				GetWindowRect(hEref,&reflinebox);
				reflinebox.bottom -= reflinebox.top;
				SetWindowPos(hEref, NULL, 0,0, buttonleft,reflinebox.bottom,
					SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
				refbox.top += reflinebox.bottom;
			}
			buttonleft -= refbox.right * 3;
			ncbox.bottom = refbox.top + refbox.bottom + 4; // ツリーY 修正
		}
									// ボタンの位置修正
		SetWindowPos(GetDlgItem(hDlg,IDOK), NULL,
				buttonleft, refbox.top, 0,0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hDlg,IDCANCEL),NULL,
				buttonleft + refbox.right,refbox.top, 0,0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOZORDER);
		SetWindowPos(hRefWnd,NULL,
				buttonleft + refbox.right * 2,refbox.top, 0,0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOZORDER);
	}

	if ( hTreeWnd != NULL ){
		int treeheight;

		treeheight = (clientbox.bottom - clientbox.top) - ncbox.bottom;
		if ( treeheight < 8 ) treeheight = 8;
		SetWindowPos(hTreeWnd, NULL, 0,0,
				clientbox.right - clientbox.left, treeheight,
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
	}
	// EditBox の大きさ修正(複数行の時は、ここでDlgの変更が起きることがあり、
	// コントロールの再配置の考慮を不要にするため、最後に修正が必要)
	SetWindowPos(hEdWnd, NULL, 0,0, editwidth, editheight,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
	InvalidateRect(hDlg, NULL, TRUE);
}

int tInputSeparateExt(HWND hDlg,TCHAR *text)
{
	TCHAR *entryptr,*ptr;
	int extoffset;

	entryptr = FindLastEntryPoint(text);
	extoffset = FindExtSeparator(entryptr);

	ptr = entryptr + extoffset;
	if ( *ptr == '.' ){
		SetDlgItemText(hDlg,IDE_INPUT_EXT,ptr + 1);
		*ptr = '\0';
		SetDlgItemText(hDlg,IDE_INPUT_LINE,text);
	}
	return extoffset;
}

void tInputConnectExt(HWND hDlg,TCHAR *text,int size,BOOL savehist)
{
	TCHAR *ptr;
	DWORD nsize = tstrlen32(text);

	ptr = text + nsize;
	*(ptr + 1) = '\0';
	GetDlgItemText(hDlg,IDE_INPUT_EXT,ptr + 1,size - nsize - 1);
	if ( *(ptr + 1) != '\0' ){
		*ptr = '.';
		if ( savehist ) WriteHistory(PPXH_GENERAL,ptr + 1,0,NULL);
	}
}

void USEFASTCALL tInputInitDialog(HWND hDlg, TINPUTSTRUCT *ts)
{
	TINPUT *tinput;
	RECT ncbox, box, refbox, deskbox;

	SIZE size;
	int width;
	HGDIOBJ hOldFont;

	tinput = ts->tinput;
	if ( !(tinput->flag & TIEX_USEINFO) ) tinput->info = NULL;

	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)ts);
	ThInit(&ts->ThMenu);

	if ( (tinput->flag & (TIEX_USEPNBTN | TIEX_USEREFLINE)) == TIEX_USEREFLINE ){
		DestroyWindow(GetDlgItem(hDlg, IDB_PREV));
		DestroyWindow(GetDlgItem(hDlg, IDB_NEXT));
	}

	ts->hEdWnd = PPxRegistExEdit(tinput->info,
			GetDlgItem(hDlg, IDE_INPUT_LINE),
			tinput->size, tinput->buff, tinput->hRtype, tinput->hWtype,
			PPXEDIT_USEALT | PPXEDIT_JOINTTREE | PPXEDIT_WANTEVENT | PPXEDIT_LINEEDIT |
			(tinput->flag & (PPXEDIT_REFTREE | PPXEDIT_SINGLEREF | PPXEDIT_INSTRSEL | PPXEDIT_LINE_MULTI)) |
			((tinput->flag & TIEX_USEREFLINE) ? 0 : PPXEDIT_TABCOMP) );
	SendMessage(ts->hEdWnd, WM_PPXCOMMAND, KE_setkeya, (LPARAM)StrK_lied);
											// メニュー追加
	if ( IsExistCustData(T("M_edit")) ){
		HMENU hMenu;
		DWORD id;

		id = IDW_MENU;
		hMenu = CreateMenu();
		PP_AddMenu(tinput->info, hDlg, hMenu, &id, T("M_edit"), &ts->ThMenu);
		SetMenu(hDlg, hMenu);
		GetWindowRect(hDlg, &box);
		SetWindowPos(hDlg, NULL, 0,0,
				box.right - box.left,
				box.bottom - box.top + GetSystemMetrics(SM_CYMENU),
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
	}
									// 文字列の大きさを取得する
	{
		HDC hDC;
		int len;
		#define LINE_MULTI_WIDTH 120

		hDC = GetDC(ts->hEdWnd);
		hOldFont = SelectObject(hDC, (HFONT)SendMessage(ts->hEdWnd, WM_GETFONT, 0, 0));
		len = tstrlen32(tinput->buff);
		if ( (tinput->flag & TIEX_LINE_MULTI) && (len > LINE_MULTI_WIDTH) ){
			len = LINE_MULTI_WIDTH;
		}
		GetTextExtentPoint32(hDC, tinput->buff, len, &size);
		SelectObject(hDC, hOldFont);
		ReleaseDC(ts->hEdWnd, hDC);
	}
							// edit controlのクライアントサイズを算出
	GetWindowRect(ts->hEdWnd, &ncbox);
	GetClientRect(ts->hEdWnd, &box);
	box.right -= box.left;
	size.cx += 32; // 編集用余白

	width = size.cx + ((ncbox.right - ncbox.left) - box.right); // 文字列幅 + エディットコントロールの枠
								// ダイアログの大きさ修正
	GetWindowRect(hDlg, &ncbox);
	GetDesktopRect(hDlg, &deskbox);
	deskbox.right -= deskbox.left + 16;

	if ( (TouchMode & TOUCH_LARGEWIDTH) || (size.cx > box.right) || ((ncbox.right - ncbox.left) > deskbox.right) ){ // 大きさが足りない・大きすぎなら調整する
		WPARAM wParam;
		LPARAM lParam;

		if ( size.cx > box.right ){
			GetClientRect(hDlg, &box);
			GetWindowRect(GetDlgItem(hDlg, IDB_REF), &refbox);

			// window サイズを算出
			ncbox.right = (width + ((refbox.right - refbox.left) * 3)) +
					(ncbox.right - ncbox.left) - (box.right - box.left);
			if ( ncbox.right > deskbox.right ) ncbox.right = deskbox.right;
		}else if ((ncbox.right - ncbox.left) > deskbox.right){
			ncbox.right = deskbox.right;
		}else{
			ncbox.right -= ncbox.left - 1;
		}

		SetWindowPos(hDlg, NULL, 0,0, ncbox.right,ncbox.bottom - ncbox.top,
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);

		SendMessage(ts->hEdWnd, EM_GETSEL, (WPARAM)&wParam, (LPARAM)&lParam);
		SendMessage(ts->hEdWnd, EM_SETSEL, 0, 0); // 先頭が表示されるように
		if ( (wParam != 0) || (lParam != 0) ){ // 選択し直し
			SendMessage(ts->hEdWnd, EM_SETSEL, wParam, lParam);
		}
	}
	if ( (tinput->info != NULL) &&
		 (tinput->info->RegID != NULL) &&
		 (tinput->info->RegID[0] == 'C') ){
		if ( X_combos_[1] < 0 ){
			GetCustData(T("X_combos"), &X_combos_, sizeof(X_combos_));
		}
		if ( X_combos_[1] & CMBS1_DIALOGNOPANE ){
			CenterWindow(hDlg);
		}else{
			MoveCenterWindow(hDlg, tinput->hOwnerWnd);
		}
	}else{
		CenterWindow(hDlg);
	}
											// Rename 用処理
	if ( tinput->flag & TIEX_USEREFLINE ){
		TCHAR *entryptr;
		int extoffset;

		LocalizeDialogText(hDlg, 0);
		SetDlgItemText(hDlg, IDE_INPUT_REF, tinput->buff);

		GetCustData(T("X_esel"), &X_esel, sizeof(X_esel));

		CheckDlgButton(hDlg, IDB_INPUT_EXT, X_esel[2]);
		ShowDlgWindow(hDlg, IDE_INPUT_EXT, !X_esel[2]);

		PPxRegistExEdit(tinput->info,
				GetDlgItem(hDlg,IDE_INPUT_EXT),
				MAX_PATH, NULL, PPXH_GENERAL, PPXH_GENERAL | PPXH_FILENAME,
				PPXEDIT_USEALT | PPXEDIT_JOINTTREE | PPXEDIT_SINGLEREF);

		entryptr = FindLastEntryPoint(tinput->buff);
		if ( !(tinput->flag & TIEX_REFEXT) ){ // 拡張子無し
			ShowDlgWindow(hDlg, IDB_INPUT_EXT,FALSE);
			ShowDlgWindow(hDlg, IDE_INPUT_EXT,FALSE);
			extoffset = tstrlen32(entryptr);
		}else if ( X_esel[2] == 0 ){ // 拡張子分離
			extoffset = tInputSeparateExt(hDlg, tinput->buff);
		}else{ // 拡張子も編集
			extoffset = FindExtSeparator(entryptr);
		}

		if ( !(tinput->flag & TIEX_USESELECT) ){
			setflag(tinput->flag, TIEX_USESELECT);

			tinput->firstC = entryptr - tinput->buff; // エントリ名先頭
			tinput->lastC = tinput->firstC + extoffset; // 拡張子前
			switch ( X_esel[0] ){ // カーソル位置を決定
				case 0: // 先頭
					if ( X_esel[1] == 0 ) tinput->lastC = tinput->firstC;
					break;

				case 2: // 末尾
					tinput->firstC = X_esel[1] ? tinput->lastC + 1 : EC_LAST;
					tinput->lastC = EC_LAST;
					break;

//				case 1: // 拡張子前
				default:
					if ( X_esel[1] == 0 ) tinput->firstC = tinput->lastC;
			}
		}

		if ( tinput->flag & TIEX_USEPNBTN ){ // アシストリスト
			PPxEDSTRUCT *PES;
			int itemcount = 0;
			int index = 0;
			TCHAR keyword[CMDLINESIZE], param[CMDLINESIZE], buf[CMDLINESIZE];

			PES = (PPxEDSTRUCT *)GetProp(ts->hEdWnd, PPxED);
			while(EnumCustTable(index++, T("M_path"), keyword, param, sizeof(param)) >= 0){
				if ( keyword[0] == '\0' ) continue;
				if ( tstrstr(tinput->buff, keyword) != NULL ){
					if ( itemcount == 0 ) CreateMainListWindow(PES, 1);
					tstrcpy(buf, tinput->buff);
					tstrreplace(buf, keyword, param);
					SendMessage(PES->list.hWnd, LB_ADDSTRING, (WPARAM)0, (LPARAM)buf);
					itemcount++;
				}
			}
			if ( itemcount > 0 ) ShowWindow(PES->list.hWnd,SW_SHOWNA);
#if 0
			{
			/*
				int mov;
				FN_REGEXP fn;

				MakeFN_REGEXP(&fn, param);
				mov = FilenameRegularExpression(line, &fn);
				FreeFN_REGEXP(&fn);
				if ( mov ){
					RXPREPLACESTRING *rexps;
					if ( IsTrue(InitRegularExpressionReplace(&rexps,keyword,FALSE)) ){
						RegularExpressionReplace(rexps, line;
						FreeRegularExpressionReplace(rexps);
					}
				}
			*/
			/*
				RXPREPLACESTRING *rexps;
				if ( IsTrue(InitRegularExpressionReplace(&rexps, param,FALSE)) ){
					RegularExpressionReplace(rexps, line);
					FreeRegularExpressionReplace(rexps);
					SendMessage(mainlist.hWnd, mainlist.msg, (WPARAM)mainlist.wParam, (LPARAM)line);
					mainlist.items++;
				}
			*/
			}
#endif
		}
	}
	SetWindowText(hDlg, MessageText(tinput->title));
	SetFocus(ts->hEdWnd);
											// 範囲選択
	if ( (tinput->flag & TIEX_USESELECT) &&
		 ( !(tinput->flag & TIEX_INSTRSEL) || (SearchPipe(tinput->buff) == NULL) ) ){

#ifndef UNICODE
		if ( xpbug < 0 ){ // PPxRegistExEdit 内で初期化済み
			CaretFixToW(tinput->buff, (DWORD *)&tinput->firstC);
			CaretFixToW(tinput->buff, (DWORD *)&tinput->lastC);
		}
#endif
		EDsHell(ts->hEdWnd, EM_SETSEL,
				(WPARAM)tinput->firstC, (LPARAM)tinput->lastC);
	}
											// 参照用ツリー使用の判定
	if ( (tinput->flag & (TIEX_REFTREE | TIEX_SINGLEREF)) ==
				(TIEX_REFTREE | TIEX_SINGLEREF) ){
		DWORD X_rtree;

		X_rtree = GetCustDword(T("X_rtree"), 0);
		if ( X_rtree == 1) X_rtree = !tinput->buff[0];
		if ( X_rtree ) setflag(tinput->flag, TIEX_REFMODE);

		if ( tinput->buff[0] && (CountPPc() > 2) ){
			if ( GetCustDword(T("X_rclst"),0) ){
				PostMessage(ts->hEdWnd, WM_PPXCOMMAND, K_raw | K_s | K_c | 'L',0);
			}
		}
	}

	if ( tinput->flag & TIEX_REFMODE ){
		PostMessage(ts->hEdWnd,WM_PPXCOMMAND, K_raw | K_s | K_c | 'I',0);
	}
	if ( tinput->flag & TIEX_TOP ) ForceSetForegroundWindow(hDlg);

	if ( IsExistCustTable(StrK_lied, T("FIRSTEVENT")) &&
		!IsExistCustTable(StrK_edit, T("FIRSTEVENT")) ){
		PostMessage(ts->hEdWnd,WM_PPXCOMMAND, K_E_FIRST, 0);
	}
	if ( tinput->flag & TIEX_EXECPRECMD ){
		PostMessage(hDlg, WM_COMMAND, TMAKEWPARAM(KE_execprecmd, 1), 0);
	}
}

void tInputPathFix(TCHAR *buff)
{
	const TCHAR *src;
	TCHAR *dst;

	src = dst = buff;
	for ( ;; ){
		TCHAR c;

		c = *src;
		switch ( c ){
			case '\0':
				*dst = '\0';
				return;
			case '\n':
			case '\r':
			case '<':
			case '>':
			case '|':
				break;
			default:
				*dst++ = c;
				#ifndef UNICODE
					if ( IskanjiA(c) ){
						c = *(src + 1);
						*dst++ = c;
						if ( c == '\0' ) return;
						src += 2;
						continue;
					}
				#endif
		}
		src++;
	}
}

void tInputContinuousMenu(HWND hDlg, LPARAM lParam)
{
	HMENU hPopupMenu = CreatePopupMenu();
	POINT pos;
	RECT box;
	int index;

	if ( lParam == 0xffffffff ){ // keyによる操作
		GetWindowRect(hDlg, &box);
		pos.x = box.left;
		pos.y = box.bottom;
	}else{ // マウス
		LPARAMtoPOINT(pos, lParam);
	}
	AppendMenuString(hPopupMenu, 1, StrContinuousMenu);
	index = TrackPopupMenu(hPopupMenu, TPM_TDEFAULT, pos.x,pos.y, 0, hDlg, NULL);
	DestroyMenu(hPopupMenu);
	if ( index == 1 ) EndDialog(hDlg, -(K_c | 'R'));
}

void USEFASTCALL SaveInputText(HWND hDlg, TINPUTSTRUCT *ts, INT_PTR result)
{
	TINPUT *tinput = ts->tinput;

	tinput->buff[0] = '\0';
	GetWindowText(ts->hEdWnd, tinput->buff, tinput->size);
	if ( tinput->hWtype ) WriteHistory(tinput->hWtype, tinput->buff, 0, NULL);
	if ( tinput->flag & TIEX_USEREFLINE ){
		tInputConnectExt(hDlg, tinput->buff, tinput->size, TRUE);
	}
	if ( tinput->flag & TIEX_FIXFORPATH ) tInputPathFix(tinput->buff);
	EndDialog(hDlg, result);
}

void tInputCalc(HWND hDlg, TINPUTSTRUCT *ts)
{
	TCHAR buf1[VFPS], buf2[0x80], *ptr;
	int result;

	buf1[0] = '\0';
	GetWindowText(ts->hEdWnd, buf1, TSIZEOF(buf1));
	if ( IsTrue(GetCalc(buf1, buf2, &result)) ){
		ptr = buf2;
	}else{
		ptr = NULL;
		if ( ts->tinput->hWtype == PPXH_FILENAME ){
			int lenName = tstrlen32(buf1), lenPath = 0;

			if ( ts->tinput->info != NULL ){
				PPXCMDENUMSTRUCT work;

				PPxEnumInfoFunc(ts->tinput->info, '1', buf1, &work);
				lenPath = tstrlen32(buf1) + lenName + 1;
			}
			if ( (lenName >= 140) || (lenPath >= MAX_PATH - 56) ){
				ptr = buf2;
				wsprintf(buf2, T("length:%c%d, path length:%c%d"),
						(lenName >= 255) ? '#' : ' ',
						lenName,
						(lenPath >= MAX_PATH) ? '#' : ' ',
						lenPath);
			}
		}
	}
	SetMessageOnCaption(hDlg, ptr);
}

void ChangeExtEditMode(HWND hDlg, HWND hBtnWnd)
{
	TCHAR buf[CMDLINESIZE];
	int len = GetWindowTextLength(GetDlgItem(hDlg, IDE_INPUT_EXT));

	GetDlgItemText(hDlg, IDE_INPUT_LINE, buf, CMDLINESIZE);
	if ( SendMessage(hBtnWnd, BM_GETCHECK, 0, 0) ){ // 拡張子編集有効
		if ( len == 0 ) return;
		tInputConnectExt(hDlg, buf, CMDLINESIZE, FALSE);
		SetDlgItemText(hDlg, IDE_INPUT_LINE, buf);
		SetDlgItemText(hDlg, IDE_INPUT_EXT, NilStr);
		X_esel[2] = 1;
	}else{ // 無効
		if ( len != 0 ) return;
		tInputSeparateExt(hDlg, buf);
		X_esel[2] = 0;
	}
	SetCustData(T("X_esel"), &X_esel, sizeof(X_esel));
	ShowDlgWindow(hDlg, IDE_INPUT_EXT, !X_esel[2]);
}

/*-----------------------------------------------------------------------------
	一行入力用ダイアログボックス/コールバック
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK tInputMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	TINPUTSTRUCT *ts;

	ts = (TINPUTSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
	if ( ts == NULL ){
		if ( message == WM_INITDIALOG ){
			tInputInitDialog(hDlg, (TINPUTSTRUCT *)lParam);
		}
		return FALSE;
	}
	switch (message){
		case WM_DESTROY:
			ThFree(&ts->ThMenu);
			break;

		case WM_SIZE:
			if ( wParam != SIZE_MINIMIZED ){
				FixInputbox(hDlg, ts->hEdWnd, ts->tinput);
			}
			return FALSE;

		case WM_CONTEXTMENU:
			if ( (GetDlgCtrlID((HWND)wParam) == IDOK) || (GetDlgCtrlID((HWND)wParam) == IDCANCEL) ){
				SendMessage(ts->hEdWnd,WM_PPXCOMMAND, K_raw | K_s | K_c | 'I', 0);
			}else if ( ts->tinput->flag & TIEX_USEPNBTN ){
				tInputContinuousMenu(hDlg, lParam);
			}
			return FALSE;

		case WM_NCHITTEST: { // サイズ変更を左右のみに制限
			LRESULT result = DefWindowProc(hDlg, WM_NCHITTEST, wParam, lParam);

			switch ( result ){
				case HTTOP:
				case HTBOTTOM: {
					PPxEDSTRUCT *PES;

					PES = (PPxEDSTRUCT *)GetProp(ts->hEdWnd, PPxED);
					if ( (PES == NULL) || (PES->hTreeWnd == NULL) || !(PES->flags & PPXEDIT_JOINTTREE) ){
						result = HTBORDER;
					}
					break;
				}

				case HTTOPLEFT:
				case HTBOTTOMLEFT:
					result = HTLEFT;
					break;

				case HTTOPRIGHT:
				case HTBOTTOMRIGHT:
					result = HTRIGHT;
					break;
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, result);
			break;
		}

		case WM_COMMAND:
			if ( HIWORD(wParam) == 0 ){	// メニュー
				if ( LOWORD(wParam) >= IDW_MENU ){
					const TCHAR *ptr;

					ptr = (TCHAR *)ts->ThMenu.bottom;
					if ( ptr != NULL ){
						ptr = GetMenuDataString(&ts->ThMenu,
								LOWORD(wParam) - IDW_MENU);
						if ( *ptr != '\0' ){
							TinputExtractMacro(ts, ptr/*,NULL,0*/);
						}
					}
				}
			}
			switch ( LOWORD(wParam) ){
				case KE_execprecmd:
					ExecPreCommand(ts);
					break;

				case IDOK:
					SaveInputText(hDlg,ts,1);
					break;

				case IDB_PREV:
				case IDB_NEXT:
					SaveInputText( hDlg,ts,LOWORD(wParam) );
					break;

				case IDCANCEL:
					EndDialog(hDlg,0);
					break;

				case IDB_REF:
					SendMessage(ts->hEdWnd,
							WM_PPXCOMMAND,K_raw | K_s | K_c | 'I',0);
					break;

				case IDB_INPUT_EXT:
					ChangeExtEditMode(hDlg,(HWND)lParam);
					break;

				case IDE_INPUT_LINE:
					if ( HIWORD(wParam) != EN_CHANGE ) break;
					if ( X_calc < 0 ){
						GetCustData(T("X_calc"), &X_calc, sizeof X_calc);
					}
					if ( X_calc ) tInputCalc(hDlg, ts);
					if ( ts->tinput->flag & TIEX_NCHANGE ){
						SendMessage(ts->tinput->hOwnerWnd, WM_PPXCOMMAND, K_EDITCHANGE, lParam);
					}
					break;

				case IDB_INPUT_OPT:
					TinputExtractMacro(ts,
							T("*execute %s,\"Edit_OptionCmd\"")/*,NULL,0*/);
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;

		default:
			if ( message == WM_PPXCOMMAND ) return ERROR_INVALID_FUNCTION;
			return FALSE;
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	一行入力用ダイアログボックス
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI tInput(HWND hWnd, const TCHAR *title, TCHAR *string, int maxlen, WORD readhist, WORD writehist)
{
	TINPUT tinput;

	tinput.hOwnerWnd= hWnd;
	tinput.hRtype	= readhist;
	tinput.hWtype	= writehist;
	tinput.title	= title;
	tinput.buff		= string;
	tinput.size		= maxlen;
	tinput.flag		= 0;
	if ( writehist == PPXH_DIR ){
		setflag(tinput.flag, TIEX_REFTREE | TIEX_SINGLEREF);
	}
	return tInputEx(&tinput);
}
/*-----------------------------------------------------------------------------
	一行入力用ダイアログボックス(拡張版)
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI tInputEx(TINPUT *tinput)
{
	int result;
	UINT dialogid;
	TINPUTSTRUCT ts;

	ts.tinput = tinput;
	if ( tinput->flag & (TIEX_USEREFLINE | TIEX_USEOPTBTN) ){
		dialogid = (tinput->flag & TIEX_USEREFLINE) ? IDD_INPUTREF : IDD_INPUT_OPT;
	}else{
		dialogid = IDD_INPUT;
	}

	if ( GetCustDword(T("X_mled"), 0) != 0 ){ // 複数行の時は、IDE_INPUT_LINE の style に ES_MULTILINE を追加、ES_AUTOHSCROLL を削除
		DLGTEMPLATE *dialog;
		WORD *dlgptr, *dlgmax;

		dialog = GetDialogTemplate(tinput->hOwnerWnd, DLLhInst, MAKEINTRESOURCE(dialogid));
		if ( dialog == NULL ) return -1;

		setflag(tinput->flag, TIEX_LINE_MULTI);

		dlgptr = (WORD *)dialog;
		dlgmax = dlgptr + ((HeapSize(ProcHeap,0,dlgptr) - sizeof(DLGTEMPLATE) - sizeof(DLGITEMTEMPLATE)) / sizeof(WORD));
		for ( ; dlgptr < dlgmax ; dlgptr++ ){ // IDE_INPUT_LINE を検索、修正
			if ( (((DLGITEMTEMPLATE *)dlgptr)->style == IDE_INPUT_LINE_STYLE) &&
				 (((DLGITEMTEMPLATE *)dlgptr)->id == IDE_INPUT_LINE) ){
				((DLGITEMTEMPLATE *)dlgptr)->style = IDE_INPUT_LINE_MULTI_STYLE;
				break;
			}
		}
		result = DialogBoxIndirectParam(DLLhInst, dialog,
				tinput->hOwnerWnd, tInputMain, (LPARAM)&ts);
		HeapFree(ProcHeap, 0, dialog);
	}else{
		result = PPxDialogBoxParam(DLLhInst, MAKEINTRESOURCE(dialogid),
				tinput->hOwnerWnd, tInputMain, (LPARAM)&ts);
	}
	SetIMEDefaultStatus(tinput->hOwnerWnd);
	return result;
}

void ClosePPeTreeWindow(PPxEDSTRUCT *PES)
{
	if ( PES->hTreeWnd == NULL ) return;

	if ( PES->flags & PPXEDIT_JOINTTREE ){ // 親と一体
		HWND hParent;
		RECT parentbox, treebox;

		SetFocus(PES->hWnd);
		hParent = GetParentCaptionWindow(PES->hWnd);
		GetWindowRect(hParent, &parentbox);
		GetWindowRect(PES->hTreeWnd, &treebox);
		DestroyWindow(PES->hTreeWnd);
		SetWindowPos(hParent,NULL,0,0,
				parentbox.right - parentbox.left,
				parentbox.bottom - parentbox.top - (treebox.bottom - treebox.top),
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	}else{
		DestroyWindow(PES->hTreeWnd);
	}
	PES->hTreeWnd = NULL;
}

// Tree 処理用 ----------------------------------------------------------------
void PPeTreeWindow(PPxEDSTRUCT *PES)
{
	HWND hParent;
	RECT box, deskbox;
	PPXCMDENUMSTRUCT work;
	TCHAR path[VFPS];
	HWND hWnd = PES->hWnd;
	DWORD X_tree[3];
	#define expand_height X_tree[2]

	if ( PES->hTreeWnd != NULL ){
		ClosePPeTreeWindow(PES);
		return;
	}
	InitVFSTree();

	hParent = GetParentCaptionWindow(hWnd);
	GetWindowRect(hParent, &box);
	GetDesktopRect(hParent, &deskbox);

	X_tree[1] = X_tree[2] = (TREEHEIGHT * GetMonitorDPI(hWnd)) / DEFAULT_WIN_DPI;
	GetCustData(StrX_tree, X_tree, sizeof(X_tree));
	if ( PES->flags & PPXEDIT_JOINTTREE ) X_tree[2] = X_tree[1];  // 親と一体

	if ( expand_height < 64 ){
		expand_height = (expand_height < 32) ? TREEHEIGHT : 64;
	}

	if ( (box.bottom + (int)expand_height) > deskbox.bottom ) {
		// 上にずらして画面内に納める
		int diff = box.bottom + (int)expand_height - deskbox.bottom;

		if ( (box.top >= deskbox.top) && ((box.top - diff) < deskbox.top) ){
			diff = box.top - deskbox.top;
		}
		if ( diff > 0 ){
			box.top -= diff;
			box.bottom -= diff;

			SetWindowPos(hParent, NULL, box.left, box.top, 0,0,
					SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
		}
		// 高さを減らして画面内に納める
		if ( (box.bottom + (int)expand_height) > deskbox.bottom ) {
			expand_height = deskbox.bottom - box.bottom;
			if ( expand_height < 100 ) expand_height = 100;
		}
	}

	if ( PES->flags & PPXEDIT_JOINTTREE ){
		SetWindowPos(hParent, NULL, 0,0,
				box.right - box.left, box.bottom - box.top + expand_height,
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

		GetClientRect(hParent, &box);

		PES->hTreeWnd = CreateWindow(TreeClassStr, MessageText(MES_TSDR),
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,box.bottom - (int)expand_height,
				box.right - box.left, (int)expand_height, hParent, NULL, NULL, 0);
	}else{
		PES->hTreeWnd = CreateWindow(TreeClassStr, MessageText(MES_TSDR),
				WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				box.left, box.bottom,
				box.right - box.left, (int)expand_height, hParent, NULL, NULL, 0);
	}
	SendMessage(PES->hTreeWnd, VTM_SETFLAG,
			(WPARAM)hWnd, (LPARAM)(VFSTREE_SELECT | VFSTREE_PATHNOTIFY));

	if ( PES->flags & PPXEDIT_SINGLEREF ){
		GetWindowText(hWnd, path, TSIZEOF(path));
	}else{
		path[0] = '\0';
	}
	if ( path[0] == '\0' ) PPxEnumInfoFunc(PES->info, '1', path, &work);
	SendMessage(PES->hTreeWnd, VTM_INITTREE, 0, (LPARAM)path);

	ShowWindow(PES->hTreeWnd, SW_SHOWNORMAL);
	SetFocus(PES->hTreeWnd);
}
