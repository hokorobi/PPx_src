/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System
				ネットワークリソース列挙 / ストリーム列挙 / POSIX directory
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#ifdef WINEGCC
#include <errno.h>
#endif
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

//=============================================================================
// ネットワークリソース列挙
//=============================================================================
DWORD EnumCheckTick = 0;
const TCHAR EnumNetServerThreadName[] = T("Enum PC name");

DWNETOPENENUM DWNetOpenEnum;
DWNETENUMRESOURCE DWNetEnumResource;
DWNETCLOSEENUM DWNetCloseEnum;
DWNETADDCONNECTION3 DWNetAddConnection3;

LOADWINAPISTRUCT MPRDLL[] = {
	LOADWINAPI1T(WNetOpenEnum),
	LOADWINAPI1T(WNetEnumResource),
	LOADWINAPI1 (WNetCloseEnum),
	LOADWINAPI1T(WNetAddConnection3),
	{NULL, NULL}
};

BOOL LoadNetFunctions(void)
{
	if ( hMPR != NULL ) return TRUE;
	hMPR = LoadWinAPI("MPR.DLL", NULL, MPRDLL, LOADWINAPI_LOAD);
	if ( hMPR == NULL) return FALSE;
	return TRUE;
}

BOOL CheckComputerActive(const TCHAR *fname, size_t strsize)
{
	TCHAR path[VFPS];
	NETRESOURCE nr = {
			RESOURCE_GLOBALNET, RESOURCETYPE_DISK,
			RESOURCEDISPLAYTYPE_SERVER, RESOURCEUSAGE_CONTAINER,
			NULL, NULL, NULL, NULL};
	HANDLE hNet;
	ERRORCODE result;

	// Vista 以降は却って遅くなるのでチェックしない
	if ( WinType >= WINTYPE_VISTA ) return TRUE;

	if ( LoadNetFunctions() == FALSE ) return FALSE;

	memcpy(path, fname, strsize * sizeof(TCHAR));
	path[strsize] = '\0';
	nr.lpRemoteName = path;		// リソース列挙ができるか？

	result = DWNetOpenEnum(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0, &nr, &hNet);
	if ( result == NO_ERROR ){
		DWNetCloseEnum(hNet);
	}else{
		if ( result == ERROR_ACCESS_DENIED ) result = NO_ERROR;
	}
	return result == NO_ERROR;
}

// 該当PCのリソース一覧を取得
BOOL MakeComputerResourceList(FF_MC *MC, const TCHAR *fname)
{
	NETRESOURCE nr = {
			RESOURCE_GLOBALNET, RESOURCETYPE_DISK,
			RESOURCEDISPLAYTYPE_SERVER, RESOURCEUSAGE_CONTAINER,
			NULL, NULL, NULL, NULL};
	HANDLE hNet;
	char nrbuf[0x800];

	if ( LoadNetFunctions() == FALSE ) return FALSE;
	nr.lpRemoteName = (TCHAR *)fname;		// リソース列挙ができるか？
	if ( (WinType < WINTYPE_2000) || (DWNetOpenEnum(RESOURCE_GLOBALNET,
		 		RESOURCETYPE_DISK, 0, &nr, &hNet) != NO_ERROR) ){
		return FALSE;
	}
	ThInit(&MC->dirs);
	ThInit(&MC->files);
	ThAddString(&MC->dirs, T("."));
	ThAddString(&MC->dirs, T(".."));

	for ( ; ; ){
		DWORD cnt, size;
		const TCHAR *p, *q;

		cnt = 1;
		size = sizeof(nrbuf);
		if ( DWNetEnumResource(hNet, &cnt, &nrbuf, &size) != NO_ERROR ) break;
		p = ((NETRESOURCE *)nrbuf)->lpRemoteName;
		for ( ; ; ){
			q = FindPathSeparator(p);
			if ( q == NULL ) break;
			p = q + 1;
		}
		ThAddString(&MC->dirs, p);
	}
	((DWORD *)nrbuf)[3] = 0;
	GetCustData(StrX_dlf, nrbuf, sizeof(DWORD) * 4);
	if ( ((DWORD *)nrbuf)[3] & XDLF_ADMINDRIVE ){
		ThAddString(&MC->dirs, T("C$"));
	}
	ThAddString(&MC->dirs, NilStr);
	ThAddString(&MC->files, NilStr);
	MC->d_off = 0;
	MC->f_off = 0;

	DWNetCloseEnum(hNet);
	return TRUE;
}

