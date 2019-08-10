/*-----------------------------------------------------------------------------
	Paper Plane cUI								～ ドラッグ処理-IDropSource ～
-----------------------------------------------------------------------------*/
#define DEFINEDnDDROP
#include "WINAPI.H"
#include <shlobj.h>
#include <ole2.h>

#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"

#include "PPC_DD.H"
#pragma hdrstop

//------------------------------------- コンストラクタ
CImpIDropSource::CImpIDropSource()
{
	m_cRefCount = 1;
}
//------------------------------------- デコンストラクタ
CImpIDropSource::~CImpIDropSource()
{
	if(m_cRefCount != 0) xmessage(XM_FaERRd, T("~CImpIDropSource"));
}
//------------------------------------- 定型部分 ------------------------------
STDMETHODIMP CImpIDropSource::QueryInterface(REFIID refiid, void **ppv)
{
	// Explorer では、IDropSourceNotify、不明{0000012C-0000-0000-C000-000000000046}が実装されている

	if ( (IID_IUnknown == refiid) || (IID_IDropSource == refiid) ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CImpIDropSource::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CImpIDropSource::Release(void)
{
	long tempCount;

	tempCount = --m_cRefCount;
	if (tempCount == 0) delete this;
	return tempCount;
}

//------------------------------------- 主要部分 ------------------------------
STDMETHODIMP CImpIDropSource::GiveFeedback(DWORD)
{
	return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}

STDMETHODIMP CImpIDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
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
