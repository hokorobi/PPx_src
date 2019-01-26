/*-----------------------------------------------------------------------------
	Paper Plane cUI											Combo Window 定義
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#define Combo_Max_Base 200 // 一体化窓に登録可能な最大数(最大は TCM_FIRST (0x1300) )
#define Combo_Max_Show 32 // 一体化窓の最大ペイン数(最大は Combo_Max_Base)

#define ComboFrameID 0x4000
#define ComboCommandIDFirst 0x4001 // コントロールIDの割当て先頭

#define PPCOMBOWinClass T("PaperPlaneCombo")
#define PPCOMBOWinFrameClass T("PPxComboFrame")

#define AREAMINSIZE 4
#define AREAMINSAVESIZE 10
#define BOTTOM_AREAMINSIZE 100

#define MTASKLEN 10 // _Pathに保存する情報量 [ID:xxxx][lock:$][num:0000][sep:-]
// ID: Aabc
#define Panelistsize(count) ((count + 2) * MTASKLEN + 16)

/*
	BaseXXX	COMBOITEMSTRUCT に記録されているindex
	ShowXXX	表示位置
*/
typedef struct {
	HWND hWnd;
	PPC_APPINFO *cinfo;
	#define CAPTURE_NONE	0
	#define CAPTURE_WINDOW	1 // 一般window
	#define CAPTURE_WINDOWEX	2 // PPx対応のcaptured window
	int capture;
	DWORD ActiveID; // アクティブ順を表す数値
	COLORREF tabtextcolor,tabbackcolor;
} COMBOITEMSTRUCT;

typedef struct {
	HWND hWnd; // tab window
	HWND hTipWnd; // tab text tip window
	int height;
} COMBOTABINFO;

typedef struct {
	RECT box;	// 表示位置
	int baseNo;	// 表示中のitem
	int resizepos; // リサイズ時に使用するサイズ比率
	COMBOTABINFO tab;
} COMBOPANES;

typedef struct {
	HWND hWnd;	// Combo 親ウィンドウ(INVALID_HANDLE_VALUE:終了中)
	HWND hAddressWnd;
	HWND hTreeWnd;
	int BaseCount,ShowCount;
	int Tabs;

	COMBOITEMSTRUCT *base;
	COMBOPANES *show;

	struct {
		HFONT handle;
//		SIZE size;
		struct {
			int cy;
		} size;
	} Font;

	struct {	// ActiveID の最小値、最大値
	// over flow は 0.1sにつき2カウントで1200日なので、考慮しない
		int low,high;
	} Active;

	struct {
		HWND hWnd;
		int Height;
	} ToolBar;

	int TopAreaHeight; //上部ドック、ツールバー、アドレスバーを含めた領域の下端
	int LeftAreaWidth; //ツリーの右端
	int BottomAreaHeight; //レポート・ジョブの上端
	DWORD FontDPI;
	ControlFontStruct cfs; // タブ用フォント(InitComboでNULL化済み)

	struct tagPanes {
		HWND hWnd; // カラム表示用ウィンドウ
		RECT box,clientbox; // 表示領域
		POINT delta; // 表示開始位置
		int resizewidth; // リサイズ時に使用するサイズ比率
	} Panes;

	struct {
		HWND hWnd;
		int JobAreaWidth;
	} Joblist;

	struct {
		HWND hWnd;
		HBRUSH hBrush;
		AREA box;
	} Report;
} COMBOSTRUCT;

extern COMBOSTRUCT Combo;

#define InfoTop (Combo.ToolBar.Height + TopDockHeight)
#define InfoBottom (InfoHeight + Combo.ToolBar.Height+ TopDockHeight)
#define AddrTop (Combo.TopAreaHeight - AddrHeight)
#define AddrBottom Combo.TopAreaHeight

//=============================================================================
#ifndef CGLOBALDEFINE
	#define GVAR extern
	#define GPARAM(x)
#else
	#undef GVAR
	#undef GPARAM
	#define GVAR
	#define GPARAM(x) = x
#endif

GVAR WINPOS ComboWinPos
#ifdef CGLOBALDEFINE
 = {{CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT},SW_SHOWDEFAULT,0}
#endif
;
GVAR SIZE ComboSize;				// Combo window の大きさ
GVAR PPXDOCKS comboDocks;
GVAR int splitwide GPARAM(4);		// ウィンドウの仕切の幅

GVAR struct {		// 最大,起動時表示ペイン数
	int max,min;
} X_mpane
#ifdef CGLOBALDEFINE
= {Combo_Max_Base,1};
#endif
;

GVAR int TabCaptionType GPARAM(0);
GVAR TCHAR *TabCaptionText GPARAM(NULL);

// Report
GVAR BOOL Reportcolor GPARAM(FALSE);
GVAR COLORREF CC_log[2]
#ifdef CGLOBALDEFINE
= {C_AUTO,C_AUTO}
#endif
;

//Tree
GVAR DWORD ComboTreeMode GPARAM(PPCTREE_OFF);
GVAR ComboDropTarget *pComboDropTarget GPARAM(NULL);

