/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library								PPx Task
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#pragma hdrstop

int PPxRegistCombo(HWND hWnd, TCHAR *ID, int mode);
int PPxRegistLiveSearch(TCHAR *ID, int mode);
COLORREF CJ_log[2];

ValueWinAPI(SetDllDirectory) = INVALID_VALUE(impSetDllDirectory);
DefineWinAPI(HRESULT, SetCurrentProcessExplicitAppUserModelID, (const WCHAR *AppID)) = NULL;

#ifdef _WIN64
	#define PLATFORMAPPID T("6")
#else
	#ifdef UNICODE
		#define PLATFORMAPPID T("W")
	#else
		#define PLATFORMAPPID T("A")
	#endif
#endif

WNDPROC OldJobProc;
#define JOBSELECTMODE_FOCUS 0
#define JOBSELECTMODE_PAUSE 1
#define JOBSELECTMODE_CLOSE 2

#define SHOWJOBBUTTON_MINRATE 3

int selectmode = JOBSELECTMODE_FOCUS;


//****************************************************************************
//		PPx 管理
//****************************************************************************

/*-----------------------------------------------------------------------------
 タスク管理テーブルの正当性を確認する。
-----------------------------------------------------------------------------*/
BOOL AliveCheck(ShareX *P)
{
	DWORD result;
	HANDLE hProcess;

	if ( P->ProcessID == MAX32 ) return TRUE; // まだ設定していない
	hProcess = OpenProcess(SYNCHRONIZE, FALSE, P->ProcessID);
	if ( hProcess != NULL ){
		result = WaitForSingleObject(hProcess, 0);
		CloseHandle(hProcess);
	}else{
		result = WAIT_ABANDONED;
	}
	if ( result != WAIT_TIMEOUT ){
		XMessage(NULL, NULL, XM_DbgLOG, T("Task broken:%s"), P->ID);
		return FALSE;
	}
	return TRUE;
}

void FixTask(void)
{
	ShareX *P;
	int i;
								// 共有メモリの破損チェック
	if ( Sm->Version != DLL_VersionValue ){
		XMessage(NULL, NULL, XM_FaERRld, MES_FDLW, Sm->Version);
		Sm->Version = DLL_VersionValue;
	}
								// 各項目のウィンドウが有効かのチェック
	P = Sm->P;
	UsePPx();
	for ( i = 0 ; i < X_Mtask ; i++, P++ ){
		if ( P->ID[0] == '\0' ) continue;			// 未登録
		if ( P->hWnd == BADHWND ) continue;		// 仮登録
		if ( P->hWnd == NULL ) continue;		// 仮登録/窓無し
		if ( IsTrue(IsWindow(P->hWnd)) ) continue;			// この窓は正常
		if ( AliveCheck(P) == FALSE ){	// 異常終了と見なす
			P->ID[0] = '\0';
		}else{	// 正確に求められていないので、
				// 確認を中断する。(長時間スワップの時などに発生)
			break;
		}
	}
	FreePPx();
}

/*-----------------------------------------------------------------------------
 指定IDを検索
-----------------------------------------------------------------------------*/
int SearchPPx(TCHAR *ID)
{
	int RegNo;

	FixTask();
	for ( RegNo = 0 ; RegNo < X_Mtask ; RegNo++ ){
		if ( tstrcmp(Sm->P[RegNo].ID, ID) == 0 ) break;
	}
	if ( RegNo == X_Mtask ) RegNo = -1;
	return RegNo;
}

/*-----------------------------------------------------------------------------
	指定方向でPPxを検索
-----------------------------------------------------------------------------*/
HWND PPxGetWindow(const TCHAR *refid, int d)
{
	int i;
	HWND hWnd;

	if ( refid == NULL ){
		THREADSTRUCT *Th;

		Th = GetCurrentThreadInfo();
		if ( Th == NULL ) return NULL;
		if ( Th->PPxNo < 0 ) return NULL;
		refid = Sm->P[Th->PPxNo].ID;
	}

	FixTask();
	UsePPx();
	if ( d >= 0 ){
		int found = -1;
		for ( i = 0 ; i < X_Mtask ; i++ ){
			if ( tstrcmp(Sm->P[i].ID, refid) == 0 ){
				if ( found != -1 ) break;
			}else if ( Sm->P[i].ID[0] ){
				found = i;
			}
		}
		i = found;
	}else{
		int found = -1, center = 0;
		for ( i = 0 ; i < X_Mtask ; i++ ){
			if ( tstrcmp(Sm->P[i].ID, refid) == 0 ){
				center = 1;
			}else if ( Sm->P[i].ID[0] ){
				if ( found == -1 ) found = i;
				if ( center ){
					found = i;
					break;
				}
			}
		}
		i = found;
	}
	hWnd = (i == -1) ? NULL : Sm->P[i].hWnd;
	FreePPx();
	return hWnd;
}
/*-----------------------------------------------------------------------------
	該当 ID の PPx の HWND を得る
-----------------------------------------------------------------------------*/
PPXDLL HWND PPXAPI PPxGetHWND(TCHAR *id)
{
	int i;
	HWND hWnd = NULL;

	UsePPx();
	i = SearchPPx(id);
	if ( i >= 0 ) hWnd = Sm->P[i].hWnd;
	FreePPx();
	return hWnd;
}

void SetAppID(void)
{
	TCHAR appIDname[256];

	if ( GetCustDword(T("X_tskb"), 1) == 0 ) return; // まとめないときは、ID設定不要…設定するとタスクバーのピン止めアイコンから分離するため
	// ※ショートカットに AppID を設定すればよいらしい

	GETDLLPROC(hShell32, SetCurrentProcessExplicitAppUserModelID);
	if ( DSetCurrentProcessExplicitAppUserModelID == NULL ) return;

	wsprintf(appIDname, T("PPx ") PLATFORMAPPID T("%s"), SyncTag);

#ifdef UNICODE
	DSetCurrentProcessExplicitAppUserModelID(appIDname);
#else
	{
		WCHAR namebuf[256];

		AnsiToUnicode(appIDname, namebuf, 256);
		DSetCurrentProcessExplicitAppUserModelID(namebuf);
	}
#endif
// ITaskbarList3::SetOverlayIcon
}

/*-----------------------------------------------------------------------------
	タスク管理テーブルにウィンドウを登録する
	hWND		登録時のウィンドウハンドル
				(BADHWND:ウィンドウを使用していない/一時指定)
	ID			3bytes の種別/4bytesの保存場所
	戻り値:		登録番号(0-), -1:使用済み/指定済み
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI PPxRegist(HWND hWnd, TCHAR *ID, int mode)
{
	ShareX *P;
	int i;
	int RegNo = -1;
	TCHAR limit = 'Z';

	if ( DSetDllDirectory == INVALID_HANDLE_VALUE ){
		GETDLLPROCT(hKernel32, SetDllDirectory);
		if ( DSetDllDirectory != NULL ){
			DSetDllDirectory(NilStr); // DLLの検索パスからカレントを除去する
		}
	}

	if ( mode >= PPXREGIST_COMBO_FIRST ){
		return PPxRegistCombo(hWnd, ID, mode);
	}
	FixTask();

	if ( mode <= PPXREGIST_SEARCH_LIVEID_MAX ){
		if ( mode == PPXREGIST_SETPPcFOCUS ){
			Sm->ppc.hLastFocusWnd = hWnd;
			return 0;
		}
		return PPxRegistLiveSearch(ID, mode);
	}
	UsePPx();
									// ID 検索を始める
	if ( (mode == PPXREGIST_NORMAL) || (mode == PPXREGIST_MAX) ){
		if ( mode == PPXREGIST_MAX ) limit = ID[2];
		if ( ID[2] == '\0' ) ID[2] = 'A';
	}
	ID[3] = '\0';

	P = Sm->P;
	for ( i = 0 ; i < X_Mtask ; ){
		if ( P->ID[0] == '\0' ){			// 空きを発見
			RegNo = i;
		}else if ( tstrcmp(P->ID, ID) == 0 ){	// 同ID有り
			switch ( mode ){
				case PPXREGIST_FREE:
					P->ID[0] = '\0';	// 登録を抹消する
					PPxDefInfo = &PPxDefInfoDummy;
					goto notfound;

				case PPXREGIST_SETHWND:
					P->hWnd = hWnd;		// HWND を登録
					goto notfound;

				case PPXREGIST_IDASSIGN: // 同名あり→登録できない
					goto notfound;

				default:	// PPXREGIST_NORMAL / PPXREGIST_MAX
					if ( ID[2] >= limit ){ // 最後まで検索
						if ( mode == PPXREGIST_MAX ) goto notfound;
						RegNo = i;
						i = X_Mtask;
						break;
					}
							// 次のIDの検索開始
					P = Sm->P;
					ID[2]++;
					i = 0;
					continue;
			}
		}
		i++;
		P++;
	}
	if ( (RegNo >= 0) && (mode != PPXREGIST_FREE) ){
		THREADSTRUCT *Th;
		ShareX *RegP = &Sm->P[RegNo];

		RegP->hWnd = hWnd;
		RegP->UsehWnd = BADHWND;
		RegP->ProcessID = GetCurrentProcessId();
		RegP->ActiveID = Sm->Active.high++;
		RegP->path[0] = '\0';
		tstrcpy(RegP->ID, ID);

		Th = GetCurrentThreadInfo();
		if ( Th != NULL ) Th->PPxNo = RegNo;

		// Windows7 以降のタスクバーに纏める処理
		if ( WinType >= WINTYPE_7 ) SetAppID();
	}
	FreePPx();
	return RegNo;
notfound:
	FreePPx();
	return -1;
}
/*-----------------------------------------------------------------------------
	タスク管理テーブルに登録された PPx 全てにコマンドを発行
-----------------------------------------------------------------------------*/
void USEFASTCALL ClosePPxOne(HWND hWnd)
{
	DWORD ProcessID;
	HANDLE hProcess;
	DWORD_PTR sendresult;

	if ( GetWindowThreadProcessId(hWnd, &ProcessID) == 0 ) return; // close済み
	if ( ProcessID == GetCurrentProcessId() ){ // 自分自身だった
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return;
	}else{
		SendMessageTimeout(hWnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 2000, &sendresult);
		if ( GetWindowThreadProcessId(hWnd, &ProcessID) == 0 ) return; // close済み
	}

	hProcess = OpenProcess(SYNCHRONIZE, FALSE, ProcessID);
	if ( hProcess != NULL ){
		WaitForSingleObject(hProcess, 2000);
		CloseHandle(hProcess);
	}
	Sleep(100);	// 念のためにいれておく
}


