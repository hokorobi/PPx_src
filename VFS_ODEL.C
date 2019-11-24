/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作, *file delete
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_FOP.H"
#include "VFS_STRU.H"
#pragma hdrstop

#define dispNwait 500
int VFSDeleteEntryError(DELETESTATUS *dinfo, const TCHAR *path, ERRORCODE error);

DWORD_PTR USECDECL FopDeleteInfoFunc(PPXAPPINFO *ppxa, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_SETSTATLINE: {
			HWND hWnd;
			TCHAR buf[200];

			hWnd = GetDlgItem(ppxa->hWnd, IDS_FOP_PROGRESS);

			wsprintf(buf, T("\x1b\x64%c %d files"),
					((DELETESTATUS *)uptr)->count % 10 + 1,
					((DELETESTATUS *)uptr)->count);
			InvalidateRect(hWnd, NULL, FALSE);
			SetWindowText(hWnd, buf);
			UpdateWindow(hWnd);
			break;
		}
		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 0;
}

BOOL BreakCheckF(DELETESTATUS *dinfo)
{
	MSG msg;
	DWORD NewTime;

	NewTime = GetTickCount();
	if ( NewTime > (dinfo->OldTime + dispNwait) ){
		dinfo->OldTime = NewTime;
		dinfo->info->Function(dinfo->info, PPXCMDID_SETSTATLINE, dinfo);

		SetTaskBarButtonProgress(dinfo->info->hWnd, TBPF_INDETERMINATE, 0);
	}

	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( (msg.message == WM_RBUTTONUP) ||
			 ((msg.message == WM_KEYDOWN) &&
			 (((int)msg.wParam == VK_ESCAPE)||((int)msg.wParam == VK_PAUSE)))){
			if ( PMessageBox(dinfo->info->hWnd, MES_QABO, T("Break check"),
						MB_APPLMODAL | MB_OKCANCEL | MB_DEFBUTTON1 |
						MB_ICONQUESTION) == IDOK){
				return TRUE;
			}
		}
		if (((msg.message >= WM_MOUSEFIRST) && (msg.message <= WM_MOUSELAST))||
			((msg.message >= WM_KEYFIRST) && (msg.message <= WM_KEYLAST)) ||
			(msg.message == WM_COMMAND) || (msg.message == WM_PPXCOMMAND) ){
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return FALSE;
}

void FDelLog(DELETESTATUS *dinfo, const TCHAR *path, ERRORCODE result)
{
	TCHAR buf[CMDLINESIZE], *p;

	p = buf + wsprintf(buf, T("Delete\t%s"), path);
	if ( result != NO_ERROR ){
		*p++ = ' ';
		*p++ = ':';
		*p++ = ' ';
		*p = '\0';
		PPErrorMsg(p, result);
	}
	SendMessage(dinfo->info->hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_WINDDOWLOG, 1), (LPARAM)buf);
}

