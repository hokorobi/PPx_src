/*-----------------------------------------------------------------------------
	Paper Plane cUI												SubThread

	celltmp は、コピー済み(スレッドセーフ)の ENTRYCELL
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
#include "PPC_SUBT.H"
#pragma hdrstop

void USEFASTCALL GetDirSizeCache(PPC_APPINFO *cinfo);
void GetColumnExtData(PPC_APPINFO *cinfo);
void USEFASTCALL FixThreads(PPC_APPINFO *cinfo, DWORD ThreadID);

typedef struct {
	struct {
		DWORD ThreadID; // 確認中のスレッドID
		DWORD Count;	// 確認待機期間
	} FOPcheck;
	struct {
		int FixRead;	// ディレクトリの更新開始タイマ
		DWORD detects;	// 検出回数
		DWORD firstTick;	// 検出計数開始時間
	} DirCheck;
} SUBTHREADSTRUCT;

#if NODLL
#define DefineWinAPINoDLL(retvar, name, param) typedef retvar (WINAPI *imp ## name) param ; extern imp ## name D ## name
DefineWinAPINoDLL(HANDLE, OpenThread, (DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId));
#else
DefineWinAPI(HANDLE, OpenThread, (DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId)) = INVALID_VALUE(impOpenThread);
#endif

const TCHAR SubThreadName[] = T("PPc sub");
const TCHAR SubThreadThumbnail[] = T("PPc thumbnail");

//-----------------------------------------------------------------------------
void USEFASTCALL IObreak(PPC_APPINFO *cinfo)
{
	DWORD threadID = GetWindowThreadProcessId(cinfo->info.hWnd, NULL);
	HANDLE hThread;

	if ( (threadID == 0) || (DOpenThread == NULL) ) return;
	hThread = DOpenThread(STANDARD_RIGHTS_REQUIRED | THREAD_TERMINATE,
			FALSE, threadID);
	if ( hThread != NULL ){
		DefineWinAPI(BOOL, CancelSynchronousIo, (HANDLE hThread));

		GETDLLPROC(GetModuleHandle(StrKernel32DLL), CancelSynchronousIo);
		if ( DCancelSynchronousIo != NULL ){
			DCancelSynchronousIo(hThread);
		}
		CloseHandle(hThread);
	}
}

#ifndef SHCNRF_ShellLevel
#if defined(__BORLANDC__) || defined(__GNUC__)
typedef struct _SHChangeNotifyEntry
{
	LPCITEMIDLIST pidl;
	BOOL fRecursive;
} SHChangeNotifyEntry;
#endif
#define SHCNRF_ShellLevel 0x0002
#define SHCNRF_NewDelivery 0x8000
#endif

#ifdef _WIN64
#define DSHChangeNotifyRegister SHChangeNotifyRegister
#define DSHChangeNotifyDeregister SHChangeNotifyDeregister
#define DSHChangeNotification_Lock SHChangeNotification_Lock
#define DSHChangeNotification_Unlock SHChangeNotification_Unlock
#else
#if !NODLL
DefineWinAPI(ULONG, SHChangeNotifyRegister, (HWND hwnd, int fSources, LONG fEvents, UINT wMsg, int cEntries, const SHChangeNotifyEntry *pshcne));
DefineWinAPI(BOOL, SHChangeNotifyDeregister, (unsigned long ulID));
DefineWinAPI(HANDLE, SHChangeNotification_Lock, (HANDLE hChange, DWORD dwProcId, LPITEMIDLIST **pppidl, LONG *plEvent));
DefineWinAPI(BOOL, SHChangeNotification_Unlock, (HANDLE hLock));

LOADWINAPISTRUCT SHChangeNotifyDLL[] = {
	LOADWINAPI1(SHChangeNotifyRegister),
	LOADWINAPI1(SHChangeNotifyDeregister),
	LOADWINAPI1(SHChangeNotification_Lock),
	LOADWINAPI1(SHChangeNotification_Unlock),
	{NULL, NULL}
};
#else
ExternWinAPI(ULONG, SHChangeNotifyRegister, (HWND hwnd, int fSources, LONG fEvents, UINT wMsg, int cEntries, const SHChangeNotifyEntry *pshcne));
ExternWinAPI(BOOL, SHChangeNotifyDeregister, (unsigned long ulID));
ExternWinAPI(HANDLE, SHChangeNotification_Lock, (HANDLE hChange, DWORD dwProcId, LPITEMIDLIST **pppidl, LONG *plEvent));
ExternWinAPI(BOOL, SHChangeNotification_Unlock, (HANDLE hLock));

extern LOADWINAPISTRUCT SHChangeNotifyDLL[];
#endif
#endif

void SetShellChangeNotification(PPC_APPINFO *cinfo)
{
	SHChangeNotifyEntry req_entry;
	LPSHELLFOLDER pSF;

	#ifndef _WIN64
	if ( DSHChangeNotifyRegister == NULL ){
		LoadWinAPI("SHELL32.dll", NULL, SHChangeNotifyDLL, LOADWINAPI_GETMODULE);
	}
	if ( DSHChangeNotifyRegister == NULL ) return;
	#endif

	req_entry.pidl = NULL;
	pSF = VFPtoIShell(NULL, cinfo->path, (LPITEMIDLIST *)&req_entry.pidl);
	if ( pSF != NULL ){
		pSF->lpVtbl->Release(pSF);
		req_entry.fRecursive = FALSE;
		cinfo->ShNotifyID = DSHChangeNotifyRegister(cinfo->info.hWnd,
				SHCNRF_ShellLevel | SHCNRF_NewDelivery, // 何処が変わったかは不要
				SHCNE_RENAMEITEM | SHCNE_CREATE | SHCNE_DELETE |
				SHCNE_MKDIR | SHCNE_RMDIR | SHCNE_UPDATEDIR |
				SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED |
				SHCNE_RENAMEFOLDER | SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED,
				WM_PPCFOLDERCHANGE, 1, &req_entry);
		FreePIDL(req_entry.pidl);
	}
}

void USEFASTCALL StartReload(PPC_APPINFO *cinfo, int *FixRead)
{
	cinfo->FDirWrite = FDW_SENDREQUEST;
	PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_SETGRAYSTATUS, 0);

	if ( cinfo->SlowMode == FALSE ){ // 低速モードでなければ、更新待ちに移行
		*FixRead = XC_rrt[
				((GetFocus() == cinfo->info.hWnd) ||
				 (GetForegroundWindow() == cinfo->info.hWnd)) ? 0 : 1];
		DEBUGLOGC("Subthred-FixRead %d", *FixRead);
	}
}

void USEFASTCALL IntervalSubThread(PPC_APPINFO *cinfo, SUBTHREADSTRUCT *sts)
{
	if ( OSver.dwMajorVersion >= 6 ){
		// ● IsHungAppWindow (W2k)を使うことを検討する 1.32
		if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_FULLPUSH ){
			cinfo->BreakFlag = TRUE;
			if ( GetAsyncKeyState(VK_CONTROL) & KEYSTATE_FULLPUSH ){
				IObreak(cinfo);
			}
		}
		if ( cinfo->StateInfo.state != StateID_NoState ){
			DWORD tick = GetTickCount();
			if ( (tick - cinfo->StateInfo.tick) > 4000 ){
				int state = cinfo->StateInfo.state;

				cinfo->StateInfo.tick = tick;
				if ( state > StateID_MAX ) state = StateID_MAX;

				XMessage(NULL, NULL, XM_DbgLOG, T("BUSY[%s:%d]%s"), cinfo->RegCID, cinfo->LoadCounter, StateText[state]);
			}
		}

/* メインスレッドが応答無しだと、やっぱりウィンドウが開けない
		if ( cinfo->StateInfo.state != StateID_NoState ){
			if ( (cinfo->StateInfo.hStateWnd == NULL) &&
				 ((GetTickCount() - cinfo->StateInfo.tick) > 1000) ){
				int state = cinfo->StateInfo.state;

				if ( state > StateID_MAX ) state = StateID_MAX;

				cinfo->StateInfo.hStateWnd = CreateWindow(T("STATIC"),
						StateText[state], WS_POPUPWINDOW | WS_CAPTION,
						cinfo->wnd.NCRect.left, cinfo->wnd.NCRect.top,
						cinfo->wnd.NCArea.cx / 2,
						cinfo->wnd.NCArea.cy - cinfo->wnd.Area.cy,
						NULL, NULL, hInst, 0);
				ShowWindow(cinfo->StateInfo.hStateWnd, SW_SHOW);
			}
		}
		if ( cinfo->StateInfo.hStateWnd != NULL ){
			MSG msg;

			while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
				if ( msg.message == WM_QUIT ) break;
				DispatchMessage(&msg);
			}
		}
*/
	}

	// カーソルがウィンドウ外にでたときに表示更新が必要な箇所の処理 -----------
	if ( (cinfo->Mpos >= 0) || (cinfo->e.cellPoint >= 0) ){
		POINT pos;
		int posType;

		GetCursorPos(&pos);
		ScreenToClient(cinfo->info.hWnd, &pos);
		posType = GetItemTypeFromPoint(cinfo, &pos, NULL);
		if ( (cinfo->e.cellPoint >= 0) &&
			 !( (posType == PPCR_CELLMARK) ||
			 	(posType == PPCR_CELLTEXT) ||
			 	(posType == PPCR_CELLTAIL) ) ){
			ENTRYINDEX oldn;

			oldn = cinfo->e.cellPoint;
			cinfo->e.cellPoint = -1;
			RefleshCell(cinfo, oldn);
			if ( cinfo->Tip.states & STIP_CMD_HOVER ){
				HideEntryTip(cinfo);
			}
		}
		if ( (cinfo->Mpos >= 0) && (posType != PPCR_MENU) ){
			cinfo->Mpos = -1;
			InvalidateRect(cinfo->info.hWnd, &cinfo->BoxInfo, FALSE);	// 更新指定
		}
	}
	// PopMsg の時限消去 ------------------------------------------------------
	if ( cinfo->PopMsgTimer != 0 ){
		if ( --cinfo->PopMsgTimer == 0 ){
			StopPopMsg(cinfo, PMF_WAITTIMER);
		}
	}
	// ディレクトリ更新後の再読み込み待ち -------------------------------------
	if ( cinfo->FDirWrite == FDW_REQUEST ){ // グレー済みなので更新
		if ( sts->DirCheck.FixRead ) sts->DirCheck.FixRead--;
		if ( !sts->DirCheck.FixRead && !TinyCheckCellEdit(cinfo) ){
			DEBUGLOGC("Subthred-Reload Q", 0);
			cinfo->FDirWrite = FDW_REQUESTED;
			PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_raw | K_c | K_F5, 0);
		}
	}
	// 再読み込みより後にして、読込み開始までに１秒以上余裕を持たせた
	if ( (cinfo->FDirWrite == FDW_WAITREQUEST) && !TinyCheckCellEdit(cinfo) ){
		DEBUGLOGC("Subthred-StartReload", 0);
		StartReload(cinfo, &sts->DirCheck.FixRead);
	}
	// fileoperation thread のハングアップチェック ----------------------------
	if ( sts->FOPcheck.ThreadID == 0 ){ // 待機中→対象を探す
		if ( cinfo->TerminateThreadID != 0 ){
			sts->FOPcheck.Count = 1;
			sts->FOPcheck.ThreadID = cinfo->TerminateThreadID;
			cinfo->TerminateThreadID = 0;
		}
	}else{ // 確認中
		if ( --sts->FOPcheck.Count == 0 ){
			FixThreads(cinfo, sts->FOPcheck.ThreadID);
			sts->FOPcheck.ThreadID = 0;
		}
	}
}

