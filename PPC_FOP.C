/*-----------------------------------------------------------------------------
	Paper Plane cUI												ファイル操作
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <winioctl.h>
#include <shlobj.h>
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#pragma hdrstop

const TCHAR NewDirName[] = MES_NWDN;
const TCHAR NewFileName[] = MES_NWFN;
const TCHAR RRenTitle[] = T("Rename\t");
const TCHAR RRenedTitle[] = T(" ->\t");
const TCHAR PPcFileOperationThreadName[] = T("PPc fileop.");
const TCHAR PPcDoFileOperationThreadName[] = T("PPc drop/paste");
const TCHAR ShellNewString[] = T("ShellNew");
const TCHAR ShellStatePath[] = T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");
const TCHAR ShellStateName[] = T("ShellState");
const TCHAR StrNewFileName[] = T("FileName");
const TCHAR StrNewData[] = T("Data");
const TCHAR QueryRecyclebin[] = MES_QDRD;
const UINT deletemesboxstyle[] = { 0, MB_QYES, MB_QNO };

enum {
	NEWCMD_DIR = CRID_NEWENTRY, NEWCMD_FILE, NEWCMD_LISTFILE, NEWCMD_REG
};

#define JPCSR 0	// 終了後カーソル移動。終わった後にデッドロックするか検証

#define RECYCLEBINOFFSET 4
#define DELETEDIALOG B2

typedef struct {
	PPXAPPINFO info;
	PPC_APPINFO *cinfo;
} PPCINFOEXT;

typedef struct {
	PPCINFOEXT infoext;
	VFSFILEOPERATION fileop;
	TCHAR optionstring[];
} FOPTHREADSTRUCT;

typedef struct {
	PPCINFOEXT infoext;
	VFSFILEOPERATION fileop;
	TM_struct files;
	TCHAR dest[VFPS];
} PPCDOFILEOPERATIONSTRUCT;


void PPcDeleteFile(PPC_APPINFO *cinfo, DWORD *X_wdel);

#define FO__NEWFOLDER	5 // 新規作成
#ifndef SFOERROR_OK
	#define SFOERROR_OK		0
	#define SFOERROR_SHFO	1
	#define SFOERROR_IFO	2
	#define SFOERROR_BADNEW	3
#endif

DefineWinAPI(int, SHFileOperation, (LPSHFILEOPSTRUCT)) = NULL;

#define PPcEnumInfoFunc(func, str, work) ((work)->buffer = str, cinfo->info.Function(&cinfo->info, func, work))

int SxFileOperation(PPC_APPINFO *cinfo, SHFILEOPSTRUCT *fileop)
{
	xIFileOperation *ifo;
	HMODULE hShell32;
	HRESULT result = S_OK;
	DefineWinAPI(HRESULT, SHCreateItemFromIDList, (LPITEMIDLIST, REFIID, void **));
	DefineWinAPI(HRESULT, SHCreateItemFromParsingName, (PCWSTR, IBindCtx *, REFIID, void **));

	const TCHAR *nameptr;
	xIShellItem *destShellItem = NULL;
	LPITEMIDLIST pidl;
#ifndef UNICODE
	WCHAR srcnameW[VFPS];
#endif
	PPXCMDENUMSTRUCT enumwork;
	TCHAR buf[VFPS];
	BOOL freemem = FALSE;

	fileop->hwnd = cinfo->info.hWnd;
	fileop->fAnyOperationsAborted	= TRUE;
	fileop->hNameMappings			= NULL;

	hShell32 = GetModuleHandle(StrShell32DLL);
	if ( FAILED(CoCreateInstance(&XCLSID_IFileOperation, NULL, CLSCTX_ALL,
			&XIID_IFileOperation, (LPVOID *)&ifo)) ){
		if ( fileop->wFunc == FO__NEWFOLDER ) return SFOERROR_BADNEW;
		if ( DSHFileOperation == NULL ){
			GETDLLPROCT(hShell32, SHFileOperation);
			if ( DSHFileOperation == NULL ) return SFOERROR_SHFO;
		}
		if ( fileop->pFrom != NULL ){
			return DSHFileOperation(fileop) ? SFOERROR_SHFO : SFOERROR_OK;
		}else{
			int fresult;
			fileop->pFrom = GetFiles(cinfo, GETFILES_FULLPATH | GETFILES_REALPATH);
			if ( fileop->pFrom == NULL ) return SFOERROR_SHFO;
			fresult = DSHFileOperation(fileop) ? SFOERROR_SHFO : SFOERROR_OK;

			HeapFree(hProcessHeap, 0, (void *)fileop->pFrom);
			return fresult;
		}
	}

	ifo->lpVtbl->SetOwnerWindow(ifo, fileop->hwnd);
	ifo->lpVtbl->SetOperationFlags(ifo, fileop->fFlags);

	GETDLLPROC(hShell32, SHCreateItemFromIDList);
	GETDLLPROC(hShell32, SHCreateItemFromParsingName);

	if ( fileop->pTo != NULL ){
#ifdef UNICODE
		#define destdirW fileop->pTo
#else
		AnsiToUnicode(fileop->pTo, srcnameW, VFPS);
		#define destdirW srcnameW
#endif
		if ( FAILED(DSHCreateItemFromParsingName(destdirW, NULL, &XIID_IShellItem, (void**)&destShellItem)) ){
			destShellItem = NULL;
			pidl = PathToPidl(fileop->pTo);
			if ( pidl != NULL ){
				if ( FAILED(DSHCreateItemFromIDList(pidl, &XIID_IShellItem, (void**)&destShellItem)) ){
					destShellItem = NULL;
				}
				FreePIDL(pidl);
			}
		}
		#undef destnameW
	}

	if ( fileop->wFunc == FO__NEWFOLDER ){
#ifdef UNICODE
		#define srcnameW fileop->pFrom
#else
		AnsiToUnicode(fileop->pFrom, srcnameW, VFPS);
#endif
		result = ifo->lpVtbl->NewItem(ifo, destShellItem, FILE_ATTRIBUTE_DIRECTORY, srcnameW, NULL, NULL);
	}else{
		// 対象を格納する
		nameptr = fileop->pFrom;
		if ( nameptr == NULL ){
			if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
				PPcEnumInfoFunc(PPXCMDID_STARTENUM, buf, &enumwork);
			}else{
				nameptr = GetFiles(cinfo, GETFILES_FULLPATH | GETFILES_REALPATH);
				if ( nameptr != NULL ){
					freemem = TRUE;
				}else{
					enumwork.enumID = CELLENUMID_END;
				}
			}
		}
		for (;;){
			xIShellItem *srcShellItem;

			if ( nameptr != NULL ){ // file
				if ( *nameptr == '\0' ) break;
#ifdef UNICODE
				result = DSHCreateItemFromParsingName(nameptr, NULL, &XIID_IShellItem, (void**)&srcShellItem);
#else
				AnsiToUnicode(nameptr, srcnameW, VFPS);
				result = DSHCreateItemFromParsingName(srcnameW, NULL, &XIID_IShellItem, (void**)&srcShellItem);
#endif
				if ( FAILED(result) ){
					PPErrorBox(fileop->hwnd, NULL, (ERRORCODE)result);
					break;
				}
			}else{ // shn
				PPcEnumInfoFunc('C', buf, &enumwork);
//				if ( *buf == '\0' ) break;
				if ( IsParentDir(buf) == FALSE ){
					VFSFullPath(NULL, buf, cinfo->path);
				}
				pidl = PathToPidl(buf);
				if ( pidl == NULL ){
					PPErrorBox(fileop->hwnd, NULL, ERROR_ACCESS_DENIED);
					break;
				}
				if ( FAILED(DSHCreateItemFromIDList(pidl, &XIID_IShellItem, (void**)&srcShellItem)) ){
					PPErrorBox(fileop->hwnd, NULL, ERROR_INVALID_FUNCTION);
					break;
				}
				FreePIDL(pidl);
			}

			if ( (fileop->wFunc == FO_COPY) || (fileop->wFunc == FO_MOVE) ){
#if 0 // コピー先名を変えるときに使用する
				const TCHAR *srcname;

				srcname = FindLastEntryPoint(nameptr ? nameptr : buf);
#ifndef UNICODE
				AnsiToUnicode(srcname, srcnameW, VFPS);
#endif
#endif
				if ( destShellItem == NULL ){
					result = E_FAIL;
					break;
				}
				if ( fileop->wFunc == FO_COPY ){
					result = ifo->lpVtbl->CopyItem(ifo, srcShellItem,
//							destShellItem, srcnameW, NULL);
							destShellItem, NULL, NULL);
				}
				if ( fileop->wFunc == FO_MOVE ){
					result = ifo->lpVtbl->MoveItem(ifo, srcShellItem,
//							destShellItem, srcnameW, NULL);
							destShellItem, NULL, NULL);
				}
			}else if ( fileop->wFunc == FO_DELETE ){
				result = ifo->lpVtbl->DeleteItem(ifo, srcShellItem, NULL);
			}
			srcShellItem->lpVtbl->Release(srcShellItem);
			if ( FAILED(result) ) break;
			if ( nameptr != NULL ){
				nameptr += tstrlen(nameptr) + 1;
			}else{
				if ( PPcEnumInfoFunc(PPXCMDID_NEXTENUM, buf, &enumwork) == 0 ) break;
			}
		}
		if ( nameptr == NULL ) PPcEnumInfoFunc(PPXCMDID_ENDENUM, buf, &enumwork);
	}

	if ( SUCCEEDED(result) ){ // 実行
		result = ifo->lpVtbl->PerformOperations(ifo);
	}
	if ( destShellItem != NULL ) destShellItem->lpVtbl->Release(destShellItem);
	ifo->lpVtbl->Release(ifo);
	if ( freemem ) HeapFree(hProcessHeap, 0, (void *)fileop->pFrom);
	return SUCCEEDED(result) ? SFOERROR_OK : SFOERROR_IFO;
}

//============================================= ファイル操作関係 - コピー・移動
void SHFileOperationFix(PPC_APPINFO *cinfo)
{
	// SHFileOperation 実行中にPPc窓が閉じた場合、改めて終了させる
	// これをしないと、PPc窓が完全に閉じない
	if ( !IsWindow(cinfo->info.hWnd) ){
		if ( !cinfo->combo || !IsWindow(cinfo->hComboWnd) ){
			RequestDestroyFlag = 1;
			if ( X_MultiThread ){
				PostQuitMessage(EXIT_SUCCESS);
			}else{
				PostMessage(cinfo->info.hWnd, WM_NULL, 0, 0);
			}
		}
	}
}

DWORD GetFopOption(const TCHAR *action)
{
	VFSFOP_OPTIONS fop;

	fop.flags = 123;
	GetCustTable(T("X_fopt"), action, &fop, sizeof(VFSFOP_OPTIONS));
	return fop.flags;
}

//-----------------------------------------------------------------------------
// ファイルの複写
ERRORCODE PPC_ExplorerCopy(PPC_APPINFO *cinfo, BOOL move)
{
	TCHAR dst[VFPS];
	SHFILEOPSTRUCT fileop;
	const TCHAR *modestr;
	int mode;

	modestr = !move ? StrFopCaption_Copy : StrFopCaption_Move;
	if ( (cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
		 (cinfo->e.Dtype.mode == VFSDT_UN) ||
		 ((cinfo->e.Dtype.mode != VFSDT_SHN) && IsNodirShnPath(cinfo)) ||
		 (cinfo->e.Dtype.mode == VFSDT_HTTP) ){
		if ( !move ){
			SetPopMsg(cinfo, POPMSG_MSG, MES_ECDR);
		}else{
			PPC_Unpack(cinfo, NULL);
		}
		return NO_ERROR;
	}
	GetPairVpath(cinfo, dst);

	if ( PPctInput(cinfo, modestr, dst, TSIZEOF(dst),
			PPXH_DIR_R, PPXH_DIR) <= 0 ){
		return ERROR_CANCELLED;
	}

	VFSFixPath(NULL, dst, cinfo->path, VFSFIX_VFPS);
	mode = VFSPT_UNKNOWN;
	VFSGetDriveType(dst, &mode, NULL);

	if ( ((mode == VFSPT_DRIVE) || (mode == VFSPT_UNC)) && (GetFileAttributesL(dst) == BADATTR) ){
		ERRORCODE result;

		result = GetLastError();
		if ( result != ERROR_NOT_READY ){
			if ( !(GetFopOption(T("copy")) & VFSFOP_OPTFLAG_NOWCREATEDIR) ){
				if ( PMessageBox(cinfo->info.hWnd, MES_QCRD,
						T("File operation warning"), MB_OKCANCEL) != IDOK ){
					return ERROR_CANCELLED;
				}
			}
			result = MakeDirectories(dst, NULL);
		}
		if ( result != NO_ERROR ){
			SetPopMsg(cinfo, result, dst);
			return result;
		}
	}

	fileop.wFunc	= move ? FO_MOVE : FO_COPY;
	fileop.pFrom	= NULL;
	fileop.pTo		= dst;
	fileop.fFlags	= 0;
//	fileop.lpszProgressTitle		= MES_TFAC;
	PPxCommonCommand(NULL, !move ? JOBSTATE_FOP_COPY : JOBSTATE_FOP_MOVE, K_ADDJOBTASK);
	if ( SxFileOperation(cinfo, &fileop) == SFOERROR_OK ){
		SetRefreshAfterList(cinfo, ALST_COPYMOVE, dst[0]);
	}
	PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
	SHFileOperationFix(cinfo);
	return NO_ERROR;
}

DWORD_PTR USECDECL PPcFOPInfoFunc(PPCINFOEXT *infoext, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	if ( IsTrue(IsWindow(infoext->info.hWnd)) &&
		 (infoext->cinfo->e.INDEXDATA.p != NULL) &&
		 (infoext->cinfo->e.CELLDATA.p != NULL) ){
		return infoext->cinfo->info.Function(&infoext->cinfo->info, cmdID, uptr);
	}

	if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
	return 0;
}

DWORD WINAPI PPc_DoFileOperationThread(PPCDOFILEOPERATIONSTRUCT *ppfo)
{
	THREADSTRUCT threadstruct = {PPcDoFileOperationThreadName, XTHREAD_TERMENABLE, NULL, 0, 0};
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	PPxRegisterThread(&threadstruct);

	PPxFileOperation(NULL, &ppfo->fileop);
	if ( ppfo->infoext.cinfo != NULL ) ppfo->infoext.cinfo->Ref--;
	TM_kill(&ppfo->files);
	PPcHeapFree(ppfo);

	PostQuitMessage(EXIT_SUCCESS);
	PeekLoop();
	PPxUnRegisterThread();
	CoUninitialize();
	return NO_ERROR;
}

// Paste / Drop 用スレッド
void PPc_DoFileOperation(PPC_APPINFO *cinfo, const TCHAR *action, TMS_struct *files, const TCHAR *destdir, const TCHAR *option, DWORD flags)
{
	DWORD tmp;
	PPCDOFILEOPERATIONSTRUCT *ppfo = PPcHeapAlloc(sizeof(PPCDOFILEOPERATIONSTRUCT));

	ppfo->infoext.cinfo = cinfo;
	if ( cinfo == NULL ){
		ppfo->fileop.info = NULL;
	}else{
		ppfo->infoext.info.Function = (PPXAPPINFOFUNCTION)PPcFOPInfoFunc;
		ppfo->infoext.info.Name  = PPcDoFileOperationThreadName;
		ppfo->infoext.info.RegID = cinfo->info.RegID;
		ppfo->infoext.info.hWnd =  cinfo->info.hWnd;
		ppfo->fileop.info = &ppfo->infoext.info;
	}

	tstrcpy(ppfo->dest, destdir);
	ppfo->files = files->tm;
	ppfo->fileop.action = action;
	ppfo->fileop.src = NULL;
	ppfo->fileop.dest = ppfo->dest;
	ppfo->fileop.files	= (const TCHAR *)ppfo->files.p;
	ppfo->fileop.option	= option;
	ppfo->fileop.dtype	= VFSDT_UNKNOWN;
	ppfo->fileop.flags	= flags;
	ppfo->fileop.hReturnWnd = (cinfo != NULL) ? cinfo->info.hWnd : NULL;
	ppfo->fileop.hReadyEvent = NULL;
#ifndef WINEGCC
	if ( OSver.dwMajorVersion < 6 ){
		PPxFileOperation(NULL, &ppfo->fileop);
		TM_kill(&ppfo->files);
		PPcHeapFree(ppfo);
		return;
	}
#endif
	if ( cinfo != NULL ) cinfo->Ref++;
	CloseHandle(CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)PPc_DoFileOperationThread, ppfo, 0, &tmp));
}

DWORD WINAPI PPcFileOperationThread(FOPTHREADSTRUCT *fop)
{
	THREADSTRUCT threadstruct = {PPcFileOperationThreadName, XTHREAD_TERMENABLE, NULL, 0, 0};
#if JPCSR
	TCHAR command[VFPS];
	int work;
	COPYDATASTRUCT copydata;
	HWND hPairWnd;

	hPairWnd = GetPairWnd(fop->cinfo);
	if ( hPairWnd != NULL ){
		InitEnumMarkCell(fop->cinfo, &work);
		tstrcpy(command + 2, EnumMarkCell(fop->cinfo, &work)->f.cFileName);
	}
#endif
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	fop->infoext.info.Function = (PPXAPPINFOFUNCTION)PPcFOPInfoFunc;
	fop->infoext.info.Name  = PPcFileOperationThreadName;
	fop->infoext.info.RegID = fop->infoext.cinfo->info.RegID;
	fop->infoext.info.hWnd =  fop->infoext.cinfo->info.hWnd;
	fop->fileop.info = &fop->infoext.info;

	PPxRegisterThread(&threadstruct);
	if ( PPxFileOperation(NULL, &fop->fileop) < 0 ){
		CloseHandle(fop->fileop.hReadyEvent);
		SetPopMsg(fop->infoext.cinfo, ERROR_INVALID_FUNCTION, NULL);
	}
#if JPCSR
	if ( (hPairWnd != NULL) && IsWindow(hPairWnd) &&
		 (FindPathSeparator(command + 2) == NULL) ){
		command[0] = '%';
		command[1] = 'J';
		copydata.dwData = 'H';
		copydata.cbData = TSTRSIZE(command);
		copydata.lpData = command;
		SendMessage(hPairWnd, WM_COPYDATA, (WPARAM)hPairWnd, (LPARAM)&copydata);
	}
#endif
	if ( FALSE != IsWindow(fop->infoext.info.hWnd) ){
		fop->infoext.cinfo->TerminateThreadID = GetCurrentThreadId();
	}
	PPcHeapFree(fop);
	PostQuitMessage(EXIT_SUCCESS);
	PeekLoop();
	PPxUnRegisterThread();
	CoUninitialize();
	return NO_ERROR;
}

BOOL IsNodirShnPath(PPC_APPINFO *cinfo)
{
	DWORD attr;

	if ( cinfo->e.Dtype.mode != VFSDT_SHN ) return FALSE;
	if ( cinfo->RealPath[0] == '?' ) return TRUE; // ディレクトリでない
			// 存在しないか、ファイル
	attr = GetFileAttributes(cinfo->RealPath);
	if ( (attr != BADATTR) && (attr & FILE_ATTRIBUTE_DIRECTORY) ){
		return FALSE;
	}else{
		return TRUE;
	}
}

void PPcFileOperation(PPC_APPINFO *cinfo, const TCHAR *action, const TCHAR *destdir, const TCHAR *option)
{
	VFSFILEOPERATION fileop;
	MSG msg;

	if ( cinfo->usereentry ){
		SetPopMsg(cinfo, POPMSG_MSG, T("reentry"));
		return;
	}

	if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
		fileop.files = GetFiles(cinfo, 0);
	}else{
		fileop.files = NULL;
	}

	if ( (cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
		 (cinfo->e.Dtype.mode == VFSDT_UN) ||
		 (IsNodirShnPath(cinfo) && (fileop.files == NULL)) ||
		 (cinfo->e.Dtype.mode == VFSDT_HTTP) ){
		if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
			PPcUnpackSelectedEntry(cinfo, destdir, action);
		}else if ( action != FileOperationMode_Move ){
			PPC_Unpack(cinfo, destdir);
		}else{
			SetPopMsg(cinfo, POPMSG_MSG, StrNotSupport);
		}
		return;
	}

	cinfo->usereentry = 1;

	fileop.action	= action;
	fileop.dest		= destdir;
	fileop.option	= option;
	fileop.dtype	= cinfo->e.Dtype.mode;
	fileop.src		= cinfo->path;
	fileop.flags	= VFSFOP_FREEFILES;

	if ( fileop.dtype != VFSDT_LFILE ){
		if ( fileop.files == NULL ){
			if ( (fileop.files = GetFiles(cinfo, 0)) == NULL ){
				SetPopMsg(cinfo, POPMSG_MSG, StrNotSupport);
				cinfo->usereentry = 0;
				return;
			}
		}
	}else{
		if ( cinfo->e.Dtype.BasePath[0] != '\0' ){
			fileop.src = cinfo->e.Dtype.BasePath;
			setflag(fileop.flags, VFSFOP_USEKEEPDIR);
		}
		fileop.files = GetFilesForListfile(cinfo);
	}

	fileop.info = &cinfo->info;
	fileop.hReturnWnd = cinfo->info.hWnd;

	if ( fileop.action[0] == '!' ){
		setflag(fileop.flags, VFSFOP_AUTOSTART);
		fileop.action++;
	}
	if ( fileop.files == NULL ){
		cinfo->usereentry = 0;
		xmessage(XM_FaERRd, T("Alloc error"));
		return;
	}
#ifndef UNICODE
	if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
		if ( PPxFileOperation(NULL, &fileop) > 0 ){
			SetRefreshAfterList(cinfo, ALST_COPYMOVE, '\0');
		}
	}else
#endif
	{
		DWORD tmp;
		FOPTHREADSTRUCT *fop;

		setflag(fileop.flags, VFSFOP_NOTIFYREADY);
		fileop.hReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		{
			DWORD strsize;

			if ( option != NULL ){
				strsize = TSTRSIZE(option);
			}else{
				strsize = 0;
			}
			fop = PPcHeapAlloc(sizeof(FOPTHREADSTRUCT) + strsize);
		}
		fop->infoext.cinfo = cinfo;
		fop->fileop = fileop;
		if ( option != NULL ){
			tstrcpy(fop->optionstring, option);
			fop->fileop.option = fop->optionstring;
		}
		EnableWindow(cinfo->info.hWnd, FALSE);
		CloseHandle(CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)PPcFileOperationThread, fop, 0, &tmp));

		{	// スレッドがパラメータを正しく受け取るまで待機する
			DWORD waitresult;

			for ( ;;) {
				waitresult = MsgWaitForMultipleObjects(1, &fileop.hReadyEvent, FALSE, 5000, QS_SENDMESSAGE);
				if ( waitresult != (WAIT_OBJECT_0 + 1) ) break;

				if ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) {
					if ( msg.message == WM_QUIT ) break;
					/* if ( msg.message == WM_PAINT ) */ DispatchMessage(&msg);
				}
			}
			CloseHandle(fileop.hReadyEvent);
		}
		EnableWindow(cinfo->info.hWnd, TRUE);
	}
	cinfo->usereentry = 0;
}

