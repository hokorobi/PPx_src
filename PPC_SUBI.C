/*-----------------------------------------------------------------------------
	Paper Plane cUI								SubThread - 画像関係

	celltmp は、コピー済み(スレッドセーフ)の ENTRYCELL
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include <shlobj.h>
#include "WINOLE.H"

#include "PPX.H"
#include "VFS.H"
#include "PPCUI.RH"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPC_SUBT.H"
#pragma hdrstop

#define RETRY_ICON_CHALLENGE 10 // アイコン取得の再試行数
#define RETRY_ICON_WAIT 20 // 再試行時の待機時間

//---- COM 定義
// SHCreateItemFromParsingName
const IID XIID_IExtractImage =
{0xBB2E617C, 0x0920, 0x11d1, {0x9A, 0x0B, 0x00, 0xC0, 0x4F, 0xC2, 0xD6, 0xC1}};
const IID XIID_IThumbnailCache =
{0xF676C15D, 0x596A, 0x4ce2, {0x82, 0x34, 0x33, 0x99, 0x6F, 0x44, 0x5D, 0xB1}};
const IID XIID_IShellItem =
{0x43826d1e, 0xe718, 0x42ee, {0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe}};
const CLSID XCLSID_LocalThumbnailCache =
{0x50EF4544, 0xAC9F, 0x4A8E, {0xB2, 0x1B, 0x8A, 0x26, 0x18, 0x0D, 0xB1, 0x3F}};
// IID XIID_ISharedBitmap = {0x091162a4, 0xbc96, 0x411f,{0xaa, 0xe8, 0xc5, 0x12, 0x2c, 0xd0, 0x33, 0x63}};

enum x_SIIGBF
{
	xSIIGBF_RESIZETOFIT = 0,
	xSIIGBF_BIGGERSIZEOK = 0x1,
	xSIIGBF_MEMORYONLY = 0x2,
	xSIIGBF_ICONONLY = 0x4,
	xSIIGBF_THUMBNAILONLY = 0x8,
	xSIIGBF_INCACHEONLY = 0x10,
	xSIIGBF_CROPTOSQUARE = 0x20,
	xSIIGBF_WIDETHUMBNAILS = 0x40,
	xSIIGBF_ICONBACKGROUND = 0x80,
	xSIIGBF_SCALEUP = 0x100
};
typedef int xSIIGBF;

const IID XIID_IShellItemImageFactory =
{0xbcc18b79, 0xba16, 0x442f, {0x80, 0xc4, 0x8a, 0x59, 0xc3, 0x0c, 0x46, 0x3b}};
#undef  INTERFACE
#define INTERFACE xIShellItemImageFactory
DECLARE_INTERFACE_(xIShellItemImageFactory, IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
	STDMETHOD_(ULONG, AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG, Release) (THIS) PURE;

	STDMETHOD(GetImage)(THIS_ SIZE size, xSIIGBF flags, HBITMAP *phbm) PURE;
};


const IID XIID_IShellIconOverlayIdentifier =
{0x0C6C4200L, 0xC589, 0x11D0, {0x99, 0x9A, 0x00, 0xC0, 0x4F, 0xD6, 0x55, 0xE1}};
#ifndef ISIOI_ICONINDEX
#define ISIOI_ICONINDEX 2
#endif
#undef  INTERFACE
#define INTERFACE xIShellIconOverlayIdentifier
DECLARE_INTERFACE_(xIShellIconOverlayIdentifier, IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, void **) PURE;
	STDMETHOD_(ULONG, AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG, Release) (THIS) PURE;

	STDMETHOD(IsMemberOf)(THIS_ PCWSTR pwszPath, DWORD dwAttrib) PURE;
	STDMETHOD(GetOverlayInfo)(THIS_ PWSTR pwszIconFile, int cchMax, int * pIndex, DWORD * pdwFlags) PURE;
	STDMETHOD(GetPriority)(THIS_ int * pIPriority) PURE;
};

//----
const TCHAR RegOverlayName[] = T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers");

typedef struct tagOverlayClass {
	xIShellIconOverlayIdentifier *isoi;
	int index;
} OverlayClass;

typedef struct {
	HICON LayIcon;
//	ANYSIZEICON LayIcon;
	CLSID ComID;
} OverlayComInfo;

struct {
	OverlayComInfo *List;
	int Count;
} OverlayCom = {NULL, -1};

typedef struct {
	DWORD next;	// 次の構造体の位置
	DWORD crc;	// 検索用
	int index;	// アイコンのインデックス
	TCHAR name[]; // ファイル名(ディレクトリ部は無し)
} CACHEICONINFO;

DefineWinAPI(HRESULT, SHCreateItemFromIDList, (LPITEMIDLIST pidl, REFIID riid, void **ppv)) = NULL;

ThSTRUCT CacheIconList = {NULL, MAX32, MAX32};

ANYSIZEICON LinkIcon = {NULL, NULL, NULL, NULL}; // ショートカット矢印

#define SelectIconHandleAnySizeMacro(iconname, iconsize) ((iconsize <= 32) ? ((iconsize <= 16) ? iconname.h16 : iconname.h32) : ((iconsize <= 48) ? iconname.h48 : iconname.h256) )

#define SelectIconSizeMacro(iconsize) ((iconsize <= 32) ? ((iconsize <= 16) ? 16 : 32) : ((iconsize <= 48) ? 48 : 256))

BOOL GetExplorerThumbnail(const TCHAR *filename, HTBMP *hTBmp, int thumsize);
HICON LoadIconOne(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, int iconsize, int mode, OverlayClassTable **LayPtr);

BOOL CheckStaticIcon(PPC_APPINFO *cinfo, const TCHAR *ext, int iconid, int iconmode);


void CheckRotate(ICONCACHESTRUCT *icons)
{
	if ( icons->writeIndex == icons->size ){
		icons->writeIndex = 0;
	}
	if ( icons->maxID >= icons->nextfixID ){
		icons->nextfixID += icons->shift;
		icons->minID += icons->shift;
		icons->minIDindex += icons->shift;
		if ( icons->minIDindex >= icons->size ) icons->minIDindex -= icons->size;
	}
	if ( icons->nextfixID >= ICONCACHEINDEX_FIRSTMAX ){ // 初期化
		icons->shift = icons->size / 3;
		icons->minID = icons->shift;
		icons->maxID = icons->size - 1;
		icons->minIDindex = icons->shift;
		icons->nextfixID = icons->maxID + icons->shift;
		icons->writeIndex = 0;
	}
}

int USEFASTCALL AddIconList(ICONCACHESTRUCT *icons, HICON hIcon)
{
	int index;

	if ( icons->maxID == ICONCACHEINDEX_FIRSTMAX ){ // 途中で初期化された
		icons->alloc = TRUE;
	}
	if ( IsTrue(icons->alloc) ){ // キャッシュを使い切っていない場合
		ERRORCODE result;

		index = DImageList_AddIcon(icons->hImage, hIcon);
		if ( index != -1 ){
			icons->maxID = index;
			if ( index >= icons->size ){
				icons->size = icons->maxID + 1;
				icons->alloc = FALSE;
			}
			return index;
		}
		result = GetLastError();
		if ( (result == ERROR_INVALID_CURSOR_HANDLE) ||
			(result == ERROR_INVALID_ICON_HANDLE) ){
			return ICONLIST_NOINDEX; // イメージリストに使えなかった
		}
		// 保存に失敗…領域が拡張できなかった or 初期化された
		if ( icons->maxID == ICONCACHEINDEX_FIRSTMAX ){
			// 途中で初期化されたため、失敗しているので、再度初期化する指示
			DImageList_Destroy(icons->hImage);
			icons->hImage = NULL;
			return ICONLIST_NOINDEX;
		}
		icons->alloc = FALSE;
		icons->size = icons->maxID + 1;
	}
	CheckRotate(icons);
	DImageList_ReplaceIcon(icons->hImage, icons->writeIndex, hIcon);
	icons->maxID++;
	icons->writeIndex++;
	return icons->maxID;
}

int AddImageIconList(ICONCACHESTRUCT *icons, HBITMAP hImage, HBITMAP hMask)
{
	int index;

	if ( icons->maxID == ICONCACHEINDEX_FIRSTMAX ){ // 途中で初期化された
		icons->alloc = TRUE;
	}
	if ( IsTrue(icons->alloc) ){ // キャッシュを使い切っていない場合
		index = DImageList_Add(icons->hImage, hImage, hMask);
		if ( index != -1 ){
			icons->maxID = index;
			if ( index >= icons->size ){
				icons->size = icons->maxID + 1;
				icons->alloc = FALSE;
			}
			return index;
		}
		// 保存に失敗…領域が拡張できなかった or 初期化された
		if ( icons->maxID == ICONCACHEINDEX_FIRSTMAX ){
			// 途中で初期化されたため、失敗しているので、再度初期化する指示
			DImageList_Destroy(icons->hImage);
			icons->hImage = NULL;
			return ICONLIST_NOINDEX;
		}
		icons->alloc = FALSE;
		icons->size = icons->maxID + 1;
	}
	CheckRotate(icons);
	DImageList_Replace(icons->hImage, icons->writeIndex, hImage, hMask);
	icons->maxID++;
	icons->writeIndex++;
	return icons->maxID;
}

// ImageListキャッシュの一部が廃棄されたのでCACHEICONINFOを調整する
void FixCacheIconInfo(void)
{
	CACHEICONINFO *ci;
	DWORD size;

	ci = (CACHEICONINFO *)CacheIconList.bottom;
	for ( ; ; ){
		if ( ci->index >= CacheIcon.minID ) break;
		if ( ci->next >= CacheIconList.top ) break;
		ci = (CACHEICONINFO *)(CacheIconList.bottom + ci->next);
	}
	size = (DWORD)((BYTE *)ci - (BYTE *)CacheIconList.bottom);
	if ( ci < (CACHEICONINFO *)(CacheIconList.bottom + CacheIconList.top) ){
		for ( ; ; ){
			DWORD next;

			next = ci->next;
			ci->next -= size;
			if ( next >= CacheIconList.top ) break;
			ci = (CACHEICONINFO *)(CacheIconList.bottom + next);
		}
	}
	memmove(CacheIconList.bottom, CacheIconList.bottom + size, CacheIconList.top - size);
	CacheIconList.top -= size;
}

// セルのアイコンがキャッシュにあるかチェック
BOOL SearchCacheIcon(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, int iconmode, OverlayClassTable **LayPtr)
{
	union {
		BYTE buf[sizeof(CACHEICONINFO) + TSTROFF(VFPS)];
		CACHEICONINFO ci;
	} cibuf;
	TCHAR path[VFPS], *filename, *ext;
	DWORD crcname, crcext, size;
	HICON hIcon;
	int listmin;

	filename = celltmp->f.cFileName;
	ext = filename + celltmp->ext;
	if ( VFSFullPath(path, filename, cinfo->path) == NULL ){
		celltmp->icon = ICONLIST_UNKNOWN;
		return TRUE;
	}
	crcname = crc32((BYTE *)path, TSTRLENGTH32(path), 0);
	crcext = crc32((BYTE *)ext, TSTRLENGTH32(ext), 0);

	if ( CacheIconList.top > sizeof(CACHEICONINFO) ){
		CACHEICONINFO *ci;

		ci = (CACHEICONINFO *)CacheIconList.bottom;
		if ( ci != NULL ){
			listmin = CacheIcon.minID;
			for ( ; ; ){
				if ( ci->index >= listmin ){
					if ( (ci->crc == crcname) && !tstrcmp(filename, ci->name) ){
						celltmp->icon = ci->index;
						return TRUE;
					}
					if ( (ci->crc == crcext) && !tstrcmp(ext, ci->name) ){
						celltmp->icon = ci->index;
						CheckStaticIcon(cinfo, ext, ci->index, iconmode);
						return TRUE;
					}
				}
				if ( ci->next >= CacheIconList.top ) break;
				ci = (CACHEICONINFO *)(CacheIconList.bottom + ci->next);
			}
		} else{ // 不整合発生時。今後検証する予定
			CacheIconList.top = 0;
		}
	}
	// キャッシュヒット無し
	// 非同期読み込みのキャッシュ使用時は、新たに読み込みしない
	if ( (cinfo->e.Dtype.ExtData == INVALID_HANDLE_VALUE) && IsTrue(cinfo->SlowMode) ) return TRUE;

	hIcon = LoadIconOne(cinfo, celltmp, CacheIconsY, iconmode, LayPtr);
	if ( hIcon == NULL ){
		celltmp->icon = cibuf.ci.index = ICONLIST_UNKNOWN;
	} else{
		int oldminID;

		oldminID = CacheIcon.minID;
		celltmp->icon = cibuf.ci.index = AddIconList(&CacheIcon, hIcon);
		DestroyIcon(hIcon);
		if ( celltmp->icon == -1 ) return TRUE; // エラーなので新に読み込まない
		if ( oldminID != CacheIcon.minID ){ // キャッシュの一部が廃棄された
			FixCacheIconInfo();
		}
	}
	if ( CheckStaticIcon(cinfo, ext, cibuf.ci.index, iconmode) == FALSE ){
		cibuf.ci.crc = crcname;
		tstrcpy(cibuf.ci.name, filename);
	} else{
		cibuf.ci.crc = crcext;
		tstrcpy(cibuf.ci.name, ext);
	}
	size = sizeof(CACHEICONINFO) + TSTRSIZE32(cibuf.ci.name);
	cibuf.ci.next = CacheIconList.top + size;
	ThAppend(&CacheIconList, &cibuf.ci, size);
	return FALSE;
}

//-----------------------------------------------------------------------------
// 情報行用のアイコンを取得表示する
void GetInfoIcon(PPC_APPINFO *cinfo, HICON *hInfoIcon, SubThreadData *threaddata)
{
	HICON hIcon;
	ENTRYCELL celltmp;

	EnterCellEdit(cinfo);
	if ( TinyCheckCellEdit(cinfo) ){
		LeaveCellEdit(cinfo);
		return;
	}

	celltmp = CEL(cinfo->e.cellN); // 対象cellを取得
	LeaveCellEdit(cinfo);
	threaddata->threeadinfo.lParam = (LPARAM)celltmp.f.cFileName;
	setflag(threaddata->threeadinfo.threadinfo.flag, XTHREAD_RESTARTREQUEST);

	if ( celltmp.icon != ICONLIST_BROKEN ){
		hIcon = LoadIconOne(cinfo, &celltmp, cinfo->XC_ifix_size.cx, cinfo->dset.infoicon, &threaddata->LayPtr); // アイコン取得
	} else{
		hIcon = NULL;
	}
	DIRECTXDEFINE(cinfo->InfoIcon_DirtyCache = TRUE);
	if ( hIcon != NULL ){						// 差し替え
		cinfo->hInfoIcon = hIcon;
	} else{
		cinfo->hInfoIcon = LoadUnknownIcon(cinfo, cinfo->XC_ifix_size.cx);
	}
	if ( *hInfoIcon != NULL ){					// 旧アイコンを削除
		// アイコン表示とアイコン削除が被らないようにする
		EnterCellEdit(cinfo);
		DestroyIcon(*hInfoIcon);
		LeaveCellEdit(cinfo);
	}
	*hInfoIcon = hIcon;
												// 表示指示
										// 表示要求が既にある時は表示を断念
	if ( !(cinfo->SubTCmdFlags & SUBT_GETINFOICON) ){
		DocksInfoRepaint(&cinfo->docks);
		if ( cinfo->combo ){
			PostMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_drawinfo, 0);
		}
		if ( !cinfo->combo || !(X_combos[0] & CMBS_COMMONINFO) ){ // 非共用時
			RECT rect;

			rect.left = cinfo->BoxInfo.left;
			rect.top = cinfo->BoxInfo.top;
			rect.right = rect.left + cinfo->iconR;
			rect.bottom = cinfo->BoxInfo.bottom - 1;
			InvalidateRect(cinfo->info.hWnd, &rect, FALSE);	// 更新指定
		}
	}
	resetflag(threaddata->threeadinfo.threadinfo.flag, XTHREAD_RESTARTREQUEST);
}

typedef struct {
	int indexcount;
	const TCHAR *resid;
	TCHAR resname[64];
} FINDICONINDEXDATA;
#pragma argsused
BOOL CALLBACK FindIconIndexProc(HANDLE hModule, LPCTSTR type, LPTSTR name, LONG_PTR lParam)
{
	UnUsedParam(hModule); UnUsedParam(type);

	if ( ((FINDICONINDEXDATA *)lParam)->indexcount > 0 ){
		((FINDICONINDEXDATA *)lParam)->indexcount--;
		return TRUE;
	}
	if ( ((LONG_PTR)name & ~0xffff) == 0 ){
		((FINDICONINDEXDATA *)lParam)->resid = name;
	} else if ( tstrlen(name) < (64 / sizeof(TCHAR)) ){
		tstrcpy(((FINDICONINDEXDATA *)lParam)->resname, name);
		((FINDICONINDEXDATA *)lParam)->resid = ((FINDICONINDEXDATA *)lParam)->resname;
	}
	return FALSE;
}

// index : 正:リソースの先頭からのインデックス、負:-リソースID
HICON LoadIconDx(const TCHAR *IDorDLLname, int index, int iconsize)
{
	if ( iconsize == 0 ) iconsize = 32;
	iconsize = SelectIconSizeMacro(iconsize);
	if ( index == LIDX_FILE ){
		return LoadImage(NULL, IDorDLLname, IMAGE_ICON, iconsize, iconsize, LR_LOADFROMFILE);
	} else{
		HICON hIcon;
		HMODULE hResFile;
		BOOL freedll = FALSE;
		FINDICONINDEXDATA fiid;

		hResFile = GetModuleHandle(IDorDLLname);
		if ( hResFile == NULL ){
			hResFile = LoadLibraryEx(IDorDLLname, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
			if ( hResFile == NULL ) return NULL;
			freedll = TRUE;
		}
		if ( index < 0 ){ // リソースID
			hIcon = LoadImage(hResFile, MAKEINTRESOURCE(-index), IMAGE_ICON, iconsize, iconsize, LR_DEFAULTCOLOR);
		} else{ // 先頭からのindex
			fiid.indexcount = index;
			fiid.resid = 0;
			EnumResourceNames(hResFile, RT_GROUP_ICON, (ENUMRESNAMEPROC)FindIconIndexProc, (LONG_PTR)&fiid);
			if ( fiid.resid != 0 ){
				hIcon = LoadImage(hResFile, fiid.resid, IMAGE_ICON, iconsize, iconsize, LR_DEFAULTCOLOR);
			}else{
				hIcon = NULL;
			}
		}
		if ( hIcon == NULL ){ // index 不一致の時は、１つ目の icon を使う
			fiid.indexcount = 0;
			fiid.resid = 0;
			EnumResourceNames(hResFile, RT_GROUP_ICON, (ENUMRESNAMEPROC)FindIconIndexProc, (LONG_PTR)&index);
			if ( fiid.resid != 0 ){
				hIcon = LoadImage(hResFile, fiid.resid, IMAGE_ICON, iconsize, iconsize, LR_DEFAULTCOLOR);
			}
//			if ( hIcon == NULL ) XMessage(NULL, NULL, XM_DbgLOG, T("%s %d"), IDorDLLname, index);
		}
		if ( freedll ) FreeLibrary(hResFile);
		return hIcon;
	}
}

// 拡張子からアイコンを取得する
HICON GetIconByExt(const TCHAR *ext, int iconsize)
{
	TCHAR buf[VFPS], progid[VFPS];
	TCHAR *indexp;
	int index = -1;
										// 拡張子からキーを求める -------------
	if ( !GetRegString(HKEY_CLASSES_ROOT, ext, NilStr, progid, TSIZEOF(progid)) ){
		return NULL;
	}
										// アプリケーションのシェル -----------
	tstrcpy(buf, progid);
	tstrcat(buf, T("\\DefaultIcon"));
	if ( !GetRegString(HKEY_CLASSES_ROOT, buf, NilStr, buf, TSIZEOF(buf)) ){
		return NULL;
	}
	tstrreplace(buf, T("\""), NilStr);
	if ( (buf[0] == '%') && (buf[1] == '1') ){
		tstrcpy(buf, progid);
		tstrcat(buf, T("\\CLSID"));
		if ( GetRegString(HKEY_CLASSES_ROOT, buf, NilStr, buf, TSIZEOF(buf)) ){
			TCHAR oldname[MAX_PATH];

			wsprintf(oldname, T("CLSID\\%s\\DefaultIcon"), buf);
			GetRegString(HKEY_CLASSES_ROOT, oldname, NilStr, buf, TSIZEOF(buf));
		} else{
			tstrcpy(buf, ext);
		}
	} // 固定
	indexp = tstrrchr(buf, ',');
	if ( indexp != NULL ){
		*indexp++ = '\0';
		index = GetIntNumber((const TCHAR **)&indexp); // 正:offset 負:resid
	}
	return LoadIconDx(buf, index, iconsize);
}

void CloseAnySizeIcon(ANYSIZEICON *icons)
{
	if ( icons->h16 != NULL ){
		DestroyIcon(icons->h16);
		icons->h16 = NULL;
	}
	if ( icons->h32 != NULL ){
		DestroyIcon(icons->h32);
		icons->h32 = NULL;
	}
	if ( icons->h48 != NULL ){
		DestroyIcon(icons->h48);
		icons->h48 = NULL;
	}
	if ( icons->h256 != NULL ){
		DestroyIcon(icons->h256);
		icons->h256 = NULL;
	}
}

void FreeOverlayCom(void)
{
	OverlayComInfo *oci, *ocip;
	int left;

	CloseAnySizeIcon(&LinkIcon);

	ocip = oci = OverlayCom.List;
	left = OverlayCom.Count;
	if ( oci == NULL ) return;
	OverlayCom.Count = -1;
	OverlayCom.List = NULL;
	for ( ; left > 0; left--, ocip++ ){
		if ( ocip->LayIcon != NULL ) DestroyIcon(ocip->LayIcon);
//		CloseAnySizeIcon(&ocip->LayIcon);
	}
	HeapFree(hProcessHeap, 0, oci);
}

int LoadOverlayCom(OverlayClass **oc)
{
	int ComCount = 0;
	OverlayComInfo *oci;
	HKEY hKey;
	int enumno = 0;

	if ( OverlayCom.Count >= 0 ){
		if ( (*oc == NULL) && (OverlayCom.Count > 0) ){
			OverlayClass *ocp;
			int index;

			ocp = HeapAlloc(hProcessHeap, 0, sizeof(OverlayClass) * (OverlayCom.Count + 1));
			if ( ocp == NULL ) return 0;
			*oc = ocp;
			oci = OverlayCom.List;
			for ( index = 0; index < OverlayCom.Count; index++, oci++ ){
				if ( SUCCEEDED(CoCreateInstance(&oci->ComID, NULL,
					CLSCTX_INPROC_SERVER, &XIID_IShellIconOverlayIdentifier,
					(LPVOID *)&ocp->isoi)) ){
					// TortoiseSVN は、CoCreateInstance 後、必ず GetOverlayInfo が必要らしい
					WCHAR layname[VFPS];
					DWORD layflags;
					int layindex;

					ocp->isoi->lpVtbl->GetOverlayInfo(ocp->isoi, layname,
						VFPS, &layindex, &layflags);
					ocp->index = index;
					ocp++;
				}
			}
			ocp->index = -1;
		}
		return OverlayCom.Count;
	}

	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegOverlayName, 0, KEY_READ, &hKey) != ERROR_SUCCESS ){
		OverlayCom.Count = 0;
		return 0;
	}
	oci = HeapAlloc(hProcessHeap, 0, sizeof(OverlayComInfo));
	if ( oci == NULL ) return 0;

	for ( ; ; ){
		TCHAR keyname[MAX_PATH];
		TCHAR idname[MAX_PATH];
		CLSID hid;
		DWORD s;
		FILETIME ft;
		HRESULT hres;
		xIShellIconOverlayIdentifier *isoi;

		s = MAX_PATH;
		if ( RegEnumKeyEx(hKey, enumno++, keyname, &s, NULL, NULL, NULL, &ft)
			!= ERROR_SUCCESS ){
			break;
		}

		if ( keyname[0] == '{' ){
			tstrcpy(idname, keyname);
		} else{
			if ( GetRegString(hKey, keyname, NilStr, idname, TSIZEOF(idname))
				== FALSE ){
				continue;
			}
		}
		{
			OverlayComInfo *newoci;
			int pri;
			#ifndef UNICODE
			WCHAR idnameW[VFPS];
			#define tidname idnameW
			AnsiToUnicode(idname, idnameW, MAX_PATH);
			#else
			#define tidname idname
			#endif
			if ( FAILED(CLSIDFromString(tidname, &hid)) ) continue;
			#undef tidname

			hres = CoCreateInstance(&hid, NULL, CLSCTX_INPROC_SERVER,
				&XIID_IShellIconOverlayIdentifier, (LPVOID *)&isoi);
			if ( FAILED(hres) ) continue;

			newoci = HeapReAlloc(hProcessHeap, 0, oci, sizeof(OverlayComInfo) * (ComCount + 1));
			if ( newoci != NULL ){
				WCHAR layname[VFPS];
				DWORD layflags;
				int index;
				HICON hlayicon;

				oci = newoci;
				oci[ComCount].ComID = hid;

				isoi->lpVtbl->GetOverlayInfo(isoi, layname, VFPS, &index, &layflags);
				if ( !(layflags & ISIOI_ICONINDEX) ) index = LIDX_FILE;
				#ifndef UNICODE
				{
					char nameA[VFPS];

					UnicodeToAnsi(layname, nameA, VFPS);
					hlayicon = LoadIconDx(nameA, index, 32);
				}
				#else
				hlayicon = LoadIconDx(layname, index, 32);
				#endif
				oci[ComCount].LayIcon/*.h32*/ = hlayicon;
				if ( hlayicon != NULL ){
					isoi->lpVtbl->GetPriority(isoi, &pri);
					ComCount++;
				}
			}
			isoi->lpVtbl->Release(isoi);
		}
	}
	RegCloseKey(hKey);
	if ( OverlayCom.Count < 0 ){
		OverlayCom.List = oci;
		OverlayCom.Count = ComCount;
	} else{
		HeapFree(hProcessHeap, 0, oci);
	}
	if ( OverlayCom.Count > 0 ) LoadOverlayCom(oc);
	return OverlayCom.Count;
}

