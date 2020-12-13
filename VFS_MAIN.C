/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System						メイン
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <olectl.h>
#include <shlobj.h>
#include "WINOLE.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#pragma hdrstop

#define HIMETRIC_INCH 2540

#define GLOBALDEFINE	// GLOBAL 変数定義
#include "VFS_STRU.H"	// Global 定義あり

#define UNUSE0 0
#define UNUSE1 "\x47\x49\x46\x38"

// VB の OleLoadPicture を使った画像読み込み(JPEG, GIF)
// x64ではOLEPRO32.DLLがないので使えない
#ifndef _WIN64
BOOL LoadOLEmode(void *image, DWORD sizeL, TCHAR *type, HANDLE *Info, HANDLE *Bm, DWORD (*AllReadProc)(void))
{
	LPPICTURE iPicture;
	LPSTREAM stream;
	HGLOBAL hGlobal;
	int x, y;
	int x1, y1;
	HDC hdc;
	RECT box;

	HDC hMdc;
	HBITMAP hBmp;
	HGDIOBJ hOldBmp;
	BITMAPINFO *bmp;
	HRESULT ComInitResult;

	if ( !memcmp(image, "\0\0\1", 4) ||	// Icon
		 !memcmp(image, "\0\0\2", 4) ){	// cursor
		if ( *(DWORD *)((BYTE *)image + 0x12) != (DWORD)(*(WORD *)((BYTE *)image + 4) * 0x10 + 6) ){
			return FALSE;
		}
	}else if ( !memcmp(image, "BM", 2) ){	// Bitmap
		if ( ((BITMAPINFOHEADER *)((char *)image + sizeof(BITMAPFILEHEADER)))->biSize != sizeof(BITMAPCOREHEADER) ){
			return FALSE; // OS/2 形式以外は、OleLoadPictureを使わない
		}
	}else if ( memcmp(image, UNUSE1, 4) &&
		 memcmp(image, "\xff\xd8", 2)		// Jpeg
		){
		return FALSE;
	}
#if UNUSE0
	if ( !memcmp(image, UNUSE1, 4) && (NO_ERROR != GetCustData(T(UNUSE1), &x, 1)) ){
		return FALSE;
	}
#endif
	if ( DOleLoadPicture == NULL ){
		if ( hOLEPRO32 != NULL ) return FALSE;
		hOLEPRO32 = LoadLibrary(T("OLEPRO32.DLL"));
		if ( hOLEPRO32 == NULL ) return FALSE;
		DOleLoadPicture =
				(D_OLELOADPICTURE)GetProcAddress(hOLEPRO32, "OleLoadPicture");
		if ( DOleLoadPicture == NULL ) return FALSE;
	}
	if ( AllReadProc ){ // 遅延読み込み有りなら全部読む
		sizeL = AllReadProc();
		if ( sizeL == 0 ) return FALSE;
	}

	hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeL);
	if ( hGlobal == NULL ) return FALSE;
	memcpy(GlobalLock(hGlobal), image, sizeL);
	GlobalUnlock(hGlobal);

	ComInitResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if ( FAILED(CreateStreamOnHGlobal(hGlobal, TRUE, &stream)) ){
		if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
		GlobalFree(hGlobal);
		return FALSE;
	}
#ifndef WINEGCC
	if ( FAILED(DOleLoadPicture(stream,
			sizeL, FALSE, &XIID_IPicture, (LPVOID *)&iPicture)) ){
		stream->lpVtbl->Release(stream);
		if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
		GlobalFree(hGlobal);
		return FALSE;
	}
	hdc = GetDC(NULL);
	hMdc = CreateCompatibleDC(hdc);

	iPicture->lpVtbl->get_Width(iPicture, (OLE_XSIZE_HIMETRIC *)&x1);
	iPicture->lpVtbl->get_Height(iPicture, (OLE_XSIZE_HIMETRIC *)&y1);
	x = CalcMulDiv(x1, GetDeviceCaps(hdc, LOGPIXELSX), HIMETRIC_INCH);
	y = CalcMulDiv(y1, GetDeviceCaps(hdc, LOGPIXELSY), HIMETRIC_INCH);

	hBmp = CreateCompatibleBitmap(hdc, x, y);
	hOldBmp = SelectObject(hMdc, hBmp);
	box.left = box.top = 0;
	box.right = x;
	box.bottom = y;
	iPicture->lpVtbl->Render(iPicture, hMdc, 0, 0, x, y, 0, y1, x1, -y1, &box);
	SelectObject(hMdc, hOldBmp);
	DeleteDC(hMdc);

	iPicture->lpVtbl->Release(iPicture);
	stream->lpVtbl->Release(stream);
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
	GlobalFree(hGlobal);

	*Info = LocalAlloc(LMEM_MOVEABLE, sizeof(BITMAPINFO) + 256 * 4 * 3);
	if ( *Info == NULL ) return FALSE;
	bmp = LocalLock(*Info);

	bmp->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmp->bmiHeader.biBitCount = 0;
	GetDIBits(hdc, hBmp, 0, y, NULL, bmp, DIB_RGB_COLORS);

	*Bm = LocalAlloc(LMEM_MOVEABLE, bmp->bmiHeader.biSizeImage + 256 * 4 + 3);
	if ( *Bm == NULL ) return FALSE;

	GetDIBits(hdc, hBmp, 0, y, LocalLock(*Bm), bmp, DIB_RGB_COLORS);
	LocalUnlock(*Bm);
	LocalUnlock(*Info);

	ReleaseDC(NULL, hdc);
	DeleteObject(hBmp);
	if ( (DWORD_PTR)type > 0x7f ) tstrcpy(type, T("OLE mode"));
	return TRUE;
