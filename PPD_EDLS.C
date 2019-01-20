/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						拡張エディット
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPD_DEF.H"
#include "PPD_EDL.H"
#pragma hdrstop

TCHAR SEPSPACE[] = CAPTIONSEPARATOR; // タイトルバーに表示するメッセージの表示間隔
TCHAR SEPSPACEFMT[] = CAPTIONSEPARATOR T("%s");

PPXDLL void PPXAPI SetMessageOnCaption(HWND hWnd,const TCHAR *message)
{
	TCHAR buf[CMDLINESIZE],*p;

	buf[0] = '\0';
	GetWindowText(hWnd,buf,TSIZEOF(buf));
	p = tstrstr(buf,SEPSPACE);	// 区切りを求める
	if ( message != NULL ){
		message = MessageText(message);
		if ( p == NULL ) p = buf + tstrlen(buf);
		wsprintf(p,SEPSPACEFMT,message);
	}else{
		if ( p == NULL ) return;
		if ( *(p + 8) == '@' ){
			*(p + 8) = ' ';
		}else{
			*p = '\0';
		}
	}
	SetWindowText(hWnd,buf);
}

void USEFASTCALL SetMessageForEdit(HWND hWnd,const TCHAR *message)
{
	HWND hParentWnd = GetParent(hWnd);

	if ( GetWindowLongPtr(hParentWnd,GWL_STYLE) & WS_CAPTION ){
		SetMessageOnCaption(hParentWnd,message);
	}
}

TCHAR *AllocEditTextBuf(HWND hWnd,int *len)
{
	TCHAR *ptr;

	*len = GetWindowTextLength(hWnd);
	if ( *len > MB ){
		xmessage(XM_GrERRld,MES_EOLT);
		return NULL;
	}
	ptr = HeapAlloc(DLLheap,0,TSTROFF(*len + 1));
	if ( ptr == NULL ){
		xmessage(XM_GrERRld,MES_ENOM);
	}
	return ptr;
}

#ifndef UNICODE
// Multi(ANSI) 仕様→Wide 仕様 に修正
void USEFASTCALL CaretFixToW(const char *str,DWORD *of)
{
	int i;
	const char *last;

	last = str + *of;
	for ( i = 0 ; str < last ; i++ ){
		if ( *str == '\0' ) break;
		if ( IskanjiA( *str ) ) str++;
		str++;
	}
	*of = i;
}

// Wide 仕様から→ Multi(ANSI) 仕様に修正
void USEFASTCALL CaretFixToA(const char *str,DWORD *of)
{
	int i;
	const char *ptr;

	if ( xpbug >= 0 ) return;

	ptr = str;
	for ( i = *of ; i ; i-- ){
		if ( *ptr == '\0' ) break;
		if ( IskanjiA( *ptr ) ) ptr++;
		ptr++;
	}
	*of = ptr - str;
}
#endif

#ifndef UNICODE
void GetEditSel(HWND hWnd,TCHAR *buf,ECURSOR *cursor)
{
	SendMessage(hWnd,EM_GETSEL,(WPARAM)&cursor->start,(LPARAM)&cursor->end);
	if ( xpbug < 0 ){
		CaretFixToA(buf,&cursor->start);
		CaretFixToA(buf,&cursor->end);
	}
}

void SetEditSel(HWND hWnd,TCHAR *buf,DWORD start,DWORD end)
{
	if ( xpbug < 0 ){
		CaretFixToW(buf,&start);
		CaretFixToW(buf,&end);
	}
	SendMessage(hWnd,EM_SETSEL,start,end);
}
#endif

BOOL InitPPeFindReplace(PPxEDSTRUCT *PES)
{
	PES->findrep = (struct PPeFindReplace *)HeapAlloc(DLLheap,0,sizeof(struct PPeFindReplace));
	if ( PES->findrep == NULL ) return FALSE;

	PES->findrep->findtext[0] = '\0';
	PES->findrep->replacetext[0] = '\0';
	return TRUE;
}

