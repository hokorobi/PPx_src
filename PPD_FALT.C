/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						内蔵デバッガ？
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <imagehlp.h>
#include <tlhelp32.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "PPX_64.H"
#pragma hdrstop

THREADSTRUCT *ThreadBottom = NULL;

#define AppName T("PPx")
#define AppExeName T("PP")

#ifdef _WIN64
	#define MSGBIT T("x64")
	#define REGPREFIX(reg) R ## reg
	#define CHECK_HOOK "hookx64"

	#define CPUTYPE IMAGE_FILE_MACHINE_AMD64
	#define LOADWINAPI164(name) {(void (WINAPI **)())&(D ## name), #name "64"}
	#define TAIL64(name) name ## 64
	#ifdef UNICODE
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name "W64"}
		#define TAIL64T(name) name ## W64
	#else
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name "64"}
		#define TAIL64T(name) name ## 64
	#endif
#endif

#ifdef _M_ARM
	#define MSGBIT T("ARM")
	#define REGPREFIX(reg) reg
	#define CHECK_HOOK "hook"

	#define CPUTYPE IMAGE_FILE_MACHINE_ARM
	#define LOADWINAPI164(name) {(void (WINAPI **)())&(D ## name), #name}
	#define TAIL64(name) name
	#ifdef UNICODE
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name "W"}
		#define TAIL64T(name) name ## W
	#else
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name}
		#define TAIL64T(name) name
	#endif
#endif

#ifdef _M_ARM64
	#define MSGBIT T("ARM64")
	#define REGPREFIX(reg) reg
	#define CHECK_HOOK "hook64"

	#define CPUTYPE IMAGE_FILE_MACHINE_ARM64
	#define LOADWINAPI164(name) {(void (WINAPI **)())&(D ## name), #name}
	#define TAIL64(name) name
	#ifdef UNICODE
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name "W"}
		#define TAIL64T(name) name ## W
	#else
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name}
		#define TAIL64T(name) name
	#endif
#endif

#ifndef CPUTYPE
	#define MSGBIT
	#define REGPREFIX(reg) E ## reg
	#define CHECK_HOOK "hook"

	#define CPUTYPE IMAGE_FILE_MACHINE_I386
	#define LOADWINAPI164(name) {(void (WINAPI **)())&(D ## name), #name}
	#define TAIL64(name) name
	#ifdef UNICODE
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name "W"}
		#define TAIL64T(name) name ## W
	#else
		#define LOADWINAPI164TW(name) {(void (WINAPI **)())&(D ## name), #name}
		#define TAIL64T(name) name
	#endif
#endif

#ifdef UNICODE
	#define GETDLLPROCTA(handle, name) D ## name = (imp ## name)GetProcAddress(handle, #name "W");

	typedef struct {
		DWORD dwSize;
		DWORD th32ModuleID;
		DWORD th32ProcessID;
		DWORD GlblcntUsage;
		DWORD ProccntUsage;
		BYTE *modBaseAddr;
		DWORD modBaseSize;
		HMODULE hModule;
		WCHAR szModule[MAX_MODULE_NAME32 + 1];
		WCHAR szExePath[MAX_PATH];
	} MODULEENTRY32X;
#else
	#define GETDLLPROCTA(handle, name) D ## name = (imp ## name)GetProcAddress(handle, #name);
	#define MODULEENTRY32X MODULEENTRY32
#endif

#ifdef WINEGCC
	#define EXTTYPE T("Wine")
#else
	#ifdef USEDIRECTX
		#ifdef USEDIRECTWRITE
			#define EXTTYPE T("Dw")
		#else
			#define EXTTYPE T("D9")
		#endif
	#else
		#define EXTTYPE
	#endif
#endif

#ifdef UNICODE
	#define MSGVER	T(FileProp_Version) EXTTYPE L" UNICODE " MSGBIT L"(" T(__DATE__) L")"
	#define MSGCTYPE T("U") MSGBIT EXTTYPE
#else
	#define MSGVER	FileProp_Version EXTTYPE " Multibyte(" T(__DATE__) ")"
	#define MSGCTYPE "M" EXTTYPE
#endif

#ifndef EXCEPTION_IN_PAGE_ERROR
	#define EXCEPTION_IN_PAGE_ERROR 0xC0000006
#endif

#define STACKWALKSIZE 0x3000

const TCHAR msgtitle[] = T("PPx Internal Error");
const TCHAR msgexfthread[] = T("ExceptionFilter");
const TCHAR NoItem[] = T("-");
//------------------------------------------------------------ 例外表示テーブル
#define msgstr_fault 0
#define msgstr_except 1
#define msgstr_unknownthread 2
#define msgstr_memaddress 3
#define msgstr_faultread 4
#define msgstr_faultwrite 5
#define msgstr_div0 6
#define msgstr_stackflow 7
#define msgstr_brokencust 8
#define msgstr_sendreport 9
#define msgstr_foundprom 10
#define msgstr_deepprom 11
#define msgstr_faultsend 12
#define msgstr_faultDEP 13

const TCHAR *msgstringsJ[] = {
	T("%s で、%s異常の対処ができなかったため、終了します。\n")
	T("  詳細\tWin%d.%d,") MSGVER T("\n%s\n%s%s"), // 0 msgstr_fault

	T("例外 %XH"), // 1 msgstr_except
	T("不明"), // 2 msgstr_unknownthread
	T("メモリ(") T(PTRPRINTFORMAT) T("H)の%s失敗"), // 3 msgstr_memaddress
	T("読込"), // 4 msgstr_faultread
	T("書込"), // 5 msgstr_faultwrite
	T("0除算"), // 6 msgstr_div0
	T("スタック"), // 7 msgstr_stackflow
	T("\n\n※カスタマイズファイルが破損したかもしれません。\nPPcustを起動すると破損チェックが行われます。"), // 8 msgstr_brokencust
	T("「はい」で、ブラウザを開いて前記内容のみを作者に送信します。\n")
	T("対策できるかもしれませんので、できれば送信をお願いします。"), // 9 msgstr_sendreport
	T("次の問題を検出しました。継続して使用できます。\n")
	T("  詳細\tWin%d.%d,") MSGVER T("\n\t%s\n\n%s"), // 10 msgstr_foundprom
	T("\n\n※異常報告中の異常再発生、メモリ不足、IMAGEHLP.DLLが使えない等の深刻な問題が起きていると思われます。"), // 11 msgstr_deepprom
	T("送信に失敗しました。"), // 12 msgstr_faultsend
	T("DEP"), // 13 msgstr_faultDEP
};

