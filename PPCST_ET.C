/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer	その他 シート
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#pragma hdrstop

void EnumEtcItem(HWND hDlg);
void EnumHistoryItem(HWND hDlg);

WORD HistFlag = PPXH_GENERAL;
HWND hEtcTreeWnd;

int seltree, EtcEditFormat;
#define ETCID 0x100

// ヒストリ関連 ---------------------------------------------------------------
struct HistLabelsStruct {
	DWORD flag;
	TCHAR *name;
};
const struct HistLabelsStruct HistLabels[] = {
	{PPXH_GENERAL,		T("汎用\0general")},
	{PPXH_NUMBER,		T("数字\0digits")},
	{PPXH_COMMAND,		T("コマンドライン\0command line")},

	{PPXH_DIR,			T("ディレクトリ\0directories")},
	{PPXH_FILENAME,		T("ファイル名\0filename")},
	{PPXH_PATH,			T("フルパスファイル\0fullpath name")},

	{PPXH_SEARCH,		T("検索用文字列\0search")},
	{PPXH_MASK,			T("エントリ名マスク\0mask")},

	{PPXH_PPCPATH,		T("PPcディレクトリ\0PPc directory")},
	{PPXH_PPVNAME,		T("PPv表示ファイル\0PPv filename")},
	{PPXH_NETPCNAME,	T("ネットワークPC\0PC names")},
	{PPXH_ROMASTR,		T("ローマ字検索\0roma search")},

	{PPXH_USER1,		T("ユーザ定義 u\0User - u")},
	{PPXH_USER2,		T("ユーザ定義 x\0User - x")},
	{0, NULL}
};

// 全般 -----------------------------------------------------------------------
const struct EtcLabelsStruct EtcLabels[] = {
	{T("エイリアス\0alias"), T("A_exec"), T("(&I) エイリアス名=内容\0&I alias name = value"), ETC_TEXT},
	{T("ディレクトリ別設定\0directory settings"), T("XC_dset"), T("(&I) 末尾「\\」付きパスは下層も有効 ※削除のみ可\0&Item"), ETC_DSET},
	{T("PPcステータス行書式\0PPc status line"), T("XC_stat"), T("(&I)\0&I"), ETC_INFODISP},
	{T("PPc情報行1行目書式\0PPc 1st info."), T("XC_inf1"), T("(&I)\0&I"), ETC_INFODISP},
	{T("PPc情報行2行目書式\0PPc 2nd info."), T("XC_inf2"), T("(&I)\0&I"), ETC_INFODISP},
	{T("PPc表示形式メニュー[;]\0PPc view style"), T("MC_celS"), T("(&I) 新規は名前を記載して「編集」\0&Item"), ETC_CELLDISP},
	{T("PPvテキスト桁数\0PPv text columns"), T("XV_cols"), T("(&I) 桁数(76等 0:最大 -1:窓幅)=該当拡張子(.ext1;.ext2) 初期値80\0&Columns(0:full -1:window width)"), ETC_TEXT},
	{T("PPv,eタブ桁数\0PPv,e tab columns"), T("XV_tab"), T("(&I) 桁数(8)=該当拡張子(.txt;.doc)、全てに一致しなければ8\0&Item"), ETC_TEXT},
	{T("PPv種別オプション\0PPv option"), T("XV_opts"), T("(&I) 例) :HTML = -tag:off、txt = -unicode -esc:off\0&Item"), ETC_TEXT},
	{T("PPv単語ハイライト\0PPv highlight text"), T("CV_hkey"), T("(&I)\0&I"), ETC_HKEY},
	{T("ファイル操作アクションメニュー[;]\0*file actions"), T("X_fopt"), T("(&I) ※削除のみ可。追加はFile Operationダイアログで\0&I only delete"), ETC_NOPARAM},
	{T("ホスト,ID-パスワード\0id saved host"), T("_IDpwd"), T("(&I) ※削除のみ可。\0&I only delete"), ETC_NOPARAM},
	{T("その他設定\0Other configs"), T("_others"), T("(&I) 項目名=内容\0&(&I) Item name = value"), ETC_TEXT},
	{T("ユーザコマンド\0User command"), T("_Command"), T("(&I) コマンド名=内容(%*arg(n))\0&(&I) command name = value(%*arg(n)"), ETC_TEXT},
	{T("ユーザデータ\0User data"), T("_User"), T("(&I) 項目名=内容\0&(&I) Item name = value"), ETC_TEXT},
	{T("書庫DLL\0Archive DLL"), T("P_arc"), T("(&I)一覧の順番で優先使用される。※削除のみ可\0&Item list"), ETC_NOPARAM},
	{T("表示パス\0Path"), T("_Path"), T("ID(&I) = パス・タブ配置\0&ID = path/layout"), ETC_TEXT},
	{T("表示開始位置\0Window position"), T("_WinPos"), T("(&I) (座標) 大きさ _:&&[ALT]用 CJDLG:[J]用 ※削除のみ可\0&I"), ETC_WINPOS},
	{T("確認済実行ファイル\0Checked applications"), T("_Execs"), T("(&I) 最新の先頭CRC32 ※おまけの設定が必要。削除のみ可\0&I"), ETC_EXECS},
	{T("遅延処理リスト\0Delayed list"), T("_Delayed"), T("(&I)処理方法 = 対象ファイル\0&I"), ETC_TEXT},
	#ifndef RELEASE
	{T("IDLキャッシュ\0IDL cache"), T("#IdlC"), T(""), ETC_NOPARAM},
	#endif
	{NULL, NULL, NULL, 0}
};

