/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作, ファイルコピー
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#include "VFS_FOP.H"
#include "FATTIME.H"
#pragma hdrstop

#define RETRYWAIT 200
#ifndef COPY_FILE_NO_BUFFERING
#define COPY_FILE_NO_BUFFERING 0x1000 // Ver6.0
#endif

//#define SubDW(dstL, dstH, srcL) {DWORD I_; I_ = (dstL); (dstL) -= (srcL); if ( (dstL) > I_ ) (dstH)--;}
#define AddLIDW(dst, srcL) {(dst.u.LowPart) += (srcL); if ( (dst.u.LowPart) < (srcL)) (dst.u.HighPart)++;}
#define SubLIDW(dst, srcL) {DWORD I_; I_ = (dst.u.LowPart); (dst.u.LowPart) -= (srcL); if ( (dst.u.LowPart) > I_ ) (dst.u.HighPart)--;}

typedef enum { DONE_NO = 0, DONE_OK, DONE_DIV, DONE_SKIP } DONEENUM;

const TCHAR Str_DiskFill[] = MES_QSPW;
const TCHAR Str_LargeFile[] = MES_QSP2;
const TCHAR Str_Space[] = MES_QSPT;

ERRORCODE CheckEntryMask(FOPSTRUCT *FS, const TCHAR *src, DWORD srcattr)
{
	WIN32_FIND_DATA ff;
	const TCHAR *name;

	name = FindLastEntryPoint(src);
	if ( FS->maskFnFlags & (REGEXPF_REQ_ATTR | REGEXPF_REQ_SIZE | REGEXPF_REQ_TIME) ){
		if ( FS->maskFnFlags & (REGEXPF_REQ_SIZE | REGEXPF_REQ_TIME) ){
			HANDLE hFF;

			hFF = FindFirstFileL(src, &ff);
			if ( hFF == INVALID_HANDLE_VALUE ) return FOPERROR_GETLASTERROR;
			FindClose(hFF);
		}else{
			tstrcpy(ff.cFileName,name);
			ff.dwFileAttributes = srcattr;
		}
		if ( FinddataRegularExpression(&ff, &FS->maskFn) ) return ERROR_ALREADY_EXISTS;
	}else{
		if ( FilenameRegularExpression(name, &FS->maskFn) ) return ERROR_ALREADY_EXISTS;
	}
	FopLog(FS, src,NULL, LOG_SKIP);
	return NO_ERROR;
}

ERRORCODE DiskFillAction(FOPSTRUCT *FS, BY_HANDLE_FILE_INFORMATION *srcfinfo, TCHAR *dst)
{
	ULARGE_INTEGER UserFree, Total, Free;
	TCHAR path[VFPS];
	BOOL result;

	// 2G over の場合は、分割を検討する。
	if ( (srcfinfo->nFileSizeHigh != 0) ||
		 (srcfinfo->nFileSizeLow >= 0x80000000) ){
	#ifdef UNICODE
		VFSFullPath(path, T(".."), dst);
		result = GetDiskFreeSpaceEx(path, &UserFree, &Total, &Free);
	#else
		DefineWinAPI(BOOL, GetDiskFreeSpaceEx, (LPCTSTR lpDirectoryName,
			PULARGE_INTEGER lpFreeBytesAvailableToCaller,
			PULARGE_INTEGER lpTotalNumberOfBytes,
			PULARGE_INTEGER lpTotalNumberOfFreeBytes));

		GETDLLPROCT(hKernel32, GetDiskFreeSpaceEx);
		if ( DGetDiskFreeSpaceEx == NULL ){
			result = FALSE;
		}else{
			VFSFullPath(path, (OSver.dwPlatformId == VER_PLATFORM_WIN32_NT) ?
					T("\\") : T(".."), dst);
			result = DGetDiskFreeSpaceEx(path, &UserFree, &Total, &Free);
		}
	#endif
		// 空き容量から分割を判断。空きがあるのに失敗したときは2Gの恐れ有り
		if ( IsTrue(result) &&
			 ((UserFree.u.HighPart > srcfinfo->nFileSizeHigh) ||
			  ( (UserFree.u.HighPart == srcfinfo->nFileSizeHigh) &&
			   (UserFree.u.LowPart >= srcfinfo->nFileSizeLow) )) ){
			if ( IDYES != FopOperationMsgBox(FS, Str_LargeFile, STR_FOPWARN,
					MB_ICONEXCLAMATION | MB_YESNO) ){
				return ERROR_CANCELLED;
			}
			return ERROR_NOT_SUPPORTED; // 2G制限で行う
		}
	}
	if ( IDYES == FopOperationMsgBox(FS, Str_DiskFill, STR_FOPWARN,
			MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2) ){
		return NO_ERROR;
	}
	return ERROR_CANCELLED;
}

#pragma argsused
VOID CALLBACK AutoRetryWriteOpen(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	TCHAR path[VFPS];
	HANDLE hFile;

	UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);

	path[0] = '\0';
	GetWindowText(hWnd, path, VFPS);

	hFile = CreateFileL(path, GENERIC_READ | GENERIC_WRITE, 0,NULL,
			OPEN_ALWAYS, 0,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		CloseHandle(hFile);
		KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		PostMessage(hWnd, WM_COMMAND, IDRETRY, 0);
	}else{
		ERRORCODE error  = GetLastError();

		if ( error != ERROR_SHARING_VIOLATION ){
			// エラーの種類が変わったので再試行を中止
			KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		}
	}
}