//-----------------------------------------------------------------------------
//	別スレッド
//-----------------------------------------------------------------------------
DWORD WINAPI SubThread(LPDWORD lpdwParam)
{
	PPC_APPINFO *cinfo;
	SubThreadData ThreadData = {
		{{SubThreadName, XTHREAD_EXITENABLE, NULL, 0, 0}, NULL, K_THREADRESTART, 0}, /* threadstruct */
		NULL
	};

	// SUBT_GETINFOICON
	HICON hInfoIcon = NULL;	//削除すべきアイコン

	// ディレクトリ更新
	SUBTHREADSTRUCT sts;

	cinfo = (PPC_APPINFO *)lpdwParam;
	cinfo->SubThreadID = GetCurrentThreadId();
	ThreadData.threeadinfo.hParentWnd = cinfo->info.hWnd;
	PPxRegisterThread(&ThreadData.threeadinfo.threadinfo);

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // アイコン取得に使用する
	sts.FOPcheck.ThreadID = 0;
	sts.DirCheck.FixRead = 0;

	if ( DOpenThread == INVALID_HANDLE_VALUE ){
		GETDLLPROC(GetModuleHandle(StrKernel32DLL), OpenThread);
	}

	for ( ; ; ){
		DWORD result;

		result = WaitForMultipleObjects( (cinfo->SubT_dir != NULL) ?
					SubTs : SubTs - 1, cinfo->SubT, FALSE, 1000);
		ThreadData.threeadinfo.threadinfo.flag = (ThreadData.threeadinfo.threadinfo.flag & 0x7f) | ( (result & 0x1ff) << 7 ) | ((cinfo->SubTCmdFlags) << 16);
		switch (result){
			case WAIT_TIMEOUT:	// 定期確認 -----------------------------------
				IntervalSubThread(cinfo, &sts);
				continue;	// 定期確認終了

			case (WAIT_OBJECT_0 + 0):	// SubT_cmd 指令発行 ------------------
//			case (WAIT_ABANDONED_0 + 0):
				ResetEvent(cinfo->SubT_cmd);
										// 終了 ===============================
				// ● test byte [cinfo->SubTCmdFlags], SUBT_EXIT
				// je @@xxx
				// のコードが、失敗することがあるようだ
				// スレッド実行が異様に遅くなっている？
				//
				// ダミーの B31 を加えて
				// test dword [cinfo->SubTCmdFlags], SUBT_EXIT or B31
				// とするのがよい？
				if ( cinfo->SubTCmdFlags & SUBT_EXIT ){
//					resetflag(cinfo->SubTCmdFlags, SUBT_EXIT); 廃棄するので不要
					CloseHandle(cinfo->SubT_cmd);
					if ( cinfo->SubT_dir != NULL ){
						FindCloseChangeNotification(cinfo->SubT_dir);
					}
					if ( cinfo->ShNotifyID != 0 ){
						DSHChangeNotifyDeregister(cinfo->ShNotifyID);
					}
					if ( hInfoIcon != NULL ) DestroyIcon(hInfoIcon);
					FreeOverlayClass(ThreadData.LayPtr);
					CoUninitialize();
					PPxUnRegisterThread();
					return 0;
				}
										// 情報行のアイコンを取得 =======
				if ( cinfo->SubTCmdFlags & SUBT_GETINFOICON ){
					resetflag(cinfo->SubTCmdFlags, SUBT_GETINFOICON);
					DEBUGLOGC("Subthred-GETINFOICON start", 0);
#ifndef DEBUGICON
					GetInfoIcon(cinfo, &hInfoIcon, &ThreadData);
#endif
					DEBUGLOGC("Subthred-GETINFOICON end", 0);
				}
										// ディレクトリ監視を停止 ===========
				if ( cinfo->SubTCmdFlags & SUBT_STOPDIRCHECK ){
					DEBUGLOGC("Subthred-STOPDIRCHECK start", 0);
					resetflag(cinfo->SubTCmdFlags, SUBT_STOPDIRCHECK);
					if ( cinfo->SubT_dir != NULL ){
						FindCloseChangeNotification(cinfo->SubT_dir);
						cinfo->SubT_dir = NULL;
					}
					if ( cinfo->ShNotifyID != 0 ){
						DSHChangeNotifyDeregister(cinfo->ShNotifyID);
					}
					DEBUGLOGC("Subthred-STOPDIRCHECK end", 0);
				}
										// ディレクトリ監視を初期化 ===========
				if ( cinfo->SubTCmdFlags & SUBT_INITDIRCHECK ){
					DEBUGLOGC("Subthred-DIRCHECK start", 0);
					resetflag(cinfo->SubTCmdFlags, SUBT_INITDIRCHECK);
					if ( cinfo->SubT_dir != NULL ){
						FindCloseChangeNotification(cinfo->SubT_dir);
					}
					if ( cinfo->ShNotifyID != 0 ){
						DSHChangeNotifyDeregister(cinfo->ShNotifyID);
					}
					if ( (cinfo->RealPath[0] == '?') ||
						((cinfo->e.Dtype.mode != VFSDT_PATH) &&
						 (cinfo->e.Dtype.mode != VFSDT_SHN)) ){
						cinfo->SubT_dir = NULL;
						if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
							SetShellChangeNotification(cinfo);
						}
					}else{
						cinfo->SubT_dir = FindFirstChangeNotification(
							cinfo->RealPath, FALSE,
							FILE_NOTIFY_CHANGE_FILE_NAME |
							FILE_NOTIFY_CHANGE_DIR_NAME |
							FILE_NOTIFY_CHANGE_ATTRIBUTES |
							FILE_NOTIFY_CHANGE_SIZE |
							FILE_NOTIFY_CHANGE_LAST_WRITE);
						if ( cinfo->SubT_dir == INVALID_HANDLE_VALUE ){
							cinfo->SubT_dir = NULL;
						}
						sts.DirCheck.detects = 0;
						sts.DirCheck.firstTick = GetTickCount();
					}
					DEBUGLOGC("Subthred-DIRCHECK end", 0);
				}
										// セルに表示するアイコンを取得 =======
				if ( cinfo->SubTCmdFlags & SUBT_GETCELLICON ){
					resetflag(cinfo->SubTCmdFlags, SUBT_GETCELLICON);
#ifndef DEBUGICON
					DEBUGLOGC("Subthred-GETCELLICON start", 0);
					if ( cinfo->EntryIcons.hImage != NULL ){
						ThreadData.threeadinfo.threadinfo.ThreadName = SubThreadThumbnail;
						GetCellIcon(cinfo, &ThreadData);
						ThreadData.threeadinfo.threadinfo.ThreadName = SubThreadName;
					}
					DEBUGLOGC("Subthred-GETCELLICON end", 0);
#endif
				}
				// 時間経ってから更新(UpdateEntryがERROR_BUSYなので再試行) ====
				if ( cinfo->SubTCmdFlags & SUBT_REQUESTRELOAD ){
					DEBUGLOGC("Subthred-REQUESTRELOAD", 0);
					resetflag(cinfo->SubTCmdFlags, SUBT_REQUESTRELOAD);
					if ( cinfo->FDirWrite == FDW_REQUESTED ){
						cinfo->FDirWrite = FDW_REQUEST;
						sts.DirCheck.FixRead = XC_rrt[0]; // 既にUpdateEntry済みなので短い方で
					}
				}
										// ディレクトリサイズのキャッシュ取得==
				if ( cinfo->SubTCmdFlags & SUBT_GETDIRSIZECACHE ){
					DEBUGLOGC("Subthred-SUBT_GETDIRSIZECACHE start", 0);
					resetflag(cinfo->SubTCmdFlags, SUBT_GETDIRSIZECACHE);
					GetDirSizeCache(cinfo);
					DEBUGLOGC("Subthred-SUBT_GETDIRSIZECACHE end", 0);
				}
										// カラム拡張内容取得==================
				if ( cinfo->SubTCmdFlags & SUBT_GETCOLUMNEXT ){
					DEBUGLOGC("Subthred-SUBT_GETCOLUMNEXT start", 0);
					resetflag(cinfo->SubTCmdFlags, SUBT_GETCOLUMNEXT);
					GetColumnExtData(cinfo);
					DEBUGLOGC("Subthred-SUBT_GETCOLUMNEXT end", 0);
				}
										// 空き容量取得 =================
				if ( cinfo->SubTCmdFlags & SUBT_GETFREESPACE ){
					resetflag(cinfo->SubTCmdFlags, SUBT_GETFREESPACE);
					GetDirectoryFreeSpace(cinfo, TRUE);
				}
										// シェルによる変更通知 ===============
				if ( cinfo->SubTCmdFlags & SUBT_FOLDERCHANGE ){
					resetflag(cinfo->SubTCmdFlags, SUBT_FOLDERCHANGE);
					if ( cinfo->FDirWrite == FDW_NORMAL ){ // まだ未検出の状態
						if ( !TinyCheckCellEdit(cinfo) ){
							StartReload(cinfo, &sts.DirCheck.FixRead); // すぐにグレー化
						}else{ // グレー待機
							cinfo->FDirWrite = FDW_WAITREQUEST;
						}
					}
				}
				continue;

			case (WAIT_OBJECT_0 + 1): {	// SubT_dir ディレクトリ書き込み監視 --
				int OldFDirWrite = cinfo->FDirWrite;
				DEBUGLOGC("Subthred-Found Change start", 0);

				if ( cinfo->FDirWrite == FDW_NORMAL ){ // まだ未検出の状態
					if ( !TinyCheckCellEdit(cinfo) ){
						StartReload(cinfo, &sts.DirCheck.FixRead); // すぐにグレー化
					}else{ // グレー待機
						cinfo->FDirWrite = FDW_WAITREQUEST;
					}
				}
				// 2秒以内に 2000 / 50 回を越える検出があった場合は、
				// 異常かもしれないので、検出再設定を行わせる
				#define DCHECK_WDOGTIME 2000
				#define DCHECK_WDOGRATE 50
				if ( ++sts.DirCheck.detects >= (DCHECK_WDOGTIME / DCHECK_WDOGRATE) ){
					DWORD tick = GetTickCount();
					if ( (tick - sts.DirCheck.firstTick) <= DCHECK_WDOGTIME ){
						FindCloseChangeNotification(cinfo->SubT_dir);
						cinfo->SubT_dir = NULL;
						setflag(cinfo->SubTCmdFlags, SUBT_INITDIRCHECK);
						SetEvent(cinfo->SubT_cmd);
						continue;
					}
					sts.DirCheck.firstTick = tick;
					sts.DirCheck.detects = 0;
				}

				if ( FindNextChangeNotification(cinfo->SubT_dir) == FALSE ){
					FindCloseChangeNotification(cinfo->SubT_dir);
					cinfo->SubT_dir = NULL;
				}

				if ( OldFDirWrite != FDW_NORMAL ){
					Sleep(10); // 連続しないように少し待機
				}

				DEBUGLOGC("Subthred-Found Change end", 0);
				continue;
			}
			default: // 待機失敗(WAIT_FAILED等) -------------------------------
												// 同期オブジェクトを再作成
				CloseHandle(cinfo->SubT_cmd);
				cinfo->SubT_cmd = CreateEvent(NULL, TRUE, FALSE, NULL);
												// ディレクトリ監視を一旦閉じる
				if ( cinfo->SubT_dir != NULL ){
					FindCloseChangeNotification(cinfo->SubT_dir);
					cinfo->SubT_dir = NULL;
					setflag(cinfo->SubTCmdFlags, SUBT_INITDIRCHECK);
				}
				Sleep(10); // 連続しないように少し待機
				continue;
		}
	}
	// ここには到達しない
}

