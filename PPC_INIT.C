/*-----------------------------------------------------------------------------
	Paper Plane cUI										初期化/終了処理
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUI.RH"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "FATTIME.H"
#include "PPCOMBO.H"
#pragma hdrstop

const TCHAR InfoName[] = T("PPc");
const TCHAR PPcClassStr[] = T(PPCWinClass);
const TCHAR TaskbarButtonCreatedReg[] = T("TaskbarButtonCreated");
const TCHAR *runasstate = NULL;
const TCHAR DefCID[] = T("CA"); // 本来の ID で設定が見つからなかったときに使う
PPXINMENU barFile[] = {
	{'K'		,T("Ma&ke dir...\tK")},
	{(DWORD_PTR)T("?newmenu"),T("Make entry(&W)...\tShift+K")},
	{K_c | 'E'	,T("Explorer Here\tCtrl+E")},
	{PPXINMENY_SEPARATE,NULL},
	{K_c | K_s | K_F10,T("S.C. Menu...\tCtrl+Shift+F10")},
	{K_c | K_cr	,T("Entry Menu...\tCtrl+Enter")},
	{'N'		,T("PPV\t&N")},
	{'Y'		,T("PPV(hold)\t&Y")},
	{'V'		,T("&Viewer\tV")},
	{'e'		,T("Text &Edit...\tE")},
	{'U'		,T("&Unpack...\tU")},
	{(DWORD_PTR)T("*pack \"%2%\\|%X|\" %Or-")	,T("&Pack...\tP")},
	{(DWORD_PTR)T("*pack \"|%2%\\|\",indiv %Or-")	,T("individual Pack...")},
	{'X'		,T("Execute...\tX")},
	{PPXINMENY_SEPARATE,NULL},
	{K_c | 'D'	,T("D&&D...\tCtrl+D")},
	{PPXINMENY_SEPARATE,NULL},
	{'D'		,T("&Delete to recycle bin\tD")},
	{K_s | 'D'	,T("Delete\tShift+D")},
	{'R'		,T("&Rename...\tR")},
	{K_c | 'R'	,T("Continuous rename...\tCtrl+R")},
	{K_s | 'R'	,T("ExRename...\tShift+R")},
	{K_s | 'O'	,T("Comment,hash...\tShift+&O")},
	{(DWORD_PTR)T("*ppffix \"%C\""),T("&Fix file ext.")},
	{PPXINMENY_SEPARATE,NULL},
	{'A'		,T("&Attribute...\tA")},
	{K_c | 'I'	,T("&Information\tI")},
	{K_a | K_cr ,T("Properties...\tAlt+Enter")},
	{PPXINMENY_SEPARATE,NULL},
	{'Q'		,T("Close\tQ")},
	{K_a | K_F4	,T("E&xit\tAlt+F4")},
	{0,NULL}
};
PPXINMENU barEdit[] = {
//	{0			,T("&Undo\tCtrl+Z")},
//	{PPXINMENY_SEPARATE,NULL},
	{K_c | 'X'	,T("Cu&t\tCtrl+X")},
	{K_c | 'C'	,T("&Clip File\tCtrl+C")},
	{K_c | 'V'	,T("&Paste\tCtrl+V")},
	{K_c | K_s | 'V',T("Paste &Shortcut\tCtrl+Shift+V")},
	{1		,NULL},
		{K_c | K_s | 'C',T("This directory\tCtrl+Shift+C")},
		{(DWORD_PTR)T("*cliptext %C"),T("&Filename")},
		{(DWORD_PTR)T("*cliptext %X"),T("Filename(widthout e&xt)")},
		{(DWORD_PTR)T("*cliptext %T"),T("&Extension")},
		{K_c | 'C',T("Full&path")},
		{(DWORD_PTR)T("*cliptext %2"),T("Pair Directory(&2)")},
		{0			,T("others C&lip")},
	{PPXINMENY_SEPARATE,NULL},
	{'W'		,T("&Write entry\tW")},
	{K_c | 'W'	,T("Whe&re is...\tCtrl+W")},
	{PPXINMENY_SEPARATE,NULL},
	{'C'		,T("&Copy...\tC")},
	{'M'		,T("&Move...\tM")},
	{PPXINMENY_SEPARATE,NULL},
	{'O'		,T("C&ompare mark...\tO")},
	{'+'		,T("Add Mark...\t+")},
	{'-'		,T("Del Mark...\t-")},
	{'/'		,T("Split Mark\t/")},
	{'*'		,T("Mark All\t*")},
	{K_s | K_home,T("&Invert Mark All\tShift+Home")},
	{K_c | 'A'	,	T("Mark &All with dir\tCtrl+A")},
	{K_s | K_end,T("&Invert Mark All with dir\tShift+End")},
	{0			,NULL}
};
PPXINMENU barView[] = {
	{(DWORD_PTR)T("?layoutmenu"),T("&Layout")},
	{K_s | 'T'	,T("&Tree window\tShift+T")},
	{K_s | 'Y'	,T("S&ync View\tShift+Y")},
	{K_c | K_s | 'I',T("Sync Info\tCtrl+Shift+I")},
	{K_a | K_s | K_cr,T("Sync properties\tAlt+Shift+Enter")},
	{'I'			,T("Drive Info\tI")},
	{(DWORD_PTR)T("*countsize")	,T("Count file size")},
	{K_s | K_ins	,T("Zoom in\tShift+Ins")},
	{K_s | K_del	,T("Zoom out\tShift+Del")},
	{K_a | '-'	,T("&Pane menu\tAlt+ -")},
	{1		,NULL},
		{'G'		,T("Swap\tG")},
		{K_F11		,T("More PPC\tF11")},
		{K_c | K_F11,T("Runas PPC\tCtrl+F11")},
		{KC_WIND	,T("Option...")},
		{PPXINMENY_SEPARATE,NULL},
		{K_tab		,T("Next PPC\tTAB")},
		{K_tab		,T("Previous PPC\tShift+TAB")},
		{PPXINMENY_SEPARATE,NULL},
		{(DWORD_PTR)T("??selectppx")	,NilStr},
		{0			,T("&Window")},
	{PPXINMENY_SEPARATE,NULL},
	{'F'		,T("&Find...\tF")},
	{K_s | 'F'	,T("wild &card...\tShift+F")},
	{(DWORD_PTR)T("?viewmenu")	,T("&View\t;")},
	{(DWORD_PTR)T("?sortmenu")	,T("&Sort\tS")},
	{PPXINMENY_SEPARATE,NULL},
	{1		,NULL},
		{K_c | K_lf	,T("previous\tCtrl+Left")},
		{K_c | K_ri	,T("next\tCtrl+Right")},
		{K_bs		,T("Up\tBS")},
		{'\\'		,T("Root\t\\")},
		{K_s | K_bs	,T("before\tShift+BS")},
		{K_c | K_s | K_lf	,T("p-list(&V)\tCtrl+Shift+Left")},
		{K_c | K_s | K_ri	,T("n-list(&N)\tCtrl+Shift+Right")},
		{'L'		,T("&Go to...\tL")},
		{(DWORD_PTR)T("?drivemenu")	,T("Drives(&L)\tShift+L")},
		{'T'		,T("&Tree\tT")},
		{PPXINMENY_SEPARATE,NULL},
		{'='		,T("Same Path\t=")},
		{(DWORD_PTR)T("%j%2")	,T("Pair window Path")},
		{0			,T("Directory(&G)")},
	{PPXINMENY_SEPARATE,NULL},
	{(DWORD_PTR)T("?diroptionmenu")	,T("Dir. Settings(&O)")},
	{K_c | 'L'	,T("Re&draw\tCtrl+L")},
	{K_v | K_c | K_F5,T("Update \tCtrl+F5")},
	{'.'		,T("&Reload\tF5")},
	{0			,NULL}
};

PPXINMENU barFavorites[] = {
	{(DWORD_PTR)T("??favorites"),NilStr},
	{0			,NULL}
};


PPXINMENU barTool[] = {
	{K_v | VK_PAUSE,T("PAUSE\tPAUSE")},
	{K_c | 'F'	,T("&Find...\tCtrl+F")},
	{KC_Tvfs	,T("&VFS switch")},
	{PPXINMENY_SEPARATE,NULL},
	{'H'		,T("s&Hell...\tH")},
	{'I'		,T("Drive &Info...\tI")},
	{PPXINMENY_SEPARATE,NULL},
	{K_ANDV		,T("Allocate Network drive(&M)...")},
	{K_FNDV		,T("Free Network drive(&D)...")},
	{PPXINMENY_SEPARATE,NULL},
	{1		,NULL},
		{K_SSav		,T("ScreenSaver")},
		{(DWORD_PTR)T("*monitoroff")	,T("sleep Monitor")},
		{PPXINMENY_SEPARATE,NULL},
		{K_Loff		,T("Logoff")},
		{K_Poff		,T("Poweroff")},
		{K_Rbt		,T("Reboot")},
		{K_Sdw		,T("Shutdown")},
		{(DWORD_PTR)T("*lockpc")	,T("lock PC")},
		{K_Suspend	,T("Suspend")},
		{K_Hibernate,T("Hibernate")},
		{0			,T("Oth&er")},
	{'J'		,T("Incremental search...\tJ")},
	{PPXINMENY_SEPARATE,NULL},
	{K_cust		,T("&Customizer")},
	{0			,NULL}
};
PPXINMENU barHelp[] = {
	{K_s | K_F1	,T("&Topic\tShift+F1")},
	{K_F1		,T("&Help\tF1")},
	{(DWORD_PTR)T("%Obd *ppcust /c"),T("&Command list")},
	{PPXINMENY_SEPARATE,NULL},
	{K_supot	,T("&Support")},
	{(DWORD_PTR)T("*checkupdate"),T("Check &Update")},
	{PPXINMENY_SEPARATE,NULL},
	{K_about	,T("&About")},
	{0			,NULL}
};

PPXINMENUBAR ppcbar[] = {
	{T("&File"),barFile},
	{T("&Edit"),barEdit},
	{T("&View"),barView},
	{T("F&avorites"),barFavorites},
	{T("&Tool"),barTool},
	{T("&Help"),barHelp},
	{NULL,NULL}
};

const TCHAR DefDirString[] = MES_DIRS;
const TCHAR DefStrBusy[] = MES_BUSY;
const TCHAR StrLoading[] = T("loading");

#if USEDELAYCURSOR || defined(USEDIRECTX)
#pragma argsused
void CALLBACK FloatProc(HWND hWnd,UINT unuse1,UINT_PTR unuse2,DWORD unuse3)
{
#if USEDELAYCURSOR
	PPC_APPINFO *cinfo;
	BOOL draw = FALSE;
	int delta;

	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	delta = (cinfo->TargetNpos.x - cinfo->cellNpos.x) / 2;
	if ( delta ){
		draw = TRUE;
		cinfo->cellNpos.x += delta;
	}else{
		if ( cinfo->cellNpos.x != cinfo->TargetNpos.x ){
			draw = TRUE;
			cinfo->cellNpos.x = cinfo->TargetNpos.x;
		}
	}
	delta = (cinfo->TargetNpos.y - cinfo->cellNpos.y) / 2;
	if ( delta ){
		draw = TRUE;
		cinfo->cellNpos.y += delta;
	}else{
		if ( cinfo->cellNpos.y != cinfo->TargetNpos.y ){
			draw = TRUE;
			cinfo->cellNpos.y = cinfo->TargetNpos.y;
		}
	}
#endif
#ifndef USEDIRECTX
	if ( draw ) InvalidateRect(hWnd,&cinfo->BoxEntries,TRUE);
#else
	InvalidateRect(hWnd,NULL,FALSE);
#endif
}
#endif

BOOL CallPPcParam(HWND hWnd,const TCHAR *param) // WinMain 内から、使用中PPcへ送信(コマンドライン)
{
	PPCSTARTPARAM psp;

	psp.show = SW_SHOW;
	LoadParam(&psp,param);
	return CallPPc(&psp,hWnd);
}

BOOL CallPPc(PPCSTARTPARAM *psp,HWND hWnd) // WinMain 内から、使用中PPcへ送信
{
	COPYDATASTRUCT copydata;
	ThSTRUCT th;
	HWND hSendWnd;
									// 呼び出し先 PPc を決定する
	if ( hWnd != NULL ){
		hSendWnd = hWnd;
	}else{
		hSendWnd = PPxCombo(BADHWND);
		if ( hSendWnd == BADHWND ){
			hSendWnd = PPcGetWindow(0,CGETW_GETFOCUS);
		}
		if ( hSendWnd == NULL ) return FALSE;
	}
									// 送信内容を作成
	ThInit(&th);
	ThAppend(&th,psp,sizeof(PPCSTARTPARAM));
	if ( psp->next != NULL ){
		ThAppend(&th,psp->next,psp->th.top);
		ThFree(&psp->th);
	}
	if ( IsTrue(psp->UseCmd) ){
		((PPCSTARTPARAM *)th.bottom)->cmd = (const TCHAR *)th.top;
		ThAddString(&th,psp->cmd);
	}
	if ( (hWnd == NULL) &&
		 (psp->show != SW_SHOWNOACTIVATE) &&
		 (psp->show != SW_SHOWMINNOACTIVE) ){
		SetForegroundWindow(hSendWnd);
	}
									// 送信
	copydata.dwData = KC_MOREPPC;
	copydata.cbData = th.top;
	copydata.lpData = th.bottom;
	SendMessage(hSendWnd,WM_COPYDATA,(WPARAM)(HWND)NULL,(LPARAM)&copydata);
	ThFree(&th);
	return TRUE;
}


/*=============================================================================
	PPC を新規起動
=============================================================================*/
void PPCuiWithPath(HWND hWnd,const TCHAR *path)
{
	TCHAR cmdline[CMDLINESIZE];

	wsprintf(cmdline,T("\"%s\""),path);
	PPCui(hWnd,cmdline);
}