void FreeOverlayClass(OverlayClassTable *oc)
{
	OverlayClass *ocp;

	if ( oc == NULL ) return;
	ocp = oc;

	for ( ; ocp->index >= 0; ocp++ ){
		ocp->isoi->lpVtbl->Release(ocp->isoi);
	}
	HeapFree(hProcessHeap, 0, oc);
}

// オーバレイアイコンの検出と描画
// ガイドライン 16x16:10 32x32:16 48x48:24 256x256:128
// shell32 サイズ一覧:16 20 24 32  40 48 64 256
void DrawOverlayIcon(OverlayClassTable **LayPtr, HBITMAP targetbmp, const TCHAR *filename, DWORD attr, UINT iconsize, DWORD *DrawX)
{
	OverlayClass **UseLayPtr, *TempOC, *ocp;

	if ( LayPtr != NULL ){
		UseLayPtr = (OverlayClass **)LayPtr;
	} else{ // LayPtr == NULL ※１
		TempOC = NULL;
		UseLayPtr = &TempOC;
	}

	if ( LoadOverlayCom(UseLayPtr) <= 0 ) return;

	ocp = *UseLayPtr;

	#pragma warning(suppress: 6011) // LoadOverlayCom で UseLayPtr が初期化
	for ( ; ocp->index >= 0; ocp++ ){
		#ifndef UNICODE
		WCHAR idnameW[VFPS];
		AnsiToUnicode(filename, idnameW, VFPS);
		#define tfname idnameW
		#else
		#define tfname filename
		#endif

		if ( S_OK == ocp->isoi->lpVtbl->IsMemberOf(ocp->isoi, tfname, attr) ){
			HDC hDC, hMDC;
			HGDIOBJ hOldBMP;
			int isize;

			hDC = GetDC(NULL);
			hMDC = CreateCompatibleDC(hDC);
			hOldBMP = SelectObject(hMDC, targetbmp);

			// アイコン大きさに合わせてオーバレイの大きさを独自決定
			if ( iconsize < 36 ){
				if ( iconsize >= 28 ){
					isize = iconsize - iconsize / 8;
				} else{
					isize = iconsize;
				}
			} else if ( iconsize < 90 ){
				isize = iconsize - iconsize / 4;
			} else{
				isize = iconsize / 3;
			}
			DrawIconEx(hMDC, *DrawX, iconsize - isize,
				OverlayCom.List[ocp->index].LayIcon, // .h32
				isize, isize, 0, NULL, DI_NORMAL);
			SelectObject(hMDC, hOldBMP);
			DeleteDC(hMDC);
			ReleaseDC(NULL, hDC);

			*DrawX += isize / 2;
			if ( *DrawX >= iconsize ) break;
		}
		#undef tfname
	}
	#pragma warning(suppress:4701) // 必ず※１
	if ( LayPtr == NULL ) FreeOverlayClass(TempOC);
}

