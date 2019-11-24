/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						拡張子自前判別
-----------------------------------------------------------------------------*/
#define ONPPXDLL // PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#pragma hdrstop

#ifdef UNICODE
DefineWinAPI(HRESULT, SHLoadIndirectString, (LPCWSTR pszSource, LPWSTR pszOutBuf, UINT cchOutBuf, void **ppvReserved)) = NULL;
#endif

const TCHAR MUIVerbStr[] = T("MUIVerb");
const TCHAR openstr[] = MES_MCOP;
const TCHAR playstr[] = MES_MCPL;
const TCHAR printstr[] = MES_MCPR;
const TCHAR runasV5str[] = MES_MCRK;
const TCHAR runasV6str[] = MES_MCRV;
const TCHAR ExtsChoiseStr[] = T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\UserChoice");
const TCHAR ProgIDstr[] = T("ProgId");
const TCHAR PAOstr[] = T("ProgrammaticAccessOnly");

// 指定拡張子のアクション一覧をメニューに登録する -----------------------------
PPXDLL int PPXAPI GetExtentionMenu(HMENU hSubMenu, const TCHAR *ext, PPXMENUDATAINFO *pmdi)
{
	DWORD Bsize; // バッファサイズ指定用
	DWORD Rtyp; // レジストリの種類

	FILETIME write;
	DWORD keyS;

	TCHAR appN[MAX_PATH]; // アプリケーションのキー
	TCHAR keyN[MAX_PATH]; // レジストリのキー名称
	TCHAR comN[MAX_PATH]; // ...\\command を示す
	TCHAR defN[MAX_PATH]; // デフォルトのアクション
	HKEY hAppShellKey, hCommandKey;

	MENUITEMINFO minfo;

	int cnt = 0;

	minfo.cbSize = sizeof(minfo);
	minfo.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID;
	minfo.fType = MFT_STRING;
//	minfo.fState = MFS_ENABLED;
	minfo.wID = pmdi->id;

	if ( ext[0] == '.' ){				// 拡張子からキーを求める -------------
		// Windows8以降
		wsprintf(keyN, ExtsChoiseStr, ext);
		if ( FALSE == GetRegString(HKEY_CURRENT_USER, keyN, ProgIDstr, appN, sizeof(appN)) ){
			// 従来
			if ( FALSE == GetRegString(HKEY_CLASSES_ROOT, ext, NilStr, appN, sizeof(appN)) ){
				// 全種
				if ( FALSE == GetRegString(HKEY_CLASSES_ROOT, WildCard_All, NilStr, appN, sizeof(appN))){
					tstrcpy(appN, T("Unknown")); // 該当無し
				}
			}
		}
	}else if ( ext[0] != '\0' ){
		tstrcpy(appN, ext);
	}else{
		tstrcpy(appN, T("Unknown")); // 拡張子無し
	}
	tstrcat(appN, T("\\shell"));
										// アプリケーションのシェル -----------
	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, appN, 0, KEY_READ, &hAppShellKey)){
		goto nomenuitem;
	}

	Bsize = sizeof(defN);
	defN[0] = '\0';
	RegQueryValueEx(hAppShellKey, NilStr, 0, &Rtyp, (LPBYTE)defN, &Bsize);
	if ( defN[0] == '\0' ) tstrcpy(defN, ShellVerb_open);	// デフォルトの指定が無い

	for ( ; ; cnt++ ){					// 設定を取り出す ---------------------
		keyS = TSIZEOF(keyN);
		if ( ERROR_SUCCESS != RegEnumKeyEx(hAppShellKey, cnt, keyN, &keyS, NULL, NULL, NULL, &write) ){
			break;
		}
		// keyがprintto(指定プリンタに印刷)以外  && shell\key name\command が開けるなら登録
		tstrcpy(comN, keyN);
		tstrcat(comN, T("\\command"));
		if ( tstricmp(keyN, T("printto")) && (ERROR_SUCCESS == RegOpenKeyEx(hAppShellKey, comN, 0, KEY_READ, &hCommandKey)) ){
			RegCloseKey(hCommandKey);
			minfo.dwTypeData = keyN;
							// デフォルトは太字にする
			minfo.fState = (tstricmp(defN, keyN) == 0) ?
					(MFS_ENABLED | MFS_DEFAULT) : MFS_ENABLED;

			if ( pmdi->th.top != MAX32 ){
				HKEY hAppItemKey;

				RegOpenKeyEx(hAppShellKey, keyN, 0, KEY_READ, &hAppItemKey);
				// 表示用の文字列があれば取得
				Bsize = sizeof(appN);
				appN[0] = '\0';
#ifdef UNICODE
				RegQueryValueEx(hAppItemKey, MUIVerbStr, 0, &Rtyp, (LPBYTE)appN, &Bsize);
				if ( appN[0] == '\0' ){
#endif
					Bsize = sizeof(appN);
					RegQueryValueEx(hAppItemKey, NilStr, 0, &Rtyp, (LPBYTE)appN, &Bsize);
#ifdef UNICODE
				}
#endif
				if ( appN[0] == '@' ){
#ifdef UNICODE
					if ( DSHLoadIndirectString == NULL ){
						GETDLLPROC(GetModuleHandle(T("shlwapi.dll")), SHLoadIndirectString);
					}
					if ( DSHLoadIndirectString != NULL ){
						DSHLoadIndirectString(appN, appN, MAX_PATH, NULL);
					}
#else
					appN[0] = '\0';
#endif
				}

				if ( appN[0] == '\0' ){ // 表示文字列が無いとき。
					Bsize = sizeof(appN);
					if ( RegQueryValueEx(hAppItemKey, PAOstr, 0, &Rtyp, (LPBYTE)appN, &Bsize) == ERROR_SUCCESS ){
						// ProgrammaticAccessOnly があるので表示しない
						RegCloseKey(hAppItemKey);
						continue;
					}

					if ( !tstricmp(keyN, ShellVerb_open) ){
						minfo.dwTypeData = (TCHAR *)MessageText(openstr);
					}else if ( !tstricmp(keyN, T("print")) ){
						minfo.dwTypeData = (TCHAR *)MessageText(printstr);
					}else if ( !tstricmp(keyN, T("play")) ){
						minfo.dwTypeData = (TCHAR *)MessageText(playstr);
					}else if ( !tstrcmp(keyN, T("runas")) ){
						minfo.dwTypeData = (TCHAR *)MessageText(
								(OSver.dwMajorVersion >= 6) ?
								runasV6str : runasV5str);
					}
				}else{
					minfo.dwTypeData = appN;
				}
				RegCloseKey(hAppItemKey);
				ThAddString(&pmdi->th, keyN);
			}
			InsertMenuItem(hSubMenu, 0xffff, TRUE, &minfo);
			minfo.wID++;
		}
	}
	RegCloseKey(hAppShellKey);
	if ( pmdi->id == minfo.wID ) goto nomenuitem;
	pmdi->id = minfo.wID;
	return cnt;

nomenuitem:
	minfo.dwTypeData = T("*open");
	minfo.fState = MFS_ENABLED;
	InsertMenuItem(hSubMenu, 0xffff, TRUE, &minfo);
	pmdi->id++;
	return 0;
}

// 1.20 時点で未使用
PPXDLL int PPXAPI PP_GetContextMenu(HMENU hSubMenu, const TCHAR *ext, DWORD *ID)
{
	PPXMENUDATAINFO pmdi;
	int result;

	pmdi.th.top = MAX32;
	pmdi.id = *ID;
	result = GetExtentionMenu(hSubMenu, ext, &pmdi);
	*ID = pmdi.id;
	return result;
}
