/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		PPxCommonCommand
-----------------------------------------------------------------------------*/
#define ONPPXDLL // PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#pragma hdrstop

HHOOK hAutoDDDLL = NULL;

struct cmdstrstruct {
	TCHAR precode;
	TCHAR str[22];
} NewPackCmd = {0x9f, T("*pack \"%2%\\|%X|\" %Or-")};

DefineWinAPI(BOOL, EnableNonClientDpiScaling, (HWND)) = INVALID_VALUE(impEnableNonClientDpiScaling);
DefineWinAPI(BOOL, GetLayeredWindowAttributes, (HWND hwnd, COLORREF *crKey, BYTE *bAlpha, DWORD *dwFlags)) = NULL;
DefineWinAPI(BOOL, SetLayeredWindowAttributes, (HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)) = NULL;

void ChangeOpaqueWindow(HWND hWnd, int opaque)
{
	HWND hTargetWnd = GetCaptionWindow(hWnd);
	COLORREF crKey;
	BYTE bAlpha;
	DWORD dwFlags;
	DWORD exstyle;

	if ( DSetLayeredWindowAttributes == NULL ){
		HMODULE hUser32;

		hUser32 = GetModuleHandleA(User32DLL);
		GETDLLPROC(hUser32, GetLayeredWindowAttributes);
		GETDLLPROC(hUser32, SetLayeredWindowAttributes);
		if ( DSetLayeredWindowAttributes == NULL ) return;
	}
	exstyle = GetWindowLong(hTargetWnd, GWL_EXSTYLE);

	if ( !(exstyle & WS_EX_LAYERED) ){
		bAlpha = 0xff;
	}else{
		if ( DGetLayeredWindowAttributes(hTargetWnd, &crKey, &bAlpha, &dwFlags) == FALSE ){
			return;
		}
	}

	opaque = (opaque * 16) + (int)(DWORD)bAlpha;
	if ( opaque < 255 ){
		if ( opaque < 15 ) opaque = 15;
		if ( opaque == (int)(DWORD)bAlpha ) return;
		if ( !(exstyle & WS_EX_LAYERED) ){
			SetWindowLong(hTargetWnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
		}
		DSetLayeredWindowAttributes(hTargetWnd, 0, (BYTE)opaque, LWA_ALPHA);
	}else{
		if ( !(exstyle & WS_EX_LAYERED) ) return;
		SetWindowLong(hTargetWnd, GWL_EXSTYLE, exstyle & ~WS_EX_LAYERED);
	}
}

void EnableNcScale(HWND hWnd)
{
	if ( DEnableNonClientDpiScaling == NULL ) return;
	if ( DEnableNonClientDpiScaling == INVALID_VALUE(impEnableNonClientDpiScaling) ){
		GETDLLPROC(GetModuleHandleA(User32DLL), EnableNonClientDpiScaling);
		if ( DEnableNonClientDpiScaling == NULL ) return;
	}

	DEnableNonClientDpiScaling(hWnd);
}

void Get_X_save_widthUI(TCHAR *path)
{
	BOOL mkdir = FALSE;

	path[0] = '\0';
	if ( NO_ERROR != GetCustData(T("X_save"), path, TSTROFF(VFPS) ) ){
		tstrcpy(path, DLLpath);
		tInput(NULL, MES_QBAK, path, VFPS, PPXH_DIR_R, PPXH_DIR);
		SetCustData(T("X_save"), path, TSTRSIZE(path));
		mkdir = TRUE;
	}
	VFSFixPath(NULL, path, DLLpath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
	if ( IsTrue(mkdir) ) MakeDirectories(path, NULL);
}

// バージョンアップ処理。GUIが登録されるタイミングで行う。
// ※メッセージループが回らないと、PPeのメッセージが処理されないから(..PPbが×)
// ※FixCharlengthTableでやってもいいかも
void AutoCustomizeUpdate(void)
{
	DWORD X_upm = 0;
	TCHAR oldver[64];

						// 例外処理ハンドラを登録し直して優先順位を変更する
	#ifndef _WIN64
	if ( DRemoveVectoredExceptionHandler != NULL )
	#endif
	{
		DRemoveVectoredExceptionHandler(UEFvec);
		UEFvec = DAddVectoredExceptionHandler(0, PPxUnhandledExceptionFilter);
	}
										// カスタマイズ領域のバージョンチェック
	tstrcpy(oldver, T("0.00"));
	GetCustData(T("PPxCFG"), &oldver, sizeof(oldver));

	// ●↓ver1.02で 102 にしてしまった対策
	if ( (oldver[1] == '.') && (tstrcmp(T(FileCfg_Version), oldver) <= 0) ) return; // 最新だった

	GetCustData(T("X_upm"), &X_upm, sizeof(X_upm));
	if ( X_upm == 0 ){
		X_upm = PMessageBox(NULL, MES_QUPD,
			T("PPx Update"), MB_YESNOCANCEL | MB_ICONEXCLAMATION);
		switch (X_upm){
			case IDNO:
				X_upm = 3;
				break;

			case IDYES:
				X_upm = 4;
				break;

			default:
				X_upm = 1;
				break;
		}
	}
	if ( X_upm == 1 ) return;	// 何もしない
	if ( X_upm == 2 ){
		XMessage(NULL, NULL, XM_RESULTld, MES_UPED, T(FileCfg_Version));
	}else{
		if ( (X_upm == 4) || (X_upm == 6) ){	// バックアップ
			HANDLE hFile;
			TCHAR *ptr;
			TCHAR fpath[MAX_PATH], fname[VFPS];
			BOOL error = FALSE;

			ptr = PPcustCDump();
			if ( NO_ERROR != GetCustTable(StrCustSetup, StrPath, &fpath, sizeof(fpath)) ){
				SetCustStringTable(StrCustSetup, StrPath, DLLpath, 0);
			}
			Get_X_save_widthUI(fpath);
			wsprintf(fname, T("PPX%c%c%c_O.TXT"), oldver[0], oldver[2], oldver[3]);
			CatPath(NULL, fpath, fname);

			hFile = CreateFileL(fpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
					FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ){
				error = TRUE;
			}else{
				DWORD size;

				if ( WriteFile(hFile, ptr, TSTRLENGTH32(ptr), &size, NULL) == FALSE ){
					error = TRUE;
				}
				CloseHandle(hFile);
			}
			if ( IsTrue(error) ){
				xmessage(XM_GrERRld, MES_FBAK);
				PPEui(NULL, fpath, ptr);
			}
			HeapFree(ProcHeap, 0, ptr);
		}

		if ( GetCustDataSize(T("P_arc")) <= 0 ){ // ●1.27から当分の間用意
			oldver[1] = '\0';
			GetCustTable(T("KC_main"), T("P"), &oldver, sizeof(oldver));
			if ( memcmp(oldver + 1, T("%\"Pack File\" %M_xpack,!"), TSTROFF(23)) == 0){
				SetCustTable(T("KC_main"), T("P"), &NewPackCmd, sizeof(NewPackCmd));
			}
		}
					// カスタマイズアップデート
		DefCust( (X_upm < 5) ? -PPXCUSTMODE_UPDATE : PPXCUSTMODE_UPDATE );
		PPxCommonCommand(NULL, 0, K_Lcust);
	}
	SetCustData(T("PPxCFG"), T(FileCfg_Version), sizeof(T(FileCfg_Version)));

	return;
}

void UnHideAllPPx(void)
{
	int i;
									// 指定あり？
	for ( i = 0 ; i < X_Mtask ; i++ ){
		if ( (Sm->P[i].ID[0] != '\0') && (Sm->P[i].ID[1] == '_') ){
			DWORD_PTR style;
			HWND hShowWnd;

			hShowWnd = Sm->P[i].hWnd;
			style = GetWindowLongPtr(hShowWnd, GWL_STYLE);
			if ( !(style & WS_VISIBLE) ){
				HWND hHWnd;

				hHWnd = GetParent(hShowWnd);
				if ( (hHWnd == NULL) || !IsWindow(hHWnd) ){
					ShowWindow(hShowWnd, SW_RESTORE);
				}
			}
		}
	}
}

// メニューの表示項目に、キー割当てを付記する(デフォルトカスタマイズ&ppcust)---
// param がキー指定かどうかを調べる
int GetKeyCust(const TCHAR *Cname, const TCHAR *param, int keynested)
{
	int key;
	TCHAR keyword[CMDLINESIZE], buf[CMDLINESIZE];
	int count;

	keynested++;
	SkipSpace(&param);
	if ( (param[0] != '%') ||
		 (param[1] != 'K') ||
		 (param[2] != '\"') ){
		return 0;
	}
	param += 3;
	key = GetKeyCode(&param);
	SkipSpace(&param);
	if ( (*param != '\"') || (key < 0) || (key & K_ex) ) return 0;
	if ( keynested >= 2 ) return key;
	count = 0;
											// 別の割当てがあるか検索
	while( EnumCustTable(count, Cname, keyword, buf, sizeof(buf)) > 0 ){
		WORD nkey;
		const TCHAR *pp;

		count++;
		if ( (UTCHAR)buf[0] == EXTCMD_CMD ){
			nkey = (WORD)GetKeyCust(Cname, buf + 1, keynested);
			if ( nkey == key ){
				pp = keyword;
				return GetKeyCode(&pp);
			}
		}else if ( (UTCHAR)buf[0] == EXTCMD_KEY ){
			nkey = *(WORD *)(buf + 1);
			if ( nkey == key ){
				pp = keyword;
				return GetKeyCode(&pp);
			}
		}
	}
											// キー割当てがかぶせられているか
	PutKeyCode(buf, key & ~K_raw);
	if ( IsExistCustTable(Cname, buf) ){
		if ( key & K_raw ) return -1; // かぶっている
	}
	return key;
}

void KeymapMenuMain(const TCHAR *MenuName, const TCHAR *KeyName, int menunested)
{
	int count = 0;
	BOOL notabfix;
	TCHAR keyword[CMDLINESIZE], param[CMDLINESIZE], buf[CMDLINESIZE], *p;
	int key;

										// 列挙の開始 -------------------------
	while( EnumCustTable(count, MenuName, keyword, param, sizeof(param)) > 0 ){
		count++;
		if ( !tstrcmp(keyword, T("||")) ){		// 改桁 ======================
			continue;
		}
		if ( !tstrcmp(keyword, T("--")) ){		// セパレータ ================
			continue;
		}
		p = param;							// 階層メニュー ==============
		if (	(GetLineParam((const TCHAR **)&p, buf) == '%') &&
				(buf[1] == 'M') &&
				(buf[2] != 'E') ){
			KeymapMenuMain(buf + 1, KeyName, menunested + 1);
			continue;
		}								// 通常の項目 ================
		if ( !keyword[0] || !menunested ) continue;
		notabfix = FALSE;
		for ( p = keyword ; *p ; p++ ){
			if ( Ismulti(*p) ){
				p++;
				continue;
			}
			if ( *p == '\t' ){
				p++;
				notabfix = TRUE;
				break;
			}
			if ( *p != '\\' ) continue;
			p++;
			if ( *p == '\\' ){
				p++;
				continue;
			}
			if ( *p == 't' ){
				p++;
				notabfix = TRUE;
				break;
			}
		}
		key = GetKeyCust(KeyName, param, 0);
		if ( key == 0 ) continue;
		if ( key > 0 ){
			if ( notabfix == FALSE ) *p++ = '\t';
			*p = '\0';
			if ( key & K_s ) tstrcat(p, T("Shift+"));
			if ( key & K_c ) tstrcat(p, T("Ctrl+"));
			if ( key & K_a ) tstrcat(p, T("Alt+"));
			if ( key & K_e ) tstrcat(p, T("Ext+"));
			if ( key & K_v ){
				BYTE tmpkey = (BYTE)key;

				if ( ((tmpkey >= 'A') && (tmpkey <= 'Z')) ||
					 ((tmpkey >= '0') && (tmpkey <= '9')) ){
					resetflag(key, K_v);
				}
			}
			PutKeyCode(p + tstrlen(p), key & 0x1ff);
		}else{
			if ( *p == '\0' ) continue;
			*p = '\0';
		}
		DeleteCustTable(MenuName, NULL, count - 1);
		InsertCustTable(MenuName, keyword, count - 1, param, TSTRSIZE(param));
	}
	return;
}

// ネットワークドライブ割り当て／切断 -----------------------------------------
DWORD ExecWNetDialog(char *name, HWND hWnd)
{
	HMODULE hMPR;
	DWORD result = 1;
	DefineWinAPI(DWORD, WNetdialog, (HWND, DWORD));

	hMPR = LoadSystemDLL(SYSTEMDLL_MPR);
	if ( hMPR == NULL ) return 1;
	DWNetdialog = (impWNetdialog)GetProcAddress(hMPR, name);
	if ( DWNetdialog != NULL ) result = DWNetdialog(hWnd, RESOURCETYPE_DISK);
	FreeLibrary(hMPR);
	return result;
}

void LockPC(void)
{
	DefineWinAPI(BOOL, LockWorkStation, (void));

	GETDLLPROC(GetModuleHandleA(User32DLL), LockWorkStation);
	if ( DLockWorkStation != NULL ) DLockWorkStation();
}

// セッション終了 -------------------------------------------------------------
void ExitSession(HWND hWnd, UINT options)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	if ( OpenProcessToken(GetCurrentProcess(),
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken) == FALSE ){
		xmessage(XM_NsERRd, T("OpenProcessToken"));
	}
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

	if ( GetLastError() != NO_ERROR ){
		xmessage(XM_NsERRd, T("AdjustTokenPrivileges"));
	}

	if ( options & EWX_EXFLAG ){
		if ( SetSystemPowerState((options & B0) ? TRUE : FALSE, FALSE) ==FALSE){
			 xmessage(XM_NsERRd, T("Suspend"));
		}
	}else{
		if ( !ExitWindowsEx(options, 0) ) xmessage(XM_NsERRd, T("ExitWindows"));
		if ( GetParent(hWnd) == NULL ) PostMessage(hWnd, WM_CLOSE, 0, 0);
	}
	CloseHandle(hToken);
}

void SendWmSysyemMessage(WPARAM command, LPARAM lParam)
{
	DWORD_PTR sendresult;

	Sleep(400);
	SendMessageTimeout(HWND_BROADCAST, WM_SYSCOMMAND, command, lParam, SMTO_ABORTIFHUNG, 2000, &sendresult);
}

// 表示Z座標を変更 ------------------------------------------------------------
void WindowZPosition(HWND hWnd, HWND mode)
{
	DWORD inactiveTID, activeTID;

	inactiveTID	= GetWindowThreadProcessId(hWnd, NULL);
	activeTID	= GetWindowThreadProcessId(GetForegroundWindow(), NULL);
	AttachThreadInput(inactiveTID, activeTID, TRUE);
	SetWindowPos(hWnd, mode, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	AttachThreadInput(inactiveTID, activeTID, FALSE);
}
/*
const TCHAR ImmersiveShellPatg[] = T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ImmersiveShell");
const TCHAR TabletMode[] = T("TabletMode");

void WmSettingChange(HWND hWnd, LPARAM lParam)
{
	if ( lParam == 0 ) break;
	if ( tstrcmp((TCHAR *)lParam, T("UserInteractionMode")) == 0 ){
		DWORD value;

		PostMessage(hWnd, WM_PPXCOMMAND, LOAD_UserInteractionMode);

		if ( IsTrue(GetRegString(HKEY_CURRENT_USER, ImmersiveShellPatg, TabletMode, (TCHAR *)&value, TSIZEOF(value))) ){
			GetCustData(T("X_pmc"), &X_pmc, sizeof(X_pmc));
			TouchMode = value ? ~X_pmc[1] : 0;
			PostMessage(hWnd, WM_PPXCOMMAND, value ? K_E_TABLET : K_E_PC, 0);
		}
	}
}
*/

void PPxCleanup(void)
{
	NowExit = TRUE;
	UsePPx();
	if ( Sm->hhookCBT ){
		UnhookWindowsHookEx(Sm->hhookCBT);
		Sm->hhookCBT = NULL;
	}
	FreePPxModule();
	FreeRMatch();
	FreeMigemo();
	// カスタマイズログが開いていたら閉じる。
	// ※ログ窓が残留して異常終了することの対策
	if ( hUpdateResultWnd != NULL ){
		DestroyWindow(hUpdateResultWnd);
		hUpdateResultWnd = NULL;
	}
	CleanUpVFS();
	CleanUpEdit();
	FreeSysColors();
	if ( hComctl32 != NULL ){
		FreeLibrary(hComctl32);
		hComctl32 = NULL;
	}
	if ( ProcTempPath[0] != '\0' ){
		TCHAR temppath[MAX_PATH];
		size_t proclen;

		GetTempPath(MAX_PATH, temppath);
		proclen = tstrlen(ProcTempPath);
		// c:\ とかを削除しないように
		if ( (proclen <= 3) || ((tstrlen(temppath) + 1) >= proclen) ){
			XMessage(NULL,NULL,XM_DbgLOG,T("temppath error: %s"), ProcTempPath);
		}else{
			DeleteDirectories(ProcTempPath, FALSE);
		}
		ProcTempPath[0] = '\0';
	}
	FreePPx();
}

/*-----------------------------------------------------------------------------
	PPxCommand の共通部

return ERROR_INVALID_FUNCTION :未処理
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI PPxCommonCommand(HWND hWnd, LPARAM lParam, WORD key)
{
	key &= ~K_raw;
	switch ( key ){
		case K_s | K_F1:
		case K_F1:
			PPxHelp(hWnd, HELP_FINDER, 0);
			break;

		case K_GETJOBWINDOW:
			if ( lParam == 0 ) return (Sm->JobList.hWnd != NULL);
			if ( lParam == 1 ){
				if ( Sm->JobList.hWnd != NULL ){
					PostMessage(Sm->JobList.hWnd, WM_CLOSE, 0, 0);
				}
				break;
			}
			if ( Sm->JobList.hWnd != NULL ){
				*(HWND *)lParam = NULL;
				break; // 使用済み
			}
			Sm->JobList.hWnd = hWnd;
			Sm->JobList.showmode = JOBLIST_INNER;
			CreateJobListWnd();
			*(HWND *)lParam = GetParent(Sm->JobList.hWnd);
			break;

		case K_ADDJOBTASK:
			SetJobTask(hWnd, (lParam & JOBFLAG_STATEMASK) ? lParam : lParam | JOBSTATE_REGIST);
			break;

		case K_DELETEJOBTASK:
			SetJobTask(hWnd, JOBSTATE_UNREGIST);
			break;

		case K_CLEANUP:					// Clean up -------------------------
			PPxCleanup();
			break;
/*
		case K_SETTINGCHANGE:
			WmSettingChange(hWnd, lParam);
			break;
		case K_E_PC:
			TouchMode = 0;
			break;
*/
		case K_E_TABLET:
			GetCustData(T("X_pmc"), &X_pmc, sizeof(X_pmc));
			TouchMode = ~X_pmc[1];
			break;

		case K_Lcust:
			if ( lParam != 0 ){
				if ( CustTick == (DWORD)lParam ) break; // 重複なので省略
				CustTick = (DWORD)lParam;
			}else{
				CustTick = GetTickCount();
			}
			CleanUpEdit();
			FreeSysColors();
			if ( (usertypes != INVALID_HANDLE_VALUE) && (usertypes != NULL) ){
				HeapFree(DLLheap, 0, usertypes);
				usertypes = INVALID_HANDLE_VALUE;
			}
			ReloadMessageText();
			GetCustData(T("X_log"), &X_log, sizeof(X_log));
			X_execs = -1;
			X_Keyra = 1;
			X_dss = DSS_NOLOAD;
			X_prtg = -1;
			DFindFirstFile = LoadFindFirstFile;
			X_jinfo[0] = MAX32;
			break;

		case K_HIDE:					// Hide window
			if ( !(GetWindowLongPtr(hWnd, GWL_STYLE) & WS_VISIBLE) ){
				break; // 既に hide の状態
			}
			if ( PPxGetHWND(T(PPTRAY_REGID) T("A")) == NULL ){
				ComExecSelf(hWnd, T(PPTRAYEXE), DLLpath, 0, NULL);
			}
			ShowWindow(hWnd, SW_HIDE);
			break;

		case K_UNHIDEALL:				// Un Hide All PPx
			UnHideAllPPx();
			break;

		case K_WTOP:					// Top window
			WindowZPosition(hWnd, HWND_TOP);
			break;

		case K_WBOT:					// Bottom window
			WindowZPosition(hWnd, HWND_BOTTOM);
			break;

		case K_c | 'L':					// Redraw -----------------------------
		case K_s | K_F5:
			InvalidateRect(hWnd, NULL, TRUE);
			break;
#if 0
		case K_a | K_space: {			// System Memu ------------------------
			POINT temppos;				// (現在未使用)

			if ( pos == NULL ){
				GetMessagePosPoint(temppos);
				ClientToScreen(hWnd, &temppos);
				pos = &temppos;
			}
			SendMessage(hWnd, WM_ENTERMENULOOP, FALSE, 0);
			TrackPopupMenu(GetSystemMenu(hWnd, FALSE),
					TPM_LEFTALIGN | TPM_LEFTBUTTON,
					pos->x, pos->y, 0, hWnd, NULL);
			SendMessage(hWnd, WM_EXITMENULOOP, FALSE, 0);
			break;
		}
#endif
		case K_cust:					// Customizer -------------------------
			ComExecSelf(hWnd, T(PPCUSTEXE), DLLpath, 0, NULL);
			break;

		case K_ANDV:					// Allocate Network drive -------------
			ExecWNetDialog("WNetConnectionDialog", hWnd);
			break;

		case K_FNDV:					// Free Network drive -----------------
			ExecWNetDialog("WNetDisconnectDialog", hWnd);
			break;

		case K_Loff:					// Logoff -----------------------------
			ExitSession(hWnd, EWX_LOGOFF);
			break;

		case K_Poff:					// Poweroff ---------------------------
			ExitSession(hWnd, EWX_POWEROFF);
			break;

		case K_Rbt:						// Reboot -----------------------------
			ExitSession(hWnd, EWX_REBOOT);
			break;

		case K_Sdw:						// Shutdown ---------------------------
			ExitSession(hWnd, EWX_SHUTDOWN);
			break;

		case K_Fsdw:					// Force Shutdown ---------------------
			ExitSession(hWnd, EWX_FORCE);
			break;

		case K_Suspend:					// Suspend ---------------------
			ExitSession(hWnd, EWX_EX_SUSPEND);
			break;

		case K_Hibernate:				// Hibernate ---------------------
			ExitSession(hWnd, EWX_EX_HIBERNATE);
			break;

		case K_SSav:					// Screen saver -----------------------
			SendWmSysyemMessage(SC_SCREENSAVE, 0);
			break;

		case K_supot:					// Support ----------------------------
			if ( PMessageBox(hWnd, MES_QTWP, MES_TTWP,
					MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES){
				ComExecSelf(hWnd, T(TOROsWEBPAGE), DLLpath, 0, NULL);
			}
			break;

		case K_s | K_esc:				// Iconic/Minimize --------------------
			SendMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, MAX32);
			break;

		case K_c | K_tab:				// [TAB] next ppx ---------------------
		case K_c | K_s | K_tab:{		// \[TAB] pre ppx ---------------------
			HWND hNextWnd;

			hNextWnd = PPxGetWindow(NULL, (key == (K_c | K_tab)) ? 1 : -1);
			if ( hNextWnd != NULL ){
				ForceSetForegroundWindow(hNextWnd);
				SetFocus(hNextWnd);
			}
			break;
		}

		case K_a | K_up:				// &[↑] move -------------------------
			MoveWindowByKey(hWnd, 0, -1);
			break;

		case K_a | K_dw:				// &[↓] move -------------------------
			MoveWindowByKey(hWnd, 0, 1);
			break;

		case K_a | K_lf:				// &[←] move -------------------------
			MoveWindowByKey(hWnd, -1, 0);
			break;

		case K_a | K_ri:				// &[→] move -------------------------
			MoveWindowByKey(hWnd, 1, 0);
			break;

		case K_c | K_s | K_v | VK_ADD:
		case K_c | K_s | K_v | VK_OEM_PLUS: // US[=/+] JIS[;/+]
			ChangeOpaqueWindow(hWnd, 1);
			break;
		case K_c | K_s | K_v | VK_SUBTRACT:
		case K_c | K_s | K_v | VK_OEM_MINUS: // US[-/_] JIS[-/=]
			ChangeOpaqueWindow(hWnd, -1);
			break;

		case K_SETIME:
			SetIMEDefaultStatus(hWnd);
			break;

		case K_ENABLE_NC_SCALE:
			EnableNcScale(hWnd);
			break;

		case K_IMEOFF:
			SetIMEStatus(hWnd, 0);
			if ( GetShiftKey() & K_e ){
				keybd_event((BYTE)X_es, 0, KEYEVENTF_KEYUP, 0);
			}
			break;

		default:
			return ERROR_INVALID_FUNCTION;
	}
	return NO_ERROR;
}