// 指定ディレクトリ内を削除する
ERRORCODE FopDeleteFileDir(DELETESTATUS *dinfo, const TCHAR *path, DWORD attr)
{
	WIN32_FIND_DATA ff;
	HANDLE hFF;
	TCHAR *lastp;
	ERRORCODE result = NO_ERROR;
	TCHAR src[VFPS], srcfile[CMDLINESIZE];

	if ( attr & FILE_ATTRIBUTE_REPARSE_POINT ){
		int r;

		if ( dinfo->flags & (VFSDE_SYMDEL_SYM | VFSDE_SYMDEL_FILE) ){
			if ( dinfo->flags & VFSDE_SYMDEL_SYM ){
				r = IDYES;
			}else{
				r = IDNO;
			}
		}else{
			GetReparsePath(path, src);
			wsprintf(srcfile, T("%s ( %s )"), path, src);

			SetJobTask(dinfo->info->hWnd, JOBSTATE_PAUSE);
			r = PMessageBox(dinfo->info->hWnd, MES_QDRP, srcfile, MB_APPLMODAL |
					MB_YESNOCANCEL | MB_PPX_ALLCHECKBOX | MB_DEFBUTTON3 |
					MB_ICONQUESTION);
			SetJobTask(dinfo->info->hWnd, JOBSTATE_UNPAUSE);

			if ( r > 0 && (r & ID_PPX_CHECKED) ){
				resetflag(r, ID_PPX_CHECKED);
				if ( r == IDYES ){
					setflag(dinfo->flags, VFSDE_SYMDEL_SYM | VFSDE_SYMDEL_FILE);
				}else{
					setflag(dinfo->flags, VFSDE_SYMDEL_FILE);
				}
			}

		}
		if ( r == IDYES ){
			BOOL boolresult;

			if ( attr & FILE_ATTRIBUTE_READONLY ){
				SetFileAttributesL(path, FILE_ATTRIBUTE_NORMAL);
			}
			if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
				boolresult = RemoveDirectoryL(path);
			}else{
				boolresult = DeleteFileL(path);
			}
			return IsTrue(boolresult) ? ERROR_NO_MORE_ITEMS : GetLastError();
		}
		if ( r != IDNO ) return ERROR_CANCELLED;
	}
	// 通常削除

	tstrcpy(src, path);
	lastp = src + tstrlen(src);
	tstrcpy(lastp, T("\\*"));
	hFF = FindFirstFileL(src, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ){
		if ( !memcmp(path, StrAux, StrAuxSize) ||
			 !memcmp(path, StrFtp, StrFtpSize) ){
			return NO_ERROR;
		}
		result = GetLastError();
		if ( !dinfo->useaction ){
			switch ( VFSDeleteEntryError(dinfo, path, result) ){
				case IDOK:	// 自動再試行で削除成功
					if ( dinfo->flags & VFSDE_REPORT ){
						FDelLog(dinfo, path, NO_ERROR);
					}
					return NO_ERROR;

				case IDB_QDADDLIST:	// 遅延リストに追加
					if ( NO_ERROR != InsertCustTable(T("_Delayed"), T("delete"),
							0x7fffffff, path, TSTRSIZE(path)) ){
						PMessageBox(dinfo->info->hWnd, MES_ECFL, path,
							MB_APPLMODAL | MB_OK | MB_ICONEXCLAMATION);
					}
//					IDIGNORE へ
				case IDRETRY:	// 再試行
				case IDIGNORE:	// 無視→削除したものとする
					return ERROR_NO_MORE_ITEMS; // dir 自身の削除をスキップ
//				case IDCANCEL:	// 中止→defaultと同じ動作
//				case IDABORT:	// 中止→defaultと同じ動作
				default:
					return ERROR_CANCELLED;	// 中止
			}
		}
		return result;
	}
	*lastp = '\0';

	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		if (	!(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				(dinfo->flags & VFSDE_WARNFILEINDIR) ){
			TCHAR mes[VFPS * 2];
			int mesresult;

			wsprintf(mes, MessageText(MES_QEFD), path);
			SetJobTask(dinfo->info->hWnd, JOBSTATE_PAUSE);
			mesresult = PMessageBox(dinfo->info->hWnd, mes, T("Delete Directory"),
				MB_APPLMODAL | MB_OKCANCEL | MB_DEFBUTTON1 | MB_ICONQUESTION);
			SetJobTask(dinfo->info->hWnd, JOBSTATE_UNPAUSE);
			if ( mesresult != IDOK ){
				result = ERROR_CANCELLED;
				break;
			}
			resetflag(dinfo->flags, VFSDE_WARNFILEINDIR);
		}
		VFSFullPath(srcfile, ff.cFileName, src);
		result = VFSDeleteEntry(dinfo, srcfile, ff.dwFileAttributes);
		if ( result != NO_ERROR ){
			// LFNがだめならSFNで試す
			if ( (result == ERROR_PATH_NOT_FOUND) && ff.cAlternateFileName[0]){
				VFSFullPath(srcfile, ff.cAlternateFileName, src);
				result = VFSDeleteEntry(dinfo, srcfile, ff.dwFileAttributes);
				if ( result != NO_ERROR ) break;
			}else{
				break;
			}
		}
	}while( IsTrue(FindNextFile(hFF, &ff)) );
	FindClose(hFF);
	return result;
}

