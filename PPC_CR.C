/*-----------------------------------------------------------------------------
	Paper Plane cUI											[Enter] コマンド
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

void RegisterAction(PPC_APPINFO *cinfo, HMENU hMenu, PPCMENUINFO *cminfo, BOOL always);

const TCHAR RegFileExtsPath[] = T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s");
const TCHAR RegApplicationName[] = T("Application");
const TCHAR RegPerceivedText[] = T("PerceivedType");
const TCHAR RegFA[] = T("SystemFileAssociations\\%s\\OpenWithList");
const TCHAR RegOpenWithList[] = T("\\OpenWithList");
const TCHAR DirName[] = T("Dir"); // M_CcrDir 用
const TCHAR NoextName[] = T("NoExt"); // M_CcrNoExt 用
const TCHAR SaveSettingsStr[] = MES_CRSS;
const TCHAR menuidstr[] = T("%M_Ccr,");
const TCHAR ExtTableID[] = T("E_cr");

const TCHAR CcrEditParam[] = T("%Ob *ppcust /:M_Ccr");
const TCHAR strMES_MCRC[] = MES_MCRC;
const TCHAR strMES_MCRD[] = MES_MCRD;
const TCHAR strMES_MCRE[] = MES_MCRE;
const TCHAR strMES_MCRX[] = MES_MCRX;
const TCHAR strMES_MEMD[] = MES_MEMD;
CRMENUSTACKCHECK DummyCrmCheck;

#define CRMOM_DEFAULTSELECT 1
#define CRMOM_DEFAULTEXECUTE 2
#define CRMOM_ADDEXTITEM 3
#define CRMOM_ADDCOMMONITEM 4
#define CRMOM_MODIFYITEM 5

// DelayMenus
#define DelayMenus_SENDTO	0
#define DelayMenus_DIRTYPE	1
#define DelayMenus_MAX 3 // 最後の NULL 分を含める

// 新規の場合は末端に登録する
int ReplaceCustTable(const TCHAR *str, const TCHAR *sub, const void *bin, size_t b_size)
{
	if ( !IsExistCustTable(str, sub) ){ // 新規登録
		return InsertCustTable(str, sub, 0, bin, b_size);
	}else{
		return SetCustTable(str, sub, bin, b_size);
	}
}

BOOL ShellExecEntriesSub(PPC_APPINFO *cinfo, const TCHAR *command, const TCHAR *csrentry, const TCHAR *path, const TCHAR *execpath, BOOL archivemode, DWORD runflag)
{
	const TCHAR *paramPtr;
	TCHAR name[VFPS], errmsg[VFPS * 2], param[VFPS * 2];
	HANDLE hProcess;
	int mode;

	if ( (command != NULL) && (command[0] == '*') ) command = NULL;

	if ( IsTrue(archivemode) ){
		csrentry = FindLastEntryPoint(csrentry);
	}
	VFSFullPath(name, (TCHAR *)csrentry, path);
	if ( VFSGetDriveType(name, &mode, NULL) != NULL ){
		if ( (mode <= VFSPT_SHN_DESK) || (mode == VFSPT_SHELLSCHEME) ){
			VFSGetRealPath(NULL, name, name);
		}
	}
	if ( execpath == NULL ){ // csrentry をそのまま実行
		paramPtr = NilStr;
	}else{ // csrentry をパラメータにして execpath を実行
		tstrcpy(param, execpath);
		tstrreplace(param, T("%1"), name);
		tstrreplace(param, T("%I"), T(":0"));
		tstrreplace(param, T("%L"), name);
		tstrreplace(param, T("%V"), name);
		tstrreplace(param, T("%W"), path);
		paramPtr = param;
		GetLineParam(&paramPtr, name); // 実行ファイルとパラメータとを切り離す
	}
	hProcess = PPxShellExecute(cinfo->info.hWnd, command, name, paramPtr, path, runflag, errmsg);
	if ( hProcess == NULL ){
		SetPopMsg(cinfo, POPMSG_MSG, errmsg);
		return FALSE;
	}
	if ( XC_exem && cinfo->e.markC ) for ( ;; ){
		DWORD result;

		result = WaitForInputIdle(hProcess, 100);
		if ( result != WAIT_TIMEOUT ) break;
		if ( (GetAsyncKeyState(VK_ESCAPE) | GetAsyncKeyState(VK_PAUSE)) & KEYSTATE_PUSH ){
			if ( PMessageBox(cinfo->info.hWnd, NULL, NULL, MB_QYES) == IDOK ){
				return FALSE;
			}
		}
	}
	CloseHandle(hProcess);
	return TRUE;
}

BOOL ShellExecEntries(PPC_APPINFO *cinfo, const TCHAR *command, const TCHAR *csrentry, const TCHAR *path, const TCHAR *execpath, BOOL archivemode)
{
	if ( !XC_exem || !cinfo->e.markC ){	// カーソル位置のみ実行
		return ShellExecEntriesSub(cinfo, command,
					csrentry, path, execpath, archivemode, 0);
	}else{					// 該当マークについて実行
		int work;
		ENTRYCELL *cell;

		InitEnumMarkCell(cinfo, &work);
		cell = EnumMarkCell(cinfo, &work);
		while ( cell != NULL ){
			csrentry = cell->f.cFileName;
			if ( ShellExecEntriesSub(cinfo, command,
					csrentry, path, execpath, archivemode, XEO_WAITIDLE) == FALSE ){
				return FALSE;
			}
			cell = EnumMarkCell(cinfo, &work);
		}
	}
	return TRUE;
}


typedef struct
{
	UINT cbSize;
	UINT fMask;
	UINT fType;
	UINT fState;
	UINT wID;
	HMENU hSubMenu;
	HBITMAP hbmpChecked;
	HBITMAP hbmpUnchecked;
	ULONG_PTR dwItemData;
	LPTSTR dwTypeData;
	UINT cch;
	HBITMAP hbmpItem;
} xMENUITEMINFO;
#ifndef MIIM_BITMAP
	#define MIIM_BITMAP      0x00000080
#endif

void SetMenuBmp(HMENU hMenu, HICON hIcon, DWORD id)
{
	HDC hMemDC;
	HBITMAP hbmp, hOldBmp;
	BITMAPINFOHEADER bmiHeader;
	LPVOID lpBits;
	xMENUITEMINFO mii;

	memset(&bmiHeader, 0, sizeof(bmiHeader));
	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = bmiHeader.biHeight = GetSystemMetrics(SM_CYSMICON);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 32;

	hMemDC = CreateCompatibleDC(NULL);
	hbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmiHeader, DIB_RGB_COLORS, &lpBits, NULL, 0);
	hOldBmp = (HBITMAP)SelectObject(hMemDC, hbmp);
	DrawIconEx(hMemDC, 0, 0, hIcon, bmiHeader.biWidth, bmiHeader.biWidth, 0, NULL, DI_NORMAL);
	SelectObject(hMemDC, hOldBmp);
	DeleteDC(hMemDC);

	mii.cbSize = sizeof(xMENUITEMINFO);
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbmp;
	SetMenuItemInfo(hMenu, id, FALSE, (MENUITEMINFO *)&mii);
}

void SetMenuBmpIdl(HMENU hMenu, LPSHELLFOLDER pSF, LPITEMIDLIST idl, DWORD id)
{
	XLPEXTRACTICON pEI;
	int iconsize;
	HICON hIcon = NULL;
	TCHAR iconname[MAX_PATH];
	int index;
	UINT flags;

	if ( FAILED(pSF->lpVtbl->GetUIObjectOf(pSF, NULL, 1, (LPCITEMIDLIST *)&idl, &IID_IExtractIcon, NULL, (void **)&pEI)) ){
		return;
	}
	iconsize = ( GetSystemMetrics(SM_CYMENUCHECK) > 16 ) ? 32 : 16;
	pEI->lpVtbl->GetIconLocation(pEI, 0, iconname, MAX_PATH, &index, &flags);
	if ( flags & GIL_NOTFILENAME ){
		// 注意 : Win2000 以前では NULL 不可
		pEI->lpVtbl->Extract(pEI, iconname, index, &hIcon, NULL, iconsize);
	}else{
		hIcon = LoadIconDx(iconname, index, iconsize);
	}
	pEI->lpVtbl->Release( pEI );
	if ( hIcon != NULL ){
		SetMenuBmp(hMenu, hIcon, id);
		DestroyIcon(hIcon);
	}
}

void AppendMenuNoDbl(HMENU hMenu, DWORD ID, const TCHAR *str)
{
	TCHAR menustr[CMDLINESIZE];
	DWORD i;

	for ( i = CRID_OPENWITH ; i < ID ; i++ ){
		GetMenuString(hMenu, i, menustr, TSIZEOF(menustr), MF_BYCOMMAND);
		if ( tstricmp(str, menustr) == 0 ) return;
	}
// HKEY_CURRENT_USER\Software\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache の名前を採用してもよい
	AppendMenuString(hMenu, ID, str);

	if ( OSver.dwMajorVersion >= 6 ){
		HICON hIcon;

		wsprintf(menustr, T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s"), str);
		if (GetRegString(HKEY_CURRENT_USER, menustr, NilStr, menustr, MAX_PATH) ||
			GetRegString(HKEY_LOCAL_MACHINE, menustr, NilStr, menustr, MAX_PATH) ){
			VFSFixPath(NULL, menustr, NULL, 0);
			str = menustr;
		}
		hIcon = LoadIconDx(str, LIDX_FILE, 32);
		if ( hIcon != NULL ){
			SetMenuBmp(hMenu, hIcon, ID);
			DestroyIcon(hIcon);
		}
	}
}

//-----------------------------------------------------------------------------
// OpenWithList を調査する(Windows2000用)
DWORD OpenWith2000(HMENU hMenu, const TCHAR *ext, HKEY hRegKey, DWORD IDmin)
{
	DWORD ID = IDmin;
	DWORD Rtyp;				// レジストリの種類
	DWORD keyS, appS;

	TCHAR rpath[MAX_PATH];	// アプリケーションのキー
	TCHAR keyN[MAX_PATH];	// レジストリのキー名称
	TCHAR defN[MAX_PATH];	// デフォルトのアクション
	TCHAR appN[MAX_PATH];	// デフォルトのアクション
	HKEY hAP;

	int cnt = 0;

	wsprintf(rpath, RegFileExtsPath, ext);

	defN[0] = '\0';
	GetRegString(hRegKey, rpath, RegApplicationName, defN, TSIZEOF(defN));

	tstrcat(rpath, RegOpenWithList);

	// Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\[ext]\OpenWithList から抽出
	if ( ERROR_SUCCESS == RegOpenKeyEx(hRegKey, rpath, 0, KEY_READ, &hAP) ){
		for ( ; ; ){					// 設定を取り出す ---------------------
			keyS = TSIZEOF(keyN);
			appS = TSIZEOF(appN);
			if ( ERROR_SUCCESS != RegEnumValue(hAP, cnt, keyN, &keyS, NULL,
					&Rtyp, (BYTE *)appN, &appS) ){
				break;
			}
			if ( keyN[1] == '\0' ){
				HKEY hTest;

				wsprintf(rpath, T("Applications\\%s\\shell\\Open"), appN);
				if ( ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT,
						rpath, 0, KEY_READ, &hTest) ){
					RegCloseKey(hTest);
					AppendMenuNoDbl(hMenu, ID, appN);
					ID++;
					if ( ID >= CRID_OPENWITH_MAX ) break;
				}
			}
			cnt++;
		}
	}
	RegCloseKey(hAP);
	// Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\[ext]\Application から抽出
	if ( (ID == CRID_OPENWITH) && defN[0] ) AppendMenuString(hMenu, ID++, defN);
	return ID;
}

void EnumOpenWithList(HMENU hMenu, DWORD *ID, const TCHAR *rpath)
{
	HKEY hAP;
	int cnt = 0;
	DWORD keyS;
	TCHAR keyN[MAX_PATH];	// レジストリのキー名称

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, rpath, 0, KEY_READ, &hAP)){
		return;
	}
	for ( ; ; ){					// 設定を取り出す ---------------------
		keyS = TSIZEOF(keyN);
		if ( ERROR_SUCCESS !=
				RegEnumKeyEx(hAP, cnt, keyN, &keyS, NULL, NULL, NULL, NULL) ){
			break;
		}
		if ( tstrchr(keyN, '.') != NULL ){
			AppendMenuNoDbl(hMenu, *ID, keyN);
			(*ID)++;
			if ( *ID >= CRID_OPENWITH_MAX ) break;
		}
		cnt++;
	}
	RegCloseKey(hAP);
}

// OpenWithList を調査する(WindowsXP以降用)
DWORD OpenWithXP(HMENU hMenu, const TCHAR *ext)
{
	DWORD ID = CRID_OPENWITH;
	TCHAR rpath[MAX_PATH];	// アプリケーションのキー
	TCHAR header[MAX_PATH];

	// CLASSES_ROOT\SystemFileAssociations\[ext]\OpenWithList から抽出
	wsprintf(rpath, RegFA, ext);
	EnumOpenWithList(hMenu, &ID, rpath);

	// CLASSES_ROOT\[ext]\PerceivedType に記載された CLASSES_ROOT\SystemFileAssociations\[PerceivedType]\OpenWithList から抽出
	header[0] = '\0';
	GetRegString(HKEY_CLASSES_ROOT, ext, RegPerceivedText, header, TSIZEOF(header));
	if ( header[0] ){
		wsprintf(rpath, RegFA, header);
		EnumOpenWithList(hMenu, &ID, rpath);
	}
	// CLASSES_ROOT\[ext]\OpenWithList から抽出
	tstrcpy(rpath, ext);
	tstrcat(rpath, RegOpenWithList);
	EnumOpenWithList(hMenu, &ID, rpath);
	return ID;
}

BOOL RPIDL2DisplayNameOf(TCHAR *name, LPSHELLFOLDER sfolder, LPCITEMIDLIST pidl)
{
	STRRET strr;

	if ( FAILED(sfolder->lpVtbl->GetDisplayNameOf(sfolder,
			pidl, SHGDN_INFOLDER | SHGDN_FORPARSING, &strr)) )
	{
		return FALSE;
	}
	switch (strr.uType){
		case STRRET_WSTR:
			#ifdef UNICODE
				tstrcpy(name, UNIONNAME(strr, pOleStr));
			#else
				UnicodeToAnsi(UNIONNAME(strr, pOleStr), name, VFPS);
			#endif
			break;

		case STRRET_OFFSET:
			tstrcpy(name, (TCHAR *)pidl + UNIONNAME(strr, uOffset));
			break;

		case STRRET_CSTR:
			#ifdef UNICODE
				AnsiToUnicode(UNIONNAME(strr, cStr), name, VFPS);
			#else
				tstrcpy(name, UNIONNAME(strr, cStr));
			#endif
			break;
	}
	return TRUE;
}

void WINAPI CRmenuDelayLoad(DelayLoadMenuStruct *DelayMenus)
{
	DWORD ID = CRID_OPENWITH;
	HMENU hMenu;
	TCHAR path[VFPS];
	TCHAR cmd[CMDLINESIZE];

	// プログラムを選択の一覧
	hMenu = DelayMenus->hMenu;
	if ( (OSver.dwMajorVersion > 5) ||
		 ((OSver.dwMajorVersion == 5) && OSver.dwMinorVersion) ){
		ID = OpenWithXP(hMenu, (const TCHAR *)DelayMenus->lParam);
	}
	ID = OpenWith2000(hMenu, (const TCHAR *)DelayMenus->lParam, HKEY_CURRENT_USER, ID);
	ID = OpenWith2000(hMenu, (const TCHAR *)DelayMenus->lParam, HKEY_LOCAL_MACHINE, ID);
	if ( ID > CRID_OPENWITH ) AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	// SendToの一覧
	{
		LPITEMIDLIST idl;
		LPSHELLFOLDER pSF;
		LPENUMIDLIST pEID;
		LPMALLOC pMA;

		pSF = VFPtoIShell(NULL, T("#9:\\"), NULL);
		if ( pSF != NULL ){
			SHGetMalloc(&pMA);

			if ( S_OK == pSF->lpVtbl->EnumObjects(pSF, NULL,
				(OSver.dwMajorVersion >= 6) ?
				ENUMOBJECTSFORFOLDERFLAG_VISTA : ENUMOBJECTSFORFOLDERFLAG_XP,
				&pEID) ){ // S_FALSE のときは、pEID = NULL
				for ( ; ; ){
					DWORD getsi;

					if ( pEID->lpVtbl->Next(pEID, 1, &idl, &getsi) != S_OK ){
						break;
					}
					if ( getsi == 0 ) break;

					if ( IsTrue(PIDL2DisplayNameOf(path, pSF, idl)) ){
						AppendMenuString(hMenu, DelayMenus->ID, path);
						if ( OSver.dwMajorVersion >= 6 ){
							SetMenuBmpIdl(hMenu, pSF, idl, DelayMenus->ID);
						}
						DelayMenus->ID++;

						RPIDL2DisplayNameOf(path, pSF, idl);
						wsprintf(cmd, T("*sendto \"%s"), path);
						ThAddString(DelayMenus->thMenuStr, cmd);
					}
					pMA->lpVtbl->Free(pMA, idl);
				}
				pEID->lpVtbl->Release(pEID);
			}
			pMA->lpVtbl->Release(pMA);
			pSF->lpVtbl->Release(pSF);
		}
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	}
	AppendMenuString(hMenu, CRID_LISTSTART, MES_ASTL);
	AppendMenuString(hMenu, CRID_SELECTEXE, MES_REGP);

	DelayMenus->hMenu = INVALID_HANDLE_VALUE; // 再度呼ばれないようにする
}

void WINAPI DirTypeDelayLoad(DelayLoadMenuStruct *DelayMenus)
{
	PPC_APPINFO *cinfo;

	cinfo = (PPC_APPINFO *)DelayMenus->lParam;
	IsFileDir(cinfo, CEL(cinfo->e.cellN).f.cFileName, NULL, NULL, DelayMenus->hMenu);
	if ( CEL(cinfo->e.cellN).f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
		AppendMenuString(DelayMenus->hMenu, CRID_DIRTYPE_JUNCTION, MES_JNJL);
		AppendMenuString(DelayMenus->hMenu, CRID_DIRTYPE_JUNCTION_PAIR, MES_JPJL);
	}

	if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){
		AppendMenuString(DelayMenus->hMenu, K_M | KC_JMPE, MES_DDJP);
	}

	if ( GetMenuItemCount(DelayMenus->hMenu) <= 0 ){
		AppendMenu(DelayMenus->hMenu, MF_GS, 0, T("none"));
	}
	DelayMenus->hMenu = INVALID_HANDLE_VALUE; // 再度呼ばれないようにする
}

BOOL IsArcInArc(PPC_APPINFO *cinfo)
{
	TCHAR *fp, *ep;
	BOOL arcmode = FALSE;
	TCHAR ParamHeader[8];

	fp = FindLastEntryPoint(CEL(cinfo->e.cellN).f.cFileName);
	ep = fp + FindExtSeparator(fp);
	if ( *ep == '.' ){				// 拡張子あり
		ep++;
		if ( NO_ERROR == GetCustTable(ExtTableID, ep, ParamHeader, sizeof(ParamHeader)) ){
			if ( ((UTCHAR)ParamHeader[0] == EXTCMD_KEY) &&
				 (*(WORD *)(ParamHeader + 1) == (WORD)KC_Edir) ){
				arcmode = TRUE;
			}
		}
	}
	if ( arcmode == FALSE ){
		FN_REGEXP fn;

		MakeFN_REGEXP(&fn, T(".lzh;.zip;.cab;.rar;.7z"));
		arcmode = FilenameRegularExpression(fp, &fn);
		FreeFN_REGEXP(&fn);
	}
	return arcmode;
}

UINT FindMenuID(HMENU hMenu, PPXMENUDATAINFO *pmdi, TCHAR *param, int *menupos, int *exec, TCHAR *Kword)
{
	TCHAR *destptr, *paramptr;
										// キーワードを抽出 -----------------
	paramptr = param + TSIZEOF(menuidstr) - 1;
	SkipSpace((const TCHAR **)&paramptr);
	if ( *paramptr == '!' ){
		if ( exec != NULL ) *exec = 1;
		paramptr++;
		SkipSpace((const TCHAR **)&paramptr);
	}
	destptr = Kword;
	if ( *paramptr == '\"' ){
		paramptr++;
		while ( *paramptr && (*paramptr != '\"') ) *destptr++ = *paramptr++;
	}else{
		while ( *paramptr && ((UTCHAR)*paramptr >' ') ) *destptr++ = *paramptr++;
	}
	*destptr = '\0';
										// キーワードを検索 -----------------
	if ( pmdi->th.top != 0 ){
		const TCHAR *action;
		DWORD id, pos;

		action = (const TCHAR *)pmdi->th.bottom;
		id = pmdi->id - CRID_EXTCMD;
		for ( pos = 0 ; pos < id ; pos++ ){
			if ( tstricmp(Kword, action) == 0 ){
				if ( menupos != NULL ) *menupos = pos;
				return id + CRID_EXTCMD;
			}
			action += tstrlen(action) + 1;
		}
	}
	return FindMenuItem(hMenu, Kword, menupos);
}

int reglist(PPC_APPINFO *cinfo, HMENU hMenu, PPCMENUINFO *cminfo, PPXMENUDATAINFO *pmdi, int TypeFlag, DelayLoadMenuStruct *DelayMenus)
{
	int cnt;
	DWORD ID;
	HMENU hSubMenu;
	TCHAR buf[VFPS];
	const TCHAR *extid;

	// OS の拡張子別の基本メニュー
	TCHAR LinkTarget[VFPS];
	const TCHAR *regext;

	regext = cminfo->regext;
	if ( tstricmp(regext, StrShortcutExt) == 0 ){
		if ( SUCCEEDED(GetLink(cinfo->info.hWnd, cminfo->PathName, LinkTarget)) && LinkTarget[0] ){
			DWORD attr;

			attr = GetFileAttributesL(LinkTarget);
			if ( attr != BADATTR ){
				if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
					regext = StrRegFolder;
				}else{
//					regext = NilStr;
				}
			}
		}
	}

	pmdi->id = CRID_EXTCMD;
	cnt = GetExtentionMenu(hMenu, regext, pmdi);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	// Send To & OS の拡張子別のプログラムメニュー
	hSubMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_EPOP, (UINT_PTR)hSubMenu, MessageText(MES_OWIT));
	DelayMenus[DelayMenus_SENDTO].hMenu = hSubMenu;
	DelayMenus[DelayMenus_SENDTO].lParam = (LPARAM)cminfo->regext;

	// シェルコンテキストメニュー
	AppendMenuString(hMenu, K_Mc | K_s | K_F10, MES_SHCM);
	if ( CEL(cinfo->e.cellN).f.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
		TypeFlag = PPEXTRESULT_DIR;
	}else if ( (TypeFlag < PPEXTRESULT_NONE) || (TypeFlag > PPEXTRESULT_FILE) ){
		if ( (TypeFlag == PPEXTRESULT_VFSDIR) ||
			 (TypeFlag == -PPEXTRESULT_VFSDIR) ||
			 IsFileDir(cinfo, CEL(cinfo->e.cellN).f.cFileName, NULL, NULL, NULL) ){
			TypeFlag = PPEXTRESULT_DIR;
		}else{
			TypeFlag = PPEXTRESULT_FILE;
		}
	}else{
		TypeFlag = PPEXTRESULT_FILE;
	}

	// ディレクトリ移動
	if ( (TypeFlag != PPEXTRESULT_FILE) || IsArcInArc(cinfo) ){
		AppendMenuString(hMenu, K_M | KC_Edir, MES_DREN);
		if ( Combo.Tabs ) AppendMenuString(hMenu, CRID_NEWTAB, MES_TABN);
	}
	// その他のディレクトリ移動
	hSubMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_EPOP, (UINT_PTR)hSubMenu, MessageText(MES_OWDT));
	DelayMenus[DelayMenus_DIRTYPE].hMenu = hSubMenu;
	DelayMenus[DelayMenus_DIRTYPE].lParam = (LPARAM)cinfo;

	// ListFile：該当エントリ移動／エントリ消去
	if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){
		AppendMenuString(hMenu, K_M | KC_JMPE, MES_DDJP);
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuString(hMenu, K_Mc | K_s | 'D', MES_ELST);
		AppendMenuString(hMenu, K_M | KC_UpdateEntryData, MES_UDED);
	}

	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	// 拡張子別メニュー
	extid = cminfo->regext;
	if ( *extid == '\0' ) extid = NoextName;
	if ( extid == StrRegFolder ) extid = DirName;
	wsprintf(buf, T("M_Ccr%s"), extid);

	ID = CRID_EXTMENU;
	PP_AddMenu(&cinfo->info, cinfo->info.hWnd, hMenu, &ID, buf, &cminfo->x.th);
	if ( ID != CRID_EXTMENU ) AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	cminfo->comID = ID;

	// 基本メニュー
	PP_AddMenu(&cinfo->info, cinfo->info.hWnd, hMenu, &ID, T("M_Ccr"), &cminfo->x.th);

	DelayMenus[DelayMenus_SENDTO].ID = cminfo->extitemID = ID;
	DelayMenus[DelayMenus_SENDTO].thMenuStr = &cminfo->x.th;
	return cnt;
}

//const TCHAR CrMenuR_str[] = T("ファイル判別メニューの再帰が起きている恐れがあります。\nE_crの内容を見直してください。");
void CrMenu(PPC_APPINFO *cinfo, BOOL ShowMenu)
{
	TCHAR Param[CMDLINESIZE], Kword[MAX_PATH];
	HMENU hMenu;
	int TypeFlag; // PPEXTRESULT_xxx
	PPCMENUINFO cminfo;
	PPXMENUDATAINFO pmdi;
	POINT pos;
	BOOL popmode, LongParam = FALSE;
	DelayLoadMenuStruct *OldDelayMenus, DelayMenus[DelayMenus_MAX];

	CRMENUSTACKCHECK *CrmCheck; // スタック異常等の検出用

	cinfo->FreezeType = FREEZE_CRMENU;
	cinfo->StateInfo.state = StateID_Crmenu;
	cinfo->StateInfo.tick = GetTickCount();

	CrmCheck = (CRMENUSTACKCHECK *)PPxCommonExtCommand(KC_GETCRCHECK, 0);
	if ( CrmCheck == (CRMENUSTACKCHECK *)(LONG_PTR)ERROR_INVALID_FUNCTION ){
		CrmCheck = &DummyCrmCheck;
	}
/*
	if ( cinfo->Ref > 25 ){
		XMessage(cinfo->info.hWnd, NULL, XM_FaERRd, CrMenuR_str);
		return;
	}
*/
	ThInit(&cminfo.x.th);
	ThInit(&pmdi.th);
	memset(DelayMenus, 0, sizeof(DelayMenus));
										// SHN 用 ----------------------------
	if ( (cinfo->RealPath[0] == '?') ||
		 (CEL(cinfo->e.cellN).f.dwFileAttributes & FILE_ATTRIBUTEX_VIRTUAL) ){
		cinfo->StateInfo.state = StateID_Celllook;
		cinfo->FreezeType = FREEZE_CRMENU_CELLLOOK;
		if ( !ShowMenu && IsTrue(CellLook(cinfo, -1)) ){
			cinfo->FreezeType = FREEZE_NONE;
			cinfo->StateInfo.state = StateID_NoState;
			return;
		}
		cinfo->FreezeType = FREEZE_CRMENU_SCMENU;
		cinfo->StateInfo.state = StateID_Scmenu;
		SCmenu(cinfo, NULL);
		cinfo->FreezeType = FREEZE_NONE;
		cinfo->StateInfo.state = StateID_NoState;
		return;
	}
	cminfo.comID = CRID_EXTMENU;
	cminfo.cellindex = cinfo->e.cellN;
	if ( XC_exem && cinfo->e.markC ){ // 該当マークについて実行なら１つ目を設定
		ENTRYINDEX newindex;

		newindex = GetFirstMarkCell(cinfo);
		if ( newindex >= 0 ) cminfo.cellindex = newindex;
	}

	if ( VFSFullPath(cminfo.PathName, CEL(cminfo.cellindex).f.cFileName, cinfo->RealPath) == NULL ){
		SetPopMsg(cinfo, ERROR_PATH_NOT_FOUND, NULL);
		cinfo->FreezeType = FREEZE_NONE;
		cinfo->StateInfo.state = StateID_NoState;
		return;
	}
	if ( !ShowMenu ){ // ディレクトリ移動
		if ( (X_ChooseMode == CHOOSEMODE_NONE) || (cinfo->e.markC == 0) ){
			if ( CEL(cinfo->e.cellN).attr & ECA_THIS ){	// "." ----------------
				if ( X_ChooseMode == CHOOSEMODE_NONE ){
					PPC_RootDir(cinfo);
					cinfo->StateInfo.state = StateID_NoState;
					cinfo->FreezeType = FREEZE_NONE;
					return;
				}
			}else if ( CEL(cinfo->e.cellN).attr & ECA_PARENT ){ // ".." -------
				PPC_UpDir(cinfo);
				cinfo->FreezeType = FREEZE_NONE;
				cinfo->StateInfo.state = StateID_NoState;
				return;
														// <dir> --------------
			}else if ( CEL(cinfo->e.cellN).f.dwFileAttributes &
						(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
				if ( (X_ChooseMode != CHOOSEMODE_NONE) ||
					 !IsExistCustTable(ExtTableID, T(":DIR")) ){
					cinfo->FreezeType = FREEZE_CRMENU_CELLLOOK;
					if ( IsTrue(CellLook(cinfo, -1)) ){
						cinfo->FreezeType = FREEZE_NONE;
						cinfo->StateInfo.state = StateID_NoState;
						return;
					}
					cinfo->FreezeType = FREEZE_CRMENU;
				}
			}
		}
		if ( X_ChooseMode != CHOOSEMODE_NONE ){ // Choose Mode の確定処理
			DoChooseResult(cinfo, Param);
			cinfo->StateInfo.state = StateID_NoState;
			cinfo->FreezeType = FREEZE_NONE;
			return;
		}
	}
	CrmCheck->enter++;
										// 拡張子から設定を取得 ---------------
	Param[CMDLINESIZE - 1] = '\0';
	if ( CEL(cinfo->e.cellN).f.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
		tstrcpy(cminfo.TypeName, T(":DIR"));
		TypeFlag = GetCustTable(ExtTableID, T(":DIR"), Param, sizeof(Param));
		if ( TypeFlag != 0 ) TypeFlag = PPEXTRESULT_NONE;
	}else{
		cinfo->FreezeType = FREEZE_CRMENU_GetExtCommand;
		cinfo->StateInfo.state = StateID_GetExtCommand;
		TypeFlag = PP_GetExtCommand(cminfo.PathName, ExtTableID, Param, cminfo.TypeName);
		cinfo->FreezeType = FREEZE_CRMENU;
	}
	cinfo->StateInfo.state = StateID_NoState;
	if ( Param[CMDLINESIZE - 1] != '\0' ){
		LongParam = TRUE;
		Param[CMDLINESIZE - 1] = '\0';
	}

	if ( tstrcmp(cminfo.TypeName, T(":DIR")) != 0 ){
		cminfo.regext = CEL(cminfo.cellindex).f.cFileName + CEL(cminfo.cellindex).ext;
		if ( *cminfo.regext == '\0' ){
			cminfo.regext = VFSFindLastEntry(cminfo.PathName);
			cminfo.regext += FindExtSeparator(cminfo.regext);
		}
	}else{
		cminfo.regext = StrRegFolder;
	}

	if ( !ShowMenu && (TypeFlag >= PPEXTRESULT_FILE) ){ // 定義済み動作有り
		TCHAR *ptr;

		ptr = Param;
		if ( (UTCHAR)*ptr == EXTCMD_KEY ){	// Key macro
			WORD *keyp;

			keyp = (WORD *)(ptr + 1);
			while ( *keyp != 0 ){
#ifndef _WIN64
				// ●↓0.40-0.41で混入したバグ対策
				if ( (*keyp & (K_ex | K_raw)) == (K_ex | K_raw) ){
					DeleteCustTable(ExtTableID, cminfo.TypeName, 0);
					break;
				}
#endif
				CrmCheck->exectype = *keyp;
				if ( (CrmCheck->exectype == (K_raw | K_cr)) &&
					 (CrmCheck->enter > 20) ){
					wsprintf(Param, T("E_cr:%s 中の @enter 指定によって再入が起きえているため、動作を中断しました。"), cminfo.TypeName);
					SetPopMsg(cinfo, POPMSG_MSG, Param);
				}else{
					PPcCommand(cinfo, *keyp++);
				}
			}
			CrmCheck->enter--;
			cinfo->FreezeType = FREEZE_NONE;
			return;
		}
		if ( (UTCHAR)*ptr == EXTCMD_CMD ) ptr++;

		SkipSpace((const TCHAR **)&ptr);
		if ( memcmp(ptr, menuidstr, TSIZEOF(menuidstr) - 1) == 0 ){
			int exec = 0;
			int menupos;
			UINT menuID;
										// キーワードに該当する項目を検索 -----

			hMenu = CreatePopupMenu();
			reglist(cinfo, hMenu, &cminfo, &pmdi, TypeFlag, DelayMenus);

			menuID = FindMenuID(hMenu, &pmdi, ptr, &menupos, &exec, Kword);
			if ( menuID != 0 ){
				if ( exec && !ShowMenu && (menuID <= CRID_MAX) ){ // 直接実行
					CrmCheck->exectype = 0x10;
					DoActionMenu(cinfo, hMenu, menuID, cminfo.cellindex, &cminfo.x.th, &pmdi, cminfo.comID);
					goto menufreefin;
				}else{ // カーソル移動
					for ( ; menupos >= 0 ; menupos-- ){
						PostMessage(cinfo->info.hWnd, WM_KEYDOWN, VK_DOWN, 0);
						PostMessage(cinfo->info.hWnd, WM_KEYUP, VK_DOWN, 0);
					}
				}
			}else{
				wsprintf(Param, MessageText(MES_EECR), Kword, cminfo.TypeName);
				SetPopMsg(cinfo, POPMSG_MSG, Param);
				TypeFlag = PPEXTRESULT_NONE; // 自動登録可能にする
			}
		}else{
			CrmCheck->exectype = 0x11;
			if ( LongParam == FALSE ){
				PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, ptr, NULL, 0);
			}else{
				int size = GetCustTableSize(ExtTableID, cminfo.TypeName);
				TCHAR *LongParamPtr;

				LongParamPtr = PPcHeapAlloc(size);
				if ( LongParamPtr != NULL ){
					GetCustTable(ExtTableID, cminfo.TypeName, LongParamPtr, size);
					PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL,
							LongParamPtr + (ptr - Param), NULL, 0);
					PPcHeapFree(LongParamPtr);
				}
			}
			goto freefin;
		}
	}else{						// 常時メニュー表示 or 該当なしの場合
		TCHAR *ptr = Param;
		UINT menuID;

		// ディレクトリ移動を試行
		if ( !ShowMenu && IsTrue(CellLook(cinfo, TypeFlag + 1)) ) goto freefin;

		// メニュー作成
		hMenu = CreatePopupMenu();

		if ( !reglist(cinfo, hMenu, &cminfo, &pmdi, TypeFlag, DelayMenus) && !ShowMenu ){
			VFSFullPath(cminfo.PathName, CEL(cminfo.cellindex).f.cFileName, cinfo->path);
			CrmCheck->exectype = 0x12;
			// 書庫内のディレクトリと思われるエントリは表示しない
			if ( ((cinfo->e.Dtype.mode != VFSDT_UN) &&
				  (cinfo->e.Dtype.mode != VFSDT_SUSIE)) ||
				 ((CEL(cminfo.cellindex).f.nFileSizeHigh == 0) &&
				  (CEL(cminfo.cellindex).f.nFileSizeLow != 0)) ){
				CrmCheck->exectype = 0x13;
				PPxView(cinfo->info.hWnd, cminfo.PathName, cinfo->NormalViewFlag);
			}
			goto menufreefin;
		}

		// デフォルト項目を見つけて強調表示
		if ( (UTCHAR)*ptr == EXTCMD_KEY ){
			menuID = *(WORD *)(ptr + 1) | K_M;
		}else{
			if ( (UTCHAR)*ptr == EXTCMD_CMD ) ptr++;
			SkipSpace((const TCHAR **)&ptr);
			if ( memcmp(ptr, menuidstr, TSIZEOF(menuidstr) - 1) == 0 ){
				menuID = FindMenuID(hMenu, &pmdi, ptr, NULL, NULL, Kword);
			}else{
				menuID = 0;
			}
		}
		if ( menuID != 0 ){
			MENUITEMINFO minfo;

			minfo.cbSize = sizeof(minfo);
			minfo.fMask = MIIM_STATE;
			minfo.fState = MFS_ENABLED | MFS_DEFAULT;
			SetMenuItemInfo(hMenu, menuID, MF_BYCOMMAND, &minfo);
		}
	}
										// メニュー表示
	DelayMenus[DelayMenus_SENDTO].initfunc = CRmenuDelayLoad;
	DelayMenus[DelayMenus_DIRTYPE].initfunc = DirTypeDelayLoad;

	cminfo.x.info = &cinfo->info;
	cminfo.x.commandID = PPXCMDID_MENUONMENU;
	GetPopupPosition(cinfo, &pos);
	popmode = PPxSetMenuInfo(hMenu, &cminfo.x);

	OldDelayMenus = cinfo->DelayMenus;
	cinfo->DelayMenus = DelayMenus;
	cminfo.x.index = TrackPopupMenu(hMenu,
		popmode ? TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RECURSE : TPM_TDEFAULT,
		pos.x, pos.y, 0, cinfo->info.hWnd, NULL);
	cinfo->DelayMenus = OldDelayMenus;
	CrmCheck->exectype = 0x14;
	if ( cminfo.x.Command != NULL ){
		CrmCheck->exectype = 0x15;
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, cminfo.x.Command, NULL, 0);
		PPcHeapFree(cminfo.x.Command);
	}

	if ( cminfo.x.index ){
		CrmCheck->exectype = 0x16;
										// ファイルの内容で判別 ---------------
		if ( GetShiftKey() & K_c ){
			RegisterAction(cinfo, hMenu, &cminfo, FALSE);
		}else if ( IsTrue(X_acr[0]) && (TypeFlag < PPEXTRESULT_FILE) ){ // 自動選択肢登録
			const TCHAR *extp;

			// 拡張子あり(ext/:type.ext)なら登録/拡張子無し(name.)は登録しない
			extp = cminfo.TypeName + FindExtSeparator(cminfo.TypeName);
			if ( (*extp == '\0') || (*(extp + 1) != '\0') ){ // '.' が末端以外?
				RegisterAction(cinfo, hMenu, &cminfo, TRUE);
			}
		}
		DoActionMenu(cinfo, hMenu, cminfo.x.index, cminfo.cellindex, &cminfo.x.th, &pmdi, cminfo.comID);
	}
