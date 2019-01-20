/*-----------------------------------------------------------------------------
	Paper Plane xUI		Dock処理
-----------------------------------------------------------------------------*/
struct tagPPC_APPINFO;

typedef struct {
	HFONT h;
	int X,Y;
} FONTINFO;
extern HBITMAP hAddressBMP;

#define REBARFIXWIDTH 3

#define NOSETWID MAX32

typedef struct {
	HWND hWnd;			// ドックに使うリバーウィンドウのハンドル
	struct tagPPC_APPINFO *cinfo; // 内容の表示対象となる PPc
	ThSTRUCT *th;		// 各種の情報保存に使用する領域
	TCHAR cust[8];		// カスタマイズに使用する名前(X_dock内の項目名,最大7字)
	// ↑上の４つは外部参照のために変更禁止
	RECT client;		// 表示領域
	UINT IDmin,IDmax;	// WM_COMMAND で使用される IDmin～IDmax
	HWND hInfoWnd;		// XC_inf1/2 に使用するウィンドウ
	HWND hStatusWnd;	// XC_stat に使用するウィンドウ
	HWND hAddrWnd;		// アドレスバーのEDITウィンドウ
	FONTINFO font;
} PPXDOCK;

#ifndef RBN_HEIGHTCHANGE
#define RBN_FIRST (0U-831U)
#define RBN_HEIGHTCHANGE    (RBN_FIRST - 0)
#endif

#define dock_menu 0
#define dock_add 1
#define dock_delete 2
#define dock_toggle 3
#define dock_focus 4
#define dock_drop 5
#define dock_sendkey 6
#define dock_size 7

typedef struct {
	PPXDOCK t,b;	// 上側、下側のドック
} PPXDOCKS;

extern const TCHAR RMENUSTR_LOG[];
extern const TCHAR RMENUSTR_JOB[];
extern const TCHAR RMPATHSTR[];

extern void DocksInit(PPXDOCKS *docks,HWND hWnd,struct tagPPC_APPINFO *cinfo,const TCHAR *RegID,HFONT hFont,int fontY,ThSTRUCT *th,UINT *CommandID);
extern BOOL DocksWmCommand(PPXDOCKS *docks,WPARAM wParam,LPARAM lParam);
extern BOOL DocksNotify(PPXDOCKS *docks,NMHDR *nmh);
extern BOOL DocksNeedTextNotify(PPXDOCKS *docks,NMHDR *nmh);
extern void DocksInfoRepaint(PPXDOCKS *docks);
extern void DocksStatusRepaint(PPXDOCKS *docks);
extern int GetHiddenMenuItemTypeFromPoint(struct tagPPC_APPINFO *cinfo,int bottomY,int height,POINT *pos,int *ItemNo);
extern void DockFixPPcBarSize(PPXDOCK *dock);
extern void DockModifyMenu(HWND hWnd,PPXDOCK *dock,POINT *menupos);
extern BOOL DockAddBar(HWND hWnd,PPXDOCK *dock,const TCHAR *name);
extern BOOL DockDropBar(struct tagPPC_APPINFO *cinfo,PPXDOCK *dock,const TCHAR *name);
extern BOOL DockCommands(HWND hWnd,PPXDOCK *dock,int mode,const TCHAR *name);
extern void InitEditColor(void); // EditControl関連CC_logの色設定
extern BOOL ToolBarDirectoryButtonRClick(HWND hParentWnd,NMHDR *nmh,ThSTRUCT *th);
extern void DocksWmDevicechange(PPXDOCKS *docks,WPARAM wParam,LPARAM lParam);
extern HMENU MakeDockMenu(PPXDOCK *dock,HMENU hPopupMenu,DWORD *index,ThSTRUCT *TH);
extern void LoadAddressBitmap(void);
extern void DrawAddressButton(HDC hDC,RECT *box);

