/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				アーカイバDLL関連
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONVFSDLL		// VFS.H の DLL export 指定
#define VFS_FCHK
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#pragma hdrstop

#define LOGLENGTH 0x8000 // 統合アーカイバのログサイズ

#define UNARC_USER_CANCEL 0x8020

const TCHAR UnarcTempClassName[] = T("UnarcTempClass");

const char DefaultZip[0x16] = "PK\5\6\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0";

void SetLogBadCrText(PPXAPPINFO *loginfo, const TCHAR *title, const TCHAR *text, int result);

const DWORD FDIoffset[] = { 0, 0xa2, 0x1000, 0x10000, MAX32 };

// 固有チェックを行うDLLリスト
#define VUCheckListMax 8
VUCHECKFUNC VUCheckList[VUCheckListMax + 1] = {
	(VUCHECKFUNC)VFS_check_def,
	(VUCHECKFUNC)VFS_check_ARJ,
	(VUCHECKFUNC)VFS_check_CAB,
	(VUCHECKFUNC)VFS_check_GCA,
	(VUCHECKFUNC)VFS_check_LHA,
	(VUCHECKFUNC)VFS_check_Rar,
	(VUCHECKFUNC)VFS_check_TAR,
	(VUCHECKFUNC)VFS_check_ZIP,
	(VUCHECKFUNC)VFS_check_7Zip,
};

const TCHAR SusiePath[] = T("Software\\Takechin\\Susie\\Plug-in");
// const TCHAR RegAttr[] = T("Attributes");
const TCHAR fixime[] = T("*noime%:");
const TCHAR ShareOpenError[] = MES_ESHO;

#ifndef WINEGCC
	#define Get_X_unbg GetCustDword(T("X_unbg"), 0)
#else
	#define Get_X_unbg 0 // まだ pptrayがないので無効にする。※PPC_ARCH.Cも
#endif


/*-----------------------------------------------------------------------------
	該当 API が無い場合に使用される、ダミールーチン群
-----------------------------------------------------------------------------*/
void DummyUnMessage(const TCHAR *message)
{
	THREADSTRUCT *ts;

	ts = GetCurrentThreadInfo();
	XMessage(NULL, (ts == NULL) ? NULL : ts->ThreadName,
			XM_FaERRld, T("No %s API"), message);
}
#pragma argsused
int WINAPI DummyUnarc(const HWND hwnd, LPCSTR szCmdLine, LPSTR szOutput, const DWORD wSize)
{
	UnUsedParam(hwnd);UnUsedParam(szCmdLine);UnUsedParam(szOutput);UnUsedParam(wSize);

	DummyUnMessage(T("Unarc"));
	return ERROR_NOT_SUPPORT;
}
#pragma argsused
HARC WINAPI DummOpenArchive(const HWND hwnd, LPCSTR szFileName, const DWORD dwMode)
{
	UnUsedParam(hwnd);UnUsedParam(szFileName);UnUsedParam(dwMode);

//	DummyUnMessage(T("OpenArchive"));
	return NULL;
}
#pragma argsused
int WINAPI DummyCloseArchive(HARC harc)
{
	UnUsedParam(harc);
	DummyUnMessage(T("CloseArchive"));
	return ERROR_NOT_SUPPORT;
}
#pragma argsused
int WINAPI DummyFindFirst(HARC harc, LPCSTR szWildName, INDIVIDUALINFOA *lpSubInfo)
{
	UnUsedParam(harc);UnUsedParam(szWildName);UnUsedParam(lpSubInfo);
	DummyUnMessage(T("FindFirst"));
	return ERROR_NOT_SUPPORT;
}
#pragma argsused
int WINAPI DummyFindNext(HARC harc, INDIVIDUALINFOA *lpSubInfo)
{
	UnUsedParam(harc);UnUsedParam(lpSubInfo);
	DummyUnMessage(T("FindNext"));
	return ERROR_NOT_SUPPORT;
}
#pragma argsused
BOOL WINAPI DummyCheckArchive(LPCSTR szFileName, const int iMode)
{
	UnUsedParam(szFileName);UnUsedParam(iMode);
	DummyUnMessage(T("CheckArchive"));
	return FALSE;
}

#pragma argsused
BOOL WINAPI DummySetUnicodeMode(const BOOL _bUnicode)
{
	UnUsedParam(_bUnicode);
	return FALSE;
}

//--------------- SusiePlugin の一覧を用意する。ここで LoadLibrary は行わない。
void LoadSusiePlugin(TCHAR *path, int all)
{
	SUSIE_DLL sdll;
	HANDLE hFF;			// FindFile 用ハンドル
	WIN32_FIND_DATA	ff;				// ファイル情報
	TCHAR dir[VFPS];

	ThInit(&Thsusie);
	ThInit(&Thsusie_str);
	tstrcpy(susiedir, path);
										// プラグインの読み込み位置を決定する
	#ifndef _WIN64
		CatPath(dir, path, T("*.SPI"));	// 32bit
	#else
		CatPath(dir, path, T("*.SPH"));	// 64bit
	#endif
										// 検索 -------------------------------
	hFF = FindFirstFileL(dir, &ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		do{
			SUSIE_DLLSTRINGS X_susie;

			X_susie.flags = VFSSUSIE_BMP | VFSSUSIE_ARC;
			X_susie.filemask[0] = '\0';
			if ( NO_ERROR == GetCustTable(T("P_susie"),
					ff.cFileName, &X_susie, sizeof(X_susie)) ){
										// 利用できないのが確認済みなら次へ
				if ( !(X_susie.flags | all) ) continue;
			}

			sdll.hadd = NULL;
			sdll.flags = X_susie.flags;
			sdll.DllNameOffset = Thsusie_str.top;
			ThAddString(&Thsusie_str, ff.cFileName);
			sdll.SupportExtOffset = Thsusie_str.top;
			ThAddString(&Thsusie_str, X_susie.filemask);
			ThAppend(&Thsusie, &sdll, sizeof sdll);
			susie_items++;
		}while( IsTrue(FindNextFile(hFF, &ff)) );
		FindClose(hFF);
	}
	susie_list = (SUSIE_DLL *)Thsusie.bottom;
}