BOOL SearchStr(PPxEDSTRUCT *PES,int mode)
{
	TCHAR *edittext,*maxptr,*ptr,*find = NULL,*findstr;
	int len;
	DWORD slen;
	ECURSOR cursor;

	if ( PES->findrep == NULL ){
		if ( InitPPeFindReplace(PES) == FALSE ) return FALSE;
	}
	findstr = PES->findrep->findtext;

	if ( (mode == 0) || (findstr[0] == '\0') ){
		if ( tInput(PES->hWnd,T("Find string"),findstr,
				TSIZEOF(PES->findrep->findtext) - 1,PPXH_SEARCH,PPXH_SEARCH) <= 0 ){
			return FALSE;
		}
		if ( findstr[0] == '\0' ) return FALSE;
	}

	edittext = AllocEditTextBuf(PES->hWnd,&len);
	if ( edittext == NULL ) return FALSE;
	GetWindowText(PES->hWnd,edittext,len);

	maxptr = edittext + len - 1;
	slen = tstrlen32(findstr);

	SendMessage(PES->hWnd,EM_GETSEL,(WPARAM)&cursor.start,(LPARAM)&cursor.end);
	CaretFixToA(edittext,&cursor.start);
	if ( mode >= 0 ){ // 進む
		maxptr -= slen;
#ifdef UNICODE
		for ( ptr = edittext + cursor.start + 1 ; ptr < maxptr ; ptr++ ){
#else
		ptr = edittext + cursor.start;
		ptr += Chrlen(*ptr);
		for ( ; ptr < maxptr ; ptr += Chrlen(*ptr) ){
#endif
			if ( tstrnicmp(ptr,findstr,slen) == 0 ){
				find = ptr;
				break;
			}
		}
	}else{ // 戻る
		for ( ptr = edittext + cursor.start - 1 ; ptr >= edittext ; ptr-- ){
			if ( tstrnicmp(ptr,findstr,slen) == 0 ){
				find = ptr;
				break;
			}
		}
	}

	if ( find != NULL ){
		cursor.start = find - edittext;
		SetEditSel(PES->hWnd,edittext,cursor.start,cursor.start + slen);
		SendMessage(PES->hWnd,EM_SCROLLCARET,0,0);
		HeapFree(DLLheap,0,edittext);
		return TRUE;
	}else{
		HeapFree(DLLheap,0,edittext);
		return FALSE;
	}
}

// ファイル検索処理 -----------------------------------------------------------
BOOL SearchFileInedInit(ESTRUCT *ED,TCHAR *str,WIN32_FIND_DATA *ff)
{
	PPXCMDENUMSTRUCT work;
	TCHAR spath[VFPS],cdir[VFPS],word[MAX_PATH];
	TCHAR *slashp,*sepp;
	DWORD length;

	length = tstrlen32(str);
	if ( length >= VFPS ) return FALSE; // 全体が長すぎ

	while ( (slashp = tstrchr(str,'/')) != NULL ) *slashp = '\\';

	tstrcpy(spath,str);
	sepp = FindLastEntryPoint(spath);
	if ( (length - (sepp - spath)) >= 256 ) return FALSE; // ファイル名長すぎ(これ以上長いと APIで落ちる)

	tstrcpy(word,sepp);
	*sepp = '*';
	*(sepp + 1) = '\0';

	tstrcpy(ED->Fname,str);

	if ( spath[0] == '%' ){
		if ( spath[1] == '\'' ){
			PP_ExtractMacro(NULL,NULL,NULL,spath,cdir,XEO_DISPONLY);
		}else{
			ExpandEnvironmentStrings(spath,cdir,VFPS);
		}
		tstrcpy(spath,cdir);
	}

	PPxEnumInfoFunc(ED->info,'1',cdir,&work);
	if ( NULL == VFSFullPath(NULL,spath,cdir) ) return FALSE;
	if ( (ED->cmdsearch & CMDSEARCH_NOUNC) && (spath[0] == '\\') ){
		return FALSE; // UNC は検索対象外
	}
	ED->hF = FindFirstFileL(spath,ff);
	if ( ED->hF == INVALID_HANDLE_VALUE ) return FALSE;

	tstrcpy(ED->Fsrc,spath);
	ED->Fword = ED->Fsrc + tstrlen(ED->Fsrc) + 1;
	tstrcpy(ED->Fword,word);
									// 検索文字列からパスを分離
	ED->FnameP = FindLastEntryPoint(ED->Fname);
	return TRUE;
}

TCHAR *SearchFileInedMain(ESTRUCT *ED,TCHAR *str,int mode)
{
	WIN32_FIND_DATA ff;
	int limit = 3;

	for ( ; ; ){
		if ( ED->hF != NULL ){					// 前回に検索を行っている場合…
			if ( tstrcmp(str,ED->Fname) == 0 ){	// 検索直後なら検索続行
				if ( FindNextFile(ED->hF,&ff) == FALSE ){
												// 最後までやったので最初から
					FindClose(ED->hF);
/*					現在、うまく伝達できないので休止中
					if ( !(mode & CMDSEARCH_ONE) ){
						SetMessageForEdit(ED->info->hWnd,MES_EENF);
					}
*/
					if ( (mode & CMDSEARCH_ONE) &&
							!(ED->cmdsearch & CMDSEARCH_CURRENT) ){
						goto abort;
					}

					if ( mode & CMDSEARCH_CURRENT ){	// コマンド検索切替
						ED->cmdsearch ^= CMDSEARCH_CURRENT;
					}else{
						limit--;
					}

					limit--;
					if ( limit <= 0 ) goto abort;
					ED->hF = FindFirstFileL(ED->Fsrc,&ff);
					if ( ED->hF == INVALID_HANDLE_VALUE ){
//						SetMessageForEdit(ED->info->hWnd,T("retry error"));
					 	goto abort;	// 検索失敗
					}
				}
			}else{							// 検索直後でないなら新規検索指定
				FindClose(ED->hF);
				ED->hF = NULL;
				if ( mode & CMDSEARCHI_FINDFIRST ) goto abort;
			}
		}
		if ( ED->hF == NULL ){					// 新規検索
			ED->cmdsearch = mode & ~CMDSEARCHI_FINDFIRST;
			if ( SearchFileInedInit(ED,str,&ff) == FALSE ) goto abort; //C4701ok
			setflag(mode,CMDSEARCHI_FINDFIRST);
		}
		if ( ED->cmdsearch & CMDSEARCH_DIRECTORY ){
			if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) continue; //C4701ok
		}

		if ( ED->cmdsearch & CMDSEARCH_FLOAT ){
			if ( tstristr(ff.cFileName,ED->Fword) == NULL ) continue;
		}else{
			TCHAR bkchr,*p;

			p = ff.cFileName + tstrlen(ED->Fword);
			bkchr = *p;
			*p = '\0';
			if ( tstricmp(ff.cFileName,ED->Fword) != 0 ) continue;
			*p = bkchr;
		}
											// 検索成功
		if ( ED->cmdsearch & CMDSEARCH_CURRENT ){
			TCHAR buf[0x400];
			FN_REGEXP fn;
			int result;

			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;

			buf[0] = '\0';
			ExpandEnvironmentStrings(T("%PATHEXT%"),buf,
					TSIZEOF(buf) - TSIZEOF(EXTPATHEXT) - 1);
			if ( buf[0] != '\0' ) tstrcat(buf,T(";"));
			tstrcat(buf,T(EXTPATHEXT));
			MakeFN_REGEXP(&fn,buf);
			result = FinddataRegularExpression(&ff,&fn);
			FreeFN_REGEXP(&fn);
			if ( result ){
				break;
			}else{
				continue;
			}
		}

		if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			if ( IsRelativeDirectory(ff.cFileName) ) continue;
			if ( !(ED->cmdsearch & CMDSEARCH_NOADDSEP) ){
				tstrcat(ff.cFileName,T("\\"));
			}
		}
		break;
	};

	tstrcpy(ED->FnameP,ff.cFileName);
	return ED->Fname;

