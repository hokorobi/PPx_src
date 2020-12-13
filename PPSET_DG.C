/*-----------------------------------------------------------------------------
	Paper Plane xUI	 setup wizard - dialog
-----------------------------------------------------------------------------*/
#include "PPSETUP.H"
#pragma hdrstop

const int jumpto[] = {
	PAGE_DEST,	// IDR_AUTOINST
	PAGE_DEST,	// IDR_CUSTINST
	PAGE_UP,	// IDR_UPDATE
	PAGE_UP,	// IDR_CHECKUPDATE
	PAGE_UN_EXEC}; // IDR_UNINST
#ifdef _WIN64
const TCHAR UnbypassName[] = T("UNBYPASS.DLL");
#endif
typedef BOOL (PPXAPI *impGetImageByHttp)(const TCHAR *urladr, ThSTRUCT *th);

/*-----------------------------------------------------------------------------
  page 1:ガイド
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK GuideDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if ( msg == WM_NOTIFY ){
			#define NHPTR ((NMHDR *)lParam)
		switch (NHPTR->code){
			case PSN_SETACTIVE:
				PropSheet_SetWizButtons(NHPTR->hwndFrom, PSWIZB_NEXT);
				//	PSN_QUERYCANCEL へ
			case PSN_QUERYCANCEL:	// すぐ終了してもよし
				return TRUE;
		}
		#undef NHPTR
	}
	return StyleDlgProc(hDlg, msg, wParam, lParam);
}
/*-----------------------------------------------------------------------------
  page 2:種別選択
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK SetTypeDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_INITDIALOG: {
			BOOL enablestate;

			SetDlgItemText(hDlg, IDS_INSTEDPPX, XX_setupedPPx);

			enablestate = (XX_setupmode >= IDR_UPDATE) ||
					 ((XX_setupedPPx[0] != '\0') && (XX_setupedPPx[0] != '?'));
			EnableWindow(GetDlgItem(hDlg, IDR_UPDATE), enablestate);
			EnableWindow(GetDlgItem(hDlg, IDR_CHECKUPDATE), enablestate);
			EnableWindow(GetDlgItem(hDlg, IDX_TESTVER), enablestate);
			EnableWindow(GetDlgItem(hDlg, IDR_UNINST), enablestate);
			CheckDlgButtons(hDlg, IDR_AUTOINST, IDR_UNINST, XX_setupmode);
			break;
		}

		case WM_COMMAND:
			if ( LOWORD(wParam) == IDB_SELINSTPPX ){
				TCHAR buf[MAX_PATH];

				if ( SelectDirectory(hDlg, MessageStr[MSG_SELECTPPX],
						BIF_STATUSTEXT, SelInstPPxProc, buf) == FALSE ) break;

				tstrcpy(XX_setupedPPx, buf);
				XX_setupmode = IDR_UPDATE;
				SetTypeDialog(hDlg, WM_INITDIALOG, 0, 0);
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch ( NHPTR->code ){
//				case PSN_APPLY:
//				case PSN_GETOBJECT: //V5 以降
//				case PSN_RESET:		// 終了時
//				case PSN_TRANSLATEACCELERATOR:  //V5 以降
#if 0					// Version 5.00 以降ならできるそうな。
#ifndef PSN_QUERYINITIALFOCUS
#define PSN_QUERYINITIALFOCUS (PSN_SETACTIVE-13)
#endif
				case PSN_QUERYINITIALFOCUS:	// フォーカスを当てる
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT,
							(LONG_PTR)GetDlgItem(hDlg, XX_setupmode))
					break;
#endif
				case PSN_SETACTIVE:
					PropSheet_SetWizButtons(
							NHPTR->hwndFrom, PSWIZB_BACK | PSWIZB_NEXT);
					break;

				case PSN_WIZNEXT:{
					XX_setupmode = GetDlgButtons(hDlg, IDR_AUTOINST, IDR_UNINST);

					if ( XX_setupmode == IDR_CHECKUPDATE ){
						if ( Execute_ExtractMacro(
								IsDlgButtonChecked(hDlg, IDX_TESTVER) ?
								T("*checkupdate p") :
								T("*checkupdate"), TRUE) != NO_ERROR ){
							SetWindowLongPtr(hDlg, DWLP_MSGRESULT,
									UsePageInfo[PAGE_SETTYPE].rID);
							break;
						}
						PropSheet_PressButton( // 終了させる
								NHPTR->hwndFrom, PSBTN_FINISH);
						return -1;
					}

					if ( (XX_setupmode <= IDR_CUSTINST) && XX_setupedPPx[0] ){
						if ( MessageBox(hDlg, MessageStr[MSG_OVERINSTALL],
								msgboxtitle,
								MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)
																 != IDYES){
							SetWindowLongPtr(hDlg, DWLP_MSGRESULT,
									UsePageInfo[PAGE_SETTYPE].rID);
							break;
						}
					}
					if ( XX_setupmode == IDR_AUTOINST ){	// お手軽用に再設定
						XX_instdestM	= IDR_PROGRAMS;
						XX_usereg		= IDR_USEREG;
						XX_setupPPx		= XX_instdestP;
						if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT ){
							XX_link_menu	= 0;
							XX_link_cmenu	= 1;
						}else{
							XX_link_menu	= 1;
							XX_link_cmenu	= 0;
						}
						XX_link_boot	= 0;
						XX_link_desk	= 0;
						XX_link_app		= 1;
					}
					if ( (XX_setupmode <= IDR_CUSTINST) ||
						 (XX_setupmode == IDR_UNINST) ){
						if ( AdminCheck() == ADMINMODE_NOADMIN ){ // UAC 制限状態
							SMessage(MessageStr[MSG_RUNAS]);
							XX_instdestM = IDR_SELCOPY;
							XX_link_menu	= 1;
							XX_link_cmenu	= 0;
						}
					}
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT,
						UsePageInfo[jumpto[XX_setupmode - IDR_AUTOINST]].rID);
					break;
				}
				case PSN_QUERYCANCEL:	// すぐ終了してもよし
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 1:インストール先
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK DestDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDE_SELCOPY:
					if ( HIWORD(wParam) == EN_CHANGE ){
						XX_instdestM = IDR_SELCOPY;
						CheckDlgButtons(hDlg,
								IDR_PROGRAMS, IDR_SELCOPY, XX_instdestM);
					}
					break;

				case IDB_SELCOPY:{
					TCHAR buf[MAX_PATH];

					if ( SelectDirectory(hDlg, MessageStr[MSG_SELECTDIR],
							BIF_USENEWUI | BIF_RETURNONLYFSDIRS, SelDirProc, buf)
							== FALSE ){
						break;
					}

					wsprintf(XX_instdestS, T("%s\\PPX"), buf);
					CheckDlgButton(hDlg, XX_instdestM, FALSE);
					XX_instdestM = IDR_SELCOPY;
					SetDlgItemText(hDlg, IDE_SELCOPY, XX_instdestS);
					break;
				}
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE:{
					TCHAR buf[CMDLINESIZE];
					int i;

					i = XX_instdestM;
					SetDlgItemText(hDlg, IDE_SELCOPY, XX_instdestS);
					XX_instdestM = i;
					wsprintf(buf, MessageStr[MSG_DEFAULTDIR], XX_instdestP);
					SetDlgItemText(hDlg, IDR_PROGRAMS, buf);
					wsprintf(buf, MessageStr[MSG_NOCOPY], MyPath);
					SetDlgItemText(hDlg, IDR_NOCOPY, buf);
					CheckDlgButtons(hDlg,
							IDR_PROGRAMS, IDR_SELCOPY, XX_instdestM);
					break;
				}
				case PSN_WIZBACK:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_SETTYPE].rID);
					break;
				case PSN_WIZNEXT:
					GetDlgItemText(hDlg, IDE_SELCOPY, XX_instdestS, MAX_PATH);
					XX_instdestM = GetDlgButtons(hDlg,
							IDR_PROGRAMS, IDR_SELCOPY);
					XX_setupPPx = (XX_instdestM == IDR_PROGRAMS) ?
							XX_instdestP : ((XX_instdestM == IDR_NOCOPY) ?
								MyPath : XX_instdestS);

					if ( XX_setupmode == IDR_AUTOINST){	// お手軽用なら窓形態へ
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_PPCW].rID);
					}
					break;
				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 2:カスタマイズファイル保存先
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK RegDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE:
					CheckDlgButtons(hDlg, IDR_USEREG, IDR_UUSEREG, XX_usereg);
					break;

				case PSN_WIZNEXT:
					XX_usereg = GetDlgButtons(hDlg, IDR_USEREG, IDR_UUSEREG);
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR
		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 3:PPc 窓配置
-----------------------------------------------------------------------------*/
void ShowDlgWindows(const HWND hDlg, UINT minid, const UINT maxid, BOOL show)
{
	HWND hControlWnd;

	for (;;){
		hControlWnd = GetDlgItem(hDlg, minid);
		if ( hControlWnd != NULL ){
			ShowWindow(hControlWnd, show ? SW_SHOW : SW_HIDE);
		}
		if ( minid == maxid ) return;
		minid++;
	}
}

