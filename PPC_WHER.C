/*-----------------------------------------------------------------------------
	Paper Plane cUI												Where is
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define PAUSECOUNT	10
#define WHERE_NOSRCINIT -10

const TCHAR BHEADER[] = T(";Base=");
const TCHAR BTHEADER[] = T("|1\r\n");

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
	InsertMacroMenuSourceE,InsertMacroMenuSourceJ
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
	InsertMacroMenuMaskE,InsertMacroMenuMaskJ
};

const TCHAR *ExWhereMenuList[] = {
	MES_TPTX,	// テキスト検索
	MES_TCMT,	// コメント
	MES_TCIT,	// チップテキスト
	MES_TCLE,	// カラム拡張
	MES_TSMD,
	NULL
};
#define CM_EXT 1000

typedef struct {
	BOOL result;
	WHERESTRING *keyword;
} TIPMATCHSTRUCT;


BOOL CheckStringMatch(const TCHAR *target,WHERESTRING *sm);

typedef struct {
	PPXAPPINFO info;
	HANDLE hFile;
	PPC_APPINFO *cinfo;
	WHEREDIALOG *pws;
} WHEREISAPPINFO;

const TCHAR *swstr[] = { T("off"),T("on") };

void ExWhereMenu(WHEREDIALOG *pws,HWND hDlg,HWND hButtonWnd)
{
	HMENU hMenu,hColumnMenu;
	int type;
	const TCHAR **menus;
	RECT box;

	hMenu = CreatePopupMenu();
	menus = ExWhereMenuList;
	for ( type = WDEXM_UNKNOWN ; type <= WDEXM_COMMENT ; type++,menus++ ){
		AppendMenuString(hMenu,type + 1,*menus);
	}
	hColumnMenu = CreatePopupMenu();
	GetColumnExtMenu(&pws->thEcdata,pws->cinfo->RealPath,hColumnMenu,CM_EXT);
	AppendMenu(hMenu,MF_EPOP,(UINT_PTR)hColumnMenu,MessageText(*menus));

	AppendMenuString(hMenu,WDEXM_MODULE,ExWhereMenuList[WDEXM_MODULE - 1]);

	GetWindowRect(hButtonWnd,&box);
	type = TrackPopupMenu(hMenu,TPM_TDEFAULT,box.left,box.bottom,0,hDlg,NULL);
	DestroyMenu(hMenu);

	if ( type > WDEXM_UNKNOWN ){
		if ( type >= CM_EXT ){
			pws->indexEc = type - CM_EXT;
			type = WDEXM_COLUMNMIN;
		}
		pws->exmode = type;
		SendMessage(hButtonWnd,WM_SETTEXT,0,(LPARAM)MessageText(ExWhereMenuList[type - 1]));
	}
}

void ButtonHelpMenu(PPC_APPINFO *cinfo,HWND hDlg,HWND hButtonWnd,UINT editID,int type)
{
	HMENU hMenu;
	int id = 1;
	RECT box;
	POINT pos;
	const TCHAR **menustr,***menulists,**menulist,*usermenu;
	TCHAR param[CMDLINESIZE];
	ERRORCODE result = ERROR_INVALID_FUNCTION;

	GetWindowRect(hButtonWnd,&box);
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
		result = PP_ExtractMacro(hDlg,&cinfo->info,&pos,usermenu,param,0);
		if ( result == ERROR_CANCELLED ) return;
	}
	if ( result != NO_ERROR ){
		param[0] = '\0';
		menulist = menulists[(LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE) ? 1 : 0];

		hMenu = CreatePopupMenu();
		for ( menustr = menulist ; *menustr != NULL ; menustr++ ){
			AppendMenuString(hMenu,id++,*menustr);
		}
		id = TrackPopupMenu(hMenu,TPM_TDEFAULT,box.left,box.bottom,0,hDlg,NULL);
		DestroyMenu(hMenu);
		if ( id ){
			TCHAR *p;
			tstrcpy(param,menulist[id - 1]);
			p = tstrchr(param,'\t');
			if ( p != NULL ) *p = '\0';
		}
	}
	if ( param[0] != '\0' ){
		if ( type >= 0 ){
			SendDlgItemMessage(hDlg,editID,EM_REPLACESEL,0,(LPARAM)param);
		}else{
			SetDlgItemText(hDlg,editID,param);
		}
	}
	SetFocus(GetDlgItem(hDlg,editID));
}

void WINAPI TipMatchCallback(TIPMATCHSTRUCT *tms,const TCHAR *text)
{
	tms->result = CheckStringMatch(text,tms->keyword);
}

BOOL TipMatch(const TCHAR *name,WHERESTRING *str)
{
	TIPMATCHSTRUCT tms;
	const TCHAR *ls;

	tms.keyword = str;
	ls = VFSFindLastEntry(name);
	GetInfoTipText(name,(ls - name) + FindExtSeparator(ls),(GETINFOTIPCALLBACK)TipMatchCallback,(void *)&tms);
	return tms.result;
}

BOOL ColumnMatch(WHEREDIALOG *pws,const TCHAR *name,WIN32_FIND_DATA *ff,WHERESTRING *str)
{
	TIPMATCHSTRUCT tms;

	tms.keyword = str;
	ExtGetData(&pws->thEcdata,pws->indexEc,name,ff->dwFileAttributes,(GETINFOTIPCALLBACK)TipMatchCallback,(void *)&tms);
	return tms.result;
}

BOOL FileMatch(const TCHAR *name,WHERESTRING *sm)
{
	TCHAR *image,*text;
	BOOL result;

	if ( NOERROR != LoadTextImage(name,&image,&text,NULL) ){
		return FALSE;
	}
	result = CheckStringMatch(text,sm);
	HeapFree( hProcessHeap,0,image);
	return result;
}
#if !NODLL
TCHAR *tstristr(const TCHAR *target,const TCHAR *findstr)
{
	size_t len,flen;
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
BOOL CheckStringMatch(const TCHAR *target,WHERESTRING *sm)
{
	if ( !sm->mode ){	// 部分一致
		return tstristr(target,sm->string) ? TRUE : FALSE;
	}else{		// 正規表現
		return FilenameRegularExpression(target,&sm->fn);
	}
}

ERRORCODE wjobinfo(WHEREDIALOG *pws,TCHAR *dir)
{
	DWORD NewTime;
	MSG msg;
	TCHAR mesbuf[CMDLINESIZE];

	if ( --pws->pausecount ) return NO_ERROR;
	pws->pausecount = PAUSECOUNT;

	while ( PeekMessage(&msg,NULL,0,0,PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( X_MultiThread || (msg.hwnd == pws->hWnd) ){
			if ( (msg.message == WM_RBUTTONUP) ||
				 ((msg.message == WM_KEYDOWN) &&
				 (((int)msg.wParam == VK_ESCAPE)||((int)msg.wParam == VK_PAUSE)))){
				if ( PMessageBox(pws->hWnd,NULL,dir,MB_QYES) == IDOK ){
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
			wsprintf(mesbuf,T("Hit:%d  %s"),pws->hitcount,dir);
		}else{
			wsprintf(mesbuf,T("%s"),dir);
		}
		SetPopMsg(pws->cinfo,POPMSG_PROGRESSBUSYMSG,mesbuf);
		UpdateWindow_Part(pws->cinfo->info.hWnd);
	}
	return NO_ERROR;
}

void MarkBo(WHEREDIALOG *pws,HWND hDlg)
{
	if ( IsDlgButtonChecked(hDlg,IDX_WHERE_MARK) ){
		SetDlgItemText(hDlg,IDC_WHERE_SRC,pws->cinfo->RealPath);
		EnableDlgWindow(hDlg,IDC_WHERE_SRC,FALSE);
	}else{
		EnableDlgWindow(hDlg,IDC_WHERE_SRC,TRUE);
	}
}

void InitWhereFileMaskText(HWND hDlg,WHEREDIALOG *PWS,UINT control,const TCHAR *text,DWORD wordmatch)
{
	PPxRegistExEdit(&PWS->cinfo->info,GetDlgItem(hDlg,control),
		TSIZEOFSTR(PWS->file1),
		(wordmatch && (memcmp(text,StrWordMatchWildCard,SIZEOFTSTR(StrWordMatchWildCard) ) == 0)) ? (text + TSIZEOFSTR(StrWordMatchWildCard)) : text,
		PPXH_WILD_R,PPXH_MASK,0);
}

void InitWhereDialog(HWND hDlg,LPARAM lParam)
{
	WHEREDIALOG *PWS;
	WildCardOptions option;

	PWS = (WHEREDIALOG *)lParam;
	SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)lParam);
	CenterPPcDialog(hDlg,PWS->cinfo);
	LocalizeDialogText(hDlg,IDD_WHERE);

	GetCustData(StrXC_rmsk,&option,sizeof(option));

	PWS->hEdWnd = PPxRegistExEdit(&PWS->cinfo->info,
		GetDlgItem(hDlg,IDC_WHERE_SRC),
		TSIZEOFSTR(PWS->src),PWS->src,PPXH_DIR_R,PPXH_DIR,
		PPXEDIT_WANTEVENT | PPXEDIT_REFTREE);
	InitWhereFileMaskText(hDlg,PWS,IDC_WHERE_F1,PWS->file1,option.wordmatch);
	InitWhereFileMaskText(hDlg,PWS,IDC_WHERE_F2,PWS->file2,option.wordmatch);
	PPxRegistExEdit(&PWS->cinfo->info,GetDlgItem(hDlg,IDC_WHERE_STR),
		TSIZEOFSTR(PWS->str),PWS->str.string,PPXH_WILD_R,PPXH_SEARCH,0);

	CheckDlgButton(hDlg,IDX_WHERE_MARK,PWS->marks);
	CheckDlgButton(hDlg,IDX_WHERE_DIR,PWS->dir);
	CheckDlgButton(hDlg,IDX_WHERE_SDIR,PWS->sdir);
	CheckDlgButton(hDlg,IDX_WHERE_VFS,PWS->vfs);
	CheckDlgButton(hDlg,IDX_MASK_WORDMATCH,option.wordmatch);
	MarkBo(PWS,hDlg);
	SetFocus(GetDlgItem(hDlg,IDC_WHERE_F1));

	if ( (PWS->exmode == WDEXM_UNKNOWN) ||
		 (PWS->exmode == WDEXM_COLUMNMIN) ){
		PWS->exmode = WDEXM_PLAINTEXT;
	}

	SendMessage(GetDlgItem(hDlg,IDS_WHERE_STR),WM_SETTEXT,
			0,(LPARAM)MessageText(ExWhereMenuList[PWS->exmode - 1]));
}

void GetMaskTextFromEdit(HWND hDlg,UINT control,TCHAR *text,DWORD wordmatch)
{
	GetDlgItemText(hDlg,control,text,MAX_PATH);
	text[MAX_PATH - 1] = '\0';
	if ( wordmatch &&
		 (*text != '\0') &&
		 (tstrlen(text) < (MAX_PATH - TSIZEOF(StrWordMatchWildCard))) &&
		 (memcmp(text,StrWordMatchWildCard,SIZEOFTSTR(StrWordMatchWildCard) ) != 0)
	){
		memmove((BYTE *)(text) + SIZEOFTSTR(StrWordMatchWildCard),text,TSTRSIZE(text));
		memcpy(text,StrWordMatchWildCard,SIZEOFTSTR(StrWordMatchWildCard));
	}
	WriteHistory(PPXH_MASK,text,0,NULL);
}

void OkWhereDialog(HWND hDlg,WHEREDIALOG *PWS)
{
	WildCardOptions option;

	GetCustData(StrXC_rmsk,&option,sizeof(option));
	option.wordmatch = IsDlgButtonChecked(hDlg,IDX_MASK_WORDMATCH);
	SetCustData(StrXC_rmsk,&option,sizeof(option));

	GetDlgItemText(hDlg,IDC_WHERE_SRC,PWS->src,TSIZEOF(PWS->src));
	WriteHistory(PPXH_DIR,PWS->src,0,NULL);

	GetMaskTextFromEdit(hDlg,IDC_WHERE_F1,PWS->file1,option.wordmatch);
	GetMaskTextFromEdit(hDlg,IDC_WHERE_F2,PWS->file2,option.wordmatch);
	GetDlgItemText(hDlg,IDC_WHERE_STR,PWS->str.string,TSIZEOF(PWS->str.string));
	WriteHistory(PPXH_SEARCH,PWS->str.string,0,NULL);

	PWS->marks = IsDlgButtonChecked(hDlg,IDX_WHERE_MARK);
	PWS->dir = IsDlgButtonChecked(hDlg,IDX_WHERE_DIR);
	PWS->sdir = IsDlgButtonChecked(hDlg,IDX_WHERE_SDIR) ?
			FILE_ATTRIBUTE_DIRECTORY : 0;
	PWS->vfs = IsDlgButtonChecked(hDlg,IDX_WHERE_VFS);
	EndDialog(hDlg,1);
}

INT_PTR CALLBACK WhereDialogProc(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch (message){
							// ダイアログ初期化 (w:focus,l:User からの param)--
		case WM_INITDIALOG:
			InitWhereDialog(hDlg,lParam);
			return FALSE;

		case WM_COMMAND: {
			WHEREDIALOG *PWS;

			PWS = (WHEREDIALOG *)GetWindowLongPtr(hDlg,DWLP_USER);
			switch ( LOWORD(wParam) ){
				case IDX_WHERE_MARK:
					MarkBo(PWS,hDlg);
					break;

				case IDOK:
					OkWhereDialog(hDlg,PWS);
					break;

				case IDCANCEL:
					EndDialog(hDlg,0);
					break;

				case IDS_WHERE_SRC:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->cinfo,hDlg,(HWND)lParam,IDC_WHERE_SRC,0);
					}
					break;
				case IDS_WHERE_F1:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->cinfo,hDlg,(HWND)lParam,IDC_WHERE_F1,1);
					}
					break;
				case IDS_WHERE_F2:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PWS->cinfo,hDlg,(HWND)lParam,IDC_WHERE_F2,1);
					}
					break;

				case IDS_WHERE_STR:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ExWhereMenu(PWS,hDlg,(HWND)lParam);
						SetFocus(GetDlgItem(hDlg,IDC_WHERE_STR));
					}
					break;

				case IDB_REF:
					SendMessage(PWS->hEdWnd,
							WM_PPXCOMMAND,K_raw | K_s | K_c | 'I',0);
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg,WM_HELP,wParam,lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)IDD_WHERE);
					break;
			}
			break;
		}
		default:
			return PPxDialogHelper(hDlg,message,wParam,lParam);
	}
	return TRUE;
}

// "filename","alternate",A:atr,C:create,L:access,W:write,S:size,comment
void WriteFF(HANDLE hFile,WIN32_FIND_DATA *ff,const TCHAR *name)
{
	TCHAR buf[VFPS + 900],*dest;
	DWORD tmp;
	int len;

	dest = buf;
	*dest++ = T('\"');
	len = TSTRLENGTH32(name);
	memcpy(dest,name,len);
	dest += len / sizeof(TCHAR);

	dest += wsprintf(dest,ListFileFormatStr,
			ff->cAlternateFileName,ff->dwFileAttributes,
			ff->ftCreationTime.dwHighDateTime,
			ff->ftCreationTime.dwLowDateTime,
			ff->ftLastAccessTime.dwHighDateTime,
			ff->ftLastAccessTime.dwLowDateTime,
			ff->ftLastWriteTime.dwHighDateTime,
			ff->ftLastWriteTime.dwLowDateTime,
			ff->nFileSizeHigh,ff->nFileSizeLow);

	*dest ++ = '\r';
	*dest ++ = '\n';
	WriteFile(hFile,buf,TSTROFF32(dest - buf),&tmp,NULL);
}

void CheckText(WHEREDIALOG *Pws,HANDLE hFile,const TCHAR *name,WIN32_FIND_DATA *ff)
{
	if ( Pws->exmode >= WDEXM_TIPTEXT ){
		if ( Pws->exmode == WDEXM_COLUMNMIN ){
			if ( IsTrue(ColumnMatch(Pws,name,ff,&Pws->str)) ){
				Pws->hitcount++;
				WriteFF(hFile,ff,name + Pws->baselen);
			}
		}else if ( IsTrue(TipMatch(name,&Pws->str)) ){
			Pws->hitcount++;
			WriteFF(hFile,ff,name + Pws->baselen);
		}
	}else{
		if ( !ff->nFileSizeHigh &&
			 (ff->nFileSizeLow < Pws->imgsize) &&
			 IsTrue(FileMatch(name,&Pws->str)) ){
			Pws->hitcount++;
			WriteFF(hFile,ff,name + Pws->baselen);
		}
	}
}

ERRORCODE WhereIsDirComment(TCHAR *dir,WHEREDIALOG *pws,HANDLE hFile)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA	ff;		// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ThSTRUCT comments;
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR;
	// このディレクトリのコメントファイルのチェック
	ThInit(&comments);
	CatPath(src,dir,T("00_INDEX.TXT"));
	GetCommentText(&comments,src);
	if ( comments.bottom != NULL ){
		TCHAR *p;

		p = (TCHAR *)comments.bottom;
		while( *p ){
			TCHAR *cp;

			cp = p + tstrlen(p) + 1;
			if( FilenameRegularExpression(p,&pws->fn1) &&
				FilenameRegularExpression(p,&pws->fn2) ){
				if ( CheckStringMatch(cp,&pws->str) ){
					CatPath(name,dir,p);
					hFF = FindFirstFileL(src,&ff);
					if ( hFF != INVALID_HANDLE_VALUE ){
						FindClose(hFF);
						pws->hitcount++;
						WriteFF(hFile,&ff,name + pws->baselen);
					}
				}
			}
			p = cp + tstrlen(cp) + 1;
		}
	}
	ThFree(&comments);

	CatPath(src,dir,T("*"));
	hFF = FindFirstFileL(src,&ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR;
	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;
		CatPath(name,dir,ff.cFileName);

		if ( ff.dwFileAttributes & pws->sdir ){
			if ( (result = WhereIsDirComment(name,pws,hFile)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws,name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(FindNextFile(hFF,&ff)) );
	result = NO_ERROR;
end:
	VFSFindClose(hFF);
	return result;
}

ERRORCODE WhereIsDir(const TCHAR *dir,WHEREDIALOG *pws,HANDLE hFile)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA ff;	// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR; // return ERROR_FILENAME_EXCED_RANGE;
	CatPath(src,(TCHAR *)dir,T("*"));
	hFF = FindFirstFileL(src,&ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR; // return ERROR_PATH_NOT_FOUND;
	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;

		name[MAX_PATH] = '\0'; // 名前の大きさ検出用
		CatPath(name,(TCHAR *)dir,ff.cFileName);

		if( FinddataRegularExpression(&ff,&pws->fn1) &&
			FinddataRegularExpression(&ff,&pws->fn2) ){
			if (pws->dir || !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						pws->str.string[0] ){
					CheckText(pws,hFile,name,&ff);
				}else{
					pws->hitcount++;

					if ( (name[MAX_PATH] != '\0') && !memcmp(name,pws->src,TSTRLENGTH(pws->src)) ){
						TCHAR *p = name + tstrlen(pws->src); // 共通パスを省略して名前を少し縮める
						if ( *p == '\\' ) p++;
						WriteFF(hFile,&ff,p);
					}else{
						WriteFF(hFile,&ff,name + pws->baselen);
					}
				}
			}
		}

		if ( ff.dwFileAttributes & pws->sdir ){
			if ( (result = WhereIsDir(name,pws,hFile)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws,name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(FindNextFile(hFF,&ff)) );
	result = NO_ERROR;
end:
	FindClose(hFF);
	return result;
}

ERRORCODE WhereIsDirEx(TCHAR *dir,WHEREDIALOG *pws,HANDLE hFile)
{
	HANDLE hFF;	// FindFile 用ハンドル
	WIN32_FIND_DATA ff;		// ファイル情報
	TCHAR src[VFPS];
	TCHAR name[VFPS];
	ERRORCODE result;
	int dirlen = tstrlen32(dir);

	if ( (dirlen + 4) >= VFPS ) return NO_ERROR;
	CatPath(src,dir,T("*"));
	hFF = VFSFindFirst(src,&ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return NO_ERROR;

	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if ( (dirlen + tstrlen(ff.cFileName) + 4) >= VFPS ) continue;
		CatPath(name,dir,ff.cFileName);

		if( FinddataRegularExpression(&ff,&pws->fn1) &&
			FinddataRegularExpression(&ff,&pws->fn2) ){
			if (pws->dir || !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
														pws->str.string[0] ){
					CheckText(pws,hFile,name,&ff);
				}else{
					pws->hitcount++;
					WriteFF(hFile,&ff,name + pws->baselen);
				}
			}
		}

		if ( pws->sdir ){
/*
			int Dtype;
			TCHAR typename[VFSGETFFINFO_TYPESIZE];
			void *ftable;

			VFSGetFFInfo(hFF,&Dtype,typename,&ftable);
*/
			if ( (result = WhereIsDirEx(name,pws,hFile)) != NO_ERROR ){
				goto end;
			}
		}
		if ( (result = wjobinfo(pws,name)) != NO_ERROR ){
			goto end;
		}
	}while( IsTrue(VFSFindNext(hFF,&ff)) );
	result = NO_ERROR;