#else
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
	GlobalFree(hGlobal);
	return FALSE;
#endif
}
#endif // _WIN64

/*-----------------------------------------------------------------------------
	画像読み込みを行う
-----------------------------------------------------------------------------*/
// Susie Plug-in のダミープログレスコールバック(無いと動かないSPIがあるため)---
#pragma argsused
int FAR WINAPI SusieProgressCallback(int nNum, int nDenom, LONG_PTR lData)
{
	UnUsedParam(nNum);UnUsedParam(nDenom);UnUsedParam(lData);
	return 0;
}

// 互換用
VFSDLL BOOL PPXAPI VFSGetDib(const TCHAR *filename, void *image, DWORD sizeL, TCHAR *type, HANDLE *Info, HANDLE *Bm)
{
	return VFSGetDibDelay(filename, image, sizeL, type, Info, Bm, NULL);
}

VFSDLL int PPXAPI VFSGetDibDelay(const TCHAR *filename, void *image, DWORD sizeL, TCHAR *type, HANDLE *Info, HANDLE *Bm, DWORD (*AllReadProc)(void))
{
	THREADSTRUCT *ts;
	const TCHAR *OldTname;
	TCHAR *fp;
	SUSIE_DLL *sudll;
	int i;
	char TempFnameA[VFPS];
#ifdef UNICODE
	WCHAR TempFnameW[VFPS];
	char TempType[MAX_PATH];
	#define TFILENAME TempFnameA
	#define TTYPE     TempType
	UnicodeToAnsi(filename, TempFnameA, VFPS);
	TempFnameA[MAX_PATH - 1] = '\0'; // 長さ調整
#else
	#define TFILENAME filename
	#define TTYPE     type
#endif
	if ( susie_items && (sizeL > 4) ){
		TCHAR *SpiName;
		BYTE headerbuf[SUSIE_CHECK_SIZE], *hptr;
		BOOL usepreview = FALSE;

		EnterCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
		sudll = susie_list;
		ts = GetCurrentThreadInfo();
		if ( ts != NULL ) OldTname = ts->ThreadName;
		fp = FindLastEntryPoint(filename);
		SpiName = tstrstr(fp, T("::"));
		if ( SpiName != NULL ){
			#ifdef UNICODE
				*(strstr(TempFnameA, "::")) = '\0';
				strcpyW(TempFnameW, filename);
				TempFnameW[SpiName - filename] = '\0';
				filename = TempFnameW;
			#else
				strcpy(TempFnameA, filename);
				TempFnameA[SpiName - filename] = '\0';
				filename = TempFnameA;
			#endif
			SpiName += 2;
		}
		#ifndef UNICODE
		{
			int namelen = strlen(filename);
			if ( namelen >= MAX_PATH ){
				filename += namelen - (MAX_PATH - 1); // 長さ調整
			}
		}
		#endif

		if ( sizeL >= SUSIE_CHECK_SIZE ){
			hptr = image;
		}else{
			memcpy(headerbuf, image, sizeL);
			memset(headerbuf + sizeL, 0, SUSIE_CHECK_SIZE - sizeL);
			hptr = headerbuf;
		}
		if ( (DWORD_PTR)type == 1 ) usepreview = TRUE;

		for ( i = 0 ; i < susie_items ; i++, sudll++ ){
			int susie_result;
			if ( SpiName == NULL ){
				if ( (sudll->flags & (VFSSUSIE_BMP | VFSSUSIE_NOAUTODETECT)) !=
					VFSSUSIE_BMP ){
					continue;
				}
				if ( CheckAndLoadSusiePlugin(sudll, fp, ts, VFS_BMP) == FALSE ){
					continue;
				}
			}else{
				if ( !(sudll->flags & VFSSUSIE_BMP) ||
					tstricmp((TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset), SpiName) ){
					continue;
				}
				if ( CheckAndLoadSusiePlugin(sudll, fp, ts, VFS_BMP | VFS_FORCELOAD_PLUGIN) == FALSE ){
					continue;
				}
			}
			#ifdef UNICODE
				if ( sudll->IsSupportedW != NULL ){
					if ( sudll->IsSupportedW(filename, hptr) == 0 ) continue;
				}else{
					if ( sudll->IsSupported(TFILENAME, hptr) == 0 ) continue;
				}
			#else
				if ( sudll->IsSupported(filename, hptr) == 0 ) continue;
			#endif

			if ( AllReadProc ){ // 遅延読み込み有りなら全部読む
				sizeL = AllReadProc();
				if ( sizeL <= 1 ){
					LeaveCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
					return sizeL ? -1 : 0;
				}
				AllReadProc = NULL; // 読み込み済み
			}
			if ( (DWORD_PTR)type > 0x7f ){
				char *extptr;
				int index = 3;

				extptr = strrchr(TFILENAME, '.');
				if ( extptr != NULL ){
					#ifdef UNICODE
						strlwr(extptr); // TempFnameA を小文字化
					#endif
					for ( index = 2 ; ; index += 2 ){
						if ( sudll->GetPluginInfo(index, TTYPE, MAX_PATH) <= 0 ){
							index = 3;
							break;
						}
						TTYPE[MAX_PATH - 1] = '\0';
						#ifdef UNICODE
							strlwr(TTYPE);
						#endif
						if ( strstr(TTYPE, extptr) != NULL  ){
							index++;
							break;
						}
					}
				}
				sudll->GetPluginInfo(index, TTYPE, MAX_PATH);
				TTYPE[MAX_PATH - 1] = '\0';
				#ifdef UNICODE
					AnsiToUnicode(TTYPE, type, VFPS);
				#endif
			}
			// プレビューがあればプレビューで、プレビューでエラーなら通常で
			if ( IsTrue(usepreview) && (sudll->GetPreview != NULL) &&
				 (SUSIEERROR_NOERROR == sudll->GetPreview((LPSTR)image,
					(LONG_PTR)sizeL, SUSIE_SOURCE_MEM,
					Info, Bm, (FARPROC)SusieProgressCallback, 0)) ){
				goto success;
			}
			susie_result = sudll->GetPicture((LPSTR)image, (LONG_PTR)sizeL,
					SUSIE_SOURCE_MEM, Info, Bm, (FARPROC)SusieProgressCallback, 0);
			if ( susie_result == SUSIEERROR_NOERROR ) goto success;
			if ( (susie_result < 0) || (susie_result == SUSIEERROR_INTERNAL) ){
#ifdef UNICODE
				if ( sudll->GetPictureW != NULL ){
					if ( SUSIEERROR_NOERROR == sudll->GetPictureW(filename, 0,
							SUSIE_SOURCE_DISK, Info, Bm,
							(FARPROC)SusieProgressCallback, 0) ){
						goto success;
					}
				}else
#endif
				if ( SUSIEERROR_NOERROR == sudll->GetPicture(TFILENAME, 0,
						SUSIE_SOURCE_DISK, Info, Bm,
						(FARPROC)SusieProgressCallback, 0) ){
					goto success;
				}
			}
		}
		if ( ts != NULL ) ts->ThreadName = OldTname;
		LeaveCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
	}
	return ValueX3264(LoadOLEmode(image, sizeL, type, Info, Bm, AllReadProc), 0);
success:
	// 読み込み成功
	if ( ts != NULL ) ts->ThreadName = OldTname;
	LeaveCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
	return 1;
}