void FixWindowLayoutItems(HWND hDlg)
{
	CheckDlgButtons(hDlg, IDR_PPC_1WINDOW, IDR_PPC_JOINT, XX_ppc_window);
	CheckDlgButton(hDlg, IDX_PPC_TREE, XX_ppc_tree);
	CheckDlgButtons(hDlg, IDR_PPC_1PANE, IDR_PPC_2PANE_V, XX_ppc_pane);
	CheckDlgButtons(hDlg, IDR_PPC_TAB_NO, IDR_PPC_TAB_SHARE, XX_ppc_tab);
	CheckDlgButtons(hDlg, IDR_PPC_JOIN_H, IDR_PPC_JOIN_P, XX_ppc_join);
	ShowDlgWindows(hDlg, IDR_PPC_1PANE, IDR_PPC_TAB_SHARE, ( XX_ppc_window == IDR_PPC_COMBO ));
	ShowDlgWindows(hDlg, IDR_PPC_JOIN_H, IDR_PPC_JOIN_P, ( XX_ppc_window == IDR_PPC_JOINT ));
}

void SaveWindowLayoutItems(HWND hDlg)
{
	XX_ppc_window = GetDlgButtons(hDlg, IDR_PPC_1WINDOW, IDR_PPC_JOINT);
	XX_ppc_tree = IsDlgButtonChecked(hDlg, IDX_PPC_TREE);
	XX_ppc_pane = GetDlgButtons(hDlg, IDR_PPC_1PANE, IDR_PPC_2PANE_V);
	XX_ppc_tab = GetDlgButtons(hDlg, IDR_PPC_TAB_NO, IDR_PPC_TAB_SHARE);
	XX_ppc_join = GetDlgButtons(hDlg, IDR_PPC_JOIN_H, IDR_PPC_JOIN_P);
}

