/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library			拡張エディット エディタ系コマンド
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "PPD_EDL.H"
#pragma hdrstop

BOOL SjisToEUCjp(char **text, DWORD *size)
{
	BYTE *ptr, *maxptr;
	BYTE *bottom, *top;
	DWORD maxsize;

	maxptr = (BYTE *)(*text + *size);
	maxsize = *size + ThSTEP;
	top = bottom = HeapAlloc(DLLheap, 0, maxsize);
	if ( bottom == NULL ) return FALSE;
	ptr = (BYTE *)*text;
	while ( ptr < maxptr ){
		WORD code1;

		if ( (top + 0x8) > (bottom + maxsize) ){
			BYTE *newbottom;

			maxsize += ThNextAllocSizeM(maxsize);
			newbottom = HeapReAlloc(DLLheap, 0, bottom, maxsize);
			if ( newbottom == NULL ) break;
#pragma warning(suppress:6001) // サイズ計算のみに使用
			top = newbottom + (top - bottom);
			bottom = newbottom;
		}
		code1 = *ptr++;

		if ( code1 < 0x80 ){
			*top++ = (BYTE)code1;
			continue;
		}
		if ( IskanjiA(code1) ){
			BYTE code2;

			code2 = *ptr++;
			if ( code2 >= 0x9f ){
				code1 = (BYTE)(code1 * 2 - (code1 >= 0xe0 ? 0xe0 : 0x60));
				code2 += (BYTE)2;
			}else if (code2){
				code1 = (BYTE)(code1 * 2 - (code1 >= 0xe0 ? 0xe1 : 0x61));
				code2 += (BYTE)(0x60 + (code2 < 0x7f));
			}
			*top++ = (BYTE)code1;
			*top++ = (BYTE)code2;
		}else{
			*top++ = (BYTE)0x8e;
			*top++ = (BYTE)code1;
		}
	}
	HeapFree(ProcHeap, 0, *text);
	*text = (char *)bottom;
	*size = top - bottom;
	return TRUE;
}

void InitEditCharCode(PPxEDSTRUCT *PES)
{
	if ( PES->CharCode != 0 ) return;
	PES->CharCode = VTYPE_SYSTEMCP;
	GetCustData(T("X_newcp"), &PES->CharCode, sizeof(DWORD));
}

