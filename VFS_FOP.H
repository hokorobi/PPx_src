/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System			ファイル操作/ *fileコマンド
-----------------------------------------------------------------------------*/
#define SECTORSIZE 2048
#define RWSIZE (1 * 512 * SECTORSIZE)
#define filebuffersize	0x8000		// デフォルトファイルバッファの大きさ
#define MAXACTIONNAME 32

// 削除するとき問題になる属性
#define OPENERRORATTRIBUTES (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)

#define FOPERROR_GETLASTERROR (ERRORCODE)-1
#define FOPERROR_DIV (ERRORCODE)-2

#ifndef COPY_FILE_ALLOW_DECRYPTED_DESTINATION
#define COPY_FILE_ALLOW_DECRYPTED_DESTINATION 0x8
#endif

enum fop_states {
	FOP_UNKNOWN,	// 未使用
	FOP_READY,		// 開始前の設定
	FOP_BUSY,		// 処理中
	FOP_TOPAUSE,	// 中断希望
	FOP_PAUSE,		// 中断中
	FOP_TOBREAK,	// 中止希望
	FOP_COUNT,		// ファイル数計算中
	FOP_SKIP,		// 計算省略/待機省略希望
	FOP_WAIT,		// 待機中
	FOP_END			// 終了
};

#define FOPR_UNKNOWN 0	// リパース選択肢未選択

struct tagFOPSTRUCT;

#pragma pack(push,1)
typedef struct {			// カスタマイズ内容(X_fopt)
							// Same Name Action -------------------------------
	int		sameSW;			// (ALL&W)同名ファイルの対処方法が設定済みなら !0
	int		same;			// 同名ファイルの対処方法 FOPSAME_ 関連
							// Name Filter ------------------------------------
	int		chrcase;		// 1:大文字化 2:小文字化
	int		sfn;			// B0=1:8.3	// B1=1:全半正規化?
	int		delspc;			// !0:空白削除
	DWORD	filter;			// VFSFOP_FILTER_
							// Attributes -------------------------------------
	DWORD	AtrMask;		// And 指定用
	DWORD	AtrFlag;		// Or 指定用
							// Etc --------------------------------------------
	int		mode;			// FOPMODE_ 関連
	DWORD	flags;			// VFSFOP_OPTFLAG_ 関連
	char	aall;			// !0:auto all check
	BYTE	firstsheet;		// 始めに開くシート FOPTAB_
	char	useburst;		// バーストモード
	BYTE	divide_unit;	// 分割大きさの単位
	DWORD	divide_num;		// 分割大きさ
	TCHAR	str[VFPS];		// ASCIIZs  1st:Dest 2nd:Rename 3rt:S.num
} VFSFOP_OPTIONS;
#pragma pack(pop)

enum { // same の内容
	FOPSAME_NEWDATE = 0,
	FOPSAME_RENAME,
	FOPSAME_OVERWRITE,
	FOPSAME_SKIP,
	FOPSAME_ARCHIVE,
	FOPSAME_ADDNUMBER,
	FOPSAME_APPEND,
	FOPSAME_SIZE,
	FOPSAME_MAX
};
enum { // mode の内容
	FOPMODE_MOVE = 0,
	FOPMODE_COPY,
	FOPMODE_MIRROR,
	FOPMODE_SHORTCUT,
	FOPMODE_LINK,
	FOPMODE_DELETE,
	FOPMODE_UNDO,
	FOPMODE_SYMLINK,
	FOPMODE_MAX
};
// flags の内容
#define VFSFOP_OPTFLAG_SKIPERROR	B0	// 軽微エラーの無視
#define VFSFOP_OPTFLAG_FLATMODE		B1	// 平坦化
#define VFSFOP_OPTFLAG_AUTOROFF		B2	// ReadOnlyの自動解除
#define VFSFOP_OPTFLAG_BACKUPOLD	B3	// 削除・上書きファイルの保存

