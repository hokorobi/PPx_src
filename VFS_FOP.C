/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System			ファイル一括操作
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
#include "PPCOMMON.RH"
#include "FATTIME.H"
#pragma hdrstop

const TCHAR STR_FOP[] = T("File operation");
const TCHAR STR_FOPWARN[] = T("File operation warning");
const TCHAR STR_FOPUNDOLOG[] = T("Undo log file");

// IDOK の文字列
const TCHAR STR_FOPOK[] = MES_FOOK;
const TCHAR STR_FOPSTART[] = MES_FOST;
const TCHAR STR_FOPCONTINUE[] = MES_FOCO;
const TCHAR STR_FOPPAUSE[] = MES_FOCP;
const TCHAR STR_FOPSKIP[] = MES_FOCS;
// IDCANCEL の文字列
const TCHAR STR_FOPCANCEL[] = MES_FOCA;
const TCHAR STR_FOPCLOSE[] = MES_FOCL;

const TCHAR StrLogCopy[] = T("Copy");			// コピー(Undo対象)
const TCHAR StrLogOverwrite[] = T("Overwrt");	// 上書きコピー(Undo不可)
const TCHAR StrLogMove[] = T("Move");			// 移動(Undo対象)
const TCHAR StrLogReplace[] = T("Replace");		// 上書き移動(Undo不可)
const TCHAR StrLogAppend[] = T("Append");		// 追加コピー／移動(Undo不可)
const TCHAR StrLogBackup[] = T("Backup");		// バックアップ
const TCHAR StrLogLink[] = T("Link");			// ショートカット/ハードリンク.
const TCHAR StrLogTest[] = T("Test");			// Test 成功(Undo不要)
const TCHAR StrLogExist[] = T("Exist");			// Test時に存在有り(Undo不要)
const TCHAR StrMenuItem_PPc[] = T("PP&c");
const TCHAR StrMenuItem_PPv[] = T("PP&v");
const TCHAR StrMenuItem_Sh[] = MES_SHCM;
const TCHAR StrAddOldName[] = T("-old");

const TCHAR *loghead[] = {
	StrLogCopy, StrLogOverwrite, StrLogMove, StrLogReplace, StrLogAppend, StrLogBackup, StrLogLink
};

#if !defined(ES_CONTINUOUS) || defined(__GNUC__)
#undef ES_SYSTEM_REQUIRED
#undef ES_CONTINUOUS
#define ES_SYSTEM_REQUIRED  ((DWORD)1)
#define ES_CONTINUOUS       ((DWORD)0x80000000)
typedef DWORD EXECUTION_STATE;
#endif
DefineWinAPI(EXECUTION_STATE, SetThreadExecutionState, (EXECUTION_STATE)) = INVALID_VALUE(impSetThreadExecutionState);

enum {
	FOPVIEWMENU_PPC = 1, FOPVIEWMENU_PPV, FOPVIEWMENU_SHMENU
};


void FopViewMenu(HWND hWnd, const TCHAR *path)
{
	POINT pos;
	HMENU hPopupMenu = CreatePopupMenu();
	int index;
	TCHAR buf[CMDLINESIZE];

	GetCursorPos(&pos);
	AppendMenuString(hPopupMenu, FOPVIEWMENU_PPC, StrMenuItem_PPc);
	AppendMenuString(hPopupMenu, FOPVIEWMENU_PPV, StrMenuItem_PPv);
	AppendMenuString(hPopupMenu, FOPVIEWMENU_SHMENU, StrMenuItem_Sh);
	index = TrackPopupMenu(hPopupMenu, TPM_TDEFAULT, pos.x, pos.y, 0, hWnd, NULL);
	DestroyMenu(hPopupMenu);
	if ( index == FOPVIEWMENU_SHMENU ){
		VFSSHContextMenu(hWnd, &pos, NULL, path, NULL);
	}else if ( index != 0 ){
		wsprintf(buf, T("\"%s\\%s\" \"%s\""),
				DLLpath,
				(index != FOPVIEWMENU_PPC) ? PPvExeName : PPcExeName,
				path);
		ComExecSelf(hWnd, buf, DLLpath, 0, NULL);
	}
}

int FopMakePIDLTable(const TCHAR *srcdir, const TCHAR *files, LPITEMIDLIST **pidls, LPSHELLFOLDER *pSF)
{
	HANDLE heap;
	LPSHELLFOLDER pParentFolder;
	LPITEMIDLIST *PIDLs;
	LPITEMIDLIST *lps;
	int cnt = 0, next = 0x400;
	int dirlen;
	const TCHAR *fileptr = files;
	TCHAR srcbuf[VFPS];

	heap = ProcHeap;
	PIDLs = HeapAlloc(heap, 0, next * sizeof(LPITEMIDLIST *));
	if ( PIDLs == NULL ) return 0;

	if ( VFSGetDriveType(fileptr, NULL, NULL) != NULL ){
		tstrcpy(srcbuf, fileptr);
		dirlen = (int)(VFSFindLastEntry(srcbuf) - srcbuf);
		srcbuf[dirlen] = '\0';

		while ( *fileptr != '\0' ){
			while ( dirlen && ((memcmp(srcbuf, fileptr, dirlen * sizeof(TCHAR)) != 0) || (*(fileptr + dirlen) != '\\')) ){
				dirlen = (int)(VFSFindLastEntry(srcbuf) - srcbuf);
				if ( srcbuf[dirlen] == '\0' ) break;
				srcbuf[dirlen] = '\0';
			}
			fileptr = fileptr + tstrlen(fileptr) + 1;
		}
		{ // drive root の時の補正
			TCHAR *last = VFSFindLastEntry(srcbuf);
			if ( (last != srcbuf) && (*last != '\\') ){
				*(last - 1) = '\0';
			}
		}
		srcdir = srcbuf;
		fileptr = files;
	}

	pParentFolder = VFPtoIShell(GetFocus(), srcdir, NULL);
	if ( pParentFolder == NULL ){
		HeapFree(heap, 0, PIDLs);
		return 0;
	}
	dirlen = tstrlen32(srcdir);

	lps = PIDLs;
	while ( *fileptr != '\0' ){
		if ( cnt == next ){
			LPITEMIDLIST *P;

			next += 0x400;
			P = HeapReAlloc(heap, 0, PIDLs, next * sizeof(LPITEMIDLIST *));
			if ( P == NULL ){
				HeapFree(heap, 0, PIDLs);
				cnt = 0;
				break;
			}else{
				lps = P + (lps - PIDLs);
				PIDLs = P;
			}
		}
		if ( dirlen && (memcmp(srcdir, fileptr, dirlen * sizeof(TCHAR)) == 0) && (*(fileptr + dirlen) == '\\') ){
			fileptr += dirlen + 1;
		}

		*lps = BindIShellAndFname(pParentFolder, fileptr);
		if ( *lps == NULL ){
			XMessage(NULL, NULL, XM_NiERRld, T("Bind error:%s"), fileptr);
			break;
		}else{
			lps++;
			cnt++;
		}
		fileptr = fileptr + tstrlen(fileptr) + 1;
	}
	*pidls = PIDLs;
	*pSF = pParentFolder;
	return cnt;
}


#pragma argsused
VOID CALLBACK PreventSleepProc(HWND hWnd, UINT msg, UINT_PTR id, DWORD time)
{
	FOPSTRUCT *FS;
	UnUsedParam(msg);UnUsedParam(id);UnUsedParam(time);

	FS = (FOPSTRUCT *)GetWindowLongPtr(hWnd, DWLP_USER);
	if ( FS->state != FOP_PAUSE ) DSetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void StartPreventSleep(HWND hWnd)
{
	if ( DSetThreadExecutionState == INVALID_VALUE(impSetThreadExecutionState) ){
		GETDLLPROC(hKernel32, SetThreadExecutionState);
	}
	if ( DSetThreadExecutionState != NULL ){
		SetTimer(hWnd, TIMERID_PREVENTSLEEP, TIMERRATE_PREVENTSLEEP, PreventSleepProc);
		PreventSleepProc(hWnd, WM_TIMER, TIMERID_PREVENTSLEEP, 0);
	}
}

void EndPreventSleep(HWND hWnd)
{
	if ( (DSetThreadExecutionState != NULL) &&
		 (DSetThreadExecutionState != INVALID_VALUE(impSetThreadExecutionState)) ){
		KillTimer(hWnd, TIMERID_PREVENTSLEEP);
		DSetThreadExecutionState(ES_CONTINUOUS);
	}
}

void USEFASTCALL SetHideMode(FOPSTRUCT *FS)
{
	if ( (Sm->JobList.hWnd != NULL) &&
		 (GetWindowLongPtr(FS->hDlg, GWL_STYLE) & WS_VISIBLE) ){
		ShowWindow(FS->hDlg, SW_HIDE);
	}
}

ERRORCODE FileOperationSourceFtp(FOPSTRUCT *FS, HINTERNET *hFTP, const TCHAR *srcentry, TCHAR *dst)
{
	ERRORCODE result;

	PeekMessageLoop(FS);
	if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;

	while( GetFileAttributesL(dst) != BADATTR ){
		BY_HANDLE_FILE_INFORMATION srcfinfo, dstfinfo;
		HANDLE hTFile;

		memset(&srcfinfo, 0, sizeof(srcfinfo));
		hTFile = CreateFileL(dst, GENERIC_READ, 0, NULL,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		result = SameNameAction(FS, hTFile, &srcfinfo, &dstfinfo, srcentry, dst);
		switch( result ){
			case ACTION_RETRY:
				continue;

			case ACTION_APPEND:
			case ACTION_OVERWRITE:
			case ACTION_CREATE:
				break;

			case ACTION_SKIP:
				FS->progs.info.EXskips++;
				return NO_ERROR;

			case ERROR_CANCELLED:
				return ERROR_CANCELLED;

			default:
				return ERROR_FILE_EXISTS; // error
		}

		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_BACKUPOLD ){
			BackupFile(FS, dst);
		}else {
			if ( dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES ){
				SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
			}
			DeleteFileL(dst);
		}
	}
	if ( DFtpGetFile(hFTP[FTPHOST], srcentry, dst, FALSE, FILE_ATTRIBUTE_ARCHIVE,
			FTP_TRANSFER_TYPE_BINARY | INTERNET_FLAG_DONT_CACHE, 0) == FALSE ){
		result = GetInetError();
		if ( result == ERROR_REQ_NOT_ACCEP ){ // ディレクトリの可能性
			TCHAR name[VFPS];
			WIN32_FIND_DATA findfile;
			HINTERNET hFtpFF;
			ThSTRUCT th;

			if ( DFtpSetCurrentDirectory(hFTP[FTPHOST], srcentry) == FALSE ){
				result = GetInetError();
			}else{
				hFtpFF = DFtpFindFirstFile(hFTP[FTPHOST], WildCard_All, &findfile, INTERNET_FLAG_RELOAD, 0);
				if ( hFtpFF == NULL ){
					result = GetInetError();
				}else{
					TCHAR *p;

					// エントリ一覧を作成(DFtpFindFirstFileの重複がダメだから)
					ThInit(&th);
					do {
						ThAddString(&th, findfile.cFileName);
					} while( IsTrue(DInternetFindNextFile(hFtpFF, &findfile)) );
					DInternetCloseHandle(hFtpFF);
					ThAddString(&th, NilStr);

					CreateDirectoryL(dst, NULL);
					p = (TCHAR *)th.bottom;
					while ( *p != '\0' ){
						CatPath(name, dst, p);
						result = FileOperationSourceFtp(FS, hFTP, p, name);
						if ( result != NO_ERROR ) break;
						p += tstrlen(p) + 1;
					};
					if ( DFtpSetCurrentDirectory(hFTP[FTPHOST], T("..")) == FALSE ){
						result = GetInetError();
					}
					ThFree(&th);

					if ( (result == NO_ERROR) && (FS->opt.fop.mode == FOPMODE_MOVE) ){
						DFtpRemoveDirectory(hFTP[FTPHOST], srcentry);
					}
					return result;
				}
			}
		}
		return result;
	}else{
		if ( FS->opt.fop.mode == FOPMODE_MOVE ){
			DFtpDeleteFile(hFTP[FTPHOST], srcentry);
		}
		FopLog(FS, srcentry, dst, LOG_COPY);
		return NO_ERROR;
	}
}

ERRORCODE FileOperationDestinationFtp(FOPSTRUCT *FS, HINTERNET *hFTP, const TCHAR *src, const TCHAR *dstentry)
{
	ERRORCODE result;
	DWORD attr;

	PeekMessageLoop(FS);
	if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;

	attr = GetFileAttributesL(src);
	if ( attr == BADATTR ) return GetLastError();
	if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
		TCHAR path[VFPS];
		WIN32_FIND_DATA findfile;
		HANDLE hFile;

		if ( DFtpCreateDirectory(hFTP[FTPHOST], dstentry) == FALSE ){
			return GetInetError();
		}
		CatPath(path, (TCHAR *)src, WildCard_All);
		hFile = FindFirstFileL(path, &findfile);
		if ( hFile == INVALID_HANDLE_VALUE ) return GetLastError();
		DFtpSetCurrentDirectory(hFTP[FTPHOST], dstentry);
		result = ERROR_PATH_NOT_FOUND;
		do {
			if ( !IsRelativeDirectory(findfile.cFileName) ){
				CatPath(path, (TCHAR *)src, findfile.cFileName);
				result = FileOperationDestinationFtp(FS, hFTP, path, findfile.cFileName);
				if ( result != NO_ERROR ) break;
			}
		}while( IsTrue(FindNextFile(hFile, &findfile)) );
		FindClose(hFile);
		DFtpSetCurrentDirectory(hFTP[FTPHOST], T(".."));
		return result;
	}

	if ( DFtpPutFile(hFTP[FTPHOST], src, dstentry, FTP_TRANSFER_TYPE_BINARY, 0)
																== FALSE ){
		return GetInetError();
	}else{
		if ( FS->opt.fop.mode == FOPMODE_MOVE ) DeleteFileL(src);
		FopLog(FS, src, dstentry, LOG_COPY);
		return NO_ERROR;
	}
}

