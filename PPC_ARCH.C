/*-----------------------------------------------------------------------------
	Paper Plane cUI		書庫操作
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <winioctl.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#pragma hdrstop

typedef struct {
	PPXAPPINFO info;
	PPC_APPINFO *parent;
	ENTRYCELL *Cell;
	TCHAR ResponseName[VFPS];
	TCHAR *DestPathLast,DestPath[VFPS];
	ThSTRUCT thFileList;
	DWORD chop;
} UNPACKINFOSTRUCT;

typedef struct {
	UNPACKINFOSTRUCT info;
	TCHAR *lastpath;
	size_t arclen,oldlastlen;
	const TCHAR *oldlast;
	HANDLE hBatchfile;
	DWORD X_unbg;
} UNUNDIRSTRUCT;

typedef struct {
	PPC_APPINFO *cinfo;
	TCHAR *files;
} PPCGETHTTPSTRUCT;

DWORD WINAPI HttpThread(PPCGETHTTPSTRUCT *pghs);
const TCHAR HttpThreadName[] = T("PPc http");
const TCHAR RUnpackTitle[] = T("Unpack\t");
const TCHAR UnpackTitle[] = MES_TUPA;

#ifndef WINEGCC
	#define Get_X_unbg GetCustXDword(T("X_unbg"),NULL,0)
#else
	#define Get_X_unbg 0 // まだ pptrayがないので無効にする。※VFS_ARCD.Cも
#endif

// 書庫dir再現用の展開
ERRORCODE UnUndirUn(UNUNDIRSTRUCT *uud)
{
	PPC_APPINFO *cinfo;
	ERRORCODE result;
	TCHAR param[CMDLINESIZE + VFPS];

	if ( uud->info.thFileList.top == 0 ) return NO_ERROR;
	cinfo = uud->info.parent;
	uud->info.ResponseName[0] = '\0';

	result = UnArc_Extract(&uud->info.info,cinfo->e.Dtype.ExtData,
			UNARCEXTRACT_PART,param,XEO_NOEDIT);
	uud->info.thFileList.top = 0;
	if ( result != NO_ERROR ) return ERROR_CANCELLED;
	UnArc_Exec(&cinfo->info,cinfo->e.Dtype.ExtData,param,
			uud->hBatchfile,uud->info.ResponseName,uud->X_unbg,NULL);
	return NO_ERROR;
}

// 書庫dir再現の為の修正
void UnUndirfix(UNUNDIRSTRUCT *uud,ENTRYCELL *cell)
{
	TCHAR *lastentry;
	size_t lastlen;
	const TCHAR *entryname;

	entryname = CellFileName(cell) + uud->arclen;
	lastentry = FindLastEntryPoint(entryname);
	lastlen = TSTROFF(lastentry - entryname);
	if ( (lastlen != uud->oldlastlen) ||
		 (memcmp(entryname,uud->oldlast,uud->oldlastlen) != 0) ){ // 新規 dir
		TCHAR mdpath[VFPS];

		UnUndirUn(uud); // 溜めたファイルを展開
		uud->oldlast = entryname;
		uud->oldlastlen = lastlen;
		memcpy(mdpath,entryname,lastlen);
		mdpath[lastlen / sizeof(TCHAR)] = '\0';
		if ( lastlen ){
			tstrcpy(uud->info.DestPathLast,mdpath);
			MakeDirectories(uud->info.DestPath,NULL);
		}else{
			*uud->info.DestPathLast = '\0';
		}
	}
	if ( !(cell->f.dwFileAttributes &
				(FILE_ATTRIBUTEX_FOLDER | FILE_ATTRIBUTE_DIRECTORY)) &&
			(*lastentry != '\0') ){ // ファイルのみ登録
		ThAddString(&uud->info.thFileList,CellFileName(cell));
	}
}

ERRORCODE UnUnsubdir(UNUNDIRSTRUCT *uud,const TCHAR *dirname)
{
	ENTRYDATAOFFSET index,maxi;
	size_t dirsize;
	TCHAR dircheckname[VFPS];
	PPC_APPINFO *cinfo;

	CatPath(dircheckname,(TCHAR *)dirname,NilStr);
	dirsize = TSTRLENGTH(dircheckname);
	cinfo = uud->info.parent;
	maxi = cinfo->e.cellDataMax;
	for ( index = 0 ; index < maxi ; index++ ){
		ENTRYCELL *cell;

		cell = &CELdata(index);
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTEX_FOLDER ) continue;
		if ( memcmp(CellFileName(cell),dircheckname,dirsize) == 0 ){
			if ( !(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
				UnUndirfix(uud,cell);
			}
		}
	}
	return NO_ERROR;
}

// 全展開用
DWORD_PTR USECDECL UnpackInfoFunc_All(UNPACKINFOSTRUCT *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
//		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)…無視
//		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)…無視
//		case PPXCMDID_NEXTENUM:		// 次へ…無視
//		case PPXCMDID_ENDENUM:		// 列挙終了…無視
//		case PPXCMDID_CHECKENUM:	//次の列挙は？…無視

//		case '0':	// 自分自身へのパス…親に任せる

		case '1': // 書庫ファイル名(フルパス)
			VFSFullPath(uptr->enums.buffer,(TCHAR *)GetCellFileName(ppxa->parent,ppxa->Cell,uptr->enums.buffer),ppxa->parent->RealPath);
			break;

		case '2': // 展開先
			tstrcpy(uptr->enums.buffer,ppxa->DestPath);
			break;

		case 'C': // 書庫名
		case 'R':
			tstrcpy(uptr->enums.buffer,CellFileName(ppxa->Cell));
			break;

		case 'X': // 書庫名
		case 'Y':
			tstrcpy(uptr->enums.buffer,CellFileName(ppxa->Cell));
			*(uptr->enums.buffer + ppxa->Cell->ext) = '\0';
			break;

		case 'T': // 書庫名
		case 't':
			if ( ppxa->Cell->ext ){
				tstrcpy(uptr->enums.buffer,CellFileName(ppxa->Cell) + ppxa->Cell->ext + 1);
			}else{
				*uptr->enums.buffer = '\0';
			}
			break;

		case 'F':	// %F 処理..DLLの置きかえ処理に任せる
			return 0;

		case PPXCMDID_ENUMATTR:
			uptr->num = ppxa->Cell->f.dwFileAttributes;
			break;

		default:
			return ppxa->parent->
					info.Function(&ppxa->parent->info,cmdID,uptr);
	}
	return 1;
}

// 部分展開、展開先指定付き用
DWORD_PTR USECDECL UnpackInfoFunc2(UNPACKINFOSTRUCT *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case '2':
			tstrcpy(uptr->enums.buffer,ppxa->DestPath);
			break;

		case PPXCMDID_GETTMPFILENAME:
			tstrcpy(ppxa->ResponseName,uptr->str);
			break;

		default:
			return ppxa->parent->
					info.Function(&ppxa->parent->info,cmdID,uptr);
	}
	return 1;
}

// 部分展開、展開先は入力させる用
DWORD_PTR USECDECL UnpackInfoFunc4(UNPACKINFOSTRUCT *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_GETTMPFILENAME:
			tstrcpy(ppxa->ResponseName,uptr->str);
			break;

		default:
			return ppxa->parent->
					info.Function(&ppxa->parent->info,cmdID,uptr);
	}
	return 1;
}

// 階層付き部分展開
DWORD_PTR USECDECL UnpackInfoFunc_Tree(UNPACKINFOSTRUCT *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 0;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
			if ( uptr->enums.enumID < (int)ppxa->thFileList.top ){
				uptr->enums.enumID += TSTROFF(tstrlen((const TCHAR *)(ppxa->thFileList.bottom + uptr->enums.enumID)) + 1);
				return 1;
			}
		// case PPXCMDID_ENDENUM へ
		case PPXCMDID_ENDENUM:		//列挙終了
			uptr->enums.enumID = -1;
			return 0;

		case 'C':
			if (uptr->enums.enumID == -1){
				*uptr->enums.buffer = '\0';
				break;
			}
		// case 'R': へ
		case 'R':
			tstrcpy(uptr->enums.buffer,(const TCHAR *)
					(ppxa->thFileList.bottom + uptr->enums.enumID));
			break;

		case '2':
			tstrcpy(uptr->enums.buffer,ppxa->DestPath);
			break;

		case PPXCMDID_GETTMPFILENAME:
			tstrcpy(ppxa->ResponseName,uptr->str);
			break;

		case PPXCMDID_ENUMATTR:
			if (uptr->enums.enumID < 0){
				return 0;
			}
			*(DWORD *)uptr->enums.buffer = FILE_ATTRIBUTE_NORMAL;
			break;

		default:
			return ppxa->parent->
					info.Function(&ppxa->parent->info,cmdID,uptr);
	}
	return 1;
}

BOOL Unpacksub(PPC_APPINFO *cinfo,TCHAR *newpath,int mode)
{
	if ( mode != UFA_EXTRACT ){
		TCHAR *p;

		tstrcpy(newpath,FindLastEntryPoint(cinfo->RealPath));
		if ( (p = tstrchr(newpath,':')) != NULL ) *p = '\0';
		MakeTempEntry(VFPS,newpath,FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_LABEL);
		if ( mode == UFA_PATH ) return FALSE;
	}
	return TRUE;
}

void RunBatchProcess(PPC_APPINFO *cinfo,const TCHAR *batchname,const TCHAR *currentdir,DWORD X_unbg)
{
	TCHAR buf[VFPS + CMDLINESIZE];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	wsprintf(buf,T("\"%s\\") T(PPTRAYEXE) T("\" /B%s"),PPcPath,batchname);

	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;

	if ( IsTrue(CreateProcess(NULL,buf,NULL,NULL,FALSE,
			((X_unbg >= 2) ? IDLE_PRIORITY_CLASS : 0) |
			CREATE_DEFAULT_ERROR_MODE,NULL,currentdir,&si,&pi)) ){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}else{
		PPErrorBox(cinfo->info.hWnd,T("Unarc"),PPERROR_GETLASTERROR);
	}
}

ERRORCODE DoUndll(PPC_APPINFO *cinfo,const TCHAR *destpath,DWORD X_unbg)
{
	TCHAR param[CMDLINESIZE + VFPS];
	const void *dt_opt = cinfo->e.Dtype.ExtData;

	if ( dt_opt == NULL ) return ERROR_INVALID_HANDLE;

	if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ){
		UNPACKINFOSTRUCT info;
		int flags;

		info.info = cinfo->info;
		info.parent = cinfo;
		info.ResponseName[0] = '\0';
		if ( destpath == NULL ){
			info.info.Function = (PPXAPPINFOFUNCTION)UnpackInfoFunc4;
			flags = 0;
		}else{
			tstrcpy(info.DestPath,destpath);
			info.info.Function = (PPXAPPINFOFUNCTION)UnpackInfoFunc2;
			flags = XEO_NOEDIT;
		}
		if ( UnArc_Extract(&info.info,dt_opt,
				UNARCEXTRACT_PART,param,flags) != NO_ERROR ){
				return ERROR_CANCELLED;
		}
		UnArc_Exec(&cinfo->info,dt_opt,param,INVALID_HANDLE_VALUE,info.ResponseName,X_unbg,NULL);
		return NO_ERROR;
	}else{ // 階層再現処理
		ENTRYCELL *cell;
		int work;
		UNUNDIRSTRUCT uud;
		TCHAR tempfilename[VFPS];

		if ( destpath == NULL ){
			GetPairPath(cinfo,uud.info.DestPath);
			CatPath(NULL,uud.info.DestPath,NilStr);
			if ( PPctInput(cinfo,UnpackTitle,uud.info.DestPath,
					TSIZEOF(uud.info.DestPath),PPXH_DIR_R,PPXH_DIR) <= 0 ){
				return ERROR_CANCELLED;
			}
			CatPath(NULL,uud.info.DestPath,NilStr);
		}else{
			CatPath(uud.info.DestPath,(TCHAR *)destpath,NilStr);
		}
		uud.X_unbg = X_unbg;
		uud.hBatchfile = INVALID_HANDLE_VALUE;
		if ( X_unbg ){
			MakeTempEntry(VFPS,tempfilename,FILE_ATTRIBUTE_NORMAL);
			uud.hBatchfile = CreateFileL(tempfilename, GENERIC_WRITE,
					0, NULL, CREATE_ALWAYS,
					FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if ( uud.hBatchfile == INVALID_HANDLE_VALUE ) return GetLastError();
		}

		uud.info.DestPathLast = uud.info.DestPath + tstrlen(uud.info.DestPath);
		uud.info.info = cinfo->info;
		uud.info.info.Function = (PPXAPPINFOFUNCTION)UnpackInfoFunc_Tree;
		uud.info.parent = cinfo;
		uud.lastpath = uud.info.DestPath + tstrlen(uud.info.DestPath);
		ThInit(&uud.info.thFileList);
		if ( cinfo->ArcPathMask[0] ){
			uud.arclen = tstrlen(cinfo->ArcPathMask) + 1;
		}else{
			uud.arclen = 0;
		}
		uud.oldlastlen = 0;

		InitEnumMarkCell(cinfo,&work);
		while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
			if ( cell->f.dwFileAttributes & FILE_ATTRIBUTEX_FOLDER ) continue;
			if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
				if ( UnUnsubdir(&uud,CellFileName(cell)) != NO_ERROR ) break;
			}else{
				UnUndirfix(&uud,cell);
			}
		}
		UnUndirUn(&uud);
		ThFree(&uud.info.thFileList);
		if ( uud.hBatchfile != INVALID_HANDLE_VALUE ){
			CloseHandle(uud.hBatchfile);
			RunBatchProcess(cinfo,tempfilename,PPcPath,X_unbg);
		}
	}
	return NO_ERROR;
}

void MakeArcDestDirPath(UNPACKINFOSTRUCT *info)
{
	TCHAR *p;

	if ( info->chop == 0 ) return;

	*info->DestPathLast = '\0';
	CatPath(NULL,info->DestPath,FindLastEntryPoint(CellFileName(info->Cell)) );
	// 拡張子を除去
	p = tstrrchr(info->DestPathLast,'.');
	if ( p != NULL ) *p = '\0';

	// ごみとり
	VFSFixPath(NULL,info->DestPathLast,NULL,0);
}

void UnpackChop(UNPACKINFOSTRUCT *info)
{
	if ( info->chop != 2 ) return;
	PP_ExtractMacro(info->info.hWnd,&info->info,NULL,T("*chopdir \"%2\""),NULL,0);
}

// SUSIE_DEST_DISK に対応していない Plug-in 向けの展開処理
// 例) axpdf.spi
ERRORCODE DoSusie_UseMemA(const SUSIE_DLL *sudll,const char *arcpath,SUSIE_FINFO *fi,const TCHAR *destfullpath)
{
	HANDLE hMap;
	DWORD tmpsize;
	int susieresult;

	HANDLE hFile;
	ERRORCODE result;

	susieresult = sudll->GetFile(arcpath,fi->position,
			(LPSTR)&hMap,SUSIE_SOURCE_DISK | SUSIE_DEST_MEM,NULL,0);
	if ( susieresult != SUSIEERROR_NOERROR ) return ERROR_READ_FAULT;

	hFile = CreateFile(destfullpath,GENERIC_WRITE,0,NULL,
			CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		result = GetLastError();
	}else{
		if ( IsTrue(WriteFile(hFile,GlobalLock(hMap),fi->filesize,&tmpsize,NULL)) ){
			result = NO_ERROR;
		}else{
			result = GetLastError();
		}
		GlobalUnlock(hMap);
		CloseHandle(hFile);
	}
	GlobalFree(hMap);
	return result;
}

const TCHAR VAS_Name[] = T("susie");
void VFSArchiveSection_Leave(void)
{
	VFSArchiveSection(VFSAS_LEAVE | VFSAS_SUSIE,
		(MainThreadID != GetCurrentThreadId()) ?
			PPcMainThreadName : PPcRootMainThreadName);
}

#ifdef UNICODE
ERRORCODE DoSusie_partW(PPC_APPINFO *cinfo,WCHAR *dstT,const SUSIE_DLL *sudll)
{
	ERRORCODE result = NO_ERROR;
	ENTRYCELL *cell;
	HLOCAL fiH;
	SUSIE_FINFOW *fi,*fiorg,*fimax;
	WCHAR *lastpath;
	int work;
	WCHAR srcpath[VFPS];

	tstrcpy(srcpath,cinfo->RealPath);

	lastpath = dstT + strlenW(dstT);

	if ( VFSArchiveSection(VFSAS_ENTER | VFSAS_SUSIE,VAS_Name) < 0 ){
		return ERROR_INVALID_HANDLE;
	}
	if ( SUSIEERROR_NOERROR != sudll->GetArchiveInfoW(srcpath,0,SUSIE_SOURCE_DISK,(HLOCAL *)&fiH) ){
		VFSArchiveSection_Leave();
		return ERROR_INVALID_HANDLE;
	}
	fiorg = LocalLock(fiH);
	fimax = (SUSIE_FINFOW *)(BYTE *)((BYTE *)fiorg + LocalSize(fiH));

	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		int namelen;
		WCHAR *cellname;

		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ) continue;
		}
		cellname = CellFileName(cell);
		namelen = ToSIZE32_T(strlenW(cellname));

		for ( fi = fiorg ; (fi < fimax) && (fi->method[0] != 0) ; fi++ ){ // 検索
			WCHAR nameW[VFPS];
			int susieresult;

			if ( fi->filename[0] == '\0' ) continue; // dir ?
			CatPath(nameW,fi->path,fi->filename);
			if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
				WCHAR dirpath[VFPS];
				const WCHAR *pathfirst;
				int depth;

				if ( memcmp(cellname,nameW,namelen * sizeof(WCHAR)) != 0 ) continue;
				if ( (nameW[namelen] != '\0') && (nameW[namelen] != '\\') ){
					continue;
				}
				dirpath[0] = '\0';
				pathfirst = fi->path;
				for ( depth = cinfo->ArcPathMaskDepth ; depth ; depth-- ){
					pathfirst = FindPathSeparator(pathfirst);
					if ( pathfirst == NULL ){
						pathfirst = NilStr;
						break;
					}
					pathfirst++;
				}
				strcpyW(lastpath,pathfirst);
				if ( strcmpW(dirpath,pathfirst) != 0 ){ // subdir 生成
					MakeDirectories(dstT,NULL);
					tstrcpy(dirpath,pathfirst);
				}
			}else{
				if ( strcmpW(cellname,nameW) != 0 ) continue;
				*lastpath = '\0';
			}
			susieresult = sudll->GetFileW(srcpath,fi->position,
					dstT,SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0);
			if ( susieresult != SUSIEERROR_NOERROR ){
/*
				if ( susieresult == SUSIEERROR_NOTSUPPORT ){
					TCHAR destfullpath[VFPS];

					CatPath(destfullpath,dstT,FindLastEntryPoint(nameW));
					result = DoSusie_UseMemW(sudll,TPATH,fi,destfullpath);
					if ( result != NO_ERROR ) break;
				}else{
*/
					result = ERROR_CAN_NOT_COMPLETE;
					break;