// PC一覧を取得
void EnumNetServerMain(NETRESOURCE *nr, BOOL extend)
{
	HANDLE hNet;
	char nrbuf[0x4000];
	const TCHAR *p, *q;
	NETRESOURCE *pnr;
	DWORD cnt, size;

	if (DWNetOpenEnum( (nr == NULL) ? RESOURCE_CONTEXT : RESOURCE_GLOBALNET,
								RESOURCETYPE_DISK, 0, nr, &hNet) != NO_ERROR){
		return;
	}
	if ( nr == NULL ){
		UsePPx();
		while ( IsTrue(DeleteHistory(PPXH_NETPCNAME, NULL)) );
		FreePPx();
	}

	pnr = (NETRESOURCE *)nrbuf;
	for ( ; ; ){
		cnt = 1;
		size = sizeof(nrbuf);
		if ( DWNetEnumResource(hNet, &cnt, &nrbuf, &size) != NO_ERROR) break;
		if ( pnr->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER ){
			UsePPx();
			if ( extend == FALSE ){
				p = pnr->lpRemoteName;
				for ( ; ; ){
					q = FindPathSeparator(p);
					if ( q == NULL ) break;
					p = q + 1;
				}
				WriteHistory(PPXH_NETPCNAME, p, 0, NULL);
			}else{
				WriteHistory(PPXH_NETPCNAME, pnr->lpRemoteName, 0, NULL);
			}
			FreePPx();
		}else{
			if ( IsTrue(extend) ) EnumNetServerMain(pnr, TRUE);
		}
	}
	DWNetCloseEnum(hNet);
	return;
}

DWORD WINAPI EnumNetServerThread(LPVOID param)
{
	THREADSTRUCT threadstruct = {EnumNetServerThreadName, XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};
	PPxRegisterThread(&threadstruct);

	EnumNetServerMain(NULL, param != NULL);

	PPxUnRegisterThread();
	if ( Sm != NULL ) Sm->EnumNerServerThreadID = 0;
	EnumCheckTick = GetTickCount();
	return 0;
}

// \\+  or 2000/XPの \\ で使用する PC 列挙
void EnumNetServer(ThSTRUCT *dirs, BOOL extend)
{
	int index = 0;

	if ( !Sm->EnumNerServerThreadID ){
		// オーバーフローは無視しても問題ない
		if ( (GetTickCount() - EnumCheckTick) >= 15 * 1000 ){
			HANDLE hThread;

			if ( LoadNetFunctions() == FALSE ) return;

			#ifndef UNICODE
			if ( WinType == WINTYPE_9x ){
				EnumNetServerThread((void *)extend);
				return;
			}
			#endif
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			  EnumNetServerThread, (void *)(DWORD_PTR)extend, 0, &Sm->EnumNerServerThreadID);
			if ( hThread != NULL ){
				WaitForSingleObject(hThread, 200); // 少しだけ待ってみる
				SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
				CloseHandle(hThread);
			}
		}
	}

	UsePPx();
	for ( ; ; ){
		const TCHAR *hisp;

		hisp = EnumHistory(PPXH_NETPCNAME, index++);
		if ( !hisp ) break;
		AddDriveList(dirs, hisp);
	}
	FreePPx();
	return;
}

//=============================================================================
// ストリーム列挙
//	将来は FileStreamInformation (NtQueryInformationFile)を使うことも検討。
//=============================================================================
void FindFirstStream(HANDLE hFile, FF_STREAM *fs, const TCHAR *filename)
{
	TCHAR *bp;

	fs->step = FFSTEP_THIS;
	fs->context = NULL;
	fs->hFile = hFile;

	bp = FindLastEntryPoint(filename);
	fs->basep = fs->basename + wsprintf( fs->basename, T("..\\%s"), bp );
}

