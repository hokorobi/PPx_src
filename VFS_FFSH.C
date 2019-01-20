/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		Shell's Namespace 列挙処理
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "WINOLE.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#define DefxIShellFolder2
#include "VFS_FF.H"
#pragma hdrstop

#if !defined(__BORLANDC__) || (__BORLANDC__ <= 0x509)
#ifndef __IShellFolder2_FWD_DEFINED__
#endif
#endif

const IID XIID_IShellFolder2 = {0x93F2F68C,0x1D1B,0x11d3,{0xA3,0x0E,0x00,0xC0,0x4F,0x79,0xAB,0xD1}};

xSHCOLUMNID Prop_Size = { {0xB725F130,0x47EF,0x101A,{0xA5,0xF1,0x02,0x60,0x8C,0x9E,0xEB,0xAC}},12}; // Storage property set,PID_STG_SIZE
xSHCOLUMNID Prop_WriteTime = { {0xB725F130,0x47EF,0x101A,{0xA5,0xF1,0x02,0x60,0x8C,0x9E,0xEB,0xAC}},14}; // Storage property set,PID_STG_WRITETIME
xSHCOLUMNID Prop_CreateTime = { {0xB725F130,0x47EF,0x101A,{0xA5,0xF1,0x02,0x60,0x8C,0x9E,0xEB,0xAC}},15}; // Storage property set,PID_STG_CREATETIME
xSHCOLUMNID Prop_AccessTime = { {0xB725F130,0x47EF,0x101A,{0xA5,0xF1,0x02,0x60,0x8C,0x9E,0xEB,0xAC}},16}; // Storage property set,PID_STG_ACCESSTIME

struct SpecialFolderListStruct {
	const TCHAR *name;
	int mode;
} SpecialFolderList[] = {
	{T("Desktop"),				CSIDL_DESKTOP},
//	{T("Desktop"),				CSIDL_DESKTOPDIRECTORY}, // Win2000/XP
	{T("DesktopFolder"),		CSIDL_DESKTOP},
	{T("DriveFolder"),			CSIDL_DRIVES},
	{T("ControlPanelFolder"),	CSIDL_CONTROLS},
	{T("RecycleBinFolder"),		CSIDL_BITBUCKET},
	{T("Personal"),				CSIDL_PERSONAL},
	{T("Start Menu"),			CSIDL_STARTMENU},
	{T("AppData"),				CSIDL_APPDATA},
	{T("Local AppData"),		0x1c},
	{NULL,0}
};

HANDLE hOleaut32SH = NULL;

#if 0
BOOL IDL2PhaseNameOf(TCHAR *name,LPSHELLFOLDER sfolder,LPCITEMIDLIST pidl)
{
	STRRET strr;

	if ( FAILED(sfolder->lpVtbl->GetDisplayNameOf(sfolder,
			pidl,SHGDN_INFOLDER | SHGDN_FORPARSING,&strr)) ) // ::{～} 形式
	{
		return FALSE;
	}
	switch (strr.uType){
		case STRRET_WSTR:
			#ifdef UNICODE
				tstrcpy(name,UNIONNAME(strr,pOleStr));
			#else
				UnicodeToAnsi(UNIONNAME(strr,pOleStr),name,VFPS);
			#endif
				CoTaskMemFree(UNIONNAME(strr,pOleStr));
			break;

		case STRRET_OFFSET:
			#ifdef UNICODE
				AnsiToUnicode((char *)pidl + UNIONNAME(strr,uOffset),name,VFPS);
			#else
				tstrcpy(name,(char *)pidl + UNIONNAME(strr,uOffset));
			#endif
			break;

		case STRRET_CSTR:
			#ifdef UNICODE
				AnsiToUnicode(UNIONNAME(strr,cStr),name,VFPS);
			#else
				tstrcpy(name,UNIONNAME(strr,cStr));
			#endif
			break;

		default:
			XMessage(NULL,NULL,XM_DbgLOG,T("???"));
	}
	return TRUE;
}

