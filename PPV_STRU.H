/*-----------------------------------------------------------------------------
	Paper Plane vUI												～ struct ～
-----------------------------------------------------------------------------*/
#include "PPX_DRAW.H"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MOVE_X 0x3fffffff  // X方向に移動可能な最大値
#include "TM_MEM.H"
#include "PPX_CV.H"

#define LONGTEXTSHIFT (12 - 1)
#define LONGTEXTWIDTH ((DWORD)(1 << LONGTEXTSHIFT))

#define DEFAULTCOLS	80	// テキストの標準表示桁数
#define DEFAULTTAB	8	// テキストの標準タブ桁数

#define PPV_MAX_VIEWSIZE ValueX3264( ((DWORD)1 * GB), ((DWORD)2 * GB) )

typedef struct {
	void	*ptr;		// 共有メモリへのポインタ
	HGLOBAL	mapH;		// 共有メモリハンドル
} VMEM;

enum {
	VCODE_END = 0,	// 終了
	VCODE_ASCII,	// Text(ASCIIZ)
	VCODE_UNICODEF,	// Text(UNICODEZ) + 補正用 1byte
	VCODE_UNICODE,	// Text(UNICODEZ)
	VCODE_COLOR,	// Color(BYTE fg, BYTE bg)
	VCODE_FONT,		// Font(BYTE type)
	VCODE_CONTROL,	// 制御文字
	VCODE_TAB,		// Tab
	VCODE_RETURN,	// RETURN(BYTE TRLSIG_xxx)
	VCODE_PARA,		// 改段
	VCODE_PAGE,		// 改頁
	VCODE_LINK,		// Link
	VCODE_SPACE,	// space
	VCODE_WSPACE,	// 全角space
	VCODE_FCOLOR,	// 文字色(COLORREF)
	VCODE_BCOLOR,	// 背景色(COLORREF)
	VCODE_MAX,		//
	VCODE_SWITCH	// VCODE の解析を行うところに指定があるマーカ
};
#define VCODE_COLOR_SIZE 3
#define VCODE_FCOLOR_SIZE 5


#define MAXLINELENGTH 0x800 // 1行の最大長
#define DISPTEXTBUFSIZE MAXLINELENGTH // 表示用内部コードバッファの大きさ ※ MAXLINELENGTH 以上

typedef struct {
	BYTE *buff; // 使用するバッファへのポインタ(確保済み/一時確保)
	DWORD size; // 使用できる大きさ
	BOOL use; // 使用中か否か
} DISPTEXTBUFSTRUCT;

typedef struct {
	BYTE *srcmax;
	BYTE *destbuf;
	DWORD destsize;
	BOOL writetbl;
	BOOL paintmode;
} MAKETEXTINFO;

// VT_TABLE.attrs
#define VTTF_TOP B0 // 行頭を含む
// #define VTTF_END B1 // 行末を含む
#define VTTF_TAG B2 // タグ表示の途中
#define VTTF_LINK B3 // リンク表示の途中
#define VTTF_HIGHLIGHT B4 // ハイライト有り
#define VTTF_USEBASE B5 // base64有り
#define VTTF_BASEOFFMASK (B6 | B7) // base64 のスキップ数
#define VTTF_BASEOFFSHIFT 6 // base64 のスキップ数

#pragma pack(push, 1)
typedef struct {
	BYTE *ptr;	// 表示開始位置
	BYTE Fclr;	// 前景色
	BYTE Bclr;	// 背景色
	BYTE type;	// 表示形式 VTYPE_
	BYTE attrs; // VTTF_
	DWORD line;	// 論理行数
} VT_TABLE;
#pragma pack(pop)