const TCHAR StrAddLabel_Edit[] = T("編集(&A)\0Edit(&A)");
const TCHAR StrAddLabel_Add[] = T("追加(&A)\0&Add");
const TCHAR StrAddLabel_Item[] = T("項目(&I)\0&Item");
const TCHAR StrBrokenData[] = T("データ破損\0Broken data");

TCHAR *SetCText(TCHAR *dest, const TCHAR *ctext)
{
	tstrcpy(dest, GetCText(ctext));
	return dest + tstrlen(dest);
}

// ヒストリ ===================================================================
#ifdef UNICODE
void AddListItem_U(HWND hWnd, const WCHAR *text)
{
	TCHAR buf[0x1000];

	strcpyW(buf, text);
	SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)buf);
}
#endif

void EnumHistoryItem(HWND hDlg)
{
	int count = 0;
	HWND hListWnd;

	hListWnd = GetDlgItem(hDlg, IDL_EXITEM);
	SendMessage(hListWnd, WM_SETREDRAW, FALSE, 0);
	SendMessage(hListWnd, LB_RESETCONTENT, 0, 0);

	for ( ;; ){
		const TCHAR *histptr;

		UsePPx();
		histptr = EnumHistory(HistFlag, count);
		if ( histptr == NULL ) break;

#ifdef UNICODE
		if ( ALIGNMENT_BITS(histptr) & 1 ){
			AddListItem_U(hListWnd, histptr);
		}else
#endif
			SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)histptr);

		FreePPx();
		count++;
	}
	FreePPx();
	SendMessage(hListWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hListWnd, NULL, TRUE);
	SendDlgItemMessage(hDlg, IDS_ETCLIST, WM_SETTEXT, 0, (LPARAM)GetCText(StrAddLabel_Item) );
}

