/*-----------------------------------------------------------------------------
	Paper Plane cUI												Where is
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define PAUSECOUNT	10

const TCHAR BASEHEADER[] = T(";Base=");
const TCHAR BASETYPEHEADER[] = T("|1\r\n");

const TCHAR *InsertMacroMenuSourceE[] = {
	T(";	path separator"),
	T(".;%path%	program directories"),
	NULL
};

const TCHAR *InsertMacroMenuSourceJ[] = {
	T(";	検索パスの区切り"),
	T(".;%path%	実行可能なディレクトリ全部"),
	NULL
};

const TCHAR **InsertMacroMenuSource[] = {
	InsertMacroMenuSourceE, InsertMacroMenuSourceJ
};

const TCHAR *InsertMacroMenuMaskE[] = {
	T("*	any number of characters"),
	T("?	a specified number of characters"),
	T("/formats/	regular expression"),
	T(",	mask separator"),
	T("&a:r+s+h-,	file attributes(*TOP* readonly,system,no hidden)"),
	T("&d:<=1,	written date(*TOP* 1day)"),
	T("&d:>=2000-1-2,	written date(*TOP* after 2000-1-2)"),
	T("&s:<=10k,	file size(*TOP* in 10,000bytes)"),
	NULL
};

const TCHAR *InsertMacroMenuMaskJ[] = {
	T("*	0文字以上の任意の文字列"),
	T("?	1文字の任意の文字列"),
	T("/正規表現/	正規表現"),
	T(",	区切り"),
	T("&a:r+s+h-,	属性限定(要先頭、readonly,system,hidden無)"),
	T("&d:<=1,	変更日付限定(要先頭、1日以内)"),
	T("&d:>=2000-1-2,	変更日付限定(要先頭、2000年1月2日以降)"),
	T("&s:<=10k,	サイズ限定(要先頭、10,000バイト以内)"),
	NULL
};

const TCHAR **InsertMacroMenuMask[] = {
	InsertMacroMenuMaskE, InsertMacroMenuMaskJ
};

const TCHAR *ExWhereMenuList[] = {
	MES_TPTX,	// テキスト検索
	MES_TCMT,	// コメント
	MES_TCIT,	// チップテキスト
	MES_TCLE,	// カラム拡張
	MES_TSMD,
	NULL
};
#define WHERE_COLUMNEXT 1000

typedef struct {
	BOOL result;
	WHERESTRUCT *Pws;
} TIPMATCHSTRUCT;


BOOL CheckStringMatch(const TCHAR *target, WHERESTRUCT *Pws);

typedef struct {
	PPXAPPINFO info;
	WriteTextStruct *sts;
	PPC_APPINFO *cinfo;
	WHERESTRUCT *pws;
} WHEREISAPPINFO;

const TCHAR *swstr[] = { T("off"), T("on") };

void ExWhereMenu(WHERESTRUCT *pws, HWND hDlg, HWND hButtonWnd)
{
	HMENU hMenu, hColumnMenu;
	int exmode;
	const TCHAR **menus;
	RECT box;

	if ( IsShowButtonMenu() == FALSE ) return;
	hMenu = CreatePopupMenu();
	menus = ExWhereMenuList;
	for ( exmode = WDEXM_UNKNOWN ; exmode <= WDEXM_COMMENT ; exmode++, menus++ ){
		AppendMenuString(hMenu, exmode + 1, *menus);
	}
	hColumnMenu = CreatePopupMenu();
	GetColumnExtMenu(&pws->thEcdata, pws->st.cinfo->RealPath, hColumnMenu, WHERE_COLUMNEXT);
	AppendMenu(hMenu, MF_EPOP, (UINT_PTR)hColumnMenu, MessageText(*menus));

	AppendMenuString(hMenu, WDEXM_MODULE, ExWhereMenuList[WDEXM_MODULE - 1]);

	GetWindowRect(hButtonWnd, &box);
	exmode = TrackPopupMenu(hMenu, TPM_TDEFAULT, box.left, box.bottom, 0, hDlg, NULL);
	DestroyMenu(hMenu);
	EndButtonMenu();

	if ( exmode > WDEXM_UNKNOWN ){
		if ( exmode >= WHERE_COLUMNEXT ){
			pws->indexEc = exmode - WHERE_COLUMNEXT;
			exmode = WDEXM_COLUMNMIN;
		}
		pws->st.exmode = exmode;
		SendMessage(hButtonWnd, WM_SETTEXT, 0, (LPARAM)MessageText(ExWhereMenuList[exmode - 1]));
	}
}

void ButtonHelpMenu(PPC_APPINFO *cinfo, HWND hDlg, HWND hButtonWnd, UINT editID, int type)
{
	if ( IsShowButtonMenu() ){
		HMENU hMenu;
		int id = 1;
		RECT box;
		POINT pos;
		const TCHAR **menustr, ***menulists, **menulist, *usermenu;
		TCHAR param[CMDLINESIZE];
		ERRORCODE result = ERROR_INVALID_FUNCTION;

		GetWindowRect(hButtonWnd, &box);
		pos.x = box.left;
		pos.y = box.bottom;

		if ( type != 0 ){
			usermenu = T("%M_mask");
			menulists = InsertMacroMenuMask;
		}else{
			usermenu = T("%M_wsrc");
			menulists = InsertMacroMenuSource;
		}
		if ( IsExistCustData(usermenu + 1) ){
			result = PP_ExtractMacro(hDlg, &cinfo->info, &pos, usermenu, param, 0);
			if ( result == ERROR_CANCELLED ) return;
		}
		if ( result != NO_ERROR ){
			param[0] = '\0';
			menulist = menulists[(LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE) ? 1 : 0];

			hMenu = CreatePopupMenu();
			for ( menustr = menulist ; *menustr != NULL ; menustr++ ){
				AppendMenuString(hMenu, id++, *menustr);
			}
			id = TrackPopupMenu(hMenu, TPM_TDEFAULT, box.left, box.bottom, 0, hDlg, NULL);
			DestroyMenu(hMenu);
			EndButtonMenu();
			if ( id ){
				TCHAR *p;
				tstrcpy(param, menulist[id - 1]);
				p = tstrchr(param, '\t');
				if ( p != NULL ) *p = '\0';
			}
		}
		if ( param[0] != '\0' ){
			if ( type >= 0 ){
				SendDlgItemMessage(hDlg, editID, EM_REPLACESEL, 0, (LPARAM)param);
			}else{
				SetDlgItemText(hDlg, editID, param);
			}
		}
	}
	SetFocus(GetDlgItem(hDlg, editID));
}

void WINAPI TipMatchCallback(TIPMATCHSTRUCT *tms, const TCHAR *text)
{
	tms->result = CheckStringMatch(text, tms->Pws);
}

BOOL TipMatch(const TCHAR *name, WHERESTRUCT *Pws)
{
	TIPMATCHSTRUCT tms;
	const TCHAR *ls;

	tms.Pws = Pws;
	ls = VFSFindLastEntry(name);
	GetInfoTipText(name, (ls - name) + FindExtSeparator(ls), (GETINFOTIPCALLBACK)TipMatchCallback, (void *)&tms);
	return tms.result;
}

BOOL ColumnMatch(WHERESTRUCT *Pws, const TCHAR *name, WIN32_FIND_DATA *ff)
{
	TIPMATCHSTRUCT tms;

	tms.Pws = Pws;
	ExtGetData(&Pws->thEcdata, Pws->indexEc, name, ff->dwFileAttributes, (GETINFOTIPCALLBACK)TipMatchCallback, (void *)&tms);
	return tms.result;
}

BOOL FileMatch(const TCHAR *name, WHERESTRUCT *Pws)
{
	TCHAR *image, *text;
	BOOL result;

	if ( NOERROR != LoadTextImage(name, &image, &text, NULL) ){
		return FALSE;
	}
	result = CheckStringMatch(text, Pws);
	HeapFree( hProcessHeap, 0, image);
	return result;
}
#if !NODLL
TCHAR *tstristr(const TCHAR *target, const TCHAR *findstr)
{
	size_t len, flen;
	const TCHAR *p, *maxptr;

	flen = tstrlen(findstr);
	len = tstrlen(target);
	maxptr = target + len - flen;

#ifdef UNICODE
	for ( p = target ; p <= maxptr ; p++ ){
#else
	for ( p = target ; p <= maxptr ; p += Chrlen(*p) ){
#endif
		if ( !tstrnicmp(p, findstr, flen) ){
			return (TCHAR *)p;
		}
	}
	return NULL;
}
#endif
BOOL CheckStringMatch(const TCHAR *target, WHERESTRUCT *Pws)
{
	if ( Pws->UseFnText == FALSE ){	// 部分一致
		return tstristr(target, Pws->st.text) ? TRUE : FALSE;
	}else{		// 正規表現
		return FilenameRegularExpression(target, &Pws->fnText);
	}
}

ERRORCODE wjobinfo(WHERESTRUCT *pws, TCHAR *dir)
{
	DWORD NewTime;
	MSG msg;
	TCHAR mesbuf[CMDLINESIZE];

	if ( --pws->pausecount ) return NO_ERROR;
	pws->pausecount = PAUSECOUNT;

	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( X_MultiThread || (msg.hwnd == pws->hWnd) ){
			if ( (msg.message == WM_RBUTTONUP) ||
				 ((msg.message == WM_KEYDOWN) &&
				 (((int)msg.wParam == VK_ESCAPE)||((int)msg.wParam == VK_PAUSE)))){
				if ( PMessageBox(pws->hWnd, NULL, dir, MB_QYES) == IDOK ){
					return ERROR_CANCELLED;
				}
			}
			if (((msg.message >= WM_MOUSEFIRST) && (msg.message <= WM_MOUSELAST))||
				((msg.message >= WM_KEYFIRST) && (msg.message <= WM_KEYLAST)) ||
				(msg.message == WM_COMMAND) || (msg.message == WM_PPXCOMMAND) ){
				continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	NewTime = GetTickCount();
	if ( NewTime > (pws->OldTime + dispNwait) ){
		pws->OldTime = NewTime;
		if ( pws->hitcount ){
			wsprintf(mesbuf, T("Hit:%d  %s"), pws->hitcount, dir);
		}else{
			tstrcpy(mesbuf, dir);
		}
		SetPopMsg(pws->st.cinfo, POPMSG_PROGRESSBUSYMSG, mesbuf);
		UpdateWindow_Part(pws->st.cinfo->info.hWnd);
	}
	return NO_ERROR;
}

void MarkBo(WHERESTRUCT *pws, HWND hDlg)
{
	if ( IsDlgButtonChecked(hDlg, IDX_WHERE_MARK) ){
		SetDlgItemText(hDlg, IDC_WHERE_SRC, pws->st.cinfo->RealPath);
		EnableDlgWindow(hDlg, IDC_WHERE_SRC, FALSE);
	}else{
		EnableDlgWindow(hDlg, IDC_WHERE_SRC, TRUE);
	}
}

void InitWhereFileMaskText(HWND hDlg, WHERESTRUCT *Pws, UINT control, const TCHAR *text, DWORD wordmatch)
{
	PPxRegistExEdit(&Pws->st.cinfo->info, GetDlgItem(hDlg, control),
		TSIZEOFSTR(Pws->st.mask1),
		(wordmatch && (memcmp(text, StrWordMatchWildCard, SIZEOFTSTR(StrWordMatchWildCard) ) == 0)) ? (text + TSIZEOFSTR(StrWordMatchWildCard)) : text,
		PPXH_WILD_R, PPXH_MASK, 0);
}

void InitWhereDialog(HWND hDlg, LPARAM lParam)
{
	WHERESTRUCT *PWS;
	WildCardOptions option;

	PWS = (WHERESTRUCT *)lParam;
	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
	CenterPPcDialog(hDlg, PWS->st.cinfo);
	LocalizeDialogText(hDlg, IDD_WHERE);

	GetCustData(StrXC_rmsk, &option, sizeof(option));

	PWS->hEdWnd = PPxRegistExEdit(&PWS->st.cinfo->info,
		GetDlgItem(hDlg, IDC_WHERE_SRC),
		TSIZEOFSTR(PWS->srcpath), PWS->srcpath, PPXH_DIR_R, PPXH_DIR,
		PPXEDIT_WANTEVENT | PPXEDIT_REFTREE);
	InitWhereFileMaskText(hDlg, PWS, IDC_WHERE_F1, PWS->st.mask1, option.wordmatch);
	InitWhereFileMaskText(hDlg, PWS, IDC_WHERE_F2, PWS->st.mask2, option.wordmatch);
	PPxRegistExEdit(&PWS->st.cinfo->info, GetDlgItem(hDlg, IDC_WHERE_STR),
		TSIZEOFSTR(PWS->st.text), PWS->st.text, PPXH_WILD_R, PPXH_SEARCH, 0);

	CheckDlgButton(hDlg, IDX_WHERE_MARK, PWS->st.marked);
	CheckDlgButton(hDlg, IDX_WHERE_DIR, PWS->st.dir);
	CheckDlgButton(hDlg, IDX_WHERE_SDIR, PWS->st.sdir);
	CheckDlgButton(hDlg, IDX_WHERE_VFS, PWS->vfs);
	CheckDlgButton(hDlg, IDX_MASK_WORDMATCH, option.wordmatch);
	MarkBo(PWS, hDlg);
	SetFocus(GetDlgItem(hDlg, IDC_WHERE_F1));

	if ( (PWS->st.exmode == WDEXM_UNKNOWN) ||
		 (PWS->st.exmode == WDEXM_COLUMNMIN) ){
		PWS->st.exmode = WDEXM_PLAINTEXT;
	}

	SendMessage(GetDlgItem(hDlg, IDS_WHERE_STR), WM_SETTEXT,
			0, (LPARAM)MessageText(ExWhereMenuList[PWS->st.exmode - 1]));
}

void GetMaskTextFromEdit(HWND hDlg, UINT control, TCHAR *text, DWORD wordmatch)
{
	GetDlgItemText(hDlg, control, text, MAX_PATH);
	text[MAX_PATH - 1] = '\0';
	if ( wordmatch &&
		 (*text != '\0') &&
		 (tstrlen(text) < (MAX_PATH - TSIZEOF(StrWordMatchWildCard))) &&
		 (memcmp(text, StrWordMatchWildCard, SIZEOFTSTR(StrWordMatchWildCard) ) != 0)
	){
		memmove((BYTE *)(text) + SIZEOFTSTR(StrWordMatchWildCard), text, TSTRSIZE(text));
		memcpy(text, StrWordMatchWildCard, SIZEOFTSTR(StrWordMatchWildCard));
	}
	WriteHistory(PPXH_MASK, text, 0, NULL);
}

void OkWhereDialog(HWND hDlg, WHERESTRUCT *PWS)
{
	WildCardOptions option;

	GetCustData(StrXC_rmsk, &option, sizeof(option));
	option.wordmatch = IsDlgButtonChecked(hDlg, IDX_MASK_WORDMATCH);
	SetCustData(StrXC_rmsk, &option, sizeof(option));

	GetDlgItemText(hDlg, IDC_WHERE_SRC, PWS->srcpath, TSIZEOF(PWS->srcpath));
	WriteHistory(PPXH_DIR, PWS->srcpath, 0, NULL);

	GetMaskTextFromEdit(hDlg, IDC_WHERE_F1, PWS->st.mask1, option.wordmatch);
	GetMaskTextFromEdit(hDlg, IDC_WHERE_F2, PWS->st.mask2, option.wordmatch);
	GetDlgItemText(hDlg, IDC_WHERE_STR, PWS->st.text, TSIZEOF(PWS->st.text));
	WriteHistory(PPXH_SEARCH, PWS->st.text, 0, NULL);

	PWS->st.marked = IsDlgButtonChecked(hDlg, IDX_WHERE_MARK);
	PWS->st.dir = IsDlgButtonChecked(hDlg, IDX_WHERE_DIR);
	PWS->st.sdir = IsDlgButtonChecked(hDlg, IDX_WHERE_SDIR) ?
			FILE_ATTRIBUTE_DIRECTORY : 0;
	PWS->vfs = IsDlgButtonChecked(hDlg, IDX_WHERE_VFS);
	EndDialog(hDlg, 1);
}

INT_PTR CALLBACK WhereDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message){
							// ダイアログ初期化 (w:focus, l:User からの param)--
		case WM_INITDIALOG:
			InitWhereDialog(hDlg, lParam);
			return FALSE;

		case WM_COMMAND: {
			WHERESTRUCT *PWS;

			PWS = (WHERESTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
			switch ( LOWORD(wParam) ){
				case IDX_WHERE_MARK:
					MarkBo(PWS, hDlg);
					break;

				case IDOK:
					OkWhereDialog(hDlg, PWS);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDS_WHERE_SRC:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->st.cinfo, hDlg, (HWND)lParam, IDC_WHERE_SRC, 0);
					}
					break;
				case IDS_WHERE_F1:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->st.cinfo, hDlg, (HWND)lParam, IDC_WHERE_F1, 1);
					}
					break;
				case IDS_WHERE_F2:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->st.cinfo, hDlg, (HWND)lParam, IDC_WHERE_F2, 1);
					}
					break;

				case IDS_WHERE_STR:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ExWhereMenu(PWS, hDlg, (HWND)lParam);
						SetFocus(GetDlgItem(hDlg, IDC_WHERE_STR));
					}
					break;

				case IDB_REF:
					SendMessage(PWS->hEdWnd,
							WM_PPXCOMMAND, K_raw | K_s | K_c | 'I', 0);
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg, WM_HELP, wParam, lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDD_WHERE);
					break;
			}
			break;
		}
		default:
			return PPxDialogHelper(hDlg, message, wParam, lParam);
	}
	return TRUE;
}

void WriteFF(HANDLE hFile, WIN32_FIND_DATA *ff, const TCHAR *name)
{
	TCHAR buf[VFPS + 900], *dest;
	DWORD tmp;

	buf[0] = T('\"');
	dest = tstpcpy(buf + 1, name);

	dest += wsprintf(dest, ListFileFormatStr,
			ff->cAlternateFileName, ff->dwFileAttributes,
			ff->ftCreationTime.dwHighDateTime,
			ff->ftCreationTime.dwLowDateTime,
			ff->ftLastAccessTime.dwHighDateTime,
			ff->ftLastAccessTime.dwLowDateTime,
			ff->ftLastWriteTime.dwHighDateTime,
			ff->ftLastWriteTime.dwLowDateTime,
			ff->nFileSizeHigh, ff->nFileSizeLow);
	*dest ++ = '\r';
	*dest ++ = '\n';
	WriteFile(hFile, buf, TSTROFF32(dest - buf), &tmp, NULL);
}

void WriteFF_basic(WriteTextStruct *sts, WIN32_FIND_DATA *ff, const TCHAR *name)
{
	TCHAR buf[VFPS + 900], *dest;

	buf[0] = T('\"');
	dest = tstpcpy(buf + 1, name);

	dest += wsprintf(dest, ListFileFormatStr,
			ff->cAlternateFileName, ff->dwFileAttributes,
			ff->ftCreationTime.dwHighDateTime,
			ff->ftCreationTime.dwLowDateTime,
			ff->ftLastAccessTime.dwHighDateTime,
			ff->ftLastAccessTime.dwLowDateTime,
			ff->ftLastWriteTime.dwHighDateTime,
			ff->ftLastWriteTime.dwLowDateTime,
			ff->nFileSizeHigh, ff->nFileSizeLow);
	*dest ++ = '\r';
	*dest ++ = '\n';
	sts->Write(sts, buf, dest - buf);
}

void WriteFF_name(WriteTextStruct *sts, WIN32_FIND_DATA *ff, const TCHAR *name)
{
	TCHAR buf[VFPS + 8], *dest;

	dest = tstpcpy(buf, name);
	if ( ff->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) *dest++ = '\\';
	*dest++ = '\r';
	*dest++ = '\n';
	sts->Write(sts, buf, dest - buf);
}

// "filename", "alternate", A:atr, C:create, L:access, W:write, S:size, comment
void WriteFF2(WriteTextStruct *sts, WIN32_FIND_DATA *ff, const TCHAR *name)
{
	if ( !(sts->wlfc_flags & WLFC_NAMEONLY) ){
		WriteFF_basic(sts, ff, name);
	}else{
		WriteFF_name(sts, ff, name);
	}
}

void CheckText(WHERESTRUCT *Pws, WriteTextStruct *sts, const TCHAR *name, WIN32_FIND_DATA *ff)
{
	if ( Pws->st.exmode >= WDEXM_TIPTEXT ){
		if ( Pws->st.exmode == WDEXM_COLUMNMIN ){
			if ( IsTrue(ColumnMatch(Pws, name, ff)) ){
				Pws->hitcount++;
				WriteFF2(sts, ff, name + Pws->baselen);
			}
		}else if ( IsTrue(TipMatch(name, Pws)) ){
			Pws->hitcount++;
			WriteFF2(sts, ff, name + Pws->baselen);
		}
	}else{
		if ( !ff->nFileSizeHigh &&
			 (ff->nFileSizeLow < Pws->imgsize) &&
			 IsTrue(FileMatch(name, Pws)) ){
			Pws->hitcount++;
			WriteFF2(sts, ff, name + Pws->baselen);
		}
	}
}

ERRORCODE WhereIsDirComment(TCHAR *dir, WHERESTRUCT *pws, WriteTextStruct *sts)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA ff;		// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ThSTRUCT comments;
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR;
	// このディレクトリのコメントファイルのチェック
	ThInit(&comments);
	CatPath(src, dir, T("00_INDEX.TXT"));
	GetCommentText(&comments, src);
	if ( comments.bottom != NULL ){
		TCHAR *p;

		p = (TCHAR *)comments.bottom;
		while( *p ){
			TCHAR *cp;

			cp = p + tstrlen(p) + 1;
			if( FilenameRegularExpression(p, &pws->fn1) &&
				FilenameRegularExpression(p, &pws->fn2) ){
				if ( CheckStringMatch(cp, pws) ){
					CatPath(name, dir, p);
					hFF = FindFirstFileL(src, &ff);
					if ( hFF != INVALID_HANDLE_VALUE ){
						FindClose(hFF);
						pws->hitcount++;
						WriteFF2(sts, &ff, name + pws->baselen);
					}
				}
			}
			p = cp + tstrlen(cp) + 1;
		}
	}
	ThFree(&comments);

	CatPath(src, dir, T("*"));
	hFF = FindFirstFileL(src, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR;
	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;
		CatPath(name, dir, ff.cFileName);

		if ( ff.dwFileAttributes & pws->st.sdir ){
			if ( (result = WhereIsDirComment(name, pws, sts)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws, name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(FindNextFile(hFF, &ff)) );
	result = NO_ERROR;
end:
	FindClose(hFF);
	return result;
}

ERRORCODE WhereIsDir(const TCHAR *dir, WHERESTRUCT *pws, WriteTextStruct *sts)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA ff;	// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR; // return ERROR_FILENAME_EXCED_RANGE;
	CatPath(src, (TCHAR *)dir, T("*"));
	hFF = FindFirstFileL(src, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR; // return ERROR_PATH_NOT_FOUND;
	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;

		name[MAX_PATH] = '\0'; // 名前の大きさ検出用
		CatPath(name, (TCHAR *)dir, ff.cFileName);

		if( FinddataRegularExpression(&ff, &pws->fn1) &&
			FinddataRegularExpression(&ff, &pws->fn2) ){
			if (pws->st.dir || !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						pws->st.text[0] ){
					CheckText(pws, sts, name, &ff);
				}else{
					pws->hitcount++;

					if ( (name[MAX_PATH] != '\0') && !memcmp(name, pws->srcpath, TSTRLENGTH(pws->srcpath)) ){
						TCHAR *p = name + tstrlen(pws->srcpath); // 共通パスを省略して名前を少し縮める
						if ( *p == '\\' ) p++;
						WriteFF2(sts, &ff, p);
					}else{
						WriteFF2(sts, &ff, name + pws->baselen);
					}
				}
			}
		}

		if ( ff.dwFileAttributes & pws->st.sdir ){
			if ( (result = WhereIsDir(name, pws, sts)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws, name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(FindNextFile(hFF, &ff)) );
	result = NO_ERROR;
end:
	FindClose(hFF);
	return result;
}

ERRORCODE WhereIsDirVFS(TCHAR *dir, WHERESTRUCT *pws, WriteTextStruct *sts)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA ff;	// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR;
	CatPath(src, dir, T("*"));
	hFF = VFSFindFirst(src, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR;

	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;
		CatPath(name, dir, ff.cFileName);

		if( FinddataRegularExpression(&ff, &pws->fn1) &&
			FinddataRegularExpression(&ff, &pws->fn2) ){
			if (pws->st.dir || !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						pws->st.text[0] ){
					CheckText(pws, sts, name, &ff);
				}else{
					pws->hitcount++;
					WriteFF2(sts, &ff, name + pws->baselen);
				}
			}
		}

		if ( pws->st.sdir ){
/*
			int Dtype;
			TCHAR typename[VFSGETFFINFO_TYPESIZE];
			void *ftable;

			VFSGetFFInfo(hFF, &Dtype, typename, &ftable);
*/
			if ( (result = WhereIsDirVFS(name, pws, sts)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws, name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(VFSFindNext(hFF, &ff)) );
	result = NO_ERROR;
end:
	VFSFindClose(hFF);
	return result;
}

