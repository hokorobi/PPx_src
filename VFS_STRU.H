/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System						内部定義
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#ifdef RELEASE
#define CHECKVFXEXNAME 0
#else
#define CHECKVFXEXNAME 0
#endif

#define CP_PPX_UCF2 0xfffff // UCF2 を指定するときの内部コード
#define VFS_FORCELOAD_PLUGIN B30 // 読み込み抑止しているPluginも読み込む

typedef struct {
	DWORD l,h;
} DDWORD;

#define MAKEFLAG_EnumObjectsForFolder ((WinType >= WINTYPE_VISTA) ? ENUMOBJECTSFORFOLDERFLAG_VISTA : ENUMOBJECTSFORFOLDERFLAG_XP)

// FTP 関連 -------------------------------------------------------------------
#ifndef HINTERNET
#ifndef ERROR_INTERNET_EXTENDED_ERROR
#define ERROR_INTERNET_EXTENDED_ERROR 12003
#define ERROR_INTERNET_CANNOT_CONNECT 12029
#define ERROR_INTERNET_LOGIN_FAILURE  12015
#endif

#define INTERNET_OPEN_TYPE_PRECONFIG	0
#define INTERNET_OPEN_TYPE_DIRECT		1
#define INTERNET_DEFAULT_FTP_PORT		21
#define INTERNET_FLAG_RELOAD			0x80000000
#define INTERNET_FLAG_DONT_CACHE		0x04000000
#define INTERNET_FLAG_PASSIVE			0x08000000
#define INTERNET_SERVICE_FTP			1
typedef LPVOID HINTERNET;
typedef WORD INTERNET_PORT;

#define FTP_TRANSFER_TYPE_UNKNOWN   0x00000000
#define FTP_TRANSFER_TYPE_ASCII     0x00000001
#define FTP_TRANSFER_TYPE_BINARY    0x00000002
#endif

TypedefWinAPI(HINTERNET,InternetOpen,(LPCTSTR lpszAgent,DWORD dwAccessType,LPCTSTR lpszProxy,LPCTSTR lpszProxyBypass,DWORD dwFlags));
TypedefWinAPI(BOOL,InternetCloseHandle,(HINTERNET hInternet));
TypedefWinAPI(HINTERNET,InternetConnect,(HINTERNET hInternet,LPCTSTR lpszServerName,INTERNET_PORT nServerPort,LPCTSTR lpszUserName,LPCTSTR lpszPassword,DWORD dwService,DWORD dwFlags,DWORD *dwContext));
TypedefWinAPI(BOOL,FtpSetCurrentDirectory,(HINTERNET hConnect,LPCTSTR lpszDirectory));
TypedefWinAPI(BOOL,FtpGetCurrentDirectory,(HINTERNET hConnect,LPTSTR lpszCurrentDirectory,LPDWORD lpdwCurrentDirectory));
TypedefWinAPI(HINTERNET,FtpFindFirstFile,(HINTERNET hConnect,LPCTSTR lpszSearchFile,WIN32_FIND_DATA *lpFindFileData,DWORD dwFlags,DWORD *dwContext));
TypedefWinAPI(BOOL,InternetFindNextFile,(HINTERNET hFind,LPVOID lpvFindData));

TypedefWinAPI(BOOL,FtpGetFile,(HINTERNET hConnect,LPCTSTR lpszRemoteFile,LPCTSTR lpszNewFile,BOOL fFailIfExists,DWORD dwFlagsAndAttributes,DWORD dwFlags,DWORD dwContext));

TypedefWinAPI(BOOL,FtpPutFile,(HINTERNET hConnect,LPCTSTR lpszLocalFile,LPCTSTR lpszNewRemoteFile,DWORD dwFlags,DWORD dwContext));

TypedefWinAPI(BOOL,FtpRenameFile,(HINTERNET hConnect,LPCTSTR lpszExisting,LPCTSTR lpszNew));