// ヒストリからディレクトリサイズを取得して設定する
void USEFASTCALL GetDirSizeCache(PPC_APPINFO *cinfo)
{
	int cnt, i, maxi;
	ENTRYCELL *cell;
	TCHAR path[VFPS];
	BYTE *hist;

	EnterCellEdit(cinfo);
	if ( TinyCheckCellEdit(cinfo) ){
		LeaveCellEdit(cinfo);
		return;
	}

	cnt = 30;	// １回で読み込むキャッシュ数
	maxi = cinfo->cellWMin + cinfo->cel.Area.cx * cinfo->cel.Area.cy * READCACHEPAGES;
	if ( maxi > cinfo->e.cellIMax ) maxi = cinfo->e.cellIMax;
	i = cinfo->cellWMin;

	for ( ; i < maxi ; i++ ){
		cell = &CEL(i);
		if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			 (cell->attr & (ECA_DIRNOH | ECA_DIRC | ECA_PARENT)) ){
			continue;
		}

		UsePPx();
		VFSFullPath(path, cell->f.cFileName, cinfo->path);
		hist = (BYTE *)SearchHistory(PPXH_PPCPATH, path);
		if ( (hist != NULL) &&
			 (GetHistoryDataSize(hist) >= (sizeof(DWORD) * 4)) ){
			int *sizes;

			sizes = (int *)GetHistoryData(hist);
			if ( sizes[3] != -1 ){
				cell->f.nFileSizeLow = sizes[2];
				cell->f.nFileSizeHigh = sizes[3];
				setflag(cell->attr, ECA_DIRG | ECA_DIRC | ECA_DIRNOH);
				RefleshCell(cinfo, i);
			}else{
				setflag(cell->attr, ECA_DIRNOH); // チェック済みの印を付加
			}
		}else{
			setflag(cell->attr, ECA_DIRNOH); // チェック済みの印を付加
		}
		FreePPx();

		cnt--;
		if ( cnt == 0 ){
			setflag(cinfo->SubTCmdFlags, SUBT_GETDIRSIZECACHE);
			SetEvent(cinfo->SubT_cmd);
			break;
		}
	}
	LeaveCellEdit(cinfo);
}