DWORD_PTR USECDECL SearchReportModuleFunction(WHEREISAPPINFO *winfo, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	TCHAR buf[VFPS + 900], *name;
	int len;

	if ( cmdID == PPXCMDID_REPORTSEARCH_FDATA ){
		name = ((WIN32_FIND_DATA *)uptr)->cFileName;
		WriteFF2(winfo->sts, (WIN32_FIND_DATA *)uptr, name);
	}else if ( (cmdID == PPXCMDID_REPORTSEARCH) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_FILE) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_DIRECTORY) ){
		if ( cmdID == PPXCMDID_REPORTSEARCH ){
			len = wsprintf(buf, T("\"%s\"\r\n"), uptr->str);
		}else{
			len = wsprintf(buf, T("\"%s\",A:%d\r\n"), uptr->str,
				(cmdID == PPXCMDID_REPORTSEARCH_DIRECTORY) ?
					FILE_ATTRIBUTE_DIRECTORY : 0);
		}
		winfo->sts->Write(winfo->sts, buf, len);
		name = uptr->str;
	}else{
		return winfo->cinfo->info.Function((PPXAPPINFO *)winfo->cinfo, cmdID, uptr);
	}
	winfo->pws->hitcount++;
	return (wjobinfo(winfo->pws, name) == NO_ERROR) ? 1 : 2;
}

