// ŒÃ‚¢SDK‚Å’è‹`‚³‚ê‚Ä‚¢‚È‚¢COM’è‹`

#ifdef __cplusplus
#ifndef SHCDF_UPDATEITEM
#define MAX_COLUMN_NAME_LEN 80
#define MAX_COLUMN_DESC_LEN 128
#define SHCDF_UPDATEITEM 0x00000001

typedef struct {
	ULONG dwFlags;
	ULONG dwReserved;
	WCHAR wszFolder[MAX_PATH];
} SHCOLUMNINIT,*LPSHCOLUMNINIT;
typedef const SHCOLUMNINIT *LPCSHCOLUMNINIT;

#ifndef WINEGCC
typedef struct {
	GUID fmtid;
	DWORD pid;
} SHCOLUMNID, *LPSHCOLUMNID;
typedef const SHCOLUMNID *LPCSHCOLUMNID;
#endif // WINEGCC

typedef struct {
	SHCOLUMNID scid;
	VARTYPE vt;
	DWORD fmt;
	UINT cChars;
	DWORD csFlags;
	WCHAR wszTitle[MAX_COLUMN_NAME_LEN];
	WCHAR wszDescription[MAX_COLUMN_DESC_LEN];
} SHCOLUMNINFO,*LPSHCOLUMNINFO;
typedef const SHCOLUMNINFO *LPCSHCOLUMNINFO;

typedef struct {
	ULONG dwFlags;
	DWORD dwFileAttributes;
	ULONG dwReserved;
	WCHAR *pwszExt;
	WCHAR wszFile[MAX_PATH];
} SHCOLUMNDATA,*LPSHCOLUMNDATA;
typedef const SHCOLUMNDATA *LPCSHCOLUMNDATA;

#undef INTERFACE
#define INTERFACE IColumnProvider

DECLARE_INTERFACE_(IColumnProvider,IUnknown)
{
	// IUnknown
	STDMETHOD (QueryInterface)(THIS_ REFIID riid,void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IColumnProvider
	STDMETHOD (Initialize)(THIS_ LPCSHCOLUMNINIT psci) PURE;
	STDMETHOD (GetColumnInfo)(THIS_ DWORD dwIndex,SHCOLUMNINFO *psci) PURE;
	STDMETHOD (GetItemData)(THIS_ LPCSHCOLUMNID pscid,LPCSHCOLUMNDATA pscd,VARIANT *pvarData) PURE;
};
#ifndef __GNUC__
#undef  INTERFACE
#define INTERFACE  IQueryInfo
DECLARE_INTERFACE_(IQueryInfo, IUnknown)
{
	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	// IQueryInfo methods
	STDMETHOD(GetInfoTip)(THIS_ DWORD dwFlags, WCHAR **ppwszTip) PURE;
	STDMETHOD(GetInfoFlags)(THIS_ DWORD *pdwFlags) PURE;
} ;
#endif // __GNUC__
#endif // ndef SHCDF_UPDATEITEM

#ifndef __IPropertySystem_INTERFACE_DEFINED__
#define __IPropertySystem_INTERFACE_DEFINED__

typedef enum {
	PDEF_ALL = 0,
	PDEF_COLUMN = 6
} PROPDESC_ENUMFILTER;

#ifndef PROPERTYKEY_DEFINED
#define PROPERTYKEY_DEFINED
typedef struct {
	GUID fmtid;
	DWORD pid;
} PROPERTYKEY;
#endif

#ifndef REFPROPERTYKEY
#define REFPROPERTYKEY const PROPERTYKEY &
#define REFPROPVARIANT const PROPVARIANT &

typedef enum {
	PDFF_DEFAULT = 0,
} PROPDESC_FORMAT_FLAGS;
#endif

//const CLSID XCLSID_PropertySystem = {0xb8967f85,0x58ae,0x4f46,{0x9f,0xb2,0x5d,0x79,0x04,0x79,0x8f,0x4b}};
//const IID XIID_IPropertySystem = {0xca724e8a,0xc3e6,0x442b,{0x88,0xa4,0x6f,0xb0,0xdb,0x80,0x35,0xa3}};
class IPropertySystem : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetPropertyDescription(
			REFPROPERTYKEY propkey,REFIID riid,void **ppv) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetPropertyDescriptionByName(
			LPCWSTR pszCanonicalName,REFIID riid,void **ppv) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetPropertyDescriptionListFromString(
			LPCWSTR pszPropList,REFIID riid,void **ppv) = 0;
		virtual HRESULT STDMETHODCALLTYPE EnumeratePropertyDescriptions(
			PROPDESC_ENUMFILTER filterOn,REFIID riid,void **ppv) = 0;
		virtual HRESULT STDMETHODCALLTYPE FormatForDisplay(
			REFPROPERTYKEY key,REFPROPVARIANT propvar,
			PROPDESC_FORMAT_FLAGS pdff,LPWSTR pszText,DWORD cchText) = 0;
		virtual HRESULT STDMETHODCALLTYPE FormatForDisplayAlloc(
			REFPROPERTYKEY key,REFPROPVARIANT propvar,
			PROPDESC_FORMAT_FLAGS pdff,LPWSTR *ppszDisplay) = 0;
		virtual HRESULT STDMETHODCALLTYPE RegisterPropertySchema(
			LPCWSTR pszPath) = 0;
		virtual HRESULT STDMETHODCALLTYPE UnregisterPropertySchema(
			LPCWSTR pszPath) = 0;
		virtual HRESULT STDMETHODCALLTYPE RefreshPropertySchema(void) = 0;
};
//const IID XIID_IPropertyDescriptionList = {0x1f9fc1d0,0xc39b,0x4b26,{0x81,0x7f,0x01,0x19,0x67,0xd3,0x44,0x0e}};
class IPropertyDescriptionList : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetCount(UINT *pcElem) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetAt(UINT iElem,REFIID riid,void **ppv) = 0;
};
#endif

