/*-----------------------------------------------------------------------------
	Paper Plane xUI					Shell's Namespace C++ / .lnk ファイル作成
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include <ole2.h>
#include <wtypes.h>
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#include "VFS_FOP.H"
#include "VFS_FF.H"
#pragma hdrstop

void CloseZipFolder(ZIPFOLDERSTRUCT *zfs);
TCHAR StrParentEntry[] = T("..");

VFSDLL HRESULT PPXAPI MakeShortCut(const TCHAR *LinkedFile, const TCHAR *LinkFname, const TCHAR *DestPath)
{
	HRESULT hres;
	IShellLink *psl;
	HRESULT ComInitResult;


	ComInitResult = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
										// IShellLink interface を取得
	hres = ::CoCreateInstance(XCLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			XIID_IShellLink, (LPVOID *)&psl);
	if ( SUCCEEDED(hres) ){
		IPersistFile *ppf;
									// IShellLink から IPersistFile interface
		hres = psl->QueryInterface(XIID_IPersistFile, (LPVOID *)&ppf);
		if ( SUCCEEDED(hres) ){
			hres = psl->SetPath(LinkedFile);	// link 対象のファイルを設定
			if ( SUCCEEDED(hres) ){
				hres = psl->SetDescription(DestPath);	// 保存/コメント先を設定
				if ( SUCCEEDED(hres) ){
					DWORD attr;

					// ファイルの時は作業/カレントディレクトリを設定する
					attr = ::GetFileAttributesL(LinkedFile);
					if ( (attr != BADATTR) && !(attr & FILE_ATTRIBUTE_DIRECTORY)){
						TCHAR path[VFPS];

						VFSFullPath(path, StrParentEntry, LinkedFile);
						psl->SetWorkingDirectory(path);
					}
					// ショートカットを指定の名前で保存する
#ifdef UNICODE
					hres = ppf->Save(LinkFname, TRUE);
#else
					WCHAR wsz[VFPS];

					AnsiToUnicode(LinkFname, wsz, VFPS);
					hres = ppf->Save(wsz, TRUE);
#endif
				}
			}
			ppf->Release();
		}
		psl->Release();
	}
	if ( SUCCEEDED(ComInitResult) ) ::CoUninitialize();
	return hres;
}

//-----------------------------------------------------------------------------
// zipfldr.dll による zip 処理
//-----------------------------------------------------------------------------
typedef HRESULT (STDAPICALLTYPE *DDllGetClassObject_)(const CLSID &rclsid, const IID &riid, void **ppv);

struct FOLDERDLLINFO
{
	const TCHAR *dllname;
	IID id;
} FolderDlls[] = {
 { zipfldrName,
	{0xE88DCCE0, 0xB7B3, 0x11d1, {0xA9, 0xF0, 0x00, 0xAA, 0x00, 0x60, 0xFA, 0x31}} },
 { lzhfldrName,
	{0x4F289A46, 0x2BBB, 0x4AE8, {0x9E, 0xDA, 0xE5, 0xE0, 0x34, 0x70, 0x7A, 0x71}} },
 { cabfldrName,
	{0x0CD7A5C0, 0x9F37, 0x11CE, {0xAE, 0x65, 0x08, 0x00, 0x2B, 0x2E, 0x12, 0x62}} },

};

BOOL OpenZipFolder(ZIPFOLDERSTRUCT *zfs, const TCHAR *fname, TCHAR *subdir, int type, BOOL usestrage)
{
#if ZIPFOLDER_USESTRAGEENUM
	#ifdef UNICODE
	#define NameW(nam) nam
	#define NameFixW(nam)
	#else
	WCHAR namew[VFPS];
	#define NameW(nam) namew
	#define NameFixW(nam) AnsiToUnicode(nam, namew, VFPS);
	#endif

	LPPERSISTFILE pPF = NULL;
	usestrage = 1;
#else
	LPITEMIDLIST idlist;
#endif
	IClassFactory *pCF = NULL;
										// DLL 読み込み
	zfs->hDLL = ::LoadLibrary(FolderDlls[type].dllname);
	if ( zfs->hDLL == NULL ) return FALSE;

	zfs->ComInitResult = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	zfs->pSF = NULL;
	zfs->pST = NULL;
										// IF を取得する
#if 1 // 自前 DLL 読み込み版
	DDllGetClassObject_ DDllGetClassObject = (DDllGetClassObject_)::GetProcAddress(zfs->hDLL, "DllGetClassObject");
	if ( DDllGetClassObject == NULL ) goto error;
	if ( FAILED(DDllGetClassObject(FolderDlls[type].id, XIID_IClassFactory, (void **)&pCF)) ){
		goto error;
	}
	if ( usestrage ){
		if ( FAILED(pCF->CreateInstance(NULL, XIID_IStorage, (void **)&zfs->pST)) ){
//			goto error;
		}
	}
  #if !ZIPFOLDER_USESTRAGEENUM
	if ( FAILED(pCF->CreateInstance(NULL, XIID_IShellFolder, (void **)&zfs->pSF)) ){
		goto error;
	}
  #endif
#else // COM より取得版
	if ( FAILED(::CoCreateInstance(FolderDlls[type].id, NULL,
			CLSCTX_INPROC_SERVER, XIID_IStorage, (LPVOID *)&zfs->pST)) ){
		goto error;
	}
  #if !ZIPFOLDER_USESTRAGEENUM
	if ( FAILED(::CoCreateInstance(FolderDlls[type].id, NULL,
			CLSCTX_INPROC_SERVER, XIID_IShellFolder, (LPVOID *)&zfs->pSF)) ){
		goto error;
	}
  #endif
#endif
					// ファイル読み込み・サブディレクトリ設定
#if ZIPFOLDER_USESTRAGEENUM
										// zip ファイルを読み込む
	if ( FAILED(zfs->pST->QueryInterface(XIID_IPersistFile, (void **)&pPF)) ){
		goto error;
	}
	NameFixW(fname);
	if ( FAILED(pPF->Load(NameW(fname), STGM_READ | STGM_SHARE_EXCLUSIVE)) ){
		goto error;
	}

	// Subdir 移動
	while ( *subdir ){
		LPSTORAGE pST;
		TCHAR *p, *next;

		p = FindPathSeparator(subdir);
		if ( p != NULL ){
			*p = '\0';
			next = p + 1;
		}else{
			next = NilStrNC;
		}
		NameFixW(subdir);
		if ( FAILED(zfs->pST->OpenStorage(NameW(subdir), NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &pST)) ){
			goto error;
		}
		zfs->pST->Release();
		zfs->pST = pST;	// 入れ替え
		subdir = next;
	}
	pPF->Release();
#else
	LPMALLOC pMA;

	if ( FAILED(SHGetMalloc(&pMA)) ) goto error;
	idlist = PathToPidl(fname);

	if ( idlist != NULL ){
		IPersistFolder *pPersistFolder;
		// IShellFolder を初期化
		if ( FAILED(zfs->pSF->QueryInterface(
				IID_IPersistFolder, (void **)&pPersistFolder)) ){
			subdir = NULL;
		}else{
			if ( FAILED(pPersistFolder->Initialize(idlist)) ){
				subdir = NULL;
			}else{
				pPersistFolder->Release();
						// Subdir 移動
				while ( *subdir ){
					IShellFolder *pShellFolder;
					TCHAR *p, *next;
					LPITEMIDLIST idl;
					HRESULT hr;

					p = FindPathSeparator(subdir);
					if ( p != NULL ){
						*p = '\0';
						next = p + 1;
					}else{
						next = NilStrNC;
					}
					idl = IShellToPidl(zfs->pSF, subdir);
					hr = zfs->pSF->BindToObject(idl, NULL, XIID_IShellFolder, (LPVOID *)&pShellFolder);
					pMA->Free(idl);
					if ( FAILED(hr) ){
						subdir = NULL;
						break;
					}
					zfs->pSF->Release();
					zfs->pSF = pShellFolder;
					subdir = next;
				}
			}
		}
		::FreePIDL(idlist);
	}
	pMA->Release();
	if ( subdir == NULL ) goto error;
#endif
	pCF->Release();
	return TRUE;
error:
#if ZIPFOLDER_USESTRAGEENUM
	if ( pPF != NULL ) pPF->Release();
#endif
	if ( pCF != NULL ) pCF->Release();
	CloseZipFolder(zfs);
	return FALSE;
}

BOOL ZipFolderFF(FF_ZIPFOLDER *zf, const TCHAR *fname, TCHAR *subdir, WIN32_FIND_DATA *findfile, int type)
{
#if ZIPFOLDER_USESTRAGEENUM
	zf->pESS = NULL;
#endif
	if ( OpenZipFolder(&zf->zfs, fname, subdir, type, FALSE) == FALSE ){
		return FALSE;
	}
#if !ZIPFOLDER_USESTRAGEENUM
	if ( FAILED(SHGetMalloc(&zf->shn.pMA)) ) return FALSE;
	if ( S_OK != zf->zfs.pSF->EnumObjects(GetFocus(),
			MAKEFLAG_EnumObjectsForFolder, &zf->shn.pEID) ){ // S_FALSE のときは、pEID = NULL
		zf->shn.pMA->Release();
		CloseZipFolder(&zf->zfs);
		return FALSE;
	}
	zf->shn.cnt = FFPIDL_UPDIR;
	ThInit(&zf->shn.dirs);
	zf->shn.d_off = 0;
	zf->shn.fix = 0;
	zf->shn.pSF = zf->zfs.pSF;
	zf->shn.pSF2 = NULL;
#endif
	SetDummydir(findfile, T("."));
	return TRUE;
}

BOOL ZipFolderFN(FF_ZIPFOLDER *zf, WIN32_FIND_DATA *findfile)
{
#if ZIPFOLDER_USESTRAGEENUM
	STATSTG ss;

	if ( zf->pESS == NULL ){		// 列挙開始
		if ( FAILED(zf->zfs.pST->EnumElements(0, NULL, 0, &zf->pESS)) ) return FALSE;
	}
	if ( zf->pESS->Next(1, &ss, NULL) != S_OK ) return FALSE;

	findfile->dwFileAttributes = 0;	// ファイルのみ
	if ( ss.type == STGTY_STORAGE ) findfile->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

	findfile->ftCreationTime = ss.ctime;
	findfile->ftLastAccessTime = ss.atime;
	findfile->ftLastWriteTime = ss.mtime;
	findfile->nFileSizeHigh = ss.cbSize.u.HighPart;
	findfile->nFileSizeLow = ss.cbSize.u.LowPart;
	findfile->cAlternateFileName[0] = '\0';
	#ifdef UNICODE
		tstrcpy(findfile->cFileName, ss.pwcsName);
	#else
		UnicodeToAnsi(ss.pwcsName, findfile->cFileName, MAX_PATH);
	#endif
	CoTaskMemFree(ss.pwcsName);
	return TRUE;
#else
	BOOL result = VFSFN_SHN(&zf->shn, findfile);

	resetflag(findfile->dwFileAttributes, FILE_ATTRIBUTE_VIRTUAL);
	return result;
#endif
}

void CloseZipFolder(ZIPFOLDERSTRUCT *zfs)
{
#if !ZIPFOLDER_USESTRAGEENUM
	if ( zfs->pSF != NULL ) zfs->pSF->Release();
#endif
	if ( zfs->pST != NULL ) zfs->pST->Release();
	if ( SUCCEEDED(zfs->ComInitResult) ) ::CoUninitialize();
	::FreeLibrary(zfs->hDLL);
}

void ZipFolderFClose(FF_ZIPFOLDER *zf)
{
#if ZIPFOLDER_USESTRAGEENUM
	if ( zf->pESS != NULL ) zf->pESS->Release();
#else
	if ( zf->shn.pEID != NULL ) zf->shn.pEID->Release();
	if ( zf->shn.pMA != NULL ) zf->shn.pMA->Release();
#endif
	CloseZipFolder(&zf->zfs);
}

ERRORCODE GetZipFolderImageSub(LPSTORAGE pST, HWND hWnd, const WCHAR *StreamName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem)
{
	LPSTREAM pStream;
	ERRORCODE result = ERROR_FILE_NOT_FOUND;

	if ( SUCCEEDED(pST->OpenStream(StreamName, NULL, STGM_READ, 0, &pStream)) ){
		STATSTG sstat;
		DWORD size;

		sstat.cbSize.u.LowPart = 0;
		pStream->Stat(&sstat, STATFLAG_DEFAULT);
		CoTaskMemFree(sstat.pwcsName);

		if ( hWnd == INVALID_HANDLE_VALUE ){
			HANDLE hFile;

			hFile = CreateFileL(((IMAGEGETEXINFO *)*mem)->dest, GENERIC_WRITE, 0,
					NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if ( hFile != INVALID_HANDLE_VALUE ){
				BYTE buf[0x10000];
				LARGE_INTEGER TotalSize;
				LARGE_INTEGER TotalTransSize;

				TotalSize.u.LowPart = sstat.cbSize.u.LowPart;
				TotalSize.u.HighPart = 0;
				TotalTransSize.u.LowPart = 0;
				TotalTransSize.u.HighPart = 0;

				result = MAX32;
				while( SUCCEEDED(pStream->Read(&buf, sizeof(buf), &size)) ){
					if ( !size ) break;
					WriteFile(hFile, buf, size, &size, NULL);
					TotalTransSize.u.LowPart += size;

					if ( ((IMAGEGETEXINFO *)*mem)->Progress != NULL ){
							((IMAGEGETEXINFO *)*mem)->Progress(TotalSize,
							TotalTransSize, TotalTransSize, TotalTransSize,
							0, 0, NULL, NULL, ((IMAGEGETEXINFO *)*mem)->lpData);
						if ( *((IMAGEGETEXINFO *)*mem)->Cancel ){
							result = ERROR_CANCELLED;
							break;
						}
					}
				}
				CloseHandle(hFile);
				if ( result == ERROR_CANCELLED ){
					DeleteFileL( ((IMAGEGETEXINFO *)*mem)->dest );
				}
			}
		}else{
			for ( ; ; ){
				*sizeL = sstat.cbSize.u.LowPart;
				*sizeH = 0;
				if ( CheckLoadSize(hWnd, sizeL) == FALSE ) break;
				if ( (*hMap = GlobalAlloc(GMEM_MOVEABLE, *sizeL + 4096)) == NULL){
					break;
				}
				if ( (*mem = (BYTE *)GlobalLock(*hMap)) == NULL ){
					GlobalFree(*hMap);
					break;
				}
				pStream->Read(*mem, *sizeL, &size);
				result = NO_ERROR;
				break;
			}
		}
		pStream->Release();
	}
	return result;
}
#if ZIPFOLDER_USESTRAGEENUM
ERRORCODE GetZipFolderImageSubDir(LPSTORAGE pParentST, const WCHAR *SubName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem)
{
	LPSTORAGE pST;
	LPENUMSTATSTG pESS;
	ERRORCODE result = MAX32;
	TCHAR *destp;
	#ifdef UNICODE
	#define NameA(nam) nam
	#define NameFixA(nam) nam
	#else
	char namea[VFPS];
	#define NameA(nam) namea
	#define NameFixA(nam) UnicodeToAnsi(nam, namea, VFPS);
	#endif

	if ( FAILED(pParentST->OpenStorage(SubName, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &pST)) ){
		return ERROR_FILE_NOT_FOUND;
	}
	destp = ((IMAGEGETEXINFO *)*mem)->dest;
	CreateDirectoryL(destp, NULL);
	destp += tstrlen(destp);

	if ( SUCCEEDED(pST->EnumElements(0, NULL, 0, &pESS)) ){
		STATSTG sstat;

		while ( pESS->Next(1, &sstat, NULL) == S_OK ){
			NameFixA(sstat.pwcsName);
			CatPath(NULL, ((IMAGEGETEXINFO *)*mem)->dest, NameA(sstat.pwcsName));
			if ( sstat.type == STGTY_STORAGE ){
				result = GetZipFolderImageSubDir(pST, sstat.pwcsName, sizeL, sizeH, hMap, mem);
			}else{
				result = GetZipFolderImageSub(pST, (HWND)INVALID_HANDLE_VALUE, sstat.pwcsName, sizeL, sizeH, hMap, mem);
			}
			*destp = '\0';
			CoTaskMemFree(sstat.pwcsName);
			if ( result != MAX32 ) break;
		}
		pESS->Release();
	}
	pST->Release();
	return result;
}
#else
BOOL ExtractMain(ZIPFOLDERSTRUCT *zfs, LPITEMIDLIST *pidl, DWORD count, const TCHAR *destdir)
{
	BOOL result;
	IDataObject *DataObject;
	IDropTarget *DropTarget;

	if ( FAILED(zfs->pSF->GetUIObjectOf(NULL, count, (LPCITEMIDLIST *)pidl,
			IID_IDataObject, NULL, (void **)&DataObject) )){
		return FALSE;
	}
	DropTarget = (IDropTarget *)GetPathInterface(NULL, destdir, IID_IDropTarget, NULL);
	if ( DropTarget != NULL ){
		result = CopyToDropTarget(DataObject, DropTarget, FALSE, NULL, DROPEFFECT_COPY);
		DropTarget->Release();
	}else{
		result = FALSE;
	}
	DataObject->Release();
	return result;
}
#endif

BOOL CopyToDropTarget(IDataObject *DataObject, IDropTarget *DropTarget, BOOL droptype, HWND hTargetWnd, DWORD alloweffect)
{
	POINT pos = { 0, 0 };
	POINTL posl;
	DWORD dropeffect;
	DWORD keystate;

	keystate = droptype ? MK_RBUTTON : MK_LBUTTON;

	if ( hTargetWnd != NULL ) ClientToScreen(hTargetWnd, &pos);
	posl.x = pos.x;
	posl.y = pos.y;
	dropeffect = alloweffect;
	if ( FAILED(DropTarget->DragEnter(DataObject, keystate, posl, &dropeffect)) ){
		return FALSE;
	}
	dropeffect = alloweffect;
	if ( FAILED(DropTarget->DragOver(keystate, posl, &dropeffect)) ){
		return FALSE;
	}
	dropeffect = alloweffect;
	if ( FAILED(DropTarget->Drop(DataObject, keystate, posl, &dropeffect)) ){
		return FALSE;
	}
	return TRUE;
}

ERRORCODE GetZipFolderImage(HWND hWnd, const TCHAR *ArchiveName, const TCHAR *EntryName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem, int type)
{
	ZIPFOLDERSTRUCT zfs;
	ERRORCODE result = ERROR_READ_FAULT;
	TCHAR sub[VFPS], *subp, *lastep;
	#ifdef UNICODE
	#define NameW(nam) nam
	#define NameFixW(nam)
	#else
	WCHAR namew[VFPS];
	#define NameW(nam) namew
	#define NameFixW(nam) AnsiToUnicode(nam, namew, VFPS);
	#endif

#if ZIPFOLDER_USESTRAGEENUM
	if ( FALSE == OpenZipFolder(&zfs, ArchiveName, NilStrNC, type, TRUE) ){
		return ERROR_FILE_NOT_FOUND;
	}
#else
	{
		TCHAR *tp;

		tstrcpy(sub, EntryName);
		lastep = VFSFindLastEntry(sub);
		if ( *lastep == '\\' ){
			*lastep++ = '\0';
			tp = sub;
		}else{
			tp = NilStrNC;
		}
		if ( FALSE == OpenZipFolder(&zfs, ArchiveName, tp, type, TRUE) ){
			return ERROR_FILE_NOT_FOUND;
		}
	}
	if ( hWnd == INVALID_HANDLE_VALUE ){
		LPITEMIDLIST pidl;
		TCHAR destdir[VFPS];

		tstrcpy(destdir, ((IMAGEGETEXINFO *)*mem)->dest);
		if ( *lastep == '\0' ){
			LPENUMIDLIST pEID;

			if ( SUCCEEDED(zfs.pSF->EnumObjects(GetFocus(),
					MAKEFLAG_EnumObjectsForFolder, &pEID)) ){
				ThSTRUCT pidls;
				DWORD count = 0;

				ThInit(&pidls);
				result = MAX32;
				while ( pEID->Next(1, &pidl, NULL) == S_OK ){
					ThAppend(&pidls, &pidl, sizeof(pidl));
					count++;
				}
				if ( count ){
					LPITEMIDLIST *pidlptr;

					pidlptr = (LPITEMIDLIST *)pidls.bottom;
					if ( ExtractMain(&zfs, pidlptr, count, destdir) == FALSE ){
						result = ERROR_READ_FAULT;
					}
					FreePIDLS(pidlptr, count);
				}
				ThFree(&pidls);
			}
		}else if ( NULL != (pidl = IShellToPidl(zfs.pSF, lastep)) ){
			*VFSFindLastEntry(destdir) = '\0';

			if ( IsTrue(ExtractMain(&zfs, &pidl, 1, destdir)) ){
				result = MAX32;
			}
			FreePIDL(pidl);
		}
		if ( result == MAX32 ) goto fin;
	}
	LPPERSISTFILE pPF;
										// zip ファイルを読み込む
	if ( (zfs.pST == NULL) || FAILED(zfs.pST->QueryInterface(XIID_IPersistFile, (void **)&pPF)) ){
		// Windows8 は IPersistFile が用意されていないのでファイル展開する。
		IMAGEGETEXINFO exinfo;
		BYTE *tmem;

		if ( hWnd == (HWND)INVALID_HANDLE_VALUE ) goto fin;
		CloseZipFolder(&zfs);
		MakeTempEntry(TSIZEOF(exinfo.dest), exinfo.dest, 0);
		CreateDirectoryL(exinfo.dest, NULL);
		exinfo.Progress = NULL;
		tmem = (BYTE *)&exinfo;
		result = GetZipFolderImage((HWND)INVALID_HANDLE_VALUE, ArchiveName, EntryName, sizeL, sizeH, hMap, &tmem, type);
		if ( result != MAX32 ) return result;
		return GetArchivefileImageFromTempExtract(exinfo.dest, EntryName, sizeL, sizeH, hMap, mem);
	}

	NameFixW(ArchiveName);
	if ( FAILED(pPF->Load(NameW(ArchiveName), STGM_READ | STGM_SHARE_EXCLUSIVE)) ){
		pPF->Release();
		goto fin;
	}
	pPF->Release();
#endif
	tstrcpy(sub, EntryName);
	subp = sub;
	for ( ; ; ){
		LPSTORAGE pST;
		TCHAR *p;

		p = FindPathSeparator(subp);
		if ( p == NULL ) break; // 最後のエントリ→ファイル名
		*p = '\0';
		NameFixW(subp);
		if ( FAILED(zfs.pST->OpenStorage(NameW(subp), NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &pST)) ){
			break;
		}
		zfs.pST->Release();
		zfs.pST = pST;	// 入れ替え
		subp = p + 1;
	}
	NameFixW(subp);
	result = GetZipFolderImageSub(zfs.pST, hWnd, NameW(subp), sizeL, sizeH, hMap, mem);
#if ZIPFOLDER_USESTRAGEENUM
	if ( (hWnd == INVALID_HANDLE_VALUE) && (result != MAX32) ){ // フォルダ？
		IMAGEGETEXINFO igei;
		BYTE *memtmp;

		igei = *((IMAGEGETEXINFO *)*mem);
		memtmp = (BYTE *)&igei;
		result = GetZipFolderImageSubDir(zfs.pST, NameW(subp), sizeL, sizeH, hMap, &memtmp);
	}
#endif
fin:
	CloseZipFolder(&zfs);
	return result;
}

EXTERN_C const IID XIID_TaskbarList = {0x56FDF342, 0xFD6D, 0x11d0, {0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90}};

#if !defined(__BORLANDC__) || (__BORLANDC__ <= 0x509)
#ifndef __ITaskbarList_INTERFACE_DEFINED__
#define __ITaskbarList_INTERFACE_DEFINED__
	EXTERN_C const IID CLSID_TaskbarList = {0x56FDF344, 0xFD6D, 0x11d0, {0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90}};

	class ITaskbarList : public IUnknown
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE HrInit( void) = 0;
		virtual HRESULT STDMETHODCALLTYPE AddTab(HWND hwnd) = 0;
		virtual HRESULT STDMETHODCALLTYPE DeleteTab(HWND hwnd) = 0;
		virtual HRESULT STDMETHODCALLTYPE ActivateTab(HWND hwnd) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetActiveAlt(HWND hwnd) = 0;
	};
#endif 	/* __ITaskbarList_INTERFACE_DEFINED__ */
#endif

