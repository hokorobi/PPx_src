/*-----------------------------------------------------------------------------
	Paper Plane commandUI												Main
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#pragma hdrstop
#include "PPC_GVAR.C"	// グローバルの実体定義

BOOL ComboFix(PPCSTARTPARAM *psp);

const TCHAR RunAlone[] = T("-alone");

int firstfocus = 0; // コンボ起動時のフォーカス窓の指定位置

#define H_GestureConfig_count 4
GESTURECONFIG H_GestureConfig[] = {
	{ GID_ZOOM, GC_ZOOM, 0 },
	{ GID_PAN, GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY /* | GC_PAN_WITH_INTERTIA */, GC_PAN_WITH_SINGLE_FINGER_VERTICALLY},
//	{ GID_ROTATE, GC_ROTATE, 0},
	{ GID_TWOFINGERTAP, GC_TWOFINGERTAP, 0},
	{ GID_PRESSANDTAP, GC_PRESSANDTAP, 0},
};

#pragma argsused
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int result;
	PPCSTARTPARAM psp;
	UnUsedParam(hPrevInstance);UnUsedParam(lpCmdLine);

									// グローバル初期化
	hInst = hInstance;
	MainThreadID = GetCurrentThreadId();
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
	InitPPcGlobal();
	psp.show = nShowCmd;
	#ifdef UNICODE
	if ( LoadParam(&psp,NULL) == FALSE ){
		result = EXIT_FAILURE;
		goto fin;
	}
	#else
	if ( LoadParam(&psp,lpCmdLine) == FALSE ){
		result = EXIT_FAILURE;
		goto fin;
	}
	#endif

#if 0
	XMessage(NULL,NULL,XM_DbgLOG,T("PSP SingleProcess:%d Reuse:%d UseCmd:%d AllocCmd:%d alone:%d show:%d Focus:%c"),
		psp.SingleProcess,psp.Reuse,psp.UseCmd,psp.AllocCmd,psp.alone,psp.show,psp.Focus);
	{
		PSPONE *next;

		next = psp.next;
		while ( next != NULL ){
			if ( next->id.RegID[0] == '\0' ) break;
			XMessage(NULL,NULL,XM_DbgLOG,T("PSPone RegID:%s RegMode:%d Pair:%d Combo:%d Lock:%d Pane:%d Path:%s"),
				next->id.RegID, next->id.RegMode, next->id.Pair,
				next->combo.use, next->combo.dirlock, next->combo.pane,
				next->path);
			next = PSPONE_next(next);
		}
	}
#endif

	if ( X_ChooseMode != CHOOSEMODE_NONE ){
		X_sps = psp.SingleProcess = FALSE;
	}else{
		if ( X_combo ){
			X_sps = psp.SingleProcess = TRUE;
			if ( ComboFix(&psp) == FALSE ){
				result = EXIT_SUCCESS;
				goto fin;
			}
		}

		 if ( IsTrue(psp.SingleProcess) ){
			if ( psp.ComboID == '\0' ){
				if ( IsTrue(CallPPc(&psp,NULL)) ){
					result = EXIT_SUCCESS;
					goto fin;
				}
			}
		}
	}
	if ( IsTrue(psp.Reuse) ) ReuseFix(&psp);

	if ( X_combos[0] & CMBS_THREAD ) X_MultiThread = 0;

	result = PPcMain(&psp);
	// これ以降はメッセージボックスを表示することができない
fin:
	PPxWaitExitThread();	// /sps指定時、ここで待機
									// 終了処理
	if ( hPreviewWnd != NULL ) DestroyWindow(hPreviewWnd);

	if ( Combo.Report.hBrush != NULL ) DeleteObject(Combo.Report.hBrush);
	if ( CacheIcon.hImage != NULL ){
		DImageList_Destroy(CacheIcon.hImage);
	}
	FreeOverlayCom();
	DeleteCriticalSection(&SHGetFileInfoSection);
	DeleteCriticalSection(&FindFirstAsyncSection);
	PPxCommonCommand(NULL,0,K_CLEANUP);
	CoUninitialize();
	return result;
}