// Addr
#define ADDRBMP_SIZE 20 // WinXP 限定
#define ADDRBMP_RESID 230 // WinXP 限定 Shell32.dll 内矢印

//=============================================================================
extern HWND hComboFocus;
extern HWND hComboRightFocus;

extern const TCHAR *ChangeReason;

extern int ComboInit;
extern ThSTRUCT thGuiWork;	// リバー関連の情報保存に使用する
extern int MoveSplit;

extern int TopDockHeight;		// ToolBar Window の高さ
extern int TabHeight;		// Tab window の高さ
extern int InfoHeight;		// 情報行の高さ
extern int AddrHeight;		// Address window の高さ
//=============================================================================
// PPCOMBO.C
extern LRESULT CALLBACK ComboProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);
extern LRESULT ComboLMouseUp(LPARAM lParam);
extern LRESULT ComboMouseMove(HWND hWnd,LPARAM lParam);

typedef struct {
	PAINTSTRUCT ps;
	HBRUSH hBr;
} DRAWCOMBOSTRUCT;
extern void DrawPaneArea(DRAWCOMBOSTRUCT *dcs);

#define SORTWIN_LAYOUTALL -1	// 窓内の配置が変更
#define SORTWIN_LAYOUTPAIN -2	// ペインの並びや割合が変更
#define SORTWIN_RESIZE -3	// 窓の大きさが変更
#define SORTWIN_FIXINNER -4	// SortWindows内での再帰調整用
#define SORTWIN_FIX_NORESIZE (Combo_Max_Base * 4)
extern void SortComboWindows(int fixshowindex);

extern void KCW_FocusFix(HWND hWnd,HWND hTargetWnd);

// PPCOMBOS.C
extern void ResetR(TCHAR *buf);
extern void CheckComboTable(const TCHAR *p1);
extern void WmComboDestroy(BOOL EndSession);
extern ERRORCODE PaneCommand(const TCHAR *param,int targetbaseindex);
extern void SetComboAddresBar(const TCHAR *path);
extern void EnterAddressBar(void);
extern void SetComboReportText(const TCHAR *text);
extern void ComboCust(void);
extern void CloseLeftArea(void);
extern void CreateLeftArea(DWORD mode,const TCHAR *initpath);
extern void CreateJobArea(void);
extern void InitComboGUI(void);

// PPCOMBOB.C	ツールバー
extern void ComboCreateToolBar(HWND hWnd);
extern void ComboToolbarCommand(int id,int orcode);

// PPCOMBOP.C	タブバー & ペイン等管理
// タブ関連
#define CREATETAB_FIRST 0 // 起動時
#define CREATETAB_APPEND 1 // 実行時
extern void CreateTabBar(int mode);
extern void SetTabInfoData(int setinfo,int showindex,HWND hItemWnd);
#define SETTABINFO_SET 0
#define SETTABINFO_ADD 1
#define SETTABINFO_ADDENTRY 2
#define SetTabInfo(showindex,hItemWnd) SetTabInfoData(SETTABINFO_SET,showindex,hItemWnd)
#define AddTabInfo(showindex,hItemWnd) SetTabInfoData(SETTABINFO_ADD,showindex,hItemWnd)

extern void SetTabColor(int baseindex);
extern BOOL TabMenu(HWND hWnd,int baseindex,int targetpane,POINT *pos);
extern int GetTabItemIndex(HWND hWnd,int tabwndindex);

extern void SelectChangeTab(NMHDR *nmh);
extern BOOL SetTabTipText(NMHDR *nmh);
extern void SelectHidePane(int showindex,HWND hWnd);
extern void ComboSwap(void);
extern void CreatePane(int baseindex);
extern void CreateAndInitPane(int baseindex);
extern void EjectPane(int baseindex);
extern void HidePane(int showindex);

extern void DestroyedPaneWindow(HWND hWnd,HWND hPaneWnd);
extern void SelectComboWindow(int showindex,HWND hTargetWnd,BOOL focus);
extern void SelectTabByWindow(HWND hTargetWnd,int selectpane);

extern int GetComboBaseIndex(HWND hWnd);
extern int GetComboShowIndex(HWND hWnd);
extern int GetComboShowIndexDefault(HWND hWnd);
extern int GetComboShowIndexFromPos(POINT *pos);
extern int GetPairPaneComboShowIndex(HWND hTargetWnd);
extern int GetPairPaneComboBaseIndex(HWND hTargetWnd);

extern void SwapPane(int targetpane);
extern void NewPane(int baseindex,const TCHAR *param);
extern void ClosePanes(HWND hTabWnd,int baseindex,int mode);
extern void CreateNewTabParam(int showindex,int newbaseindex,const TCHAR *param);
extern HWND GetHwndFromIDCombo(const TCHAR *regid);


#define NestedMsgs 16
extern int ComboProcNested;
extern UINT ComboProcMsg[NestedMsgs];
GVAR int LastTabInsType GPARAM(0);
GVAR int UseTabInsType GPARAM(0);

#ifdef __cplusplus
}
#endif