TypedefWinAPI(BOOL,FtpDeleteFile,(HINTERNET hConnect,LPCTSTR lpszFileName));

TypedefWinAPI(BOOL,FtpCreateDirectory,(HINTERNET hConnect,LPCTSTR lpszDirectory));

TypedefWinAPI(BOOL,FtpRemoveDirectory,(HINTERNET hConnect,LPCTSTR lpszDirectory));

#define FTPINET 0
#define FTPHOST 1
extern DWORD OpenFtp(TCHAR *url,HANDLE *hFtp);
extern ERRORCODE MoveFtpFile(const TCHAR *ExistingFileName,const TCHAR *NewFileName);
extern ERRORCODE CreateFtpDirectory(const TCHAR *path);
extern ERRORCODE DeleteFtpEntry(const TCHAR *path,DWORD attributes);
extern ERRORCODE GetInetError(void);

// CD-ROM/FAT 関連 ------------------------------------------------------------
// FAT ********************************
#pragma pack(push,1)
					// FATエントリ情報
typedef struct {
	char	name[8];
	char	ext[3];
	BYTE	atr;
	BYTE	vflag;
	BYTE	sum;
	WORD	Ctime;
	WORD	Cdate;
	WORD	Adate;
	WORD	clusterH;
	WORD	Wtime;
	WORD	Wdate;
	WORD	clusterL;
	DWORD	size;
} FATENTRY;
					// LFN 部分のエントリ情報
typedef struct {
	BYTE	ID;
	WORD	name1[5];
	BYTE	atr;
	BYTE	zero1;
	BYTE	sum;
	WORD	name2[6];
	WORD	zero2;
	WORD	name3[2];
} LFNENTRY;
					// X68000形式のエントリ情報
typedef struct {
	char	name1[8];
	char	ext[3];
	BYTE	atr;
	char	name2[10];
	WORD	time;
	WORD	date;
	WORD	cluster;
	DWORD	size;
} XFATENTRY;
// CD-ROM ********************************
#define CDHEADEROFFSET1 0x8000
#define CDHEADEROFFSET2 0x9310
#define CDHEADEROFFSET3 0x9318
#define CDHEADEROFFSET4 0x53000
#define DVDHEADEROFFSET 0x80000

					// CD-ROM
typedef struct {
	BYTE	type;		// 1:PVD,2:SVD
	char	ID[5];		// "CD001"
	BYTE	version;	// ボリューム記述子版数(1)
	BYTE	volumeflag;
	char	systenID[32];
	char	volumeID[32];
	char	reserved[8];
	DWORD	volumeSpaces[2];
	char	ESC[32];
	WORD	volumeBlocks[2];
	WORD	volumeSerial[2];
	WORD	SectorSize[2];
	DWORD	PathTableSize[2];
	DWORD	PathTable1LE;
	DWORD	PathTable2LE;
	DWORD	PathTable1BE;
	DWORD	PathTable2BE;
} PVDSTRUCT;

typedef struct {
	BYTE	len;		// 名前の長さ
	BYTE	extlen;		// 拡張部分の長さ
	DWORD	extend;		// 実体へのインデックス
	WORD	parent;		// 親のパスインデックス
	char	name[1];
} PATHRECORD;

typedef struct {
	BYTE year;	// 西暦(+1900)
	BYTE month;
	BYTE day;
	BYTE hour;
	BYTE minute;
	BYTE second;
	char UTC;	// x15min
} DATETIME;

#define CDATR_HIDDEN	B0
#define CDATR_DIRECTORY	B1
#define CDATR_RELAY		B2	// 関連ファイル
#define CDATR_RECORD	B3
#define CDATR_PROTECTED	B4
#define CDATR_ENDMARK	B7	// 最終エントリの印

typedef struct {
	BYTE	next;		// この構造体の長さ
	BYTE	extlen;		// 拡張部分の長さ
	DWORD	extend[2];	// 実体へのインデックス
	DWORD	size[2];	// ファイルサイズ
	DATETIME date;
	BYTE	flags;
	BYTE	fileunitsize;
	BYTE	interleave;
	WORD	serial[2];
	BYTE	len;		// 名前の長さ
	char	name[1];
} DIRECTORYRECORD;

