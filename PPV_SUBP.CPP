/*-----------------------------------------------------------------------------
	Paper Plane vUI								sub on cpp
-----------------------------------------------------------------------------*/
#define _PPV_SUBP_

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
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop


#if !defined(RELEASE) && 0
	#define MSGMSG(t,s) XMessage(NULL,NULL,XM_DbgLOG,T(t),s);
	#define MSGMSGA(t,s) MSGMSG(t,s)
#else
	#define MSGMSG(t,s)
	#define MSGMSGA(t,s) UnUsedParam(s);
#endif

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

class CPPvAccServer : public IAccessible , public IEnumVARIANT
{
private:
	long m_cRefCount;
	ULONG m_enumCount;
	IAccessible *m_AccessibleServer;

	int m_GetCursor(void);
	int m_GetMax(void);

public:
	CPPvAccServer();
	~CPPvAccServer();

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

CPPvAccServer::CPPvAccServer()
{
	m_cRefCount = 1;
	m_enumCount = 0;

	DCreateStdAccessibleObject(vinfo.info.hWnd,OBJID_CLIENT,
			xIID_IAccessible,(void **)&m_AccessibleServer);
}

CPPvAccServer::~CPPvAccServer()
{
	m_AccessibleServer->Release();
}

//==================================================================== IUnknown
STDMETHODIMP CPPvAccServer::QueryInterface(REFIID iid,void **ppv)
{
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
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CPPvAccServer::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CPPvAccServer::Release(void)
{
	if( --m_cRefCount == 0 ){
		delete this;
		return 0;
	}
	return m_cRefCount;
}
#define ModeCheckMacro if ( !(vo_.DModeBit & VO_dmode_TEXTLIKE) ) return RPC_E_DISCONNECTED;

//=================================================================== IDispatch
STDMETHODIMP CPPvAccServer::GetTypeInfoCount(UINT *pctinfo)
{
	MSGMSG("GetTypeInfoCount",0);
	*pctinfo = 0;
	return E_NOTIMPL;
};

STDMETHODIMP CPPvAccServer::GetTypeInfo(UINT,LCID,ITypeInfo **pptinfo)
{
	MSGMSG("GetTypeInfo",0);
	*pptinfo = NULL;
	return E_NOTIMPL;
};

#pragma argsused
STDMETHODIMP CPPvAccServer::GetIDsOfNames(REFIID ,OLECHAR **rgszNames,UINT cNames,LCID,DISPID *rgDispId)
{
	MSGMSGA("GetIDsOfNames %d",cNames);

	*rgszNames = NULL;
	*rgDispId = DISPID_UNKNOWN;
	return E_NOTIMPL;
};

#pragma argsused
STDMETHODIMP CPPvAccServer::Invoke(DISPID dispIdMember,REFIID riid,LCID,WORD,DISPPARAMS *,VARIANT *pVarResult,EXCEPINFO *,UINT *)
{
	MSGMSGA("Invoke %d",dispIdMember);
	if ( pVarResult != NULL ) V_VT(pVarResult) = VT_EMPTY;
	if ( riid != IID_NULL ) return DISP_E_UNKNOWNINTERFACE;
	return E_NOTIMPL;
};

//================================================================ IEnumVARIANT
STDMETHODIMP CPPvAccServer::Next(ULONG celt,VARIANT *rgVar,ULONG *pCeltFetched)
{
	HRESULT hr = S_OK;
	long fetched = 0;

	ModeCheckMacro;
	MSGMSG("next",0);
	if ( !rgVar || !pCeltFetched || (celt < 1) ) return E_INVALIDARG;
	for ( ULONG x = 0 ; x < celt ; x++ ){
		if ( ++m_enumCount <= (ULONG)m_GetMax() ){
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

STDMETHODIMP CPPvAccServer::Skip(ULONG celt)
{
	MSGMSG("skip",0);
	if ( (m_enumCount + celt) >= (ULONG)m_GetMax() ){
		return S_FALSE;
	}else{
		m_enumCount += celt;
		return S_OK;
	}
}

STDMETHODIMP CPPvAccServer::Reset()
{
	MSGMSG("reset",0);
	m_enumCount = 0;
	return S_OK;
}

STDMETHODIMP CPPvAccServer::Clone(IEnumVARIANT **ppEnum)
{
	MSGMSG("clone",0);
	CPPvAccServer *pAcc = new CPPvAccServer();
	pAcc->m_enumCount = m_enumCount;
	*ppEnum = static_cast<IEnumVARIANT*>(pAcc);
	return S_OK;
}
//================================================================ IAccessible
STDMETHODIMP CPPvAccServer::get_accParent(IDispatch **ppdispParent)
{
	MSGMSG("get_accParent",0);
	return m_AccessibleServer->get_accParent(ppdispParent);
};

STDMETHODIMP CPPvAccServer::get_accChildCount(long *pcountChildren)
{
	ModeCheckMacro;
	*pcountChildren = m_GetMax();
	MSGMSG("get_accChildCount %d",*pcountChildren);
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accChild(VARIANT varChild,IDispatch **ppdispChild)
{
	ModeCheckMacro;
	MSGMSG("get_accChild %d",V_I4(&varChild));
	*ppdispChild = NULL;
	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_GetMax()) ){
		return E_INVALIDARG;
	}
	return S_FALSE;
};

STDMETHODIMP CPPvAccServer::get_accName(VARIANT varChild,BSTR *pszName)
{
	*pszName = NULL;

	if ( V_I4(&varChild) == CHILDID_SELF ){
		MSGMSG("get_accName CHILDID_SELF",0);
		return m_AccessibleServer->get_accName(varChild,pszName);
	}else{
		ModeCheckMacro;
		if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_GetMax()) ){
			*pszName = NULL;
			return E_INVALIDARG;
		}
		int line = LogicalLineToDisplayLine(V_I4(&varChild)) - 1; // Šî’ê‚ª0
		TMS_struct text = {{NULL,0,NULL},0};
		if ( ClipMem(&text,line,line + 1) == FALSE ) text.tm.p = (void *)NilStr;

		#ifdef UNICODE
			*pszName = DSysAllocString((WCHAR *)text.tm.p);
		#else
			WCHAR name[0x2000];

			AnsiToUnicode((char *)text.tm.p,name,0x2000);
			*pszName = DSysAllocString(name);
		#endif

		TM_kill(&text.tm);

		if ( pszName == NULL ) return E_OUTOFMEMORY;
		MSGMSG("get_accName %s",pszName);
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accValue(VARIANT varChild,BSTR *pszValue)
{
	MSGMSG("get_accValue %d",V_I4(&varChild));
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	*pszValue = NULL;
	return DISP_E_MEMBERNOTFOUND;
};

STDMETHODIMP CPPvAccServer::get_accDescription(VARIANT varChild,BSTR *pszDescription)
{
	MSGMSG("get_accDescription",0);
	*pszDescription = NULL;
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	return DISP_E_MEMBERNOTFOUND;
};

STDMETHODIMP CPPvAccServer::get_accRole(VARIANT varChild,VARIANT *pvarRole)
{
	MSGMSG("get_accRole %d",V_I4(&varChild));
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;

	V_VT(pvarRole) = VT_I4;
	if ( V_I4(&varChild) == CHILDID_SELF ){
		V_I4(pvarRole) = ROLE_SYSTEM_LIST;
	}else{
		ModeCheckMacro;
		if ( V_I4(&varChild) > m_GetMax() ){
			V_VT(pvarRole) = VT_EMPTY;
			return E_INVALIDARG;
		}
		V_I4(pvarRole) = ROLE_SYSTEM_LISTITEM;
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accState(VARIANT varChild,VARIANT *pvarState)
{
	MSGMSG("get_accState %d",V_I4(&varChild));
	V_VT(pvarState) = VT_EMPTY;

	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	if ( V_I4(&varChild) == CHILDID_SELF ){
		return m_AccessibleServer->get_accState(varChild,pvarState);
	}else{
		ModeCheckMacro;
		if ( V_I4(&varChild) > m_GetMax() ) return E_INVALIDARG;

		DWORD flags = STATE_SYSTEM_SELECTABLE;
		V_VT(pvarState) = VT_I4;
		V_I4(pvarState) = flags;
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accHelp(VARIANT,BSTR *)
{
	MSGMSG("get_accHelp",0);
	return S_FALSE;
};

STDMETHODIMP CPPvAccServer::get_accHelpTopic(BSTR *pszHelpFile,VARIANT,long *)
{
	MSGMSG("get_accHelpTopic",0);
	*pszHelpFile = NULL;
	return S_FALSE;
};

STDMETHODIMP CPPvAccServer::get_accKeyboardShortcut(VARIANT,BSTR *pszKeyboardShortcut)
{
	MSGMSG("get_accKeyboardShortcut",0);
	*pszKeyboardShortcut = NULL;
	return S_FALSE;
};

STDMETHODIMP CPPvAccServer::get_accFocus(VARIANT *pvarChild)
{
	MSGMSG("get_accFocus",0);

	m_AccessibleServer->get_accFocus(pvarChild);
	if ( V_VT(pvarChild) != VT_I4 ){
		return E_INVALIDARG;
	}else{
		ModeCheckMacro;
		V_I4(pvarChild) = m_GetCursor() + 1; // Šî’ê‚ª1
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accSelection(VARIANT *pvarChildren)
{
	ModeCheckMacro;
	MSGMSG("get_accSelection",0);
	V_VT(pvarChildren) = VT_I4;
	V_I4(pvarChildren) = m_GetMax(); // Šî’ê‚ª1
	return S_OK;
};

STDMETHODIMP CPPvAccServer::get_accDefaultAction(VARIANT varChild,BSTR *pszDefaultAction)
{
	MSGMSG("get_accDefaultAction",0);

	if ( V_I4(&varChild) == CHILDID_SELF ){
		*pszDefaultAction = NULL;
		return DISP_E_MEMBERNOTFOUND;
	}
	ModeCheckMacro;

	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_GetMax()) ){
		*pszDefaultAction = NULL;
		return E_INVALIDARG;
	}
	*pszDefaultAction = DSysAllocString(L"Enter");
	return S_OK;
};

STDMETHODIMP CPPvAccServer::accSelect(long flagsSelect,VARIANT varChild)
{
	MSGMSG("accSelect",0);

	if ( flagsSelect & ~(SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION) ){
		return E_INVALIDARG;
	}

	ModeCheckMacro;
	if ( (V_VT(&varChild) != VT_I4) || (V_I4(&varChild) > m_GetMax()) ){
		return E_INVALIDARG;
	}
	SetFocus(vinfo.info.hWnd);
	return S_OK;
};

STDMETHODIMP CPPvAccServer::accLocation(long *pxLeft,long *pyTop,long *pcxWidth,long *pcyHeight,VARIANT varChild)
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
		ModeCheckMacro;
		if ( V_I4(&varChild) > m_GetMax() ) return E_INVALIDARG;

		int y  = V_I4(&varChild) - 1 - VOi->offY;
		pos.x = BoxView.left;
		pos.y = BoxView.top + y * LineY;
		ClientToScreen(vinfo.info.hWnd,&pos);

		*pxLeft = pos.x;
		*pyTop  = pos.y;
		*pcxWidth  = BoxView.right - BoxView.left;
		*pcyHeight = LineY;
		return S_OK;
	}
};

STDMETHODIMP CPPvAccServer::accNavigate(long navDir,VARIANT varStart,VARIANT *pvarEndUpAt)
{
	MSGMSG("accNavigate",0);
	V_VT(pvarEndUpAt) = VT_EMPTY;

	ModeCheckMacro;
	if ( (V_VT(&varStart) != VT_I4) || (V_I4(&varStart) > m_GetMax()) ){
		return E_INVALIDARG;
	}

	switch (navDir){
		case NAVDIR_FIRSTCHILD:
			if ( (V_I4(&varStart) == CHILDID_SELF) && (m_GetMax() > 0) ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = 1;
				break;
			}
			return S_FALSE;

		case NAVDIR_LASTCHILD:
			if ( (V_I4(&varStart) == CHILDID_SELF) && (m_GetMax() > 0) ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = m_GetMax();
				break;
			}
			return S_FALSE;

		case NAVDIR_NEXT:
		case NAVDIR_DOWN:
			if ( V_I4(&varStart) != CHILDID_SELF ){
				V_VT(pvarEndUpAt) = VT_I4;
				V_I4(pvarEndUpAt) = V_I4(&varStart) + 1; // Šî’ê‚ª1

				if ( V_I4(pvarEndUpAt) > m_GetMax() ){
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

STDMETHODIMP CPPvAccServer::accHitTest(long xLeft,long yTop,VARIANT *pvarChild)
{
	MSGMSG("accHitTest",0);
	int y = -1;
	POINT pos = {xLeft,yTop};

	V_VT(pvarChild) = VT_I4;
	ModeCheckMacro;
	ScreenToClient(vinfo.info.hWnd,&pos);

	if ( (pos.y >= BoxView.top) && (pos.y < BoxView.bottom) ){
		y = (pos.y - BoxView.top) / LineY + VOi->offY;
		if ( y >= VO_maxY ){
			y = -1;
		}else{
			if ( vo_.DModeBit == DOCMODE_TEXT ) y = VOi->ti[y].line - 1;
		}
	}

	if ( y >= 0 ){
		V_I4(pvarChild) = y + 1; // Šî’ê‚ª1
	}else{
		V_I4(pvarChild) = CHILDID_SELF;
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::accDoDefaultAction(VARIANT varChild)
{
	MSGMSG("accDoDefaultAction",0);
	if ( V_VT(&varChild) != VT_I4 ) return E_INVALIDARG;
	if ( V_I4(&varChild) != CHILDID_SELF ){
		ModeCheckMacro;
	 	if ( V_I4(&varChild) > m_GetMax() ) return E_INVALIDARG;
	}
	return S_OK;
};

STDMETHODIMP CPPvAccServer::put_accName(VARIANT,BSTR)
{
	MSGMSG("put_accName",0);
	return E_NOTIMPL;
};

STDMETHODIMP CPPvAccServer::put_accValue(VARIANT,BSTR)
{
	MSGMSG("put_accValue",0);
	return E_NOTIMPL;
};

int CPPvAccServer::m_GetCursor(void)
{
	int y = (VOsel.cursor != FALSE) ? VOsel.now.y.line : VOi->offY;
	if ( vo_.DModeBit == DOCMODE_HEX ){
		return y;
	}else{
		return VOi->ti[y].line - 1;
	}
}

int CPPvAccServer::m_GetMax(void)
{
	if ( vo_.DModeBit == DOCMODE_HEX ){
		return VO_maxY;
	}else{
		return VOi->ti[VOi->cline].line - 1;
	}
}

HMODULE hOleautDLL = NULL;
HMODULE hOleaccDLL = NULL;

IAccessible *PPvAccServer = NULL;

#pragma argsused
LRESULT WmGetObject(WPARAM wParam)
{
	if ( hOleaccDLL == NULL ){
		hOleaccDLL = LoadWinAPI("OLEACC.DLL",NULL,OLEACCDLL,LOADWINAPI_LOAD);
		if ( hOleaccDLL == NULL ) return 0;
		if ( hOleautDLL == NULL ){
			hOleautDLL = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUTDLL,LOADWINAPI_LOAD);
			if ( hOleautDLL == NULL ) return 0;
		}
	}
	if ( PPvAccServer != NULL ){
		// static_cast<IAccessible*>(PPvAccServer)->AddRef(); •s—v
		MSGMSG("WmGetObject 2 %d",PPvAccServer);
		return DLresultFromObject(xIID_IAccessible,wParam,
				static_cast<IAccessible*>(PPvAccServer));
	}else{
		CPPvAccServer *pAccServer = new CPPvAccServer();
		if ( pAccServer != NULL ){
			PPvAccServer = static_cast<IAccessible*>(pAccServer);
			if ( DNotifyWinEvent == DummyNotifyWinEvent ){
				GETDLLPROC(GetModuleHandle(StrUser32DLL),NotifyWinEvent);
				if ( DNotifyWinEvent == NULL ){
					DNotifyWinEvent = DummyNotifyWinEvent;
				}
			}
			MSGMSG("WmGetObject 1 %d",pAccServer);
			return DLresultFromObject(xIID_IAccessible,wParam,
					static_cast<IAccessible*>(pAccServer));
		}
	}
	return 0;
}

void FreeAccServer(void)
{
	if ( PPvAccServer != NULL ){
		PPvAccServer->Release();
		PPvAccServer = NULL;
	}
}