#pragma argsused
VOID CALLBACK AutoRetryDelete(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	TCHAR path[VFPS];
	DWORD attr;
	BOOL result;

	UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);

	path[0] = '\0';
	GetWindowText(hWnd, path, VFPS);
	attr = GetFileAttributesL(path);
	if ( attr == BADATTR ){
		ERRORCODE error = GetLastError();

		if ( error == ERROR_FILE_NOT_FOUND ){
			KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
			PostMessage(hWnd, WM_COMMAND, IDOK, 0);
		}
		return;
	}
	if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
		result = RemoveDirectoryL(path);
	}else{
		result = DeleteFileL(path);
	}
	if ( IsTrue(result) ){
		KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		PostMessage(hWnd, WM_COMMAND, IDOK, 0);
	}else{
		ERRORCODE error = GetLastError();

		if ( (error != ERROR_ACCESS_DENIED) &&
			 (error != ERROR_SHARING_VIOLATION)
		){ // エラーの種類が変わったので再試行を中止
			KillTimer(hWnd, TIMERID_MSGBOX_AUTORETRY);
		}
	}
}

int VFSDeleteEntryError(DELETESTATUS *dinfo, const TCHAR *path, ERRORCODE error)
{
	TCHAR msgbuf[CMDLINESIZE];
	MESSAGEDATA md;
	int result;

	md.title = path;
	md.text = msgbuf;
	md.style = MB_PPX_ADDABORTRETRYIGNORE | MB_PPX_ALLCHECKBOX | MB_DEFBUTTON2 | MB_ICONEXCLAMATION;

	msgbuf[0] = '\0';
	if ( (error == ERROR_ACCESS_DENIED) ||
		 (error == ERROR_SHARING_VIOLATION) ){
		GetAccessApplications(path, msgbuf);
		setflag(md.style, MB_PPX_AUTORETRY);
		md.autoretryfunc = AutoRetryDelete;
	}
	if ( msgbuf[0] == '\0' ) PPErrorMsg(msgbuf, error);

	SetJobTask(dinfo->info->hWnd, JOBSTATE_ERROR);
	result = (int)DialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_NULL),
			dinfo->info->hWnd, MessageBoxDxProc, (LPARAM)&md);
	SetJobTask(dinfo->info->hWnd, JOBSTATE_DEERROR);
	if ( result > 0 ){
		if ( result & ID_PPX_CHECKED ){
			resetflag(result, ID_PPX_CHECKED);
			if ( result != IDRETRY ) dinfo->useaction = result;
		}
	}else{
		result = IDCANCEL;
	}
	return result;
}

