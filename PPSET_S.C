/*-----------------------------------------------------------------------------
	Paper Plane xUI	 setup wizard								～ Sub ～
-----------------------------------------------------------------------------*/
#include "PPSETUP.H"
#pragma hdrstop

const TCHAR Str_PPxRegValue[] = T(XNAME) T("C");

BYTE *CustP;			// カスタマイズ領域

ValueWinAPI(SHGetFolderPath) = NULL;

BOOL GetWinAppDir(int csidl, TCHAR *path)
{
	IMalloc *pMalloc;
	ITEMIDLIST *pidl;

	path[0] = '\0';
	if ( DSHGetFolderPath != NULL ){
		if ( SUCCEEDED(DSHGetFolderPath(NULL, csidl, NULL, 0, path)) ){
			return TRUE;
		}
	}

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if ( SUCCEEDED(SHGetSpecialFolderLocation(NULL, csidl, &pidl)) ){
		SHGetMalloc(&pMalloc);
		SHGetPathFromIDList(pidl, path);
		pMalloc->lpVtbl->Free(pMalloc, pidl);
		pMalloc->lpVtbl->Release(pMalloc);
		CoUninitialize();
		return TRUE;
	}
	CoUninitialize();
	return FALSE;
}

void USEFASTCALL SMessage(const TCHAR *str)
{
	MessageBox(GetFocus(), str, msgboxtitle, MB_OK);
}

typedef ERRORCODE (PPXAPI *impPP_ExtractMacro)(HWND hWnd, PPXAPPINFO *ParentInfo, POINT *pos, const TCHAR *param, TCHAR *extract, int flag);

ERRORCODE Execute_ExtractMacro(const TCHAR *param, BOOL error_dialog)
{
	HMODULE hDLL;
	impPP_ExtractMacro DPP_ExtractMacro;
	ERRORCODE result;

	hDLL = LoadLibrary(T(COMMONDLL));
	if ( hDLL == NULL ){
		if ( error_dialog ){
			SMessage(MessageStr[MSG_DLLLOADERROR]);
		}else{
			WriteResult(MessageStr[MSG_DLLLOADERROR], RESULT_NORMAL);
		}
		return ERROR_FILE_NOT_FOUND;
	}
	GETDLLPROC(hDLL, PP_ExtractMacro);

	result = DPP_ExtractMacro(GetActiveWindow(), NULL, NULL, param, NULL, 0);

	FreeLibrary(hDLL);
	return result;
}

