/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library - 拡張エディット / ヒストリ・補完リスト
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "PPD_EDL.H"
#include "VFS_STRU.H"
#include "CALC.H"
#pragma hdrstop

#define AllowTick 100 // 補完列挙を中止する時間
#define ThreadAllowTick 3000 // 補完列挙を中止する時間(スレッド使用時)

/*
	コンボボックスモード
						  入力時   表示後入力
		1:ヒストリ一覧		 -          -
		2:ファイル補完一覧	 -		頭文字検索
		3:ヒストリ補完一覧  一覧    一覧再作成
*/

TCHAR BTERR[] = T("BackupText err");

#define ABS(n) (n >= 0 ? n : -n)

#define TEXT_PATH 0
#define TEXT_WORDPATH 1
#define TEXT_MASK 2

typedef struct {
	HWND hWnd;
	UINT msg;
	DWORD wParam;
	int items;
} LISTADDINFO;

FILLTEXTFILE filltext_cmd = { T("PPXUCMD.TXT"),NULL,NULL/*, {0, 0}*/ };
FILLTEXTFILE filltext_path = { T("PPXUPATH.TXT"),NULL,NULL/*, {0, 0}*/ };
FILLTEXTFILE filltext_mask = { T("PPXUMASK.TXT"),NULL,NULL/*, {0, 0}*/ };

typedef struct {
	PPXAPPINFO info;
	LISTADDINFO *list;
	PPxEDSTRUCT *PES;
} SMODULECAPPINFO;

void CleanUpEdit(void)
{
	if ( filltext_cmd.mem != NULL ){
		HeapFree(ProcHeap, 0, filltext_cmd.mem);
		filltext_cmd.mem = NULL;
	}
	if ( filltext_path.mem != NULL ){
		HeapFree(ProcHeap, 0, filltext_path.mem);
		filltext_path.mem = NULL;
	}
	if ( filltext_mask.mem != NULL ){
		HeapFree(ProcHeap, 0, filltext_mask.mem);
		filltext_mask.mem = NULL;
	}
}

void FreeBackupText(PPxEDSTRUCT *PES)
{
	if ( PES->list.OldString != NULL ){
		HeapFree(DLLheap, 0, PES->list.OldString);
		PES->list.OldString = NULL;
	}
}

void BackupText(HWND hWnd, PPxEDSTRUCT *PES)
{
	int len;

	FreeBackupText(PES);
	len = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0) + 1;
	PES->list.OldString = HeapAlloc(DLLheap, 0, TSTROFF(len));
	if ( PES->list.OldString == NULL ){
		PES->list.OldString = BTERR;
		xmessage(XM_FaERRld, BTERR);
		return;
	}
	*PES->list.OldString = '\0';
	SendMessage(hWnd, WM_GETTEXT, (WPARAM)len, (LPARAM)PES->list.OldString);
	SendMessage(hWnd, EM_GETSEL, (WPARAM)&PES->list.OldStart, (LPARAM)&PES->list.OldEnd);
}

void RestoreBackupText(HWND hEditWnd, HWND hListWnd, PPxEDSTRUCT *PES)
{
	SendMessage(hListWnd, LB_SETCURSEL, (WPARAM)-1, 0);

	if ( PES->list.OldString == NULL ) return;
	SetWindowText(hEditWnd, PES->list.OldString);
	SendMessage(hEditWnd, EM_SETSEL, (WPARAM)PES->list.OldStart, (LPARAM)PES->list.OldEnd);
}

DWORD CheckSamelen(const TCHAR *str1, const TCHAR *str2)
{
	const TCHAR *s1 = str1, *s2 = str2;
	DWORD len = 0;

	while ( (*s1 != '\0') && (*s1 == *s2) ){
	#ifdef UNICODE
		s1++;
		s2++;
		len++;
	#else
		int clen = Chrlen(*s1);
		if ( (clen > 1) && (*(s1 + 1) != *(s2 + 1) ) ) break;
		s1 += clen;
		s2 += clen;
		len += clen;
	#endif
	}
	return len;
}

BOOL MakeCompleteList(HWND hWnd,  PPxEDSTRUCT *PES)
{
	ECURSOR cursor;
	TCHAR *p;
	TCHAR buf[VFPS + MAX_PATH], buf2[VFPS + MAX_PATH];
	DWORD fmode;
	DWORD len, samelen;

	if ( (DWORD)SendMessage(PES->hWnd,
			WM_GETTEXT, TSIZEOF(buf), (LPARAM)&buf) >= TSIZEOF(buf) ){
		return FALSE;
	}
	SendMessage(PES->hWnd, WM_SETREDRAW, FALSE, 0);
												// 編集文字列(全て)を取得
	GetEditSel(PES->hWnd, buf, &cursor);
	fmode = (PES->ED.cmdsearch & CMDSEARCH_FLOAT) |
			((PES->list.WhistID & PPXH_COMMAND) ?
						CMDSEARCH_CURRENT : CMDSEARCH_OFF) |
			((PES->flags & PPXEDIT_SINGLEREF) ? 0 : CMDSEARCH_MULTI) |
			CMDSEARCH_EDITBOX | CMDSEARCH_ONE;

	if ( (PES->list.WhistID & (PPXH_DIR | PPXH_PPCPATH)) &&
		 IsTrue(GetCustDword(T("X_fdir"), TRUE)) ){
		setflag(fmode, CMDSEARCH_DIRECTORY);
	}
	// １回目検索
	PES->ED.FnameP = PES->list.OldString; // ここでリスト(backuptext)ができていること!

	p = SearchFileIned(&PES->ED, buf, &cursor, fmode | CMDSEARCHI_SAVEWORD);
	if ( p == NULL ){
		SetMessageForEdit(PES->hWnd, MES_EENF);
		SendMessage(PES->hWnd, WM_SETREDRAW, TRUE, 0);
		return FALSE;
	}
	// １回目結果を反映
	SendMessage(PES->hWnd, EM_SETSEL, cursor.start, cursor.end);
							// ※↑SearchFileIned 内で加工済み
	SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)p);
	SendMessage(PES->hWnd, EM_SETSEL, 0, 0);	// 表示開始桁を補正させる

	samelen = len = (DWORD)tstrlen(p);
#ifndef UNICODE
	if ( xpbug < 0 ) CaretFixToW(p, &len);
#endif
	PES->list.select.start = cursor.start;
	PES->list.select.end = cursor.start + len;

	SendMessage(PES->hWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(PES->hWnd,NULL, FALSE);
	tstrcpy(buf2, p);
	tstrcpy(buf, PES->ED.Fname);

	// ブラケット処理済みは、これ以上重複処理させない
	if ( PES->ED.cmdsearch & CMDSEARCHI_FINDBRAKET ){
		resetflag(fmode, CMDSEARCH_MULTI);
	}
	// ２回目検索
	cursor.start = 0;
	cursor.end   = tstrlen32(buf);
	p = SearchFileIned(&PES->ED, buf, &cursor, fmode);
	if ( p == NULL ){
		PostMessage(hWnd, WM_CLOSE, 0, 0);
	}else{
		DWORD starttime;

		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)(PES->list.OldString)); // Undo用
		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)buf2); // １回目
		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)p); // ２回目
		samelen = CheckSamelen(buf2, p);
		starttime = GetTickCount();
		// ３回目以降検索
		for ( ; ; ){
			tstrcpy(buf, PES->ED.Fname);
			cursor.start = 0;
			cursor.end   = tstrlen32(buf);
			p = SearchFileIned(&PES->ED, buf, &cursor, fmode);
			if ( p == NULL ) break;
			SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)p);
			samelen = CheckSamelen(buf2, p);
			if ( (GetTickCount() - starttime) > AllowTick ){
				SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)T("more..."));
				break;
			}
		}
	}
	if ( PES->flags & CMDSEARCHI_SELECTPART){
	#ifndef UNICODE
		if ( xpbug < 0 ) CaretFixToW(buf2, &samelen);
	#endif
	}else{
		samelen = len;
	}
	SendMessage(PES->hWnd, EM_SETSEL,
			PES->list.select.start + samelen, PES->list.select.start + len);
	SetMessageForEdit(PES->hWnd,NULL);
	SendMessage(hWnd, LB_SETCURSEL, 1, 0);
	return TRUE;
}

