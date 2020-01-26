/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer		テキスト形式ファイル操作シート
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#pragma hdrstop

#define DumpMode PPXCUSTMODE_temp_RESTORE
#define HistMode (PPXCUSTMODE_temp_RESTORE-1)

TCHAR CustName[VFPS] = T("PPX.CFG");
TCHAR HistName[VFPS] = T("PPXHIST.CFG");
const TCHAR RegPath[] = T("Software\\Classes\\Folder\\shell\\PPc\\command");
const TCHAR RegOpen[] = T("Software\\Classes\\Folder\\shell");
const TCHAR RegPPc[] = T("PPc");
const TCHAR RegQ[] =
	T("フォルダのコンテキストメニューにPPcを登録しますか？\n")
	T("※レジストリに書き込みます。\n")
	T("※Windows95,98,Me,NT,2000はごみ箱等の特殊フォルダでは使えません。\0")
	T("Register PPc to context menu?\n");
#if !NODLL
const TCHAR NilStr[1] = T("");
TCHAR NilStrNC[1] = T("");
#endif
OSVERSIONINFO OSver;

BOOL CheckOpen(void);

const TCHAR ClipTextStr[] = T("*cliptext ");
#define TEXTCUSTSIZE 0x7fff
TCHAR TextCustBackup[VFPS];
BOOL TextCustModify;

void SetButtonState(HWND hDlg)
{
	BOOL usereg = FALSE, useopen;
	HKEY HK;

	if ( RegOpenKeyEx(HKEY_CURRENT_USER, RegPath, 0, KEY_READ, &HK) ==
			ERROR_SUCCESS ){
		usereg = TRUE;
		RegCloseKey(HK);
	}else if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPath, 0, KEY_READ, &HK) ==
			ERROR_SUCCESS ){
		usereg = TRUE;
		RegCloseKey(HK);
	}
	useopen = CheckOpen();
	EnableDlgWindow(hDlg, IDB_REGPPC, !usereg && !useopen);
	EnableDlgWindow(hDlg, IDB_REGOPEN, !useopen);
	EnableDlgWindow(hDlg, IDB_UNREGPPC, usereg);
}

void USEFASTCALL InitFileDialog(HWND hDlg)
{
	TCHAR buf[VFPS + 0x100];

	SendDlgItemMessage(hDlg, IDS_INFO,
			WM_GETTEXT, (WPARAM)TSIZEOF(buf), (LPARAM)buf);
	InfoDb(buf + tstrlen(buf));
	SendDlgItemMessage(hDlg, IDS_INFO, WM_SETTEXT, 0, (LPARAM)buf);
	SetButtonState(hDlg);
	CheckDlgButton(hDlg, IDX_CONSOLE, X_chidc);
	if ( hConsoleWnd == NULL ){
		ShowDlgWindow(hDlg, IDX_CONSOLE, FALSE);
	}
}

BOOL TextCust(HWND hDlg)
{
	TCHAR text[TEXTCUSTSIZE];
	TCHAR *log = NULL;
	BOOL result = TRUE;

	GetDlgItemText(hDlg, IDE_CUSTEDIT, text, TEXTCUSTSIZE);
	PPcustCStore(text, text + tstrlen(text),
			SendDlgItemMessage(hDlg, IDX_APPEND, BM_GETCHECK, 0, 0) ?
				PPXCUSTMODE_APPEND : PPXCUSTMODE_STORE, &log, NULL);
	if ( log && (tstrstr(log, T("Error")) != NULL) ) result = FALSE;
	SetDlgItemText(hDlg, IDE_CUSTLOG,
			((log != NULL) && (*log != '\r')) ?
				log : GetCText(T("設定完了。\0no error.")) );
	if ( log != NULL ) HeapFree(GetProcessHeap(), 0, log);
	PPxPostMessage(WM_PPXCOMMAND, K_Lcust, GetTickCount());
	return result;
}