menufreefin:
	DestroyMenu(hMenu);
freefin:
	ThFree(&cminfo.x.th);
	ThFree(&pmdi.th);
	if ( IsTrue(cinfo->UnpackFix) ) OffArcPathMode(cinfo);
	CrmCheck->enter--;
	cinfo->FreezeType = FREEZE_NONE;
}

void RegisterAction(PPC_APPINFO *cinfo, HMENU hMenu, PPCMENUINFO *cminfo, BOOL always)
{
	TCHAR param[VFPS], TypeText[MAX_PATH];
	const TCHAR *TypeName;
	VFSFILETYPE vft;
	const TCHAR *ext;

	if ( cminfo->x.index > CRID_MAX ) return; // 不明IDは何もしない

	if ( (cminfo->x.index >= CRID_COMMENT) && (cminfo->x.index <= CRID_SELECTEXE) ){	// プログラムを選択…まだ決定されてないので不可
		return;
	}

	ext = FindLastEntryPoint(cminfo->PathName);
	ext += FindExtSeparator(ext);

	TypeName = cminfo->TypeName;
	vft.flags = VFSFT_TYPE | VFSFT_STRICT;
	if ( VFSGetFileType(cminfo->PathName, NULL, 0, &vft) == NO_ERROR ){
		if ( IsExistCustTable(ExtTableID, vft.type) ){
			// 種類名が既にあるので拡張子付きにする
			wsprintf(TypeText, T("%s%s"), vft.type, ext);
			TypeName = TypeText;
		}else{
			// 種類名が既にないので種類名で登録
			TypeName = vft.type;
		}
	}
	always |= GetShiftKey() & K_s;

	// 選択肢の保存 =====================================================
	// メニュー選択のみ or CRID_EXTMENU / CRID_OPENWITH / CRID_SENDTO / CRID_NEWENTRY / CRID_EXTCMD
	if ( !always || (cminfo->x.index >= CRID_EXTMENU) ){
		const TCHAR *cmdp;
		TCHAR *dest;

		// いくつかの内蔵機能は自動登録をさせない
		GetMenuDataMacro2(cmdp, &cminfo->x.th, cminfo->x.index - CRID_EXTMENU);
		if ( cmdp != NULL ){
			if ( (*cmdp == '%') && (*(cmdp + 1) == 'K') ){ // %K
				// \@K
				if ( (*(cmdp + 3) == '\\') && (*(cmdp + 4) == '@') && (*(cmdp + 5) == 'D')){
					return;
				}
				// @^X @^C @C @M @D @R
				if ( *(cmdp + 3) == '@' ){
					if ( (*(cmdp + 4) == '^') && ((*(cmdp + 5) == 'X') || (*(cmdp + 5) == 'C') || (*(cmdp + 5) == 'V')) ){
						return;
					}
					if ( tstrchr(T("CMDR"), *(cmdp + 4)) != NULL ) return;
				}
			}
			if ( memcmp(cmdp + 1, T("*sendto"), 7 * sizeof(TCHAR)) == 0 ) return;
		}

		param[0] = EXTCMD_CMD;
		tstrcpy(param + 1, always ? T("%M_Ccr,!\"") : T("%M_Ccr,\""));
		dest = param + tstrlen(param);

		*dest = '\0';
		GetMenuString(hMenu, cminfo->x.index, dest, VFPS - 20, MF_BYCOMMAND);
		if ( *dest == '\0' ) return;
		if ( tstrchr(dest, '\t') ){
			const TCHAR *p;

			dest--;	// " を除去
			p = tstrchr(dest, '&');
			if ( p != NULL ){	// ショートカット有り
				dest[0] = *(p + 1);
				dest[1] = '\0';
			}else{
				#ifdef UNICODE
				dest[0] = dest[1];
				dest[1] = '\0';
				#else
				dest[1] = dest[2];
				dest[0] = dest[1];
				dest[Chrlen(*dest)] = '\0';
				#endif
			}
		}else{
			tstrcat(param, T("\""));
		}
		ReplaceCustTable(ExtTableID, TypeName, param, TSTRSIZE(param));
	}else if ( cminfo->x.index >= K_M ){	// 内蔵コマンド
		int key;

		key = cminfo->x.index & ~K_M;
		if ( !(key & K_ex) ) key |= K_raw;
		#ifdef UNICODE
			param[0] = EXTCMD_KEY;
			param[1] = (WORD)key;
			param[2] = 0;
			ReplaceCustTable(ExtTableID, TypeName, param, TSTROFF(3));
			// 拡張子もついでに登録
			if ( (*ext == '.') && !IsExistCustTable(ExtTableID, ext + 1) ){
				ReplaceCustTable(ExtTableID, ext + 1, param, TSTROFF(3));
			}
		#else
			param[0] = EXTCMD_KEY;
			*(WORD *)(param + 1) = (WORD)key;
			*(WORD *)(param + 3) = 0;
			ReplaceCustTable(ExtTableID, TypeName, param, 5);
			// 拡張子もついでに登録
			if ( (*ext == '.') && !IsExistCustTable(ExtTableID, ext + 1) ){
				ReplaceCustTable(ExtTableID, ext + 1, param, 5);
			}
		#endif
	}else{ // 不明IDは何もしない
		return;
	}
	wsprintf(param, T("%s(%s)"), MessageText(SaveSettingsStr), TypeName);
	SetPopMsg(cinfo, POPMSG_MSG, param);
}