#ifndef __ITaskbarList2_INTERFACE_DEFINED__
#define __ITaskbarList2_INTERFACE_DEFINED__
EXTERN_C const IID IID_ITaskbarList2;
	IID IID_TaskbarList2 = {0x602D4995, 0xB13A, 0x429b, {0xA6, 0x6E, 0x19, 0x35, 0xE4, 0x4F, 0x43, 0x17}};
	class ITaskbarList2 : public ITaskbarList
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE MarkFullscreenWindow(HWND hwnd, BOOL fFullscreen) = 0;
	};
#endif 	/* __ITaskbarList2_INTERFACE_DEFINED__ */

#ifdef MIDL_PASS
typedef IUnknown *HIMAGELIST;
#endif
#include <pshpack8.h>

IID IID_TaskbarList3 = {0xea1afb91, 0x9e28, 0x4b86, {0x90, 0xe9, 0x9e, 0x9f, 0x8a, 0x5e, 0xef, 0xaf}};

#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__
typedef struct tagTHUMBBUTTON {
	DWORD dwMask;
	UINT iId;
	UINT iBitmap;
	HICON hIcon;
	WCHAR szTip[260];
	DWORD dwFlags;
} THUMBBUTTON;

typedef struct tagTHUMBBUTTON *LPTHUMBBUTTON;