/*-----------------------------------------------------------------------------
	VFS の内、プラグイン部分を有効にする
	（プラグインでないのはこれを呼び出さなくても使用できる）

・再入有り
-----------------------------------------------------------------------------*/
VFSDLL void PPXAPI VFSOn(int mode)
{
	VfsMode++;
//================================================== SUSIE プラグインの読み込み
	if ( usesusie == FALSE ){ // VFS_BMP, VFS_DIRECTORY の両方とも
		TCHAR dir[MAX_PATH];
		BOOL all;

		InitializeCriticalSection(&ArchiveSection[VFSAS_SUSIE]);
													// 1)設定 dir
		dir[0] = '\0';
		all = mode & VFS_ALL;
		GetCustData(T("P_susieP"), dir, sizeof(dir));
		if ( dir[0] != '\0' ){
			VFSFixPath(NULL, dir, DLLpath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
			LoadSusiePlugin(dir, all);
		}else if ( GetRegString(HKEY_CURRENT_USER,	// 2)susie が認識する dir
				SusiePath, StrPath, dir, TSIZEOF(dir)) ){
			LoadSusiePlugin(dir, all);
		}else{										// 3)ppx dir
			LoadSusiePlugin(DLLpath, all);
		}
		usesusie = TRUE;
	}
//================================================== Unxxx 系の読み込み
	if ( (useundll == FALSE) && (mode & VFS_DIRECTORY) ){
		UN_DLL *uD;
		int size;

		InitializeCriticalSection(&ArchiveSection[VFSAS_UNDLL]);
		size = GetCustDataSize(T("P_arc"));
		if ( size > 0 ){
			TCHAR *tbl, *p;
			DWORD w;
			int i;

			i = CountCustTable(T("P_arc"));
			undll_list = uD = HeapAlloc( DLLheap, 0, size + sizeof(UN_DLL) * i );
			if ( undll_list == NULL ) return;
			tbl = (TCHAR *)((char *)undll_list + sizeof(UN_DLL) * i);
			GetCustData(T("P_arc"), tbl, size);
			for ( ; ; ){
				w = *(WORD *)tbl;
				if ( !w ) break;
										// カスタマイズデータを取得
				p = (TCHAR *)((char *)tbl + sizeof(WORD));
				uD->DllName = p;
				p += tstrlen(p) + 1;

				uD->flags = *(DWORD *)p;
				#ifdef _WIN64 // 32/64bit版はそれぞれ用のを読み込む
				if ( uD->flags & UNDLLFLAG_64bit )
				#else
				if ( uD->flags & UNDLLFLAG_32bit )
				#endif
				{
					p += sizeof(DWORD) / sizeof(TCHAR);
					uD->knowntype = *(int *)p;
					p += sizeof(int) / sizeof(TCHAR);
					uD->ApiHeadName = p;

					uD->AllExtCMD = NilStr;
					uD->PartExtCMD = NilStr;
					uD->SingleExtCMD = NilStr;
					uD->CheckWildcard = NULL;
					uD->SupportWildcard = NULL;
					uD->DllYetAnotherName = NULL;

					p += tstrlen(p) + 1;
					uD->params = p;

					for (;;){
						if ( *p == '\0' ) break;
						if ( *(p + 1) == '=' ){
							switch ( *p ){
								case 'a': // 一括展開
									uD->AllExtCMD = p + 2;
									break;

								case 'e': // 部分展開
									uD->PartExtCMD = p + 2;
									break;

								case 'i': // 対応拡張子ワイルドカード
									uD->SupportWildcard = p + 2;
									break;

								case 's': // １ファイル展開
									uD->SingleExtCMD = p + 2;
									break;

								case 'w': // ワイルドカードマスク
									if ( *(p + 2) != '\0' ){
										uD->CheckWildcard = p + 2;
									}
									break;

								case 'y': // 予備DLL名
									uD->DllYetAnotherName = p + 2;
									break;

								// default: po 等は無視する
							}
						}
						p += tstrlen(p) + 1;
					}
											// DLL 初期設定
					uD->hadd			= NULL;
					uD->Unarc			= DummyUnarc;
					uD->UnOpenArchive	= DummOpenArchive;
					uD->UnCloseArchive	= DummyCloseArchive;
					uD->UnFindFirst		= DummyFindFirst;
					uD->UnFindNext		= DummyFindNext;
					uD->UnCheckArchive	= DummyCheckArchive;
					#ifdef UNICODE
						uD->UnarcW			= NULL;
						uD->UnOpenArchiveW	= NULL;
						uD->UnFindFirstW	= NULL;
						uD->UnFindNextW		= NULL;
						uD->UnCheckArchiveW	= NULL;
						uD->SetUnicodeMode	= DummySetUnicodeMode;
						uD->codepage = CP_ACP;
					#endif
					uD->VUCheck = VUCheckList[(uD->knowntype <= VUCheckListMax)
								? uD->knowntype : 0];
					undll_items++;
					uD++;
				}
				tbl = (TCHAR *)((char *)tbl + w);
			}
		}
		useundll = TRUE;
	}
}
/*-----------------------------------------------------------------------------
	プラグイン部分の使用を止める
-----------------------------------------------------------------------------*/
VFSDLL void PPXAPI VFSOff(void)
{
	int i;

	if ( VfsMode != 0 ){
		VfsMode--;
		if ( VfsMode != 0 ) return;
	}
	if ( ArchiverUse > 0 ) return;
	if ( useundll ){
		useundll = FALSE;
		DeleteCriticalSection(&ArchiveSection[VFSAS_UNDLL]);

		if ( undll_list != NULL ){
			for ( i = 0 ; i < undll_items ; i++ ){
				if ( undll_list[i].hadd != NULL ){
					FreeLibrary(undll_list[i].hadd);
				}
			}
			HeapFree(DLLheap, 0, undll_list);
			undll_list = NULL;
		}
		undll_items = 0;
	}
	if ( usesusie ){
		usesusie = FALSE;
		DeleteCriticalSection(&ArchiveSection[VFSAS_SUSIE]);

		if ( susie_list != NULL ){
			for ( i = 0 ; i < susie_items ; i++ ){
				if ( susie_list[i].hadd != NULL ){
					FreeLibrary(susie_list[i].hadd);
				}
			}
			ThFree(&Thsusie);
		}
		susie_items = 0;
	}
	FreePPxModule();
}

// Susie Plug-in が使えるか調べる。必要に応じて SPI を読む。-------------------
BOOL CheckAndLoadSusiePlugin(SUSIE_DLL *sudll, const TCHAR *filename, THREADSTRUCT *ts, DWORD mode)
{
	TCHAR dir[MAX_PATH];
	TCHAR *p;
	HMODULE hI;
	char inf[MAX_PATH];
	SUSIE_DLLSTRINGS X_susie;
	#ifdef UNICODE
		char LIBDIRA[MAX_PATH];
	#else
		#define LIBDIRA dir
	#endif

// ワイルドカードによるファイル名チェック
	p = (TCHAR *)(Thsusie_str.bottom + sudll->SupportExtOffset);
	if ( *p && !(mode & VFS_FORCELOAD_PLUGIN) ){
		FN_REGEXP fn;

		MakeFN_REGEXP(&fn, p);
		if( !FilenameRegularExpression(filename, &fn) ){
			FreeFN_REGEXP(&fn);
			return FALSE;
		}
		FreeFN_REGEXP(&fn);
	}

// エラー時のモジュール表示を変更
	if ( ts != NULL ){
		ts->ThreadName = (TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset);
	}

// DLL 準備済み?
	if ( sudll->hadd != NULL ) return TRUE;

	CatPath(dir, susiedir, (TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset));

// DLL 読み込み
	#ifdef UNICODE
	UnicodeToAnsi(dir, LIBDIRA, MAX_PATH);
	#endif
	hI = LoadLibraryA(LIBDIRA);

	if ( hI == NULL ){				// 読み込みできない
		ERRORCODE errcode = GetLastError();
		if ( errcode == ERROR_MOD_NOT_FOUND ){
			if ( DSetDllDirectory != NULL ){
				DSetDllDirectory(susiedir);
				hI = LoadLibraryA(LIBDIRA);
				DSetDllDirectory(NilStr); // DLLの検索パスからカレントを除去する
				errcode = GetLastError();
			}
		}
		if ( hI == NULL ){
			PPErrorBox(NULL, (TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset), errcode);
			sudll->flags = 0;
			return FALSE;
		}
	}
	sudll->GetPluginInfo = (GETPLUGININFO)GetProcAddress(hI, "GetPluginInfo");
	if ( sudll->GetPluginInfo == NULL ){
		sudll->flags = 0;
		return FALSE;
	}
	sudll->GetPluginInfo(0, inf, sizeof inf);
	X_susie.flags = sudll->flags & ~(VFSSUSIE_BMP | VFSSUSIE_ARC);
	if ( (inf[2] == 'I') && (inf[3] == 'N' )) X_susie.flags |= VFSSUSIE_BMP;
	if ( (inf[2] == 'A') && (inf[3] == 'M' )) X_susie.flags |= VFSSUSIE_ARC;
	if ( X_susie.flags == 0 ){
		XMessage(NULL, NULL, XM_NiERRld, T("SPI error:%s(%s)"), dir, inf);
	}
	if ( X_susie.flags != sudll->flags ){
		sudll->flags = X_susie.flags;
		if ( !(mode & VFS_FORCELOAD_PLUGIN) ){ // 強制読み込みでなければ保存する
			TCHAR *seo;

			seo = (TCHAR *)(Thsusie_str.bottom + sudll->SupportExtOffset);
			tstrcpy(X_susie.filemask, seo);
			SetCustTable(T("P_susie"),
				(TCHAR *)(Thsusie_str.bottom + sudll->DllNameOffset),
				&X_susie, sizeof(DWORD) + TSTRSIZE(seo));
		}
	}
	sudll->hadd = hI;
	sudll->IsSupported =(ISSUPPORTED)GetProcAddress(hI, "IsSupported");
#ifdef UNICODE
	sudll->IsSupportedW =(ISSUPPORTEDW)GetProcAddress(hI, "IsSupportedW");
#endif
															// DIB 変換
	if ( sudll->flags & VFSSUSIE_BMP ){
		sudll->GetPicture = (GETPICTURE)GetProcAddress(hI, "GetPicture");
		sudll->GetPreview = (GETPREVIEW)GetProcAddress(hI, "GetPreview");
#ifdef UNICODE
		sudll->GetPictureW = (GETPICTUREW)GetProcAddress(hI, "GetPictureW");
#endif
		return (mode & VFS_BMP);
	}
															// ARC
	if ( sudll->flags & VFSSUSIE_ARC ){
		sudll->GetArchiveInfo =
				(GETARCHIVEINFO)GetProcAddress(hI, "GetArchiveInfo");
		sudll->GetFile = (GETFILE)GetProcAddress(hI, "GetFile");
		sudll->GetFileInfo = (GETFILEINFO)GetProcAddress(hI, "GetFileInfo");
#ifdef UNICODE
		sudll->GetArchiveInfoW =
				(GETARCHIVEINFOW)GetProcAddress(hI, "GetArchiveInfoW");
		sudll->GetFileW = (GETFILEW)GetProcAddress(hI, "GetFileW");
		sudll->GetFileInfoW = (GETFILEINFOW)GetProcAddress(hI, "GetFileInfoW");
#endif
		return (mode & VFS_DIRECTORY);
	}
	sudll->hadd = NULL;
	FreeLibrary(hI);
	return FALSE;
}

//=============================================================================
// File Check
//=============================================================================
VFSDLL int PPXAPI VFSCheckDir(const TCHAR *filename, BYTE *header, DWORD hsize, void **dt_opt)
{
	int i;
	THREADSTRUCT *ts;
	const TCHAR **ThreadName, *DummyThreadName;
	const TCHAR *OldTname;
	TCHAR *DllName = NULL;
	HMENU hPopupMenu = NULL;
	TCHAR extbuf[VFPS + 2];
	int menuid = VFSCHK_MENUID;
	int MenuMode = 0;
	#define MenuMode_Dir 1
	#define MenuMode_Extract 2
	union {
		TCHAR str[CMDLINESIZE];
		BYTE bin[SUSIE_CHECK_SIZE];
		FN_REGEXP fn;
	} buf;

#ifdef UNICODE
	char filenameA[VFPS];
	#define TFILENAME filenameA
#else
	#define TFILENAME filename
#endif
	{
		TCHAR *sep;

		sep = tstrrchr(filename, ':');
		if ( (sep != NULL) && (sep > (filename + 1)) && (*(sep - 1) == ':') ){
			*(sep - 1) = '\0';
			DllName = sep + 1;
		}
	}

	if ( hsize > VFSCHKDIR_GETEXTRACTMENU ){
		hPopupMenu = (HMENU)dt_opt;
		dt_opt = NULL;
		if ( hPopupMenu != NULL ){
			MenuMode = (hsize >= VFSCHKDIR_GETDIRMENU) ? MenuMode_Dir : MenuMode_Extract;
		}
		resetflag(hsize, VFSCHKDIR_GETDIRMENU | VFSCHKDIR_GETEXTRACTMENU);
	}

	// -------------------------------- ListFile
	if ( !memcmp(header, LHEADER, sizeof(LHEADER) - 1) ||
		 !memcmp(header + 2, L";ListFile\r\n", sizeof(L";ListFile\r\n") - sizeof(WCHAR)) ){
		if ( MenuMode == 0 ) return VFSDT_LFILE;
		if ( MenuMode == MenuMode_Dir ){
			AppendMenuString(hPopupMenu, menuid++, T("listfile"));
		}
	}
	{ // ------------------------------ ディスクイメージ
		const DWORD *o;

		for ( o = FDIoffset ; *o != MAX32 ; o++ ){
			if ( CheckFATImage(header + *o, header + hsize) != CHECKFAT_NONE ){
				if ( hPopupMenu == NULL ) return VFSDT_FATIMG;
				if ( MenuMode == MenuMode_Dir ){
					AppendMenuString(hPopupMenu, menuid++, T("disk"));
				}
			}
		}
		if ( (hsize >= 0x2000) && !memcmp(header + 0x1002, "\x90\x90IPL1", 6) ){
			if ( MenuMode == 0 ) return VFSDT_FATIMG;
			if ( MenuMode == MenuMode_Dir ){
				AppendMenuString(hPopupMenu, menuid++, T("disk"));
			}
		}
	}
	if ( hsize >= 0x8100 ){
		if ( !memcmp(header + CDHEADEROFFSET1, StrISO9660, CDHEADERSIZE - 1) ||
			 !memcmp(header + CDHEADEROFFSET1, StrUDF, CDHEADERSIZE) ){
			if ( MenuMode == 0 ) return VFSDT_CDIMG;
			if ( MenuMode == MenuMode_Dir ){
				AppendMenuString(hPopupMenu, menuid++, T("CD"));
			}
		}
		if ( (hsize >= 0x9400) && (MenuMode == 0) ){
			if ( !memcmp(header + CDHEADEROFFSET2, StrISO9660, CDHEADERSIZE) ||
				 !memcmp(header + CDHEADEROFFSET2, StrUDF, CDHEADERSIZE) ){
				return VFSDT_CDIMG;
			}
			if ( !memcmp(header + CDHEADEROFFSET3, StrISO9660, CDHEADERSIZE) ||
				 !memcmp(header + CDHEADEROFFSET3, StrUDF, CDHEADERSIZE) ){
				return VFSDT_CDIMG;
			}
			if ( *(DWORD *)header == 0 ){
				HANDLE hFile;

				hFile = CreateFileL(filename, GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
						OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if ( hFile != INVALID_HANDLE_VALUE ){
					DWORD fsize;

					if ( MAX32 != SetFilePointer(hFile, CDHEADEROFFSET4, NULL, FILE_BEGIN)){
						ReadFile(hFile, buf.bin, CDHEADERSIZE, &fsize, NULL);
						if ( !memcmp(buf.bin, StrISO9660, CDHEADERSIZE) ){
							CloseHandle(hFile);
							return VFSDT_CDIMG;
						}
					}

					if ( MAX32 != SetFilePointer(hFile, DVDHEADEROFFSET, NULL, FILE_BEGIN)){
						ReadFile(hFile, buf.bin, 2, &fsize, NULL);
						if ( !memcmp(buf.bin, "\2\0", 2) ){
							CloseHandle(hFile);
							return VFSDT_CDIMG;
						}
					}
					CloseHandle(hFile);
				}
			}
		}
	}
	// -------------------------------- Exe arc

	ts = GetCurrentThreadInfo();
	ThreadName = ( ts != NULL ) ? &ts->ThreadName : &DummyThreadName;
	OldTname = *ThreadName;
/*
	separator = tstrrchr(filename, ':'); // "::" を検索
	if ( (separator != NULL) && (separator >= (filename + 2)) &&
		 (*(separator - 1) == ':') ){
		DllName = separator + 1;
	}
*/
#ifdef UNICODE
	UnicodeToAnsi(filename, filenameA, VFPS);
/*
	{
		char *separatorA;

		separatorA = strrchr(filenameA, ':'); // "::" を検索
		if ( (separatorA != NULL) && (separatorA >= (filenameA + 2)) &&
			 (*(separatorA - 1) == ':') ){
			*(separatorA - 1) = '\0';
		}
	}
*/
#endif
//---------------------------------------------------------- アーカイバ系の判別
	{
		UN_DLL *uD;
		VUCHECKSTRUCT vcs;

		vcs.filename = TFILENAME;
		vcs.header = header;
		vcs.fsize = hsize;
		vcs.floatheader = -1;
		#ifdef UNICODE
			vcs.filenameW = filename;
			WideCharToMultiByteU8(CP_UTF8, 0, filename, -1, vcs.filename8, VFPS, NULL, NULL);
		#endif
		uD = undll_list;
		for ( i = 0 ; i < undll_items ; i++, uD++ ){
			if ( uD->flags & UNDLLFLAG_DISABLE_DIR ) continue;
			if ( (DllName != NULL) && tstricmp(uD->DllName, DllName) ) continue;
			*ThreadName = uD->DllName;
			if ( uD->VUCheck(uD, &vcs) ){
				if ( dt_opt != NULL ){
					*dt_opt = (void *)(size_t)(uD - undll_list + 1);
				}
				*ThreadName = OldTname;

				if ( MenuMode == 0 ) return VFSDT_UN;
				AppendMenuString(hPopupMenu, menuid++, uD->DllName);
				continue;
			}
			if ( (MenuMode == MenuMode_Dir) && (uD->SupportWildcard != NULL) ){
				int result;

				MakeFN_REGEXP(&buf.fn, uD->SupportWildcard);
				#ifdef UNICODE
					result = FilenameRegularExpression(vcs.filenameW, &buf.fn);
				#else
					result = FilenameRegularExpression(vcs.filename, &buf.fn);
				#endif
				if ( result && IsTrue(LoadUnDLL(uD)) ){
					AppendMenuString(hPopupMenu, menuid++, uD->DllName);
				}
				FreeFN_REGEXP(&buf.fn);
			}
		}
	}
//---------------------------------------------------------------- susie の判別
	{
		SUSIE_DLL *su;
		TCHAR *fp;
		BYTE *hptr;

		fp = FindLastEntryPoint(filename);
		su = susie_list;
		if ( hsize >= SUSIE_CHECK_SIZE ){
			hptr = header;
		}else{
			memcpy(buf.bin, header, hsize);
			memset(buf.bin + hsize, 0, SUSIE_CHECK_SIZE - hsize);
			hptr = buf.bin;
		}
		for ( i = 0 ; i < susie_items ; i++, su++ ){
			if ( !(su->flags & VFSSUSIE_ARC) ) continue;
			if ( DllName == NULL ){
				if ( (su->flags & VFSSUSIE_NOAUTODETECT) &&
					 (hPopupMenu == NULL) ){
					continue;
				}
			}else if ( tstricmp((TCHAR *)(Thsusie_str.bottom + su->DllNameOffset), DllName) ){
				continue;
			}

			if ( CheckAndLoadSusiePlugin(su, fp, ts, VFS_DIRECTORY) == FALSE ){
				continue;
			}
			*ThreadName = (TCHAR *)(Thsusie_str.bottom + su->DllNameOffset);

			#ifdef UNICODE
				if ( su->IsSupportedW != NULL ){
					if ( !su->IsSupportedW(filename, hptr) ) continue;
				}else{
					if ( !su->IsSupported(TFILENAME, hptr) ) continue;
				}
			#else
				if ( !su->IsSupported(filename, hptr) ) continue;
			#endif

			if ( dt_opt != NULL ){
				*dt_opt = (void *)(size_t)(su - susie_list + 1);
			}
			*ThreadName = OldTname;

			if ( MenuMode == 0 ) return VFSDT_SUSIE;
			extbuf[0] = ':';
			extbuf[1] = ':';
			tstrcpy(extbuf + 2, (TCHAR *)(Thsusie_str.bottom + su->DllNameOffset));
			AppendMenuString(hPopupMenu, menuid++, extbuf);
		}
	}
	*ThreadName = OldTname;
//-------------- ZIP folder
	if ( (header[0] == 'P') && (header[1] == 'K') && (header[2] < 6) &&
		 (header[2] == (header[3] - 1)) ){	// PKzip ヘッダ
		const TCHAR *p = zipfldrName;

		if ( GetExecType(&p, NULL, NULL) == GTYPE_DATA ){
			if ( MenuMode == 0 )  return VFSDT_ZIPFOLDER;
			AppendMenuString(hPopupMenu, menuid++, zipfldrName);
		}
	}
	if ( (header[2] == '-') && (header[3] == 'l') && (header[4] == 'h') &&
		 (IsdigitA(header[5]) || (header[5] == 'd')) && (header[6] == '-') ){	// LHA ヘッダ
		const TCHAR *p = lzhfldrName;

		if ( GetExecType(&p, NULL, NULL) == GTYPE_DATA ){
			if ( MenuMode == 0 ) return VFSDT_LZHFOLDER;
			AppendMenuString(hPopupMenu, menuid++, lzhfldrName);
		}
	}
	if ( (header[0] == 'M') && (header[1] == 'S') && (header[2] == 'C') &&
		 (header[3] == 'F') && (*(WORD *)(header + 18) < 0x10)){ // CAB ヘッダ
		const TCHAR *p = cabfldrName;

		if ( GetExecType(&p, NULL, NULL) == GTYPE_DATA ){
			if ( MenuMode == 0 ) return VFSDT_CABFOLDER;
			AppendMenuString(hPopupMenu, menuid++, cabfldrName);
		}
	}
	if ( MenuMode == 0 ) return VFSDT_UNKNOWN;
//-------------- ShellFolder Extension 判別(まだ FF が機能しないので無効中)
/*
	{
		DWORD attr;
		TCHAR rpath[MAX_PATH], *ext;	// アプリケーションのキー

		ext = VFSFindLastEntry(filename);
		ext += FindExtSeparator(ext);
		if ( *ext ){
			if ( GetRegString(HKEY_CLASSES_ROOT,
						ext, NilStr, rpath, TSIZEOF(rpath)) ){
				tstrcat(rpath, T("\\ShellFolder"));
				// XP。Win7はStorageHandler
				if ( GetRegString(HKEY_CLASSES_ROOT,
							rpath, RegAttr, (TCHAR *)&attr, sizeof(attr)) ){
					if ( attr & SFGAO_FOLDER ){
						wsprintf(extbuf, T("+%s"), ext);
						AppendMenuString(hPopupMenu, menuid++, extbuf);
					} // return VFSDT_SHN;
				}
			}
		}
	}
*/
///-------------- E_unpack2 判別
	// ※ PP_GetExtCommand 内でも VFSCheckDir が使われている。
	//    但しここは通らない(menu追加でないため)
	if ( (MenuMode == MenuMode_Extract) &&
		 (0 <= PP_GetExtCommand(filename, T("E_unpack2"), buf.str, extbuf + 1)) ){
		extbuf[0] = '*';
		AppendMenuString(hPopupMenu, menuid++, extbuf);
	}
	return menuid - VFSCHK_MENUID;
}

VFSDLL int PPXAPI VFSCheckImage(const TCHAR *filename, BYTE *header, DWORD hsize, HMENU hPopupMenu)
{
	int i;
	THREADSTRUCT *ts;
	const TCHAR **ThreadName, *DummyThreadName;
	const TCHAR *OldTname;
//	TCHAR *DllName = NULL; // 現在、DLLName 限定を行わない
	int menuid = VFSCHK_MENUID;
#ifdef UNICODE
	char filenameA[VFPS];
	#define TFILENAME filenameA
#else
	#define TFILENAME filename
#endif
	{
		TCHAR *sep;

		sep = tstrrchr(filename, ':');
		if ( (sep != NULL) && (sep > (filename + 1)) && (*(sep - 1) == ':') ){
			*(sep - 1) = '\0';
		//	DllName = sep + 1;
		}
	}
	ts = GetCurrentThreadInfo();
	ThreadName = ( ts != NULL ) ? &ts->ThreadName : &DummyThreadName;
	OldTname = *ThreadName;

#ifdef UNICODE
	UnicodeToAnsi(filename, filenameA, VFPS);
#endif
//---------------------------------------------------------------- susie の判別
	{
		SUSIE_DLL *su;
		TCHAR *fp;
		BYTE headerbuf[SUSIE_CHECK_SIZE], *hptr;

		fp = FindLastEntryPoint(filename);
		su = susie_list;
		if ( hsize >= SUSIE_CHECK_SIZE ){
			hptr = header;
		}else{
			memcpy(headerbuf, header, hsize);
			memset(headerbuf + hsize, 0, SUSIE_CHECK_SIZE - hsize);
			hptr = headerbuf;
		}
		for ( i = 0 ; i < susie_items ; i++, su++ ){
			if ( !(su->flags & VFSSUSIE_BMP) ) continue;
			if ( CheckAndLoadSusiePlugin(su, fp, ts, VFS_BMP | VFS_FORCELOAD_PLUGIN) == FALSE ){
				continue;
			}
			*ThreadName = (TCHAR *)(Thsusie_str.bottom + su->DllNameOffset);

			#ifdef UNICODE
				if ( su->IsSupportedW != NULL ){
					if ( !su->IsSupportedW(filename, hptr) ) continue;
				}else{
					if ( !su->IsSupported(TFILENAME, hptr) ) continue;
				}
			#else
				if ( !su->IsSupported(filename, hptr) ) continue;
			#endif

			*ThreadName = OldTname;

			AppendMenuString(hPopupMenu, menuid++, (TCHAR *)(Thsusie_str.bottom + su->DllNameOffset));
		}
	}
	*ThreadName = OldTname;
	return 0;
}

//=============================================================================
//--------------------------------------------------------------- UnDLL Loaderr
#pragma argsused
BOOL VFSVUCheckError(void *uD, VUCHECKSTRUCT *vcs)
{
	UnUsedParam(uD);UnUsedParam(vcs);
	return FALSE;
}

// UnDLL Procloader -----------------------------------------------------------
typedef struct {
	HMODULE hModule;
	char *ProcName;
	char *ProcHead;
} SETUNADDERESSWSTRUCT;

ARCPROC SetUnAdderess(SETUNADDERESSWSTRUCT *sas, const char *name, ARCPROC *Proc)
{
	ARCPROC adr;

	strcpy(sas->ProcHead, name);
	*Proc = adr = (ARCPROC)GetProcAddress(sas->hModule, sas->ProcName);
	return adr;
}

#pragma argsused
BOOL WINAPI NoCheckArchive(LPCSTR szFileName, const int iMode)
{
	UnUsedParam(szFileName);UnUsedParam(iMode);
	return FALSE;
}

// UnDLL loader ---------------------------------------------------------------
BOOL LoadUnDLL(UN_DLL *uD)
{
	SETUNADDERESSWSTRUCT sas;
#ifdef UNICODE
	char DllNameA[MAX_PATH];
#endif
	char ProcName[0x60];

	if ( uD->hadd != NULL ) return TRUE;		// 既に初期化済み
	if ( uD->VUCheck == VFSVUCheckError ) return FALSE; // 使用不可
#ifdef UNICODE
	UnicodeToAnsi(uD->DllName, DllNameA, sizeof(DllNameA));
	#ifdef _WIN64
		if ( uD->flags & UNDLLFLAG_32bit ){
			if ( strstr(DllNameA, "32") ){
				strcpy(DllNameA, "UNBYPASS.DLL");
			}
		}
	#endif
	#define TDLLNAME DllNameA
#else
	#define TDLLNAME uD->DllName
#endif
	sas.hModule = LoadLibraryA(TDLLNAME);	// DLL を読み込む -----------------
	if ( sas.hModule == NULL ){				// 失敗→使えないように処置
		uD->VUCheck = VFSVUCheckError;
		return FALSE;
	}

	strcpyToA(ProcName, uD->ApiHeadName, sizeof(ProcName));
	sas.ProcName = ProcName;
	sas.ProcHead = ProcName + strlen(ProcName);
									// 各変数のポインタをセット
	uD->hadd = sas.hModule;
	SetUnAdderess(&sas, "", &uD->Unarc);
	SetUnAdderess(&sas, "OpenArchive",	(ARCPROC *)&uD->UnOpenArchive);
	SetUnAdderess(&sas, "CloseArchive",	&uD->UnCloseArchive);
	SetUnAdderess(&sas, "FindFirst",	&uD->UnFindFirst);
	SetUnAdderess(&sas, "FindNext",		&uD->UnFindNext);
	SetUnAdderess(&sas, "GetWriteTimeEx", &uD->UnGetWriteTimeEx);
	SetUnAdderess(&sas, "GetCreateTimeEx", &uD->UnGetCreateTimeEx);
	SetUnAdderess(&sas, "GetAccessTimeEx", &uD->UnGetAccessTimeEx);
	SetUnAdderess(&sas, "GetOriginalSizeEx", &uD->UnGetOriginalSizeEx);

	if ( (uD->Unarc == NULL) && (uD->UnOpenArchive == NULL) ){ // 使用できるAPIがない
		uD->VUCheck = VFSVUCheckError;
		return FALSE;
	}
	if ( uD->flags & UNDLLFLAG_DISABLE_DIR ){
		uD->UnCheckArchive = NoCheckArchive;
	}else{
		if ( SetUnAdderess(&sas, "CheckArchive", &uD->UnCheckArchive) == NULL ){
			uD->UnCheckArchive = NoCheckArchive;
			setflag(uD->flags, UNDLLFLAG_DISABLE_DIR);
		}
	}
	#ifdef UNICODE
	if ( *uD->ApiHeadName ){
		if ( ! (uD->flags & UNDLLFLAG_DISABLE_WIDEFUNCTION) ){
			if ( SetUnAdderess(&sas, "W", &uD->UnarcW) != NULL ){
				SetUnAdderess(&sas, "OpenArchiveW", (ARCPROC *)&uD->UnOpenArchiveW);
				SetUnAdderess(&sas, "FindFirstW", &uD->UnFindFirstW);
				SetUnAdderess(&sas, "FindNextW", &uD->UnFindNextW);
				if ( !(uD->flags & UNDLLFLAG_DISABLE_DIR) ){
					SetUnAdderess(&sas, "CheckArchiveW", &uD->UnCheckArchiveW);
				}
			}
		}

		if ( ! (uD->flags & UNDLLFLAG_DISABLE_UNICODEMODE) ){
			if ( SetUnAdderess(&sas, "SetUnicodeMode", &uD->SetUnicodeMode) == NULL ){
				uD->SetUnicodeMode = DummySetUnicodeMode;
			}
			if ( uD->VUCheck == (VUCHECKFUNC)VFS_check_7Zip ){
				DefineWinAPI(WORD, SevenZipGetVersion, (void));

				GETDLLPROC(sas.hModule, SevenZipGetVersion);
				if ((DSevenZipGetVersion == NULL) || (DSevenZipGetVersion()<920) ){
					// 9.20 以前は UNICODE対応が不完全
					uD->SetUnicodeMode = DummySetUnicodeMode;
				}
			}
		}
	}
	#endif

	return TRUE;
}
// %u を実行 ---------------------------------------------------------------
int UnArc_ExecExtra(UN_DLL *uD, HWND hWnd, const TCHAR *Cmd, PPXAPPINFO *loginfo)
{
	const TCHAR *p;
	char ProcName[CMDLINESIZE];
	int result;
	impUnarc Unarc, tmpUnarc;
	#ifdef UNICODE
		impUnarcW UnarcW;
	#endif
	p = tstrchr(Cmd, ']');
	strcpyToA(ProcName, Cmd + 1, CMDLINESIZE);
	ProcName[(p - Cmd) - 1] = '\0';

	Unarc = uD->Unarc;
	#ifdef UNICODE
	UnarcW = uD->UnarcW;
	#endif
	tmpUnarc = (impUnarc)GetProcAddress(uD->hadd, ProcName);
	if ( tmpUnarc == NULL ) return 1;
	uD->Unarc = tmpUnarc;
	#ifdef UNICODE
		ProcName[(p - Cmd) - 1] = 'W';
		ProcName[(p - Cmd)] = '\0';
		uD->UnarcW = (impUnarcW)GetProcAddress(uD->hadd, ProcName);
	#endif
	result = UnArc_ExecMain(uD, hWnd, p + 1, loginfo);
	uD->Unarc = Unarc;
	#ifdef UNICODE
	uD->UnarcW = UnarcW;
	#endif
	return result;
}

int DoUnarcMain2(UN_DLL *uD, HWND hWnd, const TCHAR *Cmd, PPXAPPINFO *loginfo)
{
	if ( (uD->hadd == NULL) && (LoadUnDLL(uD) == FALSE) ) return 1;

	#ifdef UNICODE
		uD->codepage = CP_ACP;
		if ( Cmd[0] == '!' ){
			if ( Cmd[1] == '2' ){
				uD->codepage = CP_PPX_UCF2;
				Cmd += 2;
			}else if ( Cmd[1] == '8' ){
				uD->codepage = CP_UTF8;
				Cmd += 2;
			}
		}
	#endif
	// API 指定有り
	if ( (*Cmd == '[') && tstrchr(Cmd, ']') ){
		return UnArc_ExecExtra(uD, hWnd, Cmd, loginfo);
	}
	return UnArc_ExecMain(uD, hWnd, Cmd, loginfo);
}

int DoUnarcMain(const TCHAR *DllName, HWND hWnd, const TCHAR *Cmd, PPXAPPINFO *loginfo)
{
	int i;
	UN_DLL *uD;
										// UnDll ------------------------------
	uD = undll_list;
	for ( i = 0 ; i < undll_items ; i++, uD++ ){
		if ( tstricmp(uD->DllName, DllName) == 0 ){
			return DoUnarcMain2(uD, hWnd, Cmd, loginfo);
		}
	}
	uD = undll_list;
	for ( i = 0 ; i < undll_items ; i++, uD++ ){
		if ( (uD->DllYetAnotherName != NULL) &&
			 (tstricmp(uD->DllYetAnotherName, DllName) == 0) ){
			return DoUnarcMain2(uD, hWnd, Cmd, loginfo);
		}
	}
	XMessage(hWnd, NULL, XM_GrERRld, MES_ENUD, DllName);
	return 1;
}

// %uzipfldr.dll, A "書庫フルパス" [*file のオプション]
int DoUnarcZipfolder(PPXAPPINFO *ppxa, HWND hWnd, const TCHAR *param)
{
	TCHAR dest[VFPS], cmd[CMDLINESIZE], *p;
	HANDLE hFile;
	DWORD size;

	if ( *param != 'A' ){
		XMessage(hWnd, NULL, XM_GrERRld, T("zipfldr command error"));
		return 1;
	}
	param++;
	GetCommandParameter(&param, dest, TSIZEOF(dest));
	p = tstrrchr(dest, '.');
	if ( p == NULL ) p = dest + tstrlen(dest);
	if ( tstricmp(p, T(".zip")) != 0 ) tstrcat(p, T(".zip"));

	hFile = CreateFileL(dest, GENERIC_WRITE, 0, NULL,
			CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		WriteFile(hFile, DefaultZip, sizeof(DefaultZip), &size, NULL);
		CloseHandle(hFile);
	}
	wsprintf(cmd, T("*file !copy,,\"%s\"%s"), dest, param);

	PP_ExtractMacro(hWnd, ppxa, NULL, cmd, NULL, 0);
	return 0;
}

int DoUnarc(PPXAPPINFO *ppxa, const TCHAR *DllName, HWND hWnd, const TCHAR *param)
{
	int result;
	HANDLE hMutex = NULL;
	HWND hBaseWnd;
	TCHAR cmd[CMDLINESIZE];
	const TCHAR *ndptr;
	DWORD X_unbg;
	int UnAllMode; // , Dlltype = 0;

	hBaseWnd = hWnd;

	ndptr = DllName;
	if ( *ndptr == '/' ) ndptr++;
	if ( tstrcmp(ndptr, T("zipfldr.dll")) == 0 ){
		return DoUnarcZipfolder(ppxa, hWnd, param);
	}
/*
	if ( tstrcmp(ndptr, T("zipfldr.dll")) == 0 ) Dlltype = 1;
	if ( tstrcmp(ndptr, T("lzhfldr2.dll")) == 0 ) Dlltype = 2;
	if ( tstrcmp(ndptr, T("cabview.dll")) == 0 ) Dlltype = 3;
	if ( Dlltype ) return DoUnarcZipfolder(ppxa, hWnd, param, Dlltype);
*/
	UnAllMode = memcmp(param, T("!all,"), TSTROFF(5));
	if ( *DllName == '/' ){ // background 無効
		DllName++;

		if ( UnAllMode != 0 ){
			if ( GetCustDword(T("X_fopw"), 0) != 2 ){
				hMutex = CreateMutex(NULL, FALSE, PPXJOBMUTEX);
				if ( hMutex != NULL ){
					if ( FALSE == WaitJobDialog(hWnd, hMutex, param, 1) ){
						CloseHandle(hMutex);
						return 2; // 中止
					}
				}
			}else{
				if ( WaitJobDialog(hWnd, NULL, param, 1) == FALSE ) return 2; // 中止
			}
		}
		hWnd = NULL;
	}else{
		X_unbg = Get_X_unbg;
		if ( X_unbg ){
			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			TCHAR buf[MAX_PATH + CMDLINESIZE], *p;
			const TCHAR *q;

			si.cb			= sizeof(si);
			si.lpReserved	= NULL;
			si.lpDesktop	= NULL;
			si.lpTitle		= NULL;
			si.dwFlags		= 0;
			si.cbReserved2	= 0;
			si.lpReserved2	= NULL;

			buf[0] = '\"';
			GetModuleFileName(DLLhInst, buf + 1, MAX_PATH);
			p = FindLastEntryPoint(buf + 1);
			p += wsprintf(p, T(PPTRAYEXE) T("\" /C %%u/%s,"), DllName);
			for ( q = param ; *q ; ){	// '%' をエスケープしつつコピー
				if ( *q == '%' ) *p++ = '%';
				*p++ = *q++;
			}
			*p = '\0';

			if ( IsTrue(CreateProcess(NULL, buf, NULL, NULL, FALSE,
					((X_unbg >= 2) ? IDLE_PRIORITY_CLASS : 0) |
					 CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi)) ){
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				return 0;
			}else{
				PPErrorBox(hWnd, buf, PPERROR_GETLASTERROR);
			}
		}
	}
	VFSOn(VFS_DIRECTORY);

	if ( UnAllMode == 0 ){ // All mode
		UN_DLL *uD;
		int i;

		param = NULL;
		uD = undll_list;
		for ( i = 0 ; i < undll_items ; i++, uD++ ){
			if ( tstricmp(uD->DllName, DllName) != 0 ) continue;

			if ( UnArc_Extract(ppxa, (void *)(LONG_PTR)(i + 1), UNARCEXTRACT_ALL, cmd, XEO_NOEDIT) != NO_ERROR ){
				VFSOff();
				return 1;
			}

			X_unbg = Get_X_unbg;
			if ( X_unbg ){
				VFSOff();
				return DoUnarc(ppxa, DllName - 1, hWnd, cmd);
			}
			param = cmd;
			break;
		}
		if ( param == NULL ){
			VFSOff();
			XMessage(hWnd, NULL, XM_GrERRld, MES_ENUD, DllName);
			return 1;
		}
	}

	// ●UnARCExecMain に統合予定
	{
		HWND hTempWnd = NULL;

		if ( hWnd == NULL ){ // ダイアログのフォーカス制御用ウィンドウを作成
			WNDCLASS UnarcTempClass;

			if ( hBaseWnd == NULL ) hBaseWnd = GetForegroundWindow();
			UnarcTempClass.style		= 0;
			UnarcTempClass.lpfnWndProc	= DefWindowProc;
			UnarcTempClass.cbClsExtra	= 0;
			UnarcTempClass.cbWndExtra	= 0;
			UnarcTempClass.hInstance	= NULL;
			UnarcTempClass.hIcon		= LoadIcon(DLLhInst, MAKEINTRESOURCE(Ic_FOP));
			UnarcTempClass.hCursor		= NULL;
			UnarcTempClass.hbrBackground	= NULL;
			UnarcTempClass.lpszMenuName	= NULL;
			UnarcTempClass.lpszClassName	= UnarcTempClassName;
			RegisterClass(&UnarcTempClass);

			hWnd = hTempWnd = CreateWindow(UnarcTempClassName, NilStr,
					WS_POPUPWINDOW, 0, 0, 0, 0, NULL, NULL, DLLhInst, NULL);
			ShowWindow(hTempWnd, SW_SHOW);
			{
				MSG msg;

				while ( PeekMessage(&msg, hTempWnd, 0, 0, PM_REMOVE) ){
					if ( msg.message == WM_QUIT ) break;
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}

		SetJobTask(hWnd, JOBSTATE_STARTJOB | JOBSTATE_ARC_PACK);
		VFSArchiveSection(VFSAS_ENTER | VFSAS_UNDLL | VFSAS_SERIALIZE, NULL);
		result = DoUnarcMain(DllName, hWnd, param, ppxa);
		VFSArchiveSection(VFSAS_LEAVE | VFSAS_UNDLL | VFSAS_SERIALIZE, NULL);
		SetJobTask(hWnd, JOBSTATE_ENDJOB);

		if ( hTempWnd != NULL ){
			if ( GetFocus() != NULL ) SetForegroundWindow(hBaseWnd);
			DestroyWindow(hTempWnd);
		}

		if ( hMutex != NULL ){
			ReleaseMutex(hMutex);
			CloseHandle(hMutex);
		}
	}
	VFSOff();
	return result;
}
//--------------------------------------------------------------- UnDLL default
int VFSUnWildCheckMain(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	FN_REGEXP fn;
	int result;

	MakeFN_REGEXP(&fn, uD->CheckWildcard);
#ifdef UNICODE
	result = FilenameRegularExpression(vcs->filenameW, &fn);
#else
	result = FilenameRegularExpression(vcs->filename, &fn);
#endif
	FreeFN_REGEXP(&fn);
	return result;
}

#define VFSUnWildCheckTemplate\
	if ( uD->CheckWildcard != NULL ){\
		if ( VFSUnWildCheckMain(uD, vcs) == 0 ) return FALSE;\
	}


void SendToCommonLog(const TCHAR *text)
{
	COPYDATASTRUCT copydata;

	if ( Sm->hCommonLogWnd == NULL ) return;
	copydata.dwData = K_WINDDOWLOG;
	copydata.cbData = TSTRSIZE32(text);
	copydata.lpData = (PVOID)text;
	SendMessage(Sm->hCommonLogWnd, WM_COPYDATA, 0, (LPARAM)&copydata);
}

#ifdef UNICODE
BOOL VFSUnCheckMainFunction(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	if ( uD->flags & UNDLLFLAG_SKIP_OPENED ){
		HANDLE hFile;

		if ( (hFile = CreateFile(vcs->filenameW, GENERIC_READ, 0, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) )
				== INVALID_HANDLE_VALUE ){
			SendToCommonLog(MessageText(MES_ESHO));
			return FALSE;
		}
		CloseHandle(hFile);
	}
	if ( uD->flags & UNDLLFLAG_SKIPDLLCHECK ) return TRUE;

	if ( uD->UnCheckArchiveW != NULL ){
		return uD->UnCheckArchiveW(vcs->filenameW, 0);
	}else if ( uD->SetUnicodeMode(TRUE) ){
		int r = uD->UnCheckArchive(vcs->filename8, 0);
		uD->SetUnicodeMode(FALSE);
		return r;
	}else{
		return uD->UnCheckArchive(vcs->filename, 0);
	}
}
#else
BOOL VFSUnCheckMainFunction(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	if ( uD->flags & UNDLLFLAG_SKIP_OPENED ){
		HANDLE hFile;

		if ( (hFile = CreateFile(vcs->filename, GENERIC_READ, 0, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) )
				== INVALID_HANDLE_VALUE ){
			SendToCommonLog(MessageText(MES_ESHO));
			return FALSE;
		}
		CloseHandle(hFile);
	}
	if ( uD->flags & UNDLLFLAG_SKIPDLLCHECK ) return TRUE;

	return uD->UnCheckArchive(vcs->filename, 0);
}
#endif

BOOL VFS_check_def(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	if ( vcs->floatheader > 0 ) return FALSE;
	VFSUnWildCheckTemplate;
	if ( (uD->hadd == NULL) && (LoadUnDLL(uD) == FALSE) ) return FALSE;
	return VFSUnCheckMainFunction(uD, vcs);
}
//-------------------------------------------------------------- UnDLL Template
#define VFSUnCheckTemplate(offset, size, first, second) \
{\
	BYTE *p, *maxp;\
	DWORD fsize;\
	int s;\
\
	p = vcs->header + offset;\
	fsize = vcs->fsize - size - offset;\
	if ( vcs->floatheader ){\
		maxp = vcs->header + fsize;\
	}else{\
		fsize = size;\
		maxp = vcs->header;\
	}\
	if ( (s = (int)fsize) <= 0 ) return FALSE;\
	while( (p = memchr(p, first, s)) != NULL ){\
		if ( second ){\
			if ( (uD->hadd == NULL) && (LoadUnDLL(uD) == FALSE) ) return FALSE;\
			return VFSUnCheckMainFunction(uD, vcs);\
		}\
		s = ToSIZE32_T(maxp - ++p);\
		if ( s <= 0 ) break;\
	}\
}
//--------------------------------------------------------------- UnDLL LHA
BOOL VFS_check_LHA(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(2, 10, '-', (*(p+1) == 'l') && ( (*(p+2) == 'h') || (*(p+2) == 'z') ) && (*(p+4) == '-') );
	return FALSE;
}
//--------------------------------------------------------------- UnDLL CAB
BOOL VFS_check_CAB(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 20, 'M', (*(p+1) == 'S') && (*(p+2) == 'C') && (*(p+3) == 'F') && (*(WORD *)(p + 18) < 0x10) );
	return FALSE;
}
//--------------------------------------------------------------- UnDLL Rar
BOOL VFS_check_Rar(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 10, 'R', (*(p+1) == 'a') && (*(p+2) == 'r') && (*(p+3) == '!'));
	return FALSE;
}
//--------------------------------------------------------------- UnDLL ARJ
BOOL VFS_check_ARJ(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 14, '\x60', (*(p + 1) == 0xEA) && (*(p + 10) == 2) && ( (*(p + 2) + (*(p + 3) << 8)) <= 2600) );

	return FALSE;
}
//--------------------------------------------------------------- UnDLL GCA
BOOL VFS_check_GCA(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 8, 'G', (*(p+1) == 'C') && (*(p+2) == 'A'));
	return FALSE;
}
//--------------------------------------------------------------- UnDLL 7-Zip
BOOL VFS_check_7Zip(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 8, '7', (*(p+1) == 'z') && (*(p+2) == 0xbc) && (*(p+3) == 0xaf));
	return VFS_check_ZIP(uD, vcs);
}
//--------------------------------------------------------------- UnDLL ZIP
BOOL VFS_check_ZIP(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	VFSUnWildCheckTemplate;
	VFSUnCheckTemplate(0, 8, 'P', (*(p+1) == 'K') && (*(p+2) == 3) && (*(p+3) == 4) );
	return FALSE;
}
//--------------------------------------------------------------- UnDLL TAR
BOOL VFS_check_TAR(UN_DLL *uD, VUCHECKSTRUCT *vcs)
{
	BYTE *header;
	DWORD fsize;

	if ( vcs->floatheader > 0 ) return FALSE;
	VFSUnWildCheckTemplate;
	header = vcs->header;
	fsize = vcs->fsize - 2;

	if (((fsize >= 0x120) && !memcmp(header + 0x101, "ustar", 5)) ||	// tar
		((fsize >= 0x10) &&
			(!memcmp(header, "\x1f\x9d\x90", 3) ||		// compress
			 !memcmp(header, "\xc7\x71", 2) ||			// CPIO
			 !memcmp(header, "!<arch>\xa", 8) ||		// ar
			 !memcmp(header, "BZh", 3) ||				// bzip2
			 !memcmp(header, "\x1f\x8b", 2) ||			// gzip
			 !memcmp(header, "\xed\xab\xee\xdb", 4) ||	// rpm
			 !memcmp(header, "2.0", 3) ||				// deb
			 !memcmp(header, "\xfd" "7zXZ", 6) ||		// xz
			 !memcmp(header, "\x5d\0\0\x4", 5) ||		// lzma
			 !memcmp(header, "\x5d\0\0\x80", 5) ))){		// lzma
		if ( (uD->hadd == NULL) && (LoadUnDLL(uD) == FALSE) ) return FALSE;
		#ifdef UNICODE
			if ( uD->UnCheckArchiveW != NULL ){
				return uD->UnCheckArchiveW(vcs->filenameW, 0);
			}
		#endif
		return uD->UnCheckArchive(vcs->filename, 0);
	}
	return FALSE;
}