void FindParamComboTarget(PPCSTARTPARAM *psp, TCHAR ID, int subid, BOOL dirlock, int showpane)
{
	PSPONE newpspo,*pspo,*foundPspo = NULL;

	pspo = psp->next;
	if ( pspo != NULL ){
		while ( pspo->id.RegID[0] != '\0' ){
			if ( pspo->id.RegMode == PPXREGIST_IDASSIGN ){
				if ( (pspo->id.RegID[2] == ID) &&
					  ((ID != 'Z') || (pspo->id.SubID == subid)) ){	// 既に同じID指定のがある→特に設定すること無し
					pspo->combo.dirlock = dirlock;
					return;
				}
			}else{ // 未定義IDの窓があれば利用
				if ( pspo->combo.use != 0 ){
					foundPspo = pspo;
					// PPXREGIST_IDASSIGN 指定のが有るかもしれないので続行
				}
			}
			pspo = PSPONE_next(pspo);
		}
		if ( foundPspo != NULL ){
			foundPspo->id.RegID[0] = 'C';
			foundPspo->id.RegID[1] = '_';
			foundPspo->id.RegID[2] = ID;
			foundPspo->id.RegID[3] = '\0';
			foundPspo->id.RegMode = PPXREGIST_IDASSIGN;
			foundPspo->id.SubID = subid;
			foundPspo->combo.dirlock = dirlock;
			return;
		}
		psp->th.top -= TSTROFF(1); // 末端を除去
	}
	// 該当がないので追加登録
	// pspone をセット*
	newpspo.id.RegID[0] = 'C';
	newpspo.id.RegID[1] = '_';
	newpspo.id.RegID[2] = ID;
	newpspo.id.RegID[3] = '\0';
	newpspo.id.RegMode = PPXREGIST_IDASSIGN;
	newpspo.id.SubID = subid;
	newpspo.id.Pair = FALSE;
	newpspo.combo.use = X_combo;
	newpspo.combo.dirlock = dirlock;
	newpspo.combo.pane = showpane;
	newpspo.path[0] = '\0';
	ThAppend(&psp->th,&newpspo,ToSIZE32_T((char *)&newpspo.path[0] - (char *)&newpspo.id.RegID + sizeof(TCHAR)));
	ThAppend(&psp->th,NilStr,TSTROFF(1));

	psp->next = (PSPONE *)psp->th.bottom;
}