void DrawLinkIcon(HBITMAP targetbmp, UINT iconsize, DWORD DrawX)
{
	HDC hDC, hMDC;
	HGDIOBJ hOldBMP;
	int isize;
	HICON *hLinkIcon;

	// アイコン大きさに合わせてリンクの大きさを独自決定
	if ( iconsize < 38 ){
		if ( iconsize >= 12 ){
			isize = 10;
		} else{
			isize = iconsize;
		}
	} else if ( iconsize < 50 ){
		isize = iconsize / 4;
	} else if ( iconsize < 70 ){
		isize = iconsize / 5;
	} else{
		isize = iconsize / 6;
	}
	hLinkIcon = (isize <= 20) ? &LinkIcon.h16 : &LinkIcon.h32;
	if ( *hLinkIcon == NULL ){
		*hLinkIcon = LoadIconDx(StrShell32DLL, -16769,
			(isize <= 20) ? 16 : 32);
	}

	hDC = GetDC(NULL);
	hMDC = CreateCompatibleDC(hDC);
	hOldBMP = SelectObject(hMDC, targetbmp);
	DrawIconEx(hMDC, DrawX, iconsize - isize, *hLinkIcon, isize, isize, 0, NULL, DI_NORMAL);
	SelectObject(hMDC, hOldBMP);
	DeleteDC(hMDC);
	ReleaseDC(NULL, hDC);
}