void LogShellAttr(const TCHAR *memo,DWORD flags)
{
	XMessage(NULL,NULL,XM_DbgLOG,T("%s: GetAttributesOf %x"),memo,flags);
// x
	if ( flags & SFGAO_CANCOPY ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_CANCOPY"));
	if ( flags & SFGAO_CANMOVE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_CANMOVE"));
	if ( flags & SFGAO_CANLINK ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_CANLINK"));
	if ( flags & SFGAO_STORAGE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_STORAGE"));
// x0
	if ( flags & SFGAO_CANRENAME ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_CANRENAME"));
	if ( flags & SFGAO_CANDELETE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_CANDELETE"));
	if ( flags & SFGAO_HASPROPSHEET ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_HASPROPSHEET"));
	if ( flags & 0x80 ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_0x80"));
// x00
	if ( flags & SFGAO_DROPTARGET ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_DROPTARGET"));
	if ( flags & 0x200 ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_0x200"));
	if ( flags & 0x400 ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_0x400"));
	if ( flags & 0x800 ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_0x800"));
// x000
	if ( flags & SFGAO_SYSTEM ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_SYSTEM"));
	if ( flags & SFGAO_ENCRYPTED ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_ENCRYPTED"));
	if ( flags & SFGAO_ISSLOW ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_ISSLOW"));
	if ( flags & SFGAO_GHOSTED ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_GHOSTED"));
// x 0000
	if ( flags & SFGAO_LINK ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_LINK"));
	if ( flags & SFGAO_SHARE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_SHARE"));
	if ( flags & SFGAO_READONLY ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_READONLY"));
	if ( flags & SFGAO_HIDDEN ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_HIDDEN"));
// x0 0000
	if ( flags & SFGAO_NONENUMERATED ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_NONENUMERATED"));
	if ( flags & SFGAO_NEWCONTENT ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_NEWCONTENT"));
	if ( flags & SFGAO_STREAM ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_STREAM"));
	if ( flags & SFGAO_STORAGEANCESTOR ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_STORAGEANCESTOR"));
// x00 0000
	if ( flags & SFGAO_VALIDATE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_VALIDATE"));
	if ( flags & SFGAO_REMOVABLE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_REMOVABLE"));
	if ( flags & SFGAO_COMPRESSED ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_COMPRESSED"));
	if ( flags & SFGAO_BROWSABLE ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_BROWSABLE"));
// x000 0000
	if ( flags & SFGAO_FILESYSANCESTOR ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_FILESYSANCESTOR"));
	if ( flags & SFGAO_FOLDER ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_FOLDER"));
	if ( flags & SFGAO_FILESYSTEM ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_FILESYSTEM"));
	if ( flags & SFGAO_HASSUBFOLDER ) XMessage(NULL,NULL,XM_DbgLOG,T("SFGAO_HASSUBFOLDER"));
}
#endif

// Win2000 等、名前が用意されていない環境でも使えるようにする
BOOL PPxGetSpecialFolderLocation(HWND hWnd,const TCHAR *name,LPITEMIDLIST *idl)
{
	struct SpecialFolderListStruct *sfls = SpecialFolderList;

	if ( memcmp(name,StrShellScheme,SIZEOFTSTR(StrShellScheme)) ) return FALSE; // ::{ 形式
	name += TSIZEOFSTR(StrShellScheme);
	while ( sfls->name != NULL ){
		if ( !tstricmp(sfls->name,name) ){
			if ( sfls->mode == CSIDL_DESKTOP ){ // CSIDL_DESKTOP は idlがない
				*idl = NULL;
				return TRUE;
			}
			if ( SUCCEEDED(SHGetSpecialFolderLocation(hWnd,sfls->mode,idl)) ){
				return TRUE;
			}
			break;
		}
		sfls++;
	}
	return FALSE;
}

// nMA: ShellMallocが必要なときに、ポインタを設定する
// nIDL: SHELLFOLDERを作成したときに使った idl が必要なら、ポインタを設定する
ERRORCODE VFPtoIShellSub(HWND hWnd,const TCHAR *vpath,int mode,LPSHELLFOLDER *nSF,LPITEMIDLIST *nIDL,DWORD *dirflags,LPMALLOC *nMA)
{
	LPSHELLFOLDER pCurSF,pNewSF;
	LPMALLOC pMA;
	LPITEMIDLIST pIDL = NULL; // nIDL に渡すための idl
	LPITEMIDLIST idl;
	DWORD errorcode = ERROR_BAD_PATHNAME;
	TCHAR tmppath[VFPS],*pathptr;
	DWORD enumflags;

	BOOL ParseFirst = TRUE;

	if ( FAILED(SHGetMalloc(&pMA)) ) return GetLastError();
	if ( FAILED(SHGetDesktopFolder(&pCurSF)) ) goto error_nosf;

	tstrcpy(tmppath,vpath);
	pathptr = tmppath;

	// ドライブ名相当の idl を取得し、IShellFolder を用意する
	switch ( mode ){
		case VFSPT_DRIVE:
			idl = IShellToPidl(pCurSF,pathptr);
			if ( idl != NULL ){
				pathptr = NilStrNC;
				break;
			}
			goto error;

		case VFSPT_UNC:
			if ( *(pathptr + 2) != '\0' ){ // PC名有り
				if ( (*(pathptr + 2) == '+') && (*(pathptr + 3) == '\0') ){
					goto error;
				}
				idl = IShellToPidl(pCurSF,pathptr);
				if ( idl != NULL ){
					pathptr = NilStrNC;
					break;
				}
			}else{
				idl = NULL;
			}
			// PC名無し…PC名列挙
			pathptr += 2;	// 「\\」 をスキップ
			if ( FAILED(SHGetSpecialFolderLocation(hWnd,CSIDL_NETWORK,&idl)) ){
				goto error;
			}
			break;

		case VFSPT_SHELLSCHEME:
			pathptr = FindPathSeparator(pathptr);
			if ( pathptr == NULL ){
				pathptr = NilStrNC;
			}else{
				*pathptr++ = '\0';
			}
			// 既知の名前？
			if ( IsTrue(PPxGetSpecialFolderLocation(hWnd,tmppath,&idl)) ){
				break;
			}
			// 既知の名前でないので、解釈させる
			idl = IShellToPidl(pCurSF,tmppath);
			if ( idl != NULL ) break;
			goto error;

		case (-1 - CSIDL_DESKTOP):
			idl = NULL;
			break;

		default:
			if ( FAILED(SHGetSpecialFolderLocation(hWnd,-1 - mode,&idl)) ){
				goto error;
			}
			break;
	}

	if ( *pathptr == '\\' ) pathptr++;	// root を skip

	if ( idl != NULL ){
		if ( FAILED(pCurSF->lpVtbl->BindToObject(pCurSF,idl,NULL,
				&XIID_IShellFolder,(LPVOID *)&pNewSF)) ){
			goto error;
		}
		if ( (dirflags != NULL) && (*pathptr == '\0') ){
			*dirflags = SFGAO_STORAGE | SFGAO_STORAGEANCESTOR | SFGAO_FILESYSANCESTOR | SFGAO_FOLDER | SFGAO_FILESYSTEM;
			if ( FAILED(pCurSF->lpVtbl->GetAttributesOf(
					pCurSF,1,(LPCITEMIDLIST *)&idl,dirflags)) ){
				*dirflags = 0;
			}
		}

		pCurSF->lpVtbl->Release(pCurSF);
		pCurSF = pNewSF;
		if ( nIDL == NULL ){
			pMA->lpVtbl->Free(pMA,idl);
		}else{
			pIDL = idl;
		}
	}else if ( dirflags != NULL ) *dirflags = 0xaa55;


	enumflags = MAKEFLAG_EnumObjectsForFolder;
										// 相対指定処理
	while( *pathptr != '\0' ){
		TCHAR *nextp;
		int cachedsize;
		TCHAR name[VFPS];

		nextp = FindPathSeparator(pathptr);
		if ( nextp != NULL ) *nextp = '\0';
												// idl キャッシュがあれば使用
		cachedsize = GetCustTableSize(IdlCacheName,tmppath);
		if ( cachedsize <= 0 ){
			idl = NULL;
		}else{
			HRESULT hr;

			idl = pMA->lpVtbl->Alloc(pMA,cachedsize);
			GetCustTable(IdlCacheName,tmppath,idl,cachedsize);
			hr = E_FAIL;

			if ( IsTrue(PIDL2DisplayNameOf(name,pCurSF,idl)) ){ // ここではキャッシュが古くてもエラーにならない
				if ( tstrcmp(name,pathptr) == 0 ){
					// キャッシュが古いとここでエラーになる
					hr = pCurSF->lpVtbl->BindToObject(pCurSF,
						idl,NULL,&XIID_IShellFolder,(LPVOID *)&pNewSF);
				}
			}
			if ( FAILED(hr) ){
				DeleteCustTable(IdlCacheName,tmppath,0);
#ifndef RELEASE
				XMessage(NULL,NULL,XM_DbgLOG,T("delete IDL cache %s"),tmppath);
#endif
				pMA->lpVtbl->Free(pMA,idl);
				idl = NULL;
			}
		}
												// ParseDisplayName で取得
		if ( idl == NULL ){
			DWORD tick,nowtick;

			tick = GetTickCount();
			if ( IsTrue(ParseFirst) ) idl = IShellToPidl(pCurSF,pathptr);

			if ( idl == NULL ){			// ParseDisplayName はだめ→自前捜索
				LPENUMIDLIST pEID;

				// ●　ここで MTP デバイスが応答無しになることがある
				if ( pCurSF->lpVtbl->EnumObjects(pCurSF,hWnd,enumflags,&pEID) != S_OK ){ // S_FALSE のときは、pEID = NULL
					goto error;
				}

				for ( ; ; ){
					DWORD count;

					if ( (pEID->lpVtbl->Next(pEID,1,&idl,&count) != S_OK) || (count == 0) ){
						if ( ParseFirst == FALSE ){
							idl = IShellToPidl(pCurSF,pathptr);
							if ( idl != NULL ){
								ParseFirst = TRUE;
								break;
							}
						}
						pEID->lpVtbl->Release(pEID);
						goto error;
					}
					if ( IsTrue(PIDL2DisplayNameOf(name,pCurSF,idl)) ){
						if ( !tstricmp(name,pathptr) ){
						// 次の ParseDisplayName で応答が著しく遅くなることがあるので、ParseDisplayName を後回しにして自前捜索を優先に
							ParseFirst = FALSE;
							break;
						}
					}
					pMA->lpVtbl->Free(pMA,idl);
				}
				pEID->lpVtbl->Release(pEID);
			}

			nowtick = GetTickCount();
			if ( tick != nowtick ){ // 時間が掛かった idl なのでキャッシュ
				DWORD flags;

				flags = SFGAO_FOLDER | SFGAO_FILESYSANCESTOR;
				if ( SUCCEEDED(pCurSF->lpVtbl->GetAttributesOf(
						pCurSF,1,(LPCITEMIDLIST *)&idl,&flags)) ){
					if ( flags & (SFGAO_FOLDER | SFGAO_FILESYSANCESTOR) ){
						int count = CountCustTable(IdlCacheName);

						if ( count > 30 ) DeleteCustTable(IdlCacheName,NULL,0);
						SetCustTable(IdlCacheName,tmppath,idl,GetPidlSize(idl));
					}
				}
			}

			// 取得した PIDL を bind
			if ( FAILED(pCurSF->lpVtbl->BindToObject(pCurSF,
					idl,NULL,&XIID_IShellFolder,(LPVOID *)&pNewSF)) ){
				pMA->lpVtbl->Free(pMA,idl);
				goto error;
			}
		}

		if ( (dirflags != NULL) && (nextp == NULL) ){
			*dirflags = SFGAO_STORAGE | SFGAO_STORAGEANCESTOR | SFGAO_FILESYSANCESTOR | SFGAO_FOLDER | SFGAO_FILESYSTEM;
			if ( FAILED(pCurSF->lpVtbl->GetAttributesOf(
					pCurSF,1,(LPCITEMIDLIST *)&idl,dirflags)) ){
				*dirflags = 0;
			}
		}

		if ( nIDL != NULL ){
			LPITEMIDLIST tmpidl;

			tmpidl = CatPidl(pMA,pIDL,idl);
			pMA->lpVtbl->Free(pMA,pIDL);
			pIDL = tmpidl;
		}
		pMA->lpVtbl->Free(pMA,idl);
		pCurSF->lpVtbl->Release(pCurSF);
		pCurSF = pNewSF;
		if ( nextp == NULL ) break;
		*nextp = '\\';
		pathptr = nextp + 1;
		continue;
	}

	if ( nIDL != NULL ){
		if ( pIDL == NULL ){
			if ( FAILED(SHGetSpecialFolderLocation(hWnd,CSIDL_DESKTOP,&pIDL)) ){
				goto error;
			}
		}
		*nIDL = pIDL;
	}else{
		pMA->lpVtbl->Free(pMA,pIDL);
	}
	if ( nSF != NULL ){
		*nSF = pCurSF;
	}else{
		pCurSF->lpVtbl->Release(pCurSF);
	}
	if ( nMA != NULL ){
		*nMA = pMA;
	}else{
		pMA->lpVtbl->Release(pMA);
	}
	return NO_ERROR;

error:
	if ( pIDL != NULL ) pMA->lpVtbl->Free(pMA,pIDL);
	pCurSF->lpVtbl->Release(pCurSF);
error_nosf:
	pMA->lpVtbl->Release(pMA);
	return errorcode;
}

ERRORCODE VFSFF_SHN(const TCHAR *vpath,FF_SHN *vshn,WIN32_FIND_DATA *findfile,int mode)
{
	LPSHELLFOLDER pSF;
	ERRORCODE errorcode;
	HRESULT hr;
	HWND hWnd;
	DWORD enumflags;

	ThInit(&vshn->dirs);
	vshn->d_off = 0;
	if ( (mode == VFSPT_UNC) && (*(vpath + 2) == '\0') ){ // UNC root
		ThAddString(&vshn->dirs,T("+"));
		if ( OSver.dwMajorVersion == 5 ){ // Win2k/XPはサーバ一覧を取得する
			EnumNetServer(&vshn->dirs,FALSE);
			ThAddString(&vshn->dirs,NilStr);
		}
	}
	hWnd = GetFocus();
	errorcode = VFPtoIShellSub(hWnd,vpath,mode,&pSF,NULL,&vshn->dirflags,&vshn->pMA);
	if ( errorcode != NO_ERROR ) goto error;

	enumflags = MAKEFLAG_EnumObjectsForFolder;

	// \\ネットワーク全体\Microsoft Windows Network は↓で時間が掛かる
	hr = pSF->lpVtbl->EnumObjects(pSF,hWnd,enumflags,&vshn->pEID);
	if ( hr != S_OK ){ // S_FALSE のときは、pEID = NULL
		if ( hr == E_INVALIDARG ){ // IDL キャッシュの異常の可能性→キャッシュを強制削除して再試行
			if ( DeleteCustData(IdlCacheName) == 0 ){
				pSF->lpVtbl->Release(pSF);
				vshn->pMA->lpVtbl->Release(vshn->pMA);
				errorcode = VFPtoIShellSub(hWnd,vpath,mode,&pSF,NULL,&vshn->dirflags,&vshn->pMA);
				if ( errorcode != NO_ERROR ) goto error;

				hr = pSF->lpVtbl->EnumObjects(pSF,hWnd,enumflags,&vshn->pEID);
			}
		}
		if ( hr != S_OK ) goto error_shn;
	}

	vshn->pSF2 = NULL;
	if ( vshn->pEID != NULL ){
		SetDummydir(findfile,T("."));
		vshn->cnt = FFPIDL_UPDIR;
		vshn->pSF = pSF;
		vshn->fix = (mode == -11) ? 1 : -1; // ごみ箱なら強制補正
		return NO_ERROR;
	}

error_shn:
	errorcode = ERROR_BAD_PATHNAME;
	pSF->lpVtbl->Release(pSF);
	vshn->pMA->lpVtbl->Release(vshn->pMA);
error:
	ThFree(&vshn->dirs);
	return errorcode;
}

void CheckFix(FF_SHN *vshn,WIN32_FIND_DATA *findfile,LPITEMIDLIST pidl)
{
	TCHAR dispname[MAX_PATH];
	int len1,len2;

	if ( FALSE == PIDL2DisplayNameOf(dispname,vshn->pSF,pidl) ) return;
	len1 = FindExtSeparator(findfile->cFileName);
	len2 = tstrlen32(dispname);
	if ( len1 < len2 ) len2 = FindExtSeparator(dispname);
	if ( (len1 != len2) || tstrnicmp(findfile->cFileName,dispname,len1) ){
		tstrcpy(findfile->cFileName,dispname);
		findfile->cAlternateFileName[0] = '\0';
		vshn->fix = 1;
	}else{
		vshn->fix = 0;
	}
}

void VTtoFileTime(VARIANT *var,FILETIME *ftime)
{
	if ( V_VT(var) == VT_DATE ){
		#ifdef _WIN64
			DWORD_PTR datetime = (DWORD_PTR)(double)((V_DATE(var) + 109205.0) * 864000000000.0);
			ftime->dwLowDateTime = (DWORD)datetime;
			ftime->dwHighDateTime = (DWORD)(datetime >> 32);
		#else
		// FILETIME 変換(32bit)
			V_DATE(var) = (V_DATE(var) + 109205.0) * 864000000000.0;
			if ( SUCCEEDED(DVariantChangeType(var,var,0,VT_UI8)) ){
				ftime->dwLowDateTime = V_I4(var);
				ftime->dwHighDateTime = *((DWORD *)&V_I4(var) + 1);
			}
		#endif
	}else{
		ftime->dwLowDateTime = V_VT(var);
	}
	DVariantClear(var);
}


BOOL VFSFN_SHN(FF_SHN *vshn,WIN32_FIND_DATA *findfile)
{
	findfile->dwReserved1 = 0;
	switch ( vshn->cnt ){
		case FFPIDL_UPDIR:
			vshn->cnt = FFPIDL_ENUM;
			SetDummydir(findfile,T(".."));
			return TRUE;

		case FFPIDL_ENUM: {
			LPITEMIDLIST pidl;
			DWORD count;
			ULONG flag;

			if ( (vshn->pEID->lpVtbl->Next(vshn->pEID,1,&pidl,&count) != S_OK) || (count == 0) ){
				vshn->cnt = FFPIDL_NETLIST;
				// このまま case FFPIDL_NETLIST: へ
			}else{
				if (
#ifdef UNICODE
					FAILED(SHGetDataFromIDList(vshn->pSF,pidl,
						SHGDFIL_FINDDATA,findfile,sizeof(WIN32_FIND_DATA))) ||
#else
					FAILED(SHGetDataFromIDList(vshn->pSF,pidl,
						SHGDFIL_FINDDATA,findfile,sizeof(WIN32_FIND_DATA) * 2)) ||
#endif
					(findfile->nFileSizeLow == 0xffffffff) ){
				// WIN32_FIND_DATA が得られないときの処理
					DWORD attr = FILE_ATTRIBUTEX_VIRTUAL;
					flag = SFGAO_FOLDER | SFGAO_COMPRESSED | SFGAO_GHOSTED;

					SetDummyFindData(findfile);
					if ( FALSE == PIDL2DisplayNameOf(findfile->cFileName,vshn->pSF,pidl) ){
						tstrcpy(findfile->cFileName,T("error"));
					}

					if ( vshn->pSF2 == NULL ){
						if ( hOleaut32SH == NULL ){
							hOleaut32SH = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUT32_Variant,LOADWINAPI_LOAD);
						}
						if ( DVariantInit != NULL ){
							vshn->pSF->lpVtbl->QueryInterface(vshn->pSF,
								&XIID_IShellFolder2,(void **)&vshn->pSF2);
						}
					}
					if ( vshn->pSF2 != NULL ){
						VARIANT var;

						DVariantInit(&var);
						// ファイルサイズを取得
						if ( SUCCEEDED(vshn->pSF2->lpVtbl->
							  GetDetailsEx(vshn->pSF2,pidl,&Prop_Size,&var)) ){
							if ( V_VT(&var) == VT_UI8 ){
								findfile->nFileSizeLow = V_I4(&var);
								findfile->nFileSizeHigh = *((DWORD *)&V_I4(&var) + 1);
							}else if ( V_VT(&var) == VT_UI4 ){
								findfile->nFileSizeLow = V_I4(&var);
							}else{
								findfile->nFileSizeLow = V_VT(&var);
							}
							DVariantClear(&var);
						}
						DVariantInit(&var);
						// 書き込み日時を取得
						if ( SUCCEEDED(vshn->pSF2->lpVtbl->GetDetailsEx(
								vshn->pSF2,pidl,&Prop_WriteTime,&var))){
							VTtoFileTime(&var,&findfile->ftLastWriteTime);
						}
						// 作成日時を取得
						if ( SUCCEEDED(vshn->pSF2->lpVtbl->GetDetailsEx(
								vshn->pSF2,pidl,&Prop_CreateTime,&var))){
							VTtoFileTime(&var,&findfile->ftCreationTime);
						}
						// 参照日時を取得
						if ( SUCCEEDED(vshn->pSF2->lpVtbl->GetDetailsEx(
								vshn->pSF2,pidl,&Prop_AccessTime,&var))){
							VTtoFileTime(&var,&findfile->ftLastAccessTime);
						}

					}
					if ( SUCCEEDED(vshn->pSF->lpVtbl->GetAttributesOf(
								vshn->pSF,1,(LPCITEMIDLIST *)&pidl,&flag))){
						if ( flag & (SFGAO_FOLDER | SFGAO_FILESYSANCESTOR) ){
							setflag(attr,FILE_ATTRIBUTE_DIRECTORY);
						}
/*	flag に SFGAO_READONLY をつけると、フロッピーディスクを読み書きするので
	調べないことにした。
						if ( flag & SFGAO_READONLY ){
							setflag(attr,FILE_ATTRIBUTE_READONLY);
						}
*/
						if ( flag & SFGAO_COMPRESSED ){
							setflag(attr,FILE_ATTRIBUTE_COMPRESSED);
						}
						if ( flag & SFGAO_GHOSTED ){
							setflag(attr,FILE_ATTRIBUTE_HIDDEN);
						}
					}
					findfile->dwFileAttributes = attr;
				}else{ // WIN32_FIND_DATA が得られた後の処理
					flag = SFGAO_FOLDER;

					if ( SUCCEEDED(vshn->pSF->lpVtbl->GetAttributesOf(
								vshn->pSF,1,(LPCITEMIDLIST *)&pidl,&flag)) ){
						if ( flag & SFGAO_FOLDER ){
							setflag(findfile->dwFileAttributes,FILE_ATTRIBUTEX_FOLDER);
						}
					}
							// XPのごみ箱は cFileName が空欄になる場合がある
					if ( (findfile->cFileName[0] == '\0') || vshn->fix ){
						if ( vshn->fix == -1 ){
							CheckFix(vshn,findfile,pidl);
						}else{
							findfile->cAlternateFileName[0] = '\0';
							if ( FALSE == PIDL2DisplayNameOf(
									findfile->cFileName,vshn->pSF,pidl) ){
								findfile->cFileName[0] = '\0';
							}
						}
					}
					// WinXP で cAlternateFileName が正しく設定されないのを対策
					if ( WinType == WINTYPE_XP ){
#if 1 // 高速版。頭２文字分だけ英数字でないかチェック。通常はこれで検出可能
#ifdef UNICODE
						DWORD data;

						data = *(DWORD *)findfile->cAlternateFileName;
						// エンディアン注意
						if ( (data & 0xff80ff80) && (WORD)data )
#else
						WORD data;

						data = *(WORD *)findfile->cAlternateFileName;
						// エンディアン注意
						if ( (data & 0x8080) && (BYTE)data )
#endif
						{
							findfile->cAlternateFileName[0] = '\0';
						}
#else // 完全版。高速版で漏れが見つかったらこちらにする
						UTCHAR *p = (UTCHAR *)findfile->cAlternateFileName;

						for ( ; *p ; p++ ){
							if ( *p >= 0x80 ){
								findfile->cAlternateFileName[0] = '\0';
								break;
							}
						}
#endif
						// cAlternateFileName が1byteずれて格納される
						// ことがあるので念のために対策
						findfile->cAlternateFileName[13] = '\0';
					}
				}
				{ // IDL の sum を用意する
					DWORD sum = 0;
					USHORT size = (USHORT)(pidl->mkid.cb - sizeof(USHORT));
					BYTE *idlptr = (BYTE *)pidl->mkid.abID;

					for(;;){
						sum += *idlptr++;
						if ( --size == 0 ) break;
					}
					// idlist が複数あるときは対象外にする
					if ( *((USHORT*)idlptr) != 0 ) sum = 0;
					findfile->dwReserved1 = sum;
				}
#if 0
				{
					TCHAR name[VFPS];
					IDL2PhaseNameOf(name,vshn->pSF,pidl);
					XMessage(NULL,NULL,XM_DbgLOG,T("%s: (%x)%s"),findfile->cFileName,GetPidlSize(pidl),name);
				}
				flag = MAX32;
				if ( SUCCEEDED(vshn->pSF->lpVtbl->GetAttributesOf(
								vshn->pSF,1,(LPCITEMIDLIST *)&pidl,&flag))){
				//	LogShellAttr(findfile->cFileName,flag);
				}
#endif
				vshn->pMA->lpVtbl->Free(vshn->pMA,pidl);
				return TRUE;
			}
		}
		case FFPIDL_NETLIST:
			if ( vshn->dirs.bottom != NULL ){
				TCHAR *p;

				p = (TCHAR *)(vshn->dirs.bottom + vshn->d_off);
				if ( *p ){
					SetDummydir(findfile,p);
					vshn->d_off += TSTRSIZE32(p);
					return TRUE;
				}
			}
			vshn->cnt = FFPIDL_NOMORE;
			break;

//		case FFPIDL_NOMORE: // 初期処理で代用
//		default:
	}
	SetLastError(ERROR_NO_MORE_FILES);
	return FALSE;
}