#ifdef UNICODE

const TCHAR *ConvertResCmd(UN_DLL *undll, const TCHAR *param, TCHAR *parambuf, DWORD flags)
{
	tstrcpy(parambuf, param);
	if ( flags & UNDLLFLAG_RESPONSE_UTF8 ){
		undll->codepage = CP_UTF8;
		tstrreplace(parambuf, T("%@"), T("%@8"));
		tstrreplace(parambuf, T("%a"), T("%a8"));
		return parambuf;
	}else // if ( flags & UNDLLFLAG_RESPONSE_UTF16 )
	{
		undll->codepage = CP_PPX_UCF2;
		tstrreplace(parambuf, T("%@"), T("%@U"));
		tstrreplace(parambuf, T("%a"), T("%aU"));
		return parambuf;
	}
//	return param;
}
#endif


VFSDLL ERRORCODE PPXAPI UnArc_Extract(PPXAPPINFO *info, const void *dt_opt, int extractmode, TCHAR *extract, int flags)
{
	const TCHAR *param;
	#ifdef UNICODE
	TCHAR parambuf[CMDLINESIZE];
	#endif
	DWORD result;
	UN_DLL *undll;
	int delta = (int)(DWORD_PTR)dt_opt - 1;

	if ( (delta < 0) || (delta >= undll_items) ){
		return ERROR_INVALID_TARGET_HANDLE;
	}
	undll = undll_list + delta;
	if ( undll->hadd == NULL ){
		if ( LoadUnDLL(undll) == FALSE ) return ERROR_INVALID_TARGET_HANDLE;
	}
	param =  (extractmode == UNARCEXTRACT_ALL) ? undll->AllExtCMD :
			((extractmode == UNARCEXTRACT_SINGLE) ?
				undll->SingleExtCMD : undll->PartExtCMD);

	if ( *param == '\0' ){
		XMessage(NULL, NULL, XM_GrERRld, T("no extract setting"));
		return ERROR_CANCELLED;
	}

	if ( undll->flags & UNDLLFLAG_FIX_BRACKET ){
		setflag(flags,
			(undll->flags & UNDLLFLAG_SKIP_OPENED) ?
			XEO_PATHSLASH : (XEO_PATHSLASH | XEO_PATHESCAPE) );
	}

	#ifdef UNICODE
	if ( undll->UnarcW != NULL ){
		param = ConvertResCmd(undll, param, parambuf, undll->flags);
	}else if ( undll->SetUnicodeMode(TRUE) ){
		param = ConvertResCmd(undll, param, parambuf, UNDLLFLAG_RESPONSE_UTF8);
		result = PP_ExtractMacro(info->hWnd, info, NULL, param, extract, flags);
		tstrreplace(extract, T(">"), T(" "));
		undll->SetUnicodeMode(FALSE);
		return result;
	}else{
		undll->codepage = CP_ACP;
	}
	#endif
	result = PP_ExtractMacro(info->hWnd, info, NULL, param, extract, flags);
	tstrreplace(extract, T(">"), T(" "));
	return result;
}

