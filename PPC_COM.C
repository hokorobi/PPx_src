/*-----------------------------------------------------------------------------
	Paper Plane cUI												各種コマンド
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

const TCHAR StrInvokeFind[] = T("find");
const TCHAR StrInvokeProperties[] = T("properties");

ERRORCODE PPXAPI PPcCommand(PPC_APPINFO *cinfo, WORD key)
{
	cinfo->FreezeType = (int)key;
	if ( !(key & K_raw) ) return ExecKeyCommand(&PPcExecKey, &cinfo->info, key);

	resetflag(key, K_raw);	// エイリアスビットを無効にする
	DEBUGLOGC("PPcCommand:%04x", key);
	switch ( key ){
//----------------------------------------------- 例外テスト用
#if 0
/*
case '!': // 一般の例外 ... PPxでは無視
	RaiseException(0x87654321, 0, 0, NULL);
	break;
case '\"': // 書き込み違反
	void t(void){ t(); }
	t();
	break;
*/
case '#': // 書き込み違反
	*(BYTE *)0x1234 = 0;
	break;
case '$': // 読み込み違反
	key = *(BYTE *)0x1234;
	break;
case '%': { // 0割
	int z = 0;
	TCHAR a[1];
	wsprintf(a, NilStr, 1/z);
	break;
}
case '&': // 報告
	PPxCommonExtCommand(K_SENDREPORT, (WPARAM)T("テスト"));
	break;
#endif
//----------------------------------------------- D&D
case K_c | 'D':
	AutoDDDialog(cinfo, NULL, AUTODD_HOOK | AUTODD_LEFT);
	break;
//----------------------------------------------- Attribute
case 'A':
	return PPC_attribute(cinfo);
//----------------------------------------------- Mark All(F+D)
case K_end:
case K_c | 'A':
	cinfo->MarkMask = MARKMASK_DIRFILE;
	PPC_AllMark(cinfo);
	break;
//----------------------------------------------- Mark
case K_s | K_c | 'A':
	setflag(cinfo->DrawTargetFlags, DRAWT_ENTRY);
	cinfo->MarkMask = MARKMASK_DIRFILE;
	CellMark(cinfo, cinfo->e.cellN,  MARK_REVERSE);
	RefleshCell(cinfo, cinfo->e.cellN);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	break;
//----------------------------------------------- Copy
case 'C':
	PPcFileOperation(cinfo, FileOperationMode_Copy, NULL, NULL);
	break;
//----------------------------------------------- Copy
case K_s | 'C':
	return PPC_ExplorerCopy(cinfo, FALSE);
//----------------------------------------------- Clip Files
case K_c | 'C':
	ClipFiles(cinfo, DROPEFFECT_COPY | DROPEFFECT_LINK, CFT_FILE | CFT_TEXT);
	SetPopMsg(cinfo, POPMSG_MSG, MES_CPDN);
	break;
//----------------------------------------------- Clip Directory
case K_c | K_s | 'C':
	ClipDirectory(cinfo);
	break;
//----------------------------------------------- Delete
case 'D':
	return DeleteEntrySH(cinfo);
//----------------------------------------------- Delete
case K_s | 'D':
	return DeleteEntry(cinfo);
//----------------------------------------------- Erase Entry from List
case K_c | K_s | 'D':
	return EraseListEntry(cinfo);
//----------------------------------------------- PPE
case K_s | 'E':
	OpenFileWithPPe(cinfo);
	break;
//----------------------------------------------- Explorer Here
case K_c | 'E':
	OpenExplorer(cinfo);
	break;
//----------------------------------------------- Find
case 'F':
	return MaskEntry(cinfo, X_dsst[1], NULL, NULL);
//----------------------------------------------- wildcard
case K_s | 'F':
	return MaskEntry(cinfo, DSMD_NOMODE, NULL, NULL);
