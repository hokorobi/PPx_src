/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						～ 拡張エディット ～
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

// CMDSEARCH_ 内部用
#define CMDSEARCHI_FINDFIRST	B16	// SearchFileInedMain 内でFindFirstを１度行った
#define CMDSEARCHI_SAVEWORD	B17	// 分離単語をFnamePに書き込む(始めの１回目専用)
#define CMDSEARCHI_FINDBRAKET	B18	// 括弧有り(通知用)
#define CMDSEARCHI_SELECTPART	B19	// 補完時に不一致部分を選択

#define TREEHEIGHT	240
#define EDITBUFSIZE	0x1000
#define CBLISTEIGHT	200

struct PPeFindReplace {
	TCHAR findtext[VFPS];
	TCHAR replacetext[VFPS];
};

// 補完用テキストファイル
typedef struct {
	const TCHAR *filename;	// ベースとなるファイル名
	TCHAR *mem;		// イメージの先頭
	TCHAR *text;	// イメージ内のテキストの先頭
//	FILETIME modifytime;
} FILLTEXTFILE;

// ComboList.mode の値
typedef enum {
	LIST_SEARCH,	// 前方一致検索(リスト表示無し)
	LIST_WRITE,		// 書込み用ヒストリで検索(リスト表示無し)
	LIST_READ,		// 読込み用ヒストリで検索(リスト表示無し)
	LIST_MANUAL,	// リストを手動で開いた
	LIST_FILL,		// エントリ自動補完用リスト
	LIST_FILLEXT	// エントリ自動補完用リスト(拡張子考慮有り)
} COMBOLISTMODE;

#define LISTU_NOLIST 0	// リストウィンドウは使用していない
#define LISTU_FOCUSMAIN 1	// 第１リスト／エディットボックスにフォーカス
#define LISTU_FOCUSSUB 2	// 第２リストにフォーカス

struct ComboList {	// コンボボックス処理用リストボックス
	HWND	hWnd, hSubWnd;	// 第１リスト用 hWnd と 第２リスト用 hWnd
	WNDPROC	OldProc, OldSubProc;	// 元 WndProc
	TCHAR	*OldString;		// リストボックス起動前のエディットボックスの内容
	DWORD	OldStart, OldEnd;// ↑の選択範囲
	int		ListWindow;		// LISTU_
	int		index;			// リスト非表示時に使用するヒストリ参照index
	ECURSOR	select;			// 書き換える範囲
	COMBOLISTMODE mode;		// 参照方法(現在) LIST_
	COMBOLISTMODE startmode;	// 参照方法(開始時) LIST_
	int		direction;		// 第１リストの表示方向 >0:下側 =<0:上側
	WORD	WhistID;		// 書込み用ヒストリID
	WORD	RhistID;		// 読込み用ヒストリID
	FILLTEXTFILE filltext_user;	// ユーザ指定用
};

// 内部で使用する PPXEDIT_ 。原則として B30 から順番に使用する

#define PPXEDIT_LINEEDIT	B27	// 一行編集ダイアログ上で使用
#define PPXEDIT_TEXTEDIT	B28	// テキスト編集(MULTILINE)
#define PPXEDIT_LISTCOMP	B29	// list 形式補完
#define PPXEDIT_LINECSR		B30	// Line cursor

