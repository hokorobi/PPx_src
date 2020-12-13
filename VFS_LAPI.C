/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System	極長パス用 WINAPI
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

#define EXTENDVFPS (VFPS + 6) // \\?… 変換に必要な大きさ

DWORD X_fstff;
impFindFirstFile DFindFirstFile = LoadFindFirstFile;
DefineWinAPI(HANDLE, FindFirstFileEx, (LPCTSTR lpFileName, DWORD fInfoLevelId, LPVOID lpFindFileData, DWORD fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)) = NULL;

#ifndef FindExSearchNameMatch
	#define FindExSearchNameMatch 0
#endif
#ifndef FindExInfoBasic
	#define FindExInfoBasic 1
#endif
#ifndef FIND_FIRST_EX_CASE_SENSITIVE
	#define FIND_FIRST_EX_CASE_SENSITIVE 1
#endif
#ifndef FIND_FIRST_EX_LARGE_FETCH
	#define FIND_FIRST_EX_LARGE_FETCH 2
#endif

#ifdef UNICODE
#define DMoveFileW MoveFileW
#define DCreateDirectoryW CreateDirectoryW
#define DCreateDirectoryExW CreateDirectoryExW
#define DRemoveDirectoryW RemoveDirectoryW
#define DDeleteFileW DeleteFileW
#define DSetFileAttributesW SetFileAttributesW
#define DGetFileAttributesW GetFileAttributesW
#define DCreateFileW CreateFileW

#define CheckWideFunctionMacro(funcname, filename, errresult) if ( FALSE == CheckWideFunctionW(filename)){ return errresult; }

BOOL CheckWideFunctionW(const TCHAR *filename)
{
	ERRORCODE error;

	error = GetLastError();
	if ( (error != ERROR_FILENAME_EXCED_RANGE) &&
		 (tstrlen(filename) < MAX_PATH) ){
		SetLastError(error);
		return FALSE;
	}
	return TRUE;
}
#else
DefineWinAPI(BOOL, MoveFileW, (LPCWSTR, LPCWSTR)) = INVALID_VALUE(impMoveFileW);
DefineWinAPI(BOOL, CreateDirectoryW, (LPCWSTR, LPSECURITY_ATTRIBUTES)) = INVALID_VALUE(impCreateDirectoryW);
DefineWinAPI(BOOL, CreateDirectoryExW, (LPCWSTR, LPCWSTR, LPSECURITY_ATTRIBUTES)) = INVALID_VALUE(impCreateDirectoryExW);
DefineWinAPI(BOOL, RemoveDirectoryW, (LPCWSTR)) = INVALID_VALUE(impRemoveDirectoryW);
DefineWinAPI(BOOL, DeleteFileW, (LPCWSTR)) = INVALID_VALUE(impDeleteFileW);
DefineWinAPI(BOOL, SetFileAttributesW, (LPCWSTR, DWORD)) = INVALID_VALUE(impSetFileAttributesW);
DefineWinAPI(DWORD, GetFileAttributesW, (LPCWSTR)) = INVALID_VALUE(impGetFileAttributesW);
DefineWinAPI(HANDLE, CreateFileW, (LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)) = INVALID_VALUE(impCreateFileW);
DefineWinAPI(HANDLE, FindFirstFileW, (LPCWSTR, LPWIN32_FIND_DATAW)) = INVALID_VALUE(impFindFirstFileW);

#ifndef UNICODE
DefineWinAPI(DWORD, GetFileAttributesExA, (LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID)) = INVALID_VALUE(impGetFileAttributesExA);
#endif

#define CheckWideFunctionMacro(funcname, filename, errresult) if ( FALSE == CheckWideFunctionA((void *)&(D ## funcname), #funcname, filename)){ return errresult; }

BOOL CheckWideFunctionA(void **function, const char *funcname, const TCHAR *filename)
{
	ERRORCODE error;

	error = GetLastError();
	if ( (error != ERROR_FILENAME_EXCED_RANGE) &&
		 (tstrlen(filename) < MAX_PATH) ){
		SetLastError(error);
		return FALSE;
	}
	if ( *function == INVALID_HANDLE_VALUE ){
		*function = GetProcAddress(hKernel32, funcname);
	}
	if ( *function == NULL ){
		SetLastError(error);
		return FALSE;
	}
	return TRUE;
}
#endif