#if NODLL
extern void GetIDLSub(TCHAR *path,LPSHELLFOLDER pSF,LPITEMIDLIST pSHidl);
#else
void GetIDLSub(TCHAR *path,LPSHELLFOLDER pSF,LPITEMIDLIST pSHidl)
{
	TCHAR *p;
	BYTE *d;

	tstrcpy(path,T("#:\\"));
	p = path + 3;
	d = (BYTE *)pSHidl;
	while( *(WORD *)d ){
		WORD *nextp,old;

		nextp = (WORD *)(BYTE *)(d + *(WORD *)d);
		old = *nextp;
		*nextp = 0;
		if ( FALSE == PIDL2DisplayNameOf(p,pSF,pSHidl) ) break;
		*(p - 1) = '\\';
		p += tstrlen(p) + 1;
		*nextp = old;
		d = (BYTE *)nextp;
	}
}
#endif

// %I から path を取得する(WindowsXP限定)
void GetIDLdigit(TCHAR *path,HANDLE hSHmem,DWORD SHid)
{
	DefineWinAPI(LPVOID,SHLockShared,(HANDLE hData,DWORD dwOtherProcId));
	DefineWinAPI(BOOL,SHUnlockShared,(LPVOID lpvData));
	DefineWinAPI(BOOL,SHFreeShared,(HANDLE hData,DWORD dwSourceProcId));
	LPITEMIDLIST pSHidl;
	HMODULE hShell32;

	// WinXP 以降でないと使用できない(MSDNでは2000以降だが実はエントリがない)
	hShell32 = GetModuleHandle(StrShell32DLL);
	GETDLLPROC(hShell32,SHLockShared);
	// W2kやVistaでは用意されていない
	if ( DSHLockShared == NULL ) return;
	GETDLLPROC(hShell32,SHUnlockShared);
	GETDLLPROC(hShell32,SHFreeShared);

	pSHidl = (LPITEMIDLIST)DSHLockShared(hSHmem,SHid);
	if ( pSHidl != NULL ){
		LPSHELLFOLDER pSF;

		SHGetDesktopFolder(&pSF);
		GetIDLSub(path,pSF,pSHidl);

		pSF->lpVtbl->Release(pSF);
		DSHUnlockShared(pSHidl);
		DSHFreeShared(hSHmem,SHid);
	}
}

// path を取得する(WindowsVista用)
void GetIDLstring(TCHAR *path)
{
	LPSHELLFOLDER pSF;
	LPITEMIDLIST pSHidl;

	SHGetDesktopFolder(&pSF);
	pSHidl = IShellToPidl(pSF,path);
	if ( pSHidl != NULL ){
		GetIDLSub(path,pSF,pSHidl);
	}
	pSF->lpVtbl->Release(pSF);
}

BOOL CALLBACK EnumChildFindEditProc(HWND hWnd,LPARAM lParam)
{
	TCHAR classname[MAX_PATH];
	HWND *hFoundWnd;

	if ( !GetClassName(hWnd,classname,TSIZEOF(classname)) ) return TRUE;
	if ( tstricmp(classname,T("edit")) ) return TRUE;
	hFoundWnd = (HWND *)lParam;
	if ( *hFoundWnd != NULL ){
		*hFoundWnd = NULL;
		return FALSE;
	}
	*hFoundWnd = hWnd;
	return TRUE;
}

HWND GetDestChoose(void)
{
	HWND hWnd,hFoundWnd = NULL;

	hWnd = GetForegroundWindow();
	if ( hWnd != NULL ){
		EnumChildWindows(hWnd,EnumChildFindEditProc,(LPARAM)&hFoundWnd);
	}
	return hFoundWnd;
}

typedef struct {
	int RegMode;
	PSPONE pspo;
	TCHAR RegID[REGIDSIZE];
} OneInfoStruct;

void MakeRegSubID(PPC_APPINFO *cinfo)
{
	if ( cinfo->RegSubIDNo < 26 ){
		wsprintf(cinfo->RegSubCID, T("C%c%c%c"), cinfo->RegID[2],
//			cinfo->combo ? TinyCharLower(ComboID[2]) : '_',
			TinyCharLower(ComboID[2]),
			cinfo->RegSubIDNo + 'a');
	}else{
		wsprintf(cinfo->RegSubCID, T("C%c%c%c%c"), cinfo->RegID[2],
//			cinfo->combo ? TinyCharLower(ComboID[2]) : '_',
			TinyCharLower(ComboID[2]),
			(cinfo->RegSubIDNo / 26) + 'a',
			(cinfo->RegSubIDNo % 26) + 'a');
	}
}

void GetRegID(OneInfoStruct *one,const TCHAR *param)
{
	one->pspo.id.RegID[2] = upper(*param);
	one->pspo.id.RegID[3] = '\0';
	one->pspo.id.SubID = -1;
	if ( (one->pspo.id.RegID[2] == 'Z') && Isalpha(*(param + 1)) ){
		one->pspo.id.SubID = 0;
		param += 2;
		for (;;){
			if ( !Isalpha( *param ) ) break;
			one->pspo.id.SubID = one->pspo.id.SubID * 26 + (upper(*param) - 'A');
			param++;
		}
	}
}

void InitPspo(OneInfoStruct *one)
{
	tstrcpy(one->pspo.id.RegID,one->RegID);
	one->pspo.id.RegMode = one->RegMode;
	one->pspo.id.SubID = -1;
	one->pspo.id.Pair = FALSE;
	one->pspo.combo.dirlock = FALSE;
	one->pspo.combo.select = FALSE;
	one->pspo.combo.pane = PSPONE_PANE_DEFAULT;
	one->pspo.path[0] = '\0';
}

void AddPspo(PPCSTARTPARAM *psp,OneInfoStruct *one)
{
	ThAppend(&psp->th,&one->pspo,PSPONE_size(&one->pspo));
	InitPspo(one);
}