PPXDLL void PPXAPI PPxSendMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i;
	DWORD_PTR sendresult;
	HWND hWnd;

	FixTask();
	for ( i = 0 ; i < X_Mtask ; i++ ){
		if ( Sm->P[i].ID[0] == '\0' ) continue;
		hWnd = Sm->P[i].hWnd;
		if ( (hWnd == NULL) ||
			 (((LONG_PTR)hWnd & (LONG_PTR)HWND_BROADCAST) == (LONG_PTR)HWND_BROADCAST) ){
			continue;
		}
		if ( uMsg == WM_CLOSE ){
			ClosePPxOne(hWnd);
		}else{
			SendMessageTimeout(hWnd, uMsg, wParam, lParam, SMTO_ABORTIFHUNG, 2000, &sendresult);
		}
	}
}

void PPxPostMessagePPb(UINT uMsg, WPARAM wParam, int index)
{
	TCHAR buf[0x100];
	HANDLE hCommIdleEvent;
	DWORD result;

	if ( Sm->P[index].UsehWnd != BADHWND ) return;
	if ( uMsg != WM_PPXCOMMAND ) return;

	wsprintf(buf, T("%s%sR"), SyncTag, Sm->P[index].ID);
	hCommIdleEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);
	if ( hCommIdleEvent == NULL ) return;
	// PPb は[1]待機状態？
	result = WaitForSingleObject(hCommIdleEvent, 0);
	CloseHandle(hCommIdleEvent);
	if ( result == WAIT_OBJECT_0 ){
		// 待機状態なので送信
		wsprintf(buf, T(">M%d"), wParam);
		SendPPB(BADHWND, buf, index);
	}
}

/*-----------------------------------------------------------------------------
	タスク管理テーブルに登録された PPx 全てにコマンドを発行
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI PPxPostMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int index;

	FixTask();
	for ( index = 0 ; index < X_Mtask ; index++ ){
		if ( Sm->P[index].ID[0] != '\0' ){
			if ( (Sm->P[index].ID[0] != 'B') || (Sm->P[index].ID[1] != '_') ){
				PostMessage(Sm->P[index].hWnd, uMsg, wParam, lParam);
			}else{ // PPb用
				PPxPostMessagePPb(uMsg, wParam, index);

			}
		}
	}
}
/*-----------------------------------------------------------------------------
	カレントディレクトリを登録
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI PPxSetPath(int RegNo, TCHAR *path)
{
	if ( (RegNo < 0) || (RegNo >= X_Mtask) ) return;
	UsePPx();
	tstrcpy(Sm->P[RegNo].path, path);
	FreePPx();
}
//====================================================================== to PPB
/*-----------------------------------------------------------------------------
	PPb を専有する
-----------------------------------------------------------------------------*/
int UsePPb(HWND hOwner)
{
	ShareX *P;
	int id, tmpid = -1;

	P = Sm->P;
	UsePPx();
								// 空いている PPb を検索する ----
	for ( id = 0 ; id < X_Mtask ; id++, P++ ){
		if ( (P->ID[0] == 'B') && (P->ID[1] == '_') ){
			if ( P->UsehWnd != BADHWND ){	// 使用中
				if ( tmpid < 0 ){
					tmpid = -2; // 使用中ありを通知
				}
			}else{				// 未使用…処理中か確認
				HANDLE hCommEvent;
				DWORD result;
				TCHAR buf[32];

				tstrcpy(buf, SyncTag);
				tstrcat(buf, P->ID);
				hCommEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);
				if ( hCommEvent == NULL ) continue;
				result = WaitForSingleObject(hCommEvent, 0);
				CloseHandle(hCommEvent);
				if ( result == WAIT_OBJECT_0 ){	// 処理中
					if ( tmpid < 0 ){		// 仮確保
						tmpid = id;
						P->UsehWnd = hOwner;
					}
					continue;
				}
				P->UsehWnd = hOwner;
				break;
			}
		}
	}
	if ( id == X_Mtask ){		// 空きがなかったら仮確保のを使用する ---------
		id = tmpid;
	}else{
		if ( (id != tmpid) && (tmpid >= 0) ){	// 仮確保を解除
			Sm->P[tmpid].UsehWnd = BADHWND;
		}
	}
	FreePPx();
	return id;
}
/*-----------------------------------------------------------------------------
	専有したPPbを解放する
-----------------------------------------------------------------------------*/
void FreePPb(HWND hOwner, int useID)
{
	ShareX *P;

	if ( useID < 0 ) return;
	P = &Sm->P[useID];
	UsePPx();
	if ( (P->ID[0] == 'B') && (P->ID[1] == '_') && (P->UsehWnd == hOwner) ){
		P->UsehWnd = BADHWND;
	}
	FreePPx();
}
/*-----------------------------------------------------------------------------
	新規に PPb を起動する
-----------------------------------------------------------------------------*/
BOOL BootPPB(const TCHAR *path, const TCHAR *line, int flags)
{
	TCHAR param[CMDLINESIZE + VFPS * 2 + 32], *p;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
											// 実行条件の指定
	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;

	if ( flags & (XEO_MAX | XEO_MIN | XEO_NOACTIVE /* | XEO_HIDE */) ){
		si.dwFlags = STARTF_USESHOWWINDOW;
		if ( flags & XEO_MAX ) si.wShowWindow = SW_SHOWMAXIMIZED;
		if ( flags & XEO_MIN ) si.wShowWindow = SW_SHOWMINNOACTIVE;
		if ( flags & XEO_NOACTIVE ) si.wShowWindow = SW_SHOWNOACTIVATE;
	//	if ( flags & XEO_HIDE ) si.wShowWindow = SW_HIDE;
	}

	p = param + wsprintf(param, T("\"%s") T(PPBEXE) T("\" -P\"%s\" "), DLLpath, path);
	tstrcpy(p, line);
	if ( IsTrue(CreateProcess(NULL, param, NULL, NULL, FALSE,
			CREATE_DEFAULT_ERROR_MODE, NULL, DLLpath, &si, &pi)) ){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return FALSE;
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
	PPb にコマンドを送信する

PPb			PPx				hCommSendEvent	hCommIdleEvent	ParamID
[1]PPb 起動	受付待ち		---				---				free
[2]受付可能					---				[signal]		free
[3]			受付停止		---				[---]			free
[4]			内容設定		---				---				check→use
[5]			設定通知		[signal]		---				use
[6]内容受領					---				---				use→free
[7]実行						---				---				free
[8]返信						[---]			---				free
[9]再受付へ
-----------------------------------------------------------------------------*/
int SendPPB(HWND hOwner, const TCHAR *param, int useID)
{
	HANDLE hCommSendEvent, hCommIdleEvent;
	ShareX *P;
	TCHAR buf[32];

	P = &Sm->P[useID];
	UsePPx();
	if ( (P->ID[0] != 'B') || (P->ID[1] != '_') || (P->UsehWnd != hOwner) ){
		FreePPx();
		XMessage(hOwner, NULL, XM_FaERRd, T("PPB useID Destroy"));
		return -1;
	}

	tstrcpy(buf, SyncTag);
	tstrcat(buf, P->ID);					// PPb との通信手段を確保 ---------
	FreePPx();
	hCommSendEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);
	tstrcat(buf, T("R"));
	hCommIdleEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);

	if ( (hCommSendEvent == NULL) || (hCommIdleEvent == NULL) ){
		XMessage(hOwner, NULL, XM_FaERRd, T("PPB-Event destroyed/1"));
		return -1;
	}
	for ( ; ; ){
		DWORD result;			//[1]→[2] PPb が待機状態になるまで待つ ---

		if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_PUSH ) break;
		result = WaitForSingleObject(hCommIdleEvent, 100);
		if ( result == WAIT_FAILED ) goto error;
		if ( result != WAIT_OBJECT_0 ) continue;
		// [2]受付可能
		ResetEvent(hCommIdleEvent);	// [3]受付停止
										// Sm->Param に設定
		if ( TSTRLENGTH(param) >= sizeof(Sm->Param) ){
			XMessage(hOwner, NULL, XM_FaERRd, T("PPB-trans error"));
		}else{		// [4]内容設定
			if ( Sm->ParamID[0] != '\0' ){
				int count = 200;
				for (;;){
					Sleep(100);
					if ( (Sm->ParamID[0] == '\0') || (count-- == 0) ) break;
				}
			}
			tstrcpy(Sm->ParamID, P->ID);
			tstrcpy(Sm->Param, param);
			SetEvent(hCommSendEvent); // [5]設定通知
		}
		break;
	}
	CloseHandle(hCommIdleEvent);
	CloseHandle(hCommSendEvent);
	return 0;