//----------------------------------------------- File find
case K_c | 'F':
case K_F3:
	if ( (OSver.dwMajorVersion >= 7) ||
		 ((OSver.dwMajorVersion == 6) && (OSver.dwMinorVersion > 0)) ||
		 (PPcSHContextMenu(cinfo, StrThisDir, StrInvokeFind) == FALSE) ){
		return WhereIs(cinfo, 0);
	}
	break;
//----------------------------------------------- Swap
case 'G':
	SwapWindow(cinfo);
	break;
//----------------------------------------------- DiskInformation
case K_s | K_F2:
case 'I':
	PPC_diskinfo(cinfo->info.hWnd);
	break;
//----------------------------------------------- FileInformation
case K_c | 'I':
	FileInfo(cinfo);
	break;
//----------------------------------------------- Sync FileInformation
case K_s | K_c | 'I':
	SyncFileInfo(cinfo);
	break;
//----------------------------------------------- Jump/Dialog type
case 'J':
	SearchEntry(cinfo);
	break;
//----------------------------------------------- Jump/Mode type
case K_s | 'J':
	InitIncSearch(cinfo, '\0');
	cinfo->IncSearchTick = INCSEARCH_NOTICK;
	ShowSearchState(cinfo);
	break;
//----------------------------------------------- maKe dir
case 'K':
	return PPC_MakeDir(cinfo);
//----------------------------------------------- maKe entry
case K_s | 'K':
	return PPC_MakeEntry(cinfo);
//----------------------------------------------- Logdisk/Goto Directory
case 'L':
	return LogDisk(cinfo);
case K_c | 'G':
	return LogBarDisk(cinfo);
//----------------------------------------------- Logdrive
case K_s | 'L':
	return PPC_DriveJump(cinfo, FALSE);
//----------------------------------------------- Redraw
#ifdef USEDIRECTX
case K_v | VK_SCROLL:
	ResetDxDraw(cinfo->DxDraw);
#endif
case K_c | 'L':
	Repaint(cinfo);
	break;
//----------------------------------------------- Move
case 'M':
	PPcFileOperation(cinfo, FileOperationMode_Move, NULL, NULL);
	break;
//----------------------------------------------- Move
case K_s | 'M':
	return PPC_ExplorerCopy(cinfo, TRUE);
//----------------------------------------------- PPV
case K_s | 'V':
case 'N':
	PPcPPv(cinfo);
	break;
//----------------------------------------------- Compare
case 'O':
	return PPcCompare(cinfo, 0);
//----------------------------------------------- Comment option
case K_s | 'O':
	return CommentCommand(cinfo, NULL);
//----------------------------------------------- Quit PPC
case K_esc:			// [ESC]
	if ( PMessageBox(cinfo->info.hWnd, MES_QPPC, T("Quit"), MB_APPLMODAL |
		 MB_OKCANCEL | MB_DEFBUTTON1 | MB_ICONQUESTION) != IDOK ){
		 return ERROR_CANCELLED;
	}
// &F4 へ続く
//----------------------------------------------- Quit PPC
case K_a | K_F4:
	if ( cinfo->combo ){
		PostMessage(cinfo->hComboWnd, WM_CLOSE, 0, 0);
		break;
	}
// 'Q' へ続く
case 'Q':
	PostMessage(cinfo->info.hWnd, WM_CLOSE, 0, 0);
	break;

//----------------------------------------------- Rename
case K_F2:
case 'R':
	return PPC_Rename(cinfo, FALSE);
case K_c | 'R':
	return PPC_Rename(cinfo, TRUE);
//----------------------------------------------- Rename with mark
case K_s | 'R':
	PPcFileOperation(cinfo, FileOperationMode_Rename, NULL, NULL);
	break;
//----------------------------------------------- Sort
case 'S':
	return SortKeyCommand(cinfo, 0, NULL);
//----------------------------------------------- Hold Sort
case K_s | 'S':
	return SortKeyCommand(cinfo, 1, NULL);
//----------------------------------------------- Tree (once)
case K_F4:
case 'T':
	PPC_Tree(cinfo, PPCTREE_SELECT);
	if ( cinfo->hTreeWnd != NULL ) SetFocus(cinfo->hTreeWnd);
	break;