// UDF ********************************
typedef struct {
	DWORD LBN; // パーテーション内オフセット
	WORD PRN; // パーテーション番号
} UDFLBADDR;

typedef struct {	// 8 bytes
	DWORD size; // 大きさ
	DWORD loc;  // セクタ番号
} UDFEXTENTAD;

typedef struct {
	DWORD length; // 大きさ
	DWORD position;  // セクタ番号
} UDFSHORTAD;

typedef struct {	// 16bytes
	DWORD length; // 大きさ
	UDFLBADDR location;  // セクタ番号
	BYTE reserved[6];
} UDFLONGAD;

typedef struct {	// 16bytes
	WORD id,ver;
	BYTE checksum,reserved;
	WORD SN,crc,len;
	DWORD loc; // 現在位置
} UDFTAG;

typedef struct {	// 12bytes
	WORD tz,year;
	BYTE month,day, hour,minute,second, ssecond,msecond,csecond;
} UDFTIME;

typedef struct {	// 64bytes
	BYTE type,text[63];
} UDFCHARINFO;

typedef struct {
	BYTE data[32];
} UDFREGID;

typedef struct {	// 20bytes
	DWORD PRNDE;
	WORD ST;
	BYTE SP[2];
	WORD MNE;
	BYTE reserved;
	BYTE FT;
	UDFLBADDR ParentICB;
	WORD Flags;
} UDFICB;

#define UDF_FID_ID 257
typedef struct {
	UDFTAG tag;
	WORD FVN;
	BYTE FC;
	BYTE L_FI;
	UDFLONGAD ICB;
	WORD L_IU;
	BYTE IU[1];
} UDFFID;
#define UDFATR_HIDDEN	B0
#define UDFATR_DIRECTORY	B1
#define UDFATR_DELETED	B2
#define UDFATR_PARENT	B3

// Descriptor 関連

typedef struct {
	UDFTAG tag;
	UDFEXTENTAD VDS,subVDS;
} UDF_AVDP;

//VDS***
// VDS 内に配置される内容...PVD=1,IUVD=4,PD=5,LVD=6,USD=7,TD=8
#define UDF_PD_ID 5
typedef struct {
	UDFTAG tag;
	DWORD VDSN;
	WORD PF;
	WORD PD;
	UDFREGID PC;
	BYTE PCU[128];
	DWORD AT;
	DWORD PAL; // +0xbc パーテーションの先頭
	DWORD PL;
	UDFREGID II;
	BYTE IU[128];
	UDFEXTENTAD ISE;
} UDF_PD;

#define UDF_LVD_ID 6
typedef struct {
	UDFTAG tag;		// 00
	DWORD VDSN;		// 10
	UDFCHARINFO DCS; // 14
	BYTE LVI[128];	// 54
	DWORD LBS;		// D4
	UDFREGID DI;	// D8
	UDFLONGAD LVCU; // F8 FD
	DWORD MTL;		// 108
	DWORD NPM;		// 10C
	UDFREGID II;	// 110
	BYTE IU[128];	// 130
	UDFEXTENTAD ISE;
	BYTE PM[2]; // MTL さいず
} UDF_LVD;
//VDS end***

#define UDF_TD_ID 8

#define UDF_FSD_ID 256
typedef struct {
	UDFTAG tag;			// 00
	UDFTIME RDT;		// 10
	WORD IL;			// 1c
	WORD MIL;			// 1e
	DWORD CSL;			// 20
	DWORD MCSL;			// 24
	DWORD FSN;			// 28
	DWORD FSDN;			// 2c
	UDFCHARINFO LVIC;	// 30
	BYTE LVI[128];		// 70
	UDFCHARINFO FSCS;	// f0
	BYTE FSI[32];		// 130
	BYTE CFI[32];		// 150
	BYTE AFI[32];		// 170
	UDFLONGAD ROOTICB;	// 190
	UDFREGID DI;
	UDFLONGAD NE;
	UDFLONGAD SSDI;
} UDF_FSD;