#define VFSFOP_OPTFLAG_UNDOLOG		B4	// Undo用ログを出力
#define VFSFOP_OPTFLAG_LOGWINDOW	B5	// ログウィンドウに経過表示
#define VFSFOP_OPTFLAG_LOGRWAIT		B6	// ログ表示したときに結果表示待機しない
#define VFSFOP_OPTFLAG_QSTART		B7	// 順序待ちしない

#define VFSFOP_OPTFLAG_NOCOUNT		B8	// カウントしない
#define VFSFOP_OPTFLAG_SECURITY		B9	// セキュリティコピー
#define VFSFOP_OPTFLAG_SACL			B10	// 監査コピー
#define VFSFOP_OPTFLAG_LOWPRI		B11	// 優先順位を低くする

#define VFSFOP_OPTFLAG_NOWCREATEDIR	B12	// ディレクトリ作成警告無し
#define VFSFOP_OPTFLAG_PREVENTSLEEP	B13	// スリープ抑制
#define VFSFOP_OPTFLAG_ALLOWDECRYPT	B14	// 暗号化非対応ドライブへの暗号化ファイルのコピーを許可
#define VFSFOP_OPTFLAG_NOWDELEXIST	B15	// サブディレクトリ内の存在警告なし

#define VFSFOP_OPTFLAG_KEEPDIR		B16	// 作成元の階層を再現(srcがListFileの時)
#define VFSFOP_OPTFLAG_JOINBATCH	B17	// 分割時に元に戻す為のバッチを生成
#define VFSFOP_OPTFLAG_ADDNUMDEST	B18	// 上書きファイル元の名前変更
#define VFSFOP_OPTFLAG_NOFIRSTEXIST	B19	// カウント時に存在チェックをしない

#define VFSFOP_OPTFLAG_SYMCOPY_SHIFT	20
#define VFSFOP_OPTFLAG_SYMCOPY_SYM	B20	// シンボリックリンクのリンク コピー
#define VFSFOP_OPTFLAG_SYMCOPY_FILE	B21	// シンボリックリンクのファイル コピー
#define VFSFOP_OPTFLAG_SYMDEL_SHIFT	22
#define VFSFOP_OPTFLAG_SYMDEL_SYM	B22	// シンボリックリンクを解除
#define VFSFOP_OPTFLAG_SYMDEL_FILE	B23	// シンボリックリンク内ファイルを削除

#define VFSFOP_OPTFLAG_WAIT_CLOSE	B24	// 移動時使用終了を待つ

//#define VFSFOP_OPTFLAG_DELDELAY		// 未 Delaymode
//#define VFSFOP_OPTFLAG_SUBSTREAM		// 未 サブストリームも必ず処理
//#define VFSFOP_OPTFLAG_NOSUBSTREAM	// 未 サブストリームも必ず処理
// 未 FLATMODE/BACKUPOLD dir名付加

// filter の内容
#define VFSFOP_FILTER_DELNUM		B0
#define VFSFOP_FILTER_NOFILEFILTER	B1
#define VFSFOP_FILTER_NODIRFILTER	B2
#define VFSFOP_FILTER_EXTRACTNAME	B3

#define SECURITY_FLAG_NONE 0
#define SECURITY_FLAG_SCOPY (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION)
#define SECURITY_FLAG_SACL (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION)

typedef struct {
	DWORD l, h;
} DDWORDS;

struct ProgressInfo {
	DWORD mark, markall;		// 処理済マーク数、全マーク数
	DWORD donefiles, filesall;	// 処理済ファイル数、全ファイル数
	DDWORDS donesize, allsize;	// 処理済バイト数、全バイト数
	DWORD LEskips, EXskips, errors; // 軽微エラー無視によるスキップ数、上書き無しスキップ数、エラー数
	DWORD busybar;			// ワーキングカウンタ
	DWORD count_exists; // 数算出時に検出した同名ファイル数
	DWORD exists[3]; // 比較結果 new date,same date,old date
};

