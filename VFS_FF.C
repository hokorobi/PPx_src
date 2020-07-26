/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				～ FindFirst/Next ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONVFSDLL		// VFS.H の DLL export 指定
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "PPX_64.H"
#include "VFS.H"
#include "VFS_STRU.H"
#define DefxIShellFolder2
#include "VFS_FF.H"
#pragma hdrstop

const DWORD CDoffset[] = { CDHEADEROFFSET1, CDHEADEROFFSET2, CDHEADEROFFSET3, MAX32 };

#define TypeName_Normal NilStr
const TCHAR TypeName_stream[] = VFSFFTYPENAME_STREAM;
const TCHAR TypeName_HTML[] = VFSFFTYPENAME_HTML;
const TCHAR TypeName_FAT[] = VFSFFTYPENAME_FAT;
const TCHAR TypeName_CD[] = VFSFFTYPENAME_CD;
const TCHAR TypeName_ZIPfolder[] = VFSFFTYPENAME_ZIPfolder;
const TCHAR TypeName_LHAfolder[] = VFSFFTYPENAME_LHAfolder;
const TCHAR TypeName_CABfolder[] = VFSFFTYPENAME_CABfolder;
const TCHAR TypeName_DriveList[] = VFSFFTYPENAME_DriveList;
const TCHAR TypeName_FTP[] = VFSFFTYPENAME_FTP;
const TCHAR TypeName_root[] = VFSFFTYPENAME_root;
const TCHAR TypeName_Network[] = VFSFFTYPENAME_Network;
const TCHAR TypeName_LongPath[] = VFSFFTYPENAME_LongPath;
const TCHAR TypeName_SHN[] = VFSFFTYPENAME_SHN;

const TCHAR *TypeName_Disk[] = { TypeName_FAT, VFSFFTYPENAME_exFAT, VFSFFTYPENAME_NTFS };

HANDLE AuxOpFF(const TCHAR *Fname, WIN32_FIND_DATA *findfile)
{
	ERRORCODE result;
	TCHAR cmdstr[CMDLINESIZE], path[VFPS];
	HANDLE hFF;

	for (;;){
		tstrcpy(path, Fname + 4);
		result = AuxOperation(NULL, T("list"), path, NilStr, NilStr, cmdstr);
		if ( result != NO_ERROR ){
			SetLastError(result);
			return INVALID_HANDLE_VALUE;
		}

		if ( cmdstr[0] == '\0' ){
			SetLastError(ERROR_INVALID_DRIVE);
			return INVALID_HANDLE_VALUE;
		}
		CatPath(NULL, cmdstr, T("*"));
		hFF = VFSFindFirst(cmdstr, findfile);
		if ( hFF != INVALID_HANDLE_VALUE ) return hFF;
		result = GetLastError();
		if ( (result == ERROR_INVALID_PASSWORD) && (AuthHostCache[0] != '\0') ){
			AuthHostCache[0] = '\1';
			AuthHostCache[1] = '\0';
			continue;
		}
		SetLastError(result);
		return INVALID_HANDLE_VALUE;
	}
}

#define CP_UNDLL 0x106ffff
BOOL FFCheckUndll(VFSFINDFIRST *VFF, VUCHECKSTRUCT *vcs, int floatheader, const TCHAR *DllName)
{
	UN_DLL *uD;
	int i;

	vcs->floatheader = floatheader;
	vcs->header += floatheader; // float の場合は、少し後ろから検索
	uD = undll_list;
	for ( i = 0 ; i < undll_items ; i++, uD++ ){
		if ( uD->flags & UNDLLFLAG_DISABLE_DIR ) continue;
		if ( (DllName != NULL) && tstricmp(uD->DllName, DllName) ) continue;
 		if ( uD->VUCheck(uD, vcs) == FALSE ) continue;
		VFF->v.UN.uD = uD;
		#ifdef UNICODE
		if ( (uD->UnOpenArchiveW != NULL) && (UnDllCodepage == CP_ACP) ){
			VFF->v.UN.Ha =
				uD->UnOpenArchiveW(GetFocus(), vcs->filenameW, M_INIT_FILE_USE);
			if ( VFF->v.UN.Ha == NULL ) continue;
			uD->codepage = CP_UNDLL;
		}else{
			const char *name;

			if ( uD->SetUnicodeMode(TRUE) && (UnDllCodepage == CP_ACP) ){
				uD->codepage = CP_UTF8;
				name = vcs->filename8;
			}else{
				uD->codepage = UnDllCodepage;
				name = vcs->filename;
			}
			VFF->v.UN.Ha = uD->UnOpenArchive(GetFocus(), name, M_INIT_FILE_USE);
			if ( VFF->v.UN.Ha == NULL ){
				VFF->v.UN.uD->SetUnicodeMode(FALSE);
				continue;
			}
		}
		#else
			VFF->v.UN.Ha =
				uD->UnOpenArchive(GetFocus(), vcs->filename, M_INIT_FILE_USE);
			if ( VFF->v.UN.Ha == NULL ) continue;
		#endif
		return TRUE;
	}
	return FALSE;
}

void USEFASTCALL SetDummyFindData(WIN32_FIND_DATA *findfile)
{
	findfile->ftCreationTime.dwLowDateTime		= 0;
	findfile->ftCreationTime.dwHighDateTime		= 0;
	findfile->ftLastAccessTime.dwLowDateTime	= 0;
	findfile->ftLastAccessTime.dwHighDateTime	= 0;
	findfile->ftLastWriteTime.dwLowDateTime		= 0;
	findfile->ftLastWriteTime.dwHighDateTime	= 0;
	findfile->nFileSizeHigh	= 0;
	findfile->nFileSizeLow	= 0;
	findfile->cAlternateFileName[0] = '\0';
}

void USEFASTCALL SetDummydir(WIN32_FIND_DATA *findfile, const TCHAR *name)
{
	SetDummyFindData(findfile);
	findfile->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	tstrcpy(findfile->cFileName, name);
}

int USEFASTCALL FFStepInfo(WIN32_FIND_DATA *findfile, int *step)
{
	if ( *step >= FFSTEP_ENTRY ) return -1;
	if ( *step <= FFSTEP_NOMORE ){
		SetLastError(ERROR_NO_MORE_FILES);
		return 0;
	}
	SetDummydir(findfile, (*step == FFSTEP_THIS) ? T(".") : T("..") );
	(*step)++;
	return 1;
}

void AddDriveList(ThSTRUCT *dirs, const TCHAR *name)
{
	const TCHAR *lp;
												// ダブリがないか検索
	lp = (const TCHAR *)dirs->bottom;
	while ( *lp != '\0' ){
		if ( tstricmp(name, lp) == 0 ) break;
		lp += tstrlen(lp) + 1;
	}
	if ( *lp  != '\0' ) return;
											// 保存
	ThAddString(dirs, name);
}