void DeleteHistoryItem(HWND hDlg)
{
	int index;
	TCHAR buf[CMDLINESIZE * 4];
	HWND hListWnd;

	hListWnd = GetDlgItem(hDlg, IDL_EXITEM);
	index = (int)SendMessage(hListWnd, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return;

	if ( LB_ERR != SendMessage(hListWnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buf)){
		DeleteHistory(HistFlag, buf);
		EnumHistoryItem(hDlg);

		SendMessage(hListWnd, LB_SETCURSEL, (WPARAM)index, 0);
		SendMessage(hListWnd, LB_SETTOPINDEX, (WPARAM)max(index - 6, 0), 0);
	}
}
// 全般 =======================================================================
// 初期化 ---------------------------------------------------------------
void InitEtcTree(HWND hDlg)
{
	HTREEITEM hHisRoot, hTempItem;
	TV_ITEM tvi;
	TV_INSERTSTRUCT tvins;
	const struct HistLabelsStruct *hl;
	const struct EtcLabelsStruct *el;
	TCHAR buf[0x60];

	SendDlgItemMessage(hDlg, IDE_EXTYPE, EM_LIMITTEXT, (WPARAM)VFPS - 1, 0);
	SendDlgItemMessage(hDlg, IDE_ALCCMD, EM_LIMITTEXT, (WPARAM)CMDLINESIZE - 1, 0);

	hEtcTreeWnd = GetDlgItem(hDlg, IDT_GENERAL);
	SendMessage(hEtcTreeWnd, WM_SETREDRAW, FALSE, 0);

	tvi.mask = TVIF_TEXT | TVIF_PARAM;
	tvi.lParam = ETCID;
	tvi.pszText = buf;
	tvins.hParent = TVI_ROOT;
	tvins.hInsertAfter = 0;
									// 各種一覧
	for ( el = EtcLabels ; el->name != NULL ; el++ ){
		wsprintf(buf, T("%s/%s"), GetCText(el->name), el->key);
		tvi.cchTextMax = tstrlen32(buf);
		TreeInsertItemValue(tvins) = tvi;
		hTempItem = (HTREEITEM)SendMessage(hEtcTreeWnd, TVM_INSERTITEM,
				0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
		if ( el == EtcLabels ){
			SendMessage(hEtcTreeWnd, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hTempItem);
		}
		tvi.lParam++;
	}
									// ヒストリ
	tvi.lParam = MAXLPARAM;
	tvi.pszText = (TCHAR *)GetCText(T("ヒストリ\0histories"));
	tvi.cchTextMax = tstrlen32(tvi.pszText);
	TreeInsertItemValue(tvins) = tvi;
	hHisRoot = (HTREEITEM)SendMessage(hEtcTreeWnd, TVM_INSERTITEM,
								0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
	tvi.lParam = 0;
	tvins.hParent = hHisRoot;
	for ( hl = HistLabels ; hl->name != NULL ; hl++ ){
		tvi.pszText = (TCHAR *)GetCText(hl->name);
		tvi.cchTextMax = tstrlen(tvi.pszText);
		TreeInsertItemValue(tvins) = tvi;
		SendMessage(hEtcTreeWnd, TVM_INSERTITEM,
				0, (LPARAM)(LPTV_INSERTSTRUCT)&tvins);
		tvi.lParam++;
	}
	TreeView_Expand(hEtcTreeWnd, hHisRoot, TVE_EXPAND);
	SendMessage(hEtcTreeWnd, WM_SETREDRAW, TRUE, 0);
}



// 項目をテキストに変換
void FormatEtcItem(TCHAR *data, int size, int type)
{
	TCHAR buf[CMDLINESIZE], *p = buf;
	LOADSETTINGS *ls;

	if ( type == ETC_HKEY ) return;		// CV_hkey

	if ( type == ETC_NOPARAM ){			// X_fopt
		data[0] = '\0';
		return;
	}

	if ( type == ETC_WINPOS ){			// _WinPos
		RECT *box;

		if ( size < (int)(sizeof(RECT) + sizeof(WORD)) ){
			SetCText(data, StrBrokenData);
			return;
		}
		box = (RECT *)data;
		wsprintf(buf, T("(%d,%d) %dx%d"),
				box->left, box->top,
				box->right - box->left, box->bottom - box->top);
		tstrcpy(data, buf);
		return;
	}
	if ( type == ETC_EXECS ){			// _Execs
		DWORD *dat;

		if ( size < (int)(sizeof(DWORD) * 2) ){
			SetCText(data, StrBrokenData);
			return;
		}
		dat = (DWORD *)data;
		wsprintf(buf, T("%08x"), dat[dat[0]]);
		tstrcpy(data, buf);
		return;
	}
	if ( size < (int)sizeof(XC_DSET) ){
		SetCText(data, StrBrokenData);
		return;
	}
										// XC_dset
	*(TCHAR *)(BYTE *)((BYTE *)data + size) = '\0';
	ls = (LOADSETTINGS *)data;
	if ( ls->dset.flags & DSET_CACHEONLY ) p = SetCText(p, T("キャッシュ \0cache,"));
	if ( ls->dset.flags & DSET_NODIRCHECK ) p = SetCText(p, T("更新監視無 \0no check,"));
	if ( ls->dset.flags & DSET_ASYNCREAD ) p = SetCText(p, T("非同期読込 \0Async read,"));
	if ( ls->dset.flags & DSET_REFRESH_ACACHE ) p = SetCText(p, T("非同期読込後常にキャッシュ保存 \0Async & cache save,"));
	if ( ls->dset.flags & DSET_NOSAVE_ACACHE ) p = SetCText(p, T("非同期読込のキャッシュ保存を禁止 \0No save async cache,"));
	if ( ls->dset.infoicon != DSETI_DEFAULT ) p = SetCText(p, T("情報行アイコン指定有 \0info.icon settings,"));
	if ( (ls->dset.cellicon != DSETI_DEFAULT) && ls->dset.cellicon ) p = SetCText(p, T("カーソル位置アイコン指定有 \0cursor line icon settings,"));
	if ( ls->dset.sort.mode.dat[0] != -1 ) p = SetCText(p, T("ソート指定有 \0sort settings,"));
	if ( ls->disp[0] ) p += wsprintf(p, T("%s"), ls->disp);
	*p = '\0';
	tstrcpy(data, buf);
}
#if !NODLL
void tstrreplace(TCHAR *text, const TCHAR *targetword, const TCHAR *replaceword)
{
	TCHAR *p;

	while ( (p = tstrstr(text, targetword)) != NULL ){
		int tlen = tstrlen32(targetword);
		int rlen = tstrlen32(replaceword);

		if ( tlen != rlen ) memmove(p + rlen, p + tlen, TSTRSIZE(p + tlen));
		memcpy(p, replaceword, TSTROFF(rlen));
		text = p + rlen;
	}
}
#endif

void CustNameEscape(TCHAR *label)
{
	tstrreplace(label, T("\""), T("\"\""));
	tstrreplace(label, T("%"), T("%%"));
}

void EnumEtcItem(HWND hDlg)
{
	int count = 0;
	int size;
	HWND hListWnd;
	TCHAR label[CMDLINESIZE * 2], data[CMDLINESIZE * 2];
	const TCHAR *key;

	hListWnd = GetDlgItem(hDlg, IDL_EXITEM);
	SendMessage(hListWnd, WM_SETREDRAW, FALSE, 0);
	SendMessage(hListWnd, LB_RESETCONTENT, 0, 0);
	EtcEditFormat = EtcLabels[seltree - ETCID].form;
	key = EtcLabels[seltree - ETCID].key;
/*
	if ( !tstrcmp(p, T("_WinPos")) ) EtcEditFormat = ETC_WINPOS;
	if ( !tstrcmp(p, T("_Execs")) ) EtcEditFormat = ETC_EXECS;
	if ( !tstrcmp(p, T("X_fopt")) ) EtcEditFormat = ETC_NOPARAM;
*/
	EnableDlgWindow(hDlg, IDB_TB_SETITEM, EtcEditFormat < ETC__VIEW);
	SetDlgItemText(hDlg, IDB_TB_SETITEM, GetCText(
		((EtcEditFormat > ETC__EDITBUTTON) && (EtcEditFormat < ETC__VIEW)) ?
			StrAddLabel_Edit : StrAddLabel_Add) );
	ShowDlgWindow(hDlg, IDB_TB_DELITEM, EtcEditFormat != ETC_INFODISP);
	if ( EtcEditFormat == ETC_INFODISP ){
		FormatCellDispSample(data, key, 1);
		SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)data);
	}else for ( ;; ){
		TCHAR *dest, *src;

		size = EnumCustTable(count, key, label, data, sizeof(data));
		if ( 0 > size ) break;

		if ( EtcEditFormat == ETC_CELLDISP ){		// MC_celS
			FormatCellDispSample(data, label, 0);
		}else{
			if ( EtcEditFormat ) FormatEtcItem(data, size, EtcEditFormat);
		}
		// ～ = ～ 形式に加工
		size = tstrlen32(label);
		dest = label + size;
		while ( size++ < 10 ) *dest++ = ' ';
		*dest++ = ' ';
		*dest++ = '=';
		*dest++ = ' ';
		tstrcpy(dest, data);

		src = label;
		if ( (EtcEditFormat == ETC_HKEY) && (*src == '/') ) src++;
		SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)src);
		count++;
	}
	SendMessage(hListWnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hListWnd, NULL, TRUE);
	SendDlgItemMessage(hDlg, IDS_ETCLIST, WM_SETTEXT, 0, (LPARAM)GetCText(EtcLabels[seltree - ETCID].comment));
}