typedef struct {
	int			offX, offY;		// 表示開始位置
	int			csrX, csrY;		// カーソルの位置
	int			textC;			// テキスト: 表示文字コード
								// 0:IBM 1:ANSI 2:JIS 3:S-JIS 4:EUC 5:UNICODE
								// RAWIMAGE: 色情報
								//	B0-B5:色数 B6:1:10bit B7:1:RGB/0:BGR
	#define WIDTH_AUTOFIX -1
	#define WIDTH_NOWARP 0
	int			width, defwidth;	// 桁数(調整済み), 桁数(元値)
	int			cline;			// 計算済みの行数(cline≦line)
	int			line;			// 全行数(内容が古いものを含む)
	BOOL		reading;		// 読込み中か
	int			tab;			// Tab幅
	BYTE		*img;			// File image
	VT_TABLE	*ti;			// Text Display index
	BYTE		*(*MakeText)(MAKETEXTINFO *mti, VT_TABLE *tbl);
} VO_INFO;

typedef struct {
	VMEM *vo; // 参照中イメージ
	BYTE *img; //
	DWORD_PTR PresetPos; // 読み込み完了後に表示する行頭へのポインタ
	int PresetY; // 読み込み完了後に表示する Y 負なら未使用
	DWORD MemSize; // ファイルサイズ(≠読み込み済みサイズ)
		// B0～B15 は PPCOMMON.H で定義
		#define PPV__reload				B16 // 再読み込み(Divを初期化しない)
		#define PPV__NoGetPosFromHist	B17
		#define PPV__stdin				B18
	DWORD OpenFlags;
} VO_MAKETEXTINFO;

typedef struct {
	int pix;	// 該当位置の左上pixel座標
	int offset;	// 左端からの文字数
} SELPOSINFOX;

typedef struct {
	int pix;	// 該当位置の左上pixel座標
	int line;	// 表示行
} SELPOSINFOY;

typedef struct {
	SELPOSINFOX x;
	SELPOSINFOY y;
} SELPOSINFO;

typedef struct tagHILIGHTKEYWORD { // 固定ハイライトキーワードのテーブル
	struct tagHILIGHTKEYWORD *next;
	const char *ascii; // Multibyte版検索文字
	const WCHAR *wide; // UNICODE版検索文字
	COLORREF color; // ハイライトの色
	int extend; // ブロック全体を着色するとき HILIGHTKEYWORD_xxx
} HILIGHTKEYWORD;
#define HILIGHTKEYWORD_T B0 // 前に拡張
#define HILIGHTKEYWORD_B B1 // 後ろに拡張
#define HILIGHTKEYWORD_R B2 // 反転

#define SELPOSCHANGE_PIX 0		// pixからoffsetを求める
#define SELPOSCHANGE_OFFSET 1	// offsetが変更された
typedef struct {
	BOOL cursor;			// カーソル表示の有無
// 選択関連
	BOOL select;			// 選択の有無
	BOOL linemode;			// 行選択モードの判別 TRUE:行選択モード

	SELPOSINFO start, now;	// 選択開始位置   ／ 終了・現在位置
	SELPOSINFO bottom, top;	// 選択先頭・上端 ／ 末尾・下端位置(bottom < top)
	int bottomOY, topOY;	// 選択先頭／終了 Y位置、画面上からの相対値
							// ※ WM_PAINT 内で使用する
	int posdiffX;			// SELPOSCHANGE_PIX 時の文字と指定位置とのずれ(カーソル左右移動時に参照)
// 検索・検索結果関連
	BOOL highlight;			// ハイライト表示中
	int lastY, foundY;		// 直前の検索 Yと該当Y
	char VSstringA[VFPS];	// 検索文字列(入力ヒストリ)
	WCHAR VSstringW[VFPS];	// 検索文字列(入力ヒストリ)
} VO_SELECT;
#ifdef UNICODE
	#define VSstring VSstringW
#else
	#define VSstring VSstringA
#endif

