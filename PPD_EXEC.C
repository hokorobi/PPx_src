/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library			外部プロセス起動/PPx間通信
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#ifdef WINEGCC
#include <unistd.h>
#endif
#pragma hdrstop

int GetExecTypeFileOnly(TCHAR *name);

#define DEFAULTWAITPPBTIME 3
#define NODIALOGTIME 3000

const TCHAR ComSpecStr[] = T("ComSpec");

struct PPBWAITSTRUCT {
	HWND hDlg;
	int user;
	DWORD starttick;
	DWORD waittick;
	DWORD flags;
};

#ifdef __GNUC__
#undef  SEE_MASK_UNICODE
#define SEE_MASK_UNICODE 0x4000 // 定義ミスがある
#endif

#define ET_PATH B0	// ディレクトリ指定あり
#define ET_EXT  B1	// 拡張子指定有り

const TCHAR PathextStr[] = T("PATHEXT");
const TCHAR AppPathKey[] =
		T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s");
#define MAXSTORE 8 // 実行ファイルCRCの保存数

typedef struct {
	PPXAPPINFO info;
	const TCHAR *name;
} CHECKEXEBININFO;
const TCHAR is1[] = T("Execute check");
const TCHAR is2[] = T("%s is new Executive file.\n\n%s\n\nExecute this file?");

// 指定時間以上経過すると表示するボックス -------------------------------------
INT_PTR CALLBACK WaitPPBDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)lParam);
			LocalizeDialogText(hDlg,IDD_WAITPPB);
			return FALSE;

		case WM_CLOSE:
			*(int *)GetWindowLongPtr(hDlg,DWLP_USER) = IDCANCEL;
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_WAITPPB_NOPPB:
				case IDB_WAITPPB_NEWPPB:
					*(int *)GetWindowLongPtr(hDlg,DWLP_USER) = LOWORD(wParam);
					break;
				case IDCANCEL:
					WaitPPBDlgBox(hDlg,WM_CLOSE,0,0);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	外部プロセスを起動する
-----------------------------------------------------------------------------*/
PPXDLL BOOL PPXAPI ComExec(HWND hOwner,const TCHAR *line,const TCHAR *path)
{
	return ComExecEx(hOwner,line,path,NULL,0,NULL);
}