error:
	XMessage(hOwner, NULL, XM_FaERRd, T("PPB-Event destroyed"));
	CloseHandle(hCommIdleEvent);
	CloseHandle(hCommSendEvent);
	return -1;
}

DWORD RecvPPBExitCode(HWND hOwner, int useID)
{
	TCHAR buf[32];
	HANDLE hCommSendEvent;
	int count = 2000;
	DWORD result;

	if ( SendPPB(hOwner, T("=E"), useID) != 0 ) return MAX32;

	tstrcpy(buf, SyncTag);
	tstrcat(buf, Sm->P[useID].ID);
	hCommSendEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, buf);
	if ( hCommSendEvent != NULL ){
		for ( ; ; ){
			if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_PUSH ) break;
			result = WaitForSingleObject(hCommSendEvent, 0);
			if ( result != WAIT_OBJECT_0 ) break;
			Sleep(20);
			if ( count-- == 0 ) break;
		}
		CloseHandle(hCommSendEvent);
	}
	result = *(DWORD *)Sm->Param;
	Sm->ParamID[0] = '\0';
	return result;
}
/*-----------------------------------------------------------------------------
	文字列を受信する
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI ReceiveStrings(const TCHAR *ID, TCHAR *str)
{
	UsePPx();
	if ( tstrcmp(Sm->ParamID, ID) == 0 ){
		if ( Sm->Param[0] != '=' ){ // 通常
			tstrcpy(str, Sm->Param);
			Sm->ParamID[0] = '\0';
			FreePPx();
			return 0;
		}
		// 戻り値あり
		*(DWORD *)Sm->Param = *(DWORD *)str;
		FreePPx();
		// RecvPPBExitCode で Sm->ParamID を解放する
		return -1;
	}
	// ID 不一致
	xmessage(XM_FaERRd, T("ReceiveStrings - IDerror"));
	FreePPx();
	return -1;
}
//====================================================================== to PPC
BOOL USEFASTCALL CheckPPcID(int no)
{
	TCHAR *id;

	id = Sm->P[no].ID;
	return (id[0] == 'C') && (id[1] == '_');
}

/*-----------------------------------------------------------------------------
	直前の PPC 番号を取得
	d:検索方向の指定		>0:次 <0:前 CGETW_PAIR
out	Number
-----------------------------------------------------------------------------*/
int PPcGetNo(int RegNo, int direction)
{
	TCHAR ID[REGIDSIZE];
	int i, j;

	tstrcpy(ID, Sm->P[RegNo].ID);
	FixTask();
	if ( direction == CGETW_PAIR ){
		ID[2] = (TCHAR)(((ID[2] - 1) ^ 1) + 1);
		for ( j = 0 ; j < X_Mtask ; j++ ){
			if ( !tstrcmp(Sm->P[j].ID, ID) ) break;
		}
	}else if ( direction >= 0 ){ // 増加方向
		for ( i = 0 ; i < X_MaxPPxID ; i++ ){
			ID[2]++;
			if ( ID[2] > 'Z' ) ID[2] = 'A';
			for ( j = 0 ; j < X_Mtask ; j++ ){
				if ( tstrcmp(Sm->P[j].ID, ID) == 0 ){
					i = 999;
					break;
				}
			}
		}
	}else{ // 減少方向
		for ( i = 0 ; i < X_MaxPPxID ; i++ ){
			ID[2]--;
			if ( ID[2] < 'A' ) ID[2] = 'Z';
			for ( j = 0 ; j < X_Mtask ; j++ ){
				if ( !tstrcmp( Sm->P[j].ID, ID) ){
					i = 999;
					break;
				}
			}
		}
	}
	if ( j == X_Mtask ) j = RegNo;
	return j;
}

int PPxRegistCombo(HWND hWnd, TCHAR *ID, int mode)
{
	int index;

	index = ID[2] - 'A'; // CB[A]

	if ( (index < 0) || (index >= X_MaxComboID) ){
		if ( mode != PPXREGIST_COMBO_IDASSIGN ) return 0;
		for ( index = 1 ; index < (X_MaxComboID - 1) ; index++ ){
			if ( Sm->ppc.hComboWnd[index] == NULL ) break;
			if ( IsWindow(Sm->ppc.hComboWnd[index]) == FALSE ){
				Sm->ppc.hComboWnd[index] = NULL;
				break;
			}
		}
		ID[2] = (TCHAR)('A' + index);
	}
	// PPXREGIST_COMBO_IDASSIGN / PPXREGIST_COMBO_FREE
	Sm->ppc.hComboWnd[index] = hProcessComboWnd =
			(mode == PPXREGIST_COMBO_IDASSIGN) ? hWnd : NULL;
	return 0;
}

// 指定された場所から生きているIDを検索する
int PPxRegistLiveSearch(TCHAR *ID, int mode)
{
	int index = -1;
	int delta;

	UsePPx();
	{
		int focus, pair;

		focus = Sm->ppc.LastFocusID;
		pair = PPcGetNo(focus, CGETW_PAIR);

		if ( mode == PPXREGIST_SEARCH_LIVEID ) index = focus;
		if ( mode == (PPXREGIST_SEARCH_LIVEID + 1) ) index = pair;

		if ( (index < 0) || (CheckPPcID(index) == FALSE) ){ // 使用済みIDを探す
			int left = mode - PPXREGIST_SEARCH_LIVEID;

			for ( index = 0 ; index < X_Mtask ; index++ ){
				if ( IsTrue(CheckPPcID(index)) ){
					if ( (index != focus) && (index != pair) ){
						if ( left == 0 ) break;
					}
					if ( left ) left--;
				}
			}
		}
	}

	delta = 1;
	ID[3] = '\0';
	if ( index < X_Mtask ){ // 使用済みID有り
		ID[2] = Sm->P[index].ID[2];
	}else{ // 未使用IDを探す
		ID[2] = (TCHAR)('A' + (mode - PPXREGIST_SEARCH_LIVEID));
		while ( ID[2] < 'Z' ){
			if ( SearchPPx(ID) < 0 ) break;
			ID[2]++;
			delta++;
		}
	}
	FreePPx();
	if ( (mode + delta) >= PPXREGIST_SEARCH_LIVEID_MAX ){
		delta = PPXREGIST_SEARCH_LIVEID_MAX - mode;
	}
	return delta;
}

/*-----------------------------------------------------------------------------
	PPc窓の取得
-----------------------------------------------------------------------------*/
PPXDLL HWND PPXAPI PPcGetWindow(int RegNo, int direction)
{
	if ( direction == CGETW_SAVEFOCUS ){
		if ( Sm->ppc.LastFocusID != RegNo ){
			if ( IsTrue(CheckPPcID(Sm->ppc.LastFocusID)) ){
				// SendMessageは、ShowWindow中でデッドロックが生じた
				PostMessage(Sm->P[Sm->ppc.LastFocusID].hWnd,
						WM_PPXCOMMAND, KC_UNFOCUS, 0);
			}
			if ( (RegNo >= 0) && (RegNo < X_Mtask) ){
				Sm->P[RegNo].ActiveID = Sm->Active.high++;
				Sm->ppc.PrevFocusID = Sm->ppc.LastFocusID;
				Sm->ppc.LastFocusID = RegNo;
			}
		}
		return NULL;
	}
	if ( (direction == CGETW_GETFOCUS) || (direction == CGETW_GETFOCUSPAIR) ){
		HWND hWnd;
		int index;

		index = Sm->ppc.LastFocusID;
		if ( IsWindow(Sm->P[index].hWnd) ){
			if ( direction == CGETW_GETFOCUSPAIR ){
				index = PPcGetNo(index, CGETW_PAIR);
			}
		}else{
			index = -1;
			FixTask();
		}

		UsePPx();
		if ( (index >= 0) && IsTrue(CheckPPcID(index)) ){
			hWnd = Sm->P[index].hWnd;
		}else{
			hWnd = NULL;
			for ( index = 0 ; index < X_Mtask ; index++ ){
				if ( IsTrue(CheckPPcID(index)) ){
					hWnd = Sm->P[index].hWnd;
					break;
				}
			}
		}
		FreePPx();
		return hWnd;
	}
	if ( direction == CGETW_PREV ){
		if ( Sm->P[Sm->ppc.PrevFocusID].ID[0] == 'C' ){
			return Sm->P[Sm->ppc.PrevFocusID].hWnd;
		}
	}
	if ( direction == CGETW_GETCOMBOHWND ){
		if ( (RegNo < 0) || (RegNo > 25) ) return NULL;
		return Sm->ppc.hComboWnd[RegNo];
	}

	if ( (RegNo < 0) || (RegNo >= X_Mtask) ) return NULL;
	{
		int i;
		HWND hWnd;

		UsePPx();
		i = PPcGetNo(RegNo, direction);
		if ( i == RegNo ){
			hWnd = NULL;
		}else{
			hWnd = Sm->P[i].hWnd;
		}
		FreePPx();
		return hWnd;
	}
}