end:
	VFSFindClose(hFF);
	return result;
}

DWORD_PTR USECDECL SearchReportModuleFunction(WHEREISAPPINFO *winfo,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	TCHAR buf[VFPS + 900],*name;
	DWORD tmp;
	int len;

	if ( cmdID == PPXCMDID_REPORTSEARCH_FDATA ){
		name = ((WIN32_FIND_DATA *)uptr)->cFileName;
		WriteFF(winfo->hFile,(WIN32_FIND_DATA *)uptr,name);
	}else if ( (cmdID == PPXCMDID_REPORTSEARCH) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_FILE) ||
		 (cmdID == PPXCMDID_REPORTSEARCH_DIRECTORY) ){
		if ( cmdID == PPXCMDID_REPORTSEARCH ){
			len = wsprintf(buf,T("\"%s\"\r\n"),uptr->str);
		}else{
			len = wsprintf(buf,T("\"%s\",A:%d\r\n"),uptr->str,
				(cmdID == PPXCMDID_REPORTSEARCH_DIRECTORY) ?
					FILE_ATTRIBUTE_DIRECTORY : 0);
		}
		WriteFile(winfo->hFile,buf,TSTROFF(len),&tmp,NULL);
		name = uptr->str;
	}else{
		return winfo->cinfo->info.Function((PPXAPPINFO *)winfo->cinfo,cmdID,uptr);
	}
	winfo->pws->hitcount++;
	return (wjobinfo(winfo->pws,name) == NO_ERROR) ? 1 : 2;
}

