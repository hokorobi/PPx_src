/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						PPx Module
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "CALC.H"
#pragma hdrstop

ThSTRUCT Thmodule,Thmodule_str;
int ppxmodule_count = -1;

typedef struct {
	PPXAPPINFOW info;
	PPXAPPINFO  *parent;
	EXECSTRUCT *Z;
} COMMANDMODULEINFOSTRUCT;

typedef int (PPXAPI *tagCommandModuleEntry)(PPXAPPINFOW *ppxa,DWORD cmdID,PPXMODULEPARAM pxs);
typedef int (USECDECL *tagOldCommandModuleEntry)(PPXAPPINFOW *ppxa,DWORD cmdID,PPXMODULEPARAM pxs);

#define MODULE_NOLOAD 0
typedef struct {
	HMODULE hDLL;	// Dll handle
	DWORD types;	// 内容(読み込み失敗しているときはMODULE_NOLOAD)
	int DllNameOffset; // DLL名
	tagCommandModuleEntry ModuleEntry;
#ifndef _WIN64
	tagOldCommandModuleEntry OldModuleEntry;
#endif
} MODULESTRUCT;

MODULESTRUCT *ppxmodule_list = NULL;
const TCHAR P_moduleStr[] = T("P_module");

DWORD_PTR USECDECL CommandModuleInfoFunc(COMMANDMODULEINFOSTRUCT *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	TCHAR buf[CMDLINESIZE];
#ifndef UNICODE
	char bufA[CMDLINESIZE];
#endif

	if ( cmdID < PPXCMDID_FILL ){
		if ( cmdID < 0x100 ){
#ifndef UNICODE
			PPXCMDENUMSTRUCT work;

			work.enumID = uptr->enums.enumID;
			work.buffer = bufA;
			PPxInfoFunc(ppxa->parent,cmdID,&work);
			AnsiToUnicode(bufA,(WCHAR *)uptr->enums.buffer,CMDLINESIZE);
#else
			PPxInfoFunc(ppxa->parent,cmdID,uptr);
#endif
		}
		return 1;
	}

	switch (cmdID){
		case PPXCMDID_EXECUTE: {
			ERRORCODE result;

			#ifdef UNICODE
			result = PP_ExtractMacro(ppxa->info.hWnd,ppxa->parent,NULL,uptr->str,NULL,0);
			#else
			char *extbuf = bufA;

			if ( UnicodeToAnsi((WCHAR *)uptr,bufA,sizeof(bufA)) == 0 ){
				if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ){
					int len;

					len = WideCharToMultiByte(CP_ACP,0,(WCHAR *)uptr,-1,NULL,0,NULL,NULL);
					extbuf = HeapAlloc(DLLheap,0,len);
					UnicodeToAnsi((WCHAR *)uptr,extbuf,len);
				}else{
					return 0;
				}
			}
			result = PP_ExtractMacro(ppxa->info.hWnd,ppxa->parent,NULL,extbuf,NULL,0);
			if ( extbuf != bufA ) HeapFree(DLLheap,0,extbuf);
			#endif
			if ( result == NO_ERROR ) result = 1;
			return result;
		}

		case PPXCMDID_EXTRACT:
			#ifdef UNICODE
			if ( NO_ERROR == PP_ExtractMacro(ppxa->info.hWnd,ppxa->parent,NULL,uptr->str,buf,XEO_EXTRACTEXEC) ){
				strcpyW(uptr->str,buf);
			}else{
				*uptr->str = '\0';
			}
			#else
			UnicodeToAnsi((WCHAR *)uptr,bufA,sizeof(bufA));
			if ( NO_ERROR == PP_ExtractMacro(ppxa->info.hWnd,ppxa->parent,NULL,bufA,buf,XEO_EXTRACTEXEC) ){
				AnsiToUnicode(buf,(WCHAR *)uptr,CMDLINESIZE);
			}else{
				*uptr->str = '\0';
			}
			#endif
			return 1;

		case PPXCMDID_GETFILEINFO: {
			VFSFILETYPE vft;

			vft.flags = VFSFT_TYPE;
			vft.type[0] = '\0';

			#ifdef UNICODE
			VFSGetFileType((WCHAR *)&uptr->nums[1],NULL,0,&vft);
			strcpyW((TCHAR *)&uptr->nums[1],vft.type);
			#else
			UnicodeToAnsi((WCHAR *)&uptr->nums[1],bufA,sizeof(bufA));
			VFSGetFileType(bufA,NULL,0,&vft);
			AnsiToUnicode(vft.type,(WCHAR *)&uptr->nums[1],CMDLINESIZE);
			#endif
			return 1;
		}

#ifndef UNICODE
		case PPXCMDID_COMBOTABIDNAME:
		case PPXCMDID_COMBOTABNAME:
			memcpy(bufA,uptr,sizeof(DWORD) * 2);
			PPxInfoFunc(ppxa->parent,cmdID,bufA);
			AnsiToUnicode(bufA,(WCHAR *)uptr,CMDLINESIZE);
			return 1;

		case PPXCMDID_COMBOGETPANE:
			UnicodeToAnsi((WCHAR *)uptr,bufA,sizeof(bufA));
			return PPxInfoFunc(ppxa->parent,cmdID,bufA);

		case PPXCMDID_COMBOGETTAB:
		case PPXCMDID_COMBOTABEXECUTE:
		case PPXCMDID_COMBOTABEXTRACT: {
			WCHAR *strW = *(WCHAR **)&uptr->nums[2];

			UnicodeToAnsi(strW,bufA,sizeof(bufA));
			*(char **)&uptr->nums[2] = bufA;
			PPxInfoFunc(ppxa->parent,cmdID,uptr);
			if ( cmdID == PPXCMDID_COMBOTABEXTRACT ){
				*(WCHAR **)&uptr->nums[2] = strW;
				AnsiToUnicode(bufA,strW,CMDLINESIZE);
			}
			return 1;
		}

		case PPXCMDID_ENTRYNAME:
		case PPXCMDID_ENTRYANAME:
		case PPXCMDID_ENTRYCOMMENT:
			*((long *)bufA) = uptr->nums[0];
		case PPXCMDID_DRIVELABEL:
		case PPXCMDID_CSRCOMMENT:
		case PPXCMDID_COMBOIDNAME:
			PPxInfoFunc(ppxa->parent,cmdID,bufA);
			AnsiToUnicode(bufA,(WCHAR *)uptr,CMDLINESIZE);
			return 1;

		case PPXCMDID_REPORTSEARCH_FDATA:
			memcpy(bufA,(WIN32_FIND_DATAW *)uptr,4 + 8 * 3 + 8);
			UnicodeToAnsi( ((WIN32_FIND_DATAW *)uptr)->cFileName,
					((WIN32_FIND_DATAA *)bufA)->cFileName,MAX_PATH);
			UnicodeToAnsi( ((WIN32_FIND_DATAW *)uptr)->cAlternateFileName,
					((WIN32_FIND_DATAA *)bufA)->cAlternateFileName,13);
			return PPxInfoFunc(ppxa->parent,PPXCMDID_REPORTSEARCH_FDATA,bufA);

		case PPXCMDID_SETPOPLINE:
		case PPXCMDID_CSRSETCOMMENT:
		case PPXCMDID_REPORTPPCCOLUMN:
		case PPXCMDID_REPORTSEARCH:
		case PPXCMDID_REPORTSEARCH_FILE:
		case PPXCMDID_REPORTSEARCH_DIRECTORY:
			UnicodeToAnsi((WCHAR *)uptr,bufA,sizeof(bufA));
			return PPxInfoFunc(ppxa->parent,cmdID,bufA);

		case PPXCMDID_ENTRYSETCOMMENT:
			UnicodeToAnsi((WCHAR *)&uptr->nums[1],bufA + 4,sizeof(bufA) - 4);
			*((long *)bufA) = uptr->nums[0];
			return PPxInfoFunc(ppxa->parent,cmdID,bufA);

		case PPXCMDID_ENTRYEXTDATA_SETDATA: {
			WCHAR *wptr;
			DWORD_PTR result;
			DWORD id = ((ENTRYEXTDATASTRUCT *)uptr)->id;

			if ( (id < DFC_COMMENTEX) || (id > DFC_COMMENTEX_MAX) ){
				return PPxInfoFunc(ppxa->parent,cmdID,uptr);
			}

			wptr = (WCHAR *)((ENTRYEXTDATASTRUCT *)uptr)->data;
			UnicodeToAnsi(wptr,bufA,sizeof(bufA));
			((ENTRYEXTDATASTRUCT *)uptr)->data = (BYTE *)bufA;
			result = PPxInfoFunc(ppxa->parent,cmdID,uptr);
			((ENTRYEXTDATASTRUCT *)uptr)->data = (BYTE *)wptr;
			return result;
		}

		case PPXCMDID_ENTRYEXTDATA_GETDATA: {
			WCHAR *wptr;
			DWORD_PTR result;
			DWORD id = ((ENTRYEXTDATASTRUCT *)uptr)->id;

			if ( (id < DFC_COMMENTEX) || (id > DFC_COMMENTEX_MAX) ){
				return PPxInfoFunc(ppxa->parent,cmdID,uptr);
			}

			wptr = (WCHAR *)((ENTRYEXTDATASTRUCT *)uptr)->data;
			((ENTRYEXTDATASTRUCT *)uptr)->data = (BYTE *)bufA;
			result = PPxInfoFunc(ppxa->parent,cmdID,uptr);
			((ENTRYEXTDATASTRUCT *)uptr)->data = (BYTE *)wptr;
			if ( result == 0 ) return result;

			AnsiToUnicode(bufA,wptr,((ENTRYEXTDATASTRUCT *)uptr)->size / sizeof(WCHAR));
			return result;
		}

		case PPXCMDID_ENTRYINSERT:
			UnicodeToAnsi((WCHAR *)uptr->dptrs[1],bufA + sizeof(void *) * 2,sizeof(bufA) - sizeof(void *) * 2);
			*((LONG_PTR *)bufA) = uptr->dptrs[0];
			*((LONG_PTR *)bufA + 1) = (LONG_PTR)(bufA + sizeof(void *) * 2);
			return PPxInfoFunc(ppxa->parent,cmdID,bufA);

		case PPXCMDID_ENTRYINFO: {
			ThSTRUCT th;
			size_t size;

			*(DWORD *)bufA = uptr->nums[0];
			PPxInfoFunc(ppxa->parent,cmdID,bufA);

			size = 0x2000 * sizeof(WCHAR);
			ThInit(&th);
			ThSize(&th,size);
			AnsiToUnicode(*(char **)bufA,(WCHAR *)th.bottom,size);
			*(char **)uptr = th.bottom;
			HeapFree(ProcHeap,0,(char *)bufA);
			return 1;
		}
#endif
		case PPXCMDID_CHARTYPE:
#ifdef UNICODE
			return 1;
#else
			return 0;
#endif

		case PPXCMDID_VERSION:
			return (VersionH * 10000 + VersionM * 100 + VersionL);

		case PPXCMDID_GETKEYNAME:
#ifdef UNICODE
			PutKeyCode(uptr->str,uptr->key);
#else
			PutKeyCode(bufA,uptr->key);
			AnsiToUnicode(bufA,(WCHAR *)uptr,64);
#endif
			return 1;

		case PPXCMDID_GETWNDVARIABLEDATA: {
			ThSTRUCT *TH = (ThSTRUCT *)PPxInfoFunc(ppxa->parent,PPXCMDID_GETWNDVARIABLESTRUCT,NULL);
			if ( TH == NULL ) TH = &ProcessStringValue;

#ifdef UNICODE
			*((WCHAR *)uptr->dptrs[1]) = '\0';
			ThGetString(TH,(WCHAR *)uptr->dptrs[0],(WCHAR *)uptr->dptrs[1],CMDLINESIZE - 1);
#else
			bufA[0] = '\0';
			UnicodeToAnsi((WCHAR *)uptr->dptrs[0],bufA,sizeof(bufA));
			ThGetString(TH,bufA,bufA,CMDLINESIZE - 1);
			AnsiToUnicode(bufA,(WCHAR *)uptr->dptrs[1],CMDLINESIZE);
			return 1;
#endif
		}

		case PPXCMDID_SETWNDVARIABLEDATA: {
			ThSTRUCT *TH = (ThSTRUCT *)PPxInfoFunc(ppxa->parent,PPXCMDID_GETWNDVARIABLESTRUCT,NULL);
			if ( TH == NULL ) TH = &ProcessStringValue;

#ifdef UNICODE
			ThSetString(TH,(WCHAR *)uptr->dptrs[0],(WCHAR *)uptr->dptrs[1]);
#else
			UnicodeToAnsi((WCHAR *)uptr->dptrs[0],bufA,sizeof(bufA));
			UnicodeToAnsi((WCHAR *)uptr->dptrs[1],bufA + 32,sizeof(bufA) - 32);
			ThSetString(TH,bufA,bufA + 32);
			return 1;
#endif
		}
/*
		case PPXCMDID_SETPOPUPPOS:
			if ( ppxa->Z != NULL ){
			}
			// default へ
*/
		default:
			return PPxInfoFunc(ppxa->parent,cmdID,uptr);
	}
}

