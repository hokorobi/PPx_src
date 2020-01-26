/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer									Main
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <wincon.h>
#include "PPXVER.H"
#include "PPX.H"
#include "PPCUST.H"
#pragma hdrstop

UINT WM_PPXCOMMAND; // PPx 間通信用 Window Message
HINSTANCE hInst = NULL;

// カスタマイズ領域などを書式化する -------------------------------------------
void InfoDb(TCHAR *buf)
{
	PPXDBINFOSTRUCT dbinfo;
	TCHAR path[VFPS];

	dbinfo.structsize = sizeof dbinfo;
	dbinfo.custpath = path;
	GetPPxDBinfo(&dbinfo);

	wsprintf(buf,
		TNL T("Customize file:\t%s") TNL
		T("Customize area:\t%6d\tused:%6d\tfree:%6d") TNL
		T("History area:\t%6d\tused:%6d\tfree:%6d") TNL,
		path,
		dbinfo.custsize, dbinfo.custsize - dbinfo.custfree, dbinfo.custfree,
		dbinfo.histsize, dbinfo.histsize - dbinfo.histfree, dbinfo.histfree );
}
/*-----------------------------------------------------------------------------
	コンソール用一行表示
-----------------------------------------------------------------------------*/
void Print(const TCHAR *str)
{
	DWORD size;

#ifdef UNICODE
	HANDLE hStdOut = GetStdHandle(STD_ERROR_HANDLE);

	if ( WriteConsole(hStdOut, str, tstrlen32(str), &size, NULL) == FALSE ){
		WriteFile(hStdOut, str, TSTRLENGTH(str), &size, NULL);
	}
#else
	HANDLE hStdOut;

	hStdOut = GetStdHandle( (OSver.dwPlatformId == VER_PLATFORM_WIN32_NT) ?
		STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
	WriteFile(hStdOut, str, TSTRLENGTH(str), &size, NULL);
#endif
}

void ErrorPrint(void)
{
	TCHAR buf[VFPS];

	PPErrorMsg(buf, PPERROR_GETLASTERROR);
	Print(buf);
	Print(T(NL));
}
/*-----------------------------------------------------------------------------
	コンソール用 Yes/On 確認
	Yes:IDYES(6) No:IDNO(7)
-----------------------------------------------------------------------------*/
int USECDECL SurePrompt(const TCHAR *message)
{
	TCHAR buf[16];
	DWORD size;
	HANDLE hStdin;

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	Print(message);
	Print(T(" [Yes/No]"));
	SetConsoleMode(hStdin, 0);
	for ( ;; ){
		buf[0] = 0;
		ReadConsole(hStdin, buf, 1, &size, NULL);
		if ( size ){
			if ( (buf[0] == 'Y') || (buf[0] == 'y') ){
				Print(T(" Yes.") TNL);
				return IDYES;
			}
			if ( (buf[0] == 'N') || (buf[0] == 'n') ){
				Print(T(" No.") TNL);
				return IDNO;
			}
		}
	}
}

HANDLE OpenDumpTarget(const TCHAR *filename)
{
	HANDLE hFile;

	if ( filename[0] == '\0' ){
		hFile = GetStdHandle(STD_OUTPUT_HANDLE);
	}else{
		hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
				FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ){
			ErrorPrint();
			return NULL;
		}
	}
	return hFile;
}

DWORD ConvertText(TCHAR **text)
{
	DWORD size, offset = 0;
	UINT codepage = 0;
	char *newtext;
	WCHAR *srcW;

	GetCustData(T("X_ccode"), &codepage, sizeof(codepage));
	if ( codepage == 0 ) goto noconvert;
	if ( codepage == 1 ){
		codepage = CP_ACP;
	}else if ( codepage == 2 ){
		codepage = CP_UTF8;
		offset = 3;
	}else if ( codepage == GetACP() ){
		codepage = CP_ACP;
	}
	// UNICODE text を用意
	#ifdef UNICODE
		srcW = *text;
		if ( offset == 0 ){
			srcW++; // BOM をスキップ
		}else{
			offset = 0;
		}
	#else
		if ( codepage == CP_ACP ) goto noconvert;

		size = MultiByteToWideChar(CP_ACP, 0, *text, -1, NULL, 0);
		srcW = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));
		if ( srcW == NULL ) goto noconvert;
		MultiByteToWideChar(CP_ACP, 0, *text, -1, srcW, size);
		HeapFree(GetProcessHeap(), 0, *text);
		*text = (char *)srcW;
	#endif
	// 指定コードページに変換
	size = WideCharToMultiByte(codepage, 0, srcW + 1, -1, NULL, 0, NULL, NULL) + offset;
	newtext = HeapAlloc(GetProcessHeap(), 0, size * sizeof(char));
	if ( newtext == NULL ) goto noconvert;
	WideCharToMultiByte(codepage, 0, srcW, -1, newtext + offset, size, NULL, NULL);
	#ifndef UNICODE // UNICODE版は予めBOMがついている
	if ( offset ){
		newtext[0] = '\xef';
		newtext[1] = '\xbb';
		newtext[2] = '\xbf';
	}
	#endif
	HeapFree(GetProcessHeap(), 0, *text);
	*text = (TCHAR *)newtext;
	return strlen32(newtext);