#define UDF_FE_ID 261
typedef struct {
	UDFTAG tag;			// 00
	UDFICB ICB;			// 10
	DWORD UID;			// 24
	DWORD GID;			// 28
	DWORD Permissions;	// 2c
	WORD FLC;			// 30
	BYTE RF;
	BYTE RDA;
	DWORD RL;			// 34
	DDWORD IL;			// 38 // ここまで UDF_EFE と共通
	DDWORD LBR;			// 40
	UDFTIME ADT;		// 48
	UDFTIME MDT;		// 54
	UDFTIME ATRDT;		// 60
	DWORD checkpoint;	// 6c
	UDFLONGAD EAICB;	// 70
	UDFREGID II;		// 80
	DDWORD UI;			// a0
	DWORD L_EA;			// a8
	DWORD L_AD;			// ac
	BYTE EA[1];			// b0
} UDF_FE;

#define UDF_EFE_ID 266
typedef struct {
	UDFTAG tag;			// 00
	UDFICB ICB;			// 10
	DWORD UID;			// 24
	DWORD GID;			// 28
	DWORD Permissions;	// 2c
	WORD FLC;			// 30
	BYTE RF;
	BYTE RDA;
	DWORD RL;			// 34
	DDWORD IL;			// 38
	DDWORD OS;			// 40 +EFE
	DDWORD LBR;			// 48
	UDFTIME ADT;		// 50
	UDFTIME MDT;		// 5C
	UDFTIME CDT;		// 68 +EFE
	UDFTIME ATRDT;		// 74
	DWORD checkpoint;	// 80
	DWORD reserved;		// 84 +EFE
	UDFLONGAD EAICB;	// 88
	UDFLONGAD SDI;		// 98 +EFE
	UDFREGID II;		// a8
	DDWORD UI;			// c8
	DWORD L_EA;			// d0
	DWORD L_AD;			// d4
	BYTE EA[1];			// d8
} UDF_EFE;
#pragma pack(pop)
//-----------------------------------------------------------------------------
typedef struct {
					// ファイル特定用の情報。VFSCheckFile の内部判別用。
	int flags;			// B0=1:他に立っているフラグがない時に立てる
						// B1=1:浮動ヘッダ
						// B4=1:大文字・小文字の区別なし(B1=1の時は現在無効)
	DWORD off;			// 検索位置/検索範囲
	DWORD hsize;		// ヘッダの大きさ
	const char *header;	// ヘッダ内容
						// 詳細試験
							// fname	対象ファイル名
							// fbottom	ファイル先頭
							// header	header が見つかった場所
							// result	結果を書きこむ位置
							// 戻り値が 0:成功 !0:失敗
	ERRORCODE (*mChk)(const TCHAR *fname,const char *image,DWORD size,
						const char *header,VFSFILETYPE *result);

					// 結果用
	const TCHAR *type;		// 種類名タグ
	const TCHAR *typetext;	// 種類名コメント
	const TCHAR *ext;		// 通常用いられる拡張子
	int dtype;		// (PPV) (LOWORD)テキストの形式 (HIWORD)変換方法
} VFD_HEADERS;

typedef struct {
	DDWORD cluster;	// 現在のクラスタ
	DDWORD clusterBase;	// クラスタブロックの先頭
	DWORD left;		// 残りクラスタ数
	BYTE *datas;	// 次のクラスタブロック情報
} CLUSTERRINFO;

typedef struct {	// ディスクイメージ共用部
	DWORD	BpC;		// クラスタあたりのバイト数
} DCOMMONS;

typedef struct {	// NTFS用
	CLUSTERRINFO mft,readmft;
	CLUSTERRINFO entry;
	void *mftd;
} NTFSS;