void FreePPxModule(void)
{
	MODULESTRUCT *mdll;
	int i;
	PPXAPPINFOW info = {NULL,L"",L"",NULL};
	PPXMODULEPARAM module;

	mdll = ppxmodule_list;
	ppxmodule_list = NULL;
	if ( mdll == NULL ) return;

	module.info = NULL;
	for ( i = ppxmodule_count ; i ; i--,mdll++ ){
		if ( mdll->types == MODULE_NOLOAD ) continue;
		if ( mdll->hDLL != NULL ){
			if ( (mdll->ModuleEntry != NULL) &&
				 (mdll->types & PPMTYPEFLAGS(PPXMEVENT_CLEANUP)) ){
				mdll->ModuleEntry(&info,PPXMEVENT_CLEANUP,module);
			}
			FreeLibrary(mdll->hDLL);
		}
	}
	ThFree(&Thmodule);
	ThFree(&Thmodule_str);
	ppxmodule_count = -1;
	return;
}

void LoadModuleList(void)
{
	MODULESTRUCT mdll;
	HANDLE hFF; 		// FindFile 用ハンドル
	WIN32_FIND_DATA ff;	// ファイル情報
	TCHAR dir[MAX_PATH];
	int modulecount = 0;

	EnterCriticalSection(&ThreadSection);

	ThInit(&Thmodule);
	ThInit(&Thmodule_str);
	CatPath(dir,DLLpath,T("PPX*.DLL"));
										// 検索 -------------------------------
	hFF = FindFirstFileL(dir,&ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		do{
			if ( !tstricmp(ff.cFileName,T("PPXLIB32.DLL")) ) continue;
			mdll.hDLL = NULL;
			mdll.types = MAX32; // 仮に全部を読み込み可能に
			GetCustTable(P_moduleStr,ff.cFileName,&mdll.types,sizeof(mdll.types));
			mdll.DllNameOffset = Thmodule_str.top;
			ThAddString(&Thmodule_str,ff.cFileName);
			ThAppend(&Thmodule,&mdll,sizeof mdll);
			modulecount++;
		}while( IsTrue(FindNextFile(hFF,&ff)) );
		FindClose(hFF);
	}
	if ( Thmodule.bottom == NULL ) modulecount = 0; // モジュールがない
	ppxmodule_list = (MODULESTRUCT *)Thmodule.bottom;
	ppxmodule_count = modulecount;

	LeaveCriticalSection(&ThreadSection);
}