INT_PTR CALLBACK PPcwDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE:
					FixWindowLayoutItems(hDlg);
					break;

				case PSN_WIZBACK:
					if ( XX_setupmode == IDR_AUTOINST ){
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_DEST].rID);
					}
					break;

				case PSN_WIZNEXT:
					SaveWindowLayoutItems(hDlg);
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		case WM_COMMAND:
			if ( (LOWORD(wParam) >= IDR_PPC_1WINDOW) &&
				 (HIWORD(wParam) == BN_CLICKED) ){
				SaveWindowLayoutItems(hDlg);
				FixWindowLayoutItems(hDlg);
			}

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 4:キー割り当て
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK KeyDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDR_XPRKEY:
					CheckDlgButton(hDlg, IDX_DOSC, FALSE);
					break;
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE:
					CheckDlgButtons(hDlg, IDR_PPXKEY, IDR_FMKEY, XX_keytype);
					CheckDlgButton(hDlg, IDX_DIAKEY, XX_diakey);
					CheckDlgButton(hDlg, IDX_EMENU, XX_emenu);
					CheckDlgButton(hDlg, IDX_DOSC, XX_doscolor);
					break;

				case PSN_WIZNEXT:
					XX_keytype = GetDlgButtons(hDlg, IDR_PPXKEY, IDR_FMKEY);
					XX_diakey = IsDlgButtonChecked(hDlg, IDX_DIAKEY);
					XX_emenu = IsDlgButtonChecked(hDlg, IDX_EMENU);
					XX_doscolor = IsDlgButtonChecked(hDlg, IDX_DOSC);
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 5:初期アプリケーション選択
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK AppDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_TEXTEDIT:
					SetExecuteFile(hDlg, IDE_TEXTEDIT,
							MessageStr[MSG_SELECTEDITOR]);
					break;
				case IDB_UNIVIEW:
					SetExecuteFile(hDlg, IDE_UNIVIEW,
							MessageStr[MSG_SELECTVIEWER]);
					break;
				case IDB_SUSIEDIR: {
					TCHAR buf[MAX_PATH];

					if ( SelectDirectory(hDlg, MessageStr[MSG_SELECTSUSIE],
							BIF_RETURNONLYFSDIRS, NULL, buf) == FALSE ) break;
					SetDlgItemText(hDlg, IDE_SUSIEDIR, buf);
					break;
				}
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE: {
					const TCHAR **arcs;

					SetDlgItemText(hDlg, IDE_TEXTEDIT, XX_editor);
					SetDlgItemText(hDlg, IDE_UNIVIEW, XX_viewer);
					// UNARC --------------------------------------------------
					SendDlgItemMessage(hDlg, IDC_UNARC, CB_RESETCONTENT, 0, 0);
					for ( arcs = UNARCS ; *arcs ; arcs++ ){
						HMODULE hDLL;

						hDLL = LoadLibraryEx(*arcs, NULL,
								DONT_RESOLVE_DLL_REFERENCES);
						if ( hDLL != NULL ){
							TCHAR dpath[MAX_PATH];

							GetModuleFileName(hDLL, dpath, TSIZEOF(dpath));
							SendDlgItemMessage(hDlg, IDC_UNARC,
									CB_ADDSTRING, 0, (LPARAM)dpath);
							FreeLibrary(hDLL);
						}
					}
					SendDlgItemMessage(hDlg, IDC_UNARC, CB_SETCURSEL, 0, 0);
					// Susie --------------------------------------------------
					SetDlgItemText(hDlg, IDE_SUSIEDIR, XX_susie);
					break;
				}

				case PSN_WIZNEXT:
					GetDlgItemText(hDlg, IDE_TEXTEDIT, XX_editor, MAX_PATH);
					GetDlgItemText(hDlg, IDE_UNIVIEW, XX_viewer, MAX_PATH);
					{
						TCHAR dir[MAX_PATH];
						GetDlgItemText(hDlg, IDE_SUSIEDIR, dir, MAX_PATH);
						if ( tstrcmp(XX_susie, dir) ){
							XX_usesusie = TRUE;
							tstrcpy(XX_susie, dir);
						}
					}
					if ( XX_setupmode == IDR_AUTOINST ){
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_READY].rID);
					}
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 6:ショートカット作成先
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK LinkDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDX_MENU:
					CheckDlgButton(hDlg, IDX_CMENU, FALSE);
					break;
				case IDX_CMENU:
					CheckDlgButton(hDlg, IDX_MENU, FALSE);
					break;
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE: {
					TCHAR buf[MAX_PATH];

					if ( XX_usereg == IDR_UUSEREG ){
						XX_link_cmenu = XX_link_app = 0;
					}
					if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
						EnableWindow(GetDlgItem(hDlg, IDX_CMENU), FALSE);
					}
					CheckDlgButton(hDlg, IDX_MENU, XX_link_menu);
					CheckDlgButton(hDlg, IDX_BOOT, XX_link_boot);
					CheckDlgButton(hDlg, IDX_DESK, XX_link_desk);
					CheckDlgButton(hDlg, IDX_CMENU, XX_link_cmenu);
					CheckDlgButton(hDlg, IDX_APP, XX_link_app);
					tstrcpy(buf, XX_setupPPx);
					tstrcat(buf, T("\\PPX.EXE"));
					SetDlgItemText(hDlg, IDE_LINKSMP, buf);
					break;
				}

				case PSN_WIZNEXT:
					XX_link_menu = IsDlgButtonChecked(hDlg, IDX_MENU);
					XX_link_boot = IsDlgButtonChecked(hDlg, IDX_BOOT);
					XX_link_desk = IsDlgButtonChecked(hDlg, IDX_DESK);
					XX_link_cmenu = IsDlgButtonChecked(hDlg, IDX_CMENU);
					XX_link_app = IsDlgButtonChecked(hDlg, IDX_APP);
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 7:実行確認
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK ReadyDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_WIZBACK:
					if ( XX_setupmode == IDR_AUTOINST ){
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_APP].rID);
					}
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page install 8/update:コピー進捗状況
-----------------------------------------------------------------------------*/
INT_PTR CALLBACK CopyDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if ( msg == WM_NOTIFY ){
		#define NHPTR ((NMHDR *)lParam)
		switch(NHPTR->code){
			case PSN_SETACTIVE:
				hResultWnd = GetDlgItem(hDlg, IDE_RESULT);
				// Vista は管理者権限で PPc を起動するため起動オプションを無効
				if ( (OSver.dwMajorVersion >= 6) &&
						(AdminCheck() == ADMINMODE_ELEVATE) ){
					SetDlgItemText(hDlg, IDX_STARTPPC, NilStr);
					CheckDlgButton(hDlg, IDX_STARTPPC, FALSE);
				}else{
					CheckDlgButton(hDlg, IDX_STARTPPC, TRUE);
				}
				if ( XX_setupmode == IDR_UPDATE ){	// アップデート
					SetupResult = SRESULT_NOERROR;
					XX_setupPPx = XX_setupedPPx;
					CloseAllPPxLocal(XX_setupPPx, TRUE); // PPxを終了させる
					CopyPPxFiles(hDlg, XX_upsrc, XX_setupPPx);
					if ( SetupResult != SRESULT_NOERROR ){
						CheckDlgButton(hDlg, IDX_STARTPPC, FALSE);
					}
					WriteResult(NilStr, RESULT_ALLDONE);
					if ( IsTrue(XX_resultquiet) && (SetupResult == SRESULT_NOERROR) ){
						PropSheet_PressButton(NHPTR->hwndFrom, PSBTN_FINISH);
					}
				}else{
					InstallMain(hDlg);
				}
				PropSheet_SetWizButtons(NHPTR->hwndFrom, PSWIZB_FINISH);
				PropSheet_CancelToClose(NHPTR->hwndFrom);
				//	PSN_QUERYCANCEL へ

			case PSN_QUERYCANCEL:	// すぐ終了してもよし
				return TRUE;

			case PSN_WIZFINISH:
				if ( ((OSver.dwMajorVersion < 6) ||
					  (AdminCheck() != ADMINMODE_ELEVATE)) &&
						IsDlgButtonChecked(hDlg, IDX_STARTPPC) ){
					TCHAR buf[CMDLINESIZE];

					wsprintf(buf, T("\"%s\\") T(CEXE) T("\""), XX_setupPPx);

					if ( IsTrue(XX_resultquiet) && (SetupResult == SRESULT_NOERROR) ){
						tstrcat(buf, T(" /k %K\"about\""));
					}

					Cmd(buf);
				}
				return TRUE;
		}
		#undef NHPTR
	}
	return StyleDlgProc(hDlg, msg, wParam, lParam);
}
/*-----------------------------------------------------------------------------
  page update:アップデート先
-----------------------------------------------------------------------------*/
BOOL SelfUpdate(void)
{
	TCHAR buf[MAX_PATH * 2];

	wsprintf(buf, T("\"%s\\SETUP.EXE"), XX_upsrc);
	if ( GetFileAttributes(buf + 1) == BADATTR ) return FALSE;
	tstrcat(buf, T("\" /U "));
	tstrcat(buf, XX_setupedPPx);
	Cmd(buf);
	return TRUE;
}