TCHAR *GetDriveEnd(TCHAR *p, int mode)
{
	TCHAR *q;

	if ( *p ){
		if ( mode == VFSPT_UNC ){	// UNC
			if ( *p == '\\' ) p++;
			q = FindPathSeparator(p);	// pc name
			if ( q != NULL ){
				p = q;
				q = FindPathSeparator(q + 1);	// share name
				if ( q != NULL ){
					p = q;
				}else{
					p += tstrlen(p);
				}
			}else{
				p += tstrlen(p);
			}
		}
	}
	return p;
}

void CreateFWriteLogWindow(FOPSTRUCT *FS)
{
	HWND hWnd;
	RECT box = {3, 135, 249, 50};

	if ( FS->hEWnd != NULL ){
		if ( IsTrue(IsWindow(FS->hEWnd)) ) return;
		FS->hEWnd = NULL;
	}

	MapDialogRect(FS->hDlg, &box);
	hWnd = FS->hEWnd = CreateWindow(EDITstr, NilStr, WS_CHILD | WS_VSCROLL |
			ES_AUTOVSCROLL | ES_NOHIDESEL | ES_LEFT | ES_MULTILINE,
			box.left, box.top, box.right, box.bottom, FS->hDlg,
			CHILDWNDID(IDE_FOP_LOG), DLLhInst, 0);
	PPxRegistExEdit(NULL, hWnd, 0x100000, NULL, 0, 0, PPXEDIT_NOWORDBREAK);
	SendMessage(hWnd, WM_SETFONT, SendMessage(FS->hDlg, WM_GETFONT, 0, 0), TRUE);
	*FS->hLogWnd = hWnd;
	SetWindowY(FS, 0); // 大きさ調整＆表示
	SetDlgFocus(FS->hDlg, IDOK);
}
void FWriteLog(HWND hEWnd, const TCHAR *message)
{
	SendMessage(hEWnd, EM_SETSEL, EC_LAST, EC_LAST);
	SendMessage(hEWnd, EM_REPLACESEL, 0, (LPARAM)message);
	SendMessage(hEWnd, EM_SCROLL, SB_LINEDOWN, 0);
}

void FWriteErrorLogs(FOPSTRUCT *FS, const TCHAR *mes, const TCHAR *type, ERRORCODE error)
{
	TCHAR buf[CMDLINESIZE], errormsg[CMDLINESIZE];

	PPErrorMsg(errormsg, error);
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
		if ( type == NULL ) type = NilStr;
		wsprintf(buf, T("%sError\t%s\r\n\t%s\r\n"), type, mes, errormsg);
		FopLog(FS, buf, NULL, LOG_STRING);
		return;
	}
	FS->NoAutoClose = TRUE;
	CreateFWriteLogWindow(FS);
	FWriteLog(FS->hEWnd, mes);
	FWriteLog(FS->hEWnd, T(":\t"));
	FWriteLog(FS->hEWnd, errormsg);
	if ( type != NULL ){
		FWriteLog(FS->hEWnd, T(" - "));
		FWriteLog(FS->hEWnd, type);
	}
	FWriteLog(FS->hEWnd, T("\r\n"));
}

void FWriteLogMsg(FOPSTRUCT *FS, const TCHAR *mes)
{
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
		FopLog(FS, mes, NULL, LOG_STRING);
		return;
	}
//	FS->NoAutoClose = TRUE;
	CreateFWriteLogWindow(FS);
	FWriteLog(FS->hEWnd, mes);
}

void FopLog(FOPSTRUCT *FS, const TCHAR *src, const TCHAR *dst, enum foplogtypes type)
{
	TCHAR buf[VFPS * 3], *srcp, *dstp;

	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_UNDOLOG ){
		DWORD size;

		switch ( type ){
			case LOG_SKIP:
				wsprintf(buf, STRLOGSKIP T("\t%s\r\n"), src);
				break;
			case LOG_DIR:
			case LOG_DIR_DELETE:
				return;
			case LOG_DELETE:
				wsprintf(buf, STRLOGDELETE T("\t%s\r\n"), src);
				break;
			case LOG_MAKEDIR:
				wsprintf(buf, STRLOGMAKEDIR T("\t%s\r\n"), src);
				break;
			case LOG_STRING:
				tstrcpy(buf, src);
				break;
			default:
				wsprintf(buf, T("%s\t%s\r\n") STRLOGDEST T("\t%s\r\n"), loghead[type], src, dst);
		}
												// 内容を出力 -----------------
		WriteFile(FS->hUndoLogFile, buf, TSTRLENGTH32(buf), &size, NULL);
	}

	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
		dstp = NilStrNC;
		srcp = FindLastEntryPoint(src);
		if ( dst != NULL ){
			dstp = FindLastEntryPoint(dst);
		}

		switch ( type ){
			case LOG_SKIP:
				wsprintf(buf, T("Skip\t%s\r\n"), srcp);
				break;
			case LOG_DELETE:
				wsprintf(buf, T("Delete\t%s\r\n"), srcp);
				break;
			case LOG_STRING:
				tstrcpy(buf, src);
				break;
			case LOG_DIR:
				wsprintf(buf, T("Source\t%s\r\nDestination %s\r\n"), src, dst);
				break;
			case LOG_DIR_DELETE:
				wsprintf(buf, T("Source\t%s\r\n"), src);
				break;
			case LOG_MAKEDIR:
				wsprintf(buf, STRLOGMAKEDIR T("\t%s\r\n"), src);
				break;
			default:
				if ( tstrcmp(srcp, dstp) ){
					wsprintf(buf, T("%s\t%s\r\n") STRLOGDEST T("\t%s\r\n"), loghead[type], srcp, dstp);
				}else{
					wsprintf(buf, T("%s\t%s\r\n"), loghead[type], srcp);
				}
		}

		if ( (FS->opt.hReturnWnd != NULL) && IsWindow(FS->opt.hReturnWnd) ){
			if ( SendMessage(FS->opt.hReturnWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_WINDDOWLOG, 1), (LPARAM)buf) != 0 ){
				return;
			}
		}
		// 共用ログがつかえないので自前で表示
		CreateFWriteLogWindow(FS);
		FWriteLog(FS->hEWnd, buf);
	}
}

void DisplaySrcNameNow(FOPSTRUCT *FS)
{
	if ( FS->progs.srcpath != NULL ){
		SetWindowText(FS->progs.hSrcNameWnd, FS->progs.srcpath);
		FS->progs.srcpath = NULL;
	}
}

void TinyDisplayProgress(FOPSTRUCT *FS)
{
	TCHAR buf[200];
	DWORD tick = GetTickCount();

	if ( (tick - FS->progs.ProgTick) >= PROGRESS_INTERVAL_TICK ){ // 1file進捗状況更新
		if ( FS->progs.info.filesall == 0 ){
			SetTaskBarButtonProgress(FS->progs.hWnd, TBPF_INDETERMINATE, 0);
		}
		FS->progs.ProgTick = tick;

		wsprintf(buf, T("\x1b\x64%c%2d/%d marks  %d/%d files"),
				FS->progs.info.busybar,
				FS->progs.info.mark, FS->progs.info.markall,
				FS->progs.info.donefiles, FS->progs.info.filesall);
		SetWindowText(FS->progs.hProgressWnd, buf);
		FS->progs.info.busybar++;
		if ( FS->progs.info.busybar > 10) FS->progs.info.busybar = 1;

		if ( FS->progs.srcpath != NULL ){
			SetWindowText(FS->progs.hSrcNameWnd, FS->progs.srcpath);
			FS->progs.srcpath = NULL;
		}
	}
}
/*	Progressが呼び出される単位
	WindowsVista-10 : 4M未満 0.5M  4M以上 1M
	WindowsXP : 64k
*/
void FullDisplayProgress(struct _ProgressWndInfo *Progs, LARGE_INTEGER TotalTransSize, LARGE_INTEGER TotalSize)
{
	DWORD tick;
	TCHAR buf[VFPS + 256], pbuf[VFPS + 256];

	tick = GetTickCount();
				// over flow 問題は考慮せず
	if ( (tick - Progs->ProgTick) >= PROGRESS_INTERVAL_TICK ){ // 1file進捗状況
		DWORD per;

		if ( Progs->info.filesall == 0 ){
			SetTaskBarButtonProgress(Progs->hWnd, TBPF_INDETERMINATE, 0);
		}
		Progs->ProgTick = tick;

		if ( TotalSize.u.HighPart || (TotalSize.u.LowPart & 0xfe000000) ){
						// 0x0200 0000 0000(2TB) ～ 0x1ff ffff ffff ffff(128P)
			if ( TotalSize.u.HighPart >= 0x200 ){
				per = (TotalTransSize.u.HighPart * 100) / TotalSize.u.HighPart;
			}else{		// 0x0200 0000(32M) ～ 0x1ff ffff ffff(2TB)
				per = ( ((((DWORD)TotalTransSize.u.HighPart) << 16) |
						(TotalTransSize.u.LowPart >> 16)) * 100) /
					  ( ((((DWORD)TotalSize.u.HighPart) << 16) |
						(TotalSize.u.LowPart >> 16)) );
			}
		}else{				// 0x0000 0001      ～  0x01ff ffff(32M)
			if ( TotalSize.u.LowPart ){
				per = (TotalTransSize.u.LowPart * 100) / TotalSize.u.LowPart;
			}else{							// 0x0000 0000
				per = 100;
			}
		}

		wsprintf(buf, T("\x1b%c%c%2d/%d marks  %d/%d files"),
				per + 1,
				Progs->info.busybar,
				Progs->info.mark, Progs->info.markall,
				Progs->info.donefiles, Progs->info.filesall);
		SetWindowText(Progs->hProgressWnd, buf);
		Progs->info.busybar++;
		if ( Progs->info.busybar > 10 ) Progs->info.busybar = 1;

		if ( Progs->srcpath != NULL ){
			SetWindowText(Progs->hSrcNameWnd, Progs->srcpath);
			Progs->srcpath = NULL;
		}
	}
				// over flow 問題は考慮せず
	if ( (tick - Progs->CapTick) >= CAPTION_INTERVAL_TICK ){ // キャプション/全体進捗更新
		const TCHAR *sfmt;
		int totalper;

		Progs->CapTick = tick;
		if ( Progs->info.filesall &&
			 (Progs->info.allsize.l | Progs->info.allsize.h) ){
			DWORD sizeL, sizeH;

			sizeL = Progs->info.donesize.l;
			sizeH = Progs->info.donesize.h;
			AddDD(sizeL, sizeH,
					TotalTransSize.u.LowPart, TotalTransSize.u.HighPart);

			if ( (Progs->info.allsize.l & 0xfe000000) || Progs->info.allsize.h ){
				if ( Progs->info.allsize.h >= 0x200 ){
					if ( sizeH & 0xfe000000 ){ // 128P over
						totalper = ((sizeH >> 16) * 100) / (Progs->info.allsize.h >> 16);
					}else{ // 0x0200 0000 0000(2TB) ～ 0x1ff ffff ffff ffff(128P)
						totalper = (sizeH * 100) / Progs->info.allsize.h;
					}
				}else{	// 0x0200 0000(32M) ～ 0x1ff ffff ffff(2TB)
					totalper =
							( ((((DWORD)sizeH) << 16) | (sizeL >> 16)) * 100) /
							( ((((DWORD)Progs->info.allsize.h) << 16) |
								(Progs->info.allsize.l >> 16)) );
				}
			}else{		// 0x0000 0001      ～  0x01ff ffff(32M)
				if ( Progs->info.allsize.l ){
					totalper = (sizeL * 100) / Progs->info.allsize.l;
				}else{	// 0x0000 0000
					totalper = 100;
				}
			}
			sfmt = T("%d%% / %s");
		}else{
			sfmt = T("%d / %s");
			totalper = Progs->info.donefiles;
		}
		if ( Progs->nowper != totalper ){
			TCHAR *path;

			Progs->nowper = totalper;

			pbuf[0] = '\0';
			GetWindowText(Progs->hWnd, pbuf, TSIZEOF(pbuf));
			path = tstrchr(pbuf, '/');
			path = (path != NULL) ? path + 2 : pbuf;

			wsprintf(buf, sfmt, totalper, path);
			SetWindowText(Progs->hWnd, buf);

			if ( Progs->info.filesall != 0 ){
				SetTaskBarButtonProgress(Progs->hWnd, totalper, 100);
			}
			SetJobTask(Progs->hWnd, JOBSTATE_SETRATE | totalper);
		}
	}
}