//				}
			}
			if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ) break; // このときは対象が１つだけ
		}
		if ( GetAsyncKeyState(VK_ESCAPE) & KEYSTATE_PUSH ){
			result = ERROR_CANCELLED;
			break;
		}
	}
	VFSArchiveSection_Leave();
	LocalUnlock(fiH);
	LocalFree(fiH);
	return result;
}
#endif


ERRORCODE DoSusie_part(PPC_APPINFO *cinfo,const TCHAR *destpath)
{
	ERRORCODE result = NO_ERROR;
	ENTRYCELL *cell;
	HLOCAL fiH;
	SUSIE_FINFO *fi,*fiorg,*fimax;
	char *lastpath;
	const SUSIE_DLL *sudll;
	int work;
	TCHAR srcpath[VFPS];
	#ifdef UNICODE
		char pathA[VFPS];
		char dstA[VFPS];
	#endif
	TCHAR dstT[VFPS];

	CatPath(dstT,(TCHAR *)destpath,NilStr);
	tstrcpy(srcpath,cinfo->RealPath);

	#ifdef UNICODE
		UnicodeToAnsi(srcpath,pathA,sizeof(pathA));
		UnicodeToAnsi(dstT,dstA,sizeof(dstA));
		#define TPATH pathA
		#define TDEST dstA
	#else
		#define TPATH srcpath
		#define TDEST dstT
	#endif
	lastpath = TDEST + strlen(TDEST);

	sudll = VFSGetSusieFuncs(cinfo->e.Dtype.ExtData);
	if ( sudll == NULL ) return ERROR_INVALID_HANDLE;
	#ifdef UNICODE
		if ( sudll->GetArchiveInfoW != NULL ){ // UNICODE 版
			return DoSusie_partW(cinfo,dstT,sudll);
		}
	#endif

	VFSArchiveSection(VFSAS_ENTER | VFSAS_SUSIE,VAS_Name);
	if ( SUSIEERROR_NOERROR != sudll->GetArchiveInfo(TPATH,0,SUSIE_SOURCE_DISK,(HLOCAL *)&fiH) ){
		VFSArchiveSection_Leave();
		return ERROR_INVALID_HANDLE;
	}
	fiorg = LocalLock(fiH);
	fimax = (SUSIE_FINFO *)(BYTE *)((BYTE *)fiorg + LocalSize(fiH));

	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		int namelen;
		#ifdef UNICODE
			char cellname[VFPS];

			UnicodeToAnsi(CellFileName(cell),cellname,sizeof(cellname));
		#else
			char *cellname;

			cellname = CellFileName(cell);
		#endif
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ) continue;
		}
		namelen = strlen32(cellname);

		for ( fi = fiorg ; (fi < fimax) && (fi->method[0] != 0) ; fi++ ){ // 検索
			char nameA[VFPS];
			int susieresult;

			if ( fi->filename[0] == '\0' ) continue; // dir ?
			#ifdef UNICODE
				wsprintfA(nameA,"%s%s",fi->path,fi->filename);
			#else
				CatPath(nameA,fi->path,fi->filename);
			#endif
			if ( cinfo->UseArcPathMask  != ARCPATHMASK_OFF ){
				TCHAR dirpath[VFPS];
				const TCHAR *pathfirst;
				int depth;
				#ifdef UNICODE
					WCHAR DIRNAME[VFPS];

					AnsiToUnicode(fi->path,DIRNAME,VFPS);
				#else
					#define DIRNAME fi->path
				#endif

				if ( memcmp(cellname,nameA,namelen) != 0 ) continue;
				if ( (nameA[namelen] != '\0') && (nameA[namelen] != '\\') ){
					continue;
				}
				dirpath[0] = '\0';
				pathfirst = DIRNAME;
				for ( depth = cinfo->ArcPathMaskDepth ; depth ; depth-- ){
					pathfirst = FindPathSeparator(pathfirst);
					if ( pathfirst == NULL ){
						pathfirst = NilStr;
						break;
					}
					pathfirst++;
				}
				strcpyToA(lastpath,pathfirst,MAX_PATH);
				if ( tstrcmp(dirpath,pathfirst) != 0 ){ // subdir 生成
					#ifdef UNICODE
						WCHAR destpathbuf[VFPS];

						AnsiToUnicode(TDEST,destpathbuf,VFPS);
						MakeDirectories(destpathbuf,NULL);
					#else
						MakeDirectories(TDEST,NULL);
					#endif
					tstrcpy(dirpath,pathfirst);
				}
			}else{
				if ( strcmp(cellname,nameA) != 0 ) continue;
				*lastpath = '\0';
			}
			susieresult = sudll->GetFile(TPATH,fi->position,
					TDEST,SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0);
			if ( susieresult != SUSIEERROR_NOERROR ){
				if ( susieresult == SUSIEERROR_NOTSUPPORT ){
					TCHAR destfullpath[VFPS];

					#ifdef UNICODE
						strcpyAToT(destfullpath,nameA,VFPS);
						VFSFullPath(destfullpath,FindLastEntryPoint(destfullpath),dstT);
					#else
						CatPath(destfullpath,dstT,FindLastEntryPoint(nameA));
					#endif
					result = DoSusie_UseMemA(sudll,TPATH,fi,destfullpath);
					if ( result != NO_ERROR ) break;
				}else{
					result = ERROR_CAN_NOT_COMPLETE;
					break;
				}
			}
			if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ) break; // このときは対象が１つだけ
		}
		if ( GetAsyncKeyState(VK_ESCAPE) & KEYSTATE_PUSH ){
			result = ERROR_CANCELLED;
			break;
		}
	}
	VFSArchiveSection_Leave();
	LocalUnlock(fiH);
	LocalFree(fiH);
	#undef TPATH
	#undef TDEST
	return result;
}

