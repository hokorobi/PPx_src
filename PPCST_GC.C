/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer									色シート
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPCUST.H"
#pragma hdrstop

#ifndef COMMON_LVB_UNDERSCORE
	#define COMMON_LVB_UNDERSCORE 0x8000
#endif

#define GC_mask		0xff
#define GC_single	0
#define GC_list		1
#define GC_table	2
#define GC_entry	3
#define GC_hmenu	4
#define GC_line		5
#define GC_aoff		B10 // C_AUTO は非表示扱い
#define GC_ppbItem	B11 // GC_list
#define GC_sortItem	B12 // GC_table
#define GC_editItem	B13 // GC_table
#define GC_haveItem	B14 // GC_list
#define GC_auto		B15 // 一部のGC_single

typedef struct {
	const TCHAR *name;
	const TCHAR *item;
	DWORD flags; // GC_
	const TCHAR **sublist;
	int subs;
} LISTS;

const TCHAR *GCline[] = {T("通常\0normal"), T("未確定\0gray"), NULL};
const TCHAR *GCCdisp[] = {T("文字\0text"), T("背景\0back"), NULL};
const TCHAR *GCcapt[] = {
	T("現在窓 文字\0selected text"), T("現在窓 背景\0selected back"),
	T("反対窓 文字\0pair text"), T("反対窓 背景\0pair back"),
	T("非選択 文字\0normal text"), T("非選択 背景\0normal back"),
	T("タブxボタン\0tab x button"), T("タブボタン背景\0tab x back"), NULL};
const TCHAR *GCentry[] = {
	T("メッセージ\0message"), T("現dir .\0."), T("親dir ..\0.."),
	T("ラベル\0label"), T("ディレクトリ\0directory"), T("システム\0system"),
	T("隠し\0hidden"), T("読出専用\0readonly"), T("通常\0normal"),
	T("圧縮\0compressed"), T("リンク\0link"), T("仮想化\0virtual"),
	T("暗号化\0encrypted"), T("特殊\0special"), NULL};
const TCHAR *GCeInfo[] = {
	T("メッセージ\0message"), T("削除\0deleted"), T("通常\0normal"),
	T("不明\0graystate"), T("更新\0modified"), T("追加\0appended"),
	T("NoFocus\0no focus"), T("枠\0box"), T("下線\0underline"),
	T("マーク「*」\0marked"), T("通常(偶数行)\0normal(even line)"),
	T("選択\0selected"), T("区切線(グリッド)\0grid"),
	T("ハイライト1\0highlight1"), T("ハイライト2\0highlight2"),
	T("ハイライト3\0highlight3"), T("ハイライト4\0highlight4"),
	T("ハイライト5\0highlight5"), T("ハイライト6\0highlight6"),
	T("ハイライト7\0highlight7"),
	T("マウスカーソル下\0mouse hover"), NULL};
const TCHAR *GCltag[] = {T("タグ\0tag"), T("コメント\0comment"), NULL};
const TCHAR *GCchar[] = {
	T("灰\0gray"), T("明赤\0light red"), T("明緑\0light green"),
	T("明青\0light blue"), T("明黄\0light yellow"), T("明水\0light cyan"),
	T("明紫\0light magenta"), T("明白\0light white"),
	T("黒(背景)\0black(back)"), T("赤\0red"), T("緑\0green"), T("青\0blue"),
	T("黄\0yellow"), T("水\0cyan"), T("紫\0magenta"), T("暗白(初期色)\0white(default)"), NULL};
const TCHAR *GCb_pals[] = {
	T("黒(背景)\0black(back)"), T("青\0blue"), T("緑\0green"), T("水\0cyan"),
	T("赤\0red"), T("紫\0magenta"), T("黄\0yellow"), T("暗白(初期色)\0white(default)"),
	T("灰\0gray"), T("明青\0light blue"), T("明緑\0light green"), T("明水\0light cyan"),
	T("明赤\0light red"), T("明紫\0light magenta"), T("明黄\0light yellow"), T("明白\0light white"), NULL};
const TCHAR *GChili[] = {T("検索\0find"),
	T("ハイライト1\0highlight1"), T("ハイライト2\0highlight2"),
	T("ハイライト3\0highlight3"), T("ハイライト4\0highlight4"),
	T("ハイライト5\0highlight5"), T("ハイライト6\0highlight6"),
	T("ハイライト7\0highlight7"), T("ハイライト8\0highlight8"),
	NULL};

const TCHAR *GClbak[] = {
	T("旧行\0old line"), T("(未実装)\0(reserverd)"), T("しおり行\0bookmark"), NULL};
const TCHAR *GClnum[] = {T("先頭行\0first line"), T("桁折り行\0second line"), NULL};
const TCHAR *GCbedit[] = {
	T("編集文字\0edit text"), T("編集背景\0edit back"),
	T("選択文字\0selected text"), T("選択背景\0selected back"),
	T("参照文字\0log text"), T("参照背景\0log back"),
	T("参照選択文字\0log selected text"), T("参照選択背景\0log selected back"), NULL};
