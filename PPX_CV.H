/*-----------------------------------------------------------------------------
	Paper Plane xUI									～ PPc,PPv共用 ～
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#define MOUSE_LONG_PUSH_TIME 1100

#define MOUSEBUTTONMASK 0xfff3
#define MOUSEMODE_PUSH 0
#define MOUSEMODE_DRAG 1
#define MOUSEMODE_NONE 2
#define MOUSEBUTTON_CANCEL_DOUBLE -1
#define MOUSEBUTTON_CANCEL 0
#define MOUSEBUTTON_L 1
#define MOUSEBUTTON_R 2
#define MOUSEBUTTON_M 3
#define MOUSEBUTTON_W 4
#define MOUSEBUTTON_X 5
#define MOUSEBUTTON_Y 6
#define MOUSEBUTTON_Z 7
#define MOUSEBUTTON_ETC 8

#define MAXGESTURES	30	// ジェスチャーの最大コマンド数
#define GESTURETEXTSIZE	(MAXGESTURES * 2 + 32)

typedef struct {
	POINT PushScreenPoint;	// ボタンを押したときのスクリーン座標
	POINT PushClientPoint;	// ボタンを押したときのクライアント座標
	POINT MovedClientPoint;	// 最新のドラッグした後のクライアント座標
	POINT MovedScreenPoint;	// 最新のドラッグした後のスクリーン座標
	SIZE MovedOffset;		// 最新のドラッグした移動量
	RECT DDRect;			// ドラッグ開始検出範囲
	int mode;				// ドラッグ/押下状態 MOUSEMODE_
	int PushButton;			// 有効なボタン MOUSEBUTTON_
	DWORD PushTick;			// ボタンを押したときのTick
	int WheelDelta;			// ホイール移動量
	struct {
		int count;	// 入力コマンド数
		TCHAR step[MAXGESTURES];	// コマンド入力一覧
	} gesture;
} MOUSESTATE;

#define CF_DIBV5 17

typedef struct {
	HDC hOffScreenDC;  // 仮想DC
	BYTE *DCBitsPtr;  // hOffScreenDC で選択しているbitmapのbits部分
	HBITMAP hOldBitmap; // hOffScreenDCに割り当てられていたbitmap
	HTBMP X_WPbmp;
	int X_WallpaperType;
	DIRECTXDEFINE(DXBMPCACHE *WPbmpCache)
} BGSTRUCT;

typedef struct {
	int item;		// 個数
	char **data;	// 内容
} HIDDENMENUS;
extern void LoadHiddenMenu(HIDDENMENUS *hms,const TCHAR *name,HANDLE heap,COLORREF mes);
extern const TCHAR DwmapiName[];
extern HMODULE hDwmapi;

extern BOOL X_awhel;

extern void AutoColor(const TCHAR *str,COLORREF *clr,int def);
// 文字列操作
extern TCHAR USEFASTCALL upper(TCHAR c);
extern void GetClipboardTypeName(TCHAR *str,UINT cliptype);
// マウス
extern const TCHAR PPxMouseButtonChar[];
extern int CheckXMouseButton(WPARAM wParam);
extern void PPxInitMouseButton(MOUSESTATE *ms);
extern void PPxCancelMouseButton(MOUSESTATE *ms);
extern int PPxGetMouseButtonDownInfo(MOUSESTATE *ms,HWND hWnd,DWORD button,LPARAM lParam);
extern int PPxDownMouseButton(MOUSESTATE *ms,HWND hWnd,WPARAM wParam,LPARAM lParam);
extern int PPxUpMouseButton(MOUSESTATE *ms,WPARAM wParam);
extern int PPxDoubleClickMouseButton(MOUSESTATE *ms,HWND hWnd,WPARAM wParam,LPARAM lParam);
extern BOOL PPxMoveMouse(MOUSESTATE *ms,HWND hWnd,LPARAM lParam);
extern BOOL PPxCheckMouseGesture(MOUSESTATE *stat,TCHAR *textbuf,const TCHAR *keyname);

extern int PPxWheelMouse(MOUSESTATE *ms,HWND hWnd,WPARAM wParam,LPARAM lParam);
// キー入力
extern WORD FixCharKeycode(WORD key);

// 背景画像処理
extern void LoadWallpaper(BGSTRUCT *bg,HWND hWnd,const TCHAR *regid);
extern void UnloadWallpaper(BGSTRUCT *bg);
extern void DrawWallPaper(DIRECTXARG(DXDRAWSTRUCT *DxDraw) BGSTRUCT *bg,HWND hWnd,PAINTSTRUCT *ps,int Areax);

extern void InitOffScreen(BGSTRUCT *bg,HDC hWndDC,SIZE *WndSize);
extern void OffScreenToScreen(BGSTRUCT *bg,HWND hWnd,HDC hWndDC,RECT *WndRect,SIZE *WndSize);
extern void FreeOffScreen(BGSTRUCT *bg);

extern int PreviewCommand(BGSTRUCT *bg,const TCHAR *param,const TCHAR *path);
extern DWORD CalcBmpHeaderSize(BITMAPINFOHEADER *dumpdata);

// メニュー
extern void USEFASTCALL AppendMenuString(HMENU hMenu,UINT id,const TCHAR *string);
extern void USEFASTCALL AppendMenuCheckString(HMENU hMenu,UINT id,const TCHAR *string,BOOL check);

// FillRect 置き換え
extern void FillBox(HDC hDC,const RECT *box,HBRUSH hbr);

extern BOOL GetAndFixTextMetrics(HDC hDC, TEXTMETRIC *tm);

#ifdef __cplusplus
}
#endif
