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

#define min_sectorsize 0x200 // 最小セクターサイズ
#define filebuffer_stacksize (min_sectorsize * 2)
#define filebuffer_defallocsize (min_sectorsize * 0x40)
			// ※ ２つともmin_sectorsize を越える必要がある

#define RETRYWAIT 200
#ifndef COPY_FILE_NO_BUFFERING
#define COPY_FILE_NO_BUFFERING 0x1000 // Ver6.0
#endif

//#define SubDW(dstL, dstH, srcL) {DWORD I_; I_ = (dstL); (dstL) -= (srcL); if ( (dstL) > I_ ) (dstH)--;}
#define AddLIDW(dst, srcL) {(dst.u.LowPart) += (srcL); if ( (dst.u.LowPart) < (srcL)) (dst.u.HighPart)++;}
#define SubLIDW(dst, srcL) {DWORD I_; I_ = (dst.u.LowPart); (dst.u.LowPart) -= (srcL); if ( (dst.u.LowPart) > I_ ) (dst.u.HighPart)--;}

typedef enum { DONE_NO = 0, DONE_OK, DONE_DIV, DONE_SKIP, DONE_MANUAL } DONEENUM;

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
			tstrcpy(ff.cFileName, name);
			ff.dwFileAttributes = srcattr;
		}
		if ( FinddataRegularExpression(&ff, &FS->maskFn) ) return ERROR_ALREADY_EXISTS;
	}else{
		if ( FilenameRegularExpression(name, &FS->maskFn) ) return ERROR_ALREADY_EXISTS;
	}
	FopLog(FS, src, NULL, LOG_SKIP);
	return NO_ERROR;
}

