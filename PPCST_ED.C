/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer	その他 シート - PPc 表示書式
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#include "PPC_DISP.H"
#pragma hdrstop

#define DFP_PARAMCOUNT 2 // 書式の最大パラメータ数

// パラメータの追加オプション用フラグ
#define COMMENTHID B31	// コメント時非表示
#define FIXFRAME B30	// W
#define FIXLENGTH B29	// w
// B16-23: W, w の長さ
#define PARAMMASK MAX16

// パラメータの形式
typedef enum { DFP_NONE = 0, DFP_WIDTH, DFP_EXT, DFP_COLOR, DFP_STRING, DFP_COLUMN, DFP_MODULE} DFPTYPES;

typedef struct {
	const TCHAR *name;
	DFPTYPES type;
	const TCHAR *list;
} DISPFORMATPARAM;

typedef struct {
	const TCHAR *name;
	TCHAR ID;
	DISPFORMATPARAM param[DFP_PARAMCOUNT];
	const TCHAR *defaultparam;
} DISPFORMATITEM;
DISPFORMATITEM dfitem[] = {
	{T("空白(中用)\0blank"),		'S', {
			{T("幅\0width"), DFP_WIDTH, T("1\0") T(" - 空欄:残り全て\1fill blank\0")},
			{NULL, DFP_NONE, NULL}}, T("1")},
	{T("空白(端用)\0blank-for-edge"), 's', {
			{T("幅\0width"), DFP_WIDTH, T("1\0") T(" - 空欄:残り全て\1fill blank\0")},
			{NULL, DFP_NONE, NULL}}, T("1")},
	{T("マーク「*」\0mark-symbol"),		'M', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("チェック\0check-mark"),		'b', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("チェックボックス\0check-box"),		'B', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("アイコン\0icon"),			'N', {
			{T("大きさ\0size"), DFP_WIDTH, T(" - 空欄:文字に合わせる\1auto size\0") T("16 - 小\1small\0") T("32 - 中\1middle\0") T("48 - 大\1large\0")},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("画像\0image"),				'n', {
			{T("横桁数\0width"), DFP_STRING, T("\0")},
			{T("縦行数\0height"), DFP_STRING, T("\0")}}, T("20,8")},
	{T("短いファイル名\0short-filename"),	'f', {
			{T("名前幅\0name-width"), DFP_WIDTH, T("8 - 通常\1normal\0") T("E8 - 拡張子優先\1for ext.\0")},
			{T("拡張子幅\0ext.-width"), DFP_EXT, T("5 - 通常\1normal\0") T(" - 空欄:ファイル名と分離しない\1joint ext.\0")}}, T("8,5")},
	{T("ファイル名\0filename"),		'F', {
			{T("名前幅\0name-width"), DFP_WIDTH, T("14 - 短め\1short\0") T("30 - 長め\1long\0") T(" - 詳細表示(１行表示)\1full width\0") T("E14 - 拡張子優先\1for ext.\0") T("M14 - 複数段表示\1multi line\0")},
			{T("拡張子幅\0ext.-width"), DFP_EXT, T("5 - 通常\1normal\0") T(" - 空欄:ファイル名と分離しない\1joint ext.\0")}}, T("17,5")},
	{T("ファイルサイズ\0size"), 'Z', {
			{T("幅\0width"), DFP_WIDTH, T("7 - 6桁分\0") T("10 - 9桁分\0")},
			{NULL, DFP_NONE, NULL}}, T("7")},
	{T("ファイルサイズ「,」付\0size\',\'"), 'z', {
			{T("幅\0width"), DFP_WIDTH, T("10 - 6桁分\0") T("14 - 9桁分\0") T("K10 - エクスプローラ風K単位\1'K'\0")},
			{NULL, DFP_NONE, NULL}}, T("10")},
	{T("更新時刻\0write-time"),		'T', {
			{T("幅\0width"), DFP_WIDTH, T("8 - 年月日\1YY-MM-DD\0") T("14 - 年月日時分\1YY-MM-DD TT:MM\0") T("17 - 年月日時分秒\1YY-MM-DD TT:MM:SS\0")},
			{NULL, DFP_NONE, NULL}}, T("14")},
	{T("時刻詳細\0time-detail"),		't', {
			{T("書式\0format"), DFP_STRING,
			 T("\"Y-n-d(w) u:M:St\" - 更新時刻\1modify(JPN)\0")
			 T("C\"Y-n-d(w) u:M:St\" - 作成時刻\1create\0")
			 T("A\"Y-n-d(w) u:M:St\" - アクセス時刻\1access\0")
			 T("\"n/d/Y(w) u:M:St\" - 更新時刻\1modify(US)\0")
			 T("\"d/n/Y(w) u:M:St\" - 更新時刻\1modify(ENG)\0")
			 },
			{NULL, DFP_NONE, NULL}}, T("\"Y-n-d(w) u:M:St\"")},
	{T("属性\0attributes"),			'A', {
			{T("幅\0width"), DFP_WIDTH, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("10")},
	{T("コメント\0comment"),		'C', {
			{T("幅\0width"), DFP_WIDTH, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("10")},
	{T("コメント(拡張)\0comment-ex."), 'u', {
			{T("ID(1-9)\0ID(1-9)"), DFP_STRING, T("1\0") T("2\0") T("3\0") T("4\0") T("5\0")},
			{T("幅\0width"), DFP_WIDTH, T("\0")}}, T("1,10")},
	{T("カラム拡張\0columns"), 'U', {
			{T("種別\0type"), DFP_COLUMN, NilStr},
			{T("幅\0width"), DFP_WIDTH, T("\0")}}, T("\"タイトル\",10")},
	{T("モジュール拡張\0modules"), 'X', {
			{T("種別\0type"), DFP_MODULE, NilStr},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("縦線「|」\0line\'|\'"),	'L', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("横線「-」\0line\'-\'"),	'H', {
			{T("幅\0width"), DFP_WIDTH, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("10")},
	{T("改行\0next-line"),			'/', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("文字色\0text-color"),		'O', {
			{T("色\0color"), DFP_COLOR, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("\"_DWHI\"")},
	{T("文字列\0user-text"),		'i', {
			{T("文字列\0text"), DFP_STRING, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("\"text\"")},
	{T("文字列(項目名)\0item-text"), 'I', {
			{T("文字列\0text"), DFP_STRING, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("\"text\"")},
	{T("id別特殊環境変数\0id-string-value"), 'v', {
			{T("変数名\0name"), DFP_STRING, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("i\"name\"")},
	{T("ディレクトリ種類\0dir.-type"), 'Y', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("ボリュームラベル\0volume-label"), 'V', {
			{T("幅\0width"), DFP_WIDTH, T("\0")},
			{NULL, DFP_NONE, NULL}}, T("8")},
	{T("ディレクトリ\0directory"), 'R', {
			{T("幅\0width"), DFP_WIDTH, T(" - 空欄:現在ディレクトリ\1current\0")T("M - ディレクトリマスク\1dir. mask\0")},
			{NULL, DFP_NONE, NULL}}, T("60")},
	{T("エントリ数\0entry-count"), 'E', {
			{T("種別\0type"), DFP_WIDTH, T(" - 空欄:表示分/全体\1show/all\0") T("0 - 全体\1all\0") T("1 - ./..除く全体\1all without ./..\0")},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("表示エントリ数\0show-entries"), 'e', {
			{T("種別\0type"), DFP_WIDTH, T(" - 空欄:全て\1all\0") T("1 - ./..除く\1without ./..\0") T("2 - ディレクトリ数\1directories\0") T("3 - ファイル数\1files\0")},
			{NULL, DFP_NONE, NULL}}, T("1")},
	{T("全ページ数\0all-pages"), 'P', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("現在ページ数\0page-no"), 'p', {
			{NULL, DFP_NONE, NULL},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("マーク情報\0mark"), 'm', {
			{T("書式\0type"), DFP_STRING, T("n - マーク数\1marks\0") T("S - 桁区切り無しサイズ\1size xxxxx\0") T("s - 桁区切り有りサイズ\1size x,xxx\0") T("K - エクスプローラ風サイズ\1size xxxK\0")},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("ディスク容量\0disk-space"), 'D', {
			{T("種別\0type"), DFP_STRING, T("F - 空き\1free\0") T("f - 空き(桁区切有)\1free x,xxx\0") T("U - 使用\1used\0") T("u - 使用(桁区切有)\1used x,xxx\0") T("T - 総計\1total\0") T("t - 総計(桁区切有)\1total x,xxx\0")},
			{NULL, DFP_NONE, NULL}}, NilStr},
	{T("不明\0unknown"), '\0', {{NULL, DFP_NONE, NULL}, {NULL, DFP_NONE, NULL}}, NilStr}
};

const TCHAR X_stat_default[] = T("I\"Mark:\"mn i\"/\" ms8 L I\"Entry:\" e0 i\"/\" E0 L I\"Free:\" Df8 L I\"Used:\" Du8 L I\"Total:\" Dt8 L Y");

const TCHAR ColumnListComment[] = T("F4キーで一覧表示します\0Press F4 key");

DWORD_PTR USECDECL ModuleFunction(PPXAPPINFO *info, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	if ( cmdID == PPXCMDID_REPORTPPCCOLUMN ){
		SendMessage(info->hWnd, CB_ADDSTRING, 0, (LPARAM)uptr->str);
		return 1;
	}
	if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
	return 0;
}

// エントリ表示書式 ===========================================================
// w / W の長さを文字列化
int GetFixMaxString(TCHAR *dest, int param)
{
	int width;

	width = (param >> 16) & 0xff;
	if ( (width == 0) || (width >= 255) ){
		dest[0] = '\0';
		return 0;
	}
	return wsprintf(dest, T("%d"), width);
}

// 選択した項目のパラメータを表示する
void SetDispFormatParamUI(HWND hDlg, int index)
{
	DISPFORMATPARAM *dfp;
	UINT sid = IDS_DFPARAM1, cid = IDC_DFPARAM1;
	int param, i;
	TCHAR formatBuf[CMDLINESIZE];
	TCHAR *format;
	BOOL show1, show2;

	if ( LB_ERR == SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETTEXT, index, (LPARAM)formatBuf) ){
		return;
	}
	format = tstrchr(formatBuf, ' ');
	if ( format == NULL ){
		format = NilStrNC;
	}else{
		format++;
	}
	param = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETITEMDATA, index, 0);
	dfp = dfitem[param & PARAMMASK].param;

	CheckDlgButton(hDlg, IDX_COMMENTHID, param & COMMENTHID);
	CheckDlgButton(hDlg, IDX_FIXFRAME, param & FIXFRAME);
	CheckDlgButton(hDlg, IDX_FIXLENGTH, param & FIXLENGTH);
	show1 = dfp[1].type == DFP_EXT;
	show2 = show1 || (tstrchr(DE_ENABLE_FIXLENGTH, dfitem[param & PARAMMASK].ID) != NULL);
	ShowDlgWindow(hDlg, IDX_FIXFRAME, show2);
	ShowDlgWindow(hDlg, IDX_FIXLENGTH, show1);
	ShowDlgWindow(hDlg, IDE_FIXMAX, show2);

	for ( i = 0 ; i < DFP_PARAMCOUNT ; i++, dfp++, sid += 2, cid += 2 ){
		HWND hCWnd;

		hCWnd = GetDlgItem(hDlg, cid);
		if ( dfp->type == DFP_NONE ){
			ShowDlgWindow(hDlg, sid, FALSE);
			ShowWindow(hCWnd, SW_HIDE);
		}else{
			SendMessage(hCWnd, CB_RESETCONTENT, 0, 0);
			if ( dfp->type == DFP_COLUMN ){
			/* メニュー表示に移行
				ThSTRUCT thEcdata;
				TCHAR path[VFPS];

				ThInit(&thEcdata);
				CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				GetCurrentDirectory(VFPS, path);
				GetColumnExtMenu(&thEcdata, path, (HMENU)hCWnd, 0);
				GetColumnExtMenu(&thEcdata, NULL, NULL, 0);
				CoUninitialize();
			*/
				SendMessage(hCWnd, CB_ADDSTRING, 0, (LPARAM)GetCText(ColumnListComment));
			}else if ( dfp->type == DFP_MODULE ){
				PPXMODULEPARAM dummypmp;
				PPXAPPINFO info = {(PPXAPPINFOFUNCTION)ModuleFunction, T("PPcust"), NilStr, NULL};
				info.hWnd = hCWnd;
				dummypmp.command = NULL;
				CallModule(&info, PPXMEVENT_FILEDRAWINFO, dummypmp, NULL);
			}else{
				TCHAR comment[128];
				const TCHAR *src;
				TCHAR *dest;

				src = dfp->list;
				while ( *src != '\0' ){
					dest = comment;
					for (;;){
						if ( *src == '\0' ) break;
						if ( (*src == ' ') && (*(src - 1) == '-') && (*(src - 2) == ' ') ){
							src++;
							*dest++ = ' ';
							if ( UseLcid == LCID_PPXDEF ){
								for (;;){
									if ( *src == '\0' ) break;
									if ( *src == '\1' ) break;
									*dest++ = *src++;
								}
							}else{
								for (;;){
									if ( *src == '\0' ) break;
									if ( *src == '\1' ){
										src++;
										for (;;){
											if ( *src == '\0' ) break;
											*dest++ = *src++;
										}
										break;
									}
									src++;
								}
							}
							break;
						}
						*dest++ = *src++;
					}
					*dest = '\0';
					SendMessage(hCWnd, CB_ADDSTRING, 0, (LPARAM)comment);
					src += tstrlen(src) + 1;
				}
			}

			if ( format != NULL ){
				TCHAR *ptr;

				ptr = format;
				if ( dfp->type != DFP_MODULE ){ // module以外はカンマで切り出し
					while ( (*ptr != '\0') && (*ptr != ',') ) ptr++;
					if ( *ptr == ',' ) *ptr++ = '\0';
				}
				SendMessage(hCWnd, WM_SETTEXT, 0, (LPARAM)format);
				format = ptr;
			}
			SetDlgItemText(hDlg, sid, GetCText(dfp->name));
			ShowDlgWindow(hDlg, sid, TRUE);
			ShowWindow(hCWnd, SW_SHOW);
		}
	}
	if ( show2 ){
		TCHAR num[8];

		GetFixMaxString(num, param);
		SetDlgItemText(hDlg, IDE_FIXMAX, num); // ここで、再設定が起きるので、最後に記載しないといけない
	}
}

// XC_stat/XC_inf1/XC_inf2 の 色指定をスキップする
TCHAR *GetInfoFormat(const TCHAR *src)
{
	int i;
	TCHAR *p;

	for ( i = 0 ; i < 4 ; i++ ){
		p = tstrchr(src, ',');
		if ( p != NULL ) src = p + 1;
	}
	return (TCHAR *)src;
}

BOOL SaveDispFormat(HWND hDlg)
{
	int index = 0, param, usefix = 0;
	TCHAR savetext[CMDLINESIZE], text[CMDLINESIZE], *dst, *p;
	const TCHAR *key;

	key = (const TCHAR *)GetWindowLongPtr(hDlg, DWLP_USER);
	if ( EtcEditFormat == ETC_INFODISP ){
		dst = savetext + wsprintf(savetext, T("*setcust %s="), key);

		// 書式以外の色やマーク指定等を用意する
		wsprintf(dst, T("%%*getcust(%s)"), key);
		PP_ExtractMacro(NULL, NULL, NULL, dst, dst, XEO_DISPONLY);
		dst = GetInfoFormat(dst);
	}else{
		wsprintf(savetext, T("*setcust MC_celS:%s="), key);
		tstrreplace(savetext, T("%"), T("%%"));
		dst = savetext + tstrlen(savetext);
	}

	for ( ;; ){
		if ( SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETTEXT, index, (LPARAM)text) == LB_ERR ){
			break;
		}
		param = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETITEMDATA, index, 0);
		if ( param & COMMENTHID ) *dst++ = 'c';
		if ( param & FIXFRAME ){
			*dst++ = 'w';
			dst += GetFixMaxString(dst, param);
		}
		if ( param & FIXLENGTH ){
			*dst++ = 'W';
			dst += GetFixMaxString(dst, param);
		}
		if ( param & (FIXFRAME | FIXLENGTH) ){
			if ( usefix ){
				SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, index, 0);
				PMessageBox(hDlg, T("「～一杯に表示」指定は、１つしか使用できません。"), StrCustTitle, MB_OK);
				return FALSE;
			}
			usefix = 1;
		}
		*dst++ = dfitem[param & PARAMMASK].ID;
		p = tstrchr(text, ' ');
		if ( p != NULL ){
			p++;
			while ( *p != '\0' ){
				*dst++ = *p++;
			}
		}
		index++;
	}
	*dst = '\0';
	PP_ExtractMacro(NULL, NULL, NULL, savetext, NULL, 0);
	return TRUE;
}

const TCHAR *GetDispFormat(TCHAR *data, const TCHAR *label, int offset)
{
	const TCHAR *src;
	TCHAR labelbuf[CMDLINESIZE * 2];

	tstrcpy(labelbuf, label);
	CustNameEscape(labelbuf);

	wsprintf(data, offset ?
			T("%%*getcust(%s)") : T("%%*getcust(\"MC_celS:%s\")"), labelbuf);
	PP_ExtractMacro(NULL, NULL, NULL, data, data, XEO_DISPONLY);
	src = data;
	if ( offset ){
		src = GetInfoFormat(src);
		if ( (*src == '\0') && (tstrcmp(label, T("XC_stat")) == 0) ){
			src = X_stat_default;
		}
	}
	return src;
}

void InitDispFormat(HWND hDlg, const TCHAR *key)
{
	TCHAR format[CMDLINESIZE], formc, textbuf[MAX_PATH];
	const TCHAR *src, *text;
	int index, param;
	BOOL fromppc = FALSE;

	PPxRegist(hDlg, PPcustRegID, PPXREGIST_IDASSIGN);
	if ( key == NULL ){ // PPc の直接編集モード
		EtcEditFormat = ETC_CELLDISP;
		key = MC_CELS_TEMPNAME;
		fromppc = TRUE;
	}
	SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)key);

	// 書式を列挙する
	src = GetDispFormat(format, key, (EtcEditFormat == ETC_INFODISP));
	if ( IsTrue(fromppc) ) DeleteCustTable(T("MC_celS"), key, 0);
	index = 0;
	while( (formc = *src++) != '\0' ){ // 各書式をリストに列挙する
		DISPFORMATITEM *tdf;
		TCHAR *p;

		if ( formc == ' ') continue;

		// プレフィックス
		param = 0;
		if ( formc == 'c' ){
			setflag(param, COMMENTHID);
			formc = *src++;
		}
		if ( (formc == 'w') || (formc == 'W') ){
			setflag(param, (formc == 'w') ? FIXFRAME : FIXLENGTH ) ;
			param = param | (GetDwordNumber(&src) << 16);
			formc = *src++;
		}

		// 書式IDチェック
		for ( tdf = dfitem ; tdf->ID ; tdf++ ){
			if ( formc == tdf->ID ) break;
			param++;
		}
		text = GetCText(tdf->name);

		tstrcpy(textbuf, text);
		p = textbuf + tstrlen(textbuf);
		*p++ = ' ';
		while( (UTCHAR)*src > ' ' ){ // パラメータ部分をコピー
			if ( *src == '\"' ){ // " で括った部分をまとめてコピー
				do {
					*p++ = *src++;
				}while ( ((UTCHAR)*src >= ' ') && (*src != '\"') );
			}
			*p++ = *src++;
		}
		*p = '\0';
		SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_ADDSTRING, 0, (LPARAM)textbuf);
		SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETITEMDATA, index, param);
		index++;
	}

	// 使用できる書式の一覧を列挙する
	{
		DISPFORMATITEM *tdf;

		for ( tdf = dfitem ; tdf->ID ; tdf++ ){
			SendDlgItemMessage(hDlg, IDL_DFLIST,
					LB_ADDSTRING, 0, (LPARAM)GetCText(tdf->name));
		}
	}
	if ( index ){
		SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, 0, 0);
		SetDispFormatParamUI(hDlg, 0);
	}
}

void SeletctNowDispFormat(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int index;

	if ( GetListCursorIndex(wParam, lParam, &index) == 0 ) return;
	SetDispFormatParamUI(hDlg, index);
}

void InsertDispFormat(HWND hDlg)
{
	int index, templeateitem;
	TCHAR param[CMDLINESIZE];

	templeateitem = (int)SendDlgItemMessage(hDlg, IDL_DFLIST, LB_GETCURSEL, 0, 0);
	if ( templeateitem == LB_ERR ) templeateitem = 0;

	index = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCURSEL, 0, 0);
	if ( index < 0 ) index = 0;

	wsprintf(param, T("%s %s"), GetCText(dfitem[templeateitem].name), dfitem[templeateitem].defaultparam);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_INSERTSTRING, index, (LPARAM)param);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETITEMDATA, index, templeateitem);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, index, 0);
	SetDispFormatParamUI(hDlg, index);
}

void DeleteDispFormat(HWND hDlg)
{
	int index;

	index = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return;
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_DELETESTRING, index, 0);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, index, 0);
	SetDispFormatParamUI(hDlg, index);
}

void UpDownDispFormat(HWND hDlg, int dist)
{
	int index, param;
	TCHAR text[CMDLINESIZE];
	DWORD shift = GetShiftKey();

	index = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return;
	if ( dist < 0 ){
		if ( index <= 0 ) return;
//		if ( shift & K_s ) dist = -min(index , 5);
		if ( shift & K_c ) dist = -index;
	}else{
		int count = SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCOUNT, 0, 0);
		if ( (index + 1) >= count ) return;
//		if ( shift & K_s ) dist = min(index , (count - index - 1));
		if ( shift & K_c ) dist = (count - index - 1);
	}

	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETTEXT, index, (LPARAM)text);
	param = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETITEMDATA, index, 0);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_DELETESTRING, index, 0);

	index += dist;
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_INSERTSTRING, index, (LPARAM)text);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETITEMDATA, index, param);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, index, 0);
}