#ifndef TBSTYLE_CUSTOMERASE
#define TBSTYLE_CUSTOMERASE 0x2000
#define CDDS_PREERASE 0x00000003
#define CDRF_SKIPDEFAULT 0x00000004
#define CDRF_DODEFAULT 0x00000000
#endif
#ifndef NM_CUSTOMDRAW
#define NM_CUSTOMDRAW (NM_FIRST-12)

typedef struct
{
	NMHDR hdr;
	DWORD dwDrawStage;
	HDC hdc;
	RECT rc;
	DWORD_PTR dwItemSpec;
	UINT uItemState;
	LPARAM lItemlParam;
} NMCUSTOMDRAW;
#endif

LRESULT DrawCCWndBack(NMCUSTOMDRAW *CSD)
{
	RECT box;

	if ( hDialogBackBrush == NULL ) return CDRF_DODEFAULT;
	if ( CSD->dwDrawStage != CDDS_PREERASE ) return CDRF_DODEFAULT;

	GetClientRect(CSD->hdr.hwndFrom, &box);
	FillBox(CSD->hdc, &box, hDialogBackBrush);
	return CDRF_SKIPDEFAULT;
}

PPXDLL LRESULT PPXAPI PPxCommonExtCommand(WORD key, WPARAM wParam)
{
	switch( key ){
		case K_menukeycust:				// Menu key comment customize ---------
			KeymapMenuMain(T("MC_menu"), T("KC_main"), 0);
			KeymapMenuMain(T("MV_menu"), T("KV_main"), 0);
			break;

		case K_REPORTSHUTDOWN:
			Sm->NowShutdown = TRUE;
			break;

		case K_CHECKUPDATE:
			AutoCustomizeUpdate();
			break;

		case K_GETDISPDPI:
			return (LRESULT)GetMonitorDPI((HWND)wParam);

		case KC_HOOKADDPROC: {
			DWORD ThreadID;

			ThreadID = GetWindowThreadProcessId((HWND)wParam, NULL);
			if ( ThreadID == 0 ) return ERROR_INVALID_DATA;
			hAutoDDDLL = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)AutoDDDLLProc, DLLhInst, ThreadID);
			break;
		}

		case KC_UNHOOKADDPROC:
			UnhookWindowsHookEx(hAutoDDDLL);
			PostMessage((HWND)wParam, WM_NULL, 0, 0); // DLL切り離しを指示する
			break;

		case K_SETAPPID:
			SetAppID();
			break;

		case K_CPOPMENU:
			return TTrackPopupMenu(NULL, (HMENU)wParam, NULL);

		case K_TBB_INIT:
			InitTaskBarButtonIF(wParam);
			break;

		case K_TBB_PROGRESS:
			SetTaskBarButtonProgress(
					((TASKBARBUTTONPROGRESSINFO *)wParam)->hWnd,
					((TASKBARBUTTONPROGRESSINFO *)wParam)->nowcount,
					((TASKBARBUTTONPROGRESSINFO *)wParam)->maxcount);
			break;

		case K_TBB_STOPPROGRESS:
			SetTaskBarButtonProgress((HWND)wParam, TBPF_NOPROGRESS, 0);
			break;

		case K_FLASHWINDOW:
			return PPxFlashWindow((HWND)wParam, PPXFLASH_FLASH);

		case K_STOPFLASHWINDOW:
			return PPxFlashWindow((HWND)wParam, PPXFLASH_STOP);

		case K_SETFAULTOPTIONINFO:
			FaultOptionInfo = (void **)wParam;
			break;

		case K_SENDREPORT:
			PPxSendReport((const TCHAR *)wParam);
			break;

		case K_SETLOGWINDOW:
			Sm->hCommonLogWnd = (HWND)wParam;
			break;

		case K_INITROMA:
			if ( LoadRegExp() == FALSE ) return FALSE;
			return (LRESULT)InitMigemo();

		case K_THREADUNREG:
			UsePPx();
			UnRegisterThread((DWORD)wParam);
			FreePPx();
			break;

		case KC_GETCRCHECK:
			return (LRESULT)&CrmenuCheck;

		#define CSD ((NMCUSTOMDRAW *)wParam)
		case K_DRAWCCBACK:
			if ( wParam == 0 ){
				InitSysColors();
				return (LRESULT)hDialogBackBrush;
			}else{
				if ( hDialogBackBrush == NULL ) return CDRF_DODEFAULT;
				if ( CSD->dwDrawStage != CDDS_PREERASE ) return CDRF_DODEFAULT;
				FillBox(CSD->hdc, &CSD->rc, hDialogBackBrush);
				return CDRF_SKIPDEFAULT;
			}
		#undef CSD

		case K_DRAWCCWNDBACK:
			return DrawCCWndBack((NMCUSTOMDRAW *)wParam);

		case K_UxTheme:
			switch (wParam) {
				case KUT_WINDOW_TEXT_COLOR:
					return C_WindowText;
				case KUT_WINDOW_BACK_COLOR:
					return C_WindowBack;
				case KUT_DIALOG_BACK_COLOR:
					return C_DialogBack;
				case KUT_INIT:
					if ( X_uxt == UXT_NA ) InitUnthemeCmd();
					// default:
				default:
					return X_uxt;
			}
		default:
			return ERROR_INVALID_FUNCTION;
	}
	return NO_ERROR;
}