BOOL MakeFloatList(HWND hWnd, PPxEDSTRUCT *PES, EDITDIST dist)
{
	int i;
	UINT msg;

	SendMessage(hWnd, LB_RESETCONTENT, 0, 0);

	BackupText(PES->hWnd, PES);
	if ( PES->list.mode >= LIST_FILL ){
		return MakeCompleteList(hWnd, PES);
	}

	msg = dist > 0 ? LB_ADDSTRING : LB_INSERTSTRING;
	UsePPx();
										// 編集中の内容をリストに入れる
	if ( dist != 0 ){
		const TCHAR *first;

		first = EnumHistory(PES->list.WhistID, 0);	// ヒストリ１つ目と重複？
		if ( first != NULL ){
			if ( tstricmp(PES->list.OldString, first) ){
				SendMessage(hWnd, msg, 0, (LPARAM)PES->list.OldString);
			}
		}
	}
										// ヒストリ内容を登録:W:近い内容
	for ( i = 0 ; i < 100 ; i++ ){
		const TCHAR *histptrW;

		histptrW = EnumHistory(PES->list.WhistID, i);
		if ( histptrW == NULL ) break;
		#ifdef UNICODE
			if (ALIGNMENT_BITS(histptrW) & 1 ){
				SendUTextMessage_U(hWnd, msg, 0, histptrW);
			}else
		#endif
				SendMessage(hWnd, msg, 0, (LPARAM)histptrW);
	}
										// ヒストリ内容を追加登録:R:遠い内容
	for ( ; i < 100 ; i++ ){
		const TCHAR *histptrR;

		histptrR = EnumHistory(PES->list.RhistID & (WORD)~PES->list.WhistID, i);
		if ( histptrR == NULL ) break;
		#ifdef UNICODE
			if (ALIGNMENT_BITS(histptrR) & 1 ){
				SendUTextMessage_U(hWnd, msg, 0, histptrR);
			}else
		#endif
				SendMessage(hWnd, msg, 0, (LPARAM)histptrR);
	}
	if ( dist < 0 ){
		SendMessage(hWnd, LB_SETCURSEL, SendMessage(hWnd, LB_GETCOUNT, 0, 0) - 1, 0);
	}
	if ( dist > 0 ){
		SendMessage(hWnd, LB_SETCURSEL, 0, 0);
	}
	FreePPx();
	return TRUE;
}

//------------------------------------- 一覧窓本体
LRESULT CALLBACK TextListWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	PPxEDSTRUCT *PES;
	PES = (PPxEDSTRUCT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch ( iMsg ){
		case WM_KILLFOCUS:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case WM_KEYDOWN:
			if ( (wParam != VK_ESCAPE) && (wParam != VK_RETURN) && (wParam != VK_F4) ){
				break;
			}
			// ESC 等なら、WM_LBUTTONUP を処理して、窓を閉じる

		case WM_LBUTTONUP:
			SendMessage(PES->hWnd, WM_COMMAND, TMAKELPARAM(0, LBN_SELCHANGE), (LPARAM)hWnd);
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		// 「マウス ポインタをウィンドウに重ねたときにアクティブ」有効時、
		// アクティブにされるのを防止する
		case WM_MOUSEACTIVATE:
			if ( HIWORD(lParam) == WM_MOUSEMOVE ) return MA_NOACTIVATE;
			break;

		case WM_DESTROY:
			PES->list.hWnd = NULL;
			if ( PES->list.hSubWnd == NULL){
				PES->list.index = 0;
				PES->list.mode = 0;
				PES->list.ListWindow = LISTU_NOLIST;
			}
			break;

		case LB_ADDSTRING:
		case LB_INSERTSTRING:
			if ( wParam == 0 ) break;
			// 送信スレッド有効チェック
			if ( wParam != PES->ActiveListThreadID ) return LB_ERR;
			wParam = 0;
			break;
	}
	return CallWindowProc(PES->list.OldProc, hWnd, iMsg, wParam, lParam);
}

LRESULT CALLBACK ListSubWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	PPxEDSTRUCT *PES;
	PES = (PPxEDSTRUCT *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch ( iMsg ){
		case WM_KILLFOCUS:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case WM_KEYDOWN:
			if ( (wParam != VK_ESCAPE) && (wParam != VK_RETURN) && (wParam != VK_F4) ){
				break;
			}
			// ESC 等なら、WM_LBUTTONUP を処理して、窓を閉じる

		case WM_LBUTTONUP:
			SendMessage(PES->hWnd, WM_COMMAND, TMAKELPARAM(0, LBN_SELCHANGE), (LPARAM)hWnd);
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		// 「マウス ポインタをウィンドウに重ねたときにアクティブ」有効時、
		// アクティブにされるのを防止する
		case WM_MOUSEACTIVATE:
			if ( HIWORD(lParam) == WM_MOUSEMOVE ) return MA_NOACTIVATE;
			break;

		case WM_DESTROY:
			PES->list.hSubWnd = NULL;
			if ( PES->list.hWnd == NULL ){
				PES->list.index = 0;
				PES->list.mode = 0;
				PES->list.ListWindow = LISTU_NOLIST;
			}
			break;

		case LB_ADDSTRING:
		case LB_INSERTSTRING:
			if ( wParam == 0 ) break;
			// 送信スレッド有効チェック
			if ( wParam != PES->ActiveListThreadID ) return LB_ERR;
			wParam = 0;
			break;
	}
	return CallWindowProc(PES->list.OldSubProc, hWnd, iMsg, wParam, lParam);
}

HWND CreateListWindowMain(PPxEDSTRUCT *PES, int direction)
{
	RECT box, deskbox;
	UINT ListHeight = CBLISTEIGHT;
	HWND hListWnd;

	if ( (X_dss != DSS_NOLOAD) && (X_dss & DSS_FONT) ){
		UINT dpi = GetMonitorDPI(PES->hWnd);

		ListHeight = (ListHeight * dpi) / DEFAULT_WIN_DPI;
	}
	GetWindowRect(PES->hWnd, &box);
	GetDesktopRect(PES->hWnd, &deskbox);

	if ( direction > 0 ){ // 下側配置
		if ( (box.bottom + (int)ListHeight) > deskbox.bottom ){
			ListHeight = deskbox.bottom - box.bottom;
			if ( ListHeight < (PES->fontY * 2) ) ListHeight = (PES->fontY * 2);
		}
	}else{ // 上側配置
		box.bottom = box.top - ListHeight;
		if ( box.bottom < deskbox.top ){
			ListHeight = box.top - deskbox.top;
			if ( ListHeight < (PES->fontY * 2) ) ListHeight = (PES->fontY * 2);
			box.bottom = box.top - ListHeight;
		}
	}
	hListWnd = CreateWindowEx(
			(WinType >= WINTYPE_2000) ? WS_EX_NOACTIVATE : 0,
			LISTBOXstr,NilStr,
/* ↓窓枠が太くなる分左右位置がずれる＆上下サイズを変えると高さが縮むので中止
			(WinType >= WINTYPE_10) ?
				(WS_THICKFRAME | WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY) :
				(WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY),
*/
			WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY,
			box.left, box.bottom, box.right - box.left, ListHeight,
			PES->hWnd,NULL, DLLhInst,NULL);
	SetWindowLongPtr(hListWnd, GWLP_USERDATA, (LONG_PTR)PES);
	SendMessage(hListWnd, WM_SETFONT, SendMessage(PES->hWnd, WM_GETFONT, 0, 0), 0);

	if ( direction <= 0 ){	// 上側配置のとき、フォントサイズに合うよう高さが
							// 変化して位置ずれ起きることあるので修正
		GetWindowRect(hListWnd, &deskbox);
		ListHeight -= (deskbox.bottom - deskbox.top);
		if ( ListHeight > 0 ){
			SetWindowPos(hListWnd,NULL, box.left, box.bottom + ListHeight,
				0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW);
		}
	}
	return hListWnd;
}

void CreateMainListWindow(PPxEDSTRUCT *PES, int direction)
{
	PES->list.hWnd = CreateListWindowMain(PES, direction);
	PES->list.direction = direction;
	PES->list.ListWindow = LISTU_FOCUSMAIN;
	PES->list.OldProc = (WNDPROC)SetWindowLongPtr
			(PES->list.hWnd, GWLP_WNDPROC, (LONG_PTR)TextListWndProc);
}