BOOL LoadModuleFile(HWND hWnd,MODULESTRUCT *mdll,DWORD types)
{
	ERRORCODE result;
	if ( !(mdll->types & types) ) return FALSE;
	EnterCriticalSection(&ThreadSection);
	if ( mdll->hDLL != NULL ){
		LeaveCriticalSection(&ThreadSection);
		return TRUE;
	}

	mdll->hDLL = LoadLibrary(
			(TCHAR *)(Thmodule_str.bottom + mdll->DllNameOffset));
	if ( mdll->hDLL == NULL ) goto loaderror;
	mdll->ModuleEntry = (tagCommandModuleEntry)
			GetProcAddress(mdll->hDLL,"ModuleEntry");
	if ( mdll->ModuleEntry != NULL ){
		PPXMODULEPARAM module;
		PPXMINFOSTRUCT pis;
		WCHAR copyright[MAX_PATH];

		pis.infotype = 0;
		pis.copyright = copyright;
		pis.typeflags = MAX32;
		module.info = &pis;
		if ( mdll->ModuleEntry(NULL,PPXM_INFORMATION,module) != PPXMRESULT_SKIP ){
			SetCustTable(P_moduleStr,(TCHAR *)(Thmodule_str.bottom + mdll->DllNameOffset),&pis.typeflags,sizeof(pis.typeflags));
		}
		LeaveCriticalSection(&ThreadSection);
		return TRUE;
	}
#ifndef _WIN64
	mdll->OldModuleEntry = (tagOldCommandModuleEntry)
			GetProcAddress(mdll->hDLL,"_ModuleEntry");
	if ( mdll->OldModuleEntry != NULL ){
		LeaveCriticalSection(&ThreadSection);
		return TRUE;
	}
#endif
	FreeLibrary(mdll->hDLL);
	mdll->hDLL = NULL;
loaderror:
	result = GetLastError();
	mdll->types = MODULE_NOLOAD;
	LeaveCriticalSection(&ThreadSection);

	if ( (result != ERROR_BAD_EXE_FORMAT) && (result != ERROR_GEN_FAILURE) ){
		XMessage(hWnd,NULL,XM_NsERRd,T("%s load error"),
				(TCHAR *)(Thmodule_str.bottom + mdll->DllNameOffset));
	}
	return FALSE;
}