//----------------------------------------------- Tree
case K_s | 'T':
	PPC_Tree(cinfo, PPCTREE_SYNC);
	if ( cinfo->hTreeWnd != NULL ) SetFocus(cinfo->hTreeWnd);
	break;
//----------------------------------------------- Unpack
case 'U':
	return PPC_Unpack(cinfo, NULL);
case K_s | 'U':
	return UnpackMenu(cinfo);
//----------------------------------------------- PPv
case 'V':
	PPC_View(cinfo);
	break;
//----------------------------------------------- Paste
case K_c | 'V':
	PPcPaste(cinfo, FALSE);
	break;
//----------------------------------------------- Short cut
case K_c | K_s | 'V':
	PPcPaste(cinfo, TRUE);
	break;
//----------------------------------------------- Write
case 'W':
	return PPC_WriteDir(cinfo);
//----------------------------------------------- Write Comment
case K_s | 'W':
	WriteComment(cinfo, NULL);
	break;
//----------------------------------------------- Where is
case K_c | 'W':
	return WhereIs(cinfo, 0);
//----------------------------------------------- Where is archive
case K_c | K_s | 'W':
	return WhereIs(cinfo, 1);
//----------------------------------------------- Cut Files
case K_c | 'X':
	ClipFiles(cinfo, DROPEFFECT_MOVE, CFT_FILE | CFT_TEXT);
	SetPopMsg(cinfo, POPMSG_MSG, MES_CUTR);
	break;
//----------------------------------------------- sHell
case 'H':
	return ExecuteCommandline(cinfo);
//----------------------------------------------- Execute
case 'X':
	return ExecuteEntry(cinfo);
//----------------------------------------------- PPc(hold)
case 'Y':
	ViewOnCursor(cinfo, PPV_NOFOREGROUND | cinfo->NormalViewFlag);
	break;
//----------------------------------------------- PPc(hold sw)
case K_s | 'Y':
	SetSyncView(cinfo, cinfo->SyncViewFlag ? 0 : 1);
	break;
//----------------------------------------------- exec
case 'Z':
	PPcShellExecute(cinfo);
	break;
//----------------------------------------------- Path Jump
case '0':
	return PPC_PathJump(cinfo);
//----------------------------------------------- AllMark
case K_home:	// [home]
case '*':
	cinfo->MarkMask = MARKMASK_FILE;
	PPC_AllMark(cinfo);
	break;
//----------------------------------------------- AddMark
case '+':
	return PPC_FindMark(cinfo, NULL, MARK_CHECK);
//----------------------------------------------- DelMark
case '-':
	return PPC_FindMark(cinfo, NULL, MARK_REMOVE);
//----------------------------------------------- Reload
case K_F5:
case '.':
	PPcReload(cinfo);
	break;
//----------------------------------------------- Update
case K_c | K_F5:
	read_entry(cinfo, RENTRY_SAVEOFF | RENTRY_UPDATE | RENTRY_MODIFYUP);
	break;
//----------------------------------------------- A/B Mark
case '/':
	DivMark(cinfo);
	break;
//----------------------------------------------- View style
case ';':
	return SetCellDisplayFormat(cinfo, 0, NULL);
//----------------------------------------------- Copy Path
case '=':
	SetPairPath(cinfo, NULL, NULL);
	break;
//----------------------------------------------- New Pane / Join
case '_':
	if ( cinfo->combo ){
		CreateNewPane(T("-noactive"));
	}else{
		cinfo->swin ^= SWIN_JOIN;
		if ( cinfo->swin & SWIN_JOIN ){
			setflag(cinfo->swin, SWIN_WBOOT);
			SendX_win(cinfo);
			JoinWindow(cinfo);
			SetPopMsg(cinfo, POPMSG_MSG, MES_JWON);
			BootPairPPc(cinfo);
		}else{
			resetflag(cinfo->swin, SWIN_WBOOT);
			SendX_win(cinfo);
			SetPopMsg(cinfo, POPMSG_MSG, MES_JWOF);
		}
	}
	break;
