/*-----------------------------------------------------------------------------
	Paper Plane cUI											Combo Window
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <commctrl.h>
#include <dbt.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCUI.RH"
#pragma hdrstop

#define CGLOBALDEFINE
#include "PPCOMBO.H" // グローバル変数定義
#define MinWidth 10 // 各ペインの最小幅／丈

//------------------------------------- debug
#define MESSAGEDEBUG 0	// combo内のウィンドウメッセージ追跡をするか
const TCHAR *ChangeReason = T("no");
int AddDelCount = 0; // 再入チェック用

//------------------------------------- Combo window 自体の情報
COMBOSTRUCT Combo;
TCHAR ComboID[] = T("CBA");

POINT ClientPos;
ThSTRUCT thGuiWork;	// リバー関連の情報保存に使用する
LPCTSTR nowCsrName = NULL;		// 現在のマウスカーソル
HCURSOR hCsr;

int TopDockHeight = 0;		// 下側Dock Window の高さ
int BottomDockHeight = 0;	// 下側Dock Window の高さ
int InfoHeight = 0;		// 情報行の高さ
int TabHeight = 0;		// Tab window の高さ
int AddrHeight = 0;		// Address window の高さ
int ComboInit = 1;		// combo window 作成中かどうか(終了中は -1 にする予定)

//------------------------------------- Combo window に登録されたpaneの情報
HWND hComboFocus = NULL;		// 現在のフォーカス
HWND hComboRightFocus = NULL;	// 現在の右側窓フォーカス(1ペインの時は、隠れている反対窓)


#define DMS_PANE 0	// ペインNo.最小値
#define DMS_NONE -1	// 該当無し
#define DMS_TOP  -2	// 情報行等
#define DMS_LEFT -3	// 左エリアの右
#define DMS_REPORT -4	// report上
#define DMS_JOB -5	// job上
int MoveSplit = DMS_NONE; // 窓幅調節するPaneNo

#define TabHeight_fix 1

const TCHAR CloseButtonChar = 'x';

//-------------------------------------
LRESULT WmComboCommand(HWND hWnd,WPARAM wParam,LPARAM lParam);
void WmComboPosChanged(HWND hWnd);

LRESULT ComboGetPPcList(void)
{
	int tabid;
	ThSTRUCT list;

	ThInit(&list);
	if ( Combo.Tabs <= 0 ){ // タブ無し
		for ( tabid = 0 ; tabid < Combo.ShowCount ; tabid++ ){
			int baseindex;
			PPC_APPINFO *cinfo;

			baseindex = Combo.show[tabid].baseNo;
			if ( baseindex < 0 ) continue;

			cinfo = Combo.base[baseindex].cinfo;
			if ( (cinfo != NULL) && (cinfo->path[0] != '\0') ){
				ThAddString(&list,cinfo->RegSubCID);
				ThAddString(&list,cinfo->path);
			}
		}
	}else for ( tabid = 0 ; tabid < Combo.ShowCount ; tabid++ ){
		HWND hTabWnd = Combo.show[tabid].tab.hWnd;
		int tabcount,tabindex;

		tabcount = TabCtrl_GetItemCount(hTabWnd);
		for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
			TC_ITEM tie;
			int baseindex;
			PPC_APPINFO *cinfo;

			tie.mask = TCIF_PARAM;
			if ( TabCtrl_GetItem(hTabWnd,tabindex,&tie) == FALSE ) continue;

			baseindex = GetComboBaseIndex((HWND)tie.lParam);
			if ( baseindex < 0 ) continue;

			cinfo = Combo.base[baseindex].cinfo;
			if ( (cinfo != NULL) && (cinfo->path[0] != '\0') ){
				ThAddString(&list,cinfo->RegSubCID);
				ThAddString(&list,cinfo->path);
			}
		}
		if ( !(X_combos[0] & CMBS_TABEACHITEM) ){
			break; // 左端タブの内容だけで問題ない
		}
		if ( tabid < (Combo.ShowCount - 1) ){
			ThAddString(&list,T("\t"));
			ThAddString(&list,T("\t")); // 区切り
		}
	}
	ThAddString(&list,T(""));
	return (LRESULT)list.bottom;
}

void WmComboDpiChanged(HWND hWnd,WPARAM wParam,RECT *newpos)
{
	int i;
	HFONT hNewControlFont;

	if ( newpos != NULL ){
		DWORD newDPI = HIWORD(wParam);

		if ( !(X_dss & DSS_ACTIVESCALE) ) return;
		if ( Combo.FontDPI == newDPI ) return; // 変更無し(起動時等)
		Combo.FontDPI = HIWORD(wParam);
	}

	for ( i = 0 ; i < Combo.BaseCount ; i++ ){
		PPC_APPINFO *cinfo;

		cinfo = Combo.base[i].cinfo;
		if ( cinfo != NULL ){
			SendMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_CHENGEDDISPDPI,wParam);
		}
	}

	if ( newpos != NULL ){
		SetWindowPos(hWnd,NULL,newpos->left,newpos->top,
				newpos->right - newpos->left, newpos->bottom - newpos->top,
				SWP_NOACTIVATE | SWP_NOZORDER);
	}
	InitComboGUI();
	if ( Combo.Tabs || (Combo.hTreeWnd != NULL) ){
		hNewControlFont = GetControlFont(Combo.FontDPI,&Combo.cfs);
		for ( i = 0 ; i < Combo.Tabs ; i++ ){
			SendMessage(Combo.show[i].tab.hWnd,WM_SETFONT,(WPARAM)hNewControlFont,TMAKELPARAM(TRUE,0));
		}
		if ( Combo.hTreeWnd != NULL ){
			SendMessage(Combo.hTreeWnd,VTM_CHANGEDDISPDPI,wParam,0);
		}
	}

	ComboSize.cx = 0;
	WmComboPosChanged(hWnd);
}

BOOL WmComboCopyData(COPYDATASTRUCT *copydata)
{
	switch ( LOWORD(copydata->dwData) ){
		case KC_MOREPPC:
			SendCallPPc(copydata);
			return TRUE;

		case K_WINDDOWLOG:
			WmComboCommand(NULL,K_WINDDOWLOG,(LPARAM)copydata->lpData);
			return TRUE;
	}
	return FALSE;
}

// フォーカスをアクティブペインに設定する
void WmComboSetFocus(void)
{
	HWND hFocusWnd;
	int showindex;

	if ( Combo.BaseCount == 0 ) return;
	hFocusWnd = hComboFocus;
	showindex = GetComboShowIndex(hFocusWnd);	// hComboFocus が実在するか確認
	if ( showindex < 0 ){			// 実在しないときは左端ペイン
		if ( GetComboBaseIndex(hFocusWnd) < 0 ){
			hFocusWnd = Combo.base[Combo.show[0].baseNo].hWnd;
		}
	}else{
		if ( GetFocus() == hFocusWnd ) return; // フォーカス移動済み
	}

	InvalidateRect(Combo.hWnd,NULL,TRUE); // pane ,status,info,tab
	SetFocus(hFocusWnd);
	ChangeReason = T("WmComboSetFocus");
}

#define SortPane_Fix(RANGEMIN_,RANGEMAX_,posmin,posmax,minsize,fixsize) {\
	int I_,RANGE_,addr C4701CHECK,X_ = Combo.Panes.clientbox.posmin;\
\
	RANGE_ = Combo.Panes.clientbox.posmax;\
	for ( I_ = RANGEMIN_ ; I_ < RANGEMAX_ ; I_++ ){\
		int panerange;\
\
		cps = &Combo.show[I_];\
		panerange = cps->box.posmax - cps->box.posmin;\
		if ( (X_ + panerange) > RANGE_ ){\
			if ( !(X_combos[0] & CMBS_TABFRAME) ){\
				panerange = RANGE_ - X_;\
				if ( panerange < minsize ) panerange = minsize;\
			}\
		}\
		cps->box.posmin = X_;\
		X_ += panerange;\
		cps->box.posmax = addr = X_;\
		X_ += splitwide + fixsize;\
		if ( (I_ < (RANGEMAX_ - 1)) && (X_ >= RANGE_) && (panerange > 100)){\
			if ( !(X_combos[0] & CMBS_TABFRAME) ){\
				X_ = cps->box.posmax = cps->box.posmin + panerange / 2;\
				X_ += splitwide + fixsize;\
			}\
		}\
	}\
	if ( Combo.show[I_ - 1].box.posmax < RANGE_ ){\
		Combo.show[I_ - 1].box.posmax = addr = RANGE_;\
	}\
	Combo.Panes.clientbox.posmax = addr;\
}

#define SortPane_ResizeSize(posmin,posmax,pos,fixsize) {\
	int I_,RANGE_,addr C4701CHECK;\
\
	RANGE_ = Combo.Panes.clientbox.posmin;\
	for ( I_ = 0 ; I_ < Combo.ShowCount ; I_++ ){\
		int panerange;\
\
		cps = &Combo.show[I_];\
		panerange = cps->box.posmax - cps->box.posmin;\
		cps->box.posmin = RANGE_;\
		RANGE_ += panerange;\
		cps->box.posmax = addr = RANGE_;\
		RANGE_ += splitwide + fixsize;\
	}\
	pos = addr - Combo.Panes.clientbox.posmin;\
}

#define SortPane_Setpos(posmin,posmax) {\
	int I_;\
\
	for ( I_ = 0 ; I_ < Combo.ShowCount ; I_++ ){\
		cps = &Combo.show[I_];\
		cps->box.posmin = Combo.Panes.clientbox.posmin;\
		cps->box.posmax = Combo.Panes.clientbox.posmax;\
\
		DeferWindowPos(hdWins,Combo.base[cps->baseNo].hWnd,NULL,\
				cps->box.left,cps->box.top,\
				cps->box.right - cps->box.left,\
				cps->box.bottom - cps->box.top,\
				SWP_NOACTIVATE | SWP_NOZORDER);\
	}\
}

#define SortPane_MakeResizeRates(posmin,posmax) {\
	int I_,base = Combo.Panes.clientbox.posmin;\
\
	Combo.Panes.resizewidth = Combo.Panes.clientbox.posmax - base;\
	for ( I_ = 0 ; I_ < Combo.ShowCount ; I_++ ){\
		Combo.show[I_].resizepos = Combo.show[I_].box.posmax - base;\
	}\
}

#define SortPane_ResizeRates(RANGEMIN_,RANGEMAX_,posmin,posmax) {\
	int base,x,I_,nw,resizewidth,newb;\
\
	x = base = Combo.Panes.clientbox.posmin;\
	nw = Combo.Panes.clientbox.posmax - base;\
	resizewidth = Combo.Panes.resizewidth;\
\
	for ( I_ = RANGEMIN_ ; I_ < RANGEMAX_ ; I_++ ){\
		Combo.show[I_].box.posmin = x;\
		newb = base + ( (Combo.show[I_].resizepos * nw) / resizewidth);\
		if ( newb < x ) newb = x;\
		Combo.show[I_].box.posmax = newb;\
		x = newb + splitwide;\
	}\
}

// ウィンドウの表示を並べ直す
// fixshowindex < 0 なら ComboWindow に収まるように調整
// fixshowindex >= 0 なら ComboWindow の大きさを調整,高さはfixshowindex窓を基準
void SortComboWindows(int fixshowindex)
{
	int PaneRight,PaneBottom;
	RECT pbox,cbox;
	COMBOPANES *cps;
	HDWP hdWins;

	AREA ToolBox;
	AREA InfoBox;
	AREA AddrBox;
	AREA TreeBox;
	AREA TabBox;
	AREA JobBox;

	RECT temprect = { 0,0,1000,1000 };

	if ( !Combo.ShowCount ) return;
//	CheckComboTable(T("@SortWindows-pre"));
	DEBUGLOGF("SortWindows - %d",fixshowindex);
	TopDockHeight = comboDocks.t.client.bottom;
	BottomDockHeight = comboDocks.b.client.bottom;

	// InfoHeight を再計算する
	if ( X_combos[0] & CMBS_COMMONINFO ){
		int showindex;

		if ( comboDocks.t.hInfoWnd || comboDocks.b.hInfoWnd ){
			InfoHeight = 0;
		}else{
			InfoHeight = Combo.Font.size.cy * 2;
			for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
				if ( cinfo == NULL ) continue;
				InfoHeight = cinfo->fontY *
						(cinfo->inf1.height + cinfo->inf2.height);
				break;
			}
			if ( XC_ifix && (InfoHeight < XC_ifix) ) InfoHeight = XC_ifix;
		}
	}

	if ( fixshowindex == SORTWIN_RESIZE ){
		if ( Combo.Panes.resizewidth < 0 ){
			// 大きさ変更時の比率を生成
			if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
				SortPane_MakeResizeRates(left,right);
			}else{
				SortPane_MakeResizeRates(top,bottom);
			}
		}
		// div 0 防止
		if ( Combo.Panes.resizewidth == 0 ) Combo.Panes.resizewidth = 1;
	}else{
		if ( fixshowindex != SORTWIN_FIXINNER ) Combo.Panes.resizewidth = -1;
	}

	if ( X_combos[0] & CMBS_VPANE ){ // 縦配列は、タブ高さを初期化
		int showindex;

		for ( showindex = 1 ; showindex <= Combo.ShowCount ; showindex++ ){
			Combo.show[showindex].tab.height = 0;
		}
	}

	if ( Combo.Tabs ){ // タブ高さを算出
		TabCtrl_AdjustRect(Combo.show[0].tab.hWnd,FALSE,&temprect);
		Combo.show[0].tab.height = TabHeight = temprect.top + TabHeight_fix;

		if ( X_combos[0] & CMBS_TABSEPARATE ){
			int tabs = 1;

			if ( Combo.Tabs > 1 ){
				if ( X_combos[0] & CMBS_TABMULTILINE ){ // 多段表示なら高さ調整
					for ( ; tabs < Combo.Tabs ; tabs++ ){
						temprect.top = 0;
						TabCtrl_AdjustRect(Combo.show[tabs].tab.hWnd,FALSE,&temprect);
						Combo.show[tabs].tab.height = temprect.top + TabHeight_fix;
						if ( !(X_combos[0] & CMBS_VPANE) ){ // 横
							if ( TabHeight <= temprect.top ) TabHeight = temprect.top + TabHeight_fix;
						}
					}
				}else if ( X_combos[0] & CMBS_VPANE ){ // 縦表示なら２つ目以降の設定

					for ( ; tabs < Combo.Tabs ; tabs++ ){
						Combo.show[tabs].tab.height = Combo.show[0].tab.height;
					}
				}
			}
//			Combo.show[tabs].tab.height = 0; // はみ出したところの調整
		}
	}

	Combo.TopAreaHeight = TopDockHeight + Combo.ToolBar.Height + InfoHeight + AddrHeight;

	Combo.Panes.box.left	= Combo.LeftAreaWidth;
	Combo.Panes.box.right	= ComboSize.cx;
	if ( X_combos[1] & CMBS1_TABBOTTOM ){
		Combo.Panes.box.top		= Combo.TopAreaHeight;
		Combo.Panes.box.bottom	= ComboSize.cy - Combo.BottomAreaHeight - BottomDockHeight - TabHeight;
	}else{
		Combo.Panes.box.top		= Combo.TopAreaHeight + TabHeight;
		Combo.Panes.box.bottom	= ComboSize.cy - Combo.BottomAreaHeight - BottomDockHeight;
	}

	if ( X_combos[0] & CMBS_TABFRAME ){
		Combo.Panes.clientbox.left = 0;
		Combo.Panes.clientbox.right = Combo.Panes.box.right - Combo.Panes.box.left;
		Combo.Panes.clientbox.top = 0;
		Combo.Panes.clientbox.bottom = Combo.Panes.box.bottom - Combo.Panes.box.top - GetSystemMetrics(SM_CYHSCROLL);

		Combo.Panes.clientbox.left -= Combo.Panes.delta.x;
		Combo.Panes.clientbox.right -= Combo.Panes.delta.x;
		Combo.Panes.clientbox.top -= Combo.Panes.delta.y;
		Combo.Panes.clientbox.bottom -= Combo.Panes.delta.y;
	}else{
		Combo.Panes.clientbox = Combo.Panes.box;
	}
	hdWins = BeginDeferWindowPos(Combo.ShowCount + 7);
	if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
		if ( fixshowindex < 0 ){	// 基準indexなしのとき
			if ( (X_combos[0] & CMBS_QPANE) && (Combo.ShowCount > 2) ){ // 複数行
				int line,maxlines = (Combo.ShowCount + 1) / 2;
				int w,wm,hdelta,i;

				hdelta = Combo.Panes.clientbox.bottom - Combo.Panes.clientbox.top;
				for ( line = 0 ; line < maxlines ; line++ ){
					w = line * 2;
					wm = w + 2;
					if ( wm > Combo.ShowCount ) wm--;

					if ( fixshowindex == SORTWIN_RESIZE ){
						SortPane_ResizeRates(w,wm,left,right);
					}

					if ( (w + 1) == Combo.ShowCount ){
						Combo.show[w].box.left = Combo.Panes.clientbox.left;
						Combo.show[w].box.top = Combo.Panes.clientbox.top
							+ ((hdelta * line) / maxlines);
						Combo.show[w].box.right = Combo.Panes.clientbox.right;
						Combo.show[w].box.bottom = Combo.Panes.clientbox.bottom;
					}else{
						SortPane_Fix(w,wm,left,right,MinWidth,0);

						Combo.show[w].box.top = Combo.show[w + 1].box.top = Combo.Panes.clientbox.top
							+ ((hdelta * line) / maxlines);
						Combo.show[w].box.bottom = Combo.show[w + 1].box.bottom = Combo.Panes.clientbox.top
							+ ((hdelta * (line + 1)) / maxlines);
					}

					if ( (line > 0) && (X_combos[0] & CMBS_TABSEPARATE) &&
						 ((Combo.show[w].box.bottom - Combo.show[w].box.top) >
							Combo.show[w].tab.height) ){
						Combo.show[w].box.top += Combo.show[w].tab.height;
						if ( (w + 1) != Combo.ShowCount ){
							Combo.show[w + 1].box.top += Combo.show[w + 1].tab.height;
						}
					}
				}
				for ( i = 0 ; i < Combo.ShowCount ; i++ ){
					cps = &Combo.show[i];
					DeferWindowPos(hdWins,Combo.base[cps->baseNo].hWnd,NULL,
							cps->box.left,cps->box.top,
							cps->box.right - cps->box.left,
							cps->box.bottom - cps->box.top,
							SWP_NOACTIVATE | SWP_NOZORDER);
				}
			}else{ // 通常
				if ( fixshowindex == SORTWIN_RESIZE ){
					SortPane_ResizeRates(0,Combo.ShowCount,left,right);
				}
				SortPane_Fix(0,Combo.ShowCount,left,right,MinWidth,0);
			}

			PaneRight = Combo.Panes.box.right;
			PaneBottom = Combo.Panes.box.bottom;
		}else{	// 基準indexありのとき
			if ( (X_combos[0] & CMBS_QPANE) && (Combo.ShowCount > 2) ){
				PaneBottom = Combo.Panes.box.bottom;
				PaneRight = Combo.Panes.box.right;
			}else{
				SortPane_ResizeSize(left,right,PaneRight,0);
				if ( fixshowindex >= SORTWIN_FIX_NORESIZE ){
					fixshowindex = SORTWIN_LAYOUTPAIN;
					PaneBottom = Combo.Panes.box.bottom;
				}else{
					if ( fixshowindex >= Combo.ShowCount ) fixshowindex = 0;
					PaneBottom = Combo.show[fixshowindex].box.bottom -
						 Combo.show[fixshowindex].box.top + Combo.Panes.box.top;
				}
			}

			if ( X_combos[0] & CMBS_TABFRAME ){
				PaneRight = Combo.Panes.box.right;
				PaneBottom += GetSystemMetrics(SM_CYHSCROLL);
			}else{
				PaneRight += Combo.Panes.box.left;
			}
		}
		if ( !(X_combos[0] & CMBS_QPANE) || (Combo.ShowCount <= 2) ){
			SortPane_Setpos(top,bottom);
		}
	}else{ // 縦配列
		if ( fixshowindex < 0 ){	// 基準indexなしのとき
			if ( fixshowindex == SORTWIN_RESIZE ){
				SortPane_ResizeRates(0,Combo.ShowCount,top,bottom);
			}

			SortPane_Fix(0,Combo.ShowCount,top,bottom,24,(cps+1)->tab.height);
			PaneRight = Combo.Panes.box.right;
			PaneBottom = Combo.Panes.box.bottom;
		}else{	// 基準indexありのとき
			SortPane_ResizeSize(top,bottom,PaneBottom,(cps+1)->tab.height);
			if ( fixshowindex >= SORTWIN_FIX_NORESIZE ){
				fixshowindex = SORTWIN_LAYOUTPAIN;
				PaneRight = Combo.Panes.box.right;
			}else{
				if ( fixshowindex >= Combo.ShowCount ) fixshowindex = 0;

				PaneRight = Combo.show[fixshowindex].box.right -
					Combo.show[fixshowindex].box.left + Combo.LeftAreaWidth;
			}
			if ( X_combos[0] & CMBS_TABFRAME ){
				PaneBottom = Combo.Panes.box.bottom;
			}else{
				PaneBottom += Combo.Panes.box.top;
			}
		}
		SortPane_Setpos(left,right);
	}
	PaneBottom += Combo.BottomAreaHeight + BottomDockHeight;

	if ( X_combos[0] & CMBS_TABFRAME ){
		SCROLLINFO sinfo;
		int showpane;

		EndDeferWindowPos(hdWins); // ←ここでSendMessageの受付が起きる
		hdWins = BeginDeferWindowPos(7+1);

							// frame の幅を算出
		sinfo.nMax = 0;
		for ( showpane = 0 ; showpane < Combo.ShowCount ; showpane++ ){
			sinfo.nMax += Combo.show[showpane].box.right - Combo.show[showpane].box.left + splitwide;
		}

		sinfo.cbSize = sizeof(sinfo);
		sinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		sinfo.nMin = 0;
		sinfo.nPage = Combo.Panes.clientbox.right - Combo.Panes.clientbox.left;
		sinfo.nPos = Combo.Panes.delta.x;
		SetScrollInfo(Combo.Panes.hWnd,SB_HORZ,&sinfo,TRUE);

		DeferWindowPos(hdWins,Combo.Panes.hWnd,NULL,
				Combo.Panes.box.left,Combo.Panes.box.top,
				Combo.Panes.box.right - Combo.Panes.box.left,
				Combo.Panes.box.bottom - Combo.Panes.box.top,
				SWP_NOZORDER | SWP_NOACTIVATE );
	}

	ToolBox.x	= 0;
	ToolBox.y		= TopDockHeight;
	ToolBox.width	= PaneRight;
	ToolBox.height	= Combo.ToolBar.Height;

//	InfoBox.x	= 0;
	InfoBox.y		= ToolBox.y + ToolBox.height;
//	InfoBox.width	= PaneRight;
	InfoBox.height	= InfoHeight;

	AddrBox.x	= 0;
	AddrBox.y		= InfoBox.y + InfoBox.height;
	AddrBox.width	= PaneRight;
	AddrBox.height	= AddrHeight;

	TreeBox.x	= 0;
	TreeBox.y		= Combo.TopAreaHeight;
	TreeBox.width	= Combo.LeftAreaWidth;
	TreeBox.height	= Combo.Panes.box.bottom - TreeBox.y;

	TabBox.x		= Combo.LeftAreaWidth;
	TabBox.y		= (X_combos[1] & CMBS1_TABBOTTOM) ? Combo.Panes.box.bottom : TreeBox.y;
	TabBox.width	= PaneRight - Combo.LeftAreaWidth;
	TabBox.height	= TabHeight;

	Combo.Report.box.x	= 0;
	Combo.Report.box.y	= Combo.Panes.box.bottom + splitwide + ((X_combos[1] & CMBS1_TABBOTTOM) ? TabHeight : 0);
	Combo.Report.box.width	= PaneRight;
	Combo.Report.box.height	= Combo.BottomAreaHeight - splitwide;

	if ( Combo.Joblist.hWnd != NULL ){ // **1
		if ( Combo.Report.hWnd != NULL ){ // ログの右に表示
			JobBox.width = Combo.Joblist.JobAreaWidth;
			Combo.Report.box.width -= JobBox.width + splitwide;
			if ( Combo.Report.box.width < (splitwide * 3) ){
				Combo.Report.box.width = (splitwide * 3);
			}
		}else if ( Combo.hTreeWnd != NULL ){ // ツリー下に表示
			JobBox.height = TreeBox.height / 4;
			TreeBox.height -= JobBox.height;
		}
	}

	if ( (fixshowindex != SORTWIN_LAYOUTPAIN) &&
		 (fixshowindex != SORTWIN_FIXINNER) ){
		if ( comboDocks.t.hWnd != NULL ){
			DeferWindowPos(hdWins,comboDocks.t.hWnd,NULL,0,0,
					PaneRight + REBARFIXWIDTH,TopDockHeight,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}
		if ( comboDocks.b.hWnd != NULL ){
			DeferWindowPos(hdWins,comboDocks.b.hWnd,NULL,
					0,PaneBottom - BottomDockHeight,
					PaneRight + REBARFIXWIDTH,BottomDockHeight,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}

		if ( Combo.Joblist.hWnd != NULL ){
			if ( Combo.Report.hWnd != NULL ){ // ログの右に表示
				JobBox.x = Combo.Report.box.x + Combo.Report.box.width + splitwide;
				JobBox.y = Combo.Report.box.y;
				JobBox.height = Combo.Report.box.height;
			}else if ( Combo.hTreeWnd != NULL ){ // ツリー下に表示
				JobBox.x = TreeBox.x;
				JobBox.y = TreeBox.y + TreeBox.height;
				JobBox.width = TreeBox.width - splitwide;
			}else{	// 下段に表示
				JobBox.x = 0;
				JobBox.y = Combo.Panes.box.bottom + splitwide;
				JobBox.width = PaneRight;
				JobBox.height = Combo.BottomAreaHeight - splitwide;
			}
			// C4701ok **1 で初期化済み
			DeferWindowPos(hdWins,Combo.Joblist.hWnd,NULL,
					JobBox.x,JobBox.y,JobBox.width,JobBox.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}

		if ( Combo.hTreeWnd != NULL ){
			DeferWindowPos(hdWins,Combo.hTreeWnd,NULL,
					TreeBox.x,TreeBox.y,
					TreeBox.width - splitwide,TreeBox.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}

		if ( Combo.Report.hWnd != NULL ){
			DeferWindowPos(hdWins,Combo.Report.hWnd,NULL,
					Combo.Report.box.x,Combo.Report.box.y,
					Combo.Report.box.width,Combo.Report.box.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}

		if ( Combo.hAddressWnd != NULL ){
			DeferWindowPos(hdWins,Combo.hAddressWnd,NULL,
					AddrBox.x,AddrBox.y,
					AddrBox.width - AddrHeight,AddrBox.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}

		if ( Combo.ToolBar.hWnd != NULL ){
			DeferWindowPos(hdWins,Combo.ToolBar.hWnd,NULL,
					ToolBox.x,ToolBox.y,
					ToolBox.width,ToolBox.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}

	if ( Combo.Tabs ){
		if ( Combo.Tabs == 1 ){
			DeferWindowPos(hdWins,Combo.show[0].tab.hWnd,NULL,
					TabBox.x,TabBox.y,
					TabBox.width,TabBox.height,
					SWP_NOACTIVATE | SWP_NOZORDER);
		}else{
			int tabpane;
			#define TabTail(pane) ((pane < Combo.Tabs - 1) ? splitwide : 0)

			if ( X_combos[0] & CMBS_VPANE ){ // 縦整列
				for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
					RECT *box;

					box = &Combo.show[tabpane].box;
					DeferWindowPos(hdWins,Combo.show[tabpane].tab.hWnd,NULL,
						box->left,
						box->top - Combo.show[tabpane].tab.height,
						box->right - box->left,
						Combo.show[tabpane].tab.height,
						SWP_NOACTIVATE | SWP_NOZORDER);
				}
						// 横整列
			}else if ( ((X_combos[0] & (CMBS_QPANE | CMBS_TABSEPARATE)) == (CMBS_QPANE | CMBS_TABSEPARATE)) && (Combo.ShowCount > 2) ){
				// 田形
				for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
					RECT *box;

					box = &Combo.show[tabpane].box;
					DeferWindowPos(hdWins,Combo.show[tabpane].tab.hWnd,NULL,
							box->left,
							box->top - Combo.show[tabpane].tab.height,
							box->right - box->left + TabTail(tabpane),
							Combo.show[tabpane].tab.height,
							SWP_NOACTIVATE | SWP_NOZORDER);
				}
			}else if ( X_combos[1] & CMBS1_TABFIXLAYOUT ){ // 比率固定
				int x = Combo.Panes.clientbox.left,nx;
				int w = Combo.Panes.clientbox.right - Combo.Panes.clientbox.left;
				for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
					nx = Combo.Panes.clientbox.left + ((tabpane + 1) * w) / Combo.Tabs;

					DeferWindowPos(hdWins,Combo.show[tabpane].tab.hWnd,NULL,
							x,TabBox.y,
							nx - x,TabBox.height,
							SWP_NOACTIVATE | SWP_NOZORDER);
					x = nx;
				}
			}else{ // 通常
				for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
					RECT *box;

					box = &Combo.show[tabpane].box;
					DeferWindowPos(hdWins,Combo.show[tabpane].tab.hWnd,NULL,
						box->left,TabBox.y,
						box->right - box->left + TabTail(tabpane),TabBox.height,
						SWP_NOACTIVATE | SWP_NOZORDER);
				}
			}
		}
	}
	EndDeferWindowPos(hdWins); // ←ここでSendMessageの受付が起きる

	// 整列後のコントロールの再調整
	if ( (fixshowindex != SORTWIN_FIXINNER) && // タブ行数が変化していたら再整列
		 (X_combos[0] & CMBS_TABMULTILINE) &&
		 Combo.Tabs ){
		int tabh = 0;
		int tabs;

		for ( tabs = 0 ; tabs < Combo.Tabs ; tabs++ ){
			temprect.top = 0;
			TabCtrl_AdjustRect(Combo.show[tabs].tab.hWnd,FALSE,&temprect);
			if ( tabh <= temprect.top ) tabh = temprect.top;
		}
		if ( TabHeight != (tabh + TabHeight_fix) ){
			SortComboWindows(SORTWIN_FIXINNER);
			return;
		}
	}

	if ( Combo.ToolBar.hWnd != NULL ){ //下にずれるので再設定する
		GetWindowRect(Combo.ToolBar.hWnd,&pbox);
		if ( pbox.top != ToolBox.x ){
			SetWindowPos(Combo.ToolBar.hWnd,NULL,
				ToolBox.x,ToolBox.y,0,0,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
		}
	}
	if ( fixshowindex >= 0 ){
		int nw;
							// Combo window の大きさを調節する
		GetWindowRect(Combo.hWnd,&pbox);
		GetClientRect(Combo.hWnd,&cbox);

		nw = PaneRight + ((pbox.right - pbox.left) - (cbox.right - cbox.left));

		if ( nw >= 16 ){
			Combo.Panes.resizewidth = -2;
			// SortComboWindows の再入がここで発生
			SetWindowPos(Combo.hWnd,NULL,0,0,
				nw,
				PaneBottom + ((pbox.bottom - pbox.top) - (cbox.bottom - cbox.top)),
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
		}
	}
	DEBUGLOGF("SortWindows - %d end",fixshowindex);
}

void WmComboNotify(NMHDR *nmh)
{
	if ( nmh->hwndFrom == NULL ) return;

	if ( IsTrue(DocksNotify(&comboDocks,nmh)) ){
		if ( nmh->code == RBN_HEIGHTCHANGE ){
			SortComboWindows(SORTWIN_LAYOUTALL);
		}
		return;
	}
	if ( nmh->code == TTN_NEEDTEXT ){
		if ( SetTabTipText(nmh) ) return;
		if ( SetToolBarTipText(Combo.ToolBar.hWnd,&thGuiWork,nmh) ) return;
		if ( DocksNeedTextNotify(&comboDocks,nmh) ) return;
		return;
	}
	if ( nmh->hwndFrom == Combo.ToolBar.hWnd ){
		if ( nmh->code == NM_RCLICK ){
			if ( IsTrue(ToolBarDirectoryButtonRClick(Combo.hWnd,nmh,&thGuiWork)) ){
				return;
			}
			PostMessage(hComboFocus,WM_PPXCOMMAND,
					TMAKELPARAM(K_POPOPS,PPT_MOUSE),0);
			PostMessage(hComboFocus,WM_PPXCOMMAND,K_layout,0);
		}
		if ( nmh->code == TBN_DROPDOWN ){
			ComboToolbarCommand(((LPNMTOOLBAR)nmh)->iItem,K_s);
		}
		return;
	}
	if ( nmh->code == TCN_SELCHANGE ){
		SelectChangeTab(nmh);
		return;
	}
}

int SplitHitTest(POINT *pos)
{
	if ( pos->y < InfoBottom ) return DMS_TOP;
	if ( pos->x < Combo.LeftAreaWidth ){
		if ( pos->x >= (Combo.LeftAreaWidth - splitwide) ) return DMS_LEFT;
	}else{
		if ( pos->y < (Combo.Report.box.y - BottomDockHeight - splitwide) ){ // ペイン
			return DMS_PANE;
		}
	}
	if ( pos->y > (Combo.Report.box.y - BottomDockHeight) ){
		return DMS_JOB;
	}
	return DMS_REPORT; // 水平線
}

LRESULT ComboLMouseDown(HWND hWnd,LPARAM lParam)
{
	POINT pos;

	LPARAMtoPOINT(pos,lParam);
	if ( (pos.y < InfoTop) || (pos.x < 0) ) return 0;
	SetCapture(hWnd);
	MoveSplit = SplitHitTest(&pos);
	if ( MoveSplit == DMS_TOP ){							// 情報行
		if ( X_combos[0] & CMBS_COMMONINFO ){
			int showindex,n,r;

			showindex = GetComboShowIndexDefault(hComboFocus);
			if ( showindex >= 0 ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
				if ( cinfo == NULL ) return 0;
				r = GetHiddenMenuItemTypeFromPoint(cinfo,InfoTop,InfoHeight,&pos,&n);
				if ( r != PPCR_MENU ) n = -1;
				if ( n >= 0 ) cinfo->DownMPos = n;
			}
		}
		return 0;
	}
	if ( MoveSplit == DMS_PANE ){
		int showindex;

		showindex = GetComboShowIndexFromPos(&pos);
		if ( showindex >= 0 ) MoveSplit = DMS_PANE + showindex;
	}
	return 0;
}

LRESULT ComboMouseButton(LPARAM lParam,WORD type)
{
	POINT pos;

	LPARAMtoPOINT(pos,lParam);

	if ( (pos.y >= AddrTop) && (pos.y < AddrBottom) ){
		SendMessage(Combo.hAddressWnd,WM_PPXCOMMAND,K_cr,0);
		EnterAddressBar();
		return 0;
	}

	if ( (pos.y < InfoBottom) && (X_combos[0] & CMBS_COMMONINFO) ){
		int showindex,n,r;

		showindex = GetComboShowIndexDefault(hComboFocus);
		if ( showindex >= 0 ){
			PPC_APPINFO *cinfo;

			cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
			if ( cinfo != NULL ){
				r = GetHiddenMenuItemTypeFromPoint(cinfo,InfoTop,InfoHeight,&pos,&n);
				PostMessage(cinfo->info.hWnd,WM_PPXCOMMAND,
						KC_MOUSECMD + (type << 16),r + (n << 16));
			}
		}
	}
	return 0;
}

LRESULT ComboLMouseUp(LPARAM lParam)
{
	// ※既に WM_CAPTURECHANGED が済んでいる→ MoveSplit = DMS_NONE 状態
	ReleaseCapture();
	if ( MoveSplit == DMS_NONE ) ComboMouseButton(lParam,'L');
	MoveSplit = DMS_NONE;
	return 0;
}

LRESULT ComboLMouseDbl(HWND hWnd,LPARAM lParam)
{
	POINT pos;
	int showindex;
	HWND hTargetWnd;

	LPARAMtoPOINT(pos,lParam);
	if ( pos.y < InfoBottom ) return ComboMouseButton(lParam,'L' + ('D'<<8));

	showindex = GetComboShowIndexFromPos(&pos);
	if ( showindex >= 0 ){
		hTargetWnd = Combo.base[Combo.show[showindex].baseNo].hWnd;
		ClientToScreen(hWnd,&pos);
		PostMessage(hTargetWnd,WM_NCLBUTTONDBLCLK,
				HTRIGHT,TMAKELPARAM(pos.x,pos.y));
	}
	return 0;
}

#pragma argsused
VOID CALLBACK HideMenuTimerProc(HWND hWnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)
{
	POINT pos;
	int showindex,n = -1,r;
	PPC_APPINFO *cinfo;
	UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);

	GetCursorPos(&pos);
	ScreenToClient(hWnd,&pos);

	showindex = GetComboShowIndexDefault(hComboFocus);
	if ( showindex < 0 ) return;

	cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
	if ( cinfo == NULL ) return;
	if ( pos.y < InfoBottom ){
		r = GetHiddenMenuItemTypeFromPoint(cinfo,InfoTop,InfoHeight,&pos,&n);
		if ( r != PPCR_MENU ) n = -1;
	}
	if ( n < 0 ){
		RECT rect;

		KillTimer(hWnd,TIMERID_COMBOHIDEMENU);
		cinfo->Mpos = -1;

		rect.left	= 0;
		rect.top	= InfoTop;
		rect.right	= ComboSize.cx;
		rect.bottom	= InfoBottom;
		InvalidateRect(hWnd,&rect,FALSE);
	}
	return;
}

LRESULT ComboMouseMove(HWND hWnd,LPARAM lParam)
{
	POINT pos;
	LPCTSTR cr;

	LPARAMtoPOINT(pos,lParam);
	switch ( MoveSplit ){
		case DMS_LEFT:	// 左エリアの右境界線ドラッグ中
			if ( pos.x < (splitwide * 3) ) pos.x = splitwide * 3;
			if ( Combo.LeftAreaWidth != pos.x ){
				Combo.LeftAreaWidth = pos.x;
				SortComboWindows(SORTWIN_LAYOUTALL);	// combo 内調整
				InvalidateRect(hWnd,NULL,TRUE);
			}
			return 0;

		case DMS_REPORT:	// ログエリアの上境界線ドラッグ中
			if ( pos.y < (Combo.Panes.box.top + splitwide * 3) ){
				pos.y = Combo.Panes.box.top + splitwide * 3;
			}
			if ( pos.y > (Combo.Report.box.y + Combo.BottomAreaHeight - splitwide * 3) ){
				pos.y = (Combo.Report.box.y + Combo.BottomAreaHeight - splitwide * 3);
			}

			if ( Combo.Report.box.y != pos.y ){
				Combo.BottomAreaHeight = Combo.Report.box.y + Combo.BottomAreaHeight - pos.y;
				SortComboWindows(SORTWIN_LAYOUTALL);	// combo 内調整
				InvalidateRect(hWnd,NULL,TRUE);
			}
			return 0;

		case DMS_JOB:	// jobエリアの左境界線ドラッグ中
			if ( pos.x < (Combo.Report.box.x + splitwide * 3) ){
				pos.x = Combo.Report.box.x + splitwide * 3;
			}
			if ( pos.x > (ComboSize.cx - splitwide * 3) ){
				pos.x = (ComboSize.cx - splitwide * 3);
			}

			if ( (ComboSize.cx - Combo.Joblist.JobAreaWidth) != pos.x ){
				Combo.Joblist.JobAreaWidth = ComboSize.cx - pos.x;
				SortComboWindows(SORTWIN_LAYOUTALL);	// combo 内調整
				InvalidateRect(hWnd,NULL,TRUE);
			}
			return 0;
	}
	if ( MoveSplit >= DMS_PANE ){	// 窓枠ドラッグ中
		if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
			int x;

			x = Combo.show[MoveSplit].box.left + (splitwide / 2 + 1);
			if ( x > pos.x ) pos.x = x;
			if ( Combo.show[MoveSplit].box.right != pos.x ){
				Combo.show[MoveSplit].box.right = pos.x;
				SortComboWindows(SORTWIN_LAYOUTPAIN);	// combo 内調整
			}
		}else{ // 縦配列
			int y;

			y = Combo.show[MoveSplit].box.top + (splitwide / 2 + 1);
			if ( y > pos.y ) pos.y = y;
			if ( Combo.show[MoveSplit].box.bottom != pos.y ){
				Combo.show[MoveSplit].box.bottom = pos.y;
				SortComboWindows(SORTWIN_LAYOUTPAIN);	// combo 内調整
			}
		}
		// pane ,tab
		InvalidateRect(hWnd,NULL,TRUE);
		return 0;
	}

	// 情報行等 or 非ドラッグ
	if ( hWnd == Combo.hWnd ){
		if ( (pos.y < Combo.Panes.box.top) && (X_combos[0] & CMBS_COMMONINFO) ){ // 情報行(隠しメニュー)
			int showindex,n,r;

			showindex = GetComboShowIndexDefault(hComboFocus);
			if ( showindex >= 0 ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
				if ( cinfo != NULL ){
					r = GetHiddenMenuItemTypeFromPoint(cinfo,InfoTop,InfoHeight,&pos,&n);
					if ( r != PPCR_MENU ) n = -1;
					if ( n != cinfo->Mpos ){
						RECT rect;

						cinfo->Mpos = n;
						rect.left	= cinfo->iconR;
						rect.top	= InfoTop;
						rect.right	= ((cinfo->HiddenMenu.item + 1) >> 1) * 5 *
								cinfo->fontX + rect.left;
						rect.bottom	= InfoBottom;
						InvalidateRect(hWnd,&rect,FALSE);

						if ( n >= 0 ){
							SetTimer(hWnd,TIMERID_COMBOHIDEMENU,
									TIMER_COMBOHIDEMENU,HideMenuTimerProc);
						}
					}
				}
			}
		}
		switch ( SplitHitTest(&pos) ){	// カーソル形状を決定・設定
			case DMS_LEFT:
			case DMS_JOB:
				cr = IDC_SIZEWE;
				break;
			case DMS_REPORT:
				cr = IDC_SIZENS;
				break;
			case DMS_PANE: // ペイン
				if ( !(X_combos[0] & CMBS_VPANE) ){
					cr = IDC_SIZEWE;
				}else{
					cr = IDC_SIZENS;
				}
				break;
			default: // DMS_NONE / DMS_TOP
				cr = IDC_ARROW;
		}
	}else{ // フレーム上
		if ( !(X_combos[0] & CMBS_VPANE) ){
			cr = IDC_SIZEWE;
		}else{
			cr = IDC_SIZENS;
		}
	}

	if ( nowCsrName != cr ){
		nowCsrName = cr;
		hCsr = LoadCursor(NULL,cr);
	}
	SetCursor(hCsr);

	return 0;
}

// 非クライアント領域の判別 ---------------------------------------------------
LRESULT ComboNCMouseCommand(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	POINT pos;
	int showindex;

	LPARAMtoPOINT(pos,lParam);
	ScreenToClient(hWnd,&pos);
	showindex = GetComboShowIndexFromPos(&pos);
	if ( showindex >= 0 ){
		PostMessage(Combo.base[Combo.show[showindex].baseNo].hWnd,message,wParam,lParam);
	}
	return 0;
}

void RequestPairRate(SCW_REQSIZE *rs)
{
	int showindex,baseindex;
	RECT box,*oldbox,*pairbox;
	PPC_APPINFO *cinfo;

	if ( Combo.ShowCount < 2 ) return;	// 反対窓はない
	showindex = GetComboShowIndex(rs->hWnd);
	if ( showindex < 0 ) return;
	baseindex = Combo.show[showindex].baseNo;

	if ( (cinfo = Combo.base[baseindex].cinfo) == NULL ) return;
	oldbox = &Combo.show[showindex].box;
	box = *oldbox;
	pairbox = &Combo.show[GetPairPaneComboShowIndex(rs->hWnd)].box;

	if ( rs->mode == FPS_KEYBOARD ){	// キーボード操作
			// 上下で左右調整は不可
		if ( X_combos[0] & CMBS_VPANE ){
			if ( rs->offsetx ) return;
		}else{
			if ( rs->offsety ) return;
		}
		if ( showindex ){	// 反対窓なら反転
			rs->offsetx = -rs->offsetx;
			rs->offsety = -rs->offsety;
		}
	}else{
		if ( rs->mode == FPS_RATE ){
			int thissize,pairsize;

			if ( X_combos[0] & CMBS_VPANE ){
				thissize = box.bottom - box.top;
				pairsize = pairbox->bottom - pairbox->top;
				box.bottom += (thissize + pairsize) * rs->offsety / 100 - thissize;
			}else{
				thissize = box.right - box.left;
				pairsize = pairbox->right - pairbox->left;
				box.right += (thissize + pairsize) * rs->offsetx / 100 - thissize;
			}
		}else{
			if ( X_combos[0] & CMBS_VPANE ){
				rs->offsety = rs->offsetx;
				rs->offsetx = 0;
			}
		}
	}
	if ( rs->mode != FPS_RATE ){
		box.right += rs->offsetx * cinfo->fontX;
		box.bottom += rs->offsety * cinfo->fontY;
	}
	if ( (box.right - box.left) < cinfo->fontX ){
		box.right = box.left + cinfo->fontX;
	}
	if ( (box.bottom - box.top) < cinfo->fontY ){
		box.bottom = box.top + cinfo->fontY;
	}
	if ( EqualRect(oldbox,&box) == FALSE ){
		int delta,pairsize;

		if ( X_combos[0] & CMBS_VPANE ){ // 上下
			delta = (box.bottom - box.top) - (oldbox->bottom - oldbox->top);
			pairsize = pairbox->bottom - pairbox->top;
			if ( (pairsize - delta) < MinWidth ){
				return;	// これ以上は無理なので中止
			}
			pairbox->top += delta;
		}else{ // 左右
			delta = (box.right - box.left) - (oldbox->right - oldbox->left);
			pairsize = pairbox->right - pairbox->left;
			if ( (pairsize - delta) < MinWidth ){
				return;	// これ以上は無理なので中止
			}
			pairbox->left += delta;
		}
		*oldbox = box;
		SortComboWindows(showindex + SORTWIN_FIX_NORESIZE);
		InvalidateRect(Combo.hWnd,NULL,TRUE);
	}
}

void KCW_treeCombo(int mode,const TCHAR *path)
{
	TCHAR treepath[VFPS];

	if ( mode == PPCTREECOMMAND ){
		if ( Combo.hTreeWnd == NULL ){	// 無い→作成
			if ( !tstrcmp(path,T("off")) ) return; // close 済み
			CreateLeftArea(PPCTREE_SYNC,NilStr);
			InvalidateRect(Combo.hWnd,NULL,TRUE);
			SortComboWindows(SORTWIN_LAYOUTALL);
			if ( *path == '\0' ) return;
		}
		if ( Combo.hTreeWnd != NULL ){
			SendMessage(Combo.hTreeWnd,VTM_TREECOMMAND,0,(LPARAM)path);
		}
		return;
	}

	if ( Combo.hTreeWnd == NULL ){	// 無い→作成
		tstrcpy(treepath,path);
		CreateLeftArea(mode,treepath);
	}else{
		if ( mode == PPCTREE_SYNC ){
			CloseLeftArea();
			SetFocus(hComboFocus);
		}else{
			return;
		}
	}
	ReplyMessage(SENDCOMBO_OK); // 呼び出し元を続行させる
	InvalidateRect(Combo.hWnd,NULL,TRUE);
	SortComboWindows(SORTWIN_LAYOUTALL);
}

void USEFASTCALL CheckRightFocus(void)
{
	if ( Combo.BaseCount <= 1 ){ // 窓が１つのみ
		if ( hComboRightFocus == NULL ) return;
	}else if ( hComboRightFocus != NULL ){ // 窓が複数…必ずhComboRightFocus有効
		int showindex = GetComboShowIndex(hComboRightFocus);

		if ( Combo.ShowCount <= 1 ){ // ペインが１つのみ
			if ( showindex < 0 ) return;
		}else if ( showindex >= 1 ){
			return;
		}
	}
	ResetR(NULL);
}

LRESULT USEFASTCALL KCW_EntryCombo(HWND hEntryWnd,DWORD type)
{
	COMBOITEMSTRUCT *cws;
	WINPOS entrybox;
	int showpane = -1; // 追加した窓を表示させるためのペイン
	TCHAR id[16],value[32];

	if ( AddDelCount ){
		XMessage(NULL,NULL,XM_DbgLOG,T("combo entry - nested %x"),hEntryWnd);
		PostMessage(Combo.hWnd,WM_PPCOMBO_NESTED_ENTRY,(WPARAM)type,(LPARAM)hEntryWnd);
		return SENDCOMBO_OK;
	}
	AddDelCount++;

	if ( (Combo.BaseCount >= Combo_Max_Base) ||
		 ((cws = HeapReAlloc( hProcessHeap,0,Combo.base,sizeof(COMBOITEMSTRUCT) * (Combo.BaseCount + 2) )) == NULL) ){
		PostMessage(hEntryWnd,WM_CLOSE,0,0);
		AddDelCount--;
		ReplyMessage(SENDCOMBO_OK);
		XMessage(hEntryWnd,NULL,XM_GrERRld,MES_EOEC);
		return SENDCOMBO_OK;
	}
	Combo.base = cws;

	cws = &Combo.base[Combo.BaseCount];
	cws->hWnd = hEntryWnd;
	cws->capture = CAPTURE_NONE;
	cws->ActiveID = Combo.Active.high++;
	cws->tabbackcolor = C_AUTO;
	cws->tabtextcolor = C_AUTO;

	cws->cinfo = NULL;
	if ( LOWORD(type) == KCW_capture ){
		if ( TMAKELPARAM(1,KCW_captureEx) ==
				SendMessage(hEntryWnd,WM_PPXCOMMAND,KCW_captureEx,0) ){
			cws->capture = CAPTURE_WINDOWEX;
		}
	}else{
		DWORD PanePID;

		GetWindowThreadProcessId(hEntryWnd,&PanePID);
		if ( PanePID == GetCurrentProcessId() ){
			const TCHAR *p;

			cws->cinfo = (PPC_APPINFO *)GetWindowLongPtr(hEntryWnd,GWLP_USERDATA);
			if ( cws->cinfo != NULL ){
				wsprintf(id,T("%s_tabcolor"),(cws->cinfo->RegSubIDNo < 0) ? cws->cinfo->RegID : cws->cinfo->RegSubCID);
				value[0] = '\0';
				GetCustTable(T("_Path"),id,value,sizeof(value));
				p = value;
				if ( value[0] ){
					cws->tabtextcolor = GetNumber(&p);
					if ( *p == ',' ) p++;
					cws->tabbackcolor = GetNumber(&p);
				}
			}
		}
	}
	Combo.BaseCount++;

	if ( type >= KCW_entry_DEFPANE ){
		int pane = (type / KCW_entry_DEFPANE) - 1;

		if ( pane < 0 ) pane = 0;
		if ( pane < PSPONE_PANE_SETPANE ){
			if ( pane == PSPONE_PANE_PAIR ){
				showpane = GetPairPaneComboShowIndex(hComboFocus);
			}else if ( pane == PSPONE_PANE_RIGHTPANE ){
				showpane = GetComboShowIndex(hComboRightFocus);
			}else if ( pane == PSPONE_PANE_NEWPANE ){
				showpane = -2;
			}
		}else{
			showpane = pane - PSPONE_PANE_SETPANE;
		}
	}
				// ペインを追加して登録
	if ( (showpane == -2) ||
		 ((Combo.ShowCount < X_mpane.max) &&
		  ( (showpane < 0) || (showpane >= Combo.ShowCount) )) ){
		wsprintf(value,T("%s%d"),ComboID,Combo.ShowCount);
		if ( NO_ERROR != GetCustTable(Str_WinPos,value,&entrybox,sizeof(entrybox)) ){
			GetWindowRect(hEntryWnd,&entrybox.pos);
		}

		// ペインを追加するための隙間を準備する(窓サイズ固定時 or 起動時)
		if ( (!(X_combos[0] & CMBS_VALWINSIZE) || ComboInit) && Combo.ShowCount ){
			RECT *tbox;

			tbox = &Combo.show[0].box;
			if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
				int addwidth,i;

				if ( (X_combos[0] & CMBS_QPANE) && (Combo.ShowCount >= 2) ){
					if ( (Combo.ShowCount & 1) ){
						tbox = &Combo.show[Combo.ShowCount - 1].box;
						tbox->right = (tbox->right + tbox->left) / 2;
					}
				}else{
					addwidth = entrybox.pos.right - entrybox.pos.left + splitwide;
					if ( (tbox->right - tbox->left) >
							(addwidth + (addwidth >> 1)) ){
						tbox->right = tbox->left +
						 max(tbox->right - tbox->left - addwidth,MinWidth);
					}else{
						addwidth = ((ComboSize.cx - Combo.LeftAreaWidth)
							 - (splitwide * Combo.ShowCount)) /
							 (Combo.ShowCount + 1);

						for ( i = 0 ; i < Combo.ShowCount ; i++ ){
							tbox = &Combo.show[i].box;
							tbox->right = tbox->left + max(addwidth,MinWidth);
						}
					}
				}
			}else{ // 縦配列
				int addwidth,i;

				addwidth = entrybox.pos.bottom - entrybox.pos.top + splitwide;
				if ( (tbox->bottom - tbox->top) >
						(addwidth + (addwidth >> 1)) ){
					tbox->bottom = tbox->top +
					 max(tbox->bottom - tbox->top - addwidth,MinWidth);
				}else{
					addwidth = ((Combo.Panes.box.bottom - Combo.Panes.box.top) -
							(splitwide * Combo.ShowCount)) /
							(Combo.ShowCount + 1);
					for ( i = 0 ; i < Combo.ShowCount ; i++ ){
						tbox = &Combo.show[i].box;
						tbox->bottom = tbox->top + max(addwidth,MinWidth);
					}
				}
			}
		}
		Combo.show[Combo.ShowCount].box = entrybox.pos;
															// タブに登録する
		CreatePane(Combo.BaseCount - 1);
		CheckRightFocus();
		ShowWindow(hEntryWnd,SW_SHOWNORMAL);

		SortComboWindows((!(X_combos[0] & CMBS_VALWINSIZE) || ComboInit ) ?
				// 窓幅固定
				((Combo.ShowCount == 1) ?
					SORTWIN_LAYOUTALL : SORTWIN_LAYOUTPAIN) :
				// 窓幅可変
				(Combo.ShowCount - 1));

		if ( showpane < 0 ) showpane = Combo.ShowCount - 1;

		if ( (X_combos[1] & CMBS1_TABSHOWMULTI) && (Combo.Tabs == 0) && (Combo.ShowCount > 1) ){
			CreateTabBar(CREATETAB_APPEND);
			SortComboWindows(SORTWIN_LAYOUTPAIN);
		}
	}else{ // X_mpane.max を越えたので、ペインを追加しないで登録
		int addtabtext = Combo.Tabs;

		if ( Combo.Tabs == 0 ){
			addtabtext = (X_combos[0] & CMBS_TABEACHITEM) ? 1 : 0;
			CreateTabBar(CREATETAB_APPEND);
		}
		CheckRightFocus();
		ShowWindow(hEntryWnd,SW_HIDE);
		SortComboWindows(SORTWIN_LAYOUTPAIN);

		if ( showpane < 0 ){
			showpane = GetComboShowIndex(hComboFocus);
			if ( showpane < 0 ) showpane = 0;
		}
															// タブに登録する
		if ( addtabtext ){
			SetTabInfoData(SETTABINFO_ADDENTRY,
					(X_combos[0] & CMBS_TABEACHITEM) ? showpane : -1,hEntryWnd);
		}
	}

	if ( comboDocks.t.cinfo == NULL ){
		comboDocks.t.cinfo = cws->cinfo;
		comboDocks.b.cinfo = cws->cinfo;
		if ( comboDocks.b.hWnd != NULL ) DockFixPPcBarSize(&comboDocks.b);
		if ( comboDocks.t.hWnd != NULL ) DockFixPPcBarSize(&comboDocks.t);
	}
	if ( ComboInit == 0 ){
		if ( type & KCW_entry_SELECTNA ){
			SelectComboWindow(showpane,hEntryWnd,FALSE);
		}else if ( !(type & KCW_entry_NOACTIVE) ){
			SelectComboWindow(showpane,hEntryWnd,TRUE);
			CheckComboTable(T("KCW_EntryCombo3"));
		}
	}
	AddDelCount--;
	return SENDCOMBO_OK;
}

// hTargetWnd がフォーカスになったことを反映させる
void KCW_FocusFix(HWND hWnd,HWND hTargetWnd)
{
	int baseindex;

	if ( hComboFocus == hTargetWnd ) return;
	baseindex = GetComboBaseIndex(hTargetWnd);
	if ( baseindex < 0 ) return;

	Combo.base[baseindex].ActiveID = Combo.Active.high++;

	if ( GetComboShowIndex(hTargetWnd) < 0 ){
		int si;
		HWND hHideWnd;

		if ( X_combos[0] & CMBS_TABEACHITEM ){ // 該当ペインを検索する
			int showindex;

			si = -1;
			for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
				if ( GetTabItemIndex(Combo.base[baseindex].hWnd,showindex) >= 0 ){
					si = showindex;
					break;
				}
			}
		}else{ // カレントペイン
			si = GetComboShowIndex(hComboFocus);
		}

		if ( si < 0 ) si = 0;
		hHideWnd = Combo.base[Combo.show[si].baseNo].hWnd;

		if ( Combo.ShowCount == 1 ){
			hComboRightFocus = hHideWnd;
		}else{
			if ( hHideWnd == hComboRightFocus ){
				hComboRightFocus = hTargetWnd;
			}
		}

		Combo.show[si].baseNo = baseindex;
		SendMessage(Combo.hWnd,WM_SETREDRAW,FALSE,0);
		ShowWindow(hHideWnd,SW_HIDE);
		SortComboWindows(SORTWIN_LAYOUTPAIN);
		SendMessage(Combo.hWnd,WM_SETREDRAW,TRUE,0);
		ShowWindow(hTargetWnd,SW_SHOWNORMAL);

		ChangeReason = T("FocusFix0");
	}else{
		// 反対窓の情報設定
		if ( Combo.BaseCount > 1 ){
			if ( Combo.ShowCount == 1 ){ // １ペインのみ…現在表示が反対窓に
				hComboRightFocus = hComboFocus;
				if ( (hComboRightFocus == NULL) ||
					 (GetComboBaseIndex(hComboRightFocus) < 0) ){
					hComboRightFocus = Combo.base[baseindex ? 0 : 1].hWnd;
				}
				ChangeReason = T("FocusFix1");
			}else{ // ２ペイン以上
				int rightpane;

				ChangeReason = T("FocusFix2");
				rightpane = GetComboShowIndex(hTargetWnd);
				if ( rightpane >= 1 ){ // 右側変更
					hComboRightFocus = hTargetWnd;
					ChangeReason = T("FocusFix3");
				}else if ( GetComboShowIndex(hComboRightFocus) < 1 ){
					// hComboRightFocus が左端か、存在しない
					hComboRightFocus = NULL;
					ChangeReason = T("FocusFix4");
				}
				if ( (hComboRightFocus == NULL) ||
					 (GetComboBaseIndex(hComboRightFocus) < 0) ){
					hComboRightFocus = Combo.base[Combo.show[1].baseNo].hWnd;
					//  hComboRightFocus == hTargetWnd , rightpane == 0 がある？
					ChangeReason = T("FocusFix5");
				}
			}
		}else{ // 反対窓が存在しない
			hComboRightFocus = NULL;
			ChangeReason = T("FocusFix6");
		}
	}

	// フォーカスに関連する情報を保存しなおし
	comboDocks.t.cinfo = comboDocks.b.cinfo = Combo.base[baseindex].cinfo;

	if ( Combo.base[baseindex].cinfo != NULL ){
		SetComboAddresBar(Combo.base[baseindex].cinfo->path);
		hComboFocus = hTargetWnd;
	}

	// フォーカスが変化した窓と、タブを再描画
	InvalidateRect(hWnd,NULL,TRUE); // pane,tab,info,status
	if ( Combo.Tabs ){
		int tabwndindex;

		tabwndindex = Combo.Tabs > 1 ? GetComboShowIndex(hTargetWnd) : 0;
		SelectTabByWindow(hTargetWnd,tabwndindex);
	}
	CheckComboTable(T("KCW_FocusFix1"));
}

LRESULT KCW_combonextppc(HWND hWnd,HWND hTargetWnd,short mode)
{
	int baseindex,si;

	if ( Combo.BaseCount < 2 ){
		PPCui(hWnd,NULL);
		return SENDCOMBO_OK;
	}
	if ( mode == 0 ){	// 反対窓へ
		baseindex = GetPairPaneComboBaseIndex(hTargetWnd);
		if ( baseindex < 0 ) return 0;
	}else{		// 基準指定有り
		baseindex = GetComboBaseIndex(hTargetWnd);
		if ( baseindex < 0 ) return 0;
		baseindex = baseindex + (short)mode;
		if ( baseindex < 0 ) baseindex = Combo.BaseCount - 1;
		if ( baseindex >= Combo.BaseCount ) baseindex = 0;
	}
	if ( Combo.Tabs ){
		for ( si = 0 ; si < Combo.ShowCount ; si++ ){ // 表示中タブにある？
			if ( Combo.show[si].baseNo == baseindex ){
				ChangeReason = T("KCW_combonextppc@1");
				SetFocus(Combo.base[Combo.show[si].baseNo].hWnd);
				return SENDCOMBO_OK;
			}
		}
		// 隠れているので、ペインのタブを切換
		if ( X_combos[0] & CMBS_TABEACHITEM ){ // 該当ペインを検索する
			int showindex;

			si = -1;
			for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){
				if ( GetTabItemIndex(Combo.base[baseindex].hWnd,showindex) >= 0 ){
					si = showindex;
					break;
				}
			}
		}else{ // カレントペイン
			si = GetComboShowIndex(hComboFocus);
		}

		if ( si < 0 ) si = 0;

		SendMessage(hWnd,WM_SETREDRAW,FALSE,0);
		ShowWindow(Combo.base[Combo.show[si].baseNo].hWnd,SW_HIDE);
		if ( Combo.ShowCount == 1 ){
			hComboRightFocus = Combo.base[Combo.show[si].baseNo].hWnd;
		}else{
			if ( Combo.base[Combo.show[si].baseNo].hWnd == hComboRightFocus ){
				hComboRightFocus = Combo.base[baseindex].hWnd;
			}
		}
		Combo.show[si].baseNo = baseindex;
		// 明後日の位置に表示させて、SortComboWindows による位置移動のときに子ウィンドウ(ツリー)を正しく描画させる
//		ShowWindow(Combo.base[baseindex].hWnd,SW_SHOWNORMAL);
		SetWindowPos(Combo.base[baseindex].hWnd,NULL,-10,-10,0,0,SWP_SHOWWINDOW);
		SendMessage(hWnd,WM_SETREDRAW,TRUE,0);
		SortComboWindows(SORTWIN_LAYOUTPAIN);
	}
	ChangeReason = T("KCW_combonextppc@2");
	SetFocus(Combo.base[baseindex].hWnd);
	return SENDCOMBO_OK;
}

void KCW_dockCommand(WORD id,LPARAM lParam)
{
	TCHAR param[CMDLINESIZE];
	POINT pos;
	PPXDOCK *dock;

	if ( id < 0x100 ){ // dock_menu
		pos = *(POINT *)lParam;
	}else{
		tstrcpy(param,(TCHAR *)lParam);
	}
	ReplyMessage(0);
	dock = ((id & 0xff) == 'B') ? &comboDocks.b : &comboDocks.t;
	if ( id < 0x100 ){ // dock_menu
		DockModifyMenu(Combo.hWnd,dock,&pos);
	}else{
		int showindex;
		PPC_APPINFO *cinfo = NULL;

		showindex = GetComboShowIndexDefault(hComboFocus);
		if ( showindex >= 0 ){
			cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
		}
		dock->cinfo = cinfo;
		if ( (id >> 8) == dock_drop ){
			if ( (cinfo != NULL) && !TinyCheckCellEdit(cinfo) ){
				DockDropBar(cinfo,dock,param);
			}
		}else{
			DockCommands(Combo.hWnd,dock,id >> 8,param);
		}
	}
	SortComboWindows(SORTWIN_LAYOUTALL);
}

void KCW_ReadyCombo(HWND hWnd)
{
	#ifndef RELEASE
	if ( AddDelCount ){
		ReplyMessage(0);
		XMessage(NULL,NULL,XM_DbgDIA,T("KCW_ReadyCombo - nested !"));
	}
	#endif

	if ( ComboPaneLayout != NULL ){ // ペインの並びを再現する
		const TCHAR *panes;
		COMBOPANES *OrgShowItem;

		OrgShowItem = PPcHeapAlloc(sizeof(COMBOPANES) * Combo.ShowCount);
		if ( OrgShowItem != NULL ){
			int i,j;

			memcpy(OrgShowItem,Combo.show,sizeof(COMBOPANES) * Combo.ShowCount);

			// 右窓を取得
			if ( ComboPaneLayout[1] != '?' ){
				for ( i = 0 ; i < Combo.BaseCount ; i++ ){
					PPC_APPINFO *cinfo;

					cinfo = Combo.base[i].cinfo;
					if ( cinfo != NULL ){
						if ( cinfo->info.RegID[2] == ComboPaneLayout[1] ){
							hComboRightFocus = cinfo->info.hWnd;
							ChangeReason = T("ReadyCombo");
							break;
						}
					}
				}
			}

			// 新しい並びを初期化
			for ( i = 0 ; i < Combo.ShowCount ; i++ ) Combo.show[i].baseNo = -1;
			panes = ComboPaneLayout + 2;
			while ( *panes ){
				const TCHAR *panesparam;

				while ( (*panes != '\0') && !Isalpha(*panes) ) panes++;

				panesparam = panes + 1;
				while ( (*panesparam != '\0') && !Isalnum(*panesparam) ){
					panesparam++;
				}
				if ( !Isdigit(*panesparam) ){ // 表示位置指定がなければ無視
					panes = panesparam;
				}else{
					int showindex = 0;
					TCHAR ID;

					ID = *panes;
					panes = panesparam;
					while ( Isdigit(*panes) ){
						showindex = showindex * 10 + (*panes - '0');
						panes++;
					}
					if ( showindex < Combo.ShowCount ){ // 該当IDを検索し、表示設定する
						for ( i = 0 ; i < Combo.BaseCount ; i++ ){
							PPC_APPINFO *cinfo;

							cinfo = Combo.base[i].cinfo;
							if ( cinfo != NULL ){
								if ( ID == cinfo->RegCID[1] ){
									Combo.show[showindex].baseNo = i;
									ShowWindow(cinfo->info.hWnd,SW_SHOWNA);

									if ( Combo.Tabs >= 2 ){
										SelectTabByWindow(cinfo->info.hWnd,showindex);
									}

									// 表示したので、元の並びから消去
									for ( j = 0 ; j < Combo.ShowCount ; j++ ){
										if ( OrgShowItem[j].baseNo == i ){
											OrgShowItem[j].baseNo = -1;
											break;
										}
									}
									break;
								}
							}
						}
					}
				}
			}

			// 割り当てなかったペインの調整
			for ( i = 0 ; i < Combo.ShowCount ; i++ ){
				if ( Combo.show[i].baseNo >= 0 ) continue;
				if ( Combo.Tabs > 1 ){
					HWND hTabWnd;
					TC_ITEM tie;
					int tabindex;
					int tabcount;

					hTabWnd = Combo.show[i].tab.hWnd;
					tabcount = TabCtrl_GetItemCount(hTabWnd);
					for ( tabindex = 0 ; tabindex < tabcount ; tabindex++ ){
						tie.mask = TCIF_PARAM;
						if ( TabCtrl_GetItem(hTabWnd,tabindex,&tie) != FALSE ){
							int bno = GetComboBaseIndex((HWND)tie.lParam);

							if ( bno >= 0 ){
								if ( GetComboShowIndex((HWND)tie.lParam) < 0 ){
									Combo.show[i].baseNo = bno;
									TabCtrl_SetCurSel(hTabWnd,tabindex);
									ShowWindow((HWND)tie.lParam,SW_SHOWNA);

									// 表示したので、元の並びから消去
									for ( j = 0 ; j < Combo.ShowCount ; j++ ){
										if ( OrgShowItem[j].baseNo == bno ){
											OrgShowItem[j].baseNo = -1;
											break;
										}
									}
									break;
								}
							}else{ // 不明タブを削除
								TabCtrl_DeleteItem(hTabWnd,tabindex);
							}
						}
					}
					if ( Combo.show[i].baseNo >= 0 ) continue;
				}

				for ( j = 0 ; j < Combo.ShowCount ; j++ ){
					if ( OrgShowItem[j].baseNo != -1 ){

						Combo.show[i].baseNo = OrgShowItem[j].baseNo;
						OrgShowItem[j].baseNo = -1;
						break;
					}
				}
			}

			for ( i = 0 ; i < Combo.ShowCount ; i++ ){
				if ( OrgShowItem[i].baseNo != -1 ){
					ShowWindow(Combo.base[OrgShowItem[i].baseNo].hWnd,SW_HIDE);
					break;
				}
			}

			// 欠番があったら詰める
			j = 0;
			for ( i = 0 ; i < Combo.ShowCount ; i++ ){
				if ( Combo.show[i].baseNo == -1 ) continue;
				Combo.show[j++].baseNo = Combo.show[i].baseNo;
			}
			Combo.ShowCount = j;
			// 窓枠大きさの再取得
			for ( i = 0 ; i < Combo.ShowCount ; i++ ){
				TCHAR buf[10];

				wsprintf(buf,T("%s%d"),ComboID,i + 1);
				GetCustTable(Str_WinPos,buf,&Combo.show[i].box,sizeof(RECT));
			}

			PPcHeapFree(OrgShowItem);
		}

		CheckRightFocus();
		SortComboWindows(SORTWIN_LAYOUTPAIN);

		PPcHeapFree(ComboPaneLayout);
		ComboPaneLayout = NULL;
	}
	if ( hWnd == NULL ) hWnd = hComboFocus;
							// hWnd が ComboWindow 内にない場合
	if ( GetComboBaseIndex(hWnd) < 0 ){
		hWnd = Combo.base[Combo.show[0].baseNo].hWnd;
	}

	if ( Combo.Tabs ){ // 非表示等の処理
		HWND hTmpRightWnd = hComboRightFocus;

		if ( GetComboShowIndex(hWnd) < 0 ){
			SelectHidePane(GetComboShowIndex(hComboFocus),hWnd);
		}

		KCW_FocusFix(Combo.hWnd,hWnd);
		if ( hComboRightFocus != hTmpRightWnd ){
			hComboRightFocus = hTmpRightWnd;
			InvalidateRect(Combo.hWnd,NULL,TRUE);
		}
	}

	{
		int i;

		for ( i = 0 ; i < Combo.BaseCount ; i++ ){
			if ( GetComboShowIndex(Combo.base[i].hWnd) < 0 ){
				ShowWindow(Combo.base[i].hWnd,SW_HIDE);
			}
		}
	}

	ChangeReason = T("KCW_ReadyCombo@1");
	SetFocus(hWnd);

	// ↓フォーカス設定に失敗したとき、強制設定
	if ( hWnd != hComboFocus ){
		KCW_FocusFix(hWnd,hWnd);
		InvalidateRect(Combo.hWnd,NULL,FALSE);
	}

	CheckRightFocus();
	CheckComboTable(T("KCW_ReadyCombo3"));
	ComboInit = 0;
}

void WmComboPosChanged(HWND hWnd)
{
	WINDOWPLACEMENT wp;

	wp.length = sizeof(wp);
	GetWindowPlacement(hWnd,&wp); //※wp.rcNormalPosition の座標はタスクバーの考慮無
	if ( wp.showCmd == SW_SHOWNORMAL ){
		PPC_APPINFO *cinfo;

		if ( Combo.ShowCount > 0 ){
			cinfo = Combo.base[Combo.show[0].baseNo].cinfo;
		}else{
			cinfo = NULL;
		}
		if ( (cinfo == NULL) || (cinfo->bg.X_WallpaperType < 10) ){
			GetWindowRect(hWnd,&ComboWinPos.pos);
		}else{ // 背景画像の再描画チェック
			GetWindowRect(hWnd,&wp.rcNormalPosition);
			if ( (wp.rcNormalPosition.left != ComboWinPos.pos.left) ||
				 (wp.rcNormalPosition.top != ComboWinPos.pos.top) ){
				int showindex;

				for ( showindex = 0 ; showindex < Combo.ShowCount ; showindex++ ){

					cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
					if ( (cinfo != NULL) && (cinfo->bg.X_WallpaperType >= 10) ){
						InvalidateRect(cinfo->info.hWnd,NULL,TRUE);
					}
				}
			}
			ComboWinPos.pos = wp.rcNormalPosition;
		}
	}else if ( (wp.showCmd == SW_SHOWMINIMIZED) && (X_tray & X_tray_Combo) ){
		// ※最小化のときと、次の隠したときと２回続けて呼ばれる
		PostMessage(hWnd,WM_PPXCOMMAND,K_HIDE,0);
		return;
	}else if ( wp.showCmd != SW_SHOWMAXIMIZED ){
		return; // SW_SHOWMINIMIZED / SW_HIDE ならここで終了
	}
	ClientPos.x = ClientPos.y = 0;
	ClientToScreen(hWnd,&ClientPos);
	GetClientRect(hWnd,&wp.rcNormalPosition);

	// 自動縦・横配置変更
	if ( (TouchMode & TOUCH_AUTOPAIRDIST) &&
		 (wp.showCmd == SW_SHOWMAXIMIZED) &&
		 (Combo.ShowCount >= 2) ){
		if ( wp.rcNormalPosition.right > wp.rcNormalPosition.bottom ){ // 横
			if ( X_combos[0] & CMBS_VPANE ){
				resetflag(X_combos[0],CMBS_VPANE);
				ComboSize.cx = -1;
			}
		}else{ // 縦
			if ( !(X_combos[0] & CMBS_VPANE) ){
				setflag(X_combos[0],CMBS_VPANE);
				InvalidateRect(hWnd,NULL,TRUE);
				ComboSize.cx = -1;
			}
		}
	}

	if ( (ComboSize.cx == wp.rcNormalPosition.right) &&
		 (ComboSize.cy == wp.rcNormalPosition.bottom) ){
		if ( ComboWinPos.show != (BYTE)wp.showCmd ){
			ComboWinPos.show = (BYTE)wp.showCmd;
			InvalidateRect(hWnd,NULL,TRUE); // ウィンドウ状態が変化したので全更新
		}
		return;
	}
	ComboWinPos.show = (BYTE)wp.showCmd;

	// サイズ変更されたので調整する
	ComboSize.cx = wp.rcNormalPosition.right;
	ComboSize.cy = wp.rcNormalPosition.bottom;

	if ( Combo.Panes.resizewidth == -2 ){
		Combo.Panes.resizewidth = -1;
		SortComboWindows(SORTWIN_LAYOUTALL);
	}else{
		SortComboWindows(SORTWIN_RESIZE);
	}
	InvalidateRect(hWnd,NULL,TRUE); // 大きさが変化したので全更新
}

LRESULT WmComboCommand(HWND hWnd,WPARAM wParam,LPARAM lParam)
{
	DEBUGLOGF("WmComboCommand %4x",wParam);

	switch ( LOWORD(wParam) ){
		case K_Lcust: // lParam は 0 / hWnd(子PPcからの通知) / Tick(PPcust)
			if ( (lParam == 0) ||
				((HWND)lParam == Combo.base[Combo.show[0].baseNo].hWnd) ){
				HWND hNowWnd = hComboFocus;

				ComboCust();
				LoadWallpaper(NULL,hWnd,ComboID);

				hComboFocus = NULL;
				KCW_FocusFix(hWnd,hNowWnd);

				SortComboWindows(SORTWIN_LAYOUTALL);
			}
			break;

		// Combo
		case KCW_capture:
		case KCW_entry:
			return KCW_EntryCombo((HWND)lParam,(DWORD)wParam);

		case KCW_ready:
			KCW_ReadyCombo((HWND)lParam);
			break;

		case KCW_focus:
			DEBUGLOGF("WmComboCommand ↑ KCW_focus",0);
			KCW_FocusFix(hWnd,(HWND)lParam);
			DEBUGLOGF("WmComboCommand KCW_focus end",0);
			break;

		case KCW_size: {
			int showindex;
			RECT box,*savedbox;

			showindex = GetComboShowIndex((HWND)lParam);
			if ( showindex < 0 ) break;
			DEBUGLOGF("WmComboCommand ↑ KCW_size",0);
			GetWindowRect((HWND)lParam,&box);
			box.left -= ClientPos.x;
			box.right -= ClientPos.x;
			box.top -= ClientPos.y;
			box.bottom -= ClientPos.y;
			savedbox = &Combo.show[showindex].box;
			if ( EqualRect(savedbox,&box) == FALSE ){
				DEBUGLOGF("WmComboCommand KCW_size - SortComboWindows",0);
				*savedbox = box;
				if ( ComboInit == 0 ) SortComboWindows(showindex);
			}
			DEBUGLOGF("WmComboCommand KCW_size end",0);
			break;
		}

		case KCW_reqsize:
			RequestPairRate((SCW_REQSIZE *)lParam);
			break;

		case KCW_setpath:
			SetTabInfo(-1,(HWND)lParam);
			break;

		case KCW_setforeground:
			SetFocus((HWND)lParam);
			if ( IsWindowVisible(hWnd) == FALSE ){
				ForceSetForegroundWindow(hWnd);
				if ( IsWindowVisible(hWnd) == FALSE ) ShowWindow(hWnd,SW_SHOW);
			}else{
				ShowWindow(hWnd,SW_SHOW);
				ForceSetForegroundWindow(hWnd);
			}
			PPxCommonCommand(hWnd,0,K_WTOP);
			break;

		case KCW_drawinfo:
			if ( InfoHeight ){
				RECT box;

				box.left	= 0;
				box.top		= InfoTop;
				box.right	= ComboSize.cx;
				box.bottom	= InfoBottom;
				InvalidateRect(hWnd,&box,FALSE);
			}
			DocksInfoRepaint(&comboDocks);
			break;

		case KCW_drawstatus:
			DocksStatusRepaint(&comboDocks);
			break;

		case KCW_nextppc:
			return KCW_combonextppc(hWnd,(HWND)lParam,(short)HIWORD(wParam));

		case KCW_getpath: {
			int baseindex;
			PPC_APPINFO *cinfo;

			if ( Combo.BaseCount < 2 ) return 0;
			baseindex = GetPairPaneComboBaseIndex(*(HWND *)lParam);
			if ( baseindex < 0 ) return 0;
			cinfo = Combo.base[baseindex].cinfo;
			if ( cinfo == NULL ) return 0;
			if ( cinfo->e.Dtype.mode != VFSDT_LFILE ){
				tstrcpy((TCHAR *)lParam,cinfo->RealPath);
			}else if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ){
				tstrcpy((TCHAR *)lParam,cinfo->path);
			}else{
				VFSFullPath((TCHAR *)lParam,cinfo->ArcPathMask,cinfo->path);
			}
			return SENDCOMBO_OK;
		}

		case KCW_getpairwnd: {
			int baseindex;

			if ( Combo.BaseCount < 2 ) return (LRESULT)NULL;
			baseindex = GetPairPaneComboBaseIndex((HWND)lParam);
			if ( baseindex < 0 ) return (LRESULT)NULL;
			return (LRESULT)Combo.base[baseindex].hWnd;
		}

		case KCW_swapwnd:
			ComboSwap();
			break;

		case KCW_tree:
			KCW_treeCombo(HIWORD(wParam),(TCHAR *)lParam);
			break;

		case KCW_tabrotate:
			if ( Combo.Tabs ){
				int tabwndindex;

				tabwndindex = GetComboBaseIndex((HWND)lParam);
				if ( tabwndindex < 0 ){
					tabwndindex = GetComboBaseIndex(hComboFocus);
				}
				PaneCommand(HIWORD(wParam) < 0x7fff ?
						T("change h-1") : T("change h+1"),tabwndindex);
			}
			break;

		case KCW_panecommand:
			return (LRESULT)PaneCommand((const TCHAR *)lParam,(int)(short)HIWORD(wParam));

		case KCW_eject:
			EjectPane(lParam);
			break;

		// Tree
		case KTN_close:
			KCW_treeCombo(PPCTREE_SYNC,NilStr);
			break;

		case KTN_escape:
			// KTN_focus へ
		case KTN_focus:
			ChangeReason = T("WmComboSetFocus@KTN_focus");
			WmComboSetFocus();
			break;

		case KTN_selected:
			SendMessage(hComboFocus,WM_PPXCOMMAND,KTN_select,lParam);
			if ( ComboTreeMode == PPCTREE_SELECT ){
				CloseLeftArea();
			}
			ChangeReason = T("KTN_selected@1");
			SetFocus(hComboFocus);
			break;

		case KTN_size:
			if ( ((int)lParam + splitwide) != Combo.LeftAreaWidth ){
				Combo.LeftAreaWidth = (int)lParam + splitwide;
				SortComboWindows(SORTWIN_LAYOUTALL);
			}
			break;

		// 中継
		case KTN_select:
		case KCW_enteraddress:
		case K_SETPOPLINENOLOG: {
			int baseindex;

			baseindex = GetComboBaseIndex(hComboFocus);
			if ( baseindex < 0 ) break;
			if ( Combo.base[baseindex].cinfo != NULL ){
				SendMessage(hComboFocus,WM_PPXCOMMAND,wParam,lParam);
			}
			break;
		}

		case K_a | '-': {
			int baseindex;

			baseindex = GetComboBaseIndex((HWND)lParam);
			if ( baseindex >= 0 ){
				ReplyMessage(ERROR_CANCELLED);
				TabMenu(NULL,baseindex,-1,NULL);
			}
			break;
		}

		case K_WINDDOWLOG:
			if ( lParam != 0 ) SetComboReportText((const TCHAR *)lParam);
			if ( HIWORD(wParam) && (Combo.Report.hWnd != NULL) &&
				!(X_combos[0] & CMBS_DELAYLOGSHOW) ){
				if ( HIWORD(wParam) == 2 ){ // 強制表示を行う
					DelayLogShowProc(Combo.Report.hWnd,
							WM_TIMER,TIMERID_DELAYLOGSHOW,0);
					break;
				}
				if ( GetWindowLongPtr(Combo.Report.hWnd,GWL_STYLE) &
						WS_VISIBLE ){
					// ログの遅延表示を開始／画面描画を止める
					SendMessage(Combo.Report.hWnd,WM_SETREDRAW,FALSE,0);
					SetTimer(Combo.Report.hWnd,TIMERID_DELAYLOGSHOW,
							TIMER_DELAYLOGSHOW,DelayLogShowProc);
				}
			}
			break;

		case KCW_addressbar:
			if ( FocusAddressBars(Combo.hAddressWnd,&comboDocks) ){
				return SENDCOMBO_OK;
			}
			return 0;

		case KCW_dock:
			KCW_dockCommand(HIWORD(wParam),lParam);
			break;

		case KCW_setmenu:
			SetMenu(Combo.hWnd,
					!(lParam & CMBS_NOMENU) ? ComboDMenu.hMenuBarMenu : NULL);
			break;

		case KCW_getsite: {
			int showindex;

			showindex = GetComboShowIndex((HWND)lParam);
			if ( showindex < 0 ) return 0;
			return showindex ? 2 : 1;
		}

		case KCW_layout:
			SortComboWindows(SORTWIN_LAYOUTALL);
			InvalidateRect(hWnd,NULL,TRUE);
			break;

		case KCW_pathfocus: {
			int baseindex;

			for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
				PPC_APPINFO *cinfo;

				cinfo = Combo.base[baseindex].cinfo;
				if ( (cinfo != NULL) &&
					 (!tstricmp((TCHAR *)lParam,cinfo->path)) ){
					SetFocus(Combo.base[baseindex].hWnd);
					return SENDCOMBO_OK + 1;
				}
			}
			break;
		}

		case KCW_closetabs: {
			int first = (short)LOWORD(lParam);
			int last = (short)HIWORD(lParam);
			int pane = HIWORD(wParam);
			HWND hTabWnd;

			if ( pane >= Combo.ShowCount ) break;
			hTabWnd = Combo.show[pane].tab.hWnd;

			for ( ; last >= first ; last-- ){
				TC_ITEM tie;

				if ( Combo.hWnd == BADHWND ) break;
				tie.mask = TCIF_PARAM;
				if ( TabCtrl_GetItem(hTabWnd,last,&tie) == FALSE ) break;
				PostMessage((HWND)tie.lParam,WM_CLOSE,0,0);
			}
			break;
		}

		case KCW_showjoblist: {
			DWORD X_jlst[2];

			GetCustData(T("X_jlst"),&X_jlst,sizeof(X_jlst));
			if ( Combo.Joblist.hWnd == NULL ){
				CreateJobArea();
				X_jlst[0] = 3;
			}else{
				PostMessage(Combo.Joblist.hWnd,WM_CLOSE,0,0);
				Combo.Joblist.hWnd = NULL;
				if ( Combo.Report.hWnd == NULL ){
					if ( Combo.BottomAreaHeight ){
						Combo.BottomAreaHeight = 0;
					}else if ( Combo.hTreeWnd == NULL ){
						Combo.LeftAreaWidth = 0;
					}
				}
				X_jlst[0] = 0;
			}
			SetCustData(T("X_jlst"),&X_jlst,sizeof(X_jlst));
			SortComboWindows(SORTWIN_LAYOUTALL);
			break;
		}

		case KCW_ActivateWnd:
		case KCW_SelectWnd:
			SelectComboWindow(HIWORD(wParam),(HWND)lParam,
					(LOWORD(wParam) == KCW_ActivateWnd));
			break;

		case KC_GETSITEHWND:
			if ( (int)lParam >= KC_GETSITEHWND_LEFTENUM ){
				int showindex = (int)lParam - KC_GETSITEHWND_LEFTENUM;
				if ( showindex >= Combo.ShowCount ) return (LRESULT)NULL;
				return (LRESULT)Combo.base[Combo.show[showindex].baseNo].hWnd;
			}
			return (LRESULT)NULL;

		case K_E_TABLET:
			WmComboDpiChanged(hWnd,0,NULL);
			if ( Combo.hTreeWnd != NULL ){
				SendMessage(Combo.hTreeWnd,VTM_CHANGEDDISPDPI,TMAKEWPARAM(0,Combo.FontDPI),0);
			}
			break;

		case KCW_ppclist:
			return ComboGetPPcList();

		default:
			PPxCommonCommand(hWnd,0,LOWORD(wParam));
			break;
	}
	return SENDCOMBO_OK;
}

void DrawPaneArea(DRAWCOMBOSTRUCT *dcs)
{
	if ( Combo.ShowCount == 0 ){ // ペイン無しの時の空間表示
		if ( dcs->hBr == NULL ) dcs->hBr = CreateSolidBrush(C_back);
		FillBox(dcs->ps.hdc,&Combo.Panes.box,dcs->hBr);
		return;
	}
	// ペイン間の仕切り線描画
	if ( !(X_combos[0] & CMBS_VPANE) ){	// 横整列
		int showindex,showmax;

		showmax = (X_combos[0] & CMBS_TABFRAME) ? Combo.ShowCount : Combo.ShowCount - 1;
		for ( showindex = 0 ; showindex < showmax ; showindex++ ){
			RECT drawbox,*box;

			box = &Combo.show[showindex].box;
			drawbox.left = box->right;
			drawbox.right = drawbox.left + splitwide;
			drawbox.top = box->top;
			drawbox.bottom = box->bottom;
			DrawEdge(dcs->ps.hdc,&drawbox,EDGE_RAISED,BF_LEFT | BF_RIGHT | BF_MIDDLE);
		}
	}else{						// 縦整列
		int showindex;

		for ( showindex = 0 ; showindex < Combo.ShowCount - 1 ; showindex++ ){
			RECT drawbox,*box;

			box = &Combo.show[showindex].box;
			drawbox.left = box->left;
			drawbox.right = box->right;
			drawbox.top = box->bottom;
			drawbox.bottom = drawbox.top + splitwide;
			DrawEdge(dcs->ps.hdc,&drawbox,EDGE_RAISED,BF_TOP | BF_BOTTOM | BF_MIDDLE);
		}
	}
}

void WmComboPaint(HWND hWnd)
{
	HGDIOBJ hOldFont;
	RECT box;
	PPC_APPINFO *cinfo = NULL;
	DRAWCOMBOSTRUCT dcs;
	RECT drawbox;

	BeginPaint(hWnd,&dcs.ps);
	dcs.hBr = NULL;

	// 標準アドレスバーボタン描画
	if ( X_combos[0] & CMBS_COMMONADDR ){
		box.right = ComboSize.cx;
		box.left = box.right - AddrHeight;
		box.top = AddrTop;
		box.bottom = box.top + AddrHeight;
		DrawAddressButton(dcs.ps.hdc,&box);
	}

	// 情報行描画
	if ( InfoHeight && (dcs.ps.rcPaint.top < InfoBottom) ){
		int showindex;

		box.left = 0;
		box.top = InfoTop;
		box.right = dcs.ps.rcPaint.right;
		box.bottom = InfoBottom;

		showindex = GetComboShowIndexDefault(hComboFocus);
		if ( showindex >= 0 ){
			cinfo = Combo.base[Combo.show[showindex].baseNo].cinfo;
			if ( (cinfo != NULL) && !TinyCheckCellEdit(cinfo) ){
#ifndef USEDELAYCURSOR
				if ( cinfo->bg.X_WallpaperType )
#endif
				{
					dcs.hBr = CreateSolidBrush(cinfo->BackColor);
					FillBox(dcs.ps.hdc,&box,dcs.hBr);
				}
				hOldFont = SelectObject(dcs.ps.hdc, Combo.Font.handle);
				SetTextAlign(dcs.ps.hdc,TA_LEFT | TA_TOP | TA_UPDATECP);
				PaintInfoLine(cinfo, &dcs.ps, &box, DISPENTRY_NO_OUTPANE);
				SetTextAlign(dcs.ps.hdc,TA_LEFT | TA_TOP | TA_NOUPDATECP);
				SelectObject(dcs.ps.hdc,hOldFont);
			}
		}
		if ( (showindex < 0) || (cinfo == NULL) ){
			if ( dcs.hBr == NULL ) dcs.hBr = CreateSolidBrush(C_back);
			FillBox(dcs.ps.hdc,&box,dcs.hBr);
		}
	}
	if ( Combo.LeftAreaWidth ){ // ツリーとペインとの間の仕切り線
		drawbox.left = Combo.LeftAreaWidth - splitwide;
		drawbox.right = Combo.LeftAreaWidth;
		drawbox.top = Combo.TopAreaHeight;
		drawbox.bottom = Combo.Report.box.y - splitwide;
		DrawEdge(dcs.ps.hdc,&drawbox,EDGE_RAISED,BF_LEFT | BF_RIGHT | BF_MIDDLE);
	}
	if ( ((Combo.Report.hWnd != NULL) || (Combo.Joblist.hWnd != NULL)) &&
		 (dcs.ps.rcPaint.bottom > (Combo.Report.box.y - splitwide)) ){ // ペインより下側の仕切り線
		drawbox.top = Combo.Report.box.y - splitwide;
		drawbox.bottom = Combo.Report.box.y;
		drawbox.left = dcs.ps.rcPaint.left;
		drawbox.right = dcs.ps.rcPaint.right;
		DrawEdge(dcs.ps.hdc,&drawbox,EDGE_RAISED,BF_TOP | BF_BOTTOM | BF_MIDDLE);
		if ( Combo.Joblist.hWnd != NULL ){ // ログ・ジョブリスト間の仕切り線
			drawbox.left = Combo.Report.box.x + Combo.Report.box.width;
			drawbox.right = drawbox.left + splitwide;
			drawbox.top = drawbox.bottom;
			drawbox.bottom = drawbox.top + Combo.Report.box.height;
			DrawEdge(dcs.ps.hdc,&drawbox,EDGE_RAISED,BF_LEFT | BF_RIGHT | BF_MIDDLE);
		}
	}

	if ( !(X_combos[0] & CMBS_TABFRAME) ) DrawPaneArea(&dcs);
	if ( dcs.hBr != NULL ) DeleteObject(dcs.hBr);
	EndPaint(hWnd,&dcs.ps);
}

void DrawTab(DRAWITEMSTRUCT *dis)
{
	TC_ITEM tie;
	TCHAR buf[CMDLINESIZE];
	int type,baseindex;
	RECT box;
	COLORREF oldc = C_AUTO,oldbc = C_AUTO;

	tie.mask = TCIF_TEXT | TCIF_PARAM;
	tie.pszText = buf;
	tie.cchTextMax = CMDLINESIZE;
	if ( TabCtrl_GetItem(dis->hwndItem,dis->itemID,&tie) == FALSE ) return;

	if ( (HWND)tie.lParam == Combo.base[Combo.show[0].baseNo].hWnd ){ // 左
		type = ( (HWND)tie.lParam == hComboFocus ) ? 1 : 3;
	}else if ( (HWND)tie.lParam == hComboRightFocus ){ // 右
		type = ( (HWND)tie.lParam == hComboFocus ) ? 1 : 3;
	}else if ( (Combo.ShowCount >= 3) &&
		(GetComboShowIndex((HWND)tie.lParam) >= 0) ){ // 表示窓
		type = 5;
	}else{ // それ以外
		type = -1;
	}

	baseindex = GetComboBaseIndex((HWND)tie.lParam);

	// 文字色
	if ( (baseindex >= 0) && (Combo.base[baseindex].tabtextcolor != C_AUTO) ){
		oldc = SetTextColor(dis->hDC,Combo.base[baseindex].tabtextcolor);
	}

	// 背景色
	if ( (baseindex >= 0) && (Combo.base[baseindex].tabbackcolor != C_AUTO) ){
		HBRUSH hBack;

		hBack = CreateSolidBrush(Combo.base[baseindex].tabbackcolor);
		FillBox(dis->hDC,&dis->rcItem,hBack);
		DeleteObject(hBack);
		oldbc = SetBkColor(dis->hDC,Combo.base[baseindex].tabbackcolor);
	}else if ( dis->itemState & ODS_SELECTED ){
		HBRUSH hBack;
		COLORREF col;

		if ( (HWND)tie.lParam == hComboFocus ){
			col = GetSysColor(COLOR_3DHIGHLIGHT);
		}else{
			col = GetSysColor(COLOR_3DLIGHT);
		}
		hBack = CreateSolidBrush(col);
		FillBox(dis->hDC,&dis->rcItem,hBack);
		DeleteObject(hBack);
		oldbc = SetBkColor(dis->hDC,col);
	}

	if ( type >= 0 ){ // フォーカスの印
		HBRUSH hBack;

		box = dis->rcItem;
		box.bottom = box.top + 4;

		hBack = CreateSolidBrush(C_capt[type]);
		FillBox(dis->hDC,&box,hBack);
		DeleteObject(hBack);
	}

	box = dis->rcItem; // TCM_GETITEMRECT で得られるRECTより左右上下2pixずつ小さい
	box.left += 6;
	box.top += 4;
	#pragma warning(suppress: 6054) // TabCtrl_GetItem で取得
	DrawText(dis->hDC,buf, tstrlen32(buf),&box,DT_LEFT | DT_NOPREFIX);

	//DT_END_ELLIPSIS DT_PATH_ELLIPSIS
	if ( oldc != C_AUTO ) SetTextColor(dis->hDC,oldc);
	if ( oldbc != C_AUTO ) SetBkColor(dis->hDC,oldbc);

	if ( !(X_combos[1] & CMBS1_NOTABBUTTON) ){ // 閉じるボタン
		HBRUSH hBack;

		box.left = box.right - (box.bottom - box.top);
		hBack = CreateSolidBrush( (C_TabCloseCBack == C_AUTO) ? GetBkColor(dis->hDC) : C_TabCloseCBack);
		FillBox(dis->hDC,&box,hBack);
		if ( C_TabCloseCText != C_AUTO ){
			oldc = SetTextColor(dis->hDC,C_TabCloseCText);
		}
		SetBkMode(dis->hDC,TRANSPARENT);
		DrawText(dis->hDC,&CloseButtonChar, 1,&box,DT_CENTER | DT_NOPREFIX);
		SetBkMode(dis->hDC,OPAQUE);
		if ( C_TabCloseCText != C_AUTO ) SetTextColor(dis->hDC,oldc);
		DeleteObject(hBack);
	}
}

LRESULT CALLBACK ComboProcMain(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch (message){
		case WM_NCCREATE:
			if ( (X_dss & DSS_COMCTRL) && (OSver.dwMajorVersion >= 10) ){
				PPxCommonCommand(hWnd,0,K_ENABLE_NC_SCALE);
			}
			return 1;

		case WM_PARENTNOTIFY:
			if ( Combo.hWnd == BADHWND ) break; // 終了動作中
			if ( LOWORD(wParam) == WM_DESTROY ){
				int baseindex = GetComboBaseIndex((HWND)lParam);

				if ( baseindex < 0 ) break;
				if ( AddDelCount ){
					PPC_APPINFO *cinfo;

					XMessage(NULL,NULL,XM_DbgLOG,T("combo destroy - nested %x"),(HWND)lParam);
					cinfo = Combo.base[baseindex].cinfo;
					if ( cinfo != NULL ){
						if ( comboDocks.t.cinfo == cinfo ){
							comboDocks.t.cinfo = NULL;
							comboDocks.b.cinfo = NULL;
						}
						Combo.base[baseindex].cinfo = NULL;
					}

					PostMessage(Combo.hWnd,WM_PPCOMBO_NESTED_DESTROY,wParam,lParam);
					break;
				}

				AddDelCount++;
				DEBUGLOGF("WM_PARENTNOTIFY - WM_DESTROY",0);
				DestroyedPaneWindow(hWnd,(HWND)lParam);
				DEBUGLOGF("WM_PARENTNOTIFY - WM_DESTROY end",0);
				AddDelCount--;
			}
			break;

		case WM_NCLBUTTONUP:
		case WM_NCRBUTTONUP:
		case WM_NCMBUTTONUP:
		case WM_NCXBUTTONUP:

		case WM_NCLBUTTONDBLCLK:
		case WM_NCRBUTTONDBLCLK:
		case WM_NCMBUTTONDBLCLK:
		case WM_NCXBUTTONDBLCLK:
			return ComboNCMouseCommand(hWnd,message,wParam,lParam);

		case WM_NCRBUTTONDOWN:
			if ( wParam == HTCAPTION ) ComboDMenu.Sysmenu = TRUE;
			return DefWindowProc(hWnd,message,wParam,lParam);

		case WM_LBUTTONDOWN:	return ComboLMouseDown(hWnd,lParam);
		case WM_LBUTTONUP:		return ComboLMouseUp(lParam);
		case WM_LBUTTONDBLCLK:	return ComboLMouseDbl(hWnd,lParam);

		case WM_RBUTTONUP:		return ComboMouseButton(lParam,'R');
		case WM_RBUTTONDBLCLK:	return ComboMouseButton(lParam,'R'+('D'<<8));
		case WM_MBUTTONUP:		return ComboMouseButton(lParam,'M');
		case WM_MBUTTONDBLCLK:	return ComboMouseButton(lParam,'M'+('D'<<8));
		case WM_XBUTTONUP:		return ComboMouseButton(lParam,'X');
		case WM_XBUTTONDBLCLK:	return ComboMouseButton(lParam,'X'+('D'<<8));

		case WM_CAPTURECHANGED:
			MoveSplit = DMS_NONE;
			break;

		case WM_MOUSEMOVE:		return ComboMouseMove(hWnd,lParam);

		case WM_SETFOCUS:
			WmComboSetFocus();
			break;
		case WM_PAINT:				WmComboPaint(hWnd);			break;
		case WM_WINDOWPOSCHANGED:	WmComboPosChanged(hWnd);	break;
		case WM_NOTIFY:				WmComboNotify((NMHDR *)lParam); break;

		case WM_COPYDATA:	return WmComboCopyData((COPYDATASTRUCT *)lParam);

		case WM_DESTROY:			WmComboDestroy(hWnd,FALSE);	break;

		case WM_ENDSESSION:
			if ( wParam ){	// TRUE:セッションの終了(WM_DESTROY は通知されない)
				WmComboDestroy(hWnd,TRUE);
				break;
			}
			break;

		case WM_SYSKEYDOWN:	// フォーカスが指定できなかったときの対策
			if ( lParam & B29 ){
				return DefWindowProc(hWnd,message,wParam,lParam);
			}
			WmComboSetFocus();
			break;

		case WM_SYSCOMMAND:
			if ( wParam >= 0xf000 ){
				WORD cmdID;

				cmdID = (WORD)(wParam & 0xfff0);
				if ( (cmdID == SC_KEYMENU) || (cmdID == SC_MOUSEMENU) ){
					if ( cmdID == SC_KEYMENU ){
						ReplyMessage(0); // デッドロック防止
					}
					ComboDMenu.Sysmenu = TRUE;
				}
				return DefWindowProc(hWnd,message,wParam,lParam);
			}
			// WM_COMMAND へ
		case WM_COMMAND:
			if ( lParam != 0 ){
				if ( (HWND)lParam == Combo.hAddressWnd ){
					if ( HIWORD(wParam) == 13 ) EnterAddressBar(); // Enter
					if ( HIWORD(wParam) == 27 ) SetFocus(hComboFocus); // ESC
					break;
				}
				if ( (HWND)lParam == Combo.ToolBar.hWnd ){
					ComboToolbarCommand(LOWORD(wParam),0);
					break;
				}
				if ( (HWND)lParam == Combo.Report.hWnd ){
					if ( HIWORD(wParam) == 27 ) SetFocus(hComboFocus); // ESC
					break;
				}
				if ( DocksWmCommand(&comboDocks,wParam,lParam) ){
					break;
				}
			}
			if ( HIWORD(wParam) == 0 ){ // メニュー
				return PostMessage(hComboFocus,WM_COMMAND,wParam,lParam);
			}
			break;

		case WM_DEVICECHANGE:
			if ( ( ((UINT)wParam == DBT_DEVICEARRIVAL) ||
				   ((UINT)wParam == DBT_DEVICEREMOVECOMPLETE) ) &&
				 (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype
						== DBT_DEVTYP_VOLUME) ){
				int baseindex;

				if ( Combo.hTreeWnd != NULL ){
					SendMessage(Combo.hTreeWnd,WM_DEVICECHANGE,wParam,lParam);
				}
				DocksWmDevicechange(&comboDocks,wParam,lParam);

				for ( baseindex = 0 ; baseindex < Combo.BaseCount ; baseindex++ ){
					SendMessage(Combo.base[baseindex].hWnd,WM_DEVICECHANGE,wParam,lParam);
				}
			}
			return DefWindowProc(hWnd,message,wParam,lParam);

		case WM_CTLCOLOREDIT:
			if ( Reportcolor ){
				if ( ((HWND)lParam == Combo.Report.hWnd) ||
					 ((HWND)lParam == Combo.hAddressWnd) ||
					 ((HWND)lParam == comboDocks.t.hAddrWnd) ||
					 ((HWND)lParam == comboDocks.b.hAddrWnd) ){
					SetTextColor((HDC)wParam,CC_log[0]);
					SetBkColor((HDC)wParam,CC_log[1]);
					return (LRESULT)Combo.Report.hBrush;
				}
			}
			return DefWindowProc(hWnd,message,wParam,lParam);

		case WM_DRAWITEM:
			if ( wParam == IDW_TABCONTROL) DrawTab((DRAWITEMSTRUCT *)lParam);
			return 1;

		case WM_INITMENU:
			DynamicMenu_InitMenu(&ComboDMenu,(HMENU)wParam,
					!(X_combos[0] & CMBS_NOMENU));
			break;

		case WM_INITMENUPOPUP: {
			int baseindex;

			baseindex = GetComboBaseIndex(hComboFocus);
			if ( baseindex >= 0 ){
				PPC_APPINFO *cinfo = Combo.base[baseindex].cinfo;

				if ( cinfo != NULL ){
					DynamicMenu_InitPopupMenu(&ComboDMenu,(HMENU)wParam,&cinfo->info);
				}
			}
			break;
		}

		// ● ComboProcNested > 1 のときでも来ることがあるので抜本対策が必要
		case WM_PPCOMBO_NESTED_ENTRY: // KCW_entry で再入になってしまった窓を再登録させる
			XMessage(NULL,NULL,XM_DbgLOG,T("COMBO_NESTED_ENTRY %x %d"),lParam,AddDelCount);
			if ( AddDelCount < 0 ) AddDelCount = 0;
			return KCW_EntryCombo((HWND)lParam,(DWORD)wParam);


		case WM_PPCOMBO_NESTED_DESTROY: // WM_PARENTNOTIFY で再入になってしまった窓を再登録解除させる
			// ●…既に存在しない窓なので特別な処理が必要
			if ( AddDelCount ){
				XMessage(NULL,NULL,XM_DbgLOG,T("COMBO_NESTED_DESTROY - dbl nested %d"),AddDelCount);
				if ( AddDelCount < 0 ) AddDelCount = 0;
			}

			if ( GetComboBaseIndex((HWND)lParam) >= 0 ){
				AddDelCount++;
				DestroyedPaneWindow(hWnd,(HWND)lParam);
				AddDelCount--;
			}
			break;

		case WM_PPCOMBO_PRECLOSE:
			ComboProcMain(hWnd,WM_PARENTNOTIFY,WM_DESTROY,lParam);
			break;

		case WM_DPICHANGED:
			WmComboDpiChanged(hWnd,wParam,(RECT *)lParam);
			break;
/*
		case WM_SETTINGCHANGE:
			PostMessage(hWnd,lParam,K_SETTINGCHANGE);
			return DefWindowProc(hWnd,message,wParam,lParam);
*/
		default:
			if ( message == WM_PPXCOMMAND ){
				return WmComboCommand(hWnd,wParam,lParam);
			}else if ( message == WM_TaskbarButtonCreated ){
				PPxCommonExtCommand(K_TBB_INIT,0);
			}
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return 0;
}

// 再入状況を確認するためのデバッグコード
int ComboProcNested = 0;
UINT ComboProcMsg[NestedMsgs];

LRESULT CALLBACK ComboProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	LRESULT result;

	if ( ComboProcNested < NestedMsgs ){
		ComboProcMsg[ComboProcNested] = message + (wParam << 16);
	}
	ComboProcNested++;
	result = ComboProcMain(hWnd,message,wParam,lParam);
	ComboProcNested--;
	return result;
}