#define PROGRESS_INTERVAL_TICK 100
#define CAPTION_INTERVAL_TICK 700
struct _ProgressWndInfo {
	HWND hWnd; // FS->hDlg 相当
	HWND hSrcNameWnd;  // IDS_FOP_SRCNAME の hWnd
	HWND hProgressWnd; // IDS_FOP_PROGRESS の hWnd
	const TCHAR *srcpath; // Source 欄に表示する文字列
	struct tagFOPSTRUCT *FS;

	DWORD ProgTick; // プログレス表示計算用Tick(Source 欄表示兼用)
	DWORD CapTick;  // キャプション更新用Tick
	struct ProgressInfo info;	// 状況表示関係(OperationStart開始時に0 fill)
	int nowper;
};

struct FopOption {
	TCHAR source[VFPS];
	const TCHAR *files;
	BOOL AllocFiles; // files を内部で生成した
	int dtype;
	DWORD fopflags; // VFSFILEOPERATION の VFSFOP_

	HWND hReturnWnd;

	VFSFOP_OPTIONS fop;
	TCHAR action[MAXACTIONNAME];	// Action Name
	TCHAR rename[MAX_PATH];	// Rename
	TCHAR renum[12];	// S.num
	int OnDriveMove;	// 同名ドライブで移動指定なら !0
	int burst;		// バーストモードなら真
	DWORD erroraction;		// エラー時の対処方法
	DWORD errorretrycount;	// エラー時の再試行回数
	BYTE *CopyBuf;	// コピー用バッファ
	DWORD CopyBufSize;
	RXPREPLACESTRING *rexps; // 名前変更用正規表現
	SECURITY_INFORMATION security;
	TCHAR *compcmd;
};

enum foptab { // 現在タブ FOPTAB_ ※-1は未設定
	FOPTAB_GENERAL = 0, FOPTAB_RENAME, FOPTAB_OPTION, FOPTAB_NONE = -1
};
struct pageinfo {
	HWND *hWnds; // ページ内のコントロールのウィンドウハンドル
	enum foptab showID;
};

typedef struct tagFOPSTRUCT{
	HWND hDlg;	// メインダイアログ
	HWND hDstED;	// コピー先のテキストボックス
	HWND hEWnd;	// エラーログ

	UINT Command;	// 同名処理時のボタン操作に使用
	int errorcount;
	BOOL Cancel;	// キャンセルの指定をするために使用
	BOOL testmode; // テスト処理かどうか
	BOOL flat;
	BOOL renamemode;
	BOOL BackupDirCreated; // deleted$ が作成済みか
	BOOL FixDest;	// 処理先が固定
	BOOL NoAutoClose; // ログ確認のため、終了後もダイアログを閉じない
	BOOL hide; // 普段は隠れているか
	BOOL DestroyWait; // WM_DESTROY で待機をするか(Fop_ShellNameSpaceで、メディアプレイヤーへ処理を行うとき、メッセージポンプをしばらく動かさないとうまく動作しないための対策)

	int reparsemode; // リパースポイントの扱い

	enum fop_states state;	// 動作状況

	struct FopOption opt;
	struct _ProgressWndInfo progs;	// 状況表示関係
	DELETESTATUS DelStat;
	PPXAPPINFO DelInfo;

	HWND *hLogWnd;
	HANDLE hUndoLogFile; // UNDOログ
	HANDLE hStartMutex; // コピー開始待機用のMutex
	PPXAPPINFO *info;
	struct pageinfo page;
	TCHAR DestDir[VFPS];
	int EnableSetting, EnableSameSetting; // グレーにするコントロールの状態
	FN_REGEXP maskFn; // ファイル名マスク
	DWORD maskFnFlags;
} FOPSTRUCT;

typedef struct {
	VFSFILEOPERATION *fileop;
	FOPSTRUCT *FS;
} FILEOPERATIONDLGBOXINITPARAMS;

#define CAPTION "File Operation Options"
#define STRLOGDEST T(" ->")				// 処理先
#define STRLOGMAKEDIR T("MakeDir")				// ディレクトリ作成
#define STRLOGDELETE T("Delete")				// エントリ削除(Undo不要->bkで)
#define STRLOGSKIP T("Skip")					// 未処理(Undo不要)