void SetLogBadCrTextFix(PPXAPPINFO *loginfo, const TCHAR *title, const TCHAR *text, int result)
{
	TCHAR buf[LOGLENGTH + 0x1000], *dest;
	TCHAR c;

	for (;;){
		dest = buf;
		for ( ;; ){
			c = *text++;
			if ( c == '\n' ){
				if ( dest > (buf + LOGLENGTH + 0xb00) ){
					*dest = '\0';
					text--;
					break;
				}
				*dest++ = '\r';
			}
			*dest++ = c;
			if ( c == '\0' ) break;
		}
		SetLogBadCrText(loginfo, title, buf, result);
		if ( c == '\0' ) break;
	}
}

void SetLogBadCrText(PPXAPPINFO *loginfo, const TCHAR *title, const TCHAR *text, int result)
{
	TCHAR *p;

	if ( (loginfo == NULL) || (PPxInfoFunc(loginfo, PPXCMDID_EXTREPORTTEXT, NULL) <= PPXCMDID_EXTREPORTTEXT_CLOSE) ){
		if ( (Sm->hCommonLogWnd == NULL) && (result == 0) ) return;
		loginfo = NULL;
	}

	p = tstrchr(text, '\n');
	if ( (p != NULL) && ((p <= text) || (*(p - 1) != '\r')) ){
		SetLogBadCrTextFix(loginfo, title, text, result);
		return;
	}

	if ( loginfo != NULL ){
		PPxInfoFunc(loginfo, PPXCMDID_EXTREPORTTEXT, (TCHAR *)text);
	}else if ( Sm->hCommonLogWnd == NULL ){
		MessageBox(GetFocus(), text, title, MB_OK);
	}else{
		SendToCommonLog(text);
	}
}

