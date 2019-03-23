/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				～ FindFirst/Next ～
-----------------------------------------------------------------------------*/
#ifdef WINEGCC
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifdef DefxIShellFolder2
#ifndef xIShellItemDef
typedef ULONG xSFGAOF;
#endif
typedef DWORD xSHGDNF;
typedef void xIEnumExtraSearch;
typedef void xSHELLDETAILS;
typedef DWORD xSHCOLSTATEF;
typedef struct {
	GUID fmtid;
	DWORD pid;
} xSHCOLUMNID;

#undef INTERFACE
#define INTERFACE xIShellFolder2
DECLARE_INTERFACE_(xIShellFolder2,IUnknown)
{
	// IUnknown
	STDMETHOD (QueryInterface)(THIS_ REFIID riid,void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IShellFolder
	STDMETHOD (ParseDisplayName)(THIS_ HWND hwnd,LPBC pbc,LPOLESTR pszDisplayName,ULONG *pchEaten,LPITEMIDLIST *ppidl,ULONG *pdwAttributes) PURE;
	STDMETHOD (EnumObjects)(THIS_ HWND hwnd,SHCONTF grfFlags,IEnumIDList **ppenumIDList) PURE;
	STDMETHOD (BindToObject)(THIS_ LPCITEMIDLIST pidl,LPBC pbc,REFIID riid,void **ppv) PURE;
	STDMETHOD (BindToStorage)(THIS_ LPCITEMIDLIST pidl,LPBC pbc,REFIID riid,void **ppv) PURE;
	STDMETHOD (CompareIDs)(THIS_ LPARAM lParam,LPCITEMIDLIST pidl1,LPCITEMIDLIST pidl2) PURE;
	STDMETHOD (CreateViewObject)(THIS_ HWND hwndOwner,REFIID riid,void **ppv) PURE;
	STDMETHOD (GetAttributesOf)(THIS_ UINT cidl,LPCITEMIDLIST *apidl,xSFGAOF *rgfInOut) PURE;
	STDMETHOD (GetUIObjectOf)(THIS_ HWND hwndOwner,UINT cidl,LPCITEMIDLIST *apidl,REFIID riid,UINT *rgfReserved,void **ppv) PURE;
	STDMETHOD (GetDisplayNameOf)(THIS_ LPCITEMIDLIST pidl,xSHGDNF uFlags,STRRET *pName) PURE;
	STDMETHOD (SetNameOf)(THIS_ HWND hwnd,LPCITEMIDLIST pidl,LPCOLESTR pszName,xSHGDNF uFlags,LPITEMIDLIST *ppidlOut) PURE;

	// IShellFolder2
	STDMETHOD (GetDefaultSearchGUID)(THIS_ GUID *pguid) PURE;
	STDMETHOD (EnumSearches)(THIS_ xIEnumExtraSearch **ppenum) PURE;
	STDMETHOD (GetDefaultColumn)(THIS_ DWORD dwRes,ULONG *pSort,ULONG *pDisplay) PURE;
	STDMETHOD (GetDefaultColumnState)(THIS_ UINT iColumn,xSHCOLSTATEF *pcsFlags) PURE;
	STDMETHOD (GetDetailsEx)(THIS_ LPCITEMIDLIST pidl,const xSHCOLUMNID *pscid,VARIANT *pv) PURE;
	STDMETHOD (GetDetailsOf)(THIS_ LPCITEMIDLIST pidl,UINT iColumn,xSHELLDETAILS *psd) PURE;
	STDMETHOD (MapColumnToSCID)(THIS_ UINT iColumn,xSHCOLUMNID *pscid) PURE;
};
#else
typedef void xIShellFolder2;
#endif

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------------------------------------------- FindFirst
#define FFSTEP_NOMORE	0	// これ以上存在しない
#define FFSTEP_THIS		1	// "."
#define FFSTEP_PARENT	2	// ".."
#define FFSTEP_ENTRY	3	// 通常のエントリ
#define FFSTEP_ENTRYNEXT	4	// VFSDT_UN で使用。FindNext相当

//	VFSDT_PATH
typedef struct {
	HANDLE hFF;	// Handle のハンドル
} FF_FFF;

//	VFSDT_DLIST 等
typedef struct {
	ThSTRUCT dirs;	// 読み込み一覧
	ThSTRUCT files;	// 読み込み一覧
	int d_off;	// 読み込み位置
	int f_off;	// 読み込み位置
} FF_MC;

//	VFSDT_LFILE
#define VFSDT_LFILE_TYPE_LIST 0		// リスト内容
#define VFSDT_LFILE_TYPE_PARENT 1	// 「..」
#define VFSDT_LFILE_TYPE_FIRSTENTRY 2		// 「.」
typedef struct {
	DWORD type;	// VFSDT_LFILE_TYPE_xxx
	TCHAR *mem;	// ファイルイメージの先頭
	TCHAR *readptr;	// 読込み位置
	TCHAR *maxptr;	// 最大位置
	const TCHAR *base; // ;BASE= の位置
	const TCHAR *search; // ;Search= の位置
	const TCHAR *longname; // MAX_PATH越えファイル名
	const TCHAR *comment; // コメント
} FF_LFILE;

#ifdef _SHLOBJ_H_
//	VFSDT_SHN
enum { FFPIDL_UPDIR,FFPIDL_ENUM,FFPIDL_NETLIST,FFPIDL_NOMORE};
typedef struct {
	LPSHELLFOLDER	pSF;
	xIShellFolder2	*pSF2;
	LPENUMIDLIST	pEID;
	LPMALLOC		pMA;
	ThSTRUCT dirs;	// 読み込み一覧
	int cnt;	// FFPIDL_
	int d_off;	// 読み込み位置
	int fix; // cFileName の補正 0:しない 1:する -1:未確定
	DWORD dirflags; // VFSGetFFInfo 用に保存
} FF_SHN;
#endif

//	VFSDT_FAT
typedef struct {
	FATSTRUCT fats;
} FF_FAT;

//	VFSDT_CD
typedef struct {
	CDS cds;
} FF_CD;

//	VFSDT_FTP
typedef struct {
	HINTERNET hFtp[2];
	HINTERNET hFtpFF;
} FF_FTP;

//	VFSDT_SUSIE
typedef struct {
	SUSIE_DLL *su;
	HLOCAL fiH;
	SUSIE_FINFO *finfo,*finfomax;
	#ifdef UNICODE
	SUSIE_FINFOW *finfoW,*finfomaxW;
	#endif
	int step;	// FFSTEP_
} FF_SU;

//	VFSDT_UN
typedef struct {
	UN_DLL *uD;
	HARC Ha;
	int step;	// FFSTEP_
	TCHAR wild[MAX_PATH];
} FF_UN;

//	VFSDT_STREAM
typedef struct {
	LPVOID context;
	HANDLE hFile;	// Handle のハンドル
	int step;	// FFSTEP_
	TCHAR basename[MAX_PATH],*basep;
} FF_STREAM;

#ifdef USESLASHPATH
typedef struct {
#ifdef WINEGCC
	char path[VFPS],*pathlast;
	DIR *OpenedDir;
#else
	HANDLE hFF;	// Handle のハンドル
#endif
} FF_SLASHDIR;
#endif

#ifdef _SHLOBJ_H_
#define ZIPFOLDER_USESTRAGEENUM 0

typedef struct {
	ZIPFOLDERSTRUCT zfs;
#if ZIPFOLDER_USESTRAGEENUM
	LPENUMSTATSTG pESS;
#else
	FF_SHN shn;
#endif
} FF_ZIPFOLDER;

typedef union {
	FF_FFF	FFF;
	FF_MC	MC;

	FF_SHN	SHN;
	FF_LFILE LFILE;
	FF_FAT	FAT;
	FF_CD	CD;
	FF_ZIPFOLDER ZIPO;
	FF_STREAM	STREAM;

	FF_FTP	FTP;
	FF_SU	SU;
	FF_UN	UN;
#ifdef USESLASHPATH
	FF_SLASHDIR	SLASH;
#endif
} FF_COMMON;

#define VFSFINDFIRSTID 0x106FF
typedef struct {
	FF_COMMON v;
	DWORD ID;		// handle 識別用 ID
	int mode;	// 読み込み形態
//	TCHAR mask[MAX_PATH];	// ファイルマスク部分
	TCHAR TypeNameBuf[VFSGETFFINFO_TYPESIZE];
	const TCHAR *TypeName;	// 読み込み形態文字列
} VFSFINDFIRST;
#endif
extern void USEFASTCALL SetDummyFindData(WIN32_FIND_DATA *findfile);
extern void USEFASTCALL SetDummydir(WIN32_FIND_DATA *findfile,const TCHAR *name);
extern int USEFASTCALL FFStepInfo(WIN32_FIND_DATA *findfile,int *step);
extern void AddDriveList(ThSTRUCT *dirs,const TCHAR *name);
//------------------------------------------------------------------ vfs_ffp.c
extern HANDLE USEFASTCALL FindFirstFileLs(const TCHAR *dir,WIN32_FIND_DATA *findfile);
//------------------------------------------------------------------ vfs_ffp.c
extern BOOL MakeComputerResourceList(FF_MC *MC,const TCHAR *fname);
#ifdef _SHLOBJ_H_
extern void EnumNetServer(ThSTRUCT *dirs,BOOL extend);
#endif

extern void FindFirstStream(HANDLE hFile,FF_STREAM *fs,const TCHAR *filename);
extern BOOL FindNextStream(FF_STREAM *fs,WIN32_FIND_DATA *findfile);
extern void FindCloseStream(FF_STREAM *fs);

#ifdef USESLASHPATH
extern ERRORCODE FirstSlashDir(FF_SLASHDIR *sdptr,const TCHAR *dir,WIN32_FIND_DATA *findfile);
extern BOOL NextSlashDir(FF_SLASHDIR *sdptr,WIN32_FIND_DATA *findfile);
extern void CloseSlashDir(FF_SLASHDIR *sdptr);
#endif
//------------------------------------------------------------------ vfs_fmy.c
extern void MakeDriveList(FF_MC *mc);

//------------------------------------------------------------------ vfs_ftp.c
extern BOOL FTPnf(FF_FTP *FTP,WIN32_FIND_DATA *findfile);
extern DWORD FTPff(FF_FTP *FTP,const TCHAR *url,WIN32_FIND_DATA *findfile);
extern BOOL FTPclose(FF_FTP *FTP);

//------------------------------------------------------------------ vfs_http.c
extern ERRORCODE MakeWebList(FF_MC *mc,const TCHAR *filename,BOOL file);

//------------------------------------------------------------------ vfs_lf.c
#ifdef _SHLOBJ_H_
extern ERRORCODE InitFindFirstListFile(VFSFINDFIRST *VFF,const TCHAR *Fname,WIN32_FIND_DATA *findfile);
#endif
extern BOOL GetListLine(FF_LFILE *list,WIN32_FIND_DATA *findfile);

//------------------------------------------------------------------ vfs_ffsh.c
#ifdef _SHLOBJ_H_
extern BOOL VFSFN_SHN(FF_SHN *vshn,WIN32_FIND_DATA *findfile);
extern DWORD VFSFF_SHN(const TCHAR *vpath,FF_SHN *vshn,WIN32_FIND_DATA *findfile,int mode);

BOOL ZipFolderFF(FF_ZIPFOLDER *zf,const TCHAR *fname,TCHAR *subdir,WIN32_FIND_DATA *findfile,int type);
BOOL ZipFolderFN(FF_ZIPFOLDER *zf,WIN32_FIND_DATA *findfile);
void ZipFolderFClose(FF_ZIPFOLDER *zf);
#endif

#ifdef __cplusplus
}
#endif
