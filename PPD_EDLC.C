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

#define OnAllowTick 100 // 補完列挙を中止する時間
#define ThreadAllowTick 3000 // 補完列挙を中止する時間(スレッド使用時)
#define CompListItems 100 // 一覧表示する最大数

struct {
	DWORD showtime, stoptime;
} X_flto = { OnAllowTick, 0};

/*
	コンボボックスモード
						  入力時   表示後入力
		1:ヒストリ一覧		 -          -
		2:ファイル補完一覧	 -		頭文字検索
		3:ヒストリ補完一覧  一覧    一覧再作成
*/

TCHAR BTERR[] = T("BackupText err");
const TCHAR StrTimeOut[] = T("*** timeout ***");

#define ABS(n) (n >= 0 ? n : -n)

typedef struct {
	HWND hWnd;
	UINT msg;
	DWORD wParam; // ThreadID が格納される
	int items;
	int show;
} LISTADDINFO;

FILLTEXTFILE filltext_cmd = { T("PPXUCMD.TXT"), NULL, NULL/*, {0, 0}*/ };
FILLTEXTFILE filltext_path = { T("PPXUPATH.TXT"), NULL, NULL/*, {0, 0}*/ };
FILLTEXTFILE filltext_mask = { T("PPXUMASK.TXT"), NULL, NULL/*, {0, 0}*/ };

typedef struct {
	PPXAPPINFO info;
	LISTADDINFO *list;
	PPxEDSTRUCT *PES;
} SMODULECAPPINFO;

#define SMODE_PATH 0
#define SMODE_WORDPATH 1
#define SMODE_MASK 2

typedef struct {
	DWORD_PTR x_handle;
	TCHAR *first, *second;
	size_t firstlen, secondlen;
	DWORD startTick;
	DWORD edmode;
} SEARCHPARAMS;


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

BOOL MakeCompleteList(HWND hWnd, PPxEDSTRUCT *PES)
{
	ECURSOR cursor;
	TCHAR *ptr;
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
	fmode = (PES->ED.cmdsearch & (CMDSEARCH_NOADDSEP | CMDSEARCH_FLOAT | CMDSEARCH_ROMA | CMDSEARCH_WILDCARD)) |
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

	ptr = SearchFileIned(&PES->ED, buf, &cursor, fmode | CMDSEARCHI_SAVEWORD);
	if ( ptr == NULL ){
		SetMessageForEdit(PES->hWnd, MES_EENF);
		SendMessage(PES->hWnd, WM_SETREDRAW, TRUE, 0);
		return FALSE;
	}
	// １回目結果を反映
	SendMessage(PES->hWnd, EM_SETSEL, cursor.start, cursor.end);
							// ※↑SearchFileIned 内で加工済み
	SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)ptr);
	SendMessage(PES->hWnd, EM_SETSEL, 0, 0);	// 表示開始桁を補正させる

	samelen = len = (DWORD)tstrlen(ptr);
#ifndef UNICODE
	if ( xpbug < 0 ) CaretFixToW(ptr, &len);
