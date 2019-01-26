/*-----------------------------------------------------------------------------
	Paper Plane xUI	 setup wizard					Uninstall
-----------------------------------------------------------------------------*/
#include "PPSETUP.H"
#pragma hdrstop

#define DELBATNAME "PPXDEL.BAT"

const TCHAR RegPath[] = T("Software\\Classes\\Folder\\shell\\PPc\\command");
const TCHAR RegOpen[] = T("Software\\Classes\\Folder\\shell");
const TCHAR Str_RegTOROid[] = T("Software\\TOROid");

void DeleteRegDir(HKEY hKey,const TCHAR *path)
{
	HKEY HK;

	if ( RegOpenKeyEx(hKey,path,0,KEY_ALL_ACCESS,&HK) == ERROR_SUCCESS ){
		DWORD index = 0;

		for ( ;; ){
			TCHAR name[MAX_PATH];
			DWORD nsize;

			nsize = sizeof(name);
			if ( RegEnumValue(HK,index,name,&nsize,NULL,NULL,NULL,NULL) != ERROR_SUCCESS){
				break;
			}
			if ( RegDeleteValue(HK,name) != ERROR_SUCCESS) index++;
//			index++;	// 見つけた順に削除するので"++"不要
		}
		RegCloseKey(HK);
	}
	RegDeleteKey(hKey,path);
}

void DeleteMenuDirs(int csidl)
{
	TCHAR buf[MAX_PATH];

	if ( GetWinAppDir(csidl,buf) == FALSE ) return; // 存在しない
	tstrcat(buf,T("\\") T(MENUDIRNAME));
	DeleteDir(buf);
}

BOOL CheckOpen(void)
{
	TCHAR data[MAX_PATH];

	data[0] = '\0';
	GetRegStrLocal(HKEY_CURRENT_USER,RegOpen,NilStr,data);
	if ( data[0] == '\0' ){
		GetRegStrLocal(HKEY_LOCAL_MACHINE,RegOpen,NilStr,data);
	}
	return !tstrcmp(data,T("PPc"));
}

#define DeleteRegisterContextDepth 3
void DeleteRegisterContext(_In_ HKEY hk)
{
	TCHAR buf[MAX_PATH];
	int depth;

	tstrcpy(buf,RegPath);
	depth = DeleteRegisterContextDepth;
	// HKEY_LOCAL_MACHINE で Folderは削除禁止
	if ( hk == HKEY_LOCAL_MACHINE ) depth--;
	for ( ; depth > 0 ; depth-- ){
		RegDeleteKey(hk,buf);
		*tstrrchr(buf,'\\') = '\0';
	}
}

void DeleteOpenRegisterContext(void)
{
	if ( CheckOpen() == FALSE ) return;
	RegDeleteValue(HKEY_CURRENT_USER,RegOpen);
	RegDeleteValue(HKEY_LOCAL_MACHINE,RegOpen);
}


