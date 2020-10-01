/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作,ダイアログ関連
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#include "VFS_FOP.H"
#include "FATTIME.H"
#pragma hdrstop

const TCHAR StrFopTitle[] = MES_IDD_FOP;
const TCHAR optstr[] = T("%2%\\\0") T("\0") T("001");
const int DialogID[] = {IDD_FOP_GENERAL, IDD_FOP_RENAME, IDD_FOP_OPTION};

void InitControlData(FOPSTRUCT *FS, int id);

#define FDLC_MOVE	0
#define FDLC_RESIZE	1
#define FDLC_LOG	2

typedef struct {
	int ID;
	int type;
} FIXDIALOGCONTROLS;
const FIXDIALOGCONTROLS SizeFixFopID[] = {
	{IDOK, FDLC_MOVE},
	{IDCANCEL, FDLC_MOVE},
	{IDB_TEST, FDLC_MOVE},
	{IDHELP, FDLC_MOVE},
	{IDC_FOP_MODE, FDLC_MOVE},
	{IDS_FOP_SRCNAME, FDLC_RESIZE},
	{IDS_FOP_PROGRESS, FDLC_RESIZE},
	{IDE_FOP_LOG, FDLC_LOG},
	{IDC_FOP_DESTDIR, FDLC_RESIZE},
	{IDB_REF, FDLC_MOVE},
	{IDE_FOP_RENAME, FDLC_RESIZE},
	{0, 0}
};

void GetDivideSize(const TCHAR *param, VFSFOP_OPTIONS *fop)
{
	const TCHAR *oldp;

	oldp = param;
	fop->divide_num = (DWORD)GetNumber(&param);
	if ( oldp == param ){
		fop->divide_num = 0;
		fop->divide_unit = '\0';
		return;
	}else{
		fop->divide_unit = (BYTE)*param;
	}
}

void FixDialogControlsSize(HWND hDlg, const FIXDIALOGCONTROLS *fdc, int RefID)
{
	HWND hRefWnd, hCtlWnd;
	RECT box, refbox;
	POINT pos = {0, 0};
	int width, clientbottom;

	memset(&box, 0, sizeof(box));
	box.right = 3;
	MapDialogRect(hDlg, &box);
	width = box.right;

	GetClientRect(hDlg, &box);
	pos.x = box.right;
	pos.y = box.bottom;
	ClientToScreen(hDlg, &pos);
	clientbottom = pos.y;

	hRefWnd = GetDlgItem(hDlg, RefID);
	GetWindowRect(hRefWnd, &refbox);
	width = pos.x - width - refbox.right;

	pos.x = pos.y = 0;
	ClientToScreen(hDlg, &pos);

	for ( ; fdc->ID != 0 ; fdc++ ){
		hCtlWnd = GetDlgItem(hDlg, fdc->ID);
		if ( hCtlWnd == NULL ) continue;

		GetWindowRect(hCtlWnd, &box);
		if ( fdc->type == FDLC_RESIZE ){
			SetWindowPos(hCtlWnd, NULL, 0, 0,
					box.right - box.left + width,
					box.bottom - box.top,
					SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);
		}else if ( fdc->type == FDLC_MOVE ){
			SetWindowPos(hCtlWnd, NULL,
					box.left - pos.x + width,
					box.top - pos.y,
					0, 0,
					SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER);
		}else{ // FDLC_LOG
			clientbottom -= box.top;
			if ( clientbottom < 4 ) clientbottom = 4;
			SetWindowPos(hCtlWnd, NULL, 0, 0,
					box.right - box.left + width, clientbottom,
					SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);
		}
	}
	InvalidateRect(hDlg, NULL, TRUE);
}

void ShowSourceTextOnTip(HWND hWnd)
{
	TCHAR text[VFPS];
	TOOLINFO ti;

	text[0] = '\0';
	GetWindowText(hWnd, text, TSIZEOF(text));
	if ( tstrlen(text) < 30 ) return;

	if ( hTipWnd == NULL ){
		LoadCommonControls(ICC_TAB_CLASSES);

		hTipWnd = CreateWindow(TOOLTIPS_CLASS, NULL,
			TTS_ALWAYSTIP | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hWnd, NULL, DLLhInst, NULL);
	}

	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = hWnd;
	ti.hinst = DLLhInst;
	ti.uId = (UINT_PTR)hWnd;
	ti.lpszText = text;
	ti.rect.left = 0;
	ti.rect.top = 0;
	ti.rect.right = 10000;
	ti.rect.bottom = 10000;
	SendMessage(hTipWnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

void SetStateOnCaption(FOPSTRUCT *FS, const TCHAR *text)
{
	TCHAR caption[VFPS * 2];

	GetWindowText(FS->hDlg, caption, TSIZEOF(caption));
	if ( text == NULL ){
		TCHAR *p;

		p = tstrrchr(caption, '-');
		if ( (p != NULL) && (p > caption) && (*(p-1) == ' ') ) *(p - 1) = '\0';
	}else{
		wsprintf(caption + tstrlen(caption), T(" - %s"), text);
	}
	SetWindowText(FS->hDlg, caption);

	FS->progs.CapTick = 0;
	FS->progs.nowper = -1;
}

void SetSymParam(DWORD *flags, const TCHAR *more, int shift)
{
	int mode;

	if ( *more == 's' ){
		mode = 1;
	}else if ( *more == 'f' ){
		mode = 2;
	}else{
		mode = 0;
	}
	*flags = (*flags & ~(3 << shift)) | (mode << shift);
}

int CheckParamOnOff(LPCTSTR *str)
{
	const TCHAR *old;
	int value;

	if ( tstricmp(*str, T("ON")) == 0 ){
		*str += 2;
		return 1;
	}
	if ( tstricmp(*str, T("OFF")) == 0 ){
		*str += 3;
		return 0;
	}
	old = *str;
	value = (int)GetNumber(str);
	if ( old != *str ) return value;
	return 1; // default値
}

DWORD CheckParamFlag(LPCTSTR *str, DWORD flags, DWORD flag, BOOL reverse)
{
	int num;

	num = CheckParamOnOff(str);
	if ( reverse ) num = !num;
	if ( num ){
		return flags | flag;
	}else{
		return flags & ~flag;
	}
}

BOOL USEFASTCALL IsParentFgWindow(HWND hTargetWnd)
{
	HWND hFgWnd = GetForegroundWindow();

	for (;;){
		if ( hFgWnd == hTargetWnd ) return TRUE;
		hTargetWnd = GetParent(hTargetWnd);
		if ( hTargetWnd == NULL ) return FALSE;
	}
}

int GetStringListCommand(const TCHAR *param, const TCHAR *commands)
{
	const TCHAR *oldparam;
	int count;

	oldparam = param;
	count = GetIntNumber(&param);
	if ( oldparam != param ) return count;

	while ( *commands ){
		if ( !tstrcmp(param, commands) ) return count;
		commands += tstrlen(commands) + 1;
		count++;
	}
	XMessage(NULL, NULL, XM_GrERRld, StrOptionError, param);
	return -1;
}

BOOL GetFopOptions(const TCHAR *param, FOPSTRUCT *FS)
{
	const TCHAR *more;
	TCHAR buf[CMDLINESIZE], buf3[VFPS];
	UTCHAR code;
	VFSFOP_OPTIONS *fop;

	fop = &FS->opt.fop;
	while( '\0' != (code = GetOptionParameter(&param, buf, CONSTCAST(TCHAR **, &more))) ){
		if ( code == '-' ){

		if ( !tstrcmp( buf + 1, T("SHEET")) ){
			fop->firstsheet = (BYTE)(GetNumber(&more) - 1);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("MODE")) ){
			fop->mode = GetStringListCommand(more, T("move\0") T("copy\0") T("mirror\0") T("shortcut\0") T("link\0") T("delete\0") T("undo\0") T("symbolic\0"));
			if ( (fop->mode < FOPMODE_MOVE) || (fop->mode >= FOPMODE_MAX) ){
				fop->mode = FOPMODE_COPY;
			}
			continue;
		}

		if ( !tstrcmp( buf + 1, T("MIN")) ){
			if ( (FS->opt.hReturnWnd != NULL) &&
				 IsParentFgWindow(FS->opt.hReturnWnd) ){
				// ※PPV_SUB と同じ方法にした方がよいかも。
				ShowWindow(FS->hDlg, SW_MINIMIZE);
				SetForegroundWindow(FS->opt.hReturnWnd);
			}else{
				ShowWindow(FS->hDlg, SW_MINIMIZE);
			}
			continue;
		}

		if ( !tstrcmp( buf + 1, T("ERROR")) ){
			if ( (*more == 'a') || (*more == '1') ){
				FS->opt.erroraction = IDABORT;
			}else if ( *more == 'r' || (*more == '2') ){
				FS->opt.erroraction = IDRETRY;
			}else if ( *more == 'i' || (*more == '3') ){
				FS->opt.erroraction = IDIGNORE;
			}else{
				FS->opt.erroraction = 0;
			}
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SYMCOPY")) ){
			SetSymParam(&fop->flags, more, VFSFOP_OPTFLAG_SYMCOPY_SHIFT);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SYMDEL")) ){
			SetSymParam(&fop->flags, more, VFSFOP_OPTFLAG_SYMDEL_SHIFT);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("RETRY")) ){
			const TCHAR *oldmore;

			oldmore = more;
			FS->opt.errorretrycount = (DWORD)GetNumber(&more);
			if ( oldmore == more ) FS->opt.errorretrycount = 3;
			continue;
		}

		// コピー対象
		if ( !tstrcmp( buf + 1, T("SRC")) ){
			if ( (FS->opt.files != NULL) && IsTrue(FS->opt.AllocFiles) ){
				// ThFree(FS->opt.files) 相当
				HeapFree(DLLheap, 0, (void *)FS->opt.files);
			}
			PP_ExtractMacro(NULL, FS->info, NULL, T("%1"), buf3, XEO_DISPONLY);
			tstrcpy(FS->opt.source, buf3);
			FS->opt.files = MakeFOPlistFromParam(more, buf3);
			FS->opt.AllocFiles = TRUE;
			continue;
		}

		if ( !tstrcmp( buf + 1, T("MASK")) ){
			FreeFN_REGEXP(&FS->maskFn);
			FS->maskFnFlags = MakeFN_REGEXP(&FS->maskFn, more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("QSTART")) ){
			setflag(fop->flags, VFSFOP_OPTFLAG_QSTART);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("NOCOUNT")) ){
			setflag(fop->flags, VFSFOP_OPTFLAG_NOCOUNT);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("CHECKEXISTFIRST")) ){
			if ( CheckParamOnOff(&more) ){
				resetflag(fop->flags, VFSFOP_OPTFLAG_NOCOUNT | VFSFOP_OPTFLAG_NOFIRSTEXIST);
			}else{
				setflag(fop->flags, VFSFOP_OPTFLAG_NOFIRSTEXIST);
			}
			continue;
		}

		// Sheet:1 一般
		if ( !tstrcmp( buf + 1, T("DEST")) ){
			TCHAR *p2;

			p2 = fop->str + tstrlen(fop->str) + 1; //Renameの位置
			tstrcpy(buf, p2); // Rename 抽出
			tstrcpy(buf3, p2 + tstrlen(p2) + 1); // S.num 抽出
			tstrcpy(fop->str, more);
			p2 = fop->str + tstrlen(fop->str) + 1; //Renameの位置
			tstrcpy(p2, buf); // Rename
			tstrcpy(p2 + tstrlen(p2) + 1, buf3);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SAME")) ){
			fop->same = GetStringListCommand(more, T("new\0") T("rename\0") T("overwrite\0") T("skip\0") T("archive\0") T("number\0") T("append\0") T("size\0"));
			if ( (fop->same < 0) || (fop->same >= FOPSAME_MAX) ){
				fop->same = FOPSAME_ADDNUMBER;
			}
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SAMEALL")) ){
			fop->sameSW = CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("AUTOSAMEALL")) ){
			fop->aall = (char)CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("BURST")) ){
			fop->useburst = (char)CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("FLAT")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_FLATMODE
, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("IFILEOP")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_IFILEOP, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SECURITY")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_SECURITY, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SACL")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_SACL, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("KEEPDIR")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_KEEPDIR, FALSE);
			continue;
		}

		// Sheet:2 名前
		if ( !tstrcmp( buf + 1, T("NAME")) ){
			TCHAR *p2;

			p2 = fop->str + tstrlen(fop->str) + 1; //Renameの位置
			tstrcpy(buf, p2 + tstrlen(p2) + 1); // S.num 抽出
			tstrcpy(p2, more);
			tstrcpy(p2 + tstrlen(p2) + 1, buf);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SNUM")) ){
			TCHAR *p;

			p = fop->str + tstrlen(fop->str) + 1; //Renameの位置
			p = p + tstrlen(p) + 1; // S.numの位置
			tstrcpy(p, more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("DIV")) ){
			GetDivideSize(more, fop);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("JOINBATCH")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_JOINBATCH, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("LOWER")) ){
			fop->chrcase = CheckParamOnOff(&more) * 2;
			continue;
		}

		if ( !tstrcmp( buf + 1, T("UPPER")) ){
			fop->chrcase = CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SFN")) ){
			fop->sfn = CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("DELETESPACE")) ){
			fop->delspc = CheckParamOnOff(&more);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("DELETENUMBER")) ){
			fop->filter = CheckParamFlag(&more, fop->filter, VFSFOP_FILTER_DELNUM, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("EXTRACTNAME")) ){
			fop->filter = CheckParamFlag(&more, fop->filter, VFSFOP_FILTER_EXTRACTNAME, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("ATTRIBUTES")) ){
			GETPARAMFLAGSSTRUCT gpfs;

			GetParamFlags(&gpfs, &more, AttrLabelString);
			fop->AtrMask = ~gpfs.mask;
			fop->AtrFlag = gpfs.value;
			continue;
		}

		if ( !tstrcmp( buf + 1, T("RENAMEEXT")) ){
			fop->filter = CheckParamFlag(&more, fop->filter, VFSFOP_FILTER_NOEXTFILTER, TRUE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("RENAMEFILE")) ){
			fop->filter = CheckParamFlag(&more, fop->filter, VFSFOP_FILTER_NOFILEFILTER, TRUE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("RENAMEDIRECTORY")) ){
			fop->filter = CheckParamFlag(&more, fop->filter, VFSFOP_FILTER_NODIRFILTER, TRUE);
			continue;
		}

		// Sheet:3 その他
		if ( !tstrcmp( buf + 1, T("QUERYCREATEDIRECTORY")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_NOWCREATEDIR, TRUE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("SKIPERROR")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_SKIPERROR, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("WAITRESULT")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_LOGRWAIT, TRUE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("BACKUP")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_BACKUPOLD, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("RENAMEDEST")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_ADDNUMDEST, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("LOG")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_LOGWINDOW, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("UNDOLOG")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_UNDOLOG, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("LOWPRI")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_LOWPRI, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("PREVENTSLEEP")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_PREVENTSLEEP, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("ALLOWDECRYPT")) ){
			fop->flags = CheckParamFlag(&more, fop->flags, VFSFOP_OPTFLAG_ALLOWDECRYPT, FALSE);
			continue;
		}

		if ( !tstrcmp( buf + 1, T("COMPCMD")) ){
			if ( *more == '\0' ){
				more = param;
				param = NilStr;
			}

			if ( FS->opt.compcmd != NULL ){
				HeapFree(DLLheap, 0, FS->opt.compcmd);
			}
			FS->opt.compcmd = HeapAlloc(DLLheap, 0, TSTRSIZE(more));
			tstrcpy(FS->opt.compcmd, more);
			continue;
		}
		}
		XMessage(NULL, NULL, XM_GrERRld, StrOptionError, buf);
		FS->DestDir[0] = '\0';
		return FALSE;
	}
	return TRUE;
}