typedef union {
	NTFSS n;
	DWORD e_useFAT; // exFAT,FAT参照の必要があるなら 1
} FCOMMONS;
//------------------------------------- FAT
typedef struct {
	DCOMMONS c;			// 共用

	HANDLE	hFile;		// ディスクイメージのファイルハンドル
	DWORD	Offset;		// ディスクイメージの実体の開始位置

	DWORD	NowFAT;		// FATのアクセス場所(FAT32,exFATで使用)
	BYTE	*FAT;		// FAT12/16:FATイメージ  FAT32,exFAT:キャッシュ中のFAT
	BYTE	*Buffer;	// バッファ(１クラスタ分)

	DWORD	BpS;		// セクタあたりのバイト数
	DWORD	roots;		// ルートディレクトリのバイト数(FAT12,16)
	int		fattype;	// FAT の種類

	DWORD	FATOffset;	// FATのファイルの位置
	DWORD	ROOT;		// ルートディレクトリの開始位置(12/16)セクタ(32)
	DWORD	DATAOffset;	// データ開始するファイルの位置
	DWORD	current;	// カレントディレクトリの開始セクタ
							// FindFirst 用
	int		nowroot;	/* 0:これ以上の読み込み不可
							1以上:残りルートセクタ数
							-1:サブディレクトリ(次のクラスタ取得済み)
							-2:サブディレクトリ(次のクラスタ未取得) */
	DWORD	nowpointer;	// ディレクトリの参照中のセクタ/クラスタ
	DWORD	nowleft;	// 残りエントリ数
	FATENTRY *nowentry;	// 参照中のエントリ
	DWORD	entryindex;	// 実体のindex

	FCOMMONS d;			// FAT/NTFS 固有部
} FATSTRUCT;

extern const DWORD FDIoffset[];
//------------------------------------- CD
#define CD_ISO9660 0
#define CD_JULIET 1
#define CD_UDF 2

typedef union {
	BYTE *raw;
	PVDSTRUCT *pvd;
	UDF_AVDP *avdp;
	UDFTAG *udftag;
	UDF_PD *pd;
	UDF_LVD *lvd;
	UDF_FSD *fsd;
	UDF_FE *fe;
	UDF_EFE *efe;
} CDSBUFFER;

typedef struct {
	DCOMMONS c;			// 共用

	HANDLE	hFile;		// ディスクイメージのファイルハンドル
	DWORD	ReadBpS;	// ディスクイメージのセクタサイズ
	DWORD	Offset;		// ディスクイメージのセクタのデータ部オフセット

	CDSBUFFER Buffer;	// バッファ(2セクタ分)

	DWORD	PathTable;	// パステーブルのセクタ/Rootディレクトリセクタ

	int		nowleft;	// エントリテーブルの残量
	DWORD	nowdir;		// 参照中のセクタ/クラスタ
	BYTE	*nowentry;	// 参照中のエントリ
	int		mode;		// CD_xxx
	DWORD	entryindex;	// 実体のindex 0:「.」 1:「..」
	DWORD	dataoffset;	// ファイルの保存位置(UDFのFE内に実体があるときに使用)
	BOOL	ReadFileNext; // ファイルの２セクタ目以降を読み込み中か

	DWORD OffsetDelta; // ●無理矢理調整用
} CDS;

/*=============================================================================
						Plugin DLL 関連の定義 */
//-------------------------------------------------------------- OpenArchive 系
//-------------------------------------- DLL 定数
#define ERROR_NOT_SUPPORT		0x8023
#define M_INIT_FILE_USE	0x00000001L	/* レジストリの設定を使用 */

//==================================================================== Win32API
#ifdef _SHLOBJ_H_
typedef HRESULT (STDAPICALLTYPE * D_OLELOADPICTURE)(LPSTREAM lpstream,
				LONG lSize,BOOL fRunmode,REFIID riid, LPVOID FAR* lplpvObj);