#ifndef __IPropertyDescription_INTERFACE_DEFINED__
#define __IPropertyDescription_INTERFACE_DEFINED__
typedef enum {
	PDTF_DEFAULT = 0,
} PROPDESC_TYPE_FLAGS;

typedef enum{
	PDVF_DEFAULT = 0,
} PROPDESC_VIEW_FLAGS;

typedef enum {
	PDDT_STRING = 0,
} PROPDESC_DISPLAYTYPE;

typedef enum {
	PDGR_DISCRETE = 0,
} PROPDESC_GROUPING_RANGE;


typedef enum {
	PDSD_GENERAL = 0,
} PROPDESC_SORTDESCRIPTION;

typedef enum {
	PDRDT_GENERAL = 0,
} PROPDESC_RELATIVEDESCRIPTION_TYPE;

typedef enum {
	PDAT_DEFAULT = 0,
} PROPDESC_AGGREGATION_TYPE;

typedef enum {
	PDCOT_NONE = 0,
} PROPDESC_CONDITION_TYPE;
typedef DWORD SHCOLSTATEF;
typedef enum {
	COP_IMPLICIT = 0,
} CONDITION_OPERATION;

class IPropertyDescription : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetPropertyKey(PROPERTYKEY *pkey) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetCanonicalName(LPWSTR *ppszName) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetPropertyType(VARTYPE *pvartype) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetDisplayName(LPWSTR *ppszName) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetEditInvitation(LPWSTR *ppszInvite) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetTypeFlags(PROPDESC_TYPE_FLAGS mask,PROPDESC_TYPE_FLAGS *ppdtFlags) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetViewFlags(PROPDESC_VIEW_FLAGS *ppdvFlags) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetDefaultColumnWidth(UINT *pcxChars) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetDisplayType(PROPDESC_DISPLAYTYPE *pdisplaytype) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetColumnState(SHCOLSTATEF *pcsFlags) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetGroupingRange(PROPDESC_GROUPING_RANGE *pgr) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetRelativeDescriptionType(PROPDESC_RELATIVEDESCRIPTION_TYPE *prdt) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetRelativeDescription(REFPROPVARIANT propvar1,REFPROPVARIANT propvar2,LPWSTR *ppszDesc1,LPWSTR *ppszDesc2) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetSortDescription(PROPDESC_SORTDESCRIPTION *psd) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetSortDescriptionLabel(BOOL fDescending,LPWSTR *ppszDescription) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetAggregationType(PROPDESC_AGGREGATION_TYPE *paggtype) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetConditionType(PROPDESC_CONDITION_TYPE *pcontype,CONDITION_OPERATION *popDefault) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetEnumTypeList(REFIID riid,void **ppv) = 0;
		virtual HRESULT STDMETHODCALLTYPE CoerceToCanonicalValue(PROPVARIANT *ppropvar) = 0;
		virtual HRESULT STDMETHODCALLTYPE FormatForDisplay(REFPROPVARIANT propvar,PROPDESC_FORMAT_FLAGS pdfFlags,LPWSTR *ppszDisplay) = 0;
		virtual HRESULT STDMETHODCALLTYPE IsValueCanonical(REFPROPVARIANT propvar) = 0;
};
#endif

