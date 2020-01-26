/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System	ファイル操作,ディレクトリコピー
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
#pragma hdrstop

BOOL GetDirFinfo(const TCHAR *dirpath, BY_HANDLE_FILE_INFORMATION *finfo)
{
	HANDLE hDir;
	WIN32_FIND_DATA ff;
	HANDLE hFF;

	hDir = CreateFileL(dirpath, 0, FILE_SHARE_WRITE | FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if ( hDir != INVALID_HANDLE_VALUE ){
		if ( GetFileInformationByHandle(hDir, finfo) == FALSE ){
			CloseHandle(hDir);
			return TRUE;
		}
		CloseHandle(hDir);
	}
	memset(finfo, 0, sizeof(BY_HANDLE_FILE_INFORMATION));

	hFF = FindFirstFileL(dirpath, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return FALSE;
	finfo->dwFileAttributes = ff.dwFileAttributes;
	finfo->ftCreationTime = ff.ftCreationTime;
	finfo->ftLastAccessTime = ff.ftLastAccessTime;
	finfo->ftLastWriteTime = ff.ftLastWriteTime;
	FindClose(hFF);
	return TRUE;
}


ERRORCODE DlgDirRename(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst)
{
	BY_HANDLE_FILE_INFORMATION srcfinfo, dstfinfo;	// ファイル情報
	ERRORCODE result;

	if ( GetDirFinfo(src, &srcfinfo) == FALSE ) return ERROR_FILE_NOT_FOUND;
	for ( ; ; ){
									// 同名ファイルがあるか確認する
		if ( IsTrue(GetDirFinfo(dst, &dstfinfo)) ){
			ERRORCODE ar = SameNameAction(FS, NULL, &srcfinfo, &dstfinfo, src, dst);

			switch( ar ){
				case ACTION_RETRY:
				case ACTION_APPEND:
				case ACTION_OVERWRITE:
				case ACTION_CREATE:
					break;

				case ACTION_SKIP:
					ar = NO_ERROR;
					FS->progs.info.EXskips++;
					// default へ
				default: // ACTION_SKIP 又は error
					return ar;
			}
		}
		if ( FS->renamemode && !(FS->opt.fop.filter & VFSFOP_FILTER_NODIRFILTER) ){
			if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);
			if ( IsTrue(MoveFileL(src, dst)) ){
				SHChangeNotify(SHCNE_RENAMEFOLDER, SHCNF_PATH, src, dst);
				return NO_ERROR;
			}
			result = GetLastError();
		}else{
			result = DlgCopyDir(FS, src, dst, srcfinfo.dwFileAttributes);
		}
		if ( (result != ERROR_ALREADY_EXISTS) &&
			 (result != ERROR_INVALID_NAME) ){
			return result;
		}
	}
}

ERRORCODE GetFFList(ThSTRUCT *th, const TCHAR *path)
{
	HANDLE hFile;
	ERRORCODE result;
	TCHAR buf[VFPS];

	ThInit(th);
	ThSize(th, sizeof(WIN32_FIND_DATA));
	CatPath(buf, (TCHAR *)path, T("*.*"));
	hFile = FindFirstFileL(buf, (WIN32_FIND_DATA *)th->bottom);
	if ( hFile == INVALID_HANDLE_VALUE ){
		result = GetLastError();
		goto error2;
	}
	do {
		if ( !IsRelativeDirectory(((WIN32_FIND_DATA *)ThLast(th))->cFileName)){
			th->top += sizeof(WIN32_FIND_DATA);
		}
		if ( ThSize(th, sizeof(WIN32_FIND_DATA)) == FALSE ) goto error;
	}while( IsTrue(FindNextFile(hFile, (WIN32_FIND_DATA *)ThLast(th))) );
	result = GetLastError();
	if ( result == ERROR_NO_MORE_FILES ){
		result = NO_ERROR;
	}else{
		ThFree(th);
	}
	FindClose(hFile);
	return result;
error:
	result = GetLastError();
	FindClose(hFile);
error2:
	ThFree(th);
	return result;
}

typedef struct {
	DWORD attr;
	TCHAR src[VFPS];
	TCHAR dst[VFPS];
} DELAYINFO;
/*-----------------------------------------------------------------------------
	dir → dir へのコピールーチン

	hDlg:		ダイアログボックスへのハンドル（状況表示、同名処理など）
	src, dst:	コピー元/先（fullpath）

	->			0:正常終了 1223(ERROR_CANCELLED):中止 その他:エラー番号
-----------------------------------------------------------------------------*/
ERRORCODE DlgCopyDir(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst, DWORD srcattr)
{
	ERRORCODE result;
	HANDLE hFF;
	WIN32_FIND_DATA find;
	TCHAR buf[VFPS], dbuf[VFPS + 80];
	DELAYINFO *delay_dest;
	ThSTRUCT thDestDirList;
	DWORD srclen, dstlen;

	// ディレクトリのマスク
	if ( (FS->maskFnFlags & (REGEXPF_BLANK | REGEXPF_PATHMASK)) == REGEXPF_PATHMASK ){
		ERRORCODE erc;

		erc = CheckEntryMask(FS, src, srcattr);
		if ( erc != ERROR_ALREADY_EXISTS ) return erc;
	}
/*
	if ( IsTrue(FS->testmode) &&
		(FS->renamemode && !(FS->opt.fop.filter & VFSFOP_FILTER_NOFILEFILTER))){
		return TestDest(FS, src, dst);
	}
*/
	// 削除処理
	if ( FS->opt.fop.mode == FOPMODE_DELETE ){
		FopLog(FS, src, dst, LOG_DIR_DELETE);
		if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);

		if ( FS->opt.fop.flags & (VFSFOP_OPTFLAG_BACKUPOLD | VFSFOP_OPTFLAG_UNDOLOG) ){
			result = BackupFile(FS, src);
		}else{
			result = VFSDeleteEntry(&FS->DelStat, src, srcattr);
		}
		if ( result == NO_ERROR ){
			FopLog(FS, src, NULL, LOG_DELETE);
		}else{
			BOOL OldNoAutoClose = FS->NoAutoClose;

			FWriteErrorLogs(FS, src, T("Delete"), result);
			if ( FS->DelStat.useaction == 0 ) FS->NoAutoClose = OldNoAutoClose;
		}
		return result;
	}

	switch ( FS->opt.fop.mode ){
		case FOPMODE_SHORTCUT: // ショートカット作成
			if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);

			result = FOPMakeShortCut(src, dst, TRUE, FALSE);
			if ( result == NO_ERROR ){
				FopLog(FS, src, dst, LOG_LINK);
			}else{
				FWriteErrorLogs(FS, dst, T("Sh_cut"), result);
			}
			return result;

		case FOPMODE_LINK: // ジャンクション作成
			if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);

			result = CreateJunction(dst, src, NULL);
			if ( result == NO_ERROR ){
				FopLog(FS, src, dst, LOG_LINK);
			}else{
				FWriteErrorLogs(FS, dst, T("HLink"), result);
			}
			return result;

		case FOPMODE_SYMLINK: // シンボリックリンク作成
			if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);

			result = FopCreateSymlink(dst, src, SYMBOLIC_LINK_FLAG_DIRECTORY);
			if ( result == NO_ERROR ){
				FopLog(FS, src, dst, LOG_LINK);
			}else{
				FWriteErrorLogs(FS, dst, T("SLink"), result);
			}
			return result;
	}
	FopLog(FS, src, dst, LOG_DIR);
	srclen = tstrlen32(src);

	// 名前変更
	if ( FS->renamemode ){
		if ( !(FS->opt.fop.filter & VFSFOP_FILTER_NODIRFILTER) ){
			if ( IsTrue(FS->testmode) ) return TestDest(FS, src, dst);
			return DlgDirRename(FS, src, dst);
		}
		// ↑ここまでがディレクトリそのものをいじる処理
		// ↓以下はディレクトリ内をいじる処理
	}else{													// 階層チェック
		DWORD len;

		if ( !tstrnicmp(src, dst, srclen) ){
			if ( dst[srclen] == '\0' ){
				return DlgDirRename(FS, src, dst);
			}else if ( dst[srclen] == '\\' ){ // 処理先の方が深い
				return ERROR_ALREADY_EXISTS;
			}
		}
		if ( !GetShortPathName(src, buf, TSIZEOF(buf)) )   tstrcpy(buf, src);
		if ( !GetShortPathName(dst, dbuf, TSIZEOF(dbuf)) ) tstrcpy(dbuf, dst);
		len = tstrlen32(buf);
		if ( tstrnicmp(buf, dbuf, len) == 0 ){
			if ( dbuf[len] == '\0' ){
				return DlgDirRename(FS, src, dst);
			}else if ( dbuf[len] == '\\' ){ // 処理先の方が深い
				return ERROR_ALREADY_EXISTS;
			}
		}
	}
	if ( (FS->testmode == FALSE) && (FS->renamemode == FALSE) ){
		ERRORCODE mderror;

		if ( CheckSaveDriveMove(&FS->opt, src, dst) ){ // 移動
			if ( IsTrue(MoveFileL(src, dst)) ) return NO_ERROR;
			result = GetLastError();
			if ( ( result != ERROR_ALREADY_EXISTS    ) &&
				 ( result != ERROR_ACCESS_DENIED     ) &&
				 ( result != ERROR_NOT_SAME_DEVICE   ) && // FS->opt.move で判定できなかったとき
				 ( result != ERROR_SHARING_VIOLATION ) ){
				if ( ((result == ERROR_INVALID_NAME) && tstrchr(src, '?')) &&
					  (FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR) ){
					FS->progs.info.LEskips++;
					FWriteErrorLogs(FS, src, T("Move"), result);
					return NO_ERROR;
				}
				return result;
			}
		}

		if ( (srcattr & FILE_ATTRIBUTE_REPARSE_POINT) &&
				 (FS->opt.fop.mode != FOPMODE_DELETE) ){
			result = FileOperationReparse(FS, src, dst, srcattr);
			if ( result != ERROR_MORE_DATA ) return result;
		}
														// ディレクトリ作成
		if ( (FS->opt.fop.mode != FOPMODE_DELETE) ||
			 (FS->opt.fop.flags & VFSFOP_OPTFLAG_BACKUPOLD) ){
			if ( !(srcattr & FILE_ATTRIBUTE_REPARSE_POINT) ){
				mderror = MakeDirectories(dst, src);
			}else{ // リパースポイントの時は、リパースポイントそのものをコピーしないようにする
				mderror = MakeDirectories(dst, NULL);
			}
			if ( mderror == NO_ERROR ){
				FopLog(FS, dst, NULL, LOG_MAKEDIR);
			}else if ( mderror == ERROR_FILE_NOT_FOUND ){
				FWriteErrorLogs(FS, src, T("Destdir create"), mderror);
				return NO_ERROR;
			}else if ( (mderror != ERROR_ALREADY_EXISTS) &&
					   (mderror != ERROR_INVALID_NAME) ){
				// エントリが存在していないことを確認する
				if ( GetFileAttributesL(dst) == BADATTR ) return mderror;
			}
			if ( (mderror != ERROR_ALREADY_EXISTS) ||
				 ((FS->opt.fop.AtrMask & 7) != 7) ){
				DWORD attr;

				attr = (srcattr & (FS->opt.fop.AtrMask | 0xffffffd8)) |
					(FS->opt.fop.AtrFlag & 0x07);
				SetFileAttributesL(dst, attr);
			}
		}
	}
														// 検索
	if ( (srclen + 5) >= VFPS ){
		FWriteErrorLogs(FS, src, T("Srcdir"), ERROR_FILENAME_EXCED_RANGE);
		return ERROR_FILENAME_EXCED_RANGE;
	}
	CatPath(buf, (TCHAR *)src, WildCard_All);
	hFF = FindFirstFileL(buf, &find);
	if ( hFF == INVALID_HANDLE_VALUE ){
		result = GetLastError();
		FWriteErrorLogs(FS, src, T("Srcdir"), result);
		if ( ((result == ERROR_ACCESS_DENIED)	|| // アクセス権がないなど
			  (result == ERROR_FILE_NOT_FOUND)	|| // ファイルが全くない
			  (result == ERROR_PATH_NOT_FOUND)	|| // パスが読めない
			  (result == ERROR_INVALID_PARAMETER )	|| // アクセス権がない?
			  (result == ERROR_SHARING_VIOLATION ) ) && // 共有違反
			  (FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR) ){
			FS->progs.info.LEskips++;
			result = NO_ERROR;
		}
		return result;
	}

	if ( FS->opt.fop.mode == FOPMODE_MIRROR ){
		result = GetFFList(&thDestDirList, dst);
		if ( result != NO_ERROR ){
			FindClose(hFF);
			return result;
		}
	}

	result = NO_ERROR;
	delay_dest = NULL;
	dstlen = tstrlen32(dst);

	while( result == NO_ERROR ){
		if ( !IsRelativeDirectory(find.cFileName) ){
			DWORD len;

			// ミラー時は、一覧と照合して,該当するなら一覧から消去
			if ( FS->opt.fop.mode == FOPMODE_MIRROR ){
				WIN32_FIND_DATA *ff, *ffmax;

				ffmax = (WIN32_FIND_DATA *)(thDestDirList.bottom + thDestDirList.top);
				for ( ff = (WIN32_FIND_DATA *)thDestDirList.bottom ; ff < ffmax ; ff++ ){
					// ファイル名が一致したときの処理
					if ( !tstricmp(find.cFileName, ff->cFileName) ){
						ff->cFileName[0] = '\0'; // 一致したので対象外にする
						// 名前は同じだけど、ディレクトリとファイルだった
						if ( ((find.dwFileAttributes ^ ff->dwFileAttributes) &
						   (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_LABEL))){
							wsprintf(dbuf, T("Unmatch\t%s\r\n"), find.cFileName);
							FWriteLogMsg(FS, dbuf);
							if ( FS->testmode == FALSE ){
								CatPath(dbuf, dst, find.cFileName);
								result = BackupFile(FS, dbuf);
							}
						}
						break;
					}
				}
			}

			len = tstrlen32(find.cFileName);
			if ( (srclen + len + 3) >= VFPS ){
				result = ERROR_FILENAME_EXCED_RANGE;
				break;
			}
			if ( (dstlen + len + 3) >= VFPS ){
				result = ERROR_FILENAME_EXCED_RANGE;
				break;
			}
			if ( result != NO_ERROR ) break;

			CatPath(buf, (TCHAR *)src, find.cFileName);
			CatPath(dbuf, dst, find.cFileName);
			FS->progs.srcpath = buf;

			if ( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				if ( !(FS->opt.fop.filter & VFSFOP_FILTER_NODIRFILTER) ){
					result = LFNfilter(&FS->opt, dbuf);
					if ( result != NO_ERROR ) break;
				}
				if ( IsTrue(FS->flat) ) tstrcpy(dbuf, dst);
				TinyDisplayProgress(FS);

				len = tstrlen32(dbuf);
				if ( srclen < len ) len = srclen;
				if ( ( (src[len] == '\0')  || (src[len] == '\\')  ) &&
					 ( (dbuf[len] == '\0') || (dbuf[len] == '\\') ) &&
					 (tstrnicmp(src, dbuf, len) == 0)
				){
					// パスが共通しているため、
					// 処理完了前に、コピーした物が混じってしまい、
					// 再度処理する恐れがあるので後回しにする
					// 例 \dir\dir\dir を \dir\dir に移動
					delay_dest = HeapAlloc(DLLheap, 0, sizeof(DELAYINFO));
					delay_dest->attr = find.dwFileAttributes;
					tstrcpy(delay_dest->src, buf);
					tstrcpy(delay_dest->dst, dbuf);
				}else{
					result = DlgCopyDir(FS, buf, dbuf, find.dwFileAttributes);
					if ( result == NO_ERROR ) FopLog(FS, src, dst, LOG_DIR);
				}
			}else{
				if ( !(FS->opt.fop.filter & VFSFOP_FILTER_NOFILEFILTER) ){
					result = LFNfilter(&FS->opt, dbuf);
					if ( result != NO_ERROR ) break;
				}else{
					//名前変更モード && ファイル名変更無し なので処理をスキップ
					if ( IsTrue(FS->renamemode) ) goto skipfileaction;
				}
				if ( IsTrue(FS->testmode) ){
					result = TestDest(FS, buf, dbuf);
				}else{
					result = DlgCopyFile(FS, buf, dbuf, find.dwFileAttributes);
				}
				skipfileaction: ;
			}
			if ( result != NO_ERROR ) break;
		}
		if ( FindNextFile(hFF, &find) == FALSE ){
			result = GetLastError();
			if ( result == ERROR_NO_MORE_FILES ){
				result = NO_ERROR;
				break;
			}
		}
	}
	FindClose(hFF);
	if ( delay_dest != NULL ){ // 後回しにした処理を再開する
		if ( result == NO_ERROR ){
			result = DlgCopyDir(FS, delay_dest->src, delay_dest->dst, delay_dest->attr);
			if ( result == NO_ERROR ) FopLog(FS, src, dst, LOG_DIR);
		}
		HeapFree(DLLheap, 0, delay_dest);
	}
	if ( (FS->testmode == FALSE) && (FS->renamemode == FALSE) ){
		if ( (result == NO_ERROR) && (FS->opt.security != SECURITY_FLAG_NONE)){
			result = CopySecurity(FS, src, dst);
		}
										// 移動ならディレクトリ削除
		if ( (result == NO_ERROR) && (FS->opt.fop.mode == FOPMODE_MOVE) ){
			if ( srcattr & FILE_ATTRIBUTE_READONLY ){
				SetFileAttributesL(src, FILE_ATTRIBUTE_NORMAL);
			}
			if ( RemoveDirectoryL(src) == FALSE ){
				if ( delay_dest == NULL ){
					if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
						ERRORCODE delerr = GetLastError();

						if ( delerr != ERROR_DIR_NOT_EMPTY ){
							FS->progs.info.LEskips++;
							FWriteErrorLogs(FS, src, T("Deldir"), PPERROR_GETLASTERROR);
						}
					}else{
						result = GetLastError();
					}
				}
			}else{
				SHChangeNotify(SHCNE_RMDIR, SHCNF_PATH, src, NULL);
			}
		}
	}
	if ( (result == NO_ERROR) && (FS->opt.fop.mode == FOPMODE_MIRROR) ){
		WIN32_FIND_DATA *ff, *ffmax;

		ffmax = (WIN32_FIND_DATA *)(thDestDirList.bottom + thDestDirList.top);
		for ( ff = (WIN32_FIND_DATA *)thDestDirList.bottom ; ff < ffmax ; ff++ ){
			if ( ff->cFileName[0] != '\0' ){
				if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_LOGWINDOW ){
					wsprintf(dbuf, T("Delete\t%s\r\n"), ff->cFileName);
					FWriteLogMsg(FS, dbuf);
				}
				if ( FS->testmode == FALSE ){
					CatPath(dbuf, dst, ff->cFileName);
					if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_BACKUPOLD ){
						result = BackupFile(FS, dbuf);
					}else{
						if ( ff->dwFileAttributes & FILE_ATTRIBUTE_READONLY ){
							SetFileAttributesL(dbuf, FILE_ATTRIBUTE_NORMAL);
						}
						if ( ff->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
							if ( DeleteDirectories(dbuf, TRUE) == FALSE ){
								FWriteErrorLogs(FS, dbuf, T("Deldir"), PPERROR_GETLASTERROR);
							}
						}else{
							if ( DeleteFileL(dbuf) == FALSE ){
								FWriteErrorLogs(FS, dbuf, T("Delete"), PPERROR_GETLASTERROR);
							}
						}
					}
				}
			}
		}
		ThFree(&thDestDirList);
	}
	FS->progs.srcpath = NULL;
	return result;
}