#pragma argsused
INT_PTR CALLBACK TextCustomizeDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UnUsedParam(lParam);

	switch(iMsg){
		case WM_INITDIALOG: {
			HWND hEdwnd;

			PPxSendMessage(WM_PPXCOMMAND, K_Scust, 0);
			MakeTempEntry(VFPS, TextCustBackup, FILE_ATTRIBUTE_NORMAL);
			CustDump(NilStr, TextCustBackup );
			TextCustModify = FALSE;
			CheckDlgButton(hDlg, IDX_APPEND, TRUE);

			hEdwnd = GetDlgItem(hDlg, IDE_CUSTEDIT);
			PPxRegistExEdit(NULL, hEdwnd, TEXTCUSTSIZE - 10, NULL,
					PPXH_GENERAL,0, PPXEDIT_USEALT | PPXEDIT_NOWORDBREAK);

			if ( IsTrue(OpenClipboard(hDlg)) ){
				HGLOBAL hG;

				hG = GetClipboardData(CF_TTEXT);
				if ( hG != NULL ){
					TCHAR *src;

					src = GlobalLock(hG);
					if ( tstrchr(src, '=') != NULL ){
						SetWindowText(hEdwnd, src);
					}
					GlobalUnlock(hG);
				}
				CloseClipboard();
			}
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_TEST:
					TextCust(hDlg);
					TextCustModify = TRUE;
					break;

				case IDOK:
					TextCustModify = TRUE;
					if ( TextCust(hDlg) == FALSE ) break;
					DeleteFile(TextCustBackup);
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					if ( IsTrue(TextCustModify) ){
						InitCust();
						CustStore(TextCustBackup, PPXCUSTMODE_temp_RESTORE, NULL);
					}
					DeleteFile(TextCustBackup);
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

void CustMain(HWND hDlg, int mode)
{
	int result = 0;
	OPENFILENAME ofile = { sizeof(ofile), NULL, NULL, GetFileExtsStr, NULL,
		0, 0, CustName, TSIZEOF(CustName), NULL, 0, NULL, NULL,
		OFN_HIDEREADONLY | OFN_SHAREAWARE,
		0, 0, NilStr, 0, NULL, NULL OPENFILEEXTDEFINE };
	ofile.hwndOwner = hDlg;
	if ( mode == DumpMode ){
		ofile.lpstrTitle = MessageText(MES_SDPF);
		if ( GetSaveFileName(&ofile) ) result = CustDump(NilStr, CustName);
	}else if ( mode == HistMode ){
		ofile.lpstrFile = HistName;
		ofile.lpstrTitle = MessageText(MES_SDPF);
		if ( GetSaveFileName(&ofile) ) result = HistDump(NilStr, HistName);
	}else{
		if ( GetOpenFileName(&ofile) ) result = CustStore(CustName, mode, hDlg);
	}
	if ( result ){
		XMessage(hDlg, NULL, XM_GrERRld, GetCText(T("失敗しました。\nコンソールに詳細が出力されています。\0Check report log.")));
	}
	return;
}

// 「開く」の初期値を設定
void RegisterOpenContext(BOOL reg)
{
	HKEY HK;

	if ( RegOpenKeyEx((OSver.dwMajorVersion >= 5) ?
				HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, RegOpen, 0,
				KEY_SET_VALUE, &HK) == ERROR_SUCCESS ){
		if ( reg ){
			RegSetValueEx(HK, NilStr, 0, REG_SZ, (LPBYTE)RegPPc, sizeof(RegPPc));
		}else{
			RegSetValueEx(HK, NilStr, 0, REG_SZ, (LPBYTE)NilStr, sizeof(NilStr));
		}
		RegCloseKey(HK);
	}
}

#define RegisterContextDepth 3
void DeleteRegisterContext(_In_ HKEY hk)
{
	TCHAR buf[VFPS];
	int depth;

	tstrcpy(buf, RegPath);
	depth = RegisterContextDepth;
	// HKEY_LOCAL_MACHINE で Folderは削除禁止
	if ( hk == HKEY_LOCAL_MACHINE ) depth--;
	for ( ; depth > 0 ; depth-- ){
		RegDeleteKey(hk, buf);
		*tstrrchr(buf, '\\') = '\0';
	}
}

void RegCreatePath(HKEY hk, int maxdepth)
{
	TCHAR buf[VFPS];
	int depth;
	HKEY newhk;
	DWORD tmp;

	tstrcpy(buf, RegPath);
	for ( depth = 0 ; depth < maxdepth ; depth++ ){
		*VFSFindLastEntry(buf) = '\0';
	}
	if ( RegCreateKeyEx(hk, buf, 0, NilStrNC, REG_OPTION_NON_VOLATILE,
				KEY_ALL_ACCESS, NULL, &newhk, &tmp) == ERROR_SUCCESS ){
		if ( maxdepth ){
			RegCreatePath(hk, maxdepth - 1);
		}else{
			GetModuleFileName(NULL, buf, VFPS);
			tstrcpy(VFSFindLastEntry(buf) + 1, T(PPCEXE));
			tstrcat(buf,
				((OSver.dwMajorVersion > 5) ||
				 ((OSver.dwMajorVersion == 5) && (OSver.dwMinorVersion > 0))) ?
				T(" /idl:%I %L") : T(" \"%1\""));
			RegSetValueEx(newhk, NilStr, 0, REG_SZ, (LPBYTE)buf, TSTRSIZE32(buf));
		}
		RegCloseKey(newhk);
	}
}

// PPcを「開く」選択肢に追加する
BOOL RegisterContext(HWND hDlg)
{
	if ( PMessageBox(hDlg, GetCText(RegQ), StrCustTitle,
				MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES ){
		return FALSE;
	}
	RegCreatePath((OSver.dwMajorVersion >= 5) ?
			HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, RegisterContextDepth);
	return TRUE;
}

void DeleteOpenRegisterContext(void)
{
	if ( CheckOpen() == FALSE ) return;
	RegisterOpenContext(FALSE);	// フォルダの初期アクションを消去
						// (RegDeleteValue(HKEY_LOCAL_MACHINE)で消えない時用)
	RegDeleteValue(HKEY_CURRENT_USER, RegOpen);
	RegDeleteValue(HKEY_LOCAL_MACHINE, RegOpen);
}

BOOL CheckOpen(void)
{
	TCHAR data[VFPS];

	data[0] = '\0';
	GetRegString(HKEY_CURRENT_USER, RegOpen, NilStr, data, TSIZEOF(data));
	if ( data[0] == '\0' ){
		GetRegString(HKEY_LOCAL_MACHINE, RegOpen, NilStr, data, TSIZEOF(data));
	}
	return !tstrcmp(data, T("PPc"));
}

void USEFASTCALL KeyInputTest(HWND hDlg)
{
	TCHAR temp[0x80];

	tstrcpy(temp, ClipTextStr);
	if ( KeyInput(GetParent(hDlg), temp + TSIZEOF(ClipTextStr) - 1) <= 0 ){
		return;
	}
	PP_ExtractMacro(hDlg, NULL, NULL, temp, NULL, 0);
}

#pragma argsused
INT_PTR CALLBACK FilePage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_CS:
					CustMain(hDlg, PPXCUSTMODE_STORE);
					break;
				case IDB_CA:
					CustMain(hDlg, PPXCUSTMODE_APPEND);
					break;
				case IDB_CU:
					CustMain(hDlg, PPXCUSTMODE_UPDATE);
					break;
				case IDB_TEXTCUST:
					PPxDialogBoxParam(hInst,
							MAKEINTRESOURCE((UseLcid == LCID_PPXDEF) ?
								IDD_TEXTCUST : IDD_TEXTCUSTE),
							hDlg, TextCustomizeDlgBox, 0);
					break;

				case IDB_CD:
					CustMain(hDlg, DumpMode);
					break;
				case IDB_CINIT:
					if ( PMessageBox(hDlg, MES_QICU, StrCustTitle,
							MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)
																 == IDYES ){
						CustInit();
					}
					break;
				case IDB_HD:
					CustMain(hDlg, HistMode);
					break;
				case IDB_HINIT:
					if ( PMessageBox(hDlg, MES_QIHI, StrCustTitle,
							MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)
																 == IDYES ){
						HistInit();
					}
					break;

				case IDB_TESTKEY:
					KeyInputTest(hDlg);
					break;

				case IDB_REGPPC:
					RegisterContext(hDlg);
					DeleteOpenRegisterContext();
					SetButtonState(hDlg);
					break;

				case IDB_REGOPEN:
					if ( IsTrue(RegisterContext(hDlg)) ){
						RegisterOpenContext(TRUE);
					}
					SetButtonState(hDlg);
					break;

				case IDB_UNREGPPC:
					DeleteOpenRegisterContext();
					DeleteRegisterContext(HKEY_CURRENT_USER);
					DeleteRegisterContext(HKEY_LOCAL_MACHINE);
					SetButtonState(hDlg);
					break;

				case IDX_CONSOLE:
					if ( hConsoleWnd == NULL ) break;
					if ( SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) ){
						X_chidc = TRUE;
						ShowWindow(hConsoleWnd, SW_HIDE);
					}else{
						X_chidc = FALSE;
						ShowWindow(hConsoleWnd, SW_SHOW);
					}
					SetCustData(T("X_chidc"), &X_chidc, sizeof(X_chidc));
					break;
			}
			break;

		case WM_INITDIALOG:
			InitFileDialog(hDlg);
			break;

		case WM_NOTIFY:
			if ( ((NMHDR *)lParam)->code == PSN_SETACTIVE ){
				InitWndIcon(hDlg, IDS_INFO);
			}
		// default へ
		default:
			return StyleDlgProc(hDlg, msg, IDD_INFO, lParam);
	}
	return TRUE;
}