BOOL FindNextStream(FF_STREAM *fs, WIN32_FIND_DATA *findfile)
{
	WIN32_STREAM_ID stid;
	DWORD size, reads;
	WCHAR wname[VFPS];

	if ( fs->step < FFSTEP_ENTRY ) return FFStepInfo(findfile, &fs->step);
	for ( ; ; ){
		size = ToSIZE32_T((LPBYTE)&stid.cStreamName - (LPBYTE)&stid);
		if ( FALSE == BackupRead(fs->hFile, (LPBYTE)&stid,
				size, &reads, FALSE, FALSE, &fs->context) ){
			break;
		}
		if ( reads < size ) break;
		if ( FALSE == BackupRead(fs->hFile, (LPBYTE)&wname,
				stid.dwStreamNameSize, &reads, FALSE, FALSE, &fs->context) ){
			break;
		}
		if ( reads < stid.dwStreamNameSize ) break;

		if ( stid.dwStreamNameSize ){
			TCHAR *p;

			wname[stid.dwStreamNameSize >> 1] = '\0';
			#ifdef UNICODE
				tstrcpy(fs->basep, wname);
			#else
				UnicodeToAnsi(wname, fs->basep, VFPS - (fs->basep - fs->basename));
			#endif
			p = tstrrchr(fs->basep, ':');
			if ( (p != NULL) && (tstrcmp(p, T(":$DATA")) == 0) ) *p = '\0';
			fs->basename[MAX_PATH - 1] = '\0';

			tstrcpy(findfile->cFileName, fs->basename);

			SetDummyFindData(findfile);
			findfile->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			findfile->nFileSizeHigh = stid.Size.u.HighPart;
			findfile->nFileSizeLow = stid.Size.u.LowPart;
		}

		BackupSeek(fs->hFile, *(DWORD *)(&stid.Size),
				*((DWORD *)(&stid.Size) + 1), &reads, &reads, &fs->context);
		if ( stid.dwStreamNameSize == 0 ) continue;
		return TRUE;
	}
	SetLastError(ERROR_NO_MORE_FILES);
	return FALSE;
}

void FindCloseStream(FF_STREAM *fs)
{
	WIN32_STREAM_ID stid;
	DWORD reads;

	if ( fs->context != NULL ){
		BackupRead(fs->hFile, (LPBYTE)&stid, 0, &reads, TRUE, FALSE, &fs->context);
	}
	CloseHandle(fs->hFile);
}

//=============================================================================
// POSIX directory
//=============================================================================
#ifdef USESLASHPATH
#ifdef WINEGCC
// time_t から FILETIME に変換する
void FiletToFileTime(const time_t *timet, FILETIME *ftime)
{
	DDmul(*timet, 10000000, &ftime->dwLowDateTime, &ftime->dwHighDateTime);
	if ( sizeof(time_t) > sizeof(int) ){
		ftime->dwHighDateTime += (*timet >> 32) * 10000000;
	}
	AddDD(ftime->dwLowDateTime, ftime->dwHighDateTime, 0xd53e8000, 0x19db1de);
}