void WriteResult(const TCHAR *str, int add)
{
	if ( (add == RESULT_ALLDONE) && (SetupResult != SRESULT_NOERROR) ){
		const TCHAR *mes;

		if ( SetupResult == SRESULT_ACCESSERROR ){
			mes = MessageStr[MSG_FAILACCESS];
		}else{
			mes = MessageStr[MSG_FAILCOPY];
		}
		WriteResult(mes, RESULT_NORMAL);
		SMessage(mes);
		return;
	}

	SendMessage(hResultWnd, EM_SETSEL, EC_LAST, EC_LAST);
	SendMessage(hResultWnd, EM_REPLACESEL, 0, (LPARAM)str);
	SendMessage(hResultWnd, EM_SETSEL, EC_LAST, EC_LAST);
	SendMessage(hResultWnd, EM_REPLACESEL, 0, (LPARAM)MessageStr[add]);
	SendMessage(hResultWnd, EM_SCROLL, SB_LINEDOWN, 0);

	if ( (add == RESULT_FAULT) && (SetupResult == SRESULT_NOERROR) ){
		SetupResult = SRESULT_FAULT;
	}
}
/*-----------------------------------------------------------------------------
	filename で指定されたファイルをメモリに読み込む。
	margin:		ファイルの末尾以降 0 padding する長さ
	image		読み込み先。NULL なら Systemheap を用い内部で確保。
	imagesize	読み込んだイメージの大きさ。image != NULL なら確保済みの
				メモリの大きさをいれておく。
	filesize	ファイルのサイズ。4G over なら MAX32(0xffffffff) が入る
				NULL 指定可
-----------------------------------------------------------------------------*/
ERRORCODE LocalLoadFileImage(const TCHAR *filename, DWORD margin, char **image, DWORD *imagesize)
{
	int		heap = 0;			// ヒープを確保したなら !0
	HANDLE	hFile;
	DWORD	sizeL, sizeH;		// ファイルの大きさ
	DWORD	result;
										// ファイルを開く ---------------------
	hFile = CreateFile(filename,
			GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return GetLastError();

										// ファイルサイズの確認 ---------------
	sizeL = GetFileSize(hFile, &sizeH);
	if ( sizeH ) sizeL = 0x80000000;

										// 読み込み準備 -----------------------
	if ( *image ){				// メモリは確保済み
		if ( *imagesize < sizeL ) sizeL = *imagesize;
	}else{						// 内部確保
		heap = 1;
		*imagesize = sizeL;
		*image = HeapAlloc(GetProcessHeap(), 0, sizeL + margin);
		if ( *image == NULL ){
			result = GetLastError();
			CloseHandle( hFile );
			return result;
		}
	}
										// 読み込み & 0 padding ---------------
	result = ReadFile(hFile, *image, sizeL, &sizeL, NULL) ?
			NO_ERROR : GetLastError();
	CloseHandle(hFile);
	if ( result != NO_ERROR ){
		if ( heap != 0 ) HeapFree(GetProcessHeap(), 0, *image);
		return result;
	}
	if ( (heap == 0) && (*imagesize > (sizeL + margin)) ){
		margin = sizeL - *imagesize;
	}
	memset(*image + sizeL, 0, margin);
	return NO_ERROR;
}
/*-----------------------------------------------------------------------------
 起動中の PPx を終了させる
-----------------------------------------------------------------------------*/
#ifndef _WIN64
void CloseOldPPx(const TCHAR *destpath, const TCHAR *name)
{
	OLDXSEND DPPxSendMessage;
	HMODULE hDLL;
	TCHAR buf[MAX_PATH];

	wsprintf(buf, name, destpath);
	DeleteFile(buf);	// 起動していない状態なら、ここで削除できる
	hDLL = LoadLibrary(buf);
	if ( hDLL != NULL ){
		DPPxSendMessage = (OLDXSEND)GetProcAddress(hDLL, "_PPxSendMessage");
		if ( DPPxSendMessage != NULL ) DPPxSendMessage(WM_CLOSE, 0, 0);
		FreeLibrary(hDLL);
//		DeleteFile(buf); ここでは削除できない可能性が高いので、何もしない
	}
	return;
}
#endif

void CloseNewPPx(HINSTANCE hDLL)
{
	PASXSEND DPPxSendMessage;

	DPPxSendMessage = (PASXSEND)GetProcAddress(hDLL, "PPxSendMessage");
	if ( DPPxSendMessage != NULL ) DPPxSendMessage(WM_CLOSE, 0, 0);
	return;
}

void CloseAllPPxLocal(TCHAR *destpath, BOOL setuppath)
{
	HMODULE hDLL;
	TCHAR buf[MAX_PATH];

	if ( destpath[0] == '?' ) return; // セットアップしたdirが不明

#ifndef _WIN64
// 0.42以前のPPxを終了
	CloseOldPPx(destpath, T("%s\\PPCOMMON.DLL"));	// MultiByte 版 PPx を終了
	if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT ){
		CloseOldPPx(destpath, T("%s\\PPXLIB32.DLL"));	// UNICODE 版 PPx を終了
	}

// 0.43以降のPPxを終了
	// MultiByte 版 PPx を終了
	wsprintf(buf, T("%s\\PPLIB32.DLL"), destpath);
	hDLL = LoadLibrary(buf);
	if ( hDLL != NULL ){
		CloseNewPPx(hDLL);
	#if !defined(UNICODE)
		if ( IsTrue(setuppath) ){
			PASSETCUSTTABLEA DSetCustTableA;

			DSetCustTableA = (PASSETCUSTTABLEA)GetProcAddress(hDLL, "SetCustTable");
			if ( DSetCustTableA != NULL ){
				DSetCustTableA("_Setup", "path", destpath, strlen(destpath) + 1);
				buf[0] = (char)((AdminCheck() == ADMINMODE_ELEVATE) ? '1' : '0');
				buf[1] = '\0';
				DSetCustTableA("_Setup", "elevate", buf, sizeof(char) * 2);
			}
		}
	#endif
		FreeLibrary(hDLL);
	#if !defined(UNICODE)
	}else{
		tstrcat(buf, MessageStr[MSG_DLL_LOADERROR_PART]);
		if ( setuppath ){
			WriteResult(buf, RESULT_NORMAL);
		}else{
			SMessage(buf);
		}
	#endif
	}

	if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT ){
	// UNICODE 版 PPx を終了
		wsprintf(buf, T("%s\\PPLIB32W.DLL"), destpath);
#else
		wsprintf(buf, T("%s\\PPLIB64W.DLL"), destpath);
#endif
		hDLL = LoadLibrary(buf);
		if ( hDLL != NULL ){
			CloseNewPPx(hDLL);
#if defined(UNICODE)
			if ( IsTrue(setuppath) ){
				PASSETCUSTTABLEW DSetCustTableW;

				DSetCustTableW = (PASSETCUSTTABLEW)GetProcAddress(hDLL, "SetCustTable");
				if ( DSetCustTableW != NULL ){
					WCHAR bufW[MAX_PATH];

					DSetCustTableW(L"_Setup", L"path", destpath, (strlenW32(destpath) + 1) * sizeof(WCHAR));
					bufW[0] = (AdminCheck() == ADMINMODE_ELEVATE) ? L'1' : L'0';
					bufW[1] = '\0';
					DSetCustTableW(L"_Setup", L"elevate", bufW, sizeof(WCHAR) * 2);
				}
			}
#endif
			FreeLibrary(hDLL);
		}else{
#if defined(UNICODE)
			tstrcat(buf, MessageStr[MSG_DLL_LOADERROR_PART]);
			if ( setuppath ){
				WriteResult(buf, RESULT_NORMAL);
			}else{
				SMessage(buf);
			}
#endif
		}
#ifndef _WIN64
	}