BOOL LoadParam(PPCSTARTPARAM *psp,const TCHAR *param)
{
	OneInfoStruct one;
	TCHAR buf[CMDLINESIZE];
	BOOL modify = FALSE;
	const TCHAR *more;
	UTCHAR code;

	one.RegMode = PPXREGIST_NORMAL;
	tstrcpy(one.RegID,T(PPC_REGID));

#ifdef UNICODE	// UNICODE 版は先頭に ppc を起動したときの指定が入っている
	if ( param == NULL ){
		param = GetCommandLine();
		GetLineParam(&param,buf);
	}
#endif
	// オプション初期化
	psp->SingleProcess = X_sps;
	psp->Reuse = FALSE;
	psp->UseCmd = FALSE;
	psp->AllocCmd = FALSE;
	//psp->cmd = NULL
	psp->usealone = FALSE;
	//psp->show
	psp->Focus = '\0';
	psp->ComboID = '\0';
	psp->next = NULL;
	one.pspo.combo.use = X_combo;
	ThInit(&psp->th);
	InitPspo(&one);

	while( '\0' != (code = GetOptionParameter(&param,buf,CONSTCAST(TCHAR **,&more))) ){
		if ( (code != '-') || (buf[1] == '#') ||
			 (tstrcmp(buf,T("-SHELL")) == 0) ){	// ディレクトリ指定
			if ( (code == '-') && (buf[1] == 'S') ){ // -shell: 形式
				Strlwr(buf);
				buf[6] = ':';
			}
			// 頭が - でないか、頭が -#～
			if ( one.pspo.path[0] != '\0' ){	// 別の窓のディレクトリ指定
				AddPspo(psp,&one);
			}
			VFSFixPath(one.pspo.path,buf,NULL,VFSFIX_VFPS | VFSFIX_NOFIXEDGE);
			continue;
		}
									//	"min"
		if ( !tstrcmp( buf + 1,T("MIN") )){
			modify = TRUE;
			psp->show = SW_SHOWMINNOACTIVE;
			continue;
		}
									//	"max"
		if ( !tstrcmp( buf + 1,T("MAX") )){
			modify = TRUE;
			psp->show = SW_SHOWMAXIMIZED;
			continue;
		}
									//	"noactive"
		if ( !tstrcmp( buf + 1,T("NOACTIVE") )){
			modify = TRUE;
			psp->show = SW_SHOWNOACTIVATE;
			continue;
		}
									//	"selectnoactive"
		if ( !tstrcmp( buf + 1,T("SELECTNOACTIVE") )){
			modify = TRUE;
			psp->show = SW_SHOWNOACTIVATE;
			one.pspo.combo.select = TRUE;
			continue;
		}
									//	"show"
		if ( !tstrcmp( buf + 1,T("SHOW") )){
			modify = TRUE;
			psp->show = SW_SHOWNORMAL;
			continue;
		}
									//	"alone"
		if ( !tstrcmp( buf + 1,T("ALONE") )){
			if ( X_combo == 0 ) X_combo = 1;
			if ( !one.pspo.combo.use ) one.pspo.combo.use = 1;
			psp->ComboID = '@';
			if ( Isalpha(*more) ) psp->ComboID = upper(*more);
			psp->usealone = TRUE;
			continue;
		}
									//	"combo"
		if ( !tstrcmp( buf + 1,T("COMBO") )){
			if ( X_combo == 0 ) X_combo = 1;
			if ( !one.pspo.combo.use ) one.pspo.combo.use = 1;
			psp->ComboID = 'A';
			if ( *more == '+' ) psp->ComboID = '@';
			if ( Isalpha(*more) ) psp->ComboID = upper(*more);
			continue;
		}
									//	"lock"
		if ( !tstrcmp( buf + 1,T("LOCK") )){
			modify = TRUE;
			one.pspo.combo.dirlock = TRUE;
			continue;
		}
									//	"tab"
		if ( !tstrcmp( buf + 1,T("TAB") )){
			modify = TRUE;
			X_combo = 2;
			if ( !one.pspo.combo.use ) one.pspo.combo.use = 2;
			continue;
		}
									//	"single"
		if ( !tstrcmp( buf + 1,T("SINGLE") )){
			modify = TRUE;
			X_combo = 0;
			one.pspo.combo.use = 0;
			continue;
		}
									//	"sps" single process
		if ( !tstrcmp( buf + 1,T("SPS") )){
			modify = TRUE;
			X_sps = psp->SingleProcess = TRUE;
			continue;
		}
									//	"mps" multi process
		if ( !tstrcmp( buf + 1,T("MPS") )){
			modify = TRUE;
			X_sps = psp->SingleProcess = FALSE;
			continue;
		}
									//	"K"
		if ( !tstrcmp( buf + 1,T("K") )){
			psp->UseCmd = TRUE;
			psp->cmd = param;
			break;
		}
									//	"R"
		if ( !tstrcmp( buf + 1,T("R") )){
			psp->Reuse = TRUE;
			continue;
		}
									//	"PANE"
		if ( !tstrcmp( buf + 1,T("PANE") )){
			if ( (UTCHAR)*more <= ' ' ){
				one.pspo.combo.pane = PSPONE_PANE_NEWPANE;
			}else if ( *more == '~' ){
				more++;
				one.pspo.combo.pane = PSPONE_PANE_PAIR;
			}else if ( *more == 'r' ){
				more++;
				one.pspo.combo.pane = PSPONE_PANE_RIGHTPANE;
			}else{
				if ( *more == 'l' ) more++;
				one.pspo.combo.pane = GetNumber(&more) + PSPONE_PANE_SETPANE;
				if ( one.pspo.combo.pane < PSPONE_PANE_SETPANE ){
					one.pspo.combo.pane = PSPONE_PANE_DEFAULT;
				}
			}
			modify = TRUE;
			continue;
		}
									//	"BOOTID:[~]A-Z"
		if ( !tstrcmp( buf + 1,T("BOOTID") )){
			if ( (one.pspo.id.RegMode != one.RegMode) ||
				 IsTrue(one.pspo.id.Pair) ){	// 別の窓のID指定
				AddPspo(psp,&one);
			}
			modify = TRUE;
			if ( *more == '~' ){
				one.pspo.id.Pair = TRUE;
			}else if ( Isalpha(*more) ){
				one.pspo.id.RegMode = PPXREGIST_IDASSIGN;
				GetRegID(&one,more);
				tstrcpy(one.RegID,one.pspo.id.RegID);
			}
			continue;
		}
									//	"BOOTMAX:A-Z"
		if ( !tstrcmp( buf + 1,T("BOOTMAX") )){
			modify = TRUE;
			if ( Isalpha(*more) ){
				one.pspo.id.RegMode = PPXREGIST_MAX;
				GetRegID(&one,more);
			}
			continue;
		}
									//	"CHOOSE:EDIT/D&D/CON"
		if ( !tstrcmp( buf + 1,T("CHOOSE") )){
			one.pspo.combo.use = 0;
			X_combo = 0;
			switch ( TinyCharUpper(*more) ){
				case 'E':
					hChooseWnd = GetDestChoose();
					if ( hChooseWnd == NULL ){
						xmessage(XM_GrERRld,T("Not found"));
						return FALSE;
					}
					X_ChooseMode = CHOOSEMODE_EDIT;
					break;
				case 'D':
					X_ChooseMode = CHOOSEMODE_DD;
					hChooseWnd = GetForegroundWindow();
					if ( hChooseWnd == NULL ){
						xmessage(XM_GrERRld,T("Not found"));
						return FALSE;
					}
					break;
				case 'C':
					X_ChooseMode = CHOOSEMODE_CON;
					if ( tstrchr(more,'1') ) X_ChooseMode = CHOOSEMODE_CON_UTF16;
					if ( tstrchr(more,'8') ) X_ChooseMode = CHOOSEMODE_CON_UTF8;
					break;
				case 'M':
					X_ChooseMode = CHOOSEMODE_MULTICON;
					if ( tstrchr(more,'1') ) X_ChooseMode = CHOOSEMODE_MULTICON_UTF16;
					if ( tstrchr(more,'8') ) X_ChooseMode = CHOOSEMODE_MULTICON_UTF8;
					break;
			}
			continue;
		}
									//	"IDL" Windowsの拡張子判別登録で使用
		if ( !tstrcmp( buf + 1,T("IDL") ) ){
			HANDLE hSHmem;
			DWORD SHid;

			if ( *more == ':' ) more++;
			hSHmem = (HANDLE)GetNumber(&more);
			if ( *more == ':' ) more++;
			SHid = GetDwordNumber(&more);
			GetIDLdigit(one.pspo.path,hSHmem,SHid);
			if ( *param ){
				tstrcpy(one.pspo.path,param);
				if ( (*one.pspo.path == ':') && (*(one.pspo.path +1) == ':') ){
					GetIDLstring(one.pspo.path);
				}
				break;
			}
			continue;
		}
		XMessage(NULL,NULL,XM_GrERRld,StrBadOption,buf);
	}
	if ( IsTrue(modify) ||
		 (one.pspo.path[0] != '\0') ||
		 (one.pspo.id.RegMode != one.RegMode) ||
		 IsTrue(one.pspo.id.Pair) ||
		 (one.pspo.combo.use != X_combo) ){
		AddPspo(psp,&one);
	}
	if ( psp->th.bottom != NULL ){
		ThAppend(&psp->th,NilStr,TSTROFF(1));
	}
	psp->next = (PSPONE *)psp->th.bottom;
	return TRUE;
}

void SetWindowMinMax(HWND hWnd,PPCSTARTPARAM *psp)
{
	HWND hParentWnd;

	if ( psp->show == SW_SHOWNOACTIVATE ) return;

	for ( ; ; ){
		hParentWnd = GetParent(hWnd);
		if ( hParentWnd == NULL ) break;
		hWnd = hParentWnd;
	}
	if ( psp->show == SW_SHOWMINNOACTIVE ){
		ShowWindow(hWnd,SW_SHOWMINNOACTIVE);
		return;
	}
	if ( psp->show == SW_SHOWMAXIMIZED ){
		ShowWindow(hWnd,SW_SHOWMAXIMIZED);
	}
	if ( IsIconic(hWnd) || !IsWindowVisible(hWnd) ){
		SendMessage(hWnd,WM_SYSCOMMAND,SC_RESTORE,0xffff0000);
	}
	SetForegroundWindow(hWnd);
}

