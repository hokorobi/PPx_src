/*-----------------------------------------------------------------------------
	Paper Plane cUI						ドラッグ処理-main

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

// クリップボードID
#ifndef CFSTR_UNTRUSTEDDRAGDROP
	#define CFSTR_UNTRUSTEDDRAGDROP T("UntrustedDragDrop")
#endif
#ifdef WINEGCC // wine1.1.39 以降で定義が誤っているのを修正
	#undef CFSTR_FILEDESCRIPTORA
	#define CFSTR_FILEDESCRIPTORA T("FILEGROUPDESCRIPTORA")
	#undef CFSTR_FILEDESCRIPTORW
	#define CFSTR_FILEDESCRIPTORW T("FILEGROUPDESCRIPTORW")
#endif

UINT cfFileContents;
UINT cfFileGroupDescriptorA,cfFileGroupDescriptorW;
#if USE_URLDROP
UINT cfURL;
#endif
const TCHAR DupOptionStr[] = T("/same:5 /sameall:1");

	// このウィンドウへのドラッグを処理するための IDropTarget

struct ExtensionsStruct {
	DWORD baseindex;	// メニューの開始インデックス
	IShellExtInit *pse;
	IContextMenu *pcm;
};

#define mCEL(No) ((ENTRYCELL *)mCinfo->e.CELLDATA.p)[((DWORD *)mCinfo->e.INDEXDATA.p)[No]]
#define mCELdata(No) ((ENTRYCELL *)mCinfo->e.CELLDATA.p)[No]
#define mCELt(No) ((DWORD *)mCinfo->e.INDEXDATA.p)[No]

const TCHAR HandlersKey[] = T("Folder\\shellex\\DragDropHandlers");
const TCHAR StrDDrecvError[] = T("D&D data receive error");
TCHAR StrLinkJump[] = T("link");

#if 0
UINT cfUntrustedDragDrop;
const IID IID_IInternetSecurityManager =
{0x79eac9ee,0xbaf9,0x11ce,{0x8c,0x82,0x00,0xaa,0x00,0x4b,0xa9,0x0b}};
const CLSID CLSID_InternetSecurityManager =
{0x7b8a2d94,0x0ac9,0x11d1,{0x89,0x6c,0x00,0xc0,0x4f,0xb6,0xbf,0xc4}};
typedef void IInternetSecurityMgrSite;
class IInternetSecurityManager : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE SetSecuritySite(IInternetSecurityMgrSite *) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetSecuritySite(IInternetSecurityMgrSite **) = 0;
		virtual HRESULT STDMETHODCALLTYPE MapUrlToZone(LPCWSTR,DWORD *,DWORD) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetSecurityId(LPCWSTR,BYTE *,DWORD *,DWORD_PTR) = 0;
		virtual HRESULT STDMETHODCALLTYPE ProcessUrlAction(
			LPCWSTR,DWORD,BYTE *,DWORD,BYTE *,DWORD,DWORD,DWORD) = 0;
		virtual HRESULT STDMETHODCALLTYPE QueryCustomPolicy(
			LPCWSTR,REFGUID,BYTE **,DWORD *,BYTE *,DWORD,DWORD) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetZoneMapping(DWORD,LPCWSTR,DWORD) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetZoneMappings(DWORD dwZone,IEnumString **ppenumString,DWORD dwFlags) = 0;
};
DefineWinAPI(HRESULT,CoInternetCreateSecurityManager,(void *,IInternetSecurityManager **,DWORD)) = 0;
#endif

void PPcDropTargetBase::SetDirectoryDefAction(void)
{
	if ( X_lddm[0] < 0 ){
		X_lddm[0] = 0;
		::GetCustData(T("X_lddm"),&X_lddm,sizeof(X_lddm));
	}

	if ( X_lddm[2] == 0 ){
		int len = TSTRLENGTH32(mSourcePath);

		mDefAction = ( memicmp((char *)mSourcePath,(char *)mTargetPath,len) == 0 ) ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
	}else{
		mDefAction = (X_lddm[2] == 1) ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
	}
}

STDMETHODIMP PPcDropTargetBase::QueryDrop_DriveBar(DWORD grfKeyState,DWORD *pdwEffect,HWND hToolBarWnd,POINT *pos)
{
	LRESULT index;
	TBBUTTON tb;

	mTargettype = TARGET_TOOLBAR;

	::ScreenToClient(hToolBarWnd,pos);
	index = SendMessage(hToolBarWnd,TB_HITTEST,0,(LPARAM)pos);

	if ( index >= 0 ){
		SendMessage(hToolBarWnd,TB_GETBUTTON,index,(LPARAM)&tb);
		wsprintf(mTargetPath,T("%c:\\"),(tb.idCommand - IDW_DRIVES) + 'A');

		mDropModeFlags = DROPMODE_TREE;
		mCell = DDCELL_CONTROL;
		SetDirectoryDefAction();
		if ( ::GetDDEffect( FILE_ATTRIBUTE_DIRECTORY,
				grfKeyState,pdwEffect,mDefAction) ){
			return S_OK;
		}
		*pdwEffect = DROPEFFECT_MOVE | DROPEFFECT_COPY | DROPEFFECT_LINK;
		return S_OK;
	}
	mDropModeFlags = 0;
	*pdwEffect = DROPEFFECT_NONE;
	mCell = DDCELL_CONTROL;
	return S_FALSE;
}

STDMETHODIMP PPcDropTargetBase::QueryDrop_ToolBar(DWORD grfKeyState,DWORD *pdwEffect,HWND hToolBarWnd,POINT *pos,ThSTRUCT *ToolBarTh)
{
	LRESULT index;
	TBBUTTON tb;

	mTargettype = TARGET_TOOLBAR;

	::ScreenToClient(hToolBarWnd,pos);
	index = SendMessage(hToolBarWnd,TB_HITTEST,0,(LPARAM)pos);
	if ( index >= 0 ){
		SendMessage(hToolBarWnd,TB_GETBUTTON,index,(LPARAM)&tb);

		const TCHAR *p = GetToolBarCmd(hToolBarWnd,ToolBarTh,tb.idCommand);
		if ( (p != NULL) && ((UTCHAR)*p == EXTCMD_CMD) &&
				 (*(p + 1) == '%') && (*(p + 2) == 'j') ){
			p += 3;
			GetLineParam(&p,mTargetPath);
			mDropModeFlags = DROPMODE_TREE;
			mCell = DDCELL_CONTROL;
			SetDirectoryDefAction();
			if ( ::GetDDEffect( FILE_ATTRIBUTE_DIRECTORY,
					grfKeyState,pdwEffect,mDefAction) ){
				return S_OK;
			}
			*pdwEffect = DROPEFFECT_MOVE | DROPEFFECT_COPY | DROPEFFECT_LINK;
			return S_OK;
		}
	} // リンク以外
	mDropModeFlags = DROPMODE_TOOLBAR;
	mCell = DDCELL_CONTROL;
	*pdwEffect = DROPEFFECT_LINK;
	return S_OK;
}

STDMETHODIMP PPcDropTargetBase::QueryDrop_Dock(DWORD grfKeyState,DWORD *pdwEffect,HWND hTargetWnd,POINT *pos,PPXDOCK *dock)
{
	REBARBANDINFO rbi;
	int i,count;
	TCHAR *bar;

	count = (int)SendMessage(dock->hWnd,RB_GETBANDCOUNT,0,0);
	for( i = 0 ; i < count ; i++ ){
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_CHILD | RBBIM_ID;
		rbi.wID = NOSETWID;

		SendMessage(dock->hWnd,RB_GETBANDINFO,i,(LPARAM)&rbi);
		if ( rbi.hwndChild != hTargetWnd ) continue;
		if ( rbi.wID == NOSETWID ) break;

		bar = ThPointerT(dock->th,rbi.wID);

		if ( tstrcmp(bar,T("drives")) == 0 ){
			mToolBarName = bar;
			return QueryDrop_DriveBar(grfKeyState,pdwEffect,hTargetWnd,pos);
		}

		if ( (bar[0] == 'B') && (bar[1] == '_') ){
			mToolBarName = bar;
			return QueryDrop_ToolBar(grfKeyState,pdwEffect,hTargetWnd,pos,dock->th);
		}
		break; // 不明コントロールは D&D 不可
	}
	// 該当無し
	mDropModeFlags = 0;
	*pdwEffect = DROPEFFECT_NONE;
	mCell = DDCELL_CONTROL;
	return S_FALSE;
}

BOOL PPcDropTargetBase::QueryDrop_Tree(HWND hTreeWnd,POINTL *pt,DWORD *pdwEffect)
{
	VTMPOINTPATHSTRUCT vpps;
	int treetype;

	mDropModeFlags = 0;

	vpps.Titem = mTitem;
	vpps.pos.x = pt->x;
	vpps.pos.y = pt->y;
	vpps.path = mTargetPath;
	treetype = (int)::SendMessage(hTreeWnd,VTM_POINTPATH,0,(LPARAM)&vpps);
	mTitem = vpps.Titem;
	if ( treetype == 2 ){
		setflag(mDropModeFlags,DROPMODE_TREELIST);
	}
	if ( mTitem == 0 ){
		mCell = DDCELL_BADPOSITION;
		if ( treetype == 2 ){
			*pdwEffect = DROPEFFECT_LINK;
			return TRUE;
		}else{
			*pdwEffect = DROPEFFECT_NONE;
			return FALSE;
		}
	}
	mDropModeFlags |= DROPMODE_TREE;
	mTargettype = TARGET_TREE;
	mCell = DDCELL_CONTROL;
	SetDirectoryDefAction();
	return FALSE;
}

#if FREEPOSMODE
void MoveFreeCellPosition(PPC_APPINFO *cinfo,POINTL *pt)
{
	POINT cellpos;

	cellpos.x = pt->x;
	cellpos.y = pt->y;
	ScreenToClient(cinfo->info.hWnd,&cellpos);
	cellpos.x += CalcFreePosOffX(cinfo);
	cellpos.y += CalcFreePosOffY(cinfo);

	if ( CEL(cinfo->e.cellN).pos.x != NOFREEPOS ){ // 既に移動している
		int i;
		// 並び替え処理
		i = cinfo->FreePosEntries - 1;
		for ( ; i > 0 ; i-- ){
			if ( cinfo->FreePosList[i].index == cinfo->e.cellN ) break;
		}
		memmove(cinfo->FreePosList + i,cinfo->FreePosList + i + 1,sizeof(FREEPOSPOINT) * (cinfo->FreePosEntries - i - 1));
		cinfo->FreePosList[cinfo->FreePosEntries - 1].index = cinfo->e.cellN;
	}else{
		if ( cinfo->FreePosEntries < FREEPOSMODE ){
			cinfo->FreePosList[cinfo->FreePosEntries].index = cinfo->e.cellN;
			cinfo->FreePosEntries++;
		}else{ // 入りきらないので古いのを元に戻す
			CEL(cinfo->FreePosList[0].index).pos.x = NOFREEPOS;
			memmove(cinfo->FreePosList,cinfo->FreePosList + 1,sizeof(FREEPOSPOINT) * (FREEPOSMODE - 1));
			cinfo->FreePosList[FREEPOSMODE - 1].index = cinfo->e.cellN;
		}
	}
	CEL(cinfo->e.cellN).pos = cellpos;

	::InvalidateRect(cinfo->info.hWnd,NULL,TRUE);
	cinfo->cellNpos.x = cinfo->TargetNpos.x = CalcCellX(cinfo,cinfo->e.cellN - cinfo->cellWMin);
	cinfo->cellNpos.y = cinfo->TargetNpos.y = CalcCellY(cinfo,cinfo->e.cellN - cinfo->cellWMin);
}
#endif

void AddToolBarButton(HWND hWnd,const TCHAR *sDrop,const TCHAR *ToolBarName)
{
	TOOLBARCUSTTABLESTRUCT tbs;
	TCHAR *tip,tiptext[VFPS * 2];
	DWORD attr = GetFileAttributes(sDrop);

	tbs.text[0] = EXTCMD_CMD;
	tip = FindLastEntryPoint(sDrop);
	wsprintf(tiptext,T("%s/%s"),tip,tip);
	if ( (attr != BADATTR) && !(attr & FILE_ATTRIBUTE_DIRECTORY) ){
		tbs.index = 11; // ファイルを登録
		wsprintf(tbs.text + 1,T("\"%s\""),sDrop);
	}else{
		tbs.index = 12; // ディレクトリを登録
		wsprintf(tbs.text + 1,T("%%j\"%s\""),sDrop);
	}
	SetCustTable(ToolBarName,tiptext,&tbs,TSTRSIZE(tbs.text) + sizeof(tbs.index));
	::PostMessage(hWnd, WM_PPXCOMMAND, K_Scust, 0);
	::PostMessage(hWnd, WM_PPXCOMMAND, K_Lcust, 0);
}

BOOL ExecFileOperation(DWORD pdwEffect,const TCHAR *targetpath,TMS_struct *files,PPC_APPINFO *cinfo,const TCHAR *option)
{
	BOOL result;
	VFSFILEOPERATION fileop;

	// 処理方法を設定
	fileop.action	= (pdwEffect == DROPEFFECT_MOVE) ? FileOperationMode_Move : FileOperationMode_Copy;
	fileop.flags	= (pdwEffect == DROPEFFECT_DETAIL) ? VFSFOP_SPECIALDEST : VFSFOP_AUTOSTART | VFSFOP_SPECIALDEST;

	if ( (::SelfDD_hWnd != NULL) && (::SelfDD_Dtype == VFSDT_PATH) ){
		// PPcからの VFSDT_PATH なら、別スレッドにしてもコピー元が削除される恐れがないので、別スレッドにする
		PPc_DoFileOperation(cinfo,fileop.action,files,targetpath,option,fileop.flags);
		return TRUE;
	}

	fileop.src		= NULL;
	fileop.dest		= targetpath;
	fileop.files	= (TCHAR *)files->tm.p;
	fileop.option	= option;
	fileop.dtype	= VFSDT_UNKNOWN;
	fileop.info		= &cinfo->info; // cinfo == NULL でも問題ない
	fileop.hReturnWnd = (cinfo != NULL) ? cinfo->info.hWnd : NULL;

	result = PPxFileOperation(NULL,&fileop);
	TM_kill(&files->tm);
	return result;
}

DWORD GetDDEffect(DWORD targetattr,DWORD keystate,DWORD *result,DWORD defaction)
{
	DWORD tmpresult,enable;

	if ( !(targetattr & FILE_ATTRIBUTE_DIRECTORY) ){ // ターゲットがファイル
		tmpresult = DROPEFFECT_LINK;
	}else{
											// シフトキーの状態による方法の判定
		if ( keystate & MK_ALT ){			// Alt
			tmpresult = (keystate & (MK_CONTROL | MK_SHIFT)) ?
					DROPEFFECT_NONE : DROPEFFECT_LINK;
		}else if ( keystate & MK_CONTROL ){	// Ctrl
			tmpresult = (keystate & MK_SHIFT) ?
					DROPEFFECT_LINK : DROPEFFECT_COPY;
		}else{
			tmpresult = (keystate & MK_SHIFT) ?
					DROPEFFECT_MOVE : DROPEFFECT_NONE;
		}
											// シフトキーが押されていない時
		if ( tmpresult == DROPEFFECT_NONE ){
			if ( DROPEFFECT_MOVE & *result & defaction ){
				tmpresult = DROPEFFECT_MOVE;
			}else if ( DROPEFFECT_COPY & *result ){
				tmpresult = DROPEFFECT_COPY;
			}else if ( DROPEFFECT_LINK & *result ){
				tmpresult = DROPEFFECT_LINK;
			}else{
				tmpresult = DROPEFFECT_NONE;
			}
		}
	}
	enable = *result & tmpresult;
	if ( enable == 0 ) enable = DROPEFFECT_LINK;
	*result = enable;
	return enable;
}

void USEFASTCALL AppendDefaultDDMenu(HMENU hMenu)
{
	::AppendMenuString(hMenu,DROPEFFECT_COPY,MES_DDCP);
	::AppendMenuString(hMenu,DROPEFFECT_MOVE,MES_DDMV);
	::AppendMenuString(hMenu,DROPEFFECT_LINK,MES_DDLI);
	::AppendMenuString(hMenu,DROPEFFECT_MAKELIST,MES_DDML);
	::AppendMenuString(hMenu,DROPEFFECT_DETAIL,MES_DDDT);
}

void DDMenu(HWND hWnd,POINTL *pt,DWORD *pdwEffect,DWORD orgEffect,DWORD key,LPDATAOBJECT pDataObj,int Cell,DROPTYPE &DDtype,PPC_APPINFO *cinfo,const TCHAR *TargetPath,DWORD DropModeFlags)
{
	HMENU hMenu = NULL;

	if ( cinfo && (Cell != DDCELL_CONTROL) && (::SelfDD_hWnd == hWnd) ){ // 同じ窓上で D&D
#if FREEPOSMODE
		if ( Cell < 0 ){
			MoveFreeCellPosition(cinfo,pt);
			*pdwEffect = DROPEFFECT_NONE;
			return;
		}
#endif
		// 左ボタンのメニュー不要指定なので、そのまま終わる
		if ( !X_lddm[1] && (key & MK_LBUTTON) && (Cell >= 0) &&
				(CEL(Cell).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			return;
		}

		hMenu = ::CreatePopupMenu();
		if ( Cell < 0 ){	// 空欄
			if ( DropModeFlags & DROPMODE_TREELIST ){
				::AppendMenuString(hMenu,DROPEFFECT_ADDTREEITEM,MES_DDAI);
			}else if ( cinfo->RealPath[0] != '?' ){
				::AppendMenuString(hMenu,DROPEFFECT_DUP,MES_DDDP);
				::AppendMenu(hMenu,MF_GS,0,MessageText(MES_DDCM));
				::AppendMenuString(hMenu,DROPEFFECT_LINK,MES_DDSC);
				::AppendMenuString(hMenu,DROPEFFECT_MAKELIST,MES_DDML);
				::AppendMenuString(hMenu,DROPEFFECT_DETAIL,MES_DDDT);
				if ( DDtype.text || DDtype.etc ){
					::AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
					if ( DDtype.text ){
						::AppendMenuString(hMenu,DROPEFFECT_TEXT,MES_DDTX);
					}
					if ( DDtype.etc ){
						::AppendMenuString(hMenu,DROPEFFECT_ETC,MES_DDET);
					}
				}
			}
		}else{				// このセル
			if ( DropModeFlags & DROPMODE_EDGE ){
				if ( !(CEL(Cell).attr & (ECA_THIS | ECA_PARENT)) ){
					::AppendMenuString(hMenu,DROPEFFECT_SWAP,MES_DDSW);
				}
			}else{
				if ( IsTrue(DDtype.hdrop) ){
					if ( CEL(Cell).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
						::AppendDefaultDDMenu(hMenu);
					}else{										// Entry
						if ( DropModeFlags & DROPMODE_FILEFOLDER ){
							::AppendMenuString(hMenu,DROPEFFECT_COPY,MES_DDCP);
						}else{
							::AppendMenuString(hMenu,DROPEFFECT_OPENWITH,MES_DDOP);
						}
					}
				}
			}
		}
	}else{
		if ( X_lddm[0] || (key & MK_RBUTTON) || (DropModeFlags & DROPMODE_TOOLBAR) ){			// 右ドラッグ
			hMenu = ::CreatePopupMenu();
		}else{											// 左ドラッグ
			if ( DDtype.hdrop == FALSE ){
#if USE_URLDROP
				if ( (DDtype.pidl == FALSE) && (DDtype.url == FALSE) )
#else
				if ( DDtype.pidl == FALSE )
#endif
				{
					hMenu = ::CreatePopupMenu();
				}
			}else{
				if ( (DDtype.priority == cfFileContents) && (cfFileContents != 0) && (*pdwEffect == DROPEFFECT_COPY) ){
					*pdwEffect = DROPEFFECT_FILECONTENTS;
				}
			}
		}
		if ( hMenu != NULL ){
			if ( DropModeFlags & DROPMODE_TOOLBAR ){
				::AppendMenuString(hMenu,DROPEFFECT_ADDTOOLBAR,MES_DDAB);
			}else{
				if ( cinfo && (cinfo->RealPath[0] == '?') ){ // 完全に仮想ならジャンプのみ
					if ( Cell != DDCELL_CONTROL ){
						::AppendMenuString(hMenu,DROPEFFECT_JUMP,MES_DDJP);
					}
															// ファイルなど
				}else if ( cinfo && (Cell >= 0) &&
					!(CEL(Cell).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
					::AppendMenuString(hMenu,DROPEFFECT_OPENWITH,MES_DDOP);
				}else{
					if ( IsTrue(DDtype.hdrop) ){
						if ( (DDtype.priority == cfFileContents) && (cfFileContents != 0) ){
							MENUITEMINFO minfo;

							minfo.cbSize = sizeof(minfo);
							minfo.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID;
							minfo.fType = MFT_STRING;
							minfo.fState = MFS_ENABLED | MFS_DEFAULT;
							minfo.wID = DROPEFFECT_FILECONTENTS;
							minfo.dwTypeData = (TCHAR *)MessageText(MES_DDCP);
							::InsertMenuItem(hMenu,0xffff,TRUE,&minfo);
						}

						::AppendDefaultDDMenu(hMenu);
						if ( cinfo && (Cell != DDCELL_CONTROL) ){
							::AppendMenuString(hMenu,DROPEFFECT_JUMP,MES_DDJP);
						}
					}else{
						if ( IsTrue(DDtype.filecontents) ){
							::AppendMenuString(hMenu,
									DROPEFFECT_FILECONTENTS,MES_DDCP);
							if ( orgEffect & DROPEFFECT_MOVE ){
								::AppendMenuString(hMenu,DROPEFFECT_MOVE,MES_DDMV);
							}
						}
#if USE_URLDROP
						if ( IsTrue(DDtype.pidl) || IsTrue(DDtype.url) )
#else
						if ( IsTrue(DDtype.pidl) )
#endif
						{
							if ( cinfo && (Cell != DDCELL_CONTROL) ){
								::AppendMenuString(hMenu,DROPEFFECT_JUMP,MES_DDJP);
							}
							::AppendMenuString(hMenu,DROPEFFECT_LINK,MES_DDSC);
						}
					}
					if ( DDtype.text || DDtype.etc ){
						::AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
						if (DDtype.text){
							::AppendMenuString(hMenu,DROPEFFECT_TEXT,MES_DDTX);
						}
						if (DDtype.etc){
							::AppendMenuString(hMenu,DROPEFFECT_ETC,MES_DDET);
						}
					}
					if ( DropModeFlags & DROPMODE_TREELIST ){
						::AppendMenuString(hMenu,DROPEFFECT_ADDTREEITEM,MES_DDAI);
					}
				}
			}
		}else{ // 左ボタンのメニュー不要指定
			if ( (cinfo != NULL) && (Cell >= 0) &&
				!(CEL(Cell).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
				// ファイルなら開く
				*pdwEffect = DROPEFFECT_OPENWITH;
			}
		}
	}
										// DragDropHandler を追加 -------------
	if ( hMenu != NULL ){
		HKEY hKey,hFolderKey = NULL;
		int enumno = 0,index = ESINDEX;
		ThSTRUCT thEsdata;
		LPITEMIDLIST idl = NULL;
		struct ExtensionsStruct ES;

		ThInit(&thEsdata);
		::RegOpenKeyEx(HKEY_CLASSES_ROOT,HandlersKey,0,KEY_READ,&hKey);
		for ( ; ; ){
			TCHAR keyname[MAX_PATH];
			TCHAR idname[MAX_PATH];
			CLSID hid;
			DWORD s;
			FILETIME ft;
			HRESULT hres;

			s = MAX_PATH;
			if ( ::RegEnumKeyEx(hKey,enumno++,keyname,&s,NULL,NULL,NULL,&ft) !=
					ERROR_SUCCESS){
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

			if ( idl == NULL ){
				idl = PathToPidl(TargetPath);
				if ( idl == NULL ) break;
			}
			if ( hFolderKey == NULL ){
				if ( ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_CLASSES_ROOT,
						StrRegFolder,0,KEY_READ,&hFolderKey) ){
					continue;
				}
			}

			hres = ::CoCreateInstance(hid,NULL,CLSCTX_INPROC_SERVER,
									IID_IShellExtInit,(LPVOID *)&ES.pse);
			if ( FAILED(hres) ) continue;

			ES.pcm = NULL;
			if ( SUCCEEDED(ES.pse->Initialize(idl,pDataObj,hFolderKey)) ){
				if ( FAILED(ES.pse->QueryInterface(
						IID_IContextMenu,(LPVOID *)&ES.pcm)) ){
					ES.pcm = NULL;
				}
			}
			if ( ES.pcm == NULL ){
				ES.pse->Release();
				continue;
			}
			ES.pcm->QueryContextMenu(hMenu,0,index,
					index + ESINDEXSTEP - 1,CMF_NORMAL | CMF_EXTENDEDVERBS);
			ES.baseindex = index;
			index += ESINDEXSTEP;
			ThAppend(&thEsdata,&ES,sizeof(struct ExtensionsStruct));
		}
		::RegCloseKey(hKey);
		// ターミネータ設定
		ES.baseindex = 0;
		::ThAppend(&thEsdata,&ES,sizeof(struct ExtensionsStruct));
										// メニュー表示 -----------------------
		::ForceSetForegroundWindow(hWnd);

		::AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
		::AppendMenuString(hMenu,DROPEFFECT_NONE,MES_CCAN);
		*pdwEffect = TrackPopupMenu(hMenu,TPM_TDEFAULT,
							pt->x,pt->y,0,hWnd,NULL);
		::DestroyMenu(hMenu);
										// DragDropHandler なら、ここで処理 ---
		if ( (ESINDEX <= *pdwEffect) &&
			 (DROPEFFECT_FILEEXTSTART > *pdwEffect) ){
			struct ExtensionsStruct *es;

			es = (struct ExtensionsStruct *)thEsdata.bottom;
			for ( ; es->baseindex ; es++ ){
				if ( *pdwEffect >= (es->baseindex + ESINDEXSTEP) ) continue;

				CMINVOKECOMMANDINFO cmi;

				cmi.cbSize		= sizeof(CMINVOKECOMMANDINFO);
				cmi.fMask		= 0;
				cmi.hwnd		= hWnd;
				cmi.lpParameters = NULL;
				cmi.lpDirectory	= NULL;
				cmi.nShow		= SW_SHOWNORMAL;
				cmi.dwHotKey	= 0;
				cmi.hIcon	= NULL;
				cmi.lpVerb	=
						(LPCSTR)MAKEINTRESOURCE(*pdwEffect - es->baseindex);
				es->pcm->InvokeCommand(&cmi);
				*pdwEffect = DROPEFFECT_NONE;
				break;
			}
		}
		{
			struct ExtensionsStruct *es;

			es = (struct ExtensionsStruct *)thEsdata.bottom;
			while ( es->baseindex ){
				es->pcm->Release();
				es->pse->Release();
				es++;
			}
		}
		::ThFree(&thEsdata);
		if ( hFolderKey != NULL ) ::RegCloseKey(hFolderKey);
		if ( idl != NULL ) ::FreePIDL(idl);
	}
}

BOOL GetDropData(LPDATAOBJECT pDataObj, TMS_struct *files)
{
	STGMEDIUM medium;
	FORMATETC fmtetc;
	BOOL result;
											// ドロップデータを取得
	fmtetc.cfFormat = CF_HDROP;
	fmtetc.ptd      = NULL;
	fmtetc.dwAspect = DVASPECT_CONTENT;
	fmtetc.lindex   = -1;
	fmtetc.tymed    = TYMED_HGLOBAL;

	if ( FAILED(pDataObj->GetData(&fmtetc, &medium)) ) return FALSE;
	result = ::HdropdataToFiles(medium.hGlobal, files);
	::ReleaseStgMedium(&medium);
	return result;
}

void LinkDrop(HWND hWnd,TCHAR *sDrop,TCHAR *targetpath)
{
	while(*sDrop){
		TCHAR link[VFPS],*p,drv[VFPS];
		const TCHAR *u;

		u = FindLastEntryPoint(sDrop);
		if ( *u == '\0' ){
			tstrcpy(drv,sDrop);
			p = drv;
			while ( *p == '\\' ) p++;
			u = p;
			while (*p && (*p != '\\') && (*p != ':') ) p++;
			*p = '\0';
			if ( *u == '\0' ) u = T("link");
		}
				// ※ファイルエントリでないことが前提
		CatPath(link,targetpath,u);
		p = VFSFindLastEntry(link);
		p += FindExtSeparator(p);
		tstrcpy(p,StrShortcutExt);
		if ( FAILED(MakeShortCut(sDrop,link,targetpath)) ){
			XMessage(hWnd,NULL,XM_GrERRld,MES_ESCT);
			break;
		}
		sDrop += tstrlen(sDrop) + 1;
	}
}

void ListDrop(HWND hWnd,TCHAR *sDrop,TCHAR *targetpath,TCHAR *destpath)
{
	HANDLE hFile;
	DWORD size;

	ExistCheck(destpath,targetpath,T("ListFile.TXT"));
	hFile = CreateFileL(destpath,GENERIC_WRITE,0,NULL,CREATE_NEW,
				FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		PPErrorBox(hWnd,destpath,PPERROR_GETLASTERROR);
		return;
	}
	WriteFile(hFile,ListFileHeaderStr,ListFileHeaderStrLen,&size,NULL);

	while( *sDrop != '\0' ){
		WIN32_FIND_DATA ff;
		HANDLE hFF;

		hFF = FindFirstFileL(sDrop,&ff);
		if ( hFF != INVALID_HANDLE_VALUE ){
			FindClose(hFF);
			WriteFF(hFile,&ff,sDrop);
		}else{
			TCHAR buf[VFPS + 2];
			DWORD len;

			len = TSTROFF(wsprintf(buf,T("\"%s\"\r\n"),sDrop));
			WriteFile(hFile,buf,len,&size,NULL);
		}
		sDrop += tstrlen(sDrop) + 1;
	}

	::CloseHandle(hFile);
}

void GetSourcePath(LPDATAOBJECT pDataObj,TCHAR *SourcePath)
{
	FORMATETC fmtetc;
	STGMEDIUM medium;

	fmtetc.cfFormat = CF_HDROP;
	fmtetc.ptd      = NULL;
	fmtetc.dwAspect = DVASPECT_CONTENT;
	fmtetc.lindex   = -1;
	fmtetc.tymed    = TYMED_HGLOBAL;

	if ( SUCCEEDED(pDataObj->GetData(&fmtetc,&medium)) ){
		DROPFILES *pDrop;

		pDrop = (DROPFILES *)GlobalLock(medium.hGlobal);
			// Multibyte でも UNICODE でも char で取得。(ドライブ名は英字だから)
		if ( pDrop != NULL ){
			TCHAR *sDrop;

			sDrop = (TCHAR *)((char *)pDrop + pDrop->pFiles);
			if ( pDrop->fWide ){
			#ifdef UNICODE
				tstrlimcpy(SourcePath,sDrop,VFPS);
			#else
				UnicodeToAnsi((WCHAR *)sDrop,SourcePath,VFPS);
			#endif
			}else{
			#ifdef UNICODE
				AnsiToUnicode((char *)sDrop,SourcePath,VFPS);
			#else
				tstrlimcpy(SourcePath,sDrop,VFPS);
			#endif
			}
			*GetDriveRoot(SourcePath) = '\0';
			::GlobalUnlock(medium.hGlobal);
		}
		::ReleaseStgMedium(&medium);
	}
}

void GetDropFormat(DROPTYPE &DDtype,LPDATAOBJECT pDataObj)
{
	IEnumFORMATETC *IEF;

	DDtype.priority = 0;
	DDtype.hdrop = FALSE;
	DDtype.pidl = FALSE;
	DDtype.filecontents = FALSE;
#if USE_URLDROP
	DDtype.url  = FALSE;
#endif
	DDtype.text = FALSE;
	DDtype.etc  = FALSE;

										// クリップボードIDを取得
	CF_xSHELLIDLIST = ::RegisterClipboardFormat(CFSTR_SHELLIDLIST);
#if USE_URLDROP
	cfURL = ::RegisterClipboardFormat(CFSTR_SHELLURL);
#endif
	cfFileGroupDescriptorA =				// ↓何故か GROUP が抜けている
		::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA);
	cfFileGroupDescriptorW =				// ↓何故か GROUP が抜けている
		::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
	cfFileContents = ::RegisterClipboardFormat(CFSTR_FILECONTENTS);

#if 0
	HMODULE hUrlmon = NULL;
	IInternetSecurityManager *ISM = NULL;

	if ( hUrlmon == NULL ){
		hUrlmon = LoadLibrary(T("Urlmon.dll"));
		if ( hUrlmon == NULL ){
			hUrlmon = (HMODULE)-1;
		}else{
			GETDLLPROC(hUrlmon,CoInternetCreateSecurityManager);
			if ( DCoInternetCreateSecurityManager != NULL ){
				DCoInternetCreateSecurityManager(NULL,&ISM,0);
			}
		}
	}

	{
		cfUntrustedDragDrop = ::RegisterClipboardFormat(CFSTR_UNTRUSTEDDRAGDROP);

		FORMATETC fe;

		fe.cfFormat= static_cast<CLIPFORMAT>(cfUntrustedDragDrop);
		fe.ptd = NULL;
		fe.dwAspect = DVASPECT_CONTENT;
		fe.lindex = -1;
		fe.tymed = TYMED_HGLOBAL;
		XMessage(NULL,NULL,XM_DbgLOG,T("%x"),pDataObj->QueryGetData(&fe));

/*
		STGMEDIUM stm;
		FORMATETC fe;
		HGLOBAL hG = GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));
		if ( hG != NULL ){
			*(DWORD *)GlobalLock(hG) = MAX32;
			GlobalUnlock(hG);

			stm.tymed = TYMED_HGLOBAL;
			stm.hGlobal = hG;
			fe.cfFormat= (WORD)cfUntrustedDragDrop;
			fe.tymed = TYMED_HGLOBAL;
			fe.ptd = NULL;
			fe.lindex = -1;
			fe.dwAspect = DVASPECT_CONTENT;
			pDataObj->SetData(&fe,&stm,TRUE);
		}
*/
	}