#define modestr_max5 7
#define modestr_max6 8

int ECTRL[] = {
//*** 同名処理の時は選択可能になる項目
//** 共用
	IDB_TEST,
//** 一般シート
// 一般チェックボックス
	IDX_FOP_SCOPY,
	IDX_FOP_SACL,
	IDX_FOP_KEEPDIR,
// 同名ファイル処理
	IDX_FOP_ADDNUMDEST,
	IDX_FOP_AALL,
	IDX_FOP_SAME,
// 同名ファイル処理グループボックス
	IDR_FOP_NEWDATE,
	IDR_FOP_RENAME,
	IDR_FOP_OVER,
	IDR_FOP_SKIP,
	IDR_FOP_ARC,
	IDR_FOP_ADDNUM,
	IDR_FOP_APPEND,
	IDR_FOP_SIZE,
//** 名前変更シート
// 名前変更グループボックス
	IDX_FOP_UPPER,
	IDX_FOP_LOWER,
	IDX_FOP_SFN,
	IDX_FOP_DELSPC,
	IDX_FOP_DELNUM,
	IDX_FOP_EXTRACTNAME,
// ファイル属性グループボックス
	IDX_FOP_RONLY,
	IDX_FOP_ARC,
	IDX_FOP_HIDE,
	IDX_FOP_SYSTEM,
	IDX_FOP_AROFF,
//** その他シート
	IDX_FOP_SKIP,
	IDX_FOP_BACKUPOLD,
	1,
//*** 処理中は常に選択できない項目
//** 共用
	IDC_FOP_ACTION,
	IDB_REF,
	IDC_FOP_MODE,
//** 一般シート
	IDC_FOP_DESTDIR,
	IDX_FOP_BURST,
	IDX_FOP_FLAT,
//** 名前変更シート
	IDE_FOP_RENAME,
	IDE_FOP_DIV,
	IDE_FOP_RENUM,
	IDX_FOP_JOINBATCH,
//** その他シート
	IDX_FOP_USELOGWINDOW,
	IDX_FOP_UNDOLOG,
	IDX_FOP_COUNTSIZE,
	IDX_FOP_CHECKEXISTFIRST,
	IDX_FOP_SERIALIZE,
	0
};

int ECTRLDEL[] = {	// Delete のときに無効になるコントロール
//	IDC_FOP_DESTDIR,
	IDR_FOP_NEWDATE,
	IDR_FOP_RENAME,
	IDR_FOP_OVER,
	IDR_FOP_SKIP,
	IDR_FOP_ARC,
	IDR_FOP_ADDNUM,
	IDR_FOP_APPEND,
	IDX_FOP_SAME,
	IDX_FOP_BURST,
	IDX_FOP_FLAT,
	IDX_FOP_SCOPY,
	IDX_FOP_SACL,
	IDX_FOP_AALL,
	0
};

void Enables(FOPSTRUCT *FS, int setting, int samesetting)
{
	int *tbl;
	HWND hDlg;

	FS->EnableSetting = setting;
	FS->EnableSameSetting = samesetting;
	hDlg = FS->hDlg;
	for ( tbl = ECTRL ; *tbl ; tbl++){
		if ( *tbl == 1 ){
			setting = samesetting;
			if ( setting == 2 ) setting = FALSE;
			continue;
		}
		EnableDlgWindow(hDlg, *tbl, setting);
	}
	if ( samesetting == 2 ){
		EnableDlgWindow(hDlg, IDR_FOP_RENAME, TRUE);
		EnableDlgWindow(hDlg, IDR_FOP_ADDNUM, TRUE);
		EnableDlgWindow(hDlg, IDR_FOP_SKIP, TRUE);
		EnableDlgWindow(hDlg, IDX_FOP_SAME, TRUE);
	}
	if ( FS->opt.fop.mode == FOPMODE_DELETE ){
		for ( tbl = ECTRLDEL ; *tbl ; tbl++){
			EnableDlgWindow(hDlg, *tbl, FALSE);
		}
	}
}