#ifdef UNICODE
void SetLogBadCrTextA(PPXAPPINFO *loginfo, const TCHAR *title, const char *text, int result, UINT codepage)
{
	WCHAR textW[LOGLENGTH + (LOGLENGTH / 2)];

	MultiByteToWideCharU8(codepage, 0, text, -1, textW, TSIZEOFW(textW));
	SetLogBadCrText(loginfo, title, textW, result);
}
#endif

#define DLOG_NOLOG 0
#define DLOG_ERRORLOG 1
#define DLOG_ALLLOG 2

int USEFASTCALL CheckLogOutput(int result)
{
	int X_dlog = DLOG_ALLLOG;

	GetCustData(T("X_dlog"), &X_dlog, sizeof(X_dlog));
	if ( X_dlog != DLOG_ERRORLOG ) return X_dlog;
	return result;
}

int UnArc_ExecMain(const UN_DLL *uD, HWND hWnd, const TCHAR *Cmd, PPXAPPINFO *loginfo)
{
	HWND hUnarcChildWnd = NULL, hUnarcParentWnd, *hCloseWnd;
	int result;
	HWND hFWnd;
	TCHAR log[LOGLENGTH];
	TCHAR CmdBuf[CMDLINESIZE];
	const TCHAR *extp, *extpl;
	#ifdef UNICODE
		char CmdA[CMDLINESIZE];
	#endif
	THREADSTRUCT *ts C4701CHECK;
	const TCHAR *oldname C4701CHECK;

	hFWnd = hWnd;
	if ( (DWORD_PTR)hFWnd < 2 ) hFWnd = GetFocus();

	extp = tstrchr(Cmd, '<'); // オプション指定有り…オプションの実体を展開
	if ( extp != NULL ){
		TCHAR *cdest;
		const TCHAR *optptr;

		memcpy(CmdBuf, Cmd, TSTROFF(extp - Cmd));
		cdest = CmdBuf + (extp - Cmd);
		*cdest = '\0';
		extp++;
		for (;;){
			if ( *extp == '\0' ) break;
			if ( *extp == '<' ){
				extp++;
				break;
			}
			extpl = extp;
			for (;;){
				if ( *extpl == '\0' ) break;
				if ( *extpl == '<' ) break;
				if ( *extpl == ',' ) break;
				extpl++;
			}
			if ( *extpl == '\0' ) break;

			optptr = uD->params;
			for (;;){
				if ( *optptr == '\0' ) break;
				if ( (*optptr == 'p') && (*(optptr + 1) == 'o') &&
					 (memcmp(optptr + 3, extp, TSTROFF(extpl - extp)) == 0) &&
					 (*(optptr + 3 + (extpl - extp)) == '=') ){
					tstrcpy(cdest, (optptr + 3 + (extpl - extp) + 1) );
					cdest += tstrlen(cdest);
					*cdest++ = ' ';
					break;
				}
				optptr += tstrlen(optptr) + 1;
			}
			if ( *extpl == '<' ) break;
			extp = extpl + 1;
		}
		memcpy(cdest, extp, TSTRSIZE(extp));
		Cmd = CmdBuf;
	}

	/* unrar32 は終了時のフォーカス問題があるので対策
		…hParentWnd が子ウィンドウでないとフォーカスが
			おかしくなるので仮ウィンドウを作成する
	*/
	if ( uD->flags & UNDLLFLAG_FIX_DUMMYWINDOW ){
		if ( hFWnd != NULL ){
			hUnarcParentWnd = GetParent(hFWnd);
			if ( hUnarcParentWnd != NULL ){
				hUnarcChildWnd = hFWnd;
				hCloseWnd = NULL;
			}else{
				hUnarcParentWnd = hFWnd;
				hCloseWnd = &hUnarcChildWnd;
			}
		}else{
			hUnarcParentWnd = NULL;
			hCloseWnd = &hUnarcParentWnd;
		}
		if ( hUnarcChildWnd == NULL ){
			MSG msg;
			WNDCLASS UnarcTempClass = {
				0, NULL, 0, 0,		// style～cbWndExtra
				NULL, NULL, NULL, NULL, NULL,	// hInstance～lpszMenuName
				UnarcTempClassName			// lpszClassName
			};

			UnarcTempClass.hIcon = LoadIcon(DLLhInst, MAKEINTRESOURCE(Ic_FOP));
			UnarcTempClass.lpfnWndProc = DefWindowProc;
			UnarcTempClass.hInstance = DLLhInst;
			RegisterClass(&UnarcTempClass);
			if ( hUnarcParentWnd == NULL ){ // 仮の親
				hUnarcParentWnd = CreateWindow(UnarcTempClassName, NilStr,
						WS_POPUPWINDOW | WS_MINIMIZEBOX, 0, 0, 0, 0,
						NULL, NULL, DLLhInst, NULL);
				ShowWindow(hUnarcParentWnd, SW_SHOW);
			}
			// 仮の子
			hUnarcChildWnd = CreateWindow(UnarcTempClassName, NilStr, WS_CHILD,
					0, 0, 0, 0, hUnarcParentWnd, NULL, DLLhInst, NULL);

			while ( PeekMessage(&msg, hUnarcChildWnd, 0, 0, PM_REMOVE) ){
				if ( msg.message == WM_QUIT ) break;
//				TranslateMessage(&msg); // ダミーウィンドウなのでキー入力無し
				DispatchMessage(&msg);
			}
		}
	}else{ // unrar32以外
		hUnarcChildWnd = hFWnd;
		hCloseWnd = NULL;
	}
	log[0] = '\0';

	ts = GetCurrentThreadInfo();
	if ( ts != NULL ){
		oldname = ts->ThreadName;
		ts->ThreadName = uD->DllName;
	}
	#ifdef UNICODE
		if ( (uD->UnarcW != NULL) && (uD->codepage != CP_ACP) ){
			result = uD->UnarcW(hUnarcChildWnd, Cmd, log, TSIZEOF(log));
			if ( result ){
				if ( result == UNARC_USER_CANCEL ){
					result = 0;
					log[0] = '\0'; // キャンセル時はログを消す
				}
				if ( uD->flags & UNDLLFLAG_FIX_UNARCRESULT ){
					// CAB32.DLL は正常終了でも-1を返すようだ。また、0x8000未満はスキップファイル数？
					if ( result < 0x8000 ) result = 0; // -1 と <0x8000 のときは正常終了
				}
			}
			if ( result && (log[0] == '\0') ){
				wsprintf(log, L"APIw error : %x : %s", result, Cmd);
			}

			if ( ts != NULL ) ts->ThreadName = oldname; // c4701ok
			if ( (log[0] != '\0') && (CheckLogOutput(result) != 0) ){
				SetLogBadCrText(loginfo, uD->DllName, log, result);
			}
		}else{
			UINT codepage;

			if ( (uD->codepage != CP_UTF8) || !uD->SetUnicodeMode(TRUE) ){
				codepage = CP_ACP;
			}else{
				codepage = CP_UTF8;
			}
			WideCharToMultiByteU8( codepage, 0, Cmd, -1, CmdA, CMDLINESIZE, NULL, NULL);
			result = uD->Unarc(hUnarcChildWnd, CmdA, (char *)log, sizeof(log));
			if ( result ){
				if ( result == UNARC_USER_CANCEL ){
					result = 0;
					log[0] = '\0'; // キャンセル時はログを消す
				}
				if ( uD->flags & UNDLLFLAG_FIX_UNARCRESULT ){
					// CAB32.DLL は正常終了でも-1を返すようだ。また、0x8000未満はスキップファイル数？
					if ( result < 0x8000 ) result = 0; // -1 と <0x8000 のときは正常終了
				}
			}

			if ( codepage == CP_UTF8 ) uD->SetUnicodeMode(FALSE);
			if ( result && (log[0] == '\0') ){
				wsprintfA((char *)log, "APIa error : %x : %s", result, CmdA);
				codepage = CP_ACP;
			}
			if ( ts != NULL ) ts->ThreadName = oldname; // c4701ok
			if ( (log[0] != '\0') && (CheckLogOutput(result) != 0) ){
				SetLogBadCrTextA(loginfo, uD->DllName, (char *)log, result, codepage);
			}
		}
	#else
		result = uD->Unarc(hUnarcChildWnd, Cmd, log, sizeof(log));
		if ( result ){
			if ( result == UNARC_USER_CANCEL ){
				result = 0;
				log[0] = '\0'; // キャンセル時はログを消す
			}
			if ( uD->flags & UNDLLFLAG_FIX_UNARCRESULT ){
				// CAB32.DLL は正常終了でも-1を返すようだ。また、0x8000未満はスキップファイル数？
				if ( result < 0x8000 ) result = 0; // -1 と <0x8000 のときは正常終了
			}
		}
		if ( result && (log[0] == '\0') ){
			wsprintf(log, "API error : %x : %s", result, Cmd);
		}
		if ( ts != NULL ) ts->ThreadName = oldname; // c4701ok
		if ( (log[0] != '\0') && (CheckLogOutput(result) != 0) ){
			SetLogBadCrText(loginfo, uD->DllName, log, result);
		}
	#endif
	if ( hCloseWnd != NULL ) DestroyWindow(*hCloseWnd);
	return result;
}