INT_PTR CALLBACK UpDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_UPSRC:{
					TCHAR buf[MAX_PATH];
					if ( SelectDirectory(hDlg, MessageStr[MSG_SELECTPPX],
							BIF_RETURNONLYFSDIRS, NULL, buf) == FALSE ) break;
					SetDlgItemText(hDlg, IDE_UPSRC, buf);
					break;
				}
			}
			break;

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_SETACTIVE:
										// コピー元
					if ( tstricmp(MyPath, XX_setupedPPx) ){	// Setup がもとか?
						SetDlgItemText(hDlg, IDE_UPSRC, MyPath);
					}
					SetDlgItemText(hDlg, IDS_UPDEST, XX_setupedPPx);
					break;

				case PSN_WIZBACK:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_SETTYPE].rID);
					break;

				case PSN_WIZNEXT:
					GetDlgItemText(hDlg, IDE_UPSRC, XX_upsrc, TSIZEOF(XX_upsrc));
							// インストール元に SETUP.EXE があれば、それを使う
					if ( tstricmp(MyPath, XX_upsrc) ){
						if ( IsTrue(SelfUpdate()) ){
							PropSheet_PressButton( // 終了させる
									NHPTR->hwndFrom, PSBTN_FINISH);
							return -1;
						}
					}
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, UsePageInfo[PAGE_COPY].rID);
					break;

				default:
					return StyleDlgProc(hDlg, msg, wParam, lParam);
			}
			break;
			#undef NHPTR

		default:
			return StyleDlgProc(hDlg, msg, wParam, lParam);
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
  page common