const TCHAR * USEFASTCALL GetSWstr(BOOL state)
{
	return swstr[state ? 1 : 0];
}

ERRORCODE WhereIsMain(PPC_APPINFO *cinfo,WHEREDIALOG *Pws)
{
	HANDLE hFile;
	TCHAR templistfile[VFPS],temp[CMDLINESIZE * 3];
	DWORD tmp;
	int fixflags;
	ERRORCODE result = NO_ERROR;

	Pws->pausecount = PAUSECOUNT;
	Pws->hitcount = 0;
	Pws->hWnd = cinfo->info.hWnd;
	Pws->OldTime = GetTickCount();
	Pws->imgsize = GetCustXDword(T("X_wsiz"),NULL,IMAGESIZELIMIT);
	Pws->cinfo = cinfo;
	Pws->baselen = 0;

	fixflags = Pws->vfs ?
		(VFSFIX_VFPS | VFSFIX_NOFIXEDGE) : (VFSFIX_PATH | VFSFIX_NOFIXEDGE);

	if ( Pws->listfilename != NULL ){
		tstrcpy(templistfile,Pws->listfilename);
	}else{
		MakeTempEntry(TSIZEOF(templistfile),templistfile,FILE_ATTRIBUTE_NORMAL);
	}
	hFile = CreateFileL(templistfile,GENERIC_WRITE,0,
			NULL,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return GetLastError();
	WriteFile(hFile,ListFileHeaderStr,ListFileHeaderStrLen,&tmp,NULL);

	VFSFixPath(temp,Pws->src,cinfo->path,fixflags);
	if ( GetFileAttributesL(temp) != BADATTR ){
		WriteFile(hFile,BHEADER,SIZEOFTSTR(BHEADER),&tmp,NULL);
		WriteFile(hFile,temp,TSTRLENGTH32(temp),&tmp,NULL);
		WriteFile(hFile,BTHEADER,SIZEOFTSTR(BTHEADER),&tmp,NULL);
	}

	tmp = wsprintf(temp,T(";Search=/type:\"%s\" /marks:%s /dir:%s /subdir:%s /vfs:%s /path:\"%s\" /mask:\"%s\" /mask2:\"%s\" /text:\"%s\"\r\n"),
		ExWhereMenuList[Pws->exmode - 1] + MESTEXTIDLEN + 1,
		GetSWstr(Pws->marks),GetSWstr(Pws->dir),
		GetSWstr(Pws->sdir),GetSWstr(Pws->vfs),
		Pws->src,Pws->file1,Pws->file2,Pws->str.string
	);
	WriteFile(hFile,temp,TSTROFF(tmp),&tmp,NULL);

	if ( Pws->exmode == WDEXM_MODULE ){
		PPXMSEARCHSTRUCT msearch;
		WHEREISAPPINFO winfo = {{(PPXAPPINFOFUNCTION)SearchReportModuleFunction,T("Where is"),NilStr,NULL},NULL,NULL,NULL};
		PPXMODULEPARAM pmp;

		#ifndef UNICODE
			WCHAR keywordW[VFPS];

			AnsiToUnicode(Pws->file1[0] ? Pws->file1 : Pws->str.string,keywordW,VFPS);
			msearch.keyword = keywordW;
		#else
			msearch.keyword = Pws->file1[0] ? Pws->file1 : Pws->str.string;
		#endif
		msearch.maxresults = MAX32;
		msearch.searchtype = PPXH_PATH | PPXH_DIR;
		winfo.info.hWnd = cinfo->info.hWnd;
		winfo.hFile = hFile;
		winfo.cinfo = cinfo;
		winfo.pws = Pws;
		pmp.search = &msearch;
		cinfo->Ref++;
		CallModule(&winfo.info,PPXMEVENT_SEARCH,pmp,NULL);
		if ( --cinfo->Ref <= 0 ){
			RequestDestroyFlag = 1;
			PostThreadMessage(GetCurrentThreadId(),WM_NULL,0,0);
		}
	}else{
		MakeFN_REGEXP(&Pws->fn1,Pws->file1);
		MakeFN_REGEXP(&Pws->fn2,Pws->file2);

		Pws->str.mode = 0;
		if ( Pws->str.string[0] == '\0' ){
			Pws->exmode = WDEXM_UNKNOWN;
		}else{
			if ( Pws->str.string[0] == '/' ){
				Pws->str.mode = 1;
				wsprintf(temp,T("o:,%s"),Pws->str.string);
				MakeFN_REGEXP(&Pws->str.fn,temp);
			}
		}

		if ( Pws->marks != 0 ){		// マーク処理あり
			ENTRYCELL *cell;
			int work;

			InitEnumMarkCell(cinfo,&work);
			while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
				if ( IsParentDir(cell->f.cFileName) ) continue;
				if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
					VFSFixPath(temp,cell->f.cFileName,cinfo->RealPath,fixflags);
					if ( Pws->vfs ){
						result = WhereIsDirEx(temp,Pws,hFile);
					}else{
						result = WhereIsDir(temp,Pws,hFile);
					}
				}else{
					if( FinddataRegularExpression(&cell->f,&Pws->fn1) &&
						FinddataRegularExpression(&cell->f,&Pws->fn2) ){
						VFSFixPath(temp,cell->f.cFileName,cinfo->RealPath,fixflags);
						if ( Pws->str.string[0] ){
							CheckText(Pws,hFile,temp,&cell->f);
						}else{
							Pws->hitcount++;
							WriteFF(hFile,&cell->f,temp);
						}
					}
				}
			}
		}else{					// マーク処理なし
			TCHAR *pathsrc,*pathnext,extractpath[VFPS];

			ExpandEnvironmentStrings(Pws->src,temp,TSIZEOF(temp));
			pathsrc = temp;
			for ( ; ; ){
				pathnext = tstrchr(pathsrc,';');
				if ( pathnext != NULL ){
					*pathnext = '\0';
				}
				VFSFixPath(extractpath,pathsrc,cinfo->path,fixflags);
				if ( Pws->exmode == WDEXM_COMMENT ){
					result = WhereIsDirComment(extractpath,Pws,hFile);
				}else if ( Pws->vfs ){
					result = WhereIsDirEx(extractpath,Pws,hFile);
				}else{
					result = WhereIsDir(extractpath,Pws,hFile);
				}
				if ( pathnext == NULL ) break;
				pathsrc = pathnext + 1;
			}
		}

		if ( Pws->str.mode ) FreeFN_REGEXP(&Pws->str.fn);

		FreeFN_REGEXP(&Pws->fn2);
		FreeFN_REGEXP(&Pws->fn1);
	}
	CloseHandle(hFile);
	StopPopMsg(cinfo,PMF_DISPLAYMASK);

	if ( Pws->thEcdata.bottom != NULL ){
		GetColumnExtMenu(&Pws->thEcdata,NULL,NULL,0); // thEcdata 解放
	}
	if ( Pws->exmode == WDEXM_COLUMNMIN ) Pws->exmode = WDEXM_UNKNOWN;
	PPcChangeDirectory(cinfo,templistfile,RENTRY_READ);
	if ( cinfo->e.Dtype.BasePath[0] != '\0' ){
		tstrcpy(cinfo->OrgPath,cinfo->e.Dtype.BasePath);
	}
	ActionInfo(cinfo->info.hWnd,&cinfo->info,AJI_COMPLETE,T("whereis"));
	return result;
}