#ifndef __IPropertyStore_INTERFACE_DEFINED__
#define __IPropertyStore_INTERFACE_DEFINED__
class IPropertyStore : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD *cProps) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetAt(DWORD iProp,PROPERTYKEY *pkey) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key,PROPVARIANT *pv) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetValue(REFPROPERTYKEY key,REFPROPVARIANT propvar) = 0;
		virtual HRESULT STDMETHODCALLTYPE Commit(void) = 0;
};
#endif

#endif // __cplusplus

#ifndef __IRegExp_INTERFACE_DEFINED__
#define __IRegExp_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE IRegExp
DECLARE_INTERFACE_(IRegExp,IDispatch)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,void **) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *) PURE;
	STDMETHOD(GetTypeInfo)(THIS_ UINT,LCID,ITypeInfo **) PURE;
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID,LPOLESTR *,UINT,LCID,DISPID *) PURE;
	STDMETHOD(Invoke)(THIS_ DISPID,REFIID,LCID,WORD,DISPPARAMS *,VARIANT *,EXCEPINFO *,UINT *) PURE;

	STDMETHOD(get_Pattern)(THIS_ BSTR *) PURE;
	STDMETHOD(put_Pattern)(THIS_ BSTR) PURE;
	STDMETHOD(get_IgnoreCase)(THIS_ VARIANT_BOOL *) PURE;
	STDMETHOD(put_IgnoreCase)(THIS_ VARIANT_BOOL) PURE;
	STDMETHOD(get_Global)(THIS_ VARIANT_BOOL *) PURE;
	STDMETHOD(put_Global)(THIS_ VARIANT_BOOL) PURE;
	STDMETHOD(Execute)(THIS_ BSTR,IDispatch **) PURE;
	STDMETHOD(Test)(THIS_ BSTR,VARIANT_BOOL *) PURE;
	STDMETHOD(Replace)(THIS_ BSTR,BSTR,BSTR *) PURE;
};

#define IRegExp_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IRegExp_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IRegExp_Release(p) (p)->lpVtbl->Release(p)
#define IRegExp_GetTypeInfoCount(p,a) (p)->lpVtbl->GetTypeInfoCount(p,a)
#define IRegExp_GetTypeInfo(p,a,b,c) (p)->lpVtbl->GetTypeInfo(p,a,b,c)
#define IRegExp_GetIDsOfNames(p,a,b,c,d,e) (p)->lpVtbl->GetIDsOfNames(p,a,b,c,d,e)
#define IRegExp_Invoke(p,a,b,c,d,e,f,g,h) (p)->lpVtbl->Invoke(p,a,b,c,d,e,f,g,h)
#define IRegExp_get_Pattern(p,a) (p)->lpVtbl->get_Pattern(p,a)
#define IRegExp_put_Pattern(p,a) (p)->lpVtbl->put_Pattern(p,a)
#define IRegExp_get_IgnoreCase(p,a) (p)->lpVtbl->get_IgnoreCase(p,a)
#define IRegExp_put_IgnoreCase(p,a) (p)->lpVtbl->put_IgnoreCase(p,a)
#define IRegExp_get_Global(p,a) (p)->lpVtbl->get_Global(p,a)
#define IRegExp_put_Global(p,a) (p)->lpVtbl->put_Global(p,a)
#define IRegExp_Execute(p,a,b) (p)->lpVtbl->Execute(p,a,b)
#define IRegExp_Test(p,a,b) (p)->lpVtbl->Test(p,a,b)
#define IRegExp_Replace(p,a,b,c) (p)->lpVtbl->Replace(p,a,b,c)
#endif // IRegExp

