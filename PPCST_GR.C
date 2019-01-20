/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer								全般シート
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#pragma hdrstop

#define IDM_SETEDITFOCUS	0xe001f000
#define IDM_RCLICKED		0xe0020000
#ifndef UDM_SETRANGE32
#define UDM_SETRANGE32 (WM_USER+111)
#endif

// itemname:key[:sub][+offset]=(edittype)
typedef enum {
	ITEM_INDEXLABEL,	// 表示用文字列(str)
	ITEM_LABEL,			// 表示用文字列(str)
	ITEM_INT,	// int(num)				i/指定無し
	ITEM_DWORD,	// DWORD(num)			d
	ITEM_WORD,	// WORD(num)			w
	ITEM_BYTE,	// byte(num)			b
	ITEM_STRING,// 文字列(str)			s
	ITEM_KEY,	// キー(num)
	ITEM_FONT,	// フォント(font)
	ITEM_RESERVED	// その他
} ITEMTYPES;

typedef enum {
	EDIT_NONE,	// 表示用文字列
	EDIT_CHECK,	// チェックボックス		?[itemtype][B]check[/def]:uncheck
	EDIT_RADIO,	// ラジオボタン			@[itemtype]hit/def
	EDIT_EDIT,	// エディットボックス	<def
	EDIT_KEY,	// キー					K
	EDIT_FILE,	// ファイル名			N
	EDIT_DIR,	// ディレクトリ名		P
	EDIT_FONT,	// フォント				F
	EDIT_ppchotkey	// PPcのホットキー	c
} EDITTYPES;

typedef enum {
	ITEMOPTION_NONE,
	ITEMOPTION_NOMACROSTR, // %0 等は使えない
} ITEMOPTION;

typedef struct {
	DWORD def,data,truedata,falsedata,mask,size;
} INTITEM;
#define IITEM_VALUE MAX32

typedef struct {
	TCHAR data[VFPS * 2];
	TCHAR setdata[VFPS * 2];
} STRITEM;

typedef union {
	INTITEM num;
	STRITEM str;
	LOGFONTWITHDPI fontdata;
} ITEMS;

typedef struct {
	ITEMTYPES itemtype;		// アイテムの種別
	ITEMOPTION option;		// 追加情報
	EDITTYPES edittype;		// アイテムの編集方法
	int layer;				// 階層
	BOOL edit;				// エディットボックスで編集中か
	TCHAR itemname[0x200];	// 表示用のアイテム名
	TCHAR *keyname;			// アイテムの保存名
	TCHAR *subname;			// テーブルのアイテムの保存名
	DWORD offset;			// 読み書きするオフセット
	ITEMS d;
} ITEMSTRUCT;


LOGFONT DefaultFont = {							// フォント構造体
	0,0,						// Width,Height
	0,0,FW_NORMAL,				// Escapement,Orientation,Weight
	FALSE,FALSE,FALSE,			// Italic,Underline,StrikeOut
	SHIFTJIS_CHARSET,			// CharSet
	OUT_DEFAULT_PRECIS,			// OutPrecision
	CLIP_DEFAULT_PRECIS,		// ClipPrecision
	DEFAULT_QUALITY,			// Quality
	FIXED_PITCH | FF_DONTCARE,	// PitchAndFamily
	T("")			// FaceName
};
const TCHAR DefaultFaceName[] = T("(未設定)\0(blank)");

const TCHAR EDIT[] = T("EDIT");
const TCHAR BUTTON[] = T("BUTTON");
WNDPROC OldFindBoxProc,OldIndexTreeProc,OldItemTreeProc;

const TCHAR *CustomList,*CustomItemList;
int CustomItemListOffset;

#define FLOAT_WIDTH 150
#define FLOAT_HEIGHT 22
#define FLOAT_BUTTONWIDTH 50
HWND hEditWnd,hButtonWnd,hSpinWnd;		// フローティングコントロール
HWND hIndexTreeWnd,hItemTreeWnd;	// 左ペイン,右ペイン
HTREEITEM hedititem;
int useedit = 0;
int useeditmodify = 0;
ITEMSTRUCT edititem;
#if !NODLL
int X_pmc[4] = {X_pmc_defvalue};
#else
extern int X_pmc[4];
#endif

const TCHAR TrayHotKeyCust[] = T("K_tray");
const TCHAR TrayHotKeyPPcCustParam[] = T("\x9f*focus !");
const TCHAR DelKeyMsg[] = T("キー割当てを削除しますか？\0Delete key setting ?");
const TCHAR WarnNoMacroMsg[] = T("警告：マクロ文字(%)は使用できません。%0の代わりに相対指定でPPxパスを指定できます。\0Warning : unsupport macro charcter.");

const TCHAR StrMenuHelp[] = T("ヘルプ\0help");
const TCHAR StrMenuDefault[] = T("初期値\0default setting");
const TCHAR StrFontDefault[] = T("フォントを初期値に戻しますか？\0Restore default setting ?");
const TCHAR StrFontList[] = T("mdfutc");
const WCHAR StrFindInfo[] = L"説明文 or X_xxx\0Word or X_xxx";

#define maxlayer 4	// 対応階層数

typedef enum {
	ICON_BLANK = 0,
	ICON_LABEL,
	ICON_CHECKOFF,
	ICON_CHECKON,
	ICON_RADIOOFF,
	ICON_RADIOON,
	ICON_BUTTON,
	ICON_DOCKCLOSE,
	ICON_DOCKOPEN
} LABELICONS;

#define TreeImageCharCount 9
const WCHAR TreeImageChars[TreeImageCharCount] = {L' ',L'・',0x2610,0x2611,0x25cb,0x25ce,0x270e,0x229e,0x229f};

void SearchPPcHotKey(ITEMSTRUCT *item)
{
	int count = 0;
	TCHAR keyword[CMDLINESIZE],param[CMDLINESIZE];

	while( EnumCustTable(count,TrayHotKeyCust,keyword,param,sizeof(param)) >= 0 ){
		if ( !tstrcmp(param,TrayHotKeyPPcCustParam) ){
			const TCHAR *ptr;

			ptr = keyword;
			item->d.num.data = GetKeyCode(&ptr);
			break;
		}
		count++;
	}
	item->d.num.def = item->d.num.data;
}

void SetPPcHotKey(ITEMSTRUCT *item)
{
	TCHAR keyword[CMDLINESIZE];

	if ( item->d.num.def != 0 ){
		PutKeyCode(keyword,item->d.num.def);
		DeleteCustTable(TrayHotKeyCust,keyword,0);
	}
	if ( item->d.num.data != 0 ){
		PutKeyCode(keyword,item->d.num.data);
		SetCustTable(TrayHotKeyCust,keyword,TrayHotKeyPPcCustParam,sizeof(TrayHotKeyPPcCustParam));
	}
}

const TCHAR *SearchList(int line)
{
	const TCHAR *p;

	if ( line <= 0 ) return NULL;
	p = CustomList;
	while( --line ){
		p += tstrlen(p) + 1;
		if ( *p == '\0' ) return NULL;
	}
	if ( *p == '\n' ) p++;
	return p;
}