HICON LoadFileIcon(const TCHAR *filename, DWORD attr, DWORD flags, UINT iconsize, OverlayClassTable **LayPtr)
{
	HICON hIcon;
	LPITEMIDLIST pidl;
	xIShellItem *isi;
	TCHAR namebuf[VFPS];

	if ( OSver.dwMajorVersion < 6 ){ // XP以前用
		SHFILEINFO shfinfo;
		int challenge = RETRY_ICON_CHALLENGE;
/* 現在未使用
		if ( flags & SHGFI_USEFILEATTRIBUTES ){
			attr = HIWORD(flags);
			flags &= 0xffff;
		}
*/
		if ( (iconsize >= 14) && (iconsize < 18) ){
			setflag(flags, SHGFI_SMALLICON);
		}
		for ( ;;){
			#pragma warning(suppress: 6001) // SHGFI_ATTR_SPECIFIED がなければ[in]ではない
			if ( 0 == SHGetFileInfo(filename, attr, &shfinfo, sizeof(shfinfo), flags) ){
				return NULL;
			}
			if ( shfinfo.hIcon != NULL ) break;
			// 成功してもアイコンがないときがある
			// →時間をおいて再度取得してみる
			if ( --challenge == 0 ) break;
			Sleep(RETRY_ICON_WAIT);
		}
		return shfinfo.hIcon;
	}
	if ( !(flags & SHGFI_PIDL) ){
//		isi = GetPathInterface(NULL, filename, &XIID_IShellItem, NULL);
//		XMessage(NULL, NULL, XM_DbgLOG, T("%d"), isi);
		if ( (pidl = PathToPidl(filename)) == NULL ) return NULL;
	} else{
		pidl = (ITEMIDLIST *)filename;
		if ( FALSE == SHGetPathFromIDList(pidl, namebuf) ) namebuf[0] = '\0';
		filename = namebuf;
	}
	hIcon = NULL;
	if ( DSHCreateItemFromIDList == NULL ){
		GETDLLPROC(GetModuleHandle(StrShell32DLL), SHCreateItemFromIDList);
		if ( DSHCreateItemFromIDList == NULL ) return NULL;
	}
	if ( DSHCreateItemFromIDList(pidl, &XIID_IShellItem, (void**)&isi) == S_OK ){
		xIShellItemImageFactory *ShellItemImage;

		if ( SUCCEEDED(isi->lpVtbl->QueryInterface(isi, &XIID_IShellItemImageFactory, (void**)&ShellItemImage)) ){
			ICONINFO iinfo;
			SIZE imgsize;
			int challenge;

			imgsize.cx = imgsize.cy = iconsize;
			for ( challenge = RETRY_ICON_CHALLENGE; challenge; challenge-- ){
				HRESULT hr;

				// Vista では、ここで dos exe ファイルの読み込み中に例外が発生することがある(PrivateExtractIcons RtlImageNtHeaderEx内)
				hr = ShellItemImage->lpVtbl->GetImage(ShellItemImage,
					imgsize, xSIIGBF_RESIZETOFIT | xSIIGBF_ICONONLY,
					&iinfo.hbmColor);
				if ( SUCCEEDED(hr) ){
					DWORD DrawX = 0;
/*
			BITMAP bmpobj;
			GetObject(iinfo.hbmColor, sizeof(BITMAP), &bmpobj);
			XMessage(NULL, NULL, XM_DbgLOG, T("> %s %d"), filename, bmpobj.bmWidth);
*/
					// オーバレイアイコンの検出と描画
					if ( flags & SHGFI_ADDOVERLAYS ){
						DrawOverlayIcon(LayPtr, iinfo.hbmColor, filename, attr, iconsize, &DrawX);
					}

					// ショートカット矢印の描画
					if ( (DrawX < iconsize) &&
						((attr & FILE_ATTRIBUTE_REPARSE_POINT) ||
							!(tstricmp(filename + FindExtSeparator(filename), T(".lnk")))) ){
						DrawLinkIcon(iinfo.hbmColor, iconsize, DrawX);

					}

					// アイコン生成
					iinfo.fIcon = TRUE;
					iinfo.xHotspot = iinfo.yHotspot = 0;
					iinfo.hbmMask = CreateBitmap(iconsize, iconsize, 1, 1, NULL); // 32bit bmp なので、適当なマスクで問題ない
					hIcon = CreateIconIndirect(&iinfo);
					DeleteObject(iinfo.hbmMask);
					DeleteObject(iinfo.hbmColor);
					break;
				} else if ( hr == E_PENDING ){ // 他のスレッドで作業中
					Sleep(RETRY_ICON_WAIT);
					continue;
				}
			}
			ShellItemImage->lpVtbl->Release(ShellItemImage);
		}
		isi->lpVtbl->Release(isi);
	}
	if ( !(flags & SHGFI_PIDL) ) FreePIDL(pidl);
	return hIcon;
}

HICON LoadDefaultDirIcon(PPC_APPINFO *cinfo, int iconsize)
{
	const TCHAR *name;
	HICON *hUseIcon, hDirIcon;

	hUseIcon = SelectIconHandleAnySizeMacro(&DirIcon, iconsize);
	if ( *hUseIcon != NULL ) return *hUseIcon;

	if ( X_dicn[0] != '\0' ){
		name = X_dicn;
		hDirIcon = LoadIconDx(name, LIDX_FILE, iconsize);

		if ( hDirIcon == NULL ){
			BOOL section = FALSE;

			if ( (cinfo->dset.infoicon == DSETI_OVLSINGLE) ||
				(cinfo->dset.cellicon == DSETI_OVLSINGLE) ){
				EnterCriticalSection(&SHGetFileInfoSection);
				section = TRUE;
			}
			hDirIcon = LoadFileIcon(name, 0, SHGFI_ICON, iconsize, NULL);
			if ( IsTrue(section) ) LeaveCriticalSection(&SHGetFileInfoSection);
		}
	} else{
		hDirIcon = GetIconByExt(StrRegFolder, iconsize);
	}
	if ( hDirIcon == NULL ){
		hDirIcon = LoadIconDx(StrShell32DLL, -4, iconsize);
	}
	*hUseIcon = hDirIcon;
	return hDirIcon;
}


