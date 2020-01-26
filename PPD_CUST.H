/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		カスタマイズ定義
-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
補助機能
	/ *...* /		コメントアウト
	-|				指定項目を削除
	+|				指定項目を強制的に追加
	=|				CS/CAでは追加、CUでは何もしない
-------------------------------------------------------------------------------
カスタマイズ 指定方法

+0 :セパレータの形式
		:		Dump
		=		一般
		,		実行 / 列挙用
		/		'='/','切替
+1 :カスタマイズ内容
		:n  	Dump		n:size		!n:all
		C		Color												4bytes
		c		Color(console)										2bytes
		K		Key													2bytes
		k		keys												2bytesz
		I		shift key											2bytes
		S[D]n	str			n:size(!=n asciiz)	D:dir check			n bytes/z
		sn		str(\n<->cr処理)	n:size(!=n asciiz)				n bytes/z
		Bm/n	bits		m:bits n:size(1-9) !=n:error			n bytes
		dn		(int)digits	n:size(1:byte 2:word 4:dword) !n:error	n bytes
		Dn		(Uint)digit	n:size(1:byte 2:word 4:dword) !n:error	n bytes
		Hn		hexs		n:size									n bytes
		V		ID/Version											5 bytes
		X		Exec(S,9fH) / keys(k,9eh)…先頭用
		x		Exec(S,9fH) / keys(k,9eh)…中間用
		M		複数行記載(改行は\n)
		m		複数行記載(改行は\0)
		E		(PPC)エントリの表示書式
		,,		省略可能 ,
-----------------------------------------------------------------------------*/
#define fT			B0	// table mode
#define fNoRelo	PPCUSTRESULT_RELOAD // (B1)カスタマイズ後、再読み込み処理をしなくてもよい
#define fReSta	PPCUSTRESULT_RESTART // (B2) カスタマイズ後、起動し直さないと反映されない
#define fB			B3	// (Data) 空欄でもカスタマイズする
#define fSwarn		B4	// (Data) 区切り文字が無いときも警告無し
#define fRC			B5	// (Data) A_colorを使用しない(A_color自体の定義用)
#define fSort		B6	// (Table) 書き出し時に並び替えを行う
#define fInternal	B7	// 常時非表示。内部保存用
#define fHide		B8	// 記録がなければ書出しない。オプション機能
#define fOld		B9	// 記録がなければ書出しない & 新規登録しない。旧項目用

#define fKeyFix		B10	// (Key) キーの処理(正規化する)
#define fK_HMenu	B11	// (Key) Hidden Menu の処理(4bytesに変換)
#define fK_ExtWild	B12	// (Key) 拡張子用ワイルドカード検出
#define fK_PathWild	B13	// (Key) パス用ワイルドカード検出
#define fK_sep		B14	// (Key) keyword が '--'なら追加書き込み(Menu,ToolBar)
#define fK_Mouse	B15	// (Key) マウス割当ての大文字変換

#define f_MENUBAR (fT | fK_sep | fB) // メニューバー
#define f_POPMENU (fT | fK_sep | fB | fNoRelo) // ポップアップメニュー
#define f_EXTRUN (fT | fK_ExtWild | fNoRelo) // ファイル判別実行
#define f_EXTSET (fT | fK_ExtWild | fNoRelo) // 拡張子・ワイルドカード別設定
#define f_KEY (fT | fKeyFix | fNoRelo) // キー割当て
#define f_MSGS (fT | fSort) // 言語別メッセージ
#define f_TOOLBAR (fT | fK_sep) // ツールバー

typedef struct {	// カスタマイズ列挙構造体 ---------------------------------
	const TCHAR *name;	// 変数名 \0:comment	\1:User Menu書込位置指定
	WORD flag;			// 型情報 f_
	const char *fmt;	// 内容形式
	const TCHAR *comment;	// コメント
} CLABEL;

typedef struct {	// Global value -------------------------------------------
	TCHAR	*SmesBuf;		// メッセージ格納領域先頭
	TCHAR	*SmesLim;		// メッセージ格納領域境界
	TCHAR	*Smes;			// メッセージ格納位置
	int		(USECDECL * Sure)(const TCHAR *message);

	int		Dnum;			// 解析行数
	int		Xmode;			/* カスタマイズの保存方法
								0(CS) 配列構造は以前の内容を一旦破棄する
								1(CA) 以前の配列の内容は維持する
								2(CU) 既に存在する項目は変更しない			*/
	int		XupdateTbl;		/* 配列のアップデート方法
								0	登録済み→指定がない限り登録せず
								!0	未登録  →全て登録する					*/
	WORD	reloadflag;

	TCHAR	CVer[64];		// カスタマイズ内容のバージョン
} PPCUSTSTRUCT;

extern const CLABEL Ctbl[];

// ppd_cust.c
extern void ErrorMes(PPCUSTSTRUCT *PCS, const TCHAR *mes);
extern void WarningMes(PPCUSTSTRUCT *PCS, const TCHAR *mes);
extern BYTE GetNumberWith(const TCHAR **line, BYTE def, DWORD max);

// ppd_cscd.c
extern COLORREF CS_color(TCHAR **linesrc, int flags);
extern int CD_color(TCHAR *dest, COLORREF color, int flags);
extern void CD_ppcdisp(PPCUSTSTRUCT *PCS, BYTE **binPtr, BYTE *binend);
extern BOOL CS_ppcdisp(PPCUSTSTRUCT *PCS, TCHAR **linePtr, BYTE **binPtr);