void ModifyCellFilename(PPC_APPINFO *cinfo, ENTRYCELL *cell, const TCHAR *newname)
{
	TCHAR *newptr, *oldptr;
	DWORD newext;

	oldptr = FindLastEntryPoint(cell->f.cFileName);
	newptr = FindLastEntryPoint(newname);
	newext = FindExtSeparator(newptr);
	if ( tstrcmp(oldptr + FindExtSeparator(oldptr), newptr + newext) != 0 ){
		cell->icon = ICONLIST_NOINDEX;
		setflag(cinfo->SubTCmdFlags, SUBT_GETCELLICON);
		SetEvent(cinfo->SubT_cmd);
	}
	tstrcpy(cell->f.cFileName, newname);
	cell->ext = (WORD)((newptr - newname) + newext);
	if ( (tstrlen(newname + cell->ext) > X_extl) ||
		(!XC_sdir && (cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ){
		cell->ext = (WORD)tstrlen(newname);
	}

	if ( cinfo->CellHashType == CELLHASH_NAME ){
		cell->cellhash = MiniHash(cell->f.cFileName);
	}

	// ●簡易拡張子色変更
	GetCustTable(T("C_ext"), newname + cell->ext + 1, &cell->extC, sizeof(COLORREF));
}
#define ACCESSCHECKTEST 0
#if ACCESSCHECKTEST
#include <Aclapi.h>

BOOL CheckFileAccess(TCHAR *src)
{
	SECURITY_DESCRIPTOR *sd;
	DWORD size;
	HANDLE hPtoken, hImpPtoken;
	GENERIC_MAPPING gm;
	DWORD accessflags = FILE_GENERIC_WRITE;
	PRIVILEGE_SET privset;
	DWORD GrantedFlags;
	BOOL AccessStatus;

	if ( OSver.dwMajorVersion < 6 ) return TRUE;

	GetFileSecurity(src, DACL_SECURITY_INFORMATION, NULL, 0, &size);
	sd = LocalAlloc(LPTR, size);
	if ( sd == NULL ) return TRUE;

	//	XMessage(NULL, NULL, XM_DUMP, (char *)sd, 0x300);
	if ( FALSE == GetFileSecurity(src, DACL_SECURITY_INFORMATION, sd, size, &size) ){
		goto error;
	}
	//	XMessage(NULL, NULL, XM_DUMP, (char *)sd, 0x300);
	//	PPErrorBox(NULL, NULL, PPERROR_GETLASTERROR);
	//	XMessage(NULL, NULL, XM_DbgDIA, T("%s %d"), src, size);
	// GetFileSecurity (200)より GetNamedSecurityInfo (XP 1000byte)のほうが、情報が多い
	if ( NO_ERROR != GetNamedSecurityInfo(src, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sd) != ERROR_SUCCESS ){
		XMessage(NULL, NULL, XM_DbgDIA, T("GetNamedSecurityInfo error"));
		goto error;
	}

	if ( FALSE == OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hPtoken) ){
		goto error;
	}
	if ( FALSE == DuplicateToken(hPtoken, SecurityImpersonation, &hImpPtoken) ){
		goto error2;
	}
	gm.GenericRead = FILE_GENERIC_READ;
	gm.GenericWrite = FILE_GENERIC_WRITE;
	gm.GenericExecute = FILE_GENERIC_EXECUTE;
	gm.GenericAll = FILE_ALL_ACCESS;
	MapGenericMask(&accessflags, &gm);

	size = sizeof(PRIVILEGE_SET);
	privset.PrivilegeCount = 0;
	if ( FALSE == AccessCheck(sd, hImpPtoken, accessflags, &gm, &privset, &size, &GrantedFlags, &AccessStatus) ) {
		XMessage(NULL, NULL, XM_DbgDIA, T("AccessCheck error"));
	} else if ( IsTrue(AccessStatus) ){
		XMessage(NULL, NULL, XM_DbgDIA, T("AccessCheck ok"));
	} else{
		XMessage(NULL, NULL, XM_DbgDIA, T("AccessCheck bad"));
	}
	CloseHandle(hImpPtoken);
	CloseHandle(hPtoken);
	LocalFree(sd);
	return TRUE;
error2:
	CloseHandle(hPtoken);
error:
	LocalFree(sd);
	return TRUE;
}
#endif


//=========================================================== 名前変更
int RenameMain(PPC_APPINFO *cinfo, ENTRYCELL *cell, BOOL continuous)
{
	TCHAR name[MAX_PATH];
	TCHAR src[VFPS], dst[VFPS], *p, titlebuf[VFPS * 2];
	TINPUT tinput;
	int result;
	const TCHAR *title;

	tstrcpy(name, cell->f.cFileName);

#if ACCESSCHECKTEST
	VFSFullPath(src, name, cinfo->RealPath);
	Messagef("%d", CheckFileAccess(src));
#endif
	title = MessageText(continuous ? MES_TREC : MES_TREN);
	tstrcpy(titlebuf, title);
	tinput.hOwnerWnd = cinfo->info.hWnd;
	tinput.hWtype = PPXH_FILENAME;
	tinput.hRtype = PPXH_NAME_R;
	tinput.title = titlebuf;
	tinput.buff = name;
	tinput.size = TSIZEOF(name);
	tinput.info = &cinfo->info;

	for ( ; ; ){
		tinput.flag = TIEX_USEREFLINE | TIEX_USEINFO | TIEX_SINGLEREF | TIEX_USEPNBTN | TIEX_REFEXT | TIEX_FIXFORPATH;
		if ( !XC_sdir && (cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			resetflag(tinput.flag, TIEX_REFEXT);
		}

		result = tInputEx(&tinput);
		if ( result <= 0 ) return result;

		// SHN 形式での名前変更
		// ※ VFSDT_ZIPFOLDER とかは対応していない
		if ( cinfo->RealPath[0] == '?' ){
			LPITEMIDLIST idl, newidl;
			LPSHELLFOLDER pSF;
		#ifndef UNICODE
			WCHAR nameW[VFPS];
		#define nameT nameW

			AnsiToUnicode(name, nameW, VFPS);
		#else
		#define nameT name
		#endif
			// dir + file 形式に変換
			if ( VFSMakeIDL(cinfo->path, &pSF, &idl, cell->f.cFileName) == FALSE ){
				return -1;
			}
			if ( SUCCEEDED(pSF->lpVtbl->SetNameOf(pSF, cinfo->info.hWnd, idl, nameT, 0, &newidl)) ){
				FreePIDL(newidl);
				// result は 1 以上
			} else{
				result = 0;
			}
			FreePIDL(idl);
			pSF->lpVtbl->Release(pSF);
			if ( result == 0 ){
				TCHAR *msgp;

				msgp = titlebuf + wsprintf(titlebuf, T("%s") CAPTIONSEPARATOR, title);
				PPErrorMsg(msgp, ERROR_ACCESS_DENIED);
				continue;
			}
		} else{
			p = VFSFindLastEntry(cell->f.cFileName);
			if ( p != cell->f.cFileName ){
				tstrcpy(titlebuf, cell->f.cFileName);
				titlebuf[p - cell->f.cFileName] = '\0';
				VFSFixPath(NULL, titlebuf, cinfo->RealPath, VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
				if ( *p == '\\' ) p++;
			} else{
				tstrcpy(titlebuf, cinfo->RealPath);
			}
			if ( tstrcmp(name, p) != 0 ){
				int dstlen;

				VFSFixPath(NULL, name, NULL, VFSFIX_REALPATH);
				VFSFullPath(dst, name, titlebuf);
				VFSFullPath(src,
					((cinfo->e.Dtype.mode == VFSDT_PATH) &&
						cell->f.cAlternateFileName[0]) ?
					cell->f.cAlternateFileName : p, titlebuf);

				if ( MoveFileL(src, dst) == FALSE ){
					ERRORCODE errorresult;
					TCHAR *msgp;

					msgp = titlebuf + wsprintf(titlebuf, T("%s") CAPTIONSEPARATOR, MessageText(MES_TREN));
					errorresult = GetLastError();
					if ( (errorresult == ERROR_ACCESS_DENIED) ||
						(errorresult == ERROR_SHARING_VIOLATION) ){
						GetAccessApplications(src, msgp);
					}
					if ( *msgp == '\0' ) PPErrorMsg(msgp, errorresult);
					continue;
				}

				WriteReport(RRenTitle, cell->f.cFileName, NO_ERROR);
				WriteReport(RRenedTitle, name, NO_ERROR);
				dstlen = tstrlen(dst);
				if ( dstlen >= MAX_PATH ){
					wsprintf(dst, T("%s(%d)"), MessageText(MES_WLLP), dstlen);
					SetPopMsg(cinfo, POPMSG_MSG, dst);
				}

				// listfile / 上下移動時は、エントリ情報の書き換えを行う
				if ( ((cinfo->e.Dtype.mode == VFSDT_LFILE) || (result > 1)) &&
					(tstrlen(dst) < VFPS) ){
					ModifyCellFilename(cinfo, cell,
						((cinfo->e.Dtype.mode == VFSDT_LFILE) &&
						(cinfo->e.Dtype.pathtype != VFSPT_AUXOP))
						? dst : name);
				} else{
					ModifyCellFilename(cinfo, cell, name);
				}

				if ( XC_alst[ALST_RENAME] == ALSTV_UPD ){
					cell->state = ECS_CHANGED;
				}
				if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
					SHChangeNotify(SHCNE_RENAMEFOLDER, SHCNF_PATH, src, dst);
				}
			}
		}
		tstrcpy(cinfo->Jfname, name);
		return result;
	}
}

ERRORCODE PPC_Rename(PPC_APPINFO *cinfo, BOOL continuous)
{
	int result;
	BOOL modify = FALSE;
	ENTRYINDEX nextcell;

	if ( continuous && (cinfo->e.markC > 0) ){
		nextcell = IsCEL_Marked(0) ? 0 : DownSearchMarkCell(cinfo, 0);
		if ( nextcell >= 0 ){
			MoveCellCsr(cinfo, nextcell - cinfo->e.cellN, NULL);
		}
	} else if ( (CEL(cinfo->e.cellN).state < ECS_NORMAL) ||
		(CEL(cinfo->e.cellN).type <= ECT_LABEL) ){
		return ERROR_BAD_COMMAND;
	}

	for ( ;;){
		if ( StartCellEdit(cinfo) ) {
			result = ERROR_BUSY;
			break;
		}
		result = RenameMain(cinfo, &CEL(cinfo->e.cellN), continuous);
		// result ... -(K_c | 'R') 切替 / -1 失敗 / 0 中止 / 1 成功(OK) / IDB_PREV / IDB_NEXT
		EndCellEdit(cinfo);
		if ( result == -(int)(K_c | 'R') ){
			continuous = !continuous;
			continue;
		}
		if ( result == 1 ) modify = TRUE;
		if ( continuous ){ // 連続モード..OKはIDB_NEXT扱い
			if ( result <= 0 ) break;
		} else{
			if ( result <= 1 ) break;
		}

		// IDB_PREV / IDB_NEXT
		if ( continuous && (cinfo->e.markC > 0) ){
			if ( result == IDB_PREV ){
				nextcell = UpSearchMarkCell(cinfo, cinfo->e.cellN);
				if ( nextcell < 0 ) continue;
			}else{
				nextcell = DownSearchMarkCell(cinfo, cinfo->e.cellN);
				if ( nextcell < 0 ){
					if ( result == 1 ) break; // OK...終了
					continue; // IDB_NEXT...再表示
				}
			}
		} else{
			nextcell = cinfo->e.cellN + ((result == IDB_PREV) ? -1 : 1);
			if ( (nextcell < 0) || (nextcell >= cinfo->e.cellIMax) ) continue; //範囲外
		}
		if ( CEL(nextcell).type <= ECT_LABEL ) continue; //非対応
		MoveCellCsr(cinfo, nextcell - cinfo->e.cellN, NULL);
	}
	if ( IsTrue(modify) ){
		if ( cinfo->e.Dtype.mode != VFSDT_LFILE ){
			RefreshAfterList(cinfo, ALST_RENAME);
		} else{ // listfile は、内容直接操作済み
			WriteListFileForRaw(cinfo, cinfo->path);
			Repaint(cinfo);
		}
		return NO_ERROR;
	} else if ( result > 0 ){
		return NO_ERROR;
	} else{
		RefleshStatusLine(cinfo); // 処理中表示が残ることがあるので追加
		return ERROR_CANCELLED;
	}
}

//========================================================== ディレクトリの作成
void GetNewName(TCHAR *dest, const TCHAR *base, const TCHAR *path)
{
	TCHAR buf[VFPS], *p;

	CatPath(buf, (TCHAR *)path, base);
	GetUniqueEntryName(buf);
	p = FindLastEntryPoint(buf);
	tstrcpy(dest, p);
}

void USEFASTCALL ReadEntryForNewEntry(PPC_APPINFO *cinfo, TCHAR *newEntryPath, DWORD flags)
{
	DWORD size;

	size = tstrlen32(cinfo->RealPath);
	if ( memcmp(newEntryPath, cinfo->RealPath, size) == 0 ){
		TCHAR code, *path;

		if ( (size == 3) && (newEntryPath[1] == ':') ) size--;
		path = newEntryPath + size;
		code = *path;
		if ( code == '\\' ){ // 現在ディレクトリ以下に作成
			TCHAR *p;

			p = FindPathSeparator(path + 1);
			if ( p != NULL ) *p = '\0';

			tstrcpy(cinfo->Jfname, path + 1);
			read_entry(cinfo, flags);
			return;
		}
	}
	read_entry(cinfo, RENTRY_UPDATE | RENTRY_NOHIST | RENTRY_FLAGS_ARELOAD | RENTRY_SAVEOFF);
	return;
}

const TCHAR *GetNewEntryName(const TCHAR *custid, const TCHAR *mesid, TCHAR *buf)
{
	const TCHAR *mes;

	*buf = '\0';
	GetCustTable(Str_others, custid, buf, TSTROFF(MAX_PATH));
	if ( *buf != '\0' ) return buf;
	mes = MessageText(mesid);
	SetCustStringTable(Str_others, custid, mes, 0);
	return mes;
}


ERRORCODE PPC_MakeDir(PPC_APPINFO *cinfo)
{
	TCHAR target[VFPS], src[CMDLINESIZE], *ptr;
	ERRORCODE result;

	TINPUT tinput;

	PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, GetNewEntryName(T("NewDir"), NewDirName, src), src, XEO_EXTRACTEXEC);
	GetNewName(target, src, cinfo->RealPath);

	tinput.hOwnerWnd= cinfo->info.hWnd;
	tinput.hRtype	= PPXH_NAME_R;
	tinput.hWtype	= PPXH_FILENAME;
	tinput.title	= MES_TMKD;
	tinput.buff		= target;
	tinput.size		= TSIZEOF(target);
	tinput.flag		= TIEX_USEINFO | TIEX_SINGLEREF | TIEX_FIXFORPATH | TIEX_INSTRSEL | TIEX_USESELECT;
	tinput.firstC	= 0;
	tinput.lastC	= EC_LAST;
	tinput.info		= &cinfo->info;
	if ( tInputEx(&tinput) <= 0 ){
		return ERROR_CANCELLED;
	}
	if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
		WIN32_FIND_DATA ff;

		memset(&ff, 0, sizeof(ff));
		if ( cinfo->ArcPathMask[0] != '\0' ){
			wsprintf(ff.cFileName, T("%s%c%s"), cinfo->ArcPathMask, *FindPathSeparator(cinfo->path), target);
		} else{
			tstrcpy(ff.cFileName, target);
		}
		ff.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		InsertEntry(cinfo, -1, ff.cFileName, &ff);
		return NO_ERROR;
	}

	ptr = VFSFixPath(src, target, cinfo->RealPath, VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH);
	if ( ptr == NULL ){
		result = ERROR_BAD_PATHNAME;
		if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
			SHFILEOPSTRUCT fileop;

			fileop.wFunc	= FO__NEWFOLDER;
			fileop.pFrom	= target;
			fileop.pTo		= cinfo->path;
			fileop.fFlags	= 0;
			fileop.lpszProgressTitle	= NULL;

			if ( SxFileOperation(cinfo, &fileop) != SFOERROR_BADNEW ){
				result = NOERROR;
			}else{ // %TEMP% に空ディレクトリを作って、それを移動することで作成する
				IDataObject *DataObject;
				IDropTarget *DropTarget;

				if ( IsTrue(MakeTempEntry(VFPS, target, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_LABEL)) ){
					DropTarget = (IDropTarget *)GetPathInterface(
						cinfo->info.hWnd, cinfo->path, &IID_IDropTarget, NULL);
					if ( DropTarget != NULL ){
						DataObject = (IDataObject *)GetPathInterface(
							cinfo->info.hWnd, target, &IID_IDataObject, NULL);
						if ( DataObject != NULL ){
							if ( SUCCEEDED(PPcCopyToDropTarget(DataObject,
									DropTarget, DROPTYPE_LEFT, cinfo->info.hWnd,
									DROPEFFECT_MOVE)) ){
								result = NO_ERROR;
							}
							DataObject->lpVtbl->Release(DataObject);
						}
						DropTarget->lpVtbl->Release(DropTarget);
					}
					if ( result != NO_ERROR ) RemoveDirectoryL(target); // 移動失敗したときに削除する
				}
			}
		}
	} else{
		if ( StartCellEdit(cinfo) ) return ERROR_PATH_BUSY;
		result = MakeDirectories(src, NULL);
		EndCellEdit(cinfo);
	}
	if ( result != NO_ERROR ){
		SetPopMsg(cinfo, result, MES_TMKD);
		if ( result == ERROR_ALREADY_EXISTS ) FindCell(cinfo, ptr);
	} else{
		ReadEntryForNewEntry(cinfo, src, RETRY_FLAGS_NEWDIR | RENTRY_UPDATE);
	}
	return result;
}