const LISTS GClist[] = {
	{T("背景色\0back ground"), T("C_back"),	GC_single | GC_auto, NULL, 0},
	{T("文字 項目名\0message text"),	T("C_mes"),		GC_single | GC_auto, NULL, 0},
	{T("文字 内容\0value text"),		T("C_info"),	GC_single | GC_auto, NULL, 0},
	{T("報告\0response"),	T("C_res"),		GC_list | GC_haveItem, GCCdisp, 2},
	{T("境界線\0line"),		T("C_line"),	GC_list | GC_haveItem, GCline, 2},
	{T("ツリー\0tree"),		T("CC_tree"),	GC_list | GC_haveItem, GCCdisp, 2},
	{T("チップ\0tip window"),		T("C_tip"),		GC_list | GC_haveItem, GCCdisp, 2},
	{T("PPc 拡張子色\0PPc file extension"), T("C_ext"),	GC_table | GC_haveItem | GC_editItem | GC_sortItem, NULL, 0},
	{T("PPc エントリ文字/属性\0PPc entry text"),	T("C_entry"), GC_list | GC_haveItem, GCentry, 14},
	{T("PPc エントリ背景/状態\0PPc entry back"),	T("C_eInfo"), GC_list | GC_haveItem, GCeInfo, 21},
	{T("PPc 情報行1\0PPc info1"), T("XC_inf1"),	GC_list | GC_haveItem, GCCdisp, 2},
	{T("PPc 情報行2\0PPc info2"), T("XC_inf2"),	GC_list | GC_haveItem, GCCdisp, 2},
//	{T("PPc エントリ"),	T("XC_celD"),	GC_list | GC_haveItem, GCCdisp, 2},
	{T("ペインタイトル・タブ\0caption, tab"), T("C_capt"),	GC_list | GC_haveItem, GCcapt, 8},
	{T("ログ、アドレスバー\0log"), T("CC_log"), GC_list | GC_haveItem, GCCdisp, 2},
	{T("PPv 文字・背景\0PPv text, back"),	T("CV_char"),	GC_list | GC_haveItem, GCchar, 16},
	{T("PPv 特殊行背景\0PPv ex. line back"),	T("CV_lbak"),	GC_list | GC_haveItem | GC_aoff, GClbak, 3},
	{T("PPv 末端線\0PPv bound"),	T("CV_boun"),	GC_single,		NULL, 0},
	{T("PPv 行カーソル\0PPv underline"), T("CV_lcsr"),	GC_single,	NULL, 0},
	{T("PPv 行番号\0PPv line number"),	T("CV_lnum"),	GC_list | GC_haveItem, GClnum, 2},
	{T("PPv 制御文字\0PPv cotrol"),		T("CV_ctrl"),	GC_single,	NULL, 0},
	{T("PPv 改行文字\0PPv line feed"),	T("CV_lf"),		GC_single,	NULL, 0},
	{T("PPv タブ文字\0PPv tab"),		T("CV_tab"),	GC_single,	NULL, 0},
	{T("PPv 全角空白\0PPv wide space"),	T("CV_spc"),	GC_single,	NULL, 0},
	{T("PPv リンク\0PPv link"),			T("CV_link"),	GC_single,	NULL, 0},
	{T("PPv タグ\0PPv tag"),			T("CV_syn"),	GC_list | GC_haveItem, GCltag, 3},
	{T("PPv ハイライト\0PPv highlight"),	T("CV_hili"),	GC_list | GC_haveItem, GChili, 9},
	{T("PPb 編集時\0PPb edit line"),	T("CB_edit"),	GC_list | GC_haveItem | GC_ppbItem, GCbedit, 4},
	{T("PPb 実行時\0PPv log"),	T("CB_com"),	GC_list | GC_haveItem | GC_ppbItem, GCCdisp, 2},
	{T("PPb 色パレット(Vista以降)\0PPb palettes(Vista)"),	T("CB_pals"),	GC_list | GC_haveItem, GCb_pals, 16},
	{NULL, NULL, 0, NULL, 0}
};

const TCHAR stringauto[] = T(" 自動\0 auto ");
const TCHAR stringaoff[] = T("非表示\0disable");
const TCHAR stringetc[] = T(" 他 \0 other ");
const TCHAR AutoString[] = T("a)");
const TCHAR SubMenuString[] = T(">");
const WCHAR stringExtInfo[] = L"拡張子/ワイルドカード\0extention/wildcard";

#define COLORLIST_WIDTH 6
#define COLORLIST_HEIGHT 12
#define COLORLIST_COLORS (COLORLIST_WIDTH * COLORLIST_HEIGHT)

enum {
//PPcの定義色
	CL_back = (COLORLIST_COLORS - 12), CL_mes, CL_info,
//Windowsの定義色
	CL_windowtext, CL_window, CL_hilighttext, CL_hilight,
	CL_btnface, CL_inactive,
//カスタマイザ関連
	CL_prev, // 直前で使用した色
	CL_user, // ユーザ作成色
	CL_auto // 動的自動取得色
};

#define CL_first CL_back
#define CL_withtext CL_user
#define CL_last CL_auto

COLORREF G_Colors[COLORLIST_COLORS] = {
	C_BLACK, C_RED, C_GREEN, C_BLUE,
	C_YELLOW, C_CYAN, C_MAGENTA, C_SBLUE,
	C_MGREEN, 0x80ff80, C_CREAM, 0x202020,

	C_DBLACK, 0x0000c0, 0x40ff00, 0xc00000,
	0x00c0c0, 0xff8000, 0x8000ff, 0xffff80,
	0x4080ff, 0x80ff00, 0x80ffff, 0x404040,

	C_GRAY, 0x404080, 0x408000, 0xa00000,
	0x408080, 0xc08000, 0xff0080, 0x804000,
	0x0080ff, 0x80ff00, 0x40c0c0, 0x606060,

	C_DWHITE, C_DRED, C_DGREEN, C_DBLUE,
	C_DYELLOW, C_DCYAN, C_DMAGENTA, 0x400080,
	0xc08080, 0x00ff80, 0x8080ff, 0xa0a0a0,

	C_WHITE, 0x000040, 0x004000, 0x400000,
	0x004080, 0x404000, 0x800040, 0x400040,
	0xff8080, 0x808040, 0xff80ff, 0xe0e0e0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, C_AUTO
};


