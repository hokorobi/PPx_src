/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System

	※ shlobj.h を include しないと、一部の定義が無効になる
-----------------------------------------------------------------------------*/
#ifdef ONPPXDLL
#define ONVFSDLL
#endif
#if !NODLL && !defined(WINEGCC)
	#ifdef ONVFSDLL
		#define VFSDLL __declspec(dllexport)
	#else
		#define VFSDLL __declspec(dllimport)
	#endif
#else
	#define VFSDLL
#endif

#if 0 || defined(WINEGCC)
#define USESLASHPATH
#endif

#ifdef __cplusplus
extern "C" { // Assume C declarations for C++
#endif // __cplusplus
//--------------------------------------------------------- 一般定義
#define VFS_check_size		0x11000	// ファイル形式判別に使うヘッダ部分のサイズ
#define VFS_TINYREADSIZE	0x800 // EXEヘッダの判断に必要なサイズ
#define VFS_ARCREADSIZE		0x2000 // EXE書庫内の書庫の判別に必要なサイズ
#define VFS_GENERALREADSIZE	0x4000 // 一般の判別に必要なサイズ

								// ディレクトリの種類 -------------------------
#define VFSDT_UNKNOWN	0			// 不明
#define VFSDT_PATH		1			// path(drive/unc)
#define VFSDT_DLIST		2			// Drive list
#define VFSDT_SHN		3			// SHN/Shell's Namespace
#define VFSDT_LFILE		4			// list file
#define VFSDT_FATIMG	5			// FAT disk image file
#define VFSDT_FATDISK	6			// FAT disk raw access
#define VFSDT_CDIMG		7			// CD-ROM image file
#define VFSDT_CDDISK	8			// CD-ROM raw access
#define VFSDT_STREAM	9			// xxx:xxxx
#define VFSDT_SLASH		10			// posix系 /
#define VFSDT_AUXOP		11			// Auxiliary Operation
#define VFSDT_FTP		21			// FTP
#define VFSDT_CABFOLDER	61			// cab folder (cabview.dll)
#define VFSDT_LZHFOLDER	62			// lzh folder (lzhfldr2.dll)
#define VFSDT_ZIPFOLDER	63			// zip folder (zipflder.dll)
#define VFSDT_SUSIE		64			// SUSIE
#define VFSDT_HTTP		80			// HTTP
#define VFSDT_UN		96			// UNxxx

#define LHEADER		";ListFile\r\n"
#define LHEADERU	";\0L\0i\0s\0t\0F\0i\0l\0e\0\r\0\n" // 最後の\0が省略

#define X_wdel_default {1, 0, 1, 1}
//--------------------------------------------------------- 旧SDKにない定義
#ifndef COPY_FILE_FAIL_IF_EXISTS
#define COPY_FILE_FAIL_IF_EXISTS        0x00000001
#define COPY_FILE_RESTARTABLE           0x00000002
#define COPY_FILE_OPEN_SOURCE_FOR_WRITE 0x00000004
#define CALLBACK_CHUNK_FINISHED         0x00000000
#define CALLBACK_STREAM_SWITCH          0x00000001

#define PROGRESS_CONTINUE   0
#define PROGRESS_CANCEL     1
#define PROGRESS_STOP       2
#define PROGRESS_QUIET      3
#endif

#ifndef CopyFileEx
typedef DWORD (WINAPI *LPPROGRESS_ROUTINE)(
	LARGE_INTEGER TotalFileSize,
	LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER StreamSize,
	LARGE_INTEGER StreamBytesTransferred,
	DWORD dwStreamNumber,
	DWORD dwCallbackReason,
	HANDLE hSourceFile,
	HANDLE hDestinationFile,
	LPVOID lpData OPTIONAL);
#endif

#define VFSFT_TYPE		B0	// 種類名 ":type"
#define VFSFT_TYPETEXT	B1	// 種類名詳細 "type name"
#define VFSFT_EXT		B2	// 種類名に該当する拡張子 "EXT"
#define VFSFT_INFO		B3	// ファイルから取得した情報 "rect=640x480\r\n"
#define VFSFT_COMMENT	B4	// ファイルから取得したコメント "comment text"
#define VFSFT_STRICT	B30	// 種類が確定しない場合はエラーにする(複合など)

#define VFSFILETYPE_TYPESIZE	16
#define VFSFILETYPE_TEXTSIZE	80
#define VFSFILETYPE_EXTSIZE		16
typedef struct {
	DWORD flags;		// どの情報を取得するかの指定
	DWORD dtype;		// PPv 用情報(LOWORD:DISPT_xx, HIWORD:VD_xx)
	TCHAR type[VFSFILETYPE_TYPESIZE];		// VFSFT_TYPE の結果
	TCHAR typetext[VFSFILETYPE_TEXTSIZE];	// VFSFT_TYPETEXT の結果
	TCHAR ext[VFSFILETYPE_EXTSIZE];			// VFSFT_EXT の結果
	TCHAR *info;		// VFSFT_INFO の結果、HeapFree(GetProcessHeap(…で解放
	TCHAR *comment;		// VFSFT_COMMENT の結果、HeapFree(GetProcessHe…で解放
} VFSFILETYPE;
/*-----------------------------------------------------------------------------
	指定ファイル／メモリイメージの種別を判別する

	FileName	ファイル名
	header		(size != 0)メモリイメージの先頭
	size		(=0)FileNameを示すファイルを判別
				(!0)FileNameとheaderを元に判別
	result		結果を書きこむ構造体
-----------------------------------------------------------------------------*/
extern VFSDLL ERRORCODE PPXAPI VFSGetFileType(const TCHAR *FileName, const char *image, SIZE32_T imagesize, VFSFILETYPE *result);
extern VFSDLL BOOL PPXAPI VFSCheckFileByExt(const TCHAR *FileName, TCHAR *result);
extern VFSDLL DWORD PPXAPI ReadFileHeader(HANDLE hFile, BYTE *header, SIZE32_T headersize);

extern VFSDLL TCHAR * PPXAPI GetDriveName(_Out_writes_z_(VFPS) TCHAR *dst, _In_z_ const TCHAR *src);
extern VFSDLL ERRORCODE PPXAPI CreateJunction(const TCHAR *FileName, const TCHAR *ExistingFileName, SECURITY_ATTRIBUTES *sa);
/*=============================================================================
=============================================== 極長パス対応Win32API互換関数 */
extern VFSDLL HANDLE PPXAPI CreateFileL(const TCHAR *FileName, DWORD Access,
		DWORD ShareMode, LPSECURITY_ATTRIBUTES SecurityAttributes,
		DWORD CreationDisposition, DWORD FlagsAndAttributes,
		HANDLE hTemplateFile);

extern VFSDLL BOOL PPXAPI DeleteFileL(const TCHAR *FileName);
extern VFSDLL BOOL PPXAPI CreateDirectoryL(const TCHAR *FileName,
		LPSECURITY_ATTRIBUTES sa);
extern VFSDLL BOOL PPXAPI RemoveDirectoryL(const TCHAR *FileName);
extern VFSDLL HANDLE PPXAPI FindFirstFileL(_In_z_ const TCHAR *dir, _Out_ WIN32_FIND_DATA *findfile);
extern VFSDLL DWORD PPXAPI GetFileAttributesL(const TCHAR *FileName);
extern VFSDLL BOOL PPXAPI SetFileAttributesL(const TCHAR *FileName, DWORD attributes);

// WIN32_FIND_DATAのcFileName,cAlternateFileName以外の情報を取得する
typedef WIN32_FIND_DATA FILE_STAT_DATA;
BOOL WINAPI GetFileSTAT(const TCHAR *lpFileName, FILE_STAT_DATA *FindFileData);

extern VFSDLL BOOL PPXAPI MoveFileWithProgressL(const TCHAR *ExistingFileName,
		const TCHAR *NewFileName,
		LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, DWORD dwFlags);
#define MoveFileL(ExistingFileName, NewFileName) MoveFileWithProgressL((ExistingFileName), (NewFileName), NULL, NULL, 0)

extern VFSDLL BOOL PPXAPI CopyFileExL(const TCHAR *ExistingFileName,
		const TCHAR *NewFileName, LPPROGRESS_ROUTINE ProgressRoutine,
		LPVOID Data, LPBOOL Cancel, DWORD CopyFlags);

/*=============================================================================
==================================================================  一般関数 */
/*-----------------------------------------------------------------------------
	VFS の Plug Dll 機能を有効にし、関連 DLL を読み込む。何度も呼び出せるが、
	その回数だけ、VFSOff を使用する必要がある。

	mode	有効にする機能（下記 define を参照）。複数ある時は"|"指定。
-----------------------------------------------------------------------------*/
#define VFS_BMP			B0			// bitmap
#define VFS_DIRECTORY	B1			// directory
#define VFS_ALL			B31			// 無効にしたPlugDLLを読み込む
extern VFSDLL void PPXAPI VFSOn(int mode);

/*-----------------------------------------------------------------------------
	VFS の Plug - in 機能を無効にする。最後に呼ばれた時、Plugin DLL を解放する
-----------------------------------------------------------------------------*/
extern VFSDLL void PPXAPI VFSOff(void);

//-------------------------------------- 指定URLのイメージを取得
extern VFSDLL BOOL PPXAPI GetImageByHttp(const TCHAR *urladr, ThSTRUCT *th);
extern VFSDLL HRESULT PPXAPI MakeShortCut(const TCHAR *LinkedFile, const TCHAR *LinkFname, const TCHAR *DestPath);

extern VFSDLL ERRORCODE PPXAPI VFSGetArchivefileImage(HWND hWnd, HANDLE hFile, const TCHAR *ArchiveName, const TCHAR *EntryName, SIZE32_T *sizeL, SIZE32_T *sizeH, HANDLE *hMap, BYTE **mem);

// ↓アラインメントがおかしいことがあったので固定にした on X64
#pragma pack(push, 4)
typedef struct {
	WIN32_FIND_DATA ff;
	TCHAR dest[VFPS];
	LPPROGRESS_ROUTINE Progress;
	LPVOID lpData;
	BOOL *Cancel;
} IMAGEGETEXINFO;
#pragma pack(pop)
/*-----------------------------------------------------------------------------
	FindFirstFile系のVFS版。使い方はFindFirstFile系と同じなので省略
-----------------------------------------------------------------------------*/
#define FILE_ATTRIBUTEX_LF_MARK	B25	// ListFileでマーク有り。PPx独自拡張
#define FILE_ATTRIBUTEX_LF_COMMENT	B26	// ListFileでコメント有り。PPx独自拡張
#define FILE_ATTRIBUTEX_FOLDER	B27	// ファイルフォルダなどを表す。PPx独自拡張
#define FILE_ATTRIBUTEX_VIRTUAL	B28	// 仮想エントリを表す。PPx独自拡張
		// SHN で WIN32_FIND_DATA でないエントリ。
#define FILE_ATTRIBUTEX_NODETAIL	B29	// ファイル名以外の情報がない。独自拡張
#define FILE_ATTRIBUTEX_MESSAGE (FILE_ATTRIBUTE_LABEL | FILE_ATTRIBUTE_TEMPORARY)	// Listfile内限定。メッセージエントリ

extern VFSDLL HANDLE PPXAPI VFSFindFirst(const TCHAR *dir, WIN32_FIND_DATA *findfile);
extern VFSDLL BOOL PPXAPI VFSFindNext(HANDLE hFF, WIN32_FIND_DATA *findfile);
extern VFSDLL BOOL PPXAPI VFSFindClose(HANDLE hFF);

#define FINDOPTIONDATA_LONGNAME	1
#define FINDOPTIONDATA_COMMENT	2
extern VFSDLL BOOL PPXAPI VFSFindOptionData(HANDLE hFF, DWORD optionID, void *data);

// VFSFind 中に読み込み対象の詳細情報を取得する
// ftable
// VFSDT_LFILE	ListFileテキストへのポインタ
// VFSDT_SUSIE	SUSIE_FINFO へのポインタ
// VFSDT_UN		UN_DLL へのポインタ
// type は VFSGETFFINFO_TYPESIZE の大きさが必要
#define VFSFFTYPENAME_STREAM	T("stream")
#define VFSFFTYPENAME_HTML		T("HTML")
#define VFSFFTYPENAME_FAT		T("FAT")
#define VFSFFTYPENAME_CD		T("CD")
#define VFSFFTYPENAME_ZIPfolder	T("ZIPfdr")
#define VFSFFTYPENAME_LHAfolder	T("LHAfdr")
#define VFSFFTYPENAME_CABfolder	T("CABfdr")
#define VFSFFTYPENAME_DriveList	T("Drives")
#define VFSFFTYPENAME_FTP		T("FTP")
#define VFSFFTYPENAME_root		T("/")
#define VFSFFTYPENAME_Network	T("Netwrk")
#define VFSFFTYPENAME_LongPath	T("Xlong")
#define VFSFFTYPENAME_SHN		T("ShellN")
#define VFSFFTYPENAME_desktopSHN	T("desktopS ")
#define VFSFFTYPENAME_directorySHN	T("dir-SHN ")
#define VFSFFTYPENAME_vfolderSHN	T("vfolderS ")
#define VFSFFTYPENAME_storageSHN	T("storageS ")
#define VFSFFTYPENAME_exFAT 	T("exFAT")
#define VFSFFTYPENAME_NTFS		T("NTFS")

extern VFSDLL void PPXAPI VFSGetFFInfo(HANDLE hFF, int *mode, TCHAR *type, void **dt_opt);

// VFSFindFirst の対象であるかを調べる
// filename は変更する場合もある

//hsize に付加。fnctable に HMENU を入れる
#define VFSCHKDIR_GETDIRMENU B31		// dir移動用
#define VFSCHKDIR_GETEXTRACTMENU B30	// 展開用
#define VFSCHK_MENUID 0xe200 // VFSCheckDir / VFSCheckImage で追加する先頭 ID
extern VFSDLL int PPXAPI VFSCheckDir(const TCHAR *filename, BYTE *header, SIZE32_T headersize, void **dt_opt);
extern VFSDLL int PPXAPI VFSCheckImage(const TCHAR *filename, BYTE *header, SIZE32_T headersize, HMENU hPopupMenu);

// 各種プラグインを使ってビットマップを取得する
extern VFSDLL BOOL PPXAPI VFSGetDib(const TCHAR *filename, void *image, SIZE32_T sizeL, TCHAR *type, HANDLE *Info, HANDLE *Bm);
extern VFSDLL int PPXAPI VFSGetDibDelay(const TCHAR *filename, void *image, SIZE32_T sizeL, TCHAR *type, HANDLE *Info, HANDLE *Bm, DWORD (*AllReadProc)(void));
// 指定したドライブのSHN表示名を取得する ※フロッピーアクセスしない
extern VFSDLL void PPXAPI GetDriveNameTitle(TCHAR *buf, TCHAR drive);

/*=============================================================================
======================================================== Path 文字列操作関係 */
/*-----------------------------------------------------------------------------
	拡張子の位置を取得する

	※階層指定「xx.xxx\yyy」の場合は、
		(VFSFindLastEntry(src) - src) + FindExtSeparator(VFSFindLastEntry(src))
		として検索すること

	src:	検索文字列へのポインタ
	->		src に対する相対位置("."を示す)。拡張子が無い場合は、src の末端
-----------------------------------------------------------------------------*/
extern VFSDLL int PPXAPI FindExtSeparator(const TCHAR *src);

/*-----------------------------------------------------------------------------
	次のパス区切りを取得する		※パス区切りは「\」のみ

	vfp:	検索文字列へのポインタ、これが「\」を示すと戻り値が vfp になる
	->		「\」を示す。NULL なら失敗
-----------------------------------------------------------------------------*/
extern VFSDLL TCHAR * PPXAPI FindPathSeparator(const TCHAR *src);

/*-----------------------------------------------------------------------------
	最下層のエントリを示すポインタを入手する

	src:	検索文字列へのポインタ
	->		下に示す位置を返す

	a:\dir1\dir2\dir3		entryonly
	   ^   ^    ^			^
-----------------------------------------------------------------------------*/
extern VFSDLL TCHAR * PPXAPI VFSFindLastEntry(const TCHAR *src);

/*-----------------------------------------------------------------------------
	ドライブの種類を調べる

	vfp:	検索文字列へのポインタ
	mode:	戻値としてドライブの種類を格納するためのポインタ。NULL指定可
				0<	SHN。符号反転して-1 で SHGetSpecialFolderLocation 用の値に
				正なら、下記 define の VFSPT_xxx に該当する

	force:	戻値として種類の強制指定を格納するためのポインタ。NULL指定可
				下記 define の VFSPTF_xxx に該当する

	->		ドライブの種類の次のポインタ(Root 指定の「\」があればそこを示す)
			NULLなら失敗

		UNC					GNC				URL
			\\xxx\yyy\...		@:\xxx...		xxxx://xxxxx/
			  ^					  ^							^
-----------------------------------------------------------------------------*/
#define VFSPT_SHN_DESK	-1		// -1 以下ならば、Explorer の特殊パス
#define VFSPT_UNKNOWN	0		// VFSGetDriveType 内部のみでこの状態になる
#define VFSPT_DRIVE		1		// 'x:'
#define VFSPT_DRIVELIST	2		// ':'
#define VFSPT_UNC		3		// '\\'
#define VFSPT_SLASH		4		// '/'
#define VFSPT_RAWDISK	6		// disk raw access
#define VFSPT_FILESCHEME 20		// 'file://'
#define VFSPT_FTP		21		// 'ftp://'
#define VFSPT_SHELLSCHEME 22	// 'shell://'
#define VFSPT_AUXOP		23		// 'aux:'
#define VFSPT_HTTP		80		// 'http://'

#define VFSPTF_AUTO		0		//	:できるだけ DriveName/UNC を使用する
#define VFSPTF_FS		1		// -:常に UNC/DN を使用する
#define VFSPTF_SHN		2		// +:常に Shell's Namespace を使用する
_Success_(return != NULL)
extern VFSDLL TCHAR * PPXAPI VFSGetDriveType(_In_z_ const TCHAR *vfp, _Out_opt_ int *resultmode, _Out_opt_ int *force);

/*-----------------------------------------------------------------------------
	src と fname の間に「\」 が入った文字列を生成する
	src の末端に「\」があってもなくてもよい

	dst:	生成した文字列を入れるポインタ。ここが NULL なら src に結果が入る
	src:	前側に配置する文字列。dst が NULL なら、生成物を入れるバッファ
	fname:	後側に配置する文字列
	->		void
-----------------------------------------------------------------------------*/
extern VFSDLL void PPXAPI CatPath(_Out_writes_opt_z_(VFPS) TCHAR *dst, TCHAR *src, _In_z_ const TCHAR *fname);

/*-----------------------------------------------------------------------------
	src を正規化したパスに変換する

	dst:	生成した文字列を入れるポインタ。ここが NULL なら src に結果が入る
	src:	変換前の文字列へのポインタ。dst が NULL ならここに結果が入る
	cur:	基準となるパスへのポインタ。NULL なら GetCurrentDirectory の内容
	flag:	正規化する手法 ※必ず先頭と末尾の、空白とタブを除去する

	->		NULL						変換に失敗
			(flag & VFSFIX_FULLPATH)	最下層のエントリを示すポインタ
			!(flag & VFSFIX_FULLPATH)	変換されたパスの先頭
-----------------------------------------------------------------------------*/
// flags 用定数
#define VFSFIX_DRIVE		B0	// １文字ドライブ指定の補正（"A"→"A:"）
#define VFSFIX_SEPARATOR	B1	// セパレータ"\", "/" の補正（"/"→"\"）
// #define VFSFIX_CASE		B2	// 大文字化(1.12より無効化中)
#define VFSFIX_FULLPATH		B3	// 絶対指定に変換（"A:"→"A:\WINDOWS"）
#define VFSFIX_REALPATH		B4	// 仮想パスの実体化
#define VFSFIX_NOBLANK		B5	// 空欄をエラーとする
#define VFSFIX_NOFIXEDGE	B6	// 先頭・末端の加工をしない(整形済みを利用)
#define VFSFIX_VREALPATH	B7	// 仮想パスの実体化(実体化できないときはそのまま)
#define VFSFIX_KEEPLASTPERIOD	B8	// 末尾「.」を維持する

#define VFSFIX_VFPS			(VFSFIX_DRIVE | VFSFIX_SEPARATOR | VFSFIX_FULLPATH)
#define VFSFIX_PATH			(VFSFIX_VFPS | VFSFIX_REALPATH)
extern VFSDLL TCHAR * PPXAPI VFSFixPath(TCHAR *dst, TCHAR *src, _In_opt_z_ const TCHAR *cur, int flags);

/*-----------------------------------------------------------------------------
	相対指定の src を絶対指定の cur より絶対指定に変換する
	階層区切りは'\'のみ

	dst:	生成した文字列を入れるポインタ。ここが NULL なら src に結果が入る
	src:	変換前の文字列へのポインタ。dst が NULL ならここに結果が入る
	cur:	基準となるパスへのポインタ。NULL なら GetCurrentDirectory の内容

	->		NULL			変換に失敗
			!NULL			最下層のエントリを示すポインタ
			※末尾は、rootなら「***\」、dirなら「***」
-----------------------------------------------------------------------------*/
extern VFSDLL TCHAR * PPXAPI VFSFullPath(_Out_writes_opt_z_(VFPS) TCHAR *dst, TCHAR *src, _In_opt_z_ const TCHAR *cur);

/*-----------------------------------------------------------------------------
	仮想ディレクトリを実体化させる
-----------------------------------------------------------------------------*/
extern VFSDLL BOOL PPXAPI VFSGetRealPath(HWND hWnd, TCHAR *path, const TCHAR *vfs);

/*-----------------------------------------------------------------------------
	指定パスを一括作成する
-----------------------------------------------------------------------------*/
extern VFSDLL ERRORCODE PPXAPI MakeDirectories(const TCHAR *dst, const TCHAR *src);

extern VFSDLL ERRORCODE PPXAPI VFSChangeDirectory(HWND hWnd, const TCHAR *path);
extern VFSDLL ERRORCODE PPXAPI VFSTryDirectory(HWND hWnd, const TCHAR *path, BOOL trymode);

// 必要なら「-n」を付加して、重複のない名前にする
extern VFSDLL BOOL PPXAPI GetUniqueEntryName(TCHAR *src);
/*=============================================================================
================================ Shell's Namespace IDL 操作関係、要 shlobj.h */
#ifdef _SHLOBJ_H_
extern VFSDLL BOOL PPXAPI PIDL2DisplayNameOf(TCHAR *name, LPSHELLFOLDER sfolder, LPCITEMIDLIST pidl);
extern VFSDLL LPSHELLFOLDER PPXAPI VFPtoIShell(HWND hWnd, const TCHAR *vfp, LPITEMIDLIST *idl);
extern VFSDLL UINT PPXAPI GetPidlSize(LPCITEMIDLIST pidl);
extern VFSDLL LPITEMIDLIST PPXAPI PathToPidl(LPCTSTR pszFile);
extern VFSDLL LPSHELLFOLDER PPXAPI PidlToIShell(LPCITEMIDLIST  pidl);
extern VFSDLL LPITEMIDLIST PPXAPI IShellToPidl(LPSHELLFOLDER pParent, LPCTSTR pszFile);
extern VFSDLL LPITEMIDLIST PPXAPI BindIShellAndFname(LPSHELLFOLDER pParentFolder, const TCHAR *fname);
extern VFSDLL LPITEMIDLIST PPXAPI BindIShellAndFdata(LPSHELLFOLDER pParentFolder, WIN32_FIND_DATA *fdata);
extern VFSDLL void PPXAPI FreePIDL(LPCITEMIDLIST pidl);

extern VFSDLL BOOL PPXAPI SHContextMenu(HWND hwnd, LPPOINT lppt, LPSHELLFOLDER lpsfParent, LPITEMIDLIST *lpi, int pidls, const TCHAR *cmd);
extern VFSDLL BOOL PPXAPI VFSMakeIDL(const TCHAR *path, LPSHELLFOLDER *ppSF, LPITEMIDLIST *pidl, const TCHAR *filename);
extern VFSDLL void PPXAPI GetTextFromCF_SHELLIDLIST(TCHAR *text, SIZE32_T textsize, HGLOBAL hGMem, BOOL lf);
#endif
extern VFSDLL BOOL PPXAPI VFSSHContextMenu(HWND hWnd, LPPOINT pos, const TCHAR *path, const TCHAR *entry, const TCHAR *cmd);
/*=============================================================================
============================================================= File Operation */
#define VFSFOP_AUTOSTART	B0	// [OK]を押さなくてもすぐに処理開始
#define VFSFOP_FREEFILES	B1	// files を HeapFree(GetProcessHeap())で解放
#define VFSFOP_NOTIFYREADY	B2	// 作成したダイアログ(hDialogWnd)を通知する
#define VFSFOP_SPECIALDEST	B3	// 処理先を常にVFSFILEOPERATIONのdestにする
#define VFSFOP_USEKEEPDIR	B4	// 階層再現指定(VFSFOP_OPTFLAG_KEEPDIR)を使用可能にする

typedef struct {
	const TCHAR *action;	// IN 処理方法
	const TCHAR *src;		// IN コピー元基準パス(NULL or "" : 使用せず)
	const TCHAR *dest;		// IN コピー先準パス(NULL or "" : 使用せず)
	const TCHAR *files;		// IN コピー元ﾌｧｲﾙ名 name1\0name2\0...namex\0\0 形式
	const TCHAR *option;	// IN 追加パラメータ(NULL or "" : 使用せず)
	int dtype;				// IN コピー元のファイル形式(通常は VFSDT_UNKNOWN )
	PPXAPPINFO *info;		// IN 参照用
	HWND hReturnWnd;		// IN 終了後、アクティブにするウィンドウ(NULL:未使用)
	DWORD flags;			// IN 追加指定 VFSFOP_
	HWND hLogWnd; // unused 結果ウィンドウ ※現在未使用

	HANDLE hReadyEvent;		// IN 初期化完了/hDialogWnd有効を通知するイベント(VFSFOP_NOTIFYREADY指定時のみ設定が必要)
	HWND hDialogWnd;		// OUT 作成されたダイアログ
} VFSFILEOPERATION;

extern VFSDLL BOOL PPXAPI PPxFileOperation(HWND hWnd, VFSFILEOPERATION *fileop);

#define VFSDE_WARNFILEINDIR B0	// Dir内のファイルがあれば警告するか
#define VFSDE_REPORT B1	// log出力するか
#define VFSDE_SYMDEL_SHIFT	2
#define VFSDE_SYMDEL_SYM	B2 // シンボリックリンク/ジャンクションdirを解除
#define VFSDE_SYMDEL_FILE	B3 // シンボリックリンク/ジャンクションdir内ファイルを削除
typedef struct {
	PPXAPPINFO *info;
	const TCHAR *path;	// 対象パス
	DWORD OldTime;		// 表示間引き用の時間計測
	DWORD count;		// 削除したファイル数
	DWORD useaction;	// エラー時の対処方法(コントロールID)
	BOOL noempty;		// 削除できなかったディレクトリがあるときに TRUE
	DWORD flags;
	DWORD warnattr;		// 警告する属性
} DELETESTATUS;

extern VFSDLL ERRORCODE PPXAPI VFSDeleteEntry(DELETESTATUS *dinfo, const TCHAR *path, DWORD attr);

extern VFSDLL ERRORCODE PPXAPI VFSLoadFileImage(const TCHAR *filename, DWORD margin, char **image, DWORD *imagesize, DWORD *filesize);

/*=============================================================================
============================================================= Tree */
#define VFSTREECLASS	"VfsTreeClass"
extern VFSDLL void PPXAPI InitVFSTree(void); // VFSTREECLASS を使用可能にする

#define VTM_SETFLAG		(WM_APP + 0) //Tree の挙動を指定する
#define VTM_INITTREE	(WM_APP + 1)
#define VTM_SETPATH		(WM_APP + 2)
#define VTM_GETPATH		(WM_APP + 3)
#define VTM_SCROLL		(WM_APP + 4)
#define VTM_POINTPATH	(WM_APP + 5)
#define VTM_ADDTREEITEM	(WM_APP + 6)
#define VTM_GETSETTINGS	(WM_APP + 7)
#define VTM_TREECOMMAND	(WM_APP + 8)
#define VTM_SETRESULT	(WM_APP + 9)
#define VTM_CHANGEDDISPDPI	(WM_APP + 10)

// VTM_SETFLAG で指定可能なフラグ
#define VFSTREE_SELECT		B0	// 選択完了したら終了
#define VFSTREE_PATHNOTIFY	B1	// パス変更があったら通知
#define VFSTREE_SYNC		B2	// 選択したらその度に通知
#define VFSTREE_PPC			B3	// PPc 用 UI
#define VFSTREE_SPLITR		B4	// 右側に分割線を表示
#define VFSTREE_MENU		B5	// ポップアップメニュー動作
#define VFSTREE_DISABLEDRAG	B6	// ドラッグ無効

// VTM_POINTPATH 用
#define VTMPP_CANADDITEM B0 // 戻り値:項目登録可
typedef struct {
	DWORD_PTR Titem;	// 最適化用、新規の時は 0 を設定
	POINT pos;		// スクリーン座標で
	TCHAR *path;	// 保存場所。Titemが変化しないときは書き換えない
} VTMPOINTPATHSTRUCT;
/*=============================================================================
====================== Plugin DLL 関連の定義。将来このヘッダなくしたい */
#define VFSAS_UNDLL 0
#define VFSAS_SUSIE 1
#define VFSAS_ENTER 0
#define VFSAS_SERIALIZE B5
#define VFSAS_LEAVE B6
#define VFSAS_CHECK B7

extern VFSDLL int PPXAPI VFSArchiveSection(DWORD mode, const TCHAR *threadname);
//-------------------------------------------------------------- OpenArchive 系
//-------------------------------------- DLL 定数
#define FNAME_MAX32		512
typedef HGLOBAL HARC;

typedef struct {
	DWORD l, h;
} ARCSIZE64;
//-------------------------------------- DLL 構造体
typedef struct {						// FindFirst 用 -----------------------
	DWORD	dwOriginalSize;		/* ファイルのサイズ */
	DWORD	dwCompressedSize;	/* 圧縮後のサイズ */
	DWORD	dwCRC;				/* 格納ファイルのチェックサム */
	UINT	uFlag;				/* 処理結果 */
	UINT	uOSType;			/* 書庫作成に使われたＯＳ */
	WORD	wRatio;				/* 圧縮率 */
	WORD	wDate;				/* 格納ファイルの日付(DOS 形式) */
	WORD	wTime;				/* 格納ファイルの時刻(〃) */
	char	szFileName[FNAME_MAX32 + 1];	/* 書庫名 */
	char	dummy1[3];
	char	szAttribute[8];		/* 格納ファイルの属性(書庫固有) */
	char	szMode[8];			/* 格納ファイルの格納モード(〃) */
} INDIVIDUALINFOA, *LPINDIVIDUALINFOA;

typedef struct {						// FindFirst 用 -----------------------
	DWORD	dwOriginalSize;		/* ファイルのサイズ */
	DWORD	dwCompressedSize;	/* 圧縮後のサイズ */
	DWORD	dwCRC;				/* 格納ファイルのチェックサム */
	UINT	uFlag;				/* 処理結果 */
	UINT	uOSType;			/* 書庫作成に使われたＯＳ */
	WORD	wRatio;				/* 圧縮率 */
	WORD	wDate;				/* 格納ファイルの日付(DOS 形式) */
	WORD	wTime;				/* 格納ファイルの時刻(〃) */
	WCHAR	szFileName[FNAME_MAX32 + 1];	/* 書庫名 */
	WCHAR	dummy1[3];
	WCHAR	szAttribute[8];		/* 格納ファイルの属性(書庫固有) */
	WCHAR	szMode[8];			/* 格納ファイルの格納モード(〃) */
} INDIVIDUALINFOW, *LPINDIVIDUALINFOW;
#ifdef UNICODE
typedef INDIVIDUALINFOA   INDIVIDUALINFO;
typedef LPINDIVIDUALINFOA LPINDIVIDUALINFO;
#else
typedef INDIVIDUALINFOW   INDIVIDUALINFO;
typedef LPINDIVIDUALINFOW LPINDIVIDUALINFO;
#endif

//-------------------------------------- DLL 関数
typedef int (WINAPI *ARCPROC)();
typedef int (WINAPI *impUnarc)(const HWND hwnd, LPCSTR szCmdLine, LPSTR szOutput, const DWORD wSize);
typedef int (WINAPI *impUnarcW)(const HWND hwnd, LPCWSTR szCmdLine, LPWSTR szOutput, const DWORD wSize);
typedef HARC (WINAPI *impUnOpenArchive) (const HWND hwnd, LPCSTR szFileName, const DWORD dwMode);
typedef HARC (WINAPI *impUnOpenArchiveW) (const HWND hwnd, LPCWSTR szFileName, const DWORD dwMode);
typedef int (WINAPI *impUnCloseArchive)(HARC harc);
typedef int (WINAPI *impUnFindFirst)(HARC harc, LPCSTR szWildName, INDIVIDUALINFOA *lpSubInfo);
typedef int (WINAPI *impUnFindFirstW)(HARC harc, LPCWSTR szWildName, INDIVIDUALINFOW *lpSubInfo);
typedef int (WINAPI *impUnFindNext)(HARC harc, INDIVIDUALINFOA *lpSubInfo);
typedef int (WINAPI *impUnFindNextW)(HARC harc, INDIVIDUALINFOW *lpSubInfo);
typedef BOOL (WINAPI *impUnCheckArchive)(LPCSTR szFileName, const int iMode);
typedef BOOL (WINAPI *impUnCheckArchiveW)(LPCWSTR szFileName, const int iMode);
typedef BOOL (WINAPI *impSetUnicodeMode)(const BOOL _bUnicode);
typedef BOOL (WINAPI *impUnGetOriginalSizeEx)(HARC harc, ARCSIZE64 *_lpllSize);
typedef BOOL (WINAPI *impUnGetWriteTimeEx)(HARC harc, FILETIME *_lpftLastWriteTime);
typedef BOOL (WINAPI *impUnGetCreateTimeEx)(HARC harc, FILETIME *_lpftCreationTime);
typedef BOOL (WINAPI *impUnGetAccessTimeEx)(HARC harc, FILETIME *_lpftLastAccessTime);
//-------------------------------------- PPx 定義
#if defined(ONPPXDLL)
typedef struct {
	const char *filename;
	BYTE *header;
	SIZE32_T fsize;
	int floatheader;	// -1:シングルパスチェック 0:固定ヘッダ 1:浮動ヘッダ
	#ifdef UNICODE
		char filename8[VFPS];
		const WCHAR *filenameW;
	#endif
} VUCHECKSTRUCT;

typedef BOOL (*VUCHECKFUNC)(void *uD, VUCHECKSTRUCT *vcs);
typedef struct tagUNDLL{				// 呼出し用 ---------------------------
	HANDLE	hadd;					// DLL のハンドル
	impUnarc			Unarc;
	impUnOpenArchive	UnOpenArchive;
	impUnCloseArchive	UnCloseArchive;
	impUnFindFirst		UnFindFirst;
	impUnFindNext		UnFindNext;
	impUnCheckArchive	UnCheckArchive;
	impUnGetWriteTimeEx UnGetWriteTimeEx;
	impUnGetCreateTimeEx UnGetCreateTimeEx;
	impUnGetAccessTimeEx UnGetAccessTimeEx;
	impUnGetOriginalSizeEx UnGetOriginalSizeEx;

	#ifdef UNICODE
		impUnarcW			UnarcW;
		impUnOpenArchiveW	UnOpenArchiveW;
		impUnFindFirstW		UnFindFirstW;
		impUnFindNextW		UnFindNextW;
		impUnCheckArchiveW	UnCheckArchiveW;
		impSetUnicodeMode	SetUnicodeMode;
	#endif
	VUCHECKFUNC VUCheck;

	const TCHAR *DllName;
	const TCHAR *DllYetAnotherName;
	const TCHAR *ApiHeadName;

	const TCHAR *AllExtCMD;		// 書庫内ファイル全展開コマンド
	const TCHAR *PartExtCMD;	// 書庫内ファイルを指定して展開するコマンド
	const TCHAR *SingleExtCMD;	// 書庫内の１ファイルを展開するコマンド
	const TCHAR *CheckWildcard;	// ファイルの書庫チェック時に使用するワイルドカード
	const TCHAR *SupportWildcard;	// 対応している拡張子を示すワイルドカード
	const TCHAR *params;

#define UNDLLFLAG_32bit					B0	// 32bit版
#define UNDLLFLAG_64bit					B1	// 64bit版
#define UNDLLFLAG_nativebit				ValueX3264(UNDLLFLAG_32bit, UNDLLFLAG_64bit)
#define UNDLLFLAG_DISABLE_DIR			B3	// ディレクトリ表示に使用しない
#define UNDLLFLAG_DISABLE_UNPACK		B4	// 展開に使用しない
#define UNDLLFLAG_DISABLE_PACK			B5	// 圧縮に使用しない
#define UNDLLFLAG_DISABLE_UNICODEMODE	B6	// ～SetUnicodeMode を使用しない
#define UNDLLFLAG_DISABLE_WIDEFUNCTION	B7	// ～W 系API を使用しない

#define UNDLLFLAG_RESPONSE_UTF8			B8	// ～W 系APIレスポンスファイルはUTF8

#define UNDLLFLAG_SKIP_OPENED			B10	// Open中の書庫をチェック対象外にする(UNRAR32, LMZIP32, 7zca32)
#define UNDLLFLAG_FIX_UNARCRESULT		B11	// UnArcの結果補正(CAB32)
#define UNDLLFLAG_FIX_BRACKET			B12	// ファイル名中の [,] 等をescape(UNZIP32)
#define UNDLLFLAG_FIX_IME				B13	// IMEを無効にする(7-zip32)
#define UNDLLFLAG_FIX_DUMMYWINDOW		B14	// ダミー窓を用意する(UNRAR32)
#define UNDLLFLAG_ENABLENAMECHECK		B15	// 相対指定等による危険をチェック
#define UNDLLFLAG_SKIPDLLCHECK			B16	// CheckArchiveによるチェックなし

	DWORD	flags;
	int		knowntype; // 既知のDLL種別
	#ifdef UNICODE
	UINT codepage;	// VFSFind / UnArc_Extract～UnArc_Exec間 で使用
	#endif
} UN_DLL;
#else
typedef void UN_DLL;
#endif
//-------------------------------------- UnXXX32 を呼び出す
// extern VFSDLL int PPXAPI UnARCExec(UN_DLL *uD, HWND hWnd, const char *Cmd, char *Log, DWORD LogSize); 1.27 廃止。ExecUnarcに変更
// extern VFSDLL int PPXAPI UnARC(const TCHAR *DllName, HWND hWnd, const TCHAR *Cmd, char *Log, DWORD LogSize); 1.27 廃止。%uを使用

#define UNARCEXTRACT_ALL 0
#define UNARCEXTRACT_PART 1
#define UNARCEXTRACT_SINGLE 2
extern VFSDLL ERRORCODE PPXAPI UnArc_Extract(PPXAPPINFO *info, const void *dt_opt, int extractmode, TCHAR *extract, int flag);
extern VFSDLL void PPXAPI UnArc_Exec(PPXAPPINFO *info, const void *dt_opt, TCHAR *param, HANDLE hBatchfile, const TCHAR *tmppath, DWORD X_unbg, const TCHAR *chopdir);

//------------------------------------------------------------ SUSIE Plug-in 系
//-------------------------------------- DLL 定数
#define SUSIEERROR_NOERROR       0
#define SUSIEERROR_NOTSUPPORT   -1
#define SUSIEERROR_USERCANCEL    1
#define SUSIEERROR_UNKNOWNFORMAT 2
#define SUSIEERROR_BROKENDATA    3
#define SUSIEERROR_EMPTYMEMORY   4
#define SUSIEERROR_FAULTMEMORY   5
#define SUSIEERROR_FAULTREAD     6
#define SUSIEERROR_RESERVED      7
#define SUSIEERROR_INTERNAL      8

typedef ULONG_PTR susie_time_t;
#define SUSIE_PATH_MAX 200
#define SUSIE_CHECK_SIZE 2048 // IsSupported が必要とする大きさ

//-------------------------------------- DLL 構造体
#pragma pack(push, 1)
struct PictureInfo{
	long	left, top;				// 画像を展開する位置
	long	width;					// 画像の幅(pixel)
	long	height;					// 画像の高さ(pixel)
	WORD	x_density;				// 画素の水平方向密度
	WORD	y_density;				// 画素の垂直方向密度
	short	colorDepth;				// １画素当たりのbit数
	#ifdef _WIN64
	  char  dummy[2]; // アラインメント
	#endif
	HLOCAL	hInfo;					// 画像内のテキスト情報
};

typedef struct {
	unsigned char	method[8];		// 圧縮法の種類
	ULONG_PTR		position;		// ファイル上での位置
	ULONG_PTR		compsize;		// 圧縮されたサイズ
	ULONG_PTR		filesize;		// 元のファイルサイズ
	susie_time_t	timestamp;		// ファイルの更新日時
	char			path[SUSIE_PATH_MAX];		// 相対パス
	char			filename[SUSIE_PATH_MAX];	// ファイルネーム
	unsigned long	crc;			// CRC
	#ifdef _WIN64
		char dummy[4]; // アラインメント
	#endif
} SUSIE_FINFO;

typedef struct {
	unsigned char	method[8];		// 圧縮法の種類
	ULONG_PTR		position;		// ファイル上での位置
	ULONG_PTR		compsize;		// 圧縮されたサイズ
	ULONG_PTR		filesize;		// 元のファイルサイズ
	susie_time_t	timestamp;		// ファイルの更新日時
	WCHAR			path[SUSIE_PATH_MAX];		// 相対パス
	WCHAR			filename[SUSIE_PATH_MAX];	// ファイルネーム
	unsigned long	crc;			// CRC
	#ifdef _WIN64
		char dummy[4]; // アラインメント
	#endif
} SUSIE_FINFOW;
#pragma pack(pop)
//-------------------------------------- DLL 関数
typedef int (__stdcall *GETPLUGININFO)(int infono, LPSTR buf, int buflen);
typedef int (__stdcall *ISSUPPORTED)(LPCSTR filename, void *dw);
typedef int (__stdcall *ISSUPPORTEDW)(LPCWSTR filename, void *dw);
typedef int (__stdcall *CONFIGURATIONDLG)(HWND hWnd, int function);
typedef int (__stdcall *GETPICTUREINFO)(LPCSTR buf, LONG_PTR len, unsigned int flag, struct PictureInfo *lpInfo);
typedef int (__stdcall *GETPICTUREINFOW)(LPCWSTR buf, LONG_PTR len, unsigned int flag, struct PictureInfo *lpInfo);
typedef int (__stdcall *GETPICTURE)(LPCSTR buf, LONG_PTR len, unsigned int flag, HANDLE *pHBInfo, HANDLE *pHBm, FARPROC lpPrgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETPICTUREW)(LPCWSTR buf, LONG_PTR len, unsigned int flag, HANDLE *pHBInfo, HANDLE *pHBm, FARPROC lpPrgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETPREVIEW)(LPCSTR buf, LONG_PTR len, unsigned int flag, HANDLE *pHBInfo, HANDLE *pHBm, FARPROC lpPrgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETPREVIEWW)(LPCWSTR buf, LONG_PTR len, unsigned int flag, HANDLE *pHBInfo, HANDLE *pHBm, FARPROC lpPrgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETARCHIVEINFO)(LPCSTR buf, LONG_PTR len, unsigned int flag, HLOCAL *lphInf);
typedef int (__stdcall *GETARCHIVEINFOW)(LPCWSTR buf, LONG_PTR len, unsigned int flag, HLOCAL *lphInf);
typedef int (__stdcall *GETFILE)(LPCSTR src, LONG_PTR len, LPSTR dest, unsigned int flag, FARPROC prgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETFILEW)(LPCWSTR src, LONG_PTR len, LPWSTR dest, unsigned int flag, FARPROC prgressCallback, LONG_PTR lData);
typedef int (__stdcall *GETFILEINFO)(LPCSTR buf, LONG_PTR len, LPCSTR filename, unsigned int flag, SUSIE_FINFO *lpInfo);
typedef int (__stdcall *GETFILEINFOW)(LPCWSTR buf, LONG_PTR len, LPCWSTR filename, unsigned int flag, SUSIE_FINFOW *lpInfo);
#define SUSIE_SOURCE_DISK 0
#define SUSIE_SOURCE_MEM 1
#define SUSIE_IGNORECASE B7
#define SUSIE_DEST_DISK 0
#define SUSIE_DEST_MEM 0x100
// int FAR WINAPI SusieProgressCallback(int nNum, int nDenom, LONG_PTR lData)

//-------------------------------------- PPx 定義
#define VFSSUSIE_BMP B0
#define VFSSUSIE_ARC B1
#define VFSSUSIE_NOAUTODETECT B2
typedef struct {
	HANDLE hadd;					// DLL のハンドル
	GETPLUGININFO	GetPluginInfo;	// Plug-inに関する情報を得る
	ISSUPPORTED		IsSupported;	// 展開可能な(対応している)ファイル形式か
	GETPICTURE		GetPicture;		// 画像を展開する
	GETPREVIEW		GetPreview;
	GETARCHIVEINFO	GetArchiveInfo;
	GETFILE			GetFile;
	GETFILEINFO		GetFileInfo;
#ifdef UNICODE
//	GETPLUGININFOW	GetPluginInfoW;
	ISSUPPORTEDW	IsSupportedW;
	GETPICTUREW		GetPictureW;
	GETARCHIVEINFOW	GetArchiveInfoW;
	GETFILEW		GetFileW;
	GETFILEINFOW	GetFileInfoW;
#endif
	DWORD flags;
	DWORD DllNameOffset;
	DWORD SupportExtOffset;	// 対応拡張子名
} SUSIE_DLL;
// DllNameOffset, SupportExtOffset が示すテーブル
typedef struct {
	DWORD flags;	// VFSSUSIE_BMP, VFSSUSIE_ARC
	TCHAR filemask[VFPS];
} SUSIE_DLLSTRINGS;

extern VFSDLL int PPXAPI VFSGetSusieList(const SUSIE_DLL **su, BYTE **strings);
extern VFSDLL const SUSIE_DLL * PPXAPI VFSGetSusieFuncs(const void *dt_opt);
#ifdef __cplusplus
}
#endif