void GetCcrID(PPC_APPINFO *cinfo, ENTRYINDEX cellindex, TCHAR *result)
{
	const TCHAR *ext;

	if ( CEL(cellindex).f.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
		ext = DirName;
	}else{
		ext = CEL(cellindex).f.cFileName + CEL(cellindex).ext;
	}
	if ( *ext == '\0' ) ext = NoextName;
	wsprintf(result, T("M_Ccr%s"), ext);
}

void AddProgToExtMenu(PPC_APPINFO *cinfo, ENTRYINDEX cellindex, const TCHAR *cmd, TCHAR *param)
{
	TCHAR typename[MAX_PATH];
	TCHAR keyname[MAX_PATH];

	tstrcpy(keyname, cmd);
	{
		TCHAR *p = keyname + FindExtSeparator(keyname);
		*p = '\0';
	}

	GetCcrID(cinfo, cellindex, typename);
	if ( !IsExistCustTable(typename, keyname) ){ // 未登録なので新規登録
		tstrreplace(param, T("%1"), T("%FNDC"));
		tstrreplace(param, T("%I"), T(":0"));
		tstrreplace(param, T("%L"), T("%FNDC"));
		tstrreplace(param, T("%V"), T("%FNDC"));
		tstrreplace(param, T("%W"), T("%1"));
		InsertCustTable(typename, keyname, 0, param, TSTRSIZE(param));
	}
}