//========================================================== エントリの作成
BOOL GetShellNewCommand(HKEY hKey, TCHAR *subname, DWORD subsize)
{
	HKEY hSKey;
	DWORD t;

	if ( RegOpenKeyEx(hKey, ShellNewString, 0, KEY_READ, &hSKey)
		!= ERROR_SUCCESS ){
		return FALSE;
	}
	if ( RegEnumValue(hSKey, 0, subname, &subsize, NULL, &t, NULL, NULL)
		!= ERROR_SUCCESS ){
		RegCloseKey(hSKey);
		return FALSE;
	}
	RegCloseKey(hSKey);
	return TRUE;
}

void SetShellNewItemMenu(HMENU hMenu, int *index, TCHAR *extname, const TCHAR *ext)
{
	HKEY hSKey;
	TCHAR subname[MAX_PATH];
	DWORD s;

	if ( RegOpenKeyEx(HKEY_CLASSES_ROOT, extname, 0, KEY_READ, &hSKey)
		!= ERROR_SUCCESS ){
		return;
	}
	s = sizeof subname;
	subname[0] = '\0';
	RegQueryValueEx(hSKey, NilStr, NULL, NULL, (LPBYTE)subname, &s);
	if ( subname[0] != '\0' ) tstrcpy(extname, subname);
	RegCloseKey(hSKey);

	tstrcat(extname, T("\t"));
	tstrcat(extname, ext);
	AppendMenuString(hMenu, *index, extname);
	(*index)++;
}