// VFS 関連で読み込んだ DLL を解放する ----------------------------------------
void USEFASTCALL CleanUpVFS(void)
{
	if ( hOLEPRO32 != NULL ){
		FreeLibrary(hOLEPRO32);
		hOLEPRO32 = NULL;
		DOleLoadPicture = NULL;
	}
	if ( hWininet != NULL ){
		FreeLibrary(hWininet);
		hWininet = NULL;
	}
	if ( hMPR != NULL ){
		FreeLibrary(hMPR);
		hMPR = NULL;
	}
}

typedef union {
	DCOMMONS c;
	FATSTRUCT fats;
	CDS cds;
} GETDISKFILEIMAGES;

int USEFASTCALL CheckFATImage(const BYTE *image, const BYTE *max)
{
	const BYTE *fat;
	DWORD off;

	if ( max < (image + 0x30) ) return CHECKFAT_NONE;
	if ( (*image != 0xeb) && (*image != 0xe9) && (*image != 0x60) ){
		return CHECKFAT_NONE;
	}
	// NTFS判別
	if ( (*(const DWORD *)(image + 3) == 0x5346544e) &&
		 (*(const DWORD *)(image + 0x24) == 0x800080) ){
		return CHECKFAT_NTFS;
	}
	// exFAT判別
	if ( (*(const DWORD *)(image + 3) == 0x41465845) &&
		 (*(const DWORD *)(image + 0xb) == 0) ){
		return CHECKFAT_EXFAT;
	}
	// FAT判別
	off = *(const WORD *)(image + 0xb + 0); // sector
	// 0000 xxx0 0000 0000 なら ok
	if ( ((off & 0xf1ff) != 0) || ((off & 0xe00) == 0) ) return CHECKFAT_NONE;

	fat = image + off * *(const WORD *)(image + 0xb + 3); // reserved
	if ( ((fat + 32) < max) && (*(const WORD *)(fat + 1) != 0xffff) ){
		return CHECKFAT_NONE;
	}
	return CHECKFAT_FAT;
}