void GetCustItemType(ITEMSTRUCT *item,TCHAR **line)
{
	item->d.num.size = 4;
	switch ( *((*line)++) ){
		case 'i':
			item->itemtype = ITEM_INT;
			break;
		case 'd':
			item->itemtype = ITEM_DWORD;
			break;
		case 'w':
			item->itemtype = ITEM_WORD;
			item->d.num.size = 2;
			break;
		case 'b':
			item->itemtype = ITEM_BYTE;
			item->d.num.size = 1;
			break;
		case 's':
			item->itemtype = ITEM_STRING;
			break;
		default:
			(*line)--;
			break;
	}
}

void GetData(ITEMSTRUCT *item,void *data,DWORD size)
{
	TCHAR work[CMDLINESIZE * 2];

	memset((char *)work + item->offset,0,size);
	if ( item->subname == NULL ){
		if ( GetCustDataSize(item->keyname) <= (int)item->offset ) return;
		if ( NO_ERROR != GetCustData(item->keyname,work,sizeof work) ) return;
	}else{
		if ( GetCustTableSize(item->keyname,item->subname) <= (int)item->offset){
			return;
		}
		if ( NO_ERROR != GetCustTable(item->keyname,item->subname,work,sizeof work) ){
			return;
		}
	}
	memcpy(data,(char *)work + item->offset,size);
}

void SetData(ITEMSTRUCT *item,void *data,DWORD size)
{
	TCHAR work[CMDLINESIZE * 2];
	int datasize;

	memset(work,0,sizeof work);
	if ( item->subname == NULL ){
		datasize = GetCustDataSize(item->keyname);
		GetCustData(item->keyname,work,sizeof work);
	}else{
		datasize = GetCustTableSize(item->keyname,item->subname);
		GetCustTable(item->keyname,item->subname,work,sizeof work);
	}
	memcpy((char *)work + item->offset,data,size);
	if ( (datasize == -1) || ((DWORD)datasize < (item->offset + size)) ){
		datasize = item->offset + size;
	}
	if ( item->subname == NULL ){
		SetCustData(item->keyname,work,datasize);
	}else{
		SetCustTable(item->keyname,item->subname,work,datasize);
	}
}

void GetCustItemData(ITEMSTRUCT *item,TCHAR **line)
{
	switch ( item->itemtype ){
		case ITEM_INT:
		case ITEM_DWORD:
		case ITEM_WORD:
		case ITEM_BYTE: {
			TCHAR *p;

			item->d.num.truedata = GetNumber((const TCHAR **)line);
			p = tstrchr(*line,'/');
			if ( p != NULL ){
				p++;
				item->d.num.def = GetNumber((const TCHAR **)&p);
				*line = p;
			}else{
				item->d.num.def = 0;
			}
			item->d.num.data = item->d.num.def;
			GetData(item,&item->d.num.data,item->d.num.size);
			break;
		}
		case ITEM_STRING:
			tstrcpy(item->d.str.setdata,*line);
			item->d.str.data[0] = '\0';
			GetData(item,&item->d.str.data,sizeof(item->d.str.data));
			break;
	}
}

void GetCustItem(ITEMSTRUCT *item,const TCHAR *line,BOOL getdata)
{
	TCHAR *itemname,*param;
	TCHAR *key,*subkey,*offset;

	item->itemtype = ITEM_LABEL;
	item->option = ITEMOPTION_NONE;
	item->edittype = EDIT_NONE;
	item->layer = 1;
	item->offset = 0;
	item->edit = FALSE;
	item->keyname = NULL;
									// layer 数の取得
	if ( *line == '.' ){
		item->itemtype = ITEM_INDEXLABEL;
		while ( *line == '.' ){
			line++;
			item->layer++;
		}
	}else{
		while ( *line == '\t' ){
			line++;
			item->layer++;
		}
	}
	param = tstrchr(line,'\x1');
	if ( param != NULL ){ // ２カ国語記載有り
		if ( UseLcid == LCID_PPXDEF ){
			tstrcpy(item->itemname,param + 1);
			itemname = item->itemname;
		}else{
			tstrcpy(item->itemname,line);
			itemname = item->itemname + (param - line);
			*itemname++ = '\0';
		}
	}else{
		tstrcpy(item->itemname,line);
		itemname = item->itemname;
	}
									// 情報の確認
	key = tstrchr(itemname,':');
	if ( key == NULL ) return; // なし
									// keyname 取得
	*key++ = '\0';
	param = tstrchr(key,'=');
	if ( param == NULL ) return;
	*param++ = '\0';
	item->keyname = key;
									// offset 取得
	offset = tstrchr(key,'+');
	if ( offset != NULL ){
		*offset++ = '\0';
		item->offset = GetNumber((const TCHAR **)&offset);
	}
									// subname 取得
	subkey = tstrchr(key,':');
	if ( subkey == NULL ){
		item->subname = NULL;
	}else{
		*subkey++ = '\0';
		item->subname = subkey;
	}
									// 中身の取得はしないか確認
	if ( getdata == FALSE ){
		item->itemtype = ITEM_RESERVED;
		return;
	}
									// 各項目の処理
	switch ( *param++ ){
		case '?':
			item->edittype = EDIT_CHECK;
			item->itemtype = ITEM_DWORD;
			GetCustItemType(item,&param);
			if ( *param == 'B' ){
				param++;
				item->d.num.mask = 0;
			}else{
				item->d.num.mask = IITEM_VALUE;
			}
			GetCustItemData(item,&param);
			if ( item->itemtype == ITEM_STRING ){
				TCHAR *sepp;

				sepp = tstrchr(param,':');
				if ( sepp != NULL ){
					*(item->d.str.setdata + (sepp - param)) = '\0';
					param = sepp;
				}
			}

			if ( *param == ':' ){
				param++;
				item->d.num.falsedata = GetNumber((const TCHAR **)&param);
			}else{
				item->d.num.falsedata = 0;
			}
			if ( item->d.num.mask == 0 ){
				item->d.num.truedata = LSBIT << item->d.num.truedata;
				item->d.num.mask = ~item->d.num.truedata;
				if ( *param == '!' ){
					param++;
					item->d.num.falsedata = 1;
				}
			}
			break;
		case '@':
			item->edittype = EDIT_RADIO;
			item->itemtype = ITEM_DWORD;
			GetCustItemType(item,&param);
			GetCustItemData(item,&param);
			break;
		case '<':
			item->edittype = EDIT_EDIT;
			item->itemtype = ITEM_STRING;
			GetCustItemType(item,&param);
			GetCustItemData(item,&param);
			break;
		case 'K':
			item->edittype = EDIT_KEY;
			item->itemtype = ITEM_KEY;
			item->d.num.data = 0;
			GetData(item,&item->d.num.data,sizeof(WORD));
			break;
		case 'N':
			item->edittype = EDIT_FILE;
			item->itemtype = ITEM_STRING;
			GetCustItemType(item,&param);
			GetCustItemData(item,&param);
			if ( *param == 'D' ) item->option = ITEMOPTION_NOMACROSTR;
			break;
		case 'P':
			item->edittype = EDIT_DIR;
			item->itemtype = ITEM_STRING;
			GetCustItemType(item,&param);
			GetCustItemData(item,&param);
			if ( *param == 'D' ) item->option = ITEMOPTION_NOMACROSTR;
			break;
		case 'F':
			item->edittype = EDIT_FONT;
			item->itemtype = ITEM_FONT;
			item->d.fontdata.font = DefaultFont;
			item->d.fontdata.dpi = 0;
			tstrcpy(item->d.fontdata.font.lfFaceName,GetCText(DefaultFaceName));
			GetData(item,&item->d.fontdata,sizeof(item->d.fontdata));
			break;
		case 'c':
			item->edittype = EDIT_ppchotkey;
			item->itemtype = ITEM_KEY;
			item->d.num.data = 0;
			SearchPPcHotKey(item);
			break;
//		default:
	}
}