const TCHAR * USEFASTCALL GetSWstr(BOOL state)
{
	return swstr[state ? 1 : 0];
}

void FreePws(WHERESTRUCT *Pws)
{
	if ( Pws->thEcdata.bottom != NULL ){
		GetColumnExtMenu(&Pws->thEcdata, NULL, NULL, 0); // thEcdata 解放
	}
	if ( Pws->st.exmode == WDEXM_COLUMNMIN ) Pws->st.exmode = WDEXM_UNKNOWN;
}

ERRORCODE WhereIsMain(PPC_APPINFO *cinfo, WHERESTRUCT *Pws)
{
	WriteTextStruct sts;
	TCHAR templistfile[VFPS], temp[CMDLINESIZE * 3];
	DWORD tmp;
	int fixflags;
	ERRORCODE result = NO_ERROR;

	Pws->pausecount = PAUSECOUNT;
	Pws->hitcount = 0;
	Pws->hWnd = cinfo->info.hWnd;
	Pws->OldTime = GetTickCount();
	Pws->imgsize = GetCustXDword(T("X_wsiz"), NULL, IMAGESIZELIMIT);
	Pws->st.cinfo = cinfo;
	Pws->baselen = 0;

	fixflags = (Pws->vfs != WHEREIS_NORMAL) ?
		(VFSFIX_VFPS | VFSFIX_NOFIXEDGE) : (VFSFIX_PATH | VFSFIX_NOFIXEDGE);

	if ( Pws->listfilename != NULL ){
		tstrcpy(templistfile, Pws->listfilename);
	}else{
		MakeTempEntry(TSIZEOF(templistfile), templistfile, FILE_ATTRIBUTE_NORMAL);
	}
	sts.hFile = CreateFileL(templistfile, GENERIC_WRITE, 0,
			NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( sts.hFile == INVALID_HANDLE_VALUE ){
		result = GetLastError();
		goto fin;
	}

	sts.wlfc_flags = Pws->wlfc_flags;
	sts.Write = (sts.wlfc_flags & WLFC_UTF8) ? WriteTextUTF8 : WriteTextNative;

	if ( sts.wlfc_flags & WLFC_BOM ){
		WriteFile(sts.hFile, ListFileHeaderStr8, sizeof(UTF8HEADER) - 1, &tmp, NULL);
	}
	if ( !(sts.wlfc_flags & WLFC_NAMEONLY) ){
		if ( sts.wlfc_flags & WLFC_UTF8 ){
			WriteFile(sts.hFile, ListFileHeaderStrA, ListFileHeaderSizeA, &tmp, NULL);
		}else{
			WriteFile(sts.hFile, ListFileHeaderStr, ListFileHeaderSize, &tmp, NULL);
		}

		VFSFixPath(temp, Pws->srcpath, cinfo->path, fixflags);
		if ( GetFileAttributesL(temp) != BADATTR ){
			sts.Write(&sts, BASEHEADER, TSIZEOFSTR(BASEHEADER));
			sts.Write(&sts, temp, tstrlen32(temp));
			sts.Write(&sts, BASETYPEHEADER, TSIZEOFSTR(BASETYPEHEADER));
		}

		tmp = wsprintf(temp, T(";Search=-type:%d -marked:%s -dir:%s -subdir:%s -vfs:%s -path:\"%s\" -mask:\"%s\" -mask2:\"%s\" -text:\"%s\"\r\n"),
			Pws->st.exmode,
			GetSWstr(Pws->st.marked), GetSWstr(Pws->st.dir),
			GetSWstr(Pws->st.sdir), GetSWstr(Pws->vfs),
			Pws->srcpath, Pws->st.mask1, Pws->st.mask2, Pws->st.text
		);
		sts.Write(&sts, temp, tmp);
	}

	if ( Pws->st.exmode == WDEXM_MODULE ){
		PPXMSEARCHSTRUCT msearch;
		WHEREISAPPINFO winfo = {{(PPXAPPINFOFUNCTION)SearchReportModuleFunction, T("Where is"), NilStr, NULL}, NULL, NULL, NULL};
		PPXMODULEPARAM pmp;

		#ifndef UNICODE
			WCHAR keywordW[VFPS];

			AnsiToUnicode(Pws->st.mask1[0] ? Pws->st.mask1 : Pws->st.text, keywordW, VFPS);
			msearch.keyword = keywordW;
		#else
			msearch.keyword = Pws->st.mask1[0] ? Pws->st.mask1 : Pws->st.text;
		#endif
		msearch.maxresults = MAX32;
		msearch.searchtype = PPXH_PATH | PPXH_DIR;
		winfo.info.hWnd = cinfo->info.hWnd;
		winfo.sts = &sts;
		winfo.cinfo = cinfo;
		winfo.pws = Pws;
		pmp.search = &msearch;
		cinfo->Ref++;
		CallModule(&winfo.info, PPXMEVENT_SEARCH, pmp, NULL);
		if ( --cinfo->Ref <= 0 ){
			RequestDestroyFlag = 1;
			PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
		}
	}else{
		MakeFN_REGEXP(&Pws->fn1, Pws->st.mask1);
		MakeFN_REGEXP(&Pws->fn2, Pws->st.mask2);

		Pws->UseFnText = FALSE;
		if ( Pws->st.text[0] == '\0' ){
			Pws->st.exmode = WDEXM_UNKNOWN;
		}else{
			if ( Pws->st.text[0] == '/' ){
				Pws->UseFnText = TRUE;
				wsprintf(temp, T("o:,%s"), Pws->st.text);
				MakeFN_REGEXP(&Pws->fnText, temp);
			}
		}

		if ( Pws->st.marked != 0 ){		// マーク処理あり
			ENTRYCELL *cell;
			int work;

			InitEnumMarkCell(cinfo, &work);
			while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
				if ( IsParentDir(cell->f.cFileName) ) continue;
				if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
					VFSFixPath(temp, cell->f.cFileName, cinfo->RealPath, fixflags);
					if ( Pws->vfs == WHEREIS_NORMAL ){
						result = WhereIsDir(temp, Pws, &sts);
					}else{
						result = WhereIsDirVFS(temp, Pws, &sts);
					}
					if ( result != NO_ERROR ) break;
				}else{
					if( FinddataRegularExpression(&cell->f, &Pws->fn1) &&
						FinddataRegularExpression(&cell->f, &Pws->fn2) ){
						VFSFixPath(temp, cell->f.cFileName, cinfo->RealPath, fixflags);
						if ( Pws->st.text[0] != '\0' ){
							CheckText(Pws, &sts, temp, &cell->f);
						}else{
							Pws->hitcount++;
							WriteFF2(&sts, &cell->f, temp);
						}
					}
					if ( (result = wjobinfo(Pws, cell->f.cFileName)) != NO_ERROR ){
						break;
					}
				}
			}
		}else{					// マーク処理なし
			TCHAR *pathsrc, *pathnext, extractpath[VFPS];

			ExpandEnvironmentStrings(Pws->srcpath, temp, TSIZEOF(temp));
			pathsrc = temp;
			for ( ; ; ){
				pathnext = tstrchr(pathsrc, ';');
				if ( pathnext != NULL ){
					*pathnext = '\0';
				}
				VFSFixPath(extractpath, pathsrc, cinfo->path, fixflags);
				if ( Pws->st.exmode == WDEXM_COMMENT ){
					result = WhereIsDirComment(extractpath, Pws, &sts);
				}else if ( Pws->vfs == WHEREIS_NORMAL ){
					result = WhereIsDir(extractpath, Pws, &sts);
				}else{
					result = WhereIsDirVFS(extractpath, Pws, &sts);
				}
				if ( pathnext == NULL ) break;
				if ( result != NO_ERROR ) break;
				pathsrc = pathnext + 1;
			}
		}

		if ( IsTrue(Pws->UseFnText) ) FreeFN_REGEXP(&Pws->fnText);

		FreeFN_REGEXP(&Pws->fn2);
		FreeFN_REGEXP(&Pws->fn1);
	}
	CloseHandle(sts.hFile);
	StopPopMsg(cinfo, PMF_DISPLAYMASK);

	if ( IsTrue(Pws->readlistfile) ){
		PPcChangeDirectory(cinfo, templistfile, RENTRY_READ);
		if ( cinfo->e.Dtype.BasePath[0] != '\0' ){
			tstrcpy(cinfo->OrgPath, cinfo->e.Dtype.BasePath);
		}
		ActionInfo(cinfo->info.hWnd, &cinfo->info, AJI_COMPLETE, T("whereis"));
	}
