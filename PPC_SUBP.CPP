/*-----------------------------------------------------------------------------
	Paper Plane cUI								sub on cpp
-----------------------------------------------------------------------------*/
#define _PPC_SUBP_

#include "WINAPI.H"
#include <string.h>
#include <shlobj.h>
#include <ole2.h>
#include "WINOLE.H"

#ifndef WINEGCC
#define UbstrVal bstrVal
#else
#include <wchar.h>
#define UbstrVal n1.n2.n3.bstrVal
#endif

#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

#define UNICODE_LRM 0x200e
#define UNICODE_RLM 0x200f

#if !defined(RELEASE) && 0
	#define MSGMSG(t,s) XMessage(NULL,NULL,XM_DbgLOG,T(t),s);
	#define MSGMSGA(t,s) MSGMSG(t,s)
#else
	#define MSGMSG(t,s)
	#define MSGMSGA(t,s) UnUsedParam(s);
#endif

typedef struct {
	ThSTRUCT *thEcdata;
	COLUMNEXTDATAINFO *cndi;
	SHCOLUMNDATA scd;
	GETINFOTIPCALLBACK callbackfunc;
	void *data;
} GetColumnDataMainStruct;

PWSTR GetPropertyValue(GetColumnDataMainStruct *gcdms,COLUMNEXTDATAINFO *cndi);
const TCHAR GetColumnDataMainErrorMsg[] = T("error");

const IID XIID_IQueryInfo = {0x00021500, 0x0000,0x0000, {0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46}};

INT_PTR (WINAPI *DPropertySheet)(LPCPROPSHEETHEADER) = NULL;
HPROPSHEETPAGE (WINAPI *DCreatePropertySheetPage)(LPCPROPSHEETPAGE) = NULL;

LOADWINAPISTRUCT COMCTL32DLL[] = {
	LOADWINAPI1T(PropertySheet),
	LOADWINAPI1T(CreatePropertySheetPage),
	{NULL,NULL}
};

void (STDAPICALLTYPE *DVariantInit)(VARIANTARG * pvarg);
HRESULT (STDAPICALLTYPE *DVariantClear)(VARIANTARG * pvarg);
HRESULT (STDAPICALLTYPE *DVariantChangeType)(VARIANTARG * pvargDest,VARIANTARG *pvarSrc,USHORT wFlags,VARTYPE vt);

HRESULT (STDAPICALLTYPE *DGetActiveObject)(REFCLSID rclsid, void * pvReserved,IUnknown ** ppunk);
HRESULT (STDAPICALLTYPE *DRegisterActiveObject)(IUnknown * punk,REFCLSID rclsid,DWORD dwFlags, DWORD *pdwRegister);
HRESULT (STDAPICALLTYPE *DRevokeActiveObject)(DWORD dwRegister,void *pvReserved);

LOADWINAPISTRUCT OLEAUT32APIS[] = {
	LOADWINAPI1(VariantInit),
	LOADWINAPI1(VariantClear),
	LOADWINAPI1(VariantChangeType),

	LOADWINAPI1(GetActiveObject),
	LOADWINAPI1(RegisterActiveObject),
	LOADWINAPI1(RevokeActiveObject),
	{NULL,NULL}
};
HANDLE hOleaut32 = NULL;
HANDLE hPropsys = NULL;

#define PAGEMAX 20
#define SYNCPROPPOSNAME T("SyncPrp")
const TCHAR PHandlersKey[] = T("shellex\\PropertySheetHandlers");
const TCHAR PAllKey[] = T("*");
// const TCHAR PDirectoryKey[] = T("Directory");
// const TCHAR PAllFileKey[] = T("AllFilesystemObjects");
const TCHAR PropThreadThreadName[] = T("Fit Prop");

WNDPROC OldPropProc;
HHOOK OldPropKeyProc;
HWND hPPWnd;
void AddExtProps(LPITEMIDLIST idlist,LPDATAOBJECT pDataObject,const TCHAR *tname,HPROPSHEETPAGE *page,int *pages);

// カラム拡張関連 - XPまで ====================================================
TCHAR *GetColumnExtTextInfo(PPC_APPINFO *cinfo,int cellcolumn,TCHAR *dst)
{
	COLUMNDATASTRUCT *cdsptr;
	COLUMNEXTDATAINFO *cndi;

	if ( (cellcolumn < 0) || (cinfo->ColumnExtDlls.bottom == NULL) ){
		return dst;
	}
	cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + cellcolumn);
	for ( ; ; ){
		if ( cdsptr->textoffset ){
			DWORD index = cdsptr->itemindex,offset;

			offset = *(DWORD *)cinfo->ColumnExtDlls.bottom;
			while ( offset < cinfo->ColumnExtDlls.top ){
				cndi = (COLUMNEXTDATAINFO *)(BYTE *)(cinfo->ColumnExtDlls.bottom + offset);
				if ( index-- == 0 ){
					dst += wsprintf(dst,T("\r\n%s\t:"),(TCHAR *)(BYTE *)(cinfo->ColumnExtDlls.bottom + offset + sizeof(COLUMNEXTDATAINFO)) );
					break;
				}
				offset = cndi->next;
			}
			dst += wsprintf(dst,T("%s"),(TCHAR *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->textoffset) );
		}
		if ( cdsptr->nextoffset == 0 ) break;
		cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->nextoffset);
	}
	return dst;
}

WORD GetColumnExtItemIndex(PPC_APPINFO *cinfo,const TCHAR *itemname)
{
	WORD itemindex = DFC_COLUMNEX;
	COLUMNEXTDATAINFO *cndi;
	DWORD offset;

	if ( cinfo->ColumnExtDlls.bottom == NULL ){
		GetColumnExtMenu(&cinfo->ColumnExtDlls,cinfo->RealPath,NULL,0);
	}

	offset = *(DWORD *)cinfo->ColumnExtDlls.bottom;
	while ( offset < cinfo->ColumnExtDlls.top ){
		cndi = (COLUMNEXTDATAINFO *)(BYTE *)(cinfo->ColumnExtDlls.bottom + offset);
		if ( !tstrcmp(itemname,(TCHAR *)(BYTE *)(cinfo->ColumnExtDlls.bottom + offset + sizeof(COLUMNEXTDATAINFO))) ){
			return itemindex;
		}
		offset = cndi->next;
		itemindex++;
	}
	return DFC_FAULT;
}

COLUMNEXTDATAINFO *GetColumnExtDataInfo(ThSTRUCT *thEcdata,DWORD index)
{
	COLUMNEXTDATAINFO *cndi;
	DWORD offset;

	if ( hOleaut32 == NULL ){
		hOleaut32 = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUT32APIS,LOADWINAPI_LOAD);
		if ( hOleaut32 == NULL ) return NULL;
	}
	if ( thEcdata->bottom == NULL ) return NULL;
	offset = *(DWORD *)thEcdata->bottom;
	while ( offset < thEcdata->top ){
		cndi = (COLUMNEXTDATAINFO *)(BYTE *)(thEcdata->bottom + offset);
		if ( index-- == 0 ) return cndi;
		offset = cndi->next;
	}
	return NULL;
}

#ifndef UNICODE
void USEFASTCALL FixPropTextA(WCHAR *src,char *dest)
{
	WCHAR *sp,*dp;

	sp = dp = src;
	while ( *sp != '\0' ){
		if ( (*sp != UNICODE_LRM) && (*sp != UNICODE_RLM) ) *dp++ = *sp;
		sp++;
	}
	*dp = '\0';
	UnicodeToAnsi(src,dest,VFPS);
}
#endif

COLUMNEXTDATAINFO *ChainPropertyList(GetColumnDataMainStruct *gcdms,COLUMNEXTDATAINFO *cndi)
{
	DWORD offset;
	const TCHAR *idname;
	COLUMNEXTDATAINFO *newcndi = NULL;
	ThSTRUCT *thEcdata = gcdms->thEcdata;

	// 同名がないか調べる
	offset = cndi->next;
	idname = (const TCHAR *)(BYTE *)((BYTE *)cndi + sizeof(COLUMNEXTDATAINFO));

	while ( offset < thEcdata->top ){
		newcndi = (COLUMNEXTDATAINFO *)(BYTE *)(thEcdata->bottom + offset);
		if ( !tstrcmp(idname,(const TCHAR *)(BYTE *)((BYTE *)newcndi + sizeof(COLUMNEXTDATAINFO))) ){
			// 発見
			cndi->next_pkey = offset;
			break;
		}
		offset = newcndi->next;
	}
	if ( cndi->next_pkey != 0 ) return newcndi;
	cndi->next_pkey = 1; // 検索終了
	return NULL;
}

