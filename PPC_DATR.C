/*-----------------------------------------------------------------------------
	Paper Plane cUI									[A]ttribute DialogBox
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <winioctl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

const TCHAR RAttrTitle[] = T("Attrib\t");

#ifndef MONTHCAL_CLASS // カレンダー API --------------
typedef DWORD MONTHDAYSTATE, * LPMONTHDAYSTATE;
#define MONTHCAL_CLASS T("SysMonthCal32")
#define MCN_FIRST (0U-750U) // monthcal
#define MCM_FIRST 0x1000

#define MCM_GETCURSEL (MCM_FIRST + 1)
#define MonthCal_GetCurSel(hmc,pst)  (BOOL)SNDMSG(hmc,MCM_GETCURSEL,0,(LPARAM)(pst))
#define MCM_SETCURSEL (MCM_FIRST + 2)
#define MonthCal_SetCurSel(hmc, pst) (BOOL)SNDMSG(hmc,MCM_SETCURSEL,0,(LPARAM)(pst))
#define MCM_GETMINREQRECT (MCM_FIRST + 9)
#define MonthCal_GetMinReqRect(hmc, prc) SNDMSG(hmc,MCM_GETMINREQRECT,0,(LPARAM)(prc))

typedef struct tagNMSELCHANGE
{
	NMHDR nmhdr;
	SYSTEMTIME stSelStart;
	SYSTEMTIME stSelEnd;
} NMSELCHANGE,FAR * LPNMSELCHANGE;
#define MCN_SELCHANGE (MCN_FIRST + 1)
typedef struct tagNMDAYSTATE
{
	NMHDR nmhdr;
	SYSTEMTIME stStart;
	int cDayState;
	LPMONTHDAYSTATE prgDayState;
} NMDAYSTATE, FAR * LPNMDAYSTATE;
#define MCN_GETDAYSTATE (MCN_FIRST + 3)
#define MCN_SELECT (MCN_FIRST + 4)

#define MCS_DAYSTATE        0x0001
#define MCS_MULTISELECT     0x0002
#define MCS_WEEKNUMBERS     0x0004
#define MCS_NOTODAYCIRCLE   0x0008
#define MCS_NOTODAY         0x0010
#endif //------------------------------

#ifndef COMPRESSION_FORMAT_NONE
#define COMPRESSION_FORMAT_NONE 0
#define COMPRESSION_FORMAT_DEFAULT 0
#endif

typedef struct {
	PPC_APPINFO *cinfo;
	JOBINFO	jinfo;
	DWORD	setattributes,maskattributes,extattributes;
	BOOL	attributes;
	DWORD	subdir,no_modifydir;
	int		compress,crypt;
	int		create,write,access;
	FILETIME fc,fa,fw;
	HWND	hPickerWnd;
	int		PickerTarget;
} ATROPTION;

struct DATEID {
	WORD date,time,old,check;
} DateIDs[] = {
	{IDE_ATR_NCD,IDE_ATR_NCT,IDE_ATR_OC,IDX_ATR_C},
	{IDE_ATR_NWD,IDE_ATR_NWT,IDE_ATR_OW,IDX_ATR_W},
	{IDE_ATR_NAD,IDE_ATR_NAT,IDE_ATR_OA,IDX_ATR_A}
};
UINT ATR_NX_GROUP[] = {IDX_ATR_NP,IDX_ATR_NY,0};
UINT ATR_NA_GROUP[] = {IDX_ATR_NT,IDX_ATR_NO,0};
UINT ATR_NN_GROUP[] = {IDX_ATR_NR,IDX_ATR_NS,IDX_ATR_NH,IDX_ATR_NA,IDX_ATR_NI,0};

DefineWinAPI(BOOL,EncryptFile,(LPCTSTR lpFileName)) = NULL;
DefineWinAPI(BOOL,DecryptFile,(LPCTSTR lpFileName,DWORD dwReserved)) = NULL;

LOADWINAPISTRUCT EncryptDLL[] = {
	LOADWINAPI1T(EncryptFile),
	LOADWINAPI1T(DecryptFile),
	{NULL,NULL}
};

const TCHAR ExEDPROP[] = T("PPcAtrHook");	// 各コントロール拡張で使用

BOOL ToFileTime(HWND hDlg,FILETIME *ftime,const FILETIME *orgtime,struct DATEID *id);

const TCHAR calclass[] = MONTHCAL_CLASS;
// ダイアログ関連 -------------------------------------------------------------
void DateTimePick(HWND hDlg,int dateid,int index)
{
	RECT boxS,boxC,boxCtrl;
	SYSTEMTIME sTime;
	FILETIME fTime,oTime;
	ATROPTION *ao;
	HWND hPickerWnd;
	struct DATEID *id;

	hDlg = GetParent(hDlg);
	ao = (ATROPTION *)GetWindowLongPtr(hDlg,DWLP_USER);
	if ( (ao->hPickerWnd == NULL) || (IsWindow(ao->hPickerWnd) == FALSE) ){
		LoadCommonControls(ICC_DATE_CLASSES);
		ao->hPickerWnd = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME,
				calclass,calclass,WS_CAPTION | WS_THICKFRAME | WS_POPUPWINDOW,
				0,0,0,0,hDlg,NULL,hInst,0);
		if ( ao->hPickerWnd == NULL ) return;
	}
	hPickerWnd = ao->hPickerWnd;
	ao->PickerTarget = dateid;

	id = &DateIDs[index];
	if ( IsTrue(ToFileTime(hDlg,&fTime,&oTime,id)) ){
		FileTimeToLocalFileTime(&fTime,&oTime);
		FileTimeToSystemTime(&oTime,&sTime);
		MonthCal_SetCurSel(hPickerWnd,&sTime);
	}

	GetWindowRect(hPickerWnd,&boxS);
	GetClientRect(hPickerWnd,&boxC);
	MonthCal_GetMinReqRect(hPickerWnd,&boxCtrl);
	boxCtrl.right +=  (boxS.right - boxS.left) - (boxC.right - boxC.left) -
			boxCtrl.left;
	boxCtrl.bottom += (boxS.bottom - boxS.top) - (boxC.bottom - boxC.top) -
			boxCtrl.top;

	GetWindowRect(GetDlgItem(hDlg,id->time),&boxS);
	MoveWindow(hPickerWnd,boxS.right,boxS.bottom - boxCtrl.bottom,
			boxCtrl.right,boxCtrl.bottom,FALSE);
	ShowWindow(hPickerWnd,SW_SHOWNOACTIVATE);
}

void WmDatrNotify(HWND hWnd,NMHDR *nh)
{
	ATROPTION *ao;

	if ( nh->hwndFrom == NULL ) return;
	ao = (ATROPTION *)GetWindowLongPtr(hWnd,DWLP_USER);
	if ( nh->hwndFrom != ao->hPickerWnd ) return;
	{//	if ( nh->code == MCN_SELCHANGE){
		SYSTEMTIME sTime;
		TCHAR date[80];

		MonthCal_GetCurSel(ao->hPickerWnd,&sTime);
		if ( (sTime.wYear < 1980) || (sTime.wYear >= 2080) ){
			wsprintf(date,T("%04d-%02d-%02d"),
					sTime.wYear % 10000,sTime.wMonth,sTime.wDay);
		}else{
			wsprintf(date,T("%02d-%02d-%02d"),
					sTime.wYear % 100,sTime.wMonth,sTime.wDay);
		}
		SetDlgItemText(hWnd,ao->PickerTarget,date);
	}
}

// FILETIME を文字列に変換 ----------------------------------------------------
int CnvDateTime(TCHAR *pack,TCHAR *date,TCHAR *time,const FILETIME *ftime)
{
	FILETIME	lTime;
	SYSTEMTIME	sTime;
	int len = 0;

	FileTimeToLocalFileTime(ftime,&lTime);
	FileTimeToSystemTime(&lTime,&sTime);
	if ( pack != NULL ){
		len = wsprintf(pack,T("%04d-%02d-%02d%3d:%02d:%02d.%03d"),
				sTime.wYear % 10000,sTime.wMonth,sTime.wDay,
				sTime.wHour,sTime.wMinute,sTime.wSecond,sTime.wMilliseconds);
	}
	if ( date != NULL ){
		if ( (sTime.wYear < 1980) || (sTime.wYear >= 2080) ){
			wsprintf(date,T("%04d-%02d-%02d"),
					sTime.wYear % 10000,sTime.wMonth,sTime.wDay);
		}else{
			wsprintf(date,T("%02d-%02d-%02d"),
					sTime.wYear % 100,sTime.wMonth,sTime.wDay);
		}
	}
	if ( time != NULL ){
		wsprintf(time,T("%2d:%02d:%02d"),
				sTime.wHour,sTime.wMinute,sTime.wSecond);
	}
	return len;
}

// Editbox 上下カーソルでコントロール間移動をできるようにする -----------------
LRESULT CALLBACK EDProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_SETFOCUS: {
			int id;

			id = (int)GetWindowLongPtr(hWnd,GWLP_ID);
			if ( id == IDE_ATR_NCD ){
				DateTimePick(hWnd,IDE_ATR_NCD,0);
			}else if ( id == IDE_ATR_NWD ){
				DateTimePick(hWnd,IDE_ATR_NWD,1);
			}else if ( id == IDE_ATR_NAD ){
				DateTimePick(hWnd,IDE_ATR_NAD,2);
			}
			break;
		}
		case WM_KILLFOCUS: {
			ATROPTION *ao;

			ao = (ATROPTION *)GetWindowLongPtr(GetParent(hWnd),DWLP_USER);
			if ( (ao->hPickerWnd != NULL) && ((HWND)wParam != ao->hPickerWnd)){
				DestroyWindow(ao->hPickerWnd);
				ao->hPickerWnd = NULL;
			}
			break;
		}
		case WM_KEYDOWN:
			if ( wParam == VK_UP ){
				PostMessage(GetParent(hWnd),WM_NEXTDLGCTL,1,FALSE);
				return 0;
			}
			if ( wParam == VK_DOWN ){
				PostMessage(GetParent(hWnd),WM_NEXTDLGCTL,0,FALSE);
				return 0;
			}
			break;

		case WM_DESTROY: {
			WNDPROC hOldED;

			hOldED = FUNCCAST(WNDPROC,GetProp(hWnd,ExEDPROP));
			SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LONG_PTR)hOldED );
			RemoveProp(hWnd,ExEDPROP);
			return CallWindowProc(hOldED,hWnd,iMsg,wParam,lParam);
		}
//		default: // なにもしない
	}
	return CallWindowProc(
			FUNCCAST(WNDPROC,GetProp(hWnd,ExEDPROP)),hWnd,iMsg,wParam,lParam);
}

// Botton 上下カーソルでコントロール間移動／左右で on/off ---------------------
LRESULT CALLBACK BUProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_KEYDOWN:
			switch ( wParam ){
				case VK_UP:
					PostMessage(GetParent(hWnd),WM_NEXTDLGCTL,1,FALSE);
					return 0;

				case VK_DOWN:
					PostMessage(GetParent(hWnd),WM_NEXTDLGCTL,0,FALSE);
					return 0;

				case 'C':
					SetFocus(GetDlgItem(GetParent(hWnd),IDE_ATR_NCD));
					return 0;

				case 'W':
					SetFocus(GetDlgItem(GetParent(hWnd),IDE_ATR_NWD));
					return 0;

//				case 'A':
//					SetFocus(GetDlgItem(GetParent(hWnd),IDE_ATR_NAD));
//					return 0;
			}
			// WM_KEYUP へ

		case WM_KEYUP:
			if ( (wParam == VK_LEFT) || (wParam == VK_RIGHT) ){
				wParam = VK_SPACE;
			}
			break;

		case WM_GETDLGCODE:
			return DLGC_BUTTON | DLGC_WANTARROWS;

		case WM_DESTROY: {
			WNDPROC hOldED;

			hOldED = FUNCCAST(WNDPROC,GetProp(hWnd,ExEDPROP));
			SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LONG_PTR)hOldED );
			RemoveProp(hWnd,ExEDPROP);
			return CallWindowProc(hOldED,hWnd,iMsg,wParam,lParam);
		}
//		default: // なにもしない
	}
	return CallWindowProc(FUNCCAST(WNDPROC,GetProp(hWnd,ExEDPROP)),
			hWnd,iMsg,wParam,lParam);
}

// 前処理 -----------------------------------------------
int SpliterItem(WORD *item,TCHAR *str)
{
	int s,c = 0;
	TCHAR *p;
										// セパレータのカウントを行う
	for ( p = str,s = 0 ; *p != '\0' ; s++ ){
		while ( *p == ' ' ) p++;	// 先頭の空白を無視
		while ( Isalnum(*p) ) p++;	// 実体部分
		if ( *p == ' ' ){			// 末尾の空白を無視(セパレータとして認識)
			while ( *p == ' ' ) p++;
			if ( *p == '\0' ) break;	// カウント無しで終了
			continue;
		}
		if ( *p != '\0' ){					// 分離部分を発見
			p++;
			continue;
		}
		break;	// 終端
	}
										// 各項目の抽出
	for ( p = str ; *p != '\0' ; ){
		int d,offset;

		while ( *p == ' ' ) p++;	// 先頭の空白を無視
		if ( *p == '\0' ) break;
		if ( c > 4 ) return -1;	// 項目が多すぎ

		offset = 0;
		d = 0;
		if ( Isalpha(*p) ){	// 元号/午の可能性
			TCHAR oc;

			oc = upper(*p);
			if ( oc == 'H' ){		offset = 1988; // 平成1989. 1.08-
			}else if ( oc == 'S' ){	offset = 1925; // 昭和1926.12.25-1989. 1.07
			}else if ( oc == 'T' ){	offset = 1911; // 大正1912. 7.30-1926.12.25
			}else if ( oc == 'M' ){	offset = 1867; // 明治1868. 9.08-1912. 7.30
			}else{
				if ( (offset = GetExtGENGOU(NULL,NULL,oc)) == 0 ){ // 未知元号
					// AM/PM 午前/午後
					if ( oc == 'P' ){	offset = 12;
					}else if ( oc != 'A' ) return -1;
					if ( TinyCharUpper(*(p + 1)) == 'M' ) p++;
				}
			}
			p++;
		}
		if ( s != 0 ){	// セパレータあり
			while ( Isdigit(*p) ){
				d = d * 10 + (*p - '0');
				p++;
			}
		}else{
			if ( !Isdigit(*p) ) return -1;
			d = *p - '0';
			p++;
			if ( !Isdigit(*p) ){
				if ( c != 0 ) return -1;
			}else{
				d = d * 10 + (*p - '0');
				p++;
			}
		}
		if ( Isalpha(*p) ){	// 午の可能性
			TCHAR oc;

			oc = upper(*p);
			if ( oc == 'P' ){	offset = 12;
			}else if ( oc != 'A' ) return -1;
			if ( TinyCharUpper(*(p + 1)) == 'M' ) p++;
			p++;
		}
		*item++ = (WORD)(d + offset);
		c++;
		if ( s != 0 ){	// セパレータあり
			if ( *p == ' ' ){		// 末尾の空白を無視(セパレータとして認識)
				while ( *p == ' ' ) p++;
				continue;
			}
			if ( *p != '\0' ) p++;	// 分離部分を発見
		}
	}
	return c;
}
// 文字列を日時に変換する -----------------------------------------------------
BOOL ToFileTime(HWND hDlg,FILETIME *ftime,const FILETIME *orgtime,struct DATEID *id)
{
	FILETIME lTime;
	SYSTEMTIME sTime,nowTime;
	TCHAR date[MAX_PATH],time[MAX_PATH];

	GetDlgItemText(hDlg,id->date,date,MAX_PATH);
	GetDlgItemText(hDlg,id->time,time,MAX_PATH);

	FileTimeToLocalFileTime(orgtime,&lTime);
	FileTimeToSystemTime(&lTime,&sTime);
	GetLocalTime(&nowTime);

	{									// 日付の加工
		WORD n[4];

		switch ( SpliterItem(n,date) ){
			case 0:
				sTime.wYear  = nowTime.wYear;
				sTime.wMonth = nowTime.wMonth;
				sTime.wDay   = nowTime.wDay;
				break;
			case 1:			// 日のみ
				if ( n[0]) sTime.wDay = n[0];
				break;
			case 2:			// 月,日
				if ( n[0] ) sTime.wMonth = n[0];
				if ( n[1] ) sTime.wDay = n[1];
				break;
			case 3:			// 年,月,日
				if (n[0] < 100){
					if (n[0] < 80) n[0] = (WORD)(n[0] + 100);
					sTime.wYear = (WORD)(n[0] + 1900);
				}else{
					sTime.wYear = n[0];
				}
				if ( n[1] ) sTime.wMonth = n[1];
				if ( n[2] ) sTime.wDay = n[2];
				break;
			default:
				return FALSE;
		}
	}
	{									// 時刻の加工
		WORD n[4];

		switch ( SpliterItem(n,time) ){
			case 0:
				sTime.wHour   = nowTime.wHour;
				sTime.wMinute = nowTime.wMinute;
				sTime.wSecond = nowTime.wSecond;
				sTime.wMilliseconds = 0;
				break;
			case 1:			//  分のみ
				sTime.wMinute = n[0];
				sTime.wSecond = 0;
				sTime.wMilliseconds = 0;
				break;
			case 2:			// 時,分
				sTime.wHour   = n[0];
				sTime.wMinute = n[1];
				sTime.wSecond = 0;
				sTime.wMilliseconds = 0;
				break;
			case 3:			// 時,分,秒
				sTime.wHour   = n[0];
				sTime.wMinute = n[1];
				sTime.wSecond = n[2];
				sTime.wMilliseconds = 0;
				break;
			case 4:			// 時,分,秒,ミリ秒
				sTime.wHour   = n[0];
				sTime.wMinute = n[1];
				sTime.wSecond = n[2];
				sTime.wMilliseconds = n[3];
				break;
			default:
				return FALSE;
		}
	}
	if (SystemTimeToFileTime(&sTime,&lTime) == FALSE) return FALSE;
	if (LocalFileTimeToFileTime(&lTime,ftime) == FALSE ) return FALSE;
	return TRUE;
}

// 日付の入力窓の後処理 -------------------------------------------------------
void AtrEdit(HWND hDlg,WORD wParam,const FILETIME *orgtime,struct DATEID *id)
{
	if ( (wParam == EN_KILLFOCUS) && IsDlgButtonChecked(hDlg,id->check) ){
		TCHAR date[MAX_PATH],time[MAX_PATH];
		FILETIME ftime;

		if ( IsTrue(ToFileTime(hDlg,&ftime,orgtime,id)) ){
			CnvDateTime(NULL,date,time,&ftime);
			SetDlgItemText(hDlg,id->date,date);
			SendDlgItemMessage(hDlg,id->date,EM_SETSEL,0,EC_LAST);
			SetDlgItemText(hDlg,id->time,time);
			SendDlgItemMessage(hDlg,id->time,EM_SETSEL,0,EC_LAST);
		}
	}
	if (wParam == EN_CHANGE) CheckDlgButton(hDlg,id->check,TRUE);
}

// 日付の入力窓の変換処理 -----------------------------------------------------
int AtrEdit2(HWND hDlg,FILETIME *ftime,const FILETIME *orgtime,struct DATEID *id)
{
	if (!IsDlgButtonChecked(hDlg,id->check)) return 0;
	if (ToFileTime(hDlg,ftime,orgtime,id) == FALSE ) return -1;
	return 1;
}

void InitAtrBox(HWND hDlg,int atrsw,int offset)
{
	HWND hCWnd;

	if ( atrsw ){
		CheckDlgButton(hDlg,IDX_ATR_OR + offset,TRUE);
		CheckDlgButton(hDlg,IDX_ATR_NR + offset,TRUE);
	}
	hCWnd = GetDlgItem(hDlg,IDX_ATR_NR + offset);
	SetProp(hCWnd,ExEDPROP,
			(HANDLE)SetWindowLongPtr(hCWnd,GWLP_WNDPROC,(LONG_PTR)BUProc));
}

void InitDateBox(HWND hDlg,FILETIME *ft,struct DATEID *id)
{
	TCHAR date[MAX_PATH],time[MAX_PATH];
	HWND hCWnd;

	CnvDateTime(time,date,NULL,ft);

	SetDlgItemText(hDlg,id->old,time);
	*(time + 19) = 0;
	SetDlgItemText(hDlg,id->date,date );
	SetDlgItemText(hDlg,id->time,time + 11);

	CheckDlgButton(hDlg,id->check,0);

	hCWnd = GetDlgItem(hDlg,id->date);
	SetProp(hCWnd,ExEDPROP,
			(HANDLE)SetWindowLongPtr(hCWnd,GWLP_WNDPROC,(LONG_PTR)EDProc));
	hCWnd = GetDlgItem(hDlg,id->time);
	SetProp(hCWnd,ExEDPROP,
			(HANDLE)SetWindowLongPtr(hCWnd,GWLP_WNDPROC,(LONG_PTR)EDProc));
}

void AtrDetailMode(HWND hDlg)
{
	int check = IsDlgButtonChecked(hDlg,IDX_ATR_DETAIL);
	UINT *checkgroup = ATR_NN_GROUP;
	WPARAM checkstate;
	LONG_PTR style;

	#if !defined(UNICODE)
	if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT )
	#endif
	{
		CheckDlgButtonGroup(hDlg,ATR_NA_GROUP,2,check);
	}
	if ( check ){
		checkstate = 2;
		style = BS_AUTO3STATE | BS_CENTER | BS_LEFTTEXT | WS_CHILD | WS_VISIBLE | WS_TABSTOP;
	}else{
		checkstate = 0;
		style = BS_AUTOCHECKBOX | BS_CENTER | BS_LEFTTEXT | WS_CHILD | WS_VISIBLE | WS_TABSTOP;
	}
	while ( *checkgroup ){
		HWND hControlWnd;

		hControlWnd = GetDlgItem(hDlg,*checkgroup++);
		SetWindowLongPtr(hControlWnd,GWL_STYLE,style);
		SendMessage(hControlWnd,BM_SETCHECK,(WPARAM)checkstate,0);
	}
}

typedef struct {
	int offset;
	DWORD attr;
} ATTRIBUTE_CHECKS;
ATTRIBUTE_CHECKS attrs[] =
{
	{0,FILE_ATTRIBUTE_READONLY},
	{1,FILE_ATTRIBUTE_HIDDEN},
	{2,FILE_ATTRIBUTE_SYSTEM},
	{3,FILE_ATTRIBUTE_ARCHIVE},
	{4,FILE_ATTRIBUTE_COMPRESSED},
	{5,FILE_ATTRIBUTE_ENCRYPTED},
	{7,FILE_ATTRIBUTE_NOT_CONTENT_INDEXED}, // 反転
	{8,FILE_ATTRIBUTE_TEMPORARY},
	{9,FILE_ATTRIBUTE_OFFLINE},
	{0,0}
};

BOOL AttrInitDialog(ATROPTION *ao,HWND hDlg)
{
	TCHAR buf[VFPS];
	DWORD atr;
	WIN32_FIND_DATA *ff;
	PPC_APPINFO *cinfo;
	ATTRIBUTE_CHECKS *attrp = attrs;

	cinfo = ao->cinfo;
	CenterPPcDialog(hDlg,cinfo);
	LocalizeDialogText(hDlg,IDD_ATR);
									// ファイルの実在チェック
	ff = &CEL(cinfo->e.cellN).f;
	if ( VFSFullPath(buf,ff->cFileName,ao->cinfo->RealPath) == NULL ){
		goto error;
	}
	atr = GetFileAttributesL(buf);
	if ( atr == BADATTR ){
		if ( !cinfo->e.markC ) goto error;
		ff = &CEL(cinfo->e.markTop).f;
		if ( VFSFullPath(buf,ff->cFileName,ao->cinfo->RealPath) == NULL ){
			goto error;
		}
		atr = GetFileAttributesL(buf);
		if ( atr == BADATTR ) goto error;
	}
	for (;;){
		InitAtrBox(hDlg,atr & attrp->attr,attrp->offset);
		attrp++;
		if ( attrp->offset == 7 ) break;
	}
	InitAtrBox(hDlg,!(atr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED),7);
	InitAtrBox(hDlg,0,8);
	InitAtrBox(hDlg,0,9);
	CheckDlgButton(hDlg,IDX_ATR_NT,2);
	CheckDlgButton(hDlg,IDX_ATR_NO,2);
	CheckDlgButtonGroup(hDlg,ATR_NX_GROUP,2,GetNT_9xValue(TRUE,FALSE));

	InitDateBox(hDlg,&ff->ftCreationTime  ,&DateIDs[0]);
	InitDateBox(hDlg,&ff->ftLastWriteTime ,&DateIDs[1]);
	InitDateBox(hDlg,&ff->ftLastAccessTime,&DateIDs[2]);

	CheckDlgButton(hDlg,IDX_ATR_MDIR,1);
	return TRUE;
error:
	SetPopMsg(cinfo,ERROR_PATH_BUSY,NULL);
	PostMessage(hDlg,WM_CLOSE,0,0);
	return FALSE;
}


void IdOkProc(ATROPTION *ao,HWND hDlg)
{
	WIN32_FIND_DATA *ff;
	PPC_APPINFO *cinfo;
	ATTRIBUTE_CHECKS *attrp = attrs;

	cinfo = ao->cinfo;
	ff = &CEL(cinfo->e.cellN).f;
	ao->attributes = IsDlgButtonChecked(hDlg,IDX_ATR_ATR);

	ao->setattributes = 0;
	ao->maskattributes = BADATTR;
	for (;;){
		int state = IsDlgButtonChecked(hDlg,IDX_ATR_NR + attrp->offset);

		if ( state != 2 ){
			resetflag(ao->maskattributes,attrp->attr);
			if ( attrp->offset != 7 ){
				if ( state ) setflag(ao->setattributes,attrp->attr);
			}else{
				if ( !state ) setflag(ao->setattributes,attrp->attr);
			}
		}
		attrp++;
		if ( attrp->attr == 0 ) break;
	}

	ao->compress = IsDlgButtonChecked(hDlg,IDX_ATR_NP);
	ao->crypt = IsDlgButtonChecked(hDlg,IDX_ATR_NY);
	ao->subdir = IsDlgButtonChecked(hDlg,IDX_ATR_SUBD) ? FILE_ATTRIBUTE_DIRECTORY : 0;
	ao->no_modifydir = IsDlgButtonChecked(hDlg,IDX_ATR_MDIR) ? 0 : FILE_ATTRIBUTE_DIRECTORY;
	ao->create = AtrEdit2(hDlg,&ao->fc,&ff->ftCreationTime  ,&DateIDs[0]);
	ao->write  = AtrEdit2(hDlg,&ao->fw,&ff->ftLastWriteTime ,&DateIDs[1]);
	ao->access = AtrEdit2(hDlg,&ao->fa,&ff->ftLastAccessTime,&DateIDs[2]);
	if ( (ao->create == -1) || (ao->write == -1) || (ao->access == -1) ){
		PPErrorBox(hDlg,NULL,ERROR_INVALID_PARAMETER);
		return;
	}

	EndDialog(hDlg,1);
}

INT_PTR CALLBACK AttributeDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_NOTIFY:
			WmDatrNotify(hDlg,(NMHDR *)lParam);
			break;

		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg,DWLP_USER,lParam);
			return AttrInitDialog((ATROPTION *)lParam,hDlg);

		case WM_COMMAND: {
			ATROPTION *ao;
			PPC_APPINFO *cinfo;

			ao = (ATROPTION *)GetWindowLongPtr(hDlg,DWLP_USER);
			cinfo = ao->cinfo;
			switch ( LOWORD(wParam) ){
				case IDOK:
					IdOkProc(ao,hDlg);
					break;
				case IDCANCEL:
					EndDialog(hDlg,0);
					break;
				case IDX_ATR_NP:
					CheckDlgButton(hDlg,IDX_ATR_NY,0);
					break;
				case IDX_ATR_NY:
					CheckDlgButton(hDlg,IDX_ATR_NP,0);
					break;
				case IDX_ATR_NR:
				case IDX_ATR_NH:
				case IDX_ATR_NS:
				case IDX_ATR_NA:
				case IDX_ATR_NI:
				case IDX_ATR_NT:
				case IDX_ATR_NO:
					CheckDlgButton(hDlg,IDX_ATR_ATR,1);
					break;
				case IDE_ATR_NCD:
				case IDE_ATR_NCT:
					AtrEdit(hDlg,HIWORD(wParam),
						&CEL(cinfo->e.cellN).f.ftCreationTime,&DateIDs[0]);
					break;
				case IDE_ATR_NWD:
				case IDE_ATR_NWT:
					AtrEdit(hDlg,HIWORD(wParam),
						&CEL(cinfo->e.cellN).f.ftLastWriteTime,&DateIDs[1]);
					break;
				case IDE_ATR_NAD:
				case IDE_ATR_NAT:
					AtrEdit(hDlg,HIWORD(wParam),
						&CEL(cinfo->e.cellN).f.ftLastAccessTime,&DateIDs[2]);
					break;

				case IDX_ATR_DETAIL:
					AtrDetailMode(hDlg);
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg,WM_HELP,wParam,lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)IDD_ATR);
					break;
			}
			break;
		}
		default:
			return PPxDialogHelper(hDlg,iMsg,wParam,lParam);
	}
	return TRUE;
}

// 実体 -----------------------------------------------------------------------

ERRORCODE AttributeMain(ATROPTION *ao,TCHAR *path,WIN32_FIND_DATA *entry)
{
	BOOL setattr = FALSE,settime = FALSE;
	DWORD atr;
	TCHAR name[VFPS];
	PPC_APPINFO *cinfo;

	cinfo = ao->cinfo;
	VFSFullPath(name,entry->cFileName,path);
	if ( IsTrue(BreakCheck(cinfo,&ao->jinfo,name)) ){
		return ERROR_CANCELLED;
	}
	atr = entry->dwFileAttributes & 0xffffff;
//------------------------------------------------------------- sub directory
	if ( atr & ao->subdir ){
		WIN32_FIND_DATA ff;
		HANDLE hFF;
		TCHAR buf[VFPS];
		ERRORCODE result = NO_ERROR;

		CatPath(buf,name,T("*"));
		hFF = FindFirstFileL(buf,&ff);
		if ( hFF == INVALID_HANDLE_VALUE ){
			result = GetLastError();
		}else{
			do{
				if ( !(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
					 !IsRelativeDir(ff.cFileName) ){
					result = AttributeMain(ao,name,&ff);
					if ( result != NO_ERROR ) break;
				}
			}while( IsTrue(FindNextFile(hFF,&ff)) );
			FindClose(hFF);
		}
		if ( result != NO_ERROR ) return result;
	}
	if ( atr & ao->no_modifydir ) return NO_ERROR;
//-------------------------------------------------------------- 暗号化解除
	if ( (ao->crypt == 0) && (atr & FILE_ATTRIBUTE_ENCRYPTED) &&
		 ( DDecryptFile != NULL) ){
		if ( atr & FILE_ATTRIBUTE_READONLY ){	// 読み込み禁止なら解除 -------
			if(SetFileAttributesL(name,atr & ~FILE_ATTRIBUTE_READONLY)==FALSE){
				return GetLastError();
			}
			setattr = TRUE;
		}
		if ( DDecryptFile(name,0) == FALSE ){
			WriteReport(RAttrTitle,name,REPORT_GETERROR);
		}else{
			settime = TRUE;
		}
	}
//------------------------------------------------------- time stamp／圧縮 設定
	if ( ao->create || ao->write || ao->access ||
		 ((ao->compress != 2) &&
			((atr ^ ao->extattributes) & FILE_ATTRIBUTE_COMPRESSED)) ){
		HANDLE hFile;

		if ( atr & FILE_ATTRIBUTE_READONLY ){	// 読み込み禁止なら解除 -------
			if(SetFileAttributesL(name,atr & ~FILE_ATTRIBUTE_READONLY)==FALSE){
				return GetLastError();
			}
			setattr = TRUE;
		}
												// ファイルを開いて時刻変更 ---
		hFile = CreateFileL(name,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,
					NULL,OPEN_EXISTING,
					(atr & FILE_ATTRIBUTE_DIRECTORY) ?
						   FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL :
						   FILE_ATTRIBUTE_NORMAL,
					NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			if ( ao->create || ao->write || ao->access ){
				FILETIME fc,fa,fw;

				if ( IsTrue(GetFileTime(hFile,&fc,&fa,&fw)) ){
					if ( ao->create ) fc = ao->fc;
					if ( ao->write  ) fw = ao->fw;
					if ( ao->access ) fa = ao->fa;
					SetFileTime(hFile,&fc,&fa,&fw);
				}
			}
			if ( ao->compress != 2 ){
				USHORT flag;
				DWORD work;

				flag = (ao->extattributes & FILE_ATTRIBUTE_COMPRESSED) ?
						(USHORT)COMPRESSION_FORMAT_DEFAULT :
						(USHORT)COMPRESSION_FORMAT_NONE;

				if ( DeviceIoControl(hFile,FSCTL_SET_COMPRESSION,(LPVOID)&flag,
						sizeof(flag),NULL,0,&work,NULL) == FALSE ){
					WriteReport(RAttrTitle,name,REPORT_GETERROR);
				}
			}
			CloseHandle(hFile);
		}
	}
//-------------------------------------------------------------- 暗号化設定
	if ( (ao->crypt == 1) && !(atr & FILE_ATTRIBUTE_ENCRYPTED) &&
		 ( DEncryptFile != NULL) ){
		if ( atr & FILE_ATTRIBUTE_READONLY ){	// 読み込み禁止なら解除 -------
			if(SetFileAttributesL(name,atr & ~FILE_ATTRIBUTE_READONLY)==FALSE){
				return GetLastError();
			}
			setattr = TRUE;
		}
		if ( DEncryptFile(name) == FALSE ){
			WriteReport(RAttrTitle,name,REPORT_GETERROR);
		}else{
			settime = TRUE;
		}
	}
	if ( settime ){	// 時刻を復旧させる
		HANDLE hFile;
		hFile = CreateFileL(name,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,
					NULL,OPEN_EXISTING,
					(atr & FILE_ATTRIBUTE_DIRECTORY) ?
						   FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL :
						   FILE_ATTRIBUTE_NORMAL,
					NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			SetFileTime(hFile,&entry->ftCreationTime,
					&entry->ftLastAccessTime,&entry->ftLastWriteTime);
			CloseHandle(hFile);
		}
	}
//-------------------------------------------------------------- attribute 準備
	if ( ao->attributes ){
		atr = (atr & ao->maskattributes) | ao->setattributes;
		if ( atr != entry->dwFileAttributes ) setattr = TRUE;
	}
//-------------------------------------------------------------- attribute 設定
	if ( setattr ){
		if ( SetFileAttributesL(name,atr) == FALSE ){
			ERRORCODE result;

			result = GetLastError();
			WriteReport(RAttrTitle,name,result);
			return result;
		}else{
			WriteReport(RAttrTitle,name,NO_ERROR);
		}
	}
	ao->jinfo.count++;
	return NO_ERROR;
}

ERRORCODE PPC_attribute(PPC_APPINFO *cinfo)
{
	ATROPTION ao;
	ENTRYCELL *cell;
	int work;
	ERRORCODE result = ERROR_FILE_NOT_FOUND;
	TCHAR buf[MAX_PATH];

	ao.cinfo = cinfo;
	ao.hPickerWnd = NULL;

	if ( PPxDialogBoxParam(hInst,MAKEINTRESOURCE(IDD_ATR),
			cinfo->info.hWnd,AttributeDlgBox,(LPARAM)&ao) <= 0 ){
		return ERROR_CANCELLED;
	}
	PPxCommonCommand(NULL,JOBSTATE_ATTRIBUTES,K_ADDJOBTASK);
	InitJobinfo(&ao.jinfo);
	cinfo->BreakFlag = FALSE;
	ao.extattributes = 0;

	if ( ao.compress == 1) ao.extattributes = FILE_ATTRIBUTE_COMPRESSED;
	if ( (ao.crypt != 2) && (DDecryptFile == NULL) ){
		LoadWinAPI("ADVAPI32.DLL",NULL,EncryptDLL,LOADWINAPI_GETMODULE);
	}
	if ( ao.crypt == 1 ) ao.extattributes = FILE_ATTRIBUTE_ENCRYPTED;

	tstrcpy(cinfo->Jfname,CEL(cinfo->e.cellN).f.cFileName);

	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		result = AttributeMain(&ao,cinfo->RealPath,&cell->f);
		if ( result ) break;
	}
	FinishJobinfo(cinfo,&ao.jinfo,result);

	wsprintf(buf,T("Attribute changed %d"),ao.jinfo.count);
	WriteReport(NULL,buf,NO_ERROR);

	if ( (result != NO_ERROR) && (result != ERROR_CANCELLED) ){
		SetPopMsg(cinfo,result,MES_TATR);
	}
	SetRefreshAfterList(cinfo,ALST_ATTRIBUTES,'\0');
	ActionInfo(cinfo->info.hWnd,&cinfo->info,AJI_COMPLETE,T("attr"));
	PPxCommonCommand(NULL,0,K_DELETEJOBTASK);
	return result;
}
