/*-----------------------------------------------------------------------------
	Paper Plane xUI					sub on cpp
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H ‚Ì DLL ’è‹`Žw’è
#include "WINAPI.H"
#include <shlobj.h>
#include <ole2.h>
#include "WINOLE.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#pragma hdrstop

// D&D ŠÖŒW ===================================================================
const CLSID XCLSID_IDATAOBJECT_AUTODD = CLSID_IDATAOBJECT_AUTODD;
IDropTarget *DropTarget;
DWORD regobjectID = 0;
const TCHAR DROPTARGETPROPNAME[] = T("OleDropTargetInterface");
HMODULE hOleaut32DLLHOOK;

class CddpIDropSource: public IDropSource{
	private:
		long m_cRefCount;
	public:
		CddpIDropSource();
		~CddpIDropSource();
										// IUnknown members -------------------
		STDMETHOD(QueryInterface)(REFIID,void **);
		STDMETHOD_(ULONG,AddRef)(void);
		STDMETHOD_(ULONG,Release)(void);
										// IDataObject members ----------------
		STDMETHOD(GiveFeedback)(DWORD dwEffect);
		STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed,DWORD grfKeyState);
};

//------------------------------------- ƒRƒ“ƒXƒgƒ‰ƒNƒ^
CddpIDropSource::CddpIDropSource()
{
	m_cRefCount = 1;
}
//------------------------------------- ƒfƒRƒ“ƒXƒgƒ‰ƒNƒ^
CddpIDropSource::~CddpIDropSource()
{
//	if(m_cRefCount != 0) xmessage(XM_FaERRld,T("~CddpIDropSource"));
}
//------------------------------------- ’èŒ^•”•ª ------------------------------
STDMETHODIMP CddpIDropSource::QueryInterface(REFIID refiid,void ** ppv)
{
	if ( XIID_IUnknown == refiid || XIID_IDropSource == refiid ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CddpIDropSource::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CddpIDropSource::Release(void)
{
	long tempCount;

	tempCount = --m_cRefCount;
	if (tempCount == 0) delete this;
	return tempCount;
}

//------------------------------------- Žå—v•”•ª ------------------------------
STDMETHODIMP CddpIDropSource::GiveFeedback(DWORD)
{
	return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}

STDMETHODIMP CddpIDropSource::QueryContinueDrag(BOOL fEscapePressed,DWORD grfKeyState)
{
	if ( fEscapePressed ||
		 ( (grfKeyState & (MK_LBUTTON | MK_RBUTTON)) ==
		 							(MK_LBUTTON | MK_RBUTTON) ) ){
		return ResultFromScode(DRAGDROP_S_CANCEL);
	}
	if (!(grfKeyState & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))){
		return ResultFromScode(DRAGDROP_S_DROP);
	}
	return ResultFromScode(S_OK);
}
void DragDropPath(HWND hWnd,const TCHAR *path)
{
	LPDATAOBJECT pDO;
	CddpIDropSource *DropSource;
	DWORD dwEffect;

	if( FAILED(::OleInitialize(NULL)) ) return;
	pDO = (LPDATAOBJECT)GetPathInterface(hWnd,path,XIID_IDataObject,NULL);
	if ( pDO != NULL ){
		DropSource = new CddpIDropSource;

		::DoDragDrop(pDO,DropSource,
				DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK |
				DROPEFFECT_SCROLL,&dwEffect);

		DropSource->Release();
		pDO->Release();
	}
	::OleUninitialize();
}

void AutoDDDLL_Register(HWND hTargetWnd)
{
	if ( regobjectID ) return; // Šù‚É“o˜^Ï‚Ý

	hOleaut32DLLHOOK = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUT32_SysStr,LOADWINAPI_LOAD);
	if ( hOleaut32DLLHOOK == NULL ) return;
	// DropTarget ‚ðŽæ“¾‚·‚é
	DropTarget = (IDropTarget *)GetProp(hTargetWnd,DROPTARGETPROPNAME);
	if ( DropTarget == NULL ) return;

	if( FAILED(::OleInitialize(NULL)) ) return;
	DropTarget->AddRef();
	::CoLockObjectExternal(DropTarget,TRUE,TRUE);
	// ROT ‚É DropTarget ‚ð“o˜^
	::DRegisterActiveObject(DropTarget,XCLSID_IDATAOBJECT_AUTODD,ACTIVEOBJECT_WEAK,&regobjectID);
}

void AutoDDDLL_Revoke(void)
{
	if ( !regobjectID ) return; // “o˜^‚µ‚Ä‚¢‚È‚¢
	DRevokeActiveObject(regobjectID,NULL);
	regobjectID = 0;
	::CoLockObjectExternal(DropTarget,FALSE,TRUE);
	DropTarget->Release();
	::OleUninitialize();
	FreeLibrary(hOleaut32DLLHOOK);
}

LRESULT CALLBACK AutoDDDLLProc(int nCode,WPARAM,CWPSTRUCT *cwps)
{
	if ( (nCode == HC_ACTION) && (cwps->message == WM_PPXCOMMAND) ){
		if ( cwps->wParam == KC_REGISTADDPROC ){
			AutoDDDLL_Register(cwps->hwnd);
		}else if ( cwps->wParam == KC_REVOKEADDPROC ){
			AutoDDDLL_Revoke();
		}
	}
	return 0;
}

// ƒJƒ‰ƒ€Šg’£ŠÖŒW =============================================================
const TCHAR HandlersKey[] = T("Folder\\shellex\\ColumnHandlers");
const CLSID XCLSID_PropertySystem = {0xb8967f85,0x58ae,0x4f46,{0x9f,0xb2,0x5d,0x79,0x04,0x79,0x8f,0x4b}};
const IID XIID_IPropertySystem = {0xca724e8a,0xc3e6,0x442b,{0x88,0xa4,0x6f,0xb0,0xdb,0x80,0x35,0xa3}};
const IID XIID_IPropertyDescriptionList = {0x1f9fc1d0,0xc39b,0x4b26,{0x81,0x7f,0x01,0x19,0x67,0xd3,0x44,0x0e}};
const IID XIID_IPropertyDescription = {0x6f79d558,0x3e96,0x4549,{0xa1,0xd1,0x7d,0x75,0xd2,0x28,0x88,0x14}};

const WCHAR SystemName[] = L"System";

void CloseColumnExtMenu(ThSTRUCT *thEcdata)
{
	IColumnProvider **Icps;
	Icps = (IColumnProvider **)(BYTE *)(thEcdata->bottom + sizeof(int));
	while ( *Icps != NULL ){
		(*Icps)->Release();
		Icps++;
	}
	ThFree(thEcdata);
}

PPXDLL void PPXAPI GetColumnExtMenu(ThSTRUCT *thEcdata,const TCHAR *targetpath,HMENU hMenu,DWORD index)
{
	HKEY hKey;
	int enumno = 0;
	DWORD itemoffset;
	DWORD oldcndi = 0;
	int writeoffset = 0;
	IColumnProvider *Icp,**Icps;
	COLUMNEXTDATAINFO *cndi,newcndi;

	if ( targetpath == NULL ){
		CloseColumnExtMenu(thEcdata);
		return;
	}
								// €–Ú‚Ö‚ÌƒIƒtƒZƒbƒg(Œ»Žž“_‚Å‚Íƒ_ƒ~[)‚ð—pˆÓ
	::ThAppend(thEcdata,&writeoffset,sizeof(writeoffset));
	// Windows 2000,XP ========================================================
								// IColumnProvider ‚Ìˆê——‚ðì¬
	if ( ::RegOpenKeyEx(HKEY_CLASSES_ROOT,HandlersKey,0,KEY_READ,&hKey) == ERROR_SUCCESS ){
		for ( ; ; ){
			TCHAR keyname[MAX_PATH];
			CLSID hid;
			DWORD s;
			FILETIME ft;
			HRESULT hres;
			SHCOLUMNINIT cci;

			s = MAX_PATH;
			if ( ::RegEnumKeyEx(hKey,enumno++,keyname,&s,NULL,NULL,NULL,&ft) !=
					ERROR_SUCCESS){
				break;
			}

			// CLSID ‚ðì¬
			#ifndef UNICODE
				WCHAR idnameW[MAX_PATH];
				#define tidname idnameW

				AnsiToUnicode(keyname,idnameW,MAX_PATH);
				AnsiToUnicode(targetpath,cci.wszFolder,MAX_PATH);
			#else
				#define tidname keyname
				tstrcpy(cci.wszFolder,targetpath);
			#endif
			::CLSIDFromString(tidname,&hid);
			hres = ::CoCreateInstance(hid,NULL,CLSCTX_INPROC_SERVER,
					XIID_IColumnProvider,(LPVOID *)&Icp);
			if ( FAILED(hres) ) continue;

			cci.dwFlags = 0;
			cci.dwReserved = 0;
			if ( Icp->Initialize(&cci) == S_OK ){
				::ThAppend(thEcdata,&Icp,sizeof(IColumnProvider *));
			}else{
				Icp->Release();
			}
		}
		::RegCloseKey(hKey);
	}
	Icp = NULL;
	::ThAppend(thEcdata,&Icp,sizeof(IColumnProvider *)); // terminater

// IColumnProvider ‚Ìˆê——‚©‚ç€–Ú(SHCOLUMNID–ˆ‚ÌCOLUMNEXTDATAINFO‚Ìˆê——)‚ðì¬
	Icps = (IColumnProvider **)(BYTE *)(thEcdata->bottom + sizeof(int));
	itemoffset = *(DWORD *)thEcdata->bottom = thEcdata->top;
	while ( *Icps != NULL ){
		DWORD subindex;

		newcndi.icps.next = 0;
		newcndi.icps.Icp = *Icps;
		newcndi.next_pkey = 0;

		subindex = 0;
		for ( ; ; ){ // ‚Ð‚Æ‚Â‚ÌIColumnProvider‚©‚çŠeitem‚ð—ñ‹“
			SHCOLUMNINFO sci;
			DWORD offset;
			#ifndef UNICODE
				char titleA[MAX_PATH];
				#define title titleA
			#else
				#define title sci.wszTitle
			#endif

			if ( newcndi.icps.Icp->GetColumnInfo(subindex++,&sci) != S_OK ){
				break;
			}
			if ( sci.wszTitle[0] == '\0' ) continue;
			offset = itemoffset;
			while ( offset < thEcdata->top ){ // “¯SHCOLUMNID‚Ì€–Ú‚ª‚È‚¢‚©’T‚·
				cndi = (COLUMNEXTDATAINFO *)(BYTE *)(thEcdata->bottom + offset);
				if ( !memcmp(&sci.scid,&cndi->id.scid,sizeof(sci.scid)) ){ // ŠY“–—L‚è¨’Ç‰Á“o˜^
					COLUMNEXTDATAINFO_ICP *icps;

					icps = &cndi->icps;
					while( icps->next ){
						icps = (COLUMNEXTDATAINFO_ICP *)(BYTE *)(thEcdata->bottom + icps->next);
					}
					icps->next = thEcdata->top;
					::ThAppend(thEcdata,&newcndi.icps,sizeof(newcndi.icps));
					offset = 0;
					break;
				}
				offset = cndi->next;
			}
			if ( offset == 0 ) continue; // ’Ç‰Á“o˜^Ï‚Ý

			// V‹K“o˜^
			if ( oldcndi != 0 ) cndi->next = thEcdata->top;
			oldcndi = thEcdata->top;
			newcndi.next = 0x7fffffff;
			newcndi.id.scid = sci.scid;
			::ThAppend(thEcdata,&newcndi,sizeof(newcndi));

			// €–Ú–¼‚ðƒƒjƒ…[‚É“o˜^
			#ifndef UNICODE
				UnicodeToAnsi(sci.wszTitle,titleA,MAX_COLUMN_NAME_LEN);
			#endif
			if ( hMenu != NULL ){
				if ( index ){
					::AppendMenuString(hMenu,index++,title);
				/* 1.46+3 ”pŽ~
				}else{
					TCHAR buf[MAX_PATH];

					wsprintf(buf,T("\"%s\""),title);
					::SendMessage((HWND)hMenu,CB_ADDSTRING,0,(LPARAM)buf);
				*/
				}
			}else{
				::ThAppend(thEcdata,title,TSTRSIZE32(title));
			}
		}
		Icps++;
	}

	// Windows Vista,7 ========================================================
	IPropertySystem *ps;
	IPropertyDescriptionList *pdl;
	IPropertyDescription *pd;
	if ( SUCCEEDED(::CoCreateInstance(XCLSID_PropertySystem,NULL,
			CLSCTX_INPROC_SERVER,XIID_IPropertySystem,(void **)&ps)) ){
		UINT count = 0,i;
		int group = 1;
		HMENU hSubMenu,hSystemMenu;

		if ( (hMenu != NULL) && index ){
			hSystemMenu = CreatePopupMenu();
			::AppendMenu(hMenu,MF_EPOP,(UINT_PTR)hSystemMenu,MessageText(MES_MGNR));
		}

		ps->EnumeratePropertyDescriptions(PDEF_COLUMN, //PDEF_ALL,
				XIID_IPropertyDescriptionList,(void**)&pdl);
		pdl->GetCount(&count);
		for ( i = 0 ; i < count; i++ ){
			if ( FAILED(pdl->GetAt(i,XIID_IPropertyDescription,(void**)&pd)) ){
				continue;
			}
			LPWSTR dname,cname;
			if ( SUCCEEDED(pd->GetDisplayName(&dname)) ){
				#undef title
				#ifndef UNICODE
					char titleA[MAX_PATH];
					#define title titleA
					UnicodeToAnsi(dname,titleA,MAX_PATH);
				#else
					#define title dname
				#endif

				if ( oldcndi != 0 ){
					cndi = (COLUMNEXTDATAINFO *)(BYTE *)(thEcdata->bottom + oldcndi);
					cndi->next = thEcdata->top;
				}
				oldcndi = thEcdata->top;

				newcndi.next = 0x7fffffff;
				pd->GetPropertyKey(&newcndi.id.pkey);
				newcndi.icps.next = 0;
				newcndi.icps.Icp = NULL;
				::ThAppend(thEcdata,&newcndi,sizeof(newcndi));

				if ( hMenu != NULL ){
					pd->GetCanonicalName(&cname);
					WCHAR *grouplast = strrchrW(cname,'.');
					if ( grouplast == NULL ){
						hSubMenu = hMenu;
					}else{
						int len = grouplast - cname;

						if ( len != group ){
							*grouplast = '\0';
							grouplast = cname;
							if ( memcmp(cname,SystemName,TSIZEOFW(SystemName) - 1 ) == 0 ){
								grouplast = cname + TSIZEOFW(SystemName) - 1;
							}
							if ( index ){
								if ( strcmpW(cname,SystemName) == 0 ){
									hSubMenu = hSystemMenu;
								}else{
									#ifdef UNICODE
										hSubMenu = CreatePopupMenu();
										::AppendMenu(hMenu,MF_EPOP,(UINT_PTR)hSubMenu,grouplast);
									#else
										TCHAR buf[MAX_PATH];

										hSubMenu = CreatePopupMenu();
										UnicodeToAnsi(grouplast,buf,200);
										::AppendMenu(hMenu,MF_EPOP,(UINT_PTR)hSubMenu,buf);
									#endif
								}
							/* 1.46+3 ”pŽ~
							}else{
								#ifdef UNICODE
									wsprintf(buf,L"----- %s",grouplast);
								#else
									tstrcpy(buf,T("----- "));
									UnicodeToAnsi(grouplast,buf + 6,200);
								#endif
								::SendMessage((HWND)hMenu,CB_ADDSTRING,0,(LPARAM)buf);
							*/
							}
							group = len;
						}
					}

					if ( index ){
						::AppendMenuString(hSubMenu,index++,title);
					/* 1.46+3 ”pŽ~
					}else{
						wsprintf(buf,T("\"%s\""),title);
						::SendMessage((HWND)hMenu,CB_ADDSTRING,0,(LPARAM)buf);
					*/
					}
					CoTaskMemFree(cname);
				}else{
					::ThAppend(thEcdata,title,TSTRSIZE32(title));
				}
				CoTaskMemFree(dname);
			}
			pd->Release();
		}
		pdl->Release();
		ps->Release();
	}
}