int CommandModule(EXECSTRUCT *Z,const TCHAR **ptr)
{
	const WCHAR *param;
	DWORD paramcount = 0;
	int result;
	MODULESTRUCT *mdll;
	int i;
	WCHAR cmdbuf[CMDLINESIZE],*next;
	COMMANDMODULEINFOSTRUCT ppxa;
	PPXMCOMMANDSTRUCT command;
	PPXMODULEPARAM module;

#ifndef UNICODE
	WCHAR regidW[REGIDSIZE],nameW[MAX_PATH];

	AnsiToUnicode(Z->Info->Name,nameW,MAX_PATH);
	AnsiToUnicode(Z->Info->RegID,regidW,REGIDSIZE);
	#define PPXAINFONAME nameW
	#define PPXAINFOREGID regidW
#else
	#define PPXAINFONAME Z->Info->Name
	#define PPXAINFOREGID Z->Info->RegID
#endif
	if ( ppxmodule_count < 0 ) LoadModuleList();
	if ( ppxmodule_count <= 0 ) return PPXMRESULT_SKIP; // モジュールがない
									// コマンド名保存
	strcpyToW(cmdbuf,*ptr,VFPS);
	param = next = cmdbuf + strlenW(cmdbuf) + 1;
	(*ptr) += tstrlen(*ptr) + 1;
									// パラメータの切り出し
#ifndef UNICODE
	while( **ptr ){
		char tmp[CMDLINESIZE];

		tmp[0] = '\0';
		GetCommandParameter(ptr,tmp,TSIZEOF(tmp));
		if ( tmp[0] == '\0' ) break;
		paramcount++;
		AnsiToUnicode(tmp,next,CMDLINESIZE);
		next = next + strlenW(next) + 1;
		if ( NextParameter(ptr) == FALSE ) break;
	}
#else
	while( **ptr ){
		*next = '\0';
		GetCommandParameter(ptr,next,CMDLINESIZE - VFPS);
		if ( *next == '\0' ) break;
		paramcount++;
		next = next + strlenW(next) + 1;
		if ( NextParameter(ptr) == FALSE ) break;
	}
#endif
										// 検索と実行
	mdll = ppxmodule_list;
	module.command = &command;

	command.commandname = cmdbuf;
	command.commandhash = Z->command;
	command.param = param;
	command.paramcount = paramcount;
	command.resultstring = NULL;

	ppxa.info.Name = PPXAINFONAME;
	ppxa.info.RegID = PPXAINFOREGID;
	ppxa.info.Function = (PPXAPPINFOFUNCTIONW)CommandModuleInfoFunc;
	ppxa.info.hWnd = Z->hWnd;
	ppxa.parent = Z->Info;
	ppxa.Z = Z;

	for ( i = ppxmodule_count ; i ; i--,mdll++ ){
		if ( mdll->hDLL == NULL ){
			if ( LoadModuleFile(ppxa.info.hWnd,mdll,PPMTYPEFLAGS(PPXMEVENT_COMMAND)) == FALSE ){
				continue;
			}
		}
#ifdef _WIN64
		result = mdll->ModuleEntry(&ppxa.info,PPXMEVENT_COMMAND,module);
#else
		if ( mdll->ModuleEntry != NULL ){
			result = mdll->ModuleEntry(&ppxa.info,PPXMEVENT_COMMAND,module);
		}else{
			result = mdll->OldModuleEntry(&ppxa.info,PPXMEVENT_COMMAND,module);
		}
#endif
		if ( result == PPXMRESULT_SKIP ) continue;
		return result;
	}
	return PPXMRESULT_SKIP;
}
#undef PPXAINFONAME
#undef PPXAINFOREGID

/*
void ToUtf8Function(EXECSTRUCT *Z,const char *param)
{
	WCHAR bufW[CMDLINESIZE];

	AnsiToUnicode(param,bufW,CMDLINESIZE);
	WideCharToMultiByteU8(CP_UTF8,0,bufW,-1,Z->dst,VFPS,NULL,NULL);
	Z->dst += tstrlen(Z->dst);
}
*/

void TreeFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	HWND hTreeWnd;
	HWND hParentWnd;
	POINT pos;
	MSG msg;

	*Z->dst = '\0';
	Z->result = ERROR_BUSY;
	if ( Z->hWnd == NULL ){
		hParentWnd = NULL;
	}else{
		hParentWnd = GetParentBox(Z->hWnd);
		if ( hParentWnd == NULL ) hParentWnd = Z->hWnd;
	}
	if ( SkipSpace(&param) == '\0' ) param = T("1"); // 空欄なら M_pjump に

	InitVFSTree();
	GetPopupPoint(Z,&pos);
	hTreeWnd = CreateWindow(TreeClassStr,ZGetTitleName(Z),
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,pos.x,pos.y,
			300,400,NULL,NULL,NULL,0);
	SendMessage(hTreeWnd,VTM_SETFLAG,(WPARAM)hParentWnd,(LPARAM)VFSTREE_MENU);
	SendMessage(hTreeWnd,VTM_TREECOMMAND,0,(LPARAM)param);
	SendMessage(hTreeWnd,VTM_SETRESULT,(WPARAM)&Z->result,(LPARAM)Z->dst);
	ShowWindow(hTreeWnd,SW_SHOWNORMAL);

	for ( ; ; ){
		if ( (int)GetMessage(&msg,NULL,0,0) <= 0 ){
			DestroyWindow(hTreeWnd);
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if ( Z->result != ERROR_BUSY ) break;
	}

	Z->dst += tstrlen(Z->dst);
}

void SelectTextFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	TCHAR opt;

	opt = SkipSpace(&param);
	*Z->dst = '\0';
	if ( Z->flag & XEO_CONSOLE ){
		Z->result = PPxInfoFunc(Z->Info,PPXCMDID_PPBSELECTTEXT,Z->dst);
	}else{
		SendMessage(Z->hWnd,WM_PPXCOMMAND,KE_seltext,(LPARAM)Z->dst);
	}

	if ( opt == 'u' ){
		char bufA[CMDLINESIZE],*srcA;
		TCHAR *dest,*max;

#ifdef UNICODE
		WideCharToMultiByteU8(CP_UTF8,0,Z->dst,-1,bufA,CMDLINESIZE,NULL,NULL);
#else
		WCHAR bufW[CMDLINESIZE];

		AnsiToUnicode(Z->dst,bufW,CMDLINESIZE);
		WideCharToMultiByteU8(CP_UTF8,0,bufW,-1,bufA,CMDLINESIZE,NULL,NULL);
#endif
		srcA = bufA;
		dest = Z->dst;
		max = dest + CMDLINESIZE / 2;
		while ( dest < max ){
			BYTE c;

			c = (BYTE)*srcA++;
			if ( c == '\0' ) break;
			if ( IsalnumA(c) ){
				*dest++ = c;
			}else if ( c == ' ' ){
				*dest++ = '+';
			}else{
				dest += wsprintf(dest,T("%%%02X"),c);
			}
		}
		*dest = '\0';
	}
	Z->dst += tstrlen(Z->dst);
}

void CountJobFunction(EXECSTRUCT *Z)
{
	int i,count = 0;
	JobX *job;

	job = Sm->Job;
	for ( i = 0 ; i < X_MaxJob ; i++,job++ ){
		if ( job->ThreadID != 0 ) count++;
	}
	Z->dst += wsprintf(Z->dst,T("%d"),count);
	return;
}

// %*linkedpath
void GetLinkedPathFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE],name[VFPS];

	GetCommandParameter(&param,buf,TSIZEOF(buf));

	if ( FAILED(GetLink(NULL,buf,name)) ){
		if ( GetReparsePath(buf,name) == 0 ) return;
	}
	Z->dst += wsprintf(Z->dst,T("%s"),name);
	return;
}

// %*name
void GetNameFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE],name[VFPS+2],code;
	const TCHAR *p;
	DWORD flag;

	GetCommandParameter(&param,buf,TSIZEOF(buf));
	p = buf;
	flag = GetFmacroOption(&p);
	if ( *p != '\0' ) goto error;
	if ( SkipSpace(&param) != ',' ) goto error;
	param++;
	code = SkipSpace(&param);
	if ( (code == '\"') || ((code != '\0') && (code != ',')) ){
		GetCommandParameter(&param,buf,TSIZEOF(buf));
	}else{
		ZGetName(Z,buf,'C');
	}

	p = GetZCurDir(Z);
	if ( SkipSpace(&param) == ',' ){	// 基準ディレクトリ指定
		param++;
		GetCommandParameter(&param,name,TSIZEOF(name));
		VFSFullPath(NULL,name,p);
		p = name;
	}

	if ( flag & FMOPT_REALPATH ){
		VFSFixPath(NULL,buf,p,VFSFIX_FULLPATH | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
	}else{
		VFSFullPath(NULL,buf,p);
	}
	if ( flag & FMOPT_UNIQUE ) GetUniqueEntryName(buf);
	GetFmacroString(flag,buf,name);

	Z->dst += wsprintf(Z->dst,T("%s"),name);
	return;

error:
	XMessage(NULL,NULL,XM_GrERRld,T("%*name(options,filename)"));
	Z->result = ERROR_INVALID_PARAMETER;
	return;
}