/*-----------------------------------------------------------------------------
	直前パスの取得
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI PPcOldPath(INT_PTR RegNo, DWORD join, TCHAR *str)
{
	int index = -1;

	UsePPx();
	if ( join == MAX32 ){
		for ( index = 0 ; index < X_Mtask ; index++ ){
			if ( Sm->P[index].hWnd == (HWND)RegNo ){
				break;
			}
		}
	}else if ( (RegNo >= 0) && (RegNo < X_Mtask) ){
		if ( X_prtg < 0 ) X_prtg = GetCustDword(T("X_prtg"), 0);
		if ( X_prtg == 0 ){
			index = PPcGetNo((int)RegNo, CGETW_PAIR);
			if ( (join == 0) && (index == RegNo) ){
				if ( Sm->P[Sm->ppc.PrevFocusID].ID[0] == 'C' ){
					index = Sm->ppc.PrevFocusID;
				}
			}
		}else{
			if ( Sm->P[Sm->ppc.PrevFocusID].ID[0] == 'C' ){
				index = Sm->ppc.PrevFocusID;
			}else{
				index = (int)RegNo;
			}
			if ( index == RegNo ) index = PPcGetNo((int)RegNo, CGETW_PAIR);
		}
	}
	if ( (index == RegNo) || (index < 0) || (index >= X_Mtask) ){
		str[0] = '\0';
	}else{
		tstrcpy(str, Sm->P[index].path);
	}
	FreePPx();
}
//====================================================================== to PPV
/*-----------------------------------------------------------------------------
	PPV を起動する
-----------------------------------------------------------------------------*/
void bootPPV(HWND hWnd, const TCHAR *fname, int flags)
{
	TCHAR buf[VFPS * 3];
	TCHAR param[64];
	const TCHAR *option;
	STARTUPINFO			si;
	PROCESS_INFORMATION pi;
											// 実行条件の指定
	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;

	if ( flags & (PPV_NOFOREGROUND | PPV_BOOTID | PPV_SETPARENTWND) ){
		option = param;

		param[0] = '\0';
		if ( flags & PPV_NOFOREGROUND ) wsprintf(param, T(" -focus:%u "), hWnd);
		if ( flags & PPV_BOOTID ){
			wsprintf(param + tstrlen(param), T(" -bootid:%c "), (flags >> 24));
		}
		if ( flags & PPV_SETPARENTWND ){
			wsprintf(param + tstrlen(param), T(" -setparent:%u "), hWnd);
		}
	}else{
		option = NilStr;
	}

	if ( flags & PPV_PARAM ){
		wsprintf(buf, T("\"%s") T(PPVEXE) T("\" %s%s"), DLLpath, option, fname);
	}else{
		wsprintf(buf, T("\"%s") T(PPVEXE) T("\" %s\"%s\""), DLLpath, option, fname);
	}

	if ( IsTrue(CreateProcess(NULL, buf, NULL, NULL, FALSE,
				CREATE_DEFAULT_ERROR_MODE, NULL, DLLpath, &si, &pi)) ){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}else{
		PPErrorBox(NULL, buf, PPERROR_GETLASTERROR);
	}
}
/*-----------------------------------------------------------------------------
	ファイルの内容を表示する
-----------------------------------------------------------------------------*/
#define RESERVE_HWND ((HWND)(LONG_PTR)-2)
PPXDLL BOOL PPXAPI PPxView(HWND hWnd, const TCHAR *param, int flags)
{
	ShareX *FindP = NULL, *ReserveP = NULL;
	int i;

	FixTask();
	UsePPx();
	for ( i = 0 ; i < X_Mtask ; i++ ){ // 使用できるPPvを検索
		if ( (Sm->P[i].ID[0] == 'V') && (Sm->P[i].ID[1] == '_') ){
			if ( flags & PPV_BOOTID ){ // ID固定
				if ( Sm->P[i].ID[2] != (flags >> 24) ) continue;
				FindP = &Sm->P[i];
				break;
			}else{
				if ( Sm->P[i].UsehWnd == RESERVE_HWND ) continue;
			}

			if ( Sm->P[i].UsehWnd == hWnd ){
				FindP = &Sm->P[i];
				break;
			}

			if ( ReserveP == NULL ){
				ReserveP = &Sm->P[i];
			}else{
				if ( ReserveP->UsehWnd == RESERVE_HWND ) ReserveP = &Sm->P[i];
			}
		}
	}
	if ( FindP == NULL ) FindP = ReserveP;
	if ( FindP != NULL ){ // PPvを発見
		HWND hVWnd;

		Sm->view.flags = flags;
		hVWnd = FindP->hWnd;
		tstrcpy((TCHAR *)Sm->view.param, param);

		if ( (flags & (PPV_SYNCVIEW | PPV_BOOTID)) == (PPV_SYNCVIEW | PPV_BOOTID)){
			FindP->UsehWnd = RESERVE_HWND;
		}

		FreePPx();
		if ( !(flags & (PPV_NOFOREGROUND | PPV_NOENSURE)) ){
			SetForegroundWindow(hVWnd);
		}
		PostMessage(hVWnd, WM_PPXCOMMAND, KV_Load, (LPARAM)hWnd);
		return TRUE;
	}
	FreePPx();
	if ( *param == 2 ) param++;
	if ( flags & PPV_DISABLEBOOT ){
		return FALSE;
	}else{
		bootPPV(hWnd, param, flags | PPV_SETPARENTWND);
		return TRUE;
	}
}

