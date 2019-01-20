/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		*file 仮想ディレクトリ関連
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <winioctl.h>
#include "PPX.H"
#include "PPX_64.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FOP.H"
#pragma hdrstop

BOOL ImgExtractFile(FOPSTRUCT *FS,HANDLE hFile,const TCHAR *srcDir,const TCHAR *srcPath,const TCHAR *dstDIR);

// "filename","alternate",A:atr,C:create,L:access,W:write,S:size,comment
void WriteFFA(HANDLE hFile,WIN32_FIND_DATA *ff,const TCHAR *name)
{
	TCHAR buf[VFPS * 2];
	DWORD tmp;

	WriteFile(hFile,"\"",sizeof(char),&tmp,NULL);
	WriteFileZT(hFile,name,&tmp);
	wsprintf(buf,T("\",\"%s\",")
			T("A:H%x,C:%u.%u,L:%u.%u,W:%u.%u,S:%u.%u\r\n"),
			ff->cAlternateFileName,
			ff->dwFileAttributes,
			ff->ftCreationTime.dwHighDateTime,
			ff->ftCreationTime.dwLowDateTime,
			ff->ftLastAccessTime.dwHighDateTime,
			ff->ftLastAccessTime.dwLowDateTime,
			ff->ftLastWriteTime.dwHighDateTime,
			ff->ftLastWriteTime.dwLowDateTime,
			ff->nFileSizeHigh,
			ff->nFileSizeLow
		);
	WriteFileZT(hFile,buf,&tmp);
}
void WriteFFW(HANDLE hFile,WIN32_FIND_DATA *ff,const TCHAR *name)
{
	TCHAR buf[VFPS * 2];
	DWORD tmp;
#ifndef UNICODE
	WCHAR bufW[VFPS * 2];
#endif

	WriteFile(hFile,L"\"",sizeof(WCHAR),&tmp,NULL);
#ifdef UNICODE
	WriteFile(hFile,name, ToSIZE32_T(strlenW(name) * sizeof(WCHAR)),&tmp,NULL);
#else
	AnsiToUnicode(name,bufW,VFPS);
	WriteFile(hFile,bufW,strlenW(bufW) * sizeof(WCHAR),&tmp,NULL);
#endif
	wsprintf(buf,T("\",\"%s\",")
			T("A:H%x,C:%u.%u,L:%u.%u,W:%u.%u,S:%u.%u\r\n"),
			ff->cAlternateFileName,
			ff->dwFileAttributes,
			ff->ftCreationTime.dwHighDateTime,
			ff->ftCreationTime.dwLowDateTime,
			ff->ftLastAccessTime.dwHighDateTime,
			ff->ftLastAccessTime.dwLowDateTime,
			ff->ftLastWriteTime.dwHighDateTime,
			ff->ftLastWriteTime.dwLowDateTime,
			ff->nFileSizeHigh,
			ff->nFileSizeLow
		);
#ifdef UNICODE
	WriteFile(hFile,buf, ToSIZE32_T(strlenW(buf) * sizeof(WCHAR)),&tmp,NULL);
#else
	AnsiToUnicode(buf,bufW,VFPS * 2);
	WriteFile(hFile,bufW,strlenW(bufW) * sizeof(WCHAR),&tmp,NULL);
#endif
}