#endif
		// Hook PPLIB.DLL があれば全部解放させる
	PostMessage(HWND_BROADCAST, WM_NULL, 0, 0);
	Sleep(100); // 他のスレッドを回す
}
/*-----------------------------------------------------------------------------
 レジストリから文字列形式の値を取得する
-----------------------------------------------------------------------------*/
BOOL GetRegStrLocal(HKEY hKey, const TCHAR *path, const TCHAR *name, _Out_ TCHAR *dest)
{
	HKEY HK;
	DWORD t, s;

	*dest = '\0';
	if ( RegOpenKeyEx(hKey, path, 0, KEY_READ, &HK) == ERROR_SUCCESS ){
		s = MAX_PATH * sizeof(TCHAR);
		if (RegQueryValueEx(HK, name, NULL, &t, (LPBYTE)dest, &s) == ERROR_SUCCESS){
			if ( t == REG_EXPAND_SZ ){
				TCHAR buf[MAX_PATH];

				tstrcpy(buf, dest);
				ExpandEnvironmentStrings(buf, dest, MAX_PATH);
			}
			RegCloseKey(HK);
			return TRUE;
		}
		RegCloseKey(HK);
	}
	return FALSE;
}
/*-----------------------------------------------------------------------------
	カスタマイズ内容へのポインタを取得する
	UsePPx-FreePPx間内で使用すること
-----------------------------------------------------------------------------*/
BYTE *LocalGetCustDataPtr(const TCHAR *str)
{
	BYTE *ptr;	// 現在の内容の先頭
	DWORD w;	// (+0)次の内容へのオフセット

	ptr = CustP;

	for ( ;; ){
		w = *(DWORD *)ptr;
		if ( w == 0 ) return NULL;	// 末尾の判断
		if ( tstricmp((TCHAR *)(ptr + 4), str ) == 0 ){
			return ptr + TSTRSIZE(str) + 4;
		}
		ptr += w;
	}
}