#endif
	if ( SUCCEEDED(pDataObj->EnumFormatEtc(DATADIR_GET,&IEF)) ){
		FORMATETC fmt;
		DWORD getsi;

		IEF->Reset();
		while ( IEF->Next(1, &fmt, &getsi) == S_OK ){
			if ( getsi == 0 ) break;
			if ( fmt.cfFormat == CF_HDROP ){
				DDtype.hdrop = TRUE;
				if ( DDtype.priority == 0 ) DDtype.priority = CF_HDROP;
				continue;
			}
			if ( fmt.cfFormat == static_cast<CLIPFORMAT>(CF_xSHELLIDLIST) ){
				DDtype.pidl = TRUE;
				continue;
			}
			if ( fmt.cfFormat == static_cast<CLIPFORMAT>(cfFileContents) ){
				DDtype.filecontents = TRUE;
				if ( DDtype.priority == 0 ){
					DDtype.priority = cfFileContents;
				}
				continue;
			}
#if USE_URLDROP
			if ( fmt.cfFormat == static_cast<CLIPFORMAT>(cfURL) ){
				DDtype.url = TRUE;
				continue;
			}
#endif
			if ( fmt.cfFormat == CF_TEXT ){
				DDtype.text = TRUE;
				continue;
			}
			DDtype.etc = TRUE;
		}
		IEF->Release();
	}

}