int ErrorActionMsgBox(FOPSTRUCT *FS, ERRORCODE error, const TCHAR *dir, BOOL autoretrymode)
{
	TCHAR mes[CMDLINESIZE];
	int result;
	MESSAGEDATA md;

	DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
	if ( FS->opt.erroraction != 0 ){
		switch( FS->opt.erroraction ){
			case IDRETRY:
				FWriteErrorLogs(FS, dir, T("Retry"), error);
				Sleep(100);
				PeekMessageLoop(FS);
				if ( FS->state == FOP_TOBREAK ) return IDABORT;
				return IDRETRY;

			// IDABORT/IDIGNORE
			default:
				return FS->opt.erroraction;
		}
	}

	if ( error == 0 ) error = ERROR_ACCESS_DENIED; // CopyFile で起きることあり
	if ( error == ERROR_CANCELLED ) return IDABORT;
	if ( error == ERROR_REQUEST_ABORTED ) return IDABORT;

	mes[0] = '\0';
	if ( (error == ERROR_ACCESS_DENIED) ||
		 (error == ERROR_SHARING_VIOLATION) ){
		GetAccessApplications(dir, mes);
	}
	if ( mes[0] == '\0' ) PPErrorMsg(mes, error);

	md.title = dir;
	md.text = mes;
	md.style = MB_ICONEXCLAMATION | MB_ABORTRETRYIGNORE | MB_DEFBUTTON1;

	if ( autoretrymode && (error == ERROR_SHARING_VIOLATION) ){
		md.style = MB_ICONEXCLAMATION | MB_PPX_ABORTRETRYRENAMEIGNORE | MB_DEFBUTTON1 | MB_PPX_AUTORETRY;
		md.autoretryfunc = AutoRetryWriteOpen;
	}
	SetJobTask(FS->hDlg, JOBSTATE_ERROR);
	result = (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL), FS->hDlg, MessageBoxDxProc, (LPARAM)&md);
	SetJobTask(FS->hDlg, JOBSTATE_DEERROR);
	SetLastError(error);
	return result;
}

#pragma argsused
VOID CALLBACK AutoRetryOpen(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	TCHAR path[VFPS];
	HANDLE hFile;

	UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);

	path[0] = '\0';
	GetWindowText(hWnd, path, VFPS);

	// 非共有で開く
	hFile = CreateFileL(path, GENERIC_READ, 0,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		CloseHandle(hFile);
		KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		PostMessage(hWnd, WM_COMMAND, IDRETRY, 0);
		return;
	}
	{
		ERRORCODE error = GetLastError();

		if ( (error != ERROR_ACCESS_DENIED) &&
			 (error != ERROR_SHARING_VIOLATION)
		){ // エラーの種類が変わったので再試行を中止
			KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		}
	}
}

/*-----------------------------------------------------------------------------
	ファイルの移動を試みる
		ERROR_ALREADY_EXISTS:この方法では無理
		それ以外:正常終了／エラーコード
-----------------------------------------------------------------------------*/
ERRORCODE TryMoveFile(FOPSTRUCT *FS, const TCHAR *src, const TCHAR *dst, DWORD srcattr)
{
	ERRORCODE result;

	if ( IsTrue(MoveFileWithProgressL(src, dst,
			(LPPROGRESS_ROUTINE)CopyProgress, &FS->progs, 0)) ){
		FS->progs.info.donefiles++;
		if ( FS->progs.info.filesall && !(srcattr & FILE_ATTRIBUTE_DIRECTORY) ){
			WIN32_FIND_DATA ff;
			HANDLE hFF;

			hFF = FindFirstFileL(dst, &ff);
			if ( hFF != INVALID_HANDLE_VALUE ){
				FindClose(hFF);
				AddDD(FS->progs.info.donesize.l, FS->progs.info.donesize.h,
						ff.nFileSizeLow, ff.nFileSizeHigh);
			}
		}
		TinyDisplayProgress(FS);
		srcattr = (srcattr & (FS->opt.fop.AtrMask | 0xffffffd8)) |
				(FS->opt.fop.AtrFlag & 0x27);
		SetFileAttributesL(dst, srcattr);
		PeekMessageLoop(FS);
		return NO_ERROR;
	}
	result = GetLastError();
	if ( (srcattr & OPENERRORATTRIBUTES) &&
		(result == ERROR_ACCESS_DENIED) ){
		if ( IsTrue(SetFileAttributesL(src, FILE_ATTRIBUTE_NORMAL)) ){
			result = TryMoveFile(FS, src, dst, FILE_ATTRIBUTE_NORMAL);
			if ( result == NO_ERROR ){
				srcattr = (srcattr & (FS->opt.fop.AtrMask | 0xffffffd8))
						| (FS->opt.fop.AtrFlag & 0x27);
				SetFileAttributesL(dst, srcattr);
			}else{
				SetFileAttributesL(src, srcattr);
			}
			return result;
		}
	}

	if ( (result != ERROR_NOT_SAME_DEVICE) &&
		 (result != ERROR_ALREADY_EXISTS) &&
		 (result != ERROR_ACCESS_DENIED) ) return result;
	return ERROR_ALREADY_EXISTS;
}

