/*-----------------------------------------------------------------------------
	Paper Plane cUI							～ ドラッグ処理-CEnumFormatEtc ～
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
CEnumFormatEtc::CEnumFormatEtc(LPFORMATETC pFE,int numberItems)
{
	m_cRefCount = 1;
	m_iCur = 0;
	m_cItems = numberItems;
	m_pStrFE = new FORMATETC[numberItems];
	memcpy(m_pStrFE,(LPBYTE)pFE,sizeof(FORMATETC) * numberItems);
}

//------------------------------------- デコンストラクタ
CEnumFormatEtc::~CEnumFormatEtc()
{
	if( m_cItems ) delete[] m_pStrFE;
}

//------------------------------------- 定型部分 ------------------------------
STDMETHODIMP CEnumFormatEtc::QueryInterface(REFIID refiid,void **ppv)
{
	Debug_DispIID(T("EnumFormatEtc"),refiid);
	if ( (IID_IUnknown == refiid) || (IID_IEnumFORMATETC == refiid) ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CEnumFormatEtc::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CEnumFormatEtc::Release(void)
{
	long tempCount;

	tempCount = --m_cRefCount;
	if ( tempCount == 0 ) delete this;
	return tempCount;
}

//------------------------------------- 主要部分 ------------------------------
STDMETHODIMP CEnumFormatEtc::Next(ULONG celt,LPFORMATETC lpFormatEtc,ULONG *pceltFetched)
{
	ULONG cReturn = 0;

	if( (celt <= 0) || (lpFormatEtc == NULL) || (m_iCur >= m_cItems) ){
		if ( NULL != pceltFetched ) *pceltFetched = 0;
		return ResultFromScode(S_FALSE);
	}
	if ((pceltFetched == NULL) && (celt != 1)) return ResultFromScode(S_FALSE);

	while ((m_iCur < m_cItems) && celt ){
		*lpFormatEtc++ = m_pStrFE[m_iCur++];
		cReturn++;
		celt--;
	}
	if ( NULL != pceltFetched ) *pceltFetched = cReturn;
	return ResultFromScode(S_OK);
}

STDMETHODIMP CEnumFormatEtc::Skip(ULONG celt)
{
	if ((m_iCur + celt) >= m_cItems) return ResultFromScode(S_FALSE);
	m_iCur += celt;
	return NOERROR;
}

STDMETHODIMP CEnumFormatEtc::Reset(void)
{
	m_iCur = 0;
	return ResultFromScode(S_OK);
}

STDMETHODIMP CEnumFormatEtc::Clone(IEnumFORMATETC ** ppCloneEnumFormatEtc)
{
	CEnumFormatEtc *newEnum;

	if(ppCloneEnumFormatEtc == NULL) return ResultFromScode(S_FALSE);

	newEnum = new CEnumFormatEtc(m_pStrFE,m_cItems);
	if (newEnum == NULL) return ResultFromScode(E_OUTOFMEMORY);
	newEnum->AddRef();
	newEnum->m_iCur = m_iCur;
	*ppCloneEnumFormatEtc = newEnum;
	return ResultFromScode(S_OK);
}