abort:
	ED->hF = NULL;
	if ( !(mode & CMDSEARCH_ONE) ) XBeep(XB_NiERR);
	return NULL;
}

PPXDLL TCHAR * PPXAPI SearchFileIned(ESTRUCT *ED,TCHAR *line,ECURSOR *cursor,int mode)
{
	TCHAR *p;
	int braket = BRAKET_NONE;
	DWORD nwP;

	if ( !(mode & CMDSEARCH_MULTI) ){
		cursor->start = 0;
		cursor->end   = tstrlen32(line);
	}else{
										// 範囲選択がされていない時の抽出処理 -
		if ( cursor->start == cursor->end ){
			braket = GetWordStrings(line,cursor);
		}
		line[cursor->end] = '\0';
	}
	nwP = cursor->start;
#ifndef UNICODE
	if ( (mode & CMDSEARCH_EDITBOX ) && (xpbug < 0) ){
		CaretFixToW(line,&cursor->start);
		CaretFixToW(line,&cursor->end);
	}
#endif
	if ( mode & CMDSEARCHI_SAVEWORD ){
		if ( braket ) setflag( mode,CMDSEARCHI_FINDBRAKET );
		tstrcpy(ED->FnameP,line + nwP);
	}
	p = SearchFileInedMain(ED,line + nwP,mode);

	if ( p == NULL ) return NULL;	// 検索失敗
									// 検索成功
	if ( mode & CMDSEARCH_MULTI ){
		if ( tstrchr(p,' ') != NULL ){	// 空白あり→ブラケット必要
			TCHAR *nline;

			nline = line;
			if ( braket == BRAKET_NONE ) *nline++ = '\"'; // ブラケット無し
			tstrcpy(nline,p);
			if ( braket != BRAKET_LEFTRIGHT ) tstrcat(nline,T("\"")); // 右ブラケット無し
			p = line;
		}
	}
	return p;
}

