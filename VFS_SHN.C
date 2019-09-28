/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System			Shell's Namespace 操作
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#pragma hdrstop

#if defined(__BORLANDC__) && (__BORLANDC__ <= 0x509)
#undef  INTERFACE
#define INTERFACE IContextMenu3

DECLARE_INTERFACE_(IContextMenu3, IContextMenu2)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG, AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG, Release) (THIS) PURE;
	STDMETHOD(QueryContextMenu)(THIS_ HMENU hmenu, UINT indexMenu,
			UINT idCmdFirst, UINT idCmdLast, UINT uFlags) PURE;
	STDMETHOD(InvokeCommand)(THIS_ LPCMINVOKECOMMANDINFO lpici) PURE;
	STDMETHOD(GetCommandString)(THIS_ UINT_PTR idCmd, UINT uType,
			UINT * pwReserved, LPSTR pszName, UINT cchMax) PURE;
	STDMETHOD(HandleMenuMsg)(THIS_ UINT uMsg, WPARAM wParam, LPARAM lParam) PURE;
	STDMETHOD(HandleMenuMsg2)(THIS_ UINT uMsg, WPARAM wParam, LPARAM lParam,
			LRESULT* plResult) PURE;
};
typedef IContextMenu3 * LPCONTEXTMENU3;
#endif

const TCHAR MountString[] = T("SYSTEM\\MountedDevices");

/*-----------------------------------------------------------------------------
	ITEMIDLIST 関連
-----------------------------------------------------------------------------*/
#if 0
//-------------------------------------- idl の一つ下の階層のITEMIDLISTを返す
LPCITEMIDLIST GetSubPidl(LPCITEMIDLIST pidl)
{
	if ( pidl != NULL ){
		return (LPCITEMIDLIST) (LPBYTE)(((LPBYTE)pidl) + pidl->mkid.cb);
	}else{
		return NULL;
	}
}
#endif
//-------------------------------------- idlの全体の大きさを求める
VFSDLL UINT PPXAPI GetPidlSize(LPCITEMIDLIST pidl)
{
	UINT size = 0;

	if ( pidl != NULL ){
		USHORT cb;

		for (;;){
			cb = pidl->mkid.cb;
			if ( cb == 0 ) break;
			size += cb;
			pidl = (LPCITEMIDLIST) (LPBYTE)(((LPBYTE)pidl) + cb);
		}
		size += sizeof(USHORT);	// ターミネータ分を追加
	}
	return size;
}
//-------------------------------------- idl1 に idl2 を付け足す
LPITEMIDLIST CatPidl(LPMALLOC pMA, LPCITEMIDLIST idl1, LPCITEMIDLIST idl2)
{
	LPITEMIDLIST idlNew;
	UINT cb1, cb2;

	cb1 = (idl1 == NULL) ? 0 : GetPidlSize(idl1) - (sizeof(BYTE) * 2);
	cb2 = GetPidlSize(idl2);
	idlNew = (LPITEMIDLIST)pMA->lpVtbl->Alloc(pMA, cb1 + cb2);

	if ( idlNew != NULL ){
		if ( idl1 != NULL ) memcpy(idlNew, idl1, cb1);
		memcpy( ((BYTE *)idlNew) + cb1, idl2, cb2);
	}
	return idlNew;
}

//-------------------------------------- pIDL のコピーを作成する
LPITEMIDLIST DupIdl(LPMALLOC pMA, LPCITEMIDLIST pIDL)
{
	LPITEMIDLIST pNewIDL;
	UINT idlsize;

	idlsize = GetPidlSize(pIDL);
	pNewIDL = (LPITEMIDLIST)pMA->lpVtbl->Alloc(pMA, idlsize);
	if ( pNewIDL == NULL ) return NULL;
	memcpy(pNewIDL, pIDL, idlsize);
	return pNewIDL;
}

//-------------------------------------- pidl のメモリを解放する
VFSDLL void PPXAPI FreePIDL(LPCITEMIDLIST pidl)
{
	LPMALLOC pMA;

	if ( SUCCEEDED(SHGetMalloc(&pMA)) ){
		pMA->lpVtbl->Free(pMA, (LPITEMIDLIST)pidl);
		pMA->lpVtbl->Release(pMA);
	}
}

void FreePIDLS(LPITEMIDLIST *pidls, int cnt)
{
	LPMALLOC pMA;

	if ( FAILED(SHGetMalloc(&pMA)) ) return;
	while( cnt ){
		pMA->lpVtbl->Free(pMA, *pidls);
		pidls++;
		cnt--;
	}
	pMA->lpVtbl->Release(pMA);
}