void USEFASTCALL PeekMessageLoopSub(FOPSTRUCT *FS)
{
	MSG msg;

	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( IsDialogMessage(FS->hDlg, &msg) == FALSE ){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void PeekMessageLoop(FOPSTRUCT *FS)
{
	MSG msg;

	PeekMessageLoopSub(FS);
	if ( FS->state != FOP_TOPAUSE ) return;
	FS->state = FOP_PAUSE;
	while( (int)GetMessage(&msg, NULL, 0, 0) > 0 ){
		if ( IsDialogMessage(FS->hDlg, &msg) == FALSE ){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if ( (FS->state == FOP_BUSY) || (FS->state == FOP_TOBREAK) ) break;
	}
}

int ShowControlsID[] = {
	IDS_FOP_SRCTITLE, IDS_FOP_SRCINFO, IDS_FOP_ATTRLABEL,
	IDS_FOP_DESTTITLE, IDS_FOP_DESTINFO, 0
};

void SetWindowY(FOPSTRUCT *FS, int y)
{
	HWND hDlg;
	RECT wrect, crect, box;
	int newheight;

	hDlg = FS->hDlg;
	if ( FS->hEWnd != NULL ){
		int state;
		int *ID;

		state = (y == WINY_FULL) ? SW_SHOWNORMAL : SW_HIDE;
		for ( ID = ShowControlsID ; *ID ; ID++ ){
			HWND hControlWnd;

			hControlWnd = GetDlgItem(hDlg, *ID);
			if ( hControlWnd != NULL ) ShowWindow(hControlWnd, state);
		}
		ShowWindow(FS->hEWnd, (state == SW_HIDE) ? SW_SHOWNORMAL : SW_HIDE);
		y = WINY_FULL;
	}
	box.top = y;
	box.bottom = 0;	// これらの値を埋めておかないと、Win9xで Ziv0 エラーが出る
	box.left = 0;	//
	box.right = 0;	//
	MapDialogRect(hDlg, &box);
	GetWindowRect(hDlg, &wrect);
	GetClientRect(hDlg, &crect);
	newheight = (wrect.bottom - wrect.top) -(crect.bottom - crect.top) +box.top;
	SetWindowPos(hDlg, NULL, 0, 0, wrect.right - wrect.left, newheight,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

	if ( IsIconic(hDlg) ){
		WINDOWPLACEMENT wpm;

		wpm.length = sizeof(wpm);
		GetWindowPlacement(hDlg, &wpm);
		wpm.rcNormalPosition.bottom = wpm.rcNormalPosition.top + newheight;
		SetWindowPlacement(hDlg, &wpm);
	}
}

TCHAR *DispAttrSub(TCHAR *str, FILETIME *srcftime, FILETIME *dstftime)
{
	FILETIME	lTime;
	SYSTEMTIME	sTime;

	*str++ = (TCHAR)((FuzzyCompareFileTime(srcftime, dstftime) > 0) ? PXSC_HILIGHT : PXSC_NORMAL);

	FileTimeToLocalFileTime(srcftime, &lTime);
	FileTimeToSystemTime(&lTime, &sTime);
	str += wsprintf(str, T("%04d-%02d-%02d%3d:%02d:%02d.%03d\n"),
				sTime.wYear % 10000, sTime.wMonth, sTime.wDay,
				sTime.wHour, sTime.wMinute, sTime.wSecond, sTime.wMilliseconds);
	return str;
}

void DispAttr(HWND hDlg, UINT cID, BY_HANDLE_FILE_INFORMATION *src, BY_HANDLE_FILE_INFORMATION *dst)
{
	TCHAR buf[0x1000], *str;

	str = buf;
	*str++ = PXSC_RIGHT;
	if ( src->dwFileAttributes != dst->dwFileAttributes ) *str++ = PXSC_HILIGHT;
	tstrcpy(str, T("________\n"));
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_READONLY )	str[0] = 'R';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )	str[1] = 'H';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM )	str[2] = 'S';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) str[3] = 'P';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )	str[4] = 'D';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )		str[5] = 'A';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY )	str[6] = 'T';
	if ( src->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ){	str[7] = 'C';
	}else if (src->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ){ str[7] = 'e';
	}
	str += 9;

	if ( src->nFileSizeHigh > dst->nFileSizeHigh ){
		*str++ = PXSC_HILIGHT;
	}else{
		*str++ = (TCHAR)((src->nFileSizeLow > dst->nFileSizeLow) ? PXSC_HILIGHT : PXSC_NORMAL);
	}
	FormatNumber(str, XFN_SEPARATOR, 22, src->nFileSizeLow, src->nFileSizeHigh);
	str += tstrlen(str);
	*str++ = '\n';

	str = DispAttrSub(str, &src->ftCreationTime, &dst->ftCreationTime);
	str = DispAttrSub(str, &src->ftLastWriteTime, &dst->ftLastWriteTime);
	DispAttrSub(str, &src->ftLastAccessTime, &dst->ftLastAccessTime);
	SetDlgItemText(hDlg, cID, buf);
}

void TopToDlgItem(HWND hWnd, int iCtrl, TCHAR *str)
{
	LONG index;

	index = (LONG)(LRESULT)SendDlgItemMessage(hWnd, iCtrl, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)str);
	if ( index != CB_ERR ){
		SendDlgItemMessage(hWnd, iCtrl, CB_DELETESTRING, (WPARAM)index, 0);
	}
	SendDlgItemMessage(hWnd, iCtrl, CB_INSERTSTRING, 0, (LPARAM)str);
	SendDlgItemMessage(hWnd, iCtrl, CB_SETCURSEL, 0, 0);
}

BOOL LoadOption(FOPSTRUCT *FS, const TCHAR *action, const TCHAR *option)
{
	TCHAR *p, buf[VFPS];
	HWND hDlg;
	VFSFOP_OPTIONS *FOP;
	BOOL result = TRUE;

	hDlg = FS->hDlg;
									// Option の初期設定 ----------------------
	FOP = &FS->opt.fop;
	FOP->sameSW		= 0;
	FOP->same		= 0;

	FOP->chrcase	= 0;
	FOP->sfn		= 0;
	FOP->delspc		= 0;
	FOP->filter		= 0;

	FOP->AtrMask	= FILEATTRMASK_DIR_FILES;		// And 指定用
	FOP->AtrFlag	= 0x20;		// Or 指定用

	FOP->mode		= FOPMODE_COPY;
	FOP->flags		= VFSFOP_OPTFLAG_SKIPERROR;
	FOP->aall		= 1;
	FOP->firstsheet	= 0;
	FOP->useburst	= 0;
	FOP->divide_unit = '\0';
	FOP->divide_num	= 0;
	memcpy(FOP->str, optstr, sizeof optstr);

	FS->Cancel		= FALSE;
	FS->Command		= 0;
									// Option の読み込み ----------------------
	if ( tstrlen(action) >= MAXACTIONNAME ){
		XMessage(NULL, STR_FOP, XM_FaERRd, T("Action name error"));
		tstrcpy(FS->opt.action, T("Copy"));
		result = FALSE;
	}else{
		tstrcpy(FS->opt.action, action);
	}

	if ( tstricmp(FS->opt.action, T("rename")) == 0 ){
		FOP->firstsheet = FOPTAB_RENAME;
	}
	if ( NO_ERROR != GetCustTable(T("X_fopt"), FS->opt.action, FOP, sizeof(VFSFOP_OPTIONS)) ){
		XMessage(NULL, STR_FOP, XM_NsERRd, T("%s setting not found."), FS->opt.action);
		result = FALSE;
		if ( tstricmp(FS->opt.action, T("copy")) == 0 ){
			// FOPMODE_COPY 設定済み
		}else if ( tstricmp(FS->opt.action, T("move")) == 0 ){
			FOP->mode = FOPMODE_MOVE;
		}else if ( tstricmp(FS->opt.action, T("rename")) == 0 ){
			FOP->mode = FOPMODE_MOVE;
			memcpy(FOP->str, optstr + sizeof(TCHAR) * 4,
					sizeof(optstr) - sizeof(TCHAR) * 4);
		}else{
			tstrcpy(FS->opt.action, NilStr);
		}
		if ( FS->opt.action[0] != '\0' ){
			SetCustTable(T("X_fopt"), FS->opt.action, FOP, sizeof(VFSFOP_OPTIONS));
			XMessage(NULL, STR_FOP, XM_NsERRd, T("Saved default setting."));
		}
	}

	if ( !(FOP->flags & VFSFOP_OPTFLAG_LOGWINDOW) ){
		if ( (FS->info != NULL) &&
			 (FS->info->Function(FS->info, PPXCMDID_EXTREPORTTEXT, NULL) ==
			   PPXCMDID_EXTREPORTTEXT_LOG) ){
			setflag(FOP->flags, VFSFOP_OPTFLAG_LOGWINDOW);
		}
	}

	if ( option != NULL ){
		if ( GetFopOptions(option, FS) == FALSE ) result = FALSE;
	}
									// 各コントロールの設定 ===================
	{
		const TCHAR *title;

		title = MessageText(StrFopTitle);
		if ( *title ) SetWindowText(hDlg, title);
	}

	wsprintf(buf, T("%s %s"), MessageText(MES_FBAC), FS->opt.action);
	SetDlgItemText(hDlg, IDC_FOP_ACTION, buf);
	wsprintf(buf, T("%s(&M)"), MessageText(JobTypeNames[FS->opt.fop.mode + JOBSTATE_FOP_MOVE]));
	SetDlgItemText(hDlg, IDC_FOP_MODE, buf);
										// Dest -----------------------------
	p = FOP->str;
	if ( FS->FixDest == FALSE ){
		if ( *p == '\0' ){
			FS->DestDir[0] = '\0';
		}else{
			PP_ExtractMacro(hDlg, FS->info, NULL, p, FS->DestDir, XEO_DISPONLY);
		}
	}
										// Rename -----------------------------
	p += tstrlen(p) + 1;
	if ( (FOP->filter & VFSFOP_FILTER_EXTRACTNAME) ||
		 (*p == RENAME_EXTRACTNAME) ){ // % 展開あり
		tstrcpy(FS->opt.rename, p);
	}else{
		PP_ExtractMacro(hDlg, FS->info, NULL, p, FS->opt.rename, XEO_DISPONLY);
	}
										// S.num -----------------------------
	tstrcpy(FS->opt.renum, p + tstrlen(p) + 1);
	if ( FS->opt.renum[0] == '\0' ) tstrcpy(FS->opt.renum, T("001"));

	if ( FOP->firstsheet > 2 ) FOP->firstsheet = 0;
	InitControlData(FS, FOP->firstsheet);
	return result;
}

