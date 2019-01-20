/*-----------------------------------------------------------------------------
	Paper Plane xUI		customizer
-----------------------------------------------------------------------------*/
#define PPXCUSTMODE_temp_RESTORE -1

#define LCID_PPXDEF LCID_JAPANESE

extern const TCHAR StrCustTitle[];
extern const TCHAR GetFileExtsStr[];
extern TCHAR PPcustRegID[REGIDSIZE];
extern const TCHAR NilStr[1];
extern TCHAR NilStrNC[1];
extern HINSTANCE hInst;
extern UINT WM_PPXCOMMAND;

extern void InfoDb(TCHAR *buf);
#define DIALOG_KEYINPUT 0
#define DIALOG_DISPFOMAT 1
extern int *SubDialog;
// console 関連 ---------------------------------------------------------------
extern void Print(const TCHAR *str);
extern int CustDump(const TCHAR *filename);
extern int CustStore(TCHAR *fname,int appendmode,HWND hDlg);
extern int CustInit(void);
extern int HistDump(const TCHAR *cmdptr, const TCHAR *filename);
extern int HistInit(void);

// GUI 関連 -------------------------------------------------------------------
#define IC_CUST			1
#define BUTTONIMAGE		2

extern BOOL X_chidc; // コンソールを隠すか
extern HWND hConsoleWnd; // コンソールウィンドウ
extern LCID UseLcid;		// 表示言語
extern OSVERSIONINFO OSver;
extern DWORD X_dss;

extern TCHAR *FirstTypeName;
extern TCHAR *FirstItemName;

extern COLORREF userColor[16];
extern void FixColorList(void);
extern int GetListCursorIndex(WPARAM wParam,LPARAM lParam,int *indexptr);
extern void InitWndIcon(HWND hDlg,UINT baseControlID);
extern const TCHAR * (USEFASTCALL *GetCText)(const TCHAR *text);
extern void MakeKeyDetailText(int key,TCHAR *dest,BOOL extype);
extern void GetControlText(HWND hDlg,UINT ID,TCHAR *text,DWORD len);

#ifdef UNICODE
	#define GetCTextW GetCText
#else
	extern const WCHAR *GetCTextW(const WCHAR *text);
#endif

// ダイアログ
extern void GUIcustomizer(int startpage,const TCHAR *param);

extern INT_PTR CALLBACK FilePage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK GeneralPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK ExtPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK KeyPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK MousePage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK MenuPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK BarPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK ColorPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK AddonPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK EtcPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);

extern INT_PTR CALLBACK StyleDlgProc(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);

extern INT_PTR CALLBACK TextCustomizeDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK CommandTreeDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);

extern void FixWildItemName(TCHAR *item);

// ppcst_et.c その他
extern void Changed(HWND hWnd);
extern int KeyInput(HWND hWnd,TCHAR *string);
extern void Test(void);
extern TCHAR *LoadTextResource(LPCTSTR rname);
extern void USEFASTCALL SetDlgFocus(HWND hDlg,int id);
extern void EnableDlgWindow(HWND hDlg,int id,BOOL state);
extern void ShowDlgWindow(const HWND hDlg,const UINT id,BOOL show);
extern void HideDlgWindow(const HWND hDlg,const UINT id);
extern void CustNameEscape(TCHAR *label);
extern void tstrreplace(TCHAR *text,const TCHAR *targetword,const TCHAR *replaceword);

extern INT_PTR CALLBACK HideMenuColorDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);
extern INT_PTR CALLBACK HighlightDialogBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);

extern int EtcEditFormat;
	// EtcEditFormat の種類
enum { ETC_TEXT = 0,
	ETC__EDITBUTTON, // これ以降は「編集」ボタン
	ETC_CELLDISP,ETC_INFODISP,ETC_HKEY,
	ETC__VIEW, // ここ以降は「編集」ボタンが無効になる
	ETC_DSET,ETC_WINPOS,ETC_EXECS,ETC_NOPARAM};

struct EtcLabelsStruct {
	const TCHAR *name,*key,*comment;
	int form;
};

extern const struct EtcLabelsStruct EtcLabels[];

// ppcst_ed.c その他-PPc表示書式
extern void FormatCellDispSample(TCHAR *data,const TCHAR *label,int offset);
extern INT_PTR CALLBACK DispFormatDialogBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);