#ifndef __IMatch_INTERFACE_DEFINED__
#define __IMatch_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE IMatch
DECLARE_INTERFACE_(IMatch, IDispatch)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID *) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *) PURE;
	STDMETHOD(GetTypeInfo)(THIS_ UINT, LCID, ITypeInfo **) PURE;
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID, LPOLESTR *, UINT, LCID, DISPID *) PURE;
	STDMETHOD(Invoke)(THIS_ DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) PURE;

	STDMETHOD(get_Value)(THIS_ BSTR *) PURE;
	STDMETHOD(get_FirstIndex)(THIS_ long *) PURE;
	STDMETHOD(get_Length)(THIS_ long *) PURE;
};
#ifdef COBJMACROS
#define IMatch_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IMatch_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IMatch_Release(p) (p)->lpVtbl->Release(p)
#define IMatch_GetTypeInfoCount(p,a) (p)->lpVtbl->GetTypeInfoCount(p,a)
#define IMatch_GetTypeInfo(p,a,b,c) (p)->lpVtbl->GetTypeInfo(p,a,b,c)
#define IMatch_GetIDsOfNames(p,a,b,c,d,e) (p)->lpVtbl->GetIDsOfNames(p,a,b,c,d,e)
#define IMatch_Invoke(p,a,b,c,d,e,f,g,h) (p)->lpVtbl->Invoke(p,a,b,c,d,e,f,g,h)
#define IMatch_get_Value(p,a) (p)->lpVtbl->get_Value(p,a)
#define IMatch_get_FirstIndex(p,a) (p)->lpVtbl->get_FirstIndex(p,a)
#define IMatch_get_Length(p,a) (p)->lpVtbl->get_Length(p,a)
#endif
#endif

#ifndef __IMatch2_INTERFACE_DEFINED__
#define __IMatch2_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE IMatch2
DECLARE_INTERFACE_(IMatch2, IDispatch)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID *) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *) PURE;
	STDMETHOD(GetTypeInfo)(THIS_ UINT, LCID, ITypeInfo **) PURE;
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID, LPOLESTR *, UINT, LCID, DISPID *) PURE;
	STDMETHOD(Invoke)(THIS_ DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) PURE;

	STDMETHOD(get_Value)(THIS_ BSTR *) PURE;
	STDMETHOD(get_FirstIndex)(THIS_ long *) PURE;
	STDMETHOD(get_Length)(THIS_ long *) PURE;
	STDMETHOD(get_SubMatches)(THIS_ IDispatch **) PURE;
};
#endif

// IMatchCollection
#ifndef __IMatchCollection_INTERFACE_DEFINED__
#define __IMatchCollection_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE IMatchCollection
DECLARE_INTERFACE_(IMatchCollection, IDispatch)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID *) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *) PURE;
	STDMETHOD(GetTypeInfo)(THIS_ UINT, LCID, ITypeInfo **) PURE;
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID, LPOLESTR *, UINT, LCID, DISPID *) PURE;
	STDMETHOD(Invoke)(THIS_ DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) PURE;

	STDMETHOD(get_Item)(THIS_ long, IDispatch **) PURE;
	STDMETHOD(get_Count)(THIS_ long *) PURE;
	STDMETHOD(get__NewEnum)(THIS_ IUnknown **) PURE;
};
#ifdef COBJMACROS
#define IMatchCollection_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IMatchCollection_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IMatchCollection_Release(p) (p)->lpVtbl->Release(p)
#define IMatchCollection_GetTypeInfoCount(p,a) (p)->lpVtbl->GetTypeInfoCount(p,a)
#define IMatchCollection_GetTypeInfo(p,a,b,c) (p)->lpVtbl->GetTypeInfo(p,a,b,c)
#define IMatchCollection_GetIDsOfNames(p,a,b,c,d,e) (p)->lpVtbl->GetIDsOfNames(p,a,b,c,d,e)
#define IMatchCollection_Invoke(p,a,b,c,d,e,f,g,h) (p)->lpVtbl->Invoke(p,a,b,c,d,e,f,g,h)
#define IMatchCollection_get_Item(p,a,b) (p)->lpVtbl->get_Item(p,a,b)
#define IMatchCollection_get_Count(p,a) (p)->lpVtbl->get_Count(p,a)
#define IMatchCollection_get__NewEnum(p,a) (p)->lpVtbl->get__NewEnum(p,a)
#endif
#endif