#define WINY_SETT	123 // 110
#define WINY_STAT	135
#define WINY_FULL	188
#define WINY_MAX	(144+41-1)

enum foplogtypes { LOG_COPY = 0,LOG_COPYOVERWRITE,LOG_MOVE,LOG_MOVEOVERWRITE,
	LOG_APPEND,LOG_BACKUP,LOG_LINK,LOG_SKIP,LOG_DIR,LOG_DIR_DELETE,
	LOG_DELETE,LOG_STRING,LOG_MAKEDIR
};

#define CreateFile_OpenSource(SRC,FILEMODE) CreateFileL(SRC,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILEMODE,NULL)

void FopLog(FOPSTRUCT *FS,const TCHAR *src,const TCHAR *dst,enum foplogtypes type);

//----------------------------------------------------------------- vfs_fop.c
extern BOOL OperationStart(FOPSTRUCT *FS);
extern void FWriteLogMsg(FOPSTRUCT *FS,const TCHAR *mes);
extern void FWriteErrorLogs(FOPSTRUCT *FS,const TCHAR *mes,const TCHAR *type,ERRORCODE error);
extern void DisplaySrcNameNow(FOPSTRUCT *FS);
extern void TinyDisplayProgress(FOPSTRUCT *FS);
extern void FullDisplayProgress(struct _ProgressWndInfo *Progs,LARGE_INTEGER TotalTransSize,LARGE_INTEGER TotalSize);

#define ACTION_CREATE NO_ERROR
#define ACTION_SKIP		MAX32
#define ACTION_RETRY	(MAX32-1)
#define ACTION_APPEND	(MAX32-2)
#define ACTION_OVERWRITE (MAX32-3)
ERRORCODE SameNameAction(FOPSTRUCT *FS,HANDLE dstH,BY_HANDLE_FILE_INFORMATION *srcfinfo,BY_HANDLE_FILE_INFORMATION *dstfinfo,const TCHAR *src,TCHAR *dst);

DWORD CALLBACK CopyProgress(LARGE_INTEGER TotalSize,LARGE_INTEGER TotalTransSize,LARGE_INTEGER StreamSize,LARGE_INTEGER StreamTransSize,DWORD StreamNumber,DWORD reason,HANDLE hSourceFile,HANDLE hDestinationFile,LPVOID lpData);

extern ERRORCODE TestDest(FOPSTRUCT *FS,const TCHAR *src,TCHAR *dst);
extern void FWriteLog(HWND hEWnd,const TCHAR *message);
extern void CreateFWriteLogWindow(FOPSTRUCT *FS);
extern void EndingOperation(FOPSTRUCT *FS);
extern HWND FopGetMsgParentWnd(FOPSTRUCT *FS);

extern const TCHAR STR_FOP[];
extern const TCHAR STR_FOPWARN[];
extern const TCHAR STR_FOPUNDOLOG[];

extern const TCHAR STR_FOPOK[];
extern const TCHAR STR_FOPSTART[];
extern const TCHAR STR_FOPCONTINUE[];

extern const TCHAR STR_FOPCANCEL[];
extern const TCHAR STR_FOPCLOSE[];

extern const TCHAR STR_FOPPAUSE[];

extern const TCHAR StrLogBackup[];
extern const TCHAR StrLogReplace[];
extern const TCHAR StrLogMove[];
extern const TCHAR StrLogOverwrite[];
extern const TCHAR StrLogCopy[];
extern const TCHAR StrLogTest[];
extern const TCHAR StrLogExist[];
extern const TCHAR StrLogAppend[];

extern BOOL USEFASTCALL CheckSaveDrive(struct FopOption *opt, _In_ const TCHAR *src, _In_ const TCHAR *dst);
extern BOOL Fop_ShellNameSpace(FOPSTRUCT *FS,const TCHAR *srcDIR,const TCHAR *dstDIR);
extern ERRORCODE RenameDestFile(FOPSTRUCT *FS,const TCHAR *dst,BOOL addOldStr);