void MakeMakeEntryItem(HMENU hMenu)
{
	DWORD regsize;
	TCHAR name[MAX_PATH];
	int i = 0, index = NEWCMD_REG;

	AppendMenuString(hMenu, NEWCMD_DIR, MES_IDIR);
	AppendMenuString(hMenu, NEWCMD_FILE, MES_NITX);
	AppendMenuString(hMenu, NEWCMD_LISTFILE, MES_ILFI);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	//-------------------------------------------------------------------------
	for ( ; ; ){
		TCHAR extname[MAX_PATH], subname[MAX_PATH];
		HKEY HK;
		FILETIME ft;

		regsize = TSIZEOF(name);
		if ( RegEnumKeyEx(HKEY_CLASSES_ROOT, i, name, &regsize,
			NULL, NULL, NULL, &ft) != ERROR_SUCCESS ){
			break;
		}
		i++;
		if ( name[0] != '.' ) continue;
		if ( RegOpenKeyEx(HKEY_CLASSES_ROOT, name, 0, KEY_READ, &HK)
			== ERROR_SUCCESS ){
			regsize = sizeof extname;
			if ( (RegQueryValueEx(HK, NilStr, NULL, NULL, (LPBYTE)extname, &regsize) == ERROR_SUCCESS) && (regsize > 1) ){
				BOOL subget = FALSE;
				HKEY hSub;
				// 既定ドキュメント下にShellNew有り
				if ( RegOpenKeyEx(HK, extname, 0, KEY_READ, &hSub)
					== ERROR_SUCCESS ){
					subget = GetShellNewCommand(hSub, subname, sizeof subname);
					RegCloseKey(hSub);
				}
				// 直下にShellNew有り
				if ( subget == FALSE ){
					subget = GetShellNewCommand(HK, subname, sizeof subname);
				}
				if ( IsTrue(subget) ){
					SetShellNewItemMenu(hMenu, &index, extname, name);
				}
			}
			RegCloseKey(HK);
		}
	}
}