/*-----------------------------------------------------------------------------
	配列内のカスタマイズ内容を取得する
	b_size = 0 なら保存に必要な大きさを返す
-----------------------------------------------------------------------------*/
int LocalGetCustTable(const TCHAR *str, const TCHAR *sub, void *bin, DWORD b_size)
{
	BYTE *p;		// 現在の内容の先頭
	size_t ssize;	// 文字列部分の大きさ
	DWORD w;		// (+0)次の内容へのオフセット

	ssize = (DWORD)TSTRSIZE(sub);

	p = LocalGetCustDataPtr(str);
	if ( p == NULL ){
		return -1;	// 未登録
	}
	for ( ;; ){
		w = *(WORD *)p;
		if ( !w ){
			return -1;
		}
		if ( !tstricmp( (TCHAR *)(p + 2), sub) ){
			w -= (WORD)(ssize + 2);
			if ( !b_size ){
				return w;
			}
			if ( w > b_size ) w = b_size;
			memcpy(bin, p + 2 + ssize, w);
			return NO_ERROR;
		}
		p += w;
	}
}
/*-----------------------------------------------------------------------------
 ディレクトリ選択
-----------------------------------------------------------------------------*/
BOOL SelectDirectory(HWND hWnd, const TCHAR *title, UINT flag, BFFCALLBACK proc, TCHAR *path)
{
	BROWSEINFO lpBI;
	LPITEMIDLIST idl;
	LPMALLOC pMA;
	BOOL result;
	TCHAR dispnamebuf[MAX_PATH];

	dispnamebuf[0] = '\0';
	if ( OSver.dwMajorVersion < 5 ) resetflag(flag, BIF_USENEWUI);

	lpBI.hwndOwner 		= hWnd;
	lpBI.pidlRoot		= NULL;
	lpBI.pszDisplayName	= dispnamebuf;
	lpBI.lpszTitle		= title;
	lpBI.ulFlags		= flag;
	lpBI.lpfn			= proc;
	lpBI.lParam			= 0;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	idl = SHBrowseForFolder(&lpBI);
	if ( idl == NULL ){
		CoUninitialize();
		return FALSE;
	}

	path[0] = '\0';
	result = SHGetPathFromIDList(idl, path);
	if ( SUCCEEDED(SHGetMalloc(&pMA)) ){
		pMA->lpVtbl->Free(pMA, idl);
		pMA->lpVtbl->Release(pMA);
	}
	CoUninitialize();
	return result;
}
/*-----------------------------------------------------------------------------
 PPx 手動選択ダイアログ
-----------------------------------------------------------------------------*/
#pragma argsused
int CALLBACK SelDirProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	UnUsedParam(lParam);UnUsedParam(lpData);

	if ( uMsg ==  BFFM_INITIALIZED ){
		TCHAR buf[MAX_PATH];

		tstrcpy(buf, XX_instdestS);
		buf[3] = '\0';
		SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM)buf);
	}
	return 0;
}

#pragma argsused
int CALLBACK SelInstPPxProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	UnUsedParam(lpData);

	switch(uMsg){
		case BFFM_INITIALIZED:{
			TCHAR buf[MAX_PATH];

			GetWindowsDirectory(buf, TSIZEOF(buf));
			buf[3] = '\0';
			SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM)buf);
			break;
		}

		case BFFM_SELCHANGED:{
			TCHAR buf[MAX_PATH], *p;
			BOOL found;

			if ( SHGetPathFromIDList((LPCITEMIDLIST)lParam, buf) ){
				p = buf + tstrlen(buf);
				tstrcpy(p, T("\\") T(COMMONDLL));
				found = GetFileAttributes(buf) != BADATTR;
#ifndef _WIN64
				tstrcpy(p, T("\\") T(OLDCOMMONDLL));
				if ( GetFileAttributes(buf) != BADATTR ) found = TRUE;
#endif
				SendMessage(hWnd, BFFM_ENABLEOK, 0, found);
				SendMessage(hWnd, BFFM_SETSTATUSTEXT, 0,
					found ? (LPARAM)MessageStr[MSG_FOUND] : (LPARAM)NilStr);
			}
			break;
		}
	}
	return 0;
}
/*-----------------------------------------------------------------------------
  実行ファイルを検索する
-----------------------------------------------------------------------------*/
OPENFILENAME ofile = {
	sizeof(ofile), NULL, NULL,
	T("Executable File\0*.exe;*.com;*.bat;*.lnk;*.cmd\0")
	T("All Files\0*.*\0") T("\0"),	// lpstrFilter
	NULL, 0, 0,
	NULL, MAX_PATH, NULL, 0, NULL, NULL, OFN_HIDEREADONLY | OFN_SHAREAWARE,
	0, 0, T("*"), 0, NULL, NULL OPENFILEEXTDEFINE
};