void EtcSelectItem(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int index;
	TCHAR buf[CMDLINESIZE * 2], *p, *param;

	if ( GetListCursorIndex(wParam, lParam, &index) == 0 ) return;
	buf[0] = '\0';
	SendMessage((HWND)lParam, LB_GETTEXT, (WPARAM)index, (LPARAM)buf);

	p = param = tstrchr(buf, '=');
	if ( p == NULL ){ // 編集欄を使用しない
		buf[0] = '\0';
		param = buf;
	}else{
		while( p > buf ){
			if ( *(p - 1) != ' ' ) break;
			p--;
		}
		*p = '\0';
		if ( *(++param) == ' ' ) param++;
	}
	SendDlgItemMessage(hDlg, IDE_EXTYPE, WM_SETTEXT, 0, (LPARAM)buf);
	SendDlgItemMessage(hDlg, IDE_ALCCMD, WM_SETTEXT, 0, (LPARAM)param);
}

void AddEtcItem(HWND hDlg)
{
	TCHAR label[CMDLINESIZE], item[CMDLINESIZE], buf[CMDLINESIZE];
	const TCHAR *key;

	GetControlText(hDlg, IDE_EXTYPE, label, TSIZEOF(label));
	GetControlText(hDlg, IDE_ALCCMD, item, TSIZEOF(item));
	key = EtcLabels[seltree - ETCID].key;
	if ( EtcEditFormat > ETC_TEXT ){
		BOOL tempcreate = FALSE;

		if ( EtcEditFormat > ETC__VIEW ) return;

		if ( EtcEditFormat == ETC_HKEY ){
			if ( (label[0] != '\0') && (item[0] != '\0') ){
				tstrcpy(buf, label);
				FixWildItemName(buf);
				SetCustStringTable(T("CV_hkey"), buf, item, 0);
			}
			if ( PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_HIGHLIGHT),
					hDlg, HighlightDialogBox, (LPARAM)label) > 0 ){
				Changed(hDlg);
				EnumEtcItem(hDlg);
				SendDlgItemMessage(hDlg, IDE_EXTYPE, WM_SETTEXT, 0, (LPARAM)NilStr);
				SendDlgItemMessage(hDlg, IDE_ALCCMD, WM_SETTEXT, 0, (LPARAM)NilStr);
			}
			return;
		}

		// ETC_CELLDISP, ETC_INFODISP
		if ( EtcEditFormat == ETC_CELLDISP ){ // [;]menu
			if ( label[0] == '\0' ) return;
			if ( !IsExistCustTable(T("MC_celS"), label) && (item[0] != '\0') ){
				TCHAR labelbuf[CMDLINESIZE * 2];

				tstrcpy(labelbuf, label);
				tstrreplace(labelbuf, T("%"), T("%%"));

				// 未登録→仮登録する
				wsprintf(buf, T("*setcust MC_celS:%s=%s"), labelbuf, item);
				PP_ExtractMacro(NULL, NULL, NULL, buf, NULL, 0);
				tempcreate = TRUE;
			}
		}
		if ( PPxDialogBoxParam(hInst,
				MAKEINTRESOURCE(SubDialog[DIALOG_DISPFOMAT]),
				hDlg, DispFormatDialogBox,
				(LPARAM)(EtcEditFormat == ETC_INFODISP ? key : label)) > 0 ){
			Changed(hDlg);
			EnumEtcItem(hDlg);
			SendDlgItemMessage(hDlg, IDE_EXTYPE, WM_SETTEXT, 0, (LPARAM)NilStr);
			SendDlgItemMessage(hDlg, IDE_ALCCMD, WM_SETTEXT, 0, (LPARAM)NilStr);
		}else if ( IsTrue(tempcreate) ){
			DeleteCustTable(T("MC_celS"), label, 0);
		}
		return;
	}

	if ( NO_ERROR == SetCustStringTable(key, label, item, 0) ){
		Changed(hDlg);
		EnumEtcItem(hDlg);
	}
}