#define MakeType_NULL 0
#define MakeType_FILE 1
#define MakeType_DATA 2
int GetTemplateFileName(TCHAR *filename, const TCHAR *dir)
{
	TCHAR temppath[VFPS];

	if ( VFSFixPath(temppath, filename, dir, VFSFIX_FULLPATH | VFSFIX_REALPATH) == NULL ){
		return MakeType_NULL;
	}
	if ( GetFileAttributesL(temppath) == BADATTR ) return MakeType_NULL;
	tstrcpy(filename, temppath);
	return MakeType_FILE;
}

ERRORCODE USEFASTCALL MakeEntryMain(PPC_APPINFO *cinfo, int type, TCHAR *name)
{
	DWORD regtype, regsize;
	TCHAR filesrc[VFPS];
	int MakeType = MakeType_NULL;
	if ( type == NEWCMD_DIR ) return PPC_MakeDir(cinfo);

	if ( (type == NEWCMD_FILE) || (type == NEWCMD_LISTFILE) ){
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL,
				GetNewEntryName(T("NewFile"), NewFileName, name),
				name, XEO_EXTRACTEXEC);
		tstrcat(name, (OSver.dwMajorVersion < 6) ? T(".TXT") : T(".txt"));
	}else{
		TCHAR *namesepp, *extname;
		TCHAR exttypename[VFPS];

		namesepp = tstrrchr(name, '\t');
		if ( namesepp != NULL ){
			HKEY HKext;

			extname = namesepp;
			while ( *namesepp != '\0' ){
				*namesepp = *(namesepp + 1);
				namesepp++;
			}
			if ( RegOpenKeyEx(HKEY_CLASSES_ROOT, extname, 0, KEY_READ, &HKext)
					== ERROR_SUCCESS ){
				regsize = sizeof exttypename;
				if ( (RegQueryValueEx(HKext, NilStr, NULL, NULL, (LPBYTE)exttypename, &regsize) == ERROR_SUCCESS) && (regsize > 1) ){
					HKEY HKsnew;
					TCHAR temppath[VFPS];

								// 既定ドキュメント下にShellNew有り
					CatPath(temppath, exttypename, ShellNewString);
					if ( RegOpenKeyEx(HKext, temppath, 0, KEY_READ, &HKsnew)
							!= ERROR_SUCCESS ){
								// 直下にShellNew有り
						if ( RegOpenKeyEx(HKext, ShellNewString, 0, KEY_READ, &HKsnew)
								!= ERROR_SUCCESS ){
							HKsnew = NULL;
						}
					}
					if ( HKsnew != NULL ){
						// filename ?
						regsize = sizeof filesrc;
						if ( RegQueryValueEx(HKsnew, StrNewFileName, NULL, NULL, (LPBYTE)filesrc, &regsize) == ERROR_SUCCESS ){
							if ( filesrc[0] != '\0' ){
								// ユーザ別Template
								MakeType = GetTemplateFileName(filesrc, T("#21:\\"));
								// ユーザ共通Template
								if ( MakeType == MakeType_NULL ){
									MakeType = GetTemplateFileName(filesrc, T("#45:\\"));
								}
								// OS標準Template(\Windows\ShellNew)
								if ( MakeType == MakeType_NULL ){
									MakeType = GetTemplateFileName(filesrc, T("#36:\\ShellNew"));
								}
							}
						}
						// data ?
						if ( MakeType == MakeType_NULL ){
							regsize = sizeof filesrc;
							if ( RegQueryValueEx(HKsnew, StrNewData, NULL, &regtype, (LPBYTE)filesrc, &regsize) == ERROR_SUCCESS ){
								if ( regtype == REG_BINARY ){
									MakeType = MakeType_DATA;
								}
							}
						}
						// 新規作成用のアプリを起動？(command)
						RegCloseKey(HKsnew);
					}
				}
				RegCloseKey(HKext);
			}
		}
	}
	// 既存ファイルと被らないようにする
	GetNewName(name, name, cinfo->RealPath);
	for ( ; ; ){
		TINPUT tinput;
		HANDLE hFile;
		TCHAR buf[CMDLINESIZE], filepath[VFPS], *lastptr;
		DWORD tmp;
		ERRORCODE result = ERROR_CANCELLED;

		tstrcpy(buf, name);
		tinput.hOwnerWnd = cinfo->info.hWnd;
		tinput.hWtype	= PPXH_FILENAME;
		tinput.hRtype	= PPXH_NAME_R;
		tinput.title	= MES_TMKF;
		tinput.buff		= buf;
		tinput.size		= VFPS;
		tinput.flag		= TIEX_USESELECT | TIEX_USEINFO | TIEX_SINGLEREF | TIEX_FIXFORPATH | TIEX_INSTRSEL;
		tinput.firstC	= 0;
		tinput.lastC	= FindExtSeparator(name);
		tinput.info		= &cinfo->info;
		if ( tInputEx(&tinput) <= 0 ) return ERROR_CANCELLED;

		if ( StartCellEdit(cinfo) ) return ERROR_PATH_BUSY;
		lastptr = VFSFixPath(filepath, buf, cinfo->RealPath, VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH);
		if ( lastptr != NULL ){
			if ( MakeType == MakeType_FILE ){ // template file をコピー
				*(lastptr - 1) = '\0';
				wsprintf(buf, T("*file !copy,\"%s\",\"%s\",/name:\"%s\""), filesrc, filepath, lastptr);
				*(lastptr - 1) = '\\';
				result = PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, buf, NULL, 0);
			}
			if ( result != NO_ERROR ){ // null file を生成
				hFile = CreateFileL(filepath, GENERIC_WRITE, 0, NULL,
						CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
				if ( hFile == INVALID_HANDLE_VALUE ){
					EndCellEdit(cinfo);
					SetPopMsg(cinfo, POPMSG_GETLASTERROR, MES_TMKF);
					continue;
				}
				if ( MakeType == MakeType_DATA ){ // template data を出力
					WriteFile(hFile, filesrc, regsize, &tmp, NULL);
				}
				if ( type == NEWCMD_LISTFILE ){ // listfile header を出力
					WriteFile(hFile, ListFileHeaderStr, ListFileHeaderSize, &tmp, NULL);
				}
				CloseHandle(hFile);
			}
		}
		EndCellEdit(cinfo);
		ReadEntryForNewEntry(cinfo, filepath, RETRY_FLAGS_NEWFILE | RENTRY_UPDATE);
		return NO_ERROR;
	}
}

