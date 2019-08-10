/*-----------------------------------------------------------------------------
	Paper Plane cUI								～ ドラッグ処理-IDataObject ～
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
CImpIDataObject::CImpIDataObject()
{
	m_cRefCount = 1;
	m_cFormatsAvailable = 0;
}
//------------------------------------- デコンストラクタ
CImpIDataObject::~CImpIDataObject()
{
	if(m_cRefCount != 0) xmessage(XM_FaERRd, T("~CImpIDataObject"));
}

//------------------------------------- 定型部分 ------------------------------
STDMETHODIMP CImpIDataObject::QueryInterface(REFIID refiid, void ** ppv)
{
	// Explorer では、IMarshal, ICallFactory, 不明{0000001B-0000-0000-C000-000000000046}が実装されている

	if ( (IID_IUnknown == refiid) || (IID_IDataObject == refiid) ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CImpIDataObject::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CImpIDataObject::Release(void)
{
	long tempCount;

	tempCount = --m_cRefCount;
	if ( tempCount == 0 ) delete this;
	return tempCount;
}

//------------------------------------- 主要部分 ------------------------------
STDMETHODIMP CImpIDataObject::GetData(LPFORMATETC pFE, LPSTGMEDIUM pSM)
{
	if ( pFE == NULL || pSM == NULL ) return ResultFromScode(E_INVALIDARG);
	for ( int i = 0; i < m_cFormatsAvailable; i++ ){
		if ( (pFE->cfFormat == m_strFormatEtc[i].cfFormat) &&
			(pFE->tymed & m_strFormatEtc[i].tymed) ){
			// 未展開 && マウスボタンが離れていたら展開する
			if ( (m_arcmode == TRUE) &&
				!((::GetAsyncKeyState(VK_LBUTTON) |
					::GetAsyncKeyState(VK_RBUTTON)) & KEYSTATE_PUSH) ){
				m_arcmode = -1;
				PPcUnpackForAction(m_cinfo, m_arctemp, UFA_EXTRACT);
			}
			if ( m_strStgMedium[i].tymed ){
				pSM->tymed = m_strStgMedium[i].tymed;
				pSM->pUnkForRelease = m_strStgMedium[i].pUnkForRelease;

				pSM->hGlobal = ::OleDuplicateData(
					m_strStgMedium[i].hGlobal, pFE->cfFormat, (UINT)NULL);
			} else{				// STGMEDIUM が未作成→この場で作成
				TCHAR backpath[VFPS];

				pSM->tymed = TYMED_HGLOBAL;
				pSM->pUnkForRelease = NULL;

				if ( IsTrue(m_arcmode) ){
					m_cinfo->UnpackFix = TRUE;
					tstrcpy(backpath, m_cinfo->RealPath);
					tstrcpy(m_cinfo->RealPath, m_arctemp);
				}

				if ( (UINT)pFE->cfFormat == CF_xSHELLIDLIST ){
					pSM->hGlobal = ::CreateShellIdList(m_cinfo);

				} else if ( pFE->cfFormat == CF_HDROP ){
					pSM->hGlobal = ::CreateHDrop(m_cinfo);
				} else if ( pFE->cfFormat == CF_TTEXT ){
					pSM->hGlobal = ::CreateHText(m_cinfo);
				} else{
					return DV_E_FORMATETC;
				}

				if ( IsTrue(m_arcmode) ){
					m_cinfo->UnpackFix = FALSE;
					tstrcpy(m_cinfo->RealPath, backpath);
				}

				if ( pSM->hGlobal == NULL ) return E_OUTOFMEMORY;
			}
			return S_OK;
		}
	}
	return DV_E_FORMATETC;
}

STDMETHODIMP CImpIDataObject::GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSM)
{
	if ( pFE == NULL || pSM == NULL ) return ResultFromScode(E_INVALIDARG);
	return ResultFromScode(E_NOTIMPL);
}

STDMETHODIMP CImpIDataObject::QueryGetData(LPFORMATETC pFE)
{
	int i;

	if ( pFE == NULL ) return ResultFromScode(S_FALSE);

	// pFE->dwAspect == 0(NULL) と誤った指定をしているアプリケーションが
	// あるので、チェックしないようにした
	//if ( !(DVASPECT_CONTENT & pFE->dwAspect) ) return DV_E_DVASPECT;

	for ( i = 0; i < m_cFormatsAvailable; i++ ){
		if ( (pFE->cfFormat == m_strFormatEtc[i].cfFormat) &&
			(pFE->tymed & m_strFormatEtc[i].tymed) ){
			return S_OK;
		}
	}
	return DV_E_TYMED;
}

STDMETHODIMP CImpIDataObject::GetCanonicalFormatEtc(LPFORMATETC, LPFORMATETC)
{
	return E_NOTIMPL;
}

STDMETHODIMP CImpIDataObject::SetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL fRelease)
{
	int i;

	if ( pFE == NULL || pSTM == NULL ) return ResultFromScode(E_INVALIDARG);
	if ( !fRelease ) return ResultFromScode(E_FAIL);

	for ( i = 0; i < m_cFormatsAvailable; i++ ){
		if ( m_strFormatEtc[i].cfFormat == pFE->cfFormat ){
			m_strStgMedium[i] = *pSTM;
			m_strFormatEtc[i] = *pFE;
		}
	}
	if ( i < MAX_NUM_FORMAT ){
		m_strStgMedium[i] = *pSTM;
		m_strFormatEtc[i] = *pFE;
		m_cFormatsAvailable++;
		return S_OK;
	} else{
		return ResultFromScode(E_OUTOFMEMORY);
	}
}

STDMETHODIMP CImpIDataObject::EnumFormatEtc(DWORD dwDir, LPENUMFORMATETC FAR *pEnum)
{
	if ( pEnum == NULL ) return ResultFromScode(OLE_S_USEREG);
	switch ( dwDir ){
		case DATADIR_GET:
			*pEnum = new CEnumFormatEtc(m_strFormatEtc, m_cFormatsAvailable);
			break;

			//case DATADIR_SET:
		default:
			return E_NOTIMPL;
	}
	return ResultFromScode(S_OK);
}
//------------------------------------- 未実装 --------------------------------
STDMETHODIMP CImpIDataObject::DAdvise(FORMATETC *, DWORD, LPADVISESINK, DWORD *)
{
	return ResultFromScode(E_NOTIMPL);
}
STDMETHODIMP CImpIDataObject::DUnadvise(DWORD)
{
	return ResultFromScode(E_NOTIMPL);
}

STDMETHODIMP CImpIDataObject::EnumDAdvise(LPENUMSTATDATA *)
{
	return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}