typedef struct tagPPxEDSTRUCT {
	PPXAPPINFO *info;	// ※必ず構造体の先頭に配置すること！
	HWND	hWnd;		// 自分自身の hWnd
	HWND	hTreeWnd;	// ツリー表示用ウィンドウ
	struct {
		HWND	hWnd, hToolBarWnd;	// 浮遊ツールバー用ウィンドウ
		ThSTRUCT Cmd;
	} FloatBar;
	WNDPROC hOldED;		// 元のウィンドウプロージャ
	int		flags;		// PPxRegistExEdit の PPXEDIT_ と同じ
	const TCHAR *AKey;	// 追加キー割当て
	CALLBACKMODULEENTRY KeyHookEntry;		// キー入力拡張モジュール
					// GUI情報
	DWORD	style;		// Window style
	DWORD	exstyle;	// Window exstyle
	DWORD	fontY, OrgFontY;	// 文字高さ,初期文字高さ
	int		caretY;		// カレットのY
	DWORD	caretLY;	// カレットの行数Y
	int		tab;		// TAB 幅(ダイアログ単位)
					// テキスト情報
	int		CrCode;		// 保存時に使用する改行コード
	int		CharCode;	// 保存時に使用する文字コード
					// 補完
	ESTRUCT	ED;			// GUI/CUI 共通構造体
					// サーチ中の判断に使用
	WORD	oldkey;		// 直前の入力キー
	WORD	oldkey2;	// 直前の入力キー
	TCHAR	filename[VFPS];
	struct PPeFindReplace *findrep;
	BOOL	mousepos;	// メニューの表示位置 FALSE:キー操作 TRUE:マウス操作
						// 一覧表示関係
	struct ComboList list;

	volatile DWORD ActiveListThreadID; // 現在有効なスレッドのID
	volatile DWORD ListThreadCount; // 生きているスレッド数
} PPxEDSTRUCT;

typedef struct {
	WORD key;		// 使用するキー 0なら末尾
						// 実行する関数	0:スルー処理、!0:戻り値
	int (USEFASTCALL *func)(PPxEDSTRUCT *PES);
} KEYCOMMANDS;

typedef struct {
	WORD	key;		// 押されたキー・機能.
	int		repeat;		// リピート数
} PPECOMMANDPARAM;
typedef BOOL (APIENTRY *impGetOpenFileName)(LPOPENFILENAME lpofn);

typedef struct {
	TCHAR	text[EDITBUFSIZE];	// 作業領域
	TCHAR	*word;				// 取得した文字列へのポインタ
	ECURSOR	cursor, cursororg;	// 選択範囲
} TEXTSEL;

enum { TEXTSEL_ALL = 0, TEXTSEL_CHAR = 1, TEXTSEL_BACK, TEXTSEL_WORD, TEXTSEL_BEFORE, TEXTSEL_AFTER };

//--------------------------------------------------------------- PPD_EDL.C
extern int xpbug;