BOOL ComboFix(PPCSTARTPARAM *psp)
{
	TCHAR list[Panelistsize(Combo_Max_Base)], comboid[] = T("CBA");
	TCHAR *listptr;
	int showpane = PSPONE_PANE_DEFAULT;

	if ( psp->ComboID != '\0' ){
		comboid[2] = (TCHAR)psp->ComboID;
		if ( comboid[2] == '@' ){
			// ID を確保しないが、未使用 ID を得る
			PPxRegist(NULL, comboid, PPXREGIST_COMBO_IDASSIGN);
			psp->ComboID = comboid[2];
		}
		if ( IsTrue(psp->Reuse) ){
			HWND hAloneWnd;

			hAloneWnd = PPcGetWindow(psp->ComboID - 'A',CGETW_GETCOMBOHWND);
			if ( hAloneWnd != NULL  ) { // 既存aloneを使用
				CallPPc(psp, hAloneWnd);
				if ( (psp->show != SW_SHOWNOACTIVATE) &&
					 (psp->show != SW_SHOWMINNOACTIVE) ){
					SetForegroundWindow(hAloneWnd);
				}
				return FALSE;
			}
		}
	}else if ( PPxCombo(BADHWND) != NULL ){
		// 普通の一体化時で、既に一体化窓があるときは、
		// 既存一体化窓への追加なので何もしない
		return TRUE;
	}else if ( IsTrue(psp->Reuse) ){
		// -r 時、既に別のPPcがあるなら、CallPPc をさせる
		if ( PPcGetWindow(0,CGETW_GETFOCUS) != NULL ) return TRUE;
	}

	if ( X_combos[1] & CMBS1_NORESTORETAB ) return TRUE;
	if ( psp->usealone ) return TRUE; // alone 時はペインの再現を行わない
	// 元の状態を取得
	list[0] = '\0';
	GetCustTable(T("_Path"),comboid,list,sizeof(list));
	if ( (list[0] == '\0') || (list[1] == '\0') || (list[2] == '\0') ){
		return TRUE;
	}

	if ( list[0] != '?' ) psp->Focus = list[0];
	listptr = list + 1;
	while ( Islower(*listptr) ) listptr++; // Z小文字部分をスキップ
	// listptr は、右側窓を示す
	if ( Isupper(*listptr) ) listptr++;
	while ( Islower(*listptr) ) listptr++; // Z小文字部分をスキップ
	// listptr は、ペイン・タブ並びの先頭を示す

	{ //Z が複数ある場合、無効にする
		TCHAR *Zfirst = tstrchr(listptr,'Z');

		if ( Zfirst != NULL ){
			TCHAR *Znext;

			Znext = Zfirst;
			for ( ;; ) {
				int i;

				Znext = tstrchr(Znext + 1,'Z');
				if ( Znext == NULL ) break;
				for ( i = 1 ; ; i++ ){
					if ( Islower(*(Zfirst + i)) ){
						if ( *(Zfirst + i) == *(Znext + i) ){
							continue; // 同じsubid...継続
						}else{ // subidがちがう...次へ
							break;
						}
					}else{
						if ( !Islower(*(Znext + i)) ){
							*Znext = ' '; // 同時に終わり... 重複なので無効
							break;
						}else{ // subidがちがう...次へ
							break;
						}
					}
				}
			}
		}
	}

	if ( X_combos[0] & CMBS_TABEACHITEM ){
		showpane = PSPONE_PANE_SETPANE;
		if ( *listptr == '-' ){
			listptr++;
		}else{ // CMBS_TABEACHITEM 用の設定でないので並び順を廃棄
			*listptr = '\0';
		}
	}else{
		if ( *listptr == '-' ){ // CMBS_TABEACHITEM 用の設定なので並び順を廃棄
			*listptr = '\0';
		}
	}
	ComboPaneLayout = PPcStrDup(list);

	if ( psp->next != NULL ){ // IDを割り振っていないコマンドラインパラメータにIDを割り当てる
		int sindex = 0;
		TCHAR *tmplist;
		PSPONE *pspo = psp->next;

		while ( pspo->id.RegID[0] != '\0' ){
			if ( (pspo->id.RegMode <= PPXREGIST_IDASSIGN) &&
				 (pspo->id.RegID[2] == '\0') ){ // ID を割り振っていない
				tmplist = listptr;
				for ( ; *tmplist != '\0' ; ){
					TCHAR ID;
					int panesubid;

					ID = *tmplist++;
					if ( !Isupper(ID) ) continue;

					if ( Islower(*tmplist) ){ // subid(ComboID)有り
						panesubid = 0;
						tmplist++; // ComboID skip
						while ( Islower(*tmplist) ){
							panesubid = (panesubid * 26) + (*tmplist++ - 'a');
						}
					}else{
						panesubid = -1;
					}

					if ( *tmplist == '$' ) tmplist++;

					if ( Isdigit(*tmplist) ){
						int showid;

						showid = 0;
						while ( Isdigit(*tmplist) ){
							showid = (showid * 10) + (*tmplist++ - '0');
						}
						if ( sindex == showid ){
							pspo->id.RegID[2] = ID;
							pspo->id.RegID[3] = '\0';
							pspo->id.SubID = panesubid;
							pspo->id.RegMode = PPXREGIST_IDASSIGN;
							break;
						}
					}
					while ( *tmplist && !Isupper(*tmplist) ) tmplist++;
				}
				sindex++;
			}
			pspo = PSPONE_next(pspo);
		}
	}

	// 元の状態を再現する為の一覧を作成する
	for ( ; *listptr != '\0' ; ){
		TCHAR ID;
		BOOL dirlock;
		int panesubid;

		ID = *listptr++;
		if ( !Isupper(ID) ) continue;

		if ( Islower(*listptr) ){ // subid有り
			panesubid = 0;
			listptr++; // ComboID部分をスキップ
			while ( Islower(*listptr) ){
				panesubid = (panesubid * 26) + (*listptr++ - 'a');
			}
		}else{
			panesubid = -1;
		}

		if ( *listptr == '$' ){
			listptr++;
			dirlock = TRUE;
		}else{
			dirlock = FALSE;
		}
		while ( Isdigit(*listptr) ) listptr++;

		FindParamComboTarget(psp, ID, panesubid, dirlock, showpane);

		if ( *listptr == '-' ){
			listptr++;
			if ( X_combos[0] & CMBS_TABEACHITEM ) showpane++;
		}

		while ( *listptr && !Isupper(*listptr) ) listptr++; // 認識できない内容をスキップ
	}
#if 0 // 整形済み PSPONE一覧表示
	{
		PSPONE *pspo;
		int sindex;

		pspo = psp->next;
		sindex = 0;
		while ( pspo->id.RegID[0] ){
			XMessage(NULL,NULL,XM_DbgDIA,T("index=%d   ID=%s.%d"),sindex++,pspo->id.RegID,pspo->id.SubID);
			pspo = PSPONE_next(pspo);
		}
	}
#endif
	return TRUE;
}