int USEFASTCALL CheckCDImage(const BYTE *image, const BYTE *imagemax)
{
	if ( (imagemax < (image + 10)) ||
		 (memcmp(image, StrISO9660, CDHEADERSIZE) && memcmp(image, StrUDF, CDHEADERSIZE)) ){
		return 0;
	}
	return 1;
}

int USEFASTCALL CheckDVDImage(const BYTE *image, const BYTE *imagemax)
{
	if ( (imagemax < (image + 10)) || memcmp(image, "\2\0", 2) ) return 0;
	return 1;
}

typedef BOOL (*DIS_OPEN)(void *structptr, const TCHAR *imgname, DWORD offset);
typedef BOOL (*DIS_CLOSE)(void *structptr);
typedef BOOL (*DIS_FINDENTRY)(void *structptr, const TCHAR *path, WIN32_FIND_DATA *ff);
typedef DWORD (*DIS_READCLUSTER)(void *structptr, BYTE *dest, DWORD destsize);
typedef BOOL (*DIS_SETNEXTCLUSTER)(void *structptr);

typedef struct {
	const DWORD offsetlist[5];	// ディスク本体の開始位置
	int (USEFASTCALL *Check)(const BYTE *image, const BYTE *max);
	DIS_OPEN Open;
	DIS_CLOSE Close;
	DIS_FINDENTRY FindEntry;
	DIS_READCLUSTER ReadCluster;
	DIS_SETNEXTCLUSTER SetNextCluster;
} DISKIMAGESTRUCT;

DISKIMAGESTRUCT fatimagestruct = {
	{ 0, 0xa2, 0x1000, 0xffff0001, MAX32 },
	CheckFATImage,
	(DIS_OPEN)OpenFATImage,
	(DIS_CLOSE)CloseFATImage,
	(DIS_FINDENTRY)FindEntryFATImage,
	(DIS_READCLUSTER)ReadFATCluster,
	(DIS_SETNEXTCLUSTER)SetFATNextCluster
};

DISKIMAGESTRUCT cdimagestruct = {
	{ CDHEADEROFFSET1, CDHEADEROFFSET2, CDHEADEROFFSET3, CDHEADEROFFSET4, MAX32 },
	CheckCDImage,
	(DIS_OPEN)OpenCDImage,
	(DIS_CLOSE)CloseCDImage,
	(DIS_FINDENTRY)FindEntryCDImage,
	(DIS_READCLUSTER)ReadCDCluster,
	(DIS_SETNEXTCLUSTER)SetCDNextCluster
};

DISKIMAGESTRUCT dvdimagestruct = {
	{ DVDHEADEROFFSET, MAX32 },
	CheckDVDImage,
	(DIS_OPEN)OpenCDImage,
	(DIS_CLOSE)CloseCDImage,
	(DIS_FINDENTRY)FindEntryCDImage,
	(DIS_READCLUSTER)ReadCDCluster,
	(DIS_SETNEXTCLUSTER)SetCDNextCluster
};