#define CONSOLECOLOR 16

COLORREF ConsoleColors[CONSOLECOLOR] = {
	C_BLACK, C_DBLUE, C_DGREEN, C_DCYAN, C_DRED, C_DMAGENTA, C_DYELLOW, C_DWHITE,
	C_DBLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_YELLOW, C_WHITE,
};

#define USERUSEPPXCOLOR 3
// 色詳細ダイアログのユーザ定義色
#define userColor_SEL 15 // 現在選択している色
COLORREF userColor[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int edit_gc = 0;	// IDL_GCTYPE の選択中インデックス
int edit_sgc;		// IDL_GCITEM の選択中インデックス

#define WINDOWSCOLOR 6
int WinColors[WINDOWSCOLOR] = {COLOR_WINDOWTEXT, COLOR_WINDOW,
		COLOR_HIGHLIGHTTEXT, COLOR_HIGHLIGHT,
		COLOR_BTNFACE, COLOR_INACTIVECAPTION
};

typedef struct {
	TCHAR text[VFPS], *textptr;
	BOOL cb_top, cb_bottom;
} HighlightItem;

#define CUSTOFFSETDATA(cust) (const BYTE *)(cust + sizeof(WORD))
int WINAPI SortColorFunc(const BYTE *cust1, const BYTE *cust2)
{
	return	*((DWORD *)(CUSTOFFSETDATA(cust1) + TSTRSIZE((const TCHAR *)CUSTOFFSETDATA(cust1)))) -
			*((DWORD *)(CUSTOFFSETDATA(cust2) + TSTRSIZE((const TCHAR *)CUSTOFFSETDATA(cust2))));
}

// 使いそうな色を一覧に用意する
void FixColorList(void)
{
	int i;

	// PPx の色から３色用意
	for ( i = 0 ; i < USERUSEPPXCOLOR ; i++ ){
		GetCustData(GClist[i].item, &userColor[i], sizeof(COLORREF));
		G_Colors[CL_first + i] = userColor[i];
	}
	// Windows の色から6色用意
	for ( i = 0 ; i < WINDOWSCOLOR ; i++ ){
		G_Colors[CL_windowtext + i] =
			userColor[USERUSEPPXCOLOR + i] = GetSysColor(WinColors[i]);
	}
}

void SetColorList(HWND hDlg, COLORREF c)
{
	int i;
	DWORD index;

	index = SendDlgItemMessage(hDlg, IDL_GCOLOR, LB_GETCURSEL, 0, 0);
	if ( index < COLORLIST_COLORS ){
		G_Colors[CL_prev] = G_Colors[index];
		InvalidateRect(GetDlgItem(hDlg, IDL_GCOLOR), NULL, FALSE);
	}

	if ( c == C_AUTO ){
		i = CL_auto;
	}else{
		for ( i = 0 ; i < CL_user ; i++ ){
			if ( G_Colors[i] == c ) break;
		}
		G_Colors[i] = c;
	}
	SendDlgItemMessage(hDlg, IDL_GCOLOR, LB_SETCURSEL, (WPARAM)i, 0);
}

void CSelectType2(HWND hDlg)
{
	const LISTS *gcl;
	TCHAR buf[0x4000];

	gcl = &GClist[edit_gc];
	switch( gcl->flags & GC_mask ){
		case GC_list:
			memset(buf, 0xff, sizeof buf);
			GetCustData(gcl->item, buf, sizeof(buf));
			if ( gcl->flags & GC_ppbItem ){ // console の色に変換
				WORD bc;

				bc = ((WORD *)buf)[edit_sgc / 2];
				((COLORREF *)buf)[0] = ConsoleColors[bc & 0xf];
				((COLORREF *)buf)[1] = ConsoleColors[(bc >> 4) & 0xf];
				CheckDlgButton(hDlg, IDX_GCULINE, bc & COMMON_LVB_UNDERSCORE);
				SetColorList(hDlg, ((COLORREF *)buf)[edit_sgc & 1]);
			}else{
				SetColorList(hDlg, ((COLORREF *)buf)[edit_sgc]);
			}
			break;
		case GC_table:{
			COLORREF c;

			SendDlgItemMessage(hDlg,
					IDL_GCITEM, LB_GETTEXT, (WPARAM)edit_sgc, (LPARAM)buf);
			GetCustTable(gcl->item, buf, (TCHAR *)&c, sizeof(c));
			SetColorList(hDlg, c);
			break;
		}
	}
	SetDlgItemText(hDlg, IDE_GCEDIT, NilStr);
}

void CSelectType(HWND hDlg)
{
	const LISTS *gcl;
	HWND hListWnd;
	BYTE buf[0x400];
	COLORREF color;
	DWORD flags;
	BOOL showflag;

	hListWnd = GetDlgItem(hDlg, IDL_GCITEM);
	gcl = &GClist[edit_gc];

	flags = gcl->flags;
	showflag = flags & GC_haveItem;	// アイテム有り
	ShowDlgWindow(hDlg, IDS_GCITEM, showflag);
	ShowDlgWindow(hDlg, IDL_GCITEM, showflag);
	showflag = flags & GC_editItem;	// アイテム追加・登録・ソート
	ShowDlgWindow(hDlg, IDE_GCEDIT, showflag);
	ShowDlgWindow(hDlg, IDB_GCADD, showflag);
	ShowDlgWindow(hDlg, IDB_GCDEL, showflag);
	ShowDlgWindow(hDlg, IDB_GCSORTN, showflag);
	ShowDlgWindow(hDlg, IDB_GCSORTC, showflag);
	ShowDlgWindow(hDlg, IDX_GCULINE, flags & GC_ppbItem); // コンソール色

	switch( flags & GC_mask ){
		case GC_single:{
			COLORREF c = C_AUTO;

			GetCustData(gcl->item, &c, sizeof(c));
			SetColorList(hDlg, c);
			break;
		}
		case GC_list:{
			const TCHAR **list;
			int i;

			memset(buf, 0, 32 * sizeof(COLORREF));
			GetCustData(gcl->item, buf, sizeof(buf));
			SendMessage(hListWnd, LB_RESETCONTENT, 0, 0);
			for ( list = gcl->sublist, i = 0 ; *list ; list++, i++ ){
				SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)GetCText(*list));
				if ( gcl->flags & GC_ppbItem ){
					WORD bc;

					bc = ((WORD *)buf)[i / 2];
					if ( i & 1 ){
						color = ConsoleColors[(bc >> 4) & 0xf];
					}else{
						color = ConsoleColors[bc & 0xf];
					}
					SendMessage(hListWnd, LB_SETITEMDATA, i, (LPARAM)color);
				}else{
					SendMessage(hListWnd, LB_SETITEMDATA, i, (LPARAM)*((COLORREF *)buf + i));
				}
			}
			edit_sgc = (gcl->sublist == GCchar) ? 15 : 0;
			SendMessage(hListWnd, LB_SETCARETINDEX, edit_sgc, TMAKELPARAM(TRUE, 0));
			CSelectType2(hDlg);
			break;
		}
		case GC_table:{
			int i, size;
			TCHAR label[MAX_PATH];

			SendMessage(hListWnd, LB_RESETCONTENT, 0, 0);
			i = 0;
			for ( ;; ){
				size = EnumCustTable(i, gcl->item, label, &color, sizeof(color));
				if ( 0 > size ) break;
				SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)label);
				SendMessage(hListWnd, LB_SETITEMDATA, i, (LPARAM)color);
				i++;
			}
			edit_sgc = 0;
			CSelectType2(hDlg);
			break;
		}
	}
}