// ISubMatches
#ifndef __ISubMatches_INTERFACE_DEFINED__
#define __ISubMatches_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE ISubMatches
DECLARE_INTERFACE_(ISubMatches, IDispatch)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID *) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(GetTypeInfoCount)(THIS_ UINT *) PURE;
	STDMETHOD(GetTypeInfo)(THIS_ UINT, LCID, ITypeInfo **) PURE;
	STDMETHOD(GetIDsOfNames)(THIS_ REFIID, LPOLESTR *, UINT, LCID, DISPID *) PURE;
	STDMETHOD(Invoke)(THIS_ DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) PURE;

	STDMETHOD(get_Item)(THIS_ long, VARIANT *) PURE;
	STDMETHOD(get_Count)(THIS_ long *) PURE;
	STDMETHOD(get__NewEnum)(THIS_ IUnknown **) PURE;
};
#endif

#undef INTERFACE
#define INTERFACE xIExtractImage
DECLARE_INTERFACE_(xIExtractImage,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(GetLocation)(THIS_ LPWSTR pszPathBuffer,DWORD cch,
			DWORD *pdwPriority,const SIZE *prgSize,DWORD dwRecClrDepth,
			DWORD *pdwFlags) PURE;
	STDMETHOD(Extract)(THIS_ HBITMAP *phBmpThumbnail) PURE;
};

#undef INTERFACE
#define INTERFACE xIThumbnailProvider
DECLARE_INTERFACE_(xIThumbnailProvider,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(GetThumbnail)(THIS_ UINT cx,HBITMAP *phbmp,int *pdwAlpha) PURE;
};

#ifndef SFGAO_HIDDEN
	#define SFGAO_HIDDEN 0x80000
	#define SFGAO_NONENUMERATED 0x100000
	#define SFGAO_NEWCONTENT 0x200000
	#define SFGAO_BROWSABLE 0x8000000
#endif
#ifndef SFGAO_STORAGE
	#define SFGAO_STORAGE 8
	#define SFGAO_SYSTEM 0x1000
	#define SFGAO_ENCRYPTED 0x2000
	#define SFGAO_ISSLOW 0x4000
	#define SFGAO_STREAM 0x400000
	#define SFGAO_STORAGEANCESTOR 0x800000
#endif

typedef int xWTS_ALPHATYPE;
enum { WTSAT_UNKNOWN = 0,WTSAT_RGB = 1, WTSAT_ARGB = 2 };
typedef int xSIGDN;
typedef int xSICHINTF;
typedef ULONG xSFGAOF;
#define xIShellItemDef

#undef INTERFACE
#define INTERFACE xIShellItem
DECLARE_INTERFACE_(xIShellItem,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(BindToHandler)(THIS_ IBindCtx *pbc,
			REFGUID bhid,REFIID riid,void **ppv) PURE;
	STDMETHOD(GetParent)(THIS_  xIShellItem **ppsi) PURE;
	STDMETHOD(GetDisplayName )(THIS_ xSIGDN sigdnName,LPWSTR *ppszName) PURE;
	STDMETHOD(GetAttributes)(xIShellItem * This,xSFGAOF sfgaoMask,xSFGAOF *psfgaoAttribs) PURE;
	STDMETHOD(Compare )(THIS_ xIShellItem *psi,xSICHINTF hint,int *piOrder) PURE;
};

