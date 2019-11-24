/*-----------------------------------------------------------------------------
	Paper Plane cUI		比較マーク
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "FATTIME.H"
#include "sha.h"
#pragma hdrstop

INT_PTR CALLBACK CompareDetailDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
#define CompareDetailDialog(hWnd, param) PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CMARK), hWnd, CompareDetailDlgBox, (LPARAM)param)

typedef struct {
	const TCHAR *name;
	DWORD mode;
} COMPAREMENUSTRUCT;

#define ATTR_FILE	0x0700
#define ATTR_DIR	(0x1700 | CMPSUBDIR)
COMPAREMENUSTRUCT CompareMenu[] = {
	{MES_CMPN, ATTR_FILE | CMP_NEW},
	{MES_CMPW, ATTR_FILE | CMP_NEWONLY},
	{MES_CMPT, ATTR_FILE | CMP_TIME},
	{MES_CMPE, ATTR_FILE | CMP_EXIST},
	{MES_CMPX, ATTR_DIR  | CMP_EXIST},
	{MES_CMPS, ATTR_FILE | CMP_SIZE},
	{MES_CMPL, ATTR_FILE | CMP_LARGE},
	{MES_CMPA, ATTR_FILE | CMP_SAMESIZETIME},
	{MES_CMPB, ATTR_DIR  | CMP_BINARY},
	{MES_CMP1, ATTR_DIR  | CMP_SHA1},
	{MES_CMPF, ATTR_FILE | CMP_EXIST | CMPWITHOUT_EXT},
	{MES_CMPQ, ATTR_FILE | CMP_SIZEONLY},
	{NULL, 0}
};

const TCHAR *CompareDetailMenu[] = {
	// CMP_2
	MES_CMPN, MES_CMPT, MES_CMPE, MES_CMPS, MES_CMPL, // 1～
	MES_CMPA, MES_CMPB, MES_CMPI, MES_CMPW, MES_CMP1, // 5～
	// CMPWITHOUT_EXT
	MES_CMPF,
	// CMP_1
	MES_CMPZ, MES_CMPM,
	NULL}; // 11～

struct COMPAREBINARYSTRUCT {
	HWND hWnd;
	char *srcbuf, *destbuf;
	SIZE32_T bufsize;
	int mode, subdir;
};

//---------------------------------------------------------- 一覧情報を送信する
BOOL PPcCompareSend(PPC_APPINFO *cinfo, COMPAREMARKPACKET *cmp, HWND PairHWnd)
{
	HANDLE	hFile;
	BOOL	result;
	DWORD	qsize, wsize;

	hFile = CreateFileL(cmp->filename,
			GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return FALSE;

	qsize = sizeof(ENTRYCELL) * cinfo->e.cellDataMax;
	result = WriteFile(hFile, cinfo->e.CELLDATA.p, qsize, &wsize, NULL);

	if ( IsTrue(result) && (qsize != wsize) ){
		SetLastError(ERROR_DISK_FULL);
		result = FALSE;
	}

	CloseHandle(hFile);

	if ( IsTrue(result) ){
		COPYDATASTRUCT copydata;

		cmp->DirType = cinfo->e.Dtype.mode;
		cmp->LoadCounter = cinfo->LoadCounter;
		copydata.dwData = 'O';
		copydata.cbData = sizeof(COMPAREMARKPACKET);
		copydata.lpData = cmp;
		SendMessage(PairHWnd, WM_COPYDATA,
					(WPARAM)cinfo->info.hWnd, (LPARAM)&copydata);
	}
	return result;
}
/*----------------------------------------------------------
	NO_ERROR			一致した
	ERROR_INVALID_DATA	一致しなかった
	その他				比較時にエラーが発生した（この時点で終了）
*/
ERRORCODE PPcCompareExistDirMain(HWND hWnd, const TCHAR *srcpath, WIN32_FIND_DATA *cellN, const TCHAR *destpath)
{
	TCHAR srcname[VFPS], destname[VFPS];
	ERRORCODE result;
	HANDLE hFFsrc, hFFdest;
								// ディレクトリ処理 ---------------------------
	WIN32_FIND_DATA ffsrc;
	WIN32_FIND_DATA ffdest;
	TCHAR tempname[VFPS];
	DWORD filecount = 0;

	VFSFullPath(srcname, cellN->cFileName, srcpath);
	VFSFullPath(destname, cellN->cFileName, destpath);
	CatPath(tempname, destname, T("*"));
								// 反対側のファイル数を算出
	hFFdest = FindFirstFileL(tempname, &ffdest);
	if ( hFFdest == INVALID_HANDLE_VALUE ){
		return ERROR_INVALID_DATA;
	}
	for ( ; ; ){
		if ( !IsRelativeDir(ffdest.cFileName) ){
			filecount++;
		}
		if ( FALSE == FindNextFile(hFFdest, &ffdest) ) break;
	}
	FindClose(hFFdest);
								// こちら側のファイル情報を順次求め、比較
	CatPath(tempname, srcname, T("*"));
	hFFsrc = FindFirstFileL(tempname, &ffsrc);
	if ( hFFsrc == INVALID_HANDLE_VALUE ){
		return ERROR_INVALID_DATA;
	}
	result = NO_ERROR;
	for ( ; ; ){
		DWORD attr;

		if ( !IsRelativeDir(ffsrc.cFileName) ){
			filecount--;
			CatPath(tempname, destname, ffsrc.cFileName);
			attr = GetFileAttributesL(tempname);
			if ( attr == BADATTR ){
				result = ERROR_INVALID_DATA;
				break;
			}
			if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
				if ( !(ffsrc.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
					result = ERROR_INVALID_DATA;
					break;
				}
				result = PPcCompareExistDirMain(hWnd, srcname, &ffsrc, destname);
				if ( result != NO_ERROR ) break;
			}
		}
		if ( FALSE == FindNextFile(hFFsrc, &ffsrc) ){
			if ( filecount ) result = ERROR_INVALID_DATA;
			break;
		}
	}
	FindClose(hFFsrc);
	return result;
}