int AllocPPb(HWND hOwner,const TCHAR *path,int flags)
{
	HANDLE hE;
	TCHAR buf[200];
	int ID;

	wsprintf(buf,T(PPBBOOTSYNC) T("%x"),hOwner);
	hE = CreateEvent(NULL,TRUE,FALSE,buf);
	wsprintf(buf,T("-IR%u"),hOwner);
	if ( BootPPB(path,buf,flags) ){	// PPb の起動に失敗
		CloseHandle(hE);
		return -1;
	}else{							// PPb を割り当てるまで待つ
		int loopcnt = (15*1000) / 500;

		while ( WaitForSingleObject(hE,500) != WAIT_OBJECT_0 ){
			MSG msg;

			while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
				if ( msg.message == WM_QUIT ) break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			loopcnt--;
			if ( loopcnt ) continue;

			CloseHandle(hE);
			xmessage(XM_GrERRld,T("Use PPB error(boot timeout)"));
			return -1;
		}
		CloseHandle(hE);
		ID = UsePPb(hOwner);
		if ( ID == -1 ) xmessage(XM_GrERRld,T("Use PPB error(share fault)"));
		return ID;
	}
}
/*-----------------------------------------------------------------------------
	外部プロセスを起動する
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI GetExecuteErrorReason(const TCHAR *filename,TCHAR *reason)
{
	ERRORCODE errcode;

	errcode = GetLastError();
	if ( errcode == ERROR_BAD_EXE_FORMAT ){
		VFSFILETYPE type;

		type.flags = VFSFT_TYPETEXT;
		errcode = VFSGetFileType(filename,NULL,0,&type);
		if ( errcode != NO_ERROR ){
			PPErrorMsg(reason,errcode);
		}else{
			wsprintf(reason,MessageText(MES_EEFT),type.typetext);
		}
	}else{
		PPErrorMsg(reason,errcode);
	}
	SetLastError(errcode);
	return errcode;
}

void PPbLoadWaitTime(HWND hOwner,struct PPBWAITSTRUCT *pws)
{
	MSG msg;

	if ( pws->user == IDOK ){
	//	pws->waittick = DEFAULTWAITPPBTIME;
		GetCustData(T("X_wppb"),&pws->waittick,sizeof(DWORD));
		if ( pws->waittick == (DWORD)-1 ){
			pws->user = IDB_WAITPPB_NOPPB;
		}else if ( pws->waittick == (DWORD)-2 ){
			pws->user = IDB_WAITPPB_NEWPPB;
		}else{
			pws->waittick *= 1000; // ms→sec
			pws->user = 0;
		}
		if ( (pws->flags & XEO_SEQUENTIAL) && (pws->user != 0) ){
			pws->waittick = DEFAULTWAITPPBTIME * 1000;
			pws->user = 0;
		}
	}
	if ( pws->user != 0 ) return;

	while ( PeekMessage(&msg,NULL,0,0,PM_REMOVE )){
		if ( msg.message == WM_QUIT ) break;
		if ( (pws->hDlg == NULL) || (IsDialogMessage(pws->hDlg,&msg) == FALSE)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
				// GetTickCountオーバーフロー時は、即座に反応
	if ( (GetTickCount() - pws->starttick) > pws->waittick ){
		if ( (pws->hDlg == NULL) && !(pws->flags & XEO_WAITQUIET) ){
			pws->hDlg = CreateDialogParam(DLLhInst,MAKEINTRESOURCE(IDD_WAITPPB),
					hOwner,WaitPPBDlgBox,(LPARAM)&pws->user);
			ShowWindow(pws->hDlg,SW_SHOWNOACTIVATE);
			if ( GetParent(hOwner) != NULL ) SetFocus(pws->hDlg);
		}
	}
	return;
}

BOOL PPbCheck(int ID)
{
	ShareX *P;
	TCHAR syncname[32];
	DWORD result = WAIT_FAILED;

	tstrcpy(syncname,SyncTag);
	P = &Sm->P[ID];
	UsePPx();
	if ( (P->ID[0] == 'B') && (P->ID[1] == '_') ){
		HANDLE hCommEvent2;

		tstrcat(syncname,P->ID);			// PPb との通信手段を確保 ---------
		tstrcat(syncname,T("R"));
		hCommEvent2 = OpenEvent(EVENT_ALL_ACCESS,FALSE,syncname);
		if (hCommEvent2 != NULL){
			result = WaitForSingleObject(hCommEvent2,50);
			CloseHandle(hCommEvent2);
		}
	}
	FreePPx();
	return (result == WAIT_OBJECT_0);
}

BOOL ComExecPPb(HWND hOwner,const TCHAR *line,const TCHAR *path,int *useppb,int flags,int ID,DWORD *ExitCode)
{
	TCHAR buf[PPbParamSize];
	struct PPBWAITSTRUCT pws = { NULL,IDOK,0,DEFAULTWAITPPBTIME,0 };
	HWND hOldFocus;

	if ( hOwner != NULL ) hOldFocus = GetFocus();
	if ( (useppb != NULL) && (*useppb >= 0) ){	// 指定PPbが空くまで待機
		ShareX *P;
		TCHAR syncname[32];

		tstrcpy(syncname,SyncTag);
		P = &Sm->P[ID];
		UsePPx();
		if ( (P->ID[0] == 'B') && (P->ID[1] == '_') && (P->UsehWnd == hOwner)){
			HANDLE hCommEvent2;

			tstrcat(syncname,P->ID);		// PPB との通信手段を確保 ---------
			tstrcat(syncname,T("R"));
			FreePPx();
			hCommEvent2 = OpenEvent(EVENT_ALL_ACCESS,FALSE,syncname);
			if ( hCommEvent2 == NULL ){
				xmessage(XM_FaERRd,T("PPB-Event destroyed"));
				FreePPb(hOwner,ID);
				goto error;
			}
			if ( hOwner != NULL ) EnableWindow(hOwner,FALSE);
			pws.flags = flags;
			pws.starttick = GetTickCount();
			for ( ; ; ){
				DWORD result;

				result = WaitForSingleObject(hCommEvent2,50);
				if ( result == WAIT_OBJECT_0 ) break;
				if ( result == WAIT_FAILED ){
					xmessage(XM_FaERRd,T("PPB-Event destroyed"));
					CloseHandle(hCommEvent2);
					FreePPb(hOwner,ID);
					return FALSE;
				}
				PPbLoadWaitTime(hOwner,&pws);
				if ( pws.user ) break;
			}
			CloseHandle(hCommEvent2);
		}else{	// 確保していた PPb が無くなった
			FreePPx();
			ID = -1;
		}
	}else if ( ID == -1 ){	// PPb は全く起動していないのですぐに用意
		ID = AllocPPb(hOwner,path,flags);
	}else{	// 任意PPbが空くまで待機
		if ( hOwner != NULL ) EnableWindow(hOwner,FALSE);
		pws.flags = flags;
		pws.starttick = GetTickCount();
		for ( ; ; ){
			FreePPb(hOwner,ID);
			ID = UsePPb(hOwner);
			if ( ID >= 0 ){
				if ( IsTrue(PPbCheck(ID)) ) break; // 確保成功
			}
			if ( ID == -1 ){	// PPb が存在しないので新規に用意
				ID = AllocPPb(hOwner,path,flags);
				break;
			}

			PPbLoadWaitTime(hOwner,&pws);
			if ( pws.user ) break;
			Sleep(50);
		}
	}
	if ( pws.hDlg != NULL ) DestroyWindow(pws.hDlg);
	if ( hOwner != NULL ){
		EnableWindow(hOwner,TRUE);
		SetFocus(hOldFocus);
	}
	switch( pws.user ){
		case IDCANCEL:
			FreePPb(hOwner,ID);
			SetLastError(ERROR_CANCELLED);
			return FALSE;
		case IDB_WAITPPB_NEWPPB: {
			int newID = AllocPPb(hOwner,path,flags);
			FreePPb(hOwner,ID);
			ID = newID;
			break;
		}
		case IDB_WAITPPB_NOPPB:
			FreePPb(hOwner,ID);
			ID = -1;
			break;
	}
					// PPb の起動に失敗 → 自前で
	if ( ID == -1 ) return ComExecSelf(hOwner,line,path,flags,ExitCode);

										// PPb が確保できているなら PPb に送信
	if ( path != NULL ){				// Path送信
		wsprintf(buf,T(">L%s"),path);
		SendPPB(hOwner,buf,ID);
	}
	if ( line != NULL ){				// パラメータ送信
		TCHAR *p1;
		const TCHAR *opt = XEO_OptionString;
		int bits;
		size_t linelen;

		buf[0] = '>';
		buf[1] = 'H';

		p1 = buf + 2;
		for ( bits = flags ; *opt ; bits >>= 1,opt++ ){
			if ( bits & 1 ) *p1++ = *opt;
		}
		if ( hOwner != NULL ){
			p1 += wsprintf(p1,T(";%u"),hOwner);
		}
		*p1++ = ',';

		linelen = tstrlen(line);
		if ( linelen > PPbMaxSendSize ){ // CMDLINESIZE 以上の長いときは分割送信
			buf[1] = 'p';
			do {
				memcpy(p1,line,PPbMaxSendSize * sizeof(TCHAR));
				*(p1 + PPbMaxSendSize) = '\0';
				SendPPB(hOwner,buf,ID);

				line += PPbMaxSendSize;
				linelen -= PPbMaxSendSize;
				p1 = buf + 2;
			}while ( linelen > PPbMaxSendSize );
			buf[1] = 'P';
		}

		tstrcpy(p1,line);
		SendPPB(hOwner,buf,ID);

		if ( flags & (XEO_WAITIDLE | XEO_SEQUENTIAL) ){
			if ( ExitCode == NULL ){
				SendPPB(hOwner,T(">"),ID);
			}else{
				*ExitCode = RecvPPBExitCode(hOwner,ID);
			}
		}
	}
	if ( useppb != NULL ){
		*useppb = ID;
	}else{
		FreePPb(hOwner,ID);
	}
	return TRUE;
error:
	if ( pws.hDlg != NULL ) DestroyWindow(pws.hDlg);
	if ( hOwner != NULL ) EnableWindow(hOwner,TRUE);
	return FALSE;
}

void PopupErrorCodeMessage(HWND hWnd,const TCHAR *title,ERRORCODE code)
{
	TCHAR buf[0x400];

	PPErrorMsg(buf,code);
	PopupErrorMessage(hWnd,title,buf);
	SetLastError(code);
}

BOOL ComExecEx(HWND hOwner,const TCHAR *line,const TCHAR *path,int *useppb,int flags,DWORD *ExitCode)
{
	int ID = -1;
		// 自前 or 自分がコンソール なら自前
	if ( flags & (XEO_NOUSEPPB | XEO_CONSOLE) ){
		if ( !(flags & XEO_NOPIPERDIR) &&
			 (SearchPipe(line) || tstrchr(line,'<') || tstrchr(line,'>')) ){
			setflag(flags,XEO_USECMD);
		}
		return ComExecSelf(hOwner,line,path,flags,ExitCode);
	}
	FixTask();
	if ( useppb != NULL ) ID = *useppb;

	if ( ID < 0 ){
		if ( flags & XEO_USEPPB ){
			ID = UsePPb(hOwner); // もし、空いている PPb があれば確保
		}else{
			int type;
			const TCHAR *p;

			p = line;
			type = GetExecType(&p,NULL,path);
			if ( (type == GTYPE_ERROR) &&
				 (GetLastError() == ERROR_PATH_NOT_FOUND ) ){
				PopupErrorCodeMessage(hOwner,T("Comexec"),ERROR_PATH_NOT_FOUND);
				return FALSE;
			}
			// Console/CMD 不要？→自前実行できるかも
			if ( ((type == GTYPE_GUI) || (type == GTYPE_SHELLEXEC)) &&
				  ( (flags & XEO_NOPIPERDIR) ||
					!(SearchPipe(line) ||tstrchr(line,'<')|| tstrchr(line,'>'))) ){
				// 順次実行不要か空きPPb無しなら、自前実行
				if ( !(flags & XEO_SEQUENTIAL) || ((ID = UsePPb(hOwner)) < 0)){
					return ComExecSelf(hOwner,line,path,flags,ExitCode);
				}
			}else{
				ID = UsePPb(hOwner); // もし、空いている PPb があれば確保
			}
		}
	}
	// PPb 経由で実行する
	return ComExecPPb(hOwner,line,path,useppb,flags,ID,ExitCode);
}

// 指定時間以上経過すると表示するボックス -------------------------------------
INT_PTR CALLBACK WaitDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			WAITDLGSTRUCT *wds = (WAITDLGSTRUCT *)lParam;

			wds->md.text = MessageText(STR_WAITOPERATION);
			wds->md.style = MB_PPX_STARTCANCEL | MB_DEFBUTTON1 | MB_PPX_NOCENTER;
			MessageBoxInitDialog(hDlg,&wds->md);
			SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)&wds->user);
			return FALSE;
		}
		case WM_CLOSE:
			*(int *)GetWindowLongPtr(hDlg,DWLP_USER) = FALSE;
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDYES:
					*(int *)GetWindowLongPtr(hDlg,DWLP_USER) = TRUE;
					break;

				case IDCANCEL:
					WaitDlgBox(hDlg,WM_CLOSE,0,0);
					break;

				case IDM_CONTINUE:
					WaitDlgBox(hDlg,WM_COMMAND,IDOK,0);
					SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)IDM_CONTINUE);
					PostMessage(hDlg,WM_NULL,0,0); // メッセージループを回す
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}
// 起動したプロセスの終了を待つ -----------------------------------------------
// TRUE:正常に終了/強制的に次へ FALSE:中断
BOOL WaitJobDialog(HWND hWnd,HANDLE handle,const TCHAR *title,DWORD flags)
{
	DWORD result,timeout = NODIALOGTIME,tick;
	HWND hDlg = NULL;
	WAITDLGSTRUCT wds;

	tick = GetTickCount() + NODIALOGTIME;
	wds.user = -1;
	for ( ; ; ){
		if ( handle != NULL ){
			result = MsgWaitForMultipleObjects(1,&handle,FALSE,
					timeout,QS_ALLEVENTS | QS_SENDMESSAGE);
		}else{
			if ( CheckJobWait(NULL) == FALSE ) break;
			Sleep(20);
			result = WAIT_OBJECT_0 + 1;
			timeout -= 20;
			if ( timeout < 40 ) result = WAIT_TIMEOUT;
		}
		if ( wds.user != -1 ) break;
		// GetTickCountオーバーフロー時は、MsgWaitForMultipleObjectsのみで判断
		if ( (result == WAIT_TIMEOUT) || (GetTickCount() > tick) ){
			tick = MAX32;
			if ( (hDlg == NULL) && !(flags & XEO_WAITQUIET) ){
				BOOL hide;

				wds.md.title = (title != NULL) ? title : PPXJOBMUTEX;
				hDlg = CreateDialogParam(DLLhInst,MAKEINTRESOURCE(IDD_NULLMIN),NULL,WaitDlgBox,(LPARAM)&wds);

				if ( hWnd != NULL ) MoveCenterWindow(hDlg,hWnd);

				hide = SetJobTask(hDlg,
						flags ? JOBSTATE_WAITJOB : JOBSTATE_WAITEXEC);
				ShowWindow(hDlg,
						(hide &&
						 (Sm->JobList.hWnd != NULL) &&
						 (Sm->JobList.hidemode == JOBLIST_HIDE)) ?
						SW_HIDE : SW_SHOWNOACTIVATE);
			}
			timeout = INFINITE;
		}else if ( result == WAIT_OBJECT_0 + 1 ){ // Message
			MSG msg;

			while ( PeekMessage(&msg,NULL,0,0,PM_REMOVE) ){
				if ( msg.message == WM_QUIT ) break;
				// 自ウィンドウのキー入力・マウス入力を無効化する
				// ※ EnableWindow を使うと、フォーカス操作まで無効化されて、
				//    別のウィンドウにフォーカスが移動してしまうことがあるため
				// ※ WM_IME_KEYLAST ≒ WM_KEYLAST
				// ※ (WM_PARENTNOTIFY - 1) ≒ WM_MOUSELAST
				if ( (msg.hwnd == hWnd) &&
					( ((msg.message >= WM_KEYFIRST) && (msg.message <= WM_IME_KEYLAST)) ||
					  ((msg.message >= WM_MOUSEFIRST) && (msg.message < WM_PARENTNOTIFY))) ){
					continue;
				}

				if ( (hDlg == NULL) || (IsDialogMessage(hDlg,&msg) == FALSE) ){
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}else{
			break;
		}
	}
	if ( hDlg ){
		DestroyWindow(hDlg);
		SetJobTask(hDlg,flags ? JOBSTATE_FINWJOB : JOBSTATE_FINWEXEC);
	}
	if ( wds.user == -1 ) wds.user = TRUE;
	return wds.user;
}

DefineWinAPI(DWORD, GetProcessId, (HANDLE Process));

#pragma argsused
void DummyGetProcessID(HANDLE hProcess, DWORD *ExitCode)
{
	UnUsedParam(hProcess);

	if ( ExitCode != NULL ) *ExitCode = -1;
}

void GetProcessIDMain(HANDLE hProcess, DWORD *ExitCode)
{
	if ( ExitCode != NULL ) *ExitCode = DGetProcessId(hProcess);
}
void InitGetProcessID(HANDLE hProcess, DWORD *ExitCode);
void (* GetProcessIDx)(HANDLE hProcess, DWORD *ExitCode) = InitGetProcessID;

/*
BOOL CALLBACK FindProcessWindow(HWND hWnd, LPARAM lParam)
{
	DWORD processID;

	if ( 0 != GetWindowThreadProcessId(hWnd, &processID) ){
		if ( processID == (DWORD)lParam ){
			Message("!");
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			return FALSE;
		}
	}
	return TRUE;
}
*/
void InitGetProcessID(HANDLE hProcess, DWORD *ExitCode)
{
	DGetProcessId = GETDLLPROC(hKernel32,GetProcessId);
	if ( DGetProcessId == NULL ){
		GetProcessIDx = DummyGetProcessID;
	}else{
		GetProcessIDx = GetProcessIDMain;
	}
	GetProcessIDx(hProcess, ExitCode);
/*
	if ( (ExitCode != NULL) && (*ExitCode != 0xffffffff) ){
		int i;
		for ( i = 0 ; i < 10000 ; i+= 100 ){
			if ( FALSE == EnumWindows(FindProcessWindow,(LPARAM)*ExitCode) ){
				break;
			}
			Sleep(100);
		}
	}
*/
}