typedef int WTS_CACHEFLAGS;
typedef int WTS_FLAGS;
#define WTS_EXTRACT 0
/*
	{	WTS_EXTRACT	= 0,
	WTS_INCACHEONLY	= 0x1,
	WTS_FASTEXTRACT	= 0x2,
	WTS_FORCEEXTRACTION	= 0x4,
	WTS_SLOWRECLAIM	= 0x8,
	WTS_EXTRACTDONOTCACHE	= 0x20,
	WTS_SCALETOREQUESTEDSIZE	= 0x40,
	WTS_SKIPFASTEXTRACT	= 0x80,
	WTS_EXTRACTINPROC	= 0x100
	} 	WTS_FLAGS;
*/

#undef INTERFACE
#define INTERFACE xISharedBitmap
DECLARE_INTERFACE_(xISharedBitmap,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(GetSharedBitmap)(THIS_ HBITMAP *phbm) PURE;
	STDMETHOD(GetSize)(THIS_ SIZE *pSize) PURE;
	STDMETHOD(GetFormat)(THIS_ xWTS_ALPHATYPE *pat) PURE;
	STDMETHOD(InitializeBitmap)(THIS_ HBITMAP hbm,xWTS_ALPHATYPE wtsAT) PURE;
	STDMETHOD(Detach )(THIS_ HBITMAP *phbm) PURE;
};
typedef struct {
	BYTE rgbKey[16];
} WTS_THUMBNAILID;

#undef INTERFACE
#define INTERFACE xIThumbnailCache
DECLARE_INTERFACE_(xIThumbnailCache,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(GetThumbnail)(THIS_ xIShellItem *pShellItem,
			UINT cxyRequestedThumbSize,WTS_FLAGS flags,
			xISharedBitmap **ppvThumb,WTS_CACHEFLAGS *pOutFlags,
			WTS_THUMBNAILID *pThumbnailID) PURE;
	STDMETHOD(GetThumbnailByID )(THIS_ WTS_THUMBNAILID thumbnailID,
			UINT cxyRequestedThumbSize,xISharedBitmap **ppvThumb,
			WTS_CACHEFLAGS *pOutFlags) PURE;
};

#ifdef __cplusplus
#define xIID_IAccessibleIID {0x618736e0,0x3c3d,0x11cf,{0x81,0x0c,0x00,0xaa,0x00,0x38,0x9b,0x71}}

#ifndef OBJID_CLIENT
#define OBJID_CLIENT ((LONG)0xFFFFFFFC)
#endif
#define CHILDID_SELF        0
#define NAVDIR_UP	1
#define NAVDIR_DOWN	2
#define NAVDIR_LEFT	3
#define NAVDIR_RIGHT 4
#define NAVDIR_NEXT  5
#define NAVDIR_PREVIOUS   6
#define NAVDIR_FIRSTCHILD 7
#define NAVDIR_LASTCHILD  8
#define ROLE_SYSTEM_LIST		( 0x21 )
#define ROLE_SYSTEM_LISTITEM	( 0x22 )
#define SELFLAG_TAKEFOCUS		( 0x1 )
#define SELFLAG_TAKESELECTION	( 0x2 )
#ifndef STATE_SYSTEM_SELECTED
#define STATE_SYSTEM_SELECTED	( 0x2 )
#define STATE_SYSTEM_FOCUSED	( 0x4 )
#define STATE_SYSTEM_FOCUSABLE	( 0x100000 )
#define STATE_SYSTEM_SELECTABLE	( 0x200000 )
#endif