ERRORCODE PPC_MakeEntry(PPC_APPINFO *cinfo)
{
	HMENU hMenu;
	int index;
	TCHAR name[CMDLINESIZE];

	hMenu = CreatePopupMenu();
	MakeMakeEntryItem(hMenu);
	//-------------------------------------------------------------------------
	index = PPcTrackPopupMenu(cinfo, hMenu);
	GetMenuString(hMenu, index, name, TSIZEOF(name), MF_BYCOMMAND);
	DestroyMenu(hMenu);
	if ( index == 0 ) return ERROR_CANCELLED;
	return MakeEntryMain(cinfo, index, name);
}

void PPcCreateHarkLink(PPC_APPINFO *cinfo)
{
	typedef BOOL (APIENTRY *impCreateHardLink)(
			LPCTSTR lpFileName, LPCTSTR lpExistingFileName,
			LPSECURITY_ATTRIBUTES lpSecurityAttributes);
	impCreateHardLink DCreateHardLink;
	TCHAR name[VFPS];
	TCHAR src[VFPS], dst[VFPS];

	GETDLLPROCT(GetModuleHandle(StrKernel32DLL), CreateHardLink);
	if ( DCreateHardLink == NULL ){
		SetPopMsg(cinfo, POPMSG_GETLASTERROR, MES_ENSO);
		return;
	}
	tstrcpy(name, CEL(cinfo->e.cellN).f.cFileName);
	if ( PPctInput(cinfo, MES_TMHL, name, TSIZEOF(name),
			PPXH_NAME_R, PPXH_FILENAME) > 0 ){
		CatPath(src, cinfo->path, CEL(cinfo->e.cellN).f.cFileName);
		VFSFixPath(dst, name, cinfo->path, VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH);
		tstrcpy(cinfo->Jfname, name);

		if ( CEL(cinfo->e.cellN).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			ERRORCODE result;

			result = CreateJunction(dst, src, NULL);
			if ( result != NO_ERROR ){
				SetPopMsg(cinfo, result, MES_TMHL);
				return;
			}
			read_entry(cinfo, RETRY_FLAGS_NEWDIR | RENTRY_UPDATE);
		}else{
			if ( DCreateHardLink(dst, src, NULL) == FALSE ){
				SetPopMsg(cinfo, POPMSG_GETLASTERROR, MES_TMHL);
				return;
			}
			read_entry(cinfo, RETRY_FLAGS_NEWFILE | RENTRY_UPDATE);
		}
	}
}

