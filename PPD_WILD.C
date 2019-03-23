/*-----------------------------------------------------------------------------
	Paper Plane xUI				ワイルドカード処理
-----------------------------------------------------------------------------*/
#define ONPPXDLL // PPCOMMON.H の DLL 定義指定
#define REXPREPLACE
#include "WINAPI.H"
#include <ole2.h>
#include <objbase.h>
#include "WINOLE.H"

#ifndef WINEGCC
#define UbstrVal bstrVal
#else
#include <wchar.h>
#define UbstrVal n1.n2.n3.bstrVal
#endif

#include "PPX.H"
#include "VFS.H"
#include "PPX_64.H"
#include "PPD_DEF.H"
#include "FATTIME.H"
#pragma hdrstop

#define PPXREGEXP void

#define EX_NONE		0		//				拡張子無し
#define EX_BOTTOM	0xffe8	//				末端
#define EX_MEM		0xffe8	//				追加メモリ
#define EX_NOT		0xffe9	// !			否定prefix
#define EX_AND		0xffea	// &			必須条件prefix
#define EX_ATTR		0xffeb	// attributes:	属性限定
#define EX_WRITEDATEREL 0xffec	// date:(w)		時刻限定(日数) ※以下、順番
#define EX_WRITEDATEABS 0xffed	// date:(w)		時刻限定(日付)   依存
#define EX_CREATEDATEREL 0xffee	// date:c		時刻限定(日数)
#define EX_CREATEDATEABS 0xffef	// date:c		時刻限定(日付)
#define EX_ACCESSDATEREL 0xfff0	// date:a		時刻限定(日数)
#define EX_ACCESSDATEABS 0xfff1	// date:a		時刻限定(日付) ここまで依存
#define EX_SIZE		0xfff2	// size:		ファイルサイズ限定
#define EX_BREGEXP	0xfff3	// /.../		正規表現 bregexp(s/.../)
#define EX_BREGONIG	0xfff4	// /.../		正規表現 bregonig(s/.../)
#define EX_IREGEXP	0xfff5	// /.../		正規表現 IRegExp(s/.../)
#define EX_ROMA		0xfff6	// roma:		roma
#define EX_DIR		0xfff7	// path:		ディレクトリ指定prefix
#define EX_JOINEXT	0xfff8	// option:i		拡張子を分離しないで比較
//#define EX_GROUP	0xfffx	// (...)		グループ化prefix

#define TYPE_EQUAL	B0	// 指定値と同じ
#define TYPE_SMALL	B1	// 指定値より小さいもの
#define TYPE_LARGE	B2	// 指定値より大きいもの
#define TYPE_INVERT(type) ((type & TYPE_EQUAL) | ((type & TYPE_SMALL) << 1) | ((type & TYPE_LARGE) >> 1))

#define EXSMEMALLOCSIZE 0x40	// メモリ確保単位

int RMatch = EX_NONE; // EX_NONE / EX_BREGEXP / EX_BREGONIG / EX_IREGEXP
DWORD X_rscah = 2;
HMODULE hOleaut32DLL = NULL;

const TCHAR AttrLabelString[] = T("rhsldaxmtfpconeivbgkuqw"); // ファイル属性ラベル FILE_ATTRIBUTE_～  残:jyz

const TCHAR inclabel[] = T("dxew");		// o: 指定用ラベル
const TCHAR defRepOpt[] = T("i"); // Replace 用初期設定
const TCHAR preope_search[] = T("s");
const TCHAR preope_match[] = T("m");
const TCHAR preope_trans[] = T("tr");

const TCHAR IREGEXPPATTENERROR[] = T("IRegExp: Pattern error");

typedef struct {
	DWORD flags; 		// 拡張 REGEXPF_xxx
	BOOL useext;	//
	TCHAR name[VFPS];	// パラメータ一時抽出先
	TCHAR *param;		// name内の':'の位置
} MAKEFNSTRUCT;

//----------------------------------------------------------------- BSTR 作成
BSTR CreateBstringLenA(const char *string,int size);
#define CreateBstringA(string) CreateBstringLenA(string,-1)
#ifdef UNICODE
#define CreateBstring(string) DSysAllocString(string)
#define CreateBstringLen(string,size) DSysAllocStringLen(string,size)
#else
#define CreateBstring(string) CreateBstringA(string)
#define CreateBstringLen(string,size) CreateBstringLenA(string,size)
#endif

//----------------------------------------------------------------- migemo 定義
typedef struct _migemo{int dummy;} migemo;
#if !defined(_WIN64) && USETASM32
	extern migemo *(USECDECL * migemo_open)(const char *dict);
	extern void (_cdecl * migemo_close)(migemo *object);
	extern char *(_cdecl * migemo_query)(migemo *object,const char *query);
	extern void (_cdecl * migemo_release)(migemo *object,char *string);

	extern migemo * migemo_open_fix(const char *dict);
#else
	migemo *(USECDECL * migemo_open)(const char *dict) = NULL;
	void (USECDECL * migemo_close)(migemo *object);
	char *(USECDECL * migemo_query)(migemo *object,const char *query);
	void (USECDECL * migemo_release)(migemo *object,char *string);

	#define migemo_open_fix migemo_open
#endif

migemo *migemodic = NULL;

LOADWINAPISTRUCT MIGEMODLL[] = {
	LOADWINAPI1ND(migemo_open),
	LOADWINAPI1ND(migemo_close),
	LOADWINAPI1ND(migemo_query),
	LOADWINAPI1ND(migemo_release),
	{NULL,NULL}
};

#ifdef UNICODE
UINT migemocodepage = CP_ACP;
UINT migemocodeflags = MB_PRECOMPOSED;
const TCHAR migemodicnameUTF8[] = T("dict\\utf-8\\migemo-dict");
#endif
const TCHAR migemodicnameSJIS[] = T("dict\\cp932\\migemo-dict");
// BRegExp 関連 ---------------------------------------------------------------
typedef struct tag_bregexp {
	const char *outp;		/* result string start ptr   */
	const char *outendp;	/* result string end ptr     */
	/*const*/ int splitctr;	/* split result counter      */
	const char **splitp;	/* split result pointer ptr  */
	int rsv1;				/* reserved for external use */
	char *parap;
	char *paraendp;
	char *transtblp;
	char **startp;
	char **endp;
	int nparens;
} BREGEXP;

int (USECDECL *BMatch)(const char *str,const char *target,const char *targetendp,BREGEXP **rxp,char *msg) = NULL;
int (USECDECL *BSubst)(char *str,char *target,char *targetendp,BREGEXP **rxp,char *msg);
int (USECDECL *BTrans)(char *str,char *target,char *targetendp,BREGEXP **rxp,char *msg);
void (USECDECL *BRegfree)(BREGEXP *rx);
//int (USECDECL *BSPLIT)(char *str,char *target,char *targetendp,int limit,BREGEXP **rxp,char *msg);
//char* (USECDECL *BRegexpVersion)(void);

LOADWINAPISTRUCT BREGDLL[] = {
	LOADWINAPI1ND(BMatch),
	LOADWINAPI1ND(BSubst),
	LOADWINAPI1ND(BTrans),
	LOADWINAPI1ND(BRegfree),
	{NULL,NULL}
};

// bregonig 関連 --------------------------------------------------------------
typedef struct tag_bregonig {
	const TCHAR *outp;
	const TCHAR *outendp;
	const int   splitctr; // アラインメント有り
	const TCHAR **splitp;
	INT_PTR rsv1;
	TCHAR *parap;
	TCHAR *paraendp;
	TCHAR *transtblp;
	TCHAR **startp;
	TCHAR **endp;
	int nparens;
} BREGONIG;

int (USECDECL * DBMatch)(TCHAR *str,TCHAR *target,TCHAR *targetendp,BREGONIG **rxp,TCHAR *msg) = NULL;
int (USECDECL * DBSubst)(TCHAR *str,TCHAR *target,TCHAR *targetendp,BREGONIG **rxp,TCHAR *msg);
int (USECDECL * DBTrans)(TCHAR *str,TCHAR *target,TCHAR *targetendp,BREGONIG **rxp,TCHAR *msg);
void (USECDECL * DBRegfree)(BREGONIG *rx);

#ifdef UNICODE
	#define LOADWINAPI1Tx LOADWINAPI1T
#else
	#define LOADWINAPI1Tx LOADWINAPI1
#endif

LOADWINAPISTRUCT BREGONIGDLL[] = {
	LOADWINAPI1Tx(BMatch),
	LOADWINAPI1Tx(BSubst),
	LOADWINAPI1Tx(BTrans),
	LOADWINAPI1Tx(BRegfree),
	{NULL,NULL}
};

// Oleaut32 関連定義 ----------------------------------------------------------
BSTR (STDAPICALLTYPE *DSysAllocString)(const OLECHAR *);
BSTR (STDAPICALLTYPE *DSysAllocStringLen)(const OLECHAR *,UINT);
typedef INT (STDAPICALLTYPE *DSYSREALLOCSTRING)(BSTR *,const OLECHAR *);
typedef INT (STDAPICALLTYPE *DSYSREALLOCSTRINGLEN)(BSTR *,const OLECHAR *,UINT);
void (STDAPICALLTYPE *DSysFreeString)(BSTR);
typedef UINT (STDAPICALLTYPE *DSYSSTRINGLEN)(BSTR);

HRESULT (STDAPICALLTYPE *DGetActiveObject)(REFCLSID rclsid, void * pvReserved,IUnknown ** ppunk);
HRESULT (STDAPICALLTYPE *DRegisterActiveObject)(IUnknown * punk,REFCLSID rclsid,DWORD dwFlags, DWORD *pdwRegister);
HRESULT (STDAPICALLTYPE *DRevokeActiveObject)(DWORD dwRegister,void *pvReserved);

void (STDAPICALLTYPE *DVariantInit)(VARIANTARG * pvarg);
HRESULT (STDAPICALLTYPE *DVariantClear)(VARIANTARG * pvarg);
HRESULT (STDAPICALLTYPE *DVariantChangeType)(VARIANTARG * pvargDest,VARIANTARG *pvarSrc,USHORT wFlags,VARTYPE vt);

LOADWINAPISTRUCT OLEAUT32_SysStr[] = {
	LOADWINAPI1(SysAllocString),
	LOADWINAPI1(SysAllocStringLen),
//	LOADWINAPI1(SysReAllocString),
//	LOADWINAPI1(SysReAllocStringLen),
	LOADWINAPI1(SysFreeString),
//	LOADWINAPI1(SysStringLen),
	LOADWINAPI1(GetActiveObject),
	LOADWINAPI1(RegisterActiveObject),
	LOADWINAPI1(RevokeActiveObject),
	{NULL,NULL}
};

LOADWINAPISTRUCT OLEAUT32_Variant[] = {
	LOADWINAPI1(VariantInit),
	LOADWINAPI1(VariantClear),
	LOADWINAPI1(VariantChangeType),
	{NULL,NULL}
};


//---------------------------------------------------- 解析時のメモリ管理の定義
typedef struct {
	BYTE *alloced;		// 確保したメモリへのポインタ NULL:使用せず
	BYTE *dest;			// 保存先
	BYTE **fixptr;		// EXS_MEM の補修位置
	DWORD size,left;	// 確保メモリと残量
} EXSMEM;

#define CEM_ERROR 0
#define CEM_COMP 1
#define CEM_ANALYZE -1
int CheckExtMode(EXSMEM *exm,MAKEFNSTRUCT *mfs);