void MakeEditText(TCHAR *dest,ITEMSTRUCT *item)
{
	switch ( item->itemtype ){
		case ITEM_INT:
			wsprintf(dest,T("%d"),item->d.num.data);
			break;
		case ITEM_DWORD:
		case ITEM_WORD:
		case ITEM_BYTE:
			wsprintf(dest,T("%u"),item->d.num.data);
			break;
		case ITEM_STRING:
			tstrcpy(dest,item->d.str.data);
			break;
		case ITEM_KEY:
			if ( item->d.num.data ){
				MakeKeyDetailText(item->d.num.data,dest,FALSE);
			}else{
				dest[0] = '\0';
			}
			break;
		case ITEM_FONT: {
			int pix,pt;

			pix = item->d.fontdata.font.lfHeight;
			if ( pix == 0 ){
				tstrcpy(dest,item->d.fontdata.font.lfFaceName);
			}else{
				if ( tstrcmp(item->keyname,T("F_dlg")) == 0 ){
					pix = (item->d.fontdata.font.lfHeight * DEFAULT_WIN_DPI) / DEFAULT_DTP_DPI;
					pt = item->d.fontdata.font.lfHeight;
				}else{
					pix = item->d.fontdata.font.lfHeight;
					pt = (item->d.fontdata.font.lfHeight * DEFAULT_DTP_DPI) / DEFAULT_WIN_DPI;
				}
				wsprintf(dest,T("%s,%dpt(%d)"),
						item->d.fontdata.font.lfFaceName,
						(pt >= 0) ? pt : -pt,pix);
			}
			break;
		}
		default:
			dest[0] = '\0';
			break;
	}
}

int MakeDisplayText(TCHAR *dest,ITEMSTRUCT *item,TV_ITEM *tvi)
{
	LABELICONS state = ICON_LABEL;
	TCHAR *foot = NULL,footbuf[0x1000],destbuf[0x1000];

	switch(item->edittype){
		case EDIT_CHECK:
			if ( item->itemtype == ITEM_STRING ){ // 文字列変更
				state = !tstrcmp(item->d.str.data,item->d.str.setdata) ?
					ICON_CHECKON : ICON_CHECKOFF;
			}else if ( item->d.num.mask == IITEM_VALUE ){	// 値変更
				state = (item->d.num.data == item->d.num.falsedata) ?
					ICON_CHECKOFF : ICON_CHECKON;
			}else{							// bit 変更
				state = (item->d.num.data & item->d.num.truedata) ?
					ICON_CHECKON : ICON_CHECKOFF;
				if ( item->d.num.falsedata ){
					state = (state == ICON_CHECKOFF) ?
						ICON_CHECKON : ICON_CHECKOFF;
				}
			}
			break;

		case EDIT_RADIO:
			switch ( item->itemtype ){
				case ITEM_INT:
				case ITEM_DWORD:
				case ITEM_WORD:
				case ITEM_BYTE:
					state = ( item->d.num.data == item->d.num.truedata ) ?
							ICON_RADIOON : ICON_RADIOOFF;
					break;
				case ITEM_STRING:
					state = !tstrcmp(item->d.str.data,item->d.str.setdata) ?
							ICON_RADIOON : ICON_RADIOOFF;
					break;
//				default:
			}
			break;
		case EDIT_FONT:
		case EDIT_FILE:
		case EDIT_DIR:
		case EDIT_EDIT:
		case EDIT_KEY:
		case EDIT_ppchotkey:
			state = ICON_BUTTON;
			if ( IsTrue(item->edit) ) break;
			foot = footbuf;
			MakeEditText(destbuf,item);
			wsprintf(footbuf,T(" [ %s ]"),destbuf);
			break;
//		default:
	}
	tstrcpy(dest,item->itemname);
	if ( foot != NULL ) tstrcat(dest,foot);
	tvi->iImage = tvi->iSelectedImage = state;
	setflag(tvi->mask,TVIF_IMAGE | TVIF_SELECTEDIMAGE);
	return state;
}


void LoadListTree(BOOL itemtree)
{
	HWND hTwnd;
	HTREEITEM hParent[maxlayer + 1]; // 0:Root 1:分類 2:項目 3:選択肢
	HTREEITEM hFirst = NULL;
	int linecount = 1;
	const TCHAR *line,*lines;
	int layer,oldlayer = maxlayer;

	hParent[0] = TVI_ROOT; // 右ツリーの時はこちらが root
	hParent[1] = TVI_ROOT; // 左ツリーの時はこちらが root
	if ( IsTrue(itemtree) ){ // 右ツリーを表示
		hTwnd = hItemTreeWnd;
		lines = CustomItemList;
		linecount = CustomItemListOffset;
		SendMessage(hTwnd,WM_SETREDRAW,FALSE,0);
		SendMessage(hTwnd,TVM_SELECTITEM,TVGN_CARET,(LPARAM)NULL); // 選択解除(TVN_SELCHANGED が連続発行されないように)
		TreeView_DeleteAllItems(hTwnd);
	}else{ // 左(+右)ツリーの表示
		hTwnd = hIndexTreeWnd;
		lines = CustomList;
		SendMessage(hTwnd,WM_SETREDRAW,FALSE,0);
	}

	while ( *lines ){
		TCHAR dest[0x1000];
		ITEMSTRUCT item;
		TV_ITEM tvi;
		TV_INSERTSTRUCT tvins;

		line = lines;
		GetCustItem(&item,line,itemtree); // itemtree なら内容も取得
		if ( IsTrue(itemtree) ){ // item のときは、次のIndexで終了
			if ( item.itemtype == ITEM_INDEXLABEL ) break;
			item.layer--;
		}else{ // indexのときは、Itemをスキップ
			if ( item.itemtype != ITEM_INDEXLABEL ) goto next;
		}
		layer = item.layer;
		if ( layer > maxlayer ){
#ifndef RELEASE
			XMessage(hTwnd,NULL,XM_DbgDIA,T("Flow layer %d"),linecount);
#endif
			goto next;
		}

		tvi.mask = TVIF_TEXT | TVIF_PARAM;
		MakeDisplayText(dest,&item,&tvi);

		if ( (layer == 1) && (tvi.iImage == ITEM_INDEXLABEL) ){
			tvi.iImage = tvi.iSelectedImage = ICON_DOCKCLOSE;
		}

		tvi.pszText = dest;
		tvi.cchTextMax = tstrlen32(dest);
		tvi.lParam = (LPARAM)linecount;

		tvins.hParent = hParent[layer - 1];
		tvins.hInsertAfter = 0;
		TreeInsertItemValue(tvins) = tvi;
		hParent[layer] = (HTREEITEM)SendMessage(hTwnd,TVM_INSERTITEM,
								0,(LPARAM)(LPTV_INSERTSTRUCT)&tvins);
		if ( hFirst == NULL ) hFirst = hParent[layer];
		if ( layer > oldlayer ) TreeView_Expand(hTwnd,tvins.hParent,TVE_EXPAND);
		oldlayer = layer;
next:
		linecount++;
		lines += tstrlen(lines) + 1;
		if ( *lines == '\n' ) lines++;
	}
	SendMessage(hTwnd,TVM_SELECTITEM,TVGN_FIRSTVISIBLE,(LPARAM)hFirst);
	SendMessage(hTwnd,WM_SETREDRAW,TRUE,0);
	InvalidateRect(hTwnd,NULL,TRUE);
}

