/*-----------------------------------------------------------------------------
	Paper Plane vUI												～ command ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

const EXECKEYCOMMANDSTRUCT PPvExecKeyImg =
		{(EXECKEYCOMMANDFUNCTION)PPvCommand,T("KV_img"),T("KV_main")};
const EXECKEYCOMMANDSTRUCT PPvExecKeyPage =
		{(EXECKEYCOMMANDFUNCTION)PPvCommand,T("KV_page"),T("KV_main")};
const EXECKEYCOMMANDSTRUCT PPvExecKeyCrt =
		{(EXECKEYCOMMANDFUNCTION)PPvCommand,T("KV_crt"),T("KV_main")};
const TCHAR WinmmName[] = T("winmm.dll");

ERRORCODE PPXAPI PPvCommand(PPV_APPINFO *vinfo,WORD key)
{
	TCHAR buf[CMDLINESIZE];
	HWND hWnd;

	if ( !(key & K_raw) ){
		const EXECKEYCOMMANDSTRUCT *ek;

		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			ek = &PPvExecKeyImg;
		}else{
			ek = (VOsel.cursor != FALSE) ? &PPvExecKeyCrt : &PPvExecKeyPage;
		}
		return ExecKeyCommand(ek,&vinfo->info,key);
	}
	hWnd = vinfo->info.hWnd;
	resetflag(key,K_raw);	// エイリアスビットを無効にする
	switch(key){
//----------------------------------------------- All
case K_c | 'A':
	if ( !(vo_.DModeBit & VO_dmode_SELECTABLE) ) break;

	if ( VOsel.select != FALSE ){ // 選択中
		ResetSelect(TRUE);
	}else{
		VOsel.select = TRUE;
		VOsel.line = TRUE;

		VOsel.now.x.offset = VOsel.start.x.offset = 0;
		VOsel.start.y.line = 0;
		VOsel.now.y.line = VO_maxY - 1;
		FixSelectRange();
	}
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- キャレットモード
case '5':
case 'I':
	if ( !(vo_.DModeBit & VO_dmode_SELECTABLE) ) break;

	XV_tmod = !XV_tmod;
	SetCustData(T("XV_tmod"),&XV_tmod,sizeof(XV_tmod));
	if ( XV_tmod ){
		InitCursorMode(hWnd);
	}else{
		ResetSelect(TRUE);
	}
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- Cilp
case K_c | 'C':
	if ( (vo_.DModeType == DISPT_DOCUMENT) &&
		 (vo_.DocmodeType == DOCMODE_EMETA) ){
		OpenClipboard(hWnd);
		EmptyClipboard();
		SetClipboardData(CF_ENHMETAFILE,vo_.eMetafile.handle);
		CloseClipboard();
		SetPopMsg(POPMSG_NOLOGMSG,MES_CPTX);
		break;
	}
	if ( vo_.DModeBit & VO_dmode_SELECTABLE ){
		if ( VOsel.select != FALSE ){ // 選択中なのでコピー
			ClipText(hWnd);
			ResetSelect(TRUE);
			InvalidateRect(hWnd,NULL,FALSE);
			SetPopMsg(POPMSG_NOLOGMSG,MES_CPTX);
			break;
		}
		if ( VOsel.cursor == FALSE ){ // カーソルモードになっていないので有効
			InitCursorMode(hWnd);
			InvalidateRect(hWnd,NULL,FALSE);
			SetPopMsg(POPMSG_NOLOGMSG,MES_CPMS);
			break;
		}
		// 非選択中なのでページモードにする
		ResetSelect(TRUE);
		InvalidateRect(hWnd,NULL,TRUE);
		if ( XV_tmod == 0 ) SetPopMsg(POPMSG_NOLOGMSG,MES_CPML);
		break;
	}else if ( vo_.DModeBit == DOCMODE_BMP ){
		ClipBitmap(hWnd);
	}
	break;
//----------------------------------------------- Aspect Rate
case 'A':
	if ( vo_.DModeType == DISPT_IMAGE ) InputAspectRate(hWnd);
	break;
//----------------------------------------------- Find Back
case K_c | 'B':
case 'B':
	if ( FindInputBox(hWnd,T("Find back")) == FALSE ) break;
//----------------------------------------------- Find Back
case K_s | K_F3:
case '[':				// [[]
	DoFind(hWnd,-1);
	break;
//----------------------------------------------- 制御記号の表示トグル
case 'C':
	if ( XV_bctl[1] ){
		XV_bctl[0] = XV_bctl[1] = XV_bctl[2] = 0;
	}else{
		GetCustData(T("XV_bctl"),&XV_bctl,sizeof(XV_bctl));
		if ( !XV_bctl[0] ) XV_bctl[0] = 1;
		if ( !XV_bctl[1] ) XV_bctl[1] = 1;
	}
	InvalidateRect(hWnd,NULL,FALSE);
	break;
//----------------------------------------------- PPE
case K_c | 'E':
case K_s | 'E':
	if ( vo_.DModeBit != DOCMODE_NONE ){
		if ( (vo_.file.IsFile == 1) &&			// ファイルなら直接開く
			 !(GetFileAttributesL(vo_.file.name) & FILE_ATTRIBUTE_DIRECTORY) ){
			PPEui(hWnd,vo_.file.name,NULL);
		}else{ // ファイル以外ならメモリから開く
			BYTE *image;

			if ( vo_.file.size.h || (vo_.file.size.l > 2 * MB) ){
				SetPopMsg(POPMSG_MSG,T("Open error:Image too large."));
				break;
			}
			image = HeapAlloc(PPvHeap,0,vo_.file.size.l + 2);
			if ( image == NULL ) break;
			memcpy(image,vo_.file.image,vo_.file.size.l);
			*(image + vo_.file.size.l) = '\0';
			*(image + vo_.file.size.l + 1) = '\0';
			PPEui(BADHWND,vo_.file.name,(TCHAR *)image);
			HeapFree(PPvHeap,0,image);
		}
	}
	break;
//----------------------------------------------- Find Foward
case K_c | 'F':
case 'F':
	if ( FindInputBox(hWnd,T("Find forward")) == FALSE ) return ERROR_CANCELLED;
//----------------------------------------------- Find Foward
case K_F3:
case ']':				// []]
	DoFind(hWnd,1);
	break;
//----------------------------------------------- shell
case 'H':
	PPvShell(hWnd);
	break;
//----------------------------------------------- Jump
case 'J': {
	BOOL logical;
	DWORD csrY;

	if ( vo_.DModeBit == DOCMODE_NONE ) break;
	logical = ( (VOi->ti != NULL) && ( vo_.DModeBit == DOCMODE_TEXT ) ) ? XV_numt : 0;
	csrY = (VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY;
	wsprintf(buf,T("%u"),logical ? VOi->ti[csrY].line : (DWORD)(csrY + 1) );
	if ( tInput(hWnd,T("Jump Line"),buf,TSIZEOF(buf),
			PPXH_NUMBER,PPXH_NUMBER) <= 0 ){
		return ERROR_CANCELLED;
	}
	JumpLine(buf);
	break;
}
//----------------------------------------------- Redraw
#ifdef USEDIRECTX
case K_v | VK_SCROLL:
	ResetDxDraw(DxDraw);
#endif
case K_c | 'L':
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- 左端行番号表示トグル
case 'U':
	XV_lnum ^= 1;
	InvalidateRect(hWnd,NULL,TRUE);
	SetScrollBar();
	if ( VOi->defwidth != WIDTH_AUTOFIX ) break;
	UpdateWindow(hWnd); // 1回表示しないと行番号の表示幅が計算できない
	WmWindowPosChanged(hWnd);
	break;
//----------------------------------------------- 行番号扱いトグル
case 'T':
	XV_numt ^= 1;
	InvalidateRect(hWnd,NULL,TRUE);
	SetScrollBar();
	break;
//----------------------------------------------- Memo / Alpha Grid / DumpEMF
case 'M':
	if ( (vo_.memo.bottom == NULL) || (vo_.memo.top <= 1) ){
		if ( ((vo_.DModeType == DISPT_IMAGE) || (vo_.DModeType == DISPT_RAWIMAGE)) && (vo_.bitmap.transcolor >= 0) ){
			ModifyAlpha(hWnd);
			break;
		}
		if ( (vo_.DModeType == DISPT_DOCUMENT) &&
			 (vo_.DocmodeType == DOCMODE_EMETA) ){
			DumpEmf();
			break;
		}
		SetPopMsg(POPMSG_NOLOGMSG,T("No memo."));
	}else{
		PPEui(BADHWND,T("Memo"),(TCHAR *)vo_.memo.bottom);
	}
	break;

//----------------------------------------------- メモ表示トグル
case K_s | 'M':
	X_swmt ^= 1;
	if ( X_swmt ) GetMemo();
	InvalidateRect(hWnd,NULL,TRUE);
	SetPopMsg(POPMSG_NOLOGMSG,X_swmt ? T("memo on") : T("memo off"));
	break;

//----------------------------------------------- Width
case ';':
	return ChangeWidth(hWnd);
//----------------------------------------------- Reload
case K_F5:
case '.':
	PPvReload(hWnd);
	break;
//----------------------------------------------- Open
case K_c | 'O':
	tstrcpy(buf,vo_.file.name);
	if ( tInput(hWnd,T("Open"),buf,VFPS,PPXH_NAME_R,PPXH_PATH) <= 0 ){
		return ERROR_CANCELLED;
	}
	if ( buf[0] == '\0' ) return ERROR_CANCELLED;
	VFSFixPath(NULL,buf,NULL,VFSFIX_PATH);
	OpenAndFollowViewObject(hWnd,buf,NULL,NULL,0);
	break;
//----------------------------------------------- Save
case K_c | 'S':
	PPvSave(hWnd);
	break;
//----------------------------------------------- Play
case 'P':
	if ( vo_.DModeBit == DOCMODE_NONE ) break;

	if ( vo_.DModeBit == DOCMODE_BMP ){
		if ( vo_.bitmap.page.max == 0 ) break;
		if ( vo_.bitmap.page.animate == FALSE ){
			vo_.bitmap.page.animate = TRUE;
			ChangePage(1);
		}else{
			vo_.bitmap.page.animate = FALSE;
			KillTimer(hWnd,TIMERID_ANIMATE);
			SetPopMsg(POPMSG_NOLOGMSG,T("Stop animate"));
		}
		break;
	}
	if ( *(DWORD *)vo_.file.image != 0x46464952 ) break; // RIFF
	if ( FileDivideMode > FDM_NODIVMAX ){
		SetPopMsg(POPMSG_NOLOGMSG,T("Play error(divide mode)"));
		break;
	}
	if ( hWinmm == NULL ){
		SetPopMsg(POPMSG_NOLOGMSG,T("Loading winmm.dll"));
		UpdateWindow_Part(hWnd);
		hWinmm = LoadLibrary(WinmmName);
	}
	if ( hWinmm != NULL ){
		if ( DsndPlaySound == NULL ){
			GETDLLPROCT(hWinmm,sndPlaySound);
		}
		if ( DsndPlaySound != NULL ){
			DsndPlaySound((LPCTSTR)vo_.file.image,
					SND_ASYNC | SND_NODEFAULT | SND_MEMORY);
		}
	}
	break;
//----------------------------------------------- Print
case K_c | 'P':
	PPVPrint(hWnd);
	break;
//----------------------------------------------- Reverse background
case 'R':
case K_s | 'R':
	ReverseBackground(hWnd);
	break;
//----------------------------------------------- Paste
case K_c | 'V':
	PPvPaste(hWnd);
	break;
//----------------------------------------------- Paste Type
case K_s | K_c | 'V':
	PPvPasteType(hWnd);
	break;
//----------------------------------------------- eXecute
case 'X':
	return PPvExecute(hWnd);
//----------------------------------------------- TAB
case K_tab:
	VOi->tab = (VOi->tab != 8) ? 8 : 4;
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- Zoom mode change
case '=':
case K_c | K_v | '0':
case K_c | K_v | VK_NUMPAD0:
	SetMag(IMGD_NORMAL);
	break;
case '\\':
	SetMag(IMGD_AUTOWINDOWSIZE_2);
	break;
//----------------------------------------------- Zoom in
case K_ins:
case '+':
case K_c | K_v | VK_ADD:
case K_c | K_v | 0xbb: // US[=/+] JIS[;/+]
	SetMag(+IMGD_OFFSET);
	break;
//----------------------------------------------- Zoom out
case K_del:
case '-':
case K_c | K_v | VK_SUBTRACT:
case K_c | K_v | 0xbd: // US[-/_] JIS[-/=]
	SetMag(-IMGD_OFFSET);
	break;
//----------------------------------------------- TextCode
case '@':
	return PPV_TextCode(hWnd,0);
//----------------------------------------------- Display
case ':':
	return PPV_DisplayType(hWnd,FALSE);
case '*': // 仮実装
	return PPV_DisplayType(hWnd,TRUE);
//----------------------------------------------- Font width
case 'W':
	XV_unff = XV_unff ? FALSE : TRUE;
	if ( XV_unff == FALSE ){
		if ( hUnfixedFont != NULL ){
			DeleteObject(hUnfixedFont);
			hUnfixedFont = NULL;
		}
	}else{
		MakeUnfixedFont();
	}
	ReMakeIndexes(hWnd);
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- Zap
case 'Z':
	if ( vo_.file.name[0] == '\0' ) break;
	if( NULL == PPxShellExecute(hWnd,NULL,vo_.file.name,NilStr,NilStr,0,buf) ){
		SetPopMsg(POPMSG_NOLOGMSG,buf);
	}
	break;
//----------------------------------------------- Menu Bar On/Off
case '^':
	X_win ^= XWIN_MENUBAR;
	SetCustTable(T("X_win"),T("V"),&X_win,sizeof(X_win));
	SetMenu(hMainWnd, (X_win & XWIN_MENUBAR) ? DynamicMenu.hMenuBarMenu : NULL);
	break;
//----------------------------------------------- Help
case K_F1:
	PPxHelp(hWnd,HELP_CONTEXT,IDH_PPV);
	break;
//----------------------------------------------- Minimize
case K_cr:
	if ( VOsel.select != FALSE ){
		PPvCommand(vinfo,K_c | 'C');
		break;
	}
// K_s | K_esc へ続く
case K_bs:
case 'Y':
case 'N':
case K_s | K_esc:
	PPvMinimize(hWnd);
	Mpos = -1;
	break;
//----------------------------------------------- quit
case K_esc:
	if ( PMessageBox(hWnd,MES_QPPV,T("Quit"),MB_APPLMODAL |
		 MB_OKCANCEL | MB_DEFBUTTON1 | MB_ICONQUESTION) != IDOK ){
		return ERROR_CANCELLED;
	}
// 'Q' へ続く
//----------------------------------------------- Quit
case K_a | K_F4:
case 'Q':
	PostMessage(hWnd,WM_CLOSE,0,0);
	break;
//----------------------------------------------- Save position/Goto posision
case 'D':
	SaveBookmark(BookmarkID_1st);
	SetPopMsg(POPMSG_NOLOGMSG,T("Saved position #1"));
	break;

case K_c | 'D':
	return SaveBookmarkMenu(hWnd);

case 'G':
	if ( Bookmark[BookmarkID_1st].pos.x >= 0 ){
		if ( GotoBookmark(hWnd,BookmarkID_1st) == FALSE ){
			GotoBookmark(hWnd,BookmarkID_undo);
		}else{
			SetPopMsg(POPMSG_NOLOGMSG,T("Goto position #1"));
		}
	}
	break;

case K_c | 'G':
	return GotoBookmarkMenu(hWnd);
//----------------------------------------------- Rotate Left(K) Right(L)
case 'K':
	Rotate(hWnd,-1);
	break;
case 'L':
	Rotate(hWnd,1);
	break;
//----------------------------------------------- top
case K_s | K_Pup:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(0,1-VO_sizeY,TRUE);
		break;
	}
	// K_c | K_home へ
case K_home:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(-VOsel.now.x.offset,0,FALSE);
		break;
	}
	// K_c | K_home へ
case K_c | K_home:
	MoveCsrkey(0,-VO_maxY,FALSE);
	break;
//----------------------------------------------- bottom
case K_s | K_Pdw:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(0,VO_sizeY - 1,TRUE);
		break;
	}
	// K_c | K_end へ
case K_end:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(MAX_MOVE_X,0,FALSE);
		break;
	}
	// K_c | K_end へ
case K_c | K_end:
	MoveCsrkey(0,VO_maxY,FALSE);
	if ( IsTrue(BackReader) ){
		mtinfo.PresetY = 0x3fffffff;
		setflag(mtinfo.OpenFlags,PPV__NoGetPosFromHist);
	}
	break;
//----------------------------------------------- up line
case K_up:
	MoveCsrkey(0,-VO_stepY,FALSE);
	break;
//----------------------------------------------- down line
case K_dw:
	MoveCsrkey(0,VO_stepY,FALSE);
	break;
//-----------------------------------------------
case '_':
	Use2ndView++;
	if ( Use2ndView > 2 ) Use2ndView = 0;
	WmWindowPosChanged(hWnd);
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//-----------------------------------------------
case '/':
	ReadSizeChange(hWnd);
	break;
case '>':
	DivChange(1);
	break;
case '<':
	DivChange(-1);
	break;
//----------------------------------------------- page up
case K_s | K_up:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(0,-VO_stepY,TRUE);
		break;
	}
case K_Pup:
case K_s | K_space:
	MoveCsrkey(0,1-VO_sizeY,FALSE);
	break;
//----------------------------------------------- page down
case K_s | K_dw:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(0,VO_stepY,TRUE);
		break;
	}
case K_Pdw:
case K_space:
	MoveCsrkey(0,VO_sizeY-1,FALSE);
	break;
//----------------------------------------------- left
case K_s | K_home:
	MoveCsrkey(-VOsel.now.x.offset,0,( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ) );
	break;
//----------------------------------------------- right
case K_s | K_end:
	MoveCsrkey(MAX_MOVE_X,0,( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ) );
	break;
//----------------------------------------------- line left
case K_lf:
	MoveCsrkey(-VO_stepX,0,FALSE);
	break;
//----------------------------------------------- line right
case K_ri:
	MoveCsrkey(VO_stepX,0,FALSE);
	break;
//----------------------------------------------- page left
case K_s | K_lf:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(-VO_stepX,0,TRUE);
	}else{
		MoveCsrkey(1 - VO_sizeX,0,FALSE);
	}
	break;
//----------------------------------------------- page right
case K_s | K_ri:
	if ( (VOsel.cursor != FALSE) && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		MoveCsrkey(VO_stepX,0,TRUE);
	}else{
		MoveCsrkey(VO_sizeX - 1,0,FALSE);
	}
	break;
//----------------------------------------------- prev page
case K_c | K_Pup:
	if ( vo_.DModeBit == DOCMODE_BMP ){
		vo_.bitmap.page.animate = FALSE;
		ChangePage(-1);
	}else{
		DivChange(-1);
	}
	break;
//----------------------------------------------- next page
case K_c | K_Pdw:
	if ( vo_.DModeBit == DOCMODE_BMP ){
		vo_.bitmap.page.animate = FALSE;
		ChangePage(1);
	}else{
		DivChange(1);
	}
	break;
//----------------------------------------------- top page
case K_c | K_s | K_Pup:
	if ( FileDivideMode < FDM_NODIVMAX ){
		if ( vo_.DModeBit == DOCMODE_BMP ){ // 画像ページのトップ
			ChangePage(-vo_.bitmap.page.current);
		}
		break;
	}
	if ( !(FileDividePointer.LowPart | FileDividePointer.HighPart) ) break;
	// 分割ページのトップ
	FileDividePointer.LowPart = FileDividePointer.HighPart = 0;
	PPvReload(hMainWnd);
	break;
//----------------------------------------------- last page
case K_c | K_s | K_Pdw:
	if ( FileDivideMode < FDM_NODIVMAX ){
		if ( vo_.DModeBit == DOCMODE_BMP ){ // 画像ページの末尾
			ChangePage(vo_.bitmap.page.max - vo_.bitmap.page.current - 1);
		}
		break;
	}
	// 分割ページの末尾
	GetDivMax(&FileDividePointer);
	PPvReload(hMainWnd);
	break;
//----------------------------------------------- &\[↑] size
case K_s | K_a | K_up:
	FixWindowSize(hWnd,0,-1);
	break;
//----------------------------------------------- &\[↓] size
case K_s | K_a | K_dw:
	FixWindowSize(hWnd,0,1);
	break;
//----------------------------------------------- &\[←] size
case K_s | K_a | K_lf:
	FixWindowSize(hWnd,-1,0);
	break;
//----------------------------------------------- &\[→] size
case K_s | K_a | K_ri:
	FixWindowSize(hWnd,1,0);
	break;
//----------------------------------------------- A_exec
case K_s | K_cr:
	PPvEditFile(hWnd);
	break;
//----------------------------------------------- Fix WindowFrame
case K_a | K_F6:
	FixWindowSize(hWnd,0,0);
	break;
//----------------------------------------------- Memu
case K_F10:
	if (X_win & XWIN_MENUBAR) return ERROR_INVALID_FUNCTION;
//	break; なし
//----------------------------------------------- System Memu
case K_a | K_space:
	DynamicMenu.Sysmenu = TRUE;
	PostMessage(hWnd, WM_SYSCOMMAND, SC_KEYMENU, TMAKELPARAM(0, 0));
	// メニューを表示させるキーを予め送信
	PP_ExtractMacro(hWnd, &vinfo->info, NULL, T("%k\"&down"), NULL, 0);
	break;
//----------------------------------------------- Context Menu
case K_apps:
case K_s | K_F10:
case K_c | K_cr:
	PPvContextMenu(hWnd);
	break;
//----------------------------------------------- ShellContextMenu
case K_s | K_apps:
case (K_c | K_s | K_F10):
	ViewfileContextMenu(hWnd);
	break;
//----------------------------------------------- About
case K_about:
	SetPopMsg(POPMSG_MSG,T(" Paper Plane vUI Version ")
			T(FileProp_Version) T("(") T(__DATE__) T(",")
			T(DRAWSTRING) T(BITSTRING) T(",")
			T(CODETYPESTRING) T(") (c)TORO "));
	break;
//----------------------------------------------- Open
case KV_Load:
	PPvReceiveRequest(hWnd);
	break;
//-----------------------------------------------
case K_a | K_home:	// &[home]
	LoadWinpos(hWnd);
	break;
//-----------------------------------------------
case K_a | K_s | K_home:	// &\[home]
	wsprintf(buf,T("%s_"),RegCID);
	SetCustTable(T("_WinPos"),buf,&WinPos,sizeof(WinPos));
	SetPopMsg(POPMSG_MSG,MES_SAVP);
	break;
//----------------------------------------------- VFS toggle
case KC_Tvfs:
	X_vfs = !X_vfs;
	if (X_vfs){
		PPxPostMessage(WM_PPXCOMMAND,K_Lvfs,0);
		SetPopMsg(POPMSG_NOLOGMSG,MES_VFSL);
	}else{
		PPxPostMessage(WM_PPXCOMMAND,K_Fvfs,0);
		SetPopMsg(POPMSG_NOLOGMSG,MES_VFSU);
	}
	break;
//----------------------------------------------- load VFS
case K_Lvfs:
	VFSOn(VFS_DIRECTORY | VFS_BMP);
	break;
//----------------------------------------------- unload VFS
case K_Fvfs:
	VFSOff();
	break;
//----------------------------------------------- Save Cust
case K_Scust:
	PPvSaveCust();
	break;
//----------------------------------------------- Load Cust
case K_Lcust:
	PPxCommonCommand(hWnd,0,key);
	PPvLoadCust();
	{
		HDC hDC;

		DeleteObject(C_BackBrush);
		C_BackBrush = CreateSolidBrush(C_back);
		DeleteObject(hStatusLine);
		hStatusLine = CreateSolidBrush(C_line);

		DeleteFonts();
		hDC = GetDC(hMainWnd);
		MakeFonts(hDC,X_textmag);
		ReleaseDC(hMainWnd,hDC);
	}
	UnloadWallpaper(&BackScreen);
	LoadWallpaper(&BackScreen,hWnd,RegCID);
	FullDraw = X_fles | BackScreen.X_WallpaperType;
	if ( BackScreen.X_WallpaperType ) X_scrm = 0;
	InitGui();
	SetScrollBar();
	InvalidateRect(hWnd,NULL,TRUE);
	break;
//----------------------------------------------- Print setup
case K_c | 'U':
	GetCustData(T("X_prts"),&PrintInfo.X_prts,sizeof(PrintInfo.X_prts));
	wsprintf(buf,T("%d,%d,%d,%d, %d"),
			PrintInfo.X_prts.margin.left,PrintInfo.X_prts.margin.top,
			PrintInfo.X_prts.margin.right,PrintInfo.X_prts.margin.bottom,
			PrintInfo.X_prts.imagedivision);
	if ( tInput(hWnd,T("left,top,right,bottom, Image DPI(-1:Full Page)"),
			buf,TSIZEOF(buf),PPXH_NUMBER,PPXH_NUMBER) > 0 ){
		LONG *value = &PrintInfo.X_prts.margin.left;
		const TCHAR *p = buf;
		int values;
		for ( values = 5 ; values ; values--,value++ ){
			*value = GetNumber(&p);
			if ( SkipSpace(&p) != ',' ) break;
			p++;
		}
		SetCustData(T("X_prts"),&PrintInfo.X_prts,sizeof(PrintInfo.X_prts));
	}else{
		return ERROR_CANCELLED;
	}
	break;

case K_FIRSTCMD:
	if ( (FirstCommand != NULL) && (BackReader == FALSE) ){
		const TCHAR *cmd = FirstCommand;

		FirstCommand = NULL;
		PP_ExtractMacro(hWnd,&vinfo->info,NULL,cmd,NULL,0);
	}
	break;

//----------------------------------------------- Layout
case K_layout:
	PPvLayoutCommand();
	break;

//===============================================
default:
	if ( !PPxCommonCommand(hWnd,0,key) ) break;
//----------------------------------------------- Menu
	if ( !(X_win & XWIN_MENUBAR) && !(key & K_v) &&
				((key & (K_e | K_s | K_c | K_a)) == (K_a)) ){
		SystemDynamicMenu(&DynamicMenu,&PPvInfo.info,key);
		break;
	}
	return ERROR_INVALID_FUNCTION;
	}
	return NO_ERROR;
}