void GetColumnDataMain(GetColumnDataMainStruct *gcdms)
{
	COLUMNEXTDATAINFO *cndi;
	COLUMNEXTDATAINFO_ICP *icps;
	VARIANT var;

	cndi = gcdms->cndi;
	if ( cndi == NULL ){
		gcdms->callbackfunc(gcdms->data,GetColumnDataMainErrorMsg);
		return;
	}
	icps = &cndi->icps;

	DVariantInit(&var);

	// XP 用
	if ( icps->Icp != NULL ) for (;;){
		if ( icps->Icp->GetItemData(&cndi->id.scid,&gcdms->scd,&var) == S_OK ){
			VARIANT newvar;
			DVariantInit(&newvar);
			DVariantChangeType(&newvar,&var,0,VT_BSTR);

			#ifndef UNICODE
				TCHAR buf[VFPS];

				UnicodeToAnsi(newvar.UbstrVal,buf,VFPS);

				gcdms->callbackfunc(gcdms->data,buf);
			#else
				gcdms->callbackfunc(gcdms->data,newvar.UbstrVal);
			#endif
			::DVariantClear(&newvar);
			::DVariantClear(&var);
			return;
		}
		// 他に取得が可能か？
		if ( icps->next != 0 ){
			icps = (COLUMNEXTDATAINFO_ICP *)(BYTE *)(gcdms->thEcdata->bottom + icps->next);
			continue;
		}
		// Vista以降用取得が可能か？
		if ( cndi->next_pkey == 1 ) return;
		if ( cndi->next_pkey != 0 ){
			cndi = (COLUMNEXTDATAINFO *)(BYTE *)(gcdms->thEcdata->bottom + cndi->next_pkey);
			break;
		}
		// cndi->next_pkey == 0
		cndi = ChainPropertyList(gcdms,cndi);
		if ( cndi != NULL ) break;
		return;
	}

	// Vista 以降用
	if ( cndi->icps.Icp == NULL ){
		PWSTR ValueText = GetPropertyValue(gcdms,cndi);
		if ( ValueText != NULL ){
		#ifndef UNICODE
			TCHAR buf[VFPS];

			FixPropTextA(ValueText,buf);
			gcdms->callbackfunc(gcdms->data,buf);
		#else
			gcdms->callbackfunc(gcdms->data,ValueText);
		#endif
			CoTaskMemFree(ValueText);
			return;
		}
	}

}

typedef struct {
	PPC_APPINFO *cinfo;
	ENTRYCELL *cell;
	DWORD CommentID;
} ExtExecCallbackStruct;

void WINAPI ExtExecCallback(ExtExecCallbackStruct *eecs,const TCHAR *text)
{
	SetComment(eecs->cinfo,eecs->CommentID,eecs->cell,text);
}

void ExtExec(PPC_APPINFO *cinfo,ThSTRUCT *thEcdata,DWORD index,DWORD CommentID)
{
	GetColumnDataMainStruct gcdms;
	ExtExecCallbackStruct eecs;
	int work;

	gcdms.thEcdata = thEcdata;
	gcdms.cndi = GetColumnExtDataInfo(thEcdata,index);
	gcdms.callbackfunc = (GETINFOTIPCALLBACK)ExtExecCallback;
	gcdms.data = &eecs;
	eecs.cinfo = cinfo;
	eecs.CommentID = CommentID;

	InitEnumMarkCell(cinfo,&work);
	while ( (eecs.cell = EnumMarkCell(cinfo,&work)) != NULL ){
		gcdms.scd.dwFlags = 0;
		gcdms.scd.dwFileAttributes = eecs.cell->f.dwFileAttributes;
		#ifndef UNICODE
			char fname[VFPS];

			VFSFullPath(fname,eecs.cell->f.cFileName,cinfo->RealPath);
			AnsiToUnicode(fname,gcdms.scd.wszFile,MAX_PATH);
			if ( gcdms.scd.wszFile[0] != '\0' ){
				gcdms.scd.pwszExt = strrchrW(gcdms.scd.wszFile + 1,'.');
				if ( gcdms.scd.pwszExt == NULL ){
					gcdms.scd.pwszExt = gcdms.scd.wszFile + strlenW(gcdms.scd.wszFile);
				}else{
					gcdms.scd.pwszExt++;
				}
			}else{
				gcdms.scd.pwszExt = gcdms.scd.wszFile;
			}
		#else
			if ((tstrlen(eecs.cell->f.cFileName) + tstrlen(cinfo->RealPath)) >= (VFPS - 1)) break;
			VFSFullPath(gcdms.scd.wszFile,eecs.cell->f.cFileName,cinfo->RealPath);
			if ( *(eecs.cell->f.cFileName + eecs.cell->ext) == '\0' ){
				gcdms.scd.pwszExt = gcdms.scd.wszFile + eecs.cell->ext;
			}else{
				gcdms.scd.pwszExt = gcdms.scd.wszFile + eecs.cell->ext + 1;
			}
		#endif
		GetColumnDataMain(&gcdms);
	}
	::Repaint(cinfo);
}

void ExtGetData(ThSTRUCT *thEcdata,DWORD index,const TCHAR *filename,DWORD attributes,GETINFOTIPCALLBACK callbackfunc,void *data)
{
	GetColumnDataMainStruct gcdms;

	gcdms.thEcdata = thEcdata;
	gcdms.cndi = GetColumnExtDataInfo(thEcdata,index);
	gcdms.callbackfunc = callbackfunc;
	gcdms.data = data;

	gcdms.scd.dwFlags = 0;
	gcdms.scd.dwFileAttributes = attributes;
	#ifndef UNICODE
		AnsiToUnicode(filename,gcdms.scd.wszFile,MAX_PATH);
	#else
		tstrcpy(gcdms.scd.wszFile,filename);
	#endif
	gcdms.scd.pwszExt = strrchrW(gcdms.scd.wszFile,'.');
	if ( gcdms.scd.pwszExt == NULL ){
		gcdms.scd.pwszExt = gcdms.scd.wszFile + strlenW(gcdms.scd.wszFile);
	}else{
		gcdms.scd.pwszExt++;
	}
	GetColumnDataMain(&gcdms);
}

// カラム拡張関連 - Vista以降 =================================================
const IID XIID_IPropertyStore = {0x886d8eeb,0x8cf2,0x4446, {0x8d,0x02, 0xcd,0xba,0x1d,0xbd,0xcf,0x99}};
const IID XIID_IPropertyDescription = {0x6f79d558,0x3e96,0x4549,{0xa1,0xd1,0x7d,0x75,0xd2,0x28,0x88,0x14}};

#ifndef PSSTDAPI
typedef enum {
	GPS_DEFAULT = 0,
	GPS_BESTEFFORT = 0x40,
} GETPROPERTYSTOREFLAGS;
#endif

// OLE32.dll
DefineWinAPI(HRESULT,PropVariantClear,(PROPVARIANT *pvar));

// Shell32.dll
DefineWinAPI(HRESULT,SHGetPropertyStoreFromParsingName,(PCWSTR pszPath,IBindCtx *pbc,GETPROPERTYSTOREFLAGS flags,REFIID riid,void **ppv));

// Propsys.dll
HRESULT (WINAPI *DPSFormatForDisplayAlloc)(REFPROPERTYKEY key,REFPROPVARIANT propvar,PROPDESC_FORMAT_FLAGS pdff,PWSTR *ppszDisplay);
//HRESULT (WINAPI *DPSGetNameFromPropertyKey)(REFPROPERTYKEY propkey,PWSTR *ppszCanonicalName);
//HRESULT (WINAPI *DPSGetPropertyKeyFromName)(PCWSTR pszName,PROPERTYKEY *ppropkey);
HRESULT (WINAPI *DPSGetPropertyDescription)(REFPROPERTYKEY propkey,REFIID riid,void **ppv);

LOADWINAPISTRUCT PROPSAPIS[] = {
	LOADWINAPI1(PSFormatForDisplayAlloc),
//	LOADWINAPI1(PSGetNameFromPropertyKey),
//	LOADWINAPI1(PSGetPropertyKeyFromName),
	LOADWINAPI1(PSGetPropertyDescription),
	{NULL,NULL}
};

BOOL InitPropSys(void)
{
	GETDLLPROC(GetModuleHandle(StrShell32DLL),SHGetPropertyStoreFromParsingName);
	if ( DSHGetPropertyStoreFromParsingName == NULL ) return FALSE;
	if ( DPSFormatForDisplayAlloc == NULL ){
		hPropsys = LoadWinAPI("PROPSYS.DLL",NULL,PROPSAPIS,LOADWINAPI_LOAD);
		if ( hPropsys == NULL ) return FALSE;
	}
	GETDLLPROC(GetModuleHandle(T("OLE32.DLL")),PropVariantClear);
	return TRUE;
}