/*-----------------------------------------------------------------------------
	result	FALSE:cancel
-----------------------------------------------------------------------------*/
BOOL FileSave(PPxEDSTRUCT *PES, int mode)
{
	TCHAR name[VFPS];
	BOOL result = FALSE;

	InitEditCharCode(PES);

	if ( PES->filename[0] != '\0' ){
		tstrcpy(name, PES->filename);
	}else{
		if ( PES->flags & PPXEDIT_TEXTEDIT ){
			GetWindowText(GetParentCaptionWindow(PES->hWnd), name, TSIZEOF(name));
			if ( name[0] == ' ' ) tstrcpy(name, T("EDITTEXT.TXT"));
		}else{
			tstrcpy(name, T("EDITTEXT.TXT"));
		}
	}
	if ( FindPathSeparator(name) == NULL ) VFSFullPath(NULL, name, NULL);

	while ( (!(mode & EDL_FILEMODE_DIALOG) &&
			  (mode & EDL_FILEMODE_NODIALOG) &&
			  (PES->filename[0] != '\0') ) ||
			( tInput(PES->hWnd, (mode & EDL_FILEMODE_APPEND) ?
					MES_TAPN : MES_TSVN, name, TSIZEOF(name),
					PPXH_NAME_R, PPXH_PATH) > 0) ){
		HANDLE hFile;
		DWORD size;
		char *text;

		if ( !(mode & EDL_FILEMODE_APPEND) && tstricmp(name, PES->filename) ){
			hFile = CreateFileL(name, GENERIC_READ,
					FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if ( hFile != INVALID_HANDLE_VALUE ){	// 同名ファイルあり
				CloseHandle(hFile);
				if ( PMessageBox(PES->hWnd, MES_QSAM, T("File exist"),
							MB_APPLMODAL | MB_OKCANCEL |
							MB_DEFBUTTON1 | MB_ICONQUESTION) != IDOK ){
					resetflag(mode, EDL_FILEMODE_NODIALOG);
					continue;
				}
			}
		}
		hFile = CreateFileL(name, GENERIC_WRITE, 0, NULL,
				(mode & EDL_FILEMODE_APPEND) ? OPEN_ALWAYS : CREATE_ALWAYS,
				FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ){
			PPErrorBox(PES->hWnd, T("File open error"), PPERROR_GETLASTERROR);
			resetflag(mode, EDL_FILEMODE_NODIALOG);
			continue;
		}
		tstrcpy(PES->filename, name);
										// 書き込み準備(各種コード変換)
		#ifdef UNICODE	// UNICODE版は個別に処理する
		if ( (PES->CharCode == VTYPE_UTF8) ||
			 (PES->CharCode == CP_UTF8) ||
			 (PES->CharCode == VTYPE_UNICODE) ||
			 (PES->CharCode == CP__UTF16L) ||
			 (PES->CharCode == VTYPE_UNICODEB) ||
			 (PES->CharCode == CP__UTF16B) ){
			int offset;

			size = GetWindowTextLengthW(PES->hWnd) + 1; // BOM分を加算
			text = HeapAlloc(ProcHeap, 0, TSTROFF(size + 1)); // '\0'分を加算
			if ( text == NULL ){
				PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
				return FALSE;
			}
			// BOM なし
			if ( (PES->CharCode == CP_UTF8) ||
				 (PES->CharCode == CP__UTF16L) ||
				 (PES->CharCode == CP__UTF16B) ){
				offset = 0;
				size--; // BOM分
			}else{ // VTYPE_UNICODE / VTYPE_UNICODEB / VTYPE_UTF8 は BOM あり
				*(WCHAR *)text = 0xfeff; // UCF2HEADER
				offset = 1;
			}

			GetWindowTextW(PES->hWnd, (WCHAR *)text + offset, size + 1);

			if ( PES->CrCode != VTYPE_CRLF ){
				WCHAR *src, *dst;
				WCHAR crcode;

				crcode = (WCHAR)((PES->CrCode == VTYPE_CR) ? '\r' : '\n');
				src = dst = (WCHAR *)text + 1;
				while ( *src ){
					if ( *src == '\r' ){
						src += 2; // '\r\n' 分
						*dst++ = crcode;
						continue;
					}
					*dst++ = *src++;
				}
				size = dst - (WCHAR *)text;
			}
			// CP_UTF8 / VTYPE_UTF8 UCS-2 → UTF-8 変換
			if ( (PES->CharCode == CP_UTF8) || (PES->CharCode == VTYPE_UTF8) ){
				int newsize;
				char *newtext;

				newsize = WideCharToMultiByteU8(CP_UTF8, 0, (WCHAR *)text, size, NULL, 0, NULL, NULL);
				newtext = HeapAlloc(ProcHeap, 0, newsize);
				if ( newtext == NULL ){
					HeapFree(ProcHeap, 0, text);
					PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
					return FALSE;
				}
				size = WideCharToMultiByteU8(CP_UTF8, 0, (WCHAR *)text, size, newtext, newsize, NULL, NULL);
				HeapFree(ProcHeap, 0, text);
				text = newtext;
			}else{ // CP__UTF16L / VTYPE_UNICODE / CP__UTF16B / VTYPE_UNICODEB
				size *= sizeof(WCHAR);
				if ( (PES->CharCode == VTYPE_UNICODEB) || (PES->CharCode == CP__UTF16B) ){
					// バイトオーダ変換
					WCHAR *uptr;
					uptr = (WCHAR *)text;
					while ( *uptr ){
						WCHAR lc;

						lc = *uptr;
						*uptr++ = (WCHAR)((lc >> 8) | (lc << 8));
					}
				}
			}
		}else{
			if ( PES->CharCode == VTYPE_SYSTEMCP ){
				size = GetWindowTextLengthA(PES->hWnd);
				text = HeapAlloc(ProcHeap, 0, size + 1);
				if ( text == NULL ){
					PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
					return FALSE;
				}
				GetWindowTextA(PES->hWnd, text, size + 1);
			}else{
				UINT cp;
				int sizeW;
				WCHAR *textW;

				cp = (PES->CharCode < VTypeToCPlist_max) ? VTypeToCPlist[PES->CharCode] : PES->CharCode;
				sizeW = GetWindowTextLengthW(PES->hWnd) + 1; // '\0'分を加算
				textW = HeapAlloc(ProcHeap, 0, TSTROFF(sizeW));
				if ( textW == NULL ){
					PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
					return FALSE;
				}
				GetWindowTextW(PES->hWnd, textW, sizeW);

				size = WideCharToMultiByteU8(cp, 0, textW, sizeW, NULL, 0, NULL, NULL);
				text = HeapAlloc(ProcHeap, 0, size);
				if ( text == NULL ){
					HeapFree(ProcHeap, 0, textW);
					PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
					return FALSE;
				}
				WideCharToMultiByteU8(cp, 0, textW, sizeW, text, size, NULL, NULL);
				if ( size ) size--; // '\0'除去
				HeapFree(ProcHeap, 0, textW);
			}

			if ( PES->CrCode != VTYPE_CRLF ){
				char *src, *dst;
				char crcode;

				crcode = (char)((PES->CrCode == VTYPE_CR) ? '\r' : '\n');
				src = dst = text;
				while ( *src ){
					if ( *src == '\r' ){
						src += 2;
						*dst++ = crcode;
						continue;
					}
					*dst++ = *src++;
				}
				size = dst - text;
			}
		}
		#else	// ANSI版は、UNICODEの場合、ある程度まとめて処理する
		size = GetWindowTextLength(PES->hWnd);
		text = HeapAlloc(ProcHeap, 0, size + 1);
		if ( text == NULL ){
			PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
			return FALSE;
		}
		GetWindowText(PES->hWnd, text, size + 1);
		if ( PES->CrCode != VTYPE_CRLF ){
			char *src, *dst;
			char crcode;

			crcode = (char)((PES->CrCode == VTYPE_CR) ? '\r' : '\n');
			src = dst = text;
			while ( *src ){
				if ( *src == '\r' ){
					src += 2;
					*dst++ = crcode;
					continue;
				}
				*dst++ = *src++;
			}
			size = dst - text;
		}
		if ( PES->CharCode != VTYPE_SYSTEMCP ){
			UINT cp;
			WCHAR *newtext;

			cp = (PES->CharCode < VTypeToCPlist_max) ? VTypeToCPlist[PES->CharCode] : PES->CharCode;
			// 一旦 UNICODE へ
			size = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
			newtext = HeapAlloc(ProcHeap, 0, size * sizeof(WCHAR));
			if ( newtext != NULL ){
				int offset;

				if ( (PES->CharCode == VTYPE_UTF8) ||
					 (PES->CharCode == VTYPE_UNICODE) ||
					 (PES->CharCode == VTYPE_UNICODEB) ){
					*newtext = 0xfeff; // UCF2HEADER
					offset = 1;
				}else{
					offset = 0;
				}
				size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
						text, -1, newtext + offset, size) + offset;
				HeapFree(ProcHeap, 0, text);
				text = (char *)newtext;
				if ( size ) size--; // '\0'除去
			}
			if ( (cp == CP__UTF16L) || (cp == CP__UTF16B) ){ // UNICODE の時は、そのまま
				if ( cp == CP__UTF16B ){ // バイトオーダ変換
					WCHAR *uptr;
					DWORD left;
					uptr = (WCHAR *)text;
					left = size;
					while ( left-- ){
						WCHAR lc;

						lc = *uptr;
						*uptr++ = (WCHAR)((lc >> 8) | (lc << 8));
					}
				}
				size *= sizeof(WCHAR);
			}else{ // UNICODE から該当 codepage へ
				int newsize;
				char *newtext;

				newsize = WideCharToMultiByteU8(cp, 0, (WCHAR *)text, size, NULL, 0, NULL, NULL);
				newtext = HeapAlloc(ProcHeap, 0, newsize);
				if ( newtext == NULL ){
					HeapFree(ProcHeap, 0, text);
					PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
					return FALSE;
				}
				size = WideCharToMultiByteU8(cp, 0, (WCHAR *)text, size, newtext, newsize, NULL, NULL);
				HeapFree(ProcHeap, 0, text);
				text = newtext;
			}
		}
		#endif
		if ( PES->CharCode == VTYPE_EUCJP ) SjisToEUCjp(&text, &size);
										// 書き込み
		if ( mode & EDL_FILEMODE_APPEND ){
			DWORD i = 0;

			SetFilePointer(hFile, 0, (PLONG)&i, FILE_END);
		}
		if ( WriteFile(hFile, text, size, (DWORD *)&size, NULL) == FALSE ){
			PPErrorBox(PES->hWnd, T("File write error"), PPERROR_GETLASTERROR);
		}else{
			result = TRUE;
			XEditClearModify(PES);
		}
		CloseHandle(hFile);

		HeapFree(ProcHeap, 0, text);
		if ( PES->flags & PPXEDIT_TEXTEDIT ){
			SetWindowText(GetParentCaptionWindow(PES->hWnd), name);
		}
		if ( IsTrue(result) ) break;
	}
	return result;
}