void SetExecuteFile(HWND hWnd, int ID, const TCHAR *title)
{
	TCHAR buf[MAX_PATH];

	buf[0] = '\0';
	ofile.hwndOwner = hWnd;
	ofile.hInstance = hInst;
	ofile.lpstrFile = buf;
	ofile.lpstrTitle = title;
	if ( IsTrue(GetOpenFileName(&ofile)) ) SetDlgItemText(hWnd, ID, buf);
}
/*-----------------------------------------------------------------------------
  ファイルをコピーする
-----------------------------------------------------------------------------*/
BOOL CopyPPxFileMain(HWND hWnd, const TCHAR *srcpath, const TCHAR *destpath, const TCHAR *filename, int *count)
{
	int waitcount;
	DWORD atr;
	TCHAR srcname[MAX_PATH], destname[MAX_PATH];

	wsprintf(srcname, T("%s\\%s"), srcpath, filename);
	waitcount = 100; // 10 秒
	for (;;){
		ERRORCODE result;

		if ( GetFileAttributes(srcname) != BADATTR ) break;
		result = GetLastError();
		if ( result == ERROR_FILE_NOT_FOUND ) return TRUE;
		if ( (result == ERROR_ACCESS_DENIED) ||
			 (result == ERROR_SHARING_VIOLATION) ){
			Sleep(100);
			waitcount--;
			if ( waitcount >= 0 ) continue;
		}
		wsprintf(srcname + tstrlen(srcname), T(" error(%d)"), result);
		WriteResult(srcname, RESULT_FAULT);
		return TRUE;
	}
	wsprintf(destname, T("%s\\%s"), destpath, filename);

	waitcount = 100; // 10 秒
	for ( ;; ){
		atr = GetFileAttributes(destname);
		if ( atr == BADATTR ) atr = FILE_ATTRIBUTE_NORMAL;
		SetFileAttributes(destname, FILE_ATTRIBUTE_NORMAL);
		if ( CopyFile(srcname, destname, FALSE) ){
			SetFileAttributes(destname, atr);
			WriteResult(filename, RESULT_SUCCESS);
			(*count)++;
			break;
		}else{
			TCHAR *mes;
			ERRORCODE result;

			result = GetLastError();
			if ( (result == ERROR_ACCESS_DENIED) ||
				 (result == ERROR_SHARING_VIOLATION) ){
				Sleep(100);
				waitcount--;
				if ( waitcount >= 0 ) continue;
				SetupResult = SRESULT_ACCESSERROR;
			}
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_IGNORE_INSERTS |
					FORMAT_MESSAGE_FROM_SYSTEM, NULL, result,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
					(LPTSTR)&mes, 0, NULL);
			result = MessageBox(hWnd, mes, filename,
					MB_APPLMODAL | MB_ABORTRETRYIGNORE |
					MB_ICONEXCLAMATION | MB_DEFBUTTON2);
			LocalFree(mes);
										// 対処 -------------------------------
			switch(result){
				case IDRETRY:	// 再試行
					waitcount = 20;
					continue;
				case IDIGNORE:	// 無視
					WriteResult(destname, RESULT_FAULT);
					break;
//				case IDABORT:	// 中止→defaultと同じ動作
				default:
					WriteResult(destname, RESULT_FAULT);
					if ( SetupResult == SRESULT_NOERROR ){
						SetupResult = SRESULT_FAULT;
					}
					return FALSE;	// 中止
			}
			break;
		}
	}
	return TRUE;
}