enum {
	IMGD_NORMAL = 0, // 等倍
	IMGD_WINDOWSIZE = -1, // 窓枠に合わせる
	IMGD_AUTOWINDOWSIZE = -2,	 // 窓枠より小さければ等倍、そうでなければ縮小
	IMGD_FIXWINDOWSIZE = -3,	 // 窓枠の大きさを変える
	IMGD_AUTOFIXWINDOWSIZE = -4, // 窓枠の大きさを変える, 最大窓枠より大→縮小
//
	IMGD_AUTOWINDOWSIZE_2 = -5,  // IMGD_NORMAL / IMGD_AUTOWINDOWSIZE トグル
	IMGD_OFFSET = 10, // 拡縮オフセット
	IMGD_MINMAG = 10, // 最小縮小値
	IMGD_MAXMAG = 9990, // 最大拡大値
};

enum {
	IMGD_MM_FULLSCALE = 0, // 原寸
	IMGD_MM_FRAME, // 窓枠の大きさに拡縮
	IMGD_MM_MAGNIFY, // 任意倍率
};
#define ASPACTX 1000

typedef struct {
	HIDDENMENUS HiddenMenu;
	struct {
		int imgD[2];	// +0:IMGD_xx / 1-10000:%単位で拡大縮小
						// +1:SetStretchBltMode の値
		int MagMode;	// IMGD_MM_xx
		int AspectRate;	// アスペクト比 0:無効
		int AspectW, AspectH;
		BOOL monomode;	// 白黒表示モード
	} img;
} PPVVAR;
typedef BOOL (APIENTRY *impPPRINTDLG)(LPPRINTDLG);

enum {
	VSHOW_SDI = 0,	// SDI 表示
	VSHOW_ONWINDOW,		// 親ウィンドウに重ねて表示
	VSHOW_ONPAIRWINDOW,	// 親ウィンドウの反対ウィンドウに重ねて表示
	VSHOW_ONMAINWINDOW,	// 親ウィンドウが所属する主ウィンドウに重ねて表示
	VSHOW_INWINDOW,		// 親ウィンドウの子ウィンドウ
	VSHOW_INPAIRWINDOW,	// 親ウィンドウの反対ウィンドウの子ウィンドウ
	VSHOW_INMAINWINDOW,	// 親ウィンドウが所属する主ウィンドウの子ウィンドウ
};

typedef struct { // -1:指定なし
	DISPT dtype;	// 表示形式
	int history;	// ヒストリ保存の有無
	// テキスト関係
	int T_code;		// テキスト表示形式
	int T_siso;		// si/so 処理
	int T_esc;		// esc 処理
	int T_mime;		// mime 処理
	int T_tag;		// html/xml tag 処理
	int T_show_css;	// css 表示
	int T_show_script;	// script 表示
	int T_tab;		// タブ桁数
#define VIEWOPTIONS_WIDTHOFFSET 10
	int T_width;	// 桁数
	int linespace;	// 行間隔
	int tailflags;
	// 画像関係
	int I_animate;	// アニメーション表示
	int I_CheckeredPattern;	// 格子表示
} VIEWOPTIONS;

typedef struct X_prtsStruct {
	RECT margin;		// 余白
	LONG imagedivision;	// 画像の解像度(DPI) -1:用紙一杯 0:1200で200/400
} X_PRTSSTRUCT;

typedef struct {
	X_PRTSSTRUCT X_prts;
	int tileX, tileY;	// 集約処理
	int width, height;	// 文字桁数／行数
} PRINTINFO;

TypedefWinAPI(BOOL, sndPlaySound, (LPCTSTR pszSound, UINT fuSound));

typedef struct {
	PPXAPPINFO info;
} PPV_APPINFO;

typedef struct {
	COLORREF fg, bg;
	BOOL bgmode;
} SELINFO;