const TCHAR *msgstringsE[] = {
	T("Detected error in %s thread.\n")
	T("Terminating PPx by %s error.\n")
	T(" detail\tWin%d.%d,") MSGVER T("\n%s\n%s%s"), // 0

	T("Exception %XH"),
	T("Unknown"),
	T(PTRPRINTFORMAT) T("H %s memory access"),
	T("Read"),
	T("Write"),
	T("Divide by zero"),
	T("Stack"),
	T("\n\nSeem Customize data collapse."),
	T("When you select 'yes',this report is sent to TORO."),
	T("Detected probrem. It can be used continuously.\n")
	T(" detail\tWin%d.%d,") MSGVER T("\n\t%s\n\n%s"),
	T("\n\n*It seems that a serious problem such as memory shortage and IMAGEHLP.DLL has occurred. "), // 11 msgstr_deepprom
	T("send fault."),
	T("DEP"), // 13
};


#if defined(UNICODE) && !defined(CBA_READ_MEMORY) && !defined(WINEGCC)
typedef struct {
	DWORD SizeOfStruct, BaseOfImage, ImageSize, TimeDateStamp, CheckSum, NumSyms;
	SYM_TYPE SymType;
	WCHAR ModuleName[32], ImageName[256], LoadedImageName[256];
} IMAGEHLP_MODULEW, *PIMAGEHLP_MODULEW;
#endif
DefineWinAPI(BOOL, StackWalk, (DWORD, HANDLE, HANDLE, TAIL64(LPSTACKFRAME), PVOID,
	TAIL64(PREAD_PROCESS_MEMORY_ROUTINE),
	TAIL64(PFUNCTION_TABLE_ACCESS_ROUTINE),
	TAIL64(PGET_MODULE_BASE_ROUTINE),
	TAIL64(PTRANSLATE_ADDRESS_ROUTINE))
) = NULL;

DefineWinAPI(BOOL,SymInitialize,(HANDLE,PCSTR,BOOL)) = NULL;
DefineWinAPI(PVOID,SymFunctionTableAccess,(HANDLE,DWORD_PTR));
DefineWinAPI(BOOL,SymGetSymFromAddr,
	(HANDLE,DWORD_PTR,DWORD_PTR *,TAIL64(IMAGEHLP_SYMBOL) *));
DefineWinAPI(BOOL,SymCleanup,(HANDLE));

DefineWinAPI(DWORD_PTR,SymGetModuleBase,(HANDLE,DWORD_PTR));
DefineWinAPI(BOOL,SymGetModuleInfo,(HANDLE,DWORD_PTR,TAIL64T(IMAGEHLP_MODULE) *)) = NULL;

LOADWINAPISTRUCT IMAGEHLPDLL[] = {
	LOADWINAPI164(StackWalk),
	LOADWINAPI1(SymInitialize),
	LOADWINAPI164(SymFunctionTableAccess),
	LOADWINAPI164(SymGetModuleBase),
	LOADWINAPI164(SymGetSymFromAddr),
	LOADWINAPI1(SymCleanup),
	LOADWINAPI164TW(SymGetModuleInfo),
	{NULL,NULL}
};

#define defThreadQuerySetWin32StartAddress 9

int ShowErrorDialog(const TCHAR **Msg,int msgtype,const TCHAR *threadtext,const TCHAR *infotext,const TCHAR *addrtext,const TCHAR *comment);

// 指定スレッドを検索 ---------------------------------------------------------
THREADSTRUCT *GetThreadInfoFromID(THREADSTRUCT **prev, DWORD ThreadID)
{
	THREADSTRUCT *pT, *prevT = NULL;

	pT = ThreadBottom;
	if ( IsTrue(IsBadReadPtr(pT,sizeof(THREADSTRUCT))) ){
		ThreadBottom = NULL;
		return NULL;
	}

	for ( ; pT != NULL ; pT = pT->next ){
		if ( pT->ThreadID == ThreadID ) break;
		prevT = pT;
		// 次が消滅していないか調べる
		if ( IsTrue(IsBadReadPtr(pT->next,sizeof(THREADSTRUCT))) ){
			pT = pT->next = NULL;
			break;
		}
	}
	if ( prev != NULL ) *prev = prevT;
	return pT;
}

THREADSTRUCT *GetCurrentThreadInfo(void)
{
	return GetThreadInfoFromID(NULL,GetCurrentThreadId());
}