// BCC 5.0 に無い定義
#ifdef TOOLBARCLASSNAME
//------------------------------------- ReBar
#ifndef REBARCLASSNAME
#define REBARCLASSNAME T("ReBarWindow32")

#define RBS_TOOLTIPS        0x0100
#define RBS_VARHEIGHT       0x0200
#define RBS_BANDBORDERS     0x0400
#define RBS_FIXEDORDER      0x0800
#define RBS_REGISTERDROP    0x1000
#define RBS_AUTOSIZE        0x2000
#define RBS_VERTICALGRIPPER 0x4000
#define RBS_DBLCLKTOGGLE    0x8000

typedef struct tagREBARINFO
{
	UINT		cbSize;
	UINT		fMask;
#ifndef NOIMAGEAPIS
	HIMAGELIST	himl;
#else
	HANDLE		himl;
#endif
}   REBARINFO, FAR *LPREBARINFO;

#define RBBS_BREAK          0x00000001
#define RBBS_FIXEDSIZE      0x00000002
#define RBBS_CHILDEDGE      0x00000004
#define RBBS_HIDDEN         0x00000008
#define RBBS_NOVERT         0x00000010
#define RBBS_FIXEDBMP       0x00000020
#define RBBS_VARIABLEHEIGHT 0x00000040
#define RBBS_GRIPPERALWAYS  0x00000080
#define RBBS_NOGRIPPER      0x00000100
#define RBBS_USECHEVRON     0x00000200
#define RBBS_HIDETITLE      0x00000400
#define RBBIM_STYLE         0x00000001
#define RBBIM_COLORS        0x00000002
#define RBBIM_TEXT          0x00000004
#define RBBIM_IMAGE         0x00000008
#define RBBIM_CHILD         0x00000010
#define RBBIM_CHILDSIZE     0x00000020
#define RBBIM_SIZE          0x00000040
#define RBBIM_BACKGROUND    0x00000080
#define RBBIM_ID            0x00000100
#define RBBIM_IDEALSIZE     0x00000200
#define RBBIM_LPARAM        0x00000400
#define RBBIM_HEADERSIZE    0x00000800

typedef struct tagREBARBANDINFOA
{
	UINT        cbSize;
	UINT        fMask;
	UINT        fStyle;
	COLORREF    clrFore;
	COLORREF    clrBack;
	LPSTR       lpText;
	UINT        cch;
	int         iImage;
	HWND        hwndChild;
	UINT        cxMinChild;
	UINT        cyMinChild;
	UINT        cx;
	HBITMAP     hbmBack;
	UINT        wID;
#if (_WIN32_IE >= 0x0400)
	UINT        cyChild;
	UINT        cyMaxChild;
	UINT        cyIntegral;
	UINT        cxIdeal;
	LPARAM      lParam;
	UINT        cxHeader;
#endif
}   REBARBANDINFOA, FAR *LPREBARBANDINFOA;
typedef REBARBANDINFOA CONST FAR *LPCREBARBANDINFOA;

#define REBARBANDINFOA_V3_SIZE CCSIZEOF_STRUCT(REBARBANDINFOA, wID)
#define REBARBANDINFOW_V3_SIZE CCSIZEOF_STRUCT(REBARBANDINFOW, wID)

typedef struct tagREBARBANDINFOW
{
	UINT        cbSize;
	UINT        fMask;
	UINT        fStyle;
	COLORREF    clrFore;
	COLORREF    clrBack;
	LPWSTR      lpText;
	UINT        cch;
	int         iImage;
	HWND        hwndChild;
	UINT        cxMinChild;
	UINT        cyMinChild;
	UINT        cx;
	HBITMAP     hbmBack;
	UINT        wID;
#if (_WIN32_IE >= 0x0400)
	UINT        cyChild;
	UINT        cyMaxChild;
	UINT        cyIntegral;
	UINT        cxIdeal;
	LPARAM      lParam;
	UINT        cxHeader;
#endif
}   REBARBANDINFOW, FAR *LPREBARBANDINFOW;
typedef REBARBANDINFOW CONST FAR *LPCREBARBANDINFOW;