BOOL USEFASTCALL FixIOSize(DWORD *iostep)
{
	if ( *iostep <= 32 * 1024 ) return FALSE;
	if ( *iostep <= 64 * 1024 ){
		*iostep = 32 * 1024;
	}else if ( *iostep <= 256 * 1024 ){
		*iostep = 64 * 1024;
	}else if ( *iostep <= 512 * 1024 ){
		*iostep = 256 * 1024;
	}else{
		*iostep = 512 * 1024;
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	file → file へのコピールーチン
	src, dst:	コピー元/先（fullpath）
-----------------------------------------------------------------------------*/
ERRORCODE DlgCopyFile(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst, DWORD srcattr)
{
	HWND hDlg;
	BY_HANDLE_FILE_INFORMATION srcfinfo, dstfinfo;	// ファイル情報
	HANDLE srcH, dstH;	// ファイルハンドル
															// コピー用バッファ
	BYTE filebuffer[filebuffersize * 2], *filebuf, *filebufp;
	DWORD readsize, writesize;					// バッファを読み書きした量
	DWORD buffersize;							// 読み書き大きさ
	LARGE_INTEGER TotalTransSize, TotalSize;

	struct {
		ULARGE_INTEGER divsize; // 分割サイズ
		ULARGE_INTEGER leftsize; // 残サイズ
		BOOL use; // true...分割有り
		DWORD count;	// 分割番号
	} div;

	TCHAR *dsttail;
	DWORD dstatr;								// 書き込み時の属性

	DWORD oldtick;			// 書き込み間隔調整用tickカウンタ
	DWORD readstep, writestep;	// １度に読み書きする大きさ(ドライブ占有防止用)

	int dst1st = 1;	// (分割時の)最初のファイル書き込みなら !0
	ERRORCODE error = NO_ERROR;	// エラー発生時は NO_ERROR 以外
	int errorretrycount; // 失敗したときにやり直す回数
	DONEENUM done = DONE_NO;	// 作業が終了したら !DONE_NO
	int append = 0;	// 追加なら !0
	int deldst = 1;	// コピー先を削除する必要があるなら !0
	int fixdstatr= 0;	// コピー先属性を修復する必要があるなら !0
	DWORD filemode;
	int avtry;

	struct FopOption *opt;

	hDlg = FS->hDlg;
	opt = &FS->opt;

	buffersize = filebuffersize;
	errorretrycount = FS->opt.errorretrycount;
	filemode = FS->opt.burst ? FILE_FLAG_NO_BUFFERING : 0;
	PeekMessageLoopSub(FS);
	if ( FS->state != FOP_BUSY ){
		PeekMessageLoop(FS);
		if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;
	}

	if ( !(FS->maskFnFlags & REGEXPF_BLANK) ){
		ERRORCODE erc;

		erc = CheckEntryMask(FS, src, srcattr);
		if ( erc != ERROR_ALREADY_EXISTS ) return erc;
	}

	if ( opt->fop.mode == FOPMODE_DELETE ){
		ERRORCODE result;

		if ( opt->fop.flags & (VFSFOP_OPTFLAG_BACKUPOLD | VFSFOP_OPTFLAG_UNDOLOG) ){
			result = BackupFile(FS, src);
		}else{
			result = VFSDeleteEntry(&FS->DelStat, src, srcattr);
		}
		if ( result == NO_ERROR ){
			FopLog(FS, src,NULL, LOG_DELETE);
		}else{
			BOOL OldNoAutoClose = FS->NoAutoClose;

			FWriteErrorLogs(FS, src, T("Delete"),result);
			if ( FS->DelStat.useaction == 0 ) FS->NoAutoClose = OldNoAutoClose;
		}
		return result;
	}else if ( opt->fop.mode == FOPMODE_SHORTCUT ){
		TCHAR *p;

		p = VFSFindLastEntry(dst);
		p += FindExtSeparator(p);
		tstrcpy(p, StrShortcutExt);
	}

	if ( FS->ifo != NULL ){
		CopyFileWithIfo(FS, src, dst);
		return NO_ERROR;
	}

	// 移動を試みる -----------------------------------------------------------
	while ( CheckSaveDrive(opt, src, dst) && (opt->fop.divide_num == 0) ){
		ERRORCODE tryresult;

		if ( IsTrue(FS->renamemode) &&
			 (opt->fop.same != FOPSAME_ADDNUMBER) &&
			 (tstrcmp(VFSFindLastEntry(src), VFSFindLastEntry(dst)) == 0) ){
			if ( GetFileAttributesL(dst) != BADATTR ) break;
		}
		tryresult = TryMoveFile(FS, src, dst, srcattr);
		if ( (tryresult == ERROR_SHARING_VIOLATION) &&
			 !(opt->fop.flags & VFSFOP_OPTFLAG_WAIT_CLOSE) ){
			MESSAGEDATA md;
			int msgresult;

			GetAccessApplications(src, (TCHAR *)filebuffer);
			md.title = src;
			md.text = (TCHAR *)filebuffer;
			md.style = MB_RETRYCANCEL | MB_DEFBUTTON1 | MB_ICONEXCLAMATION | MB_PPX_AUTORETRY;
			md.autoretryfunc = AutoRetryOpen;

			SetJobTask(hDlg, JOBSTATE_ERROR);
			msgresult = (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL),
					hDlg, MessageBoxDxProc, (LPARAM)&md);
			SetJobTask(hDlg, JOBSTATE_DEERROR);
			if ( msgresult == IDRETRY ) continue;
		}

		if ( ((tryresult == ERROR_ACCESS_DENIED)	||
			  (tryresult == ERROR_FILE_NOT_FOUND)	||
			  (tryresult == ERROR_SHARING_VIOLATION )  ||
			  (tryresult == ERROR_LOCK_VIOLATION )  ||
			  (tryresult == ERROR_CANT_ACCESS_FILE )  || //アクセス権以外の理由で開けない
			  ((tryresult == ERROR_INVALID_NAME) && tstrchr(src, '?')) ) &&
			  (opt->fop.flags & VFSFOP_OPTFLAG_SKIPERROR) ){
			FS->progs.info.LEskips++;
			FWriteErrorLogs(FS, src, T("Move"), tryresult);
			FopLog(FS, src, dst, LOG_SKIP);
			return NO_ERROR;
		}
		if ( tryresult == ERROR_ALREADY_EXISTS ) break;
		if ( tryresult == NO_ERROR ) FopLog(FS, src, dst, LOG_MOVE);
		return tryresult;
	}

	if ( (srcattr & FILE_ATTRIBUTE_REPARSE_POINT) &&
		 (FS->opt.fop.mode != FOPMODE_DELETE) ){
		ERRORCODE result;

		result = FileOperationReparse(FS, src, dst, srcattr);
		if ( result != ERROR_MORE_DATA ) return result;
	}
										// コピー元を開く ---------------------
	avtry = FS->opt.errorretrycount;
	while ( (srcH = CreateFile_OpenSource(src, filemode)) == INVALID_HANDLE_VALUE ){
		int result;
		BOOL OldNoAutoClose;

		OldNoAutoClose = FS->NoAutoClose;
		error = GetLastError();
		if ( (error == ERROR_SHARING_VIOLATION) ||
			 (error == ERROR_LOCK_VIOLATION) ){
			if ( avtry ){
				avtry--;
				Sleep(RETRYWAIT);
				continue;
			}
		}

		if ( ((error == ERROR_ACCESS_DENIED)	||
			  (error == ERROR_FILE_NOT_FOUND)	|| //dir/driv?はあるがファイル無
			  (error == ERROR_PATH_NOT_FOUND)	|| //dir/drive?もファイルも無
			  (error == ERROR_SHARING_VIOLATION )  ||
			  (error == ERROR_LOCK_VIOLATION )  ||
			  (error == ERROR_CANT_ACCESS_FILE )  || //アクセス権以外の理由で開けない
			  ((error == ERROR_INVALID_NAME) && tstrchr(src, '?')) ) &&
			  (opt->fop.flags & VFSFOP_OPTFLAG_SKIPERROR) ){
			FS->progs.info.LEskips++;
			FWriteErrorLogs(FS, src, T("Src open"), error);
			return NO_ERROR;
		}

		if ( (error != ERROR_CANCELLED) &&
			 (error != ERROR_REQUEST_ABORTED) &&
			 (errorretrycount != 0) ){
			errorretrycount--;
			FWriteErrorLogs(FS, src, T("Src open AutoRetry"), error);
			Sleep(RETRYWAIT);
			FS->NoAutoClose = OldNoAutoClose;
			continue;
		}
		result = ErrorActionMsgBox(FS, error, src, FALSE);
		if ( result == IDIGNORE ){
			FWriteErrorLogs(FS, src, T("Src open"), error);
			if ( FS->opt.erroraction == 0 ) FS->NoAutoClose = OldNoAutoClose;
			return NO_ERROR;
		}
		if ( FS->opt.erroraction == 0 ) FS->NoAutoClose = OldNoAutoClose;
		if ( result == IDRETRY ) continue;
		return ERROR_CANCELLED; // 既にエラー報告したのでキャンセル扱い
	}

	srcfinfo.dwFileAttributes = BADATTR;
	srcfinfo.nFileSizeHigh = MAX32;

	if ( GetFileInformationByHandle(srcH, &srcfinfo) == FALSE ){
		error = FOPERROR_GETLASTERROR;
	}
/*------------------------------------------
NEC PC98x1用Windows95～機種不問Win9x間でネットワーク経由
で参照すると属性が正しく得られないための処置
------------------------------------------*/
	if ( srcfinfo.dwFileAttributes != srcattr ){
		HANDLE hFF;
		WIN32_FIND_DATA ff;

		hFF = FindFirstFileL(src, &ff);
		if ( hFF == INVALID_HANDLE_VALUE ){
			error = FOPERROR_GETLASTERROR;
		}else{
			FindClose(hFF);
			srcfinfo.dwFileAttributes = ff.dwFileAttributes;
			srcfinfo.nFileSizeHigh = ff.nFileSizeHigh;
			srcfinfo.nFileSizeLow = ff.nFileSizeLow;
		}
	}

// CreateFile (open) →見つけた → CreateFile (create) で再作成
	avtry = FS->opt.errorretrycount;
	while( error == NO_ERROR ){
									// 同名ファイルがあるか確認する
		// ※ これで得たハンドルを再利用しても、属性、拡張属性を変更できない
		//    ため、再利用せずに廃棄する。
		dstH = CreateFileL(dst, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ,
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | filemode,NULL);

		// 存在するけど開けなかった
		if ( dstH == INVALID_HANDLE_VALUE ){
			ERRORCODE dsterror;

			dsterror = GetLastError();

			if ( (dsterror == ERROR_SHARING_VIOLATION) ||
				 (dsterror == ERROR_LOCK_VIOLATION) ){
				if ( GetFileAttributesL(dst) != BADATTR ){
					if ( avtry  ){
						avtry--;
						Sleep(RETRYWAIT);
						continue;
					}
					dstH = NULL;
				}
			}
		}

		if ( dstH != INVALID_HANDLE_VALUE ){	// 発見
			ERRORCODE result;

			result = SameNameAction(FS, dstH, &srcfinfo, &dstfinfo, src, dst);
			switch( result ){
				case ACTION_RETRY:
					if ( CheckSaveDrive(opt, src, dst) && (opt->fop.divide_num == 0) ){
						if ( srcH != INVALID_HANDLE_VALUE ){
							CloseHandle(srcH);
							srcH = INVALID_HANDLE_VALUE;
						}
						result = TryMoveFile(FS, src, dst, srcattr);
						if ( (result != ERROR_ALREADY_EXISTS) &&
							 (result != ERROR_INVALID_NAME) ){
							if ( result == NO_ERROR ){
								FopLog(FS, src, dst, LOG_MOVE);
							}
							return result;
						}
					}
					continue;

				case ACTION_APPEND:
					append = 1;
					// ACTION_OVERWRITE へ
				case ACTION_OVERWRITE:
					fixdstatr = 1;
					break;

				case ACTION_CREATE:
					break;

				default: // ACTION_SKIP 又は error
					if ( srcH != INVALID_HANDLE_VALUE ){
						CloseHandle(srcH);
					}
					if ( result == ACTION_SKIP ){
						FS->progs.info.EXskips++;
						FopLog(FS, src, dst, LOG_SKIP);
						if ( FS->progs.info.filesall ){
							SubDD(FS->progs.info.allsize.l, FS->progs.info.allsize.h,
								srcfinfo.nFileSizeLow, srcfinfo.nFileSizeHigh);
						}
						result = NO_ERROR;
					}
					return result;
			}
			if ( CheckSaveDrive(opt, src, dst) && (opt->fop.divide_num == 0) ){
				if ( srcH != INVALID_HANDLE_VALUE ){
					CloseHandle(srcH);
					srcH = INVALID_HANDLE_VALUE;
				}
				if ( dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES ){
					SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
				}
				DeleteFileL(dst);
				result = TryMoveFile(FS, src, dst, srcattr);
				if ( result != ERROR_ALREADY_EXISTS ){
					if ( result == NO_ERROR ){
						FopLog(FS, src, dst, LOG_MOVEOVERWRITE);
					}
					return result;
				}
			}

			if ( srcH == INVALID_HANDLE_VALUE ){	// 再オープン
				srcH = CreateFile_OpenSource(src, filemode);
			}
		}
		break;
	}

	div.count = 0;
	if ( opt->fop.divide_num == 0 ){ // 分割しない
		div.use = FALSE;
	}else{
		const TCHAR *ptr;

		wsprintf((TCHAR *)filebuffer, T("%u%c"), opt->fop.divide_num, opt->fop.divide_unit);
		ptr = (const TCHAR *)filebuffer;
		GetSizeNumber( &ptr, &div.divsize.u.LowPart, &div.divsize.u.HighPart);
		div.use = TRUE;
		div.leftsize = div.divsize;
		if ( (srcfinfo.nFileSizeHigh > div.leftsize.u.HighPart) ||
			 ((srcfinfo.nFileSizeHigh == div.leftsize.u.HighPart) &&
			  (srcfinfo.nFileSizeLow > div.leftsize.u.LowPart)) ){
			div.count++;
		}
	}
	readsize = 0;
	dsttail = dst + tstrlen(dst);
										// コピー先を開く ---------------------
	dstatr = (srcfinfo.dwFileAttributes
			 & (opt->fop.AtrMask | 0xffffffd8)) | (opt->fop.AtrFlag & 0x27);

	if ( (error == NO_ERROR) && (opt->fop.mode > FOPMODE_MIRROR) ){
		if ( opt->fop.mode == FOPMODE_SHORTCUT ){
			error = FOPMakeShortCut(src, dst, FALSE, FALSE);
		}else if ( FS->opt.fop.mode == FOPMODE_LINK ){
			if ( DCreateHardLink(dst, src,NULL) == FALSE ){
				error = GetLastError();
			}
		}else if ( FS->opt.fop.mode == FOPMODE_SYMLINK ){
			error = FopCreateSymlink(dst, src, 0);
		}else{
			error = ERROR_INVALID_FUNCTION;
		}
		done = DONE_OK;
	}else
	#ifndef UNICODE
	if ( WinType != WINTYPE_9x )
	#endif
	{
		while ( (error == NO_ERROR) && !div.count && !append && !FS->opt.burst){
			DWORD flags = 0;

			FS->Cancel = FALSE;
			if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_ALLOWDECRYPT ){
				setflag(flags, COPY_FILE_ALLOW_DECRYPTED_DESTINATION);
			}
			if ( (WinType >= WINTYPE_VISTA) && ((srcfinfo.nFileSizeHigh != 0) ||
				  (srcfinfo.nFileSizeLow >= (64 * MB)) ) ){
				setflag(flags, COPY_FILE_NO_BUFFERING);
			}

			if ( IsTrue(CopyFileExL(src, dst, (LPPROGRESS_ROUTINE)CopyProgress,
					&FS->progs, &FS->Cancel, flags)) ){
				SetFileAttributesL(dst, dstatr);
				if ( opt->security != SECURITY_FLAG_NONE ){
					error = CopySecurity(FS, src, dst);
				}
				done = DONE_OK;
				break;
			}else{
				int result;
				BOOL OldNoAutoClose = FS->NoAutoClose;

				error = GetLastError();
				if ( error == NO_ERROR ) error = ERROR_ACCESS_DENIED;
				if ( error == ERROR_ACCESS_DENIED ){
					if ( IsTrue(FS->Cancel) ){
						error = ERROR_CANCELLED;
					}else{
						DWORD attr;


						attr = GetFileAttributesL(dst);
								// ディレクトリに対してコピーしようとした
						if ( (attr != BADATTR) && (attr & FILE_ATTRIBUTE_DIRECTORY) ){
							if( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
								FS->progs.info.LEskips++;
								FWriteErrorLogs(FS, src, T("Dest"), ERROR_DIRECTORY);
								error = NO_ERROR;
								done = DONE_SKIP;
								break;
							}
						}
					}
				}else if ( (error == ERROR_DISK_FULL) ||
						((error == ERROR_INVALID_PARAMETER) && (srcfinfo.nFileSizeHigh || (srcfinfo.nFileSizeLow >= 0x80000000))) ){
					error = DiskFillAction(FS, &srcfinfo, dst); //分割確認
					if ( error != ERROR_CANCELLED ){ // 分割指示有り
						if ( error == ERROR_NOT_SUPPORTED ){ // 2G 制限
							div.divsize.u.LowPart = 0x7fff8000; // 2G - 32k
							div.divsize.u.HighPart = 0;
						}
						if ( div.divsize.u.LowPart | div.divsize.u.HighPart ){
							div.use = TRUE;
							div.leftsize = div.divsize;
							if ( (srcfinfo.nFileSizeHigh > div.leftsize.u.HighPart) ||
								 ((srcfinfo.nFileSizeHigh == div.leftsize.u.HighPart) &&
								  (srcfinfo.nFileSizeLow > div.leftsize.u.LowPart)) ){
								div.count++;
							}
						}
						error = NO_ERROR;
					}
					break;
				}

				if ( (error == ERROR_BAD_NETPATH) &&
					 ( ((src[0] == '\\') && (src[1] == '\\') && (src[2] == '.')) ||
					   ((dst[0] == '\\') && (dst[1] == '\\') && (dst[2] == '.'))) ){
					// コピー元・先が \\.\Volume～ や \\.\Harddisk0Partition1
					// の場合、CopyFileEx ではエラーになるので自前コピーをする
					error = NO_ERROR;
					break;
				}

				if ( (error != ERROR_CANCELLED) && (error != ERROR_REQUEST_ABORTED) && errorretrycount && (FS->Cancel == FALSE) ){
					errorretrycount--;
					FWriteErrorLogs(FS, dst, T("Dest AutoRetry"), error);
					Sleep(RETRYWAIT);
					error = NO_ERROR;
					FS->NoAutoClose = OldNoAutoClose;
					continue;
				}
				result = ErrorActionMsgBox(FS, error, dst, TRUE);
				if ( FS->opt.erroraction == 0 ){
					FS->NoAutoClose = OldNoAutoClose;
				}
				if ( result == IDRETRY ){
					error = NO_ERROR;
					continue;
				}else if ( result == IDX_FOP_ADDNUMDEST ){
					error = RenameDestFile(FS, dst, TRUE);
					if ( error != NO_ERROR ){
						PPErrorBox(FS->hDlg, dst, error);
						error = NO_ERROR;
					}
					continue;
				}else if ( result == IDIGNORE ){
					// アクセスが拒否された場合→コピーそのものをしない
					if ( (error == ERROR_ACCESS_DENIED) ||
						 (error == ERROR_SHARING_VIOLATION ) ||
						 (error == ERROR_LOCK_VIOLATION ) ){
						FS->progs.info.errors++;
						FWriteErrorLogs(FS, dst, T("Error Skip"), error);
						done = DONE_SKIP;
					}
				}else{
					error = ERROR_CANCELLED;
					break;
				}
				buffersize = 512;
//		FS->opt.burst = 0; バーストモードにしてシステム介入をさけた方がいいかも。
				error = NO_ERROR;	// 強制コピー処理へ
				break;
			}
		}
	}
//=========================================================================
	while ( (error == NO_ERROR) && (done == DONE_NO) ){
		if ( buffersize != 512 ){
			if ( FS->opt.CopyBufSize && (FS->opt.CopyBuf == NULL) ){
				FS->opt.CopyBuf = VirtualAlloc(NULL,
						FS->opt.CopyBufSize, MEM_COMMIT, PAGE_READWRITE);
				if (FS->opt.CopyBuf == NULL){
					FWriteErrorLogs(FS,NilStr, T("Alloc Buffer"), PPERROR_GETLASTERROR);
				}
			}
			if ( FS->opt.CopyBuf != NULL ){
				filebuf = FS->opt.CopyBuf;
				buffersize = FS->opt.CopyBufSize;
				break;
			}
			FS->opt.CopyBufSize = 0;
		}
//		FS->opt.burst = 0;
		filebuf = filebuffer +
				(buffersize - (ALIGNMENT_BITS(filebuffer) & (buffersize - 1)) );
		break;
	}
	oldtick = GetTickCount();
	readstep = writestep = min(32 * KB, buffersize);
	if ( append ) filemode = 0;	// Append時はバーストモード使用不可

	avtry = FS->opt.errorretrycount;
	while ( (error == NO_ERROR) && (done == DONE_NO) ){
		ULARGE_INTEGER CopyLeft; // 残サイズ

		if ( div.count ) wsprintf(dsttail, T(".%03d"), div.count - 1);
		dstH = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0,NULL,
				dst1st ? (append ? OPEN_ALWAYS : CREATE_ALWAYS) : CREATE_NEW,
				dstatr | FILE_FLAG_SEQUENTIAL_SCAN | filemode,
				(
	#ifndef UNICODE
					(WinType != WINTYPE_9x) &&
	#endif
					dst1st && !append) ? srcH : NULL);
		if ( dstH == INVALID_HANDLE_VALUE ){
			deldst = 0;
			dst1st = 0;
			error = GetLastError();
			if ( (error == ERROR_SHARING_VIOLATION) ||
				 (error == ERROR_LOCK_VIOLATION) ){
				if ( avtry ){
					avtry--;
					Sleep(RETRYWAIT);
					continue;
				}
			}
			break;
		}
		if ( dst1st ){ // ファイルコピー開始時の処理(分割2ファイル目は実行無し)
			ULARGE_INTEGER nowpos;
			ULARGE_INTEGER curpos;

			CopyLeft.u.LowPart = TotalSize.u.LowPart = srcfinfo.nFileSizeLow;
			CopyLeft.u.HighPart = TotalSize.u.HighPart = srcfinfo.nFileSizeHigh;
			TotalTransSize.u.LowPart = TotalTransSize.u.HighPart = 0;

			if ( append ){			// 追加なら末尾へ
				LONG sizetmp;

				sizetmp = 0;
				SetFilePointer(dstH, 0, &sizetmp, FILE_END);
			}
									// 現在位置を取得
			nowpos.u.HighPart = 0;
			nowpos.u.LowPart = SetFilePointer(dstH, 0, (PLONG)&nowpos.u.HighPart, FILE_CURRENT);
									// ファイルサイズをOSに通知
			if ( div.use == FALSE ){
				curpos.u.HighPart = srcfinfo.nFileSizeHigh;
				curpos.u.LowPart = srcfinfo.nFileSizeLow;
				if ( FS->opt.burst ){
					AddDD(curpos.u.LowPart, curpos.u.HighPart, SECTORSIZE - 1, 0);
					curpos.u.LowPart &= ~(SECTORSIZE - 1);
				}
				curpos.u.LowPart = SetFilePointer(dstH, curpos.u.LowPart, (PLONG)&curpos.u.HighPart, FILE_CURRENT);
			}else{
				curpos.u.HighPart = div.leftsize.u.HighPart;
				curpos.u.LowPart = SetFilePointer(dstH, div.leftsize.u.LowPart, (PLONG)&curpos.u.HighPart, FILE_CURRENT);
			}
			if ( (curpos.u.LowPart != MAX32) || (GetLastError() == NO_ERROR) ){
				if ( SetEndOfFile(dstH) == FALSE ){
					error = GetLastError();
					if ( (error == ERROR_DISK_FULL) ||
						((error == ERROR_INVALID_PARAMETER) && (srcfinfo.nFileSizeHigh || (srcfinfo.nFileSizeLow >= 0x80000000))) ){
						error = DiskFillAction(FS, &srcfinfo, dst); //分割確認
						if ( error == ERROR_NOT_SUPPORTED ){ // 2G 制限
							div.divsize.u.LowPart = 0x7fff8000; // 2G - 32k
							div.divsize.u.HighPart = 0;
							div.leftsize = div.divsize;
							div.use = TRUE;
							error = NO_ERROR;
						}
					}
				}
				SetFilePointer(dstH,nowpos.u.LowPart, (PLONG)&nowpos.u.HighPart, FILE_BEGIN);
			}
		}
		dst1st = 0;
		if ( div.count && div.use ){
			CopyLeft = div.leftsize;
		}

		FullDisplayProgress(&FS->progs, TotalTransSize, TotalSize);
										// コピー処理 -------------------------
		while( error == NO_ERROR ){
			errorretrycount = FS->opt.errorretrycount;
											// 読み込み
			if ( readsize == 0 ){
				// バーストモード時、最後の読み込みは読込サイズを控えめにする
				if ( FS->opt.burst &&
					 (CopyLeft.u.HighPart == 0) &&
					 (CopyLeft.u.LowPart < buffersize) ){
					buffersize =
						(CopyLeft.u.LowPart + (SECTORSIZE - 1)) & ~(SECTORSIZE - 1);
					if ( buffersize == 0 ) buffersize = SECTORSIZE;
				}
				filebufp = filebuf;
				while( readsize < buffersize ){
					DWORD size, tick,rsize;

					rsize = buffersize - readsize;
					rsize = min(readstep,rsize);
					if ( ReadFile(srcH, filebufp,rsize, &size,NULL) ){
						readsize += size;
						if ( size < rsize ) break;
						filebufp += size;
						tick = GetTickCount();
						size = max(tick - oldtick, 1);
						oldtick = tick;
						// 読み込み大きさの調整
						if ( (size < 60 * 1000) && (rsize == readstep)){
							readstep = (((readstep / 0x8000) * 100) / size + 1 )* 0x8000;
						}
						FullDisplayProgress(&FS->progs, TotalTransSize, TotalSize);
						PeekMessageLoop(FS);
						if ( FS->state == FOP_TOBREAK ){
							error = ERROR_CANCELLED;
							break;
						}
					}else{
						int result;
						DWORD tmp;
						ERRORCODE errcode = GetLastError();
						BOOL OldNoAutoClose = FS->NoAutoClose;

						if ( (errcode == ERROR_NO_SYSTEM_RESOURCES) &&
							 IsTrue(FixIOSize(&readstep)) ){
							continue;
						}
						if ( (errcode != ERROR_CANCELLED) &&
							 (error != ERROR_REQUEST_ABORTED) &&
							 errorretrycount ){
							errorretrycount--;
							FWriteErrorLogs(FS, src, T("Src AutoRetry"), errcode);
							Sleep(RETRYWAIT);
							error = NO_ERROR;
							FS->NoAutoClose = OldNoAutoClose;
							continue;
						}

						result = ErrorActionMsgBox(FS, errcode, src, FALSE);
						if ( FS->opt.erroraction == 0 ){
							FS->NoAutoClose = OldNoAutoClose;
						}
						if ( result == IDRETRY ){
							error = NO_ERROR;
						}
						if ( result != IDIGNORE ){
							error = ERROR_CANCELLED;
							break;
						}
						// ignore 時は低速でも読める確率を上げるために細切れに
						// 読む & 512バイトスキップする
						FS->opt.erroraction = IDIGNORE;
						readstep = buffersize = 512;
						memset(filebuf, 0x55,readstep);
						tmp = 0;
						SetFilePointer(srcH,readstep, (PLONG)&tmp, FILE_CURRENT);
					}
				}
				if ( error != NO_ERROR ) break;
				filebufp = filebuf;
				SubLIDW(CopyLeft,readsize);
			}
			if ( readsize == 0 ){
				done = DONE_OK;
				break;
			}
			// バーストモード時の、最終書き込みのための処理
			// ※クラスタ単位で書き込んだ後、サイズ調整を行う
			if ( (readsize != buffersize) && FS->opt.burst ){
				DWORD tmp;

				if ( WriteFile(dstH, filebufp, buffersize, &writesize,NULL)
						== FALSE){
					error = FOPERROR_GETLASTERROR;	// 書き込み失敗
					break;
				}
				CloseHandle(dstH);
				AddLIDW(TotalTransSize,readsize);

				if ( dstatr & FILE_ATTRIBUTE_READONLY ){
					SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
				}
//				avtry = FS->opt.errorretrycount;
				dstH = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0,NULL,
						OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS,NULL);
				if ( dstH == INVALID_HANDLE_VALUE ){
					error = GetLastError();
				}else{ // サイズ縮小
					tmp = MAX32;
					SetFilePointer(dstH,readsize - buffersize,
							(PLONG)&tmp, FILE_END);
					if ( !SetEndOfFile(dstH) ){
						error = GetLastError();
					}
				}
				done = DONE_OK;
				break;
			}
			while ( readsize != 0 ){
				ERRORCODE err;
				DWORD tick, size, wsize;

				wsize = min(writestep,readsize);
				if ( div.use && (div.leftsize.u.HighPart == 0) && (wsize > div.leftsize.u.LowPart) ){
					wsize = div.leftsize.u.LowPart;
				}
				if ( IsTrue(WriteFile(dstH, filebufp, wsize, &writesize,NULL)) ){
					AddLIDW(TotalTransSize, writesize);
									// writesize が正しく取得できない場合の対策
					if ( writesize != wsize ){
						err = ERROR_DISK_FULL;
					}else{
						readsize -= writesize;
						filebufp += writesize;
						if ( div.use ){
							if ( div.leftsize.u.HighPart > 0 ){
								SubLIDW(div.leftsize, writesize);
							}else{
								div.leftsize.u.LowPart -= writesize;
								if ( div.leftsize.u.LowPart == 0 ){
									div.leftsize = div.divsize;
									error = FOPERROR_DIV;
									break;
								}
							}
						}

						tick = GetTickCount();
						size = max(tick - oldtick, 10);
						oldtick = tick;
						// 読み込み大きさの調整
						if ( (size < 60 * 1000) && (wsize == writestep) ){
							writestep = (((writestep / 0x8000) * 100) / size + 1 ) * 0x8000;
						}
						FullDisplayProgress(&FS->progs, TotalTransSize, TotalSize);
						PeekMessageLoop(FS);
						if ( FS->state == FOP_TOBREAK ){
							error = ERROR_CANCELLED;
							break;
						}
						continue;
					}
				}else{
					err = GetLastError();	// 書き込み失敗
					if ( (err == ERROR_NO_SYSTEM_RESOURCES) &&
						IsTrue(FixIOSize(&writestep)) ){
						continue;
					}
				}
				if ( (err != ERROR_DISK_FULL) &&
					 (err != ERROR_CANCELLED) &&
					 (error != ERROR_REQUEST_ABORTED) ){
					if ( errorretrycount ){
						errorretrycount--;
						FWriteErrorLogs(FS, dst, T("Dest AutoRetry"), PPERROR_GETLASTERROR);
						Sleep(RETRYWAIT);
						continue;
					}

					switch ( ErrorActionMsgBox(FS, err, dst, FALSE) ){
						case IDRETRY:
						case IDIGNORE:
							continue;
//						default: // 省略
					}
					error = ERROR_CANCELLED;
					break;
				}else{
					if ( IDOK == FopOperationMsgBox(FS, Str_Space, STR_FOPWARN,
							MB_ICONEXCLAMATION | MB_OKCANCEL) ){
						done = DONE_DIV;
						error = FOPERROR_DIV;
					}else{
						error = FOPERROR_GETLASTERROR;
					}
				}
				SetLastError(err);
				break;
			}
		}
												// 終了処理
		if ( error == FOPERROR_DIV ){
			error = NO_ERROR;
			div.count++;
		}
		if ( error == FOPERROR_GETLASTERROR ) error = GetLastError();