//=============================================================================
INT_PTR CALLBACK UnExecDialog(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg){
		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch(NHPTR->code){
				case PSN_WIZNEXT:{
					TCHAR buf[MAX_PATH];

					SetDlgItemText(hDlg,IDS_INSTEDPPX,T("削除中"));
					UpdateWindow(hDlg);
					CloseAllPPxLocal(XX_setupedPPx,FALSE);
//-----------------------------------------------------------------------------
	{ // AppData 内データの削除

		if ( GetWinAppDir(CSIDL_APPDATA,buf) ){ // Win95 は FALSE
			tstrcat(buf,T("\\") T(PPxSettingsAppdataPath));
			if ( GetFileAttributes(buf) != BADATTR ){
				int i;

				for ( i = 40 ; i ; i-- ){
					if ( IsTrue(DeleteDir(buf)) ) break;
					Sleep(500);
				}
				tstrcat(buf,T("\\.."));
				RemoveDirectory(buf);
			}
		}
	// ショートカット削除

		if ( GetWinAppDir(CSIDL_STARTUP,buf) ){
			tstrcat(buf,T("\\PPc") T(ShortcutExt));
			DeleteFile(buf);
		}

		DeleteMenuDirs(CSIDL_PROGRAMS);
		DeleteMenuDirs(CSIDL_COMMON_PROGRAMS);
		DeleteMenuDirs(CSIDL_DESKTOPDIRECTORY);
		DeleteMenuDirs(CSIDL_COMMON_DESKTOPDIRECTORY);

	// レジストリ削除
		// Uninstall
		DeleteRegDir(HKEY_CURRENT_USER,Str_InstallPPxPath);
		DeleteRegDir(HKEY_LOCAL_MACHINE,Str_InstallPPxPath);

		// 設定保存場所
		DeleteRegDir(HKEY_CURRENT_USER,Str_PPxRegPath);
		{ // 各ユーザの HKEY_CURRENT_USER\Software\TOROid\PPX
			int index = 0;
			for ( ;; ){
				TCHAR name[MAX_PATH];
				DWORD nsize;
				FILETIME ft;

				nsize = sizeof(name);
				if ( RegEnumKeyEx(HKEY_USERS,index,name,&nsize,
						NULL,NULL,NULL,&ft) != ERROR_SUCCESS ){
					break;
				}
				tstrcat(name,Str_PPxRegPath);
				DeleteRegDir(HKEY_USERS,name);
				index++;
			}
		}
		{	// TOROid キーが不要なら削除
			HKEY HK;

			if ( RegOpenKeyEx(HKEY_CURRENT_USER,Str_RegTOROid,0,
									KEY_ALL_ACCESS,&HK) == ERROR_SUCCESS ){
				TCHAR name[MAX_PATH];
				DWORD nsize;
				FILETIME ft;

				nsize = sizeof(name);
				if ( RegEnumKeyEx(HK,0,name,&nsize,NULL,NULL,NULL,&ft)
													!= ERROR_SUCCESS){
					RegCloseKey(HK);
					RegDeleteKey(HKEY_CURRENT_USER,Str_RegTOROid);
				}else{
					RegCloseKey(HK);
				}
			}
		}
		// 関連づけを削除
		DeleteOpenRegisterContext();
		DeleteRegisterContext(HKEY_CURRENT_USER);
		DeleteRegisterContext(HKEY_LOCAL_MACHINE);
	}
	// ファイル削除準備
	{
		HRSRC hRsrc;
		DWORD size;
		HANDLE hFile;

		TCHAR tempname[MAX_PATH],temppath[MAX_PATH];
		TCHAR cmd[MAX_PATH],param[MAX_PATH * 2];

		hRsrc = FindResource(hInst,MAKEINTRESOURCE(IF_PPXDEL),RT_RCDATA);
		size = SizeofResource(hInst,hRsrc);

		GetTempPath(TSIZEOF(temppath), temppath);
		wsprintf(tempname,T("%s\\") T(DELBATNAME),temppath);
		hFile = CreateFile(tempname,GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,
				CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if ( hFile == INVALID_HANDLE_VALUE ){
			SMessage(T("作業ファイルの作成失敗"));
			break;
		}else{
			WriteFile(hFile,
					LockResource(LoadResource(hInst,hRsrc)),size,&size,NULL);
			CloseHandle(hFile);
		}
//		GetShortPathName(XX_setupedPPx,XX_setupedPPx,sizeof(XX_setupedPPx));
		ExpandEnvironmentStrings(T("%COMSPEC%"),cmd,sizeof(cmd));
		if ( (cmd[0] != '\0') && (XX_setupedPPx[0] != '\0') ){
			STARTUPINFO si;
			PROCESS_INFORMATION pi;

			if ( tstrchr(tempname,' ') != NULL ){
				wsprintf(param,T("%s /C \"%s\" \"%s\""),cmd,tempname,XX_setupedPPx);
			}else{ // tempname に空白が無いときは、「"」で括ると失敗する
				wsprintf(param,T("%s /C %s \"%s\""),cmd,tempname,XX_setupedPPx);
			}

			si.cb			= sizeof(si);
			si.lpReserved	= NULL;
			si.lpDesktop	= NULL;
			si.lpTitle		= NULL;
			si.dwFlags		= 0;
			si.cbReserved2	= 0;
			si.lpReserved2	= NULL;
			if ( CreateProcess(NULL,param,NULL,NULL,FALSE,CREATE_NEW_CONSOLE |
					CREATE_DEFAULT_ERROR_MODE | IDLE_PRIORITY_CLASS,
					NULL,temppath,&si,&pi) ){
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}else{
				SMessage(T("バッチの起動失敗"));
				break;
			}
		}
	}
//-----------------------------------------------------------------------------
					PropSheet_PressButton(NHPTR->hwndFrom,PSBTN_FINISH);
					return -1;
				}
			}
			break;
			#undef NHPTR

//		default: // 特にすることなし
	}
	return StyleDlgProc(hDlg,msg,wParam,lParam);
}