/*-----------------------------------------------------------------------------
char	*FindPathS(char *src)

	先頭から最初に現れる「\」の位置を知らせる。
	無いなら文字列の末尾

	src			検索を始める位置

戻り値
	(char *)	「\」を示すか、NULL ターミネータの位置
-----------------------------------------------------------------------------*/
char *FindPathS(char *src)
{
	while ( (*src) && (*src != '\\') ){
		#ifndef UNICODE
			if (IskanjiA(*src++)){
				if ( *src ) src++;
			}
		#else
			src++;
		#endif
	}
	return src;
}
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VFSFINDFIRST * USEFASTCALL IsVFSFINDFIRST(HANDLE hFF)
{
	VFSFINDFIRST *VFF;

	VFF = (VFSFINDFIRST *)hFF;
//	if ( IsTrue(IsBadWritePtr(VFF, sizeof(VFSFINDFIRST))) ) return NULL;
	if ( VFF->ID != VFSFINDFIRSTID ) return NULL;
	return VFF;
}
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VFSDLL void PPXAPI VFSGetFFInfo(HANDLE hFF, int *mode, TCHAR *type, void **dt_opt)
{
	VFSFINDFIRST *VFF;
	TCHAR *p;

	VFF = IsVFSFINDFIRST(hFF);

	switch( VFF->mode ){
		case VFSDT_LFILE:
			if ( (INT_PTR)mode <= 4 ){
				const TCHAR *ptr;
				switch ( (INT_PTR)mode ){
					case 2: ptr = VFF->v.LFILE.search; break;
					case 3: ptr = VFF->v.LFILE.sort; break;
					case 4: ptr = VFF->v.LFILE.view; break;
//					case 1:
					default:
						ptr = VFF->v.LFILE.base; break;
				}
				tstrcpy(type, ptr);
				return;
			}
			*dt_opt = VFF->v.LFILE.readptr;
			break;
		case VFSDT_SUSIE:
			*dt_opt = (void *)(size_t)(VFF->v.SU.su - susie_list + 1);
			break;
		case VFSDT_UN:
			*dt_opt = (void *)(size_t)(VFF->v.UN.uD - undll_list + 1);
			break;
		default:
			*dt_opt = NULL;
	}
	*mode = VFF->mode;

	p = tstrlimcpy(type, VFF->TypeName, VFSGETFFINFO_TYPESIZE - 1);
	if ( p != type ){
		*p = ' ';
		*(p + 1) = '\0';
	}
}

const TCHAR FDResPubString[] = T("FDResPub");

FF_MC *InitMCsubdir(VFSFINDFIRST *VFF)
{
	FF_MC *mc;

	VFF->mode = VFSDT_DLIST;
	mc = &VFF->v.MC;
	ThInit(&mc->dirs);
	ThInit(&mc->files);
	mc->f_off = 0;
	mc->d_off = 0;
	ThAddString(&mc->dirs, T("."));
	ThAddString(&mc->dirs, T(".."));
	return mc;
}