BOOL ComExecSelfShell(HWND hOwner,const TCHAR *title,const TCHAR *exename,const TCHAR *param,const TCHAR *path,int flag,DWORD *ExitCode)
{
	HANDLE hProcess;
	TCHAR reason[VFPS];

	if ( NULL != (hProcess = PPxShellExecute(hOwner,NULL,exename,param,path,flag,reason)) ){
		if ( flag & (XEO_WAITIDLE | XEO_SEQUENTIAL) ){
			BOOL result = TRUE;

			if ( flag & XEO_WAITIDLE ) WaitForInputIdle(hProcess,INFINITE);
			if ( flag & XEO_SEQUENTIAL ){
				result = WaitJobDialog(hOwner,hProcess,title,flag & XEO_WAITQUIET);
				if ( ExitCode != NULL ){
					*ExitCode = (DWORD)-1;
					GetExitCodeProcess(hProcess, ExitCode);
				}
			}else{
				GetProcessIDx(hProcess, ExitCode);
			}
			CloseHandle(hProcess);
			return result;
		}
		return TRUE;
	}
	PopupErrorMessage(hOwner,T("ComExecSelf"),reason);
	return FALSE;
}

#ifdef WINEGCC // system で実行
int ComExecSystem(TCHAR *commandline,const TCHAR *CurrentDir)
{
	char bufA[CMDLINESIZE];

	TCHAR *src,*dst,OldCurrentDir[VFPS];
	char *chpath;
	BOOL fixsep = FALSE;
	int result;

							// Z: ドライブのパスをネイティブに変換する
	for ( dst = src = commandline ; *src ; src++ ){
		if ( (*src == ':') && (src > commandline) &&
				((*(src - 1) == 'Z') || (*(src - 1) == 'z')) ){
			fixsep = TRUE;
			dst--;
			continue;
		}else if ( IsTrue(fixsep) ){
			if ( (*src == ' ') || (*src == '\t') ){
				fixsep = FALSE;
			}else if ( *src == '\\' ){
				*dst++ = '/';
				continue;
			}
		}
		*dst++ = *src;
	}
	*dst = '\0';

	GetCurrentDirectory(TSIZEOF(OldCurrentDir),OldCurrentDir);
#ifdef UNICODE
	UnicodeToUtf8(CurrentDir,bufA,CMDLINESIZE);

	while ( (chpath = strchr(bufA,'\\')) != NULL ) *chpath = '/';
	chpath = bufA;
	if ( (*(chpath + 1) == ':') &&
		( (*chpath == 'z') || (*chpath == 'Z') ) ){
		chpath += 2;
	}
	chdir(chpath);
	SetCurrentDirectory(CurrentDir);
	if ( (UnicodeToUtf8(commandline,bufA,CMDLINESIZE) == 0) &&
		 (GetLastError() == ERROR_INSUFFICIENT_BUFFER) ){ // バッファが小さい
		int len;
		char *bufptr;

		len = UnicodeToUtf8(commandline,NULL,0);
		bufptr = malloc(len);
		UnicodeToUtf8(commandline,bufptr,len);
		result = system(bufptr);
		free(bufptr);
	}else{
		result = system(bufA);
	}
#else
	strcpy(bufA,CurrentDir);
	while ( (chpath = strchr(bufA,'\\')) != NULL ) *chpath = '/';
	chpath = bufA;
	if ( (*(chpath + 1) == ':') &&
		( (*chpath == 'z') || (*chpath == 'Z') ) ){
		chpath += 2;
	}
	chdir(chpath);
	SetCurrentDirectory(CurrentDir);
	result = system(commandline);
#endif
	SetCurrentDirectory(OldCurrentDir);
	return result;
}
#endif