#include <poppack.h>
// THUMBBUTTON flags
#define THBF_ENABLED             0x0000
#define THBF_DISABLED            0x0001
#define THBF_DISMISSONCLICK      0x0002
#define THBF_NOBACKGROUND        0x0004
#define THBF_HIDDEN              0x0008
// THUMBBUTTON mask
#define THB_BITMAP          0x0001
#define THB_ICON            0x0002
#define THB_TOOLTIP         0x0004
#define THB_FLAGS           0x0008
#define THBN_CLICKED        0x1800

typedef enum TBATFLAG {
	TBATF_USEMDITHUMBNAIL	= 0x1,
	TBATF_USEMDILIVEPREVIEW	= 0x2
} TBATFLAG;



EXTERN_C const IID IID_ITaskbarList3;

	class ITaskbarList3 : public ITaskbarList2
	{
	public:
#ifdef _WIN64
		virtual HRESULT STDMETHODCALLTYPE SetProgressValue(
			HWND hwnd, ULONGLONG ullCompleted, ULONGLONG ullTotal) = 0;
#else
		// ULONGLONG に対応していないコンパイラ対策
		virtual HRESULT STDMETHODCALLTYPE SetProgressValue(
			HWND hwnd, DWORD dummy1, DWORD ullCompleted, DWORD dummy2, DWORD ullTotal) = 0;
		#define ITaskbarList3_OldCompiler
#endif

		virtual HRESULT STDMETHODCALLTYPE SetProgressState(
			HWND hwnd, int tbpFlags) = 0;

		virtual HRESULT STDMETHODCALLTYPE RegisterTab(
			HWND hwndTab, HWND hwndMDI) = 0;

		virtual HRESULT STDMETHODCALLTYPE UnregisterTab(HWND hwndTab) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetTabOrder(
			HWND hwndTab, HWND hwndInsertBefore) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetTabActive(
			HWND hwndTab, HWND hwndMDI, TBATFLAG tbatFlags) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons(
			HWND hwnd, UINT cButtons, LPTHUMBBUTTON pButton) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons(
			HWND hwnd, UINT cButtons, LPTHUMBBUTTON pButton) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList(
			HWND hwnd, HIMAGELIST himl) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon(
			HWND hwnd, HICON hIcon, LPCWSTR pszDescription) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip(
			HWND hwnd, LPCWSTR pszTip) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip(
			HWND hwnd, RECT *prcClip) = 0;
	};
