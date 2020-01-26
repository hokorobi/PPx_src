/*-----------------------------------------------------------------------------
	Paper Plane xUI	 setup wizard - Install
-----------------------------------------------------------------------------*/
#include "PPSETUP.H"
#include "PPXVER.H"
#pragma hdrstop

WORD KeyType[] = { 0, IF_DEFXPR, IF_DEFKL, IF_DEFFM };
SID_IDENTIFIER_AUTHORITY siaNTAuthority = {SECURITY_WORLD_SID_AUTHORITY};

const DWORD JointMode[] = {
	0x10f,	//1 0000 1111	IDR_PPC_JOIN_H
	0x14f,	//1 0100 1111	IDR_PPC_JOIN_V
	0x18f	//1 1000 1111	IDR_PPC_JOIN_P
};

#if defined(UNICODE)
	#define MAKESHORTCUTT MAKESHORTCUTW
#else
	#define MAKESHORTCUTT MAKESHORTCUTA
#endif
MAKESHORTCUTT DMakeShortCut = NULL;

const TCHAR Str_DisplayName[] = T("DisplayName");
const TCHAR Str_DisplayNameValue[] = T("Paper Plane xUI");
const TCHAR Str_Publisher[] = T("Publisher");
const TCHAR Str_PublisherValue[] = T("TORO");
const TCHAR Str_UninstallString[] = T("UninstallString");
const TCHAR Str_DisplayIcon[] = T("DisplayIcon");

HRESULT MakeShortCutS(const TCHAR *LinkedFile, const TCHAR *LinkFname, const TCHAR *DestPath)
{
	if ( DMakeShortCut == NULL ) return E_NOTIMPL;

	return DMakeShortCut(LinkedFile, LinkFname, DestPath);
}

// 階層ディレクトリの一括作成 -------------------------------------------------
ERRORCODE MakeDir(const TCHAR *name)
{
	ERRORCODE ec = 0;

	if ( CreateDirectory(name, NULL) == FALSE ){
		ec = GetLastError();
		if ( ec == ERROR_PATH_NOT_FOUND ){
			TCHAR buf[MAX_PATH], *wp;

			if ( GetFullPathName(name, TSIZEOF(buf), buf, &wp) == FALSE ){
				ec = GetLastError();
			}else{
				*wp = 0;
				ec = MakeDir(buf);
				if (!ec){
					if ( IsTrue(CreateDirectory(name, NULL)) ){
						ec = 0;
					}else{
						ec = GetLastError();
					}
				}
			}
		}
	}
	return ec;
}

BOOL MakeStartMenus(TCHAR *destpath, TCHAR *menudest)
{
	INSTALLFILES *p;
	TCHAR linked[MAX_PATH], destname[MAX_PATH];
	ERRORCODE result;

	tstrcat(menudest, T("\\") T(MENUDIRNAME));
	result = MakeDir(menudest);
	if ( (result != NO_ERROR) && (result != ERROR_ALREADY_EXISTS) ){
		WriteResult(menudest, RESULT_FAULT);
	}

	for ( p = InstallFiles ; p->filename ; p++ ){
		if ( p->dispname ){
			wsprintf(linked, T("%s\\%s"), destpath, p->filename);
			if ( GetFileAttributes(linked) == BADATTR ) continue;
			wsprintf(destname, T("%s\\%s") T(ShortcutExt), menudest, p->dispname);
			if ( FAILED(MakeShortCutS(linked, destname, menudest)) ){
				WriteResult(destname, RESULT_FAULT);
				break;
			}
		}
	}
	// Ver6 未満は、ファイルがある場所を開く方法が用意されていないので、
	// 代わりのショートカットを用意
	if ( OSver.dwMajorVersion < 6 ){
		wsprintf(destname, T("%s\\Jump to directory") T(ShortcutExt), menudest);
		if ( !SUCCEEDED(MakeShortCutS(destpath, destname, menudest)) ){
			WriteResult(destname, RESULT_FAULT);
		}
	}
	return TRUE;
}