void DeleteEtcItem(HWND hDlg)
{
	TCHAR label[CMDLINESIZE];
	const TCHAR *key;

	key = EtcLabels[seltree - ETCID].key;
	GetControlText(hDlg, IDE_EXTYPE, label, TSIZEOF(label));
	if ( NO_ERROR == DeleteCustTable(key, label, 0) ){
		HWND hListWnd;
		int index;

		hListWnd = GetDlgItem(hDlg, IDL_EXITEM);
		index = (int)SendMessage(hListWnd, LB_GETCURSEL, 0, 0);
		Changed(hDlg);
		EnumEtcItem(hDlg);
		if ( index != LB_ERR ){
			SendMessage(hListWnd, LB_SETCURSEL, (WPARAM)index, 0);
			SendMessage(hListWnd, LB_SETTOPINDEX, (WPARAM)max(index - 6, 0), 0);
			EtcSelectItem(hDlg, TMAKEWPARAM(IDL_EXITEM, LBN_SELCHANGE), (LPARAM)hListWnd);
		}
	}
}

// 共通 -----------------------------------------------------------------------
void EtcSelectType(HWND hDlg, HWND hTwnd)
{
	TV_ITEM tvi;
	BOOL etc;
	HWND hExitem;

	tvi.hItem = TreeView_GetSelection(hTwnd);
	tvi.mask = TVIF_PARAM;
	TreeView_GetItem(hTwnd, &tvi);
	seltree = tvi.lParam;
	etc = seltree >= ETCID;
	ShowDlgWindow(hDlg, IDB_TB_SETITEM, etc);
	ShowDlgWindow(hDlg, IDB_MEUP, etc);
	ShowDlgWindow(hDlg, IDB_MEDW, etc);
	if ( etc ){ // Etc
		etc = (seltree < 0x102) || (seltree > 0x104);
		ShowDlgWindow(hDlg, IDE_EXTYPE, etc);
		ShowDlgWindow(hDlg, IDE_ALCCMD, etc);
		EnumEtcItem(hDlg);
	}else if ( (tvi.lParam >= 0) && (tvi.lParam < 14) ){ // Histroy
		HistFlag = (WORD)HistLabels[tvi.lParam].flag;
		EnumHistoryItem(hDlg);
	}
	hExitem = GetDlgItem(hDlg, IDL_EXITEM);
	SendMessage(hExitem, LB_SETCURSEL, 0, 0);
	EtcSelectItem(hDlg, TMAKEWPARAM(IDL_EXITEM, LBN_SELCHANGE), (LPARAM)hExitem);
}

