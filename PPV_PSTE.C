/*-----------------------------------------------------------------------------
	Paper Plane vUI											～ ペースト処理 ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

UINT CF_SHELLIDLIST;

INT_PTR CALLBACK GetPasteTypeMain(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			UINT cliptype = 0;
			int index = 0;

			SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)lParam);
			LocalizeDialogText(hDlg,IDD_PASTETYPE);
			while ( (cliptype = EnumClipboardFormats(cliptype)) != 0 ){
				TCHAR name[VFPS];

				GetClipboardTypeName(name,cliptype);
				SendDlgItemMessage(hDlg,IDL_PT_LIST,
							LB_ADDSTRING,0,(LPARAM)name);
				SendDlgItemMessage(hDlg,IDL_PT_LIST,
							LB_SETITEMDATA,(WPARAM)index++,(LPARAM)cliptype);
			}
			SendDlgItemMessage(hDlg,IDL_PT_LIST,LB_SETCURSEL,(WPARAM)0,0);
			GetPasteTypeMain(hDlg,WM_COMMAND,
					TMAKELPARAM(IDL_PT_LIST,LBN_SELCHANGE),
					(LPARAM)GetDlgItem(hDlg,IDL_PT_LIST));
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					EndDialog(hDlg,1);
					break;
				case IDCANCEL:
					EndDialog(hDlg,0);
					break;
				case IDL_PT_LIST:
					if ( HIWORD(wParam) == LBN_SELCHANGE ){
						LRESULT type;

						type = SendMessage((HWND)lParam,LB_GETCURSEL,0,0);
						if ( type == LB_ERR ) break;
						*(UINT *)GetWindowLongPtr(hDlg,DWLP_USER) =
								SendMessage((HWND)lParam,
											LB_GETITEMDATA,(WPARAM)type,0);
					}
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg,0);
			break;
	}
	return FALSE;
}

UINT GetPasteType(HWND hWnd)
{
	UINT result = 0;

	if ( !CountClipboardFormats() ) return 0;

	if ( PPxDialogBoxParam(hInst,MAKEINTRESOURCE(IDD_PASTETYPE),hWnd,
				GetPasteTypeMain,(LPARAM)&result) > 0 ){
		return result;
	}else{
		return 0;
	}
}

HGLOBAL PasteMain(UINT type,VIEWOPTIONS *viewopts)
{
	TCHAR *result;
	HGLOBAL viewdata C4701CHECK;
	HANDLE clipdata;
	DWORD size;
	BOOL resize = FALSE;
	char *src,*dst;

	InitViewOptions(viewopts);
	for (;;){
		clipdata = GetClipboardData(type);
		if ( clipdata == NULL ){
			result = T("Clip fault");
			break;
		}
		if ( type == CF_ENHMETAFILE ){
			HENHMETAFILE hEMeta;

			hEMeta = clipdata;
			size = GetEnhMetaFileBits(hEMeta,0,NULL);

			viewdata = GlobalAlloc(GMEM_MOVEABLE,size);
			if ( viewdata == NULL ){
				result = T("Alloc error");
				break;
			}
			dst = GlobalLock(viewdata);
			GetEnhMetaFileBits(hEMeta,size,(LPBYTE)dst);
			GlobalUnlock(viewdata);
			result = NULL;
			break;
		}
		if ( type == CF_SHELLIDLIST ){
			TCHAR text[0x10000];
			DWORD sisize;

			GetTextFromCF_SHELLIDLIST(text,0x10000,clipdata,TRUE);
			sisize = TSTRLENGTH32(text);
			viewdata = GlobalAlloc(GMEM_MOVEABLE,sisize);
			if ( viewdata == NULL ){
				result = T("Alloc error");
				break;
			}
			memcpy(GlobalLock(viewdata),text,sisize);
			GlobalUnlock(viewdata);
			result = NULL;
			break;
		}

		size = GlobalSize(clipdata);
		if ( !size ){
			result = T("unsupport");
			break;
		}
		if ( (type == CF_DIB) || (type == CF_DIBV5) ){
			size += sizeof(BITMAPFILEHEADER);
			viewdata = GlobalAlloc(GMEM_MOVEABLE,size);
			if ( viewdata == NULL ){
				result = T("Alloc error");
				break;
			}
			dst = GlobalLock(viewdata);
			src = GlobalLock(clipdata);
			if ( (dst == NULL) || (src == NULL) ){
				result = T("Alloc error");
				break;
			}

			((BITMAPFILEHEADER *)dst)->bfType = 'B' + ('M' << 8);
			((BITMAPFILEHEADER *)dst)->bfSize = size;
			((BITMAPFILEHEADER *)dst)->bfReserved1 = 0;
			((BITMAPFILEHEADER *)dst)->bfReserved2 = 0;
			((BITMAPFILEHEADER *)dst)->bfOffBits =
					CalcBmpHeaderSize( (BITMAPINFOHEADER *)src ) +
					sizeof(BITMAPFILEHEADER);
			memcpy(dst + sizeof(BITMAPFILEHEADER),src,
					size - sizeof(BITMAPFILEHEADER));
		}else{
			viewdata = GlobalAlloc(GMEM_MOVEABLE,size);
			if ( viewdata == NULL ){
				result = T("Alloc error");
				break;
			}
			dst = GlobalLock(viewdata);
			src = GlobalLock(clipdata);
			if ( (dst == NULL) || (src == NULL) ){
				result = T("Alloc error");
				break;
			}
			if ( (type == CF_TEXT) || (type == CF_OEMTEXT) ){
				resize = TRUE;
				size = strlen32((char *)src);
			}
			if ( type == CF_UNICODETEXT ){
				resize = TRUE;
				size = strlenW32((const WCHAR *)src) * sizeof(WCHAR);
				viewopts->T_code = VTYPE_UNICODE;
			}
			memcpy(dst,src,size);
		}
		GlobalUnlock(clipdata);
		GlobalUnlock(viewdata);
		if ( resize != FALSE ){
			HGLOBAL hNew;

			hNew = GlobalReAlloc(viewdata,size,GMEM_MOVEABLE);
			if ( hNew != NULL ) viewdata = hNew;
		}
		result = NULL;
		break;
	}
	if ( result != NULL ){
		SetPopMsg(0,result);
		return NULL;
	}else{
		return viewdata; // C4701ok
	}
}

void PPvPaste(HWND hWnd)
{
	UINT type,tmptype;
	HGLOBAL viewdata;
	VIEWOPTIONS viewopts;

	if ( OpenClipboard(hWnd) == FALSE ){
		SetPopMsg(POPMSG_NOLOGMSG,T(" Clipboard open error "));
		return;
	}
	if ( !CountClipboardFormats() ) return; // 何もない
	CF_SHELLIDLIST = RegisterClipboardFormat(CFSTR_SHELLIDLIST);

	// 最優先形式を取得
	type = tmptype = EnumClipboardFormats(0);
	while ( tmptype ){
		// UNICODEでないとだめな文字があればUNICODEそうでなければTEXT
		if ( (tmptype == CF_TEXT) || (tmptype == CF_UNICODETEXT) ){
			HANDLE clipdata;
			WCHAR *src;
			int size;
			BOOL inwchar;

			clipdata = GetClipboardData(CF_UNICODETEXT);
			src = GlobalLock(clipdata);
			size = WideCharToMultiByte(CP_ACP,0,src,-1,NULL,0,NULL,&inwchar);
			GlobalUnlock(clipdata);
			if ( (size != 0) && inwchar ){
				type = CF_UNICODETEXT;
			}else{
				type = CF_TEXT;
			}
			break;
		}
		if ( (tmptype == CF_DIB) ||
			 (tmptype == CF_DIBV5) ||
			 (tmptype == CF_ENHMETAFILE) ||
			 (tmptype == CF_SHELLIDLIST) ){	// 優先形式にサポート形式有り
			type = tmptype;
			break;
		}
		tmptype = EnumClipboardFormats(tmptype);
	}
	if ( type == 0 ){ // 該当無し
		CloseClipboard();
		return;
	}
	viewdata = PasteMain(type,&viewopts);
	CloseClipboard();
	if ( viewdata != NULL ){
		OpenAndFollowViewObject(&vinfo,T("Clipboard"),viewdata,&viewopts,0);
	}
}

ERRORCODE PPvPasteType(HWND hWnd)
{
	UINT type;
	HGLOBAL viewdata;
	VIEWOPTIONS viewopts;

	if ( OpenClipboard(hWnd) == FALSE ){
		SetPopMsg(POPMSG_NOLOGMSG,T(" Clipboard open error "));
		return ERROR_BAD_COMMAND;
	}

	type = GetPasteType(hWnd);
	if ( type == 0 ){
		CloseClipboard();
		return ERROR_CANCELLED;
	}
	CF_SHELLIDLIST = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
	viewdata = PasteMain(type,&viewopts);
	CloseClipboard();
	if ( viewdata != NULL ){
		OpenAndFollowViewObject(&vinfo,T("Clipboard"),viewdata,&viewopts,0);
	}
	return NO_ERROR;
}