fin:
	FreePws(Pws);
	return result;
}

ERRORCODE WhereIsDialogMain(WHERESTRUCT *Pws)
{
	if ( PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_WHERE),
			Pws->st.cinfo->info.hWnd, WhereDialogProc, (LPARAM)Pws) <= 0 ){
		FreePws(Pws);
		return ERROR_CANCELLED;
	}
	Pws->st.cinfo->WhereIsSettings = Pws->st;
	return WhereIsMain(Pws->st.cinfo, Pws);
}

ERRORCODE WhereIsDialog(PPC_APPINFO *cinfo, int mode)
{
	WHERESTRUCT ws;

	ThInit(&ws.thEcdata);

		// Shell's Namespace だったら仮想モードに移行
	if ( cinfo->RealPath[0] == '?' ) mode = WHEREIS_INVFS;

	ws.st = cinfo->WhereIsSettings;
	if ( ws.st.cinfo == NULL ){	// 新規の場合は初期化
		ws.st.cinfo = cinfo;
		ws.st.marked = FALSE;
		ws.st.dir = FALSE;
		ws.st.sdir = FILE_ATTRIBUTE_DIRECTORY;
		ws.st.exmode = WDEXM_PLAINTEXT;
		ws.st.mask1[0] = '\0';
		ws.st.mask2[0] = '\0';
		ws.st.text[0] = '\0';
	}

	tstrcpy(ws.srcpath, cinfo->path);
	ws.readlistfile = TRUE;
//	ws.appendmode = FALSE;
	ws.wlfc_flags = 0;
	ws.listfilename = NULL;
	ws.vfs = mode;
	return WhereIsDialogMain(&ws);
}