ERRORCODE DoSusie_all(UNPACKINFOSTRUCT *info,const TCHAR *filename,void *dt_opt)
{
	TCHAR param[VFPS];
	TCHAR dirpath[VFPS];
	HLOCAL fiH;
	int dirbug = -1; // -1:未チェック 0:相対dir付加bug有 1:正常spi
	const SUSIE_DLL *sudll;

	sudll = VFSGetSusieFuncs(dt_opt);
	if ( sudll == NULL ) return ERROR_INVALID_HANDLE;

	dirpath[0] = '\0';
	MakeDirectories(info->DestPath,NULL);
	CatPath(param,info->DestPath,NilStr);
	VFSArchiveSection(VFSAS_ENTER | VFSAS_SUSIE,VAS_Name);

#ifdef UNICODE
	if ( sudll->GetArchiveInfoW != NULL ){ // UNICODE 版
		WCHAR *lastpath;
		const WCHAR *oldmakedir = L"";
		SUSIE_FINFOW *fiW,*fimaxW;

		lastpath = param + strlenW(param);
		sudll->GetArchiveInfoW(filename,0,SUSIE_SOURCE_DISK,(HLOCAL *)&fiH);
		fiW = LocalLock(fiH);
		fimaxW = (SUSIE_FINFOW *)(BYTE *)((BYTE *)fiW + LocalSize(fiH));
		for ( ; (fiW < fimaxW) && (fiW->method[0] != 0) ; fiW++ ){
			int susieresult;

			if ( fiW->filename[0] == '\0' ) continue; // dir ?
			*lastpath = '\0';

			if ( fiW->path[0] != '\0' ){
				if ( strcmpW(fiW->path,oldmakedir) != 0 ){
					wsprintf(dirpath,L"%s%s",param,fiW->path);
					MakeDirectories(dirpath,NULL);
					oldmakedir = fiW->path;
				}
				if ( dirbug != 0 ){
					*lastpath = '\\';
					strcpyW(lastpath,fiW->path);
				}
			}

			susieresult = sudll->GetFileW(filename,(LONG_PTR)fiW->position,
					param,SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0);
			if ( susieresult != SUSIEERROR_NOERROR ){
				ERRORCODE result;
				if ( fiW->path[0] && (dirbug < 0) ){ // bug を確認
					*lastpath = '\0';
					if ( sudll->GetFileW(filename,(LONG_PTR)fiW->position,param,
							SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0) ==
							SUSIEERROR_NOERROR ){
						dirbug = 0; // 成功→bug有り
						WriteReport(RUnpackTitle,filename,NO_ERROR);
						continue;
					}
					*lastpath = '\\';
				//	dirbug = 1;
				}
				if ( susieresult == SUSIEERROR_NOTSUPPORT ){
/*
					WCHAR destfullpath[VFPS];

					CatPath(destfullpath,fiW->path[0] ? dirpath : param,fiW->filename);
					result = DoSusie_UseMemA(sudll,TFILENAME2,fi,destfullpath);
					if ( result == NO_ERROR ) continue;
*/
				}else{
					if ( (Combo.Report.hWnd == NULL) && (hCommonLog == NULL) ){
						SetPopMsg(info->parent,ERROR_WRITE_FAULT,NULL);
					}else{
						WriteReport(RUnpackTitle,filename,ERROR_WRITE_FAULT);
					}
					result = ERROR_WRITE_FAULT;
				}
				VFSArchiveSection_Leave();
				LocalUnlock(fiH);
				LocalFree(fiH);
				return result;
			}
			WriteReport(RUnpackTitle,filename,NO_ERROR);
		}
		VFSArchiveSection_Leave();
		LocalUnlock(fiH);
		LocalFree(fiH);
		UnpackChop(info);
		return NO_ERROR;
	}
#endif
	{ // ANSI 版
		const char *oldmakedir = "";
		char *lastpath;
		SUSIE_FINFO *fi,*fimax;
		#ifdef UNICODE
			char paramA[VFPS];
			char filenameA[VFPS];

			UnicodeToAnsi(param,paramA,sizeof(paramA));
			UnicodeToAnsi(filename,filenameA,sizeof(filenameA));
			#define TFILENAME2 filenameA
			#define TDEST paramA
		#else
			#define TFILENAME2 filename
			#define TDEST param
		#endif

		lastpath = TDEST + strlen(TDEST);
		sudll->GetArchiveInfo(TFILENAME2,0,SUSIE_SOURCE_DISK,(HLOCAL *)&fiH);
		fi = LocalLock(fiH);
		fimax = (SUSIE_FINFO *)(BYTE *)((BYTE *)fi + LocalSize(fiH));
		for ( ; (fi < fimax) && (fi->method[0] != 0) ; fi++ ){
			int susieresult;

			if ( fi->filename[0] == '\0' ) continue; // dir ?
			*lastpath = '\0';

			if ( fi->path[0] != '\0' ){
				if ( strcmp(fi->path,oldmakedir) != 0 ){
				#ifdef UNICODE
					WCHAR direntryW[VFPS];

					AnsiToUnicode(fi->path,direntryW,MAX_PATH);
					wsprintf(dirpath,L"%s%s",param,direntryW);
				#else
					wsprintf(dirpath,"%s%s",param,fi->path);
				#endif
					MakeDirectories(dirpath,NULL);
					oldmakedir = fi->path;
				}
				if ( dirbug != 0 ){
					*lastpath = '\\';
					strcpy(lastpath,fi->path);
				}
			}

			susieresult = sudll->GetFile(TFILENAME2,(LONG_PTR)fi->position,
					TDEST,SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0);
			if ( susieresult != SUSIEERROR_NOERROR ){
				ERRORCODE result;
				if ( fi->path[0] && (dirbug < 0) ){ // bug を確認
					*lastpath = '\0';
					if ( sudll->GetFile(TFILENAME2,(LONG_PTR)fi->position,TDEST,
							SUSIE_SOURCE_DISK | SUSIE_DEST_DISK,NULL,0) ==
							SUSIEERROR_NOERROR ){
						dirbug = 0; // 成功→bug有り
						WriteReport(RUnpackTitle,filename,NO_ERROR);
						continue;
					}
					*lastpath = '\\';
					dirbug = 1;
				}
				if ( susieresult == SUSIEERROR_NOTSUPPORT ){
					TCHAR destfullpath[VFPS];

					#ifdef UNICODE
						strcpyAToT(destfullpath,fi->filename,VFPS);
						VFSFullPath(NULL,destfullpath,fi->path[0] ? dirpath : param);
					#else
						CatPath(destfullpath,fi->path[0] ? dirpath : param,fi->filename);
					#endif
					result = DoSusie_UseMemA(sudll,TFILENAME2,fi,destfullpath);
					if ( result == NO_ERROR ) continue;
				}else{
					if ( (Combo.Report.hWnd == NULL) && (hCommonLog == NULL) ){
						SetPopMsg(info->parent,ERROR_WRITE_FAULT,NULL);
					}else{
						WriteReport(RUnpackTitle,filename,ERROR_WRITE_FAULT);
					}
					result = ERROR_WRITE_FAULT;
				}
				VFSArchiveSection_Leave();
				LocalUnlock(fiH);
				LocalFree(fiH);
				return result;
			}
			WriteReport(RUnpackTitle,filename,NO_ERROR);
		}
		VFSArchiveSection_Leave();
		LocalUnlock(fiH);
		LocalFree(fiH);
		UnpackChop(info);
		return NO_ERROR;
	}
}