void DoActionMenu_NewTab(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	TCHAR param[CMDLINESIZE];

	param[0] = '\"';
	VFSFullPath(param + 1, CEL(cellindex).f.cFileName, cinfo->path);
	CallPPcParam(Combo.hWnd, param);
}

void DoActionMenu_AddStartList(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	TCHAR param[CMDLINESIZE], buf[CMDLINESIZE];

	VFSFullPath(buf, CEL(cellindex).f.cFileName, cinfo->RealPath);
	wsprintf(param, T("*file !copy,\"%s\",\"#11:\\\",-mode:3"), buf);
	PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, param, NULL, 0);
}

void DoActionMenu_SelectExe(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	TCHAR param[CMDLINESIZE], buf[CMDLINESIZE];
	TCHAR cmd[VFPS];

	GetCcrID(cinfo, cellindex, buf);
	wsprintf(cmd, MessageText(MES_SSRP), buf);
	param[1] = '\0';

	if ( PPctInput(cinfo, cmd, param + 1, TSIZEOF(param) - 1,
			PPXH_NAME_R, PPXH_FILENAME) > 0 ){
		TCHAR *p;

		p = FindLastEntryPoint(param + 1);
		tstrcpy(cmd, p); // ファイル名部を取得
		p = tstrchr(cmd, '\"'); // セパレータ除去
		if ( p != NULL ) *p = '\0';
		*(cmd + FindExtSeparator(cmd)) = '\0'; // 拡張子除去

		if ( (*(param + 1) != '\"') && tstrchr(param + 1, ' ') ){
			*param = '\"';
			tstrcat(param, T("\""));
			p = param;
		}else{
			p = param + 1;
		}
		tstrcat(p, T(" %FDC"));

		ReplaceCustTable(buf, cmd, p, TSTRSIZE(p));
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, p, NULL, 0);
	}
}