void VistaProperties(const TCHAR *filename,ThSTRUCT *text)
{
	#ifdef UNICODE
		#define propfilename filename
	#else
		WCHAR propfilename[VFPS];
		AnsiToUnicode(filename,propfilename,VFPS);
	#endif
	HRESULT hr;
	IPropertyStore *ps;

	if ( DSHGetPropertyStoreFromParsingName == NULL ){
		if ( InitPropSys() == FALSE ) return;
	}

#pragma warning(suppress: 6011) // InitPropSys で DSHGetPropertyStoreFromParsingName が初期化
	hr = DSHGetPropertyStoreFromParsingName(propfilename,NULL,GPS_BESTEFFORT,XIID_IPropertyStore,reinterpret_cast<void**>(&ps));

	if ( SUCCEEDED(hr) ){
		DWORD propcount;

		hr = ps->GetCount(&propcount);
		if ( SUCCEEDED(hr) && propcount ){
			ThCatString(text,T("Columns:\r\n"));
			for ( DWORD index = 0; index < propcount; index++ ){
				PROPERTYKEY propertyKey;

				hr = ps->GetAt(index,&propertyKey);
				if ( SUCCEEDED(hr) ){
					PROPVARIANT PropValue;

					hr = ps->GetValue(propertyKey,&PropValue);
					if ( SUCCEEDED(hr) ){
						PWSTR ValueText;

						hr = DPSFormatForDisplayAlloc(propertyKey,PropValue, PDFF_DEFAULT,&ValueText);
						if ( SUCCEEDED(hr) ){
							IPropertyDescription *pd;

							hr = DPSGetPropertyDescription(propertyKey,XIID_IPropertyDescription,reinterpret_cast<void**>(&pd));
							if( SUCCEEDED(hr) ){
								PWSTR pszPropertyLabel = NULL;

								hr = pd->GetDisplayName(&pszPropertyLabel);
								if ( SUCCEEDED(hr) ){
									TCHAR buf[0x1000];

									#ifdef UNICODE
										wsprintf(buf,L"%s\t:%s\r\n",pszPropertyLabel,ValueText);
									#else
									{
										char *dest;

										UnicodeToAnsi(pszPropertyLabel,buf,MAX_PATH);
										dest = buf + tstrlen(buf);
										*dest++ = '\t';
										*dest++ = ':';
										FixPropTextA(ValueText,dest);
										dest += tstrlen(dest);
										*dest++ = '\r';
										*dest++ = '\n';
										*dest = '\0';
									}
									#endif
									ThCatString(text,buf);
									CoTaskMemFree(pszPropertyLabel);
								}
								pd->Release();
							}
							CoTaskMemFree(ValueText);
						}
						DPropVariantClear(&PropValue);
					}
				}
			}
		}
		ps->Release();
	}
	return;
}

PWSTR GetPropertyValue(GetColumnDataMainStruct *gcdms,COLUMNEXTDATAINFO *cndi)
{
	IPropertyStore *ps = NULL;
	PWSTR ValueText = NULL;
	PROPVARIANT PropValue;

	if ( DSHGetPropertyStoreFromParsingName == NULL ){
		if ( InitPropSys() == FALSE ) return NULL;
	}
#pragma warning(suppress: 6011) // InitPropSys で DSHGetPropertyStoreFromParsingName が初期化
	if ( FAILED(DSHGetPropertyStoreFromParsingName(gcdms->scd.wszFile,NULL,GPS_BESTEFFORT,XIID_IPropertyStore,reinterpret_cast<void**>(&ps))) ){
		return NULL;
	}
	for (;;){
		if ( SUCCEEDED(ps->GetValue(cndi->id.pkey,&PropValue)) ){
			HRESULT hr = DPSFormatForDisplayAlloc(cndi->id.pkey,PropValue,PDFF_DEFAULT,&ValueText);
			DPropVariantClear(&PropValue);
			if ( SUCCEEDED(hr) ) break;
		}
		// 他に取得が可能か？
		if ( cndi->next_pkey == 1 ) break;
		if ( cndi->next_pkey != 0 ){
			cndi = (COLUMNEXTDATAINFO *)(BYTE *)(gcdms->thEcdata->bottom + cndi->next_pkey);
			continue;
		}
		// cndi->next_pkey == 0
		cndi = ChainPropertyList(gcdms,cndi);
		if ( cndi == NULL ) break;
	}
	ps->Release();
	return ValueText;
}

// チップ拡張関連 =========================================================
BOOL GetInfoTipText(const TCHAR *filename,int extlen,GETINFOTIPCALLBACK callbackfunc,void *data)
{
	TCHAR name[VFPS],idname[MAX_PATH];
	LPMALLOC pMA;
	CLSID hid;
	IPersistFile *ppf;
	HRESULT hres;

	if ( FAILED(SHGetMalloc(&pMA)) ) return FALSE;

	wsprintf(name,T("%s\\ShellEx\\{00021500-0000-0000-C000-000000000046}"),
			filename + extlen);
	idname[0] = '\0';
	GetRegString(HKEY_CLASSES_ROOT,name,NilStr,idname,TSIZEOF(idname));
	if ( idname[0] != '\0' ){
		#undef tidname
		#ifndef UNICODE
			WCHAR idnameW[MAX_PATH];
			#define tidname idnameW

			AnsiToUnicode(idname,idnameW,MAX_PATH);
		#else
			#define tidname idname
		#endif
		if ( SUCCEEDED(::CLSIDFromString(tidname, &hid)) ){
			if ( SUCCEEDED( ::CoCreateInstance(hid, NULL, CLSCTX_INPROC_SERVER,
				IID_IPersistFile, (LPVOID *)&ppf)) ){
			#ifdef UNICODE
				hres = ppf->Load(filename, STGM_READ);
			#else
				WCHAR wsz[VFPS];

				::MultiByteToWideChar(CP_ACP, 0, filename, -1, wsz, TSIZEOF(wsz));
				hres = ppf->Load(wsz, STGM_READ);	// link を取得
			#endif
				if ( SUCCEEDED(hres) ){
					IQueryInfo *pqi;

					hres = ppf->QueryInterface(XIID_IQueryInfo, (LPVOID *)&pqi);
					if ( SUCCEEDED(hres) ){
						WCHAR *tiptext;

						if ( SUCCEEDED(pqi->GetInfoTip(0, &tiptext)) && tiptext ){
						#ifdef UNICODE
							callbackfunc(data, tiptext);
						#else
							TCHAR buf[0x1000];
							buf[0] = '\0';

							UnicodeToAnsi(tiptext, buf, 0x1000 - 1);
							buf[0x1000 - 1] = '\0';
							callbackfunc(data, buf);
						#endif
							pMA->Free(tiptext);
						}
						pqi->Release();
					}
				}
				ppf->Release();
			}
		}
	}
	pMA->Release();
	return TRUE;
}

typedef struct {
	PPC_APPINFO *cinfo;
	ENTRYCELL *cell;
} EXTINFOTIPSTRUCT;

void WINAPI ExtInfoTipCallback(EXTINFOTIPSTRUCT *eits,const TCHAR *text)
{
	SetComment(eits->cinfo,0,eits->cell,text);
}

void ExtInfoTip(PPC_APPINFO *cinfo)
{
	EXTINFOTIPSTRUCT eits;
	int work;
	TCHAR name[VFPS];

	eits.cinfo = cinfo;

	InitEnumMarkCell(cinfo,&work);
	while ( (eits.cell = EnumMarkCell(cinfo,&work)) != NULL ){
		const TCHAR *ls;

		VFSFullPath(name,eits.cell->f.cFileName,cinfo->RealPath);
		ls = VFSFindLastEntry(name);
		GetInfoTipText(name,(ls - name) + FindExtSeparator(ls),(GETINFOTIPCALLBACK)ExtInfoTipCallback,(void *)&eits);
	}
	::Repaint(cinfo);
}

//  ================================================
#if !NODLL
HRESULT GetLink(HWND hWnd,const TCHAR *LinkFile,TCHAR *Original)
{
	HRESULT hres;
	IShellLink *psl;
	WIN32_FIND_DATA ff;
										// IShellLink interface を取得
	hres = ::CoCreateInstance(CLSID_ShellLink,NULL,
			CLSCTX_INPROC_SERVER,IID_IShellLink,(LPVOID *)&psl);
	if ( SUCCEEDED(hres) ){
		IPersistFile *ppf;
									// IShellLink から IPersistFile interface
		hres = psl->QueryInterface(IID_IPersistFile,(LPVOID *)&ppf);
		if ( SUCCEEDED(hres) ){
			#ifdef UNICODE
				hres = ppf->Load(LinkFile,STGM_READ);	// link を取得
			#else
				WCHAR wsz[VFPS];

				::MultiByteToWideChar(CP_ACP,0,LinkFile,-1,wsz,TSIZEOF(wsz));
				hres = ppf->Load(wsz,STGM_READ);	// link を取得
			#endif
			if ( SUCCEEDED(hres) ){
				hres = psl->Resolve(hWnd,SLR_ANY_MATCH);
				if ( SUCCEEDED(hres) ){
					hres = psl->GetPath(Original,MAX_PATH,&ff,SLGP_UNCPRIORITY);
					if ( FAILED(hres) || (Original[0] == '\0') ){
						LPITEMIDLIST idl;

						if ( SUCCEEDED(psl->GetIDList(&idl)) ){
							LPSHELLFOLDER pSF;

							SHGetDesktopFolder(&pSF);
							GetIDLSub(Original,pSF,idl);
							pSF->Release();

							FreePIDL(idl);
							hres = S_OK;
						}
					}
				}
			}
			ppf->Release();
		}
		psl->Release();
	}
	return hres;
}
#endif
// 同期プロパティ関連 =========================================================

struct PROPOBJECT {
	LPITEMIDLIST idlist;
	LPSHELLFOLDER ShellFolder;
	LPDATAOBJECT DataObject;
};
PROPOBJECT Props = {NULL,NULL,NULL};

BOOL CALLBACK PropPageFunc(HPROPSHEETPAGE hPSP,LPARAM lParam)
{
	*((HPROPSHEETPAGE *)lParam) = hPSP;
	return TRUE;
}