void USEFASTCALL PPCuiWithPathForLock(PPC_APPINFO *cinfo,const TCHAR *path)
{
	TCHAR cmdline[CMDLINESIZE];

	if ( X_combos[0] & CMBS_REUSETLOCK ){
		if ( SendMessage(cinfo->hComboWnd,WM_PPXCOMMAND,KCW_pathfocus,(LPARAM)path) == (SENDCOMBO_OK + 1) ){
			return;
		}
	}

	if ( cinfo->combo ){
		if ( cinfo->info.hWnd != hComboFocus ){
			wsprintf(cmdline,T("\"%s\" -noactive"),path);
		}else{
			wsprintf(cmdline,T("\"%s\""),path);
		}
		CallPPcParam(Combo.hWnd,cmdline);
		return;
	}
	PPCuiWithPath(cinfo->info.hWnd,path);
}

void RunNewPPc(PPCSTARTPARAM *psp,MAINWINDOWSTRUCT *mws)
{
	DWORD tmp;

#ifndef _WIN64 // ユーザ空間が狭くなったら警告(Win64ではユーザ空間がWin8:8T/Win8.1:128Tなので実質警告不要)
	MEMORYSTATUS mstate;

	mstate.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&mstate);

	if ( mstate.dwAvailVirtual < (500 * MB) ){
		XMessage(NULL,NULL,XM_ImWRNld,MemWarnStr);
	}
#endif
	if ( X_MultiThread == 0 ){
		if ( IsTrue(PPxRegisterThread(NULL)) ){
			CreatePPcWindow(psp,mws);
			return;
		}
		// PPcMain を通さずに初期化されている状態？ 将来削除可能。
		PPxCommonExtCommand(K_SENDREPORT,(WPARAM)T("RunNewPPc"));
		return;
	}

	CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)PPcMain,psp,0,&tmp));
}

int GetPPcSubID(const TCHAR *idstr, TCHAR *dest)
{
	const TCHAR *strptr;

	strptr = idstr;
	if ( Isupper(*strptr) ){
		*dest++ = *strptr++;
		while ( Islower(*strptr) ) *dest++ = *strptr++;
	}
	*dest = '\0';
	return strptr - idstr;
}