BOOL CopyPPxFiles(HWND hWnd, const TCHAR *srcpath, const TCHAR *destpath)
{
	INSTALLFILES *ip;
	WIN32_FIND_DATA ff;
	HANDLE hFF;
	TCHAR path[CMDLINESIZE];
	int count = 0;

	if ( (XX_setupmode == IDR_UPDATE) && (OSver.dwMajorVersion >= 6) ){
		for ( ip = InstallFiles ; ip->filename ; ip++ ){
			const TCHAR *ep;

			ep = tstrchr(ip->filename, '.');
			if ( (ep != NULL) &&
				 ((ep[1] == 'E') || (ep[1] == 'D') || (ep[1] == 's')) ){
				wsprintf(path, T("%s\\%s"), srcpath, ip->filename);
				if ( GetFileAttributes(path) != BADATTR ){
					wsprintf(path, T("*checksignature !\"%s\\%s\""), srcpath, ip->filename);
					if ( Execute_ExtractMacro(path, FALSE) == ERROR_INVALID_DATA ){
						WriteResult(ip->filename, MSG_BADSIG);
						SetupResult = SRESULT_FAULT;
						return FALSE;
					}
				}
			}
		}
	}

	for ( ip = InstallFiles ; ip->filename ; ip++ ){
		if ( CopyPPxFileMain(hWnd, srcpath, destpath, ip->filename, &count) == FALSE ){
			return FALSE;
		}
	}
	wsprintf(path, T("%s\\PPX1*.TXT"), srcpath);
	hFF = FindFirstFile(path, &ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		FindClose(hFF);
		if ( CopyPPxFileMain(hWnd, srcpath, destpath, ff.cFileName, &count) == FALSE ){
			return FALSE;
		}
	}
	if ( count == 0 ){
		WriteResult(ip->filename, MSG_COPYNOFILE);
		SetupResult = SRESULT_FAULT;
		return FALSE;
	}
	return TRUE;
}

void CheckDlgButtons(HWND hDlg, int start, int end, int check)
{
	int i;

	for ( i = start ; i <= end ; i++ ){
		CheckDlgButton(hDlg, i, i == check );
	}
	return;
}

int GetDlgButtons(HWND hDlg, int start, int end)
{
	int i;

	for ( i = start ; i <= end ; i++ ){
		if ( SendDlgItemMessage(hDlg, i, BM_GETCHECK, 0, 0) ) break;
	}
	return i;
}

void Cmd(TCHAR *param)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	si.cb = sizeof(si);
	si.lpReserved = NULL;
	si.lpDesktop = NULL;
	si.lpTitle = NULL;
	si.dwFlags = 0;
	si.cbReserved2 = 0;
	si.lpReserved2 = NULL;

	if ( CreateProcess(NULL, param, NULL, NULL, FALSE,
		   CREATE_NEW_CONSOLE | CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi) ){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}
/*-----------------------------------------------------------------------------
  PPx を検索する
-----------------------------------------------------------------------------*/
void MakeUserfilename(TCHAR *dst)
{
	TCHAR path[MAX_PATH];
	TCHAR UserName[MAX_PATH];
										// 固定ファイルがあるならそれを使用 ===
	wsprintf(dst, T("%s\\") T(XNAME) T("CDEF.DAT"), MyPath);
	if ( GetFileAttributes(dst) != BADATTR ) return;

	if ( GetRegStrLocal(HKEY_CURRENT_USER, Str_PPxRegPath, Str_PPxRegValue, dst) == FALSE ){
		TCHAR *p;
		DWORD t;

		if ( GetWinAppDir(CSIDL_APPDATA, path) ){
			tstrcat(path, T("\\") T(PPxSettingsAppdataPath));
		}else{
			tstrcpy(path, MyPath);
			if ( path[0] == '\\' ) GetWindowsDirectory(path, MAX_PATH);
		}

		t = TSIZEOF(UserName);
		if( GetUserName(UserName, &t) == FALSE ) UserName[0] = '\0';
		t = 0xa55a;
		p = UserName;

		while ( *p ) t = (t << 1) + (DWORD)*p++;
		wsprintf(dst, T("%s\\") T(XNAME) T("C%04X.DAT"), path, t & 0xffff);
	}
}