//-------------------------------------- 指定の idl の表示名を取得する
VFSDLL BOOL PPXAPI PIDL2DisplayNameOf(TCHAR *name, LPSHELLFOLDER sfolder, LPCITEMIDLIST pidl)
{
	STRRET strr;

	if ( FAILED(sfolder->lpVtbl->GetDisplayNameOf(sfolder,
			pidl, SHGDN_INFOLDER, &strr)) )
//			pidl, SHGDN_INFOLDER | SHGDN_FORPARSING, &strr)) ) // ::{～} 形式
	{
		return FALSE;
	}
	switch (strr.uType){
		case STRRET_WSTR:
			if ( UNIONNAME(strr, pOleStr) == NULL ) return FALSE;
			#ifdef UNICODE
				tstrcpy(name, UNIONNAME(strr, pOleStr));
			#else
				UnicodeToAnsi(UNIONNAME(strr, pOleStr), name, VFPS);
			#endif
				CoTaskMemFree(UNIONNAME(strr, pOleStr));
			break;

		case STRRET_OFFSET:
			#ifdef UNICODE
				AnsiToUnicode((char *)pidl + UNIONNAME(strr, uOffset), name, VFPS);
			#else
				tstrcpy(name, (char *)pidl + UNIONNAME(strr, uOffset));
			#endif
			break;

		case STRRET_CSTR:
			#ifdef UNICODE
				AnsiToUnicode(UNIONNAME(strr, cStr), name, VFPS);
			#else
				tstrcpy(name, UNIONNAME(strr, cStr));
			#endif
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

LPITEMIDLIST PathToPidl2(const TCHAR *vfs)
{
	LPITEMIDLIST idl = NULL, idl2, idlc;
	LPSHELLFOLDER pSF;
	LPMALLOC pMA;
	TCHAR tempdir[VFPS], *p;
	HWND hWnd = NULL;

	pSF = VFPtoIShell(hWnd, vfs, &idl);
	if ( pSF == NULL ){ // 最後のエントリがbindできないと思われるので試行する
		TCHAR c;

		tstrcpy(tempdir, vfs);
		p = VFSFindLastEntry(tempdir);
		if ( (c = *p) == '\0' ) return NULL; // エントリがない
		*p = '\0';
		pSF = VFPtoIShell(hWnd, tempdir, &idl); // 最終エントリ以外で取得
		if ( pSF == NULL ) return NULL;
									// 最終エントリを処理
		if ( c == '\\' ){
			p++;
		}else{
			*p = c;
		}
		idl2 = BindIShellAndFname(pSF, p);
		if ( idl2 == NULL ){
			pSF->lpVtbl->Release(pSF);
			return NULL;
		}
		SHGetMalloc(&pMA);
		idlc = CatPidl(pMA, idl, idl2);
		pMA->lpVtbl->Free(pMA, idl2);
		pMA->lpVtbl->Free(pMA, idl);
		pMA->lpVtbl->Release(pMA);
		idl = idlc;
	}
	pSF->lpVtbl->Release(pSF);
	return idl;
}

/*-----------------------------------------------------------------------------
	Fullpath で構成された pszFile を絶対指定の ITEMIDLIST に変換する。
	NULL なら失敗
-----------------------------------------------------------------------------*/
VFSDLL LPITEMIDLIST PPXAPI PathToPidl(LPCTSTR pszFile)
{
	LPSHELLFOLDER piDesktop;
	LPITEMIDLIST pidl;
									// shell name space のroot(desktop)を取得
	if ( FAILED(SHGetDesktopFolder(&piDesktop)) ) return NULL;

	pidl = IShellToPidl(piDesktop, pszFile);
	piDesktop->lpVtbl->Release(piDesktop);
	if ( pidl == NULL ) pidl = PathToPidl2(pszFile);
	return pidl;
}
/*=============================================================================
	IShellFolder 関連
=============================================================================*/
/*-----------------------------------------------------------------------------
	絶対指定のフォルダパスから IShell を取得する（末尾の \ の有無は問わない）
	また、絶対形式のidlを取得できる
-----------------------------------------------------------------------------*/
VFSDLL LPSHELLFOLDER PPXAPI VFPtoIShell(HWND hWnd, const TCHAR *vfp, LPITEMIDLIST *idl)
{
	LPSHELLFOLDER pSF;
	TCHAR	vfpTemp[VFPS];	// パスの保存場所
	int		mode;
	TCHAR	*vp, *p;
										// 検索対象の種類を判別 ---------------
	tstrcpy(vfpTemp, vfp);
	vp = VFSGetDriveType(vfpTemp, &mode, NULL);
	if ( vp == NULL ){		// 種類が分からない→相対指定の可能性→絶対化
		VFSFullPath(vfpTemp, (TCHAR *)vfp, NULL);
		vp = VFSGetDriveType(vfpTemp, &mode, NULL);
		if ( vp == NULL ) return NULL;	// それでも種類が分からない→エラー
	}
										// 末尾の \ を除去 --------------------
	p = vfpTemp;
	if ( mode == VFSPT_UNC ) p += 2;
	for ( ; ; ){
		p = FindPathSeparator(p);
		if ( p == NULL ) break;
		p++;
		if ( *p == '\0' ){
			*(p - 1) = '\0';
			break;
		}
	}
										// 種類別の処理 -----------------------
	switch (mode){
		case VFSPT_DRIVELIST:
			return VFPtoIShell(hWnd, vp, idl);

		case VFSPT_HTTP:
		case VFSPT_FTP:
			return NULL;

		case VFSPT_DRIVE:
										// 「x:\...」の idl を入手する
			if ( *vp == '\0' ){	// rootを示す「\」が無いなら追加
				*vp = '\\';
				*(vp + 1) = '\0';
			}
			vp -= 2;
			break;

		case VFSPT_UNC:
			vp -= 2;
			break;

		case VFSPT_SHELLSCHEME:
			vp = vfpTemp;
			if ( (*vp == '+') || (*vp == '-') ) vp++;
			break;

		default:							// 不明 ---------------------------
			if ( mode >= 0 ) return NULL;
										// PIDL:Special folder ----------------
	}

	if ( VFPtoIShellSub(hWnd, vp, mode, &pSF, idl, NULL, NULL) != NO_ERROR ){
		return NULL;
	}
	return pSF;
}

DWORD GetIpdlSum(LPITEMIDLIST pidl)
{
	DWORD sum = 0;
	USHORT size = (USHORT)(pidl->mkid.cb - sizeof(USHORT));
	BYTE *idlptr = (BYTE *)pidl->mkid.abID;

	for(;;){
		sum += *idlptr++;
		if ( --size == 0 ) break;
	}
	// idlist が複数あるときは対象外にする
	if ( *((USHORT*)idlptr) != 0 ) sum = 0;
	return sum;
}

VFSDLL LPITEMIDLIST PPXAPI BindIShellAndFdata(LPSHELLFOLDER pParentFolder, WIN32_FIND_DATA *fdata)
{
	LPITEMIDLIST pidl, bkpidl;
	LPENUMIDLIST pEID;
	LPMALLOC pMA;

	pidl = IShellToPidl(pParentFolder, fdata->cFileName);
	if ( (pidl != NULL) && (GetIpdlSum(pidl) == fdata->dwReserved1) ){
		return pidl;
	}

	if ( S_OK != pParentFolder->lpVtbl->EnumObjects(pParentFolder, NULL,
			MAKEFLAG_EnumObjectsForFolder, &pEID) ){
		return NULL;
	}
	SHGetMalloc(&pMA);
	bkpidl = NULL;

	for ( ; ; ){
		TCHAR name[VFPS];
		DWORD count;

		if ( (pEID->lpVtbl->Next(pEID, 1, &pidl, &count) != S_OK) || (count == 0) ){
			pidl = bkpidl;
			break;
		}
		if ( IsTrue(PIDL2DisplayNameOf(name, pParentFolder, pidl)) ){
			if ( !tstricmp(name, fdata->cFileName) ){
				if ( GetIpdlSum(pidl) == fdata->dwReserved1 ){
					if ( bkpidl != NULL ) pMA->lpVtbl->Free(pMA, bkpidl);
					break;
				}else if ( bkpidl == NULL ){
					bkpidl = pidl;
					continue;
				}
			}
		}
		pMA->lpVtbl->Free(pMA, pidl);
	}
	pMA->lpVtbl->Release(pMA);
	pEID->lpVtbl->Release(pEID);
	return pidl;
}
//-------------------------------------- fname に対応した idl を求める
VFSDLL LPITEMIDLIST PPXAPI BindIShellAndFname(LPSHELLFOLDER pParentFolder, const TCHAR *fname)
{
	LPITEMIDLIST pidl;

	pidl = IShellToPidl(pParentFolder, fname);
	if ( pidl == NULL ){
		LPENUMIDLIST pEID;
		LPMALLOC pMA;

		if ( S_OK != pParentFolder->lpVtbl->EnumObjects(pParentFolder, NULL,
				MAKEFLAG_EnumObjectsForFolder, &pEID) ){
			return NULL;
		}
		SHGetMalloc(&pMA);
		for ( ; ; ){
			TCHAR name[VFPS];
			DWORD count;

			if ( (pEID->lpVtbl->Next(pEID, 1, &pidl, &count) != S_OK) || (count == 0) ){
				pidl = NULL;
				break;
			}
			if ( IsTrue(PIDL2DisplayNameOf(name, pParentFolder, pidl)) ){
				if ( !tstricmp(name, fname) ) break;
			}
			pMA->lpVtbl->Free(pMA, pidl);
		}
		pMA->lpVtbl->Release(pMA);
		pEID->lpVtbl->Release(pEID);
	}
	return pidl;
}
/*-----------------------------------------------------------------------------
	pParentFolder を親として pszFile を相対指定の ITEMIDLIST に変換する。
	NULL なら失敗
-----------------------------------------------------------------------------*/
VFSDLL LPITEMIDLIST PPXAPI IShellToPidl(LPSHELLFOLDER pParent, LPCTSTR pszFile)
{
	LPITEMIDLIST pidl;
	ULONG chEaten, atr = 0;
	#ifndef UNICODE
		OLECHAR olePath[VFPS];
									// 32bit OLE は UNICODE を使用するので変換
		AnsiToUnicode(pszFile, olePath, VFPS);
									// ParseDisplayName を使ってIDLISTに変換
		if( FAILED(pParent->lpVtbl->ParseDisplayName(
				pParent, NULL, NULL, olePath, &chEaten, &pidl, &atr)) ){
			#if CHECKVFXEXNAME
				XMessage(NULL, NULL, XM_DbgLOG, T("IShellToPidl fail:%s"), pszFile);
			#endif
			return NULL;
		}
	#else
		if( FAILED(pParent->lpVtbl->ParseDisplayName(
				pParent, NULL, NULL, (TCHAR *)pszFile, &chEaten, &pidl, &atr)) ){
			#if CHECKVFXEXNAME
				XMessage(NULL, NULL, XM_DbgLOG, T("IShellToPidl fail:%s"), pszFile);
			#endif
			return NULL;
		}
	#endif
	return pidl;
}
/*-----------------------------------------------------------------------------
	pidl を IShellFolder 形式に変換
	NULL なら失敗

	※ #:\ライブラリ\ドキュメント とかは対応していない
-----------------------------------------------------------------------------*/
VFSDLL LPSHELLFOLDER PPXAPI PidlToIShell(LPCITEMIDLIST pidl)
{
	LPSHELLFOLDER piDesktop;
	LPSHELLFOLDER pParent;
	HRESULT hr;
									// shell name space のroot(desktop)を取得
	if ( FAILED(SHGetDesktopFolder(&piDesktop)) ) return NULL;
									// BindToObject を使って関連付け
	hr = piDesktop->lpVtbl->BindToObject(piDesktop,
			pidl, NULL, &XIID_IShellFolder, (LPVOID *)&pParent);
	piDesktop->lpVtbl->Release(piDesktop);

	if ( FAILED(hr) ) return NULL;
	return pParent;
}
//-------------------------------------- SHN パスなら、実体パスを取得
VFSDLL BOOL PPXAPI VFSGetRealPath(HWND hWnd, TCHAR *path, const TCHAR *vfs)
{
	LPITEMIDLIST idl = NULL, idl2, idlc;
	LPSHELLFOLDER pSF;
	LPMALLOC pMA;
	BOOL result;
	TCHAR tempdir[VFPS], *ptr;

	pSF = VFPtoIShell(hWnd, vfs, &idl);
	if ( pSF == NULL ){ // 最後のエントリがbindできないと思われるので試行する
		TCHAR c;

		tstrcpy(tempdir, vfs);
		ptr = VFSFindLastEntry(tempdir);
		if ( (c = *ptr) == '\0' ) goto error; // エントリがない
		*ptr = '\0';
		pSF = VFPtoIShell(hWnd, tempdir, &idl); // 最終エントリ以外で取得
		if ( pSF == NULL ) goto error;
									// 最終エントリを処理
		if ( c == '\\' ){
			ptr++;
		}else{
			*ptr = c;
		}
		idl2 = BindIShellAndFname(pSF, ptr);
		if ( idl2 == NULL ){
			pSF->lpVtbl->Release(pSF);
			goto error;
		}
		SHGetMalloc(&pMA);
		idlc = CatPidl(pMA, idl, idl2);
		pMA->lpVtbl->Free(pMA, idl2);
		pMA->lpVtbl->Free(pMA, idl);
		pMA->lpVtbl->Release(pMA);
		idl = idlc;
	}
	if ( (result = SHGetPathFromIDList(idl, path)) == FALSE ) path[0] = '\0';
	FreePIDL(idl);
	return result;
error:
	path[0] = '\0';
	return FALSE;
}
/*=============================================================================
	コンテキストメニュー
=============================================================================*/
#define MENUCLASS2 T("PPXSHC2") T(TAPITAIL)
LRESULT CALLBACK C2Proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

WNDCLASS c2Class = { 0, C2Proc, 0, 0, NULL, NULL, NULL, NULL, NULL, MENUCLASS2 };

typedef struct {
	LPCONTEXTMENU	cm;
	LPCONTEXTMENU2	cm2;
	LPCONTEXTMENU3	cm3;
} CONTEXTMENUS;

//-----------------------------------------------------------------------------
LRESULT CALLBACK C2Proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CONTEXTMENUS *cms;

	if (	(message == WM_INITMENUPOPUP) || (message == WM_DRAWITEM) ||
			(message == WM_MEASUREITEM) ){
		cms = (CONTEXTMENUS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if ( cms != NULL ){
			if ( cms->cm3 != NULL ){
				cms->cm3->lpVtbl->HandleMenuMsg(cms->cm3, message, wParam, lParam);
			}else if ( cms->cm2 != NULL ){
				cms->cm2->lpVtbl->HandleMenuMsg(cms->cm2, message, wParam, lParam);
			}
		}
		return (message == WM_INITMENUPOPUP) ? 0 : TRUE;
	}
	if ( message == WM_MENUCHAR ){
		LRESULT result = 0;

		cms = (CONTEXTMENUS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if ( (cms != NULL) && (cms->cm3 != NULL) ){
			cms->cm3->lpVtbl->HandleMenuMsg2(cms->cm3, message, wParam, lParam, &result);
		}
		return result;
	}

	if ( message == WM_CREATE ){
		SetWindowLongPtr(hWnd, GWLP_USERDATA,
						(LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void InvokeMenu(CONTEXTMENUS *cms, CMINVOKECOMMANDINFO *cmi)
{
	if ( cms->cm3 != NULL ){
		cms->cm3->lpVtbl->InvokeCommand(cms->cm3, cmi);
	}else if ( cms->cm2 != NULL ){
		cms->cm2->lpVtbl->InvokeCommand(cms->cm2, cmi);
	}else{
		cms->cm->lpVtbl->InvokeCommand(cms->cm, cmi);
	}
}
/*-----------------------------------------------------------------------------
	Shell's Namespace ファイル名の ContextMenu 処理を行う
	lpsfParent + lpi	ファイル名
	lppt				popupmenu の表示位置
	Cmd					デフォルト
-----------------------------------------------------------------------------*/
#define BASEINDEX 1
#define LASTINDEX 0x7fff
VFSDLL BOOL PPXAPI SHContextMenu(HWND hWnd, LPPOINT lppt, LPSHELLFOLDER lpsfParent, LPITEMIDLIST *lpi, int pidls, const TCHAR *cmd)
{
	#ifdef UNICODE
		CMINVOKECOMMANDINFOEX cmi;
		char VerbA[128];
		#define CMIFLAG CMIC_MASK_UNICODE
		#define CMISTRUCT (CMINVOKECOMMANDINFO *)
		#define SetIntresVerb(cmd) cmi.lpVerb = (char *)(cmi.lpVerbW = cmd)
	#else
		CMINVOKECOMMANDINFO cmi;
		#define CMIFLAG 0
		#define CMISTRUCT
		#define SetIntresVerb(cmd) cmi.lpVerb = cmd
	#endif
	CONTEXTMENUS cms;
	int idCmd;
	HWND hCWnd = NULL;
	HMENU hMenu;
	BOOL result = FALSE;
										// IContextMemu を取得 ----------------
	if ( FAILED(lpsfParent->lpVtbl->GetUIObjectOf(lpsfParent, hWnd, pidls,
			(LPCITEMIDLIST *)lpi, &XIID_IContextMenu, NULL, (void **)&cms.cm)) ){
		return FALSE;
	}
	if ( FAILED(cms.cm->lpVtbl->QueryInterface(cms.cm,
				&XIID_IContextMenu2, (void **)&cms.cm2)) ){
		cms.cm2 = NULL;
		cms.cm3 = NULL;
	}else{
		if ( FAILED(cms.cm2->lpVtbl->QueryInterface(cms.cm2,
				&XIID_IContextMenu3, (void **)&cms.cm3)) ){
			cms.cm3 = NULL;
		}
	}
	hMenu = CreatePopupMenu();
	if ( hMenu != NULL ){
		LRESULT lr;
										// ContextMemu を PopupMenu に設定 ----
		if ( cms.cm3 != NULL ){
			lr = cms.cm3->lpVtbl->QueryContextMenu(cms.cm3, hMenu, 0,
						BASEINDEX, LASTINDEX, CMF_NORMAL | CMF_EXTENDEDVERBS);
		}else if ( cms.cm2 != NULL ){
			lr = cms.cm2->lpVtbl->QueryContextMenu(cms.cm2, hMenu, 0,
						BASEINDEX, LASTINDEX, CMF_NORMAL | CMF_EXTENDEDVERBS);
		}else{
			lr = cms.cm->lpVtbl->QueryContextMenu(cms.cm, hMenu, 0,
						BASEINDEX, LASTINDEX, CMF_NORMAL | CMF_EXTENDEDVERBS);
		}
		if ( SUCCEEDED(lr) ){
			memset(&cmi, 0, sizeof(cmi));

			cmi.cbSize	= sizeof(cmi);
			cmi.fMask	= CMIFLAG;
			cmi.hwnd	= hWnd;
			cmi.nShow	= SW_SHOWNORMAL;

			if ( cmd > (const TCHAR *)(DWORD_PTR)0x10000 ){
				if ( *cmd == '\0' ){ // NilStr-デフォルト実行
					MENUITEMINFO minfo;
					int index = 0;

					cmd = NULL;
					for ( ; ; index++ ){
						minfo.cbSize = sizeof(minfo); // GetMenuItemInfo の度に内容が破壊？
						minfo.fMask = MIIM_STATE | MIIM_ID;
						if ( GetMenuItemInfo(hMenu, index, MF_BYPOSITION, &minfo) == FALSE ) break;
						if ( (minfo.fState & MFS_DEFAULT) && (minfo.fState != (UINT)-1) ){
							cmd = MAKEINTRESOURCE(minfo.wID - BASEINDEX);
							break;
						}
					}
				}else if ( *(cmd + 1) == '\0' ){ // 1文字... ショートカット
					UINT menuID;

					menuID = FindMenuItem(hMenu, cmd, NULL);
					if ( menuID != 0 ){
						cmd = MAKEINTRESOURCE(menuID - BASEINDEX);
					}else{
						cmd = NULL;
					}
				}
			}
			if ( cmd != NULL ){
				#ifndef UNICODE
					cmi.lpVerb = cmd;
				#else
					if ( IS_INTRESOURCE(cmd) ){
						cmi.lpVerb = (char *)(cmi.lpVerbW = cmd);
					}else{
						UnicodeToAnsi(cmd, VerbA, sizeof(VerbA) - 1);
						VerbA[sizeof(VerbA) - 1] = '\0';
						cmi.lpVerb = VerbA;
						cmi.lpVerbW = cmd;
					}
				#endif
				InvokeMenu(&cms, CMISTRUCT &cmi);
				if ( SUCCEEDED(lr) ) result = TRUE;
			}else{
										// ContextMemu 用ウィンドウを作成 -----
				c2Class.hCursor   = LoadCursor(NULL, IDC_ARROW);
				c2Class.hInstance = DLLhInst;
				RegisterClass(&c2Class);

				hCWnd = CreateWindow(MENUCLASS2, MENUCLASS2,
						(hWnd != NULL) ? WS_CHILD : 0,
						0, 0, 0, 0, hWnd, NULL, DLLhInst, (LPVOID)&cms);

				idCmd = TrackPopupMenu(hMenu, TPM_TDEFAULT,
						lppt->x, lppt->y, 0, hCWnd, NULL);
										// ContextMemu の項目を実行 --------
				if ( idCmd > 0 ){
					SetIntresVerb((LPTSTR)MAKEINTRESOURCE(idCmd - BASEINDEX));
					InvokeMenu(&cms, CMISTRUCT &cmi);
				}
				result = TRUE;
			}
		}
		DestroyMenu(hMenu);
	}
	if ( cms.cm3 != NULL ) cms.cm3->lpVtbl->Release(cms.cm3);
	if ( cms.cm2 != NULL ) cms.cm2->lpVtbl->Release(cms.cm2);
	if ( hCWnd != NULL ) PostMessage(hCWnd, WM_CLOSE, 0, 0);
	cms.cm->lpVtbl->Release(cms.cm);
	return result;
}

LPITEMIDLIST GetCachedFnameIdl(LPSHELLFOLDER pSF, const TCHAR *fullpath, const TCHAR *name)
{
	int cachedsize;
	LPITEMIDLIST idl;
	LPMALLOC pMA;
	TCHAR bufname[VFPS];

	cachedsize = GetCustTableSize(IdlCacheName, fullpath + 2);
	if ( cachedsize <= 0 ) return NULL;

	SHGetMalloc(&pMA);
	idl = pMA->lpVtbl->Alloc(pMA, cachedsize);
	GetCustTable(IdlCacheName, fullpath + 2, idl, cachedsize);

	bufname[0] = '\0';
	if ( IsTrue(PIDL2DisplayNameOf(bufname, pSF, idl)) ){
		if ( tstrcmp(name, bufname) == 0 ){
			pMA->lpVtbl->Release(pMA);
			return idl;
		}
	}
	pMA->lpVtbl->Free(pMA, idl);
	pMA->lpVtbl->Release(pMA);
	return NULL;
}

// IShellFolder を用意する
VFSDLL BOOL PPXAPI VFSMakeIDL(const TCHAR *path, LPSHELLFOLDER *ppSF, LPITEMIDLIST *pidl, const TCHAR *filename)
{
	LPITEMIDLIST idl = NULL;
	LPSHELLFOLDER pSF;
	TCHAR name1[VFPS], *fname;

	fname = VFSFullPath(name1, (TCHAR *)filename, path);
	if ( fname == NULL ) return FALSE;
	if ( *fname == '\0' ){		// ドライブなど
		int mode;

		VFSGetDriveType(name1, &mode, NULL);
		if ( mode == VFSPT_DRIVE ){
			LPSHELLFOLDER pTmpSF;

			if ( FAILED(SHGetDesktopFolder(&pSF)) ) return FALSE;
			idl = IShellToPidl(pSF, NilStr);
			if ( idl != NULL ){
				if ( FAILED(pSF->lpVtbl->BindToObject(pSF, idl, NULL, &XIID_IShellFolder, (LPVOID *)&pTmpSF)) ){
					goto error;
				}
				FreePIDL(idl);
				pSF->lpVtbl->Release(pSF);
				pSF = pTmpSF;
				idl = BindIShellAndFname(pSF, name1);
			}
		}else{
			pSF = VFPtoIShell(NULL, name1, &idl);
			if ( pSF == NULL ) return FALSE;
			pSF->lpVtbl->Release(pSF);
			if ( FAILED(SHGetDesktopFolder(&pSF)) ){
				pSF = NULL;
				goto error;
			}
		}
	}else{
		TCHAR backupChar;

		backupChar = *fname;
		*fname = '\0';

		pSF = VFPtoIShell(NULL, name1, NULL);
		if ( pSF == NULL ) goto error;

		*fname = backupChar;
		idl = GetCachedFnameIdl(pSF, name1, fname);
		if ( idl == NULL ) idl = BindIShellAndFname(pSF, fname);
	}
	if ( idl != NULL ){
		*ppSF = pSF;
		*pidl = idl;
		return TRUE;
	}
error:
	if ( idl != NULL ) FreePIDL(idl);
	if ( pSF != NULL ) pSF->lpVtbl->Release(pSF);
	return FALSE;
}

/*-----------------------------------------------------------------------------
	Asciiz ファイル名の ContextMenu 処理を行う
	path + entry	ファイル名
	lppt			popupmenu の表示位置
-----------------------------------------------------------------------------*/
VFSDLL BOOL PPXAPI VFSSHContextMenu(HWND hWnd, LPPOINT pos, const TCHAR *path, const TCHAR *entry, const TCHAR *cmd)
{
	LPITEMIDLIST idl;
	LPSHELLFOLDER pSF;
	BOOL result;
								// dir + file 形式に変換
	if ( VFSMakeIDL(path, &pSF, &idl, entry) == FALSE ){
		return FALSE;
	}
	result = SHContextMenu(hWnd, pos, pSF, &idl, 1, cmd);
	FreePIDL(idl);
	pSF->lpVtbl->Release(pSF);
	return result;
}

VFSDLL void PPXAPI GetDriveNameTitle(TCHAR *buf, TCHAR drive)
{
	WCHAR wbuf[VFPS];

	// Floppy かどうかを判定。Win2k/XP以降用
	wsprintf(buf, T("\\DosDevices\\%c:"), drive);
	wbuf[0] = '\0';
	for ( ; ; ){
		HKEY hKey;
		DWORD t, s;

		if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE,
				MountString, 0, KEY_READ, &hKey) != ERROR_SUCCESS ){
			break;
		}
		s = sizeof wbuf;
		if ( RegQueryValueEx(hKey, buf, NULL, &t, (LPBYTE)wbuf, &s)== ERROR_SUCCESS ){
			*(WORD *)(BYTE *)((BYTE *)wbuf + (s & 0x1fe)) = '\0';
		}
		RegCloseKey(hKey);
		break;
	}
	if ( strstrW(wbuf, L"FLOPPY") != NULL ){
		tstrcpy(buf, T("Floppy"));
	}else{
		LPITEMIDLIST idl;
		LPSHELLFOLDER pSF, pNSF;

		wsprintf(buf, T("%c:\\"), drive);
		if ( SUCCEEDED(SHGetDesktopFolder(&pSF)) ){
			idl = IShellToPidl(pSF, NilStr);
			if ( FAILED(pSF->lpVtbl->BindToObject(pSF, idl, NULL,
						&XIID_IShellFolder, (LPVOID *)&pNSF)) ){
				pNSF = NULL;
			}
			FreePIDL(idl);
			pSF->lpVtbl->Release(pSF);
			if ( pNSF != NULL ){
				idl = BindIShellAndFname(pNSF, buf);
				PIDL2DisplayNameOf(buf, pNSF, idl);
				FreePIDL(idl);
				pNSF->lpVtbl->Release(pNSF);
			}
		}
	}
}

VFSDLL void PPXAPI GetTextFromCF_SHELLIDLIST(TCHAR *text, DWORD textsize, HGLOBAL hGMem, BOOL lf)
{
	LPSHELLFOLDER pDesktop, pParent;
	LPIDA pIdA = (LPIDA)GlobalLock(hGMem);
	LPITEMIDLIST pRootPidl;
	TCHAR path[VFPS], name[VFPS], fullname[VFPS];
	DWORD left;
	UINT *aoffset;

	if ( pIdA == NULL ){
		*text = '\0';
		return;
	}

	pRootPidl = (LPITEMIDLIST)(LPBYTE)((LPBYTE)(pIdA) + pIdA->aoffset[0]);
	if ( IsTrue(SHGetPathFromIDList(pRootPidl, path)) ){
		DWORD plen;

		left = pIdA->cidl;
		plen = tstrlen32(path);
		if ( (left < 1) && (textsize > plen) ){
			tstrcpy(text, path);
			text += plen;
		}else{
			aoffset = pIdA->aoffset + 1;
			if ( SUCCEEDED(SHGetDesktopFolder(&pDesktop)) ){
				if ( FAILED(pDesktop->lpVtbl->BindToObject(pDesktop,
					  pRootPidl, NULL, &XIID_IShellFolder, (LPVOID *)&pParent)) ){
					pParent = pDesktop;
				}
				while( left-- ){
					DWORD len;

					if ( PIDL2DisplayNameOf(name, pParent, (LPITEMIDLIST)(LPBYTE)((LPBYTE)(pIdA) + *aoffset++)) == FALSE ){
						break;
					}
					CatPath(fullname, path, name);
					len = tstrlen32(fullname);
					if ( textsize > (len + 2) ){
						tstrcpy(text, fullname);
						text += len;
					}
					if ( left ){
						if ( IsTrue(lf) ){
							*text++ = '\r';
							*text++ = '\n';
						}else{
							*text++ = ' ';
						}
					}
				}
				if ( pParent != pDesktop ) pParent->lpVtbl->Release(pParent);
			}
			pDesktop->lpVtbl->Release(pDesktop);
		}
	}
	GlobalUnlock(hGMem);
	*text = '\0';
}