ERRORCODE GetDiskFileImage(DISKIMAGESTRUCT *di, HWND hWnd, const TCHAR *ArchiveName, const TCHAR *EntryName, BYTE *header, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem, DWORD fsize)
{
	GETDISKFILEIMAGES dist;
	const DWORD *o;
	TCHAR readdrive[8];
	TCHAR dir[VFPS], name[MAX_PATH];
	WIN32_FIND_DATA findfile;
	DWORD size, tempo[2];
	HANDLE hFile;
	ERRORCODE result = MAX32;
	BYTE *AllocBuffer = NULL;

	if ( ArchiveName[0] == '#' ){
		wsprintf(readdrive, T("\\\\.\\%c:"), ArchiveName[1]);
		ArchiveName = readdrive;
	}else{
		readdrive[0] = '\0';
	}

	tstrcpy(dir, EntryName);
	{
		TCHAR *p;

		p = VFSFindLastEntry(dir);
		tstrcpy(name, (*p == '\\') ? p + 1 : p );
		*p = '\0';
	}
	for ( o = di->offsetlist ; *o != MAX32 ; o++ ){
		if ( *o == 0xffff0001 ){
			DWORD pc9801drive;

			if ( fsize < 0x2000 ) break;
			pc9801drive = GetPC9801FirstDriveSector(header);
			if ( pc9801drive == 0 ) break;
			tempo[0] = pc9801drive;
			tempo[1] = 0;
			o = tempo;
		}
		if ( fsize > *o ){
			if ( di->Check(header + *o, header + fsize) == 0 ) continue;
		}else{
			DWORD tsize;

			hFile = CreateFileL(ArchiveName, GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ) continue;
			SetFilePointer(hFile, *o, NULL, FILE_BEGIN);
			tsize = 0;
			ReadFile(hFile, header, 0x800, &tsize, NULL); // エラーでも tsize = 0
			CloseHandle(hFile);
			if ( di->Check(header, header + tsize) == 0 ) continue;
		}
		if ( di->Open(&dist, ArchiveName, *o) == FALSE ){
			return ERROR_NO_MORE_FILES;
		}
		if ( di->FindEntry(&dist, dir, &findfile) == FALSE ){
			di->Close(&dist);
			return ERROR_NO_MORE_FILES;
		}
		for ( ; ; ){
			if ( !tstrcmp(findfile.cFileName, name) ) break;
			if ( di->FindEntry(&dist, NULL, &findfile) == FALSE ){
				di->Close(&dist);
				return ERROR_NO_MORE_FILES;
			}
		}
		*sizeL = findfile.nFileSizeLow;
		*sizeH = findfile.nFileSizeHigh;
#if VFS_check_size < 0x8000
#error headerは32k以上
#endif
		if ( hWnd == INVALID_HANDLE_VALUE ){
			HANDLE hDstFile;
			LARGE_INTEGER TotalSize;
			LARGE_INTEGER TotalTransSize;

			if ( findfile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				di->Close(&dist);
				return ERROR_NOT_SUPPORTED;
			}
			hDstFile = CreateFileL(((IMAGEGETEXINFO *)*mem)->dest,
					GENERIC_WRITE, 0, NULL, CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL, NULL);
			if ( hDstFile == INVALID_HANDLE_VALUE ){
				memcpy(&((IMAGEGETEXINFO *)*mem)->ff,
						&findfile, sizeof(WIN32_FIND_DATA));
				result = GetLastError();
				di->Close(&dist);
				return result;
			}
			NotifyFileSize(hDstFile, findfile.nFileSizeLow, findfile.nFileSizeHigh);
			TotalSize.u.LowPart = findfile.nFileSizeLow;
			TotalSize.u.HighPart = findfile.nFileSizeHigh;
			TotalTransSize.u.LowPart = 0;
			TotalTransSize.u.HighPart = 0;

			if ( dist.c.BpC > VFS_check_size ){
				header = AllocBuffer = HeapAlloc(DLLheap, 0, dist.c.BpC);
			}

			while( findfile.nFileSizeLow || findfile.nFileSizeHigh ){
				if ( readdrive[0] == '\0' ){ // ファイル
					if ( 0 == (size = di->ReadCluster(&dist, header, min(VFS_check_size, findfile.nFileSizeLow))) ){
						result = ERROR_READ_FAULT;
						break;
					}
				}else{ // ドライブダイレクト中はセクタ単位で読み込む必要がある
					if ( 0 == (size = di->ReadCluster(&dist, header, dist.c.BpC)) ){
						result = ERROR_READ_FAULT;
						break;
					}
				}
				if ( (size > findfile.nFileSizeLow) && (findfile.nFileSizeHigh == 0) ){
					size = findfile.nFileSizeLow;
				}
				SubDD(findfile.nFileSizeLow, findfile.nFileSizeHigh, size, 0);
				if ( WriteFile(hDstFile, header, size, &size, NULL) == FALSE ){
					result = GetLastError();
					break;
				}

				AddDD(TotalTransSize.u.LowPart, TotalTransSize.u.HighPart, size, 0);
				((IMAGEGETEXINFO *)*mem)->Progress(TotalSize, TotalTransSize,
					TotalTransSize, TotalTransSize, 0, 0, NULL, NULL,
					((IMAGEGETEXINFO *)*mem)->lpData);
				if ( *((IMAGEGETEXINFO *)*mem)->Cancel ){
					result = ERROR_CANCELLED;
					break;
				}

				if ( di->SetNextCluster(&dist) == FALSE ) break;
			}

			if ( AllocBuffer != NULL ) HeapFree(DLLheap, 0, AllocBuffer);

			SetFileTime(hDstFile, NULL, //&findfile.ftCreationTime,
					&findfile.ftLastAccessTime, &findfile.ftLastWriteTime);
			CloseHandle(hDstFile);
			di->Close(&dist);
			if ( result != MAX32 ){
				DeleteFileL( ((IMAGEGETEXINFO *)*mem)->dest );
			}
			return result;
		}else{
			BYTE *memp;
			DWORD ReadSize;

			if ( findfile.nFileSizeHigh || (CheckLoadSize(hWnd, sizeL) == FALSE) ){
				di->Close(&dist);
				return ERROR_CANCELLED;
			}
			if ( (*hMap = GlobalAlloc(GMEM_MOVEABLE, *sizeL + dist.c.BpC))== NULL ){
				di->Close(&dist);
				return ERROR_NOT_ENOUGH_MEMORY;
			}
			if ( (*mem = GlobalLock(*hMap)) == NULL ){
				di->Close(&dist);
				return ERROR_NO_MORE_FILES;
			}
			memp = *mem;
			ReadSize = *sizeL;
			while( ReadSize > 0 ){
				if ( readdrive[0] == '\0' ){ // ファイル
					if ( 0 == (size = di->ReadCluster(&dist, memp, ReadSize)) ){
						break;
					}
				}else{ // ドライブダイレクト中はセクタ単位で読み込む必要がある
					if ( 0 == (size = di->ReadCluster(&dist, memp, dist.c.BpC)) ){
						break;
					}
				}
				if ( size > ReadSize ){
					size = ReadSize;
				}
				ReadSize -= size;
				memp += size;
				if ( di->SetNextCluster(&dist) == FALSE ) break;
			}
			di->Close(&dist);
			return ReadSize ? ERROR_READ_FAULT : NO_ERROR;
		}
	}
	return ERROR_NO_MORE_FILES;
}