void SaveParam(FOPSTRUCT *FS, const TCHAR *actionname)
{
	TCHAR *p;

	SaveControlData(FS);
	if ( PMessageBox(FS->hDlg, MES_QSDP, STR_FOP,
			MB_YESNO | MB_DEFBUTTON1) == IDYES){
		tstrcpy(FS->opt.fop.str, T("%2%\\"));
	}else{
		tstrcpy(FS->opt.fop.str, FS->DestDir);
	}
	p = FS->opt.fop.str + tstrlen(FS->opt.fop.str) + 1;
	tstrcpy(p, FS->opt.rename);
	p += tstrlen(p) + 1;
	tstrcpy(p, FS->opt.renum);
	p += tstrlen(p) + 1;

	SetCustTable(T("X_fopt"), actionname, &FS->opt.fop, (BYTE *)p - (BYTE *)&FS->opt.fop);
	LoadOption(FS, actionname, NULL);
}

void USEFASTCALL CheckMask(HWND hDlg, int id, DWORD attr, VFSFOP_OPTIONS *FOP)
{
	int check;

	check = (attr & FOP->AtrMask ? 2 : 0) | (attr & FOP->AtrFlag ? 1 : 0);
	CheckDlgButton(hDlg, id, check);
}

void SetDestDirPath(HWND hED, const TCHAR *dest)
{
	if ( SetWindowTextWithSelect(hED, dest) == FALSE ){
		SendMessage(hED, EM_SETSEL, EC_LAST, EC_LAST);
	}
}

void LoadControlData(FOPSTRUCT *FS)
{
	HWND hDlg;
	TCHAR buf[VFPS];
	VFSFOP_OPTIONS *FOP;

	hDlg = FS->hDlg;
	FOP = &FS->opt.fop;
	switch( FS->page.showID ){
		case FOPTAB_GENERAL:	// 一般シート
			// 処理先一行編集
			SetDestDirPath(FS->hDstED, FS->DestDir);
			// 同名ファイル処理グループボックス
			CheckRadioButton(hDlg, IDR_FOP_NEWDATE, IDR_FOP_APPEND,
					IDR_FOP_NEWDATE + FOP->same);
			CheckDlgButton(hDlg, IDR_FOP_SIZE, FOP->same == FOPSAME_SIZE);
			CheckDlgButton(hDlg, IDX_FOP_SAME, FOP->sameSW);
			// 各種チェックボックス
			CheckDlgButton(hDlg, IDX_FOP_BURST, FOP->useburst);
			CheckDlgButton(hDlg, IDX_FOP_FLAT, FOP->flags & VFSFOP_OPTFLAG_FLATMODE);
			CheckDlgButton(hDlg, IDX_FOP_AALL, FOP->aall);
			CheckDlgButton(hDlg, IDX_FOP_SCOPY, FOP->flags & VFSFOP_OPTFLAG_SECURITY);
			FS->opt.security = (FOP->flags & VFSFOP_OPTFLAG_SECURITY) ?
				SECURITY_FLAG_SCOPY : SECURITY_FLAG_NONE;
			CheckDlgButton(hDlg, IDX_FOP_SACL, FOP->flags & VFSFOP_OPTFLAG_SACL);
			if ( FOP->flags & VFSFOP_OPTFLAG_SACL ){
				FS->opt.security = SECURITY_FLAG_SACL;
			}
			CheckDlgButton(hDlg, IDX_FOP_KEEPDIR, FOP->flags & VFSFOP_OPTFLAG_KEEPDIR);
			CheckDlgButton(hDlg, IDX_FOP_ADDNUMDEST, FOP->flags & VFSFOP_OPTFLAG_ADDNUMDEST);
			break;

		case FOPTAB_RENAME:	// 名前シート
			// 名前変更グループボックス
			CheckDlgButton(hDlg, IDX_FOP_UPPER, FOP->chrcase == 1);
			CheckDlgButton(hDlg, IDX_FOP_LOWER, FOP->chrcase == 2);
			CheckDlgButton(hDlg, IDX_FOP_SFN, FOP->sfn);
			CheckDlgButton(hDlg, IDX_FOP_DELSPC, FOP->delspc);
			CheckDlgButton(hDlg, IDX_FOP_DELNUM, FOP->filter & VFSFOP_FILTER_DELNUM);
			CheckDlgButton(hDlg, IDX_FOP_EXTRACTNAME, FOP->filter & VFSFOP_FILTER_EXTRACTNAME);
			// ファイル属性グループボックス
			CheckMask(hDlg, IDX_FOP_RONLY, FILE_ATTRIBUTE_READONLY, FOP);
			CheckMask(hDlg, IDX_FOP_ARC, FILE_ATTRIBUTE_ARCHIVE, FOP);
			CheckMask(hDlg, IDX_FOP_HIDE, FILE_ATTRIBUTE_HIDDEN, FOP);
			CheckMask(hDlg, IDX_FOP_SYSTEM, FILE_ATTRIBUTE_SYSTEM, FOP);
			CheckDlgButton(hDlg, IDX_FOP_AROFF, FOP->flags & VFSFOP_OPTFLAG_AUTOROFF);
			// 一行編集-分割サイズ
			wsprintf(buf, T("%u%c"), FOP->divide_num, FOP->divide_unit);
			SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_RESETCONTENT, 0, 0);

			UsePPx();
			{
				int index = 0;
				const TCHAR *histptr;

				for ( ; ; ){
					histptr = EnumHistory(PPXH_NUMBER, index++);
					if ( histptr == NULL ) break;
					#ifdef UNICODE
						if ( ALIGNMENT_BITS(histptr) & 1 ){
							SendUTextMessage_U(GetDlgItem(hDlg, IDE_FOP_DIV), CB_ADDSTRING, 0, histptr);
						}else
					#endif
						SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_ADDSTRING, 0, (LPARAM)histptr);
					if ( index >= 50 ) break;
				}
			}
			FreePPx();
			SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_ADDSTRING, 0, (LPARAM)T("1457664"));
			SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_ADDSTRING, 0, (LPARAM)T("10M"));
			SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_ADDSTRING, 0, (LPARAM)T("650M"));
			SendDlgItemMessage(hDlg, IDE_FOP_DIV, CB_ADDSTRING, 0, (LPARAM)T("1G"));
			TopToDlgItem(hDlg, IDE_FOP_DIV, buf);
			CheckDlgButton(hDlg, IDX_FOP_JOINBATCH, FOP->flags & VFSFOP_OPTFLAG_JOINBATCH);
			// その他一行編集
			SetDlgItemText(hDlg, IDE_FOP_RENAME, FS->opt.rename);
			SetDlgItemText(hDlg, IDE_FOP_RENUM, FS->opt.renum);
			// フィルタ対象
			CheckDlgButton(hDlg, IDX_FOP_FILEFIX,
						!(FOP->filter & VFSFOP_FILTER_NOFILEFILTER));
			CheckDlgButton(hDlg, IDX_FOP_EXTFIX,
						!(FOP->filter & VFSFOP_FILTER_NOEXTFILTER));
			CheckDlgButton(hDlg, IDX_FOP_DIRFIX,
						!(FOP->filter & VFSFOP_FILTER_NODIRFILTER));
			// 起動ページ
			CheckDlgButton(hDlg, IDX_FOP_1STSHEET, (FOP->firstsheet == 1));
			break;

		case FOPTAB_OPTION:	// その他シート
			CheckDlgButton(hDlg, IDX_FOP_NOWCREATEDIR, FOP->flags & VFSFOP_OPTFLAG_NOWCREATEDIR);
			CheckDlgButton(hDlg, IDX_FOP_SKIP, FOP->flags & VFSFOP_OPTFLAG_SKIPERROR);
			CheckDlgButton(hDlg, IDX_FOP_LOGRWAIT, FOP->flags & VFSFOP_OPTFLAG_LOGRWAIT);
			CheckDlgButton(hDlg, IDX_FOP_BACKUPOLD, FOP->flags & VFSFOP_OPTFLAG_BACKUPOLD);
			CheckDlgButton(hDlg, IDX_FOP_USELOGWINDOW, FOP->flags & VFSFOP_OPTFLAG_LOGWINDOW);
			CheckDlgButton(hDlg, IDX_FOP_UNDOLOG, FOP->flags & VFSFOP_OPTFLAG_UNDOLOG);
			CheckDlgButton(hDlg, IDX_FOP_LOWPRI, FOP->flags & VFSFOP_OPTFLAG_LOWPRI);
			CheckDlgButton(hDlg, IDX_FOP_PREVENTSLEEP, FOP->flags & VFSFOP_OPTFLAG_PREVENTSLEEP);
			CheckDlgButton(hDlg, IDX_FOP_ALLOWDECRYPT, FOP->flags & VFSFOP_OPTFLAG_ALLOWDECRYPT);

			CheckDlgButton(hDlg, IDX_FOP_COUNTSIZE, !(FOP->flags & VFSFOP_OPTFLAG_NOCOUNT));
			EnableDlgWindow(hDlg, IDX_FOP_CHECKEXISTFIRST, !(FS->opt.fop.flags & VFSFOP_OPTFLAG_NOCOUNT));
			CheckDlgButton(hDlg, IDX_FOP_CHECKEXISTFIRST, !(FOP->flags & VFSFOP_OPTFLAG_NOFIRSTEXIST));
			CheckDlgButton(hDlg, IDX_FOP_SERIALIZE, !(FOP->flags & VFSFOP_OPTFLAG_QSTART));
			CheckDlgButton(hDlg, IDX_FOP_WAITCLOSE, !(FOP->flags & VFSFOP_OPTFLAG_WAIT_CLOSE));
			break;
	}
	Enables(FS, FS->EnableSetting, FS->EnableSameSetting);
}