LRESULT CALLBACK EDsHell(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
extern void InitExEdit(PPxEDSTRUCT *PES);
extern const TCHAR PPxED[];

#define BRAKET_NONE 0
#define BRAKET_LEFT 1
#define BRAKET_LEFTRIGHT 2
extern int GetWordStrings(TCHAR *line, ECURSOR *cursor);

extern ERRORCODE PPXAPI EditExtractMacro(PPxEDSTRUCT *PES, const TCHAR *param, TCHAR *extract, int flags);

typedef struct {
	int codepage;
	int crcode;
	int tabwidth;
} EDITMODESTRUCT;
#define EDITMODESTRUCT_DEFAULT 0,-1,-1

extern void PPeSetTab(PPxEDSTRUCT *PES, int usetab);
extern BOOL EditModeParam(EDITMODESTRUCT *ems, const TCHAR *param, const TCHAR *more);
extern void XEditSetModify(PPxEDSTRUCT *PES);
extern void XEditClearModify(PPxEDSTRUCT *PES);

#define charmenu_items 13
#define charmenu_other 10
#define charmenu_sjis 2
extern PPXINMENU returnmenu[];
extern PPXINMENU charmenu[charmenu_items];
//--------------------------------------------------------------- PPD_EDLC.C
typedef int EDITDIST;
#define EDITDIST_BACK ((EDITDIST)-1)
#define EDITDIST_DIALOG ((EDITDIST)0)
#define EDITDIST_NEXT ((EDITDIST)1)
#define EDITDIST_NEXT_FILL ((EDITDIST)2) // FloatListで使用、LIST_FILL有効
#define EDITDIST_NEXT_WITHNOW ((EDITDIST)3) // SearchStrで使用、現在位置も検索

extern void FreeBackupText(PPxEDSTRUCT *PES);
extern void FloatList(PPxEDSTRUCT *PES, EDITDIST dist);
extern BOOL ListUpDown(HWND hWnd, PPxEDSTRUCT *PES, int offset, int repeat);
extern ERRORCODE ListPageUpDown(PPxEDSTRUCT *PES, EDITDIST dist);
extern void ListSearch(HWND hWnd, PPxEDSTRUCT *PES, int len);
extern void KeyStepFill(PPxEDSTRUCT *PES, BOOL histmode);
extern void FixListPosition(PPxEDSTRUCT *PES, HWND hWnd);
extern void ListSelect(PPxEDSTRUCT *PES, HWND hListWnd);
extern HWND PPxRegistExEditCombo(HWND hED, int maxlen, const TCHAR *defstr, WORD rHist, WORD wHist);

extern void CreateMainListWindow(PPxEDSTRUCT *PES, int direction);
extern void CreateSubListWindow(PPxEDSTRUCT *PES);
extern void CancelListThread(PPxEDSTRUCT *PES);

//--------------------------------------------------------------- PPD_EDLL.C
extern void ClosePPeTreeWindow(PPxEDSTRUCT *PES);
extern void PPeTreeWindow(PPxEDSTRUCT *PES);

//--------------------------------------------------------------- PPD_EDLM.C
#define PPE_OPEN_MODE_OPEN	0
#define PPE_OPEN_MODE_INSERT	1
#define PPE_OPEN_MODE_OPENNEW	-1
#define PPE_OPEN_MODE_CMDOPEN	2
#define PPE_OPEN_MODE_CMDINSERT	3
#define PPE_OPEN_MODE_EXCMDOPEN	4
extern BOOL OpenFromFile(PPxEDSTRUCT *PES, int openmode, const TCHAR *fname);
extern void OpenMainFromMem(PPxEDSTRUCT *PES, int openmode, const TCHAR *filename, const TCHAR *textimage, DWORD memsize, int usecp);

#define EDL_FILEMODE_APPEND		B0
#define EDL_FILEMODE_NODIALOG	B1
#define EDL_FILEMODE_DIALOG		B2

extern BOOL FileSave(PPxEDSTRUCT *PES, int mode);
extern void FileOpen(PPxEDSTRUCT *PES, int mode);
extern void JumptoLine(HWND hWnd, int line);
extern void InitEditCharCode(PPxEDSTRUCT *PES);

//--------------------------------------------------------------- PPD_EDLS.C
#ifndef UNICODE
extern void USEFASTCALL CaretFixToW(const char *str, DWORD *of);
extern void USEFASTCALL CaretFixToA(const char *str, DWORD *of);
#else
#define CaretFixToW(str, of)
#define CaretFixToA(str, of)
#endif
extern BOOL InitPPeFindReplace(PPxEDSTRUCT *PES);
extern BOOL SelectEditStrings(PPxEDSTRUCT *PES, TEXTSEL *ts, int mode);
extern TCHAR *SearchFileInedMain(ESTRUCT *ED, TCHAR *str, int mode);

extern BOOL SearchStr(PPxEDSTRUCT *PES, EDITDIST dist);
extern int CheckXPbug(void);
extern void GetHeight(PPxEDSTRUCT *PES, HFONT hFont);
extern void LineCursor(PPxEDSTRUCT *PES, DWORD mes);
extern void USEFASTCALL SetMessageForEdit(HWND hWnd, const TCHAR *message);

#ifndef UNICODE
extern void GetEditSel(HWND hWnd, TCHAR *buf, ECURSOR *cursor);
extern void SetEditSel(HWND hWnd, TCHAR *buf, DWORD start, DWORD end);
#else
#define GetEditSel(hWnd, buf, cursor) SendMessage(hWnd, EM_GETSEL, (WPARAM)cursor.start, (LPARAM)cursor.end)
#define SetEditSel(hWnd, buf, start, end) SendMessage(hWnd, EM_SETSEL, start, end)
#endif

#ifdef __cplusplus
}
#endif
