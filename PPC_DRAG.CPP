/*-----------------------------------------------------------------------------
	Paper Plane cUI											～ ドラッグ処理 ～
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

const CLSID XCLSID_IDATAOBJECT_AUTODD = CLSID_IDATAOBJECT_AUTODD;
const TCHAR targetnames[] = T("EDIT,RichEdit*,TEDIT");
const TCHAR menustr[] = T("%MC_mdds");

//
void CalcDragTarget(PPC_APPINFO *cinfo,POINT *pos,RECT *area)
{
	POINT start;
	RECT box;
	int bottom;

	if ( pos->x < cinfo->BoxEntries.left )   pos->x = cinfo->BoxEntries.left;
	if ( pos->x > cinfo->BoxEntries.right )  pos->x = cinfo->BoxEntries.right;
	if ( pos->y < cinfo->BoxEntries.top )    pos->y = cinfo->BoxEntries.top;
	if ( pos->y > cinfo->BoxEntries.bottom ) pos->y = cinfo->BoxEntries.bottom;

	start = cinfo->MouseStat.PushClientPoint;

	if ( cinfo->ScrollBarHV == SB_HORZ ){
		start.x += (cinfo->MouseDragWMin - cinfo->cellWMin) * cinfo->cel.Area.cy;
	}else{
		start.y += (cinfo->MouseDragWMin - cinfo->cellWMin) * cinfo->cel.Size.cy;
	}

	if ( start.x < pos->x ){
		box.left = start.x;
		box.right = pos->x;
		if ( (cinfo->ScrollBarHV != SB_HORZ) &&
			 (box.left < cinfo->BoxEntries.left) ){
			box.left = cinfo->BoxEntries.left;
		}
		if ( box.right >= cinfo->BoxEntries.right ){
			box.right = cinfo->BoxEntries.right - 1;
		}
	}else{
		box.left = pos->x;
		box.right = start.x;
		if ( box.left < cinfo->BoxEntries.left ){
			box.left = cinfo->BoxEntries.left;
		}
		if ( (cinfo->ScrollBarHV == SB_HORZ) &&
			 (box.right >= cinfo->BoxEntries.right) ){
			box.right = cinfo->BoxEntries.right - 1;
		}
	}
	bottom = cinfo->BoxEntries.top + cinfo->cel.Size.cy * cinfo->cel.Area.cy;
	if ( start.y < pos->y ){
		box.top = start.y;
		box.bottom = pos->y;
		if ( (cinfo->ScrollBarHV == SB_HORZ) &&
			 (box.top < cinfo->BoxEntries.top) ){
			box.top = cinfo->BoxEntries.top;
		}
		if ( box.bottom >= bottom ) box.bottom = bottom - 1;
	}else{
		box.top = pos->y;
		box.bottom = start.y;
		if ( box.top < cinfo->BoxEntries.top){
			box.top = cinfo->BoxEntries.top;
		}
		if ( (cinfo->ScrollBarHV == SB_HORZ) &&
			 (box.bottom >= bottom) ){
			box.bottom = bottom - 1;
		}
	}
	if ( area != NULL ) *area = box;
}

void DrawDragFrame(HWND hWnd,const RECT *area)
{
	HDC hDC;

	hDC = GetDC(hWnd);
	::DrawFocusRect(hDC,area);
	::ReleaseDC(hWnd,hDC);
}

int MinusDiv(int v1,int v2) // 負数被除数の丸め処理を調整した割り算
{
	return (v1 >= 0) ? (v1 / v2) : -((-v1 + (v2 - 1)) / v2);
}

void MarkDragArea(PPC_APPINFO *cinfo, RECT *area, int markmode)
{
	RECT box;
	int ofL,ofR;

	if( !(cinfo->cel.Size.cx && cinfo->cel.Size.cy) ) return;

	box.left = MinusDiv(area->left - cinfo->BoxEntries.left, cinfo->cel.Size.cx) * cinfo->cel.Area.cy;
	ofL = (area->left - cinfo->TreeX) % cinfo->cel.Size.cx;
	if ( ofL < 0 ){
		ofL = cinfo->cel.Size.cx + ofL;
		box.left -= cinfo->cel.Area.cy;
	}
	if ( (DWORD)ofL >= cinfo->CellNameWidth ){
		box.left += cinfo->cel.Area.cy;
	}
	box.right = MinusDiv(area->right - cinfo->BoxEntries.left, cinfo->cel.Size.cx) * cinfo->cel.Area.cy;
	ofR = (area->right - cinfo->TreeX) % cinfo->cel.Size.cx;
	if ( ofR < 0 ) ofR = cinfo->cel.Size.cx + ofR;
	if ( ofR < (cinfo->fontX + 2) ) box.right -= cinfo->cel.Area.cy;

	box.top = MinusDiv(area->top - cinfo->BoxEntries.top, cinfo->cel.Size.cy);
	box.bottom = MinusDiv(area->bottom - cinfo->BoxEntries.top, cinfo->cel.Size.cy);

	if ( (box.left == box.right) && (box.top == box.bottom)){
		if ( ((DWORD)ofL >= cinfo->CellNameWidth) &&
			 (ofR < (cinfo->fontX + 2)) ){
			return;
		}
	}
	cinfo->MarkMask = 0x1f;

#if FREEPOSMODE
	{ // FreePos の該当物を探す
		int index;
		for ( index = 0 ; index < cinfo->FreePosEntries ; index++ ){
			ENTRYCELL *cell;
			int cellindex;
			RECT cellbox,tmpbox,bbox;

			cellindex = cinfo->FreePosList[index].index;
			cell = &CEL(cellindex);

			bbox = *area;
			bbox.left += CalcFreePosOffX(cinfo);
			bbox.right += CalcFreePosOffX(cinfo);
			bbox.top += CalcFreePosOffY(cinfo);
			bbox.bottom += CalcFreePosOffY(cinfo);

			cellbox.left = cell->pos.x;
			cellbox.top = cell->pos.y;
			cellbox.right = cell->pos.x + cinfo->cel.Size.cx;
			cellbox.bottom = cell->pos.y + cinfo->cel.Size.cy;
			if ( IntersectRect(&tmpbox,&bbox,&cellbox) ){
				CellMark(cinfo,cellindex,markmode);
			}
		}
	}
#endif

#if 1
	{
		ENTRYINDEX index, indexmax;

		index = cinfo->cellWMin + box.left + box.top;
		indexmax = cinfo->cellWMin + box.right + box.bottom;
		if ( box.left < box.right ) { // ２段以上のときの補正
			int righttop = min(box.top, box.bottom);
			// 右端の選択範囲にエントリが無い場合、右端段を除外
			if ( (cinfo->cellWMin + box.right + righttop) >= cinfo->e.cellIMax ){
				indexmax -= cinfo->cel.Area.cy;
			}
		}

		for ( ; index <= indexmax ; index++){
#if FREEPOSMODE
			if ( (index >= 0) && (index < cinfo->e.cellIMax) && (CEL(index).pos.x == NOFREEPOS) ){
#else
			if ( (index >= 0) && (index < cinfo->e.cellIMax) ){
#endif
				CellMark(cinfo,index,markmode);
			}
		}
	}
#else // 箱形選択
	for ( ; box.left <= box.right ; box.left += cinfo->cel.Area.cy){
		ENTRYINDEX index, y;

		for ( y = box.top ; y <= box.bottom ; y++){
			index = cinfo->cellWMin + box.left + y;
#if FREEPOSMODE
			if ( (index >= 0) && (index < cinfo->e.cellIMax) && (CEL(index).pos.x == NOFREEPOS) ){
#else
			if ( (index >= 0) && (index < cinfo->e.cellIMax) ){
#endif
				CellMark(cinfo,index,markmode);
			}
		}
	}
#endif
}

void MoreDrag(PPC_APPINFO *cinfo)
{
	POINT pos;
	HWND hTargetWnd;
	TCHAR classname[MAX_PATH],setdata[CMDLINESIZE];
	FN_REGEXP fn;

	DWORD XC_mdd = 1;

	GetCustData(T("XC_mdd"),&XC_mdd,sizeof(XC_mdd));
	if ( !XC_mdd ) return;

	::GetCursorPos(&pos);
	hTargetWnd = ::WindowFromPoint(pos);
	::GetClassName(hTargetWnd,classname,MAX_PATH);

	if ( tstrcmp(classname,T("SysTreeView32")) == 0 ){
		hTargetWnd = GetParent(hTargetWnd);
		::GetClassName(hTargetWnd,classname,MAX_PATH);
		// SHELLDLL_DefView なら、ファイルを開くダイアログ
		if ( tstrcmp(classname,T("SHBrowseForFolder ShellNameSpace Control")) == 0 ){
			HMENU hMenu;
			hMenu = ::CreatePopupMenu();
			int id;

			#ifdef UNICODE
				#define DVirtualAllocEx VirtualAllocEx
				#define DVirtualFreeEx VirtualFreeEx
			#else
				DefineWinAPI(LPVOID,VirtualAllocEx,(HANDLE,LPVOID,ULONG_PTR,DWORD,DWORD));
				DefineWinAPI(BOOL,VirtualFreeEx,(HANDLE,LPVOID,ULONG_PTR,DWORD));
				HMODULE hKernel32 = GetModuleHandle(StrKernel32DLL);

				GETDLLPROC(hKernel32,VirtualAllocEx);
				if ( DVirtualAllocEx == NULL ) return;
				GETDLLPROC(hKernel32,VirtualFreeEx);
			#endif
			if ( CEL(cinfo->e.cellN).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				VFSFullPath(setdata,CEL(cinfo->e.cellN).f.cFileName,cinfo->path);
			}else{
				tstrcpy(setdata,cinfo->RealPath);
			}
			::AppendMenuString(hMenu,1,setdata);
			AppendMenuString(hMenu,0,MES_CCAN);
			id = ::TrackPopupMenu(hMenu,TPM_TDEFAULT,
					pos.x,pos.y,0,cinfo->info.hWnd,NULL);
			::DestroyMenu(hMenu);
			if ( id ){
				HANDLE hTargetProcess;
				DWORD TargetProcessID;
				LPVOID TargetMem;
				size_t strsize;

				GetWindowThreadProcessId(hTargetWnd,&TargetProcessID);
				hTargetProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,0,TargetProcessID);
				if ( hTargetProcess != NULL ){
					strsize = TSTRSIZE(setdata);
					TargetMem = DVirtualAllocEx(hTargetProcess,NULL,strsize,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
					if ( TargetMem != NULL ){
						if ( FALSE != WriteProcessMemory(hTargetProcess,TargetMem,setdata,strsize,NULL) ){
							SendMessage(GetParent(hTargetWnd),BFFM_SETSELECTION,(WPARAM)TRUE,(LPARAM)TargetMem);
						}
						DVirtualFreeEx(hTargetProcess,TargetMem,0,MEM_RELEASE);
					}
					CloseHandle(hTargetProcess);
				}
			}
		}
		return;
	}

	MakeFN_REGEXP(&fn,targetnames);
	if ( !FilenameRegularExpression(classname,&fn) ){
		FreeFN_REGEXP(&fn);
		return;
	}
	FreeFN_REGEXP(&fn);

	ForceSetForegroundWindow(cinfo->info.hWnd);

	setdata[0] = '\0';
	if ( IsExistCustData(&menustr[1]) ){
		if ( PP_ExtractMacro(cinfo->info.hWnd,
				&cinfo->info,&pos,menustr,setdata,0) != NO_ERROR ){
			return;
		}
	}else{ // メニューがない場合の代用メニュー
		HMENU hMenu;
		hMenu = ::CreatePopupMenu();
		int id;

		::AppendMenuString(hMenu,1,T("Insert &Entry"));
		AppendMenuString(hMenu,0,MES_CCAN);
		id = ::TrackPopupMenu(hMenu,TPM_TDEFAULT,
				pos.x,pos.y,0,cinfo->info.hWnd,NULL);
		::DestroyMenu(hMenu);
		if ( id != 0 ){
			VFSFullPath(setdata,CEL(cinfo->e.cellN).f.cFileName,cinfo->path);
		}
	}
	if ( setdata[0] != '\0' ){
		::SendMessage(hTargetWnd,WM_SETTEXT,0,(LPARAM)setdata);
		ForceSetForegroundWindow(hTargetWnd);
	}
}

struct PPcDataObject {
	IDataObject *DataObject;
	CImpIDataObject *CDataObject;
	LPITEMIDLIST *pidls;
	LPSHELLFOLDER pSF;
	int count;
};

void DestroyPPcDataObject(PPcDataObject &pdo)
{
	if ( pdo.DataObject != NULL ){
		if ( pdo.CDataObject == NULL ){
			pdo.DataObject->Release();
		}else{
			pdo.CDataObject->Release();
		}
	}
	if ( pdo.pSF != NULL ){
		FreePIDLS(pdo.pidls,pdo.count);
		pdo.pSF->Release();
		HeapFree( hProcessHeap,0,pdo.pidls);
	}
}

BOOL CreatePPcDataObject(PPC_APPINFO *cinfo, PPcDataObject &pdo, const TCHAR *src)
{
	pdo.DataObject = NULL;
	pdo.CDataObject = NULL;
	pdo.pSF = NULL;

	if ( src != NULL ){ // 直接指定
		LPITEMIDLIST idl;

		if ( FALSE == VFSMakeIDL(cinfo->path, &pdo.pSF, &idl,src) ){
			return FALSE;
		}
		if ((pdo.pidls = (LPITEMIDLIST *)HeapAlloc(hProcessHeap, 0, 2 * sizeof(LPITEMIDLIST *))) == NULL) {
			return FALSE;
		}
		pdo.pidls[0] = idl;
		pdo.count = 1;
		if ( FAILED(pdo.pSF->GetUIObjectOf(cinfo->info.hWnd, pdo.count,
				(LPCITEMIDLIST *)pdo.pidls, IID_IDataObject,
				NULL, (void **)&pdo.DataObject) )){
			pdo.DataObject = NULL;
			DestroyPPcDataObject(pdo);
			return FALSE;
		}
	}else if ( IsNodirShnPath(cinfo) ){ // SHN 形式
		pdo.count = MakePIDLTable(cinfo, &pdo.pidls, &pdo.pSF);
		if ( pdo.count == 0 ) return FALSE;

		if ( FAILED(pdo.pSF->GetUIObjectOf(cinfo->info.hWnd, pdo.count,
				(LPCITEMIDLIST *)pdo.pidls, IID_IDataObject,
				NULL, (void **)&pdo.DataObject) )){
			pdo.DataObject = NULL;
			DestroyPPcDataObject(pdo);
			return FALSE;
		}
	}else{
		STGMEDIUM stm;
		FORMATETC fe;
										// ShellIDList のクリップボードIDを取得
		CF_xSHELLIDLIST = ::RegisterClipboardFormat(CFSTR_SHELLIDLIST);
										// ドラッグ対象を準備
		pdo.CDataObject = new CImpIDataObject;
		pdo.CDataObject->m_cinfo = cinfo;
										// アーカイブモードの判定
		pdo.CDataObject->m_arcmode = PPcUnpackForAction(cinfo, pdo.CDataObject->m_arctemp, UFA_PATH);

		stm.tymed = 0;		// Local仕様：GetData時に生成
		fe.tymed = TYMED_HGLOBAL;
		fe.ptd = NULL;
		fe.lindex = -1;
		fe.dwAspect = DVASPECT_CONTENT;

		fe.cfFormat = (WORD)CF_xSHELLIDLIST;	// ShellIDList 形式
		pdo.CDataObject->SetData(&fe, &stm, TRUE);

		if ( cinfo->RealPath[0] != '?' ){			// CF_HDROP 形式
			fe.cfFormat = CF_HDROP;
			pdo.CDataObject->SetData(&fe, &stm, TRUE);
		}
		if ( GetCustXDword(T("X_tdd"), NULL, 1) ){	// CF_TEXT 形式
			fe.cfFormat = CF_TTEXT;
			pdo.CDataObject->SetData(&fe, &stm, TRUE);
		}
		pdo.DataObject = pdo.CDataObject;
	}
	return TRUE;
}

void PPcDragFile(PPC_APPINFO *cinfo)
{
	PPcDataObject pdo;
	HRESULT result;
	DWORD dwEffect;

	if ( !cinfo->e.markC && (CEL(cinfo->e.cellN).type <= 3) ) return;

	if ( IsTrue(CreatePPcDataObject(cinfo,pdo,NULL)) ){ // D&D 本体を実行
		CImpIDropSource *CDropSource;

		CDropSource = new CImpIDropSource;
		::SelfDD_hWnd = cinfo->info.hWnd;
		::SelfDD_Dtype = cinfo->e.Dtype.mode;
		result = ::DoDragDrop(pdo.DataObject,CDropSource,
				DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK |
				DROPEFFECT_SCROLL,&dwEffect);
		::SelfDD_hWnd = NULL;
		CDropSource->Release();
		DestroyPPcDataObject(pdo);

			// ドラッグ成功 & アクション無し→テキストボックスD&Dをチャレンジ
		if ( (result == DRAGDROP_S_DROP) && !dwEffect ){
			MoreDrag(cinfo);
		}
	}
}
#if !NODLL
void * GetPathInterface(HWND hWnd,const TCHAR *name,REFIID riid,const TCHAR *path)
{
	LPITEMIDLIST pidl;
	LPSHELLFOLDER pSF;
	void *ifptr;

	if ( VFSMakeIDL(path,&pSF,&pidl,name) == FALSE ) return NULL;
	if ( FAILED(pSF->GetUIObjectOf(hWnd,1,(LPCITEMIDLIST *)&pidl,
			riid,NULL,&ifptr)) ){
		ifptr = NULL;
	}
	FreePIDL(pidl);
	pSF->Release();
	return ifptr;
}
#endif
HRESULT PPcCopyToDropTarget(IDataObject *DataObject,IDropTarget *DropTarget,BOOL droptype,HWND hTargetWnd,DWORD alloweffect)
{
	POINT pos = { 0,0 };
	POINTL posl;
	DWORD dropeffect;
	DWORD keystate;
	HRESULT result;

	if ( droptype ){
		keystate = MK_RBUTTON;
		if ( hTargetWnd != NULL ) SetForegroundWindow(hTargetWnd);
	}else{
		keystate = MK_LBUTTON;
	}

	if ( hTargetWnd != NULL ) ClientToScreen(hTargetWnd,&pos);
	posl.x = pos.x;
	posl.y = pos.y;

	dropeffect = alloweffect;
	result = DropTarget->DragEnter(DataObject,keystate,posl,&dropeffect);
	if ( FAILED(result) ) goto fin;

	if ( (dropeffect & alloweffect) == 0 ){
		dropeffect = alloweffect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
		keystate = MK_RBUTTON;
		if ( hTargetWnd == NULL ){
			hTargetWnd = GetForegroundWindow();
			if ( hTargetWnd != NULL ){
				ClientToScreen(hTargetWnd,&pos);
				posl.x = pos.x;
				posl.y = pos.y;
			}
		}
		result = DropTarget->DragEnter(DataObject,keystate,posl,&dropeffect);
		if ( FAILED(result) ) goto fin;
	}
	dropeffect = alloweffect;
	result = DropTarget->DragOver(keystate,posl,&dropeffect);
	if ( FAILED(result) ) goto fin;

	if ( (dropeffect & alloweffect) == 0 ){
		result = E_FAIL;
	}else{
		dropeffect = alloweffect;
		result = DropTarget->Drop(DataObject,keystate,posl,&dropeffect);
	}
fin:
	if ( FAILED(result) ) PPErrorBox(hTargetWnd,NULL,result);
	return result;
}

BOOL CopyToShnPathFiles(PPC_APPINFO *cinfo,const TCHAR *destpath,DWORD alloweffect)
{
	PPcDataObject pdo;
	IDropTarget *DropTarget;

	if ( CreatePPcDataObject(cinfo,pdo,NULL) == FALSE ) return FALSE;
	DropTarget = (IDropTarget *)GetPathInterface(cinfo->info.hWnd,destpath,IID_IDropTarget,cinfo->path);
	if ( DropTarget == NULL ) goto finl;
	PPcCopyToDropTarget(pdo.DataObject,DropTarget,FALSE,NULL,alloweffect);
	DropTarget->Release();
finl:
	DestroyPPcDataObject(pdo);
	return TRUE;
}

// DLL hook を利用した Auto D&D を行う
BOOL AutoDD_UseDLL(PPC_APPINFO *cinfo,HWND hTargetWnd,const TCHAR *src,BOOL droptype)
{
	PPcDataObject pdo;
	IUnknown *UnknownCOM = NULL;
	IDropTarget *DropTarget = NULL;
	int trycount = 3;
	BOOL result = FALSE;

	if ( CreatePPcDataObject(cinfo,pdo,src) == FALSE ) return FALSE;
	if ( hOleaut32 == NULL ){
		hOleaut32 = LoadWinAPI("OLEAUT32.DLL",NULL,OLEAUT32APIS,LOADWINAPI_LOAD);
		if ( hOleaut32 == NULL ) return FALSE;
	}
	// 相手にPPXLIB.DLL を注入する
	if ( PPxCommonExtCommand(KC_HOOKADDPROC,(WPARAM)hTargetWnd) != NO_ERROR ){
		goto finl;
	}
	SendMessage(hTargetWnd,WM_PPXCOMMAND,KC_REGISTADDPROC,0);
	// GetActiveObject はシステムフックコールバック内では正常に機能しないので、こちら側で DropTarget を取得する
	for ( ; ; ){
		if ( SUCCEEDED(::DGetActiveObject(XCLSID_IDATAOBJECT_AUTODD,NULL,&UnknownCOM)) ){
			break;
		}else{
			Sleep(100);
			if ( trycount-- ) continue;
			goto errorfin;
		}
	}
	UnknownCOM->AddRef();
	if ( FAILED(UnknownCOM->QueryInterface(IID_IDropTarget,(void **)&DropTarget)) ){
		goto errorfin;
	}

	PPcCopyToDropTarget(pdo.DataObject,DropTarget,droptype,hTargetWnd,
			DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK);
	result = TRUE;
errorfin:
	if ( DropTarget != NULL ) DropTarget->Release();
	if ( UnknownCOM != NULL ){
		UnknownCOM->Release(); // AddRef 分
		UnknownCOM->Release(); // GetActiveObject 分
	}
	SendMessage(hTargetWnd,WM_PPXCOMMAND,KC_REVOKEADDPROC,0);
	PPxCommonExtCommand(KC_UNHOOKADDPROC,(WPARAM)hTargetWnd);
finl:
	DestroyPPcDataObject(pdo);
	return result;
}

void ExecSendTo(PPC_APPINFO *cinfo,const TCHAR *cmd)
{
	const TCHAR *cmdext;
	TCHAR buf[CMDLINESIZE],buf2[VFPS];
	HRESULT result = E_FAIL;

	cmdext = cmd + FindExtSeparator(cmd);
	if ( tstrlen(cmdext) > 5 ){
		if ( GetRegString(HKEY_CLASSES_ROOT,cmdext,NilStr,buf,TSIZEOF(buf)) ){
			tstrcat(buf,T("\\shellex\\DropHandler"));
			if ( GetRegString(HKEY_CLASSES_ROOT,buf,NilStr,buf2,TSIZEOF(buf2)) ){
				CLSID hid;
				#undef tidname
				#ifndef UNICODE
					WCHAR idnameW[MAX_PATH];
					#define tidname idnameW

					AnsiToUnicode(buf2,idnameW,MAX_PATH);
				#else
					#define tidname buf2
				#endif

				if ( SUCCEEDED(::CLSIDFromString(tidname,&hid)) ){
					IDropTarget *pDropTarget;
					if ( SUCCEEDED(::CoCreateInstance(hid,NULL,
							CLSCTX_INPROC_SERVER,IID_IDropTarget,
							(LPVOID *)&pDropTarget)) ){
						PPcDataObject pdo;
						if ( IsTrue(CreatePPcDataObject(cinfo,pdo,NULL)) ){
							result = PPcCopyToDropTarget(pdo.DataObject,pDropTarget,FALSE,NULL,DROPEFFECT_COPY);
							DestroyPPcDataObject(pdo);
						}
						pDropTarget->Release();
					}
				}
			}
		}
	}
	if ( FAILED(result) ){
		VFSFixPath(buf2,(TCHAR *)cmd,T("-#9:\\"),VFSFIX_FULLPATH | VFSFIX_REALPATH);
		if ( tstricmp(cmdext,StrShortcutExt) == 0 ){
			TCHAR pbuf[VFPS];

			if ( SUCCEEDED(GetLink(cinfo->info.hWnd,buf2,pbuf)) ){
				DWORD attr;

				buf2[0] = '\0';
				ExpandEnvironmentStrings(pbuf,buf2,TSIZEOF(buf2));
				attr = GetFileAttributesL(buf2);
				if ( (attr & FILE_ATTRIBUTE_DIRECTORY) && (attr != BADATTR) ){
					PPcFileOperation(cinfo,FileOperationMode_Copy,buf2,NULL);
					return;
				}
			}
		}
		wsprintf(buf,T("\"%s\" %%FDC"),buf2);
		PP_ExtractMacro(cinfo->info.hWnd,&cinfo->info,NULL,buf,NULL,0);
	}
}