// 補助リスト(常に上に位置する)
void CreateSubListWindow(PPxEDSTRUCT *PES)
{
	PES->list.hSubWnd = CreateListWindowMain(PES, 0);
	PES->list.OldSubProc = (WNDPROC)SetWindowLongPtr
			(PES->list.hSubWnd, GWLP_WNDPROC, (LONG_PTR)ListSubWndProc);
}

void FloatList(PPxEDSTRUCT *PES, EDITDIST dist)
{
	if ( PES->list.hWnd != NULL ){
		PostMessage(PES->list.hWnd, WM_CLOSE, 0, 0);
		return;
	}
	if ( (dist > EDITDIST_NEXT) || (dist < EDITDIST_BACK) ){ // EDITDIST_NEXT_FILL / EDITDIST_BACK_FILL
		PES->list.mode = LIST_FILL;
	}else{
		PES->list.mode = LIST_MANUAL;
	}
	CreateMainListWindow(PES, dist); // PES->list.hWnd 再生成
	if ( FALSE != MakeFloatList(PES->list.hWnd, PES, dist) ){
		ShowWindow(PES->list.hWnd, SW_SHOWNA);
	}else{
		PostMessage(PES->list.hWnd, WM_CLOSE, 0, 0);
	}
}

void FixListPosition(PPxEDSTRUCT *PES, HWND hWnd)
{
	RECT parentbox, listbox;

	GetWindowRect(hWnd, &parentbox);
	GetWindowRect(PES->list.hWnd, &listbox);
	SetWindowPos(PES->list.hWnd,NULL, parentbox.left,
		PES->list.direction > 0 ?
			parentbox.bottom : parentbox.top - (listbox.bottom - listbox.top),
		0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

	if ( PES->list.hSubWnd != NULL ){
		GetWindowRect(PES->list.hSubWnd, &listbox);
		SetWindowPos(PES->list.hSubWnd,NULL, parentbox.left,
				parentbox.top - (listbox.bottom - listbox.top),
		0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
	}
}

// テキスト中のコマンド名の末尾を検索
DWORD GetCommandEnd(const TCHAR *text)
{
	const TCHAR *p, *subp;
	DWORD wP;

	p = text;
	while ( (*p == ' ') || (*p == '\t') ) p++; // 行頭空白
	// コマンドをスキップ
	if ( *p == '\"' ){
		p++;
		while ( ((UTCHAR)*p >= ' ') && (*p != '\"') ) p++;
		if ( *p == '\"' ) p++;
	}else{
		while ( (UTCHAR)*p > ' ' ) p++;
	}

	for ( ; ; ){ // コマンド直後のオプションを選択範囲から除く
		subp = p;
		while ( (*subp == ' ') || (*subp == '\t') ) subp++;
		if ( !((*subp == '/') || (*subp == '-')) ) break;
		while ( (UTCHAR)*subp > ' ' ) subp++;
		p = subp;
	}

	if ( (*p == ' ') || (*p == '\t') ) p++; // コマンド区切りを１文字確保

	wP = p - text;
#ifndef UNICODE
	if ( xpbug < 0 ) CaretFixToW(text, &wP);
#endif
	return wP;
}

void ListSelect(PPxEDSTRUCT *PES, HWND hListWnd)
{
	TCHAR buf[0x1000];
	HWND hWnd;

	if ( (hListWnd != PES->list.hWnd) && (hListWnd != PES->list.hSubWnd) ){
		return;
	}
	buf[0] = '\0';
	SendMessage(hListWnd, LB_GETTEXT,
			(WPARAM)SendMessage(hListWnd, LB_GETCURSEL, 0, 0), (LPARAM)buf);
	hWnd = PES->hWnd;
	if ( PES->list.mode < LIST_FILL ){
		SetWindowText(hWnd, buf);
		SendMessage(hWnd, EM_SETSEL, EC_LAST, EC_LAST);
	}else{
		TCHAR *p;
		DWORD wP, lP;

		// コメント以降を削除
		for ( p = buf ; *p ; p++ ){
			if ( (*p == ' ') || (*p == '\t') ){
				if ( *(p + 1) == ';' ){
					*p = '\0';
					break;
				}
			}
		}
		if ( PES->list.mode == LIST_FILLEXT ){
			SendMessage(hWnd, EM_GETSEL, (WPARAM)&wP, (LPARAM)&lP);
			// カーソルが置き換え位置より後なら、全て置きかえにする
			if ( (wP == lP) && (lP > PES->list.select.end) ){
				PES->list.select.end = EC_LAST;
				PES->list.mode = LIST_FILL;
			}else{ // カーソルが置き換え位置より前なら、拡張子を残す
				p = buf + FindExtSeparator(VFSFindLastEntry(buf));
				if ( *p == '.' ) *p = '\0';
			}
		}

		SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
		SendMessage(hWnd, EM_SETSEL, PES->list.select.start, PES->list.select.end);
		SendMessage(hWnd, EM_REPLACESEL, 0, (LPARAM)buf);

		if ( (PES->list.WhistID != PPXH_COMMAND) || PES->list.select.start ){
			SendMessage(hWnd, EM_GETSEL, (WPARAM)&wP, (LPARAM)&PES->list.select.end);
			SendMessage(hWnd, EM_SETSEL, PES->list.select.end, PES->list.select.end);
		}else{ // コマンドなら、パラメータの部分を選択状態にする
			PES->list.select.end = EC_LAST;
			SendMessage(hWnd, EM_SETSEL, GetCommandEnd(buf), PES->list.select.end);
		}
		SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(hWnd, NULL, FALSE);
	}
}

ERRORCODE ListPageUpDown(PPxEDSTRUCT *PES, EDITDIST dist)
{
	if ( (PES->findrep != NULL) && (PES->findrep->findtext[0] != '\0') ){
		SearchStr(PES, dist);
		return NO_ERROR;
	}

	if ( PES->flags & PPXEDIT_TEXTEDIT ) return ERROR_INVALID_FUNCTION;

	if ( PES->list.ListWindow != LISTU_NOLIST ){ // リスト表示中→リスト移動
		HWND hListWnd;
		WPARAM sendkeyID;

		hListWnd = (PES->list.ListWindow == LISTU_FOCUSSUB) ?
			PES->list.hSubWnd : PES->list.hWnd;

		sendkeyID = (dist > 0) ? VK_NEXT : VK_PRIOR;
		SendMessage(hListWnd, WM_KEYDOWN, sendkeyID, 0);
		SendMessage(hListWnd, WM_KEYUP, sendkeyID, 0);
		return NO_ERROR;
	}else if ( PES->flags & PPXEDIT_COMBOBOX ){
		return ERROR_INVALID_FUNCTION;
	}else if ( PES->style & WS_VSCROLL ){
		FloatList(PES, dist);
	}
	return NO_ERROR;
}

BOOL ListUpDown(HWND hWnd, PPxEDSTRUCT *PES, int offset, int repeat)
{
	TCHAR nowline[CMDLINESIZE];

	if ( !(PES->flags & PPXEDIT_NOINCLIST) ){
		if ( PES->list.hWnd == NULL ){
			if ( PES->flags & PPXEDIT_TEXTEDIT ) return FALSE;	// 複数行は未対応
			if ( !(PES->style & WS_VSCROLL) ) return FALSE;	// スクロールバー無は未対応
		}
		// リスト表示時の処理 -------------------------------------------------
		if ( (PES->list.ListWindow == LISTU_FOCUSSUB) || // 第２リスト上？
			 ((PES->list.hWnd == NULL) && (PES->list.hSubWnd != NULL)) ){
			int index, count;

			index = SendMessage(PES->list.hSubWnd, LB_GETCURSEL, 0, 0);
			count = SendMessage(PES->list.hSubWnd, LB_GETCOUNT, 0, 0);
			if ( index < 0 ) index = count;
			index += offset;
			if ( index < 0 ){ // 上にはみ出す
				index = 0;
			}
			if ( index >= count ){
				PES->list.ListWindow = LISTU_FOCUSMAIN; // 第１リスト(編集行)へ
				if ( PES->list.OldString != NULL ){
					PES->list.select.end = PES->list.OldEnd;
					RestoreBackupText(hWnd, PES->list.hSubWnd, PES);
				}
				return TRUE;
			}
			SendMessage(PES->list.hSubWnd, LB_SETCURSEL, index, 0);
			SendMessage(hWnd, WM_COMMAND, TMAKELPARAM(0, LBN_SELCHANGE), (LPARAM)PES->list.hSubWnd);
			return TRUE;
		}
		if ( PES->list.hWnd != NULL ){	// 第１リスト表示中→リスト移動
			int nowindex, index, count;

			nowindex = SendMessage(PES->list.hWnd, LB_GETCURSEL, 0, 0);
			index = nowindex + offset;
			if ( PES->list.direction < 0 ){ // 第１リスト-上側表示
				count = SendMessage(PES->list.hWnd, LB_GETCOUNT, 0, 0);
				if ( nowindex == -1 ){ // 未選択
					if ( offset < 0 ){ // ↑
						if ( PES->flags & PPXEDIT_COMBOBOX ){
							LRESULT cbindex;

							cbindex = SendMessage(GetParent(PES->hWnd), CB_GETCURSEL, 0, 0);
							if ( (cbindex != 0) && (cbindex != CB_ERR) ){
								return FALSE; // コンボボックスリストの操作中
							}
						}
						index = count - 1; // list.hWnd の最上位行へ

						PES->list.select.start = 0;
						PES->list.select.end = EC_LAST;

					}else{ // コンボボックスの操作へ
						return FALSE;
					}
					BackupText(hWnd, PES);
				}else if ( index == -1 ){ // 最上位行... 止まる
					index = 0;
				}else if ( index >= count ){ // 編集行へ
					if ( PES->list.OldString != NULL ){
						PES->list.select.end = PES->list.OldEnd;
						RestoreBackupText(hWnd, PES->list.hWnd, PES);
					}
					return TRUE;
				}
			}else{ // 第１リスト-下側表示
				if ( nowindex == -1 ) BackupText(hWnd, PES);
				if ( index < 0 ){ // -1(0-1):フォーカス解除 -2(-1-1):第2へ
					if ( (index == -2) && (PES->list.hSubWnd != NULL) ){
						PES->list.ListWindow = LISTU_FOCUSSUB; // 第２リストへ
						count = SendMessage(PES->list.hSubWnd, LB_GETCOUNT, 0, 0);
						SendMessage(PES->list.hSubWnd, LB_SETCURSEL, count - 1, 0);
						SendMessage(hWnd, WM_COMMAND, TMAKELPARAM(0, LBN_SELCHANGE), (LPARAM)PES->list.hSubWnd);
					}else{ // 編集行へ
						if ( PES->list.OldString != NULL ){
							PES->list.select.end = PES->list.OldEnd;
							RestoreBackupText(hWnd, PES->list.hWnd, PES);
						}
					}
					return TRUE;
				}else if ( SendMessage(PES->list.hWnd, LB_GETCOUNT, 0, 0) == 0 ){
					return FALSE;
				}
			}
			SendMessage(PES->list.hWnd, LB_SETCURSEL, index, 0);
			SendMessage(hWnd, WM_COMMAND, TMAKELPARAM(0, LBN_SELCHANGE), (LPARAM)PES->list.hWnd);
			return TRUE;
		}
	}

	// リスト表示なしの時の処理 -----------------------------------------------
	if ( PES->oldkey == 0 ){	// 新規サーチ？
		BackupText(hWnd, PES);
		PES->list.mode = PES->list.startmode =
				(*PES->list.OldString && SendMessage(hWnd, EM_GETMODIFY, 0, 0)) ?
				LIST_SEARCH : LIST_WRITE;
		PES->list.index = 0;
		repeat = 0;	// 初めての時はリピートを無視する
	}
	if ( PES->list.OldString == NULL ) return TRUE;	// バックアップ失敗時は不可
					// 現在の編集内容に戻ったとき、リピート時はサーチさせない
	if ( (PES->list.index == 0) &&
			(PES->list.mode == PES->list.startmode) && repeat ){
		return TRUE;
	}
	PES->list.index += offset;

	nowline[0] = '\0';
	SendMessage(PES->hWnd, WM_GETTEXT, TSIZEOF(nowline), (LPARAM)&nowline);

	UsePPx();
	for ( ; ; ){
		const TCHAR *hstr;

		if ( PES->list.index == 0 ){	// 一つ前の検索モードに戻る
			if ( PES->list.mode == PES->list.startmode ){
				SetWindowText(hWnd, PES->list.OldString);
				SendMessage(hWnd, EM_SETSEL, EC_LAST, EC_LAST);
				break;
			}
			PES->list.mode--;
			PES->list.index = CountHistory(
					(PES->list.mode != LIST_WRITE) ?
							PES->list.RhistID : PES->list.WhistID) * -offset;
			continue;
		}

		hstr = EnumHistory( // ↓LIST_READ の方がいいかも？
					(PES->list.mode != LIST_WRITE) ?
							PES->list.RhistID : PES->list.WhistID,
					ABS(PES->list.index) - 1);
		if ( hstr == NULL ){
				// 読込みヒストリモード以外なら別のに切り換えて再トライ
			if ( PES->list.mode < LIST_READ ){
				PES->list.mode++;
				PES->list.index = offset;
				continue;
			}
				// 読込みヒストリモードならサーチ終了
			PES->list.index -= offset;
			break;
		}
		if ( PES->list.mode == LIST_SEARCH ){
			size_t len;

			len = tstrlen(PES->list.OldString);
			if ( (len > tstrlen(hstr)) || tstrnicmp(PES->list.OldString, hstr, len) ){
				PES->list.index += offset;
				continue;
			}
		}

		if ( tstrcmp(nowline, hstr) == 0 ){ // 重複を削除
			PES->list.index += offset;
			continue;
		}

		SetWindowText(hWnd, hstr);
		if ( PES->list.WhistID != PPXH_COMMAND ){
			SendMessage(hWnd, EM_SETSEL, EC_LAST, EC_LAST);
		}else{
			SendMessage(hWnd, EM_SETSEL, GetCommandEnd(hstr), EC_LAST);
		}
		break;
	}
	FreePPx();
#if 0
	{
		TCHAR buf[100];

		wsprintf(buf, "Key:%d mode:%d index:%d"
					, PES->oldkey, PES->list.mode, PES->list.index);
		SetWindowText(GetParentCaptionWindow(hWnd), buf);
	}
#endif
	return TRUE;
}

// Combo box list のインクリメンタルサーチ
void ListSearch(HWND hWnd, PPxEDSTRUCT *PES, int len)
{
	int index, limit, offset,nlen;
	TCHAR text[0x1000];

	if ( PES->list.ListWindow == LISTU_NOLIST ){
		return;
	}

	nlen = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);
	if ( len == nlen ) return;

	if ( PES->list.mode >= LIST_FILL ){
		PostMessage(PES->list.hWnd, WM_CLOSE, 0, 0);
		return;
	}

	if ( GetWindowText(hWnd, text, TSIZEOF(text)) == 0 ) return;
	limit = SendMessage(PES->list.hWnd, LB_GETCOUNT, 0, 0) - 1;
	if ( PES->list.direction > 0 ){
		index = 0;
		offset = 1;
	}else{
		index = limit;
		offset = -1;
	}
	for ( ; (index >= 0) && (index <= limit) ; index += offset ){
		int llen;
		TCHAR ltext[0x1000];

		llen = SendMessage(PES->list.hWnd, LB_GETTEXTLEN, index, 0);
		if ( llen < len ) continue; // 検索側が長いので無視できる
		if ( SendMessage(PES->list.hWnd, LB_GETTEXT, index, (LPARAM)ltext)
					== LB_ERR ){
			return;
		}
		if ( tstrnicmp(text, ltext, len) == 0 ){
								// [↓]で選択できるように、一つ手前に移動する
			SendMessage(PES->list.hWnd, LB_SETTOPINDEX, index, 0);
			SendMessage(PES->list.hWnd, LB_SETCURSEL, index - 1, 0);
			break;
		}
	}
	return;
}

// 検索対象の長さを決定
const TCHAR *CalcTargetWordWidth(const TCHAR *text, LPCTSTR *last, int mode)
{
	TCHAR code;
	const TCHAR *first;
	int usebraket = 0;

	while ( (*text == ' ') || (*text == '\t') ) text++;
	first = text;
	if ( mode == TEXT_MASK ){ // マスク文字列用
		while ( (code = *text) != '\0' ){
			if ( (code == '\r') || (code == '\n') ) break;
			/*
			if ( (code == ';') && ((text > first) && ((*(text - 1) == ' ') || (*(text - 1) == '\t')) ) ){
				break;
			}
			*/
			text++;
		}
	}else while ( (code = *text) != '\0' ){ // コマンド・パス
		if ( (code == '\r') || (code == '\n') ) break;
		// if ( code == ';' ) break; // コメント
		/*if ( (code == '\\') && ((UTCHAR)*(text + 1) > ' ') ){ // パスは区切り以降を検索対象にする
			first = text + 1;
		}else */if ( mode != TEXT_PATH ){
			// くくりのない空白区切り
			if ( ((code == ' ') || (code == '\t')) && !usebraket ) break;
			if ( code == '\"' ){
				if ( usebraket ) break; // くくりの終わり
					// くくりの始まり
				usebraket = 1;
				first = text;
			}
		}
		#ifdef UNICODE
			text++;
		#else
			text += Chrlen(*text);
		#endif
	}
	*last = text;
	return first;
}

TCHAR *tmemimem(const TCHAR *source, const TCHAR *sourcelast, const TCHAR *word, int wordlen)
{
	const TCHAR *src, *dest, *destlast;

	if ( wordlen == 0 ) return (TCHAR *)source;
	sourcelast -= (wordlen - 1);
	destlast = word + wordlen;
	while ( source <= sourcelast ){
		src = source;
		dest = word;
		for ( ; ; ){
			TCHAR s, d;

			s = *src++;
			if ( Isupper(s) ) s += (TCHAR)0x20;
			d = *dest++;
			if ( Isupper(d) ) d += (TCHAR)0x20;
			if ( s != d ) break;
			if ( dest >= destlast ) return (TCHAR *)source;
		}
		source++;
	}
	return NULL;
}

LRESULT AddBraketTextToList(LISTADDINFO *list, const TCHAR *text, int braket)
{
	TCHAR buf[CMDLINESIZE], *p;

	if ( braket ){
		p = tstrchr(text, ' ');
		if ( (p != NULL) && (*(p + 1) != ';') ){	// 空白あり→ブラケット必要
			TCHAR *nline;

			nline = buf;
			if ( braket == (BRAKET_NONE + 1)) *nline++ = '\"'; // ブラケット無し
			tstrlimcpy(nline, text, CMDLINESIZE - 4);
			if ( braket != (BRAKET_LEFTRIGHT + 1) ) tstrcat(nline, T("\"")); // 右ブラケット無し
			text = buf;
		}
	}
	#ifdef UNICODE
		if ( ALIGNMENT_BITS(text) & 1 ){
			return SendUTextMessage_U(list->hWnd, list->msg, list->wParam, text);
		}else
	#endif
			return SendMessage(list->hWnd, list->msg, list->wParam, (LPARAM)text);
}

// 環境変数の一覧作成
void EnvironmentStringsSearch(LISTADDINFO *list, TCHAR *first, size_t firstlen, int braket, WORD mode)
{
	TCHAR *envptr;
	const TCHAR *p;
	TCHAR name[64], buf[96], *dst, *maxptr;
	int itemcount = 0;
	BOOL pathmode = (mode == PPXH_COMMAND) ? FALSE : TRUE;

	if ( firstlen > 63 ) return;
	if ( firstlen ){
		first++;
		firstlen--;
	}
	if ( (*first == '\'') && firstlen ){
		first++;
		firstlen--;
	}
	memcpy(name, first, firstlen * sizeof(TCHAR) );
	name[firstlen] = '\0';

	p = envptr = GetEnvironmentStrings();
	if ( p == NULL ) return;
	for ( ; *p != '\0' ; p += tstrlen(p) + 1 ){
		if ( *p == '=' ) continue;
		if ( tstristr(p,name) != NULL ){
			dst = buf;
			maxptr = buf + TSIZEOF(buf) - 10;

			*dst++ = '%';
			if ( pathmode == FALSE ) *dst++ = '\'';
			for ( ;; ){ // 名前をコピー
				if ( *p == '=' ){
					p++;
					break;
				}
				*dst++ = *p++;
				if ( dst >= maxptr ) break;
			}
			*dst++ = (TCHAR)(pathmode ? '%' : '\'');

			*dst++ = ' ';
			*dst++ = ';';
			*dst++ = ' ';

			while ( *p != '\0' ){ // 内容をコピー
				*dst++ = *p++;
				if ( dst >= maxptr ) break;
			}
			*dst = '\0';

			if ( LB_ERR == AddBraketTextToList(list, buf, braket) ) break;
			itemcount++;
		}
	}
	list->items += itemcount;
	FreeEnvironmentStrings(envptr);
}

// テキストファイルから一覧作成
void TextSearch(LISTADDINFO *list, FILLTEXTFILE *filltext, const TCHAR *first, size_t firstlen, const TCHAR *second, size_t secondlen, DWORD lastTick, int smode, int braket)
{
	TCHAR buf[CMDLINESIZE], buf2[VFPS];
	const TCHAR *text, *checkfirst, *checklast;
	TCHAR *dest;
	int itemcount = 0;

	if ( filltext->mem == NULL ){
		// マルチスレッドなので、imageptr が更新されても textptr が未設定のことがある→filltextの直接使用は危険
		TCHAR *imageptr = NULL, *textptr;

		MakeUserfilename(buf, CUSTNAME, PPxRegPath);
		tstrcpy( FindLastEntryPoint(buf), filltext->filename );
		if ( LoadTextImage(buf, &imageptr, &textptr, NULL) != NO_ERROR ){
			BOOL cancel = FALSE;

			*(VFSFullPath(buf2, (TCHAR *)filltext->filename, DLLpath) + 3) = 'F';
			CopyFileExL(buf2, buf, NULL, NULL, &cancel, COPY_FILE_FAIL_IF_EXISTS);
			if ( LoadTextImage(buf, &imageptr, &textptr, NULL) != NO_ERROR ){
				return;
			}
		}
		if ( filltext->mem == NULL ){ // ●別のスレッドに先を越された（クリティカルセッションを使う方向にした方がよい。）
			filltext->text = textptr;
			filltext->mem = imageptr;
		}else{
			HeapFree(ProcHeap, 0, imageptr);
		}
	}

	while ( (*first == '\\') && firstlen ){
		first++;
		firstlen--;
	}

	text = filltext->text;

	while ( *text != '\0' ){
		if ( (*text != ' ') && (*text != '\t') ){ // 行頭からのみ検索
			checkfirst = CalcTargetWordWidth(text, &checklast, smode);
/*
			{
				char ab[2000];
				memcpy(ab, checkfirst, checklast - checkfirst);
				ab[checklast - checkfirst] = '\0';
				XMessage(NULL,NULL, XM_DbgLOG, T("%s"), ab);


			}
*/
			if ( tmemimem(checkfirst, checklast, first, firstlen) != NULL ){
				if ( second == NULL ){ // １つめのみは前方一致
					// 一致したので一行取り出す
					dest = buf;
					while ( IsEOL(&text) ) *dest++ = *text++;
					*dest = '\0';

					if ( first == NilStrNC ){
						TCHAR *p = tstrrchr(buf, ';');

						if ( (p != NULL) && (p > buf) &&
							 ((UTCHAR)*(p - 1) <= ' ') ){
							*p = '\0';
						}
					}

					if ( LB_ERR == AddBraketTextToList(list, buf, braket) ){
						goto enumbreak;
					}
					itemcount++;
				// ２つめなら、完全単語一致
				}else{
					if ( (UTCHAR)*(text + firstlen) <= ' ' ){
					 	text += firstlen;
						if ( (*text == ' ') || (*text == '\t') ){
							text++;
							while ( (*text == ' ') || (*text == '\t') ) text++;
							if ( *text == ';' ){ // コメント表示
								text++;
								dest = buf;
								while ( IsEOL(&text) ) *dest++ = *text++;
								*dest = '\0';
								SetMessageForEdit(list->hWnd, buf);
							}
						}

						for ( ; ; ){
							// 次の行へ
							while ( IsEOL(&text) ) text++;
							if ( *text != '\0' ) text++;

							if ( (*text != ' ') && (*text != '\t') ) break;
							while ( (*text == ' ') || (*text == '\t') ) text++;

							if ( !tstrnicmp(second, text, secondlen) ){ // 前方一致したか
								// 一致したので取り出す
								dest = buf;
								while ( IsEOL(&text) ) *dest++ = *text++;
								*dest = '\0';

								if ( LB_ERR == AddBraketTextToList(list, buf, braket) ){
									goto enumbreak;
								}
								itemcount++;
							}
						}
					}
				}
			}else{
				text = checklast; // ここまでチェックしたのでスキップ
			}
		}
		// 次の行へ
		while ( IsEOL(&text) ) text++;
		if ( *text != '\0' ) text++;

		if ( GetTickCount() > lastTick ){
			SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)T("*** text search timeout ***"));
			break;
		}
	}