void DeleteComment(HWND hDlg, UINT ID, TCHAR *text)
{
	TCHAR *p;

	p = tstrstr(text, T(" - "));
	if ( p != NULL ){
		*p = '\0';
		SetDlgItemText(hDlg, ID, text);
	}
}

void MakeFixMax(HWND hDlg, int *param, int flag)
{
	TCHAR num[10];
	const TCHAR *src;
	int width;

	GetDlgItemText(hDlg, IDE_FIXMAX, num, TSIZEOF(num));
	src = num;
	width = GetIntNumber(&src);
	if ( (width < 0) || (width >= 255 ) ) width = 0;
	*param |= (width << 16) | flag;
}

void ModifyDispFormat(HWND hDlg)
{
	int index, param;
	TCHAR text[CMDLINESIZE], buf[CMDLINESIZE], *ptr;

	index = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return;

	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETTEXT, index, (LPARAM)text);
	param = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETITEMDATA, index, 0) & PARAMMASK;
	ptr = tstrchr(text, ' ');
	if ( ptr == NULL ) ptr = text + tstrlen(text);
	*ptr++ = ' ';

	if ( dfitem[param].param[0].type != DFP_NONE ){
		GetDlgItemText(hDlg, IDC_DFPARAM1, buf, TSIZEOF(buf));
		if ( dfitem[param].param[0].type == DFP_COLUMN ){
			if ( buf[0] != '\"' ) return; // 区分名の為更新しない
		}
		DeleteComment(hDlg, IDC_DFPARAM1, buf);
		tstrcpy(ptr, buf);
		ptr += tstrlen(ptr);
	}
	if ( dfitem[param].param[1].type != DFP_NONE ){
		GetDlgItemText(hDlg, IDC_DFPARAM2, buf, TSIZEOF(buf));
		if ( buf[0] != '\0' ){
			DeleteComment(hDlg, IDC_DFPARAM2, buf);
			*ptr++ = ',';
			tstrcpy(ptr, buf);
		}
	}
	if ( IsDlgButtonChecked(hDlg, IDX_COMMENTHID) ) setflag(param, COMMENTHID);
	if ( IsDlgButtonChecked(hDlg, IDX_FIXFRAME) ){
		MakeFixMax(hDlg, &param, FIXFRAME);
	}
	if ( IsDlgButtonChecked(hDlg, IDX_FIXLENGTH) ){
		MakeFixMax(hDlg, &param, FIXLENGTH);
	}

	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_DELETESTRING, index, 0);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_INSERTSTRING, index, (LPARAM)text);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETITEMDATA, index, param);
	SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_SETCURSEL, index, 0);
}