/*-----------------------------------------------------------------------
	テキストスタック(新しいものほど後ろに配置される)

	1st:削除した方法
			0:EOD
			B0	0:BS	1:DEL
			B1	1=選択状態
			B7	1
	2nd:削除した文字列
----------------------------------------------------------------------*/
// テキストスタックに保存 -----------------------------------------------------
PPXDLL void PPXAPI PushTextStack(TCHAR mode,TCHAR *text)
{
	TCHAR *p,*b;
	int size;	// 保存に必要な大きさ

	if ( !text ) return;
	size = tstrlen32(text) + 1 + 1 + 1;	// 文字列 + 削除方法 + EOD
	if ( size <= 3 ) return; // 空
	if ( size > TextStackSize ) return; // 入りきらない

	UsePPx();
	b = NULL;							// 保存する場所を決める
	for ( p = Sm->TextStack ; *p ; p += tstrlen(p) + 1){
		if ( !b && ( (p - Sm->TextStack) >= size) ){
			b = p;
		}
	}
										// 容量チェック
	if ( (TextStackSize - (p - Sm->TextStack)) < size ){
		if ( b != NULL ){
			memmove(Sm->TextStack,b,TSTROFF(p - b));
			p -= b - Sm->TextStack;
		}else{
			p = Sm->TextStack;
		}
	}
	*p = (UTCHAR)(mode | B7);
	tstrcpy(p + 1,text);
	*(p + size - 1) = 0;
	FreePPx();
}

// テキストスタックから取り出す -----------------------------------------------
PPXDLL void PPXAPI PopTextStack(TCHAR *mode,TCHAR *text)
{
	TCHAR *ptr,*b;
										// 末尾を捜す
	UsePPx();
	for ( b = ptr = Sm->TextStack ; *ptr ; ptr += tstrlen(ptr) + 1 ) b = ptr;
	*mode = (*b) & (TCHAR)~B7;
	if (*b){
		tstrcpy(text,b + 1);
		*b = '\0';
	}else{
		*text = '\0';
	}
	FreePPx();
}