void FreePropCom(PROPOBJECT *po)
{
	if ( po->DataObject != NULL )	po->DataObject->Release();
	if ( po->ShellFolder != NULL )	po->ShellFolder->Release();
	if ( po->idlist != NULL ) FreePIDL(po->idlist);
	po->idlist = NULL;
	po->ShellFolder = NULL;
	po->DataObject = NULL;
}

void SavePropPos(HWND hWnd)
{
	WINPOS wpos = {{0,0,0,0},0,0};

	GetWindowRect(hWnd,&wpos.pos);
	SetCustTable(Str_WinPos,SYNCPROPPOSNAME,&wpos,sizeof(wpos));
}

INT_PTR CALLBACK PageDlgProc(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg){
		case WM_INITDIALOG: {
			TCHAR buf[VFPS],*p;
			SYNCPROPINFO *info;
			HICON hFileIcon;
			DWORD attr;
			RECT box;

			info = (SYNCPROPINFO *)(((PROPSHEETPAGE *)lParam)->lParam);

			tstrcpy(buf,info->filename);
			attr = GetFileAttributesL(buf);
			if ( attr == BADATTR ) attr = 0;
			if ( OSver.dwMajorVersion >= 6 ){
				GetWindowRect(GetDlgItem(hDlg,IDS_ICON),&box);
				box.bottom -= box.top;
			}else{
				box.bottom = 32;
			}

			EnterCriticalSection(&SHGetFileInfoSection);
			hFileIcon = LoadFileIcon(info->filename,attr,SHGFI_ICON,
					box.bottom,NULL);
			LeaveCriticalSection(&SHGetFileInfoSection);
			SendDlgItemMessage(hDlg,IDS_ICON,STM_SETICON,(WPARAM)hFileIcon,0);

			p = VFSFindLastEntry(buf);
			if ( (p == buf) || (*p == '\0') ){
				SetDlgItemText(hDlg,IDE_FILENAME,p);
				SetDlgItemText(hDlg,IDE_PPATH,NilStr);
				SetDlgItemText(hDlg,IDE_FILETYPE,NilStr);
				SetDlgItemText(hDlg,IDE_PASSOC,NilStr);
			}else{
				TCHAR *extp,buf2[0x100];
				SetDlgItemText(hDlg,IDE_FILENAME,(*p == '\\') ? p + 1 : p);
				extp = p + FindExtSeparator(p);

				SetDlgItemText(hDlg,IDE_PASSOC,extp);
				if ( *extp == '.' ){
					buf2[0] = '\0';
					GetRegString(HKEY_CLASSES_ROOT,extp,NilStr,buf2,TSIZEOF(buf2));
					if ( buf2[0] != '\0' ){
						GetRegString(HKEY_CLASSES_ROOT,buf2,NilStr,buf2,TSIZEOF(buf2));
					}
					SetDlgItemText(hDlg,IDE_FILETYPE,buf2);
				}
				*p = '\0';
				SetDlgItemText(hDlg,IDE_PPATH,buf);
			}
			FormatNumber(buf,XFN_SEPARATOR | XFN_MINKILO,6,info->ff.nFileSizeLow,info->ff.nFileSizeHigh);
			p = buf + tstrlen(buf);
			*p++ = ' ';
			*p++ = '(';
			FormatNumber(p,XFN_SEPARATOR,20,info->ff.nFileSizeLow,info->ff.nFileSizeHigh);
			p += tstrlen(p);
			*p++ = ')';
			*p = '\0';
			SetDlgItemText(hDlg,IDE_PSIZE,buf);
			CnvDateTime(buf,NULL,NULL,&info->ff.ftCreationTime);
			SetDlgItemText(hDlg,IDE_PCREATETIME,buf);
			CnvDateTime(buf,NULL,NULL,&info->ff.ftLastWriteTime);
			SetDlgItemText(hDlg,IDE_PMODIFYTIME,buf);
			CnvDateTime(buf,NULL,NULL,&info->ff.ftLastAccessTime);
			SetDlgItemText(hDlg,IDE_PACCESSTIME,buf);
			break;
		}

		case WM_NOTIFY:
			#define NHPTR ((NMHDR *)lParam)
			switch( NHPTR->code ){
				case PSN_APPLY:
					break;

				default:
					return FALSE;
			}
			break;
			#undef NHPTR
		case WM_CLOSE:	// ※PPxDialogHelper では WM_CLOSE で EndDialogするから
			break;

		case WM_DESTROY:
			DestroyIcon((HICON)SendDlgItemMessage(hDlg,IDS_ICON,STM_GETICON,0,0));
			break;

		default:	// 特になし
			return PPxDialogHelper(hDlg,msg,wParam,lParam);
	}
	return TRUE;
}

int MakePropPages(SYNCPROPINFO *info,HPROPSHEETPAGE *page)
{
	int pages = 1;
	const TCHAR *p;
	TCHAR buf[VFPS];
	PROPOBJECT NewProps;
	PROPSHEETPAGE psp;
//	DWORD attr;

//	attr = GetFileAttributesL(info->filename);

// Page 0 の設定
	psp.dwSize = sizeof(psp);
	psp.dwFlags = 0;
	psp.hInstance = hInst;
	psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPGENERAL);
	psp.pfnDlgProc	= PageDlgProc;
	psp.lParam		= (LPARAM)info;
	page[0] = DCreatePropertySheetPage(&psp);

// Page 1 以降（拡張）の設定
	if ( FALSE == VFSMakeIDL(NilStr,&NewProps.ShellFolder,&NewProps.idlist,info->filename) ){
		return pages;
	}
	if ( SUCCEEDED(NewProps.ShellFolder->GetUIObjectOf(NULL,1,
				(LPCITEMIDLIST *)&NewProps.idlist,
				IID_IDataObject,NULL,(void **)&NewProps.DataObject) )){
	}

	// 拡張子に該当する拡張を取得
	p = VFSFindLastEntry(info->filename);
	p += FindExtSeparator(p);
	if ( *p != '\0' ){
										// 拡張子からキーを求める -------------
		if ( GetRegString(HKEY_CLASSES_ROOT,p,NilStr,buf,TSIZEOF(buf)) ){
										// アプリケーションのシェル -----------
			AddExtProps(NewProps.idlist,NewProps.DataObject,buf,page,&pages);
		}
	}

	// * の拡張を取得
	AddExtProps(NewProps.idlist,NewProps.DataObject,PAllKey,page,&pages);

/*  ● UNICODE 板で CoCreateInstance がデッドロック?を起こすことがあるので休止中 2016-7
	if ( attr != BADATTR ){
		AddExtProps(NewProps.idlist,NewProps.DataObject,
			( attr & FILE_ATTRIBUTE_DIRECTORY ) ? PDirectoryKey : PAllFileKey,
			page,&pages);
	}
*/
	// Props を更新
	FreePropCom(&Props);
	Props = NewProps;

	return pages;
}

void InitProcWindow(HWND hWnd)
{
	RECT pbox,bbox;
	HWND hBwnd;
	LPARAM lFont;
	WINPOS wpos;
	TCHAR PropTab[VFPS];

	hPropWnd = hWnd;
	lFont = SendMessage(hWnd,WM_GETFONT,0,0);

	if ( NO_ERROR == GetCustTable(Str_WinPos,SYNCPROPPOSNAME,&wpos,sizeof(wpos)) ){
		SetWindowPos(hWnd,NULL,wpos.pos.left,wpos.pos.top,0,0,SWP_NOZORDER | SWP_NOSIZE);
	}

#if 0	// 窓に連結するためのコード。未使用
	SetParent(hPropWnd,hParentWnd);
	SetWindowLongPtr(hPropWnd,GWL_STYLE,
		(GetWindowLongPtr(hPropWnd,GWL_STYLE) & ~(WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW | WS_DLGFRAME)) | WS_CHILD  );

	GetWindowRect(hPPWnd,&pbox);
	SetForegroundWindow(hPPWnd);
#endif
										// ボタンを追加
	GetClientRect(hWnd,&pbox);
	GetWindowRect(GetDlgItem(hWnd,IDOK),&bbox);
	hBwnd = CreateWindow(T("BUTTON"),T("&<"),BS_CENTER | WS_CHILD | WS_VISIBLE | WS_TABSTOP,pbox.left + 10,pbox.bottom - (bbox.bottom-bbox.top) - 5,(bbox.bottom-bbox.top),(bbox.bottom-bbox.top),hWnd,(HMENU)K_up,hInst,NULL);
	SendMessage(hBwnd,WM_SETFONT,lFont,0);
	hBwnd = CreateWindow(T("BUTTON"),T("&>"),BS_CENTER | WS_CHILD | WS_VISIBLE | WS_TABSTOP,pbox.left + 15+(bbox.bottom-bbox.top),pbox.bottom - (bbox.bottom-bbox.top) - 5,(bbox.bottom-bbox.top),(bbox.bottom-bbox.top),hWnd,(HMENU)K_dw,hInst,NULL);
	SendMessage(hBwnd,WM_SETFONT,lFont,0);

	PropTab[0] = '\0';
	GetCustTable(T("_others"),T("SyncPropTab"),PropTab,sizeof(PropTab));
	if ( PropTab[0] != '\0' ){
		int i,pages;
		TC_ITEM tie;
		TCHAR tabname[VFPS];
		HWND hTabWnd;

		hTabWnd = (HWND)SendMessage(hWnd,PSM_GETTABCONTROL,0,0);
		pages = SendMessage(hTabWnd,TCM_GETITEMCOUNT,0,0);

		// 表示ページを決定
		for ( i = 0 ; i < pages ; i++ ){
			tie.mask = TCIF_TEXT;
			tie.pszText = tabname;
			tie.cchTextMax = VFPS;
			tabname[0] = '\0';
			TabCtrl_GetItem(hTabWnd,i,&tie);
			if ( !tstrcmp(tabname,PropTab) ){
				SetProp(hWnd,PropThreadThreadName,(HANDLE)i);
				SendMessage(hWnd,PSM_SETCURSEL,i,0);
			}
		}
	}
}

