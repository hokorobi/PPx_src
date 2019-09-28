/*-----------------------------------------------------------------------------
	Paper Plane bUI													Sub
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <wincon.h>
#include <string.h>
#include "PPX.H"
#include "VFS.H"
#include "TCONSOLE.H"
#include "PPB.H"
#pragma hdrstop

DWORD_PTR USECDECL PPbExecInfoFunc(PPXAPPINFO *ppb, DWORD cmdID, PPXAPPINFOUNION *uptr);
void ExeProcess(const TCHAR *name, const TCHAR *path, int flag);
int ExecCommand(const TCHAR *ptr);
void PauseCommand(const TCHAR *message);

#if !NODLL
const TCHAR NilStr[] = T("");
const TCHAR ComSpecStr[] = T("ComSpec");
#else
extern const TCHAR NilStr[1];
extern const TCHAR ComSpecStr[];
#endif
const TCHAR ShellOptions[] = T(XEO_STRINGS);

//---------------------------------- PPCOMMON との通信
PPXAPPINFO ppbexecinfo = {(PPXAPPINFOFUNCTION)PPbExecInfoFunc, T("PPb"), RegID, NULL};

// 一行編集時に使用する
#pragma argsused
DWORD_PTR USECDECL PPbInfoFunc(PPXAPPINFO *ppb, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	UnUsedParam(ppb);
	switch(cmdID){
		case '1':
			tstrcpy(uptr->enums.buffer, EditPath);
			break;

		case PPXCMDID_PPBEXEC:
			ExeProcess(
					((PPXCMD_PPBEXEC *)uptr)->name,
					((PPXCMD_PPBEXEC *)uptr)->path,
					((PPXCMD_PPBEXEC *)uptr)->flag);
			return ExitCode;

		case PPXCMDID_PPBINSERTSEL:
			Replace(uptr->str, REPLACE_SELECT);
			return NO_ERROR;

		case PPXCMDID_PPBINSERT:
			Replace(uptr->str, 0);
			return NO_ERROR;

		case PPXCMDID_PPBREPLACE:
			Replace(uptr->str, REPLACE_ALL);
			return NO_ERROR;

		case PPXCMDID_PPBSELECTTEXT:
			tstrcpy(uptr->str, EditText + SelStart);
			*(uptr->str + (SelEnd - SelStart)) = '\0';
			return NO_ERROR;

		case PPXCMDID_PPBEDITTEXT:
			tstrcpy(uptr->str, EditText);
			return NO_ERROR;

		case PPXCMDID_PPXCOMMAD:
			CommonCommand(uptr->key);
			break;

		case PPXCMDID_COMMAND:{
			TCHAR *param;

			param = uptr->str + tstrlen(uptr->str) + 1;
			if ( tstrcmp(uptr->str, T("PAUSE")) == 0 ){
				PauseCommand(param);
				break;
			}
			return ERROR_INVALID_FUNCTION ^ 1;
		}

		case PPXCMDID_REQUIREKEYHOOK:
			KeyHookEntry = FUNCCAST(CALLBACKMODULEENTRY, uptr);
			break;

		case PPXCMDID_SETPOPLINE:
			ClearFormLine();
			tputstr(T("\r"));
			tputstr(uptr->str);
			break;

//		case PPXCMDID_POPUPPOS:
//			break;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

// ファイル実行時に使用する
#pragma argsused
DWORD_PTR USECDECL PPbExecInfoFunc(PPXAPPINFO *ppb, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	UnUsedParam(ppb);
	switch(cmdID){
		case '0':	// 自分自身へのパス
			CatPath(uptr->enums.buffer, PPxPath, NilStr);
			break;

		case '1':
			tstrcpy(uptr->enums.buffer, CurrentPath);
			break;

		case PPXCMDID_PPBEXEC:
			ExeProcess(
					((PPXCMD_PPBEXEC *)uptr)->name,
					((PPXCMD_PPBEXEC *)uptr)->path,
					((PPXCMD_PPBEXEC *)uptr)->flag);
			return ExitCode;

		case PPXCMDID_COMMAND:{
			TCHAR *param;

			param = uptr->str + tstrlen(uptr->str) + 1;
			if ( tstrcmp(uptr->str, T("PAUSE")) == 0 ){
				PauseCommand(param);
				break;
			}
			return ERROR_INVALID_FUNCTION ^ 1;
		}

		case PPXCMDID_REQUIREKEYHOOK:
			KeyHookEntry = FUNCCAST(CALLBACKMODULEENTRY, uptr);
			break;

		case PPXCMDID_SETPOPLINE:
			tputstr(uptr->str);
			break;

//		case PPXCMDID_POPUPPOS:
//			break;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

/*-----------------------------------------------------------------------------
	PPb の終了時に呼ぶ
-----------------------------------------------------------------------------*/
void ReleasePPB(void)
{
	if ( RegNo >= 0 ){
		PPxRegist(hMainWnd, RegID, PPXREGIST_FREE);
		if ( !IsIconic(hMainWnd) && IsWindowVisible(hMainWnd) ){
			GetWindowRect(hMainWnd, &WinPos.pos);
			SetCustTable(T("_WinPos"), RegCID, &WinPos, sizeof(WinPos));
		}
		CloseHandle(hCommSendEvent);	// 通信用イベントの解放
		CloseHandle(hCommIdleEvent);	// 通信用イベントの解放
		tRelease();					// コンソールライブラリの解除
	}
	PPxCommonCommand(NULL, 0, K_CLEANUP);
	CoUninitialize();
}
/*-----------------------------------------------------------------------------
	ウィンドウタイトルを登録する
-----------------------------------------------------------------------------*/
void TitleDisp(const TCHAR *addmes)
{
	TCHAR title[WNDTITLESIZE + VFPS + CMDLINESIZE + 4];

	if ( addmes == NULL ){
		wsprintf(title, T("%s%s"), WndTitle, CurrentPath);
	}else{
		wsprintf(title, T("%s%s - %s"), WndTitle, CurrentPath, addmes);
	}
										// Title を設定
	SetConsoleTitle(title);
}
#ifdef UNICODE
#define PipeCheck(str) tstrchr(str, '|')
#else
BOOL PipeCheck(const char *str)
{
	char type;

	for ( ;; ){
		type = *str;
		if ( type == '|' ) return TRUE;
		type = (char)Chrlen(type);
		if ( type == 0 ) return FALSE;
		str += type;
	}
}
#endif
/*-----------------------------------------------------------------------------
	コマンドライン実行処理
-----------------------------------------------------------------------------*/
BOOL ExeProcessShell(HWND hOwner, const TCHAR *exename, const TCHAR *param, const TCHAR *path, int flag)
{
	HANDLE hProcess;
	TCHAR reason[VFPS];

	if ( NULL != (hProcess = PPxShellExecute(hOwner, NULL, exename, param, path, flag, reason)) ){
		if ( flag & (XEO_WAITIDLE | XEO_SEQUENTIAL) ){
			if ( flag & XEO_WAITIDLE ) WaitForInputIdle(hProcess, INFINITE);
			if ( flag & XEO_SEQUENTIAL ){
				WaitForSingleObject(hProcess, INFINITE);
				GetExitCodeProcess(hProcess, &ExitCode);
			}
			CloseHandle(hProcess);
		}
		return TRUE;
	}
	tputstr(reason);
	tputstr(T("\n"));
	return FALSE;
}