// ファイルの実行・複写元の準備のために展開を行う
BOOL PPcUnpackForAction(PPC_APPINFO *cinfo, TCHAR *newpath, int mode)
{
	switch (cinfo->e.Dtype.mode){
		case VFSDT_UN:
			if ( Unpacksub(cinfo, newpath, mode) == FALSE ) return TRUE;
			if ( DoUndll(cinfo, newpath, 0) != NO_ERROR ) return FALSE;
			return TRUE;

		case VFSDT_SUSIE: {
			ERRORCODE result;

			if ( Unpacksub(cinfo, newpath, mode) == FALSE ) return TRUE;
			if ( (result = DoSusie_part(cinfo, newpath)) != NO_ERROR ){
				SetPopMsg(cinfo, result, NULL);
				return FALSE;
			}
			return TRUE;
		}
		case VFSDT_FATIMG:
//		case VFSDT_FATDISK:
		case VFSDT_CDIMG:
//		case VFSDT_CDDISK:
		case VFSDT_FTP:
		case VFSDT_HTTP:
		case VFSDT_CABFOLDER:
		case VFSDT_LZHFOLDER:
		case VFSDT_ZIPFOLDER:{
			VFSFILEOPERATION fileop;

			if ( Unpacksub(cinfo, newpath, mode) == FALSE ) return TRUE;
			if ( (fileop.files = GetFiles(cinfo, 0)) == NULL ) return FALSE;
			fileop.action	= FileOperationMode_Copy;
			fileop.src		= cinfo->path;
			fileop.dest		= newpath;
			fileop.option	= NULL;
			fileop.dtype	= cinfo->e.Dtype.mode;
			fileop.info		= &cinfo->info;
			fileop.flags	= VFSFOP_FREEFILES | VFSFOP_AUTOSTART | VFSFOP_SPECIALDEST;
			fileop.hReturnWnd = cinfo->info.hWnd;
			if ( PPxFileOperation(NULL, &fileop) <= 0 ) break;
			return TRUE;
		}
//		default:
//			break;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
ERRORCODE PPcUnpackSelectedEntry(PPC_APPINFO *cinfo, const TCHAR *destpath, const TCHAR *action)
{
	HWND hFWnd;
	TCHAR dst[VFPS];
	ERRORCODE result = NO_ERROR;

	hFWnd = GetForegroundWindow();
	GetPairPath(cinfo, dst);
	switch( cinfo->e.Dtype.mode ){
		case VFSDT_CABFOLDER:
		case VFSDT_LZHFOLDER:
		case VFSDT_ZIPFOLDER:
			PPcFileOperation(cinfo, action, destpath, NULL);
			return NO_ERROR;

		case VFSDT_UN:
			if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
				if ( destpath == NULL ){
					if ( PPctInput(cinfo, UnpackTitle, dst, TSIZEOF(dst),
							PPXH_DIR_R, PPXH_DIR) <= 0 ){
						break;
					}
					destpath = dst;
				}
			}
			result = DoUndll(cinfo, destpath, Get_X_unbg);
			break;

		case VFSDT_SUSIE:
			if ( destpath == NULL ){
				if ( PPctInput(cinfo, UnpackTitle, dst, TSIZEOF(dst),
						PPXH_DIR_R, PPXH_DIR) <= 0 ){
					result = ERROR_CANCELLED;
					break;
				}
				destpath = dst;
			}
			result = DoSusie_part(cinfo, destpath);
			if ( result != NO_ERROR ) SetPopMsg(cinfo, result, NULL);
			break;

		case VFSDT_HTTP: {
			DWORD tmp;
			HANDLE hT;
			PPCGETHTTPSTRUCT *pghs;

			pghs = PPcHeapAlloc(sizeof(PPCGETHTTPSTRUCT));
			pghs->cinfo = cinfo;
			if ( (pghs->files = GetFiles(cinfo, GETFILES_SETPATHITEM)) == NULL ){
				break;
			}
			hT = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE)HttpThread, pghs, 0, &tmp);
			if ( hT != NULL ){
				SetThreadPriority(hT, THREAD_PRIORITY_LOWEST);
				CloseHandle(hT);
			}
			break;
		}

		case VFSDT_SHN:
			if ( PPctInput(cinfo,
					((action != FileOperationMode_Move) ?
						StrFopCaption_Copy : StrFopCaption_Move),
					dst, TSIZEOF(dst), PPXH_DIR_R, PPXH_DIR) <= 0 ){
				return ERROR_CANCELLED;
			}
			CopyToShnPathFiles(cinfo, dst, (action != FileOperationMode_Move) ?
					DROPEFFECT_COPY : DROPEFFECT_MOVE);
			break;

		default:
			SetPopMsg(cinfo, POPMSG_MSG, MES_ECDR);
			result = ERROR_BAD_COMMAND;
			break;
	}
	if ( (hFWnd == cinfo->info.hWnd) &&
			(cinfo->info.hWnd != GetForegroundWindow()) ){
		SetForegroundWindow(cinfo->info.hWnd);
	}
	return result;
}