#ifndef TVM_SETITEMHEIGHT
	#define TVM_SETITEMHEIGHT (TV_FIRST + 27)
	#define TVM_GETITEMHEIGHT (TV_FIRST + 28)
#endif

void EnterTouchMode(HWND hDlg)
{
	int dpi = (int)PPxCommonExtCommand(K_GETDISPDPI,(WPARAM)hDlg);
	int nowheight = (int)SendMessage(hItemTreeWnd,TVM_GETITEMHEIGHT,0,0);
	int minheight = (dpi * 60) >> 8; // 6.0mm ( 60 / 254 ) の近似値

	if ( nowheight < minheight ){
		SendMessage(hIndexTreeWnd,TVM_SETITEMHEIGHT,minheight,0);
		SendMessage(hItemTreeWnd,TVM_SETITEMHEIGHT,minheight,0);
	}
}

#ifndef WM_POINTERUP
#define WM_POINTERUP 0x0247
#define POINTER_FLAG_INCONTACT 4
#endif

void USEFASTCALL CheckTouch(HWND hWnd,UINT iMsg/*,WPARAM wParam*/)
{
	if ( // ((iMsg == WM_POINTERUP) && (X_pmc[0] < 0) && (wParam & POINTER_FLAG_INCONTACT)) ||
		 ((iMsg == WM_GESTURE) && (X_pmc[0] < 0))
	){
		EnterTouchMode(GetParent(hWnd));
	}
}

LRESULT CALLBACK IndexTreeProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	CheckTouch(hWnd,iMsg);
	return CallWindowProc(OldIndexTreeProc,hWnd,iMsg,wParam,lParam);
}

LRESULT CALLBACK ItemTreeProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	CheckTouch(hWnd,iMsg);
	return CallWindowProc(OldItemTreeProc,hWnd,iMsg,wParam,lParam);
}

#ifndef SM_MAXIMUMTOUCHES
 #define SM_MAXIMUMTOUCHES 95
#endif

void LoadList(HWND hDlg)	// チェックボックス等の画像を用意
{
	HIMAGELIST hImage;
	int IconSize;

	CustomList = LoadTextResource(MAKEINTRESOURCE(DEFCUSTLIST));
	hIndexTreeWnd = GetDlgItem(hDlg,IDT_GENERAL);
	hItemTreeWnd = GetDlgItem(hDlg,IDT_GENERALITEM);

	GetCustData(T("X_pmc"),&X_pmc,sizeof(X_pmc));
	if ( X_pmc[0] > 0 ){
		EnterTouchMode(hDlg);
	}else if ( X_pmc[0] < 0 ){
		if ( GetSystemMetrics(SM_MAXIMUMTOUCHES) != 0 ){
			OldIndexTreeProc =(WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg,
					IDT_GENERAL),GWLP_WNDPROC,(LONG_PTR)IndexTreeProc);
			OldItemTreeProc =(WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg,
					IDT_GENERALITEM),GWLP_WNDPROC,(LONG_PTR)ItemTreeProc);
		}
	}

	IconSize = GetSystemMetrics(SM_CXICON) / 2;
	if ( (IconSize >= 24) && (OSver.dwMajorVersion >= 6) ){
	#if 0
		// 画像の拡縮で対応する案
		HBITMAP hOrgBMP,hBigBMP;
		HDC hDC,hOrgMemDC,hBigMemDC;
		HGDIOBJ hOldOrgBmp,hOldBigBmp,hOldFont;
		#define bitcolor 4
		int i;
		NONCLIENTMETRICS ncm;
		HFONT hControlFont;
		TEXTMETRIC tm;

		ncm.cbSize = sizeof(ncm);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0);

		hDC = GetDC(hDlg);
		hOrgMemDC = CreateCompatibleDC(hDC);
		hBigMemDC = CreateCompatibleDC(hDC);

		hControlFont = CreateFontIndirect(&ncm.lfStatusFont);
		hOldFont = SelectObject(hOrgMemDC,hControlFont);
		GetTextMetrics(hOrgMemDC,&tm);
		SelectObject(hOrgMemDC,hOldFont);
		DeleteObject(hControlFont);