int RunUnARCExec(PPXAPPINFO *loginfo, const void *dt_opt, TCHAR *param, const TCHAR *tmppath)
{
	HWND hOldFocusWnd;
	int result;
	UN_DLL *undll;
	int delta = (int)(DWORD_PTR)dt_opt - 1;

	if ( (delta < 0) || (delta >= undll_items) ) return -1;
	undll = undll_list + delta;
	if ( (undll->hadd == NULL) && (LoadUnDLL(undll) == FALSE) ) return -1;

	VFSArchiveSection(VFSAS_ENTER | VFSAS_UNDLL, NULL);
	hOldFocusWnd = GetFocus();

	result = UnArc_ExecMain(undll, loginfo->hWnd, param, loginfo);
	SetFocus(hOldFocusWnd);
	VFSArchiveSection(VFSAS_LEAVE | VFSAS_UNDLL, NULL);
	if ( tmppath[0] != '\0' ) DeleteFileL(tmppath);
	return result;
}

VFSDLL void PPXAPI UnArc_Exec(PPXAPPINFO *info, const void *dt_opt, TCHAR *param, HANDLE hBatchfile, const TCHAR *tmppath, DWORD X_unbg, const TCHAR *chopdir)
{
	if ( (hBatchfile != INVALID_HANDLE_VALUE) || X_unbg ){
		const TCHAR *q, *adds;
		const UN_DLL *undll;
		TCHAR buf[CMDLINESIZE * 2], *p;
		int delta = (int)(DWORD_PTR)dt_opt - 1;

		if ( (delta < 0) || (delta >= undll_items) ) return;
		undll = undll_list + delta;

		if ( hBatchfile == INVALID_HANDLE_VALUE ){
			p = buf+wsprintf(buf, T("\"%s\\") T(PPTRAYEXE) T("\" /C "), DLLpath);
		}else{
			p = buf;
		}
		if ( chopdir != NULL ) p += wsprintf(p, T("*makedir \"%s\"%%:"), chopdir);
		adds = (undll->flags & UNDLLFLAG_FIX_IME) ? fixime : NilStr;
		p += wsprintf(p, T("%s%%u/%s,"), adds, undll->DllName);
		#ifdef UNICODE
		if ( undll->codepage != CP_ACP ){
			p += wsprintf(p, (undll->codepage == CP_PPX_UCF2) ? T("!2") : T("!8"));
		}
		#endif
		for ( q = param ; *q ; ){	// '%' をエスケープしつつコピー
			if ( *q == '%' ) *p++ = '%';
			*p++ = *q++;
		}
		*p = '\0';
		if ( tmppath[0] != '\0' ){
			p += wsprintf(p, T(NL) T("*delete \"%s\""), tmppath);
		}
		if ( chopdir != NULL ){
			wsprintf(p, T(NL) T("*chopdir \"%s\""), chopdir);
		}
		if ( hBatchfile == INVALID_HANDLE_VALUE ){
			STARTUPINFO si;
			PROCESS_INFORMATION pi;

			si.cb			= sizeof(si);
			si.lpReserved	= NULL;
			si.lpDesktop	= NULL;
			si.lpTitle		= NULL;
			si.dwFlags		= 0;
			si.cbReserved2	= 0;
			si.lpReserved2	= NULL;

			if ( IsTrue(CreateProcess(NULL, buf, NULL, NULL, FALSE,
					((X_unbg >= 2) ? IDLE_PRIORITY_CLASS : 0) |
							CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi)) ){
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				return;
			}else{
				PPErrorBox(info->hWnd, T("Unarc"), PPERROR_GETLASTERROR);
			}
		}else{
			DWORD temp;

			WriteFile(hBatchfile, buf, TSTRLENGTH32(buf), &temp, NULL);
			WriteFile(hBatchfile, T(NL), sizeof(TCHAR) * 2, &temp, NULL);
		}
	}else{
		RunUnARCExec(info, dt_opt, param, tmppath);
	}
}