#endif 	/* __ITaskbarList3_INTERFACE_DEFINED__ */

ITaskbarList *pTaskbarList = NULL;
ITaskbarList3 *pTaskbarList3 = NULL;
const WCHAR TaskElevate[] = L"";

extern "C" void InitTaskBarButtonIF(WPARAM wParam)
{
	if ( WinType < WINTYPE_7 ) return;

	EnterCriticalSection(&ThreadSection);
	if ( pTaskbarList != NULL ){
		if ( wParam ){
			LeaveCriticalSection(&ThreadSection);
			return;
		}
		if ( pTaskbarList3 != NULL ){
			pTaskbarList3->Release();
			pTaskbarList3 = NULL;
		}
		pTaskbarList->Release();
		pTaskbarList = NULL;
	}
	LeaveCriticalSection(&ThreadSection);

	// ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if ( SUCCEEDED(::CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, XIID_TaskbarList, (void **)(&pTaskbarList))) ){
		pTaskbarList->HrInit();
		if ( SUCCEEDED(pTaskbarList->QueryInterface(IID_TaskbarList3, (void **)(&pTaskbarList3)) )){
			pTaskbarList3->HrInit();
			if ( Sm->RunAsMode == RUNAS_NOCHECK ) CheckRunAs();
			if ( Sm->RunAsMode == RUNAS_RUNAS ){ // 別ユーザ, 昇格中ならアイコンを付加 ※タスクバーのアイコンが小さいときは表示されない
				pTaskbarList3->SetOverlayIcon(GetFocus(), LoadIcon((HINSTANCE)hShell32, MAKEINTRESOURCE(220)), TaskElevate);
			}
		}
	}
	// ::CoUninitialize();
}