COLORREF USEFASTCALL ConColorConvertSub(COLORREF color)
{
	color &= 0xff;
	if ( color < 0x60 ) return 0;
	if ( color < 0xe0 ) return 0x80;
	return 0xff;
}

COLORREF USEFASTCALL ConColorConvert(COLORREF color)
{
	if ( color == C_DWHITE ) return color;
	color = ConColorConvertSub(color) |
		(ConColorConvertSub(color >> 8) << 8) |
		(ConColorConvertSub(color >> 16) << 16);
	if ( color & 0x010101 ){ // 0xff の要素を見つけたら、0x80→0xff変換
		if ( color & 0x000080 ) color |= 0x0000ff;
		if ( color & 0x008000 ) color |= 0x00ff00;
		if ( color & 0x800000 ) color |= 0xff0000;
	}
	return color;
}

void SelectColor(HWND hDlg, DWORD index)
{
	COLORREF c;
	TCHAR buf[0x4000];

	if ( index >= COLORLIST_COLORS ) return;
	c = G_Colors[index];
	switch( GClist[edit_gc].flags & GC_mask ){
		case GC_single:
			SetCustData(GClist[edit_gc].item, (TCHAR *)&c, sizeof(c));
			SendDlgItemMessage(hDlg, IDL_GCTYPE,
					LB_SETITEMDATA, (WPARAM)edit_gc, (LPARAM)c);
			InvalidateRect(GetDlgItem(hDlg, IDL_GCTYPE), NULL, FALSE);
			break;

		case GC_list: {
			DWORD size, minsize;

			if ( GClist[edit_gc].flags & GC_ppbItem ){	// コンソール
				int i;
				WORD *dest;

				minsize = GClist[edit_gc].subs * sizeof(WORD);
				size = GetCustDataSize(GClist[edit_gc].item);
				if ( (size < minsize) || (size > 0x800000) ) size = minsize;
				dest = &((WORD *)buf)[edit_sgc / 2];
				*dest = 0;

				GetCustData(GClist[edit_gc].item, buf, sizeof(buf));
				c = ConColorConvert(c);
				for ( i = 0 ; i < CONSOLECOLOR ; i++ ){
					if ( ConsoleColors[i] == c ) break;
				}
				if ( i < CONSOLECOLOR ){
					if ( edit_sgc & 1 ){ // B4-7
						*dest = (WORD)((*dest & 0x7f0f) | (i << 4));
					}else{				// B0-3
						*dest = (WORD)((*dest & 0x7ff0) | i);
					}
					if ( IsDlgButtonChecked(hDlg, IDX_GCULINE) ){
						setflag(*dest, COMMON_LVB_UNDERSCORE);
					}
					SetCustData(GClist[edit_gc].item, buf, size);
				}else{
					c = (COLORREF)SendDlgItemMessage(hDlg, IDL_GCITEM,
							LB_GETITEMDATA, (WPARAM)edit_sgc, 0);
				}
				CSelectType2(hDlg);
			}else{									// GUI
				minsize = GClist[edit_gc].subs * sizeof(COLORREF);
				size = GetCustDataSize(GClist[edit_gc].item);
				if ( (size < minsize) || (size > 0x800000) ) size = minsize;

				memset(buf, 0xff, sizeof buf);
				GetCustData(GClist[edit_gc].item, buf, sizeof(buf));
				((COLORREF *)buf)[edit_sgc] = c;
				SetCustData(GClist[edit_gc].item, buf, size);
			}
			SendDlgItemMessage(hDlg, IDL_GCITEM,
					LB_SETITEMDATA, (WPARAM)edit_sgc, (LPARAM)c);
			InvalidateRect(GetDlgItem(hDlg, IDL_GCITEM), NULL, FALSE);
			break;
		}
		case GC_table:
			GetControlText(hDlg, IDE_GCEDIT, buf, TSIZEOF(buf));
			if ( buf[0] != '\0' ) break; // 追加中は、選択で登録しない

			SendDlgItemMessage(hDlg, IDL_GCITEM,
					LB_GETTEXT, (WPARAM)edit_sgc, (LPARAM)buf);
			SetCustTable(GClist[edit_gc].item, buf, (TCHAR *)&c, sizeof(c));

			SendDlgItemMessage(hDlg, IDL_GCITEM,
					LB_SETITEMDATA, (WPARAM)edit_sgc, (LPARAM)c);
			InvalidateRect(GetDlgItem(hDlg, IDL_GCITEM), NULL, FALSE);
			break;
	}
	FixColorList();
}