ERRORCODE PPcDupFile(PPC_APPINFO *cinfo)
{
	TCHAR name[VFPS];
	TCHAR src[VFPS * 2], dst[VFPS];
	TINPUT tinput;
	ENTRYCELL *cell;
	ERRORCODE result;

	tinput.hOwnerWnd = cinfo->info.hWnd;
	tinput.hWtype	= PPXH_FILENAME;
	tinput.hRtype	= PPXH_NAME_R;
	tinput.title	= MES_TDUP;
	tinput.buff		= name;
	tinput.size		= TSIZEOF(name);
	tinput.info		= &cinfo->info;
	tinput.flag		= TIEX_USEREFLINE | TIEX_USEINFO | TIEX_SINGLEREF | TIEX_FIXFORPATH;
	cell = &CEL(cinfo->e.cellN);
	if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
		setflag(tinput.flag, TIEX_REFEXT);
	}
	tstrcpy(name, FindLastEntryPoint(cell->f.cFileName) );
	if ( tInputEx(&tinput) <= 0 ) return ERROR_CANCELLED;

	if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		VFSFullPath(dst, name, cinfo->RealPath);
		wsprintf(src, T("*file !copy,\"%%*name(NDC,\"%%R\")\\*\",\"%s\",/querycreatedirectory:off"), dst);
	}else{
		wsprintf(src, T("*file !copy,%%*name(BDC,\"%%R\"),\"%%1\",/name:\"%s\""), name);
	}
	result = PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, src, NULL, 0);
	tstrcpy(cinfo->Jfname, name);
	read_entry(cinfo, RENTRY_JUMPNAME | RENTRY_SAVEOFF);
	return result;
}

//============================================= [W]コマンド関連/ 順番の書き込み
ERRORCODE WriteFSDir(PPC_APPINFO *cinfo)
{
	TCHAR tmppath[VFPS], src[VFPS], dst[VFPS];
	int i, firstindex;
	JOBINFO jinfo;

	if ( PMessageBox(cinfo->info.hWnd, MES_QWRD, MES_TWRD, MB_QYES) != IDOK ){
		return ERROR_CANCELLED;
	}
									// 作業ディレクトリを作成
	GetDriveName(src, cinfo->path);
	CatPath(tmppath, src, T("PPXTMP"));
	GetUniqueEntryName(tmppath);
	if ( CreateDirectory(tmppath, NULL) == FALSE ){
		CatPath(tmppath, cinfo->path, T("PPXTMP"));
		GetUniqueEntryName(tmppath);
		if ( CreateDirectory(tmppath, NULL) == FALSE ){
			SetPopMsg(cinfo, POPMSG_GETLASTERROR, MES_TWRD);
			return ERROR_ACCESS_DENIED;
		}
	}
	PPxCommonCommand(NULL, JOBSTATE_FOP_MOVE, K_ADDJOBTASK);
	InitJobinfo(&jinfo);
	cinfo->BreakFlag = FALSE;
	firstindex = 0;
											// IO.SYS/MSDOS.SYS の可能性
	if ( (CEL(0).f.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ||
		 (CEL(1).f.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ){
		firstindex = 2;
	}

	for ( i = firstindex ; i < cinfo->e.cellIMax ; i++ ){
		if ( CEL(i).type > ECT_NOFILEDIRMAX ){
			jinfo.count++;
			CatPath(src, cinfo->path, CEL(i).f.cFileName);
			CatPath(dst, tmppath, CEL(i).f.cFileName);
			MoveFile(src, dst);
			if ( IsTrue(BreakCheck(cinfo, &jinfo, src)) ) break;
		}
	}
	for ( i = firstindex ; i < cinfo->e.cellIMax ; i++ ){
		if ( CEL(i).type > ECT_NOFILEDIRMAX ){
			jinfo.count++;
			CatPath(src, tmppath, CEL(i).f.cFileName);
			CatPath(dst, cinfo->path, CEL(i).f.cFileName);
			MoveFile(src, dst);
			if ( IsTrue(BreakCheck(cinfo, &jinfo, dst)) ) break;
		}
	}
	if ( RemoveDirectoryL(tmppath) == FALSE ){
		SetPopMsg(cinfo, POPMSG_GETLASTERROR, MES_EDTD);
	}
	read_entry(cinfo, RENTRY_SAVEOFF);
	FinishJobinfo(cinfo, &jinfo, NO_ERROR);
	PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
	return NO_ERROR;
}

ERRORCODE PPC_WriteDir(PPC_APPINFO *cinfo)
{
	if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) ||
		 (cinfo->dset.flags & (/*DSET_ASYNCREAD | */ DSET_CACHEONLY)) ){
		TCHAR pathbuf[VFPS];
		const TCHAR *path;

		if ( PMessageBox(cinfo->info.hWnd, MES_QWRD, T("ListFile"), MB_QYES) != IDOK ){
			return ERROR_CANCELLED;
		}
		if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){
			path = cinfo->path;
		}else{
			GetCache_Path(pathbuf, cinfo->path, NULL);
			path = pathbuf;
		}
		WriteListFileForUser(cinfo, path, WLFC_WRITEDIR_DEFAULT);
		return NO_ERROR;
	}
	if ( (cinfo->e.Dtype.mode == VFSDT_PATH) ||
		 (cinfo->e.Dtype.mode == VFSDT_SHN) ){
		return WriteFSDir(cinfo);
	}
	SetPopMsg(cinfo, POPMSG_MSG, MES_EBWS);
	return ERROR_BAD_COMMAND;
}