-----------------------------------------------------------------------------*/
#pragma argsused
INT_PTR CALLBACK StyleDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UnUsedParam(wParam);

	switch (msg){
//		case WM_HELP:
		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
//				case PSN_APPLY:
//				case PSN_HELP:
				case PSN_QUERYCANCEL:
					if ( MessageBox(hDlg, MessageStr[MSG_ABORT], msgboxtitle,
							MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)
								!= IDYES ){
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
					}
					break;
				default:
					return FALSE;
			}
			break;
			#undef NHPTR

		default:	// 特になし
			return FALSE;
	}
	return TRUE;
}

BOOL CreateSourceDirectory(TCHAR *dir)
{
	TCHAR buf[MAX_PATH];

	GetTempPath(TSIZEOF(buf), buf);
	wsprintf(dir, T("%s\\PPXUPDIR"), buf);
	DeleteDir(dir);
	if ( CreateDirectory(dir, NULL) != FALSE ){
		DeleteUpsrc = TRUE;
		return TRUE;
	}else{
		SMessage(T("%temp%\\PPXUPDIR create error"));
		return FALSE;
	}
}

BOOL UnarcFile(const TCHAR *arcname, const char *arcfuncname, const char *arcparam)
{
	HMODULE hUN;
	impUnarc unarcfunc;
	BOOL result = FALSE;
	TCHAR destdir[MAX_PATH];
	char buf[CMDLINESIZE], log[0x1000];

#ifdef _WIN64
	if ( *arcname == '7' ){
		hUN = LoadLibrary(T("7-ZIP64.DLL"));
	}else{
		hUN = NULL;
	}
	if ( hUN == NULL ){
		hUN = LoadLibrary(arcname);
		if ( (hUN == NULL) && (GetLastError() == ERROR_BAD_EXE_FORMAT) ){
			hUN = LoadLibrary(UnbypassName);
		}
	}
#else
	hUN = LoadLibrary(arcname);
#endif
	if ( hUN == NULL ) return FALSE;
	unarcfunc = (impUnarc)GetProcAddress(hUN, arcfuncname);
	if ( unarcfunc != NULL ){
		if ( CreateSourceDirectory(destdir) != FALSE ){
#ifdef UNICODE
			char XX_setupedPPxA[MAX_PATH];
			char destdirA[MAX_PATH];

			UnicodeToAnsi(XX_setupedPPx, XX_setupedPPxA, MAX_PATH);
			XX_setupedPPxA[MAX_PATH - 1] = '\0';
			UnicodeToAnsi(destdir, destdirA, MAX_PATH);
			destdirA[MAX_PATH - 1] = '\0';
			if ( (GetFileAttributesA(XX_setupedPPxA) == BADATTR) ||
				 (GetFileAttributesA(destdirA) == BADATTR) ){
				FreeLibrary(hUN);
				return FALSE;
			}
			wsprintfA(buf, arcparam, XX_setupedPPxA, destdirA);
#else
			wsprintfA(buf, arcparam, XX_setupedPPx, destdir);
#endif
			if ( unarcfunc(NULL, buf, log, sizeof(log)) == 0 ){
				result = TRUE;
				tstrcpy(XX_upsrc, destdir);
				tstrcpy(XX_setupedPPx, MyPath);
			}else{
			#ifndef UNICODE
				SMessage(log);
			#endif
			}
		}
	}
	FreeLibrary(hUN);
	return result;
}