void SaveControlData(FOPSTRUCT *FS)
{
	HWND hDlg;
	TCHAR buf[VFPS];

	hDlg = FS->hDlg;
	switch( FS->page.showID ){
		case FOPTAB_GENERAL: {	// 一般シート
			// 処理先
			GetWindowText(FS->hDstED, FS->DestDir, VFPS);
			if ( FS->DestDir[0] == '.' ){ // 相対指定？
				if ( !(FS->opt.fopflags & VFSFOP_AUTOSTART) ){
					const TCHAR *p;

					p = FS->DestDir + 1;
					for ( ;; ){
						TCHAR c;

						c = *p;
						// 相対指定ならヒストリに記憶
						if ( (c == '\0') || (c == '\\') ){
							WriteHistory(PPXH_DIR, FS->DestDir, 0, NULL);
							break;
						}
						if ( c != '.' ) break;
						p++;
					}
				}
			}
			if ( FS->DestDir[0] == ' ' ){ // 先頭に空白がある場合、実在か確認
				VFSFixPath(buf, FS->DestDir, FS->opt.source, VFSFIX_VFPS | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
				if ( GetFileAttributes(buf) == BADATTR ){
					VFSFixPath(NULL, FS->DestDir, FS->opt.source, 0);
				}
			}else{
				VFSFixPath(NULL, FS->DestDir, FS->opt.source, 0);
			}
			if ( !(FS->opt.fopflags & VFSFOP_USEKEEPDIR) ){
				resetflag(FS->opt.fop.flags, VFSFOP_OPTFLAG_KEEPDIR);
			}
			break;
		}

		case FOPTAB_RENAME:	// 名前シート
			// 分割
			GetDlgItemText(hDlg, IDE_FOP_DIV, buf, TSIZEOF(buf));
			GetDivideSize(buf, &FS->opt.fop);
			if ( FS->opt.fop.divide_num && !(FS->opt.fopflags & VFSFOP_AUTOSTART)){
				WriteHistory(PPXH_NUMBER, buf, 0, NULL);
			}
			// 名前
			GetDlgItemText(hDlg, IDE_FOP_RENAME, buf, TSIZEOF(FS->opt.rename));
			if ( tstrcmp(FS->opt.rename, buf) != 0 ){
				TCHAR *rendst = FS->opt.rename;

				if ( (FS->opt.fop.filter & VFSFOP_FILTER_EXTRACTNAME) &&
					 (buf[0] != RENAME_EXTRACTNAME) &&
					 (tstrchr(buf, '%') != NULL) ){
					*rendst++ = RENAME_EXTRACTNAME;
				}
				VFSFixPath(rendst, buf, NULL, 0);
				if ( !(FS->opt.fopflags & VFSFOP_AUTOSTART) ){
					WriteHistory(PPXH_FILENAME, FS->opt.rename, 0, NULL);
				}
			}
			// 連番
			GetDlgItemText(hDlg, IDE_FOP_RENUM, buf, TSIZEOF(FS->opt.renum));
			if ( tstrcmp(FS->opt.renum, buf) ){
				tstrcpy(FS->opt.renum, buf);
				if ( !(FS->opt.fopflags & VFSFOP_AUTOSTART) ){
					WriteHistory(PPXH_NUMBER, buf, 0, NULL);
				}
			}
			break;

		case FOPTAB_OPTION:	// その他シート
			break;
	}
}

void SetTabActive(HWND hDlg, UINT id, BOOL active)
{
	DWORD_PTR style;
	HWND hCtrlWnd;

	hCtrlWnd = GetDlgItem(hDlg, id);
	if ( hCtrlWnd == NULL ) return;

	style = GetWindowLongPtr(hCtrlWnd, GWL_STYLE) & ~BS_FLAT;
	if ( active ) style |= BS_FLAT;
	SetWindowLongPtr(hCtrlWnd, GWL_STYLE, style);
	EnableWindow(hCtrlWnd,!active);
}

void InitControlData(FOPSTRUCT *FS, int id)
{
	HWND hFocusWnd = NULL;

	if ( id > 2 ) id = 0;
	if ( (FS->page.showID >= FOPTAB_GENERAL) && (id != FS->page.showID) ){ //違うページなら削除
		HWND *hWnds;

		for ( hWnds = FS->page.hWnds ; *hWnds != NULL ; hWnds++ ){
			DestroyWindow(*hWnds);
		}
		HeapFree(DLLheap, 0, FS->page.hWnds);
		FS->page.hWnds = NULL;
	}
	SetTabActive(FS->hDlg, IDB_FOP_GENERAL, id == FOPTAB_GENERAL);
	SetTabActive(FS->hDlg, IDB_FOP_RENAME, id == FOPTAB_RENAME);
	SetTabActive(FS->hDlg, IDB_FOP_OPTION, id == FOPTAB_OPTION);

	if ( FS->page.hWnds == NULL ){				// シートがないなら新規作成
		FS->page.hWnds = CreateDialogWindow(DLLhInst,
				MAKEINTRESOURCE(DialogID[id]), FS->hDlg);
		switch ( id ){
			case FOPTAB_GENERAL:	// 一般シート
													// 処理先の設定
				hFocusWnd = FS->hDstED = PPxRegistExEdit(FS->info,
						GetDlgItem(FS->hDlg, IDC_FOP_DESTDIR), VFPS, NilStr,
						PPXH_DIR_R, PPXH_DIR, // PPXEDIT_WANTEVENT |
						PPXEDIT_REFTREE | PPXEDIT_SINGLEREF | PPXEDIT_ENABLE_WIDTH_CHANGE);
				#ifndef UNICODE
					if ( WinType == WINTYPE_9x ){
						EnableDlgWindow(FS->hDlg, IDX_FOP_SCOPY, FALSE);
						EnableDlgWindow(FS->hDlg, IDX_FOP_SACL, FALSE);
					}
				#endif
				if ( !(FS->opt.fopflags & VFSFOP_USEKEEPDIR) ){
					ShowDlgWindow(FS->hDlg, IDX_FOP_KEEPDIR, SW_HIDE);
				}
				break;

			case FOPTAB_RENAME:	// 名前シート
													// 変更名
				hFocusWnd = PPxRegistExEdit(FS->info,
						GetDlgItem(FS->hDlg, IDE_FOP_RENAME),
						TSIZEOFSTR(FS->opt.rename), NilStr,
						PPXH_NAME_R, PPXH_FILENAME, PPXEDIT_ENABLE_WIDTH_CHANGE);
													// 番号
				PPxRegistExEdit(FS->info,
						GetDlgItem(FS->hDlg, IDE_FOP_RENUM),
						TSIZEOFSTR(FS->opt.renum), NilStr,
						PPXH_NUMBER, PPXH_NUMBER, 0);
				break;

			case FOPTAB_OPTION:	// その他シート
				hFocusWnd = GetDlgItem(FS->hDlg, IDOK);
				break;
		}
	}else{
		switch ( id ){
			case FOPTAB_GENERAL:
				hFocusWnd = FS->hDstED;
				break;
			case FOPTAB_RENAME:
				hFocusWnd = GetDlgItem(FS->hDlg, IDE_FOP_RENAME);
				break;
			case FOPTAB_OPTION:
				hFocusWnd = GetDlgItem(FS->hDlg, IDOK);
				break;
		}
	}
	FS->page.showID = id;
	LoadControlData(FS);
	if ( hFocusWnd != NULL ){
		SetFocus(IsWindowEnabled(hFocusWnd) ? hFocusWnd : GetDlgItem(FS->hDlg, IDOK));
	}
}

void SetFopTab(FOPSTRUCT *FS, int id)
{
	if ( id == FS->page.showID ) return;
	SendMessage(FS->hDlg, WM_SETREDRAW, FALSE, 0);
	if ( FS->page.showID >= FOPTAB_GENERAL ) SaveControlData(FS);
	InitControlData(FS, id);
	SendMessage(FS->hDlg, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(FS->hDlg, NULL, TRUE);
}

BOOL IsShowButtonMenu(void)
{
	if ( (GetTickCount() - ButtonMenuTick) >= SKIPBUTTONMENUTIME ) return TRUE;
	ButtonMenuTick = 0;
	return FALSE;
}

void EndButtonMenu(void)
{
	ButtonMenuTick = (GetAsyncKeyState(VK_LBUTTON) & KEYSTATE_PUSH) ? GetTickCount() : 0;
}

void SelectAction(FOPSTRUCT *FS, HWND hCtrlWnd)
{
	RECT box;
	HMENU hMenu;
	int i;
	TCHAR buf[VFPS];

	if ( IsShowButtonMenu() == FALSE ) return;
	GetWindowRect(hCtrlWnd, &box);
	hMenu = CreatePopupMenu();
	for ( i = 0 ; ; i++ ){
		if ( EnumCustTable(i, T("X_fopt"), buf, NULL, 0) < 0 ) break;
		AppendMenuCheckString(hMenu, i + 1, buf,!tstricmp(buf, FS->opt.action));
	}
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuString(hMenu, 0xf000, MES_MSAV);
	AppendMenu(hMenu, MF_GS, 0, MessageText(MES_MDLC));

	i = TrackPopupMenu(hMenu, TPM_TDEFAULT, box.left, box.bottom, 0, FS->hDlg, NULL);
	if ( i ) GetMenuString(hMenu, i, buf, TSIZEOF(buf), MF_BYCOMMAND);
	EndButtonMenu();
	DestroyMenu(hMenu);
	if ( i ){
		if ( i == 0xf000 ){
			tstrcpy(buf, FS->opt.action);
			if ( tInput(FS->hDlg, T("Save action name"),
					buf, MAXACTIONNAME, PPXH_GENERAL, PPXH_GENERAL) <= 0 ){
				return;
			}
			SaveParam(FS, buf);
		}else{
			if ( GetShiftKey() & K_s ){
				if ( PMessageBox(FS->hDlg, T("Delete action?"),
						STR_FOP, MB_YESNO | MB_DEFBUTTON2) == IDYES){
					DeleteCustTable(T("X_fopt"), buf, 0);
				}
			}else{
				LoadOption(FS, buf, NULL);
			}
		}
	}
}

void SelectMode(FOPSTRUCT *FS, HWND hCtrlWnd)
{
	RECT box;
	HMENU hMenu;
	int i, maxmode;
	TCHAR buf[VFPS];
	const TCHAR **pp;

	if ( IsShowButtonMenu() == FALSE ) return;
	GetWindowRect(hCtrlWnd, &box);
	hMenu = CreatePopupMenu();
	maxmode = (WinType >= WINTYPE_VISTA) ? modestr_max6 : modestr_max5;
	for ( pp = JobTypeNames + JOBSTATE_FOP_MOVE, i = 0 ; i < maxmode ; pp++, i++ ){
		AppendMenuCheckString(hMenu, i + 1, *pp, i == FS->opt.fop.mode);
	}
	i = TrackPopupMenu(hMenu, TPM_TDEFAULT, box.left, box.bottom, 0, FS->hDlg, NULL);
	EndButtonMenu();
	DestroyMenu(hMenu);
	if ( (i >= (FOPMODE_MOVE + 1)) && (i <= (FOPMODE_SYMLINK + 1)) ){
		FS->opt.fop.mode = i - 1;

		wsprintf(buf, T("%s(&M)"), MessageText(JobTypeNames[i + JOBSTATE_FOP_MOVE - 1]));
		SetWindowText(hCtrlWnd, buf);
		Enables(FS, FS->EnableSetting, FS->EnableSameSetting);
	}
}

void SetMask(LPARAM lParam, VFSFOP_OPTIONS *FOP, DWORD atr)
{
	int i;

	i = (int)(LRESULT)SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
	FOP->AtrMask = (FOP->AtrMask & ~atr) | ( (i == 2) ? atr : 0);
	FOP->AtrFlag = (FOP->AtrFlag & ~atr) | ( (i == 1) ? atr : 0);
}

int USEFASTCALL IsControlChecked(LPARAM lParam)
{
	return (int)(LRESULT)SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
}

void SetFlagButton(LPARAM lParam, DWORD *flags, DWORD flag)
{
	if ( SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) ){
		setflag(*flags, flag);
	}else{
		resetflag(*flags, flag);
	}
}

void SetNegFlagButton(LPARAM lParam, DWORD *flags, DWORD flag)
{
	if ( SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) ){
		resetflag(*flags, flag);
	}else{
		setflag(*flags, flag);
	}
}