void ChangeUnderline(HWND hDlg)
{
	DWORD size, minsize;
	WORD *dest;
	BYTE buf[0x100];

	minsize = GClist[edit_gc].subs * sizeof(WORD);
	size = GetCustDataSize(GClist[edit_gc].item);
	if ( (size < minsize) || (size > 0x800000) ) size = minsize;
	dest = &((WORD *)buf)[edit_sgc / 2];
	*dest = 0;

	GetCustData(GClist[edit_gc].item, buf, sizeof(buf));
	*dest = (WORD)(*dest & ~COMMON_LVB_UNDERSCORE);
	if ( IsDlgButtonChecked(hDlg, IDX_GCULINE) ){
		setflag(*dest, COMMON_LVB_UNDERSCORE);
	}
	SetCustData(GClist[edit_gc].item, buf, size);
}

void InitColorControls(HWND hDlg)
{
	HDC hDC;
	HGDIOBJ hOldFont;
	TEXTMETRIC tm;
	HWND hCListWnd;
	RECT box;

	hCListWnd = GetDlgItem(hDlg, IDL_GCOLOR);
	GetClientRect(hCListWnd, &box);
	SendMessage(hCListWnd, LB_SETCOLUMNWIDTH,
			(WPARAM)((box.right - box.left) / COLORLIST_WIDTH), 0);
	SendMessage(hCListWnd, LB_SETITEMHEIGHT,
			0, (LPARAM)((box.bottom - box.top) / COLORLIST_HEIGHT));

	hDC = GetDC(hDlg);
	hOldFont = SelectObject(hDC, (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0));
	GetTextMetrics(hDC, &tm);
	SelectObject(hDC, hOldFont);
	ReleaseDC(hDlg, hDC);
	SendDlgItemMessage(hDlg, IDL_GCTYPE, LB_SETITEMHEIGHT, 0, (LPARAM)tm.tmHeight);
	SendDlgItemMessage(hDlg, IDL_GCITEM, LB_SETITEMHEIGHT, 0, (LPARAM)tm.tmHeight);
}

void InitColorPage(HWND hDlg)
{
	int i;
	HWND hCListWnd;
	COLORREF color;

	InitColorControls(hDlg);
	hCListWnd = GetDlgItem(hDlg, IDL_GCOLOR);

		/* はじめは、値に COLORREF を直接入れていたが、SP 無し
		   WinXP で、0 等を使用しても登録されないため、ダミーの値を
		   入れるように変更した									*/
	for ( i = 0 ; i <= CL_last ; i++ ){
		SendMessage(hCListWnd, LB_ADDSTRING, 0, (LONG)1);
	}
	for ( i = 0 ; GClist[i].name ; i++ ){
		SendDlgItemMessage(hDlg, IDL_GCTYPE,
				LB_ADDSTRING, 0, (LPARAM)GetCText(GClist[i].name));
		if ( (GClist[i].flags & GC_mask) == GC_single ){
			GetCustData(GClist[i].item, &color, sizeof(color));
			SendDlgItemMessage(hDlg, IDL_GCTYPE,
					LB_SETITEMDATA, (WPARAM)i, (LPARAM)color);
		}else{
			SendDlgItemMessage(hDlg, IDL_GCTYPE,
					LB_SETITEMDATA, (WPARAM)i, (LPARAM)0xfffffffe);
		}
	}
	SendDlgItemMessage(hDlg, IDE_GCEDIT, EM_SETCUEBANNER, 0, (LPARAM)GetCTextW(stringExtInfo));

	FixColorList();
}


// 選択可能な色の一覧の表示
void ColorSampleDraw(DRAWITEMSTRUCT *lpdis)
{
	if ( lpdis->itemAction & (ODA_DRAWENTIRE | ODA_SELECT) ){
		int OldBkMode;
		HBRUSH hB;
		COLORREF showcolor;

		OldBkMode = SetBkMode(lpdis->hDC, OPAQUE);
		showcolor = (lpdis->itemID == CL_auto) ?
				GetSysColor(COLOR_WINDOW) : G_Colors[lpdis->itemID];

		if ( GClist[edit_gc].flags & GC_ppbItem ){
			showcolor = ConColorConvert(showcolor);
		}

		hB = CreateSolidBrush(showcolor);
		FillRect(lpdis->hDC, &lpdis->rcItem, hB);
		DeleteObject(hB);

		if ( lpdis->itemID >= CL_withtext ){
			const TCHAR *text;

			text = GetCText(
				(lpdis->itemID == CL_auto) ?
					( (GClist[edit_gc].flags & GC_aoff) ?
						stringaoff : stringauto)  : stringetc);
			TextOut(lpdis->hDC, lpdis->rcItem.left + 2,
					lpdis->rcItem.top + 2, text, tstrlen32(text));
		}

		SetBkMode(lpdis->hDC, OldBkMode);
		if ( lpdis->itemState & ODS_SELECTED ){
			RECT rect;

			FrameRect(lpdis->hDC, &lpdis->rcItem, GetStockObject(BLACK_BRUSH));
			rect.left = lpdis->rcItem.left + 1;
			rect.top = lpdis->rcItem.top + 1;
			rect.right = lpdis->rcItem.right - 1;
			rect.bottom = lpdis->rcItem.bottom - 1;
			FrameRect(lpdis->hDC, &rect, GetStockObject(WHITE_BRUSH));
		}
	}
	if ( lpdis->itemAction & ODA_FOCUS ){
		int i;
		RECT rect;
		for ( i = 0 ; i < 2 ; i++ ){
			rect.left = lpdis->rcItem.left + i;
			rect.top = lpdis->rcItem.top + i;
			rect.right = lpdis->rcItem.right - i;
			rect.bottom = lpdis->rcItem.bottom - i;
			DrawFocusRect(lpdis->hDC, &rect);
		}
	}
}