//----------------------------------------------- Top Window
case KC_PTOP: {
	HWND hPairWnd;

	hPairWnd = GetPairWnd(cinfo);
	if ( hPairWnd != NULL ) PPxCommonCommand(hPairWnd, 0, K_WTOP);
	break;
}
//----------------------------------------------- Bottom Window
case KC_PBOT: {
	HWND hPairWnd;

	hPairWnd = GetPairWnd(cinfo);
	if ( hPairWnd != NULL ) PPxCommonCommand(hPairWnd, 0, K_WBOT);
	break;
}
//----------------------------------------------- Window
case KC_WIND:
	PPC_window(cinfo->info.hWnd);
	break;
//----------------------------------------------- Layout
case K_layout:
	PPcLayoutCommand(cinfo, NilStr);
	break;
//----------------------------------------------- Bottom Cell
case K_c | K_Pup:	// ^[PgUP]
case K_s | K_Pup:	// \[PgUP]
case '<':
	MoveCellCsr(cinfo, -cinfo->e.cellN, NULL);
	break;
//----------------------------------------------- Top Cell
case K_c | K_Pdw:	// ^[PgDW]
case K_s | K_Pdw:	// \[PgDW]
case '>':
	MoveCellCsr(cinfo, cinfo->e.cellIMax - cinfo->e.cellN, NULL);
	break;
//----------------------------------------------- Root
case '\\':
	PPC_RootDir(cinfo);
	break;
//----------------------------------------------- Desktop
case '|':
	if ( cinfo->path[0] != ':' ){
		SetPPcDirPos(cinfo);
		FixRootEntryCursor(cinfo);
		read_entry(cinfo, RENTRY_JUMPNAME | RENTRY_JUMPNAME_INC);
	}
	break;
//----------------------------------------------- Menu Bar On/Off
case '^':
	ToggleMenuBar(cinfo);
	break;
//----------------------------------------------- Help
case K_F1:
	PPxHelp(cinfo->info.hWnd, HELP_CONTEXT, IDH_PPC);
	break;
//----------------------------------------------- Pair Window
case K_s | K_ri:
case K_s | K_lf:
	PPcChangeWindow(cinfo, PPCHGWIN_PAIR);
	break;
//----------------------------------------------- Next Window
case K_tab:
case K_F6:
	PPcChangeWindow(cinfo, PPCHGWIN_NEXT);
	break;
//----------------------------------------------- Previous Window
case K_s | K_tab:
case K_s | K_F6:
	PPcChangeWindow(cinfo, PPCHGWIN_BACK);
	break;
//----------------------------------------------- Fix WindowFrame
case K_a | K_F6:
	FixWindowSize(cinfo, 0, 0);
	break;

//----------------------------------------------- CPU info
case K_s | K_F8:
	return PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("*cpu"), NULL, 0);
//----------------------------------------------- Memu
case K_F10:
	if ( cinfo->X_win & XWIN_MENUBAR ) return ERROR_INVALID_FUNCTION;	// メニューバーへ移動
//	PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("%k\"down"), NULL, 0);
	PPcSystemMenu(cinfo);	// &SPACE で表示
	break;
//----------------------------------------------- ContextMenu(dir)
case K_c | K_apps:
	return PPcDirContextMenu(cinfo);
//----------------------------------------------- ContextMenu
case K_apps:
case K_s | K_F10:
case K_c | K_cr:
	CrMenu(cinfo, TRUE);
	break;
//----------------------------------------------- ShellContextMenu
case K_s | K_apps:
case (K_c | K_s | K_F10):
	SCmenu(cinfo, NULL);
	break;
//----------------------------------------------- More PPC
case K_F11:
	PPCui(cinfo->info.hWnd, NULL);
	break;
//----------------------------------------------- Dup PPC
case K_s | K_F11:
	PPCuiWithPath(cinfo->info.hWnd, cinfo->path);
	break;