#if !defined(_WIN64) || defined(WINEGCC)
DefineWinAPI(BOOL, CopyFileExW, (LPCWSTR ExistingFileName, LPCWSTR NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, LPBOOL Cancel, DWORD CopyFlags)) = INVALID_VALUE(impCopyFileExW);
DefineWinAPI(BOOL, MoveFileWithProgressW, (LPCWSTR ExistingFileName, LPCWSTR NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, DWORD Flags)) = INVALID_VALUE(impMoveFileWithProgressW);

BOOL LoadWideFunction(FARPROC *function, char *funcname)
{
	if ( *function == INVALID_VALUE(FARPROC) ){
		*function = GetProcAddress(hKernel32, funcname);
	}
	if ( *function == NULL ){
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return FALSE;
	}
	return TRUE;
}
#endif

BOOL USEFASTCALL ConvertWideFileName(WCHAR *widename, const TCHAR *multiname)
{
	// C:\name\name		→	\\?\C:\name\name
	// \\name\name\name	→	\\?\UNC\name\name\name
	*widename++ = '\\';
	*widename++ = '\\';
	*widename++ = '?';
	*widename++ = '\\';
	if ( *multiname == '\\' ){
		multiname++;
		*widename++ = 'U';
		*widename++ = 'N';
		*widename++ = 'C';
	}
#ifndef UNICODE
	if ( 0 == MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
							multiname, -1, widename, VFPS) ){
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return FALSE;
	}
#else
	{
		size_t size;
		size = tstrlen(multiname);
		if ( size >= VFPS ){
			SetLastError(ERROR_FILENAME_EXCED_RANGE);
			return FALSE;
		}
		memcpy(widename, multiname, TSTROFF(size + 1));
	}
#endif
	return TRUE;
}

VFSDLL BOOL PPXAPI CopyFileExL(const TCHAR *ExistingFileName, const TCHAR *NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, LPBOOL Cancel, DWORD CopyFlags)
{
	WCHAR wideexistingname[EXTENDVFPS];
	WCHAR widenewname[EXTENDVFPS];
#ifndef _WIN64
	if (
	#ifndef UNICODE
		(WinType == WINTYPE_9x) ||
	#endif
		(FALSE == LoadWideFunction((FARPROC *)&DCopyFileExW, "CopyFileExW")) ){
		if ( ProgressRoutine != NULL ){
			SetLastError(ERROR_INVALID_FUNCTION);
			return FALSE;
		}
		return CopyFile(ExistingFileName, NewFileName,
				(CopyFlags & COPY_FILE_FAIL_IF_EXISTS) ? TRUE : FALSE);
	}
#else
	#define DCopyFileExW CopyFileExW
#endif
	if ( FALSE == ConvertWideFileName(wideexistingname, ExistingFileName) ){
		return FALSE;
	}
	if ( FALSE == ConvertWideFileName(widenewname, NewFileName) ){
		return FALSE;
	}
	return DCopyFileExW(wideexistingname, widenewname,
			ProgressRoutine, Data, Cancel, CopyFlags);
}