// 項目一覧 + 現在の色の表示
void ColorItemDraw(DRAWITEMSTRUCT *lpdis)
{
	TCHAR buf[CMDLINESIZE];
	int len, OldBkMode;
	COLORREF OldText, OldBack;
	RECT box;
	HBRUSH hB;

	OldBkMode = SetBkMode(lpdis->hDC, OPAQUE);
	box = lpdis->rcItem;
	if ( (lpdis->itemAction & ODA_FOCUS) && !(lpdis->itemState & ODS_FOCUS) ){
		hB = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
		FillRect(lpdis->hDC, &lpdis->rcItem, hB);
		DeleteObject(hB);
	}

	box.right = box.left + (box.bottom - box.top);
	if ( lpdis->itemData == 0xfffffffe ){ // 下層アリ
		TextOut(lpdis->hDC, box.left, box.top, SubMenuString, 1);
	}else if ( lpdis->itemData == C_AUTO ){ // 自動
		TextOut(lpdis->hDC, box.left, box.top, AutoString, 2);
	}else{
		hB = CreateSolidBrush((COLORREF)lpdis->itemData);
		FillRect(lpdis->hDC, &box, hB);
		DeleteObject(hB);
	}

	if ( lpdis->itemState & ODS_SELECTED ){
		OldText = SetTextColor(lpdis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
		OldBack = SetBkColor(lpdis->hDC, GetSysColor(COLOR_HIGHLIGHT));
	}
	len = (int)SendMessage(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)buf);
	TextOut(lpdis->hDC, box.right, box.top, buf, len);
	if ( lpdis->itemState & ODS_SELECTED ){
		SetTextColor(lpdis->hDC, OldText);
		SetBkColor(lpdis->hDC, OldBack);
	}
	if ( lpdis->itemState & ODS_FOCUS ){
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
	}
	SetBkMode(lpdis->hDC, OldBkMode);
}

BOOL SelectUserColor(HWND hDlg)
{
	CHOOSECOLOR cc;
	DWORD index;

	index = SendDlgItemMessage(hDlg, IDL_GCOLOR, LB_GETCURSEL, 0, 0);
	if ( index < COLORLIST_COLORS ){
		cc.rgbResult = G_Colors[index];
	}
	userColor[userColor_SEL] = G_Colors[CL_user];
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = hDlg;
	cc.lpCustColors = userColor;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	if ( ChooseColor(&cc) ){
		G_Colors[CL_user] = cc.rgbResult;
		SendDlgItemMessage(hDlg, IDL_GCOLOR, LB_SETCURSEL, (WPARAM)CL_user, 0);
		return TRUE;
	}
	return FALSE;
}

#pragma argsused
INT_PTR CALLBACK ColorPage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TCHAR buf[MAX_PATH];

	switch (msg){
		case WM_SETTINGCHANGE:
			InitColorControls(hDlg);
			break;

		case WM_INITDIALOG:
			InitColorPage(hDlg);
			CSelectType(hDlg);
			return FALSE;

		case WM_DRAWITEM:
			if ( wParam == IDL_GCOLOR ){
				ColorSampleDraw((DRAWITEMSTRUCT *)lParam);
			}else if ( (wParam == IDL_GCITEM) || (wParam == IDL_GCTYPE) ){
				ColorItemDraw((DRAWITEMSTRUCT *)lParam);
			}
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_TEST:
					Test();
					break;
				case IDL_GCTYPE:
					if ( GetListCursorIndex(wParam, lParam, &edit_gc) > 0 ){
						CSelectType(hDlg);
					}
					break;
				case IDL_GCITEM:
					if ( GetListCursorIndex(wParam, lParam, &edit_sgc) > 0 ){
						CSelectType2(hDlg);
					}
					break;
				case IDB_GCADD:
					GetControlText(hDlg, IDE_GCEDIT, buf, TSIZEOF(buf));
					if ( buf[0] != '\0' ){
						int index;

						FixWildItemName(buf);
						if ( !IsExistCustTable(GClist[edit_gc].item, buf) ){
							SendDlgItemMessage(hDlg, IDL_GCITEM,
									LB_ADDSTRING, 0, (LPARAM)buf);
							Changed(hDlg);
						}
						index = (int)SendDlgItemMessage(hDlg, IDL_GCITEM,
								LB_FINDSTRING, 0, (LPARAM)buf);
						if ( index != LB_ERR ){
							SendDlgItemMessage(hDlg, IDL_GCITEM,
									LB_SETCURSEL, (WPARAM)index, 0);
							edit_sgc = index;
						}
						SetDlgItemText(hDlg, IDE_GCEDIT, NilStr);
						SelectColor(hDlg, SendDlgItemMessage(hDlg, IDL_GCOLOR,
								LB_GETCURSEL, 0, 0));
					}
					break;

				case IDB_GCDEL:
					SendDlgItemMessage(hDlg, IDL_GCITEM, LB_GETTEXT,
							(WPARAM)edit_sgc, (LPARAM)(LPCSTR)buf);
					DeleteCustTable(GClist[edit_gc].item, buf, 0);
					SendDlgItemMessage(hDlg, IDL_GCITEM, LB_DELETESTRING,
							(WPARAM)edit_sgc, 0);
					Changed(hDlg);
					break;

				case IDL_GCOLOR:
					if ( HIWORD(wParam) == LBN_SELCHANGE ){
						SelectColor(hDlg,
								SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0));
						Changed(hDlg);
					}
					break;

				case IDB_GCSEL:
					if ( IsTrue(SelectUserColor(hDlg))){
						SelectColor(hDlg, CL_user);
						Changed(hDlg);
					}
					InvalidateRect(GetDlgItem(hDlg, IDL_GCOLOR), NULL, FALSE);
					break;

				case IDB_GCSORTN:
					SortCustTable(T("C_ext"), NULL);
					CSelectType(hDlg);
					break;

				case IDB_GCSORTC:
					SortCustTable(T("C_ext"), SortColorFunc);
					CSelectType(hDlg);
					break;

				case IDX_GCULINE:
					ChangeUnderline(hDlg);
					Changed(hDlg);
					break;
			}
			break;

		case WM_NOTIFY:
			if ( ((NMHDR *)lParam)->code == PSN_SETACTIVE ){
				InitWndIcon(hDlg, IDB_TEST);
			}
		// default へ
		default:
			return StyleDlgProc(hDlg, msg, IDD_COLOR, lParam);
	}
	return TRUE;
}