enumbreak:
	list->items += itemcount;
	return;
}

// ディレクトリから一覧作成
void EntrySearch(PPxEDSTRUCT *PES, LISTADDINFO *list, int mode, int braket, TCHAR *first, DWORD lastTick)
{
	int itemcount = 0;
	TCHAR *p;
	ESTRUCT ED;

	ED.hF = NULL;
	ED.info= PES->info;
	ED.romahandle = 0;
	ED.cmdsearch = 0;

	setflag(mode, CMDSEARCH_NOADDSEP);
	for ( ; ; ){
		p = SearchFileInedMain(&ED, first, mode);
		if ( p == NULL ) break;

		if ( LB_ERR == AddBraketTextToList(list, p, braket) ) break;
		itemcount++;

		if ( GetTickCount() > lastTick ){
			SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)T("*** timeout ***"));
			break;
		}
		memmove(first, p, TSTRSIZE(p));
	}
	list->items += itemcount;
	SearchFileIned(&ED, NilStrNC, NULL, 0);
	return;
}

// ヒストリから一覧作成
void HistorySearch(LISTADDINFO *list, WORD targethist, TCHAR *first, size_t firstlen, DWORD lastTick, int smode, int braket)
{
	int itemcount = 0, index;
	TCHAR textbuf[CMDLINESIZE];
	size_t histlen;

	for ( index = 0 ; ; index++ ){
		const TCHAR *hist;
		const TCHAR *checkfirst, *checklast;

		UsePPx();
		hist = EnumHistory(targethist, index);
		if ( hist == NULL ){
			FreePPx();
			break;
		}

		if ( GetTickCount() > lastTick ){
			FreePPx();
			SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)T("*** timeout ***"));
			break;
		}

		histlen = tstrlen(hist);
		if ( firstlen > histlen ){
			FreePPx();
			continue;
		}
		if ( histlen < CMDLINESIZE ){
			tstrcpy(textbuf, hist);
			hist = textbuf;
		}
		FreePPx();

		checkfirst = CalcTargetWordWidth(hist, &checklast, smode);
		if ( tmemimem(checkfirst, checklast, first, firstlen) == NULL ){
			// 前方一致
			if ( (checkfirst == hist) || tstrnicmp(hist, first, firstlen) ){
				continue;
			}
		}
		if ( LB_ERR == AddBraketTextToList(list, hist, braket) ) break;
		itemcount++;
	}
	list->items += itemcount;
	return;
}