//		tm.tmHeight = 32;

		hImage = ImageList_Create(tm.tmHeight,tm.tmHeight,bitcolor,TreeImageCharCount,TreeImageCharCount);

		hOrgBMP = LoadBitmap(hInst,MAKEINTRESOURCE(BUTTONIMAGE));
		hBigBMP = CreateCompatibleBitmap(hDC,tm.tmHeight,tm.tmHeight);

		hOldOrgBmp = SelectObject(hOrgMemDC,hOrgBMP);

		for ( i = 0 ; i < TreeImageCharCount ; i++ ){
			hOldBigBmp = SelectObject(hBigMemDC,hBigBMP);
			StretchBlt(hBigMemDC,0,0,tm.tmHeight,tm.tmHeight,
					   hOrgMemDC,16 * i,0,16,16,SRCCOPY);
			SelectObject(hBigMemDC,hOldBigBmp);
			ImageList_AddMasked(hImage,hBigBMP,RGB(255,0,255));
		}
		SelectObject(hOrgMemDC,hOldOrgBmp);

		DeleteObject(hBigBMP);
		DeleteObject(hOrgBMP);

		DeleteDC(hBigMemDC);
		DeleteDC(hOrgMemDC);
		ReleaseDC(hDlg,hDC);
	#else
		// 文字で対応する案
		HBITMAP hBMP;
		HDC hDC,hMemDC;
		RECT box;
		HGDIOBJ hOldBmp,hOldFont;
		#define bitcolor 4
		int i;
		NONCLIENTMETRICS ncm;
		HFONT hControlFont;
		TEXTMETRIC tm;

		ncm.cbSize = sizeof(ncm);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0);

		hDC = GetDC(hDlg);
		hMemDC = CreateCompatibleDC(hDC);

		hControlFont = CreateFontIndirect(&ncm.lfStatusFont);
		hOldFont = SelectObject(hMemDC,hControlFont);
		GetTextMetrics(hMemDC,&tm);
		if ( tm.tmHeight == 0 ){
			tm.tmHeight = GetSystemMetrics(SM_CYMENU) - 5;
		}

		hImage = ImageList_Create(tm.tmHeight,tm.tmHeight,bitcolor,TreeImageCharCount,TreeImageCharCount);

		hBMP = CreateCompatibleBitmap(hMemDC,tm.tmHeight,tm.tmHeight);
		box.left = box.top = 0;
		box.right = box.bottom = tm.tmHeight;

		for ( i = 0 ; i < TreeImageCharCount ; i++ ){
			hOldBmp = SelectObject(hMemDC,hBMP);
			FillRect(hMemDC,&box,GetStockObject(WHITE_BRUSH));
			TextOutW(hMemDC,0,0,&TreeImageChars[i],1);
			SelectObject(hMemDC,hOldBmp);
			ImageList_Add(hImage,hBMP,NULL);
		}
		DeleteObject(hBMP);
		SelectObject(hMemDC,hOldFont);
		DeleteObject(hControlFont);
		DeleteDC(hMemDC);
		ReleaseDC(hDlg,hDC);
	#endif
	}else{ // 内蔵画像を使用する
		hImage = ImageList_LoadImage(hInst,MAKEINTRESOURCE(BUTTONIMAGE),
				16,0,RGB(255,0,255),IMAGE_BITMAP,LR_DEFAULTCOLOR);
	}
	SendMessage(hItemTreeWnd,TVM_SETIMAGELIST,(WPARAM)TVSIL_NORMAL,(LPARAM)hImage);

	LoadListTree(FALSE);
/*  ↑で右側も表示されるので不要
	CustomItemListOffset = 2;
	CustomItemList = SearchList(2);
	LoadListTree(TRUE);
*/
	hEditWnd = CreateWindow(EDIT,NilStr,
					WS_BORDER | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
					0,0,FLOAT_WIDTH,FLOAT_HEIGHT,hDlg,(HMENU)IDE_FLOAT,
					hInst,NULL);
	hSpinWnd = CreateWindow(T("msctls_updown32"),NilStr,
					UDS_SETBUDDYINT | UDS_AUTOBUDDY | UDS_ARROWKEYS |
					UDS_NOTHOUSANDS |
					WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
					0,0,FLOAT_WIDTH,FLOAT_HEIGHT,hDlg,NULL,hInst,NULL);
	SendMessage(hSpinWnd,UDM_SETRANGE,0,TMAKELPARAM(0,0x7fff));
	SendMessage(hSpinWnd,UDM_SETRANGE32,0,0x7fffffff);
	hButtonWnd = CreateWindow(BUTTON,GetCText(T("参照\0ref")),
					WS_BORDER | WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
					0,0,FLOAT_BUTTONWIDTH,FLOAT_HEIGHT,hDlg,
					(HMENU)IDB_FLOAT,hInst,NULL);
}

void DispFix(HWND hTwnd,HTREEITEM titem,ITEMSTRUCT *item)
{
	TCHAR disp[0x1000];
	TV_ITEM tvi;

	tvi.mask = TVIF_TEXT;
	MakeDisplayText(disp,item,&tvi);
	tvi.hItem = titem;
	tvi.pszText = disp;
	tvi.cchTextMax = tstrlen32(disp);
	TreeView_SetItem(hTwnd,&tvi);
}

void ModifyEditItem(HWND hWnd)
{
	TCHAR editbuf[VFPS * 2];
	const TCHAR *p;

	GetWindowText(hEditWnd,editbuf,TSIZEOF(editbuf));
	switch ( edititem.itemtype ){
		case ITEM_INT:
		case ITEM_DWORD:
		case ITEM_WORD:
		case ITEM_BYTE:
			p = editbuf;
			edititem.d.num.data = GetNumber(&p);
			SetData(&edititem,&edititem.d.num.data,edititem.d.num.size);
			break;
		case ITEM_STRING:
			if ( edititem.option == ITEMOPTION_NOMACROSTR ){
				if ( tstrchr(editbuf,'%') != NULL ){
					SetDlgItemText(hWnd,IDE_FIND,GetCText(WarnNoMacroMsg));
				}else{
					SetDlgItemText(hWnd,IDE_FIND,NilStr);
				}
			}
			tstrcpy(edititem.d.str.data,editbuf);
			SetData(&edititem,&edititem.d.str.data,
					TSTRSIZE32(edititem.d.str.data));
			break;
//		default:
	}
}

void RefreshItems(HWND hTwnd,ITEMSTRUCT *item,HTREEITEM hItem1)
{
	HTREEITEM hItem;
	TV_ITEM tvi;
	ITEMSTRUCT enumitem;
	const TCHAR *p;

	if ( item->layer <= 2 ){
		DispFix(hTwnd,hItem1,item);
		return;
	}
	hItem = TreeView_GetParent(hTwnd,hItem1);
	hItem = TreeView_GetChild(hTwnd,hItem);

	while ( hItem != NULL ){
		tvi.hItem = hItem;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTwnd,&tvi);
		p = SearchList((int)tvi.lParam);
		if ( p == NULL ) break;
		GetCustItem(&enumitem,p,TRUE);
		DispFix(hTwnd,hItem,&enumitem);
		hItem = TreeView_GetNextSibling(hTwnd,hItem);
	}
}

void CloseEdit(HWND hDlg,HWND hTwnd)
{
	if ( !useedit ) return;
	ShowWindow(hEditWnd,SW_HIDE);
	ShowWindow(hSpinWnd,SW_HIDE);
	ShowWindow(hButtonWnd,SW_HIDE);
	useedit = 0;
	edititem.edit = FALSE;
	ModifyEditItem(hDlg);
	RefreshItems(hTwnd,&edititem,hedititem);
}