BOOL OperationStartListFile(FOPSTRUCT *FS,const TCHAR *srcDIR,TCHAR *dstDIR)
{
	HANDLE hFile;
	WIN32_FIND_DATA ff;
	HANDLE hFF;
	const TCHAR *p;
	char srcA[VFPS];
	DWORD size;
	int i;
	BOOL useUNICODE;
#ifdef UNICODE
	WCHAR srcW[VFPS];
	#define src srcW
#else
	#define src srcA
#endif
	if ( FS->testmode ) return TRUE;

	hFile = CreateFileL(dstDIR,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_ALWAYS,
											FILE_ATTRIBUTE_NORMAL,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return FALSE;

	// 簡易UNICODE判定
#ifdef UNICODE
	srcA[1] = 0;
#else
	srcA[1] = 1;
#endif
	ReadFile(hFile,srcA,2,&size,NULL);
	useUNICODE = ((srcA[1] == 0) || ((BYTE)srcA[1] == 0xfe)) ? TRUE : FALSE;
	// 改行追加
	size = 0;
	SetFilePointer(hFile,0,(PLONG)&size,FILE_END);
	if ( useUNICODE ){
		WriteFile(hFile,L"\r\n",4,&size,NULL);
	}else{
		WriteFile(hFile,"\r\n",2,&size,NULL);
	}

	// エントリ追加
	for ( p = FS->opt.files ; *p ; p = p + tstrlen(p) + 1 ){
		if ( IsParentDirectory(p) ) continue;
		if ( FS->opt.dtype == VFSDT_SHN ){
			tstrcpy(src,p);
		}else{
			if ( VFSFullPath(src,(TCHAR *)p,srcDIR) == NULL ) continue;
		}
		hFF = FindFirstFileL(src,&ff);
		if ( hFF != INVALID_HANDLE_VALUE ){
			FindClose(hFF);
			if ( useUNICODE ){
				WriteFFW(hFile,&ff,src);
			}else{
				WriteFFA(hFile,&ff,src);
			}
		}
	}
	CloseHandle(hFile);

	// 更新したので各PPcに通知
	tstrcpy(src,dstDIR);
	tstrcat(src,StrListFileid);

	FixTask();
	for ( i = 0 ; i < X_Mtask ; i++ ){
		if ( (Sm->P[i].ID[0] != '\0') &&
			 (!tstrcmp(Sm->P[i].path,dstDIR) || !tstrcmp(Sm->P[i].path,src)) ){
			PostMessage(Sm->P[i].hWnd,WM_PPXCOMMAND,
					K_raw | K_c | K_v | VK_F5,0);
		}
	}
	return TRUE;
}

int OperationStartToFile(FOPSTRUCT *FS,const TCHAR *srcDIR,TCHAR *dstDIR)
{
	VFSFILETYPE vft;
										// ファイルの内容で判別 ---------------
	vft.flags = VFSFT_TYPETEXT;
	if ( VFSGetFileType(dstDIR,NULL,0,&vft) != NO_ERROR ){
		vft.type[0] = '\0';
	}
	/* ショートカットをコピー先に(●準備中)
	if ( !tstrcmp(vft.type,T(":LINK")) ){
		if ( SUCCEEDED(GetLink(FS->hDlg,dstDIR,dstDIR)) && dstDIR[0] ){
			Message(dstDIR);
			return -1;
		}
	}
	*/
	// listfile
	if ( !tstrcmp(vft.type,T(":XLF")) ){
		return OperationStartListFile(FS,srcDIR,dstDIR);
	}
	if ( Fop_ShellNameSpace(FS,srcDIR,dstDIR) ) return 1;

	PPErrorBox(FS->hDlg,dstDIR,ERROR_DIRECTORY);
	return 0;
}

HANDLE tOpenFile(const TCHAR *filename,LPCTSTR *wp)
{
	TCHAR buf[VFPS],*fp;
	HANDLE	hFile;
	TCHAR	*vp,drive;
	int		openmode,offset = 0;
	TCHAR *separator;

	tstrcpy(buf,filename);
	vp = VFSGetDriveType(buf,&openmode,NULL);
	if ( vp == NULL ){		// 種類が分からない→相対指定の可能性→絶対化
		VFSFullPath(NULL,buf,NULL);
		vp = VFSGetDriveType(buf,&openmode,NULL);
		if ( vp == NULL ){	// それでも種類が分からない→エラー
			return INVALID_HANDLE_VALUE;
		}
	}
	if (openmode == VFSPT_RAWDISK){
		vp -= 2;
		drive = *vp;
		wsprintf(buf,T("\\\\.\\%c:"),drive);
		offset = 1;
	}

	separator = tstrrchr(buf,':'); // "::" を検索
	if ( (separator != NULL) && (separator >= (buf + 2)) && (*(separator - 1) == ':') ){
		*(separator - 1) = '\0';

		fp = FindPathSeparator(separator);
		if ( fp == NULL ) fp = separator + tstrlen(separator);
		hFile = CreateFileL(buf,GENERIC_READ,
						FILE_SHARE_WRITE | FILE_SHARE_READ,NULL,
						OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	}else{
		for ( ; ; ){
			fp = buf + tstrlen(buf);
			hFile = CreateFileL(buf,GENERIC_READ,
					FILE_SHARE_WRITE | FILE_SHARE_READ,NULL,
					OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
			if ( hFile != INVALID_HANDLE_VALUE ) break;
			fp = VFSFindLastEntry(buf);
			if ( fp == NULL ){
				hFile = INVALID_HANDLE_VALUE;
				break;
			}
			*fp = '\0';
		}
	}
	if ( offset ) fp = vp + 2;
	*wp = filename + (fp - buf);
	return hFile;
}

/*
	WinXP 以降では SetFileValidData が追加されているが、
	特権SE_MANAGE_VOLUME_NAMEが必要なほか、ネットワークドライブ、圧縮ファイル
	暗号化ファイル等には使えない。

	DefineWinAPI(BOOL,SetFileValidData,(HANDLE hFile,LONGLONG ValidDataLength));
	GETDLLPROC(hKernel32,SetFileValidData);
	DSetFileValidData(dstH,srcfinfo.nFileSizeLow);
*/
BOOL NotifyFileSize(HANDLE hFile,DWORD sizeL,DWORD sizeH)
{
	SetFilePointer(hFile,sizeL,(PLONG)&sizeH,FILE_BEGIN);
	SetEndOfFile(hFile);
	sizeH = 0;
	SetFilePointer(hFile,0,(PLONG)&sizeH,FILE_BEGIN);
	return TRUE;
}
#define IOCTL_CDROM_GET_DRIVE_GEOMETRY 0x0002404C

// ディスクイメージを生成する(CD-ROMは本来のサイズより小さくなる問題がある)
BOOL ImgExtractImage(FOPSTRUCT *FS,HANDLE hFile,const TCHAR *srcPath,const TCHAR *dstDIR)
{
	TCHAR path[VFPS],*p;
	DWORD size,sizeL,sizeH;
	HANDLE hDstFile;
	BYTE buff[0x10000];
	ERRORCODE result_error = FALSE;
	LARGE_INTEGER TotalTransSize,TotalSize;

	TotalTransSize.u.LowPart = 0;
	TotalTransSize.u.HighPart = 0;

	wsprintf(path,T("%s.img"),srcPath);
	while( (p = tstrchr(path,':')) != NULL ) *p = '-';
	while( (p = FindPathSeparator(path)) != NULL ) *p = '-';
	VFSFullPath(NULL,path,dstDIR);

	if ( (FS->opt.dtype == VFSDT_FATDISK) || (FS->opt.dtype == VFSDT_CDDISK) ){
		DISK_GEOMETRY diskinfo;
		DWORD tmp;

		if ( (FALSE != DeviceIoControl(hFile,IOCTL_CDROM_GET_DRIVE_GEOMETRY,
						NULL,0,&diskinfo,sizeof(DISK_GEOMETRY),&tmp,NULL)) ||
			 (FALSE != DeviceIoControl(hFile,IOCTL_DISK_GET_DRIVE_GEOMETRY,
						NULL,0,&diskinfo,sizeof(DISK_GEOMETRY),&tmp,NULL)) ){
			DWORD tracksize,tmpL,tmpH;

			tracksize = diskinfo.TracksPerCylinder *
				diskinfo.SectorsPerTrack * diskinfo.BytesPerSector;

			DDmul(tracksize,diskinfo.Cylinders.u.LowPart,
					&TotalSize.u.LowPart,(DWORD *)&TotalSize.u.HighPart);
			DDmul(tracksize,diskinfo.Cylinders.u.HighPart,&tmpL,&tmpH);
			TotalSize.u.HighPart += tmpL;
		}else{
			TotalSize.u.LowPart = 200 * MB;
			TotalSize.u.HighPart = 0;
		}
	}else{
		TotalSize.u.LowPart = GetFileSize(hFile,(DWORD *)&TotalSize.u.HighPart);
	}
	for ( ; ; ){
		ERRORCODE error;
		BY_HANDLE_FILE_INFORMATION srcfinfo,dstfinfo;

		memset(&srcfinfo,0,sizeof(srcfinfo));

		hDstFile = CreateFileL(path,GENERIC_WRITE,0,NULL,
				CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
		if ( hDstFile != INVALID_HANDLE_VALUE ) break;	// 成功

		error = GetLastError();
		if ( (error != ERROR_ALREADY_EXISTS) &&
			(error != ERROR_FILE_EXISTS) ){
			PPErrorBox(NULL,path,error);
			return FALSE;
		}
		hDstFile = CreateFileL(path,GENERIC_READ,0,NULL,
				OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

		error = SameNameAction(FS,hDstFile,&srcfinfo,&dstfinfo,srcPath,path);
		switch( error ){
			case ACTION_RETRY:
				continue;

			case ACTION_APPEND:
			case ACTION_OVERWRITE:
				// ACTION_CREATE へ
			case ACTION_CREATE:
				break;

			case ACTION_SKIP:
				FS->progs.info.EXskips++;
				return TRUE;

			default: // error
				return FALSE;
		}
		if ( dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES ){
			SetFileAttributesL(path,FILE_ATTRIBUTE_NORMAL);
		}
		DeleteFileL(path);
	}

	sizeL = TotalSize.u.LowPart;
	sizeH = TotalSize.u.HighPart;
	NotifyFileSize(hDstFile,sizeL,sizeH);
	size = 0;
	SetFilePointer(hFile,0,(PLONG)&size,FILE_BEGIN);

	while( sizeL || sizeH ){
		if ( ReadFile(hFile,buff,
				sizeH ? sizeof(buff) : min(sizeL,sizeof(buff)),
				&size,NULL) == FALSE ){
			result_error = TRUE;
			break;
		}
		if ( !size ) break;
		if ( WriteFile(hDstFile,buff,size,&size,NULL) == FALSE ){
			result_error = TRUE;
			break;
		}
		SubDD(sizeL,sizeH,size,0);
		AddDD(TotalTransSize.u.LowPart,TotalTransSize.u.HighPart,size,0);
		FullDisplayProgress(&FS->progs,TotalTransSize,TotalSize);
		PeekMessageLoop(FS);
		if ( FS->state == FOP_TOBREAK ){
			result_error = TRUE;
			break;
		}
	}
	CloseHandle(hDstFile);
	if ( IsTrue(result_error) ){
		DeleteFileL(path);
		return FALSE;
	}
	return TRUE;
}

BOOL ImgExtractDir(FOPSTRUCT *FS,HANDLE hFile,const TCHAR *srcDir,const TCHAR *srcPath,const TCHAR *dstDIR)
{
	TCHAR src[VFPS],dst[VFPS];
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	BOOL result = TRUE;

	CreateDirectoryL(dstDIR,NULL);
	CatPath(src,(TCHAR *)srcDir,srcPath);
	CatPath(NULL,src,WildCard_All);

	hFF = VFSFindFirst(src,&ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		do {
			if ( ff.cFileName[0] == '\0' ){ // 空欄エントリ名
				continue;
			}

			CatPath(src,(TCHAR *)srcPath,ff.cFileName);
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				if ( IsRelativeDirectory(ff.cFileName) ) continue;
				CatPath(dst,(TCHAR *)dstDIR,ff.cFileName);
				result = ImgExtractDir(FS,hFile,srcDir,src,dst);
			}else{
				result = ImgExtractFile(FS,hFile,srcDir,src,dstDIR);
			}
			if ( result == FALSE ) break;
		}while( IsTrue(VFSFindNext(hFF,&ff)) );
		VFSFindClose(hFF);
	}
	return result;
}
BOOL ImgExtractFile(FOPSTRUCT *FS,HANDLE hFile,const TCHAR *srcDir,const TCHAR *srcPath,const TCHAR *dstDIR)
{
	DWORD sizeL,sizeH;
	HGLOBAL hMap;
	BYTE *mem;
	const TCHAR *q;
	DWORD tmp;
	IMAGEGETEXINFO exinfo;
	ERRORCODE result;

	PeekMessageLoop(FS);
	if ( IsTrue(FS->Cancel) ) return FALSE;

	q = FindLastEntryPoint(srcPath);
	if ( IsParentDirectory(q) ) return TRUE;
	if ( VFSFullPath(exinfo.dest,(TCHAR *)q,dstDIR) == NULL ){
		FWriteErrorLogs(FS,exinfo.dest,T("Destpath"),PPERROR_GETLASTERROR);
		return FALSE;
	}
	if ( LFNfilter(&FS->opt,exinfo.dest) != NO_ERROR ) return FALSE;
	FS->progs.srcpath = srcPath;

	exinfo.Progress = (LPPROGRESS_ROUTINE)CopyProgress;
	exinfo.lpData = &FS->progs;
	exinfo.Cancel = &FS->Cancel;

	mem = (BYTE *)&exinfo;
	for ( ; ; ){
		BY_HANDLE_FILE_INFORMATION srcfinfo,dstfinfo;
		HANDLE hTFile;

		result = VFSGetArchivefileImage(INVALID_HANDLE_VALUE,hFile,
				srcDir,srcPath,&sizeL,&sizeH,&hMap,&mem);
		if ( result == ERROR_NOT_SUPPORTED ){
			return ImgExtractDir(FS,hFile,srcDir,srcPath,exinfo.dest);
		}
		if ( result == MAX32 ){
			FS->progs.info.donefiles++;

			if ( FS->progs.info.filesall ){
				AddDD(FS->progs.info.donesize.l,
					  FS->progs.info.donesize.h, sizeL,sizeH);
			}
			return TRUE; // VFSGetArchivefileImage 内で処理済み
		}

		if ( result == NO_ERROR ){
			HANDLE hDstFile;

			hDstFile = CreateFileL(exinfo.dest,GENERIC_WRITE,0,NULL,CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL,NULL);
			if ( hDstFile != INVALID_HANDLE_VALUE ){
				WriteFile(hDstFile,mem,sizeL,&tmp,NULL);
				CloseHandle(hDstFile);
				GlobalUnlock(hMap);
				GlobalFree(hMap);
				FS->progs.info.donefiles++;
				if ( FS->progs.info.filesall ){
					AddDD(FS->progs.info.donesize.l,FS->progs.info.donesize.h,
						sizeL,0);
				}
				return TRUE;
			}
			result = GetLastError();
			memset(&srcfinfo,0,sizeof(srcfinfo));
		}else{
			srcfinfo.dwFileAttributes = exinfo.ff.dwFileAttributes;
			srcfinfo.ftCreationTime = exinfo.ff.ftCreationTime;
			srcfinfo.ftLastAccessTime = exinfo.ff.ftLastAccessTime;
			srcfinfo.ftLastWriteTime = exinfo.ff.ftLastWriteTime;
			srcfinfo.ftLastWriteTime = exinfo.ff.ftLastWriteTime;
			srcfinfo.nFileSizeHigh = exinfo.ff.nFileSizeHigh;
			srcfinfo.nFileSizeLow = exinfo.ff.nFileSizeLow;
		}

		if ( (result != ERROR_ALREADY_EXISTS) &&
			(result != ERROR_FILE_EXISTS) ){
			if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
				if ( tstrchr(srcPath,'/') != NULL ){ // ファイル名が異常
					FWriteErrorLogs(FS,exinfo.dest,T("Dest open"),result);
					return TRUE;
				}
			}
			if ( result != ERROR_CANCELLED ) PPErrorBox(NULL,STR_FOP,result);
			return FALSE;
		}
		hTFile = CreateFileL(exinfo.dest,GENERIC_READ,0,NULL,
				OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		result = SameNameAction(FS,hTFile,&srcfinfo,&dstfinfo,srcPath,exinfo.dest);
		switch( result ){
			case ACTION_RETRY:
				continue;

			case ACTION_APPEND:
			case ACTION_OVERWRITE:
				// ACTION_CREATE へ
			case ACTION_CREATE:
				break;

			case ACTION_SKIP:
				FS->progs.info.EXskips++;
				return TRUE;

			case ERROR_CANCELLED:
				return ERROR_CANCELLED;

			default: // error
				FWriteErrorLogs(FS,exinfo.dest,NULL,result);
				return FALSE;
		}
		if ( dstfinfo.dwFileAttributes & OPENERRORATTRIBUTES ){
			SetFileAttributesL(exinfo.dest,FILE_ATTRIBUTE_NORMAL);
		}
		DeleteFileL(exinfo.dest);
	}
}

void ImgExtract(FOPSTRUCT *FS,const TCHAR *srcDIR,const TCHAR *dstDIR)
{
	HANDLE hFile;
	TCHAR *wp,*sp,src[VFPS];
	const TCHAR *p;

	hFile = tOpenFile(srcDIR,(const TCHAR **)&wp);
	if ( hFile == INVALID_HANDLE_VALUE ) return;

	for ( p = FS->opt.files ; *p ; p = p + tstrlen(p) + 1,FS->progs.info.mark++ ){
		if ( VFSFullPath(src,(TCHAR *)p,srcDIR) == NULL ){
			FWriteErrorLogs(FS,src,T("Srcpath"),PPERROR_GETLASTERROR);
			continue;
		}

		sp = src + (wp - srcDIR);
		*sp++ = '\0';
		if ( *sp == '\0' ){	// ディスクイメージ自体の取得
			if ( ImgExtractImage(FS,hFile,src,dstDIR) ) break;
			continue;
		}
		if ( ImgExtractFile(FS,hFile,src,sp,dstDIR) == FALSE ) break;
	}
	CloseHandle(hFile);
}