void FopDialogDestroy(HWND hDlg)
{
	FOPSTRUCT *FS;
	MSG msg;

	FS = (FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
	if ( IsTrue(FS->DestroyWait) ){
		// メディアプレイヤーへ処理を行うとき、メッセージポンプをしばらく動かす
		while ( IsTrue(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) ){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			Sleep(80);
		}
	}
	// ●1.2x 強制終了対策が必要…
	DeleteJobTask();
}

void FopDialogInit(HWND hDlg, FILEOPERATIONDLGBOXINITPARAMS *fopip)
{
	FOPSTRUCT *FS;
	VFSFILEOPERATION *FOPop;

	FS = fopip->FS;
	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)FS);
	FOPop = fopip->fileop;
	FS->hLogWnd = &FOPop->hLogWnd;
	FS->hDlg = hDlg;
	FS->hEWnd = NULL;
	tstrcpy(FS->opt.source, (FOPop->src != NULL) ? FOPop->src : NilStr);
	FS->opt.files = FOPop->files;
	FS->opt.AllocFiles = FALSE;
	FS->state = FOP_READY;
	FS->testmode = FALSE;
	FS->FixDest = FALSE;
	FS->NoAutoClose = FALSE;
	FS->hide = FALSE;
	FS->DestroyWait = FALSE;

	FS->progs.hWnd = hDlg;
	FS->progs.hSrcNameWnd  = GetDlgItem(hDlg, IDS_FOP_SRCNAME);
	FS->progs.hProgressWnd = GetDlgItem(hDlg, IDS_FOP_PROGRESS);
	FS->progs.srcpath = NULL;
	FS->progs.FS = FS;
	FS->progs.info.busybar = 1;

	FS->opt.security = 0;
	FS->opt.SrcDtype = FOPop->dtype;
	FS->opt.fopflags = FOPop->flags;
	FS->opt.hReturnWnd = FOPop->hReturnWnd;
	FS->opt.CopyBuf = NULL;
	FS->opt.X_cbsz.CopyBufSize = 2 * 1024; // 2Mbytes
	FS->opt.X_cbsz.EnablePPxBurstSize = 0; // off
	FS->opt.X_cbsz.DisableApiCacheSize = 64 * 1024; // 64Mbytes
	FS->opt.erroraction = 0;
	FS->opt.errorretrycount = 1;
	FS->opt.rexps = NULL;
	FS->opt.compcmd = NULL;

	FS->info = FOPop->info;
	FS->page.showID = FOPTAB_NONE;
	FS->page.hWnds = NULL;

	FS->EnableSetting = TRUE;
	FS->EnableSameSetting = TRUE;
	FS->maskFnFlags = MakeFN_REGEXP(&FS->maskFn, NilStr);

	MoveCenterWindow(hDlg, FOPop->hReturnWnd);
	LocalizeDialogText(hDlg, IDD_FOP);
	InitSysColors();
	{
		int X_fatim = 0;

		GetCustData(T("X_fatim"), &X_fatim, sizeof(X_fatim));
		FuzzyCompareFileTime = X_fatim ?
				FuzzyCompareFileTime2 : FuzzyCompareFileTime0;
	}
	ShowDlgWindow(hDlg, IDB_FOP_LOG, SW_HIDE);
	SetWindowY(FS, WINY_SETT);	// Window を小型にする
								// 各コントロールの設定 ===============
	SetDlgItemText(hDlg, IDOK, MessageText(STR_FOPOK));
	SetDlgItemText(hDlg, IDCANCEL, MessageText(STR_FOPCANCEL));
	if ( LoadOption(FS, FOPop->action, FOPop->option) == FALSE ){
		resetflag(FOPop->flags, VFSFOP_AUTOSTART);
	}
	if ( FS->opt.files == NULL ){
		FS->opt.files = MakeFOPlistFromPPx(FS->info);
		FS->opt.AllocFiles = TRUE;
		if ( FS->opt.files == NULL ){
			XMessage(NULL, STR_FOP, XM_FaERRd, T("Source not found"));
			EndDialog(hDlg, 0);
			return;
		}
	}
	if ( FS->opt.source[0] == '\0' ){
		tstrcpy(FS->opt.source, FS->opt.files);
		if ( !(GetFileAttributesL(FS->opt.source) & FILE_ATTRIBUTE_DIRECTORY) ){
			*VFSFindLastEntry(FS->opt.source) = '\0';
		}
	}
	SetWindowText(FS->progs.hSrcNameWnd, FS->opt.files);

	if ( (FOPop->dest != NULL) && FOPop->dest[0] ){
		if ( SearchPipe(FOPop->dest) != NULL ){
			tstrcpy(FS->DestDir, FOPop->dest);
		}else{
			CatPath(FS->DestDir, (TCHAR *)FOPop->dest, NilStr);
		}
		SetDestDirPath(FS->hDstED, FS->DestDir);
	}
	if ( FOPop->flags & VFSFOP_SPECIALDEST ) FS->FixDest = TRUE;

	GetCustData(T("X_cbsz"), &FS->opt.X_cbsz, sizeof(FS->opt.X_cbsz));
	FS->opt.X_cbsz.CopyBufSize = ((FS->opt.X_cbsz.CopyBufSize * 1024) + 0xffff) & 0xffff0000;

	InvalidateRect(hDlg, NULL, TRUE);

	if ( FOPop->flags & VFSFOP_NOTIFYREADY ){
		FOPop->hDialogWnd = hDlg;
		SetEvent(FOPop->hReadyEvent);
		resetflag(FOPop->flags, VFSFOP_NOTIFYREADY);
		EnableWindow(FOPop->hReturnWnd, TRUE);
		if ( !IsIconic(hDlg) && (hDlg != GetForegroundWindow()) ){
			SetForegroundWindow(hDlg);
		}
		Sleep(0);
	}

	if ( (FS->opt.fop.firstsheet == FOPTAB_GENERAL) ||
		 (FS->opt.fop.firstsheet == FOPTAB_RENAME) ){
		if ( IsExistCustTable(T("K_lied"), T("FIRSTEVENT")) || IsExistCustTable(T("K_edit"), T("FIRSTEVENT")) ){
			SendMessage(GetDlgItem(hDlg,
				(FS->opt.fop.firstsheet == FOPTAB_GENERAL) ?
				IDC_FOP_DESTDIR : IDE_FOP_RENAME), WM_PPXCOMMAND, K_E_FIRST, 0);
		}
	}

	if ( FOPop->flags & VFSFOP_AUTOSTART ){
		PostMessage(GetDlgItem(hDlg, IDOK), BM_CLICK, 0, 0);
		return;
	}

	{
		DWORD X_rtree;

		X_rtree = GetCustDword(T("X_rtree"), 0);
		if ( X_rtree == 1 ) X_rtree = !GetWindowTextLength(FS->hDstED);
		if ( X_rtree != 0 ){
			PostMessage(FS->hDstED, WM_PPXCOMMAND, K_raw | K_s | K_c | 'I', 0);
		}
	}
	if ( GetCustDword(T("X_rclst"), 0) > 0 ){
		PostMessage(FS->hDstED, WM_PPXCOMMAND, K_raw | K_s | K_c | '3', 0);
	}
}

void UnHideWindow(HWND hWnd)
{
	if ( !(GetWindowLongPtr(hWnd, GWL_STYLE) & WS_VISIBLE) ){
		ShowWindow(hWnd, SW_RESTORE);
	}
}