#ifdef UNICODE
#define REBARBANDINFO       REBARBANDINFOW
#define LPREBARBANDINFO     LPREBARBANDINFOW
#define LPCREBARBANDINFO    LPCREBARBANDINFOW
#else
#define REBARBANDINFO       REBARBANDINFOA
#define LPREBARBANDINFO     LPREBARBANDINFOA
#define LPCREBARBANDINFO    LPCREBARBANDINFOA
#endif

#define RBN_FIRST (0U-831U)
#define RBN_LAST  (0U-859U)
#define RB_INSERTBANDA  (WM_USER +  1)
#define RB_DELETEBAND   (WM_USER +  2)
#define RB_GETBARINFO   (WM_USER +  3)
#define RB_SETBARINFO   (WM_USER +  4)
#define RB_GETBANDINFO  (WM_USER +  5)
#define RB_SETBANDINFOA (WM_USER +  6)
#define RB_SETPARENT    (WM_USER +  7)
#define RB_HITTEST      (WM_USER +  8)
#define RB_GETRECT      (WM_USER +  9)
#define RB_INSERTBANDW  (WM_USER +  10)
#define RB_SETBANDINFOW (WM_USER +  11)
#define RB_GETBANDCOUNT (WM_USER +  12)
#define RB_GETROWCOUNT  (WM_USER +  13)
#define RB_GETROWHEIGHT (WM_USER +  14)
#define RB_IDTOINDEX    (WM_USER +  16)
#define RB_GETTOOLTIPS  (WM_USER +  17)
#define RB_SETTOOLTIPS  (WM_USER +  18)
#define RB_SETBKCOLOR   (WM_USER +  19)
#define RB_GETBKCOLOR   (WM_USER +  20)
#define RB_SETTEXTCOLOR (WM_USER +  21)
#define RB_GETTEXTCOLOR (WM_USER +  22)
#define RB_SIZETORECT   (WM_USER +  23)
#define RB_SETCOLORSCHEME   CCM_SETCOLORSCHEME
#define RB_GETCOLORSCHEME   CCM_GETCOLORSCHEME

#ifdef UNICODE
#define RB_INSERTBAND   RB_INSERTBANDW
#define RB_SETBANDINFO   RB_SETBANDINFOW
#else
#define RB_INSERTBAND   RB_INSERTBANDA
#define RB_SETBANDINFO   RB_SETBANDINFOA
#endif

#define RB_BEGINDRAG    (WM_USER + 24)
#define RB_ENDDRAG      (WM_USER + 25)
#define RB_DRAGMOVE     (WM_USER + 26)
#define RB_GETBARHEIGHT (WM_USER + 27)
#define RB_GETBANDINFOW (WM_USER + 28)
#define RB_GETBANDINFOA (WM_USER + 29)

#define RB_MINIMIZEBAND (WM_USER + 30)
#define RB_MAXIMIZEBAND (WM_USER + 31)
#define RB_GETDROPTARGET (CCM_GETDROPTARGET)
#define RB_GETBANDBORDERS (WM_USER + 34)
#define RB_SHOWBAND     (WM_USER + 35)
#define RB_SETPALETTE   (WM_USER + 37)
#define RB_GETPALETTE   (WM_USER + 38)
#define RB_MOVEBAND     (WM_USER + 39)
#define RB_SETUNICODEFORMAT     CCM_SETUNICODEFORMAT
#define RB_GETUNICODEFORMAT     CCM_GETUNICODEFORMAT
#define RB_PUSHCHEVRON  (WM_USER + 43)
#define RBN_HEIGHTCHANGE    (RBN_FIRST - 0)
#define RBN_GETOBJECT       (RBN_FIRST - 1)
#define RBN_LAYOUTCHANGED   (RBN_FIRST - 2)
#define RBN_AUTOSIZE        (RBN_FIRST - 3)
#define RBN_BEGINDRAG       (RBN_FIRST - 4)
#define RBN_ENDDRAG         (RBN_FIRST - 5)
#define RBN_DELETINGBAND    (RBN_FIRST - 6)
#define RBN_DELETEDBAND     (RBN_FIRST - 7)
#define RBN_CHILDSIZE       (RBN_FIRST - 8)
#define RBN_CHEVRONPUSHED   (RBN_FIRST - 10)
#define RBN_MINMAX          (RBN_FIRST - 21)