typedef struct {
	PPXAPPINFO info;
	const TCHAR *srcDIR;
	const TCHAR *dstDIR;
	const TCHAR *file;
} VGA_INFO;

DWORD_PTR USECDECL VGAInfo(VGA_INFO *vga, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case '1':
			tstrcpy(uptr->enums.buffer, vga->srcDIR);
			break;
		case 'C':
			tstrcpy(uptr->enums.buffer, vga->file);
			break;
		case '2':
			tstrcpy(uptr->enums.buffer, vga->dstDIR);
			break;

		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 1;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
			if ( uptr->enums.enumID ){
				uptr->enums.enumID = 0;
				return 1;
			}
			return 0;

//		case PPXCMDID_ENDENUM:		//列挙終了…何もしない

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

ERRORCODE GetArchivefileImageFromTempExtract(TCHAR *ExtractPath, const TCHAR *EntryName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem)
{
	HANDLE hReadFile;
	DWORD size;
	ERRORCODE result;

	VFSFullPath(ExtractPath, FindLastEntryPoint(EntryName), ExtractPath); // 展開先ファイル名を用意
	hReadFile = CreateFileL(ExtractPath, GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hReadFile == INVALID_HANDLE_VALUE ) return GetLastError();

	*sizeL = GetFileSize(hReadFile, sizeH);
	if ( (*sizeL == MAX32) && ((result = GetLastError()) != NO_ERROR) ){
		goto close_fin;
	}

	if ( *sizeH ) *sizeL = MAX32;	// 丸め込み1
	if ( CheckLoadSize((HWND)LFI_ALWAYSLIMITLESS, sizeL) == FALSE ){
		result = ERROR_CANCELLED;
		goto close_fin;
	}
	*sizeH = 0;
	if ( (*hMap = GlobalAlloc(GMEM_MOVEABLE, *sizeL + 4096)) == NULL ){
		goto error_close_fin;
	}
	if ( (*mem = (BYTE *)GlobalLock(*hMap)) == NULL ) goto error_close_fin;
	if ( ReadFile(hReadFile, *mem, *sizeL, (DWORD *)&size, NULL) == FALSE ){
		goto error_close_fin;
	}
	*sizeL = size;
	memset(*mem + size, 0, 4096);
	result = NO_ERROR;
close_fin:
	CloseHandle(hReadFile);
	SetFileAttributesL(ExtractPath, FILE_ATTRIBUTE_NORMAL);
	DeleteFileL(ExtractPath);
	return result;

error_close_fin:
	result = GetLastError();
	goto close_fin;

}

ERRORCODE SusieGetArchivefileImage(const TCHAR *arcfile, const TCHAR *EntryName, SIZE32_T *sizeL, SIZE32_T *sizeH, HANDLE *hMap, BYTE **mem, SUSIE_DLL *su)
{
	ERRORCODE result;
//	char *separatorA;
	#ifdef UNICODE
		WCHAR TempW[VFPS];
		char Temp2[VFPS];
	#else
		char Temp1[VFPS];
	#endif

/*
	separatorA = strrchr(arcfile, ':'); // "::" を検索
	if ( (separatorA != NULL) && (separatorA >= (arcfile + 2)) &&
		 (*(separatorA - 1) == ':') ){
		*(separatorA - 1) = '\0';
	}
*/
	EnterCriticalSection(&ArchiveSection[VFSAS_SUSIE]);

	#ifdef UNICODE
	if ( su->GetFileInfoW != NULL ){
		SUSIE_FINFOW fiW;

		strcpyW(TempW, arcfile);
		result = su->GetFileInfoW(TempW, 0, EntryName, SUSIE_IGNORECASE | SUSIE_SOURCE_DISK, &fiW);
		if ( result != SUSIEERROR_NOERROR ) goto error;
		result = su->GetFileW(TempW, (LONG_PTR)fiW.position,
				(LPWSTR)hMap, SUSIE_SOURCE_DISK | SUSIE_DEST_MEM, NULL, 0);
		if ( (result != SUSIEERROR_NOERROR) || (*hMap == NULL) ) goto error;
		*sizeL = (SIZE32_T)LocalSize(*hMap);
		if ( *sizeL > fiW.filesize ) *sizeL = (SIZE32_T)fiW.filesize;
	}else
	#endif
	{
		SUSIE_FINFO fi;
	#ifdef UNICODE
		#define ARCHIVENAME	(char *)TempW
		#define ENTRYNAMEA	Temp2
		UnicodeToAnsi(arcfile, (char *)TempW, VFPS);
		UnicodeToAnsi(EntryName, Temp2, VFPS);
	#else
		#define ARCHIVENAME Temp1
		#define ENTRYNAMEA	EntryName
		strcpy(Temp1, arcfile);
	#endif
		result = su->GetFileInfo(ARCHIVENAME, 0, ENTRYNAMEA, SUSIE_IGNORECASE | SUSIE_SOURCE_DISK, &fi);
		if ( result != SUSIEERROR_NOERROR ){
			if ( (result == SUSIEERROR_INTERNAL) ||
				 (result == (ERRORCODE)SUSIEERROR_NOTSUPPORT) ){
				HLOCAL fiH;
				SUSIE_FINFO *nfi, *fimax;

				result = su->GetArchiveInfo(ARCHIVENAME, 0,
						SUSIE_SOURCE_DISK, &fiH);
				if ( (result != SUSIEERROR_NOERROR) || (fiH == NULL) ) goto error;
				nfi = LocalLock(fiH);
				fimax = (SUSIE_FINFO *)(BYTE *)((BYTE *)nfi + LocalSize(fiH));
				for ( ; (nfi < fimax) && (nfi->method[0] != 0) ; nfi++ ){
					size_t len;

					if ( nfi->filename[0] == '\0' ) continue; // dir ?
					len = strlen(nfi->path);
					if ( (memcmp(nfi->path, ENTRYNAMEA, len) == 0) &&
						 (strcmp(nfi->filename, ENTRYNAMEA + len) == 0) ){
						fi = *nfi;
						result = SUSIEERROR_NOERROR;
						break;
					}
				}
				LocalUnlock(fiH);
				LocalFree(fiH);
			}
			if ( result != SUSIEERROR_NOERROR ) goto error;
		}
		if ( SUSIEERROR_NOERROR != (result = su->GetFile(ARCHIVENAME,
				(LONG_PTR)fi.position, (LPSTR)hMap,
				SUSIE_SOURCE_DISK | SUSIE_DEST_MEM, NULL, 0)) ){
			goto error;
		}
		if ( *hMap == NULL ) goto error;
		*sizeL = (SIZE32_T)LocalSize(*hMap);
		if ( *sizeL > fi.filesize ) *sizeL = (SIZE32_T)fi.filesize;
	}
	LeaveCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
	*mem = LocalLock(*hMap);
	*sizeH = 0;
	return NO_ERROR;
error:
	LeaveCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
	return result;
}

ERRORCODE UnarcGetArchivefileImage(HWND hWnd, const TCHAR *arcfile, const TCHAR *EntryName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem, void *dt_opt)
{
	TCHAR buf[CMDLINESIZE];
	TCHAR tmppath[VFPS], entry[VFPS];
	VGA_INFO vga;
	ERRORCODE result;
	const UN_DLL *undll;
	int extractmode;

	vga.info.Function = (PPXAPPINFOFUNCTION)VGAInfo;
	vga.info.Name = T("getfile_arc");
	vga.info.RegID = NilStr;
	vga.info.hWnd = hWnd;
	vga.srcDIR = arcfile;
	vga.dstDIR = tmppath;
	vga.file = entry;

	MakeTempEntry(TSIZEOF(tmppath), tmppath, FILE_ATTRIBUTE_COMPRESSED);

	undll = undll_list + (int)(DWORD_PTR)dt_opt - 1;

	tstrcpy(entry, EntryName);
	if ( undll->flags & UNDLLFLAG_FIX_BRACKET ){
		TCHAR *dp;

		for ( dp = entry ; *dp ; dp++ ){
			if ( Ismulti(*dp) ){
				dp++;
				if ( *dp == '\0' ) break;
				continue;
			}

			switch ( *dp ){
				case '\\':
					*dp = '/';
					break;

				case '[':
				case ']':
					if ( undll->flags & UNDLLFLAG_SKIP_OPENED ) break;
					memmove(dp + 1, dp, TSTRSIZE(dp));
					*dp++ = '\\';
					break;
			}
		}
	}
	extractmode = (*FindLastEntryPoint(entry) != '@') ? UNARCEXTRACT_SINGLE : 0;
	result = UnArc_Extract(&vga.info, dt_opt, extractmode, buf, XEO_NOEDIT);
	if ( result != NO_ERROR ) return result;

	if ( RunUnARCExec(&vga.info, dt_opt, buf, NilStr) ) return ERROR_READ_FAULT;
	return GetArchivefileImageFromTempExtract(tmppath, EntryName, sizeL, sizeH, hMap, mem);
}

/*
	hWnd = INVALID_HANDLE_VALUE 全展開
	hFile = NULL
	ArchiveName
	sizeL = NULL ファイル種別確定済み
*/

VFSDLL ERRORCODE PPXAPI VFSGetArchivefileImage(HWND hWnd, HANDLE hFile, const TCHAR *ArchiveName, const TCHAR *EntryName, DWORD *sizeL, DWORD *sizeH, HANDLE *hMap, BYTE **mem)
{
	BYTE header[VFS_check_size];
	TCHAR arcfile[VFPS];
	DWORD fsize = 0;
	void *dt_opt;
	LONG high = 0;
	int type;

	if ( hFile != NULL ){
		SetFilePointer(hFile, 0, &high, FILE_BEGIN);
		fsize = ReadFileHeader(hFile, header, sizeof(header));
		if ( hWnd != INVALID_HANDLE_VALUE ) CloseHandle(hFile);
	}

	tstrcpy(arcfile, ArchiveName);
	if ( *EntryName == '\\' ) EntryName++;
/*
	p = tstrrchr(ArchiveName, ':');
	if ( (p != NULL) && (p != ArchiveName) && (*(p - 1) == ':') ){
		tstrcpy(arcfile, ArchiveName);
		arcfile[p - ArchiveName - 1] = '\0';
		ArchiveName = arcfile;
		dllp = p + 1;
	}
*/
	if ( sizeL == NULL ){
		type = *sizeH;
		sizeH = NULL;
		dt_opt = NULL;
	}else{
		type = VFSCheckDir(arcfile, header, fsize, &dt_opt);
	}
	switch( type ){
		case VFSDT_CABFOLDER:
		case VFSDT_LZHFOLDER:
		case VFSDT_ZIPFOLDER:
			return GetZipFolderImage(hWnd, arcfile, EntryName, sizeL, sizeH, hMap, mem, VFSDT_ZIPFOLDER - type); // 0(VFSDT_ZIPFOLDER)～2(VFSDT_CABFOLDER)

		case VFSDT_FATIMG:
			return GetDiskFileImage(&fatimagestruct, hWnd, arcfile, EntryName, header, sizeL, sizeH, hMap, mem, fsize);

		case VFSDT_CDIMG: {
			ERRORCODE result;

			result = GetDiskFileImage(&cdimagestruct, hWnd, arcfile, EntryName, header, sizeL, sizeH, hMap, mem, fsize);
			if ( result != ERROR_NO_MORE_FILES ) return result;
			result = GetDiskFileImage(&dvdimagestruct, hWnd, arcfile, EntryName, header, sizeL, sizeH, hMap, mem, fsize);
			return result;
		}

		case VFSDT_UN:
			return UnarcGetArchivefileImage(hWnd, arcfile, EntryName, sizeL, sizeH, hMap, mem, dt_opt);

		case VFSDT_SUSIE:
			return SusieGetArchivefileImage(arcfile, EntryName, sizeL, sizeH, hMap, mem, susie_list + (int)(DWORD_PTR)dt_opt - 1);
	}
	return ERROR_PATH_NOT_FOUND;
}

VFSDLL int PPXAPI VFSArchiveSection(DWORD mode, const TCHAR *threadname)
{
	if ( mode & VFSAS_CHECK ) return ArchiverUse;

	if ( mode & VFSAS_LEAVE ){
		LeaveCriticalSection(&ArchiveSection[mode & 1]);
		ArchiverUse--;
	}else{
		if ( ((mode & 1) == VFSAS_UNDLL) ? useundll : usesusie ){
			EnterCriticalSection(&ArchiveSection[mode & 1]);
			ArchiverUse++;
		}else{
			return -1;
		}
	}
	if ( threadname != NULL ){
		THREADSTRUCT *ts;

		ts = GetCurrentThreadInfo();
		if ( ts != NULL ) ts->ThreadName = threadname;
	}
	return 0;
}

VFSDLL int PPXAPI VFSGetSusieList(const SUSIE_DLL **suptr, BYTE **strings)
{
	SUSIE_DLL *sudll;
	int i;

	*suptr = sudll = susie_list;
	*strings = (BYTE *)Thsusie_str.bottom;

	for ( i = 0 ; i < susie_items ; i++, sudll++ ){
		CheckAndLoadSusiePlugin(sudll, NULL, NULL, VFS_BMP | VFS_DIRECTORY | VFS_FORCELOAD_PLUGIN);
	}
	return susie_items;
}

VFSDLL const SUSIE_DLL * PPXAPI VFSGetSusieFuncs(const void *dt_opt)
{
	int delta = PtrToValue(dt_opt) - 1;
	SUSIE_DLL *sudll;

	if ( (delta < 0) || (delta >= susie_items) ) return NULL;
	sudll = susie_list + delta;
	if ( sudll->hadd != NULL ) return sudll;

	if ( CheckAndLoadSusiePlugin(sudll, NULL, NULL, VFS_BMP | VFS_DIRECTORY | VFS_FORCELOAD_PLUGIN) != FALSE ){
		return sudll;
	}
	return NULL;
}