//-----------------------------------------------------------------------------
ERRORCODE PPcUnpackAll_Zipfolder(UNPACKINFOSTRUCT *info, const TCHAR *filename, DWORD type)
{
	IMAGEGETEXINFO exinfo;
	BYTE *mem;
	ERRORCODE result;

	MakeDirectories(info->DestPath, NULL);
	exinfo.Progress = (LPPROGRESS_ROUTINE)NULL;
	exinfo.lpData = NULL;
	exinfo.Cancel = NULL;
	tstrcpy(exinfo.dest, info->DestPath);
	mem = (BYTE *)&exinfo;

	result = VFSGetArchivefileImage(INVALID_HANDLE_VALUE, NULL, filename, NilStr, NULL, &type, NULL, &mem);
	if ( result == MAX32 ) result = NO_ERROR;
	if ( result != NO_ERROR ) PPErrorBox(NULL, T("Unarc"), result);
	UnpackChop(info);
	return result;
}

// 全ファイル展開を行う
ERRORCODE PPcUnpackAll(UNPACKINFOSTRUCT *info, HANDLE hBatchfile, const TCHAR *exname)
{
	TCHAR filename[VFPS];
	void *dt_opt;
	BYTE header[0x8000];
	DWORD fsize, type;
	DWORD X_unbg = Get_X_unbg;
	ERRORCODE result;

	MakeArcDestDirPath(info);
	VFSFixPath(filename, (TCHAR *)GetCellFileName(info->parent, info->Cell, filename), info->parent->path, VFSFIX_REALPATH | VFSFIX_FULLPATH | VFSFIX_NOFIXEDGE);

	fsize = GetFileHeader(filename, header, sizeof(header));
	if ( fsize ){										// ファイルだった
		if ( exname != NULL ) tstrcat(filename, exname);
		type = VFSCheckDir(filename, header, fsize, &dt_opt);
		switch( type ){
			case VFSDT_UN: {
				TCHAR buf[CMDLINESIZE];

				if ( (result = UnArc_Extract(&info->info, dt_opt,
						UNARCEXTRACT_ALL, buf, XEO_NOEDIT)) == NO_ERROR ){
					TCHAR OldCurrentDir[VFPS];

					MakeDirectories(info->DestPath, NULL);
					GetCurrentDirectory(TSIZEOF(OldCurrentDir), OldCurrentDir);
					SetCurrentDirectory(info->parent->RealPath);
					UnArc_Exec(&info->info, dt_opt, buf, hBatchfile, NilStr, X_unbg,
							(info->chop == 2) ? info->DestPath : NULL);
					if ( X_unbg == 0 ) UnpackChop(info);
					SetCurrentDirectory(OldCurrentDir);
				}
				return result;
			}

			case VFSDT_SUSIE:
				return DoSusie_all(info, filename, dt_opt);

			case VFSDT_CABFOLDER:
			case VFSDT_LZHFOLDER:
			case VFSDT_ZIPFOLDER:
				return PPcUnpackAll_Zipfolder(info, filename, type);
		}
	}
		//---------------------------------------------------------------------
	return PP_ExtractMacro(info->info.hWnd, &info->parent->info,
			NULL, T("%ME_unpack2"), NULL, XEO_NOEDIT | XEO_NOEXECMARK);
}