DWORD GetOnOffOption(const TCHAR *more, DWORD defvalue)
{
	if ( SkipSpace(&more) < ' ' ) return defvalue;
	return GetStringCommand(&more, T("OFF\0") T("ON\0"));
}

ERRORCODE WhereIsCommand(PPC_APPINFO *cinfo, const TCHAR *param, BOOL usedialog)
{
	WHERESTRUCT ws;
	TCHAR listfile[VFPS];
	TCHAR paramtmp[VFPS], code, *more;

	if ( IsTrue(usedialog) ){
		if ( *param == '!' ){ // 即時実行
			param++;
			usedialog = FALSE;
		}
	}
	ThInit(&ws.thEcdata);
	tstrcpy(ws.srcpath, cinfo->path);
	ws.listfilename = NULL;
	ws.readlistfile = TRUE;
//	ws.appendmode = FALSE;
	ws.wlfc_flags = 0;
	ws.vfs = FALSE;

	ws.st.marked = FALSE;
	ws.st.dir = FALSE;
	ws.st.sdir = FILE_ATTRIBUTE_DIRECTORY;
	ws.st.exmode = WDEXM_PLAINTEXT;
	ws.st.mask1[0] = '\0';
	ws.st.mask2[0] = '\0';
	ws.st.text[0] = '\0';
	listfile[0] = '\0';

	code = SkipSpace(&param);
	if ( (code != '-') && (code != '/') ){
		GetCommandParameterDual(&param, ws.srcpath, TSIZEOF(ws.srcpath));
		if ( ws.srcpath[0] == '\0' ) tstrcpy(ws.srcpath, cinfo->path);
	}
	if ( SkipSpace(&param) != ',' ){ // 空白区切り形式
		for (;;){
			code = GetOptionParameter(&param, paramtmp, &more);
			if ( code == '\0' ) break;
			if ( code != '-' ) break;
			if ( (tstrcmp(paramtmp + 1, T("MASK")) == 0) ||
				 (tstrcmp(paramtmp + 1, T("MASK1")) == 0) ){
				tstrlimcpy(ws.st.mask1, more, TSIZEOF(ws.st.mask1));
				continue;
			}
			if ( (tstrcmp(paramtmp + 1, T("MASK2")) == 0) ){
				tstrlimcpy(ws.st.mask2, more, TSIZEOF(ws.st.mask2));
				continue;
			}
			if ( (tstrcmp(paramtmp + 1, T("PATH")) == 0) ){
				tstrlimcpy(ws.srcpath, more, TSIZEOF(ws.srcpath));
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("TEXT")) == 0 ){
				tstrlimcpy(ws.st.text, more, TSIZEOF(ws.st.text));
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("DIR")) == 0 ){
				ws.st.dir = GetOnOffOption(more, TRUE);
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("SEARCH")) == 0 ){
				ws.readlistfile = FALSE;
				continue;
			}