BOOL USEFASTCALL SelectEditStringsS(PPxEDSTRUCT *PES,TEXTSEL *ts,int mode)
{
	DWORD len;

	len = SendMessage(PES->hWnd,WM_GETTEXT,EDITBUFSIZE,(LPARAM)ts->text);
	if ( len >= EDITBUFSIZE ) return FALSE;		// 入りきらない
	SendMessage(PES->hWnd,EM_GETSEL,(WPARAM)&ts->cursor.start,(LPARAM)&ts->cursor.end);
	ts->cursororg = ts->cursor;
#ifndef UNICODE
	if ( xpbug < 0 ){						// XP bug 回避
		CaretFixToA(ts->text,&ts->cursor.start);
		CaretFixToA(ts->text,&ts->cursor.end);
	}
#endif
	if ( (ts->cursor.start >= EDITBUFSIZE) || (ts->cursor.end >= EDITBUFSIZE) ){
		return FALSE;
	}

	if ( ts->cursor.start < ts->cursor.end ){			// 範囲選択あり
		ts->text[ts->cursor.end] = '\0';
		ts->word = ts->text + ts->cursor.start;
		return TRUE;
	}else{						// 範囲選択なし
		TCHAR *top,*end;

		top = end = ts->text + ts->cursor.start;
		switch ( mode ){
			case TEXTSEL_CHAR:	// Del
				#ifdef UNICODE
					end += 1;
				#else
					end += Chrlen(*end);
				#endif
				break;
			case TEXTSEL_BACK:	// BS
				top -= bchrlen(ts->text,top - ts->text);
				break;
			case TEXTSEL_WORD:	// Word
				while( (UTCHAR)*end > ' ' ) end++;
				break;
			case TEXTSEL_BEFORE:	// カーソルより前
				top = ts->text;
				break;
			case TEXTSEL_AFTER:	// カーソルより後ろ
				end += tstrlen(end);
				break;
//			case TEXTSEL_ALL:	// 全選択
		}
		*end = 0;

		SetEditSel(PES->hWnd,ts->text,top - ts->text,end - ts->text);
		ts->word = top;
		return TRUE;
	}
}