VFSDLL BOOL PPXAPI MoveFileWithProgressL(const TCHAR *ExistingFileName, const TCHAR *NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, DWORD Flags)
{
	BOOL result;
	WCHAR wideexistingname[EXTENDVFPS];
	WCHAR widenewname[EXTENDVFPS];

#ifndef _WIN64
	if ( FALSE == LoadWideFunction(
			(FARPROC *)&DMoveFileWithProgressW, "MoveFileWithProgressW") ){
		result = MoveFile(ExistingFileName, NewFileName);
		if ( IsTrue(result) ) return result;
	}else
#else
	#define DMoveFileWithProgressW MoveFileWithProgressW
#endif
	{
		if ( FALSE == ConvertWideFileName(wideexistingname, ExistingFileName) ){
			return FALSE;
		}
		if ( FALSE == ConvertWideFileName(widenewname, NewFileName) ){
			return FALSE;
		}
		result = DMoveFileWithProgressW(wideexistingname, widenewname,
				ProgressRoutine, Data, Flags);
		if ( IsTrue(result) ) return result;
	}

	if ( !memcmp(ExistingFileName, StrAux, StrAuxSize) ){
		ERRORCODE errorcode;
		const TCHAR *newptr;

		newptr = NewFileName;
		if ( !memcmp(newptr, StrAux, StrAuxSize) ){
			newptr += 4;
			if ( *newptr == '/' ){
				while ( *newptr == '/' ) newptr++;
				while ( (*newptr != '/') && (*newptr != '\0') ) newptr++;
				if ( *newptr == '/' ) newptr++;
			}else{
				while ( (*newptr != '\\') && (*newptr != '\0') ) newptr++;
				if ( *newptr == '\\' ) newptr++;
			}
		}
		errorcode = FileOperationAux(T("rename"), ExistingFileName, NilStr, newptr);
		if ( errorcode == NO_ERROR ) return TRUE;
		SetLastError(errorcode);
		return FALSE;
	}

	if ( !memcmp(ExistingFileName, StrFtp, StrFtpSize) ){
		ERRORCODE errorcode;

		errorcode = MoveFtpFile(ExistingFileName, NewFileName);
		if ( errorcode == NO_ERROR ) return TRUE;
		SetLastError(errorcode);
		return FALSE;
	}
	{
		ERRORCODE errorcode;

		errorcode = GetLastError();
		if ( (errorcode != ERROR_ACCESS_DENIED) &&
			 (errorcode != ERROR_FILE_NOT_FOUND) ){ // 末尾ピリオドの時に出る
			if ( (tstrlen(ExistingFileName) < MAX_PATH) &&
				 (tstrlen(NewFileName) < MAX_PATH) ){
				SetLastError(errorcode);
				return result;
			}
		}
	}
#ifndef UNICODE
	if ( FALSE == LoadWideFunction((FARPROC *)&DMoveFileW, "MoveFileW") ){
		return FALSE;
	}
#endif
	if ( FALSE == ConvertWideFileName(wideexistingname, ExistingFileName) ){
		return FALSE;
	}
	if ( FALSE == ConvertWideFileName(widenewname, NewFileName) ){
		return FALSE;
	}
	return DMoveFileW(wideexistingname, widenewname);
}

VFSDLL BOOL PPXAPI CreateDirectoryL(const TCHAR *FileName, LPSECURITY_ATTRIBUTES sa)
{
	WCHAR widename[EXTENDVFPS];
	BOOL result;

	result = CreateDirectory(FileName, sa);
	if ( IsTrue(result) ){
		SHChangeNotify(SHCNE_MKDIR, SHCNF_PATH, FileName, NULL);
		return TRUE;
	}

	CheckWideFunctionMacro(CreateDirectoryW, FileName, FALSE);

	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return FALSE;
	}
	return DCreateDirectoryW(widename, sa);
}

BOOL CreateDirectoryExL(const TCHAR *FileName, const TCHAR *FileName2, LPSECURITY_ATTRIBUTES sa)
{
	WCHAR widename[EXTENDVFPS];
	WCHAR widename2[EXTENDVFPS];
	BOOL result;
	ERRORCODE error;

	result = CreateDirectoryEx(FileName, FileName2, sa);
	if ( IsTrue(result) ){
		SHChangeNotify(SHCNE_MKDIR, SHCNF_PATH, FileName, NULL);
		return TRUE;
	}

	error = GetLastError();
	if ( (error != ERROR_FILENAME_EXCED_RANGE) &&
		 (tstrlen(FileName) < MAX_PATH) && (tstrlen(FileName2) < MAX_PATH) ){
		SetLastError(error);
		return FALSE;
	}
	SetLastError(ERROR_FILENAME_EXCED_RANGE);
	CheckWideFunctionMacro(CreateDirectoryExW, FileName, FALSE);
	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return FALSE;
	}
	if ( FALSE == ConvertWideFileName(widename2, FileName2) ){
		return FALSE;
	}
	return DCreateDirectoryExW(widename, widename2, sa);
}