void DoActionMenu_OpenWith(PPC_APPINFO *cinfo, HMENU hMenu, UINT crID, ENTRYINDEX cellindex, PPXMENUDATAINFO *pmdi)
{
	TCHAR withparam[CMDLINESIZE], buf[CMDLINESIZE];
	TCHAR cmd[VFPS], newpath[VFPS];
	const TCHAR *cfilename;
	BOOL archivemode;

	tstrcpy( newpath, cinfo->RealPath );

	cfilename = CEL(cellindex).f.cFileName;
	if ( IsTrue(cinfo->UnpackFix) ){
		archivemode = FALSE;
	}else{
		archivemode = PPcUnpackForAction(cinfo, newpath, UFA_ALL);
	}
	if ( IsTrue(archivemode) ){
		cfilename = FindLastEntryPoint(cfilename);
	}
	if ( hMenu != NULL ){
		GetMenuString(hMenu, crID, cmd, TSIZEOF(cmd), MF_BYCOMMAND);
	}else{
		cmd[0] = '\0';
	}

	if ( crID >= CRID_EXTCMD ){ // CRID_EXTCMD ... Open
		const TCHAR *action;

		action = (const TCHAR *)pmdi->th.bottom;
		if ( action != NULL ){
			crID -= CRID_EXTCMD;
			while ( crID-- ) action += tstrlen(action) + 1;
		}
		ShellExecEntries(cinfo, action, cfilename, newpath, NULL, archivemode);
		return;
	}

	if ( crID >= CRID_DIRTYPE ){ // CRID_DIRTYPE
		VFSFullPath(newpath, (TCHAR *)cfilename, cinfo->path);
		cinfo->Jfname[0] = '\0';

		switch ( crID ){
			case CRID_DIRTYPE_PAIR:
				if ( !(CEL(cellindex).f.dwFileAttributes &
					(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER)) ){
					tstrcpy(newpath, cinfo->path);
					tstrcpy(cinfo->Jfname, cfilename);
				}
				break;

			case CRID_DIRTYPE_CLSID:
			case CRID_DIRTYPE_CLSID_PAIR:
			case CRID_DIRTYPE_SHORTCUT:
			case CRID_DIRTYPE_SHORTCUT_PAIR:
				IsFileDir(cinfo, newpath, newpath, cinfo->Jfname, NULL);
				break;

			case CRID_DIRTYPE_JUNCTION:
			case CRID_DIRTYPE_JUNCTION_PAIR:
				GetReparsePath(newpath, cmd);
				if ( cmd[0] != '\0' ) tstrcpy(newpath, cmd);
				if ( !(CEL(cellindex).f.dwFileAttributes &
					(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER)) ){
					TCHAR *p = VFSFindLastEntry(newpath);

					if ( *p != '\0' ) tstrcpy(cinfo->Jfname, p + 1);
					*p = '\0';
				}
				break;

			case CRID_DIRTYPE_STREAM:
			case CRID_DIRTYPE_STREAM_PAIR:
				tstrcat(newpath, T("::stream"));
				break;

			default: // CRID_DIRTYPE～CRID_DIRTYPE_STREAM未満 書庫
				if ( cmd[0] != ':' ){
					if ( (cinfo->e.Dtype.mode == VFSDT_ZIPFOLDER) ||
						 (cinfo->e.Dtype.mode == VFSDT_UN) ||
						 (cinfo->e.Dtype.mode == VFSDT_SUSIE) ){
						if ( IsTrue(CellLook(cinfo, -2)) ) return;
					}
					tstrcat(newpath, T("::"));
				}
				tstrcat(newpath, cmd);
				break;
		}
		if ( (crID >= CRID_DIRTYPE_PAIR) || (GetShiftKey() & K_s) ){
			SetPairPath(cinfo, newpath, cinfo->Jfname);
			return;
		}

		if ( IsTrue(cinfo->ChdirLock) ){
			PPCuiWithPathForLock(cinfo, newpath);
		}else{
			SetPPcDirPos(cinfo);
			tstrcpy(cinfo->path, newpath);
			read_entry(cinfo, cinfo->Jfname[0] ? (RENTRY_READ | RENTRY_JUMPNAME) : RENTRY_READ);
		}
		return;
	}

	{					// Open With(CRID_OPENWITH)
		// パラメータを取得する(Windows2000のみ)
		wsprintf(buf, T("Applications\\%s\\shell\\Open\\Command"), cmd);
		if ( !GetRegString(HKEY_CLASSES_ROOT, buf, NilStr, withparam, TSIZEOF(withparam)) ){
			wsprintf(withparam, T("%s %%1"), cmd); // ない場合は初期設定(XPなど)
		}
		ShellExecEntries(cinfo, NULL, cfilename, newpath, withparam, archivemode);
		AddProgToExtMenu(cinfo, cellindex, cmd, withparam);
	}
}