BOOL USEFASTCALL SelectEditStringsM(PPxEDSTRUCT *PES,TEXTSEL *ts,int mode)
{
	size_t line,topline,bufsize;
	LRESULT wLP,wBP;
	TCHAR *destptr;
	TCHAR tmp[0x1000];
	DWORD pos;
#ifndef UNICODE
	DWORD pos2;
#endif
	SendMessage(PES->hWnd,EM_GETSEL,
			(WPARAM)&ts->cursor.start,(WPARAM)&ts->cursor.end);
	ts->cursororg = ts->cursor;
									// 読み込み位置を決定 -----------------
	if ( ts->cursor.start != ts->cursor.end ){			// 範囲選択あり
		line = SendMessage(PES->hWnd,EM_LINEFROMCHAR,
				(WPARAM)ts->cursor.start,0);
		topline = SendMessage(PES->hWnd,EM_LINEFROMCHAR,
				(WPARAM)ts->cursor.end,0);
	}else{						// 範囲選択なし
		line = SendMessage(PES->hWnd,EM_LINEFROMCHAR,(WPARAM)-1,0);
		if ( (mode == TEXTSEL_BACK) && line ) line--;
		topline = line + 1;
	}
									// 行単位で読み込み -----------------------
	destptr = ts->text;
	bufsize = EDITBUFSIZE;
	wBP = wLP = SendMessage(PES->hWnd,EM_LINEINDEX,(WPARAM)line,0);
	if ( (LONG_PTR)wLP >= 0 ) do {
		size_t len;

		*(WORD *)tmp = TSIZEOF(tmp) - 2 - 1;
		// 行テキストと文字数(xpbug影響なし)を取得
		len = SendMessage(PES->hWnd,EM_GETLINE,(WPARAM)line,(LPARAM)tmp);
		// len=0 (読み込み失敗の可能性あり)でも、続行する必要あり
		if ( bufsize < len ) return FALSE;	// バッファ不足
		bufsize -= len;
		memcpy(destptr,tmp,TSTROFF(len));

		// 次の行へ & 改行があれば改行を追加
		line++;
		#ifndef UNICODE
		if ( xpbug < 0 ){
			int i = 0;
			while ( i < len ){
				wLP++;
				i += IskanjiA( *(destptr + i) ) ? 2 : 1;
			}
			destptr += len;
			len = wLP;
		}else{
			destptr += len;
			len += wLP;
		}
		#else
			destptr += len;
			len += wLP;
		#endif
		wLP = SendMessage(PES->hWnd,EM_LINEINDEX,(WPARAM)line,0);
		if ( (LONG_PTR)wLP < 0 ) break;				// これ以上行がない

		if ( len < (size_t)wLP ){	// 改行が隠れている
			if ( 1 > (bufsize -= 2) ) return FALSE;	// バッファ不足
			*destptr++ = '\r';
			*destptr++ = '\n';
		}
	}while( line <= topline );
	*destptr = '\0';
									// 範囲選択 ---------------------------
	// 頭出し
	pos = ts->cursor.start - wBP;
#ifndef UNICODE
	CaretFixToA(ts->text,&pos);
#endif
	if ( pos >= EDITBUFSIZE ) return FALSE;
	destptr = ts->text + pos;
	if ( ts->cursor.start < ts->cursor.end ){	// 範囲選択あり
		pos = ts->cursor.end - ts->cursor.start;
#ifndef UNICODE
		CaretFixToA(destptr,&pos);
#endif
		if ( (destptr + pos) >= (ts->text + EDITBUFSIZE) ) return FALSE;
		*(destptr + pos) = '\0';
		ts->word = destptr;
	}else{						// 範囲選択なし
		TCHAR *top,*end;

		top = end = destptr;
		switch ( mode ){
			case TEXTSEL_CHAR:	// Del
				if ( *end == 0xd ){
					end++;
					if ( *end == 0xa ) end++;
				}else{
					#ifdef UNICODE
						end++;
					#else
						end += Chrlen(*end);
					#endif
				}
				break;
			case TEXTSEL_BACK:	// BS
				if ( (ts->text < top) && ( *(top - 1) == 0xa ) ){
					top--;
					if ( (ts->text < top) && ( *(top - 1) == 0xd ) ) top--;
				}else{
					top -= bchrlen(ts->text,top - ts->text);
				}
				break;
			case TEXTSEL_WORD:	// Word
				while( (UTCHAR)*end > ' ' ) end++;
				break;
			case TEXTSEL_BEFORE:	// カーソルより前
				top = ts->text;
				break;
			case TEXTSEL_AFTER:	// カーソルより後ろ
				while ( *end ){
					if ( *end == '\r' ) break;
					end++;
				}
				break;
//			case TEXTSEL_ALL:	// 全選択
		}
		*end = '\0';

		#ifndef UNICODE
			pos = end - ts->text;
			pos2 = top - ts->text;
			if ( xpbug < 0 ){
				CaretFixToW(ts->text,&pos);
				CaretFixToW(ts->text,&pos2);
			}
			SendMessage(PES->hWnd,EM_SETSEL,pos2 + wBP,pos + wBP);
		#else
			SendMessage(PES->hWnd,EM_SETSEL,
					top - ts->text + wBP,end - ts->text + wBP);
		#endif
		ts->word = top;
	}
	return TRUE;
}

BOOL SelectEditStrings(PPxEDSTRUCT *PES,TEXTSEL *ts,int mode)
{
	BOOL result;

	if ( PES->flag & PPXEDIT_TEXTEDIT ){
		SendMessage(PES->hWnd,WM_SETREDRAW,FALSE,0);
		result = SelectEditStringsM(PES,ts,mode);	// マルチライン用
	}else{
		SendMessage(PES->hWnd,WM_SETREDRAW,FALSE,0);
		result = SelectEditStringsS(PES,ts,mode);	// 一行用
	}
	SendMessage(PES->hWnd,WM_SETREDRAW,TRUE,0);
	return result;
}