//------------------------------------------------------------- Exs_regexp 定義
typedef struct {	// 通常の検索対象
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	TCHAR data[]; // 処理内容
} EXS_DATA;

typedef struct {	// 追加メモリへのポインタ
	WORD ext;	// (拡張子の位置)EX_MEM
	WORD next;	// 1(解析を簡単にするため)
	BYTE *nextptr;	// 次の解析内容へのポインタ
} EXS_MEM;

typedef struct {	// ! 否定
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
} EXS_NOT;

typedef struct {	// & 論理積
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
} EXS_AND;

typedef struct {	// option(EX_NONE,EX_JOINEXT等)
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
} EXS_OPTION;

typedef struct {	// /.../ 正規表現
	WORD ext;	// EX_BREGEXP
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	BREGONIG *rxp;
	TCHAR data[]; // 処理内容
} EXS_BONIREGEXP;

typedef struct {	// /.../ 正規表現
	WORD ext;	// EX_BREGEXP
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	BREGEXP *rxp;
	char data[]; // 処理内容
} EXS_BREGEXP;

typedef struct {	// /.../ 正規表現
	WORD ext;	// EX_IREGEXP
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	IRegExp *iregexp;
} EXS_IREGEXP;

typedef struct {	// roma: ローマ字検索
	WORD ext;	// EX_ROMA
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	DWORD_PTR handle;
	TCHAR data[]; // 処理内容
} EXS_ROMA;

typedef struct {	// attribute:.... ファイル属性
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	DWORD mask;	// 属性マスク
	DWORD value;	// 比較値
} EXS_ATTR;

typedef struct {	// date:...days
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	FILETIME days;	// 基準時刻
	DWORD type;
} EXS_WRITEDATEREL;

typedef struct {	// date:...y/m/d
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	SYSTEMTIME date;	// 基準時刻
	DWORD type;
} EXS_WRITEDATEABS;

typedef struct {	// size:<>...K/M/G/T
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
	DWORD sizeL,sizeH;	// 基準サイズ
	DWORD type;
} EXS_SIZE;

typedef struct {	// path:
	WORD ext;	// 拡張子のオフセット/EX_????
	WORD next;	// 次の解析内容へのオフセット(0:最終)
} EXS_DIR;

TCHAR *SplitRegularExpression(TCHAR *dst,const TCHAR *src);
BOOL SimpleRegularExpression(const TCHAR *src,const TCHAR *ss);
BOOL FilenameRegularExpression2(const TCHAR *fname,const TCHAR *fext,EXS_DATA *exs);

// 正規表現ライブラリ読み込み -------------------------------------------------
BOOL LoadRMatch(void)
{
	if ( (hBregonigDLL != NULL) || (hBregexpDLL != NULL) ) return TRUE;
	hBregonigDLL = LoadWinAPI("BREGONIG.DLL",NULL,BREGONIGDLL,LOADWINAPI_LOAD);
	if ( hBregonigDLL != NULL ) return TRUE;

	hBregexpDLL = LoadWinAPI("BREGEXP.DLL",NULL,BREGDLL,LOADWINAPI_LOAD);
	if ( hBregexpDLL != NULL ) return TRUE;
	return FALSE;
}

void FreeRMatch(void)
{
	if ( hBregexpDLL != NULL ){
		FreeLibrary(hBregexpDLL);
		hBregexpDLL = NULL;
	}
	if ( hBregonigDLL != NULL ){
		FreeLibrary(hBregonigDLL);
		hBregonigDLL = NULL;
		DBSubst = NULL;
	}
	RMatch = EX_NONE;
}

// const TCHAR VBScriptRegularExpression[] = T("CLSID\\{3F4DACA4-160D-11D2-A8E9-00104B365C9F}");

BOOL LoadRegExp(void)
{
	IRegExp *regexp;
	int X_retyp;
	HRESULT ComInitResult;
//	HKEY HK;

	if ( RMatch != EX_NONE ) return TRUE;

	X_retyp = GetCustDword(T("X_retyp"),0);
	if ( X_retyp < 2 ){		// BREGEXP
		if ( IsTrue(LoadRMatch()) ){
			RMatch = (DBSubst != NULL) ? EX_BREGONIG : EX_BREGEXP;
			return TRUE;
		}
		if ( X_retyp == 1 ){
			goto loaderror;
		}
	}
							// IRegExp
	if ( hOleaut32DLL == NULL ){
		hOleaut32DLL = LoadSystemWinAPI(SYSTEMDLL_OLEAUT32, OLEAUT32_SysStr);
		if ( hOleaut32DLL == NULL ) goto loaderror;

		LoadWinAPI(NULL,hOleaut32DLL,OLEAUT32_Variant,LOADWINAPI_HANDLE);
	}

	ComInitResult = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if ( FAILED(CoCreateInstance(&XCLSID_RegExp,NULL,
					CLSCTX_INPROC_SERVER,&XIID_IRegExp,(void **)&regexp)) ){
		if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
		goto loaderror;
	}
	IRegExp_Release(regexp);
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
/*
	if ( RegOpenKeyEx(HKEY_CLASSES_ROOT,VBScriptRegularExpression,0,KEY_READ,&HK) == ERROR_SUCCESS ){
		RegCloseKey(HK);
	}else{
		goto loaderror;
	}
*/
	RMatch = EX_IREGEXP;
	return TRUE;

loaderror:
	PMessageBox(GetFocus(),MES_BREX,NULL,MB_APPLMODAL | MB_OK);
	return FALSE;
}
//-----------------------------------------------------------------------------
#define MIGEMO_FREE 0
#define MIGEMO_LOADING 1
#define MIGEMO_READY 2
volatile int LoadMigemo = MIGEMO_FREE;

void WaitMigemoLoad(void)
{
	int waitcount = 1000 / 50;

	while ( LoadMigemo == MIGEMO_LOADING ){
		Sleep(50);
		if ( --waitcount == 0 ) break; // 強制待機終了
	}
}

void FreeMigemo(void)
{
	if ( LoadMigemo == MIGEMO_LOADING ) WaitMigemoLoad();
	if ( hMigemoDLL == NULL ) return;
	LoadMigemo = MIGEMO_LOADING;
	if ( migemodic != NULL ){
		migemo_close(migemodic);
		migemodic = NULL;
	}
	FreeLibrary(hMigemoDLL);
	hMigemoDLL = NULL;
	LoadMigemo = MIGEMO_FREE;
}

BOOL InitMigemo(void)
{
	TCHAR migemodicpath[MAX_PATH];

	if ( LoadMigemo == MIGEMO_READY ) return TRUE;
	WaitMigemoLoad();
	if ( LoadMigemo == MIGEMO_READY ) return TRUE;
	LoadMigemo = MIGEMO_LOADING;

	hMigemoDLL = LoadWinAPI("MIGEMO.DLL",NULL,MIGEMODLL,LOADWINAPI_LOAD);
	#ifdef _WIN64
	if ( hMigemoDLL == NULL ){
		hMigemoDLL = LoadWinAPI("MIGEMO64.DLL",NULL,MIGEMODLL,LOADWINAPI_LOAD);
	}
	#endif
	if ( hMigemoDLL != NULL ){
		TCHAR *ptr;
		#ifdef UNICODE
		char migemodicpathA[MAX_PATH];
		#else
		#define migemodicpathA migemodicpath
		#endif

		GetModuleFileName(hMigemoDLL,migemodicpath,TSIZEOF(migemodicpath));
		ptr = tstrrchr(migemodicpath,'\\') + 1;

#ifdef UNICODE
		tstrcpy(ptr, migemodicnameUTF8);
		if ( GetFileAttributes(migemodicpath) != BADATTR ){
			migemocodepage = CP_UTF8;
			migemocodeflags = 0;
		}else
#endif
		{
			tstrcpy(ptr, migemodicnameSJIS);
			if ( GetFileAttributes(migemodicpath) == BADATTR ){
				tstrcpy(ptr + 5,migemodicnameSJIS + 11); // migemo-dict のみ
				if ( GetFileAttributes(migemodicpath) == BADATTR ){
					migemodicpath[0] = '\0';
				}
			}
		}

		if ( migemodicpath[0] != '\0' ){
			#ifdef UNICODE
				UnicodeToAnsi(migemodicpath, migemodicpathA, MAX_PATH);
			#endif
			migemodic = migemo_open_fix(migemodicpathA);
			if ( migemodic != NULL ){
				GetCustData(T("X_rscah"),&X_rscah,sizeof X_rscah);
				LoadMigemo = MIGEMO_READY;
				return TRUE;
			}
		}
		PMessageBox(GetFocus(),T("migemo-dict open error"),NULL,MB_APPLMODAL | MB_OK);
	}
	LoadMigemo = MIGEMO_FREE;
	FreeMigemo();
	PMessageBox(GetFocus(),MES_MIGE,NULL,MB_APPLMODAL | MB_OK);
	return FALSE;
}

typedef struct {
	BREGEXP *bregwork;
	char migemotext[];
} SRS_BREG;

typedef struct {
	BREGONIG *boniregwork;
	TCHAR migemotext[];
} SRS_BONIREG;

typedef union { // 各種ポインタ(共用体)
	void *ptr;
	IRegExp *ireg;
	SRS_BREG *bm;
	SRS_BONIREG *bo;
} SRS_UNION;