// 指定した hWnd の PPx を占有する
PPXDLL BOOL PPXAPI BusyPPx(HWND hWnd, HWND hUseWnd)
{
	int i;

	FixTask();
	UsePPx();
	for ( i = 0 ; i < X_Mtask ; i++ ){
		if ( Sm->P[i].hWnd == hWnd ){
			Sm->P[i].UsehWnd = hUseWnd;
			FreePPx();
			return TRUE;
		}
	}
	FreePPx();
	return FALSE;
}
/*-----------------------------------------------------------------------------
	ファイル名を取得する
	0:成功 1:既に読み込んだ
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI PPvViewName(TCHAR *param)
{
	int flags;

	UsePPx();
	if ( !*Sm->view.param ){
		FreePPx();
		return -1;
	}
	tstrcpy(param, (TCHAR *)Sm->view.param);
	flags = Sm->view.flags;
	FreePPx();
	return flags;
}

// Combo Window を登録/取得する
// hWnd = NULL : 登録済み hWnd を取得、戻り値が BADHWND なら新規
// hWnd = HWND : 登録。もう一度指定すると解除
// hWnd = BADHWND : 現在のhWndを得る
PPXDLL HWND PPXAPI PPxCombo(HWND hWnd)
{
	UsePPx();

	if ( hWnd == NULL ){
		if ( Sm->ppc.hComboWnd[0] == NULL ){
			Sm->ppc.hComboWnd[0] = hProcessComboWnd = BADHWND;
		}
	}else if ( hWnd != BADHWND ){
		if ( hWnd == Sm->ppc.hComboWnd[0] ){
			Sm->ppc.hComboWnd[0] = hProcessComboWnd = NULL;
		}else{
			if ( IsWindow(Sm->ppc.hComboWnd[0]) == FALSE ){ // Combo window が壊れているか？
				Sm->ppc.hComboWnd[0] = hProcessComboWnd = NULL;
			}
			if ( (Sm->ppc.hComboWnd[0] == NULL) || (Sm->ppc.hComboWnd[0] == BADHWND) ){
				Sm->ppc.hComboWnd[0] = hProcessComboWnd = hWnd;
			}
		}
	}
	FreePPx();
	return Sm->ppc.hComboWnd[0];
}

int GetPPxListFromCombo(HMENU hPopupMenu, int mode, ThSTRUCT *thMenuData, TCHAR *ppclist, DWORD *useppclist, MENUITEMINFO *minfo)
{
	TCHAR *listIDptr, *listPathPtr;
	int item = 0;
	TCHAR buf2[VFPS];

	if ( ppclist == NULL ) return 0;

	listIDptr = ppclist;
	for ( ; *listIDptr != '\0'; ){
		listPathPtr = listIDptr + tstrlen(listIDptr) + 1;
		if ( *listIDptr == '\t' ){
			if ( hPopupMenu ) AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
		}else{
			setflag(*useppclist, 1 << (listIDptr[2] - 'A'));
			if ( hPopupMenu != NULL ){
				if ( mode == GetPPcList_Path ){
					if ( thMenuData != NULL ){
						ThAddString(thMenuData, EscapeMacrochar(listPathPtr, minfo->dwTypeData));
					}
					wsprintf(minfo->dwTypeData, T("%c&%s: %s"), *listIDptr, listIDptr + 2, listPathPtr);
				}else{
					wsprintf(minfo->dwTypeData, T("PPc %c&%s: %s"), *listIDptr, listIDptr + 2, listPathPtr);
				}
				minfo->fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_DATA;
				minfo->dwItemData = 0;
				InsertMenuItem(hPopupMenu, 0, FALSE, minfo);
				++minfo->wID;
			}
			if ( thMenuData != NULL ){
				wsprintf(buf2, T("C_%s"), listIDptr + 2);
				switch ( mode ){
					case GetPPxList_IdList:
						tstrcat(buf2, T(","));
						ThCatString(thMenuData, buf2);
						break;
					case GetPPxList_Id:
						ThAddString(thMenuData, buf2);
						break;
					case GetPPxList_Select:
						wsprintf(minfo->dwTypeData, T("*focus %s"), buf2);
						ThAddString(thMenuData, minfo->dwTypeData);
						break;
				}
			}
			item++;
		}
		listIDptr = listPathPtr + tstrlen(listPathPtr) + 1;
	}
	HeapFree(DLLheap, 0, ppclist);
	if ( (mode != GetPPcList_Path) && (hPopupMenu != NULL) ){
		AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
	}
	return item;
}

int GetPPxListFromCurrentCombo(HMENU hPopupMenu, int mode, ThSTRUCT *th, DWORD *useppclist, MENUITEMINFO *minfo)
{
	return GetPPxListFromCombo(hPopupMenu, mode, th, (TCHAR *)SendMessage(hProcessComboWnd, WM_PPXCOMMAND, KCW_ppclist, 0), useppclist, minfo);
}

int GetPPxListFromProcessCombo(HMENU hPopupMenu, int mode, ThSTRUCT *th, DWORD *useppclist, MENUITEMINFO *minfo, HWND hComboWnd)
{
	LRESULT lr;
	TCHAR buf1[VFPS + 16], buf2[VFPS];
	TCHAR *ppclist;
	DWORD size;
	ERRORCODE ec;

	lr = SendMessage(hComboWnd, WM_PPXCOMMAND, TMAKEWPARAM(KCW_ppclist, 1), 0);
	if ( lr == 0 ) return 0;

	wsprintf(buf1, T("%%temp%%\\ppxl%d"), (int)lr);
	ExpandEnvironmentStrings(buf1, buf2, TSIZEOF(buf2));

	ppclist = NULL;
	ec = LoadFileImage(buf2, 4, (char **)&ppclist, &size, LFI_ALWAYSLIMITLESS);
	DeleteFile(buf2);
	if ( ec != NO_ERROR ) return 0;

	return GetPPxListFromCombo(hPopupMenu, mode, th, ppclist, useppclist, minfo);
}

/*
	GetPPcList_Path なら "x&ID: path" 形式
	他 mode なら "PPc x&ID: path" 又は "PPx ID: & " 形式
*/
int GetPPxList(HMENU hPopupMenu, int mode, ThSTRUCT *th, DWORD *menuid)
{
	int i, minpos, pos, items = 0;
	MENUITEMINFO minfo;
	ShareX *sx;
	TCHAR buf1[VFPS + 16], buf2[VFPS];
	DWORD useppclist = 0; // 現プロセスで使用しているPPcのID一覧

	minfo.cbSize = sizeof(minfo);
	minfo.fType = MFT_STRING;
	minfo.fState = MFS_ENABLED;
	minfo.dwTypeData = buf1;
	minfo.wID = *menuid;

	// 自己一体化窓列挙
	if ( hProcessComboWnd != NULL ){
		items += GetPPxListFromCurrentCombo(hPopupMenu, mode, th, &useppclist, &minfo);
	}

	// 一体化窓列挙
	for ( i = 0 ; i < X_MaxComboID ; i++ ){
		HWND hComboWnd;

		hComboWnd = Sm->ppc.hComboWnd[i];
		if ( (hComboWnd == NULL) || (hComboWnd == BADHWND) || (hComboWnd == hProcessComboWnd) ){
			continue;
		}
		items += GetPPxListFromProcessCombo(hPopupMenu, mode, th, &useppclist, &minfo, hComboWnd);
	}

	minpos = GetMenuItemCount(hPopupMenu);

	// 独立窓列挙
	UsePPx();
	sx = Sm->P;
	for ( i = 0 ; i < X_Mtask ; i++, sx++ ){
		TCHAR *name, ID;

		ID = sx->ID[0];
		if ( ID != '\0' ){
			if ( (ID == 'C') && (useppclist & (1 << (sx->ID[2] - 'A'))) ){
				continue;
			}

			if ( mode == GetPPcList_Path ){
				if ( (ID != 'C') || (sx->ID[1] != '_') ) continue;
				if ( th != NULL ){
					ThAddString(th, EscapeMacrochar(sx->path, buf1));
				}
				wsprintf(buf1, T("%c&%c: %s"),
						(sx->hWnd == Sm->ppc.hLastFocusWnd) ? '*' : ' ',
						sx->ID[2], sx->path);
			}else{
				switch ( ID ){
					case 'B':
						name = T("PPb");
						break;
					case 'C':
						name = T("PPc");
						break;
					case 'V':
						name = T("PPv");
						break;
					case 'c':
						name = T("PPcust");
						break;
					default:
						continue;
				}
				if ( (ID == 'C') && (sx->ID[2] <= 'Z') ){
					wsprintf(buf1, T("%s %c&%c: %s"), name,
							(sx->hWnd == Sm->ppc.hLastFocusWnd) ? '*' : ' ',
							sx->ID[2], sx->path);
				}else {
					wsprintf(buf1, T("%s %c: & "), name, sx->ID[2]);
				}
			}
			for ( pos = minpos ; pos < (minpos + items) ; pos++ ){ // 簡易ソート
				GetMenuString(hPopupMenu, pos, buf2, TSIZEOF(buf2), MF_BYPOSITION);
				if ( tstrcmp(buf1, buf2) < 0 ) break;
			}
			if ( mode == GetPPxList_hWnd ){
				minfo.dwItemData = (ULONG_PTR)sx->hWnd;
			}else{
				if ( th != NULL ) switch ( mode ){
					case GetPPxList_Id:
						ThAddString(th, sx->ID);
						break;
					case GetPPxList_Select:
						wsprintf(buf2, T("*focus %s"), sx->ID);
						ThAddString(th, buf2);
						break;
				}
			}
			minfo.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_DATA;
			InsertMenuItem(hPopupMenu, pos, TRUE, &minfo);
			minfo.wID++;
			items++;
			continue;
		}
	}
	FreePPx();
	*menuid = minfo.wID;
	return items;
}

//****************************************************************************
//		Job 管理
//****************************************************************************
void JobListItemDraw(DRAWITEMSTRUCT *lpdis)
{
	TCHAR buf[CMDLINESIZE];
	int len;
	COLORREF backcolor = C_AUTO;
	HBRUSH hB;
	DWORD state = 0;
	int index, rate = 0, height, width;
	RECT box;

	if ( (int)lpdis->itemID >= 0 ){
		UsePPx();
		index = (int)lpdis->itemData;
		if ( (index >= 0) && (index < X_MaxJob) && (Sm->Job[index].ThreadID != 0) ){
			state = Sm->Job[index].state.dw;
			rate = Sm->Job[index].state.b.rate;
		}
		FreePPx();

		// 背景色決定・描画
		if ( state & (JOBFLAG_WAITEXEC | JOBFLAG_WAITJOB | JOBFLAG_PAUSE | JOBFLAG_ERROR) ){
			COLORREF c;

			if ( backcolor == C_AUTO ) backcolor = GetBkColor(lpdis->hDC);
			if ( state & (JOBFLAG_ERROR | JOBFLAG_PAUSE) ){
				c = (state & JOBFLAG_ERROR) ? C_RED : C_YELLOW;
			}else{
				c = (state & JOBFLAG_WAITEXEC) ? C_GREEN : C_CYAN;
			}
			#define GetPointColor(bc, fc) (BYTE)(int)((int)bc + ((int)fc - (int)bc) / 4)
			backcolor = RGB(
				GetPointColor(GetRValue(backcolor), GetRValue(c)),
				GetPointColor(GetRValue(backcolor), GetGValue(c)),
				GetPointColor(GetBValue(backcolor), GetBValue(c)));
		}
		if ( backcolor != C_AUTO ){
			hB = CreateSolidBrush(backcolor);
			FillBox(lpdis->hDC, &lpdis->rcItem, hB);
			DeleteObject(hB);
			SetBkMode(lpdis->hDC, TRANSPARENT);
		}else{
			SetBkMode(lpdis->hDC, OPAQUE);
		}

		// 処理グラフの描画
		height = lpdis->rcItem.bottom - lpdis->rcItem.top;
		width = lpdis->rcItem.right - lpdis->rcItem.left;
		if ( rate ){
			box.left = lpdis->rcItem.left;
			box.top = lpdis->rcItem.bottom - (height / 4);
			box.bottom = lpdis->rcItem.bottom - 1;
			box.right = lpdis->rcItem.left + (width * rate) / 100;
			FillBox(lpdis->hDC, &box, GetHighlightBackBrush());
			SetBkMode(lpdis->hDC, TRANSPARENT);
		}

		// マウス操作用ボタンの描画
		box = lpdis->rcItem;
		if ( (height * SHOWJOBBUTTON_MINRATE) < width ){
			int ah;

			box.left = box.right - height;
			// フォーカス有り
			if ( (selectmode == JOBSELECTMODE_PAUSE) &&
				 (lpdis->itemState & ODS_SELECTED) ){
				box.top++;
				box.bottom--;
				box.right--;
				FrameRect(lpdis->hDC, &box, GetStockObject(DKGRAY_BRUSH));
				box.right++;
				box.bottom++;
				box.top--;
			}
			ah = height / 4;
			box.top += ah;
			box.left += ah;
			box.right -= ah;
			box.bottom -= ah;
			if ( state & JOBFLAG_PAUSE ){ // 中断中→再生ボタンを表示
				HGDIOBJ hOldBrush;
				POINT pos[3];

				hOldBrush = SelectObject(lpdis->hDC, GetStockObject(BLACK_BRUSH));
				pos[0].x = pos[2].x = box.left;
				pos[0].y = box.top;
				pos[1].x = box.right;
				pos[1].y = box.top + ah;
				pos[2].y = box.bottom;
				Polygon(lpdis->hDC, pos, 3);
				SelectObject(lpdis->hDC, hOldBrush);
			}else{ // 停止ボタン
				FillBox(lpdis->hDC, &box, GetStockObject(BLACK_BRUSH));
			}
			box = lpdis->rcItem;
			box.right -= height;
		}

		// 表示文字列を描画
		len = (int)SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)buf);
		DrawText(lpdis->hDC, buf, len, &box, DT_END_ELLIPSIS | DT_NOPREFIX);
		SetBkMode(lpdis->hDC, OPAQUE);
	}

	// フォーカス枠を描画
	if ( lpdis->itemState & ODS_FOCUS ){
		FrameRect(lpdis->hDC, &lpdis->rcItem, GetStockObject(DKGRAY_BRUSH));
	}
}