void SetEditWindow(HWND hDlg,HWND hTwnd,ITEMSTRUCT *item,HTREEITEM hTitem)
{
	RECT box;
	POINT pos;

	item->edit = TRUE;
	DispFix(hTwnd,hTitem,item);
	TreeView_GetItemRect(hTwnd,hTitem,&box,TRUE);
	pos.x = box.right;
	pos.y = (box.top + box.bottom) / 2 - (FLOAT_HEIGHT / 2);
	ClientToScreen(hTwnd,&pos);
	ScreenToClient(hDlg,&pos);

	SetWindowPos(hEditWnd,HWND_TOP,pos.x,pos.y,0,0,SWP_NOSIZE | SWP_SHOWWINDOW);
	if ( item->edittype != EDIT_EDIT ){
		SetWindowPos(hButtonWnd,HWND_TOP,
				pos.x + FLOAT_WIDTH,pos.y,0,0,SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
	}else{
		if ( item->itemtype != ITEM_STRING ){
			SetWindowPos(hSpinWnd,HWND_TOP,
				pos.x + FLOAT_WIDTH,pos.y,0,0,SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
		}
	}
	PostMessage(hDlg,WM_COMMAND,IDM_SETEDITFOCUS,0);
}

void SelectItem_Font(HWND hDlg,HWND hTwnd,HTREEITEM titem,ITEMSTRUCT *item)
{
	CHOOSEFONT cfont;

// ※仮想中はdpiの変化無し
	memset(&cfont,0,sizeof(CHOOSEFONT));
	cfont.lStructSize = sizeof(CHOOSEFONT);
	cfont.hwndOwner = hDlg;
	// cfont.hDC = NULL;
	cfont.lpLogFont = &item->d.fontdata.font;
	cfont.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_FORCEFONTEXIST |
			CF_NOVERTFONTS | CF_INITTOLOGFONTSTRUCT;
	// cfont.rgbColors = 0;

	// (未設定) なら、初期値を設定する
	if ( item->d.fontdata.font.lfFaceName[0] == '(' ){
		const TCHAR *pos = tstrchr(StrFontList,item->keyname[2]);

		item->d.fontdata.font.lfFaceName[0] = '\0';
		if ( pos != NULL ){
			GetPPxFont((int)(pos - StrFontList),PPxCommonExtCommand(K_GETDISPDPI,(WPARAM)hDlg),&item->d.fontdata);
		}
	}
	// ダイアログは Point を Pixel に変換する
	if ( tstrcmp(item->keyname,T("F_dlg")) == 0 ){
		item->d.fontdata.font.lfHeight = (item->d.fontdata.font.lfHeight * DEFAULT_WIN_DPI) / DEFAULT_DTP_DPI;
	}

	if ( IsTrue(ChooseFont(&cfont)) ){
		// ダイアログは Pixel を Point に変換する
		if ( tstrcmp(item->keyname,T("F_dlg")) == 0 ){
			item->d.fontdata.font.lfHeight = (item->d.fontdata.font.lfHeight * DEFAULT_DTP_DPI) / DEFAULT_WIN_DPI;
		}
		item->d.fontdata.dpi = PPxCommonExtCommand(K_GETDISPDPI,(WPARAM)hDlg);
		SetData(item,&item->d.fontdata,sizeof(item->d.fontdata));
		RefreshItems(hTwnd,item,titem);
		Changed(hDlg);
	}
}

BOOL SelectItem(HWND hDlg,HWND hTwnd,HTREEITEM titem,int linenumber)
{
	ITEMSTRUCT item;
	const TCHAR *listp;

	listp = SearchList(linenumber);
	if ( listp == NULL ) return FALSE;
	GetCustItem(&item,listp,TRUE);
	switch ( item.edittype ){
		case EDIT_NONE:			// ラベル／折り畳み処理
			if ( (item.itemtype == ITEM_INDEXLABEL) &&
				(CustomItemListOffset != (linenumber + 1)) ){
				CustomItemListOffset = linenumber + 1;
				CustomItemList = SearchList(linenumber + 1);
				LoadListTree(TRUE);
				return TRUE;
			}

			if ( item.layer == 1 ){
				TreeView_Expand(hTwnd,titem,TVE_TOGGLE);
			}
			return TRUE;

		case EDIT_CHECK:		// チェックボックス／更新も行う
			if ( item.itemtype == ITEM_STRING ){
				if ( !tstrcmp(item.d.str.data,item.d.str.setdata) ){
					wsprintf(item.d.str.data,T("%d"),item.d.num.falsedata);
				}else{
					tstrcpy(item.d.str.data,item.d.str.setdata);
				}
				SetData(&item,&item.d.str.data,TSTRSIZE32(item.d.str.data));
			}else{
				if ( item.d.num.mask == IITEM_VALUE ){	// 値変更
					item.d.num.data = ( item.d.num.data == item.d.num.falsedata ) ?
						item.d.num.truedata : item.d.num.falsedata;
				}else{							// bit 変更
					item.d.num.data ^= item.d.num.truedata;
				}
				SetData(&item,&item.d.num.data,item.d.num.size);
			}
			RefreshItems(hTwnd,&item,titem);
			Changed(hDlg);
			return TRUE;

		case EDIT_RADIO:		// ラジオボックス／更新も行う
			switch ( item.itemtype ){
				case ITEM_INT:
				case ITEM_DWORD:
				case ITEM_WORD:
				case ITEM_BYTE:
					if ( item.d.num.data == item.d.num.truedata ) return FALSE;
					Changed(hDlg);
					item.d.num.data = item.d.num.truedata;
					SetData(&item,&item.d.num.data,item.d.num.size);
					break;
				case ITEM_STRING:
					if ( !tstrcmp(item.d.str.data,item.d.str.setdata) ){
						return FALSE;
					}
					Changed(hDlg);
					tstrcpy(item.d.str.data,item.d.str.setdata);
					SetData(&item,&item.d.str.data,TSTRSIZE32(item.d.str.data));
					break;
//				default:
			}
			RefreshItems(hTwnd,&item,titem);
			break;
		case EDIT_FILE:			// ファイル名ボックス／編集開始
		case EDIT_DIR:			// ディレクトリ／編集開始
		case EDIT_EDIT: {		// エディットボックス／編集開始
			TCHAR text[0x1000];

			SendMessage(hEditWnd,EM_LIMITTEXT,VFPS - 1,0);
			MakeEditText(text,&item);
			SetWindowText(hEditWnd,text);

			SetEditWindow(hDlg,hTwnd,&item,titem);

			useedit = linenumber;
			hedititem = titem;
			edititem = item;
			edititem.keyname = edititem.itemname +
					(item.keyname - item.itemname);
			if( item.subname ){
				edititem.subname = edititem.itemname +
						(item.subname - item.itemname);
			}
			break;
		}
		case EDIT_KEY:
		case EDIT_ppchotkey:
		{							// キー／編集＆更新
			TCHAR temp[64];
			const TCHAR *p;

			PutKeyCode(temp,item.d.num.data);
			if ( KeyInput(GetParent(hDlg),temp) <= 0 ){
				if ( item.edittype != EDIT_ppchotkey ){
					return TRUE;
				}
				// EDIT_ppchotkey で、既に割当てがあるときは削除するか確認
				if ( (item.d.num.def == 0) ||
					 (PMessageBox(hDlg,GetCText(DelKeyMsg),StrCustTitle,MB_YESNO) != IDYES) ){
					return TRUE;
				}
				item.d.num.data = 0;
			}else{
				p = temp;
				item.d.num.data = (WORD)GetKeyCode(&p);
			}
			if ( item.edittype == EDIT_ppchotkey ){
				SetPPcHotKey(&item);
			}else{
				SetData(&item,&item.d.num.data,sizeof item.d.num.data);
			}
			RefreshItems(hTwnd,&item,titem);
			Changed(hDlg);
			return TRUE;
		}
		case EDIT_FONT:		// フォント／編集＆更新
			SelectItem_Font(hDlg,hTwnd,titem,&item);
			return TRUE;

//		default:
	}
	return FALSE;
}

void TreeHelp(HWND hDlg,HWND hTwnd)
{
	TV_ITEM tvi;
	ITEMSTRUCT item;
	const TCHAR *p;
	TV_HITTESTINFO hit;

	GetMessagePosPoint(hit.pt);
	ScreenToClient(hTwnd,&hit.pt);
	tvi.hItem = TreeView_HitTest(hTwnd,&hit);

	tvi.mask = TVIF_PARAM;
	TreeView_GetItem(hTwnd,&tvi);

	p = SearchList((int)(tvi.lParam));
	if ( p == NULL ) return;

	GetCustItem(&item,p,FALSE);
	if ( item.keyname == NULL ){
		int layer;

		layer = item.layer;
		if ( layer == 1 ) return;
		p = SearchList((int)(tvi.lParam) + 1);
		if ( p == NULL ) return;
		GetCustItem(&item,p,FALSE);
		if ( item.keyname == NULL ) return;
		if ( item.layer != (layer + 1) ) return;
	}
	if ( (item.keyname[0] == 'F') && (item.keyname[1] == '_') ){
		HMENU hPopMenu;
		POINT pos;
		int index;

		hPopMenu = CreatePopupMenu();
		AppendMenu(hPopMenu,MF_ES,1,GetCText(StrMenuDefault));
		AppendMenu(hPopMenu,MF_ES,2,GetCText(StrMenuHelp));
		GetMessagePosPoint(pos);
		index = TrackPopupMenu(hPopMenu,TPM_TDEFAULT,pos.x,pos.y,0,hTwnd,NULL);
		DestroyMenu(hPopMenu);
		switch ( index ){
			case 1: // 初期化
				if ( PMessageBox(hTwnd,GetCText(StrFontDefault),StrCustTitle,MB_YESNO) == IDYES ){
					DeleteCustData(item.keyname);
					GetCustItem(&item,p,TRUE);
					RefreshItems(hTwnd,&item,tvi.hItem);
					Changed(hDlg);
				}
				return;

			case 2: // help
				break;

			default:
				return;
		}
	}
	PPxHelp(hTwnd,HELP_KEY,(DWORD_PTR)item.keyname);
}

void ModifyItem2(HWND hDlg,HWND hTwnd)
{
	TV_ITEM tvi;

	tvi.hItem = TreeView_GetSelection(hTwnd);
	tvi.mask = TVIF_PARAM;
	TreeView_GetItem(hTwnd,&tvi);
	SelectItem(hDlg,hTwnd,tvi.hItem,(int)(tvi.lParam));
}

BOOL TreeNotify(HWND hDlg,NMHDR *nmh)
{
	switch (nmh->code){
		case PSN_SETACTIVE:
			InitWndIcon(hDlg,IDB_TEST);
			break;

		case PSN_APPLY:
			CloseEdit(hDlg,hItemTreeWnd);
		// PSN_HELP へ
		case PSN_HELP:
			StyleDlgProc(hDlg,WM_NOTIFY,IDD_GENERAL,(LPARAM)nmh);
			break;

		case NM_CLICK:
			if ( nmh->hwndFrom == hItemTreeWnd ){
			// おまじない。この時点ではまだカーソル位置のアイテムが
			// 選択されていない。
				PostMessage(hDlg,WM_COMMAND,IDM_RCLICKED,(LPARAM)nmh->hwndFrom);
			}
			break;

		case NM_RCLICK:
			TreeHelp(hDlg,nmh->hwndFrom);
			break;

		case NM_DBLCLK:
			ModifyItem2(hDlg,nmh->hwndFrom);
			break;

		case TVN_KEYDOWN:
			switch ( ((TV_KEYDOWN *)nmh)->wVKey ){
				case VK_F1:
					TreeHelp(hDlg,nmh->hwndFrom);
					break;

				case VK_SPACE:
					ModifyItem2(hDlg,nmh->hwndFrom);
					break;
			}
			break;

		case TVN_SELCHANGED:
			if ( nmh->hwndFrom == hIndexTreeWnd ){
				PostMessage(hDlg,WM_COMMAND,IDM_RCLICKED,(LPARAM)nmh->hwndFrom);
			}
			break;

//		default:
	}
	return 0;
}

void PushFloatButton_File(HWND hDlg)
{
	TCHAR Name[VFPS];
	TCHAR Path[VFPS],*p;

	OPENFILENAME ofile = {sizeof(ofile),NULL,NULL,GetFileExtsStr,NULL,0,0,
		NULL,TSIZEOF(Name),NULL,0,NULL,NULL,OFN_HIDEREADONLY | OFN_SHAREAWARE,
		0,0,NilStr,0,NULL,NULL OPENFILEEXTDEFINE };

	ofile.hwndOwner = hDlg;
	ofile.lpstrFile = Name;
	ofile.lpstrInitialDir = Path;

	GetWindowText(hEditWnd,Path,TSIZEOF(Path));
	p = (Path[0] == '\"') ? (Path + 1) : Path;
	p = VFSFindLastEntry(p);
	tstrcpy(Name,(*p == '\\') ? p + 1 : p);
	*p = '\0';
	p = tstrchr(Name,'\"');
	if ( p != NULL ) *p = '\0';
	if ( IsTrue(GetOpenFileName(&ofile)) ){
		if ( (edititem.keyname[0] == 'A') && (tstrchr(Name,' ') != NULL) ){
			VFSFullPath(NULL,Name,Path);
			wsprintf(Path,T("\"%s\""),Name);
		}else{
			VFSFullPath(Path,Name,Path);
		}
		SetWindowText(hEditWnd,Path);
	}
	RefreshItems(hIndexTreeWnd,&edititem,hedititem);
}

void PushFloatButton_Dir(HWND hDlg)
{
	TCHAR Path[VFPS];

	TINPUT tinput;

	tinput.hOwnerWnd	= hDlg;
	tinput.hWtype		= PPXH_DIR;
	tinput.hRtype		= PPXH_DIR_R;
	tinput.title		= NilStr;
	tinput.buff			= Path;
	tinput.size			= TSIZEOF(Path);
	tinput.flag			= TIEX_REFTREE | TIEX_SINGLEREF | TIEX_REFMODE;
	tinput.firstC		= 0;
	tinput.lastC		= EC_LAST;

	GetWindowText(hEditWnd,Path,TSIZEOF(Path));

	if ( tInputEx(&tinput) > 0 ) SetWindowText(hEditWnd,Path);
	RefreshItems(hIndexTreeWnd,&edititem,hedititem);
}

#if !NODLL
TCHAR *tstristr(const TCHAR *target,const TCHAR *findstr)
{
	size_t len,flen;
	const TCHAR *p,*tagetmax;

	flen = tstrlen(findstr);
	len = tstrlen(target);
	tagetmax = target + len - flen;

#ifdef UNICODE
	for ( p = target ; p <= tagetmax ; p++ ){
#else
	for ( p = target ; p <= tagetmax ; p += Chrlen(*p) ){
#endif
		if ( !tstrnicmp(p,findstr,flen) ){
			return (TCHAR *)p;
		}
	}
	return NULL;
}
#endif

BOOL FindItem(HWND hTreeWnd,HTREEITEM hItem,int findline,HTREEITEM *hFoundItem)
{
	while ( hItem != NULL ){
		TV_ITEM tvi;

		tvi.hItem = hItem;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTreeWnd,&tvi);

		if ( tvi.lParam == findline ){
			*hFoundItem = hItem;
			SendMessage(hTreeWnd,TVM_SELECTITEM,TVGN_CARET,(LPARAM)hItem);
			return TRUE;
		}
		if ( IsTrue(FindItem(hTreeWnd,TreeView_GetChild(hTreeWnd,hItem),findline,hFoundItem)) ){
			return TRUE;
		}
		hItem = TreeView_GetNextSibling(hTreeWnd,hItem);
	}
	return FALSE;
}

void FindKeyword(HWND hDlg)
{
	TCHAR findtext[VFPS];
	TV_ITEM tvi;
	int firstline = 1;
	const TCHAR *p;

	int line,indexline;
	HTREEITEM hItem;

	findtext[0] = '\0';
	if ( FirstTypeName != NULL ){
		tstrcpy(findtext,FirstTypeName);
		FirstTypeName = NULL;
		FirstItemName = NULL;
		SetFocus(hItemTreeWnd);
	}else{
		GetControlText(hDlg,IDE_FIND,findtext,TSIZEOF(findtext));
	}
	if ( findtext[0] == '\0' ){
		SetFocus(GetDlgItem(hDlg,IDE_FIND));
		return;
	}
	// index の位置を求める
	tvi.hItem = TreeView_GetSelection(hIndexTreeWnd);
	if ( tvi.hItem != NULL ){
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hIndexTreeWnd,&tvi);
		indexline = (int)tvi.lParam;
	}else{
		indexline = 1;
	}
	// item の位置を求める
	tvi.hItem = TreeView_GetSelection(hItemTreeWnd);
	if ( tvi.hItem != NULL ){
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hItemTreeWnd,&tvi);
		p = SearchList((int)tvi.lParam);
		if ( (p != NULL) && (tstristr(p,findtext) != NULL) ){
			firstline = (int)tvi.lParam;
		}
	}
	if ( firstline == 1 ) p = CustomList;
	line = firstline;

	// 全般タグ内
	for ( ;; ){
		p += tstrlen(p) + 1;
		line++;

		if ( *p == '\0' ){
			line = 1;
			p = CustomList;
		}
		if ( line == firstline ) break; // 一周した

		if ( *p == '.' ){	// index だった
			indexline = line;
		}else{ // item
			if ( tstristr(p,findtext) ){
				// index の位置を求める
				if ( IsTrue(FindItem(hIndexTreeWnd,TreeView_GetRoot(hIndexTreeWnd),indexline,&hItem)) ){
					CustomItemListOffset = indexline + 1;
					CustomItemList = SearchList(indexline + 1);
					LoadListTree(TRUE);
					// item の位置を求める
					FindItem(hItemTreeWnd,TreeView_GetRoot(hItemTreeWnd),line,&hItem);
				}
				return;
			}
		}
	}
	// その他タグ内
	{
		const struct EtcLabelsStruct *el;

		for ( el = EtcLabels ; el->name != NULL ; el++ ){
			if ( tstristr(el->name,findtext) || tstristr(el->key,findtext) ){
				PropSheet_SetCurSel(GetParent(hDlg),NULL,9);
			}
		}
	}
}