const TCHAR *FixCurrentDirPath(const TCHAR *path)
{
	if ( (path != NULL) && (
			(path[0] == '?') || !(
				(Isalpha(path[0]) && (path[1] == ':')) ||
				((path[0] == '\\') && (path[1] == '\\') && (tstrchr(path + 2,'\\') != NULL)) ||
				(path[0] == '/')
			)
		  ) ){
		if ( TempPath[0] == '\0' ) GetTempPath(MAX_PATH,TempPath);
		path = TempPath;
	}
	return path;
}

//CREATE_PROTECTED_PROCESS		0x00040000			Vista以降
//CREATE_PRESERVE_CODE_AUTHZ_LEVEL	0x02000000		XP以降
/*-----------------------------------------------------------------------------
	自前で外部プロセスを起動する
-----------------------------------------------------------------------------*/
BOOL ComExecSelf(HWND hOwner,const TCHAR *execarg,const TCHAR *path,int flags,DWORD *ExitCode)
{
	TCHAR LineStaticBuf[CMDLINESIZE + MAX_PATH],*LineBuf = LineStaticBuf;
	TCHAR *exeterm,*paramdest;
	size_t linelen;

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	const TCHAR *param;
	int exetype;
	int result;
	DWORD createflags = CREATE_DEFAULT_ERROR_MODE;

	param = execarg;
	while( (*param == ' ') || (*param == '\t') ) param++;
	exetype = GetExecType(&param,LineStaticBuf,path);

	if ( exetype == GTYPE_SHELLEXEC ){	// ShellExecute で起動
		return ComExecSelfShell(hOwner,execarg,LineStaticBuf,param,path,flags,ExitCode);
	}
	if ( !(flags & XEO_NOSCANEXE) && (CheckExebin(LineStaticBuf,exetype) == FALSE) ){
		return FALSE;
	}
									// LineStaticBuf で足りないならメモリ確保
	linelen = tstrlen(LineStaticBuf) + tstrlen(param) + MAX_PATH;
	if ( linelen > (CMDLINESIZE + MAX_PATH) ){
		LineBuf = HeapAlloc(DLLheap,0,TSTROFF(linelen));
		if ( LineBuf == NULL ) return FALSE;
		tstrcpy(LineBuf,LineStaticBuf);
	}
	if ( (flags & XEO_CONSOLE) && (exetype == GTYPE_CONSOLE) ){
		setflag(flags,XEO_SEQUENTIAL);
	}

	// unknown / data / cmd
	if ( (exetype == GTYPE_ERROR) || (exetype == GTYPE_DATA) || (flags & XEO_USECMD) ){
	#ifndef WINEGCC // unknown / data ... cmd 経由
		BOOL sepfix = FALSE;

		// CMD 関連
		if ( (LineBuf[0] == '\"') && (tstrchr(param,'\"') != NULL) ){
			sepfix = TRUE;
		}
		if ( tstrlen(param) > 8000 ){ // CMD の長さ制限
			PPErrorMsg(LineStaticBuf,RPC_S_STRING_TOO_LONG);
			PopupErrorMessage(hOwner,T("ComExecSelf"),LineStaticBuf);
			SetLastError(RPC_S_STRING_TOO_LONG);
			result = FALSE;
			goto fin;
		}
		// %COMSPEC% 取得
		if ( GetEnvironmentVariable(ComSpecStr,LineBuf,MAX_PATH) == 0 ){
			#ifdef UNICODE
				tstrcpy(LineBuf,T("CMD.EXE"));
			#else
				tstrcpy(LineBuf,T("COMMAND.COM"));
			#endif // UNICODE
		}
		exeterm = paramdest = LineBuf + tstrlen(LineBuf);
		tstrcpy(paramdest,T(" /C "));
		paramdest += 4;
		// cmd は「"file name.bat" param」は成功し、「"file name.bat" "param"」
		// で失敗するので、「""file name.bat" "param"」に加工する
		if ( IsTrue(sepfix) ) *paramdest++ = '\"';
		param = execarg;
	#else // WINEGCC // system で実行する
		tstrcpy(LineBuf,execarg);
		result = ComExecSystem(LineBuf,path);
		if ( ExitCode != NULL ) ExitCode = result;
		result = (result == -1) ? FALSE : TRUE;
		goto fin;
	#endif // WINEGCC
	}else{
		exeterm = paramdest = LineBuf + tstrlen(LineBuf);
		if ( *param != ' ' ) *paramdest++ = ' ';
	}

	tstrcpy(paramdest,param);
											// 実行条件の指定
	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;
	si.wShowWindow	= SW_SHOWDEFAULT;

	if ( flags & (XEO_MAX | XEO_MIN | XEO_NOACTIVE | XEO_HIDE) ){
		si.dwFlags = STARTF_USESHOWWINDOW;
		if ( flags & XEO_MAX ) si.wShowWindow = SW_SHOWMAXIMIZED;
		if ( flags & XEO_MIN ) si.wShowWindow = SW_SHOWMINNOACTIVE;
		if ( flags & XEO_NOACTIVE ) si.wShowWindow = SW_SHOWNOACTIVATE;
		if ( flags & XEO_HIDE ){
			si.wShowWindow = SW_HIDE;
			if ( !(flags & XEO_USECMD) ){ // リダイレクトするときは DETACHED 不可
				setflag(createflags,DETACHED_PROCESS);
			}
		}
	}
	if ( flags & XEO_LOW ) setflag(createflags,IDLE_PRIORITY_CLASS);
	result = TRUE;

	if ( hOwner != NULL ){
		SendMessage(hOwner,WM_PPXCOMMAND,K_SETPOPLINENOLOG,(LPARAM)T("execute..."));
		UpdateWindow(hOwner);
	}

	path = FixCurrentDirPath(path);
	if ( IsTrue(CreateProcess(NULL,LineBuf,NULL,NULL,FALSE,
			createflags,NULL,path,&si,&pi)) ){
		CloseHandle(pi.hThread);
		if ( flags & XEO_WAITIDLE ) WaitForInputIdle(pi.hProcess,INFINITE);
		if ( flags & XEO_SEQUENTIAL ){
			result = WaitJobDialog(hOwner,pi.hProcess,execarg,flags & XEO_WAITQUIET);
			if ( ExitCode != NULL ){
				*ExitCode = (DWORD)-1;
				GetExitCodeProcess(pi.hProcess, ExitCode);
			}
		}else{
			GetProcessIDx(pi.hProcess, ExitCode);
		}
		CloseHandle(pi.hProcess);
	}else{
		ERRORCODE errorcode;
		TCHAR *exefilename;

		exefilename = LineBuf;
		if ( *exefilename == '\"' ){ // 「"」 括りを除去
			exefilename++;
			exeterm--;
		}
		*exeterm = '\0';

		errorcode = GetLastError();
		if ( errorcode == ERROR_ELEVATION_REQUIRED ){ // UAC が必要
			// ※但し、Vistaは一度このエラーが来ると互換性アシスタントがでて、
			//   それ以降はエラーが出なくなる
			result = ComExecSelfShell(hOwner,execarg,exefilename,param,path,flags,ExitCode);
		}else{
			errorcode = GetExecuteErrorReason(exefilename,LineStaticBuf);
			PopupErrorMessage(hOwner,T("ComExecSelf"),LineStaticBuf);
			SetLastError(errorcode);
			result = FALSE;
		}
	}
	if ( (hOwner != NULL) && IsTrue(result) ){
		SendMessage(hOwner,WM_PPXCOMMAND,K_SETPOPLINENOLOG,0);
	}

fin:
	if ( LineBuf != LineStaticBuf ) HeapFree(DLLheap,0,LineBuf);
	return result;
}
/*-----------------------------------------------------------------------------
	ShellExecute を実行し、エラーがでたら文字列を取得する。
	失敗したら NULL を返す
	※HANDLE はXEO_WAITIDLE | XEO_SEQUENTIAL でなければ開放不要である
-----------------------------------------------------------------------------*/
PPXDLL HANDLE PPXAPI PPxShellExecute(HWND hwnd,LPCTSTR lpOperation,LPCTSTR lpFile,LPCTSTR lpParameters,LPCTSTR lpDirectory,int flag,TCHAR *ErrMsg)
{
	SHELLEXECUTEINFO ExeInfo;

	if ( !(flag & XEO_NOSCANEXE) && (CheckExebin(lpFile,-1) == FALSE) ){
		SetLastError(PPErrorMsg(ErrMsg,ERROR_CANCELLED));
		return NULL;
	}

	ExeInfo.cbSize			= sizeof(ExeInfo);
	ExeInfo.fMask			= SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_UNICODE;
	ExeInfo.hwnd			= hwnd;
	ExeInfo.lpVerb			= lpOperation;
	ExeInfo.lpFile			= lpFile;
	ExeInfo.lpParameters	= lpParameters;
	ExeInfo.lpDirectory		= FixCurrentDirPath(lpDirectory);
	ExeInfo.nShow			= SW_SHOWNORMAL;
	if ( flag & XEO_MAX ) ExeInfo.nShow = SW_SHOWMAXIMIZED;
	if ( flag & XEO_MIN ) ExeInfo.nShow = SW_SHOWMINNOACTIVE;
	if ( flag & XEO_NOACTIVE ) ExeInfo.nShow = SW_SHOWNOACTIVATE;
	if ( flag & XEO_HIDE ){
		ExeInfo.nShow = SW_HIDE;
//		if ( !(flag & XEO_USECMD) ){ // リダイレクトするときは DETACHED 不可
//			setflag(createflag,DETACHED_PROCESS);
//		}
//		setflag(createflag,DETACHED_PROCESS);
	}
//	if ( flag & XEO_LOW ) setflag(createflag,IDLE_PRIORITY_CLASS);

	if ( flag & (XEO_WAITIDLE | XEO_SEQUENTIAL) ){
		setflag(ExeInfo.fMask,SEE_MASK_NOCLOSEPROCESS);
	}
	if ( IsTrue(ShellExecuteEx(&ExeInfo)) ){
		if ( (flag & (XEO_WAITIDLE | XEO_SEQUENTIAL)) &&
				(ExeInfo.hProcess != NULL) ){
			return ExeInfo.hProcess;
		}
		return INVALID_HANDLE_VALUE;
	}else{
		GetExecuteErrorReason(lpFile,ErrMsg);
		return NULL;
	}
}