void SetFileProcWindow(HWND hWnd,SYNCPROPINFO *info)
{
	int i,pages,newpages;
	HPROPSHEETPAGE page[PAGEMAX];
	TCHAR filename[VFPS],oldtitle[VFPS];
	int OldPage,FirstPage;
	HWND hTabWnd;
	TC_ITEM tie;

	FirstPage = (int)(DWORD_PTR)GetProp(hWnd,PropThreadThreadName);
	hTabWnd = (HWND)SendMessage(hWnd,PSM_GETTABCONTROL,0,0);
	OldPage = TabCtrl_GetCurSel(hTabWnd);
	oldtitle[0] = '\0';
	if ( FirstPage != OldPage ){ // ページ変更あり
		tie.mask = TCIF_TEXT;
		tie.pszText = oldtitle;
		tie.cchTextMax = VFPS;
		TabCtrl_GetItem(hTabWnd,OldPage,&tie);
		SetCustTable(T("_others"),T("SyncPropTab"),oldtitle,TSTRSIZE(oldtitle));
	}else{ // ページ変更無し
		GetCustTable(T("_others"),T("SyncPropTab"),oldtitle,sizeof(oldtitle));
	}

	tstrcpy(filename,info->filename);
	pages = (int)SendMessage(hTabWnd,TCM_GETITEMCOUNT,0,0);
	newpages = MakePropPages(info,page);
	if ( newpages ){
		SendMessage(hWnd,WM_SETREDRAW,FALSE,0);

		// 新しいページを末尾に追加
		for ( i = 0 ; i < newpages ; i++ ){
			SendMessage(hWnd,PSM_ADDPAGE,0,(LPARAM)page[i]);
		}
		// 古いページを削除
		for ( i = 0 ; i < pages ; i++ ){
		// PropSheet_UnChanged(hWnd,hwndPage);
			SendMessage(hWnd,PSM_REMOVEPAGE,0,0);
		}
		SetWindowText(hWnd,filename);
		// 表示ページを決定
		for ( i = 0 ; i < newpages ; i++ ){
			tie.mask = TCIF_TEXT;
			tie.pszText = filename;
			tie.cchTextMax = VFPS;
			filename[0] = '\0';
			TabCtrl_GetItem(hTabWnd,i,&tie);
			if ( !tstrcmp(filename,oldtitle) ) break;
		}
		if ( i >= newpages ) i = 0;
		SetProp(hWnd,PropThreadThreadName,(HANDLE)i);
		SendMessage(hWnd,PSM_SETCURSEL,i,0);
		SendMessage(hWnd,WM_SETREDRAW,TRUE,0);
		InvalidateRect(hWnd,NULL,TRUE);
	}
}

LRESULT CALLBACK ProcHookProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch ( uMsg ){
		case WM_DESTROY:
			SavePropPos(hWnd);
			break;

		case WM_COMMAND:
			if ( LOWORD(wParam) == K_up ){
				PostMessage(hPPWnd,WM_KEYDOWN,VK_UP,lParam);
			}
			if ( LOWORD(wParam) == K_dw ){
				PostMessage(hPPWnd,WM_KEYDOWN,VK_DOWN,lParam);
			}
			break;

		default:
			if ( uMsg == WM_PPXCOMMAND ){
				switch ( LOWORD(wParam) ){
					case KC_SYNCPROP_INIT:	// 初期化
						InitProcWindow(hWnd);
						break;
					case KC_SYNCPROP_SETFILE:	// ファイル指定
						SetFileProcWindow(hWnd,(SYNCPROPINFO *)lParam);
						break;
				}
			}
	}
	return CallWindowProc(OldPropProc,hWnd,uMsg,wParam,lParam);
}

LRESULT CALLBACK ProcKeyHookProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	if ( nCode >= 0 ){
		if ( !(DWORD)(lParam & B31) ){
			if ( (wParam == VK_PRIOR) || (wParam == VK_NEXT) ){
				if ( !(GetAsyncKeyState(VK_CONTROL) & KEYSTATE_PUSH) ){
					PostMessage(hPPWnd,WM_KEYDOWN,
							(wParam == VK_PRIOR) ? VK_UP : VK_DOWN,lParam);
					return 1;
				}
			}
		}
	}
	return CallNextHookEx(OldPropKeyProc,nCode,wParam,lParam);
}

int CALLBACK PropSheetProc(HWND hWnd,UINT uMsg,LPARAM lParam)
{
	if ( uMsg == PSCB_PRECREATE ){
		RECT pbox;

		GetWindowRect(hPPWnd,&pbox);

		((LPDLGTEMPLATE)lParam)->style &= ~(DS_CONTEXTHELP | DS_CENTER);
		// ↓効いていない？
		((LPDLGTEMPLATE)lParam)->x = (WORD)pbox.right;
		((LPDLGTEMPLATE)lParam)->y = (WORD)pbox.top;
	}else if ( uMsg == PSCB_INITIALIZED ){
		OldPropProc = (WNDPROC)SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LONG_PTR)ProcHookProc);

		OldPropKeyProc = SetWindowsHookEx(WH_KEYBOARD,
				(HOOKPROC)ProcKeyHookProc,NULL,GetCurrentThreadId());
		PostMessage(hWnd,WM_PPXCOMMAND,KC_SYNCPROP_INIT,0);
	}
	return 0;
}

void AddExtProps(LPITEMIDLIST idlist,LPDATAOBJECT pDataObject,const TCHAR *tname,HPROPSHEETPAGE *page,int *pages)
{
	IShellExtInit *pShellExt;
	LPSHELLPROPSHEETEXT pShellProp;
	HKEY hKey,hFolderKey;
	int enumno = 0;

	if ( ERROR_SUCCESS != ::RegOpenKeyEx(
			HKEY_CLASSES_ROOT,tname,0,KEY_READ,&hFolderKey) ){
		return;
	}
	::RegOpenKeyEx(hFolderKey,PHandlersKey,0,KEY_READ,&hKey);

	for ( ; ; ){
		TCHAR keyname[MAX_PATH];
		TCHAR idname[MAX_PATH];
		CLSID hid;
		DWORD s;
		FILETIME ft;
		HRESULT hres;

		// IShellExtInit を取得
		s = MAX_PATH;
		if ( ::RegEnumKeyEx(hKey,enumno++,keyname,&s,NULL,NULL,NULL,&ft) !=
				ERROR_SUCCESS ){
			break;
		}
		if ( keyname[0] == '{' ){
			tstrcpy(idname,keyname);
		}else{
			if ( GetRegString(hKey,keyname,NilStr,idname,TSIZEOF(idname))
					== FALSE ){
				continue;
			}
		}
		#ifndef UNICODE
			WCHAR idnameW[MAX_PATH];
			#define tidname idnameW

			AnsiToUnicode(idname,idnameW,MAX_PATH);
		#else
			#define tidname idname
		#endif
		if ( FAILED(::CLSIDFromString(tidname,&hid)) ) continue;

		hres = ::CoCreateInstance(hid,NULL,
				CLSCTX_INPROC_SERVER,IID_IShellExtInit,(LPVOID *)&pShellExt);
		if ( FAILED(hres) ) continue;
		// IShellPropSheetExt を取得
		if ( SUCCEEDED(pShellExt->Initialize(idlist,pDataObject,hFolderKey)) ){
			// ●この付近でハンドルが２つリークしているかも 2015-09
			if ( SUCCEEDED(pShellExt->QueryInterface(
						IID_IShellPropSheetExt,(LPVOID *)&pShellProp)) ){
				page[*pages] = NULL;
				pShellProp->AddPages(PropPageFunc,(LPARAM)&page[*pages]);
				if ( page[*pages] != NULL ) (*pages)++;
					//LPFNADDPROPSHEETPAGE
				pShellProp->Release();
			}
		}
		pShellExt->Release();
		if ( *pages >= PAGEMAX ) break;
	}
	::RegCloseKey(hKey);
	::RegCloseKey(hFolderKey);
}