PPXDLL BOOL PPXAPI SearchRomaString(const TCHAR *text, const TCHAR *searchstr, DWORD mode, DWORD_PTR *handles)
{
	SRS_UNION srs;
	char mesA[256];
	#ifdef UNICODE
		char textA[VFPS];
		#define TXTSTR textA
		WCHAR mes[256];
	#else
		#define TXTSTR text
		#define mes mesA
	#endif
	if ( handles == NULL ){
		return migemodic != NULL;
	}
								// 領域解放
	srs = *(SRS_UNION *)handles;
	if ( text == NULL ){
		if ( srs.ptr != NULL ){
			if ( RMatch != EX_IREGEXP ){
				if ( RMatch == EX_BREGONIG ){
					DBRegfree( srs.bo->boniregwork );
				}else{ // EX_BREGEXP
					BRegfree( srs.bm->bregwork );
				}
				HeapFree(DLLheap,0,(char *)srs.bm);
			}else{ // EX_IREGEXP
				IRegExp_Release( srs.ireg );
			}
			*handles = 0;
		}
		return TRUE;
	}
								// 新規検索
	if ( srs.ptr == NULL ){
		char *query;
		BOOL usehist = FALSE;
		BOOL usesave = FALSE;
		#ifdef UNICODE
			char searchstrA[VFPS];
			#define SSTR searchstrA
		#else
			#define SSTR searchstr
		#endif

		if ( LoadRegExp() == FALSE ) return TRUE;	// RegExp 読み込み失敗
													// Migemo を準備
		if ( InitMigemo() == FALSE ) return TRUE;

		if ( RMatch == EX_IREGEXP ){ // IRegExp
			if ( FAILED(CoCreateInstance(&XCLSID_RegExp,NULL,
					CLSCTX_INPROC_SERVER,&XIID_IRegExp,(void **)&srs.ireg)) ){
				return TRUE;
			}
		}
		if ( tstrlen(searchstr) <= X_rscah ){ // キャッシュ検索
			usesave = TRUE;
			UsePPx();
			query = (char *)SearchHistory(PPXH_ROMASTR, searchstr);
			if ( query != NULL ){
				query = (char *)GetHistoryData(query);
				usehist = TRUE;
			}
		}
		if ( usehist == FALSE ){	// Migemo で正規表現を取得する
			size_t length;
			#ifdef UNICODE
				WideCharToMultiByte(migemocodepage, 0, searchstr, -1, searchstrA, VFPS, NULL, NULL);
			#endif
			query = migemo_query(migemodic, SSTR);
			if ( query != NULL ){
				length = strlen(query);
			}else{
				if ( IsTrue(usesave) ) FreePPx();
				return FALSE;
			}
			if ( IsTrue(usesave) && length && (length < 0xff00) ){
				LimitHistory(PPXH_ROMASTR);
				WriteHistory(PPXH_ROMASTR,searchstr,(WORD)(length + 1),query);
			}
		}
													// RegExp を準備
		if ( RMatch != EX_IREGEXP ){ // BRegExp
			if ( RMatch == EX_BREGONIG ){
				size_t querylen;
				TCHAR *p;

				#ifdef UNICODE
					querylen = MultiByteToWideChar(migemocodepage, migemocodeflags, query, -1, NULL, 0);
				#else
					querylen = strlen(query);
				#endif

				srs.bo = (SRS_BONIREG *)HeapAlloc(DLLheap,0,
						sizeof(SRS_BONIREG) + (querylen + 12) * sizeof(TCHAR));
				if ( srs.bo == NULL ) return FALSE;
				*handles = (DWORD_PTR)srs.bo;
				srs.bo->boniregwork = NULL;
				p = srs.bo->migemotext;
				*p++ = '/';
				if ( !(mode & ISEA_FLOAT) ) *p++ = '^';
				#ifdef UNICODE
					wcscpy(p + MultiByteToWideChar(migemocodepage, migemocodeflags, query, -1, p, querylen) - 1,L"/i");
				#else
					strcpy(p,query);
					strcpy(p + querylen,"/ki");
				#endif
			}else{ // EX_BREGEXP
				size_t querylen;
				char *p;

				querylen = strlen(query);
				srs.bm = (SRS_BREG *)HeapAlloc(DLLheap,0,
						sizeof(SRS_BREG) + querylen + 12);
				if ( srs.bm == NULL ) return FALSE;
				*handles = (DWORD_PTR)srs.bm;
				srs.bm->bregwork = NULL;
				p = (char *)srs.bm->migemotext;
				*p++ = '/';
				if ( !(mode & ISEA_FLOAT) ) *p++ = '^';
				strcpy(p,query);
				#ifdef UNICODE
					strcpy(p + querylen, (migemocodepage == CP_UTF8) ? "/i" : "/ki");
				#else
					strcpy(p + querylen,"/ki");
				#endif
			}
		}else{ // EX_IREGEXP
			BSTR pattern;

			IRegExp_put_IgnoreCase(srs.ireg,VARIANT_TRUE);
			#ifdef UNICODE
			{
				size_t querylen;
				WCHAR *widep;

				querylen = MultiByteToWideChar(migemocodepage, migemocodeflags, query, -1, NULL, 0);
				widep = (WCHAR *)HeapAlloc(DLLheap,0,(querylen + 2) * sizeof(WCHAR));
				if ( widep == NULL ) return FALSE;
				MultiByteToWideChar(migemocodepage, migemocodeflags, query, -1, widep, querylen);
				pattern = CreateBstring(widep);
				HeapFree(DLLheap,0,widep);
			}
			#else
				pattern = CreateBstringA(query);
			#endif
			if ( FAILED(IRegExp_put_Pattern(srs.ireg,pattern)) ){
				XMessage(NULL,NULL,XM_GrERRld,IREGEXPPATTENERROR);
			}
			DSysFreeString(pattern);
		}
		if ( usehist == FALSE ) migemo_release(migemodic,query);
		if ( IsTrue(usesave) ) FreePPx();
	}
													// 検索
	if ( *text == '\0' ) return FALSE; // 空文字列
	if ( RMatch != EX_IREGEXP ){
		if ( RMatch == EX_BREGONIG ){
			return DBMatch( srs.bo->migemotext,(TCHAR *)text,
					(TCHAR *)text + tstrlen(text),&srs.bo->boniregwork,mes);
		}else{ // EX_BREGEXP
			#ifdef UNICODE
				WideCharToMultiByte(migemocodepage, 0, text, -1, textA, VFPS, NULL, NULL);
			#endif
			return BMatch( srs.bm->migemotext,TXTSTR,
					TXTSTR + strlen(TXTSTR),&srs.bm->bregwork,mesA);
		}
	}else{ // EX_IREGEXP
		VARIANT_BOOL result;
		BSTR target;

		result = 0;
		target = CreateBstring(text);
		IRegExp_Test(srs.ireg,target,&result);
		DSysFreeString(target);
		return (BOOL)result;
	}
}
//-----------------------------------------------------------------------------
BSTR CreateBstringLenA(const char *string,int size)
{
	BSTR bstring;
	UINT bsize;

	if ( size == -1 ) size = strlen32(string);
	bsize = MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,string,size,NULL,0);
	if ( size == -1 ) bsize--;
	bstring = DSysAllocStringLen(NULL,bsize);
	MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,string,size,bstring,bsize + 1);
	return bstring;
}

#define ONEDAYH 0xC9
#define ONEDAYL 0x2A69C000
#define DAY_NONE 0
#define DAY_RELATIVE 1
#define DAY_ABSOLUTELY 2

int GetWildDate(const TCHAR **string,FILETIME *ftime,SYSTEMTIME *stime)
{
	DWORD num,num2,num3;
	const TCHAR *nowp,*oldp;
	SYSTEMTIME stmptime;
	FILETIME ftmptime;
	TCHAR c;

	oldp = nowp = *string;
	num = (DWORD)GetDigitNumber(&nowp);
	if ( oldp == nowp ) return 0;
	c = SkipSpace(&nowp);
	if ( (c == '-') || (c == '/') ){ // Date 2007-1-31
		nowp++;
		oldp = nowp;
		num2 = (DWORD)GetDigitNumber(&nowp);
		if ( oldp == nowp ) return 0;
		c = SkipSpace(&nowp);
		if ( (c == '-') || (c == '/') ){ // パラメータが3つ 年-月-日
			nowp++;
			oldp = nowp;
			num3 = (DWORD)GetDigitNumber(&nowp);
			if ( oldp == nowp ) return 0;
			if ( num < 100 ) num = num < 80 ? num + 2000 : num + 1900;
			stmptime.wYear = (WORD)num;
			stmptime.wMonth = (WORD)num2;
			stmptime.wDay = (WORD)num3;
		}else{	// パラメータが2つ 月-日
			GetLocalTime(&stmptime);
			stmptime.wMonth = (WORD)num;
			stmptime.wDay = (WORD)num2;
		}
		stmptime.wHour = 0;
		stmptime.wMinute = 0;
		stmptime.wSecond = 0;
		stmptime.wMilliseconds = 0;
		if ( stime != NULL ) *stime = stmptime;
		if ( ftime != NULL ){
			SystemTimeToFileTime(&stmptime,&ftmptime);
			LocalFileTimeToFileTime(&ftmptime,ftime);
		}
		*string = nowp;
		return DAY_ABSOLUTELY;
	}else{	// ひにち相対
		DWORD dateL,dateH,tmpH,tmpHH;

		DDmul(num,ONEDAYL,&dateL,&dateH);
		DDmul(num,ONEDAYH,&tmpH,&tmpHH);
		dateH += tmpH;

		GetSystemTimeAsFileTime(ftime);
//		GetSystemTime(&stmptime);
//		SystemTimeToFileTime(&stmptime,ftime);
		SubDD(ftime->dwLowDateTime,ftime->dwHighDateTime,dateL,dateH);
		*string = nowp;
		return DAY_RELATIVE;
	}
}