noconvert: ;
	return TSTRLENGTH(*text);
}

//-----------------------------------------------------------------------------
int CustDump(const TCHAR *cmdptr, const TCHAR *filename)
{
	TCHAR *ptr, buf[CMDLINESIZE], *mask = NULL, code, *more;
	HANDLE hFile;
	DWORD size, wsize;
	int result = EXIT_SUCCESS;

	while( '\0' != (code = GetOptionParameter(&cmdptr, buf, CONSTCAST(TCHAR **, &more))) ){
		if ( code == '-' ){
			if ( !tstrcmp( buf + 1, T("MASK")) ){
				mask = more;
			}
		}
	}

	PPxSendMessage(WM_PPXCOMMAND, K_Scust, 0);
	ptr = PPcust(
			(mask == NULL ) ? PPXCUSTMODE_DUMP_ALL : PPXCUSTMODE_DUMP_PART,
			mask);

	hFile = OpenDumpTarget(filename);
	if ( hFile == NULL ){
		result = EXIT_FAILURE;
	}else{
		TCHAR *writeptr;

		#ifdef UNICODE
		DWORD handletype;

		handletype = GetFileType(hFile);
		if ( handletype != FILE_TYPE_CHAR ){
		#endif
			size = ConvertText(&ptr);
			writeptr = ptr;
			while ( size ){
				if ( WriteFile(hFile, writeptr, (size > 0x4000) ? 0x4000 : size, &wsize, NULL) == FALSE ){
					ErrorPrint();
					result = EXIT_FAILURE;
					break;
				}
				size -= wsize;
				writeptr += wsize / sizeof(TCHAR);
			}
		#ifdef UNICODE
		}else{
			WriteConsole(hFile, ptr, strlenW(ptr), &size, NULL);
		}
		#endif
		CloseHandle(hFile);
	}
	HeapFree(GetProcessHeap(), 0, ptr);
	Print(T("Done."));
	InfoDb(buf);
	Print(buf);
	return result;
}
//-----------------------------------------------------------------------------
int CustStore(TCHAR *filename, int appendmode, HWND hDlg)
{
	TCHAR *mem, *text, *maxptr;		// カスタマイズ解析位置
	TCHAR *log;
	int result;

	PPxSendMessage(WM_PPXCOMMAND, K_Scust, 0);
										// ファイル読み込み処理 ---------------
	if ( filename[0] == '\0' ) tstrcpy(filename, T("con")); // 第2
//	if ( filename[0] == '\0' ) tstrcpy(filename, T("<")); // 第2


	if ( LoadTextImage(filename, &mem, &text, &maxptr) != NO_ERROR ){
		ErrorPrint();
		return EXIT_FAILURE;
	}
										// 解析・格納 -------------------------
	if ( appendmode == PPXCUSTMODE_temp_RESTORE ){
		appendmode = PPXCUSTMODE_STORE;
	}else{
		Print(T("Customize data storing...") TNL);
	}
	result = PPcustCStore(text, maxptr, appendmode, &log, SurePrompt);
	HeapFree(GetProcessHeap(), 0, mem);
										// 結果表示 ---------------------------
	if ( hDlg != NULL ){
		SetDlgItemText(hDlg, IDS_INFO, (log != NULL) ? log : T("Customize done.") );
	}
	if ( log != NULL ){
		Print(log);
		HeapFree(GetProcessHeap(), 0, log);
	}
	if ( result != 0 ){
		TCHAR buf[VFPS + 0x100];

		Print(T("Done."));
		InfoDb(buf);
		Print(buf);
		if ( !(result & PPCUSTRESULT_RELOAD) ){
			PPxPostMessage(WM_PPXCOMMAND, K_Lcust, GetTickCount());
		}
		return EXIT_SUCCESS;
	}else{
		Print(T("Abort.") TNL);
		return EXIT_FAILURE;
	}
}
//-----------------------------------------------------------------------------
int CustInit(void)
{
	InitCust();
	Print(T("Customize data initialized") TNL);
	return EXIT_SUCCESS;
}
//-----------------------------------------------------------------------------
int HistDump(const TCHAR *cmdptr, const TCHAR *filename)
{
	UTCHAR code;
	TCHAR buf[CMDLINESIZE];
	HANDLE hFile;
	const TCHAR *more;
	WORD historytype = PPXH_ALL_R;
	int format = -1;
	UINT codepage = 0;
	DWORD size;

	GetCustData(T("X_ccode"), &codepage, sizeof(codepage));
	while( '\0' != (code = GetOptionParameter(&cmdptr, buf, CONSTCAST(TCHAR **, &more))) ){
		if ( code == '-' ){
			if ( !tstrcmp( buf + 1, T("MASK")) ){
				historytype = GetHistoryType(&more);
			}else if ( !tstrcmp( buf + 1, T("FORMAT")) ){
				format = GetNumber(&more);
			}
		}
	}

	hFile = OpenDumpTarget(filename);
	if ( hFile == NULL ){
		return EXIT_FAILURE;
	}else{
		int count = 1;
		int result = EXIT_SUCCESS;

		#ifdef UNICODE
		DWORD handletype;

		handletype = GetFileType(hFile);

		if ( handletype != FILE_TYPE_CHAR ){
			if ( codepage == 0 ){
				WriteFile(hFile, UCF2HEADER, UCF2HEADERSIZE, &size, NULL);
			}
			if ( codepage == 2 ){
				WriteFile(hFile, UTF8HEADER, UTF8HEADERSIZE, &size, NULL);
				codepage = CP_UTF8;
			}
		}
		#endif
		if ( format < 0 ) Print(T("History data dumping...") TNL);

		for ( ;; ){
			TCHAR buf[0x10010];
			const TCHAR *p;

			UsePPx();
			p = EnumHistory(historytype, count);
			if ( p == NULL ) break;
			switch ( format ){
				case 0:
					wsprintf(buf, T("%s") TNL, p);
					break;

				case 2:
					wsprintf(buf, T("%04X,%d,,%s") TNL,
						*(WORD *)((BYTE *)p - 4), // ヒストリ種類
						*(WORD *)((BYTE *)p - 6) - *(WORD *)((BYTE *)p - 2) - 6, // 追加情報サイズ
						p);
					break;

				default:
					wsprintf(buf, T("%04X,%s") TNL, *(WORD *)((BYTE *)p - 4), p);
					break;
			}
			FreePPx();
			#ifdef UNICODE
			if ( handletype == FILE_TYPE_CHAR ){
				WriteConsole(hFile, buf, strlenW(buf), &size, NULL);
			}else if ( codepage != 0 ){
				char bufA[0x10010];

				size = WideCharToMultiByte(codepage, 0, buf, -1, bufA, sizeof bufA, NULL, NULL);
				if ( size ) size--;
				if ( WriteFile(hFile, bufA, size, (DWORD *)&size, NULL) == FALSE ){
					ErrorPrint();
					result = -1;
					break;
				}
			}else
			#endif
			if ( WriteFile(hFile, buf, TSTRLENGTH(buf), (DWORD *)&size, NULL) == FALSE ){
				ErrorPrint();
				result = EXIT_FAILURE;
				break;
			}
			count++;
		}
		FreePPx();
		CloseHandle(hFile);
		if ( format < 0 ) Print(T("Done.") TNL);
		return result;
	}
}
//-----------------------------------------------------------------------------
int HistInit(void)
{
	InitHistory();
	Print(T("History initialized") TNL);
	return EXIT_SUCCESS;
}
/*-----------------------------------------------------------------------------
	メイン
-----------------------------------------------------------------------------*/
const TCHAR Title[] = T("PPx Customizer  ") TCopyright TNL;
// スタートアップコードを簡略にするため、void 指定
int USECDECL main(void)
{
	const TCHAR *cmdptr, *cmdp2, *more, *ptr;
	TCHAR Command[0x200], Param[VFPS], FileName[VFPS];
	UTCHAR code;

#if NODLL
	InitCommonDll(GetModuleHandle(NULL));
#endif
#ifndef UNICODE
	OSver.dwOSVersionInfoSize = sizeof(OSver);
	GetVersionEx(&OSver);
#endif
	FixCharlengthTable((char *)T_CHRTYPE);
	WM_PPXCOMMAND = RegisterWindowMessageA(PPXCOMMAND_WM);

	cmdptr = GetCommandLine();
	GetLineParam(&cmdptr, Command); // 自身のパス
	if ( GetLineParam(&cmdptr, Command) == '\0' ){ // 第1
		Print(Title);
		GUIcustomizer(0, NULL);
		return EXIT_SUCCESS;
	}
	if ( (*Command == '/') || (*Command == '-') ){
		if ( *(Command + 1) == '?' ){
			Print(Title);
			Print(
				T("Usage:") T(PPCUSTEXE) T(" [command [filename]]") TNL
				T("  command: Customize  History") TNL
				T("    ------------------------") TNL
				T("    Dump     CD        HD") TNL
				T("    Append   CA        --") TNL
				T("    Store    CS        --") TNL
				T("    Update   CU        --") TNL
				T("    Init.   CINIT     HINIT") TNL
				T("    Size    CSIZE     HSIZE") TNL);
			return EXIT_FAILURE;
		}else if ( Isdigit(*(Command + 1)) ){ // -n 指定ページを開く
			GUIcustomizer(*(Command + 1) - '0', NULL);
			return EXIT_SUCCESS;
		}else if ( *(Command + 1) == ':' ){ // -: PPcエントリ書式
			GUIcustomizer(-1, Command + 2);
			return EXIT_SUCCESS;
		}else if ( *(Command + 1) == 'e' ){ // -edit 編集エディタ
			PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_TEXTCUST),
					GetForegroundWindow(), TextCustomizeDlgBox, 0);
			return EXIT_SUCCESS;
		}else if ( *(Command + 1) == 'c' ){ // -c コマンド実行ツリー
			PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CMDTREE),
					GetForegroundWindow(), CommandTreeDlgBox, 0);
			return EXIT_SUCCESS;
		}
	}

	FileName[0] = '\0';
	cmdp2 = cmdptr;
	while( '\0' != (code = GetOptionParameter(&cmdp2, Param, CONSTCAST(TCHAR **, &more))) ){
		if ( code != '-' ){
			tstrcpy(FileName, Param);
			continue;
		}
	}