BOOL CheckNameArchive(PPC_APPINFO *cinfo, ENTRYCELL *cell)
{
	TCHAR arcname[VFPS];
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	BOOL result = FALSE;

	VFSFullPath(arcname, (TCHAR *)GetCellFileName(cinfo, cell, arcname), cinfo->path);
	CatPath(NULL, arcname, T("*"));

	hFF = VFSFindFirst(arcname, &ff);	// .
	if ( hFF == INVALID_HANDLE_VALUE ) return FALSE;
	VFSFindNext(hFF, &ff);				// ..
	while ( IsTrue(VFSFindNext(hFF, &ff)) ){
		if ( CheckWarningName(ff.cFileName) ){
			result = TRUE;
			break;
		}
	}
	VFSFindClose(hFF);

	if ( IsTrue(result) ){
		if ( PMessageBox(cinfo->info.hWnd,
				MES_QEWE, ff.cFileName, MB_QYES | MB_DEFBUTTON2) != IDOK){
			return TRUE;
		}
	}
	return FALSE;
}

BOOL IsShn_UnpackCheck(PPC_APPINFO *cinfo)
{
	int work;
	ENTRYCELL *cell;
	TCHAR namebuf[VFPS];

	if ( cinfo->e.Dtype.mode != VFSDT_SHN ) return TRUE;
	if ( cinfo->RealPath[0] != '?' ) return FALSE;

	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		namebuf[0] = '\0';
		if ( (cell->f.ftLastWriteTime.dwHighDateTime |
			  cell->f.ftLastWriteTime.dwLowDateTime) == 0 ){
			return TRUE;
		}
		if ( VFSFullPath(namebuf, CellFileName(cell), cinfo->path) != NULL ){
			if ( VFSGetRealPath(cinfo->info.hWnd,  namebuf,  namebuf) == FALSE ){
				return FALSE;
			}
		}
		if ( namebuf[0] == '\0' ) return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// ファイルの展開
ERRORCODE PPC_Unpack(PPC_APPINFO *cinfo, const TCHAR *destpath)
{
	HANDLE hBatchfile = INVALID_HANDLE_VALUE;
	TCHAR tempfilename[MAX_PATH];
	DWORD X_unbg;

	UNPACKINFOSTRUCT info;
	int work;
	BOOL error = FALSE;

	if ( cinfo->UnpackFix ) OffArcPathMode(cinfo);
	if ( (cinfo->e.Dtype.mode != VFSDT_PATH) &&
		 (cinfo->e.Dtype.mode != VFSDT_LFILE) &&
		 IsTrue(IsShn_UnpackCheck(cinfo)) ){
		return PPcUnpackSelectedEntry(cinfo, destpath, FileOperationMode_Copy); // 仮想ディレクトリ内
	}

	if ( destpath != NULL ){
		tstrcpy(info.DestPath, destpath);
	}else{
		GetPairPath(cinfo, info.DestPath);
		CatPath(NULL, info.DestPath, NilStr);
		if ( PPctInput(cinfo, UnpackTitle, info.DestPath,
					TSIZEOF(info.DestPath), PPXH_DIR_R, PPXH_DIR) <= 0 ){
			return ERROR_CANCELLED;
		}
		VFSFixPath(NULL, info.DestPath, cinfo->path, VFSFIX_PATH);
	}

	X_unbg = Get_X_unbg;
	if ( (cinfo->e.markC > 1) && X_unbg ){
		MakeTempEntry(MAX_PATH, tempfilename, FILE_ATTRIBUTE_NORMAL);
		hBatchfile = CreateFileL(tempfilename,  GENERIC_WRITE,
					0,  NULL,  CREATE_ALWAYS,
					FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN,  NULL);
	}
	info.chop = GetCustXDword(T("X_arcdr"), NULL, 0);
	info.DestPathLast = info.DestPath + tstrlen(info.DestPath);
	info.info = cinfo->info;
	info.info.Function = (PPXAPPINFOFUNCTION)UnpackInfoFunc_All;
	info.parent = cinfo;
	InitEnumMarkCell(cinfo, &work);
	PPxCommonCommand(NULL, JOBSTATE_ARC_EXTRACT, K_ADDJOBTASK);
	while ( (info.Cell = EnumMarkCell(cinfo, &work)) != NULL ){
		if ( X_wnam && IsTrue(CheckNameArchive(cinfo, info.Cell)) ){
			error = TRUE;
			break;
		}
		PPcUnpackAll(&info, hBatchfile, NULL);
		if ( GetAsyncKeyState(VK_ESCAPE) & KEYSTATE_PUSH ){
			error = TRUE;
			break;
		}
	}
	if ( hBatchfile != INVALID_HANDLE_VALUE ){
		CloseHandle(hBatchfile);
		if ( IsTrue(error) ){
			DeleteFileL(tempfilename);
		}else{
			RunBatchProcess(cinfo, tempfilename,
				(GetFileAttributes(cinfo->path) != BADATTR ) ?
				cinfo->path : PPcPath, X_unbg);
		}
	}
	PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);
	return NO_ERROR;
}