void ExeProcess(const TCHAR *execarg, const TCHAR *path, int flags)
{
	TCHAR LineStaticBuf[CMDLINESIZE * 2], *LineBuf = LineStaticBuf;
	TCHAR *exeterm;
	size_t linelen;

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	const TCHAR *param;
	int exetype;
	DWORD createflags = CREATE_DEFAULT_ERROR_MODE;

	while ( (*execarg == ' ') || (*execarg == '\t') ) execarg++;
									// 種別に応じて実行する
	param = execarg;
	exetype = GetExecType(&param, LineStaticBuf, path);

	ExitCode = EXIT_SUCCESS;
	if ( exetype == GTYPE_SHELLEXEC ){	// ShellExecute で起動
		ExeProcessShell(NULL, LineStaticBuf, param, path, flags);
		return;
	}
	if ( !(flags & XEO_NOSCANEXE) && (CheckExebin(LineStaticBuf, exetype) == FALSE) ){
		return;
	}
									// LineStaticBuf で足りないならメモリ確保
	linelen = tstrlen(LineStaticBuf) + tstrlen(param) + MAX_PATH;
	if ( linelen > (CMDLINESIZE + MAX_PATH) ){
		LineBuf = HeapAlloc(GetProcessHeap(), 0, TSTROFF(linelen));
		if ( LineBuf == NULL ) return;
		tstrcpy(LineBuf, LineStaticBuf);
	}
	if ( exetype == GTYPE_CONSOLE ) setflag(flags, XEO_SEQUENTIAL);

	if ( !(flags & XEO_NOPIPERDIR) ){
		if ( PipeCheck(param) )   exetype = GTYPE_ERROR;
		if ( tstrchr(param, '<') ) exetype = GTYPE_ERROR;
		if ( tstrchr(param, '>') ) exetype = GTYPE_ERROR;
	}

	// unknown / data / cmd
	if ( (exetype == GTYPE_ERROR) || (exetype == GTYPE_DATA) ){
	#ifndef WINEGCC // unknown / data ... cmd 経由
		BOOL sepfix = FALSE;

		if ( (LineBuf[0] == '\"') && (tstrchr(param, '\"') != NULL) ){
			sepfix = TRUE;
		}
		if ( tstrlen(param) > 8000 ){ // CMD の長さ制限
			PPErrorMsg(LineStaticBuf, RPC_S_STRING_TOO_LONG);
			tputstr(LineStaticBuf);
			tputstr(T("\n"));
			goto fin;
		}
		// %COMSPEC% 取得
		if ( GetEnvironmentVariable(ComSpecStr, LineBuf, MAX_PATH) == 0 ){
			#ifdef UNICODE
				tstrcpy(LineBuf, T("CMD.EXE"));
			#else
				tstrcpy(LineBuf, T("COMMAND.COM"));
			#endif // UNICODE
		}
		exeterm = LineBuf + tstrlen(LineBuf);
		tstrcpy(exeterm, T(" /C "));
		// cmd は「"file name.bat" param」は成功し、「"file name.bat" "param"」
		// で失敗するので、「""file name.bat" "param"」に加工する
		if ( IsTrue(sepfix) ) tstrcat(LineBuf, T("\""));
		tstrcat(exeterm, execarg);
	#else // WINEGCC // system で実行する
		int result;

		tstrcpy(LineBuf, execarg);

		#ifdef UNICODE
		{
			char bufA[CMDLINESIZE * 4];

			UnicodeToUtf8(path, bufA, CMDLINESIZE * 4);
			chdir(bufA); // ● c: とか z: とかの変換をまだしていない
			SetCurrentDirectory(path);

			UnicodeToUtf8(LineBuf, bufA, CMDLINESIZE * 4);
			result = system(bufA);
		}
		#else
			chdir(path); // ● c: とか z: とかの変換をまだしていない
			SetCurrentDirectory(path);

			result = system(bufA);
		#endif
		if ( result < 0 ) tputstr(T("execute error.\n"));

		SetCurrentDirectory(PPxPath);
		goto fin;
		#endif
	}else{
		exeterm = LineBuf + tstrlen(LineBuf);
		if ( *param == ' ' ) param++;
		*exeterm = ' ';
		tstrcpy(exeterm + 1, param);
	}
											// 実行条件の指定
	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;
	si.wShowWindow	= SW_SHOWDEFAULT;

	if ( flags & (XEO_MAX | XEO_MIN | XEO_NOACTIVE | XEO_HIDE) ){
		si.dwFlags = STARTF_USESHOWWINDOW;
		if ( flags & XEO_MAX ) si.wShowWindow = SW_SHOWMAXIMIZED;
		if ( flags & XEO_MIN ) si.wShowWindow = SW_SHOWMINNOACTIVE;
		if ( flags & XEO_NOACTIVE ) si.wShowWindow = SW_SHOWNOACTIVATE;
		if ( flags & XEO_HIDE ){
			si.wShowWindow = SW_HIDE;
//			if ( !(flags & XEO_USECMD) ){ // リダイレクトするときは DETACHED 不可
//				setflag(createflags, DETACHED_PROCESS);
//			}
			setflag(createflags, DETACHED_PROCESS);
		}
	}
	if ( flags & XEO_LOW ) setflag(createflags, IDLE_PRIORITY_CLASS);

	if ( IsTrue(CreateProcess(NULL, LineBuf, NULL, NULL, FALSE,
			createflags, NULL, path, &si, &pi)) ){
		CloseHandle(pi.hThread);
		if ( flags & XEO_WAITIDLE ) WaitForInputIdle(pi.hProcess, INFINITE);
		if ( (exetype != GTYPE_GUI) || (flags & XEO_SEQUENTIAL) ){
			WaitForSingleObject(pi.hProcess, INFINITE);
			GetExitCodeProcess(pi.hProcess, &ExitCode);
		}
		CloseHandle(pi.hProcess);
	}else{
		ERRORCODE errorcode;

		param = LineBuf;
		if ( *param == '\"' ){ // 「"」 括りを除去
			param++;
			*(exeterm - 1) = '\0';
		}
		*exeterm = '\0';

		errorcode = GetLastError();
		if ( errorcode == ERROR_ELEVATION_REQUIRED ){ // UAC が必要
			ExeProcessShell(NULL, param, exeterm + 1, path, flags);
		}else{
			GetExecuteErrorReason(param, LineStaticBuf);
			tputstr(LineStaticBuf);
			tputstr(T("\n"));
		}
	}
fin:
	if ( LineBuf != LineStaticBuf ) HeapFree(GetProcessHeap(), 0, LineBuf);
}