interface IAccessible : public IDispatch
{
	public:
		STDMETHOD(get_accParent)(IDispatch **) = 0;
		STDMETHOD(get_accChildCount)(long *) = 0;
		STDMETHOD(get_accChild)(VARIANT,IDispatch **) = 0;
		STDMETHOD(get_accName)(VARIANT,BSTR *) = 0;
		STDMETHOD(get_accValue)(VARIANT,BSTR *) = 0;
		STDMETHOD(get_accDescription)(VARIANT,BSTR *) = 0;
		STDMETHOD(get_accRole)(VARIANT,VARIANT *) = 0;
		STDMETHOD(get_accState)(VARIANT,VARIANT *) = 0;
		STDMETHOD(get_accHelp)(VARIANT,BSTR *) = 0;
		STDMETHOD(get_accHelpTopic)(BSTR *,VARIANT,long *) = 0;
		STDMETHOD(get_accKeyboardShortcut)(VARIANT,BSTR *) = 0;
		STDMETHOD(get_accFocus)(VARIANT *) = 0;
		STDMETHOD(get_accSelection)(VARIANT *) = 0;
		STDMETHOD(get_accDefaultAction)(VARIANT,BSTR *) = 0;
		STDMETHOD(accSelect)(long,VARIANT) = 0;
		STDMETHOD(accLocation)(long *,long *,long *,long *,VARIANT) = 0;
		STDMETHOD(accNavigate)(long,VARIANT,VARIANT *) = 0;
		STDMETHOD(accHitTest)(long,long,VARIANT *) = 0;
		STDMETHOD(accDoDefaultAction)(VARIANT) = 0;
		STDMETHOD(put_accName)(VARIANT,BSTR) = 0;
		STDMETHOD(put_accValue)(VARIANT,BSTR) = 0;
};
#endif // __cplusplus

#define xIID_IFileOperation {0x947aab5f,0x0a5c,0x4c13,{0xb4,0xd6,0x4b,0xf7,0x83,0x6f,0xc9,0xf8}}
#define IFileOperationProgressSink void
#define IOperationsProgressDialog void
#define IPropertyChangeArray void
#undef INTERFACE
#define INTERFACE xIFileOperation
DECLARE_INTERFACE_(xIFileOperation,IUnknown)
{
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
	STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
	STDMETHOD_(ULONG,Release) (THIS) PURE;

	STDMETHOD(Advise)(THIS_ IFileOperationProgressSink *,DWORD *pdwCookie) PURE;
	STDMETHOD(Unadvise)(THIS_ DWORD) PURE;
	STDMETHOD(SetOperationFlags)(THIS_ DWORD) PURE;
	STDMETHOD(SetProgressMessage)(THIS_ LPCWSTR) PURE;
	STDMETHOD(SetProgressDialog)(THIS_ IOperationsProgressDialog *) PURE;
	STDMETHOD(SetProperties)(THIS_ IPropertyChangeArray *) PURE;
	STDMETHOD(SetOwnerWindow)(THIS_ HWND hwndOwner) PURE;
	STDMETHOD(ApplyPropertiesToItem)(THIS_ xIShellItem *) PURE;
	STDMETHOD(ApplyPropertiesToItems)(THIS_ IUnknown *) PURE;
	STDMETHOD(RenameItem)(THIS_ xIShellItem *, LPCWSTR, IFileOperationProgressSink *) PURE;
	STDMETHOD(RenameItems)(THIS_ IUnknown *, LPCWSTR ) PURE;
	STDMETHOD(MoveItem)(THIS_ xIShellItem *, xIShellItem *, LPCWSTR, IFileOperationProgressSink *) PURE;
	STDMETHOD(MoveItems)(THIS_ IUnknown *, xIShellItem *) PURE;
	STDMETHOD(CopyItem)(THIS_ xIShellItem *, xIShellItem *, LPCWSTR, IFileOperationProgressSink *) PURE;
	STDMETHOD(CopyItems)(THIS_ IUnknown *, xIShellItem *) PURE;
	STDMETHOD(DeleteItem)(THIS_ xIShellItem *, IFileOperationProgressSink *) PURE;
	STDMETHOD(DeleteItems)(THIS_ IUnknown *) PURE;
	STDMETHOD(NewItem)(THIS_ xIShellItem *, DWORD, LPCWSTR, LPCWSTR, IFileOperationProgressSink *) PURE;
	STDMETHOD(PerformOperations)(THIS) PURE;
	STDMETHOD(GetAnyOperationsAborted)(THIS_ BOOL *) PURE;
};