ERRORCODE UnpackMenu(PPC_APPINFO *cinfo)
{
	TCHAR pathbuf[VFPS], unname[VFPS], cmd[CMDLINESIZE];
	BYTE header[VFS_check_size];
	DWORD fsize;					// 読み込んだ大きさ
	HMENU hPopupMenu;
	int index;
	ERRORCODE result;
	UNPACKINFOSTRUCT info;
	int work;

	if ( cinfo->UnpackFix ) OffArcPathMode(cinfo);
	InitEnumMarkCell(cinfo, &work);
	if ( (info.Cell = EnumMarkCell(cinfo, &work)) == NULL ){
		return ERROR_NO_MORE_FILES;
	}

	if ( VFSFullPath(pathbuf, CellFileName(info.Cell), cinfo->RealPath) == NULL ){
		return ERROR_BAD_PATHNAME;
	}
	fsize = GetFileHeader(pathbuf, header, sizeof(header));
	if ( fsize < 4 ) return ERROR_INVALID_DATA;

	hPopupMenu = CreatePopupMenu();
	if ( 0 == VFSCheckDir(pathbuf, header, fsize | VFSCHKDIR_GETEXTRACTMENU, (void **)hPopupMenu) ){
		DestroyMenu(hPopupMenu);
		SetPopMsg(cinfo, POPMSG_MSG, T("no archive"));
		return ERROR_INVALID_DATA;
	}

	index = PPcTrackPopupMenu(cinfo, hPopupMenu);
	if ( index <= 0 ){
		DestroyMenu(hPopupMenu);
		return ERROR_CANCELLED;
	}

	GetMenuString(hPopupMenu, index, unname, TSIZEOF(unname), MF_BYCOMMAND);
	DestroyMenu(hPopupMenu);

	if ( unname[0] == '*' ){ // E_unpack2 を使用
		return PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info,
				NULL, T("%ME_unpack2"), NULL, XEO_NOEDIT | XEO_NOEXECMARK);
	}

	GetPairPath(cinfo, info.DestPath);
	CatPath(NULL, info.DestPath, NilStr);
	if ( PPctInput(cinfo, UnpackTitle, info.DestPath,
			TSIZEOF(info.DestPath), PPXH_DIR_R, PPXH_DIR) <= 0 ){
		return ERROR_CANCELLED;
	}
	VFSFixPath(NULL, info.DestPath, cinfo->path, VFSFIX_PATH);
	MakeDirectories(info.DestPath, NULL);

	info.chop = GetCustXDword(T("X_arcdr"), NULL, 0);
	info.DestPathLast = info.DestPath + tstrlen(info.DestPath);
	info.info = cinfo->info;
	info.info.Function = (PPXAPPINFOFUNCTION)UnpackInfoFunc_All;
	info.parent = cinfo;

	PPxCommonCommand(NULL, JOBSTATE_ARC_EXTRACT, K_ADDJOBTASK);

	for (;;){
		if ( X_wnam && IsTrue(CheckNameArchive(cinfo, info.Cell)) ){
			result = ERROR_INVALID_DATA;
			break;
		}

		if ( unname[0] == ':' ){ // Susie を使用
			result = PPcUnpackAll(&info, INVALID_HANDLE_VALUE, unname);
		}else{	// undll を使用
			MakeArcDestDirPath(&info);

			// PPcUnpackAll に統合する予定
			if ( tstrcmp(unname, T("zipfldr.dll")) == 0 ){
				result = PPcUnpackAll_Zipfolder(&info, pathbuf, VFSDT_ZIPFOLDER);
			}else if ( tstrcmp(unname, T("lzhfldr2.dll")) == 0 ){
				result = PPcUnpackAll_Zipfolder(&info,  pathbuf, VFSDT_LZHFOLDER);
			}else if ( tstrcmp(unname, T("cabview.dll")) == 0 ){
				result = PPcUnpackAll_Zipfolder(&info, pathbuf, VFSDT_CABFOLDER);
			}else{
				wsprintf(cmd, T("%%u/%s,!all,"), unname);
				result = PP_ExtractMacro(cinfo->info.hWnd, &info.info, NULL, cmd, NULL, 0);
				UnpackChop(&info);
			}
		}
		if ( result != NO_ERROR ) break;
		if ( GetAsyncKeyState(VK_ESCAPE) & KEYSTATE_PUSH ){
			result = ERROR_CANCELLED;
			break;
		}
		if ( (info.Cell = EnumMarkCell(cinfo, &work)) == NULL ) break;
		if ( VFSFullPath(pathbuf, CellFileName(info.Cell), cinfo->RealPath) == NULL ){
			result = ERROR_BAD_PATHNAME;
			break;
		}
	}

	PPxCommonCommand(NULL, 0, K_DELETEJOBTASK);

	return result;
}