VFSDLL BOOL PPXAPI RemoveDirectoryL(const TCHAR *FileName)
{
	WCHAR widename[EXTENDVFPS];
	BOOL result;

	result = RemoveDirectory(FileName);
	if ( IsTrue(result) ) return TRUE;
	CheckWideFunctionMacro(RemoveDirectoryW, FileName, FALSE);

	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return FALSE;
	}
	return DRemoveDirectoryW(widename);
}

VFSDLL BOOL PPXAPI DeleteFileL(const TCHAR *FileName)
{
	WCHAR widename[EXTENDVFPS];
	BOOL result;
	ERRORCODE errorcode;

	result = DeleteFile(FileName);
	if ( IsTrue(result) ) return result;
	errorcode = GetLastError();
	if ( (errorcode != ERROR_ACCESS_DENIED) &&
		 (errorcode != ERROR_FILE_NOT_FOUND) && // Fileがない→末尾ピリオドかも
		 (errorcode != ERROR_FILENAME_EXCED_RANGE) &&
		 (tstrlen(FileName) < MAX_PATH) ){
		SetLastError(errorcode);
		return FALSE;
	}
#ifndef UNICODE
	if ( FALSE == LoadWideFunction((FARPROC *)&DDeleteFileW, "DeleteFileW") ){
		return FALSE;
	}
#endif
	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return FALSE;
	}
	return DDeleteFileW(widename);
}

VFSDLL BOOL PPXAPI SetFileAttributesL(const TCHAR *FileName, DWORD attributes)
{
	WCHAR widename[EXTENDVFPS];
	BOOL result;

	result = SetFileAttributes(FileName, attributes);
	if ( IsTrue(result) ) return result;

	CheckWideFunctionMacro(SetFileAttributesW, FileName, FALSE);

	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return FALSE;
	}
	return DSetFileAttributesW(widename, attributes);
}

VFSDLL DWORD PPXAPI GetFileAttributesL(const TCHAR *FileName)
{
	WCHAR widename[EXTENDVFPS];
	DWORD result;

	result = GetFileAttributes(FileName);
	if ( result != BADATTR ) return result;

	CheckWideFunctionMacro(GetFileAttributesW, FileName, BADATTR);

	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return BADATTR;
	}
	return DGetFileAttributesW(widename);
}

VFSDLL HANDLE PPXAPI CreateFileL(const TCHAR *FileName, DWORD Access, DWORD ShareMode, LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDisposition, DWORD FlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE hFile;
	WCHAR widename[EXTENDVFPS];

	hFile = CreateFile(FileName, Access, ShareMode, SecurityAttributes,
			CreationDisposition, FlagsAndAttributes, hTemplateFile);
	if ( hFile != INVALID_HANDLE_VALUE ) return hFile;

	CheckWideFunctionMacro(CreateFileW, FileName, INVALID_HANDLE_VALUE);

	if ( FALSE == ConvertWideFileName(widename, FileName) ){
		return INVALID_HANDLE_VALUE;
	}
	return DCreateFileW(widename, Access, ShareMode, SecurityAttributes,
			CreationDisposition, FlagsAndAttributes, hTemplateFile);
}

HANDLE USEFASTCALL FindFirstFileLs(const TCHAR *dir, WIN32_FIND_DATA *findfile)
{
	WCHAR widename[EXTENDVFPS];
#ifndef UNICODE
	WIN32_FIND_DATAW findfilew;
	HANDLE hFile;
#endif
	if ( FALSE == ConvertWideFileName(widename, dir) ){
		return INVALID_HANDLE_VALUE;
	}
#ifndef UNICODE
	if ( FALSE == LoadWideFunction((FARPROC *)&DFindFirstFileW, "FindFirstFileW") ){
		return INVALID_HANDLE_VALUE;
	}
	hFile = DFindFirstFileW(widename, &findfilew);
	if ( hFile != INVALID_HANDLE_VALUE ){
		memcpy(findfile, &findfilew,
				(4 * sizeof(DWORD)) + (3 * sizeof(FILETIME)));
		WideCharToMultiByte(CP_ACP, 0,
				findfilew.cFileName, -1, findfile->cFileName, VFPS, NULL, NULL);
		WideCharToMultiByte(CP_ACP, 0,
				findfilew.cAlternateFileName, -1,
				findfile->cAlternateFileName, VFPS, NULL, NULL);
	}
	return hFile;
#else
	return DFindFirstFile(widename, findfile);
#endif
}