//FlushFileBuffers
		if ( dstH != INVALID_HANDLE_VALUE ){
			if ( error != NO_ERROR ){
				if ( deldst ){		// ファイルの大きさを 0 にする
					DWORD tmp = 0;

					SetFilePointer(dstH, 0, (PLONG)&tmp, FILE_BEGIN);
					SetEndOfFile(dstH);
				}
				CloseHandle(dstH);

				if ( deldst ){
					if ( dstatr & FILE_ATTRIBUTE_READONLY ){
						SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
					}
					DeleteFileL(dst);
				}
			}else{
				WIN32_FIND_DATA ff;
				HANDLE hFF;

				SetFileTime(dstH,	NULL, //&srcfinfo.ftCreationTime,
									&srcfinfo.ftLastAccessTime,
									&srcfinfo.ftLastWriteTime);
				CloseHandle(dstH);
				hFF = FindFirstFileL(dst, &ff);	// 正しくコピーできたか確認する
				if ( hFF == INVALID_HANDLE_VALUE ){
					error = ERROR_INVALID_DATA;
				}else{
					FindClose(hFF);

					if ( div.count ){
						// 最後の 0byte ファイルを削除
						if ( (ff.nFileSizeLow == 0) && (ff.nFileSizeHigh == 0) ){
							DeleteFileL(dst);
							div.count--;
						}
					}else if ( !append &&
							((srcfinfo.nFileSizeLow != ff.nFileSizeLow) ||
							 (srcfinfo.nFileSizeHigh != ff.nFileSizeHigh))){
						error = ERROR_INVALID_DATA;
						DeleteFileL(dst);
					}

					if ( ff.dwFileAttributes != dstatr ){
						SetFileAttributesL(dst, dstatr);
					}
					if ( opt->security != SECURITY_FLAG_NONE ){
						error = CopySecurity(FS, src, dst);
					}
					if ( done == DONE_DIV ){
						if ( div.count == 1 ){
							TCHAR buf[VFPS];

							tstrcpy(buf, dst);
							tstrcpy(dsttail, T(".000"));
							div.count++;
							MoveFileL(buf, dst);
						}
						done = DONE_NO;
						XMessage(hDlg, STR_FOP, XM_INFOld, MES_NEWD);
					}
				}
			}
		}else if ( fixdstatr && (error != NO_ERROR) &&
				(dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES) ){
			 SetFileAttributesL(dst, dstfinfo.dwFileAttributes);
		}
	}
	if ( FS->state == FOP_TOBREAK ) error = ERROR_CANCELLED;
	if ( srcH != INVALID_HANDLE_VALUE ){
		CloseHandle(srcH);

		if ( error == NO_ERROR ){
												// 連結バッチファイルの生成
			if( div.count && (FS->opt.fop.flags & VFSFOP_OPTFLAG_JOINBATCH) ){
				HANDLE hBFile;
				DWORD divi;
				TCHAR *dstptr;

				tstrcpy(dsttail, T(".bat"));
				dstptr = VFSFindLastEntry(dst);
				if ( *dstptr == '\\' ) dstptr++;
				hBFile = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0,NULL,
						CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN,NULL);
				WriteFile(hBFile, "@copy /b \"", 10, &writesize,NULL);
				for ( divi = 0 ; ; ){
					wsprintf(dsttail, T(".%03d"), divi);
					WriteFileZT(hBFile, dstptr, &writesize);
					if ( ++divi == div.count ){
						WriteFile(hBFile, "\" \"", 3, &writesize,NULL);
						*dsttail = '\0';
						WriteFileZT(hBFile, dstptr, &writesize);
						WriteFile(hBFile, "\"\r\n", 3, &writesize,NULL);
						break;
					}else{
						WriteFile(hBFile, "\" + \"", 5, &writesize,NULL);
					}
				}
				CloseHandle(hBFile);
			}
												// コピー処理で"ArcOnly"の処理
			if ( (opt->fop.mode != FOPMODE_MOVE) &&
				(opt->fop.same == FOPSAME_ARCHIVE) &&
				(srcfinfo.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ){
				resetflag(srcfinfo.dwFileAttributes, FILE_ATTRIBUTE_ARCHIVE);
				SetFileAttributesL(src, srcfinfo.dwFileAttributes);
			}

			if ( done != DONE_NO ){
				if ( append ){
					FopLog(FS, src, dst, LOG_APPEND);
				}else if (opt->fop.mode == FOPMODE_MOVE){ // 移動→元を削除
					if ( done != DONE_SKIP ){
						if (srcfinfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY){
							SetFileAttributesL(src, FILE_ATTRIBUTE_NORMAL);
						}
						if ( DeleteFileL(src) == FALSE ){
							FWriteErrorLogs(FS, src, T("Move(Src delete)"), PPERROR_GETLASTERROR);
						}
					}
					FopLog(FS, src, dst, (fixdstatr ? LOG_MOVEOVERWRITE : LOG_MOVE ));
				}else{
					enum foplogtypes type;

					switch ( opt->fop.mode ){
						case FOPMODE_SHORTCUT: // ショートカット作成
						case FOPMODE_LINK: // ジャンクション作成
						case FOPMODE_SYMLINK: // シンボリックリンク作成
							type = LOG_LINK;
							break;
						default:
							type = fixdstatr ? LOG_COPYOVERWRITE : LOG_COPY;
					}
					FopLog(FS, src, dst, type);
				}
			}
		}
	}
	if ( FS->progs.info.filesall ){
		AddDD(FS->progs.info.donesize.l, FS->progs.info.donesize.h,
				srcfinfo.nFileSizeLow, srcfinfo.nFileSizeHigh);
	}
	FS->progs.info.donefiles++;
	return error;
}