void InputFunctionOption(EXECSTRUCT *Z, TINPUT *tinput, const TCHAR *param, TCHAR *optionbuf)
{
	TCHAR buf[CMDLINESIZE];
	TINPUT_EDIT_OPTIONS options;
	UTCHAR code;
	const TCHAR *optptr,*more;

	tstrlimcpy(optionbuf, param, CMDLINESIZE);
	*tinput->buff = '\0';

	optptr = optionbuf;
	while( '\0' != (code = GetOptionParameter(&optptr, buf, CONSTCAST(TCHAR **,&more))) ){
		if ( (code != '-') || !tstrcmp(buf + 1, T("TEXT")) ){
			tstrlimcpy(tinput->buff, buf, tinput->size);
			continue;
		}
/*
		if ( !tstrcmp(buf + 1, T("RET")) ){ // 変数保存
			setflag(tinput->flag, 0);
			continue;
		}
*/
		if ( !tstrcmp(buf + 1, T("K")) ){
			tinput->StringVariable = &Z->StringVariable;
			setflag(tinput->flag, TIEX_EXECPRECMD);
			if ( *more != '\0' ){
				ThSetString(tinput->StringVariable,T("Input_FirstCmd"),more);
				continue;
			}else{
				ThSetString(tinput->StringVariable,T("Input_FirstCmd"),optptr);
				break;
			}
		}
		if ( !tstrcmp(buf + 1, T("TITLE")) ){
			tinput->title = optionbuf;
			tstrcpy(optionbuf, more); // optionbuf の -title 記載部を上書き
			continue;
		}
		if ( !tstrcmp(buf + 1, T("MODE")) ){
			if ( IsTrue(GetEditMode(&more,&options)) ){
				tinput->hRtype = options.hist_readflags;
				tinput->hWtype = HistWriteTypeflag[options.hist_writetype];
				tinput->flag = (tinput->flag & ~(TIEX_REFTREE | TIEX_SINGLEREF)) |
						TinputTypeflags[options.hist_writetype] | B31;
				if ( options.flags != 0 ){
					if ( options.flags & TINPUT_EDIT_OPTIONS_use_optionbutton ){
						setflag(tinput->flag,TIEX_USEOPTBTN);
					}else{
						setflag(tinput->flag,TIEX_USEREFLINE);
					}
				}
			}
			continue;
		}
		if ( !tstrcmp(buf + 1, T("SELECT")) ){
			if ( *more == 'i' ){
				setflag(tinput->flag, TIEX_INSTRSEL);
			}else{
				setflag(tinput->flag, TIEX_USESELECT);
				if ( (*more == '\0') || (*more == 'a') ){
					tinput->firstC = 0;
					tinput->lastC = EC_LAST;
				}else if ( *more == 'l' ){
					tinput->firstC = tinput->lastC = EC_LAST;
				}else if ( *more == 't' ){
					tinput->firstC = tinput->lastC = EC_LAST;
				}else {
					tinput->firstC = tinput->lastC = GetNumber(&more);
					if ( SkipSpace(&more) == ',' ){
						more++;
						tinput->lastC = GetNumber(&more);
					}
				}
			}
		}

		#if 0
		if ( !tstrcmp(buf + 1, T("BANNER")) ){ // 複数行表示の時は使用できない
			SendDlgItemMessage(hDlg, IDE_INPUT_LINE, EM_SETCUEBANNER, 1, (LPARAM)more); // MultiByte 版の時は、UNICODE化が必要
			continue;
		}
		#endif
	}
}
// %*input
void InputFunction(EXECSTRUCT *Z, const TCHAR *param)
{
	TINPUT tinput;
	TCHAR optionbuf[CMDLINESIZE];

	tinput.title = ZGetTitleName(Z);
	tinput.buff = Z->dst;
	tinput.size = CMDLINESIZE - ToSIZE32_T(Z->dst - Z->DstBuf) - 1;
	tinput.flag = TIEX_USEINFO;

	InputFunctionOption(Z, &tinput, param, optionbuf);

	if ( ZTinput(Z,&tinput) != FALSE ) Z->dst += tstrlen(Z->dst);
}

void GetCustFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE],str[VFPS+2],*sub;

	GetCommandParameter(&param,str,TSIZEOF(str));
	sub = tstrchr(str,':');
	if ( sub != NULL ) *sub++ = '\0';

	PPcustCDumpText(str,sub,buf);
	Z->dst += wsprintf(Z->dst,T("%s"),buf);
	return;
}

void RegExpFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE];
	RXPREPLACESTRING *rexps;

	GetCommandParameter(&param,Z->dst,CMDLINESIZE);
	if ( SkipSpace(&param) != ',' ) goto error;
	param++;
	GetCommandParameter(&param,buf,TSIZEOF(buf));

	if ( FALSE == InitRegularExpressionReplace(&rexps,buf,FALSE) ) return;
	RegularExpressionReplace(rexps,Z->dst);
	FreeRegularExpressionReplace(rexps);

	Z->dst += tstrlen(Z->dst);
	return;

error:
	if ( *Z->dst != '?' ){
		XMessage(NULL,NULL,XM_GrERRld,T("%*regexp(src,regexp)"));
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}
	GetRegularExpressionName(Z->dst);
	Z->dst += tstrlen(Z->dst);
}

void CalculationFunction(EXECSTRUCT *Z,const TCHAR *param)
{
	int result;
	TCHAR buf[CMDLINESIZE];
	const TCHAR *p;

	GetCommandParameter(&param,buf,TSIZEOF(buf));
	p = buf;
	if ( CalcString(&p,&result) == CALC_NOERROR ){
		Z->dst += wsprintf(Z->dst,T("%d"),result);
	}else{
		XMessage(NULL,NULL,XM_GrERRld,T("Calculation error"));
		Z->result = ERROR_INVALID_PARAMETER;
	}
	return;
}

void NestedFunction(EXECSTRUCT *Z)
{
	TCHAR function[CMDLINESIZE],*dst;
	BOOL separate = FALSE;

	dst = function + wsprintf(function,T("%%*%s"),Z->dst);
	for ( ;; ){
		TCHAR code;

		code = *Z->src;
		if ( (code == '\0') || (code == '\r') || (code == '\n') ||
			 (dst >= (function + CMDLINESIZE)) ){
			Z->result = ERROR_INVALID_PARAMETER;
			return;
		}
		*dst++ = code;
		Z->src++;
		if ( code == '\"' ){
			if ( separate == FALSE ){
				separate = TRUE;
			}else if ( *Z->src != '\"' ){ // 末尾 "
				separate = FALSE;
			}else{ // エスケープ "
				*Z->dst++ = '\"';
				Z->src++;
			}
		}else if ( (code == ')') && (separate == FALSE) ){
			break;
		}
	}
	*dst = '\0';

	Z->result = PP_ExtractMacro(Z->hWnd,Z->Info,NULL,function,Z->dst,0);
	if ( Z->result == NO_ERROR ){
		while ( *Z->dst != '\0' ) Z->dst++;
	}
}