void JobListSelect(HWND hWnd, int listindex, int mode)
{
	HWND hTargetWnd;
	int state;

	int index = (int)SendMessage(hWnd, LB_GETITEMDATA, (WPARAM)listindex, 0);
//	index = CallWindowProc(OldJobProc, hWnd, LB_GETITEMDATA, (WPARAM)listindex, 0); ←こちらだと失敗する。
	if ( (listindex < 0) && (listindex >= X_MaxJob) ) return;

	UsePPx();
	hTargetWnd = Sm->Job[index].hWnd;
	state = Sm->Job[index].state.dw;
	FreePPx();

	if ( hTargetWnd == NULL ) return;

	if ( !(GetWindowLongPtr(hTargetWnd, GWL_STYLE) & WS_VISIBLE) ){
		ShowWindow(hTargetWnd, SW_RESTORE);
	}

	if ( mode == JOBSELECTMODE_FOCUS ){
		ShowWindow(hTargetWnd, SW_SHOWNORMAL);
		SetForegroundWindow(hTargetWnd);
	}else{
		if ( state & JOBFLAG_ENABLEHIDE ){
			SendMessage(hTargetWnd, WM_COMMAND,
					(mode == JOBSELECTMODE_CLOSE) ? IDCANCEL : IDOK, 0);
		}else{
			PostMessage(hTargetWnd, WM_KEYDOWN, VK_PAUSE, 0);
		}
	}
}

void JobListButtonUp(HWND hWnd, LPARAM lParam)
{
	int index, height, mode = JOBSELECTMODE_FOCUS;
	RECT box;

	index = (int)CallWindowProc(OldJobProc, hWnd, LB_ITEMFROMPOINT, 0, lParam);
	if ( (index < 0) || HIWORD(index) ) return;

	CallWindowProc(OldJobProc, hWnd, LB_GETITEMRECT, 0, (LPARAM)&box);
	#pragma warning(suppress:6001) // 前の行で初期化
	height = box.bottom - box.top;
	if ( (height * SHOWJOBBUTTON_MINRATE) < (box.right - box.left) ){
		int x = LOSHORTINT(lParam);

		x -= box.right - (height * 1);
		if ( x >= 0 ){
			mode = (x / height) + 1;
			x = x % height;
			if ( (x < 1) || (x >= (height - 1)) ){
				return; // ボタンからずれているので無視する
			}
		}
	}
	JobListSelect(hWnd, index, mode);
}

LRESULT CALLBACK JobListWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_KEYDOWN:
			if ( (wParam == VK_LEFT) || (wParam == VK_RIGHT) ){
				if ( wParam == VK_LEFT ){
					selectmode--;
					if ( selectmode < JOBSELECTMODE_FOCUS ){
//						selectmode = JOBSELECTMODE_CLOSE;
						selectmode = JOBSELECTMODE_PAUSE;
					}
				}else{
					selectmode++;
//					if ( selectmode > JOBSELECTMODE_CLOSE ){
					if ( selectmode > JOBSELECTMODE_PAUSE ){
						selectmode = JOBSELECTMODE_FOCUS;
					}
				}
				InvalidateRect(hWnd, NULL, TRUE);
				return 0;
			}

			if ( (wParam == VK_SPACE) || (wParam == VK_RETURN) ){
				int index = (int)CallWindowProc(OldJobProc, hWnd, LB_GETCURSEL, 0, 0);

				if ( index >= 0 ) JobListSelect(hWnd, index, selectmode);
				return 0;
			}
			if ( wParam == VK_ESCAPE ){
				HWND hPWnd = GetParent(hWnd);

				if ( Sm->JobList.showmode == JOBLIST_INNER ){
					SetFocus(GetParent(hPWnd));
				}else{
					PostMessage(hPWnd, WM_CLOSE, 0, 0);
				}
				return 0;
			}
			break;

		case WM_LBUTTONUP:
			JobListButtonUp(hWnd, lParam);
			break;
	}
	return CallWindowProc(OldJobProc, hWnd, iMsg, wParam, lParam);
}

void DestroyJobList(HWND hWnd)
{
	int index;
	JobX *job = Sm->Job;
	WINPOS WPos;
	WINDOWPLACEMENT wp;
	HFONT hMesFont;
	HBRUSH hJobBackBrush;

	for ( index = 0 ; index < X_MaxJob ; index++, job++ ){
		if ( job->ThreadID == 0 ) continue;
		if ( !(job->state.dw & JOBFLAG_ENABLEHIDE) ) continue;
		if ( !(GetWindowLongPtr(job->hWnd, GWL_STYLE) & WS_VISIBLE) ){
			ShowWindow(job->hWnd, SW_SHOWNOACTIVATE);
		}
	}
	Sm->JobList.hWnd = NULL;
	hMesFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
	if ( hMesFont != NULL ) DeleteObject(hMesFont);

	hJobBackBrush = (HBRUSH)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if ( hJobBackBrush != NULL ) DeleteObject(hJobBackBrush);

	if ( Sm->JobList.showmode != JOBLIST_INNER ){
		wp.length = sizeof(wp);
		GetWindowPlacement(hWnd, &wp);
		if ( wp.showCmd == SW_SHOWNORMAL ){
			WPos.show = (BYTE)wp.showCmd;
			WPos.reserved = 0;
			GetWindowRect(hWnd, &WPos.pos);
			SetCustTable(T("_WinPos"), T("JobLst"), &WPos, sizeof(WPos));
		}
		PostQuitMessage(EXIT_SUCCESS);
	}
}

LRESULT CALLBACK JobListParentWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_DRAWITEM:
			JobListItemDraw((DRAWITEMSTRUCT *)lParam);
			break;

		case WM_DESTROY:
			DestroyJobList(hWnd);
			break;

		case WM_CTLCOLORLISTBOX: {
			LRESULT hJobBackBrush = GetWindowLongPtr(hWnd, GWLP_USERDATA);

			if ( hJobBackBrush != 0 ){
				SetTextColor((HDC)wParam, CJ_log[0]);
				SetBkColor((HDC)wParam, CJ_log[1]);
				return hJobBackBrush;
			}
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		case WM_SIZE:
			SetWindowPos(Sm->JobList.hWnd, NULL,
				0, 0, LOWORD(lParam), HIWORD(lParam),
				SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		// default へ

		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}
	return 0;
}

#define JOBINFOTEXTSIZE (VFPS + 32)
void MakeJobInfoText(HWND hWnd, DWORD state, TCHAR *text)
{
	TCHAR *p;
	const TCHAR *sstr = NilStr;
	DWORD statetype;

	if ( state & (JOBFLAG_ERROR | JOBFLAG_PAUSE | JOBFLAG_WAITEXEC | JOBFLAG_WAITJOB) ){
		if ( state & (JOBFLAG_ERROR | JOBFLAG_PAUSE) ){
			sstr = (state & JOBFLAG_ERROR) ? MES_JLER : MES_JLPA;
		}else{
			sstr = (state & JOBFLAG_WAITEXEC) ? MES_JLEX : MES_JLWA;
		}
		sstr = MessageText(sstr);
	}
	statetype = state & JOBFLAG_NAMEMASK;
	if ( statetype >= JOBSTATE_MAX ) statetype = JOBSTATE_NORMAL;
	p = text + wsprintf(text, T("%s%s:"), sstr, MessageText(JobTypeNames[statetype]) );
	if ( hWnd == NULL ){
		THREADSTRUCT *ts;

		UsePPx();
		ts = GetCurrentThreadInfo();
		if ( ts != NULL ) tstrcpy(p, ts->ThreadName);
		FreePPx();
	}else{
		GetWindowText(hWnd, p, JOBINFOTEXTSIZE - (p - text));
		if ( Isdigit(*p) ){
			TCHAR *p2;

			if ( (p2 = tstrchr(p, '/')) != NULL ){
				p2 += 1;
				memmove(p, p2, TSTRSIZE(p2));
			}
		}
	}
}