BOOL CreatePPcWindow(PPCSTARTPARAM *psp,MAINWINDOWSTRUCT *mws)
{
	BOOL usepath = FALSE;
	DWORD tmp;
	TCHAR focusID[REGEXTIDSIZE];
	BOOL lastinit = TRUE;
	PPC_APPINFO *cinfo;
	int showpane = PSPONE_PANE_DEFAULT;
	int select = FALSE;

	focusID[0] = '\0';
	if ( (psp != NULL) && (psp->next != NULL) ){
		showpane = psp->next->combo.pane;
		select = psp->next->combo.select;
	}

	cinfo = (PPC_APPINFO *)HeapAlloc( hProcessHeap,HEAP_ZERO_MEMORY,sizeof(PPC_APPINFO));
	if ( cinfo == NULL ){
		XMessage(NULL,NULL,XM_GrERRld,T("mem alloc error"));
		return FALSE;
	}
	// cinfo は 0 fill 済み
	//********** ここで psp->next は次の pspo を示すようになる
	if ( psp != NULL ){
		cinfo->combo = ( psp->next != NULL ) ? psp->next->combo.use : X_combo;
		cinfo->WinPos.show = (BYTE)psp->show;
	}else{
		cinfo->combo = X_combo;
		cinfo->WinPos.show = SW_SHOWDEFAULT;
	}

	if ( cinfo->combo ){
		cinfo->combo = -1; // Size 変更通知を無効にする
		if ( Combo.hWnd == NULL ){ // Combo を新規起動→左端をフォーカス指定
			firstfocus = 1;
		}
		cinfo->hComboWnd = InitCombo(psp);
		if ( cinfo->hComboWnd == NULL ) return FALSE;
	}

	if ( RegisterID(cinfo, psp, &usepath) == FALSE ){
		HeapFree(hProcessHeap, 0, cinfo);
		return FALSE;
	}

	InitPPcWindow(cinfo, usepath);
	if ( cinfo->swin & SWIN_WBOOT ){
		if ( !(psp && psp->next && psp->next->id.RegID[0]) ){
			// 後続の指定が無ければ、連結対象を開く
			BootPairPPc(cinfo);
		}
	}
	InitCli(cinfo);

	if ( cinfo->combo == 0 ){
		if ( cinfo->WinPos.pos.left == (int)CW_USEDEFAULT ){ // 幅が未定義なら調節
			// ShowWindow より前に指定する必要有り
			PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_raw | K_a | K_F6,0);
		}
		ShowWindow(cinfo->info.hWnd,cinfo->WinPos.show);
		read_entry(cinfo,RENTRY_READ);
	}
	// X_IME == 1 のときは、WM_SETFOCUS で IMEOFF
	if ( X_IME == 2 ) PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_IMEOFF,0);
										// メインウインドウの表示を更新 -------
	PPxRegist(cinfo->info.hWnd,cinfo->RegID,PPXREGIST_SETHWND);	// 正式登録

	cinfo->hSubThread = CreateThread(NULL,0,
			(LPTHREAD_START_ROUTINE)SubThread,cinfo,0,&tmp);

	dd_init(cinfo);
										// フォーカスの再設定
	if ( cinfo->combo ){
	// 一体時
		WPARAM keycmd;

		if ( X_combos[0] & CMBS_DEFAULTLOCK ) cinfo->ChdirLock = TRUE;

		keycmd = ((cinfo->WinPos.show != SW_SHOWNOACTIVATE) ?
						KCW_entry : (KCW_entry + KCW_entry_NOACTIVE)) +
				(showpane + 1) * KCW_entry_DEFPANE;
		if ( select ) setflag(keycmd,KCW_entry_SELECTNA);

		//↓２枚目以降は計算されていないことがあるので対策
		WmWindowPosChanged(cinfo);
		SendMessage(cinfo->hComboWnd,WM_PPXCOMMAND,
				keycmd,(LPARAM)cinfo->info.hWnd); // 内部でFocusいじるのでSend
	}else if ( (cinfo->swin & SWIN_WBOOT) && (cinfo->WinPos.show != SW_SHOWNOACTIVATE) ){
	// 連結時
		HWND PairHWnd;
		DWORD flag;

		flag = (cinfo->swin & SWIN_BFOCUES) | (cinfo->RegID[2] & PAIRBIT);
		// アクティブA..現在B/アクティブB..現在A なら反対へ
		if ( (flag == 0) || (flag == (PAIRBIT | SWIN_BFOCUES)) ){
			PairHWnd = GetJoinWnd(cinfo);
			if ( PairHWnd != NULL ) ForceSetForegroundWindow(PairHWnd);
		}else{
			ForceSetForegroundWindow(cinfo->info.hWnd);
		}
	}
										// 初期化が完了したのでFixを有効
	resetflag(cinfo->swin,SWIN_BUSY);
	IOX_win(cinfo,TRUE);

	if ( (!cinfo->combo || !(X_combos[0] & CMBS_COMMONTREE)) && cinfo->XC_tree.mode ){
		PPC_Tree(cinfo,cinfo->XC_tree.mode);
	}

	if ( !(psp && psp->next && psp->next->id.RegID[0]) && // ←別のを呼び出しなし
			(cinfo->swin & SWIN_JOIN) && CheckReady(cinfo) ){	// 連結
		if ( GetJoinWnd(cinfo) != NULL ) JoinWindow(cinfo);
	}
	// 次のPPcを起動 / psp を解放
	if ( psp != NULL ){
		if ( psp->Focus != 0 ){
			focusID[0] = 'C';
			if ( ComboPaneLayout == NULL ){
				focusID[1] = psp->Focus;
				focusID[2] = '\0';
			}else{
				GetPPcSubID(ComboPaneLayout, focusID + 1);
			}
		}
		if ( psp->next != NULL ){
			if ( psp->next->id.RegID[0] ){	// 次を起動
				lastinit = FALSE;
				RunNewPPc(psp, mws);
			}else{ // 終わり / cmd 用のメモリを確保してなければここで解放
				psp->next = NULL;
				if ( psp->AllocCmd == FALSE ){ // cmd は静的なので PSPONE 解放
					ThFree(&psp->th);
				}
			}
		}
	}
	// これ以降は psp の使用禁止 **********************************************
	if ( (cinfo->docks.t.hWnd != NULL) ||
		 (cinfo->docks.b.hWnd != NULL) ||
		 (cinfo->hHeaderWnd != NULL) /*|| (cinfo.hToolBarWnd != NULL)*/ ){
		WmWindowPosChanged(cinfo); // 位置を再調整する
	}

	if ( cinfo->combo ){	// 一体化完了後読み込みをしたいがまだ効果が薄すぎ
		read_entry(cinfo,RENTRY_READ);
	}

	if ( CountCustTable(T("_Delayed")) > 0 ){
		PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,KC_DODO,0);
	}
	if ( cinfo->FirstCommand != NULL ){
		PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_FIRSTCMD,0);
	}
	if ( IsExistCustTable(StrKC_main,T("FIRSTEVENT")) ){
		PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_E_FIRST,0);
	}
	if ( lastinit && firstinit ){
		if ( cinfo->combo ){
			if ( Combo.BaseCount < X_mpane.min ){
				MorePPc(NULL,&cinfo->mws);
			}else{
				HWND hWnd;

				hWnd = firstfocus ? NULL : cinfo->info.hWnd;
				firstfocus = 0;
				firstinit = 0;
				if ( focusID[0] != '\0' ){
					HWND hFocusWnd;

					hFocusWnd = GetHwndFromIDCombo(focusID);
					if ( hFocusWnd != NULL ) hWnd = hFocusWnd;
				}
				PostMessage(cinfo->hComboWnd,WM_PPXCOMMAND,KCW_ready,(LPARAM)hWnd);
			}
		}
	}