ERRORCODE WhereIs(PPC_APPINFO *cinfo,int mode)
{
	WHEREDIALOG *Pws;

		// Shell's Namespace だったら仮想モードに移行
	if ( cinfo->RealPath[0] == '?' ) mode = TRUE;
	Pws = &cinfo->Pws;
	if ( mode < 0 ){
		mode -= WHERE_NOSRCINIT;
	}else{
		tstrcpy(Pws->src,cinfo->path);
	}
	Pws->vfs = mode;
	if ( Pws->cinfo == NULL ){	// 新規の場合は初期化
		Pws->file1[0] = '\0';
		Pws->file2[0] = '\0';
		Pws->str.string[0] = '\0';
		Pws->marks = FALSE;
		Pws->dir = FALSE;
		Pws->sdir = FILE_ATTRIBUTE_DIRECTORY;
		Pws->exmode = WDEXM_PLAINTEXT;
		Pws->listfilename = NULL;
		ThInit(&Pws->thEcdata);
	}
	Pws->cinfo = cinfo;
	if ( PPxDialogBoxParam(hInst,MAKEINTRESOURCE(IDD_WHERE),
			cinfo->info.hWnd,WhereDialogProc,(LPARAM)Pws) <= 0 ){
		return ERROR_CANCELLED;
	}
	return WhereIsMain(cinfo,Pws);
}

