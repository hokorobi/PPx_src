/*-----------------------------------------------------------------------------
	Paper Plane cUI		インクリメンタルサーチ
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <winioctl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

const TCHAR *SearchStateTypeString[4] = {NilStr,T("name"),T("cmt"),T("f+c")};
const TCHAR StrKC_incs[] = T("KC_incs");
const TCHAR ENTRYJUMPPROP[] = T("PPxEJUMP");
const TCHAR EENFstr[] = MES_EENF;

const TCHAR StrDlgPos_Combo[] = T("CBJDLG");
const TCHAR StrDlgPos_Single[] = T("CJDLG");

typedef struct {
	WNDPROC	hEdit;
	HWND	hEditWnd;
	int		FindIndex;
	int		OldIndex;
	TCHAR	FindStr[MAX_PATH];
	PPC_APPINFO *cinfo;
} ENTRYJUMPDIALOG;

#define INCOFF_EXT -2
#define INCOFF_MARKS -2
#define INCOFF_HIGHLIGHTS -3

int IncSearchMain(PPC_APPINFO *cinfo,const TCHAR *findstr,int first,int offset);
void SetSearchHilight(PPC_APPINFO *cinfo,const TCHAR *FindStr);
void ClearSearchHilight(PPC_APPINFO *cinfo);
INT_PTR CALLBACK EntryJumpDialog(HWND hDlg,UINT message,WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EntryJumpEditProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
ERRORCODE PPXAPI IncSearchKey(PPC_APPINFO *cinfo,WORD key);

const EXECKEYCOMMANDSTRUCT PPcISearchKey = {(EXECKEYCOMMANDFUNCTION)IncSearchKey,StrKC_incs,NULL};

void InitXC_isea(BOOL load)
{
	if ( load != FALSE ) GetCustData(T("XC_isea"),&XC_isea,sizeof(XC_isea));

	if ( !(XC_isea[0] & (ISEA_FNAME | ISEA_COMMENT)) ){
		setflag(XC_isea[0],ISEA_FNAME);
	}

	if ( XC_isea[0] & ISEA_ROMA ){
		if ( PPxCommonExtCommand(K_INITROMA,0) == FALSE ){
			resetflag(XC_isea[0],ISEA_ROMA);
		}
	}
}

void InitIncSearch(PPC_APPINFO *cinfo,TCHAR c)
{
	InitXC_isea(TRUE);

	cinfo->IncSearchMode = TRUE;
	cinfo->IncSearchTick = GetTickCount();
	cinfo->IncSearchOldN = cinfo->e.cellN;
	cinfo->IncSearchString[0] = c;
	cinfo->IncSearchString[1] = '\0';
}

void HitIncSearch(PPC_APPINFO *cinfo,ENTRYINDEX n)
{
	MoveCellCsr(cinfo,n - cinfo->e.cellN,NULL);
	SetSearchHilight(cinfo,cinfo->IncSearchString);
}

void IncSearchBackSpace(TCHAR *p)
{
#ifdef UNICODE
	int len;

	len = tstrlen32(p);
	if ( len ) *(p + len - 1) = '\0';
#else
	for ( ;; ){
		int size;

		size = Chrlen(*p);
		if ( !size ) break;
		if ( !*(p + size) ){
			*p = '\0';
			break;
		}
		p += size;
	}
#endif
}

BOOL IncSearch(PPC_APPINFO *cinfo)
{
	int n;

	if ( cinfo->IncSearchString[0] == '\0' ) return FALSE;
									// 検索準備
	n = IncSearchMain(cinfo,cinfo->IncSearchString,cinfo->IncSearchOldN,1);
	if ( n != -1 ){
		HitIncSearch(cinfo,n);
		return TRUE;
	}else{
		IncSearchBackSpace(cinfo->IncSearchString);
		return FALSE;
	}
}

void ChangeIncSearchTargetFlag(void)
{
	XC_isea[0] += ISEA_FNAME;
	if ( XC_isea[0] & (ISEA_FNAME << 2) ){
		XC_isea[0] = (XC_isea[0] & ~(ISEA_FNAME | ISEA_COMMENT | (ISEA_COMMENT << 1))) | ISEA_FNAME;
	}
}

void ShowSearchState(PPC_APPINFO *cinfo)
{

	TCHAR buf[CMDLINESIZE];

	wsprintf(buf,T("%c/%c/%s>%s"),
		XC_isea[0] & ISEA_ROMA ? 'R' : 'n',
		XC_isea[0] & ISEA_FLOAT ? 'p' : 'F',
		SearchStateTypeString[XC_isea[0] >> 24],
		cinfo->IncSearchString);

	SetPopMsg(cinfo,POPMSG_PROGRESSMSG,buf);
}

void EndSearchMode(PPC_APPINFO *cinfo)
{
	cinfo->IncSearchMode = FALSE;
	StopPopMsg(cinfo,PMF_DISPLAYMASK);
	ClearSearchHilight(cinfo);
}

void MarkSearchEntry(PPC_APPINFO *cinfo)
{
	cinfo->MarkMask = MARKMASK_DIRFILE;
	CellMark(cinfo,cinfo->e.cellN,MARK_REVERSE);
	RefleshCell(cinfo,cinfo->e.cellN);
	RefleshInfoBox(cinfo,DE_ATTR_MARK);
}

void MarkAllSearchEntry(PPC_APPINFO *cinfo,const TCHAR *FindStr)
{
	if ( (XC_isea[0] & ISEA_ROMA) && (SearchRomaString(NULL,NULL,0,NULL) == FALSE) ){
		return;
	}
	cinfo->MarkMask = MARKMASK_DIRFILE;
	IncSearchMain(cinfo,FindStr,0,INCOFF_MARKS);
	Repaint(cinfo);
}

ERRORCODE PPXAPI IncSearchKey(PPC_APPINFO *cinfo,WORD key)
{
	int n;

	if ( !(key & K_raw) ){
		return ExecKeyCommand(&PPcISearchKey,&cinfo->info,key);
	}

	switch ( key & (WORD)(0xff | K_v | K_a | K_s) ){
		case K_up:
			n = IncSearchMain(cinfo,cinfo->IncSearchString,cinfo->e.cellN - 1,-1);
			if ( n != -1 ) HitIncSearch(cinfo,n);
			break;

		case K_dw:
		case K_tab:
			n = IncSearchMain(cinfo,cinfo->IncSearchString,cinfo->e.cellN + 1,1);
			if ( n != -1 ) HitIncSearch(cinfo,n);
			break;

		case K_esc:
			EndSearchMode(cinfo);
			break;

		case K_a | 'M':
			MarkSearchEntry(cinfo);
			break;

//		case K_a | K_c | 'M':
		case K_a | K_s | 'M':
			MarkAllSearchEntry(cinfo,cinfo->IncSearchString);
			break;

		case K_bs:
			IncSearchBackSpace(cinfo->IncSearchString);
			ShowSearchState(cinfo);
			SetSearchHilight(cinfo,cinfo->IncSearchString);
			break;

		case K_a | K_v | 'O':
			XC_isea[0] ^= ISEA_ROMA;
			InitXC_isea(FALSE);
			ShowSearchState(cinfo);
			break;

		case K_a | K_v | 'F':
			XC_isea[0] ^= ISEA_FLOAT;
			ShowSearchState(cinfo);
			break;

		case K_a | K_v | 'T':
			ChangeIncSearchTargetFlag();
			ShowSearchState(cinfo);
			break;

		default:
			return ERROR_INVALID_FUNCTION;
	}
	return NO_ERROR;
}

BOOL IncSearchKeyDown(PPC_APPINFO *cinfo,WORD key,WPARAM wParam,LPARAM lParam)
{
	BYTE keys[256];
	WORD diskey[4],noskey;
	DWORD tick;

	tick = GetTickCount();
	if ( (cinfo->IncSearchTick != INCSEARCH_NOTICK) &&
		 ((tick - cinfo->IncSearchTick) >= XC_ito) ){
		EndSearchMode(cinfo);
		return FALSE;
	}
	if ( cinfo->IncSearchTick != INCSEARCH_NOTICK){
		cinfo->IncSearchTick = tick;
	}
	if ( IncSearchKey(cinfo,key) != ERROR_INVALID_FUNCTION ) return TRUE;
	if ( key & (K_c | K_a | K_e) ) return TRUE;
	noskey = key & (WORD)0xff;
	if ( noskey < VK_BACK ) return TRUE; // マウスボタン
	if ( (noskey >= VK_SHIFT) && (noskey < VK_SPACE) ){
		return TRUE; // シフトキー、IME関連キー
	}
	if ( noskey > 0xe1 ) return TRUE; // 0xe3(変換)、0xf3,0xf4(半角)等
	GetKeyboardState(keys);
	#ifdef UNICODE
	if ( 0 < ToUnicode(wParam,(lParam >> 16) & 0xff,keys,diskey,4,0) ){
	#else
	diskey[0] = 0;	// diskey[0] の上位バイトに値が入らない場合の対策
	if ( 0 < ToAscii(wParam,(lParam >> 16) & 0xff,keys,diskey,0) ){
	#endif
		if ( diskey[0] >= ' ' ) return TRUE;
	}
	EndSearchMode(cinfo);
	return FALSE;
}

BOOL WmCharSearch(PPC_APPINFO *cinfo,WORD key)
{
	int len;
	DWORD tick;
	WORD mkey;

	mkey = FixCharKeycode(key);
	if ( IncSearchKey(cinfo,mkey) != ERROR_INVALID_FUNCTION ) return TRUE;

	tick = GetTickCount();
	if ( (cinfo->IncSearchTick == INCSEARCH_NOTICK) ||
		 ((tick - cinfo->IncSearchTick) < XC_ito) ){
		if ( mkey & K_a ) return TRUE;
		if ( cinfo->IncSearchTick != INCSEARCH_NOTICK){
			cinfo->IncSearchTick = tick;
		}
		if ( (key >= ' ') && (key != 0x7f) ){
			len = tstrlen32(cinfo->IncSearchString);
			cinfo->IncSearchString[len++] = (TCHAR)key;
			cinfo->IncSearchString[len] = '\0';

			if ( cinfo->multicharkey ){
				cinfo->multicharkey = 0;
			}else{
				if ( Ismulti(key) ){
					cinfo->multicharkey = 1;
					return TRUE;
				}
			}
			IncSearch(cinfo);
			ShowSearchState(cinfo);
			if ( cinfo->IncSearchTick != INCSEARCH_NOTICK ){
				cinfo->IncSearchTick = GetTickCount();
			}
		}
		return TRUE;
	}
	EndSearchMode(cinfo);
	return FALSE;
}

int IncSearchMain(PPC_APPINFO *cinfo,const TCHAR *findstr,int first,int offset)
{
	TCHAR filter[VFPS];
	FN_REGEXP fn;
	int maxn,markmode C4701CHECK;

	if ( *findstr == '\0' ) return -1;

	maxn = cinfo->e.cellIMax;
	if ( offset <= INCOFF_EXT ){
		if ( offset == INCOFF_HIGHLIGHTS ){
			maxn = first + cinfo->cel.Area.cx * cinfo->cel.Area.cy;
			if ( maxn > cinfo->e.cellIMax ) maxn = cinfo->e.cellIMax;
			maxn -= cinfo->cellWMin;
		}else{ // ( offset == INCOFF_MARKS )
			markmode = !IsCEL_Marked(cinfo->e.cellN);
		}
	}

	if ( XC_isea[0] & ISEA_ROMA ){
		DWORD_PTR handle = 0;
		int i,n;

		n = first;
		for ( i = 0 ; i < maxn ; i++ ){
			if ( n < 0 ) n = cinfo->e.cellIMax - 1;
			if ( n >= cinfo->e.cellIMax ) n = 0;

			if ( ((XC_isea[0] & ISEA_FNAME) && SearchRomaString(
					CEL(n).f.cFileName,findstr,XC_isea[0],&handle)) ||
				 ((XC_isea[0] & ISEA_COMMENT) &&
				  (CEL(n).comment != EC_NOCOMMENT) &&
				  SearchRomaString(ThPointerT(&cinfo->EntryComments,
						CEL(n).comment),findstr,XC_isea[0],&handle)) ){
				if ( offset > INCOFF_EXT ){
					SearchRomaString(NULL,NULL,0,&handle);
					return n;
				}else{
					if ( offset == INCOFF_MARKS ){
						CellMark(cinfo,n,markmode); // C4701ok
					}else{ // INCOFF_HIGHLIGHTS
						CEL(n).state |= (BYTE)ECS_HLBIT;
					}
					n++;
					continue;
				}
			}
			if ( offset > INCOFF_EXT ){
				n += offset;
			}else{
				if ( offset == INCOFF_HIGHLIGHTS ){
					CEL(n).state &= (BYTE)ECS_HLMASK;
				}
				n++;
			}
		}
		SearchRomaString(NULL,NULL,0,&handle);
		return -1;
	}
									// 検索準備
	if ( XC_isea[0] & ISEA_FLOAT ){	// 部分一致
		wsprintf(filter,T("o:ew,%s"),findstr);
	}else{					// 前方一致
		wsprintf(filter,T("%s*"),findstr);
	}

	if ( MakeFN_REGEXP(&fn,filter) & REGEXPF_ERROR ) return -1;
	{
		int i,n;

		n = first;
		for ( i = 0 ; i < maxn ; i++ ){
			if ( n < 0 ) n = cinfo->e.cellIMax - 1;
			if ( n >= cinfo->e.cellIMax ) n = 0;

			if ( ((XC_isea[0] & ISEA_FNAME) &&
					FinddataRegularExpression(&CEL(n).f,&fn)) ||
				 ((XC_isea[0] & ISEA_COMMENT) && (CEL(n).comment != EC_NOCOMMENT) &&
					FilenameRegularExpression(ThPointerT(&cinfo->EntryComments,CEL(n).comment),&fn)) ){

				if ( offset > INCOFF_EXT ){
					FreeFN_REGEXP(&fn);
					return n;
				}else{
					if ( offset == INCOFF_MARKS ){
						CellMark(cinfo,n,markmode);
					}else{ // INCOFF_HIGHLIGHTS
						CEL(n).state |= (BYTE)ECS_HLBIT;
					}
					n++;
					continue;
				}
			}

			if ( offset > INCOFF_EXT ){
				n += offset;
			}else{
				if ( offset == INCOFF_HIGHLIGHTS ){
					CEL(n).state &= (BYTE)ECS_HLMASK;
				}
				n++;
			}
		}
	}
	FreeFN_REGEXP(&fn);
	return -1;
}

void InitEntryJumpDialog(HWND hDlg,PPC_APPINFO *cinfo)
{
	ENTRYJUMPDIALOG *PES;
	RECT DlgBox;
	RECT WndBox;
	WINPOS wpos = {{0,0,0,0},0,0};
	const TCHAR *custkey;
	HWND hRefWnd;

								// 作業領域を確保
	PES = PPcHeapAlloc(sizeof(ENTRYJUMPDIALOG));
	SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)PES);
	PES->FindIndex = 0;
	PES->OldIndex = cinfo->e.cellN;
	PES->FindStr[0] = '\0';
	PES->cinfo = cinfo;
	PES->hEditWnd = GetDlgItem(hDlg,IDE_INPUT_LINE);

	LocalizeDialogText(hDlg,IDD_EJUMP);
	InitXC_isea(TRUE);

	SetDlgItemText(hDlg,IDB_FINDTYPE,MessageText(IncTypeString[XC_isea[0] >> 24]));
	CheckDlgButton(hDlg,IDX_MODE,(XC_isea[0] & ISEA_ROMA));
	CheckDlgButton(hDlg,IDX_FRONT,!(XC_isea[0] & ISEA_FLOAT));

	PPxRegistExEdit(&cinfo->info,PES->hEditWnd,TSIZEOF(PES->FindStr),
			PES->FindStr,PPXH_WILD_R,PPXH_MASK,
			PPXEDIT_WANTEVENT | PPXEDIT_NOINCLIST);
	if ( IsTrue(SetProp(PES->hEditWnd,ENTRYJUMPPROP,(HANDLE)PES)) ){
		PES->hEdit = (WNDPROC)SetWindowLongPtr(
				PES->hEditWnd,GWLP_WNDPROC,(LONG_PTR)EntryJumpEditProc);
	}
	SendMessage(PES->hEditWnd,WM_PPXCOMMAND,KE_setkeya,(LPARAM)StrKC_incs);

	if ( (X_combos[1] & CMBS1_DIALOGNOPANE) && (cinfo->combo) ){
		custkey = StrDlgPos_Combo;
		hRefWnd = cinfo->hComboWnd;
	}else{
		custkey = StrDlgPos_Single;
		hRefWnd = cinfo->info.hWnd;
	}
	GetCustTable(Str_WinPos,custkey,&wpos,sizeof(wpos));
	GetWindowRect(hRefWnd,&WndBox);
	GetWindowRect(hDlg,&DlgBox);

	SetWindowPos(hDlg,NULL,
			WndBox.left + wpos.pos.left,WndBox.top + wpos.pos.top,
			0,0,SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

// ジャンプ処理 ---------------------------------------------------------------
void SetSearchHilight(PPC_APPINFO *cinfo,const TCHAR *FindStr)
{
	if ( !XC_isea[1] ) return;
	if ( (XC_isea[0] & ISEA_ROMA) && (SearchRomaString(NULL,NULL,0,NULL) == FALSE) ){
		return;
	}
	IncSearchMain(cinfo,FindStr,cinfo->cellWMin,INCOFF_HIGHLIGHTS);
	Repaint(cinfo);
}

void ClearSearchHilight(PPC_APPINFO *cinfo)
{
	ENTRYINDEX i;

	if ( !XC_isea[1] ) return;
	for ( i = 0 ; i < cinfo->e.cellIMax ; i++ ){
		CEL(i).state &= (BYTE)ECS_HLMASK;
	}
	Repaint(cinfo);
}

void EntryJumpSub(ENTRYJUMPDIALOG *PES,BOOL updownmode,int offset)
{
	PPC_APPINFO *cinfo;
	int n,first;

	if ( updownmode ){
		first = PES->FindIndex + offset;
	}else{
		first = PES->OldIndex;
	}

	cinfo = PES->cinfo;
	n = IncSearchMain(cinfo,PES->FindStr,first,offset);
	if ( n != -1 ){
		MoveCellCsr(cinfo,n - cinfo->e.cellN,NULL);
		SetMessageOnCaption(GetParent(PES->hEditWnd),
			( updownmode && (n == PES->FindIndex) ) ? EENFstr : NULL);
		PES->FindIndex = cinfo->e.cellN;
		SetSearchHilight(cinfo,PES->FindStr);
	}else{
		SetMessageOnCaption(GetParent(PES->hEditWnd),EENFstr);
		ClearSearchHilight(cinfo);
	}
}
// エディットボックス拡張 -----------------------------------------------------
LRESULT EntryJumpEditKey(ENTRYJUMPDIALOG *PES,WPARAM wParam)
{
	switch( wParam ){
		case K_up:		// 前の一致へ
			if ( GetShiftKey() & K_s ) break;
			EntryJumpSub(PES,TRUE,-1);
			return ERROR_SEEK;

		case K_dw:	// 次の一致へ
			if ( GetShiftKey() & K_s ) break;
			EntryJumpSub(PES,TRUE,1);
			return ERROR_SEEK;

//		default:		// 処理不要
	}
	return ERROR_INVALID_FUNCTION;
}

LRESULT CALLBACK EntryJumpEditProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	ENTRYJUMPDIALOG *PES = (ENTRYJUMPDIALOG *)GetProp(hWnd,ENTRYJUMPPROP);

	if ( PES == NULL ) return DefWindowProc(hWnd,iMsg,wParam,lParam);

	if ( iMsg == WM_KEYDOWN ){
		if ( EntryJumpEditKey(PES,wParam | K_v ) == ERROR_SEEK ) return 0;
	}
	return CallWindowProc(PES->hEdit,hWnd,iMsg,wParam,lParam);
}

void EndEntryJumpDialog(HWND hDlg,ENTRYJUMPDIALOG *PES)
{
	RECT DlgBox;
	RECT WndBox;
	WINPOS wpos = {{0,0,0,0},0,0};
	const TCHAR *custkey;
	HWND hRefWnd;

	if ( (X_combos[1] & CMBS1_DIALOGNOPANE) && (PES->cinfo->combo) ){
		custkey = StrDlgPos_Combo;
		hRefWnd = PES->cinfo->hComboWnd;
	}else{
		custkey = StrDlgPos_Single;
		hRefWnd = PES->cinfo->info.hWnd;
	}

	GetWindowRect(hRefWnd,&WndBox);
	GetWindowRect(hDlg,&DlgBox);
	wpos.pos.left = DlgBox.left - WndBox.left;
	wpos.pos.top = DlgBox.top - WndBox.top;
	SetCustTable(Str_WinPos,custkey,&wpos,sizeof(wpos));

	if ( XC_isea[2] ){
		GetWindowText(PES->hEditWnd,PES->FindStr,TSIZEOF(PES->FindStr));
		WriteHistory((WORD)XC_isea[2],PES->FindStr,0,NULL);
	}

	EndDialog(hDlg,1);
}

// ダイアログ -----------------------------------------------------------------
INT_PTR CALLBACK EntryJumpDialog(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	ENTRYJUMPDIALOG *PES;
	PES = (ENTRYJUMPDIALOG *)GetWindowLongPtr(hDlg,DWLP_USER);

	switch (message){
		case WM_INITDIALOG:
			InitEntryJumpDialog(hDlg,(PPC_APPINFO *)lParam);
			return TRUE;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					EndEntryJumpDialog(hDlg,PES);
					break;

				case IDCANCEL:
					EndDialog(hDlg,0);
					break;

				case IDE_INPUT_LINE:
					if ( HIWORD(wParam) == EN_CHANGE ){
						GetWindowText(PES->hEditWnd,
								PES->FindStr,TSIZEOF(PES->FindStr));
						EntryJumpSub(PES,FALSE,1);
					}
					break;

				case IDB_PREV:
					EntryJumpSub(PES,TRUE,-1);
					SetFocus(PES->hEditWnd);
					break;

				case IDB_NEXT:
					EntryJumpSub(PES,TRUE,1);
					SetFocus(PES->hEditWnd);
					break;

				case IDB_MARK:
					if ( (GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_SHIFT)) & KEYSTATE_PUSH ){
						GetWindowText(PES->hEditWnd,
								PES->FindStr,TSIZEOF(PES->FindStr));
						MarkAllSearchEntry(PES->cinfo,PES->FindStr);
					}else{
						MarkSearchEntry(PES->cinfo);
					}
					SetFocus(PES->hEditWnd);
					break;

				case IDX_FRONT:
					resetflag(XC_isea[0],ISEA_FLOAT);
					if ( !IsDlgButtonChecked(hDlg,IDX_FRONT) ){
						setflag(XC_isea[0],ISEA_FLOAT);
					}
					SetFocus(PES->hEditWnd);
					break;

				case IDX_MODE:
					resetflag(XC_isea[0],ISEA_ROMA);
					if ( IsDlgButtonChecked(hDlg,IDX_MODE) ){
						if ( PPxCommonExtCommand(K_INITROMA,0) == FALSE ){
							SendMessage((HWND)lParam,BM_SETCHECK,0,0);
						}else{
							setflag(XC_isea[0],ISEA_ROMA);
						}
					}
					SetFocus(PES->hEditWnd);
					break;

				case IDB_FINDTYPE:
					ChangeIncSearchTargetFlag();
					SetDlgItemText(hDlg,IDB_FINDTYPE,MessageText(IncTypeString[XC_isea[0] >> 24]));
					SetFocus(PES->hEditWnd);
					break;

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)IDD_EJUMP);
					break;
			}
			break;

		case WM_DESTROY:
			SetWindowLongPtr(PES->hEditWnd,GWLP_WNDPROC,(LONG_PTR)PES->hEdit);
			RemoveProp(PES->hEditWnd,ENTRYJUMPPROP);
			ClearSearchHilight(PES->cinfo);
			PPcHeapFree(PES);
			break;

		default:
			if ( message == WM_PPXCOMMAND ){
				return EntryJumpEditKey(PES,wParam);
			}
			return PPxDialogHelper(hDlg,message,wParam,lParam);
	}
	return TRUE;
}

void SearchEntryOnekey(PPC_APPINFO *cinfo,WORD key)
{
	int cnt,cell,seat;
	TCHAR c = (TCHAR)(BYTE)upper((TCHAR)key);

	seat = GetCustXDword(T("XC_seat"),NULL,0);
	if ( seat == 1 ){			// インクリメンタルサーチへ
		InitIncSearch(cinfo,c);
		if ( XC_isea[0] & ISEA_ROMA ) cinfo->IncSearchString[0] |= 0x20; //小文字
		if ( IncSearch(cinfo) == FALSE ){
			cinfo->IncSearchMode = FALSE;
		}
		return;
	}
	if ( seat == 2 ){			// インクリメンタルサーチへ移行するか判別
						// 条件1:直前のコマンドが検索であること
		if ( !(cinfo->PrevCommand & K_v) &&
			(((cinfo->PrevCommand & (K_e | K_s | K_c | K_a)) == (K_a | K_s)) ||
			 ((cinfo->PrevCommand & (K_e | K_s | K_c | K_a)) == K_e) ) ){
						// 条件2:前回と異なる文字であること
			if ( c != (TCHAR)(BYTE)upper((TCHAR)cinfo->PrevCommand) ){
				InitIncSearch(cinfo,(TCHAR)upper((BYTE)cinfo->PrevCommand));
				cinfo->IncSearchString[1] = c;
				cinfo->IncSearchString[2] = '\0';
				if ( XC_isea[0] & ISEA_ROMA ){ // 簡易小文字化
					cinfo->IncSearchString[0] |= 0x20;
					cinfo->IncSearchString[1] |= 0x20;
				}
				IncSearch(cinfo);
				return;
			}
		}
	}
	// 頭文字検索

	if ( (GetCustXDword(T("XC_seam"),NULL,0) != 0) ||
		 (upper(CEL(cinfo->e.cellN).f.cFileName[0]) == c) ){
		cell = cinfo->e.cellN + 1;
	}else{
		cell = 0;
	}
	for ( cnt = 0 ; cnt < cinfo->e.cellIMax ; cnt++ ){
		if ( cell >= cinfo->e.cellIMax ) cell = 0;
		if ( upper(CEL(cell).f.cFileName[0]) == c ){
			MoveCellCsr(cinfo,cell - cinfo->e.cellN,NULL);
			return;
		}
		cell++;
	}
}

void SearchEntry(PPC_APPINFO *cinfo)
{
	ENTRYINDEX oldcellN;

	oldcellN = cinfo->e.cellN;

	if ( PPxDialogBoxParam(hInst,MAKEINTRESOURCE(IDD_EJUMP),
				cinfo->info.hWnd,EntryJumpDialog,(LPARAM)cinfo) <= 0 ){
		MoveCellCsr(cinfo,oldcellN - cinfo->e.cellN,NULL); // 戻す
	}
}