void TestButton(FOPSTRUCT *FS)
{
	TCHAR buf[0x200];

	if ( FS->state != FOP_READY ){
		FS->Command = IDB_TEST;
		return;
	}

	FS->testmode = TRUE;
	OperationStart(FS);
	EndingOperation(FS);
	SetDlgItemText(FS->hDlg, IDOK, MessageText(STR_FOPOK));
	Enables(FS, TRUE, TRUE);

	if ( FS->state == FOP_TOBREAK ){
		buf[0] = ' ';
		buf[1] = '\0';
	}else{
		if ( FS->errorcount ){
			wsprintf(buf, T("Total: %d / %s: %d\r\n"),
				FS->progs.info.donefiles, MessageText(MES_FLTE), FS->errorcount);
		}else{
			wsprintf(buf, T("Total: %d / %s\n"), FS->progs.info.donefiles, MessageText(MES_FLTC));
		}
	}
	FWriteLogMsg(FS, buf);
	SetWindowText(FS->progs.hProgressWnd, buf);

	FS->testmode = FALSE;
	FS->state = FOP_READY;
	if ( FS->page.showID == FOPTAB_GENERAL ){
		SetFocus(FS->hDstED);
	}else if ( FS->page.showID == FOPTAB_RENAME ){
		SetDlgFocus(FS->hDlg, IDE_FOP_RENAME);
	}
	UnHideWindow(FS->hDlg);
	SetTaskBarButtonProgress(FS->hDlg, TBPF_NOPROGRESS, 0);
	ActionInfo(FS->hDlg, FS->info, AJI_COMPLETE, T("foptest")); // 通知
}

void OperationResult(FOPSTRUCT *FS, BOOL result)
{
	TCHAR buf[CMDLINESIZE];

	EndingOperation(FS);
	if ( IsTrue(result) ){
		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
			TCHAR *p = buf, *mes;

			if ( FS->progs.info.donefiles == MAX32 ){
				tstrcpy(buf, MessageText(MES_COMP));
			}else{
				if ( IsTrue(FS->Cancel) ){
					mes = MES_FLCA;
				}else{
					if ( FS->progs.info.donefiles == 0 ){
						mes = MES_FLNO;
					}else if (
						((FS->progs.info.filesall == 0) || (FS->progs.info.donefiles == FS->progs.info.filesall)) &&
						(FS->progs.info.LEskips == 0) && (FS->progs.info.EXskips == 0) &&
						(FS->progs.info.errors == 0) ){
						mes = MES_FLAC;
					}else{
						mes = MES_FLPC;
					}
				}
				p += wsprintf(p, T("%s:%d"), MessageText(mes), FS->progs.info.donefiles);
				if ( FS->progs.info.filesall ){
					p += wsprintf(p, T("/%d"), FS->progs.info.filesall);
				}
				if ( FS->progs.info.LEskips ){
					p += wsprintf(p, T(" %s:%d"), MessageText(MES_FLLS),
							FS->progs.info.LEskips);
				}
				if ( FS->progs.info.EXskips ){
					p += wsprintf(p, T(" %s:%d"), MessageText(MES_FLEX),
							FS->progs.info.EXskips);
				}
				if ( FS->progs.info.errors ){
					p += wsprintf(p, T(" %s:%d"), MessageText(MES_FLER),
							FS->progs.info.errors);
				}
				tstrcpy(p, T("\r\n"));
			}
			FopLog(FS, buf, NULL, LOG_STRING);
		}

		if ( FS->opt.compcmd != NULL ){
			PP_ExtractMacro(FS->info->hWnd, FS->info, NULL, FS->opt.compcmd, NULL, 0);
		}

		if ( (FS->hEWnd == NULL) &&
			 (FS->opt.hReturnWnd != NULL) &&
			 IsParentFgWindow(FS->hDlg) ){
			SetForegroundWindow(FS->opt.hReturnWnd);
		}
	}
	DEBUGLOGF("*file end", 0);

	if ( (FS->NoAutoClose == FALSE) || (FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGRWAIT)){
		ActionInfo(FS->opt.hReturnWnd, FS->info, AJI_COMPLETE, T("fop")); // 通知

		if ( (FS->opt.hReturnWnd != NULL) && (GetFocus() != NULL) && IsWindow(FS->opt.hReturnWnd) ){
			SetForegroundWindow(FS->opt.hReturnWnd);
		}
		EndDialog(FS->hDlg, result);
	}else{ // NoAutoClose か、ログ表示しているなら閉じない
		HWND hDlg = FS->hDlg, hCancel;

		FS->state = FOP_END;
		EnableDlgWindow(hDlg, IDOK, FALSE);
		EnableDlgWindow(hDlg, IDB_TEST, FALSE);
		Enables(FS, FALSE, FALSE);

		hCancel = GetDlgItem(hDlg, IDCANCEL);
		SetWindowText(hCancel, MessageText(STR_FOPCLOSE));
		SetWindowLongPtr(hCancel, GWL_STYLE, BS_DEFPUSHBUTTON | BS_CENTER | WS_CHILD | WS_VISIBLE | WS_TABSTOP);
		SetFocus(hCancel);

		FS->progs.info.busybar = 12;
		FS->progs.srcpath = NilStr;
		FS->progs.ProgTick = 0;
		TinyDisplayProgress(FS);

		buf[0] = '\0';
		GetWindowText(hDlg, buf, TSIZEOF(buf));
		tstrcat(buf, T(" - finished"));
		SetWindowText(hDlg, buf);

		SetTaskBarButtonProgress(hDlg, TBPF_PAUSED, 0);
		ActionInfo(hDlg, FS->info, AJI_COMPLETE, T("foplog")); // 通知
		UnHideWindow(hDlg);
	}
}