//----------------------------------------------------------------- vfs_ocd.c
extern ERRORCODE DlgCopyDir(FOPSTRUCT *FS,const TCHAR *src,TCHAR *dst,DWORD srcatr);

//----------------------------------------------------------------- vfs_ocf.c
extern ERRORCODE DlgCopyFile(FOPSTRUCT *FS,const TCHAR *src,TCHAR *dst,DWORD srcatr);
extern ERRORCODE CheckEntryMask(FOPSTRUCT *FS,const TCHAR *src,DWORD srcatr);

//----------------------------------------------------------------- vfs_odel.c
extern DWORD_PTR USECDECL FopDeleteInfoFunc(PPXAPPINFO *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr);

//----------------------------------------------------------------- vfs_odlg.c
extern void Enables(FOPSTRUCT *FS,int setting,int samesetting);
extern void USEFASTCALL PeekMessageLoopSub(FOPSTRUCT *FS);
extern void PeekMessageLoop(FOPSTRUCT *FS);
extern void SetWindowY(FOPSTRUCT *FS,int y);
extern void DispAttr(HWND hDlg,UINT cID,BY_HANDLE_FILE_INFORMATION *src,BY_HANDLE_FILE_INFORMATION *dst);
extern void SaveControlData(FOPSTRUCT *FS);
extern INT_PTR CALLBACK FileOperationDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);
extern void SetFopTab(FOPSTRUCT *FS,int id);
extern void SetStateOnCaption(FOPSTRUCT *FS,const TCHAR *text);

//----------------------------------------------------------------- vfs_oflt.c
extern ERRORCODE LFNfilter(struct FopOption *opt,TCHAR *src);

//----------------------------------------------------------------- vfs_oimg.c
extern BOOL OperationStartListFile(FOPSTRUCT *FS,const TCHAR *srcDIR,TCHAR *dstDIR);
extern int OperationStartToFile(FOPSTRUCT *FS,const TCHAR *srcDIR,TCHAR *dstDIR);
extern void ImgExtract(FOPSTRUCT *FS,const TCHAR *srcDIR,const TCHAR *dstDIR);

//----------------------------------------------------------------- vfs_olnk.c
#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
#define SYMBOLIC_LINK_FLAG_DIRECTORY (0x1)
#endif
TypedefWinAPI(BOOL,CreateHardLink,
		(LPCTSTR lpFileName,LPCTSTR lpExistingFileName,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes));

extern ERRORCODE FOPMakeShortCut(const TCHAR *src,TCHAR *dst,BOOL directory,BOOL test);
extern ERRORCODE FopCreateSymlink(const TCHAR *FileName,const TCHAR *ExistingFileName,DWORD flags);
extern ValueWinAPI(CreateHardLink);

//----------------------------------------------------------------- vfs_osub.c
extern ERRORCODE CopySecurity(FOPSTRUCT *FS,const TCHAR *src,const TCHAR *dst);
extern ERRORCODE BackupFile(FOPSTRUCT *FS,const TCHAR *src);
extern void SetFopLowPriority(FOPSTRUCT *FS);
extern void FopCompareFile(HWND hWnd,const TCHAR *src,const TCHAR *dest);
extern BOOL UndoCommand(FOPSTRUCT *FS);
extern void GetUndoLogFileName(TCHAR *name);
extern ERRORCODE FileOperationReparse(FOPSTRUCT *FS,const TCHAR *src,TCHAR *dst,DWORD attr);
extern int FopOperationMsgBox(FOPSTRUCT *FS,const TCHAR *msg,const TCHAR *title,UINT flags);

typedef struct {
	PPXAPPINFO info;
	const TCHAR *filename;
} FILENAMEINFOSTRUCT;
extern DWORD_PTR USECDECL FilenameInfoFunc(FILENAMEINFOSTRUCT *linfo,DWORD cmdID,PPXAPPINFOUNION *uptr);