BOOL IsBreakCompare(struct COMPAREBINARYSTRUCT *cbs)
{
	MSG msg;

	if ( !(GetAsyncKeyState(VK_PAUSE) & KEYSTATE_FULLPUSH) ){
		if ( PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) == FALSE ){
			return FALSE;
		}
		if ( (msg.message != WM_KEYDOWN) || ((int)msg.wParam != VK_PAUSE) ){
			return FALSE;
		}
	}
	return (PMessageBox(cbs->hWnd, NULL, MES_TFCP, MB_QYES) == IDOK);
}

ERRORCODE PPcCompareBinary(struct COMPAREBINARYSTRUCT *cbs, const TCHAR *srcpath, WIN32_FIND_DATA *cellN, const TCHAR *destpath, WIN32_FIND_DATA *cellp)
{
	TCHAR srcname[VFPS], destname[VFPS];
	HANDLE hSrc, hDest;
	ERRORCODE result;
	DWORD srcsize, destsize;
								// ディレクトリ処理 ---------------------------
	if ( cellN->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		if ( (cbs->subdir) && (cellp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			HANDLE hFFsrc, hFFdest;
			WIN32_FIND_DATA ffsrc;
			WIN32_FIND_DATA ffdest;
			TCHAR tempname[VFPS];
			DWORD filecount = 0;

			VFSFullPath(srcname, cellN->cFileName, srcpath);
			VFSFullPath(destname, cellN->cFileName, destpath);
			CatPath(tempname, destname, T("*"));
								// 反対側のファイル数を算出
			hFFdest = FindFirstFileL(tempname, &ffdest);
			if ( hFFdest == INVALID_HANDLE_VALUE ){
				return ERROR_INVALID_DATA;
			}
			for ( ; ; ){
				if ( !IsRelativeDir(ffdest.cFileName) ){
					filecount++;
				}
				if ( FALSE == FindNextFile(hFFdest, &ffdest) ) break;
			}
			FindClose(hFFdest);
								// こちら側のファイル情報を順次求め、比較
			CatPath(tempname, srcname, T("*"));
			hFFsrc = FindFirstFileL(tempname, &ffsrc);
			if ( hFFsrc == INVALID_HANDLE_VALUE ){
				return ERROR_INVALID_DATA;
			}
			result = NO_ERROR;
			for ( ; ; ){
				if ( !IsRelativeDir(ffsrc.cFileName) ){
					filecount--;
					CatPath(tempname, destname, ffsrc.cFileName);
					hFFdest = FindFirstFileL(tempname, &ffdest);
					if ( hFFdest == INVALID_HANDLE_VALUE ){
						result = ERROR_INVALID_DATA;
						break;
					}
					FindClose(hFFdest);
					result = PPcCompareBinary(cbs,
							srcname, &ffsrc, destname, &ffdest);
					if ( result != NO_ERROR ) break;
				}
				if ( FALSE == FindNextFile(hFFsrc, &ffsrc) ){
					if ( filecount ) result = ERROR_INVALID_DATA;
					break;
				}
			}
			FindClose(hFFsrc);
			return result;
		}
		return ERROR_INVALID_DATA;
	}

	if ( cellp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		return ERROR_INVALID_DATA;
	}
								// ファイルサイズが同じ？
	if ( cellN->nFileSizeHigh != cellp->nFileSizeHigh ){
		return ERROR_INVALID_DATA;
	}
	if ( cellN->nFileSizeLow != cellp->nFileSizeLow ){
		return ERROR_INVALID_DATA;
	}
								// イメージチェック ---------------------------
	VFSFullPath(srcname, cellN->cFileName, srcpath);
	VFSFullPath(destname, cellN->cFileName, destpath);
	hSrc = CreateFileL(srcname, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hSrc == INVALID_HANDLE_VALUE ){
		return GetLastError();
	}
	hDest = CreateFileL(destname, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hDest == INVALID_HANDLE_VALUE ){
		result = GetLastError();
		CloseHandle(hSrc);
		return result;
	}
	result = NO_ERROR;
	if ( cbs->mode == CMP_BINARY ){ // CMP_BINARY
		for ( ; ; ){
			if ( FALSE == ReadFile(hSrc, cbs->srcbuf, cbs->bufsize, &srcsize, NULL) ){
				result = GetLastError();
				break;
			}
			if ( srcsize == 0 ) break;

			if ( FALSE == ReadFile(hDest, cbs->destbuf, cbs->bufsize, &destsize, NULL) ){
				result = GetLastError();
				break;
			}
			if ( (srcsize != destsize) ||
				memcmp(cbs->srcbuf, cbs->destbuf, srcsize) ){
				result = ERROR_INVALID_DATA;
				break;
			}

			if ( IsBreakCompare(cbs) ){
				result = ERROR_CANCELLED;
				break;
			}
		}
	}else{ // CMP_SHA1
		SHA1Context sha1;

		SHA1Reset(&sha1);
		for ( ; ; ){
			if ( FALSE == ReadFile(hSrc, cbs->srcbuf, cbs->bufsize, &srcsize, NULL) ){
				result = GetLastError();
				break;
			}
			if ( srcsize == 0 ) break;
			SHA1Input(&sha1, (uint8_t *)cbs->srcbuf, srcsize);

			if ( IsBreakCompare(cbs) ){
				result = ERROR_CANCELLED;
				break;
			}
		}
		SHA1Result(&sha1, (uint8_t *)&srcname);

		if ( result == NO_ERROR ){
			SHA1Reset(&sha1);
			for ( ; ; ){
				if ( FALSE == ReadFile(hDest, cbs->destbuf, cbs->bufsize, &destsize, NULL) ){
					result = GetLastError();
					break;
				}
				if ( destsize == 0 ) break;
				SHA1Input(&sha1, (uint8_t *)cbs->destbuf, destsize);

				if ( IsBreakCompare(cbs) ){
					result = ERROR_CANCELLED;
					break;
				}
			}
			SHA1Result(&sha1, (uint8_t *)&destname);
			if ( result == NO_ERROR ){
				if ( memcmp(srcname, destname, SHA1HashSize) ){
					result = ERROR_INVALID_DATA;
				}
			}
		}
	}
	CloseHandle(hSrc);
	CloseHandle(hDest);
	return result;
}

ERRORCODE PPcCompareBinaryMain(HWND hWnd, int mode, int subdir, const TCHAR *srcpath, WIN32_FIND_DATA *cellN, const TCHAR *destpath, WIN32_FIND_DATA *cellp)
{
	struct COMPAREBINARYSTRUCT cbs;
	ERRORCODE result;

	GetAsyncKeyState(VK_PAUSE); // 読み捨て(最下位bit対策)
	cbs.hWnd = hWnd;
	cbs.bufsize = GetNT_9xValue(1 * MB, 256 * KB); // NT:1Mbytes / 9x:256kbytes
	cbs.srcbuf = PPcHeapAlloc(cbs.bufsize);
	cbs.destbuf = PPcHeapAlloc(cbs.bufsize);
	cbs.mode = mode;
	cbs.subdir = subdir;
	result = PPcCompareBinary(&cbs, srcpath, cellN, destpath, cellp);
	PPcHeapFree(cbs.srcbuf);
	PPcHeapFree(cbs.destbuf);
	return result;
}

void MessageMatchEntry(HWND hWnd, HWND hPairWnd, DWORD PairLoadCounter, TCHAR *filename)
{
	COPYDATASTRUCT copydata;

	copydata.dwData = TMAKELPARAM('O' + 0x100, PairLoadCounter);
	copydata.cbData = TSTRSIZE32(filename);
	copydata.lpData = filename;
	SendMessage(hPairWnd, WM_COPYDATA, (WPARAM)hWnd, (LPARAM)&copydata);
}

BOOL USEFASTCALL CompareProgress(PPC_APPINFO *cinfo, int in)
{
	TCHAR buf[1000];
	BOOL result;

	wsprintf(buf, T("%d/%d"), in, cinfo->e.cellIMax);
	SetPopMsg(cinfo, POPMSG_PROGRESSBUSYMSG, buf);
	UpdateWindow_Part(cinfo->info.hWnd);
	if ( IsTrue( result = ReadDirBreakCheck(cinfo) ) ){
		SetPopMsg(cinfo, POPMSG_MSG, MES_BRAK);
	}
	return result;
}
//------------------------------------------------------------------ 比較メイン
BOOL PPcCompareMain(PPC_APPINFO *cinfo, HWND hPairWnd, COMPAREMARKPACKET *cmp)
{
	ENTRYCELL *paircell = NULL, *pairbase;
	int mode;
	ENTRYCOUNT cells;			// 相手のセル数
	DWORD sizeL;		// ファイルの大きさ
	TCHAR pairpath[VFPS];
	DWORD PairLoadCounter;
	DWORD dirmask, unmarkmask;
	int subdir;
	DWORD tick;
	BOOL SimpleDir; // ファイル名がディレクトリ内で一意かどうか(速度最適化用)

	mode = cmp->mode;
	PairLoadCounter = cmp->LoadCounter;
	cinfo->MarkMask = (mode >> CMPMARKSHIFT) & CMPMARKMASK;
	subdir = (mode & CMPSUBDIR) ? FILE_ATTRIBUTE_DIRECTORY : 0;
	dirmask = cinfo->MarkMask & FILE_ATTRIBUTE_DIRECTORY;
	unmarkmask = cinfo->MarkMask ^ CMPMARKMASK;
	mode &= CMPTYPEMASK | CMPDOWN;

	if ( (mode == (CMP_BINARY | CMPDOWN)) ||
		 (mode == (CMP_SIZEONLY | CMPDOWN)) ||
		 (mode == (CMP_SHA1 | CMPDOWN)) ){
		Repaint(cinfo);
		return TRUE;	// 済み
	}
	if ( (((mode == CMP_EXIST) || (mode == CMP_SAMESIZETIME)) && (subdir != 0)) ||
		 (mode == CMP_BINARY) || (mode == CMP_SHA1) ){
		DWORD attr;

		PPcOldPath((INT_PTR)hPairWnd, MAX32, pairpath);
		attr = GetFileAttributes(pairpath);
		if ( (attr == BADATTR) || !(attr & FILE_ATTRIBUTE_DIRECTORY) ){
			XMessage(NULL, NULL, XM_GrERRld, StrNotSupport);
			return FALSE;
		}
	}
										// File Open --------------------------
	if ( LoadFileImage(cmp->filename, 0,
			(char **)&paircell, &sizeL, LFI_ALWAYSLIMITLESS) ){
		return FALSE;
	}
	if ( (mode == CMP_BINARY) || (mode == CMP_SHA1) ){
		SetPopMsg(cinfo, POPMSG_PROGRESSBUSYMSG, MES_SCMP);
		UpdateWindow_Part(cinfo->info.hWnd);
	}else{
		StopPopMsg(cinfo, PMF_DISPLAYMASK);
	}
										// 比較準備 ---------------------------
	tick = GetTickCount();
	cells = sizeL / sizeof(ENTRYCELL);
	pairbase = paircell;
	// 相対指定を除去
	while ( cells && IsRelativeDir(pairbase->f.cFileName) ){
		pairbase++;
		cells--;
	}
	{
		ENTRYINDEX in;

		for ( in = 0 ; in < cells ; in++ ){
			if ( pairbase[in].state < ECS_NORMAL ){
				// SysMsgやDeletedは比較対象外にする
				pairbase[in].f.cFileName[0] = '\0';
			}
		}
	}
	SimpleDir = (cinfo->e.Dtype.mode == VFSDT_PATH) && (cmp->DirType == VFSDT_PATH);
										// 比較ループ -------------------------

	if ( mode & CMPWITHOUT_EXT ){
		ENTRYINDEX in, ip;
		int extoffset;

		for ( in = 0 ; in < cinfo->e.cellIMax ; in++ ){
			TCHAR *nowname, *pairname;
			ENTRYCELL *cellN, *cellp;

			cellN = &CEL(in);
			if ( cellN->state < ECS_NORMAL ) continue; // SysMsgやDeletedはだめ
			if ( cellN->attr & (ECA_PARENT | ECA_THIS) ) continue;
			if ( cellN->f.dwFileAttributes & unmarkmask ) continue;

			nowname = FindLastEntryPoint(cellN->f.cFileName);
			extoffset = cellN->ext - (nowname - cellN->f.cFileName);
			cellp = pairbase;
			for ( ip = cells ; ip ; ip--, cellp++ ){
				DWORD nowtick;
														// マーク対象外属性？
				if ( cellp->f.dwFileAttributes & unmarkmask ) continue;

				nowtick = GetTickCount();
				if ( (nowtick - tick) > 1000 ){
					tick = nowtick;
					if ( IsTrue(CompareProgress(cinfo, in)) ){
						in = cinfo->e.cellIMax;
						break;
					}
				}
				pairname = FindLastEntryPoint(cellp->f.cFileName);
				if ( tstrnicmp(nowname, pairname, extoffset) ) continue; // 名前不一致
				// 名前末尾不一致
				if ( (pairname[extoffset] != '\0') && // 末尾でない
					  ((pairname[extoffset] != '.') || // 拡張子でない
					   (tstrchr(pairname + extoffset + 1, '.') != NULL)) ){
					continue;
				}
				if ( cellp->f.dwFileAttributes & dirmask ){
							// 一方がファイルで、他方がディレクトリなら対象外
					if ( (cellp->f.dwFileAttributes ^
						cellN->f.dwFileAttributes) & FILE_ATTRIBUTE_DIRECTORY){
						continue;
					}
				}
				CellMark(cinfo, in, MARK_CHECK);
			}
		}
	}else if ( mode != CMP_SIZEONLY ){
		ENTRYINDEX in, ip;

		for ( in = 0 ; in < cinfo->e.cellIMax ; in++ ){
			TCHAR *nowname, *pairname;
			ENTRYCELL *cellN, *cellp;

			cellN = &CEL(in);
			if ( cellN->state < ECS_NORMAL ) continue; // SysMsgやDeletedはだめ
			if ( cellN->attr & (ECA_PARENT | ECA_THIS) ) continue;
			if ( cellN->f.dwFileAttributes & unmarkmask ) continue;

			nowname = FindLastEntryPoint(cellN->f.cFileName);
			cellp = pairbase;
			for ( ip = cells ; ip ; ip--, cellp++ ){
				DWORD nowtick;
														// マーク対象外属性？
				if ( cellp->f.dwFileAttributes & unmarkmask ) continue;

				nowtick = GetTickCount();
				if ( (nowtick - tick) > 1000 ){
					tick = nowtick;
					if ( IsTrue(CompareProgress(cinfo, in)) ){
						in = cinfo->e.cellIMax;
						break;
					}
				}
				pairname = FindLastEntryPoint(cellp->f.cFileName);
				if ( tstricmp(nowname, pairname) ) continue;

				if ( cellp->f.dwFileAttributes & dirmask ){
							// 一方がファイルで、他方がディレクトリなら対象外
					if ( (cellp->f.dwFileAttributes ^
						cellN->f.dwFileAttributes) & FILE_ATTRIBUTE_DIRECTORY){
						continue;
					}
				}

				switch(mode){
					case CMP_NEW:				// old(反対窓 new file)
					case CMP_TIME:				// old(反対窓 TimeStamp)
						if ( FuzzyCompareFileTime(&cellN->f.ftLastWriteTime,
								&cellp->f.ftLastWriteTime) < 0){
							CellMark(cinfo, in, MARK_CHECK);
						}
						break;
					case CMP_NEW | CMPDOWN:	// new(現在窓 new file)
					case CMP_TIME | CMPDOWN:	// new(現在窓 TimeStamp)
						if ( FuzzyCompareFileTime(&cellN->f.ftLastWriteTime,
								&cellp->f.ftLastWriteTime) > 0){
							CellMark(cinfo, in, MARK_CHECK);
						}
						break;

					case CMP_EXIST | CMPDOWN:	// Exist(現在窓 Exist)
						if ( cellN->f.dwFileAttributes & subdir) break;
						CellMark(cinfo, in, MARK_CHECK);
						break;
					case CMP_EXIST:				// Exist(反対窓 Exist)
						if ( cellN->f.dwFileAttributes & subdir){
							if ( NO_ERROR != PPcCompareExistDirMain(
									cinfo->info.hWnd, cinfo->RealPath,
									&cellN->f, pairpath) ){
								break;
							}
							MessageMatchEntry(cinfo->info.hWnd, hPairWnd,
									PairLoadCounter, cellp->f.cFileName);
						}
						CellMark(cinfo, in, MARK_CHECK);
						break;

					case CMP_SIZE:				// small size(反対窓 FileSize)
					case CMP_SIZE | CMPDOWN:	// large size(現在窓 FileSize)
						if ( (cellN->f.nFileSizeHigh != cellp->f.nFileSizeHigh) ||
							 (cellN->f.nFileSizeLow != cellp->f.nFileSizeLow) ){
							CellMark(cinfo, in, MARK_CHECK);
						}
						break;

					case CMP_LARGE:				// small size(反対窓 FileSize)
						if ( cellN->f.nFileSizeHigh == cellp->f.nFileSizeHigh){
							if (cellN->f.nFileSizeLow < cellp->f.nFileSizeLow){
								CellMark(cinfo, in, MARK_CHECK);
							}
						}else if (cellN->f.nFileSizeHigh < cellp->f.nFileSizeHigh){
							CellMark(cinfo, in, MARK_CHECK);
						}
						break;
					case CMP_LARGE | CMPDOWN:	// large size(現在窓 FileSize)
						if ( cellN->f.nFileSizeHigh == cellp->f.nFileSizeHigh){
							if (cellN->f.nFileSizeLow > cellp->f.nFileSizeLow){
								CellMark(cinfo, in, MARK_CHECK);
							}
						}else if (cellN->f.nFileSizeHigh > cellp->f.nFileSizeHigh){
							CellMark(cinfo, in, MARK_CHECK);
						}
						break;

					case CMP_SAMESIZETIME | CMPDOWN:	// Same(現在窓 FileSize)
					case CMP_SAMESIZETIME:				// Same
						if ( FuzzyCompareFileTime(&cellN->f.ftLastWriteTime,
								&cellp->f.ftLastWriteTime)) break;
						if ( cellN->f.nFileSizeHigh != cellp->f.nFileSizeHigh){
							break;
						}
						if ( cellN->f.nFileSizeLow != cellp->f.nFileSizeLow ){
							break;
						}
						CellMark(cinfo, in, MARK_CHECK);
						break;

					case CMP_BINARY | CMPDOWN:	// Same Binary(現在窓)
					case CMP_SHA1 | CMPDOWN:	// SHA1(現在窓)
						// 何もしない
						break;

					case CMP_SHA1:				// Same SHA-1
					case CMP_BINARY: {			// Same Binary
						ERRORCODE result;
												// SysMsgやDeletedなのでだめ
						if ( cellp->state < ECS_NORMAL ) break;
						result = PPcCompareBinaryMain(cinfo->info.hWnd, mode,
								subdir,
								cinfo->RealPath, &cellN->f, pairpath, &cellp->f);
						if ( result == NO_ERROR ){
							CellMark(cinfo, in, MARK_CHECK);
							MessageMatchEntry(cinfo->info.hWnd, hPairWnd,
									PairLoadCounter, cellp->f.cFileName);
							break;
						}
						if ( result == ERROR_INVALID_DATA ) break;
						in = cinfo->e.cellIMax;
						if ( result == ERROR_CANCELLED ){
							SetPopMsg(cinfo, POPMSG_MSG, MES_BRAK);
						}else{
							PPErrorBox(cinfo->info.hWnd, cellp->f.cFileName, result);
						}
						break;
					}
//					default:	// 不明動作は処理しない
				}

				// SimpleDir の時は、比較済みの比較対象を次回の比較対象から外す
				if ( SimpleDir ){
					if ( pairbase == cellp ){ // 先頭の時
						pairbase++;
						cells--;

						while ( cells && (pairbase->f.cFileName[0] == '\0') ){
							pairbase++;
							cells--;
						}
					}else{
						cellp->f.cFileName[0] = '\0';	// 比較したので消去
					}
				}
				break;
			}
														// new file
			if ( (ip == 0) &&
				((mode == (CMP_NEW | CMPDOWN)) || (mode == CMP_NEWONLY) || (mode == (CMP_NEWONLY | CMPDOWN))) ){
				CellMark(cinfo, in, MARK_CHECK);
			}
		}
	}else{
		ENTRYINDEX in, ip;
		int hilight = 0;

		for ( in = 0 ; in < cinfo->e.cellIMax ; in++ ){
			ENTRYCELL *cellN, *cellp;

			cellN = &CEL(in);
			if ( cellN->state < ECS_NORMAL ) continue; // SysMsgやDeletedはだめ
			if ( cellN->attr & (ECA_PARENT | ECA_THIS) ) continue;
			if ( cellN->f.dwFileAttributes & unmarkmask ) continue;

			cellp = pairbase;
			for ( ip = cells ; ip ; ip--, cellp++ ){
				DWORD nowtick;
														// マーク対象外属性？
				if ( cellp->f.dwFileAttributes & unmarkmask ) continue;

				nowtick = GetTickCount();
				if ( (nowtick - tick) > 1000 ){
					tick = nowtick;
					if ( IsTrue(CompareProgress(cinfo, in)) ){
						in = cinfo->e.cellIMax;
						break;
					}
				}

				if ( (cellN->f.nFileSizeHigh == cellp->f.nFileSizeHigh) &&
					 (cellN->f.nFileSizeLow  == cellp->f.nFileSizeLow) ){
					if ( cellp->f.dwFileAttributes & dirmask ){
							// 一方がファイルで、他方がディレクトリなら対象外
						if ( (cellp->f.dwFileAttributes ^ cellN->f.dwFileAttributes) &
								FILE_ATTRIBUTE_DIRECTORY ){
							continue;
						}
					}
					hilight = (hilight & 3) + 1;
					cellN->state = (BYTE)((cellN->state & ECS_HLMASK) + (hilight * ECS_HLBIT));
					CellMark(cinfo, in, MARK_CHECK);
					MessageMatchEntry(cinfo->info.hWnd, hPairWnd,
							PairLoadCounter, cellp->f.cFileName);
				}
			}
		}
	}
	HeapFree( hProcessHeap, 0, paircell);
	StopPopMsg(cinfo, PMF_DISPLAYMASK);

	if ( (mode == CMP_BINARY) || (mode == CMP_SHA1) || (mode & CMPDOWN) ){
		PostMessage( ((mode == CMP_BINARY) || (mode == CMP_SHA1)) ?
				hPairWnd : cinfo->info.hWnd, WM_PPXCOMMAND, KC_ENDCOMPARE, 0);
	}
	Repaint(cinfo);
	return TRUE;
}

void PPcCompareOneWindow(PPC_APPINFO *cinfo, int mode)
{
	int i, oldmatch = -1, hilight = 0;
	XC_SORT xc = { {{0, -1, -1, -1}}, 0, SORTE_DEFAULT_VALUE};

	cinfo->MarkMask = (mode >> CMPMARKSHIFT) & CMPMARKMASK;
	mode = (mode & CMPTYPEMASK) - CMP_1SIZE;
	xc.mode.dat[0] = (char)((mode == 0) ? 2 : 17); // 検索しやすいようにソート
	PPC_SortMain(cinfo, &xc);

	for ( i = 0 ; i < cinfo->e.cellIMax - 1 ; i++ ){
		if ( mode == 0 ){
			if ( (CEL(i).f.nFileSizeHigh == CEL(i + 1).f.nFileSizeHigh) &&
				 (CEL(i).f.nFileSizeLow == CEL(i + 1).f.nFileSizeLow) ){
				if ( (CEL(i).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
					 (CEL(i+1).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
					continue;
				}
			}else{
				continue;
			}
		}else{
			if ((CEL(i).comment != EC_NOCOMMENT) &&
				(CEL(i + 1).comment != EC_NOCOMMENT) &&
				!tstrcmp(ThPointerT(&cinfo->EntryComments, CEL(i).comment),
						 ThPointerT(&cinfo->EntryComments, CEL(i+ 1).comment))){
				// 空行
			}else{
				continue;
			}
		}
		// 該当有り
		if ( i != oldmatch ){ // 非連続
			hilight = (hilight & 3) + 1;
			CellMark(cinfo, i, MARK_CHECK);
			CEL(i).state = (BYTE)((CEL(i).state & ECS_HLMASK) + (hilight * ECS_HLBIT));
		}
		oldmatch = i + 1;
		CellMark(cinfo, i + 1 , MARK_CHECK);
		CEL(i + 1).state = (BYTE)((CEL(i + 1).state & ECS_HLMASK) + (hilight * ECS_HLBIT));
	}

	xc.mode.dat[0] = 6;					// マークエントリを集める
	PPC_SortMain(cinfo, &xc);
	Repaint(cinfo);
}

DWORD CompareHashMain(PPC_APPINFO *cinfo, const TCHAR *hashtop, DWORD hashlen)
{
	ENTRYCELL *cell;
	int hlid;
	int work;
	int match = 0;
	TCHAR *cmtptr;

	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		if ( cell->comment == EC_NOCOMMENT ) continue;
		cmtptr = ThPointerT(&cinfo->EntryComments, cell->comment);
		cmtptr = tstrchr(cmtptr, ':');
		if ( cmtptr == NULL ) continue;

		if ( !memicmp(cmtptr + 1, hashtop, hashlen * sizeof(TCHAR)) ){
			hlid = 4;
			cell->state = (CEL(cinfo->e.cellN).state & (BYTE)ECS_HLMASK) | (BYTE)(ECS_HLBIT * hlid);
			Repaint(cinfo);
			match++;
		}
	}
	return match;
}

DWORD CompareHashFromClipBoard(PPC_APPINFO *cinfo, int comparemode)
{
	TCHAR ClipBoardText[0x800], *ptr, *hashtop;
	HGLOBAL hG;
	size_t size = 0;
	int match;
	DWORD oldhashlen = 0;

	if ( OpenClipboardCheck(cinfo) == FALSE ) return 0;

#ifdef UNICODE
	hG = GetClipboardData(CF_UNICODETEXT);
#else
	hG = GetClipboardData(CF_TEXT);
#endif
	if ( hG != NULL ) {
		size = GlobalSize(hG);
		if ( size < sizeof(ClipBoardText) ){
			memcpy(ClipBoardText, GlobalLock(hG), size);
			ClipBoardText[size / sizeof(TCHAR)] = '\0';
		}
	}
	CloseClipboard();
	if ( size == 0 ) return 0;

	match = 0;
	for ( ptr = ClipBoardText; ; ptr++ ){ // テキスト中のハッシュを探す
		TCHAR c, *type;
		DWORD hashlen;

		c = *ptr;
		if ( c == '\0' ) break;
		if ( !Isxdigit(c) ) continue;
		hashtop = ptr++;
		hashlen = 1;
		for ( ;; ){ // 16進文字列の長さを求める
			c = *ptr;
			if ( !Isxdigit(c) ) break;
			ptr++;
			hashlen++;
		}
		switch (hashlen){
			case 8:
				type = T("crc32");
				break;

			case 32:
				type = T("md5");
				break;

			case 40:
				type = T("sha1");
				break;

			case 56:
				type = T("sha224");
				break;

			case 64:
				type = T("sha256");
				break;

			default: // 該当するものが無かった
				continue;
		}
		if ( comparemode ){ // 各エントリと比較
			if ( hashlen != oldhashlen ){
				if ( CommentCommand(cinfo, type) != NO_ERROR ) return 0;
				oldhashlen = hashlen;
			}
			match += CompareHashMain(cinfo, hashtop, hashlen);
		}else{ // ハッシュ検出のみ
			return hashlen;
		}
	}
	if ( comparemode ){
		wsprintf(ClipBoardText, T("match: %d"), match);
		SetPopMsg(cinfo, POPMSG_MSG, ClipBoardText);
	}
	return 0;
}

//--------------------------------------------------------------- 処理選択&準備
ERRORCODE PPcCompare(PPC_APPINFO *cinfo, int mode)
{
	HWND hPairWnd;
	int mode_type;

	hPairWnd = GetPairWnd(cinfo);
	if ( (mode & CMPTYPEMASK) == 0 ){ // 方法指定無し…メニュー選択
		HMENU hMenu;

		hMenu = CreatePopupMenu();
		if ( hPairWnd ){
			COMPAREMENUSTRUCT *cm;

			for ( cm = CompareMenu ; cm->mode ; cm++ ){
				if ( cm->mode & CMPWITHOUT_EXT ){
					AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				}
				AppendMenuString(hMenu, cm->mode, cm->name);
			}
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		}
		AppendMenuString(hMenu, CMP_1SIZE, MES_CMPZ);
		AppendMenuString(hMenu, CMP_1COMMENT, MES_CMPM);
		AppendMenu(hMenu,
				(CompareHashFromClipBoard(cinfo, 0) != 0) ? MF_ES : MF_GS,
				CMP_1HASH, MessageText(StrMesCMPH));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuString(hMenu, CMP_DETAIL, MES_CMPD);
		mode = PPcTrackPopupMenu(cinfo, hMenu);
		DestroyMenu(hMenu);
	}
	if ( mode == CMP_1HASH ){
		CompareHashFromClipBoard(cinfo, 1);
		return NO_ERROR;
	}else if ( mode == CMP_DETAIL ){
		mode = CompareDetailDialog(cinfo->info.hWnd, hPairWnd);
		if ( mode < 0 ) mode = 0;
	}else if ( mode ){
		if ( !(mode & (CMPMARKMASK << CMPMARKSHIFT)) ) mode |= 0x700; // 属性補正
	}
	mode_type = mode & CMPTYPEMASK;
	if ( mode_type >= CMP_2WINDOW ){
		COMPAREMARKPACKET cmp;

		if ( hPairWnd == NULL ) return ERROR_CANCELLED;
		if ( (mode_type == CMP_BINARY) || (mode_type == CMP_SHA1) ){
			SetPopMsg(cinfo, POPMSG_PROGRESSBUSYMSG, MES_SCMP);
		}
		cmp.mode = mode;
		MakeTempEntry(MAX_PATH, cmp.filename, FILE_ATTRIBUTE_NORMAL);
		PPcCompareSend(cinfo, &cmp, hPairWnd);
		return NO_ERROR;
	}
	if ( mode_type >= CMP_1WINDOW ){
		PPcCompareOneWindow(cinfo, mode);
		return NO_ERROR;
	}
	return ERROR_CANCELLED;
}

int GetCMarkMode(HWND hDlg)
{
	int mode = (int)SendMessage(hDlg, CB_GETCURSEL, 0, 0);
	mode += (mode >= CMP2TYPES) ? (CMP_1WINDOW - CMP2TYPES) : CMP_2WINDOW;
	if ( mode >= CMP_2END ){
		mode = CMP_EXIST | CMPWITHOUT_EXT;
	}
	return mode;
}

int GetCMarkSettings(HWND hDlg)
{
	int mode = GetCMarkMode(GetDlgItem(hDlg, IDC_TYPE));

	mode |= (GetAttibuteSettings(hDlg) << CMPMARKSHIFT) |
		(IsDlgButtonChecked(hDlg, IDX_WHERE_SDIR) ?
				(CMPSUBDIR | (FILE_ATTRIBUTE_DIRECTORY << CMPMARKSHIFT)) : 0);
	return mode;
}

//  --------------------------------------------------------------
INT_PTR CALLBACK CompareDetailDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			const TCHAR **dmp = CompareDetailMenu;

			CenterWindow(hDlg);
			LocalizeDialogText(hDlg, IDD_CMARK);
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);

			if ( (HWND)lParam == NULL ){ // 反対なし？→２窓系をスキップ
				dmp += CMP2TYPES;
			}

			for ( ; *dmp != NULL ; dmp++ ){
				SendDlgItemMessage(hDlg, IDC_TYPE,
						CB_ADDSTRING, 0, (LPARAM)MessageText(*dmp) );
			}

			SendDlgItemMessage(hDlg, IDC_TYPE, CB_SETCURSEL, 0, 0);
			SetAttibuteSettings(hDlg, 0x27);
			EnableDlgWindow(hDlg, IDX_WHERE_SDIR, 0);
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					EndDialog(hDlg, GetCMarkSettings(hDlg));
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDC_TYPE:
					if ( HIWORD(wParam) == CBN_SELCHANGE ){
						int mode = GetCMarkMode((HWND)lParam);
						EnableDlgWindow(hDlg, IDX_WHERE_SDIR,
								(mode == CMP_EXIST) ||
								(mode == CMP_BINARY) ||
								(mode == CMP_SHA1)
						);
					}
					break;

//				case IDB_SAVE:
//					SaveSortSettings(hDlg);
//					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

ERRORCODE CompareMarkEntry(PPC_APPINFO *cinfo, const TCHAR *param)
{
	int mode;
	DWORD attr;

	mode = GetIntNumber(&param); // モード
	NextParameter(&param);
	attr = GetDwordNumber(&param); // 属性
	NextParameter(&param);
	if ( GetNumber(&param) ) setflag(mode, CMPSUBDIR);
	return PPcCompare(cinfo, mode | (attr << 8) | CMPWAIT);
}