DWORD_PTR USECDECL SearchCReportModuleFunction(SMODULECAPPINFO *sinfo, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	if ( (cmdID == PPXCMDID_REPORTSEARCH) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_FILE) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_DIRECTORY) ){
		if ( LB_ERR == SendMessage(sinfo->list->hWnd, sinfo->list->msg, (WPARAM)sinfo->list->wParam, (LPARAM)uptr->str) ){
			return 2;
		}
		sinfo->list->items++;
		return 1;
	}
	return sinfo->PES->info->Function(sinfo->PES->info, cmdID, uptr);
}

// 検索モジュールを使って一覧作成
void ModuleSearch(PPxEDSTRUCT *PES, LISTADDINFO *list, TCHAR *first, size_t firstlen)
{
	TCHAR msg[VFPS];
	PPXMSEARCHSTRUCT msearch;
	PPXMODULEPARAM pmp;
	SMODULECAPPINFO smca;
#ifndef UNICODE
	WCHAR keywordW[VFPS];
#endif
	if ( firstlen >= VFPS ) return;
	memcpy(msg, first, firstlen * sizeof(TCHAR));
	msg[firstlen] = '\0';

#ifndef UNICODE
	AnsiToUnicode(msg, keywordW, VFPS);
	msearch.keyword = keywordW;
#else
	msearch.keyword = msg;
#endif
	msearch.searchtype = PES->list.WhistID | PES->list.RhistID | PPXH_SEARCH_NAMEONLY;
	msearch.maxresults = 100;
	smca.info.Function = (PPXAPPINFOFUNCTION)SearchCReportModuleFunction;
	smca.info.Name = T("Edit");
	smca.info.RegID = NilStr;
	smca.info.hWnd = PES->hWnd;
	smca.list = list;
	smca.PES = PES;
	pmp.search = &msearch;
	CallModule(&smca.info, PPXMEVENT_SEARCH, pmp,NULL);
	return;
}