/*
			if ( tstrcmp(paramtmp + 1, T("APPEND")) == 0 ){
				ws.appendmode = TRUE;
				ws.readlistfile = FALSE;
				continue;
			}
*/
			if ( tstrcmp(paramtmp + 1, T("SUBDIR")) == 0 ){
				ws.st.sdir = GetOnOffOption(more, TRUE);
				if ( ws.st.sdir != 0 ) ws.st.sdir = FILE_ATTRIBUTE_DIRECTORY;
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("VFS")) == 0 ){
				ws.vfs = GetOnOffOption(more, TRUE);
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("TYPE")) == 0 ){
				ws.st.exmode = GetOnOffOption(more, TRUE);
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("MARKED")) == 0 ){
				ws.st.marked = GetOnOffOption(more, TRUE);
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("NAME")) == 0 ){
				ws.readlistfile = FALSE;
				ws.wlfc_flags |= WLFC_NAMEONLY;
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("LISTFILE")) == 0 ){
				tstrlimcpy(listfile, more, TSIZEOF(listfile));
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("UTF8")) == 0 ){
				ws.wlfc_flags |= WLFC_UTF8;
				continue;
			}
			if ( tstrcmp(paramtmp + 1, T("UTF8BOM")) == 0 ){
				ws.wlfc_flags |= WLFC_UTF8 | WLFC_BOM;
				continue;
			}
		}
	}else{ // , 区切り旧形式
		param++; // "," をスキップ
		GetCommandParameter(&param, ws.st.mask1, TSIZEOF(ws.st.mask1));
		NextParameter(&param);
		GetCommandParameter(&param, ws.st.mask2, TSIZEOF(ws.st.mask2));
		NextParameter(&param);
		GetCommandParameter(&param, ws.st.text, TSIZEOF(ws.st.text));
		NextParameter(&param);
		ws.st.dir = GetNumber(&param);
		NextParameter(&param);
		if ( Isdigit(*param) ){
			ws.st.sdir = GetNumber(&param) ? FILE_ATTRIBUTE_DIRECTORY : 0;
		}else{
			ws.st.sdir = FILE_ATTRIBUTE_DIRECTORY;
 		}
		NextParameter(&param);
		ws.vfs = GetNumber(&param);
		NextParameter(&param);
		ws.st.exmode = GetNumber(&param);
		if ( (ws.st.exmode <= WDEXM_UNKNOWN) ||
			 (ws.st.exmode == WDEXM_COLUMNMIN) ||
			 (ws.st.exmode > WDEXM_MODULE) ){
			ws.st.exmode = WDEXM_PLAINTEXT;
		}
		NextParameter(&param);
		ws.st.marked = GetNumber(&param);
		NextParameter(&param);
		GetCommandParameter(&param, listfile, TSIZEOF(listfile));
	}

	if ( listfile[0] != '\0' ){
		VFSFixPath(NULL, listfile, cinfo->path, VFSFIX_VFPS | VFSFIX_NOFIXEDGE);
		ws.listfilename = listfile;
	}

	ws.st.cinfo = cinfo;

	if ( IsTrue(usedialog) ){
		return WhereIsDialogMain(&ws);
	}else{
		return WhereIsMain(cinfo, &ws);
	}
}