BOOL RegisterID(PPC_APPINFO *cinfo, PPCSTARTPARAM *psp, BOOL *usepath)
{
	int MultiRegMode, pane;
	BOOL Pair = FALSE;
								// 登録準備
	if ( (psp == NULL) || (psp->next == NULL) ){
		tstrcpy(cinfo->RegID, T(PPC_REGID));
		cinfo->RegSubIDNo = -1;
		MultiRegMode = PPXREGIST_NORMAL;
		pane = PSPONE_PANE_DEFAULT;
	}else{
		DWORD size;
		PSPONE *pspo;

		pspo = psp->next;
		size = PSPONE_size(pspo);
		tstrcpy(cinfo->RegID, pspo->id.RegID);
		cinfo->RegSubIDNo = pspo->id.SubID;

		tstrcpy(cinfo->path, pspo->path);
		MultiRegMode = pspo->id.RegMode;

		Pair = pspo->id.Pair;
		pane = pspo->combo.pane;
		cinfo->ChdirLock = pspo->combo.dirlock;
		psp->next = (PSPONE *)(char *)((char *)pspo + size); // 次へ
	}
	// ComboID B 以降の自動割当ては、必ず Zxxx 形式にする
	if ( cinfo->combo &&
		 (ComboID[2] > 'A') &&
		 (MultiRegMode == PPXREGIST_NORMAL) ){
		cinfo->RegID[2] = 'Z';
		cinfo->RegID[3] = '\0';
	}

	if ( psp != NULL ){			// 実行コマンドを確保(/k オプション)
		if ( IsTrue(psp->UseCmd) ){
			psp->UseCmd = FALSE;
			cinfo->FirstCommand = psp;
		}
								// 再利用処理(/r オプション)
		if ( IsTrue(psp->Reuse) ){
			HWND hWnd;

			if ( MultiRegMode == PPXREGIST_IDASSIGN ){
				if ( Pair && cinfo->RegID[2] ){
					cinfo->RegID[2] = (TCHAR)(((cinfo->RegID[2] - 1) ^ 1) + 1);
				}
				cinfo->RegNo = PPxRegist(PPXREGIST_DUMMYHWND,
						cinfo->RegID, PPXREGIST_IDASSIGN);
				if ( cinfo->RegNo >= 0 ){	// 新規割当てに成功
					goto newppc;
				}
				if ( cinfo->RegSubIDNo >= 0 ){
					MakeRegSubID(cinfo);
					hWnd = GetHwndFromIDCombo(cinfo->RegSubCID);
				}else{
					hWnd = PPxGetHWND(cinfo->RegID);
				}
			}else{
				hWnd = PPcGetWindow(0,
						!Pair ? CGETW_GETFOCUS : CGETW_GETFOCUSPAIR);
			}
			if ( pane != PSPONE_PANE_DEFAULT ){
				if ( (hWnd != NULL) && (pane != PSPONE_PANE_NEWPANE) ){
					if ( pane == PSPONE_PANE_PAIR ){
						pane = KC_GETSITEHWND_PAIR;
					}else{
						pane = pane - PSPONE_PANE_SETPANE + KC_GETSITEHWND_LEFTENUM;
					}
					hWnd = (HWND)SendMessage(hWnd,WM_PPXCOMMAND,
							KC_GETSITEHWND,(LPARAM)pane);
				}
			}

									// 起動済み PPc にパス等を通知
			if ( hWnd != NULL ){
				COPYDATASTRUCT copydata;

				SetWindowMinMax(hWnd,psp);
				if ( cinfo->path[0] != '\0' ){
					copydata.dwData = 0x200 + '=';
					copydata.cbData = TSTRSIZE(cinfo->path);
					copydata.lpData = cinfo->path;
					SendMessage(hWnd,WM_COPYDATA,0,(LPARAM)&copydata);
				}
				if ( cinfo->FirstCommand != NULL ){
					copydata.dwData = 0x100 + 'H';
					copydata.cbData = TSTRSIZE(psp->cmd);
					copydata.lpData = (PVOID)psp->cmd;
					SendMessage(hWnd,WM_COPYDATA,0,(LPARAM)&copydata);
					psp->AllocCmd = FALSE; // ここで使用したら自由に解放できる
					cinfo->FirstCommand = NULL; // 次回に影響しないように初期化
				}
				goto nextppc; // このPPcは起動しない / 次のPPcへ
			}
		}
	}
								// ID を確保
	cinfo->RegNo = PPxRegist(PPXREGIST_DUMMYHWND,cinfo->RegID,MultiRegMode);
	if ( (cinfo->RegNo < 0) && (cinfo->RegSubIDNo < 0) ){
		// alone の場合、1枚も開けないときはとりあえず１枚開くようにする
		if ( cinfo->combo && psp && psp->usealone && (Combo.hWnd == NULL) && (MultiRegMode == PPXREGIST_IDASSIGN) ){
			cinfo->RegNo = PPxRegist(PPXREGIST_DUMMYHWND,cinfo->RegID,PPXREGIST_NORMAL);
			if ( cinfo->RegNo < 0 ) goto nextppc;
		}else{
			goto nextppc;
		}
	}
newppc:		// 新規 PPc を起動
	if ( (cinfo->RegID[2] == 'Z') && cinfo->combo ){
		if ( cinfo->RegSubIDNo < 0 ) cinfo->RegSubIDNo = GetComboRegZID();
		MakeRegSubID(cinfo);
	}else{
		cinfo->RegSubCID[0] = 'C';
		cinfo->RegSubCID[1] = cinfo->RegID[2];
		// cinfo->RegSubCID[2] は 0 に初期化済み
	}

	if ( cinfo->path[0] == '\0' ){
		GetCurrentDirectory(VFPS,cinfo->path);
	}else{
		*usepath = TRUE;
	}
	return TRUE;

nextppc:	// 初期化に失敗した場合、次の PPc があれば処理する
	if ( (psp != NULL) && (psp->next != NULL) ){
		if ( psp->next->id.RegID[0] ){	// 次の初期化へ
			return RegisterID(cinfo,psp,usepath);
		}else{ // 終わり / cmd 用のメモリを確保してなければここで解放
			int ComboID;

			psp->next = NULL;
			ComboID = psp->ComboID;
			if ( psp->AllocCmd == FALSE ){ // cmd は静的なので PSPONE 解放
				ThFree(&psp->th);
			}
			// comboのfocusfix
			if ( X_combo ){
				// alone で開き足りないかをチェック
				if ( (ComboID > 'A') && (Combo.BaseCount < X_mpane.min) ){
					MorePPc(NULL,&cinfo->mws);
				}else{
					PostMessage(Combo.hWnd,WM_PPXCOMMAND,KCW_ready,(LPARAM)NULL);
				}
			}
		}
	}
	return FALSE;
}

int GetCustTableCID(const TCHAR *str,const TCHAR *sub,void *bin,size_t b_size)
{
	if ( NO_ERROR == GetCustTable(str,sub,bin,b_size) ) return 0;
	return GetCustTable(str,(sub[1] == '\0') ? DefCID + 1 : DefCID,bin,b_size);
}

HFONT GetControlFont(DWORD dpi,ControlFontStruct *cfs)
{
	if ( (cfs->hFont == NULL) || (dpi != cfs->FontDPI) ){
		LOGFONTWITHDPI cursfont;

		if ( cfs->hFont != NULL ) DeleteObject( cfs->hFont );
		cfs->FontDPI = dpi;
		GetPPxFont(PPXFONT_F_ctrl,dpi,&cursfont);
		cfs->hFont = CreateFontIndirect(&cursfont.font);
	}
	return cfs->hFont;
}

void USEFASTCALL HideScrollBar(PPC_APPINFO *cinfo)
{
	HWND hWnd = cinfo->info.hWnd;

	if ( GetWindowLongPtr(hWnd,GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL) ){
		SCROLLINFO sinfo;

		sinfo.cbSize = sizeof(sinfo);
		sinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		sinfo.nMin = 0;
		sinfo.nMax = 1;
		sinfo.nPage = 2;
		sinfo.nPos = 0;
		SetScrollInfo(hWnd,SB_HORZ,&sinfo,FALSE);
		SetScrollInfo(hWnd,SB_VERT,&sinfo,FALSE);
	}
}

void USEFASTCALL CreateScrollBar(PPC_APPINFO *cinfo)
{
	// ※ ScrollBarHV を SBS_HORZ / SBS_VERT として使用
	cinfo->hScrollTargetWnd = cinfo->hScrollBarWnd =
		CreateWindowEx(0,T("SCROLLBAR"),NilStr,WS_CHILD | cinfo->ScrollBarHV,
			0,0,10,10,cinfo->info.hWnd,(HMENU)IDW_SCROLLBAR,hInst,NULL);
	HideScrollBar(cinfo);
}