// エイリアスから一覧作成
void AliasSearch(LISTADDINFO *list, TCHAR *first, size_t firstlen, DWORD lastTick, int smode)
{
	int itemcount = 0, index = 0;
	TCHAR keyword[CMDLINESIZE], param[CMDLINESIZE];
	int offset = 0;

	if ( *first == '%' ){
		keyword[0] = '%';
		keyword[1] = '\'';
		offset = 2;
	}

	while(EnumCustTable(index++, T("A_exec"), keyword + offset, param, sizeof(param)) >= 0){
		const TCHAR *checkfirst, *checklast;

		if ( GetTickCount() > lastTick ){
			SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)T("*** timeout ***"));
			break;
		}

		if ( firstlen <= tstrlen(keyword) ){
			checkfirst = CalcTargetWordWidth(keyword, &checklast, smode);
			if ( tmemimem(checkfirst, checklast, first, firstlen) == NULL ){
				// 前方一致
				if ( (checkfirst == keyword) || tstrnicmp(keyword, first, firstlen) ){
					continue;
				}
			}
		}else{
			continue;
		}
		if ( offset != 0 ) tstrcat(keyword, T("'"));
		wsprintf(keyword + tstrlen(keyword), T(" ;%s"), param);
		if ( LB_ERR == SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)keyword) ){
			break;
		}
		itemcount++;
	}
	list->items += itemcount;
	return;
}

void AddCalcNumber(LISTADDINFO *list, TCHAR *first, size_t firstlen)
{
	TCHAR text[VFPS],result[VFPS + 16];
	int num;
	const TCHAR *ptr;

	if ( firstlen >= VFPS ) return;
	memcpy(text, first, firstlen * sizeof(TCHAR));
	text[firstlen] = '\0';
	ptr = text;
	if ( CalcString(&ptr, &num) != CALC_NOERROR ) return;
	ptr = text;
	while ( *ptr ){
		TCHAR c;

		c = *ptr++;
		if ( (c == ' ') || Isdigit(c) ) continue;
		break;
	}
	wsprintf(result, T("%d ;=%s"),num, text);
	SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)result);
	list->items++;
	wsprintf(result, T("%x ;(16)=%s"),num, text);
	SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)result);
	list->items++;
}

const TCHAR *FindCommandParamPoint(const TCHAR *line, DWORD cursorPos, size_t *secondPos)
{
	DWORD firstWordPos;
	const TCHAR *tempP;
	const TCHAR *p;

	if ( cursorPos == 0 ) return NULL;

	// カーソルが１番目の単語かを確認
	firstWordPos = 0;

	while ( line[firstWordPos] == ' ' ) firstWordPos++; // 単語前の空白をスキップ

	if ( firstWordPos >= cursorPos ) return NULL;
	// カーソルがコマンド区切りの直後なら最初の単語扱い
	tempP = line + cursorPos; // startP;

	while ( tempP > line ){
		TCHAR c;

		c = *(tempP - 1);

		if ( c == ' ' ){
			tempP--;
			continue;
		}
		if ( (c == ':') || (c == '|') || (c == '&') ) return NULL;
		break;
	}

	p = line + firstWordPos;
	while ( (UTCHAR)*p > ' ' ) p++;
	*secondPos = p - line - firstWordPos;

	return line + firstWordPos;
}