// Vista 以降の PC 一覧( \\ )
ERRORCODE FindUncPClist(VFSFINDFIRST *VFF, WIN32_FIND_DATA *findfile)
{
	FF_MC *mc;
	TCHAR textbuf[VFPS];
	int index = 0;

	// ネットワーク探索が有効かを判断
	SC_HANDLE hService;
	SC_HANDLE hSCManager;
	SERVICE_STATUS fdinfo;

	fdinfo.dwCurrentState = SERVICE_RUNNING;
	hSCManager = OpenSCManager(NULL, NULL, GENERIC_READ);
	if ( hSCManager != NULL ){
		hService = OpenService(hSCManager, FDResPubString, SERVICE_QUERY_STATUS);
		if ( hService != NULL ){
			QueryServiceStatus(hService, &fdinfo);
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(hSCManager);
	}
	// 有効
	if ( fdinfo.dwCurrentState != SERVICE_STOPPED ) return ERROR_DIRECTORY;

	// ネットワーク探索が無効なので独自処理
	mc = InitMCsubdir(VFF);
	VFF->TypeName = TypeName_Network;

	ThAddString(&mc->dirs, T("+"));	// 拡張 PC 一覧

	((DWORD *)textbuf)[3] = 0;
	GetCustData(StrX_dlf, textbuf, sizeof(DWORD) * 4);
	if ( !(((DWORD *)textbuf)[3] & XDLF_NODISPSHARE) ){
									// ヒストリからPC名を抽出
		UsePPx();
		for ( ; ; ){
			const TCHAR *hisp;
			TCHAR *p;
			int mode;

			hisp = EnumHistory(PPXH_PPCPATH, index++);
			if ( hisp == NULL ) break;

			p = VFSGetDriveType(hisp, &mode, NULL);
			// UNC 以外 / 「\\」は無視
			if ( (p == NULL) || (mode != VFSPT_UNC) || (*p == '\0') ) continue;

			tstrcpy(textbuf, hisp + 2); // 「\\」をスキップ
			p = FindPathSeparator(p);
			if ( p != NULL ){
				p = textbuf + (p - (hisp + 2));
				*p = '\0';
			}
			AddDriveList(&mc->dirs, textbuf);
		}
		FreePPx();
	}

	ThAddString(&mc->files, NilStr);

	VFSFindNext((HANDLE)VFF, findfile);
	return NO_ERROR;
/*
ヒストリ列挙

ad pc 列挙
	tls.password[0] = '\0';
	GetRegString(HKEY_LOCAL_MACHINE, DomainNameRegPathString, DomainNameRegNameString, tls.password, TSIZEOF(tls.password));
	if ( tls.password[0] != '\0' ){
		TCHAR *src, *dest;


				// f_dis 開始していなければ、ここで止める

			}

*/
}

#define FDHEADERSIZE	VFS_check_size
#define FDHEADERMARGIN	0x100

// ディレクトリ化ファイル ----
// ERROR_BAD_PATHNAME	ファイルでなかった
// ERROR_FILE_EXISTS	形式が分かったけど、完全でないファイル
// ERROR_INVALID_DRIVE	形式が分からなかったファイル
ERRORCODE FileDirectory(TCHAR *fname, VFSFINDFIRST *VFF, WIN32_FIND_DATA *findfile, const TCHAR *fwild, TCHAR *DllName)
{
	HANDLE hFile;
	BYTE header[FDHEADERSIZE + FDHEADERMARGIN];
	DWORD fsize;
	TCHAR dir[VFPS], *subdir = T("");

	hFile = CreateFileL(fname, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		for ( ; ; ){				// 高速検索
			tstrcpy(dir, fname);
			subdir = tstrchr(dir, '.');
			if ( subdir == NULL ) break;
			subdir = VFSFindLastEntry(subdir);
			if ( *subdir != '\\' ) break;
			*subdir = '\0';
			subdir++;
			hFile = CreateFileL(dir, GENERIC_READ,
					FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			break;
		}
		if ( hFile == INVALID_HANDLE_VALUE ){	// 完全検索
			tstrcpy(dir, fname);
			for ( ; ; ){
				subdir = VFSFindLastEntry(dir);
				if ( *subdir == '\0' ) return ERROR_BAD_PATHNAME; // ファイルでない
				*subdir = '\0';
				hFile = CreateFileL(dir, GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
				if ( hFile != INVALID_HANDLE_VALUE ) break;
			}
			subdir = fname + (subdir - dir);
			if ( *subdir == '\\' ) subdir++;
		}
		fname = dir;
	}
	if ( DllName != NULL ){
		subdir = FindPathSeparator(DllName);
		if ( subdir != NULL ){
			*subdir++ = '\0';
		}else{
			subdir = T("");
		}

		if ( tstrcmp(DllName, T("stream")) == 0 ){
			FindFirstStream(hFile, &VFF->v.STREAM, fname);
			VFF->mode = VFSDT_STREAM;
			VFF->TypeName = TypeName_stream;
			VFSFindNext((HANDLE)VFF, findfile);
			return NO_ERROR;
		}
	}
	fsize = ReadFileHeader(hFile, header, FDHEADERSIZE);
	CloseHandle(hFile); // ネットワークだと、ここで時間が掛かることがある
										// List File --------------------------
	if ( !memcmp(header, LHEADER, sizeof(LHEADER) - 1) ||
		 !memcmp(header, UTF8HEADER LHEADER, sizeof(LHEADER) - 1 + 3) ||
		 !memcmp(header + 2, L";ListFile\r\n",
		 					sizeof(L";ListFile\r\n") - sizeof(WCHAR)) ){
		if ( *subdir != '\0' ) return ERROR_INVALID_DRIVE;
		return InitFindFirstListFile(VFF, fname, findfile);
	}
										// html File --------------------------
	if ( *header == '<' ){
		BYTE *p;
		DWORD i;

		p = header + 1;
		for ( i = 0 ; i < min(1000, fsize) ; i++, p++ ){
			if ( (*p < 0x20) || (*p > 0x7f) ) break;
			if ( *p == '>' ){
				if ( MakeWebList(&VFF->v.MC, fname, TRUE) != NO_ERROR ){
					return ERROR_FILE_EXISTS;
				}
				VFF->mode = VFSDT_HTTP;
				VFF->TypeName = TypeName_HTML;
				VFSFindNext((HANDLE)VFF, findfile);
				return NO_ERROR;
			}
		}
	}
									// FAT Image ------------------------------
	if ( (DllName == NULL) || (tstrcmp(DllName, T("disk")) == 0) ){
		const DWORD *o;
		DWORD pc9801drive;

		// ↓wildcardの詳細指定は未サポート
		if ( (*fwild != '\0') && (*fwild != '*') ) return ERROR_FILE_NOT_FOUND;

		for ( o = FDIoffset ; *o != MAX32 ; o++ ){
			int fatresult;

			fatresult = CheckFATImage(header + *o, header + fsize);
			if ( fatresult == CHECKFAT_NONE ) continue;
			VFF->TypeName = TypeName_Disk[fatresult - CHECKFAT_FIRST];
			VFF->mode = VFSDT_FATIMG;

			if ( OpenFATImage(&VFF->v.FAT.fats, fname, *o) == FALSE ){
				return ERROR_NO_MORE_FILES;
			}

			if ( FindEntryFATImage(&VFF->v.FAT.fats, subdir, findfile) == FALSE ){
				CloseFATImage(&VFF->v.FAT.fats);
				return ERROR_NO_MORE_FILES;
			}
			return NO_ERROR;
		}
		if ( (fsize >= 0x2000) && ((pc9801drive = GetPC9801FirstDriveSector(header)) != 0) ){
			VFF->TypeName = TypeName_FAT;
			VFF->mode = VFSDT_FATIMG;
			if ( OpenFATImage(&VFF->v.FAT.fats, fname, pc9801drive) == FALSE ){
				return ERROR_NO_MORE_FILES;
			}

			if ( FindEntryFATImage(&VFF->v.FAT.fats, subdir, findfile) == FALSE ){
				CloseFATImage(&VFF->v.FAT.fats);
				return ERROR_NO_MORE_FILES;
			}
			return NO_ERROR;
		}
	}
									// CD Image ------------------------------
	if ( ((DllName == NULL) || (tstrcmp(DllName, TypeName_CD) == 0)) && (fsize > 38 * 1024) ){
		const DWORD *o;

		for ( o = CDoffset ; *o != MAX32 ; o++ ){
			if ( memcmp( (char *)header + *o, StrISO9660, CDHEADERSIZE) &&
				 memcmp( (char *)header + *o, StrUDF, CDHEADERSIZE) ) continue;
			if ( OpenCDImage(&VFF->v.CD.cds, fname, *o) == FALSE ){
				return ERROR_NO_MORE_FILES;
			}
			VFF->mode = VFSDT_CDIMG;
			VFF->TypeName = TypeName_CD;
			if ( FindEntryCDImage(&VFF->v.CD.cds, subdir, findfile) == FALSE ){
				CloseCDImage(&VFF->v.CD.cds);
				return ERROR_NO_MORE_FILES;
			}
			if ( (*fwild != '\0') && (*fwild != '*') ) for (;;){
				if ( IsTrue(FindEntryCDImage(&VFF->v.CD.cds, NULL, findfile)) ){
					if ( tstricmp(findfile->cFileName, fwild) == 0 ) break;
				}else{
					CloseCDImage(&VFF->v.CD.cds);
					return ERROR_NO_MORE_FILES;
				}
			}
			return NO_ERROR;
		}
	}
	if ( *subdir == '\0' ){ // 以下、サブディレクトリがないことが限定のDLL
										// UnDll ------------------------------
		VUCHECKSTRUCT vcs;

		#ifdef UNICODE
			char UTemp[VFPS];
			#define TFILENAME UTemp
			vcs.filenameW = fname;
			UnicodeToAnsi(fname, UTemp, VFPS);
			WideCharToMultiByteU8(CP_UTF8, 0, fname, -1, vcs.filename8, VFPS, NULL, NULL);
		#else
			#define TFILENAME fname
		#endif

		vcs.filename = TFILENAME;
		vcs.header = header;
		vcs.fsize = fsize;
		#undef TFILENAME

		if ( IsTrue(FFCheckUndll(VFF, &vcs, 0, DllName)) ||
			 IsTrue(FFCheckUndll(VFF, &vcs, 1, DllName)) ){
			VFF->mode = VFSDT_UN;
			VFF->TypeName = VFF->TypeNameBuf;
			#ifdef UNICODE
				if ( VFF->v.UN.uD->UnFindFirstW != NULL ){
					strcpyW(VFF->v.UN.wild, fwild);
				}else{
					UnicodeToAnsi(fwild, (char *)VFF->v.UN.wild, sizeof(VFF->v.UN.wild));
				}
				wcslimcpy(VFF->TypeNameBuf, VFF->v.UN.uD->DllName, VFSGETFFINFO_TYPESIZE - 1);
			#else
				strcpy(VFF->v.UN.wild, fwild);
				strlimcpy(VFF->TypeNameBuf, VFF->v.UN.uD->DllName, VFSGETFFINFO_TYPESIZE - 1);
			#endif
			VFF->v.UN.step = FFSTEP_THIS;
			FFStepInfo(findfile, &VFF->v.UN.step);
			return NO_ERROR;
		}
										// Susie ------------------------------
		{
			THREADSTRUCT *ts;
			const TCHAR *OldTname;
			TCHAR *fp;
			SUSIE_DLL *sudll;
			int i;
			#ifdef UNICODE
				char STemp[VFPS];
				#define TFILENAME STemp
				UnicodeToAnsi(fname, STemp, VFPS);
			#else
				#define TFILENAME fname
			#endif

			ts = GetCurrentThreadInfo();
			if ( ts != NULL ) OldTname = ts->ThreadName;

			fp = FindLastEntryPoint(fname);
			sudll = susie_list;
			for ( i = 0 ; i < susie_items ; i++, sudll++ ){
				int s;

				if ( DllName == NULL ){
					if ( (sudll->flags & (VFSSUSIE_ARC | VFSSUSIE_NOAUTODETECT)) !=
						VFSSUSIE_ARC ){
						continue;
					}
				}else if ( !(sudll->flags & VFSSUSIE_ARC) ||
					tstricmp((TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset), DllName) ){
					continue;
				}
				if ( CheckAndLoadSusiePlugin(sudll, fp, ts, VFS_DIRECTORY) == FALSE){
					continue;
				}
				#ifdef UNICODE
					if ( sudll->IsSupportedW != NULL ){
						if ( !sudll->IsSupportedW(fname, header) ) continue;
					}else{
						if ( !sudll->IsSupported(TFILENAME, header) ) continue;
					}
					VFF->v.SU.finfoW = NULL;
				#else
					if ( !sudll->IsSupported(TFILENAME, header) ) continue;
				#endif

				#ifdef UNICODE
					if ( sudll->GetArchiveInfoW != NULL ){
						s = sudll->GetArchiveInfoW(fname, 0, SUSIE_SOURCE_DISK, (HLOCAL *)&VFF->v.SU.fiH);
						if ( s != 0 ) continue;

						VFF->v.SU.finfo = NULL;
						VFF->v.SU.finfoW = LocalLock(VFF->v.SU.fiH);
						VFF->v.SU.finfomaxW = (SUSIE_FINFOW *)(BYTE *)((BYTE *)VFF->v.SU.finfoW + LocalSize(VFF->v.SU.fiH));
					}
					if ( VFF->v.SU.finfoW == NULL )
				#endif
				{
					s = sudll->GetArchiveInfo(TFILENAME, 0, SUSIE_SOURCE_DISK, (HLOCAL *)&VFF->v.SU.fiH);
					if ( (s != 0) && (s != 2) ) continue; //2:LHASAD.SPIでの正常値
					VFF->v.SU.finfo = LocalLock(VFF->v.SU.fiH);
					if ( VFF->v.SU.finfo == NULL ) continue;
					VFF->v.SU.finfomax = (SUSIE_FINFO *)(BYTE *)((BYTE *)VFF->v.SU.finfo + LocalSize(VFF->v.SU.fiH));
				}

				VFF->mode = VFSDT_SUSIE;
				VFF->v.SU.su = sudll;

				VFF->TypeName = VFF->TypeNameBuf;
				#ifdef UNICODE
					wcslimcpy(VFF->TypeNameBuf, (WCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset), VFSGETFFINFO_TYPESIZE - 1);
				#else
					strlimcpy(VFF->TypeNameBuf, Thsusie_str.bottom + sudll->DllNameOffset, VFSGETFFINFO_TYPESIZE - 1);
				#endif

				VFF->v.SU.step = FFSTEP_THIS;
				FFStepInfo(findfile, &VFF->v.SU.step);

				if ( ts != NULL ) ts->ThreadName = OldTname;
				return NO_ERROR;
			}
			if ( ts != NULL ) ts->ThreadName = OldTname;
			#undef TFILENAME
		}
	}
									// ZIP (zipflder.dll) ---------------------
	if ( (DllName == NULL) || (tstrcmp(DllName, zipfldrName) == 0) ){
		if ( (DllName != NULL) || ((header[0] == 'P') && (header[1] == 'K') && (header[2] == 0x3)) ){
			if ( IsTrue(ZipFolderFF(&VFF->v.ZIPO, fname, subdir, findfile, 0)) ){
				VFF->mode = VFSDT_ZIPFOLDER;
				VFF->TypeName = TypeName_ZIPfolder;
				return NO_ERROR;
			}
		}
	}
									// LZH (lzhfldr2.dll) ---------------------
	if ( (DllName == NULL) || (tstrcmp(DllName, lzhfldrName) == 0) ){
		if ( (header[2] == '-') && (header[3] == 'l') && (header[4] == 'h') ){
			if ( IsTrue(ZipFolderFF(&VFF->v.ZIPO, fname, subdir, findfile, 1)) ){
				VFF->mode = VFSDT_LZHFOLDER;
				VFF->TypeName = TypeName_LHAfolder;
				return NO_ERROR;
			}
		}
	}
									// CAB (cabview.dll) ---------------------
	if ( (DllName == NULL) || (tstrcmp(DllName, cabfldrName) == 0) ){
		if ( (header[0] == 'M') && (header[1] == 'S') && (header[2] == 'C') ){
			if ( IsTrue(ZipFolderFF(&VFF->v.ZIPO, fname, subdir, findfile, 2)) ){
				VFF->mode = VFSDT_CABFOLDER;
				VFF->TypeName = TypeName_CABfolder;
				return NO_ERROR;
			}
		}
	}
									// CD/DVD Image --------------------------
	if ( ((DllName == NULL) || (tstrcmp(DllName, TypeName_CD) == 0)) && (fsize > 38 * 1024) && (*(DWORD *)header == 0) ){
		hFile = CreateFileL(fname, GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( MAX32 == SetFilePointer(hFile, CDHEADEROFFSET4, NULL, FILE_BEGIN) ){
			CloseHandle(hFile);
		}else{
			CloseHandle(hFile);

			if ( OpenCDImage(&VFF->v.CD.cds, fname, CDHEADEROFFSET4) == FALSE ){
				return ERROR_NO_MORE_FILES;
			}
			VFF->mode = VFSDT_CDIMG;
			VFF->TypeName = TypeName_CD;
			if ( FindEntryCDImage(&VFF->v.CD.cds, subdir, findfile)==FALSE){
				CloseCDImage(&VFF->v.CD.cds);
				return ERROR_NO_MORE_FILES;
			}
			return NO_ERROR;
		}
	}
	return ERROR_INVALID_DRIVE;
}
/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
VFSDLL HANDLE PPXAPI VFSFindFirst(const TCHAR *dir, WIN32_FIND_DATA *findfile)
{
	VFSFINDFIRST *VFF;
	ERRORCODE errorcode;
	ERRORCODE realerror = NO_ERROR; // GNC/UNCが可能:NO_ERROR / その結果
	// ERROR_BADKEY : FindFirstFile を試さず、SHN を直接使用する (通常でないエラーを利用している)
	TCHAR Fname[VFPS], *Fwild;
	int mode, force;
	TCHAR *vp;

										// 検索対象の種類を判別 ---------------
	tstrcpy(Fname, dir);
	vp = VFSGetDriveType(Fname, &mode, &force);
	if ( vp == NULL ){		// 種類が分からない→相対指定の可能性→絶対化
		VFSFullPath(Fname, (TCHAR *)dir, NULL);
		vp = VFSGetDriveType(Fname, &mode, &force);
		if ( vp == NULL ){	// それでも種類が分からない→エラー
			errorcode = ERROR_BAD_PATHNAME;
			goto error_nofree;
		}
	}
										// 検索用ハンドルを作成 ---------------
	VFF = HeapAlloc( DLLheap, 0, sizeof(VFSFINDFIRST) );
	if ( VFF == NULL ){
		errorcode = ERROR_NOT_ENOUGH_MEMORY;
		goto error_nofree;
	}
	VFF->ID = VFSFINDFIRSTID;
/*
	if ( func == NULL ){
		VFF->callback = DefaultCallback;
	}else{
		VFF->callback = func;
		VFF->funcwork = funcwork;
	}
*/
										// ファイルマスクを分離 ---------------
	Fwild = FindPathSeparator(Fname);
	if ( Fwild == NULL ) Fwild = Fname;
	for ( ; ; ){
		TCHAR *p;

		p = FindPathSeparator(Fwild + 1);
		if ( p == NULL ) break;
		Fwild = p;
	}
	*Fwild++ = '\0';
										// 種類別の処理 -----------------------
	switch (mode){
		case VFSPT_RAWDISK: {	// "#A:" を "\\.\A:" に変換
			TCHAR readdrive[8];

			wsprintf(readdrive, T("\\\\.\\%c:"), *(vp - 2));
			if ( IsTrue(OpenFATImage(&VFF->v.FAT.fats, readdrive, 0)) ){
				VFF->mode = VFSDT_FATDISK;
				VFF->TypeName = TypeName_FAT;
				while ( *vp == '\\' ) vp++;
				if ( FindEntryFATImage(&VFF->v.FAT.fats, vp, findfile) == FALSE ){
					errorcode = *vp ? ERROR_PATH_NOT_FOUND : ERROR_FILE_NOT_FOUND;
					CloseFATImage(&VFF->v.FAT.fats);
					goto error;
				}
				return (HANDLE)VFF;
			}
			if ( IsTrue(OpenCDImage(&VFF->v.CD.cds, readdrive, CDHEADEROFFSET1)) ){
				VFF->mode = VFSDT_CDDISK;
				VFF->TypeName = TypeName_CD;
				while ( *vp == '\\' ) vp++;
				if ( FindEntryCDImage(&VFF->v.CD.cds, vp, findfile) == FALSE){
					errorcode = *vp ? ERROR_PATH_NOT_FOUND : ERROR_FILE_NOT_FOUND;
					CloseCDImage(&VFF->v.CD.cds);
					goto error;
				}
				return (HANDLE)VFF;
			}
			errorcode = ERROR_NO_MORE_FILES;
			goto error;
		}

		case VFSPT_DRIVELIST:
			VFF->mode = VFSDT_DLIST;
			VFF->TypeName = TypeName_DriveList;
			MakeDriveList(&VFF->v.MC);
			VFSFindNext((HANDLE)VFF, findfile);
			return (HANDLE)VFF;

		case VFSPT_HTTP:
			VFF->mode = VFSDT_HTTP;
			VFF->TypeName = TypeName_HTML;
			errorcode = MakeWebList(&VFF->v.MC, Fname, FALSE);
			if ( errorcode != NO_ERROR ) goto error;
			VFSFindNext((HANDLE)VFF, findfile);
			return (HANDLE)VFF;

		case VFSPT_AUXOP:
			HeapFree( DLLheap, 0, VFF );
			return AuxOpFF(Fname, findfile);

		case VFSPT_FTP:
			VFF->mode = VFSDT_FTP;
			VFF->TypeName = TypeName_FTP;
			errorcode = FTPff(&VFF->v.FTP, dir, findfile);
			if ( errorcode != NO_ERROR ) goto error;
			return (HANDLE)VFF;
#ifdef USESLASHPATH
		case VFSPT_SLASH:
			VFF->mode = VFSDT_SLASH;
			VFF->TypeName = TypeName_root;
#ifdef WINEGCC
			errorcode = FirstSlashDir(&VFF->v.SLASH, Fname, findfile);
#else
			errorcode = FirstSlashDir(&VFF->v.SLASH, dir, findfile);
#endif
			if ( errorcode != NO_ERROR ) goto error;
			return (HANDLE)VFF;
#endif
		case VFSPT_DRIVE:
#ifdef USESLASHPATH
#ifdef WINEGCC
			if ( (*(vp - 2) == 'Z') || (*(vp - 2) == 'z') ){
				TCHAR *ps;

				VFF->mode = VFSDT_SLASH;
				VFF->TypeName = TypeName_root;
				if ( *vp == '\0' ){
					*vp = '/';
					*(vp + 1) = '\0';
				}else{
					ps = vp;
					while ( (ps = FindPathSeparator(ps)) != NULL) *ps++ = '/';
				}
				errorcode = FirstSlashDir(&VFF->v.SLASH, vp, findfile);
				if ( errorcode == NO_ERROR ) return (HANDLE)VFF;
				if ( errorcode != ERROR_DIRECTORY ) goto error;
			}
#endif
#endif
			if ( force != VFSPTF_SHN ){
				vp -= 2;
				break;
			}
										// 「x:\...」の idl を入手する
			if ( !*vp ){	// rootを示す「\」が無いなら追加
				*vp = '\\';
				*(vp + 1) = '\0';
			}
			vp -= 2;
			realerror = ERROR_BADKEY; // FindFirstFile のトライをスキップ
			break;

		case VFSPT_UNC:
									// 「\\」 PC一覧
			if ( Fwild <= vp ){
				if ( WinType >= WINTYPE_VISTA ){
					errorcode = FindUncPClist(VFF, findfile);
					if ( errorcode == NO_ERROR ) return (HANDLE)VFF;
					if ( errorcode != ERROR_DIRECTORY ) goto error;
				}
				memcpy(Fname, T("\\\\\0*.*"), TSTROFF(7));
				vp = Fname;
				Fwild = Fname + 3;
				realerror = ERROR_BADKEY; // FindFirstFile のトライをスキップ
				break;
			}
						// 「\\xxx」PCリソース一覧 / 「\\+」拡張 PC 一覧
			if ( FindPathSeparator(vp) == NULL ){
				if ( (*vp == '+') && (*(vp + 1) == '\0') ){ // 拡張 PC 一覧
					FF_MC *mc;

					mc = InitMCsubdir(VFF);
					VFF->TypeName = TypeName_Network;

					EnumNetServer(&mc->dirs, TRUE);

					ThAddString(&mc->files, NilStr);
					VFSFindNext((HANDLE)VFF, findfile);
					return (HANDLE)VFF;
				}

				// リソース一覧 (SHNパスの場合キャッシュ無しだと数秒待たされる)
				if ( IsTrue(MakeComputerResourceList(&VFF->v.MC, vp - 2)) ){
					VFF->mode = VFSDT_DLIST;
					VFF->TypeName = TypeName_Network;
					VFSFindNext((HANDLE)VFF, findfile);
					return (HANDLE)VFF;
				}
				mode = -1 - CSIDL_NETWORK;
				realerror = ERROR_BADKEY; // FindFirstFile のトライをスキップ
				break;
			}
								// 「\\xxx\xxx\」 一般
			vp -= 2;
			if ( force == VFSPTF_SHN ) realerror = ERROR_BADKEY;
			break;

		case VFSPT_SHELLSCHEME:
			realerror = ERROR_BADKEY; // FindFirstFile のトライをスキップ
			vp = Fname;
			if ( (*vp == '+') || (*vp == '-') ) vp++;
			break;

		default:
			errorcode = ERROR_BAD_PATHNAME;
			if ( mode >= 0 ) goto error;
										// Shell's Namespace ------------------
			realerror = ERROR_BADKEY; // FindFirstFile のトライをスキップ
	}
	if ( realerror == NO_ERROR ){		// GNC/UNC ----------------------------
		*(Fwild - 1) = '\\';
		VFF->mode = VFSDT_PATH;
		VFF->v.FFF.hFF = DFindFirstFile(vp, findfile);
		if ( VFF->v.FFF.hFF != INVALID_HANDLE_VALUE ){	// 成功
			VFF->TypeName = TypeName_Normal;
			return (HANDLE)VFF;
		}
		errorcode = GetLastError();
		if ((errorcode == ERROR_FILENAME_EXCED_RANGE) ||
		   ((errorcode == ERROR_PATH_NOT_FOUND) && (tstrlen(vp) >= MAX_PATH))){
			VFF->v.FFF.hFF = FindFirstFileLs(vp, findfile);
			if (VFF->v.FFF.hFF != INVALID_HANDLE_VALUE){	// 成功
				VFF->TypeName = TypeName_LongPath;
				return (HANDLE)VFF;
			}
			errorcode = GetLastError();
		}
		*(Fwild - 1) = '\0';

		if ((errorcode == ERROR_INVALID_PARAMETER) ||
			(errorcode == ERROR_DIRECTORY) ||
			(errorcode == ERROR_PATH_NOT_FOUND) ||
			(errorcode == ERROR_BAD_NETPATH) ||
			(errorcode == ERROR_INVALID_NAME) ){	// FF に失敗→ ファイル？
			ERRORCODE temperror;
			TCHAR *separator;
			TCHAR *DllName = NULL;

			separator = tstrrchr(vp, ':'); // "::" を検索
			if ( (separator != NULL) &&
				 (separator >= (vp + 2)) &&
				 (*(separator - 1) == ':') ){
				*(separator - 1) = '\0';
													// 強制listfile ?
				if ( !memcmp((TCHAR *)(separator + 1), StrListFileid + 2, TSTROFF(8))){
					ERRORCODE result;

					result = InitFindFirstListFile(VFF, vp, findfile);
					if ( result != NO_ERROR ){
						errorcode = result;
						goto error;
					}
					return (HANDLE)VFF;
				}
				DllName = separator + 1;
				if ( errorcode == ERROR_INVALID_NAME ){
					errorcode = ERROR_DIRECTORY;
				}
			}
			// ERROR_INVALID_NAME は listfileチェックのみ
			if ( errorcode == ERROR_INVALID_NAME ) goto error;

										// FileDirectory ?
			temperror = FileDirectory(Fname, VFF, findfile, Fwild, DllName);
			if ( temperror == NO_ERROR ) return (HANDLE)VFF;
			if ( temperror != ERROR_INVALID_DRIVE ){
				// temperror == ERROR_BAD_PATHNAME は、親で上位階層に移動させる
				if ( temperror == ERROR_BAD_PATHNAME ){
					temperror = ERROR_PATH_NOT_FOUND;
				}
				errorcode = temperror;
				goto error;
			}
			// mode = VFSPT_UNKNOWN; SHNで読めるか試す
		}

		if (	(errorcode != ERROR_BAD_NETPATH) &&
				(errorcode != ERROR_DIRECTORY ) &&
				(errorcode != ERROR_NETWORK_UNREACHABLE) ){
			goto error;
		}
		if ( mode == VFSPT_DRIVE ) goto error;
		realerror = errorcode;
							// 失敗→ Shell's Namespace が使えるか試す
	}
	errorcode = VFSFF_SHN(vp, &VFF->v.SHN, findfile, mode);
	if ( errorcode == NO_ERROR ){
		VFF->mode = VFSDT_SHN;
		VFF->TypeName = TypeName_SHN;
		return (HANDLE)VFF;
	}else if ( (errorcode == ERROR_BAD_PATHNAME)
		&& ((mode < 0) || (mode == VFSDT_SHN)) ){
		TCHAR *p = tstrstr(Fname, T("::"));

		if ( (p != NULL) && (p != Fname) && (*(Fname - 1) != '\\') ){
			*p = '\0';
			if ( IsTrue(VFSGetRealPath(NULL, Fname, Fname)) ){
				p = tstrstr(dir, T("::"));
				if ( p != NULL ) tstrcat(Fname, p);
				HeapFree( DLLheap, 0, VFF );
				return VFSFindFirst(Fname, findfile);
			}
		}
	}
	if ( realerror != ERROR_BADKEY ) errorcode = realerror;
										// エラー終了処理 ---------------------
error:
	HeapFree( DLLheap, 0, VFF );
error_nofree:
	SetLastError(errorcode);
	return INVALID_HANDLE_VALUE;
}

BOOL FindSusieNext(VFSFINDFIRST *VFF, WIN32_FIND_DATA *findfile)
{
	ULONG_PTR tmptime;

	/*
		finfo->path は通常 path\ 形式
	*/

	if ( VFF->v.SU.step < FFSTEP_ENTRY ){
		return FFStepInfo(findfile, &VFF->v.SU.step);
	}
	#ifdef UNICODE
	if ( VFF->v.SU.finfo == NULL ){
		if ( (VFF->v.SU.finfoW >= VFF->v.SU.finfomaxW) || (VFF->v.SU.finfoW->method[0] == 0) ){
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;
		}
		tmptime = VFF->v.SU.finfoW->timestamp;
		#ifdef _WIN64
		{
			findfile->nFileSizeLow = (DWORD)VFF->v.SU.finfoW->filesize;
			findfile->nFileSizeHigh = (DWORD)(VFF->v.SU.finfoW->filesize >> 32);
		}
		#else
			findfile->nFileSizeLow = VFF->v.SU.finfoW->filesize;
		#endif
		CatPath(findfile->cFileName,
				VFF->v.SU.finfoW->path, VFF->v.SU.finfoW->filename);
		VFF->v.SU.finfoW++;
	}else
	#endif
	{
		if ( (VFF->v.SU.finfo >= VFF->v.SU.finfomax) || (VFF->v.SU.finfo->method[0] == 0) ){
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;
		}
		tmptime = VFF->v.SU.finfo->timestamp;
		#ifdef _WIN64
		{
			findfile->nFileSizeLow = (DWORD)VFF->v.SU.finfo->filesize;
			findfile->nFileSizeHigh = (DWORD)(VFF->v.SU.finfo->filesize >> 32);
		}
		#else
			findfile->nFileSizeLow = VFF->v.SU.finfo->filesize;
		#endif
		#ifdef UNICODE
		{
			TCHAR Temp[VFPS];

			AnsiToUnicode(VFF->v.SU.finfo->path, findfile->cFileName, MAX_PATH);
			AnsiToUnicode(VFF->v.SU.finfo->filename, Temp, VFPS);
			CatPath(NULL, findfile->cFileName, Temp);
		}
		#else
			CatPath(findfile->cFileName, VFF->v.SU.finfo->path, VFF->v.SU.finfo->filename);
		#endif
		VFF->v.SU.finfo++;
	}
	#ifdef _WIN64
		// time_t64 から FILETIME に変換する
		tmptime = (tmptime * 10000000) + 0x19db1ded53e8000;
		findfile->ftLastWriteTime.dwLowDateTime = (DWORD)tmptime;
		findfile->ftLastWriteTime.dwHighDateTime = (DWORD)(ULONG_PTR)(tmptime >> 32);
	#else
		// time_t32 から FILETIME に変換する
		DDmul(tmptime, 10000000,
				&findfile->ftLastWriteTime.dwLowDateTime,
				&findfile->ftLastWriteTime.dwHighDateTime);
		AddDD(findfile->ftLastWriteTime.dwLowDateTime,
				findfile->ftLastWriteTime.dwHighDateTime,
				0xd53e8000, 0x19db1de);
		findfile->nFileSizeHigh = 0;
	#endif

	findfile->dwFileAttributes = 0;
	findfile->ftCreationTime.dwLowDateTime = 0;
	findfile->ftCreationTime.dwHighDateTime = 0;
	findfile->ftLastAccessTime.dwHighDateTime = 0;
	findfile->ftLastAccessTime.dwLowDateTime = 0;
	findfile->cAlternateFileName[0] = '\0';
	return TRUE;
}

#ifdef UNICODE
void SetVFFundllW(FF_UN *UN, WIN32_FIND_DATA *findfile, INDIVIDUALINFOW *arcinfo)
{
	FILETIME ftime;
	ARCSIZE64 s64;
	TCHAR *p;

	findfile->dwFileAttributes = 0;
	if ( ! ((UN->uD->UnGetWriteTimeEx != NULL) &&
			IsTrue(UN->uD->UnGetWriteTimeEx(UN->Ha, &findfile->ftLastWriteTime)) ) ){
		DosDateTimeToFileTime(arcinfo->wDate, arcinfo->wTime, &ftime);
		LocalFileTimeToFileTime(&ftime, &findfile->ftLastWriteTime);
	}
	if ( ! ((UN->uD->UnGetCreateTimeEx != NULL) &&
			IsTrue(UN->uD->UnGetCreateTimeEx(UN->Ha, &findfile->ftCreationTime)) ) ){
		findfile->ftCreationTime.dwLowDateTime = 0;
		findfile->ftCreationTime.dwHighDateTime = 0;
	}
	if ( ! ((UN->uD->UnGetAccessTimeEx != NULL) &&
			IsTrue(UN->uD->UnGetAccessTimeEx(UN->Ha, &findfile->ftLastAccessTime)) ) ){
		findfile->ftLastAccessTime.dwLowDateTime = 0;
		findfile->ftLastAccessTime.dwHighDateTime = 0;
	}
	if ( (UN->uD->UnGetOriginalSizeEx != NULL) &&
		 IsTrue(UN->uD->UnGetOriginalSizeEx(UN->Ha, &s64)) ){
		findfile->nFileSizeHigh = s64.h;
		findfile->nFileSizeLow = s64.l;
	}else{
		findfile->nFileSizeHigh = 0;
		findfile->nFileSizeLow = arcinfo->dwOriginalSize;
	}
	findfile->cAlternateFileName[0] = '\0';

	p = arcinfo->szFileName;
	p[MAX_PATH - 1] = '\0';		// 長さ調節
	while ( (p = tstrchr(p, '/')) != NULL ) *p = '\\';

	tstrcpy(findfile->cFileName, arcinfo->szFileName);
//	tstrlimcpy(findfile->cFileName, arcinfo->szFileName, MAX_PATH);
}
#endif
void SetVFFundllA(FF_UN *UN, WIN32_FIND_DATA *findfile, INDIVIDUALINFOA *arcinfo)
{
	FILETIME ftime;
	ARCSIZE64 s64;
	char *p;

	findfile->dwFileAttributes = 0;
	if ( ! ((UN->uD->UnGetWriteTimeEx != NULL) &&
			 IsTrue(UN->uD->UnGetWriteTimeEx(UN->Ha, &findfile->ftLastWriteTime)) ) ){
		DosDateTimeToFileTime(arcinfo->wDate, arcinfo->wTime, &ftime);
		LocalFileTimeToFileTime(&ftime, &findfile->ftLastWriteTime);
	}
	if ( ! ((UN->uD->UnGetCreateTimeEx != NULL) &&
			 IsTrue(UN->uD->UnGetCreateTimeEx(UN->Ha, &findfile->ftCreationTime)) ) ){
		findfile->ftCreationTime.dwLowDateTime = 0;
		findfile->ftCreationTime.dwHighDateTime = 0;
	}
	if ( ! ((UN->uD->UnGetAccessTimeEx != NULL) &&
			 IsTrue(UN->uD->UnGetAccessTimeEx(UN->Ha, &findfile->ftLastAccessTime)) ) ){
		findfile->ftLastAccessTime.dwLowDateTime = 0;
		findfile->ftLastAccessTime.dwHighDateTime = 0;
	}
	if ( (UN->uD->UnGetOriginalSizeEx != NULL) &&
		 IsTrue(UN->uD->UnGetOriginalSizeEx(UN->Ha, &s64)) ){
		findfile->nFileSizeHigh = s64.h;
		findfile->nFileSizeLow = s64.l;
	}else{
		findfile->nFileSizeHigh = 0;
		findfile->nFileSizeLow = arcinfo->dwOriginalSize;
	}
	findfile->cAlternateFileName[0] = '\0';

	p = arcinfo->szFileName;
	p[MAX_PATH - 1] = '\0';		// 長さ調節
	while ( (p = strchr(p, '/')) != NULL ) *p = '\\';

	#ifdef UNICODE
		tstrcpy(findfile->cFileName, L"???");
		MultiByteToWideCharU8(UN->uD->codepage,
				(UN->uD->codepage == CP_UTF8) ? 0 : MB_PRECOMPOSED,
				arcinfo->szFileName, -1, findfile->cFileName, MAX_PATH);
	#else
		tstrlimcpy(findfile->cFileName, arcinfo->szFileName, MAX_PATH);
	#endif
}

BOOL FindUnxNext(VFSFINDFIRST *VFF, WIN32_FIND_DATA *findfile)
{
	union {
		INDIVIDUALINFOA A;
		#ifdef UNICODE
			INDIVIDUALINFOW W;
		#endif
	} arcinfo;

	#ifdef UNICODE
	if ( VFF->v.UN.uD->codepage == CP_UNDLL ){
		switch( VFF->v.UN.step ){
			case FFSTEP_ENTRYNEXT:
				if ( VFF->v.UN.uD->UnFindNextW(VFF->v.UN.Ha, &arcinfo.W) ){
					VFF->v.UN.step = FFSTEP_NOMORE;
					break;
				}
				SetVFFundllW(&VFF->v.UN, findfile, &arcinfo.W);
				return TRUE;

			case FFSTEP_ENTRY:
				if ( VFF->v.UN.uD->UnFindFirstW(
						VFF->v.UN.Ha, VFF->v.UN.wild, &arcinfo.W)){
					VFF->v.UN.step = FFSTEP_NOMORE;
					break;
				}
				VFF->v.UN.step++;
				SetVFFundllW(&VFF->v.UN, findfile, &arcinfo.W);
				return TRUE;
		}
	}else
	#endif
	{
		switch( VFF->v.UN.step ){
			case FFSTEP_ENTRYNEXT:
				if ( VFF->v.UN.uD->UnFindNext(VFF->v.UN.Ha, &arcinfo.A) ){
					VFF->v.UN.step = FFSTEP_NOMORE;
					break;
				}
				SetVFFundllA(&VFF->v.UN, findfile, &arcinfo.A);
				return TRUE;

			case FFSTEP_ENTRY:
				if ( VFF->v.UN.uD->UnFindFirst(
						VFF->v.UN.Ha, (char *)VFF->v.UN.wild, &arcinfo.A)){
					VFF->v.UN.step = FFSTEP_NOMORE;
					break;
				}
				VFF->v.UN.step++;
				SetVFFundllA(&VFF->v.UN, findfile, &arcinfo.A);
				return TRUE;
		}
	}
	return FFStepInfo(findfile, &VFF->v.UN.step);
}
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VFSDLL BOOL PPXAPI VFSFindNext(HANDLE hFF, WIN32_FIND_DATA *findfile)
{
	VFSFINDFIRST *VFF;
	ERRORCODE errorcode;

	VFF = IsVFSFINDFIRST(hFF);
	if ( VFF == NULL ){
		errorcode = ERROR_INVALID_HANDLE;
		goto error;
	}

	switch( VFF->mode ){
		case VFSDT_PATH:				// GNC/UNC ----------------------------
#if 0
			return FindNextFile(VFF->v.FFF.hFF, findfile);
#else
		{ // 64bit では nFileSizeLow に directory file のサイズが入るようだ
			BOOL result;

			result = FindNextFile(VFF->v.FFF.hFF, findfile);
			#ifdef WINEGCC
			if ( findfile->dwFileAttributes == BADATTR ){
				findfile->dwFileAttributes = FILE_ATTRIBUTE_DEVICE;
			}
			#endif
			if ( findfile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				findfile->nFileSizeLow = 0;
			}
			return result;
		}
#endif
		case VFSDT_FTP:					// ftp  -------------------------------
			return FTPnf(&VFF->v.FTP, findfile);

		case VFSDT_LFILE:					// ListFile -----------------------
			if ( VFF->v.LFILE.type == VFSDT_LFILE_TYPE_LIST ){
				return GetListLine(&VFF->v.LFILE, findfile);
			}
			SetDummydir(findfile, T(".."));
			findfile->dwReserved0 = 0;
			findfile->dwReserved1 = 0;
			VFF->v.LFILE.type = VFSDT_LFILE_TYPE_LIST;
			return TRUE;

		case VFSDT_HTTP:					// http ---------------------------
		case VFSDT_DLIST: {					// Drive List ---------------------
			TCHAR *p;

			p = (TCHAR *)(VFF->v.MC.dirs.bottom + VFF->v.MC.d_off);
			if ( (p != NULL) && *p ){
				SetDummydir(findfile, p);
				VFF->v.MC.d_off += TSTRSIZE32(p);
				return TRUE;
			}
			p = (TCHAR *)(VFF->v.MC.files.bottom + VFF->v.MC.f_off);
			if ( (p != NULL) && *p ){
				findfile->dwFileAttributes = FILE_ATTRIBUTE_READONLY;
				SetDummyFindData(findfile);
				tstrcpy(findfile->cFileName, p);
				VFF->v.MC.f_off += TSTRSIZE32(p);
				return TRUE;
			}
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;
		}

		case VFSDT_SHN:						// SHN ---------------------------
			return VFSFN_SHN(&VFF->v.SHN, findfile);

		case VFSDT_STREAM:					// stream -------------------------
			return FindNextStream(&VFF->v.STREAM, findfile);

		case VFSDT_FATIMG:					// FAT Image File ----------------
		case VFSDT_FATDISK:					// FAT Disk ----------------
			if ( IsTrue(FindEntryFATImage(&VFF->v.FAT.fats, NULL, findfile)) ){
				return TRUE;
			}
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;

		case VFSDT_CDIMG:					// CD Image File ----------------
		case VFSDT_CDDISK:					// CD ----------------
			if ( IsTrue(FindEntryCDImage(&VFF->v.CD.cds, NULL, findfile)) ){
				return TRUE;
			}
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;

		case VFSDT_CABFOLDER:				// cab folder ----------------
		case VFSDT_LZHFOLDER:				// lzh folder ----------------
		case VFSDT_ZIPFOLDER:				// zip folder ----------------
			if ( IsTrue(ZipFolderFN(&VFF->v.ZIPO, findfile)) ){
				return TRUE;
			}
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;
#ifdef USESLASHPATH
		case VFSDT_SLASH:					// / directory ----------------
			if ( IsTrue(NextSlashDir(&VFF->v.SLASH, findfile)) ){
				return TRUE;
			}
			SetLastError(ERROR_NO_MORE_FILES);
			return FALSE;
#endif
		case VFSDT_SUSIE:					// SUSIE --------------------------
			return FindSusieNext(VFF, findfile);

		case VFSDT_UN:						// UNx --------------------------
			return FindUnxNext(VFF, findfile);

//		default: ここでは処理せず
	}
	errorcode = ERROR_INVALID_TARGET_HANDLE;
	// error: へ
										// エラー終了処理 ---------------------
error:
	SetLastError(errorcode);
	return FALSE;
}

VFSDLL BOOL PPXAPI VFSFindClose(HANDLE hFF)
{
	VFSFINDFIRST *VFF;

	VFF = IsVFSFINDFIRST(hFF);
	if ( VFF == NULL ){
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	switch (VFF->mode){
		case VFSDT_PATH:
			FindClose(VFF->v.FFF.hFF);
			break;
		case VFSDT_FTP:
			FTPclose(&VFF->v.FTP);
			break;
		case VFSDT_LFILE:
			HeapFree(DLLheap, 0, VFF->v.LFILE.mem);
			break;
		case VFSDT_HTTP:
		case VFSDT_DLIST:
			ThFree(&VFF->v.MC.dirs);
			ThFree(&VFF->v.MC.files);
			break;
		case VFSDT_SHN:
			if ( VFF->v.SHN.pSF != NULL ){
				VFF->v.SHN.pEID->lpVtbl->Release(VFF->v.SHN.pEID);
				VFF->v.SHN.pSF->lpVtbl->Release(VFF->v.SHN.pSF);
				VFF->v.SHN.pMA->lpVtbl->Release(VFF->v.SHN.pMA);
				if ( VFF->v.SHN.pSF2 != NULL ){
					VFF->v.SHN.pSF2->lpVtbl->Release(VFF->v.SHN.pSF2);
				}
			}
			ThFree(&VFF->v.SHN.dirs);
			break;
		case VFSDT_FATIMG:
		case VFSDT_FATDISK:
			CloseFATImage(&VFF->v.FAT.fats);
			break;
		case VFSDT_CDIMG:
		case VFSDT_CDDISK:
			CloseCDImage(&VFF->v.CD.cds);
			break;

		case VFSDT_STREAM:
			FindCloseStream(&VFF->v.STREAM);
			break;

		case VFSDT_SUSIE:
			LocalUnlock(VFF->v.SU.fiH);
			LocalFree(VFF->v.SU.fiH);
			break;
		case VFSDT_UN:
			#ifdef UNICODE
				VFF->v.UN.uD->SetUnicodeMode(FALSE);
			#endif
			VFF->v.UN.uD->UnCloseArchive(VFF->v.UN.Ha);
			break;
		case VFSDT_CABFOLDER:
		case VFSDT_LZHFOLDER:
		case VFSDT_ZIPFOLDER:
			ZipFolderFClose(&VFF->v.ZIPO);
			break;
#ifdef USESLASHPATH
		case VFSDT_SLASH:					// / directory ----------------
			CloseSlashDir(&VFF->v.SLASH);
			break;
#endif
	}
	HeapFree(DLLheap, 0, VFF);
	return TRUE;
}

VFSDLL BOOL PPXAPI VFSFindOptionData(HANDLE hFF, DWORD optionID, void *data)
{
	VFSFINDFIRST *VFF;

	VFF = IsVFSFINDFIRST(hFF);
	if ( VFF == NULL ){
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	switch ( VFF->mode ){
		case VFSDT_LFILE:
			switch ( optionID ){
				case FINDOPTIONDATA_LONGNAME:
					if ( VFF->v.LFILE.longname == NULL ) break;
					tstrlimcpy((TCHAR *)data, VFF->v.LFILE.longname, VFPS);
					return TRUE;

				case FINDOPTIONDATA_COMMENT:
					if ( VFF->v.LFILE.comment == NULL ) break;
					tstrlimcpy((TCHAR *)data, VFF->v.LFILE.comment, CMDLINESIZE);
					return TRUE;
			}
			break;
	}
	return FALSE;
}