void OpenMainFromMem(PPxEDSTRUCT *PES, int openmode, const TCHAR *filename, const TCHAR *textimage, DWORD memsize, int usecp)
{
	int crcode = VTYPE_CRLF, charcode;
	TCHAR *text;

	charcode = FixTextImage((const char *)textimage, memsize, &text, usecp);
	{									// 改行コードの決定
		int size;
		TCHAR *ptr;

		for ( ptr = text, size = 0x1000 ; *ptr && size ; ptr++, size-- ){
			if ( *ptr == '\r' ){ // CR
				if ( *(ptr + 1) == '\n' ){ // LF
					break;	// CRLF
				}
				crcode = VTYPE_CR;
				break;
			}else if ( *ptr == '\n' ){ // LF
				crcode = VTYPE_LF;
				break;
			}
		}
	}
	if ( openmode != PPE_OPEN_MODE_INSERT ){
		PES->CharCode = (charcode >= 0) ? charcode : -charcode;
		PES->CrCode = crcode;
		if ( filename != NULL ){
			tstrcpy(PES->filename, filename);
			if ( PES->flags & PPXEDIT_TEXTEDIT ){
				SetWindowText(GetParentCaptionWindow(PES->hWnd), filename);
			}
		}
	}
										// 改行コードに合わせて SetText
	if ( crcode != VTYPE_CRLF ){
		TCHAR *p, code;

		if ( openmode != PPE_OPEN_MODE_INSERT ){ // 上書き
			SendMessage(PES->hWnd, EM_SETSEL, 0, EC_LAST);
		}
		p = text;
		code = (TCHAR)((crcode == VTYPE_CR) ? T('\r') : T('\n'));
		while ( *p != '\0' ){
			TCHAR *next;

			next = tstrchr(p, code);
			if ( next == NULL ){
				SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)p);
				break;
			}else{
				*next = '\0';
				SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)p);
				SendMessage(PES->hWnd, EM_SETSEL, EC_LAST, EC_LAST);
				SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)T("\r\n"));
				SendMessage(PES->hWnd, EM_SETSEL, EC_LAST, EC_LAST);
				p = next + 1;
			}
		}
		SendMessage(PES->hWnd, EM_SETSEL, EC_LAST, EC_LAST);
	}else{
		if ( openmode != PPE_OPEN_MODE_INSERT ){ // 上書き
			SetWindowText(PES->hWnd, text);
		}else{
			SendMessage(PES->hWnd, EM_REPLACESEL, 0, (LPARAM)text);
		}
	}
	if ( charcode < 0 ) HeapFree(DLLheap, 0, text);
}