ERRORCODE WhereIsCommand(PPC_APPINFO *cinfo,const TCHAR *param,BOOL usedialog)
{
	WHEREDIALOG ws;
	TCHAR listfile[VFPS];

	if ( IsTrue(usedialog) ){
		if ( *param == '!' ){ // 即時実行
			param++;
			usedialog = FALSE;
		}
	}
	ThInit(&ws.thEcdata);
	GetCommandParameter(&param,ws.src,TSIZEOF(ws.src));
	NextParameter(&param);
	GetCommandParameter(&param,ws.file1,TSIZEOF(ws.file1));
	NextParameter(&param);
	GetCommandParameter(&param,ws.file2,TSIZEOF(ws.file2));
	NextParameter(&param);
	GetCommandParameter(&param,ws.str.string,TSIZEOF(ws.str.string));
	NextParameter(&param);
	ws.dir = GetNumber(&param);
	NextParameter(&param);
	if ( Isdigit(*param) ){
		ws.sdir = GetNumber(&param) ? FILE_ATTRIBUTE_DIRECTORY : 0;
	}else{
		ws.sdir = FILE_ATTRIBUTE_DIRECTORY;
 	}
	NextParameter(&param);
	ws.vfs = GetNumber(&param);
	NextParameter(&param);
	ws.exmode = GetNumber(&param);
	if ( (ws.exmode <= WDEXM_UNKNOWN) || (ws.exmode == WDEXM_COLUMNMIN) ||
		(ws.exmode > WDEXM_MODULE) ){
		ws.exmode = WDEXM_PLAINTEXT;
	}
	NextParameter(&param);
	ws.marks = GetNumber(&param);
	NextParameter(&param);
	ws.listfilename = NULL;
	GetCommandParameter(&param,listfile,TSIZEOF(listfile));
	if ( listfile[0] != '\0' ){
		VFSFixPath(NULL,listfile,cinfo->path,VFSFIX_VFPS | VFSFIX_NOFIXEDGE);
		ws.listfilename = listfile;
	}

	ws.cinfo = cinfo;

	if ( IsTrue(usedialog) ){
		cinfo->Pws = ws;
		return WhereIs(cinfo,ws.vfs + WHERE_NOSRCINIT);
	}else{
		return WhereIsMain(cinfo,&ws);
	}
}