BOOL SearchPPx(void)
{
	char *image = NULL;
	TCHAR *p;
	DWORD size;
	BOOL result;
	TCHAR buf[MAX_PATH];

	MakeUserfilename(buf);
												// カスタマイズファイルを開く
	if ( LocalLoadFileImage(buf, 64, &image, &size) != NO_ERROR ){
												// インストール登録がある？
		if ( IsTrue(GetRegStrLocal(HKEY_CURRENT_USER, Str_InstallPPxPath, Set_InstallPPxName, XX_setupedPPx)) ){
			return TRUE;
		}
		if ( IsTrue(GetRegStrLocal(HKEY_LOCAL_MACHINE, Str_InstallPPxPath, Set_InstallPPxName, XX_setupedPPx)) ){
			return TRUE;
		}
		return FALSE;
	}
	XX_setupedPPx[0] = '\0';
											// 正当性チェック
	CustP = (BYTE *)image + 8;
	if ( strcmp(image, CustID) != 0 ){
		SMessage(T("カスタマイズ領域のIDがおかしい"));
		result = FALSE;
	}else{										// セットアップパスを求める
		buf[0] = '\0';
		if ( NO_ERROR == LocalGetCustTable(T("_Setup"), T("path"), buf, sizeof(buf)) ){
			tstrcpy(XX_setupedPPx, buf);
			p = tstrrchr(XX_setupedPPx, '\\');
			if ( (p != NULL) && (*(p + 1) == '\0') ) *p = '\0';
		}else{								// なかったのでファイルの場所にする
			tstrcpy(XX_setupedPPx, buf);
			p = tstrrchr(XX_setupedPPx, '\\');
			if ( p != NULL ) *p = '\0';
		}

		wsprintf(buf, T("%s\\") T(COMMONDLL), XX_setupedPPx);
		if ( GetFileAttributes(buf) & FILE_ATTRIBUTE_DIRECTORY ){ // DLLがファイルでない
			tstrcpy(buf, XX_setupedPPx);
			XX_setupedPPx[0] = '?';
			tstrcpy(XX_setupedPPx + 1, buf);
			return FALSE; // image がリークするが、無視
		}
		result = TRUE;
	}
	HeapFree(GetProcessHeap(), 0, image);

	if ( IsTrue(result) ) return TRUE;
	XX_setupedPPx[0] = '\0';
	return FALSE;
}

BOOL DeleteDir(const TCHAR *path)
{
	TCHAR buf[MAX_PATH];
	WIN32_FIND_DATA	ff;
	HANDLE	hFF;

	wsprintf(buf, T("%s\\*"), path);
	hFF = FindFirstFile(buf, &ff);
	if ( INVALID_HANDLE_VALUE == hFF ) return TRUE;
	do{
		if ( IsRelativeDir(ff.cFileName) ) continue;
		wsprintf(buf, T("%s\\%s"), path, ff.cFileName);
		SetFileAttributes(buf, FILE_ATTRIBUTE_NORMAL);
		DeleteFile(buf);
	}while(FindNextFile(hFF, &ff));
	FindClose(hFF);
	return RemoveDirectory(path);
}

#ifndef TokenElevationType
#define TokenElevationType 18
#endif
typedef enum  {
  xTokenElevationTypeDefault = 1,
  xTokenElevationTypeFull,
  xTokenElevationTypeLimited
} xTOKEN_ELEVATION_TYPE;

int AdminCheck(void)
{
	// UAC 状態を取得
	if ( OSver.dwMajorVersion >= 6 ){
		HANDLE hCurToken;

		if ( OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hCurToken) ){
			DWORD tetsize = 0;
			xTOKEN_ELEVATION_TYPE tet = xTokenElevationTypeDefault;

			GetTokenInformation(hCurToken, TokenElevationType, &tet, sizeof(tet), &tetsize);
			CloseHandle(hCurToken);
			if ( tet == xTokenElevationTypeFull ){ // UAC 昇格状態
				return ADMINMODE_ELEVATE;
			}else{ //  if ( tet == xTokenElevationTypeLimited ){
				return ADMINMODE_NOADMIN;
			}
		}
	}
	return ADMINMODE_ADMIN;
}