// 項目を一つ上下に移動
void EItemUpDown(HWND hDlg, int offset)
{
	TCHAR label[MAX_PATH], para[0x8000];
	const TCHAR *key;
	int size, index;

	key = EtcLabels[seltree - ETCID].key;

	index = (int)SendDlgItemMessage(hDlg, IDL_EXITEM, LB_GETCURSEL, 0, 0);
	if ( offset < 0 ){
		if ( index <= 0 ) return;
	}else{
		if ( index < 0 ) return;
		if ((index + 1) >=SendDlgItemMessage(hDlg, IDL_EXITEM, LB_GETCOUNT, 0, 0)){
			return;
		}
	}
	size = EnumCustTable(index, key, label, para, sizeof(para));
	if ( size < 0 ) return;
	DeleteCustTable(key, NULL, index);
	InsertCustTable(key, label, index + offset, para, size);
	EtcSelectType(hDlg, GetDlgItem(hDlg, IDT_GENERAL));

	index += offset;
	Changed(hDlg);
	SendDlgItemMessage(hDlg, IDL_EXITEM, LB_SETCURSEL, (WPARAM)index, 0);
}


BOOL EtcTreeNotify(HWND hDlg, NMHDR *nmh)
{
	switch (nmh->code){
		case PSN_SETACTIVE:
			InitWndIcon(hDlg, IDB_TB_DELITEM);
			break;

		case PSN_APPLY:
		case PSN_HELP:
			StyleDlgProc(hDlg, WM_NOTIFY, IDD_ETCTREE, (LPARAM)nmh);
			break;

		case NM_DBLCLK:
			EtcSelectType(hDlg, nmh->hwndFrom);
			break;

		case TVN_KEYDOWN:
			switch ( ((TV_KEYDOWN *)nmh)->wVKey ){
				case VK_SPACE:
					EtcSelectType(hDlg, nmh->hwndFrom);
					break;
			}
			break;

		case TVN_SELCHANGED:
			if ( nmh->hwndFrom == hEtcTreeWnd ){
				EtcSelectType(hDlg, nmh->hwndFrom);
			}
			break;

//		default:
	}
	return 0;
}

INT_PTR CALLBACK EtcPage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg){
		case WM_INITDIALOG:
			InitEtcTree(hDlg);
			return FALSE;

		case WM_NOTIFY:
			EtcTreeNotify(hDlg, (NMHDR *)lParam);
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDL_EXITEM:
					if ( seltree >= ETCID ) EtcSelectItem(hDlg, wParam, lParam);
					break;

				case IDB_TB_SETITEM:
					if ( seltree >= ETCID ) AddEtcItem(hDlg);
					break;

				case IDB_TB_DELITEM:
					if ( seltree >= ETCID ){
						DeleteEtcItem(hDlg);
					}else{
						DeleteHistoryItem(hDlg);
					}
					break;

				case IDB_MEUP:
					EItemUpDown(hDlg, -1);
					break;

				case IDB_MEDW:
					EItemUpDown(hDlg, +1);
					break;

				case IDB_TEST:
					Test();
					break;
			}
			break;
		default:
			return StyleDlgProc(hDlg, msg, IDD_ETCTREE, lParam);
	}
	return TRUE;
}