LRESULT CALLBACK FindBoxProc(HWND hWnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	if ( iMsg == WM_GETDLGCODE ) return DLGC_WANTALLKEYS;
	if ( iMsg == WM_KEYDOWN ){
		if ( wParam == VK_TAB ){
			SetFocus(GetNextDlgTabItem(GetParent(hWnd),hWnd,
					GetKeyState(VK_SHIFT) < 0));
			return 0;
		}
		if ( wParam == VK_RETURN ){
			FindKeyword(GetParent(hWnd));
			return 0;
		}
		if ( wParam == VK_ESCAPE ){
			SetFocus(GetNextDlgTabItem(GetParent(hWnd),hWnd,TRUE));
		}
	}
	return CallWindowProc(OldFindBoxProc,hWnd,iMsg,wParam,lParam);
}

INT_PTR CALLBACK GeneralPage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg){
		case WM_INITDIALOG:
			SendDlgItemMessage(hDlg,IDE_FIND,EM_LIMITTEXT,MAX_PATH - 1,0);
			SendDlgItemMessage(hDlg,IDE_FIND,EM_SETCUEBANNER,0,(LPARAM)GetCTextW(StrFindInfo));
			OldFindBoxProc =(WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg,
					IDE_FIND),GWLP_WNDPROC,(LONG_PTR)FindBoxProc);
			LoadList(hDlg);
			if ( FirstTypeName == NULL ) break;
			FindKeyword(hDlg);
			return FALSE;