BOOL CreateDat(TCHAR cfgtype)
{
	HANDLE hFile;
	DWORD size;
	TCHAR buf[MAX_PATH];

	SECURITY_ATTRIBUTES *psa = NULL, sa;
	BYTE ACLBUF[0x200];	// 適当に用意
	PSID pSID;

	// Everyone 権限を作成する
	sa.lpSecurityDescriptor = NULL;
	if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT ){
		sa.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)
				LocalAlloc(LMEM_FIXED, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if ( sa.lpSecurityDescriptor != NULL ){
			if ( InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION) ){
				PACL pACL;
							// DESCRIPTOR (Everyone) を作成
				SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, NULL, FALSE);
				pACL = (PACL)ACLBUF;
				InitializeAcl(pACL, sizeof(ACLBUF), ACL_REVISION2);

				if ( IsTrue(AllocateAndInitializeSid(&siaNTAuthority,
						1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pSID)) ){

					AddAccessAllowedAce(pACL, ACL_REVISION2, GENERIC_ALL, pSID);
					SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, pACL, FALSE);

					sa.nLength = sizeof sa;
					sa.bInheritHandle = FALSE;
					psa = &sa;
					FreeSid(pSID);
				}
			}
		}
	}

	wsprintf(buf, T("%s\\") T(XNAME) T("%cDEF.DAT"), XX_setupPPx, cfgtype);
	hFile = CreateFile(buf, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			psa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( sa.lpSecurityDescriptor != NULL ) LocalFree(sa.lpSecurityDescriptor);
	if ( hFile == INVALID_HANDLE_VALUE ){
		WriteResult(buf, RESULT_FAULT);
		return FALSE;
	}else{
		size = 4;
		WriteFile(hFile, "RST", size, &size, NULL);
		SetFilePointer(hFile, 0x10000, NULL, FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		return TRUE;
	}
}

int USEFASTCALL WriteConfigRes(HANDLE hFile, WORD resource)
{
	HRSRC hres;
	DWORD size;

	if ( resource == 0 ) return 0;
	hres = FindResource(hInst, MAKEINTRESOURCE(resource), RT_RCDATA);
	#ifdef UNICODE
	{
		WCHAR bufw[0x4000];
		char bufa[0x4000];
		DWORD rsize;
		UINT cp;

		cp = IsValidCodePage(CP__SJIS) ? CP__SJIS : CP_ACP;
		rsize = SizeofResource(hInst, hres);
		rsize = MultiByteToWideChar(cp, 0,
				LockResource(LoadResource(hInst, hres)), rsize, bufw, TSIZEOFW(bufw));
		rsize = WideCharToMultiByte(CP_UTF8, 0, bufw, rsize, bufa, TSIZEOFA(bufa), NULL, NULL);
		return (WriteFile(hFile, bufa, rsize, &size, NULL) == FALSE) ? 1 : 0;
	}
	#else
		return (WriteFile(hFile, LockResource(LoadResource(hInst, hres)),
				SizeofResource(hInst, hres), &size, NULL) == FALSE) ? 1 : 0;
	#endif

}

int WriteConfigf(HANDLE hFile, const TCHAR *formats, ...)
{
	TCHAR bufT[0x800];
	va_list argptr;
	DWORD size;

	va_start(argptr, formats);
	wvsprintf(bufT, formats, argptr);
	va_end(argptr);
	#ifdef UNICODE
	{
		char bufA[0x800];

		UnicodeToUtf8(bufT, bufA, sizeof(bufA));
		return (WriteFile(hFile, bufA, strlen32(bufA), &size, NULL) == FALSE) ? 1 : 0;
	}
	#else
		return (WriteFile(hFile, bufT, strlen32(bufT), &size, NULL) == FALSE) ? 1 : 0;
	#endif
}

BOOL InstallMain(HWND hWnd)
{
	HMODULE hDLL;
	HANDLE hFile;
	DWORD size;
	int errstate;
	MEMORYSTATUS memstat;
#ifdef UNICODE
	char bufA[0x400];
	WCHAR bufT[0x400];
#else
	char bufA[0x400];
	#define bufT bufA
#endif

	memstat.dwLength = sizeof(memstat);
	GlobalMemoryStatus(&memstat);
//===================================== 上書きの場合のバックアップ ============
	SetupResult = SRESULT_NOERROR;
	if ( XX_setupedPPx[0] ){ // 上書きインストール...backup
		const TCHAR *destpath;

		CloseAllPPxLocal(XX_setupedPPx, FALSE);
		destpath = (XX_setupedPPx[0] != '?') ? XX_setupedPPx : T(".");
		wsprintf(bufT, T("\"%s\\") T(CSEXE) T("\" CD \"%s\\PPXold.CFG\""), destpath, destpath);
		Cmd(bufT);
		wsprintf(bufT, MessageStr[MSG_BACKUP], destpath);
		SMessage(bufT); // ppcust の実行が終わるまで、待たせる必要がある
	}
//===================================== インストールディレクトリを作成 ========
	if ( XX_instdestM != IDR_NOCOPY ){
		if ( MakeDir(XX_setupPPx) == ERROR_ACCESS_DENIED ){
			SetupResult = SRESULT_ACCESSERROR;
		}
//===================================== ファイルをコピー ======================
		CopyPPxFiles(hWnd, MyPath, XX_setupPPx);
	}
//===================================== カスタマイズ位置設定 ==================
	if ( XX_usereg == IDR_UUSEREG ){
		CreateDat('C');
		CreateDat('H');
	}
//===================================== 初期カスタマイズ ======================
	wsprintf(bufT, T("%s\\PPXDEF.CFG"), XX_setupPPx);
	hFile = CreateFile(bufT, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		errstate = 1;
	}else{
		#ifdef UNICODE
			WriteFile(hFile, UTF8HEADER, UTF8HEADERSIZE, &size, NULL);
		#endif
		errstate = WriteConfigf(hFile,
			T("PPxCFG	= ") T(FileCfg_Version) T(" ;これらの定義はカスタマイズの初期化時に読込みます") TNL
			T("_Setup	= {") TNL
			T("path	= %s") TNL
			T("elevate	= %d") TNL
			T("}") TNL
			T("A_exec	= {") TNL
			T("editor	= \"%s\"") TNL
			T("viewer	= \"%s\"") TNL
			T("}") TNL,
				XX_setupPPx, // _Setup:path
				(AdminCheck() == ADMINMODE_ELEVATE) ? 1 : 0,
				XX_editor, // A_exec:editor
				XX_viewer); // A_exec:viewer
		if ( IsTrue(XX_usesusie) ){
			errstate |= WriteConfigf(hFile, T("P_susieP	= %s") TNL, XX_susie);
		}

		errstate |= WriteConfigRes(hFile, IF_DEFPPX);

		if ( XX_emenu == FALSE ) errstate |= WriteConfigRes(hFile, IF_DEFJPN);

		errstate |= WriteConfigRes(hFile, KeyType[XX_keytype - IDR_PPXKEY]);

		if ( (XX_emenu == FALSE) && (XX_keytype == IDR_XPRKEY) ){
			errstate |= WriteConfigRes(hFile, IF_DEFXPJ);
		}

		bufA[0] = '\0';

		if ( XX_ppc_window == IDR_PPC_COMBO ){ // 一体化時の追加分
			DWORD X_mpane = 1;
			DWORD X_combos = B0 | B13 | B18;

			if ( XX_ppc_tree ) setflag(X_combos, B2);
			if ( XX_ppc_pane == IDR_PPC_2PANE_V ) setflag(X_combos, B4);
			if ( XX_ppc_pane >= IDR_PPC_2PANE_H ) X_mpane = 2;
			switch ( XX_ppc_tab ){
				case IDR_PPC_TAB_FULLSEPARATE:
					setflag(X_combos, B5 | B13 | B19);
					break;

				case IDR_PPC_TAB_SEMISEPARATE:
					setflag(X_combos, B5);
					break;

				case IDR_PPC_TAB_SHARE:
					X_combos = (X_combos & ~(B13 | B19)) | B5;
					break;
			}
			wsprintfA(bufA, "X_combo	= 1"NL
				"X_combos	= H%x,0"NL
				"X_mpane	= %d,%d"NL,
				X_combos,
				X_mpane, X_mpane);
		}else if ( XX_ppc_window == IDR_PPC_JOINT ){ // 連結追加分
			wsprintfA(bufA, "XC_swin	= {" NL
				"A	= %d" NL
				"}" NL,
				JointMode[XX_ppc_join - IDR_PPC_JOIN_H]
			);
		}

		// ２画面向け追加設定
		if ( XX_ppc_window >= IDR_PPC_COMBO ){
			strcat(bufA,
				"XC_page	= 1"NL
				"XC_mvLR	= 4,1,4,B0100,0,B100"NL
				"X_dsst	= 3,1"NL );
			strcat(bufA,
				( XX_keytype == IDR_XPRKEY ) ?
					"XC_celF	= {"NL
					"A	= s1NwF16,5zK7S1T14s2"NL
					"}"NL  :
					"XC_celF	= {"NL
					"A	= MwF16,5z10S1T14s1"NL
					"}"NL  );
		}

		if ( XX_ppc_tree && (XX_ppc_window != IDR_PPC_COMBO) ){
			strcat(bufA,
				"XC_tree	= {"NL
				"A	= 1,200,"NL
				"}"NL
			);
		}

		if ( bufA[0] != '\0' ){ // ANSI/UTF8のどちらでも変わらないのでそのまま
			if ( WriteFile(hFile, bufA, strlen32(bufA), &size, NULL) == FALSE ){
				errstate = 1;
			}
		}

		if ( IsTrue(XX_diakey) ) errstate |= WriteConfigRes(hFile, IF_DEFDIA);
		if ( XX_doscolor == FALSE ) errstate |= WriteConfigRes(hFile, IF_DEFWIN);

		// 低スペック(Win9xかメモリ少なめ)の設定
		if ( (memstat.dwTotalPhys <= (LOWSETTINGMEM_MB * MB)) ||
			 (OSver.dwPlatformId != VER_PLATFORM_WIN32_NT) ){
			errstate |= WriteConfigRes(hFile, IF_DEFLOW);
		}
		CloseHandle(hFile);
	}
	if ( errstate != 0 ) WriteResult(T("PPXDEF.CFG"), RESULT_FAULT);
	if ( XX_setupedPPx[0] ){ // 上書きインストール...旧カスタマイズを削除
		wsprintf(bufT, T("\"%s\\") T(CSEXE) T("\" CINIT"), XX_setupPPx);
		Cmd(bufT);
	}
							// PPLIBxxx.DLL をロード(＆初期カスタマイズ)
	wsprintf(bufT, T("%s\\") T(COMMONDLL), XX_setupPPx);
	hDLL = LoadLibrary(bufT);
	if ( hDLL != NULL ){
		DMakeShortCut = (MAKESHORTCUTT)GetProcAddress(hDLL, "MakeShortCut");
//===================================== ショートカットを作成 ==================
		if ( SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) ){
			if ( XX_link_boot ){
				TCHAR linked[MAX_PATH], destname[MAX_PATH];

				GetWinAppDir(CSIDL_STARTUP, bufT);
				wsprintf(linked, T("%s\\") T(CEXE), XX_setupPPx);
				wsprintf(destname, T("%s\\PPc") T(ShortcutExt), bufT);
				if ( FAILED(MakeShortCutS(linked, destname, bufT)) ){
					WriteResult(destname, RESULT_FAULT);
				}
			}
			if ( XX_link_menu ){
				GetWinAppDir(CSIDL_PROGRAMS, bufT);
				MakeStartMenus(XX_setupPPx, bufT);
			}
			if ( XX_link_cmenu ){
				GetWinAppDir(CSIDL_COMMON_PROGRAMS, bufT);
				MakeStartMenus(XX_setupPPx, bufT);
			}
			if ( XX_link_desk ){
				if ( XX_link_cmenu ){
					GetWinAppDir(CSIDL_COMMON_DESKTOPDIRECTORY, bufT);
				}else{
					GetWinAppDir(CSIDL_DESKTOPDIRECTORY, bufT);
				}
				MakeStartMenus(XX_setupPPx, bufT);
			}
			CoUninitialize();
		}
		FreeLibrary(hDLL);
	}else{
		WriteResult(MessageStr[MSG_DLLLOADERROR], RESULT_FAULT);
	}
//===================================== レジストリの設定 ======================
	if ( XX_link_app ){
		HKEY HK;
		DWORD tmp;

		if ( RegCreateKeyEx(
				(XX_link_cmenu ||
					(OSver.dwPlatformId != VER_PLATFORM_WIN32_NT)) ?
					HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
					Str_InstallPPxPath, 0, T(""), REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS, NULL, &HK, &tmp) == ERROR_SUCCESS ){
			// 表示名
			RegSetValueEx(HK, Str_DisplayName, 0, REG_SZ,
				(LPBYTE)Str_DisplayNameValue, sizeof(Str_DisplayNameValue));
			// 発行元
			RegSetValueEx(HK, Str_Publisher, 0, REG_SZ,
				(LPBYTE)Str_PublisherValue, sizeof(Str_PublisherValue));
			// インストール先ディレクトリ
			RegSetValueEx(HK, Set_InstallPPxName, 0, REG_SZ,
				(LPBYTE)XX_setupPPx, TSTRSIZE32(XX_setupPPx));
			// アンインストールプログラム
			wsprintf(bufT, T("%s\\SETUP.EXE"), XX_setupPPx);
			RegSetValueEx(HK, Str_UninstallString, 0, REG_SZ,
				(LPBYTE)bufT, TSTRSIZE32(bufT));
			// 表示アイコン
			RegSetValueEx(HK, Str_DisplayIcon, 0, REG_SZ,
				(LPBYTE)bufT, TSTRSIZE32(bufT));
		}else{
			WriteResult(MessageStr[RESULTINFO_UNINSTALLDATA], RESULT_FAULT);
		}
		RegCloseKey(HK);
	}
//===================================== 作業ファイルの削除 ====================
	WriteResult(NilStr, RESULT_ALLDONE);
	if ( SetupResult == SRESULT_NOERROR ){
		WriteResult(MessageStr[RESULTINFO_INSTALL], RESULT_SUCCESS);
	}
	return TRUE;
#ifndef UNICODE
  #undef bufT
#endif
}