// Http 処理
DWORD WINAPI HttpThread(PPCGETHTTPSTRUCT *pghs)
{
	TCHAR *path, *files;
	TCHAR dst[VFPS], buf[VFPS], name[VFPS];
	THREADSTRUCT threadstruct = {HttpThreadName, XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};
	PPC_APPINFO *cinfo;

	cinfo = pghs->cinfo;
	path = pghs->files;
	PPcHeapFree(pghs);
	cinfo->BreakFlag = FALSE;
	PPxRegisterThread(&threadstruct);
	GetPairPath(cinfo, dst);
	CatPath(NULL, dst, NilStr);

	if ( PPctInput(cinfo, MES_TCHP, dst, TSIZEOF(dst), PPXH_DIR_R, PPXH_DIR) > 0 ){
		MakeDirectories(dst, NULL);
		files = path + tstrlen(path) + 1;

		for ( ; *files ; files += tstrlen(files) + 1){
			ThSTRUCT th;
			char *bottom, *p;
			TCHAR *namep, *namedst;
			DWORD size;
			HANDLE hFile;
										// メモリ上に取得 --------------------
			VFSFullPath(name, files, path);

			SetPopMsg(cinfo, POPMSG_PROGRESSMSG, name);
			if ( GetImageByHttp(name, &th) == FALSE ) continue;
			bottom = th.bottom;
			size = th.top - 1;
			p = strstr(bottom, "\r\n\r\n");
			if ( p != NULL ){
				if ( *(p + 4) == '\0' ){ // データ無し
					*p = '\0';
					// ステータスを取得する
					if ( (strstr(th.bottom, " 302") != NULL) && ((p = strstr(th.bottom, "Location: ")) != NULL) ){
						p += 10;
						while ( *p == ' ' ) p++;
						namep = buf;
						for (;;){
							if ( namep >= (buf + VFPS - 1) ) break;
							if ( *p <= ' ' ) break;
							*namep++ = *p++;
						}
						*namep = '\0';
						if ( *p <= ' ' ){
							VFSFullPath(name, buf, path);
							SetPopMsg(cinfo, POPMSG_PROGRESSMSG, name);
							ThFree(&th);
							if ( GetImageByHttp(name, &th) == FALSE ) continue;

							bottom = th.bottom;
							size = th.top - 1;
							p = strstr(bottom, "\r\n\r\n");
							if ( (p != NULL)  && (*(p + 4) != '\0') ){
								*p = '\0';
								size -= p - bottom + 4;
								bottom = p + 4;
							}
						}
					}
				}else{ // データあり
					size -= p - bottom + 4;
					bottom = p + 4;
				}
			}
										// 書き込み名生成 ---------------------
			tstrcpy(buf, files);
			namep = tstrrchr(buf, '/');
			if ( (namep != NULL) && (*(namep + 1) == '\0') ){
				*namep = '\0';
				namep = tstrrchr(buf, '/');
			}
			if ( namep != NULL ){
				namep++;
			}else{
				namep = buf;
			}
			namedst = buf;
			for ( ; *namep ; namep++ ){
				if ( (*namep == '?') ) *namep = '_';
				*namedst++ = *namep;
			}
			*namedst = '\0';
			if ( (tstrchr(buf, '.') == NULL) && (bottom[0] == '<') ){
				tstrcat(buf, T(".html"));
			}
			VFSFullPath(name, buf, dst);
			if ( IsTrue(cinfo->BreakFlag) ) break;

			if ( GetFileAttributesL(name) != BADATTR ){
				if ( PMessageBox(cinfo->info.hWnd,
						MES_QSAM, files, MB_QYES) != IDOK) continue;
			}

										// 書き込み ---------------------------
			hFile = CreateFileL(name, GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						 NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ){
				PPErrorBox(cinfo->info.hWnd, MES_TCHP, PPERROR_GETLASTERROR);
				ThFree(&th);
				break;
			}else{
				if ( !WriteFile(hFile, bottom, size, &size, NULL) ){
					PPErrorBox(cinfo->info.hWnd, MES_TCHP, PPERROR_GETLASTERROR);
					CloseHandle(hFile);
					ThFree(&th);
					break;
				}
				CloseHandle(hFile);
				ThFree(&th);
			}
			if ( IsTrue(cinfo->BreakFlag) ) break;
		}
	}
	HeapFree( hProcessHeap, 0, path);
	SetPopMsg(cinfo, POPMSG_MSG, cinfo->BreakFlag ? MES_BRAK : MES_HPDN);
	PPxUnRegisterThread();
	return TRUE;
}

BOOL OnArcPathMode(PPC_APPINFO *cinfo)
{
	TCHAR newpath[VFPS];
	BOOL arcmode;

	if ( IsTrue(cinfo->UnpackFix) ) return TRUE; // 既に有効
	if ( (cinfo->e.markC == 0) &&
		 (CEL(cinfo->e.cellN).attr & (ECA_PARENT | ECA_THIS)) ){
		return FALSE;
	}

	tstrcpy(newpath, cinfo->RealPath);
	arcmode = PPcUnpackForAction(cinfo,  newpath,  UFA_ALL);
	if ( IsTrue(arcmode) ){
		if ( cinfo->UnpackFixOldRealPath == NULL ){
			cinfo->UnpackFixOldPath = PPcHeapAlloc(VFPS);
			cinfo->UnpackFixOldRealPath = PPcHeapAlloc(VFPS);
		}
		if ( cinfo->UnpackFixOldRealPath != NULL ){
			tstrcpy(cinfo->UnpackFixOldPath,  cinfo->path);
			tstrcpy(cinfo->UnpackFixOldRealPath,  cinfo->RealPath);
		}
		tstrcpy(cinfo->path,  newpath);
		tstrcpy(cinfo->RealPath,  newpath);
		cinfo->UnpackFix = TRUE;
	}
	return arcmode;
}

void OffArcPathMode(PPC_APPINFO *cinfo)
{
	if ( cinfo->UnpackFixOldPath != NULL ){
		tstrcpy(cinfo->path,  cinfo->UnpackFixOldPath);
		tstrcpy(cinfo->RealPath, cinfo->UnpackFixOldRealPath);
		PPcHeapFree(cinfo->UnpackFixOldRealPath);
		PPcHeapFree(cinfo->UnpackFixOldPath);
		cinfo->UnpackFixOldPath = NULL;
		cinfo->UnpackFixOldRealPath = NULL;
	}
	cinfo->UnpackFix = FALSE;
}