//----------------------------------------------- Runas PPC
case K_c | K_F11:
	return PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("*ppc -runas \"%1\""), NULL, 0);
//----------------------------------------------- Dup File
case K_F12:
	return PPcDupFile(cinfo);
//----------------------------------------------- Create Hard Link
case K_s | K_F12:
	PPcCreateHarkLink(cinfo);
	break;
//----------------------------------------------- Properties
case K_a | K_cr:
	SCmenu(cinfo, StrInvokeProperties);
	break;
//----------------------------------------------- Sync Properties
case K_a | K_s | K_cr:
	PPcSyncProperties(cinfo, NilStr);
	break;
//----------------------------------------------- Invert Mark(F)
case K_s | K_home:		// \[home]
	cinfo->MarkMask = MARKMASK_FILE;
	PPC_ReverseMark(cinfo);
	break;
//----------------------------------------------- Invert Mark(F+D)
case K_s | K_end:
	cinfo->MarkMask = MARKMASK_DIRFILE;
	PPC_ReverseMark(cinfo);
	break;
//-----------------------------------------------
case K_c | K_home:
	setflag(cinfo->DrawTargetFlags, DRAWT_ENTRY);
	cinfo->MarkMask = MARKMASK_FILE;
	ClearMark(cinfo);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	break;
//-----------------------------------------------
case K_c | K_end:
	setflag(cinfo->DrawTargetFlags, DRAWT_ENTRY);
	cinfo->MarkMask = MARKMASK_DIRFILE;
	ClearMark(cinfo);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	break;
//----------------------------------------------- jumpposition
case K_a | K_home:
	PPcMoveWindowSavedPosition(cinfo);
	break;
//----------------------------------------------- saveposition
case K_a | K_s | K_home:
	PPcSaveWindowPosition(cinfo);
	break;
//----------------------------------------------- VFS toggle
case KC_Tvfs:
	X_vfs = !X_vfs;
	if ( X_vfs ){
		PPxSendMessage(WM_PPXCOMMAND, K_Lvfs, 0);
		SetPopMsg(cinfo, POPMSG_MSG, MES_VFSL);
	}else{
		PPxSendMessage(WM_PPXCOMMAND, K_Fvfs, 0);
		SetPopMsg(cinfo, POPMSG_MSG, MES_VFSU);
	}
	break;
//----------------------------------------------- Enter Directory
case KC_Edir:
	CellLook(cinfo, -1);
	break;
//----------------------------------------------- Jump Entry
case KC_JMPE:
	JumpPathEntry(cinfo, CEL(cinfo->e.cellN).f.cFileName, RENTRY_JUMPNAME);
	break;
//----------------------------------------------- Update Entry Data
case KC_UpdateEntryData:
	UpdateEntryData(cinfo);
	break;
//----------------------------------------------- Do Delayed File Operation
case KC_DODO:
	DelayedFileOperation(cinfo);
	break;
//----------------------------------------------- load VFS
case K_Lvfs:
	VFSOn(VFS_DIRECTORY);
	break;
//----------------------------------------------- unload VFS
case K_Fvfs:
	if ( cinfo->e.Dtype.ExtData != INVALID_HANDLE_VALUE ){
		cinfo->e.Dtype.ExtData = NULL;
	}
	VFSOff();
	break;
//----------------------------------------------- Save Cust
case K_Scust:
	PPcSaveCust(cinfo);
	break;
//----------------------------------------------- Reload Cust
case K_Lcust: // ※ WM_PPXCOMMAND は、WmPPxCommand で処理する
	PPcReloadCustomize(cinfo,  0);
	break;
//----------------------------------------------- About
case K_about:
	SetPopMsg(cinfo, POPMSG_MSG, T(" Paper Plane cUI Version ")
			T(FileProp_Version)
			T("(") T(__DATE__) T(",") RUNENVSTRINGS T(") (c)TORO "));
	break;
//----------------------------------------------- cinfo->swin の更新通信
case KC_Join:
	IOX_win(cinfo, FALSE);
	break;