extern "C" void SetTaskBarButtonProgress(HWND hWnd, int nowcount, int maxcount)
{
	if ( pTaskbarList3 == NULL ){
		if ( pTaskbarList != NULL ) return;
		InitTaskBarButtonIF(1);
		if ( pTaskbarList3 == NULL ) return;
	}

	#if 0
	{	// 呼び出し間隔計測用
		DWORD oldtick, nowtick = GetTickCount();

		oldtick = (DWORD)GetProp(hWnd, T("IntervalCheck"));
		if ( (nowtick - oldtick) < 100 ){
			XMessage(NULL, NULL, XM_DbgLOG, T("TaskBar %x %3dm %d"), (DWORD)hWnd & 0xff, (nowtick - oldtick), nowcount);
		}
		SetProp(hWnd, T("IntervalCheck"), (HANDLE)nowtick);
	}
	#endif

	if ( maxcount != 0 ){
		#if defined(ITaskbarList3_OldCompiler)
			pTaskbarList3->SetProgressValue(hWnd, 0, nowcount, 0, maxcount);
		#else
			pTaskbarList3->SetProgressValue(hWnd, nowcount, maxcount);
		#endif
		pTaskbarList3->SetProgressState(hWnd, TBPF_NORMAL);
	}else{
		pTaskbarList3->SetProgressState(hWnd, (TBPFLAG)nowcount);
	}
}