// ※ パラメータは、Z->DstBuf 上なので、パラメータを読む前に Z->dst を使うとパラメータが破損する
void FunctionModule(EXECSTRUCT *Z)
{
	TCHAR cmdname[64];
	PPXMDLFUNCSTRUCT mdlparam;
	const TCHAR *dptr;
	DWORD namehash;

// 関数名抽出
	namehash = GetModuleNameHash(Z->dst,cmdname);
	dptr = Z->dst + tstrlen(Z->dst) + 1;

// PPx common 内蔵関数
	if ( cmdname[0] <= 'I' ){ //------------------------------------------- A-I
		if ( !tstrcmp(cmdname,T("ADDCHAR")) ){
			TCHAR code = *dptr;

			if ( code != '\0' ){
				if ( (Z->DstBuf < Z->dst) && (*(Z->dst - 1) != code) ){
					*Z->dst++ = code;
				}
			}
			return;
		}

		if ( !tstrcmp(cmdname,T("CALC")) ){
			CalculationFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("EDITTEXT")) ){
			*Z->dst = '\0';
			if ( Z->flag & XEO_CONSOLE ){
				Z->result = PPxInfoFunc(Z->Info,PPXCMDID_PPBEDITTEXT,Z->dst);
			}else{
				SendMessage(Z->hWnd,WM_PPXCOMMAND,KE_edtext,(LPARAM)Z->dst);
			}
			Z->dst += tstrlen(Z->dst);
			return;
		}
		if ( !tstrcmp(cmdname,T("ERRORMSG")) ){
			PPErrorMsg(Z->dst,GetNumber(&dptr));
			Z->dst += tstrlen(Z->dst);
			return;
		}
		if ( !tstrcmp(cmdname,T("EXITCODE")) || !tstrcmp(cmdname,T("ERRORLEVEL")) ){ // *exitcode
			Z->dst += wsprintf(Z->dst,T("%d"),Z->ExitCode);
			return;
		}
		if ( !tstrcmp(cmdname,T("EXTRACT")) ){ // *extract
			HWND hWnd;

			hWnd = GetPPxhWndFromID(Z->Info,&dptr,NULL);
			if ( SkipSpace(&dptr) == ',' ) dptr++;
			GetCommandParameter(&dptr,Z->dst,CMDLINESIZE);
			if ( hWnd == NULL ){ // 指定無し
				PP_ExtractMacro(Z->hWnd, Z->Info, NULL, Z->dst, Z->dst, XEO_EXTRACTEXEC);
				Z->dst += tstrlen(Z->dst);
				return;
			}else if ( hWnd == BADHWND ){ // 該当無し…何もしない
				return;
			}
			ExtractPPxCall(hWnd,Z,Z->dst);
			return;
		}
		if ( !tstrcmp(cmdname,T("F")) ){
			GetNameFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("GETCUST")) ){
			GetCustFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("INPUT")) ){
			InputFunction(Z,dptr);
			return;
		}
	}else{ //-------------------------------------------------------------- J-Z
		if ( !tstrcmp(cmdname,T("JOB")) ){
			CountJobFunction(Z);
			return;
		}
		if ( !tstrcmp(cmdname,T("LINKEDPATH")) ){
			GetLinkedPathFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("NAME")) ){
			GetNameFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("REGEXP")) ){
			RegExpFunction(Z,dptr);
			return;
		}
		if ( !tstrcmp(cmdname,T("SELECTTEXT")) ){
			SelectTextFunction(Z,dptr);
			return;
		}
/*
		if ( !tstrcmp(cmdname,T("TOUTF8")) ){
			ToUtf8Function(Z,dptr);
			return;
		}
*/
		if ( !tstrcmp(cmdname,T("TREE")) ){
			TreeFunction(Z,dptr);
			return;
		}
	}

// 各 PPx 内蔵関数
	mdlparam.param = cmdname;
	mdlparam.dest = Z->dst;
	mdlparam.dest[0] = '\0';
	mdlparam.optparam = dptr;

	if ( 1 != (Z->result = (PPxInfoFunc(Z->Info,PPXCMDID_FUNCTION,&mdlparam) ^ 1)) ){
		Z->dst += tstrlen(mdlparam.dest);
		return;
	}