DWORD WINAPI PropThread(SYNCPROPINFO *infoparam)
{
	SYNCPROPINFO info;
	THREADSTRUCT threadstruct = {PropThreadThreadName,XTHREAD_EXITENABLE | XTHREAD_TERMENABLE,NULL,0,0};
	PROPSHEETHEADER head;
	HPROPSHEETPAGE page[PAGEMAX];
	int pages;

	PPxRegisterThread(&threadstruct);
	info = *infoparam;
	::PPcHeapFree((void *)infoparam);

	if( FAILED(::OleInitialize(NULL)) ) goto nooleend;

	if ( LoadWinAPI(NULL,(HINSTANCE)LoadCommonControls(ICC_TAB_CLASSES),COMCTL32DLL,LOADWINAPI_HANDLE) == NULL ){
		goto end;
	}

	// プロパティシートを表示
	pages = MakePropPages(&info,page);
	head.dwSize		= sizeof(PROPSHEETHEADER);
	head.dwFlags	= PSH_USECALLBACK;
	head.hwndParent	= NULL;
	head.hInstance	= hInst;
	head.nPages		= pages;
	head.nStartPage = 0;
	head.pszCaption = info.filename;
	head.phpage = page;
	head.pfnCallback = PropSheetProc;
	DPropertySheet(&head);

	::FreePropCom(&Props);
	hPropWnd = NULL;
end:
	::OleUninitialize();
nooleend:
	::PPxUnRegisterThread();
	return 0;
}

void SyncProperties(HWND hWnd,SYNCPROPINFO *info)
{
	hPPWnd = hWnd;
	if ( info == NULL ){	// Prop の廃棄
		if ( hPropWnd != NULL ) PostMessage(hPropWnd,WM_CLOSE,0,0);
		return;
	}

	if ( hPropWnd != NULL ){	// Prop の変更
		if ( !IsWindowEnabled(hPropWnd) ){
			SendMessage(hWnd,WM_PPXCOMMAND,K_SETPOPLINENOLOG,(LPARAM)T("SyncProp. : Dialog is being used."));
			return;
		}
		SendMessage(hPropWnd,WM_PPXCOMMAND,KC_SYNCPROP_SETFILE,(LPARAM)info);
	}else{						// Prop を作成
		SYNCPROPINFO *infoparam;
		DWORD tmp;

		infoparam = (SYNCPROPINFO *)PPcHeapAlloc(sizeof(SYNCPROPINFO));
		*infoparam = *info;
		CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)
				  PropThread,(void *)infoparam,0,&tmp));
	}
}

IID xIID_IAccessible = xIID_IAccessibleIID;

HRESULT (STDAPICALLTYPE *DCreateStdAccessibleObject)(HWND,LONG,REFIID,void **);
LRESULT (STDAPICALLTYPE *DLresultFromObject)(REFIID,WPARAM,LPUNKNOWN);

BSTR (STDAPICALLTYPE *DSysAllocString)(const OLECHAR *) = NULL;

LOADWINAPISTRUCT OLEACCDLL[] = {
	LOADWINAPI1(CreateStdAccessibleObject),
	LOADWINAPI1(LresultFromObject),
	{NULL,NULL}
};

LOADWINAPISTRUCT OLEAUTDLL[] = {
	LOADWINAPI1(SysAllocString),
	{NULL,NULL}
};

class CPPcAccServer : public IAccessible , public IEnumVARIANT
{
private:
	long m_cRefCount;
	PPC_APPINFO *m_cinfo;
	ULONG m_enumCount;
	IAccessible *m_AccessibleServer;

public:
	CPPcAccServer(PPC_APPINFO *cinfo);
	~CPPcAccServer();

	// IUnknown
	STDMETHOD(QueryInterface)(REFIID riid,void **ppvObj);
	STDMETHOD_(ULONG,AddRef)(void);
	STDMETHOD_(ULONG,Release)(void);

	// IDispatch
	STDMETHOD(GetTypeInfoCount)(UINT *pctinfo);
	STDMETHOD(GetTypeInfo)(UINT itinfo,LCID lcid,ITypeInfo **pptinfo);
	STDMETHOD(GetIDsOfNames)(REFIID riid,
			OLECHAR **rgszNames,UINT cNames,LCID lcid,
			DISPID *rgdispid);
	STDMETHOD(Invoke)(DISPID dispidMember,REFIID riid,LCID lcid,WORD wFlags,
			DISPPARAMS *pdispparams,VARIANT *pvarResult,EXCEPINFO *pexcepinfo,
			UINT *puArgErr);

	// IAccessible
	STDMETHOD(get_accParent)(IDispatch **ppdispParent);
	STDMETHOD(get_accChildCount)(long *pcountChildren);
	STDMETHOD(get_accChild)(VARIANT varChild,IDispatch **ppdispChild);
	STDMETHOD(get_accName)(VARIANT varChild,BSTR *pszName);
	STDMETHOD(get_accValue)(VARIANT varChild,BSTR *pszValue);
	STDMETHOD(get_accDescription)(VARIANT varChild,BSTR *pszDescription);
	STDMETHOD(get_accRole)(VARIANT varChild,VARIANT *pvarRole);
	STDMETHOD(get_accState)(VARIANT varChild,VARIANT *pvarState);
	STDMETHOD(get_accHelp)(VARIANT varChild,BSTR *pszHelp);
	STDMETHOD(get_accHelpTopic)(BSTR *pszHelpFile,VARIANT varChild,long *pidTopic);
	STDMETHOD(get_accKeyboardShortcut)(VARIANT varChild,BSTR *pszKeyboardShortcut);
	STDMETHOD(get_accFocus)(VARIANT *pvarChild);
	STDMETHOD(get_accSelection)(VARIANT *pvarChildren);
	STDMETHOD(get_accDefaultAction)(VARIANT varChild,BSTR *pszDefaultAction);
	STDMETHOD(accSelect)(long flagsSelect,VARIANT varChild);
	STDMETHOD(accLocation)(long *pxLeft,long *pyTop,long *pcxWidth,long *pcyHeight,VARIANT varChild);
	STDMETHOD(accNavigate)(long navDir,VARIANT varStart,VARIANT *pvarEndUpAt);
	STDMETHOD(accHitTest)(long xLeft,long yTop,VARIANT *pvarChild);
	STDMETHOD(accDoDefaultAction)(VARIANT varChild);
	STDMETHOD(put_accName)(VARIANT varChild,BSTR szName);
	STDMETHOD(put_accValue)(VARIANT varChild,BSTR szValue);

	// IEnumVARIANT
	STDMETHOD(Next)(ULONG celt,VARIANT *rgVar,ULONG *pCeltFetched);
	STDMETHOD(Skip)(ULONG celt);
	STDMETHOD(Reset)();
	STDMETHOD(Clone)(IEnumVARIANT **ppEnum);
};

CPPcAccServer::CPPcAccServer(PPC_APPINFO *cinfo)
{
	m_cRefCount = 1;
	m_cinfo = cinfo;
	m_enumCount = 0;

	DCreateStdAccessibleObject(cinfo->info.hWnd,OBJID_CLIENT,
			xIID_IAccessible,(void **)&m_AccessibleServer);
}

CPPcAccServer::~CPPcAccServer()
{
	m_AccessibleServer->Release();
}