void * GetPathInterface(HWND hWnd, const TCHAR *name, REFIID riid, const TCHAR *path)
{
	LPSHELLFOLDER pSF;
	LPITEMIDLIST pidl;
	void *ifptr;

	if ( VFSMakeIDL(path, &pSF, &pidl, name) == FALSE ) return NULL;
	if ( FAILED(pSF->GetUIObjectOf(hWnd, 1, (LPCITEMIDLIST *)&pidl,
			riid, NULL, & ifptr)) ){
		ifptr = NULL;
	}
	FreePIDL(pidl);
	pSF->Release();
	return ifptr;
}

VFSDLL ERRORCODE PPXAPI VFSLoadFileImage(const TCHAR *filename, DWORD margin, char **image, DWORD *imagesize, DWORD *filesize)
{
	ERRORCODE result;
	IStream *pStream;
	STATSTG sstat;
	HWND hWnd = GetFocus();

	result = LoadFileImage(filename, margin, image, imagesize, filesize);
	if ( result == NO_ERROR ) return NO_ERROR;

	LPSHELLFOLDER pSF;
	LPITEMIDLIST pidl;

	// IStream を取得する
	if ( VFSMakeIDL(NULL, &pSF, &pidl, filename) == FALSE ) return result;
	if ( FAILED(pSF->BindToObject(pidl, NULL, IID_IStream, (void **)&pStream)) ){
		pStream = NULL;
	}
	FreePIDL(pidl);
	pSF->Release();
	if ( pStream == NULL ) return result;

	// IStream から内容を取得する
	if ( SUCCEEDED(pStream->Stat(&sstat, STATFLAG_NONAME)) ){
		for (;;){
			BOOL freeheap = FALSE;
			DWORD sizeL = sstat.cbSize.u.LowPart;

			if ( sstat.cbSize.u.HighPart != 0 ) break;

			if ( (filesize == LFI_ALWAYSLIMIT) ||
				 (filesize == LFI_ALWAYSLIMITLESS) ){
				hWnd = (HWND)filesize;
				filesize = NULL;
			}
			if ( filesize != NULL ) *filesize = sizeL;

			if ( *image != NULL ){				// メモリは確保済み
				DWORD imgsize;

				imgsize = *imagesize;
				if ( imgsize < sizeL ) sizeL = imgsize;
				if ( imgsize < (sizeL + margin) ) margin = imgsize - sizeL;
			}else{
				if ( CheckLoadSize(hWnd, &sizeL) == FALSE ) break;

				if ( (*image = static_cast<char *>(HeapAlloc(ProcHeap, 0, sizeL + margin))) == NULL ){
					break;
				}
				freeheap = TRUE;
			}
			if ( FAILED(pStream->Read(*image, sizeL, &sizeL)) ){
				if ( IsTrue(freeheap) ) HeapFree(ProcHeap, 0, *image);
				break;
			}
			result = NO_ERROR;
			if ( margin != 0 ) memset(*image + sizeL, 0, margin);
			if ( imagesize != NULL ) *imagesize = sizeL;
			break;
		}
	}
	pStream->Release();
	return result;
}