// Module
	{
		DWORD paramcount = 0;
		BOOL result;
		MODULESTRUCT *mdll;
		int i;
		WCHAR cmdbuf[CMDLINESIZE],*next;
		const TCHAR *pptr;
		COMMANDMODULEINFOSTRUCT ppxa;
		PPXMCOMMANDSTRUCT function;
		PPXMODULEPARAM module;
		TCHAR olddir[VFPS];
#ifndef UNICODE
		WCHAR regidW[REGIDSIZE],nameW[MAX_PATH],destW[CMDLINESIZE];

		AnsiToUnicode(Z->Info->Name,nameW,MAX_PATH);
		AnsiToUnicode(Z->Info->RegID,regidW,REGIDSIZE);
		#define PPXAINFONAME nameW
		#define PPXAINFOREGID regidW
		#define DESTBUF destW
#else
		#define PPXAINFONAME Z->Info->Name
		#define PPXAINFOREGID Z->Info->RegID
		#define DESTBUF Z->dst
#endif
		if ( ppxmodule_count < 0 ) LoadModuleList();
									// コマンド名保存
		strcpyToW(cmdbuf,cmdname,MAX_PATH);
		function.param = next = cmdbuf + strlenW(cmdbuf) + 1;
		pptr = dptr;

									// パラメータの切り出し
#ifndef UNICODE
		while ( *pptr ){
			char tmp[CMDLINESIZE];

			tmp[0] = '\0';
			GetCommandParameter(&pptr,tmp,TSIZEOF(tmp));
			if ( (tmp[0] == '\0') && (SkipSpace(&pptr) != ',') ) break;
			paramcount++;
			AnsiToUnicode(tmp,next,CMDLINESIZE);
			next = next + strlenW(next) + 1;
			if ( NextParameter(&pptr) == FALSE ) break;
		}
#else
		while ( *pptr ){
			*next = '\0';
			GetCommandParameter(&pptr,next,CMDLINESIZE - VFPS);
			if ( (*next == '\0') && (SkipSpace(&pptr) != ',') ) break;
			paramcount++;
			next = next + strlenW(next) + 1;
			if ( NextParameter(&pptr) == FALSE ) break;
		}
#endif
		GetCurrentDirectory(TSIZEOF(olddir),olddir);
		SetCurrentDirectory(GetZCurDir(Z));
										// 検索と実行
		mdll = ppxmodule_list;
		module.command = &function;
		function.commandname = cmdbuf;
		function.commandhash = namehash;
		function.paramcount = paramcount;
		function.resultstring = DESTBUF;
		function.resultstring[0] = '\0';

		ppxa.info.Name = PPXAINFONAME;
		ppxa.info.RegID = PPXAINFOREGID;
		ppxa.info.Function = (PPXAPPINFOFUNCTIONW)CommandModuleInfoFunc;
		ppxa.info.hWnd = Z->hWnd;
		ppxa.parent = Z->Info;
		ppxa.Z = Z;

		for ( i = ppxmodule_count ; i ; i--,mdll++ ){
			if ( mdll->hDLL == NULL ){
				if ( LoadModuleFile(ppxa.info.hWnd,mdll,PPMTYPEFLAGS(PPXMEVENT_FUNCTION)) == FALSE ){
					continue;
				}
			}
#ifdef _WIN64
			result = mdll->ModuleEntry(&ppxa.info,PPXMEVENT_FUNCTION,module);
#else
			if ( mdll->ModuleEntry != NULL ){
				result = mdll->ModuleEntry(&ppxa.info,PPXMEVENT_FUNCTION,module);
			}else{
				result = mdll->OldModuleEntry(&ppxa.info,PPXMEVENT_FUNCTION,module);
			}
#endif
			if ( result == PPXMRESULT_SKIP ) continue;
			Z->result = (result == PPXMRESULT_STOP) ? ERROR_CANCELLED : NO_ERROR;
#ifndef UNICODE
			UnicodeToAnsi(DESTBUF,Z->dst,CMDLINESIZE);
#endif
			Z->dst += tstrlen(Z->dst);
			goto endfunc;
		}
		((TCHAR *)cmdbuf)[CMDLINESIZE - 1] = '\0';
		if ( NO_ERROR != GetCustTable(StrUserCommand,cmdname,(TCHAR *)cmdbuf,sizeof(cmdbuf)) ){
			XMessage(NULL,NULL,XM_GrERRld,T("Unknown function:%%*%s"),cmdname);
			Z->result = ERROR_INVALID_FUNCTION;
		}else{
			*Z->dst = *cmdname;

			if ( ((TCHAR *)cmdbuf)[CMDLINESIZE - 1] != '\0' ){
				TCHAR *longbuf;
				int size = GetCustTableSize(StrUserCommand,cmdname);

				longbuf = HeapAlloc(DLLheap,0,size);
				if ( longbuf == NULL ){
					Z->result = RPC_S_STRING_TOO_LONG;
					goto endfunc;
				}
				GetCustTable(StrUserCommand,cmdname,longbuf,size);
				UserCommand(Z,Z->dst,longbuf,Z->dst);
				HeapFree(DLLheap,0,longbuf);
			}else{
				UserCommand(Z,Z->dst,(TCHAR *)cmdbuf,Z->dst);
			}
			Z->dst += tstrlen(Z->dst);
		}
endfunc:
		SetCurrentDirectory(olddir);
		return;
	}
}
#undef PPXAINFONAME
#undef PPXAINFOREGID

PPXDLL int PPXAPI CallModule(PPXAPPINFO *info,DWORD func,PPXMODULEPARAM ModuleParam,CALLBACKMODULEENTRY CallBackModule)
{
	MODULESTRUCT *mdll;
	int i;
	COMMANDMODULEINFOSTRUCT ppxa;

#ifndef UNICODE
		WCHAR regidW[REGIDSIZE],nameW[MAX_PATH];

		AnsiToUnicode(info->Name,nameW,MAX_PATH);
		AnsiToUnicode(info->RegID,regidW,REGIDSIZE);
		#define PPXAINFONAME nameW
		#define PPXAINFOREGID regidW
#else
		#define PPXAINFONAME info->Name
		#define PPXAINFOREGID info->RegID
#endif
										// 検索と実行
	ppxa.info.Name = PPXAINFONAME;
	ppxa.info.RegID = PPXAINFOREGID;
	ppxa.info.Function = (PPXAPPINFOFUNCTIONW)CommandModuleInfoFunc;
	ppxa.info.hWnd = info->hWnd;
	ppxa.parent = info;
	ppxa.Z = NULL;

	if ( CallBackModule != NULL ){
		return CallBackModule(&ppxa.info,func,ModuleParam);
	}

	if ( ppxmodule_count < 0 ) LoadModuleList();
	mdll = ppxmodule_list;
	for ( i = ppxmodule_count ; i ; i--,mdll++ ){
		int result;

		if ( mdll->hDLL == NULL ){
			if ( LoadModuleFile(ppxa.info.hWnd,mdll,PPMTYPEFLAGS(func)) == FALSE ){
				continue;
			}
		}
#ifndef _WIN64
		if ( mdll->ModuleEntry == NULL ) continue;
#endif
		result = mdll->ModuleEntry(&ppxa.info,func,ModuleParam);
		if ( result == PPXMRESULT_SKIP ) continue;
		return result;
	}
	return PPXMRESULT_SKIP;
}