void PauseCommand(const TCHAR *message)
{
	int press = 0;

	tInit(NULL);
	if ( (message != NULL) && (*message != '\0') ){
		tputstr(message);
		tputstr(T("\n"));
	}else{
		tputstr(MessageText(MES_KWAT));
	}
	for ( ; ; ){
		INPUT_RECORD con;
		DWORD read;

		ReadConsoleInput(hStdin, &con, 1, &read);
		if ( read == 0 ) continue;
		switch(con.EventType){
			case KEY_EVENT:
				if ( con.Event.KeyEvent.bKeyDown == FALSE ) continue;
				break;

			case MOUSE_EVENT:
				if ( con.Event.MouseEvent.dwButtonState != 0 ){
					press = 1;
					continue;
				}
				if ( !press ) continue;
				break;

			default:
				continue;
		}
		break;
	}
	tRelease();
}
/*-----------------------------------------------------------------------------
	コマンドラインを解析する
-----------------------------------------------------------------------------*/
void PPbExecuteInput(TCHAR *param, DWORD size)
{
	TCHAR *ptr, *next;
	int i;
	TCHAR buf[CMDLINESIZE * 2];

									// 計算式か？ ----------------------------
	if ( IsTrue(GetCalc(param, buf, &i)) ){
		ptr = param;
		if ( GTYPE_ERROR == GetExecType((const TCHAR **)&ptr, NULL, CurrentPath) ){
			tputstr(buf);
			tputstr(T("\n"));
			wsprintf(buf, T("%ld"), i);
			WriteHistory(PPXH_NUMBER, buf, 0, NULL);
			return;
		}
	}

	ptr = param;
	while ( (DWORD)(ptr - param) < size ){
		const TCHAR *tmpptr;

		SkipSpace((const TCHAR **)&ptr); // ---------------------- 先頭空白削除
		next = ptr;
									// --------------------------- 区切りの検索
		while ( !( (*next == 0) || (*next == 0xd) || (*next == 0xa) )) next++;
		if ( (*next == 0xd) || (*next == 0xa) ){
			*next = '\0';
			next++;
		}
		if ( *ptr == '\0' ) break;
									// --------------------------- コマンド解析
		tmpptr = ptr;
		GetLineParam(&tmpptr, buf);
		tstrupr(buf);
									// 内部コマンド ---------------------------
		if ( !tstrcmp(buf, T("CD")) ){
			ERRORCODE result;

			GetLineParam(&tmpptr, buf);
			VFSFixPath(NULL, buf, CurrentPath, VFSFIX_FULLPATH | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
			if ( (result = VFSChangeDirectory(NULL, buf)) == NO_ERROR ){
				GetCurrentDirectory(VFPS - 1, CurrentPath);
			}else{
				PPErrorMsg(buf, result);
				tputstr(buf);
				tputstr(T("\n"));
			}
			break;
		}
		if ( !tstrcmp(buf, T("PAUSE")) ){
			PauseCommand(tmpptr);
			break;
		}
									// --------------------------- 外部コマンド
		tRelease();
		tputstr(T("\n"));
		VFSChangeDirectory(NULL, CurrentPath);
		TitleDisp(ptr);
		ppbexecinfo.hWnd = hMainWnd;
		PP_ExtractMacro(hMainWnd, &ppbexecinfo, NULL, ptr, NULL, XEO_CONSOLE);
		GetCurrentDirectory(VFPS - 1, CurrentPath);
		ptr = next;
	}
}

/*-----------------------------------------------------------------------------
	受信コマンドを解析する
-----------------------------------------------------------------------------*/
void PPbExecuteRecv(TCHAR *param)
{
	switch( *param ){
		case '<':				// ログ出力 -------------------------------
			tputstr( param + 1 );
			tputstr(T("\n"));
			return;
		case '>':				// 受信コマンド ---------------------------
			if ( ExecCommand(param + 1) ) tputstr(T(" \n"));
			return;
		default:
			return;
	}
}

int ExecCommand(const TCHAR *ptr)
{
	switch( *ptr++ ){
		case 'L':		// >Lpath	Chdir -----------------------------
			VFSChangeDirectory(NULL, CurrentPath);
			VFSChangeDirectory(NULL, ptr);
			GetCurrentDirectory(VFPS - 1, CurrentPath);
			// '\0' へ
		case '\0':		// >	sync / NULL ---------------------------
			return 0; // 改行無し

		case 'p':		// >pcmd line	sHell(part) -------------------
			if ( LongParam.top == 0 ) ThCatString(&LongParam, T("H"));
			ThCatString(&LongParam, ptr);
			break;
		case 'P':		// >Pcmd line	sHell(last part)
			ThCatString(&LongParam, ptr);
			ExecCommand((TCHAR *)LongParam.bottom);
			ThFree(&LongParam);
			break;

		case 'H':{		// >H[option char][;HWND], [>*]cmdline	sHell -----
			int flags = 0;
			int i;
			TCHAR c;
			WORD atr = T_CYA;

			while ( (c = *ptr) != '\0' ){
				ptr++;
				if ( c == ';' ){
					hBackWnd = (HWND)GetNumber(&ptr);
				}
				if ( c == ',' ) break;
				for ( i = 0 ; ShellOptions[i] ; i++ ){
					if ( c == ShellOptions[i] ){
						setflag(flags, 1 << i);
						break;
					}
				}
			}
			if ( (*ptr == '>') && (*(ptr + 1) == '*') ){
				ppbexecinfo.hWnd = hMainWnd;
				PP_ExtractMacro(hMainWnd, &ppbexecinfo, NULL, ptr + 1, NULL, XEO_NOUSEPPB | XEO_CONSOLE);
				return 0;
			}

			tRelease();
			tputstr(T("\n"));

			if ( *ptr == '@' ){ // echo なし
				ptr++;
			}else{
				tputstr(CurrentPath);
				tputstr(T(">"));
				GetCustData(T("CB_com"), &atr, sizeof(atr));
				SetConsoleTextAttribute(hStdout, atr);
				tputstr(ptr);
				SetConsoleTextAttribute(hStdout, screen.wAttributes);
				tputstr(T("\n"));
			}

			SetCurrentDirectory(CurrentPath);
			TitleDisp(ptr);
			ExeProcess(ptr, CurrentPath, flags);
			GetCurrentDirectory(VFPS - 1, CurrentPath);
			if ( flags & XEO_WAITKEY ){
				PauseCommand(NilStr);
				tputstr(T("\n"));
			}
			break;
		}
		default:
			tputstr(ptr - 1);
			tputstr(MessageText(MES_UCMD));
	}
	return 1; // 改行あり
}