//====================================================== 起動するプロセスの判別
int GetE2(TCHAR *fname)
{
	DWORD Bsize;				// バッファサイズ指定用
	DWORD Rtyp;				// レジストリの種類

	TCHAR appN[MAX_PATH];		// アプリケーションのキー
	TCHAR defN[MAX_PATH];		// デフォルトのアクション
	HKEY hExt,hAP;
	TCHAR *ext;
	int type = GTYPE_SHELLEXEC; //GTYPE_DATA;

	ext = VFSFindLastEntry(fname);
	ext += FindExtSeparator(ext);

										// 拡張子からキーを求める -------------
	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT,ext,0,KEY_READ,&hExt)){
		RegOpenKeyEx(HKEY_CLASSES_ROOT,WildCard_All,0,KEY_READ,&hExt);
	}
	Bsize = sizeof appN;					// 拡張子の識別子
	Rtyp = REG_SZ;
	if ( ERROR_SUCCESS ==
				RegQueryValueEx(hExt,NilStr,0,&Rtyp,(LPBYTE)appN,&Bsize) ){
		tstrcat(appN,T("\\shell"));
										// アプリケーションのシェル -----------
		if ( ERROR_SUCCESS ==
				RegOpenKeyEx(HKEY_CLASSES_ROOT,appN,0,KEY_READ,&hAP)){
			Bsize = sizeof defN;
			defN[0] = '\0';
			RegQueryValueEx(hAP,NilStr,0,&Rtyp,(LPBYTE)defN,&Bsize);
			if ( defN[0] == '\0' ) tstrcpy(defN,ShellVerb_open); // の指定が無い

			tstrcat(defN,T("\\command"));
			if ( GetRegString(hAP,defN,NilStr,appN,sizeof(appN)) ){
				TCHAR *p,*q = NULL;

				p = appN;
				while( (*p == ' ') || (*p == '\t') ) p++;
				if ( *p != '\0' ){
					if ( *p == '\"' ){
						p++;
						q = tstrchr(p,'\"');
					}else{
						q = tstrchr(p,' ');
						if ( q == NULL ) q = tstrchr(p,'\t');
					}
				}
				if ( q == NULL ) q = p + tstrlen(p);
				*q = '\0';
				if ( tstrcmp(p,T("%1")) ){
					type = GetExecTypeFileOnly(p);

					if ( (type == GTYPE_SHELLEXEC) || (type == GTYPE_GUI) ){
						type = GTYPE_SHELLEXEC;	// GUI型
					}else{
						type = GTYPE_DATA;		// CUI型
					}
				}else{	// 自分自身で実行可能→ COM,EXT,BAT,CMD の可能性
					type = GTYPE_DATA;
				}
			}
			RegCloseKey(hAP);
		}
	}
	RegCloseKey(hExt);
	return type;
}
/*-----------------------------------------------------------------------------
	指定位置のみでプロセスを調べる
-----------------------------------------------------------------------------*/
int GetExecTypeFileOnly(TCHAR *name)
{
	int type = GTYPE_DATA;
	DWORD tmp;
	WORD wp;
	HANDLE hFile;						// 確認中のファイル
	BYTE header[0x200];
										// ファイルの先頭を取得する -----------
	hFile = CreateFileL(name,GENERIC_READ,FILE_SHARE_WRITE | FILE_SHARE_READ,
					NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		for ( ; ; ){
			TCHAR *p;

			if ( FALSE ==
					ReadFile(hFile,header,sizeof(IMAGE_DOS_HEADER),&tmp,NULL)){
				type = GTYPE_SHELLEXEC; //GTYPE_ERROR;
				break;	// 読み込み失敗
			}
							// MS EXE header を確認			データ、batch、com
			if ( (tmp < sizeof(IMAGE_DOS_HEADER)) ||
			  (((IMAGE_DOS_HEADER *)header)->e_magic != IMAGE_DOS_SIGNATURE) ){
				type = GetE2(name);
				break;
			}
							// 拡張子の確認
			p = tstrrchr(name,'.');
			if ( p == NULL ) break;
			if ( tstricmp(p + 1,T("COM")) && tstricmp(p + 1,T("EXE")) && tstricmp(p + 1,T("SCR")) ){
				break;
			}

			type = GTYPE_CONSOLE;
			tmp = ((IMAGE_DOS_HEADER *)header)->e_lfanew;	// 拡張ヘッダoffset
														// DOS EXE
			if ( !tmp ) break;
			if ( SetFilePointer(hFile,tmp,NULL,FILE_BEGIN) != tmp ) break;
			if ( ReadFile(hFile,header,0x60,&tmp,NULL) == FALSE ) break;
			if ( tmp < 0x60 ) break;
							// NE header を確認				WIN16,OS/2
			if (((IMAGE_OS2_HEADER *)header)->ne_magic == IMAGE_OS2_SIGNATURE){
				type = GTYPE_GUI;
				break;
			}
							// PE header を確認				DOS EXE
			if (((IMAGE_NT_HEADERS *)header)->Signature != IMAGE_NT_SIGNATURE){
				break;
			}
			wp = ((IMAGE_NT_HEADERS *)header)->OptionalHeader.Subsystem;
			if ( (wp == IMAGE_SUBSYSTEM_UNKNOWN) ||
				 (wp >= IMAGE_SUBSYSTEM_WINDOWS_CUI) ){ // ネイティブ・GUI 以外
				break;
			}
			type = GTYPE_GUI;
			break;
		};
		CloseHandle( hFile );
		return type;
	}
	// 存在しない(BADATTR),ファイル,ラベルならエラー
	if ( GetFileAttributesL(name) &
			(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_LABEL) ){
		return GTYPE_ERROR;
	}
	return GTYPE_SHELLEXEC; //GTYPE_DATA
}
/*-----------------------------------------------------------------------------
	指定ディレクトリで拡張子を補完してプロセスを調べる
-----------------------------------------------------------------------------*/
int GetExecTypeDirOnly(TCHAR *name,int flag)
{
	int type;
	TCHAR *p,*q,*n;
	TCHAR ext[2048];
										// 素のままで調べる ===================
	type = GetExecTypeFileOnly(name);
	if ( type != GTYPE_ERROR ) return type;
	if ( flag & ET_EXT ) return GTYPE_ERROR;	// 拡張子があるのでここで終了

										// 拡張子を付加して検索 ===============
	p = ext;
	if ( GetEnvironmentVariable(PathextStr,ext,TSIZEOF(ext)) == 0 ){
		p = T(".COM;.EXE;.BAT");
	}
	n = name + tstrlen(name);
	while( *p != '\0' ){
		q = tstrchr(p,';');
		if ( q == NULL ) q = p + tstrlen(p);
		memcpy(n,p,TSTROFF(q - p));
		*(n + (q - p)) = '\0';
		type = GetExecTypeFileOnly(name);
		if ( type != GTYPE_ERROR ) return type;
		p = q;
		if ( *p ) p++;
	}
	return GTYPE_ERROR;
}