BOOL ModifyDispColumnFormat(HWND hDlg, HWND hCBwnd)
{
	int index;
	HMENU hPopMenu;
	ThSTRUCT thEcdata;
	TCHAR path[VFPS];
	RECT box;
	HRESULT ComInitResult;
	#define MenuFirstID 1

	index = (int)SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETCURSEL, 0, 0);
	if ( index == LB_ERR ) return TRUE; // エラーなので無視させる

	if ( dfitem[SendDlgItemMessage(hDlg, IDL_DFOMAT, LB_GETITEMDATA, index, 0) & PARAMMASK].param[0].type != DFP_COLUMN ){
		return FALSE; // 別の種類だった
	}

	ThInit(&thEcdata);
	ComInitResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	GetCurrentDirectory(VFPS, path);

	hPopMenu = CreatePopupMenu();
	GetColumnExtMenu(&thEcdata, path, hPopMenu, MenuFirstID);

	GetWindowRect(hCBwnd, &box);
	index = TrackPopupMenu(hPopMenu, TPM_TOPALIGN | TPM_RIGHTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, box.right, box.bottom, 0, hDlg, NULL);
	GetColumnExtMenu(&thEcdata, NULL, NULL, 0);
	if ( SUCCEEDED(ComInitResult) ) CoUninitialize();

	if ( index >= MenuFirstID ){
		GetMenuString(hPopMenu, index, path + 1, TSIZEOF(path) - 1, MF_BYCOMMAND);
		path[0] = '\"';
		tstrcat(path, T("\""));
		SetWindowText(hCBwnd, path);
		ModifyDispFormat(hDlg);
	}
	DestroyMenu(hPopMenu);
	PostMessage(hCBwnd, CB_SHOWDROPDOWN, 0, 0);
	return TRUE;
}