//----------------------------------------------- Zoom in
case K_s | K_ins:
case K_c | K_v | VK_ADD:
case K_c | K_v | VK_OEM_PLUS: // US[=/+] JIS[;/+]
	SetMag(cinfo, 10);
	break;
//----------------------------------------------- Zoom out
case K_s | K_del:
case K_c | K_v | VK_SUBTRACT:
case K_c | K_v | VK_OEM_MINUS: // US[-/_] JIS[-/=]
	SetMag(cinfo, -10);
	break;
//----------------------------------------------- Zoom 100%
case K_c | K_v | '0':
case K_c | K_v | VK_NUMPAD0:
	SetMag(cinfo, 0);
	break;
//----------------------------------------------- Pop cell
case K_ins:
	PPcPopCell(cinfo);
	break;
//----------------------------------------------- Push cell
case K_del:
	PPcPushCell(cinfo);
	break;
//----------------------------------------------- Iconic/Minimize
case K_s | K_esc:
	if ( cinfo->combo ){
		SendMessage(cinfo->hComboWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	}else if ( cinfo->hTrayWnd != NULL ){
		ShowWindow(cinfo->hTrayWnd, SW_MINIMIZE); // 他のPPxをまとめて最小化
	}else{
		ShowWindow(cinfo->info.hWnd, SW_MINIMIZE);
	}
	cinfo->Mpos = -1;
	break;
//----------------------------------------------- Mark & [↓]
case K_space:
	cinfo->MarkMask = MARKMASK_DIRFILE;
	CellMark(cinfo, cinfo->e.cellN, MARK_REVERSE);
	if ( cinfo->e.cellN >= (cinfo->e.cellIMax - 1) ){
		RefleshCell(cinfo, cinfo->e.cellN);
	}
	MoveCellCursor(cinfo, &XC_mvUD);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	break;
//----------------------------------------------- Mark & [↑]
case K_s | K_space:
	cinfo->MarkMask = MARKMASK_DIRFILE;
	CellMark(cinfo, cinfo->e.cellN, MARK_REVERSE);
	if ( cinfo->e.cellN == 0 ) RefleshCell(cinfo, cinfo->e.cellN);
	MoveCellCursorR(cinfo, &XC_mvUD);
	RefleshInfoBox(cinfo, DE_ATTR_MARK);
	break;
//----------------------------------------------- System Memu
case K_a | '-':
	if ( cinfo->combo ){
		SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND, K_a | '-', (LPARAM)cinfo->info.hWnd);
		break;
	}
	PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("%k\"down"), NULL, 0);
	// case K_a | K_space: へ
//----------------------------------------------- System Memu
case K_a | K_space:
	PPcSystemMenu(cinfo);
	break;
//----------------------------------------------- [↑]
case K_up:
	MoveCellCursorR(cinfo, &XC_mvUD);
	break;
//----------------------------------------------- [↓]
case K_dw:
	MoveCellCursor(cinfo, &XC_mvUD);
	break;
//----------------------------------------------- [←]
case K_lf:
	MoveCellCursorR(cinfo, &XC_mvLR);
	break;
//----------------------------------------------- [→]
case K_ri:
	MoveCellCursor(cinfo, &XC_mvLR);
	break;
//----------------------------------------------- ^[←]
case K_c | K_lf:
	JumpPathTrackingList(cinfo, -1);
	break;
//----------------------------------------------- ^[→]
case K_c | K_ri:
	JumpPathTrackingList(cinfo, 1);
	break;
//----------------------------------------------- ^\[←]
case K_c | K_s | K_lf:
	PathTrackingListMenu(cinfo, -1);
	break;
//----------------------------------------------- ^\[→]
case K_c | K_s | K_ri:
	PathTrackingListMenu(cinfo, 1);
	break;
//----------------------------------------------- &\[↑] size
case K_s | K_a | K_up:
	FixWindowSize(cinfo, 0, -1);
	break;