const TCHAR *GetPackParams(const TCHAR *str, TCHAR *arcnamebuf, TCHAR **param, TCHAR **term)
{
	TCHAR *dest = arcnamebuf;

	for (;;){
		if ( *str == '\0' ) return NULL;
		if ( *str == '=' ) break;

		if ( *str == ':' ){
			if ( *param != NULL ) *param = dest;
		}

		*dest++ = *str++;
	}

	while ( (arcnamebuf < dest) && ((*(dest-1) == ' ') || (*(dest-1) == '\t')) ) dest--;
	*term = dest;
	*dest = '\0';

	str++;
	SkipSpace(&str);
	return str;
}

void GetPackMenu(HMENU hMenuDest, ThSTRUCT *thMenuData, DWORD *PopupID)
{
	HMENU hTypeMenu;
	HMENU hOtherMenu;

	UN_DLL *uD;
	TCHAR parambuf[CMDLINESIZE];
	TCHAR buf[CMDLINESIZE];
	TCHAR PackName[MAX_PATH];
	TCHAR PackOption[MAX_PATH];
	int i;
	TCHAR *comment, *term;
	BOOL gmode;

	ThGetString(&ProcessStringValue, T("Edit_PackMode"), parambuf, CMDLINESIZE);
	gmode = (parambuf[0] == 'g');

	hTypeMenu = CreatePopupMenu();
	if ( undll_items ){
		PackName[0] = '\0';
		PackOption[0] = '\0';
		if ( gmode ){
			GetCustTable(StrCustOthers, T("PackName"), PackName, sizeof(PackName));
			GetCustTable(StrCustOthers, T("PackOption"), PackOption, sizeof(PackOption));
		}
		uD = undll_list;
		for ( i = 0 ; i < undll_items ; i++, uD++ ){
			const TCHAR *ptr;

			if ( uD->flags & UNDLLFLAG_DISABLE_PACK ) continue;
			if ( uD->VUCheck == VFSVUCheckError ) continue; // 使用不可
			ptr = uD->params;
			for (;;){
				if ( *ptr == '\0' ) break;
				if ( *ptr == 'p' ){
					if ( (uD->hadd == NULL) && (LoadUnDLL(uD) == FALSE) ){
						break;
					}

					// 書庫種類等
					if ( *(ptr + 1) == ':' ){
						const TCHAR *param;

						param = GetPackParams(ptr + 2, parambuf, &comment, &term);
						if ( param != NULL ){
							TCHAR *p;

							wsprintf(term, T(" - %s"), uD->DllName);
							p = tstrchr(term, '.');
							if ( p != NULL ) *p = '\0';
							AppendMenuString(hTypeMenu, (*PopupID)++, parambuf);
							wsprintf(parambuf, T("%%u%s,%s"), uD->DllName, param);
							ThAddString(thMenuData, parambuf);
						}
					// オプション
					}else if ( (*(ptr + 1) == 'o') && (*(ptr + 2) == ':') ){
						TCHAR *param;
						int cmp;

						comment = NULL;
						param = (TCHAR *)GetPackParams(ptr + 3, parambuf, &comment, &term);
						if ( param != NULL ){
							TCHAR *p;

							if ( comment == NULL ){
								tstrcpy(term + 1, uD->DllName);
								p = tstrchr(term + 1, '.');
								if ( p != NULL ) *p = '\0';
								cmp = tstrstr(PackName, term + 1) == NULL;
								p = parambuf;
							}else{
								*comment = '\0';
								wsprintf(term + 1, T("%s - %s"), parambuf, uD->DllName);
								p = tstrchr(term + 1, '.');
								if ( p != NULL ) *p = '\0';
								cmp = tstrcmp(PackName, term + 1);
								p = comment + 1;
							}
							if ( cmp == 0 ){
								TCHAR *strp;


								if ( gmode ){
									strp = tstrstr(PackOption, p);
									if ( (strp != NULL) &&
										 (*(strp + tstrlen(p)) == ',' ) ){
										cmp = 1;
									}
									AppendMenuCheckString(hMenuDest, (*PopupID)++, p, cmp);
									wsprintf(buf, T("!*togglecustword _others:PackOption,%s,"), p);
									ThAddString(thMenuData, buf);
								}
							}
						}
					}
				}
				ptr += tstrlen(ptr) + 1;
			}
		}
	}
	AppendMenuString(hTypeMenu, (*PopupID)++, StrPackZipFolderTitle);
	ThAddString(thMenuData, StrPackZipFolderCommand);

	buf[0] = '0';
	GetCustTable(StrCustOthers, T("PackIndiv"), buf, sizeof(buf));
	AppendMenuCheckString(hMenuDest, (*PopupID)++, MES_PACI, buf[0] == '1');
	wsprintf(buf, T("!*setcust _others:PackIndiv=%c"), buf[0] == '1' ? '0' : '1');
	ThAddString(thMenuData, buf);

	AppendMenu(hMenuDest, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenuDest, MF_EPOP, (UINT_PTR)hTypeMenu, MessageText(MES_PACT));

	hOtherMenu = CreatePopupMenu();
	PP_AddMenu(NULL, NULL, hOtherMenu, PopupID, T("M_xpack"), thMenuData);
	AppendMenu(hMenuDest, MF_EPOP, (UINT_PTR)hOtherMenu, MessageText(MES_PACO));
}

BOOL FindPackType(const TCHAR *dllname, TCHAR *arcname, TCHAR *arccommand)
{
	UN_DLL *uD;
	int i;

	uD = undll_list;
	for ( i = 0 ; i < undll_items ; i++, uD++ ){
		const TCHAR *ptr;
		TCHAR *dstp;

		if ( tstricmp(uD->DllName, dllname) != 0 ) continue;

		dstp = arcname + tstrlen(arcname);
		*dstp++ = '=';

		ptr = uD->params;
		for (;;){
			if ( *ptr == '\0' ) break;
			if ( memcmp(ptr, arcname, TSTROFF(dstp - arcname)) == 0 ){
				wsprintf(arccommand, T("%%u%s,%s"), dllname, ptr + (dstp - arcname));
				*(dstp - 1) = '\0';
				return TRUE;
			}
			ptr += tstrlen(ptr) + 1;
		}
		*(dstp - 1) = '\0';
		break;
	}
	return FALSE;
}