BOOL NextSlashDir(FF_SLASHDIR *sdptr, WIN32_FIND_DATA *findfile)
{
	struct dirent *entry;
	struct stat status;
#ifndef UNICODE
	WCHAR filenameW[VFPS];
#endif
	entry = readdir(sdptr->OpenedDir);
	if ( entry == NULL ) return FALSE;

	findfile->nFileSizeHigh	= 0;
	findfile->nFileSizeLow	= 0;
	findfile->cAlternateFileName[0] = '\0';
	findfile->dwFileAttributes = 0;

#ifdef UNICODE
	MultiByteToWideCharU8(CP_UTF8, 0, entry->d_name, -1, findfile->cFileName, VFPS);
#else
	MultiByteToWideCharU8(CP_UTF8, 0, entry->d_name, -1, filenameW, VFPS);
	UnicodeToAnsi(filenameW, findfile->cFileName, MAX_PATH);
#endif
	strcpy(sdptr->pathlast, entry->d_name);

	if ( stat(sdptr->path, &status) != -1 ){
		findfile->dwReserved0 = status.st_mode;
		if ( S_ISDIR(status.st_mode) ){
			setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY);
		}else{
			if ( !S_ISREG(status.st_mode) ){
				setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DEVICE);
			}
			findfile->nFileSizeLow = status.st_size;
#ifdef _WIN64
			if ( sizeof(status.st_size) > 4 ){
				findfile->nFileSizeHigh = status.st_size >> 32;
			}
#endif
		}
		if ( S_ISCHR(status.st_mode) || S_ISBLK(status.st_mode) ){
			setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_DEVICE);
		}
		if ( S_ISLNK(status.st_mode) ){
			setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_REPARSE_POINT);
		}
		if ( !(status.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ){
			setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_READONLY);
		}
		if ( (entry->d_name[0] == '.') &&
			(entry->d_name[1] != '\0') &&
			(entry->d_name[1] != '.') ){
			setflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_HIDDEN);
		}
		FiletToFileTime(&status.st_atime, &findfile->ftLastAccessTime);
		FiletToFileTime(&status.st_mtime, &findfile->ftLastWriteTime);
		FiletToFileTime(&status.st_ctime, &findfile->ftCreationTime);
	}else{
		findfile->dwReserved0 = 0;
		findfile->ftCreationTime.dwLowDateTime		= 0;
		findfile->ftCreationTime.dwHighDateTime		= 0;
		findfile->ftLastAccessTime.dwLowDateTime	= 0;
		findfile->ftLastAccessTime.dwHighDateTime	= 0;
		findfile->ftLastWriteTime.dwLowDateTime		= 0;
		findfile->ftLastWriteTime.dwHighDateTime	= 0;
	}
	return TRUE;
}

ERRORCODE FirstSlashDir(FF_SLASHDIR *sdptr, const TCHAR *dir, WIN32_FIND_DATA *findfile)
{
#ifndef UNICODE
	WCHAR dirW[VFPS];

	AnsiToUnicode(dir, dirW, VFPS);
	UnicodeToUtf8(dirW, sdptr->path, VFPS);
#else
	UnicodeToUtf8(dir, sdptr->path, VFPS);
#endif
	sdptr->OpenedDir = opendir(sdptr->path);
	if ( NULL != sdptr->OpenedDir ){
		sdptr->pathlast = sdptr->path + strlen(sdptr->path);
		*sdptr->pathlast++ = '/';
		if ( IsTrue(NextSlashDir(sdptr, findfile)) ) return NO_ERROR;
		return ERROR_NO_MORE_FILES;
	}
	switch ( errno ){
		case EACCES:
			return ERROR_ACCESS_DENIED;
		case EBADF:
			return ERROR_INVALID_DRIVE;
		case ENOENT:
		case ENOTDIR:
			return ERROR_DIRECTORY;
		case ENOMEM:
			return ERROR_OUTOFMEMORY;
		case ENFILE:
		case EMFILE:
			return ERROR_OUTOFMEMORY;
		default:
			return ERROR_PATH_NOT_FOUND;
	}
}

void CloseSlashDir(FF_SLASHDIR *sdptr)
{
	closedir( sdptr->OpenedDir );
}
#else
ERRORCODE FirstSlashDir(FF_SLASHDIR *sdptr, const TCHAR *dir, WIN32_FIND_DATA *findfile)
{
	TCHAR buf[VFPS], *p;

	buf[0] = 'C';
	buf[1] = ':';
	p = buf + 2;
	tstrcpy(p, dir);
	for ( ;; ){
		p = tstrchr(p, '/');
		if ( p == NULL ) break;
		*p++ = '\\';
	}
	sdptr->hFF = FindFirstFile(buf, findfile);
	if ( sdptr->hFF != INVALID_HANDLE_VALUE ){	// 成功
		return NO_ERROR;
	}
	return GetLastError();
}
BOOL NextSlashDir(FF_SLASHDIR *sdptr, WIN32_FIND_DATA *findfile)
{
	return FindNextFile(sdptr->hFF, findfile);
}
void CloseSlashDir(FF_SLASHDIR *sdptr)
{
	FindClose(sdptr->hFF);
}
#endif // WINEGCC
#endif // USESLASHPATH