void GetHeight(PPxEDSTRUCT *PES,HFONT hFont)
{
	HDC hDC;
	HGDIOBJ hOldFont C4701CHECK;	//一時保存用
	TEXTMETRIC tm;

	hDC = GetWindowDC(PES->hWnd);
	if ( hFont ) hOldFont = SelectObject(hDC,hFont);
	GetTextMetrics(hDC,&tm);
	if ( hFont ) SelectObject(hDC,hOldFont); // C4701ok
	ReleaseDC(PES->hWnd,hDC);

	PES->fontY = tm.tmHeight;
}

void LineCursor(PPxEDSTRUCT *PES,DWORD mes)
{
	POINT pos;
	DWORD line;
	HDC hDC;

	if ( GetFocus() != PES->hWnd ) return;
	line = CallWindowProc(PES->hOldED,PES->hWnd,EM_GETFIRSTVISIBLELINE,0,0);
	GetCaretPos(&pos);
	if ( (PES->caretY != pos.y) || (line != PES->caretLY) || (mes == WM_PAINT)){
		HBRUSH hBrush;
		RECT box;
		int topbackup;

		CallWindowProc(PES->hOldED,PES->hWnd,EM_GETRECT,0,(LPARAM)&box);
		if ( PES->exstyle & WS_EX_CLIENTEDGE ){
			box.top++;
			box.left++;
		}

		hDC = GetWindowDC(PES->hWnd);
		hBrush = (HBRUSH)SendMessage(GetParent(PES->hWnd),WM_CTLCOLOREDIT,(WPARAM)hDC,(LPARAM)PES->hWnd);
		topbackup = box.top;
		box.top += PES->caretY + (PES->caretLY - line + 1) * PES->fontY - 1;
		box.bottom = box.top + 1;
		FillBox(hDC,&box,hBrush);

		box.top = topbackup + pos.y + PES->fontY - 1;
		box.bottom = box.top + 1;
		hBrush = CreateSolidBrush(GetTextColor(hDC));
		FillBox(hDC,&box,hBrush);
		DeleteObject(hBrush);

		ReleaseDC(PES->hWnd,hDC);
		PES->caretY = pos.y;
		PES->caretLY = line;
	}
}

//=============================================================================
// システムフック処理
//-----------------------------------------------------------------------------
// システムフックハンドラ
//-----------------------------------------------------------------------------
LRESULT CALLBACK CBTProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	if( nCode == HCBT_CREATEWND ){
		TCHAR buf[MAX_PATH];

		GetClassName((HWND)wParam,buf,TSIZEOF(buf));
		if( tstricmp(buf,T("Edit")) == 0 ){
			PPxRegistExEdit(NULL,(HWND)wParam,0,NULL,0,0,0);
		}
	}
	return CallNextHookEx(Sm->hhookCBT,nCode,wParam,lParam);
}

BOOL CALLBACK PPxUnHookEditSub(HWND hWnd,LPARAM lParam)
{
	TCHAR buf[MAX_PATH];

	EnumChildWindows(hWnd,PPxUnHookEditSub,lParam);

	if ( GetClassName((HWND)hWnd,buf,TSIZEOF(buf)) ){
		if( tstricmp(buf,T("Edit")) == 0 ){
			SendMessage(hWnd,WM_PPXCOMMAND,KE__FREE,0);
		}
	}
	return TRUE;
}
//------------------------------------ 設定処理
PPXDLL BOOL PPXAPI PPxHookEdit(int local)
{
	if ( local < 0 ){				// フック解放
		if ( Sm->hhookCBT ){
			EnumWindows(PPxUnHookEditSub,0);
			UsePPx();
			UnhookWindowsHookEx(Sm->hhookCBT);
			Sm->hhookCBT = NULL;
			FreePPx();
			PostMessage(HWND_BROADCAST,WM_NULL,0,0);
			return TRUE;
		}
	}else if ( Sm->hhookCBT == NULL ){	// フック設定
		UsePPx();
		Sm->hhookCBT = SetWindowsHookEx(WH_CBT,CBTProc,DLLhInst,
				local ? GetCurrentThreadId() : 0);
		FreePPx();
		PostMessage(HWND_BROADCAST,WM_NULL,0,0);
		return TRUE;
	}
	return FALSE;
}