/*-----------------------------------------------------------------------------
	IDropTarget interface(ドラッグ処理)

	WM_CREATE		RegisterDragDrop()
	WM_CLOSE		RevokeDragDrop()
	IUnknown		QueryInterface,AddRef,Release
	IDropTarget		DragEnter,DragOver,DragLeave,Drop
-----------------------------------------------------------------------------*/
//============================================================== コンストラクタ
CDropTarget::CDropTarget()
{
	mRef = 1;
	mShnDropTarget = NULL;
}
//==================================================================== IUnknown
STDMETHODIMP CDropTarget::QueryInterface(REFIID iid,void **ppv)
{
	// Explorer では、IMarshal,不明{0000001B-0000-0000-C000-000000000046}が実装されている
	if( (iid == IID_IUnknown) || (iid == IID_IDropTarget) ){
		*ppv = this;
		AddRef();
		return NOERROR;
	}
	if ( mShnDropTarget != NULL ){
		return mShnDropTarget->QueryInterface(iid,ppv);
	}
	*ppv = NULL;
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CDropTarget::AddRef(void)
{
	return ++mRef;
}

STDMETHODIMP_(ULONG) CDropTarget::Release(void)
{
	if ( --mRef == 0 ){
		delete this;
		return 0;
	}
	return mRef;
}

//========================================================= IDropTarget Methods
// 指定セルのフォーカス枠表示をする
void MarkFocusCell(PPC_APPINFO *cinfo,int cell,int edge)
{
	HDC hDC;
	int i,w;
	RECT box;
	BYTE *p;

	if ( !edge && X_poshl ){ // セルを選んだときは背景色変化で済ませる
		RefleshCell(cinfo,cell);
		UpdateWindow(cinfo->info.hWnd); // 背景色を変化
		return;
	}
	hDC = ::GetDC(cinfo->info.hWnd);

#if FREEPOSMODE
	if ( CEL(cell).pos.x == NOFREEPOS ){
#endif
		box.left  = CalcCellX(cinfo,cell - cinfo->cellWMin);
		box.top   = CalcCellY(cinfo,cell - cinfo->cellWMin);
#if FREEPOSMODE
	}else{
		box.left  = CEL(cell).pos.x - CalcFreePosOffX(cinfo);
		box.top   = CEL(cell).pos.y - CalcFreePosOffY(cinfo);
		edge = 0;
	}
#endif

	box.right = box.left + cinfo->cel.Size.cx - cinfo->fontX;
	if ( !edge ){
		box.bottom = box.top + cinfo->cel.Size.cy;
		w = 3;
	}else{
		box.bottom = box.top + 3;
		w = 2;
	}

	p = cinfo->celF.fmt;
	if ( (*p == DE_WIDEV) || (*p == DE_WIDEW) ) p += DE_WIDEV_SIZE - 1;
	if ( (*p == DE_ICON) || (*p == DE_CHECK) || (*p == DE_CHECKBOX) ){
		box.left += cinfo->fontX * 2;
	}else if ( *p == DE_ICON2 ){
		box.left += *(p + 1) + ICONBLANK;
	}else if ( *p == DE_MARK ){
		box.left += cinfo->fontX;
	}

	for ( i = 0 ; i < w ; i++ ){
		::DrawFocusRect(hDC,&box);
		box.left++;
		box.top++;
		box.right--;
		box.bottom--;
	}
/*
	if ( !edge && (cell == cinfo->e.cellN) && cinfo->iconR ){
		box.left  = cinfo->BoxInfo.left;
		box.top   = cinfo->BoxInfo.top;
		box.right = box.left + cinfo->iconR;
		box.bottom = cinfo->BoxInfo.bottom - 1;
		for ( i = 0 ; i < 3 ; i++){
			DrawFocusRect(hDC,&box);
			box.left++;
			box.top++;
			box.right--;
			box.bottom--;
		}
	}
*/
	::ReleaseDC(cinfo->info.hWnd,hDC);
}

/*-----------------------------------------------------------------------------
	DragEnter		マウスが自分のウィンドウに入った時に呼ばれる

		DWORD grfKeyState		キーの状態(MK_CONTROL,MK_SHIFT,MK_ALT,
										   MK_BUTTON,MK_LBUTTON,MK_MBUTTON,
										   MK_RBUTTON)
		POINT pt				マウスの現在位置
		LPDWORD pdwEffect		動作の指定（変更可）
-----------------------------------------------------------------------------*/
STDMETHODIMP CDropTarget::DragEnter(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect)
{
	HRESULT result;
/*
	UINT cfUntrustedDragDrop = ::RegisterClipboardFormat(CFSTR_UNTRUSTEDDRAGDROP);
	FORMATETC fe;
			fe.cfFormat= (WORD)cfUntrustedDragDrop;
			fe.ptd = NULL;
			fe.dwAspect = DVASPECT_CONTENT;
			fe.lindex = -1;
			fe.tymed = TYMED_HGLOBAL;
			XMessage(NULL,NULL,XM_DbgLOG,T("%x"),pDataObj->QueryGetData(&fe));
*/
	GetDropFormat(mDDtype, pDataObj);
	GetSourcePath(pDataObj, mSourcePath);
										// ドロップ可能かの確認
	mCell = DDCELL_BADPOSITION;
	mTitem = 0;
	mDefAction = DROPEFFECT_NONE;
	mDropModeFlags = 0;
#if FREEPOSMODE
	mCellPos.x = NOFREEPOS;
#endif
	result = QueryDrop(grfKeyState, &pt, pdwEffect);

	if ( IsNodirShnPath(mCinfo) ){ // SHN 形式
		mShnDropTarget = (IDropTarget *)GetPathInterface(mCinfo->info.hWnd, mCinfo->path, IID_IDropTarget, NULL);
		if ( mShnDropTarget != NULL ){
			return mShnDropTarget->DragEnter(pDataObj, grfKeyState, pt, pdwEffect);
		}
	}
	return result;
}

/*-----------------------------------------------------------------------------
	DragOver		マウスが自分のウィンドウを動いた時に呼ばれる

		DWORD grfKeyState		キーの状態(MK_CONTROL,MK_SHIFT,MK_ALT,
										   MK_BUTTON,MK_LBUTTON,MK_MBUTTON,
										   MK_RBUTTON)
		POINT pt				マウスの現在位置
		LPDWORD pdwEffect		動作の指定（変更可）
-----------------------------------------------------------------------------*/
STDMETHODIMP CDropTarget::DragOver(DWORD grfKeyState,POINTL pt,LPDWORD pdwEffect)
{
	POINT pos;
										// スクロール処理
	pos.x = pt.x;
	pos.y = pt.y;
	::ScreenToClient(mCinfo->info.hWnd,&pos);
	SetDDScroll(mCinfo,&pos);

	if ( mShnDropTarget != NULL ){
		return mShnDropTarget->DragOver(grfKeyState,pt,pdwEffect);
	}

	mKeyState = grfKeyState;
	return QueryDrop(grfKeyState,&pt,pdwEffect);
}

/*-----------------------------------------------------------------------------
	DragLeave		マウスが自分のウィンドウを離れた時に呼ばれる
-----------------------------------------------------------------------------*/
STDMETHODIMP CDropTarget::DragLeave(void)
{
	if ( mCell >= 0 ){
		mCinfo->e.cellPoint = -1;
		MarkFocusCell(mCinfo,mCell,mEdge); // 消去
	}
	SetDDScroll(mCinfo,NULL);
	if ( mCinfo->hTreeWnd != NULL ){
		::SendMessage(mCinfo->hTreeWnd,VTM_POINTPATH,0,(LPARAM)NULL);
	}

	mCell = DDCELL_BADPOSITION;
	mTitem = 0;

	if ( mShnDropTarget != NULL ){
		mShnDropTarget->DragLeave();
		mShnDropTarget->Release();
		mShnDropTarget = NULL;
	}

	return S_OK;
}
/*-----------------------------------------------------------------------------
	Drop			ドロップ処理

		DWORD grfKeyState		キーの状態(MK_CONTROL,MK_SHIFT,MK_ALT,
										   MK_BUTTON,MK_LBUTTON,MK_MBUTTON,
										   MK_RBUTTON)
		POINT pt				マウスの現在位置
		LPDWORD pdwEffect		動作の指定（変更可）
-----------------------------------------------------------------------------*/
BOOL CDropTarget::DropDDFormat(LPDATAOBJECT pDataObj,POINTL *pt,DWORD pdwEffect)
{
	TCHAR *sDrop,buf[MAXCMDLINESIZE];
	int mode;
	TMS_struct files = {{NULL,0,NULL},0};

	if ( GetDropData(pDataObj,&files) == FALSE ) return FALSE;
	sDrop = (TCHAR *)files.tm.p;

	if ( VFSGetDriveType(mTargetPath,&mode,NULL) != NULL ){
		if ( (mode <= VFSPT_SHN_DESK) || (mode <= VFSPT_UNC) ){
			VFSGetRealPath(NULL,mTargetPath,mTargetPath);
		}
	}
	switch( pdwEffect ){
		case DROPEFFECT_ADDTOOLBAR:
			AddToolBarButton(mCinfo->info.hWnd,sDrop,mToolBarName);
			break;

		case DROPEFFECT_ADDTREEITEM:
			::SendMessage(mCinfo->hTreeWnd,VTM_ADDTREEITEM,0,(LPARAM)sDrop);
			break;

		case DROPEFFECT_MAKELIST: {
			TCHAR destpath[VFPS];

			ListDrop(mCinfo->info.hWnd,sDrop,mTargetPath,destpath);
			tstrcpy(mCinfo->Jfname,FindLastEntryPoint(destpath));
			::read_entry(mCinfo,RENTRY_JUMPNAME | RENTRY_NOHIST);
			break;
		}
//=============================================================================
		case DROPEFFECT_LINK:
			LinkDrop(mCinfo->info.hWnd,sDrop,mTargetPath);
			if ( mCell != DDCELL_CONTROL ){
				::read_entry(mCinfo,RENTRY_SAVEOFF | RENTRY_NOHIST);
			}else{
				if ( mCinfo->hTreeWnd != NULL ){
					::SendMessage(mCinfo->hTreeWnd,VTM_SETPATH,0,(LPARAM)mTargetPath);
				}
			}
			break;
//=============================================================================
		case DROPEFFECT_JUMP: {
			TCHAR *uptr, *p;

			uptr = FindLastEntryPoint(sDrop);
			if ( *uptr == '\0' ){
				tstrcpy(buf, sDrop);
				p = buf;
				while ( *p == '\\' ) p++;
				uptr = p;
				while (*p && (*p != '\\') && (*p != ':') ) p++;
				*p = '\0';
				if ( *uptr == '\0' ) uptr = StrLinkJump;
			}else{
				*(uptr - 1) = '\0';
			}
			tstrcpy(mCinfo->path, sDrop);
			tstrcpy(mCinfo->Jfname, uptr);
			::read_entry(mCinfo,RENTRY_JUMPNAME | RENTRY_NOHIST);
			sDrop += tstrlen(sDrop) + 1;
			if ( !tstrcmp(sDrop, uptr) ) sDrop += tstrlen(sDrop) + 1;
			if ( *sDrop ){
				mCinfo->MarkMask = 0x1f;
				CellMark(mCinfo,mCinfo->e.cellN,MARK_CHECK);
				while ( *sDrop ){
					uptr = FindLastEntryPoint(sDrop);
					if ( IsTrue(FindCell(mCinfo, uptr)) ){
						CellMark(mCinfo,mCinfo->e.cellN,MARK_CHECK);
					}
					sDrop += tstrlen(sDrop) + 1;
				}
				RefleshCell(mCinfo,mCinfo->e.cellN);
			}
			break;
		}
//=============================================================================
		case DROPEFFECT_SWAP: {	// swap
			int c;
			POINT pos;

			pos.x = pt->x;
			pos.y = pt->y;
			::ScreenToClient(mCinfo->info.hWnd,&pos);
			if ( GetItemTypeFromPoint(mCinfo,&pos,&c) == PPCR_CELLTEXT ){
				int cell,maxc;

				maxc = mCinfo->e.cellIMax + mCinfo->e.cellStack - 1;
				cell = mCELt(mCinfo->e.cellN);
				if ( mCinfo->e.cellN < maxc ){
					memmove(&mCELt(mCinfo->e.cellN),&mCELt(mCinfo->e.cellN+1),
							sizeof(ENTRYINDEX) * (maxc - mCinfo->e.cellN));
				}
				mCELt(maxc) = cell;
				mCinfo->e.cellStack++;
				mCinfo->e.cellIMax--;
				c--;
				maxc = mCinfo->e.cellIMax + mCinfo->e.cellStack - 1;
				cell = mCELt(maxc);
				if ( c < maxc ){
					memmove( &mCELt(c+2),&mCELt(c + 1),
							sizeof(ENTRYINDEX) * (maxc - c - 1));
				}
				mCELt(c + 1) = cell;
				mCinfo->e.cellIMax++;
				mCinfo->e.cellStack--;
				::InvalidateRect(mCinfo->info.hWnd,NULL,TRUE);
			}
			break;
		}
//=============================================================================
		case DROPEFFECT_DUP:
			::ExecFileOperation(pdwEffect,mTargetPath,&files,mCinfo,DupOptionStr);
			::read_entry(mCinfo,RENTRY_UPDATE | RENTRY_SAVEOFF);
			return TRUE;
//=============================================================================
		case DROPEFFECT_OPENWITH: {
			TCHAR *p;

			buf[0] = ' ';
			p = VFSFullPath(buf + 1,mCEL(mCell).f.cFileName,mTargetPath);
			if ( p != NULL ){
				p += tstrlen(p);
				if ( tstrchr(buf + 1,' ') != NULL ){
					buf[0] = '\"';
					*p++ = '\"';
				}

				while ( *sDrop ){
					TCHAR *space;
					DWORD len;

					len = tstrlen32(sDrop);
					if ( (p + len + 4) > (buf + TSIZEOF(buf)) ){
						XMessage(mCinfo->info.hWnd,NULL,XM_GrERRld,T("parameter too many"));
						break;
					}
					*p++ = ' ';
					space = tstrchr(sDrop,' ');
					if ( space != NULL ) *p++ = '\"';
					tstrcpy(p,sDrop);
					p += len;
					if ( space != NULL ) *p++ = '\"';
					sDrop += len + 1;
				}
				*p = '\0';
			}
			ComExec(mCinfo->info.hWnd,buf,mTargetPath);
			break;
		}
//=============================================================================
		case DROPEFFECT_COPY:
			if ( mDropModeFlags & DROPMODE_FILEFOLDER ){
				VFSFullPath(mTargetPath,(TCHAR *)GetCellFileName(mCinfo,&mCEL(mCell),mTargetPath),mCinfo->path);
			}
		// default へ
		default:
			::ExecFileOperation(pdwEffect,mTargetPath,&files,mCinfo,NULL);
			if ( mCell == DDCELL_CONTROL ){
				if ( mCinfo->hTreeWnd != NULL ){
					::SendMessage(mCinfo->hTreeWnd,
							VTM_SETPATH,0,(LPARAM)mTargetPath);
				}
			}
			return TRUE;
//=============================================================================
	}
	TMS_kill(&files);
	return TRUE;
}

BOOL CDropTarget::OpenFGD(FGDSTRUCT *FGD,LPDATAOBJECT pDataObj)
{
	FGD->uFGD.w = (FILEGROUPDESCRIPTORW *)GetGlobalData(pDataObj,
			cfFileGroupDescriptorW,&FGD->medium);
	if ( FGD->uFGD.w != NULL ){
		FGD->unicode = TRUE;
		return TRUE;
	}
	FGD->uFGD.a = (FILEGROUPDESCRIPTORA *)GetGlobalData(pDataObj,
			cfFileGroupDescriptorA,&FGD->medium);
	if ( FGD->uFGD.a != NULL ){
		FGD->unicode = FALSE;
		return TRUE;
	}
	return FALSE;
}

void CDropTarget::CloseFGD(FGDSTRUCT *FGD)
{
	::GlobalUnlock(FGD->medium.hGlobal);
	::ReleaseStgMedium(&FGD->medium);
}

void CDropTarget::GetFGDFileName(FGDSTRUCT *FGD,int index,TCHAR *dest,const TCHAR *path)
{
	if ( FGD->unicode ){
		#ifdef UNICODE
			ExistCheck(dest,path,FGD->uFGD.w->fgd[index].cFileName);
		#else
			char temp[VFPS];

			UnicodeToAnsi(FGD->uFGD.w->fgd[index].cFileName,temp,sizeof temp);
			ExistCheck(dest,path,temp);
		#endif
	}else{
		#ifdef UNICODE
			WCHAR temp[VFPS];

			AnsiToUnicode(FGD->uFGD.a->fgd[index].cFileName,temp,TSIZEOF(temp));
			ExistCheck(dest,path,temp);
		#else
			ExistCheck(dest,path,FGD->uFGD.a->fgd[index].cFileName);
		#endif
	}
}

#define GetFGDcnt(FGD) ( (FGD.unicode == FALSE) ? FGD.uFGD.a->cItems : FGD.uFGD.w->cItems)
#define GetFGD(FGD,index,item) ( (FGD.unicode == FALSE) ? FGD.uFGD.a->fgd[index].item : FGD.uFGD.w->fgd[index].item)

void WriteMedium(STGMEDIUM *medium,HANDLE hFile)
{
	DWORD size;

	if ( medium->tymed & TYMED_HGLOBAL ){
		char *dumpdata;

		size = ToSIZE32_T(GlobalSize(medium->hGlobal));
		dumpdata = static_cast<char *>(::GlobalLock(medium->hGlobal));
		if ( dumpdata != NULL ){
			::WriteFile(hFile,dumpdata,size,&size,NULL);
			::GlobalUnlock(medium->hGlobal);
		}
		return;
	}
	if ( medium->tymed & TYMED_ISTREAM ){
		char buf[0x1000];

		for ( ; ; ){
			size = 0;
			medium->pstm->Read(&buf,sizeof(buf),&size);
			if ( size == 0 ) break;
			::WriteFile(hFile,buf,size,&size,NULL);
		}
		medium->pstm->Release();
		return;
	}
	XMessage(NULL,NULL,XM_GrERRld,T("tymed error"));
}

#ifndef FD_WRITESTIME
#define FD_WRITESTIME 0x20
#endif

BOOL CDropTarget::DropEtcFormat(LPDATAOBJECT pDataObj,POINTL *pt,DWORD pdwEffect)
{
	FORMATETC fmtetc;
	STGMEDIUM medium;

	fmtetc.dwAspect = DVASPECT_CONTENT;
	fmtetc.lindex   = -1;
	fmtetc.ptd      = NULL;

	switch ( pdwEffect ){
		case DROPEFFECT_FILECONTENTS:
			FGDSTRUCT FGD;
			int maxfmt;

			if ( FALSE == OpenFGD(&FGD,pDataObj) ){
				SetPopMsg(mCinfo,POPMSG_MSG,StrDDrecvError);
				return FALSE;
			}

			fmtetc.cfFormat = static_cast<CLIPFORMAT>(cfFileContents);
			fmtetc.tymed    = TYMED_HGLOBAL | TYMED_ISTREAM;
			maxfmt = GetFGDcnt(FGD);

			for ( fmtetc.lindex = 0 ; fmtetc.lindex < maxfmt ; fmtetc.lindex++ ){
				HRESULT result = pDataObj->GetData(&fmtetc,&medium);
				if ( FAILED(result) ){
					PPErrorBox(NULL,NULL,result);
					continue;
				}
				HANDLE hFile;
				TCHAR name[VFPS];

				GetFGDFileName(&FGD,fmtetc.lindex,name,mTargetPath);

				hFile = CreateFileL(name,GENERIC_WRITE,0,NULL,CREATE_NEW,
						FILE_FLAG_SEQUENTIAL_SCAN,NULL);
				if ( hFile != INVALID_HANDLE_VALUE ){
					WriteMedium(&medium,hFile);
					if ( GetFGD(FGD,fmtetc.lindex,dwFlags) & FD_WRITESTIME ){
						::SetFileTime(hFile,NULL,NULL,
								&GetFGD(FGD,fmtetc.lindex,ftLastWriteTime));
					}
					::CloseHandle(hFile);
				}else{
					PPErrorBox(NULL,name,PPERROR_GETLASTERROR);
				}
				::ReleaseStgMedium(&medium);
			}
			CloseFGD(&FGD);
			return TRUE;

		case DROPEFFECT_TEXT:
			fmtetc.cfFormat = CF_TEXT;
			break;

		case DROPEFFECT_ETC: {
			HMENU hMenu = CreatePopupMenu();

			IEnumFORMATETC *IEF;
			if ( SUCCEEDED(pDataObj->EnumFormatEtc(DATADIR_GET,&IEF)) ){
				FORMATETC fmt;
				DWORD getsi;

				IEF->Reset();
				while ( IEF->Next(1,&fmt,&getsi) == S_OK ){
					TCHAR type[300];

					if ( getsi == 0 ) break;
					GetClipboardTypeName(type,fmt.cfFormat);

					::AppendMenuString(hMenu,fmt.cfFormat,type);
				}
				IEF->Release();
			}

			::AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
			::AppendMenuString(hMenu,0,MES_CCAN);
			fmtetc.cfFormat = static_cast<WORD>(TrackPopupMenu(hMenu,
					TPM_TDEFAULT,pt->x,pt->y,0,mCinfo->info.hWnd,NULL));
			::DestroyMenu(hMenu);
			if ( fmtetc.cfFormat == 0 ) return FALSE;
			if ( fmtetc.cfFormat == static_cast<CLIPFORMAT>(cfFileContents) ){
				return DropEtcFormat(pDataObj,pt,DROPEFFECT_FILECONTENTS);
			}
			break;
		}
	}
	fmtetc.tymed    = TYMED_HGLOBAL;
	if ( SUCCEEDED(pDataObj->GetData(&fmtetc, &medium)) ){
		SaveClipboardData(medium.hGlobal,fmtetc.cfFormat,mCinfo);
		::ReleaseStgMedium(&medium);
	}else{
		SetPopMsg(mCinfo,POPMSG_MSG,StrDDrecvError);
	}
	return TRUE;
}

void *CDropTarget::GetGlobalData(LPDATAOBJECT pDataObj,UINT format,STGMEDIUM *medium)
{
	FORMATETC fmtetc;

	fmtetc.cfFormat = (WORD)format;
	fmtetc.ptd      = NULL;
	fmtetc.dwAspect = DVASPECT_CONTENT;
	fmtetc.lindex   = -1;
	fmtetc.tymed    = TYMED_HGLOBAL;
	if ( FAILED(pDataObj->GetData(&fmtetc,medium)) ) return NULL;
	return ::GlobalLock(medium->hGlobal);
}

#if USE_URLDROP
STDMETHODIMP CDropTarget::DropUrl(LPDATAOBJECT pDataObj)
{
	FGDSTRUCT FGD;
	STGMEDIUM medium;
	TCHAR name[VFPS];
	char *cp;
	HANDLE hFile;
	DWORD size;

#define URLLINKTAG "[InternetShortcut]\r\nURL="

	if ( FALSE == OpenFGD(&FGD,pDataObj) ){
		ExistCheck(name,mTargetPath,T("url.url"));
	}else{
		GetFGDFileName(&FGD,0,name,mTargetPath);
		CloseFGD(&FGD);
	}
	cp = (char *)GetGlobalData(pDataObj,cfURL,&medium);
	if ( cp == NULL ) return E_INVALIDARG;
	hFile = CreateFileL(name,GENERIC_WRITE,0,NULL,
			CREATE_NEW,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		::WriteFile(hFile,URLLINKTAG,sizeof(URLLINKTAG) - 1,&size,NULL);
		::WriteFile(hFile,cp,strlen(cp),&size,NULL);
		::WriteFile(hFile,"\r\n",2,&size,NULL);
		::CloseHandle(hFile);
	}else{
		PPErrorBox(NULL,name,PPERROR_GETLASTERROR);
	}
	::GlobalUnlock(medium.hGlobal);
	::ReleaseStgMedium(&medium);

	return S_OK;
}
#endif

STDMETHODIMP CDropTarget::DropPidlLink(LPDATAOBJECT pDataObj,DWORD pdwEffect)
{
	STGMEDIUM medium;
	DWORD count,i;
	LPITEMIDLIST pRootPidl;
	LPITEMIDLIST pItemPidl;
	IShellLink *psl;
	LPSHELLFOLDER pSF,pRoot;
	HRESULT hres;
	TCHAR namepath[VFPS];
	BOOL UseRootPIDL;

	if ( FAILED(::SHGetDesktopFolder(&pSF)) ) return E_INVALIDARG;

	LPIDA pIdA = (LPIDA)GetGlobalData(pDataObj,CF_xSHELLIDLIST,&medium);
	if ( pIdA == NULL ) return E_INVALIDARG;

	count = pIdA->cidl;

	pRootPidl = (LPITEMIDLIST)((BYTE *)(pIdA) + pIdA->aoffset[0]);

	if ( pdwEffect == DROPEFFECT_JUMP ){
		::GetIDLSub(mCinfo->path,pSF,pRootPidl);
		::read_entry(mCinfo,RENTRY_NOHIST);
	}

	if ( SUCCEEDED(pSF->BindToObject(
			pRootPidl,NULL,IID_IShellFolder,(LPVOID *)&pRoot)) ){
		pSF->Release();
		UseRootPIDL = TRUE;
	}else{
		UseRootPIDL = FALSE;
		pRoot = pSF;
	}

	for ( i = 0 ; i < count ; i++ ){
		TCHAR *p;

		pItemPidl = (LPITEMIDLIST)((BYTE *)(pIdA) + pIdA->aoffset[i + 1]);
		// 名前を取得
		if ( FALSE == PIDL2DisplayNameOf(namepath,pRoot,pItemPidl) ){
			::wsprintf(namepath,T("%d"),i);
		}
		while ( NULL != (p = tstrchr(namepath,':')) ) *p = '_';

		if ( pdwEffect == DROPEFFECT_JUMP ){
			if ( IsTrue(FindCell(mCinfo,namepath)) && (count > 1) ){
				mCinfo->MarkMask = 0x1f;
				CellMark(mCinfo,mCinfo->e.cellN,MARK_CHECK);
			}
		}else{
										// IShellLink interface を取得
			hres = ::CoCreateInstance(CLSID_ShellLink,NULL,
					CLSCTX_INPROC_SERVER,IID_IShellLink,(LPVOID *)&psl);
			if ( SUCCEEDED(hres) ){
				IPersistFile *ppf;
									// IShellLink から IPersistFile interface
				hres = psl->QueryInterface(IID_IPersistFile,(LPVOID *)&ppf);
				if ( SUCCEEDED(hres) ){
					tstrcat(namepath,StrShortcutExt);
					ExistCheck(namepath,mTargetPath,namepath);

					if ( IsTrue(UseRootPIDL) ){
						LPMALLOC pMA;
						UINT cb1, cb2;
						LPITEMIDLIST idlNew;

						SHGetMalloc(&pMA);
						cb1 = GetPidlSize(pRootPidl) - (sizeof(BYTE) * 2);
						cb2 = GetPidlSize(pItemPidl);
						idlNew = (LPITEMIDLIST)pMA->Alloc(cb1 + cb2);
						if ( idlNew != NULL ){
							memcpy(idlNew,pRootPidl,cb1);
							memcpy( ((BYTE *)idlNew) + cb1,pItemPidl,cb2);
							psl->SetIDList(idlNew);
							pMA->Free(idlNew);
						}
						pMA->Release();
					}else{
						psl->SetIDList(pItemPidl);
					}
#ifdef UNICODE
					ppf->Save(namepath,TRUE);
#else
					{
						WCHAR wsz[VFPS];

						AnsiToUnicode(namepath,wsz,VFPS);
						ppf->Save(wsz,TRUE);
					}
#endif
					ppf->Release();
				}
				psl->Release();
			}
		}
	}
	pRoot->Release();
	return S_OK;
}

STDMETHODIMP CDropTarget::Drop(LPDATAOBJECT pDataObj,DWORD grfKeyState,POINTL pt,LPDWORD pdwEffect)
{
	DWORD orgeffect = *pdwEffect;
	SetDDScroll(mCinfo,NULL);

	#if 0
	{
		STGMEDIUM stm;
		FORMATETC fe = {cfUntrustedDragDrop,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
		if ( SUCCEEDED(pDataObj->GetData(&fe,&stm)) ){
			IInternetSecurityManager *pSecurityMgr;
			if ( SUCCEEDED(CoCreateInstance(CLSID_InternetSecurityManager,NULL, CLSCTX_INPROC_SERVER,IID_IInternetSecurityManager,(void**)&pSecurityMgr)) ){
			#define URLPOLICY_ALLOW 0x00
			#define PUAF_ENFORCERESTRICTED 0x00000100
			#define PUAF_DEFAULT 0x00000000
			#define URLACTION_SHELL_ENHANCED_DRAGDROP_SECURITY 0x0000180B

			DWORD dwPolicy = URLPOLICY_ALLOW;
			DWORD aa = /* stm.hGlobal */ URLACTION_SHELL_ENHANCED_DRAGDROP_SECURITY;
			if ( SUCCEEDED(pSecurityMgr->ProcessUrlAction(mTargetPath,aa,reinterpret_cast<BYTE*>(&dwPolicy),sizeof(dwPolicy),NULL,0,PUAF_ENFORCERESTRICTED,NULL))){

				::ReleaseStgMedium(&stm);
			}
		}
	}
	#endif

	if ( mShnDropTarget != NULL ){
		HRESULT result = mShnDropTarget->Drop(pDataObj,grfKeyState,pt,pdwEffect);
		mShnDropTarget->Release();
		mShnDropTarget = NULL;
		if ( SUCCEEDED(result) ){
			PostMessage(mCinfo->info.hWnd,WM_PPXCOMMAND,K_c | K_F5,0);
		}
		return result;
	}

	if ( QueryDrop(mKeyState,&pt,pdwEffect) == S_OK ){
		// 一体化時に非アクティブPPcからD&Dするとメッセージループが
		// 回らなくなるための対策
		if ( ::SelfDD_hWnd != NULL ){
			::SendMessage(::SelfDD_hWnd,WM_PPCSETFOCUS,0,0);
		}

		::DDMenu(mCinfo->info.hWnd, &pt, pdwEffect, orgeffect, mKeyState, pDataObj, mCell, mDDtype, mCinfo, mTargetPath, mDropModeFlags);

		if ( (mCinfo->hTreeWnd != NULL) && (*pdwEffect != DROPEFFECT_ADDTREEITEM) ){
			::SendMessage(mCinfo->hTreeWnd,VTM_POINTPATH,0,(LPARAM)NULL);
		}

		if ( mCell >= 0 ){ // MarkFocusCell(mCinfo,mCell,mEdge); の代わりに消去
			mCinfo->e.cellPoint = -1;
			 ::InvalidateRect(mCinfo->info.hWnd,NULL,TRUE);
		}
		if ( (*pdwEffect > DROPEFFECT_OBJECTSTART) &&
			 (*pdwEffect <= DROPEFFECT_OBJECTEND) ){
			::SetForegroundWindow(mCinfo->info.hWnd);

			if ( IsTrue(DropEtcFormat(pDataObj, &pt, *pdwEffect)) ){
				return S_OK;
			}else{
				*pdwEffect = DROPEFFECT_NONE;
				return E_INVALIDARG;
			}
		}
		if ( *pdwEffect != DROPEFFECT_NONE ){
			::SetForegroundWindow(mCinfo->info.hWnd);

			if ( IsTrue(mDDtype.hdrop) ){
				if ( IsTrue(DropDDFormat(pDataObj, &pt, *pdwEffect)) ){
					return S_OK;
				}
			}
			if ( mDDtype.pidl ) return DropPidlLink(pDataObj,*pdwEffect);
#if USE_URLDROP
			if ( mDDtype.url ) return DropUrl(pDataObj);
#endif
			if ( mDDtype.filecontents && DropEtcFormat(pDataObj, &pt, DROPEFFECT_FILECONTENTS) ){
				return S_OK;
			}
			SetPopMsg(mCinfo, POPMSG_MSG, StrDDrecvError);
			return S_OK;
		}
	}else{
		if ( mCell >= 0 ) ::InvalidateRect(mCinfo->info.hWnd,NULL,TRUE);
	}
	*pdwEffect = DROPEFFECT_NONE;
	return E_INVALIDARG;
}

//==================================================================== 内部処理
/* このままだと書庫内一覧を作成することがあるので保留
BOOL CheckFileFolder(PPC_APPINFO *cinfo,ENTRYINDEX cellno)
{
	HANDLE hFF;
	TCHAR path[VFPS];
	WIN32_FIND_DATA ff;

	VFSFullPath(path + 1,(TCHAR *)GetCellFileName(cinfo,&CEL(cellno),path),cinfo->path);
	CatPath(NULL,path,T("*"));
	path[0] = '+';
	hFF = VFSFindFirst(path,&ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		VFSFindClose(hFF);
		return TRUE;
	}
	return FALSE;
}
*/
/*-----------------------------------------------------------------------------
	QueryDrop			ドロップができるか判定する

		DWORD grfKeyState		キーの状態(MK_CONTROL,MK_SHIFT,MK_ALT,
										   MK_BUTTON,MK_LBUTTON,MK_MBUTTON,
										   MK_RBUTTON)
		POINT pt				マウスの現在位置(スクリーン座標)
		LPDWORD pdwEffect		動作の指定（変更可）
-----------------------------------------------------------------------------*/
STDMETHODIMP CDropTarget::QueryDrop(DWORD grfKeyState, POINTL *pt, DWORD *pdwEffect)
{
	HWND hTargetWnd, hTargetWnd2;
	POINT cpos;
	DWORD NewDropModeFlags = 0;

	cpos.x = pt->x;
	cpos.y = pt->y;
	hTargetWnd = ::WindowFromPoint(cpos);
	hTargetWnd2 = ::GetParent(hTargetWnd);
	if ( hTargetWnd == NULL ) hTargetWnd = BADHWND;
	if ( hTargetWnd2 == NULL ) hTargetWnd2 = BADHWND;

	if ( hTargetWnd2 == mCinfo->docks.t.hWnd ){
		return QueryDrop_Dock(grfKeyState, pdwEffect, hTargetWnd, &cpos, &mCinfo->docks.t);
	}

	if ( hTargetWnd2 == mCinfo->docks.b.hWnd ){
		return QueryDrop_Dock(grfKeyState, pdwEffect, hTargetWnd, &cpos, &mCinfo->docks.b);
	}

	if ( hTargetWnd == mCinfo->hToolBarWnd ){
		mToolBarName = T("B_cdef");
		return QueryDrop_ToolBar(grfKeyState, pdwEffect, hTargetWnd, &cpos, &mCinfo->thGuiWork);
	}

	if ( (hTargetWnd == mCinfo->hTreeWnd) ||
		 (hTargetWnd2 == mCinfo->hTreeWnd) ){	// ツリーウィンドウ =======
		if ( IsTrue(QueryDrop_Tree(mCinfo->hTreeWnd, pt, pdwEffect)) ) return S_OK;
												// メインウィンドウ ===========
	}else{
		int celltype, edge;			// ポイント対象を求める
		ENTRYINDEX cellno;

		::ScreenToClient(mCinfo->info.hWnd, &cpos);
		celltype = GetItemTypeFromPoint(mCinfo, &cpos, &cellno);
#if FREEPOSMODE
		{ // 移動中cellを表示
			RECT box;
			HDC hDC;
			hDC = ::GetDC(mCinfo->info.hWnd);
			if ( mCellPos.x != NOFREEPOS ){
				box.left = mCellPos.x;
				box.top = mCellPos.y;
				box.right = box.left + mCinfo->cel.Size.x;
				box.bottom = box.top + mCinfo->cel.Size.y;
				DrawFocusRect(hDC,&box);
			}
			box.left = mCellPos.x = cpos.x;
			box.top = mCellPos.y = cpos.y;
			box.right = box.left + mCinfo->cel.Size.x;
			box.bottom = box.top + mCinfo->cel.Size.y;
			DrawFocusRect(hDC,&box);
			::ReleaseDC(mCinfo->info.hWnd, hDC);

		}
#endif
		if ( !((celltype == PPCR_CELLTEXT) || (celltype == PPCR_INFOICON)) ){
			cellno = -1;
		}
		if ( cellno >= 0 ){
			if ( mCEL(cellno).state < ECS_NORMAL ) cellno = -1;
		}
		if ( ::SelfDD_hWnd != mCinfo->info.hWnd ){
			edge = 0;
		}else{
			edge = (celltype == PPCR_CELLTEXT) && (((cpos.y -
					mCinfo->BoxEntries.top) % mCinfo->cel.Size.cy) < 3);
		}
													// デフォルト動作を決める
		if ( (cellno != mCell) || (edge != mEdge) ){
			TCHAR *targetdir;

			if ( mCell >= 0 ){
				mCinfo->e.cellPoint = -1;
				MarkFocusCell(mCinfo, mCell, mEdge); // 消去
			}
			mCell = cellno;
			targetdir = (mCinfo->e.Dtype.mode != VFSDT_LFILE) ?
					mCinfo->RealPath : mCinfo->path;
			if ( cellno < 0 ){ // セル以外
				tstrcpy(mTargetPath, targetdir);
				SetDirectoryDefAction();
			}else{ // セルを指している
				mEdge = edge;
				if ( edge ) setflag(NewDropModeFlags, DROPMODE_EDGE);

				mCinfo->e.cellPoint = cellno;
				mCinfo->e.cellPointType = PPCR_CELLTEXT;
				MarkFocusCell(mCinfo, cellno, edge); // 表示
				if ( mCEL(cellno).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					VFSFullPath(mTargetPath, mCEL(mCell).f.cFileName, targetdir);
					SetDirectoryDefAction();
				}else{
					tstrcpy(mTargetPath, targetdir);
					mDefAction = DROPEFFECT_LINK;
/*
					if ( IsTrue(CheckFileFolder(mCinfo, cellno)) ){
						setflag(NewDropModeFlags, DROPMODE_FILEFOLDER); // 開発中
						mDefAction = DROPEFFECT_COPY;
					}
*/
				}
			}
			mDropModeFlags = NewDropModeFlags;
		}
	}
	if ( ::GetDDEffect( (mCell >= 0) ?
				mCEL(mCell).f.dwFileAttributes : FILE_ATTRIBUTE_DIRECTORY,
			grfKeyState, pdwEffect,mDefAction) ){
		return S_OK;
	}
	*pdwEffect = ( *pdwEffect & (DROPEFFECT_MOVE | DROPEFFECT_COPY) ) | DROPEFFECT_LINK;
	return S_OK;
}

void CDropTarget::SetPPcAppInfo(PPC_APPINFO *cinfo)
{
	mCinfo = cinfo;
}

void USEFASTCALL dd_close_main(LPDROPTARGET *ppDropTarget)
{
	::CoLockObjectExternal(*ppDropTarget,FALSE,TRUE);
	(*ppDropTarget)->Release();
	(*ppDropTarget) = NULL;
	::OleUninitialize();
}

void dd_combo_init(void)
{
	if ( FAILED(OleInitialize(NULL)) ) return;
	pComboDropTarget = new ComboDropTarget;
	::CoLockObjectExternal(pComboDropTarget,TRUE,FALSE);
	if ( SUCCEEDED(::RegisterDragDrop(Combo.hWnd,
			static_cast<LPDROPTARGET>(pComboDropTarget)))){
		return;
	}
	// 失敗後の処理
	dd_close_main(POINTERCAST(LPDROPTARGET *,&pComboDropTarget));
	return;
}

void dd_init(PPC_APPINFO *cinfo)
{
	if ( cinfo->pDropTarget != NULL ){
		XMessage(cinfo->info.hWnd,NULL,XM_GrERRld,T("dd_init error"));
		return;
	}

	if ( FAILED(OleInitialize(NULL)) ) return;

	cinfo->pDropTarget = new CDropTarget; // 参照数:1
	cinfo->pDropTarget->SetPPcAppInfo(cinfo);
	::CoLockObjectExternal(cinfo->pDropTarget,TRUE,FALSE); // 参照数:1→2→3→2
	if ( SUCCEEDED(::RegisterDragDrop(cinfo->info.hWnd,
			static_cast<LPDROPTARGET>(cinfo->pDropTarget))) ){ // 参照数:++
		return;
	}
	// 失敗後の処理
	dd_close_main(POINTERCAST(LPDROPTARGET *,&cinfo->pDropTarget));
}

void dd_combo_close(void)
{
	if ( pComboDropTarget == NULL ) return;
	::RevokeDragDrop(Combo.hWnd);
	dd_close_main(POINTERCAST(LPDROPTARGET *,&pComboDropTarget));
	return;
}

void dd_close(PPC_APPINFO *cinfo)
{
	if ( cinfo->pDropTarget == NULL ) return;
	::RevokeDragDrop(cinfo->info.hWnd); // 参照数:--
	dd_close_main(POINTERCAST(LPDROPTARGET *,&cinfo->pDropTarget));
}
