/*-----------------------------------------------------------------------------
	Paper Plane cUI						ドラッグ処理-combo用

ドロップ対象
	PPc エントリ/アイコン
	PPc 空欄
	PPc ツリー
	Combo-Tab-Tab		PPc ツリー + 新規タブでリスト作成
	Combo-Tab-非Tab		新規タブでリスト作成
	Combo-ツリー		PPc ツリー
	Combo-その他		PPc ツリー
-----------------------------------------------------------------------------*/
#define DEFINEDnDDROP
#include "WINAPI.H"
#include <shlobj.h>
#include <ole2.h>
#include <wtypes.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#include "PPCOMBO.H"
#pragma hdrstop

UINT_PTR comboscrolltimerID = 0;
/*
int comboscrollcount;
POINT oldcombopos;
*/
//==============================================================
void CALLBACK ComboDDTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	POINT pos; // ,cpos;
	HWND hTargetWnd, hTargetWnd2;
/*
	int x = GetSystemMetrics(SM_CXDRAG);
	int y = GetSystemMetrics(SM_CYDRAG);
	int tabpane;
*/
	::GetCursorPos(&pos);
/*
	cpos.x = pos.x - oldcombopos.x;
	cpos.y = pos.y - oldcombopos.y;
	if ( (cpos.x < -x) || (cpos.x > x) || (cpos.y < -y) || (cpos.y > y) ){
		comboscrollcount = 0;
	}
	oldcombopos = pos;
*/
	hTargetWnd = ::WindowFromPoint(pos);
	hTargetWnd2 = ::GetParent(hTargetWnd);
/*
	for ( tabpane = 0 ; tabpane < Combo.Tabs ; tabpane++ ){
		if ( hTargetWnd == Combo.show[tabpane].tab.hWnd ){
			comboscrollcount++;
			if ( comboscrollcount > (1500 / TIMER_DRAGSCROLL) ){
				cpos = pos;
				ScreenToClient(hTargetWnd, &cpos);
				PostMessage(hTargetWnd, WM_LBUTTONDOWN, 0, TMAKELPARAM(cpos.x, cpos.y));
				PostMessage(hTargetWnd, WM_LBUTTONUP, 0, TMAKELPARAM(cpos.x, cpos.y));
			}
			return;
		}
	}
	comboscrollcount = 0;
*/
	if ( (hTargetWnd == Combo.hTreeWnd) || (hTargetWnd2 == Combo.hTreeWnd) ){
		::SendMessage(Combo.hTreeWnd, VTM_SCROLL, 0, 0);
	}
	return;
}

void CmbSetDDScroll(BOOL reg)
{
	if ( IsTrue(reg) ){	// 登録
		if ( ((Combo.hTreeWnd != NULL) || Combo.Tabs) && !comboscrolltimerID){
//			comboscrollcount = 0;
			comboscrolltimerID = SetTimer(Combo.hWnd,
				TIMERID_COMBODRAGSCROLL, TIMER_DRAGSCROLL, ComboDDTimerProc);
		}
	}else{					// 登録解除
		if (comboscrolltimerID){
			KillTimer(Combo.hWnd, TIMERID_COMBODRAGSCROLL);
			comboscrolltimerID = 0;
		}
	}
}