void InitGuiControl(PPC_APPINFO *cinfo)
{
	UINT ID = IDW_TOOLCOMMANDS;
	RECT box;

	if ( cinfo->combo ?
		  ((DWORD)X_combos[0] & CMBS_HEADER) : (cinfo->X_win & XWIN_HEADER) ){
		LoadCommonControls(ICC_LISTVIEW_CLASSES);
		cinfo->hHeaderWnd = CreateWindowEx(0,WC_HEADER,NilStr,
				(OSver.dwMajorVersion >= 6) ?
				WS_CHILD | WS_VISIBLE | CCS_NODIVIDER | HDS_BUTTONS | HDS_CHECKBOXES :
				WS_CHILD | WS_VISIBLE | CCS_NODIVIDER | HDS_BUTTONS,
				0,0,0,0,cinfo->info.hWnd,(HMENU)IDW_HEADER,hInst,NULL);
		if ( cinfo->hHeaderWnd != NULL ){
			HD_LAYOUT hdrl;
			RECT hdrbox;
			WINDOWPOS hdrwpos;

			SendMessage(cinfo->hHeaderWnd,WM_SETFONT,(WPARAM)GetControlFont(cinfo->FontDPI,&cinfo->cfs),0);
			ShowWindow(cinfo->hHeaderWnd,SW_SHOW);

			{
				const TCHAR **text;
				TCHAR **dest = hdrstrings;

				for ( text = hdrstrings_eng ; *text != NULL ; text++ ){
					*dest++ = (TCHAR *)MessageText(*text);
				}
			}

			hdrl.prc = &hdrbox;
			hdrl.pwpos = &hdrwpos;
			SendMessage(cinfo->hHeaderWnd,HDM_LAYOUT,0,(LPARAM)&hdrl);
			cinfo->HeaderHeight = hdrwpos.cy - 2;

			FixHeader(cinfo);
			SetWindowPos(cinfo->hHeaderWnd,NULL,
					cinfo->BoxEntries.left,
					cinfo->BoxEntries.top - cinfo->HeaderHeight,
					cinfo->BoxStatus.right - cinfo->BoxEntries.left,
					cinfo->HeaderHeight,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
	if ( cinfo->combo == 0 ){ // 非一体化時に設定する内容 ---------------------
		// ツールバー
		if ( cinfo->X_win & XWIN_TOOLBAR ){
			cinfo->hToolBarWnd = CreateToolBar(&cinfo->thGuiWork,cinfo->info.hWnd,&ID,T("B_cdef"),PPcPath,0);
			if ( cinfo->hToolBarWnd != NULL ){
				GetWindowRect(cinfo->hToolBarWnd,&box);
				cinfo->ToolbarHeight = box.bottom - box.top;
			}
		}
										// Dock
		DocksInit(&cinfo->docks,cinfo->info.hWnd,cinfo,cinfo->RegCID,cinfo->hBoxFont,cinfo->fontY,&cinfo->thGuiWork,&ID);

		if ( cinfo->docks.t.hWnd != NULL ){
			MoveWindow(cinfo->docks.t.hWnd,0,0,
					cinfo->wnd.Area.cx,cinfo->docks.t.client.bottom,TRUE);
		}
		if ( cinfo->docks.b.hWnd != NULL ){
			MoveWindow(cinfo->docks.b.hWnd,0,
					cinfo->wnd.Area.cy - cinfo->docks.b.client.bottom,
					cinfo->wnd.Area.cx,cinfo->docks.b.client.bottom,TRUE);
		}

	}

	if ( !(X_combos[1] & CMBS1_NOSLIMSCROLL) &&
#ifndef UNICODE
		 (OSver.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
#endif
		 (XC_page || (cinfo->docks.b.hWnd != NULL)) ){

		if ( cinfo->hScrollBarWnd == NULL ){
			CreateScrollBar(cinfo);
		}else{
			cinfo->hScrollTargetWnd = cinfo->hScrollBarWnd;
		}
	}else{
		if ( cinfo->hScrollBarWnd != NULL ){
			DestroyWindow(cinfo->hScrollBarWnd);
			cinfo->hScrollBarWnd = NULL;
		}
		cinfo->hScrollTargetWnd = cinfo->info.hWnd;
	}

	if ( (cinfo->hToolBarWnd != NULL) ||
//		 (cinfo->hTreeWnd != NULL) || // ←PPC_Tree で処理済み。
		 (cinfo->hHeaderWnd != NULL) ||
		 (cinfo->hScrollBarWnd != NULL) ||
		 (cinfo->docks.t.hWnd != NULL) ||
		 (cinfo->docks.b.hWnd != NULL) ){
		X_awhel = FALSE;	// メッセージ処理がループするので禁止中
		SetWindowLong(cinfo->info.hWnd,GWL_STYLE,
				GetWindowLongPtr(cinfo->info.hWnd,GWL_STYLE) | WS_CLIPCHILDREN);
	}

#if USEDELAYCURSOR || defined(USEDIRECTX)
	#if USEDELAYCURSOR || SHOWFRAMERATE
		SetTimer(cinfo->info.hWnd,TIMERID_DELAYCURSOR,TIMERRATE_DRAW,FloatProc);
	#endif
	if ( X_fles == 0 ){
	#ifndef USEDIRECTX
		X_fles = 1;
	#endif
		cinfo->FullDraw = 1;
	}
	#if USEDELAYCURSOR
		cinfo->freeCell = TRUE;
	#endif
#endif
}

void CloseGuiControl(PPC_APPINFO *cinfo)
{
	if ( cinfo->hToolBarWnd != NULL ){
		CloseToolBar(cinfo->hToolBarWnd);
		cinfo->hToolBarWnd = NULL;
	}
	if ( cinfo->hHeaderWnd != NULL ){
		DestroyWindow(cinfo->hHeaderWnd);
		cinfo->hHeaderWnd = NULL;
		cinfo->HeaderHeight = 0;
	}
	if ( cinfo->docks.t.hWnd != NULL ) DestroyWindow(cinfo->docks.t.hWnd);
	if ( cinfo->docks.b.hWnd != NULL ) DestroyWindow(cinfo->docks.b.hWnd);
	ThFree(&cinfo->thGuiWork);
}

#if 0
#define ToLightPart(c, lv) ((((c) + lv) < 0x100 ) ? ((c) += lv) : 0xff)
#define ToDrakPart(c, lv)  (((c) > lv) ? ((c) -= lv) : 0)
COLORREF ToLight(COLORREF color, DWORD lv)
{
	DWORD r,g,b;

	r = GetRValue(color);
	g = GetGValue(color);
	b = GetBValue(color);
	return RGB(ToLightPart(r,lv), ToLightPart(g,lv), ToLightPart(b,lv));
}

COLORREF ToDark(COLORREF color, DWORD lv)
{
	DWORD r,g,b;

	r = GetRValue(color);
	g = GetGValue(color);
	b = GetBValue(color);
	return RGB(ToDrakPart(r,lv), ToDrakPart(g,lv), ToDrakPart(b,lv));
}

void FixLightColor(void)
{
	DWORD lightmode;
	const TCHAR RegThemesPath[] = T("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
	const TCHAR RegLightName[] = T("AppsUseLightTheme");
	DWORD blight;

	if ( GetRegString(HKEY_CURRENT_USER, RegThemesPath, RegLightName, (TCHAR *)&lightmode, TSIZEOF(lightmode)) == FALSE ) return;

	// 0:dark 1:light
	blight = GetRValue(C_back) + GetGValue(C_back) + GetBValue(C_back);
	if ( (lightmode == 0) && (blight > (0xa0 * 3)) ){ // dark用に背景を暗く
	}else if ( (lightmode == 1) && (blight < (0x60 * 3)) ){ // light用に背景を明るく
		C_back = ToLight(C_back,0xc0);
		C_mes =  ToDark(C_mes,0xc0);
		C_info = ToDark(C_info,0xc0);
		C_entry[ECT_NORMAL] = ToDark(C_entry[ECT_NORMAL],0xc0);
	}
}
#endif

// カスタマイズ内容をまとめて取得 ---------------------------------------------
void PPcLoadCust(PPC_APPINFO *cinfo)
{
	int i,X_fatim = 0;
	DWORD work[3];

	GetCustData(T("X_log")	,&X_log,sizeof(X_log));
	GetCustData(T("X_IME")	,&X_IME,sizeof(X_IME));
	GetCustTableCID(T("X_win")	,cinfo->RegCID,
			&cinfo->X_win,sizeof(cinfo->X_win));
	GetCustTableCID(T("XC_tree"),cinfo->RegCID+1,
			&cinfo->XC_tree,sizeof(cinfo->XC_tree));
	if ( NO_ERROR != GetCustTableCID(T("XC_sort"),cinfo->RegCID+1,
			&cinfo->XC_sort,sizeof(cinfo->XC_sort)) ){
		cinfo->XC_sort.mode.block = -1;	// 0-3 をまとめて -1
		cinfo->XC_sort.atr = 0x1f;
		cinfo->XC_sort.option = NORM_IGNORECASE;
	}
	GetCustTableCID(T("XC_mask"),cinfo->RegCID+1,
			&cinfo->mask,sizeof(cinfo->mask));

	GetCustData(T("X_fatim")	,&X_fatim,sizeof(X_fatim));
	FuzzyCompareFileTime = X_fatim ? FuzzyCompareFileTime2: FuzzyCompareFileTime0;

	XC_mvUD.outw_type = OUTTYPE_COLUMNSCROLL;

	GetCustData(T("XC_mvUD")	,&XC_mvUD,sizeof(XC_mvUD));
	GetCustData(T("XC_mvLR")	,&XC_mvLR,sizeof(XC_mvLR));
	GetCustData(T("XC_mvPG")	,&XC_mvPG,sizeof(XC_mvPG));
	GetCustData(T("XC_mvSC")	,&XC_mvSC,sizeof(XC_mvSC));
	if ( NO_ERROR != GetCustData(T("XC_mvWH"),&XC_mvWH,sizeof(XC_mvWH)) ){
		XC_mvWH = XC_mvSC;
	}
	GetCustData(T("XC_msel")	,&XC_msel,sizeof(XC_msel));
	GetCustData(T("XC_exem")	,&XC_exem,sizeof(XC_exem));
	GetCustData(T("XC_nsbf")	,&XC_nsbf,sizeof(XC_nsbf));
	GetCustData(T("XC_page")	,&XC_page,sizeof(XC_page));
	GetCustData(T("XC_smar")	,&XC_smar,sizeof(XC_smar));
	GetCustData(T("XC_fwin")	,&XC_fwin,sizeof(XC_fwin));
	GetCustData(T("XC_rrt")		,&XC_rrt,sizeof(XC_rrt));

	cinfo->ScrollBarHV = (cinfo->X_win & XWIN_SWAPSCROLL) ? SB_VERT : SB_HORZ;
	if ( XC_page ){
		cinfo->ScrollBarHV ^= (SB_HORZ | SB_VERT);
		if ( XC_mvUD.outw_type <= OUTTYPE_PAGE ){
			XC_mvUD.outw_type = OUTTYPE_LINESCROLL;
		}
		if ( XC_mvSC.outw_type <= OUTTYPE_PAGE ){
			XC_mvSC.outw_type = OUTTYPE_LINESCROLL;
		}
		if ( XC_mvWH.outw_type <= OUTTYPE_PAGE ){
			XC_mvWH.outw_type = OUTTYPE_LINESCROLL;
		}
	}

	GetCustData(T("X_fles")		,&X_fles,sizeof(X_fles));
	GetCustData(T("X_alt")		,&X_alt,sizeof(X_alt));
	GetCustData(T("X_iacc")		,&X_iacc,sizeof(X_iacc));
	GetCustData(T("X_evoc")		,&X_evoc,sizeof(X_evoc));
	X_extl--;
	GetCustData(T("X_extl")		,&X_extl,sizeof(X_extl));
	X_extl++;
	if ( X_extl <= 0 ) X_extl = MAX_PATH;
	GetCustData(T("X_acr")		,&X_acr,sizeof X_acr);
	GetCustData(T("X_Slow")		,&X_Slow,sizeof X_Slow);
	GetCustData(T("X_wnam")		,&X_wnam,sizeof X_wnam);

	GetCustData(T("X_tray")		,&X_tray,sizeof(X_tray));

	work[0] = work[1] = 1;	// 「.」「..」を表示
	work[2] = 0;	// 強制表示はしない
	GetCustData (T("XC_tdir")	,&work,sizeof(DWORD) * 3);
	rdirmask = 0;

	if ( X_ChooseMode != CHOOSEMODE_NONE ){
		setflag(rdirmask,ECAX_FORCER); // choose時は「.」を強制表示
	}else{
		if ( !work[0] ) setflag(rdirmask,ECA_THIS); // 「.」は非表示
		if ( work[2] ) setflag(rdirmask,B31); // 強制表示
	}
	if ( !work[1] ) setflag(rdirmask,ECA_PARENT); // 「..」は非表示

	GetCustData(T("XC_dpmk")	,&XC_dpmk,sizeof(XC_dpmk));
	GetCustData(T("XC_sdir")	,&XC_sdir,sizeof(XC_sdir));
	GetCustData(T("XC_awid")	,&XC_awid,sizeof(XC_awid));
	GetCustData(T("XC_emov")	,&XC_emov,sizeof(XC_emov));
	GetCustData(T("XC_gmod")	,&XC_gmod,sizeof(XC_gmod));

	GetCustData(T("XC_alst")	,&XC_alst,sizeof(XC_alst));
	GetCustData(T("XC_alac")	,&XC_alac,sizeof(XC_alac));
	GetCustData(T("XC_acsr")	,&XC_acsr,sizeof(XC_acsr));

	GetCustData(T("X_svsz")		,&X_svsz,sizeof(X_svsz));

	GetCustData(T("XC_Gest")	,&XC_Gest,sizeof(XC_Gest));

	GetCustData(T("XC_szcm")	,&XC_szcm,sizeof(XC_szcm));

	GetCustData(T("XC_pmsk")	,&XC_pmsk,sizeof(XC_pmsk));
	GetCustData(T("XC_limc")	,&XC_limc,sizeof(XC_limc));
	GetCustData(T("XC_fexc")	,&XC_fexc,sizeof(XC_fexc));
	GetCustData(T("XC_cdc")		,&XC_cdc,sizeof(XC_cdc));

	FreeCFMT(&cinfo->stat);
	LoadCFMT(&cinfo->stat,T("XC_stat"),NULL,&CFMT_stat);
	if ( !cinfo->stat.fmt[0] ){
		cinfo->stat.attr = DE_ATTR_STATIC | DE_ATTR_MARK | DE_ATTR_DIR;
	}

	FreeCFMT(&cinfo->inf1);
	LoadCFMT(&cinfo->inf1,T("XC_inf1"),NULL,&CFMT_inf1);

	FreeCFMT(&cinfo->inf2);
	LoadCFMT(&cinfo->inf2,T("XC_inf2"),NULL,&CFMT_inf2);

	if ( cinfo->FixcelF == FALSE ){
		FreeCFMT(&cinfo->celF);
		LoadCFMT(&cinfo->celF,T("XC_celF"),
				!IsExistCustTable(T("XC_celF"),cinfo->RegCID + 1) ?
					DefCID + 1 : cinfo->RegCID + 1,&CFMT_celF);
	}

//--------------------------
	AutoColor	(T("C_back"),	&C_back, COLOR_WINDOW);
	GetCustData (T("C_line"),	C_line, sizeof(C_line));
	AutoColor	(T("C_mes"),	&C_mes, COLOR_WINDOWTEXT);
	AutoColor	(T("C_info"),	&C_info, COLOR_WINDOWTEXT);
	GetCustData	(T("C_res"),	&C_res, sizeof(C_res));
	if ( C_res[0] == C_AUTO) C_res[0] = GetSysColor(COLOR_HIGHLIGHTTEXT);
	if ( C_res[1] == C_AUTO) C_res[1] = GetSysColor(COLOR_HIGHLIGHT);

	GetCustData (T("C_entry"),	C_entry, sizeof(C_entry));
	if ( C_entry[ECT_NORMAL] == C_AUTO ) C_entry[ECT_NORMAL] = GetSysColor(COLOR_WINDOWTEXT);
	GetCustData (T("C_eInfo"), C_eInfo, sizeof(C_eInfo));
	if ( C_eInfo[ECS_MARK] == C_AUTO ) C_eInfo[ECS_MARK] = GetSysColor(COLOR_HIGHLIGHT);
	if ( C_eInfo[ECS_SELECT] == C_AUTO ) C_eInfo[ECS_SELECT] = GetSysColor(COLOR_HIGHLIGHT);

	C_defextC = C_AUTO;
	GetCustTable(T("C_ext"), T("*"), &C_defextC, sizeof(C_defextC));

//	FixLightColor();
	cinfo->BackColor = C_back;
	for ( i = ECS_MESSAGE ; i < ECS_NOFOCUS ; i++ ){
		if ( C_eInfo[i] == C_AUTO ) C_eInfo[i] = C_back;
	}
	if ( cinfo->stat.fc == C_AUTO ) cinfo->stat.fc = C_info;
	if ( cinfo->stat.bc == C_AUTO ) cinfo->stat.bc = C_back;
//--------------------------

	GetCustData(T("XC_ifix"),&XC_ifix,sizeof(XC_ifix));
	GetCustData(T("XC_ito"),&XC_ito,sizeof(XC_ito));
	GetCustData(T("X_ardir"),&X_ardir,sizeof(X_ardir));
	X_ardir[1] *= MB;
	if ( X_ardir[1] < MB ) X_ardir[1] = 512 * KB;

	GetCustData(T("X_askp"),&X_askp,sizeof(X_askp));
	GetCustData(T("XC_ulh"),&XC_ulh,sizeof(XC_ulh));
	GetCustData(T("X_stip"),&X_stip,sizeof(X_stip));
	cinfo->Tip.mode = X_stip[TIP_LONG_TIME];

	GetCustData(T("X_inag"),&cinfo->X_inag,sizeof(cinfo->X_inag));
	GetCustData(T("X_poshl"),&X_poshl,sizeof(X_poshl));
	GetCustData(T("X_nfmt"),&X_nfmt,sizeof(X_nfmt));

	GetCustData(T("X_pmc"),&X_pmc,sizeof(X_pmc));
	if ( X_pmc[0] >= 0 ){
		if ( X_pmc[0] == 0 ){
			TouchMode = 0;
		}else{
			PPcEnterTabletMode(cinfo);
		}
	}

	if ( OSver.dwMajorVersion >= 6 ){
		GetCustData(T("X_dss"),&X_dss,sizeof(X_dss));
	}

	GetCustData(T("XC_drag"),&XC_drag,sizeof(XC_drag));

	X_dicn[0] = '\0';
	GetCustData(T("X_dicn"),X_dicn,sizeof(X_dicn));
	if ( X_dicn[0] != '\0' ){
		VFSFixPath(NULL,X_dicn,PPcPath,VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}

	cinfo->UseLoadEvent = IsExistCustTable(StrKC_main,T("LOADEVENT"));
	cinfo->UseSelectEvent = IsExistCustTable(StrKC_main,T("SELECTEVENT"));
	cinfo->UseActiveEvent = IsExistCustTable(StrKC_main,T("ACTIVEEVENT"));
	if ( !(cinfo->swin & SWIN_BUSY) ){
		IOX_win(cinfo,FALSE);
		resetflag(cinfo->swin,SWIN_BUSY);
	}
	DirString = MessageText(DefDirString);
	DirStringLength = tstrlen32(DirString);
	StrBusy = MessageText(DefStrBusy);
	StrBusyLength = tstrlen(StrBusy);

	LoadHiddenMenu(&cinfo->HiddenMenu,T("HM_ppc"),hProcessHeap,C_mes);

	X_lddm[0] = -1;

	if ( cinfo->info.hWnd != NULL ) CloseGuiControl(cinfo);
}

void SaveTreeSettings(HWND hTreeWnd,const TCHAR *custid,DWORD mode,DWORD width)
{
	PPCTREESETTINGS pts;

	pts.mode = mode;
	pts.width = width;
	pts.name[0] = '\0';
	if ( hTreeWnd != NULL ){
		SendMessage(hTreeWnd,VTM_GETSETTINGS,0,(LPARAM)&pts.name);
	}
	SetCustTable(T("XC_tree"),custid,&pts,sizeof(DWORD) * 2 + TSTRSIZE(pts.name));
}

// カスタマイズ内容をまとめて設定 ---------------------------------------------
void PPcSaveCust(PPC_APPINFO *cinfo)
{
	TCHAR buf[VFPS];

	if ( Isalpha(cinfo->RegCID[1]) == FALSE ){
		GetWindowText(cinfo->info.hWnd,buf,VFPS);
		XMessage(cinfo->info.hWnd,NULL,XM_FaERRld,T("RegCID error : %s"),buf);
		return;
	}
	SavePPcDir(cinfo,FALSE);

	SaveTreeSettings(cinfo->hTreeWnd,cinfo->RegCID + 1,
			cinfo->XC_tree.mode,cinfo->XC_tree.width);

	SetCustTable(T("_Path"),cinfo->RegSubCID,cinfo->path,TSTRSIZE(cinfo->path));
	if ( cinfo->combo == 0 ){ // Combo時は、Comboで保存する
		SetCustTable(Str_WinPos,
				cinfo->RegSubCID, &cinfo->WinPos, sizeof(cinfo->WinPos));
	}
	IOX_win(cinfo,TRUE);
}

BOOL CheckReady(PPC_APPINFO *cinfo)
{
	TCHAR id[2] = T("A");
	int x;

	if ( cinfo->swin & SWIN_BUSY ) return FALSE;
	if ( cinfo->combo ) return TRUE;

	id[0] += (TCHAR)((*(cinfo->RegCID + 1) - (TCHAR)'A') & (TCHAR)0x7e);
	if ( NO_ERROR != GetCustTable(T("XC_swin"),id,&x,sizeof(x)) ) return FALSE;
	return ( x & SWIN_BUSY ) ? FALSE : TRUE;
}

// 初期化(全PPc共用) ----------------------------------------------------------
void InitPPcGlobal(void)
{
	WNDCLASS wcClass;

#if NODLL
	InitCommonDll((HINSTANCE)hInst);
#endif
	PPxCommonExtCommand(K_CHECKUPDATE,0);
	PPxCommonExtCommand(K_SETFAULTOPTIONINFO,(WPARAM)&FaultInfoPtr);
	SetErrorMode(SEM_FAILCRITICALERRORS); // 致命的エラーを取得可能にする

	OSver.dwOSVersionInfoSize = sizeof(OSver);
	GetVersionEx(&OSver);
	hProcessHeap = GetProcessHeap();
	WM_PPXCOMMAND = RegisterWindowMessageA(PPXCOMMAND_WM);
	WM_TaskbarButtonCreated = RegisterWindowMessage(TaskbarButtonCreatedReg);
	FixCharlengthTable(T_CHRTYPE);
	DefDriveList = GetLogicalDrives();

	Combo.hWnd = Combo.Report.hWnd = NULL;

	GetModuleFileName(hInst,PPcPath,VFPS);
	*VFSFindLastEntry(PPcPath) = '\0';

	InitializeCriticalSection(&FindFirstAsyncSection);
	InitializeCriticalSection(&SHGetFileInfoSection);
											// API 取得
#ifndef UNICODE
	GETDLLPROCT(GetModuleHandle(StrKernel32DLL),GetDiskFreeSpaceEx);
#endif
	if ( OSver.dwMajorVersion >= 6 ){
		hDwmapi = LoadLibrary(DwmapiName);

		if ( hDwmapi != NULL ){
			GETDLLPROC(hDwmapi,DwmIsCompositionEnabled);
			DDwmIsCompositionEnabled(&UseOffScreen);
		}
	}
										// ウインドウクラスを定義する
	wcClass.style			= CS_DBLCLKS;
	wcClass.lpfnWndProc		= WndProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= hInst;
	wcClass.hIcon			= LoadIcon(hInst,MAKEINTRESOURCE(Ic_PPC));
	wcClass.hCursor			= LoadCursor(NULL,IDC_ARROW);
	wcClass.hbrBackground	= NULL;
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= PPcClassStr;
	RegisterClass(&wcClass);

#ifndef WINEGCC
	GetCustData(T("X_sps"),&X_sps,sizeof(X_sps));
#else
	X_sps = 1;
#endif
	GetCustData(T("X_combo"),&X_combo,sizeof(X_combo));
	LoadCombos();

#ifdef USEDIRECTX
	setflag(X_combos[0],CMBS_THREAD);
#endif
}

void USEFASTCALL SetLinebrush(PPC_APPINFO *cinfo,COLORREF color)
{
	if ( cinfo->linebrush != NULL ) DeleteObject(cinfo->linebrush);
	cinfo->linebrush = CreateSolidBrush(color);
}

// 初期化(個別初期化) ---------------------------------------------------------
void InitPPcWindow(PPC_APPINFO *cinfo,BOOL usepath)
{
	HWND hWnd;
	POINT windowsize;
	BYTE showmode;
	HMENU hMenu = NULL;
	// PPC_APPINFO の初期化。0 fill 済みであることが前提
									// PPc 情報
	cinfo->info.Function = (PPXAPPINFOFUNCTION)PPcGetIInfo;
	cinfo->info.Name = InfoName;
	cinfo->info.RegID = cinfo->RegID;
	cinfo->info.hWnd = NULL;
	cinfo->Ref = 1;
//	cinfo->RegNo = LoadParam で定義済み
	cinfo->RegCID[0] = 'C';
	cinfo->RegCID[1] = cinfo->RegID[2];
//	cinfo->RegCID[2] = '\0';
//	cinfo->RegID = LoadParam で定義済み
//	cinfo->hHeaderWnd = NULL;
									// 同期
	InitializeCriticalSection(&cinfo->edit.section);
//	cinfo->edit.ref = 0;

	cinfo->SubT_cmd = CreateEvent(NULL,TRUE,FALSE,NULL);
//	cinfo->SubT_dir = NULL;
//	cinfo->SubTCmdFlags = 0;

									// ディレクトリ情報
//	cinfo->RealPath[0] = '\0';
//	cinfo->ArcPathMask[0] = '\0';
//	cinfo->OrgPath[0] = '\0';
//	cinfo->Caption[0] = '\0';
	ThInit(&cinfo->PathTrackingList);

									// Cell の実体
//	cinfo->e.CELLDATA.s = 0;
	TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * 2);
//	cinfo->e.INDEXDATA.s = 0;
	TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * 2);
	SetDummyCell(&CELdata(0),StrLoading);
	CELt(0) = 0;
	ThInit(&cinfo->EntryComments);

//	cinfo->e.cellN = 0;
	cinfo->e.cellIMax = cinfo->e.cellDataMax = 1;
//	cinfo->e.cellStack = 0;

	cinfo->e.markTop = ENDMARK_ID;
	cinfo->e.markLast = ENDMARK_ID;
									// Window
	cinfo->X_textmag = 100;

	cinfo->fontX = cinfo->fontY = 1;
									// 表示領域用RECT
	cinfo->cel.VArea.cx = cinfo->cel.Area.cx = 1;
	cinfo->cel.VArea.cy = cinfo->cel.Area.cy = 1;

									// Pop Message
//	cinfo->PopMsgTimer = 0;
//	cinfo->PopMsgFlag = 0;

									// 内部動作
	cinfo->oldmousemvpos.x = -1;
//	cinfo->multicharkey = 0;

//	cinfo->usereentry = 0;
//	cinfo->sizereentry = 0;
//	cinfo->oldU = 0;

//	cinfo->dds.hTreeWnd = NULL;

	cinfo->oldspos.nPage = (UINT)-1;
//	cinfo->UnpackFix = FALSE;
//	cinfo->ChdirLock = FALSE;
//	cinfo->PrevCommand = 0;

//	cinfo->NormalViewFlag = 0;
//	cinfo->SyncViewFlag = 0;
//	cinfo->hInfoIcon = NULL;
//	cinfo->InfoIcon_DirtyCache = FALSE;

	cinfo->MousePush = MOUSE_NONE;
//	cinfo->KeyHookEntry = NULL;
//	cinfo->IncSearchMode = FALSE;

	cinfo->NowJoint = TRUE;
//	cinfo->hSyncInfoWnd = NULL;
//	cinfo->hTipWnd = NULL;
//	cinfo->OldIconsPath[0] = '\0';
//	cinfo->ModifyComment = FALSE;

//	cinfo->KeyChar = 0;
//	cinfo->MouseUpFG = 0;
//	cinfo->ddtimer_id = 0;

//	cinfo->HiddenMenu.data = NULL;
//	cinfo->PushArea = 0;
//	cinfo->swin = 0;

//	cinfo->mask.atr = 0;
//	cinfo->mask.file[0] = '\0';

//	cinfo->Pws.cinfo = NULL;	// その他の項目の初期化は後回し

//	cinfo->FDirWrite = FDW_NORMAL;
//	cinfo->CLIPDATAS[0] = NULL;
//	cinfo->CLIPDATAS[1] = NULL;
//	cinfo->CLIPDATAS[2] = NULL;

	cinfo->X_win = XWIN_MENUBAR;
//	cinfo->hTreeWnd = NULL;
//	cinfo->TreeX = 0;
	cinfo->CellHashType = CELLHASH_NONE;
	cinfo->e.cellPoint = -1;
	cinfo->Mpos = -1;

//	cinfo->XC_tree.mode = FALSE;
//	cinfo->XC_tree.width = 0;
//	cinfo->hEntryIcons = NULL;

	cinfo->BackColor = cinfo->CursorColor = C_AUTO;

//	cinfo->RequestVolumeLabel = FALSE;
//	cinfo->VolumeLabel[0] = '\0';
//	cinfo->DelayMenus = NULL;

	cinfo->FontDPI = DEFAULT_WIN_DPI;

	ThInit(&cinfo->thGuiWork);
	ThInit(&cinfo->StringVariable);
	ThInit(&cinfo->ColumnData);

	PPxInitMouseButton(&cinfo->MouseStat);

								// カスタマイズ開始 ---------------------------
										// 初期化が完了するまでFixを禁止
	IOX_win(cinfo,FALSE);	// cinfo->swin 読み込み開始
	if ( cinfo->combo ){ // combo 時は joint を無効にする
		cinfo->swin = 0;
	}
	setflag(cinfo->swin,SWIN_BUSY);
	if ( IsTrue(usepath) && (cinfo->swin & SWIN_WBOOT) ){
		if ( cinfo->RegCID[1] & B0 ){ // A,C,E...
			// パス指定されているなら、A,C,E... にフォーカスを固定する
			resetflag(cinfo->swin,SWIN_BFOCUES);
		}
	}
	IOX_win(cinfo,TRUE);

	showmode = cinfo->WinPos.show;

	// 起動時のウィンドウサイズを決定
	if ( NO_ERROR == GetCustTable(Str_WinPos,cinfo->RegSubCID,
			&cinfo->WinPos,sizeof(cinfo->WinPos)) ){
		windowsize.x = cinfo->WinPos.pos.right - cinfo->WinPos.pos.left;
		windowsize.y = cinfo->WinPos.pos.bottom - cinfo->WinPos.pos.top;
	}else{
		cinfo->WinPos.pos.left = CW_USEDEFAULT;
		cinfo->WinPos.pos.top = CW_USEDEFAULT;
		windowsize.x = cinfo->WinPos.pos.right = 640;	// VGAに収まるサイズ
		windowsize.y = cinfo->WinPos.pos.bottom = 400;
		if ( (cinfo->combo != 0) || (cinfo->swin & SWIN_WBOOT) ){
			windowsize.x /= 2;
		}
	}
	if ( (showmode == SW_SHOWDEFAULT) || (showmode == SW_SHOWNORMAL) ){
		// 以前の値を利用
		if ( (cinfo->WinPos.show == SW_SHOWMINIMIZED) ||
			 (cinfo->WinPos.show == SW_HIDE) ){
			showmode = SW_SHOWNORMAL; // 最小化の時は通常に戻す
		}else{
			showmode = cinfo->WinPos.show; // 利用可能
		}
	}

	if ( usepath == FALSE ){
		GetCustTable(T("_Path"),cinfo->RegSubCID,cinfo->path,sizeof(cinfo->path));
	}
	PPcLoadCust(cinfo);
	if ( cinfo->X_win & XWIN_HIDETASK ){
		int testtime = 200;

		cinfo->hTrayWnd = PPxGetHWND(T(PPTRAY_REGID) T("A"));
		if ( cinfo->hTrayWnd == NULL ) ComExec(NULL,T(PPTRAYEXE),PPcPath);
		while(  (cinfo->hTrayWnd == NULL) || (cinfo->hTrayWnd == BADHWND) ){
			Sleep(50);
			cinfo->hTrayWnd = PPxGetHWND(T(PPTRAY_REGID) T("A"));
			if ( --testtime == 0 ) break;
		}
		// PPtray から窓一括管理用hWndを取得
		cinfo->hTrayWnd =
				(HWND)SendMessage(cinfo->hTrayWnd,WM_PPXCOMMAND,KRN_getcwnd,0);
	}
	if ( cinfo->X_inag ){
		cinfo->X_inag = INAG_UNFOCUS;
		cinfo->C_BackBrush = CreateSolidBrush(GetGrayColorB(C_back));
	}else{
		cinfo->C_BackBrush = CreateSolidBrush(C_back);
	}
	SetLinebrush(cinfo,LINE_NORMAL);
	if ( X_ChooseMode != CHOOSEMODE_NONE ) cinfo->combo = 0;
										// ウィンドウを作成 -------------------
	{
		DWORD style,exstyle;

		if ( cinfo->combo ){
			exstyle = WS_EX_TOOLWINDOW;
			style = WS_CHILD | WS_SYSMENU | WS_MINIMIZEBOX;
		}else{
			InitDynamicMenu(&cinfo->DynamicMenu,T("MC_menu"),ppcbar);
			exstyle = 0;
			style = (cinfo->X_win & XWIN_NOTITLE) ?
					WS_NOTITLEOVERLAPPED : WS_OVERLAPPEDWINDOW;
			if ( cinfo->X_win & XWIN_MENUBAR ){
				hMenu = cinfo->DynamicMenu.hMenuBarMenu;
			}
		}
		hWnd = cinfo->info.hWnd = CreateWindowEx(exstyle, PPcClassStr, NilStr,
				style, cinfo->WinPos.pos.left, cinfo->WinPos.pos.top,
				windowsize.x, windowsize.y,
				(cinfo->hComboWnd != NULL) ? Combo.Panes.hWnd : cinfo->hTrayWnd,
				hMenu, hInst, cinfo);
	}
	if ( cinfo->combo == 0 ) InitSystemDynamicMenu(&cinfo->DynamicMenu,hWnd);
	CreateDxDraw(&cinfo->DxDraw,hWnd);

	cinfo->FontDPI = PPxCommonExtCommand(K_GETDISPDPI,(WPARAM)hWnd);
	InitFont(cinfo);

//	cinfo->bg.hOffScreenDC = NULL;
//	cinfo->bg.X_WPbmp.DIB = NULL;
	LoadWallpaper(&cinfo->bg,hWnd,cinfo->RegCID);
	cinfo->FullDraw = X_fles | cinfo->bg.X_WallpaperType;
	CloseGuiControl(cinfo);
	ChangeSizeDxDraw(cinfo->DxDraw,C_back);
	InitGuiControl(cinfo);
	cinfo->WinPos.show = showmode; // InitGuiControl で値が変わる可能性がある
										// runas 状態のチェック -------------
	if ( runasstate == NULL ) runasstate = CheckRunAs();
	if ( runasstate != NULL ){
		wsprintf(cinfo->UserInfo,T("PPC[%s](%s)"),cinfo->RegSubCID + 1,runasstate);
	}else{
		wsprintf(cinfo->UserInfo,T("PPC[%s]"),cinfo->RegSubCID + 1);
	}
	SetCurrentDirectory(PPcPath); // カレントディレクトリをPPC.EXEの場所に移動
	VFSOn(VFS_DIRECTORY);
}

HFONT CreateMesFont(int mag,DWORD FontDPI)
{
	LOGFONTWITHDPI cursfont;

	GetPPxFont(PPXFONT_F_mes,FontDPI,&cursfont);

	if ( mag ){
		cursfont.font.lfHeight = (cursfont.font.lfHeight * mag) / 100;
		cursfont.font.lfWidth  = (cursfont.font.lfWidth  * mag) / 100;
	}
	return CreateFontIndirect(&cursfont.font);
}

// フォントの設定 -------------------------------------------------------------
void InitFont(PPC_APPINFO *cinfo)
{
	HDC hDC;
	HGDIOBJ hOldFont;	//一時保存用
	TEXTMETRIC tm;

	cinfo->hBoxFont = CreateMesFont(cinfo->X_textmag,cinfo->FontDPI);
										// ディスプレイコンテキストを得る
	hDC = GetDC(cinfo->info.hWnd);
						// 現在のディスプレイコンテキストにフォントを設定する
	hOldFont = SelectObject(hDC,cinfo->hBoxFont);
										// フォント情報を入手
	UsePFont = GetAndFixTextMetrics(hDC,&tm);
	cinfo->fontX = tm.tmAveCharWidth;

	cinfo->X_lspc = 0;
	GetCustData(T("X_lspc"),&cinfo->X_lspc,sizeof(cinfo->X_lspc));
	cinfo->X_lspcOrg = cinfo->X_lspc;
	cinfo->fontY = tm.tmHeight + cinfo->X_lspc;
	if ( cinfo->fontY <= 0 ) cinfo->fontY = 1;

	SelectObject(hDC,hOldFont);
	ReleaseDC(cinfo->info.hWnd,hDC); // ディスプレイコンテキストの解放

	#if DRAWMODE != DRAWMODE_GDI
	{
		int w = SetFontDxDraw(cinfo->DxDraw,cinfo->hBoxFont,0);

		if ( w > cinfo->fontX ) cinfo->fontX = w;
	}
	#endif

	if ( XC_ifix != 0 ){
		cinfo->XC_ifix_size.cx = cinfo->XC_ifix_size.cy = XC_ifix * cinfo->X_textmag / 100;
	}else{
		int infoheight = cinfo->fontY * 2;

		cinfo->XC_ifix_size.cx = cinfo->XC_ifix_size.cy = 32;
		if ( 32 > infoheight ){ // 32 より低い時は縦に潰す
			cinfo->XC_ifix_size.cy = infoheight;
		}else if ( 40 <= infoheight ){ // 40 以上なら合わせる
			cinfo->XC_ifix_size.cx = cinfo->XC_ifix_size.cy = infoheight;
		}
	}
	if ( cinfo->iconR != 0 ) cinfo->iconR = cinfo->XC_ifix_size.cx;
}
//------------------------------------- 終了処理
void PreClosePPc(PPC_APPINFO *cinfo)
{
	if ( IsTrue(cinfo->mws.DestroryRequest) ) return;
	cinfo->mws.DestroryRequest = TRUE;
	ExecKeyCommand(&PPcExecKey,&cinfo->info,K_E_CLOSE);

	if ( cinfo->combo ){ // 一体化の親に通知
		SendMessage(Combo.hWnd,WM_PPCOMBO_PRECLOSE,0,(LPARAM)cinfo->info.hWnd);
	}

	PPxRegist(NULL,cinfo->RegID,PPXREGIST_FREE);
	PPcSaveCust(cinfo);

	FreeAccServer(cinfo);
	dd_close(cinfo);
	if ( cinfo->X_win & XWIN_HIDETASK ){
		if ( cinfo->hTrayWnd != NULL ){
			PostMessage(cinfo->hTrayWnd,WM_PPXCOMMAND,KRN_freecwnd,0);
		}
	}
						// サブスレッドを停止
	setflag(cinfo->SubTCmdFlags,SUBT_EXIT);
	SetEvent(cinfo->SubT_cmd);
		// ここでスレッドを切り替えて、サブスレッドが進行するように
		// ※ FixClosedPPc で待ってもサブスレッドが実行されない...
	WaitForSingleObject(cinfo->hSubThread,50);
}

void ClosePPc(PPC_APPINFO *cinfo)
{
	if ( IsTrue(cinfo->mws.NowClose) ){ // 既に ClosePPc を実行済み?
		if ( cinfo->mws.NowClose == 2 ) RequestDestroyFlag = 1;
		return;
	}
	cinfo->mws.NowClose = 1; // Close処理開始

	PreClosePPc(cinfo);

	// 受付を終了	※Combo window の時は WM_DESTROY が２回来る為対処
	if ( SetWindowLongPtr(cinfo->info.hWnd,GWLP_USERDATA,0) != 0 ){
		cinfo->Ref--;
	}

//	cinfo->edit.ref++;	// これ以上のセル編集を禁止
	// 異常動作防止のためのデータ設定
	cinfo->e.cellIMax = cinfo->e.cellDataMax = 1;
	CEL(0).state = ECS_MESSAGE;
	CEL(0).mark_fw = NO_MARK_ID;
	cinfo->e.markTop = ENDMARK_ID;

	setflag(cinfo->swin,SWIN_BUSY);

//	DestroyWindow(cinfo->hTipWnd); // 親指定してある

	if ( IsTrue(cinfo->ModifyComment) ){
		WriteComment(cinfo,cinfo->CommentFile);
	}
	if ( hPropWnd != NULL ) DestroyWindow(hPropWnd);
	CloseGuiControl(cinfo);

	UnloadWallpaper(&cinfo->bg);
	ThFree(&cinfo->PathTrackingList);
	ThFree(&cinfo->EntryComments);
	ThFree(&cinfo->ColumnData);
	FreeDynamicMenu(&cinfo->DynamicMenu);

	FreeInfoIconCache(cinfo);
	CloseDxDraw(&cinfo->DxDraw);

	if ( cinfo->HiddenMenu.data ) PPcHeapFree(cinfo->HiddenMenu.data);
	DeleteObject(cinfo->C_BackBrush);
	DeleteObject(cinfo->hBoxFont);
	DeleteObject(cinfo->linebrush);
	CloseAnySizeIcon(&DirIcon);
	CloseAnySizeIcon(&UnknownIcon);
	if ( cinfo->cfs.hFont != NULL ) DeleteObject(cinfo->cfs.hFont);
	VFSOff();
	FreeOffScreen(&cinfo->bg);

	FreeCFMT(&cinfo->stat);
	FreeCFMT(&cinfo->inf1);
	FreeCFMT(&cinfo->inf2);
	FreeCFMT(&cinfo->celF);

	if ( !(cinfo->X_win & XWIN_MENUBAR) ){
		DestroyMenu(cinfo->DynamicMenu.hMenuBarMenu);
	}
	cinfo->mws.NowClose = 2; // メインループに後始末を依頼
}