ERRORCODE DiskFillAction(FOPSTRUCT *FS, FILE_STAT_DATA *srcStat, const TCHAR *dst)
{
	ULARGE_INTEGER UserFree, Total, Free;
	TCHAR path[VFPS];
	BOOL result;

	// 2G over の場合は、分割を検討する。
	if ( (srcStat->nFileSizeHigh != 0) ||
		 (srcStat->nFileSizeLow >= 0x80000000) ){
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
			 ((UserFree.u.HighPart > srcStat->nFileSizeHigh) ||
			  ( (UserFree.u.HighPart == srcStat->nFileSizeHigh) &&
			   (UserFree.u.LowPart >= srcStat->nFileSizeLow) )) ){
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

	hFile = CreateFileL(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_ALWAYS, 0, NULL);
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
	FShowLog(FS);
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
	md.style = MB_ICONEXCLAMATION | MB_ABORTRETRYIGNORE | MB_DEFBUTTON1 | MB_PPX_ALLCHECKBOX;

	if ( autoretrymode && (error == ERROR_SHARING_VIOLATION) ){
		md.style = MB_ICONEXCLAMATION | MB_PPX_ABORTRETRYRENAMEIGNORE | MB_DEFBUTTON1 | MB_PPX_AUTORETRY | MB_PPX_ALLCHECKBOX;
		md.autoretryfunc = AutoRetryWriteOpen;
	}
	SetJobTask(FS->hDlg, JOBSTATE_ERROR);
	result = (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL), FS->hDlg, MessageBoxDxProc, (LPARAM)&md);
	SetJobTask(FS->hDlg, JOBSTATE_DEERROR);
	if ( result & ID_PPX_CHECKED ){
		resetflag(result, ID_PPX_CHECKED);
		FS->opt.erroraction = result;
	}
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
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

BOOL FixAttributes(const TCHAR *path, DWORD pathattr, struct FopOption *opt)
{
	DWORD newattr;

	newattr = (pathattr & (opt->fop.AtrMask | 0xffffffd8)) | (opt->fop.AtrFlag & 0x27);
	if ( (newattr | FILE_ATTRIBUTE_ARCHIVE) == pathattr ) return TRUE;
	return SetFileAttributesL(path, newattr);
}

typedef struct {
		ULARGE_INTEGER divsize; // 分割サイズ
		ULARGE_INTEGER leftsize; // 残サイズ
		BOOL use; // true...分割有り
		DWORD count;	// 分割番号
} DIVINFO;

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

typedef struct {
	HANDLE srcH, dstH;	// ファイルハンドル
	BY_HANDLE_FILE_INFORMATION srcfinfo, dstfinfo;	// ファイル情報
	DIVINFO div;

	BOOL append;	// 追加なら TRUE
	BOOL overwrite;	// 上書きする
	TCHAR *dsttail;
	DWORD SrcBurstFlag, DstBurstFlag;
					// コピー用バッファ ※filebufferは文字列加工用としても利用
	BYTE filebuffer[max(filebuffer_stacksize, VFPS) * 2];

} MCOPYSTRUCT;

ERRORCODE ManualCopy(FOPSTRUCT *FS, const TCHAR *src, const TCHAR *dst, FILE_STAT_DATA *srcStat, MCOPYSTRUCT *ms)
{
	DWORD buffersize;		// 読み書き大きさ
	DWORD oldtick;			// 書き込み間隔調整用tickカウンタ
	DWORD readstep, writestep;	// １度に読み書きする大きさ(ドライブ占有防止用)
	DWORD readsize, writesize;					// バッファを読み書きした量
	ERRORCODE error = NO_ERROR;
	int done = DONE_NO;
	BYTE *filebuf, *filebufp;
	BOOL deldst = TRUE;		// コピー先を削除する必要があるなら !0
	BOOL dst1st = TRUE; // (分割時の)最初のファイル書き込みなら true
	int retrycount;
	struct FopOption *opt;

	opt = &FS->opt;
	readsize = 0;

	// バッファを確保
	if ( FS->opt.CopyBuf == NULL ){
		if ( FS->opt.X_cbsz.CopyBufSize < filebuffer_defallocsize ){
			FS->opt.X_cbsz.CopyBufSize = filebuffer_defallocsize;
		}
		FS->opt.CopyBuf = VirtualAlloc(NULL,
				FS->opt.X_cbsz.CopyBufSize, MEM_COMMIT, PAGE_READWRITE);
		if ( FS->opt.CopyBuf == NULL ){
			FWriteErrorLogs(FS, NilStr, T("Alloc Buffer"), PPERROR_GETLASTERROR);
		}
	}
	if ( FS->opt.CopyBuf != NULL ){
		buffersize = FS->opt.X_cbsz.CopyBufSize;
		filebuf = FS->opt.CopyBuf;
	}else{ // メモリ確保できなかったのでスタックを使用
		FS->opt.X_cbsz.CopyBufSize = 0;
//		SrcBurstFlag = 0;
		buffersize = min_sectorsize;
		filebuf = ms->filebuffer +
				(buffersize - (ALIGNMENT_BITS(ms->filebuffer) & (buffersize - 1)) );
	}

	oldtick = GetTickCount();
	readstep = writestep = min(32 * KB, buffersize);
	if ( ms->append ) ms->DstBurstFlag = 0;	// Append時はバーストモード使用不可

	retrycount = FS->opt.errorretrycount;
	while ( (error == NO_ERROR) && (done == DONE_NO) ){ // コピー実体
		ULARGE_INTEGER CopyLeft; // 残サイズ
		DWORD dstatr; // 書き込み時の属性
														// コピー先を開く
		dstatr = (srcStat->dwFileAttributes & (opt->fop.AtrMask | 0xffffffd8)) | (opt->fop.AtrFlag & 0x27);

		if ( ms->div.count ){
			wsprintf(ms->dsttail, T(".%03d"), ms->div.count - 1);
		}

		ms->dstH = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0, NULL,
				dst1st ? (ms->append ? OPEN_ALWAYS : CREATE_ALWAYS) : CREATE_NEW,
				dstatr | FILE_FLAG_SEQUENTIAL_SCAN | ms->DstBurstFlag,
				(
	#ifndef UNICODE
					(WinType != WINTYPE_9x) &&
	#endif
					dst1st && !ms->append) ? ms->srcH : NULL);
		if ( ms->dstH == INVALID_HANDLE_VALUE ){
			deldst = FALSE;
			dst1st = FALSE;
			error = GetLastError();
			if ( (error == ERROR_SHARING_VIOLATION) ||
				 (error == ERROR_LOCK_VIOLATION) ){
				if ( retrycount > 0 ){
					retrycount--;
					Sleep(RETRYWAIT);
					continue;
				}
			}
			break;
		}
		if ( dst1st ){ // ファイルコピー開始時の処理(分割2ファイル目は実行無し)
			ULARGE_INTEGER nowpos;
			ULARGE_INTEGER curpos;

			CopyLeft.u.LowPart = FS->progs.FileSize.u.LowPart = ms->srcfinfo.nFileSizeLow;
			CopyLeft.u.HighPart = FS->progs.FileSize.u.HighPart = ms->srcfinfo.nFileSizeHigh;
			FS->progs.FileTransSize.u.LowPart = FS->progs.FileTransSize.u.HighPart = 0;

			if ( ms->append ){			// 追加なら末尾へ
				LONG sizetmp;

				sizetmp = 0;
				SetFilePointer(ms->dstH, 0, &sizetmp, FILE_END);
			}
									// 現在位置を取得
			nowpos.u.HighPart = 0;
			nowpos.u.LowPart = SetFilePointer(ms->dstH, 0, (PLONG)&nowpos.u.HighPart, FILE_CURRENT);
									// ファイルサイズをOSに通知
			if ( ms->div.use == FALSE ){
				curpos.u.HighPart = ms->srcfinfo.nFileSizeHigh;
				curpos.u.LowPart = ms->srcfinfo.nFileSizeLow;
				if ( ms->DstBurstFlag ){
					AddDD(curpos.u.LowPart, curpos.u.HighPart, SECTORSIZE - 1, 0);
					curpos.u.LowPart &= ~(SECTORSIZE - 1);
				}
				curpos.u.LowPart = SetFilePointer(ms->dstH, curpos.u.LowPart, (PLONG)&curpos.u.HighPart, FILE_CURRENT);
			}else{
				curpos.u.HighPart = ms->div.leftsize.u.HighPart;
				curpos.u.LowPart = SetFilePointer(ms->dstH, ms->div.leftsize.u.LowPart, (PLONG)&curpos.u.HighPart, FILE_CURRENT);
			}
			if ( (curpos.u.LowPart != MAX32) || (GetLastError() == NO_ERROR) ){
				if ( SetEndOfFile(ms->dstH) == FALSE ){
					error = GetLastError();
					if ( (error == ERROR_DISK_FULL) ||
						((error == ERROR_INVALID_PARAMETER) && (ms->srcfinfo.nFileSizeHigh || (ms->srcfinfo.nFileSizeLow >= 0x80000000))) ){
						error = DiskFillAction(FS, srcStat, dst); //分割確認
						if ( error == ERROR_NOT_SUPPORTED ){ // 2G 制限
							ms->div.divsize.u.LowPart = 0x7fff8000; // 2G - 32k
							ms->div.divsize.u.HighPart = 0;
							ms->div.leftsize = ms->div.divsize;
							ms->div.use = TRUE;
							error = NO_ERROR;
						}
					}
				}
				SetFilePointer(ms->dstH, nowpos.u.LowPart, (PLONG)&nowpos.u.HighPart, FILE_BEGIN);
			}
			dst1st = FALSE;
		}
		if ( ms->div.count && ms->div.use ){
			CopyLeft = ms->div.leftsize;
		}

		SetFullProgress(&FS->progs);
										// コピー処理 -------------------------
		while( error == NO_ERROR ){
			int errorretrycount;

			errorretrycount = FS->opt.errorretrycount;
											// 読み込み
			if ( readsize == 0 ){
				// バーストモード時、最後の読み込みは読込サイズを控えめにする
				if ( ms->SrcBurstFlag &&
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
					if ( ReadFile(ms->srcH, filebufp, rsize, &size, NULL) ){
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
						SetFullProgress(&FS->progs);
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
						// 読む & min_sectorsizeバイトスキップする
						FS->opt.erroraction = IDIGNORE;
						readstep = buffersize = min_sectorsize;
						memset(filebuf, 0x55,readstep);
						tmp = 0;
						SetFilePointer(ms->srcH, readstep, (PLONG)&tmp, FILE_CURRENT);
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
			if ( (readsize != buffersize) && ms->DstBurstFlag ){
				DWORD tmp;

				if ( WriteFile(ms->dstH, filebufp, buffersize, &writesize, NULL)
						== FALSE){
					error = FOPERROR_GETLASTERROR;	// 書き込み失敗
					break;
				}
				CloseHandle(ms->dstH);
				AddLIDW(FS->progs.FileTransSize, readsize);

				if ( dstatr & FILE_ATTRIBUTE_READONLY ){
					SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
				}
//				retrycount = FS->opt.errorretrycount;
				ms->dstH = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0,
						NULL, OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL);
				if ( ms->dstH == INVALID_HANDLE_VALUE ){
					error = GetLastError();
				}else{ // サイズ縮小
					tmp = MAX32;
					SetFilePointer(ms->dstH, readsize - buffersize,
							(PLONG)&tmp, FILE_END);
					if ( !SetEndOfFile(ms->dstH) ){
						error = GetLastError();
					}
				}
				done = DONE_OK;
				break;
			}
			while ( readsize != 0 ){
				ERRORCODE err;
				DWORD tick, size, wsize;

				wsize = min(writestep, readsize);
				if ( ms->div.use && (ms->div.leftsize.u.HighPart == 0) && (wsize > ms->div.leftsize.u.LowPart) ){
					wsize = ms->div.leftsize.u.LowPart;
				}
				if ( IsTrue(WriteFile(ms->dstH, filebufp, wsize, &writesize, NULL)) ){
					AddLIDW(FS->progs.FileTransSize, writesize);
									// writesize が正しく取得できない場合の対策
					if ( writesize != wsize ){
						err = ERROR_DISK_FULL;
					}else{
						readsize -= writesize;
						filebufp += writesize;
						if ( ms->div.use ){
							if ( ms->div.leftsize.u.HighPart > 0 ){
								SubLIDW(ms->div.leftsize, writesize);
							}else{
								ms->div.leftsize.u.LowPart -= writesize;
								if ( ms->div.leftsize.u.LowPart == 0 ){
									ms->div.leftsize = ms->div.divsize;
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
						SetFullProgress(&FS->progs);
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
			ms->div.count++;
		}
		if ( error == FOPERROR_GETLASTERROR ) error = GetLastError();
//FlushFileBuffers
		if ( ms->dstH != INVALID_HANDLE_VALUE ){
			if ( error != NO_ERROR ){
				if ( deldst ){		// ファイルの大きさを 0 にする
					DWORD tmp = 0;

					SetFilePointer(ms->dstH, 0, (PLONG)&tmp, FILE_BEGIN);
					SetEndOfFile(ms->dstH);
				}
				CloseHandle(ms->dstH);

				if ( deldst ){
					if ( dstatr & FILE_ATTRIBUTE_READONLY ){
						SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
					}
					DeleteFileL(dst);
				}
				break;
			}else{
				FILE_STAT_DATA fsd;

				SetFileTime(ms->dstH,	NULL, //&srcfinfo.ftCreationTime,
									&ms->srcfinfo.ftLastAccessTime,
									&ms->srcfinfo.ftLastWriteTime);
				CloseHandle(ms->dstH);

				// 正しくコピーできたか確認する
				if ( IsTrue(GetFileSTAT(dst, &fsd)) ){
					if ( ms->div.count ){
						// 最後の 0byte ファイルを削除
						if ( (fsd.nFileSizeLow == 0) && (fsd.nFileSizeHigh == 0) ){
							DeleteFileL(dst);
							ms->div.count--;
						}
					}else if ( !ms->append &&
							((ms->srcfinfo.nFileSizeLow != fsd.nFileSizeLow) ||
							 (ms->srcfinfo.nFileSizeHigh != fsd.nFileSizeHigh))){
						error = ERROR_INVALID_DATA;
						DeleteFileL(dst);
					}

					if ( fsd.dwFileAttributes != dstatr ){
						SetFileAttributesL(dst, dstatr);
					}
					if ( opt->security != SECURITY_FLAG_NONE ){
						error = CopySecurity(FS, src, dst);
					}
					if ( done == DONE_DIV ){
						if ( ms->div.count == 1 ){
							TCHAR buf[VFPS];

							tstrcpy(buf, dst);
							tstrcpy(ms->dsttail, T(".000"));
							ms->div.count++;
							MoveFileL(buf, dst);
						}
						done = DONE_NO;
						XMessage(FS->hDlg, STR_FOP, XM_INFOld, MES_NEWD);
					}
				}else{
					error = ERROR_INVALID_DATA;
				}
			}
		}else if ( ms->overwrite && (error != NO_ERROR) &&
				(ms->dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES) ){
			// 上書きに失敗したので、元のファイルの属性を戻す
			SetFileAttributesL(dst, ms->dstfinfo.dwFileAttributes);
		}
	}
	if ( error == NO_ERROR ){
												// 連結バッチファイルの生成
		if( ms->div.count && (FS->opt.fop.flags & VFSFOP_OPTFLAG_JOINBATCH) ){
			HANDLE hBFile;
			DWORD divi;
			TCHAR *dstptr;
			DWORD writesize;

			tstrcpy(ms->dsttail, T(".bat"));
			dstptr = VFSFindLastEntry(dst);
			if ( *dstptr == '\\' ) dstptr++;
			hBFile = CreateFileL(dst, GENERIC_READ | GENERIC_WRITE, 0, NULL,
					CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if ( hBFile != INVALID_HANDLE_VALUE ){
				WriteFile(hBFile, "@copy /b \"", 10, &writesize, NULL);
				for ( divi = 0 ; ; ){
					wsprintf(ms->dsttail, T(".%03d"), divi);
					WriteFileZT(hBFile, dstptr, &writesize);
					if ( ++divi == ms->div.count ){
						WriteFile(hBFile, "\" \"", 3, &writesize, NULL);
						*ms->dsttail = '\0';
						WriteFileZT(hBFile, dstptr, &writesize);
						WriteFile(hBFile, "\"\r\n", 3, &writesize, NULL);
						break;
					}else{
						WriteFile(hBFile, "\" + \"", 5, &writesize, NULL);
					}
				}
			}
			CloseHandle(hBFile);
		}
	}
	return error;
}

/*
	NO_ERROR	コピーをおこなった
	ERROR_FILE_EXISTS	存在チェック
	ERROR_OCF_MCOPY		(COPY_FILE_FAIL_IF_EXISTS時)コピー先がある
						分割する
						特殊な書式
	ERRORFLAG_SKIP(Customer code flag) Skip error
*/
#define ERRORFLAG_SKIP B29
#define ERROR_OCF_MCOPY ERROR_OUT_OF_PAPER

ERRORCODE TryCopyFileEx(FOPSTRUCT *FS, const TCHAR *src, const TCHAR *dst, FILE_STAT_DATA *srcStat, DWORD flags, DWORD FileKiroSize, DIVINFO *div)
{
	int errorretrycount = FS->opt.errorretrycount;

	FS->Cancel = FALSE;
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_ALLOWDECRYPT ){
		setflag(flags, COPY_FILE_ALLOW_DECRYPTED_DESTINATION);
	}
	if ( (WinType >= WINTYPE_VISTA) &&
		 ((FS->opt.X_cbsz.DisableApiCacheSize != 0) &&
		  (FileKiroSize >= FS->opt.X_cbsz.DisableApiCacheSize)) ){
		setflag(flags, COPY_FILE_NO_BUFFERING);
	}

	for (;;){
		ERRORCODE error;
		BOOL OldNoAutoClose;
		int result;

		if ( IsTrue(CopyFileExL(src, dst, (LPPROGRESS_ROUTINE)CopyProgress,
				&FS->progs, &FS->Cancel, flags)) ){
			FixAttributes(dst, srcStat->dwFileAttributes, &FS->opt);
			if ( (FS->opt.fop.same == FOPSAME_ARCHIVE) &&
				 (srcStat->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ){
				resetflag(srcStat->dwFileAttributes, FILE_ATTRIBUTE_ARCHIVE);
				SetFileAttributesL(src, srcStat->dwFileAttributes);
			}

			if ( FS->opt.security != SECURITY_FLAG_NONE ){
				return CopySecurity(FS, src, dst);
			}
			if ( FS->state != FOP_TOBREAK ) return NO_ERROR;
			return ERROR_CANCELLED;
		}

		OldNoAutoClose = FS->NoAutoClose;
		error = GetLastError();
		if ( error == NO_ERROR ) error = ERROR_ACCESS_DENIED;
		// 先にファイル存在
		if ( (error == ERROR_FILE_EXISTS) &&
			 (flags & COPY_FILE_FAIL_IF_EXISTS) ){
			return ERROR_FILE_EXISTS;
		} else

		// open 不可
		if ( error == ERROR_ACCESS_DENIED ){
			if ( IsTrue(FS->Cancel) ){
				return ERROR_CANCELLED;
			}else{
				DWORD attr;

				attr = GetFileAttributesL(dst);
						// ディレクトリに対してコピーしようとした
				if ( (attr != BADATTR) && (attr & FILE_ATTRIBUTE_DIRECTORY) ){
					if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
						FS->progs.info.LEskips++;
						FWriteErrorLogs(FS, src, T("Dest"), ERROR_DIRECTORY);
						return NO_ERROR | ERRORFLAG_SKIP;
					}
				}
			}
		} else

		// 空き不足／サイズ制限
		if ( (error == ERROR_DISK_FULL) ||
			 ((error == ERROR_INVALID_PARAMETER) && (srcStat->nFileSizeHigh || (srcStat->nFileSizeLow >= 0x80000000))) ){
			error = DiskFillAction(FS, srcStat, dst); //分割確認
			if ( error != ERROR_CANCELLED ){ // 分割指示有り
				if ( error == ERROR_NOT_SUPPORTED ){ // 2G 制限
					div->divsize.u.LowPart = 0x7fff8000; // 2G - 32k
					div->divsize.u.HighPart = 0;
				}
				if ( div->divsize.u.LowPart | div->divsize.u.HighPart ){
					div->use = TRUE;
					div->leftsize = div->divsize;
					if ( (srcStat->nFileSizeHigh > div->leftsize.u.HighPart) ||
						 ((srcStat->nFileSizeHigh == div->leftsize.u.HighPart) &&
						  (srcStat->nFileSizeLow > div->leftsize.u.LowPart)) ){
						div->count++;
					}
				}
				error = ERROR_OCF_MCOPY;
			}
			return error;
		}

		// コピー先が使えない書式
		if ( (error == ERROR_BAD_NETPATH) &&
			 ( ((src[0] == '\\') && (src[1] == '\\') && (src[2] == '.')) ||
			   ((dst[0] == '\\') && (dst[1] == '\\') && (dst[2] == '.'))) ){
			// コピー元・先が \\.\Volume～ や \\.\Harddisk0Partition1
			// の場合、CopyFileEx ではエラーになるので自前コピーをする
			return ERROR_OCF_MCOPY;
		}

		// その他の不明なエラー → リトライを行う
		if ( (error != ERROR_CANCELLED) && (error != ERROR_REQUEST_ABORTED) && errorretrycount && (FS->Cancel == FALSE) ){
			errorretrycount--;
			FWriteErrorLogs(FS, dst, T("Dest AutoRetry"), error);
			Sleep(RETRYWAIT);
			FS->NoAutoClose = OldNoAutoClose;
			continue;
		}

		// その他の不明なエラー 対処方法を選択・処理
		result = ErrorActionMsgBox(FS, error, dst, TRUE);
		if ( FS->opt.erroraction == 0 ){
			FS->NoAutoClose = OldNoAutoClose;
		}
		if ( result == IDRETRY ){
			continue;
		}else if ( result == IDX_FOP_ADDNUMDEST ){
			error = RenameDestFile(FS, dst, TRUE);
			if ( error != NO_ERROR ){
				PPErrorBox(FS->hDlg, dst, error);
			}
			continue;
		}else if ( result == IDIGNORE ){
			// アクセスが拒否された場合→コピーそのものをしない
			if ( (error == ERROR_ACCESS_DENIED) ||
				 (error == ERROR_SHARING_VIOLATION ) ||
				 (error == ERROR_LOCK_VIOLATION ) ){
				FS->progs.info.errors++;
				FWriteErrorLogs(FS, dst, T("Error Skip"), error);
				return error | ERRORFLAG_SKIP;
			}
//		buffersize = min_sectorsize;
//		SrcBurstFlag = 0; バーストモードにしてシステム介入をさけた方がいいかも。
//		error = ERROR_OCF_MCOPY; // 強制コピー処理へ
//		break;
			return ERROR_OCF_MCOPY;
		}
		// cancel
		return ERROR_CANCELLED;
	}
}

/*-----------------------------------------------------------------------------
	ファイルの移動を試みる
		ERROR_ALREADY_EXISTS:この方法では無理
		それ以外:正常終了／エラーコード
-----------------------------------------------------------------------------*/
ERRORCODE TryMoveFile(FOPSTRUCT *FS, const TCHAR *src, const TCHAR *dst, FILE_STAT_DATA *srcStat)
{
	ERRORCODE result;
	#define TMF_RETRY B31

	if ( IsTrue(MoveFileWithProgressL(src, dst,
			(LPPROGRESS_ROUTINE)CopyProgress, &FS->progs, 0)) ){
		FS->progs.info.donefiles++;
		if ( !(srcStat->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			AddDD(FS->progs.info.donesize.l, FS->progs.info.donesize.h,
					srcStat->nFileSizeLow, srcStat->nFileSizeHigh);
		}
		SetTinyProgress(FS);
		if ( srcStat->dwFileAttributes & TMF_RETRY ){ // TMF_RETRY は強制変更
			SetFileAttributesL(dst, srcStat->dwFileAttributes);
		}else{
			FixAttributes(dst, srcStat->dwFileAttributes, &FS->opt);
		}
		PeekMessageLoop(FS);
		return NO_ERROR;
	}
	result = GetLastError();
	if ( (result == ERROR_ACCESS_DENIED) &&
		 !(srcStat->dwFileAttributes & TMF_RETRY) &&
		 (srcStat->dwFileAttributes & OPENERRORATTRIBUTES) ){
		// 移動できない属性なので、NORMALだけにして試す
		if ( IsTrue(SetFileAttributesL(src, FILE_ATTRIBUTE_NORMAL)) ){
			setflag(srcStat->dwFileAttributes, TMF_RETRY);
			result = TryMoveFile(FS, src, dst, srcStat);
			resetflag(srcStat->dwFileAttributes, TMF_RETRY);
			if ( result != NO_ERROR ){ // エラーのときは src を元に戻す
				SetFileAttributesL(src, srcStat->dwFileAttributes);
			}
			return result;
		}
	}

	if ( (result != ERROR_NOT_SAME_DEVICE) &&
		 (result != ERROR_ALREADY_EXISTS) &&
		 (result != ERROR_ACCESS_DENIED) ) return result;
	return ERROR_ALREADY_EXISTS;
}

/*-----------------------------------------------------------------------------
	file → file へのコピールーチン
	src, dst:	コピー元/先（fullpath）
-----------------------------------------------------------------------------*/
ERRORCODE DlgCopyFile(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst, FILE_STAT_DATA *srcStat)
{
	MCOPYSTRUCT ms;
	struct FopOption *opt;

	ERRORCODE error = NO_ERROR;	// エラー発生時は NO_ERROR 以外
	DONEENUM done = DONE_NO;	// 作業が終了したら !DONE_NO
	DWORD FileKiroSize;
	int retrycount; // 失敗したときにやり直す回数

	opt = &FS->opt;
	ms.append = FALSE;
	ms.overwrite = FALSE;

	if ( FS->hProgsEvent == NULL ){
		PeekMessageLoopSub(FS);
		if ( FS->state != FOP_BUSY ){
			PeekMessageLoop(FS);
			if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;
		}
	}else{
		if ( FS->state == FOP_TOBREAK ) return ERROR_CANCELLED;
	}

	if ( !(FS->maskFnFlags & REGEXPF_BLANK) ){
		ERRORCODE erc;

		erc = CheckEntryMask(FS, src, srcStat->dwFileAttributes);
		if ( erc != ERROR_ALREADY_EXISTS ) return erc;
	}

	if ( opt->fop.mode == FOPMODE_DELETE ){
		ERRORCODE result;

		if ( opt->fop.flags & (VFSFOP_OPTFLAG_BACKUPOLD | VFSFOP_OPTFLAG_UNDOLOG) ){
			result = BackupFile(FS, src);
		}else{
			result = VFSDeleteEntry(&FS->DelStat, src, srcStat->dwFileAttributes);
		}
		if ( result == NO_ERROR ){
			FopLog(FS, src, NULL, LOG_DELETE);
		}else{
			BOOL OldNoAutoClose = FS->NoAutoClose;

			FWriteErrorLogs(FS, src, T("Delete"),result);
			if ( FS->DelStat.useaction == 0 ) FS->NoAutoClose = OldNoAutoClose;
		}
		return result;
	}else if ( opt->fop.mode == FOPMODE_SHORTCUT ){
		TCHAR *ptr;

		ptr = VFSFindLastEntry(dst);
		ptr += FindExtSeparator(ptr);
		tstrcpy(ptr, StrShortcutExt);
	}

	if ( FS->ifo != NULL ){
		CopyFileWithIfo(FS, src, dst);
		return NO_ERROR;
	}

	// 移動を試みる -----------------------------------------------------------
	while ( CheckSaveDriveMove(opt, src, dst) && (opt->fop.divide_num == 0) ){
		ERRORCODE tryresult;

		if ( IsTrue(FS->renamemode) &&
			 (opt->fop.same != FOPSAME_ADDNUMBER) &&
			 (tstrcmp(VFSFindLastEntry(src), VFSFindLastEntry(dst)) == 0) ){
			if ( GetFileAttributesL(dst) != BADATTR ) break;
		}
		tryresult = TryMoveFile(FS, src, dst, srcStat);
		if ( (tryresult == ERROR_SHARING_VIOLATION) &&
			 !(opt->fop.flags & VFSFOP_OPTFLAG_WAIT_CLOSE) ){
			MESSAGEDATA md;
			int msgresult;

			GetAccessApplications(src, (TCHAR *)ms.filebuffer);
			md.title = src;
			md.text = (TCHAR *)ms.filebuffer;
			md.style = MB_RETRYCANCEL | MB_DEFBUTTON1 | MB_ICONEXCLAMATION | MB_PPX_AUTORETRY;
			md.autoretryfunc = AutoRetryOpen;

			SetJobTask(FS->hDlg, JOBSTATE_ERROR);
			msgresult = (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL),
					FS->hDlg, MessageBoxDxProc, (LPARAM)&md);
			SetJobTask(FS->hDlg, JOBSTATE_DEERROR);
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

	if ( srcStat->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
		ERRORCODE result;

		result = FileOperationReparse(FS, src, dst, srcStat->dwFileAttributes);
		if ( result != ERROR_MORE_DATA ) return result;
	}

	// 分割チェック
	ms.div.count = 0;
	if ( opt->fop.divide_num == 0 ){ // 分割しない
		ms.div.use = FALSE;
	}else{
		const TCHAR *ptr;

		wsprintf((TCHAR *)ms.filebuffer, T("%u%c"), opt->fop.divide_num, opt->fop.divide_unit);
		ptr = (const TCHAR *)ms.filebuffer;
		GetSizeNumber(&ptr, &ms.div.divsize.u.LowPart, &ms.div.divsize.u.HighPart);
		ms.div.use = TRUE;
		ms.div.leftsize = ms.div.divsize;
		if ( ( srcStat->nFileSizeHigh >  ms.div.leftsize.u.HighPart) ||
			 ((srcStat->nFileSizeHigh == ms.div.leftsize.u.HighPart) &&
			  (srcStat->nFileSizeLow  >  ms.div.leftsize.u.LowPart)) ){
			ms.div.count++;
		}
	}

	FileKiroSize = (srcStat->nFileSizeHigh >> 10) ?
			FFD : ShiftDivDD(srcStat->nFileSizeLow, srcStat->nFileSizeHigh, 10);

	// バーストチェック
	ms.SrcBurstFlag = ms.DstBurstFlag = 0;
	if ( FS->opt.burst ||
		 ( (FS->opt.X_cbsz.EnablePPxBurstSize != 0) &&
		   (FileKiroSize >= FS->opt.X_cbsz.EnablePPxBurstSize) ) ){
		ms.SrcBurstFlag = ms.DstBurstFlag = FILE_FLAG_NO_BUFFERING;
	}

	// 新規作成でコピーできるならコピー(appendは常に新規作成のため判定不要)
	if ( FOPMODE_ISCOPYMOVE(opt->fop.mode) && !ms.div.count && (ms.SrcBurstFlag == 0) ){
		ERRORCODE ercode;

		ercode = TryCopyFileEx(FS, src, dst, srcStat, COPY_FILE_FAIL_IF_EXISTS, FileKiroSize, &ms.div);
		if ( ercode == NO_ERROR ){
			done = DONE_OK;
			goto fin;
		}
		if ( ercode & ERRORFLAG_SKIP ){
			error = ercode & ~ERRORFLAG_SKIP;
			goto fin;
		}
		if ( ercode == ERROR_OCF_MCOPY ){
			done = DONE_MANUAL;
		}else if ( ercode != ERROR_FILE_EXISTS ){
			error = ercode;
			goto fin;
		}
	}
										// コピー元を開く ---------------------
	retrycount = FS->opt.errorretrycount;
	while ( (ms.srcH = CreateFile_OpenSource(src, ms.SrcBurstFlag)) == INVALID_HANDLE_VALUE ){
		int result;
		BOOL OldNoAutoClose;

		OldNoAutoClose = FS->NoAutoClose;
		error = GetLastError();
		if ( (error == ERROR_SHARING_VIOLATION) ||
			 (error == ERROR_LOCK_VIOLATION) ){
			if ( retrycount > 0 ){
				retrycount--;
				Sleep(RETRYWAIT);
				continue;
			}
		}

		// src は開けないが、無視可能？
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

		// その他のエラーは、リトライしてから対処方法選択
		if ( (error != ERROR_CANCELLED) &&
			 (error != ERROR_REQUEST_ABORTED) &&
			 (retrycount > 0) ){
			retrycount--;
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

	ms.srcfinfo.dwFileAttributes = BADATTR;
	ms.srcfinfo.nFileSizeHigh = MAX32;

	if ( GetFileInformationByHandle(ms.srcH, &ms.srcfinfo) == FALSE ){
		error = FOPERROR_GETLASTERROR;
	}

#ifndef UNICODE
	/*------------------------------------------
	NEC PC98x1用Windows95～機種不問Win9x間でネットワーク経由
	で参照すると属性が正しく得られないための処置
	------------------------------------------*/
	if ( ms.srcfinfo.dwFileAttributes != srcStat->dwFileAttributes ){
		HANDLE hFF;
		WIN32_FIND_DATA ff;

		hFF = FindFirstFileL(src, &ff);
		if ( hFF == INVALID_HANDLE_VALUE ){
			error = FOPERROR_GETLASTERROR;
		}else{
			FindClose(hFF);
			ms.srcfinfo.dwFileAttributes = ff.dwFileAttributes;
			ms.srcfinfo.nFileSizeHigh = ff.nFileSizeHigh;
			ms.srcfinfo.nFileSizeLow = ff.nFileSizeLow;
		}
	}
#endif

//	存在チェック、 CreateFile (open) →見つけた → CreateFile (create) で再作成
	retrycount = FS->opt.errorretrycount;
	while ( error == NO_ERROR ){
									// 同名ファイルがあるか確認する
		// ※ これで得たハンドルを再利用しても、属性、拡張属性を変更できない
		//    ため、再利用せずに廃棄する。
		ms.dstH = CreateFileL(dst, GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | ms.DstBurstFlag, NULL);
		if ( ms.dstH == INVALID_HANDLE_VALUE ){ // 存在するけど開けなかった
			ERRORCODE dsterror;

			dsterror = GetLastError();

			if ( (dsterror == ERROR_SHARING_VIOLATION) ||
				 (dsterror == ERROR_LOCK_VIOLATION) ){
				if ( GetFileAttributesL(dst) != BADATTR ){
					if ( retrycount > 0 ){
						retrycount--;
						Sleep(RETRYWAIT);
						continue;
					}
					ms.dstH = NULL;
				}
			}
		}
		if ( ms.dstH != INVALID_HANDLE_VALUE ){	// 発見
			ERRORCODE result;

			result = SameNameAction(FS, ms.dstH, &ms.srcfinfo, &ms.dstfinfo, src, dst);
			switch( result ){
				case ACTION_RETRY:
					if ( CheckSaveDriveMove(opt, src, dst) && (opt->fop.divide_num == 0) ){
						if ( ms.srcH != INVALID_HANDLE_VALUE ){
							CloseHandle(ms.srcH);
							ms.srcH = INVALID_HANDLE_VALUE;
						}
						result = TryMoveFile(FS, src, dst, srcStat);
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
					ms.append = TRUE;
					// ACTION_OVERWRITE へ
					// Fall through
				case ACTION_OVERWRITE:
					ms.overwrite = TRUE;
					break;

				case ACTION_CREATE:
					break;

				default: // ACTION_SKIP 又は error
					if ( (result != NO_ERROR) &&
						 (result <= ACTION_ERROR) &&
						 (result != ERROR_CANCELLED) ){ // 各種エラー
						switch ( ErrorActionMsgBox(FS, result, dst, FALSE) ){
							case IDRETRY:
								continue;

							case IDIGNORE:
								break;

							default: // キャンセル→対処方法選択に戻る
								opt->fop.sameSW = 0;
								continue;
						}
					}

					if ( ms.srcH != INVALID_HANDLE_VALUE ){
						CloseHandle(ms.srcH);
					}

					if ( result == ACTION_SKIP ){
						FS->progs.info.EXskips++;
						FopLog(FS, src, dst, LOG_SKIP);
						if ( FS->progs.info.filesall ){
							SubDD(FS->progs.info.allsize.l,
								FS->progs.info.allsize.h,
								ms.srcfinfo.nFileSizeLow,
								ms.srcfinfo.nFileSizeHigh);
						}
						result = NO_ERROR;
					}
					return result;
			}
			if ( CheckSaveDriveMove(opt, src, dst) && (opt->fop.divide_num == 0) ){
				if ( ms.srcH != INVALID_HANDLE_VALUE ){
					CloseHandle(ms.srcH);
					ms.srcH = INVALID_HANDLE_VALUE;
				}
				if ( ms.dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES ){
					SetFileAttributesL(dst, FILE_ATTRIBUTE_NORMAL);
				}
				DeleteFileL(dst);
				result = TryMoveFile(FS, src, dst, srcStat);
				if ( result != ERROR_ALREADY_EXISTS ){
					if ( result == NO_ERROR ){
						FopLog(FS, src, dst, LOG_MOVEOVERWRITE);
					}
					return result;
				}
			}

			if ( ms.srcH == INVALID_HANDLE_VALUE ){	// 再オープン
				ms.srcH = CreateFile_OpenSource(src, ms.SrcBurstFlag);
			}
		}
		break;
	}

	ms.dsttail = dst + tstrlen(dst);
										// コピー先を開く ---------------------
	if ( done == DONE_MANUAL ){
		done = DONE_NO;
	}else if ( (error == NO_ERROR) && !FOPMODE_ISCOPYMOVE(opt->fop.mode) ){
		if ( opt->fop.mode == FOPMODE_SHORTCUT ){
			error = FOPMakeShortCut(src, dst, FALSE, FALSE);
		}else if ( FS->opt.fop.mode == FOPMODE_LINK ){
			if ( DCreateHardLink(dst, src, NULL) == FALSE ){
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
	{	// CopyFileEx でコピー
		if ( (error == NO_ERROR) && !ms.div.count && !ms.append && (ms.SrcBurstFlag == 0) ){
			ERRORCODE ercode;

			ercode = TryCopyFileEx(FS, src, dst, srcStat, 0, FileKiroSize, &ms.div);
			if ( ercode == NO_ERROR ){
				done = DONE_OK;
			}else if ( ercode & ERRORFLAG_SKIP ){
				error = ercode & ~ERRORFLAG_SKIP;
				done = DONE_SKIP;
			}else if ( ercode == ERROR_OCF_MCOPY ){
				done = DONE_NO;
			}else {
				error = ercode;
			}
		}
	}

	if ( (error == NO_ERROR) && (done == DONE_NO) ){
		error = ManualCopy(FS, src, dst, srcStat, &ms); // 自前コピー
		if ( error == NO_ERROR ) done = DONE_OK;
	}

	if ( FS->state == FOP_TOBREAK ) error = ERROR_CANCELLED;
	if ( ms.srcH != INVALID_HANDLE_VALUE ) CloseHandle(ms.srcH);
fin:
	if ( (error == NO_ERROR) && (done != DONE_NO) ){
		if ( done == DONE_OK ){
												// コピー処理で"ArcOnly"の処理
			if ( (opt->fop.mode != FOPMODE_MOVE) &&
				(opt->fop.same == FOPSAME_ARCHIVE) &&
				(srcStat->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ){
				resetflag(srcStat->dwFileAttributes, FILE_ATTRIBUTE_ARCHIVE);
				SetFileAttributesL(src, srcStat->dwFileAttributes);
			}
		}
		if ( ms.append ){
			FopLog(FS, src, dst, LOG_APPEND);
		}else if ( opt->fop.mode == FOPMODE_MOVE ){ // 移動→元を削除
			if ( done == DONE_OK ){
				if (ms.srcfinfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY){
					SetFileAttributesL(src, FILE_ATTRIBUTE_NORMAL);
				}
				if ( DeleteFileL(src) == FALSE ){
					FWriteErrorLogs(FS, src, T("Move(Src delete)"), PPERROR_GETLASTERROR);
				}
			}
			FopLog(FS, src, dst, (ms.overwrite ? LOG_MOVEOVERWRITE : LOG_MOVE ));
		}else{
			enum foplogtypes type;

			switch ( opt->fop.mode ){
				case FOPMODE_SHORTCUT: // ショートカット作成
				case FOPMODE_LINK: // ジャンクション作成
				case FOPMODE_SYMLINK: // シンボリックリンク作成
					type = LOG_LINK;
					break;
				default:
					type = ms.overwrite ? LOG_COPYOVERWRITE : LOG_COPY;
			}
			FopLog(FS, src, dst, type);
		}
	}
	AddDD(FS->progs.info.donesize.l, FS->progs.info.donesize.h,
			srcStat->nFileSizeLow, srcStat->nFileSizeHigh);
	FS->progs.info.donefiles++;
	return error;
}