void USEFASTCALL AddJobListStr(int i, const TCHAR *text, int posindex)
{
	int index;

	index = (int)SendMessage(Sm->JobList.hWnd, LB_INSERTSTRING, (WPARAM)posindex, (LPARAM)text);
	SendMessage(Sm->JobList.hWnd, LB_SETITEMDATA, (WPARAM)index, (LPARAM)i);
}

void ShowJobListWnd(HWND hWnd)
{
	HWND hParent;
	RECT targetbox, box;

	if ( Sm->JobList.showmode == JOBLIST_INNER ) return;

	hParent = GetParent(Sm->JobList.hWnd);
	if ( X_jlst[0] != JOBLIST_FLOAT ){
		ShowWindow(hParent, SW_SHOWNOACTIVATE);
		return;
	}

	if ( hWnd == NULL ) hWnd = PPcGetWindow(0, CGETW_GETFOCUS);
	if ( hWnd == NULL ){
		hWnd = GetForegroundWindow();
	}else{
		HWND hWParent;

		for (;;){
			hWParent = GetParent(hWnd);
			if ( hWParent == NULL ) break;
			hWnd = hWParent;
		}
	}
	GetWindowRect(hWnd, &targetbox);
	GetWindowRect(hParent, &box);

	SetWindowPos(hParent, HWND_TOP,
			targetbox.right,
			targetbox.bottom - (box.bottom - box.top),
			0, 0, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSIZE);

	if ( SendMessage(Sm->JobList.hWnd, LB_GETCURSEL, 0, 0) == LB_ERR ){
		SendMessage(Sm->JobList.hWnd, LB_SETCURSEL, 0, 0);
	}
}

void InitJobList(HWND hMainWnd)
{
	WNDCLASS wcClass;
	HWND hParentWnd;
	DWORD style;
	WINPOS WPos = {{0, 0, 200, 100}, 0, 0};

	wcClass.style			= 0;
	wcClass.lpfnWndProc		= JobListParentWndProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= 0;
	wcClass.hInstance		= DLLhInst;
	wcClass.hIcon			= LoadIcon(DLLhInst, MAKEINTRESOURCE(Ic_FOP));
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= (HBRUSH)(LONG_PTR)(COLOR_WINDOW + 1);
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= T("Jobist");
											// クラスを登録する
	RegisterClass(&wcClass);

	if ( hMainWnd == NULL ){
		style = WS_OVERLAPPEDWINDOW;
	}else{
		style = WS_CHILD | WS_VISIBLE;
	}
	GetCustTable(T("_WinPos"), T("JobLst"), &WPos, sizeof(WPos));
	WPos.pos.right -= WPos.pos.left;
	if ( WPos.pos.right < 32 ) WPos.pos.right = 200;
	WPos.pos.bottom -= WPos.pos.top;
	if ( WPos.pos.bottom < 32 ) WPos.pos.bottom = 100;

	hParentWnd = CreateWindowEx(WS_EX_TOOLWINDOW, T("Jobist"), T("PPx job"),
			style, WPos.pos.left, WPos.pos.top,
			WPos.pos.right, WPos.pos.bottom, hMainWnd, NULL, DLLhInst, NULL);

	GetCustData(T("CC_log"), &CJ_log, sizeof(CJ_log));

	Sm->JobList.hWnd = CreateWindowEx(0, LISTBOXstr, NilStr,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
			0, 0, 200, 100, hParentWnd, NULL, DLLhInst, NULL);

	if ( Sm->JobList.hWnd != NULL ){
		HBRUSH hJobBackBrush = NULL;
		LOGFONTWITHDPI cursfont;

		OldJobProc = (WNDPROC)SetWindowLongPtr(
				Sm->JobList.hWnd, GWLP_WNDPROC, (LONG_PTR)JobListWndProc);

		GetPPxFont(PPXFONT_F_mes, GetMonitorDPI(hParentWnd), &cursfont);

		SendMessage(Sm->JobList.hWnd, WM_SETFONT, (WPARAM)
			CreateFontIndirect(&cursfont.font), TMAKELPARAM(TRUE, 0));
		if ( (hMainWnd != NULL) && (CJ_log[0] != C_AUTO) || (CJ_log[1] != C_AUTO) ){
			InitSysColors();
			if ( CJ_log[0] == C_AUTO ) CJ_log[0] = C_WindowText;
			if ( CJ_log[1] == C_AUTO ) CJ_log[1] = C_WindowBack;
			hJobBackBrush = CreateSolidBrush(CJ_log[1]);
		}
		SetWindowLongPtr(hParentWnd, GWLP_USERDATA, (LONG_PTR)hJobBackBrush);
		ShowJobListWnd(NULL);
	}
}

#pragma argsused
DWORD WINAPI JobListThread(LPVOID param)
{
	MSG msg;
	THREADSTRUCT threadstruct = {T("Joblist"), XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};
	UnUsedParam(param);

	PPxRegisterThread(&threadstruct);

	InitJobList(NULL);

	if ( Sm->JobList.hWnd != NULL ){
		for ( ; ; ){
			if ( (int)GetMessage(&msg, NULL, 0, 0) <= 0 ) break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sm->JobList.hWnd = NULL;
	}
	PPxUnRegisterThread();
	return 0;
}

void CreateJobListWnd(void)
{
	DWORD tid;
	HWND hMainWnd = Sm->JobList.hWnd;

	X_jlst[0] = JOBLIST_OFF;
	GetCustData(T("X_jlst"), X_jlst, sizeof(X_jlst));

	Sm->JobList.hWnd = NULL;
	Sm->JobList.hidemode = X_jlst[1];
	if ( hMainWnd == NULL ){
		int i;

		CloseHandle(CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE)JobListThread, 0, 0, &tid));
		for ( i = 0 ; i < 60 ; i++ ){
			Sleep(50);
			if ( Sm->JobList.hWnd != NULL ) break;
		}
	}else{
		InitJobList(hMainWnd);
	}
	if ( Sm->JobList.hWnd != NULL ){ // 既存の内容を一括登録
		int i;

		for ( i = 0 ; i < X_MaxJob ; i++ ){
			TCHAR buf[VFPS * 2];
			HWND hJobWnd;
			DWORD state;

			if ( Sm->Job[i].ThreadID == 0 ) continue;
			hJobWnd = Sm->Job[i].hWnd;
			state = Sm->Job[i].state.dw;
			MakeJobInfoText(hJobWnd, state, buf);
			AddJobListStr(i, buf, -1);
		}
	}
}

void CheckJobList(void)
{
	int i;
	JobX *job;

	job = Sm->Job;
	for ( i = 0 ; i < X_MaxJob ; i++, job++ ){
		if ( job->ThreadID &&
			 (job->hWnd != NULL) &&
			 (IsWindow(job->hWnd) == FALSE) ){
			job->ThreadID = 0;
		}
	}
}

void StartNextJobTask(void)
{
	int i, index C4701CHECK;
	HWND hNextJobWnd;

	if ( GetCustDword(T("X_fopw"), 0) != 2 ) return;
	hNextJobWnd = NULL;
	UsePPx();
	for ( i = 0 ; i < X_MaxJob ; i++ ){
		if ( Sm->Job[i].ThreadID == 0 ) continue;
		// プロセス終了完了待ちは対象外
		if ( Sm->Job[i].state.dw & JOBFLAG_WAITEXEC ) continue;

		if ( Sm->Job[i].state.dw & JOBFLAG_WAITJOB ){ // 実行待ちなら覚えておく
			if ( hNextJobWnd == NULL ){
				if ( IsTrue(IsWindow(Sm->Job[i].hWnd)) ){
					index = i;
					hNextJobWnd = Sm->Job[i].hWnd;
				}
			}
		}else if ( Sm->Job[i].state.dw & JOBFLAG_ENABLEWAIT ){ // Job実行有り→待ち
			FreePPx();
			return;
		}
	}
	FreePPx();

	if ( hNextJobWnd != NULL ){
		DWORD_PTR sendresult = 0;

		SendMessageTimeout(hNextJobWnd, WM_COMMAND, IDM_CONTINUE, 0, SMTO_ABORTIFHUNG, 2000, &sendresult);
		if ( sendresult != (DWORD_PTR)IDM_CONTINUE ){
			// 実行失敗
			UsePPx();
			if ( Sm->Job[index].hWnd == hNextJobWnd ){  // C4701ok
				resetflag(Sm->Job[index].state.dw, JOBFLAG_WAITJOB);
			}
			FreePPx();

			CheckJobList();
			StartNextJobTask();
		}
	}
}

int USEFASTCALL FindJobListIndex(int i)
{
	int index = 0, li;

	for ( ; ; ){
		li = (int)SendMessage(Sm->JobList.hWnd, LB_GETITEMDATA, (WPARAM)index, 0);
		if ( li < 0 ) return -1;
		if ( li == i ) return index;
		index++;
	}
}