PPXDLL BOOL PPXAPI PPxWaitExitThread(void)
{
	DWORD CurrentThreadID;

	CurrentThreadID = GetCurrentThreadId();
	{
		for ( ; ; ){
			THREADSTRUCT *ts;
			MSG msg;

			EnterCriticalSection(&ThreadSection);
			ts = ThreadBottom;
			if ( ts == NULL ) break;
			if ( IsTrue(IsBadReadPtr(ts,sizeof(THREADSTRUCT))) ){
				ThreadBottom = NULL;
				break;
			}
			for ( ;; ){
				THREADSTRUCT *nextts;

				// 強制終了を許可していないスレッドがあるか
				if (	(ts->ThreadID != CurrentThreadID) &&
						!(ts->flag & XTHREAD_EXITENABLE) ){
					break;
				}
				nextts = ts->next;
				if ( nextts != NULL ){
					// 次が消滅していないか調べる
					if ( IsBadReadPtr(nextts,sizeof(THREADSTRUCT)) == FALSE ){
						ts = nextts;
						continue;
					}
					ts->next = NULL;
				}
				LeaveCriticalSection(&ThreadSection);
				return TRUE;
			}
			LeaveCriticalSection(&ThreadSection);

			// 現在のスレッドが終了するのを待つ
			if ( DOpenThread == INVALID_HANDLE_VALUE ){
				GETDLLPROC(hKernel32,OpenThread);
			}

			while ( IsTrue(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) ){
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if ( DOpenThread == NULL ){
				Sleep(100);
			}else{
				HANDLE hThread;

				if ( IsBadReadPtr(ts,sizeof(THREADSTRUCT)) == FALSE ){
					hThread = DOpenThread(SYNCHRONIZE,FALSE,ts->ThreadID);
					if ( hThread != NULL ){
						DWORD result = MsgWaitForMultipleObjects(1,&hThread,FALSE,200,QS_ALLEVENTS);

						CloseHandle(hThread);
						if ( result == (WAIT_OBJECT_0 + 1) ) continue; // Message
						if ( result != WAIT_TIMEOUT ) hThread = NULL;
					}	// スレッドを開けない→強制終了と見なす
					if ( (hThread == NULL) && (IsBadReadPtr(ts,sizeof(THREADSTRUCT)) == FALSE) ){
						setflag(ts->flag,XTHREAD_EXITENABLE);
					}
				}
			}
		}
		LeaveCriticalSection(&ThreadSection);
	}
	return TRUE;
}

// 処理中のスレッドを登録 -----------------------------------------------------
// ●PPxWaitExitThread の直前に位置すること
PPXDLL BOOL PPXAPI PPxRegisterThread(THREADSTRUCT *threadstruct)
{
	if ( threadstruct == NULL ){ // NULL...スレッド登録済みかを確認する
		THREADSTRUCT *foundthread;

		UsePPx();
		foundthread = GetCurrentThreadInfo();
		FreePPx();
		return (foundthread != NULL);
	}

	threadstruct->next = NULL;
	threadstruct->ThreadID = GetCurrentThreadId();
	threadstruct->PPxNo = -1;

	EnterCriticalSection(&ThreadSection);
	if ( ThreadBottom == NULL ){		// 新規登録
		ThreadBottom = threadstruct;
	}else{
		THREADSTRUCT *prevthread;

		if ( GetThreadInfoFromID(&prevthread,threadstruct->ThreadID) != NULL ){
			LeaveCriticalSection(&ThreadSection);
			return FALSE;	// 既に登録済み
		}
									// 追加登録
		prevthread->next = threadstruct;
	}
	LeaveCriticalSection(&ThreadSection);

	if ( X_jlst[0] == JOBLIST_THREAD ){
		if ( tstrcmp(threadstruct->ThreadName,T("Joblist")) ){
			SetJobTask(NULL,JOBSTATE_REGIST | JOBSTATE_THREAD);
		}
	}
//	*(BYTE *)0x1234 = 0; // 落とすコード
	return TRUE;
}

// 処理中のスレッドを登録を抹消する -------------------------------------------
void UnRegisterThread(DWORD ThreadID)
{
	THREADSTRUCT *pT,*PrevT;

	pT = GetThreadInfoFromID(&PrevT,ThreadID);
	if ( pT != NULL ){
		if ( PrevT == NULL ){		// 先頭
			ThreadBottom = pT->next;
		}else{
			PrevT->next = pT->next;
		}
	}
}

PPXDLL BOOL PPXAPI PPxUnRegisterThread(void)
{
	DWORD ThreadID;

	if ( X_jlst[0] == JOBLIST_THREAD ) SetJobTask(NULL,JOBSTATE_UNREGIST);

	ThreadID = GetCurrentThreadId();
	UsePPx();
	UnRegisterThread(ThreadID);
	// Job が登録されていたら削除する
	{
		int i;

		for ( i = 0 ; i < X_MaxJob ; i++ ){
			if ( Sm->Job[i].ThreadID == ThreadID ) Sm->Job[i].ThreadID = 0;
		}
	}
	FreePPx();
	return TRUE;
}

const TCHAR *GetThreadName(DWORD id)
{
	THREADSTRUCT *pT;

	pT = GetThreadInfoFromID(NULL,id);
	if ( pT != NULL ) return pT->ThreadName;
	return T("???");
}

#define WSSIZE 1024
void QueryEncode(TCHAR *dst)
{
	char bufA[CMDLINESIZE],*srcA;
	TCHAR *dest, *maxptr;

#ifdef UNICODE
	WideCharToMultiByteU8(CP_UTF8,0,dst,-1,bufA,CMDLINESIZE,NULL,NULL);
#else
	WCHAR bufW[CMDLINESIZE];

	AnsiToUnicode(dst,bufW,CMDLINESIZE);
	WideCharToMultiByteU8(CP_UTF8,0,bufW,-1,bufA,CMDLINESIZE,NULL,NULL);
#endif
	srcA = bufA;
	dest = dst;
	maxptr = dest + WSSIZE - 100;
	while ( dest < maxptr ){
		BYTE c;

		c = (BYTE)*srcA++;
		if ( c == '\0' ) break;
		if ( IsalnumA(c) || (c == '=') ){
			*dest++ = c;
		}else if ( c == '\1' ){
			*dest++ = '&';
		}else if ( c == ' ' ){
			*dest++ = '+';
		}else{
			dest += wsprintf(dest,T("%%%02X"),c);
		}
	}
	*dest = '\0';
}

const TCHAR *FixCustTable(void)
{
	BYTE *ptr,*maxptr;
	DWORD DataHeaderSize;
	WORD TableDataSize;
	TCHAR *DataName,*DataNamePtr;

	if ( enumcustcache.Counter != Sm->CustomizeWrite ) return NilStr;
	enumcustcache.Counter = Sm->CustomizeWrite - 1; // キャッシュを無効に

	ptr = enumcustcache.DataPtr;
	maxptr = CustP + X_Csize - CUST_FOOTER_SIZE;
	// Data ヘッダのチェック
	if ( (ptr <= CustP) || (ptr >= maxptr) ) return T("Hdr");
	DataHeaderSize = *(DWORD *)ptr;
	if ( DataHeaderSize < (sizeof(DWORD) + sizeof(TCHAR) + CUST_TABLE_FOOTER_SIZE) ){
		*(DWORD *)ptr = 0;
		 return T("Hdr");
	}
	if ( ((ptr + DataHeaderSize) > maxptr) ){
		DataHeaderSize = maxptr - ptr;
		if ( DataHeaderSize < (sizeof(DWORD) + sizeof(TCHAR) + CUST_TABLE_FOOTER_SIZE) ){
			*(DWORD *)ptr = 0;
			 return T("Hdr");
		}
		*(DWORD *)ptr = DataHeaderSize;
		*(DWORD *)maxptr = 0;
	}

	DataNamePtr = DataName = (TCHAR *)(ptr + 4);
	// Data name のチェック
	for ( ;; ){
		if ( (BYTE *)DataNamePtr >= maxptr ) return T("HdrName");
		if ( *DataNamePtr == '\0' ) break;
		DataNamePtr++;
	}
	// Table のチェック
	maxptr = ptr + DataHeaderSize - CUST_TABLE_FOOTER_SIZE;
	ptr = (BYTE *)(DataNamePtr + 1);
	for ( ;; ){
		TCHAR *TableNamePtr;
		// Table ヘッダのチェック
		TableDataSize = *(WORD *)ptr;
		if ( TableDataSize == 0 ) break;
		if ( (ptr + TableDataSize) > maxptr ){
			*(WORD *)ptr = 0;
			return DataName;
		}
		// Table name のチェック
		TableNamePtr = (TCHAR *)(ptr + sizeof(WORD));
		for ( ;; ){
			if ( (BYTE *)TableNamePtr >= maxptr ){
				*(WORD *)ptr = 0;
				return DataName;
			}
			if ( *TableNamePtr == '\0' ) break;
			TableNamePtr++;
		}
		ptr += TableDataSize;
	}
	return T("no error");
}

// address ( modulename [basename].funcname + offset 形式を出力する
void GetFailedModuleAddr(HANDLE hProcess,TAIL64(IMAGEHLP_SYMBOL) *syminfo,TCHAR *dest,void *addr)
{
	TAIL64T(IMAGEHLP_MODULE) mdlinfo;
	DWORD_PTR funcoffset;

	// address
	dest += wsprintf(dest,T(PTRPRINTFORMAT),addr);
	mdlinfo.SizeOfStruct = sizeof(mdlinfo);
	// modulename,basename
	if ( (DSymGetModuleInfo != NULL) && IsTrue(DSymGetModuleInfo(
			GetCurrentProcess(),(DWORD_PTR)addr,&mdlinfo)) ){
		dest += wsprintf(dest,T("(%s:") T(PTRPRINTFORMAT),
				mdlinfo.ModuleName,mdlinfo.BaseOfImage);
	}else{
		if ( ((DWORD_PTR)addr >= (DWORD_PTR)DLLhInst) &&
			 ((DWORD_PTR)addr < ((DWORD_PTR)DLLhInst + 0xb0000)) ){
			dest += wsprintf(dest,T("(PPLIB:") T(PTRPRINTFORMAT),DLLhInst);
		}
	}
	// funcname,offset
	if ( (DSymGetSymFromAddr != NULL) && IsTrue(DSymGetSymFromAddr(
			hProcess,(DWORD_PTR)addr,&funcoffset,syminfo)) ){
		#ifdef UNICODE
		WCHAR funcnameW[256];

		AnsiToUnicode(syminfo->Name,funcnameW,256);
		dest += wsprintf(dest,T(".%s+") T(PTRPRINTFORMAT),funcnameW,funcoffset);
		#else
		dest += wsprintf(dest,T(".%s+") T(PTRPRINTFORMAT),syminfo->Name,funcoffset);
		#endif
	}
	*dest++ = ')';
	*dest = '\0';
}

//-------------------------------------------------------------------- 例外処理
#define SHOWSTACKS 3 // 標準のスタック表示数
#define SHOWSTACKSBUFSIZE 0x200

typedef struct {
	EXCEPTION_POINTERS *ExceptionInfo;
	THREADSTRUCT *FaultThread;
	HANDLE hThread;
	volatile DWORD Result;
} ExceptionData;

volatile DWORD PPxUnhandledExceptionFilterMainCount = 0;
volatile PVOID PPxUnhandledExceptionFilterOldAddress;

void GetExceptionCodeText(EXCEPTION_RECORD *ExceptionRecord, const TCHAR **Msg, TCHAR *exceptionbuf, const TCHAR **Comment)
{
	if ( (ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) ||
		 (ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) ){
		DWORD rwmode = msgstr_faultread;
		DWORD rwinfo = (DWORD)ExceptionRecord->ExceptionInformation[0];

		if ( rwinfo ){
			rwmode = (rwinfo == 8) ? msgstr_faultDEP : msgstr_faultwrite;
		}
		wsprintf(exceptionbuf,Msg[msgstr_memaddress],
				ExceptionRecord->ExceptionInformation[1],Msg[rwmode]);
		if ( (Comment != NULL) &&
			 (ExceptionRecord->ExceptionAddress >= FUNCCAST(void *,DefCust)) &&
			 (ExceptionRecord->ExceptionAddress <= FUNCCAST(void *,GetCustDword)) ){
			*Comment = Msg[msgstr_brokencust];
		}
	}else if ( ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO){
		tstrcpy(exceptionbuf,Msg[msgstr_div0]);
	}else if ( ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW ){
		tstrcpy(exceptionbuf,Msg[msgstr_stackflow]);
	}else{
		wsprintf(exceptionbuf,Msg[msgstr_except],ExceptionRecord->ExceptionCode);
	}
}

DWORD WINAPI PPxUnhandledExceptionFilterMain(ExceptionData *EP)
{
	EXCEPTION_RECORD *ExceptionRecord;
	HANDLE hProcess;
	char symblebuffer[STACKWALKSIZE];
	TAIL64(IMAGEHLP_SYMBOL) *syminfo;
	EXCEPTION_POINTERS *ExceptionInfo;

	TCHAR threadnamebuf[0x100];
	const TCHAR *threadname;

	THREADSTRUCT *tstruct;

	TCHAR exceptionbuf[256],stackinfo[SHOWSTACKSBUFSIZE + 16],*stackptr;
	const TCHAR **Msg = msgstringsE;
	const TCHAR *Comment = NilStr;
	int result = 0;
	TAIL64(STACKFRAME) sframe;

	if ( LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE ) Msg = msgstringsJ;

	ExceptionInfo = EP->ExceptionInfo;
	ExceptionRecord = ExceptionInfo->ExceptionRecord;

	// 例外詳細 exception
	GetExceptionCodeText(ExceptionRecord, Msg, exceptionbuf, &Comment);

	// PPxUnhandledExceptionFilterMain 内で落ちたかもしれないので前回のを表示
	if ( PPxUnhandledExceptionFilterMainCount != 0 ){
		TCHAR *eb;
		wsprintf(stackinfo,
				T("n:") T(PTRPRINTFORMAT) T("(PPLIB:") T(PTRPRINTFORMAT)
				T(")\np:") T(PTRPRINTFORMAT),
				ExceptionRecord->ExceptionAddress,DLLhInst,
				PPxUnhandledExceptionFilterOldAddress);
		eb = exceptionbuf + tstrlen(exceptionbuf);
		*eb++ = '\n';
		*eb++ = 'p';
		*eb++ = ':';
		GetExceptionCodeText(ExceptionRecord, Msg, eb, NULL);

		ShowErrorDialog(Msg,msgstr_fault,msgexfthread,exceptionbuf,stackinfo,Msg[msgstr_deepprom]);
		ExitProcess(ExceptionRecord->ExceptionCode);
		return EXCEPTION_EXECUTE_HANDLER;
	}
	PPxUnhandledExceptionFilterOldAddress = ExceptionRecord->ExceptionAddress;
	PPxUnhandledExceptionFilterMainCount++;

	// デバッグ情報、シンボル情報取得の準備
	hProcess = GetCurrentProcess();

	if ( DSymInitialize == NULL ){
		LoadSystemWinAPI(SYSTEMDLL_IMAGEHLP, IMAGEHLPDLL);
	}
	if ( DSymInitialize != NULL ){
		DSymInitialize(hProcess,NULL,TRUE);
		syminfo = (TAIL64(IMAGEHLP_SYMBOL) *)symblebuffer;
		syminfo->SizeOfStruct = STACKWALKSIZE;
		syminfo->MaxNameLength = STACKWALKSIZE - sizeof(TAIL64(IMAGEHLP_SYMBOL));
	}

	// 登録 PPx をテーブルから排除、Thread name取得
	tstruct = EP->FaultThread;
	if ( tstruct != NULL ){
		if ( tstruct->PPxNo >= 0 ){
			Sm->P[tstruct->PPxNo].ID[0] = '\0';
		}
		threadname = tstruct->ThreadName;
		if ( IsTrue(IsBadReadPtr(threadname,8)) ){
			threadname = T("broken PPx");
		}
	}else{ // PPx の登録スレッド以外
		TCHAR *namelast;
		DefineWinAPI(LONG,NtQueryInformationThread,(HANDLE,int,PVOID,ULONG,PULONG));

		GetModuleFileName(NULL,stackinfo,SHOWSTACKSBUFSIZE);
		tstrcpy(threadnamebuf,FindLastEntryPoint(stackinfo));
		threadname = threadnamebuf;
		namelast = threadnamebuf + tstrlen(threadnamebuf);
		*namelast++ = ' ';

		GETDLLPROC(GetModuleHandle(T("ntdll.dll")),NtQueryInformationThread);
		if ( DNtQueryInformationThread == NULL ){
			tstrcpy(namelast,Msg[msgstr_unknownthread]);
		}else{
			ULONG ulLength;
			PVOID pBeginAddress = NULL;

			DNtQueryInformationThread(EP->hThread,
					defThreadQuerySetWin32StartAddress,&pBeginAddress,
					sizeof(pBeginAddress),&ulLength);

			if ( pBeginAddress == FUNCCAST(PVOID,PPxUnhandledExceptionFilterMain) ){
				wsprintf(namelast,T("%s(PPLIB:") T(PTRPRINTFORMAT) T(")"),msgexfthread,DLLhInst);
			}else{
				GetFailedModuleAddr(hProcess,syminfo,namelast,pBeginAddress);
			}
		}
	}
	stackptr = stackinfo;
	stackinfo[0] = '\0';

	// スタック/例外アドレス stackinfo
	if ( DStackWalk != NULL ){
		BOOL findapp = FALSE;
		CONTEXT targetThc;
		int stacks = SHOWSTACKS;
		int count = 0;

		targetThc = *ExceptionInfo->ContextRecord;
		targetThc.ContextFlags = CONTEXT_FULL;
		memset(&sframe,0,sizeof(sframe));

#ifdef _M_ARM
		sframe.AddrPC.Offset = 0;
		sframe.AddrStack.Offset = 0;
		sframe.AddrFrame.Offset = 0;
#else
		sframe.AddrPC.Offset = ExceptionInfo->ContextRecord->REGPREFIX(ip);
		sframe.AddrStack.Offset = ExceptionInfo->ContextRecord->REGPREFIX(sp);
		sframe.AddrFrame.Offset = ExceptionInfo->ContextRecord->REGPREFIX(bp);
#endif
		sframe.AddrPC.Mode = AddrModeFlat;
		sframe.AddrStack.Mode = AddrModeFlat;
		sframe.AddrFrame.Mode = AddrModeFlat;

		for (;;){
			TCHAR addrstr[512];
			BOOL sresult;

			count++;
			sresult = DStackWalk(CPUTYPE, hProcess, EP->hThread, &sframe,
					&targetThc, NULL, DSymFunctionTableAccess,
					DSymGetModuleBase, NULL);
			if ( (sresult == 0) ||
				 (sframe.AddrPC.Offset == sframe.AddrReturn.Offset) ){
				break;
			}
			// エラーアドレスが記載されていないときは追加
			if ( (count == 1) &&
				((void *)sframe.AddrPC.Offset !=
					 (void *)ExceptionRecord->ExceptionAddress) ){
				GetFailedModuleAddr(hProcess,syminfo,addrstr,
						ExceptionRecord->ExceptionAddress);
				XMessage(NULL,NULL,XM_DbgLOG,T("Addr:%s"),addrstr);
				stackptr += wsprintf(stackptr,T("\t*%s\n"),addrstr);
			}
			// 指示アドレスの情報を記載
			GetFailedModuleAddr(hProcess,syminfo,addrstr,
					(void *)sframe.AddrPC.Offset);
			XMessage(NULL,NULL,XM_DbgLOG,T("Stack:%s"),addrstr);

			if ( stacks || !findapp ){ // 残り行数有りか、Appが見つかっていない
				if ( !findapp && (tstrstr(addrstr,T("(") AppExeName) != NULL)){
					findapp = TRUE;
					if ( stacks <= 1 ) stacks = 2;
				}

				// カスタマイズ領域破損の詳細
				if ( (tstrstr(addrstr,T("SortCustTable")) != NULL) || (tstrstr(addrstr,T("EnumCustTable")) != NULL) || (tstrstr(addrstr,T("GetCustTable")) != NULL) ){
					wsprintf(addrstr + tstrlen(addrstr),T(" %s:") T(PTRPRINTFORMAT) T(",") T(PTRPRINTFORMAT),
							FixCustTable(),
							CustHeaderFromCustP,
							(BYTE *)CustHeaderFromCustP + X_Csize);
				}

				// PPc main_2nd と思われるスレッド(PPxUnRegisterThread後)
				if ( (tstrstr(addrstr,T("CriticalSection")) != NULL) &&
					 (tstruct == NULL) &&
					 (tstrstr(threadname,T("(PPC")) != NULL) ){
					stacks += 3;
					if ( NowExit ) result = IDNO;
					// スレッド共通化
					GetCustData(T("X_combos"),&X_combos_,sizeof(X_combos_));
					setflag(X_combos_[0],CMBS_THREAD);
					SetCustData(T("X_combos"),&X_combos_,sizeof(X_combos_));
				}

				// 送信用情報の用意
				if ( (stacks || findapp) &&
					 (((stackptr - stackinfo) + tstrlen(addrstr)) < (SHOWSTACKSBUFSIZE - 8)) ){
					stackptr += wsprintf(stackptr,T("\t%d:%s\n"),count,addrstr);
					if ( stacks ) stacks--;
				}
			}
		}
		if ( ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW ){
			stackptr += wsprintf(stackptr,T("\tstacks:%d:%d#%x\n"),
					count, CrmenuCheck.enter, CrmenuCheck.exectype);
		}
	}
	if ( stackinfo == stackptr ){
		GetFailedModuleAddr(hProcess,syminfo,stackinfo,
				ExceptionRecord->ExceptionAddress);
	}
	{
		DWORD tick = GetTickCount();

		if ( (tick - StartTick) < 3000 ) tstrcat(stackinfo,T("(B)"));
		if ( (tick - CustTick) < 3000 ) tstrcat(stackinfo,T("(C)"));
		if ( NowExit ) tstrcat(stackinfo,T("(E)"));
		if ( Sm->NowShutdown ) tstrcat(stackinfo,T("(S)"));
	}

	// PPxスレッド以外であり、PPx 以外の実行ファイルで、PPxが関与していないならエラー処理をしない
	if ( (tstrchr(threadname,'(') != NULL) &&
		 (tstristr(threadname,T("PP")) == NULL) &&
		 (tstristr(stackinfo,T("(") AppExeName) == NULL) ){
		result = IDCANCEL;
	// スレッド再起動可能スレッドで、一部の異常が起きやすいDLLなら、Thr再起動
	}else if ( ((tstruct != NULL) && (tstruct->flag & XTHREAD_RESTARTREQUEST))
		&&
		( (WinType == WINTYPE_VISTA) || // ← SHGetFileInfo が DOS EXE で落ちる対策、XP,7,10 では問題なし
		  (tstrstr(stackinfo,T("SS1H001")) != 0) ||
		  (tstrstr(stackinfo,T("SugarSyncShellExt")) != 0) ||
		  (tstrstr(stackinfo,T("Resource")) != 0) || // LdrResSearchResource, FindResourceEx, BaseDllMapResourceId
		  (tstrstr(stackinfo,T("TortoiseSVN")) != 0) ||
		  (tstrstr(stackinfo,T("libapr_tsvn")) != 0) ) ){
		result = IDNO;
	// 例外そのものか、例外を握りつぶして動作する物なら続行させて、任せる
	}else if (
			(tstrstr(stackinfo,T(CHECK_HOOK)) != 0) ||
			(tstrstr(stackinfo,T("RaiseException")) != 0) ){
		result = IDCANCEL;
	#ifdef _WIN64
	}else if ( Sm->NowShutdown &&
		(((DWORD_PTR)ExceptionRecord->ExceptionAddress & 0x7FF00000ffff) == 0x7FF0000024EB) ){
		result = IDNO;
	#endif
	}else if ( result == 0 ){
		result = ShowErrorDialog(Msg,msgstr_fault,threadname,exceptionbuf,stackinfo,Comment);
	}
	PPxUnhandledExceptionFilterMainCount--;

	if ( result == IDCANCEL ){
		if ( DSymCleanup != NULL ) DSymCleanup(hProcess);
		EP->Result = EXCEPTION_CONTINUE_SEARCH;
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if ( (tstruct != NULL) &&
		 (tstruct->flag & (XTHREAD_RESTARTREQUEST | XTHREAD_TERMENABLE)) ){
		HANDLE hCloseThread;

		if ( DSymCleanup != NULL ) DSymCleanup(hProcess);
		if ( tstruct->flag & XTHREAD_RESTARTREQUEST ){
			SendMessage( ((RESTARTTHREADSTRUCT *)tstruct)->hParentWnd,
					WM_PPXCOMMAND,
					((RESTARTTHREADSTRUCT *)tstruct)->wParam,
					((RESTARTTHREADSTRUCT *)tstruct)->lParam);
		}
		hCloseThread = EP->hThread;
		TerminateThread(hCloseThread,0);
		CloseHandle(hCloseThread);
		return 0;
	}

	ExitProcess(ExceptionRecord->ExceptionCode);
	return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI PPxUnhandledExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo)
{
	ExceptionData EP;
	DWORD tid;
	HANDLE hThread,hProcess;
	THREADSTRUCT *PrevT;

	EXCEPTION_RECORD *ExceptionRecord = ExceptionInfo->ExceptionRecord;
	DWORD_PTR checkaddr = (DWORD_PTR)ExceptionRecord->ExceptionAddress;
	DWORD_PTR targetaddr;
	DWORD ExceptionCode = ExceptionRecord->ExceptionCode;

#ifndef _WIN64
	if ( ExceptionCode == EXCEPTION_FLT_INVALID_OPERATION ){
		if ( InitFloat() != FALSE ) return EXCEPTION_CONTINUE_EXECUTION;
	}
#endif
	if ( ExceptionCode & B29 ){ // ユーザ定義
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if ((ExceptionCode != EXCEPTION_ACCESS_VIOLATION) &&
//		(ExceptionCode != EXCEPTION_DATATYPE_MISALIGNMENT) && // 境界アクセス違反
		(ExceptionCode != EXCEPTION_FLT_INVALID_OPERATION) &&
		(ExceptionCode != EXCEPTION_IN_PAGE_ERROR) &&
		(ExceptionCode != EXCEPTION_STACK_OVERFLOW) &&
		(ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO) ){
//		XMessage(NULL,NULL,XM_DbgLOG,T("Exception! code:%xH"),ExceptionRecord->ExceptionCode);
		return EXCEPTION_CONTINUE_SEARCH;
	}
	// IsBadWritePtr/IsBadReadPtr 内は無視
	targetaddr = (DWORD_PTR)GetProcAddress(hKernel32,"IsBadWritePtr");
	if ( targetaddr != 0 ){
		if ( (checkaddr >= (targetaddr -  0x60)) &&
			 (checkaddr <  (targetaddr + 0x140)) ){
			return EXCEPTION_CONTINUE_SEARCH;
		}
		targetaddr = (DWORD_PTR)GetProcAddress(hKernel32,"IsBadReadPtr");
		if ( (checkaddr >= (targetaddr -  0x60)) &&
			 (checkaddr <  (targetaddr + 0x140)) ){
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	// Kaspersky Anti-Virus がメモリの読み込み違反の例外処理を
	// 正しく行っていないため、見つけたら(prremote 内で、-1 の読み込み違反)
	// 処理を続行させるようにする
	if ( (ExceptionCode == EXCEPTION_ACCESS_VIOLATION) &&
		(ExceptionRecord->ExceptionInformation[1] == (DWORD_PTR)-1) ){
		DWORD_PTR hPRREMOTE = (DWORD_PTR)GetModuleHandle(T("prremote.dll"));
		if ( (hPRREMOTE != 0) &&
			 (hPRREMOTE < checkaddr) ){
			return EXCEPTION_CONTINUE_SEARCH;
		}
		hPRREMOTE = (DWORD_PTR)GetModuleHandle(T("scrchpg.dll"));
		if ( (hPRREMOTE != 0) &&
			 (hPRREMOTE < checkaddr) ){
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}
/*
	if ( (checkaddr >= FUNCCAST(DWORD_PTR,PPxWaitExitThread)) &&
		 (checkaddr < FUNCCAST(DWORD_PTR,PPxRegisterThread)) ){
		*ExceptionInfo->ContextRecord = WaitThreadContext;
		ExceptionRecord->ExceptionFlags = 0;
		ExceptionRecord->ExceptionAddress = (PVOID)WaitThreadContext.REGPREFIX(ip);
		return EXCEPTION_CONTINUE_EXECUTION;
	}
*/
	// 報告ダイアログを表示するためのスレッドを作成する
	EP.Result = EXCEPTION_EXECUTE_HANDLER;
	EP.ExceptionInfo = ExceptionInfo;
	EP.FaultThread = GetThreadInfoFromID(&PrevT,GetCurrentThreadId());
	if ( EP.FaultThread != NULL ){ // PPxUnRegisterThread を行う
		if ( PrevT == NULL ){		// 先頭
			ThreadBottom = EP.FaultThread->next;
		}else{
			PrevT->next = EP.FaultThread->next;
		}
	}

	hProcess = GetCurrentProcess();
	DuplicateHandle(hProcess,GetCurrentThread(),
			hProcess,&EP.hThread,0,FALSE,DUPLICATE_SAME_ACCESS);
	hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)
			PPxUnhandledExceptionFilterMain,&EP,0,&tid);
	if ( hThread == NULL ) return EXCEPTION_CONTINUE_SEARCH;
	CloseHandle(hThread);
	while ( EP.Result == EXCEPTION_EXECUTE_HANDLER ){
		Sleep(100);
	}
	CloseHandle(EP.hThread);
	return EP.Result;
}

DWORD WINAPI PPxSendReportThread(TCHAR *text)
{
	const TCHAR **Msg = msgstringsE;

	if ( LOWORD(GetUserDefaultLCID()) == LCID_JAPANESE ) Msg = msgstringsJ;
	ShowErrorDialog(Msg,msgstr_foundprom,NoItem,text + 1,NoItem,NULL);
	text[0] = '\1';
	return 0;
}

void PPxSendReport(const TCHAR *text)
{
	TCHAR *textbuf;
	DWORD tid;

	textbuf = HeapAlloc(DLLheap,0,TSTRSIZE(text) + sizeof(TCHAR));
	if ( textbuf == NULL ) return;
	*textbuf = '\0';
	tstrcpy(textbuf + 1,text);
	CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)
			PPxSendReportThread,textbuf,0,&tid));
	while( textbuf[0] == '\0' ) Sleep(100);
}

int ShowErrorDialog(const TCHAR **Msg,int msgtype,const TCHAR *threadtext,const TCHAR *infotext,const TCHAR *addrtext,const TCHAR *comment)
{
	TCHAR msgbuf[WSSIZE],*typename;
	int result;
	DWORD OSsubver;

	OSsubver = (OSver.dwMajorVersion == 10) ? OSver.dwBuildNumber : OSver.dwMinorVersion;
	if ( msgtype == msgstr_foundprom ){
		wsprintf(msgbuf,Msg[msgstr_foundprom],OSver.dwMajorVersion,OSsubver,
				infotext,Msg[msgstr_sendreport]);
		typename = T("report");
	}else{
		wsprintf(msgbuf,Msg[msgstr_fault],threadtext,infotext,
				OSver.dwMajorVersion,OSsubver,
				addrtext,Msg[msgstr_sendreport],comment);
		typename = T("fault");
	}
	msgbuf[TSIZEOF(msgbuf) - 8] = '\0';
	XMessage(NULL,NULL,XM_DbgLOG,T("%s"),msgbuf);
	result = MessageBox(NULL,msgbuf,msgtitle,
			MB_YESNOCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP);
	if ( result == IDYES ){
		wsprintf(msgbuf,REPORTURL T("?exe=") AppName
			T("\1ver=") T(FileProp_Version) MSGCTYPE
			T("\1type=%s\1thread=%s\1info=%s\1os=%d.%d\1addr=%s"),
			typename,threadtext,infotext,
			OSver.dwMajorVersion,OSsubver,addrtext);
		msgbuf[TSIZEOF(msgbuf) - 8] = '\0';
		QueryEncode(tstrchr(msgbuf,'?') + 1);
		if ( PPxShellExecute(NULL,NULL,msgbuf,NULL,NULL,XEO_NOSCANEXE,msgbuf)
				== NULL ){
			MessageBox(NULL,Msg[msgstr_faultsend],msgbuf,MB_OK | MB_ICONSTOP);
		}
	}
	return result;
}

void CPUinfo(void)
{
	HANDLE hSnapshot;
	THREADENTRY32 ThreadData;
	MODULEENTRY32X ModuleData;
	DWORD pid = GetCurrentProcessId();
	HWND hEWnd;
	TCHAR buf[0x1000],buf1[0x400];
	MSG msg;
	HANDLE hProcess;
	char symblebuffer[STACKWALKSIZE];
	TAIL64(IMAGEHLP_SYMBOL) *syminfo;
	ThSTRUCT thText;

	DefineWinAPI(HANDLE,CreateToolhelp32Snapshot,(DWORD,DWORD));
	DefineWinAPI(BOOL,Thread32First,(HANDLE,LPTHREADENTRY32));
	DefineWinAPI(BOOL,Thread32Next,(HANDLE,LPTHREADENTRY32));
	DefineWinAPI(BOOL,Module32First,(HANDLE,MODULEENTRY32X *));
	DefineWinAPI(BOOL,Module32Next,(HANDLE,MODULEENTRY32X *));
	DefineWinAPI(BOOL,GetThreadTimes,(HANDLE,LPFILETIME,LPFILETIME,LPFILETIME,LPFILETIME));
	DefineWinAPI(LONG,NtQueryInformationThread,(HANDLE,int,PVOID,ULONG,PULONG));

	GETDLLPROC(hKernel32,CreateToolhelp32Snapshot);
	GETDLLPROC(hKernel32,Thread32First);
	GETDLLPROC(hKernel32,Thread32Next);
	GETDLLPROCTA(hKernel32,Module32First);
	GETDLLPROCTA(hKernel32,Module32Next);
	GETDLLPROC(hKernel32,OpenThread);
	GETDLLPROC(hKernel32,GetThreadTimes);
	GETDLLPROC(GetModuleHandleA("ntdll.dll"),NtQueryInformationThread);

	ThInit(&thText);

	hProcess = GetCurrentProcess();
	if ( DSymInitialize == NULL ){
		LoadSystemWinAPI(SYSTEMDLL_IMAGEHLP, IMAGEHLPDLL);
	}
	if ( DSymInitialize != NULL ){
		DSymInitialize(hProcess,NULL,TRUE);
		syminfo = (TAIL64(IMAGEHLP_SYMBOL) *)symblebuffer;
		syminfo->SizeOfStruct = STACKWALKSIZE;
		syminfo->MaxNameLength = STACKWALKSIZE - sizeof(TAIL64(IMAGEHLP_SYMBOL));
	}
	wsprintf(buf,T(FileProp_Version) T(" WinVer:%d.%d.%d\r\n[CPU]\r\n"),OSver.dwMajorVersion,OSver.dwMinorVersion,OSver.dwBuildNumber);
	ThCatString(&thText,buf);
	if ( DCreateToolhelp32Snapshot != NULL ){
		hSnapshot = DCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE,0);
	}else{
		hSnapshot = INVALID_HANDLE_VALUE;
	}
	if ( hSnapshot != INVALID_HANDLE_VALUE ){
		ThreadData.dwSize = sizeof(THREADENTRY32);
		if ( DThread32First(hSnapshot,&ThreadData) ) do {
			if ( ThreadData.th32OwnerProcessID == pid ){
				const TCHAR *tstr;
				HANDLE hThread;
				FILETIME CreateTime,ExitTime,KernelTime,UserTime;

				THREADSTRUCT *pT;
				DWORD a,b,c;

				tstr = T("?");
				a = b = c = 0;
				pT = ThreadBottom;
				for ( ; pT != NULL ; pT = pT->next ){
					if ( pT->ThreadID == ThreadData.th32ThreadID ){
						tstr = pT->ThreadName;
						a = pT->flag & 0x7f;
						b = (pT->flag >> 7) & 0x1ff;
						c = (pT->flag >> 16) & 0xffff;
						break;
					}
				}
				if ( DOpenThread != NULL ){
					hThread = DOpenThread(THREAD_QUERY_INFORMATION,
							FALSE,ThreadData.th32ThreadID);
					if ( hThread != NULL ){
						ULONG ulLength;
						PVOID pBeginAddress = NULL;

#define defThreadQuerySetWin32StartAddress 9
						if ( tstr[0] == '?' ){
							if ( DSymInitialize != NULL ){
								DNtQueryInformationThread(hThread,
									defThreadQuerySetWin32StartAddress,
									&pBeginAddress,sizeof(pBeginAddress),
									&ulLength);
								wsprintf(buf1,T("%x"),pBeginAddress);
								GetFailedModuleAddr(hProcess,syminfo,buf1,
									pBeginAddress);
								tstr = buf1;
							}
						}
						DGetThreadTimes(hThread,&CreateTime,&ExitTime,&KernelTime,&UserTime);
						CloseHandle(hThread);
					}
				}
				wsprintf(buf,T("%d(%s) Tflag:%x Ewait:%x Eflag:%x %u\r\n"),
					ThreadData.th32ThreadID,
					tstr,
					a,b,c,
					KernelTime.dwLowDateTime + UserTime.dwLowDateTime
				);
				ThCatString(&thText,buf);
			}
		} while ( DThread32Next(hSnapshot,&ThreadData) );

		ThCatString(&thText,T("\r\n[Modules]\r\n"));
		ModuleData.dwSize = sizeof(MODULEENTRY32X);
		if ( DModule32First(hSnapshot,&ModuleData) ) do {
			wsprintf(buf,T(PTRPRINTFORMAT) T(" %7x %s\r\n"),
					ModuleData.modBaseAddr,
					ModuleData.modBaseSize,
					ModuleData.szExePath
			);
			ThCatString(&thText,buf);
		} while ( DModule32Next(hSnapshot,&ModuleData) );
		CloseHandle(hSnapshot);
	}
	hEWnd = PPEui(NULL,T("PPx CPU info."),(TCHAR *)thText.bottom);
	ThFree(&thText);

	while ( IsWindow(hEWnd) ){
		if( (int)GetMessage(&msg,NULL,0,0) <= 0 ) break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

#if 0
DefineWinAPI(LONG,NtQueryInformationThread,(HANDLE,int,PVOID,ULONG,PULONG)) = NULL;

void GetThreadInfo(void)
{
	TCHAR buf[VFPS];
	ULONG ulLength;
	PVOID pBeginAddress = NULL;
	TAIL64T(IMAGEHLP_MODULE) mdlinfo;

	if ( DNtQueryInformationThread == NULL ){
		GETDLLPROC(GetModuleHandle(T("ntdll.dll")),NtQueryInformationThread);
		if ( DNtQueryInformationThread == NULL ) return;
	}
	if ( DSymInitialize == NULL ){
		if ( LoadSystemWinAPI(SYSTEMDLL_IMAGEHLP, IMAGEHLPDLL) == NULL ){
			return;
		}
		DSymInitialize(GetCurrentProcess(),NULL,TRUE);
	}

	DNtQueryInformationThread(GetCurrentThread(),
			defThreadQuerySetWin32StartAddress,&pBeginAddress,
			sizeof(pBeginAddress),&ulLength);

	if ( pBeginAddress != (PVOID)PPxUnhandledExceptionFilterMain ){
		mdlinfo.SizeOfStruct = sizeof(mdlinfo);
		if ( IsTrue(DSymGetModuleInfo(GetCurrentProcess(),(DWORD_PTR)pBeginAddress,&mdlinfo)) ){
			XMessage(NULL,NULL,XM_DbgLOG,T("New thread: ") T(PTRPRINTFORMAT) T(":") T(PTRPRINTFORMAT) T(" %s"),pBeginAddress,mdlinfo.BaseOfImage,mdlinfo.ModuleName);
		}
	}
}
#endif