typedef struct { // KeyStepFillMain に情報を渡した後、廃棄される構造体
	PPxEDSTRUCT *PES;
	HANDLE hReadyEvent;
	BOOL histmode;
} KEYSTEPFILLMAIN_INFO;


DWORD WINAPI KeyStepFillMain(KEYSTEPFILLMAIN_INFO *ksfinfo)
{
	HWND hWnd;
	ECURSOR cursor;
	TCHAR line[CMDLINESIZE * 2];
	DWORD mode;

	int dbllist;
	TCHAR *startP;
	int braket = 0; // BRAKET_ に 1を足した値
	DWORD nwP;
	DWORD lastTick;
	size_t len;
	size_t firstWordLen;
	LISTADDINFO mainlist, sublist;
	DWORD ThreadID;

	PPxEDSTRUCT *PES;
	BOOL histmode, threadmode;

	PES = ksfinfo->PES;
	histmode = ksfinfo->histmode;
	PES->ActiveListThreadID = ThreadID = GetCurrentThreadId();

	if ( ksfinfo->hReadyEvent != NULL ){
		THREADSTRUCT threadstruct = {T("FillList"), XTHREAD_EXITENABLE /*| XTHREAD_TERMENABLE*/,NULL, 0, 0};

		PES->ListThreadCount++;
		SetEvent(ksfinfo->hReadyEvent); // 待機完了 / ksfinfo 解放指示
		PPxRegisterThread(&threadstruct);
		CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
		threadmode = TRUE;
		mainlist.wParam = sublist.wParam = ThreadID;
	}else{
		threadmode = FALSE;
		mainlist.wParam = sublist.wParam = 0;
	}
	hWnd = PES->hWnd;
	mode = PES->ED.cmdsearch & (CMDSEARCH_FLOAT | CMDSEARCH_ROMA);
	if ( X_flst[0] >= 5 ) mode |= CMDSEARCH_NOUNC;
												// サイズチェック
	line[0] = '\0';
	if ( IsTrue(histmode) ){
		cursor.start = cursor.end = 0;
	}else{
		if ( (DWORD)SendMessage(hWnd, WM_GETTEXT,
				TSIZEOF(line), (LPARAM)&line) >= TSIZEOF(line) ){
			goto fin;
		}
												// 編集文字列(全て)を取得
		GetEditSel(hWnd, line, &cursor);
	}
	mode |= CMDSEARCH_ONE | CMDSEARCH_EDITBOX |
			((PES->list.WhistID & PPXH_COMMAND) ?
						CMDSEARCH_CURRENT : CMDSEARCH_OFF) |
			((PES->flags & PPXEDIT_SINGLEREF) ? 0 : CMDSEARCH_MULTI);

	if ( (PES->list.WhistID & (PPXH_DIR | PPXH_PPCPATH)) &&
		 IsTrue(GetCustDword(T("X_fdir"), TRUE)) ){
		setflag(mode, CMDSEARCH_DIRECTORY);
	}
	if ( X_flst[1] != 0 ) setflag(mode, CMDSEARCH_FLOAT);

	PES->list.ListWindow = LISTU_FOCUSMAIN;
	mainlist.hWnd = PES->list.hWnd;
	mainlist.msg = LB_ADDSTRING;
	mainlist.items = 0;
	dbllist = (X_flst[0] >= 4) && !(PES->flags & PPXEDIT_COMBOBOX);
	if ( dbllist ){
		sublist.hWnd = PES->list.hSubWnd;
		sublist.msg = LB_INSERTSTRING;
		sublist.items = 0;
	}else{
		sublist = mainlist;
	}

		// 検索対象の切り出し
	if ( !(mode & CMDSEARCH_MULTI) ){ // 単一パラメータ
		cursor.start = 0;
		if ( X_flst[0] < 5 ){
			cursor.end = tstrlen32(line);
		}else{
			line[cursor.end] = '\0';
		}
	}else{
										// 範囲選択がされていない時の抽出処理 -
		if ( cursor.start == cursor.end ){
			// 空白区切りの１語を検索対象にする
			braket = GetWordStrings(line, &cursor) + 1;
			// -option:[n] の [n] を検索対象にする
			if ( (line[cursor.start] == '-') || (line[cursor.start] == '/') ){
				DWORD tmpstart = cursor.start + 1, cmdlen = 0;
				for (;;){
					TCHAR chr = line[tmpstart];

					if ( !Isalnum(chr) ) break;
					tmpstart++;
					cmdlen++;
				}
				if ( (cmdlen > 0) && (line[tmpstart] == ':') ){
					cursor.start = tmpstart + 1;
				}
			}
		}else{
			braket = 1;
		}
		line[cursor.end] = '\0';
	}
	PES->list.select.start = nwP = cursor.start;
	PES->list.select.end = cursor.end;
	if ( IsTrue(histmode) ){
		PES->list.select.end = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);
	}
#ifndef UNICODE
	if ( xpbug < 0 ){
		if ( IsTrue(histmode) ){
			PES->list.select.end = EC_LAST; // ヒストリ時は末尾であれば良い
							// ※ CaretFixToW に必要な line が用意していない
		}else{
			CaretFixToW(line, &PES->list.select.end);
		}
	}
#endif
	startP = line + nwP;

	// 補完一覧の作成 ------------------------------
	// オーバーフローは、補完がその場で終わるだけなので対処しない
	lastTick = GetTickCount() + (threadmode ? ThreadAllowTick : AllowTick);
	len = tstrlen(startP);

	if ( PES->list.filltext_user.mem != NULL ){
		const TCHAR *firstWordPtr;

		firstWordPtr = FindCommandParamPoint(line, nwP, &firstWordLen);
		if ( firstWordPtr == NULL ){
			TextSearch(&mainlist, &PES->list.filltext_user, startP, len, NULL, 0, lastTick, TEXT_PATH, 0);
		}else{
			TextSearch(&sublist, &PES->list.filltext_user, firstWordPtr, firstWordLen, startP, len, lastTick, TEXT_WORDPATH, 0);
		}
	}

	switch ( PES->list.WhistID ){
		case PPXH_COMMAND: {
			WORD histmask;
			const TCHAR *firstWordPtr;

			AddCalcNumber(&sublist, startP, len);

			firstWordPtr = FindCommandParamPoint(line, nwP, &firstWordLen);
			if ( firstWordPtr == NULL ){ // 最初の単語ならコマンドの補完を行う
				HistorySearch(&sublist, PPXH_COMMAND, startP, len, lastTick, TEXT_WORDPATH, 0);
				AliasSearch(&sublist, startP, len, lastTick, TEXT_WORDPATH);
				TextSearch(&sublist, &filltext_cmd, startP, len,NULL, 0, lastTick, TEXT_WORDPATH, 0);
				EntrySearch(PES, &mainlist, mode, braket, startP, GetTickCount() + AllowTick);
				histmask = (WORD)~PPXH_COMMAND;
			}else{
				TextSearch(&sublist, &filltext_cmd, firstWordPtr, firstWordLen, startP, len, lastTick, TEXT_WORDPATH, 0);
				EntrySearch(PES, &mainlist, mode, braket, startP, GetTickCount() + AllowTick);
				HistorySearch(&sublist, PPXH_DIR_R, startP, len, lastTick, TEXT_WORDPATH, braket);
				TextSearch(&mainlist, &filltext_path, startP, len,NULL, 0, lastTick, TEXT_PATH, braket);
				AliasSearch(&sublist, startP, len, lastTick, TEXT_WORDPATH);
				histmask = (WORD)~PPXH_DIR_R;
			}
			ModuleSearch(PES, &mainlist, startP, len);
			HistorySearch(&sublist, PES->list.RhistID & (WORD)~histmask, startP, len, lastTick, TEXT_PATH, 0);
			if ( *startP == '%' ){
				EnvironmentStringsSearch(&mainlist, startP, len, braket, PES->list.WhistID);
			}
			break;
		}
		case PPXH_MASK:
			HistorySearch(&mainlist, PPXH_MASK, startP, len, lastTick, TEXT_MASK, 0);
			TextSearch(&sublist, &filltext_mask, startP, len,NULL, 0, lastTick, TEXT_MASK, 0);
			HistorySearch(&sublist, PES->list.RhistID & (WORD)~PPXH_MASK, startP, len, lastTick, TEXT_PATH, 0);
			break;

		case PPXH_GENERAL:
			AddCalcNumber(&sublist, startP, len);

		case PPXH_DIR:
		case PPXH_FILENAME:
		case PPXH_PATH:

		case PPXH_PPCPATH:
		case PPXH_PPVNAME:
			HistorySearch(&mainlist, PES->list.WhistID, startP, len, lastTick, TEXT_PATH, 0);
			TextSearch(&sublist, &filltext_path, startP, len, NULL, 0, lastTick, TEXT_PATH, 0);
			EntrySearch(PES, &mainlist, mode, braket, startP, GetTickCount() + AllowTick);
			ModuleSearch(PES, &mainlist, startP, len);
			HistorySearch(&sublist, PES->list.RhistID & (WORD)~PES->list.WhistID, startP, len, lastTick, TEXT_PATH, 0);
			if ( *startP == '%' ){
				EnvironmentStringsSearch(&mainlist, startP, len, braket, PES->list.WhistID);
			}
			break;

		default:
			HistorySearch(&mainlist, PES->list.RhistID, startP, len, lastTick, TEXT_WORDPATH, 0);
			break;
	}
	if ( PES->ActiveListThreadID != ThreadID ) goto fin;

	//-----------------------
	if ( dbllist ){
		if ( sublist.items == 0 ){
			ShowWindow(sublist.hWnd, SW_HIDE);
		}else{
			// 一番最後を表示し、カーソルを表示させない
			SendMessage(sublist.hWnd, LB_SETCURSEL, SendMessage(sublist.hWnd, LB_GETCOUNT, 0, 0) - 1, 0);
			SendMessage(sublist.hWnd, LB_SETCURSEL, (WPARAM)-1, 0);
			SendMessage(sublist.hWnd, WM_SETREDRAW, TRUE, 0);
			ShowWindow(sublist.hWnd, SW_SHOWNA);
		}
	}else{
		mainlist.items += sublist.items;
	}

	if ( mainlist.items == 0 ){
		ShowWindow(mainlist.hWnd, SW_HIDE);
		if ( sublist.items == 0 ) PES->list.ListWindow = LISTU_NOLIST;
	}else{
		SendMessage(mainlist.hWnd, WM_SETREDRAW, TRUE, 0);
		ShowWindow(mainlist.hWnd, SW_SHOWNA);
	}

	BackupText(hWnd, PES);
	if ( (X_flst[0] >= 5) && (PES->list.OldString != NULL) ){
		DWORD lP;

		lP = PES->list.OldEnd;
		CaretFixToA(PES->list.OldString, &lP);
		if ( *(PES->list.OldString + lP) == '.' ){
			PES->list.mode = PES->list.startmode = LIST_FILLEXT;
		}
	}