//=============================================================================
INT_PTR CALLBACK HideMenuColorDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
			InitColorPage(hDlg);
			SetColorList(hDlg, *(COLORREF *)lParam);
			return TRUE;

		case WM_DRAWITEM:
			if (wParam == IDL_GCOLOR) ColorSampleDraw((DRAWITEMSTRUCT *)lParam);
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDB_GCSEL:
					SelectUserColor(hDlg);
					break;

				case IDL_GCOLOR: {
					int index;

					if ( GetListCursorIndex(wParam, lParam, &index) != 0 ){
						*(COLORREF *)GetWindowLongPtr(hDlg, DWLP_USER) =
								G_Colors[index];
					}
					break;
				}
				case IDOK:
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

//=============================================================================
// ハイライト(CV_hkey)

void InitHighlightDialogBox(HWND hDlg, TCHAR *subkey)
{
	TCHAR text[VFPS], data[0x10000], *line, *next;
	HWND hListWnd;

	wsprintf(text, GetCText(T("ハイライト (拡張子:%s)\0Highlight (%s)")), subkey);
	SetWindowText(hDlg, text);

	FixWildItemName(subkey);
	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)subkey);
	InitColorPage(hDlg);

	hListWnd = GetDlgItem(hDlg, IDL_GCITEM);

	data[0] = '\0';
	if ( (NO_ERROR == GetCustTable(T("CV_hkey"), subkey, data, sizeof(data))) && data[0] ){
		line = data;
		for(;;){
			int index;
			COLORREF color;
			TCHAR *dst;

			dst = text;
			next = tstrchr(line, '\n');
			if ( next != NULL ) *next = '\0';
			for(;;){
				TCHAR c = *line;
				if ( (c == '<') || (c == '>') ){
					*dst++ = c;
					line++;
					continue;
				}
				break;
			}
			*dst++ = ',';
			color = GetColor((const TCHAR **)&line, TRUE);
			if ( SkipSpace((const TCHAR **)&line) == ',' ) line++;
			SkipSpace((const TCHAR **)&line);
			tstrcpy(dst, line);

			if ( dst[0] ){
				index = (int)SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)text);
				SendMessage(hListWnd, LB_SETITEMDATA, index, (LPARAM)color);
			}

			if ( next != NULL ){
				line = next + 1;
				continue;
			}
			break;
		}
	}
}

void GetHighlightItem(HWND hListWnd, int index, HighlightItem *hi)
{
	TCHAR *wordptr;

	if ( hListWnd != NULL ){
		SendMessage(hListWnd, LB_GETTEXT, index, (LPARAM)hi->text);
	}

	hi->cb_top = hi->cb_bottom = FALSE;
	wordptr = hi->text;

	for(;;){
		TCHAR c = *wordptr;

		if ( c == '<' ){
			hi->cb_top = TRUE;
		}else if ( c == '>' ){
			hi->cb_bottom = TRUE;
		}else{
			if ( c == ',' ) wordptr++;
			hi->textptr = wordptr;
			return;
		}
		wordptr++;
	}
}

void SetHighlightItem(HWND hListWnd, int index, HighlightItem *hi, COLORREF color)
{
	TCHAR text[VFPS], *dst = text;

	if ( hi->cb_top ) *dst++ = '<';
	if ( hi->cb_bottom ) *dst++ = '>';
	if ( hListWnd == NULL ) dst += wsprintf(dst, T("H%06X"), color);
	*dst++ = ',';
	tstrcpy(dst, hi->textptr);
	if ( hListWnd == NULL ){
		tstrcpy(hi->text, text);
		return;
	}

	if ( index != LB_ERR ){
		SendMessage(hListWnd, LB_DELETESTRING, index, 0);
		index = (int)SendMessage(hListWnd, LB_INSERTSTRING, index, (LPARAM)text);
	}else{
		index = (int)SendMessage(hListWnd, LB_ADDSTRING, 0, (LPARAM)text);
	}
	SendMessage(hListWnd, LB_SETITEMDATA, index, (LPARAM)color);
	if ( index != LB_ERR ){
		SendMessage(hListWnd, LB_SETCURSEL, (WPARAM)index, 0);
	}
}