ERRORCODE UnarcZipfolder(void)
{
	TCHAR dir[MAX_PATH], buf[CMDLINESIZE];
	DWORD result;

	if ( CreateSourceDirectory(dir) == FALSE ) return ERROR_ACCESS_DENIED;

	wsprintf(buf, T("*checkupdate u \"%s\" \"%s\""), XX_setupedPPx, dir);
	result = Execute_ExtractMacro(buf, TRUE);

	if ( result == NO_ERROR ){
		tstrcpy(XX_upsrc, dir);
		tstrcpy(XX_setupedPPx, MyPath);
	}
	return result;
}

BOOL DownLoadPPx(BOOL usebeta)
{
	BOOL dosetup = FALSE;
	HMODULE hDLL;
	ThSTRUCT th;
	char *ptr, *lp;
	TCHAR dir[MAX_PATH], buf[CMDLINESIZE];
	#ifdef UNICODE
		WCHAR ARCHIVENAME[MAX_PATH];
	#else
		#define ARCHIVENAME (ptr + 2)
	#endif
	impGetImageByHttp DGetImageByHttp;

	hDLL = LoadLibrary(T(COMMONDLL));
	if ( hDLL == NULL ){
		SMessage(MessageStr[MSG_DLLLOADERROR]);
		return FALSE;
	}
	// ファイル一覧を取得
	GETDLLPROC(hDLL, GetImageByHttp);
	if ( DGetImageByHttp(T(TOROsWEB) T(PPxWebPage), &th) == FALSE ){
		ptr = NULL;
	}else{ // ファイル名を抽出
		ptr = strstr(th.bottom, "/ppxmes");
		if ( ptr != NULL ) *ptr = '\0';

		ptr = strstr(th.bottom, ARCNAME);
	}
	if ( ptr == NULL ){
		SMessage(MessageStr[MSG_SITEERROR]);
		goto fin;
	}

	if ( usebeta ){
		char *nextptr;

		nextptr = ptr;
		for (;;){ // 開発版を抽出
			nextptr = strstr(nextptr + 1, ARCNAME);
			if ( (nextptr == NULL) || (*(nextptr - 1) != '\n') ) break;
#ifndef UNICODE
			if ( *(nextptr + 5) == '6' ) continue;
#endif
			ptr = nextptr;
		}
	}

	// 既存ファイルを確認
	if ( (lp = strchr(ptr, '\r')) != NULL ) *lp = '\0';
	if ( (lp = strchr(ptr, '\n')) != NULL ) *lp = '\0';
	tstrcpy(dir, XX_setupedPPx);
	if ( dir[0] != '\0' ) tstrcat(dir, T("\\"));
	#ifdef UNICODE
		AnsiToUnicode(ptr + 2, ARCHIVENAME, TSIZEOF(ARCHIVENAME));
	#endif
	wsprintf(XX_setupedPPx, T("%s%s"), dir, ARCHIVENAME);
	if ( GetFileAttributes(XX_setupedPPx) != BADATTR ){
		wsprintf(buf, MessageStr[MSG_RELOADARCHIVE], ARCHIVENAME);
		if ( MessageBox(GetFocus(), buf, msgboxtitle,
				MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES ){
			goto fin;
		}
	}
	wsprintf(buf, T("*httpget \"") T(TOROsWEB) T("/%s\",\"%s\""), ARCHIVENAME, XX_setupedPPx);
	if ( Execute_ExtractMacro(buf, TRUE) == NO_ERROR ) dosetup = TRUE;
fin:
	FreeLibrary(hDLL);
	return dosetup;
}

#ifdef _WIN64
	#define STRUNZIP "UNZIP64.DLL"
	#define STR7ZIP  "7-ZIP64.DLL"
#else
	#define STRUNZIP "UNZIP32.DLL"
	#define STR7ZIP  "7-ZIP32.DLL"
#endif

/*
	/s[q] srcpath / zip		指定パスから現在パスへコピー
	/U[q] destpath			新しいSETUPを使ってdestpathへコピー
	/D[q] savepath			正式版をsavepathに保存して更新
	/B[q] savepath			開発版をsavepathに保存して更新
*/
BOOL AutoSetup(const TCHAR *param)
{
	TCHAR option, *p;
	TCHAR dir[MAX_PATH], buf[CMDLINESIZE];

	// パスを抽出
	option = TinyCharUpper(*param);
	param++;
	if ( *param == 'q' ){
		param++;
		XX_resultquiet = TRUE;
	}
	while ( *param == ' ' ) param++;
	if ( *param == '\"' ) param++;
	tstrcpy(XX_setupedPPx, param);
	if ( (p = tstrchr(XX_setupedPPx, '\"')) != NULL ) *p = '\0';

	if ( option == 'U' ){
		tstrcpy(XX_upsrc, MyPath);
		return AUTOSETUP_COPYDIALOG;
	}
	if ( option != 'S' ){
		if ( (option != 'D') && (option != 'B') ) return AUTOSETUP_FIN;
		if ( DownLoadPPx(option == 'B') == FALSE ) return AUTOSETUP_FIN;
	}

	if ( GetFileAttributes(XX_setupedPPx) & FILE_ATTRIBUTE_DIRECTORY ){
		// ディレクトリを指定した
		tstrcpy(dir, XX_setupedPPx);
	}else{
		if ( OSver.dwMajorVersion >= 6 ){
			wsprintf(buf, T("*checksignature !\"%s\""), XX_setupedPPx);
			if ( Execute_ExtractMacro(buf, TRUE) == ERROR_INVALID_DATA ){
				return AUTOSETUP_FIN;
			}
		}

		if ( UnarcFile(T("7-ZIP32.DLL"),
				"SevenZip", "x \"%s\" \"-o%s\" *") == FALSE ){
			if ( UnarcFile(T("UNZIP32.DLL"),
					"UnZip", "-x \"%s\" \"%s\\\" *.*") == FALSE ){
				if ( UnarcZipfolder() != NO_ERROR ){
					SMessage(MessageStr[MSG_UNZIP]);
					return AUTOSETUP_FIN;
				}
			}
		}
		if ( IsTrue(SelfUpdate()) ) return AUTOSETUP_FIN;
		return AUTOSETUP_COPYDIALOG;
	}
	wsprintf(buf, T("%s\\setup.exe"), dir);
	if ( GetFileAttributes(buf) == BADATTR ){	// 新規setup無し
		tstrcpy(XX_upsrc, dir);
		if ( SearchPPx() == FALSE ){
			if ( SelectDirectory(NULL, MessageStr[MSG_SELECTPPX],
					BIF_STATUSTEXT, SelInstPPxProc, XX_setupedPPx) == FALSE ){
				return AUTOSETUP_FIN;
			}
		}
		return AUTOSETUP_COPYDIALOG;
	}										// 新規setupで処理
	if ( IsTrue(SearchPPx()) ){
		wsprintf(buf + tstrlen(buf), T(" /U%c %s"),
				IsTrue(XX_resultquiet) ? 'q' : ' ',
				XX_setupedPPx);
	}
	Cmd(buf);
	return AUTOSETUP_FIN;
}