//==================================================================== IUnknown
STDMETHODIMP CPPcAccServer::QueryInterface(REFIID iid,void **ppv)
{
#if 0
	LPOLESTR lplpsz;
	char a[2000];
	StringFromCLSID(iid,&lplpsz);
	tstrcpy(a,"CLSID\\");
	UnicodeToAnsi(lplpsz,a + 6,2000 - 6);
	CoTaskMemFree(lplpsz);
	if ( !strcmp(a + 6,"{00000000-0000-0000-C000-000000000046}") ){
		strcpy(a,"IUnknown");
	}else if ( !strcmp(a + 6,"{00000003-0000-0000-C000-000000000046}") ){
		strcpy(a,"IMarshal");
	}else if ( !strcmp(a + 6,"{00000018-0000-0000-C000-000000000046}") ){
		strcpy(a,"IStdMarshalInfo");
	}else if ( !strcmp(a + 6,"{00000019-0000-0000-C000-000000000046}") ){
		strcpy(a,"IExternalConnection");
	}else if ( !stricmp(a + 6,"{618736e0-3c3d-11cf-810c-00aa00389b71}") ){
		strcpy(a,"IAccessible");
	}else if ( !strcmp(a + 6,"{00020400-0000-0000-C000-000000000046}") ){
		strcpy(a,"IDispatch");
	}else if ( !strcmp(a + 6,"{00020404-0000-0000-C000-000000000046}") ){
		strcpy(a,"IEnumVARIANT");
	}else if ( !strcmp(a + 6,"{6D5140C1-7436-11CE-8034-00AA006009FA}") ){
		strcpy(a,"IServiceProvider");
	}else if ( !stricmp(a + 6,"{7852b78d-1cfd-41c1-a615-9c0c85960b5f}") ){
		strcpy(a,"IAccIdentity");
	}

	GetRegString(HKEY_CLASSES_ROOT,a,NilStr,a,1000);
	MSGMSG("QueryInterface : %s",a);
#endif
	if ( (iid == IID_IUnknown) || (iid == xIID_IAccessible) ){
		*ppv = static_cast<IAccessible*>(this);
		AddRef();
		return NOERROR;
	}
	if ( iid == IID_IEnumVARIANT ){
		*ppv = static_cast<IEnumVARIANT*>(this);
		AddRef();
		return NOERROR;
	}
	if ( iid == IID_IDispatch ){
		*ppv = static_cast<IDispatch*>(this);
		AddRef();
		return NOERROR;
	}
	// IAccIdentity,IServiceProvider,IExternalConnection,IAgileObject
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CPPcAccServer::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CPPcAccServer::Release(void)
{
	long count = m_cRefCount;

	if ( count > 0 ){
		if( (m_cRefCount = count - 1) == 0 ){
			MSGMSG("CPPcAccServer Delete",0);
			m_cinfo->PPcAccServer = NULL;

			delete this;
			return 0;
		}
		MSGMSG("CPPcAccServer Release %d",count);
	}
	return count;
}
//=================================================================== IDispatch
STDMETHODIMP CPPcAccServer::GetTypeInfoCount(UINT *pctinfo)
{
	MSGMSG("GetTypeInfoCount",0);
	*pctinfo = 0;
	return E_NOTIMPL;
};

STDMETHODIMP CPPcAccServer::GetTypeInfo(UINT,LCID,ITypeInfo **pptinfo)
{
	MSGMSG("GetTypeInfo",0);
	*pptinfo = NULL;
	return E_NOTIMPL;
};

#pragma argsused
STDMETHODIMP CPPcAccServer::GetIDsOfNames(REFIID ,OLECHAR **rgszNames,UINT cNames,LCID,DISPID *rgDispId)
{
	MSGMSGA("GetIDsOfNames %d",cNames);

	*rgszNames = NULL;
	*rgDispId = DISPID_UNKNOWN;
	return E_NOTIMPL;
};

#pragma argsused
STDMETHODIMP CPPcAccServer::Invoke(DISPID dispIdMember,REFIID riid,LCID,WORD,DISPPARAMS *,VARIANT *pVarResult,EXCEPINFO *,UINT *)
{
	MSGMSGA("Invoke %d",dispIdMember);
	if ( pVarResult != NULL ) V_VT(pVarResult) = VT_EMPTY;
	if ( riid != IID_NULL ) return DISP_E_UNKNOWNINTERFACE;
	return E_NOTIMPL;
};

//================================================================ IEnumVARIANT
STDMETHODIMP CPPcAccServer::Next(ULONG celt,VARIANT *rgVar,ULONG *pCeltFetched)
{
	long childCount;
	HRESULT hr = S_OK;
	long fetched = 0;

	MSGMSG("next",0);
	get_accChildCount(&childCount);
	if ( !rgVar || !pCeltFetched || (celt < 1) ) return E_INVALIDARG;
	for ( ULONG x = 0 ; x < celt ; x++ ){
		if ( ++m_enumCount <= (ULONG)childCount ){
			V_VT(&rgVar[x]) = VT_I4;
			V_I4(&rgVar[x]) = m_enumCount;
			fetched++;
		}else{
			V_VT(&rgVar[x]) = VT_EMPTY;
			hr = S_FALSE;
		}
	}
	*pCeltFetched = fetched;
	return hr;
}

STDMETHODIMP CPPcAccServer::Skip(ULONG celt)
{
	long childCount;

	MSGMSG("skip",0);
	get_accChildCount(&childCount);
	if ( (m_enumCount + celt) >= (ULONG)childCount ){
		return S_FALSE;
	}else{
		m_enumCount += celt;
		return S_OK;
	}
}

STDMETHODIMP CPPcAccServer::Reset()
{
	MSGMSG("reset",0);
	m_enumCount = 0;
	return S_OK;
}

STDMETHODIMP CPPcAccServer::Clone(IEnumVARIANT **ppEnum)
{
	MSGMSG("clone",0);
	CPPcAccServer *pAcc = new CPPcAccServer(m_cinfo);
	pAcc->m_enumCount = m_enumCount;
	*ppEnum = static_cast<IEnumVARIANT*>(pAcc);
	return S_OK;
}
//================================================================ IAccessible
#define mCEL(No) ((ENTRYCELL *)m_cinfo->e.CELLDATA.p)[((DWORD *)m_cinfo->e.INDEXDATA.p)[No]]

STDMETHODIMP CPPcAccServer::get_accParent(IDispatch **ppdispParent)
{
	MSGMSG("get_accParent",0);
	return m_AccessibleServer->get_accParent(ppdispParent);
};

STDMETHODIMP CPPcAccServer::get_accChildCount(long *pcountChildren)
{
	*pcountChildren = m_cinfo->e.cellIMax;
	MSGMSG("get_accChildCount %d",*pcountChildren);
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accChild(VARIANT varChild,IDispatch **ppdispChild)
{
	MSGMSG("get_accChild %d",V_I4(&varChild));
	*ppdispChild = NULL;
	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_cinfo->e.cellIMax) ){
		return E_INVALIDARG;
	}
	return S_FALSE;
};

STDMETHODIMP CPPcAccServer::get_accName(VARIANT varChild,BSTR *pszName)
{
	*pszName = NULL;

	if ( V_I4(&varChild) == CHILDID_SELF ){
		MSGMSG("get_accName CHILDID_SELF",0);
		return m_AccessibleServer->get_accName(varChild,pszName);
	}else{
		if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_cinfo->e.cellIMax) ){
			*pszName = NULL;
			return E_INVALIDARG;
		}

		ENTRYINDEX celln = (ENTRYINDEX)V_I4(&varChild) - 1; // 基底が0
		ENTRYCELL *cell = &mCEL(celln);
		#ifdef UNICODE
			*pszName = DSysAllocString(cell->f.cFileName);
		#else
			WCHAR name[MAX_PATH];

			AnsiToUnicode(cell->f.cFileName,name,MAX_PATH);
			*pszName = DSysAllocString(name);
		#endif
		if ( pszName == NULL ) return E_OUTOFMEMORY;
		MSGMSG("get_accName %s",pszName);
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accValue(VARIANT varChild,BSTR *pszValue)
{
	MSGMSG("get_accValue %d",V_I4(&varChild));
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	*pszValue = NULL;
	return DISP_E_MEMBERNOTFOUND;
};

STDMETHODIMP CPPcAccServer::get_accDescription(VARIANT varChild,BSTR *pszDescription)
{
	MSGMSG("get_accDescription",0);
	*pszDescription = NULL;
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	return DISP_E_MEMBERNOTFOUND;
};