//----------------------------------------------- &\[↓] size
case K_s | K_a | K_dw:
	FixWindowSize(cinfo, 0, 1);
	break;
//----------------------------------------------- &\[←] size
case K_s | K_a | K_lf:
	FixWindowSize(cinfo, -1, 0);
	break;
//----------------------------------------------- &\[→] size
case K_s | K_a | K_ri:
	FixWindowSize(cinfo, 1, 0);
	break;
//----------------------------------------------- ^&[↑] rate
case K_c | K_a | K_up:
	FixPaneSize(cinfo, 0, -1, FPS_KEYBOARD);
	break;
//----------------------------------------------- ^&[↓] rate
case K_c | K_a | K_dw:
	FixPaneSize(cinfo, 0, 1, FPS_KEYBOARD);
	break;
//----------------------------------------------- ^&[←] rate
case K_c | K_a | K_lf:
	FixPaneSize(cinfo, -1, 0, FPS_KEYBOARD);
	break;
//----------------------------------------------- ^&[→] rate
case K_c | K_a | K_ri:
	FixPaneSize(cinfo, 1, 0, FPS_KEYBOARD);
	break;
//----------------------------------------------- back page
case K_s | K_up:	// \[↑]
case K_Pup:			// [PgUP]
	MoveCellCursorR(cinfo, &XC_mvPG);
	break;
//----------------------------------------------- next page
case K_s | K_dw:	// \[↓]
case K_Pdw:			// [PgDW]
	MoveCellCursor(cinfo, &XC_mvPG);
	break;
//----------------------------------------------- up directory
case K_bs:
	PPC_UpDir(cinfo);
	break;
//----------------------------------------------- previous directory
case K_s | K_bs:
	PPcBackDirectory(cinfo);
	break;
//----------------------------------------------- previous directory list
case K_c | K_bs:
	PPcBackDirectoryList(cinfo);
	break;
//----------------------------------------------- directory size / exec
case K_s | K_cr:
	CountOrEdit(cinfo);
	break;
//----------------------------------------------- go directory / exec
case K_cr:
	CrMenu(cinfo, FALSE);
	break;
//----------------------------------------------- Enter PAUSE
case K_v | VK_PAUSE:
	cinfo->BreakFlag = TRUE;
	break;
//----------------------------------------------- Execute command(/K)
case K_FIRSTCMD:
	FirstCommand(cinfo);
	break;
//-----------------------------------------------
case K_SETGRAYSTATUS:
	SetGrayStatus(cinfo);
	break;

//----------------------------------------------- アクティブ化時の一覧更新
case KC_CHECKRELOAD:
	RefreshAfterList(cinfo, ALST_ACTIVATE);
	break;

//=============================================================================
default:
	if ( !PPxCommonCommand(cinfo->info.hWnd, 0, key) ) break;
//----------------------------------------------- Menu
	if ( (!(cinfo->X_win & XWIN_MENUBAR) || cinfo->combo) && !(key & K_v) &&
				((key & (K_e | K_s | K_c | K_a)) == (K_a)) ){
		SystemDynamicMenu(!(cinfo->combo) ? &cinfo->DynamicMenu : &ComboDMenu,
				&cinfo->info, key);
		break;
//----------------------------------------------- Search
	}else if (!(key & K_v) &&
		(((key & (K_e | K_s | K_c | K_a)) == (K_a | K_s)) ||
		 ((key & (K_e | K_c | K_a)) == K_e) ) ){
		SearchEntryOnekey(cinfo, key);
		break;
//----------------------------------------------- Drive jump
	}else if ( !(key & K_v) && IsdigitA(key) ){
		JumpDrive(cinfo, key);
		break;
//----------------------------------------------- 該当無し
	}else{
		DEBUGLOGC("PPcCommand:%04x unknown end", key);
		return ERROR_INVALID_FUNCTION;
	}
}
	cinfo->PrevCommand = key;
	DEBUGLOGC("PPcCommand:%04x end", key);
	return NO_ERROR;
}