#endif
typedef ERRORCODE (APIENTRY *DWNETOPENENUM)(DWORD fdwScope,DWORD fdwType,DWORD fdwUsage,LPNETRESOURCE lpNetResource,LPHANDLE lphEnum);
typedef ERRORCODE (APIENTRY *DWNETENUMRESOURCE)(HANDLE hEnum,LPDWORD lpcEntries,LPVOID lpvBuffer,LPDWORD lpcbBuffer);
typedef ERRORCODE (APIENTRY *DWNETCLOSEENUM)(HANDLE hEnum);
typedef ERRORCODE (APIENTRY *DWNETADDCONNECTION3)(HWND hwndOwner,LPNETRESOURCE lpNetResource,LPTSTR lpPassword,LPTSTR lpUserName,DWORD fdwConnectio);
//typedef ERRORCODE (APIENTRY *DWNETCANCELCONNECTION2)(LPTSTR lpszName,DWORD fdwConnection,BOOL fForce);
//typedef ERRORCODE (APIENTRY *DWNETGETCONNECTION)(LPTSTR lpszLocalName,LPTSTR lpszRemoteName,DWORD *lpcchBuffer);

extern DWNETOPENENUM DWNetOpenEnum;
extern DWNETENUMRESOURCE DWNetEnumResource;
extern DWNETCLOSEENUM DWNetCloseEnum;
extern DWNETADDCONNECTION3 DWNetAddConnection3;
//extern DWNETCANCELCONNECTION2 DWNetCancelConnection2;
//extern DWNETGETCONNECTION DWNetGetConnection;
//===================================================================== Func
//------------------------------------------------------------------ vfs_main.c
extern void USEFASTCALL CleanUpVFS(void);

#define CHECKFAT_NONE 0
#define CHECKFAT_FIRST 1
#define CHECKFAT_FAT 1
#define CHECKFAT_EXFAT 2
#define CHECKFAT_NTFS 3
extern int USEFASTCALL CheckFATImage(const BYTE *image,const BYTE *max);
extern ERRORCODE GetArchivefileImageFromTempExtract(TCHAR *ExtractPath,const TCHAR *EntryName,DWORD *sizeL,DWORD *sizeH,HANDLE *hMap,BYTE **mem);

//------------------------------------------------------------------ vfs_arcd.c
extern BOOL CheckAndLoadSusiePlugin(SUSIE_DLL *su, const TCHAR *filename, THREADSTRUCT *ts, DWORD mode);