fin:
	if ( PES->ActiveListThreadID == ThreadID ) PES->ActiveListThreadID = 0;

	if ( threadmode == FALSE ) return 0;
	PES->ListThreadCount--;
	CoUninitialize();
	PPxUnRegisterThread();
	return 0;
}

void CancelListThread(PPxEDSTRUCT *PES)
{
	PES->ActiveListThreadID = 0;

	for ( ;; ){
		MSG msg;

		if ( PES->ListThreadCount == 0 ) break;

		while ( PeekMessage(&msg, PES->hWnd, 0, 0, PM_REMOVE) ){
			if ( msg.message == WM_QUIT ) break;
//			TranslateMessage(&msg); // wm_char 不要
			DispatchMessage(&msg);
		}
		Sleep(20);
	}
}

void KeyStepFill(PPxEDSTRUCT *PES, BOOL histmode)
{
	KEYSTEPFILLMAIN_INFO ksfinfo;
	MSG msginfo;

	ksfinfo.PES = PES;
	ksfinfo.histmode = histmode;
	ksfinfo.hReadyEvent = NULL;
	PES->ActiveListThreadID = 0;

	if ( IsTrue(PeekMessage(&msginfo, PES->hWnd, WM_CHAR, WM_CHAR, PM_NOREMOVE)) ){
		return; // 更に入力があるので、その時に処理する
	}
	if ( IsTrue(PeekMessage(&msginfo, PES->hWnd, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE)) ){
		if ( (msginfo.wParam == VK_RETURN) || ((msginfo.wParam >= '0') && (msginfo.wParam <= 'Z')) ){
			return; // 更に入力があるので、その時に処理する
		}
	}
	if ( PES->ListThreadCount > 20 ) return; // スレッド数が多すぎるので中止

	// メインリストウィンドウ作成
	if ( PES->list.hWnd == NULL ){
		int direction;

		PES->list.mode = PES->list.startmode = LIST_FILL;
		direction = (PES->flags & PPXEDIT_COMBOBOX) ? -1 : 1;

		CreateMainListWindow(PES, direction);
	}else{
		SendMessage(PES->list.hWnd, WM_SETREDRAW, FALSE, 0);
		SendMessage(PES->list.hWnd, LB_RESETCONTENT, TRUE, 0);
	}
	// サブリストウィンドウ作成
	if ( (X_flst[0] >= 4) &&
		 !(PES->flags & PPXEDIT_COMBOBOX) ){
		if ( PES->list.hSubWnd == NULL ){
			CreateSubListWindow(PES);
		}else{
			SendMessage(PES->list.hSubWnd, WM_SETREDRAW, FALSE, 0);
			SendMessage(PES->list.hSubWnd, LB_RESETCONTENT, TRUE, 0);
		}
	}

	#ifndef WINEGCC
	#ifndef _WIN64
	if ( OSver.dwMajorVersion >= 5 ) // 次の行に続く
	#endif
	{
		HANDLE hThread;

		ksfinfo.hReadyEvent = CreateEvent(NULL, TRUE, FALSE,NULL);
		hThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE)KeyStepFillMain,
				&ksfinfo, 0, (DWORD *)&PES->ActiveListThreadID);
		if ( hThread != NULL ){
			WaitForSingleObject(ksfinfo.hReadyEvent, INFINITE);
			CloseHandle(ksfinfo.hReadyEvent);
			SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
			CloseHandle(hThread);
			return;
		}
		CloseHandle(ksfinfo.hReadyEvent);
		ksfinfo.hReadyEvent = NULL;
	}
	#endif
	KeyStepFillMain(&ksfinfo);
}


HWND PPxRegistExEditCombo(HWND hED, int maxlen, const TCHAR *defstr, WORD rHist, WORD wHist)
{
	int i;
	const TCHAR *p;
	POINT LP = {4, 4};
	HWND hRealED;
	LISTADDINFO list = {NULL, CB_ADDSTRING, 0, 0};

	SendMessage(hED, CB_LIMITTEXT, maxlen - 1, 0);
	if ( *defstr != '\0' ){
		SendMessage(hED, CB_ADDSTRING, 0, (LPARAM)defstr);
		SendMessage(hED, CB_SETCURSEL, 0, 0);
	}
									// ヒストリ内容を登録
	UsePPx();
	for ( i = 0 ; i < 100 ; i++ ){
		if ( (p = EnumHistory((WORD)wHist, i)) == NULL ) break;
		SendMessage(hED, CB_ADDSTRING, 0, (LPARAM)p);
	};
	FreePPx();

	list.hWnd = hED;
	TextSearch(&list, &filltext_mask,NilStrNC, 0,NULL, 0, GetTickCount() + AllowTick, TEXT_MASK, 0);
	UsePPx();
	for ( ; i < 100 ; i++ ){
		if ( (p = EnumHistory((WORD)(rHist & ~wHist), i)) == NULL ) break;
		SendMessage(hED, CB_ADDSTRING, 0, (LPARAM)p);
	};
	FreePPx();

	for ( ; ; ){
		hRealED = ChildWindowFromPoint(hED, LP);
		if ( hRealED == NULL ) break;
		if ( hRealED == hED ){
			LP.x += 2;
			LP.y += 2;
			continue;
		}
/* 効果が無いので停止中
		if ( hRealED != NULL ){
			SetWindowLong(hRealED, GWL_STYLE,
					GetWindowLong(hRealED, GWL_STYLE) | ES_NOHIDESEL);
		}
*/
		break;
	}
	return hRealED;
}