typedef struct tagNMREBARCHILDSIZE
{
	NMHDR hdr;
	UINT uBand;
	UINT wID;
	RECT rcChild;
	RECT rcBand;
} NMREBARCHILDSIZE, *LPNMREBARCHILDSIZE;
#define RBNM_ID         0x00000001
#define RBNM_STYLE      0x00000002
#define RBNM_LPARAM     0x00000004
typedef struct tagNMREBAR
{
	NMHDR   hdr;
	DWORD   dwMask;           // RBNM_*
	UINT    uBand;
	UINT    fStyle;
	UINT    wID;
	LPARAM  lParam;
} NMREBAR, *LPNMREBAR;

typedef struct tagNMRBAUTOSIZE
{
	NMHDR hdr;
	BOOL fChanged;
	RECT rcTarget;
	RECT rcActual;
} NMRBAUTOSIZE, *LPNMRBAUTOSIZE;

typedef struct tagNMREBARCHEVRON
{
	NMHDR hdr;
	UINT uBand;
	UINT wID;
	LPARAM lParam;
	RECT rc;
	LPARAM lParamNM;
} NMREBARCHEVRON, *LPNMREBARCHEVRON;
#define RBHT_NOWHERE    0x0001
#define RBHT_CAPTION    0x0002
#define RBHT_CLIENT     0x0003
#define RBHT_GRABBER    0x0004
#define RBHT_CHEVRON    0x0008
typedef struct _RB_HITTESTINFO
{
	POINT pt;
	UINT flags;
	int iBand;
} RBHITTESTINFO, FAR *LPRBHITTESTINFO;
#endif

#ifndef RB_GETBARHEIGHT
#define RB_GETBARHEIGHT (WM_USER + 27)
#define RBN_FIRST (0U-831U)
#endif

#ifndef NM_CUSTOMDRAW
#define CDIS_SELECTED       0x0001
#define CDIS_GRAYED         0x0002
#define CDIS_DISABLED       0x0004
#define CDIS_CHECKED        0x0008
#define CDIS_FOCUS          0x0010
#define CDIS_DEFAULT        0x0020
#define CDIS_HOT            0x0040
#define CDIS_MARKED         0x0080
#define CDIS_INDETERMINATE  0x0100

#define CDRF_DODEFAULT          0x00000000
#define CDRF_NEWFONT            0x00000002
#define CDRF_SKIPDEFAULT        0x00000004


#define CDRF_NOTIFYPOSTPAINT    0x00000010
#define CDRF_NOTIFYITEMDRAW     0x00000020
#define CDRF_NOTIFYSUBITEMDRAW  0x00000020
#define CDRF_NOTIFYPOSTERASE    0x00000040
#define NM_CUSTOMDRAW           (NM_FIRST-12)

typedef struct tagNMCUSTOMDRAWINFO
{
	NMHDR hdr;
	DWORD dwDrawStage;
	HDC hdc;
	RECT rc;
	DWORD_PTR dwItemSpec;
	UINT  uItemState;
	LPARAM lItemlParam;
} NMCUSTOMDRAW, FAR * LPNMCUSTOMDRAW;
#endif

#ifndef RBS_DBLCLKTOGGLE
#define RBS_DBLCLKTOGGLE    0x8000
#define RBN_LAYOUTCHANGED   (RBN_FIRST - 2)
#endif

#ifndef RB_SETBANDWIDTH
#define RB_SETBANDWIDTH (WM_USER + 44)
#endif
#endif