HRESULT GetLink(HWND hWnd,const TCHAR *LinkFile,TCHAR *Original)
{
	HRESULT hres;
	IShellLink *psl;
	WIN32_FIND_DATA f;
										// IShellLink interface ‚ðŽæ“¾
	hres = ::CoCreateInstance(XCLSID_ShellLink,NULL,
			CLSCTX_INPROC_SERVER,XIID_IShellLink,(LPVOID *)&psl);
	if ( SUCCEEDED(hres) ){
		IPersistFile *ppf;
									// IShellLink ‚©‚ç IPersistFile interface
		hres = psl->QueryInterface(XIID_IPersistFile,(LPVOID *)&ppf);
		if ( SUCCEEDED(hres) ){
			#ifdef UNICODE
				hres = ppf->Load(LinkFile,STGM_READ);	// link ‚ðŽæ“¾
			#else
				WCHAR wsz[VFPS];

				::MultiByteToWideChar(CP_ACP,0,LinkFile,-1,wsz,TSIZEOF(wsz));
				hres = ppf->Load(wsz,STGM_READ);	// link ‚ðŽæ“¾
			#endif
			if ( SUCCEEDED(hres) ){
				hres = psl->Resolve(hWnd,SLR_ANY_MATCH);
				if ( SUCCEEDED(hres) ){
					hres = psl->GetPath(Original,MAX_PATH,&f,SLGP_UNCPRIORITY);
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