void WINAPI ColumnCallback(TCHAR *dst, const TCHAR *src)
{
	tstrlimcpy(dst, src, CMDLINESIZE);
}

// カラム拡張内容取得
void GetColumnExtData(PPC_APPINFO *cinfo)
{
	int cnt, i, maxi;
	ENTRYCELL *cell;
	TCHAR path[VFPS], text[CMDLINESIZE];
	DWORD useLoadCounter;

	EnterCellEdit(cinfo);
	if ( TinyCheckCellEdit(cinfo) ){
		LeaveCellEdit(cinfo);
		return;
	}
	useLoadCounter = cinfo->LoadCounter;

	cnt = 30;	// １回で読み込むキャッシュ数
	maxi = cinfo->cellWMin + cinfo->cel.Area.cx * cinfo->cel.Area.cy * 2;
	if ( maxi > cinfo->e.cellIMax ) maxi = cinfo->e.cellIMax;
	i = cinfo->cellWMin;

	for ( ; i < maxi ; i++ ){
		BOOL refrash;
		int ptroffset;

		cell = &CEL(i);
		ptroffset = cell->cellcolumn;
		if ( ptroffset < CCI_NOLOAD ) continue; // 空欄
/* 不要
		if ( (cell->attr & (ECA_PARENT | ECA_THIS)) ||
			 (cell->state < ECS_NORMAL) ){
			cell->cellcolumn = CCI_NODATA;
			continue;
		}
*/
		if ( ptroffset < 0 ) continue; // まだ表示していない
		refrash = FALSE;
		for ( ; ; ){
			COLUMNDATASTRUCT *cdsptr;
			int nowptroffset;

			nowptroffset = ptroffset;
			cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + ptroffset);
			ptroffset = cdsptr->nextoffset;
			if ( cdsptr->textoffset == 0 ){
				int itemindex;
				DWORD attributes;

				VFSFullPath(path, cell->f.cFileName, cinfo->RealPath);
				text[0] = '\0';
				itemindex = cdsptr->itemindex;
				attributes = cell->f.dwFileAttributes;
				LeaveCellEdit(cinfo);
				ExtGetData(&cinfo->ColumnExtDlls, itemindex, path, attributes, (GETINFOTIPCALLBACK)ColumnCallback, text);
				EnterCellEdit(cinfo);
				if ( useLoadCounter != cinfo->LoadCounter ){
					LeaveCellEdit(cinfo);
					return;
				}
				// EnterCellEdit の前で ColumnData.bottom が変化する恐れ有り
				cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + nowptroffset);
				cdsptr->textoffset = cinfo->ColumnData.top;
				ThAppend(&cinfo->ColumnData, text, TSTRSIZE32(text));
				refrash = TRUE;
			}
			if ( ptroffset == 0 ) break;
		}
		if ( useLoadCounter != cinfo->LoadCounter ) break;

		if ( IsTrue(refrash) ){ // 取得有り
			RefleshCell(cinfo, i);
		}else{ // 取得無し…次へ
			continue;
		}
		cnt--;
		if ( cnt == 0 ){
			setflag(cinfo->SubTCmdFlags, SUBT_GETCOLUMNEXT);
			SetEvent(cinfo->SubT_cmd);
			break;
		}
	}
	LeaveCellEdit(cinfo);
}

void USEFASTCALL FixThreads(PPC_APPINFO *cinfo, DWORD ThreadID)
{
	HANDLE hThread;

	if ( DOpenThread == NULL ) return;
	hThread = DOpenThread(STANDARD_RIGHTS_REQUIRED | THREAD_TERMINATE,
		FALSE, ThreadID);
	if ( hThread == NULL ) return;

	PPxCommonExtCommand(K_THREADUNREG, ThreadID);
#pragma warning(suppress: 6258) // 強制終了を意図
	TerminateThread(hThread, 0);
	CloseHandle(hThread);
	PostMessage(cinfo->info.hWnd, WM_PPXCOMMAND, KC_POPTENDFIX, 0);
}