void FopClose(HWND hDlg)
{
	FOPSTRUCT *FS;

	FS = (FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
	if ( FS->testmode ){
		FS->state = FOP_TOBREAK;
		FS->Cancel = TRUE;
		return;
	}
	if ( (FS->opt.hReturnWnd != NULL) && (GetFocus() != NULL) && IsWindow(FS->opt.hReturnWnd) ){
		SetForegroundWindow(FS->opt.hReturnWnd);
	}
	if ( (FS->state == FOP_READY) || (FS->state == FOP_END) ){
		EndDialog(hDlg, 0);
	}else{
		EnableDlgWindow(hDlg, IDOK, FALSE);
		EnableDlgWindow(hDlg, IDCANCEL, FALSE);
		SetDlgItemText(hDlg, IDCANCEL, T("Canceling"));
		FS->state = FOP_TOBREAK;
		FS->Cancel = TRUE;
	}
}

void FopCommands(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	FOPSTRUCT *FS;

	FS = (FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
	switch ( LOWORD(wParam) ){
		// ダイアログ共用 =============================================
		case IDOK:
			SetFocus((HWND)lParam); // IDOK にフォーカスを明示指定
			switch ( FS->state ){
				case FOP_READY:	//-------------- OK（開始）--------
					if ((FS->opt.hReturnWnd != NULL) && IsWindow(FS->opt.hReturnWnd)){
						PostMessage(FS->opt.hReturnWnd, WM_PPXCOMMAND, K_SETIME, -1);
					}
					OperationResult(FS, OperationStart(FS));
					break;

				case FOP_BUSY:	//-------------- Pause（中断希望）-
					FS->state = FOP_TOPAUSE;
					SetWindowText((HWND)lParam, MessageText(STR_FOPCONTINUE));
					SetStateOnCaption(FS, T("Pause"));
					Enables(FS, TRUE, FALSE);
					DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
					SetTaskBarButtonProgress(hDlg, TBPF_PAUSED, 0);
					SetJobTask(hDlg, JOBSTATE_PAUSE);
					break;

				case FOP_TOPAUSE: //---------- Pausing（中断解除）---
					// FOP_PAUSE へ
				case FOP_PAUSE:	//---------- Pause（中断中）-------
					FS->state = FOP_BUSY;
					SetWindowText((HWND)lParam, MessageText(STR_FOPPAUSE));
					SetStateOnCaption(FS, NULL);
					Enables(FS, FALSE, FALSE);
					FS->progs.ProgTick = 0;
					TinyDisplayProgress(FS);
					InvalidateRect(FS->progs.hProgressWnd, NULL, TRUE);
					SetJobTask(hDlg, JOBSTATE_UNPAUSE);
					break;

				case FOP_COUNT:	//---------- Skip（省略希望）------
					// FOP_WAIT へ
				case FOP_WAIT:	//---------- Start（省略希望）-----
					FS->state = FOP_SKIP;
					break;
			}
			break;

		case IDCANCEL:			// 中止 ---------------------------
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;

		case IDB_REF:				// 参照 ---------------------------
			SendMessage(FS->hDstED, WM_PPXCOMMAND, K_raw | K_s | K_c | 'I', 0);
			break;

		case IDB_TEST:				// 設定保存 / 比較 ----------------
			TestButton(FS);
			break;

		case IDS_FOP_SRCNAME:
			if ( HIWORD(wParam) == WM_CONTEXTMENU ){
				ShowSourceTextOnTip((HWND)lParam);
			}
			break;

		case IDS_FOP_SRCINFO:
		case IDS_FOP_DESTINFO:
			if ( HIWORD(wParam) == WM_CONTEXTMENU ){
				if ( FS->state != FOP_READY ) FS->Command = LOWORD(wParam);
			}
			break;

		case IDS_FOP_PROGRESS:
			if ( (HIWORD(wParam) == WM_CONTEXTMENU) && (FS->hEWnd != NULL) ){
				SetWindowY(FS, IsWindowVisible(FS->hEWnd) ? WINY_FULL : 0);
			}
			break;

		case IDC_FOP_ACTION:
			if ( HIWORD(wParam) == BN_CLICKED ) SelectAction(FS, (HWND)lParam);
			break;

		case IDC_FOP_MODE:
			SelectMode(FS, (HWND)lParam);
			break;
		// シート切り替え
		case IDB_FOP_GENERAL:	SetFopTab(FS, FOPTAB_GENERAL);	break;
		case IDB_FOP_RENAME:	SetFopTab(FS, FOPTAB_RENAME);	break;
		case IDB_FOP_OPTION:	SetFopTab(FS, FOPTAB_OPTION);	break;

		case IDQ_GETDIALOGID:
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)DialogID[FS->page.showID]);
			break;

		case IDB_FOP_LOG:
			if ( FS->hEWnd != NULL ){
				if ( IsWindowVisible(FS->hEWnd) &&
					 !((FS->state == FOP_READY) || (FS->state == FOP_END)) ){
					SetWindowY(FS, WINY_FULL);
				}else{
					SetWindowY(FS, 0);
					SetFocus(FS->hEWnd);
				}
			}
			break;

		case IDHELP:
			PPxDialogHelp(hDlg);
			break;

		case IDM_CONTINUE:
			if ( FS->state == FOP_WAIT ){
				FileOperationDlgBox(hDlg, WM_COMMAND, IDOK, 0);
				SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDM_CONTINUE);
				PostMessage(hDlg, WM_NULL, 0, 0); // メッセージループを回す
			}
			break;

		// 一般シート =============================================
		// 同名ファイル処理グループボックス
		case IDR_FOP_NEWDATE:
		case IDR_FOP_RENAME:
		case IDR_FOP_OVER:
		case IDR_FOP_SKIP:
		case IDR_FOP_ARC:
		case IDR_FOP_ADDNUM:
		case IDR_FOP_APPEND:
		case IDR_FOP_SIZE:
			if ( LOWORD(wParam) != IDR_FOP_SIZE ){
				FS->opt.fop.same = LOWORD(wParam) - IDR_FOP_NEWDATE;
			}else{
				FS->opt.fop.same = FOPSAME_SIZE;
			}
			if ( FS->opt.fop.aall ){
				FS->opt.fop.sameSW = 1;
				CheckDlgButton(hDlg, IDX_FOP_SAME, TRUE);
			}
			break;

		// 各種チェックボックス
		case IDX_FOP_BURST:
			FS->opt.fop.useburst = (char)IsControlChecked(lParam);
			break;

		case IDX_FOP_FLAT:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_FLATMODE);
			break;

		case IDX_FOP_SCOPY:
			if ( IsControlChecked(lParam) ){
				FS->opt.security = SECURITY_FLAG_SCOPY;
				setflag(FS->opt.fop.flags, VFSFOP_OPTFLAG_SECURITY);
			}else{
				FS->opt.security = SECURITY_FLAG_NONE;
				resetflag(FS->opt.fop.flags,
						VFSFOP_OPTFLAG_SECURITY | VFSFOP_OPTFLAG_SACL);
				CheckDlgButton(hDlg, IDX_FOP_SACL, FALSE);
			}
			break;

		case IDX_FOP_SACL:
			if ( IsControlChecked(lParam) ){
				FS->opt.security = SECURITY_FLAG_SACL;
				setflag(FS->opt.fop.flags,
						VFSFOP_OPTFLAG_SECURITY | VFSFOP_OPTFLAG_SACL);
				CheckDlgButton(hDlg, IDX_FOP_SCOPY, TRUE);
			}else{
				resetflag(FS->opt.fop.flags, VFSFOP_OPTFLAG_SACL);
				resetflag(FS->opt.security, SACL_SECURITY_INFORMATION);
			}
			break;

		case IDX_FOP_KEEPDIR:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_KEEPDIR);
			break;

		case IDX_FOP_ADDNUMDEST:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_ADDNUMDEST);
			break;

		case IDX_FOP_SAME:
			FS->opt.fop.sameSW = IsControlChecked(lParam);
			break;

		case IDX_FOP_AALL:
			FS->opt.fop.aall = (char)IsControlChecked(lParam);
			break;

		// 名前シート =========================================
		// 名前変更グループボックス
		case IDX_FOP_UPPER:
			FS->opt.fop.chrcase = IsControlChecked(lParam) ? 1 : 0;
			CheckDlgButton(hDlg, IDX_FOP_LOWER, FALSE);
			break;

		case IDX_FOP_LOWER:
			FS->opt.fop.chrcase = IsControlChecked(lParam) ? 2 : 0;
			CheckDlgButton(hDlg, IDX_FOP_UPPER, FALSE);
			break;

		case IDX_FOP_SFN:
			FS->opt.fop.sfn = IsControlChecked(lParam);
			break;

		case IDX_FOP_DELSPC:
			FS->opt.fop.delspc = IsControlChecked(lParam);
			break;

		case IDX_FOP_DELNUM:
			SetFlagButton(lParam, &FS->opt.fop.filter, VFSFOP_FILTER_DELNUM);
			break;

		case IDX_FOP_EXTRACTNAME:
			SetFlagButton(lParam, &FS->opt.fop.filter, VFSFOP_FILTER_EXTRACTNAME);
			break;
		// ファイル属性属性グループボックス
		case IDX_FOP_RONLY:
			SetMask(lParam, &FS->opt.fop, FILE_ATTRIBUTE_READONLY);
			break;

		case IDX_FOP_HIDE:
			SetMask(lParam, &FS->opt.fop, FILE_ATTRIBUTE_HIDDEN);
			break;

		case IDX_FOP_SYSTEM:
			SetMask(lParam, &FS->opt.fop, FILE_ATTRIBUTE_SYSTEM);
			break;

		case IDX_FOP_ARC:
			SetMask(lParam, &FS->opt.fop, FILE_ATTRIBUTE_ARCHIVE);
			break;

		case IDX_FOP_AROFF:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_AUTOROFF);
			break;
		// その他チェック
		case IDX_FOP_JOINBATCH:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_JOINBATCH);
			break;

		case IDX_FOP_1STSHEET:
			FS->opt.fop.firstsheet = (BYTE)(IsControlChecked(lParam) ? 1 : 0);
			break;

		case IDX_FOP_FILEFIX:
			SetNegFlagButton(lParam, &FS->opt.fop.filter, VFSFOP_FILTER_NOFILEFILTER);
			break;
		case IDX_FOP_EXTFIX:
			SetNegFlagButton(lParam, &FS->opt.fop.filter, VFSFOP_FILTER_NOEXTFILTER);
			break;
		case IDX_FOP_DIRFIX:
			SetNegFlagButton(lParam, &FS->opt.fop.filter, VFSFOP_FILTER_NODIRFILTER);
			break;

		// その他シート =========================================
		case IDX_FOP_NOWCREATEDIR:
			SetFlagButton(lParam, &FS->opt.fop.flags,
					VFSFOP_OPTFLAG_NOWCREATEDIR);

		case IDX_FOP_SKIP:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_SKIPERROR);
			break;

		case IDX_FOP_BACKUPOLD:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_BACKUPOLD);
			break;

		case IDX_FOP_USELOGWINDOW:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_LOGWINDOW);
			break;

		case IDX_FOP_UNDOLOG:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_UNDOLOG);
			break;

		case IDX_FOP_LOGRWAIT:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_LOGRWAIT);
			break;

		case IDX_FOP_LOWPRI:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_LOWPRI);
			if ( (FS->state >= FOP_BUSY) && (FS->state <= FOP_SKIP) ){
				SetFopLowPriority(FS);
			}
			break;

		case IDX_FOP_PREVENTSLEEP:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_PREVENTSLEEP);
			if ( (FS->state >= FOP_BUSY) && (FS->state <= FOP_SKIP) ){
				if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_PREVENTSLEEP ){
					StartPreventSleep(FS->hDlg);
				}else{
					EndPreventSleep(FS->hDlg);
				}
			}
			break;

		case IDX_FOP_ALLOWDECRYPT:
			SetFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_ALLOWDECRYPT);
			break;

		case IDX_FOP_COUNTSIZE:
			SetNegFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_NOCOUNT);
			EnableDlgWindow(hDlg, IDX_FOP_CHECKEXISTFIRST, !(FS->opt.fop.flags & VFSFOP_OPTFLAG_NOCOUNT));
			break;

		case IDX_FOP_CHECKEXISTFIRST:
			SetNegFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_NOFIRSTEXIST);
			break;

		case IDX_FOP_SERIALIZE:
			SetNegFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_QSTART);
			break;

		case IDX_FOP_WAITCLOSE:
			SetNegFlagButton(lParam, &FS->opt.fop.flags, VFSFOP_OPTFLAG_WAIT_CLOSE);
			break;
	}
}

/*-----------------------------------------------------------------------------
	ダイアログボックス処理
-----------------------------------------------------------------------------*/
void FopMinMaxInfo(HWND hDlg, LPARAM lParam)
{
	RECT box;

	box.top = 0;
	box.bottom = WINY_SETT;
	box.left = 0;
	box.right = 254; // IDD_FOP のおおまかな幅(dialog unit)
	MapDialogRect(hDlg, &box);

	((MINMAXINFO *)lParam)->ptMinTrackSize.x = box.right;
	((MINMAXINFO *)lParam)->ptMinTrackSize.y = box.bottom;
}

INT_PTR CALLBACK FileOperationDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG:
			FopDialogInit(hDlg, (FILEOPERATIONDLGBOXINITPARAMS *)lParam);
			return FALSE;

		case WM_CLOSE:
			FopClose(hDlg);
			break;

		case WM_DESTROY:
			FopDialogDestroy(hDlg);
			break;

		case WM_COMMAND:
			FopCommands(hDlg, wParam, lParam);
			break;

		case WM_NCHITTEST: { // サイズ変更を制限
			LRESULT result = DefWindowProc(hDlg, WM_NCHITTEST, wParam, lParam);

			switch ( result ){
				case HTBOTTOM:
					if ( ((FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER))->hEWnd != NULL ){
						break; // Log があるときは、サイズ変更有り
					}
					// HTTOP へ(サイズ変更不可)
				case HTTOP:
					result = HTBORDER;
					break;

				case HTBOTTOMLEFT:
					if ( ((FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER))->hEWnd != NULL ){
						break; // Log があるとき
					}
				case HTTOPLEFT:
					result = HTLEFT;
					break;

				case HTBOTTOMRIGHT:
					if ( ((FOPSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER))->hEWnd != NULL ){
						break; // Log があるとき
					}
				case HTTOPRIGHT:
					result = HTRIGHT;
					break;
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, result);
			break;
		}

		case WM_SIZE:
			if ( wParam != SIZE_MINIMIZED ){
				FixDialogControlsSize(hDlg, SizeFixFopID, IDS_FOP_PROGRESS);
			}
			return FALSE;

		case WM_GETMINMAXINFO:
			FopMinMaxInfo(hDlg, lParam);
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}