VFSDLL HANDLE PPXAPI FindFirstFileL(_In_z_ const TCHAR *dir,_Out_ WIN32_FIND_DATA *findfile)
{
	HANDLE hFF;
	ERRORCODE errorcode;

	hFF = DFindFirstFile(dir, findfile);
	if ( hFF != INVALID_HANDLE_VALUE ) return hFF;

	errorcode = GetLastError();
	if ( (errorcode == ERROR_FILENAME_EXCED_RANGE) || (tstrlen(dir) >= MAX_PATH) ){
		return FindFirstFileLs(dir, findfile);
	}else{
		SetLastError(errorcode);
		return INVALID_HANDLE_VALUE;
	}
}

HANDLE WINAPI FindFirstFileUseEx(const TCHAR *lpFileName, WIN32_FIND_DATA *FindFileData)
{
	return DFindFirstFileEx(lpFileName, (X_fstff >= 2) ? FindExInfoBasic : 0,
			FindFileData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
}

HANDLE WINAPI LoadFindFirstFile(const TCHAR *lpFileName, WIN32_FIND_DATA *FindFileData)
{
	GETDLLPROCT(hKernel32, FindFirstFile);
	if ( WinType >= WINTYPE_7 ){
		X_fstff = GetCustDword(T("X_fstff"), 1);
		if ( X_fstff ){
			GETDLLPROCT(hKernel32, FindFirstFileEx);
			if ( DFindFirstFileEx != NULL ){
				DFindFirstFile = FindFirstFileUseEx;
			}
		}
	}
	return DFindFirstFile(lpFileName, FindFileData);
}


// WIN32_FIND_DATA と WIN32_FILE_ATTRIBUTE_DATA は共通しているのでそのまま処理
// C:\ を指定したとき、
// Win10 FILE_ATTRIBUTE_VIRTUAL | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN
// NT4 SP6a FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN
BOOL WINAPI GetFileSTAT(const TCHAR *lpFileName, FILE_STAT_DATA *FindFileData)
{
	BOOL result;
	ERRORCODE errorcode;

#ifdef UNICODE
	WCHAR widename[EXTENDVFPS];

	result = GetFileAttributesExW(lpFileName, GetFileExInfoStandard, FindFileData);
	if ( IsTrue(result) ) return result;

	errorcode = GetLastError();
	if ( (errorcode != ERROR_ACCESS_DENIED) &&
		 (errorcode != ERROR_FILENAME_EXCED_RANGE) &&
		 (tstrlen(lpFileName) < MAX_PATH) ){
		SetLastError(errorcode);
		return FALSE;
	}

	if ( FALSE == ConvertWideFileName(widename, lpFileName) ){
		return FALSE;
	}
	return GetFileAttributesExW(widename, GetFileExInfoStandard, FindFileData);
#else
	HANDLE hFF;

	if ( DGetFileAttributesExA == INVALID_VALUE(impGetFileAttributesExA) ){
		GETDLLPROC(hKernel32, GetFileAttributesExA);
	}
	if ( DGetFileAttributesExA != NULL ){
		result = DGetFileAttributesExA(lpFileName, GetFileExInfoStandard, FindFileData);
		if ( result == TRUE ) return result;

		errorcode = GetLastError();
		if ( (errorcode == ERROR_FILENAME_EXCED_RANGE) || (tstrlen(lpFileName) >= MAX_PATH) ){
			hFF = FindFirstFileLs(lpFileName, FindFileData);
			if ( hFF != INVALID_HANDLE_VALUE ){
				FindClose(hFF);
				return TRUE;
			}
			return FALSE;
		}else{
			SetLastError(errorcode);
			return FALSE;
		}
	}
	// Windows 95 の場合、GetFileAttributesEx がない。Ex は 98/NT4 以降
	hFF = FindFirstFileL(lpFileName, FindFileData);
	if ( hFF != INVALID_HANDLE_VALUE ){
		FindClose(hFF);
		return TRUE;
	}
	return FALSE;
#endif
}