void DoActionMenu(PPC_APPINFO *cinfo, HMENU hMenu, UINT crID, ENTRYINDEX cellindex, ThSTRUCT *THmenu, PPXMENUDATAINFO *pmdi, DWORD extmenuID)
{
	if ( crID > CRID_MAX ) return;
	if ( crID == CRID_NEWTAB ){	// 新規タブ
		DoActionMenu_NewTab(cinfo, cellindex);
		return;
	}
	if ( crID == CRID_SELECTEXE ){	// プログラムを選択
		DoActionMenu_SelectExe(cinfo, cellindex);
		return;
	}
	if ( crID == CRID_LISTSTART ){	// スタートメニュー／画面の一覧に登録
		DoActionMenu_AddStartList(cinfo, cellindex);
		return;
	}
	DxSetMotion(cinfo->DxDraw, DXMOTION_Launch);
	if ( crID >= CRID_OPENWITH ){
		DoActionMenu_OpenWith(cinfo, hMenu, crID, cellindex, pmdi);
		return;
	}
	if ( crID >= CRID_EXTMENU ){ // CRID_EXTMENU ～  拡張子別メニュー等 ======
		const TCHAR *menucmd;

		GetMenuDataMacro2(menucmd, THmenu, crID - CRID_EXTMENU);
		if ( menucmd != NULL ){
			if ( crID < extmenuID ){
				UnpackExec(cinfo, menucmd);
			}else{
				PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, menucmd, NULL, 0);
			}
		}
		return;
	}
	if ( crID >= K_M ){	// 内蔵コマンド
		PPcCommand(cinfo, (WORD)((crID - K_M) | K_raw));
		return;
	}
	SetPopMsg(cinfo, POPMSG_MSG, T("action id error"));
}