#endif
	PES->list.select.start = cursor.start;
	PES->list.select.end = cursor.start + len;

	SendMessage(PES->hWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(PES->hWnd, NULL, FALSE);
	tstrcpy(buf2, ptr);
	tstrcpy(buf, PES->ED.Fname);

	// ブラケット処理済みは、これ以上重複処理させない
	if ( PES->ED.cmdsearch & CMDSEARCHI_FINDBRAKET ){
		resetflag(fmode, CMDSEARCH_MULTI);
	}
	// ２回目検索
	cursor.start = 0;
	cursor.end   = tstrlen32(buf);
	ptr = SearchFileIned(&PES->ED, buf, &cursor, fmode);
	if ( ptr == NULL ){
		PostMessage(hWnd, WM_CLOSE, 0, 0);
	}else{
		DWORD starttime;

		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)(PES->list.OldString)); // Undo用
		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)buf2); // １回目
		SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)ptr); // ２回目
		samelen = CheckSamelen(buf2, ptr);
		starttime = GetTickCount();
		// ３回目以降検索
		for ( ; ; ){
			tstrcpy(buf, PES->ED.Fname);
			cursor.start = 0;
			cursor.end   = tstrlen32(buf);
			ptr = SearchFileIned(&PES->ED, buf, &cursor, fmode);
			if ( ptr == NULL ) break;
			SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)ptr);
			samelen = CheckSamelen(buf2, ptr);
			if ( (GetTickCount() - starttime) > OnAllowTick ){
				SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)T("more..."));
				break;
			}
		}
	}
	if ( PES->flags & CMDSEARCHI_SELECTPART ){
	#ifndef UNICODE
		if ( xpbug < 0 ) CaretFixToW(buf2, &samelen);
	#endif
	}else{
		samelen = len;
	}
	SendMessage(PES->hWnd, EM_SETSEL,
			PES->list.select.start + samelen, PES->list.select.start + len);
	SetMessageForEdit(PES->hWnd, NULL);
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
	for ( i = 0 ; i < CompListItems ; i++ ){
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
	for ( ; i < CompListItems ; i++ ){
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
	box.right -= box.left;
	if ( X_flst[2] != 0 ){
		int listwidth;

		listwidth = PES->fontY * X_flst[2] / 2;
		if ( box.right < listwidth ) box.right = listwidth;
		if ( box.right > (deskbox.right - deskbox.left) ){
			box.right = deskbox.right - deskbox.left;
		}
		if ( (box.left + box.right) > deskbox.right ){
			box.left = deskbox.right - box.right;
		}
		if ( box.left < deskbox.left ){
			box.left = deskbox.left;
		}
	}
	hListWnd = CreateWindowEx(
			(WinType >= WINTYPE_2000) ? WS_EX_NOACTIVATE : 0,
			ListBoxClassName, NilStr,
/* ↓窓枠が太くなる分左右位置がずれる＆上下サイズを変えると高さが縮むので中止
			(WinType >= WINTYPE_10) ?
				(WS_THICKFRAME | WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY) :
				(WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY),
*/
			WS_BORDER | WS_POPUP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY,
			box.left, box.bottom, box.right, ListHeight,
			PES->hWnd, NULL, DLLhInst, NULL);
	FixUxTheme(hListWnd, ListBoxClassName);
	SetWindowLongPtr(hListWnd, GWLP_USERDATA, (LONG_PTR)PES);
	SendMessage(hListWnd, WM_SETFONT, SendMessage(PES->hWnd, WM_GETFONT, 0, 0), 0);

	if ( direction <= 0 ){	// 上側配置のとき、フォントサイズに合うよう高さが
							// 変化して位置ずれ起きることあるので修正
		GetWindowRect(hListWnd, &deskbox);
		ListHeight -= (deskbox.bottom - deskbox.top);
		if ( ListHeight > 0 ){
			SetWindowPos(hListWnd, NULL, box.left, box.bottom + ListHeight,
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
	if ( X_flst[2] != 0 ) parentbox.left = listbox.left;
	SetWindowPos(PES->list.hWnd, NULL, parentbox.left,
		PES->list.direction > 0 ?
			parentbox.bottom : parentbox.top - (listbox.bottom - listbox.top),
		0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

	if ( PES->list.hSubWnd != NULL ){
		GetWindowRect(PES->list.hSubWnd, &listbox);
		SetWindowPos(PES->list.hSubWnd, NULL, parentbox.left,
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
			if ( PES->flags & PPXEDIT_TEXTEDIT ) return FALSE;	// PPe は未対応
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

//			if ( (PES->flags & PPXEDIT_TEXTEDIT) && (SendMessage(PES->list.hWnd, LB_GETCOUNT, 0, 0) <= 0) ) return FALSE;	// PPe は未対応

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
	int index, limit, offset, nlen;
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
#if 1 // 深いことはしない用
#define SMODEPARAM(smode)
const TCHAR *CalcTargetWordWidth(const TCHAR *text, LPCTSTR *last)
{
	TCHAR code;
	const TCHAR *first;

	while ( (*text == ' ') || (*text == '\t') ) text++;
	first = text;

	while ( (code = *text) != '\0' ){
		if ( (code == '\r') || (code == '\n') ) break;
		text++;
	}
	*last = text;
	return first;
}
#else
#define SMODEPARAM(smode), smode
const TCHAR *CalcTargetWordWidth(const TCHAR *text, LPCTSTR *last, int mode)
{
	TCHAR code;
	const TCHAR *first;
	int usebraket = 0;

	while ( (*text == ' ') || (*text == '\t') ) text++;
	first = text;
	if ( mode == SMODE_MASK ){ // マスク文字列用
		while ( (code = *text) != '\0' ){
			if ( (code == '\r') || (code == '\n') ) break;
			#if 0 // コメントを検索対象から外す時用
			if ( (code == ';') && ((text > first) && ((*(text - 1) == ' ') || (*(text - 1) == '\t')) ) ){
				break;
			}
			#endif
			text++;
		}
	}else while ( (code = *text) != '\0' ){ // コマンド・パス
		if ( (code == '\r') || (code == '\n') ) break;
		#if 0 // コメントを検索対象から外す時用
		if ( code == ';' ) break; // コメント
		#endif
		#if 0 // パス形式の時、最終エントリ以降に限定するとき
		if ( (code == '\\') && ((UTCHAR)*(text + 1) > ' ') ){
			first = text + 1;
		}else
		#endif
		#if 0 // 空白区切り以降を検索対象から外す
		if ( mode != SMODE_PATH ){
			// くくりのない空白区切り
			if ( ((code == ' ') || (code == '\t')) && !usebraket ) break;
			if ( code == '\"' ){
				if ( usebraket ) break; // くくりの終わり
					// くくりの始まり
				usebraket = 1;
				first = text;
			}
		}
		#endif
		#ifdef UNICODE
			text++;
		#else
			text += Chrlen(*text);
		#endif
	}
	*last = text;
	return first;
}
#endif

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

#define tstrcpartcmp(src, static_find_str) memcmp(src, static_find_str, TSIZEOFSTR(static_find_str))

const TCHAR ShellDirRegStr[] = T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FolderDescriptions");
const TCHAR ShellDirNameStr[] = T("Name");

void ShellDriveSearch(LISTADDINFO *list, const TCHAR *first, int braket)
{
	HKEY hShellDirKey, hSubKey;
	int count = 0;
	TCHAR KeyName[128], buf[VFPS];
	const TCHAR *search;
	DWORD rsize;
	FILETIME write;

	if ( (first[0] != 's') || (first[1] != 'h') || (first[2] != 'e') ){
		return;
	}
	search = NilStr;
	if ( first[3] != '\0' ){
		if ( first[3] != 'l' ) return;
		if ( first[4] != '\0' ){
			if ( first[4] != 'l' ) return;
			search = (first[5] == ':') ? (first + 6) : (first + 5);
		}
	}

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, ShellDirRegStr, 0, KEY_READ, &hShellDirKey)){
		return;
	}
	for ( ; ; count++ ){					// 設定を取り出す ---------------------
		rsize = TSIZEOF(KeyName);
		if ( ERROR_SUCCESS != RegEnumKeyEx(hShellDirKey, count, KeyName, &rsize, NULL, NULL, NULL, &write) ){
			break;
		}

		if ( ERROR_SUCCESS != RegOpenKeyEx(hShellDirKey, KeyName, 0, KEY_READ, &hSubKey)){
			continue;
		}

		rsize = TSIZEOF(KeyName);
		if ( ERROR_SUCCESS == RegQueryValueEx(hSubKey, ShellDirNameStr, NULL, NULL, (LPBYTE)KeyName, &rsize) ){
			if ( tstristr(KeyName, search) != NULL ){
				wsprintf(buf, T("shell:%s"), KeyName);
				if ( LB_ERR == AddBraketTextToList(list, buf, braket) ) break;

				list->items++;
			}
		}
		RegCloseKey(hSubKey);
	}
	RegCloseKey(hShellDirKey);
}

// auxの一覧作成
void AuxSearch(LISTADDINFO *list, const TCHAR *first)
{
	int count, size;
	const TCHAR *search;
	TCHAR name[VFPS], buf[CMDLINESIZE];

	if ( (first[0] != 'a') || (first[1] != 'u') || (first[2] != 'x')  ){
		return;
	}
	search = (first[3] == ':') ? (first + 4) : (first + 3);

	for ( count = 0 ; ; count++ ){
		size = EnumCustData(count, name, NULL, 0);
		if ( 0 > size ) break;

		if ( ((name[0] != 'M') && (name[0] != 'S')) ||
			 (tstrcpartcmp(name + 1, T("_aux")) != 0) ){
			continue;
		}
		if ( tstrstr(name, search) == NULL ) continue;

		if ( GetCustTable(name, T("base"), buf, sizeof(buf)) == NO_ERROR ){
			tstrreplace(buf, T(" %; "), T(" ; "));
		}else{
			wsprintf(buf, T("aux:%s"), name);
		}

		if ( LB_ERR == SendMessage(list->hWnd, list->msg, list->wParam, (LPARAM)buf) ){
			break;
		}
		list->items++;
	}
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
		if ( tstristr(p, name) != NULL ){
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

void FreeExtraSearch(SEARCHPARAMS *spms)
{
	if ( spms->x_handle > 1 ){
		if ( spms->edmode & CMDSEARCH_WILDCARD ){
			FreeFN_REGEXP((FN_REGEXP *)spms->x_handle);
			HeapFree(DLLheap, 0, (void *)spms->x_handle);
		}else if ( spms->edmode & CMDSEARCH_ROMA ){
			SearchRomaString(NULL, NULL, 0, &spms->x_handle);
		}
	}
	spms->x_handle = 0;
}

const TCHAR WordSearchOption[] = T("o:xe,&*"); // wildcard 用
const TCHAR WordSearchOption_R[] = T("o:xe,&r:"); // wildcard + roma
const TCHAR WordSearchDummy[] = T("!*"); // 常に失敗用

void MakeWordSearch(SEARCHPARAMS *spms, const TCHAR *searchstr)
{
	TCHAR text[0x1000];
	FN_REGEXP *fr;

	fr = (FN_REGEXP *)spms->x_handle;

	if ( tstrlen(searchstr) >= MAX_PATH ){
		MakeFN_REGEXP(fr, WordSearchDummy); // 常に失敗の内容にする
		return;
	}

	if ( spms->edmode & CMDSEARCH_ROMA ){
		tstrcpy(text, WordSearchOption_R);
		tstrcpy(text + TSIZEOFSTR(WordSearchOption_R), searchstr);
		tstrreplace(text + TSIZEOFSTR(WordSearchOption_R), T("."), T(",&r:"));
	}else{
/*
		 // wildcard だけの時、and 指定が無いときは失敗に
		if ( (tstrlen(searchstr) >= MAX_PATH) ||
			 ((tstrchr(searchstr, '.') == NULL) && (spms->startTick != 3)) ){
			MakeFN_REGEXP(fr, WordSearchDummy); // 常に失敗の内容にする
			return;
		}
*/
		tstrcpy(text, WordSearchOption);
		tstrcpy(text + TSIZEOFSTR(WordSearchOption), searchstr);
		tstrreplace(text + TSIZEOFSTR(WordSearchOption), T("."), T("*,&*"));
		tstrcat(text + TSIZEOFSTR(WordSearchOption), T("*"));
	}
	MakeFN_REGEXP(fr, text);
}
/*
void InitSearchString(SEARCHPARAMS *spms, const TCHAR *searchstr)
{
	if ( !(spms->edmode & (CMDSEARCH_ROMA | CMDSEARCH_WILDCARD) ) ) return;
	if ( spms->edmode & CMDSEARCH_WILDCARD ){
		spms->x_handle = (DWORD_PTR)HeapAlloc(DLLheap, 0, sizeof(FN_REGEXP));
		MakeWordSearch(spms, searchstr);
	}else{
		SearchRomaString(NilStr, searchstr, ISEA_FLOAT, &spms->x_handle);
	}
}
*/
BOOL ExtraSearchString(const TCHAR *text, TCHAR *textlast, const TCHAR *searchstr, SEARCHPARAMS *spms)
{
	if ( ! (spms->edmode & (CMDSEARCH_ROMA | CMDSEARCH_WILDCARD) ) ) return FALSE;
	if ( spms->x_handle == 1 ) return FALSE;
	if ( spms->edmode & CMDSEARCH_WILDCARD ){
		if ( spms->x_handle == 0 ){
			spms->x_handle = (DWORD_PTR)HeapAlloc(DLLheap, 0, sizeof(FN_REGEXP));
			MakeWordSearch(spms, searchstr);
		}

		if ( textlast != NULL ){
			TCHAR backup;
			BOOL result;

			backup = *textlast;
			*textlast = '\0';

			result = FilenameRegularExpression(text, (FN_REGEXP *)spms->x_handle);
			*textlast = backup;
			return result;
		}else{
			return FilenameRegularExpression(text, (FN_REGEXP *)spms->x_handle);
		}
	}else{ // CMDSEARCH_ROMA
		if ( textlast != NULL ){
			TCHAR backup;
			BOOL result;

			backup = *textlast;
			*textlast = '\0';

			result = SearchRomaString(text, searchstr, ISEA_FLOAT, &spms->x_handle);
			*textlast = backup;
			return result;
		}else{
			return SearchRomaString(text, searchstr, ISEA_FLOAT, &spms->x_handle);
		}
	}
}

BOOL ExtraSearchStringS(const TCHAR *text, ESTRUCT *ED)
{
	SEARCHPARAMS spms;
	BOOL result;

	if ( ED->romahandle == 1 ){ // 検索初期化失敗のときは簡易検索
		return (tstristr(text, ED->Fword) != NULL);
	}

	spms.edmode = ED->cmdsearch;
	spms.x_handle = ED->romahandle;
	spms.startTick = 3; // 識別用

	result = ExtraSearchString(text, NULL, ED->Fword, &spms);
	ED->romahandle = spms.x_handle;
	if ( spms.x_handle <= 1 ){ // 検索初期化失敗のときは簡易検索
		spms.x_handle = 1;
		return (tstristr(text, ED->Fword) != NULL);
	}
	return result;
}

BOOL CheckFillTimeout(LISTADDINFO *list, SEARCHPARAMS *spms)
{
	DWORD nowtick = GetTickCount();

	// 表示時間以内
	if ( list->show == 0 ){
		if ( (nowtick - spms->startTick) <= X_flto.showtime ) return FALSE;
		if ( list->items > 0 ){
			SendMessage(list->hWnd, WM_SETREDRAW, TRUE, 0);
			ShowWindow(list->hWnd, SW_SHOWNA);
			list->show = 1;
		}
	}

	// TimeOut
	if ( (nowtick - spms->startTick) <= X_flto.stoptime ) return FALSE;
	SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)StrTimeOut);
	return TRUE;
}

// テキストファイルから一覧作成
void TextSearch(LISTADDINFO *list, FILLTEXTFILE *filltext, SEARCHPARAMS *spms SMODEPARAM(smode), int braket)
{
	TCHAR buf[CMDLINESIZE], buf2[VFPS];
	TCHAR *text;
	const TCHAR *checkfirst;
	TCHAR *checklast;
	TCHAR *first;
	TCHAR *dest;
	int itemcount = 0;
	size_t firstlen;

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
	first = spms->first;
	firstlen = spms->firstlen;
	while ( (*first == '\\') && firstlen ){
		first++;
		firstlen--;
	}

	text = filltext->text;
	while ( *text != '\0' ){
		if ( (*text != ' ') && (*text != '\t') ){ // 行頭からのみ検索
			checkfirst = CalcTargetWordWidth(text, (const TCHAR **) &checklast SMODEPARAM(smode));
/*
			{
				char ab[2000];
				memcpy(ab, checkfirst, checklast - checkfirst);
				ab[checklast - checkfirst] = '\0';
				XMessage(NULL, NULL, XM_DbgLOG, T("%s"), ab);


			}
*/
			if ( (tmemimem(checkfirst, checklast, first, firstlen) != NULL) ||
				 ExtraSearchString(checkfirst, checklast, first, spms) ){
				if ( spms->second == NULL ){ // １つめのみは前方一致
					// 一致したので一行取り出す
					dest = buf;
					while ( IsEOL((const TCHAR **)&text) ) *dest++ = *text++;
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
								while ( IsEOL((const TCHAR **)&text) ) *dest++ = *text++;
								*dest = '\0';
								SetMessageForEdit(list->hWnd, buf);
							}
						}

						for ( ; ; ){
							// 次の行へ
							while ( IsEOL((const TCHAR **)&text) ) text++;
							if ( *text != '\0' ) text++;

							if ( (*text != ' ') && (*text != '\t') ) break;
							while ( (*text == ' ') || (*text == '\t') ) text++;

							if ( !tstrnicmp(spms->second, text, spms->secondlen) ){ // 前方一致したか
								// 一致したので取り出す
								dest = buf;
								while ( IsEOL((const TCHAR **)&text) ) *dest++ = *text++;
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
		while ( IsEOL((const TCHAR **)&text) ) text++;
		if ( *text != '\0' ) text++;

		if ( IsTrue(CheckFillTimeout(list, spms)) ) break;
	}
enumbreak:
	list->items += itemcount;
	return;
}

// ディレクトリから一覧作成 ※ spms の first 内の文字列の firstlen 以降が破壊される
void EntrySearch(PPxEDSTRUCT *PES, LISTADDINFO *list, SEARCHPARAMS *spms, int braket)
{
	int itemcount = 0;
	TCHAR *ptr;
	ESTRUCT ED;
	DWORD mode;

	ED.hF = NULL;
	ED.info= PES->info;
	ED.romahandle = spms->x_handle; // 親のを利用
	ED.cmdsearch = 0;
	mode = spms->edmode | CMDSEARCH_NOADDSEP;
	for ( ; ; ){
		ptr = SearchFileInedMain(&ED, spms->first, mode);
		if ( ptr == NULL ) break;

		if ( LB_ERR == AddBraketTextToList(list, ptr, braket) ) break;
		itemcount++;

		if ( IsTrue(CheckFillTimeout(list, spms)) ) break;
		memmove(spms->first, ptr, TSTRSIZE(ptr));
	}
	list->items += itemcount;
	spms->x_handle = ED.romahandle;
	ED.romahandle = 0; // 親で解放させる
	SearchFileIned(&ED, NilStrNC, NULL, 0);
	return;
}

// ヒストリから一覧作成
void HistorySearch(LISTADDINFO *list, SEARCHPARAMS *spms, WORD targethist SMODEPARAM(smode), int braket)
{
	int itemcount = 0, index;
	TCHAR textbuf[CMDLINESIZE];
	size_t histlen;

	for ( index = 0 ; ; index++ ){
		const TCHAR *hist;
		const TCHAR *checkfirst;
		TCHAR *checklast;

		UsePPx();
		hist = EnumHistory(targethist, index);
		if ( hist == NULL ){
			FreePPx();
			break;
		}

		if ( IsTrue(CheckFillTimeout(list, spms)) ) {
			FreePPx();
			break;
		}

		histlen = tstrlen(hist);
		if ( spms->firstlen > histlen ){
			FreePPx();
			continue;
		}
		if ( histlen >= CMDLINESIZE ){
			FreePPx();
			continue;
		}
		tstrcpy(textbuf, hist);
		hist = textbuf;
		FreePPx();

		checkfirst = CalcTargetWordWidth(hist, (const TCHAR **)&checklast SMODEPARAM(smode));
		if ( tmemimem(checkfirst, checklast, spms->first, spms->firstlen) == NULL ){
			if ( ExtraSearchString(checkfirst, checklast, spms->first, spms) ){
			}else if ( (checkfirst == hist) || tstrnicmp(hist, spms->first, spms->firstlen) ){ // 前方一致
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
	msearch.maxresults = PPXMSEARCH_SHORTRESULT;
	smca.info.Function = (PPXAPPINFOFUNCTION)SearchCReportModuleFunction;
	smca.info.Name = WC_EDIT;
	smca.info.RegID = NilStr;
	smca.info.hWnd = PES->hWnd;
	smca.list = list;
	smca.PES = PES;
	pmp.search = &msearch;
	CallModule(&smca.info, PPXMEVENT_SEARCH, pmp, NULL);
	return;
}

// エイリアスから一覧作成
void AliasSearch(LISTADDINFO *list, SEARCHPARAMS *spms SMODEPARAM(smode))
{
	int itemcount = 0, index = 0;
	TCHAR keyword[CMDLINESIZE], param[CMDLINESIZE];
	int offset = 0;

	if ( *spms->first == '%' ){
		keyword[0] = '%';
		keyword[1] = '\'';
		offset = 2;
	}

	while( EnumCustTable(index++, T("A_exec"), keyword + offset, param, sizeof(param)) >= 0){
		const TCHAR *checkfirst, *checklast;

		if ( IsTrue(CheckFillTimeout(list, spms)) ) break;

		if ( spms->firstlen <= tstrlen(keyword) ){
			checkfirst = CalcTargetWordWidth(keyword, &checklast SMODEPARAM(smode));
			if ( tmemimem(checkfirst, checklast, spms->first, spms->firstlen) == NULL ){
				// 前方一致
				if ( (checkfirst == keyword) || tstrnicmp(keyword, spms->first, spms->firstlen) ){
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
	TCHAR text[VFPS], result[VFPS + 16];
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
	wsprintf(result, T("%d ;=%s"), num, text);
	SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)result);
	list->items++;
	wsprintf(result, T("%x ;(16)=%s"), num, text);
	SendMessage(list->hWnd, list->msg, (WPARAM)list->wParam, (LPARAM)result);
	list->items++;
}

TCHAR *FindCommandParamPoint(TCHAR *line, DWORD cursorPos, size_t *secondPos)
{
	DWORD firstWordPos;
	TCHAR *tempP;
	TCHAR *p;

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
	TCHAR linebuf[CMDLINESIZE * 2], *line;

	int dbllist;
	TCHAR *startP;
	int braket = 0; // BRAKET_ に 1を足した値
	DWORD nwP;
	size_t len;
	size_t firstWordLen;
	LISTADDINFO mainlist, sublist;
	DWORD ThreadID;

	PPxEDSTRUCT *PES;
	BOOL histmode, threadmode;
	SEARCHPARAMS spms, spms2;

	PES = ksfinfo->PES;
	histmode = ksfinfo->histmode;
	PES->ActiveListThreadID = ThreadID = GetCurrentThreadId();

	if ( ksfinfo->hReadyEvent != NULL ){ // Sub thread mode
		THREADSTRUCT threadstruct = {T("FillList"), XTHREAD_EXITENABLE /*| XTHREAD_TERMENABLE*/, NULL, 0, 0};

		PES->ListThreadCount++;
		SetEvent(ksfinfo->hReadyEvent); // 待機完了 / ksfinfo 解放指示
		PPxRegisterThread(&threadstruct);
		CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
		threadmode = TRUE;
		mainlist.wParam = sublist.wParam = ThreadID;
	}else{ // on thread mode
		threadmode = FALSE;
		mainlist.wParam = sublist.wParam = 0;
	}
	hWnd = PES->hWnd;
	spms.edmode = PES->ED.cmdsearch & (CMDSEARCH_FLOAT | CMDSEARCH_ROMA | CMDSEARCH_WILDCARD);
	if ( X_flst[0] >= 5 ) spms.edmode |= CMDSEARCH_NOUNC;
												// サイズチェック
	line = linebuf;
	linebuf[0] = '\0';
	if ( IsTrue(histmode) ){
		cursor.start = cursor.end = 0;
	}else{
		if ( (DWORD)SendMessage(hWnd, WM_GETTEXT,
				CMDLINESIZE, (LPARAM)&linebuf) >= CMDLINESIZE ){
			DWORD getlen = (DWORD)SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);
			if ( (getlen >= 0x20000) || (WinType < WINTYPE_VISTA) ) goto fin2;
			line = (TCHAR *)HeapAlloc(DLLheap, 0, TSTROFF(getlen) + CMDLINESIZE);
			if ( line == NULL ) goto fin2;
			SendMessage(hWnd, WM_GETTEXT, getlen + 8, (LPARAM)line);
		}
												// 編集文字列(全て)を取得
		GetEditSel(hWnd, line, &cursor);
	}
	spms.edmode |= CMDSEARCH_ONE | CMDSEARCH_EDITBOX |
			((PES->list.WhistID & PPXH_COMMAND) ?
					CMDSEARCH_CURRENT : CMDSEARCH_OFF) |
			((PES->flags & PPXEDIT_SINGLEREF) ? 0 : CMDSEARCH_MULTI);

	if ( (PES->list.WhistID & (PPXH_DIR | PPXH_PPCPATH)) &&
		 IsTrue(GetCustDword(T("X_fdir"), TRUE)) ){
		setflag(spms.edmode, CMDSEARCH_DIRECTORY);
	}
	if ( X_flst[1] != 0 ) setflag(spms.edmode, CMDSEARCH_FLOAT);

	PES->list.ListWindow = LISTU_FOCUSMAIN;
	mainlist.hWnd = PES->list.hWnd;
	mainlist.msg = LB_ADDSTRING;
	mainlist.items = 0;
	mainlist.show = 0;
	dbllist = (X_flst[0] >= 4) && !(PES->flags & PPXEDIT_COMBOBOX);
	if ( dbllist ){
		sublist.hWnd = PES->list.hSubWnd;
		sublist.msg = LB_INSERTSTRING;
		sublist.items = 0;
		sublist.show = 0;
	}else{
		sublist = mainlist;
		sublist.show = 1;
	}

		// 検索対象の切り出し
	if ( !(spms.edmode & CMDSEARCH_MULTI) ){ // 単一パラメータ
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
	// Tickオーバーフローは、補完がその場で終わるだけなので対処しない
	len = tstrlen(startP);

	spms.x_handle = spms2.x_handle = 0;
	spms.first = startP;
	spms.firstlen = len;
	spms.second = 0;
	spms.secondlen = 0;
	spms.startTick = GetTickCount();

	if ( PES->list.filltext_user.mem != NULL ){
		TCHAR *firstWordPtr;

		firstWordPtr = FindCommandParamPoint(line, nwP, &firstWordLen);
		if ( firstWordPtr == NULL ){
			TextSearch(&mainlist, &PES->list.filltext_user, &spms SMODEPARAM(SMODE_PATH), 0);
		}else{
			spms2.first = firstWordPtr;
			spms2.firstlen = firstWordLen;
			spms2.second = startP;
			spms2.secondlen = len;
			spms2.startTick = spms.startTick;
			spms2.edmode = spms.edmode;
			TextSearch(&sublist, &PES->list.filltext_user, &spms2 SMODEPARAM(SMODE_WORDPATH), 0);
		}
	}

	switch ( PES->list.WhistID ){
		case PPXH_COMMAND: {
			WORD histmask;
			TCHAR *firstWordPtr;

			AddCalcNumber(&sublist, startP, len);

			firstWordPtr = FindCommandParamPoint(line, nwP, &firstWordLen);
			if ( firstWordPtr == NULL ){ // 最初の単語ならコマンドの補完を行う
				HistorySearch(&sublist, &spms, PPXH_COMMAND SMODEPARAM(SMODE_WORDPATH), 0);
				AliasSearch(&sublist, &spms SMODEPARAM(SMODE_WORDPATH));
				TextSearch(&sublist, &filltext_cmd, &spms SMODEPARAM(SMODE_WORDPATH), 0);
				spms.startTick = GetTickCount();
				EntrySearch(PES, &mainlist, &spms, braket);
				*(startP + len) = '\0'; // EntrySearch 内で破壊されるので修復
				histmask = (WORD)~PPXH_COMMAND;
			}else{ // ２語目以降はパラメータ扱い
				spms2.first = firstWordPtr;
				spms2.firstlen = firstWordLen;
				spms2.second = startP;
				spms2.secondlen = len;
				spms2.startTick = spms.startTick;
				spms2.edmode = spms.edmode;
				TextSearch(&sublist, &filltext_cmd, &spms2 SMODEPARAM(SMODE_WORDPATH), 0);

				spms.startTick = GetTickCount();
				EntrySearch(PES, &mainlist, &spms, braket);
				*(startP + len) = '\0'; // EntrySearch 内で破壊されるので修復
				HistorySearch(&sublist, &spms, PPXH_DIR_R SMODEPARAM(SMODE_WORDPATH), braket);
				TextSearch(&mainlist, &filltext_path, &spms SMODEPARAM(SMODE_PATH), braket);
				AliasSearch(&sublist, &spms SMODEPARAM(SMODE_WORDPATH));
				histmask = (WORD)~PPXH_DIR_R;
			}
			ModuleSearch(PES, &mainlist, startP, len);
			HistorySearch(&sublist, &spms, PES->list.RhistID & (WORD)~histmask SMODEPARAM(SMODE_PATH), 0);
			if ( *startP == '%' ){
				EnvironmentStringsSearch(&mainlist, startP, len, braket, PES->list.WhistID);
			}
			if ( firstWordPtr != NULL ){
				AuxSearch(&mainlist, startP);
			}
			ShellDriveSearch(&mainlist, startP, braket);
			break;
		}
		case PPXH_MASK:
			HistorySearch(&mainlist, &spms, PPXH_MASK SMODEPARAM(SMODE_MASK), 0);
			TextSearch(&sublist, &filltext_mask, &spms SMODEPARAM(SMODE_MASK), 0);
			HistorySearch(&sublist, &spms, PES->list.RhistID & (WORD)~PPXH_MASK SMODEPARAM(SMODE_PATH), 0);
			break;

		case PPXH_GENERAL:
			AddCalcNumber(&sublist, startP, len);

		case PPXH_DIR:
		case PPXH_FILENAME:
		case PPXH_PATH:

		case PPXH_PPCPATH:
		case PPXH_PPVNAME:
			HistorySearch(&mainlist, &spms, PES->list.WhistID SMODEPARAM(SMODE_PATH), 0);
			TextSearch(&sublist, &filltext_path, &spms SMODEPARAM(SMODE_PATH), 0);
			spms.startTick = GetTickCount();
			EntrySearch(PES, &mainlist, &spms, braket);
			*(startP + len) = '\0'; // EntrySearch 内で破壊されるので修復
			ModuleSearch(PES, &mainlist, startP, len);
			HistorySearch(&sublist, &spms, PES->list.RhistID & (WORD)~PES->list.WhistID SMODEPARAM(SMODE_PATH), 0);
			if ( *startP == '%' ){
				EnvironmentStringsSearch(&mainlist, startP, len, braket, PES->list.WhistID);
			}
			AuxSearch(&mainlist, startP);
			ShellDriveSearch(&mainlist, startP, braket);
			break;

		default:
			HistorySearch(&mainlist, &spms, PES->list.RhistID SMODEPARAM(SMODE_WORDPATH), 0);
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
	if ( spms.x_handle != 0 ) FreeExtraSearch(&spms);
	if ( spms2.x_handle != 0 ) FreeExtraSearch(&spms2);
fin2:
	if ( PES->ActiveListThreadID == ThreadID ) PES->ActiveListThreadID = 0;
	if ( line != linebuf ) HeapFree(DLLheap, 0, line);

	if ( threadmode == FALSE ) return 0;
	PES->ListThreadCount--;
	CoUninitialize();
	PPxUnRegisterThread();
	return 0;
}

void CancelListThread(PPxEDSTRUCT *PES)
{
	#define ThreadChkSleepTime 20
	int WaitTimer = 8000 / ThreadChkSleepTime;

	PES->ActiveListThreadID = 0;

	for ( ;; ){
		MSG msg;

		if ( PES->ListThreadCount == 0 ) break;

		while ( PeekMessage(&msg, PES->hWnd, 0, 0, PM_REMOVE) ){
			if ( msg.message == WM_QUIT ) break;
//			TranslateMessage(&msg); // wm_char 不要
			DispatchMessage(&msg);
		}
		Sleep(ThreadChkSleepTime);
		if ( --WaitTimer <= 0 ) break;
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
	if ( PES->ListThreadCount > 20 ){
#ifndef RELEASE
		XMessage(NULL, NULL, XM_DbgLOG, T("> KeyStepFill 20 Thread"));
		SetMessageForEdit(PES->hWnd, T("> KeyStepFill 20 Thread"));
#endif
		return; // スレッド数が多すぎるので中止
	}

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

	if ( X_flto.stoptime == 0 ){
		GetCustData(T("X_flto"), &X_flto, sizeof(X_flto));

		#ifndef _WIN64
		if ( WinType < WINTYPE_2000 ) {
			X_flto.stoptime = OnAllowTick;
		}else
		#endif
		{
			X_flto.stoptime = ThreadAllowTick;
		}
	}

	#ifndef WINEGCC
	#ifndef _WIN64
	if ( WinType >= WINTYPE_2000 ) // 次の行に続く
	#endif
	{
		HANDLE hThread;

		ksfinfo.hReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
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
	LISTADDINFO list = {NULL, CB_ADDSTRING, 0, 0, 1};
	SEARCHPARAMS spms;

	SendMessage(hED, CB_LIMITTEXT, maxlen - 1, 0);
	if ( *defstr != '\0' ){
		SendMessage(hED, CB_ADDSTRING, 0, (LPARAM)defstr);
		SendMessage(hED, CB_SETCURSEL, 0, 0);
	}
									// ヒストリ内容を登録
	UsePPx();
	for ( i = 0 ; i < CompListItems ; i++ ){
		if ( (p = EnumHistory((WORD)wHist, i)) == NULL ) break;
		SendMessage(hED, CB_ADDSTRING, 0, (LPARAM)p);
	};
	FreePPx();

	list.hWnd = hED;
	spms.x_handle= 0;
	spms.startTick = GetTickCount();
	spms.first = NilStrNC;
	spms.firstlen = 0;
	spms.second = NULL;
	spms.secondlen = 0;

	TextSearch(&list, &filltext_mask, &spms SMODEPARAM(SMODE_MASK), 0);
	UsePPx();
	for ( ; i < CompListItems ; i++ ){
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