HICON LoadUnknownIcon(PPC_APPINFO *cinfo, int iconsize)
{
	TCHAR path[VFPS];
	HICON *hUseIcon, hGetIcon;

	if ( iconsize == 0 ) iconsize = 32;
	hUseIcon = SelectIconHandleAnySizeMacro(&UnknownIcon, iconsize);
	if ( *hUseIcon != NULL ) return *hUseIcon;

	path[0] = '\0';
	GetCustData(T("X_uicn"), path, sizeof(path));
	if ( path[0] != '\0' ){
		VFSFixPath(NULL, path, PPcPath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
		hGetIcon = LoadIconDx(path, LIDX_FILE, iconsize);

		if ( hGetIcon == NULL ){
			BOOL section = FALSE;

			if ( (cinfo->dset.infoicon == DSETI_OVLSINGLE) ||
				(cinfo->dset.cellicon == DSETI_OVLSINGLE) ){
				EnterCriticalSection(&SHGetFileInfoSection);
				section = TRUE;
			}
			hGetIcon = LoadFileIcon(path, 0, SHGFI_ICON, iconsize, NULL);
			if ( IsTrue(section) ) LeaveCriticalSection(&SHGetFileInfoSection);
		}
	} else{
		hGetIcon = NULL;
	}
	if ( hGetIcon == NULL ){
		iconsize = SelectIconSizeMacro(iconsize);
		hGetIcon = LoadImage(hInst, MAKEINTRESOURCE(Ic_UNKNOWN), IMAGE_ICON, iconsize, iconsize, LR_DEFAULTCOLOR);
	}
	*hUseIcon = hGetIcon;
	return hGetIcon;
}

HICON LoadIconFromIcol(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, int iconsize)
{
	int index = 0, fnresult;
	TCHAR keyword[VFPS];
	TCHAR buf[VFPS];
	const TCHAR *ext, *filename;
	FN_REGEXP fn;

	filename = celltmp->f.cFileName;
	ext = filename + celltmp->ext;
	if ( *ext == '.' ) ext++;

	while( EnumCustTable(index, T("X_icnl"), keyword, buf, sizeof(buf)) >= 0 ){
		index++;
		if ( keyword[0] != '/' ){
			if ( tstricmp(keyword, ext) != 0 ) continue;
		}else{
			MakeFN_REGEXP(&fn, keyword + 1);
			fnresult = FinddataRegularExpression(&celltmp->f, &fn);
			FreeFN_REGEXP(&fn);
			if ( fnresult == 0 ) continue;
		}
		VFSFullPath(NULL, buf, cinfo->RealPath);
		return LoadFileIcon(buf, 0, SHGFI_ICON, iconsize, NULL);
	}
	return INVALID_HANDLE_VALUE;
}

// １このアイコン取得   ※celltmp は予めコピーしてスレッドセーフにしておくこと
HICON LoadIconOne(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, int iconsize, int mode, OverlayClassTable **LayPtr)
{
	TCHAR buf[VFPS];

	// 独自アイコンを使用するときの処理
	if ( IsTrue(Use_X_icnl) ){
		HICON hFileIcon;

		hFileIcon = LoadIconFromIcol(cinfo, celltmp, iconsize);
		if ( hFileIcon != INVALID_HANDLE_VALUE ) return hFileIcon;
	}
	// ディレクトリに独自アイコンを使用するときの処理
	if ( X_dicn[0] && (celltmp->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
		TCHAR *p;
		char *image = NULL;

		VFSFullPath(buf, celltmp->f.cFileName, cinfo->RealPath);
		p = buf + tstrlen(buf);
		// DSETI_NORMAL 以上では、フォルダカスタマイズの反映を有効にする
		if ( (mode >= DSETI_NORMAL) && (cinfo->e.Dtype.mode != VFSDT_SHN) ){
			wsprintf(p, T("\\desktop.ini"));
			if ( NO_ERROR == LoadFileImage(buf, 4, &image, NULL, NULL) ){
				if ( strstr(image, "IconFile=") ) p = NULL; // カスタマイズ有り
				HeapFree(hProcessHeap, 0, image);
			}
		}
		if ( p != NULL ){
			HICON hUseIcon;

			hUseIcon = LoadDefaultDirIcon(cinfo, iconsize);
			if ( hUseIcon != NULL ) return CopyIcon(hUseIcon);
		}
	}
	// ファイルに応じたアイコンを取得する処理
	if ( mode != DSETI_EXTONLY ){
		HICON fileicon;
		BOOL section = FALSE;

		if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
			XLPEXTRACTICON pEI;
			TCHAR iconname[VFPS];

			if ( celltmp->state < ECS_NORMAL ) return NULL;

			VFSFullPath(iconname, celltmp->f.cFileName, cinfo->path);
			if ( VFSGetRealPath(cinfo->info.hWnd, iconname, iconname) == FALSE ){
				iconname[0] = '\0';
			}
			if ( iconname[0] == '\0' ){
				pEI = GetPathInterface(cinfo->info.hWnd, celltmp->f.cFileName, &IID_IExtractIcon, cinfo->path);
			} else{
				pEI = NULL;
			}

			if ( pEI != NULL ){
				HICON hIcon = NULL;
				int index;
				UINT flag;

				if ( S_OK != pEI->lpVtbl->GetIconLocation(
					pEI, 0, iconname, MAX_PATH, &index, &flag) ){
					flag = 0;
				}
				if ( flag & GIL_NOTFILENAME ){
					HICON hSmallIcon;

					if ( iconsize != 16 ) iconsize = 32;

					if ( SUCCEEDED(pEI->lpVtbl->Extract(pEI, iconname, index,
						&hIcon, &hSmallIcon, iconsize)) ){
						DestroyIcon(hSmallIcon);
					}
				} else{
					hIcon = LoadIconDx(iconname, index, iconsize);
				}
				pEI->lpVtbl->Release(pEI);
				if ( hIcon != NULL ) return hIcon;
			} else{ // LoadFileIcon で取得する
				if ( iconname[0] != '\0' ){
					if ( (cinfo->dset.infoicon == DSETI_OVLSINGLE) ||
						(cinfo->dset.cellicon == DSETI_OVLSINGLE) ){
						EnterCriticalSection(&SHGetFileInfoSection);
						section = TRUE;
					}

					fileicon = LoadFileIcon(iconname,
						celltmp->f.dwFileAttributes,
						(mode >= DSETI_OVL) ?
						(SHGFI_ICON | SHGFI_ADDOVERLAYS) : SHGFI_ICON,
						iconsize, LayPtr);

					if ( IsTrue(section) ){
						LeaveCriticalSection(&SHGetFileInfoSection);
					}
					if ( fileicon != NULL ) return fileicon;
				} else{
					LPITEMIDLIST pidl;
					LPSHELLFOLDER pSF;

					VFSFullPath(buf, celltmp->f.cFileName, cinfo->path);
					pSF = VFPtoIShell(NULL, buf, &pidl);

					if ( pSF != NULL ){
						pSF->lpVtbl->Release(pSF);
						if ( (cinfo->dset.infoicon == DSETI_OVLSINGLE) ||
							(cinfo->dset.cellicon == DSETI_OVLSINGLE) ){
							EnterCriticalSection(&SHGetFileInfoSection);
							section = TRUE;
						}

						fileicon = LoadFileIcon((const TCHAR *)pidl,
							celltmp->f.dwFileAttributes,
							(mode >= DSETI_OVL) ?
							(SHGFI_PIDL | SHGFI_ICON | SHGFI_ADDOVERLAYS) :
							(SHGFI_PIDL | SHGFI_ICON), iconsize, LayPtr);

						if ( IsTrue(section) ){
							LeaveCriticalSection(&SHGetFileInfoSection);
						}
						FreePIDL(pidl);
						if ( fileicon != NULL ) return fileicon;
					}
				}
			}
		} else if ( (cinfo->e.Dtype.mode == VFSDT_PATH) ||
			(cinfo->e.Dtype.mode == VFSDT_DLIST) ){
			if ( (celltmp->state >= ECS_NORMAL) && (cinfo->RealPath[0] != '?') ){
				buf[MAX_PATH - 1] = '\0';
				if ( celltmp->attr & ECA_THIS ){
					tstrcpy(buf, cinfo->path);
					*VFSFindLastEntry(buf) = '\0';
				} else{
					if ( VFSFullPath(buf, celltmp->f.cFileName,
						cinfo->RealPath) == NULL ){
						return NULL;
					}
				}
				if ( (buf[MAX_PATH - 1] == '\0') &&
					((buf[0] != '\\') || (FindPathSeparator(buf + 2) != NULL)) ){
					if ( (cinfo->dset.infoicon == DSETI_OVLSINGLE) ||
						(cinfo->dset.cellicon == DSETI_OVLSINGLE) ){
						EnterCriticalSection(&SHGetFileInfoSection);
						section = TRUE;
					}
					fileicon = LoadFileIcon(buf, celltmp->f.dwFileAttributes,
						(mode >= DSETI_OVL) ? (SHGFI_ICON | SHGFI_ADDOVERLAYS) : SHGFI_ICON, iconsize, LayPtr);
					if ( IsTrue(section) ){
						LeaveCriticalSection(&SHGetFileInfoSection);
					}
					if ( fileicon != NULL ) return fileicon;
				}
			}
		}
	}
	// アイコン簡易取得
	if ( celltmp->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		HICON hUseIcon;

		hUseIcon = LoadDefaultDirIcon(cinfo, iconsize);
		if ( hUseIcon != NULL ) return CopyIcon(hUseIcon);
	}
	// 拡張子に応じたアイコンを取得する処理
	return GetIconByExt(celltmp->f.cFileName + celltmp->ext, iconsize);
}
// セルのアイコンが拡張子で共通に使われるアイコンかを調べ、そうであれば全てのファイルに反映
BOOL CheckStaticIcon(PPC_APPINFO *cinfo, const TCHAR *ext, int iconid, int iconmode)
{
	TCHAR buf[VFPS];
	TCHAR *p;
	int i;

	if ( iconmode >= DSETI_OVLNOC ) return FALSE;
	if ( *ext == '\0' ) return FALSE; // 拡張子なし→dynamic
										// 拡張子からキーを求める -------------
	if ( IsTrue(GetRegString(HKEY_CLASSES_ROOT, ext, NilStr, buf, TSIZEOF(buf))) ){
		p = buf + tstrlen(buf);
										// アプリケーションのシェル -----------
		tstrcpy(p, T("\\shellex\\IconHandler"));
		if ( GetRegString(HKEY_CLASSES_ROOT, buf, NilStr, buf, TSIZEOF(buf)) ){
			return FALSE;	// アイコンハンドラ有り→dynamic
		}
		tstrcpy(p, T("\\DefaultIcon"));
		if ( GetRegString(HKEY_CLASSES_ROOT, buf, NilStr, buf, TSIZEOF(buf)) ){
			if ( (buf[0] == '%') || (buf[1] == '%') ) return FALSE; // % 指定→dynamic
		}
	}
	EnterCellEdit(cinfo);
	if ( !TinyCheckCellEdit(cinfo) ) for ( i = 0; i < cinfo->e.cellIMax; i++ ){
		ENTRYCELL *cell;

		cell = &CEL(i);
		if ( cell->icon != ICONLIST_NOINDEX ) continue;
		if ( tstricmp(cell->f.cFileName + cell->ext, ext) == 0 ){
			cell->icon = iconid;
		}
	}
	LeaveCellEdit(cinfo);

	InvalidateRect(cinfo->info.hWnd, &cinfo->BoxEntries, FALSE);
	return TRUE;
}

void CreateIconList(PPC_APPINFO *cinfo, ICONCACHESTRUCT *icons, int width, int height)
{
	HDC hDC;
	UINT flags;

	hDC = GetDC(cinfo->info.hWnd);
	flags = GetDeviceCaps(hDC, BITSPIXEL);
	ReleaseDC(cinfo->info.hWnd, hDC);
	if ( (cinfo->bg.X_WallpaperType != 0) || (C_eInfo[ECS_EVEN] != C_AUTO) ){
		icons->maskmode = ICONLIST_MASK;
		setflag(flags, ILC_MASK);
	} else{
		icons->maskmode = ICONLIST_NOMASK;
		if ( OSver.dwMajorVersion < 6 ){
			if ( flags == 32 ) flags = 24; // メモリを節約する
		}
	}
	icons->hImage = DImageList_Create(width, height, flags, 128, 128);
	DImageList_SetBkColor(icons->hImage,
		(flags & ILC_MASK) ? CLR_NONE : cinfo->BackColor);
	icons->minID = icons->minIDindex = 0;
	icons->maxID = icons->nextfixID = ICONCACHEINDEX_FIRSTMAX;

	icons->size = GetSystemMetrics(SM_CXFULLSCREEN) * GetSystemMetrics(SM_CYFULLSCREEN) * READCACHEPAGES;
	if ( icons->size < PPCWINDOWICONCACHEMAX ){
		icons->size = PPCWINDOWICONCACHEMAX;
	}
	icons->size = (icons->size / (width * height)) + 1;
	icons->writeIndex = 0;
	icons->alloc = TRUE;
	#ifdef USEDIRECTX
	icons->width = width;
	icons->height = height;
	#endif
}

int USEFASTCALL FindCellFormatImagePosition(BYTE *fmt)
{
	BYTE code;

	code = *fmt;
	if ( code == DE_IMAGE ) return 0;
	if ( (code != DE_WIDEV) && (code != DE_WIDEW) ) return -1;
	return (*(fmt + 4) == DE_IMAGE) ? 4 : -1;
}

//-----------------------------------------------------------------------------
// cell のアイコン / 縮小画像を取得する
void USEFASTCALL GetCellIcon(PPC_APPINFO *cinfo, SubThreadData *threaddata)
{
	int cnt, i, maxIndex, iconmode;
	DWORD useLoadCounter;
	ENTRYCELL celltmp;

	if ( cinfo->EntryIcons.hImage == INVALID_HANDLE_VALUE ){ //アイコン保存場所の準備
		if ( CacheIconList.top == MAX32 ) ThInit(&CacheIconList);
		if ( CacheIcon.hImage == NULL ){
			CreateIconList(cinfo, &CacheIcon, CacheIconsX, CacheIconsY);
		}
	}
	EnterCellEdit(cinfo);
	if ( TinyCheckCellEdit(cinfo) ){
		LeaveCellEdit(cinfo);
		return;
	}

	useLoadCounter = cinfo->LoadCounter;
	iconmode = cinfo->dset.cellicon;
	if ( iconmode == DSETI_NOSPACE ){ // 情報行の設定を流用
		iconmode = (int)cinfo->dset.infoicon;
		if ( iconmode == DSETI_NOSPACE ){ // 詰める設定の時は、表示にする
			iconmode = DSETI_OVL;
		}
	}

	cnt = 10;	// １回で読み込むアイコン数
	maxIndex = cinfo->cellWMin + cinfo->cel.Area.cx * cinfo->cel.Area.cy * 2;
	if ( maxIndex > cinfo->e.cellIMax ) maxIndex = cinfo->e.cellIMax;
	i = cinfo->cellWMin;

	if ( iconmode < DSETI_EXTONLY ){	// アイコン表示しない
		for ( ; i < maxIndex ; i++ ){
			if ( CEL(i).icon != ICONLIST_NOINDEX ) continue;
			CEL(i).icon = ICONLIST_UNKNOWN;
			DIRECTXDEFINE(CEL(i).iconcache = 0);
			RefleshCell(cinfo, i);
		}
	} else for ( ; i < maxIndex ; i++ ){
		int offset;

		if ( CEL(i).icon != ICONLIST_NOINDEX ) continue;
		celltmp = CEL(i);
		LeaveCellEdit(cinfo);
		threaddata->threeadinfo.lParam = (LPARAM)celltmp.f.cFileName;
		setflag(threaddata->threeadinfo.threadinfo.flag, XTHREAD_RESTARTREQUEST);

		if ( 0 <= (offset = FindCellFormatImagePosition(cinfo->celF.fmt)) ){
			LoadCellImage(cinfo, &celltmp, cinfo->celF.fmt + offset, NULL);
		} else if ( celltmp.icon == ICONLIST_NOINDEX ){
			if ( cinfo->EntryIcons.hImage == INVALID_HANDLE_VALUE ){
				// キャッシュに該当したときは余分にアイコン読み込み
				if ( IsTrue(SearchCacheIcon(cinfo, &celltmp, iconmode, &threaddata->LayPtr)) ) cnt++;

			} else{ // キャッシュを使わない(拡大縮小中/画像)
				HICON hIcon;

				if ( cinfo->e.Dtype.ExtData == INVALID_HANDLE_VALUE ){
					EnterCellEdit(cinfo);
					break;
				}
				hIcon = LoadIconOne(cinfo, &celltmp, cinfo->EntryIconGetSize, iconmode, &threaddata->LayPtr);
				if ( hIcon == NULL ){
					celltmp.icon = ICONLIST_LOADERROR;
				} else{
					EnterCellEdit(cinfo);
					celltmp.icon = AddIconList(&cinfo->EntryIcons, hIcon);
					LeaveCellEdit(cinfo);
					DestroyIcon(hIcon);
					if ( celltmp.icon == ICONLIST_NOINDEX ){
						celltmp.icon = ICONLIST_LOADERROR;
					}
				}
				CheckStaticIcon(cinfo, celltmp.f.cFileName + celltmp.ext,
					celltmp.icon, iconmode);
			}
		}

		resetflag(threaddata->threeadinfo.threadinfo.flag, XTHREAD_RESTARTREQUEST);
		EnterCellEdit(cinfo);
		if ( TinyCheckCellEdit(cinfo) ) break;
		if ( i >= cinfo->e.cellIMax ) break;
		if ( useLoadCounter != cinfo->LoadCounter ) break;

		if ( celltmp.icon != ICONLIST_NOINDEX ){
			CEL(i).icon = celltmp.icon;
			DIRECTXDEFINE(CEL(i).iconcache = 0);
			RefleshCell(cinfo, i);
		}
		cnt--;
		if ( cnt == 0 ){
			setflag(cinfo->SubTCmdFlags, SUBT_GETCELLICON);
			SetEvent(cinfo->SubT_cmd);
			break;
		}
	}
	LeaveCellEdit(cinfo);
}

// １この縮小画像を取得
BOOL USEFASTCALL LoadCellImage(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, BYTE *fmt, const TCHAR *UseFile)
{
	HBITMAP hBMP, hMaskBMP;
	HGDIOBJ hOldBMP;
	HDC hDC, hMDC;
	int olds;
	DWORD imgsize;
	int bmpWidth, bmpHeight;
	int drawX = -1, drawY, drawWidth, drawHeight;
	int iconX, iconY;
	BITMAPINFO bmpinfo;
	BOOL savecache = FALSE;
	int iconsize;

	int useimage = 1; // 画像を表示する(1:表示&bmp有り、0:なし、-1:表示&bmpなし)
	RECT box;

	// 画像関連
	TCHAR name[VFPS];
	HTBMP hTbmp;
	LPVOID lpBits;
	int bright = 100;
	int seplen;
	DWORD sizeL = 0, sizeH;

	// アイコン
	HICON hIcon = NULL;
	BOOL IconDestroy = TRUE;

	bmpWidth = cinfo->fontX * *(fmt + 1);
	bmpHeight = cinfo->fontY * *(fmt + 2);
	if ( (*(fmt + 3) == DE_END) || (*(fmt + 3) == DE_BLANK) ){
		bmpHeight -= cinfo->fontY;
	}
	if ( (bmpWidth <= XC_ocig[OCIG_THUMBAPISIZE]) &&
		(bmpHeight <= XC_ocig[OCIG_THUMBAPISIZE]) ){
		bright = BMPFIX_PREVIEW;
	}

	if ( UseFile == NULL ){
		imgsize = GetCustXDword(T("X_wsiz"), NULL, IMAGESIZELIMIT);
		if ( (celltmp->state < ECS_NORMAL) ||
			(celltmp->attr & (ECA_THIS | ECA_PARENT)) || // ファイル以外
			(celltmp->f.nFileSizeHigh || (celltmp->f.nFileSizeLow > imgsize)) ){ // サイズ制限
			useimage = 0;
		}
		if ( VFSFullPath(name, celltmp->f.cFileName, cinfo->RealPath) == NULL ){
			useimage = 0;	// パス異常
		}

		if ( useimage != 0 ){
			if ( (cinfo->e.Dtype.mode == VFSDT_SHN) && (GetFileAttributes(name) == BADATTR) ){
				VFSGetRealPath(cinfo->info.hWnd, name, name);
			}
			seplen = tstrlen32(name);
			tstrcpy(name + seplen, EntryImageThumbName); // キャッシュパスを生成

			// 書庫内指定
			if ( (cinfo->e.Dtype.mode == VFSDT_UN) ||
				(cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
				(cinfo->e.Dtype.mode == VFSDT_CABFOLDER) ||
				(cinfo->e.Dtype.mode == VFSDT_LZHFOLDER) ||
				(cinfo->e.Dtype.mode == VFSDT_ZIPFOLDER) ){
				HANDLE hMap;
				BYTE *mem;

				// 設定が無効 || ディレクトリ || サイズが0 なら読み込みしない
				if ( (XC_ocig[OCIG_INARCHIVE] == 0) ||
					(celltmp->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
					(celltmp->f.nFileSizeLow == 0) ){
					useimage = 0;
				} else{
					TCHAR *pathlast;

					seplen = tstrlen32(cinfo->RealPath);
					pathlast = name + seplen;
					if ( *pathlast == '\\' ){
						TCHAR *p;

						*pathlast = ':';
						for ( ;;){
							p = VFSFindLastEntry(pathlast + 1);
							if ( *p != '\\' ) break;
							*p = '_';
						}
						*tstrchr(pathlast + 1, ':') = '_';
					}
					// キャッシュを読み込む
					if ( LoadBMP(&hTbmp, name, bright) == FALSE ){
					// キャッシュ無し…書庫内のファイルを読み込む
						HANDLE hFile = CreateFileL(cinfo->RealPath, GENERIC_READ,
							FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
							OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if ( hFile == INVALID_HANDLE_VALUE ){
							celltmp->icon = ICONLIST_LOADERROR;
							return FALSE;
						}
						if ( NO_ERROR == VFSGetArchivefileImage(
							cinfo->info.hWnd, hFile, cinfo->RealPath,
							celltmp->f.cFileName, &sizeL, &sizeH, &hMap, &mem) ){
							hTbmp.dibfile = HeapAlloc(hProcessHeap, 0, sizeL);
							memcpy(hTbmp.dibfile, mem, sizeL);
							GlobalUnlock(hMap);
							GlobalFree(hMap);

							hTbmp.hPalette = INVALID_HANDLE_VALUE;
							hTbmp.info = NULL;
							hTbmp.bm = NULL;

							if ( IsTrue(InitBMP(&hTbmp, celltmp->f.cFileName, sizeL, bright)) ){
								sizeL = 0;
								if ( XC_ocig[OCIG_SAVEANS] ){ // 保存する？
									savecache = TRUE;
								}
							} else{
								useimage = 0;
							}
						} else{ // 失敗
							sizeL = 0;
							useimage = 0;
						}
						CloseHandle(hFile);
					}
				}
				// キャッシュを読み込む
			} else if ( LoadBMP(&hTbmp, name, bright) == FALSE ){
				name[seplen] = '\0';
				// 通常読込
				// Susie 優先？(そうでなければスキップ)
				if ( !((XC_ocig[OCIG_EXPLORERMODE] == 2) || (XC_ocig[OCIG_WITHICON] == 4)) || (LoadBMP(&hTbmp, name, bright) == FALSE) ){
					// Explorer互換？ (使わないならスキップ)
					if ( !((XC_ocig[OCIG_EXPLORERMODE] >= 1) || (XC_ocig[OCIG_WITHICON] >= 3)) || (GetExplorerThumbnail(name, &hTbmp, max(bmpWidth, bmpHeight)) == FALSE) ){
						// Susie 優先でない？(優先ならスキップ)
						if ( (XC_ocig[OCIG_EXPLORERMODE] == 2) || (XC_ocig[OCIG_WITHICON] == 4) || (LoadBMP(&hTbmp, name, bright) == FALSE) ){
							useimage = 0; // 読み込みできない
						}
					}
				}
				if ( IsTrue(useimage) && XC_ocig[OCIG_SAVEANS] && !(celltmp->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){ // 保存する？
					savecache = TRUE;
				}
			}
		}
	} else{ // ファイル名指定読み込み
		if ( LoadBMP(&hTbmp, UseFile, bright) == FALSE ){
			celltmp->icon = ICONLIST_LOADERROR;
			return FALSE;
		}
	}

	hDC = GetDC(cinfo->info.hWnd);
	hMDC = CreateCompatibleDC(hDC);
	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.bmiHeader.biWidth = bmpWidth;
	bmpinfo.bmiHeader.biHeight = bmpHeight;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = (WORD)((OSver.dwMajorVersion < 6) ? 24 : 32);
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;
	hBMP = CreateDIBSection(hDC, &bmpinfo, DIB_RGB_COLORS, &lpBits, NULL, 0);
	hMaskBMP = CreateBitmap(bmpWidth, bmpHeight, 1, 1, NULL);

	hOldBMP = SelectObject(hMDC, hBMP);
	olds = SetStretchBltMode(hMDC, HALFTONE);

	// 画像作成
	if ( IsTrue(useimage) && (hTbmp.size.cx > 0) && (hTbmp.size.cy > 0) && (hTbmp.bits != NULL) && (lpBits != NULL) ){
		int rate;
		int srcX = 0, srcY = 0;

		// 32bit 画像で、左上と中央の alpha が異なるなら、透明処理を行う
		if ( (hTbmp.DIB->biBitCount == 32) && (hTbmp.size.cy > 2) &&
			(*(hTbmp.bits + 3) != *(hTbmp.bits + (hTbmp.size.cx *
			(hTbmp.size.cy + 1) / 2 * sizeof(DWORD)) + 3)) ){
			int size = hTbmp.size.cx * hTbmp.size.cy;
			BYTE *alphaptr = hTbmp.bits + 3;
			COLORREF fillcolor = C_back;

			fillcolor = ((fillcolor >> 16) & 0xff) | (fillcolor & 0xff00) | ((fillcolor << 16) & 0xff0000);
			for ( ; size; size--, alphaptr += sizeof(DWORD) ){
				if ( *alphaptr < 0xa0 ){
					*(COLORREF *)(BYTE *)(alphaptr - 3) = fillcolor;
				}
			}
		}

		// 画像の書き込み
		drawWidth = bmpWidth;
		drawHeight = bmpHeight;
		drawX = drawY = 0;

		rate = XC_ocig[OCIG_SCALEMODE];
		if ( rate >= 2 ){
			rate = (hTbmp.size.cx >= (hTbmp.size.cy * rate)) ||
				((hTbmp.size.cx * rate) <= hTbmp.size.cy);
		}

		if ( rate ){ // 一部分のみ表示有効
			if ( drawWidth > hTbmp.size.cx ){ // 幅が枠より小さい
				drawWidth = hTbmp.size.cx;
				if ( drawHeight >= hTbmp.size.cy ){ // 高さが枠より小さい
					drawHeight = hTbmp.size.cy;
				}
			} else if ( drawHeight >= hTbmp.size.cy ){ // 高さが枠より小さい
				drawWidth = hTbmp.size.cx;
				drawHeight = hTbmp.size.cy;
			// 高さを枠に合わせる
			} else if (
				(((drawWidth << 15) / hTbmp.size.cx) <
				((drawHeight << 15) / hTbmp.size.cy)) ){
				drawWidth = (hTbmp.size.cx * drawHeight) / hTbmp.size.cy;
			// 幅を枠に合わせる
			} else{
				drawHeight = (hTbmp.size.cy * drawWidth) / hTbmp.size.cx;
			}
			drawX = (bmpWidth - drawWidth) / 2;
			if ( drawX < 0 ){
				srcX = -drawX;
				hTbmp.size.cx += drawX;
				drawWidth += drawX;
				drawX = 0;
			}
			drawY = (bmpHeight - drawHeight) / 2;
			if ( drawY < 0 ){
				srcY = -drawY;
				hTbmp.size.cy += drawY;
				drawHeight += drawY;
				drawY = 0;
			}

			if ( cinfo->bg.X_WallpaperType == 0 ){
				box.left = 0;
				box.top = 0;
				box.right = bmpWidth;
				box.bottom = bmpHeight;
				FillBox(hMDC, &box, cinfo->C_BackBrush);
			}
		} else{ // 常に全体表示
			if ( (drawWidth >= hTbmp.size.cx) &&
				(drawHeight >= hTbmp.size.cy) ){ // 表示対象が枠より小さい
				drawWidth = hTbmp.size.cx;
				drawX = (bmpWidth - drawWidth) / 2;
				drawHeight = hTbmp.size.cy;
				drawY = (bmpHeight - drawHeight) / 2;
				if ( cinfo->bg.X_WallpaperType == 0 ){
					box.left = 0;
					box.top = 0;
					box.right = bmpWidth;
					box.bottom = bmpHeight;
					FillBox(hMDC, &box, cinfo->C_BackBrush);
				}
			// 高さを枠に合わせる
			} else if (
				(((drawWidth << 15) / hTbmp.size.cx) >
				((drawHeight << 15) / hTbmp.size.cy)) ){
				drawWidth = (hTbmp.size.cx * drawHeight) / hTbmp.size.cy;
				if ( bmpWidth != drawWidth ){
					drawX = (bmpWidth - drawWidth) / 2;

					if ( cinfo->bg.X_WallpaperType == 0 ){
						box.left = 0;
						box.top = 0;
						box.right = drawX;
						box.bottom = bmpHeight;
						FillBox(hMDC, &box, cinfo->C_BackBrush);
						box.left = box.right + drawWidth;
						box.right = bmpWidth;
						FillBox(hMDC, &box, cinfo->C_BackBrush);
					}
				}
			// 幅を枠に合わせる
			} else{
				drawHeight = (hTbmp.size.cy * drawWidth) / hTbmp.size.cx;
				if ( bmpHeight != drawHeight ){
					drawY = (bmpHeight - drawHeight) / 2;

					if ( cinfo->bg.X_WallpaperType == 0 ){
						box.left = 0;
						box.top = 0;
						box.right = bmpWidth;
						box.bottom = drawY;
						FillBox(hMDC, &box, cinfo->C_BackBrush);
						box.top = box.bottom + drawHeight;
						box.bottom = box.top + bmpHeight;
						FillBox(hMDC, &box, cinfo->C_BackBrush);
					}
				}
			}
		}
		#if 0
		{
			HBITMAP hTbmpDIB;
			HDC hMDC2;
			HGDIOBJ hOldOBJ2;
			BLENDFUNCTION blendFunction;
			char *bits;

			DefineWinAPI(BOOL, AlphaBlend, (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION)) = NULL;
			hMDC2 = CreateCompatibleDC(hDC);
			hTbmpDIB = CreateDIBSection(hMDC2, (BITMAPINFO *)hTbmp.DIB, DIB_RGB_COLORS, &bits, NULL, 0);
			memcpy(bits, hTbmp.bits, hTbmp.size.cx * hTbmp.size.cy * bmpinfo.bmiHeader.biBitCount / 8);
			hOldOBJ2 = SelectObject(hMDC2, hTbmpDIB);

			GETDLLPROC(GetModuleHandle(T("Msimg32")), AlphaBlend);
			blendFunction.BlendOp = AC_SRC_OVER;
			blendFunction.BlendFlags = 0;
			blendFunction.SourceConstantAlpha = 255;
			blendFunction.AlphaFormat = AC_SRC_ALPHA;
			DAlphaBlend(hMDC, drawX, drawY, drawWidth, drawHeight,
				hMDC2, srcX, srcY, hTbmp.size.cx, hTbmp.size.cy, blendFunction);
			SelectObject(hMDC2, hOldOBJ2);
			DeleteObject(hTbmpDIB);
			DeleteDC(hMDC2);
		}
		#else
		StretchDIBits(hMDC, drawX, drawY, drawWidth, drawHeight,
			srcX, srcY, hTbmp.size.cx, hTbmp.size.cy,
			hTbmp.bits, (BITMAPINFO *)hTbmp.DIB, DIB_RGB_COLORS, SRCCOPY);
		#endif
		ReleaseDC(cinfo->info.hWnd, hDC);
		SetStretchBltMode(hMDC, olds);

		if ( bmpinfo.bmiHeader.biBitCount == 32 ){ // α再設定
			BYTE *ptr;
			int x, xmax, y, ymax;

			xmax = drawWidth;
			x = drawX;
			if ( x < 0 ){
				xmax += x;
				x = 0;
			}
			if ( (x + xmax) > bmpWidth ) xmax = bmpWidth - x;
			ymax = drawHeight;
			y = drawY;
			if ( y < 0 ){
				ymax += y;
				y = 0;
			}
			if ( (y + ymax) > bmpHeight ) ymax = bmpHeight - y;
			ptr = (BYTE *)lpBits + (x + y * bmpWidth) * 4 + 3;
			for ( y = 0; y < ymax; y++ ){
				for ( x = 0; x < xmax; x++, ptr += 4 ){
					*ptr = 0xff;
				}
				ptr += (bmpWidth - xmax) * 4;
			}
		}

		if ( savecache && LoadImageSaveAPI() ){ // UseFile では使用されない
			bmpinfo.bmiHeader.biXPelsPerMeter = bmpinfo.bmiHeader.biYPelsPerMeter = 0;
			SaveCacheFile(cinfo, name, seplen, &bmpinfo, lpBits);
		}
	// テキストプレビューを生成
	} else if ( (useimage == 0) && (XC_ocig[OCIG_SHOWTEXT] != 0) ){
		TCHAR *mem, *text, *maxptr;
		DWORD memsize;

		box.left = box.top = 0;
		box.right = bmpWidth;
		box.bottom = bmpHeight;
		FillBox(hMDC, &box, cinfo->C_BackBrush);

		memsize = ((bmpWidth / cinfo->fontX) + 20) * (bmpHeight / cinfo->fontY) * sizeof(TCHAR);
		if ( memsize < 0x100 ) memsize = 0x100;
		mem = HeapAlloc(hProcessHeap, 0, memsize);
		mem[memsize / sizeof(TCHAR) - 8] = '\0';
		memsize -= 8;

		if ( (sizeL != 0) && (hTbmp.dibfile != NULL) ){
			if ( sizeL >= memsize ){
				memcpy(mem, hTbmp.dibfile, memsize);
			} else{
				memcpy(mem, hTbmp.dibfile, sizeL);
				memset(mem + sizeL, 0, memsize - sizeL);
			}
		} else{
			if ( NO_ERROR != LoadFileImage(UseFile ? UseFile : name, 0x20, (char **)&mem, &memsize, NULL) ){
				memsize = 0;
			}
		}
		if ( memsize ){
			HGDIOBJ hOldObj;
			DWORD len;

			maxptr = mem + memsize / sizeof(TCHAR);
			LoadTextImage(NULL, &mem, &text, &maxptr);
			len = maxptr - text;
			if ( (len >= (DWORD)XC_ocig[OCIG_SHOWTEXT]) &&
				((memsize / 4 / sizeof(TCHAR)) < len) ){
				SetTextColor(hMDC, C_info);
				SetBkColor(hMDC, cinfo->BackColor);
				hOldObj = SelectObject(hMDC, cinfo->hBoxFont);
				DrawText(hMDC, text, len, &box, DT_LEFT | DT_NOPREFIX | DT_TABSTOP + 0x4000);
				SelectObject(hMDC, hOldObj);
				useimage = -1;
				drawX = drawY = 0;
				drawWidth = bmpWidth;
				drawHeight = bmpHeight;
			}

			if ( bmpinfo.bmiHeader.biBitCount == 32 ){
				BYTE *ptr;
				int size = bmpinfo.bmiHeader.biWidth * bmpinfo.bmiHeader.biHeight;
				for ( ptr = (BYTE *)lpBits + 3; size ; ptr += 4, size-- ){
					*ptr = 0xdf; // 適当にαを設定
				}
			}
		}
		HeapFree(hProcessHeap, 0, mem);
	// 空欄表示
	} else if ( cinfo->bg.X_WallpaperType == 0 ){
		box.left = 0;
		box.top = 0;
		box.right = bmpWidth;
		box.bottom = bmpHeight;
		FillBox(hMDC, &box, cinfo->C_BackBrush);
	}
	if ( (sizeL != 0) && (hTbmp.dibfile != NULL) ){
		HeapFree(hProcessHeap, 0, hTbmp.dibfile);
		hTbmp.dibfile = NULL;
	}
	// アイコン表示
	if ( (XC_ocig[OCIG_WITHICON] == 1) || ((XC_ocig[OCIG_WITHICON] >= 2) && (useimage == 0)) ){
		ICONINFO iinfo;
		BITMAP bmpobj;

		// アイコンの位置・大きさを決定
		if ( useimage == 0 ){ // 画像代わりに中央配置
			if ( (bmpinfo.bmiHeader.biBitCount == 32) && (lpBits != NULL) ){
				memset(lpBits, 0, bmpinfo.bmiHeader.biWidth * bmpinfo.bmiHeader.biHeight * 4);
			}

			hIcon = LoadIconOne(cinfo, celltmp, cinfo->EntryIconGetSize, DSETI_OVL, NULL);
			if ( hIcon == NULL ){
				IconDestroy = FALSE;
				hIcon = LoadUnknownIcon(cinfo, cinfo->EntryIconGetSize);
			}

			GetIconInfo(hIcon, &iinfo);
			GetObject(iinfo.hbmColor, sizeof(BITMAP), &bmpobj);
			iconsize = bmpobj.bmWidth;
			if ( iconsize > cinfo->EntryIconGetSize ){
				iconsize = cinfo->EntryIconGetSize;
			}
			DeleteObject(iinfo.hbmMask);
			DeleteObject(iinfo.hbmColor);
			iconX = (bmpWidth - iconsize) / 2;
			iconY = (bmpHeight - iconsize) / 2;
		} else{ // 右下に配置
			iconsize = ((bmpWidth > 96) && (bmpHeight > 96)) ? 32 : 16;
			iconX = bmpWidth - iconsize;
			iconY = bmpHeight - iconsize;

			hIcon = LoadIconOne(cinfo, celltmp, iconsize, DSETI_OVL, NULL);
			if ( hIcon == NULL ){
				IconDestroy = FALSE;
				hIcon = LoadUnknownIcon(cinfo, iconsize);
			}
		}
		DrawIconEx(hMDC, iconX, iconY, hIcon, iconsize, iconsize, 0, NULL, DI_NORMAL);
	}

	// マスク作成
	SelectObject(hMDC, hMaskBMP);
	box.left = 0;
	box.top = 0;
	box.right = bmpWidth;
	box.bottom = bmpHeight;
	FillBox(hMDC, &box, GetStockObject(WHITE_BRUSH));

	if ( hIcon != NULL ){
		DrawIconEx(hMDC, iconX, iconY, hIcon, iconsize, iconsize, 0, NULL, DI_MASK);
		if ( IconDestroy ) DestroyIcon(hIcon);
	}

	if ( (useimage != 0) && (drawX >= 0) ){ // 画像領域
		box.left = drawX;
		box.top = drawY;
		box.right = drawX + drawWidth;
		box.bottom = drawY + drawHeight;
		FillBox(hMDC, &box, GetStockObject(BLACK_BRUSH));
	}
	SelectObject(hMDC, hOldBMP);
	DeleteDC(hMDC);

	EnterCellEdit(cinfo);

	if ( IsTrue(useimage) || (hIcon != NULL) ){
		celltmp->icon = AddImageIconList(&cinfo->EntryIcons, hBMP, hMaskBMP);
	} else{
		celltmp->icon = ICONLIST_LOADERROR;
	}
	LeaveCellEdit(cinfo);
	DeleteObject(hMaskBMP);
	DeleteObject(hBMP);
	if ( useimage > 0 ) FreeBMP(&hTbmp);
	return TRUE;
}

BOOL GetExplorerThumbnail(const TCHAR *filename, HTBMP *hTBmp, int thumsize)
{
	TCHAR path[VFPS], name[VFPS], *p;
	WCHAR wbuf[VFPS];
	HBITMAP hBmp;
	BOOL result = TRUE;

	if ( OSver.dwMajorVersion >= 6 ){ // Vista / 7
		LPITEMIDLIST pidl;
		xIShellItem *isi;

		#if 0
		LPSHELLFOLDER pSF;

		pSF = VFPtoIShell(NULL, filename, &pidl);
		if ( pSF == NULL ) return FALSE;
		pSF->lpVtbl->Release(pSF);
		#else
		if ( (pidl = PathToPidl(filename)) == NULL ) return FALSE;
		#endif
		if ( DSHCreateItemFromIDList == NULL ){
			GETDLLPROC(GetModuleHandle(StrShell32DLL), SHCreateItemFromIDList);
		}
		hBmp = NULL;
		if ( DSHCreateItemFromIDList(pidl, &XIID_IShellItem, (void**)&isi) == S_OK ){
			xIThumbnailCache *itc;
			xISharedBitmap *isb;
			WTS_CACHEFLAGS cacheFlags;
			WTS_THUMBNAILID thumbnailId;

			if ( SUCCEEDED(CoCreateInstance(&XCLSID_LocalThumbnailCache, NULL, CLSCTX_INPROC, &XIID_IThumbnailCache, (void**)&itc)) ){
				if ( itc->lpVtbl->GetThumbnail(itc, isi, thumsize, WTS_EXTRACT, &isb, &cacheFlags, &thumbnailId) == S_OK ){
					isb->lpVtbl->Detach(isb, &hBmp);
					isb->lpVtbl->Release(isb);
				}
				itc->lpVtbl->Release(itc);
			}
			isi->lpVtbl->Release(isi);
		}
		FreePIDL(pidl);
		if ( hBmp == NULL ) result = FALSE;
	} else{ // 2000, XP
		xIExtractImage *iei;
		DWORD priority, flag = 0;
		SIZE imgsize;

		tstrcpy(path, filename);
		p = VFSFindLastEntry(path);
		tstrcpy(name, (*p == '\\') ? p + 1 : p);
		*p = '\0';

		iei = GetPathInterface(NULL, name, &XIID_IExtractImage, path);
		if ( iei == NULL ) return FALSE;
		imgsize.cx = thumsize;
		imgsize.cy = thumsize;
		if ( FAILED(iei->lpVtbl->GetLocation(iei, wbuf, sizeof(wbuf), &priority, &imgsize, 24, &flag)) ){
			result = FALSE;
		} else if ( FAILED(iei->lpVtbl->Extract(iei, &hBmp)) ){
			result = FALSE;
		}
		iei->lpVtbl->Release(iei);
	}
	if ( result == FALSE ) return FALSE;

	{
		BITMAPINFO *bmp;
		HDC hdc;

		hdc = GetDC(NULL);

		hTBmp->dibfile = NULL;
		hTBmp->hPalette = NULL;

		hTBmp->info = LocalAlloc(LMEM_MOVEABLE, sizeof(BITMAPINFO));
		if ( hTBmp->info == NULL ) return FALSE;
		bmp = (BITMAPINFO *)LocalLock(hTBmp->info);
		hTBmp->DIB = &bmp->bmiHeader;
		bmp->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmp->bmiHeader.biBitCount = 0;
		GetDIBits(hdc, hBmp, 0, thumsize, NULL, bmp, DIB_RGB_COLORS);

		hTBmp->bm = LocalAlloc(LMEM_MOVEABLE, bmp->bmiHeader.biSizeImage);
		if ( hTBmp->bm == NULL ) return FALSE;
		hTBmp->bits = LocalLock(hTBmp->bm);

		GetDIBits(hdc, hBmp, 0, bmp->bmiHeader.biHeight, hTBmp->bits, bmp, DIB_RGB_COLORS);

		ReleaseDC(NULL, hdc);
		DeleteObject(hBmp);

		hTBmp->size.cx = bmp->bmiHeader.biWidth;
		hTBmp->size.cy = bmp->bmiHeader.biHeight;
		if ( bmp->bmiHeader.biHeight < 0 ){
			hTBmp->size.cy = -bmp->bmiHeader.biHeight; // トップダウン
		}
	}
	return TRUE;
}

void SaveCacheFile(PPC_APPINFO *cinfo, TCHAR *filename, int seplen, BITMAPINFO *bmpinfo, LPVOID lpBits)
{
	FILETIME fc, fa, fw;
	HANDLE hFile;

	// 時刻保存
	filename[seplen] = '\0';
	hFile = CreateFileL(filename, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		GetFileTime(hFile, &fc, &fa, &fw);
		CloseHandle(hFile);
	}
	// 画像保存
	filename[seplen] = ':';
	if ( ImageSaveByAPI(bmpinfo, bmpinfo->bmiHeader.biSize, lpBits,
		DwordBitSize(bmpinfo->bmiHeader.biWidth *
			bmpinfo->bmiHeader.biBitCount) * bmpinfo->bmiHeader.biHeight,
		filename) != SUSIEERROR_NOERROR ){
		SetPopMsg(cinfo, POPMSG_MSG, T("cache save error"));
	}
	// 時刻復帰
	if ( hFile != INVALID_HANDLE_VALUE ){
		filename[seplen] = '\0';
		hFile = CreateFileL(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			SetFileTime(hFile, &fc, &fa, &fw);
			CloseHandle(hFile);
		}
	}
}