#pragma argsused
DWORD CALLBACK CopyProgress(LARGE_INTEGER TotalSize, LARGE_INTEGER TotalTransSize, LARGE_INTEGER StreamSize, LARGE_INTEGER StreamTransSize, DWORD StreamNumber, DWORD reason, HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData)
{
	UnUsedParam(StreamSize);UnUsedParam(StreamTransSize);UnUsedParam(StreamNumber);UnUsedParam(hSourceFile);UnUsedParam(hDestinationFile);

	if ( reason == CALLBACK_CHUNK_FINISHED ){
		FullDisplayProgress((struct _ProgressWndInfo *)lpData, TotalTransSize, TotalSize);
		PeekMessageLoop(((struct _ProgressWndInfo *)lpData)->FS);
	}
	return PROGRESS_CONTINUE;
}


ERRORCODE TestDest(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst)
{
	TCHAR buf[VFPS * 2 + 0x100];
	DWORD attr;

	PeekMessageLoop(FS);
	if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;
	if ( FS->opt.fop.mode == FOPMODE_SHORTCUT ){
		attr = (FOPMakeShortCut(src, dst,
				GetFileAttributesL(src) & FILE_ATTRIBUTE_DIRECTORY,
				TRUE) == NO_ERROR) ? BADATTR : 0;
	}else{
		attr = GetFileAttributesL(dst);
	}

	if ( attr != BADATTR ){
		FS->errorcount++;
	}else{
		FS->progs.info.donefiles++;
	}
	wsprintf(buf, T("%s\t%s\r\n") STRLOGDEST T("\t%s\r\n"),
		(attr == BADATTR) ? StrLogTest : StrLogExist, src, dst);

	FWriteLogMsg(FS, buf);
	return NO_ERROR;
}

ERRORCODE RenameDestFile(FOPSTRUCT *FS, const TCHAR *dst, BOOL addOldStr)
{
	TCHAR newolddstname[VFPS];
	TCHAR extbuf[VFPS], *extp;

	tstrcpy(newolddstname, dst);
	if ( addOldStr &&
		 (tstrlen(newolddstname) < (VFPS - (TSIZEOFSTR(StrAddOldName) + 2))) ){
		extp = VFSFindLastEntry(newolddstname);
		extp += FindExtSeparator(extp);
		tstrcpy(extbuf, extp);
		tstrcpy(extp, StrAddOldName);
		tstrcpy(extp + TSIZEOFSTR(StrAddOldName), extbuf);
	}

	if ( GetUniqueEntryName(newolddstname) == FALSE ){
		return ERROR_ALREADY_EXISTS;
	}
	if ( IsTrue(MoveFileL(dst, newolddstname)) ){
		FopLog(FS, dst, newolddstname, LOG_BACKUP);
		return NO_ERROR;
	}
	return GetLastError();
}

const TCHAR SameAction_addnumber[] = MES_SNAN;
const TCHAR SameAction_append[] = MES_SNAP;
const TCHAR SameAction_backup[] = MES_SNBK;
const TCHAR SameAction_changename[] = MES_SNCN;
const TCHAR SameAction_copy[] = MES_SNCP;
const TCHAR SameAction_delete[] = MES_SNDE;
const TCHAR SameAction_keep[] = MES_SNKP;
const TCHAR SameAction_link[] = MES_SNLN;
const TCHAR SameAction_move[] = MES_SNMV;
const TCHAR SameAction_overwrite[] = MES_SNOW;
const TCHAR SameAction_restore[] = MES_SNRE;
const TCHAR SameAction_skip[] = MES_SNSK;

const TCHAR *SameAction_Srclist[] = {
	SameAction_move,
	SameAction_copy,
	SameAction_copy,
	SameAction_link,
	SameAction_link,
	SameAction_delete,
	SameAction_restore,
	SameAction_link,
};

void ChangeEntryActionHelp(FOPSTRUCT *FS, BY_HANDLE_FILE_INFORMATION *srcfinfo, BY_HANDLE_FILE_INFORMATION *dstfinfo)
{
	TCHAR buf[VFPS], *lastp;
	const TCHAR *srcmemo = NULL, *dstmemo = NULL;

	switch ( FS->opt.fop.same ){
		case FOPSAME_NEWDATE:
			if ( FuzzyCompareFileTime(&srcfinfo->ftLastWriteTime,
					&dstfinfo->ftLastWriteTime) > 0){
				break;
			}
			// FOPSAME_SKIP へ
		case FOPSAME_SKIP:
			srcmemo = SameAction_skip;
			dstmemo = SameAction_keep;
			break;

		case FOPSAME_ARCHIVE:
			if ( srcfinfo->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) break;
			srcmemo = SameAction_skip;
			dstmemo = SameAction_keep;
			break;

		case FOPSAME_RENAME:
		case FOPSAME_ADDNUMBER:
			srcmemo = SameAction_changename;
			dstmemo = SameAction_keep;
			break;

		case FOPSAME_APPEND:
			srcmemo = SameAction_append;
			dstmemo = SameAction_append;
			break;

		case FOPSAME_SIZE:
			if ( (srcfinfo->nFileSizeLow != dstfinfo->nFileSizeLow) ||
				 (srcfinfo->nFileSizeHigh != dstfinfo->nFileSizeHigh) ){
				break;
			}
			srcmemo = SameAction_skip;
			dstmemo = SameAction_keep;
			break;

							// これらは常に上書き
//		case FOPSAME_OVERWRITE:
//		default:
//			break;
	}
	if ( srcmemo == NULL ){
		srcmemo = SameAction_Srclist[FS->opt.fop.mode];
	}
	GetDlgItemText(FS->hDlg, IDS_FOP_SRCTITLE, buf, VFPS);
	lastp = tstrrchr(buf , '(');
	if ( lastp != NULL ) *lastp = '\0';
	tstrcpy(buf + tstrlen(buf), MessageText(srcmemo));
	SetDlgItemText(FS->hDlg, IDS_FOP_SRCTITLE, buf);

	if ( dstmemo == NULL ){
		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_ADDNUMDEST ){
			dstmemo = SameAction_addnumber;
		}else if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_BACKUPOLD ){
			dstmemo = SameAction_backup;
		}else{
			dstmemo = SameAction_overwrite;
		}
	}
	GetDlgItemText(FS->hDlg, IDS_FOP_DESTTITLE, buf, VFPS);
	lastp = tstrrchr(buf , '(');
	if ( lastp != NULL ) *lastp = '\0';
	tstrcpy(buf + tstrlen(buf), MessageText(dstmemo));
	SetDlgItemText(FS->hDlg, IDS_FOP_DESTTITLE, buf);
}