typedef struct {
	PAINTSTRUCT ps; // 描画情報
	RECT view; // 表示範囲

	int drawYtop, drawYbottom;	// 表示を行う範囲(行単位)
	SIZE shift;					// 表示開始位置をずらす文字単位の量
	SIZE shiftdot;				// 表示開始位置をずらすdot単位の量
	SIZE fontsize;				// 文字の大きさ
	int lineY;					// 一行の大きさ
	HFONT hBoxFont;				// 使用フォント
	SELINFO si;

	BOOL print;	// 印刷モードか
} PPVPAINTSTRUCT;

typedef struct {
	DWORD l, h;
} DblDWORD;

typedef struct {
	BYTE	*ptr;		// 共有メモリへのポインタ
	HLOCAL	mapH;		// 共有メモリハンドル
} VBYTE_LMEM;

typedef struct {
	int type;
	DWORD SupportTypeFlags; // 表示可能な形式 VO_type_xxx
	DWORD DModeType; // 現在の表示形式
	DWORD DModeBit; // 現在の表示形式をbitで表現(1bitのみ) DOCMODE_xxx ※DOCUMENT時にDModeTypeと値が異なることあり

	DWORD DocmodeType; // Docmodeの時の表示形式 DOCMODE_xxx
	BOOL clone; // 分割表示用のViewObjectなので、イメージを共有している
	struct {
		TCHAR name[VFPS]; // ファイル名
		BYTE *image; // ファイルの中身
		HGLOBAL mapH;	// メモリハンドル
		DWORD UseSize; // image で使用しているサイズ
		DWORD ImageSize; // メモリ確保時に使用する現在の確保メモリ
						// ※ FILEBUFMARGIN 分は含まれない

		int IsFile; // ファイルかそれ以外か
		BOOL memdata; // ファイルでないメモリ等から得たので、履歴記憶しない

		TCHAR typeinfo[VFPS]; // 種別名の表示テキスト
		TCHAR source[CMDLINESIZE]; // 元のファイル名
		#define SOURCEFROM_NONE 0
		#define SOURCEFROM_FILENAME 1 // ファイル名より
		#define SOURCEFROM_BASEURL_REL 2 // ファイル中 base hrefより(相対指定)
		#define SOURCEFROM_GETADDR 3 // ファイル中 GET/CONNECTより ※ BASEURL より後に判断される
		#define SOURCEFROM_BASEURL 4 // ファイル中 base hrefより
		int sourcefrom; // source の由来
		struct {
			BYTE *image; // ファイルの中身
			HGLOBAL mapH;	// メモリハンドル
			DWORD UseSize; // image で使用しているサイズ
			DWORD ImageSize; // メモリ確保時に使用する現在の確保メモリ
		} other;
	} file;
	struct {
		// 行毎の情報
		VMEM text;
		VMEM document;
	} text;
	struct {
		HPALETTE hPal; // パレット
		VBYTE_LMEM bits; // ビットマップイメージ
		BITMAPINFOHEADER *info; // ビットマップ情報(画像取得時)
		BITMAPINFOHEADER *ShowInfo; // ビットマップ情報(表示向けに加工済)
		BOOL AllocShowInfo; // ShowInfoがHeapAllocしているか
		HLOCAL info_hlocal; // info の確保ハンドル(Susie用)
		int transcolor; // 透明色
		int rotate; // 回転量 00:なし 01:右 10:上下 11:左
		int UseICC; // ICCプロファイルを使用するか？ -1 / ICM_xx
		struct {
			int current;
			int max;
			BOOL animate;
		} page;
		SIZE rawsize; // 表示サイズ(ボトムアップの補正済み)
		SIZE showsize; // 表示サイズ(アスペクト比・ボトムアップの補正済み)
		DIRECTXDEFINE(DXBMPCACHE *DxCache)
	} bitmap;
	struct {
		HENHMETAFILE handle;
	} eMetafile;
	ThSTRUCT memo; // コメント等のテキスト
} PPvViewObject;