VFSDLL ERRORCODE PPXAPI VFSDeleteEntry(DELETESTATUS *dinfo, const TCHAR *path, DWORD attr)
{
	int result;

	if ( attr & dinfo->warnattr ){
		SetJobTask(dinfo->info->hWnd, JOBSTATE_PAUSE);
		result = PMessageBox(dinfo->info->hWnd, MES_QDFA, path,
			MB_APPLMODAL | MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION);
		SetJobTask(dinfo->info->hWnd, JOBSTATE_UNPAUSE);
		if ( result != IDYES ) return ERROR_CANCELLED;
		resetflag(dinfo->warnattr, attr);
	}

	if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
		ERRORCODE error;

		error = FopDeleteFileDir(dinfo, path, attr);
		if ( error != NO_ERROR ){
			if ( error == ERROR_NO_MORE_ITEMS ){ // symlink 削除済み / 無視
				return NO_ERROR;
			}
			if ( memcmp(path, StrFtp, StrFtpSize) &&
				 (dinfo->flags & VFSDE_REPORT) ){
				FDelLog(dinfo, path, error);
			}
			return error;
		}
	}

	dinfo->path = path;
	if ( IsTrue(BreakCheckF(dinfo)) ) return ERROR_CANCELLED;
	for ( ; ; ){
		ERRORCODE error;
										// 削除を行う -------------------------
		if ( attr & FILE_ATTRIBUTE_READONLY ){
			SetFileAttributesL(path, FILE_ATTRIBUTE_NORMAL);
		}
		if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
			if ( IsTrue(RemoveDirectoryL(path)) ){
				SHChangeNotify(SHCNE_RMDIR, SHCNF_PATH, path, NULL);
				if ( dinfo->flags & VFSDE_REPORT ){
					FDelLog(dinfo, path, NO_ERROR);
				}
				return NO_ERROR;
			}
		}else{
			if ( IsTrue(DeleteFileL(path)) ){
				dinfo->count++;
				if ( dinfo->flags & VFSDE_REPORT ){
					FDelLog(dinfo, path, NO_ERROR);
				}
				return NO_ERROR;
			}
		}
										// エラーの表示と、対処の選択 ---------
		error = GetLastError();

		if ( (error == ERROR_PATH_NOT_FOUND) && (path[0] == '#') ){
			POINT pos;

			GetCursorPos(&pos);
			if ( IsTrue(VFSSHContextMenu(GetFocus(), &pos, path, NilStr, T("delete"))) ){
				return NO_ERROR;
			}
		}

		if ( dinfo->flags & VFSDE_REPORT ){
			FDelLog(dinfo, path, error);
		}

		if ( (attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
			 (GetFileAttributesL(path) == BADATTR) ){
			return NO_ERROR;
		}

		if ( error == ERROR_DIR_NOT_EMPTY ){
			dinfo->noempty = TRUE;
			return NO_ERROR;
		}

		if ( !memcmp(path, StrAux, StrAuxSize) ){
			error = FileOperationAux(
					(attr & FILE_ATTRIBUTE_DIRECTORY) ? T("deldir") : T("del"),
					path, NilStr, NilStr);
			if ( error == NO_ERROR ) return NO_ERROR;
		}

		if ( !memcmp(path, StrFtp, StrFtpSize) ){
			error = DeleteFtpEntry(path, attr);
			if ( error == NO_ERROR ) return NO_ERROR;
		}
		if ( dinfo->useaction ){
			result = dinfo->useaction;
		}else{
			result = VFSDeleteEntryError(dinfo, path, error);
		}
										// 対処 -------------------------------
		switch ( result ){
			case IDOK:	// 自動再試行で削除成功
				if ( !(attr & FILE_ATTRIBUTE_DIRECTORY) ) dinfo->count++;
				if ( dinfo->flags & VFSDE_REPORT ){
					FDelLog(dinfo, path, NO_ERROR);
				}
				return NO_ERROR;

			case IDRETRY:	// 再試行
				continue;
			case IDB_QDADDLIST:	// 遅延リストに追加
				if ( NO_ERROR != InsertCustTable(T("_Delayed"), T("delete"),
						0x7fffffff, path, TSTRSIZE(path)) ){
					PMessageBox(dinfo->info->hWnd, MES_ECFL, path,
						MB_APPLMODAL | MB_OK | MB_ICONEXCLAMATION);
				}
//				IDIGNORE へ
			case IDIGNORE:	// 無視→削除したものとする
				return NO_ERROR;
//			case IDCANCEL:	// 中止→defaultと同じ動作
//			case IDABORT:	// 中止→defaultと同じ動作
			default:
				return ERROR_CANCELLED;	// 中止
		}
	}
}