void DeleteJobTaskMain(int i)
{
	int index;

	if ( Sm->JobList.hWnd == NULL ) return;

	index = FindJobListIndex(i);
	if ( index < 0 ) return;

	SendMessage(Sm->JobList.hWnd, LB_DELETESTRING, (WPARAM)index, 0);
	if ( (Sm->JobList.showmode != JOBLIST_INNER) &&
		 ((int)SendMessage(Sm->JobList.hWnd, LB_GETCOUNT, 0, 0) <= 0) ){
		ShowWindow(GetParent(Sm->JobList.hWnd), SW_HIDE);
	}
}

BOOL SetJobTask(HWND hWnd, DWORD state)
{
	DWORD i, challenge, ThreadID;

	TCHAR buf[JOBINFOTEXTSIZE];

	if ( ((state & JOBFLAG_STATEMASK) == JOBSTATE_SETRATE) &&
		 (Sm->JobList.hWnd == NULL) ){
		return FALSE;
	}

	ThreadID = GetCurrentThreadId();
	UsePPx();
	// 既存の登録があるか
	for ( i = 0 ; i < X_MaxJob ; i++ ){
		JobX *jxp = &Sm->Job[i];

		if ( jxp->ThreadID != ThreadID ) continue;

		if ( state & (JOBFLAG_REG | JOBFLAG_UNREG) ){
			if ( state & JOBFLAG_UNREG ){ // 終了系
				resetflag(jxp->state.dw, state);
				if ( !(state & (JOBFLAG_WAITEXEC | JOBFLAG_WAITJOB | JOBFLAG_PAUSE | JOBFLAG_ERROR)) && (jxp->state.b.count == 0) ){ // 参照0なら削除
					FreePPx();
					jxp->ThreadID = 0;
					DeleteJobTaskMain(i);
					return TRUE;
				}
				if ( !(state & JOBFLAG_CHANGESTATE) ) jxp->state.b.count--;
			}else{	// 登録系 JOBFLAG_REG
				jxp->hWnd = hWnd;
				setflag(jxp->state.dw, state);
				if ( !(state & JOBFLAG_CHANGESTATE) ) jxp->state.b.count++;
			}
			FreePPx();
			if ( Sm->JobList.hWnd != NULL ){ // 一覧の内容を更新
				int index = FindJobListIndex(i);
				int listindex = (int)SendMessage(Sm->JobList.hWnd, LB_GETCURSEL, 0, 0);

				MakeJobInfoText(hWnd, jxp->state.dw, buf);
				if ( index >= 0 ){
					SendMessage(Sm->JobList.hWnd, LB_DELETESTRING, (WPARAM)index, 0);
				}
				AddJobListStr(i, buf, index);
				if ( listindex == LB_ERR ) listindex = 0;
				SendMessage(Sm->JobList.hWnd, LB_SETCURSEL, listindex, 0);
				InvalidateRect(Sm->JobList.hWnd, NULL, TRUE);
			}
			return TRUE;
		}
//		if ( state & JOBSTATE_SETRATE ){
			jxp->state.b.rate = (BYTE)state;
//			goto fin;
//		}
		goto fin;
	}
	if ( state & JOBFLAG_UNREG ){ // 登録が不要なものはここで終わり
		goto fin;
	}

	if ( X_jlst[0] < 0 ){
		X_jlst[0] = JOBLIST_OFF;
		GetCustData(T("X_jlst"), X_jlst, sizeof(X_jlst));
	}

	// 新規登録 (限度一杯なら 30 秒待つ)
	for ( challenge = 30 * (1000 / 100) ; challenge ; challenge-- ){
		for ( i = 0 ; i < X_MaxJob ; i++ ){
			if ( Sm->Job[i].ThreadID != 0 ) continue;
			Sm->Job[i].ThreadID = ThreadID;
			Sm->Job[i].hWnd = hWnd;
			Sm->Job[i].state.dw = state;

			FreePPx();
			if ( Sm->JobList.hWnd == NULL ){
				if ( X_jlst[0] != JOBLIST_OFF ){
					Sm->JobList.showmode = 0;
					CreateJobListWnd();
				}
			}else{
				MakeJobInfoText(hWnd, state, buf);
				AddJobListStr(i, buf, -1);
				ShowJobListWnd(hWnd);
			}
			return TRUE;
		}
		CheckJobList();
		Sleep(100);
	}
	// 登録できなかった→管理対象外として実行
fin:
	FreePPx();
	if ( Sm->JobList.hWnd != NULL ) InvalidateRect(Sm->JobList.hWnd, NULL, FALSE);
	return FALSE;
}

void DeleteJobTask(void)
{
	int i;
	DWORD ThreadID;

	UsePPx();
	ThreadID = GetCurrentThreadId();
	for ( i = 0 ; i < X_MaxJob ; i++ ){
		if ( Sm->Job[i].ThreadID == ThreadID ){
			Sm->Job[i].ThreadID = 0;
			DeleteJobTaskMain(i);
			goto fin;
		}
	}
fin:
	FreePPx();
	StartNextJobTask();
}

BOOL CheckJobWait(HANDLE hHandle)
{
	if ( hHandle != NULL ){ // X_fopw == 1
		return (WaitForSingleObject(hHandle, 0) != WAIT_OBJECT_0);
	}else{ // X_fopw == 2
		int i;

		if ( DOpenThread == INVALID_HANDLE_VALUE ){
			GETDLLPROC(hKernel32, OpenThread);
		}

		for ( i = 0 ; i < X_MaxJob ; i++ ){
			if ( Sm->Job[i].ThreadID == 0 ) continue; // 未登録
			if ( !(Sm->Job[i].state.dw & JOBFLAG_ENABLEWAIT) ){ // 待機対象外
				continue;
			}
			// 自分自身は対象外
			if ( Sm->Job[i].ThreadID == GetCurrentThreadId() ) continue;

			if ( DOpenThread != NULL ){
				HANDLE hThread;

				hThread = DOpenThread(
						STANDARD_RIGHTS_REQUIRED | THREAD_TERMINATE,
						FALSE, Sm->Job[i].ThreadID);
				if ( hThread != NULL ){ // スレッドは有効
					CloseHandle(hThread);
				}else{
					Sm->Job[i].ThreadID = 0;
					continue;
				}
			}
			return TRUE; // 実行中があるので、確定終了
		}
		return FALSE; // 実行中のものはなかった
	}
}

BOOL JobListMenu(EXECSTRUCT *Z, TCHAR param)
{
	int i = 0;

	if ( param == 's' ){
		SetJobTask(Z->hWnd, JOBSTATE_STARTJOB | JOBSTATE_NORMAL);
		return TRUE;
	}

	if ( param == 'e' ){
		SetJobTask(Z->hWnd, JOBSTATE_UNREGIST);
		return TRUE;
	}

	if ( (param != '\0') && (param != 't') ){ // Job が存在していないなら終了
		for ( ; i < X_MaxJob ; i++ ){
			if ( Sm->Job[i].ThreadID ) break;
		}
		if ( i == X_MaxJob ) return FALSE; // 存在しない...後続可能
	}

	if ( param == 'l' ){
		if ( (Sm->JobList.hWnd != NULL) && IsTrue(IsWindow(Sm->JobList.hWnd)) ){
			int listindex = (int)SendMessage(Sm->JobList.hWnd, LB_GETCOUNT, 0, 0);

			if ( listindex <= 0 ) return FALSE;
			JobListSelect(Sm->JobList.hWnd, listindex - 1, JOBSELECTMODE_CLOSE);
		}else{
			HWND hTargetWnd;
			int state, li = i;

			for ( ; i < X_MaxJob ; i++ ){
				if ( Sm->Job[i].ThreadID ) li = i;
			}

			UsePPx();
			hTargetWnd = Sm->Job[li].hWnd;
			state = Sm->Job[li].state.dw;
			FreePPx();

			if ( hTargetWnd != NULL ){
				if ( state & JOBFLAG_ENABLEHIDE ){
					SendMessage(hTargetWnd, WM_COMMAND, IDCANCEL, 0);
				}else{
					PostMessage(hTargetWnd, WM_KEYDOWN, VK_PAUSE, 0);
				}
			}
		}
		return TRUE;
	}

	// 一覧表示をする
	selectmode = (param != '\0') ? JOBSELECTMODE_PAUSE : JOBSELECTMODE_FOCUS;
	if ( (Sm->JobList.hWnd != NULL) && IsTrue(IsWindow(Sm->JobList.hWnd)) ){
		ShowJobListWnd(Z->hWnd);
	}else{
		Sm->JobList.showmode = 0;
		CreateJobListWnd();
	}
	// 開発中 "thread" ... スレッドの一覧も登録する
	if ( param == 't' ) X_jlst[0] = JOBLIST_THREAD;

	// 一覧の表示待ち
	for ( i = 0 ; i < 60 ; i ++ ){
		if ( Sm->JobList.hWnd != NULL ) break;
		Sleep(50);
	}
	SetForegroundWindow(Sm->JobList.hWnd);
	SetFocus(Sm->JobList.hWnd);
	if ( SendMessage(Sm->JobList.hWnd, LB_GETCURSEL, 0, 0) == LB_ERR ){
		SendMessage(Sm->JobList.hWnd, LB_SETCURSEL, 0, 0);
	}
	return TRUE; // 一覧表示...コマンド後続を終了
}