//===================================================================== 削除
ERRORCODE DeleteEntrySH(PPC_APPINFO *cinfo)
{
	BYTE regbuf[0x100];
	HKEY HK;
	DWORD size, t;
	DWORD X_wdel[4] = X_wdel_default;

	if ( cinfo->e.markC == 0 ){
		if ( (CEL(cinfo->e.cellN).state < ECS_NORMAL) ||
			 (CEL(cinfo->e.cellN).type <= ECT_LABEL) ){
			SetPopMsg(cinfo, POPMSG_MSG, MES_EDEL);
			return ERROR_BAD_COMMAND;
		}
	}
	GetCustData(T("X_wdel"), &X_wdel, sizeof(X_wdel));
	if ( X_wdel[3] ){
		regbuf[RECYCLEBINOFFSET] = 0;
		if ( RegOpenKeyEx(HKEY_CURRENT_USER, ShellStatePath, 0, KEY_READ, &HK) == ERROR_SUCCESS ){
			size = sizeof(regbuf);
			RegQueryValueEx(HK, ShellStateName, NULL, &t, regbuf, &size);
		}
		if ( regbuf[RECYCLEBINOFFSET] & DELETEDIALOG ){
			if ( PMessageBox(cinfo->info.hWnd, QueryRecyclebin, MES_TDEL, MB_QYES) != IDOK ){
				return ERROR_CANCELLED;
			}
		}
	}

	if ( X_wdel[0] & B4 ){
		if ( CheckOffScreenMark(cinfo, MES_TDEL) != IDYES ){
			return ERROR_CANCELLED;
		}
	}

	if ( cinfo->e.Dtype.mode != VFSDT_SHN ){
		TCHAR *names;
		SHFILEOPSTRUCT fileop;

		names = GetFiles(cinfo, GETFILES_FULLPATH | GETFILES_REALPATH);
		if ( names == NULL ){
			ERRORCODE result;

			PPxCommonCommand(NULL, JOBSTATE_FOP_DELETE, K_ADDJOBTASK);
			result = SCmenu(cinfo, T("delete"));
			PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
			return result;

//			SetPopMsg(cinfo, POPMSG_MSG, MES_EDEL);
//			return ERROR_BAD_COMMAND;
		}
		fileop.wFunc				= FO_DELETE;
		fileop.pFrom				= names;
		fileop.pTo					= NULL;
		fileop.fFlags				= FOF_ALLOWUNDO;
		fileop.lpszProgressTitle	= MessageText(MES_TDEL);

		PPxCommonCommand(NULL, JOBSTATE_FOP_DELETE, K_ADDJOBTASK);
		if ( SxFileOperation(cinfo, &fileop) == SFOERROR_OK ){
			SetRefreshAfterList(cinfo, ALST_DELETE, '\0');
		}
		PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
		SHFileOperationFix(cinfo);
	}else{							// PIDL
		ERRORCODE result;

		PPxCommonCommand(NULL, JOBSTATE_FOP_DELETE, K_ADDJOBTASK);
		result = SCmenu(cinfo, T("delete"));
		PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
		return result;
	}
	return NO_ERROR;
}

ERRORCODE EraseListEntry(PPC_APPINFO *cinfo)
{
	int work;
	ENTRYCELL *cell;

	if ( cinfo->e.Dtype.mode != VFSDT_LFILE ) return ERROR_BAD_COMMAND;

	if ( StartCellEdit(cinfo) ) return ERROR_PATH_BUSY;
	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		cell->state = ECS_DELETED;	// 削除
	}
	EndCellEdit(cinfo);
	WriteListFileForRaw(cinfo, cinfo->path);
	SetRefreshAfterList(cinfo, ALST_DELETE, '\0');
	SetPopMsg(cinfo, POPMSG_MSG, MES_LERE);
	return NO_ERROR;
}

ERRORCODE DeleteEntry(PPC_APPINFO *cinfo)
{
	TCHAR buf[VFPS * 2];
	int result;
	DWORD X_wdel[4] = X_wdel_default;

	if ( cinfo->RealPath[0] == '?' ) return DeleteEntrySH(cinfo);

	if ( cinfo->e.markC <= 1 ){
		if ( !cinfo->e.markC &&
			 ((CEL(cinfo->e.cellN).state < ECS_NORMAL) ||
			 (CEL(cinfo->e.cellN).type  < 4)) ){
			SetPopMsg(cinfo, POPMSG_MSG, MES_EDEL);
			return ERROR_BAD_COMMAND;
		}
		wsprintf(buf, T("%s %s"), cinfo->e.markC ?
				CELdata(cinfo->e.markTop).f.cFileName :
				CEL(cinfo->e.cellN).f.cFileName, MessageText(MES_QDL1));
	}else{
		wsprintf(buf, T("%s %s%d %s"), CELdata(cinfo->e.markTop).f.cFileName,
				MessageText(MES_QDL2), cinfo->e.markC, MessageText(MES_QDL3));
	}
	if ( StartCellEdit(cinfo) ) return ERROR_PATH_BUSY;

	GetCustData(T("X_wdel"), &X_wdel, sizeof(X_wdel));
	if ( X_wdel[2] ){
		if ( X_wdel[2] > 2 ) X_wdel[2] -= 2; // 旧設定の変換
		if ( X_wdel[2] > 2 ) X_wdel[2] = 1;
		result = PMessageBox(cinfo->info.hWnd, buf, MES_TDEL, deletemesboxstyle[X_wdel[2]]);
	}else{
		result = IDOK;
	}

	if ( (X_wdel[0] & B4) && ((result == IDOK) || (result == IDYES)) ){
		result = CheckOffScreenMark(cinfo, MES_TDEL);
	}

	if ( (result == IDOK) || (result == IDYES) ){
		PPcDeleteFile(cinfo, X_wdel);
		EndCellEdit(cinfo);
		SetRefreshAfterList(cinfo, ALST_DELETE, '\0');
		ActionInfo(cinfo->info.hWnd, &cinfo->info, AJI_COMPLETE, T("delete"));
		return NO_ERROR;
	}else{
		EndCellEdit(cinfo);
		return ERROR_CANCELLED;
	}
}

void PPcDeleteFile(PPC_APPINFO *cinfo, DWORD *X_wdel)
{
	ENTRYCELL *cell;
	int work;

	DELETESTATUS dstat;
	TCHAR name[VFPS];
	ERRORCODE err = NO_ERROR;

	dstat.OldTime = GetTickCount();
	dstat.count = 0;
	dstat.useaction = 0;
	dstat.noempty = FALSE;
	dstat.flags = X_wdel[0];
	dstat.warnattr = X_wdel[1];
	dstat.info = &cinfo->info;

	if ( (Combo.Report.hWnd != NULL) ||
		 ((hCommonLog != NULL) && IsTrue(IsWindow(hCommonLog))) ){
		setflag(dstat.flags, VFSDE_REPORT);
	}

	PPxCommonCommand(NULL, JOBSTATE_FOP_DELETE, K_ADDJOBTASK);
	if ( cinfo->e.Dtype.mode != VFSDT_SHN ){
		TCHAR *path;

		InitEnumMarkCell(cinfo, &work);
		if ( cinfo->e.Dtype.mode == VFSDT_FATDISK ){
			path = cinfo->path + 1;
		}else if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) && (cinfo->e.Dtype.BasePath[0] != '\0') ){
			path = cinfo->e.Dtype.BasePath;
		}else{
			path = cinfo->path;
		}
		while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
			TCHAR *entry;
			if ( IsParentDir(cell->f.cFileName) ) continue;

			entry = cell->f.cFileName;
			if ( *entry == '>' ){
				TCHAR *longname = (TCHAR *)EntryExtData_GetDATAptr(cinfo, DFC_LONGNAME, cell);
				if ( longname != NULL ) entry = longname;
			}

			#ifdef UNICODE
				VFSFullPath(name, entry, path);
			#else
				VFSFullPath(name,
					((cell->f.cAlternateFileName[0] != '\0') &&
					 (strchr(entry, '?') != NULL) ) ?
					cell->f.cAlternateFileName : entry, path);
			#endif
			err = VFSDeleteEntry(&dstat, name, cell->f.dwFileAttributes);
			if ( err != NO_ERROR ) break;
			cell->state = ECS_DELETED;	// 削除に成功したので状態を変更
		}

		if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){ // listfile 更新
			WriteListFileForRaw(cinfo, cinfo->path);
		}
	}else{
		TCHAR *ptr, *files;

		files = ptr = GetFiles(cinfo, 0);
		if ( ptr == NULL ){
			xmessage(XM_FaERRd, T("Alloc error"));
			return;
		}
		while ( *ptr != '\0' ){
			DWORD attr;

			attr = GetFileAttributesL(ptr);
			if ( attr != BADATTR ){
				if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
					if ( !(attr & FILE_ATTRIBUTE_REPARSE_POINT) ){
						err = VFSDeleteEntry(&dstat, ptr, attr);
						if ( err != NO_ERROR ) break;
					}
				}else{
					err = VFSDeleteEntry(&dstat, ptr, attr);
					if ( err != NO_ERROR ) break;
				}
			}
			ptr += tstrlen(ptr) + 1;
		}
		HeapFree( hProcessHeap, 0, files);
	}
	PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
	if ( Combo.hWnd != NULL ){
		SendMessage(Combo.hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_WINDDOWLOG, PPLOG_SHOWLOG), 0);
	}
	StopPopMsg(cinfo, PMF_DISPLAYMASK);
	PPxCommonExtCommand(K_TBB_STOPPROGRESS, (WPARAM)cinfo->info.hWnd);

//	if ( (err == NO_ERROR) && dstat.noempty ) err = ERROR_DIR_NOT_EMPTY;
	if ( (err != NO_ERROR) &&
		 (err != ERROR_CANCELLED) &&
		 (err != ERROR_DIR_NOT_EMPTY) ){
		SetPopMsg(cinfo, err, NULL);
	}
}