void ExecMenuCust(PPCMENUINFO *cminfo, const TCHAR *param)
{
	cminfo->x.Command = PPcHeapAlloc(TSTROFF(CMDLINESIZE));
	if ( cminfo->x.Command == NULL ) return;
	tstrcpy(cminfo->x.Command, param);
	PostMessage(cminfo->x.info->hWnd, WM_CHAR, 0x1f, 0); // メニューを閉じる
}

void PPcCRMenuOnMenu(PPC_APPINFO *cinfo, PPCMENUINFO *cminfo)
{
	HMENU hMenu;
	int index;
	POINT pos;
	TCHAR param[CMDLINESIZE];

	wsprintf(param, T("%s %s"), cminfo->regext, cminfo->TypeName);
	SetPopMsg(cinfo, POPMSG_NOLOGMSG, param);

	hMenu = CreatePopupMenu();
	if ( ((DWORD)cminfo->x.index < cminfo->extitemID) ||
		 ((cminfo->x.index >= CRID_EXTCMD) && (cminfo->x.index < CRID_NEWTAB))){
		AppendMenuString(hMenu, CRMOM_DEFAULTSELECT, strMES_MCRD);
		AppendMenuString(hMenu, CRMOM_DEFAULTEXECUTE, strMES_MCRE);

		if ( (cminfo->x.index >= CRID_EXTMENU) &&
			 ((DWORD)cminfo->x.index < cminfo->extitemID) ){
			AppendMenuString(hMenu, CRMOM_MODIFYITEM, strMES_MEMD);
		}
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	}

	AppendMenuString(hMenu, CRMOM_ADDEXTITEM, strMES_MCRX);
	AppendMenuString(hMenu, CRMOM_ADDCOMMONITEM, strMES_MCRC);
	TinyGetMenuPopPos(FindWindow(T(WNDCLASS_POPUPMENU), NULL), &pos);
	index = TrackPopupMenu(hMenu, TPM_TDEFAULT | TPM_RECURSE,
			pos.x, pos.y, 0, cinfo->info.hWnd, NULL);

	DestroyMenu(hMenu);
	switch (index){
		case CRMOM_DEFAULTSELECT:
		case CRMOM_DEFAULTEXECUTE:
			RegisterAction(cinfo, cminfo->x.hMenu, cminfo, (index == CRMOM_DEFAULTEXECUTE) );
			break;

		case CRMOM_ADDCOMMONITEM:
			ExecMenuCust(cminfo, CcrEditParam);
			break;

		case CRMOM_ADDEXTITEM: {
			const TCHAR *regext;

			tstrcpy(param, CcrEditParam);
			regext = cminfo->regext;
			if ( *regext == '\0' ) regext = NoextName;
			if ( regext == StrRegFolder ) regext = DirName;
			tstrcat(param, regext);
			ExecMenuCust(cminfo, param);
			break;
		}

		case CRMOM_MODIFYITEM:
			tstrcpy(param, CcrEditParam);
			if ( (DWORD)cminfo->x.index < cminfo->comID ){
				GetCcrID(cinfo, cminfo->cellindex, param + CMDLINESIZE / 2);
				wsprintf(param + tstrlen(param), T("%s:#%d"),
					param + CMDLINESIZE / 2 + 5, cminfo->x.index - CRID_EXTMENU);
			}else{
				wsprintf(param + tstrlen(param), T(":#%d"), cminfo->x.index - cminfo->comID);
			}
			PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, param, NULL, 0);
			break;
	}
}