extern BOOL VFS_check_def(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_ARJ(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_LHA(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_TAR(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_CAB(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_Rar(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_GCA(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_ZIP(UN_DLL *uD, VUCHECKSTRUCT *vcs);
extern BOOL VFS_check_7Zip(UN_DLL *uD, VUCHECKSTRUCT *vcs);

extern int DoUnarc(PPXAPPINFO *ppxa,const TCHAR *DllName,HWND hWnd,const TCHAR *param);
extern int UnArc_ExecMain(const UN_DLL *uD,HWND hWnd,const TCHAR *Cmd,PPXAPPINFO *loginfo);
extern int RunUnARCExec(PPXAPPINFO *loginfo,const void *dt_opt,TCHAR *param,const TCHAR *tmppath);

extern BOOL LoadUnDLL(UN_DLL *uD);
extern void GetPackMenu(HMENU hMenuDest,ThSTRUCT *thMenuData,DWORD *PopupID);
extern BOOL FindPackType(const TCHAR *dllname,TCHAR *arcname,TCHAR *arccommand);

//------------------------------------------------------------------ vfs_fchk.c
extern char *usertypes;

//------------------------------------------------------------------ vfs_fop.c
extern TCHAR *MakeFOPlistFromPPx(PPXAPPINFO *info/*,int flag*/);
extern TCHAR *MakeFOPlistFromParam(const TCHAR *param,const TCHAR *path);
//------------------------------------------------------------------ vfs_oimg.c
extern BOOL NotifyFileSize(HANDLE hFile,DWORD sizeL,DWORD sizeH);

//------------------------------------------------------------------ vfs_ffp.c
extern BOOL LoadNetFunctions(void);
extern BOOL CheckComputerActive(const TCHAR *fname,size_t strsize);

//------------------------------------------------------------------ vfs_ffsh.c
#ifdef _SHLOBJ_H_
extern ERRORCODE VFPtoIShellSub(HWND hWnd,const TCHAR *vpath,int mode,LPSHELLFOLDER *nSF,LPITEMIDLIST *nIDL,DWORD *dirflags,LPMALLOC *nMA);
extern LPITEMIDLIST CatPidl(LPMALLOC piMalloc,LPCITEMIDLIST idl1,LPCITEMIDLIST idl2);
#endif
//------------------------------------------------------------------ vfs_lapi.c
extern BOOL CreateDirectoryExL(const TCHAR *FileName,const TCHAR *FileName2,LPSECURITY_ATTRIBUTES sa);

TypedefWinAPI(HANDLE,FindFirstFile,(const TCHAR *lpFileName,WIN32_FIND_DATA *FindFileData));
extern impFindFirstFile DFindFirstFile;
HANDLE WINAPI LoadFindFirstFile(const TCHAR *lpFileName,WIN32_FIND_DATA *FindFileData);
//------------------------------------------------------------------ vfs_path.c
extern BOOL IsRelativeDirectory(const TCHAR *path);
extern BOOL IsParentDirectory(const TCHAR *path);
extern TCHAR *FindLastEntryPoint(const TCHAR *src);
extern DWORD GetReparsePath(const TCHAR *path,TCHAR *pathbuf);
#ifdef _SHLOBJ_H_
extern void GetIDLSub(TCHAR *path,LPSHELLFOLDER pSF,LPITEMIDLIST pSHidl);
#endif

//------------------------------------------------------------------ vfs_cd.c
extern BOOL OpenCDImage(CDS *cds,const TCHAR *fname,int offset);
extern BOOL FindEntryCDImage(CDS *cds,const TCHAR *fname,WIN32_FIND_DATA *ff);
extern void CloseCDImage(CDS *cds);
extern DWORD ReadCDCluster(CDS *cds,BYTE *dest,DWORD destsize);
extern BOOL SetCDNextCluster(CDS *cds);

//------------------------------------------------------------------ vfs_fat.c
extern BOOL OpenFATImage(FATSTRUCT *fats,const TCHAR *fname,int offset);
extern void CloseFATImage(FATSTRUCT *fats);
extern BOOL FindEntryFATImage(FATSTRUCT *fats,TCHAR *fname,WIN32_FIND_DATA *ff);
extern DWORD ReadFATCluster(FATSTRUCT *fats,BYTE *dest,DWORD destsize);
extern BOOL SetFATNextCluster(FATSTRUCT *fats);
extern DWORD GetPC9801FirstDriveSector(const BYTE *header);

//------------------------------------------------------------------ vfs_shn.c
#ifdef _SHLOBJ_H_
extern LPITEMIDLIST DupIdl(LPMALLOC pMA,LPCITEMIDLIST pIDL);
extern void FreePIDLS(LPITEMIDLIST *pidls,int cnt);
extern BOOL CopyToDropTarget(IDataObject *DataObject,IDropTarget *DropTarget,BOOL droptype,HWND hTargetWnd,DWORD alloweffect);
#endif

extern ERRORCODE AuxOperation(PPXAPPINFO *ParentInfo,const TCHAR *id,TCHAR *AuxPath,const TCHAR *src,const TCHAR *dst,TCHAR *resulttext);
extern ERRORCODE FileOperationAux(const TCHAR *id,const TCHAR *AuxPath,const TCHAR *src,const TCHAR *dst);

//---------------------------------------------------------------- vfs_shnp.cpp
#ifdef _SHLOBJ_H_
typedef struct {
	HMODULE hDLL;		// zipfldr.dll
	LPSTORAGE pST;
	LPSHELLFOLDER pSF;
	HRESULT ComInitResult;
} ZIPFOLDERSTRUCT;

extern ERRORCODE GetZipFolderImage(HWND hWnd,const TCHAR *ArchiveName,const TCHAR *EntryName,DWORD *sizeL,DWORD *sizeH,HANDLE *hMap,BYTE **mem,int type);

#endif

#ifndef __ITaskbarList3_INTERFACE_DEFINED__
typedef enum TBPFLAG {
	TBPF_NOPROGRESS	= 0,
	TBPF_INDETERMINATE	= 0x1,
	TBPF_NORMAL	= 0x2,
	TBPF_ERROR	= 0x4,
	TBPF_PAUSED	= 0x8
} TBPFLAG;
#endif

extern void SetTaskBarButtonProgress(HWND hWnd,int nowcount,int maxcount);
extern void InitTaskBarButtonIF(WPARAM wParam);
extern void * GetPathInterface(HWND hWnd,const TCHAR *name,REFIID riid,const TCHAR *path);
//=============================================================================
#ifndef GLOBALDEFINE
	#define GVAR extern
	#define GPARAM(x)
#else
	#undef GVAR
	#undef GPARAM
	#define GVAR
	#define GPARAM(x) = x
#endif


GVAR int VfsMode GPARAM(0);	// VFS の動作状態
GVAR CRITICAL_SECTION ArchiveSection[2];	// アーカイバのスレッドセーフ用
										// 0:UnDLL , 1:Susie
GVAR volatile int ArchiverUse GPARAM(0);	// アーカイバの使用中確認

GVAR const TCHAR StrShellScheme[6+1] GPARAM(T("shell:"));
GVAR const TCHAR zipfldrName[] GPARAM(T("zipfldr.dll"));
GVAR const TCHAR lzhfldrName[] GPARAM(T("lzhfldr2.dll"));
GVAR const TCHAR cabfldrName[] GPARAM(T("cabview.dll"));
GVAR const TCHAR RPATHSTR[] GPARAM(T("RemotePath"));

//--------------------------------------------------------------------- UnDLL
GVAR BOOL useundll GPARAM(FALSE);
GVAR int undll_items GPARAM(0);			// 数
GVAR UN_DLL *undll_list GPARAM(NULL);	// 情報
#ifdef UNICODE
GVAR UINT UnDllCodepage GPARAM(CP_ACP);
#endif
//--------------------------------------------------------------------- Susie
GVAR BOOL usesusie GPARAM(FALSE);
GVAR int susie_items GPARAM(0);		// SUSIE plug-in の数
GVAR SUSIE_DLL *susie_list GPARAM(NULL);	// SUSIE plug-in 情報
GVAR TCHAR susiedir[VFPS];
GVAR ThSTRUCT Thsusie,Thsusie_str;

//---------------------------------------------------------------------
GVAR HMODULE hMPR	 GPARAM(NULL);
GVAR HMODULE hWininet GPARAM(NULL);

#ifdef _SHLOBJ_H_
GVAR HMODULE hOLEPRO32 GPARAM(NULL);
GVAR D_OLELOADPICTURE DOleLoadPicture GPARAM(NULL);
#endif

GVAR ValueWinAPI(InternetCloseHandle);
GVAR ValueWinAPI(FtpGetFile);
GVAR ValueWinAPI(FtpPutFile);
GVAR ValueWinAPI(FtpCreateDirectory);
GVAR ValueWinAPI(FtpRemoveDirectory);
GVAR ValueWinAPI(FtpRenameFile);
GVAR ValueWinAPI(FtpDeleteFile);
GVAR ValueWinAPI(FtpFindFirstFile);
GVAR ValueWinAPI(InternetFindNextFile);
GVAR ValueWinAPI(FtpSetCurrentDirectory);

#ifdef __cplusplus
}
#endif