/*-----------------------------------------------------------------------------
	ファイルを開く処理
-----------------------------------------------------------------------------*/
BOOL OpenFromFile(PPxEDSTRUCT *PES, int openmode, const TCHAR *fname)
{
	BOOL newmode = FALSE;
	DWORD memsize;
	TCHAR filename[VFPS];
	TCHAR *textimage = NULL;
	EDITMODESTRUCT ems = { EDITMODESTRUCT_DEFAULT };

	const TCHAR *exec = NULL, *curdir = NULL;

	TCHAR buf[CMDLINESIZE], code, *more;

	if ( openmode >= PPE_OPEN_MODE_CMDOPEN ){
		if ( openmode == PPE_OPEN_MODE_EXCMDOPEN ){
			curdir = ((PPE_CMDMODEDATA *)fname)->curdir;
			fname = ((PPE_CMDMODEDATA *)fname)->param;
			openmode = PPE_OPEN_MODE_OPENNEW;
		}else{
			openmode -= PPE_OPEN_MODE_CMDOPEN; // CMDxxx を xxx に変換
		}

		filename[0] = '\0';
		for ( ;; ){
			code = GetOptionParameter(&fname, buf, &more);
			if ( code == '\0' ) break;
			if ( code != '-' ){
				if ( (curdir != NULL) || (PES->filename[0] == '\0') ){
					VFSFixPath(filename, buf, curdir, VFSFIX_FULLPATH | VFSFIX_REALPATH);
				}else{
					// カレントディレクトリを生成
					VFSFullPath(filename, T(".."), PES->filename);
					VFSFixPath(filename, buf, filename, VFSFIX_FULLPATH | VFSFIX_REALPATH);
				}
			}else{
				if ( tstrcmp(buf + 1, T("NEW")) == 0 ){
					newmode = TRUE;
				}else if ( tstrcmp(buf + 1, T("K")) == 0 ){
					exec = fname;
					break;
				}else if ( EditModeParam(&ems, buf + 1, more) == FALSE ){
					XMessage(PES->hWnd, T("File open error"), XM_GrERRld, T("option error : %s"), buf);
				}
			}
			NextParameter(&fname);
		}
	}else{
		tstrcpy(filename, fname);
	}
	if ( filename[0] != '\0' ){	// ファイルの読み込み
		ERRORCODE result;

		result = VFSLoadFileImage(filename, 0x40, (char **)&textimage, &memsize, NULL);
		if ( result != NO_ERROR ){
			if ( newmode == FALSE ){
				HWND hParentWnd;

				if ( PES->filename[0] == '\0' ){
					tstrcpy(PES->filename, filename);
					// 設定初期化1
					PPeSetTab(PES, ems.tabwidth);
					if ( ems.crcode >= 0 ) PES->CrCode = ems.crcode;
				}
				hParentWnd = GetParent(PES->hWnd);
				if ( openmode >= 0 ){ // PPE_OPEN_MODE_OPEN / PPE_OPEN_MODE_INSERT
					PPErrorBox(PES->hWnd, filename, result);
				}else{ // PPE_OPEN_MODE_OPENNEW
					if ( hParentWnd != NULL ){
						SetWindowText(hParentWnd, filename);
					}
				}
				if ( hParentWnd != NULL ){
					SetMessageOnCaption(hParentWnd, T("open error"));
				}
				return FALSE;
			}
		}
	}
	if ( (textimage == NULL) && (openmode != PPE_OPEN_MODE_INSERT) ){
		textimage = HeapAlloc(ProcHeap, HEAP_ZERO_MEMORY, 4);
		memsize = 4;
	}
	SendMessage(PES->hWnd, WM_SETREDRAW, FALSE, 0);
										// 文字コード判別＆変換
	if ( textimage != NULL ){
		OpenMainFromMem(PES, openmode, filename, textimage, memsize, ems.codepage);
		HeapFree(ProcHeap, 0, textimage);
	}
	// 設定初期化2
	PPeSetTab(PES, ems.tabwidth);
	if ( ems.crcode >= 0 ) PES->CrCode = ems.crcode;

	SendMessage(PES->hWnd, EM_SCROLLCARET, 0, 0);
	if ( openmode == PPE_OPEN_MODE_INSERT ){
		XEditSetModify(PES);
	}else{
		XEditClearModify(PES);
	}
	SendMessage(PES->hWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(PES->hWnd, NULL, TRUE);

	SendMessage(PES->hWnd, WM_PPXCOMMAND, K_E_LOAD, 0);

	if ( exec != NULL ) EditExtractMacro(PES, exec, NULL, 0);
	return TRUE;
}


void FileOpen(PPxEDSTRUCT *PES, int mode)
{
	TCHAR name[VFPS];

	name[0] = '\0';
	if ( tInput(PES->hWnd,
			(mode == 0) ? T("Open Filename") : T("Insert Filename"),
			name, TSIZEOF(name), PPXH_NAME_R, PPXH_PATH) > 0 ){
		OpenFromFile(PES, mode, name);
	}
}

// 指定行へジャンプ -----------------------------------------------------------
void JumptoLine(HWND hWnd, int line)
{
	DWORD wP, lP;
	int offset;

	SendMessage(hWnd, EM_GETSEL, (WPARAM)&wP, (WPARAM)&lP);
	offset = SendMessage(hWnd, EM_LINEINDEX, (WPARAM)-1, 0);
	wP -= offset;
	offset = SendMessage(hWnd, EM_LINEINDEX, (WPARAM)line, 0);
	if ( offset != -1 ){
		lP = SendMessage(hWnd, EM_LINELENGTH, (WPARAM)line, 0);
		if ( wP > lP ) wP = lP;
		wP += offset;
		SendMessage(hWnd, EM_SETSEL, wP, wP);
		SendMessage(hWnd, EM_SCROLLCARET, 0, 0);
	}
}