STDMETHODIMP CPPcAccServer::get_accRole(VARIANT varChild,VARIANT *pvarRole)
{
	MSGMSG("get_accRole %d",V_I4(&varChild));
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;

	V_VT(pvarRole) = VT_I4;
	if ( V_I4(&varChild) == CHILDID_SELF ){
		V_I4(pvarRole) = ROLE_SYSTEM_LIST;
	}else{
		if ( V_I4(&varChild) > m_cinfo->e.cellIMax ){
			V_VT(pvarRole) = VT_EMPTY;
			return E_INVALIDARG;
		}
		V_I4(pvarRole) = ROLE_SYSTEM_LISTITEM;
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accState(VARIANT varChild,VARIANT *pvarState)
{
	MSGMSG("get_accState %d",V_I4(&varChild));
	V_VT(pvarState) = VT_EMPTY;

	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	if ( V_I4(&varChild) == CHILDID_SELF ){
		return m_AccessibleServer->get_accState(varChild,pvarState);
	}else{
		if ( V_I4(&varChild) > m_cinfo->e.cellIMax ) return E_INVALIDARG;

//		DWORD flags = STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_SELECTABLE;
		DWORD flags = STATE_SYSTEM_SELECTABLE;
		ENTRYINDEX celln = (ENTRYINDEX)V_I4(&varChild) - 1; // 基底が0
		ENTRYCELL *cell = &mCEL(celln);

		if ( celln == m_cinfo->e.cellN ) setflag(flags,STATE_SYSTEM_FOCUSED);
		if ( IsCellPtrMarked(cell) )     setflag(flags,STATE_SYSTEM_SELECTED);

		V_VT(pvarState) = VT_I4;
		V_I4(pvarState) = flags;
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accHelp(VARIANT,BSTR *)
{
	MSGMSG("get_accHelp",0);
	return S_FALSE;
};

STDMETHODIMP CPPcAccServer::get_accHelpTopic(BSTR *pszHelpFile,VARIANT,long *)
{
	MSGMSG("get_accHelpTopic",0);
	*pszHelpFile = NULL;
	return S_FALSE;
};

STDMETHODIMP CPPcAccServer::get_accKeyboardShortcut(VARIANT,BSTR *pszKeyboardShortcut)
{
	MSGMSG("get_accKeyboardShortcut",0);
	*pszKeyboardShortcut = NULL;
	return S_FALSE;
};

STDMETHODIMP CPPcAccServer::get_accFocus(VARIANT *pvarChild)
{
	MSGMSG("get_accFocus",0);

	m_AccessibleServer->get_accFocus(pvarChild);
	if ( V_VT(pvarChild) != VT_I4 ){
		return E_INVALIDARG;
	}else{
		V_I4(pvarChild) = m_cinfo->e.cellN + 1; // 基底が1
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accSelection(VARIANT *pvarChildren)
{
	MSGMSG("get_accSelection",0);
	V_VT(pvarChildren) = VT_I4;
	V_I4(pvarChildren) = m_cinfo->e.cellN + 1; // 基底が1
	return S_OK;
};

STDMETHODIMP CPPcAccServer::get_accDefaultAction(VARIANT varChild,BSTR *pszDefaultAction)
{
	MSGMSG("get_accDefaultAction",0);

	if ( V_I4(&varChild) == CHILDID_SELF ){
		*pszDefaultAction = NULL;
		return DISP_E_MEMBERNOTFOUND;
	}

	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_cinfo->e.cellIMax) ){
		*pszDefaultAction = NULL;
		return E_INVALIDARG;
	}
	*pszDefaultAction = DSysAllocString(L"Enter");
	return S_OK;
};

STDMETHODIMP CPPcAccServer::accSelect(long flagsSelect,VARIANT varChild)
{
	MSGMSG("accSelect",0);

	if ( flagsSelect & ~(SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION) ){
		return E_INVALIDARG;
	}

	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_cinfo->e.cellIMax) ){
		return E_INVALIDARG;
	}

	SetFocus(m_cinfo->info.hWnd);

	if ( ((flagsSelect & (SELFLAG_TAKESELECTION | SELFLAG_TAKEFOCUS)) != 0) &&
		(V_I4(&varChild) != CHILDID_SELF) ){
		MoveCellCsr(m_cinfo,((ENTRYINDEX)V_I4(&varChild) - 1) - m_cinfo->e.cellN,NULL);
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::accLocation(long *pxLeft,long *pyTop,long *pcxWidth,long *pcyHeight,VARIANT varChild)
{
	MSGMSG("accLocation",0);
	POINT pos;
	*pxLeft = 0;
	*pyTop = 0;
	*pcxWidth = 0;
	*pcyHeight = 0;

	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	if ( V_I4(&varChild) == CHILDID_SELF ){
		return m_AccessibleServer->accLocation(pxLeft,pyTop,pcxWidth,pcyHeight,varChild);
	}else{
		if ( V_I4(&varChild) > m_cinfo->e.cellIMax ) return E_INVALIDARG;

		ENTRYINDEX cell = V_I4(&varChild) - 1 - m_cinfo->cellWMin;
		pos.x = CalcCellX(m_cinfo,cell);
		pos.y = CalcCellY(m_cinfo,cell);
		ClientToScreen(m_cinfo->info.hWnd,&pos);

		*pxLeft = pos.x;
		*pyTop  = pos.y;
		*pcxWidth  = m_cinfo->cel.Size.cx - m_cinfo->fontX;
		*pcyHeight = m_cinfo->cel.Size.cy;
		return S_OK;
	}
};

STDMETHODIMP CPPcAccServer::accNavigate(long navDir,VARIANT varStart,VARIANT *pvarEndUpAt)
{
	MSGMSG("accNavigate",0);
	V_VT(pvarEndUpAt) = VT_EMPTY;

	if ( (V_VT(&varStart) != VT_I4) || (V_I4(&varStart) > m_cinfo->e.cellIMax) ){
		return E_INVALIDARG;
	}

	switch (navDir){
		case NAVDIR_FIRSTCHILD:
			if ( (V_I4(&varStart) == CHILDID_SELF) && (m_cinfo->e.cellIMax > 0) ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = 1;
				break;
			}
			return S_FALSE;

		case NAVDIR_LASTCHILD:
			if ( (V_I4(&varStart) == CHILDID_SELF) && (m_cinfo->e.cellIMax > 0) ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = m_cinfo->e.cellIMax;
				break;
			}
			return S_FALSE;

		case NAVDIR_NEXT:
		case NAVDIR_DOWN:
			if ( V_I4(&varStart) != CHILDID_SELF ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = V_I4(&varStart) + 1; // 基底が1

				if ( V_I4(pvarEndUpAt) > m_cinfo->e.cellIMax ){
					V_VT(pvarEndUpAt) = VT_EMPTY;
					return S_FALSE;
				}
			}else{
				return m_AccessibleServer->accNavigate(navDir,varStart,pvarEndUpAt);
			}
			break;

		case NAVDIR_PREVIOUS:
		case NAVDIR_UP:
			if ( V_I4(&varStart) != CHILDID_SELF ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = V_I4(&varStart) - 1;

				if ( V_I4(pvarEndUpAt) < 1 ){
					V_VT(pvarEndUpAt) = VT_EMPTY;
					return S_FALSE;
				}
			}else{
				return m_AccessibleServer->accNavigate(navDir,varStart,pvarEndUpAt);
			}
			break;

		case NAVDIR_LEFT:
		case NAVDIR_RIGHT:
			if ( V_I4(&varStart) == CHILDID_SELF ){
				return m_AccessibleServer->accNavigate(navDir,varStart,pvarEndUpAt);
			}else{
				V_VT(pvarEndUpAt) = VT_EMPTY;
				return S_FALSE;
			}
		default:
			V_VT(pvarEndUpAt) = VT_EMPTY;
			return S_FALSE;
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::accHitTest(long xLeft,long yTop,VARIANT *pvarChild)
{
	MSGMSG("accHitTest",0);
	int celln = -1;
	POINT pos = {xLeft,yTop};

	V_VT(pvarChild) = VT_I4;
	ScreenToClient(m_cinfo->info.hWnd,&pos);
	GetItemTypeFromPoint(m_cinfo,&pos,&celln);
	if ( celln >= 0 ){
		V_I4(pvarChild) = celln + 1; // 基底が1
	}else{
		V_I4(pvarChild) = CHILDID_SELF;
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::accDoDefaultAction(VARIANT varChild)
{
	MSGMSG("accDoDefaultAction",0);
	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_cinfo->e.cellIMax) ){
		return E_INVALIDARG;
	}
	if ( V_I4(&varChild) != CHILDID_SELF ){
		if ( SUCCEEDED(accSelect(SELFLAG_TAKESELECTION, varChild)) ){
			PostMessage(m_cinfo->info.hWnd,WM_PPXCOMMAND,K_raw | K_cr, 0);
		}
	}
	return S_OK;
};

STDMETHODIMP CPPcAccServer::put_accName(VARIANT,BSTR)
{
	MSGMSG("put_accName",0);
	return E_NOTIMPL;
};

STDMETHODIMP CPPcAccServer::put_accValue(VARIANT,BSTR)
{
	MSGMSG("put_accValue",0);
	return E_NOTIMPL;
};

HMODULE hOleautDLL = NULL;
HMODULE hOleaccDLL = NULL;

LRESULT WmGetObject(PPC_APPINFO *cinfo,WPARAM wParam)
{
	if ( hOleaccDLL == NULL ){
		hOleaccDLL = LoadWinAPI("OLEACC.DLL",NULL,OLEACCDLL,LOADWINAPI_LOAD);
		if ( hOleaccDLL == NULL ) return 0;
		if ( hOleautDLL == NULL ){
			hOleautDLL = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUTDLL,LOADWINAPI_LOAD);
			if ( hOleautDLL == NULL ) return 0;
		}
	}
	if ( cinfo->PPcAccServer != NULL ){
		// static_cast<IAccessible*>(cinfo->PPcAccServer)->AddRef(); 不要
		MSGMSG("WmGetObject stock %d",cinfo->PPcAccServer);
		return DLresultFromObject(xIID_IAccessible,wParam,
				static_cast<IAccessible*>(cinfo->PPcAccServer));
	}else{
		CPPcAccServer *pAccServer = new CPPcAccServer(cinfo);
		if ( pAccServer != NULL ){
			cinfo->PPcAccServer = static_cast<void*>(pAccServer);
			if ( DNotifyWinEvent == DummyNotifyWinEvent ){
				GETDLLPROC(GetModuleHandle(StrUser32DLL),NotifyWinEvent);
				if ( DNotifyWinEvent == NULL ){
					DNotifyWinEvent = DummyNotifyWinEvent;
				}
			}
			MSGMSG("WmGetObject new %d",pAccServer);
			return DLresultFromObject(xIID_IAccessible,wParam,
					static_cast<IAccessible*>(pAccServer));
		}
	}
	return 0;
}

void FreeAccServer(PPC_APPINFO *cinfo)
{
	if ( cinfo->PPcAccServer != NULL ){
		static_cast<IAccessible*>(cinfo->PPcAccServer)->Release();
		cinfo->PPcAccServer = NULL;
	}
}

#ifndef RELEASE
#ifndef WINEGCC
void Debug_DispIID(const TCHAR *mes,REFIID riid)
{
	WCHAR *iidstring,pathW[MAX_PATH];
	TCHAR path[MAX_PATH],name[MAX_PATH];

	StringFromIID(riid,&iidstring);
	wsprintfW(pathW,L"Interface\\%s",iidstring);
	CoTaskMemFree(iidstring);
#ifdef UNICODE
	wcscpy(path, pathW);
#else
	UnicodeToAnsi(pathW,path,MAX_PATH);
#endif
	if ( GetRegString(HKEY_CLASSES_ROOT,path,NilStr,name,sizeof(name)) ){
		XMessage(NULL,NULL,XM_DbgLOG,T("%s %s"),mes,name);
	}else{
		XMessage(NULL,NULL,XM_DbgLOG,T("%s %s"),mes,path);
	}
}
#else
void Debug_DispIID(const TCHAR *mes,REFIID riid)
{
}
#endif
#endif
