/*-----------------------------------------------------------------------------
	Paper Plane bUI
-----------------------------------------------------------------------------*/
#define WNDTITLESIZE 0x80
extern HANDLE hCommSendEvent;	// 外部からのコマンド受け付け用イベント
extern HANDLE hCommIdleEvent;	// 外部からのコマンド受け付け用イベント
extern HWND hMainWnd;		// コンソールウィンドウのハンドル
extern HWND hBackWnd;		// 最小化時に戻る Window
extern WINPOS WinPos;
extern TCHAR RegID[REGIDSIZE];
extern TCHAR RegCID[REGIDSIZE - 1];
extern TCHAR PPxPath[VFPS];	// PPx 本体のあるパス
extern TCHAR EditPath[VFPS];	// PPb command line のパス
extern TCHAR WndTitle[WNDTITLESIZE];
extern TCHAR *CurrentPath;		// 現在の実行パス(編集用/外部編集用を切り替え)
extern PPXAPPINFO ppbappinfo;
extern int RegNo;			// PPx Register ID
extern ThSTRUCT LongParam;	// 長いコマンドライン用

extern int SelStart;
extern int SelEnd;
extern TCHAR *EditText;		// 表示内容
extern DWORD ExitCode;

//------------------------------------- PPB_SUB.C
extern void PPbExecuteRecv(TCHAR *param);
extern void PPbExecuteInput(TCHAR *param, DWORD size);
extern DWORD_PTR USECDECL PPbInfoFunc(PPXAPPINFO *ppb, DWORD cmdID, PPXAPPINFOUNION *uptr);
extern void ReleasePPB(void);
extern void TitleDisp(const TCHAR *addmes);

//------------------------------------- TCINPUT.C
#define TCI_RECV	-3	// コマンド受信
#define TCI_EXECUTE	-2	// 実行
#define TCI_QUIT	-1	// 終了
extern int tCInput(TCHAR *buf, size_t bsize, WORD htype);
extern void Replace(const TCHAR *istr, BOOL all);
extern void CommonCommand(int key);