// ワイルドカード処理メイン ---------------------------------------------------
/*-----------------------------------------------------------------------------
	１項目について判別

	ss:	command(srearchstring) NULL command(srearchstring) NULL ... NULL NULL
-----------------------------------------------------------------------------*/
BOOL SimpleRegularExpression(const TCHAR *src,const TCHAR *ss)
{
	if ( *ss == '\0' ) return FALSE;		//検索文字無し→無条件に不成立
	while( *ss != '\0' ){
		switch( *ss ){
			case '*': {				// n 文字の任意の文字
				size_t len;

				ss++;
				if ( *ss == '\0' ){			// 検索内容が付随せず→持ち越し
					int wild = 0;

					ss++;
					while( *ss != '\0' ){
						switch (*ss){
							case '?':
								wild++;
							case '*':
								ss++;
								break;
						}
						if ( *ss != '\0' ) break;
						ss++;
					}
					while( wild != 0 ){			// ? の数だけ文字を確保
						if ( Ismulti(*src) ) src++;
						if ( *src == '\0' ) return FALSE;
						src++;
						wild--;
					}
					if ( *ss == '\0' ) return TRUE; // 末尾の'*'だった
				}
										// 文字列の検索
				len = tstrlen32(ss);	// i > 0 のはず
				while ( *src ){
					src = tstrchr(src,*ss);
					if ( src == NULL ) return FALSE;
					if ( memcmp(src,ss,TSTROFF(len)) == 0 ){
						const TCHAR *src2,*ss2;

						src2 = src + len;
						ss2  = ss + len + 1;
						if ( *ss2 == '\0' ){
							if ( *src2 == '\0') return TRUE;
						}else{
							if ( SimpleRegularExpression(src2,ss2) ){
								return TRUE;
							}
						}
					}
					src++;
				}
				return FALSE;
			}
/*
			case '\\':				// 各種文字
				ss++;
				switch ( *ss ){
					case 'd':
						if ( !*src ) return FALSE;	// 対象文字が無い
						if ( !Isdigit(*src) ) return FALSE;
						if ( Ismulti(*src) ) src++;
						src++;
						break;
					case 'D':
						if ( !*src ) return FALSE;	// 対象文字が無い
						if ( Isdigit(*src) ) return FALSE;
						if ( Ismulti(*src) ) src++;
						src++;
						break;
					case 's':
						if ( !*src ) return FALSE;	// 対象文字が無い
						if ( *src != ' ' ) return FALSE;
						if ( Ismulti(*src) ) src++;
						src++;
						break;
					case 'S':
						if ( !*src ) return FALSE;	// 対象文字が無い
						if ( *src == ' ' ) return FALSE;
						if ( Ismulti(*src) ) src++;
						src++;
						break;
				}
				if ( *ss ) continue;
				break;
*/
			case '?':				// 1 文字の任意の文字
				ss++;
				if ( Ismulti(*src) ) src++;
				if ( *src == '\0' ) return FALSE;
				src++;
				if ( *ss == '\0' ){
					ss++;
					break;
				}
				// default へ
			default: {				// 通常の文字列
				size_t len;

				len = tstrlen(ss);
				if ( memcmp(src,ss,TSTROFF(len)) ) return FALSE;
				src += len;
				ss += len + 1;
			}
		}
	}
	return *src ? FALSE : TRUE;
}
//-----------------------------------------------------------------------------
BOOL FilenameRegularExpression2(const TCHAR *fname,const TCHAR *fext,EXS_DATA *exs)
{
	const TCHAR *ext;

	ext = (TCHAR *)(BYTE *)((BYTE *)exs->data + exs->ext);
	if ( *ext != '\0' ){	// 拡張子指定あり -------------------------
								// (ext[0] は 0 or '.' の二択しかない)
		if ( !*(ext + 1) ? !*fext :					// 空欄拡張子指定
				SimpleRegularExpression(fext,ext + 1) ){
			if ( exs->data[0] == '\0' ){	// ファイル名は何でもよい→完全一致
				return TRUE;
			}else{						// ファイル名で一致を調べる
				return SimpleRegularExpression(fname,exs->data);
			}
		}else{							// 拡張子が違う→一致しない
			return FALSE;
		}
	}else{						// 拡張子指定なし -------------------------
		if ( exs->data[0] == '\0' ){		// ファイル名は何でもよい→完全一致
			return TRUE;
		}else{						// ファイル名で一致を調べる
			return SimpleRegularExpression(fname,exs->data);
		}
	}
}
//-----------------------------------------------------------------------------
PPXDLL int PPXAPI FilenameRegularExpression(const TCHAR *src,FN_REGEXP *fn)
{
	TCHAR fname[VFPS];	// 比較対象のファイル名
	const TCHAR *fext;	// 比較対象の拡張子
	BYTE *b;
	BOOL result;
	BOOL usenot = FALSE;
	BOOL useand = FALSE;
	char mesbufA[256];
	#ifdef UNICODE
		char fbuf[VFPS];
		BOOL conved = FALSE;
		#define NAME fbuf
		WCHAR mesbuf[256];
	#else
		#define NAME src
		#define mesbuf mesbufA
	#endif

	b = fn->b;
	if ( ((EXS_DATA *)b)->ext == EX_NONE ) return 1; // 条件なし→すべてヒット
									// 比較対象の準備
	if ( ((EXS_DATA *)b)->ext == EX_JOINEXT ){ // 拡張子分離を必ずしない
		if ( tstrlen(src) >= VFPS ){
			fname[0] = '\0';
		}else{
			tstrcpy(fname,src);
			CharLower(fname);
		}
		fext = NilStr;
		b += ((EXS_DATA *)b)->next;
		if ( ((EXS_DATA *)b)->ext == EX_NONE ) return 1;
	}else{
		int extOffset;

		tstrcpy(fname,src);
		CharLower(fname);
		extOffset = FindExtSeparator(fname);
		if ( fname[extOffset] ){
			fname[extOffset++] = '\0';
		}
		fext = fname + extOffset;
	}

	for ( ; ; ){
		WORD ext;

		ext = ((EXS_DATA *)b)->ext;
		switch ( ext ){
			case EX_NONE:
				return 1;

			case EX_MEM:
				b = ((EXS_MEM *)b)->nextptr;
				continue;

			case EX_NOT:
				if ( !((EXS_DATA *)b)->next ) return 0;
				b += ((EXS_DATA *)b)->next;
				usenot = TRUE;
				continue;

			case EX_AND:
				if ( !((EXS_DATA *)b)->next ) return 0;
				b += ((EXS_DATA *)b)->next;
				useand = TRUE;
				continue;

			case EX_BREGEXP:
				if ( *src == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				#ifdef UNICODE
				if ( conved == FALSE ){
					conved = TRUE;
					UnicodeToAnsi(src,fbuf,VFPS);
				}
				#endif
				result = BMatch(((EXS_BREGEXP *)b)->data,NAME,
						NAME + strlen(NAME),
						&((EXS_BREGEXP *)b)->rxp,mesbufA);
				break;

			case EX_BREGONIG:
				if ( *src == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				result = DBMatch(((EXS_BONIREGEXP *)b)->data,(TCHAR *)src,
						(TCHAR *)src + tstrlen(src),
						&((EXS_BONIREGEXP *)b)->rxp,mesbuf);
				break;

			case EX_IREGEXP: {
				BSTR target;

				if ( *src == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				result = FALSE;
				target = CreateBstring(src);
				IRegExp_Test(((EXS_IREGEXP *)b)->iregexp,
						target,(VARIANT_BOOL *)&result);
				DSysFreeString(target);
				break;
			}

			case EX_ROMA:
				if ( ((EXS_ROMA *)b)->data[0] ){
					result = SearchRomaString(src,
							((EXS_ROMA *)b)->data,
							ISEA_FLOAT,
							&((EXS_ROMA *)b)->handle);
				}else{
					result = TRUE;
				}
				break;

			case EX_DIR:
			case EX_ATTR:
			case EX_WRITEDATEABS:
			case EX_WRITEDATEREL:
			case EX_CREATEDATEABS:
			case EX_CREATEDATEREL:
			case EX_ACCESSDATEABS:
			case EX_ACCESSDATEREL:
			case EX_SIZE:
				XMessage(NULL,NULL,XM_GrERRld,T("This matching don't support 'xxx:'."));
				result = FALSE;
				break;

			default:
				if ( ext >= EX_BOTTOM ){ // 不明コード…無視する
					result = FALSE;
					break;
//					b += ((EXS_DATA *)b)->next;
//					continue;
				}
				result = FilenameRegularExpression2(fname,fext,(EXS_DATA *)b);
				break;
		}
		{
			WORD nextsize;

			nextsize = ((EXS_DATA *)b)->next;

			if ( IsTrue(usenot) ){
				usenot = FALSE;
				useand = FALSE;
				if ( IsTrue(result) ) return 0;
				if ( nextsize == 0 ) return 1;
			}else{
				if ( IsTrue(useand) ){
					useand = FALSE;
					if ( result == FALSE ) return 0;
					if ( nextsize == 0 ) return 1;
				}else{
					if ( IsTrue(result) ) return 1;
					if ( nextsize == 0 ) return 0;
				}
			}
			b += nextsize;
		}
	}
}

BOOL CompareValue(int cp,int type)
{
	switch ( type ){
		case TYPE_EQUAL: // ==
			return (cp == 0);
		case TYPE_SMALL: // <
			return (cp < 0);
		case TYPE_LARGE: // >
			return (cp > 0);
		case TYPE_EQUAL | TYPE_SMALL: // <=
			return (cp <= 0);
		case TYPE_EQUAL | TYPE_LARGE: // >=
			return (cp >= 0);
		case TYPE_SMALL | TYPE_LARGE: // <> , ><
		case TYPE_EQUAL | TYPE_SMALL | TYPE_LARGE: // !=
			return cp;
		default:
			return 0;
	}
}

BOOL USEFASTCALL RelDateCompareValue(EXS_WRITEDATEREL *dr,const FILETIME *datetime)
{
	return CompareValue(FuzzyCompareFileTime(&dr->days,datetime),dr->type);
}

#define ABSDATEVALUE(ftime) ((ftime.wYear * 512) + (ftime.wMonth * 32) + ftime.wDay)
BOOL USEFASTCALL AbsDateCompareValue(EXS_WRITEDATEABS *da,const FILETIME *datetime)
{
	SYSTEMTIME stime;
	FILETIME lfime;

	FileTimeToLocalFileTime(datetime,&lfime);
	FileTimeToSystemTime(&lfime,&stime);
	return CompareValue(ABSDATEVALUE(da->date) - ABSDATEVALUE(stime),da->type);
}

//-----------------------------------------------------------------------------
PPXDLL int PPXAPI FinddataRegularExpression(const WIN32_FIND_DATA *ff,FN_REGEXP *fn)
{
	TCHAR fname[VFPS];	// 比較対象のファイル名
	const TCHAR *fext;	// 比較対象の拡張子
	BYTE *b;
	BOOL result;
	BOOL usenot = FALSE;
	BOOL useand = FALSE;
	char mesbufA[256];
	#ifdef UNICODE
		char fbuf[VFPS];
		BOOL conved = FALSE;
		#define NAMED fbuf
		WCHAR mesbuf[256];
	#else
		#define NAMED ff->cFileName
		#define mesbuf mesbufA
	#endif

	b = fn->b;
	if ( ((EXS_DATA *)b)->ext == EX_NONE ) return 1; // 条件なし→すべてヒット
									// 比較対象の準備
	{
		const TCHAR *src;
		TCHAR *dest;

		src = ff->cFileName;
		dest = fname;
		for (;;){
			#ifdef UNICODE
				WCHAR chr;

				chr = *src++;
				if ( chr == '\0' ) break;
				*dest++ = chr;
				if ( (chr == '/') || (chr == '\\') ) dest = fname;
			#else
				char chr,chrtype;

				chr = *src++;
				if ( chr == '\0' ) break;
				chrtype = T_CHRTYPE[chr];
				*dest++ = chr;
				if ( chrtype & T_IS_KNJ ) *dest++ = *src++;
				if ( (chr == '/') || (chr == '\\') ) dest = fname;
			#endif
		}
		*dest = '\0';
		CharLower(fname);
	}

	if ( ((EXS_DATA *)b)->ext == EX_JOINEXT ){ // 拡張子分離を必ずしない
		fext = NilStr;
		b += ((EXS_DATA *)b)->next;
		if ( ((EXS_DATA *)b)->ext == EX_NONE ) return 1;
	}else{	// ファイルの時だけ拡張子分離
		if ( !(ff->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			int extOffset;

			extOffset = FindExtSeparator(fname);
			if ( fname[extOffset] != '\0' ) fname[extOffset++] = '\0';
			fext = fname + extOffset;
		}else{
			fext = NilStr;
		}
	}
	for ( ; ; ){
		WORD ext;

		ext = ((EXS_DATA *)b)->ext;
		switch ( ext ){
			case EX_NONE:
				return 1;

			case EX_MEM:
				b = ((EXS_MEM *)b)->nextptr;
				continue;

			case EX_DIR:
				if ( !(ff->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
					// ファイルは次の判定を無視する
					for ( ; ; ){
						if ( !((EXS_DATA *)b)->next ) return 0;
						b += ((EXS_DATA *)b)->next;
						if ( ((EXS_DATA *)b)->ext == EX_MEM ){
							b = ((EXS_MEM *)b)->nextptr;
							continue;
						}
						break;
					}
					// b は次の判定処理をポイントしている→次のでスキップ
				}
				// ディレクトリ→ディレクトリ判定になる
				if ( !((EXS_DATA *)b)->next ) return 0;
				b += ((EXS_DATA *)b)->next;
				if ( ((EXS_DATA *)b)->ext == EX_MEM ){
					b = ((EXS_MEM *)b)->nextptr;
				}
				continue;

			case EX_NOT:
				if ( !((EXS_DATA *)b)->next ) return 0;
				b += ((EXS_DATA *)b)->next;
				usenot = TRUE;
				continue;

			case EX_AND:
				if ( !((EXS_DATA *)b)->next ) return 0;
				b += ((EXS_DATA *)b)->next;
				useand = TRUE;
				continue;

			case EX_BREGEXP:
				if ( ff->cFileName[0] == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				#ifdef UNICODE
				if ( conved == FALSE ){
					conved = TRUE;
					UnicodeToAnsi(ff->cFileName,fbuf,VFPS);
				}
				#endif
				result = BMatch(((EXS_BREGEXP *)b)->data,
						NAMED,
						NAMED + strlen(NAMED),
						&((EXS_BREGEXP *)b)->rxp,mesbufA);
				break;

			case EX_BREGONIG:
				if ( ff->cFileName[0] == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				result = DBMatch(((EXS_BONIREGEXP *)b)->data,
						(TCHAR *)ff->cFileName,
						(TCHAR *)ff->cFileName + tstrlen(ff->cFileName),
						&((EXS_BONIREGEXP *)b)->rxp,mesbuf);
				break;

			case EX_IREGEXP: {
				BSTR target;

				if ( ff->cFileName[0] == '\0' ){ // 文字列が空の時
					result = 0;
					break;
				}
				result = FALSE;
				target = CreateBstring(ff->cFileName);
				IRegExp_Test(((EXS_IREGEXP *)b)->iregexp,target,
						(VARIANT_BOOL *)&result);
				DSysFreeString(target);
				break;
			}

			case EX_ROMA:
				if ( ((EXS_ROMA *)b)->data[0] ){
					result = SearchRomaString(ff->cFileName,
							((EXS_ROMA *)b)->data,
							ISEA_FLOAT,
							&((EXS_ROMA *)b)->handle);
				}else{
					result = TRUE;
				}
				break;

			case EX_ATTR:
				result = (ff->dwFileAttributes &
						((EXS_ATTR *)b)->mask) == ((EXS_ATTR *)b)->value;
				break;

			case EX_WRITEDATEREL:
				result = RelDateCompareValue(
						(EXS_WRITEDATEREL *)b,&ff->ftLastWriteTime);
				break;
			case EX_CREATEDATEREL:
				result = RelDateCompareValue(
						(EXS_WRITEDATEREL *)b,&ff->ftCreationTime);
				break;
			case EX_ACCESSDATEREL:
				result = RelDateCompareValue(
						(EXS_WRITEDATEREL *)b,&ff->ftLastAccessTime);
				break;

			case EX_WRITEDATEABS:
				result = AbsDateCompareValue(
						(EXS_WRITEDATEABS *)b,&ff->ftLastWriteTime);
				break;
			case EX_CREATEDATEABS:
				result = AbsDateCompareValue(
						(EXS_WRITEDATEABS *)b,&ff->ftCreationTime);
				break;
			case EX_ACCESSDATEABS:
				result = AbsDateCompareValue(
						(EXS_WRITEDATEABS *)b,&ff->ftLastAccessTime);
				break;

			case EX_SIZE: {
				int cp;

				cp = ff->nFileSizeHigh - ((EXS_SIZE *)b)->sizeH;
				if ( !cp ) cp = ff->nFileSizeLow - ((EXS_SIZE *)b)->sizeL;
				result = CompareValue(cp,((EXS_SIZE *)b)->type);
				break;
			}

			default:
				if ( ext >= EX_BOTTOM ){ // 不明コード…無視する
					result = FALSE;
					break;
//					b += ((EXS_DATA *)b)->next;
//					continue;
				}
				result = FilenameRegularExpression2(fname,fext,(EXS_DATA *)b);
				break;
		}
		{
			WORD nextsize;

			nextsize = ((EXS_DATA *)b)->next;

			if ( IsTrue(usenot) ){
				usenot = FALSE;
				useand = FALSE;
				if ( result ) return 0;
				if ( nextsize == 0 ) return 1;
			}else{
				if ( IsTrue(useand) ){
					useand = FALSE;
					if ( !result ) return 0;
					if ( nextsize == 0 ) return 1;
				}else{
					if ( result ) return 1;
					if ( nextsize == 0 ) return 0;
				}
			}
			b += nextsize;
		}
	}
}
// ワイルドカードの保存内容を破棄 ---------------------------------------------
PPXDLL void PPXAPI FreeFN_REGEXP(FN_REGEXP *fn)
{
	BYTE *p,*allocptr = NULL;

	p = fn->b;
	for ( ; ; ){
		switch( ((EXS_DATA *)p)->ext ){
			case EX_MEM:
				p = allocptr = ((EXS_MEM *)p)->nextptr;
				continue;

			case EX_BREGEXP:
				BRegfree( ((EXS_BREGEXP *)p)->rxp );
				break;

			case EX_BREGONIG:
				DBRegfree( ((EXS_BONIREGEXP *)p)->rxp );
				break;

			case EX_IREGEXP:
				IRegExp_Release( ((EXS_IREGEXP *)p)->iregexp );
				break;

			case EX_ROMA:
				SearchRomaString(NULL,NULL,0,&((EXS_ROMA *)p)->handle);
				break;
		}

		if ( ((EXS_DATA *)p)->next == 0 ) break;
		p += ((EXS_DATA *)p)->next;
	}
	if ( allocptr != NULL ) HeapFree(DLLheap,0,allocptr);
}

TCHAR *SearchSeparater(const TCHAR *text,TCHAR separater)
{
	const TCHAR *ptr = text;
	TCHAR sep = separater;

	for (;;){
		TCHAR chr;

		chr = *ptr;
		if ( chr == '\0' ) return NULL;
		if ( chr == sep ) return (TCHAR *)ptr;
		if ( chr != '\\' ){
			#ifdef UNICODE
				ptr++;
			#else
				ptr += Chrlen(chr);
			#endif
			continue;
		}
		if ( *(ptr + 1) == '\0' ) return NULL;
		ptr += 2;
	}
}

// ワイルドカード内容保存ができるかチェック、必要なら拡張。--------------------
BOOL CheckExsmem(_In_ EXSMEM *exm,DWORD size)
{
									// EXS_MEM が確保できるようにする
	while ( exm->left < (size + sizeof(EXS_MEM) ) ){
		if ( exm->alloced == NULL ){ // 新規
			exm->alloced = HeapAlloc(DLLheap,0,EXSMEMALLOCSIZE);
			if ( exm->alloced == NULL ) return FALSE;
			exm->fixptr = &((EXS_MEM *)exm->dest)->nextptr;
			((EXS_MEM *)exm->dest)->ext = EX_MEM;
			((EXS_MEM *)exm->dest)->next = 1;
			exm->dest = exm->alloced;
			exm->size = exm->left = EXSMEMALLOCSIZE;
		}else{
			BYTE *newptr;

			newptr = HeapReAlloc(DLLheap,0,
					exm->alloced,exm->size + EXSMEMALLOCSIZE);
			if ( newptr == NULL ) return FALSE;
			exm->dest = newptr + (exm->dest - exm->alloced);
			exm->alloced = newptr;
			exm->size += EXSMEMALLOCSIZE;
			exm->left += EXSMEMALLOCSIZE;
		}
		*exm->fixptr = exm->alloced;
	}
	return TRUE;
}

// 正規表現を取得 -------------------------------------------------------------
BOOL RegExpWildCard(EXSMEM *exm,const TCHAR **src)
{
	char *termp;
	const TCHAR *lastsep;
	DWORD size,structsize;
	BOOL IgnoreCase = TRUE;

	if ( LoadRegExp() == FALSE ) return FALSE;	// DLL 読み込み失敗

	lastsep = SearchSeparater(*src + 1,'/');	// 区切りがあるか？
	if ( lastsep != NULL ){
		if ( (size = ToSIZE32_T(lastsep - *src)) == 0 ) return FALSE;	// '/' しか無かった
		lastsep++;
		if ( *lastsep == 'I' ){
			IgnoreCase = FALSE;
			lastsep++;
		}
	}else{
		if ( (size = tstrlen32(*src)) == 0 ) return FALSE;	// '/' しか無かった
		lastsep = *src + size; // 最後を示すように
	}

	if ( RMatch != EX_IREGEXP ){
		if ( RMatch == EX_BREGONIG ){
			// 4 = ("/ki" + '\0')TCHAR
			structsize = sizeof(EXS_BONIREGEXP) + (size * sizeof(TCHAR)) + 4 * sizeof(TCHAR);
			if ( CheckExsmem(exm,structsize) == FALSE ) return FALSE;

			((EXS_BONIREGEXP *)(exm->dest))->ext = EX_BREGONIG;
			((EXS_BONIREGEXP *)(exm->dest))->next = (WORD)structsize;
			((EXS_BONIREGEXP *)(exm->dest))->rxp = NULL;
			memcpy(((EXS_BONIREGEXP *)(exm->dest))->data,*src,size * sizeof(TCHAR));
			tstrcpy( ((EXS_BONIREGEXP *)(exm->dest))->data + size ,
					IgnoreCase ? T("/ki") : T("/k") );
		}else{ // EX_BREGEXP
			// 4 = ("/ki" + '\0')ANSI
			// UNICODE の時は size は概算(多め)の文字列の長さとする
			structsize = sizeof(EXS_BREGEXP) + (size * sizeof(char)) + 4;
			if ( CheckExsmem(exm,structsize) == FALSE ) return FALSE;

			((EXS_BREGEXP *)(exm->dest))->ext = EX_BREGEXP;
			((EXS_BREGEXP *)(exm->dest))->next = (WORD)structsize;
			((EXS_BREGEXP *)(exm->dest))->rxp = NULL;
			#ifdef UNICODE
				termp = ((EXS_BREGEXP *)(exm->dest))->data +
							WideCharToMultiByte(CP_ACP,0,*src,size,
						 ((EXS_BREGEXP *)(exm->dest))->data,
						 size * sizeof(TCHAR),NULL,NULL);
			#else
				memcpy(((EXS_BREGEXP *)(exm->dest))->data,*src,size);
				termp = ((EXS_BREGEXP *)(exm->dest))->data + size;
			#endif
			strcpy(termp,IgnoreCase ? "/ki" : "/k" );
		}
	}else{	// EX_IREGEXP
		BSTR pattern;
		IRegExp *regexp;

		if ( CheckExsmem(exm,sizeof(EXS_IREGEXP)) == FALSE ) return FALSE;

		if ( FAILED(CoCreateInstance(&XCLSID_RegExp,NULL,
				CLSCTX_INPROC_SERVER,&XIID_IRegExp,(void **)&regexp)) ){
			return FALSE;
		}
		((EXS_IREGEXP *)(exm->dest))->ext = EX_IREGEXP;
		((EXS_IREGEXP *)(exm->dest))->next = (WORD)sizeof(EXS_IREGEXP);
		((EXS_IREGEXP *)(exm->dest))->iregexp = regexp;

		if ( IgnoreCase ) IRegExp_put_IgnoreCase(regexp,VARIANT_TRUE);
		pattern = CreateBstringLen(*src + 1,size - 1);
		if ( FAILED(IRegExp_put_Pattern(regexp,pattern)) ){
			XMessage(NULL,NULL,XM_GrERRld,IREGEXPPATTENERROR);
			IRegExp_Release(regexp);
			DSysFreeString(pattern);
			return FALSE;
		}
		DSysFreeString(pattern);
	}
	*src = lastsep;
	return TRUE;
}

int GetCompareType(const TCHAR **p)
{
	int type = 0;

	SkipSpace(p);
	for ( ; ; (*p)++ ){
		switch (**p){
			case '=':
				setflag(type,TYPE_EQUAL);
				continue;
			case '!':
				setflag(type,TYPE_EQUAL | TYPE_SMALL | TYPE_LARGE);
				continue;
			case '<':
				setflag(type,TYPE_SMALL);
				continue;
			case '>':
				setflag(type,TYPE_LARGE);
				continue;
		}
		break;
	}
	return type;
}
/*------------------------------------------- スペシャルキャラクタを抽出する
		abcdef*ght? -> abcdef *ght ? に分割
---------------------------------------------------------------------------*/
TCHAR *SplitWordExpression(TCHAR *dst,const TCHAR *src)
{
	TCHAR c;
	const TCHAR *s;

	*dst++ = '*';
	while ( *src != '\0' ){
		SIZE32_T ms;

		s = src + 1;
		for (;;){
			c = *s;
			if ( (c == '\0') || (c == '*') || (c == '?') ) break;
			s++;
		}
		ms = ToSIZE32_T(s - src);
		memcpy(dst,src,TSTROFF(ms));
		dst += ms;
		*dst++ = '\0';

		*dst++ = '*';
		if ( *s == '\0' ) *dst++ = '\0';
		src = s;
	}
	*dst++ = '\0';
	return dst;
}

TCHAR *SplitRegularExpression(TCHAR *dst,const TCHAR *src)
{
	TCHAR c;
	const TCHAR *s;

	while ( *src != '\0' ){
		SIZE32_T ms;

		s = src + 1;
		for (;;){
			c = *s;
			if ( (c == '\0') || (c == '*') || (c == '?') ) break;
			s++;
		}
		ms = ToSIZE32_T(s - src);
		memcpy(dst,src,TSTROFF(ms));
		dst += ms;
		*dst++ = '\0';
		src = s;
	}
	*dst++ = '\0';
	return dst;
}

//---------------------------------------- ファイル判別用に中間コードを生成する
PPXDLL DWORD PPXAPI MakeFN_REGEXP(FN_REGEXP *fn,const TCHAR *src)
{
	EXSMEM exm;
	MAKEFNSTRUCT mfs;
	TCHAR sepchar;
									// 指定無し→無条件で該当する
	if ( *src == '\0' ){
		((EXS_DATA *)fn->b)->ext = EX_NONE;
		((EXS_DATA *)fn->b)->next = 0;
		return REGEXPF_BLANK;
	}
									// 初期化
	exm.dest = fn->b;
	exm.alloced = NULL;
	exm.size = exm.left = sizeof(FN_REGEXP);

	mfs.flags = 0;
	mfs.useext = TRUE;
	sepchar = ';';
									// 検索開始
	for ( ; ; ){
		switch ( SkipSpace(&src) ){
			case '!':	// not 識別子
				src++;
				if ( *src == '\0' ) goto error;
				if ( CheckExsmem(&exm,sizeof(EXS_NOT)) == FALSE ) goto error;
				((EXS_NOT *)exm.dest)->ext = EX_NOT;
				((EXS_NOT *)exm.dest)->next = sizeof(EXS_NOT);
				exm.dest += sizeof(EXS_NOT);
				exm.left -= sizeof(EXS_NOT);
				continue;

			case '&':	// and 識別子
				src++;
				if ( *src == '\0' ) goto error;
				if ( CheckExsmem(&exm,sizeof(EXS_AND)) == FALSE ) goto error;
				((EXS_AND *)exm.dest)->ext = EX_AND;
				((EXS_AND *)exm.dest)->next = sizeof(EXS_AND);
				exm.dest += sizeof(EXS_AND);
				exm.left -= sizeof(EXS_AND);
				continue;

			case '/': // 正規表現
				if ( RegExpWildCard(&exm,&src) == FALSE ) goto error;
				break;

			default: {
				const TCHAR *nextsrc;
				DWORD extoffset, extrasize = 0;
				TCHAR *namebufdest;

				mfs.param = NULL;
										// １回分の解析文字列を準備 -----------
				namebufdest = mfs.name;
				nextsrc = src;
				for ( ;; ){
					TCHAR c;

					c = *nextsrc;
					if ( (c == '\0') || (c == ',') || (c == sepchar) ){
						break;
					}else if ( (c == '?') || (c == '*') ){
						extrasize += (mfs.flags & REGEXPF_WORDMATCH) ?
								sizeof(TCHAR) * 2 : sizeof(TCHAR);
					}else if ( (c == ':') && (mfs.param == NULL) ){ // option
						mfs.param = namebufdest + 1;
					}else if ( c == '\\' ){
						TCHAR cnext;

						cnext = *(nextsrc + 1);
						if ( (cnext == ',') || (cnext == ';') ){
							c = cnext;
							nextsrc++;
						}
					}
					*namebufdest++ = c;
					nextsrc++;
					if ( namebufdest < (mfs.name + VFPS) ) continue;
					if ( !(mfs.flags & REGEXPF_SILENTERROR) ){
						xmessage(XM_FaERRd,T("wildcard too long"));
					}
					goto error;
				}
				*namebufdest = '\0';

				if ( mfs.param != NULL ){ // オプションを取得
					int extresult;

					src = nextsrc;
					extresult = CheckExtMode(&exm,&mfs);
					if ( extresult == CEM_ERROR ) goto error;
					if ( mfs.flags & REGEXPF_WORDMATCH ) sepchar = ' ';
					if ( extresult > 0 /* CEM_COMP */ ) break;
				}

				CharLower(mfs.name);

				if ( mfs.flags & REGEXPF_WORDMATCH ){
					extrasize +=
						TSTROFF(1 + 2) + // ファイル名部 部分一致時に使う*の分
						TSTROFF(1 + 2);  // 拡張子部分一致時に使う「*」の分

					if ( *nextsrc == ' ' ){
						const TCHAR *tmpsrc;

						tmpsrc = nextsrc + 1;
						while ( *tmpsrc == ' ' ) tmpsrc++;
						if ( *tmpsrc != '\0' ){
							if ( (*tmpsrc == 'O') && (*(tmpsrc+1) == 'R') && (*(tmpsrc+2) == ' ') ){
								nextsrc = tmpsrc + 2;
							}else{
								if ( CheckExsmem(&exm,sizeof(EXS_AND)) == FALSE ) goto error;
								((EXS_AND *)exm.dest)->ext = EX_AND;
								((EXS_AND *)exm.dest)->next = sizeof(EXS_AND);
								exm.dest += sizeof(EXS_AND);
								exm.left -= sizeof(EXS_AND);
							}
						}
					}
				}

				// ※ファイル名部分と拡張子部分をまとめて算出し、'\0'*2分を付加
				if ( CheckExsmem(&exm,
						sizeof(EXS_DATA) + // ヘッダサイズ
						TSTROFF(namebufdest - mfs.name) + extrasize + // 文字列長
									// ファイル名部
						TSTROFF(1 + 1) + // 文字列Nil + 文字列群の末尾
									// 拡張子部
						TSTROFF(1 + 1) // 文字列Nil + 文字列群の末尾
					 ) == FALSE ){


					goto error;
				}
										// 解析 -------------------------------
										// ファイル名と拡張子を分離
				if ( IsTrue(mfs.useext) ){
					const TCHAR *extp;
					extp = tstrrchr(mfs.name,'.');
					extoffset = (extp != NULL) ? ToSIZE32_T(extp - mfs.name) : tstrlen32(mfs.name);
					mfs.name[extoffset] = '\0';
				}else{
					extoffset = tstrlen32(mfs.name);
				}
										// ファイル名部分を解析
				if ( mfs.flags & REGEXPF_WORDMATCH ){
					mfs.param = SplitWordExpression(((EXS_DATA *)exm.dest)->data,mfs.name);
				}else{
					mfs.param = SplitRegularExpression(((EXS_DATA *)exm.dest)->data,mfs.name);
				}

				((EXS_DATA *)exm.dest)->ext =
						(WORD)TSTROFF(mfs.param - ((EXS_DATA *)exm.dest)->data);
				if ( *(src + extoffset) == '.' ){
					*mfs.param++ = '.';
					extoffset++;
				}
										// 拡張子部分を解析
				mfs.param = SplitRegularExpression(mfs.param,mfs.name + extoffset);
				((EXS_DATA *)exm.dest)->next = (WORD)((BYTE *)mfs.param - exm.dest);
				src = nextsrc;
			}
		}
		if ( (*src == '\0') || ((*src != ',') && (*src != sepchar)) ) break;
		while ( (*src == ',') || (*src == sepchar) ) src++;
		if ( *src == '\0' ) break;

		exm.left -= ((EXS_DATA *)exm.dest)->next;
		exm.dest += ((EXS_DATA *)exm.dest)->next;
	}							// 末端処理
	((EXS_DATA *)exm.dest)->next = 0;
//	XMessage(NULL,NULL,XM_DUMP,(char *)fn->b,64);
	return mfs.flags;
error:
	((EXS_DATA *)exm.dest)->ext = EX_NONE;
	((EXS_DATA *)exm.dest)->next = 0;
	return REGEXPF_ERROR;
}
//-----------------------------------------------------------------------------
typedef struct {
	// bregexp / bonigexp
	BREGEXP *bregp;
	BREGONIG *bonip;
	TCHAR *string_rb; // breg/boni 用の x/pattern/replace/ 保存場所
	// IRegExp
	IRegExp *iregexp;
	BSTR string_ri; // iregexp 用の replace 保存場所
	// match
	TCHAR *string_rh;

	HRESULT ComInitResult;

#define RXMODE_BREGEXP_S 0	// s/.../		bregexp
#define RXMODE_BREGEXP_TR 1	// tr/.../		bregexp
#define RXMODE_BREGEXP_H 2	// f/.../		bregexp
#define RXMODE_BREGEXP_MAX 2

#define RXMODE_IREGEXP_MIN 3	// iregexp
#define RXMODE_IREGEXP_S 3		// s/.../
//#define RXMODE_BREGEXP_TR 4	// tr/.../ 未対応
#define RXMODE_IREGEXP_H 5		// f/.../
	int mode;
} RXPREPLACESTRING;

#define Issymbol(chr) ( Isgraph(chr) && !(T_CHRTYPE[(unsigned char)(chr)] & (T_IS_DIG | T_IS_UPP | T_IS_LOW)) )

size_t RegularExpressionMatch_GetItem(RXPREPLACESTRING *rxps,IMatchCollection *MatchCollection,TCHAR *dest,int itemno)
{
	if ( rxps->mode <= RXMODE_BREGEXP_MAX ){	// BRegExp
		if ( DBMatch != NULL ){ // bregonig
			if ( rxps->bonip->nparens < itemno ) return 0;
			tstrcpy(dest,rxps->bonip->startp[itemno]);
			return rxps->bonip->endp[itemno] - rxps->bonip->startp[itemno];

		}else{
#ifdef UNICODE
			char renameA[VFPS];

			if ( rxps->bregp->nparens < itemno ) return 0;
			strcpy(renameA,rxps->bregp->startp[itemno]);
			renameA[rxps->bregp->endp[itemno] - rxps->bregp->startp[itemno]] = '\0';
			return AnsiToUnicode(renameA,dest,VFPS);
#else
			if ( rxps->bregp->nparens < itemno ) return 0;
			tstrcpy(dest,rxps->bregp->startp[itemno]);
			return rxps->bregp->endp[itemno] - rxps->bregp->startp[itemno];
#endif
		}
	}else{
		IDispatch *dispatch;
		int len = 0;

		if ( FAILED(MatchCollection->lpVtbl->get_Item(MatchCollection,0/*group no*/,&dispatch)) ){
			return 0;
		}

		if ( itemno == 0 ){
			IMatch *match;
			BSTR result;

			if ( FAILED(dispatch->lpVtbl->QueryInterface(dispatch,&XIID_IMatch,(void**)&match)) ){
				return 0;
			}
			dispatch->lpVtbl->Release(dispatch);
			if ( SUCCEEDED(match->lpVtbl->get_Value(match,&result)) ){
				#ifdef UNICODE
					tstrcpy(dest,result);
					len = tstrlen32(dest);
				#else
					UnicodeToAnsi(result,dest,VFPS);
					len = tstrlen(dest);
				#endif
				DSysFreeString(result);
			}
			match->lpVtbl->Release(match);
		}else{
			IMatch2 *match2;
			ISubMatches *submatches;
			VARIANT value;

			if ( FAILED(dispatch->lpVtbl->QueryInterface(dispatch,&XIID_IMatch2,(void**)&match2)) ){
				return 0;
			}
			dispatch->lpVtbl->Release(dispatch);

			match2->lpVtbl->get_SubMatches(match2,&dispatch);
			dispatch->lpVtbl->QueryInterface(dispatch,&XIID_ISubMatches,(void**)&submatches);
			dispatch->lpVtbl->Release(dispatch);

			DVariantInit(&value);
			if ( SUCCEEDED(submatches->lpVtbl->get_Item(submatches,itemno - 1,&value)) ){
				#ifdef UNICODE
					tstrcpy(dest,V_BSTR(&value));
					len = tstrlen32(dest);
				#else
					UnicodeToAnsi(V_BSTR(&value),dest,VFPS);
					len = tstrlen(dest);
				#endif
				DVariantClear(&value);
			}
			submatches->lpVtbl->Release(submatches);
			match2->lpVtbl->Release(match2);
		}
		return len;
	}
}

void RegularExpressionMatch_Result(RXPREPLACESTRING *rxps,IMatchCollection *Matchs,TCHAR *target)
{
	const TCHAR *fmtptr;
	TCHAR *dest;
	TCHAR *destmax;

	dest = target;
	fmtptr = rxps->string_rh;
	destmax = dest + VFPS - 1;
	if ( fmtptr[0] == '\0' ){
		dest += RegularExpressionMatch_GetItem(rxps,Matchs,dest,0);
	}else{
		for ( ; *fmtptr != '\0' ; ){
			if ( (*fmtptr == '$') && Isdigit(*(fmtptr + 1)) ){
				dest += RegularExpressionMatch_GetItem(rxps,Matchs,dest,*(fmtptr + 1) - '0');
				fmtptr += 2;
			}else{
				if ( *fmtptr == '\\' ) fmtptr++;
				#ifndef UNICODE
					if ( Iskanji(*fmtptr) ) *dest++ = *fmtptr++;
				#endif
				*dest++ = *fmtptr++;
			}
			if ( dest >= destmax ) break;
		}
	}
	*dest = '\0';
}

BOOL RegularExpressionMatch_bregexp(RXPREPLACESTRING *rxps,TCHAR *target)
{
	TCHAR msg[VFPS];

	if ( DBMatch != NULL ){ // bregonig
		int result;

		msg[0] = '\0';
		result = DBMatch(rxps->string_rb,target,target + tstrlen(target),
				&rxps->bonip,msg); // msg は未利用
		if ( result ){
			if( rxps->bonip == NULL ){
				target[0] = '\0';
			}else{
				RegularExpressionMatch_Result(rxps,NULL,msg);
				tstrcpy(target,msg);
			}
		}
	}else{ // bregexp
		int result;
#ifdef UNICODE
		char targetA[VFPS];
		char renameA[VFPS];

		UnicodeToAnsi(target,targetA,VFPS);
		UnicodeToAnsi(rxps->string_rb,renameA,VFPS);
#else
		#define targetA target
		#define renameA rxps->string_rb
#endif
		msg[0] = '\0';
		result = BMatch(renameA,targetA,targetA + strlen(targetA),
				&rxps->bregp,(char *)msg); // msg は未利用
		if ( result ){
			if( rxps->bregp == NULL ){
				target[0] = '\0';
			}else{
				RegularExpressionMatch_Result(rxps,NULL,msg);
				tstrcpy(target,msg);
			}
		}
	}
	return TRUE;
}

BOOL RegularExpressionMatch_iregexp(RXPREPLACESTRING *rxps,TCHAR *target)
{
	BSTR btarget;
	IDispatch *RegDispatch = NULL;
	IMatchCollection *Matchs;

	btarget = CreateBstring(target);
	if ( FAILED(IRegExp_Execute(rxps->iregexp,btarget,&RegDispatch)) ){
		XMessage(NULL,NULL,XM_GrERRld,T("IRegExp error"));
		DSysFreeString(btarget);
		return FALSE;
	}
	DSysFreeString(btarget);
	target[0] = '\0';
	if ( SUCCEEDED(RegDispatch->lpVtbl->QueryInterface(RegDispatch,&XIID_IMatchCollection,(void **)&Matchs)) ){
		RegularExpressionMatch_Result(rxps,Matchs,target);
		Matchs->lpVtbl->Release(Matchs);
	}
	RegDispatch->lpVtbl->Release(RegDispatch);
	return TRUE;
}

BOOL InitRegularExpressionReplace(RXPREPLACESTRING **rxpsptr,TCHAR *rxstring,BOOL slash)
{
	TCHAR *slist,*rlist,separater = '/';
	const TCHAR *option = defRepOpt,*preope = preope_search;
	TCHAR buf[VFPS];
	RXPREPLACESTRING *rxps;

	if ( LoadRegExp() == FALSE ) return FALSE;
	rxps = HeapAlloc(DLLheap,0,sizeof(RXPREPLACESTRING));
	if ( rxps == NULL ) return FALSE;
	rxps->mode = RXMODE_BREGEXP_S;
	// 正規表現のコマンド & 左端セパレータをチェックする
	{
		TCHAR *ptr;

		ptr = rxstring;
		if ( Issymbol(*ptr) ){
			if ( (slash == FALSE) || (*ptr == '/') ){
				separater = *ptr++;
			}
		}else if ( (*ptr == 's') && Issymbol(*(ptr + 1)) ){
			if ( (slash == FALSE) || (*(ptr + 1) == '/') ){
				separater = *(ptr + 1);
				ptr += 2;
			}
		}else if ( (*ptr == 'y') && Issymbol(*(ptr + 1)) ){
			if ( (slash == FALSE) || (*(ptr + 1) == '/') ){
				separater = *(ptr + 1);
				ptr += 2;
				rxps->mode = RXMODE_BREGEXP_TR;
				preope = preope_trans;
			}
		}else if ( (*ptr == 'h') && Issymbol(*(ptr + 1)) ){
			if ( (slash == FALSE) || (*(ptr + 1) == '/') ){
				separater = *(ptr + 1);
				ptr += 2;
				rxps->mode = RXMODE_BREGEXP_H;
				rxps->string_rh = NULL;
				preope = preope_match;
			}
		}else if((*ptr == 't') && (*(ptr + 1) == 'r') && Issymbol(*(ptr + 2))){
			if ( (slash == FALSE) || (*(ptr + 2) == '/') ){
				separater = *(ptr + 2);
				ptr += 3;
				rxps->mode = RXMODE_BREGEXP_TR;
				preope = preope_trans;
			}
		}
		slist = ptr;
	}

	rlist = SearchSeparater(slist,separater); // 中央(右端)セパレータ
	if ( rlist == NULL ) goto noparamerror;
	*rlist++ = '\0';
	{
		TCHAR *p = SearchSeparater(rlist,separater); // 右端セパレータ
		if ( p != NULL ){
			*p = '\0';
			option = p + 1;
											// 余分なセパレータがないか確認
			if ( tstrchr(option,separater) != NULL ){
				XMessage(NULL,NULL,XM_GrERRld,T("RegExp: Bad separater"));
				goto error;
			}
		}
	}
	if ( rxps->mode != RXMODE_BREGEXP_S ){
		if ( rxps->mode == RXMODE_BREGEXP_H ){
			int rsize;

			rsize = TSTRSIZE32(rlist);
			rxps->string_rh = HeapAlloc(DLLheap,0,rsize);
			if ( rxps->string_rh == NULL ) goto error;
			memcpy(rxps->string_rh,rlist,rsize);
		}else{ // if ( rxps->mode == RXMODE_BREGEXP_TR )
			if ( (RMatch == EX_IREGEXP) && (LoadRMatch() == FALSE) ){
				XMessage(NULL,NULL,XM_GrERRld,T("IRegExp: Unsupport 'tr'"));
				goto error;
			}
		}
	}

	if ( RMatch != EX_IREGEXP ){ // EX_BREGEXP / EX_BREGONIG
		int ssize;

		ssize = (wsprintf(buf,
#ifdef UNICODE
				(RMatch == EX_BREGONIG) ? T("%s%c%s%c%s%c%s") : T("%s%c%s%c%s%c%sk"),
#else
				T("%s%c%s%c%s%c%s"),
#endif
				preope,separater, slist,separater, rlist,separater, option)
				+ 1) * sizeof(TCHAR);

		rxps->string_rb = HeapAlloc(DLLheap,0,ssize);
		if ( rxps->string_rb == NULL ) goto error;
		memcpy(rxps->string_rb,buf,ssize);
		rxps->bregp = NULL;
		rxps->bonip = NULL;
	}else{	// EX_IREGEXP
		BSTR pattern;
		IRegExp *regexp;

		rxps->ComInitResult = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if ( FAILED(CoCreateInstance(&XCLSID_RegExp,NULL,
					CLSCTX_INPROC_SERVER,&XIID_IRegExp,(void **)&regexp)) ){
			if ( SUCCEEDED(rxps->ComInitResult) ) CoUninitialize();
			goto error;
		}
		while( *option ){
			switch (*option++){
				case 'i':
					IRegExp_put_IgnoreCase(regexp,VARIANT_TRUE);
					break;
				case 'g':
					IRegExp_put_Global(regexp,VARIANT_TRUE);
					break;
				default:
					XMessage(NULL,NULL,XM_GrERRld,T("IRegExp: Option error"));
					break;
			}
		}

		pattern = CreateBstring(slist);
		if ( FAILED(IRegExp_put_Pattern(regexp,pattern)) ){
			XMessage(NULL,NULL,XM_GrERRld,IREGEXPPATTENERROR);
			IRegExp_Release(regexp);
			DSysFreeString(pattern);
			if ( SUCCEEDED(rxps->ComInitResult) ) CoUninitialize();
			goto error;
		}
		DSysFreeString(pattern);
		rxps->mode += RXMODE_IREGEXP_MIN;
		rxps->string_ri = CreateBstring(rlist);
		rxps->iregexp = regexp;
	}
	*rxpsptr = rxps;
	return TRUE;

noparamerror:
	XMessage(NULL,NULL,XM_GrERRld,
			T("RegExp: Bad command or Not found src\'/\'replace") );
	// error へ
error:
	HeapFree(DLLheap,0,rxps);
	return FALSE;
}

void FreeRegularExpressionReplace(RXPREPLACESTRING *rxps)
{
	if ( rxps->mode <= RXMODE_BREGEXP_MAX ){
		if ( rxps->mode == RXMODE_BREGEXP_H ) HeapFree(DLLheap,0,rxps->string_rh);
		if ( rxps->bregp != NULL ) BRegfree(rxps->bregp);
		if ( rxps->bonip != NULL ) DBRegfree(rxps->bonip);
		HeapFree(DLLheap,0,rxps->string_rb);
	}else{
		if ( rxps->mode == RXMODE_IREGEXP_H ) HeapFree(DLLheap,0,rxps->string_rh);
		IRegExp_Release(rxps->iregexp);
		DSysFreeString(rxps->string_ri);
		if ( SUCCEEDED(rxps->ComInitResult) ) CoUninitialize();
	}
	HeapFree(DLLheap,0,rxps);
}

BOOL RegularExpressionReplace(RXPREPLACESTRING *rxps,TCHAR *target)
{
	if ( rxps->mode <= RXMODE_BREGEXP_MAX ){	// BRegExp(s/tr/h)
		TCHAR msg[VFPS];

		if ( rxps->mode == RXMODE_BREGEXP_H ){
			return RegularExpressionMatch_bregexp(rxps,target);
		}
		if ( DBSubst != NULL ){ // bregonig
			int count; // 変換した文字数

			msg[0] = '\0';
			if ( rxps->mode != RXMODE_BREGEXP_TR ){
				count = DBSubst(rxps->string_rb,target,target + tstrlen(target),
						&rxps->bonip,msg);
			}else{
				count = DBTrans(rxps->string_rb,target,target + tstrlen(target),
						&rxps->bonip,msg);
			}
			if ( count ){
				if( (rxps->bonip == NULL) || (rxps->bonip->outp == NULL) ){
					target[0] = '\0';
				}else{
					tstrcpy(target,rxps->bonip->outp);
				}
			}
		}else{ // bregexp
			int count; // 変換した文字数
#ifdef UNICODE
			char targetA[VFPS];
			char renameA[VFPS];

			UnicodeToAnsi(target,targetA,VFPS);
			UnicodeToAnsi(rxps->string_rb,renameA,VFPS);
#else
			#define targetA target
			#define renameA rxps->string_rb
#endif
			msg[0] = '\0';
			if ( rxps->mode != RXMODE_BREGEXP_TR ){
				count = BSubst(renameA,targetA,targetA + strlen(targetA),
						&rxps->bregp,(char *)msg);
			}else{
				count = BTrans(renameA,targetA,targetA + strlen(targetA),
						&rxps->bregp,(char *)msg);
			}
			if ( count ){
				if( (rxps->bregp == NULL) || (rxps->bregp->outp == NULL) ){
					target[0] = '\0';
				}else{
#ifdef UNICODE
					AnsiToUnicode(rxps->bregp->outp,target,VFPS);
#else
					tstrcpy(target,rxps->bregp->outp);
#endif
				}
			}
		}
		return TRUE;
	}else{	// IRegExp(s/h)
		BSTR btarget,result = NULL;

		if ( rxps->mode == RXMODE_IREGEXP_H ){
			return RegularExpressionMatch_iregexp(rxps,target);
		}
		btarget = CreateBstring(target);
		if ( FAILED(IRegExp_Replace(rxps->iregexp,
				btarget,rxps->string_ri,&result)) ){
			XMessage(NULL,NULL,XM_GrERRld,T("IRegExp error"));
			DSysFreeString(btarget);
			return FALSE;
		}
		DSysFreeString(btarget);
#ifdef UNICODE
		tstrcpy(target,result);
#else
		UnicodeToAnsi(result,target,MAX_PATH);
#endif
		DSysFreeString(result);
		return TRUE;
	}
}

void GetParamFlags(GETPARAMFLAGSSTRUCT *gpfs,const TCHAR **param,const TCHAR *flagnames)
{
	DWORD flags;
	const TCHAR *lp,*nowp;

	gpfs->mask = 0;
	gpfs->value = 0;
	nowp = *param;
	while( *nowp != '\0' ){
		TCHAR c;

		c = TinyCharLower(SkipSpace(&nowp));
		for ( flags = LSBIT,lp = flagnames ; *lp ; flags <<= 1,lp++){
			if ( c != *lp ) continue;
			nowp++;
			setflag(gpfs->mask, flags);
			switch( *nowp ){
				case '-':
					nowp++;
					break;
				case '+':
					nowp++;
				default:
					setflag(gpfs->value, flags);
			}
			break;
		}
		if ( *lp == '\0' ) break;
	}
	*param = nowp;
	return;
}

int CheckExtMode(EXSMEM *exm, MAKEFNSTRUCT *mfs)
{
	switch ( TinyCharLower(mfs->name[0]) ){
		case 'a': {	// Attribute: 属性指定
			GETPARAMFLAGSSTRUCT gpfs;

			setflag(mfs->flags, REGEXPF_REQ_ATTR);
			if ( CheckExsmem(exm, sizeof(EXS_ATTR)) == FALSE ) return CEM_ERROR;
			GetParamFlags(&gpfs, (const TCHAR **)&mfs->param, AttrLabelString);
			if ( (gpfs.mask & FILE_ATTRIBUTE_DIRECTORY) &&
				!(gpfs.value & FILE_ATTRIBUTE_DIRECTORY) ){
				setflag(mfs->flags, REGEXPF_PATHMASK);
			}
			((EXS_ATTR *)exm->dest)->ext = EX_ATTR;
			((EXS_ATTR *)exm->dest)->next = sizeof(EXS_ATTR);
			((EXS_ATTR *)exm->dest)->mask = gpfs.mask;
			((EXS_ATTR *)exm->dest)->value = gpfs.value;
			break;
		}

		case 'p': {	// Path: ディレクトリ指定
			setflag(mfs->flags, REGEXPF_PATHMASK | REGEXPF_REQ_ATTR);
										// ワイルドカード指定がない
			if ( SkipSpace((const TCHAR **)&mfs->param) == '\0' ){
				((EXS_DATA *)exm->dest)->ext = EX_NONE;
				((EXS_OPTION *)exm->dest)->next = 0;
				break;
			}

			if ( CheckExsmem(exm, sizeof(EXS_DIR)) == FALSE ) return CEM_ERROR;
			((EXS_DIR *)exm->dest)->ext = EX_DIR;
			((EXS_DIR *)exm->dest)->next = sizeof(EXS_DIR);
			exm->left -= sizeof(EXS_DIR);
			exm->dest += sizeof(EXS_DIR);
			if ( *mfs->param != '/' ){
				memmove(mfs->name, mfs->param, TSTRSIZE(mfs->param));
				return CEM_ANALYZE; //解析を続行させて名前/正規表現の処理を行う
			}
			// 正規表現
			if ( RegExpWildCard(exm, (const TCHAR **)&mfs->param) == FALSE ){
				return CEM_ERROR; // error;
			}
			break;
		}

		case 'o': // Option: オプション指定
		case 'i': {	// 旧オプション指定
			if ( SkipSpace((const TCHAR **)&mfs->param) != '\0' ){
				GETPARAMFLAGSSTRUCT gpfs;

				GetParamFlags(&gpfs, (const TCHAR **)&mfs->param, inclabel);
				mfs->flags = (mfs->flags & ~(REGEXPF_PATHMASK | REGEXPF_NOEXT | REGEXPF_SILENTERROR | REGEXPF_WORDMATCH)) | gpfs.value;
				// REGEXPF_PATHMASK 以外のフラグがなければ、保存不要
				if ( !(gpfs.value & ~REGEXPF_PATHMASK) ){
					((EXS_ATTR *)exm->dest)->ext = 0;
					((EXS_OPTION *)exm->dest)->next = 0;
					break;
				}
			}	// 拡張子非分離指定
			if ( CheckExsmem(exm,sizeof(EXS_OPTION)) == FALSE ) return CEM_ERROR;
			((EXS_OPTION *)exm->dest)->ext = EX_JOINEXT;
			((EXS_OPTION *)exm->dest)->next = sizeof(EXS_OPTION);
			mfs->useext = FALSE;
			break;
		}

		case 'd': { // Date: 日付指定
			DWORD type,type2;
			FILETIME ftime;
			SYSTEMTIME stime;
			int daytype;
			WORD offset = 0;
			TCHAR typechar;

			setflag(mfs->flags, REGEXPF_REQ_TIME);
			typechar = TinyCharLower(SkipSpace((const TCHAR **)&mfs->param));
			if ( typechar == 'c' ){
				mfs->param++;
				offset = EX_CREATEDATEREL - EX_WRITEDATEREL;
			}else if ( typechar == 'a' ){
				mfs->param++;
				offset = EX_ACCESSDATEREL - EX_WRITEDATEREL;
			}
			type = GetCompareType((const TCHAR **)&mfs->param);
			daytype = GetWildDate((const TCHAR **)&mfs->param, &ftime, &stime);
			if ( daytype == DAY_NONE ) return CEM_ERROR;
			type2 = GetCompareType((const TCHAR **)&mfs->param);
			if ( type2 ){ // type2 は符号反転を行う
				type = TYPE_INVERT(type2);
			}
			if ( daytype == DAY_ABSOLUTELY ){
				if ( CheckExsmem(exm,sizeof(EXS_WRITEDATEABS)) == FALSE ){
					return CEM_ERROR;
				}
				((EXS_WRITEDATEABS *)exm->dest)->ext = (WORD)(offset + EX_WRITEDATEABS);
				((EXS_WRITEDATEABS *)exm->dest)->next = sizeof(EXS_WRITEDATEABS);
				((EXS_WRITEDATEABS *)exm->dest)->date = stime;

				if ( !type ) type = TYPE_EQUAL; // 指定無し…当日
				((EXS_WRITEDATEABS *)exm->dest)->type = TYPE_INVERT(type);
			}else{
				if ( CheckExsmem(exm,sizeof(EXS_WRITEDATEREL)) == FALSE){
					return CEM_ERROR;
				}
				((EXS_WRITEDATEREL *)exm->dest)->ext = (WORD)(offset + EX_WRITEDATEREL);
				((EXS_WRITEDATEREL *)exm->dest)->next = sizeof(EXS_WRITEDATEREL);
				((EXS_WRITEDATEREL *)exm->dest)->days = ftime;
				if ( !type ) type = TYPE_SMALL; // 指定無し…日数以内
				((EXS_WRITEDATEREL *)exm->dest)->type = type;
			}
			break;
		}

		case 's': { // Size: ファイルサイズ指定
			DWORD type;

			setflag(mfs->flags,REGEXPF_REQ_SIZE);
			if ( CheckExsmem(exm,sizeof(EXS_SIZE)) == FALSE) return CEM_ERROR;
			type = GetCompareType((const TCHAR **)&mfs->param);
			if ( !type ) type = TYPE_SMALL;

			if ( FALSE == GetSizeNumber((const TCHAR **)&mfs->param,
									&((EXS_SIZE *)exm->dest)->sizeL,
									&((EXS_SIZE *)exm->dest)->sizeH) ){
				return CEM_ERROR;
			}
			((EXS_SIZE *)exm->dest)->ext = EX_SIZE;
			((EXS_SIZE *)exm->dest)->next = sizeof(EXS_SIZE);
			((EXS_SIZE *)exm->dest)->type = type;
			break;
		}

		case 'r': { // Roma: ローマ字指定
			DWORD size,structsize;

			if ( LoadRegExp() == FALSE ) return CEM_ERROR; // DLL 読み込み失敗
			if ( InitMigemo() == FALSE ) return CEM_ERROR;

			size = tstrlen32(mfs->param);
			structsize = sizeof(EXS_ROMA) + size * sizeof(TCHAR) + sizeof(TCHAR);
			if ( CheckExsmem(exm,structsize) == FALSE) return CEM_ERROR;

			((EXS_ROMA *)(exm->dest))->ext = EX_ROMA;
			((EXS_ROMA *)(exm->dest))->next = (WORD)structsize;
			((EXS_ROMA *)(exm->dest))->handle = 0;
			memcpy(((EXS_ROMA *)(exm->dest))->data, mfs->param, size * sizeof(TCHAR));
			((EXS_ROMA *)(exm->dest))->data[size] = '\0';
			break;
		}

		default:
			if ( !(mfs->flags & REGEXPF_SILENTERROR) ){
				xmessage(XM_FaERRd,T("bad wildcard attribute"));
			}
			return CEM_ERROR;
	}
	return CEM_COMP; // 処理成功 & 解釈終了
}

void GetRegularExpressionName(TCHAR *str)
{
	const TCHAR *p;

	LoadRegExp();
	switch ( RMatch ){
		case EX_BREGEXP:
			p = T("BREGEXP");
			break;

		case EX_BREGONIG:
			p = T("bregonig");
			break;

		case EX_IREGEXP:
			p = T("IRegExp");
			break;

		default:
			p = T("none");
			break;
	}
	tstrcpy(str,p);
}