ERRORCODE SameNameAction(FOPSTRUCT *FS, HANDLE dstH, BY_HANDLE_FILE_INFORMATION *srcfinfo, BY_HANDLE_FILE_INFORMATION *dstfinfo, const TCHAR *src, TCHAR *dst)
{
	BOOL samefile = FALSE;
	HANDLE hDlg = FS->hDlg;
	struct FopOption *opt = &FS->opt;
	ERRORCODE result;	// 負の時はスキップ
	TCHAR pathbuf[VFPS];

										// 処理先情報の取得 -------------------
	if ( dstH != NULL ){
		if( GetFileInformationByHandle(dstH, dstfinfo) == FALSE ){
			memset(dstfinfo, 0, sizeof(BY_HANDLE_FILE_INFORMATION));
			dstfinfo->dwFileAttributes = BADATTR;
		}
		CloseHandle(dstH);
	}
	if ( (dstH == NULL)
	#ifndef UNICODE
		|| (WinType == WINTYPE_9x)
	#endif
	){
		dstfinfo->dwFileAttributes = GetFileAttributesL(dst);
	}
										// 同一ファイルなら名前変更に切替 -----
	if ( (srcfinfo->dwVolumeSerialNumber == dstfinfo->dwVolumeSerialNumber) &&
		 (srcfinfo->nFileIndexHigh == dstfinfo->nFileIndexHigh) &&
		 (srcfinfo->nFileIndexLow  == dstfinfo->nFileIndexLow) &&
		 // ↓ 別PCの同名パスのときに上記条件を満たしてしまうための対策
		 ((src[0] != '\\') || (dst[0] != '\\') || !tstricmp(src, dst)) ){
		// ファイルカウント時は何もしない
		if ( FS->progs.info.count_exists >= 2 ) return NO_ERROR;

		samefile = TRUE;

		if ( (opt->fop.same != FOPSAME_RENAME) &&
			 (opt->fop.same != FOPSAME_SKIP) &&
			 (opt->fop.same != FOPSAME_ADDNUMBER) ){
			opt->fop.same = FOPSAME_RENAME;
			opt->fop.sameSW = 0; // 始めの設定と異なるので再選択させる
			CheckRadioButton(hDlg, IDR_FOP_NEWDATE, IDR_FOP_ARC, IDR_FOP_RENAME);
		}
	}

	for ( ; ; ){
		if ( opt->fop.sameSW == 0 ){		// 対処方法選択UI -----------------
			int now_same = -1;
			DWORD now_flags = 0;

			if ( !(GetWindowLongPtr(hDlg, GWL_STYLE) & WS_VISIBLE) ){
				ShowWindow(hDlg, SW_SHOWNORMAL);
			}

			ActionInfo(hDlg, FS->info, AJI_SHOW, T("fop")); // 通知
			SetTaskBarButtonProgress(hDlg, TBPF_PAUSED, 0);
			SetJobTask(FS->hDlg, JOBSTATE_PAUSE);
			FS->state = FOP_PAUSE;			// 準備
			FS->Command = 0;

			SetFopTab(FS, FOPTAB_GENERAL);
			if ( samefile == FALSE ){
				Enables(FS, TRUE, FALSE);
			}else{
				Enables(FS, FALSE, 2);
			}
			SetDlgItemText(hDlg, IDOK, MessageText(STR_FOPCONTINUE));
			SetDlgItemText(hDlg, IDB_TEST, MessageText(MES_FOFC));
			DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示

			DispAttr(hDlg, IDS_FOP_SRCINFO, srcfinfo, dstfinfo);
			DispAttr(hDlg, IDS_FOP_DESTINFO, dstfinfo, srcfinfo);

			if ( opt->fop.aall != 0 ){
				opt->fop.sameSW = 1;
				CheckDlgButton(hDlg, IDX_FOP_SAME, TRUE);
			}

			if ( FS->progs.info.count_exists >= 2 ){ // 存在一覧を表示
				int pos;
				pos = wsprintf(pathbuf, MessageText(MES_QSAN), FS->progs.info.count_exists);
				switch ( opt->fop.same ){
					case FOPSAME_NEWDATE:
						wsprintf(pathbuf + pos, T("%d overwrite, %d copy"), FS->progs.info.exists[2], FS->progs.info.filesall - FS->progs.info.count_exists);
						break;
					case FOPSAME_RENAME:
						wsprintf(pathbuf + pos, T("%d rename, %d copy"), FS->progs.info.count_exists, FS->progs.info.filesall - FS->progs.info.count_exists);
						break;
					case FOPSAME_SKIP:
						wsprintf(pathbuf + pos, T("%d copy"), FS->progs.info.filesall - FS->progs.info.count_exists);
						break;
					case FOPSAME_ADDNUMBER:
						wsprintf(pathbuf + pos, T("%d number copy"), FS->progs.info.count_exists);
						break;
				}

				SetWindowText(FS->progs.hProgressWnd, pathbuf);
				SetWindowY(FS, 0);
			}else{ // １ファイルの詳細比較を表示
				SetWindowText(FS->progs.hProgressWnd, MessageText(MES_QSAO));
				SetWindowY(FS, WINY_FULL);
			}

			SetDlgFocus(hDlg, opt->fop.same + IDR_FOP_NEWDATE);
										// 選択
			for ( ; ; ){
				MSG msg;

				if ( (now_same != opt->fop.same) || (FS->opt.fop.flags != now_flags) ){
					now_same = opt->fop.same;
					now_flags = FS->opt.fop.flags;
					ChangeEntryActionHelp(FS, srcfinfo, dstfinfo);
				}
				if( (int)GetMessage(&msg, NULL, 0, 0) <= 0 ){
					FS->state = FOP_TOBREAK;
					break;
				}
				if ( IsDialogMessage(hDlg, &msg) == FALSE ){
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if ( FS->Command != 0 ){
					if ( FS->Command == IDB_TEST ){
						FopCompareFile(hDlg, src, dst);
					}else{
						FopViewMenu(hDlg, (FS->Command == IDS_FOP_SRCINFO) ? src : dst);
					}
					FS->Command = 0;
				}
				if ( (FS->state == FOP_BUSY) || (FS->state == FOP_TOBREAK) ){
					break;
				}
			}
											// 実行状態に戻す
			SetWindowY(FS, WINY_STAT);
			SetTaskBarButtonProgress(hDlg, TBPF_NORMAL, 0);
			SetJobTask(FS->hDlg, JOBSTATE_UNPAUSE);
			if ( FS->hide ) SetHideMode(FS);
			if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;

			if ( FS->progs.info.count_exists >= 2 ){ // 存在一覧のときは、この場で処理しない
				return NO_ERROR;
			}
		}else if ( FS->progs.srcpath != NULL ){
			DWORD tick = GetTickCount();

			if ( (tick - FS->progs.ProgTick) >= PROGRESS_INTERVAL_TICK ){ // 1file進捗状況更新
				FS->progs.ProgTick = tick;
				DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
			}
		}
									// 対処する -----------------------
		switch ( opt->fop.same ){
			case FOPSAME_NEWDATE:
				result = (FuzzyCompareFileTime(&srcfinfo->ftLastWriteTime,
						&dstfinfo->ftLastWriteTime) > 0) ?
						ACTION_OVERWRITE : ACTION_SKIP;
				break;

			case FOPSAME_RENAME: {
				TCHAR *fname;
				TINPUT tinput;
				FILENAMEINFOSTRUCT finfo;

				tstrcpy(pathbuf, dst);
				fname = FindLastEntryPoint(pathbuf);

				finfo.info.Function = (PPXAPPINFOFUNCTION)FilenameInfoFunc;
				finfo.info.Name = MES_TENN;
				finfo.info.RegID = NilStr;
				finfo.filename = dst;

				tinput.hOwnerWnd = hDlg;
				tinput.hWtype	= PPXH_FILENAME;
				tinput.hRtype	= PPXH_NAME_R;
				tinput.title	= MES_TENN;
				tinput.buff		= fname;
				tinput.flag		= TIEX_USEREFLINE | TIEX_SINGLEREF | TIEX_REFEXT | TIEX_USEINFO | TIEX_FIXFORPATH;
				tinput.size		= (int)(VFPS - (fname - pathbuf));
				tinput.info		= &finfo.info;
				if ( tinput.size > MAX_PATH ) tinput.size = MAX_PATH;

				if ( (srcfinfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					!GetCustDword(T("XC_sdir"), 0) ){
					setflag(tinput.flag, TIEX_USESELECT);
					tinput.firstC = 0;
					tinput.lastC = EC_LAST;
				}
				if ( tInputEx(&tinput) <= 0 ){
					opt->fop.sameSW = 0; // キャンセルされたので再選択させる
					continue;
				}
				VFSFixPath(NULL, fname, NULL, 0);
				if ( opt->fop.flags & VFSFOP_OPTFLAG_ADDNUMDEST ){
					if ( MoveFileL(dst, pathbuf) == FALSE ) continue;
					FopLog(FS, dst, pathbuf, LOG_BACKUP);
				}else{
					tstrcpy(dst, pathbuf);
				}
				return ACTION_RETRY;
			}
			case FOPSAME_OVERWRITE:
				result = ACTION_OVERWRITE;
				break;

			case FOPSAME_SKIP:
				return ACTION_SKIP;

			case FOPSAME_ARCHIVE:
				result = (srcfinfo->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ?
						ACTION_OVERWRITE : ACTION_SKIP;
				break;

			case FOPSAME_APPEND:
				result = ACTION_APPEND;
				break;

			case FOPSAME_SIZE:
				result = ((srcfinfo->nFileSizeLow != dstfinfo->nFileSizeLow) ||
						(srcfinfo->nFileSizeHigh != dstfinfo->nFileSizeHigh)) ?
						ACTION_OVERWRITE : ACTION_SKIP;
				break;

//			case FOPSAME_ADDNUMBER:
			default:
				if ( opt->fop.flags & VFSFOP_OPTFLAG_ADDNUMDEST ){
					ERRORCODE error = RenameDestFile(FS, dst, FALSE);
					if ( error != NO_ERROR ) return error;
				}else{
					if ( GetUniqueEntryName(dst) == FALSE ){
						return ERROR_ALREADY_EXISTS;
					}
				}
				return ACTION_CREATE;
		}
		break;
	}
	if ( result == ACTION_SKIP ) return result;
	// ここに来るのは、ACTION_OVERWRITE か ACTION_APPEND のみになる
	if ( opt->fop.flags & VFSFOP_OPTFLAG_ADDNUMDEST ){
		ERRORCODE error = RenameDestFile(FS, dst, TRUE);
		if ( error != NO_ERROR ) return error;
	}else{
		if ( opt->fop.flags & VFSFOP_OPTFLAG_BACKUPOLD ){
			ERRORCODE error = BackupFile(FS, dst);
			if ( error != NO_ERROR ) return error;
		}

		// 上書きするファイルが書き込み禁止等ならそれを解除
		if ( dstfinfo->dwFileAttributes & OPENERRORATTRIBUTES ){
			SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
		}
	}
	return result;
}

typedef struct {
	TCHAR SrcPath[VFPS], DestPath[VFPS];
	DWORD CountTick, LogDrawTick;
} COUNTWORKSTRUCT;

#define CountSrcFF  CountFF[0]
#define CountDestFF CountFF[1]

const TCHAR *StrExist[4] = {MES_EXNW, MES_EXEX, MES_EXOL, MES_EXNF};

ERRORCODE ShowExistsList(FOPSTRUCT *FS, COUNTWORKSTRUCT *cws)
{
	HANDLE hSrcFile, hDestFile;
	BY_HANDLE_FILE_INFORMATION srcfinfo, destfinfo;
	ERRORCODE result;
	TCHAR buf[200];

	wsprintf(buf, T("[%s:%d][%s:%d][%s:%d]%s:%d\r\n"),
		MessageText(StrExist[2]), FS->progs.info.exists[2],
		MessageText(StrExist[1]), FS->progs.info.exists[1],
		MessageText(StrExist[0]), FS->progs.info.exists[0],
		MessageText(StrExist[3]), FS->progs.info.filesall - FS->progs.info.count_exists
	);
	SendMessage(FS->hEWnd, EM_SETSEL, 0, 0);
	SendMessage(FS->hEWnd, EM_REPLACESEL, 0, (LPARAM)buf);
	SendMessage(FS->hEWnd, WM_VSCROLL, SB_TOP, 0);

	hSrcFile = CreateFile_OpenSource(cws->SrcPath, 0);
	if ( hSrcFile != INVALID_HANDLE_VALUE ){
		if ( GetFileInformationByHandle(hSrcFile, &srcfinfo) == FALSE ){
			memset(&srcfinfo, 0, sizeof(srcfinfo));
		}
		CloseHandle(hSrcFile);
	}
	hDestFile = CreateFile_OpenSource(cws->DestPath, 0);
	result = SameNameAction(FS, hDestFile, &srcfinfo, &destfinfo, cws->SrcPath, cws->DestPath);
	FS->progs.info.count_exists = 0;
	return result;
}

void Count_Exists(FOPSTRUCT *FS, COUNTWORKSTRUCT *cws, WIN32_FIND_DATA *CountFF, const TCHAR *filename, const TCHAR *SrcPath, const TCHAR *DestPath)
{
	TCHAR buf[VFPS + 64];
	DWORD tick;
	int compare;

	CreateFWriteLogWindow(FS);
	compare = FuzzyCompareFileTime(&CountSrcFF.ftLastWriteTime, &CountDestFF.ftLastWriteTime) + 1;
	FS->progs.info.exists[compare]++;
	wsprintf(buf, T("%s\t%s\r\n"), MessageText(StrExist[compare]), filename);
	FWriteLog(FS->hEWnd, buf);

	tick = GetTickCount();
	if ( FS->progs.info.count_exists++ == 0 ){
		cws->LogDrawTick = tick;

		tstrcpy(cws->SrcPath, SrcPath);
		tstrcpy(cws->DestPath, DestPath);

		SendMessage(FS->hEWnd, WM_SETREDRAW, FALSE, 0);
	}else if ( (tick - cws->LogDrawTick) > 100 ){
		cws->LogDrawTick = tick;
		SendMessage(FS->hEWnd, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(FS->hEWnd, NULL, TRUE);
		UpdateWindow(FS->hEWnd);
		SendMessage(FS->hEWnd, WM_SETREDRAW, FALSE, 0);
	}
}

void CountSub(FOPSTRUCT *FS, COUNTWORKSTRUCT *cws, TCHAR *dir, TCHAR *dest)
{
	TCHAR *dirsep, *destlast;
	HANDLE hFF; // FindFile 用ハンドル
	WIN32_FIND_DATA CountFF[2]; // ファイル情報

	dirsep = dir + tstrlen(dir);
	if ( (dirsep - dir + 5) >= VFPS ) return;

	if ( *dest != '\0' ){
		CatPath(NULL, dest, NilStr);
		destlast = dest + tstrlen(dest);
	}else{
		destlast = dest;
	}

	CatPath(NULL, dir, WildCard_All);
	hFF = VFSFindFirst(dir, &CountSrcFF);
	if ( hFF != INVALID_HANDLE_VALUE ){
		do{
			if ( IsRelativeDirectory(CountSrcFF.cFileName) ) continue;
			PeekMessageLoopSub(FS);
											// 省略希望 || 長時間 のため省略
									// ※ over flow でいきなり省略が起きるが、
									//    対策するほどではないでしょう(^^;
			if ( (FS->state != FOP_COUNT) || (cws->CountTick < GetTickCount()) ){
				FS->progs.info.filesall = 0;
				FS->progs.info.count_exists = 0;
				break;
			}
			if ( dest[0] != '\0' ){
				tstrlimcpy(destlast, CountSrcFF.cFileName, VFPS - (destlast - dest));
			}
			if ( CountSrcFF.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				*dirsep = '\0';
				if ( (dirsep - dir + tstrlen(CountSrcFF.cFileName) + 5) < VFPS ){
					CatPath(NULL, dir, CountSrcFF.cFileName);
					CountSub(FS, cws, dir, dest);
				}
			}else{
				AddDD(FS->progs.info.allsize.l, FS->progs.info.allsize.h,
						CountSrcFF.nFileSizeLow, CountSrcFF.nFileSizeHigh);
				FS->progs.info.filesall++;
				if ( dest[0] != '\0' ){
					HANDLE hDestFF;

					if ( (hDestFF = FindFirstFileL(dest, &CountDestFF)) != INVALID_HANDLE_VALUE ){
						FindClose(hDestFF);
						*dirsep = '\0';
						CatPath(NULL, dir, CountSrcFF.cFileName);
						Count_Exists(FS, cws, CountFF, dest, dir, dest);
					}
				}
			}
		}while( IsTrue(VFSFindNext(hFF, &CountSrcFF)) );
		VFSFindClose(hFF);
	}
	return;
}

BOOL CountMain(FOPSTRUCT *FS, const TCHAR *current, const TCHAR *destdir)
{
	const TCHAR *p;
	TCHAR src[VFPS], dest[VFPS], *destlast;
	int X_cntt;
	COUNTWORKSTRUCT cws;

	X_cntt = 10;
	GetCustData(T("X_cntt"), &X_cntt, sizeof X_cntt);
	if ( X_cntt <= 0 ) return TRUE;

	if ( (FS->opt.SrcDtype != VFSDT_PATH) &&
		 (FS->opt.SrcDtype != VFSDT_DLIST) &&
		 (FS->opt.SrcDtype != VFSDT_SHN) &&
		 (FS->opt.SrcDtype != VFSDT_FATIMG) &&
		 (FS->opt.SrcDtype != VFSDT_CDIMG) &&
		 (FS->opt.SrcDtype != VFSDT_STREAM) ){
		return TRUE;
	}

	if ( !(FS->opt.fop.flags & VFSFOP_OPTFLAG_NOFIRSTEXIST) &&
		 (FS->opt.SrcDtype == VFSDT_PATH) &&
		 (destdir != NULL) &&
		 !FS->opt.fop.sameSW ){
		CatPath(dest, (TCHAR *)destdir, NilStr);
		destlast = dest + tstrlen(dest);
	}else{
		dest[0] = '\0';
	}

	cws.CountTick = GetTickCount() + X_cntt * 1000;
	FS->state = FOP_COUNT;

	SetDlgItemText(FS->hDlg, IDOK, MessageText(STR_FOPSKIP));
	SetWindowText(FS->progs.hProgressWnd, MessageText(MES_IFCN));
	for ( p = FS->opt.files ; *p ; p = p + tstrlen(p) + 1 ){
		WIN32_FIND_DATA CountFF[2]; // ファイル情報
		HANDLE hFF;

		if ( IsParentDirectory(p) ) continue;
		if ( FS->opt.SrcDtype == VFSDT_SHN ){
			tstrcpy(src, p);
		}else{
			if ( VFSFullPath(src, (TCHAR *)p, current) == NULL ){
				continue;
			}
		}

		PeekMessageLoopSub(FS);
					// 省略希望 || 長時間 のため省略
						// ※ over flow でいきなり省略が起きるが、
						//    対策するほどではないでしょう(^^;
		if ( (FS->state != FOP_COUNT) || (cws.CountTick < GetTickCount()) ){
			FS->progs.info.filesall = 0;
			FS->progs.info.count_exists = 0;
			break;
		}

		hFF = VFSFindFirst(src, &CountSrcFF);
		if ( hFF != INVALID_HANDLE_VALUE ){
			VFSFindClose(hFF);
			if ( dest[0] != '\0' ) tstrcpy(destlast, CountSrcFF.cFileName);
			if ( CountSrcFF.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				CountSub(FS, &cws, src, dest);
			}else{
				AddDD(FS->progs.info.allsize.l, FS->progs.info.allsize.h,
						CountSrcFF.nFileSizeLow, CountSrcFF.nFileSizeHigh);
				FS->progs.info.filesall++;
				if ( dest[0] != '\0' ){
					HANDLE hDestFF;

					if ( (hDestFF = FindFirstFileL(dest, &CountDestFF)) != INVALID_HANDLE_VALUE ){
						FindClose(hDestFF);
						Count_Exists(FS, &cws, CountFF, CountSrcFF.cFileName, src, dest);
					}
				}
			}
		}
	}
	if ( FS->state == FOP_TOBREAK ) return FALSE;

	if ( FS->progs.info.count_exists != 0 ){
		SendMessage(FS->hEWnd, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(FS->hEWnd, NULL, TRUE);
		if ( FS->progs.info.count_exists >= 2 ){
			if ( ShowExistsList(FS, &cws) != NO_ERROR ) return FALSE;
		}
	}

	InvalidateRect(FS->progs.hProgressWnd, NULL, TRUE);
	SetWindowText(FS->progs.hProgressWnd, NilStr);
	return TRUE;
}

void ReleaseStartMutex(FOPSTRUCT *FS)
{
	if ( FS->hStartMutex == NULL ) return;
	ReleaseMutex(FS->hStartMutex);
	CloseHandle(FS->hStartMutex);
	FS->hStartMutex = NULL;
}

void USEFASTCALL EndOperation(FOPSTRUCT *FS)
{
	if ( FS->ifo != NULL ) FreeIfo(FS);
	DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
	if ( FS->hUndoLogFile != NULL ) CloseHandle(FS->hUndoLogFile);
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
		if ( (FS->opt.hReturnWnd != NULL) && IsWindow(FS->opt.hReturnWnd) ){
			SendMessage(FS->opt.hReturnWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_WINDDOWLOG, 2), 0);
		}
	}
	SetJobTask(FS->hDlg, JOBSTATE_ENDJOB);
}

BOOL Fop_ShellNameSpace(FOPSTRUCT *FS, const TCHAR *srcDIR, const TCHAR *dstDIR)
{
	LPSHELLFOLDER pParentFolder = NULL;
	LPITEMIDLIST *PIDLs = NULL;
	IDataObject *DataObject;
	IDropTarget *DropTarget = NULL;
	int count C4701CHECK;
	BOOL result = FALSE;
	HRESULT ComInitResult;

	ComInitResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if ( !((FS->opt.fop.mode == FOPMODE_COPY) ||
		   (FS->opt.fop.mode == FOPMODE_MOVE)) ){
		XMessage(FS->hDlg, STR_FOP, XM_FaERRd, T("No support this mode"));
		goto fin;
	}
	DropTarget = (IDropTarget *)GetPathInterface(FS->hDlg, dstDIR, &IID_IDropTarget, NULL);
	if ( DropTarget == NULL ){
		XMessage(FS->hDlg, STR_FOP, XM_FaERRd, T("Destination bind error"));
		goto fin;
	}
	count = FopMakePIDLTable(srcDIR, FS->opt.files, &PIDLs, &pParentFolder);
	if ( count == 0 ) goto fin;

	if ( FAILED(pParentFolder->lpVtbl->GetUIObjectOf(pParentFolder, NULL, count, (LPCITEMIDLIST *)PIDLs, &IID_IDataObject, NULL, (void **)&DataObject) )){
		XMessage(FS->hDlg, STR_FOP, XM_FaERRd, T("Source bind error"));
		goto fin;
	}
	result = CopyToDropTarget(DataObject, DropTarget, FALSE, FS->hDlg,
			(FS->opt.fop.mode == FOPMODE_COPY) ?
				DROPEFFECT_COPY : DROPEFFECT_MOVE);
	{ // DataObject の使用が終わるまで待機する(MTPがバッググラウンドで使用する)
		ULONG ref;
		TCHAR progs[] = T("\x1b\x64\x1");

		for ( ;; ){
			DataObject->lpVtbl->AddRef(DataObject);
			ref = DataObject->lpVtbl->Release(DataObject); // 使用が終わっていたら、直前の AddRef 分だけ(1)になるはず
			if ( ref <= 1 ) break;
			Sleep(100);
			PeekMessageLoopSub(FS);
			if ( FS->state == FOP_TOBREAK ) break;

			SetWindowText(FS->progs.hProgressWnd, progs);
			progs[2] += (TCHAR)1;
			if ( progs[2] > '\xa' ) progs[2] = '\x1';
		}
		if ( ref > 0 ) DataObject->lpVtbl->Release(DataObject);
	}


	if ( result == FALSE ){
		XMessage(FS->hDlg, STR_FOP, XM_FaERRd, T("Operation fault"));
	}
	FS->DestroyWait = TRUE; // メディアプレイヤーへ処理を行うとき、メッセージポンプをしばらく動かす対策を有効にする
fin:
	if ( pParentFolder != NULL ) pParentFolder->lpVtbl->Release(pParentFolder);
	if ( PIDLs != NULL ){
		FreePIDLS(PIDLs, count); // C4701ok
		HeapFree(ProcHeap, 0, PIDLs);
	}
	if ( DropTarget != NULL ) DropTarget->lpVtbl->Release(DropTarget);
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();

	SetJobTask(FS->hDlg, JOBSTATE_ENDJOB);
	ReleaseStartMutex(FS);
	EndOperation(FS);

/*	処理完了後に通知を送りたいが、処理完了にかなり遅延があるのでこれだとむりぽい
	if ( FS->info != NULL ){
		TCHAR path[VFPS];
		PPXCMDENUMSTRUCT IInfo;

		if ( PPxEnumInfoFunc(FS->info, '2', path, &IInfo) ){
			if ( tstrcmp(path, dstDIR) == 0 ){
				PostMessage(
			}
		}
	}
*/
	return result;
}


void JobErrorBox(FOPSTRUCT *FS, const TCHAR *msg, ERRORCODE err)
{
	HWND hWnd = FS->hDlg;

	DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
	SetTaskBarButtonProgress(hWnd, TBPF_ERROR, 0);
	SetJobTask(hWnd, JOBSTATE_ERROR);
	PPErrorBox(hWnd, msg, err);
	SetJobTask(hWnd, JOBSTATE_DEERROR);
}

BOOL InitUndoLog(FOPSTRUCT *FS)
{
	HWND hWaitDlg = NULL;
	WAITDLGSTRUCT wds;
	TCHAR buf[VFPS];

	wds.user = -1;
	GetUndoLogFileName(buf);
	for ( ; ; ){
		ERRORCODE result;
		MSG msg;

		FS->hUndoLogFile = CreateFileL(buf, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( FS->hUndoLogFile != INVALID_HANDLE_VALUE ) break;
		FS->hUndoLogFile = NULL;
		result = GetLastError();

		if ( result != ERROR_SHARING_VIOLATION ){
			PPErrorBox(FS->hDlg, STR_FOPUNDOLOG, result);
			ReleaseStartMutex(FS);
			return FALSE;
		}

		if ( hWaitDlg == NULL ){
			wds.md.title = T("Fild operation(undo)");
			hWaitDlg = CreateDialogParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL), NULL, WaitDlgBox, (LPARAM)&wds);
			ShowWindow(hWaitDlg, SW_SHOWNOACTIVATE);
		}
		if ( wds.user != -1 ) break;

		while ( IsTrue(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) ){
			if ( msg.message == WM_QUIT ) break;
			if ( (hWaitDlg == NULL) || (IsDialogMessage(hWaitDlg, &msg) == FALSE) ){
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		Sleep(100);
	}
	if ( hWaitDlg != NULL ) DestroyWindow(hWaitDlg);
	if ( wds.user == 0 ){
		ReleaseStartMutex(FS);
		return FALSE;
	}
	#ifdef UNICODE
	{
		DWORD size;
		WriteFile(FS->hUndoLogFile, UCF2HEADER, UCF2HEADERSIZE, &size, NULL);
	}
	#endif
	return TRUE;
}

typedef struct {
	PPXAPPINFO info;
	FOPSTRUCT *FS;
	const TCHAR *srcDIR;
	const TCHAR *dstDIR;
	int srcLen;
} FIO_INFO;

DWORD_PTR USECDECL FioInfo(FIO_INFO *fio, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case '1':
			tstrcpy(uptr->enums.buffer, fio->srcDIR);
			break;
		case 'C':
			if ( memcmp((const TCHAR *)uptr->enums.enumID,
					fio->srcDIR, TSTROFF(fio->srcLen)) ){
				tstrcpy(uptr->enums.buffer, (const TCHAR *)uptr->enums.enumID);
			}else{
				tstrcpy(uptr->enums.buffer, (const TCHAR *)uptr->enums.enumID + fio->srcLen + 1);
			}
			break;
		case '2':
			tstrcpy(uptr->enums.buffer, fio->dstDIR);
			break;

		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = (INT_PTR)fio->FS->opt.files;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
			if ( *(const TCHAR *)uptr->enums.enumID != '\0' ){
				uptr->enums.enumID = (INT_PTR)(uptr->enums.enumID +
						tstrlen((const TCHAR *)uptr->enums.enumID) + 1);
				if ( *(const TCHAR *)uptr->enums.enumID != '\0' ){
					return 1;
				}
			}
			return 0;

//		case PPXCMDID_ENDENUM:		//列挙終了…何もしない

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

BOOL USEFASTCALL CheckSaveDrive(struct FopOption *opt, _In_ const TCHAR *src, _In_ const TCHAR *dst)
{
	const TCHAR *srcdend;

	if ( opt->OnDriveMove ) return TRUE;
	if ( opt->fop.mode != FOPMODE_MOVE ) return FALSE;

	srcdend = VFSGetDriveType(src, NULL, NULL);
	if ( (srcdend == NULL) || (srcdend == src) ) return FALSE;
	if ( tstrnicmp(src, dst, srcdend - src) ) return FALSE;
	return TRUE;
}

DWORD GetFileHeaderImg(const TCHAR *filename, BYTE *header, DWORD headersize)
{
	HANDLE hFile;
	DWORD fsize;

	hFile = CreateFileL(filename, GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return 0;

	fsize = ReadFileHeader(hFile, header, headersize);
	CloseHandle(hFile);
	return fsize;
}

BOOL FileImageOperation(FOPSTRUCT *FS, TCHAR *srcDIR, const TCHAR *dstDIR)
{
	void *dt_opt;
	DWORD fsize;
	BYTE header[VFS_check_size];
	FIO_INFO fio;

	fio.info.Function = (PPXAPPINFOFUNCTION)FioInfo;
	fio.info.Name = T("Fop_arc");
	fio.info.RegID = NilStr;
	fio.info.hWnd = FS->hDlg;
	fio.FS = FS;
	fio.srcDIR = srcDIR;
	fio.dstDIR = dstDIR;
	fio.srcLen = tstrlen32(srcDIR);

	fsize = GetFileHeaderImg(srcDIR, header, sizeof(header));
	if ( fsize == 0 ) return FALSE;
	switch( VFSCheckDir(srcDIR, header, fsize, &dt_opt) ){
		case VFSDT_UN: {
			TCHAR buf[CMDLINESIZE];

			if ( NO_ERROR == UnArc_Extract(&fio.info, dt_opt, UNARCEXTRACT_PART,
					buf, XEO_NOEDIT) ){
				TCHAR OldCurrentDir[VFPS];
				int result;

				GetCurrentDirectory(TSIZEOF(OldCurrentDir), OldCurrentDir);
				SetCurrentDirectory(srcDIR);
				// ●1.27 fio.info でログ出力
				result = RunUnARCExec(&fio.info, dt_opt, buf, NilStr);
				SetCurrentDirectory(OldCurrentDir);
				if ( result == 0 ) break;
			}
			return FALSE;
		}
		default:
			return FALSE;
	}
	ReleaseStartMutex(FS);
	EndOperation(FS);
	return TRUE;
}

HRESULT InitFopClasses(void)
{
	WNDCLASS wcClass;
											// クラスを登録する
	wcClass.style			= CS_PARENTDC;
	wcClass.lpfnWndProc		= PPxStaticProc;
	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= sizeof(LONG_PTR); // 0:進捗バーの描画位置保存
	wcClass.hInstance		= DLLhInst;
	wcClass.hIcon			= NULL;
	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcClass.hbrBackground	= WNDCLASSBRUSH(COLOR_3DFACE + 1);
	wcClass.lpszMenuName	= NULL;
	wcClass.lpszClassName	= T(PPXSTATICCLASS);
	RegisterClass(&wcClass);

	wcClass.style			= 0;
	wcClass.lpfnWndProc		= DefDlgProc;
//	wcClass.cbClsExtra		= 0;
	wcClass.cbWndExtra		= DLGWINDOWEXTRA;
//	wcClass.hInstance		= DLLhInst; // ↑と同じ
	wcClass.hIcon			= LoadIcon(DLLhInst, MAKEINTRESOURCE(Ic_FOP));
//	wcClass.hCursor			= LoadCursor(NULL, IDC_ARROW); // ↑と同じ
//	wcClass.hbrBackground	= WNDCLASSBRUSH(COLOR_3DFACE + 1); // ↑と同じ
//	wcClass.lpszMenuName	= NULL; // ↑と同じ
	wcClass.lpszClassName	= T(PPFileOpWinClass);
	RegisterClass(&wcClass);

	GetCustData(T("X_flst"), &X_flst, sizeof(X_flst));
	if ( X_flst[0] == 2 ){ // OS 自動補完を使うときは初期化
		return CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	}else{
		return E_FAIL;
	}
}

VFSDLL BOOL PPXAPI PPxFileOperation(HWND hWnd, VFSFILEOPERATION *fileop)
{
	FILEOPERATIONDLGBOXINITPARAMS fopip;
	BOOL result;
	HRESULT ComInitResult;

	fileop->hLogWnd = NULL;
	ComInitResult = InitFopClasses();

	fopip.fileop = fileop;
	fopip.FS = HeapAlloc(DLLheap, 0, sizeof(FOPSTRUCT));
	if ( fopip.FS != NULL ){
		result = (BOOL)PPxDialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_FOP),
				hWnd, FileOperationDlgBox, (LPARAM)&fopip);
		// メモリ解放
		if ( IsTrue(fopip.FS->opt.AllocFiles) && (fopip.FS->opt.files != NULL) ){
			// ThFree(fopip.FS->opt.files) 相当
			HeapFree(DLLheap, 0, (void *)fopip.FS->opt.files);
		}
		if ( fopip.FS->opt.compcmd != NULL ){
			HeapFree(DLLheap, 0, (void *)fopip.FS->opt.compcmd);
		}
		HeapFree(DLLheap, 0, fopip.FS->page.hWnds);
		FreeFN_REGEXP(&fopip.FS->maskFn);
		HeapFree(DLLheap, 0, fopip.FS);
	}else{
		result = FALSE;
	}
	if ( fileop->flags & VFSFOP_NOTIFYREADY ){ // 初期化前に終了した時用
		SetEvent(fileop->hReadyEvent);
		EnableWindow(fileop->hReturnWnd, TRUE);
	}
	if ( fileop->flags & VFSFOP_FREEFILES ){
		HeapFree(ProcHeap, 0, (LPVOID)fileop->files);
	}
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
	return result;
}

#define STRSTEP 0x400
TCHAR *MakeFOPlistFromPPx(PPXAPPINFO *info)
{
	HANDLE heap;
	TCHAR *names, *p, *next;
	PPXCMDENUMSTRUCT work;
	DWORD allocsize = TSTROFF(STRSTEP);
	PPXCMD_F fbuf;

	if ( info == NULL ) return NULL;
										// 保存用のメモリを確保する
	heap = ProcHeap;
	names = HeapAlloc(heap, 0, allocsize);
	if ( names == NULL ){
		XMessage(NULL, STR_FOP, XM_FaERRd, T("GetFiles - AllocError"));
		return NULL;
	}
	p = names;
	next = names + STRSTEP - VFPS;

	PPxEnumInfoFunc(info, PPXCMDID_STARTENUM, fbuf.dest, &work);
	for( ; ; ){
		fbuf.source = FullPathMacroStr;
		fbuf.dest[0] = '\0';
		Get_F_MacroData(info, &fbuf, &work);
		if ( fbuf.dest[0] == '\0' ) break;
		if ( IsParentDirectory(fbuf.dest) == FALSE ){
			if ( p >= next ){				// メモリの追加
				TCHAR *np;

				allocsize += TSTROFF(STRSTEP);
				np = HeapReAlloc(heap, 0, names, allocsize);
				if ( np == NULL ){
					XMessage(NULL, STR_FOP, XM_FaERRd, T("GetFiles - ReAllocError"));
					HeapFree(heap, 0, names);
					names = NULL;
					break;
				}else{
					p = np + (p - names);
					next = np + TSIZEOF(allocsize) - VFPS;
					names = np;
				}
			}
			tstrcpy(p, fbuf.dest);
			p += tstrlen(p) + 1;
		}
		if ( PPxEnumInfoFunc(info, PPXCMDID_NEXTENUM, fbuf.dest, &work) == 0 ){
			break;
		}
	}
	PPxEnumInfoFunc(info, PPXCMDID_ENDENUM, fbuf.dest, &work);
	*p++ = '\0';
	*p = '\0';
	return names;
}

TCHAR *MakeFOPlistFromParam(const TCHAR *param, const TCHAR *path)
{
	ThSTRUCT th;
									// レスポンスファイル
	if ( (param[0] == '@') && (GetFileAttributesL(param + 1) != BADATTR)){
		TCHAR src[VFPS];
		TCHAR *mem, *ps, *pd;

		VFSFullPath(src, (TCHAR *)param + 1, path);

		if ( LoadTextImage(src, (TCHAR **)&mem, (TCHAR **)&ps, NULL) != NO_ERROR){
			return NULL;
		}
		pd = mem;
		while (*ps){
			TCHAR *oldpd;

			oldpd = pd;
			if ( *ps == ';' ){ // 行頭「;」はコメント扱い
				while ( (*ps != '\r') && (*ps != '\n') ) ps++;
			}else if ( *ps == '\"' ){
				ps++;
				while( *ps ){
					if ( *ps == '\r' || *ps == '\n' ) break;
					if ( *ps == '\"' ){
						ps++;
						break;
					}
					*pd++ = *ps++;
				}
				while ( (*ps != '\r') && (*ps != '\n') ) ps++;
			}else{
				while( *ps ){
					if ( *ps == '\r' || *ps == '\n' ) break;
					*pd++ = *ps++;
				}
			}
			while ( (*ps == '\r') || (*ps == '\n') ) ps++;
			if (oldpd != pd) *pd++ = '\0';
		}
		*pd = '\0';
		return mem;
	}
									// ワイルドカード
	if ( tstrchr(param, '*') || tstrchr(param, '?') ){
		HANDLE			hFF;
		WIN32_FIND_DATA	ff;
		FN_REGEXP fn;
		const TCHAR *p;
		TCHAR mask[VFPS], hpath[VFPS];
		TCHAR src[VFPS], name[VFPS];

		p = VFSFindLastEntry(param);
		if ( *p == '\\' ){
			tstrcpy(mask, p + 1);
			tstrcpy(hpath, param);
			hpath[p - param] = '\0';
			VFSFullPath(NULL, hpath, path);
			CatPath(src, hpath, WildCard_All);
		} else {
			tstrcpy(mask, p);
			tstrcpy(hpath, path);
			CatPath(src, (TCHAR *)path, WildCard_All);
		}
		hFF = FindFirstFileL(src, &ff);

		if ( hFF == INVALID_HANDLE_VALUE ) return NULL;
		ThInit(&th);

		MakeFN_REGEXP(&fn, mask);
		do{
			if ( IsRelativeDirectory(ff.cFileName) ) continue;
			if( FinddataRegularExpression(&ff, &fn) ){
				CatPath(name, hpath, ff.cFileName);
				ThAddString(&th, name);
			}
		}while( IsTrue(FindNextFile(hFF, &ff)) );
		FreeFN_REGEXP(&fn);
		FindClose(hFF);
		ThAddString(&th, NilStr);
		return (TCHAR *)th.bottom;
	}
	ThInit(&th);
	ThAddString(&th, param);
	ThAddString(&th, NilStr);
	return (TCHAR *)th.bottom;
}

HWND FopGetMsgParentWnd(FOPSTRUCT *FS)
{
	HWND hWnd = FS->hDlg;
	WINDOWPLACEMENT wp;

	wp.length = sizeof(wp);
	GetWindowPlacement(hWnd, &wp);
	if ( (wp.showCmd == SW_SHOWNORMAL) || (wp.showCmd == SW_SHOWMAXIMIZED) ){
		return hWnd;
	}
	if ( (FS->opt.hReturnWnd != NULL) && IsWindow(FS->opt.hReturnWnd) ){
		return FS->opt.hReturnWnd;
	}
	return hWnd;
}


BOOL OperationStart(FOPSTRUCT *FS)
{
	TCHAR srcPath[VFPS], dstPath[VFPS];
	TCHAR srcDIR[VFPS], dstDIR[VFPS], *srcdir, *dstdir;
	int srctype, dsttype; // 各パスの種類
	DWORD dstattr;
	DWORD mainresult = NO_ERROR;
	HINTERNET hFTP[2];
	const TCHAR *fileptr;
	HWND hDlg = FS->hDlg;

	CheckAndInitIfo(FS);

	FS->errorcount = 0;
	memset(&FS->progs.info, 0, sizeof(FS->progs.info));
	FS->flat = FS->opt.fop.flags & VFSFOP_OPTFLAG_FLATMODE;
	FS->BackupDirCreated = FALSE;
	FS->reparsemode = FOPR_UNKNOWN;
	if ( FS->opt.fop.flags & (VFSFOP_OPTFLAG_SYMCOPY_SYM | VFSFOP_OPTFLAG_SYMCOPY_FILE) ){
		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SYMCOPY_SYM ){
			FS->reparsemode = IDYES; // シンボリックとしてコピー
		}else{
			FS->reparsemode = IDNO; // ファイルとしてコピー
		}
	}

	FS->hUndoLogFile = NULL;
	SaveControlData(FS);
	SetFopLowPriority(FS);

	if ( FS->opt.files == NULL ){
		XMessage(NULL, STR_FOP, XM_FaERRd, T("No source"));
		return FALSE;
	}
	FS->hide = SetJobTask(hDlg, JOBSTATE_STARTJOB | JOBFLAG_ENABLEHIDE | (FS->opt.fop.mode + JOBSTATE_FOP_MOVE));
	if ( FS->hide ){
		if ( (Sm->JobList.hWnd != NULL) && (Sm->JobList.hidemode == JOBLIST_HIDE) && !FS->testmode ){
			ShowWindow(hDlg, SW_HIDE);
		}else{
			FS->hide = FALSE;
		}
	}

	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_PREVENTSLEEP ){
		StartPreventSleep(hDlg);
	}

	FS->progs.ProgTick = 0;
	FS->progs.CapTick = 0;
	FS->progs.nowper = -1;

	if ( (FS->opt.fop.mode == FOPMODE_LINK) && (DCreateHardLink == NULL) ){
		GETDLLPROCT(hKernel32, CreateHardLink);
		if ( DCreateHardLink == NULL ){
			XMessage(hDlg, MES_TMHL, XM_FaERRd, MES_ENSO);
			return FALSE;
		}
	}
	if ( (FS->opt.fop.mode == FOPMODE_MOVE) && (FS->DestDir[0] == '\0') ){
		FS->renamemode = TRUE;
	}else{
		FS->renamemode = FALSE;
	}
	if ( FS->opt.fop.mode == FOPMODE_DELETE ){
		DWORD X_wdel[4] = X_wdel_default;

		if ( FS->testmode ){
			XMessage(hDlg, NULL, XM_GrERRld, T("test mode not support"));
			return FALSE;
		}

		GetCustData(T("X_wdel"), &X_wdel, sizeof(X_wdel));

		FS->DelStat.OldTime = GetTickCount();
		FS->DelStat.count = 0;
		FS->DelStat.useaction = 0;
		FS->DelStat.noempty = FALSE;
		FS->DelStat.flags = X_wdel[0];
		FS->DelStat.warnattr = X_wdel[1];
		FS->DelStat.info = &FS->DelInfo;
		FS->DelInfo.Name = STR_FOP;
		FS->DelInfo.Function = (PPXAPPINFOFUNCTION)FopDeleteInfoFunc;
		FS->DelInfo.hWnd = hDlg;
		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_NOWDELEXIST ){
			resetflag(FS->DelStat.flags, VFSDE_WARNFILEINDIR);
		}
		if ( FS->opt.erroraction ){
			FS->DelStat.useaction = FS->opt.erroraction;
		}
		if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
			FS->DelStat.useaction = IDIGNORE;
		}
		FS->DelStat.flags |= (FS->opt.fop.flags & (3 << VFSFOP_OPTFLAG_SYMDEL_SHIFT)) >> (VFSFOP_OPTFLAG_SYMDEL_SHIFT - VFSDE_SYMDEL_SHIFT);
	}
	if ( FS->opt.SrcDtype == VFSDT_UNKNOWN ){
		VFSGetDriveType(FS->opt.files, &FS->opt.SrcDtype, 0);
		if ( FS->opt.SrcDtype == VFSDT_UNKNOWN ){
			VFSGetDriveType(FS->opt.source, &FS->opt.SrcDtype, 0);
		}else if ( FS->opt.SrcDtype == VFSPT_RAWDISK ){
			tstrcpy(FS->opt.source, FS->opt.files);
		}
		if ( FS->opt.SrcDtype == VFSPT_UNC ){
			FS->opt.SrcDtype = VFSDT_PATH;
		}else if ( FS->opt.SrcDtype < VFSDT_SHN ){
			FS->opt.SrcDtype = VFSDT_PATH;
		}
	}
										// Window サイズ変更 ----------
	SetWindowY(FS, WINY_STAT);
										// コピー元のパス形成
	if ( FS->opt.SrcDtype != VFSDT_SHN ){
		VFSFixPath(srcDIR, FS->opt.source, NULL, VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_NOFIXEDGE);
		srcdir = VFSGetDriveType(srcDIR, &srctype, NULL);
		if ( srcdir == NULL ){
			XMessage(hDlg, NULL, XM_GrERRld, T("Bad source path"));
			return FALSE;
		}
		if ( srctype < 0 ){
			srcDIR[0] = '?';
			srcDIR[1] = '\0';
			srcdir = srcDIR;
		}
	}else{
		srcDIR[0] = '?';
		srcDIR[1] = '\0';
		srcdir = srcDIR;
		srctype = VFSPT_SHN_DESK;
	}
										// コピー先のパス形成
	VFSFixPath(dstDIR, FS->DestDir, FS->opt.source, VFSFIX_VFPS | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
	dstdir = VFSGetDriveType(dstDIR, &dsttype, NULL);
	if ( (dsttype < 0) || (dstdir == NULL) ){
		VFSFixPath(dstDIR, FS->DestDir, FS->opt.source, VFSFIX_VFPS | VFSFIX_NOFIXEDGE);
		dstdir = VFSGetDriveType(dstDIR, &dsttype, NULL);
		if ( (dsttype >= 0) || (dstdir == NULL) ){
			XMessage(hDlg, NULL, XM_GrERRld, T("Bad dest. path"));
			return FALSE;
		}
	}
	if ( !(FS->opt.fopflags & VFSFOP_AUTOSTART) ){
		TCHAR *lastp = NULL;

		// ヒストリ登録内容を必ず末尾「\」にする
		if ( (dsttype == VFSPT_DRIVE) || (dsttype == VFSPT_UNC) ){
			lastp = VFSFindLastEntry(dstDIR);
			if ( *lastp == '\\' ) lastp++;
			if ( *lastp != '\0' ){
				lastp += tstrlen(lastp);
				*lastp = '\\';
				*(lastp + 1) = '\0';
			}
		}
		WriteHistory(PPXH_DIR, dstDIR, 0, NULL);
		if ( lastp != NULL ) *lastp = '\0';
	}

	SetWindowText(hDlg, dstDIR);
										// コントロール変更 -----------
	Enables(FS, FALSE, FALSE);

	dstdir = GetDriveEnd(dstdir, dsttype);
	srcdir = GetDriveEnd(srcdir, srctype);

	if ( FS->opt.fop.mode == FOPMODE_UNDO ) return UndoCommand(FS);
	{	// 待機 ===============================================================
		DWORD X_fopw = GetCustDword(T("X_fopw"), 0);

		FS->hStartMutex = (X_fopw == 1) ?
				CreateMutex(NULL, FALSE, PPXJOBMUTEX) : NULL;
		if (	!(FS->opt.fop.flags & VFSFOP_OPTFLAG_QSTART) &&
				X_fopw && IsTrue(CheckJobWait(FS->hStartMutex)) ){
			const TCHAR *waitstr;

			FS->state = FOP_WAIT;
			SetDlgItemText(hDlg, IDOK, MessageText(STR_FOPSTART));
			waitstr = MessageText(STR_WAITOPERATION);
			SetWindowText(FS->progs.hProgressWnd, waitstr);
			SetStateOnCaption(FS, waitstr);
			SetJobTask(hDlg, JOBSTATE_WAITJOB);
			for ( ; ; ){
				if ( FS->hStartMutex != NULL ){ // X_fopw == 1
					DWORD result;

					result = MsgWaitForMultipleObjects(1, &FS->hStartMutex, FALSE,
							INFINITE, QS_ALLEVENTS | QS_SENDMESSAGE);
					if ( result == WAIT_OBJECT_0 ) break;
					if ( result == WAIT_ABANDONED_0 ) break;
					PeekMessageLoop(FS);
				}else{ // X_fopw == 2
					MSG msg;

					while( (int)GetMessage(&msg, NULL, 0, 0) > 0 ){
						if (IsDialogMessage(FS->hDlg, &msg) == FALSE){
							TranslateMessage(&msg);
							DispatchMessage(&msg);
						}
						if ( FS->state != FOP_WAIT ) break;
					}
				}
				if ( FS->state != FOP_WAIT ){
					ReleaseStartMutex(FS);
					if ( IsTrue(FS->Cancel) ){
						SetJobTask(hDlg, JOBSTATE_FINWJOB);
						SetJobTask(hDlg, JOBSTATE_ENDJOB);
						EndOperation(FS);
						return FALSE;
					}
					break;
				}
			}
			SetJobTask(hDlg, JOBSTATE_FINWJOB);
			SetStateOnCaption(FS, NULL);
		}
	}
	// ========================================================================

	if ( dsttype < 0 ){ // shell name space
		return Fop_ShellNameSpace(FS, srcDIR, dstDIR);
	}

	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_UNDOLOG ){
		if ( InitUndoLog(FS) == FALSE ) return FALSE;
	}

	// ========================================================================
	// src が FTP なら、サイズ算出ができない
	if ( srctype == VFSPT_FTP ){
		setflag(FS->opt.fop.flags, VFSFOP_OPTFLAG_NOCOUNT);
	// dst の調査。先がFTPだったり削除モードなら調査できない
	}else if ( (dsttype != VFSPT_FTP) && (dsttype != VFSPT_AUXOP) && (FS->opt.fop.mode != FOPMODE_DELETE) ){
//		TCHAR *sptr = tstrstr(dstDIR, T("::"));

//		if ( sptr != NULL ) *sptr = '\0';
		dstattr = GetFileAttributesL(dstDIR);
//		if ( sptr != NULL ) *sptr = ':';
		if ( (FS->testmode == FALSE) && (dstattr == BADATTR) ){
			DWORD result;

			result = GetLastError();
			// dstDIR が "\\.\Volume{…}" とかの場合、この状態になる
			if ( (result == ERROR_INVALID_PARAMETER) && (dstDIR[0] == '\\') ){
				result = NO_ERROR;
			}

			if ( (result != ERROR_NOT_READY) && (result != NO_ERROR) ){
			// 実際は ERROR_PATH_NOT_FOUND が多いらしい…TT
				TCHAR *p;

				p = tstrstr(dstDIR, StrListFileid);
				if ( p != NULL ){
					BOOL listresult;

					*p = '\0';
					listresult = OperationStartListFile(FS, srcDIR, dstDIR);
					ReleaseStartMutex(FS);
					EndOperation(FS);
					return listresult;
				}

				if ( !(FS->opt.fop.flags & VFSFOP_OPTFLAG_NOWCREATEDIR) &&
				   PMessageBox(FopGetMsgParentWnd(FS), MES_QCRD, STR_FOPWARN, MB_OKCANCEL) != IDOK){
					ReleaseStartMutex(FS);
					EndOperation(FS);
					return FALSE;
				}
				result = MakeDirectories(dstDIR, NULL);
				if ( result == NO_ERROR ){
					FopLog(FS, dstDIR, NULL, LOG_MAKEDIR);
				}
			}
			if ( result != NO_ERROR ){
				JobErrorBox(FS, dstDIR, result);
				ReleaseStartMutex(FS);
				EndOperation(FS);
				return FALSE;
			}
		}else if ( (FS->renamemode == FALSE) &&
					!(dstattr & FILE_ATTRIBUTE_DIRECTORY) ){
			int result;

			if ( !((FS->opt.fop.mode == FOPMODE_COPY) ||
				   (FS->opt.fop.mode == FOPMODE_MOVE)) ){
				XMessage(FS->hDlg, STR_FOP, XM_FaERRd, T("No support this mode"));
				return FALSE;
			}

			result = OperationStartToFile(FS, srcDIR, dstDIR);
			if ( result >= 0 ){
				ReleaseStartMutex(FS);
				EndOperation(FS);
				return result;
			}
		}
	}
										// ファイル数算出 ---------------------
	{
		const TCHAR *cfileptr;

		SetTaskBarButtonProgress(hDlg, TBPF_INDETERMINATE, 0);
		cfileptr = FS->opt.files;
		while ( *cfileptr != '\0' ){
			cfileptr = cfileptr + tstrlen(cfileptr) + 1;
			FS->progs.info.markall++;
		}
	}

	if ( (FS->opt.fop.mode != FOPMODE_SHORTCUT) &&
		 (FS->opt.fop.mode != FOPMODE_LINK) &&
		 (FS->opt.fop.mode != FOPMODE_UNDO) &&
		 (FS->opt.fop.mode != FOPMODE_SYMLINK) &&
		 !(FS->opt.fop.flags & VFSFOP_OPTFLAG_NOCOUNT) &&
		 (FS->testmode == FALSE) &&
		 (CountMain(FS, srcDIR, (dsttype == VFSPT_FTP) ? NULL : dstDIR) == FALSE) ){
		ReleaseStartMutex(FS);
		EndOperation(FS);
		return FALSE;
	}
										// コントロール変更 -----------
	FS->state = FOP_BUSY;
	SetDlgItemText(hDlg, IDOK, MessageText(STR_FOPPAUSE));
	UpdateWindow(hDlg);

							// (複写&削除)か移動かを判断
	FS->opt.OnDriveMove =
			(FS->opt.fop.mode == FOPMODE_MOVE) &&
			( ((srcdir - srcDIR) == (dstdir - dstDIR)) &&
					!memcmp(srcDIR, dstDIR, TSTROFF(srcdir - srcDIR)) );

	// 名前加工を行うかを確認する
	FS->opt.UseNameFilter = 0;

	if ( FS->opt.rename[0] != '\0' ){
		setflag(FS->opt.UseNameFilter, NameFilter_Use | NameFilter_Rename);
										// マクロ展開
		if ( (FS->opt.rename[0] == RENAME_EXTRACTNAME) ||
			 (FS->opt.fop.filter & VFSFOP_FILTER_EXTRACTNAME) ){
			setflag(FS->opt.UseNameFilter, NameFilter_ExtractName);
										// 名前変更に正規表現を使うか
		}else if ( tstrchr(FS->opt.rename, '/') != NULL ){
			if ( FALSE == InitRegularExpressionReplace( // ※ FS->opt.rename 破壊
					&FS->opt.rexps, FS->opt.rename, TRUE) ){
				EndOperation(FS);
			return FALSE;
			}
		}
	}
	if ( IsTrue(FS->opt.fop.delspc) ||
		 IsTrue(FS->opt.fop.sfn) ||
		 (FS->opt.fop.chrcase != 0) ||
		 (FS->opt.fop.filter & VFSFOP_FILTER_DELNUM) ){
		setflag(FS->opt.UseNameFilter, NameFilter_Use);
	}

	FS->opt.burst = FS->opt.fop.useburst;

	if ( (FS->opt.SrcDtype == VFSDT_ZIPFOLDER) ||
		 (FS->opt.SrcDtype == VFSDT_LZHFOLDER) ||
		 (FS->opt.SrcDtype == VFSDT_CABFOLDER) ||
		 (FS->opt.SrcDtype == VFSDT_FATIMG)  ||
		 (FS->opt.SrcDtype == VFSDT_CDIMG) ||
		 (FS->opt.SrcDtype == VFSDT_FATDISK) ||
		 (FS->opt.SrcDtype == VFSDT_CDDISK) ){
		ImgExtract(FS, srcDIR, dstDIR);
		EndOperation(FS);
		ReleaseStartMutex(FS);
		return TRUE;
	}

	if ( srctype == VFSPT_FTP ){
		ERRORCODE result = OpenFtp(srcDIR, hFTP);
		if ( result != NO_ERROR ){
			ReleaseStartMutex(FS);
			EndOperation(FS);
			return FALSE;
		}
	}else if ( dsttype == VFSPT_FTP ){
		ERRORCODE result = OpenFtp(dstDIR, hFTP);
		if ( result != NO_ERROR ){
			ReleaseStartMutex(FS);
			EndOperation(FS);
			return FALSE;
		}
	}
										// メインループ ---------------
	if ( (FS->opt.fop.flags & VFSFOP_OPTFLAG_AUTOROFF) &&
		 (FS->opt.fop.AtrMask & FILE_ATTRIBUTE_READONLY) ){
		VFSFullPath(dstPath, (TCHAR *)FS->opt.files, srcDIR);
		GetDriveName(srcPath, dstPath);
		if ( GetDriveType(srcPath) == DRIVE_CDROM ){
			resetflag(FS->opt.fop.AtrMask, FILE_ATTRIBUTE_READONLY);
			resetflag(FS->opt.fop.AtrFlag, FILE_ATTRIBUTE_READONLY);
		}
	}
	FopLog(FS, srcDIR, dstDIR, LOG_DIR);

	// srcDIR が書庫ファイルの時の展開処理
	if ( (FS->opt.SrcDtype != VFSDT_SHN) &&
		 (FS->opt.SrcDtype != VFSDT_LFILE) ){
		DWORD srcdirattr;

		srcdirattr = GetFileAttributesL(srcDIR);
		if ( (srcdirattr != BADATTR) && !(srcdirattr & FILE_ATTRIBUTE_DIRECTORY) ){
			BOOL result;

			result = FileImageOperation(FS, srcDIR, dstDIR);
			if ( IsTrue(result) ) return result;
		}
	}
	for ( fileptr = FS->opt.files;
		 *fileptr != '\0';
		 fileptr = fileptr + tstrlen(fileptr) + 1, FS->progs.info.mark++ ){

		if ( IsParentDirectory(fileptr) ) continue;
		if ( FS->opt.SrcDtype == VFSDT_SHN ){
			tstrcpy(srcPath, fileptr);
		}else{
			if ( VFSFullPath(srcPath, (TCHAR *)fileptr, srcDIR) == NULL ){
				FWriteErrorLogs(FS, srcPath, T("Srcpath"), PPERROR_GETLASTERROR);
				continue;
			}
		}
		if ( FS->renamemode == FALSE ){
			const TCHAR *entry;

			if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_KEEPDIR ){
				entry = fileptr;
			}else{
				entry = VFSFindLastEntry(fileptr);
			}
			if ( *entry == '\\' ) entry++;
			if ( VFSFullPath(dstPath, (TCHAR *)entry, dstDIR) == NULL ){
				FWriteErrorLogs(FS, dstPath, T("Dstpath"), PPERROR_GETLASTERROR);
				continue;
			}
			if ( (FS->opt.fop.flags & VFSFOP_OPTFLAG_KEEPDIR) &&
				 (FS->testmode == FALSE) ){
				TCHAR *lastentry;

				lastentry = VFSFindLastEntry(dstPath);
				*lastentry = '\0';
				if ( BADATTR == GetFileAttributesL(dstPath) ){
					MakeDirectories(dstPath, NULL);
				}
				*lastentry = '\\';
			}
		}else{
			tstrcpy(dstPath, srcPath);
		}
		FS->progs.srcpath = srcPath;

		if ( srctype == VFSPT_FTP ){
			mainresult = FileOperationSourceFtp(FS, hFTP, fileptr, dstPath);
		}else if ( dsttype == VFSPT_FTP ){
			mainresult = FileOperationDestinationFtp(FS, hFTP, srcPath, fileptr);
		}else if ( srctype == VFSPT_AUXOP ){
			mainresult = FileOperationAux(/*FS, */T("get"), srcDIR, fileptr, dstPath);
		}else if ( dsttype == VFSPT_AUXOP ){
			mainresult = FileOperationAux(/*FS, */T("store"), dstDIR, srcPath, fileptr);
		}else{
			DWORD attr;

			attr = GetFileAttributesL(srcPath);
			if ( attr == BADATTR ){
				int mode;

				attr = 0;
				if ( VFSGetDriveType(srcPath, &mode, NULL) != NULL ){
					if ( (mode <= VFSPT_SHN_DESK) || (mode == VFSPT_SHELLSCHEME) ){
						VFSGetRealPath(NULL, srcPath, srcPath);
						attr = GetFileAttributesL(srcPath);
						if ( attr == BADATTR ) attr = 0;
					}
				}
			}
			if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
				if ( !(FS->opt.fop.filter & VFSFOP_FILTER_NODIRFILTER) ){
					if ( LFNfilter(&FS->opt, dstPath) > ERROR_NO_MORE_FILES ) break;
				}
				if ( IsTrue(FS->flat) ) tstrcpy(dstPath, dstDIR);
				TinyDisplayProgress(FS);
				mainresult = DlgCopyDir(FS, srcPath, dstPath, attr);
				if ( mainresult == NO_ERROR ) FopLog(FS, srcDIR, dstDIR, LOG_DIR);
			}else{
				if ( !(FS->opt.fop.filter & VFSFOP_FILTER_NOFILEFILTER) ){
					if ( LFNfilter(&FS->opt, dstPath) != NO_ERROR ) break;
				}else{
				//名前変更モード && ファイル名変更無し なので処理をスキップ
					if ( IsTrue(FS->renamemode) ) continue;
				}
				if ( IsTrue(FS->testmode) ){
					mainresult = TestDest(FS, srcPath, dstPath);
				}else{
					mainresult = DlgCopyFile(FS, srcPath, dstPath, attr);
				}
			}
		}

		if ( FS->state == FOP_TOBREAK ) break;

		if ( mainresult != NO_ERROR ){
			if ( mainresult != ERROR_CANCELLED ){
				JobErrorBox(FS, STR_FOP, mainresult);
			}
			break;
		}
	}
	if ( FS->opt.fop.mode == FOPMODE_DELETE ){
		if ( !mainresult && FS->DelStat.noempty ){
			JobErrorBox(FS, STR_FOP, ERROR_DIR_NOT_EMPTY);
		}
	}

	if ( (srctype == VFSPT_FTP) || (dsttype == VFSPT_FTP) ){
		DInternetCloseHandle(hFTP[FTPHOST]);
		DInternetCloseHandle(hFTP[FTPINET]);
	}
	ReleaseStartMutex(FS);
	if ( FS->testmode == FALSE ){
		PPxPostMessage(WM_PPXCOMMAND, K_ENDCOPY, dstPath[0]);
		if ( (FS->opt.fop.mode == FOPMODE_MOVE) && (dstPath[0] != srcPath[0]) ){
			PPxPostMessage(WM_PPXCOMMAND, K_ENDCOPY, srcPath[0]);
		}
	}
	EndOperation(FS);
	return TRUE;
}

void EndingOperation(FOPSTRUCT *FS)
{
	SetFopLowPriority(NULL);
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_PREVENTSLEEP ){
		EndPreventSleep(FS->hDlg);
	}
	if ( FS->opt.rexps != NULL ){
		FreeRegularExpressionReplace(FS->opt.rexps);
		FS->opt.rexps = NULL;
	}
	if ( FS->opt.CopyBuf != NULL ){
		if ( VirtualFree(FS->opt.CopyBuf, 0, MEM_RELEASE) == FALSE ){
			FWriteErrorLogs(FS, NilStr, T("Free Buffer"), PPERROR_GETLASTERROR);
		}
		FS->opt.CopyBuf = NULL;
	}
}