const IID xxXIID_IFileOperationProgressSink = xIID_IFileOperationProgressSink;
class CIFileOperationProgressSink : public xIFileOperationProgressSink
{
	private:
		long m_cRefCount;
		FOPSTRUCT *m_FS;
	public:
		CIFileOperationProgressSink(FOPSTRUCT *FS){
			m_FS = FS;
			m_cRefCount = 1;
		};
//		~CIFileOperationProgressSink(){ };

		STDMETHOD(QueryInterface) (REFIID riid, void **ppv) {
			if ( (riid == IID_IUnknown) || (riid == xxXIID_IFileOperationProgressSink) ){
				*ppv = this;
				AddRef();
				return NOERROR;
			}
			*ppv = NULL;
			return ResultFromScode(E_NOINTERFACE);
		};
	STDMETHOD_(ULONG, AddRef) (void)  { return ++m_cRefCount; };
	STDMETHOD_(ULONG, Release) (void) {
		long tempCount;

		tempCount = --m_cRefCount;
		if ( tempCount == 0 ) { XMessage(NULL,NULL,XM_DbgLOG,T("sink free")); delete this;}
		return tempCount;
	}

	STDMETHOD(StartOperations)(void) { XMessage(NULL,NULL,XM_DbgLOG,T("Start IFO")); return S_OK; };

	STDMETHOD(FinishOperations)(HRESULT) { XMessage(NULL,NULL,XM_DbgLOG,T("Fin IFO")); return S_OK; };
	STDMETHOD(PreRenameItem)(DWORD flags, xIShellItem *, LPCWSTR) { XMessage(NULL,NULL,XM_DbgLOG,T("PreRen %x"),flags); return S_OK; };
	STDMETHOD(PostRenameItem)(DWORD flags, xIShellItem *psiItem, LPCWSTR pszNewName, HRESULT hrRename, xIShellItem *psiNewlyCreated) { XMessage(NULL,NULL,XM_DbgLOG,T("PostRen %x %x"),flags, hrRename); return S_OK; };
	STDMETHOD(PreMoveItem)(DWORD dwFlags, xIShellItem *psiItem, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName) { return S_OK; };
	STDMETHOD(PostMoveItem)(DWORD flags, xIShellItem *psiItem, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName, HRESULT hrMove, xIShellItem *psiNewlyCreated) { XMessage(NULL,NULL,XM_DbgLOG,T("PostMove %x %x"),flags, hrMove); return S_OK; };
	STDMETHOD(PreCopyItem)(DWORD dwFlags, xIShellItem *psiItem, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName) { return S_OK; };
	STDMETHOD(PostCopyItem)(DWORD flags, xIShellItem *psiItem, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName, HRESULT hrCopy, xIShellItem *psiNewlyCreated) { XMessage(NULL,NULL,XM_DbgLOG,T("PostCopy %x %x"),flags, hrCopy); return S_OK; };
	STDMETHOD(PreDeleteItem)(DWORD dwFlags, xIShellItem *psiItem) { return S_OK; };
	STDMETHOD(PostDeleteItem)(DWORD dwFlags, xIShellItem *psiItem, HRESULT hrDelete, xIShellItem *psiNewlyCreated) { return S_OK; };
	STDMETHOD(PreNewItem)(DWORD dwFlags, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName) { return S_OK; };
	STDMETHOD(PostNewItem)(DWORD dwFlags, xIShellItem *psiDestinationFolder, LPCWSTR pszNewName, LPCWSTR pszTemplateName, DWORD dwFileAttributes, HRESULT hrNew, xIShellItem *psiNewItem) { return S_OK; };
	STDMETHOD(UpdateProgress)(UINT iWorkTotal, UINT iWorkSoFar) { XMessage(NULL,NULL,XM_DbgLOG,T("Prog %d %d"),iWorkTotal, iWorkSoFar); return S_OK; };
	STDMETHOD(ResetTimer)(void) { XMessage(NULL,NULL,XM_DbgLOG,T("ResetTimer")); return S_OK; };
	STDMETHOD(PauseTimer)(void) { XMessage(NULL,NULL,XM_DbgLOG,T("PauseTimer"));return S_OK; };
	STDMETHOD(ResumeTimer)(void) { XMessage(NULL,NULL,XM_DbgLOG,T("ResumeTimer"));return S_OK; };
};