//	cinfo->mws.hWnd = cinfo->info.hWnd;
	cinfo->mws.ThreadID = GetCurrentThreadId();
	cinfo->mws.NowClose = FALSE;
	cinfo->mws.cinfo = cinfo;
	UsePPx();
	cinfo->mws.next = mws->next;
	mws->next = &cinfo->mws;
	FreePPx();
	DxSetMotion(cinfo->DxDraw,DXMOTION_NewWindow);

#if XTOUCH
	{
		HMODULE hUser32;

		hUser32 = GetModuleHandle(StrUser32DLL);
		GETDLLPROC(hUser32,RegisterTouchWindow);
		if ( DRegisterTouchWindow != NULL ){
			if ( DRegisterTouchWindow(cinfo->info.hWnd,0) ){
				SetPopMsg(cinfo,POPMSG_MSG,T("enable touch"));
			}
			GETDLLPROC(hUser32,GetTouchInputInfo);
			GETDLLPROC(hUser32,CloseTouchInputHandle);
		}
	}
#endif
	{
		HMODULE hUser32;

		hUser32 = GetModuleHandle(StrUser32DLL);
		GETDLLPROC(hUser32,GetGestureInfo);
	}

	if ( XC_page == 0 ){
		HMODULE hUser32;

		hUser32 = GetModuleHandle(StrUser32DLL);
		GETDLLPROC(hUser32,SetGestureConfig);
		if ( DSetGestureConfig != NULL ){
			DSetGestureConfig(cinfo->info.hWnd,0,H_GestureConfig_count,H_GestureConfig,sizeof(GESTURECONFIG));
		}
	}

	if ( cinfo->combo ){ // サイズ通知を有効にする
		PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,KCW_ready,0);
	}
	return TRUE;
}