#define PreFramePage_NoCheck	-1
#define PreFramePage_Off		-2
#define PreFramePage_BG			-3
#define PreFramePage_Restore	-4
typedef struct {
	int page;
	POINT offset;
	SIZE size;
	BYTE *bitmap;
	DWORD bitmapsize;
	BYTE fillcolor;
} PreFrameInfoStruct;
extern PreFrameInfoStruct PreFrameInfo;


#define HEXADRWIDTH 9
#define HEXNWIDTH 3
#define HEXSTRWIDTH 16
#define HEXNUMSWIDTH (HEXNWIDTH * HEXSTRWIDTH + 1 + 1)
#define HEXWIDTH (HEXADRWIDTH + HEXNUMSWIDTH + HEXSTRWIDTH)

ExternWinAPI(void, NotifyWinEvent, (DWORD event, HWND hwnd, LONG idObject, LONG idChild));

#define CV__defback 8
#define CV__deftext 15
#define CV__olddefback 0
#define CV__olddeftext 7
#define CV__highlight 16

//========= History option ID
#pragma pack(push, 4)
typedef struct{
	DWORD LowPart, HighPart;
} DDWORDST;

typedef struct {
	POINT pos;
	DDWORDST div;
} BookmarkInfo;
#pragma pack(pop)

// ヒストリの追加情報 ※内部処理はこの記載順に依存する
#define HISTOPTSIZE_HEADER		2	// (BYTE)length , (BYTE)HISTOPTID_xxx

#define HISTOPTID_OLDTAIL		101	// tailmode:前回の読み込み完了位置 (要先頭)
#define HISTOPTSIZE_OLDTAIL		(HISTOPTSIZE_HEADER + 4)	// (DWORD)y
#define HISTOPTSIZE_OLDTAIL_L	(HISTOPTSIZE_HEADER + 12)	// (DWORD)y, (DDWORDST)div

#define HISTOPTID_FileDIV		103	// FileDividePointer (要先頭)
#define HISTOPTSIZE_FileDIV		(HISTOPTSIZE_HEADER + 8)	// (DDWORDST)div

#define HISTOPTID_OTHERCP		102	// other cpの時のcode page
#define HISTOPTSIZE_OTHERCP		(HISTOPTSIZE_HEADER + 2)	// (WORD)codepage

#define HISTOPTID_WIDTH			104	// 表示桁数
#define HISTOPTSIZE_WIDTH		(HISTOPTSIZE_HEADER + 4)	// (int)width

#define HISTOPTID_CARET			105	// キャレットカーソルの位置
#define HISTOPTSIZE_CARET		(HISTOPTSIZE_HEADER + 8)	// (POINT)pos

#define HISTOPTID_BookmarkMin	0	// しおりの位置
#define HISTOPTID_BookmarkMax	63
#define HISTOPTSIZE_Bookmark	(HISTOPTSIZE_HEADER + 8)	// (POINT)pos
#define HISTOPTSIZE_Bookmark_L	(HISTOPTSIZE_HEADER + 16)	// (POINT)pos (DDWORDST)div

#define HISTOPT_VMODESHIFT 8
#define HISTOPT_TEXTOPT_CODESHIFT 16
#define HISTOPT_TEXTOPT_FLAGSHIFT 24
#define HISTOPT_IMAGEOPT_FLAGSHIFT 24

#define HISTOPT_TEXTOPT_MIME	B0	// VO_Tmime
#define HISTOPT_TEXTOPT_ESC		B1	// VO_Tesc
#define HISTOPT_TEXTOPT_TAGMASK	(B4 | B5)	// VO_Ttag(flag位置注意)
#define HISTOPT_TEXTOPT_TAGSHIFT	4	// VO_Ttag(flag位置注意)

//================================================== グローバル変数(PPV_GVAR.C)
#define GLOBALEXTERN
#include "PPV_GVAR.C"
#undef GLOBALEXTERN

#ifdef __cplusplus
}
#endif