const IID xxXIID_IShellItem =
{0x43826d1e, 0xe718, 0x42ee, {0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe}};
IID XIID_IFileOperation = xIID_IFileOperation;
CLSID XCLSID_IFileOperation = xCLSID_IFileOperation;
DefineWinAPI(HRESULT, SHCreateItemFromParsingName, (PCWSTR, IBindCtx *, REFIID, void **));

void CheckAndInitIfo(FOPSTRUCT *FS)
{
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_IFILEOP ){
		if ( FAILED(CoCreateInstance(XCLSID_IFileOperation, NULL, CLSCTX_ALL,
				XIID_IFileOperation, (LPVOID *)&FS->ifo)) ){
			FS->ifo = NULL;
		}else{
			DWORD cookie;

			FS->ifo->SetOwnerWindow(FS->hDlg);
			FS->ifo->SetOperationFlags(
//				FOF_ALLOWUNDO |	// Vista 以降ならセッション全体でUndo可能
//				FOF_FILESONLY | // ワイルドカード指定時、ディレクトリを無視
//				FOF_NOCONFIRMATION | // 全て Yes
				FOF_NOCONFIRMMKDIR | // フォルダの作成確認をしない
				FOF_NO_CONNECTED_ELEMENTS | // ファイルを組扱いしない
				FOF_NOCOPYSECURITYATTRIBS | // ACLをコピーしない
//				FOF_NOERRORUI | // エラー表示しない。無視扱い
//				FOF_NORECURSION | // サブディレクトリ無し
				FOF_RENAMEONCOLLISION | // 名前が被ったら、名前変更
//				FOF_SILENT | // プログレス無し
				FOF_WANTNUKEWARNING | // 削除時、完全削除するなら警告
//				FOFX_ADDUNDORECORD | // ユーザ指定によりundo記録 Win8
				FOFX_NOSKIPJUNCTIONS | // Shell namespace junctions を無視しない
//				FOFX_PREFERHARDLINK | // できるだけハードリンク
				FOFX_SHOWELEVATIONPROMPT | // 必要なら UAC
				FOFX_EARLYFAILURE | // エラーが出たらすぐに終了
				FOFX_PRESERVEFILEEXTENSIONS | // 名前変更は拡張子を維持
//				FOFX_KEEPNEWERFILE | // 新しいファイル側を残す
//				FOFX_NOCOPYHOOKS | // コピー拡張をしない
//				FOFX_NOMINIMIZEBOX | // 最小化ボタン無し
//				FOFX_MOVEACLSACROSSVOLUMES | // 別ドライブでもACLコピー
//				FOFX_DONTDISPLAYSOURCEPATH | // ソース名非表示
//				FOFX_DONTDISPLAYDESTPATH | // 先非表示
				FOFX_RECYCLEONDELETE | // ごみ箱削除 Win8
//				FOFX_REQUIREELEVATION | // 昇格の確認ダイアログ無し→いきなり昇格へ VistaSP1
//				FOFX_COPYASDOWNLOAD | // 表示をダウンロードにする Win7
//				FOFX_DONTDISPLAYLOCATIONS | // ダイアログに場所情報が無い Win7
				0);
			GETDLLPROC(hShell32, SHCreateItemFromParsingName);
			CIFileOperationProgressSink *ifop = new CIFileOperationProgressSink(FS);
			FS->ifo->Advise(ifop, &cookie); // cookie は必須
			ifop->Release(); // ifo 内で Addref されている。ifo->Release で解放
		}
	}else{
		FS->ifo = NULL;
	}
}

BOOL CopyFileWithIfo(FOPSTRUCT *FS, const TCHAR *src, TCHAR *dst)
{
	xIShellItem *destShellItem = NULL;
	xIShellItem *srcShellItem;
	WCHAR destdirW[VFPS];
	WCHAR *dstptr;
	HRESULT result = FALSE;
#ifdef UNICODE
	#define srcdirW src
	tstrcpy(destdirW, dst);
#else
	WCHAR srcdirW[VFPS];
	AnsiToUnicode(dst, destdirW, VFPS);
	AnsiToUnicode(src, srcdirW, VFPS);
#endif
	dstptr = strrchrW(destdirW, '\\');
	if ( dstptr != NULL ) *dstptr = '\0';

	if ( FAILED(DSHCreateItemFromParsingName(destdirW, NULL, xxXIID_IShellItem, (void**)&destShellItem)) ){
		return FALSE;
	}
	if ( FAILED(DSHCreateItemFromParsingName(srcdirW, NULL, xxXIID_IShellItem, (void**)&srcShellItem)) ){
		goto closedst;
	}
	if ( FS->opt.fop.mode == FOPMODE_MOVE ){
		result = FS->ifo->MoveItem(srcShellItem, destShellItem, NULL, NULL);
	}else{
		result = FS->ifo->CopyItem(srcShellItem, destShellItem, NULL, NULL);
	}
	if ( SUCCEEDED(result) ){
		result = FS->ifo->PerformOperations();
	}
	srcShellItem->Release();
closedst:
	destShellItem->Release();
	if ( SUCCEEDED(result) ){
		return TRUE;;
	}else{
		XMessage(NULL,NULL,XM_DbgLOG,T("ifo failed %x"),result);
		return FALSE;;
	}
}

void FreeIfo(FOPSTRUCT *FS)
{
	FS->ifo->Release();
	FS->ifo = NULL;
}