void PPCui(HWND hWnd, const TCHAR *cmdline)
{
	TCHAR param[CMDLINESIZE], dir[VFPS];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if ( cmdline != RunAlone ){
		if ( X_sps || X_combo ){
			MorePPc(cmdline, &MainWindows);
			return;
		}
	}

	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= 0;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;

	GetModuleFileName(hInst, dir, VFPS);
#ifdef WINEGCC
	tstrcpy(tstrrchr(dir, '\\') + 1, T(PPCEXE)); //Z:\...\PPC.EXE を Z:\...\ppc に修正
#endif
	if ( cmdline ){
		wsprintf(param, T("\"%s\" %s"), dir, cmdline);
	}else{
		wsprintf(param, T("\"%s\""), dir);
	}
											// カレントディレクトリを作成
	*tstrrchr(dir, '\\') = '\0'; // 最後は「\PPC.EXE」なので、漢字対策せず

	if ( IsTrue(CreateProcess(NULL, param, NULL, NULL, FALSE,
			CREATE_DEFAULT_ERROR_MODE, NULL, dir, &si, &pi)) ){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}else{
		PPErrorBox(hWnd, dir, PPERROR_GETLASTERROR);
	}
}

void MorePPc(const TCHAR *cmdline,MAINWINDOWSTRUCT *mws)
{
	DWORD nexttop C4701CHECK,cmdtop C4701CHECK;
	PPCSTARTPARAM *psp = NULL,defpsp;
	ThSTRUCT th;

	if ( (cmdline == NULL) && X_combo && (X_combos[0] & CMBS_TABMAXALONE) && (Combo.ShowCount >= X_mpane.max) ){
		PPCui(NULL,RunAlone);
		return;
	}

	if ( cmdline != NULL ){
		defpsp.show = SW_SHOW;

		LoadParam(&defpsp,cmdline);

//		if ( defpsp.alone ){ // 新規プロセスが必要なオプション
//			return FALSE;
//		}

		ThInit(&th);
		ThAppend(&th,&defpsp,sizeof(PPCSTARTPARAM));
		if ( defpsp.next != NULL ){
			nexttop = th.top;
			ThAppend(&th,defpsp.next,defpsp.th.top);
			ThFree(&defpsp.th);
		}
		if ( IsTrue(defpsp.UseCmd) ){
			cmdtop = th.top;
			((PPCSTARTPARAM *)th.bottom)->AllocCmd = TRUE;
			((PPCSTARTPARAM *)th.bottom)->cmd = (const TCHAR *)(DWORD_PTR)th.top;
			ThAddString(&th,defpsp.cmd);
		}
		psp = (PPCSTARTPARAM *)th.bottom;
		psp->th = th;
		if ( defpsp.next != NULL ){
			psp->next = (PSPONE *)ThPointer(&th,nexttop);  // C4701ok
		}
		if ( IsTrue(defpsp.UseCmd) ){
			psp->cmd = ThPointerT(&th,cmdtop);  // C4701ok
		}
	}
	RunNewPPc(psp,mws);
}

// WM_COPYDATA 経由で PPc 操作を行う
void SendCallPPc(COPYDATASTRUCT *copydata)
{
#define PSP ((PPCSTARTPARAM *)th.bottom)
	ThSTRUCT th;

	ThInit(&th);
	ThAppend(&th,copydata->lpData,copydata->cbData);
	ReplyMessage(TRUE);

	PSP->th = th;
	// ポインタをオフセットから実際の値に変換
	if ( IsTrue(PSP->UseCmd) ){
		PSP->AllocCmd = TRUE;
		PSP->cmd = ThPointerT(&th,(size_t)(PSP->cmd));
	}
	if ( PSP->next != NULL ){
		PSP->next = (PSPONE *)ThPointer(&th,sizeof(PPCSTARTPARAM));
		if ( IsTrue(PSP->Reuse) ) ReuseFix(PSP); // /R なら ID割当て
	}
	RunNewPPc(PSP,&MainWindows);
#undef PSP
}