void ChangeHighlightItem(HWND hDlg, int id)
{
	HighlightItem hi;
	HWND hListWnd;
	int index;
	COLORREF color;
	TCHAR text[VFPS];

	hListWnd = GetDlgItem(hDlg, IDL_GCITEM);
	index = (int)SendMessage(hListWnd, LB_GETCURSEL, 0, 0);
	text[0] = '\0';
	GetControlText(hDlg, IDE_GCEDIT, text, TSIZEOF(text));

	if ( index != LB_ERR ){ // 選択有り
		GetHighlightItem(hListWnd, index, &hi);
		color = (COLORREF)SendMessage(hListWnd, LB_GETITEMDATA, index, 0);
		if ( tstrcmp(hi.textptr, text) != 0 ){
			if ( id != IDB_GCADD ) return;
			index = LB_ERR;
		}
	}
	if ( index == LB_ERR ){ // 選択無し…新規(addボタンの時のみ)
		int cindex;

		if ( (id != IDB_GCADD) || (text[0] == '\0') ) return;
		cindex = (int)SendDlgItemMessage(hDlg, IDL_GCOLOR, LB_GETCURSEL, 0, 0);
		if ( cindex == LB_ERR ) cindex = 0;
		color = G_Colors[cindex];

		tstrcpy(hi.text, text);
		hi.textptr = hi.text;
	}
	hi.cb_top = IsDlgButtonChecked(hDlg, IDX_HL_TOP);
	hi.cb_bottom = IsDlgButtonChecked(hDlg, IDX_HL_BOTTOM);

	SetHighlightItem(hListWnd, index, &hi, color);
}

void SelectHighlightItem(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int index;
	HighlightItem hi;

	if ( GetListCursorIndex(wParam, lParam, &index) == 0 ) return;
	GetHighlightItem((HWND)lParam, index, &hi);
	if ( hi.textptr == hi.text ) return;

	SetDlgItemText(hDlg, IDE_GCEDIT, hi.textptr);
	SetColorList(hDlg, (COLORREF)SendMessage((HWND)lParam, LB_GETITEMDATA, index, 0));
	CheckDlgButton(hDlg, IDX_HL_TOP, hi.cb_top);
	CheckDlgButton(hDlg, IDX_HL_BOTTOM, hi.cb_bottom);
}

void SelectHighlightColor(HWND hDlg, DWORD colorindex)
{
	int index;
	HWND hListWnd;

	if ( colorindex >= COLORLIST_COLORS ) return;

	hListWnd = GetDlgItem(hDlg, IDL_GCITEM);
	index = (int)SendMessage(hListWnd, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return;
	SendMessage(hListWnd, LB_SETITEMDATA, index, G_Colors[colorindex]);
	InvalidateRect(hListWnd, NULL, FALSE);
}

void SaveHighlightDialogBox(HWND hDlg)
{
	HighlightItem hi;
	HWND hListWnd;
	int count, index = 0;
	TCHAR text[0x2000], *dst = text, *subkey;

	subkey = (TCHAR *)GetWindowLongPtr(hDlg, DWLP_USER);
	hListWnd = GetDlgItem(hDlg, IDL_GCITEM);
	count = (int)SendMessage(hListWnd, LB_GETCOUNT, 0, 0);
	if ( count == 0 ){
		DeleteCustTable(T("CV_hkey"), subkey, 0);
		return;
	}
	while ( index < count ){
		COLORREF color;

		GetHighlightItem(hListWnd, index, &hi);
		color = (COLORREF)SendMessage(hListWnd, LB_GETITEMDATA, index, 0);
		SetHighlightItem(NULL, 0, &hi, color);
		dst += wsprintf(dst, T("%s\n"), hi.text);
		index++;
	}
	*dst = '\0';
	SetCustStringTable(T("CV_hkey"), subkey, text, 0);
}

INT_PTR CALLBACK HighlightDialogBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG:
			InitHighlightDialogBox(hDlg, (TCHAR *)lParam);
			return FALSE;

		case WM_DRAWITEM:
			if ( wParam == IDL_GCOLOR ){
				ColorSampleDraw((DRAWITEMSTRUCT *)lParam);
			}else if ( wParam == IDL_GCITEM ){
				ColorItemDraw((DRAWITEMSTRUCT *)lParam);
			}
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					SaveHighlightDialogBox(hDlg);
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDL_GCITEM:
					SelectHighlightItem(hDlg, wParam, lParam);
					break;

				case IDL_GCOLOR:
					if ( HIWORD(wParam) == LBN_SELCHANGE ){
						SelectHighlightColor(hDlg,
								SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0));
					}
					break;

				case IDX_HL_TOP:
				case IDX_HL_BOTTOM:
				case IDB_GCADD:
					ChangeHighlightItem(hDlg, LOWORD(wParam));
					break;

				case IDB_GCDEL: {
					int index;

					index = (int)SendDlgItemMessage(hDlg, IDL_GCITEM, LB_GETCURSEL, 0, 0);
					if ( index == LB_ERR ) break;
					SendDlgItemMessage(hDlg,
							IDL_GCITEM, LB_DELETESTRING, (WPARAM)index, 0);
					break;
				}
				case IDB_GCSEL:
					if ( IsTrue(SelectUserColor(hDlg)) ){
						SelectHighlightColor(hDlg, CL_user);
					}
					InvalidateRect(GetDlgItem(hDlg, IDL_GCOLOR), NULL, FALSE);
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}