void FixSelectedItem(HWND hComboWnd)
{
	TCHAR buf[CMDLINESIZE], *ptr;

	SendMessage(hComboWnd, WM_GETTEXT, CMDLINESIZE, (LPARAM)buf);
	ptr = tstrstr(buf, T(" - "));
	if ( ptr != NULL ){
		*ptr = '\0';
		SendMessage(hComboWnd, WM_SETTEXT, 0, (LPARAM)buf);
	}
}

#define WM_APP_CB_SELECTED (WM_APP + 106)
INT_PTR CALLBACK DispFormatDialogBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG:
			InitDispFormat(hDlg, (TCHAR *)lParam);
			break;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					if ( IsTrue(SaveDispFormat(hDlg)) ) EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDL_DFOMAT:
					SeletctNowDispFormat(hDlg, wParam, lParam);
					break;

				case IDB_TB_SETITEM:
					InsertDispFormat(hDlg);
					break;

				case IDB_TB_DELITEM:
					DeleteDispFormat(hDlg);
					break;

				case IDB_MEUP:
					UpDownDispFormat(hDlg, -1);
					break;

				case IDB_MEDW:
					UpDownDispFormat(hDlg, 1);
					break;

				case IDC_DFPARAM1:
					if ( HIWORD(wParam) == CBN_DROPDOWN ){
						if ( IsTrue(ModifyDispColumnFormat(hDlg, (HWND)lParam)) ){
							break;
						}
					}
					// IDC_DFPARAM2 へ
				case IDC_DFPARAM2:
					if ( HIWORD(wParam) == CBN_EDITCHANGE ){
						ModifyDispFormat(hDlg);
					}else if ( HIWORD(wParam) == CBN_SELCHANGE ){
						// 更新完了後にメッセージを受け取るようにする
						PostMessage(hDlg, WM_APP_CB_SELECTED, 0, lParam);
					}
					break;

				case IDX_COMMENTHID:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ModifyDispFormat(hDlg);
					}
					break;

				case IDX_FIXFRAME:
					if ( HIWORD(wParam) == BN_CLICKED ){
						CheckDlgButton(hDlg, IDX_FIXLENGTH, FALSE);
						ModifyDispFormat(hDlg);
					}
					break;

				case IDX_FIXLENGTH:
					if ( HIWORD(wParam) == BN_CLICKED ){
						CheckDlgButton(hDlg, IDX_FIXFRAME, FALSE);
						ModifyDispFormat(hDlg);
					}
					break;

				case IDE_FIXMAX:
					if ( HIWORD(wParam) == EN_CHANGE ){
						ModifyDispFormat(hDlg);
					}
					break;
			}
			break;

		case WM_APP_CB_SELECTED:
			FixSelectedItem((HWND)lParam);
			ModifyDispFormat(hDlg);
			break;

		case WM_DESTROY:
			PPxRegist(NULL, PPcustRegID, PPXREGIST_FREE);
			// default: へ
		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

void FormatCellDispSample(TCHAR *data, const TCHAR *label, int offset)
{
	TCHAR format[CMDLINESIZE];

	tstrcpy(data, GetDispFormat(format, label, offset));
}