#pragma argsused
DWORD_PTR USECDECL PPxGetDummyIInfo(PPXAPPINFO *info,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	UnUsedParam(info);

	if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
	return 0;
}

BOOL FixClosedPPc(PPC_APPINFO *cinfo, BOOL final)
{
	if ( final == FALSE ){				// SubThread が終わらなかったら中止
		if ( (cinfo->Ref > 0) || (WAIT_TIMEOUT == WaitForSingleObject(cinfo->hSubThread, 0)) ){
			return FALSE;
		}
	} else{								// SubThread が終わらなかったら強制終了
		// ここで待ってもサブスレッドが進まないことが多い...PreClosePPcで進める
		if ( WAIT_TIMEOUT == WaitForSingleObject(cinfo->hSubThread, 100) ){
			XMessage(NULL, NULL, XM_DbgLOG, T("FixClosedPPc timeout : %c"), cinfo->RegID[2]);
			PPxCommonExtCommand(K_THREADUNREG, cinfo->SubThreadID);
		#pragma warning(suppress: 6258) // 強制終了を意図
			TerminateThread(cinfo->hSubThread, 0);
			WaitForSingleObject(cinfo->hSubThread, 100);
		}
	}
	CloseHandle(cinfo->hSubThread);

	// サブスレッド終了後に解放する必要があるリソース
	DeleteCriticalSection(&cinfo->edit.section);
	TM_kill(&cinfo->e.INDEXDATA);
	TM_kill(&cinfo->e.CELLDATA);

	if ( cinfo->ColumnExtDlls.bottom != NULL ){
		GetColumnExtMenu(&cinfo->ColumnExtDlls, NULL, NULL, 0); // thEcdata 解放...スレッド揃える必要がある？
	}
	cinfo->info.Function = (PPXAPPINFOFUNCTION)PPxGetDummyIInfo;
	cinfo->info.hWnd = (HWND)(DWORD_PTR)0xfffffffe;

	if ( cinfo->Ref <= 0 ) HeapFree(hProcessHeap, 0, cinfo);
	// ●↑1.1x cinfo->Ref != 0 のときは現在、意図的にリークさせて落ちないようにする
	return TRUE;
}

/*===========================================================================*/
BOOL FixPPcWindowList(MAINWINDOWSTRUCT *mws, BOOL final)
{
	MAINWINDOWSTRUCT *nowmws, *oldmws, *nextmws;
	BOOL havewindow = FALSE;
	DWORD ThreadID = GetCurrentThreadId();

	RequestDestroyFlag = 0;
	UsePPx();
	nowmws = mws;
	nextmws = nowmws->next;
	for ( ; ; ){
		oldmws = nowmws;
		nowmws = nextmws;
		if ( nowmws == NULL ) break;
		nextmws = nowmws->next;

		if ( IsTrue(nowmws->DestroryRequest) && (nowmws->cinfo != NULL) &&
			(nowmws->cinfo->Ref <= 1) ){
			DestroyWindow(nowmws->cinfo->info.hWnd);
		}

		if ( nowmws->ThreadID != ThreadID ) continue;

		if ( nowmws->NowClose < 2 ){
			havewindow = TRUE;
		} else if ( (nowmws->next != NULL)/*最後でないか*/ ||
			(oldmws == mws)/*残りの１つ(first thread)か*/ ){
			if ( nowmws->cinfo != NULL ){
				//	nowmws->hWnd = NULL;
				// ※↓nowmws 自身がここで消滅
				if ( FixClosedPPc(nowmws->cinfo, final) == FALSE ){
					RequestDestroyFlag = 1;
					continue;
				}
			}
			oldmws->next = nextmws;
			nowmws = oldmws; // nowmws は廃棄されたので元に戻す
		}
	}
	FreePPx();
	return havewindow;
}