//============================================================== コンストラクタ
ComboDropTarget::ComboDropTarget()
{
	refs = 1;
}
//==================================================================== IUnknown
STDMETHODIMP ComboDropTarget::QueryInterface(REFIID iid, void **ppv)
{
	// Explorer では、IMarshal, 不明{0000001B-0000-0000-C000-000000000046}が実装されている。
	if( (iid == IID_IUnknown) || (iid == IID_IDropTarget) ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) ComboDropTarget::AddRef(void)
{
	return ++refs;
}

STDMETHODIMP_(ULONG) ComboDropTarget::Release(void)
{
	if(--refs == 0){
		delete this;
		return 0;
	}
	return refs;
}

STDMETHODIMP ComboDropTarget::DragEnter(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect)
{
	mTitem = 0;
	mTargetPath[0] = '\0';
	GetSourcePath(pDataObj, mSourcePath);
	return QueryDrop(grfKeyState, &pt, pdwEffect);
}

STDMETHODIMP ComboDropTarget::DragOver(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect)
{
	mKeyState = grfKeyState;
	CmbSetDDScroll(TRUE);
	return QueryDrop(grfKeyState, &pt, pdwEffect);
}
STDMETHODIMP ComboDropTarget::DragLeave(void)
{
	CmbSetDDScroll(FALSE);
	if ( Combo.hTreeWnd != NULL ){
		::SendMessage(Combo.hTreeWnd, VTM_POINTPATH, 0, (LPARAM)NULL);
	}
	mTitem = 0;
	mTargetPath[0] = '\0';
	return S_OK;
}

void ComboDropTarget::ComboDropDDFormat(LPDATAOBJECT pDataObj, DWORD pdwEffect, TCHAR *TargetPath)
{
	TMS_struct files = {{NULL, 0, NULL}, 0};
	TCHAR *sDrop;
	int mode;

	if ( GetDropData(pDataObj, &files) == FALSE ) return;
	sDrop = (TCHAR *)files.tm.p;

	if ( VFSGetDriveType(TargetPath, &mode, NULL) != NULL ){
		if ( (mode <= VFSPT_SHN_DESK) || (mode <= VFSPT_UNC) ){
			VFSGetRealPath(NULL, TargetPath, TargetPath);
		}
	}
	switch( pdwEffect ){
//=============================================================================
		case DROPEFFECT_ADDTREEITEM:
			::SendMessage(Combo.hTreeWnd, VTM_ADDTREEITEM, 0, (LPARAM)sDrop);
			break;

		case DROPEFFECT_MAKELIST: {
			TCHAR destpath[VFPS];

			ListDrop(Combo.hWnd, sDrop, TargetPath, destpath);
			if ( mTargettype == TARGET_TREE ){
				::SendMessage(Combo.hTreeWnd, VTM_SETPATH, 0, (LPARAM)TargetPath);
			}
			break;
		}

		case DROPEFFECT_LINK:
			LinkDrop(Combo.hWnd, sDrop, TargetPath);
			if ( mTargettype == TARGET_TREE ){
				::SendMessage(Combo.hTreeWnd, VTM_SETPATH, 0, (LPARAM)TargetPath);
			}
			break;
//=============================================================================
		case DROPEFFECT_JUMP:
		case DROPEFFECT_SWAP:
		case DROPEFFECT_DUP:
		case DROPEFFECT_OPENWITH:
			break;
		default:
			::ExecFileOperation(pdwEffect, TargetPath, &files, NULL, NULL);
			if ( mTargettype == TARGET_TREE ){
				::SendMessage(Combo.hTreeWnd, VTM_SETPATH, 0, (LPARAM)TargetPath);
			}
/*
			if ( IsTrue(::ExecFileOperation(pdwEffect, TargetPath, &files, NULL, NULL)) ){
				if ( mTargettype == TARGET_TREE ){
					::SendMessage(Combo.hTreeWnd, VTM_SETPATH, 0, (LPARAM)TargetPath);
				}
			}
*/
			return;
//=============================================================================
	}
	TMS_kill(&files);
}

STDMETHODIMP ComboDropTarget::Drop(LPDATAOBJECT pDataObj, DWORD, POINTL pt, LPDWORD pdwEffect)
{
	DROPTYPE DDtype;
	DWORD orgEffect = *pdwEffect;

	CmbSetDDScroll(FALSE);
	if ( QueryDrop(mKeyState, &pt, pdwEffect) == S_OK ){
		GetDropFormat(DDtype, pDataObj);

		if ( (mTargettype == TARGET_TOOLBAR) || (mTargettype == TARGET_TREE) || (mTargettype == TARGET_PPC) ){
			// 一体化時に非アクティブPPcからD&Dするとメッセージループが
			// 回らなくなるための対策
			if ( ::SelfDD_hWnd != NULL ){
				::SendMessage(::SelfDD_hWnd, WM_PPCSETFOCUS, 0, 0);
			}

			::DDMenu(Combo.hWnd, &pt, pdwEffect, orgEffect, mKeyState, pDataObj,
					((mTargettype == TARGET_TREE) || (mTargettype == TARGET_TOOLBAR)) ? DDCELL_CONTROL : DDCELL_SPACE, DDtype, NULL,
					mTargetPath, mDropModeFlags);
		}

		if ( (Combo.hTreeWnd != NULL) && (*pdwEffect != DROPEFFECT_ADDTREEITEM) ){
			::SendMessage(Combo.hTreeWnd, VTM_POINTPATH, 0, (LPARAM)NULL);
		}
		if ( (*pdwEffect > DROPEFFECT_OBJECTSTART) &&
			 (*pdwEffect <= DROPEFFECT_OBJECTEND) ){

			*pdwEffect = DROPEFFECT_NONE;
			return E_INVALIDARG;
		}

		if ( *pdwEffect != DROPEFFECT_NONE ){
			if ( mTargettype == TARGET_TOOLBAR ){
				if ( *pdwEffect != DROPEFFECT_ADDTOOLBAR ){
					ComboDropDDFormat(pDataObj, *pdwEffect, mTargetPath);
					return S_OK;
				}
			}

			if ( mTargettype == TARGET_TREE ){
				ComboDropDDFormat(pDataObj, *pdwEffect, mTargetPath);
				return S_OK;
			}
			{
				TMS_struct files = {{NULL, 0, NULL}, 0};
				TCHAR *sDrop;

				if ( GetDropData(pDataObj, &files) == FALSE ) return S_OK;
				sDrop = (TCHAR *)files.tm.p;

				if ( (mTargettype == TARGET_TOOLBAR) && (*pdwEffect == DROPEFFECT_ADDTOOLBAR)  ){
					AddToolBarButton(Combo.hWnd, sDrop, mToolBarName);
				}else if ( mTargettype == TARGET_REPORT ){
					::SendMessage(Combo.Report.hWnd, EM_REPLACESEL, 0, (LPARAM)sDrop);
				}else if ( mTargettype == TARGET_ADDR ){
					::SendMessage(Combo.hAddressWnd, WM_SETTEXT, 0, (LPARAM)sDrop);
				}else if ( mTargettype == TARGET_PPC ){
					ComboDropDDFormat(pDataObj, *pdwEffect, mTargetPath);
				}
			TMS_kill(&files);
			}
		}
		return S_OK;
	}
	*pdwEffect = DROPEFFECT_NONE;
	return E_INVALIDARG;
}

STDMETHODIMP ComboDropTarget::QueryDrop(DWORD grfKeyState, POINTL *pt, DWORD *pdwEffect)
{
	HWND hTargetWnd, hTargetWnd2;
	POINT cpos;

	mDefAction = DROPEFFECT_COPY;
	mDropModeFlags = 0;
	mTargettype = TARGET_NONE;
	cpos.x = pt->x;
	cpos.y = pt->y;
	hTargetWnd = ::WindowFromPoint(cpos);
	hTargetWnd2 = ::GetParent(hTargetWnd);
	if ( hTargetWnd == NULL ) hTargetWnd = BADHWND;
	if ( hTargetWnd2 == NULL ) hTargetWnd2 = BADHWND;

	if ( hTargetWnd2 == comboDocks.t.hWnd ){
		return QueryDrop_Dock(grfKeyState, pdwEffect, hTargetWnd, &cpos, &comboDocks.t);
	}

	if ( hTargetWnd2 == comboDocks.b.hWnd ){
		return QueryDrop_Dock(grfKeyState, pdwEffect, hTargetWnd, &cpos, &comboDocks.b);
	}

	if ( hTargetWnd == Combo.ToolBar.hWnd ){
		mToolBarName = T("B_cdef");
		return QueryDrop_ToolBar(grfKeyState, pdwEffect, hTargetWnd, &cpos, &thGuiWork);
	}

												// ツリーウィンドウ =======
	if ( (hTargetWnd == Combo.hTreeWnd) || (hTargetWnd2 == Combo.hTreeWnd) ){
		if ( IsTrue(QueryDrop_Tree(Combo.hTreeWnd, pt, pdwEffect)) ) return S_OK;
	}else  if (hTargetWnd == Combo.Report.hWnd){
		mTargettype = TARGET_REPORT;
		mDefAction = DROPEFFECT_LINK;
	}else if (hTargetWnd == Combo.hAddressWnd){
		mTargettype = TARGET_ADDR;
		mDefAction = DROPEFFECT_LINK;
	}else{
		PPC_APPINFO *cinfo;

		cinfo = ::GetComboTarget(hTargetWnd, &cpos);
		if ( cinfo != NULL ){
			tstrcpy(mTargetPath, (cinfo->e.Dtype.mode != VFSDT_LFILE) ? cinfo->RealPath : cinfo->path);
			mTargettype = TARGET_PPC;
			SetDirectoryDefAction();
		}
	}

	if (::GetDDEffect(FILE_ATTRIBUTE_DIRECTORY, grfKeyState, pdwEffect, mDefAction)){
		return S_OK;
	}

	*pdwEffect = DROPEFFECT_MOVE | DROPEFFECT_COPY | DROPEFFECT_LINK;
	return S_OK;
}