/*
		case WM_DESTROY: {	// １回しか作成しないのでイメージリストの廃棄を省略
			HIMAGELIST hImage;

			hImage = SendMessage(hItemTreeWnd,TVM_SETIMAGELIST,(WPARAM)TVSIL_NORMAL,(LPARAM)NULL);
			if ( hImage != NULL ) ImageList_Destroy(hImage);

		}
*/
		case WM_NOTIFY:
			TreeNotify(hDlg,(NMHDR *)lParam );
			break;

		case WM_COMMAND:
			if ( wParam == IDM_SETEDITFOCUS ){
				SetFocus(hEditWnd);
				break;
			}
			if ( wParam == IDM_RCLICKED ){
				CloseEdit(hDlg,hItemTreeWnd);
				ModifyItem2(hDlg,(HWND)lParam);
				break;
			}
			if ( (HIWORD(wParam) == EN_CHANGE) &&
				 (useedit != 0) &&
				 (useeditmodify == 0) &&
				 ((HWND)lParam == hEditWnd) ){
				useeditmodify = 1;
				ModifyEditItem(hDlg);
				useeditmodify = 0;
				Changed(hDlg);
				break;
			}
			if ( LOWORD(wParam) == IDB_FLOAT ){
				if ( edititem.edittype == EDIT_FILE ){
					PushFloatButton_File(hDlg);
				}
				if ( edititem.edittype == EDIT_DIR ){
					PushFloatButton_Dir(hDlg);
				}
				break;
			}
			if ( LOWORD(wParam) == IDB_TEST ){
				Test();
				break;
			}
			if ( LOWORD(wParam) == IDB_FIND ){
				FindKeyword(hDlg);
				break;
			}
			break;

		case WM_DESTROY:
			return StyleDlgProc(hDlg,msg,wParam,lParam);

		default:
			return FALSE;
//			return StyleDlgProc(hDlg,msg,wParam,lParam); // とりあえず使用せず
	}
	return TRUE;
}
