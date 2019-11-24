/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作, *file link/junc
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <winioctl.h>
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_FOP.H"
#pragma hdrstop

ValueWinAPI(CreateHardLink) = NULL;
DefineWinAPI(BOOL, CreateSymbolicLink, (LPCTSTR lpSymlinkFileName, LPCTSTR lpTargetFileName, DWORD dwflags)) = NULL;

VFSDLL ERRORCODE PPXAPI CreateJunction(const TCHAR *FileName, const TCHAR *ExistingFileName, SECURITY_ATTRIBUTES *sa)
{
	HANDLE hFile;
	DWORD size;
	REPARSE_DATA_IOBUFFER rdio;
	TCHAR tmpname[VFPS];
#ifndef UNICODE
	char namebuf[VFPS];
#endif

	if ( tstrlen(ExistingFileName) == 3 ){ // ジャンクション元がルート
		wsprintf(tmpname, T("%s\\%c"), FileName, *ExistingFileName);
		FileName = tmpname;
	}
#ifdef UNICODE
	CatPath(rdio.ReparseGuid.PathBuffer, T("\\??"), ExistingFileName);
#else
	CatPath(namebuf, T("\\??"), ExistingFileName);
	AnsiToUnicode(namebuf, rdio.ReparseGuid.PathBuffer, MAX_PATH);
#endif
	if ( CreateDirectoryL(FileName, sa) == FALSE ) goto error;
	hFile = CreateFileL(FileName, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		RemoveDirectoryL(FileName);
		goto error;
	}

	rdio.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	rdio.Reserved = 0;
	rdio.ReparseGuid.SubstituteNameOffset = 0;
	rdio.ReparseGuid.SubstituteNameLength =
			(WORD)(strlenW(rdio.ReparseGuid.PathBuffer) * sizeof(WCHAR));
	rdio.ReparseGuid.PrintNameOffset =
			(WORD)(rdio.ReparseGuid.SubstituteNameLength + sizeof(WCHAR));
	rdio.ReparseGuid.PrintNameLength = 0;
	*(WCHAR *)(BYTE *)((BYTE *)rdio.ReparseGuid.PathBuffer +
			rdio.ReparseGuid.PrintNameOffset) = '\0';
	rdio.ReparseDataLength = (WORD)( (sizeof(WORD) * 4) +
			(rdio.ReparseGuid.PrintNameOffset + sizeof(WORD)) );
	size = sizeof(DWORD) + sizeof(WORD) * 2 + rdio.ReparseDataLength;
	if ( DeviceIoControl(hFile, FSCTL_SET_REPARSE_POINT,
			&rdio, size, NULL, 0, &size, NULL) == FALSE ){
		CloseHandle(hFile);
		RemoveDirectoryL(FileName);
		goto error;
	}
	CloseHandle(hFile);
	return NO_ERROR;
error:
	return GetLastError();
}

ERRORCODE FOPMakeShortCut(const TCHAR *src, TCHAR *dst, BOOL directory, BOOL test)
{
	TCHAR linkpath[VFPS], linkpathdir[VFPS], *p;

	tstrcpy(linkpath, dst);
	p = VFSFindLastEntry(linkpath);
	if ( (p > linkpath) && (*p != '\\') ) p--; // c:\ の時の補正
	*p = '\0';
	tstrcpy(linkpathdir, linkpath);
	*p = '\\';
	if ( directory == FALSE ){ // ファイル名…拡張子or末尾
		p += FindExtSeparator(p);
	}else{ // ディレクトリ…末尾
		p += tstrlen(p);
	}
	tstrcpy(p, StrShortcutExt);
	if ( test == FALSE ){
		if ( FAILED(MakeShortCut(src, linkpath, linkpathdir)) ){
			return ERROR_ACCESS_DENIED;
		}
	}else{
		DWORD attr;

		tstrcpy(dst, linkpath);
		attr = GetFileAttributesL(linkpath);
		if ( attr != BADATTR ){
			return ERROR_ALREADY_EXISTS;
		}
	}
	return NO_ERROR;
}

ERRORCODE FopCreateSymlink(const TCHAR *FileName, const TCHAR *ExistingFileName, DWORD flags)
{
	if ( DCreateSymbolicLink == NULL ){
		GETDLLPROCT(hKernel32, CreateSymbolicLink);
		if ( DCreateSymbolicLink == NULL ) return GetLastError();
	}
	SetLastError(NO_ERROR);
	if ( IsTrue(DCreateSymbolicLink(FileName, ExistingFileName, flags)) ){
		ERRORCODE err = GetLastError();

		if ( err != ERROR_PRIVILEGE_NOT_HELD ) return NO_ERROR; // Windows7 PPx64 で FALSE にならないので対策
		return err;
	}
	return GetLastError();
}