void FileNameCopy(TCHAR *dest,TCHAR *src)
{
	if ( dest == NULL ) return;
	if ( tstrchr(src,' ') != NULL ){
		int len;

		*dest++ = '\"';
		len = tstrlen32(src);
		memcpy(dest,src,TSTROFF(len));
		*(dest + len) = '\"';
		*(dest + len + 1) = '\0';
	}else{
		tstrcpy(dest,src);
	}
}
/*-----------------------------------------------------------------------------
	ファイルの場所と種類を調べる & 実行名とパラメータを分離する
	※fpath は「"」付で出力されることあり
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI GetExecType(LPCTSTR *name,TCHAR *fpath,const TCHAR *path)
{
	TCHAR fname[VFPS],envpath[1024],exepath[VFPS];
	const TCHAR *p;
	int flag = 0;
	int type;
									// 実行ファイルを exepath に抽出 ========
	{
		const TCHAR *lastp;

		p = *name;
		while( (*p == ' ') || (*p == '\t') ) p++;
		if ( *p == '\0' ) goto generror;
		if ( *p == '\"' ){
			p++;
			lastp = tstrchr(p,'\"');
		}else{
			lastp = p;
			while( (*lastp != '\0') && (*lastp != ' ') && (*lastp != '\t') ){
				lastp++;
			}
		}
		if ( lastp == NULL ) lastp = p + tstrlen(p);
		if ( (lastp - p) >= VFPS ) goto generror; // コマンドが長すぎ
		memcpy(exepath,p,TSTROFF(lastp - p));
		*(exepath + (lastp - p)) = '\0';

		*name = lastp;
		if ( *lastp != '\0' ) (*name)++;
	}
										// 実行ファイルパスの形式を解析 =======
	p = exepath;
	for ( ; *p != '\0' ; p++ ){
		if ( *p == ':' ){
			if ( !(flag & ET_PATH) ){
				int size;

				size = p - exepath;
				if ( ((size == 3) && !memcmp(exepath,T("ftp"),TSTROFF(3))) ||
					 ((size == 4) && !memcmp(exepath,T("http"),TSTROFF(4))) ||
					 ((size == 5) && !memcmp(exepath,T("https"),TSTROFF(5))) ||
					 ((size == 6) && !memcmp(exepath,T("mailto"),TSTROFF(6)))){
					if ( fpath != NULL ) tstrcpy(fpath,exepath);
					return GTYPE_SHELLEXEC;
				}
				flag = (flag & ~ET_EXT) | ET_PATH;
			}
			continue;
		}
		if ( *p == '\\' ){
			flag = (flag & ~ET_EXT) | ET_PATH;
			continue;
		}
		if ( *p == '.') setflag(flag,ET_EXT);
#ifndef UNICODE
		if ( IskanjiA(*p) && (*(p+1)) ) p++;
#endif
	}
										// カレントディレクトリで検索 =========
	if ( flag & ET_PATH ){
		VFSFixPath(fname,exepath,path,VFSFIX_SEPARATOR | VFSFIX_FULLPATH |
				VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
	}else{
		VFSFullPath(fname,exepath,path);
	}
	type = GetExecTypeDirOnly(fname,flag);
	if ( type != GTYPE_ERROR ){
		FileNameCopy(fpath,fname);
		return type;
	}
	if ( flag & ET_PATH ){
		SetLastError(ERROR_PATH_NOT_FOUND);
		goto error; // path 指定ありならここで終わり
	}
										// PPxディレクトリで検索 =========
	CatPath(fname,DLLpath,exepath);
	type = GetExecTypeDirOnly(fname,flag);
	if ( type != GTYPE_ERROR ){
		FileNameCopy(fpath,fname);
		return type;
	}
	{									// 環境変数 path で検索 ===============
		const TCHAR *envptr,*sep;
		TCHAR *envbuf = NULL;
		DWORD envlen;

		envptr = envpath;
		envlen = GetEnvironmentVariable(StrPath,envpath,TSIZEOF(envpath));
		if ( envlen == 0 ) goto generror;
		if ( envlen >= (TSIZEOF(envpath) - 16) ){
			envbuf = HeapAlloc(DLLheap,0,TSTROFF(envlen + 16));
			if ( envbuf == NULL ) goto generror;
			GetEnvironmentVariable(StrPath,envbuf,envlen + 16);
			envptr = envbuf;
		}

		while ( *envptr != '\0' ){
			sep = tstrchr(envptr,';');
			if ( sep == NULL ) sep = envptr + tstrlen(envptr);
			memcpy(fname,envptr,TSTROFF(sep - envptr));
			*(fname + (sep - envptr)) = '\0';
			CatPath(NULL,fname,exepath);
			type = GetExecTypeDirOnly(fname,flag);
			if ( type != GTYPE_ERROR ){
				FileNameCopy(fpath,fname);
				if ( envbuf != NULL ) HeapFree(DLLheap,0,envbuf);
				return type;
			}
			envptr = sep;
			if ( *envptr != '\0' ) envptr++;
		}
		if ( envbuf != NULL ) HeapFree(DLLheap,0,envbuf);
	}
										// レジストリ App Paths で検索 ========
							// ※本当は%path%よりも前だが、あえてこの位置で検索
	wsprintf(envpath,AppPathKey,exepath);
	fname[0] = '\0';
	if ( (GetRegString(HKEY_CURRENT_USER,envpath,NilStr,fname,TSIZEOF(fname)) == FALSE) &&
		 (GetRegString(HKEY_LOCAL_MACHINE,envpath,NilStr,fname,TSIZEOF(fname)) == FALSE)

	 ){
		if ( !(flag & ET_EXT) ){
			tstrcat(envpath,T(".exe"));
			if ( GetRegString(HKEY_CURRENT_USER,envpath,NilStr,fname,TSIZEOF(fname)) == FALSE ){
				GetRegString(HKEY_LOCAL_MACHINE,envpath,NilStr,fname,TSIZEOF(fname));
			}
		}
	}
	if ( fname[0] != '\0' ){
		type = GetExecTypeFileOnly(fname);
		if ( type != GTYPE_ERROR ){
			FileNameCopy(fpath,fname);
			return type;
		}
	}

	if ( flag & ET_EXT ){ // 拡張子有りなら、cmdで実行するコマンドでない
		if ( fpath != NULL ) tstrcpy(fpath,exepath);
		return GTYPE_SHELLEXEC;
	}
generror:
	SetLastError(NO_ERROR);
error:
	if ( fpath != NULL ) *fpath = '\0';
	return GTYPE_ERROR;
}

DWORD GetExeCrc(const TCHAR *path)
{
	HANDLE hFile;
	DWORD crc;
	BYTE bin[0x8000];
	DWORD fsize;

	crc = 0;

	hFile = CreateFileL(path,GENERIC_READ,FILE_SHARE_READ,NULL,
							OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return 0;

	if ( IsTrue(ReadFile(hFile,bin,sizeof bin,&fsize,NULL)) ){
		crc = crc32(bin,fsize,0);
	}
	CloseHandle(hFile);
	return crc;
}

BOOL CheckExecs(const TCHAR *file,DWORD *crcs,DWORD crc)
{
	DWORD i;

	memset(crcs,0,(MAXSTORE + 1) * sizeof(DWORD));
	GetCustTable( T("_Execs"),file,crcs,(MAXSTORE + 1)*sizeof(DWORD));
	if ( crcs[0] == 0 ) return FALSE;
	if ( crcs[0] >= MAXSTORE ) crcs[0] = MAXSTORE;
	i = crcs[0];
	while ( i ){
		crcs++;
		if ( *crcs == crc ) return TRUE;
		i--;
	}
	return FALSE;
}

DWORD_PTR USECDECL CebInfo(CHECKEXEBININFO *CIS,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case 'C':
			tstrcpy(uptr->enums.buffer,CIS->name);
			break;
		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

PPXDLL BOOL PPXAPI CheckExebin(const TCHAR *path,int type)
{
	TCHAR file[VFPS],buf[VFPS * 2],info[0x600],*inf;
	const TCHAR *p;
	DWORD crc,crcs[MAXSTORE + 1];

	if ( X_execs < 0 ){
		X_execs = 0;
		GetCustData( T("X_execs"),&X_execs,sizeof(X_execs));
	}
	if ( X_execs == 0 ) return TRUE;

	VFSFixPath(buf,(TCHAR *)path,NULL,(*path == '\"') ?
		(VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH) :
		(VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE) );
	if ( type < 0 ){
		p = buf;
		type = GetExecType(&p,NULL,NULL);
	}
	if ( (type != GTYPE_GUI) && (type != GTYPE_CONSOLE) ) return TRUE;

	tstrcpy(file,FindLastEntryPoint(buf));

	crc = GetExeCrc(buf);
	if ( FALSE == CheckExecs(file,crcs,crc) ){
		VFSFILETYPE vft;
		ERRORCODE err;
		TCHAR cmd[CMDLINESIZE];

		cmd[0] = '\0';
		GetCustData( T("X_execx"),&cmd,sizeof(cmd));
		if ( cmd[0] ){
			CHECKEXEBININFO cebinfo;

			cebinfo.info.Function = (PPXAPPINFOFUNCTION)CebInfo;
			cebinfo.info.Name = T("check");
			cebinfo.info.RegID = NilStr;
			cebinfo.name = buf;
			PP_ExtractMacro(NULL,(PPXAPPINFO *)&cebinfo,NULL,cmd,NULL,XEO_NOSCANEXE);
		}

		vft.flags = VFSFT_TYPE | VFSFT_TYPETEXT | VFSFT_EXT | VFSFT_INFO;
		err = VFSGetFileType(buf,NULL,0,&vft);
		if ( err != NO_ERROR ){
			inf = T("Unknown File Type");
		}else{
			TCHAR *pp;

			pp = info + wsprintf(info,T("%s:\n"),vft.typetext);
			if ( vft.info != NULL ){
				if ( tstrlen(vft.info) > 0x400 ){
					tstrcpy(pp,T("*info too large*"));
				}else{
					tstrcpy(pp,vft.info);
				}
				HeapFree(ProcHeap,0,vft.info);
			}
			inf = info;
		}
		wsprintf(buf,is2,path,inf);
		if ( PMessageBox(NULL, buf, is1,
				MB_ICONQUESTION | MB_DEFBUTTON2 | MB_OKCANCEL) == IDOK ){
			if ( crcs[0] == MAXSTORE ){
				memmove(&crcs[1],&crcs[2],(MAXSTORE - 1) * sizeof(DWORD));
			}else{
				crcs[0]++;
			}
			crcs[crcs[0]] = crc;
			SetCustTable( T("_Execs"),file,crcs,(crcs[0] + 1) * sizeof(DWORD));
		}else{
			return FALSE;
		}
	}
	return TRUE;
}