//-------------------------------------------------------------- Customize Dump
	if ( !tstricmp(Command, T("CD")) || !tstricmp(Command, T("DC")) ){
		Print(T("Customize data dumping...") TNL);
		return CustDump(cmdptr, FileName);
	}
//------------------------------------------------------------- Customize Store
	if ( !tstricmp(Command, T("CS")) || !tstricmp(Command, T("SC")) ){
		return CustStore(FileName, PPXCUSTMODE_STORE, NULL);
	}
//------------------------------------------------------------ Customize Append
	if ( !tstricmp(Command, T("CA")) || !tstricmp(Command, T("AC")) ){
		return CustStore(FileName, PPXCUSTMODE_APPEND, NULL);
	}
//------------------------------------------------------------- Customize Store
	if ( !tstricmp(Command, T("CU")) ){
		return CustStore(FileName, PPXCUSTMODE_UPDATE, NULL);
	}
//------------------------------------------------------------- Customize Init.
	if ( !tstricmp(Command, T("CINIT")) ){
		CustInit();
		return EXIT_SUCCESS;
	}
//------------------------------------------------------------- Customize size
	if ( !tstricmp(Command, T("CSIZE")) ){
		ptr = FileName;
		SetPPxDBsize(PPXDB_CUSTOMIZE, GetNumber(&ptr));
		return EXIT_SUCCESS;
	}
//--------------------------------------------------------------- History Dump
	if ( !tstricmp(Command, T("HD")) ){
		return HistDump(cmdptr, FileName);
	}
//--------------------------------------------------------------- History Init.
	if ( !tstricmp(Command, T("HINIT")) ){
		HistInit();
		return EXIT_SUCCESS;
	}
//------------------------------------------------------------- History size
	if ( !tstricmp(Command, T("HSIZE")) ){
		ptr = FileName;
		SetPPxDBsize(PPXDB_HISTORY, GetNumber(&ptr));
		return EXIT_SUCCESS;
	}
//----------------------------------------------------------------------- Error
	Print(T("Unknown command") TNL);
	return EXIT_FAILURE;
}
