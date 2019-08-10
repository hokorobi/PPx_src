/*-----------------------------------------------------------------------------
	Paper Plane CUI								～ ドラッグ処理-データ作成 ～
-----------------------------------------------------------------------------*/
#define DEFINEDnDDROP
#include "WINAPI.H"
#include <shlobj.h>
#include <ole2.h>

#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"

#include "PPC_DD.H"
#pragma hdrstop

TCHAR * USEFASTCALL FixName(PPC_APPINFO *cinfo, ENTRYCELL *cell)
{
	TCHAR *cfilename;

	cfilename = cell->f.cFileName;
	if ( IsTrue(cinfo->UnpackFix) ){
		cfilename = FindLastEntryPoint(cfilename);
	}
	return cfilename;
}

HGLOBAL CreateHText(PPC_APPINFO *cinfo)
{
	TCHAR buf[VFPS];
	TMS_struct files = {{NULL, 0, NULL}, 0};
	ENTRYCELL *cell;
	int work, count = 0;

	TMS_reset(&files);
	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		if ( cinfo->RealPath[0] == '?' ){
			VFSFullPath(buf, FixName(cinfo, cell), cinfo->path);
		}else{
			VFSFullPath(buf, FixName(cinfo, cell), cinfo->RealPath);
		}
		tstrcat(buf, T("\r\n"));
		TMS_set(&files, buf);
		files.p -= sizeof(TCHAR);
		count++;
	}
	if ( count == 1 ){
		files.p -= TSTROFF(2);
		*((TCHAR *)files.tm.p + files.p) = '\0';
	}
	TMS_off(&files);
	return ::GlobalReAlloc(files.tm.h, files.p + sizeof(TCHAR), GMEM_MOVEABLE);
}
//-----------------------------------------------------------------------------
HGLOBAL CreateHDrop(PPC_APPINFO *cinfo)
{
	TCHAR buf[VFPS];
	TMS_struct files = {{NULL, 0, NULL}, 0};
	ENTRYCELL *cell;
	int work;
	DROPFILES *dp;

	if (cinfo->RealPath[0] == '?') return NULL;

	TMS_reset(&files);
	TM_check(&files.tm, sizeof(DROPFILES));
	dp = (DROPFILES *)((BYTE *)files.tm.p + files.p);
	dp->pFiles = sizeof(DROPFILES);
	dp->pt.x = 0;
	dp->pt.y = 0;
	dp->fNC = FALSE;
	files.p = sizeof(DROPFILES);
	InitEnumMarkCell(cinfo, &work);

#ifndef UNICODE
	if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
		dp->fWide = FALSE;	// OEM charset
		while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
			VFSFullPath(buf, FixName(cinfo, cell), cinfo->RealPath);
			TMS_set(&files, buf);
		}
		TMS_set(&files, "");
	}else
#endif
	{
		dp->fWide = TRUE;	// UNICODE

		while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
			int l;
#ifndef UNICODE
			OLECHAR olePath[VFPS];

			VFSFullPath(buf, FixName(cinfo, cell), cinfo->RealPath);
			l = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buf,-1,
					olePath,VFPS) * sizeof(WCHAR);
			if ( TM_check(&files.tm, files.p + l + sizeof(WCHAR)) == FALSE ){
				goto memerror;
			}
			memcpy((char *)files.tm.p + files.p, olePath, l);
#else
			VFSFullPath(buf, FixName(cinfo, cell), cinfo->RealPath);
			l = TSTRSIZE32(buf);
			if ( TM_check(&files.tm, files.p + l + sizeof(WCHAR)) == FALSE ){
				goto memerror;
			}
			memcpy((char *)files.tm.p + files.p, buf, l);
#endif
			files.p += l;
		}
		*(WCHAR *)((char *)files.tm.p + files.p) = '\0';
		files.p += 2;
	}

	TMS_off(&files);
	return ::GlobalReAlloc(files.tm.h, files.p, GMEM_MOVEABLE);
memerror:
	TMS_kill(&files);
	return NULL;
}
//-----------------------------------------------------------------------------
HGLOBAL CreateShellIdList(PPC_APPINFO *cinfo)
{
	HGLOBAL hGlobal;
	int cnt;
	cnt = cinfo->e.markC ? cinfo->e.markC : 1;
	unsigned int CurPosition = sizeof(CIDA) + sizeof(UINT) * cnt;
	LPMALLOC pMA;

	ENTRYCELL *cell;
	int work, j = 0;

	LPITEMIDLIST pRootPidl;
	LPSHELLFOLDER pParentFolder =
			VFPtoIShell(cinfo->info.hWnd, cinfo->path, &pRootPidl);

	if ( pParentFolder == NULL ) return NULL;
	int sizeofPidl = GetPidlSize(pRootPidl);

	DWORD TotalSize = (DWORD)(sizeof(CIDA) + sizeof(UINT) * cnt
												+ sizeofPidl * (cnt + 1) );
	hGlobal = ::GlobalAlloc(GHND | GMEM_SHARE, TotalSize);
	if ( hGlobal == NULL ) return NULL;

	LPIDA pIdA = (LPIDA)::GlobalLock(hGlobal);
	pIdA->cidl = cnt;
	pIdA->aoffset[0] = CurPosition;
	memcpy((LPBYTE)(pIdA) + CurPosition, (LPBYTE)pRootPidl, sizeofPidl);
	CurPosition += sizeofPidl;

	::SHGetMalloc(&pMA);
	pMA->Free(pRootPidl);

	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		TCHAR FileName[VFPS];
		LPITEMIDLIST pItemPidl;

		tstrcpy(FileName, FixName(cinfo, cell));
		pItemPidl = BindIShellAndFname(pParentFolder, FileName);
		pIdA->aoffset[j + 1] = CurPosition;
		sizeofPidl = GetPidlSize(pItemPidl);
		if( TotalSize < (CurPosition + sizeofPidl) ){
			::GlobalUnlock(hGlobal);
			TotalSize += (cnt - j) * sizeofPidl;
			hGlobal = ::GlobalReAlloc(hGlobal, TotalSize, GHND | GMEM_SHARE);
			if(hGlobal == NULL){
				pParentFolder->Release();
				pMA->Release();
				return NULL;
			}
			pIdA = (LPIDA)::GlobalLock(hGlobal);
		}
		memcpy((LPBYTE)(pIdA) + CurPosition, (LPBYTE)pItemPidl, sizeofPidl);
		pMA->Free(pItemPidl);
		CurPosition += sizeofPidl;
		j++;
	}
	pParentFolder->Release();
	pMA->Release();

	::GlobalUnlock(hGlobal);
	return hGlobal;
}
